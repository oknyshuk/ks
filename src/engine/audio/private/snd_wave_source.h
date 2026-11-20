#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "snd_audio_source.h"
class IterateRIFF;
#include "sentence.h"
#include "snd_sfx.h"
import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.tier1.keyvalues;

//=============================================================================
// Functions to create audio sources from wave files or from wave data.
//=============================================================================
extern CAudioSource* Audio_CreateMemoryWave( CSfxTable *pSfx, SoundError &soundError );
extern CAudioSource* Audio_CreateStreamedWave( CSfxTable *pSfx, SoundError &soundError );

class CAudioSourceWave : public CAudioSource
{
public:
	CAudioSourceWave( AudioSourceKind kind, CSfxTable *pSfx );
	CAudioSourceWave( AudioSourceKind kind, CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceWave( void );

	int				GetType( void );
	void			GetCacheData( CAudioSourceCachedInfo *info );

	void					Setup( const char *pFormat, int formatSize, IterateRIFF &walk );
	int				SampleRate( void );
	int				SampleSize( void );
	int				SampleCount( void );

	int				Format( void );
	int				DataSize( void );

	void					*GetHeader( void );
	bool			IsVoiceSource();
	bool			IsPlayerVoice() { return false; }

	void			ParseChunk( IterateRIFF &walk, int chunkName );
	void			ParseSentence( IterateRIFF &walk );

	void					ConvertSamples( char *pData, int sampleCount );
	bool					IsLooped( void );
	bool					IsStereoWav( void );
	bool					IsStreaming( void );
	int						GetCacheStatus( void );
	int64					ConvertLoopedPosition( int64 samplePosition );
	void					CacheLoad( void );
	void					CacheUnload( void );
	int				ZeroCrossingBefore( int sample );
	int				ZeroCrossingAfter( int sample );
	void			ReferenceAdd( CAudioMixer *pMixer );
	void			ReferenceRemove( CAudioMixer *pMixer );
	bool			CanDelete( void );
	CSentence		*GetSentence( void );
	const char				*GetName(char *pBuf, std::size_t bufLen );
	int				GetQuality( void );

	bool			IsAsyncLoad();

	void			CheckAudioSourceCache();

	char const		*GetFileName( char *pOutBuffer, std::size_t bufLen );

	// 360 uses alternate play once semantics
	void			SetPlayOnce( bool bIsPlayOnce ) { m_bIsPlayOnce = true ? bIsPlayOnce : false; }
	bool			IsPlayOnce() { return true ? m_bIsPlayOnce : false; }

	void			SetSentenceWord( bool bIsWord ) { m_bIsSentenceWord = bIsWord; }
	bool			IsSentenceWord() { return m_bIsSentenceWord; }

	int						GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples );

	int				SampleToStreamPosition( int samplePosition ) { return 0; }
	int				StreamToSamplePosition( int streamPosition ) { return 0; }

	// TERROR: limit data read while rebuilding cache
	void					SetRebuildingCache( void ) { m_bIsRebuildingCache = true; }

protected:
	// the source as the interface sees it: calls through it reach the member of the source's own class
	CAudioSource			&Source() { return *this; }

	// calls the ParseChunk of the source's class
	void					DispatchParseChunk( IterateRIFF &walk, int chunkName );

	void					ParseCueChunk( IterateRIFF &walk );
	void					ParseSamplerChunk( IterateRIFF &walk );

	void					Init( const char *pHeaderBuffer, int headerSize );
	bool					GetStartupData( int &nFileSize );
	bool					GetXboxAudioStartupData();

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		CVProfScope _( "CAudioSourceWave::GetCachedDataPointer", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			contract_assert( !"CAudioSourceWave::GetCachedDataPointer info == NULL" );
			return nullptr;
		}

		return (byte *)info->CachedData();
	}

	int				m_bits;
	int				m_rate;
	int				m_channels;
	int				m_format;
	int				m_sampleSize;
	int				m_loopStart;
	int				m_sampleCount;			// can be "samples" or "bytes", depends on format

	CSfxTable		*m_pSfx;
	CSentence		*m_pTempSentence;

	int				m_dataStart;			// offset of sample data
	int				m_dataSize;				// size of sample data

	char			*m_pHeader;
	int				m_nHeaderSize;

	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;

	int				m_nCachedDataSize;

	// number of actual samples (regardless of format)
	// compressed formats alter definition of m_sampleCount 
	// used to spare expensive calcs by decoders
	int				m_numDecodedSamples;

	// additional data needed by xma decoder to for looping
	unsigned short	m_loopBlock;			// the block the loop occurs in
	unsigned short	m_numLeadingSamples;	// number of leader samples in the loop block to discard
	unsigned short	m_numTrailingSamples;	// number of trailing samples in the final block to discard
	unsigned short	m_quality;

	unsigned int	m_bNoSentence : 1;
	unsigned int	m_bIsPlayOnce : 1;
	unsigned int	m_bIsSentenceWord : 1;
	unsigned int	m_bIsRebuildingCache : 1;	// TERROR: limit data read while rebuilding cache

private:
	CAudioSourceWave( const CAudioSourceWave & ); // not implemented, not allowed
	int				m_refCount;
	
