#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client-server neutral sound interface

import std;
import ks.tier1;
#include "soundflags.h"
#include "irecipientfilter.h"
#include "engine/SndInfo.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

//-----------------------------------------------------------------------------
// forward declaration
//-----------------------------------------------------------------------------

// Handy defines for EmitSound
inline constexpr auto SOUND_FROM_LOCAL_PLAYER = -1;
inline constexpr auto SOUND_FROM_WORLD = 0;



// These are used to feed a soundlevel to the sound system and have it use
// goldsrc-type attenuation. We should use this as little as possible and 
// phase it out as soon as possible.

// Take a regular sndlevel and convert it to compatibility mode.
template <typename T1> [[gnu::always_inline]] inline auto SNDLEVEL_TO_COMPATIBILITY_MODE( T1 x ) { return ((soundlevel_t)(int)( (x) + 256 )); }

// Take a compatibility-mode sndlevel and get the REAL sndlevel out of it.
template <typename T1> [[gnu::always_inline]] inline auto SNDLEVEL_FROM_COMPATIBILITY_MODE( T1 x ) { return ((soundlevel_t)(int)( (x) - 256 )); }

// Tells if the given sndlevel is marked as compatibility mode.
template <typename T1> [[gnu::always_inline]] inline constexpr auto SNDLEVEL_IS_COMPATIBILITY_MODE( T1 x ) { return ( (x) >= 256 ); }


	
//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
inline constexpr const char *IENGINESOUND_CLIENT_INTERFACE_VERSION = "IEngineSoundClient003";
inline constexpr const char *IENGINESOUND_SERVER_INTERFACE_VERSION = "IEngineSoundServer003";

class IEngineSound
{
public:
	enum class kind_t : unsigned char { server, client };
	explicit IEngineSound( kind_t kind ) : m_kind( kind ) {}

	// Precache a particular sample
	bool PrecacheSound( const char *pSample, bool bPreload = false, bool bIsUISound = false );
	bool IsSoundPrecached( const char *pSample );
	void PrefetchSound( const char *pSample );
	bool IsLoopingSound( const char *pSample );

	// Just loads the file header and checks for duration (not hooked up for .mp3's yet)
	// Is accessible to server and client though
	float GetSoundDuration( const char *pSample );  

	// Pitch of 100 is no pitch shift.  Pitch > 100 up to 255 is a higher pitch, pitch < 100
	// down to 1 is a lower pitch.   150 to 70 is the realistic range.
	// EmitSound with pitch != 100 should be used sparingly, as it's not quite as
	// fast (the pitchshift mixer is not native coded).

	// NOTE: setting iEntIndex to -1 will cause the sound to be emitted from the local
	// player (client-side only)
	// Will return the sound guid. If negative, the guid is unknown (call may be successful or not). 0 if the sound was not emitted. Positive if the guid is valid.
	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
		float flVolume, float flAttenuation, int nSeed, int iFlags = 0, int iPitch = PITCH_NORM, 
		const Vector *pOrigin = nullptr, const Vector *pDirection = nullptr, std::vector< Vector >* pUtlVecOrigins = nullptr, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundlevel, int nSeed, int iFlags = 0, int iPitch = PITCH_NORM, 
		const Vector *pOrigin = nullptr, const Vector *pDirection = nullptr, std::vector< Vector >* pUtlVecOrigins = nullptr, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundlevel, int nSeed, int iFlags = 0, int iPitch = PITCH_NORM,
		const Vector *pOrigin = nullptr, const Vector *pDirection = nullptr, std::vector< Vector >* pUtlVecOrigins = nullptr, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	void StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash = SOUNDEMITTER_INVALID_HASH );

	// stop all active sounds (client only)
	void StopAllSounds(bool bClearBuffers);

	// Set the room type for a player (client only)
	void SetRoomType( IRecipientFilter& filter, int roomType );

	// Set the dsp preset for a player (client only)
	void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );
	
	// emit an "ambient" sound that isn't spatialized
	// only available on the client, assert on server
	int EmitAmbientSound( const char *pSample, float flVolume, int iPitch = PITCH_NORM, int flags = 0, float soundtime = 0.0f );


//	virtual EntChannel_t	CreateEntChannel() = 0;

	float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	int		GetGuidForLastSoundEmitted();
	bool	IsSoundStillPlaying( int guid );
	void	StopSoundByGuid( int guid, bool bForceSync = false );
	// Set's master volume (0.0->1.0)
	void	SetVolumeByGuid( int guid, float fvol );

	// Retrieves list of all active sounds
	void	GetActiveSounds( std::vector< SndInfo_t >& sndlist );

	void	PrecacheSentenceGroup( const char *pGroupName );
	void	NotifyBeginMoviePlayback();
	void	NotifyEndMoviePlayback();
	bool	IsMoviePlaying();

	bool	GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight );
	
	float	GetElapsedTimeByGuid( int guid );

	// engine capable of playing sounds?
	bool GetPreventSound( void );

	void SetReplaySoundFade( float flReplayVolume );
	float GetReplaySoundFade()const;

private:
	kind_t m_kind;
};
