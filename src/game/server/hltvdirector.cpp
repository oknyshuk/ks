// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include "eiface.h"
#include "igameevents.h"
#include "ihltv.h"
#include "networkvar.h"
#include "shareddefs.h"
#include "datacache/imdlcache.h"
#include "declare_class.h"
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
#include "server_class.h"
#include "reflect_sendtable.h"
#include "vphysics/constraints.h"
#include "vscript/ivscript.h"
module ks.server;

import std;
import ks.core.dbg;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import :entity_2;
import ks.math.ssequaternion;
import :entity_dispatch;
import :entity_kind;
import ks.core.basetypes;
import ks.math.transform;
import ks.math.vmatrix;
import ks.core.color;
import ks.core.platform;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// hltvdirector.cpp: implementation of the CHLTVDirector class.
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static ConVar tv_delay( side_registry(), "tv_delay", "10", FCVAR_RELEASE, "GOTV broadcast delay in seconds", true, 0, true, HLTV_MAX_DELAY );
static ConVar tv_allow_static_shots( side_registry(), "tv_allow_static_shots", "1", FCVAR_RELEASE, "Auto director uses fixed level cameras for shots" );

static constexpr auto RANDOM_MAX_ELEMENTS = 256;
static int s_RndOrder[RANDOM_MAX_ELEMENTS];
static void InitRandomOrder(int nFields)
{
	if ( nFields > RANDOM_MAX_ELEMENTS )
	{
		contract_assert( nFields > RANDOM_MAX_ELEMENTS );
		nFields = RANDOM_MAX_ELEMENTS;
	}

	for ( int i=0; i<nFields; i++ )
	{
		s_RndOrder[i]=i;
	}

	for ( int i=0; i<(nFields/2); i++ )
	{
		int pos1 = RandomInt( 0, nFields-1 );
		int pos2 = RandomInt( 0, nFields-1 );
		int temp = s_RndOrder[pos1];
		s_RndOrder[pos1] = s_RndOrder[pos2];
		s_RndOrder[pos2] = temp;
	}
};

static float WeightedAngle( Vector vec1, Vector vec2)
{
	VectorNormalize( vec1 );
	VectorNormalize( vec2 );

	float a = DotProduct( vec1, vec2 ); // a = [-1,1]

	a = (a + 1.0f) / 2.0f;

	contract_assert( a <= 1 && a >= 0 );
		
	return a*a;	// vectors are facing opposite direction
}

CHLTVDirector::CHLTVDirector()
{
	m_iPVSEntity = 0;
	m_fDelay = 30.0;
	m_iLastPlayer = 1;
	m_iCameraMan = 0;
	m_nNumFixedCameras = 0;
	m_nNextAnalyzeTick = 0;
	m_iCameraManIndex = 0;
}

CHLTVDirector::~CHLTVDirector()
{

}

bool CHLTVDirector::Init()
{
	return gameeventmanager->LoadEventsFromFile( "resource/hltvevents.res" ) > 0;
}

void CHLTVDirector::Shutdown()
{
	RemoveEventsFromHistory(-1); // all
}

void CHLTVDirector::FireGameEvent( IGameEvent * event )
{
	if ( m_HltvServers.empty() )
		return;	// don't do anything

	CHLTVGameEvent gameevent;

	gameevent.m_Event = gameeventmanager->DuplicateEvent( event );
	gameevent.m_Priority = event->GetInt( "priority", -1 ); // priorities are leveled between 0..10, -1 means ignore
	gameevent.m_Tick = gpGlobals->tickcount;
	
	m_EventHistory.insert( gameevent );
}

void CHLTVDirector::AddHLTVServer( IHLTVServer *hltv )
{
	RemoveEventsFromHistory(-1); // all

	if ( hltv ) 
	{
		RemoveHLTVServer( hltv );
		HltvServerRecord_t rec;
		rec.m_pHLTVServer = hltv;
		rec.m_pHLTVClient = UTIL_PlayerByIndex( hltv->GetHLTVSlot() + 1 );

		if ( rec.m_pHLTVClient && rec.m_pHLTVClient->IsHLTV() )
		{
			rec.m_pHLTVServer = hltv;
		}
		else
		{
			rec.m_pHLTVServer  = nullptr;
			Error( "Couldn't find GOTV client player." );
		}

		if ( m_HltvServers.empty() )
		{
			// register for events the director needs to know
			ListenForGameEvent( "player_hurt" );
			ListenForGameEvent( "player_death" );
			ListenForGameEvent( "round_end" );
			ListenForGameEvent( "round_start" );
			ListenForGameEvent( "hltv_rank_entity" );
			ListenForGameEvent( "hltv_rank_camera" );
		}
		m_HltvServers.push_back( rec );
	}
}

void CHLTVDirector::RemoveHLTVServer( IHLTVServer *hltv )
{
	RemoveEventsFromHistory( -1 );
	for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
	{
		if ( m_HltvServers[ i ].m_pHLTVServer == hltv )
		{
			ks::swap_remove( m_HltvServers, i );
			break;
		}
	}
}

bool CHLTVDirector::IsActive( void )
{
	return !m_HltvServers.empty();
}

float CHLTVDirector::GetDelay( void )
{
	return m_fDelay;
}

int	CHLTVDirector::GetDirectorTick( void )
{
	// just simple delay it
	return m_nBroadcastTick;
}

int	CHLTVDirector::GetPVSEntity( void )
{
	return m_iPVSEntity;
}

Vector CHLTVDirector::GetPVSOrigin( void )
{
	return m_vPVSOrigin;
}

void CHLTVDirector::UpdateSettings()
{
	// set delay
	m_fDelay = tv_delay.GetFloat();

	int newBroadcastTick = gpGlobals->tickcount;
	
	if ( m_fDelay < HLTV_MIN_DIRECTOR_DELAY )
	{
		// instant broadcast, no delay
		m_fDelay = 0.0;
	}
	else
	{
		// broadcast time is current time - delay time
		newBroadcastTick -= TIME_TO_TICKS( m_fDelay );
	}

	if( (m_nBroadcastTick == 0) && (newBroadcastTick > 0) )
	{
		// we start broadcasting right now, reset NextShotTimer
		m_nNextShotTick = 0;
	}

	// check if camera man is still valid 
	if ( m_iCameraManIndex > 0 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_iCameraManIndex );
		if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		{
			SetCameraMan( 0 );
		}
	}

   	m_nBroadcastTick = std::max( 0, newBroadcastTick );
}

const char** CHLTVDirector::GetModEvents()
{
	static const char *s_modevents[] =
	{
		"hltv_status",
		"hltv_chat",
		"player_connect",
		"player_disconnect",
		"player_team",
		"player_info",
		"server_cvar",
		"player_death",
		"player_chat",
		"round_start",
		"round_end",
		nullptr
	};

	return s_modevents;
}

void CHLTVDirector::StartAutoRecording()
{
	for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
	{
		if ( !m_HltvServers[ i ].m_pHLTVServer->IsRecording() )
		{
			m_HltvServers[ i ].m_pHLTVServer->StartAutoRecording();
		}
	}
}

void CHLTVDirector::BuildCameraList( void )
{
	m_nNumFixedCameras = 0;
	std::memset( m_pFixedCameras, 0, sizeof ( m_pFixedCameras ) );

	CBaseEntity *pCamera = gEntList.FindEntityByClassname( nullptr, GetFixedCameraEntityName() );

	while ( pCamera && m_nNumFixedCameras < MAX_NUM_CAMERAS)
	{
		CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, STRING(pCamera->m_target) );

		if ( pTarget )
		{
			// look at target if any given
			QAngle angles;
			VectorAngles( pTarget->GetAbsOrigin() - pCamera->GetAbsOrigin(), angles );
			pCamera->SetAbsAngles( angles );
		}

		m_pFixedCameras[m_nNumFixedCameras] = pCamera;

		m_nNumFixedCameras++;
		pCamera = gEntList.FindEntityByClassname( pCamera, GetFixedCameraEntityName() );
	}
}

// this is called with every new map 
void CHLTVDirector::LevelInitPostEntity( void )
{
	BuildCameraList();

	m_vPVSOrigin.Init();
	m_iPVSEntity = 0;
	m_nNextShotTick = 0;
	m_nNextAnalyzeTick = 0;
	m_iCameraManIndex = 0;

	RemoveEventsFromHistory(-1); // all

	// DevMsg("HLTV Director: found %i fixed cameras.\n", m_nNumFixedCameras );
}

void CHLTVDirector::FrameUpdatePostEntityThink( void )
{
	if ( m_HltvServers.empty() )
		return;	// don't do anything

	// This function is called each tick
	UpdateSettings();	// update settings from cvars

	if ( (m_nNextAnalyzeTick < gpGlobals->tickcount) && 
		 (m_fDelay >= HLTV_MIN_DIRECTOR_DELAY) )
	{
		m_nNextAnalyzeTick = gpGlobals->tickcount + TIME_TO_TICKS( 0.5f );

		AnalyzePlayers();

		AnalyzeCameras();
	}

	if ( m_nBroadcastTick <= 0 )
	{
		// game start is still in delay loop
		StartDelayMessage();
	}
	else if ( m_nNextShotTick <= m_nBroadcastTick )
	{
		// game is being broadcasted, generate camera shots
		StartNewShot();		
	}
}

void CHLTVDirector::StartDelayMessage()
{
	if ( m_nNextShotTick > gpGlobals->tickcount )
		return;

	// check the next 8 seconds for interrupts/important events
	m_nNextShotTick = gpGlobals->tickcount + TIME_TO_TICKS( DEF_SHOT_LENGTH );

	// game hasn't started yet, we are still in the broadcast delay hole
	IGameEvent *msg = gameeventmanager->CreateEvent( "hltv_message", true );

	if ( msg )
	{
		msg->SetString("text", "#GOTV_Please_Wait_for_Broadcast_to_Start" );

		// send spectators the HLTV director command as a game event
		for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
		{
			m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( msg );
		}
		gameeventmanager->FreeEvent( msg );
	}

	StartBestFixedCameraShot( true );
}

void CHLTVDirector::StartBestPlayerCameraShot()
{
	float flPlayerRanking[MAX_PLAYERS];

	std::memset( flPlayerRanking, 0, sizeof(flPlayerRanking) );

	auto index = FindFirstEvent( m_nBroadcastTick );

	float flBestRank = -1.0f;
	int iBestCamera = -1;
	int iBestTarget = -1;

	// sum all ranking values for the cameras

	while( index != m_EventHistory.end() )
	{
		const CHLTVGameEvent &dc = *index;

		if ( dc.m_Tick >= m_nNextShotTick )
			break; 

		// search for camera ranking events
		if ( V_strcmp( dc.m_Event->GetName(), "hltv_rank_entity") == 0 )
		{
			int index = dc.m_Event->GetInt("index"); 

			if ( index < MAX_PLAYERS )
			{
				flPlayerRanking[index] += dc.m_Event->GetFloat("rank" );

				// find best camera
				if ( flPlayerRanking[index] > flBestRank )
				{
					iBestCamera = index;
					flBestRank = flPlayerRanking[index];
					iBestTarget = dc.m_Event->GetInt("target"); 
				}
			}
		}

		++index;
	}

	if ( iBestCamera != -1 )
	{
		// view over shoulder, randomly left or right
		StartChaseCameraShot( iBestCamera, iBestTarget, 112.0f, 20, (RandomFloat()>0.5)?20:-20, true );
	}
	else
	{
		StartBestFixedCameraShot( true );
	}
}

void CHLTVDirector::StartFixedCameraShot(int iCamera, int iTarget)
{
	CBaseEntity *pCamera = m_pFixedCameras[iCamera];
	
	Vector vCamPos = pCamera->GetAbsOrigin();
	QAngle aViewAngle = pCamera->GetAbsAngles();

	m_iPVSEntity = 0;	// don't use camera entity, since it may not been transmitted
	m_vPVSOrigin = vCamPos;

	IGameEvent *shot = gameeventmanager->CreateEvent( "hltv_fixed", true );

	if ( shot )
	{
		shot->SetInt("posx", vCamPos.x );
		shot->SetInt("posy", vCamPos.y );
		shot->SetInt("posz", vCamPos.z );
		shot->SetInt("theta", aViewAngle.x );
		shot->SetInt("phi", aViewAngle.y );
		shot->SetInt("target", iTarget );
		shot->SetFloat("fov", RandomFloat(50,110) );
	
		// send spectators the HLTV director command as a game event
		for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
		{
			m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( shot );
		}
		gameeventmanager->FreeEvent( shot );
	}
}

void CHLTVDirector::StartChaseCameraShot(int iTarget1, int iTarget2, int distance, int phi, int theta, bool bInEye)
{
	IGameEvent *shot = gameeventmanager->CreateEvent( "hltv_chase", true );

	if ( !shot )
		return;
	
	shot->SetInt("target1", iTarget1 );
	shot->SetInt("target2", iTarget2 );
	shot->SetInt("distance", distance );
	shot->SetInt("phi", phi ); // hi/low
	shot->SetInt( "theta", theta ); // left/right
	shot->SetInt( "ineye", bInEye?1:0 );
		
	m_iPVSEntity = iTarget1;

	// send spectators the HLTV director command as a game event
	for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
	{
		m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( shot );
	}
	gameeventmanager->FreeEvent( shot );
}

void CHLTVDirector::StartBestFixedCameraShot( bool bForce )
{
	float	flCameraRanking[MAX_NUM_CAMERAS];

	if ( m_nNumFixedCameras <= 0 )
		return;

	std::memset( flCameraRanking, 0, sizeof(flCameraRanking) );

	auto index = FindFirstEvent( m_nBroadcastTick );

	float flBestRank = -1.0f;
	int iBestCamera = -1;
	int iBestTarget = -1;

	// sum all ranking values for the cameras

	while( index != m_EventHistory.end() )
	{
		const CHLTVGameEvent &dc = *index;

		if ( dc.m_Tick >= m_nNextShotTick )
			break; 

		// search for camera ranking events
		if ( V_strcmp( dc.m_Event->GetName(), "hltv_rank_camera") == 0 )
		{
			int index = dc.m_Event->GetInt("index"); 
			flCameraRanking[index] += dc.m_Event->GetFloat("rank" );

			// find best camera
			if ( flCameraRanking[index] > flBestRank )
			{
				iBestCamera = index;
				flBestRank = flCameraRanking[index];
				iBestTarget = dc.m_Event->GetInt("target"); 
			}
		}

		++index;
	}

	if ( !bForce && flBestRank == 0 )
	{
		// if we are not forcing a fixed camera shot, switch to player chase came
		// if no camera shows any players
		StartBestPlayerCameraShot();
	}
	else if ( iBestCamera != -1 )
	{
		StartFixedCameraShot( iBestCamera, iBestTarget );
	}
}

void CHLTVDirector::StartRandomShot() 
{
	int toTick = m_nBroadcastTick + TIME_TO_TICKS ( DEF_SHOT_LENGTH );
	m_nNextShotTick = std::min( m_nNextShotTick, toTick );

	// follow a player
	StartBestPlayerCameraShot();
}

void CHLTVDirector::CreateShotFromEvent( const CHLTVGameEvent *event )
{
	const char *name = event->m_Event->GetName();
	
	bool bPlayerHurt = V_strcmp( "player_hurt", name ) == 0;
	bool bPlayerKilled = V_strcmp( "player_death", name ) == 0;
	bool bRoundStart = V_strcmp( "round_start", name ) == 0;
	bool bRoundEnd = V_strcmp( "round_end", name ) == 0;

	if ( bPlayerHurt || bPlayerKilled )
	{
		CBaseEntity *victim = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );
		CBaseEntity *attacker = UTIL_PlayerByUserId( event->m_Event->GetInt("attacker") );

		if ( !victim )
			return;

		// if killed self
		if ( bPlayerKilled && ( attacker == victim || attacker == nullptr ) )
		{
			// player killed self or by WORLD
			StartChaseCameraShot( victim->entindex(), 0, 96, 20, 0, false );
		}
		// otherwise a hurt or kill
		else if ( attacker != victim && attacker != nullptr )
		{
			// don't randomize POV if it's a multikill
			if ( event->m_Event->GetInt("priority") < 9 )
			{
				if ( RandomFloat(0,1) > 0.75 )
				{
					V_swap( attacker, victim );
				}
			}
		
			StartChaseCameraShot( attacker->entindex(), victim->entindex(), 96, -20, (RandomFloat()>0.5)?30:-30, true );
		}
				
		// stay on shot for 2 seconds after death/hurt
		m_nNextShotTick = std::min( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
	}
	else if ( bRoundStart || bRoundEnd )
	{
		StartBestPlayerCameraShot();
	}
	else
	{
		DevMsg( "No known TV shot for event %s\n", name );
	}
}

void CHLTVDirector::CheckHistory()
{
	int lastTick = -1;

	for ( const CHLTVGameEvent &dc : m_EventHistory )
	{
		contract_assert( lastTick <= dc.m_Tick );
		lastTick = dc.m_Tick;
	}
}

void CHLTVDirector::RemoveEventsFromHistory(int tick)
{
	// history is sorted by tick, so the events to remove are a prefix
	auto last = ( tick == -1 ) ? m_EventHistory.end() : m_EventHistory.lower_bound( tick );

	for ( auto it = m_EventHistory.begin(); it != last; ++it )
	{
		gameeventmanager->FreeEvent( it->m_Event );
	}
	m_EventHistory.erase( m_EventHistory.begin(), last );

}

CHLTVDirector::EventHistory_t::const_iterator CHLTVDirector::FindFirstEvent( int tick ) const
{
	return m_EventHistory.lower_bound( tick );
}

bool CHLTVDirector::SetCameraMan( int iPlayerIndex )
{
	return true;
}

void CHLTVDirector::FinishCameraManShot()
{
}

bool CHLTVDirector::StartCameraManShot()
{
	return false;	// no camera man found
}

void CHLTVDirector::StartInstantBroadcastShot()
{
	m_nNextShotTick = m_nBroadcastTick + TIME_TO_TICKS( MAX_SHOT_LENGTH );

	{
		RemoveEventsFromHistory(-1); // all

		AnalyzePlayers();

		AnalyzeCameras();

		StartRandomShot();
	}
}

void CHLTVDirector::StartNewShot()
{
	// we can remove all events the
	int smallestTick = std::max(0, gpGlobals->tickcount - TIME_TO_TICKS(HLTV_MAX_DELAY) );
    RemoveEventsFromHistory( smallestTick );

	// if the delay time is too short for autodirector, just show next best thing
	if ( m_fDelay < HLTV_MIN_DIRECTOR_DELAY )
	{
		StartInstantBroadcastShot();
		return;
	}

	   // ok, no camera man active, now check how much time
	// we have for the next shot, if the time diff to the next
	// important event we have to switch to is too short (<2sec)
	// just extent the current shot and don't start a new one

	// check the next 8 seconds for interrupts/important events
	m_nNextShotTick = m_nBroadcastTick + TIME_TO_TICKS( MAX_SHOT_LENGTH );

	if ( m_nBroadcastTick <= 0 )
	{
		// game hasn't started yet, we are still in the broadcast delay hole
		IGameEvent *msg = gameeventmanager->CreateEvent( "hltv_message", true );

		if ( msg )
		{
			msg->SetString("text", "Please wait for broadcast to start ..." );
			
			// send spectators the HLTV director command as a game event
			for ( int i = 0; i < std::ssize( m_HltvServers ); ++i )
			{
				m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( msg );
			}
			gameeventmanager->FreeEvent( msg );
		}

		StartBestFixedCameraShot( true );
		return;
	}
	
	float flDuration = TICKS_TO_TIME(m_nNextShotTick - m_nBroadcastTick);

	if ( flDuration < MIN_SHOT_LENGTH )
		return;	// not enough time for a new shot

	// find the most interesting game event for next shot
	const CHLTVGameEvent *dc = FindBestGameEvent();

	if ( dc )
	{
		// show the game event
		CreateShotFromEvent( dc );
	}
	else
	{
		// no interesting events found, start random shot
		StartRandomShot();
	}
}

