// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "engine/IEngineSound.h"
import std;
#include "sound.h"
#include "client.h"
#include "vox.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "enginesingleuserfilter.h"
#include "snd_audio_source.h"
#include "audio/private/snd_sfx.h"
#include "cl_demo.h"

// HACK:  expose in sound.h maybe?
void DSP_FastReset(int dsp);

extern float g_flReplaySoundFade;

#include "enginesoundimpl.h"


//-----------------------------------------------------------------------------
// Client-server neutral sound interface accessor
//-----------------------------------------------------------------------------
static CEngineSoundClient s_EngineSoundClient;
static InterfaceReg s_Reg_CEngineSoundClient_IEngineSound( []() -> void * { return static_cast<IEngineSound *>( &s_EngineSoundClient ); }, IENGINESOUND_CLIENT_INTERFACE_VERSION );;

IEngineSound *EngineSoundClient()
{
	return &s_EngineSoundClient;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEngineSoundClient::CEngineSoundClient() :
	IEngineSound( kind_t::client ),
	m_bMoviePlaying( false )
{
}

CEngineSoundClient::~CEngineSoundClient()
{
}


//-----------------------------------------------------------------------------
// Precache a particular sample
//-----------------------------------------------------------------------------
bool CEngineSoundClient::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
	CSfxTable *pTable = S_PrecacheSound( pSample );
	if ( pTable )
	{
		if ( bIsUISound )
		{
			S_MarkUISound( pTable );
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::PrefetchSound( const char *pSample )
{
	S_PrefetchSound( pSample, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundClient::IsSoundPrecached( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return true;
	}

	int idx = GetBaseLocalClient().LookupSoundIndex( pSample );
	if ( idx == -1 )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Returns if the sound is looping
//-----------------------------------------------------------------------------
bool CEngineSoundClient::IsLoopingSound( const char *pSample )
{
	CSfxTable *pTable = S_PrecacheSound( pSample );
	if ( !pTable || !pTable->pSource )
		return false;

	return pTable->pSource->IsLooped();
}

extern IBaseClientDLL *g_ClientDLL;

//-----------------------------------------------------------------------------
// Actually does the work of emitting a sound
//-----------------------------------------------------------------------------
int CEngineSoundClient::EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	;

	if (flVolume < 0 || flVolume > 1)
	{
		Warning ("EmitSound: %s volume out of bounds = %f\n", pSample, flVolume);
		return 0;
	}

	if (iSoundLevel < MIN_SNDLVL_VALUE || iSoundLevel > MAX_SNDLVL_VALUE)
	{
		Warning ("EmitSound: %s soundlevel out of bounds = %d\n", pSample, iSoundLevel);
		return 0;
	}

	if (iPitch < 0 || iPitch > 255)
	{
		Warning ("EmitSound: %s pitch out of bounds = %i\n", pSample, iPitch);
		return 0;
	}

	bool bInEyeSound = false;

	if (iEntIndex < 0)
	{
		bInEyeSound = true;
		if (g_ClientDLL)
			iEntIndex = g_ClientDLL->GetInEyeEntity();

		if (iEntIndex < 0)
			iEntIndex = GetLocalClient().GetViewEntity();
	}

	// See if local player is a recipient
	int i = 0;
	int c = filter.GetRecipientCount();
	for ( ; i < c ; i++ )
	{
		int index = filter.GetRecipientIndex( i );
		if ( index == GetLocalClient().m_nPlayerSlot + 1 )
			break;
	}

	// Local player not receiving sound
	if ( i >= c )
		return 0;


	// Point at origin if they didn't specify a sound source.
	Vector vecDummyOrigin;
	if (!pOrigin)
	{
		// Try to use the origin of the entity
		IClientEntity *pEnt = entitylist->GetClientEntity( iEntIndex );
		// don't update position if we stop this sound
		if (pEnt && !(iFlags & SND_STOP) )
		{
			vecDummyOrigin = pEnt->GetRenderOrigin();
		}
		else
		{
			vecDummyOrigin.Init();
		}

		pOrigin = &vecDummyOrigin;
	}

	Vector vecDirection;
	if (!pDirection)
	{
		IClientEntity *pEnt = entitylist->GetClientEntity( iEntIndex );
		if (pEnt && !(iFlags & SND_STOP))
		{
			QAngle angles;
			angles = pEnt->GetAbsAngles();
			AngleVectors( angles, &vecDirection );
		}
		else
		{
			vecDirection.Init();
		}

		pDirection = &vecDirection;
	}

	if ( pUtlVecOrigins )
	{
		(*pUtlVecOrigins).push_back( *pOrigin );
	}

	// L4D
	float delay = soundtime;
	if ( soundtime > 0.0f )
	{
		// this sound was played directly on the client, use its clock sync
		delay = S_ComputeDelayForSoundtime( soundtime, CLOCK_SYNC_CLIENT );
		if ( delay < 0 && delay > -0.100f )
		{
			delay = 0;
		}
	}

	StartSoundParams_t params;
	params.staticsound = iChannel == CHAN_STATIC;
	params.soundsource = iEntIndex;
	params.entchannel = iChannel;
	params.origin = *pOrigin;
	params.direction = *pDirection;
	params.bUpdatePositions = bUpdatePositions;
	params.fvol = flVolume;
	params.soundlevel = iSoundLevel;
	params.flags = iFlags;
	params.pitch = iPitch;
	params.fromserver = false;
	params.delay = delay;
	params.speakerentity = speakerentity;
	params.m_bIsScriptHandle = ( iFlags & SND_IS_SCRIPTHANDLE ) ? true : false ;
	params.m_bInEyeSound = bInEyeSound;
	if ( iFlags & SND_GENERATE_GUID )
	{
		params.m_nQueuedGUID = StartSoundParams_t::GENERATE_GUID;
	}

	// soundentry handling
	if ( iFlags & SND_IS_SCRIPTHANDLE )
	{
		// Don't actually play sounds if playing a demo and skipping ahead
		// but always stop sounds
		if ( demoplayer->IsSkipping() && !(iFlags&SND_STOP) )
		{
			return 0;
		}
		params.m_nSoundScriptHash = nSoundEntryHash;
		return S_StartSoundEntry( params, nSeed, false );
	}

	CSfxTable *pSound = S_PrecacheSound(pSample);
	if (!pSound)
		return 0;

	params.pSfx = pSound;

	// Don't actually play sounds if playing a demo and skipping ahead
	// but always stop sounds
	if ( demoplayer->IsSkipping() && !(iFlags&SND_STOP) )
	{
		return 0;
	}
	return S_StartSound( params );
}


//-----------------------------------------------------------------------------
// Plays a sentence
//-----------------------------------------------------------------------------
void CEngineSoundClient::EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, 
	int iSentenceIndex, float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePosition, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	if ( iSentenceIndex >= 0 )
	{
		char pName[8];
		V_snprintf( pName, sizeof(pName), "!%d", iSentenceIndex );
		EmitSoundInternal( filter, iEntIndex, iChannel, nullptr, SOUNDEMITTER_INVALID_HASH, pName, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePosition, soundtime, speakerentity );
	}
}


//-----------------------------------------------------------------------------
// Emits a sound
//-----------------------------------------------------------------------------
int CEngineSoundClient::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
	float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	CVProfScope _( "CEngineSoundClient::EmitSound", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	return EmitSound( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), nSeed, iFlags, 
		iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );

}


int CEngineSoundClient::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH nSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	CVProfScope _( "CEngineSoundClient::EmitSound", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		int iSentenceIndex = -1;
		VOX_LookupString( PSkipSoundChars(pSample), &iSentenceIndex );
		if (iSentenceIndex >= 0)
		{
			EmitSentenceByIndex( filter, iEntIndex, iChannel, iSentenceIndex, flVolume,
				iSoundLevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
		}
		else
		{
			DevWarning( 2, "Unable to find %s in sentences.txt\n", PSkipSoundChars(pSample));
		}
		return -1;
	}
	else
	{
		return EmitSoundInternal( filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}

//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundClient::StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash )
{
	;

	CEngineSingleUserFilter filter( GetLocalClient().m_nPlayerSlot + 1 );
	EmitSound( filter, iEntIndex, iChannel, pSample, nSoundEntryHash, pSample, 0, SNDLVL_NONE, 0, SND_STOP, PITCH_NORM,
		nullptr, nullptr, nullptr, true );
}


void CEngineSoundClient::SetRoomType( IRecipientFilter& filter, int roomType )
{
}

void CEngineSoundClient::SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset )
{
	extern void dsp_player_set( int val );
	dsp_player_set( dspType );
	if ( fastReset )
	{
		DSP_FastReset( dspType );
	}
}


int CEngineSoundClient::EmitAmbientSound( const char *pSample, float flVolume, 
										  int iPitch, int flags, float soundtime /*= 0.0f*/ )
{
	float delay = 0.0f;
	if ( soundtime != 0.0f )
	{
		delay = soundtime - GetBaseLocalClient().m_flLastServerTickTime;
	}

	CSfxTable *pSound = S_PrecacheSound(pSample);

	StartSoundParams_t params;
	params.staticsound = true;
	params.soundsource = SOUND_FROM_LOCAL_PLAYER;
	params.entchannel = CHAN_STATIC;
	params.pSfx = pSound;
	params.origin = vec3_origin;
	params.fvol = flVolume;
	params.soundlevel = SNDLVL_NONE;
	params.flags = flags;
	params.pitch = iPitch;
	params.fromserver = false;
	params.delay = delay;

	return S_StartSound( params );
}

void CEngineSoundClient::StopAllSounds(bool bClearBuffers)
{
	S_StopAllSounds( bClearBuffers );
}

bool CEngineSoundClient::GetPreventSound( void )
{
	return S_GetPreventSound( );
}

float CEngineSoundClient::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	return S_GetGainFromSoundLevel( soundlevel, dist );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : float
//-----------------------------------------------------------------------------
float CEngineSoundClient::GetSoundDuration( const char *pSample )
{
	return AudioSource_GetSoundDuration( pSample );
}

// Client .dll only functions
//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int	
//-----------------------------------------------------------------------------
int CEngineSoundClient::GetGuidForLastSoundEmitted()
{
	return S_GetGuidForLastSoundEmitted();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundClient::IsSoundStillPlaying( int guid )
{
	return S_IsSoundStillPlaying( guid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::StopSoundByGuid( int guid, bool bForceSync )
{
	S_StopSoundByGuid( guid, bForceSync );
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves list of all active sounds
// Input  : sndlist - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::GetActiveSounds( std::vector< SndInfo_t >& sndlist )
{
	S_GetActiveSounds( sndlist );
}

//-----------------------------------------------------------------------------
// Purpose: Set's master volume (0.0->1.0)
// Input  : guid - 
//			fvol - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::SetVolumeByGuid( int guid, float fvol )
{
	S_SetVolumeByGuid( guid, fvol );
}

//-----------------------------------------------------------------------------
// Purpose: Returns sound's current elapsed time
// Input  : guid - 
//-----------------------------------------------------------------------------
float CEngineSoundClient::GetElapsedTimeByGuid( int guid )
{
	return S_GetElapsedTimeByGuid( guid );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEngineSoundClient::PrecacheSentenceGroup( const char *pGroupName )
{
	VOX_PrecacheSentenceGroup( this, pGroupName );
}

void CEngineSoundClient::NotifyBeginMoviePlayback()
{
	StopAllSounds(true);

	m_bMoviePlaying = true;
}

void CEngineSoundClient::NotifyEndMoviePlayback()
{

	m_bMoviePlaying = false;
}

bool CEngineSoundClient::IsMoviePlaying()
{
	return m_bMoviePlaying;
}

bool CEngineSoundClient::GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight )
{
	return S_GetSoundChannelVolume( sound, flVolumeLeft, flVolumeRight );
}
