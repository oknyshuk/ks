// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk


#include <cstdio>
import std;
import ks.core.cmdline;
import ks.core.logging;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.strtools;
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "idedicatedexports.h"
#include "appframework/AppFramework.h"
#include "filesystem_init.h"
#include "tier2/tier2.h"
#include "dedicated.h"
#include "istudiorender.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "materialsystem/imaterialsystem.h"

bool InitInstance( );
void Cvar_InstallEngineHooks( ICvar *pCVar );
void Cvar_RemoveEngineHooks();
void ProcessConsoleInput( void );
const char *UTIL_GetExecutableDir( );
const char *UTIL_GetBaseDir( void );

const char *g_gameName = "csgo";

#include "console/TextConsoleUnix.h"
CTextConsoleUnix console;

IDedicatedServerAPI *engine = nullptr;

int g_nSubProcessId = 0;

extern char g_szEXEName[ 256 ];

class CDedicatedServerLoggingListener : public ILoggingListener
{
public:
	CDedicatedServerLoggingListener() : ILoggingListener( [this]( const LoggingContext_t *pContext, const tchar *pMessage ) { Log( pContext, pMessage ); } ) {}

	void Log( const LoggingContext_t *pContext, const tchar *pMessage )
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
			std::fflush(stdout);
			Plat_ExitProcess(1);
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
		len += std::strlen(argv[i]);
	}
	
	if ( len > MAX_LINUX_CMDLINE )
	{
		std::printf( "command line too long, %i max\n", MAX_LINUX_CMDLINE );
		std::exit(-1);
		return;
	}
	
	linuxCmdline[0] = '\0';
	for ( i = 0; i < argc; i++ )
	{
		if ( i > 0 )
		{
			std::strcat( linuxCmdline, " " );
		}
		std::strcat( linuxCmdline, argv[ i ] );
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
	void *pSystem = AddSystem( dedicatedModule, VENGINE_DEDICATEDEXPORTS_API_VERSION );
	if ( !pSystem )
		return false;

	// Install the engine's ConVar linkage rules before any other system is added
	Cvar_InstallEngineHooks( static_cast<ICvar *>( FindSystem( CVAR_INTERFACE_VERSION ) ) );

	static const char * const pBuiltinSystems[] =
	{
		MATERIAL_SYSTEM_INTERFACE_VERSION,
		STUDIO_RENDER_INTERFACE_VERSION,
		VPHYSICS_INTERFACE_VERSION,
		DATACACHE_INTERFACE_VERSION,
		MDLCACHE_INTERFACE_VERSION,
		STUDIO_DATA_CACHE_INTERFACE_VERSION,
		SOUNDEMITTERSYSTEM_INTERFACE_VERSION,
		VSCRIPT_INTERFACE_VERSION,
		VENGINE_HLDS_API_VERSION,
	};

	for ( const char *pInterfaceName : pBuiltinSystems )
	{
		if ( !AddSystem( dedicatedModule, pInterfaceName ) )
			return false;
	}

	return sys->LoadModules( this );
}

bool CDedicatedAppSystemGroup::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	CFSSetupInfo setupInfo;
	setupInfo.m_pDirectoryName = nullptr;
	setupInfo.m_bOnlyUseDirectoryName = false;
	if ( FileSystem_SetupEnvironment( setupInfo ) != FS_OK )
		return false;

	if ( FileSystem_SetBasePaths( g_pFullFileSystem ) != FS_OK )
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
	Cvar_RemoveEngineHooks();
	sys->DestroyConsoleWindow();
	console.ShutDown();
	BaseClass::PostShutdown();
}

void CDedicatedAppSystemGroup::Destroy() 
{
	LoggingSystem_PopLoggingState();
}


//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
static bool GetExecutableName( char *out, int nMaxLen )
{
	V_strncpy( out, g_szEXEName, nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir( char *pBaseDir, int nMaxLen )
{
	int j;
	char *pBuffer = nullptr;

	pBaseDir[ 0 ] = 0;

	if ( GetExecutableName( pBaseDir, nMaxLen ) )
	{
		pBuffer = std::strrchr( pBaseDir, CORRECT_PATH_SEPARATOR );
		if ( pBuffer && *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		j = std::strlen( pBaseDir );
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
		std::strcpy( pBaseDir, pOverrideDir );
	}

	V_strlower( pBaseDir );
	V_FixSlashes( pBaseDir );
}


static bool s_GameInfoSuggestFN( CFSSetupInfo const *pFsSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	V_strncpy( pchPathBuffer, g_gameName, nBufferLength );
	return true;
}




//-----------------------------------------------------------------------------
//
// Main entry point for dedicated server, shared between win32 and linux
//
//-----------------------------------------------------------------------------

int DedicatedServerMain(int argc, char **argv)
{
	SetupFPUControlWord();

	std::strcpy(g_szEXEName, *argv);
	// Store off command line for argument searching
	BuildCmdLine(argc, argv);

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Store off command line for argument searching
	CommandLine()->CreateCmdLine( GetCommandLine() );
	Plat_SetCommandLine( CommandLine()->GetCmdLine() );

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
	CFSApplication application( &dedicatedSystems );
	int nRet = application.Run( );

	return nRet;
}
