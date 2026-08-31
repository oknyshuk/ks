//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#if defined( USE_SDL )
#undef PROTECTED_THINGS_ENABLE
#include <SDL3/SDL.h>
#endif

#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
#include "winlite.h"
#endif

#if defined( IS_WINDOWS_PC ) && !defined( USE_SDL )
#include <winsock.h>
#else
	#include "tier0/dynfunction.h"
#endif
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

#if defined( PLATFORM_WINDOWS )
#include "vaudio/ivaudio.h"
extern void VAudioInit();
extern IVAudio * vaudio;
#endif

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
#if defined( WIN32 )
	int				WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
#endif
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

#if USE_SDL
	SDL_Window		*m_hWindow;
#elif defined( WIN32 ) 
	HWND			m_hWindow;
	HINSTANCE		m_hInstance;

	// Stores a wndproc to chain message calls to
	WNDPROC			m_ChainedWindowProc;

	RECT			m_rcLastRestoredClientRect;
#else
#error
#endif

	int				m_x;
	int				m_y;
	int				m_width;
	int				m_height;
	bool			m_bActiveApp;
	bool			m_bCanPostActivateEvents;

	int				m_iDesktopWidth, m_iDesktopHeight, m_iDesktopRefreshRate;
	void			UpdateDesktopInformation( HWND hWnd );
#ifdef WIN32
	void			UpdateDesktopInformation( WPARAM wParam, LPARAM lParam );
#endif
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

#ifdef WIN32
	// This is cheesy, but GetAsyncKeyState is blocked (in protected_things.h) 
	// from being accidentally used, so we get it through it by getting its pointer directly.
	static HINSTANCE hInst = LoadLibrary( "user32.dll" );
	if ( !hInst )
		return;

	typedef SHORT (WINAPI *GetAsyncKeyStateFn)( int vKey );
	static GetAsyncKeyStateFn pfn = (GetAsyncKeyStateFn)GetProcAddress( hInst, "GetAsyncKeyState" );
	if ( !pfn )
		return;

	// In this mode, we enter a wait state where we only pay attention to R and Q.
	while ( 1 )
	{
		if ( pfn( 'R' ) & 0x8000 )
			break;

		if ( pfn( 'Q' ) & 0x8000 )
			TerminateProcess( GetCurrentProcess(), 1 );

		if ( pfn( 'S' ) & 0x8000 )
		{
			if ( !g_bWaitingForStepKeyUp )
			{
				// Do a single step.
				g_bVCRSingleStep = true;
				g_bWaitingForStepKeyUp = true;	// Don't do another single step until they release the S key.
				break;
			}
		}
		else
		{
			// Ok, they released the S key, so we'll process it next time the key goes down.
			g_bWaitingForStepKeyUp = false;
		}
	
		Sleep( 2 );
	}
#else
	Assert( !"Impl me" );
#endif
}

#ifdef WIN32
void VCR_HandlePlaybackMessages( 
	HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam 
	)
{
	if ( uMsg == WM_KEYDOWN )
	{
		if ( wParam == VK_SUBTRACT || wParam == 0xbd )
		{
			g_iVCRPlaybackSleepInterval += 5;
		}
		else if ( wParam == VK_ADD || wParam == 0xbb )
		{
			g_iVCRPlaybackSleepInterval -= 5;
		}
		else if ( toupper( wParam ) == 'Q' )
		{
			TerminateProcess( GetCurrentProcess(), 1 );
		}
		else if ( toupper( wParam ) == 'P' )
		{
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'S' && !g_bVCRSingleStep )
		{
			g_bWaitingForStepKeyUp = true;
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'D' )
		{
			g_bShowVCRPlaybackDisplay = !g_bShowVCRPlaybackDisplay;
		}

		g_iVCRPlaybackSleepInterval = clamp( g_iVCRPlaybackSleepInterval, 0, 500 );
	}
}

//-----------------------------------------------------------------------------
// Calls the default window procedure
// FIXME: It would be nice to remove the need for this, which we can do
// if we can make unicode work when running inside hammer.
//-----------------------------------------------------------------------------
static LONG WINAPI CallDefaultWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: The user has accepted an invitation to a game, we need to detect if 
//			it's our game and restart properly if it is
//-----------------------------------------------------------------------------
void XBX_HandleInvite( DWORD nUserId )
{
}

