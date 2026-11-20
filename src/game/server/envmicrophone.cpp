// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <math.h>
#include <time.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "igameevents.h"
#include "stringregistry.h"
#include "eventlist.h"
#include "declare_class.h"
#include "collisionutils.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.vprof;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import ks.core.threadtools;
import ks.game.tempentity;
import ks.math.mathlib;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.core.random;
import ks.tier1.iconvar;
import ks.game.triggers_shared;

class CBaseEntity;
class CBaseFilter;
class CSound;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements an entity that measures sound volume at a point in a map.
//			This entity listens as though it is an NPC, meaning it will only
//			hear sounds that were emitted using the CSound::InsertSound function.
//			It does not hear danger sounds since they are not technically sounds.

//#define DEBUG_MICROPHONE

static const float MICROPHONE_SETTLE_EPSILON = 0.005;

// List of env_microphones who want to be told whenever a sound is started
static std::vector< CHandle<CEnvMicrophone> > s_Microphones;

LINK_ENTITY_TO_CLASS(env_microphone, CEnvMicrophone);

IMPLEMENT_REFLECT_DATAMAP( CEnvMicrophone )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvMicrophone::~CEnvMicrophone( void )
{
	ks::erase_first( s_Microphones, this );
}

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CEnvMicrophone::Spawn_Impl(void)
{
	//
	// Build our sound type mask from our spawnflags.
	//
	static int nFlags[][2] =
	{
		{ SF_MICROPHONE_SOUND_COMBAT,			SOUND_COMBAT },
		{ SF_MICROPHONE_SOUND_WORLD,			SOUND_WORLD },
		{ SF_MICROPHONE_SOUND_PLAYER,			SOUND_PLAYER },
		{ SF_MICROPHONE_SOUND_BULLET_IMPACT,	SOUND_BULLET_IMPACT },
		{ SF_MICROPHONE_SOUND_EXPLOSION,		SOUND_CONTEXT_EXPLOSION },
	};

	for (int i = 0; i < sizeof(nFlags) / sizeof(nFlags[0]); i++)
	{
		if (m_spawnflags & nFlags[i][0])
		{
			m_nSoundMask |= nFlags[i][1];
		}
	}

	if (m_flSensitivity == 0)
	{
		//
		// Avoid a divide by zero in CanHearSound.
		//
		m_flSensitivity = 1;
	}
	else if (m_flSensitivity > 10)
	{
		m_flSensitivity = 10;
	}

	m_flSmoothFactor = clamp(m_flSmoothFactor, 0, 0.9);

	if (!m_bDisabled)
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned and after a load game.
//			Finds the reference point at which to measure sound level.
//-----------------------------------------------------------------------------
void CEnvMicrophone::Activate_Impl(void)
{
	BaseClass::Activate_Impl();

	// Get a handle to my filter entity if there is one
	if (m_iszListenFilter != NULL_STRING)
	{
		m_hListenFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( nullptr, m_iszListenFilter ));
	}

	if (m_target != NULL_STRING)
	{
		m_hMeasureTarget = gEntList.FindEntityByName(nullptr, STRING(m_target) );

		//
		// If we were given a bad measure target, just measure sound where we are.
		//
		if ((m_hMeasureTarget == nullptr) || (m_hMeasureTarget->edict() == nullptr))
		{
			// We've decided to disable this warning since this seems to be the 90% case.
			//Warning( "EnvMicrophone - Measure target not found or measure target with no origin. Using Self.!\n");
			m_hMeasureTarget = this;
		}
	}
	else
	{
		m_hMeasureTarget = this;
	}

	ActivateSpeaker();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvMicrophone::OnRestore_Impl( void )
{
	BaseClass::OnRestore_Impl();

	ActivateSpeaker();
}

//-----------------------------------------------------------------------------
// Purpose: If we've got a speaker, add ourselves to the list of microphones that want to listen
//-----------------------------------------------------------------------------
void CEnvMicrophone::ActivateSpeaker( void )
{
	// If we're enabled, set the dsp_speaker preset to my specified one
	if ( !m_bDisabled )
	{
		ConVarRef dsp_speaker( "dsp_speaker" );
		if ( dsp_speaker.IsValid() )
		{
			int iDSPPreset = m_iSpeakerDSPPreset;
			if ( !iDSPPreset )
			{
				// Reset it to the default
				iDSPPreset = std::atoi( dsp_speaker.GetDefault() );
			}
			DevMsg( 2, "Microphone %s set dsp_speaker to %d.\n", STRING(GetEntityName()), iDSPPreset);
			dsp_speaker.SetValue( m_iSpeakerDSPPreset );
		}
	}

	if ( m_iszSpeakerName != NULL_STRING )
	{
		// We've got a speaker to play heard sounds through. To do this, we need to add ourselves 
		// to the list of microphones who want to be told whenever a sound is played.
		if ( ks::find_index( s_Microphones, this ) == -1 )
		{
			s_Microphones.push_back( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stops the microphone from sampling the sound level and firing the
//			SoundLevel output.
//-----------------------------------------------------------------------------
void CEnvMicrophone::InputEnable( inputdata_t &inputdata )
{
	if (m_bDisabled)
	{
		m_bDisabled = false;
		SetNextThink( gpGlobals->curtime + 0.1f );

		ActivateSpeaker();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resumes sampling the sound level and firing the SoundLevel output.
//-----------------------------------------------------------------------------
void CEnvMicrophone::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	if ( m_hSpeaker )
	{
		CBaseEntity::StopSound( m_hSpeaker->entindex(), CHAN_STATIC, m_szLastSound );
		m_szLastSound[0] = 0;

		// Remove ourselves from the list of active mics
		ks::erase_first( s_Microphones, this );
	}
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvMicrophone::InputSetSpeakerName( inputdata_t &inputdata )
{
	SetSpeakerName( inputdata.value.StringID() );
}

//-----------------------------------------------------------------------------
// Purpose: Checks whether this microphone can hear a given sound, and at what
//			relative volume level.
// Input  : pSound - Sound to test.
//			flVolume - Returns with the relative sound volume from 0 - 1.
// Output : Returns true if the sound could be heard at the sample point, false if not.
//-----------------------------------------------------------------------------
bool CEnvMicrophone::CanHearSound(CSound *pSound, float &flVolume)
{
	flVolume = 0;

	if ( m_bDisabled )
	{
		return false;
	}

	// Cull out sounds except from specific entities
	CBaseFilter *pFilter = m_hListenFilter.Get();
	if ( pFilter )
	{
		CBaseEntity *pSoundOwner = pSound->m_hOwner.Get();
		if ( !pSoundOwner || !pFilter->PassesFilter( this, pSoundOwner ) )
		{
			return false;
		}
	}

	float flDistance = (pSound->GetSoundOrigin() - m_hMeasureTarget->GetAbsOrigin()).Length();

	if (flDistance == 0)
	{
		flVolume = 1.0;
		return true;
	}

	// Over our max range?
	if ( m_flMaxRange && flDistance > m_flMaxRange )
	{
		return false;
	}

	if (flDistance <= pSound->Volume() * m_flSensitivity)
	{
		flVolume = 1 - (flDistance / (pSound->Volume() * m_flSensitivity));
		flVolume = clamp(flVolume, 0, 1);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the microphone can hear the specified sound
//-----------------------------------------------------------------------------
bool CEnvMicrophone::CanHearSound( int entindex, soundlevel_t soundlevel, float &flVolume, const Vector *pOrigin )
{
	if ( m_bDisabled )
	{
		flVolume = 0;
		return false;
	}

	if ( ( m_spawnflags & SF_MICROPHONE_IGNORE_NONATTENUATED ) && soundlevel == SNDLVL_NONE )
	{
		return false;
	}

	// Sound might be coming from an origin or from an entity.
	CBaseEntity *pEntity = nullptr;
	if ( entindex )
	{
		pEntity = CBaseEntity::Instance( INDEXENT(entindex) );
	}
			    
	// Cull out sounds except from specific entities
	CBaseFilter *pFilter = m_hListenFilter.Get();
	if ( pFilter )
	{
		if ( !pEntity || !pFilter->PassesFilter( this, pEntity ) )
		{
			flVolume = 0;
			return false;
		}
	}

	float flDistance = 0;
	if ( pOrigin )
	{
		flDistance = pOrigin->DistTo( m_hMeasureTarget->GetAbsOrigin() );
	}
	else if ( pEntity )
	{
		flDistance = pEntity->WorldSpaceCenter().DistTo( m_hMeasureTarget->GetAbsOrigin() );
	}

	// Over our max range?
	if ( m_flMaxRange && flDistance > m_flMaxRange )
	{
		return false;
	}

	// Reduce the volume by the amount it fell to get to the microphone
	float gain = enginesound->GetDistGainFromSoundLevel( soundlevel, flDistance );
	flVolume *= gain;

	return ( flVolume > 0 );
}

void CEnvMicrophone::SetSensitivity( float flSensitivity )
{
	m_flSensitivity = flSensitivity;
}

void CEnvMicrophone::SetMaxRange( float flMaxRange )
{
	m_flMaxRange = flMaxRange;
}

void CEnvMicrophone::SetSpeakerName( string_t iszSpeakerName )
{
	m_iszSpeakerName = iszSpeakerName;

	// Set the speaker to null. This will force it to find the speaker next time a sound is routed.
	m_hSpeaker = nullptr;
	ActivateSpeaker();
}

//--------------------------------------------------------------------------------------------------
// Same as above, but skips the speaker name lookup
//--------------------------------------------------------------------------------------------------
void CEnvMicrophone::SetSpeaker( string_t iszSpeakerName, EHANDLE hSpeaker )
{
	m_iszSpeakerName = iszSpeakerName;
	m_hSpeaker = hSpeaker;
	ActivateSpeaker();
}

//-----------------------------------------------------------------------------
// Purpose: Listens for sounds and updates the value of the SoundLevel output.
//-----------------------------------------------------------------------------
void CEnvMicrophone::Think_Impl(void)
{
	int nSound = CSoundEnt::ActiveList();
	bool fHearSound = false;

	float flMaxVolume = 0;
	
	//
	// Find the loudest sound that this microphone cares about.
	//
	while (nSound != SOUNDLIST_EMPTY)
	{
		CSound *pCurrentSound = CSoundEnt::SoundPointerForIndex(nSound);

		if (pCurrentSound)
		{
			if (m_nSoundMask & pCurrentSound->SoundType())
			{
				float flVolume = 0;
				if (CanHearSound(pCurrentSound, flVolume) && (flVolume > flMaxVolume))
				{
					flMaxVolume = flVolume;
					fHearSound = true;
				}
			}
		}

		nSound = pCurrentSound->NextSound();
	}

	if( fHearSound )
	{
		m_OnHeardSound.FireOutput( this, this );
	}

	if (flMaxVolume != m_SoundLevel.Get())
	{
		//
		// Don't smooth if we are within an epsilon. This allows the output to stop firing
		// much more quickly.
		//
		if (fabs(flMaxVolume - m_SoundLevel.Get()) < MICROPHONE_SETTLE_EPSILON)
		{
			m_SoundLevel.Set(flMaxVolume, this, this);
		}
		else
		{
			m_SoundLevel.Set(flMaxVolume * (1 - m_flSmoothFactor) + m_SoundLevel.Get() * m_flSmoothFactor, this, this);
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Hook for the sound system to tell us when a sound's been played
//-----------------------------------------------------------------------------
MicrophoneResult_t CEnvMicrophone::SoundPlayed( int entindex, const char *soundname, soundlevel_t soundlevel, float flVolume, int iFlags, int iPitch, const Vector *pOrigin, float soundtime, std::vector< Vector >& soundorigins )
{
	if ( m_bAvoidFeedback )
		return MicrophoneResult_Ok;

	// Don't hear sounds that have already been heard by a microphone to avoid feedback!
	if ( iFlags & SND_SPEAKER )
		return MicrophoneResult_Ok;

	if ( !CanHearSound( entindex, soundlevel, flVolume, pOrigin ) )
		return MicrophoneResult_Ok;

	// We've heard it. Play it out our speaker. If our speaker's gone away, we're done.
	if ( !m_hSpeaker )
	{
		// First time, find our speaker. Done here, because finding it in Activate() wouldn't
		// find players, and we need to be able to specify !player for a speaker.
		if ( m_iszSpeakerName != NULL_STRING )
		{
			m_hSpeaker = gEntList.FindEntityByName(nullptr, STRING(m_iszSpeakerName) );

			if ( !m_hSpeaker )
			{
				Warning( "EnvMicrophone %s specifies a non-existent speaker name: %s\n", STRING(GetEntityName()), STRING(m_iszSpeakerName) );
				m_iszSpeakerName = NULL_STRING;
			}
		}

		if ( !m_hSpeaker )
		{
			return MicrophoneResult_Remove;
		}
	}

	m_bAvoidFeedback = true;

	// Add the speaker flag. Detected at playback and applies the speaker filter.
	iFlags |= SND_SPEAKER;
	CPASAttenuationFilter filter( m_hSpeaker );

	EmitSound_t ep;
	ep.m_nChannel = CHAN_STATIC;
	ep.m_pSoundName = soundname;
	ep.m_flVolume = flVolume;
	ep.m_SoundLevel = soundlevel;
	ep.m_nFlags = iFlags;
	ep.m_nPitch = iPitch;
	ep.m_pOrigin = &m_hSpeaker->GetAbsOrigin();
	ep.m_flSoundTime = soundtime;
	ep.m_nSpeakerEntity = entindex;

	CBaseEntity::EmitSound( filter, m_hSpeaker->entindex(), ep );

	V_strncpy( m_szLastSound, soundname, sizeof(m_szLastSound) );
	m_OnRoutedSound.FireOutput( this, this, 0 );

	m_bAvoidFeedback = false;

	// Copy emitted origin to soundorigins array
	for ( int i = 0; i < std::ssize( ep.m_UtlVecSoundOrigin ); ++i )
	{
		soundorigins.push_back( ep.m_UtlVecSoundOrigin[ i ] );
	}

	// Do we want to allow the original sound to play?
	if ( m_spawnflags & SF_MICROPHONE_SWALLOW_ROUTED_SOUNDS )
	{
		return MicrophoneResult_Swallow;
	}

	return MicrophoneResult_Ok;
}

void CEnvMicrophone::SoundStopped( const char *soundname )
{
	if ( m_hSpeaker )
	{
		CBaseEntity::StopSound( m_hSpeaker->entindex(), CHAN_STATIC, soundname, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called by the sound system whenever a sound is played so that
//			active microphones can have a chance to pick up the sound.
// Output : Returns whether or not the sound was swallowed by the microphone.
//			Swallowed sounds should not be played by the sound system.
//-----------------------------------------------------------------------------
bool CEnvMicrophone::OnSoundPlayed( int entindex, const char *soundname, soundlevel_t soundlevel, float flVolume, int iFlags, int iPitch, const Vector *pOrigin, float soundtime, std::vector< Vector >& soundorigins )
{
	bool bSwallowed = false;

	// Loop through all registered microphones and tell them the sound was just played
	int iCount = std::ssize( s_Microphones );
	if ( iCount > 0 )
	{
		// Iterate backwards because we might be deleting microphones.
		for ( int i = iCount - 1; i >= 0; i-- )
		{
			if ( s_Microphones[i] )
			{
				MicrophoneResult_t eResult = s_Microphones[i]->SoundPlayed(
					entindex, 
					soundname, 
					soundlevel, 
					flVolume, 
					iFlags, 
					iPitch, 
					pOrigin, 
					soundtime,
					soundorigins );

				if ( eResult == MicrophoneResult_Swallow )
				{
					// Microphone told us to swallow it
					bSwallowed = true;
				}
				else if ( eResult == MicrophoneResult_Remove )
				{
					ks::swap_remove( s_Microphones, i );
				}
			}
		}
	}

	return bSwallowed;
}

void CEnvMicrophone::OnSoundStopped( const char *soundname )
{
	// Loop through all registered microphones and tell them which sound to stop
	int iCount = std::ssize( s_Microphones );
	if ( iCount > 0 )
	{
		for ( int i = iCount - 1; i >= 0; i-- )
		{
			if ( s_Microphones[i] )
			{
				s_Microphones[i]->SoundStopped( soundname );
			}
		}
	}
}

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

static constexpr auto TEMPENT_TEST_GAP = 1.0f;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecOrigin - 
//			&vecAngles - 
//			*single_te - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CTempEntTester::Create( const Vector &vecOrigin, const QAngle &vecAngles, const char *lifetime, const char *single_te )
{
	float life;
	char classname[ 128 ];
	if ( lifetime && lifetime[0] )
	{
		life = std::atoi( lifetime );
		life = std::max<std::common_type_t<double, float>>( 1.0, life );
		life = std::min<std::common_type_t<double, float>>( 1000.0, life );

		life += gpGlobals->curtime;
	}
	else
	{
		Msg( "Usage:  te <lifetime> <entname>\n" );
		return nullptr;
	}

	if ( single_te && single_te[0] )
	{
		V_strncpy( classname, single_te ,sizeof(classname));
		strlwr( classname );
	}
	else
	{
		Msg( "Usage:  te <lifetime> <entname>\n" );
		return nullptr;
	}

	CTempEntTester *p = ( CTempEntTester * )CBaseEntity::CreateNoSpawn( "te_tester", vecOrigin, vecAngles );
	if ( !p )
	{
		return nullptr;
	}

	V_strncpy( p->m_szClass, classname ,sizeof(p->m_szClass));
	p->m_fLifeTime = life;

	p->Spawn();

	return p;
}

LINK_ENTITY_TO_CLASS( te_tester, CTempEntTester );

//-----------------------------------------------------------------------------
// Purpose: Called when object is being created
//-----------------------------------------------------------------------------
void CTempEntTester::Spawn_Impl( void )
{
	// Not a physical thing...
	AddEffects( EF_NODRAW );

	m_pCurrent = CBaseTempEntity::GetList();
	while ( m_pCurrent )
	{
		char name[ 128 ];
		V_strncpy( name, m_pCurrent->GetName() ,sizeof(name));
		strlwr( name );
		if ( std::strstr( name, m_szClass ) )
		{
			break;
		}

		m_pCurrent = m_pCurrent->GetNext();
	}

	if ( !m_pCurrent )
	{
		DevMsg("Couldn't find temp entity '%s'\n", m_szClass );
		UTIL_Remove( this );
		return;
	}

	// Think right away
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Called when object should fire itself and move on
//-----------------------------------------------------------------------------
void CTempEntTester::Think_Impl( void )
{
	// Should never happen
	if ( !m_pCurrent )
	{
		UTIL_Remove( this );
		return;
	}

	m_pCurrent->Test( GetLocalOrigin(), GetLocalAngles() );
	SetNextThink( gpGlobals->curtime + TEMPENT_TEST_GAP );

	// Time to destroy?
	if ( gpGlobals->curtime >= m_fLifeTime )
	{
		UTIL_Remove( this );
		return;
	}
}

class CBaseEntity;
class CBasePlayer;
class CNavArea;
class CTeam;

using namespace ::ks::reflect;

CEventLog::CEventLog()
{
}

CEventLog::~CEventLog()
{
}

void CEventLog::FormatPlayer( CBaseEntity *ent, char *str, int len ) const
{
	if ( !str || len <= 0 )
	{
		return;
	}

	CBasePlayer *player = ToBasePlayer( ent );

	const char *playerName = "Unknown";
	int userID = 0;
	const char *networkIDString = "";
	const char *teamName = "";
	int areaID = 0;
	if ( player )
	{
		playerName = player->GetPlayerName();
		userID = player->GetUserID();
		networkIDString = player->GetNetworkIDString();
		CTeam *team = player->GetTeam();
		if ( team )
		{
			teamName = team->GetName();
		}
	}

	if ( ent && ent->MyCombatCharacterPointer() )
	{
		CNavArea *area = ent->MyCombatCharacterPointer()->GetLastKnownArea();
		if ( area )
		{
			areaID = area->GetID();
		}
	}

	V_snprintf( str, len, "\"%s<%i><%s><%s><Area %d>\"", playerName, userID, networkIDString, teamName, areaID );
}

const char *CEventLog::FormatPlayer( CBaseEntity *ent ) const
{
	const int MaxEntries = 4;
	const int BufferLength = PLAYER_LOGINFO_SIZE;
	static char s_buffer[ MaxEntries ][ BufferLength ];
	static int s_index = 0;

	char *ret = s_buffer[ s_index++ ];
	if ( s_index >= MaxEntries )
	{
		s_index = 0;
	}

	FormatPlayer( ent, ret, BufferLength );
	return ret;
}

void CEventLog::FireGameEvent( IGameEvent *event )
{
	if ( !g_bIsLogging )
		return;

	PrintEvent ( event );
}

bool CEventLog::PrintEvent( IGameEvent *event )
{
	const char * name = event->GetName();

	if ( StringHasPrefixCaseSensitive( name, "server_" ) )
	{
		return true; // we don't care about server events (engine does)
	}
	else if ( StringHasPrefixCaseSensitive( name, "player_" ) )
	{
		return PrintPlayerEvent( event );
	}
	else if ( StringHasPrefixCaseSensitive( name, "team_" ) )
	{
		return PrintTeamEvent( event );
	}
	else if ( StringHasPrefixCaseSensitive( name, "game_" ) )
	{
		return PrintGameEvent( event );
	}
	else
	{
		return PrintOtherEvent( event ); // bomb_, round_, et al
	}
}

bool CEventLog::PrintGameEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + V_strlen("game_"); // remove prefix

	return false;
}

bool CEventLog::PrintPlayerEvent( IGameEvent *event )
{
	const char * eventName = event->GetName();
	const int userid = event->GetInt( "userid" );

	if ( StringHasPrefixCaseSensitive( eventName, "player_connect" ) ) // player connect is before the CBasePlayer pointer is setup
	{
		const char *name = event->GetString( "name" );
		const char *address = event->GetString( "address" );
		const char *networkid = event->GetString("networkid" );
		UTIL_LogPrintf( "\"%s<%i><%s><>\" connected, address \"%s\"\n", name, userid, networkid, address);
		return true;
	}
	else if ( StringHasPrefixCaseSensitive( eventName, "player_disconnect" ) )
	{
		const char *reason = event->GetString("reason" );
		const char *name = event->GetString("name" );
		const char *networkid = event->GetString("networkid" );
		CTeam *team = nullptr;
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

		if ( pPlayer )
		{
			team = pPlayer->GetTeam();
		}

		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected (reason \"%s\")\n", name, userid, networkid, team ? team->GetName() : "", reason );
		return true;
	}

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if ( !pPlayer)
	{
		DevMsg( "CEventLog::PrintPlayerEvent: Failed to find player (userid: %i, event: %s)\n", userid, eventName );
		return false;
	}

	if ( StringHasPrefixCaseSensitive( eventName, "player_team" ) )
	{
		const bool bDisconnecting = event->GetBool( "disconnect" );

		if ( !bDisconnecting )
		{
			const int newTeam = event->GetInt( "team" );
			const int oldTeam = event->GetInt( "oldteam" );
			CTeam *team = GetGlobalTeam( newTeam );
			CTeam *oldteam = GetGlobalTeam( oldTeam );

			const char *playerName = pPlayer->GetPlayerName();
			int userID = pPlayer->GetUserID();
			if ( userID > 0 )
			{
				const char *networkID = pPlayer->GetNetworkIDString();
				const char *oldTeamName = (oldteam) ? oldteam->GetName() : "Unassigned";
				const char *teamName = (team) ? team->GetName() : "Unassigned";

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\"\n", 
				playerName,
				userID,
				networkID,
				oldTeamName,
				teamName );
			}
		}

		return true;
	}
	else if ( StringHasPrefixCaseSensitive( eventName, "player_death" ) )
	{
		const int attackerid = event->GetInt("attacker" );

		CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
		CTeam *team = pPlayer->GetTeam();
		CTeam *attackerTeam = nullptr;
		
		if ( pAttacker )
		{
			attackerTeam = pAttacker->GetTeam();
		}
		if ( pPlayer == pAttacker && pPlayer )  
		{  

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : "",
							pAttacker->GetClassname()
							);
		}
		else if ( pAttacker )
		{
			CTeam *attackerTeam = pAttacker->GetTeam();

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\"\n",  
							pAttacker->GetPlayerName(),
							attackerid,
							pAttacker->GetNetworkIDString(),
							attackerTeam ? attackerTeam->GetName() : "",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);								
		}
		else
		{  
			// killed by the world
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"world\"\n",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);
		}
		return true;
	}
	else if ( StringHasPrefixCaseSensitive( eventName, "player_activate" ) )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><>\" entered the game\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString()
							);

		return true;
	}
	else if ( StringHasPrefixCaseSensitive( eventName, "player_changename" ) )
	{
		const char *newName = event->GetString( "newname" );
		const char *oldName = event->GetString( "oldname" );
		CTeam *team = pPlayer->GetTeam();
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
					oldName,
					userid,
					pPlayer->GetNetworkIDString(),
					team ? team->GetName() : "",
					newName
					);
		return true;
	}
				   
// ignored events
//player_hurt
	return false;
}

bool CEventLog::PrintTeamEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + V_strlen("team_"); // remove prefix

	return false;
}

bool CEventLog::PrintOtherEvent( IGameEvent *event )
{
	return false;
}

bool CEventLog::Init()
{
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "player_activate" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_connect" );

	return true;
}

// NOTE: If CStringRegistry allowed storing arbitrary data, we could just use that.
// in this case we have the "isPrivate" member and the replacement rules 
// (eventIndex can be reused by private activities), so a custom table is necessary
struct eventlist_t
{
	int					eventIndex;
	int					iType;
	CStringRegistry::Key	stringKey;
	short				isPrivate;
};

std::vector< eventlist_t > g_EventList;

// This stores the actual event names.  Also, the string ID in the registry is simply an index 
// into the g_EventList array.
CStringRegistry	g_EventStrings;

// this is just here to accelerate adds
static int g_HighestEvent = 0;

int g_nEventListVersion = 1;

void EventList_Init( void )
{
	g_HighestEvent = 0;
}

void EventList_Free( void )
{
	g_EventStrings.ClearStrings();
	g_EventList.clear(); g_EventList.shrink_to_fit();

	// So studiohdrs can reindex event indices
	++g_nEventListVersion;
}

// add a new event to the database
eventlist_t *EventList_AddEventEntry( const char *pName, int iEventIndex, bool isPrivate, int iType )
{
	int index = std::ssize( g_EventList ); g_EventList.emplace_back();
	eventlist_t *pList = &g_EventList[index];
	pList->eventIndex = iEventIndex;
	pList->stringKey = g_EventStrings.AddString( pName, index );
	pList->isPrivate = isPrivate;
	pList->iType = iType;	

	// UNDONE: This implies that ALL shared activities are added before ANY custom activities
	// UNDONE: Segment these instead?  It's a 32-bit int, how many activities do we need?
	if ( iEventIndex > g_HighestEvent )
	{
		g_HighestEvent = iEventIndex;
	}

	return pList;
}

// get the database entry from a string
static eventlist_t *ListFromString( const char *pString )
{
	// just use the string registry to do this search/map
	int stringID = g_EventStrings.GetStringID( pString );
	if ( stringID < 0 )
		return nullptr;

	return &g_EventList[stringID];
}

// Get the database entry for an index
static eventlist_t *ListFromEvent( int eventIndex )
{
	// ugly linear search
	for ( int i = 0; i < std::ssize( g_EventList ); i++ )
	{
		if ( g_EventList[i].eventIndex == eventIndex )
		{
			return &g_EventList[i];
		}
	}

	return nullptr;
}

int EventList_GetEventType( int eventIndex )
{
	eventlist_t *pEvent = ListFromEvent( eventIndex );

	if ( pEvent )
	{
		return pEvent->iType;
	}

	return -1;
}

bool EventList_RegisterSharedEvent( const char *pszEventName, int iEventIndex, int iType )
{
	// UNDONE: Do we want to do these checks when not in developer mode? or maybe DEBUG only?
	// They really only matter when you change the list of code controlled activities.  IDs
	// for content controlled activities never collide because they are generated.

	// first, check to make sure the slot we're asking for is free. It must be for 
	// a shared event.
	eventlist_t *pList = ListFromString( pszEventName );
	if ( !pList )
	{
		pList = ListFromEvent( iEventIndex );
	}

	//Already in list.
	if ( pList )
	{
		return false;
	}
	// ----------------------------------------------------------------

	EventList_AddEventEntry( pszEventName, iEventIndex, false, iType );
	return true;
}

Animevent EventList_RegisterPrivateEvent( const char *pszEventName )
{
	eventlist_t *pList = ListFromString( pszEventName );
	if ( pList )
	{
		// this activity is already in the list. If the activity we collided with is also private, 
		// then the collision is OK. Otherwise, it's a bug.
		if ( pList->isPrivate )
		{
			return (Animevent)pList->eventIndex;
		}
		else
		{
			// this private activity collides with a shared activity. That is not allowed.
			Warning( "***\nShared<->Private Event collision!\n***\n" );
			contract_assert( 0 );
			return AE_INVALID;
		}
	}

	int iType = AE_TYPE_SERVER;

	pList = EventList_AddEventEntry( pszEventName, g_HighestEvent+1, true, iType );
	return (Animevent)pList->eventIndex;
}

// Get the index for a given Event name
// Done at load time for all models
int EventList_IndexForName( const char *pszEventName )
{
	// this is a fast O(lgn) search (actually does 2 O(lgn) searches)
	eventlist_t *pList = ListFromString( pszEventName );

	if ( pList )
	{
		return pList->eventIndex;
	}

	return -1;
}

// Get the name for a given index
// This should only be used in debug code, it does a linear search
// But at least it only compares integers
const char *EventList_NameForIndex( int eventIndex )
{
	eventlist_t *pList = ListFromEvent( eventIndex );
	if ( pList )
	{
		return g_EventStrings.GetStringForKey( pList->stringKey );
	}
	return nullptr;
}

void EventList_RegisterSharedEvents( void )
{
	REGISTER_SHARED_ANIMEVENT( AE_EMPTY, AE_TYPE_SERVER );
	
	REGISTER_SHARED_ANIMEVENT( AE_NPC_LEFTFOOT, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_RIGHTFOOT, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_BODYDROP_LIGHT, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_BODYDROP_HEAVY, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_SWISHSOUND, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_180TURN, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_ITEM_PICKUP, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_DROP, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_SEQUENCE_NAME, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_SEQUENCE_NUMBER, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_ACTIVITY, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_HOLSTER, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_DRAW, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_FIRE, AE_TYPE_SERVER | AE_TYPE_WEAPON );

	REGISTER_SHARED_ANIMEVENT( AE_CL_PLAYSOUND, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_SV_PLAYSOUND, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_STOPSOUND, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_START_SCRIPTED_EFFECT, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_STOP_SCRIPTED_EFFECT, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CLIENT_EFFECT_ATTACH, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_MUZZLEFLASH, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_MUZZLEFLASH, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_THUMPER_THUMP, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_AMMOCRATE_PICKUP_AMMO, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_RAGDOLL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_ADDGESTURE, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_RESTARTGESTURE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_ATTACK_BROADCAST, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_HURT_INTERACTION_PARTNER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_SET_INTERACTION_CANTDIE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_SV_DUSTTRAIL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_CREATE_PARTICLE_EFFECT, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_STOP_PARTICLE_EFFECT, AE_TYPE_CLIENT );	
	REGISTER_SHARED_ANIMEVENT( AE_CL_ADD_PARTICLE_EFFECT_CP, AE_TYPE_CLIENT );	
	REGISTER_SHARED_ANIMEVENT( AE_CL_CREATE_PARTICLE_BRASS, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_RAGDOLL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_ENABLE_BODYGROUP, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_DISABLE_BODYGROUP, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_VALUE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_TO_CLIP, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_TO_NEXTCLIP, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_EJECT_MAG, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_EJECT_MAG_UNHIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_SHOW_SILENCER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_ATTACH_SILENCER_COMPLETE, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_HIDE_SILENCER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_DETACH_SILENCER_COMPLETE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_PRIMARYATTACK, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_INCREMENTAMMO, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_HIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_UNHIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_ASW_FOOTSTEP, AE_TYPE_CLIENT  );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_FOOTSTEP, AE_TYPE_CLIENT  );
	
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_A, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_B, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_C, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_REMOVE_CLIENT_AIM, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_PLAYWPNSOUND, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_COMPLETE_RELOAD, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_MELEE_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_MELEE_START_COLLISION_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_MELEE_STOP_COLLISION_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_SCREEN_SHAKE, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_START_DETECTING_COMBO, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_STOP_DETECTING_COMBO, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_COMBO_TRANSITION, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_ALLOW_MOVEMENT, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_SKILL_EVENT, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_TUG_INCAP, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_BEGIN_TAUNT_LOOP, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CLIENT_EJECT_BRASS, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_CL_SET_STATTRAK_GLOW, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_CZ_DUMP_CURRENT_MAG, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_CZ_UPDATE_BODYGROUP, AE_TYPE_SERVER );
	
	REGISTER_SHARED_ANIMEVENT( AE_WPN_CLIP_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_NEXTCLIP_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_EMPTYSHOTS_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );

}

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements an explosion entity and a support spark shower entity.

//-----------------------------------------------------------------------------
// Purpose: Spark shower, created by the explosion entity.
//-----------------------------------------------------------------------------
class CShower : public CPointEntity
{
public:
	DECLARE_CLASS( CShower, CPointEntity );

	void Spawn_Impl( void );
	void Think_Impl( void );
	void Touch_Impl( CBaseEntity *pOther );
	int ObjectCaps_Impl( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( spark_shower, CShower );

void CShower::Spawn_Impl( void )
{
	Vector vecForward;
	AngleVectors( GetLocalAngles(), &vecForward );

	Vector vecNewVelocity;
	vecNewVelocity = random_valve->RandomFloat( 200, 300 ) * vecForward;
	vecNewVelocity.x += random_valve->RandomFloat(-100.f,100.f);
	vecNewVelocity.y += random_valve->RandomFloat(-100.f,100.f);
	if ( vecNewVelocity.z >= 0 )
		vecNewVelocity.z += 200;
	else
		vecNewVelocity.z -= 200;
	SetAbsVelocity( vecNewVelocity );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetGravity( UTIL_ScaleForGravity( 400 ) ); // fall a bit more slowly than normal
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetSolid( SOLID_NONE );
	UTIL_SetSize(this, vec3_origin, vec3_origin );
	AddEffects( EF_NODRAW );
	m_flSpeed = random_valve->RandomFloat( 0.5, 1.5 );

	SetLocalAngles( vec3_angle );
}

void CShower::Think_Impl( void )
{
	g_pEffects->Sparks( GetAbsOrigin() );

	m_flSpeed -= 0.1;
	if ( m_flSpeed > 0 )
		SetNextThink( gpGlobals->curtime + 0.1f );
	else
		UTIL_Remove( this );
	SetGroundEntity( nullptr );
}

void CShower::Touch_Impl( CBaseEntity *pOther )
{
	Vector vecNewVelocity = GetAbsVelocity();

	if ( GetFlags() & FL_ONGROUND )
		vecNewVelocity *= 0.1;
	else
		vecNewVelocity *= 0.6;

	if ( (vecNewVelocity.x*vecNewVelocity.x+vecNewVelocity.y*vecNewVelocity.y) < 10.0 )
		m_flSpeed = 0;

	SetAbsVelocity( vecNewVelocity );
}

class CEnvExplosion : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvExplosion, CPointEntity );

	CEnvExplosion( void )
	{
		// Default to invalid.
		m_sFireballSprite = -1;
	};

	void Precache_Impl( void );
	void Spawn_Impl( );
	void Smoke ( void );
	void SetCustomDamageType( int iType ) { m_iCustomDamageType = iType; }
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	int DrawDebugTextOverlays_Impl(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Explode", .type = FIELD_VOID } ]] void InputExplode( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "iMagnitude" } ]] int m_iMagnitude;// how large is the fireball? how much damage?
	[[= ks::reflect::Key{ .name = "iRadiusOverride" } ]] int m_iRadiusOverride;// For use when m_iMagnitude results in larger radius than designer desires.
	int m_spriteScale; // what's the exact fireball sprite scale? 
	[[= ks::reflect::Key{ .name = "DamageForce" } ]] float m_flDamageForce;	// How much damage force should we use?
	string_t m_iszFireballSprite;
	short m_sFireballSprite;
	EHANDLE m_hInflictor;
	int m_iCustomDamageType;

	// passed along to the RadiusDamage call
	[[= ks::reflect::Key{ .name = "ignoredClass" } ]] int m_iClassIgnore;
	[[= ks::reflect::Key{ .name = "ignoredEntity" } ]] EHANDLE m_hEntityIgnore;

};

LINK_ENTITY_TO_CLASS( env_explosion, CEnvExplosion );

IMPLEMENT_REFLECT_DATAMAP( CEnvExplosion )

bool CEnvExplosion::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "fireballsprite"))
	{
		m_iszFireballSprite = AllocPooledString( szValue );
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

void CEnvExplosion::Precache_Impl( void )
{
	if ( m_iszFireballSprite != NULL_STRING )
	{
		m_sFireballSprite = PrecacheModel( STRING( m_iszFireballSprite ) );
	}

	//PrecacheParticleSystem( "freeze_explosion" ) ;
	PrecacheScriptSound( "explode_3" );
}

void CEnvExplosion::Spawn_Impl( void )
{ 
	Precache();

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	SetMoveType( MOVETYPE_NONE );
	/*
	if ( m_iMagnitude > 250 )
	{
		m_iMagnitude = 250;
	}
	*/

	float flSpriteScale;
	flSpriteScale = ( m_iMagnitude - 50) * 0.6;

	// Control the clamping of the fireball sprite
	if( m_spawnflags & SF_ENVEXPLOSION_NOCLAMPMIN )
	{
		// Don't inhibit clamping altogether. Just relax it a bit.
		if ( flSpriteScale < 1 )
		{
			flSpriteScale = 1;
		}
	}
	else
	{
		if ( flSpriteScale < 10 )
		{
			flSpriteScale = 10;
		}
	}

	if( m_spawnflags & SF_ENVEXPLOSION_NOCLAMPMAX )
	{
		// We may need to adjust this to suit designers' needs.
		if ( flSpriteScale > 200 )
		{
			flSpriteScale = 200;
		}
	}
	else
	{
		if ( flSpriteScale > 50 )
		{
			flSpriteScale = 50;
		}
	}

	m_spriteScale = (int)flSpriteScale;
	m_iCustomDamageType = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for making the explosion explode.
//-----------------------------------------------------------------------------
void CEnvExplosion::InputExplode( inputdata_t &inputdata )
{ 
	trace_t tr;

	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );// intangible

	Vector vecSpot = GetAbsOrigin() + Vector( 0 , 0 , 8 );
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), (MASK_SOLID_BRUSHONLY | MASK_WATER), this, COLLISION_GROUP_NONE, &tr );
	
	// Pull out of the wall a bit. We used to move the explosion origin itself, but that seems unnecessary, not to mention a
	// little weird when you consider that it might be in hierarchy. Instead we just calculate a new virtual position at
	// which to place the explosion. We don't use that new position to calculate radius damage because according to Steve's
	// comment, that adversely affects the force vector imparted on explosion victims when they ragdoll.
	Vector vecExplodeOrigin = GetAbsOrigin();
	if ( tr.fraction != 1.0 )
	{
		vecExplodeOrigin = tr.endpos + (tr.plane.normal * 24 );
	}

	// draw decal
	if (! ( m_spawnflags & SF_ENVEXPLOSION_NODECAL ))
	{
		if ( ! ( m_spawnflags & SF_ENVEXPLOSION_ICE ))
			UTIL_DecalTrace( &tr, "Scorch" );
		else
			UTIL_DecalTrace( &tr, "Ice_Explosion_Decal" );
	}

	// It's stupid that this entity's spawnflags and the flags for the
	// explosion temp ent don't match up. But because they don't, we
	// have to reinterpret some of the spawnflags to determine which
	// flags to pass to the temp ent.
	int nFlags = TE_EXPLFLAG_NONE;

	if( m_spawnflags & SF_ENVEXPLOSION_NOFIREBALL )
	{
		nFlags |= TE_EXPLFLAG_NOFIREBALL;
	}
	
	if( m_spawnflags & SF_ENVEXPLOSION_NOSOUND )
	{
		nFlags |= TE_EXPLFLAG_NOSOUND;
	}
	
	if ( m_spawnflags & SF_ENVEXPLOSION_RND_ORIENT )
	{
		nFlags |= TE_EXPLFLAG_ROTATE;
	}

	if ( m_nRenderMode == kRenderTransAlpha )
	{
		nFlags |= TE_EXPLFLAG_DRAWALPHA;
	}
	else if ( m_nRenderMode != kRenderTransAdd )
	{
		nFlags |= TE_EXPLFLAG_NOADDITIVE;
	}

	if( m_spawnflags & SF_ENVEXPLOSION_NOPARTICLES )
	{
		nFlags |= TE_EXPLFLAG_NOPARTICLES;
	}

	if( !(m_spawnflags & SF_ENVEXPLOSION_NODLIGHTS) )
	{
		nFlags |= TE_EXPLFLAG_DLIGHT;
	}

	if ( m_spawnflags & SF_ENVEXPLOSION_NOFIREBALLSMOKE )
	{
		nFlags |= TE_EXPLFLAG_NOFIREBALLSMOKE;
	}

	if ( m_spawnflags & SF_ENVEXPLOSION_ICE )
	{
		nFlags |= TE_EXPLFLAG_ICE;
	}

	//Get the damage override if specified
	int	iRadius = ( m_iRadiusOverride > 0 ) ? m_iRadiusOverride : ( m_iMagnitude * 2.5f );

	CPASFilter filter( vecExplodeOrigin );
	te->Explosion( filter, 0.0,
		vecExplodeOrigin, 
		( m_sFireballSprite < 1 ) ? g_sModelIndexFireball : m_sFireballSprite,
		!( m_spawnflags & SF_ENVEXPLOSION_NOFIREBALL ) ? ( m_spriteScale / 10.0 ) : 0.0,
		15,
		nFlags,
		iRadius,
		m_iMagnitude );

	// do damage
	if ( !( m_spawnflags & SF_ENVEXPLOSION_NODAMAGE ) )
	{
		CBaseEntity *pAttacker = GetOwnerEntity() ? GetOwnerEntity() : this;

		// Only calculate damage type if we didn't get a custom one passed in
		int iDamageType = m_iCustomDamageType;
		if ( iDamageType == -1 )
		{
			iDamageType = HasSpawnFlags( SF_ENVEXPLOSION_GENERIC_DAMAGE ) ? DMG_GENERIC : DMG_BLAST;
		}

		CTakeDamageInfo info( m_hInflictor ? m_hInflictor : this, pAttacker, m_iMagnitude, iDamageType );

		if( HasSpawnFlags( SF_ENVEXPLOSION_SURFACEONLY ) )
		{
			info.AddDamageType( DMG_BLAST_SURFACE );
		}

		if ( m_flDamageForce )
		{
			// Not the right direction, but it'll be fixed up by RadiusDamage.
			info.SetDamagePosition( GetAbsOrigin() );
			info.SetDamageForce( Vector( m_flDamageForce, 0, 0 ) );
		}

		RadiusDamage( info, GetAbsOrigin(), iRadius, m_iClassIgnore, m_hEntityIgnore.Get() );
	}

	SetThink( &CEnvExplosion::Smoke );
	SetNextThink( gpGlobals->curtime + 0.3 );

	// Only do these effects if we're not submerged
	if ( UTIL_PointContents( GetAbsOrigin(), MASK_WATER ) & CONTENTS_WATER )
	{
		// draw sparks
		if ( !( m_spawnflags & SF_ENVEXPLOSION_NOSPARKS ) )
		{
			int sparkCount = random_valve->RandomInt(0,3);

			for ( int i = 0; i < sparkCount; i++ )
			{
				QAngle angles;
				VectorAngles( tr.plane.normal, angles );
				Create( "spark_shower", vecExplodeOrigin, angles, nullptr );
			}
		}
	}
}

void CEnvExplosion::Smoke( void )
{
	if ( !(m_spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		UTIL_Remove( this );
	}
}

// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int magnitude, int radius, int nSpawnFlags, float flExplosionForce, CBaseEntity *pInflictor, int iCustomDamageType,
	const EHANDLE *ignoredEntity , Class_T ignoredClass )
{
	char			buf[128];

	CEnvExplosion *pExplosion = (CEnvExplosion*)CBaseEntity::Create( "env_explosion", center, angles, pOwner );
	V_snprintf( buf,sizeof(buf), "%3d", magnitude );
	const char *szKeyName = "iMagnitude";
	char *szValue = buf;
	pExplosion->KeyValue( szKeyName, szValue );

	pExplosion->AddSpawnFlags( nSpawnFlags );

	if ( radius )
	{
		V_snprintf( buf,sizeof(buf), "%d", radius );
		pExplosion->KeyValue( "iRadiusOverride", buf );
	}

	if ( flExplosionForce != 0.0f )
	{
		V_snprintf( buf,sizeof(buf), "%.3f", flExplosionForce );
		pExplosion->KeyValue( "DamageForce", buf );
	}

	variant_t emptyVariant;
	pExplosion->m_nRenderMode = kRenderTransAdd;
	pExplosion->SetOwnerEntity( pOwner );
	pExplosion->Spawn();
	pExplosion->m_hInflictor = pInflictor;
	pExplosion->SetCustomDamageType( iCustomDamageType );
	if (ignoredEntity)
	{
		pExplosion->m_hEntityIgnore = *ignoredEntity;
	}
	pExplosion->m_iClassIgnore = ignoredClass;

	pExplosion->AcceptInput( "Explode", nullptr, nullptr, emptyVariant, 0 );
}

void ExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, float flExplosionForce, bool bSurfaceOnly, bool bSilent, int iCustomDamageType )
{
	// For E3, no sparks
	int nFlags = SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE;
	if ( !doDamage )
	{
		nFlags |= SF_ENVEXPLOSION_NODAMAGE;
	}

	if( bSurfaceOnly )
	{
		nFlags |= SF_ENVEXPLOSION_SURFACEONLY;
	}

	if( bSilent )
	{
		nFlags |= SF_ENVEXPLOSION_NOSOUND;
	}

	ExplosionCreate( center, angles, pOwner, magnitude, radius, nFlags, flExplosionForce, nullptr, iCustomDamageType );
}

// this version lets you specify classes or entities to be ignored
void ExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, 
					 const EHANDLE *ignoredEntity, Class_T ignoredClass,
					 float flExplosionForce , bool bSurfaceOnly , bool bSilent , int iCustomDamageType )
{
	// For E3, no sparks
	int nFlags = SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE;
	if ( !doDamage )
	{
		nFlags |= SF_ENVEXPLOSION_NODAMAGE;
	}

	if( bSurfaceOnly )
	{
		nFlags |= SF_ENVEXPLOSION_SURFACEONLY;
	}

	if( bSilent )
	{
		nFlags |= SF_ENVEXPLOSION_NOSOUND;
	}

	ExplosionCreate( center, angles, pOwner, magnitude, radius, nFlags, flExplosionForce, nullptr, iCustomDamageType, ignoredEntity, ignoredClass );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CEnvExplosion::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		V_snprintf(tempstr,sizeof(tempstr),"    magnitude: %i", m_iMagnitude);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

class CBaseEntity;
class CBaseFilter;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// ###################################################################
//	> BaseFilter
// ###################################################################
LINK_ENTITY_TO_CLASS(filter_base, CBaseFilter);

IMPLEMENT_REFLECT_DATAMAP( CBaseFilter )

//-----------------------------------------------------------------------------

bool CBaseFilter::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	return true;
}

bool CBaseFilter::PassesFilter( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	bool baseResult = PassesFilterImpl( pCaller, pEntity );
	return (m_bNegated) ? !baseResult : baseResult;
}

bool CBaseFilter::PassesDamageFilter_Impl(const CTakeDamageInfo &info)
{
	bool baseResult = PassesDamageFilterImpl(info);
	return (m_bNegated) ? !baseResult : baseResult;
}

bool CBaseFilter::PassesDamageFilterImpl( const CTakeDamageInfo &info )
{
	return PassesFilterImpl( nullptr, info.GetAttacker() );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for testing the activator. If the activator passes the
//			filter test, the OnPass output is fired. If not, the OnFail output is fired.
//-----------------------------------------------------------------------------
void CBaseFilter::InputTestActivator( inputdata_t &inputdata )
{
	if ( PassesFilter( inputdata.pCaller, inputdata.pActivator ) )
	{
		m_OnPass.FireOutput( inputdata.pActivator, this );
	}
	else
	{
		m_OnFail.FireOutput( inputdata.pActivator, this );
	}
}

// ###################################################################
//	> FilterMultiple
//
//   Allows one to filter through mutiple filters
// ###################################################################
static constexpr auto MAX_FILTERS = 10;
enum filter_t
{
	FILTER_AND,
	FILTER_OR,
};

class CFilterMultiple : public CBaseFilter
{
	DECLARE_CLASS( CFilterMultiple, CBaseFilter );
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "FilterType" } ]] filter_t	m_nFilterType;
	[[= ks::reflect::Key{ .name = "Filter10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Filter09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Filter08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Filter07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Filter06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Filter05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Filter04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Filter03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Filter02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Filter01", .index = 0 } ]] string_t	m_iFilterName[MAX_FILTERS];
	EHANDLE		m_hFilter[MAX_FILTERS];

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );
	bool PassesDamageFilterImpl(const CTakeDamageInfo &info);
	public: void Activate_Impl(void);
	private:
};

