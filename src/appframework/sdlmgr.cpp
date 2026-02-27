//====== Copyright  1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: An application framework
//
//=============================================================================//

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "appframework/ilaunchermgr.h"
#include "tier1/keyvalues.h"
#include "filesystem.h"

#include "materialsystem/imaterialsystem.h"
#include "togl/rendermechanism.h"
#include "tier0/fasttimer.h"


// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"

ConVar sdl_double_click_size( "sdl_double_click_size", "2" );
ConVar sdl_double_click_time( "sdl_double_click_time", "400" );

static void DebugPrintf( const char *pMsg, ... )
{
	va_list args;
	va_start( args, pMsg );
	char buf[2048];
	V_vsnprintf( buf, sizeof( buf ), pMsg, args );
	va_end( args );

	Plat_DebugString( buf );
}

// #define SDLAPP_DEBUG
#ifdef SDLAPP_DEBUG
class LinuxAppFuncLogger
{
	public:
		LinuxAppFuncLogger( const char *funcName ) : m_funcName( funcName )
		{
			printf( ">%s\n", m_funcName );
		};

		LinuxAppFuncLogger( const char *funcName, char *fmt, ... )
		{
			m_funcName = funcName;

			va_list	vargs;
			va_start(vargs, fmt);
			vprintf( fmt, vargs );
			va_end( vargs );
		}

		~LinuxAppFuncLogger( )
		{
			printf( "<%s\n", m_funcName );
		};

		const char *m_funcName;
};
#define	SDLAPP_FUNC			LinuxAppFuncLogger _logger_( __FUNCTION__ )
#else
#define SDLAPP_FUNC
#endif


//-----------------------------------------------------------------------------
#if !defined( DEDICATED )

class CSDLMgr : public ILauncherMgr
{
public:

	CSDLMgr();

// ILauncherMgr impls.
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();

	virtual void *QueryInterface( const char *pInterfaceName );

	// Init, shutdown
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen );

	// Get the next N events. The function returns the number of events that were filled into your array.
	virtual int GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvents = false );

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y );
	virtual void GetCursorPosition( int *px, int *py );

	virtual void *GetWindowRef() { return (void *)m_Window; }

	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen );
	virtual bool IsWindowFullScreen() { return m_bFullScreen; }
	virtual void MoveWindow( int x, int y );
	virtual void SizeWindow( int width, int tall );
	virtual void PumpWindowsMessageLoop();

	virtual void DestroyGameWindow();
	virtual void SetApplicationIcon( const char *pchAppIconFile );

	virtual void GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta = false );

	virtual int GetActiveDisplayIndex();
	virtual void GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz ); // Retrieve the size of the monitor (desktop)
	virtual void RenderedSize( uint &width, uint &height, bool set );	// either set or retrieve rendered size value (from dxabstract)
	virtual void DisplayedSize( uint &width, uint &height );			// query backbuffer size (window size whether FS or windowed)

	virtual void GetStackCrawl( CStackCrawlParams *params );

	virtual void WaitUntilUserInput( int msSleepTime );

  	virtual InputCursorHandle_t LoadCursorFromFile( const char *pchFileName );

	virtual void FreeCursor( const InputCursorHandle_t pchCursor );
	virtual void SetCursorIcon( const InputCursorHandle_t pchCursor );

	// Post an event to the input event queue.
	// if debugEvent is true, post it to the debug event queue.
	void PostEvent( const CCocoaEvent &theEvent, bool debugEvent=false );

	// ask if an event is debug flavor or not.
	bool IsDebugEvent( CCocoaEvent& event );

	virtual void SetMouseVisible( bool bState );
	virtual void SetMouseCursor( SDL_Cursor *hCursor );
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab ) { m_bForbidMouseGrab = bForbidMouseGrab; }

	virtual void OnFrameRendered();

	virtual double GetPrevGLSwapWindowTime() { return m_flPrevGLSwapWindowTime; }

	// Returns all dependent libraries
	virtual const AppSystemInfo_t* GetDependencies() {return NULL;}

	virtual void SetGammaRamp( const uint16 *pRed, const uint16 *pGreen, const uint16 *pBlue );

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	virtual void ForceSystemCursorVisible();
	virtual void UnforceSystemCursorVisible();
#endif

	// Returns the tier
	virtual AppSystemTier_t GetTier()
	{
		return APP_SYSTEM_TIER2;
	}
	// Reconnect to a particular interface
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) {}

	// Called to create a game window that will be hidden, designed for
	// getting an OpenGL context going so we can begin initializing things.
    bool CreateHiddenGameWindow( const char *pTitle, bool bWindowed, int width, int height );

	virtual bool IsSingleton() { return false; }

private:
	void handleKeyInput( const SDL_Event &event );

#if defined( OSX )
	bool					m_force_vsync;				// true if 10.6.4 + bad NV driver
#endif

	SDL_Window *m_Window;


	bool m_bCursorVisible;
	bool m_bSetMouseVisibleCalled;
	int m_nFramesCursorInvisibleFor;
	SDL_Cursor *m_hCursor;
	bool m_bSetMouseCursorCalled;

	bool m_bHasFocus;
	bool m_bFullScreen;
	bool m_bForbidMouseGrab;  // temporary setting showing if the mouse should
	                          // grab if possible.

	int m_nMouseXDelta;
	int m_nMouseYDelta;

	int m_ScreenWidth;
	int m_ScreenHeight;

	int m_renderedWidth;
	int m_rendererHeight;

	int m_WindowWidth;
	int m_WindowHeight;

    bool m_bExpectSyntheticMouseMotion;
    int  m_nMouseTargetX;
    int  m_nMouseTargetY;
    int  m_nWarpDelta;
    bool m_bRawInput;

	int m_lastKnownSwapInterval;	//-2 if unknown, 0/1/-1 otherwise
	int m_lastKnownSwapLimit;		//-1 if unknown, 0/1 otherwise

	int m_pixelFormatAttribs[32];
	int m_pixelFormatAttribCount;

	float m_flMouseXScale;
	float m_flMouseYScale;

	// !!! FIXME: can we rename these from "Cocoa"?
	CThreadMutex m_CocoaEventsMutex;					// use for either queue below
	CUtlLinkedList<CCocoaEvent,int> m_CocoaEvents;
	CUtlLinkedList<CCocoaEvent,int> m_DebugEvents;		// intercepted keys which wil be passed over to GLM

	uint m_keyModifierMask;
	uint32_t m_keyModifiers;
	uint32_t m_mouseButtons;

	bool m_bGotMouseButtonDown;
	Uint32 m_MouseButtonDownTimeStamp;
	int m_MouseButtonDownX;
	int m_MouseButtonDownY;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	int m_nForceCursorVisible;
	int m_nForceCursorVisiblePrev;
	SDL_Cursor* m_hSystemArrowCursor;