const CHLTVGameEvent *CHLTVDirector::FindBestGameEvent()
{
	const CHLTVGameEvent *bestEvent[4] = {};
	int	bestEventPrio[4];

	V_memset( bestEventPrio, 0, sizeof(bestEventPrio) );
	
	auto index = FindFirstEvent( m_nBroadcastTick );
		
	// search for next 4 best events within next 8 seconds
	for (int i = 0; i<4; i ++)
	{
		bestEventPrio[i] = 0;
		bestEvent[i] = nullptr;
	
		int tillTick = m_nBroadcastTick + TIME_TO_TICKS( 2.0f*(1.0f+i) );

		if ( tillTick > m_nNextShotTick )
			break;

		// sum all action for the next time
		while ( index != m_EventHistory.end()  )
		{
			const CHLTVGameEvent &event = *index;

			if ( event.m_Tick > tillTick )
				break;

			int priority = event.m_Priority;

			if ( priority > bestEventPrio[i] )
			{
				bestEvent[i] = &event;
				bestEventPrio[i] = priority;
			}
			
			++index;
		}
	}

	if ( !( bestEventPrio[0] || bestEventPrio[1] || bestEventPrio[2] ) )
		return nullptr; // no event found at all, give generic algorithm a chance

	// camera cut rules :

	if ( bestEventPrio[1] >= bestEventPrio[0] &&
		 bestEventPrio[1] >= bestEventPrio[2] &&
		 bestEventPrio[1] >= bestEventPrio[3] )
	{
		return bestEvent[1];	// best case
	}
	else if ( bestEventPrio[0] > bestEventPrio[1] &&
			  bestEventPrio[0] > bestEventPrio[2] )
	{
		return bestEvent[0];	// event 0 is very important
	}
	else if (	bestEventPrio[2] > bestEventPrio[3] ) 
	{
		return bestEvent[2];	
	}
	else
	{
		// event 4 is the best but too far away, so show event 1 
		if ( bestEvent[0] )
			return bestEvent[0];
		else
			return nullptr;
	}
}

void CHLTVDirector::AnalyzeCameras()
{
	InitRandomOrder( m_nNumFixedCameras );

	for ( int i = 0; i<m_nNumFixedCameras; i++ )
	{
		int iCameraIndex = s_RndOrder[i];
		CBaseEntity *pCamera = m_pFixedCameras[ iCameraIndex ];

		float	flRank = 0.0f;
		int		iClosestPlayer = 0;
		float	flClosestPlayerDist = 100000.0f;
		int		nCount = 0; // Number of visible targets
		Vector	vDistribution; vDistribution.Init(); // distribution of targets

		Vector vCamPos = pCamera->GetAbsOrigin();

		for ( int j=0; j<m_nNumActivePlayers; j++ )
		{
			CBasePlayer *pPlayer = m_pActivePlayers[j];

			Vector vPlayerPos = pPlayer->GetAbsOrigin();

			float dist = VectorLength( vPlayerPos - vCamPos );

			if ( dist > 1024.0f || dist < 4.0f )
				continue;	// too colse or far away

			// check visibility
			trace_t tr;
			UTIL_TraceLine( vCamPos, pPlayer->GetAbsOrigin(), MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr  );

			if ( tr.fraction < 1.0 )
				continue;	// not visible for camera

			nCount++;

			// remember closest player
			if ( dist <  flClosestPlayerDist )
			{
				iClosestPlayer = pPlayer->entindex();
				flClosestPlayerDist = dist;
			}

			Vector v1; AngleVectors( pPlayer->EyeAngles(), &v1 );

			// check players orientation towards camera
			Vector v2 = vCamPos - vPlayerPos;
			VectorNormalize( v2 );

			// player/camera cost function:
			flRank += ( 1.0f/std::sqrt(dist) ) * WeightedAngle( v1, v2 );

			vDistribution += v2;
		}

		if ( nCount > 0 )
		{
			// normalize distribution
			flRank *= VectorLength( vDistribution ) / nCount; 
		}
		
		IGameEvent *event = gameeventmanager->CreateEvent("hltv_rank_camera");

		if ( event )
		{
			event->SetFloat("rank", flRank );
			event->SetInt("index",  iCameraIndex ); // index in m_pFixedCameras
			event->SetInt("target",  iClosestPlayer ); // ent index
			gameeventmanager->FireEvent( event );
		}
	}
}

void CHLTVDirector::BuildActivePlayerList()
{
	// first build list of all active players

	m_nNumActivePlayers = 0;

	for ( int i =1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsAlive() )
			continue;

		if ( pPlayer->IsObserver() )
			continue;

		if ( pPlayer->GetTeamNumber() <= TEAM_SPECTATOR )
			continue;
        
		m_pActivePlayers[m_nNumActivePlayers] = pPlayer;
		m_nNumActivePlayers++;
	}
}

void CHLTVDirector::AnalyzePlayers()
{
	// build list of current active players
	BuildActivePlayerList();

	// analyzes every active player

	InitRandomOrder( m_nNumActivePlayers );
	
	for ( int i = 0; i<m_nNumActivePlayers; i++ )
	{
		int iPlayerIndex = s_RndOrder[i];

		CBasePlayer *pPlayer = m_pActivePlayers[ iPlayerIndex ];

		float	flRank = 0.0f;
		int		iBestFacingPlayer = 0;
		float	flBestFacingPlayer = 0.0f;
		int		nCount = 0; // Number of visible targets
		Vector	vDistribution; vDistribution.Init(); // distribution of targets
		
		Vector vCamPos = pPlayer->GetAbsOrigin();

		Vector v1; AngleVectors( pPlayer->EyeAngles(), &v1 );

		v1 *= -1; // inverted

		for ( int j=0; j<m_nNumActivePlayers; j++ )
		{
			if ( iPlayerIndex == j )
				continue;  // don't check against itself
			
			CBasePlayer *pOtherPlayer = m_pActivePlayers[j];

			Vector vPlayerPos = pOtherPlayer->GetAbsOrigin();

			float dist = VectorLength( vPlayerPos - vCamPos );

			if ( dist > 1024.0f || dist < 4.0f )
				continue;	// too close or far away

			// check visibility
			trace_t tr;
			UTIL_TraceLine( vCamPos, pOtherPlayer->GetAbsOrigin(), MASK_SOLID, pOtherPlayer, COLLISION_GROUP_NONE, &tr  );

			if ( tr.fraction < 1.0 )
				continue;	// not visible for camera

			nCount++;

			// check players orientation towards camera
			Vector v2; AngleVectors( pOtherPlayer->EyeAngles(), &v2 );

			float facing = WeightedAngle( v1, v2 );

			// remember closest player
			if ( facing > flBestFacingPlayer )
			{
				iBestFacingPlayer = pOtherPlayer->entindex();
				flBestFacingPlayer = facing;
			}

			// player/camera cost function:
			flRank += ( 1.0f/std::sqrt(dist) ) * facing;

			vDistribution += v2;
		}

		if ( nCount > 0 )
		{
			float flDistribution =  VectorLength( vDistribution ) / nCount; // normalize distribution
			flRank *= flDistribution;
		}

		IGameEvent *event = gameeventmanager->CreateEvent("hltv_rank_entity");
		if ( event )
		{
			event->SetInt("index",  pPlayer->entindex() );
			event->SetFloat("rank", flRank );
			event->SetInt("target",  iBestFacingPlayer ); // ent index
			gameeventmanager->FireEvent( event );
		}
	}
}

class CBasePlayer;
class CUserCmd;
class IGameSystem;

// Purpose: Deals with singleton

// enable this #define (here or in your vpc) to have 
// each GameSystem accounted for individually in the vprof.
// You must enable VPROF_LEVEL 1 too.
// #define VPROF_ACCOUNT_GAMESYSTEMS

// Pointer to a member method of IGameSystem
typedef void (IGameSystem::*GameSystemFunc_t)();

// Pointer to a member method of IGameSystem
typedef void (IGameSystemPerFrame::*PerFrameGameSystemFunc_t)();

// Used to invoke a method of all added Game systems in order
static void InvokeMethod( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in order
static void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in reverse order
static void InvokeMethodReverseOrder( GameSystemFunc_t f );

// Used to invoke a method of all added Game systems in order
static void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed = 0 );

static bool s_bSystemsInitted = false; 

// List of all installed Game systems
static std::vector< IGameSystem* > s_GameSystems;
// List of all installed Game systems
static std::vector< IGameSystemPerFrame* > s_GameSystemsPerFrame;

// The map name
static char* s_pMapName = 0;

static CBasePlayer *s_pRunCommandPlayer = nullptr;
static CUserCmd *s_pRunCommandUserCmd = nullptr;

//-----------------------------------------------------------------------------
// Auto-registration of game systems
//-----------------------------------------------------------------------------
static	CAutoGameSystem *s_pSystemList = nullptr;

CAutoGameSystem::CAutoGameSystem( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pSystemList;
		s_pSystemList = this;
	}
}

static	CAutoGameSystemPerFrame *s_pPerFrameSystemList = nullptr;

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
CAutoGameSystemPerFrame::CAutoGameSystemPerFrame( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pPerFrameSystemList;
		s_pPerFrameSystemList = this;
	}
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystem::~IGameSystem()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystemPerFrame::~IGameSystemPerFrame()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// Adds a system to the list of systems to run
//-----------------------------------------------------------------------------
void IGameSystem::Add( IGameSystem* pSys )
{
	s_GameSystems.push_back( pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		s_GameSystemsPerFrame.push_back( static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes a system from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::Remove( IGameSystem* pSys )
{
	ks::erase_first( s_GameSystems, pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		ks::erase_first( s_GameSystemsPerFrame, static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes *all* systems from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::RemoveAll(  )
{
	s_GameSystems.clear();
	s_GameSystemsPerFrame.clear();
}

//-----------------------------------------------------------------------------
// Client systems can use this to get at the map name
//-----------------------------------------------------------------------------
char const*	IGameSystem::MapName()
{
	return s_pMapName;
}

CBasePlayer *IGameSystem::RunCommandPlayer()
{
	return s_pRunCommandPlayer;
}

CUserCmd *IGameSystem::RunCommandUserCmd()
{
	return s_pRunCommandUserCmd;
}

//-----------------------------------------------------------------------------
// Invokes methods on all installed game systems
//-----------------------------------------------------------------------------
bool IGameSystem::InitAllSystems()
{
	int i;

	{
		// first add any auto systems to the end
		CAutoGameSystem *pSystem = s_pSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}
			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}

	{
		CAutoGameSystemPerFrame *pSystem = s_pPerFrameSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}

			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}
	// Now remember that we are initted so new CAutoGameSystems will add themselves automatically.
	s_bSystemsInitted = true;

	// PS3: haul the fontlib into memory; some systems (eg vgui) need it.
	for ( i = 0; i < std::ssize( s_GameSystems ); ++i )
	{
		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();

		IGameSystem *sys = s_GameSystems[i];

		bool valid = sys->Init();

		if ( !valid )
		{
			DevWarning( 1, "Failed to load %s\n", sys->Name() );
			return false;
		}
	}

	return true;
}

void IGameSystem::PostInitAllSystems( void )
{
	InvokeMethod( &IGameSystem::PostInit, "PostInit" );
}

void IGameSystem::ShutdownAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::Shutdown );
}

void IGameSystem::LevelInitPreEntityAllSystems( char const* pMapName )
{
	// Store off the map name
	if ( s_pMapName )
	{
		delete[] s_pMapName;
	}

	int len = V_strlen(pMapName) + 1;
	s_pMapName = new char [ len ];
	V_strncpy( s_pMapName, pMapName, len );
	
	InvokeMethodTickProgress( &IGameSystem::LevelInitPreEntity, "LevelInitPreEntity" );
}

void IGameSystem::LevelInitPostEntityAllSystems( void )
{
	InvokeMethod( &IGameSystem::LevelInitPostEntity, "LevelInitPostEntity" );
}

void IGameSystem::LevelShutdownPreEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPreEntity );
}

void IGameSystem::LevelShutdownPostEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPostEntity );

	if ( s_pMapName )
	{
		delete[] s_pMapName;
		s_pMapName = 0;
	}
}

void IGameSystem::OnSaveAllSystems()
{
	InvokeMethod( &IGameSystem::OnSave );
}

void IGameSystem::OnRestoreAllSystems()
{
	InvokeMethod( &IGameSystem::OnRestore );
}

void IGameSystem::SafeRemoveIfDesiredAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::SafeRemoveIfDesired );
}

void IGameSystem::FrameUpdatePreEntityThinkAllSystems()
{
	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePreEntityThink );
}

void IGameSystem::FrameUpdatePostEntityThinkAllSystems()
{
	SafeRemoveIfDesiredAllSystems();

	InvokePerFrameMethod( &IGameSystemPerFrame::FrameUpdatePostEntityThink );
}

void IGameSystem::PreClientUpdateAllSystems() 
{
	InvokePerFrameMethod( &IGameSystemPerFrame::PreClientUpdate );
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethod( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	MDLCACHE_COARSE_LOCK();
	MDLCACHE_CRITICAL_SECTION();
	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();
		(sys->*f)();
	}
}
//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystemsPerFrame );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystemPerFrame *sys  = s_GameSystemsPerFrame[i];
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in reverse order
//-----------------------------------------------------------------------------
void InvokeMethodReverseOrder( GameSystemFunc_t f )
{
	int i;
	int c = std::ssize( s_GameSystems );
	MDLCACHE_CRITICAL_SECTION();
	for ( i = c; --i >= 0; )
	{
		IGameSystem *sys = s_GameSystems[i];
		(sys->*f)();
	}
}

class CBaseEntity;
class CPointCamera;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Link between entities and cameras
//-----------------------------------------------------------------------------
class CInfoCameraLink : public CLogicalEntity
{
	DECLARE_CLASS( CInfoCameraLink, CLogicalEntity );
 	DECLARE_DATADESC();

public:
	CInfoCameraLink();
	~CInfoCameraLink();

	void Activate_Impl();

private:
	[[= ks::reflect::Input{ .name = "SetCamera", .type = FIELD_STRING } ]] void InputSetCamera(inputdata_t &inputdata);
	void InputSetTargetEntity(inputdata_t &inputdata);
	void SetCameraByName(const char *szName);

	CHandle<CPointCamera> m_hCamera;
	EHANDLE m_hTargetEntity;
	[[= ks::reflect::Key{ .name = "PointCamera" } ]] string_t m_strPointCamera;

	friend CBaseEntity *CreateInfoCameraLink( CBaseEntity *pTarget, CPointCamera *pCamera );
	friend void PointCameraSetupVisibility( CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize );
};

//-----------------------------------------------------------------------------
// List of all info camera links
//-----------------------------------------------------------------------------
std::list<CInfoCameraLink *> g_InfoCameraLinkList;	// list: UTIL_Remove may unlink during iteration

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CInfoCameraLink )

LINK_ENTITY_TO_CLASS( info_camera_link, CInfoCameraLink );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CInfoCameraLink::CInfoCameraLink()
{		
	g_InfoCameraLinkList.push_back( this );
}

CInfoCameraLink::~CInfoCameraLink()
{
	g_InfoCameraLinkList.remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned and after a load game.
//-----------------------------------------------------------------------------
void CInfoCameraLink::Activate_Impl()
{
	BaseClass::Activate_Impl();

	// Checks necessary to prevent interference with CreateInfoCameraLink
	if ( !m_hCamera )
	{
		SetCameraByName( STRING(m_strPointCamera) );
	}

	if ( !m_hTargetEntity )
	{
		m_hTargetEntity = gEntList.FindEntityByName( nullptr, STRING(m_target) );
	}
}

void CInfoCameraLink::SetCameraByName(const char *szName)
{
	CBaseEntity *pBaseEnt = gEntList.FindEntityByName( nullptr, szName );
	if( pBaseEnt )
	{
		m_hCamera = dynamic_cast<CPointCamera *>( pBaseEnt );
		if ( m_hCamera )
		{
			// Keep the camera name consistent for save/load
			m_strPointCamera = MAKE_STRING( szName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoCameraLink::InputSetCamera(inputdata_t &inputdata)
{
	SetCameraByName( inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CreateInfoCameraLink( CBaseEntity *pTarget, CPointCamera *pCamera )
{
	CInfoCameraLink *pInfoCameraLink = (CInfoCameraLink*)CreateEntityByName( "info_camera_link" );
	if ( !pInfoCameraLink )
		return nullptr;

	pInfoCameraLink->m_hCamera = pCamera;
	pInfoCameraLink->m_hTargetEntity = pTarget;
	pInfoCameraLink->Spawn();
	return pInfoCameraLink;
}

//-----------------------------------------------------------------------------
// Sets up visibility 
//-----------------------------------------------------------------------------
void PointCameraSetupVisibility( CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize )
{
	for ( CPointCamera *pCameraEnt = GetPointCameraList(); pCameraEnt != nullptr; pCameraEnt = pCameraEnt->m_pNext )
	{
		pCameraEnt->SetActive( false );
	}
	
	for ( auto it = g_InfoCameraLinkList.begin(), itNext = it; it != g_InfoCameraLinkList.end(); it = itNext )
	{
		itNext = std::next( it );
		CInfoCameraLink *pLink = *it;

		CBaseEntity *pTargetEnt = pLink->m_hTargetEntity;
		if ( !pTargetEnt )
		{
			UTIL_Remove( pLink );
			continue;
		}

		// Don't bother if it's not visible
		if ( pTargetEnt->IsEffectActive( EF_NODRAW ) )
			continue;

		if ( !pTargetEnt->NetworkProp()->IsInPVS( pPlayer->edict(), pvs, pvssize ) )
			continue;

		if ( engine->CheckAreasConnected( area, pTargetEnt->NetworkProp()->AreaNum() ) )
		{
			CPointCamera *pCameraEnt = pLink->m_hCamera;
			if ( pCameraEnt )
			{
				engine->AddOriginToPVS( pCameraEnt->GetAbsOrigin() );
				pCameraEnt->SetActive( true );
			}
		}
	}
}

using namespace ::ks::reflect;

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //

class CInfoOverlayAccessor : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_InfoOverlayAccessor", .base = false } ]]
	[[= ks::reflect::From<"m_iTextureFrameIndex", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CInfoOverlayAccessor, CPointEntity );

	int  	UpdateTransmitState_Impl();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:

	CNetworkVar( int, m_iOverlayID, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "OverlayID" } ]] );
};
							  
// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CInfoOverlayAccessor, DT_InfoOverlayAccessor )

LINK_ENTITY_TO_CLASS( info_overlay_accessor, CInfoOverlayAccessor );

IMPLEMENT_REFLECT_DATAMAP( CInfoOverlayAccessor )

int CInfoOverlayAccessor::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

class CInfoRemarkable;

using namespace ::ks::reflect;

// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.

/// A global list of entities that can be remarked upon.
/// @todo This is an awful, brute force solution. Fix it.
class CRemarkableEntityList : public CAutoGameSystem
{
public:
	CRemarkableEntityList( char const *name ) : CAutoGameSystem( name )
	{}

	virtual void LevelShutdownPostEntity()  
	{ 
		m_list = {};
	}

	void AddEntity( CInfoRemarkable *pItem )
	{
		m_list.push_back( pItem );
	}

	void RemoveEntity( CInfoRemarkable *pItem )
	{
		ks::erase_first( m_list, pItem );
	}
	CInfoRemarkable::tRemarkableList m_list;
};

CRemarkableEntityList g_RemarkableList( "CRemarkableEntityList" );

CInfoRemarkable::tRemarkableList *CInfoRemarkable::GetListOfAllThatIsRemarkable( void )
{
	return &g_RemarkableList.m_list;
}

CInfoRemarkable::~CInfoRemarkable()
{
	g_RemarkableList.RemoveEntity( this );
}

void CInfoRemarkable::Spawn_Impl( void )
{
	g_RemarkableList.AddEntity(this);
	m_iTimesRemarkedUpon = 0;
}

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_remarkable, CInfoRemarkable );

//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CInfoRemarkable )

static factorylist_t s_factories;

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData )
{
	s_factories = sourceData;
}

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData )
{
	destData = s_factories;
}

class CBaseEntity;

using namespace ::ks::reflect;

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission:public CPointEntity
{
public:
	DECLARE_CLASS( CInfoIntermission, CPointEntity );

	void Spawn_Impl( void );
	void Think_Impl( void );
};

void CInfoIntermission::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetLocalAngles( vec3_angle );
	SetNextThink( gpGlobals->curtime + 2 );// let targets spawn !
}

