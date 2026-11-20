// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// HACKHACK fix this include
import std;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.random;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
#include "server.h"
#include "host_cmd.h"
#include "keys.h"
#include "vengineserver_impl.h"
#include "gl_matsysiface.h"
#include "checksum_engine.h"
#include "sv_main.h"
#include "host.h"
#include "demo.h"
#include "cdll_int.h"
#include "networkstringtableserver.h"
#include "networkstringtableclient.h"
#include "host_state.h"
#include "r_local.h"
#include "PlayerState.h"
#include "profile.h"
#include "cl_main.h"
#include "zone.h"
#include "GameEventManager.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "cmd.h"
#include "filesystem.h"
#include "icliententity.h"
#include "hltvserver.h"
#include "cdll_engine_int.h"
#include "cl_splitscreen.h"
#ifndef DEDICATED
#include "engineui.h"
#endif
#include "sound.h"
#include "voice.h"
#include "sv_rcon.h"
#include "matchmaking/imatchframework.h"
#include "snd_audio_source.h"
#include "netconsole.h"

extern IVEngineClient *engineClient;

#define PLAYERID_PREFIX "STEAM_"

#ifndef DEDICATED
bool g_bInEditMode = false;
#endif
KeyValues *g_pLaunchOptions = nullptr;

void PerformKick( cmd_source_t commandSource, int iSearchIndex, char* szSearchString, bool bForceKick, const char* pszMessage );

ConVar host_name_store( "host_name_store", "1", FCVAR_RELEASE, "Whether hostname is recorded in game events and GOTV." );
ConVar host_info_show( "host_info_show", "1", FCVAR_RELEASE, "How server info gets disclosed in server queries: 0 - query disabled, 1 - show only general info, 2 - show full info" );
static void HostnameChanged( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( sv.IsActive() && host_name_store.GetBool() )
	{
		// look up the descriptor first to avoid a DevMsg for HL2 and mods that don't define a
		// hostname_change event
		CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( "hostname_changed" );
		if ( descriptor )
		{
			IGameEvent *event = g_GameEventManager.CreateEvent( "hostname_changed" );
			if ( event )
			{
				ConVarRef var( pConVar );
				event->SetString( "hostname", var.GetString() );
				g_GameEventManager.FireEvent( event );
			}
		}
	}
}
ConVar host_name( "hostname", "", FCVAR_RELEASE, "Hostname for server.", false, 0.0f, false, 0.0f, HostnameChanged );
ConVar host_map( "host_map", "", FCVAR_RELEASE, "Current map name." );

bool CanShowHostTvStatus()
{
	if ( !serverGameDLL )
		return true;

	if ( serverGameDLL->IsValveDS() )
	{
		// By default OFFICIAL server will NOT print TV information in "status" output
		// Running with -display_tv_status will reveal GOTV information
		static bool s_bCanShowHostTvStatusOFFICIAL = !!CommandLine()->FindParm( "-display_tv_status" );
		return s_bCanShowHostTvStatusOFFICIAL;
	}
	else
	{
		// By default COMMUNITY server will print TV information in "status" output
		// Running with -disable_tv_status will conceal GOTV information
		static bool s_bCanShowHostTvStatusCOMMUNITY = !CommandLine()->FindParm( "-disable_tv_status" );
		return s_bCanShowHostTvStatusCOMMUNITY;
	}
}


ConVar voice_recordtofile("voice_recordtofile", "0", FCVAR_RELEASE, "Record mic data and decompressed voice data into 'voice_micdata.wav' and 'voice_decompressed.wav'");
ConVar voice_inputfromfile("voice_inputfromfile", "0",FCVAR_RELEASE, "Get voice input from 'voice_input.wav' rather than from the microphone.");

uint GetGameAppID()
{
	return 215;
}

// Globals
int	gHostSpawnCount = 0;


// store arbitrary launch arguments in KeyValues to avoid having to add code for every new
//   launch parameter (like edit mode, commentary mode, background, etc. do)
void SetLaunchOptions( const CCommand &args )
{
	if ( g_pLaunchOptions )
	{
		g_pLaunchOptions->deleteThis();
	}
	g_pLaunchOptions = new KeyValues( "LaunchOptions" );
	for ( int i = 0 ; i < args.ArgC() ; i++ )
	{
		g_pLaunchOptions->SetString( va("Arg%d", i), args[i] );
	}
}

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f (void)
{
	HostState_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void _restart( const CCommand &args );
static ConCommand _restart_command( "_restart", _restart, "Shutdown and restart the engine." );
static void _restart( const CCommand &args )
{
	/*
	// FIXME:  How to handle restarts?
#ifndef DEDICATED
	if ( !EngineTool_CheckQuitHandlers() )
	{
		return;
	}
#endif
	*/

	HostState_Restart();
}

#ifndef DEDICATED
//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
void Host_LightCrosshair (void);

static ConCommand light_crosshair( "light_crosshair", Host_LightCrosshair, "Show texture color at crosshair", FCVAR_CHEAT );

void Host_LightCrosshair (void)
{
	Vector endPoint;
	Vector lightmapColor;

	// max_range * sqrt(3)
	VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );
	
	R_LightVec( MainViewOrigin(), endPoint, true, lightmapColor );
	int r = LinearToTexture( lightmapColor.x );
	int g = LinearToTexture( lightmapColor.y );
	int b = LinearToTexture( lightmapColor.z );

	ConMsg( "Luxel Value: %d %d %d\n", r, g, b );
}
#endif

/*
==================
Host_Status_PrintClient

Print client info to console 
==================
*/
void Host_Status_PrintClient( IClient *client, bool bShowAddress, void (*print) (const char *fmt, ...) )
{
	INetChannelInfo *nci = client->GetNetChannel();

	const char *state = "challenging";

	if ( client->IsActive() )
		state = "active";
	else if ( client->IsSpawned() )
		state = "spawning";
	else if ( client->IsConnected() )
		state = "connecting";
	
	if ( nci != nullptr )
	{
		print( "# %2i %i \"%s\" %s %s %i %i %s %d", 
			client->GetUserID(), client->GetPlayerSlot() + 1, client->GetClientName(), client->GetNetworkIDString(), COM_FormatSeconds( nci->GetTimeConnected() ),
			(int)(1000.0f*nci->GetAvgLatency( FLOW_OUTGOING )), (int)(100.0f*nci->GetAvgLoss(FLOW_INCOMING)), state, (int)nci->GetDataRate() );

		if ( bShowAddress ) 
		{
			print( " %s", nci->GetAddress() );
		}
	}
	else
	{
		print( "#%2i \"%s\" %s %s %.0f", 
			client->GetUserID(), client->GetClientName(), client->GetNetworkIDString(), state, client->GetUpdateRate() );
	}
	
	print( "\n" );

}

typedef void ( *FnPrintf_t )(const char *fmt, ...);

void Host_Client_Printf(const char *fmt, ...)
{
	std::va_list		argptr;
	char		string[1024];

	__builtin_va_start (argptr,fmt);
	V_vsnprintf (string, sizeof( string ), fmt,argptr);
	__builtin_va_end (argptr);

	host_client->ClientPrintf( "%s", string );
}

static void Host_Client_PrintfStub(const char *fmt, ...)
{
}