#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Main windows procedure
//-----------------------------------------------------------------------------
int CGame::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)

{
	LONG			lRet = 0;
	BOOL			bCallDefault = 0;
#if defined( WIN32 )
	HDC				hdc;
	PAINTSTRUCT		ps;

	//
	// NOTE: the way this function works is to handle all messages that just call through to
	// Windows or provide data to it.
	//
	// Any messages that change the engine's internal state (like key events) are stored in a list
	// and processed at the end of the frame. This is necessary for VCR mode to work correctly because
	// Windows likes to pump messages during some of its API calls like SetWindowPos, and unless we add
	// custom code around every Windows API call so VCR mode can trap the wndproc calls, VCR mode can't 
	// reproduce the calls to the wndproc.
	//

	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		return CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
#endif // WIN32

	//
	// Note: NO engine state should be changed in here while in VCR record or playback. 
	// We can send whatever we want to Windows, but if we change its state in here instead of 
	// in DispatchAllStoredGameMessages, the playback may not work because Windows messages 
	// are not deterministic, so you might get different messages during playback than you did during record.
	//
	InputEvent_t event;
	memset( &event, 0, sizeof(event) );
	event.m_nTick = g_pInputSystem->GetPollTick();

	switch ( uMsg )
	{
	case WM_CREATE:
		::SetForegroundWindow( hWnd );
		break;

	case WM_ACTIVATEAPP:
		{
			if ( CanPostActivateEvents() )
			{
				bool bActivated = ( wParam == 1 );
				event.m_nType = IE_AppActivated;
				event.m_nData = bActivated;
				g_pInputSystem->PostUserEvent( event );
			}
			// handle focus changes including fullscreen 
			if ( wParam == 0 )
			{
				S_UpdateWindowFocus( false );
			}
			else
			{
				S_UpdateWindowFocus( true );
			}
		}
		break;

	case WM_POWERBROADCAST:
		// Don't go into Sleep mode when running engine, we crash on resume for some reason (as
		//  do half of the apps I have running usually anyway...)
		if ( wParam == PBT_APMQUERYSUSPEND )
		{
			Msg( "OS requested hibernation, ignoring request.\n" );
			return BROADCAST_QUERY_DENY;
		}

		bCallDefault = true;
		break;

#if defined( WIN32 )
	case WM_SYSCOMMAND:
		if ( ( wParam == SC_MONITORPOWER ) || ( wParam == SC_KEYMENU ) || ( wParam == SC_SCREENSAVE ) )
            return lRet;
    
		if ( wParam == SC_CLOSE ) 
		{
			// handle the close message, but make sure 
			// it's not because we accidently hit ALT-F4
			if ( HIBYTE(GetKeyState(VK_LMENU)) || HIBYTE(GetKeyState(VK_RMENU) ) )
				return lRet;

			Cbuf_Clear( Cbuf_GetCurrentPlayer() );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
		}

#ifndef DEDICATED
		S_BlockSound();
		S_ClearBuffer();
#endif

		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );

#ifndef DEDICATED
		S_UnblockSound();
#endif
		break;
#endif

	case WM_SYS_SHUTDOWNREQUEST:
		Assert( false );
		Cbuf_Clear( Cbuf_GetCurrentPlayer() );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit_gameconsole\n" );
		break;

	case WM_MOVE:
		event.m_nType = IE_WindowMove;
		event.m_nData = (short)LOWORD(lParam);
		event.m_nData2 = (short)HIWORD(lParam);
		g_pInputSystem->PostUserEvent( event );
		break;

#if defined( WIN32 )
	case WM_SIZE:
		if ( wParam != SIZE_MINIMIZED )
		{
			// Update restored client rect
			::GetClientRect( hWnd, &m_rcLastRestoredClientRect );
		}
		else
		{
			// Fix the window rect to have same client area as it used to have
			// before it got minimized
			RECT rcWindow;
			::GetWindowRect( hWnd, &rcWindow );

			rcWindow.right = rcWindow.left + m_rcLastRestoredClientRect.right;
			rcWindow.bottom = rcWindow.top + m_rcLastRestoredClientRect.bottom;

			::AdjustWindowRect( &rcWindow, ::GetWindowLong( hWnd, GWL_STYLE ), FALSE );
			::MoveWindow( hWnd, rcWindow.left, rcWindow.top,
				rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, FALSE );
		}
		break;
#endif

	case WM_SETFOCUS:
		break;
	
	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	case WM_COPYDATA:
		//
		// Researching all codebase legacy yields the following use cases.
		// COPYDATASTRUCT.dwParam = 0 in most cases:
		// + another engine instance passing over commandline when executed with -hijack param
		// + worldcraft editor sending a command
		// + mdlviewer sending a reload model command
		// + Hammer -> engine remote console command.
		// COPYDATASTRUCT.cbData = 0 in another case:
		// + materialsystem enumerating and sending message to other materialsystem windows
		// Our WNDPROC should return true to indicate that the message was handled.
		//
		{
			COPYDATASTRUCT &cpData = *( ( COPYDATASTRUCT * ) lParam );
			if ( cpData.cbData )
			{	// There is payload supplied to the message
				if ( cpData.dwData == 0 )
				{	// Legacy protocol to put console command into command buffer
					const char *pcBuffer = ( const char * ) ( cpData.lpData );
					Cbuf_AddText( Cbuf_GetCurrentPlayer(), pcBuffer );
					lRet = 1;
				}
				else if ( cpData.dwData == 0x43525950 ) // CRYP
				{	// Encryption key supplied for connection
					// Format:
					// dot.ted.ip.adr:port>4bytes16bytesupto256bytes
					const char *pcBuffer = ( const char * ) ( cpData.lpData );
					const char *pcTerm = V_strnchr( pcBuffer, '>', 24 );
					if ( pcTerm && pcTerm > pcBuffer )
					{
						DWORD numBytesForAddress = pcTerm - pcBuffer + 1;
						if ( ( numBytesForAddress < cpData.cbData ) &&
							( cpData.cbData - numBytesForAddress > sizeof( int32 ) + NET_CRYPT_KEY_LENGTH ) &&
							( cpData.cbData - numBytesForAddress - sizeof( int32 ) - NET_CRYPT_KEY_LENGTH <= 256 ) &&
							!!( *reinterpret_cast< const int32 * >( pcTerm + 1 ) & 0xFFFF0000 ) ) // client key must use high bits and be not zero
						{
							CFmtStr fmtAddr( "%.*s", pcTerm - pcBuffer, pcBuffer );
							extern void RegisterServerCertificate( char const *szServerAddress, int numBytesPayload, void const *pvPayload );
							RegisterServerCertificate( fmtAddr.Access(), cpData.cbData - numBytesForAddress, pcTerm + 1 );
							lRet = 1;
						}
					}
				}
			}
		}
		break;


#if defined( WIN32 )
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		RECT rcClient;
		GetClientRect( hWnd, &rcClient );
		EndPaint(hWnd, &ps);
		break;
#endif

#if defined( WIN32 )
	case WM_DISPLAYCHANGE:
		if ( !m_iDesktopHeight || !m_iDesktopWidth )
		{
			UpdateDesktopInformation( wParam, lParam );
		}
		break;
#endif

	case WM_IME_NOTIFY:
		switch ( wParam )
		{
		default:
			break;

#ifndef DEDICATED
		case 14:
            if ( !videomode->IsWindowedMode() )
				return 0;
			break;
#endif
		}
		bCallDefault = true;
		break;

	default:
		bCallDefault = true;
	    break;
    }

	if ( bCallDefault )
	{
		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
	}

    // return 0 if handled message, 1 if not
    return lRet;
}
#else

