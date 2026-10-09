// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client side CTeam class

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "c_team.h"

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Team's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_PlayerList(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_Team *pTeam = (C_Team*)pOut;
	pTeam->m_aPlayers[pData->m_iElement] = pData->m_Value.m_Int;
}


void RecvProxyArrayLength_PlayerArray( void *pStruct, int objectID, int currentArrayLength )
{
	C_Team *pTeam = (C_Team*)pStruct;
	
	if ( std::ssize( pTeam->m_aPlayers ) != currentArrayLength )
		pTeam->m_aPlayers.assign( currentArrayLength, {} );
}


IMPLEMENT_REFLECT_CLIENTCLASS( C_Team, DT_Team, CTeam )

IMPLEMENT_REFLECT_PREDMAP( C_Team );

// Global list of client side team entities
std::vector< C_Team * > g_Teams;

//=================================================================================================
// C_Team functionality

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Team::C_Team()
{
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0;	
	m_scoreOvertime = 0;
	m_iClanID = 0;
	
	memset( m_szTeamname, 0, sizeof(m_szTeamname) );
	memset( m_szClanTeamname, 0, sizeof(m_szClanTeamname) );
	memset( m_szTeamFlagImage, 0, sizeof(m_szTeamFlagImage) );
	memset( m_szTeamLogoImage, 0, sizeof(m_szTeamLogoImage) );
	memset( m_szTeamMatchStat, 0, sizeof( m_szTeamMatchStat ) );
	
	m_iDeaths = 0;
	m_iPing = 0;
	m_iPacketloss = 0;
	m_bSurrendered = 0;
	m_numMapVictories = 0;

	// Add myself to the global list of team entities
	g_Teams.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Team::~C_Team()
{
	ks::erase_first( g_Teams, this );
}


void C_Team::RemoveAllPlayers()
{
	m_aPlayers.clear();
}

void C_Team::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}


//-----------------------------------------------------------------------------
// Gets the ith player on the team (may return NULL) 
//-----------------------------------------------------------------------------
C_BasePlayer* C_Team::GetPlayer( int idx )
{
	return (C_BasePlayer*)cl_entitylist->GetEnt(m_aPlayers[idx]);
}


int C_Team::GetTeamNumber() const
{
	return m_iTeamNum;
}


//=================================================================================================
// TEAM HANDLING
//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_Team::Get_Name( void )
{
	return m_szTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_Team::Get_ClanName( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szClanTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_Team::Get_FlagImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamFlagImage;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_Team::Get_LogoImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamLogoImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Team::Get_Deaths( void )
{
	return m_iDeaths;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Team::Get_Ping( void )
{
	return m_iPing;
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of players in this team
//-----------------------------------------------------------------------------
int C_Team::Get_Number_Players( void )
{
	return std::ssize( m_aPlayers );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified player is on this team
//-----------------------------------------------------------------------------
bool C_Team::ContainsPlayer( int iPlayerIndex )
{
	for (int i = 0; i < std::ssize( m_aPlayers ); i++ )
	{
		if ( m_aPlayers[i] == iPlayerIndex )
			return true;
	}

	return false;
}


void C_Team::ClientThink()
{
}

int C_Team::GetGGLeader( int nTeam )
{
	if ( nTeam == TEAM_CT )
		return m_nGGLeaderEntIndex_CT;
	else if ( nTeam == TEAM_TERRORIST )
		return m_nGGLeaderEntIndex_T;

	return -1;
}

//=================================================================================================
// GLOBAL CLIENT TEAM HANDLING
//=================================================================================================
// Purpose: Get the C_Team for the local player
//-----------------------------------------------------------------------------
C_Team *GetLocalTeam( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return nullptr;
	
	return GetPlayersTeam( player->index );
}

//-----------------------------------------------------------------------------
// Purpose: Get the C_Team for the specified team number
//-----------------------------------------------------------------------------
C_Team *GetGlobalTeam( int iTeamNumber )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->GetTeamNumber() == iTeamNumber )
			return g_Teams[i];
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of teams you can access via GetGlobalTeam() (hence the +1)
//-----------------------------------------------------------------------------
int GetNumTeams()
{
	return std::ssize( g_Teams ) + 1; 
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
C_Team *GetPlayersTeam( int iPlayerIndex )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex ) )
			return g_Teams[i];
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
C_Team *GetPlayersTeam( C_BasePlayer *pPlayer )
{
	return GetPlayersTeam( pPlayer->entindex() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the two specified players are on the same team
//-----------------------------------------------------------------------------
bool ArePlayersOnSameTeam( int iPlayerIndex1, int iPlayerIndex2 )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex1 ) && g_Teams[i]->ContainsPlayer( iPlayerIndex2 ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of team managers
//-----------------------------------------------------------------------------
int GetNumberOfTeams( void )
{
	return std::ssize( g_Teams );
}
