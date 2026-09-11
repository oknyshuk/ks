//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#undef PROTECTED_THINGS_ENABLE
#include <SDL3/SDL.h>


	#include "tier0/dynfunction.h"
#include "appframework/ilaunchermgr.h"
#include "appframework/sdlwindow.h"

#include "igame.h"
#include "cl_main.h"
#include "host.h"
#include "quakedef.h"
#include "tier0/icommandline.h"
#include "ivideomode.h"
#include "gl_matsysiface.h"
#include "materialsystem/materialsystem_config.h"
#include "cdll_engine_int.h"
#include "engineui.h"
#include "iengine.h"
#include "avi/iavi.h"
#include "keys.h"
#include "tier3/tier3.h"
#include "sound.h"
#include "sys_dll.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "GameUI/IGameUI.h"
#include "sv_main.h"
#if defined( BINK_VIDEO )
#include "bink/bink.h"
#endif
#include "inputsystem/iinputstacksystem.h"
#include "avi/ibik.h"
#include "materialsystem/imaterial.h"
#include "characterset.h"
#include "server.h"

#include "rocketui/rocketui.h"

#include "localize/ilocalize.h"


#include "snd_dev_sdl.h"

#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"

#include "tier1/fmtstr.h"

#include "cl_steamauth.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cv_uipanel_active;

void S_BlockSound (void);
void S_UnblockSound (void);
void ClearIOStates( void );


//-----------------------------------------------------------------------------
// Game input events
//-----------------------------------------------------------------------------
enum GameInputEventType_t
{
	IE_WindowMove = IE_FirstAppEvent,
	IE_AppActivated,
};

//-----------------------------------------------------------------------------
// Purpose: Main game interface, including message pump and window creation
//-----------------------------------------------------------------------------
class CGame : public IGame
{
public:
					CGame( void );
	virtual			~CGame( void );

	bool			Init( void *pvInstance );
	bool			Shutdown( void );

	bool			CreateGameWindow( void );
	void			DestroyGameWindow();
	void			SetGameWindow( void* hWnd );

	// This is used in edit mode to override the default wnd proc associated w/
	bool			InputAttachToGameWindow();
	void			InputDetachFromGameWindow();

	void			PlayStartupVideos( void );

	void*			GetMainWindow( void );
	void**			GetMainWindowAddress( void );

	void			GetDesktopInfo( int &width, int &height, int &refreshrate );


	void			SetWindowXY( int x, int y );
	void			SetWindowSize( int w, int h );
	void			GetWindowRect( int *x, int *y, int *w, int *h );

	bool			IsActiveApp( void );

	void			SetCanPostActivateEvents( bool bEnable );
	bool			CanPostActivateEvents();

	virtual void    OnScreenSizeChanged( int nOldWidth, int nOldHeight );

public:
	void			SetMainWindow( HWND window );
	void			SetActiveApp( bool active );
	// plays a video file and waits until completed. Can be interrupted by user input.
	virtual void	PlayVideoListAndWait( const char *szVideoFileList, bool bNeedHealthWarning = false );
	virtual void	PlayVideoAndWait(const char *filename, bool bNeedHealthWarning = false);

// Message handlers.
public:
	void	HandleMsg_WindowMove( const InputEvent_t &event );
	void	HandleMsg_ActivateApp( const InputEvent_t &event );
	void	HandleMsg_Close( const InputEvent_t &event );
	void	HandleMsg_WindowSizeChanged( const InputEvent_t &event );

	// Call the appropriate HandleMsg_ function.
	void	DispatchInputEvent( const InputEvent_t &event );

	// Dispatch all the queued up messages.
	virtual void	DispatchAllStoredGameMessages();

	InputContextHandle_t GetInputContext() { return m_hInputContext; }

private:
	void			AppActivate( bool fActive );

private:
	void AttachToWindow();
	void DetachFromWindow();

	static const wchar_t CLASSNAME[];

	bool			m_bExternallySuppliedWindow;

	SDL_Window		*m_hWindow;

	int				m_x;
	int				m_y;
	int				m_width;
	int				m_height;
	bool			m_bActiveApp;
	bool			m_bCanPostActivateEvents;

