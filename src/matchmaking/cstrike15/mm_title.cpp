// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.fmtstr;
import ks.tier1.iappsystem;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
#include "mm_title.h"

import std;
#include "gametypes/igametypes.h"

extern IGameTypes *g_pGameTypes;


CMatchTitle::CMatchTitle()
{
}

CMatchTitle::~CMatchTitle()
{
}


//
// Init / shutdown
//

InitReturnVal_t CMatchTitle::Init()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "server_pre_shutdown", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "game_newmap", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "finale_start", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "round_start", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "round_end", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "difficulty_changed", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "player_connect", false );
		mgr->AddListener( this, GameEventCallback_t( std::cw<&CMatchTitle::FireGameEvent>, this ), "player_disconnect", false );
	}


	g_pGameTypes->Initialize();

	return INIT_OK;
}

void CMatchTitle::Shutdown()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->RemoveListener( this );
	}
}


//
// Implementation
//

uint64 CMatchTitle::GetTitleID()
{
	return 0ull;
}

uint64 CMatchTitle::GetTitleServiceID()
{
	return 0ull;
}


uint64 CMatchTitle::GetTitleSettingsFlags()
{
	return MATCHTITLE_SETTING_MULTIPLAYER
		| MATCHTITLE_VOICE_INGAME
	;
}



void CMatchTitle::PrepareNetStartupParams( void *pNetStartupParams )
{

}

int CMatchTitle::GetTotalNumPlayersSupported()
{
	return 64; // On PC this is not limited, return max number dedicated servers can ever run with
}

// Get a guest player name
char const * CMatchTitle::GetGuestPlayerName( int iUserIndex )
{
	if ( ILocalize *pLocalize = g_pMatchExtensions->GetILocalize() )
	{
		if ( wchar_t* wStringTableEntry = pLocalize->Find( "#SFUI_LocalPlayer" ) )
		{
			static char szName[ MAX_PLAYER_NAME_LENGTH ] = {0};
			pLocalize->ConvertUnicodeToANSI( wStringTableEntry, szName, std::size( szName ) );
			return szName;
		}
	}
	
	return "";
}

// Sets up all necessary client-side convars and user info before
// connecting to server
void CMatchTitle::PrepareClientForConnect( KeyValues *pSettings )
{
	int numPlayers = 1;

	//
	// Now we set the convars
	//

	for ( int k = 0; k < numPlayers; ++ k )
	{
		int iController = k;
		IPlayerLocal *pPlayerLocal = g_pPlayerManager->GetLocalPlayer( iController );
		if ( !pPlayerLocal )
			continue;

		// Set "name"
		static SplitScreenConVarRef s_cl_name( "name" );
		char const *szName = pPlayerLocal->GetName();
		s_cl_name.SetValue( k, szName );

		// Set "networkid_force"
	}
}

bool CMatchTitle::StartServerMap( KeyValues *pSettings )
{
	int numPlayers = 1;

	char const *szMap = pSettings->GetString( "game/map", nullptr );
	if ( !szMap )
		return false;

	// Check that we have the server interface and that the map is valid
	if ( !g_pMatchExtensions->GetIVEngineServer() )
		return false;
	if ( !g_pMatchExtensions->GetIVEngineServer()->IsMapValid( szMap ) )
		return false;

	//
	// Prepare game dll reservation package
	//
	KeyValues *pGameDllReserve = g_pMatchFramework->GetMatchNetworkMsgController()->PackageGameDetailsForReservation( pSettings );
	KeyValues::AutoDelete autodelete( pGameDllReserve );

	pGameDllReserve->SetString( "map/mapcommand", ( numPlayers <= 1 ) ? "map" : "ss_map" );

	// Run map based off the faked reservation packet
	g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForKeyValues( pGameDllReserve );

	return true;
}

//
// <Vitaliy> July-2012
// CS:GO hack: training map has been implemented storing progress in convars instead of
// properly storing it in game profile data, force storing the convars into profile
// when the training session finishes here
// Please, do not use this approach in future code.
//
bool g_bPlayingTrainingMap = false;