LINK_ENTITY_TO_CLASS(filter_multi, CFilterMultiple);

IMPLEMENT_REFLECT_DATAMAP( CFilterMultiple )

//------------------------------------------------------------------------------
// Purpose : Called after all entities have been loaded
//------------------------------------------------------------------------------
void CFilterMultiple::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	
	// We may reject an entity specified in the array of names, but we want the array of valid filters to be contiguous!
	int nNextFilter = 0;

	// Get handles to my filter entities
	for ( int i = 0; i < MAX_FILTERS; i++ )
	{
		if ( m_iFilterName[i] != NULL_STRING )
		{
			CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_iFilterName[i] );
			CBaseFilter *pFilter = dynamic_cast<CBaseFilter *>(pEntity);
			if ( pFilter == nullptr )
			{
				Warning("filter_multi: Tried to add entity (%s) which is not a filter entity!\n", STRING( m_iFilterName[i] ) );
				continue;
			}

			// Take this entity and increment out array pointer
			m_hFilter[nNextFilter] = pFilter;
			nNextFilter++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity passes our filter, false if not.
// Input  : pEntity - Entity to test.
//-----------------------------------------------------------------------------
bool CFilterMultiple::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	// Test against each filter
	if (m_nFilterType == FILTER_AND)
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != nullptr)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (!pFilter->PassesFilter( pCaller, pEntity ) )
				{
					return false;
				}
			}
		}
		return true;
	}
	else  // m_nFilterType == FILTER_OR
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != nullptr)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (pFilter->PassesFilter( pCaller, pEntity ) )
				{
					return true;
				}
			}
		}
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity passes our filter, false if not.
// Input  : pEntity - Entity to test.
//-----------------------------------------------------------------------------
bool CFilterMultiple::PassesDamageFilterImpl(const CTakeDamageInfo &info)
{
	// Test against each filter
	if (m_nFilterType == FILTER_AND)
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != nullptr)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (!pFilter->PassesDamageFilter(info))
				{
					return false;
				}
			}
		}
		return true;
	}
	else  // m_nFilterType == FILTER_OR
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != nullptr)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (pFilter->PassesDamageFilter(info))
				{
					return true;
				}
			}
		}
		return false;
	}
}