	int				m_iDesktopWidth, m_iDesktopHeight, m_iDesktopRefreshRate;
	void			UpdateDesktopInformation( HWND hWnd );
	InputContextHandle_t m_hInputContext;
};

static CGame g_Game;
IGame *game = ( IGame * )&g_Game;


const wchar_t CGame::CLASSNAME[] = L"Valve001";

// In VCR playback mode, it sleeps this amount each frame.
int g_iVCRPlaybackSleepInterval = 0;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
bool g_bVCRSingleStep = false;

bool g_bWaitingForStepKeyUp = false;	// Used to prevent it from running frames while you hold the S key down.

bool g_bShowVCRPlaybackDisplay = true;

InputContextHandle_t GetGameInputContext()
{
	return g_Game.GetInputContext();
}

// These are all the windows messages that can change game state.
// See CGame::WindowProc for a description of how they work.
struct GameMessageHandler_t
{
	int	m_nEventType;
	void (CGame::*pFn)( const InputEvent_t &event );
};

GameMessageHandler_t g_GameMessageHandlers[] = 
{
	{ IE_AppActivated,			&CGame::HandleMsg_ActivateApp },
	{ IE_WindowMove,			&CGame::HandleMsg_WindowMove },
	{ IE_Close,					&CGame::HandleMsg_Close },
	{ IE_Quit,					&CGame::HandleMsg_Close },
	{ IE_WindowSizeChanged,		&CGame::HandleMsg_WindowSizeChanged },
};


void CGame::AppActivate( bool fActive )
{
	// If text mode, force it to be active.
	if ( g_bTextMode )
	{
		fActive = true;
	}

	// Don't bother if we're already in the correct state
	if ( IsActiveApp() == fActive )
		return;

	// Don't let video modes changes queue up another activate event
	SetCanPostActivateEvents( false );

#ifndef DEDICATED
	if ( videomode )
	{
		if ( fActive )
		{
			videomode->RestoreVideo();
		}
		else
		{
			videomode->ReleaseVideo();
		}
	}

	if ( host_initialized )
	{
		if ( fActive )
		{
			// Clear keyboard states (should be cleared already but...)
			// UI_ActivateMouse will reactivate the mouse soon.
			ClearIOStates();
			
			UpdateMaterialSystemConfig();
		}
		else
		{
			// Clear keyboard input and deactivate the mouse while we're away.
			ClearIOStates();

			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_DeactivateMouse();
			}
		}
	}
#endif // DEDICATED
	SetActiveApp( fActive );

	// Allow queueing of activation events
	SetCanPostActivateEvents( true );
}

void CGame::HandleMsg_WindowMove( const InputEvent_t &event )
{
	game->SetWindowXY( event.m_nData, event.m_nData2 );
#ifndef DEDICATED
	videomode->UpdateWindowPosition();
#endif
}

void CGame::HandleMsg_ActivateApp( const InputEvent_t &event )
{
	AppActivate( event.m_nData ? true : false );
}

void CGame::HandleMsg_Close( const InputEvent_t &event )
{
	if ( eng->GetState() == IEngine::DLL_ACTIVE )
	{
		eng->SetQuitting( IEngine::QUIT_TODESKTOP );
	}
}

void CGame::HandleMsg_WindowSizeChanged( const InputEvent_t &event )
{
#ifndef DEDICATED
	// Window size changed - this happens on Wayland when moving between displays
	int nNewWidth = event.m_nData;
	int nNewHeight = event.m_nData2;

	if ( nNewWidth > 0 && nNewHeight > 0 && videomode )
	{
		videomode->OnWindowSizeChanged( nNewWidth, nNewHeight );
	}
#endif
}