void CMatchTitle::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();
	
	if ( !V_stricmp( "OnPlayerMachinesConnected", szEvent ) ||
		!V_stricmp( "OnPlayerMachinesDisconnected", szEvent ) )
	{
		// Player counts changed on host, update aggregate fields
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )
			return;
		KeyValues *kvPackage = new KeyValues( "Update" );
		if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
		{
			extern void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate );
			UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
		}
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
	}
	else if ( !V_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !V_stricmp( pEvent->GetString( "state" ), "created" ) ||
			 !V_stricmp( pEvent->GetString( "state" ), "ready" ) )
		{
			if ( IMatchSession *pSession = g_pMatchFramework->GetMatchSession() )
			{
				if ( !V_stricmp( "training", pSession->GetSessionSettings()->GetString( "game/type" ) ) )
					g_bPlayingTrainingMap = true;
			}
		}
		else if ( !V_stricmp( pEvent->GetString( "state" ), "closed" ) )
		{
			if ( g_bPlayingTrainingMap )
			{
				g_bPlayingTrainingMap = false;
				g_pMatchExtensions->GetIVEngineClient()->ClientCmd_Unrestricted( CFmtStr( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );
			}
		}
	}
	else if ( !V_stricmp( szEvent, "Client::CmdKeyValues" ) )
	{
		KeyValues *pCmd = pEvent->GetFirstTrueSubKey();
		if ( !pCmd )
			return;
		char const *szCmd = pCmd->GetName();
		if ( !V_stricmp( "ExtendedServerInfo", szCmd ) )
		{
			KeyValuesDumpAsDevMsg( pCmd, 2, 1 );
			g_pGameTypes->SetAndParseExtendedServerInfo( pCmd );
		}
	}
}

//
//
//

void CMatchTitle::FireGameEvent( IGameEvent *pIGameEvent )
{
	// Parse the game event
	char const *szGameEvent = pIGameEvent->GetName();
	if ( !szGameEvent || !*szGameEvent )
		return;

	// Check if the current match session is on an active game server
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;
	KeyValues *pSessionSettings = pMatchSession->GetSessionSettings();
	char const *szGameServer = pSessionSettings->GetString( "server/server", "" );
	char const *szSystemLock = pSessionSettings->GetString( "system/lock", "" );
	if ( ( !szGameServer || !*szGameServer ) &&
		( !szSystemLock || !*szSystemLock ) )
		return;

	// Also don't run on the client when there's a host
	char const *szSessionType = pMatchSession->GetSessionSystemData()->GetString( "type", nullptr );
	if ( szSessionType && !V_stricmp( szSessionType, "client" ) )
		return;

	if ( !V_stricmp( "round_start", szGameEvent ) )
	{
		KeyValues *kvUpdate = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( kvUpdate );

		pMatchSession->UpdateSessionSettings( kvUpdate );
	}
	else if ( !V_stricmp( "round_end", szGameEvent ) )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !V_stricmp( "finale_start", szGameEvent ) )
	{
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state finale "
				" } "
			" } "
			) ) );
	}
	else if ( !V_stricmp( "game_newmap", szGameEvent ) )
	{
		const char *szMapName = pIGameEvent->GetString( "mapname", "" );

		KeyValues *kvUpdate = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( kvUpdate );

		contract_assert( szMapName && *szMapName );
		if ( szMapName && *szMapName )
		{
			kvUpdate->SetString( "update/game/map", szMapName );
		}

		pMatchSession->UpdateSessionSettings( kvUpdate );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !V_stricmp( "server_pre_shutdown", szGameEvent ) )
	{
		char const *szReason = pIGameEvent->GetString( "reason", "quit" );
		if ( !V_stricmp( szReason, "quit" ) )
		{
			DevMsg( "Received server_pre_shutdown notification - server is shutting down...\n" );

			// Transform the server shutdown event into game end event
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
				"OnEngineDisconnectReason", "reason", "Server shutting down"
				) );
		}
	}
}

//-----------------------------------------------------------------------------
#include "../facade/imatchtitle.inl"
