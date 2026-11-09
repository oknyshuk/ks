#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#define MP3_SUPPORT	1

inline constexpr auto AUDIOSOURCE_COPYBUF_SIZE = 4096;

struct channel_t;
class CSentence;
class CSfxTable;

class CAudioSource;
class IAudioDevice;

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Purpose: This is an instance of an audio source.
//			Mixers are attached to channels and reference an audio source.
//			Mixers are specific to the sample format and source format.
//			Mixers are never re-used, so they can track instance data like
//			sample position, fractional sample, stream cache, faders, etc.
//-----------------------------------------------------------------------------
enum class AudioMixerKind : unsigned char
{
	WavePCM,		// 8/16 bit mono/stereo PCM (snd_wave_mixer.cpp)
	WaveADPCM,		// snd_wave_mixer_adpcm.cpp
	Sentence,		// snd_sentence_mixer.cpp
};

class CAudioMixer
{
public:
	// The mixers are a closed set (AudioMixerKind): every call below switches on the kind of the
	// mixer and calls the member of the same name of its class (snd_wave_mixer.cpp).

	// Destroys a mixer of any kind
	static void Delete( CAudioMixer *pMixer );

	// return number of samples mixed
	int MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	int SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	bool ShouldContinueMixing( void );

	CAudioSource *GetSource( void );
	
	// get the current position (next sample to be mixed)
	int GetSamplePosition( void );

	// Allow the mixer to modulate pitch and volume. 
	// returns a floating point modulator
	float ModifyPitch( float pitch );
	float GetVolumeScale( void );

	// NOTE: Playback is optimized for linear streaming.  These calls will usually cost performance
	// It is currently optimal to call them before any playback starts, but some audio sources may not
	// guarantee this.  Also, some mixers may choose to ignore these calls for internal reasons (none do currently).

	bool IsSetSampleStartSupported() const;

	// Move the current position to newPosition 
	// BUGBUG: THIS CALL DOES NOT SUPPORT MOVING BACKWARD, ONLY FORWARD!!!
	void SetSampleStart( int newPosition );

	// End playback at newEndPosition
	void SetSampleEnd( int newEndPosition );

	// How many samples to skip before commencing actual data reading ( to allow sub-frametime sound
	//  offsets and avoid synchronizing sounds to various 100 msec clock intervals throughout the
	//  engine and game code)
	void SetStartupDelaySamples( int delaySamples );
	int GetMixSampleSize();

	// Certain async loaded sounds lazilly load into memory in the background, use this to determine
	//  if the sound is ready for mixing
	bool IsReadyToMix();
	
	// NOTE: The "saved" position can be different than the "sample" position
	// NOTE: Allows mixer to save file offsets, loop info, etc
	int GetPositionForSave();
	void SetPositionFromSaved( int savedPosition );

protected:
	explicit CAudioMixer( AudioMixerKind kind ) : m_Kind( kind ) {}
	~CAudioMixer( void ) {}

	AudioMixerKind m_Kind;
};

inline int CalcSampleSize( int bitsPerSample, int channels ) 
{
	return (bitsPerSample >> 3) * channels;
}

#include "cached_file_data.h"

class CSentence;
class CSfxTable;
class CAudioSourceCachedInfo : public IBaseCacheInfo
{
public:
	CAudioSourceCachedInfo();
	CAudioSourceCachedInfo( const CAudioSourceCachedInfo& src );

	virtual ~CAudioSourceCachedInfo();

	CAudioSourceCachedInfo& operator =( const CAudioSourceCachedInfo& src );

	void	Clear();
	void	RemoveData();

	virtual void Save( ks::byte_writer& buf );
	virtual void Restore( ks::byte_reader& buf );
	virtual void Rebuild( char const *filename );

	// A hack, but will work okay
	static int s_CurrentType;
	static CSfxTable *s_pSfx;
	static bool s_bIsPrecacheSound;

