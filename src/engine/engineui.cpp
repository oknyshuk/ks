// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Implements all the functions exported by the GameUI dll

// main precompiled header for client files


#include "common.h"

#include "cmd.h"
#include "render.h"


#include "screen.h"


#include "cdll_engine_int.h"
#include "client_class.h"
#include "client.h"
#include "cl_main.h"

#include "host.h"
#include "filesystem.h"
#include "filesystem_engine.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif


#include "keys.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "sys_dll.h"
#include "sound.h"
#include "soundflags.h"
#include "filesystem_engine.h"
#include "igame.h"
#include "cl_demoactionmanager.h"
#include "icolorcorrectiontools.h"
#include "client.h"
#include "server.h"
#include "ivideomode.h"
#include "cl_pluginhelpers.h"
#include "cl_main.h" // CL_IsHL2Demo()
#include "inputsystem/iinputstacksystem.h"

// interface to gameui dll
#include <GameUI/IGameUI.h>

// interface to expose vgui root panels
#include <iengineui.h>
import std;
import ks.tier1;
import ks.desktop;

#include "localize/ilocalize.h"

#include "engineui.h"
#include "LoadScreenUpdate.h"



#include "rocketui/rocketui.h"

extern IVEngineClient *engineClient;
extern HWND *pmainwindow;
extern bool g_bTextMode;
static int g_syncReportLevel = -1;

static void UI_PlaySound(const char *pFileName);

void UI_ActivateMouse();

extern CreateInterfaceFn g_AppSystemFactory;

// functions to reference GameUI and GameConsole functions, from GameUI.dll
IGameUI *staticGameUIFuncs = nullptr;
IGameUI* GetGameUI( void )
{
	return staticGameUIFuncs;
}

// cache some of the state we pass through to matsystemsurface, for visibility
bool s_bWindowsInputEnabled = true;

ConVar r_drawui( "r_drawui", "1", FCVAR_CHEAT, "Enable the rendering of vgui panels" );
ConVar gameui_xbox( "gameui_xbox", "0", 0 );

// Tracks whether console window is open or not - true as soon as we receive the request to open it, until after it has shutdown
ConVar cv_console_window_open( "console_window_open", nullptr, FCVAR_HIDDEN, "Is the console window active" );
ConVar cv_ignore_ui_activate_key( "ignore_ui_activate_key", nullptr, FCVAR_HIDDEN, "When set will ignore UI activation key" );
ConVar cv_uipanel_active( "ui_panel_active", nullptr, FCVAR_HIDDEN, "Is a vgui panel currently active" );
ConVar cv_server_browser_dialog_open( "server_browser_dialog_open", nullptr, FCVAR_HIDDEN, "Is the server browser window active" );

void ClearIOStates( void );

// turn this on if you're tuning progress bars
// #define ENABLE_LOADING_PROGRESS_PROFILING

#define PT( x ) #x, x