// ###################################################################
//	> FilterName
// ###################################################################
class CFilterName : public CBaseFilter
{
	DECLARE_CLASS( CFilterName, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t m_iFilterName;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		// special check for !player as GetEntityName for player won't return "!player" as a name
		if (FStrEq(STRING(m_iFilterName), "!player"))
		{
			return pEntity->IsPlayer();
		}
		else
		{
			return pEntity->NameMatches( STRING(m_iFilterName) );
		}
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_name, CFilterName );

IMPLEMENT_REFLECT_DATAMAP( CFilterName )

// ###################################################################
//	> FilterModel
// ###################################################################
class CFilterModel : public CBaseFilter
{
	DECLARE_CLASS( CFilterModel, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "model" } ]] string_t m_iFilterModel;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		return ( FStrEq( STRING( m_iFilterModel ), STRING( pEntity->GetModelName() ) ) );

	}
};

LINK_ENTITY_TO_CLASS( filter_activator_model, CFilterModel );

IMPLEMENT_REFLECT_DATAMAP( CFilterModel )

// ###################################################################
//	> FilterContext
// ###################################################################
class CFilterContext : public CBaseFilter
{
	DECLARE_CLASS( CFilterContext, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "ResponseContext" } ]] string_t m_iFilterContext;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		int i = pEntity->FindContextByName( STRING( m_iFilterContext ) );
		return ( ( i != -1 && std::atoi( pEntity->GetContextValue( i ) ) > 0 ) );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_context, CFilterContext );