	inline int		Type() const
	{
		return info.m_Type;
	}
	void	SetType( int type )
	{
		info.m_Type = type;
	}

	inline int		Bits() const
	{
		return info.m_bits;
	}
	void	SetBits( int bits )
	{
		info.m_bits = bits;
	}

	inline int		Channels() const
	{
		return info.m_channels;
	}
	void	SetChannels( int channels )
	{
		info.m_channels = channels;
	}

	inline int		SampleSize() const
	{
		return info.m_sampleSize;
	}
	void	SetSampleSize( int size )
	{
		info.m_sampleSize = size;
	}

	inline int		Format() const
	{
		return info.m_format;
	}
	void	SetFormat( int format )
	{
		info.m_format = format;
	}

	inline int		SampleRate() const
	{
		return info.m_rate;
	}
	void	SetSampleRate( int rate )
	{
		info.m_rate = rate;
	}

	inline int		CachedDataSize() const
	{
		return (int)m_usCachedDataSize;
	}

	void	SetCachedDataSize( int size )
	{
		m_usCachedDataSize = (unsigned short)size;
	}

	inline const byte	*CachedData() const
	{
		return m_pCachedData;
	}

	void	SetCachedData( const byte *data )
	{
		m_pCachedData = ( byte * )data;
		flags.m_bCachedData = ( data != nullptr ) ? true : false;
	}

	inline int		HeaderSize() const
	{
		return (int)m_usHeaderSize;
	}

	void	SetHeaderSize( int size )
	{
		m_usHeaderSize = (unsigned short)size;
	}

	inline const byte	*HeaderData() const
	{
		return m_pHeader;
	}

	void	SetHeaderData( const byte *data )
	{
		m_pHeader = ( byte * )data;
		flags.m_bHeader = ( data != nullptr ) ? true : false;
	}

	inline int		LoopStart() const
	{
		return m_loopStart;
	}
	void	SetLoopStart( int start )
	{
		m_loopStart = start;
	}

	inline int		SampleCount() const
	{
		return m_sampleCount;
	}

	void	SetSampleCount( int count )
	{
		m_sampleCount = count;
	}
	inline int		DataStart() const
	{
		return m_dataStart;
	}
	void	SetDataStart( int start )
	{
		m_dataStart = start;
	}
	inline int		DataSize() const
	{
		return m_dataSize;
	}
	void	SetDataSize( int size )
	{
		m_dataSize = size;
	}
	inline CSentence	*Sentence() const
	{
		return m_pSentence;
	}
	void	SetSentence( CSentence *sentence )
	{
		m_pSentence = sentence;
		flags.m_bSentence = ( sentence != nullptr ) ? true : false;
	}

private:

	union
	{
		unsigned int infolong;
		struct
		{
			unsigned int				m_Type : 2;  // 0 1 2 or 3
			unsigned int				m_bits : 5;  // 0 to 31
			unsigned int				m_channels : 2; // 1 or 2
			unsigned int				m_sampleSize : 3; // 1 2 or 4
			unsigned int				m_format : 2; // 1 == PCM, 2 == ADPCM
			unsigned int				m_rate : 17; // 0 to 64 K
		} info;
	};

	union
	{
		byte	flagsbyte;
		struct
		{
			bool			m_bSentence : 1;
			bool			m_bCachedData : 1;
			bool			m_bHeader : 1;
		} flags;
	};

	int				m_loopStart;
	int				m_sampleCount;
	int				m_dataStart;	// offset of wave data chunk
	int				m_dataSize;		// size of wave data chunk

	unsigned short	m_usCachedDataSize;
	unsigned short	m_usHeaderSize;

	CSentence		*m_pSentence;
	byte			*m_pCachedData;
	byte			*m_pHeader;
};

class CAudioSourceCache
{
public:
	typedef CCachedFileData< CAudioSourceCachedInfo > CacheType_t;

