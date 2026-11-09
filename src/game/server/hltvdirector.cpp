// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// hltvdirector.cpp: implementation of the CHLTVDirector class.
//
//////////////////////////////////////////////////////////////////////

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "ndebugoverlay.h"
#include "hltvdirector.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static ConVar tv_delay( "tv_delay", "10", FCVAR_RELEASE, "GOTV broadcast delay in seconds", true, 0, true, HLTV_MAX_DELAY );
static ConVar tv_allow_static_shots( "tv_allow_static_shots", "1", FCVAR_RELEASE, "Auto director uses fixed level cameras for shots" );


constexpr auto RANDOM_MAX_ELEMENTS = 256;
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

#if !defined( CSTRIKE_DLL ) && !defined( TF_DLL )// add your mod here if you use your own director

static CHLTVDirector s_HLTVDirector;	// singleton

static InterfaceReg s_Reg_CHLTVDirector_IHLTVDirector( []() -> void * { return static_cast<IHLTVDirector *>( &s_HLTVDirector ); }, INTERFACEVERSION_HLTVDIRECTOR );;

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}

#endif // MODs



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

#ifdef _DEBUG
	CheckHistory();
#endif
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
