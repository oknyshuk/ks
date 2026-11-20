// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.characterset;
import ks.tier1.commandbuffer;
import ks.tier1.convar;
import ks.tier1.filename_table;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.lzmadecoder;
import ks.tier1.strtools;
import ks.tier1.text_reader;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

#include "tier2/riff.h"
#include "sound.h"
#include "soundservice.h"
#include "filesystem.h"

#include "snd_device.h"
#include "sound_private.h"
#include "snd_audio_source.h"
#include "snd_convars.h"
#include "snd_dev_xaudio.h"
#include "snd_sfx.h"
#include "snd_wave_source.h"
#include "snd_wave_data.h"
#include "snd_wave_mixer_private.h"
#include "snd_io.h"

#include "audio/private/posix_stubs.h"
#include "../../host.h"
#include "xwvfile.h"
import std;

// This determines how much data to pre-cache (will invalidate per-map caches if changed).
constexpr auto SOUND_DIRECTORY_LENGTH = 6; // i.e., V_strlen( "sound/" )
inline constexpr const char *MASTER_CACHE_NAME = "_master";
#define SOUND_PREFETCH_FILE	"scripts/sound_prefetch.txt"

extern ConVar snd_async_spew_blocking;
extern double realtime;
ConVar snd_async_minsize("snd_async_minsize", "262144");
static ConVar snd_prefetch_common( "snd_prefetch_common", "1", FCVAR_RELEASE, "Prefetch common sounds from directories specified in " SOUND_PREFETCH_FILE );