	CAudioSourceCache()
	{
		m_pMasterSoundCache = nullptr;
		m_pBuildingCache = nullptr;
		m_nServerCount = -1;
	}

	bool Init( unsigned int memSize );
	void Shutdown();

	void LevelInit( char const *mapname );
	void LevelShutdown();

	CAudioSourceCachedInfo *GetInfo( int audiosourcetype, bool soundisprecached, CSfxTable *sfx );
	CAudioSourceCachedInfo *GetInfoByName( const char *soundName );
	void RebuildCacheEntry( int audiosourcetype, bool soundisprecached, CSfxTable *sfx );

	bool BuildMasterPrecachedSoundsCache();
	bool UpdateMasterPrecachedSoundsCache();
	void WriteManifest();

	void ValidateSoundCache( char const *pchWavFile );
private:
	// Purpose: 
	CacheType_t *LookUpCacheEntry( const char *fn, int audiosourcetype, bool soundisprecached, CSfxTable *sfx );

	struct AudioSourceUsage_t
	{
		AudioSourceUsage_t() :
			handle( 0 ),
			count( 0u )
		{
		}
		FileNameHandle_t	handle;
		unsigned int		count;
	};

	static bool AudioSourceUsageLessFunc( const AudioSourceUsage_t& lhs, const AudioSourceUsage_t& rhs )
	{
		return lhs.handle < rhs.handle;
	}

	CacheType_t *AllocAudioCache( char const *cachename, bool bNeverCheckDisk );
	bool LoadMasterCache( char const *pchLanguage, bool bAllowEmpty );

	void RecursiveBuildSoundList( std::set< FileNameHandle_t >& other, const char *pCurrentLanguage, const char *pCurrentDir, const char *pPathID );
	CacheType_t *BuildCacheFromList( char const *cachename, std::set< FileNameHandle_t >& list, bool showprogress = false, float flProgressStart = 0.0f, float flProgressEnd = 1.0f );
	bool IsValidCache( char const *cachename );
	void RemoveCache( char const *cachename );

	void GetAudioCacheLanguageSuffix( std::string &sLanguage );
	void PrefetchCommandSounds();

	enum
	{
		MAX_LIST_SIZE = 1024
	};

	std::string				m_szMODPath;
	std::string				m_szMapCacheBase;
	std::string				m_szMasterCache;
	std::string				m_szCurrentLanguage;

	typedef enum
	{
		CACHE_MASTER,
		CACHE_BUILDING
	} SoundCacheType_t;

	void					SetCachePointer( SoundCacheType_t ptrType, CacheType_t *ptr );

	// All sounds (no startup data) referenced anywhere in game
	CacheType_t				*m_pMasterSoundCache;
	CacheType_t				*m_pBuildingCache;

	int						m_nServerCount;
};


extern CAudioSourceCache *audiosourcecache;

typedef ks::handle_t WaveCacheHandle_t;
enum
{
	// purposely 0
	INVALID_WAVECACHE_HANDLE = (ks::handle_t)0 
};

typedef int StreamHandle_t;
enum
{
	INVALID_STREAM_HANDLE = (StreamHandle_t)~0 
};

typedef unsigned int streamFlags_t;
enum
{
	STREAMED_FROMDVD    = 0x00000001,		// stream buffers are compliant to dvd sectors
	STREAMED_SINGLEPLAY = 0x00000002,		// non recurring data, buffers don't need to persist and can be recycled
	STREAMED_QUEUEDLOAD = 0x00000004,		// hint the streamer to load using the queued loader system
	STREAMED_TRANSIENT  = 0x00000008,		// hint the streamer to pool memory buffers according to dynamic nature
};

enum SoundError
{
	SE_NO_STREAM_BUFFER	=	-1000,
	SE_FILE_NOT_FOUND,
	SE_CANT_GET_NAME,
	SE_SKIPPED,
	SE_NO_SOURCE_SETUP,
	SE_CANT_CREATE_MIXER,