#endif


#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Creates the game window 
//-----------------------------------------------------------------------------
static LRESULT WINAPI HLEngineWindowProc( HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM  lParam )
{
	return g_Game.WindowProc( hWnd, uMsg, wParam, lParam );
}

#define DEFAULT_EXE_ICON 101

static void DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs( void )
{
#ifdef IS_WINDOWS_PC
	WSAData wsaData;
	if ( ! WSAStartup( 0x0101, &wsaData ) )
	{
		SOCKET tmpSocket = socket( AF_INET, SOCK_DGRAM, 0 );
		if ( tmpSocket != INVALID_SOCKET )
		{
			char Options[]={ 1 };
			setsockopt( tmpSocket, SOL_SOCKET, SO_BROADCAST, Options, sizeof(Options));
			char pszHostName[256];
			gethostname( pszHostName, sizeof( pszHostName ) );
			hostent *hInfo = gethostbyname( pszHostName );
			if ( hInfo )
			{
				sockaddr_in myIpAddress;
				memset( &myIpAddress, 0, sizeof( myIpAddress ) );
				myIpAddress.sin_family = AF_INET;
				myIpAddress.sin_port = htons( 27015 );			// our normal server port
				myIpAddress.sin_addr.S_un.S_un_b.s_b1 = hInfo->h_addr_list[0][0];
				myIpAddress.sin_addr.S_un.S_un_b.s_b2 = hInfo->h_addr_list[0][1];
				myIpAddress.sin_addr.S_un.S_un_b.s_b3 = hInfo->h_addr_list[0][2];
				myIpAddress.sin_addr.S_un.S_un_b.s_b4 = hInfo->h_addr_list[0][3];
				if ( bind( tmpSocket, ( sockaddr * ) &myIpAddress, sizeof( myIpAddress ) ) != -1 )
				{
					if ( sendto( tmpSocket, pszHostName, 1, 0, ( sockaddr *) &myIpAddress, sizeof( myIpAddress ) ) == -1 )
					{
						// error?
					}

				}
			}
			closesocket( tmpSocket );
		}
		WSACleanup();
	}
	
#endif
}
#endif

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

	#if defined( DX_TO_VK_ABSTRACTION )
		V_strcat( windowName, " - Vulkan", sizeof( windowName ) );
	#endif

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
		