static tokenset_t< LevelLoadingProgress_e > g_ProgressTokens[]=
{
	{ PT( PROGRESS_DEFAULT ) },
	{ PT( PROGRESS_NONE ) },
	{ PT( PROGRESS_CHANGELEVEL ) },
	{ PT( PROGRESS_SPAWNSERVER ) },
	{ PT( PROGRESS_LOADWORLDMODEL ) },
	{ PT( PROGRESS_CRCMAP ) },
	{ PT( PROGRESS_CRCCLIENTDLL ) },
	{ PT( PROGRESS_CREATENETWORKSTRINGTABLES ) },
	{ PT( PROGRESS_PRECACHEWORLD ) },
	{ PT( PROGRESS_CLEARWORLD ) },
	{ PT( PROGRESS_LEVELINIT ) },
	{ PT( PROGRESS_PRECACHE ) },
	{ PT( PROGRESS_ACTIVATESERVER ) },
	{ PT( PROGRESS_BEGINCONNECT ) },
	{ PT( PROGRESS_SIGNONCHALLENGE ) },
	{ PT( PROGRESS_SIGNONCONNECT ) },
	{ PT( PROGRESS_SIGNONCONNECTED ) },
	{ PT( PROGRESS_PROCESSSERVERINFO ) },
	{ PT( PROGRESS_PROCESSSTRINGTABLE ) },
	{ PT( PROGRESS_SIGNONNEW ) },
	{ PT( PROGRESS_SENDCLIENTINFO ) },
	{ PT( PROGRESS_SENDSIGNONDATA ) },
	{ PT( PROGRESS_SIGNONSPAWN ) },
	{ PT( PROGRESS_CREATEENTITIES ) },
	{ PT( PROGRESS_FULLYCONNECTED ) },
	{ PT( PROGRESS_PRECACHELIGHTING ) },
	{ PT( PROGRESS_READYTOPLAY ) },
	{ PT( PROGRESS_HIGHESTITEM ) },
	{ nullptr, PROGRESS_INVALID }
};
//-----------------------------------------------------------------------------
// Purpose: Console command to hide the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
static void gameui_hide( const CCommand &args );
static ConCommand gameui_hide_command( "gameui_hide", gameui_hide, "Hides the game UI" );
static void gameui_hide( const CCommand &args )
{
	EngineUI()->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: Console command to activate the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
static void gameui_activate( const CCommand &args );
static ConCommand gameui_activate_command( "gameui_activate", gameui_activate, "Shows the game UI" );
static void gameui_activate( const CCommand &args )
{
	EngineUI()->ActivateGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void gameui_preventescape( const CCommand &args );
static ConCommand gameui_preventescape_command( "gameui_preventescape", gameui_preventescape, "Escape key doesn't hide game UI" );
static void gameui_preventescape( const CCommand &args )
{
	EngineUI()->SetNotAllowedToHideGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void gameui_allowescapetoshow( const CCommand &args );
static ConCommand gameui_allowescapetoshow_command( "gameui_allowescapetoshow", gameui_allowescapetoshow, "Escape key allowed to show game UI" );
static void gameui_allowescapetoshow( const CCommand &args )
{
	EngineUI()->SetNotAllowedToShowGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void gameui_preventescapetoshow( const CCommand &args );
static ConCommand gameui_preventescapetoshow_command( "gameui_preventescapetoshow", gameui_preventescapetoshow, "Escape key doesn't show game UI" );
static void gameui_preventescapetoshow( const CCommand &args )
{
	EngineUI()->SetNotAllowedToShowGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void gameui_allowescape( const CCommand &args );
static ConCommand gameui_allowescape_command( "gameui_allowescape", gameui_allowescape, "Escape key allowed to hide game UI" );
static void gameui_allowescape( const CCommand &args )
{
	EngineUI()->SetNotAllowedToHideGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: Console command to enable progress bar for next load
//-----------------------------------------------------------------------------
void BaseUI_ProgressEnabled_f()
{
	EngineUI()->EnabledProgressBarForNextLoad();
}
static ConCommand progress_enable("progress_enable", &BaseUI_ProgressEnabled_f );

//-----------------------------------------------------------------------------
//
// Purpose: Centerpoint for handling all user interface in the engine
//
//-----------------------------------------------------------------------------
class CEngineUI : public IEngineUIInternal
{
public:
	CEngineUI();
	~CEngineUI();

	// Methods of IEngineUIInternal
	void Init();
	void Connect();
	void Shutdown();
	bool SetVGUIDirectories();
	bool IsInitialized() const;
	bool Key_Event( const InputEvent_t &event );
	void UpdateButtonState( const InputEvent_t &event );
	void PostInit();

	CreateInterfaceFn GetGameUIFactory()
	{
		return m_GameUIFactory;
	}
	
	// handlers for game UI (main menu)
	void ActivateGameUI();
	bool HideGameUI();
	bool IsGameUIVisible();

	// console
	void ShowConsole();
	void HideConsole();
	bool IsConsoleVisible();
	void ClearConsole();

	// level loading
	void OnLevelLoadingStarted( char const *levelName, bool bLocalServer );
	void OnLevelLoadingFinished();
	void NotifyOfServerConnect(const char *game, int IP, int connectionPort);
	void NotifyOfServerDisconnect();
	void UpdateProgressBar(LevelLoadingProgress_e progress, bool showDialog = true );
	void UpdateCustomProgressBar( float progress, const wchar_t *desc );
	void StartCustomProgress();
	void FinishCustomProgress();
	void UpdateSecondaryProgressBarWithFile( float progress, const char *pDesc, int nBytesTotal );
	void UpdateSecondaryProgressBar( float progress, const wchar_t *desc );
	void StartLoadingScreenForCommand( const char* command );
	void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	void EnabledProgressBarForNextLoad()
	{
		m_bShowProgressDialog = true;
	}

	// Should pause?
	bool ShouldPause();
	void ShowErrorMessage();

	void SetNotAllowedToHideGameUI( bool bNotAllowedToHide )
	{
		m_bNotAllowedToHideGameUI = bNotAllowedToHide;
	}

	void SetNotAllowedToShowGameUI( bool bNotAllowedToShow )
	{
		m_bNotAllowedToShowGameUI = bNotAllowedToShow;
	}

	void HideLoadingPlaque( void )
	{
		if ( scr_drawloading )
		{
			OnLevelLoadingFinished();
			S_OnLoadScreen( false );
		}

		S_PreventSound(false);//it is now safe to use audio again.

		scr_disabled_for_loading = false;
		scr_drawloading = false;
	}

	void SetGameDLLPanelsVisible( bool show )
	{
	}

	// Allows the level loading progress to show map-specific info
	void SetProgressLevelName( const char *levelName );

	void OnToolModeChanged( bool bGameMode );
	InputContextHandle_t GetGameUIInputContext() { return m_hGameUIInputContext; }

	void NeedConnectionProblemWaitScreen();
	void ShowPasswordUI( char const *pchCurrentPW );

	void SetProgressBias( float bias );
	void UpdateProgressBar( float progress, const char *pszDesc = nullptr, bool showDialog = true );

	bool IsPlayingFullScreenVideo();

public:
	void SetEngineVisible( bool state );

	void Simulate();

	// debug overlays
	void HideDebugSystem();

	bool IsShiftKeyDown();
	bool IsAltKeyDown();
	bool IsCtrlKeyDown();

public:
	enum { MAX_NUM_FACTORIES = 5 };
	CreateInterfaceFn m_FactoryList[MAX_NUM_FACTORIES];
	int m_iNumFactories;

	CSysModule *m_hStaticGameUIModule;
	CreateInterfaceFn m_GameUIFactory;

	bool m_bGameUIVisible;

	// progress bar
	bool m_bShowProgressDialog;
	LevelLoadingProgress_e m_eLastProgressPoint;

	// progress bar debugging
	int m_nLastProgressPointRepeatCount;
	double m_flLoadingStartTime;
	struct LoadingProgressEntry_t
	{
		double flTime;
		LevelLoadingProgress_e eProgress;
	};
	std::vector< LoadingProgressEntry_t > m_LoadingProgress;

	bool					m_bSaveProgress : 1;
	// game ui hiding control
	bool					m_bNotAllowedToHideGameUI : 1;
	bool					m_bNotAllowedToShowGameUI : 1;

	// used to start the progress from an arbitrary position
	float					m_ProgressBias;

	InputContextHandle_t m_hGameUIInputContext;

	IMaterial *m_pConstantColorMaterial;
};

#include "facade/iengineui.inl"



//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CEngineUI g_EngineUIImp;
static InterfaceReg s_Reg_CEngineUI_IEngineUI( []() -> void * { return static_cast<IEngineUI *>( &g_EngineUIImp ); }, VENGINE_UI_VERSION );;

IEngineUIInternal *EngineUI()
{
	return &g_EngineUIImp;
}

//-----------------------------------------------------------------------------
// The loader progress is updated by the queued loader. It uses an initial
// reserved portion of the bar.
//-----------------------------------------------------------------------------
constexpr auto PROGRESS_RESERVE = 0.50f;

	
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngineUI::CEngineUI()
{
	m_bGameUIVisible = false;

	m_hGameUIInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	m_hStaticGameUIModule = nullptr;
	m_GameUIFactory = nullptr;
	
	m_bShowProgressDialog = false;
	m_bSaveProgress = false;
	m_bNotAllowedToHideGameUI = false;
	m_bNotAllowedToShowGameUI = false;
	m_ProgressBias = 0;
	m_pConstantColorMaterial = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEngineUI::~CEngineUI()
{
}


//-----------------------------------------------------------------------------
// add all the base search paths used by VGUI (platform, skins directory, language dirs)
//-----------------------------------------------------------------------------
bool CEngineUI::SetVGUIDirectories()
{
	// add vgui skins directory last

	return true;
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
void CEngineUI::Init()
{
	const char *szDllName = "";
	
	if ( CommandLine()->FindParm( "-gameuidll" ) )
	{
		COM_TimestampedLog( "Loading gameui.dll" );

		// load the GameUI dll
		szDllName = "gameui";
		m_hStaticGameUIModule = g_pFileSystem->LoadModule(szDllName, "GAMEBIN", true); // LoadModule() does a GetLocalCopy() call
		m_GameUIFactory = Sys_GetFactory(m_hStaticGameUIModule);
		if ( !m_GameUIFactory )
		{
			Error( "Could not load: %s\n", szDllName );
		}
	}
	else
	{
		// Get the gameui interfaces from client.dll
		extern CreateInterfaceFn g_ClientFactory;
		m_GameUIFactory = g_ClientFactory;
		szDllName = "client";
	}
	
	// get the initialization func
	staticGameUIFuncs = (IGameUI *)m_GameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr);
	if (!staticGameUIFuncs )
	{
		Error( "Could not get IGameUI interface %s from %s\n", GAMEUI_INTERFACE_VERSION, szDllName );
	}

	// Create UI Input contexts
	// NOTE: The GameUI context may or may not be used by the client
	// so we'll start it out disabled
	m_hGameUIInputContext = g_pInputStackSystem->PushInputContext();
	g_pInputStackSystem->EnableInputContext( m_hGameUIInputContext, false );


	colorcorrectiontools->Init();

	COM_TimestampedLog( "materials->CacheUsedMaterials()" );

	// This material is used by CPotteryWheelPanel::Paint() to copy stencil to the render target's alpha. Not sure of the best place to put it, but this needs to be done sometime before the used materials are precached.
	m_pConstantColorMaterial = materials->FindMaterial( "dev/constant_color", TEXTURE_GROUP_OTHER, true );
	if ( m_pConstantColorMaterial )
	{
		m_pConstantColorMaterial->IncrementReferenceCount();
	}
		
	// Make sure that these materials are in the materials cache
	materials->CacheUsedMaterials();

	COM_TimestampedLog( "g_pLocalize->AddFile" );


	// load the base localization file
	g_pLocalize->AddFile( "resource/valve_%language%.txt" );

	char szFileName[MAX_PATH];

	// We also want to load the localization file for the base game.  Nomrally, all these values would already be in valve_language.txt, but
	// with CSGO we decided to move them into csgo_language (which is NOT a mod).
	V_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", GetCurrentGame() );
	szFileName[ sizeof( szFileName ) - 1 ] = '\0';
	g_pLocalize->AddFile( szFileName );


	// don't need to load the "valve" localization file twice
	// Each mod can have its own language.txt in addition to the valve_%%langauge%%.txt file under defaultgamedir.
	// load mod-specific localization file for kb_act.lst, user.scr, settings.scr, etc.
	V_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", GetCurrentMod() );
	szFileName[ sizeof( szFileName ) - 1 ] = '\0';
	g_pLocalize->AddFile( szFileName );

	// Load a low-violence-specific string file to override strings in the mod string file
	if ( g_bLowViolence )
	{
		V_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%_lv.txt", GetCurrentMod() );
		szFileName[ sizeof( szFileName ) - 1 ] = '\0';
		g_pLocalize->AddFile( szFileName );
	}

	COM_TimestampedLog( "staticGameUIFuncs->Initialize" );

	staticGameUIFuncs->Initialize( g_GameSystemFactory );

	COM_TimestampedLog( "staticGameUIFuncs->Start" );
	staticGameUIFuncs->Start();


	// show the game UI
	COM_TimestampedLog( "ActivateGameUI()" );
	ActivateGameUI();

	if ( !CommandLine()->CheckParm( "-forcestartupmenu" ) &&
		!CommandLine()->CheckParm( "-hideconsole" ) &&
		( CommandLine()->FindParm( "-toconsole" ) || CommandLine()->FindParm( "-console" ) || CommandLine()->FindParm( "-rpt" ) || CommandLine()->FindParm( "-allowdebug" ) ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "rocket_console_show\n" );
	}

}

void CEngineUI::PostInit()
{
	staticGameUIFuncs->PostInit();
}

//-----------------------------------------------------------------------------
// Purpose: connects interfaces in gameui
//-----------------------------------------------------------------------------
void CEngineUI::Connect()
{
	staticGameUIFuncs->Connect( g_GameSystemFactory );

//		g_pLauncherMgr = (ILauncherMgr *)g_GameSystemFactory(  LINUXMGR_INTERFACE_VERSION, NULL );	
}

//-----------------------------------------------------------------------------
// Are we initialized?
//-----------------------------------------------------------------------------
bool CEngineUI::IsInitialized() const
{
	return staticGameUIFuncs != nullptr;
}

extern bool g_bUsingLegacyAppSystems;
//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CEngineUI::Shutdown()
{
	if ( m_pConstantColorMaterial )
	{
		m_pConstantColorMaterial->DecrementReferenceCount();
		m_pConstantColorMaterial = nullptr;
	}

	colorcorrectiontools->Shutdown();

	demoaction->Shutdown();

	if ( g_PluginManager )
	{
		g_PluginManager->Shutdown();
	}

	// unload the gameUI
	staticGameUIFuncs->Shutdown();
	staticGameUIFuncs = nullptr;

	// Disable the input contexts
	if ( m_hGameUIInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->PopInputContext(); // GameUI
		m_hGameUIInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	}

	// unload the dll
	if ( m_hStaticGameUIModule )
	{
		Sys_UnloadModule(m_hStaticGameUIModule);
	}

	m_hStaticGameUIModule = nullptr;
	m_GameUIFactory = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Toggle engine panel active/inactive
//-----------------------------------------------------------------------------
void CEngineUI::SetEngineVisible( bool state )
{
}


//-----------------------------------------------------------------------------
// Should pause?
//-----------------------------------------------------------------------------
bool CEngineUI::ShouldPause()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Shows any GameUI related panels
//-----------------------------------------------------------------------------
void CEngineUI::ActivateGameUI()
{
	if ( m_bNotAllowedToShowGameUI )
		return;

	if (!staticGameUIFuncs)
		return;

	// clear any keys that might be stuck down
	ClearIOStates();

	m_bGameUIVisible = true;

	SetEngineVisible( false );

	staticGameUIFuncs->OnGameUIActivated();
	
//Reapplying this hack so that the game doesn't pause when the player opens up the menu.
//This existed initially but was removed with the Portal 2 integration.
	if ( sv.IsPlayingSoloAgainstBots() )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "pause\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides an Game UI related features (not client UI stuff tho!)
//-----------------------------------------------------------------------------
bool CEngineUI::HideGameUI()
{
	if ( m_bNotAllowedToHideGameUI )
		return false;

	if (!staticGameUIFuncs)
		return false;

	const char *levelName = engineClient->GetLevelName();
	bool bInNonBgLevel = levelName && levelName[0] && !engineClient->IsLevelMainMenuBackground();
	if ( bInNonBgLevel )
	{
		m_bGameUIVisible = false;

		SetEngineVisible( true );

		staticGameUIFuncs->OnGameUIHidden();
	}

	// Tracker 18820:  Pulling up options/console was perma-pausing the background levels, now we
	//  unpause them when you hit the Esc key even though the UI remains...
	if ( levelName && 
		 levelName[0] && 
		 ( ( engineClient->GetMaxClients() <= 1 ) || sv.IsPlayingSoloAgainstBots() ) && 
		 engineClient->IsPaused() )
	{
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "unpause\n");
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game console (but not the complete GameUI!)
//-----------------------------------------------------------------------------
void CEngineUI::HideConsole()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "rocket_console_hide\n" );
}

//-----------------------------------------------------------------------------
// Purpose: shows the console
//-----------------------------------------------------------------------------
void CEngineUI::ShowConsole()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "rocket_console_show\n" );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently open
//-----------------------------------------------------------------------------
bool CEngineUI::IsConsoleVisible()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: clears all text from the console
//-----------------------------------------------------------------------------
void CEngineUI::ClearConsole()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "rocket_console_clear\n" );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CEngineUI::IsGameUIVisible() 
{
	return m_bGameUIVisible;
}


// list of progress bar strings
struct LoadingProgressDescription_t
{
	LevelLoadingProgress_e eProgress;	// current progress
	int nPercent;						// % of the total time this is at
	int nRepeat;						// number of times this is expected to repeat (usually 0)
	const char *pszDesc;				// user description of progress
};

LoadingProgressDescription_t g_ListenServerLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		nullptr },
	{ PROGRESS_SPAWNSERVER,					5,		0,		"#LoadingProgress_SpawningServer" },
	{ PROGRESS_LOADWORLDMODEL,				8,		5,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_CREATENETWORKSTRINGTABLES,	12,		0,		nullptr },
	{ PROGRESS_PRECACHEWORLD,				15,		0,		"#LoadingProgress_PrecacheWorld" },
	{ PROGRESS_CLEARWORLD,					16,		20,		nullptr },
	{ PROGRESS_LEVELINIT,					20,		200,	"#LoadingProgress_LoadResources" },
	{ PROGRESS_ACTIVATESERVER,				50,		0,		nullptr },
	{ PROGRESS_SIGNONCHALLENGE,				51,		0,		"#LoadingProgress_Connecting" },
	{ PROGRESS_SIGNONCONNECT,				55,		0,		nullptr },
	{ PROGRESS_SIGNONCONNECTED,				56,		1,		"#LoadingProgress_SignonLocal" },
	{ PROGRESS_PROCESSSERVERINFO,			58,		0,		nullptr },
	{ PROGRESS_PROCESSSTRINGTABLE,			60,		3,		nullptr },	// 16
	{ PROGRESS_SIGNONNEW,					63,		200,	nullptr },
	{ PROGRESS_SENDCLIENTINFO,				80,		1,		nullptr },
	{ PROGRESS_SENDSIGNONDATA,				81,		1,		"#LoadingProgress_SignonDataLocal" },
	{ PROGRESS_SIGNONSPAWN,					83,		10,		nullptr },
	{ PROGRESS_CREATEENTITIES,				85,		3,		nullptr },
	{ PROGRESS_FULLYCONNECTED,				86,		0,		nullptr },
	{ PROGRESS_PRECACHELIGHTING,			87,		50,		nullptr },
	{ PROGRESS_READYTOPLAY,					95,		100,	nullptr },
	{ PROGRESS_HIGHESTITEM,					100,	0,		nullptr },
};

LoadingProgressDescription_t g_RemoteConnectLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		nullptr },
	{ PROGRESS_CHANGELEVEL,					1,		0,		"#LoadingProgress_Changelevel" },
	{ PROGRESS_BEGINCONNECT,				5,		0,		"#LoadingProgress_BeginConnect" },
	{ PROGRESS_SIGNONCHALLENGE,				10,		0,		"#LoadingProgress_Connecting" },
	{ PROGRESS_SIGNONCONNECTED,				11,		0,		nullptr },
	{ PROGRESS_PROCESSSERVERINFO,			12,		0,		"#LoadingProgress_ProcessServerInfo" },
	{ PROGRESS_PROCESSSTRINGTABLE,			15,		3,		nullptr },
	{ PROGRESS_LOADWORLDMODEL,				20,		14,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_SIGNONNEW,					30,		200,	"#LoadingProgress_PrecacheWorld" },
	{ PROGRESS_SENDCLIENTINFO,				60,		1,		"#LoadingProgress_SendClientInfo" },
	{ PROGRESS_SENDSIGNONDATA,				64,		1,		"#LoadingProgress_SignonData" },
	{ PROGRESS_SIGNONSPAWN,					65,		10,		nullptr },
	{ PROGRESS_CREATEENTITIES,				85,		3,		nullptr },
	{ PROGRESS_FULLYCONNECTED,				86,		0,		nullptr },
	{ PROGRESS_PRECACHELIGHTING,			87,		50,		nullptr },
	{ PROGRESS_READYTOPLAY,					95,		100,	nullptr },
	{ PROGRESS_HIGHESTITEM,					100,	0,		nullptr },
};

static LoadingProgressDescription_t *g_pLoadingProgressDescriptions = nullptr;

//-----------------------------------------------------------------------------
// Purpose: returns current progress point description
//-----------------------------------------------------------------------------
LoadingProgressDescription_t &GetProgressDescription(LevelLoadingProgress_e eProgress)
{
	// search for the item in the current list
	int i = 0;
	while ( true )
	{
		// find the closest match
		if (g_pLoadingProgressDescriptions[i].eProgress >= eProgress)
			return g_pLoadingProgressDescriptions[i];
	
		if ( g_pLoadingProgressDescriptions[i].eProgress == PROGRESS_HIGHESTITEM )
			break;

		++i;
	}

	// not found
	return g_pLoadingProgressDescriptions[0];
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineUI::OnLevelLoadingStarted( char const *levelName, bool bLocalServer )
{
	if (!staticGameUIFuncs)
		return;

	ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
	if ( pSyncReportConVar )
	{
		// If convar is set to 2, suppress warnings during level load
		g_syncReportLevel = pSyncReportConVar->GetInt();
		if ( g_syncReportLevel > 1 )
		{
			pSyncReportConVar->SetValue( 0 );
		}
	}
	
	// TCR requirement, always!!!
	m_bShowProgressDialog = true;

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted( levelName, m_bShowProgressDialog );

	// reset progress bar timers
	m_flLoadingStartTime = Plat_FloatTime();
	m_LoadingProgress.clear();
	m_eLastProgressPoint = PROGRESS_NONE;
	m_nLastProgressPointRepeatCount = 0;
	m_ProgressBias = 0;

	// choose which progress bar to use
	if ( !bLocalServer )
	{
		// we're connecting
		g_pLoadingProgressDescriptions = g_RemoteConnectLoadingProgressDescriptions;
	}
	else
	{
		g_pLoadingProgressDescriptions = g_ListenServerLoadingProgressDescriptions;
	}

	if ( m_bShowProgressDialog )
	{
		ActivateGameUI();
	}

	m_bShowProgressDialog = false;
}

void CEngineUI::StartLoadingScreenForCommand( const char* command )
{
	staticGameUIFuncs->StartLoadingScreenForCommand( command );
}

void CEngineUI::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
	staticGameUIFuncs->StartLoadingScreenForKeyValues( keyValues );
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineUI::OnLevelLoadingFinished()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;


	// Restore convar setting after level load
	if ( g_syncReportLevel > 1 )
	{
		ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		if ( pSyncReportConVar )
		{
			pSyncReportConVar->SetValue( g_syncReportLevel );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineUI::ShowErrorMessage()
{
	if (!staticGameUIFuncs || !gfExtendedError)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

	HideGameUI();
}


//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
constexpr auto LOADING_PRESENT_UPDATE_INTERVAL = 0.05f;
static double g_flLastUpdateTime = 0.0f;
void CEngineUI::UpdateProgressBar( LevelLoadingProgress_e progress, bool showDialog )
{
	if (!staticGameUIFuncs)
		return;

	if ( !ThreadInMainThread() )
		return;

	if (!g_pLoadingProgressDescriptions)
		return;

	// don't go backwards
	if (progress < m_eLastProgressPoint)
		return;

	bool bNewCheckpoint = progress != m_eLastProgressPoint;

	// Early time-based throttle for UpdateProgressBar
	double t = Plat_FloatTime();
    float dt = t - g_flLastUpdateTime;
    if ( ( !bNewCheckpoint && ( dt < LOADING_PRESENT_UPDATE_INTERVAL ) ) || 
		g_pMaterialSystem->IsInFrame() )
	{
		return;
	}

	// count progress repeats
	if ( !bNewCheckpoint )
	{				         
		++m_nLastProgressPointRepeatCount;
	}
	else
	{
		m_nLastProgressPointRepeatCount = 0;

	}

	// construct a string describing it
	LoadingProgressDescription_t &desc = GetProgressDescription(progress);

	// calculate partial progress
	float flPerc = desc.nPercent / 100.0f;
	if ( desc.nRepeat > 1 && m_nLastProgressPointRepeatCount )
	{
		// cap the repeat count
		m_nLastProgressPointRepeatCount = std::min(m_nLastProgressPointRepeatCount, desc.nRepeat);

		// next progress point
		float flNextPerc = GetProgressDescription((LevelLoadingProgress_e)((int)progress + 1)).nPercent / 100.0f;

		// move along partially towards the next tick
		flPerc += (flNextPerc - flPerc) * ((float)m_nLastProgressPointRepeatCount / desc.nRepeat);
	}

	// the bias allows the loading bar to have an optional reserved initial band
	// isolated from the normal progress descriptions
	flPerc = flPerc * ( 1.0f - m_ProgressBias ) + m_ProgressBias;

	// Send loading progress to the server
	GetBaseLocalClient().SendLoadingProgress( (int)(flPerc * 100) );

	UpdateProgressBar( flPerc, desc.pszDesc, showDialog );

	// Help with profiling load times.
	ETWMarkPrintf( "UpdateProgressBar to %s, stage %d, took %1.3f s", desc.pszDesc, progress, Plat_FloatTime() - g_flLastUpdateTime );

	m_eLastProgressPoint = progress;

	// NOTE: It is necessary to re-read time, since Refresh
	// may block, and if it does, it'll force a refresh every allocation
	// if we don't resample time after the block
	g_flLastUpdateTime = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
void CEngineUI::UpdateCustomProgressBar( float progress, const wchar_t *desc )
{
	char ansi[1024];
	g_pLocalize->ConvertUnicodeToANSI( desc, ansi, sizeof( ansi ) );
	UpdateProgressBar( progress, ansi );
}

void CEngineUI::StartCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted( nullptr, true );
	m_bSaveProgress = staticGameUIFuncs->SetShowProgressText( true );
}

void CEngineUI::FinishCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->SetShowProgressText( m_bSaveProgress );
	staticGameUIFuncs->OnLevelLoadingFinished( false, "", "" );
}

void CEngineUI::SetProgressBias( float bias )
{
	m_ProgressBias = bias;
}

void CEngineUI::UpdateProgressBar( float progress, const char *pDesc, bool showDialog )
{
	if ( !staticGameUIFuncs )
		return;

	bool bUpdated = staticGameUIFuncs->UpdateProgressBar( progress, pDesc ? pDesc : "", showDialog );
	if ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( false ) )
	{
		while ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( true ) )
		{
			extern void V_RenderUIOnly();
			V_RenderUIOnly();

			if ( g_ClientGlobalVariables.frametime != 0.0f && g_ClientGlobalVariables.frametime != 0.1f)
			{
				if ( g_pRocketUI )
					g_pRocketUI->RunFrame( g_ClientGlobalVariables.realtime );
			}
			else
			{
				break;
			}
		}
	}
	else if ( bUpdated )
	{
		if ( g_pRocketUI )
			g_pRocketUI->RunFrame( g_ClientGlobalVariables.realtime );
		// re-render vgui on screen
		extern void V_RenderUIOnly();
		V_RenderUIOnly();
	}
}

void CEngineUI::UpdateSecondaryProgressBarWithFile( float progress, const char *pPath, int nBytesTotal )
{
	if ( !pPath )
		return;

	char szFile[MAX_PATH];
	V_strcpy_safe( szFile, pPath );
	//V_StripFilename(szFile);
	wchar_t wszPercent[ 10 ];
	V_snwprintf( wszPercent, std::size( wszPercent ), L"%d%%",  (int)(100*progress) );
	wchar_t wszMegs[ 64 ];
	
	char szBytes[32];
	V_strcpy_safe( szBytes, V_pretifymem(nBytesTotal) );
	V_strtowcs( szBytes, -1, wszMegs, std::size( wszMegs ) );

	wchar_t wszFile[ MAX_PLAYER_NAME_LENGTH ];
	g_pLocalize->ConvertANSIToUnicode( szFile, wszFile, sizeof( wszFile ) );
	wchar_t szWideBuff[ 256 ];
	g_pLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pLocalize->Find( "#SFUI_DownLoading_" ), 3, wszFile, wszPercent, wszMegs );
	UpdateSecondaryProgressBar( progress, szWideBuff );
}

void CEngineUI::UpdateSecondaryProgressBar( float progress, const wchar_t *desc )
{
	if ( !staticGameUIFuncs )
		return;

	bool bUpdated = staticGameUIFuncs->UpdateSecondaryProgressBar( progress, desc ? desc : L"" );
	if ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( false ) )
	{
		while ( staticGameUIFuncs->LoadingProgressWantsIsolatedRender( true ) )
		{
			extern void V_RenderUIOnly();
			V_RenderUIOnly();

			if ( g_ClientGlobalVariables.frametime != 0.0f && g_ClientGlobalVariables.frametime != 0.1f)
			{
				if ( g_pRocketUI )
					g_pRocketUI->RunFrame( g_ClientGlobalVariables.realtime );
			}
			else
			{
				break;
			}
		}
	}
	else if ( bUpdated )
	{
		if ( g_pRocketUI )
			g_pRocketUI->RunFrame( g_ClientGlobalVariables.realtime );
		// re-render vgui on screen
		extern void V_RenderUIOnly();
		V_RenderUIOnly();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
void CEngineUI::UpdateButtonState( const InputEvent_t &event )
{
}

		
//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
bool CEngineUI::Key_Event( const InputEvent_t &event )
{
	bool bDown = ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked );
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( IsShiftKeyDown() )
	{
		switch( code )
		{
		case KEY_F1:
			if ( bDown )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "debugsystemui" );
			}
			return true;

		case KEY_F2:
			if ( bDown )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "demoui" );
			}
			return true;
		}
	}

			   
	// ESCAPE toggles game ui
	bool isConsole = false;
	ButtonCode_t uiToggleKey = isConsole ? KEY_XBUTTON_START : KEY_ESCAPE;
	ButtonCode_t baseButtonCode = GetBaseButtonCode( code );

	// make sure PS3 supports the console default press (START) or ESCAPE
	bool isUIToggleKey = ( baseButtonCode == uiToggleKey ) ;

	// Hitting the Start button on any xbox controller brings up the game ui, so translate to base code space
	if ( bDown && isUIToggleKey )
	{
		if ( cv_console_window_open.GetBool() )
		{
			HideConsole();
		}
		else if ( IsGameUIVisible()  )
		{
			// Don't allow hiding of the game ui if there's no level
			const char *pLevelName = engineClient->GetLevelName();
			if ( pLevelName && pLevelName[0] )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_hide" );
			}
		}
		else if ( !cv_ignore_ui_activate_key.GetBool() )
		{
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
		}
		return true;
	}

	return false;
}

