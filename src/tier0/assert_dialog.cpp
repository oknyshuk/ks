//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"

#include "tier0/valve_off.h"
char *GetCommandLine();
#include "resource.h"
#include "tier0/valve_on.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"

#include <dlfcn.h>

// We lazily load the SDL shared object, and only reference functions if it's
// available, so this can be included on the dedicated server too.
#include <SDL3/SDL.h>

typedef bool ( SDLCALL FUNC_SDL_ShowMessageBox )( const SDL_MessageBoxData *messageboxdata, int *buttonid );
typedef SDL_Window* ( SDLCALL FUNC_SDL_GetKeyboardFocus )();

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CDialogInitInfo
{
public:
	const tchar *m_pFilename;
	int m_iLine;
	const tchar *m_pExpression;
};


class CAssertDisable
{
public:
	tchar m_Filename[512];
	
	// If these are not -1, then this CAssertDisable only disables asserts on lines between
	// these values (inclusive).
	int m_LineMin;		
	int m_LineMax;
	
	// Decremented each time we hit this assert and ignore it, until it's 0. 
	// Then the CAssertDisable is removed.
	// If this is -1, then we always ignore this assert.
	int m_nIgnoreTimes;	

	CAssertDisable *m_pNext;
};


static bool g_bAssertsEnabled = true;
static bool g_bAssertDialogEnabled = true;

static CAssertDisable *g_pAssertDisables = NULL;


// Set to true if they want to break in the debugger.
static bool g_bBreak = false;

static CDialogInitInfo g_Info CONSTRUCT_EARLY;

static bool g_bDisableAsserts = false;


// -------------------------------------------------------------------------------- //
// Internal functions.
// -------------------------------------------------------------------------------- //


static bool IsDebugBreakEnabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-debugbreak") ) != NULL );
	return bResult;
}

static bool AssertStack()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-assertstack") ) != NULL );
	return bResult;
}

static bool AreAssertsDisabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-noassert") ) != NULL );
	return bResult || g_bDisableAsserts;
}

static bool AllAssertOnce()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-assertonce") ) != NULL );
	return bResult;
}

static bool AreAssertsEnabledInFileLine( const tchar *pFilename, int iLine )
{
	CAssertDisable **pPrev = &g_pAssertDisables;
	CAssertDisable *pNext;
	for ( CAssertDisable *pCur=g_pAssertDisables; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;

		if ( _tcsicmp( pFilename, pCur->m_Filename ) == 0 )
		{
			// Are asserts disabled in the whole file?
			bool bAssertsEnabled = true;
			if ( pCur->m_LineMin == -1 && pCur->m_LineMax == -1 )
				bAssertsEnabled = false;
			
			// Are asserts disabled on the specified line?
			if ( iLine >= pCur->m_LineMin && iLine <= pCur->m_LineMax )
				bAssertsEnabled = false;

			if ( !bAssertsEnabled )
			{
				// If this assert is only disabled for the next N times, then countdown..
				if ( pCur->m_nIgnoreTimes > 0 )
				{
					--pCur->m_nIgnoreTimes;
					if ( pCur->m_nIgnoreTimes == 0 )
					{
						// Remove this one from the list.
						*pPrev = pNext;
						delete pCur;
						continue;
					}
				}
				
				return false;
			}
		}

		pPrev = &pCur->m_pNext;
	}

	return true;
}


CAssertDisable* CreateNewAssertDisable( const tchar *pFilename )
{
	CAssertDisable *pDisable = new CAssertDisable;
	pDisable->m_pNext = g_pAssertDisables;
	g_pAssertDisables = pDisable;

	pDisable->m_LineMin = pDisable->m_LineMax = -1;
	pDisable->m_nIgnoreTimes = -1;
	
	_tcsncpy( pDisable->m_Filename, g_Info.m_pFilename, sizeof( pDisable->m_Filename ) - 1 );
	pDisable->m_Filename[ sizeof( pDisable->m_Filename ) - 1 ] = 0;
	
	return pDisable;
}


void IgnoreAssertsInCurrentFile()
{
	CreateNewAssertDisable( g_Info.m_pFilename );
}


CAssertDisable* IgnoreAssertsNearby( int nRange )
{
	CAssertDisable *pDisable = CreateNewAssertDisable( g_Info.m_pFilename );
	pDisable->m_LineMin = g_Info.m_iLine - nRange;
	pDisable->m_LineMax = g_Info.m_iLine - nRange;
	return pDisable;
}



// -------------------------------------------------------------------------------- //
// Interface functions.
// -------------------------------------------------------------------------------- //

// provides access to the global that turns asserts on and off
PLATFORM_INTERFACE bool AreAllAssertsDisabled()
{
	return !g_bAssertsEnabled;
}

PLATFORM_INTERFACE void SetAllAssertsDisabled( bool bAssertsDisabled )
{
	g_bAssertsEnabled = !bAssertsDisabled;
}


// provides access to the global that turns asserts on and off
PLATFORM_INTERFACE bool IsAssertDialogDisabled()
{
	return !g_bAssertDialogEnabled;
}

PLATFORM_INTERFACE void SetAssertDialogDisabled( bool bAssertDialogDisabled )
{
	g_bAssertDialogEnabled = !bAssertDialogDisabled;
}

SDL_Window *g_SDLWindow = NULL;

