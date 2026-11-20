// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



import std;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.minidump;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iappsystem;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.registry;
import ks.tier1.strtools;

	#include "appframework/ilaunchermgr.h"

#include "quakedef.h"
#include "idedicatedexports.h"
#include "engine_launcher_api.h"
#include "ivideomode.h"
#include "common.h"
#include "iregistry.h"
#include "keys.h"
#include "cdll_engine_int.h"
#include "traceinit.h"
#include "iengine.h"
#include "igame.h"
#include "engine_hlds_api.h"
#include "filesystem_engine.h"
#include "cl_main.h"
#include "client.h"
#include "tier3/tier3.h"
#include "MapReslistGenerator.h"
#include "DevShotGenerator.h"
#include "gl_shader.h"
#include "l_studio.h"
#include "sys_dll.h"
#include "materialsystem/materialsystem_config.h"
#include "server.h"
#include "datacache/idatacache.h"
#include "vphysics_interface.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/IAppSystemGroup.h"

// This is here just for legacy support of older .dlls!!!
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "eiface.h"
#include "matchmaking/imatchframework.h"
#include "profile.h"
#include "status.h"


#ifndef DEDICATED
#include "sys_mainwind.h"
#include "IGameUIFuncs.h"

#endif // DEDICATED

#include "rocketui/rocketui.h"







//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
IDedicatedExports *dedicated = nullptr;
extern CreateInterfaceFn g_AppSystemFactory;
IPhysics *g_pPhysics = nullptr;

#ifndef DEDICATED
extern CreateInterfaceFn g_ClientFactory;
#endif

bool g_bRunningFromPerforce;

CreateInterfaceFn g_pfnMatchmakingFactory = nullptr;

IMatchFramework *g_pIfaceMatchFramework = nullptr;
bool s_bIsDedicatedServer = false;

// For the libraries that behave differently without a client (particles).
bool UTIL_IsDedicatedServer( void )
{
	return s_bIsDedicatedServer;
}

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Host_GetHostInfo(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
const char *Key_BindingForKey( ButtonCode_t code );
void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );
void Host_ReadPreStartupConfiguration();
void EditorToggle_f();

void *pmainwindow = nullptr;

//-----------------------------------------------------------------------------
// ConVars and console commands
//-----------------------------------------------------------------------------
#if !defined(DEDICATED)
static ConCommand editor_toggle( "editor_toggle", EditorToggle_f, "Disables the simulation and returns focus to the editor", FCVAR_CHEAT );
#endif



#ifndef DEDICATED
//-----------------------------------------------------------------------------
// Purpose: exports an interface that can be used by the launcher to run the engine
//			this is the exported function when compiled as a blob
//-----------------------------------------------------------------------------
void F( IEngineAPI **api )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary to prevent the LTCG compiler from crashing.
	*api = ( IEngineAPI * )(factory(VENGINE_LAUNCHER_API_VERSION, nullptr));
}
#endif // DEDICATED

extern bool cs_initialized;
extern int			lowshift;
static const char	*empty_string = "";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
extern void SCR_UpdateScreen(void);
extern bool g_bMajorMapChange; 
extern bool g_bPrintingKeepAliveDots;

void Sys_ShowProgressTicks(char* specialProgressMsg)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClearIOStates( void )
{
#ifndef DEDICATED
	if ( g_ClientDLL ) 
	{
		g_ClientDLL->IN_ClearStates();
	}
#endif
}

void MoveConsoleWindowToFront()
{
}

std::vector< char > g_TextModeLine;

char NextGetch()
{
	return -1;
	// NOTE: for some reason, kbhit() KILLS performance on the client.. when using it, the client
	// goes so slow that it's player's motion is all jerky. If we need input, probably the
	// best thing to do is to hook the console window's wndproc and get the keydown messages.
	/*
	// Sort of hacky to overload the gamemsg loop with these messages, but it does the trick.
	if ( VCRGetMode() == VCR_Playback )
	{
		unsigned int uMsg, wParam;
		long lParam;
		if ( VCRHook_PlaybackGameMsg( uMsg, wParam, lParam ) )
		{
			Assert( uMsg == 0xFFFF );
			return (char)wParam;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		if ( kbhit() )
		{
			char ch = getch();
			VCRHook_RecordGameMsg( 0xFFFF, ch, 0 );
			return ch;
		}
		else
		{
			VCRHook_RecordEndGameMsg();
			return -1;
		}
	}
	*/
}

void EatTextModeKeyPresses()
{
	if ( !g_bTextMode )
		return;
	
	static bool bFirstRun = true;
	if ( bFirstRun )
	{
		bFirstRun = false;
		MoveConsoleWindowToFront();
	}

	char ch;
	while ( (ch = NextGetch()) != -1 )
	{
		if ( ch == 8 )
		{
			// Backspace..
			if ( !g_TextModeLine.empty() )
			{
				g_TextModeLine.erase( g_TextModeLine.begin() + ( std::ssize( g_TextModeLine ) - 1 ) );
			}
		}
		else if ( ch == '\r' )
		{
			// Finish the line.
			if ( !g_TextModeLine.empty() )
			{
				g_TextModeLine.insert( g_TextModeLine.end(), ( "\n" ), ( "\n" ) + 2 );
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), g_TextModeLine.data() );
				g_TextModeLine.clear(); g_TextModeLine.shrink_to_fit();
			}
			std::printf( "\n" );
		}
		else
		{
			g_TextModeLine.push_back( ch );
		}

		std::printf( "%c", ch );
	}	
}


//-----------------------------------------------------------------------------
// The SDK launches the game with the full path to gameinfo.txt, so we need
// to strip off the path.
//-----------------------------------------------------------------------------
const char *GetModDirFromPath( const char *pszPath )
{
	char *pszSlash = V_strrchr( pszPath, '\\' );
	if ( pszSlash )
	{
		return pszSlash + 1;
	}
	else if ( ( pszSlash  = V_strrchr( pszPath, '/' ) ) != nullptr )
	{
		return pszSlash + 1;
	}

	// Must just be a mod directory already.
	return pszPath;
}

//-----------------------------------------------------------------------------
// Purpose: Main entry
//-----------------------------------------------------------------------------
#ifndef DEDICATED
#include "gl_matsysiface.h"
#endif

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems
//-----------------------------------------------------------------------------
class CModAppSystemGroup : public CAppSystemGroup
{
	typedef CAppSystemGroup BaseClass;
public:
	// constructor
	CModAppSystemGroup( bool bServerOnly, CAppSystemGroup *pParentAppSystem = nullptr )
		: BaseClass( pParentAppSystem ),
		m_bServerOnly( bServerOnly )
	{
	}

	CreateInterfaceFn GetFactory()
	{
		return CAppSystemGroup::GetFactory();
	}

	// Methods of IApplication
	bool Create();
	bool PreInit();
	int Main();
	void PostShutdown();
	void Destroy();

private:

	bool IsServerOnly() const
	{
		return m_bServerOnly;
	}
	bool ModuleAlreadyInList( std::vector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName );

