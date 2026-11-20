// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "usermessages_schema.h"
#include "vphysics_interface.h"
#include "ai_criteria.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "edict.h"
#include "gamerules_register.h"
#include "entitydefs.h"
#include "baseentity.h"
#include "enginecallback.h"
#include "reflect_annotations.h"
#include "const.h"
#include "declare_class.h"
#include "worldsize.h"
#include "filesystem.h"
#include "tier2/tier2_logging.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.types;
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
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.dbg;
import ks.core.platform;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.core.memalloc;
import ks.core.vprof;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.core.wchartypes;
import ks.core.threadtools;

class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSPlayer;
class CRecipientFilter;
class CTeam;
class CTeamSpawnPoint;

using namespace ::ks::reflect;

// Purpose: Team management class. Contains all the details for a specific team

std::vector< CTeam * > g_Teams;

extern ConVar mp_radar_showall;

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the Team's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void SendProxy_PlayerList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTeam *pTeam = (CTeam*)pData;

	// If this assertion fails, then SendProxyArrayLength_PlayerArray must have failed.
	contract_assert( iElement < std::ssize( pTeam->m_aPlayers ) );

	CBasePlayer *pPlayer = pTeam->m_aPlayers[iElement];
	pOut->m_Int = pPlayer->entindex();
}

int SendProxyArrayLength_PlayerArray( const void *pStruct, int objectID )
{
	CTeam *pTeam = (CTeam*)pStruct;
	return std::ssize( pTeam->m_aPlayers );
}

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CTeam, DT_Team )

