#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
//
// The two implementations of IEngineSound (see facade/iEngineSound.inl for the dispatch).

#include "engine/IEngineSound.h"
#include "vox.h"

extern float g_flReplaySoundFade;

//-----------------------------------------------------------------------------
//
// Server-side implementation of the engine sound interface
//
//-----------------------------------------------------------------------------
class CEngineSoundServer : public IEngineSound
{
public:
	// constructor, destructor
	CEngineSoundServer();
	~CEngineSoundServer();

	bool PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound );
	bool IsSoundPrecached( const char *pSample );
	void PrefetchSound( const char *pSample );
	bool IsLoopingSound( const char *pSample )
	{
		Warning( "Can't call IsLoopingSound from server\n" );
		return false;
	}

	float GetSoundDuration( const char *pSample );  

	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	void StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash = SOUNDEMITTER_INVALID_HASH );

	void StopAllSounds( bool bClearBuffers );

	// Set the room type for a player
	void SetRoomType( IRecipientFilter& filter, int roomType );
	void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );

	// emit an "ambient" sound that isn't spatialized - specify left/right volume
	// only available on the client, assert on server
	int EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime = 0.0f );

	float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	int		GetGuidForLastSoundEmitted()
	{ 
		Warning( "Can't call GetGuidForLastSoundEmitted from server\n" );
		return 0; 
	}
	bool	IsSoundStillPlaying( int guid )
	{ 
		Warning( "Can't call IsSoundStillPlaying from server\n" );
		return false; 
	}
	void	StopSoundByGuid( int guid, bool bForce )	
	{ 
		Warning( "Can't call StopSoundByGuid from server\n" );
		return; 
	}

	// Retrieves list of all active sounds
	void	GetActiveSounds( std::vector< SndInfo_t >& sndlist )
	{ 
		Warning( "Can't call GetActiveSounds from server\n" );
		return; 
	}

	// Set's master volume (0.0->1.0)
	void	SetVolumeByGuid( int guid, float fvol )
	{
		Warning( "Can't call SetVolumeByGuid from server\n" );
		return; 
	}
	// return's sound's current elapsed time
	float GetElapsedTimeByGuid( int guid)
	{
		Warning( "Can't call GetElapsedTimeByGuid from server\n" );
		return 0.0; 
	}
	void	PrecacheSentenceGroup( const char *pGroupName )
	{
		VOX_PrecacheSentenceGroup( this, pGroupName );
	}
	void	NotifyBeginMoviePlayback()
	{
		contract_assert( false && "Not supported" );
	}

	void	NotifyEndMoviePlayback()
	{
		contract_assert( false && "Not supported" );
	}

	bool	IsMoviePlaying()
	{
		contract_assert( false && "Not supported" );
		return false;
	}

	bool	GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight )
	{
		Warning( "Can't call GetSoundChannelVolume from server\n" );
		return false;
	}

	bool GetPreventSound( )
	{
		contract_assert( false && "Not supported" );
		return false;
	}


	void SetReplaySoundFade( float flReplayVolume ) { contract_assert( !"Client-only service" ); }
	float GetReplaySoundFade()const { contract_assert( !"Client-only service" );  return 0.0f; }

	void SetReplayMusicGain( float flReplayMusicGain ) { contract_assert( !"Client-only service" ); }
	float GetReplayMusicGain()const { contract_assert( !"Client-only service" );  return 0.0f; }


public:
	void EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );
};

//-----------------------------------------------------------------------------
//
// Client-side implementation of the engine sound interface
//
//-----------------------------------------------------------------------------
class CEngineSoundClient : public IEngineSound
{
public:
	// constructor, destructor
	CEngineSoundClient();
	~CEngineSoundClient();

	bool PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound );
	bool IsSoundPrecached( const char *pSample );
	void PrefetchSound( const char *pSample );
	bool IsLoopingSound( const char *pSample );

	float GetSoundDuration( const char *pSample );  

	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
		float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	void StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash = SOUNDEMITTER_INVALID_HASH );

	void StopAllSounds(bool bClearBuffers);

	void SetRoomType( IRecipientFilter& filter, int roomType );
	void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );

	int EmitAmbientSound( const char *pSample, float flVolume, 
		int iPitch, int flags, float soundtime = 0.0f );

	float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	int		GetGuidForLastSoundEmitted();
	bool	IsSoundStillPlaying( int guid );
	bool	GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight );
	void	StopSoundByGuid( int guid, bool bForceSync );
	// Set's master volume (0.0->1.0)
	void	SetVolumeByGuid( int guid, float fvol );
	float   GetElapsedTimeByGuid( int guid );

	// Retrieves list of all active sounds
	void	GetActiveSounds( std::vector< SndInfo_t >& sndlist );

	void	PrecacheSentenceGroup( const char *pGroupName );
	void	NotifyBeginMoviePlayback();
	void	NotifyEndMoviePlayback();
	bool	IsMoviePlaying();
	bool    GetPreventSound( void );

	void SetReplaySoundFade( float flReplayVolume ) { g_flReplaySoundFade = flReplayVolume; }
	float GetReplaySoundFade()const { return g_flReplaySoundFade; }

public:
	int EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	bool m_bMoviePlaying;
};