void CInfoIntermission::Think_Impl ( void )
{
	CBaseEntity *pTarget;

	// find my target
	pTarget = gEntList.FindEntityByName( nullptr, m_target );

	if ( pTarget )
	{
		Vector dir = pTarget->GetLocalOrigin() - GetLocalOrigin();
		VectorNormalize( dir );
		QAngle angles;
		VectorAngles( dir, angles );
		SetLocalAngles( angles );
	}
}

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CItem;

using namespace ::ks::reflect;

// Purpose: Handling for the base world item. Most of this was moved from items.cpp.

static constexpr auto ITEM_PICKUP_BOX_BLOAT = 24;

class CWorldItem : public CBaseAnimating
{
public:
	DECLARE_CLASS( CWorldItem, CBaseAnimating );

	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue ); 
	void	Spawn_Impl( void );

	int		m_iType;
};

LINK_ENTITY_TO_CLASS(world_items, CWorldItem);

bool CWorldItem::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "type"))
	{
		m_iType = std::atoi(szValue);
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

void CWorldItem::Spawn_Impl( void )
{
	CBaseEntity *pEntity = nullptr;

	switch (m_iType) 
	{
	case 44: // ITEM_BATTERY:
		pEntity = CBaseEntity::Create( "item_battery", GetLocalOrigin(), GetLocalAngles() );
		break;
	case 45: // ITEM_SUIT:
		pEntity = CBaseEntity::Create( "item_suit", GetLocalOrigin(), GetLocalAngles() );
		break;
	}

	if (!pEntity)
	{
		Warning("unable to create world_item %d\n", m_iType );
	}
	else
	{
		pEntity->m_target = m_target;
		pEntity->SetName( GetEntityName() );
		pEntity->ClearSpawnFlags();
		pEntity->AddSpawnFlags( m_spawnflags );
	}

	UTIL_RemoveImmediate( this );
}

IMPLEMENT_REFLECT_DATAMAP( CItem )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CItem::CItem()
{
	m_bActivateWhenAtRest = false;
}

CItem::~CItem()
{
	if ( m_pConstraint )
	{
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = nullptr;
	}
}

bool CItem::CreateItemVPhysicsObject( void )
{
	// Create the object in the physics system
	int nSolidFlags = GetSolidFlags() | FSOLID_NOT_STANDABLE;
	if ( !m_bActivateWhenAtRest )
	{
		nSolidFlags |= FSOLID_TRIGGER;
	}

	if ( VPhysicsInitNormal( SOLID_VPHYSICS, nSolidFlags, false ) == nullptr )
	{
		SetSolid( SOLID_BBOX );
		AddSolidFlags( nSolidFlags );

		// If it's not physical, drop it to the floor
		if (UTIL_DropToFloor(this, MASK_SOLID) == 0)
		{
			Warning( "Item %s fell out of level at %f,%f,%f\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
			UTIL_Remove( this );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem::Spawn_Impl( void )
{
	SetNetworkQuantizeOriginAngAngles( true );

	if ( g_pGameRules->IsAllowedToSpawn( this ) == false )
	{
		UTIL_Remove( this );
		return;
	}

	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );
	SetBlocksLOS( false );
	AddEFlags( EFL_NO_ROTORWASH_PUSH );
	
	// This will make them not collide with the player, but will collide
	// against other items + weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	if( HasBloatedCollision() )
	{
		CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT );
	}
	SetTouch(&CItem::ItemTouch);

	if ( CreateItemVPhysicsObject() == false )
		return;

	m_takedamage = DAMAGE_EVENTS_ONLY;

	// Constrained start?
	if ( HasSpawnFlags( SF_ITEM_START_CONSTRAINED ) )
	{
		//Constrain the weapon in place
		IPhysicsObject *pReferenceObject, *pAttachedObject;

		pReferenceObject = g_PhysWorldObject;
		pAttachedObject = VPhysicsGetObject();

		if ( pReferenceObject && pAttachedObject )
		{
			constraint_fixedparams_t fixed;
			fixed.Defaults();
			fixed.InitWithCurrentObjectState( pReferenceObject, pAttachedObject );

			fixed.constraint.forceLimit	= lbs2kg( 10000 );
			fixed.constraint.torqueLimit = lbs2kg( 10000 );

			m_pConstraint = physenv->CreateFixedConstraint( pReferenceObject, pAttachedObject, nullptr, fixed );

			m_pConstraint->SetGameData( (void *) this );
		}
	}

	Vector origin = GetAbsOrigin();
	QAngle angles = GetAbsAngles();
	NetworkQuantize( origin, angles );
	SetAbsOrigin( origin );
	SetAbsAngles( angles );
}

void CItem::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer )
	{
		pPlayer->PickupObject( this );
	}
}

extern int gEvilImpulse101;

//-----------------------------------------------------------------------------
// Activate when at rest, but don't allow pickup until then
//-----------------------------------------------------------------------------
void CItem::ActivateWhenAtRest( float flTime /* = 0.5f */ )
{
	RemoveSolidFlags( FSOLID_TRIGGER );
	m_bActivateWhenAtRest = true;
	SetThink( &CItem::ComeToRest );
	SetNextThink( gpGlobals->curtime + flTime );
}

//-----------------------------------------------------------------------------
// Become touchable when we are at rest
//-----------------------------------------------------------------------------
void CItem::OnEntityEvent_Impl( EntityEvent_t event, void *pEventData )
{
	BaseClass::OnEntityEvent_Impl( event, pEventData );

	if( event == ENTITY_EVENT_WATER_TOUCH && m_bActivateWhenAtRest )
	{
		// Check if we are stomping on someone else's think function
		contract_assert( m_pfnThink == nullptr || m_pfnThink == static_cast <void (CBaseEntity::*)(void)>(&CItem::ComeToRest) );

		// Delay rest for a sec, to avoid changing collision 
		// properties inside a collision callback.
		SetThink( &CItem::ComeToRest );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Become touchable when we are at rest
//-----------------------------------------------------------------------------
void CItem::ComeToRest( void )
{
	if ( m_bActivateWhenAtRest )
	{
		m_bActivateWhenAtRest = false;
		AddSolidFlags( FSOLID_TRIGGER );
		SetThink( nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to tell whether an item may be picked up by the player.  This
//			accounts for solid obstructions being in the way.
// Input  : *pItem - item in question
//			*pPlayer - player attempting the pickup
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_ItemCanBeTouchedByPlayer( CBaseEntity *pItem, CBasePlayer *pPlayer )
{
	if ( pItem == nullptr || pPlayer == nullptr )
		return false;

	// For now, always allow a vehicle riding player to pick up things they're driving over
	

	// Get our test positions
	Vector vecStartPos;
	IPhysicsObject *pPhysObj = pItem->VPhysicsGetObject();
	if ( pPhysObj != nullptr )
	{
		// Use the physics hull's center
		QAngle vecAngles;
		pPhysObj->GetPosition( &vecStartPos, &vecAngles );
	}
	else
	{
		// Use the generic bbox center
		vecStartPos = pItem->CollisionProp()->WorldSpaceCenter();
	}

	Vector vecEndPos = pPlayer->EyePosition();

	// FIXME: This is the simple first try solution towards the problem.  We need to take edges and shape more into account
	//		  for this to be fully robust.

	// Trace between to see if we're occluded
	trace_t tr;
	CTraceFilterSkipTwoEntities filter( pPlayer, pItem, COLLISION_GROUP_PLAYER_MOVEMENT );
	UTIL_TraceLine( vecStartPos, vecEndPos, MASK_SOLID, &filter, &tr );

	// Occluded
	// FIXME: For now, we exclude starting in solid because there are cases where this doesn't matter
	if ( tr.fraction < 1.0f )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Whether or not the item can be touched and picked up by the player, taking
//			into account obstructions and other hinderances
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CItem::ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer )
{
	return UTIL_ItemCanBeTouchedByPlayer( this, pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pOther - 
//-----------------------------------------------------------------------------
void CItem::ItemTouch( CBaseEntity *pOther )
{
	ItemTouchInternal( pOther, false );
}

void CItem::ItemForceTouch( CBaseEntity *pOther )
{
	ItemTouchInternal( pOther, true );
}

void CItem::ItemTouchInternal( CBaseEntity *pOther, bool bForceTouch )
{
	// Vehicles can touch items + pick them up
	

	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// Must be a valid pickup scenario (no blocking). Though this is a more expensive
	// check than some that follow, this has to be first Obecause it's the only one
	// that inhibits firing the output OnCacheInteraction.
	if ( !bForceTouch && ItemCanBeTouchedByPlayer( pPlayer ) == false )
		return;

	m_OnCacheInteraction.FireOutput(pOther, this);

	// Can I even pick stuff up?
	if ( !pPlayer->IsAllowedToPickupWeapons() )
		return;

	// ok, a player is touching this item, but can he have it?
	if ( !g_pGameRules->CanHaveItem( pPlayer, this ) )
	{
		// no? Ignore the touch.
		return;
	}

	if ( MyTouch( pPlayer ) )
	{
		m_OnPlayerTouch.FireOutput(pOther, this);

		SetTouch( nullptr );
		SetThink( nullptr );

		// player grabbed the item. 
		g_pGameRules->PlayerGotItem( pPlayer, this );
		if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_YES )
		{
			Respawn(); 
		}
		else
		{
			UTIL_Remove( this );

		}
	}
	else if (gEvilImpulse101)
	{
		UTIL_Remove( this );
	}
}

CBaseEntity* CItem::Respawn_Impl( void )
{
	SetTouch( nullptr );
	AddEffects( EF_NODRAW );

	VPhysicsDestroyObject();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );

	UTIL_SetOrigin( this, g_pGameRules->VecItemRespawnSpot( this ) );// blip to whereever you should respawn.
	SetAbsAngles( g_pGameRules->VecItemRespawnAngles( this ) );// set the angles.

	UTIL_DropToFloor( this, MASK_SOLID );

	RemoveAllDecals(); //remove any decals

	SetThink ( &CItem::Materialize );
	SetNextThink( gpGlobals->curtime + g_pGameRules->FlItemRespawnTime( this ) );
	return this;
}

void CItem::Materialize( void )
{
	CreateItemVPhysicsObject();

	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.

		// this sound no longer exists
		// EmitSound( "Item.Materialize" );
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	SetTouch( &CItem::ItemTouch );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem::Precache_Impl()
{
	BaseClass::Precache_Impl();

	// this sound no longer exists
	// PrecacheScriptSound( "Item.Materialize" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//			PICKED_UP_BY_CANNON - 
//-----------------------------------------------------------------------------
void CItem::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_OnCacheInteraction.FireOutput(pPhysGunUser, this);

	if ( reason == PICKED_UP_BY_CANNON )
	{
		// Expand the pickup box
		CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT * 2 );

		if( m_pConstraint != nullptr )
		{
			physenv->DestroyConstraint( m_pConstraint );
			m_pConstraint = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//			reason - 
//-----------------------------------------------------------------------------
void CItem::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	// Restore the pickup box to the original
	CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto LIGHTGLOW_MAXDIST_BITS = 16;
static constexpr auto LIGHTGLOW_MAXDIST_MAX_VALUE = ((1 << LIGHTGLOW_MAXDIST_BITS)-1);

static constexpr auto LIGHTGLOW_OUTERMAXDIST_BITS = 16;
static constexpr auto LIGHTGLOW_OUTERMAXDIST_MAX_VALUE = ((1 << LIGHTGLOW_OUTERMAXDIST_BITS)-1);

void SendProxy_Angles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class CLightGlow : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_LightGlow", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .enc = ks::reflect::WireEnc::QAngles }, SendProxy_Angles>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CLightGlow, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CLightGlow();
					
	void	Spawn_Impl( void );
	void	Activate_Impl( void );
	int		UpdateTransmitState_Impl( void );

	[[= ks::reflect::Input{ .name = "Color", .type = FIELD_COLOR32 } ]] void InputColor(inputdata_t &data);

public:
	CNetworkVar( int, m_nHorizontalSize, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "HorizontalGlowSize" } ]] );
	CNetworkVar( int, m_nVerticalSize, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "VerticalGlowSize" } ]] );
	CNetworkVar( int, m_nMinDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "MinDist" } ]] );
	CNetworkVar( int, m_nMaxDist, [[= ks::reflect::Net{ .bits = LIGHTGLOW_MAXDIST_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "MaxDist" } ]] );
	CNetworkVar( int, m_nOuterMaxDist, [[= ks::reflect::Net{ .bits = LIGHTGLOW_OUTERMAXDIST_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "OuterMaxDist" } ]] );

	CNetworkVar( float, m_flGlowProxySize, [[= ks::reflect::Net{ .bits = 6, .low = 0.0f, .high = 64.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "GlowProxySize" } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE, .wire = "HDRColorScale" } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
};

extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_REFLECT_SERVERCLASS( CLightGlow, DT_LightGlow )

LINK_ENTITY_TO_CLASS( env_lightglow, CLightGlow );

IMPLEMENT_REFLECT_DATAMAP( CLightGlow )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CLightGlow::CLightGlow( void )
{
	m_nHorizontalSize = 0.0f;
	m_nVerticalSize = 0.0f;
	m_nMinDist = 0.0f;
	m_nMaxDist = 0.0f;

	m_flGlowProxySize = 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightGlow::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	// No model but we still need to force this!
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: Always transmit light glows to clients to avoid spikes as we enter
//			or leave PVS. Done because we often have many glows in an area.
//-----------------------------------------------------------------------------
int CLightGlow::UpdateTransmitState_Impl( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightGlow::Activate_Impl()
{
	BaseClass::Activate_Impl();

	if ( m_nMaxDist > LIGHTGLOW_MAXDIST_MAX_VALUE )
	{
		Warning( "env_lightglow maxdist too large (%d should be %d).\n", m_nMaxDist.Get(), LIGHTGLOW_MAXDIST_MAX_VALUE );
		m_nMaxDist = LIGHTGLOW_MAXDIST_MAX_VALUE;
	}

	if ( m_nOuterMaxDist > LIGHTGLOW_OUTERMAXDIST_MAX_VALUE )
	{
		Warning( "env_lightglow outermaxdist too large (%d should be %d).\n", m_nOuterMaxDist.Get(), LIGHTGLOW_OUTERMAXDIST_MAX_VALUE );
		m_nOuterMaxDist = LIGHTGLOW_OUTERMAXDIST_MAX_VALUE;
	}
}

void CLightGlow::InputColor(inputdata_t &inputdata)
{
	m_clrRender = inputdata.value.Color32();
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: spawn and think functions for editor-placed lights

LINK_ENTITY_TO_CLASS( light, CLight );

IMPLEMENT_REFLECT_DATAMAP( CLight )

//
// Cache user-entity-field values until spawn is called.
//
bool CLight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "pitch"))
	{
		QAngle angles = GetAbsAngles();
		angles.x = std::atof(szValue);
		SetAbsAngles( angles );
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

// Light entity
// If targeted, it will toggle between on or off.
void CLight::Spawn_Impl( void )
{
	if (!GetEntityName())
	{       // inert light
		UTIL_Remove( this );
		return;
	}
	
	if (m_iStyle >= 32)
	{
		if ( m_iszPattern == NULL_STRING && m_iDefaultStyle > 0 )
		{
			m_iszPattern = MAKE_STRING(GetDefaultLightstyleString(m_iDefaultStyle));
		}

		if (FBitSet(m_spawnflags, SF_LIGHT_START_OFF))
			engine->LightStyle(m_iStyle, "a");
		else if (m_iszPattern != NULL_STRING)
			engine->LightStyle(m_iStyle, (char *)STRING( m_iszPattern ));
		else
			engine->LightStyle(m_iStyle, "m");
	}
}

void CLight::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (m_iStyle >= 32)
	{
		if ( !ShouldToggle( useType, !FBitSet(m_spawnflags, SF_LIGHT_START_OFF) ) )
			return;

		Toggle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turn the light on
//-----------------------------------------------------------------------------
void CLight::TurnOn( void )
{
	if ( m_iszPattern != NULL_STRING )
	{
		engine->LightStyle( m_iStyle, (char *) STRING( m_iszPattern ) );
	}
	else
	{
		engine->LightStyle( m_iStyle, "m" );
	}

	CLEARBITS( m_spawnflags, SF_LIGHT_START_OFF );
}

//-----------------------------------------------------------------------------
// Purpose: Turn the light off
//-----------------------------------------------------------------------------
void CLight::TurnOff( void )
{
	engine->LightStyle( m_iStyle, "a" );
	SETBITS( m_spawnflags, SF_LIGHT_START_OFF );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the light on/off
//-----------------------------------------------------------------------------
void CLight::Toggle( void )
{
	//Toggle it
	if ( FBitSet( m_spawnflags, SF_LIGHT_START_OFF ) )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle the "turnon" input handler
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLight::InputTurnOn( inputdata_t &inputdata )
{
	TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose: Handle the "turnoff" input handler
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLight::InputTurnOff( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose: Handle the "toggle" input handler
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLight::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting a light pattern
//-----------------------------------------------------------------------------
void CLight::InputSetPattern( inputdata_t &inputdata )
{
	m_iszPattern = inputdata.value.StringID();
	engine->LightStyle(m_iStyle, (char *)STRING( m_iszPattern ));

	// Light is on if pattern is set
	CLEARBITS(m_spawnflags, SF_LIGHT_START_OFF);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for fading from first value in old pattern to 
//			first value in new pattern
//-----------------------------------------------------------------------------
void CLight::InputFadeToPattern( inputdata_t &inputdata )
{
	m_iCurrentFade	= (STRING(m_iszPattern))[0];
	m_iTargetFade	= inputdata.value.String()[0];
	m_iszPattern	= inputdata.value.StringID();
	SetThink(&CLight::FadeThink);
	SetNextThink( gpGlobals->curtime );

	// Light is on if pattern is set
	CLEARBITS(m_spawnflags, SF_LIGHT_START_OFF);
}

//------------------------------------------------------------------------------
// Purpose : Fade light to new starting pattern value then stop thinking
//------------------------------------------------------------------------------
void CLight::FadeThink(void)
{
	if (m_iCurrentFade < m_iTargetFade)
	{
		m_iCurrentFade++;
	}
	else if (m_iCurrentFade > m_iTargetFade)
	{
		m_iCurrentFade--;
	}

	// If we're done fading instantiate our light pattern and stop thinking
	if (m_iCurrentFade == m_iTargetFade)
	{
		engine->LightStyle(m_iStyle, (char *)STRING( m_iszPattern ));
		SetNextThink( TICK_NEVER_THINK );
	}
	// Otherwise instantiate our current fade value and keep thinking
	else
	{
		char sCurString[2];
		sCurString[0] = m_iCurrentFade;
		sCurString[1] = 0;
		engine->LightStyle(m_iStyle, sCurString);

		// UNDONE: Consider making this settable war to control fade speed
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//
// shut up spawn functions for new spotlights
//
LINK_ENTITY_TO_CLASS_ALIAS( light_spot, CLight );
LINK_ENTITY_TO_CLASS_ALIAS( light_glspot, CLight );
LINK_ENTITY_TO_CLASS_ALIAS( light_directional, CLight );

class CEnvLight : public CLight
{
public:
	DECLARE_CLASS( CEnvLight, CLight );

	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue ); 
	void	Spawn_Impl( void );
};

LINK_ENTITY_TO_CLASS( light_environment, CEnvLight );

bool CEnvLight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	bool bSuccess = true;
	if (FStrEq(szKeyName, "_light"))
	{
		int tmp[4];
		V_StringToIntArray( tmp, 4, szValue );
		CCascadeLight::SetLightColor( tmp[0], tmp[1], tmp[2], tmp[3] );
	}
	else if ( FStrEq( szKeyName, "_ambient" ) )
	{
		// nothing
	}
	else
	{
		bSuccess = BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	if ( FStrEq( szKeyName, "pitch" ) )
	{
		CCascadeLight::SetEnvLightShadowPitch( std::atof( szValue ) );
	}

	CCascadeLight::SetEnvLightShadowAngles( GetAbsAngles() );
		
	return bSuccess;
}

void CEnvLight::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl( );
		
	CCascadeLight::SetEnvLightShadowAngles( GetAbsAngles() );
}

KS_ENTITY_ABSTRACT_KIND( CItem )

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Player for HL2.

LINK_ENTITY_TO_CLASS( logic_eventlistener, CLogicEventListener);

IMPLEMENT_REFLECT_DATAMAP( CLogicEventListener )

class CScriptEventTableWriter : public IGameEventVisitor2
{
public:
	CScriptEventTableWriter( IScriptVM* pVM, HSCRIPT table )
	: mVM(pVM), mTable(table)
	{}

	// IGameEventVisitor2
	bool VisitString( const char* name, const char* value )
	{
		mVM->SetValue( mTable, name, value );
		return true;
	}

	bool VisitFloat( const char* name, float value )
	{
		mVM->SetValue( mTable, name, value );
		return true;
	}

	bool VisitInt( const char* name, int value )
	{
		mVM->SetValue( mTable, name, value );
		return true;
	}

	bool VisitBool( const char*name, bool value )
	{
		mVM->SetValue( mTable, name, value );
		return true;
	}

	// TODO: Uint64, wstring?

private:
	IScriptVM* mVM;
	HSCRIPT mTable;
};

//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CLogicEventListener::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	ListenForGameEvent( STRING(m_iszEventName) );
}

//-----------------------------------------------------------------------------
// Purpose: FireGameEvent
//-----------------------------------------------------------------------------
void CLogicEventListener::FireGameEvent( IGameEvent *event )
{
	if ( !m_bIsEnabled )
		return;

	const char *name = event->GetName();
	if ( V_strcmp( name, STRING(m_iszEventName) ) == 0 )
	{
		if ( m_nTeam > 0 )
		{
			int nPlayerTeam = TEAM_INVALID;
			int	playerId = event->GetInt( "userid", 0 );
			for ( int i = 0; i <= MAX_PLAYERS; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->GetUserID() == playerId )
				{
					nPlayerTeam = pPlayer->GetTeamNumber();
					break;
				}
			}

			if ( nPlayerTeam != m_nTeam )
				return;
		}

		if ( m_bFetchEventData && ValidateScriptScope() )
		{
			HSCRIPT entityTable = m_ScriptScope;
			IScriptVM* vm = m_ScriptScope.GetVM();

			ScriptVariant_t table;
			vm->CreateTable( table );
			{
				CScriptEventTableWriter tableWriter( vm, table );
				event->ForEventData( &tableWriter );
			}
			vm->SetValue( entityTable, "event_data", table );
			vm->ReleaseValue( table );
		}

		m_OnEventFired.FireOutput( nullptr, nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLogicEventListener::InputEnable( inputdata_t &inputdata )
{
	m_bIsEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLogicEventListener::InputDisable( inputdata_t &inputdata )
{
	m_bIsEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: an event listener that listens specifically for item_equip and sends an output if the weapon matches
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( logic_eventlistener_itemequip, CLogicEventListenerItemEquip);

IMPLEMENT_REFLECT_DATAMAP( CLogicEventListenerItemEquip )

	//-----------------------------------------------------------------------------
	// Purpose: Spawn
	//-----------------------------------------------------------------------------
	void CLogicEventListenerItemEquip::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	ListenForGameEvent( "item_equip" );
}

//-----------------------------------------------------------------------------
// Purpose: FireGameEvent
//-----------------------------------------------------------------------------
void CLogicEventListenerItemEquip::FireGameEvent( IGameEvent *event )
{
	if ( !m_bIsEnabled )
		return;

	const char *name = event->GetName();
	if ( V_strcmp( name, "item_equip" ) == 0 )
	{
		if ( m_nTeam > 0 )
		{
			int nPlayerTeam = TEAM_INVALID;
			int	playerId = event->GetInt( "userid", 0 );
			for ( int i = 0; i <= MAX_PLAYERS; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->GetUserID() == playerId )
				{
					nPlayerTeam = pPlayer->GetTeamNumber();
					break;
				}
			}

			if ( nPlayerTeam != m_nTeam )
				return;
		}

		if ( m_szWeaponClassname != NULL_STRING )
		{
			if ( V_strcmp( m_szWeaponClassname.ToCStr(), event->GetString( "item", "" ) ) != 0 )
				return;
		}

		if ( m_nWeaponType > -1 )
		{
			int	nWepType = event->GetInt( "weptype", 0 );
			if ( nWepType != m_nWeaponType )
				return;
		}

		m_OnEventFired.FireOutput( nullptr, nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLogicEventListenerItemEquip::InputEnable( inputdata_t &inputdata )
{
	m_bIsEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CLogicEventListenerItemEquip::InputDisable( inputdata_t &inputdata )
{
	m_bIsEnabled = false;
}

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: This will measure the movement of a target entity and move
// another entity to match the movement of the first.

//-----------------------------------------------------------------------------
// This will measure the movement of a target entity and move
// another entity to match the movement of the first.
//-----------------------------------------------------------------------------
class CLogicMeasureMovement : public CLogicalEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CLogicMeasureMovement, CLogicalEntity );

public:
	void Activate_Impl();

private:
	void SetMeasureTarget( const char *pName );
	void SetMeasureReference( const char *pName );
	void SetTarget( const char *pName );
	void SetTargetReference( const char *pName );

	[[= ks::reflect::Input{ .name = "SetMeasureTarget", .type = FIELD_STRING } ]] void InputSetMeasureTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMeasureReference", .type = FIELD_STRING } ]] void InputSetMeasureReference( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTarget", .type = FIELD_STRING } ]] void InputSetTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTargetReference", .type = FIELD_STRING } ]] void InputSetTargetReference( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTargetScale", .type = FIELD_FLOAT } ]] void InputSetTargetScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	void MeasureThink();

private:
	enum
	{
		MEASURE_POSITION = 0,
		MEASURE_EYE_POSITION,
	};

	[[= ks::reflect::Key{ .name = "MeasureTarget" } ]] string_t m_strMeasureTarget;
	[[= ks::reflect::Key{ .name = "MeasureReference" } ]] string_t m_strMeasureReference;
	[[= ks::reflect::Key{ .name = "TargetReference" } ]] string_t m_strTargetReference;

	EHANDLE m_hMeasureTarget;
	EHANDLE m_hMeasureReference;
	EHANDLE m_hTarget;
	EHANDLE m_hTargetReference;

	[[= ks::reflect::Key{ .name = "TargetScale" } ]] float m_flScale;
	[[= ks::reflect::Key{ .name = "MeasureType" } ]] int m_nMeasureType;
};

LINK_ENTITY_TO_CLASS( logic_measure_movement, CLogicMeasureMovement );

IMPLEMENT_REFLECT_DATAMAP( CLogicMeasureMovement )

//-----------------------------------------------------------------------------
// Methods to change various targets
//-----------------------------------------------------------------------------
void CLogicMeasureMovement::Activate_Impl()
{
	BaseClass::Activate_Impl();

	SetMeasureTarget( STRING(m_strMeasureTarget) );
	SetMeasureReference( STRING(m_strMeasureReference) );
	SetTarget( STRING(m_target) );
	SetTargetReference( STRING(m_strTargetReference) );
	
	SetThink( &CLogicMeasureMovement::MeasureThink );
	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Sets the name
//-----------------------------------------------------------------------------
void CLogicMeasureMovement::SetMeasureTarget( const char *pName )
{
	m_hMeasureTarget = gEntList.FindEntityByName( nullptr, pName );
	if ( !m_hMeasureTarget )
	{
		if ( V_strnicmp( STRING(m_strMeasureTarget), "!player", 8 ) )
		{
			Warning("logic_measure_movement: Unable to find measure target entity %s\n", pName );
		}
	}
}

void CLogicMeasureMovement::SetMeasureReference( const char *pName )
{
	m_hMeasureReference = gEntList.FindEntityByName( nullptr, pName );
	if ( !m_hMeasureReference )
	{
		Warning("logic_measure_movement: Unable to find measure reference entity %s\n", pName );
	}
}

void CLogicMeasureMovement::SetTarget( const char *pName )
{
	m_hTarget = gEntList.FindEntityByName( nullptr, pName );
	if ( !m_hTarget )
	{
		Warning("logic_measure_movement: Unable to find movement target entity %s\n", pName );
	}
}

void CLogicMeasureMovement::SetTargetReference( const char *pName )
{
	m_hTargetReference = gEntList.FindEntityByName( nullptr, pName );
	if ( !m_hTargetReference )
	{
		Warning("logic_measure_movement: Unable to find movement reference entity %s\n", pName );
	}
}

//-----------------------------------------------------------------------------
// Apply movement
//-----------------------------------------------------------------------------
void CLogicMeasureMovement::MeasureThink( )
{
	// FIXME: This is a hack to make measuring !player simpler. The player isn't
	// created at Activate time, so m_hMeasureTarget may be NULL because of that.
	if ( !m_hMeasureTarget.Get() && !V_strnicmp( STRING(m_strMeasureTarget), "!player", 8 ) )
	{
		SetMeasureTarget( STRING(m_strMeasureTarget) );
	}

	// Make sure all entities are valid
	if ( m_hMeasureTarget.Get() && m_hMeasureReference.Get() && m_hTarget.Get() && m_hTargetReference.Get() )
	{
		matrix3x4_t matRefToMeasure, matWorldToMeasure;
		switch( m_nMeasureType )
		{
		case MEASURE_POSITION:
			MatrixInvert( m_hMeasureTarget->EntityToWorldTransform(), matWorldToMeasure );
			break;

		case MEASURE_EYE_POSITION:
			AngleIMatrix( m_hMeasureTarget->EyeAngles(), m_hMeasureTarget->EyePosition(), matWorldToMeasure );
			break;

		// FIXME: Could add attachment point measurement here easily
		}

		ConcatTransforms( matWorldToMeasure, m_hMeasureReference->EntityToWorldTransform(), matRefToMeasure );
		
		// Apply the scale factor
		if ( ( m_flScale != 0.0f ) && ( m_flScale != 1.0f ) )
		{
			Vector vecTranslation;
			MatrixGetColumn( matRefToMeasure, 3, vecTranslation );
			vecTranslation /= m_flScale;
			MatrixSetColumn( vecTranslation, 3, matRefToMeasure );
		}

		// Now apply the new matrix to the new reference point
		matrix3x4_t matMeasureToRef, matNewTargetToWorld;
		MatrixInvert( matRefToMeasure, matMeasureToRef );

		ConcatTransforms( m_hTargetReference->EntityToWorldTransform(), matMeasureToRef, matNewTargetToWorld );

		Vector vecNewOrigin;
		QAngle vecNewAngles;
		MatrixAngles( matNewTargetToWorld, vecNewAngles, vecNewOrigin );
		m_hTarget->SetAbsOrigin( vecNewOrigin );
		m_hTarget->SetAbsAngles( vecNewAngles );
	}

	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Enable, disable
//-----------------------------------------------------------------------------
void CLogicMeasureMovement::InputEnable( inputdata_t &inputdata )
{
	SetThink( &CLogicMeasureMovement::MeasureThink );
	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}

void CLogicMeasureMovement::InputDisable( inputdata_t &inputdata )
{
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
// Methods to change various targets
//-----------------------------------------------------------------------------
void CLogicMeasureMovement::InputSetMeasureTarget( inputdata_t &inputdata )
{
	m_strMeasureTarget = MAKE_STRING( inputdata.value.String() );
	SetMeasureTarget( inputdata.value.String() );
	SetTarget( STRING(m_target) );
	SetTargetReference( STRING(m_strTargetReference) );
}

void CLogicMeasureMovement::InputSetMeasureReference( inputdata_t &inputdata )
{
	m_strMeasureReference = MAKE_STRING( inputdata.value.String() );
	SetMeasureReference( inputdata.value.String() );
}

void CLogicMeasureMovement::InputSetTarget( inputdata_t &inputdata )
{
	m_target = MAKE_STRING( inputdata.value.String() );
	SetTarget( inputdata.value.String() );
}

void CLogicMeasureMovement::InputSetTargetReference( inputdata_t &inputdata )
{
	m_strTargetReference = MAKE_STRING( inputdata.value.String() );
	SetTargetReference( inputdata.value.String() );
}

void CLogicMeasureMovement::InputSetTargetScale( inputdata_t &inputdata )
{
	m_flScale = inputdata.value.Float();
}

class CBaseEntity;
class IEntityListener;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto SF_NAV_START_ON = 0x0001;

enum navproperties_t
{
	NAV_IGNORE = 1<<0,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLogicNavigation : public CLogicalEntity,
						 public IEntityListener
{
	DECLARE_CLASS( CLogicNavigation, CLogicalEntity );

	public: using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	private:
	public: void Activate_Impl( void );
	private:

private:
	public: void UpdateOnRemove_Impl();
	private:

	void OnEntitySpawned( CBaseEntity *pEntity );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata ) { TurnOn(); }
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata ) { TurnOff(); }
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata ) 
	{ 
		if ( m_isOn )
			TurnOff();
		else
			TurnOn();
	}

	void TurnOn();
	void TurnOff();
	void UpdateProperty();

	DECLARE_DATADESC();

	bool				m_isOn;
	navproperties_t		m_navProperty;
};

LINK_ENTITY_TO_CLASS(logic_navigation, CLogicNavigation);

IMPLEMENT_REFLECT_DATAMAP( CLogicNavigation )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKeyName - 
//			*szValue - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CLogicNavigation::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq(szKeyName, "navprop") )
	{
		if ( FStrEq( szValue, "Ignore" ) )
		{
			m_navProperty = NAV_IGNORE;
		}
		else
		{
			DevMsg( 1, "Unknown nav property %s\n", szValue );
		}
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::Activate_Impl()
{
	BaseClass::Activate_Impl();

	if ( HasSpawnFlags( SF_NAV_START_ON ) )
	{
		TurnOn();
		RemoveSpawnFlags( SF_NAV_START_ON );
	}
	else if ( m_isOn )
	{
		gEntList.AddListenerEntity( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::UpdateOnRemove_Impl()
{
	if ( m_isOn )
	{
		gEntList.RemoveListenerEntity( this );
	}

	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::OnEntitySpawned( CBaseEntity *pEntity )
{
	if ( m_isOn && ( m_navProperty & NAV_IGNORE ) && pEntity->NameMatches( m_target ) )
	{
		pEntity->SetNavIgnore();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::TurnOn()
{
	if ( m_isOn )
		return;
	m_isOn = true;
	gEntList.AddListenerEntity( this );
	UpdateProperty();
}

void CLogicNavigation::UpdateProperty()
{
	CBaseEntity *pEntity = nullptr;
	while ( ( pEntity = gEntList.FindEntityByName( pEntity, STRING(m_target) ) ) != nullptr )
	{
		if ( m_isOn )
		{
			if ( m_navProperty & NAV_IGNORE )
				pEntity->SetNavIgnore();
		}
		else
		{
			if ( m_navProperty & NAV_IGNORE )
				pEntity->ClearNavIgnore();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::TurnOff()
{
	if ( !m_isOn )
		return;

	m_isOn = false;
	gEntList.RemoveListenerEntity( this );
	UpdateProperty();
}

class CBaseFilter;
class CLogicPlayerProxy;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Player for HL2.

LINK_ENTITY_TO_CLASS( logic_playerproxy, CLogicPlayerProxy);

IMPLEMENT_REFLECT_DATAMAP( CLogicPlayerProxy )

void CLogicPlayerProxy::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	if ( m_hPlayer == nullptr )
	{
		m_hPlayer = UTIL_GetSinglePlayer();
	}
}

bool CLogicPlayerProxy::PassesDamageFilter_Impl( const CTakeDamageInfo &info )
{
	if (m_hDamageFilter)
	{
		CBaseFilter *pFilter = (CBaseFilter *)(m_hDamageFilter.Get());
		return pFilter->PassesDamageFilter(info);
	}

	return true;
}

void CLogicPlayerProxy::InputSetPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == nullptr )
		return;

	m_hPlayer->SetHealth( inputdata.value.Int() );

}

void CLogicPlayerProxy::InputRequestPlayerHealth( inputdata_t &inputdata )
{
	if ( m_hPlayer == nullptr )
		return;

	m_RequestedPlayerHealth.Set( m_hPlayer->GetHealth(), inputdata.pActivator, inputdata.pCaller );
}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Plays a movie and reports on finish

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLogicPlayMovie : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicPlayMovie, CLogicalEntity );
	DECLARE_DATADESC();

	CLogicPlayMovie( void ) { }
	~CLogicPlayMovie( void ) { }

	void Precache_Impl( void );
	void Spawn_Impl( void );
	
private:

	[[= ks::reflect::Input{ .name = "PlayMovie", .type = FIELD_VOID } ]] void		InputPlayMovie( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "PlayMovieForAllPlayers", .type = FIELD_VOID } ]] void		InputPlayMovieForAllPlayers( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "PlayLevelTransitionMovie", .type = FIELD_VOID } ]] void		InputPlayLevelTransitionMovie( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "FadeAllMovies", .type = FIELD_VOID } ]] void		InputFadeAllMovies( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "__MovieFinished", .type = FIELD_VOID } ]] void		InputMovieFinished( inputdata_t &data );

	[[= ks::reflect::Key{ .name = "MovieFilename" } ]] string_t	m_strMovieFilename;
	[[= ks::reflect::Key{ .name = "allowskip" } ]] bool		m_bAllowUserSkip;
	[[= ks::reflect::Key{ .name = "loopvideo" } ]] bool		m_bLoopVideo;
	[[= ks::reflect::Key{ .name = "fadeintime" } ]] float		m_bFadeInTime;

	[[= ks::reflect::Key{ .name = "OnPlaybackFinished" } ]] COutputEvent	m_OnPlaybackFinished;
};

LINK_ENTITY_TO_CLASS( logic_playmovie, CLogicPlayMovie );

IMPLEMENT_REFLECT_DATAMAP( CLogicPlayMovie )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::Precache_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::Spawn_Impl( void )
{
}

static bool Helper_BIsValidMovieFileString( char const *szFileString )
{
	if ( !szFileString || !*szFileString )
		return false;
	char const *szFileStringStart = szFileString;
	while ( *szFileString )
	{
		if ( !V_isalnum( *szFileString ) ) switch( *szFileString )
		{
		case '-':
		case '_':
		case '/':
		case '.':
			++ szFileString;
			continue;
		default:
			DevWarning( "logic_playmovie has invalid movie filename char '%c' in \"%s\"\n", *szFileString, szFileStringStart );
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayMovie( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING(m_strMovieFilename) ) )
		return;

	const char *szVideoCommand = ( m_bAllowUserSkip ) ? "playvideo_exitcommand" : "playvideo_exitcommand_nointerrupt";
	// Build the hacked string

	char szClientCmd[256];
	V_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s end_movie %s\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		GetEntityNameAsCStr() );

	// Send it on
	engine->ServerCommand( szClientCmd );
}

//-----------------------------------------------------------------------------
// Purpose: Plays a bink movie for all connected players
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayMovieForAllPlayers( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING( m_strMovieFilename ) ) )
		return;

	const char *szVideoCommand = ( m_bAllowUserSkip ) ? "playvideo_exitcommand" : "playvideo_exitcommand_nointerrupt";
	// Build the hacked string

	char szClientCmd[256];
	V_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s end_movie %s\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		GetEntityNameAsCStr() );

	bool bSplit = false;

	// Find out if we are in splitscreen first
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			if ( engine->IsSplitScreenPlayer( pToPlayer->entindex() ) )
			{
				bSplit = true;
				break;
			}
		}
	}

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
			// if we are in split screen, just play one movie
			if ( bSplit )
				return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputPlayLevelTransitionMovie( inputdata_t &data )
{
	if ( !Helper_BIsValidMovieFileString( STRING( m_strMovieFilename ) ) )
		return;

	const char *szVideoCommand = "playvideo_end_level_transition";

	m_bFadeInTime = std::max( 0.1f, m_bFadeInTime );

	char szClientCmd[256];
	V_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %s %f\n", 
		szVideoCommand,
		STRING(m_strMovieFilename), 
		m_bFadeInTime );

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputFadeAllMovies( inputdata_t &data )
{
	// TODO: HAX FOR PAX
	const char *szVideoCommand = "stopvideos_fadeout";

	char szClientCmd[256];
	V_snprintf( szClientCmd, sizeof(szClientCmd), 
		"%s %f\n", 
		szVideoCommand,
		1.0f );

	// Send it on
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "%s", szClientCmd );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicPlayMovie::InputMovieFinished( inputdata_t &data )
{
	// Simply fire our output
	m_OnPlaybackFinished.FireOutput( this, this );
}

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: When triggered, will attempt to fire off each of its outputs.  Each output
// has its own chance of firing.