void CEngineUI::Simulate()
{

	{
		CVProfScope _( "CEngineUI::Simulate", 0, "UI_Simulate", false, BUDGETFLAG_OTHER );

		int w = 0, h = 0;
		// Pixel size of the drawable: this feeds the UI viewport, which must match the
		// backbuffer. (Was ILauncherMgr::RenderedSize(), whose setter nobody ever called,
		// so this had been silently setting a 0x0 viewport every frame.)
		const ks::desktop::size pixels = ks::desktop::window_pixel_size( ks::desktop::game_window() );
		w = pixels.width;
		h = pixels.height;
		// don't hold this reference over RunFrame()
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->Viewport( 0, 0, w, h );
		}

		staticGameUIFuncs->RunFrame();

		UI_ActivateMouse();
	}
}

void CEngineUI::HideDebugSystem( void )
{
}

bool CEngineUI::IsShiftKeyDown( void )
{
	return g_pInputSystem && ( g_pInputSystem->IsButtonDown( KEY_LSHIFT ) || g_pInputSystem->IsButtonDown( KEY_RSHIFT ) );
}

bool CEngineUI::IsAltKeyDown( void )
{
	return g_pInputSystem && ( g_pInputSystem->IsButtonDown( KEY_LALT ) || g_pInputSystem->IsButtonDown( KEY_RALT ) );
}