	bool AddLegacySystems();
	bool	m_bServerOnly;
};

#ifndef DEDICATED
//-----------------------------------------------------------------------------
//
// Main engine interface exposed to launcher
//
//-----------------------------------------------------------------------------
class CEngineAPI : public CTier3AppSystem< IEngineAPI >
{
	typedef CTier3AppSystem< IEngineAPI > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// This function must be called before init
	virtual bool SetStartupInfo( StartupInfo_t &info );

	virtual int Run( );

	// Sets the engine to run in a particular editor window
	virtual void SetEngineWindow( void *hWnd );

	// Posts a console command
	virtual void PostConsoleCommand( const char *pConsoleCommand );

	// Are we running the simulation?
	virtual bool IsRunningSimulation( ) const;

	// Start/stop running the simulation
	virtual void ActivateSimulation( bool bActive );

	// Reset the map we're on
	virtual void SetMap( const char *pMapName );

	bool MainLoop();

private:
	int RunListenServer();

	// Hooks a particular mod up to the registry
	void SetRegistryMod( const char *pModName );

	// One-time setup, based on the initially selected mod
	// FIXME: This should move into the launcher!
	bool OnStartup( void *pInstance, const char *pStartupModName );
	void OnShutdown();

	// Initialization, shutdown of a mod.
	bool ModInit( const char *pModName, const char *pGameDir );
	void ModShutdown();

	// Initializes, shuts down the registry
	bool InitRegistry( const char *pModName );
	void ShutdownRegistry();

	// Handles there being an error setting up the video mode
	InitReturnVal_t HandleSetModeError();

	// Purpose: Message pump when running stand-alone
	void PumpMessages();

	// Purpose: Message pump when running with the editor
	void PumpMessagesEditMode( bool &bIdle, long &lIdleCount );

	// Activate/deactivates edit mode shaders
	void ActivateEditModeShaders( bool bActive );

private:
	void *m_hEditorHWnd;
	bool m_bRunningSimulation;
	StartupInfo_t m_StartupInfo;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CEngineAPI s_EngineAPI;
static InterfaceReg s_Reg_CEngineAPI_IEngineAPI( []() -> void * { return static_cast<IEngineAPI *>( &s_EngineAPI ); }, VENGINE_LAUNCHER_API_VERSION );;


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CEngineAPI::Connect( CreateInterfaceFn factory ) 
{ 
	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pFileSystem = g_pFullFileSystem;
	if ( !g_pFileSystem )
		return false;

	g_pFileSystem->SetWarningFunc( Warning );

	if ( !Shader_Connect( true ) )
		return false;

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, nullptr );

	g_pSoundEmitterSystem = (ISoundEmitterSystemBase *)factory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, nullptr);
	
	g_pRocketUI = ( IRocketUI* ) factory( ROCKETUI_INTERFACE_VERSION, nullptr );


	if ( !g_pStudioRender || !g_pDataCache || !g_pPhysics || !g_pMDLCache || !g_pInputSystem || !g_pSoundEmitterSystem)
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	if (!g_pStudioRender)
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, nullptr );
	
	ConnectMDLCacheNotify();

	return true; 
}

void CEngineAPI::Disconnect()
{
	DisconnectMDLCacheNotify();

	g_pPhysics = nullptr;
	g_pSoundEmitterSystem = nullptr;

	Shader_Disconnect();

	g_pFileSystem = nullptr;

	BaseClass::Disconnect();

	g_AppSystemFactory = nullptr;
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CEngineAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, nullptr );				// to prevent the LTCG compiler from crashing.
}


static bool WantsFullMemoryDumps()
{
	return false;
}


//-----------------------------------------------------------------------------
// Sets startup info
//-----------------------------------------------------------------------------
bool CEngineAPI::SetStartupInfo( StartupInfo_t &info ) 
{
	g_bTextMode = info.m_bTextMode;

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>( info.m_pBaseDirectory );

	// Copy off all the startup info
	m_StartupInfo = info;


	// Needs to be done prior to init material system config
	TRACEINIT( COM_InitFilesystem( m_StartupInfo.m_pInitialMod ), COM_ShutdownFileSystem() );

	//
	// VPK content-shadowing overrides
	// See below:
	//		CDedicatedServerAPI::ModInit
	//	to ensure that dedicated servers also mount the VPKs to check clients CRCs
	//

	// This is the client initializing, if we are running in lowviolence mode then inject the lowviolence VPK at the head of search path
	if ( CommandLine()->FindParm( "-pakxv_lowviolence" ) )
	{
		g_pFullFileSystem->AddSearchPath( "lowviolence", "COMPAT:GAME", PATH_ADD_TO_HEAD );
	}

	if ( CommandLine()->FindParm( "-perfectworld" ) )
	{
		g_pFullFileSystem->AddSearchPath( "perfectworld", "COMPAT:GAME", PATH_ADD_TO_HEAD );
	}

	// Enable file tracking - client always does this in case it connects to a pure server.
	// server only does this if sv_pure is set
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		// If it's not singleplayer_only
		if ( V_stricmp( modinfo->GetString("type", "singleplayer_only"), "singleplayer_only") == 0 )
		{
			DevMsg( "Disabling whitelist file tracking in filesystem...\n" );
			g_pFileSystem->EnableWhitelistFileTracking( false, false, false );
		}
		else
		{
			DevMsg( "Enabling whitelist file tracking in filesystem...\n" );
			g_pFileSystem->EnableWhitelistFileTracking( true, false, false );
		}
	}
	modinfo->deleteThis();

	//
	// Configure breakpad
	//

	extern void Sys_Version( bool bDedicated );
	Sys_Version( false );

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::Init() 
{
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bRunningSimulation = false;

	// Initialize the FPU control word

	SetupFPUControlWord();

	// This creates the videomode singleton object, it doesn't depend on the registry
	VideoMode_Create();

	// Initialize the editor hwnd to render into
	m_hEditorHWnd = nullptr;

	// One-time setup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	if ( !OnStartup( m_StartupInfo.m_pInstance, m_StartupInfo.m_pInitialMod ) )
	{
		return HandleSetModeError();
	}

	return INIT_OK; 
}

void CEngineAPI::Shutdown() 
{
	VideoMode_Destroy();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the engine to run in a particular editor window
//-----------------------------------------------------------------------------
void CEngineAPI::SetEngineWindow( void *hWnd )
{
	if ( !InEditMode() )
		return;

	// Detach input from the previous editor window
	game->InputDetachFromGameWindow();

	m_hEditorHWnd = hWnd;
	videomode->SetGameWindow( m_hEditorHWnd );
}


//-----------------------------------------------------------------------------
// Posts a console command
//-----------------------------------------------------------------------------
void CEngineAPI::PostConsoleCommand( const char *pCommand )
{
	contract_assert( 0 ); // This isn't being used, but I'm assuming it would be for a dedicated server type console, so issueing the command into the server's buffer may make sense.
	Cbuf_AddText( CBUF_SERVER, pCommand );
}

	
//-----------------------------------------------------------------------------
// Is the engine currently rinning?
//-----------------------------------------------------------------------------
bool CEngineAPI::IsRunningSimulation() const
{
	return (eng->GetState() == IEngine::DLL_ACTIVE);
}


//-----------------------------------------------------------------------------
// Reset the map we're on
//-----------------------------------------------------------------------------
void CEngineAPI::SetMap( const char *pMapName )
{
//	if ( !V_stricmp( sv.mapname, pMapName ) )
//		return;

	char buf[MAX_PATH];
	V_snprintf( buf, MAX_PATH, "map %s", pMapName );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), buf );
}