	// Avoid 0 on purpose, to avoid ambiguity on the meaning

	SE_OK	=	1,
};

constexpr auto STREAM_BUFFER_COUNT = 4;

struct asyncwaveparams_t;
class CAsyncWaveData;

//-----------------------------------------------------------------------------
// Purpose: Implements a cache of .wav / .mp3 data based on filename
//-----------------------------------------------------------------------------
class CAsyncWavDataCache
{
public:
	CAsyncWavDataCache();

	bool			Init( unsigned int memSize );
	void			Shutdown();

	// implementation that treats file as monolithic
	WaveCacheHandle_t	AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch = false );
	void			PrefetchCache( char const *filename, int datasize, int startpos );
	bool			CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	bool			CopyDataIntoMemory( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	void			SetPostProcessed( WaveCacheHandle_t handle, bool proc );
	void			Unload( WaveCacheHandle_t handle );
	bool			GetDataPointer( WaveCacheHandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed );
	bool			IsDataLoadCompleted( WaveCacheHandle_t handle, bool *pIsValid, bool *pIsMissing = nullptr );
	void			RestartDataLoad( WaveCacheHandle_t* handle, char const *filename, int datasize, int startpos );
	bool			IsDataLoadInProgress( WaveCacheHandle_t handle );

	// Xbox: alternate multi-buffer streaming implementation
	StreamHandle_t	OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags, SoundError &soundError );
	void			CloseStreamedLoad( StreamHandle_t hStream );
	int				CopyStreamedDataIntoMemory( StreamHandle_t hStream, void *pBuffer, int bufferSize, int copyStartPos, int bytesToCopy );
	bool			IsStreamedDataReady( StreamHandle_t hStream );
	void			*GetStreamedDataPointer( StreamHandle_t hStream, bool bSync );

	void			Flush( bool bTearDownStaticPool = false );

	void			UpdateLoopPosition( StreamHandle_t hStream, int nLoopPosition );

	enum MemoryUsageType
	{
		SPEW_BASIC = 0,
		SPEW_MUSIC_NONSTREAMING,
		SPEW_NONSTREAMING,
		SPEW_ALL,
	};
	void					SpewMemoryUsage( MemoryUsageType level );

	// Cache helpers
	bool					GetItemName( uintp clientId, const void *pItem, char *pDest, unsigned nMaxLen  );