void Host_PrintStatus( cmd_source_t commandSource, void ( *print )(const char *fmt, ...), bool bShort )
{
	bool bWithAddresses = ( ( commandSource != kCommandSrcNetClient ) && ( commandSource != kCommandSrcNetServer ) && ( print == ConMsg ) ); // guarantee to never print for remote

	IClient	*client;
	int j;

	if ( !print ) { return; }

	// ============================================================
	// Server status information.
	print( "hostname: %s\n", host_name.GetString() );

	if ( bWithAddresses )
	{
		print( "version : %s/%d %d/%d\n",
			Sys_GetVersionString(), GetHostVersion(),
			GetServerVersion(), build_number() );
	}
	else
	{
		print( "version : %s\n",
			Sys_GetVersionString() );
	}

	if ( NET_IsMultiplayer() )
	{
		print( "udp/ip  : %s:%i\n", net_local_adr.ToString(true), sv.GetUDPPort() );

		const char *osType =
			"Linux";

		print( "os      :  %s\n", osType );

		char const *serverType = sv.IsHLTV() ? "hltv" : ( sv.IsDedicated() ? ( serverGameDLL->IsValveDS() ? "official dedicated" : "community dedicated" ) : "listen" );
		print( "type    :  %s\n", serverType );
	}

#ifndef DEDICATED												// no client on dedicated server
	if ( !sv.IsDedicated() && GetBaseLocalClient().IsConnected() )
	{
		print( "map     : %s at: %d x, %d y, %d z\n", sv.GetMapName(), (int)MainViewOrigin()[0], (int)MainViewOrigin()[1], (int)MainViewOrigin()[2]);
	}
#else
	{
		print( "map     : %s\n", sv.GetMapName() );
	}
#endif

	if ( CanShowHostTvStatus() )
	{
		for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
		{
			print( "gotv[%i]:  port %i, delay %.1fs, rate %.1f\n", hltv.GetIndex(), hltv->GetUDPPort(), hltv->GetDirector() ? hltv->GetDirector()->GetDelay() : 0.0f, hltv->GetSnapshotRate() );
		}
	}

	int nHumans;
	int nMaxHumans;
	int nBots;

	sv.GetMasterServerPlayerCounts( nHumans, nMaxHumans, nBots );

	print( "players : %i humans, %i bots (%i/%i max) (%s)\n\n",
		nHumans, nBots, nMaxHumans, sv.GetNumGameSlots(), sv.IsHibernating() ? "hibernating" : "not hibernating" );
	// ============================================================
#if SUPPORT_NET_CONSOLE
	if ( g_pNetConsoleMgr && g_pNetConsoleMgr->IsActive() && bWithAddresses )
	{
		print( "netcon  :  %s:%i\n", net_local_adr.ToString( true), g_pNetConsoleMgr->GetAddress().GetPort() );
	}
#endif

	// Early exit for this server.
	if ( bShort )
	{
		for ( j=0 ; j < sv.GetClientCount() ; j++ )
		{
			client = sv.GetClient( j );

			if ( !client->IsActive() )
				continue;

			if ( !bWithAddresses && !CanShowHostTvStatus() && client->IsHLTV() )
				continue;

			print( "#%i - %s\n" , j + 1, client->GetClientName() );
		}
		return;
	}

	// the header for the status rows
	print( "# userid name uniqueid connected ping loss state rate" );
	if ( bWithAddresses )
	{
		print( " adr" ); 
	}
	print( "\n" );

	for ( j=0 ; j < sv.GetClientCount() ; j++ )
	{
		client = sv.GetClient( j );

		if ( !client->IsConnected() )
			continue; // not connected yet, maybe challenging

		if ( !CanShowHostTvStatus() && client->IsHLTV() )
			continue;
		
		Host_Status_PrintClient( client, bWithAddresses, print );
	}
	print( "#end\n" );

}

//-----------------------------------------------------------------------------
// Host_Status_f
//-----------------------------------------------------------------------------
static void status( const CCommand &args );
static ConCommand status_command( "status", status, "Display map and connection status." );
static void status( const CCommand &args )
{
	void (*print) (const char *fmt, ...) = Host_Client_PrintfStub;

	if ( args.Source() != kCommandSrcNetClient )
	{
		if ( !sv.IsActive() )
		{
			Cmd_ForwardToServer( args );
			return;
		}
		print = ConMsg;
	}
	else
	{
		print = Host_Client_Printf;
	}

	bool bShort = false;
	if ( args.ArgC() == 2 )
	{
		if ( !V_stricmp( args[1], "short" ) )
		{
			bShort = true;
		}
	}

	Host_PrintStatus( args.Source(), print, bShort );
}

static void hltv_replay_status( const CCommand &args );
static ConCommand hltv_replay_status_command( "hltv_replay_status", hltv_replay_status, "Show Killer Replay status and some statistics, works on listen or dedicated server." );
static void hltv_replay_status( const CCommand &args )
{
	HltvReplayStats_t hltvStats;

	for ( int j = 0; j < sv.GetClientCount(); j++ )
	{
		IClient *client = sv.GetClient( j );

		if ( !client->IsConnected() )
			continue; // not connected yet, maybe challenging

		if ( !CanShowHostTvStatus() && client->IsHLTV() )
			continue;

		INetChannelInfo *nci = client->GetNetChannel();

		const char *state = "challenging";

		if ( client->IsActive() )
			state = "active";
		else if ( client->IsSpawned() )
			state = "spawning";
		else if ( client->IsConnected() )
			state = "connecting";

		if ( nci != nullptr )
		{
			ConMsg( "# %2i %i \"%s\" %s %s %s %s",
				client->GetUserID(), client->GetPlayerSlot() + 1, client->GetClientName(), client->GetNetworkIDString(), COM_FormatSeconds( nci->GetTimeConnected() ),
				state, client->GetHltvReplayStatus() );
			if ( client->GetHltvReplayDelay() )
				ConMsg( ", in replay NOW" );
		}
		else
		{
			ConMsg( "#%2i \"%s\" %s %s %s",
				client->GetUserID(), client->GetClientName(), client->GetNetworkIDString(), state, client->GetHltvReplayStatus() );
		}

		if ( static_cast< CBaseClient * >( client )->m_client_kind == CBaseClient::client_kind_t::game )
		{
			CGameClient *pClient = static_cast< CGameClient * >( client );
			if ( pClient->m_HltvReplayStats.nStartRequests )
				hltvStats += pClient->m_HltvReplayStats;
			if ( pClient->m_nForceWaitForTick > 0 )
			{
				ConMsg( ", force-waiting for tick %d - server tick %d, current frame %d", pClient->m_nForceWaitForTick, sv.GetTick(), pClient->m_pCurrentFrame ? pClient->m_pCurrentFrame->tick_count : 0 );
			}
		}

		ConMsg( "\n" );
	}

	extern HltvReplayStats_t m_DisconnectedClientsHltvReplayStats;
	if ( m_DisconnectedClientsHltvReplayStats.nClients > 1 )
		ConMsg( "%u disconnected clients: %s\n", m_DisconnectedClientsHltvReplayStats.nClients - 1, m_DisconnectedClientsHltvReplayStats.AsString() );

	if ( hltvStats.nClients > 0 )
	{
		ConMsg( "%u current clients: %s\n", hltvStats.nClients, hltvStats.AsString() );
	}
}