IMPLEMENT_REFLECT_DATAMAP( CFilterContext )

// ###################################################################
//	> FilterClass
// ###################################################################
class CFilterClass : public CBaseFilter
{
	DECLARE_CLASS( CFilterClass, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filterclass" } ]] string_t m_iFilterClass;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		return pEntity->ClassMatches( STRING(m_iFilterClass) );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_class, CFilterClass );

IMPLEMENT_REFLECT_DATAMAP( CFilterClass )

// ###################################################################
//	> FilterTeam
// ###################################################################
class FilterTeam : public CBaseFilter
{
	DECLARE_CLASS( FilterTeam, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filterteam" } ]] int		m_iFilterTeam;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
	 	return ( pEntity != nullptr && pEntity->GetTeamNumber() == m_iFilterTeam );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_team, FilterTeam );

IMPLEMENT_REFLECT_DATAMAP( FilterTeam )

// ###################################################################
//	> FilterMassGreater
// ###################################################################
class CFilterMassGreater : public CBaseFilter
{
	DECLARE_CLASS( CFilterMassGreater, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filtermass" } ]] float m_fFilterMass;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		if ( pEntity->VPhysicsGetObject() == nullptr )
			return false;

		return ( pEntity->VPhysicsGetObject()->GetMass() > m_fFilterMass );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_mass_greater, CFilterMassGreater );

IMPLEMENT_REFLECT_DATAMAP( CFilterMassGreater )

// ###################################################################
//	> FilterDamageType
// ###################################################################
class FilterDamageType : public CBaseFilter
{
	DECLARE_CLASS( FilterDamageType, CBaseFilter );
	DECLARE_DATADESC();

protected:

	bool PassesFilterImpl(CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		ASSERT( false );
	 	return true;
	}

	bool PassesDamageFilterImpl(const CTakeDamageInfo &info)
	{
	 	return (info.GetDamageType() & ~DMG_DIRECT) == m_iDamageType;
	}

	[[= ks::reflect::Key{ .name = "damagetype" } ]] int m_iDamageType;
};

LINK_ENTITY_TO_CLASS( filter_damage_type, FilterDamageType );

IMPLEMENT_REFLECT_DATAMAP( FilterDamageType )

// ###################################################################
//	> CFilterEnemy
// ###################################################################

static constexpr auto SF_FILTER_ENEMY_NO_LOSE_AQUIRED = (1<<0);

class CFilterEnemy : public CBaseFilter
{
	DECLARE_CLASS( CFilterEnemy, CBaseFilter );
		// NOT SAVED	
		// m_iszPlayerName
	DECLARE_DATADESC();

public:

	virtual bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );
	virtual bool PassesDamageFilterImpl( const CTakeDamageInfo &info );

private:

	bool	PassesNameFilter( CBaseEntity *pCaller );
	bool	PassesProximityFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy );
	bool	PassesMobbedFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy );

	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t	m_iszEnemyName;				// Name or classname
	[[= ks::reflect::Key{ .name = "filter_radius" } ]] float		m_flRadius;					// Radius (enemies are acquired at this range)
	[[= ks::reflect::Key{ .name = "filter_outer_radius" } ]] float		m_flOuterRadius;			// Outer radius (enemies are LOST at this range)
	[[= ks::reflect::Key{ .name = "filter_max_per_enemy" } ]] int			m_nMaxSquadmatesPerEnemy;	// Maximum number of squadmates who may share the same enemy
	string_t	m_iszPlayerName;			// "!player"

};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	if ( pCaller == nullptr || pEntity == nullptr )
		return false;

	// If asked to, we'll never fail to pass an already acquired enemy
	//	This allows us to use test criteria to initially pick an enemy, then disregard the test until a new enemy comes along
	if ( HasSpawnFlags( SF_FILTER_ENEMY_NO_LOSE_AQUIRED ) && ( pEntity == pCaller->GetEnemy() ) )
		return true;

	// This is a little weird, but it's saying that if we're not the entity we're excluding the filter to, then just pass it through
	if ( PassesNameFilter( pEntity ) == false )
		return true;

	if ( PassesProximityFilter( pCaller, pEntity ) == false )
		return false;

	// NOTE: This can result in some weird NPC behavior if used improperly
	if ( PassesMobbedFilter( pCaller, pEntity ) == false )
		return false;

	// The filter has been passed, meaning:
	//	- If we wanted all criteria to fail, they have
	//  - If we wanted all criteria to succeed, they have

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesDamageFilterImpl( const CTakeDamageInfo &info )
{
	// NOTE: This function has no meaning to this implementation of the filter class!
	contract_assert( 0 );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the enemy's name or classname
// Input  : *pEnemy - Entity being assessed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesNameFilter( CBaseEntity *pEnemy )
{
	// If there is no name specified, we're not using it
	if ( m_iszEnemyName	== NULL_STRING )
		return true;

	// Cache off the special case player name
	if ( m_iszPlayerName == NULL_STRING )
	{
		m_iszPlayerName = FindPooledString( "!player" );
	}

	if ( m_iszEnemyName == m_iszPlayerName )
	{
		if ( pEnemy->IsPlayer() )
		{
			if ( m_bNegated )
				return false;

			return true;
		}
	}

	// May be either a targetname or classname
	bool bNameOrClassnameMatches = ( m_iszEnemyName == pEnemy->GetEntityName() || m_iszEnemyName == pEnemy->m_iClassname );

	// We only leave this code block in a state meaning we've "succeeded" in any context
	if ( m_bNegated )
	{
		// We wanted the names to not match, but they did
		if ( bNameOrClassnameMatches )
			return false;
	}
	else
	{
		// We wanted them to be the same, but they weren't
		if ( bNameOrClassnameMatches == false )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the enemy's proximity to the caller's position
// Input  : *pCaller - Entity assessing the target
//			*pEnemy - Entity being assessed
// Output : Returns true if potential enemy passes this filter stage
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesProximityFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy )
{
	// If there is no radius specified, we're not testing it
	if ( m_flRadius <= 0.0f )
		return true;

	// We test the proximity differently when we've already picked up this enemy before
	bool bAlreadyEnemy = ( pCaller->GetEnemy() == pEnemy );

	// Get our squared length to the enemy from the caller
	float flDistToEnemySqr = ( pCaller->GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();

	// Two radii are used to control oscillation between true/false cases
	// The larger radius is either specified or defaulted to be double or half the size of the inner radius
	float flLargerRadius = m_flOuterRadius;
	if ( flLargerRadius == 0 )
	{
		flLargerRadius = ( m_bNegated ) ? (m_flRadius*0.5f) : (m_flRadius*2.0f);
	}

	float flSmallerRadius = m_flRadius;
	if ( flSmallerRadius > flLargerRadius )
	{
		V_swap( flLargerRadius, flSmallerRadius );
	}

	float flDist;	
	if ( bAlreadyEnemy )
	{
		flDist = ( m_bNegated ) ? flSmallerRadius : flLargerRadius;
	}
	else
	{
		flDist = ( m_bNegated ) ? flLargerRadius : flSmallerRadius;
	}

	// Test for success
	if ( flDistToEnemySqr <= (flDist*flDist) )
	{
		// We wanted to fail but didn't
		if ( m_bNegated )
			return false;

		return true;
	}
	
	// We wanted to succeed but didn't
	if ( m_bNegated == false )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to govern how many squad members can target any given entity
// Input  : *pCaller - Entity assessing the target
//			*pEnemy - Entity being assessed
// Output : Returns true if potential enemy passes this filter stage
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesMobbedFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy )
{
	// Must be a valid candidate
	return true;
}

LINK_ENTITY_TO_CLASS( filter_enemy, CFilterEnemy );

IMPLEMENT_REFLECT_DATAMAP( CFilterEnemy )

class CBaseAnimating;
class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

//---------------------------------------------------------
//---------------------------------------------------------

/********************************************************************
 NOTE: if you are looking at this file becase you would like flares 
 to be considered as fires (and thereby trigger gas traps), be aware 
 that the env_flare class is actually found in weapon_flaregun.cpp 
 and is really a repurposed piece of ammunition. (env_flare isn't the 
 rod-like safety flare prop, but rather the bit of flame on the end.)

 You will have some difficulty making it work here, because CFlare 
 does not inherit from CFire and will thus not be enumerated by 
 CFireSphere::EnumElement(). In order to have flares be detected and 
 used by this system, you will need to promote certain member functions 
 of CFire into an interface class from which both CFire and CFlare 
 inherit. You will also need to modify CFireSphere::EnumElement so that
 it properly disambiguates between fires and flares.

 For some partial work towards this end, see changelist 192474.

 ********************************************************************/
static constexpr auto FIRE_HEIGHT = 256.0f;
template <typename T1> [[gnu::always_inline]] inline auto FIRE_SCALE_FROM_SIZE( T1 firesize ) { return (firesize * (1/FIRE_HEIGHT)); }

static constexpr auto FIRE_MAX_GROUND_OFFSET = 24.0f; //(2 feet)

static constexpr auto DEFAULT_ATTACK_TIME = 4.0f;
static constexpr auto DEFAULT_DECAY_TIME = 8.0f;

// UNDONE: This shouldn't be constant but depend on specific fire
static constexpr auto FIRE_WIDTH = 128;
#define	FIRE_MINS				Vector(-20,-20,0 )   // Sould be FIRE_WIDTH in size
#define FIRE_MAXS				Vector( 20, 20,20)	 // Sould be FIRE_WIDTH in size
static constexpr auto FIRE_SPREAD_DAMAGE_MULTIPLIER = 2.0;

static constexpr auto FIRE_MAX_HEAT_LEVEL = 64.0f;
static constexpr auto FIRE_NORMAL_ATTACK_TIME = 20.0f;
static constexpr auto FIRE_THINK_INTERVAL = 0.1;

ConVar fire_maxabsorb( side_registry(), "fire_maxabsorb", "50" );
ConVar fire_absorbrate( side_registry(), "fire_absorbrate", "3" );
ConVar fire_extscale(side_registry(), "fire_extscale", "12");
ConVar fire_extabsorb(side_registry(), "fire_extabsorb", "5");
ConVar fire_heatscale( side_registry(), "fire_heatscale", "1.0" );
ConVar fire_incomingheatscale( side_registry(), "fire_incomingheatscale", "0.1" );
ConVar fire_dmgscale( side_registry(), "fire_dmgscale", "0.1" );
ConVar fire_dmgbase( side_registry(), "fire_dmgbase", "1" );
ConVar fire_growthrate( side_registry(), "fire_growthrate", "1.0" );
ConVar fire_dmginterval( side_registry(), "fire_dmginterval", "1.0" );

class CFire : public CBaseEntity
{
public:
	DECLARE_CLASS( CFire, CBaseEntity );
	
	int DrawDebugTextOverlays_Impl(void);

	CFire( void );
	
	void UpdateOnRemove_Impl( void );

	void	Precache_Impl( void );
	void	Init( const Vector &position, float scale, float attackTime, float fuel, int flags, int fireType );
	bool	GoOut();
	
	void	BurnThink();
	void	GoOutThink();
	void	GoOutInSeconds( float seconds );

	void	SetOwner( CBaseEntity *hOwner ) { m_hOwner = hOwner; }
	
	void	Scale( float end, float time );
	void	AddHeat( float heat, bool selfHeat = false );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );

	bool	IsBurning( void ) const;

	bool	GetFireDimensions( Vector *pFireMins, Vector *pFireMaxs );
	
	void	Extinguish( float heat );
	void	DestroyEffect();

	virtual	void Update( float simTime );

	void	Spawn_Impl( void );
	void	Activate_Impl( void );
	void	StartFire( void );
	void	Start();
	void	SetToOutSize()
	{
		UTIL_SetSize( this, Vector(-8,-8,0), Vector(8,8,8) );
	}

	float	GetHeatLevel()	{ return m_flHeatLevel; }

	int UpdateTransmitState_Impl();

	void DrawDebugGeometryOverlays_Impl(void) 
	{
		if (m_debugOverlays & OVERLAY_BBOX_BIT) 
		{	
			if ( m_lastDamage > gpGlobals->curtime && m_flHeatAbsorb > 0 )
			{
				NDebugOverlay::EntityBounds(this, 88, 255, 128, 0 ,0);
				char tempstr[512];
				V_snprintf( tempstr, sizeof(tempstr), "Heat: %.1f", m_flHeatAbsorb );
				EntityText(1,tempstr, 0);
			}
			else if ( !IsBurning() )
			{
				NDebugOverlay::EntityBounds(this, 88, 88, 128, 0 ,0);
			}

			if ( IsBurning() )
			{
				Vector mins, maxs;
				if ( GetFireDimensions( &mins, &maxs ) )
				{
					NDebugOverlay::Box(GetAbsOrigin(), mins, maxs, 128, 0, 0, 10, 0);
				}
			}

		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	void Disable();

	//Inputs
	[[= ks::reflect::Input{ .name = "StartFire", .type = FIELD_VOID } ]] void	InputStartFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Extinguish", .type = FIELD_FLOAT } ]] void	InputExtinguish( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ExtinguishTemporary", .type = FIELD_FLOAT } ]] void	InputExtinguishTemporary( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

protected:
	
	void	Spread( void );
	void	SpawnEffect( fireType_e type, float scale );

	CHandle<CBaseFire>	m_hEffect;
	EHANDLE		m_hOwner;
	
	[[= ks::reflect::Key{ .name = "firetype" } ]] int		m_nFireType;

	float	m_flFuel;
	float	m_flDamageTime;
	float	m_lastDamage;
	[[= ks::reflect::Key{ .name = "firesize" } ]] float	m_flFireSize;	// size of the fire in world units
	float	m_flLastNavUpdateTime;	// last time we told the nav mesh about ourselves

	[[= ks::reflect::Key{ .name = "ignitionpoint" } ]] float	m_flHeatLevel;	// Used as a "health" for the fire.  > 0 means the fire is burning
	float	m_flHeatAbsorb;	// This much heat must be "absorbed" before it gets transferred to the flame size
	[[= ks::reflect::Key{ .name = "damagescale" } ]] float	m_flDamageScale;

	float	m_flMaxHeat;
	float	m_flLastHeatLevel;

	//NOTENOTE: Lifetime is an expression of the sum total of these amounts plus the global time when started
	[[= ks::reflect::Key{ .name = "fireattack" } ]] float	m_flAttackTime;	//Amount of time to scale up

	bool	m_bEnabled;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bStartDisabled;
	bool	m_bDidActivate;

	[[= ks::reflect::Key{ .name = "OnIgnited" } ]] COutputEvent	m_OnIgnited;
	[[= ks::reflect::Key{ .name = "OnExtinguished" } ]] COutputEvent	m_OnExtinguished;

	DECLARE_DATADESC();
};

class CFireSphere : public IPartitionEnumerator
{
public:
	CFireSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius );
	// This gets called	by the enumeration methods with each element
	// that passes the test.
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
	
	int GetCount() { return m_count; }
	bool AddToList( CFire *pEntity );

private:
	Vector			m_origin;
	float			m_radiusSqr;
	CFire			**m_pList;
	int				m_listMax;
	int				m_count;
	bool			m_onlyActiveFires;
};

CFireSphere::CFireSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius )
{
	m_pList = pList;
	m_listMax = listMax;
	m_count = 0;
	m_onlyActiveFires = onlyActiveFires;
	m_origin = origin;
	m_radiusSqr = radius * radius;
}

bool CFireSphere::AddToList( CFire *pFire )
{
	if ( m_count >= m_listMax )
		return false;
	m_pList[m_count] = pFire;
	m_count++;
	return true;
}

IterationRetval_t CFireSphere::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
	if ( pEntity )
	{
		// UNDONE: Measure which of these is faster
//		CFire *pFire = dynamic_cast<CFire *>(pEntity);
		if ( !FClassnameIs( pEntity, "env_fire" ) )
			return ITERATION_CONTINUE;

		CFire *pFire = static_cast<CFire *>(pEntity);
		if ( pFire )
		{
			if ( !m_onlyActiveFires || pFire->IsBurning() )
			{
				if ( (m_origin - pFire->GetAbsOrigin()).LengthSqr() < m_radiusSqr )
				{
					if ( !AddToList( pFire ) )
						return ITERATION_STOP;
				}
			}
		}
	}