#if defined( USE_SDL )
	modinfo->deleteThis();
	modinfo = NULL;

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
#elif defined( WIN32 ) && !defined( USE_SDL )
#ifndef DEDICATED

	WNDCLASSW wc;
	memset( &wc, 0, sizeof( wc ) );

    wc.style         = CS_OWNDC | CS_DBLCLKS;

    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = CLASSNAME;

	// find the icon file in the filesystem
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath( "resource/game.ico", localPath, sizeof(localPath) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		wc.hIcon = (HICON)::LoadImage(NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
	}
	else
	{
		wc.hIcon = (HICON)LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE( DEFAULT_EXE_ICON ) );
	}
	

#ifndef DEDICATED
	char const *pszGameType = modinfo->GetString( "type" );
	if ( pszGameType && Q_stristr( pszGameType, "multiplayer" ) )
		DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs();
#endif

	wchar_t uc[512];
	::MultiByteToWideChar(CP_UTF8, 0, windowName, -1, uc, sizeof( uc ) / sizeof(wchar_t));

	modinfo->deleteThis();
	modinfo = NULL;
	// Oops, we didn't clean up the class registration from last cycle which
	// might mean that the wndproc pointer is bogus
	UnregisterClassW( CLASSNAME, m_hInstance );
	// Register it again
    RegisterClassW( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	// Give it a frame if we want a border
	if ( videomode->IsWindowedMode() )
	{
		if( !CommandLine()->FindParm( "-noborder" )&& !videomode->NoWindowBorder() )
		{
			style |= WS_OVERLAPPEDWINDOW;
			style &= ~WS_THICKFRAME;
		}
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	int w, h;

	// Create a full screen size window by default, it'll get resized later anyway
	w = GetSystemMetrics( SM_CXSCREEN );
	h = GetSystemMetrics( SM_CYSCREEN );

	// Create the window
	DWORD exFlags = 0;
	if ( g_bTextMode )
	{
		style &= ~WS_VISIBLE;
		exFlags |= WS_EX_TOOLWINDOW; // So it doesn't show up in the taskbar.
	}

	HWND hwnd = CreateWindowExW( exFlags, CLASSNAME, uc, style, 
		0, 0, w, h, NULL, NULL, m_hInstance, NULL );
	// NOTE: On some cards, CreateWindowExW slams the FPU control word
	SetupFPUControlWord();

	if ( !hwnd )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}

	SetMainWindow( hwnd );

	AttachToWindow( );
	return true;
#else
	return true;
#endif
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// Destroys the game window 
//-----------------------------------------------------------------------------
void CGame::DestroyGameWindow()
{
#if defined( USE_SDL )
	g_pLauncherMgr->DestroyGameWindow();
#elif defined( WIN32 )
#ifndef DEDICATED
	// Destroy all things created when the window was created
	if ( !m_bExternallySuppliedWindow )
	{
		DetachFromWindow( );

		if ( m_hWindow )
		{
			DestroyWindow( m_hWindow );
			m_hWindow = (HWND)0;
		}

		UnregisterClassW( CLASSNAME, m_hInstance );
	}
	else
	{
		m_hWindow = (HWND)0;
		m_bExternallySuppliedWindow = false;
	}

#endif // !DEDICATED 
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// This is used in edit mode to specify a particular game window (created by hammer)
//-----------------------------------------------------------------------------
void CGame::SetGameWindow( void *hWnd )
{
	m_bExternallySuppliedWindow = true;
#if defined( USE_SDL )
	SDL_RaiseWindow( (SDL_Window *)hWnd );
#elif defined( WIN32 ) 
	SetMainWindow( (HWND)hWnd );
#else
#error
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGame::AttachToWindow()
{
	if ( !m_hWindow )
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	m_ChainedWindowProc = (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)HLEngineWindowProc );
#endif
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
#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_hWindow || !m_ChainedWindowProc )
	{
		m_ChainedWindowProc = NULL;
		return;
	}
#endif

	if ( g_pInputSystem )
	{
		// Detach the input system window proc
		g_pInputSystem->EnableInput( false );
		g_pInputSystem->DetachFromWindow( );
	}

#if defined( WIN32 ) && !defined( USE_SDL )
	Assert( (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC ) == HLEngineWindowProc );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)m_ChainedWindowProc );
#endif
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

#if defined( WIN32 ) && !defined( USE_SDL )
	// Capture + hide the mouse
    g_pInputStackSystem->SetMouseCapture( m_hInputContext, true );
#else
	Assert( !"Impl me" );
	return false;
#endif
	return true;
}