//-----------------------------------------------------------------------------
// Host_Ping_f
//-----------------------------------------------------------------------------
static void ping( const CCommand &args );
static ConCommand ping_command( "ping", ping, "Display ping to server." );
static void ping( const CCommand &args )
{
	if ( args.Source() != kCommandSrcNetClient )
	{
		Cmd_ForwardToServer( args );
		return;
	}

	host_client->ClientPrintf( "Client ping times:\n" );

	for ( int i=0; i< sv.GetClientCount(); i++ )
	{
		IClient	*client = sv.GetClient(i);

		if ( !client->IsConnected() || client->IsFakeClient() )
			continue;

		host_client->ClientPrintf ("%4.0f ms : %s\n", 
			1000.0f * client->GetNetChannel()->GetAvgLatency( FLOW_OUTGOING ), client->GetClientName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : editmode - 
//-----------------------------------------------------------------------------
extern void GetPlatformMapPath( const char *pMapPath, char *pPlatformMapPath, int maxLength );

bool CL_HL2Demo_MapCheck( const char *name )
{
	if ( CL_IsHL2Demo() && !sv.IsDedicated() )
	{
		if (    !V_stricmp( name, "d1_trainstation_01" ) || 
				!V_stricmp( name, "d1_trainstation_02" ) || 
				!V_stricmp( name, "d1_town_01" ) || 
				!V_stricmp( name, "d1_town_01a" ) || 
				!V_stricmp( name, "d1_town_02" ) || 
				!V_stricmp( name, "d1_town_03" ) ||
				!V_stricmp( name, "background01" ) ||
				!V_stricmp( name, "background03" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

bool CL_PortalDemo_MapCheck( const char *name )
{
	if ( CL_IsPortalDemo() && !sv.IsDedicated() )
	{
		if (    !V_stricmp( name, "testchmb_a_00" ) || 
				!V_stricmp( name, "testchmb_a_01" ) || 
				!V_stricmp( name, "testchmb_a_02" ) || 
				!V_stricmp( name, "testchmb_a_03" ) || 
				!V_stricmp( name, "testchmb_a_04" ) || 
				!V_stricmp( name, "testchmb_a_05" ) ||
				!V_stricmp( name, "testchmb_a_06" ) ||
				!V_stricmp( name, "background1" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

enum EMapFlags
{
	EMAP_NONE = 0,

	EMAP_EDIT_MODE = (1<<0),
	EMAP_BACKGROUND = (1<<1),
	EMAP_SPLITSCREEN = (1<<3)

};

int _Host_Map_f_CompletionFunc( char const *cmdname, char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

// Note, leaves name alone if no match possible
static bool Host_Map_Helper_FuzzyName( const CCommand &args, char *name, std::size_t bufsize )
{
	char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
	std::string argv0;
	argv0 = args.Arg( 0 );
	argv0 += " ";

	if ( _Host_Map_f_CompletionFunc( argv0.c_str(), args[1], commands ) > 0 )
	{
		V_strncpy( name, &commands[ 0 ][ std::ssize( argv0 ) ], bufsize );
		return true;
	}
	return false;
}

void Host_Changelevel_f( const CCommand &args );
void Host_Map_Helper( const CCommand &args, EMapFlags flags )
{
	char	name[MAX_QPATH];

	if (args.ArgC() < 2)
	{
		Warning("No map specified\n");
		return;
	}

	if ( ( sv.IsActive() && !sv.IsSinglePlayerGame() && !sv.IsLevelMainMenuBackground() ) ||
		 ( sv.IsActive() && sv.IsDedicated() ) )
	{
		// Using the 'map' command while in a map disconnects all players.
		// Ease the pain of this common error by forwarding to the correct command.
		Host_Changelevel_f( args );
		return;
	}
	
	bool bBackground = ( flags & EMAP_BACKGROUND ) != 0;
	bool bSplitScreenConnect = ( flags & EMAP_SPLITSCREEN ) != 0;

	char ppath[ MAX_QPATH ];

	// If there is a .bsp on the end, strip it off!
	V_StripExtension( args[ 1 ], ppath, sizeof( ppath ) );

	// Call with quiet flag for initial search
	if ( !modelloader->Map_IsValid( ppath, true ) )
	{
		Host_Map_Helper_FuzzyName( args, ppath, sizeof( ppath ) );
		if ( !modelloader->Map_IsValid( ppath ) )
		{
			Warning( "map load failed: %s not found or invalid\n", ppath );
			return;
		}
	}

	GetPlatformMapPath( ppath, name, sizeof( name ) );

	// If I was in edit mode reload config file
	// to overwrite WC edit key bindings
#if !defined(DEDICATED)
	bool bEditmode = ( flags & EMAP_EDIT_MODE ) != 0;
	if ( !bEditmode )
	{
		if ( g_bInEditMode )
		{
			// Re-read config from disk
			Host_ReadConfiguration( -1, false );
			g_bInEditMode = false;
		}
	}
	else
	{
		g_bInEditMode = true;
	}

#endif

	SetLaunchOptions( args );
	if ( !CL_HL2Demo_MapCheck( name ) )
	{
		Warning( "map load failed: %s not found or invalid\n", name );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( name ) )
	{
		Warning( "map load failed: %s not found or invalid\n", name );
		return;	
	}

#ifdef DEDICATED
	if ( sv.IsDedicated() )
#else
	// Stop demo loop
	GetBaseLocalClient().demonum = -1;
	if ( GetBaseLocalClient().m_nMaxClients == 0 || sv.IsDedicated() )
#endif
	{
		Host_Disconnect( false );	// stop old game

		HostState_NewGame( name, false, bBackground, bSplitScreenConnect );
	}

	if (args.ArgC() == 10)
	{
		if (V_stricmp(args[2], "setpos") == 0
			&& V_stricmp(args[6], "setang") == 0) 
		{
			Vector newpos;
			newpos.x = std::atof( args[3] );
			newpos.y = atof( args[4] );
			newpos.z = atof( args[5] );

			QAngle newangle;
			newangle.x = atof( args[7] );
			newangle.y = atof( args[8] );
			newangle.z = atof( args[9] );
			
			HostState_SetSpawnPoint(newpos, newangle);
		}
	}
}


// Handle a map command from the console.  Active clients are kicked off.
void Host_Map_f( const CCommand &args )
{
	Host_Map_Helper( args, (EMapFlags)0 );
}

// Handle a map group command from the console
void Host_MapGroup_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Host_MapGroup_f: No mapgroup specified\n" );
		return;
	}

	Msg( "Setting mapgroup to '%s'\n", args[1] );

	HostState_SetMapGroupName( args[1] );
}

// Handle smap command to connect multiple splitscreen users at once
void Host_SplitScreen_Map_f( const CCommand &args )
{
	Host_Map_Helper( args, EMAP_SPLITSCREEN );
}

//-----------------------------------------------------------------------------
// handle a map_edit <servername> command from the console. 
// Active clients are kicked off.
// UNDONE: protect this from use if not in dev. mode
//-----------------------------------------------------------------------------
#ifndef DEDICATED
static void map_edit( const CCommand &args );
static ConCommand map_edit_command( "map_edit", map_edit, "" );
static void map_edit( const CCommand &args )
{
	Host_Map_Helper( args, EMAP_EDIT_MODE );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Runs a map as the background
//-----------------------------------------------------------------------------
void Host_Map_Background_f( const CCommand &args )
{
	Host_Map_Helper( args, EMAP_BACKGROUND );
}




//-----------------------------------------------------------------------------
// Restarts the current server for a dead player
//-----------------------------------------------------------------------------
static void restart( const CCommand &args );
static ConCommand restart_command( "restart", restart, "Restart the game on the same level (add setpos to jump to current view position on restart)." );
static void restart( const CCommand &args )
{
	if ( 
#if !defined(DEDICATED)
		demoplayer->IsPlayingBack() || 
#endif
		!sv.IsActive() )
		return;

	if ( sv.IsMultiplayer() )
		return;

	bool bRememberLocation = ( args.ArgC() == 2 && !V_stricmp( args[1], "setpos" ) );

	bool bSplitScreenConnect = GET_NUM_SPLIT_SCREEN_PLAYERS() == 2 ;

	Host_Disconnect(false);	// stop old game

	if ( !CL_HL2Demo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	HostState_NewGame( sv.GetMapName(), bRememberLocation, false, bSplitScreenConnect );
}




//-----------------------------------------------------------------------------
// Purpose: Goes to a new map, taking all clients along
// Output : void Host_Changelevel_f
//-----------------------------------------------------------------------------
void Host_Changelevel_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "changelevel <levelname> : continue game on a new level\n" );
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "Can't changelevel, not running server\n" );
		return;
	}

	char mapname[MAX_PATH];
	V_StripExtension( args[ 1 ], mapname, sizeof( mapname ) );

	bool bMapMustExist = true;

	if ( bMapMustExist && !modelloader->Map_IsValid( mapname, true ) )
	{
		Host_Map_Helper_FuzzyName( args, mapname, sizeof( mapname ) );
		if ( !modelloader->Map_IsValid( mapname ) )
		{
			Warning( "changelevel failed: %s not found\n", mapname );
			return;
		}
	}

	if ( !CL_HL2Demo_MapCheck( mapname ) )
	{
		Warning( "changelevel failed: %s not found\n", mapname );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( mapname ) )
	{
		Warning( "changelevel failed: %s not found\n", mapname );
		return;	
	}

	SetLaunchOptions( args );

	HostState_ChangeLevelMP( mapname, args[2] );
}

//-----------------------------------------------------------------------------
// Purpose: Changing levels within a unit, uses save/restore
//-----------------------------------------------------------------------------
void Host_Changelevel2_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg ("changelevel2 <levelname> : continue game on a new level in the unit\n");
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "Can't changelevel2, not in a map\n" );
		return;
	}

	if ( !g_pVEngineServer->IsMapValid( args[1] ) )
	{
		if ( !CL_IsHL2Demo() || (CL_IsHL2Demo() && !(!V_stricmp( args[1], "d1_trainstation_03" ) || !V_stricmp( args[1], "d1_town_02a" ))) )	
		{
			Warning( "changelevel2 failed: %s not found\n", args[1] );
			return;
		}
	}

#if !defined(DEDICATED)
	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( CL_IsHL2Demo() && !sv.IsDedicated() && !V_stricmp( args[1], "d1_trainstation_03" ) ) 
	{
		void CL_DemoTransitionFromTrainstation();
		CL_DemoTransitionFromTrainstation();
		return; 
	}

	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( CL_IsHL2Demo() && !sv.IsDedicated() && !V_stricmp( args[1], "d1_town_02a" ) && !V_stricmp( args[2], "d1_town_02_02a" )) 
	{
		void CL_DemoTransitionFromRavenholm();
		CL_DemoTransitionFromRavenholm();
		return; 
	}

	if ( CL_IsPortalDemo() && !sv.IsDedicated() && !V_stricmp( args[1], "testchmb_a_07" ) ) 
	{
		void CL_DemoTransitionFromTestChmb();
		CL_DemoTransitionFromTestChmb();
		return; 
	}

#endif

	// allow a level transition to d1_trainstation_03 so the Host_Changelevel() can act on it
	if ( !CL_HL2Demo_MapCheck( args[1] ) ) 
	{
		Warning( "changelevel failed: %s not found\n", args[1] );
		return;	
	}

	SetLaunchOptions( args );

	HostState_ChangeLevelSP( args[1], args[2] );
}


// On PS/3, due to Matchmaking framework event architecture, Host_Disconnect is called recursively on quit.
// Bad things happen. Since it's not really necessary to disconnect recursively, we'll track and prevent it on PS/3 during shutdown.
int g_nHostDisconnectReentrancyCounter = 0;
class CDisconnectReentrancyCounter
{
public:
	CDisconnectReentrancyCounter() { g_nHostDisconnectReentrancyCounter++ ;}
	~CDisconnectReentrancyCounter() { g_nHostDisconnectReentrancyCounter-- ;}
};

//-----------------------------------------------------------------------------
// Purpose: Shut down client connection and any server
//-----------------------------------------------------------------------------
void Host_Disconnect( bool bShowMainMenu )
{


	IGameEvent *disconnectEvent = g_GameEventManager.CreateEvent( "cs_game_disconnected" );

	if ( disconnectEvent )
		g_GameEventManager.FireEventClientSide( disconnectEvent );

	CDisconnectReentrancyCounter autoReentrancyCounter;
	


	// Switch to single-threaded rendering during shutdown to
	// avoid synchronization issues between destructed objects
	// and the renderer
	Host_AllowQueuedMaterialSystem( false );

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetLocalClient().Disconnect( bShowMainMenu );
		}
	}
#endif

#if !defined( DEDICATED )
	if ( g_ClientDLL && bShowMainMenu )
	{
		// forcefully stop any of the full screen video panels used for loading or whatever
		// this is a safety precaution to ensure we don't orpan any of the hacky global video panels
		g_ClientDLL->ShutdownMovies();
	}
#endif 

	HostState_GameShutdown();

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( bShowMainMenu && !engineClient->IsDrawingLoadingImage() && ( GetBaseLocalClient().demonum == -1 ) )
		{


			EngineUI()->ActivateGameUI();
		}
	}
#endif
}
  
//-----------------------------------------------------------------------------
// Kill the client and any local server.
//-----------------------------------------------------------------------------
static void disconnect( const CCommand &args );
static ConCommand disconnect_command( "disconnect", disconnect, "Disconnect game from server.", FCVAR_SERVER_CAN_EXECUTE );
static void disconnect( const CCommand &args )
{
#ifndef DEDICATED
	GetBaseLocalClient().demonum = -1;
#endif

	if ( args.ArgC() > 1 )
	{
		COM_ExplainDisconnection( false, "%s", args[ 1 ] );
	}

	Host_Disconnect(true);
}

static void version( const CCommand &args );
static ConCommand version_command( "version", version, "Print version info string." );
static void version( const CCommand &args )
{
	ConMsg( "Protocol version %i [%i/%i]\nExe version %s (%s)\n", GetHostVersion(), GetServerVersion(), GetClientVersion(), Sys_GetVersionString(), Sys_GetProductString() );
	ConMsg( "Exe build: " __TIME__ " " __DATE__ " (%i)\n", build_number() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void pause( const CCommand &args );
static ConCommand pause_command( "pause", pause, "Toggle the server pause state." );
static void pause( const CCommand &args )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPausable() )
		return;

	// toggle paused state
	sv.SetPaused( !sv.IsPaused() );
	
	// send text message who paused the game
	if ( host_client )
		sv.BroadcastPrintf( "%s %s the game\n", host_client->GetClientName(), sv.IsPaused() ? "paused" : "unpaused" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void setpause( const CCommand &args );
static ConCommand setpause_command( "setpause", setpause, "Set the pause state of the server." );
static void setpause( const CCommand &args )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPausable() )
		return;

	sv.SetPaused( true );

	if ( !args.FindArg( "nomsg" ) )
	{
		// send text message who paused the game
		if ( host_client )
			sv.BroadcastPrintf( "%s paused the game\n", host_client->GetClientName() );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void unpause( const CCommand &args );
static ConCommand unpause_command( "unpause", unpause, "Unpause the game." );
static void unpause( const CCommand &args )
{
#if !defined( CLIENT_DLL )

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		if ( !GetBaseLocalClient().m_szLevelName[ 0 ] )
			return;
	}
#endif

	if ( !sv.IsPaused() )
		return;

	sv.SetPaused( false );

	if ( !args.FindArg( "nomsg" ) )
	{
		// send text messaage who unpaused the game
		if ( host_client )
			sv.BroadcastPrintf( "%s unpaused the game\n", host_client->GetClientName() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Kicks a user off of the server using their userid or uniqueid
//-----------------------------------------------------------------------------
static void kickid_ex( const CCommand &args );
static ConCommand kickid_ex_command( "kickid_ex", kickid_ex, "Kick a player by userid or uniqueid, provide a force-the-kick flag and also assign a message." );
static void kickid_ex( const CCommand &args )
{
	const char	*pszArg1 = nullptr, *pszMessage = nullptr;
	int			iSearchIndex = -1;
	char		szSearchString[128];
	int			argsStartNum = 1;
	bool		bPlayerID = false;
	bool		bForce = false;

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kickid_ex < userid | uniqueid > < force ( 0 / 1 ) > { message }\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( StringHasPrefix( pszArg1, PLAYERID_PREFIX ) && V_strstr( args[2], ":" ) )
		{
			V_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
			argsStartNum = 5;
			bPlayerID = true;
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			V_snprintf( szSearchString, sizeof( szSearchString ), "%s", pszArg1 );
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = V_atoi( pszArg1 );
	}

	// check for game type and game mode
	if ( args.ArgC() > argsStartNum )
	{
		if ( std::atoi( args[ argsStartNum + 1 ] ) == 1 )
		{
			bForce = true;
		}

		argsStartNum++;
	}

	// check for a message
	if ( args.ArgC() > argsStartNum )
	{
		int j;
		int dataLen = 0;

		pszMessage = args.ArgS();
		for ( j = 1; j <= argsStartNum; j++ )
		{
			dataLen += V_strlen( args[j] ) + 1; // +1 for the space between args
		}

		if ( bPlayerID )
		{
			dataLen -= 5; // SteamIDs don't have spaces between the args[) values
		}

		if ( dataLen > V_strlen( pszMessage ) ) // saftey check
		{
			pszMessage = nullptr;
		}
		else
		{
			pszMessage += dataLen;
		}
	}

	PerformKick( args.Source(), iSearchIndex, szSearchString, bForce, pszMessage );
}

//-----------------------------------------------------------------------------
// Kicks a user off of the server using their userid or uniqueid
//-----------------------------------------------------------------------------
static void kickid( const CCommand &args );
static ConCommand kickid_command( "kickid", kickid, "Kick a player by userid or uniqueid, with a message." );
static void kickid( const CCommand &args )
{
	const char	*pszArg1 = nullptr, *pszMessage = nullptr;
	int			iSearchIndex = -1;
	char		szSearchString[128];
	int			argsStartNum = 1;
	bool		bPlayerID = false;

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kickid < userid | uniqueid > { message }\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( StringHasPrefix( pszArg1, PLAYERID_PREFIX ) && V_strstr( args[2], ":" ) )
		{
			V_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
			argsStartNum = 5;
			bPlayerID = true;
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			V_snprintf( szSearchString, sizeof( szSearchString ), "%s", pszArg1 );
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = V_atoi( pszArg1 );
	}

	// check for a message
	if ( args.ArgC() > argsStartNum )
	{
		int j;
		int dataLen = 0;

		pszMessage = args.ArgS();
		for ( j = 1; j <= argsStartNum; j++ )
		{
			dataLen += V_strlen( args[j] ) + 1; // +1 for the space between args
		}

		if ( bPlayerID )
		{
			dataLen -= 5; // SteamIDs don't have spaces between the args[) values
		}

		if ( dataLen > V_strlen( pszMessage ) ) // saftey check
		{
			pszMessage = nullptr;
		}
		else
		{
			pszMessage += dataLen;
		}
	}

	PerformKick( args.Source(), iSearchIndex, szSearchString, false, pszMessage );
}

void PerformKick( cmd_source_t commandSource, int iSearchIndex, char* szSearchString, bool bForceKick, const char* pszMessage )
{
	IClient		*client = nullptr;
	const char	*who = "Console";

	// find this client
	int i;
	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.GetClient( i );

		if ( !client->IsConnected() )
		{
			continue;
		}

		// searching by UserID
		if ( iSearchIndex != -1 )
		{
			if ( client->GetUserID() == iSearchIndex )
			{
				// found!
				break;
			}
		}
		// searching by UniqueID
		else	
		{
			if ( V_stricmp( client->GetNetworkIDString(), szSearchString ) == 0 ) 
			{
				// found!
				break;
			}
		}
	}

	// now kick them
	if ( i < sv.GetClientCount() )
	{
		if ( client->IsSplitScreenUser() && client->GetSplitScreenOwner() )
		{
			client = client->GetSplitScreenOwner();
		}

		if ( commandSource == kCommandSrcNetClient )
		{
			who = host_client->m_Name;
		}

		if ( host_client == client && !sv.IsDedicated() && !bForceKick )
		{
			// can't kick yourself!
			return;
		}

		if ( iSearchIndex != -1 || !client->IsFakeClient() )
		{
			if ( pszMessage )
			{
				client->Disconnect( CFmtStr( "Kicked by %s : %s", who, pszMessage ) );
			}
			else
			{
				client->Disconnect( CFmtStr( "Kicked by %s", who ) );
			}
		}
	}
	else
	{
		if ( iSearchIndex != -1 )
		{
			ConMsg( "userid \"%d\" not found\n", iSearchIndex );
		}
		else
		{
			ConMsg( "uniqueid \"%s\" not found\n", szSearchString );			
		}
	}
}

/*
==================
Host_Kick_f

Kicks a user off of the server using their name
==================
*/
static void kick( const CCommand &args );
static ConCommand kick_command( "kick", kick, "Kick a player by name." );
static void kick( const CCommand &args )
{
	const char	*who = "Console";
	char		*pszName = nullptr;
	IClient		*client = nullptr;
	int			i = 0;
	char		name[64];

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kick < name >\n" );
		return;
	}

	// copy the name to a local buffer
	std::memset( name, 0, sizeof(name) );
	V_strncpy( name, args.ArgS(), sizeof(name) );
	pszName = name;

	// safety check
	if ( pszName && pszName[0] != 0 )
	{
		//HACK-HACK
		// check for the name surrounded by quotes (comes in this way from rcon)
		int len = V_strlen( pszName ) - 1; // (minus one since we start at 0)
		if ( pszName[0] == '"' && pszName[len] == '"' )
		{
			// get rid of the quotes at the beginning and end
			pszName[len] = 0;
			pszName++;
		}

		for ( i = 0; i < sv.GetClientCount(); i++ )
		{
			client = sv.GetClient(i);

			if ( !client->IsConnected() )
				continue;

			// found!
			if ( V_strcasecmp( client->GetClientName(), pszName ) == 0 ) 
				break;
		}

		// now kick them
		if ( i < sv.GetClientCount() )
		{
			if ( client->IsSplitScreenUser() && client->GetSplitScreenOwner() )
			{
				client = client->GetSplitScreenOwner();
			}

			if ( args.Source() == kCommandSrcNetClient )
			{
				who = host_client->m_Name;
			}

			// can't kick yourself!
			if ( host_client == client && !sv.IsDedicated() )
				return;

			client->Disconnect( CFmtStr( "Kicked by %s", who ) );
		}
		else
		{
			ConMsg( "Can't kick \"%s\", name not found\n", pszName );
		}
	}
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

void Host_PrintMemoryStatus( const char *mapname )
{
	const float MB = 1.0f / ( 1024*1024 );
	contract_assert( mapname );
	const ks::platform::heap_info memstats = ks::platform::heap_usage();
	Msg( "[MEMORYSTATUS] [%s] Operating system reports sbrk size: %.2f MB, Used: %.2f MB, #mallocs = %lu\n",
		mapname, MB*memstats.arena, MB*memstats.in_use, memstats.mapped_blocks );

	{
		g_pMemAlloc->SetStatsExtraInfo( mapname, "" );
	}

	int nTotal = g_pMemAlloc->GetSize( 0 );
	if (nTotal == -1)
	{
		Msg( "Internal heap corrupted!\n" );
	}
	else
	{
		Msg( "Internal heap reports: %5.2f MB (%d bytes)\n", nTotal/(1024.0f*1024.0f), nTotal );
	}

	Msg( "\nHunk Memory Used:\n" );
	Hunk_Print();

	Msg( "\nDatacache reports:\n" );
	g_pDataCache->OutputReport( DC_SUMMARY_REPORT, nullptr );
}

//-----------------------------------------------------------------------------
// Dump memory stats
//-----------------------------------------------------------------------------
static void memory( const CCommand &args );
static ConCommand memory_command( "memory", memory, "Print memory stats." );
static void memory( const CCommand &args )
{
	ConMsg( "Heap Used:\n" );
	int nTotal = g_pMemAlloc->GetSize( 0 );
	if (nTotal == -1)
	{
		ConMsg( "Corrupted!\n" );
	}
	else
	{
		ConMsg( "%5.2f MB (%d bytes)\n", nTotal/(1024.0f*1024.0f), nTotal );
	}

	ConMsg("\nVideo Memory Used:\n");
	CVProfile *pProf = &g_VProfCurrentProfile;
	int prefixLen = V_strlen( "TexGroup_Global_" );
	float total = 0.0f;
	for ( int i=0; i < pProf->GetNumCounters(); i++ )
	{
		if ( pProf->GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
		{
			float value = pProf->GetCounterValue( i ) * (1.0f/(1024.0f*1024.0f) );
			total += value;
			const char *pName = pProf->GetCounterName( i );
			if ( StringHasPrefix( pName, "TexGroup_Global_" ) )
			{
				pName += prefixLen;
			}
			ConMsg( "%5.2f MB: %s\n", value, pName );
		}
	}
	ConMsg("------------------\n");
	ConMsg( "%5.2f MB: total\n", total );

	ConMsg( "\nHunk Memory Used:\n" );
	Hunk_Print();
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


#ifndef DEDICATED

//MOTODO move all demo commands to demoplayer


//-----------------------------------------------------------------------------
// Purpose: Gets number of valid demo names
// Output : int
//-----------------------------------------------------------------------------
int Host_GetNumDemos()
{
	int c = 0;
#ifndef DEDICATED
	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = GetBaseLocalClient().demos[ i ];
		if ( !demoname[ 0 ] )
			break;

		++c;
	}
#endif
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_PrintDemoList()
{
	int count = Host_GetNumDemos();

#ifndef DEDICATED
	int next = GetBaseLocalClient().demonum;
	if ( next >= count || next < 0 )
	{
		next = 0;
	}

	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = GetBaseLocalClient().demos[ i ];
		if ( !demoname[ 0 ] )
			break;

		bool isnextdemo = next == i ? true : false;

		DevMsg( "%3s % 2i : %20s\n", isnextdemo ? "-->" : "   ", i, GetBaseLocalClient().demos[ i ] );
	}
#endif

	if ( !count )
	{
		DevMsg( "No demos in list, use startdemos <demoname> <demoname2> to specify\n" );
	}
}


#ifndef DEDICATED
//-----------------------------------------------------------------------------
//
// Con commands related to demos, not available on dedicated servers
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Specify list of demos for the "demos" command
//-----------------------------------------------------------------------------
static void startdemos( const CCommand &args );
static ConCommand startdemos_command( "startdemos", startdemos, "Play demos in demo sequence." );
static void startdemos( const CCommand &args )
{
	int	c = args.ArgC() - 1;
	if (c > MAX_DEMOS)
	{
		Msg ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Msg ("%i demo(s) in loop\n", c);

	for ( int i=1 ; i<c+1 ; i++ )
	{
		V_strncpy( GetBaseLocalClient().demos[i-1], args[i], sizeof(GetBaseLocalClient().demos[0]) );
	}

	GetBaseLocalClient().demonum = 0;

	Host_PrintDemoList();

	if ( !sv.IsActive() && !demoplayer->IsPlayingBack() )
	{
		CL_NextDemo ();
	}
	else
	{
		GetBaseLocalClient().demonum = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return to looping demos, optional resume demo index
//-----------------------------------------------------------------------------
static void demos( const CCommand &args );
static ConCommand demos_command( "demos", demos, "Demo demo file sequence." );
static void demos( const CCommand &args )
{
	CClientState &cl = GetBaseLocalClient();
	int oldn = cl.demonum;
	cl.demonum = -1;
	Host_Disconnect(false);
	cl.demonum = oldn;

	if (cl.demonum == -1)
		cl.demonum = 0;

	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			cl.demonum = clamp( V_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", cl.demos[ cl.demonum ] );
		}
	}

	Host_PrintDemoList();

	CL_NextDemo ();
}

//-----------------------------------------------------------------------------
// Purpose: Stop current demo
//-----------------------------------------------------------------------------
static void stopdemo( const CCommand &args );
static ConCommand stopdemo_command( "stopdemo", stopdemo, "Stop playing back a demo.", FCVAR_DONTRECORD );
static void stopdemo( const CCommand &args )
{
	if ( !demoplayer->IsPlayingBack() )
		return;
	
	Host_Disconnect (true);
}

//-----------------------------------------------------------------------------
// Purpose: Skip to next demo
//-----------------------------------------------------------------------------
static void nextdemo( const CCommand &args );
static ConCommand nextdemo_command( "nextdemo", nextdemo, "Play next demo in sequence." );
static void nextdemo( const CCommand &args )
{
	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			GetBaseLocalClient().demonum = clamp( V_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", GetBaseLocalClient().demos[ GetBaseLocalClient().demonum ] );
		}
	}
	Host_EndGame( false, "Moving to next demo..." );
}

//-----------------------------------------------------------------------------
// Purpose: Print out the current demo play order
//-----------------------------------------------------------------------------
static void demolist( const CCommand &args );
static ConCommand demolist_command( "demolist", demolist, "Print demo sequence list." );
static void demolist( const CCommand &args )
{
	Host_PrintDemoList();
}


//-----------------------------------------------------------------------------
// Purpose: Host_Soundfade_f
//-----------------------------------------------------------------------------
static void soundfade( const CCommand &args );
static ConCommand soundfade_command( "soundfade", soundfade, "Fade client volume.", FCVAR_SERVER_CAN_EXECUTE );
static void soundfade( const CCommand &args )
{
	float percent;
	float inTime, holdTime, outTime;

	if (args.ArgC() != 3 && args.ArgC() != 5)
	{
		Msg("soundfade <percent> <hold> [<out> <int>]\n");
		return;
	}

	percent = clamp( std::atof(args[1]), 0.0f, 100.0f );
	
	holdTime = std::max<std::common_type_t<float, double>>( 0.0f, atof(args[2]) );

	inTime = 0.0f;
	outTime = 0.0f;
	if (args.ArgC() == 5)
	{
		outTime = std::max<std::common_type_t<float, double>>( 0.0f, atof(args[3]) );
		inTime = std::max<std::common_type_t<float, double>>( 0.0f, atof( args[4]) );
	}

	S_SoundFade( percent, holdTime, outTime, inTime );
}

#endif // !DEDICATED

#endif


//-----------------------------------------------------------------------------
// Shutdown the server
//-----------------------------------------------------------------------------
static void killserver( const CCommand &args );
static ConCommand killserver_command( "killserver", killserver, "Shutdown the server." );
static void killserver( const CCommand &args )
{
	Host_Disconnect(true);
	
	if ( !sv.IsDedicated() )
	{
		// close network sockets and reopen if multiplayer game
		NET_SetMultiplayer( false );
		NET_SetMultiplayer( !!( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SETTING_MULTIPLAYER ) );
	}
}

// [hpe:jason] Enable ENGINE_VOICE for Cstrike 1.5, all platforms
	ConVar voice_vox( "voice_vox", "false", FCVAR_DEVELOPMENTONLY ); // Controls open microphone (no push to talk) settings
	#undef NO_ENGINE_VOICE

#ifdef NO_ENGINE_VOICE
ConVar voice_ptt( "voice_ptt", "-1.0", FCVAR_DEVELOPMENTONLY ); // Time when ptt key was released, 0 means to keep transmitting voice
#endif

#if !defined(DEDICATED)
void Host_VoiceRecordStart_f(void)
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( 0 );
#else
	ConVarRef voice_vox( "voice_vox" );

	if ( voice_vox.GetBool() == true )
		return;

	int iSsSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( GetLocalClient( iSsSlot ).IsActive() )
	{
		const char *pUncompressedFile = nullptr;
		const char *pDecompressedFile = nullptr;
		const char *pInputFile = nullptr;

		if (voice_recordtofile.GetInt())
		{
			pUncompressedFile = "voice_micdata.wav";
			pDecompressedFile = "voice_decompressed.wav";
		}

		if (voice_inputfromfile.GetInt())
		{
			pInputFile = "voice_input.wav";
		}
		if (Voice_RecordStart(pUncompressedFile, pDecompressedFile, pInputFile))
		{
		}
	}
#endif // #ifndef NO_ENGINE_VOICE
}

void Host_VoiceRecordStop_f( const CCommand &args )
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( (float) Plat_FloatTime() );
#else
	ConVarRef voice_vox( "voice_vox" );

	if ( voice_vox.GetBool() == true )
		return;

	int iSsSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( GetLocalClient( iSsSlot ).IsActive() )
	{
		if (Voice_IsRecording())
		{
			CL_SendVoicePacket(true);
			Voice_RecordStop();
		}

		if ( args.ArgC() == 2 && V_strcasecmp( args[1], "force" ) == 0 )
		{
			// do nothing
		}
		else
		{

			voice_vox.SetValue( 0 );
		}
	}
#endif // #ifndef NO_ENGINE_VOICE
}
#endif

// TERROR: adding a toggle for voice
void Host_VoiceToggle_f( const CCommand &args )
{
#ifdef NO_ENGINE_VOICE
	voice_ptt.SetValue( (float) ( voice_ptt.GetFloat() ? 0.0f : Plat_FloatTime() ) );
#endif
#if !defined( DEDICATED ) && !defined( NO_ENGINE_VOICE )
	if ( GetBaseLocalClient().IsActive() )
	{
		bool bToggle = false;

		if ( args.ArgC() == 2 && V_strcasecmp( args[1], "on" ) == 0 )
		{
			bToggle = true;
		}

		if ( Voice_IsRecording() && bToggle == false )
		{
			CL_SendVoicePacket(true);
			Voice_RecordStop();
		}
		else if ( bToggle == true && Voice_IsRecording() == false )
		{
			const char *pUncompressedFile = nullptr;
			const char *pDecompressedFile = nullptr;
			const char *pInputFile = nullptr;

			if (voice_recordtofile.GetInt())
			{
				pUncompressedFile = "voice_micdata.wav";
				pDecompressedFile = "voice_decompressed.wav";
			}

			if (voice_inputfromfile.GetInt())
			{
				pInputFile = "voice_input.wav";
			}

			Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper for modelloader->Print() function call
//-----------------------------------------------------------------------------
static void listmodels( const CCommand &args );
static ConCommand listmodels_command( "listmodels", listmodels, "List loaded models." );
static void listmodels( const CCommand &args )
{
	modelloader->Print();
}

/*
==================
Host_IncrementCVar
==================
*/
static void incrementvar( const CCommand &args );
static ConCommand incrementvar_command( "incrementvar", incrementvar, "Increment specified convar value.", FCVAR_DONTRECORD );
static void incrementvar( const CCommand &args )
{
	if( args.ArgC() != 5 )
	{
		Warning( "Usage: incrementvar varName minValue maxValue delta\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "Host_IncrementCVar_f without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = std::atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float delta = atof( args[ 4 ] );
	float newValue = currentValue + delta;
	if( newValue > endValue )
	{
		newValue = startValue;
	}
	else if ( newValue < startValue )
	{
		newValue = endValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Host_MultiplyCVar_f
//-----------------------------------------------------------------------------
static void multvar( const CCommand &args );
static ConCommand multvar_command( "multvar", multvar, "Multiply specified convar value.", FCVAR_DONTRECORD );
static void multvar( const CCommand &args )
{
	if (( args.ArgC() != 5 ))
	{
		Warning( "Usage: multvar varName minValue maxValue factor\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "multvar without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = std::atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float factor = atof( args[ 4 ] );
	float newValue = currentValue * factor;
	if( newValue > endValue )
	{
		newValue = endValue;
	}
	else if ( newValue < startValue )
	{
		newValue = startValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void dumpstringtables( const CCommand &args );
static ConCommand dumpstringtables_command( "dumpstringtables", dumpstringtables, "Print string tables to console." );
static void dumpstringtables( const CCommand &args )
{
	SV_PrintStringTables();
#ifndef DEDICATED
	CL_PrintStringTables();
#endif
}

static void stringtabledictionary( const CCommand &args );
static ConCommand stringtabledictionary_command( "stringtabledictionary", stringtabledictionary, "Create dictionary for current strings." );
static void stringtabledictionary( const CCommand &args )
{
	if ( !sv.IsActive() )
	{
		Warning( "stringtabledictionary: only valid when running a map\n" );
		return;
	}

	SV_CreateDictionary( sv.GetMapName() );
}

// Register shared commands
static void quit( const CCommand &args );
static ConCommand quit_command( "quit", quit, "Exit the engine.", FCVAR_NONE );
static void quit( const CCommand &args )
{ 
	if ( args.ArgC() > 1 && V_strcmp( args[ 1 ], "prompt" ) == 0 )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit_prompt" );
		return;
	}

	Host_Quit_f();
}

static ConCommand cmd_exit("exit", Host_Quit_f, "Exit the engine.");

#ifndef DEDICATED
#ifdef VOICE_OVER_IP
static ConCommand startvoicerecord("+voicerecord", Host_VoiceRecordStart_f);
static ConCommand endvoicerecord("-voicerecord", Host_VoiceRecordStop_f);
static ConCommand togglevoicerecord("voicerecord_toggle", Host_VoiceToggle_f);
#endif // VOICE_OVER_IP

#endif

//-----------------------------------------------------------------------------
// Purpose: Force a null pointer crash. useful for testing minidumps
//-----------------------------------------------------------------------------
static void crash( const CCommand &args );
static ConCommand crash_command( "crash", crash, "Cause the engine to crash (Debug!!)", FCVAR_CHEAT );
static void crash( const CCommand &args )
{ 
	Msg( "forcing crash\n" );
	char *p = 0;
	*p = 0;
}

static void spincycle( const CCommand &args );
static ConCommand spincycle_command( "spincycle", spincycle, "Cause the engine to spincycle (Debug!!)", FCVAR_CHEAT );
static void spincycle( const CCommand &args )
{ 
	if ( args.ArgC() > 1 )
	{
		const char *pParam = args.Arg( 1 );
		if ( pParam && *pParam && pParam[ V_strlen( pParam ) - 1 ] == 's' )
		{
			float flSeconds = V_atof( pParam );
			if ( flSeconds > 0 )
			{
				Msg( "Sleeping for %.3f seconds\n", flSeconds );
				ThreadSleep( flSeconds * 1000 );
				return;
			}
		}
	}

	int numCycles = ( args.ArgC() > 1 ) ? V_atoi( args.Arg(1) ) : 10;
	Msg( "forcing spincycle for %d cycles\n", numCycles );
	for( int k = 0; k < numCycles; ++ k )
	{
		( void ) RandomInt( 0, numCycles );
	}
}

static void forktest( const CCommand &args );
static ConCommand forktest_command( "forktest", forktest, "Cause the engine to fork and wait for child PID, parameter can be passed for requested exit code (Debug!!)", FCVAR_CHEAT );
static void forktest( const CCommand &args )
{
	EndWatchdogTimer();	// End the watchdog in case child takes too long

	int nExitCodeRequested = ( args.ArgC() > 1 ) ? V_atoi( args.Arg(1) ) : 0;
	
	long long pID = ks::platform::fork_process();
	Msg( "forktest: Forked, pID = %d\n", (int) pID );
	if ( pID == 0 )  // are we the forked child?
	{
		//
		// Close every inherited descriptor that is not #0 (stdin), #1 (stdout), #2 (stderr)
		// This will close all sockets and file handles that can result in process hanging
		// when network events occur on the machine and make NFS handles go bad.
		//
		if ( !CommandLine()->FindParm( "-forkfdskeepall" ) )
		{
			ks::platform::detach_inherited_descriptors( !CommandLine()->FindParm( "-forkfdskeepstd" ) );
		}

		Msg( "Child finished successfully!\n" );
		ks::platform::exit_process_now( nExitCodeRequested );		// don't do a normal c++ exit, don't want to call destructors, etc.
	}
	else
	{
		ks::platform::child_status status{};
		bool bReaped = ks::platform::reap_child( pID, true, &status );
		Msg( "Parent finished wait: %d, exit: %d, code: %d\n", bReaped ? (int)status.pid : -1, (int)status.exited, status.code );
	}
}

static void flush( const CCommand &args );
static ConCommand flush_command( "flush", flush, "Flush unlocked cache memory.", FCVAR_CHEAT );
static void flush( const CCommand &args )
{
#if !defined( DEDICATED )
	g_ClientDLL->InvalidateMdlCache();
#endif // DEDICATED
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( true );
#if !defined( DEDICATED )
	wavedatacache->Flush();
#endif
}

static void flush_locked( const CCommand &args );
static ConCommand flush_locked_command( "flush_locked", flush_locked, "Flush unlocked and locked cache memory.", FCVAR_CHEAT );
static void flush_locked( const CCommand &args )
{
#if !defined( DEDICATED )
	g_ClientDLL->InvalidateMdlCache();
#endif // DEDICATED
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( false );
#if !defined( DEDICATED )
	wavedatacache->Flush();
#endif
}

static void cache_print( const CCommand &args );
static ConCommand cache_print_command( "cache_print", cache_print, "cache_print [section]\nPrint out contents of cache memory." );
static void cache_print( const CCommand &args )
{
	const char *pszSection = nullptr;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT, pszSection );
}

static void cache_print_lru( const CCommand &args );
static ConCommand cache_print_lru_command( "cache_print_lru", cache_print_lru, "cache_print_lru [section]\nPrint out contents of cache memory." );
static void cache_print_lru( const CCommand &args )
{
	const char *pszSection = nullptr;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT_LRU, pszSection );
}

static void cache_print_summary( const CCommand &args );
static ConCommand cache_print_summary_command( "cache_print_summary", cache_print_summary, "cache_print_summary [section]\nPrint out a summary contents of cache memory." );
static void cache_print_summary( const CCommand &args )
{
	const char *pszSection = nullptr;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_SUMMARY_REPORT, pszSection );
}


#ifndef DEDICATED
// NOTE: As of shipping the 360 version of L4D, this command will not work correctly. See changelist 612757 (terror src) for why.
static void ss_connect( const CCommand &args );
static ConCommand ss_connect_command( "ss_connect", ss_connect, "If connected with available split screen slots, connects a split screen player to this machine.", FCVAR_DEVELOPMENTONLY );
static void ss_connect( const CCommand &args )
{
	if ( host_state.max_splitscreen_players == 1 )
	{
		Msg( "Can't ss_connect, game does not support split screen.\n" );
		return;
	}

	if ( !GetBaseLocalClient().IsConnected() )
	{
		Msg( "Can't ss_connect, not connected to game.\n" );
		return;
	}
	

	int nSlot = 1;
#ifndef DEDICATED
	while ( splitscreen->IsValidSplitScreenSlot( nSlot ) )
	{
		++nSlot;
	}
#endif

	if ( nSlot >= host_state.max_splitscreen_players )
	{
		Msg( "Can't ss_connect, no more split screen player slots!\n" );
		return;
	}

	// Grab convars for next available slot
	CCLCMsg_SplitPlayerConnect_t msg;
	Host_BuildUserInfoUpdateMessage( nSlot, &msg.convars.mut(), false );

	GetBaseLocalClient().m_NetChannel->SendNetMsg( msg );
}


static void ss_disconnect( const CCommand &args );
static ConCommand ss_disconnect_command( "ss_disconnect", ss_disconnect, "If connected with available split screen slots, connects a split screen player to this machine.", FCVAR_DEVELOPMENTONLY );
static void ss_disconnect( const CCommand &args )
{
	if ( args.Source() == kCommandSrcNetClient )
	{
#ifndef DEDICATED
		host_client->SplitScreenDisconnect( args );
#endif
		return;
	}

	// Get the first valid slot
	int nSlot = -1;
	for ( int i = 1; i < host_state.max_splitscreen_players; ++i )
	{
		if ( IS_VALID_SPLIT_SCREEN_SLOT( i ) )
		{
			nSlot = i;
			break;

		}
	}

	if ( args.ArgC() > 1 )
	{
		int cmdslot = V_atoi( args.Arg( 1 ) );
		if ( IS_VALID_SPLIT_SCREEN_SLOT( cmdslot ) )
		{
			nSlot = cmdslot;
		}
		else
		{
			Msg( "Can't ss_disconnect, slot %d not active\n", cmdslot );
			return;
		}
	}

	if ( ! IS_VALID_SPLIT_SCREEN_SLOT( nSlot ) )
	{
		Msg( "Can't ss_disconnect, no split screen users active\n" );
		return;
	}

	char buf[ 256 ];
	V_snprintf( buf, sizeof( buf ), "ss_disconnect %d\n", nSlot );

	CCommand argsClient;
	argsClient.Tokenize( buf, kCommandSrcCode );
	Cmd_ForwardToServer( argsClient );
#ifndef DEDICATED
	splitscreen->SetDisconnecting( nSlot, true );
#endif
}

#endif