void CGame::DispatchInputEvent( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	// Handle button events specially, 
	// since we have all manner of crazy filtering going on	when dealing with them
	case IE_ButtonPressed:
	case IE_ButtonDoubleClicked:
	case IE_ButtonReleased:
	case IE_KeyTyped:
	case IE_KeyCodeTyped:
		Key_Event( event );
		break;

	// Broadcast analog values both to VGui & to GameUI
	case IE_AnalogValueChanged:
		{
			if ( g_pRocketUI && g_pRocketUI->HandleInputEvent( event ) )
				break;

			if ( g_ClientDLL && g_ClientDLL->HandleGameUIEvent( event ) )
				break;
		}
		break;

	case IE_OverlayEvent:
		if ( event.m_nData == 1 )
		{
			// Overlay has activated
			if ( !EngineUI()->IsGameUIVisible() && sv.IsActive() && sv.IsSinglePlayerGame() )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
			}
		}
		break;

	default:

		if ( g_pRocketUI && g_pRocketUI->HandleInputEvent( event ) )
			break;

		for ( int i=0; i < ARRAYSIZE( g_GameMessageHandlers ); i++ )
		{
			if ( g_GameMessageHandlers[i].m_nEventType == event.m_nType )
			{
				(this->*g_GameMessageHandlers[i].pFn)( event );
				break;
			}
		}
		break;
	}
}


void CGame::DispatchAllStoredGameMessages()
{
	int nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( int i = 0; i < nEventCount; ++i )
	{
		DispatchInputEvent( pEvents[i] );
	}
}

void VCR_EnterPausedState()
{
	// Turn this off in case they're in single-step mode.
	g_bVCRSingleStep = false;

	Assert( !"Impl me" );
}


//-----------------------------------------------------------------------------
// Purpose: The user has accepted an invitation to a game, we need to detect if 
//			it's our game and restart properly if it is
//-----------------------------------------------------------------------------
void XBX_HandleInvite( DWORD nUserId )
{
}





bool CGame::CreateGameWindow( void )
{
	// get the window name
	char windowName[256];
	windowName[0] = 0;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
	{
		Q_strncpy( windowName, modinfo->GetString("game"), sizeof(windowName) );
	}

	if (!windowName[0])
	{
		Q_strncpy( windowName, "HALF-LIFE 2", sizeof(windowName) );
	}

	if ( IsOpenGL() )
	{
		V_strcat( windowName, " - OpenGL", sizeof( windowName ) );
	}

		V_strcat( windowName, " - Vulkan", sizeof( windowName ) );

#if PIX_ENABLE || defined( PIX_INSTRUMENTATION )
	// PIX_ENABLE/PIX_INSTRUMENTATION is a big slowdown (that should never be checked in, but sometimes is by accident), so add this to the Window title too.
	V_strcat( windowName, " - PIX_ENABLE", sizeof( windowName ) );
#endif

	const char *p = CommandLine()->ParmValue( "-window_name_suffix", "" );
	if ( p && V_strlen( p ) )
	{
		V_strcat( windowName, " - ", sizeof( windowName ) );
		V_strcat( windowName, p, sizeof( windowName ) );
	}
		
	modinfo->deleteThis();
	modinfo = nullptr;

	// Create the window config-correct from the chosen material system mode so
	// it lands at the right resolution / windowed state (matches Win32 structure)
	// instead of an engine->launcher reach-around after creation. The launcher
	// honors sdl_displayindex and FULLSCREEN_DESKTOP steering internally.
	bool bWindowed = true;
	int nWidth = 0;
	int nHeight = 0;
	if ( g_pMaterialSystemConfig )
	{
		bWindowed = g_pMaterialSystemConfig->Windowed();
		nWidth = g_pMaterialSystemConfig->m_VideoMode.m_Width;
		nHeight = g_pMaterialSystemConfig->m_VideoMode.m_Height;
	}

	if ( !g_pLauncherMgr->CreateGameWindow( windowName, bWindowed, nWidth, nHeight, true ) )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}
	
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath( "resource/game-icon.bmp", localPath, sizeof(localPath) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		g_pLauncherMgr->SetApplicationIcon( localPath );
	}
	
	SetMainWindow( ( HWND )GetGameSDLWindow() );

	AttachToWindow( );
	return true;
}


//-----------------------------------------------------------------------------
// Destroys the game window 
//-----------------------------------------------------------------------------
void CGame::DestroyGameWindow()
{
	g_pLauncherMgr->DestroyGameWindow();
}