#ifdef _DEBUG
	// Only set in debug mode so you can see the name.
	const char		*m_pDebugName;
#endif
};

// CAudioSourceMemWave is a bunch of wave data that is all in memory.
// To use it:
// - derive from CAudioSourceMemWave
// - call CAudioSourceWave::Init with a WAVEFORMATEX
// - set m_sampleCount.
// - implement GetDataPointer
class CAudioSourceMemWave : public CAudioSourceWave
{
public:
							CAudioSourceMemWave();
							CAudioSourceMemWave( CSfxTable *pSfx );
							CAudioSourceMemWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceMemWave();

	// These are all implemented by CAudioSourceMemWave.
	CAudioMixer*	CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError );
	int				GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	int				ZeroCrossingBefore( int sample );
	int				ZeroCrossingAfter( int sample );

	int				GetCacheStatus( void );
	void			CacheLoad( void );
	void			CacheUnload( void );

	// by definition, should already be in memory
	void			Prefetch() {}

	void			ParseChunk( IterateRIFF &walk, int chunkName );
	void					ParseDataChunk( IterateRIFF &walk );

protected:								   

	// Whoeover derives must implement this.
	char			*GetDataPointer( void );

	WaveCacheHandle_t		m_hCache;
	StreamHandle_t			m_hStream;

private:
	CAudioSourceMemWave( const CAudioSourceMemWave & ); // not implemented, not accessible
};



//-----------------------------------------------------------------------------
// Purpose: Wave source for streaming wave files
// UNDONE: Handle looping
//-----------------------------------------------------------------------------
class CAudioSourceStreamWave : public CAudioSourceWave
{
public:
	CAudioSourceStreamWave( CSfxTable *pSfx );
	CAudioSourceStreamWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceStreamWave();

	CAudioMixer		*CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError );
	int				GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	void			ParseChunk( IterateRIFF &walk, int chunkName );
	bool			IsStreaming( void ) { return true; }

	int		GetCacheStatus( void );

	// the streaming data source (see CAudioSource::UpdateLoopingSamplePosition)
	int64 UpdateLoopingSamplePosition( int64 samplePosition )
	{
		return ConvertLoopedPosition( samplePosition );
	}
	void UpdateSamples( char *pData, int sampleCount )
	{
		ConvertSamples( pData, sampleCount );
	}
	int GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples )
	{
		return CAudioSourceWave::GetLoopingInfo( pLoopBlock, pNumLeadingSamples, pNumTrailingSamples );
	}

	void Prefetch();

	int	SampleToStreamPosition( int samplePosition );
	int	StreamToSamplePosition( int streamPosition );

private:
	CAudioSourceStreamWave( const CAudioSourceStreamWave & ); // not implemented, not accessible

	// We need this for -tools mode to get access to the raw samples
	FileHandle_t		m_hWaveFileAccess;
};


// ------------------------------------------------------------------------- //
// CAudioSourceVoice.
// This feeds the data from an incoming voice channel (a guy on the server
// who is speaking) into the sound engine.
// ------------------------------------------------------------------------- //

class CAudioSourceVoice : public CAudioSourceWave
{
public:
								CAudioSourceVoice(CSfxTable *pSfx, int iEntity);
								~CAudioSourceVoice();
	
	int					GetType( void )
	{
		return AUDIO_SOURCE_VOICE;
	}
	void				GetCacheData( CAudioSourceCachedInfo *info )
	{
		contract_assert( 0 );
	}


	CAudioMixer			*CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError );
	int					GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	int					SampleRate( void );
	
	// Sample size is in bytes.  It will not be accurate for compressed audio.  This is a best estimate.
	// The compressed audio mixers understand this, but in general do not assume that SampleSize() * SampleCount() = filesize
	// or even that SampleSize() is 100% accurate due to compression.
	int					SampleSize( void );

	// Total number of samples in this source.  NOTE: Some sources are infinite (mic input), they should return
	// a count equal to one second of audio at their current rate.
	int					SampleCount( void );

	bool				IsVoiceSource()				{return true;}
	bool				IsPlayerVoice()				{return true;}

	bool				IsLooped()					{return false;}
	bool				IsStreaming()				{return true;}
	bool				IsStereoWav()				{return false;}
	int					GetCacheStatus()			{return AUDIO_IS_LOADED;}
	void				CacheLoad()					{}
	void				CacheUnload()				{}
	CSentence			*GetSentence()				{return nullptr;}
	int					GetQuality()				{ return 0; }

	int					ZeroCrossingBefore( int sample )	{return sample;}
	int					ZeroCrossingAfter( int sample )		{return sample;}
	
	// mixer's references
	void				ReferenceAdd( CAudioMixer *pMixer );
	void				ReferenceRemove( CAudioMixer *pMixer );

	// check reference count, return true if nothing is referencing this
	bool				CanDelete();

	void				Prefetch() {}

	// Nothing, not a cache object...
	void				CheckAudioSourceCache() {} 

private:
	CAudioSourceVoice( const CAudioSourceVoice & );

	// Which entity's voice this is for.
	int							m_iChannel;
	
	// How many mixers are referencing us.
	int							m_refCount;
};




PathTypeFilter_t GetAudioPathFilter();