#endif

	double m_flPrevGLSwapWindowTime;

	bool m_bTextMode;
};

ILauncherMgr *g_pLauncherMgr = NULL;

void* CreateSDLMgr()
{
	if ( g_pLauncherMgr == NULL )
	{
		g_pLauncherMgr = new CSDLMgr();
	}
	return (void *)g_pLauncherMgr;
}

// Display index to show window on.
static bool g_bSDLDisplayindexSet = false;
static void sdl_displayindex_changed( IConVar *pConVar, const char *pOldString, float flOldValue );
ConVar sdl_displayindex( "sdl_displayindex", "-1", FCVAR_ARCHIVE | FCVAR_HIDDEN, "SDL fullscreen display index.", sdl_displayindex_changed );
static void sdl_displayindex_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	int NumVideoDisplays = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &NumVideoDisplays );
	if ( displays )
		SDL_free( displays );

	if ( ( sdl_displayindex.GetInt() < 0 ) || ( sdl_displayindex.GetInt() >= NumVideoDisplays ) )
	{
		sdl_displayindex.SetValue( 0 );
	}

	g_bSDLDisplayindexSet = true;
}


// Return display index of largest SDL display ( plus width & height ).
static int GetLargestDisplaySize( int& Width, int& Height )
{
	int nDisplay = 0;

	Width = 640;
	Height = 480;

	int displayCount = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &displayCount );
	if ( displays )
	{
		for ( int i = 0; i < displayCount; i++ )
		{
			SDL_Rect rect = { 0, 0, 0, 0 };

			SDL_GetDisplayBounds( displays[i], &rect );

			if ( ( rect.w > Width ) || ( ( rect.w == Width ) && ( rect.h > Height ) ) )
			{
				Width = rect.w;
				Height = rect.h;

				nDisplay = i;
			}
		}
		SDL_free( displays );
	}

	return nDisplay;
}

CON_COMMAND( grab_window, "grab/ungrab window." )
{
	if ( g_pLauncherMgr && g_pLauncherMgr->GetWindowRef() )
	{
		bool bGrab;
		SDL_Window *pWindow = ( SDL_Window * )g_pLauncherMgr->GetWindowRef();

		if ( args.ArgC() >= 2 )
		{
			bGrab = ( args[ 1 ][ 0 ] == '1' ) ? true : false;
		}
		else
		{
			bGrab = SDL_GetWindowMouseGrab( pWindow ) ? false : true;
		}

		g_pLauncherMgr->SetForbidMouseGrab( !bGrab );

		if ( bGrab != SDL_GetWindowMouseGrab( pWindow ) )
		{
			Msg( "SetWindowGrab( %s )\n", bGrab ? "true" : "false" );
			SDL_SetWindowMouseGrab( pWindow, bGrab );

			// force non-fullscreen windows to the foreground if grabbed, so you can't
			//  get your mouse locked to something in the background.
			if ( bGrab && !g_pLauncherMgr->IsWindowFullScreen() )
			{
				SDL_RaiseWindow( pWindow );
			}
		}
	}
}

CSDLMgr::CSDLMgr()
{
	m_Window = NULL;
	Init();
}

InitReturnVal_t CSDLMgr::Init()
{
	SDLAPP_FUNC;

	if (m_Window != NULL)
		return INIT_OK;  // already initialized.

#if ALLOW_TEXT_MODE
	m_bTextMode = CommandLine()->FindParm( "-textmode" );
#else
	m_bTextMode = false;
#endif

	// Default to using Xinerama and XRandR if available.

	// If someone is running multimon, we want the individual
	// screen sizes (not combined), so default to xinerama on.
	SDL_SetHint( "SDL_VIDEO_X11_XINERAMA", "1" );
	// We don't need xrandr since we're not changing the desktop resolution.
	SDL_SetHint( "SDL_VIDEO_X11_XRANDR", "0" );
	// Default to no XVidMode.
	SDL_SetHint( "SDL_VIDEO_X11_XVIDMODE", "0" );

	// Prefer Wayland over X11 - X11/XWayland has issues on modern Wayland desktops.
	// This must be set before SDL_Init(). Falls back to X11 if Wayland unavailable.
	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "wayland,x11" );
	SDL_SetHint( SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1" );

	if (!m_bTextMode && !SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
			Error( "SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError() );

		if (!SDL_Vulkan_LoadLibrary(NULL))
			Error( "SDL_Vulkan_LoadLibrary(NULL) failed: %s", SDL_GetError() );
	}

	if ( !m_bTextMode )
		Msg("SDL video target is '%s'\n", SDL_GetCurrentVideoDriver());

	m_bForbidMouseGrab = true;
	if ( !CommandLine()->FindParm( "-nomousegrab" ) && CommandLine()->FindParm( "-mousegrab" ) )
	{
		m_bForbidMouseGrab = false;
	}

	m_bCursorVisible = true;
	m_bSetMouseVisibleCalled = false;
	m_nFramesCursorInvisibleFor = 0;
	m_hCursor = NULL;
	m_bSetMouseCursorCalled = false;


	m_bHasFocus = true;
	m_keyModifiers = 0;
	m_keyModifierMask = 0;
	m_mouseButtons = 0;

	m_Window = NULL;
	m_bFullScreen = false;
	m_nMouseXDelta = 0;
	m_nMouseYDelta = 0;
	m_ScreenWidth = 0;
	m_ScreenHeight = 0;
	m_renderedWidth = 0;
	m_rendererHeight = 0;
	m_WindowWidth = 0;
	m_WindowHeight = 0;
	m_pixelFormatAttribCount = 0;
	m_lastKnownSwapInterval = -2;
	m_lastKnownSwapLimit = -1;
	m_flMouseXScale = 1.0f;
	m_flMouseYScale = 1.0f;

	m_bGotMouseButtonDown = false;
	m_MouseButtonDownTimeStamp = 0;
	m_MouseButtonDownX = 0;
	m_MouseButtonDownY = 0;

	m_bExpectSyntheticMouseMotion = false;
	m_nMouseTargetX = 0;
	m_nMouseTargetY = 0;
	m_nWarpDelta = 0;
	m_bRawInput = false;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	m_nForceCursorVisible = 0;
	m_nForceCursorVisiblePrev = 0;
	m_hSystemArrowCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_DEFAULT );
#endif

	m_flPrevGLSwapWindowTime = 0.0f;

	memset(m_pixelFormatAttribs, '\0', sizeof (m_pixelFormatAttribs));

	int *attCursor = m_pixelFormatAttribs;

	m_pixelFormatAttribCount = (attCursor - &m_pixelFormatAttribs[0]) / 2;

	// We need a Vulkan context before we dig down further, so create an initial
	// window at desktop resolution to avoid issues on Wayland.
	int initWidth = 1920, initHeight = 1080;
	const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode( SDL_GetPrimaryDisplay() );
	if ( desktopMode )
	{
		initWidth = desktopMode->w;
		initHeight = desktopMode->h;
	}
	if ( !CreateHiddenGameWindow( "", true, initWidth, initHeight ) )
		Error( "CreateGameWindow failed" );

	if ( !m_bTextMode )
		SDL_HideWindow( m_Window );

	return INIT_OK;
}

