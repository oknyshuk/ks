//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include <stdio.h>
#include <stdlib.h>
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "checksum_md5.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "idedicatedexports.h"
#include "appframework/AppFramework.h"
#include "filesystem_init.h"
#include "tier2/tier2.h"
#include "dedicated.h"
#include "vstdlib/cvar.h"
#include <mcheck.h>
#include <unistd.h>
#define _chdir chdir

bool InitInstance( );
void ProcessConsoleInput( void );
const char *UTIL_GetExecutableDir( );
bool NET_Init( void );
void NET_Shutdown( void );
const char *UTIL_GetBaseDir( void );

const char *g_gameName = "csgo";

#include "console/TextConsoleUnix.h"
CTextConsoleUnix console;

IDedicatedServerAPI *engine = NULL;

int g_nSubProcessId = 0;

extern char g_szEXEName[ 256 ];

class CDedicatedServerLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( sys )
		{
			if ( g_nSubProcessId )
			{
				sys->Printf( " #%0x2d:%s", g_nSubProcessId, pMessage );
			}
			else
			{
				sys->Printf( "#%s", pMessage );
			}
		}

		if ( pContext->m_Severity == LS_ERROR )
		{
			fflush(stdout);
			_exit(1);
		}
	}
};


#define MAX_LINUX_CMDLINE 2048
static char linuxCmdline[ MAX_LINUX_CMDLINE + 1 ];

void BuildCmdLine( int argc, char **argv )
{
	int len;
	int i;
	
	for (len = 0, i = 0; i < argc; i++)
	{
		len += strlen(argv[i]);
	}
	
	if ( len > MAX_LINUX_CMDLINE )
	{
		printf( "command line too long, %i max\n", MAX_LINUX_CMDLINE );
		exit(-1);
		return;
	}
	
	linuxCmdline[0] = '\0';
	for ( i = 0; i < argc; i++ )
	{
		if ( i > 0 )
		{
			strcat( linuxCmdline, " " );
		}
		strcat( linuxCmdline, argv[ i ] );
	}
}

char *GetCommandLine()
{
	return linuxCmdline;
}

static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;
static CDedicatedServerLoggingListener s_DedicatedServerLoggingListener;

bool RunServerIteration( bool bSupressStdIOBecauseWeAreAForkedChild )
{
	bool bDone = false;

	if (! bSupressStdIOBecauseWeAreAForkedChild )
	{
		// Calling ProcessConsoleInput can cost about a tenth of a millisecond.
		// We used to call it up to 1,000 times a second. Even calling it once
		// a frame is wasteful since the console hardly needs that level of
		// responsiveness, and calling it too frequently is a waste of CPU time
		// and power.
		static int s_nProcessCount;
		// Don't set this too high since the users keystrokes are not reflected
		// until this ProcessConsoleInput is called.
		const int nConsoleInputFrames = 5;
		++s_nProcessCount;
		if ( s_nProcessCount > nConsoleInputFrames )
		{
			s_nProcessCount = 0;
			ProcessConsoleInput();
		}
	}

	if ( !engine->RunFrame() )
	{
		bDone = true;
	}

	sys->UpdateStatus( 0  /* don't force */ );

	return bDone;
}

//-----------------------------------------------------------------------------
//
//  Server loop
//
//-----------------------------------------------------------------------------
void RunServer( bool bSupressStdIOBecauseWeAreAForkedChild )
{
	// run 2 engine frames first to get the engine to load its resources
	if ( !engine->RunFrame() )
	{
		return;
	}

	if ( !engine->RunFrame() )
	{
		return;
	}

	bool bDone = false;
	while ( ! bDone )
	{
		bDone = RunServerIteration( bSupressStdIOBecauseWeAreAForkedChild );
	}
}

//-----------------------------------------------------------------------------
//
// initialize the console
//
//-----------------------------------------------------------------------------
bool ConsoleStartup( CreateInterfaceFn dedicatedFactory )
{
	return console.Init();
}


//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CDedicatedAppSystemGroup::Create( )
{
	// Hook the debug output stuff (override the spew func in the appframework)
	LoggingSystem_PushLoggingState();
	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	LoggingSystem_RegisterLoggingListener( &s_DedicatedServerLoggingListener );

	// Added the dedicated exports module for the engine to grab
	AppModule_t dedicatedModule = LoadModule( Sys_GetFactoryThis() );
	IAppSystem *pSystem = AddSystem( dedicatedModule, VENGINE_DEDICATEDEXPORTS_API_VERSION );
	if ( !pSystem )
		return false;

	return sys->LoadModules( this );
}

