// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.dbg;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
#include "engine/IEngineSound.h"
import std;
#include "quakedef.h"
#include "vox.h"
#include "server.h"
#include "sv_main.h"
#include "edict.h"
#include "sound.h"
#include "host.h"
#include "vengineserver_impl.h"
#include "enginesingleuserfilter.h"
#include "snd_audio_source.h"
#include "soundchars.h"
#ifndef DEDICATED
#include "engineui.h"
#endif
#include "SoundEmitterSystem/isoundemittersystembase.h"

#include "enginesoundimpl.h"


//-----------------------------------------------------------------------------
// Client-server neutral sound interface accessor
//-----------------------------------------------------------------------------
static CEngineSoundServer s_EngineSoundServer;
static InterfaceReg s_Reg_CEngineSoundServer_IEngineSound( []() -> void * { return static_cast<IEngineSound *>( &s_EngineSoundServer ); }, IENGINESOUND_SERVER_INTERFACE_VERSION );;

IEngineSound *EngineSoundServer()
{
	return &s_EngineSoundServer;
}



//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEngineSoundServer::CEngineSoundServer() :
	IEngineSound( kind_t::server )
{
}

CEngineSoundServer::~CEngineSoundServer()
{
}


//-----------------------------------------------------------------------------
// Precache a particular sample
//-----------------------------------------------------------------------------
bool CEngineSoundServer::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
#ifndef DEDICATED
	EngineUI()->UpdateProgressBar( PROGRESS_DEFAULT ); 
#endif
	int		i;

	if ( pSample && TestSoundChar( pSample, CHAR_SENTENCE ) )
	{
		return true;
	}

	if ( pSample[0] <= ' ' )
	{
		Host_Error( "CEngineSoundServer::PrecacheSound:  Bad string: %s", pSample );
	}
	
	// add the sound to the precache list
	// Start at 1, since 0 is used to indicate an error in the sound precache
	i = SV_FindOrAddSound( pSample, bPreload );
	if ( i >= 0 )
		return true;

	Host_Error( "CEngineSoundServer::PrecacheSound: '%s' overflow", pSample );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundServer::IsSoundPrecached( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return true;
	}

	int idx = SV_SoundIndex( pSample );
	if ( idx == -1 )
	{
		return false;
	}

	return true;
}

void CEngineSoundServer::PrefetchSound( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return;
	}

	int idx = SV_SoundIndex( pSample );
	if ( idx == -1 )
	{
		return;
	}

	// Tell clients to prefetch the sound
	CSVCMsg_Prefetch_t msg;
	msg.sound_index = idx;

	sv.BroadcastMessage( msg, true, false );
}

//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundServer::EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*=-1*/ )
{
	contract_assert( ( pDirection == nullptr ) && "Direction specification not currently supported on server sounds" );
	contract_assert( bUpdatePositions && "Non-updated positions not currently supported on server sounds" );

	if (flVolume < 0 || flVolume > 1)
	{
		Warning ("EmitSound: %s volume out of bounds = %f\n", pSample, flVolume);
		return;
	}

	if (iSoundLevel < MIN_SNDLVL_VALUE || iSoundLevel > MAX_SNDLVL_VALUE)
	{
		Warning ("EmitSound: %s soundlevel out of bounds = %d\n", pSample, iSoundLevel);
		return;
	}

	if (iPitch < 0 || iPitch > 255)
	{
		Warning ("EmitSound: %s pitch out of bounds = %i\n", pSample, iPitch);
		return;
	}

	edict_t *pEdict = (iEntIndex >= 0) ? &sv.edicts[iEntIndex] : nullptr; 
	SV_StartSound( filter, pEdict, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, iSoundLevel,
		iFlags, iPitch, pOrigin, soundtime, speakerentity, pUtlVecOrigins, nSeed );
}


