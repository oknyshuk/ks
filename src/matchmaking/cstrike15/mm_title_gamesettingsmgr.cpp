// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.vstdlib;
#include "mm_title.h"

import std;

#include "filesystem.h"
#include "gametypes/igametypes.h"
#include "mathlib/expressioncalculator.h"

#include "csgo.spa.h"


#include "inputsystem/iinputsystem.h"


extern IGameTypes *g_pGameTypes;

struct AggregateSkillProperty
{
	char const *szSkill;
	unsigned int dwSkillPropertyId;
	unsigned int dwSkillMinPropertyId;
	unsigned int dwSkillMaxPropertyId;
};

static AggregateSkillProperty g_AggregateSkillProperties[] =
{
	{ "skill0",		PROPERTY_CSS_AGGREGATE_SKILL0,		PROPERTY_CSS_SEARCH_SKILL0_MIN,		PROPERTY_CSS_SEARCH_SKILL0_MAX },
	{ "skill1",		PROPERTY_CSS_AGGREGATE_SKILL1,		PROPERTY_CSS_SEARCH_SKILL1_MIN,		PROPERTY_CSS_SEARCH_SKILL1_MAX },
	{ "skill2",		PROPERTY_CSS_AGGREGATE_SKILL2,		PROPERTY_CSS_SEARCH_SKILL2_MIN,		PROPERTY_CSS_SEARCH_SKILL2_MAX },
	{ "skill3",		PROPERTY_CSS_AGGREGATE_SKILL3,		PROPERTY_CSS_SEARCH_SKILL3_MIN,		PROPERTY_CSS_SEARCH_SKILL3_MAX },
	{ "skill4",		PROPERTY_CSS_AGGREGATE_SKILL4,		PROPERTY_CSS_SEARCH_SKILL4_MIN,		PROPERTY_CSS_SEARCH_SKILL4_MAX },
	nullptr,
};

class CMatchTitleGameSettingsMgr : public IMatchTitleGameSettingsMgr
{
public:
	// Extends server game details
	void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest );

	// Adds the essential part of game details to be broadcast
	void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings );

	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys );

	// Executes the command on the session settings, this function on host
	// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
	// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
	// be modified
	void ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the lobby for game or adjust settings of new players who
	// join a game in progress, this function is allowed to modify
	// Members/Game subkeys and has to fill in modified players KeyValues
	void PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Initializes full game settings from potentially abbreviated game settings
	void InitializeGameSettings( KeyValues *pSettings, char const *szReason );

	// Sets the bspname key given a mapgroup
	void SetBspnameFromMapgroup( KeyValues *pSettings );
};

CMatchTitleGameSettingsMgr g_MatchTitleGameSettingsMgr;
IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr = &g_MatchTitleGameSettingsMgr;


//
// Implementation of CMatchTitleGameSettingsMgr
//

// Extends server game details
void CMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	// Server is always in game
	pDetails->SetString( "game/state", "game" );

	//
	// Determine map info
	//
	{
		int mode = g_pGameTypes->GetCurrentGameMode();
		int type = g_pGameTypes->GetCurrentGameType();
		const char *modeName = g_pGameTypes->GetGameModeFromInt( type,  mode );
		const char *typeName = g_pGameTypes->GetGameTypeFromInt( type );

		int numHumanSlots = g_pGameTypes->GetMaxPlayersForTypeAndMode( type, mode );
		pDetails->SetInt( "members/numSlots", numHumanSlots );

		pDetails->SetString( "game/map", si.m_szMapName );
		pDetails->SetString( "game/mapgroupname", si.m_szMapGroupName );
		pDetails->SetString( "game/mode", modeName );
		pDetails->SetString( "game/type", typeName );
		if ( !g_pMatchExtensions->GetIServerGameDLL()->IsValveDS() )
		{
			pDetails->SetInt( "game/hosted", 1 );
			pDetails->SetString( "options/server", "dedicated" );
		}
	}
}

// Adds the essential part of game details to be broadcast
void CMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{
	static KeyValues *pkvExt = KeyValues::FromString(
		"settings",
		" game { "
			" mapgroupname #empty# "
			" map #empty# "
			" mode #empty# "
			" type #empty# "
			" state #empty# "
			" hosted 0 "
			" spectate 0 "
			" apr 0 "
			" ark 0 "
			" loc #empty# "
		" } "
		);

	// TODO: Is this the appropriate spot to add in game values to initialize the dedicated server state?

	pDetails->MergeFrom( pkvExt, KeyValues::MERGE_KV_UPDATE );
}