void CGame::InputDetachFromGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_ChainedWindowProc )
		return;

	// Release + show the mouse
	ReleaseCapture();
#else
	Assert( !"Impl me" );
#endif

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

#if defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )
	VAudioInit();
	void *pMilesEngine = NULL;
	if ( g_pBIK) 
	{
		ConVarRef windows_speaker_config("windows_speaker_config");
		
		if ( windows_speaker_config.IsValid() && windows_speaker_config.GetInt() >= 5 )
		{
			pMilesEngine = vaudio ? vaudio->CreateMilesAudioEngine() : NULL;
			g_pBIK->SetMilesSoundDevice( pMilesEngine );
		}
		else
		{
			g_pBIK->SetMilesSoundDevice( NULL );
		}
	}
#endif // defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )

	PlayVideoListAndWait( pszFile );

#if defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )
	if ( pMilesEngine )
	{
		g_pBIK->SetMilesSoundDevice( NULL );
		vaudio->DestroyMilesAudioEngine( pMilesEngine );
	}
#endif

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

#if defined( USE_SDL )
	bool CursorStateBak = SDL_CursorVisible();
	SDL_HideCursor();
#elif defined( WIN32 )
	// hide cursor while playing videos
	::ShowCursor(FALSE);
#endif
	


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



#if defined( USE_SDL )
	if ( CursorStateBak )
		SDL_ShowCursor();
	else
		SDL_HideCursor();
#elif defined( WIN32 )
	// show cursor again
	::ShowCursor(TRUE);
#endif
#endif // DEDICATED
}