bool CSDLMgr::Connect( CreateInterfaceFn factory )
{
	SDLAPP_FUNC;

	return true;
}

void CSDLMgr::Disconnect()
{
	SDLAPP_FUNC;

}

void *CSDLMgr::QueryInterface( const char *pInterfaceName )
{
	SDLAPP_FUNC;
#if defined(USE_SDL)
	if ( !Q_stricmp( pInterfaceName, SDLMGR_INTERFACE_VERSION ) )
		return this;
#endif
	return NULL;
}

void CSDLMgr::Shutdown()
{
	SDLAPP_FUNC;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	SDL_DestroyCursor( m_hSystemArrowCursor );
#endif

	DestroyGameWindow();
	SDL_Vulkan_UnloadLibrary();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool CSDLMgr::CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen )
{
	SDLAPP_FUNC;

	if( ( width <= 0 ) || ( height <= 0 ) )
	{
		// Don't mess with current width, height - use current (or sane defaults).
		uint defaultWidth = 0;
		uint defaultHeight = 0;
		uint defaultRefreshHz = 0; // Not used

		int displayindex = sdl_displayindex.GetInt();
		this->GetNativeDisplayInfo( displayindex, defaultWidth, defaultHeight, defaultRefreshHz );

		if ( 0 == defaultWidth ) defaultWidth = 1024;
		if ( 0 == defaultHeight ) defaultHeight = 768;

		width = m_WindowWidth ? m_WindowWidth : defaultWidth;
		height = m_WindowHeight ? m_WindowHeight : defaultHeight;
	}

	if ( m_Window )
	{
		if ( pTitle )
		{
			SDL_SetWindowTitle( m_Window, pTitle );
		}

		// Show the window BEFORE any fullscreen transition.
		// On Wayland, a hidden window has no associated wl_output, so the
		// compositor can't know which monitor to fullscreen on. Showing it
		// first maps the surface, then SyncWindow waits for the compositor
		// to assign the window to an output before we request fullscreen.
		SDL_ShowWindow( m_Window );
		SDL_SyncWindow( m_Window );

		if ( m_bFullScreen != !bWindowed )
		{
			SetWindowFullScreen( !bWindowed, width, height, bDesktopFriendlyFullscreen );
		}
		else
		{
			SizeWindow( width, height );

			// Center on the window's current output (compositor's choice),
			// or a specific display if the user set sdl_displayindex.
			int displayindex = sdl_displayindex.GetInt();
			if ( displayindex >= 0 )
			{
				int dCount = 0;
				SDL_DisplayID *dList = SDL_GetDisplays( &dCount );
				SDL_DisplayID displayID = ( dList && displayindex < dCount ) ? dList[displayindex] : SDL_GetPrimaryDisplay();
				if ( dList ) SDL_free( dList );
				MoveWindow( SDL_WINDOWPOS_CENTERED_DISPLAY( displayID ), SDL_WINDOWPOS_CENTERED_DISPLAY( displayID ) );
			}
			else
			{
				MoveWindow( SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
			}
		}

		SDL_RaiseWindow( m_Window );
		SDL_StartTextInput( m_Window );

		return true;
	}

	if ( CreateHiddenGameWindow( pTitle, true, width, height ) )
	{
		SDL_ShowWindow( m_Window );
		SDL_SyncWindow( m_Window );  // Wait for compositor to place the window on an output
		SDL_StartTextInput( m_Window );  // SDL3: text input is off by default
		return true;
	}
	else
	{
		return false;
	}
}

bool CSDLMgr::CreateHiddenGameWindow( const char *pTitle, bool bWindowed, int width, int height )
{
	if ( m_bTextMode )
		return true;

	m_bFullScreen = !bWindowed;

	// Create the window. On Wayland, SDL_GetPrimaryDisplay() may not match
	// the compositor's primary output (SDL picks the highest-res display as
	// "primary" when DBus info is unavailable). So for the default case
	// (sdl_displayindex -1), don't specify a display — let the compositor
	// place the window on its preferred output. Only target a specific
	// display when the user explicitly sets sdl_displayindex.
	int displayindex = sdl_displayindex.GetInt();

	SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	if ( displayindex >= 0 )
	{
		int displayCount = 0;
		SDL_DisplayID *displayList = SDL_GetDisplays( &displayCount );
		SDL_DisplayID displayID = ( displayList && displayindex < displayCount ) ? displayList[displayindex] : SDL_GetPrimaryDisplay();
		if ( displayList ) SDL_free( displayList );

		SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetStringProperty( props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, pTitle );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY( displayID ) );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY( displayID ) );
		SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true );
		SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true );
		SDL_SetBooleanProperty( props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true );
		m_Window = SDL_CreateWindowWithProperties( props );
		SDL_DestroyProperties( props );
	}
	else
	{
		m_Window = SDL_CreateWindow( pTitle, width, height, flags );
	}

	if (m_Window == NULL)
		Error( "Failed to create SDL window: %s", SDL_GetError() );

#if defined( LINUX )
	SetAssertDialogParent( m_Window );
#endif

	m_WindowWidth = width;
	m_WindowHeight = height;

	return true;
}