private:
	void					Clear();

	// tags the signature of a buffer inside a rb tree for faster than linear find
	struct BufferEntry_t
	{
		FileNameHandle_t	m_hName;
		WaveCacheHandle_t	m_hWaveData;
		int					m_StartPos;
		unsigned int		m_bIsTransient : 1;
		unsigned int		m_bCanBeShared : 1;
	};

	struct BufferEntryLess
	{
		bool operator()( const BufferEntry_t& lhs, const BufferEntry_t& rhs ) const
		{
			if ( lhs.m_bIsTransient != rhs.m_bIsTransient )
			{
				return lhs.m_bIsTransient < rhs.m_bIsTransient;
			}

			if ( lhs.m_hName != rhs.m_hName )
			{
				return lhs.m_hName < rhs.m_hName;
			}

			if ( lhs.m_StartPos != rhs.m_StartPos )
			{
				return lhs.m_StartPos < rhs.m_StartPos;
			}

			return lhs.m_bCanBeShared < rhs.m_bCanBeShared;
		}
	};

	// A signature is re-added whenever its buffer was evicted, so it can repeat.
	std::multiset< BufferEntry_t, BufferEntryLess >	m_BufferList;

	// encapsulates (n) buffers for a streamed wave object
	struct StreamedEntry_t
	{
		FileNameHandle_t	m_hName;
		WaveCacheHandle_t	m_hWaveData[STREAM_BUFFER_COUNT];
		int					m_Front;			// buffer index, forever incrementing
		int					m_NextStartPos;		// predicted offset if mixing linearly
		int					m_DataSize;			// length of the data set in bytes
		int					m_DataStart;		// file offset where data set starts
		int					m_LoopStart;		// offset in data set where loop starts
		int					m_BufferSize;		// size of the buffer in bytes
		int					m_numBuffers;		// number of buffers (1 or 2) to march through
		int					m_SectorSize;		// size of sector on stream device
		bool				m_bSinglePlay;		// hint to keep same buffers
		bool				m_bIsTransient;		// hint for buffer lifetime
	};
	ks::slot_pool< StreamedEntry_t, StreamHandle_t >	m_StreamedHandles;

	std::map< FileNameHandle_t, WaveCacheHandle_t >	m_CacheHandles;

	// these are buffers that were still in-flight at time of stream closure
	// we track these until we can remove an outstanding lock
	// some of these buffers are allowed to be reclaimed
	struct DeadBufferEntry_t
	{
		WaveCacheHandle_t	hWaveData;
		bool				bSinglePlay;
	};
	std::vector< DeadBufferEntry_t >	m_DeadBuffers;

	WaveCacheHandle_t		FindOrCreateBuffer( asyncwaveparams_t &params, bool bFind );		
	void					CleanupDeadBuffers( bool bSync );
	bool					m_bInitialized;

	struct  StreamData_t
	{
		int				actualCopied;
		CAsyncWaveData	*pWaveData[STREAM_BUFFER_COUNT];
		int				index;
		bool			bWaiting;
		int				copyStartPos;
	};

	bool InitializeStreamData( const StreamedEntry_t &streamedEntry, StreamData_t & streamData, int copyStartPos );
	void CopyFromCurrentBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData, StreamHandle_t hStream, void *pBuffer, int bufferSize, int bytesToCopy );
	void PrefetchNextBuffers( StreamedEntry_t &streamedEntry, StreamData_t & streamData );
};


extern CAsyncWavDataCache *wavedatacache;

struct CAudioSourceCachedInfoHandle_t
{
	CAudioSourceCachedInfoHandle_t() :
		info( nullptr ),
		m_FlushCount( 0 )
	{
	}

	CAudioSourceCachedInfo	*info;
	unsigned int			m_FlushCount;