//-----------------------------------------------------------------------------
// Plays a Bink video until the video completes or user input cancels
//-----------------------------------------------------------------------------
void CGame::PlayVideoAndWait( const char *filename, bool bNeedHealthWarning )
{
#if defined( BINK_VIDEO )

#if defined( IS_WINDOWS_PC ) || defined( OSX )
	if ( !filename || !filename[0] )
		return;

	if ( !g_pBIK )
		return;

	if ( Q_stristr( filename, "RATINGBOARD" ) )
		return;

	// Supplying a NULL context will cause Bink to allocate its own
	// FIXME: At this point we're playing at the full volume of the computer, NOT the user's set volume in the game!
	Audio_CreateSDLAudioDevice();

 	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

#if defined ( QUICKTIME_VIDEO )
	IQuickTime *pVideoPlayer = g_pQuickTime;
	QUICKTIMEMaterial_t VideoHandle;
	QUICKTIMEMaterial_t InvalidVideoHandle = QUICKTIMEMATERIAL_INVALID;
#elif defined( BINK_VIDEO )
	IBik *pVideoPlayer = g_pBIK;
	BIKHandle_t	VideoHandle;
	BIKHandle_t InvalidVideoHandle = BIKHANDLE_INVALID;
#else
  #error "Need to have support for video playback enabled via source_video_base.vpc"
#endif
	
	if ( !pVideoPlayer )
		return;

	// get the path to the media file and play it.
	char localPath[MAX_PATH];
	
	// Are we wanting to use a quicktime ".mov" version of the media instead of what's specified?
#if defined( FORCE_QUICKTIME ) && defined( QUICKTIME_VIDEO )
	// is it not a .mov file extension?
	if ( V_stristr( com_token, ".mov") == NULL )
	{
		// Compose Quicktime version
		char QTPath[MAX_PATH];
		V_strncpy( QTPath, filename, MAX_PATH );
		V_SetExtension( QTPath, ".mov", MAX_PATH );
		
		g_pFileSystem->GetLocalPath( QTPath, localPath, sizeof(localPath) );
	}
	else
#endif 
	{
		V_strncpy( localPath, filename, sizeof(localPath) );
	}
	
	// Load and create our BINK or QuickTime video
	VideoHandle = pVideoPlayer->CreateMaterial( "VideoMaterial", localPath, "GAME" );
	if ( VideoHandle == InvalidVideoHandle )
	{
		return;
	}

	float flU0 = 0.0f;
	float flV0 = 0.0f;
	float flU1, flV1;
	pVideoPlayer->GetTexCoordRange( VideoHandle, &flU1, &flV1 );

	IMaterial *pMaterial = pVideoPlayer->GetMaterial( VideoHandle );

	int nTexHeight = pMaterial->GetMappingHeight();
	int nTexWidth = pMaterial->GetMappingWidth();

	int nWidth, nHeight;
	pVideoPlayer->GetFrameSize( VideoHandle, &nWidth, &nHeight );

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();

	// Determine how the video's aspect ratio relates to the screen's
	float flPhysicalFrameRatio = aspectRatioInfo.m_flFrameBuffertoPhysicalScalar * ( ( float )m_width / ( float )m_height );
	float flVideoRatio = ( ( float ) nWidth / ( float ) nHeight );

	int nPlaybackWidth;
	int nPlaybackHeight;
	
	if ( flVideoRatio > flPhysicalFrameRatio )
	{
		// Height must be adjusted
		nPlaybackWidth = m_width;
		// Have to account for the difference between physical and pixel aspect ratios.
		nPlaybackHeight = ( ( float )m_width / aspectRatioInfo.m_flPhysicalToFrameBufferScalar ) / flVideoRatio;
	}
	else if ( flVideoRatio < flPhysicalFrameRatio )
	{
		// Width must be adjusted
		// Have to account for the difference between physical and pixel aspect ratios.
		nPlaybackWidth = ( float )m_height * flVideoRatio * aspectRatioInfo.m_flPhysicalToFrameBufferScalar;
		nPlaybackHeight = m_height;
	}
	else
	{
		// Ratio matches
		nPlaybackWidth = m_width;
		nPlaybackHeight = m_height;
	}

	// Turn off our vertex alpha for these draw calls as they don't write alpha per-vertex
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_VERTEXALPHA, false );

	// Prep the screen
	pRenderContext->Viewport( 0, 0, m_width, m_height );
	pRenderContext->DepthRange( 0, 1 );
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );
	
	// Find our letterboxing offset
	int xpos = ( (float) ( m_width - nPlaybackWidth ) / 2 );
	int ypos = ( (float) ( m_height - nPlaybackHeight ) / 2 );

	// Enable the input system's message pump
	g_pInputSystem->EnableMessagePump( true );

	// We need to make sure that these keys have been released since last pressed, otherwise you can skip
	// movies inadvertently 
	bool bKeyDebounced = ( UserRequestingMovieSkip() == false );
	bool bExitingProcess = false;

	while ( 1 )
	{

		// Pump messages to avoid lockups on focus change
		g_pInputSystem->PollInputState( GetBaseLocalClient().IsActive() );
		game->DispatchAllStoredGameMessages();

		// xbox cannot skip legals
		if ( bKeyDebounced )
		{
			if ( !bExitingProcess && UserRequestingMovieSkip() )
				break;
		}
		else
		{
			bKeyDebounced = ( UserRequestingMovieSkip() == false );
		}

		// Update our frame
		if ( pVideoPlayer->Update( VideoHandle ) == false )
			break;
		
		if( IsPS3QuitRequested() )
			break;

		pRenderContext->AntiAliasingHint( AA_HINT_MOVIE );

		// Clear the draw buffer and blt the material to it
		pRenderContext->ClearBuffers( true, true, true );
		pRenderContext->DrawScreenSpaceRectangle( pMaterial, xpos, ypos, nPlaybackWidth, nPlaybackHeight, flU0*nTexWidth, flV0*nTexHeight, flU1*nTexWidth-1, flV1*nTexHeight-1, nTexWidth, nTexHeight );

		// Busy wait until we are ready to swap.
#ifdef QUICKTIME_VIDEO
		while ( !pVideoPlayer->ReadyForSwap( BIKHandle ) )
#else
		// TODO - is this valid with threaded bink changes?: while ( !pVideoPlayer->ReadyForSwap( BIKHandle ) )
#endif
		{
			NULL;
		}

		g_pMaterialSystem->SwapBuffers();
		
		if ( ENABLE_BIK_PERF_SPEW )
		{
			// timing debug code for bink playback
			static double flPreviousTime = -1.0;
			double flTime = Plat_FloatTime();
			double flDeltaTime = flTime - flPreviousTime;
			if ( flDeltaTime > 0.0 )
			{
				Warning( "%0.2lf sec*60 %0.2lf fps\n", flDeltaTime * 60.0, 1.0 / flDeltaTime );
			}
			flPreviousTime = flTime;
		}
	}

	// Disable the input system's message pump
	g_pInputSystem->EnableMessagePump( false );

	// Clean up the Bink video
	if ( VideoHandle != InvalidVideoHandle )
	{
		pVideoPlayer->DestroyMaterial( VideoHandle );
	}

