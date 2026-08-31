//====== Copyright  1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: An application framework
//
//=============================================================================//

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "appframework/ilaunchermgr.h"
#include "appframework/sdlwindow.h"
#include "tier1/keyvalues.h"
#include "filesystem.h"

#include "materialsystem/imaterialsystem.h"
#include "togl/rendermechanism.h"
#include "tier0/fasttimer.h"


// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"


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
	virtual int GetEvents( SDL_Event *pEvents, int nMaxEventsToReturn );

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y );
	virtual void GetCursorPosition( int *px, int *py );

	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen );
	virtual void PumpWindowsMessageLoop();

	// Not part of ILauncherMgr; used internally by CreateGameWindow()/SetWindowFullScreen().
	void MoveWindow( int x, int y );
	void SizeWindow( int width, int tall );

	virtual void DestroyGameWindow();
	virtual void SetApplicationIcon( const char *pchAppIconFile );

	virtual void GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta = false );

	virtual int GetActiveDisplayIndex();
	// Not part of ILauncherMgr; used internally by CreateGameWindow().
	void GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz );

  	virtual InputCursorHandle_t LoadCursorFromFile( const char *pchFileName );

	virtual void FreeCursor( const InputCursorHandle_t pchCursor );
	virtual void SetCursorIcon( const InputCursorHandle_t pchCursor );

	// Post an event to the input event queue.
	void PostEvent( const SDL_Event &theEvent );

	// Map SDL window coordinates into engine screen space.
	virtual void WindowToEngineCoords( float wx, float wy, int &ex, int &ey );
	void EngineToWindowCoords( int ex, int ey, float &wx, float &wy );

	virtual void SetMouseVisible( bool bState );

	// Push the desired pointer state to SDL. Called when intent changes, not per frame.
	void ApplyPointerState();
	virtual void SetMouseCursor( SDL_Cursor *hCursor );
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab ) { m_bForbidMouseGrab = bForbidMouseGrab; }

	virtual void OnFrameRendered();

	// Returns all dependent libraries
	virtual const AppSystemInfo_t* GetDependencies() {return NULL;}

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


	SDL_Window *m_Window;


	bool m_bCursorVisible;
	int m_nFramesCursorInvisibleFor;
	SDL_Cursor *m_hCursor;

	bool m_bHasFocus;
	bool m_bFullScreen;
	bool m_bForbidMouseGrab;  // temporary setting showing if the mouse should
	                          // grab if possible.

	float m_flMouseXDelta;
	float m_flMouseYDelta;

	int m_ScreenWidth;
	int m_ScreenHeight;


	int m_WindowWidth;
	int m_WindowHeight;

    bool m_bExpectSyntheticMouseMotion;
    int  m_nMouseTargetX;
    int  m_nMouseTargetY;
    int  m_nWarpDelta;

	int m_lastKnownSwapInterval;	//-2 if unknown, 0/1/-1 otherwise
	int m_lastKnownSwapLimit;		//-1 if unknown, 0/1 otherwise

	int m_pixelFormatAttribs[32];
	int m_pixelFormatAttribCount;


	// Event queue. Produced by PumpWindowsMessageLoop() and drained by
	// CInputSystem::PollInputState_Linux(), which calls PumpWindowsMessageLoop()
	// itself -- so this is single threaded and needs no lock. A fixed ring also
	// avoids a heap allocation per event, which matters at high mouse polling rates.
	// Sized for two pumps per drain (2 x 100 SDL events x up to 2 entries each),
	// since PostEvent() drops on overflow and a dropped KEY_UP is a stuck key.
	static const int kMaxQueuedEvents = 512;
	SDL_Event m_Events[ kMaxQueuedEvents ];
	// Ring-owned copy of SDL_TextInputEvent::text. SDL allocates that string itself
	// and recycles it on the next pump, so it must not be read after PostEvent()
	// returns; each queued text event points at its slot here instead.
	char m_EventText[ kMaxQueuedEvents ][ 64 ];
	int m_nEventsHead;							// next slot to read
	int m_nEventsCount;



#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	int m_nForceCursorVisible;
	int m_nForceCursorVisiblePrev;
	SDL_Cursor* m_hSystemArrowCursor;