ConVar force_audio_english( "force_audio_english", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Keeps track of whether we're forcing english in a localized language." );

// #define DEBUG_CHUNKS

//-----------------------------------------------------------------------------
// Purpose: Report chunk error
// Input  : id - chunk FOURCC
//-----------------------------------------------------------------------------
void ChunkError( unsigned int id )
{
}

//-----------------------------------------------------------------------------
// Purpose: Determine a true sample count for an ADPCM blob
//-----------------------------------------------------------------------------
int ADPCMSampleCount( ADPCMWAVEFORMAT *pFormat, int length )
{
	// determine a true sample count
	int nChannels = ( pFormat->wfx.nChannels );
	int wSamplesPerBlock = ( pFormat->wSamplesPerBlock );

	int blockSize = (( wSamplesPerBlock - 2) * nChannels ) / 2;
	blockSize += 7 * nChannels;

	int blockCount = length / blockSize;
	int blockRem = length % blockSize;
	
	// total samples in complete blocks
	int sampleCount = blockCount * wSamplesPerBlock;

	// add remaining in a short block
	if ( blockRem )
	{
		sampleCount += wSamplesPerBlock - (((blockSize - blockRem) * 2) / nChannels );
	}

	return sampleCount;
}

//-----------------------------------------------------------------------------
// Purpose: Init to empty wave
//-----------------------------------------------------------------------------
CAudioSourceWave::CAudioSourceWave( AudioSourceKind kind, CSfxTable *pSfx ) : CAudioSource( kind )
{
	m_format = 0;
	m_pHeader = nullptr;
	m_nHeaderSize = 0;

	// no looping
	m_loopStart = -1;

	m_sampleSize = 1;
	m_sampleCount = 0;
	m_bits = 0;
	m_channels = 0;
	m_dataStart = 0;
	m_dataSize = 0;
	m_rate = 0;

	m_refCount = 0;

	m_pSfx = pSfx;
#ifdef _DEBUG
	if ( m_pSfx )
	{
		char buf[MAX_PATH];
		m_pDebugName = MemAlloc_StrDup( m_pSfx->getname(buf, sizeof(buf)) );
	}
#endif

	m_bNoSentence = false;
	m_pTempSentence = nullptr;
	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
	m_bIsSentenceWord = false;

	m_numDecodedSamples = 0;

	// TERROR: limit data read while rebuilding cache
	m_bIsRebuildingCache = false;
}

CAudioSourceWave::CAudioSourceWave( AudioSourceKind kind, CSfxTable *pSfx, CAudioSourceCachedInfo *info ) : CAudioSource( kind )
{
	m_pSfx = pSfx;
#ifdef _DEBUG
	if ( m_pSfx )
	{
		char buf[MAX_PATH];
		m_pDebugName = MemAlloc_StrDup( m_pSfx->getname(buf, sizeof(buf)) );
	}
#endif

	m_refCount = 0;

	m_pHeader = nullptr;
	m_nHeaderSize = 0;

	if ( info->HeaderData() )
	{
		m_pHeader = new char[ info->HeaderSize() ];
		contract_assert( m_pHeader );
		V_memcpy( m_pHeader, info->HeaderData(), info->HeaderSize() );
		m_nHeaderSize = info->HeaderSize();
	}
	
	m_bits = info->Bits();
	m_channels = info->Channels();
	m_sampleSize = info->SampleSize();
	m_format = info->Format();
	m_dataStart = info->DataStart();
	m_dataSize = info->DataSize();
	m_rate = info->SampleRate();
	m_loopStart = info->LoopStart();
	m_sampleCount = info->SampleCount();
	m_numDecodedSamples = m_sampleCount;

	if ( m_format == WAVE_FORMAT_ADPCM && m_pHeader )
	{
		m_numDecodedSamples = ADPCMSampleCount( (ADPCMWAVEFORMAT *)m_pHeader, m_sampleCount );
	}

	m_bNoSentence = false;
	m_pTempSentence = nullptr;
	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
	m_bIsSentenceWord = false;

	// TERROR: limit data read while rebuilding cache
	m_bIsRebuildingCache = false;
}


CAudioSourceWave::~CAudioSourceWave( void )
{
#if _DEBUG
	if ( !CanDelete() )
		contract_assert( 0 );
#endif

	// for non-standard waves, we store a copy of the header in RAM
	delete[] m_pHeader;
	delete m_pTempSentence;
}

int CAudioSourceWave::GetType( void )
{
	return AUDIO_SOURCE_WAV;
}

void CAudioSourceWave::GetCacheData( CAudioSourceCachedInfo *info )
{
	contract_assert( info->Type() == CAudioSource::AUDIO_SOURCE_WAV );

	int nFileSize;
	if (GetStartupData( nFileSize ) == false)
	{
		m_dataStart = 0;
		m_dataSize = nFileSize;

		// START OF HACK - HACK - HACK! Necessary for Portal 2 so it fits in one single DVD
		// If that's not a WAV file, assume that's an MP3
		// Unfortunately shortcomings of the sound cache force us to do this here
		info->SetBits( 8 );
		info->SetChannels( 1 );			// Make this more accurate at some point 
		info->SetSampleRate( 44100 );	// Make this more accurate at some point
		info->SetLoopStart( -1 );		// No loop
		info->SetSampleCount( m_dataSize );
		info->SetDataSize( m_dataSize );
		info->SetDataStart( 0 );
		info->SetType( CAudioSource::AUDIO_SOURCE_MP3 );
		// END OF HACK - HACK - HACK!
		return;
	}

	info->SetBits( m_bits );
	info->SetChannels( m_channels );
	info->SetSampleSize( m_sampleSize );
	info->SetFormat( m_format );
	info->SetDataStart( m_dataStart );	// offset of wave data chunk
	info->SetDataSize( m_dataSize );		// size of wave data chunk
	info->SetSampleRate( m_rate );
	info->SetLoopStart( m_loopStart );
	info->SetSampleCount( m_sampleCount );

	if ( m_pTempSentence )
	{
		CSentence *scopy = new CSentence;
		*scopy = *m_pTempSentence;
		info->SetSentence( scopy );

		// Wipe it down to basically nothing
		delete m_pTempSentence;
		m_pTempSentence = nullptr;
	}

	if ( m_pHeader && m_nHeaderSize > 0 )
	{
		byte *data = new byte[ m_nHeaderSize ];
		V_memcpy( data, m_pHeader, m_nHeaderSize );
		info->SetHeaderSize( m_nHeaderSize );
		info->SetHeaderData( data );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CAudioSourceWave::GetFileName( char *pOutBuf, std::size_t bufLen )
{
	return m_pSfx ? m_pSfx->GetFileName( pOutBuf, bufLen ) : "NULL m_pSfx";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceWave::IsAsyncLoad()
{
	CVProfScope _( "CAudioSourceWave::IsAsyncLoad", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( !m_AudioCacheHandle.IsValid() )
	{
		m_AudioCacheHandle.Get( Source().GetType(), m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}

	// If there's a bit of "cached data" then we don't have to lazy/async load (we still async load the remaining data,
	//  but we run from the cache initially)
	if ( m_dataSize > snd_async_minsize.GetInt() )
		return true;
	return ( m_nCachedDataSize > 0 ) ? false : true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceWave::CheckAudioSourceCache()
{

	contract_assert( m_pSfx );

	if ( !m_pSfx->IsPrecachedSound() )
	{
		return;
	}

	// This will "re-cache" this if it's not in this level's cache already
	m_AudioCacheHandle.Get( Source().GetType(), true, m_pSfx, &m_nCachedDataSize );
}

//-----------------------------------------------------------------------------
// Purpose: Init the wave data.
// Input  : *pHeaderBuffer - the RIFF fmt chunk
//			headerSize - size of that chunk
//-----------------------------------------------------------------------------
void CAudioSourceWave::Init( const char *pHeaderBuffer, int headerSize )
{
	const WAVEFORMATEX *pHeader = (const WAVEFORMATEX *)pHeaderBuffer;

	// copy the relevant header data
	m_format = ( pHeader->wFormatTag );
	m_bits = ( pHeader->wBitsPerSample );
	m_rate = ( pHeader->nSamplesPerSec );
	m_channels = ( pHeader->nChannels );
	m_sampleSize = (m_bits * m_channels)/8;
	
	// this can never be zero -- other functions divide by this. 
	// this should never happen, but avoid crashing
	if ( m_sampleSize <= 0 )
	{
		m_sampleSize = 1;
	}

	if ( m_format == WAVE_FORMAT_ADPCM )
	{
		// For non-standard waves (like ADPCM) store the header, it has the decoding coefficients
		m_pHeader = new char[headerSize];
		std::memcpy( m_pHeader, pHeader, headerSize );
		m_nHeaderSize = headerSize;
		
		// treat ADPCM sources as a file of bytes.  They are decoded by the mixer
		m_sampleSize = 1;
	}
}


int	CAudioSourceWave::SampleRate( void ) 
{ 
	return m_rate; 
}


//-----------------------------------------------------------------------------
// Purpose: Size of each sample
// Output : 
//-----------------------------------------------------------------------------
int	CAudioSourceWave::SampleSize( void ) 
{ 
	return m_sampleSize; 
}

//-----------------------------------------------------------------------------
// Purpose: Total number of samples in this source
// Output : int
//-----------------------------------------------------------------------------
int CAudioSourceWave::SampleCount( void ) 
{
	// caller wants real samples 
	return m_numDecodedSamples;
}

int	CAudioSourceWave::Format( void ) 
{ 
	return m_format; 
}

int	CAudioSourceWave::DataSize( void ) 
{ 
	return m_dataSize; 
}

bool CAudioSourceWave::IsVoiceSource()
{
	if ( GetSentence() )
	{
		if ( GetSentence()->GetVoiceDuck() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Do any sample conversion
//			For 8 bit PCM, convert to signed because the mixing routine assumes this
// Input  : *pData - pointer to sample data
//			sampleCount - number of samples
//-----------------------------------------------------------------------------
void CAudioSourceWave::ConvertSamples( char *pData, int sampleCount )
{
	if ( m_format == WAVE_FORMAT_PCM )
	{
		if ( m_bits == 8 )
		{
			for ( int i = 0; i < sampleCount*m_channels; i++ )
			{
				*pData = (unsigned char)((int)((unsigned)*pData) - 128);
				pData++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parse base chunks
// Input  : &walk - riff file to parse
//		  : chunkName - name of the chunk to parse
//-----------------------------------------------------------------------------
// UNDONE: Move parsing loop here and drop each chunk into a virtual function
//			instead of this being virtual.
void CAudioSourceWave::ParseChunk( IterateRIFF &walk, int chunkName )
{
	switch( chunkName )
	{
		case WAVE_CUE:
			ParseCueChunk( walk );
			break;
		case WAVE_SAMPLER:
			ParseSamplerChunk( walk );
			break;
		case WAVE_VALVEDATA:
			ParseSentence( walk );
			break;
		default:
			// unknown and don't care
			ChunkError( walk.ChunkName() );
			break;
	}
}

bool CAudioSourceWave::IsLooped( void ) 
{ 
	return (m_loopStart >= 0) ? true : false; 
}

bool CAudioSourceWave::IsStereoWav( void ) 
{ 
	return (m_channels == 2) ? true : false;
}

bool CAudioSourceWave::IsStreaming( void ) 
{ 
	return false; 
}


int CAudioSourceWave::GetCacheStatus( void )
{
	return AUDIO_IS_LOADED;
}

void CAudioSourceWave::CacheLoad( void )
{
}

void CAudioSourceWave::CacheUnload( void )
{
}

int	CAudioSourceWave::ZeroCrossingBefore( int sample )
{
	return sample;
}


int	CAudioSourceWave::ZeroCrossingAfter( int sample )
{
	return sample;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
void CAudioSourceWave::ParseSentence( IterateRIFF &walk )
{
	std::vector< char > buf( walk.ChunkSize() );
	walk.ChunkRead( buf.data() );

	m_pTempSentence = new CSentence();
	contract_assert( m_pTempSentence );

	m_pTempSentence->InitFromDataChunk( buf.data(), buf.size() );

	// Throws all phonemes into one word, discards sentence memory, etc.
	m_pTempSentence->MakeRuntimeOnly();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CSentence
//-----------------------------------------------------------------------------
CSentence *CAudioSourceWave::GetSentence( void )
{

	// Already checked and this wav doesn't have sentence data...
	if ( m_bNoSentence == true )
	{
		return nullptr;
	}

	// Look up sentence from cache
	CAudioSourceCachedInfo *info = m_AudioCacheHandle.FastGet();
	if ( !info )
	{
		info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}
	contract_assert( info );
	if ( !info )
	{
		m_bNoSentence = true;
		return nullptr;
	}

	CSentence *sentence = info->Sentence();
	if ( !sentence )
	{
		m_bNoSentence = true;
		return nullptr;
	}

	if ( sentence->m_bIsValid )
	{
		return sentence;
	}

	m_bNoSentence = true;

	return nullptr;
}

const char *CAudioSourceWave::GetName( char *pBuf, std::size_t bufLen )
{ 
	return m_pSfx ? m_pSfx->getname(pBuf, bufLen) : nullptr; 
}

int CAudioSourceWave::GetQuality()
{
	return ( m_format == WAVE_FORMAT_XMA ? m_quality : 0 );
}

PathTypeFilter_t GetAudioPathFilter()
{
	// XBX_IsAudioLocalized() is always false on PC, no localized audio path filtering needed
	// No audio exists outside of zips, all the audio is inside the zips
	return FILTER_CULLNONPACK;
}

//-----------------------------------------------------------------------------
// Load a native xaudio or legacy wav
//-----------------------------------------------------------------------------
bool CAudioSourceWave::GetXboxAudioStartupData()
{
	char		fileName[MAX_PATH];
	char		tempFileName[MAX_PATH];
	

	// try native optimal xma wav file first
	V_StripExtension( m_pSfx->GetFileName(fileName, sizeof(fileName)), tempFileName, sizeof( tempFileName ) );
	V_snprintf( fileName, sizeof( fileName ), "sound\\%s"".wav", tempFileName );

	V_FixDoubleSlashes( fileName );
	PathTypeQuery_t pathType;
	char szFullName[MAX_PATH];
	if ( !g_pFullFileSystem->RelativePathToFullPath( fileName, "GAME", szFullName, sizeof( szFullName ), GetAudioPathFilter(), &pathType ) )
	{
		// not found, not supported
		return false;
	}

	auto buf = g_pFullFileSystem->ReadFile( szFullName, "GAME", sizeof( xwvHeader_t ) );
	if ( !buf )
	{
		// not found, not supported
		return false;
	}
	else
	{
		ks::byte_reader headerReader( *buf );
		xwvHeader_t header = headerReader.read< xwvHeader_t >();
		if ( !headerReader.ok() )
		{
			return false;
		}
		xwvHeader_t* pHeader = &header;
		if ( pHeader->id != XWV_ID || pHeader->version != XWV_VERSION )
		{
			return false;
		}

		switch ( pHeader->format )
		{
		case XWV_FORMAT_XMA:
			m_format = WAVE_FORMAT_XMA;
			break;
		case XWV_FORMAT_PCM:
			m_format = WAVE_FORMAT_PCM;
			break;
		case XWV_FORMAT_MP3:
			m_format = WAVE_FORMAT_MP3;
			break;
		case XWV_FORMAT_TEMP:
			m_format = WAVE_FORMAT_TEMP;
			break;
		default:
			// unknown
			return false;
		}

		m_rate = pHeader->GetSampleRate();
		m_channels = pHeader->channels;
		m_dataStart = pHeader->dataOffset;
		m_dataSize = pHeader->dataSize;
		m_quality = pHeader->quality;

		m_loopStart = pHeader->loopStart;
		m_loopBlock = pHeader->loopBlock;
		m_numLeadingSamples	= pHeader->numLeadingSamples;
		m_numTrailingSamples = pHeader->numTrailingSamples;

		switch (m_format)
		{
		case WAVE_FORMAT_MP3:
		case WAVE_FORMAT_TEMP:
			// MP3 and temp format do not store numDecodedSamples correctly (in bytes instead of samples).
			// We would need to change makegamedata accordingly (corresponding code is commented).
			// However I [oliviern] did not want to change the data that late in Portal 2. So patch in the code instead.
			pHeader->numDecodedSamples /= m_channels * sizeof( short );

			// Pass through...

		case WAVE_FORMAT_XMA:
			// xma and MP3 are compressed blocks, trick to fool system to treat data as bytes, not samples
			// unfortunate, but callers must know xma context and provide offsets in samples or bytes
			m_bits = 16;
			m_sampleSize = 1;
			m_sampleCount = m_dataSize;
			break;

		default:
			m_bits = 16;
			m_sampleSize = sizeof( short ) * m_channels;
			m_sampleCount = m_dataSize / m_sampleSize;
			break;
		}

		// keep true decoded samples because cannot be easily determined
		m_numDecodedSamples = pHeader->numDecodedSamples;

		m_bNoSentence = true;

		if ( pHeader->staticDataSize )
		{
			// get optional data
			auto fileBuffer = g_pFullFileSystem->ReadFile( szFullName, "GAME", pHeader->staticDataSize, sizeof( xwvHeader_t ) );
			if ( !fileBuffer )
			{
				return false;
			}

			unsigned char *pData = (unsigned char *)fileBuffer->data() + sizeof( xwvHeader_t );
			if ( pHeader->GetSeekTableSize() )
			{
				// store off the seek table
				m_nHeaderSize = pHeader->GetSeekTableSize();
				m_pHeader = new char[m_nHeaderSize];
				V_memcpy( m_pHeader, pData, m_nHeaderSize );

				// advance past optional seek table
				pData += m_nHeaderSize;
			}

			if ( pHeader->vdatSize )
			{
				m_pTempSentence = new CSentence();
				contract_assert( m_pTempSentence );
				m_bNoSentence = false;

				// vdat is precompiled into minimal binary format and possibly compressed
				CLZMA lzma;
				
				if ( lzma.IsCompressed( pData ) )
				{
					// uncompress binary vdat and restore
					std::vector< std::byte > targetBuffer( lzma.GetActualSize( pData ) );
					lzma.Uncompress( pData, (unsigned char *)targetBuffer.data() );
					ks::byte_reader targetReader( targetBuffer );
					m_pTempSentence->CacheRestoreFromBuffer( targetReader );
				}
				else
				{
					ks::byte_reader fileReader( *fileBuffer );
					m_pTempSentence->CacheRestoreFromBuffer( fileReader );
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Bastardized construction routine.  This is just to avoid complex
//			constructor functions so code can be shared more easily by sub-classes
// Input  : *pFormatBuffer - RIFF header
//			formatSize - header size
//			&walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceWave::Setup( const char *pFormatBuffer, int formatSize, IterateRIFF &walk )
{
	Init( pFormatBuffer, formatSize );

	while ( walk.ChunkAvailable() )
	{
		DispatchParseChunk( walk, walk.ChunkName() );
		walk.ChunkNext();
	}
}


bool CAudioSourceWave::GetStartupData( int &nfileSize )
{
	char formatBuffer[1024];
	char nameBuf[MAX_PATH];
	const char *pName = m_pSfx->GetFileName(nameBuf, sizeof(nameBuf));
	InFileRIFF riff( pName, *g_pSndIO );
	nfileSize = riff.GetFileSize();

	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	int format = 0;
	int formatSize = 0;

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	while ( walk.ChunkAvailable() && format == 0 )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_FMT:
			{
				if ( walk.ChunkSize() <= sizeof( formatBuffer ) )
				{
					walk.ChunkRead( formatBuffer );
					formatSize = walk.ChunkSize();
					format = ((WAVEFORMATEX *)formatBuffer)->wFormatTag;
					if( ((WAVEFORMATEX *)formatBuffer)->wBitsPerSample > 16)
					{
						Warning("Unsupported %d-bit wave file %s\n", (int)((WAVEFORMATEX *)formatBuffer)->wBitsPerSample, pName);
					}
				}
			}
			break;
		default:
			{
				ChunkError( walk.ChunkName() );
			}
			break;
		}
		walk.ChunkNext();
	}

	// Not really a WAVE file or no format chunk, bail
	if ( !format )
	{
		return false;
	}

	Setup( formatBuffer, formatSize, walk );

	if ( !m_dataStart || !m_dataSize )
	{
		// failed during setup
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: parses loop information from a cue chunk
// Input  : &walk - RIFF iterator
// Output : int loop start position
//-----------------------------------------------------------------------------
void CAudioSourceWave::ParseCueChunk( IterateRIFF &walk )
{
	// Cue chunk as specified by RIFF format
	// see $/research/jay/sound/riffnew.htm
	struct 
	{
		unsigned int dwName; 
		unsigned int dwPosition;
		unsigned int fccChunk;
		unsigned int dwChunkStart;
		unsigned int dwBlockStart; 
		unsigned int dwSampleOffset;
	} cue_chunk;

	int cueCount;

	// assume that the cue chunk stored in the wave is the start of the loop
	// assume only one cue chunk, UNDONE: Test this assumption here?
	cueCount = walk.ChunkReadInt();
	if ( cueCount > 0 )
	{
		walk.ChunkReadPartial( &cue_chunk, sizeof(cue_chunk) );
		m_loopStart = ( cue_chunk.dwSampleOffset );
	}
}

//-----------------------------------------------------------------------------
// Purpose: parses loop information from a 'smpl' chunk
// Input  : &walk - RIFF iterator
// Output : int loop start position
//-----------------------------------------------------------------------------
void CAudioSourceWave::ParseSamplerChunk( IterateRIFF &walk )
{
	// Sampler chunk for MIDI instruments
	// Parse loop info from this chunk too
	struct SampleLoop
	{
		unsigned int	dwIdentifier;
		unsigned int	dwType;
		unsigned int	dwStart;
		unsigned int	dwEnd;
		unsigned int	dwFraction;
		unsigned int	dwPlayCount;
	};

	struct 
	{
		unsigned int	dwManufacturer;
		unsigned int	dwProduct;
		unsigned int	dwSamplePeriod;
		unsigned int	dwMIDIUnityNote;
		unsigned int	dwMIDIPitchFraction;
		unsigned int	dwSMPTEFormat;
		unsigned int	dwSMPTEOffset;
		unsigned int	cSampleLoops;
		unsigned int	cbSamplerData;
		struct SampleLoop Loops[1];
	} samplerChunk;

	// assume that the loop end is the sample end
	// assume that only the first loop is relevant

	walk.ChunkReadPartial( &samplerChunk, sizeof(samplerChunk) );
	if ( ( samplerChunk.cSampleLoops ) > 0 )
	{
		// only support normal forward loops
		if ( ( samplerChunk.Loops[0].dwType ) == 0 )
		{
			m_loopStart = ( samplerChunk.Loops[0].dwStart );
		}
#ifdef _DEBUG
		else
		{
			char nameBuf[MAX_PATH];
			Msg("Unknown sampler chunk type %d on %s\n", ( samplerChunk.Loops[0].dwType ), m_pSfx->GetFileName(nameBuf,sizeof(nameBuf)) );
		}
#endif
	}
	// else discard - this is some other non-loop sampler data we don't support
}


//-----------------------------------------------------------------------------
// Purpose: get the wave header
//-----------------------------------------------------------------------------
void *CAudioSourceWave::GetHeader( void )
{
	return m_pHeader;
}

//-----------------------------------------------------------------------------
// Gets the looping information. Some parameters are interpreted based on format
//-----------------------------------------------------------------------------
int CAudioSourceWave::GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples )
{
	if ( pLoopBlock )
	{
		// for xma, the block that contains the loop point
		*pLoopBlock = m_loopBlock;
	}

	if ( pNumLeadingSamples )
	{
		// for xma, the number of leading samples at the loop block to discard
		*pNumLeadingSamples = m_numLeadingSamples;
	}

	if ( pNumTrailingSamples )
	{
		// for xma, the number of trailing samples at the final block to discard
		*pNumTrailingSamples = m_numTrailingSamples;
	}

	// the loop point in samples
	return m_loopStart;
}

//-----------------------------------------------------------------------------
// Purpose: wrap the position wrt looping
// Input  : samplePosition - absolute position
// Output : int - looped position
//-----------------------------------------------------------------------------
int64 CAudioSourceWave::ConvertLoopedPosition( int64 samplePosition )
{
	switch ( m_format )
	{
	case WAVE_FORMAT_XMA:
	case WAVE_FORMAT_MP3:
	case WAVE_FORMAT_TEMP:
		// xma and mp3 mixer interprets loops and *always* sends a corrected position
		return samplePosition;
	}

	// if the wave is looping and we're past the end of the sample
	// convert to a position within the loop
	// At the end of the loop, we return a short buffer, and subsequent call
	// will loop back and get the rest of the buffer
	if ( m_loopStart >= 0 && samplePosition >= m_sampleCount )
	{
		// size of loop
		int loopSize = m_sampleCount - m_loopStart;
		// subtract off starting bit of the wave
		samplePosition -= m_loopStart;
		
		if ( loopSize )
		{
			// "real" position in memory (mod off extra loops)
			samplePosition = m_loopStart + (samplePosition % loopSize);
		}
		// ERROR? if no loopSize
	}

	return samplePosition;
}

//-----------------------------------------------------------------------------
// Purpose: remove the reference for the mixer getting deleted
// Input  : *pMixer - 
//-----------------------------------------------------------------------------
void CAudioSourceWave::ReferenceRemove( CAudioMixer *pMixer )
{
	m_refCount--;

	if ( m_refCount == 0 && ( ( IsPlayOnce() ) ) )
	{
		SetPlayOnce( false ); // in case it gets used again
		Source().CacheUnload();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add a mixer reference
// Input  : *pMixer - 
//-----------------------------------------------------------------------------
void CAudioSourceWave::ReferenceAdd( CAudioMixer *pMixer )
{
	m_refCount++;
}


//-----------------------------------------------------------------------------
// Purpose: return true if no mixers reference this source
//-----------------------------------------------------------------------------
bool CAudioSourceWave::CanDelete( void )
{
	if ( m_refCount > 0 )
		return false;

	return true;
}


CAudioSourceMemWave::CAudioSourceMemWave() :
	CAudioSourceWave( AudioSourceKind::MemWave, nullptr )
{
	m_hCache = 0;
	m_hStream = INVALID_STREAM_HANDLE;
}

CAudioSourceMemWave::CAudioSourceMemWave( CSfxTable *pSfx ) : 
	CAudioSourceWave( AudioSourceKind::MemWave, pSfx )
{
	m_hCache = 0;
	m_hStream = INVALID_STREAM_HANDLE;

}

CAudioSourceMemWave::CAudioSourceMemWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) :
	CAudioSourceWave( AudioSourceKind::MemWave, pSfx, info )
{
	m_hCache = 0;
	m_hStream = INVALID_STREAM_HANDLE;
}

CAudioSourceMemWave::~CAudioSourceMemWave() 
{
}

//-----------------------------------------------------------------------------
// Purpose: Creates a mixer and initializes it with an appropriate mixer
//-----------------------------------------------------------------------------
CAudioMixer *CAudioSourceMemWave::CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError )
{
	CAudioMixer *pMixer = CreateWaveMixer( CreateWaveDataMemory(*this), m_format, m_channels, m_bits, initialStreamPosition, skipInitialSamples, bUpdateDelayForChoreo );
	if ( pMixer )
	{
		ReferenceAdd( pMixer );
		soundError = SE_OK;
	}
	else
	{
		soundError = SE_CANT_CREATE_MIXER;
	}
	return pMixer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **pData - output pointer to samples
//			samplePosition - position (in samples not bytes) 
//			sampleCount - number of samples (not bytes)
// Output : int - number of samples available
//-----------------------------------------------------------------------------
int CAudioSourceMemWave::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{ 
	// handle position looping
	samplePosition = ConvertLoopedPosition( samplePosition );

	// how many samples are available (linearly not counting looping)
	int totalSampleCount = m_sampleCount - samplePosition;

	// may be asking for a sample out of range, clip at zero
	if ( totalSampleCount < 0 )
	{
		totalSampleCount = 0;
	}

	// clip max output samples to max available
	if ( sampleCount > totalSampleCount )
	{
		sampleCount = totalSampleCount;
	}

	// byte offset in sample database
	samplePosition *= m_sampleSize;

	// if we are returning some samples, store the pointer
	if ( sampleCount )
	{
		// Starting past end of "preloaded" data, just use regular cache
		if ( samplePosition >= m_nCachedDataSize )
		{
			*pData = GetDataPointer();
		}
		else
		{
			// Start async loader if we haven't already done so
			CacheLoad();

			// Return less data if we are about to run out of uncached data
			if ( samplePosition + ( sampleCount * m_sampleSize ) >= m_nCachedDataSize )
			{
				sampleCount = ( m_nCachedDataSize - samplePosition ) / m_sampleSize;
			}

			// Point at preloaded/cached data from .cache file for now
			*pData = GetCachedDataPointer();
		}

		if ( *pData )
		{
			*pData = (char *)*pData + samplePosition;
		}
		else
		{
			// End of data or some other problem
			sampleCount = 0;
		}
	}

	return sampleCount;
}


// Hardcoded macros to test for zero crossing
template <typename T1> [[gnu::always_inline]] inline constexpr auto ZERO_X_8( T1 b ) { return ((b)<8 && (b)>-8); }
template <typename T1> [[gnu::always_inline]] inline constexpr auto ZERO_X_16( T1 b ) { return ((b)<2048 && (b)>-2048); }

//-----------------------------------------------------------------------------
// Purpose: Search backward for a zero crossing starting at sample
// Input  : sample - starting point
// Output : position of zero crossing
//-----------------------------------------------------------------------------
int	CAudioSourceMemWave::ZeroCrossingBefore( int sample )
{
	char *pWaveData = GetDataPointer();

	if ( m_format == WAVE_FORMAT_PCM )
	{
		if ( m_bits == 8 )
		{
			char *pData = pWaveData + sample * m_sampleSize;
			bool zero = false;

			if ( m_channels == 1 )
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_8(*pData) )
						zero = true;
					else
					{
						sample--;
						pData--;
					}
				}
			}
			else
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_8(*pData) && ZERO_X_8(pData[1]) )
						zero = true;
					else
					{
						sample--;
						pData--;
					}
				}
			}
		}
		else
		{
			short *pData = (short *)(pWaveData + sample * m_sampleSize);
			bool zero = false;

			if ( m_channels == 1 )
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_16(*pData) )
						zero = true;
					else
					{
						pData--;
						sample--;
					}
				}
			}
			else
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_16(*pData) && ZERO_X_16(pData[1]) )
						zero = true;
					else
					{
						sample--;
						pData--;
					}
				}
			}
		}
	}
	return sample;
}


//-----------------------------------------------------------------------------
// Purpose: Search forward for a zero crossing
// Input  : sample - starting point
// Output : position of found zero crossing
//-----------------------------------------------------------------------------
int	CAudioSourceMemWave::ZeroCrossingAfter( int sample )
{
	char *pWaveData = GetDataPointer();
	contract_assert( pWaveData );
	if ( !pWaveData )
		return sample;

	if ( m_format == WAVE_FORMAT_PCM )
	{
		if ( m_bits == 8 )
		{
			char *pData = pWaveData + sample * m_sampleSize;
			bool zero = false;

			if ( m_channels == 1 )
			{
				while ( sample < SampleCount() && !zero )
				{
					if ( ZERO_X_8(*pData) )
						zero = true;
					else
					{
						sample++;
						pData++;
					}
				}
			}
			else
			{
				while ( sample < SampleCount() && !zero )
				{
					if ( ZERO_X_8(*pData) && ZERO_X_8(pData[1]) )
						zero = true;
					else
					{
						sample++;
						pData++;
					}
				}
			}
		}
		else
		{
			short *pData = (short *)(pWaveData + sample * m_sampleSize);
			bool zero = false;

			if ( m_channels == 1 )
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_16(*pData) )
						zero = true;
					else
					{
						pData++;
						sample++;
					}
				}
			}
			else
			{
				while ( sample > 0 && !zero )
				{
					if ( ZERO_X_16(*pData) && ZERO_X_16(pData[1]) )
						zero = true;
					else
					{
						sample++;
						pData++;
					}
				}
			}
		}
	}
	return sample;
}

//-----------------------------------------------------------------------------
// Purpose: parse chunks with unique processing to in-memory waves
// Input  : &walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::ParseChunk( IterateRIFF &walk, int chunkName )
{
	switch( chunkName )
	{
		// this is the audio data
		case WAVE_DATA:
			ParseDataChunk( walk );
			return;
	}

	CAudioSourceWave::ParseChunk( walk, chunkName );
}


//-----------------------------------------------------------------------------
// Purpose: reads the actual sample data and parses it
// Input  : &walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::ParseDataChunk( IterateRIFF &walk )
{	
	m_dataStart = walk.ChunkFilePosition() + 8;
	m_dataSize  = walk.ChunkSize();

	// 360 streaming model loads data later, but still needs critical member setup
	char *pData = nullptr;
	pData = GetDataPointer();
	if ( !pData )
	{
		char nameBuf[MAX_PATH];
		Error( "CAudioSourceMemWave (%s): GetDataPointer() failed.", m_pSfx ? m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)) : "m_pSfx = NULL" );
	}

	// load them into memory (bad!!, this is a duplicate read of the data chunk)
	walk.ChunkRead( pData );

	if ( m_format == WAVE_FORMAT_PCM )
	{
		// number of samples loaded
		m_sampleCount = m_dataSize / m_sampleSize;
		m_numDecodedSamples = m_sampleCount;
	}
	else if ( m_format == WAVE_FORMAT_ADPCM )
	{
		// The ADPCM mixers treat the wave source as a flat file of bytes.
		// Since each "sample" is a byte (this is a flat file), the number of samples is the file size
		m_sampleCount = m_dataSize;
		m_sampleSize = 1;

		// file says 4, output is 16
		m_bits = 16;

		m_numDecodedSamples = ADPCMSampleCount( (ADPCMWAVEFORMAT *)m_pHeader, m_dataSize );
	}

	// some samples need to be converted
	if ( pData )
	{
		ConvertSamples( pData, m_sampleCount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CAudioSourceMemWave::GetCacheStatus( void )
{
	CVProfScope _( "CAudioSourceMemWave::GetCacheStatus", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// NOTE: This will start the load if it isn't started
	bool bCacheValid, bIsMissing;
	bool bCompleted = wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid, &bIsMissing );
	if ( !bCacheValid )
	{
		char nameBuf[MAX_PATH];
		wavedatacache->RestartDataLoad( &m_hCache, m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), m_dataSize, m_dataStart );
	}
	if ( bCompleted )
		return AUDIO_IS_LOADED;
	if ( bIsMissing )
		return AUDIO_ERROR_LOADING;
	if ( wavedatacache->IsDataLoadInProgress( m_hCache ) )
		return AUDIO_LOADING;
	return AUDIO_NOT_LOADED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::CacheLoad( void )
{
	char nameBuf[MAX_PATH];
	// Commence lazy load?
	if ( m_hCache != 0 )
	{
		bool bCacheValid;
		wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid );
		if ( !bCacheValid )
		{
			wavedatacache->RestartDataLoad( &m_hCache, m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), m_dataSize, m_dataStart );
		}
		return;
	}

	m_hCache = wavedatacache->AsyncLoadCache( m_pSfx->GetFileName(nameBuf,sizeof(nameBuf)), m_dataSize, m_dataStart );
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::CacheUnload( void )
{
	if ( m_hCache != 0 )
	{
		wavedatacache->Unload( m_hCache );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : char
//-----------------------------------------------------------------------------
char *CAudioSourceMemWave::GetDataPointer( void )
{
	char *pWaveData = nullptr;

	char nameBuf[MAX_PATH];
	bool bSamplesConverted = false;

	if ( m_hCache == 0 )
	{
		// not in cache, start loading
		CacheLoad();
	}

	// mount the requested data, blocks if necessary
	// TERROR: limit data read while rebuilding cache
	int dataSize = m_dataSize;
	if ( m_bIsRebuildingCache )
	{
		const char *filename = m_pSfx->GetFileName(nameBuf, sizeof(nameBuf));
		if ( V_stristr( filename, "music" ) != nullptr )
		{
			dataSize = std::min( dataSize, 32768 );
		}
	}
	wavedatacache->GetDataPointer( 
		m_hCache, 
		m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), 
		dataSize, 
		m_dataStart, 
		(void **)&pWaveData, 
		0, 
		&bSamplesConverted );

	// If we have reloaded data from disk (async) and we haven't converted the samples yet, do it now
	// FIXME:  Is this correct for stereo wavs?
	if ( pWaveData && !bSamplesConverted )
	{
		ConvertSamples( pWaveData, m_dataSize/m_sampleSize );
		wavedatacache->SetPostProcessed( m_hCache, true );
	}
	return pWaveData;
}

//-----------------------------------------------------------------------------
// Purpose: Save a copy of the file name for instances to open later
// Input  : *pFileName - filename
//-----------------------------------------------------------------------------
CAudioSourceStreamWave::CAudioSourceStreamWave( CSfxTable *pSfx ) : CAudioSourceWave( AudioSourceKind::StreamWave, pSfx )
{
	m_pSfx = pSfx;
	m_dataStart = -1;
	m_dataSize = 0;
	m_sampleCount = 0;

}

CAudioSourceStreamWave::CAudioSourceStreamWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) : 
	CAudioSourceWave( AudioSourceKind::StreamWave, pSfx, info ) 
	,m_hWaveFileAccess( FILESYSTEM_INVALID_HANDLE )
{
	m_pSfx = pSfx;
	m_dataStart = info->DataStart();
	m_dataSize = info->DataSize();

	m_sampleCount = info->SampleCount();
}

//-----------------------------------------------------------------------------
// Purpose: free the filename buffer
//-----------------------------------------------------------------------------
CAudioSourceStreamWave::~CAudioSourceStreamWave( void )
{
	if ( m_hWaveFileAccess != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Close( m_hWaveFileAccess );
		m_hWaveFileAccess = FILESYSTEM_INVALID_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create an instance (mixer & wavedata) of this sound
// Output : CAudioMixer * - pointer to the mixer
//-----------------------------------------------------------------------------
CAudioMixer *CAudioSourceStreamWave::CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError )
{
	char fileName[MAX_PATH];
	const char *pFileName = m_pSfx->GetFileName(fileName, sizeof(fileName));

	// BUGBUG: Source constructs the IWaveData, mixer frees it, fix this?
	IWaveData *pWaveData = CreateWaveDataStream( *this, this, pFileName, m_dataStart, m_dataSize, m_pSfx, initialStreamPosition, skipInitialSamples, soundError );
	if ( pWaveData )
	{
		CAudioMixer *pMixer = CreateWaveMixer( pWaveData, m_format, m_channels, m_bits, initialStreamPosition, skipInitialSamples, bUpdateDelayForChoreo );
		if ( pMixer )
		{
			ReferenceAdd( pMixer );
			return pMixer;
		}

		// no mixer, delete the stream buffer/instance
		IWaveData::Delete( pWaveData );
	}

	return nullptr;
}

void CAudioSourceStreamWave::Prefetch()
{
	char nameBuf[MAX_PATH];
	PrefetchDataStream( m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), m_dataStart, m_dataSize );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CAudioSourceStreamWave::SampleToStreamPosition( int samplePosition )
{
	switch ( m_format )
	{
	case WAVE_FORMAT_XMA:
		if ( m_nHeaderSize != 0 )
		{
			// Run through the seek table to find the block closest to the desired sample. 
			// Each seek table entry is the index (counting from the beginning of the file) 
			// of the first sample in the corresponding block, but there's no entry for the 
			// first block (since the index would always be zero).
			int *pSeekTable = (int*)m_pHeader;
			int packet = 0;
			for ( int i = 0; i < m_nHeaderSize/(int)sizeof( int ); ++i )
			{
				if ( samplePosition < pSeekTable[i] )
				{
					packet = i;
					break;
				}
			}

			int streamPosition = ( packet == 0 ) ? 0 : ( packet - 1 ) * 2048;
			return streamPosition;
		}
		break;

	case WAVE_FORMAT_PCM:
		return samplePosition * m_sampleSize;
	}

	// Function is not supported
	char fileName[MAX_PATH];
	const char *pFileName = GetFileName( fileName, sizeof( fileName ) );
	Warning( "SampleToStreamPosition( %d ) is not supported for sound '%s'.\n", samplePosition, pFileName );
	// not in the expected format or lacking the seek table
	return -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CAudioSourceStreamWave::StreamToSamplePosition( int streamPosition )
{
	switch ( m_format )
	{
	case WAVE_FORMAT_XMA:
		if ( m_nHeaderSize != 0 )
		{
			int packet = streamPosition/2048;
			if ( packet <= 0 )
			{
				return 0;
			}
			if ( packet > m_nHeaderSize/(int)sizeof( int ) )
			{
				return m_numDecodedSamples;
			}

			return ((int*)m_pHeader)[packet - 1];
		}
		break;

	case WAVE_FORMAT_PCM:
		return streamPosition / m_sampleSize;
	}

	// Function is not supported
	char fileName[MAX_PATH];
	const char *pFileName = GetFileName( fileName, sizeof( fileName ) );
	Warning( "StreamToSamplePosition( %d ) is not supported for sound '%s'.\n", streamPosition, pFileName );
	// not in the expected format or lacking the seek table
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Parse a stream wave file chunk
//			unlike the in-memory file, don't load the data, just get a reference to it.
// Input  : &walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceStreamWave::ParseChunk( IterateRIFF &walk, int chunkName )
{
	// NOTE: It would be nice to break out of parsing once we have the data start and
	//		save seeking over the whole file.  But to do so, the other needed chunks must occur
	//		before the DATA chunk.  But, that is not standard and breaks most other wav parsers.

	switch( chunkName )
	{
	case WAVE_DATA:
		// data starts at chunk + 8 (chunk name, chunk size = 2*4=8 bytes)
		// don't load the data, just know where it is so each instance 
		// can load it later
		m_dataStart = walk.ChunkFilePosition() + 8;
		m_dataSize = walk.ChunkSize();
		m_sampleCount = m_dataSize / m_sampleSize;
		return;
	}
	CAudioSourceWave::ParseChunk( walk, chunkName );
}

//-----------------------------------------------------------------------------
// Purpose: This is not implemented here.  This source has no data.  It is the
//			WaveData's responsibility to load/serve the data
//-----------------------------------------------------------------------------
int CAudioSourceStreamWave::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	// Only -tools mode uses this to build a "preview" of the wave form for PCM data only
	if ( GetType() == WAVE_FORMAT_PCM ) 
	{
		if ( m_hWaveFileAccess == FILESYSTEM_INVALID_HANDLE )
		{
			char buf[ MAX_PATH ];
			const char *pFilename = GetFileName( buf, sizeof( buf ) );
			m_hWaveFileAccess = g_pFullFileSystem->Open( CFmtStr( "sound\\%s", pFilename ), "rb", "GAME" );
		}

		if ( m_hWaveFileAccess != FILESYSTEM_INVALID_HANDLE )
		{
			g_pFullFileSystem->Seek( m_hWaveFileAccess, m_dataStart + samplePosition * SampleSize(), FILESYSTEM_SEEK_HEAD );
			if ( copyBuf != nullptr )
			{
				g_pFullFileSystem->Read( copyBuf, sampleCount * SampleSize(), m_hWaveFileAccess ); 
			}
			*pData = copyBuf;
			return sampleCount;
		}
	}
	return 0;
}

int CAudioSourceStreamWave::GetCacheStatus( void )
{
	if ( !m_dataSize || !m_dataStart )
	{
		// didn't get precached properly
		return AUDIO_NOT_LOADED;
	}

	return AUDIO_IS_LOADED;
}


//-----------------------------------------------------------------------------
// Purpose: Create a wave audio source (streaming or in memory)
// Input  : *pName - file name (NOTE: CAUDIOSOURCE KEEPS A POINTER TO pSfx)
//			streaming - if true, don't load, stream each instance
// Output : CAudioSource * - a new source
//-----------------------------------------------------------------------------
CAudioSource *CreateWave( CSfxTable *pSfx, bool bStreaming )
{
	contract_assert( pSfx );

#if defined( _DEBUG )
	// For some reason you can't usually do pSfx->getname() in the dev studio debugger, so for convenience we'll grab the name
	// here in debug builds at least...
	char nameBuf[MAX_PATH];
	char const *pName = pSfx->getname( nameBuf, sizeof(nameBuf) );
	(void)pName;
#endif

	CAudioSourceWave *pWave = nullptr;

	// Caching should always work, so if we failed to cache, it's a problem reading the file data, etc.
	bool bIsMapSound = pSfx->IsPrecachedSound();
	CAudioSourceCachedInfo *pInfo = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_WAV, bIsMapSound, pSfx );

	if ( pInfo && pInfo->Type() != CAudioSource::AUDIO_SOURCE_UNK )
	{
		// create the source from this file
		if ( bStreaming )
		{
			pWave = new CAudioSourceStreamWave( pSfx, pInfo );
		}
		else
		{
			pWave = new CAudioSourceMemWave( pSfx, pInfo );
		}
	}
	if ( pWave && !pWave->Format() )
	{
		// lack of format indicates failure
		CAudioSource::Delete( pWave );
		pWave = nullptr;
	}

	return pWave;
}


static bool Audio_IsMP3( const char *pName )
{
	int len = std::strlen(pName);
	if ( len > 4 )
	{
		if ( !V_strnicmp( &pName[len - 4], ".mp3", 4 ) )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// MP3 sounds cannot be played: the decoder was a separate library (vaudio_miles) that is not part of
// the game, so there is no source for them. Starting one fails with SE_CANT_CREATE_MIXER, which
// S_StartSound() reports as "Can't create mixer."
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Wrapper for CreateWave()
//-----------------------------------------------------------------------------
CAudioSource *Audio_CreateStreamedWave( CSfxTable *pSfx, SoundError &soundError )
{
	char nameBuf[MAX_PATH];
	if ( Audio_IsMP3( pSfx->GetFileName( nameBuf, sizeof(nameBuf) ) ) )
	{
		soundError = SE_CANT_CREATE_MIXER;
		return nullptr;
	}

	return CreateWave( pSfx, true );
}


//-----------------------------------------------------------------------------
// Purpose: Wrapper for CreateWave()
//-----------------------------------------------------------------------------
CAudioSource *Audio_CreateMemoryWave( CSfxTable *pSfx, SoundError &soundError )
{
	char nameBuf[MAX_PATH];
	if ( Audio_IsMP3( pSfx->GetFileName( nameBuf, sizeof(nameBuf) ) ) )
	{
		soundError = SE_CANT_CREATE_MIXER;
		return nullptr;
	}

	// In case the WAV file is actually an MP3 file.
	bool bIsMapSound = pSfx->IsPrecachedSound();
	// We pass AUDIO_SOURCE_WAV to not break the sound cache hack :(
	CAudioSourceCachedInfo *pInfo = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_WAV, bIsMapSound, pSfx );
	if ( pInfo->Type() == CAudioSource::AUDIO_SOURCE_MP3 )
	{
		soundError = SE_CANT_CREATE_MIXER;
		return nullptr;
	}

	return CreateWave( pSfx, false );
}

float GetMP3Duration_Helper( char const *filename );
static float Audio_GetMP3Duration( char const *pName )
{
	// Deduce from file
	return GetMP3Duration_Helper( pName );
}


void MaybeReportMissingWav( char const *wav )
{
	static ks::intern_pool wavErrors;

	if ( !wavErrors.contains( wav ) )
	{
		// See if file exists
		if ( g_pFullFileSystem->FileExists( wav ) )
		{
			DevWarning( "Bad Audio file '%s'\n", wav );
		}
		else
		{
			DevWarning( "Missing wav file '%s'\n", wav );
		}
		wavErrors.intern( wav );
	}
}

static float Audio_GetADPCMWaveDuration( char const *pName )
{
	char formatBuffer[1024];
	WAVEFORMATEX *pfmt = (WAVEFORMATEX *)formatBuffer;

	InFileRIFF riff( pName, *g_pSndIO );

	if ( riff.RIFFName() != RIFF_WAVE )
	{
		MaybeReportMissingWav( pName );
		return 0.0f;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	int format = 0;
	int formatSize = 0;
	int sampleCount = 0;

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	while ( walk.ChunkAvailable() && ( format == 0 || sampleCount == 0 ) )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_FMT:
			if ( walk.ChunkSize() <= sizeof( formatBuffer ) )
			{
				walk.ChunkRead( formatBuffer );
				formatSize = walk.ChunkSize();
				format = ( pfmt->wFormatTag );
			}
			break;
		case WAVE_DATA:
			if ( format != 0 )
			{
				int dataSize = walk.ChunkSize();
				if ( format == WAVE_FORMAT_ADPCM )
				{
					// Dummy size for now
					sampleCount = dataSize;
				}
				else
				{
					sampleCount = dataSize / ( ( pfmt->wBitsPerSample ) >> 3 );
				}
			}
			break;
		default:
			ChunkError( walk.ChunkName() );
			break;
		}
		walk.ChunkNext();
	}

	float sampleRate = ( pfmt->nSamplesPerSec );

	// Determine actual duration
	sampleCount = ADPCMSampleCount( (ADPCMWAVEFORMAT *)formatBuffer, sampleCount );
	
	return (float)sampleCount / sampleRate;
}


static float Audio_GetWaveDuration( char const *pName )
{

	CAudioSourceCachedInfo *pInfo = audiosourcecache->GetInfoByName( CFmtStr( "sound/%s", PSkipSoundChars( pName ) ) );

	if ( !pInfo )
	{
		return 0.0f;
	}

	int nFormat = pInfo->Format();
	int nSampleCount = pInfo->SampleCount();
	int nSampleRate = pInfo->SampleRate();

	// Not really a WAVE file or no format chunk, bail
	if ( !nFormat || !nSampleCount )
		return 0.0f;

	if ( nFormat == WAVE_FORMAT_ADPCM )
	{
		return Audio_GetADPCMWaveDuration( pName );
	}
	else
	{
		return (float)nSampleCount / nSampleRate;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fast method for determining duration of .wav/.mp3, exposed to server as well
// Input  : *pName - 
// Output : float
//-----------------------------------------------------------------------------
float AudioSource_GetSoundDuration( char const *pName )
{
#if defined( MP3_SUPPORT )
	if ( Audio_IsMP3( pName ) )
	{
		return Audio_GetMP3Duration( pName );
	}
#endif


	return Audio_GetWaveDuration( pName );
}

float AudioSource_GetSoundDuration( CSfxTable *pSfx )
{
	if ( pSfx && pSfx->pSource )
	{
		return (float)pSfx->pSource->SampleCount() / (float)pSfx->pSource->SampleRate();
	}

	return 0;
}

CAudioSourceCachedInfo::CAudioSourceCachedInfo() :
	infolong( 0 ),
	flagsbyte( 0 ),
	m_dataStart( 0 ),
	m_dataSize( 0 ),
	m_loopStart( 0 ),
	m_sampleCount( 0 ),
	m_usCachedDataSize( 0 ),
	m_pCachedData( 0 ),
	m_usHeaderSize( 0 ),
	m_pHeader( 0 ),
	m_pSentence( 0 )
{
}

CAudioSourceCachedInfo& CAudioSourceCachedInfo::operator =( const CAudioSourceCachedInfo& src )
{
	if ( this == &src )
		return *this;

	infolong = src.infolong;
	flagsbyte = src.flagsbyte;
	SetDataStart( src.DataStart() );
	SetDataSize( src.DataSize() );
	SetLoopStart( src.LoopStart() );
	SetSampleCount( src.SampleCount() );

	CSentence *scopy = nullptr;
	if ( src.Sentence() )
	{
		scopy = new CSentence();
		*scopy = *src.Sentence();
	}
	SetSentence( scopy );

	byte *data = nullptr;

	contract_assert( src.CachedDataSize() == 0 || src.CachedData() );

	m_usCachedDataSize = 0;

	if ( src.CachedData() && src.CachedDataSize() > 0 )
	{
		SetCachedDataSize( src.CachedDataSize() );
		data = new byte[ src.CachedDataSize() ];
		contract_assert( data );
		V_memcpy( data, src.CachedData(), src.CachedDataSize() );
	}

	SetCachedData( data );

	data = nullptr;

	contract_assert( src.HeaderSize() == 0 || src.HeaderData() );

	m_usHeaderSize = 0;

	if ( src.HeaderData() && src.HeaderSize() > 0 )
	{
		SetHeaderSize( src.HeaderSize() );
		data = new byte[ src.HeaderSize() ];
		contract_assert( data );
		V_memcpy( data, src.HeaderData(), src.HeaderSize() );
	}

	SetHeaderData( data );

	return *this;
}

CAudioSourceCachedInfo::CAudioSourceCachedInfo( const CAudioSourceCachedInfo& src )
{
	if ( this == &src )
	{
		contract_assert( 0 );
		return;
	}

	infolong = src.infolong;
	flagsbyte = src.flagsbyte;
	SetDataStart( src.DataStart() );
	SetDataSize( src.DataSize() );
	SetLoopStart( src.LoopStart() );
	SetSampleCount( src.SampleCount() );

	CSentence *scopy = nullptr;
	if ( src.Sentence() )
	{
		scopy = new CSentence();
		*scopy = *src.Sentence();
	}
	SetSentence( scopy );

	byte *data = nullptr;

	contract_assert( src.CachedDataSize() == 0 || src.CachedData() );

	m_usCachedDataSize = 0;

	if ( src.CachedData() && src.CachedDataSize() > 0 )
	{
		SetCachedDataSize( src.CachedDataSize() );
		data = new byte[ src.CachedDataSize() ];
		contract_assert( data );
		V_memcpy( data, src.CachedData(), src.CachedDataSize() );
	}

	SetCachedData( data );

	data = nullptr;

	contract_assert( src.HeaderSize() == 0 || src.HeaderData() );

	m_usHeaderSize = 0;

	if ( src.HeaderData() && src.HeaderSize() > 0 )
	{
		SetHeaderSize( src.HeaderSize() );
		data = new byte[ src.HeaderSize() ];
		contract_assert( data );
		V_memcpy( data, src.HeaderData(), src.HeaderSize() );
	}

	SetHeaderData( data );
}

CAudioSourceCachedInfo::~CAudioSourceCachedInfo()
{
	Clear();
}

void CAudioSourceCachedInfo::Clear()
{
	infolong = 0;
	flagsbyte = 0;
	m_dataStart = 0;
	m_dataSize = 0;
	m_loopStart = 0;
	m_sampleCount = 0;

	delete m_pSentence;
	m_pSentence = nullptr;

	delete[] m_pCachedData;
	m_pCachedData = nullptr;
	m_usCachedDataSize = 0;

	delete[] m_pHeader;
	m_pHeader = nullptr;
	m_usHeaderSize = 0;
}

void CAudioSourceCachedInfo::RemoveData()
{
	delete[] m_pCachedData;
	m_pCachedData = nullptr;
	m_usCachedDataSize = 0;
	flags.m_bCachedData = false;
}

void CAudioSourceCachedInfo::Save( ks::byte_writer& buf )
{
	buf.write< int >( infolong );
	buf.write< char >( flagsbyte );
	buf.write< int >( m_dataStart );
	buf.write< int >( m_dataSize );
	buf.write< int >( m_loopStart );
	buf.write< int >( m_sampleCount );

	if ( flags.m_bSentence )
	{
		m_pSentence->CacheSaveToBuffer( buf, CACHED_SENTENCE_VERSION );
	}

	contract_assert( m_usCachedDataSize < 65535 );

	if ( flags.m_bCachedData && m_pCachedData )
	{
		buf.write< int >( m_usCachedDataSize );
		buf.write_bytes( m_pCachedData, m_usCachedDataSize );
	}

	contract_assert( m_usHeaderSize <= 32767 );

	if ( flags.m_bHeader )
	{
		buf.write< short >( m_usHeaderSize );
		buf.write_bytes( m_pHeader, m_usHeaderSize );
	}
}

void CAudioSourceCachedInfo::Restore( ks::byte_reader& buf )
{
	// Wipe any old data!!!
	Clear();

	infolong		= buf.read< int >();
	flagsbyte		= buf.read< char >();
	m_dataStart		= buf.read< int >();
	m_dataSize		= buf.read< int >();
	m_loopStart		= buf.read< int >();
	m_sampleCount	= buf.read< int >();
	if ( flags.m_bSentence )
	{
		m_pSentence = new CSentence();
		contract_assert( m_pSentence );
		m_pSentence->CacheRestoreFromBuffer( buf );
	}

	if ( flags.m_bCachedData )
	{
		m_usCachedDataSize = buf.read< int >();
		contract_assert( m_usCachedDataSize > 0 && m_usCachedDataSize < 65535 );
		if ( m_usCachedDataSize > 0 )
		{
			byte *data = new byte[ m_usCachedDataSize ];
			buf.read_bytes( data, m_usCachedDataSize );
			SetCachedData( data );
		}
	}

	if ( flags.m_bHeader )
	{
		m_usHeaderSize = buf.read< short >();
		contract_assert( m_usHeaderSize > 0 && m_usHeaderSize <= 32767 );
		if ( m_usHeaderSize > 0 )
		{
			byte *data = new byte[ m_usHeaderSize ];
			buf.read_bytes( data, m_usHeaderSize );
			SetHeaderData( data );
		}
	}
}

int CAudioSourceCachedInfo::s_CurrentType = CAudioSource::AUDIO_SOURCE_MAXTYPE;
CSfxTable *CAudioSourceCachedInfo::s_pSfx = nullptr;
bool CAudioSourceCachedInfo::s_bIsPrecacheSound = false;
static bool g_bSoundRebuilt = false;
void CAudioSourceCachedInfo::Rebuild( char const *filename )
{
	g_bSoundRebuilt = true;
	// Wipe any old data
	Clear();

	contract_assert( s_pSfx );
	contract_assert( s_CurrentType != CAudioSource::AUDIO_SOURCE_MAXTYPE );


	SetType( s_CurrentType );

	CAudioSource *as = nullptr;

	// Note though this instantiates a specific AudioSource subclass, it doesn't matter, we just need one for .wav
	switch ( s_CurrentType )
	{
	default:
	case CAudioSource::AUDIO_SOURCE_VOICE:
		break;
	case CAudioSource::AUDIO_SOURCE_WAV:
		// TERROR: limit data read while rebuilding cache
		{
			CAudioSourceMemWave *memAs = new CAudioSourceMemWave( s_pSfx );
			memAs->SetRebuildingCache();
			as = memAs;
		}
		break;
	}

	if ( as )
	{
		as->GetCacheData( this );
		CAudioSource::Delete( as );
	}
}

constexpr auto AUDIOSOURCE_CACHE_VERSION = 3;
static CAudioSourceCache	g_ASCache;
CAudioSourceCache *audiosourcecache = &g_ASCache;

unsigned int CAudioSourceCachedInfoHandle_t::s_nCurrentFlushCount = 1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceCachedInfoHandle_t::InvalidateCache()
{
	++s_nCurrentFlushCount;
}

bool CAudioSourceCache::LoadMasterCache( char const *pchLanguage, bool bAllowEmpty )
{
	m_szMasterCache = CFmtStr( "%s/_master%s.cache", AUDIOSOURCE_CACHE_ROOTDIR, pchLanguage );

	char fullpath[ MAX_PATH ];
	V_snprintf( fullpath, sizeof( fullpath ), "%s%s", m_szMODPath.c_str(), m_szMasterCache.c_str() );
	// Just for display
	V_FixSlashes( fullpath, INCORRECT_PATH_SEPARATOR );
	DevMsg(	1, "Trying cache :  '%s'\n", fullpath );

	CacheType_t *cache = AllocAudioCache( m_szMasterCache.c_str(), true );

	contract_assert( cache );
	if ( !cache->Init() || 
		( !bAllowEmpty && cache->Count() == 0 ) )
	{
		Warning( "Failed to init '%s'\n", m_szMasterCache.c_str() );
		m_szMasterCache = "";
		delete cache;
		return false;
	}
	else
	{
		DevMsg(	1, "Successfully loaded audio cache file\n" );
		SetCachePointer( CACHE_MASTER, cache );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceCache::Init( unsigned int memSize )
{
#if defined( _DEBUG )
	Msg( "CAudioSourceCache: Init\n" );
#endif

	if ( !wavedatacache->Init( memSize ) )
	{
		Error( "Unable to init wavedatacache system\n" );
		return false;
	}


	GetAudioCacheLanguageSuffix( m_szCurrentLanguage );

	if ( std::ssize( m_szCurrentLanguage ) > 0 )
	{
		DevMsg(	1, "Audio Caches using '%s' as suffix\n", m_szCurrentLanguage.c_str() );
	}

	char sz[ MAX_PATH ];
	V_strncpy( sz, g_pSoundServices->GetGameDir(), sizeof( sz ) );
	V_StripTrailingSlash( sz );

	// Special handling for -tempcontent so that audio caches are loaded out of the tempentent "GAME" dir rather than the main "MOD" dir.
	// To do this we pass in a full path so that the CCachedFileData doesn't get to specify that it only wants "MOD" search path
	if ( CommandLine()->FindParm( "-tempcontent" ) )
	{
		V_strncat( sz, "_tempcontent", sizeof( sz ), COPY_ALL_CHARACTERS );	
	}

	V_StripTrailingSlash( sz );

	char szDLCPath[ MAX_PATH ];

	int nHighestDLC = 1;
	for ( ;nHighestDLC <= 99; nHighestDLC++ )
	{
		V_snprintf( szDLCPath, sizeof( szDLCPath ), "%s_dlc%d", sz, nHighestDLC );
		if ( !g_pFullFileSystem->IsDirectory( szDLCPath ) )
		{
			// does not exist, highest dlc available is previous
			nHighestDLC--;
			break;
		}

		V_snprintf( szDLCPath, sizeof( szDLCPath ), "%s_dlc%d/dlc_disabled.txt", sz, nHighestDLC );
		if ( g_pFullFileSystem->FileExists( szDLCPath ) )
		{
			// disabled, highest dlc available is previous
			nHighestDLC--;
			break;
		}
	}

	if ( nHighestDLC > 0 )
	{
		V_snprintf( szDLCPath, sizeof( szDLCPath ), "%s_dlc%d", sz, nHighestDLC );
	}
	else
	{
		V_strncpy( szDLCPath, sz, sizeof( szDLCPath ) );
	}

	V_FixSlashes( szDLCPath );

	m_szMODPath = szDLCPath;
	// Add trailing slash
	m_szMODPath	+= CFmtStr( "%c", CORRECT_PATH_SEPARATOR );

	g_pFullFileSystem->CreateDirHierarchy( CFmtStr( "%s%s", m_szMODPath.c_str(), AUDIOSOURCE_CACHE_ROOTDIR ), "GAME" );

	// Assume failure
	SetCachePointer( CACHE_MASTER, nullptr );
	bool bSuccess = LoadMasterCache( m_szCurrentLanguage.c_str(), false );
	if ( !bSuccess && V_stricmp( m_szCurrentLanguage.c_str(), "" ) )
	{
		bSuccess = LoadMasterCache( "", true );
	}

	if ( !bSuccess )
	{
		Warning( "  .cache load failed, forcing rebuild [lang:%s]!\n", m_szCurrentLanguage.c_str() );
		BuildMasterPrecachedSoundsCache();
	}
	// Tools mode always tries to update sound cache for now
	else if ( CommandLine()->FindParm( "-tools" ) && 
		!CommandLine()->FindParm( "-norebuildaudiocache" ) )
	{
		UpdateMasterPrecachedSoundsCache();
	}

	if ( snd_prefetch_common.GetBool() )
	{
		PrefetchCommandSounds();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceCache::Shutdown()
{
#if defined( _DEBUG )
	Msg( "CAudioSourceCache: Shutdown\n" );
#endif

	if ( m_pMasterSoundCache )
	{
		m_pMasterSoundCache->Shutdown();
		delete m_pMasterSoundCache;
	}

	SetCachePointer( CACHE_MASTER, nullptr );

	wavedatacache->Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cachename - 
// Output : CacheType_t
//-----------------------------------------------------------------------------
CAudioSourceCache::CacheType_t *CAudioSourceCache::AllocAudioCache( char const *cachename, bool bNeverCheckDisk )
{

	CacheType_t *cache = new CacheType_t( CFmtStr( "%s%s", m_szMODPath.c_str(), cachename ), AUDIOSOURCE_CACHE_VERSION, nullptr, CACHED_FILE_USE_FILESIZE, bNeverCheckDisk );
	return cache;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mapname - 
//-----------------------------------------------------------------------------
void CAudioSourceCache::LevelInit( char const *mapname )
{
	m_szMapCacheBase = CFmtStr( "%s/%s", AUDIOSOURCE_CACHE_ROOTDIR, mapname );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceCache::LevelShutdown()
{

	// Get precached sound count and store manifest if running with -makereslists
	if ( !CommandLine()->FindParm( "-makereslists" ) )
	{
		return;
	}

	int count = g_pSoundServices->GetPrecachedSoundCount();

	if ( !count )
	{
		return;
	}

	// So that we only save this out once per level
	if ( g_pSoundServices->GetServerCount() == m_nServerCount )
	{
		return;
	}

	m_nServerCount = g_pSoundServices->GetServerCount();

	WriteManifest();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceCache::WriteManifest()
{

	int count = g_pSoundServices->GetPrecachedSoundCount();

	if ( !count )
	{
		DevMsg( "Skipping WriteManifest, no entries in sound precache string table\n" );
		return;
	}

	// Save manifest out to disk...
	ks::text_writer buf;

	for ( int i = 0; i < count; ++i )
	{
		char const *fn = g_pSoundServices->GetPrecachedSound( i );
		if ( fn && fn[ 0 ] )
		{
			char full[ 512 ];
			V_snprintf( full, sizeof( full ), "sound\\%s", PSkipSoundChars( fn ) );
			V_strlower( full );
			V_FixSlashes( full );

			// Write to file
			buf.printf( "\"%s\"\r\n", full );
		}
	}

	g_pFullFileSystem->CreateDirHierarchy( AUDIOSOURCE_CACHE_ROOTDIR, "MOD" );

	char manifest_name[ 512 ];
	V_snprintf( manifest_name, sizeof( manifest_name ), "%s.manifest", m_szMapCacheBase.c_str() );

	if ( g_pFullFileSystem->FileExists( manifest_name, "MOD" ) && 
		!g_pFullFileSystem->IsFileWritable( manifest_name, "MOD" ) )
	{
		g_pFullFileSystem->SetFileWritable( manifest_name, true, "MOD" );
	}

	// Now write to file
	FileHandle_t fh;
	fh = g_pFullFileSystem->Open( manifest_name, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFullFileSystem->Write( buf.c_str(), buf.size(), fh );
		g_pFullFileSystem->Close( fh );

		DevMsg( "WriteManifest:  Persisting cache manifest '%s' (%d entries)\n", manifest_name, count );
	}
	else
	{
		Warning( "WriteManifest:  Unable to persist cache manifest '%s', check file permissions\n", manifest_name );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioSourceCache::CacheType_t *CAudioSourceCache::LookUpCacheEntry( const char *fn, int audiosourcetype, bool soundisprecached, CSfxTable *sfx )
{

	// Hack to remember the type of audiosource to create if we need to recreate it
	CAudioSourceCachedInfo::s_CurrentType = audiosourcetype;
	CAudioSourceCachedInfo::s_pSfx = sfx;
	CAudioSourceCachedInfo::s_bIsPrecacheSound = false;

	CacheType_t *pCache = nullptr;

	// If building a cache, just shortcut through to target cache
	if ( m_pBuildingCache )
	{
		pCache = m_pBuildingCache;
	}
	else 
	{
		// Grab from the full master list
		pCache = m_pMasterSoundCache;
	}

	return pCache;
}


CAudioSourceCachedInfo *CAudioSourceCache::GetInfoByName( const char *soundName )
{

	CVProfScope _( "CAudioSourceCache::GetInfoByName", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );


	if ( !m_pMasterSoundCache )
	{
		return nullptr;		
	}

	return m_pMasterSoundCache->Get( soundName );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioSourceCachedInfo *CAudioSourceCache::GetInfo( int audiosourcetype, bool soundisprecached, CSfxTable *sfx )
{
	CVProfScope _( "CAudioSourceCache::GetInfo", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );


	contract_assert( sfx );

	char fn[ 512 ];
	char nameBuf[MAX_PATH];
	V_snprintf( fn, sizeof( fn ), "sound/%s", sfx->GetFileName( nameBuf, sizeof(nameBuf) ) );

	CAudioSourceCachedInfo *info = nullptr;
	CacheType_t *pCache = LookUpCacheEntry( fn, audiosourcetype, soundisprecached, sfx );
	if ( !pCache )
		return nullptr;

	info = pCache->Get( fn );

	if ( info && info->Format() == 0 )
	{
		if ( g_pFullFileSystem->FileExists( fn, "BSP" ) )
		{
			DevMsg( 1, "Forced rebuild of bsp cache sound '%s'\n", fn );
			info = pCache->RebuildItem( fn );
			contract_assert( info->Format() != 0 );
		}
	}

	return info;
}


void CAudioSourceCache::RebuildCacheEntry( int audiosourcetype, bool soundisprecached, CSfxTable *sfx )
{
	CVProfScope _( "CAudioSourceCache::GetInfo", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );


	contract_assert( sfx );

	char fn[ 512 ];
	char nameBuf[MAX_PATH];
	V_snprintf( fn, sizeof( fn ), "sound/%s", sfx->GetFileName( nameBuf, sizeof(nameBuf) ) );
	CacheType_t *pCache = LookUpCacheEntry( fn, audiosourcetype, soundisprecached, sfx );
	if ( !pCache )
		return;

	pCache->RebuildItem( fn );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cachename - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceCache::IsValidCache( char const *cachename )
{

	CacheType_t *cache = AllocAudioCache( cachename, true );

	// This will delete any outdated .cache files
	bool valid = cache->IsUpToDate();

	delete cache;

	return valid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cachename - 
//-----------------------------------------------------------------------------
void CAudioSourceCache::RemoveCache( char const *cachename )
{

	if ( g_pFullFileSystem->FileExists( cachename, "MOD" ) )
	{
		if ( !g_pFullFileSystem->IsFileWritable( cachename, "MOD" ) )
		{
			g_pFullFileSystem->SetFileWritable( cachename, true, "MOD" );
		}
		g_pFullFileSystem->RemoveFile( cachename, "MOD" );
	}
}

void CAudioSourceCache::SetCachePointer( SoundCacheType_t ptrType, CacheType_t *ptr )
{

	bool dirty = false;

	switch ( ptrType )
	{
	default:
		Error( "SetCachePointer with bogus type %i\n", (int)ptrType );
		break;
	case CACHE_MASTER:
		if ( m_pMasterSoundCache != ptr )
		{
			dirty = true;
			m_pMasterSoundCache = ptr;
		}
		break;
	case CACHE_BUILDING:
		if ( m_pBuildingCache != ptr )
		{
			dirty = true;
			m_pBuildingCache = ptr;
		}
		break;
	}

	if ( dirty )
	{
		CAudioSourceCachedInfoHandle_t::InvalidateCache();
	}
}

void CAudioSourceCache::ValidateSoundCache( char const *pchWavFile )
{
	contract_assert( m_pMasterSoundCache );
	if ( !m_pMasterSoundCache )
		return;

	m_pMasterSoundCache->SetNeverCheckDisk( false );

	// Touch the cache
	CSfxTable *pTable = S_PrecacheSound( &pchWavFile[ SOUND_DIRECTORY_LENGTH ] );
	if ( pTable && pTable->pSource )
	{
		CAudioSourceCachedInfo::s_CurrentType = pTable->pSource->GetType();
		CAudioSourceCachedInfo::s_pSfx = pTable;
		CAudioSourceCachedInfo::s_bIsPrecacheSound = false;

		g_bSoundRebuilt = false;

		m_pMasterSoundCache->RecheckItem( pchWavFile );

		if ( g_bSoundRebuilt )
		{
			Msg( " updated '%s'\n", (char *)&pchWavFile[ SOUND_DIRECTORY_LENGTH ] );
		}
	}

	m_pMasterSoundCache->SetNeverCheckDisk( true );

	// Persist data to HD if dirty
	if ( m_pMasterSoundCache->IsDirty() )
	{
		m_pMasterSoundCache->Save();
	}
}

bool CAudioSourceCache::UpdateMasterPrecachedSoundsCache()
{
	contract_assert( m_pMasterSoundCache );
	if ( !m_pMasterSoundCache )
		return true;

	float flStart = Plat_FloatTime();

	Msg( "Updating sound cache [%d entries]\n", m_pMasterSoundCache->Count() );

	std::set< FileNameHandle_t > soundsOnDisk;
	// Build recursive list of all wav files for all languages
	RecursiveBuildSoundList( soundsOnDisk, m_szCurrentLanguage.c_str(), "sound", "GAME" );

	Msg( "Found %d sound files on disk\n", (int)soundsOnDisk.size() );

	char soundname[ 512 ];

	m_pMasterSoundCache->SetNeverCheckDisk( false );
	m_pMasterSoundCache->ForceRecheckDiskInfo();

	int nUpdated = 0;

	for ( FileNameHandle_t handle : soundsOnDisk )
	{
		soundname[ 0 ] = 0;
		g_pFullFileSystem->String( handle, soundname, sizeof( soundname ) );

		g_bSoundRebuilt = false;

		// Touch the cache
		CSfxTable *pTable = S_PrecacheSound( &soundname[ SOUND_DIRECTORY_LENGTH ] );
		if ( pTable && pTable->pSource )
		{
			CAudioSourceCachedInfo::s_CurrentType = pTable->pSource->GetType();
			CAudioSourceCachedInfo::s_pSfx = pTable;
			CAudioSourceCachedInfo::s_bIsPrecacheSound = false;

			m_pMasterSoundCache->Get( soundname );
		}

		if ( g_bSoundRebuilt )
		{
			++nUpdated;
			Msg( " updated '%s'\n", (char *)&soundname[ SOUND_DIRECTORY_LENGTH ] );
		}
	}

	m_pMasterSoundCache->SetNeverCheckDisk( true );

	// Persist data to HD if dirty
	m_pMasterSoundCache->Save();

	float flEnd = Plat_FloatTime();

	Msg( "Updated %i out of %i cached files [%.3f msec]\n", nUpdated, (int)soundsOnDisk.size(), 1000.0f * ( flEnd - flStart ) );

	CAudioSourceCachedInfoHandle_t::InvalidateCache();

	return true;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : showprogress - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceCache::BuildMasterPrecachedSoundsCache()
{

	char fn[ 512 ];
	V_snprintf( fn, sizeof( fn ), "%s/%s%s.cache", AUDIOSOURCE_CACHE_ROOTDIR, MASTER_CACHE_NAME, m_szCurrentLanguage.c_str() );

	DevMsg(	1, "Fast Build Temp Cache:  '%s'\n", fn );

	// Blow away the cache if rebuilding, which will force a full cache build
	RemoveCache( fn );

	g_pSoundServices->CacheBuildingStart();	

	CacheType_t *pOtherNoData = nullptr;
	std::set< FileNameHandle_t > other;
	
	// Build recursive list of all wav files for all languages
	RecursiveBuildSoundList( other, m_szCurrentLanguage.c_str(), "sound", "GAME" );

	pOtherNoData = BuildCacheFromList( fn, other, true, 0.0f, 1.0f );
	if ( pOtherNoData )
	{
		if ( m_pMasterSoundCache )
		{
			// Don't shutdown/save, since we have a new one already
			delete m_pMasterSoundCache;
		}

		// Take over ptr
		SetCachePointer( CACHE_MASTER, pOtherNoData );
	}

	g_pSoundServices->CacheBuildingFinish();	

	return true;
}

//-----------------------------------------------------------------------------
void CAudioSourceCache::RecursiveBuildSoundList( std::set< FileNameHandle_t >& root, const char *pLanguage, const char *pCurrentDir, const char *pathID )
{
	FileFindHandle_t fh;
	FileNameHandle_t handle;

	char path[ 512 ];
    V_snprintf( path, sizeof( path ), "%s/*.*", pCurrentDir );

	V_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, pathID, &fh );
	if ( fn )
	{
		do
		{
			if ( *fn != '.' )
			{
				if ( g_pFullFileSystem->FindIsDirectory( fh ) )
				{
					char nextdir[ 512 ];
					V_snprintf( nextdir, sizeof( nextdir ), "%s/%s", pCurrentDir, fn );

					RecursiveBuildSoundList( root, pLanguage, nextdir, pathID );
				}
				else
				{
					char ext[ 10 ];
					V_ExtractFileExtension( fn, ext, sizeof( ext ) );

					if ( ( !V_stricmp( ext, "wav" ) || !V_stricmp( ext, "mp3" ) ) && !V_stristr( fn, ".360." ) && !V_stristr( fn, ".ps3." ) )
					{
						char relative[ 512 ];
						V_snprintf( relative, sizeof( relative ), "%s/%s", pCurrentDir, fn );

						V_FixSlashes( relative );
						handle = g_pFullFileSystem->FindOrAddFileName( relative );

						root.insert( handle );
					}
				}
			}

			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cachename - 
//			FileNameHandle_t - 
//			soundlist - 
//			fulldata - 
//			showprogress - 
// Output : CAudioSourceCache::CacheType_t
//-----------------------------------------------------------------------------
CAudioSourceCache::CacheType_t *CAudioSourceCache::BuildCacheFromList( char const *cachename, std::set< FileNameHandle_t >& soundlist, bool showprogress /*= false*/, float flProgressStart /*= 0.0f*/, float flProgressEnd /*= 1.0f*/ )
{

	float flStart = Plat_FloatTime();


	CacheType_t *newCache = nullptr;

	newCache = AllocAudioCache( cachename, false );
	contract_assert( newCache );
	if ( newCache->Init() )
	{
		SetCachePointer( CACHE_BUILDING, newCache );

		int visited = 0;

		int nSoundCount = (int)soundlist.size();
		for ( FileNameHandle_t handle : soundlist )
		{
			char soundname[ 512 ];
			soundname[ 0 ] = 0;
			if ( g_pFullFileSystem->String( handle, soundname, sizeof( soundname ) ) )
			{
				// Touch the cache
				// Force it to go into the "other" cache but to also appear as "full data" precache
				CSfxTable *pTable = S_PrecacheSound( &soundname[ SOUND_DIRECTORY_LENGTH ] );
				// This will "re-cache" this if it's not in this level's cache already
				if ( pTable && pTable->pSource )
				{
					GetInfo( pTable->pSource->GetType(), false, pTable );
				}
			}
			else
			{
				contract_assert( !"Unable to find FileNameHandle_t in fileystem list." );
			}

			++visited;

			if ( !( visited % 100 ) )
			{
				Msg( "  progress %i/%i (%i %%)\n",
					visited, nSoundCount, (int)( 100.0f * ( float) visited / (float) nSoundCount ) );
			}

			if ( showprogress )
			{
				float frac = ( float )( visited - 1 )/( float )nSoundCount;

				frac = flProgressStart + frac * ( flProgressEnd - flProgressStart );

				char base[ 256 ];
				V_FileBase( soundname, base, sizeof( base ) );
				V_strlower( base );
				g_pSoundServices->CacheBuildingUpdateProgress( frac, base );
			}
		}

		Msg( "Touched %i cached files\n", nSoundCount );

		SetCachePointer( CACHE_BUILDING, nullptr );

		// Persist data to HD if dirty
		newCache->Save();
	}
	else
	{
		delete newCache;
		newCache = nullptr;
	}

	float flEnd = Plat_FloatTime();
	Msg( "Elapsed time:  %.2f seconds\n",
		flEnd - flStart );

	return newCache;
}

//-----------------------------------------------------------------------------
void CAudioSourceCache::GetAudioCacheLanguageSuffix( std::string &sLanguage )
{
	char const *pchLanguage = g_pSoundServices->GetUILanguage();

	if ( !pchLanguage || !*pchLanguage || !V_stricmp( pchLanguage, "english" ) )
	{
		sLanguage = "";
		return;
	}
	
	//Check language right here to see if we need the caches for it.
	char szLanguageList[ MAX_PATH ];
	V_snprintf( szLanguageList, sizeof( szLanguageList ), "%s/localization_cache_list.txt", AUDIOSOURCE_CACHE_ROOTDIR );	
	FileHandle_t fh = g_pFullFileSystem->Open( szLanguageList, "r" );
	char szCacheLanguage[ MAX_LIST_SIZE ];
	if ( fh )
	{
		g_pFullFileSystem->Read( szCacheLanguage, MAX_LIST_SIZE, fh);
		g_pFullFileSystem->Close( fh );

		if ( V_stristr( szCacheLanguage, pchLanguage ) )
		{
			sLanguage = CFmtStr( "_%s", pchLanguage );
			return;
		}
		else
		{
			sLanguage = "";
			return;
		}
	}	

	sLanguage = CFmtStr( "_%s", pchLanguage );
	return;
}

void CAudioSourceCache::PrefetchCommandSounds()
{

	if ( !m_pMasterSoundCache )
		return;

	std::vector< std::string > vecSearch;
	if ( auto prefetchData = g_pFullFileSystem->ReadFile( SOUND_PREFETCH_FILE, "GAME" ) )
	{
		ks::text_reader buf( ks::text_view( *prefetchData ) );
		characterset_t breakSet;
		CharacterSetBuild( &breakSet, "" );

		// parse reslist
		char substr[MAX_PATH];
		for ( ;; )
		{
			auto token = buf.read_token( breakSet, true );
			if ( !token || token->empty() )
			{
				break;
			}

			const std::size_t tokenLen = std::min( token->size(), sizeof( substr ) - 1 );
			V_memcpy( substr, token->data(), tokenLen );
			substr[tokenLen] = 0;

			V_FixSlashes( substr );
			V_strlower( substr );
			vecSearch.push_back( std::string( substr ) );

			Msg( "Prefetching data for subdir:  %s\n", substr );
		}
	}

	// Nothing to do
	if ( vecSearch.empty() )
		return;

	COM_TimestampedLog( "PrefetchSounds Start" );
	float flStart = Plat_FloatTime();
	uint64 uBytesPrefeteched = 0ull;
	uint32 uSoundsPrefetched = 0u;
	// Now walk the cache and prefetch shiz
	for ( int i = 0 ; i < m_pMasterSoundCache->Count(); ++i )
	{
		char szFile[ MAX_PATH ];
		m_pMasterSoundCache->GetElementName( i, szFile, sizeof( szFile ) );
		V_FixSlashes( szFile );
		V_strlower( szFile );

		for ( int j = 0; j < std::ssize( vecSearch ); ++j )
		{
			const std::string &str = vecSearch[ j ];
			if ( V_stristr( szFile, str.c_str() ) )
			{
				++uSoundsPrefetched;
				const CAudioSourceCachedInfo *info = (*m_pMasterSoundCache)[ i ];
				uBytesPrefeteched += info->DataSize();

				S_PrefetchSound( szFile, false );
				break;
			}
		}
	}

	float flEnd = Plat_FloatTime();

	COM_TimestampedLog( "PrefetchSounds Finish" );
	Msg( "Prefetched %u sounds, %s [%.3f msec]\n",
		uSoundsPrefetched, V_pretifymem( uBytesPrefeteched ), 1000.0f * ( flEnd - flStart ) );
}

static void snd_rebuildaudiocache( const CCommand &args );
static ConCommand snd_rebuildaudiocache_command( "snd_rebuildaudiocache", snd_rebuildaudiocache, "rebuild audio cache for current language\n" );
static void snd_rebuildaudiocache( const CCommand &args )
{
	g_ASCache.BuildMasterPrecachedSoundsCache();
}

static void snd_writemanifest( const CCommand &args );
static ConCommand snd_writemanifest_command( "snd_writemanifest", snd_writemanifest, "If running a game, outputs the precache manifest for the current level\n" );
static void snd_writemanifest( const CCommand &args )
{
	g_ASCache.WriteManifest();
}

static void snd_updateaudiocache( const CCommand &args );
static ConCommand snd_updateaudiocache_command( "snd_updateaudiocache", snd_updateaudiocache, "checks _master.cache based on file sizes and rebuilds any change/new entries\n" );
static void snd_updateaudiocache( const CCommand &args )
{
	g_ASCache.UpdateMasterPrecachedSoundsCache();
}

void S_ValidateSoundCache( char const *pchWavFile )
{
	g_ASCache.ValidateSoundCache( pchWavFile );
}

//-----------------------------------------------------------------------------
// CAudioSource dispatch: the sources are a closed set (AudioSourceKind), so the interface switches
// on the kind and calls the member of the same name of the source's class. A class that does not
// define one inherits it from CAudioSourceWave.
//-----------------------------------------------------------------------------
#define KS_AUDIO_SOURCE_DISPATCH( call ) \
	switch ( m_Kind ) \
	{ \
	case AudioSourceKind::MemWave: return static_cast< CAudioSourceMemWave * >( this )->call; \
	case AudioSourceKind::StreamWave: return static_cast< CAudioSourceStreamWave * >( this )->call; \
	case AudioSourceKind::Voice: return static_cast< CAudioSourceVoice * >( this )->call; \
	} \
	std::unreachable()

void CAudioSource::Delete( CAudioSource *pSource )
{
	if ( !pSource )
		return;
	switch ( pSource->m_Kind )
	{
	case AudioSourceKind::MemWave: delete static_cast< CAudioSourceMemWave * >( pSource ); return;
	case AudioSourceKind::StreamWave: delete static_cast< CAudioSourceStreamWave * >( pSource ); return;
	case AudioSourceKind::Voice: delete static_cast< CAudioSourceVoice * >( pSource ); return;
	}
	std::unreachable();
}

CAudioMixer *CAudioSource::CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError )
{
	KS_AUDIO_SOURCE_DISPATCH( CreateMixer( initialStreamPosition, skipInitialSamples, bUpdateDelayForChoreo, soundError ) );
}

int CAudioSource::GetType( void )
{
	KS_AUDIO_SOURCE_DISPATCH( GetType(  ) );
}

void CAudioSource::GetCacheData( CAudioSourceCachedInfo *info )
{
	KS_AUDIO_SOURCE_DISPATCH( GetCacheData( info ) );
}

int CAudioSource::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	KS_AUDIO_SOURCE_DISPATCH( GetOutputData( pData, samplePosition, sampleCount, copyBuf ) );
}

int CAudioSource::SampleRate( void )
{
	KS_AUDIO_SOURCE_DISPATCH( SampleRate(  ) );
}

bool CAudioSource::IsVoiceSource(  )
{
	KS_AUDIO_SOURCE_DISPATCH( IsVoiceSource(  ) );
}

bool CAudioSource::IsPlayerVoice(  )
{
	KS_AUDIO_SOURCE_DISPATCH( IsPlayerVoice(  ) );
}

int CAudioSource::SampleSize( void )
{
	KS_AUDIO_SOURCE_DISPATCH( SampleSize(  ) );
}

int CAudioSource::SampleCount( void )
{
	KS_AUDIO_SOURCE_DISPATCH( SampleCount(  ) );
}

int CAudioSource::Format( void )
{
	KS_AUDIO_SOURCE_DISPATCH( Format(  ) );
}

int CAudioSource::DataSize( void )
{
	KS_AUDIO_SOURCE_DISPATCH( DataSize(  ) );
}

bool CAudioSource::IsLooped( void )
{
	KS_AUDIO_SOURCE_DISPATCH( IsLooped(  ) );
}

bool CAudioSource::IsStereoWav( void )
{
	KS_AUDIO_SOURCE_DISPATCH( IsStereoWav(  ) );
}

bool CAudioSource::IsStreaming( void )
{
	KS_AUDIO_SOURCE_DISPATCH( IsStreaming(  ) );
}

int CAudioSource::GetCacheStatus( void )
{
	KS_AUDIO_SOURCE_DISPATCH( GetCacheStatus(  ) );
}

void CAudioSource::CacheLoad( void )
{
	KS_AUDIO_SOURCE_DISPATCH( CacheLoad(  ) );
}

void CAudioSource::CacheUnload( void )
{
	KS_AUDIO_SOURCE_DISPATCH( CacheUnload(  ) );
}

CSentence *CAudioSource::GetSentence( void )
{
	KS_AUDIO_SOURCE_DISPATCH( GetSentence(  ) );
}

int CAudioSource::GetQuality( void )
{
	KS_AUDIO_SOURCE_DISPATCH( GetQuality(  ) );
}

int CAudioSource::ZeroCrossingBefore( int sample )
{
	KS_AUDIO_SOURCE_DISPATCH( ZeroCrossingBefore( sample ) );
}

int CAudioSource::ZeroCrossingAfter( int sample )
{
	KS_AUDIO_SOURCE_DISPATCH( ZeroCrossingAfter( sample ) );
}

void CAudioSource::ReferenceAdd( CAudioMixer *pMixer )
{
	KS_AUDIO_SOURCE_DISPATCH( ReferenceAdd( pMixer ) );
}

void CAudioSource::ReferenceRemove( CAudioMixer *pMixer )
{
	KS_AUDIO_SOURCE_DISPATCH( ReferenceRemove( pMixer ) );
}

bool CAudioSource::CanDelete( void )
{
	KS_AUDIO_SOURCE_DISPATCH( CanDelete(  ) );
}

void CAudioSource::Prefetch(  )
{
	KS_AUDIO_SOURCE_DISPATCH( Prefetch(  ) );
}

bool CAudioSource::IsAsyncLoad(  )
{
	KS_AUDIO_SOURCE_DISPATCH( IsAsyncLoad(  ) );
}

void CAudioSource::CheckAudioSourceCache(  )
{
	KS_AUDIO_SOURCE_DISPATCH( CheckAudioSourceCache(  ) );
}

char const *CAudioSource::GetFileName( char *pBuf, std::size_t bufLen )
{
	KS_AUDIO_SOURCE_DISPATCH( GetFileName( pBuf, bufLen ) );
}

void CAudioSource::SetPlayOnce( bool isPlayOnce )
{
	KS_AUDIO_SOURCE_DISPATCH( SetPlayOnce( isPlayOnce ) );
}

bool CAudioSource::IsPlayOnce(  )
{
	KS_AUDIO_SOURCE_DISPATCH( IsPlayOnce(  ) );
}

void CAudioSource::SetSentenceWord( bool bIsWord )
{
	KS_AUDIO_SOURCE_DISPATCH( SetSentenceWord( bIsWord ) );
}

bool CAudioSource::IsSentenceWord(  )
{
	KS_AUDIO_SOURCE_DISPATCH( IsSentenceWord(  ) );
}

int CAudioSource::SampleToStreamPosition( int samplePosition )
{
	KS_AUDIO_SOURCE_DISPATCH( SampleToStreamPosition( samplePosition ) );
}

int CAudioSource::StreamToSamplePosition( int streamPosition )
{
	KS_AUDIO_SOURCE_DISPATCH( StreamToSamplePosition( streamPosition ) );
}

// the streamed sources are the data source of a streaming wave data
int64 CAudioSource::UpdateLoopingSamplePosition( int64 samplePosition )
{
	switch ( m_Kind )
	{
	case AudioSourceKind::StreamWave: return static_cast< CAudioSourceStreamWave * >( this )->UpdateLoopingSamplePosition( samplePosition );
	default: break;
	}
	std::unreachable();
}

void CAudioSource::UpdateSamples( char *pData, int sampleCount )
{
	switch ( m_Kind )
	{
	case AudioSourceKind::StreamWave: static_cast< CAudioSourceStreamWave * >( this )->UpdateSamples( pData, sampleCount ); return;
	default: break;
	}
	std::unreachable();
}

int CAudioSource::GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples )
{
	switch ( m_Kind )
	{
	case AudioSourceKind::StreamWave: return static_cast< CAudioSourceStreamWave * >( this )->GetLoopingInfo( pLoopBlock, pNumLeadingSamples, pNumTrailingSamples );
	default: break;
	}
	std::unreachable();
}

// CAudioSourceWave::Setup() parses the chunks of the file with the ParseChunk of the source's class
void CAudioSourceWave::DispatchParseChunk( IterateRIFF &walk, int chunkName )
{
	switch ( m_Kind )
	{
	case AudioSourceKind::MemWave: static_cast< CAudioSourceMemWave * >( this )->ParseChunk( walk, chunkName ); return;
	case AudioSourceKind::StreamWave: static_cast< CAudioSourceStreamWave * >( this )->ParseChunk( walk, chunkName ); return;
	case AudioSourceKind::Voice: ParseChunk( walk, chunkName ); return;
	default: break;
	}
	std::unreachable();
}