static const int SF_REMOVE_ON_FIRE				= 0x001;	// Relay will remove itself after being triggered.
static const int SF_ALLOW_FAST_RETRIGGER		= 0x002;	// Unless set, entity will disable itself until the last output is sent.

LINK_ENTITY_TO_CLASS(logic_random_outputs, CLogicRandomOutputs);

IMPLEMENT_REFLECT_DATAMAP( CLogicRandomOutputs )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicRandomOutputs::CLogicRandomOutputs(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: Read in the chance of firing each output
//-----------------------------------------------------------------------------
bool CLogicRandomOutputs::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( szValue && szValue[0] )
	{
		for ( int i=0; i < NUM_RANDOM_OUTPUTS; i++ )
		{
			if ( FStrEq( szKeyName, UTIL_VarArgs( "OnTriggerChance%d", i ) ) )
			{
				m_flOnTriggerChance[i] = std::atof( szValue );
				return true;
			}
		}
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Kickstarts a think if we have OnSpawn connections.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	if ( m_OnSpawn.NumberOfElements() > 0)
	{
		SetNextThink( gpGlobals->curtime + 0.01 );
	}
}

//-----------------------------------------------------------------------------
// If we have OnSpawn connections, this is called shortly after spawning to
// fire the OnSpawn output.
//-----------------------------------------------------------------------------
void CLogicRandomOutputs::Think_Impl()
{
	// Fire an output when we spawn. This is used for self-starting an entity
	// template -- since the logic_random_outputs is inside the template, it gets all the
	// name and I/O connection fixup, so can target other entities in the template.
	m_OnSpawn.FireOutput( this, this );

	// We only get here if we had OnSpawn connections, so this is safe.
	if ( m_spawnflags & SF_REMOVE_ON_FIRE )
	{
		UTIL_Remove(this);
	}
}

//------------------------------------------------------------------------------
// Purpose: Turns on the entity, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Enables us to fire again. This input is only posted from our Trigger
//			function to prevent rapid refire.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputEnableRefire( inputdata_t &inputdata )
{ 
	Msg(" now enabling refire\n" );
	m_bWaitForRefire = false;
}

//------------------------------------------------------------------------------
// Purpose: Cancels any I/O events in the queue that were fired by us.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputCancelPending( inputdata_t &inputdata )
{ 
	g_EventQueue.CancelEvents( this );

	// Stop waiting; allow another Trigger.
	m_bWaitForRefire = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns off the entity, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}

//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the entity.
//------------------------------------------------------------------------------
void CLogicRandomOutputs::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the logic_random_outputs.
//-----------------------------------------------------------------------------
void CLogicRandomOutputs::InputTrigger( inputdata_t &inputdata )
{
	if ((!m_bDisabled) && (!m_bWaitForRefire))
	{
		for ( int i=0 ; i < NUM_RANDOM_OUTPUTS ; i++ )
		{
			if ( RandomFloat() <= m_flOnTriggerChance[i] )
			{
				m_Output[i].FireOutput( inputdata.pActivator, this );
			}
		}
		
		if (m_spawnflags & SF_REMOVE_ON_FIRE)
		{
			UTIL_Remove(this);
		}
		else if (!(m_spawnflags & SF_ALLOW_FAST_RETRIGGER))
		{
			// find the max delay from all our outputs
			float fMaxDelay = 0;
			for ( int i=0 ; i < NUM_RANDOM_OUTPUTS ; i++ )
			{
				fMaxDelay = std::max( fMaxDelay, m_Output[i].GetMaxDelay() );
			}
			if ( fMaxDelay > 0 )
			{
				// Disable the relay so that it cannot be refired until after the last output
				// has been fired and post an input to re-enable ourselves.
				m_bWaitForRefire = true;
				g_EventQueue.AddEvent(this, "EnableRefire", fMaxDelay + 0.001, this, this);
			}
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Fires an output when the map spawns (or respawns if not set to
//			only fire once). It can be set to check a global state before firing.

static const int SF_AUTO_FIREONCE		= 0x01;
static const int SF_AUTO_FIREONRELOAD	= 0x02;

class CLogicAuto : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CLogicAuto, CBaseEntity );

	void Activate_Impl(void);
	void Think_Impl(void);

	int ObjectCaps_Impl(void) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	void FireGameEvent( IGameEvent *event );					// incoming event processing

	DECLARE_DATADESC();

private:

	// fired no matter why the map loaded
	[[= ks::reflect::Key{ .name = "OnMapSpawn" } ]] COutputEvent m_OnMapSpawn;

	// fired for specified types of map loads
	[[= ks::reflect::Key{ .name = "OnNewGame" } ]] COutputEvent m_OnNewGame;
	[[= ks::reflect::Key{ .name = "OnLoadGame" } ]] COutputEvent m_OnLoadGame;
	[[= ks::reflect::Key{ .name = "OnMapTransition" } ]] COutputEvent m_OnMapTransition;
	[[= ks::reflect::Key{ .name = "OnBackgroundMap" } ]] COutputEvent m_OnBackgroundMap;
	[[= ks::reflect::Key{ .name = "OnMultiNewMap" } ]] COutputEvent m_OnMultiNewMap;
	[[= ks::reflect::Key{ .name = "OnMultiNewRound" } ]] COutputEvent m_OnMultiNewRound;

	[[= ks::reflect::Key{ .name = "globalstate" } ]] string_t m_globalstate;
};

LINK_ENTITY_TO_CLASS(logic_auto, CLogicAuto);

IMPLEMENT_REFLECT_DATAMAP( CLogicAuto )

//------------------------------------------------------------------------------
// Purpose : Fire my outputs here if I fire on map reload
//------------------------------------------------------------------------------
void CLogicAuto::Activate_Impl(void)
{
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "teamplay_round_start" );

	BaseClass::Activate_Impl();
	SetNextThink( gpGlobals->curtime + 0.2 );
}

//-----------------------------------------------------------------------------
// Purpose: Called shortly after level spawn. Checks the global state and fires
//			targets if the global state is set or if there is not global state
//			to check.
//-----------------------------------------------------------------------------
void CLogicAuto::Think_Impl(void)
{
	if (!m_globalstate || GlobalEntity_GetState(m_globalstate) == GLOBAL_ON)
	{
		if (gpGlobals->eLoadType == MapLoad_Transition)
		{
			m_OnMapTransition.FireOutput(nullptr, this);
		}
		else if (gpGlobals->eLoadType == MapLoad_NewGame)
		{
			m_OnNewGame.FireOutput(nullptr, this);
		}
		else if (gpGlobals->eLoadType == MapLoad_LoadGame)
		{
			m_OnLoadGame.FireOutput(nullptr, this);
		}
		else if (gpGlobals->eLoadType == MapLoad_Background)
		{
			m_OnBackgroundMap.FireOutput(nullptr, this);
		}

		m_OnMapSpawn.FireOutput(nullptr, this);

		if ( g_pGameRules->IsMultiplayer() )
		{
			m_OnMultiNewMap.FireOutput(nullptr, this);
		}

		if (m_spawnflags & SF_AUTO_FIREONCE)
		{
			UTIL_Remove(this);
		}
	}
}

void CLogicAuto::FireGameEvent( IGameEvent *gameEvent )
{
	if ( FStrEq( gameEvent->GetName(), "round_start" ) || FStrEq( gameEvent->GetName(), "teamplay_round_start" ) )
	{
		if ( g_pGameRules->IsMultiplayer() )
		{
			m_OnMultiNewRound.FireOutput(nullptr, this);
		}
	}
}

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements many of the entities that control logic flow within a map.

extern CServerGameDLL g_ServerGameDLL;

//-----------------------------------------------------------------------------
// Purpose: An entity that acts as a container for game scripts.
//-----------------------------------------------------------------------------

static constexpr auto MAX_SCRIPT_GROUP = 16;

class CLogicScript : public CPointEntity
{
public:
	DECLARE_CLASS( CLogicScript, CPointEntity );
	DECLARE_DATADESC();

	void RunVScripts_Impl()
	{
		/*
			EntityGroup <- [];
			function __AppendToScriptGroup( name ) 
			{
				if ( name.len() == 0 ) 
				{ 
					EntityGroup.append( null ); 
				} 
				else
				{ 
					local ent = Entities.FindByName( null, name );
					EntityGroup.append( ent );
					if ( ent != 0 )
					{
						ent.ValidateScriptScope();
						ent.GetScriptScope().EntityGroup <- EntityGroup;
					}
				}
			}
		*/

 		static const char szAddCode[] =
		{
			0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x20,0x3c,0x2d,0x20,0x5b,0x5d,0x3b,0x0d,0x0a,
			0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x5f,0x5f,0x41,0x70,0x70,0x65,0x6e,0x64,0x54,0x6f,0x53,
			0x63,0x72,0x69,0x70,0x74,0x47,0x72,0x6f,0x75,0x70,0x28,0x20,0x6e,0x61,0x6d,0x65,0x20,0x29,0x20,0x0d,
			0x0a,0x7b,0x0d,0x0a,0x09,0x69,0x66,0x20,0x28,0x20,0x6e,0x61,0x6d,0x65,0x2e,0x6c,0x65,0x6e,0x28,0x29,
			0x20,0x3d,0x3d,0x20,0x30,0x20,0x29,0x20,0x0d,0x0a,0x09,0x7b,0x20,0x0d,0x0a,0x09,0x09,0x45,0x6e,0x74,
			0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x2e,0x61,0x70,0x70,0x65,0x6e,0x64,0x28,0x20,0x6e,0x75,0x6c,
			0x6c,0x20,0x29,0x3b,0x20,0x0d,0x0a,0x09,0x7d,0x20,0x0d,0x0a,0x09,0x65,0x6c,0x73,0x65,0x0d,0x0a,0x09,
			0x7b,0x20,0x0d,0x0a,0x09,0x09,0x6c,0x6f,0x63,0x61,0x6c,0x20,0x65,0x6e,0x74,0x20,0x3d,0x20,0x45,0x6e,
			0x74,0x69,0x74,0x69,0x65,0x73,0x2e,0x46,0x69,0x6e,0x64,0x42,0x79,0x4e,0x61,0x6d,0x65,0x28,0x20,0x6e,
			0x75,0x6c,0x6c,0x2c,0x20,0x6e,0x61,0x6d,0x65,0x20,0x29,0x3b,0x0d,0x0a,0x09,0x09,0x45,0x6e,0x74,0x69,
			0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x2e,0x61,0x70,0x70,0x65,0x6e,0x64,0x28,0x20,0x65,0x6e,0x74,0x20,
			0x29,0x3b,0x0d,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x65,0x6e,0x74,0x20,0x21,0x3d,0x20,0x6e,0x75,
			0x6c,0x6c,0x20,0x29,0x0d,0x0a,0x09,0x09,0x7b,0x0d,0x0a,0x09,0x09,0x09,0x65,0x6e,0x74,0x2e,0x56,0x61,
			0x6c,0x69,0x64,0x61,0x74,0x65,0x53,0x63,0x72,0x69,0x70,0x74,0x53,0x63,0x6f,0x70,0x65,0x28,0x29,0x3b,
			0x0d,0x0a,0x09,0x09,0x09,0x65,0x6e,0x74,0x2e,0x47,0x65,0x74,0x53,0x63,0x72,0x69,0x70,0x74,0x53,0x63,
			0x6f,0x70,0x65,0x28,0x29,0x2e,0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x20,0x3c,0x2d,
			0x20,0x45,0x6e,0x74,0x69,0x74,0x79,0x47,0x72,0x6f,0x75,0x70,0x3b,0x0d,0x0a,0x09,0x09,0x7d,0x0d,0x0a,
			0x09,0x7d,0x0d,0x0a,0x7d,0x0d,0x0a,0x00
		};

		int iLastMember;
		for ( iLastMember = MAX_SCRIPT_GROUP - 1; iLastMember >= 0; iLastMember-- )
		{
			if ( m_iszGroupMembers[iLastMember] != NULL_STRING )
			{
				break;
			}
		}

		if ( iLastMember >= 0 )
		{
			HSCRIPT hAddScript = g_pScriptVM->CompileScript( szAddCode );
			if ( hAddScript )
			{
				ValidateScriptScope();
				m_ScriptScope.Run( hAddScript );
				HSCRIPT hAddFunc = m_ScriptScope.LookupFunction( "__AppendToScriptGroup" );
				if ( hAddFunc )
				{
					for ( int i = 0; i <= iLastMember; i++ )
					{
						m_ScriptScope.Call( hAddFunc, nullptr, STRING(m_iszGroupMembers[i]) );
					}
					g_pScriptVM->ReleaseFunction( hAddFunc );
					m_ScriptScope.ClearValue( "__AppendToScriptGroup" );
				}

				g_pScriptVM->ReleaseScript( hAddScript );
			}
		}
		BaseClass::RunVScripts_Impl();
	}

	[[= ks::reflect::Key{ .name = "Group16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Group14", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Group13", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Group12", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Group11", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Group10", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Group09", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Group08", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Group07", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Group06", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Group05", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Group04", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Group03", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Group02", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Group01", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Group00", .index = 0 } ]] string_t m_iszGroupMembers[MAX_SCRIPT_GROUP];

};

LINK_ENTITY_TO_CLASS( logic_script, CLogicScript );

IMPLEMENT_REFLECT_DATAMAP( CLogicScript )

//-----------------------------------------------------------------------------
// Purpose: Compares a set of integer inputs to the one main input
//			Outputs true if they are all equivalant, false otherwise
//-----------------------------------------------------------------------------
class CLogicCompareInteger : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicCompareInteger, CLogicalEntity );

	// outputs
	[[= ks::reflect::Key{ .name = "OnEqual" } ]] COutputEvent m_OnEqual;
	[[= ks::reflect::Key{ .name = "OnNotEqual" } ]] COutputEvent m_OnNotEqual;

	// data
	[[= ks::reflect::Key{ .name = "IntegerValue" } ]] int m_iIntegerValue;
	[[= ks::reflect::Key{ .name = "ShouldComparetoValue" } ]] int m_iShouldCompareToValue;

	DECLARE_DATADESC();

	CMultiInputVar m_AllIntCompares;

	// Input handlers
	[[= ks::reflect::Input{ .name = "InputValue", .type = FIELD_INPUT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CompareValues", .type = FIELD_INPUT } ]] void InputCompareValues( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( logic_multicompare, CLogicCompareInteger );

IMPLEMENT_REFLECT_DATAMAP( CLogicCompareInteger )

//-----------------------------------------------------------------------------
// Purpose: Adds to the list of compared values
//-----------------------------------------------------------------------------
void CLogicCompareInteger::InputValue( inputdata_t &inputdata )
{
	// make sure it's an int, if it can't be converted just throw it away
	if ( !inputdata.value.Convert(FIELD_INTEGER) )
		return;

	// update the value list with the new value
	m_AllIntCompares.AddValue( inputdata.value, inputdata.nOutputID );

	// if we haven't already this frame, send a message to ourself to update and fire
	if ( !m_AllIntCompares.m_bUpdatedThisFrame )
	{
		// TODO: need to add this event with a lower priority, so it gets called after all inputs have arrived
		g_EventQueue.AddEvent( this, "CompareValues", 0, inputdata.pActivator, this, inputdata.nOutputID );
		m_AllIntCompares.m_bUpdatedThisFrame = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces a recompare
//-----------------------------------------------------------------------------
void CLogicCompareInteger::InputCompareValues( inputdata_t &inputdata )
{
	m_AllIntCompares.m_bUpdatedThisFrame = false;

	// loop through all the values comparing them
	int value = m_iIntegerValue;
	CMultiInputVar::inputitem_t *input = m_AllIntCompares.m_InputList;

	if ( !m_iShouldCompareToValue && input )
	{
		value = input->value.Int();
	}

	while ( input )
	{
		if ( input->value.Int() != value )
		{
			// false
			m_OnNotEqual.FireOutput( inputdata.pActivator, this );
			return;
		}

		input = input->next;
	}

	// true! all values equal
	m_OnEqual.FireOutput( inputdata.pActivator, this );
}

class CLogicRegisterActivator : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRegisterActivator, CLogicalEntity );

	CLogicRegisterActivator();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "RegisterEntity", .type = FIELD_STRING } ]] void InputRegisterEntity( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator1", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator1( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator2", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator2( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator3", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator3( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireRegisteredAsActivator4", .type = FIELD_VOID } ]] void InputFireRegisteredAsActivator4( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate1" } ]] COutputEvent m_OnRegisteredActivate1;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate2" } ]] COutputEvent m_OnRegisteredActivate2;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate3" } ]] COutputEvent m_OnRegisteredActivate3;
	[[= ks::reflect::Key{ .name = "OnRegisteredActivate4" } ]] COutputEvent m_OnRegisteredActivate4;
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	EHANDLE	m_hRegisteredEntity;
};