//-----------------------------------------------------------------------------
// Plays a sentence
//-----------------------------------------------------------------------------
void CEngineSoundServer::EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, 
	int iSentenceIndex, float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	if ( iSentenceIndex >= 0 )
	{
		char pName[8];
		V_snprintf( pName, sizeof(pName), "!%d", iSentenceIndex );
		EmitSoundInternal( filter, iEntIndex, iChannel, nullptr, SOUNDEMITTER_INVALID_HASH, pName, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}


//-----------------------------------------------------------------------------
// Emits a sound
//-----------------------------------------------------------------------------
int CEngineSoundServer::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	CVProfScope _( "CEngineSoundServer::EmitSound", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	EmitSound( filter, iEntIndex, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), nSeed, iFlags, 
		iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	return -1;	// Negative for unsupported feature.
}

int CEngineSoundServer::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, std::vector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	CVProfScope _( "CEngineSoundServer::EmitSound", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
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
			DevWarning( 2, "Unable to find %s in sentences.txt\n", PSkipSoundChars(pSample) );
		}
	}
	else
	{
		EmitSoundInternal( filter, iEntIndex, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
	return -1;	// Negative for unsupported feature.
}

void BuildRecipientList( std::vector< edict_t * >& list, const IRecipientFilter& filter )
{
	int c = filter.GetRecipientCount();
	for ( int i = 0; i < c; i++ )
	{
		int playerindex = filter.GetRecipientIndex( i );
		if ( playerindex < 1 || playerindex > sv.GetClientCount() )
			continue;

		CGameClient *cl = sv.Client( playerindex - 1 );
		// Never output to bots
		if ( cl->IsFakeClient() )
			continue;

		if ( !cl->IsSpawned() )
			continue;

		list.push_back( cl->edict );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			roomType - 
//-----------------------------------------------------------------------------
void CEngineSoundServer::SetRoomType( IRecipientFilter& filter, int roomType )
{
	std::vector< edict_t * > players;
	BuildRecipientList( players, filter );

	for ( int i = 0 ; i < std::ssize( players ); i++ )
	{
		g_pVEngineServer->ClientCommand( players[ i ], "room_type %i\n", roomType );
	}
}

// Set the dsp preset for a player (client only)
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			dspType - 
//-----------------------------------------------------------------------------
void CEngineSoundServer::SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset )
{
	contract_assert( !fastReset );
	if ( fastReset )
	{
		Warning( "SetPlayerDSP:  fastReset only valid from client\n" );
	}

	std::vector< edict_t * > players;
	BuildRecipientList( players, filter );

	for ( int i = 0 ; i < std::ssize( players ); i++ )
	{
		g_pVEngineServer->ClientCommand( players[ i ], "dsp_player %i\n", dspType );

		KeyValues *kvClientCmd = new KeyValues( "dsp_player" );
		kvClientCmd->SetInt( nullptr, dspType );
		g_pVEngineServer->ClientCommandKeyValues( players[ i ], kvClientCmd );
	}
}

void CEngineSoundServer::StopAllSounds(bool bClearBuffers)
{
	contract_assert( false && "Not supported" );
}
// bool CEngineSoundServer::GetPreventSound( void )
// {
// 	AssertMsg( 0, "Not supported" );
// }

int CEngineSoundServer::EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime /*= 0.0f*/ )
{
	contract_assert( false && "Not supported" );
	return 0;
}

//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundServer::StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash )
{
	CEngineRecipientFilter filter;
	filter.AddAllPlayers();
	filter.MakeReliable();

	int nFlags = SND_STOP | ( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH ? SND_IS_SCRIPTHANDLE : 0 );

	EmitSound( filter, iEntIndex, iChannel, pSample, nSoundEntryHash, pSample, 0, SNDLVL_NONE, 0, nFlags, PITCH_NORM,
				nullptr, nullptr, nullptr, true );
}



float SV_GetSoundDuration( const char *pSample )
{
#ifdef DEDICATED
	return 0; // TODO: make this return a real value (i.e implement an OS independent version of the sound code)
#else
	return AudioSource_GetSoundDuration( pSample );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : float
//-----------------------------------------------------------------------------
float CEngineSoundServer::GetSoundDuration( const char *pSample )
{
	return Host_GetSoundDuration( pSample );
}

float CEngineSoundServer::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	return S_GetGainFromSoundLevel( soundlevel, dist );
}

/*
//-----------------------------------------------------------------------------
// FIXME: Move into the CEngineSoundServer class?
//-----------------------------------------------------------------------------
void Host_RestartAmbientSounds()
{
	if (!sv.active)
	{
		return;
	}
	
	
#ifndef DEDICATED
	const int 	NUM_INFOS = 64;
	SoundInfo_t soundInfo[NUM_INFOS];

	int nSounds = S_GetCurrentStaticSounds( soundInfo, NUM_INFOS, CHAN_STATIC );
	
	for ( int i = 0; i < nSounds; i++)
	{
		if (soundInfo[i].looping && 
			soundInfo[i].entity != -1 )
		{
			Msg("Restarting sound %s...\n", soundInfo[i].name);
			S_StopSound(soundInfo[i].entity, soundInfo[i].channel);
			CEngineRecipientFilter filter;
			filter.AddAllPlayers();

			SV_StartSound( filter, EDICT_NUM(soundInfo[i].entity), 
						   CHAN_STATIC, 
						   soundInfo[i].name, 
						   soundInfo[i].volume,
						   soundInfo[i].soundlevel,
						   0,                    // @Q (toml 05-09-02): Is this correct, or will I need to squirrel away the original flags?
						   soundInfo[i].pitch );
		}
	}
#endif
} */