int CSDLMgr::GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvent )
{
	SDLAPP_FUNC;

	m_CocoaEventsMutex.Lock();

	CUtlLinkedList<CCocoaEvent,int> &queue = debugEvent ? m_CocoaEvents : m_DebugEvents;

	int nAvailable = queue.Count();
	int nToWrite = MIN( nAvailable, nMaxEventsToReturn );

	CCocoaEvent *pCurEvent = pEvents;
	for ( int i=0; i < nToWrite; i++ )
	{
		int iHead = queue.Head();
		memcpy( pCurEvent, &queue[iHead], sizeof( CCocoaEvent ) );
		queue.Remove( iHead );
		++pCurEvent;
	}

	m_CocoaEventsMutex.Unlock();

	return nToWrite;
}

bool CSDLMgr::IsDebugEvent( CCocoaEvent& event )
{
	SDLAPP_FUNC;

	bool result = false;

	#if GLMDEBUG
		// simple rule for now, if the option key is involved, it's a debug key
		// but only if GLM debugging is builtin

		result |= ( (event.m_EventType == CocoaEvent_KeyDown) && ((event.m_ModifierKeyMask & (1<<eControlKey))!=0) );
	#endif

	return result;
}

// Set the mouse cursor position.
void CSDLMgr::SetCursorPosition( int x, int y )
{
	SDLAPP_FUNC;

	int windowHeight = 0;
	int windowWidth = 0;
	SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int rx, ry, width, height;
	pRenderContext->GetViewport( rx, ry, width, height );
	if ( width != windowWidth || height != windowHeight  )
	{
		// Scale from viewport coords to window coords
		x = x * (float)windowWidth/width;
		y = y * (float)windowHeight/height;
	}

    m_bExpectSyntheticMouseMotion = true;
	m_nMouseTargetX = x;
	m_nMouseTargetY = y;

	SDL_WarpMouseInWindow(m_Window, (float)x, (float)y);
}

void CSDLMgr::GetCursorPosition( int *px, int *py )
{
	float fx, fy;
	SDL_GetMouseState(&fx, &fy);
	int x = (int)fx, y = (int)fy;

	int windowHeight = 0;
	int windowWidth = 0;
	SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int rx, ry, width, height;
	pRenderContext->GetViewport( rx, ry, width, height );

	if ( width != windowWidth || height != windowHeight  )
	{
		// Scale from window coords to viewport coords so mouse matches VGUI
		x = x * (float)width/windowWidth;
		y = y * (float)height/windowHeight;
	}
	*px = x;
	*py = y;
}

void CSDLMgr::PostEvent( const CCocoaEvent &theEvent, bool debugEvent )
{
	SDLAPP_FUNC;

	m_CocoaEventsMutex.Lock();

	CUtlLinkedList<CCocoaEvent,int> &queue = debugEvent ? m_CocoaEvents : m_DebugEvents;
	queue.AddToTail( theEvent );

	m_CocoaEventsMutex.Unlock();
}

void CSDLMgr::SetMouseVisible( bool bState )
{
	SDLAPP_FUNC;

    m_bCursorVisible = bState;
    m_bSetMouseVisibleCalled = true;
}

void CSDLMgr::SetMouseCursor( SDL_Cursor *hCursor )
{
	SDLAPP_FUNC;

	if ( m_hCursor != hCursor )
	{
		if ( !hCursor )
		{
			// SDL_SetCursor( NULL ) just forces a cursor redraw, so don't ever bother doing that.
			SetMouseVisible( false );
		}
		else
		{
			SetMouseVisible( true );
		}

		m_hCursor = hCursor;
		m_bSetMouseCursorCalled = true;
	}
}

void CSDLMgr::OnFrameRendered()
{
	SDLAPP_FUNC;

	if ( !m_bHasFocus )
	{
		return;
	}

	if ( !m_bCursorVisible )
	{
		++m_nFramesCursorInvisibleFor;
	}

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	if ( m_nForceCursorVisible > 0 )
	{
		// Edge case: We were just asked to force the cursor visible, so do it now.
		if ( m_nForceCursorVisiblePrev == 0 )
		{
			SDL_SetCursor( m_hSystemArrowCursor );
			SDL_SetWindowMouseGrab( m_Window, false );
			SDL_SetWindowRelativeMouseMode( m_Window, false );
			SDL_ShowCursor();
		}

		// No further cursor processing.
		m_nForceCursorVisiblePrev = m_nForceCursorVisible;
		return;
	}
	else if ( m_nForceCursorVisiblePrev > 0 )
	{
		Assert( m_nForceCursorVisible == 0 );

		// Make sure to give the normal processing a shot at putting things
		// back correctly.
		m_bSetMouseCursorCalled = true;
		m_bSetMouseVisibleCalled = true;
	}

	m_nForceCursorVisiblePrev = m_nForceCursorVisible;
#endif

	if ( m_bCursorVisible && m_bSetMouseCursorCalled )
	{
		SDL_SetCursor( m_hCursor );

		m_bSetMouseCursorCalled = false;
	}

	if ( m_bSetMouseVisibleCalled )
	{
		ConVarRef rawinput( "m_rawinput" );

		m_bRawInput = !m_bCursorVisible && rawinput.IsValid() && rawinput.GetBool();

		bool bWindowGrab = !m_bCursorVisible;
		bool bRelativeMouseMode = bWindowGrab;

		if ( !m_bRawInput )
		{
			if ( m_bForbidMouseGrab )
				bWindowGrab = false;

			bRelativeMouseMode = false;
		}

		SDL_SetWindowMouseGrab( m_Window, bWindowGrab );
		SDL_SetWindowRelativeMouseMode( m_Window, bRelativeMouseMode );

		if ( m_bCursorVisible && m_hCursor )
			SDL_ShowCursor();
		else
			SDL_HideCursor();

		m_bSetMouseVisibleCalled = false;

		if ( m_bCursorVisible )
		{
			//if we were invisible for any number of frames and are set back to visible, then center the cursor.
			if ( m_nFramesCursorInvisibleFor > 60 )
			{
				int windowHeight = 0, windowWidth = 0;
				SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);
				SDL_WarpMouseInWindow( m_Window, (float)(windowWidth/2), (float)(windowHeight/2) );
			}

			m_nFramesCursorInvisibleFor = 0;
		}
	}
}


