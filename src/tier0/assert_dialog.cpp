// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import ks.core.cmdline;
import ks.core.dbg;
import ks.core.platform;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import std;

char *GetCommandLine();
inline constexpr auto IDD_ASSERT_DIALOG = 101;
inline constexpr auto IDC_FILENAME_CONTROL = 1000;
inline constexpr auto IDC_LINE_CONTROL = 1001;
inline constexpr auto IDC_IGNORE_FILE = 1002;
inline constexpr auto IDC_IGNORE_NEARBY = 1003;
inline constexpr auto IDC_IGNORE_NUMLINES = 1004;
inline constexpr auto IDC_IGNORE_THIS = 1005;
inline constexpr auto IDC_BREAK = 1006;
inline constexpr auto IDC_IGNORE_ALL = 1008;
inline constexpr auto IDC_IGNORE_ALWAYS = 1009;
inline constexpr auto IDC_IGNORE_NUMTIMES = 1010;
inline constexpr auto IDC_ASSERT_MSG_CTRL = 1011;
inline constexpr auto IDC_NOID = -1;


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

static CAssertDisable *g_pAssertDisables = nullptr;


// Set to true if they want to break in the debugger.
static bool g_bBreak = false;

static CDialogInitInfo g_Info [[gnu::init_priority(101)]];

static bool g_bDisableAsserts = false;


// -------------------------------------------------------------------------------- //
// Internal functions.
// -------------------------------------------------------------------------------- //


static bool IsDebugBreakEnabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), "-debugbreak" ) != nullptr );
	return bResult;
}

static bool AssertStack()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), "-assertstack" ) != nullptr );
	return bResult;
}

static bool AreAssertsDisabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), "-noassert" ) != nullptr );
	return bResult || g_bDisableAsserts;
}

static bool AllAssertOnce()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), "-assertonce" ) != nullptr );
	return bResult;
}

static bool AreAssertsEnabledInFileLine( const tchar *pFilename, int iLine )
{
	CAssertDisable **pPrev = &g_pAssertDisables;
	CAssertDisable *pNext;
	for ( CAssertDisable *pCur=g_pAssertDisables; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;

		if ( V_tier0_stricmp( pFilename, pCur->m_Filename ) == 0 )
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
	
	std::strncpy( pDisable->m_Filename, g_Info.m_pFilename, sizeof( pDisable->m_Filename ) - 1 );
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
bool AreAllAssertsDisabled()
{
	return !g_bAssertsEnabled;
}

void SetAllAssertsDisabled( bool bAssertsDisabled )
{
	g_bAssertsEnabled = !bAssertsDisabled;
}


// provides access to the global that turns asserts on and off
bool IsAssertDialogDisabled()
{
	return !g_bAssertDialogEnabled;
}

void SetAssertDialogDisabled( bool bAssertDialogDisabled )
{
	g_bAssertDialogEnabled = !bAssertDialogDisabled;
}

static AssertDialogFunc_t g_pfnAssertDialog = nullptr;

void SetAssertDialogHandler( AssertDialogFunc_t func )
{
	g_pfnAssertDialog = func;
}

bool ShouldUseNewAssertDialog()
{
	static bool bMPIWorker = ( _tcsstr( Plat_GetCommandLine(), "-mpi_worker" ) != nullptr );
	if ( bMPIWorker )
	{
		return false;
	}

	return true;		// always show an assert dialog
}


bool DoNewAssertDialog( const tchar *pFilename, int line, const tchar *pExpression )
{
	static std::recursive_mutex assertDialogMutex;
	std::scoped_lock lock( assertDialogMutex );

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


	std::fprintf( stderr, "\033[1;33mASSERT: \033[1;31m%s\033[1;32m:%i:\033[1;31m%s\033[0m\n", pFilename, line, pExpression );
	

	if( std::getenv( "GAME_ASSERT_DIALOG" ) && g_pfnAssertDialog )
	{
		char text[ 4096 ];
		const char *DefaultAction = Plat_IsInDebugSession() ? "Break" : "Corefile";
		static const int buttonids[] = { IDC_BREAK, IDC_IGNORE_THIS, IDC_IGNORE_FILE, IDC_IGNORE_ALWAYS, IDC_IGNORE_ALL };
		const char *labels[] = { DefaultAction, "Ignore", "Ignore This File", "Always Ignore", "Ignore All Asserts" };

		std::snprintf( text, sizeof( text ), "File: %s\nLine: %i\nExpr: %s\n", pFilename, line, pExpression );
		text[ sizeof( text ) - 1 ] = 0;

		const int chosen = g_pfnAssertDialog( "Assertion Failed", text, labels, std::size( labels ), 0, 1 );
		const int buttonid = ( chosen >= 0 && chosen < (int)std::size( buttonids ) ) ? buttonids[chosen] : IDC_BREAK;

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
	else if ( std::getenv( "RAISE_ON_ASSERT" ) )
	{
		g_bBreak = true;
	}


	return g_bBreak;
}