PLATFORM_INTERFACE void SetAssertDialogParent( struct SDL_Window *window )
{
	g_SDLWindow = window;
}

PLATFORM_INTERFACE struct SDL_Window * GetAssertDialogParent()
{
	return g_SDLWindow;
}

PLATFORM_INTERFACE bool ShouldUseNewAssertDialog()
{
	static bool bMPIWorker = ( _tcsstr( Plat_GetCommandLine(), _T("-mpi_worker") ) != NULL );
	if ( bMPIWorker )
	{
		return false;
	}

#ifdef DBGFLAG_ASSERTDLG
	return true;		// always show an assert dialog
#else
	return Plat_IsInDebugSession();		// only show an assert dialog if the process is being debugged
#endif // DBGFLAG_ASSERTDLG
}


PLATFORM_INTERFACE bool DoNewAssertDialog( const tchar *pFilename, int line, const tchar *pExpression )
{
	LOCAL_THREAD_LOCK();

	if ( AreAssertsDisabled() )
		return false;

	// If they have the old mode enabled (always break immediately), then just break right into
	// the debugger like we used to do.
	if ( IsDebugBreakEnabled() )
		return true;

	// Have ALL Asserts been disabled?
	if ( !g_bAssertsEnabled )
		return false;

	// Has this specific Assert been disabled?
	if ( !AreAssertsEnabledInFileLine( pFilename, line ) )
		return false;

	// Now create the dialog.
	g_Info.m_pFilename = pFilename;
	g_Info.m_iLine = line;
	g_Info.m_pExpression = pExpression;

	if ( AssertStack() )
	{
		IgnoreAssertsNearby( 0 );
		// @TODO: add-back callstack spew support
		Warning( "%s (%d) : Assertion callstack...(NOT IMPLEMENTED IN NEW LOGGING SYSTEM.)\n", pFilename, line );
		// Warning_SpewCallStack( 10, "%s (%d) : Assertion callstack...\n", pFilename, line );
		return false;
	}

	if( AllAssertOnce() )
	{
		IgnoreAssertsNearby( 0 );
	}

	g_bBreak = false;


	#define COLOR_YELLOW 	"\033[1;33m"
	#define COLOR_GREEN 	"\033[1;32m"
	#define COLOR_RED 		"\033[1;31m"
	#define COLOR_END		"\033[0m"
	fprintf(stderr, COLOR_YELLOW "ASSERT: " COLOR_RED "%s" COLOR_GREEN ":%i:" COLOR_RED "%s" COLOR_END "\n", pFilename, line, pExpression);
	

	static FUNC_SDL_ShowMessageBox *pfnSDLShowMessageBox = NULL;
    static FUNC_SDL_GetKeyboardFocus *pfnSDLGetKeyboardFocus = NULL;
	if( getenv( "GAME_ASSERT_DIALOG" ) && !pfnSDLShowMessageBox )
	{

        void *ret = dlopen( "libSDL3.so.0", RTLD_LAZY );

        pfnSDLShowMessageBox = ( FUNC_SDL_ShowMessageBox * )dlsym( ret, "SDL_ShowMessageBox" );
        pfnSDLGetKeyboardFocus = ( FUNC_SDL_GetKeyboardFocus * )dlsym( ret, "SDL_GetKeyboardFocus" );
    }

	if( pfnSDLShowMessageBox )
	{
		int buttonid;
		char text[ 4096 ];
		SDL_MessageBoxData messageboxdata = { 0 };
		const char *DefaultAction = Plat_IsInDebugSession() ? "Break" : "Corefile";
		SDL_MessageBoxButtonData buttondata[] =
		{
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,	IDC_BREAK,			DefaultAction			},
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,	IDC_IGNORE_THIS,	"Ignore"				},
			{ 0,										IDC_IGNORE_FILE,	"Ignore This File"		},
			{ 0,										IDC_IGNORE_ALWAYS,	"Always Ignore"			},
			{ 0,										IDC_IGNORE_ALL,		"Ignore All Asserts"	},
		};

		_snprintf( text, sizeof( text ), "File: %s\nLine: %i\nExpr: %s\n", pFilename, line, pExpression );
		text[ sizeof( text ) - 1 ] = 0;

		messageboxdata.window = g_SDLWindow;
		messageboxdata.title = "Assertion Failed";
		messageboxdata.message = text;
		messageboxdata.numbuttons = ARRAYSIZE( buttondata );
		messageboxdata.buttons = buttondata;

		bool Ret = ( *pfnSDLShowMessageBox )( &messageboxdata, &buttonid );
		if( !Ret )
		{
			buttonid = IDC_BREAK;
		}

		switch( buttonid )
		{
		default:
		case IDC_BREAK:
			// Break on this Assert
			g_bBreak = true;
			break;
		case IDC_IGNORE_THIS:
			// Ignore this Assert once
			break;
        case IDC_IGNORE_FILE:
			IgnoreAssertsInCurrentFile();
			break;
		case IDC_IGNORE_ALWAYS:
			// Ignore this Assert from now on
			IgnoreAssertsNearby( 0 );
			break;
		case IDC_IGNORE_ALL:
			// Ignore all Asserts from now on
			g_bAssertsEnabled = false;
			break;
		}
	}
	else if ( getenv( "RAISE_ON_ASSERT" ) )
	{
		g_bBreak = true;
	}


	return g_bBreak;
}