void CSDLMgr::SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen )
{
	SDLAPP_FUNC;

	// Guard fullscreen re-application: on Wayland, calling SDL_SetWindowFullscreenMode
	// on an already-fullscreen window re-sends xdg_toplevel_set_fullscreen, which can
	// move the window to a different output. This path is hit on every focus gain via
	// RestoreVideo -> AdjustWindow -> ChangeDisplaySettingsToFullscreen.
	// Only skip when already fullscreen. Windowed->windowed calls must still run
	// to handle resolution changes (SizeWindow).
	if ( bFullScreen && m_bFullScreen )
		return;

	if ( m_bFullScreen != bFullScreen )
	{
		if ( bFullScreen )
		{
			SDL_SetWindowFullscreenMode( m_Window, NULL );
		}

		SDL_SetWindowFullscreen( m_Window, bFullScreen );
		SDL_SyncWindow( m_Window );
		m_bFullScreen = bFullScreen;
	}

	if ( !bFullScreen )
	{
		SizeWindow( nWidth, nHeight );
	}

	if (bFullScreen)
	{
		int drawableW, drawableH;
		SDL_GetWindowSizeInPixels(m_Window, &drawableW, &drawableH);

		if ( drawableW > 0 && drawableH > 0 )
		{
			// Use drawable size for accurate mouse scaling
			m_flMouseXScale = (float)nWidth / (float)drawableW;
			m_flMouseYScale = (float)nHeight / (float)drawableH;
		}
		else
		{
			// Fallback: query desktop mode for the window's current display
			SDL_DisplayID displayID = SDL_GetDisplayForWindow( m_Window );
			if ( !displayID )
				displayID = SDL_GetPrimaryDisplay();
			const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode( displayID );
			if ( desktopMode )
			{
				m_flMouseXScale = (float)nWidth / (float)desktopMode->w;
				m_flMouseYScale = (float)nHeight / (float)desktopMode->h;
			}
			else
			{
				m_flMouseXScale = 1.0f;
				m_flMouseYScale = 1.0f;
			}
		}
	}
	else
	{
		// Use 1:1 scaling for windowed mode
		m_flMouseXScale = 1.0f;
		m_flMouseYScale = 1.0f;
	}
}


void CSDLMgr::MoveWindow( int x, int y )
{
	SDLAPP_FUNC;

	SDL_SetWindowPosition(m_Window, x, y);
}

void CSDLMgr::SizeWindow( int width, int tall )
{
	SDLAPP_FUNC;

	SDL_SetWindowSize(m_Window, width, tall);
	SDL_SyncWindow(m_Window);

	int actualW, actualH;
	SDL_GetWindowSize(m_Window, &actualW, &actualH);
	if (actualW > 0 && actualH > 0)
	{
		m_WindowWidth = actualW;
		m_WindowHeight = actualH;
	}
	else
	{
		m_WindowWidth = width;
		m_WindowHeight = tall;
	}
}


// key input handler
void CSDLMgr::handleKeyInput( const SDL_Event &event )
{
	SDLAPP_FUNC;

	Assert( ( event.type == SDL_EVENT_KEY_DOWN ) || ( event.type == SDL_EVENT_KEY_UP ) );

#ifdef OSX
	if ( event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB &&
	     SDL_GetModState()&KMOD_GUI && !CommandLine()->FindParm( "-noexclusivefs" ) )
	{
		// If we're in exclusive fullscreen mode, and they command-tab, handle
		// that by forcing minimization of the window.
		SDL_MinimizeWindow( m_Window );
	}
#endif

	const bool bPressed = ( event.type == SDL_EVENT_KEY_DOWN );

	// !!! FIXME: we should be getting text input from a different event...
	CCocoaEvent theEvent;
	theEvent.m_EventType = ( bPressed ) ? CocoaEvent_KeyDown : CocoaEvent_KeyUp;
	theEvent.m_VirtualKeyCode = event.key.scancode;
	theEvent.m_UnicodeKey = 0;
	theEvent.m_UnicodeKeyUnmodified = 0;

	// key modifiers aren't necessarily reliable in all the cases we'd want, so track it ourselves.
	const uint32_t ModCAPSLOCK = (1 << 0);
	const uint32_t ModSHIFTR   = (1 << 1);
	const uint32_t ModSHIFTL   = (1 << 2);
	const uint32_t ModCTRLR    = (1 << 3);
	const uint32_t ModCTRLL    = (1 << 4);
	const uint32_t ModALTR     = (1 << 5);
	const uint32_t ModALTL     = (1 << 6);
	const uint32_t ModGUIR     = (1 << 7);
	const uint32_t ModGUIL     = (1 << 8);

	#define KEYSYMCASE(mod,side,op,key) \
		case SDLK_##side##mod: \
			m_keyModifiers op Mod##mod##side; \
			theEvent.m_VirtualKeyCode = -key; \
			break

	//bool bDropKey = false;
	if (bPressed)
	{
		switch (event.key.key)
		{
			KEYSYMCASE(CAPSLOCK,,|=,KEY_CAPSLOCK);
			KEYSYMCASE(SHIFT,R,|=,KEY_RSHIFT);
			KEYSYMCASE(SHIFT,L,|=,KEY_LSHIFT);
			KEYSYMCASE(CTRL,R,|=,KEY_RCONTROL);
			KEYSYMCASE(CTRL,L,|=,KEY_LCONTROL);
			KEYSYMCASE(GUI,R,|=,KEY_RWIN);
			KEYSYMCASE(GUI,L,|=,KEY_LWIN);
			KEYSYMCASE(ALT,R,|=,KEY_RALT);
			KEYSYMCASE(ALT,L,|=,KEY_LALT);
			default: break;  // don't care.
		}
	}
	else
	{
		switch (event.key.key)
		{
			KEYSYMCASE(CAPSLOCK,,&= ~,KEY_CAPSLOCK);
			KEYSYMCASE(SHIFT,R,&= ~,KEY_RSHIFT);
			KEYSYMCASE(SHIFT,L,&= ~,KEY_LSHIFT);
			KEYSYMCASE(CTRL,R,&= ~,KEY_RCONTROL);
			KEYSYMCASE(CTRL,L,&= ~,KEY_LCONTROL);
			KEYSYMCASE(GUI,R,&= ~,KEY_RWIN);
			KEYSYMCASE(GUI,L,&= ~,KEY_LWIN);
			KEYSYMCASE(ALT,R,&= ~,KEY_RALT);
			KEYSYMCASE(ALT,L,&= ~,KEY_LALT);
			default: break;  // don't care.
		}
	}

	#undef KEYSYMCASE

	m_keyModifierMask = 0;
	if (m_keyModifiers & ModCAPSLOCK)
		m_keyModifierMask |= (1<<eCapsLockKey);
	if (m_keyModifiers & (ModSHIFTR | ModSHIFTL))
		m_keyModifierMask |= (1<<eShiftKey);
	if (m_keyModifiers & (ModCTRLR | ModCTRLL))
		m_keyModifierMask |= (1<<eControlKey);
	if (m_keyModifiers & (ModALTR | ModALTL))
		m_keyModifierMask |= (1<<eAltKey);
	if (m_keyModifiers & (ModGUIR | ModGUIL))
		m_keyModifierMask |= (1<<eCommandKey);

	theEvent.m_ModifierKeyMask = m_keyModifierMask;

	// make a decision about this event - does it go in the normal evt queue or into the debug queue.
	bool debug = IsDebugEvent( theEvent );

#if GLMDEBUG
	bool bIsShifted = ( ((theEvent.m_ModifierKeyMask & (1<<eCapsLockKey))!=0) || ((theEvent.m_ModifierKeyMask & (1<<eShiftKey))!=0) );
	theEvent.m_UnicodeKeyUnmodified = event.key.key;
	if ( bIsShifted )
	{
		switch ( event.key.key )
		{
			case '[':
				theEvent.m_UnicodeKeyUnmodified = '{';
				break;
			case ']':
				theEvent.m_UnicodeKeyUnmodified = '}';
				break;
			case 'h':
				theEvent.m_UnicodeKeyUnmodified = 'H';
				break;
			case ',':
				theEvent.m_UnicodeKeyUnmodified = '<';
				break;
			case '.':
				theEvent.m_UnicodeKeyUnmodified = '>';
				break;
		}
	}
#endif

	PostEvent( theEvent, debug );
}