// Initializes full game settings from potentially abbreviated game settings
void CMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings, const char *szReason )
{
	if ( KeyValues *kv = pSettings->FindKey( "game", true ) )
	{
		kv->SetString( "state", "lobby" );
	}

	const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", nullptr );
	// if no mapgroup specified, randomly select a mapgroup based on type and mode
	if ( !pMapGroupName )
	{
		const char *pGameTypeName = pSettings->GetString( "game/type", nullptr );
		const char *pGameModeName = pSettings->GetString( "game/mode", nullptr );
		if ( pGameTypeName && pGameModeName )
		{
			pMapGroupName = g_pGameTypes->GetRandomMapGroup( pGameTypeName, pGameModeName );
			if ( pMapGroupName )
			{
				pSettings->SetString( "game/mapgroupname", pMapGroupName );
			}
		}
	}

	// map name should not be coming in here, only mapgroup, so set mapname from mapgroupname
	SetBspnameFromMapgroup( pSettings );

	// Set the number of slots
	int numSlots = pSettings->GetInt( "members/numSlots", 10 );

	// If this is an official game, then we know how many slots we will force
	if ( !V_stricmp( "searchempty", pSettings->GetString( "options/createreason" ) ) &&
		!pSettings->GetBool( "game/hosted" ) )
	{
		const char *pGameTypeName = pSettings->GetString( "game/type" );
		const char *pGameModeName = pSettings->GetString( "game/mode" );
		int iType, iMode;
		if ( g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameTypeName, pGameModeName, iType, iMode ) )
			numSlots = g_pGameTypes->GetMaxPlayersForTypeAndMode( iType, iMode );
	}
	
	pSettings->SetInt( "members/numSlots", numSlots );


	if ( KeyValues *kvLocalPlayer = pSettings->FindKey( "members/machine0/player0" ) )
	{
		// If we are joining via nearby discovery then pass it to the host
		if ( int nNearbyJoin = pSettings->GetInt( "options/nby" ) )
		{
			kvLocalPlayer->SetInt( "game/nby", nNearbyJoin );
		}

		// After we initialized local player transfer that data into session
		pSettings->SetInt( "game/ark", kvLocalPlayer->GetInt( "game/ranking" )*10 );
		pSettings->SetInt( "game/apr", kvLocalPlayer->GetInt( "game/prime" ) );
		pSettings->SetString( "game/loc", kvLocalPlayer->GetString( "game/loc" ) );
	}
}

void CMatchTitleGameSettingsMgr::SetBspnameFromMapgroup( KeyValues *pSettings )
{
	const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", nullptr );
	const char *pMapName = pSettings->GetString( "game/map", nullptr );
	if ( !pMapName && pMapGroupName )
	{
		pMapName = g_pGameTypes->GetRandomMap( pMapGroupName );
		if ( pMapName && pMapName[0] )
		{			
			pSettings->SetString( "game/map", pMapName );
		}
	}
}

// Extends game settings update packet before it gets merged with
// session settings and networked to remote clients
void CMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{
	if ( char const *szMmQueueUpdate = pUpdateDeleteKeys->GetString( "update/game/mmqueue", nullptr ) )
	{
		int nAPR = pSettings->GetInt( "game/apr" );
		int nUpdateAlready = pUpdateDeleteKeys->GetInt( "update/game/apr", -1 );
		if ( nUpdateAlready >= 0 )
			nAPR = nUpdateAlready;
		pUpdateDeleteKeys->SetInt( "update/game/apr", nAPR | 0x2 );
	}

	if ( char const *szMmQueueUpdate = pUpdateDeleteKeys->GetString( "delete/game/mmqueue", nullptr ) )
	{
		int nAPR = pSettings->GetInt( "game/apr" );
		int nUpdateAlready = pUpdateDeleteKeys->GetInt( "update/game/apr", -1 );
		if ( nUpdateAlready >= 0 )
			nAPR = nUpdateAlready;
		pUpdateDeleteKeys->SetInt( "update/game/apr", nAPR & ~0x2 );
	}
}

// Prepares the lobby for game or adjust settings of new players who
// join a game in progress, this function is allowed to modify
// Members/Game subkeys and has to write modified players XUIDs
void CMatchTitleGameSettingsMgr::PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	// set player avatar/teams, etc	
}