	inline CAudioSourceCachedInfo *Get( int audiosourcetype, bool soundisprecached, CSfxTable *sfx, int *pcacheddatasize )
	{
		CVProfScope _( "CAudioSourceCachedInfoHandle_t::Get", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		if ( m_FlushCount != s_nCurrentFlushCount )
		{
			// Reacquire
			info = audiosourcecache->GetInfo( audiosourcetype, soundisprecached, sfx );

			if ( pcacheddatasize && info )
			{
				*pcacheddatasize = info->CachedDataSize();
			}

			// Tag as current
			m_FlushCount = s_nCurrentFlushCount;
		}
		return info;
	}

	inline bool IsValid()
	{
		return !!( m_FlushCount == s_nCurrentFlushCount );
	}

	inline CAudioSourceCachedInfo *FastGet()
	{
		CVProfScope _( "CAudioSourceCachedInfoHandle_t::FastGet", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		if ( m_FlushCount != s_nCurrentFlushCount )
		{
			return nullptr;
		}
		return info;
	}

	static void				InvalidateCache();
	static unsigned int		s_nCurrentFlushCount;
};


//-----------------------------------------------------------------------------
// Purpose: A source is an abstraction for a stream, cached file, or procedural
//			source of audio.
//-----------------------------------------------------------------------------
enum class AudioSourceKind : unsigned char
{
	MemWave,		// a wave file in memory (snd_wave_source.cpp)
	StreamWave,		// a streamed wave file (snd_wave_source.cpp)
	Voice,			// a voice channel (voice_sound_engine_interface.cpp)
};

class CAudioSource
{
public:
	enum
	{
		AUDIO_SOURCE_UNK = 0,
		AUDIO_SOURCE_WAV,
		AUDIO_SOURCE_MP3,
		AUDIO_SOURCE_VOICE,

		AUDIO_SOURCE_MAXTYPE,
	};

	enum
	{
		AUDIO_NOT_LOADED = 0,
		AUDIO_IS_LOADED,
		AUDIO_LOADING,
		AUDIO_ERROR_LOADING,
	};

	// The sources are a closed set (AudioSourceKind): every call below switches on the kind of the
	// source and calls the member of the same name of its class (snd_wave_source.cpp).

	// Destroys a source of any kind
	static void Delete( CAudioSource *pSource );

	// Create an instance (mixer) of this audio source
	CAudioMixer			*CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError );

	// Serialization for caching
	int					GetType( void );
	void				GetCacheData( CAudioSourceCachedInfo *info );

	// Provide samples for the mixer. You can point pData at your own data, or if you prefer to copy the data,
	// you can copy it into copyBuf and set pData to copyBuf.
	int					GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	
	int					SampleRate( void );

	// Returns true if the source is a voice source.
	// This affects the voice_overdrive behavior (all sounds get quieter when
	// someone is speaking).
	bool				IsVoiceSource();

	// Returns true if this sound comes from player voice chat
	bool				IsPlayerVoice();
	
	// Sample size is in bytes.  It will not be accurate for compressed audio.  This is a best estimate.
	// The compressed audio mixers understand this, but in general do not assume that SampleSize() * SampleCount() = filesize
	// or even that SampleSize() is 100% accurate due to compression.
	int					SampleSize( void );

	// Total number of samples in this source.  NOTE: Some sources are infinite (mic input), they should return
	// a count equal to one second of audio at their current rate.
	int					SampleCount( void );

	int					Format( void );
	int					DataSize( void );

	bool				IsLooped( void );
	bool				IsStereoWav( void );
	bool				IsStreaming( void );
	int					GetCacheStatus( void );
	int 				IsCached( void ) { return GetCacheStatus() == AUDIO_IS_LOADED ? true : false; }
	void				CacheLoad( void );
	void				CacheUnload( void );
	CSentence			*GetSentence( void );
	int					GetQuality( void );

	// these are used to find good splice/loop points.
	// If not implementing these, simply return sample
	int					ZeroCrossingBefore( int sample );
	int					ZeroCrossingAfter( int sample );
	
	// mixer's references
	void				ReferenceAdd( CAudioMixer *pMixer );
	void				ReferenceRemove( CAudioMixer *pMixer );

	// check reference count, return true if nothing is referencing this
	bool				CanDelete( void );

	void				Prefetch();

	bool				IsAsyncLoad();

	// Make sure our data is rebuilt into the per-level cache
	void				CheckAudioSourceCache();

	char const			*GetFileName(char *pBuf, std::size_t bufLen);

	void				SetPlayOnce( bool );
	bool				IsPlayOnce();

	// Used to identify a word that is part of a sentence mixing operation
	void				SetSentenceWord( bool bIsWord );
	bool				IsSentenceWord();

	int					SampleToStreamPosition( int samplePosition );
	int					StreamToSamplePosition( int streamPosition );

	// The streamed sources (StreamWave, StreamMP3) are the data source of a streaming CWaveData:
	int64				UpdateLoopingSamplePosition( int64 samplePosition );
	void				UpdateSamples( char *pData, int sampleCount );
	int					GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples );

protected:
	explicit CAudioSource( AudioSourceKind kind ) : m_Kind( kind ) {}
	~CAudioSource( void ) {}

	AudioSourceKind m_Kind;
};

// Fast method for determining duration of .wav/.mp3, exposed to server as well
extern float AudioSource_GetSoundDuration( char const *pName );

// uses wave file cached in memory already
extern float AudioSource_GetSoundDuration( CSfxTable *pSfx );