	return ITERATION_CONTINUE;
}

int FireSystem_GetFiresInSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius )
{
	CFireSphere sphereEnum( pList, listMax, onlyActiveFires, origin, radius );
	::partition->EnumerateElementsInSphere( PARTITION_ENGINE_NON_STATIC_EDICTS, origin, radius, false, &sphereEnum );

	return sphereEnum.GetCount();
}

bool FireSystem_IsValidFirePosition( const Vector &position, float testRadius )
{
	CFire *pList[1];
	int count = FireSystem_GetFiresInSphere( pList, std::size( pList ), true, position, testRadius );
	if ( count > 0 )
		return false;
	return true;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool FireSystem_IsFireInWall( Vector &position, fireType_e type )
{
	// Don't check natural fire against walls
	if (type == FIRE_NATURAL)
		return false;

	trace_t tr;
	UTIL_TraceHull( position, position+Vector(0,0,0.1), FIRE_MINS,FIRE_MAXS,MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );
	if (tr.fraction != 1.0 || tr.startsolid)
	{
		//NDebugOverlay::Box(position,FIRE_MINS,FIRE_MAXS,255,0,0,50,10);
		return true;
	}
	//NDebugOverlay::Box(position,FIRE_MINS,FIRE_MAXS,0,255,0,50,10);

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether or not a new fire may be placed at a given location
// Input  : &position - where we are trying to put the new fire
//			separationRadius - the maximum distance fires must be apart from one another
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FireSystem_CanAddFire( Vector *position, float separationRadius, fireType_e type, int flags )
{
	//See if we found a fire inside the sphere
	if ( !FireSystem_IsValidFirePosition( *position, separationRadius ) )
		return false;

	// Unless our fire is floating, make sure were not too high
	if (!(flags & SF_FIRE_DONT_DROP))
	{
		trace_t	tr;
		Vector	startpos = *position;
		Vector	endpos = *position;

		startpos[2] += 1;
		endpos[2] -= FIRE_MAX_GROUND_OFFSET;

		UTIL_TraceLine( startpos, endpos, MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );

		//See if we're floating too high 
		if ( ( tr.allsolid ) || ( tr.startsolid) || ( tr.fraction == 1.0f ) )
		{
			return false;
		}

		//TODO: If we've hit an entity here, start it on fire
		CBaseEntity *pEntity = tr.Ent<CBaseEntity>();

		if ( ENTINDEX( pEntity->edict() ) != 0 )
		{
			return false;
		}
	}

	// Check if fire is in a wall, if so try shifting around a bit
	if (FireSystem_IsFireInWall( *position, type ))
	{
		Vector vTestPos = *position;
		vTestPos.x		+= 10;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.y		+= 10;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.y		-= 20;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.x		-= 20;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		return false;
	}

	//Able to add here
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Starts a fire at a specified location
// Input  : &position - position to start the fire at
//			flags - any special modifiers
//-----------------------------------------------------------------------------
bool FireSystem_StartFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type )
{
	CVProfScope _( "FireSystem_StartFire1", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	Vector testPos = position;
	//Must be okay to add fire here
	if ( FireSystem_CanAddFire( &testPos, 16.0f, type, flags ) == false )
	{
		CFire *pFires[16];
		int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), true, position, 16.0f );
		for ( int i = 0; i < fireCount; i++ )
		{
			// add to this fire
			pFires[i]->AddHeat( fireHeight, false );
		}

		return false;
	}

	//Create a new fire entity
	CFire *fire = (CFire *) CreateEntityByName( "env_fire" );
	
	if ( fire == nullptr )
		return false;

	//Spawn the fire
	// Fires not placed by a designer should be cleaned up automatically (not catch fire again)
	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->Spawn();
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );
	fire->Start();
	fire->SetOwner( owner );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Starts a fire on a specified model.
// Input  : pEntity - The model entity to catch on fire.
//			fireHeight - 
//			attack - 
//			fuel - 
//			flags - 
//			owner - 
//			type - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FireSystem_StartFire( CBaseAnimating *pEntity, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type )
{
	CVProfScope _( "FireSystem_StartFire2", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	Vector position = pEntity->GetAbsOrigin();
	Vector testPos = position;

	// Make sure its a valid position for fire (not in a wall, etc)
	if ( FireSystem_CanAddFire( &testPos, 16.0f, type, flags ) == false )
	{
		// Contribute heat to all fires within 16 units of this fire.
		CFire *pFires[16];
		int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), true, position, 16.0f );
		for ( int i = 0; i < fireCount; i++ )
		{
			pFires[i]->AddHeat( fireHeight, false );
		}

		return false;
	}

	// Create a new fire entity
	CFire *fire = (CFire *) CreateEntityByName( "env_fire" );
	if ( fire == nullptr )
	{
		return false;
	}

	// Spawn the fire.
	// Fires not placed by a designer should be cleaned up automatically (not catch fire again).
	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->Spawn();
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );
	fire->Start();
	fire->SetOwner( owner );

	return true;
}

void FireSystem_ExtinguishInRadius( const Vector &origin, float radius, float rate )
{
	// UNDONE: pass this instead of percent
	float heat = (1-rate) * fire_extscale.GetFloat();

	CFire *pFires[32];
	int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), false, origin, radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->Extinguish( heat );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			radius - 
//			heat - 
//-----------------------------------------------------------------------------
void FireSystem_AddHeatInRadius( const Vector &origin, float radius, float heat )
{
	CVProfScope _( "FireSystem_AddHeatInRadius", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CFire *pFires[32];

	int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), false, origin, radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->AddHeat( heat );
	}
}

//-----------------------------------------------------------------------------

bool FireSystem_GetFireDamageDimensions( CBaseEntity *pEntity, Vector *pFireMins, Vector *pFireMaxs )
{
	CFire *pFire = dynamic_cast<CFire *>(pEntity);

	if ( pFire && pFire->GetFireDimensions( pFireMins, pFireMaxs ) )
	{
		*pFireMins /= FIRE_SPREAD_DAMAGE_MULTIPLIER;
		*pFireMaxs /= FIRE_SPREAD_DAMAGE_MULTIPLIER;
		return true;
	}
	pFireMins->Init();
	pFireMaxs->Init();
	return false;
}

//==================================================
// CFire
//==================================================
IMPLEMENT_REFLECT_DATAMAP( CFire )

LINK_ENTITY_TO_CLASS( env_fire, CFire );

//==================================================
// CFire
//==================================================

CFire::CFire( void )
{
	m_flFuel = 0.0f;
	m_flAttackTime = 0.0f;
	m_flDamageTime = 0.0f;
	m_lastDamage = 0;
	m_nFireType = FIRE_NATURAL;

	//Spreading
	m_flHeatAbsorb = 8.0f;
	m_flHeatLevel = 0;

	// Must be in the constructor!
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
}