void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate )
{
	bool bAllPrime = true;
	int nAvgRank = 0;
	int nHaveRank = 0;
	int nTotalPlayers = 0;

	char const *szBestCountry = "";
	float flBestCountryWeight = 0.0f;
	ks::idict< float > mapPlayerCountries;
	IPlayerLocal *pLocalPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	CPlayerId idLocalPlayer( pLocalPlayer ? pLocalPlayer->GetXUID() : 0ull );
	for ( int iMachine = 0, numMachines = pFullGameSettings->GetInt( "members/numMachines" ); iMachine < numMachines; ++iMachine )
	{
		KeyValues *pMachine = pFullGameSettings->FindKey( CFmtStr( "members/machine%d", iMachine ) );
		for ( int iPlayer = 0, numPlayers = pMachine->GetInt( "numPlayers" ); iPlayer < numPlayers; ++iPlayer )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", iPlayer ) );
			if ( !pPlayer )
				continue;

			if ( !pPlayer->GetInt( "game/prime" ) )
				bAllPrime = false;

			int nRanking = pPlayer->GetInt( "game/ranking" );
			if ( nRanking )
			{
				nAvgRank += nRanking;
				++ nHaveRank;
			}
			++ nTotalPlayers;

			char const *szLocation = pPlayer->GetString( "game/loc" );
			if ( !*szLocation )
				continue;

			float flNewWeightOfThisCountry = (
				mapPlayerCountries[szLocation] += ( 1.0f + ( ( CPlayerId( pPlayer->GetUint64( "xuid" ) ).GetAccountID() == idLocalPlayer.GetAccountID() ) ? 0.5f : 0.0f ) )
				);
			if ( flNewWeightOfThisCountry > flBestCountryWeight )
			{
				szBestCountry = szLocation;
				flBestCountryWeight = flNewWeightOfThisCountry;
			}
		}
	}

	if ( !nAvgRank )
	{
		nAvgRank = 7 * 10; // Nova II
	}
	else if ( nHaveRank == nTotalPlayers )
	{
		nAvgRank = ( nAvgRank * 10 ) / nHaveRank;
	}
	else
	{
		int nAvgRankedPeople = ( nAvgRank * 10 );
		for ( ; nHaveRank < nTotalPlayers; ++ nHaveRank )
		{
			nAvgRankedPeople += ( nAvgRankedPeople * 9 / nHaveRank / 10 );
		}
		nAvgRank = nAvgRankedPeople / nHaveRank;
	}

	int nAPR = bAllPrime ? 1 : 0;
	if ( *pFullGameSettings->GetString( "game/mmqueue" ) )
		nAPR |= 0x2;

	int numSlots = 5;
	if ( !V_stricmp( pFullGameSettings->GetString( "game/mode" ), "cooperative" ) )
		numSlots = 2;
	if ( nTotalPlayers >= numSlots )
		nAPR |= 0x4;

	pUpdate->SetInt( "game/ark", nAvgRank );
	pUpdate->SetInt( "game/apr", nAPR );
	pUpdate->SetString( "game/loc", szBestCountry );

#ifdef _DEBUG
	DevMsg( "UpdateAggregateMembersSettings: ark %d->%d, apr %d->%d, loc %s->%s\n",
		pFullGameSettings->GetInt( "game/ark" ), nAvgRank,
		pFullGameSettings->GetInt( "game/apr" ), nAPR,
		pFullGameSettings->GetString( "game/loc" ), szBestCountry );
#endif
}

// Executes the command on the session settings, this function on host
// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
// be modified
void CMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	char const *szCommand = pCommand->GetName();

	if ( !V_stricmp( "Game::SetPlayerRanking", szCommand ) )
	{
		if ( !V_stricmp( "host", pSessionSystemData->GetString( "type", "host" ) ) &&
			// !V_stricmp( "lobby", pSessionSystemData->GetString( "state", "lobby" ) ) && - avatars also update when players change team ingame
			ppPlayersUpdated )
		{
			XUID xuidPlayer = pCommand->GetUint64( "xuid" );

			// We know that the current session is the host. Validate that it is either updating itself, or a remote user
			// is updating their own data
			if ( !pSessionSystemData	// offline session OK
				|| ( ( xuidPlayer == pSessionSystemData->GetUint64( "xuidHost" ) ) && !pCommand->GetUint64( "_remote_xuidsrc" ) ) // host player update issued locally
				|| ( xuidPlayer == pCommand->GetUint64( "_remote_xuidsrc" ) ) // remote command on behalf of matching XUID
				)
				; // ok to process this update
			else
				return;

			// Find the layer that is going to be updated
			KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, xuidPlayer );
			if ( !pPlayer )
				return;

			KeyValues *kvGameKey = pCommand->FindKey( "game" );
			if ( pPlayer && kvGameKey )
			{
				KeyValues *kvPlayerGame = pPlayer->FindKey( "game", true );
				if ( !kvPlayerGame )
					return;

				kvPlayerGame->MergeFrom( kvGameKey, KeyValues::MERGE_KV_UPDATE );

				// Notify the sessions of a player update
				* ( ppPlayersUpdated ++ ) = pPlayer;

				// Also recompute aggregate session fields
				if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
				{
					KeyValues *kvPackage = new KeyValues( "Update" );
					if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
					{
						UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
					}
					pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
				}
			}
			return;
		}
	}
}

//-----------------------------------------------------------------------------
#include "../facade/imatchtitlegamesettingsmgr.inl"