void CSDLMgr::PumpWindowsMessageLoop()
{
	SDLAPP_FUNC;

	SDL_Event event;
	int nEventsProcessed = 0;
	while ( SDL_PollEvent(&event) && nEventsProcessed < 100 )
	{
		nEventsProcessed++;

		switch ( event.type )
		{
			case SDL_EVENT_MOUSE_MOTION:
			{
                if( m_bHasFocus == false )
					{
					break;
					}

				// When SDL_WarpMouseInWindow is called, an SDL_EVENT_MOUSE_MOTION
				// event is sent. We want to ignore such 'synthetic'
				// mouse motion events.
				if ( m_bExpectSyntheticMouseMotion &&
					 (int)event.motion.x == m_nMouseTargetX &&
					 (int)event.motion.y == m_nMouseTargetY )
				{
					m_bExpectSyntheticMouseMotion = false;
					break;
				}

                m_nMouseXDelta += (int)event.motion.xrel;
                m_nMouseYDelta += (int)event.motion.yrel;

				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_MouseMove;
				// Scale mouse coords from window space to render resolution
				{
					int mx = (int)event.motion.x;
					int my = (int)event.motion.y;
					if ( m_bCursorVisible )
					{
						int bw, bh;
						g_pMaterialSystem->GetBackBufferDimensions( bw, bh );
						if ( bw > 0 && bh > 0 && m_WindowWidth > 0 && m_WindowHeight > 0 &&
							 (bw != m_WindowWidth || bh != m_WindowHeight) )
						{
							mx = mx * bw / m_WindowWidth;
							my = my * bh / m_WindowHeight;
						}
					}
					theEvent.m_MousePos[0] = mx;
					theEvent.m_MousePos[1] = my;
				}
				theEvent.m_MouseButtonFlags = m_mouseButtons;
				PostEvent( theEvent );
				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				// SDL buttons:
				//  1 = Left button
				//  2 = Middle button
				//  3 = Right button
				//  4 = Wheel Forward		; These events are handled by SDL_MOUSEWHEEL and don't come through here.
				//  5 = Wheel Back			;
				//  6 = Wheel Tilt Left		;
				//  7 = Wheel Tilt Right	;
				//  8 = Browser Back
				//  9 = Browser Forward
				//  10 = Task button (probably similar to Alt-Tab)

				// every other platform does left(1), right(2), middle(3)...
				int button;

				switch( event.button.button )
				{
				case 1:	button = 1; break;
				case 2: button = 3; break;
				case 3: button = 2; break;
				default:
					// For all buttons above 4, map them to 4 & 5 forever. This is because different mice
					//	will use different mappings. Ie, mousewheel mice can do this:
					//    4 = Wheel Forward		; These events are handled by SDL_MOUSEWHEEL and don't come through here.
					//    5 = Wheel Back		;
					//    6 = Wheel Tilt Left	;
					//    7 = Wheel Tilt Right	;
					//    8 = Browser Back
					//    9 = Browser Forward
					//    10 = Task button (probably similar to Alt-Tab)
					// Mice without wheels can do 4/5 as regular 4/5, etc.
					button = 4 + ( event.button.button & 0x1 );
					break;
				}

				const bool bPressed = event.button.down;
				const CocoaMouseButton_t cocoaButton = ( CocoaMouseButton_t )( 1 << (button - 1 ) );

				if (bPressed)
					m_mouseButtons |= cocoaButton;
				else
					m_mouseButtons &= ~cocoaButton;

				bool bDoublePress = false;

				if (bPressed)
				{
					if ( m_bGotMouseButtonDown &&
						 ( (int)( event.button.timestamp - m_MouseButtonDownTimeStamp ) <= sdl_double_click_time.GetInt() ) &&
						 ( abs( (int)event.button.x - m_MouseButtonDownX ) <= sdl_double_click_size.GetInt() ) &&
						 ( abs( (int)event.button.y - m_MouseButtonDownY ) <= sdl_double_click_size.GetInt() ) )
					{
						bDoublePress = true;
						m_bGotMouseButtonDown = false;
					}
					else
					{
						m_MouseButtonDownTimeStamp = event.button.timestamp;
						m_MouseButtonDownX = (int)event.button.x;
						m_MouseButtonDownY = (int)event.button.y;
						m_bGotMouseButtonDown = true;
					}
				}

				CCocoaEvent theEvent;
				theEvent.m_EventType = (bPressed) ? CocoaEvent_MouseButtonDown : CocoaEvent_MouseButtonUp;
				// Scale mouse coords from window space to viewport space to match VGUI
				{
					int mx = (int)event.button.x;
					int my = (int)event.button.y;
					if ( m_bCursorVisible )
					{
						CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
						int rx, ry, vw, vh;
						pRenderContext->GetViewport( rx, ry, vw, vh );
						if ( vw > 0 && vh > 0 && m_WindowWidth > 0 && m_WindowHeight > 0 &&
							 (vw != m_WindowWidth || vh != m_WindowHeight) )
						{
							mx = mx * vw / m_WindowWidth;
							my = my * vh / m_WindowHeight;
						}
					}
					theEvent.m_MousePos[0] = mx;
					theEvent.m_MousePos[1] = my;
				}
				theEvent.m_MouseButtonFlags = m_mouseButtons;
				theEvent.m_nMouseClickCount = bDoublePress ? 2 : 1;
				theEvent.m_MouseButton = cocoaButton;
				PostEvent( theEvent );

				break;
			}

			case SDL_EVENT_MOUSE_WHEEL:
			{
				int scroll = event.wheel.y;

#ifdef OSX
				if ( scroll == 0 && ( SDL_GetModState()&KMOD_SHIFT ) )
				{
					scroll = -event.wheel.x;
				}
#endif

				if ( scroll )
				{
					CCocoaEvent theEvent;
					theEvent.m_EventType = CocoaEvent_MouseScroll;
					theEvent.m_MousePos[0] = scroll;
					theEvent.m_MousePos[1] = scroll;
					PostEvent( theEvent );
				}
				break;
			}

			case SDL_EVENT_WINDOW_EXPOSED:
			{
				/*if ( ev.xexpose.count > 0 )
					break; // multiple expose events queued
				EVENT_LOG( "Got event Expose\n" );
				int iPanel = m_mapWindowToVPanel.Find( ev.xexpose.window );
				if ( iPanel != m_mapWindowToVPanel.InvalidIndex() )
					drawVGUI( m_pXDisplay, ev.xexpose.window, m_mapWindowToVPanel[ iPanel ], m_GLContext );
				m_mapSentInvalidate.RemoveAll();*/
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			{
				m_bHasFocus = true;
				m_bSetMouseVisibleCalled = true;

				// SDL3: text input is off by default and may be reset by
				// compositor changes. Re-enable on every focus gain.
				SDL_StartTextInput( m_Window );

				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_AppActivate;
				theEvent.m_ModifierKeyMask = 1;
				PostEvent( theEvent );
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_LOST:
			{
				m_bHasFocus = false;

				SDL_SetWindowMouseGrab( m_Window, false );
				SDL_SetWindowRelativeMouseMode( m_Window, false );
				SDL_ShowCursor();

				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_AppActivate;
				theEvent.m_ModifierKeyMask = 0;
				PostEvent( theEvent );

				// Reset our key modifiers. This also happens in CocoaEvent_AppActivate in inputsystem.cpp for
				//	the g_pInputSystem, and WM_ACTIVATEAPP on Windows in that file.
				m_keyModifiers = 0;
				// Reset SDL state as well. SDL_keyboard.modstate in SDL_keyboard.c gets waaay out of alignment.
				SDL_SetModState( SDL_KMOD_NONE );
				break;
			}
#ifdef LINUX
			case SDL_EVENT_WINDOW_RESIZED:
			{
				int newWidth = event.window.data1;
				int newHeight = event.window.data2;

				if ( newWidth > 0 && newHeight > 0 )
				{
					if ( newWidth != m_WindowWidth || newHeight != m_WindowHeight )
					{
						m_WindowWidth = newWidth;
						m_WindowHeight = newHeight;

						// Don't update sdl_displayindex here — automatic updates
						// lock the convar to whichever monitor the compositor picked,
						// preventing the user from moving the window.
						// sdl_displayindex should only change via explicit user action.

						CCocoaEvent theEvent;
						theEvent.m_EventType = CocoaEvent_WindowSizeChanged;
						theEvent.m_MousePos[0] = newWidth;
						theEvent.m_MousePos[1] = newHeight;
						PostEvent( theEvent );
					}
				}
				break;
			}
#endif

			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_KEY_DOWN:
				if(event.type == SDL_EVENT_KEY_DOWN && event.key.repeat &&
				   (event.key.key == SDLK_BACKSPACE ||
				    event.key.key == SDLK_DELETE))
				{
					// If we have repeated keydown events, we want to
					// generate a synthetic keyup event, since Scaleform
					// doesn't behave well getting multiple keydown events
					// without corresponding keyups.
					event.type = SDL_EVENT_KEY_UP;
					handleKeyInput(event);
					event.type = SDL_EVENT_KEY_DOWN;
				}

				handleKeyInput(event);
				break;

			case SDL_EVENT_TEXT_INPUT:
			{
				const char *text = event.text.text;

				if ( text && text[ 0 ] )
				{
					// SDL3: text is now a const char* (was fixed 32-byte array in SDL2)
					static const int kMaxTextInputChars = 32;
					wchar_t WBuf[ kMaxTextInputChars + 1 ];
					WBuf[ 0 ] = 0;
					V_UTF8ToUnicode( text, WBuf, sizeof( WBuf ) );

					for ( int i = 0; i < kMaxTextInputChars; i++ )
					{
						wchar_t ch = WBuf[ i ];
						if ( ch == '\0' )
							break;

						CCocoaEvent theEvent;
						theEvent.m_EventType = CocoaEvent_KeyDown;
						theEvent.m_VirtualKeyCode = 0;
						theEvent.m_UnicodeKey = ch;
						theEvent.m_UnicodeKeyUnmodified = ch;
						theEvent.m_ModifierKeyMask = m_keyModifierMask;
						PostEvent( theEvent, false );

						theEvent.m_EventType = CocoaEvent_KeyUp;
						theEvent.m_VirtualKeyCode = 0;
						theEvent.m_UnicodeKey = 0;
						theEvent.m_UnicodeKeyUnmodified = 0;
						theEvent.m_ModifierKeyMask = m_keyModifierMask;
						PostEvent( theEvent, false );
					}
				}
				break;
			}

			case SDL_EVENT_QUIT:
			{
				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_AppQuit;
				PostEvent( theEvent );
				break;
			}

			default:
				break;
		}
	}
}


void CSDLMgr::DestroyGameWindow()
{
	SDLAPP_FUNC;

	if ( m_Window )
	{
		SDL_SetWindowFullscreen(m_Window, false);  // just in case.
		SDL_SetWindowMouseGrab(m_Window, false);  // just in case.
		SDL_DestroyWindow(m_Window);
		m_Window = NULL;
	}
}


void CSDLMgr::SetApplicationIcon( const char *pchAppIconFile )
{
	SDLAPP_FUNC;

	SDL_Surface *icon = SDL_LoadBMP(pchAppIconFile);
	if (icon)
	{
		SDL_SetWindowIcon(m_Window, icon);
		SDL_DestroySurface(icon);
	}
}

void CSDLMgr::GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta )
{
	SDLAPP_FUNC;

    x = m_nMouseXDelta * (m_bCursorVisible ? m_flMouseXScale : 1.0);
    y = m_nMouseYDelta * (m_bCursorVisible ? m_flMouseYScale : 1.0);

	m_nMouseXDelta = m_nMouseYDelta = 0;
}

//  Returns the current active display index
//
int CSDLMgr::GetActiveDisplayIndex()
{
	// Always return the display the window is actually on right now.
	// Don't modify sdl_displayindex — that convar is only for explicit user overrides.
	SDL_DisplayID activeDisplayID = SDL_GetDisplayForWindow( m_Window );
	int activeDisplayindex = 0;
	{
		int dCount = 0;
		SDL_DisplayID *dList = SDL_GetDisplays( &dCount );
		if ( dList )
		{
			for ( int di = 0; di < dCount; di++ )
			{
				if ( dList[di] == activeDisplayID )
				{
					activeDisplayindex = di;
					break;
				}
			}
			SDL_free( dList );
		}
	}

	return activeDisplayindex;
}

//  Returns the resolution of the nth display. 0 is the default display.
//
void CSDLMgr::GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz )
{
	if ( nDisplay == -1 )
	{
		nDisplay = g_bSDLDisplayindexSet ? sdl_displayindex.GetInt() : -1;
	}

	// Resolve display ID: -1 uses the window's current output
	SDL_DisplayID displayID;
	if ( nDisplay < 0 )
	{
		displayID = m_Window ? SDL_GetDisplayForWindow( m_Window ) : 0;
		if ( !displayID )
			displayID = SDL_GetPrimaryDisplay();
	}
	else
	{
		int displayCount = 0;
		SDL_DisplayID *displays = SDL_GetDisplays( &displayCount );
		displayID = ( displays && nDisplay < displayCount ) ? displays[nDisplay] : SDL_GetPrimaryDisplay();
		if ( displays ) SDL_free( displays );
	}

	const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode( displayID );
	if ( !mode )
	{
		mode = SDL_GetDesktopDisplayMode( SDL_GetPrimaryDisplay() );
	}

	if ( mode )
	{
		nRefreshHz = (uint)mode->refresh_rate;
		nWidth = mode->w;
		nHeight = mode->h;
	}
	else
	{
		nRefreshHz = 0;
		nWidth = 0;
		nHeight = 0;
	}
}