//-----------------------------------------------------------------------------
// This is used in edit mode to specify a particular game window (created by hammer)
//-----------------------------------------------------------------------------
void CGame::SetGameWindow( void *hWnd )
{
	m_bExternallySuppliedWindow = true;
	SDL_RaiseWindow( (SDL_Window *)hWnd );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGame::AttachToWindow()
{
	if ( !m_hWindow )
		return;

	if ( g_pInputSystem )
	{
		// Attach the input system window proc
		g_pInputSystem->AttachToWindow( (void *)m_hWindow );
		g_pInputSystem->EnableInput( true );
		g_pInputSystem->EnableMessagePump( false );
	}
}

void CGame::DetachFromWindow()
{

	if ( g_pInputSystem )
	{
		// Detach the input system window proc
		g_pInputSystem->EnableInput( false );
		g_pInputSystem->DetachFromWindow( );
	}

}


//-----------------------------------------------------------------------------
// This is used in edit mode to override the default wnd proc associated w/
// the game window specified in SetGameWindow. 
//-----------------------------------------------------------------------------
bool CGame::InputAttachToGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return true;

	AttachToWindow();

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( true );

	Assert( !"Impl me" );
	return false;
	return true;
}

void CGame::InputDetachFromGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return;

	Assert( !"Impl me" );

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( false );

	DetachFromWindow();
}

void CGame::PlayStartupVideos( void )
{
	if ( Plat_IsInBenchmarkMode() )
		return;

#ifndef DEDICATED
	// Wait for the mode to change and stabilized
	// FIXME: There's really no way to know when this is completed, so we have to guess a time that will mostly be correct
	if ( videomode->IsWindowedMode() == false )
	{
		ThreadSleep( 1000 );
	}

	bool bEndGame = CommandLine()->CheckParm("-endgamevid") ? true : false;
	bool bRecap = CommandLine()->CheckParm("-recapvid") ? true : false;	// FIXME: This is a temp addition until the movie playback is centralized -- jdw
	bool bNeedHealthWarning = g_pFullFileSystem->FileExists( "media/HealthWarning.txt" );

	if ( !bNeedHealthWarning && 
		!bEndGame && 
		!bRecap && 
		( CommandLine()->CheckParm( "-dev" ) || 
			CommandLine()->CheckParm( "-novid" ) || 
			CommandLine()->CheckParm( "-allowdebug" ) ||
			CommandLine()->CheckParm( "-console" ) ||
			CommandLine()->CheckParm( "-toconsole" ) ) )
		return;

	const char *pszFile = "media/startupvids" PLATFORM_EXT ".txt";
	if ( bEndGame )
	{
		// Don't go back into the map that triggered this.
		CommandLine()->RemoveParm( "+map" );
		CommandLine()->RemoveParm( "+load" );
		
		pszFile = "media/EndGameVids.txt";
	}
	else if ( bRecap )
	{
		pszFile = "media/RecapVids.txt";
	}


	PlayVideoListAndWait( pszFile );


#endif // DEDICATED
}
	

//-----------------------------------------------------------------------------
// Purpose: Tests for players attempting to skip a movie via keypress
//-----------------------------------------------------------------------------
bool UserRequestingMovieSkip( void )
{

	return ( g_pInputSystem->IsButtonDown( KEY_ESCAPE ) || 
			g_pInputSystem->IsButtonDown( KEY_SPACE ) || 
			g_pInputSystem->IsButtonDown( KEY_ENTER ) );
}