bool CEngineUI::IsCtrlKeyDown( void )
{
	return g_pInputSystem && ( g_pInputSystem->IsButtonDown( KEY_LCONTROL ) || g_pInputSystem->IsButtonDown( KEY_RCONTROL ) );
}


//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineUI::NotifyOfServerConnect(const char *game, int IP, int connectionPort)
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnConnectToServer2(game, IP, connectionPort);
}

//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineUI::NotifyOfServerDisconnect()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnDisconnectFromServer();
}

//-----------------------------------------------------------------------------
// A helper to play sounds through vgui
//-----------------------------------------------------------------------------
void UI_PlaySound( const char *pFileName )
{
	//Put '+' on the front of sounds to make them play without spatialization.
	char buf[2048];
	V_snprintf( buf, sizeof( buf ), "%c%s", '+', pFileName );

	// Point at origin if they didn't specify a sound source.
	Vector vDummyOrigin;
	vDummyOrigin.Init();

	CSfxTable *pSound = (CSfxTable*)S_PrecacheSound( buf );
	if ( pSound )
	{
		S_MarkUISound( pSound );

		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = GetLocalClient().GetViewEntity();
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSound;
		params.origin = vDummyOrigin;
		params.pitch = PITCH_NORM;
		params.soundlevel = SNDLVL_IDLE;
		params.flags = 0;
		params.fvol = 1.0f;

		S_StartSound( params );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UI_ActivateMouse()
{
	if ( !g_ClientDLL )
		return;

	// Don't mess with mouse if not active
	if ( !game->IsActiveApp() )
	{
		g_ClientDLL->IN_DeactivateMouse ();
		return;
	}
		
	// Mouse look is off exactly while an interactive UI document is on screen. Ask
	// RocketUI, which derives that from the documents themselves and so cannot go stale.
	// The engine's GameUI flag is not consulted: nothing draws a VGUI game UI in this
	// fork (CGameUI::OnGameUIActivated only shows RmlUi documents), so it is a pause/state
	// bit that any panel could leave set -- which used to kill mouse look until ESC. It is
	// the fallback only when RocketUI is absent (tools, dedicated).
	if ( g_pRocketUI ? g_pRocketUI->IsConsumingInput() : EngineUI()->IsGameUIVisible() )
	{
		g_ClientDLL->IN_DeactivateMouse();
		return;
	}

	if ( !g_bTextMode )
	{
		g_ClientDLL->IN_ActivateMouse ();
	}
	else
	{
		g_ClientDLL->IN_DeactivateMouse ();
	}
}

void UI_SetGameDLLPanelsVisible( bool show )
{
	EngineUI()->SetGameDLLPanelsVisible( show );
}

void CEngineUI::SetProgressLevelName( const char *levelName )
{
//	staticGameUIFuncs->SetProgressLevelName( levelName );
}

void CEngineUI::OnToolModeChanged( bool bGameMode )
{
}

void CEngineUI::NeedConnectionProblemWaitScreen()
{
	return staticGameUIFuncs->NeedConnectionProblemWaitScreen();
}

void CEngineUI::ShowPasswordUI( char const *pchCurrentPW )
{
//	staticGameUIFuncs->ShowPasswordUI( pchCurrentPW );
}

bool CEngineUI::IsPlayingFullScreenVideo()
{
	return staticGameUIFuncs->IsPlayingFullScreenVideo();
}