#endif

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
#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = 0;
	m_ChainedWindowProc = NULL;
#endif

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

#if defined( WIN32 ) && !defined( USE_SDL )
	OSVERSIONINFO	vinfo;
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if ( !GetVersionEx( &vinfo ) )
	{
		return false;
	}

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
	{
		return false;
	}

	m_hInstance = (HINSTANCE)pvInstance;
#endif

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

#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = 0;
#endif


	return true;
}

void *CGame::GetMainWindow( void )
{
	return (void*)m_hWindow;
}

#if defined(USE_SDL)
void** CGame::GetMainWindowAddress( void )
{
	m_hWindow = GetGameSDLWindow();
	return (void**)&m_hWindow;
}
#elif defined( WIN32 ) 
void** CGame::GetMainWindowAddress( void )
{
	return (void**)&m_hWindow;
}
#else
#error
#endif

void CGame::GetDesktopInfo( int &width, int &height, int &refreshrate )
{
#if defined(USE_SDL)

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

#elif defined( WIN32 )
	// order of initialization means that this might get called early.  In that case go ahead and grab the current
	// screen window and setup based on that.
	// we need to do this when initializing the base list of video modes, for example
	if ( m_iDesktopWidth == 0 )
	{
		HDC dc = ::GetDC( NULL );
		width = ::GetDeviceCaps(dc, HORZRES);
		height = ::GetDeviceCaps(dc, VERTRES);
		refreshrate = ::GetDeviceCaps(dc, VREFRESH);
		::ReleaseDC( NULL, dc );
		return;
	}
	width = m_iDesktopWidth;
	height = m_iDesktopHeight;
	refreshrate = m_iDesktopRefreshRate;
#else
#error
#endif
}

void CGame::UpdateDesktopInformation( HWND hWnd )
{
#if defined(USE_SDL)
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
#elif defined( WIN32 ) 
	HDC dc = ::GetDC( hWnd );
	m_iDesktopWidth = ::GetDeviceCaps(dc, HORZRES);
	m_iDesktopHeight = ::GetDeviceCaps(dc, VERTRES);
	m_iDesktopRefreshRate = ::GetDeviceCaps(dc, VREFRESH);
	::ReleaseDC( hWnd, dc );
#else
#error
#endif
}

#ifdef WIN32
void CGame::UpdateDesktopInformation( WPARAM wParam, LPARAM lParam )
{
	m_iDesktopWidth = LOWORD( lParam );
	m_iDesktopHeight = HIWORD( lParam );
}
#endif

void CGame::SetMainWindow( HWND window )
{
#if defined( USE_SDL )
	m_hWindow = (SDL_Window*)window;
#elif defined( WIN32 ) && !defined( USE_SDL )
	m_hWindow = window;
#else
#error
#endif

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