void CGame::PlayVideoListAndWait( const char *szVideoFileList, bool bNeedHealthWarning /* = false */ )
{
#ifndef DEDICATED

	CUtlBuffer vidBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szVideoFileList, "GAME", vidBuffer ) )
	{
		return;
	}

	bool CursorStateBak = SDL_CursorVisible();
	SDL_HideCursor();
	


	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );
	char moviePath[MAX_PATH];
	while ( !IsPS3QuitRequested() )
	{
		int nTokenSize = vidBuffer.ParseToken( &breakSet, moviePath, sizeof( moviePath ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		// get the path to the file and play it.
		PlayVideoAndWait( moviePath, bNeedHealthWarning );
	}



	if ( CursorStateBak )
		SDL_ShowCursor();
	else
		SDL_HideCursor();
#endif // DEDICATED
}

//-----------------------------------------------------------------------------
// Plays a Bink video until the video completes or user input cancels
//-----------------------------------------------------------------------------
void CGame::PlayVideoAndWait( const char *filename, bool bNeedHealthWarning )
{
#if defined( BINK_VIDEO )


#endif // BINK_VIDEO
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGame::CGame()
{
	m_x = m_y = 0;
	m_width = m_height = 0;
	m_bActiveApp = false;
	m_bCanPostActivateEvents = true;
	m_iDesktopWidth = 0;
	m_iDesktopHeight = 0;
	m_iDesktopRefreshRate = 0;
	m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGame::~CGame()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGame::Init( void *pvInstance )
{
	m_bExternallySuppliedWindow = false;


	if ( g_pInputStackSystem )
	{
		m_hInputContext = g_pInputStackSystem->PushInputContext();

		// Capture + hide the mouse
		g_pInputStackSystem->SetMouseCapture( m_hInputContext, true );
	}

	return true;
}


bool CGame::Shutdown( void )
{
	if ( m_hInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->PopInputContext();
		m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	}



	return true;
}

void *CGame::GetMainWindow( void )
{
	return (void*)m_hWindow;
}

void** CGame::GetMainWindowAddress( void )
{
	m_hWindow = GetGameSDLWindow();
	return (void**)&m_hWindow;
}

void CGame::GetDesktopInfo( int &width, int &height, int &refreshrate )
{

	width = 1920;
	height = 1080;
	refreshrate = 0;

	// Go through all displays and return the size of the largest.
	// Use SDL_GetDesktopDisplayMode for more reliable resolution info (especially on Wayland).
	int numDisplays = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &numDisplays );
	if ( displays )
	{
		for( int i = 0; i < numDisplays; i++ )
		{
			const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode( displays[i] );

			if ( mode )
			{
				if ( ( mode->w > width ) || ( ( mode->w == width ) && ( mode->h > height ) ) )
				{
					width = mode->w;
					height = mode->h;
					refreshrate = (int)mode->refresh_rate;
				}
			}
		}
		SDL_free( displays );
	}

}

void CGame::UpdateDesktopInformation( HWND hWnd )
{
	// Get the size of the display we will be displayed fullscreen on.
	static ConVarRef sdl_displayindex( "sdl_displayindex" );
	int displayIndex = sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;

	int numDisplays = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &numDisplays );
	SDL_DisplayID displayID = ( displays && displayIndex < numDisplays ) ? displays[displayIndex] : SDL_GetPrimaryDisplay();
	if ( displays )
		SDL_free( displays );

	const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode( displayID );
	if ( mode )
	{
		m_iDesktopWidth = mode->w;
		m_iDesktopHeight = mode->h;
		m_iDesktopRefreshRate = (int)mode->refresh_rate;
	}
}


void CGame::SetMainWindow( HWND window )
{
	m_hWindow = (SDL_Window*)window;

	// update our desktop info (since the results will change if we are going to fullscreen mode)
	if ( !m_iDesktopWidth || !m_iDesktopHeight )
	{
		UpdateDesktopInformation( window );
	}
}

void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
}

void CGame::GetWindowRect( int *x, int *y, int *w, int *h )
{
	if ( x )
	{
		*x = m_x;
	}
	if ( y )
	{
		*y = m_y;
	}
	if ( w )
	{
		*w = m_width;
	}
	if ( h )
	{
		*h = m_height;
	}
}

bool CGame::IsActiveApp( void )
{
	return m_bActiveApp;
}

void CGame::SetCanPostActivateEvents( bool bEnabled )
{
	m_bCanPostActivateEvents = bEnabled;
}

bool CGame::CanPostActivateEvents()
{
	return m_bCanPostActivateEvents;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

void CGame::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnScreenSizeChanged( nOldWidth, nOldHeight );
	}
}