//-----------------------------------------------------------------------------
// UpdateOnRemove
//-----------------------------------------------------------------------------
void CFire::UpdateOnRemove_Impl( void )
{
	//Stop any looping sounds that might be playing
	StopSound( "Fire.Plasma" );

	DestroyEffect();

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Precache_Impl( void )
{
	if ( m_nFireType == FIRE_NATURAL )
	{
		UTIL_PrecacheOther("_firesmoke");
		
		if ( m_spawnflags & SF_FIRE_SMOKELESS )
		{
			PrecacheParticleSystem( "env_fire_tiny" );
			PrecacheParticleSystem( "env_fire_small" );
			PrecacheParticleSystem( "env_fire_medium" );
			PrecacheParticleSystem( "env_fire_large" );
		}
		else
		{
			PrecacheParticleSystem( "env_fire_tiny_smoke" );
			PrecacheParticleSystem( "env_fire_small_smoke" );
			PrecacheParticleSystem( "env_fire_medium_smoke" );
			PrecacheParticleSystem( "env_fire_large_smoke" );
		}
	}

	if ( m_nFireType == FIRE_PLASMA )
	{
		UTIL_PrecacheOther("_plasma");
	}

	PrecacheScriptSound( "Fire.Plasma" );
}

//------------------------------------------------------------------------------
// Purpose : Input handler for starting the fire.
//------------------------------------------------------------------------------
void CFire::InputStartFire( inputdata_t &inputdata )
{
	if ( !m_bEnabled )
		return;

	StartFire();
}

void CFire::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CFire::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CFire::Disable() 
{
	m_bEnabled = false;
	if ( IsBurning() )
	{
		GoOut();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFire::InputExtinguish( inputdata_t &inputdata )
{
	m_spawnflags &= ~SF_FIRE_INFINITE;
	GoOutInSeconds( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFire::InputExtinguishTemporary( inputdata_t &inputdata )
{
	GoOutInSeconds( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Starts burning.
//-----------------------------------------------------------------------------
void CFire::StartFire( void )
{
	if ( m_hEffect != nullptr )
		return;

	// Trace down and start a fire there. Nothing fancy yet.
	Vector vFirePos;
	trace_t tr;
	if ( m_spawnflags & SF_FIRE_DONT_DROP )
	{
		vFirePos = GetAbsOrigin();
	}
	else
	{
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 1024 ), MASK_FIRE_SOLID, this, COLLISION_GROUP_NONE, &tr );
		vFirePos = tr.endpos;
	}

	int spawnflags = m_spawnflags;
	m_spawnflags |= SF_FIRE_START_ON;
	Init( vFirePos, m_flFireSize, m_flAttackTime, GetHealth(), m_spawnflags, (fireType_e) m_nFireType );
	Start();
	m_spawnflags = spawnflags;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	Precache();

	m_takedamage = DAMAGE_NO;

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetToOutSize();

	// set up the ignition point
	m_flHeatAbsorb = m_flHeatLevel * 0.05;
	m_flHeatLevel = 0;
	Init( GetAbsOrigin(), m_flFireSize, m_flAttackTime, m_flFuel, m_spawnflags, m_nFireType );
	
	if( m_bStartDisabled )
	{
		Disable();
	}
	else
	{
		m_bEnabled = true;
	}
}

int CFire::UpdateTransmitState_Impl()
{
	// Don't want to be FL_EDICT_DONTSEND because our fire entity may make us transmit.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	
	//See if we should start active
	if ( !m_bDidActivate && ( m_spawnflags & SF_FIRE_START_ON ) )
	{
		m_flHeatLevel = m_flMaxHeat;

		StartFire();
	}

	m_bDidActivate = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::SpawnEffect( fireType_e type, float scale )
{
	CBaseFire *pEffect = nullptr;
	switch ( type )
	{
	default:
	case FIRE_NATURAL:
		{
			CFireSmoke	*fireSmoke = (CFireSmoke *) CreateEntityByName( "_firesmoke" );
			fireSmoke->EnableSmoke( ( m_spawnflags & SF_FIRE_SMOKELESS )==false );
			fireSmoke->EnableGlow( ( m_spawnflags & SF_FIRE_NO_GLOW )==false );
			fireSmoke->EnableVisibleFromAbove( ( m_spawnflags & SF_FIRE_VISIBLE_FROM_ABOVE )!=false );
			
			pEffect			= fireSmoke;
			m_nFireType		= FIRE_NATURAL;
			m_takedamage	= DAMAGE_YES;
		}
		break;

	case FIRE_PLASMA:
		{
			CPlasma	*plasma = (CPlasma *) CreateEntityByName( "_plasma" );
			plasma->EnableSmoke( true );
		
			pEffect			= plasma;
			m_nFireType		= FIRE_PLASMA;
			m_takedamage	= DAMAGE_YES;

			// Start burn sound
			EmitSound( "Fire.Plasma" );
		}
		break;
	}

	UTIL_SetOrigin( pEffect, GetAbsOrigin() );
	pEffect->Spawn();
	pEffect->SetParent( this );
	pEffect->Scale( m_flFireSize, m_flFireSize, 0 );
	//Start it going
	pEffect->Enable( ( m_spawnflags & SF_FIRE_START_ON ) );
	m_hEffect = pEffect;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn and initialize the fire
// Input  : &position - where the fire resides
//			lifetime - 
//-----------------------------------------------------------------------------
void CFire::Init( const Vector &position, float scale, float attackTime, float fuel, int flags, int fireType )
{
	m_flAttackTime = attackTime;
	
	m_spawnflags = flags;
	m_nFireType = fireType;

	if ( flags & SF_FIRE_INFINITE )
	{
		fuel = 0;
	}
	m_flFuel = fuel;
	if ( m_flFuel )
	{
		m_spawnflags |= SF_FIRE_DIE_PERMANENT;
	}

	Vector localOrigin = position;
	if ( GetMoveParent() )
	{
		EntityMatrix parentMatrix;
		parentMatrix.InitFromEntity( GetMoveParent() );
		localOrigin = parentMatrix.WorldToLocal( position );
	}
	UTIL_SetOrigin( this, localOrigin );

	SetSolid( SOLID_NONE );
	m_flFireSize = scale;
	m_flMaxHeat = FIRE_MAX_HEAT_LEVEL * FIRE_SCALE_FROM_SIZE(scale);
	//See if we should start on
	if ( m_spawnflags & SF_FIRE_START_FULL )
	{
		m_flHeatLevel = m_flMaxHeat;
	}
	m_flLastHeatLevel = 0;

}

void CFire::Start()
{
	float boxWidth = (m_flFireSize * (FIRE_WIDTH/FIRE_HEIGHT))*0.5f;
	UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,0),Vector(boxWidth,boxWidth,m_flFireSize));

	//Spawn the client-side effect
	SpawnEffect( (fireType_e)m_nFireType, FIRE_SCALE_FROM_SIZE(m_flFireSize) );
	m_OnIgnited.FireOutput( this, this );
	SetThink( &CFire::BurnThink );
	m_flLastNavUpdateTime = 0.0f;
	m_flDamageTime = 0;
	// think right now
	BurnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether or not the fire is still active
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFire::IsBurning( void ) const
{
	if ( m_flHeatLevel > 0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage box of the fire
//-----------------------------------------------------------------------------
bool CFire::GetFireDimensions( Vector *pFireMins, Vector *pFireMaxs )
{
	if ( m_flHeatLevel <= 0 )
	{
		pFireMins->Init();
		pFireMaxs->Init();
		return false;
	}

	float scale = m_flHeatLevel / m_flMaxHeat;
	float damageRadius = scale * m_flFireSize * FIRE_WIDTH / FIRE_HEIGHT * 0.5;	

	damageRadius *= FIRE_SPREAD_DAMAGE_MULTIPLIER; //FIXME: Trying slightly larger radius for burning

	if ( damageRadius < 16 )
	{
		damageRadius = 16;
	}

	pFireMins->Init(-damageRadius,-damageRadius,0);
	pFireMaxs->Init(damageRadius,damageRadius,m_flFireSize*scale);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update the fire and its children
//-----------------------------------------------------------------------------
void CFire::Update( float simTime )
{
	CVProfScope _( "CFire::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( m_flFuel != 0 )
	{
		m_flFuel -= simTime;
		if ( m_flFuel <= 0 )
		{
			GoOutInSeconds( 1 );
			return;
		}
	}

	float strength = m_flHeatLevel / FIRE_MAX_HEAT_LEVEL;
	if ( m_flHeatLevel != m_flLastHeatLevel )
	{
		m_flLastHeatLevel = m_flHeatLevel;
		// Make the effect the appropriate size given the heat level
		m_hEffect->Scale( strength, 0.5f );		
	}
	// add heat to myself (grow)
	float addedHeat = (m_flAttackTime > 0) ? m_flMaxHeat / m_flAttackTime : m_flMaxHeat;
	addedHeat *= simTime * fire_growthrate.GetFloat();
	AddHeat( addedHeat, true );

	// add heat to nearby fires
	float outputHeat = strength * m_flHeatLevel;

	Vector fireMins;
	Vector fireMaxs;
	Vector fireEntityDamageMins;
	Vector fireEntityDamageMaxs;

	GetFireDimensions( &fireMins, &fireMaxs );

	if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 ) // if set to 1.0, optimizer will remove this code
	{
		fireEntityDamageMins = fireMins / FIRE_SPREAD_DAMAGE_MULTIPLIER;
		fireEntityDamageMaxs = fireMaxs / FIRE_SPREAD_DAMAGE_MULTIPLIER;
	}

	//NDebugOverlay::Box( GetAbsOrigin(), fireMins, fireMaxs, 255, 255, 255, 0, fire_dmginterval.GetFloat() );
	fireMins += GetAbsOrigin();
	fireMaxs += GetAbsOrigin();

	if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 )
	{
		fireEntityDamageMins += GetAbsOrigin();
		fireEntityDamageMaxs += GetAbsOrigin();
	}

	CBaseEntity *pNearby[256];
	CFire *pFires[16];
	int nearbyCount = UTIL_EntitiesInBox( pNearby, std::size( pNearby ), fireMins, fireMaxs, 0 );
	int fireCount = 0;
	int i;

	// is it time to do damage?
	bool damage = false;
	int outputDamage = 0;
	if ( m_flDamageTime <= gpGlobals->curtime )
	{
		m_flDamageTime = gpGlobals->curtime + fire_dmginterval.GetFloat();
		outputDamage = (fire_dmgbase.GetFloat() + outputHeat * fire_dmgscale.GetFloat() * m_flDamageScale) * fire_dmginterval.GetFloat();
		if ( outputDamage )
		{
			damage = true;
		}
	}
	int damageFlags = (m_nFireType == FIRE_NATURAL) ? DMG_BURN : DMG_PLASMA;
	for ( i = 0; i < nearbyCount; i++ )
	{
		CBaseEntity *pOther = pNearby[i];

		if ( pOther == this )
			continue;
		
		if ( FClassnameIs( pOther, "env_fire" ) )
		{
			if ( fireCount < std::size( pFires ) )
			{
				pFires[fireCount] = (CFire *)pOther;
				fireCount++;
			}
			continue;
		}

		if ( pOther->m_takedamage == DAMAGE_NO )
			continue;

		if ( !damage )
			continue;

		if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 || ( pOther->CollisionProp()->GetSurroundingBoundsType() == USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS ) )
		{
			// Do a tighter bounds for players and 
			Vector otherMins, otherMaxs;
			pOther->CollisionProp()->WorldSpaceAABB( &otherMins, &otherMaxs );
			bool bDoDamage = IsBoxIntersectingBox( otherMins, otherMaxs, fireEntityDamageMins, fireEntityDamageMaxs );
			if ( !bDoDamage )
				continue;
		}

		// Make sure can actually see entity (don't damage through walls)
		trace_t tr;
		UTIL_TraceLine( this->WorldSpaceCenter(), pOther->WorldSpaceCenter(), MASK_FIRE_SOLID, pOther, COLLISION_GROUP_NONE, &tr );

		if (tr.fraction == 1.0 && !tr.startsolid)
		{
			pOther->TakeDamage( CTakeDamageInfo( this, this, outputDamage, damageFlags ) );
		}
	}

	outputHeat *= fire_heatscale.GetFloat() * simTime;

	if ( fireCount > 0 )
	{
		outputHeat /= fireCount;
		for ( i = 0; i < fireCount; i++ )
		{
			pFires[i]->AddHeat( outputHeat, false );
		}
	}
}

// Destroy any effect I have
void CFire::DestroyEffect()
{
	CBaseFire *pEffect = m_hEffect;
	if ( pEffect != nullptr )
	{
		//disable the graphics and remove the entity
		pEffect->Enable( false );
		UTIL_Remove( pEffect );
	}
}
//-----------------------------------------------------------------------------
// Purpose: Think
//-----------------------------------------------------------------------------
void CFire::BurnThink( void )
{
	SetNextThink( gpGlobals->curtime + FIRE_THINK_INTERVAL );

	Update( FIRE_THINK_INTERVAL );

}

void CFire::GoOutThink()
{
	GoOut();
}

void CFire::GoOutInSeconds( float seconds )
{
	Scale( 0.0f, seconds );
	
	SetThink( &CFire::GoOutThink );
	SetNextThink( gpGlobals->curtime + seconds );
}

//------------------------------------------------------------------------------
// Purpose : Blasts of significant size blow out fires that take damage
// Input   :
// Output  :
//------------------------------------------------------------------------------
int CFire::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	return 0;
}

void CFire::AddHeat( float heat, bool selfHeat )
{
	if ( m_bEnabled )
	{
		if ( !selfHeat )
		{
			if ( IsBurning() )
			{
				// scale back the incoming heat from surrounding fires
				// if I've already ignited
				heat *= fire_incomingheatscale.GetFloat();
			}
		}
		m_lastDamage = gpGlobals->curtime + 0.5;
		bool start = m_flHeatLevel <= 0 ? true : false;
		if ( m_flHeatAbsorb > 0 )
		{
			float absorbDamage = heat * fire_absorbrate.GetFloat();
			if ( absorbDamage > m_flHeatAbsorb )
			{
				heat -= m_flHeatAbsorb / fire_absorbrate.GetFloat();
				m_flHeatAbsorb = 0;
			}
			else
			{
				m_flHeatAbsorb -= absorbDamage;
				heat = 0;
			}
		}

		m_flHeatLevel += heat;
		if ( start && m_flHeatLevel > 0 && m_hEffect == nullptr )
		{
			StartFire();
		}
		if ( m_flHeatLevel > m_flMaxHeat )
			m_flHeatLevel = m_flMaxHeat;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : end - 
//			time - 
//-----------------------------------------------------------------------------
void CFire::Scale( float end, float time )
{
	CBaseFire *pEffect = m_hEffect;
	if ( pEffect )
	{
		pEffect->Scale( end, time );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//-----------------------------------------------------------------------------
void CFire::Extinguish( float heat )
{
	if ( !m_bEnabled )
		return;

	m_lastDamage = gpGlobals->curtime + 0.5;
	bool out = m_flHeatLevel > 0 ? true : false;

	m_flHeatLevel -= heat;
	m_flHeatAbsorb += fire_extabsorb.GetFloat() * heat;
	if ( m_flHeatAbsorb > fire_maxabsorb.GetFloat() )
	{
		m_flHeatAbsorb = fire_maxabsorb.GetFloat();
	}

	// drift toward the average attack time after being sprayed
	// some fires are heavily scripted so their attack looks weird 
	// once interacted with.  Basically, this blends out the scripting 
	// as the fire is sprayed with the extinguisher.
	float averageAttackTime = m_flMaxHeat * (FIRE_NORMAL_ATTACK_TIME/FIRE_MAX_HEAT_LEVEL);
	m_flAttackTime = Approach( averageAttackTime, m_flAttackTime, 2 * gpGlobals->frametime );

	if ( m_flHeatLevel <= 0 )
	{
		m_flHeatLevel = 0;
		if ( out )
		{
			GoOut();
		}
	}
}

bool CFire::GoOut()
{
	//Signal death
	m_OnExtinguished.FireOutput( this, this );

	DestroyEffect();
	m_flHeatLevel -= 20;
	if ( m_flHeatLevel > 0 )
		m_flHeatLevel = 0;

	m_flLastHeatLevel = m_flHeatLevel; 
	SetThink(nullptr);
	SetNextThink( TICK_NEVER_THINK );
	if ( m_spawnflags & SF_FIRE_DIE_PERMANENT )
	{
		UTIL_Remove( this );
		return true;
	}
	SetToOutSize();
	
	return false;
}

//==================================================
// CEnvFireSource is a source of heat that the player 
// cannot put out
//==================================================

static constexpr auto FIRESOURCE_THINK_TIME = 0.25; // seconds to

static constexpr auto SF_FIRESOURCE_START_ON = 0x0001;

class CEnvFireSource : public CBaseEntity
{
	DECLARE_CLASS( CEnvFireSource, CBaseEntity );
public:
	void Spawn_Impl();
	void Think_Impl();
	void TurnOn();
	void TurnOff();
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	bool		m_bEnabled;
	[[= ks::reflect::Key{ .name = "fireradius" } ]] float		m_radius;
	[[= ks::reflect::Key{ .name = "firedamage" } ]] float		m_damage;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvFireSource )

LINK_ENTITY_TO_CLASS( env_firesource, CEnvFireSource );

void CEnvFireSource::Spawn_Impl()
{
	if ( m_spawnflags & SF_FIRESOURCE_START_ON )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CEnvFireSource::Think_Impl()
{
	if ( !m_bEnabled )
		return;
	SetNextThink( gpGlobals->curtime + FIRESOURCE_THINK_TIME );

	CFire *pFires[128];
	int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), false, GetAbsOrigin(), m_radius );

	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->AddHeat( m_damage * FIRESOURCE_THINK_TIME );
	}
}

void CEnvFireSource::TurnOn()
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	SetNextThink( gpGlobals->curtime );
}

void CEnvFireSource::TurnOff()
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;
	SetNextThink( TICK_NEVER_THINK );
}
void CEnvFireSource::InputEnable( inputdata_t &inputdata )
{
	TurnOn();
}
void CEnvFireSource::InputDisable( inputdata_t &inputdata )
{
	TurnOff();
}

//==================================================
// CEnvFireSensor detects changes in heat
//==================================================
static constexpr auto SF_FIRESENSOR_START_ON = 1;

class CEnvFireSensor : public CBaseEntity
{
	DECLARE_CLASS( CEnvFireSensor, CBaseEntity );
public:
	void Spawn_Impl();
	void Think_Impl();
	void TurnOn();
	void TurnOff();
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	bool			m_bEnabled;
	bool			m_bHeatAtLevel;
	[[= ks::reflect::Key{ .name = "fireradius" } ]] float			m_radius;
	[[= ks::reflect::Key{ .name = "heatlevel" } ]] float			m_targetLevel;
	[[= ks::reflect::Key{ .name = "heattime" } ]] float			m_targetTime;
	float			m_levelTime;

	[[= ks::reflect::Key{ .name = "OnHeatLevelStart" } ]] COutputEvent	m_OnHeatLevelStart;
	[[= ks::reflect::Key{ .name = "OnHeatLevelEnd" } ]] COutputEvent	m_OnHeatLevelEnd;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvFireSensor )

LINK_ENTITY_TO_CLASS( env_firesensor, CEnvFireSensor );

void CEnvFireSensor::Spawn_Impl()
{
	if ( m_spawnflags & SF_FIRESENSOR_START_ON )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CEnvFireSensor::Think_Impl()
{
	if ( !m_bEnabled )
		return;

	float time = m_targetTime * 0.25;
	if ( time < 0.1 )
	{
		time = 0.1;
	}
	SetNextThink( gpGlobals->curtime + time );

	float heat = 0;
	CFire *pFires[128];
	int fireCount = FireSystem_GetFiresInSphere( pFires, std::size( pFires ), true, GetAbsOrigin(), m_radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		heat += pFires[i]->GetHeatLevel();
	}

	if ( heat >= m_targetLevel )
	{
		m_levelTime += time;
		if ( m_levelTime >= m_targetTime )
		{
			if ( !m_bHeatAtLevel )
			{
				m_bHeatAtLevel = true;
				m_OnHeatLevelStart.FireOutput( this, this );
			}
		}
	}
	else
	{
		m_levelTime = 0;
		if ( m_bHeatAtLevel )
		{
			m_bHeatAtLevel = false;
			m_OnHeatLevelEnd.FireOutput( this, this );
		}
	}
}

void CEnvFireSensor::TurnOn()
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	SetNextThink( gpGlobals->curtime );
	m_bHeatAtLevel = false;
	m_levelTime = 0;
}

void CEnvFireSensor::TurnOff()
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;
	SetNextThink( TICK_NEVER_THINK );
	if ( m_bHeatAtLevel )
	{
		m_bHeatAtLevel = false;
		m_OnHeatLevelEnd.FireOutput( this, this );
	}

}
void CEnvFireSensor::InputEnable( inputdata_t &inputdata )
{
	TurnOn();
}
void CEnvFireSensor::InputDisable( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFire::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print flame size
		V_snprintf(tempstr,sizeof(tempstr),"    size: %f", m_flFireSize);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

#undef FIRE_MINS
#undef FIRE_MAXS

using namespace ::ks::reflect;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

//==================================================
// CBaseFire
//==================================================

CBaseFire::CBaseFire( void )
{
	m_flStartScale		= 0.0f;
	m_flScale			= 0.0f;
	m_flScaleTime		= 0.0f;
	m_nFlags			= bitsFIRE_NONE;
}

CBaseFire::~CBaseFire( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Take the current scale of the flame and move it towards a destination
// Input  : size - destination size
//			time - time to scale across
//-----------------------------------------------------------------------------
void CBaseFire::Scale( float size, float time )
{
	//Send to the client
	m_flScale		= size;
	m_flScaleTime	= time;
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded Scale() function to set size
// Input  : start - beginning sizek
//			size - destination size
//			time - time to scale across
//-----------------------------------------------------------------------------
void CBaseFire::Scale( float start, float size, float time )
{
	//Send to the client
	m_flStartScale	= start;
	m_flScale		= size;
	m_flScaleTime	= time;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseFire::Enable( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRE_ACTIVE;
	}
	else
	{
		m_nFlags &= ~bitsFIRE_ACTIVE;
	}
}

//==================================================
// CFireSmoke
//==================================================

//Link the entity
LINK_ENTITY_TO_CLASS( _firesmoke, CFireSmoke );

//Send datatable
IMPLEMENT_REFLECT_SERVERCLASS( CFireSmoke, DT_FireSmoke )

//Data description 

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CFireSmoke::CFireSmoke( void ) 
{
	//Client-side
	m_flScale			= 0.0f;
	m_flScaleTime		= 0.0f;
	m_nFlags			= bitsFIRE_NONE;

	//Server-side
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFireSmoke::~CFireSmoke( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::Precache_Impl()
{
	BaseClass::Precache_Impl();
}

void CFireSmoke::Spawn_Impl()
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CFireSmoke::EnableSmoke( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_SMOKE;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_SMOKE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CFireSmoke::EnableGlow( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_GLOW;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_GLOW;
	}
}

void CFireSmoke::EnableVisibleFromAbove( int state )
{
	if ( state )
	{
		m_nFlags |= bitsFIRESMOKE_VISIBLE_FROM_ABOVE;
	}
	else
	{
		m_nFlags &= ~bitsFIRESMOKE_VISIBLE_FROM_ABOVE;
	}
}

KS_ENTITY_ABSTRACT_KIND( CBaseFire )

class CBaseEntity;
class CBasePlayer;
class CFishPool;
class CTakeDamageInfo;

using namespace ::ks::reflect;

// fish.cpp
// Simple fish behavior
// Author: Michael S. Booth, April 2005

ConVar fish_dormant( side_registry(), "fish_dormant", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Turns off interactive fish behavior. Fish become immobile and unresponsive." );

//-----------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( fish, CFish );

//-----------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------
/**
 * Send fish position relative to pool origin
 */
void SendProxy_FishOriginX( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CFish *fish = (CFish *)pStruct;
	contract_assert( fish );

	const Vector &v = fish->GetAbsOrigin();
	Vector origin = fish->m_poolOrigin;

	pOut->m_Float = v.x - origin.x;
}

void SendProxy_FishOriginY( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CFish *fish = (CFish *)pStruct;
	contract_assert( fish );

	const Vector &v = fish->GetAbsOrigin();
	Vector origin = fish->m_poolOrigin;

	pOut->m_Float = v.y - origin.y;
}

// keep angle in normalized range when sending it
void SendProxy_FishAngle( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	float value = *((float *)pData);

	while( value > 360.0f )
		value -= 360.0f;

	while (value < 0.0f)
		value += 360.0f;

	pOut->m_Float = value;
}

/**
 * NOTE: Do NOT use SPROP_CHANGES_OFTEN, as it will reorder this list.
 * The pool origin must arrive befoore m_x and m_y or the fish will
 * respawn at the origin and zip back to their proper places.
 */
IMPLEMENT_REFLECT_SERVERCLASS( CFish, DT_CFish )

//-------------------------------------------------------------------------------------------------------------
CFish::CFish( void )
{
}

//-------------------------------------------------------------------------------------------------------------
CFish::~CFish()
{
}

//-------------------------------------------------------------------------------------------------------------
void CFish::Initialize( CFishPool *pool, unsigned int id )
{
	m_pool = pool;
	m_id = id;

	m_poolOrigin = pool->GetAbsOrigin();
	m_waterLevel = pool->GetWaterLevel();

	// pass relative position to the client
	Vector deltaPos = GetAbsOrigin() - m_poolOrigin;
	m_x = deltaPos.x;
	m_y = deltaPos.y;
	m_z = m_poolOrigin->z;

	SetModel( pool->GetModelName().ToCStr() );
}

//-------------------------------------------------------------------------------------------------------------
void CFish::Spawn_Impl( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_NOT_SOLID | FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_FLY );

	m_angle = RandomFloat( 0.0f, 360.0f );
	m_angleChange = 0.0f;

	m_forward = Vector( 1.0f, 0.0, 0.0f );
	m_perp.x = -m_forward.y;
	m_perp.y = m_forward.x;
	m_perp.z = 0.0f;

	m_speed = 0.0f;
	m_calmSpeed = RandomFloat( 10.0f, 20.0f );
	m_panicSpeed = m_calmSpeed * RandomFloat( 4.0f, 5.0f );
	m_desiredSpeed = m_calmSpeed;

	m_turnClockwise = (RandomInt( 0, 100 ) < 50);

	m_avoidRange = RandomFloat( 40.0f, 75.0f );

	m_iHealth = 1;
	m_iMaxHealth = 1;
	m_takedamage = DAMAGE_YES;

	// spread out a bit
	m_disperseTimer.Start( RandomFloat( 0.0f, 10.0f ) );
	m_goTimer.Start( RandomFloat( 10.0f, 60.0f ) );
	m_moveTimer.Start( RandomFloat( 2.0f, 10.0 ) );
	m_desiredSpeed = m_calmSpeed;
}

//-------------------------------------------------------------------------------------------------------------
void CFish::Event_Killed_Impl( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;
	m_lifeState = LIFE_DEAD;
}

//-------------------------------------------------------------------------------------------------------------
/**
 * In contact with "other"
 */
void CFish::Touch_Impl( CBaseEntity *other )
{
	if (other && other->IsPlayer())
	{
		// touched a Player - panic!
		Panic();
	}
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Influence my motion to flock with other nearby fish
 * 'amount' ranges from zero to one, representing the amount of flocking influence allowed
 * If 'other' is NULL, flock to the center of the pool.
 */
void CFish::FlockTo( CFish *other, float amount )
{
	// allow fish to disperse a bit at round start
	if (!m_disperseTimer.IsElapsed())
		return;

	const float maxRange = (other) ? 100.0f : 300.0f;

	Vector to = (other) ? (other->GetAbsOrigin() - GetAbsOrigin()) : (m_pool->GetAbsOrigin() - GetAbsOrigin());
	float range = to.NormalizeInPlace();

	if (range > maxRange)
		return;

	// if they are close and we are moving together, avoid them
	const float avoidRange = 25.0f;
	if (other && range < avoidRange)
	{
		// compute their relative velocity to us
		Vector relVel = other->GetAbsVelocity() - GetAbsVelocity();

		if (DotProduct( to, relVel ) < 0.0f)
		{
			const float avoidPower = 5.0f;

			// their comin' right at us! - avoid
			if (DotProduct( m_perp, to ) > 0.0f)
			{
				m_angleChange -= avoidPower * (1.0f - range/avoidRange);
			}
			else
			{
				m_angleChange += avoidPower * (1.0f - range/avoidRange);
			}
			return;
		}
	}

	// turn is 2 if 'other' is behind us, 1 perpendicular, and 0 straight ahead
	float turn = 1.0f + DotProduct( -m_forward, to );

	Vector perp( -m_forward.y, m_forward.x, 0.0f );
	float side = (DotProduct( perp, to ) > 1.0f) ? 1.0f : -1.0f;

	if (turn > 1.0f)
	{
		// always turn one way to avoid dithering if many fish are behind us
		side = (m_turnClockwise) ? 1.0f : -1.0f;
	}

	float power = 1.0f - (range / maxRange);

	const float flockInfluence = 0.7f; // 0.3f;	// 0.3
	m_angleChange += amount * flockInfluence * power * side * turn;
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Returns a value between zero (no danger of hitting an obstacle)
 * and one (extreme danger of hitting an obstacle).
 * This is used to modulate later flocking behaviors.
 */
float CFish::Avoid( void )
{
	const float avoidPower = 100.0f; // 50.0f; // 25.0f;

	// 
	// Stay within pool bounds.
	// This may cause problems with pools with oddly concave portions
	// right at the max range.
	//
	Vector toCenter = m_pool->GetAbsOrigin() - GetAbsOrigin();
	const float avoidZone = 20.0f;
	if (toCenter.IsLengthGreaterThan( m_pool->GetMaxRange() - avoidZone ))
	{
		// turn away from edge
		if (DotProduct( toCenter, m_forward ) < 0.0f)
		{
			m_angleChange += (m_turnClockwise) ? -avoidPower : avoidPower;
		}

		// take total precedence over flocking
		return 1.0f;
	}

	trace_t result;
	const float sideOffset = 0.2f;

	float rightDanger = 0.0f;
	float leftDanger = 0.0f;

	// slightly right of forward
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + m_avoidRange * (m_forward + sideOffset * m_perp), MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );
	if (result.fraction < 1.0f)
	{
		rightDanger = 1.0f - result.fraction;
	}

	// slightly left of forward
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + m_avoidRange * (m_forward - sideOffset * m_perp), MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );
	if (result.fraction < 1.0f)
	{
		// steer away
		leftDanger = 1.0f - result.fraction;
	}

	// steer away - prefer one side to avoid cul-de-sacs
	if (m_turnClockwise)
	{
		if (rightDanger > 0.0f)
		{
			m_angleChange -= avoidPower * rightDanger;
		}
		else
		{
			m_angleChange += avoidPower * leftDanger;
		}
	}
	else
	{
		if (leftDanger > 0.0f)
		{
			m_angleChange += avoidPower * leftDanger;
		}
		else
		{
			m_angleChange -= avoidPower * rightDanger;
		}
	}

	return (leftDanger > rightDanger) ? leftDanger : rightDanger;
}

//-------------------------------------------------------------------------------------------------------------
void CFish::Panic( void )
{
	// start to panic
	m_panicTimer.Start( RandomFloat( 5.0f, 15.0f ) );
	m_moveTimer.Start( RandomFloat( 10.0f, 20.0f ) );
	m_desiredSpeed = m_panicSpeed;
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Invoked each server tick
 */
void CFish::Update( float deltaT )
{
	Vector deltaPos = GetAbsOrigin() - m_poolOrigin;
	const float safetyMargin = 5.0f;

	// pass relative position to the client
	// clamp them here to cover the rare cases where a fish's high velocity skirts the range limit
	m_x = clamp( deltaPos.x, -255.0f, 255.0f );
	m_y = clamp( deltaPos.y, -255.0f, 255.0f );
	m_z = m_poolOrigin->z;

	//
	// Dead fish just coast to a stop.  All floating to the
	// surface and bobbing motion is handled client-side.
	//
	if (m_lifeState == LIFE_DEAD)
	{
		// don't allow fish to leave maximum range of pool
		if (deltaPos.IsLengthGreaterThan( m_pool->GetMaxRange() - safetyMargin ))
		{
			SetAbsVelocity( Vector( 0, 0, 0 ) );
		}
		else
		{
			// decay movement speed to zero
			Vector vel = GetAbsVelocity();

			const float drag = 1.0f;
			vel -= drag * vel * deltaT;

			SetAbsVelocity( vel );
		}

		return;
	}

	//
	// Living fish behavior
	//

	// periodically change our turning preference
	if (m_turnTimer.IsElapsed())
	{
		m_turnTimer.Start( RandomFloat( 10.0f, 30.0f ) );
		m_turnClockwise = !m_turnClockwise;
	}

	if (m_panicTimer.GetRemainingTime() > 0.0f)
	{
		// panicking
		m_desiredSpeed = m_panicSpeed;
	}
	else if (m_moveTimer.GetRemainingTime() > 0.0f)
	{
		// normal movement
		m_desiredSpeed = m_calmSpeed;
	}
	else if (m_goTimer.IsElapsed())
	{
		// move every so often
		m_goTimer.Start( RandomFloat( 10.0f, 60.0f ) );
		m_moveTimer.Start( RandomFloat( 2.0f, 10.0 ) );
		m_desiredSpeed = m_calmSpeed;
	}

	// avoid obstacles
	float danger = Avoid();

	// flock towards visible fish
	for( int i=0; i<std::ssize( m_visible ); ++i )
	{
		FlockTo( m_visible[i], (1.0f - danger) );
	}

	// flock towards center of pool
	FlockTo( nullptr, (1.0f - danger) );

	//
	// Update orientation
	//

	// limit rate of angular change - proportional to movement rate
	const float maxAngleChange = (25.0f + 175.0f * (m_speed/m_panicSpeed)) * deltaT;
	if (m_angleChange > maxAngleChange)
	{
		m_angleChange = maxAngleChange;
	}
	else if (m_angleChange < -maxAngleChange)
	{
		m_angleChange = -maxAngleChange;
	}

	m_angle += m_angleChange;
	m_angleChange = 0.0f;

	m_forward.x = std::cos( m_angle * std::numbers::pi/180.0f );
	m_forward.y = std::sin( m_angle * std::numbers::pi/180.0f );
	m_forward.z = 0.0f;

	m_perp.x = -m_forward.y;
	m_perp.y = m_forward.x;
	m_perp.z = 0.0f;

	//
	// Update speed
	//
	const float rate = 2.0f;
	m_speed += rate * (m_desiredSpeed - m_speed) * deltaT;

	// decay desired speed if done moving
	if (m_moveTimer.IsElapsed())
	{
		const float decayRate = 1.0f;
		m_desiredSpeed -= decayRate * deltaT;
		if (m_desiredSpeed < 0.0f)
		{
			m_desiredSpeed = 0.0f;
		}
	}

	Vector vel = m_speed * m_forward;

	// don't allow fish to leave maximum range of pool
	if (deltaPos.IsLengthGreaterThan( m_pool->GetMaxRange() - safetyMargin ))
	{
		Vector toCenter = -deltaPos;

		float radial = DotProduct( toCenter, vel );
		if (radial < 0.0f)
		{
			// heading out of range, zero the radial velocity component
			toCenter.NormalizeInPlace();
			Vector perp( -toCenter.y, toCenter.x, 0.0f );

			float side = DotProduct( perp, vel );

			vel = side * perp;
		}
	}

	SetAbsVelocity( vel );

	m_flSpeed = m_speed;
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Zero the visible vector
 */
void CFish::ResetVisible( void )
{
	m_visible.clear();
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Add this fish to our visible vector
 */
void CFish::AddVisible( CFish *fish )
{
	m_visible.push_back( fish );
}

//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
/**
 * A CFishPool manages a collection of CFish, and defines where the "pool" is in the world.
 */

LINK_ENTITY_TO_CLASS( func_fish_pool, CFishPool );

//-------------------------------------------------------------------------------------------------------------
CFishPool::CFishPool( void )
{
	m_fishCount = 0;
	m_maxRange = 255.0f;
	m_swimDepth = 0.0f;
	m_isDormant = false;

	m_visTimer.Start( 0.5f );

	ListenForGameEvent( "player_shoot" );
	ListenForGameEvent( "player_footstep" );
	ListenForGameEvent( "weapon_fire" );
	ListenForGameEvent( "hegrenade_detonate" );
	ListenForGameEvent( "flashbang_detonate" );
	ListenForGameEvent( "smokegrenade_detonate" );
	ListenForGameEvent( "molotov_detonate" );
	ListenForGameEvent( "decoy_detonate" );
	ListenForGameEvent( "bomb_exploded" );
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Initialize the fish pool
 */
void CFishPool::Spawn_Impl()
{
	SetThink( &CFishPool::Update );
	SetNextThink( gpGlobals->curtime );

	m_waterLevel = UTIL_WaterLevel( GetAbsOrigin(), GetAbsOrigin().z, GetAbsOrigin().z + 1000.0f );

	trace_t result;
	for( int i=0; i<m_fishCount; ++i )
	{
		QAngle heading( 0.0f, RandomFloat( 0, 360.0f ), 0.0f );

		CFish *fish = (CFish *)Create( "fish", GetAbsOrigin(), heading, this );
		fish->Initialize( this, i );

		if (fish)
		{
			CHandle<CFish> hFish;
			hFish.Set( fish );
			m_fishes.push_back( hFish );
		}
	}
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Parse KeyValue pairs
 */
bool CFishPool::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq( szKeyName, "fish_count" ))
	{
		m_fishCount = std::atoi(szValue);
		return true;
	}
	else if (FStrEq( szKeyName, "max_range" ))
	{
		m_maxRange = std::atof(szValue);
		if (m_maxRange <= 1.0f)
		{
			m_maxRange = 1.0f;
		}
		else if (m_maxRange > 255.0f)
		{
			// stay within 8 bits range
			m_maxRange = 255.0f;
		}

		return true;
	}
	else if (FStrEq( szKeyName, "model" ))
	{
		PrecacheModel( szValue );
		SetModelName( AllocPooledString( szValue ) );
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Game event processing
 */
void CFishPool::FireGameEvent( IGameEvent *event )
{
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	
	// the fish panic
	const float loudRange = 500.0f;
	const float quietRange = 75.0f;

	float range = (V_strcmp( "player_footstep", event->GetName() )) ? loudRange : quietRange;

	for( int i=0; i<std::ssize( m_fishes ); ++i )
	{
		// if player is NULL, assume a game-wide event
		if (player && (player->GetAbsOrigin() - m_fishes[i]->GetAbsOrigin()).IsLengthGreaterThan( range ))
		{
			// event too far away to care
			continue;
		}

		m_fishes[i]->Panic();
	}
}

//-------------------------------------------------------------------------------------------------------------
/**
 * Invoked each server tick
 */
void CFishPool::Update( void )
{
	float deltaT = 0.1f;
	SetNextThink( gpGlobals->curtime + deltaT );

	/// @todo Go dormant when no players are around to see us

	if (fish_dormant.GetBool())
	{
		if (!m_isDormant)
		{
			// stop all the fish
			for( int i=0; i<std::ssize( m_fishes ); ++i )
			{
				m_fishes[i]->SetAbsVelocity( Vector( 0, 0, 0 ) );
			}
			
			m_isDormant = true;
		}

		return;
	}
	else
	{
		m_isDormant = false;
	}

	// update fish to fish visibility
	if (m_visTimer.IsElapsed())
	{
		m_visTimer.Reset();

		int i, j;
		trace_t result;

		// reset each fishes vis list
		for( i=0; i<std::ssize( m_fishes ); ++i )
		{
			m_fishes[i]->ResetVisible();
		}

		// build new vis lists - line of sight is symmetric
		for( i=0; i<std::ssize( m_fishes ); ++i )
		{
			if (!m_fishes[i]->IsAlive())
				continue;

			for( j=i+1; j<std::ssize( m_fishes ); ++j )
			{
				if (!m_fishes[j]->IsAlive())
					continue;

				UTIL_TraceLine( m_fishes[i]->GetAbsOrigin(), m_fishes[j]->GetAbsOrigin(), MASK_PLAYERSOLID, m_fishes[i], COLLISION_GROUP_NONE, &result );
				if (result.fraction >= 1.0f)
				{
					// the fish can see each other
					m_fishes[i]->AddVisible( m_fishes[j] );
					m_fishes[j]->AddVisible( m_fishes[i] );
				}
			}
		}		
	}

	// simulate the fishes behavior
	for( int i=0; i<std::ssize( m_fishes ); ++i )
	{
		m_fishes[i]->Update( deltaT );
	}
}

float CFishPool::GetMaxRange( void ) const
{
	return m_maxRange;
}

float CFishPool::GetWaterLevel( void ) const
{
	return m_waterLevel;
}

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CBaseTrigger;
class CFogController;
struct fogparams_t;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: An entity that allows level designer control over the fog parameters.

CFogSystem s_FogSystem( "FogSystem" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CFogSystem *FogSystem( void )
{
	return &s_FogSystem;
}

LINK_ENTITY_TO_CLASS( env_fog_controller, CFogController );

IMPLEMENT_REFLECT_DATAMAP( CFogController )

IMPLEMENT_REFLECT_SERVERCLASS( CFogController, DT_FogController );

CFogController::CFogController()
{
	// Make sure that old maps without fog fields don't get wacked out fog values.
	m_fog.enable = false;
	m_fog.maxdensity = 1.0f;
	m_fog.HDRColorScale = 1.0f;
}

CFogController::~CFogController()
{
}

void CFogController::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	m_fog.colorPrimaryLerpTo = m_fog.colorPrimary;
	m_fog.colorSecondaryLerpTo = m_fog.colorSecondary;
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CFogController::Activate_Impl( ) 
{
	BaseClass::Activate_Impl();

	if ( m_bUseAngles )
	{
		AngleVectors( GetAbsAngles(), &m_fog.dirPrimary.GetForModify() );
		m_fog.dirPrimary.GetForModify() *= -1.0f; 
	}	    
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFogController::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//------------------------------------------------------------------------------
// Purpose: Input handler for setting the fog start distance.
//------------------------------------------------------------------------------
void CFogController::InputSetStartDist(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.start = inputdata.value.Float();
}

//------------------------------------------------------------------------------
// Purpose: Input handler for setting the fog end distance.
//------------------------------------------------------------------------------
void CFogController::InputSetEndDist(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.end = inputdata.value.Float();
}

//------------------------------------------------------------------------------
// Input handler for setting the maximum density of the fog. This lets us bring
// the start distance in without the scene fogging too much.
//------------------------------------------------------------------------------
void CFogController::InputSetMaxDensity( inputdata_t &inputdata )
{
	m_fog.maxdensity = inputdata.value.Float();
}

//------------------------------------------------------------------------------
// Purpose: Input handler for turning on the fog.
//------------------------------------------------------------------------------
void CFogController::InputTurnOn(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.enable = true;
}

//------------------------------------------------------------------------------
// Purpose: Input handler for turning off the fog.
//------------------------------------------------------------------------------
void CFogController::InputTurnOff(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.enable = false;
}

//------------------------------------------------------------------------------
// Purpose: Input handler for setting the primary fog color.
//------------------------------------------------------------------------------
void CFogController::InputSetColor(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.colorPrimary = inputdata.value.Color32();
}

//------------------------------------------------------------------------------
// Purpose: Input handler for setting the secondary fog color.
//------------------------------------------------------------------------------
void CFogController::InputSetColorSecondary(inputdata_t &inputdata)
{
	// Get the world entity.
	m_fog.colorSecondary = inputdata.value.Color32();
}

void CFogController::InputSetFarZ(inputdata_t &inputdata)
{
	m_fog.farz = inputdata.value.Int();
}

void CFogController::InputSetZoomFogScale( inputdata_t &inputdata )
{
	m_fog.ZoomFogScale = inputdata.value.Float();
}

//------------------------------------------------------------------------------
// Purpose: Sets the angles to use for the secondary fog direction.
//------------------------------------------------------------------------------
void CFogController::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	SetAbsAngles( angles );

	AngleVectors( GetAbsAngles(), &m_fog.dirPrimary.GetForModify() );
	m_fog.dirPrimary.GetForModify() *= -1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFogController::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		V_snprintf(tempstr,sizeof(tempstr),"State: %s",(m_fog.enable)?"On":"Off");
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"Start: %3.0f",m_fog.start.Get() );
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"End  : %3.0f",m_fog.end.Get() );
		EntityText(text_offset,tempstr,0);
		text_offset++;

		color32 color = m_fog.colorPrimary;
		V_snprintf(tempstr,sizeof(tempstr),"1) Red  : %i",color.r);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"1) Green: %i",color.g);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"1) Blue : %i",color.b);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		color = m_fog.colorSecondary;
		V_snprintf(tempstr,sizeof(tempstr),"2) Red  : %i",color.r);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"2) Green: %i",color.g);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"2) Blue : %i",color.b);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"HDR Color Scale: %0.3f",m_fog.HDRColorScale.Get() );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

static constexpr auto FOG_CONTROLLER_COLORPRIMARY_LERP = (1 << 0);
static constexpr auto FOG_CONTROLLER_COLORSECONDARY_LERP = (1 << 1);
static constexpr auto FOG_CONTROLLER_START_LERP = (1 << 2);
static constexpr auto FOG_CONTROLLER_END_LERP = (1 << 3);
static constexpr auto FOG_CONTROLLER_MAXDENSITY_LERP = (1 << 4);

void CFogController::InputSetColorLerpTo(inputdata_t &data)
{
	m_iChangedVariables |= FOG_CONTROLLER_COLORPRIMARY_LERP;
	m_fog.colorPrimaryLerpTo = data.value.Color32();
}

void CFogController::InputSetColorSecondaryLerpTo(inputdata_t &data)
{
	m_iChangedVariables |= FOG_CONTROLLER_COLORSECONDARY_LERP;
	m_fog.colorSecondaryLerpTo = data.value.Color32();
}

void CFogController::InputSetStartDistLerpTo(inputdata_t &data)
{
	m_iChangedVariables |= FOG_CONTROLLER_START_LERP;
	m_fog.startLerpTo = data.value.Float();
}

void CFogController::InputSetEndDistLerpTo(inputdata_t &data)
{
	m_iChangedVariables |= FOG_CONTROLLER_END_LERP;
	m_fog.endLerpTo = data.value.Float();
}

void CFogController::InputSetMaxDensityLerpTo(inputdata_t &data)
{
	m_iChangedVariables |= FOG_CONTROLLER_MAXDENSITY_LERP;
	m_fog.maxdensityLerpTo = data.value.Float();
}

void CFogController::InputStartFogTransition(inputdata_t &data)
{
	SetThink( &CFogController::SetLerpValues );

	m_fog.lerptime = gpGlobals->curtime + m_fog.duration + 0.1;
    SetNextThink( gpGlobals->curtime + m_fog.duration );
}

void CFogController::SetLerpValues( void )
{
	if ( m_iChangedVariables & FOG_CONTROLLER_COLORPRIMARY_LERP )
	{
		m_fog.colorPrimary = m_fog.colorPrimaryLerpTo;
	}

	if ( m_iChangedVariables & FOG_CONTROLLER_COLORSECONDARY_LERP )
	{
		m_fog.colorSecondary = m_fog.colorSecondaryLerpTo;
	} 

	if ( m_iChangedVariables & FOG_CONTROLLER_START_LERP )
	{
		m_fog.start = m_fog.startLerpTo;
	}

	if ( m_iChangedVariables & FOG_CONTROLLER_END_LERP )
	{
		m_fog.end = m_fog.endLerpTo;
	}

	if ( m_iChangedVariables & FOG_CONTROLLER_MAXDENSITY_LERP )
	{
		m_fog.maxdensity = m_fog.maxdensityLerpTo;
	}

	m_iChangedVariables = 0;
	m_fog.lerptime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the fog controller.
//-----------------------------------------------------------------------------
void CFogSystem::LevelInitPreEntity( void )
{
	m_hMasterController = nullptr;
	ListenForGameEvent( "round_start" );
}

//-----------------------------------------------------------------------------
// Purpose: Find the master controller.  If no controller is 
//			set as Master, use the first controller found.
//-----------------------------------------------------------------------------
void CFogSystem::InitMasterController( void )
{
	CFogController *pFogController = nullptr;
	do
	{
		pFogController = static_cast<CFogController*>( gEntList.FindEntityByClassname( pFogController, "env_fog_controller" ) );
		if ( pFogController )
		{
			if ( m_hMasterController.Get() == nullptr )
			{
				m_hMasterController = pFogController;
			}
			else
			{
				if ( pFogController->IsMaster() )
				{
					m_hMasterController = pFogController;
				}
			}
		}
	} while ( pFogController );
}

void CFogSystem::SetMasterController( CFogController *pFogController )
{
	m_hMasterController = pFogController;
}

//-----------------------------------------------------------------------------
// Purpose: On a multiplayer map restart, re-find the master controller.
//-----------------------------------------------------------------------------
void CFogSystem::FireGameEvent( IGameEvent *pEvent )
{
	InitMasterController();
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master fog controller.  If no controller is 
//			set as Master, use the first fog controller found.
//-----------------------------------------------------------------------------
void CFogSystem::LevelInitPostEntity( void )
{
	InitMasterController();

	// HACK: Singleplayer games don't get a call to CBasePlayer::Spawn on level transitions.
	// CBasePlayer::Activate is called before this is called so that's too soon to set up the fog controller.
	// We don't have a hook similar to Activate that happens after LevelInitPostEntity
	// is called, or we could just do this in the player itself.
	if ( gpGlobals->maxClients == 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer && ( pPlayer->m_PlayerFog.m_hCtrl.Get() == nullptr ) )
		{
			pPlayer->InitFogController();
		}
	}
}

//--------------------------------------------------------------------------------------------------------
class CFogTrigger : public CBaseTrigger
{
	[[= ks::reflect::KeyFrom<"m_fog.colorPrimary", ks::reflect::Key{ .name = "fogcolor" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.colorSecondary", ks::reflect::Key{ .name = "fogcolor2" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.dirPrimary", ks::reflect::Key{ .name = "fogdir" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.enable", ks::reflect::Key{ .name = "fogenable" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.blend", ks::reflect::Key{ .name = "fogblend" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.start", ks::reflect::Key{ .name = "fogstart" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.end", ks::reflect::Key{ .name = "fogend" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_fog.farz", ks::reflect::Key{ .name = "farz" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFogTrigger, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void StartTouch_Impl( CBaseEntity *other );
	void EndTouch_Impl( CBaseEntity *other );

	fogparams_t *GetFog( void )
	{
		return &m_fog;
	}

private:
	fogparams_t	m_fog;
};

LINK_ENTITY_TO_CLASS( trigger_fog, CFogTrigger );

IMPLEMENT_REFLECT_DATAMAP( CFogTrigger )

//--------------------------------------------------------------------------------------------------------
void CFogTrigger::Spawn_Impl( void )
{
	AddSpawnFlags( SF_TRIGGER_ALLOW_ALL );

	BaseClass::Spawn_Impl();
	InitTrigger();
}

//--------------------------------------------------------------------------------------------------------
void CFogTrigger::StartTouch_Impl( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::StartTouch_Impl( other );

	CBaseCombatCharacter *character = other->MyCombatCharacterPointer();
	if ( !character )
		return;

	character->OnFogTriggerStartTouch( this );
}

//--------------------------------------------------------------------------------------------------------
void CFogTrigger::EndTouch_Impl( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::EndTouch_Impl( other );

	CBaseCombatCharacter *character = other->MyCombatCharacterPointer();
	if ( !character )
		return;

	character->OnFogTriggerEndTouch( this );
}

//--------------------------------------------------------------------------------------------------------
bool GetWorldFogParams( CBaseCombatCharacter *character, fogparams_t &fog )
{

	fogparams_t *targetFog = nullptr;
	if ( character && character->GetFogTrigger() )
	{
		CFogTrigger *trigger = dynamic_cast< CFogTrigger * >(character->GetFogTrigger());
		if ( trigger )
		{
			targetFog = trigger->GetFog();
		}
	}

	if ( !targetFog && FogSystem()->GetMasterFogController() )
	{
		targetFog = &(FogSystem()->GetMasterFogController()->m_fog);
	}

	if ( targetFog )
	{
		if ( *targetFog != fog )
		{
			fog = *targetFog;
			return true;
		}
	}
	else
	{
		if ( fog.farz != -1 || fog.enable != false )
		{
			// No fog controller in this level. Use default fog parameters.
			fog.farz = -1;
			fog.enable = false;
			return true;
		}
	}

	return false;
}