#endif


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
	SDL_Window *pWindow = GetGameSDLWindow();
	if ( pWindow )
	{
		bool bGrab;

		if ( args.ArgC() >= 2 )
		{
			bGrab = ( args[ 1 ][ 0 ] == '1' ) ? true : false;
		}
		else
		{
			bGrab = SDL_GetWindowMouseGrab( pWindow ) ? false : true;
		}

		if ( g_pLauncherMgr )
			g_pLauncherMgr->SetForbidMouseGrab( !bGrab );

		if ( bGrab != SDL_GetWindowMouseGrab( pWindow ) )
		{
			Msg( "SetWindowGrab( %s )\n", bGrab ? "true" : "false" );
			SDL_SetWindowMouseGrab( pWindow, bGrab );

			// force non-fullscreen windows to the foreground if grabbed, so you can't
			//  get your mouse locked to something in the background.
			if ( bGrab && !( SDL_GetWindowFlags( pWindow ) & SDL_WINDOW_FULLSCREEN ) )
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

	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "wayland,x11" );
	SDL_SetHint( SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1" );
	// Never run relative motion through the desktop pointer curve: mouselook wants
	// the device deltas. This is SDL's default, stated because it is load-bearing.
	SDL_SetHint( SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0" );

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
	m_nFramesCursorInvisibleFor = 0;
	m_hCursor = NULL;


	m_bHasFocus = true;

	m_Window = NULL;
	m_bFullScreen = false;
	m_nEventsHead = 0;
	m_nEventsCount = 0;
	m_flMouseXDelta = 0.0f;
	m_flMouseYDelta = 0.0f;
	m_ScreenWidth = 0;
	m_ScreenHeight = 0;
	m_WindowWidth = 0;
	m_WindowHeight = 0;
	m_pixelFormatAttribCount = 0;
	m_lastKnownSwapInterval = -2;
	m_lastKnownSwapLimit = -1;


	m_bExpectSyntheticMouseMotion = false;
	m_nMouseTargetX = 0;
	m_nMouseTargetY = 0;
	m_nWarpDelta = 0;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	m_nForceCursorVisible = 0;
	m_nForceCursorVisiblePrev = 0;
	m_hSystemArrowCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_DEFAULT );
#endif


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

	// Publish for every other module; see appframework/sdlwindow.h.
	SetGameSDLWindow( m_Window );

	SetAssertDialogParent( m_Window );

	// The compositor decides the final size; latch what we actually got, since
	// WindowToEngineCoords() divides by it.
	SDL_GetWindowSize( m_Window, &m_WindowWidth, &m_WindowHeight );

	return true;
}


int CSDLMgr::GetEvents( SDL_Event *pEvents, int nMaxEventsToReturn )
{
	SDLAPP_FUNC;

	const int nToWrite = MIN( m_nEventsCount, nMaxEventsToReturn );

	for ( int i = 0; i < nToWrite; i++ )
	{
		pEvents[i] = m_Events[ m_nEventsHead ];
		m_nEventsHead = ( m_nEventsHead + 1 ) % kMaxQueuedEvents;
	}
	m_nEventsCount -= nToWrite;

	return nToWrite;
}

// Set the mouse cursor position, given engine screen space coordinates.
void CSDLMgr::SetCursorPosition( int x, int y )
{
	SDLAPP_FUNC;

	float wx, wy;
	EngineToWindowCoords( x, y, wx, wy );

    m_bExpectSyntheticMouseMotion = true;
	m_nMouseTargetX = (int)wx;
	m_nMouseTargetY = (int)wy;

	SDL_WarpMouseInWindow( m_Window, wx, wy );
}

// Map SDL window coordinates into engine screen space.
//
// The engine's screen space is the backbuffer: engine->GetScreenSize() calls
// IMatRenderContext::GetWindowSize(), which delegates to
// CShaderDeviceBase::GetWindowSize(), and under USE_SDL that returns
// GetBackBufferDimensions(). VGUI and RocketUI therefore work in backbuffer
// pixels, and this is the only conversion a cursor position needs.
//
// m_WindowWidth/Height are the LOGICAL window size (tracked from
// SDL_EVENT_WINDOW_RESIZED), which is the same space SDL reports mouse
// coordinates in. Normalising by them gives a [0,1] fraction, so scaling that by
// the pixel-space backbuffer is correct at any pixel density. Do not substitute
// SDL_GetWindowSizeInPixels() here: that mixes logical and pixel spaces, and only
// happens to agree while the window lacks SDL_WINDOW_HIGH_PIXEL_DENSITY.
//
// Deliberately NOT IMatRenderContext::GetViewport(): the viewport is transient
// render state (the 3D view installs a sub-rect partway through the frame), so a
// viewport-based mapping silently depends on where in the frame the event was
// pumped. Two of the three cursor paths used to do that, which is why they
// disagreed with each other.
void CSDLMgr::WindowToEngineCoords( float wx, float wy, int &ex, int &ey )
{
	int bw = 0, bh = 0;
	if ( g_pMaterialSystem )
		g_pMaterialSystem->GetBackBufferDimensions( bw, bh );

	if ( bw > 0 && bh > 0 && m_WindowWidth > 0 && m_WindowHeight > 0 &&
	     ( bw != m_WindowWidth || bh != m_WindowHeight ) )
	{
		ex = (int)( wx * (float)bw / (float)m_WindowWidth );
		ey = (int)( wy * (float)bh / (float)m_WindowHeight );
	}
	else
	{
		ex = (int)wx;
		ey = (int)wy;
	}
}

// Inverse of WindowToEngineCoords: engine screen space back into SDL window
// coordinates, for warping the cursor.
void CSDLMgr::EngineToWindowCoords( int ex, int ey, float &wx, float &wy )
{
	int bw = 0, bh = 0;
	// NULL before tier2 is connected and again after shutdown.
	if ( g_pMaterialSystem )
		g_pMaterialSystem->GetBackBufferDimensions( bw, bh );

	if ( bw > 0 && bh > 0 && m_WindowWidth > 0 && m_WindowHeight > 0 &&
	     ( bw != m_WindowWidth || bh != m_WindowHeight ) )
	{
		wx = (float)ex * (float)m_WindowWidth / (float)bw;
		wy = (float)ey * (float)m_WindowHeight / (float)bh;
	}
	else
	{
		wx = (float)ex;
		wy = (float)ey;
	}
}

void CSDLMgr::GetCursorPosition( int *px, int *py )
{
	float fx, fy;
	SDL_GetMouseState( &fx, &fy );
	WindowToEngineCoords( fx, fy, *px, *py );
}

void CSDLMgr::PostEvent( const SDL_Event &theEvent )
{
	SDLAPP_FUNC;

	if ( m_nEventsCount >= kMaxQueuedEvents )
	{
		// Dropping the newest event is better than overwriting one not yet read.
		AssertMsg( false, "CSDLMgr event queue overflow" );
		return;
	}

	const int nSlot = ( m_nEventsHead + m_nEventsCount ) % kMaxQueuedEvents;
	m_Events[ nSlot ] = theEvent;

	if ( theEvent.type == SDL_EVENT_TEXT_INPUT )
	{
		// See m_EventText: the string SDL handed us does not outlive this call.
		V_strncpy( m_EventText[ nSlot ],
		           theEvent.text.text ? theEvent.text.text : "",
		           sizeof( m_EventText[ nSlot ] ) );

		// V_strncpy truncates on a byte boundary, which can leave a partial UTF-8
		// sequence at the end for a long IME commit; back up to the last start byte
		// so the consumer never sees an invalid encoding.
		char *pText = m_EventText[ nSlot ];
		int nLen = V_strlen( pText );
		while ( nLen > 0 && ( (unsigned char)pText[ nLen - 1 ] & 0xC0 ) == 0x80 )
			--nLen;							// drop trailing continuation bytes
		if ( nLen > 0 )
		{
			const unsigned char cLead = (unsigned char)pText[ nLen - 1 ];
			const int nExpected = ( cLead < 0x80 ) ? 1 : ( cLead >= 0xF0 ) ? 4 : ( cLead >= 0xE0 ) ? 3 : ( cLead >= 0xC0 ) ? 2 : 1;
			if ( V_strlen( pText ) - ( nLen - 1 ) < nExpected )
				pText[ nLen - 1 ] = '\0';	// lead byte without its full sequence
		}

		m_Events[ nSlot ].text.text = m_EventText[ nSlot ];
	}

	m_nEventsCount++;
}

// SDL owns the pointer lifecycle: relative mode is a window flag
// (SDL_WINDOW_MOUSE_RELATIVE_MODE) whose effects SDL scopes to focus itself, and the
// Wayland backend re-derives the lock from that flag as focus moves. So state it once
// per intent change and leave it alone -- do not clear it on focus loss, and do not
// re-assert it per frame (SDL_SetWindowRelativeMouseMode flushes pending motion).
void CSDLMgr::ApplyPointerState()
{
	if ( !m_Window )
		return;

	// Hidden cursor means mouselook: relative mode gives unaccelerated, sub-pixel
	// deltas with the pointer locked, via the Wayland relative-pointer and
	// pointer-constraints protocols.
	const bool bRelativeMouseMode = !m_bCursorVisible;
	const bool bWindowGrab = bRelativeMouseMode && !m_bForbidMouseGrab;
	const bool bShowCursor = m_bCursorVisible && m_hCursor;

	// Compare against SDL, never against a cached copy of what we last asked for. The
	// window is recreated and reset underneath us (mode switches, device resets), so a
	// cache says nothing about reality -- and SDL_SetWindowRelativeMouseMode flushes
	// pending motion, so we must not call it when nothing actually changed either.
	if ( SDL_GetWindowRelativeMouseMode( m_Window ) != bRelativeMouseMode )
		SDL_SetWindowRelativeMouseMode( m_Window, bRelativeMouseMode );

	if ( SDL_GetWindowMouseGrab( m_Window ) != bWindowGrab )
		SDL_SetWindowMouseGrab( m_Window, bWindowGrab );

	if ( bShowCursor )
	{
		SDL_SetCursor( m_hCursor );
		if ( !SDL_CursorVisible() )
			SDL_ShowCursor();
	}
	else if ( SDL_CursorVisible() )
	{
		SDL_HideCursor();
	}
}

void CSDLMgr::SetMouseVisible( bool bState )
{
	SDLAPP_FUNC;

	const bool bBecameVisible = ( bState && !m_bCursorVisible );

	m_bCursorVisible = bState;

	// Warp before relative mode ends, per SDL: that is what decides where the cursor
	// reappears. Only bother if mouselook has been holding the pointer for a while.
	if ( bBecameVisible )
	{
		if ( m_nFramesCursorInvisibleFor > 60 )
		{
			int windowWidth = 0, windowHeight = 0;
			SDL_GetWindowSize( m_Window, &windowWidth, &windowHeight );
			SDL_WarpMouseInWindow( m_Window, (float)( windowWidth / 2 ), (float)( windowHeight / 2 ) );
		}

		m_nFramesCursorInvisibleFor = 0;
	}

	ApplyPointerState();
}

void CSDLMgr::SetMouseCursor( SDL_Cursor *hCursor )
{
	SDLAPP_FUNC;

	if ( m_hCursor != hCursor )
	{
		m_hCursor = hCursor;

		// SDL_SetCursor( NULL ) just forces a cursor redraw, so no cursor means hide.
		SetMouseVisible( hCursor != NULL );

		// ...and if visibility did not change, the cursor shape still did.
		ApplyPointerState();
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

		// Overlay let go of the cursor: put our own intent back.
		ApplyPointerState();
	}

	m_nForceCursorVisiblePrev = m_nForceCursorVisible;
#endif

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
		// Display we intend to steer the window onto (0 = leave it where it is).
		// Declared out here so the post-fullscreen sanity check (D) can use it.
		SDL_DisplayID targetDisplay = 0;

		if ( bFullScreen )
		{
			// Steer FULLSCREEN_DESKTOP onto the intended monitor. We keep
			// FULLSCREEN_DESKTOP (SDL owns real fullscreen); crispness comes from
			// the backbuffer tracking the surface pixel size, so all we need is to
			// get the window onto the right output before going fullscreen.
			// Otherwise we fullscreen on whatever output the window currently sits
			// on (e.g. a smaller internal panel), producing a scaled/blurry image.
			int dCount = 0;
			SDL_DisplayID *dList = SDL_GetDisplays( &dCount );

			if ( dList && dCount > 0 )
			{
				// 1. Explicit user override wins. This is the only disambiguator
				//    for two identical-resolution monitors. sdl_displayindex is an
				//    index into SDL_GetDisplays(), not a raw SDL_DisplayID.
				int idx = sdl_displayindex.GetInt();
				if ( idx >= 0 && idx < dCount )
				{
					targetDisplay = dList[idx];
				}

				// 2. Exact desktop-mode match for the requested resolution. If
				//    several displays match, prefer the one the window is already
				//    on, else the primary, else the first match.
				if ( !targetDisplay && nWidth > 0 && nHeight > 0 )
				{
					SDL_DisplayID curDisplay = SDL_GetDisplayForWindow( m_Window );
					SDL_DisplayID primary = SDL_GetPrimaryDisplay();
					SDL_DisplayID firstMatch = 0;
					for ( int i = 0; i < dCount; i++ )
					{
						const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode( dList[i] );
						if ( dm && dm->w == nWidth && dm->h == nHeight )
						{
							if ( dList[i] == curDisplay )
							{
								targetDisplay = dList[i];
								break;
							}
							if ( !firstMatch )
								firstMatch = dList[i];
							if ( dList[i] == primary )
								targetDisplay = dList[i];
						}
					}
					if ( !targetDisplay )
						targetDisplay = firstMatch;
				}

				// 3. Fall back to the largest display by area.
				if ( !targetDisplay )
				{
					int bestArea = 0;
					for ( int i = 0; i < dCount; i++ )
					{
						SDL_Rect rect = { 0, 0, 0, 0 };
						SDL_GetDisplayBounds( dList[i], &rect );
						int area = rect.w * rect.h;
						if ( area > bestArea )
						{
							bestArea = area;
							targetDisplay = dList[i];
						}
					}
				}
			}

			// 4. If we picked a specific display, move the window there. Otherwise
			//    fullscreen on the current output (no move).
			if ( targetDisplay )
			{
				SDL_SetWindowPosition( m_Window,
					SDL_WINDOWPOS_CENTERED_DISPLAY( targetDisplay ),
					SDL_WINDOWPOS_CENTERED_DISPLAY( targetDisplay ) );
				SDL_SyncWindow( m_Window );
			}

			if ( dList ) SDL_free( dList );

			SDL_SetWindowFullscreenMode( m_Window, NULL );
		}

		SDL_SetWindowFullscreen( m_Window, bFullScreen );
		SDL_SyncWindow( m_Window );

		// D. Wayland positioning is a hint, not a guarantee. If we targeted a
		//    specific display, verify the window actually landed there so a future
		//    silent-blur regression is diagnosable. No retry / no exclusive fallback.
		if ( bFullScreen && targetDisplay )
		{
			SDL_DisplayID actual = SDL_GetDisplayForWindow( m_Window );
			if ( actual != targetDisplay )
			{
				Warning( "SetWindowFullScreen: requested display 0x%x but window landed on 0x%x\n",
					(unsigned)targetDisplay, (unsigned)actual );
			}
		}

		m_bFullScreen = bFullScreen;
	}

	if ( !bFullScreen )
	{
		SizeWindow( nWidth, nHeight );
	}


	ApplyPointerState();
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

	ApplyPointerState();
}