LINK_ENTITY_TO_CLASS( team_manager, CTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified team manager
//-----------------------------------------------------------------------------
CTeam *GetGlobalTeam( int iIndex )
{
	if ( iIndex < 0 || iIndex >= GetNumberOfTeams() )
		return nullptr;

	return g_Teams[ iIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of team managers
//-----------------------------------------------------------------------------
int GetNumberOfTeams( void )
{
	return std::ssize( g_Teams );
}

const char* GetTeamName( int iTeam )
{
	CTeam *pTeam = GetGlobalTeam( iTeam );
	if ( pTeam )
	{
		return pTeam->GetName();
	}
	else
	{
		return "UNKNOWN TEAM";
	}
}

int CTeam::m_nStaticGGLeader_CT = -1;
int CTeam::m_nStaticGGLeader_T = -1;

//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
CTeam::CTeam( void )
{
	std::memset( m_szTeamname.GetForModify(), 0, sizeof(m_szTeamname) );
	memset( m_szClanTeamname.GetForModify(), 0, sizeof(m_szClanTeamname) );
	memset( m_szTeamFlagImage.GetForModify(), 0, sizeof(m_szTeamFlagImage) );
	memset( m_szTeamLogoImage.GetForModify(), 0, sizeof(m_szTeamLogoImage) );
	memset( m_szTeamMatchStat.GetForModify(), 0, sizeof( m_szTeamMatchStat ) );
	m_numMapVictories.GetForModify() = 0;
	ResetTeamLeaders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeam::~CTeam( void )
{
	m_aSpawnPoints.clear(); m_aSpawnPoints.shrink_to_fit();
	m_aPlayers.clear(); m_aPlayers.shrink_to_fit();
}

void CTeam::ResetTeamLeaders()
{
	m_flLastPlayerSortTime = 0;
	m_nGGLeaderEntIndex_CT = -1;
	m_nGGLeaderEntIndex_T = -1;

	m_bGGHasLeader_CT = false;
	m_bGGHasLeader_T = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CTeam::Think_Impl( void )
{
	if ( m_flLastPlayerSortTime + 0.025f < gpGlobals->curtime )
	{
		DetermineGGLeaderAndSort();
	}
}

void CTeam::DetermineGGLeaderAndSort( void )
{
	std::vector< CCSPlayer* >	playerList_CT;
	std::vector< CCSPlayer* >	playerList_T;

	int iNumPlayers = GetNumPlayers();
	//		m_aPlayers.Sort( PlayerSortFunction );
	for ( int i = 0; i < iNumPlayers; i++ )
	{
		CCSPlayer *player = static_cast< CCSPlayer* >( GetPlayer( i ) );
		if ( player )
		{
			if ( player->GetTeamNumber() == TEAM_CT )
			{
				if ( player->GetPlayerGunGameWeaponIndex() > 0 )
					m_bGGHasLeader_CT = true;

				playerList_CT.push_back( player );
			}
			else if ( player->GetTeamNumber() == TEAM_TERRORIST )
			{
				if ( player->GetPlayerGunGameWeaponIndex() > 0 )
					m_bGGHasLeader_T = true;

				playerList_T.push_back( player );
			}
		}
	}

	m_nStaticGGLeader_CT = m_nGGLeaderEntIndex_CT;
	m_nStaticGGLeader_T = m_nGGLeaderEntIndex_T;

	ks::qsort( playerList_CT, TeamGGSortFunction );
	ks::qsort( playerList_T, TeamGGSortFunction );

	if ( !playerList_CT.empty() && m_bGGHasLeader_CT )
		m_nGGLeaderEntIndex_CT = playerList_CT[0]->entindex();
	if ( !playerList_T.empty() && m_bGGHasLeader_T )
		m_nGGLeaderEntIndex_T = playerList_T[0]->entindex();

	// send an event down so the client gets it asap
	if ( m_nLastGGLeader_CT != m_nGGLeaderEntIndex_CT || m_nLastGGLeader_T != m_nGGLeaderEntIndex_T )
	{
		int nLeaderIndex = m_nGGLeaderEntIndex_CT;
		if ( m_nLastGGLeader_T != m_nGGLeaderEntIndex_T )
			nLeaderIndex = m_nGGLeaderEntIndex_T;

		for ( int i = 1; i <= iNumPlayers; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->entindex() == nLeaderIndex )
			{
				//if we made it this far, we are the current leader
				IGameEvent *event = gameeventmanager->CreateEvent( "gg_team_leader" );
				if ( event )
				{
					event->SetInt( "playerid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
				break;
			}
		}
	}
	m_nLastGGLeader_CT = m_nGGLeaderEntIndex_CT;
	m_nLastGGLeader_T = m_nGGLeaderEntIndex_T;
}

int CTeam::GetGGLeader( int nTeam )
{
	if ( nTeam == TEAM_CT )
		return m_nGGLeaderEntIndex_CT;
	else if ( nTeam == TEAM_TERRORIST )
		return m_nGGLeaderEntIndex_T;

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting players in valve containers
//-----------------------------------------------------------------------------
int CTeam::TeamGGSortFunction( CCSPlayer* const *entry1, CCSPlayer* const *entry2 )
{
	// bail out early if either player is an empty slot, i.e. has a player index of -1
	if ( entry1 == nullptr )
		return 1;
	if ( entry2 == nullptr )
		return -1;
	if ( (*entry1)->entindex() == -1 )
		return 1;
	if ( (*entry2)->entindex() == -1 )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( (*entry1)->GetPlayerGunGameWeaponIndex() > (*entry2)->GetPlayerGunGameWeaponIndex() )
		return -1;
	else if ( (*entry1)->GetPlayerGunGameWeaponIndex() < (*entry2)->GetPlayerGunGameWeaponIndex() )
		return 1;
	else
	{
		int nLeader = m_nStaticGGLeader_CT;
		if ( (*entry1)->GetTeamNumber() == TEAM_TERRORIST )
			nLeader = m_nStaticGGLeader_T;

		// Current GG leader always sorts in front in the case of a tie
		if ( (*entry1)->entindex() == nLeader )
			return -1;
		else if ( (*entry2)->entindex() == nLeader )
			return 1;
		//else
		//	return entry1->entindex() - entry2->entindex();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Teams are always transmitted to clients
//-----------------------------------------------------------------------------
int CTeam::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Visibility/scanners
//-----------------------------------------------------------------------------
int CTeam::ShouldTransmitToPlayer( CBasePlayer* pRecipient, CBaseEntity* pEntity )
{
	// Always transmit the observer target to players and to spectators
	if ( pRecipient && (pRecipient->IsObserver() && pRecipient->GetObserverTarget() == pEntity) )
		return FL_EDICT_ALWAYS;

	// Spec that isn't a coach
	if ( pRecipient->IsSpectator() )
		return FL_EDICT_ALWAYS;

	// Same team
	if ( pRecipient->GetAssociatedTeamNumber() == pEntity->GetTeamNumber() )
		return FL_EDICT_ALWAYS;

	// radar_show is 'all' or set to the target's team.
	if ( mp_radar_showall.GetInt() && 
		(
			( mp_radar_showall.GetInt() == 1 ) ||
			( mp_radar_showall.GetInt() == pRecipient->GetTeamNumber() )
		) )
		return FL_EDICT_ALWAYS;

	return FL_EDICT_PVSCHECK;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CTeam::Init( const char *pName, int iNumber )
{
	InitializeSpawnpoints();
	InitializePlayers();
	ResetTeamLeaders();

	m_bSurrendered = 0;
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0; 	
	m_scoreOvertime = 0;

	V_strncpy( m_szTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
	m_iTeamNum = iNumber;
}

//-----------------------------------------------------------------------------
// DATA HANDLING
//-----------------------------------------------------------------------------
int CTeam::GetTeamNumber( void ) const
{
	return m_iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetName( const char *pName )
{
	V_strncpy( m_szTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetName( void )
{
	return m_szTeamname;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetClanName( const char *pName )
{
	V_strncpy( m_szClanTeamname.GetForModify(), pName, MAX_TEAM_NAME_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetClanName( void )
{
	return m_szClanTeamname;
}

void CTeam::SetClanID( uint32 iClanID )
{
	m_iClanID = iClanID;
}

uint32 CTeam::GetClanID( void )
{
	return m_iClanID;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetFlagImageString( const char *pName )
{
	V_strncpy( m_szTeamFlagImage.GetForModify(), pName, MAX_TEAM_FLAG_ICON_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetFlagImageString( void )
{
	return m_szTeamFlagImage;
}

//-----------------------------------------------------------------------------
// Purpose: Set the team's name
//-----------------------------------------------------------------------------
void CTeam::SetLogoImageString( const char *pName )
{
	V_strncpy( m_szTeamLogoImage.GetForModify(), pName, MAX_TEAM_FLAG_ICON_LENGTH );
}

void CTeam::SetNumMapVictories( int numMapVictories )
{
	if ( numMapVictories != m_numMapVictories )
	{
		m_numMapVictories = numMapVictories;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the team's name
//-----------------------------------------------------------------------------
const char *CTeam::GetLogoImageString( void )
{
	return m_szTeamLogoImage;
}

//-----------------------------------------------------------------------------
// Purpose: Update the player's client data
//-----------------------------------------------------------------------------
void CTeam::UpdateClientData( CBasePlayer *pPlayer )
{
}

//------------------------------------------------------------------------------------------------------------------
// SPAWNPOINTS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::InitializeSpawnpoints( void )
{
	m_iLastSpawn = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::AddSpawnpoint( CTeamSpawnPoint *pSpawnpoint )
{
	m_aSpawnPoints.push_back( pSpawnpoint );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::RemoveSpawnpoint( CTeamSpawnPoint *pSpawnpoint )
{
	for (int i = 0; i < std::ssize( m_aSpawnPoints ); i++ )
	{
		if ( m_aSpawnPoints[i] == pSpawnpoint )
		{
			m_aSpawnPoints.erase( m_aSpawnPoints.begin() + i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the player at one of this team's spawnpoints. Return true if successful.
//-----------------------------------------------------------------------------
CBaseEntity *CTeam::SpawnPlayer( CBasePlayer *pPlayer )
{
	if ( m_aSpawnPoints.empty() )
		return nullptr;

	// Randomize the start spot
	int iSpawn = m_iLastSpawn + random_valve->RandomInt( 1,3 );
	if ( iSpawn >= std::ssize( m_aSpawnPoints ) )
		iSpawn -= std::ssize( m_aSpawnPoints );
	int iStartingSpawn = iSpawn;

	// Now loop through the spawnpoints and pick one
	int loopCount = 0;
	do 
	{
		if ( iSpawn >= std::ssize( m_aSpawnPoints ) )
		{
			++loopCount;
			iSpawn = 0;
		}

		// check if pSpot is valid, and that the player is on the right team
		if ( (loopCount > 3) || m_aSpawnPoints[iSpawn]->IsValid( pPlayer ) )
		{
			// DevMsg( 1, "player: spawning at (%s)\n", STRING(m_aSpawnPoints[iSpawn]->m_iName) );
			m_aSpawnPoints[iSpawn]->m_OnPlayerSpawn.FireOutput( pPlayer, m_aSpawnPoints[iSpawn] );

			m_iLastSpawn = iSpawn;
			return m_aSpawnPoints[iSpawn];
		}

		iSpawn++;
	} while ( iSpawn != iStartingSpawn ); // loop if we're not back to the start

	return nullptr;
}

//------------------------------------------------------------------------------------------------------------------
// PLAYERS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::InitializePlayers( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified player to this team. Remove them from their current team, if any.
//-----------------------------------------------------------------------------
void CTeam::AddPlayer( CBasePlayer *pPlayer )
{
	m_aPlayers.push_back( pPlayer );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Remove this player from the team
//-----------------------------------------------------------------------------
void CTeam::RemovePlayer( CBasePlayer *pPlayer )
{
	ks::erase_first( m_aPlayers, pPlayer );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of players in this team.
//-----------------------------------------------------------------------------
int CTeam::GetNumPlayers( void )
{
	return std::ssize( m_aPlayers );
}

//-----------------------------------------------------------------------------
// Purpose: Get a specific player
//-----------------------------------------------------------------------------
CBasePlayer *CTeam::GetPlayer( int iIndex )
{
	contract_assert( iIndex >= 0 && iIndex < std::ssize( m_aPlayers ) );
	return m_aPlayers[ iIndex ];
}

//------------------------------------------------------------------------------------------------------------------
// SCORING
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::ResetScores( void )
{
	m_bSurrendered = 0;
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0;	
	m_scoreOvertime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeam::AwardAchievement( int iAchievement )
{
	contract_assert( iAchievement >= 0 && iAchievement < 255 );	// must fit in short 

	CRecipientFilter filter;

	int iNumPlayers = GetNumPlayers();

	for ( int i=0;i<iNumPlayers;i++ )
	{
		if ( GetPlayer(i) )
		{
			filter.AddRecipient( GetPlayer(i) );
		}
	}

	ks::net::CCSUsrMsg_AchievementEvent msg;
	msg.achievement = iAchievement;
	SendUserMessage( filter, ks::net::CS_UM_AchievementEvent, msg );
}

int CTeam::GetAliveMembers( void )
{
	int iAlive = 0;

	int iNumPlayers = GetNumPlayers();

	for ( int i=0;i<iNumPlayers;i++ )
	{
		if ( GetPlayer(i) && GetPlayer(i)->IsAlive() )
		{
			iAlive++;
		}
	}

	return iAlive;
}

int CTeam::GetBotMembers( std::vector< CCSBot* > *pOutVecBots /*= NULL*/ )
{
	int iBots = 0;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		if ( GetPlayer( i ) && GetPlayer( i )->IsBot() )
		{
			if ( pOutVecBots )
			{
				CCSBot* pBot = dynamic_cast< CCSBot* > ( GetPlayer( i ) );
				if ( pBot )
				{
					pOutVecBots->push_back( pBot );
				}
			}
			iBots++;
		}
	}

	return iBots;
}

int CTeam::GetHumanMembers( std::vector< class CCSPlayer* > *pOutVecPlayers /*= NULL */ )
{
	int iPlayers = 0;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		if ( GetPlayer( i ) && !GetPlayer( i )->IsBot() )
		{
			if ( pOutVecPlayers )
			{
				CCSPlayer* pPlayer = dynamic_cast< CCSPlayer * > ( GetPlayer( i ) );
				if ( pPlayer )
				{
					pOutVecPlayers->push_back( pPlayer );
				}
			}
			iPlayers++;
		}
	}
	return iPlayers;
}

class CBaseEntity;
class CBasePlayer;
class CTeamSpawnPoint;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Team spawnpoint handling

LINK_ENTITY_TO_CLASS( info_player_teamspawn, CTeamSpawnPoint );

IMPLEMENT_REFLECT_DATAMAP( CTeamSpawnPoint )

//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_player_teamspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a player to spawn in?
//-----------------------------------------------------------------------------
bool CTeamSpawnPoint::IsValid( CBasePlayer *pPlayer )
{
	CBaseEntity *ent = nullptr;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() && plent != pPlayer )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputEnable( inputdata_t &inputdata )
{
	m_iDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = true;
}

//===========================================================================================================
// VEHICLE SPAWNPOINTS
//===========================================================================================================
LINK_ENTITY_TO_CLASS( info_vehicle_groundspawn, CTeamVehicleSpawnPoint );

IMPLEMENT_REFLECT_DATAMAP( CTeamVehicleSpawnPoint )

//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a vehicle to spawn in?
//-----------------------------------------------------------------------------
bool CTeamVehicleSpawnPoint::IsValid( void )
{
	CBaseEntity *ent = nullptr;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamVehicleSpawnPoint::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		// Don't add vehicle spawnpoints to the team for now
		//GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_vehicle_groundspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
class CTeam;

using namespace ::ks::reflect;

static char team_names[MAX_TEAMS][MAX_TEAMNAME_LENGTH];
static int team_scores[MAX_TEAMS];
static int num_teams = 0;

extern bool		g_fGameOver;
extern ConVar sv_allchat;
extern ConVar spec_replay_bot;

// [jason] Used to allow dead chat as well
extern ConVar sv_deadtalk;

REGISTER_GAMERULES_CLASS( CTeamplayRules );

CTeamplayRules::CTeamplayRules()
{
	m_DisableDeathMessages = false;
	m_DisableDeathPenalty = false;
	m_bSwitchTeams = false;
	m_bScrambleTeams = false;

	std::memset( team_names, 0, sizeof(team_names) );
	memset( team_scores, 0, sizeof(team_scores) );
	num_teams = 0;

	// Copy over the team from the server config
	m_szTeamList[0] = 0;

	RecountTeams();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamplayRules::Precache( void )
{
	BaseClass::Precache();

	// Call the Team Manager's precaches
	for ( int i = 0; i < GetNumberOfTeams(); i++ )
	{
		CTeam *pTeam = GetGlobalTeam( i );
		pTeam->Precache();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// WARNING - this function is NOT called in CS:GO 
//
//  CCSGameRules (which has CTeamplayRules as a baseclass) ::Think() calls
//    CGameRules::Think() directly, bypassing CTeamplayRules::Think() and 
//    CMultiplayRules::Think()
//
//  Code placed in here is likely to NEVER be called
//-----------------------------------------------------------------------------
void CTeamplayRules::Think ( void )
{
	BaseClass::Think();

	///// Check game rules /////

	if ( g_fGameOver )   // someone else quit the game already
	{
		BaseClass::Think();
		return;
	}

	float flTimeLimit = mp_timelimit.GetFloat() * 60;
	
	if ( flTimeLimit != 0 && gpGlobals->curtime >= flTimeLimit )
	{
		ChangeLevel();
		return;
	}

	float flFragLimit = fraglimit.GetFloat();
	if ( flFragLimit )
	{
		// check if any team is over the frag limit
		for ( int i = 0; i < num_teams; i++ )
		{
			if ( team_scores[i] >= flFragLimit )
			{
				ChangeLevel();
				return;
			}
		}
	}
}

//=========================================================
// ClientCommand
// the user has typed a command which is unrecognized by everything else;
// this check to see if the gamerules knows anything about the command
//=========================================================
bool CTeamplayRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	if( BaseClass::ClientCommand( pEdict, args ) )
		return true;
	
	if ( FStrEq( args[0], "menuselect" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		//int slot = atoi( args[1] );

		// select the item from the current menu

		return true;
	}

	return false;
}

const char *CTeamplayRules::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
{
	return "default";
}

//=========================================================
// InitHUD
//=========================================================
void CTeamplayRules::InitHUD( CBasePlayer *pPlayer )
{
	SetDefaultPlayerTeam( pPlayer );
	BaseClass::InitHUD( pPlayer );

	// this does a string compare and doesn't need to happen every frame
	//RecountTeams();

	/* TODO this has to be rewritten, maybe add a new USERINFO cvar "team"
	const char *team = engine->GetClientConVarValue( pPlayer->entindex(), "cl_team" );

	// update the current player of the team he is joining
	char text[1024];
	if ( !strcmp( mdls, pPlayer->TeamName() ) )
	{
		V_snprintf( text,sizeof(text), "You are on team \'%s\'\n", pPlayer->TeamName() );
	}
	else
	{
		V_snprintf( text,sizeof(text), "You were assigned to team %s\n", pPlayer->TeamName() );
	}

	ChangePlayerTeam( pPlayer, pPlayer->TeamName(), false, false );
	if ( V_strlen( pPlayer->TeamName() ) > 0 )
	{
		UTIL_SayText( text, pPlayer );
	}
	RecountTeams(); */
}

void CTeamplayRules::ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, bool bKill, bool bGib )
{
	int damageFlags = DMG_GENERIC;
	// int clientIndex = pPlayer->entindex();

	if ( !bGib )
	{
		damageFlags |= DMG_NEVERGIB;
	}
	else
	{
		damageFlags |= DMG_ALWAYSGIB;
	}

	// copy out the team name from the model
	// pPlayer->SetTeamName( pTeamName );
}

//-----------------------------------------------------------------------------
// Purpose: Player has just left the game
//-----------------------------------------------------------------------------
void CTeamplayRules::ClientDisconnected( edict_t *pClient )
{
	// Msg( "CLIENT DISCONNECTED, REMOVING FROM TEAM.\n" );

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
	if ( pPlayer )
	{
		pPlayer->SetConnected( PlayerDisconnecting );

		// Remove the player from his team
		if ( pPlayer->GetTeam() )
		{
			pPlayer->ChangeTeam( 0 );
		}
	}

	BaseClass::ClientDisconnected( pClient );
}

//=========================================================
// ClientUserInfoChanged
//=========================================================
void CTeamplayRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	/* TODO: handle skin, model & team changes 

  	char text[1024];

	// skin/color/model changes
	int iTeam = V_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_team" ) );
	int iClass = V_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_class" ) );

	if ( defaultteam.GetBool() )
	{
		// int clientIndex = pPlayer->entindex();

		// engine->SetClientKeyValue( clientIndex, "model", pPlayer->TeamName() );
		// engine->SetClientKeyValue( clientIndex, "team", pPlayer->TeamName() );
		UTIL_SayText( "Not allowed to change teams in this game!\n", pPlayer );
		return;
	}

	if ( defaultteam.GetFloat() || !IsValidTeam( mdls ) )
	{
		// int clientIndex = pPlayer->entindex();

		// engine->SetClientKeyValue( clientIndex, "model", pPlayer->TeamName() );
		V_snprintf( text,sizeof(text), "Can't change team to \'%s\'\n", mdls );
		UTIL_SayText( text, pPlayer );
		V_snprintf( text,sizeof(text), "Server limits teams to \'%s\'\n", m_szTeamList );
		UTIL_SayText( text, pPlayer );
		return;
	}

	ChangePlayerTeam( pPlayer, mdls, true, true );
	// recound stuff
	RecountTeams(); */

	const char *pszName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );

	const char *pszOldName = pPlayer->GetPlayerName();

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	// Note, this is case sensitive
	if ( ( V_strcmp( pszOldName, pszName ) != 0 ) &&
		CanClientCustomizeOwnIdentity() )
	{
		if ( pszOldName[0] != '\0' )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "player_changename" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetString( "oldname", pszOldName );
				event->SetString( "newname", pszName );
				gameeventmanager->FireEvent( event );
			}
		}

		pPlayer->SetPlayerName( pszName );
	}
}

//=========================================================
// Deathnotice. 
//=========================================================
void CTeamplayRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( m_DisableDeathMessages )
		return;

	CBaseEntity *pKiller = info.GetAttacker();
	if ( pVictim && pKiller && pKiller->IsPlayer() )
	{
		CBasePlayer *pk = (CBasePlayer*)pKiller;

		if ( pk )
		{
			if ( (pk != pVictim) && (PlayerRelationship( pVictim, pk ) == GR_TEAMMATE) )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );
				if ( event )
				{
					event->SetInt("killer", pk->GetUserID() );
					event->SetInt("victim", pVictim->GetUserID() );
					event->SetInt("priority", 7 );	// HLTV event priority, not transmitted
					if( !OnReplayPrompt( pVictim, pk ) )
						event->SetBool( "noreplay", true );

					gameeventmanager->FireEvent( event );
				}
				return;
			}
		}
	}

	BaseClass::DeathNotice( pVictim, info );
}

//=========================================================
//=========================================================
void CTeamplayRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( !m_DisableDeathPenalty )
	{
		BaseClass::PlayerKilled( pVictim, info );
		RecountTeams();
	}
}

//=========================================================
// IsTeamplay
//=========================================================
bool CTeamplayRules::IsTeamplay( void )
{
	return true;
}

bool CTeamplayRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
	{
		// my teammate hit me.
		if ( ( mp_friendlyfire.GetInt() == 0 ) && ( pAttacker != pPlayer ) )
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return false;
		}
	}

	return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

//=========================================================
//=========================================================
int CTeamplayRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	// don't do string compares, just compare the team number
// 	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
// 	{
// 		return GR_TEAMMATE;
// 	}

	if ( pPlayer->GetTeamNumber() != TEAM_INVALID && pTarget->GetTeamNumber() != TEAM_INVALID && pPlayer->GetTeamNumber() == pTarget->GetTeamNumber() )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pListener - 
//			*pSpeaker - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTeamplayRules::PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly )
{
	if ( !bTeamOnly && sv_allchat.GetBool() )
	{
		if ( !pSpeaker->IsAlive() )
		{
			// [jason] convar allows the dead to speak/chat with the living
			return ( ( !pListener->IsAlive() || sv_deadtalk.GetBool() ) &&
					 ( PlayerRelationship( pListener, pSpeaker ) == GR_TEAMMATE ) );
		}
	}

	return ( PlayerRelationship( pListener, pSpeaker ) == GR_TEAMMATE );
}

//=========================================================
//=========================================================
bool CTeamplayRules::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	// always autoaim, unless target is a teammate
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt && pTgt->IsPlayer() )
	{
		if ( PlayerRelationship( pPlayer, pTgt ) == GR_TEAMMATE )
			return false; // don't autoaim at teammates
	}

	return BaseClass::ShouldAutoAim( pPlayer, target );
}

//=========================================================
//=========================================================
int CTeamplayRules::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if ( !pKilled )
		return 0;

	if ( !pAttacker )
		return 1;

	if ( pAttacker != pKilled && PlayerRelationship( pAttacker, pKilled ) == GR_TEAMMATE )
		return -1;

	return 1;
}

//=========================================================
//=========================================================
// const char *CTeamplayRules::GetTeamID( CBaseEntity *pEntity )
// {
// 	if ( pEntity == NULL || pEntity->edict() == NULL )
// 		return "";
// 
// 	// return their team name
// 	return pEntity->TeamID();
// }

int CTeamplayRules::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		// try to find existing team
		for ( int tm = 0; tm < num_teams; tm++ )
		{
			if ( !stricmp( team_names[tm], pTeamName ) )
				return tm;
		}
	}
	
	return -1;	// No match
}

const char *CTeamplayRules::GetIndexedTeamName( int teamIndex )
{
	if ( teamIndex < 0 || teamIndex >= num_teams )
		return "";

	return team_names[ teamIndex ];
}

bool CTeamplayRules::IsValidTeam( const char *pTeamName ) 
{
	if ( !m_teamLimit )	// Any team is valid if the teamlist isn't set
		return true;

	return ( GetTeamIndex( pTeamName ) != -1 ) ? true : false;
}

const char *CTeamplayRules::TeamWithFewestPlayers( void )
{
	int i;
	int minPlayers = MAX_TEAMS;
	int teamCount[ MAX_TEAMS ];
	char *pTeamName = nullptr;

	std::memset( teamCount, 0, MAX_TEAMS * sizeof(int) );
	
	// loop through all clients, count number of players on each team
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );

		if ( plr )
		{
			int team = GetTeamIndex( plr->TeamID() );
			if ( team >= 0 )
				teamCount[team] ++;
		}
	}

	// Find team with least players
	for ( i = 0; i < num_teams; i++ )
	{
		if ( teamCount[i] < minPlayers )
		{
			minPlayers = teamCount[i];
			pTeamName = team_names[i];
		}
	}

	return pTeamName;
}

//=========================================================
//=========================================================
void CTeamplayRules::RecountTeams( void )
{
	char	*pName;
	char	teamlist[TEAMPLAY_TEAMLISTLENGTH];

	// loop through all teams, recounting everything
	num_teams = 0;

	// Copy all of the teams from the teamlist
	// make a copy because strtok is destructive
	V_strncpy( teamlist, m_szTeamList, sizeof(teamlist) );
	pName = teamlist;
	pName = std::strtok( pName, ";" );
	while ( pName != nullptr && *pName )
	{
		if ( GetTeamIndex( pName ) < 0 )
		{
			V_strncpy( team_names[num_teams], pName, sizeof(team_names[num_teams]));
			num_teams++;
		}
		pName = strtok( nullptr, ";" );
	}

	if ( num_teams < 2 )
	{
		num_teams = 0;
		m_teamLimit = false;
	}

	// Sanity check
	std::memset( team_scores, 0, sizeof(team_scores) );

	// loop through all clients
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = UTIL_PlayerByIndex( i );

		if ( plr )
		{
			const char *pTeamName = plr->TeamID();
			// try add to existing team
			int tm = GetTeamIndex( pTeamName );
			
			if ( tm < 0 ) // no team match found
			{ 
				if ( !m_teamLimit )
				{
					// add to new team
					tm = num_teams;
					num_teams++;
					team_scores[tm] = 0;
					V_strncpy( team_names[tm], pTeamName, MAX_TEAMNAME_LENGTH );
				}
			}

			if ( tm >= 0 )
			{
				team_scores[tm] += plr->FragCount();
			}
		}
	}
}

using namespace ::ks::reflect;

// Purpose: round based game rule convars shared by the multiplayer rules.

ConVar mp_capstyle( side_registry(), "mp_capstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture points used. 0 = Fixed players required to cap. 1 = More players cap faster, but longer cap times." );
ConVar mp_blockstyle( side_registry(), "mp_blockstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture point blocking used. 0 = Blocks break captures completely. 1 = Blocks only pause captures." );
ConVar mp_respawnwavetime( side_registry(), "mp_respawnwavetime", "10.0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Time between respawn waves." );
ConVar mp_capdeteriorate_time( side_registry(), "mp_capdeteriorate_time", "90.0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Time it takes for a full capture point to deteriorate." );
ConVar mp_tournament ( side_registry(), "mp_tournament", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );

ConVar mp_teams_unbalance_limit( side_registry(), "mp_teams_unbalance_limit", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
					 "Teams are unbalanced when one team has this many more players than the other team. (0 disables check)",
					 true, 0,	// min value
					 true, 30	// max value
					 );

ConVar mp_maxrounds( side_registry(), "mp_maxrounds", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "max number of rounds to play before server changes maps", true, 0, false, 0 );
ConVar mp_winlimit( side_registry(), "mp_winlimit", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Max score one team can reach before server changes maps", true, 0, false, 0 );
ConVar mp_disable_respawn_times( side_registry(), "mp_disable_respawn_times", "0", FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar mp_bonusroundtime( side_registry(), "mp_bonusroundtime", "15", FCVAR_REPLICATED, "Time after round win until round restarts", true, 5, true, 15 );

ConVar mp_showroundtransitions( side_registry(), "mp_showroundtransitions", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show gamestate round transitions." );
ConVar mp_enableroundwaittime( side_registry(), "mp_enableroundwaittime", "1", FCVAR_REPLICATED, "Enable timers to wait between rounds." );
ConVar mp_showcleanedupents( side_registry(), "mp_showcleanedupents", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show entities that are removed on round respawn." );
ConVar mp_restartround( side_registry(), "mp_restartround", "0", FCVAR_GAMEDLL, "If non-zero, the current round will restart in the specified number of seconds" );	

ConVar mp_stalemate_timelimit( side_registry(), "mp_stalemate_timelimit", "240", FCVAR_REPLICATED, "Timelimit (in seconds) of the stalemate round." );
ConVar mp_autoteambalance( side_registry(), "mp_autoteambalance", "1", FCVAR_NOTIFY | FCVAR_RELEASE );

ConVar mp_stalemate_enable( side_registry(), "mp_stalemate_enable", "0", FCVAR_NOTIFY, "Enable/Disable stalemate mode." );
ConVar mp_stalemate_at_timelimit( side_registry(), "mp_stalemate_at_timelimit", "0", FCVAR_NOTIFY, "Allow the match to end when mp_timelimit hits instead of waiting for the end of the current round." );

// Utility function
bool FindInList( const char **pStrings, const char *pToFind )
{
	int i = 0;
	while ( pStrings[i][0] != 0 )
	{
		if ( V_stricmp( pStrings[i], pToFind ) == 0 )
			return true;
		i++;
	}

	return false;
}

class CBaseEntity;
class CPointTemplate;

using namespace ::ks::reflect;

// Purpose: Template entities are used by spawners to create copies of entities
//			that were configured by the level designer. This allows us to spawn
//			entities with arbitrary sets of key/value data and entity I/O
//			connections.
//			Template entities are marked with a special spawnflag which causes
//			them not to spawn, but to be saved as a string containing all the
//			map data (keyvalues and I/O connections) from the BSP. Template
//			entities are looked up by name by the spawner, which copies the
//			map data into a local string (that's how the template data is saved
//			and restored). Once all the entities in the map have been activated,
//			the template database is freed.

ConVar template_debug( side_registry(), "template_debug", "0" );

// This is appended to key's values that will need to be unique in template instances
const char *ENTITYIO_FIXUP_STRING = "&0000";


struct TemplateEntityData_t
{
	const char	*pszName;
	char		*pszMapData;
	string_t	iszMapData;
	int			iMapDataLength;
	bool		bNeedsEntityIOFixup;	// If true, this template has entity I/O in its mapdata that needs fixup before spawning.
	char		*pszFixedMapData;		// A single copy of this template that we used to fix up the Entity I/O whenever someone wants a fixed version of this template

	int			m_nHammerID;			// Used to update the template in Foundry

	DECLARE_SIMPLE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( TemplateEntityData_t )

struct grouptemplate_t
{
	CEntityMapData *pMapDataParser;
	char			pszName[MAPKEY_MAXLENGTH];
	int				iIndex;
	bool			bChangeTargetname;
};

static std::vector< TemplateEntityData_t * > g_Templates;

int	g_iCurrentTemplateInstance;

void Templates_FreeTemplate( TemplateEntityData_t *pTemplate )
{
	g_pMemAlloc->Free((void *)pTemplate->pszName);
	g_pMemAlloc->Free(pTemplate->pszMapData);
	if ( pTemplate->pszFixedMapData )
	{
		g_pMemAlloc->Free(pTemplate->pszFixedMapData);
	}

	g_pMemAlloc->Free(pTemplate);
}

//-----------------------------------------------------------------------------
// Purpose: Saves the given entity's keyvalue data for later use by a spawner.
//			Returns the index into the templates.
//-----------------------------------------------------------------------------
int Templates_Add(CBaseEntity *pEntity, const char *pszMapData, int nLen, int nHammerID)
{
	const char *pszName = STRING(pEntity->GetEntityName());
	if ((!pszName) || (!std::strlen(pszName)))
	{
		DevWarning(1, "RegisterTemplateEntity: template entity with no name, class %s\n", pEntity->GetClassname());
		return -1;
	}

	TemplateEntityData_t *pEntData = (TemplateEntityData_t *)MemAlloc_Alloc(sizeof(TemplateEntityData_t));
	pEntData->m_nHammerID = nHammerID;
	pEntData->pszName = MemAlloc_StrDup( pszName );

	// We may modify the values of the keys in this mapdata chunk later on to fix Entity I/O
	// connections. For this reason, we need to ensure we have enough memory to do that.
	int iKeys = MapEntity_GetNumKeysInEntity( pszMapData );
	int iExtraSpace = (std::strlen(ENTITYIO_FIXUP_STRING)+1) * iKeys;

	// Extra 1 because the mapdata passed in isn't null terminated
	pEntData->iMapDataLength = nLen + iExtraSpace + 1;
	pEntData->pszMapData = (char *)MemAlloc_Alloc( pEntData->iMapDataLength );
	std::memcpy(pEntData->pszMapData, pszMapData, nLen + 1);
	pEntData->pszMapData[nLen] = '\0';

	// We don't alloc these suckers right now because that gives us no time to
	// tweak them for Entity I/O purposes.
	pEntData->iszMapData = NULL_STRING;
	pEntData->bNeedsEntityIOFixup = false;
	pEntData->pszFixedMapData = nullptr;

	g_Templates.push_back( pEntData ); return (int)std::ssize( g_Templates ) - 1;
}

void Templates_RemoveByHammerID( int nHammerID )
{
	for ( int i=std::ssize( g_Templates )-1; i >= 0; i-- )
	{
		if ( g_Templates[i]->m_nHammerID == nHammerID )
		{
			Templates_FreeTemplate( g_Templates[i] );
			g_Templates.erase( g_Templates.begin() + i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified index needs to be fixed up to be unique
//			when the template is spawned.
//-----------------------------------------------------------------------------
bool Templates_IndexRequiresEntityIOFixup( int iIndex )
{
	contract_assert( iIndex < std::ssize( g_Templates ) );
	return g_Templates[iIndex]->bNeedsEntityIOFixup;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up a template entity by its index in the templates
//			Used by point_templates because they often have multiple templates with the same name
//-----------------------------------------------------------------------------
string_t Templates_FindByIndex( int iIndex )
{
	contract_assert( iIndex < std::ssize( g_Templates ) );

	// First time through we alloc the mapdata onto the pool.
	// It's safe to do it now because this isn't called until post Entity I/O cleanup.
	if ( g_Templates[iIndex]->iszMapData == NULL_STRING )
	{
		g_Templates[iIndex]->iszMapData = AllocPooledString( g_Templates[iIndex]->pszMapData );
	}

	return g_Templates[iIndex]->iszMapData;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int Templates_GetStringSize( int iIndex )
{
	contract_assert( iIndex < std::ssize( g_Templates ) );
	return g_Templates[iIndex]->iMapDataLength;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up a template entity by name, returning the map data blob as
//			a null-terminated string containing key/value pairs.
//			NOTE: This can't handle multiple templates with the same targetname.
//-----------------------------------------------------------------------------
string_t Templates_FindByTargetName(const char *pszName)
{
	int nCount = std::ssize( g_Templates );
	for (int i = 0; i < nCount; i++)
	{
		TemplateEntityData_t *pTemplate = g_Templates[ i ];
		if ( !stricmp(pTemplate->pszName, pszName) )
			return Templates_FindByIndex( i );
	}

	return NULL_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: A CPointTemplate has asked us to reconnect all the entity I/O links
//			inside it's templates. Go through the keys and add look for values
//			that match a name within the group's entity	names. Append %d to any
//			found values, which will later be filled out by a unique identifier
//			whenever the template is instanced.
//-----------------------------------------------------------------------------
void Templates_ReconnectIOForGroup( CPointTemplate *pGroup )
{
	int iCount = pGroup->GetNumTemplates();
	if ( !iCount )
		return;

	// First assemble a list of the targetnames of all the templates in the group.
	// We need to store off the original names here, because we're going to change
	// them as we go along.
	std::vector< grouptemplate_t > GroupTemplates;
	int i;
	for ( i = 0; i < iCount; i++ )
	{
		grouptemplate_t newGroupTemplate;
		newGroupTemplate.iIndex = pGroup->GetTemplateIndexForTemplate(i);
		newGroupTemplate.pMapDataParser = new CEntityMapData( g_Templates[ newGroupTemplate.iIndex ]->pszMapData, g_Templates[ newGroupTemplate.iIndex ]->iMapDataLength );
		contract_assert( newGroupTemplate.pMapDataParser );
		newGroupTemplate.pMapDataParser->ExtractValue( "targetname", newGroupTemplate.pszName );
		newGroupTemplate.bChangeTargetname = false;

		GroupTemplates.push_back( newGroupTemplate );
	}

	if (pGroup->AllowNameFixup())
	{
		char keyName[MAPKEY_MAXLENGTH];
		char value[MAPKEY_MAXLENGTH];
		char valueclipped[MAPKEY_MAXLENGTH];
		
		// Now go through all the entities in the group and parse their mapdata keyvalues.
		// We're looking for any values that match targetnames of any of the group entities.
		for ( i = 0; i < iCount; i++ )
		{
			// We need to know what instance of each key we're changing.
			// Store a table of the count of the keys we've run into.
			ks::idict< int > KeyInstanceCount;
			CEntityMapData *mapData = GroupTemplates[i].pMapDataParser;

			// Loop through our keys
			if ( !mapData->GetFirstKey(keyName, value) )
				continue;

			do 
			{
				// Ignore targetnames
				if ( !stricmp( keyName, "targetname" ) )
					continue;

				// Add to the count for this 
				int &nKeyCount = KeyInstanceCount[ keyName ];
				nKeyCount++;

				// Entity I/O values are stored as "Targetname,<data>", so we need to see if there's a ',' in the string
				char *sValue = value;

				// FIXME: This is very brittle. Any key with a , will not be found.
				char delimiter = VMF_IOPARAM_STRING_DELIMITER;
				if( std::strchr( value, delimiter ) == nullptr )
				{
					delimiter = ',';
				}

				char *s = std::strchr( value, delimiter );
				if ( s )
				{
					// Grab just the targetname of the receiver
					V_strncpy( valueclipped, value, (s - value+1) );
					sValue = valueclipped;
				}

				// Loop through our group templates
				for ( int iTName = 0; iTName < iCount; iTName++ )
				{
					char *pName = GroupTemplates[iTName].pszName;
					if ( stricmp( pName, sValue ) )
						continue;

					if ( template_debug.GetInt() )
					{
						Msg("Template Connection Found: Key %s (\"%s\") in entity named \"%s\"(%d) matches entity %d's targetname\n", keyName, sValue, GroupTemplates[i].pszName, i, iTName );
					}

					char newvalue[MAPKEY_MAXLENGTH];

					// Get the current key instance. (-1 because it's this one we're changing)
					int nKeyInstance = nKeyCount - 1;

					// Add our IO value to the targetname
					// We need to append it if this isn't an Entity I/O value, or prepend it to the ',' if it is
					if ( s )
					{
						V_strncpy( newvalue, valueclipped, MAPKEY_MAXLENGTH );
						V_strncat( newvalue, ENTITYIO_FIXUP_STRING, sizeof(newvalue), COPY_ALL_CHARACTERS );
						V_strncat( newvalue, s, sizeof(newvalue), COPY_ALL_CHARACTERS );
						mapData->SetValue( keyName, newvalue, nKeyInstance );
					}
					else
					{
						V_strncpy( newvalue, sValue, MAPKEY_MAXLENGTH );
						V_strncat( newvalue, ENTITYIO_FIXUP_STRING, sizeof(newvalue), COPY_ALL_CHARACTERS );
						mapData->SetValue( keyName, newvalue, nKeyInstance );
					}
					
					// Remember we changed this targetname
					GroupTemplates[iTName].bChangeTargetname = true;

					// Set both entity's flags telling them their template needs fixup when it's spawned
					g_Templates[ GroupTemplates[i].iIndex ]->bNeedsEntityIOFixup = true;
					g_Templates[ GroupTemplates[iTName].iIndex ]->bNeedsEntityIOFixup = true;
				}
			} 
			while ( mapData->GetNextKey(keyName, value) );
		}

		// Now change targetnames for all entities that need them changed
		for ( i = 0; i < iCount; i++ )
		{
			char value[MAPKEY_MAXLENGTH];

			if ( GroupTemplates[i].bChangeTargetname )
			{
				CEntityMapData *mapData = GroupTemplates[i].pMapDataParser;
				mapData->ExtractValue( "targetname", value );
				V_strncat( value, ENTITYIO_FIXUP_STRING, sizeof(value), COPY_ALL_CHARACTERS );
				mapData->SetValue( "targetname", value );
			}
		}
	}

	// Delete our group parsers
	for ( i = 0; i < iCount; i++ )
	{
		delete GroupTemplates[i].pMapDataParser;
	}
	GroupTemplates.clear(); GroupTemplates.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: Someone's about to start instancing a new group of entities.
//			Generate a unique identifier for this group.
//-----------------------------------------------------------------------------
void Templates_StartUniqueInstance( void )
{
	g_iCurrentTemplateInstance++;

	// Make sure there's enough room to fit it into the string
	int iMax = std::pow(10.0f, (int)((std::strlen(ENTITYIO_FIXUP_STRING) - 1)));	// -1 for the &
	if ( g_iCurrentTemplateInstance >= iMax )
	{
		// We won't hit this.
		contract_assert( 0 );
		// Hopefully there were still be instance number 0 around.
		g_iCurrentTemplateInstance = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Someone wants to spawn an instance of a template that requires
//			entity IO fixup. Fill out the pMapData with a copy of the template
//			with unique key/values where the template requires them.
//-----------------------------------------------------------------------------
char *Templates_GetEntityIOFixedMapData( int iIndex )
{
	contract_assert( Templates_IndexRequiresEntityIOFixup( iIndex ) );

	// First time through?
	if ( !g_Templates[iIndex]->pszFixedMapData )
	{
		g_Templates[iIndex]->pszFixedMapData = (char *)MemAlloc_Alloc( g_Templates[iIndex]->iMapDataLength );
		V_strncpy( g_Templates[iIndex]->pszFixedMapData, g_Templates[iIndex]->pszMapData, g_Templates[iIndex]->iMapDataLength );
	}

	int iFixupSize = std::strlen(ENTITYIO_FIXUP_STRING); // strlen("&0000\0") = 5!
	char *sOurFixup = new char[iFixupSize+1]; // do alloc room here for the null terminator
	V_snprintf( sOurFixup, iFixupSize+1, "%c%.4d", ENTITYIO_FIXUP_STRING[0], g_iCurrentTemplateInstance );

	// Now rip through the map data string and replace any instances of the fixup string with our unique identifier
	char *c = g_Templates[iIndex]->pszFixedMapData;
	do
	{
		if ( *c == ENTITYIO_FIXUP_STRING[0] )
		{
			// Make sure it's our fixup string
			bool bValid = true;
			for ( int i = 1; i < iFixupSize; i++ )
			{
				// Look for any number, because we've already used this string
				if ( !(*(c+i) >= '0' && *(c+i) <= '9') )
				{
					// Some other string
					bValid = false;
					break;
				}
			}

			// Stomp it with our unique string
			if ( bValid )
			{
				std::memcpy( c, sOurFixup, iFixupSize );
				c += iFixupSize;
			}
		}
		c++;
	} while (*c);

	delete [] sOurFixup;
	return g_Templates[iIndex]->pszFixedMapData;
}

//-----------------------------------------------------------------------------
// Purpose: Frees all the template data. Called on level shutdown.
//-----------------------------------------------------------------------------
void Templates_RemoveAll(void)
{
	int nCount = std::ssize( g_Templates );
	for (int i = 0; i < nCount; i++)
	{
		TemplateEntityData_t *pTemplate = g_Templates[ i ];
		Templates_FreeTemplate( pTemplate );
	}

	g_Templates.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Hooks in the template manager's callbacks.
//-----------------------------------------------------------------------------
class CTemplatesHook : public CAutoGameSystem
{
public:
	CTemplatesHook( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual void LevelShutdownPostEntity( void )
	{
		Templates_RemoveAll();
	}
};

CTemplatesHook g_TemplateEntityHook( "CTemplatesHook" );

//-----------------------------------------------------------------------------
// TEMPLATE SAVE / RESTORE
//-----------------------------------------------------------------------------
static short TEMPLATE_SAVE_RESTORE_VERSION = 1;

//-----------------------------------------------------------------------------

//-------------------------------------

//=========================================================
// NPC template
//=========================================================

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

LINK_ENTITY_TO_CLASS( point_tesla, CTesla );

IMPLEMENT_REFLECT_DATAMAP( CTesla )

IMPLEMENT_REFLECT_SERVERCLASS( CTesla, DT_Tesla )

CTesla::CTesla()
{
	m_SourceEntityName = NULL_STRING;
	m_SoundName = NULL_STRING;
	m_iszSpriteName = NULL_STRING;
	m_NumBeams[0] = m_NumBeams[1] = 6;
	m_flRadius = 200;
	m_flThickness[0] = m_flThickness[1] = 5;
	m_flTimeVisible[0] = 0.3;
	m_flTimeVisible[1] = 0.55;
	m_flArcInterval[0] = m_flArcInterval[1] = 0.5;
	
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

void CTesla::Spawn_Impl()
{
	if ( m_iszSpriteName.Get() == NULL_STRING )
	{
		m_iszSpriteName = AllocPooledString("sprites/physbeam.vmt");
	}

	Precache();
	BaseClass::Spawn_Impl();
}

void CTesla::Activate_Impl()
{
	BaseClass::Activate_Impl();

	SetThink( &CTesla::ShootArcThink );
	SetupForNextArc();
}

void CTesla::Precache_Impl()
{
	PrecacheModel( STRING(m_iszSpriteName.Get()) );
	BaseClass::Precache_Impl();

	PrecacheScriptSound( STRING( m_SoundName.Get() ) );
}

void CTesla::SetupForNextArc()
{
	if (m_bOn)
	{
		float flTimeToNext = RandomFloat( m_flArcInterval[0], m_flArcInterval[1] );
		SetNextThink( gpGlobals->curtime + flTimeToNext );
	}
	else
	{
		SetNextThink( TICK_NEVER_THINK );
	}
}

CBaseEntity* CTesla::GetSourceEntity()
{
	if ( m_SourceEntityName != NULL_STRING )
	{
		CBaseEntity *pRet = gEntList.FindEntityByName( nullptr, m_SourceEntityName );
		if ( pRet )
			return pRet;
	}

	return this;
}		

void CTesla::ShootArcThink()
{
	DoSpark();
	SetupForNextArc();
}

void CTesla::DoSpark()
{
	// Shoot out an arc.
	EntityMessageBegin( this );

		CBaseEntity *pEnt = GetSourceEntity();
		
		WRITE_VEC3COORD( pEnt->GetAbsOrigin() );
		WRITE_SHORT( pEnt->entindex() );
		WRITE_FLOAT( m_flRadius );
		WRITE_BYTE( m_Color.r );
		WRITE_BYTE( m_Color.g );
		WRITE_BYTE( m_Color.b );
		WRITE_BYTE( m_Color.a );
		WRITE_CHAR( RandomInt( m_NumBeams[0], m_NumBeams[1] ) );
		WRITE_FLOAT( RandomFloat( m_flThickness[0], m_flThickness[1] ) );
		WRITE_FLOAT( RandomFloat( m_flTimeVisible[0], m_flTimeVisible[1] ) );

	MessageEnd();
}

void CTesla::InputDoSpark( inputdata_t &inputdata )
{
	DoSpark();
}

void CTesla::InputTurnOn( inputdata_t &inputdata )
{
	m_bOn = true;
	SetupForNextArc();
}

void CTesla::InputTurnOff( inputdata_t &inputdata )
{
	m_bOn = false;
	SetupForNextArc();
}

class CBaseEntity;

using namespace ::ks::reflect;

// In this test, the server makes an entity (call it A) that has an EHANDLE to another 
// entity (call it B). Intitially, A is sent to the client but B is not. Later, 
// the server decides to send B to the client too. At that point, without resending A's EHANDLE,
// the client's EHANDLE should access B.

	// ------------------------------------------------------------------------------------ //
	// The main entity class (class A).
	// ------------------------------------------------------------------------------------ //
	class CHandleTest : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_HandleTest" } ]]
	static constexpr bool ks_class = true;

	public:
		DECLARE_CLASS( CHandleTest, CBaseEntity );
		DECLARE_SERVERCLASS();

		CHandleTest()
		{
			m_bSendHandle = false;
		}
		
		 int UpdateTransmitState_Impl()
		{
			// ALWAYS transmit to all clients.
			return SetTransmitState( FL_EDICT_ALWAYS );
		}

		 void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
		{
			if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
				return;

			BaseClass::SetTransmit_Impl( pInfo, bAlways );
			
			// Force the thing we're pointing at to be sent too?
			if ( m_bSendHandle )
				m_Handle->SetTransmit( pInfo, bAlways );
		}

		CNetworkHandle( CBaseEntity, m_Handle, [[= ks::reflect::Net{} ]] );
		CNetworkVar( bool, m_bSendHandle, [[= ks::reflect::Net{ .bits = -1 } ]] );
	};

IMPLEMENT_REFLECT_SERVERCLASS( CHandleTest, DT_HandleTest )
	
	LINK_ENTITY_TO_CLASS( handle_test, CHandleTest );

	// ------------------------------------------------------------------------------------ //
	// The class pointed to by the handle.
	// ------------------------------------------------------------------------------------ //
	class CHandleDummy : public CBaseEntity
	{
		DECLARE_CLASS( CHandleDummy, CBaseEntity );
	public:
	};
	LINK_ENTITY_TO_CLASS( handle_dummy, CHandleDummy );

	CHandle<CHandleTest> g_HandleTest;

	// The test runs this command.
	void CC_Test_EHandle()
	{
		if ( g_HandleTest.Get() )
		{
			g_HandleTest->m_bSendHandle = !g_HandleTest->m_bSendHandle;
		}
		else
		{
			CHandleTest *pHolder = CREATE_ENTITY( CHandleTest, "handle_test" );
			pHolder->m_Handle = CREATE_ENTITY( CHandleDummy, "handle_dummy" );
			pHolder->Spawn();
			g_HandleTest = pHolder;
			Msg( "Created EHANDLE test entity. Run this command again to transmit the second ent.\n" );
		}
	}
	ConCommand Test_EHandle( side_registry(), "Test_EHandle", CC_Test_EHandle, 0, FCVAR_CHEAT );

class CBaseEntity;

using namespace ::ks::reflect;

class CTest_ProxyToggle_Networkable;
static CTest_ProxyToggle_Networkable *g_pTestObj = 0;
static bool g_bEnableProxy = true;

// ---------------------------------------------------------------------------------------- //
// CTest_ProxyToggle_Networkable
// ---------------------------------------------------------------------------------------- //

namespace DT_ProxyToggle_ProxiedData { extern SendTable g_SendTable; }
void *SendProxy_TestProxyToggle( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients,
    int objectID );

class CTest_ProxyToggle_Networkable : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ProxyToggle_ProxiedData", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_ProxyToggle" } ]]
	[[= ks::reflect::SubTable<"blah", &DT_ProxyToggle_ProxiedData::g_SendTable, SendProxy_TestProxyToggle, true>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTest_ProxyToggle_Networkable, CBaseEntity );
	DECLARE_SERVERCLASS();

			CTest_ProxyToggle_Networkable()
			{
				m_WithProxy = 1241;
				g_pTestObj = this;
			}

			~CTest_ProxyToggle_Networkable()
			{
				g_pTestObj = nullptr;
			}

	int UpdateTransmitState_Impl()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CNetworkVar( int, m_WithProxy, [[= ks::reflect::Net{ .bits = -1, .table = "DT_ProxyToggle_ProxiedData" } ]] );
};

void* SendProxy_TestProxyToggle( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
{
	if ( g_bEnableProxy )
	{
		return (void*)pData;
	}
	else
	{
		pRecipients->ClearAllRecipients();
		return nullptr;
	}
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_TestProxyToggle );

// ---------------------------------------------------------------------------------------- //
// Datatables.
// ---------------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( test_proxytoggle, CTest_ProxyToggle_Networkable );

IMPLEMENT_REFLECT_TABLE_IN( CTest_ProxyToggle_Networkable, DT_ProxyToggle_ProxiedData );

IMPLEMENT_REFLECT_SERVERCLASS( CTest_ProxyToggle_Networkable, DT_ProxyToggle )

// ---------------------------------------------------------------------------------------- //
// Console commands for this test.
// ---------------------------------------------------------------------------------------- //

void Test_ProxyToggle_EnableProxy( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_ProxyToggle_EnableProxy: requires parameter (0 or 1)." );
	}

	g_bEnableProxy = !!std::atoi( args[ 1 ] );
}

void Test_ProxyToggle_SetValue( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_ProxyToggle_SetValue: requires value parameter." );
	}
	else if ( !g_pTestObj )
	{
		Error( "Test_ProxyToggle_SetValue: no entity present." );
	}

	g_pTestObj->m_WithProxy = std::atoi( args[ 1 ] );
}

ConCommand cc_Test_ProxyToggle_EnableProxy( side_registry(), "Test_ProxyToggle_EnableProxy", Test_ProxyToggle_EnableProxy, 0, FCVAR_CHEAT );
ConCommand cc_Test_ProxyToggle_SetValue( side_registry(), "Test_ProxyToggle_SetValue", Test_ProxyToggle_SetValue, 0, FCVAR_CHEAT );

class CBaseEntity;
class CBasePlayer;
class CWorld;

using namespace ::ks::reflect;

CStressEntityReg	*CStressEntityReg::s_pHead = nullptr;

// CStressEntityReg::s_pHead in array form for convenient access.
std::vector< CStressEntityReg* > g_StressEntityRegs;

std::vector< EHANDLE > g_StressEntities;

CBaseEntity* MoveToRandomSpot( CBaseEntity *pEnt )
{
	if ( pEnt )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
		if ( pLocalPlayer )
		{			
			Vector vForward;
			pLocalPlayer->EyeVectors(&vForward );

			UTIL_SetOrigin( pEnt, GetRandomSpot() );
		}
	}

	return pEnt;
}

Vector GetRandomSpot()
{
	CWorld *pEnt = GetWorldEntity();
	if ( pEnt )
	{
		Vector vMin, vMax;
		pEnt->GetWorldBounds( vMin, vMax );
		return Vector(
			RandomFloat( vMin.x, vMax.x ),
			RandomFloat( vMin.y, vMax.y ),
			RandomFloat( vMin.z, vMax.z ) );
	}
	else
	{
		return Vector( 0, 0, 0 );
	}
}

void Test_InitRandomEntitySpawner( const CCommand &args )
{
	// Put the list of registered functions into array form for convenience.
	g_StressEntityRegs.clear(); g_StressEntityRegs.shrink_to_fit();
	for ( CStressEntityReg *pCur=CStressEntityReg::GetListHead(); pCur; pCur=pCur->GetNext() )
		g_StressEntityRegs.push_back( pCur );

	// Create slots for all the entities..
	int nSlots = 100;
	if ( args.ArgC() >= 2 )
		nSlots = std::atoi( args[ 1 ] );

	g_StressEntities.clear(); g_StressEntities.shrink_to_fit();
	g_StressEntities.assign( nSlots, {} );

	Msg( "Test_InitRandomEntitySpawner: created %d slots.\n", nSlots );
}

void Test_SpawnRandomEntities( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		Error( "Test_SpawnRandomEntities <min # entities> <max # entities> missing arguments." );
	}

	if ( g_StressEntities.empty() )
	{
		Error( "Test_SpawnRandomEntities: not initialized (call Test_InitRandomEntitySpawner frst)." );
	} 

	int nMin = std::atoi( args[ 1 ] );
	int nMax = atoi( args[ 2 ] );
	int count = RandomInt( nMin, nMax );

	for ( int i=0; i < count; i++ )
	{
		int iSlot = RandomInt( 0, std::ssize( g_StressEntities ) - 1 );

		// Remove any old entity in this slot.
		if ( g_StressEntities[iSlot].Get() )
			UTIL_RemoveImmediate( g_StressEntities[iSlot] );

		// Create a new one in this slot.
		int iType = RandomInt( 0, std::ssize( g_StressEntityRegs ) - 1 );
		g_StressEntities[iSlot] = g_StressEntityRegs[iType]->GetFn()();
	}
}

void Test_RandomizeInPVS( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_RandomizeInPVS <percentage chance to change>" );
	}

	int percent = std::atoi( args[ 1 ] );
	for ( int i=0; i < std::ssize( g_StressEntities ); i++ )
	{
		CBaseEntity *pEnt = g_StressEntities[i];

		if ( pEnt )
		{
			if ( RandomInt( 0, 100 ) < percent )
			{
				if ( pEnt->IsEffectActive( EF_NODRAW ) )
					pEnt->RemoveEffects( EF_NODRAW );
				else
					pEnt->AddEffects( EF_NODRAW );
			}
		}
	}
}

void Test_RemoveAllRandomEntities()
{
	for ( int i=0; i < std::ssize( g_StressEntities ); i++ )
	{
		if ( g_StressEntities[i].Get() )
			UTIL_Remove( g_StressEntities[i] );
	}
}

ConCommand cc_Test_InitRandomEntitySpawner( side_registry(), "Test_InitRandomEntitySpawner", Test_InitRandomEntitySpawner, 0, FCVAR_CHEAT );
ConCommand cc_Test_SpawnRandomEntities( side_registry(), "Test_SpawnRandomEntities", Test_SpawnRandomEntities, 0, FCVAR_CHEAT );
ConCommand cc_Test_RandomizeInPVS( side_registry(), "Test_RandomizeInPVS", Test_RandomizeInPVS, 0, FCVAR_CHEAT );
ConCommand cc_Test_RemoveAllRandomEntities( side_registry(), "Test_RemoveAllRandomEntities", Test_RemoveAllRandomEntities, 0, FCVAR_CHEAT );

CStressEntityReg::CStressEntityReg( StressEntityFn fn)
{
	m_pFn = fn;
	m_pNext = s_pHead;
	s_pHead = this;
}

using namespace ::ks::reflect;

using namespace ::ks::reflect;

// -------------------------------------------------------------------------------- //
// An entity used to test traceline
// -------------------------------------------------------------------------------- //

class CTestTraceline : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TestTraceline", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 1 }>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 2 }>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTestTraceline, CPointEntity );

	void	Spawn_Impl( void );
	int  	UpdateTransmitState_Impl();

	DECLARE_SERVERCLASS();

private:
	void	Spin( void );
};
							  
// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CTestTraceline, DT_TestTraceline )

LINK_ENTITY_TO_CLASS( test_traceline, CTestTraceline );

void	CTestTraceline::Spawn_Impl( void )
{
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	SetNextThink( gpGlobals->curtime );

	SetThink( &CTestTraceline::Spin );
}

void	CTestTraceline::Spin( void )
{
	static ConVar	traceline_spin( side_registry(), "traceline_spin","1" );

	if (traceline_spin.GetInt())
	{
		float s = std::sin( gpGlobals->curtime );
		QAngle angles = GetLocalAngles();

		angles[0] = 180.0 * 0.5 * (s * s * s + 1.0f) + 90;
		angles[1] = gpGlobals->curtime * 10;
		   
		SetLocalAngles( angles );

	}
	SetNextThink( gpGlobals->curtime );
}

int CTestTraceline::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

CTextStatsMgr g_TextStatsMgr;	// The default text stats manager.

// ------------------------------------------------------------------------------------------ //
// CTextStatsMgr implementation.
// ------------------------------------------------------------------------------------------ //
CTextStatsMgr::CTextStatsMgr( void )
{
	m_szStatFilename[0] = 0;
}

bool CTextStatsMgr::WriteFile( IFileSystem *pFileSys, const char *pFilename )
{
	// If no filename was specified, use out preset one
	if ( !pFilename )
	{
		pFilename = m_szStatFilename;
	}

	FileHandle_t hFile = pFileSys->Open( pFilename, "wt", "LOGDIR" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return false;
	
	CTextStat *pHead = CTextStat::GetTextStatsList();
	for ( CTextStat *pCur=pHead->m_pNext; pCur != pHead; pCur=pCur->m_pNext )
	{
		if ( pCur->m_pMgr == this )
			pCur->m_PrintFn( pFileSys, hFile, pCur->m_pUserData );
	}

	pFileSys->Close( hFile );

	// Call each CTextStatFile..
	for( CTextStatFile *pCurFile=CTextStatFile::s_pHead; pCurFile; pCurFile=pCurFile->m_pNext )
	{
		pCurFile->m_pFn();
	}

	return true;
}

char *CTextStatsMgr::GetStatsFilename( void )
{
	return m_szStatFilename;
}

void CTextStatsMgr::SetStatsFilename( const char *sFilename )
{
	contract_assert( sFilename && sFilename[0] );

	V_strncpy( m_szStatFilename, sFilename, sizeof(m_szStatFilename) );
}

// ------------------------------------------------------------------------------------------ //
// CTextStat implementation.
// ------------------------------------------------------------------------------------------ //

CTextStat::CTextStat()
{
	m_pPrev = m_pNext = this;
	m_pMgr = nullptr;
}

CTextStat::CTextStat( TextStatPrintFn printFn, void *pUserData, CTextStatsMgr *pMgr )
{
	m_pPrev = m_pNext = this;
	Init( printFn, pUserData, pMgr );
}

CTextStat::~CTextStat()
{
	Term();		
}

void CTextStat::Init( TextStatPrintFn printFn, void *pUserData, CTextStatsMgr *pMgr )
{
	Term();

	m_pPrev = GetTextStatsList();
	m_pNext = GetTextStatsList()->m_pNext;
	m_pPrev->m_pNext = m_pNext->m_pPrev = this;
	
	m_PrintFn = printFn;
	m_pUserData = pUserData;
	m_pMgr = pMgr;
}

void CTextStat::Term()
{
	// Remove from the global list.
	m_pPrev->m_pNext = m_pNext;
	m_pNext->m_pPrev = m_pPrev;
	m_pPrev = m_pNext = this;
	m_pMgr = nullptr;
}

CTextStat::CTextStat( bool bGlobalListHead )
{
	contract_assert( bGlobalListHead );
	m_pPrev = m_pNext = this;
}

CTextStat* CTextStat::GetTextStatsList()
{
	static CTextStat theList( true );
	return &theList;
}

void CTextStat::RemoveFn( void *pUserData )
{
	CTextStat *pReg = (CTextStat*)pUserData;
	pReg->Term();
}

// ------------------------------------------------------------------------------------------ //
// CTextStatInt implementation.
// ------------------------------------------------------------------------------------------ //
				
CTextStatInt::CTextStatInt( const char *pName, int initialValue, CTextStatsMgr *pMgr )
{
	m_pName = pName;
	m_Value = initialValue;
	m_Reg.Init( &CTextStatInt::PrintFn, this, pMgr );
}

void CTextStatInt::PrintFn( IFileSystem *pFileSys, FileHandle_t hFile, void *pUserData )
{
	CTextStatInt *pStat = (CTextStatInt*)pUserData;
	pFileSys->FPrintf( hFile, "%s %d\n", pStat->m_pName, pStat->m_Value );
}

// ------------------------------------------------------------------------------------------ //
// CTextStatFile functions.
// ------------------------------------------------------------------------------------------ //

CTextStatFile *CTextStatFile::s_pHead = nullptr;

CTextStatFile::CTextStatFile( TextStatFileFn fn )
{
	m_pFn = fn;
	m_pNext = CTextStatFile::s_pHead;
	CTextStatFile::s_pHead = this;
}

class CTimedEventMgr;

using namespace ::ks::reflect;

// ------------------------------------------------------------------------------------------ //
// CEventRegister.
// ------------------------------------------------------------------------------------------ //

CEventRegister::CEventRegister()
{
	m_bRegistered = false;
	m_pEventMgr = nullptr;
}

CEventRegister::~CEventRegister()
{
	Term();
}

void CEventRegister::Init( CTimedEventMgr *pMgr, IEventRegisterCallback *pCallback )
{
	Term();
	m_pEventMgr = pMgr;
	m_pCallback = pCallback;
}

void CEventRegister::Term()
{
	// Unregister.
	if ( m_pEventMgr && m_bRegistered )
	{
		m_pEventMgr->RemoveEvent( this );
	}
}
	
void CEventRegister::SetUpdateInterval( float interval )
{
	contract_assert( m_pEventMgr );

	if ( m_pEventMgr )
	{
		// Register for this event.
		m_flUpdateInterval = interval;
		m_flNextEventTime = gpGlobals->curtime + m_flUpdateInterval;

		m_pEventMgr->RegisterForNextEvent( this );
	}
}

void CEventRegister::StopUpdates()
{
	if ( m_pEventMgr )
	{
		// Unregister our next event.
		m_pEventMgr->RemoveEvent( this );
	}
}

void CEventRegister::Reregister()
{
	if ( m_flUpdateInterval > 1e-6 && m_pEventMgr )
	{
		while ( m_flNextEventTime <= gpGlobals->curtime )
		{
			m_flNextEventTime += m_flUpdateInterval;
		}

		m_pEventMgr->RegisterForNextEvent( this );
	}
}

// ------------------------------------------------------------------------------------------ //
// CTimedEventMgr.
// ------------------------------------------------------------------------------------------ //

bool TimedEventMgr_LessFunc( CEventRegister* const &a, CEventRegister* const &b )
{
	return a->m_flNextEventTime > b->m_flNextEventTime;
}

CTimedEventMgr::CTimedEventMgr() = default;

void CTimedEventMgr::FireEvents()
{
	CVProfScope _( "CTimedEventMgr::FireEvents", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	while ( !m_Events.empty() )
	{
		// Fire the top element, then break out.
		CEventRegister *pEvent = m_Events.front();
		if ( gpGlobals->curtime >= pEvent->m_flNextEventTime )
		{
			// Reregister for the timed event, then fire the callback for the event.
			ks::heap_pop( m_Events, TimedEventMgr_LessFunc );
			pEvent->m_bRegistered = false;
			pEvent->Reregister();

			pEvent->m_pCallback->FireEvent();
		}
		else
		{
			break;
		}
	}
}

void CTimedEventMgr::RegisterForNextEvent( CEventRegister *pEvent )
{
	RemoveEvent( pEvent );
	ks::heap_push( m_Events, pEvent, TimedEventMgr_LessFunc );
	pEvent->m_bRegistered = true;
}

void CTimedEventMgr::RemoveEvent( CEventRegister *pEvent )
{
	if ( pEvent->m_bRegistered )
	{
		// Find the event in the list and remove it.
		int i = ks::find_index( m_Events, pEvent );
		if ( i >= 0 )
			ks::heap_erase( m_Events, i, TimedEventMgr_LessFunc );
	}
}