void CSDLMgr::RenderedSize( uint &width, uint &height, bool set )
{
	SDLAPP_FUNC;

	if (set)
	{
		m_renderedWidth = width;
		m_rendererHeight = height;	// latched from NotifyRenderedSize
	}
	else
	{
		width = m_renderedWidth;
		height = m_rendererHeight;
	}
}

void CSDLMgr::DisplayedSize( uint &width, uint &height )
{
	SDLAPP_FUNC;

	int w, h;
	SDL_GetWindowSize(m_Window, &w, &h);
	width = (uint) w;
	height = (uint) h;
}

void CSDLMgr::GetStackCrawl( CStackCrawlParams *params )
{
	SDLAPP_FUNC;
}

void CSDLMgr::WaitUntilUserInput( int msSleepTime )
{
	SDLAPP_FUNC;
	if ( m_bTextMode )
		return;

	SDL_WaitEventTimeout(NULL, msSleepTime);
}

static KeyValues *LoadCursorResource()
{
	static const char *pPath = "resource/cursor/cursor.res";
	KeyValues *pKeyValues = new KeyValues( pPath );
	const bool bLoadedCursorResource = pKeyValues->LoadFromFile( g_pFullFileSystem, pPath );
	Assert( bLoadedCursorResource );
	return pKeyValues;
}