bool CDedicatedAppSystemGroup::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = false;
	steamInfo.m_bSetSteamDLLPath = false;
	steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
	steamInfo.m_bNoGameInfo = steamInfo.m_bSteam;
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFullFileSystem;
	fsInfo.m_bToolsMode = false;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	if ( !NET_Init() )
		return false;

	// Needs to be done prior to init material system config
	CFSSearchPathsInit initInfo;

	initInfo.m_pFileSystem = g_pFullFileSystem;
	initInfo.m_pDirectoryName = CommandLine()->ParmValue( "-game", g_gameName );

	// Load gameinfo.txt and setup all the search paths, just like the tools do.
	FileSystem_LoadSearchPaths( initInfo );

	if ( !sys->CreateConsoleWindow() )
		return false;

	return true;
}

int CDedicatedAppSystemGroup::Main( )
{
	if ( !ConsoleStartup( GetFactory() ) )
		return -1;

	// Set up mod information
	ModInfo_t info;
	info.m_pInstance = GetAppInstance();
	info.m_pBaseDirectory = UTIL_GetBaseDir();
	info.m_pInitialMod = CommandLine()->ParmValue( "-game", g_gameName );
	info.m_pInitialGame = CommandLine()->ParmValue( "-defaultgamedir", g_gameName );
	info.m_pParentAppSystemGroup = this;
	info.m_bTextMode = CommandLine()->CheckParm( "-textmode" ) ? true : false;

	if ( engine->ModInit( info ) )
	{
		engine->ModShutdown();
	} // if engine->ModInit

	return 0;
}

void CDedicatedAppSystemGroup::PostShutdown()
{
	sys->DestroyConsoleWindow();
	console.ShutDown();
	NET_Shutdown();
	BaseClass::PostShutdown();
}

void CDedicatedAppSystemGroup::Destroy() 
{
	LoggingSystem_PopLoggingState();
}


//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
bool GetExecutableName( char *out, int nMaxLen )
{
	Q_strncpy( out, g_szEXEName, nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir( char *pBaseDir, int nMaxLen )
{
	int j;
	char *pBuffer = NULL;

	pBaseDir[ 0 ] = 0;

	if ( GetExecutableName( pBaseDir, nMaxLen ) )
	{
		pBuffer = strrchr( pBaseDir, CORRECT_PATH_SEPARATOR );
		if ( pBuffer && *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		j = strlen( pBaseDir );
		if (j > 0)
		{
			if ( ( pBaseDir[ j-1 ] == '\\' ) || 
				 ( pBaseDir[ j-1 ] == '/' ) )
			{
				pBaseDir[ j-1 ] = 0;
			}
		}
	}

	char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
	if ( pOverrideDir )
	{
		strcpy( pBaseDir, pOverrideDir );
	}

	Q_strlower( pBaseDir );
	Q_FixSlashes( pBaseDir );
}


static bool s_GameInfoSuggestFN( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	V_strncpy( pchPathBuffer, g_gameName, nBufferLength );
	return true;
}




//-----------------------------------------------------------------------------
//
// Main entry point for dedicated server, shared between win32 and linux
//
//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
	SetupFPUControlWord();

	strcpy(g_szEXEName, *argv);
	// Store off command line for argument searching
	BuildCmdLine(argc, argv);

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Store off command line for argument searching
	CommandLine()->CreateCmdLine( GetCommandLine() );
	Plat_SetCommandLine( CommandLine()->GetCmdLine() );

	if ( CommandLine()->CheckParm( "-mtrace" ) )
	{
		mtrace();
	}

	// Figure out the directory the executable is running from
	// and make that be the current working directory
	char pBasedir[ MAX_PATH ];
	UTIL_ComputeBaseDir( pBasedir, MAX_PATH );
	_chdir( pBasedir );

	// Rehook the command line.
	CommandLine()->CreateCmdLine( GetCommandLine() );

	if ( !InitInstance() )
		return -1;

	SetSuggestGameInfoDirFn( s_GameInfoSuggestFN );
	CDedicatedAppSystemGroup dedicatedSystems;
	CSteamApplication steamApplication( &dedicatedSystems );
	int nRet = steamApplication.Run( );

	if ( CommandLine()->CheckParm( "-mtrace" ) )
	{
		muntrace();
	}

	return nRet;
}