// key input handler
// Events CInputSystem actually consumes. Anything else is handled locally (or
// ignored) and must not take a slot in the ring: SDL emits a lot of traffic we do
// not care about (window enter/leave/exposed, display changes, poll sentinels), and
// PostEvent() drops on overflow -- dropping a real KEY_UP would leave a key stuck
// down. This also keeps events carrying SDL-owned pointers (drop/clipboard) out of
// a queue that outlives them.
static bool IsForwardedToInputSystem( Uint32 nType )
{
	switch ( nType )
	{
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_TEXT_INPUT:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_QUIT:
			return true;
		default:
			return false;
	}
}

void CSDLMgr::PumpWindowsMessageLoop()
{
	SDLAPP_FUNC;

	SDL_Event event;
	int nEventsProcessed = 0;
	while ( SDL_PollEvent( &event ) && nEventsProcessed < 100 )
	{
		nEventsProcessed++;

		// This switch only maintains state that CSDLMgr itself owns (focus, window
		// size, cursor grab, and the relative-motion accumulator behind
		// GetMouseDelta). It deliberately does not interpret input: every event is
		// forwarded verbatim to CInputSystem, which is the single place that turns
		// SDL events into Source input events.
		bool bForward = true;

		switch ( event.type )
		{
			case SDL_EVENT_MOUSE_MOTION:
			{
				if ( !m_bHasFocus )
				{
					bForward = false;
					break;
				}

				// SDL_WarpMouseInWindow generates a motion event when the cursor is
				// visible (in relative mode SDL_HINT_MOUSE_RELATIVE_WARP_MOTION
				// defaults off). Swallow the one we asked for, and disarm either way:
				// on Wayland the warp is advisory and may land elsewhere, which would
				// otherwise leave this armed and eat a later real motion event.
				if ( m_bExpectSyntheticMouseMotion )
				{
					m_bExpectSyntheticMouseMotion = false;

					if ( (int)event.motion.x == m_nMouseTargetX &&
						 (int)event.motion.y == m_nMouseTargetY )
					{
						bForward = false;
						break;
					}
				}

				// Keep sub-pixel precision. Wayland's relative pointer protocol
				// delivers wl_fixed (1/256 px) deltas, so truncating each event to an
				// int silently discarded all slow movement: with a high polling rate
				// mouse, |xrel| < 1.0 per event floored to zero every time.
				m_flMouseXDelta += event.motion.xrel;
				m_flMouseYDelta += event.motion.yrel;
				break;
			}

			case SDL_EVENT_KEY_DOWN:
			{
				if ( event.key.repeat &&
					 ( event.key.key == SDLK_BACKSPACE || event.key.key == SDLK_DELETE ) )
				{
					// UI text fields want a keyup between auto-repeats rather than a
					// run of keydowns with no release.
					SDL_Event up = event;
					up.type = SDL_EVENT_KEY_UP;
					up.key.type = SDL_EVENT_KEY_UP;
					up.key.down = false;
					up.key.repeat = false;
					PostEvent( up );
				}
				break;
			}

			case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
			case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			{
				// The surface was rebuilt: entering/leaving fullscreen or changing
				// resolution does not reliably preserve the Wayland pointer
				// constraint, and SDL's own warp emulation toggles relative mode on
				// the way through. Focus changes do NOT need this -- SDL re-derives
				// the lock from the window flag itself -- which is exactly why
				// alt-tabbing recovered nothing while a mode change broke aiming.
				ApplyPointerState();
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			{
				m_bHasFocus = true;

				// SDL3: text input is off by default and may be reset by
				// compositor changes. Re-enable on every focus gain.
				SDL_StartTextInput( m_Window );
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_LOST:
			{
				m_bHasFocus = false;

				// Deliberately NOT clearing relative mode or the grab here. Those are
				// latched window flags that SDL already scopes to focus -- it stops
				// constraining and reporting motion on its own, and re-establishes the
				// lock when focus returns. Clearing them threw away the only record of
				// our intent, so mouselook never came back.

				// SDL_keyboard.modstate drifts out of alignment otherwise.
				SDL_SetModState( SDL_KMOD_NONE );
				break;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
				// LOGICAL size: the space SDL reports mouse coordinates in, so latch
				// it for WindowToEngineCoords() and keep it to ourselves. The engine
				// works in pixels and hears about those from
				// SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED instead; forwarding both would
				// have it compare logical against pixel and bounce SetMode() back at
				// us under fractional display scaling.
				if ( event.window.data1 > 0 && event.window.data2 > 0 )
				{
					// Don't update sdl_displayindex here -- automatic updates lock the
					// convar to whichever monitor the compositor picked, preventing the
					// user from moving the window. It should only change via explicit
					// user action.
					m_WindowWidth = event.window.data1;
					m_WindowHeight = event.window.data2;
				}
				break;
			}

			default:
				break;
		}

		if ( bForward && IsForwardedToInputSystem( event.type ) )
			PostEvent( event );
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
		SetGameSDLWindow( NULL );
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

void CSDLMgr::GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta )
{
	SDLAPP_FUNC;

	// Raw relative motion, exactly as SDL reported it. No screen-space scaling:
	// these are pointer deltas, not positions, so a resolution ratio never applied.
	x = m_flMouseXDelta;
	y = m_flMouseYDelta;

	m_flMouseXDelta = m_flMouseYDelta = 0.0f;
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