InputCursorHandle_t CSDLMgr::LoadCursorFromFile( const char *pchFileName )
{
	// On SDL we don't support .ani files, like are used on Windows. Instead,
	// we expect there to be a .bmp file in the same location which will
	// contain the image for the cursor. (This means we don't support
	// animated or scaling cursors).
	char path[PATH_MAX];
	V_StripExtension( pchFileName, path, sizeof( path ) );
	V_strcat( path, ".bmp", sizeof( path ) );

	SDL_Surface *surface = SDL_LoadBMP( path );
	if ( surface == NULL )
	{
		Warning( "Failed to load image for cursor from %s: %s\n", path, SDL_GetError() );
		return NULL;
	}

	// The cursor resource file contains information on the cursor's
	// x,y hotspot. Load it and find the x,y hotspot.
	static KeyValues *pCursorResource = LoadCursorResource();

	char pchCursorName[PATH_MAX];
	V_FileBase( path, pchCursorName, sizeof( pchCursorName ) );

	int nHotX = 0, nHotY = 0;

	KeyValues *pRes = pCursorResource->FindKey( pchCursorName );
	if ( pRes != NULL )
	{
		nHotX = pRes->GetInt( "hotx" );
		nHotY = pRes->GetInt( "hoty" );
	}

	SDL_Cursor *cursor = SDL_CreateColorCursor( surface, nHotX, nHotY );
	if( cursor == NULL )
	{
		Warning( "Failed to load cursor from %s: %s\n", path, SDL_GetError() );
		return NULL;
	}
	return reinterpret_cast< InputCursorHandle_t >( cursor );
}

void CSDLMgr::FreeCursor( const InputCursorHandle_t pchCursor )
{
	SDL_DestroyCursor( reinterpret_cast< SDL_Cursor* >( pchCursor ) );
}

void CSDLMgr::SetCursorIcon( const InputCursorHandle_t pchCursor )
{
	SDL_Cursor *cursor = reinterpret_cast< SDL_Cursor* >( pchCursor );
	SDL_SetCursor( cursor );
}


//===============================================================================

void CSDLMgr::SetGammaRamp( const uint16 *pRed, const uint16 *pGreen, const uint16 *pBlue )
{
	// SDL3: SDL_SetWindowGammaRamp was removed. Gamma ramp support is no longer
	// available through SDL - use platform-specific or shader-based approaches instead.
	static bool bWarned = false;
	if ( !bWarned )
	{
		ConMsg( "SetGammaRamp: not supported in SDL3 (gamma ramp API removed)\n" );
		bWarned = true;
	}
}

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
//===============================================================================
void CSDLMgr::ForceSystemCursorVisible()
{
	Assert( m_nForceCursorVisible >= 0 );
	m_nForceCursorVisible += 1;
}

//===============================================================================
void CSDLMgr::UnforceSystemCursorVisible()
{
	Assert( m_nForceCursorVisible >= 1 );
	m_nForceCursorVisible -= 1;
}

#endif


#endif  // !DEDICATED