//-----------------------------------------------------------------------------
// Start/stop running the simulation
//-----------------------------------------------------------------------------
void CEngineAPI::ActivateSimulation( bool bActive )
{
	// FIXME: Not sure what will happen in this case
	if ( ( eng->GetState() != IEngine::DLL_ACTIVE )	&&
		 ( eng->GetState() != IEngine::DLL_PAUSED ) )
	{
		return;
	}

	bool bCurrentlyActive = (eng->GetState() != IEngine::DLL_PAUSED);
	if ( bActive == bCurrentlyActive )
		return;

	// FIXME: Should attachment/detachment be part of the state machine in IEngine?
	if ( !bActive )
	{
		eng->SetNextState( IEngine::DLL_PAUSED );

		// Detach input from the previous editor window
		game->InputDetachFromGameWindow();
	}
	else
	{
		eng->SetNextState( IEngine::DLL_ACTIVE );

		// Start accepting input from the new window
		// FIXME: What if the attachment fails?
		game->InputAttachToGameWindow();
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: Message pump when running stand-alone
//-----------------------------------------------------------------------------
void CEngineAPI::PumpMessages()
{
	g_pLauncherMgr->PumpWindowsMessageLoop();

	// Get input from attached devices
	g_pInputSystem->PollInputState( GetBaseLocalClient().IsActive() );

	// NOTE: Under some implementations of Win9x, 
	// dispatching messages can cause the FPU control word to change
	SetupFPUControlWord();

	game->DispatchAllStoredGameMessages();

	EatTextModeKeyPresses();
}

//-----------------------------------------------------------------------------
// Purpose: Message pump
//-----------------------------------------------------------------------------
bool CEngineAPI::MainLoop()
{
	// Main message pump
	while ( true )
	{
		// Pump messages unless someone wants to quit
		if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		{
			if ( eng->GetQuitting() != IEngine::QUIT_TODESKTOP )
				return true;
			return false;
		}

		// Pump the message loop
		PumpMessages();

		eng->Frame();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the registry
//-----------------------------------------------------------------------------
bool CEngineAPI::InitRegistry( const char *pModName )
{
	char szRegSubPath[MAX_PATH];
	V_snprintf( szRegSubPath, sizeof(szRegSubPath), "%s\\%s", "Source", pModName );
	return registry->Init( szRegSubPath );
	return true;
}

void CEngineAPI::ShutdownRegistry( )
{
	registry->Shutdown( );
}


//-----------------------------------------------------------------------------
// One-time setup, based on the initially selected mod
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
bool CEngineAPI::OnStartup( void *pInstance, const char *pStartupModName )
{
	// This fixes a bug on certain machines where the input will 
	// stop coming in for about 1 second when someone hits a key.
	// (true means to disable priority boost)

	// FIXME: Turn videomode + game into IAppSystems?

	// Try to create the window
	COM_TimestampedLog( "game->Init" );

	splitscreen->Init();

	// This has to happen before CreateGameWindow to set up the instance
	// for use by the code that creates the window
	if ( !game->Init( pInstance ) )
	{
		goto onStartupError;
	}

	// Try to create the window
	COM_TimestampedLog( "videomode->Init" );

	// This needs to be after Shader_Init and registry->Init
	// This way mods can have different default video settings
	if ( !videomode->Init( ) )
	{
		goto onStartupShutdownGame;
	}

	COM_TimestampedLog( "InitRegistry" );
	
	// We need to access the registry to get various settings (specifically,
	// InitMaterialSystemConfig requires it).
	if ( !InitRegistry( pStartupModName ) )
	{
		goto onStartupShutdownVideoMode;
	}

	COM_TimestampedLog( "materials->ModInit" );

	materials->ModInit();

	COM_TimestampedLog( "InitMaterialSystemConfig" );

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
	InitMaterialSystemConfig( InEditMode() );
	
	{
	}

	COM_TimestampedLog( "ShutdownRegistry" );

	ShutdownRegistry();
	return true;

	// Various error conditions
onStartupShutdownVideoMode:
	videomode->Shutdown();

onStartupShutdownGame:
	game->Shutdown();

onStartupError:
	return false;
}


//-----------------------------------------------------------------------------
// One-time shutdown (shuts down stuff set up in OnStartup)
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
void CEngineAPI::OnShutdown()
{
	if ( videomode )
	{
		videomode->Shutdown();
	}

	// Shut down the game
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );

	splitscreen->Shutdown();

}

static bool IsValveMod( const char *pModName )
{
	// Figure out if we're running a Valve mod or not.
	return ( V_stricmp( pModName, "cstrike" ) == 0 ||
		V_stricmp( pModName, "dod" ) == 0 ||
		V_stricmp( pModName, "hl1mp" ) == 0 ||
		V_stricmp( pModName, "tf" ) == 0 ||
		V_stricmp( pModName, "hl2mp" ) == 0 ||
		V_stricmp( pModName, "csgo" ) == 0 );
}

//-----------------------------------------------------------------------------
// Initialization, shutdown of a mod.
//-----------------------------------------------------------------------------
bool CEngineAPI::ModInit( const char *pModName, const char *pGameDir )
{
	COM_TimestampedLog( "ModInit" );

	// Set up the engineparms_t which contains global information about the mod
	host_parms.mod = COM_StringCopy( GetModDirFromPath( pModName ) );
	host_parms.game = COM_StringCopy( pGameDir );

	// By default, restrict server commands in Valve games and don't restrict them in mods.
	bool bRestrictCommands = IsValveMod( host_parms.mod );
	GetBaseLocalClient().m_bRestrictServerCommands = bRestrictCommands;
	GetBaseLocalClient().m_bRestrictClientCommands = bRestrictCommands;

	// build the registry path we're going to use for this mod
	InitRegistry( pModName );

	// This sets up the game search path, depends on host_parms
	TRACEINIT( MapReslistGenerator_Init(), MapReslistGenerator_Shutdown() );
	TRACEINIT( DevShotGenerator_Init(), DevShotGenerator_Shutdown() );

	COM_TimestampedLog( "Host_ReadPreStartupConfiguration - Start" );

	// Slam cvars based on mod/config.cfg
	Host_ReadPreStartupConfiguration();

	COM_TimestampedLog( "Host_ReadPreStartupConfiguration - Finish" );
	// Create the game window now that we have a search path
	// FIXME: Deal with initial window width + height better
	if ( !videomode || !videomode->CreateGameWindow( g_pMaterialSystemConfig->m_VideoMode.m_Width, g_pMaterialSystemConfig->m_VideoMode.m_Height, g_pMaterialSystemConfig->Windowed(), g_pMaterialSystemConfig->NoWindowBorder() ) )
	{
		return false;
	}

	return true;
}

void CEngineAPI::ModShutdown()
{
	COM_StringFree(host_parms.mod);
	COM_StringFree(host_parms.game);
	
	// Stop accepting input from the window
	game->InputDetachFromGameWindow();

	TRACESHUTDOWN( DevShotGenerator_Shutdown() );
	TRACESHUTDOWN( MapReslistGenerator_Shutdown() );

	ShutdownRegistry();
}


//-----------------------------------------------------------------------------
// Purpose: Handles there being an error setting up the video mode
// Output : Returns true on if the engine should restart, false if it should quit
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::HandleSetModeError()
{
	// show an error, see if the user wants to restart
	if ( CommandLine()->FindParm( "-safe" ) )
	{
		Sys_MessageBox( "Failed to set video mode.\n\nThis game has a minimum requirement of DirectX 7.0 compatible hardware.\n", "Video mode error", false );
		return INIT_FAILED;
	}
	
	if ( CommandLine()->FindParm( "-autoconfig" ) )
	{
		if ( Sys_MessageBox( "Failed to set video mode - falling back to safe mode settings.\n\nGame will now restart with the new video settings.", "Video - safe mode fallback", true ))
		{
			CommandLine()->AppendParm( "-safe", nullptr );
			return (InitReturnVal_t)INIT_RESTART;
		}
		return INIT_FAILED;
	}

	if ( Sys_MessageBox( "Failed to set video mode - resetting to defaults.\n\nGame will now restart with the new video settings.", "Video mode warning", true ) )
	{
		CommandLine()->AppendParm( "-autoconfig", nullptr );
		return (InitReturnVal_t)INIT_RESTART;
	}

	return INIT_FAILED;
}

//-----------------------------------------------------------------------------
// Purpose: Main loop for non-dedicated servers
//-----------------------------------------------------------------------------
int CEngineAPI::RunListenServer()
{
	//
	// NOTE: Systems set up here should depend on the mod 
	// Systems which are mod-independent should be set up in the launcher or Init()
	//

	// Innocent until proven guilty
	int nRunResult = RUN_OK;

	// Happens every time we start up and shut down a mod
	if ( ModInit( m_StartupInfo.m_pInitialMod, m_StartupInfo.m_pInitialGame ) )
	{
		CModAppSystemGroup modAppSystemGroup( false, m_StartupInfo.m_pParentAppSystemGroup );

		// Store off the app system factory...
		g_AppSystemFactory = modAppSystemGroup.GetFactory();

		nRunResult = modAppSystemGroup.Run();

		g_AppSystemFactory = nullptr;

		// Shuts down the mod
		ModShutdown();

		// Disconnects from the editor window
		videomode->SetGameWindow( nullptr );
	}

	// Closes down things that were set up in OnStartup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	OnShutdown();

	return nRunResult;
}



static void render_blanks( const CCommand &args );
static ConCommand render_blanks_command( "render_blanks", render_blanks, "render N blank frames" );
static void render_blanks( const CCommand &args )
{
	uint nFrames = 0;
	if( *args[1] )
		nFrames = std::atoi( args[1] );
	if( !nFrames || nFrames > 1000 )
	{
		nFrames = 10;
		Msg("Clamping to %d frames", nFrames );
	}
	MaterialLock_t matlock = materials->Lock();
	materials->SpinPresent( nFrames );
	materials->Unlock( matlock );	
}


extern void S_ClearBuffer();
#endif // #ifndef DEDICATED

extern bool g_bUpdateMinidumpComment;
void GetSpew( char *buf, std::size_t buflen );

// should not exceed 32K, since current breakpad minidump reading code limits total comment size to 32k.
constexpr auto DUMP_COMMENT_SIZE = 32768;

// Turn this to 1 to allow for > 3kb of comments in dumps
static ConVar sys_minidumpexpandedspew( "sys_minidumpexpandedspew", "0" );

extern "C" void FailSafe( unsigned int uStructuredExceptionCode, struct _EXCEPTION_POINTERS * pExceptionInfo	)
{
	// Nothing, this just catches a crash when creating the comment block
}

class CErrorText
{
public:
	explicit CErrorText( int size ) : 
		m_Size( size ),
		m_errorText( new char[ size ] )
	{
		V_memset( m_errorText, 0x00, m_Size );
	}

	~CErrorText()
	{
		delete[] m_errorText;
	}

	void BuildComment( char const *pchSysErrorText )
	{

		try 
		{
			int nSize = m_Size;
			nSize = std::min( nSize, DUMP_COMMENT_SIZE );

			V_memset( m_errorText, 0x00, nSize );

			if ( pchSysErrorText )
			{
				// Strip trailing return character (note this overwrites the return character built into the string)
				char *pchFixup = (char *)pchSysErrorText;
				int lenFixed = V_strlen( pchFixup );
				if ( pchFixup[ lenFixed - 1 ] == '\n' )
				{
					pchFixup[ lenFixed - 1 ] = 0;
				}
				V_snprintf( m_errorText, nSize, "\nSys_Error( %s )\n", pchFixup );
			}
			else
			{
				V_snprintf( m_errorText, nSize, "\nCrash\n" );
			}

			V_strncat( m_errorText, Status_GetBuffer(), nSize );

			// Latch in case below stuff crashes

			bool bExtendedSpew = sys_minidumpexpandedspew.GetBool();
			if ( bExtendedSpew )
			{
				try
				{
					
					V_strncat( m_errorText, "\nConVars (non-default)\n\n", nSize );
					char header[ 128 ];
					V_snprintf( header, sizeof( header ), "%25.25s %25.25s %25.25s\n", "var", "value", "default" );
					V_strncat( m_errorText, header, nSize );

					int len = V_strlen( m_errorText );
					if ( len < nSize )
					{
						int remainder = nSize - len;
						char *pbuf = m_errorText + len;

						ICvar::Iterator iter( g_pCVar );
						for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
						{
							ConCommandBase *var = iter.Get();
							if ( var->IsCommand() )
								continue;

							const ConVar *cvar = ( const ConVar * )var;
							if ( cvar->GetFlags() & ( FCVAR_SERVER_CANNOT_QUERY | FCVAR_PROTECTED ) )
								continue;

							if ( !( cvar->GetFlags() & FCVAR_NEVER_AS_STRING ) )
							{
								char var1[ MAX_OSPATH ];
								char var2[ MAX_OSPATH ];

								V_strncpy( var1, Host_CleanupConVarStringValue( cvar->GetString() ), sizeof( var1 ) );
								V_strncpy( var2, Host_CleanupConVarStringValue( cvar->GetDefault() ), sizeof( var2 ) );

								if ( !V_stricmp( var1, var2 ) )
									continue;
							}
							else
							{
								if ( cvar->GetFloat() == V_atof( cvar->GetDefault() ) )
									continue;
							}

							char cvarcmd[ MAX_OSPATH ];
							
							int add = 0;
							if ( !( cvar->GetFlags() & FCVAR_NEVER_AS_STRING ) )
							{
								add = V_snprintf( cvarcmd, sizeof(cvarcmd),"%25.25s %25.25s %25.25s\n",
									cvar->GetName(), Host_CleanupConVarStringValue( cvar->GetString() ), cvar->GetDefault() );
							}
							else
							{
								add = V_snprintf( cvarcmd, sizeof(cvarcmd),"%25.25s %25.25f %25.25f\n",
								cvar->GetName(), cvar->GetFloat(), V_atof( cvar->GetDefault() ) );
							}

							int toCopy = std::min( add, remainder );
							if ( toCopy <= 0 )
								break;

							V_memcpy( pbuf, cvarcmd, toCopy );

							pbuf += toCopy;
							remainder -= toCopy;
							if ( remainder <= 0 )
								break;
						}
						*pbuf = 0;
					}

					V_strncat( m_errorText, "\nConsole History (reversed)\n\n", nSize );

					// Get console
					len = V_strlen( m_errorText );
					
					if ( len < nSize )
					{
						GetSpew( m_errorText + len, nSize - len );
					}
				}
				catch ( ... )
				{
					V_strncat( m_errorText, "exception thrown building console/convar history!!!\n", nSize );
				}

			}
		}
		catch ( ... )
		{
			// Oh oh
		}
		

	}

public:
	int		m_Size;
	char	*m_errorText;
};

static CErrorText errorText( DUMP_COMMENT_SIZE );

void BuildMinidumpComment( char const *pchSysErrorText )
{
	errorText.BuildComment( pchSysErrorText );
}

#ifndef DEDICATED

extern "C" void WriteMiniDumpUsingExceptionInfo( unsigned int uStructuredExceptionCode, struct _EXCEPTION_POINTERS * pExceptionInfo	)
{
	// TODO: dynamically set the minidump comment from contextual info about the crash (i.e current VPROF node)?
} 

extern "C" void WriteMiniDump( void	);

//-----------------------------------------------------------------------------
// Purpose: Main 
//-----------------------------------------------------------------------------
int CEngineAPI::Run()
{
	contract_assert( !"Impl minidump handling on Posix" );
	return RunListenServer();
}
#endif // DEDICATED

bool g_bUsingLegacyAppSystems = false;

bool CModAppSystemGroup::AddLegacySystems()
{
	g_bUsingLegacyAppSystems = true;

	AppSystemInfo_t appSystems[] = 
	{
		{ "engine.so", SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }					// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;


	return true;
}

//-----------------------------------------------------------------------------
static int VerToInt( const char *pszVersion )
{
	char szOut[ 32 ];
	const char *pIn = pszVersion;
	char *pOut = szOut;

	if ( !pszVersion || std::strlen( pszVersion ) > sizeof( szOut ) ) // double check we won't overflow the buffer
	{
		return 0;
	}

	while ( *pIn )
	{
		if ( *pIn != '.' )
		{
			*pOut++ = *pIn;
		}
		pIn++;
	}
	*pOut = '\0';

	return std::atoi( szOut );
}


inline constexpr const char *VERSION_KEY = "PatchVersion=";
inline constexpr const char *CLIENT_VERSION_KEY = "ClientVersion=";
inline constexpr const char *SERVER_VERSION_KEY = "ServerVersion=";
inline constexpr const char *PRODUCT_KEY = "ProductName=";
inline constexpr const char *PRODUCT_STRING = "valve";
inline constexpr const char *VERSION_STRING = "1.0.1.0";
inline constexpr const char *FS_MAGIC_NUM_KEY = "FSKey=";

static std::string g_sVersionString;
static std::string g_sProductString;
static int32 sHostVersion;
static int32 sClientVersion;
static int32 sServerVersion;

//-----------------------------------------------------------------------------
const char *GetHostVersionString()
{
	return g_sVersionString.c_str();
}

//-----------------------------------------------------------------------------
const char *GetHostProductString()
{
	return g_sProductString.c_str();
}

int32 GetHostVersion()
{
	return sHostVersion;
}

//-----------------------------------------------------------------------------
int32 GetClientVersion()
{
	// if we are running from perforce, we report 0 as version so checking isn't enforced
	if ( g_bRunningFromPerforce )
	{
		return 0;
	}
	return sClientVersion;
}

//-----------------------------------------------------------------------------
int32 GetServerVersion()
{
	// if we are running from perforce, we report 0 as version so checking isn't enforced
	if ( g_bRunningFromPerforce )
	{
		return 0;
	}
	return sServerVersion;
}

const char *Sys_GetVersionString()
{
	return g_sVersionString.c_str();
}

const char *Sys_GetProductString()
{
	return g_sProductString.c_str();
}

static bool ParseVersionInfFile( const char *szFileName )
{
	ks::ScratchArena<> scratch;
	char *buffer;
	int bufsize = 0;
	FileHandle_t fp = nullptr;
	const char *pbuf = nullptr;
	const int numKeysExpected = 4; // number of expected keys
	int gotKeys = 0;

	// Mod's copy of the file is the first option.
	fp = g_pFileSystem->Open( szFileName, "r" );
	if ( fp )
	{
		bufsize = g_pFileSystem->Size( fp );
		buffer = ( char * )scratch.AllocBytes( bufsize + 1 );
		contract_assert( buffer );

		int iBytesRead = g_pFileSystem->Read( buffer, bufsize, fp );
		g_pFileSystem->Close( fp );

		buffer[iBytesRead] = '\0';

		// Read 
		pbuf = buffer;

		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf )
				break;

			if ( V_strlen( com_token )  <= 0 )
				break;


			if ( !V_strnicmp( com_token, VERSION_KEY, V_strlen( VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				V_strncpy( buf, com_token+V_strlen( VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				g_sVersionString = buf;
				sHostVersion = VerToInt( buf );

				gotKeys++;
				continue;
			}

			if ( !V_strnicmp( com_token, CLIENT_VERSION_KEY, V_strlen( CLIENT_VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				V_strncpy( buf, com_token+V_strlen( CLIENT_VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				sClientVersion = std::atoi( buf );

				gotKeys++;
				continue;
			}

			if ( !V_strnicmp( com_token, SERVER_VERSION_KEY, V_strlen( SERVER_VERSION_KEY ) ) )
			{
				char buf[ 256 ];
				V_strncpy( buf, com_token+V_strlen( SERVER_VERSION_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				sServerVersion = std::atoi( buf );

				gotKeys++;
				continue;
			}

			if ( !V_strnicmp( com_token, PRODUCT_KEY, V_strlen( PRODUCT_KEY ) ) )
			{
				char buf[ 256 ];
				V_strncpy( buf, com_token+V_strlen( PRODUCT_KEY ), sizeof( buf ) - 1 );
				buf[ sizeof( buf ) - 1 ] = '\0';

				g_sProductString = buf;

				gotKeys++;
				continue;
			}

		}
	}

	return gotKeys == numKeysExpected;
}


//-----------------------------------------------------------------------------
static bool ParsePerforceInfFile( const char *szFileName, uint64 &unFileSystemMagicNumber )
{
	char *buffer;
	int bufsize = 0;
	FileHandle_t fp = nullptr;

	// Mod's copy of the file is the first option.
	fp = g_pFileSystem->Open( szFileName, "r" );
	if ( fp )
	{
		bufsize = g_pFileSystem->Size( fp );
		buffer = ( char * )__builtin_alloca( bufsize + 1 );
		contract_assert( buffer );

		int iBytesRead = g_pFileSystem->Read( buffer, bufsize, fp );
		g_pFileSystem->Close( fp );

		buffer[iBytesRead] = '\0';

		// Read 
		const char *pbuf = buffer;

		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf )
				break;

			if ( V_strlen( com_token )  <= 0 )
				break;

			// Get the magic number that allows us to run internally. This magic number should only live in the Perforce.inf
			// file which is never shipped. 
			if ( !V_strnicmp( com_token, FS_MAGIC_NUM_KEY, V_strlen( FS_MAGIC_NUM_KEY ) ) )
			{
				char szFSKey[64];
				V_strncpy( szFSKey, com_token + V_strlen( FS_MAGIC_NUM_KEY ), sizeof( szFSKey ) - 1 );
				unFileSystemMagicNumber = std::atoi(szFSKey);
				return true;
			}
		}
	}
	return false;
}


void Sys_Version( bool bDedicated )
{

	g_sVersionString = VERSION_STRING;
	g_sProductString = PRODUCT_STRING;

	uint64 unFSMagicNumber = 0;

	if ( !ParseVersionInfFile( "steam.inf" ) )
	{
		Sys_Error( "Unable to load version from steam.inf" );
	}

	ParsePerforceInfFile( "perforce.inf", unFSMagicNumber );

	// If the magic number is found in the perforce.inf then we are running from Perforce
	g_bRunningFromPerforce = ( unFSMagicNumber == 2190015756ull / 2 );

}


//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::Create()
{
	COM_TimestampedLog( "CModAppSystemGroup::Create() - Start" );

	//////////////////////////////////////////////////////////////////////////
	//
	// Matchmaking
	//

	g_pfnMatchmakingFactory = Sys_GetFactoryThis();
	g_pIfaceMatchFramework = ( IMatchFramework * ) g_pfnMatchmakingFactory( IMATCHFRAMEWORK_VERSION_STRING, nullptr );
	if ( !g_pIfaceMatchFramework )
	{
		Sys_Error( "Could not create the match framework" );
	}

	g_pMatchFramework = g_pIfaceMatchFramework;

	AddSystem( g_pIfaceMatchFramework, IMATCHFRAMEWORK_VERSION_STRING );
	
	Host_SubscribeForProfileEvents( true );

	//////////////////////////////////////////////////////////////////////////
	//
	// Client/server
	//

#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		if ( !ClientDLL_Load() )
			return false;
		ClientDLL_Connect();
	}
#endif

	if ( !ServerDLL_Load( IsServerOnly() ) )
	{
#ifndef DEDICATED
		if ( !IsServerOnly() )
		{
			ClientDLL_Disconnect();
		}
#endif
	
		return false;
	}

	IClientDLLSharedAppSystems *clientSharedSystems = 0;

#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		clientSharedSystems = ( IClientDLLSharedAppSystems * )g_ClientFactory( CLIENT_DLL_SHARED_APPSYSTEMS, nullptr );
		if ( !clientSharedSystems )
			return AddLegacySystems();
	}
#endif

	IServerDLLSharedAppSystems *serverSharedSystems = ( IServerDLLSharedAppSystems * )g_ServerFactory( SERVER_DLL_SHARED_APPSYSTEMS, nullptr );
	if ( !serverSharedSystems )
	{
		contract_assert( !"Expected both game and client .dlls to have or not have shared app systems interfaces!!!" );
		return AddLegacySystems();
	}

	// Load game and client .dlls and build list then
	std::vector< AppSystemInfo_t >	systems;

	int i;
	int serverCount = serverSharedSystems->Count();
	for ( i = 0 ; i < serverCount; ++i )
	{
		const char *dllName = serverSharedSystems->GetDllName( i );
		const char *interfaceName = serverSharedSystems->GetInterfaceName( i );

		AppSystemInfo_t info;
		info.m_pModuleName = dllName;
		info.m_pInterfaceName = interfaceName;

		systems.push_back( info );
	}

	if ( !IsServerOnly() )
	{
		int clientCount = clientSharedSystems->Count();
		for ( i = 0 ; i < clientCount; ++i )
		{
			const char *dllName = clientSharedSystems->GetDllName( i );
			const char *interfaceName = clientSharedSystems->GetInterfaceName( i );

			if ( ModuleAlreadyInList( systems, dllName, interfaceName ) )
				continue;

			AppSystemInfo_t info;
			info.m_pModuleName = dllName;
			info.m_pInterfaceName = interfaceName;

			systems.push_back( info );
		}
	}

	AppSystemInfo_t info;
	info.m_pModuleName = "";
	info.m_pInterfaceName = "";
	systems.push_back( info );

	if ( !AddSystems( systems.data() ) ) 
		return false;


	COM_TimestampedLog( "CModAppSystemGroup::Create() - Finish" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, we might need to verify if the interface names differ for the client versus the server
// Input  : list - 
//			*moduleName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::ModuleAlreadyInList( std::vector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName )
{
	for ( int i = 0; i < std::ssize( list ); ++i )
	{
		if ( !V_stricmp( list[ i ].m_pModuleName, moduleName ) &&
			 !V_stricmp( list[ i ].m_pInterfaceName, interfaceName ) )
			return true;
	}

	return false;
}

bool CModAppSystemGroup::PreInit()
{
	return true;
}

void SV_ShutdownGameDLL();
int CModAppSystemGroup::Main()
{
	int nRunResult = RUN_OK;

	if ( IsServerOnly() )
	{
		// Start up the game engine
		if ( eng->Load( true, host_parms.basedir ) )
		{
			// Dedicated server drives frame loop manually
			dedicated->RunServer();

			SV_ShutdownGameDLL();
		}
	}
	else
	{
		eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

		COM_TimestampedLog( "eng->Load" );

		// Start up the game engine
		if ( eng->Load( false, host_parms.basedir ) )					
		{
#if !defined(DEDICATED)
			if ( s_EngineAPI.MainLoop() )
			{
				nRunResult = RUN_RESTART;
			}

			// unload systems
			eng->Unload();
#endif
			SV_ShutdownGameDLL();
		}
	}
	
	return nRunResult;
}

void CModAppSystemGroup::PostShutdown()
{
}

void CModAppSystemGroup::Destroy()
{
	// unload game and client .dlls
	ServerDLL_Unload();
#ifndef DEDICATED
	if ( !IsServerOnly() )
	{
		ClientDLL_Unload();
	}
#endif
	
	/// Matchmaking

	Host_SubscribeForProfileEvents( false );

	// Host_Shutdown() runs after Destroy() on a dedicated server
	if ( g_pMatchFramework )
	{
		g_pMatchFramework->Shutdown();
		g_pMatchFramework = nullptr;
	}

	g_pIfaceMatchFramework = nullptr;
	g_pfnMatchmakingFactory = nullptr;

	/// vjobs

}

//-----------------------------------------------------------------------------
// Console command to toggle back and forth between the engine running or not
//-----------------------------------------------------------------------------
#ifndef DEDICATED
void EditorToggle_f()
{
	// Will switch back to the editor
	bool bActive = (eng->GetState() != IEngine::DLL_PAUSED);
	s_EngineAPI.ActivateSimulation( !bActive );
}
#endif // DEDICATED



//-----------------------------------------------------------------------------
//
// Purpose: Expose engine interface to launcher	for dedicated servers
//
//-----------------------------------------------------------------------------
class CDedicatedServerAPI : public CTier3AppSystem< IDedicatedServerAPI >
{
	typedef CTier3AppSystem< IDedicatedServerAPI > BaseClass;

public:
	CDedicatedServerAPI() :
	  m_pDedicatedServer( 0 )
	{
	}
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

	virtual bool ModInit( ModInfo_t &info );
	virtual void ModShutdown( void );

	virtual bool RunFrame( void );

	virtual void AddConsoleText( char *text );
	virtual void UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
	virtual void UpdateHostname(char *pszHostname, int maxlen);

	virtual void SetSubProcessID( int nID, ks::platform::socket *pSocketToParent ) override;

	static void PreMinidumpCallback( void *pvContext );
	void PreMinidumpCallbackImpl();

private:
	int BuildMapCycleListHints( char **hints );

	CModAppSystemGroup *m_pDedicatedServer;
};

void CDedicatedServerAPI::SetSubProcessID( int nId, ks::platform::socket *pSocketToParent )
{
	g_nForkID = nId;
	g_nSocketToParentProcess = pSocketToParent;
	ks::platform::set_nonblocking( pSocketToParent, nullptr );

}

// Static method
void CDedicatedServerAPI::PreMinidumpCallback( void *pvContext )
{
	if ( !pvContext )
	{
		return;
	}
	
	((CDedicatedServerAPI *)pvContext)->PreMinidumpCallbackImpl();
}


void CDedicatedServerAPI::PreMinidumpCallbackImpl()
{
	EndWatchdogTimer(); // Uploading the dump can take a while, turn off our watchdog

	// Win32 dedicated servers build a minidump comment in the exception handler itself
	std::fprintf( stderr, "PreMinidumpCallback: updating dump comment\n" );
	BuildMinidumpComment( nullptr );
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CDedicatedServerAPI s_Singleton_CDedicatedServerAPI;
static InterfaceReg s_Reg_CDedicatedServerAPI_IDedicatedServerAPI( []() -> void * { return static_cast<IDedicatedServerAPI *>( &s_Singleton_CDedicatedServerAPI ); }, VENGINE_HLDS_API_VERSION );;

bool g_bIsVGuiBasedDedicatedServer = false; // Assume use convar
//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CDedicatedServerAPI::Connect( CreateInterfaceFn factory ) 
{ 
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	dedicated = ( IDedicatedExports * )factory( VENGINE_DEDICATEDEXPORTS_API_VERSION, nullptr );
	if ( !dedicated )
		return false;

	g_pFileSystem = g_pFullFileSystem;

	g_pFileSystem->SetWarningFunc( Warning );

	if ( !Shader_Connect( false ) )
		return false;

	if ( !g_pStudioRender )
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, nullptr );

	g_pSoundEmitterSystem = (ISoundEmitterSystemBase*)factory( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, nullptr);

#if defined( DEDICATED )
	if ( !g_pDataCache || !g_pPhysics || !g_pMDLCache ) 
#else
	if ( !g_pDataCache || !g_pPhysics || !g_pMDLCache || !g_pSoundEmitterSystem)
#endif
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	ConnectMDLCacheNotify();

#ifndef DEDICATED
	splitscreen->Init();
#endif

	return true; 
}

void CDedicatedServerAPI::Disconnect() 
{
#ifndef DEDICATED
	splitscreen->Shutdown();
#endif

	DisconnectMDLCacheNotify();

	g_pPhysics = nullptr;
	g_pSoundEmitterSystem = nullptr;

	Shader_Disconnect();

	g_pFileSystem = nullptr;

	ConVar_Unregister();

	dedicated = nullptr;

	BaseClass::Disconnect();

	g_AppSystemFactory = nullptr;
}

//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CDedicatedServerAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, nullptr );				// to prevent the LTCG compiler from crashing.
}

//-----------------------------------------------------------------------------
// Creates the hint list for a multiplayer map rotation from the map cycle.
// The map cycle message is a text string with CR/CRLF separated lines.
//	-removes comments
//	-removes arguments
//-----------------------------------------------------------------------------
const char *szCommonPreloads  = "MP_Preloads";
const char *szReslistsBaseDir = "reslists2";
const char *szReslistsExt     = ".lst";

int CDedicatedServerAPI::BuildMapCycleListHints(char **hints)
{
	char szMap[ MAX_OSPATH + 2 ]; // room for one path plus <CR><LF>
	unsigned int length;
	char szMod[MAX_OSPATH];

	// Determine the mod directory.
	V_FileBase(com_gamedir, szMod, sizeof( szMod ) );

	// Open mapcycle.txt
	char cszMapCycleTxtFile[MAX_OSPATH];
	V_snprintf(cszMapCycleTxtFile, sizeof( cszMapCycleTxtFile ), "%s\\mapcycle.txt", szMod);
	FileHandle_t pFile = g_pFileSystem->Open(cszMapCycleTxtFile, "rb");
	if ( pFile == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg("Unable to open %s", cszMapCycleTxtFile);
		return 0;
	}

	// Start off with the common preloads.
	V_snprintf(szMap, sizeof( szMap ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, szMod, szCommonPreloads, szReslistsExt);
	int hintsSize = std::strlen(szMap) + 1;
	*hints = (char*)MemAlloc_Alloc( hintsSize );
	if ( *hints == nullptr )
	{
		ConMsg("Unable to allocate memory for map cycle hints list");
		g_pFileSystem->Close( pFile );
		return 0;
	}
	V_strncpy( *hints, szMap, hintsSize );
		
	// Read in and parse mapcycle.txt
	length = g_pFileSystem->Size(pFile);
	if ( length )
	{
		char *pStart = (char *)MemAlloc_Alloc(length);
		if ( pStart && ( 1 == g_pFileSystem->Read(pStart, length, pFile) )
		   )
		{
			const char *pFileList = pStart;

			while ( 1 )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) <= 0 )
					break;

				V_strncpy(szMap, com_token, sizeof(szMap));

				// Any more tokens on this line?
				if ( COM_TokenWaiting( pFileList ) )
				{
					pFileList = COM_Parse( pFileList );
				}

				char mapLine[sizeof(szMap)];
				V_snprintf(mapLine, sizeof(mapLine), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, szMod, szMap, szReslistsExt);
				*hints = (char*)g_pMemAlloc->Realloc(*hints, strlen(*hints) + 1 + strlen(mapLine) + 1); // count NULL string terminators
				if ( *hints == nullptr )
				{
					ConMsg("Unable to reallocate memory for map cycle hints list");
					g_pFileSystem->Close( pFile );
					return 0;
				}
				V_strncat(*hints, mapLine, hintsSize, COPY_ALL_CHARACTERS);
			}
		}
	}

	g_pFileSystem->Close(pFile);

	// Tack on <moddir>\mp_maps.txt to the end to make sure we load reslists for all multiplayer maps we know of
	V_snprintf(szMap, sizeof( szMap ), "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, szMod);
	*hints = (char*)g_pMemAlloc->Realloc(*hints, strlen(*hints) + 1 + strlen(szMap) + 1); // count NULL string terminators
	V_strncat( *hints, szMap, hintsSize, COPY_ALL_CHARACTERS );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 0 == normal, 1 == dedicated server
//			*instance - 
//			*basedir - 
//			*cmdline - 
//			launcherFactory - 
//-----------------------------------------------------------------------------
bool CDedicatedServerAPI::ModInit( ModInfo_t &info )
{
	g_bIsVGuiBasedDedicatedServer = dedicated->IsGuiDedicatedServer();
	s_bIsDedicatedServer = true;

	//
	// Configure breakpad
	// this must be done after mod search path chain is configured
	//
	Sys_Version( true );


	eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>(info.m_pBaseDirectory);
	host_parms.mod = const_cast<char*>(GetModDirFromPath(info.m_pInitialMod));
	host_parms.game = const_cast<char*>(info.m_pInitialGame);

	g_bTextMode = info.m_bTextMode;

	TRACEINIT( COM_InitFilesystem( info.m_pInitialMod ), COM_ShutdownFileSystem() );
	// set this up as early as possible, if the server isn't going to run pure, stop CRCing bits as we load them
	// this happens even before the ConCommand's are processed, but we need to be sure to either CRC every file
	// that is loaded, or not bother doing any
	// Note that this mirrors g_sv_pure_mode from sv_main.cpp
	int pure_mode = 1; // default to on, +sv_pure 0 or -sv_pure 0 will turn it off
	if ( CommandLine()->CheckParm("+sv_pure") )
		pure_mode = CommandLine()->ParmValue( "+sv_pure", 1 );
	else if ( CommandLine()->CheckParm("-sv_pure") )
		pure_mode = CommandLine()->ParmValue( "-sv_pure", 1 );
	if ( pure_mode )
	{
		// Mount all compatibility VPKs as well at the tail of the VPK search chain for verifying sv_pure clients
		//
		// See above:
		//		CEngineAPI::SetStartupInfo
		//	for how clients enable VPK content-shadowing
		//
		char const *szCompatibilityVPKs[] = {
			"lowviolence",
			"perfectworld"
		};
		for ( int j = 0; j < std::size( szCompatibilityVPKs ); ++ j )
		{	// Add in same order listed to tail
			g_pFullFileSystem->AddSearchPath( szCompatibilityVPKs[ j ], "COMPAT:GAME", PATH_ADD_TO_TAIL );
		}
		g_pFullFileSystem->EnableWhitelistFileTracking( true, true, CommandLine()->FindParm( "-sv_pure_verify_hashes" ) ? true : false );
	}
	else
		g_pFullFileSystem->EnableWhitelistFileTracking( false, false, false );

	materials->ModInit();

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
#ifndef DEDICATED
	InitMaterialSystemConfig( true );						// !!should this be called standalone or not?
#endif

	// Initialize general game stuff and create the main window
	if ( game->Init( nullptr ) )
	{
		m_pDedicatedServer = new CModAppSystemGroup( true, info.m_pParentAppSystemGroup );

		// Store off the app system factory...
		g_AppSystemFactory = m_pDedicatedServer->GetFactory();

		m_pDedicatedServer->Run();
		return true;
	}

	return false;
}

void CDedicatedServerAPI::ModShutdown( void )
{
	if ( m_pDedicatedServer )
	{
		delete m_pDedicatedServer;
		m_pDedicatedServer = nullptr;
	}

	g_AppSystemFactory = nullptr;

	// Unload GL, Sound, etc.
	eng->Unload();

	// Shut down memory, etc.
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );
}

bool CDedicatedServerAPI::RunFrame( void )
{
	// Bail if someone wants to quit.
	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
	{
		return false;
	}

	// Run engine frame
	eng->Frame();
	return true;
}

void CDedicatedServerAPI::AddConsoleText( char *text )
{
	Cbuf_AddText( CBUF_SERVER, text );
}

void CDedicatedServerAPI::UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen )
{
	Host_GetHostInfo( fps, nActive, nMaxPlayers, pszMap, maxlen );
}

void CDedicatedServerAPI::UpdateHostname(char *pszHostname, int maxlen)
{
	if ( pszHostname && ( maxlen > 0 ) )
	{
		V_strncpy( pszHostname, sv.GetName(), maxlen );
	}
}

#ifndef DEDICATED

class CGameUIFuncs : public IGameUIFuncs
{
public:
	bool IsKeyDown( const char *keyname, bool& isdown )
	{
		isdown = false;
		if ( !g_ClientDLL )
			return false;

		return g_ClientDLL->IN_IsKeyDown( keyname, isdown );
	}

	const char	*GetBindingForButtonCode( ButtonCode_t code )
	{
		return ::Key_BindingForKey( code );
	}

	ButtonCode_t GetButtonCodeForBind( const char *bind, int userId )
	{
		const char *pKeyName = Key_NameForBinding( bind , userId );
		if ( !pKeyName )
			return KEY_NONE;
		return g_pInputSystem->StringToButtonCode( pKeyName ) ;
	}

	void GetVideoModes( struct vmode_s **ppListStart, int *pCount )
	{
		if ( videomode )
		{
			*pCount = videomode->GetModeCount();
			*ppListStart = videomode->GetMode( 0 );
		}
		else
		{
			*pCount = 0;
			*ppListStart = nullptr;
		}
	}

	void GetDesktopResolution( int &width, int &height )
	{
		int refreshrate;
		game->GetDesktopInfo( width, height, refreshrate );
	}

	void SetFriendsID( uint friendsID, const char *friendsName )
	{
		GetLocalClient().SetFriendsID( friendsID, friendsName );
	}
};

#include "facade/igameuifuncs.inl"


static CGameUIFuncs s_Singleton_CGameUIFuncs;
static InterfaceReg s_Reg_CGameUIFuncs_IGameUIFuncs( []() -> void * { return static_cast<IGameUIFuncs *>( &s_Singleton_CGameUIFuncs ); }, VENGINE_GAMEUIFUNCS_VERSION );;

#endif