//-----------------------------------------------------------------------------
// Purpose: Stores an entity and uses it later for certain actions
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS(logic_register_activator, CLogicRegisterActivator);

IMPLEMENT_REFLECT_DATAMAP( CLogicRegisterActivator )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicRegisterActivator::CLogicRegisterActivator(void)
{
}

//------------------------------------------------------------------------------
// Purpose: Register an entity - can use it later as an activator
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputRegisterEntity( inputdata_t &inputdata )
{
	m_hRegisteredEntity = gEntList.FindEntityByName( nullptr, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
}

//------------------------------------------------------------------------------
// Purpose: Turns entitiy on, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns entity off, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}

//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the entity.
//------------------------------------------------------------------------------
void CLogicRegisterActivator::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator1( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate1.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator2( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate2.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator3( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate3.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the messages.
//-----------------------------------------------------------------------------
void CLogicRegisterActivator::InputFireRegisteredAsActivator4( inputdata_t &inputdata )
{
	if (!m_bDisabled && m_hRegisteredEntity)
	{
		m_OnRegisteredActivate4.FireOutput( m_hRegisteredEntity.Get(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Timer entity. Fires an output at regular or random intervals.
//-----------------------------------------------------------------------------
//
// Spawnflags and others constants.
//
static const int SF_TIMER_UPDOWN = 1;
static const float LOGIC_TIMER_MIN_INTERVAL = 0.01;

class CTimerEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS( CTimerEntity, CLogicalEntity );

	void Spawn_Impl( void );
	void Think_Impl( void );

	void Toggle( void );
	void Enable( void );
	void Disable( void );
	void FireTimer( void );

	int DrawDebugTextOverlays_Impl(void);

	// outputs
	[[= ks::reflect::Key{ .name = "OnTimer" } ]] COutputEvent m_OnTimer;
	[[= ks::reflect::Key{ .name = "OnTimerHigh" } ]] COutputEvent m_OnTimerHigh;
	[[= ks::reflect::Key{ .name = "OnTimerLow" } ]] COutputEvent m_OnTimerLow;

	// inputs
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireTimer", .type = FIELD_VOID } ]] void InputFireTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RefireTime", .type = FIELD_FLOAT } ]] void InputRefireTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ResetTimer", .type = FIELD_VOID } ]] void InputResetTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddToTimer", .type = FIELD_FLOAT } ]] void InputAddToTimer( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SubtractFromTimer", .type = FIELD_FLOAT } ]] void InputSubtractFromTimer( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int m_iDisabled;
	[[= ks::reflect::Key{ .name = "RefireTime" } ]] float m_flRefireTime;
	bool m_bUpDownState;
	[[= ks::reflect::Key{ .name = "UseRandomTime", .input = true } ]] int m_iUseRandomTime;
	[[= ks::reflect::Key{ .name = "LowerRandomBound", .input = true } ]] float m_flLowerRandomBound;
	[[= ks::reflect::Key{ .name = "UpperRandomBound", .input = true } ]] float m_flUpperRandomBound;

	// methods
	void ResetTimer( void );
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( logic_timer, CTimerEntity );

IMPLEMENT_REFLECT_DATAMAP( CTimerEntity )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Spawn_Impl( void )
{
	if (!m_iUseRandomTime && (m_flRefireTime < LOGIC_TIMER_MIN_INTERVAL))
	{
		m_flRefireTime = LOGIC_TIMER_MIN_INTERVAL;
	}

	if ( !m_iDisabled && (m_flRefireTime > 0 || m_iUseRandomTime) )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Think_Impl( void )
{
	FireTimer();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the time the timerentity will next fire
//-----------------------------------------------------------------------------
void CTimerEntity::ResetTimer( void )
{
	if ( m_iDisabled )
		return;

	if ( m_iUseRandomTime )
	{
		m_flRefireTime = random_valve->RandomFloat( m_flLowerRandomBound, m_flUpperRandomBound );
	}

	SetNextThink( gpGlobals->curtime + m_flRefireTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Enable( void )
{
	m_iDisabled = false;
	ResetTimer();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Disable( void )
{
	m_iDisabled = true;
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::Toggle( void )
{
	if ( m_iDisabled )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::FireTimer( void )
{
	if ( !m_iDisabled )
	{
		//
		// Up/down timers alternate between two outputs.
		//
		if (m_spawnflags & SF_TIMER_UPDOWN)
		{
			if (m_bUpDownState)
			{
				m_OnTimerHigh.FireOutput( this, this );
			}
			else
			{
				m_OnTimerLow.FireOutput( this, this );
			}

			m_bUpDownState = !m_bUpDownState;
		}
		//
		// Regular timers only fire a single output.
		//
		else
		{
			m_OnTimer.FireOutput( this, this );
		}

		ResetTimer();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTimerEntity::InputFireTimer( inputdata_t &inputdata )
{
	FireTimer();
}

//-----------------------------------------------------------------------------
// Purpose: Changes the time interval between timer fires
//			Resets the next firing to be time + newRefireTime
// Input  : Float refire frequency in seconds.
//-----------------------------------------------------------------------------
void CTimerEntity::InputRefireTime( inputdata_t &inputdata )
{
	float flRefireInterval = inputdata.value.Float();

	if ( flRefireInterval < LOGIC_TIMER_MIN_INTERVAL)
	{
		flRefireInterval = LOGIC_TIMER_MIN_INTERVAL;
	}

	if (m_flRefireTime != flRefireInterval )
	{
		m_flRefireTime = flRefireInterval;
		ResetTimer();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTimerEntity::InputResetTimer( inputdata_t &inputdata )
{
	// don't reset the timer if it isn't enabled
	if ( m_iDisabled )
		return;

	ResetTimer();
}

//-----------------------------------------------------------------------------
// Purpose: Adds to the time interval if the timer is enabled
// Input  : Float time to add in seconds
//-----------------------------------------------------------------------------
void CTimerEntity::InputAddToTimer( inputdata_t &inputdata )
{
	// don't add time if the timer isn't enabled
	if ( m_iDisabled )
		return;
	
	// Add time to timer
 	float flNextThink = GetNextThink();	
	SetNextThink( flNextThink += inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Subtract from the time interval if the timer is enabled
// Input  : Float time to subtract in seconds
//-----------------------------------------------------------------------------
void CTimerEntity::InputSubtractFromTimer( inputdata_t &inputdata )
{
	// don't add time if the timer isn't enabled
	if ( m_iDisabled )
		return;

	// Subtract time from the timer but don't let the timer go negative
	float flNextThink = GetNextThink();
	if ( ( flNextThink - gpGlobals->curtime ) <= inputdata.value.Float() )
	{
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		SetNextThink( flNextThink -= inputdata.value.Float() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CTimerEntity::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print refire time
		V_snprintf(tempstr,sizeof(tempstr),"refire interval: %.2f sec", m_flRefireTime);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print seconds to next fire
		if ( !m_iDisabled )
		{
			float flNextThink = GetNextThink();
			V_snprintf( tempstr, sizeof( tempstr ), "      firing in: %.2f sec", flNextThink - gpGlobals->curtime );
			EntityText( text_offset, tempstr, 0);
			text_offset++;
		}
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Computes a line between two entities
//-----------------------------------------------------------------------------
class CLogicLineToEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicLineToEntity, CLogicalEntity );

	void Activate_Impl(void);
	void Spawn_Impl( void );
	void Think_Impl( void );

	// outputs
	[[= ks::reflect::Key{ .name = "Line" } ]] COutputVector m_Line;

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "source" } ]] string_t m_SourceName;
	EHANDLE	m_StartEntity;
	EHANDLE m_EndEntity;
};

LINK_ENTITY_TO_CLASS( logic_lineto, CLogicLineToEntity );

IMPLEMENT_REFLECT_DATAMAP( CLogicLineToEntity )

//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Activate_Impl(void)
{
	BaseClass::Activate_Impl();

	if (m_target != NULL_STRING)
	{
		m_EndEntity = gEntList.FindEntityByName( nullptr, m_target );

		//
		// If we were given a bad measure target, just measure sound where we are.
		//
		if ((m_EndEntity == nullptr) || (m_EndEntity->edict() == nullptr))
		{
			Warning( "logic_lineto - Target not found or target with no origin!\n");
			m_EndEntity = this;
		}
	}
	else
	{
		m_EndEntity = this;
	}

	if (m_SourceName != NULL_STRING)
	{
		m_StartEntity = gEntList.FindEntityByName( nullptr, m_SourceName );

		//
		// If we were given a bad measure target, just measure sound where we are.
		//
		if ((m_StartEntity == nullptr) || (m_StartEntity->edict() == nullptr))
		{
			Warning( "logic_lineto - Source not found or source with no origin!\n");
			m_StartEntity = this;
		}
	}
	else
	{
		m_StartEntity = this;
	}
}

//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Spawn_Impl(void)
{
	SetNextThink( gpGlobals->curtime + 0.01f );
}

//-----------------------------------------------------------------------------
// Find the entities
//-----------------------------------------------------------------------------
void CLogicLineToEntity::Think_Impl(void)
{
	CBaseEntity* pDest = m_EndEntity.Get();
	CBaseEntity* pSrc = m_StartEntity.Get();
	if (!pDest || !pSrc || !pDest->edict() || !pSrc->edict())
	{
		// Can sleep for a long time, no more lines.
		m_Line.Set( vec3_origin, this, this );
		SetNextThink( gpGlobals->curtime + 10 );
		return;
	}

	Vector delta;
	VectorSubtract( pDest->GetAbsOrigin(), pSrc->GetAbsOrigin(), delta ); 
	m_Line.Set(delta, this, this);

	SetNextThink( gpGlobals->curtime + 0.01f );
}

//-----------------------------------------------------------------------------
// Purpose: Remaps a given input range to an output range.
//-----------------------------------------------------------------------------
static const int SF_MATH_REMAP_IGNORE_OUT_OF_RANGE = 1;
static const int SF_MATH_REMAP_CLAMP_OUTPUT_TO_RANGE = 2;

class CMathRemap : public CLogicalEntity
{
public:

	DECLARE_CLASS( CMathRemap, CLogicalEntity );

	void Spawn_Impl(void);

	// Keys
	[[= ks::reflect::Key{ .name = "in1" } ]] float m_flInMin;
	[[= ks::reflect::Key{ .name = "in2" } ]] float m_flInMax;
	[[= ks::reflect::Key{ .name = "out1" } ]] float m_flOut1;		// Output value when input is m_fInMin
	[[= ks::reflect::Key{ .name = "out2" } ]] float m_flOut2;		// Output value when input is m_fInMax

	bool  m_bEnabled;

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_FLOAT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutValue" } ]] COutputFloat m_OutValue;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_remap, CMathRemap);

IMPLEMENT_REFLECT_DATAMAP( CMathRemap )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::Spawn_Impl(void)
{
	//
	// Avoid a divide by zero in ValueChanged.
	//
	if (m_flInMin == m_flInMax)
	{
		m_flInMin = 0;
		m_flInMax = 1;
	}

	//
	// Make sure min and max are set properly relative to one another.
	//
	if (m_flInMin > m_flInMax)
	{
		float flTemp = m_flInMin;
		m_flInMin = m_flInMax;
		m_flInMax = flTemp;
	}

	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathRemap::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that is called when the input value changes.
//-----------------------------------------------------------------------------
void CMathRemap::InputValue( inputdata_t &inputdata )
{
	float flValue = inputdata.value.Float();

	//
	// Disallow out-of-range input values to avoid out-of-range output values.
	//
	float flClampValue = clamp(flValue, m_flInMin, m_flInMax);

	if ((flClampValue == flValue) || !FBitSet(m_spawnflags, SF_MATH_REMAP_IGNORE_OUT_OF_RANGE))
	{
		//
		// Remap the input value to the desired output range and update the output.
		//
		float flRemappedValue = m_flOut1 + (((flValue - m_flInMin) * (m_flOut2 - m_flOut1)) / (m_flInMax - m_flInMin));

		if ( FBitSet( m_spawnflags, SF_MATH_REMAP_CLAMP_OUTPUT_TO_RANGE ) )
		{
			flRemappedValue = clamp( flRemappedValue, m_flOut1, m_flOut2 );
		}

		if ( m_bEnabled == true )
		{
			m_OutValue.Set(flRemappedValue, inputdata.pActivator, this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remaps a given input range to an output range.
//-----------------------------------------------------------------------------
static const int SF_COLOR_BLEND_IGNORE_OUT_OF_RANGE = 1;

class CMathColorBlend : public CLogicalEntity
{
public:

	DECLARE_CLASS( CMathColorBlend, CLogicalEntity );

	void Spawn_Impl(void);

	// Keys
	[[= ks::reflect::Key{ .name = "inmin" } ]] float m_flInMin;
	[[= ks::reflect::Key{ .name = "inmax" } ]] float m_flInMax;
	[[= ks::reflect::Key{ .name = "colormin" } ]] color32 m_OutColor1;		// Output color when input is m_fInMin
	[[= ks::reflect::Key{ .name = "colormax" } ]] color32 m_OutColor2;		// Output color when input is m_fInMax

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_FLOAT } ]] void InputValue( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutColor" } ]] COutputColor32 m_OutValue;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_colorblend, CMathColorBlend);

IMPLEMENT_REFLECT_DATAMAP( CMathColorBlend )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMathColorBlend::Spawn_Impl(void)
{
	//
	// Avoid a divide by zero in ValueChanged.
	//
	if (m_flInMin == m_flInMax)
	{
		m_flInMin = 0;
		m_flInMax = 1;
	}

	//
	// Make sure min and max are set properly relative to one another.
	//
	if (m_flInMin > m_flInMax)
	{
		float flTemp = m_flInMin;
		m_flInMin = m_flInMax;
		m_flInMax = flTemp;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that is called when the input value changes.
//-----------------------------------------------------------------------------
void CMathColorBlend::InputValue( inputdata_t &inputdata )
{
	float flValue = inputdata.value.Float();

	//
	// Disallow out-of-range input values to avoid out-of-range output values.
	//
	float flClampValue = clamp(flValue, m_flInMin, m_flInMax);
	if ((flClampValue == flValue) || !FBitSet(m_spawnflags, SF_COLOR_BLEND_IGNORE_OUT_OF_RANGE))
	{
		//
		// Remap the input value to the desired output color and update the output.
		//
		color32 Color;
		Color.r = m_OutColor1.r + (((flClampValue - m_flInMin) * (m_OutColor2.r - m_OutColor1.r)) / (m_flInMax - m_flInMin));
		Color.g = m_OutColor1.g + (((flClampValue - m_flInMin) * (m_OutColor2.g - m_OutColor1.g)) / (m_flInMax - m_flInMin));
		Color.b = m_OutColor1.b + (((flClampValue - m_flInMin) * (m_OutColor2.b - m_OutColor1.b)) / (m_flInMax - m_flInMin));
		Color.a = m_OutColor1.a + (((flClampValue - m_flInMin) * (m_OutColor2.a - m_OutColor1.a)) / (m_flInMax - m_flInMin));

		m_OutValue.Set(Color, inputdata.pActivator, this);
	}
}

//-----------------------------------------------------------------------------
// Console command to set the state of a global
//-----------------------------------------------------------------------------
void CC_Global_Set( const CCommand &args )
{
	const char *szGlobal = args[1];
	const char *szState = args[2];

	if ( szGlobal == nullptr || szState == nullptr )
	{
		Msg( "Usage: global_set <globalname> <state>: Sets the state of the given env_global (0 = OFF, 1 = ON, 2 = DEAD).\n" );
		return;
	}

	int nState = std::atoi( szState );

	int nIndex = GlobalEntity_GetIndex( szGlobal );

	if ( nIndex >= 0 )
	{
		GlobalEntity_SetState( nIndex, ( GLOBALESTATE )nState );
	}
	else
	{
		GlobalEntity_Add( szGlobal, STRING( gpGlobals->mapname ), ( GLOBALESTATE )nState );
	}
}

static ConCommand global_set( side_registry(), "global_set", CC_Global_Set, "global_set <globalname> <state>: Sets the state of the given env_global (0 = OFF, 1 = ON, 2 = DEAD).", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Holds a global state that can be queried by other entities to change
//			their behavior, such as "predistaster".
//-----------------------------------------------------------------------------
static const int SF_GLOBAL_SET = 1;	// Set global state to initial state on spawn

class CEnvGlobal : public CLogicalEntity
{
public:
	DECLARE_CLASS( CEnvGlobal, CLogicalEntity );

	void Spawn_Impl( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Remove", .type = FIELD_VOID } ]] void InputRemove( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCounter", .type = FIELD_INTEGER } ]] void InputSetCounter( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddToCounter", .type = FIELD_INTEGER } ]] void InputAddToCounter( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetCounter", .type = FIELD_VOID } ]] void InputGetCounter( inputdata_t &inputdata );

	int DrawDebugTextOverlays_Impl(void);

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "Counter" } ]] COutputInt m_outCounter;
		
	[[= ks::reflect::Key{ .name = "globalstate" } ]] string_t	m_globalstate;
	int			m_triggermode;
	[[= ks::reflect::Key{ .name = "initialstate" } ]] int			m_initialstate;
	[[= ks::reflect::Key{ .name = "counter" } ]] int			m_counter;			// A counter value associated with this global.
};

IMPLEMENT_REFLECT_DATAMAP( CEnvGlobal )

LINK_ENTITY_TO_CLASS( env_global, CEnvGlobal );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvGlobal::Spawn_Impl( void )
{
	if ( !m_globalstate )
	{
		UTIL_Remove( this );
		return;
	}

	if ( FBitSet( m_spawnflags, SF_GLOBAL_SET ) )
	{
		if ( !GlobalEntity_IsInTable( m_globalstate ) )
		{
			GlobalEntity_Add( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)m_initialstate );
		}
		
		if ( m_counter != 0 )
		{
			GlobalEntity_SetCounter( m_globalstate, m_counter );
		}
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputTurnOn( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_ON );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputTurnOff( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_OFF );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_OFF );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputRemove( inputdata_t &inputdata )
{
	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, GLOBAL_DEAD );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_DEAD );
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputSetCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	GlobalEntity_SetCounter( m_globalstate, inputdata.value.Int() );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputAddToCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	GlobalEntity_AddToCounter( m_globalstate, inputdata.value.Int() );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CEnvGlobal::InputGetCounter( inputdata_t &inputdata )
{
	if ( !GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, GLOBAL_ON );
	}

	m_outCounter.Set( GlobalEntity_GetCounter( m_globalstate ), inputdata.pActivator, this );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnvGlobal::InputToggle( inputdata_t &inputdata )
{
	GLOBALESTATE oldState = GlobalEntity_GetState( m_globalstate );
	GLOBALESTATE newState;

	if ( oldState == GLOBAL_ON )
	{
		newState = GLOBAL_OFF;
	}
	else if ( oldState == GLOBAL_OFF )
	{
		newState = GLOBAL_ON;
	}
	else
	{
		return;
	}

	if ( GlobalEntity_IsInTable( m_globalstate ) )
	{
		GlobalEntity_SetState( m_globalstate, newState );
	}
	else
	{
		GlobalEntity_Add( m_globalstate, gpGlobals->mapname, newState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CEnvGlobal::DrawDebugTextOverlays_Impl(void) 
{
	// Skip AIClass debug overlays
	int text_offset = CBaseEntity::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		V_snprintf(tempstr,sizeof(tempstr),"State: %s",STRING(m_globalstate));
		EntityText(text_offset,tempstr,0);
		text_offset++;

		GLOBALESTATE nState = GlobalEntity_GetState( m_globalstate );

		switch( nState )
		{
		case GLOBAL_OFF:
			V_strncpy(tempstr,"Value: OFF",sizeof(tempstr));
			break;

		case GLOBAL_ON:
			V_strncpy(tempstr,"Value: ON",sizeof(tempstr));
			break;

		case GLOBAL_DEAD:
			V_strncpy(tempstr,"Value: DEAD",sizeof(tempstr));
			break;
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static constexpr auto MS_MAX_TARGETS = 32;

static const int SF_MULTI_INIT	= 1;

class CMultiSource : public CLogicalEntity
{
public:
	DECLARE_CLASS( CMultiSource, CLogicalEntity );

	void Spawn_Impl( );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Use_Impl( ::CBaseEntity *pActivator, ::CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	ObjectCaps_Impl( void ) { return(BaseClass::ObjectCaps_Impl() | FCAP_MASTER); }
	bool IsTriggered_Impl( ::CBaseEntity *pActivator );
	void Register( void );

	DECLARE_DATADESC();

	EHANDLE		m_rgEntities[MS_MAX_TARGETS];
	int			m_rgTriggered[MS_MAX_TARGETS];

	[[= ks::reflect::Key{ .name = "OnTrigger" } ]] COutputEvent m_OnTrigger;		// Fired when all connections are triggered.

	int			m_iTotal;
	[[= ks::reflect::Key{ .name = "globalstate" } ]] string_t	m_globalstate;
};

IMPLEMENT_REFLECT_DATAMAP( CMultiSource )

LINK_ENTITY_TO_CLASS( multisource, CMultiSource );

//-----------------------------------------------------------------------------
// Purpose: Cache user entity field values until spawn is called.
// Input  : szKeyName - Key to handle.
//			szValue - Value for key.
// Output : Returns true if the key was handled, false if not.
//-----------------------------------------------------------------------------
bool CMultiSource::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (	FStrEq(szKeyName, "style") ||
				FStrEq(szKeyName, "height") ||
				FStrEq(szKeyName, "killtarget") ||
				FStrEq(szKeyName, "value1") ||
				FStrEq(szKeyName, "value2") ||
				FStrEq(szKeyName, "value3"))
	{
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiSource::Spawn_Impl()
{ 
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_spawnflags |= SF_MULTI_INIT;	// Until it's initialized
	SetThink(&CMultiSource::Register);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CMultiSource::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	int i = 0;

	// Find the entity in our list
	while (i < m_iTotal)
		if ( m_rgEntities[i++] == pCaller )
			break;

	// if we didn't find it, report error and leave
	if (i > m_iTotal)
	{
		Warning("MultiSrc: Used by non member %s.\n", pCaller->edict() ? pCaller->GetClassname() : "<logical entity>");
		return;	
	}

	// CONSIDER: a Use input to the multisource always toggles.  Could check useType for ON/OFF/TOGGLE

	contract_assert( i > 0 );
	m_rgTriggered[i-1] ^= 1;

	// 
	if ( IsTriggered( pActivator ) )
	{
		DevMsg( 2, "Multisource %s enabled (%d inputs)\n", GetDebugName(), m_iTotal );
		USE_TYPE useType = USE_TOGGLE;
		if ( m_globalstate != NULL_STRING )
			useType = USE_ON;

		m_OnTrigger.FireOutput(pActivator, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMultiSource::IsTriggered_Impl( CBaseEntity * )
{
	// Is everything triggered?
	int i = 0;

	// Still initializing?
	if ( m_spawnflags & SF_MULTI_INIT )
		return 0;

	while (i < m_iTotal)
	{
		if (m_rgTriggered[i] == 0)
			break;
		i++;
	}

	if (i == m_iTotal)
	{
		if ( !m_globalstate || GlobalEntity_GetState( m_globalstate ) == GLOBAL_ON )
			return 1;
	}
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiSource::Register(void)
{ 
	CBaseEntity *pTarget = nullptr;

	m_iTotal = 0;
	std::memset( m_rgEntities, 0, MS_MAX_TARGETS * sizeof(EHANDLE) );

	SetThink(&CMultiSource::SUB_DoNothing);

	// search for all entities which target this multisource (m_iName)
	// dvsents2: port multisource to entity I/O!

	pTarget = gEntList.FindEntityByTarget( nullptr, STRING(GetEntityName()) );

	while ( pTarget && (m_iTotal < MS_MAX_TARGETS) )
	{
		if ( pTarget )
			m_rgEntities[m_iTotal++] = pTarget;

		pTarget = gEntList.FindEntityByTarget( pTarget, STRING(GetEntityName()) );
	}

	pTarget = gEntList.FindEntityByClassname( nullptr, "multi_manager" );
	while (pTarget && (m_iTotal < MS_MAX_TARGETS))
	{
		if ( pTarget && pTarget->HasTarget(GetEntityName()) )
			m_rgEntities[m_iTotal++] = pTarget;

		pTarget = gEntList.FindEntityByClassname( pTarget, "multi_manager" );
	}

	m_spawnflags &= ~SF_MULTI_INIT;
}

//-----------------------------------------------------------------------------
// Purpose: Holds a value that can be added to and subtracted from.
//-----------------------------------------------------------------------------
class CMathCounter : public CLogicalEntity
{
	DECLARE_CLASS( CMathCounter, CLogicalEntity );
private:
	[[= ks::reflect::Key{ .name = "min" } ]] float m_flMin;		// Minimum clamp value. If min and max are BOTH zero, no clamping is done.
	[[= ks::reflect::Key{ .name = "max" } ]] float m_flMax;		// Maximum clamp value.
	bool m_bHitMin;		// Set when we reach or go below our minimum value, cleared if we go above it again.
	bool m_bHitMax;		// Set when we reach or exceed our maximum value, cleared if we fall below it again.

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;

	public: using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl(const char *szKeyName, const char *szValue);
	private:
	public: void Spawn_Impl(void);
	private:

	public: int DrawDebugTextOverlays_Impl(void);
	private:

	void UpdateOutValue(CBaseEntity *pActivator, float fNewValue);

	// Inputs
	[[= ks::reflect::Input{ .name = "Add", .type = FIELD_FLOAT } ]] void InputAdd( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Divide", .type = FIELD_FLOAT } ]] void InputDivide( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Multiply", .type = FIELD_FLOAT } ]] void InputMultiply( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_FLOAT } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueNoFire", .type = FIELD_FLOAT } ]] void InputSetValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxValueNoFire", .type = FIELD_FLOAT } ]] void InputSetMaxValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinValueNoFire", .type = FIELD_FLOAT } ]] void InputSetMinValueNoFire( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Subtract", .type = FIELD_FLOAT } ]] void InputSubtract( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHitMax", .type = FIELD_FLOAT } ]] void InputSetHitMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHitMin", .type = FIELD_FLOAT } ]] void InputSetHitMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetValue", .type = FIELD_VOID } ]] void InputGetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OutValue" } ]] COutputFloat m_OutValue;
	[[= ks::reflect::Key{ .name = "OnGetValue" } ]] COutputFloat m_OnGetValue;	// Used for polling the counter value.
	[[= ks::reflect::Key{ .name = "OnHitMin" } ]] COutputEvent m_OnHitMin;
	[[= ks::reflect::Key{ .name = "OnHitMax" } ]] COutputEvent m_OnHitMax;
	[[= ks::reflect::Key{ .name = "OnChangedFromMin" } ]] COutputEvent m_OnChangedFromMin;
	[[= ks::reflect::Key{ .name = "OnChangedFromMax" } ]] COutputEvent m_OnChangedFromMax;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(math_counter, CMathCounter);

IMPLEMENT_REFLECT_DATAMAP( CMathCounter )

//-----------------------------------------------------------------------------
// Purpose: Handles key values from the BSP before spawn is called.
//-----------------------------------------------------------------------------
bool CMathCounter::KeyValue_Impl(const char *szKeyName, const char *szValue)
{
	//
	// Set the initial value of the counter.
	//
	if (!stricmp(szKeyName, "startvalue"))
	{
		m_OutValue.Init(std::atoi(szValue));
		return(true);
	}

	return(BaseClass::KeyValue_Impl(szKeyName, szValue));
}

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CMathCounter::Spawn_Impl( void )
{
	//
	// Make sure max and min are ordered properly or clamp won't work.
	//
	if (m_flMin > m_flMax)
	{
		float flTemp = m_flMax;
		m_flMax = m_flMin;
		m_flMin = flTemp;
	}

	//
	// Clamp initial value to within the valid range.
	//
	if ((m_flMin != 0) || (m_flMax != 0))
	{
		float flStartValue = clamp(m_OutValue.Get(), m_flMin, m_flMax);
		m_OutValue.Init(flStartValue);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CMathCounter::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		V_snprintf(tempstr,sizeof(tempstr),"    min value: %f", m_flMin);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"    max value: %f", m_flMax);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"current value: %f", m_OutValue.Get());
		EntityText(text_offset,tempstr,0);
		text_offset++;

		if( m_bDisabled )
		{	
			V_snprintf(tempstr,sizeof(tempstr),"*DISABLED*");		
		}
		else
		{
			V_snprintf(tempstr,sizeof(tempstr),"Enabled.");
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Change min/max
//-----------------------------------------------------------------------------
void CMathCounter::InputSetHitMax( inputdata_t &inputdata )
{
	m_flMax = inputdata.value.Float();
	if ( m_flMax < m_flMin )
	{
		m_flMin = m_flMax;
	}
	UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
}

void CMathCounter::InputSetHitMin( inputdata_t &inputdata )
{
	m_flMin = inputdata.value.Float();
	if ( m_flMax < m_flMin )
	{
		m_flMax = m_flMin;
	}
	UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the accumulator value.
// Input  : Float value to add.
//-----------------------------------------------------------------------------
void CMathCounter::InputAdd( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring ADD because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() + inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for multiplying the current value.
// Input  : Float value to multiply the value by.
//-----------------------------------------------------------------------------
void CMathCounter::InputDivide( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring DIVIDE because it is disabled\n", GetDebugName() );
		return;
	}

	if (inputdata.value.Float() != 0)
	{
		float fNewValue = m_OutValue.Get() / inputdata.value.Float();
		UpdateOutValue( inputdata.pActivator, fNewValue );
	}
	else
	{
		DevMsg( 1, "LEVEL DESIGN ERROR: Divide by zero in math_value\n" );
		UpdateOutValue( inputdata.pActivator, m_OutValue.Get() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for multiplying the current value.
// Input  : Float value to multiply the value by.
//-----------------------------------------------------------------------------
void CMathCounter::InputMultiply( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring MULTIPLY because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() * inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetValue( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETVALUE because it is disabled\n", GetDebugName() );
		return;
	}

	UpdateOutValue( inputdata.pActivator, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flNewValue = clamp(flNewValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flNewValue );

}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the MAX value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetMaxValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETMAXVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if ( flNewValue < m_flMin )
	{
		DevMsg("Math Counter %s ignoring SETMAXVALUENOFIRE because the value is less than the Minimum Value!\n", GetDebugName() );
		return;
	}

	m_flMax = flNewValue;

	float flCurValue = m_OutValue.Get();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flCurValue = clamp(flCurValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flCurValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for updating the MAX value.
// Input  : Float value to set.
//-----------------------------------------------------------------------------
void CMathCounter::InputSetMinValueNoFire( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SETMINVALUENOFIRE because it is disabled\n", GetDebugName() );
		return;
	}

	float flNewValue = inputdata.value.Float();
	if ( flNewValue > m_flMax )
	{
		DevMsg("Math Counter %s ignoring SETMINVALUENOFIRE because the value is greater than the Maximum Value!\n", GetDebugName() );
		return;
	}

	m_flMin = flNewValue;

	float flCurValue = m_OutValue.Get();
	if (( m_flMin != 0 ) || (m_flMax != 0 ))
	{
		flCurValue = clamp(flCurValue, m_flMin, m_flMax);
	}

	m_OutValue.Init( flCurValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for subtracting from the current value.
// Input  : Float value to subtract.
//-----------------------------------------------------------------------------
void CMathCounter::InputSubtract( inputdata_t &inputdata )
{
	if( m_bDisabled )
	{
		DevMsg("Math Counter %s ignoring SUBTRACT because it is disabled\n", GetDebugName() );
		return;
	}

	float fNewValue = m_OutValue.Get() - inputdata.value.Float();
	UpdateOutValue( inputdata.pActivator, fNewValue );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputGetValue( inputdata_t &inputdata )
{
	float flOutValue = m_OutValue.Get();
	m_OnGetValue.Set( flOutValue, inputdata.pActivator, inputdata.pCaller );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMathCounter::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the value to the new value, clamping and firing the output value.
// Input  : fNewValue - Value to set.
//-----------------------------------------------------------------------------
void CMathCounter::UpdateOutValue(CBaseEntity *pActivator, float fNewValue)
{
	if ((m_flMin != 0) || (m_flMax != 0))
	{
		//
		// Fire an output any time we reach or exceed our maximum value.
		//
		if ( fNewValue >= m_flMax )
		{
			if ( !m_bHitMax )
			{
				m_bHitMax = true;
				m_OnHitMax.FireOutput( pActivator, this );
			}
		}
		else
		{
			// Fire an output if we just changed from the maximum value
			if ( m_OutValue.Get() == m_flMax )
			{
				m_OnChangedFromMax.FireOutput( pActivator, this );
			}
			
			m_bHitMax = false;
		}

		//
		// Fire an output any time we reach or go below our minimum value.
		//
		if ( fNewValue <= m_flMin )
		{
			if ( !m_bHitMin )
			{
				m_bHitMin = true;
				m_OnHitMin.FireOutput( pActivator, this );
			}
		}
		else
		{
			// Fire an output if we just changed from the minimum value
			if ( m_OutValue.Get() == m_flMin )
			{
				m_OnChangedFromMin.FireOutput( pActivator, this );
			}
			m_bHitMin = false;
		}

		fNewValue = clamp(fNewValue, m_flMin, m_flMax);
	}

	m_OutValue.Set(fNewValue, pActivator, this);
}

//-----------------------------------------------------------------------------
// Purpose: Compares a single string input to up to 16 case values, firing an
//			output corresponding to the case value that matched, or a default
//			output if the input value didn't match any of the case values.
//
//			This can also be used to fire a random output from a set of outputs.
//-----------------------------------------------------------------------------
static constexpr auto MAX_LOGIC_CASES = 16;

class CLogicCase : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCase, CLogicalEntity );
private:
	[[= ks::reflect::Key{ .name = "Case16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Case15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Case14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Case13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Case12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Case11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Case10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Case09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Case08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Case07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Case06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Case05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Case04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Case03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Case02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Case01", .index = 0 } ]] string_t m_nCase[MAX_LOGIC_CASES];

	int m_nShuffleCases;
	int m_nLastShuffleCase;
	unsigned char m_uchShuffleCaseMap[MAX_LOGIC_CASES];

	public: void Spawn_Impl(void);
	private:

	int BuildCaseMap(unsigned char *puchMap);

	// Inputs
	[[= ks::reflect::Input{ .name = "InValue", .type = FIELD_INPUT } ]] void InputValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PickRandom", .type = FIELD_VOID } ]] void InputPickRandom( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PickRandomShuffle", .type = FIELD_VOID } ]] void InputPickRandomShuffle( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnCase01", .index = 0 } ]] [[= ks::reflect::Key{ .name = "OnCase02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "OnCase03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "OnCase04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "OnCase05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "OnCase06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "OnCase07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "OnCase08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "OnCase09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "OnCase10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "OnCase11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "OnCase12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "OnCase13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "OnCase14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "OnCase15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "OnCase16", .index = 15 } ]] COutputEvent m_OnCase[MAX_LOGIC_CASES];		// Fired when the input value matches one of the case values.
	[[= ks::reflect::Key{ .name = "OnDefault" } ]] COutputVariant m_OnDefault;					// Fired when no match was found.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_case, CLogicCase);

IMPLEMENT_REFLECT_DATAMAP( CLogicCase )

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CLogicCase::Spawn_Impl( void )
{
	m_nLastShuffleCase = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Evaluates the new input value, firing the appropriate OnCaseX output
//			if the input value matches one of the "CaseX" keys.
// Input  : Value - Variant value to compare against the values of the case fields.
//				We use a variant so that we can convert any input type to a string.
//-----------------------------------------------------------------------------
void CLogicCase::InputValue( inputdata_t &inputdata )
{
	const char *pszValue = inputdata.value.String();
	for (int i = 0; i < MAX_LOGIC_CASES; i++)
	{
		if ((m_nCase[i] != NULL_STRING) && !stricmp(STRING(m_nCase[i]), pszValue))
		{
			m_OnCase[i].FireOutput( inputdata.pActivator, this );
			return;
		}
	}
	
	m_OnDefault.Set( inputdata.value, inputdata.pActivator, this );
}

//-----------------------------------------------------------------------------
// Count the number of valid cases, building a packed array
// that maps 0..NumCases to the actual CaseX values.
//
// This allows our zany mappers to set up cases sparsely if they desire.
// NOTE: assumes pnMap points to an array of MAX_LOGIC_CASES
//-----------------------------------------------------------------------------
int CLogicCase::BuildCaseMap(unsigned char *puchCaseMap)
{
	std::memset(puchCaseMap, 0, sizeof(unsigned char) * MAX_LOGIC_CASES);

	int nNumCases = 0;
	for (int i = 0; i < MAX_LOGIC_CASES; i++)
	{
		if (m_OnCase[i].NumberOfElements() > 0)
		{
			puchCaseMap[nNumCases] = (unsigned char)i;
			nNumCases++;
		}
	}
	
	return nNumCases;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the case statement choose a case at random.
//-----------------------------------------------------------------------------
void CLogicCase::InputPickRandom( inputdata_t &inputdata )
{
	unsigned char uchCaseMap[MAX_LOGIC_CASES];
	int nNumCases = BuildCaseMap( uchCaseMap );

	//
	// Choose a random case from the ones that were set up by the level designer.
	//
	if ( nNumCases > 0 )
	{
		int nRandom = random_valve->RandomInt(0, nNumCases - 1);
		int nCase = (unsigned char)uchCaseMap[nRandom];

		contract_assert( nCase < MAX_LOGIC_CASES );

		if (nCase < MAX_LOGIC_CASES)
		{
			m_OnCase[nCase].FireOutput( inputdata.pActivator, this );
		}
	}
	else
	{
		DevMsg( 1, "Firing PickRandom input on logic_case %s with no cases set up\n", GetDebugName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes the case statement choose a case at random.
//-----------------------------------------------------------------------------
void CLogicCase::InputPickRandomShuffle( inputdata_t &inputdata )
{
	int nAvoidCase = -1;
	int nCaseCount = m_nShuffleCases;
	
	if ( nCaseCount == 0 )
	{
		// Starting a new shuffle batch.
		nCaseCount = m_nShuffleCases = BuildCaseMap( m_uchShuffleCaseMap );
		
		if ( ( m_nShuffleCases > 1 ) && ( m_nLastShuffleCase != -1 ) )
		{
			// Remove the previously picked case from the case map for this pick only.
			// This avoids repeats across shuffle batch boundaries.		
			nAvoidCase = m_nLastShuffleCase;
			
			for (int i = 0; i < m_nShuffleCases; i++ )
			{
				if ( m_uchShuffleCaseMap[i] == nAvoidCase )
				{
					unsigned char uchSwap = m_uchShuffleCaseMap[i];
					m_uchShuffleCaseMap[i] = m_uchShuffleCaseMap[nCaseCount - 1];
					m_uchShuffleCaseMap[nCaseCount - 1] = uchSwap;
					nCaseCount--;
					break;
				}
			}
		}
	}
	
	//
	// Choose a random case from the ones that were set up by the level designer.
	// Never repeat a case within a shuffle batch, nor consecutively across batches.
	//
	if ( nCaseCount > 0 )
	{
		int nRandom = random_valve->RandomInt( 0, nCaseCount - 1 );

		int nCase = m_uchShuffleCaseMap[nRandom];
		contract_assert( nCase < MAX_LOGIC_CASES );

		if (nCase < MAX_LOGIC_CASES)
		{
			m_OnCase[nCase].FireOutput( inputdata.pActivator, this );
		}
		
		m_uchShuffleCaseMap[nRandom] = m_uchShuffleCaseMap[m_nShuffleCases - 1];
		m_nShuffleCases--;

		m_nLastShuffleCase = nCase;
	}
	else
	{
		DevMsg( 1, "Firing PickRandom input on logic_case %s with no cases set up\n", GetDebugName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Compares a floating point input to a predefined value, firing an
//			output to indicate the result of the comparison.
//-----------------------------------------------------------------------------
class CLogicCompare : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCompare, CLogicalEntity );

public:
	int DrawDebugTextOverlays_Impl(void);

private:
	// Inputs
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_FLOAT } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueCompare", .type = FIELD_FLOAT } ]] void InputSetValueCompare( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCompareValue", .type = FIELD_FLOAT } ]] void InputSetCompareValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Compare", .type = FIELD_VOID } ]] void InputCompare( inputdata_t &inputdata );

	void DoCompare(CBaseEntity *pActivator, float flInValue);

	[[= ks::reflect::Key{ .name = "InitialValue" } ]] float m_flInValue;					// Place to hold the last input value for a recomparison.
	[[= ks::reflect::Key{ .name = "CompareValue" } ]] float m_flCompareValue;				// The value to compare the input value against.

	// Outputs
	[[= ks::reflect::Key{ .name = "OnLessThan" } ]] COutputFloat m_OnLessThan;			// Fired when the input value is less than the compare value.
	[[= ks::reflect::Key{ .name = "OnEqualTo" } ]] COutputFloat m_OnEqualTo;			// Fired when the input value is equal to the compare value.
	[[= ks::reflect::Key{ .name = "OnNotEqualTo" } ]] COutputFloat m_OnNotEqualTo;		// Fired when the input value is not equal to the compare value.
	[[= ks::reflect::Key{ .name = "OnGreaterThan" } ]] COutputFloat m_OnGreaterThan;		// Fired when the input value is greater than the compare value.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_compare, CLogicCompare);

IMPLEMENT_REFLECT_DATAMAP( CLogicCompare )

//-----------------------------------------------------------------------------
// Purpose: Input handler for a new input value without performing a comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetValue( inputdata_t &inputdata )
{
	m_flInValue = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for a setting a new value and doing the comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetValueCompare( inputdata_t &inputdata )
{
	m_flInValue = inputdata.value.Float();
	DoCompare( inputdata.pActivator, m_flInValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for a new input value without performing a comparison.
//-----------------------------------------------------------------------------
void CLogicCompare::InputSetCompareValue( inputdata_t &inputdata )
{
	m_flCompareValue = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing a recompare of the last input value.
//-----------------------------------------------------------------------------
void CLogicCompare::InputCompare( inputdata_t &inputdata )
{
	DoCompare( inputdata.pActivator, m_flInValue );
}

//-----------------------------------------------------------------------------
// Purpose: Compares the input value to the compare value, firing the appropriate
//			output(s) based on the comparison result.
// Input  : flInValue - Value to compare against the comparison value.
//-----------------------------------------------------------------------------
void CLogicCompare::DoCompare(CBaseEntity *pActivator, float flInValue)
{
	if (flInValue == m_flCompareValue)
	{
		m_OnEqualTo.Set(flInValue, pActivator, this);
	}
	else
	{
		m_OnNotEqualTo.Set(flInValue, pActivator, this);

		if (flInValue > m_flCompareValue)
		{
			m_OnGreaterThan.Set(flInValue, pActivator, this);
		}
		else
		{
			m_OnLessThan.Set(flInValue, pActivator, this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CLogicCompare::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print duration
		V_snprintf(tempstr,sizeof(tempstr),"    Initial Value: %f", m_flInValue);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print hold time
		V_snprintf(tempstr,sizeof(tempstr),"    Compare Value: %f", m_flCompareValue);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Tests a boolean value, firing an output to indicate whether the
//			value was true or false.
//-----------------------------------------------------------------------------
class CLogicBranch : public CLogicalEntity
{
	DECLARE_CLASS( CLogicBranch, CLogicalEntity );
	
public:

	void UpdateOnRemove_Impl();

	void AddLogicBranchListener( CBaseEntity *pEntity );
	inline bool GetLogicBranchState();
	int DrawDebugTextOverlays_Impl( void );

private:

	enum LogicBranchFire_t
	{
		LOGIC_BRANCH_FIRE,
		LOGIC_BRANCH_NO_FIRE,
	};

	// Inputs
	[[= ks::reflect::Input{ .name = "SetValue", .type = FIELD_BOOLEAN } ]] void InputSetValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetValueTest", .type = FIELD_BOOLEAN } ]] void InputSetValueTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleTest", .type = FIELD_VOID } ]] void InputToggleTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_VOID } ]] void InputTest( inputdata_t &inputdata );

	void UpdateValue(bool bNewValue, CBaseEntity *pActivator, LogicBranchFire_t eFire);

	[[= ks::reflect::Key{ .name = "InitialValue" } ]] bool m_bInValue;					// Place to hold the last input value for a future test.
	
	std::vector< EHANDLE > m_Listeners;	// A list of logic_branch_listeners that are monitoring us.

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrue" } ]] COutputEvent m_OnTrue;				// Fired when the value is true.
	[[= ks::reflect::Key{ .name = "OnFalse" } ]] COutputEvent m_OnFalse;				// Fired when the value is false.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_branch, CLogicBranch);

IMPLEMENT_REFLECT_DATAMAP( CLogicBranch )

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranch::UpdateOnRemove_Impl()
{
	for ( int i = 0; i < std::ssize( m_Listeners ); i++ )
	{
		CBaseEntity *pEntity = m_Listeners[ i ].Get();
		if ( pEntity )
		{
			g_EventQueue.AddEvent( this, "_OnLogicBranchRemoved", 0, this, this );
		}
	}
	
	BaseClass::UpdateOnRemove_Impl();
}
	
//-----------------------------------------------------------------------------
// Purpose: Input handler to set a new input value without firing outputs.
// Input  : Boolean value to set.
//-----------------------------------------------------------------------------
void CLogicBranch::InputSetValue( inputdata_t &inputdata )
{
	UpdateValue( inputdata.value.Bool(), inputdata.pActivator, LOGIC_BRANCH_NO_FIRE );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to set a new input value and fire appropriate outputs.
// Input  : Boolean value to set.
//-----------------------------------------------------------------------------
void CLogicBranch::InputSetValueTest( inputdata_t &inputdata )
{
	UpdateValue( inputdata.value.Bool(), inputdata.pActivator, LOGIC_BRANCH_FIRE );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the boolean value without firing outputs.
//-----------------------------------------------------------------------------
void CLogicBranch::InputToggle( inputdata_t &inputdata )
{
	UpdateValue( !m_bInValue, inputdata.pActivator, LOGIC_BRANCH_NO_FIRE );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the boolean value and then firing the
//			appropriate output based on the new value.
//-----------------------------------------------------------------------------
void CLogicBranch::InputToggleTest( inputdata_t &inputdata )
{
	UpdateValue( !m_bInValue, inputdata.pActivator, LOGIC_BRANCH_FIRE );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing a test of the last input value.
//-----------------------------------------------------------------------------
void CLogicBranch::InputTest( inputdata_t &inputdata )
{
	UpdateValue( m_bInValue, inputdata.pActivator, LOGIC_BRANCH_FIRE );
}

//-----------------------------------------------------------------------------
// Purpose: Tests the last input value, firing the appropriate output based on
//			the test result.
// Input  : bInValue - 
//-----------------------------------------------------------------------------
void CLogicBranch::UpdateValue( bool bNewValue, CBaseEntity *pActivator, LogicBranchFire_t eFire )
{
	if ( m_bInValue != bNewValue )
	{
		m_bInValue = bNewValue;

		for ( int i = 0; i < std::ssize( m_Listeners ); i++ )
		{
			CBaseEntity *pEntity = m_Listeners[ i ].Get();
			if ( pEntity )
			{
				g_EventQueue.AddEvent( pEntity, "_OnLogicBranchChanged", 0, this, this );
			}
		}
	}

	if ( eFire == LOGIC_BRANCH_FIRE )
	{
		if ( m_bInValue )
		{
			m_OnTrue.FireOutput( pActivator, this );
		}
		else
		{
			m_OnFalse.FireOutput( pActivator, this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Accessor for logic_branchlist to test the value of the branch on demand.
//-----------------------------------------------------------------------------
bool CLogicBranch::GetLogicBranchState()
{
	return m_bInValue;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranch::AddLogicBranchListener( CBaseEntity *pEntity )
{
	if ( ks::find_index( m_Listeners, pEntity ) == -1 )
	{
		m_Listeners.push_back( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLogicBranch::DrawDebugTextOverlays_Impl( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print refire time
		V_snprintf( tempstr, sizeof(tempstr), "Branch value: %s", (m_bInValue) ? "TRUE" : "FALSE" );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;
	}

	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Autosaves when triggered
//-----------------------------------------------------------------------------
class CLogicAutosave : public CLogicalEntity
{
	DECLARE_CLASS( CLogicAutosave, CLogicalEntity );

protected:
	// Inputs
	[[= ks::reflect::Input{ .name = "Save", .type = FIELD_VOID } ]] void InputSave( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SaveDangerous", .type = FIELD_FLOAT } ]] void InputSaveDangerous( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinHitpointsThreshold", .type = FIELD_INTEGER } ]] void InputSetMinHitpointsThreshold( inputdata_t &inputdata );

	DECLARE_DATADESC();
	[[= ks::reflect::Key{ .name = "NewLevelUnit" } ]] bool m_bForceNewLevelUnit;
	[[= ks::reflect::Key{ .name = "MinimumHitPoints" } ]] int m_minHitPoints;
	[[= ks::reflect::Key{ .name = "MinHitPointsToCommit" } ]] int m_minHitPointsToCommit;
};

LINK_ENTITY_TO_CLASS(logic_autosave, CLogicAutosave);

IMPLEMENT_REFLECT_DATAMAP( CLogicAutosave )

//-----------------------------------------------------------------------------
// Purpose: Save!
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSave( inputdata_t &inputdata )
{
	if ( m_bForceNewLevelUnit )
	{
		engine->ClearSaveDir();
	}

	engine->ServerCommand( "autosave\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Save safely!
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSaveDangerous( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );

	if ( !pPlayer ) 
		return;

	if ( g_ServerGameDLL.m_fAutoSaveDangerousTime != 0.0f && g_ServerGameDLL.m_fAutoSaveDangerousTime >= gpGlobals->curtime )
	{
		// A previous dangerous auto save was waiting to become safe

		if ( pPlayer->GetDeathTime() == 0.0f || pPlayer->GetDeathTime() > gpGlobals->curtime )
		{
			// The player isn't dead, so make the dangerous auto save safe
			engine->ServerCommand( "autosavedangerousissafe\n" );
		}
	}

	if ( m_bForceNewLevelUnit )
	{
		engine->ClearSaveDir();
	}

	if ( pPlayer->GetHealth() >= m_minHitPoints )
	{
		engine->ServerCommand( "autosavedangerous\n" );
		g_ServerGameDLL.m_fAutoSaveDangerousTime = gpGlobals->curtime + inputdata.value.Float();

		// Player must have this much health when we go to commit, or we don't commit.
		g_ServerGameDLL.m_fAutoSaveDangerousMinHealthToCommit = m_minHitPointsToCommit;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Autosaves when triggered
//-----------------------------------------------------------------------------
class CLogicActiveAutosave : public CLogicAutosave
{
	DECLARE_CLASS( CLogicActiveAutosave, CLogicAutosave );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]]
	void InputEnable( inputdata_t &inputdata )
	{
		m_flStartTime = -1;
		SetThink( &CLogicActiveAutosave::SaveThink );
		SetNextThink( gpGlobals->curtime );
	}

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]]
	void InputDisable( inputdata_t &inputdata )
	{
		SetThink( nullptr );
	}

	void SaveThink()
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer )
		{
			if ( m_flStartTime < 0 )
			{
				if ( pPlayer->GetHealth() <= m_minHitPoints )
				{
					m_flStartTime = gpGlobals->curtime;
				}
			}
			else
			{
				if ( pPlayer->GetHealth() >= m_TriggerHitPoints )
				{
					inputdata_t inputdata;
					DevMsg( 2, "logic_active_autosave (%s, %d) triggered\n", STRING( GetEntityName() ), entindex() );
					if ( !m_flDangerousTime )
					{
						InputSave( inputdata );
					}
					else
					{
						inputdata.value.SetFloat( m_flDangerousTime );
						InputSaveDangerous( inputdata );
					}
					m_flStartTime = -1;
				}
				else if ( m_flTimeToTrigger > 0 && gpGlobals->curtime - m_flStartTime > m_flTimeToTrigger )
				{
					m_flStartTime = -1;
				}
			}
		}

		float thinkInterval = ( m_flStartTime < 0 ) ? 1.0 : 0.5;
		SetNextThink( gpGlobals->curtime + thinkInterval );
	}

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "TriggerHitPoints" } ]] int m_TriggerHitPoints;
	[[= ks::reflect::Key{ .name = "TimeToTrigger" } ]] float m_flTimeToTrigger;
	float m_flStartTime;
	[[= ks::reflect::Key{ .name = "DangerousTime" } ]] float m_flDangerousTime;
};

LINK_ENTITY_TO_CLASS(logic_active_autosave, CLogicActiveAutosave);

IMPLEMENT_REFLECT_DATAMAP( CLogicActiveAutosave )

//-----------------------------------------------------------------------------
// Purpose: Keyfield set func
//-----------------------------------------------------------------------------
void CLogicAutosave::InputSetMinHitpointsThreshold( inputdata_t &inputdata )
{
	int setTo = inputdata.value.Int();
	contract_assert( ( setTo >= 0 && setTo <= 100 ) && "Tried to set autosave MinHitpointsThreshold to %d!\n" );
	m_minHitPoints = setTo;
}

// Finds the named physics object.  If no name, returns the world
// If a name is specified and an object not found - errors are reported
IPhysicsObject *FindPhysicsObjectByNameOrWorld( string_t name, CBaseEntity *pErrorEntity )
{
	if ( !name )
		return g_PhysWorldObject;

	IPhysicsObject *pPhysics = FindPhysicsObjectByName( name.ToCStr(), pErrorEntity );
	if ( !pPhysics )
	{
		DevWarning("%s: can't find %s\n", pErrorEntity->GetClassname(), name.ToCStr());
	}
	return pPhysics;
}

class CLogicCollisionPair : public CLogicalEntity
{
	DECLARE_CLASS( CLogicCollisionPair, CLogicalEntity );
public:

	void EnableCollisions( bool bEnable )
	{
		IPhysicsObject *pPhysics0 = FindPhysicsObjectByNameOrWorld( m_nameAttach1, this );
		IPhysicsObject *pPhysics1 = FindPhysicsObjectByNameOrWorld( m_nameAttach2, this );

		// need two different objects to do anything
		if ( pPhysics0 && pPhysics1 && pPhysics0 != pPhysics1 )
		{
			m_disabled = !bEnable;
			m_succeeded = true;
			if ( bEnable )
			{
				PhysEnableEntityCollisions( pPhysics0, pPhysics1 );
			}
			else
			{
				PhysDisableEntityCollisions( pPhysics0, pPhysics1 );
			}
		}
		else
		{
			m_succeeded = false;
		}
	}

	void Activate_Impl( void )
	{
		if ( m_disabled )
		{
			EnableCollisions( false );
		}
		BaseClass::Activate_Impl();
	}

	[[= ks::reflect::Input{ .name = "DisableCollisions", .type = FIELD_VOID } ]]
	void InputDisableCollisions( inputdata_t &inputdata )
	{
		if ( m_succeeded && m_disabled )
			return;
		EnableCollisions( false );
	}

	[[= ks::reflect::Input{ .name = "EnableCollisions", .type = FIELD_VOID } ]]
	void InputEnableCollisions( inputdata_t &inputdata )
	{
		if ( m_succeeded && !m_disabled )
			return;
		EnableCollisions( true );
	}
	// If Activate() becomes PostSpawn()
	//void OnRestore() { Activate(); }

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t		m_nameAttach1;
	[[= ks::reflect::Key{ .name = "attach2" } ]] string_t		m_nameAttach2;
	[[= ks::reflect::Key{ .name = "startdisabled" } ]] bool			m_disabled;
	bool			m_succeeded;
};

IMPLEMENT_REFLECT_DATAMAP( CLogicCollisionPair )

LINK_ENTITY_TO_CLASS( logic_collision_pair, CLogicCollisionPair );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static constexpr auto MAX_LOGIC_BRANCH_NAMES = 16;

class CLogicBranchList : public CLogicalEntity
{
	DECLARE_CLASS( CLogicBranchList, CLogicalEntity );

	public: void Spawn_Impl();
	private:
	public: void Activate_Impl();
	private:
	public: int DrawDebugTextOverlays_Impl( void );
	private:

private:

	enum LogicBranchListenerLastState_t
	{
		LOGIC_BRANCH_LISTENER_NOT_INIT = 0,
		LOGIC_BRANCH_LISTENER_ALL_TRUE,
		LOGIC_BRANCH_LISTENER_ALL_FALSE,
		LOGIC_BRANCH_LISTENER_MIXED,
	};

	void DoTest( CBaseEntity *pActivator );

	[[= ks::reflect::Key{ .name = "Branch16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Branch15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Branch14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Branch13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Branch12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Branch11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Branch10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Branch09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Branch08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Branch07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Branch06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Branch05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Branch04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Branch03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Branch02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Branch01", .index = 0 } ]] string_t m_nLogicBranchNames[MAX_LOGIC_BRANCH_NAMES];
	std::vector< EHANDLE > m_LogicBranchList;
	LogicBranchListenerLastState_t m_eLastState;

	// Inputs
	[[= ks::reflect::Input{ .name = "_OnLogicBranchRemoved", .type = FIELD_INPUT } ]] void Input_OnLogicBranchRemoved( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_OnLogicBranchChanged", .type = FIELD_INPUT } ]] void Input_OnLogicBranchChanged( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_INPUT } ]] void InputTest( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnAllTrue" } ]] COutputEvent m_OnAllTrue;			// Fired when all the registered logic_branches become true.
	[[= ks::reflect::Key{ .name = "OnAllFalse" } ]] COutputEvent m_OnAllFalse;			// Fired when all the registered logic_branches become false.
	[[= ks::reflect::Key{ .name = "OnMixed" } ]] COutputEvent m_OnMixed;				// Fired when one of the registered logic branches changes, but not all are true or false.

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(logic_branch_listener, CLogicBranchList);

IMPLEMENT_REFLECT_DATAMAP( CLogicBranchList )

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CLogicBranchList::Spawn_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Finds all the logic_branches that we are monitoring and register ourselves with them.
//-----------------------------------------------------------------------------
void CLogicBranchList::Activate_Impl( void )
{
	for ( int i = 0; i < MAX_LOGIC_BRANCH_NAMES; i++ )
	{
		CBaseEntity *pEntity = nullptr;
		while ( ( pEntity = gEntList.FindEntityGeneric( pEntity, STRING( m_nLogicBranchNames[i] ), this ) ) != nullptr )
		{
			if ( FClassnameIs( pEntity, "logic_branch" ) )
			{
				CLogicBranch *pBranch = (CLogicBranch *)pEntity;
				pBranch->AddLogicBranchListener( this );
				m_LogicBranchList.push_back( pBranch );
			}
			else
			{
				DevWarning( "logic_branchlist %s refers to entity %s, which is not a logic_branch\n", GetDebugName(), pEntity->GetDebugName() );
			}
		}
	}
	
	BaseClass::Activate_Impl();
}

//-----------------------------------------------------------------------------
// Called when a monitored logic branch is deleted from the world, since that
// might affect our final result.
//-----------------------------------------------------------------------------
void CLogicBranchList::Input_OnLogicBranchRemoved( inputdata_t &inputdata )
{
	int nIndex = ks::find_index( m_LogicBranchList, inputdata.pActivator );
	if ( nIndex != -1 )
	{
		ks::swap_remove( m_LogicBranchList, nIndex );
	}

	// See if this logic_branch's deletion affects the final result.
	DoTest( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Called when the value of a monitored logic branch changes.
//-----------------------------------------------------------------------------
void CLogicBranchList::Input_OnLogicBranchChanged( inputdata_t &inputdata )
{
	DoTest( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Input handler to manually test the monitored logic branches and fire the
// appropriate output.
//-----------------------------------------------------------------------------
void CLogicBranchList::InputTest( inputdata_t &inputdata )
{
	// Force an output.
	m_eLastState = LOGIC_BRANCH_LISTENER_NOT_INIT;

	DoTest( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CLogicBranchList::DoTest( CBaseEntity *pActivator )
{
	bool bOneTrue = false;
	bool bOneFalse = false;
	
	for ( int i = 0; i < std::ssize( m_LogicBranchList ); i++ )
	{
		CLogicBranch *pBranch = (CLogicBranch *)m_LogicBranchList[ i ].Get();
		if ( pBranch && pBranch->GetLogicBranchState() )
		{
			bOneTrue = true;
		}
		else
		{
			bOneFalse = true;
		}
	}

	// Only fire the output if the new result differs from the last result.
	if ( bOneTrue && !bOneFalse )
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_ALL_TRUE )
		{
			m_OnAllTrue.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_ALL_TRUE;
		}
	}
	else if ( bOneFalse && !bOneTrue )
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_ALL_FALSE )
		{
			m_OnAllFalse.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_ALL_FALSE;
		}
	}
	else
	{
		if ( m_eLastState != LOGIC_BRANCH_LISTENER_MIXED )
		{
			m_OnMixed.FireOutput( pActivator, this );
			m_eLastState = LOGIC_BRANCH_LISTENER_MIXED;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLogicBranchList::DrawDebugTextOverlays_Impl( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		for ( int i = 0; i < std::ssize( m_LogicBranchList ); i++ )
		{
			CLogicBranch *pBranch = (CLogicBranch *)m_LogicBranchList[ i ].Get();
			if ( pBranch )
			{
				V_snprintf( tempstr, sizeof(tempstr), "Branch (%s): %s", STRING(pBranch->GetEntityName()), (pBranch->GetLogicBranchState()) ? "TRUE" : "FALSE" );
				EntityText( text_offset, tempstr, 0 );
				text_offset++;
			}
		}
	}

	return text_offset;
}
