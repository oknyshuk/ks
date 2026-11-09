// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Defines the entry point for the application.


#include <locale.h>

constexpr auto O_EXLOCK = 0;
import std;
import ks.desktop;

import ks.vstdlib;
#include "appframework/ilaunchermgr.h"
#include "engine_launcher_api.h"
#include "appframework/AppFramework.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem_init.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "reslistgenerator.h"
#include "vscript/ivscript.h"
#include "networksystem/inetworksystem.h"
#include "rocketui/rocketui.h"


constexpr auto MB_OK = 0x00000001;
constexpr auto MB_SYSTEMMODAL = 0x00000002;
constexpr auto MB_ICONERROR = 0x00000004;
int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType );


inline constexpr const char *DEFAULT_HL2_GAMEDIR = "csgo";


// A logging channel used during engine initialization
LoggingChannelID_t LOG_EngineInitialization = LoggingSystem_RegisterLoggingChannel( "EngineInitialization", nullptr );;
extern void* CreateSDLMgr();
void Cvar_InstallEngineHooks( ICvar *pCVar );
void Cvar_RemoveEngineHooks();


extern bool DoesFileExistIn( const char *pDirectoryName, const char *pFilename );

//-----------------------------------------------------------------------------
// Modules...
//-----------------------------------------------------------------------------
static IEngineAPI *g_pEngineAPI;

static bool g_bTextMode = false;

static char g_szBasedir[MAX_PATH];
static char g_szGamedir[MAX_PATH];

// copied from sys.h
struct FileAssociationInfo
{
	char const  *extension;
	char const  *command_to_issue;
};

static FileAssociationInfo g_FileAssociations[] =
{
	{ ".dem", "playdemo" },
	{ ".sav", "load" },
	{ ".bsp", "map" },
};


class CLeakDump
{
public:
	CLeakDump()
	 :	m_bCheckLeaks( false )
	{
	}

	~CLeakDump()
	{
		if ( m_bCheckLeaks )
		{
			g_pMemAlloc->DumpStats();
		}
	}

	bool m_bCheckLeaks;
} g_LeakDump;

class CLauncherLoggingListener : public ILoggingListener
{
public:
	CLauncherLoggingListener() : ILoggingListener( [this]( const LoggingContext_t *pContext, const tchar *pMessage ) { Log( pContext, pMessage ); } ) {}

	void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( pContext->m_Severity == LS_WARNING && pContext->m_ChannelID == LOG_EngineInitialization )
		{
			::MessageBox( nullptr, pMessage, "Warning!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
		else if ( pContext->m_Severity == LS_ASSERT && !ShouldUseNewAssertDialog() )
		{
			::MessageBox( nullptr, pMessage, "Assert!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
		else if ( pContext->m_Severity == LS_ERROR )
		{
			::MessageBox( nullptr, pMessage, "Error!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: Return the game directory
// Output : char
//-----------------------------------------------------------------------------
char *GetGameDirectory( void )
{
	return g_szGamedir;
}

void SetGameDirectory( const char *game )
{
	V_strncpy( g_szGamedir, game, sizeof(g_szGamedir) );
}

//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
static bool GetExecutableName( char *out, int outSize )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return the executable name
// Output : char
//-----------------------------------------------------------------------------
const char * GetExecutableFilename()
{
	char exepath[MAX_PATH];
	static char filename[MAX_PATH];

	filename[0] = 0;
	return filename;
}

//-----------------------------------------------------------------------------
// Purpose: Return the base directory
// Output : char
//-----------------------------------------------------------------------------
char *GetBaseDirectory( void )
{
	return g_szBasedir;
}

//-----------------------------------------------------------------------------
// Purpose: Determine the directory where this .exe is running from
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir()
{
	g_szBasedir[0] = 0;


	if ( !g_szBasedir[0] && GetExecutableName( g_szBasedir, sizeof( g_szBasedir ) ) )
	{
		char *pBuffer = std::strrchr( g_szBasedir, '\\' );
		if ( *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		int j = std::strlen( g_szBasedir );
		if (j > 0)
		{
			if ( ( g_szBasedir[j-1] == '\\' ) ||
				 ( g_szBasedir[j-1] == '/' ) )
			{
				g_szBasedir[j-1] = 0;
			}
		}
	}

	char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
	if ( pOverrideDir )
	{
		std::strcpy( g_szBasedir, pOverrideDir );
	}

	V_FixSlashes( g_szBasedir );

}


void InitTextMode()
{
}

void SortResList( char const *pchFileName, char const *pchSearchPath );

inline constexpr const char *ALL_RESLIST_FILE = "all.lst";
#define ENGINE_RESLIST_FILE  "engine.lst"


// create file to dump out to
class CLogAllFiles
{
public:
	CLogAllFiles();
	void Init();
	void Shutdown();
	void LogFile( const char *fullPathFileName, const char *options );

private:
	static void LogAllFilesFunc( const char *fullPathFileName, const char *options );
	void LogToAllReslist( char const *line );

	bool		m_bActive;
	char		m_szCurrentDir[k_cMaxPath];

	// persistent across restarts
	std::set< std::string, ks::iless > m_Logged;
	std::string	m_sResListDir;
	std::string	m_sFullGamePath;
};

static CLogAllFiles g_LogFiles;

CLogAllFiles::CLogAllFiles() :
	m_bActive( false )
{
	m_sResListDir = "reslists";
}

void CLogAllFiles::Init()
{

	// Can't do this in edit mode
	if ( CommandLine()->CheckParm( "-edit" ) )
	{
		return;
	}

	if ( !CommandLine()->CheckParm( "-makereslists" ) )
	{
		return;
	}

	m_bActive = true;

	char const *pszDir = nullptr;
	if ( CommandLine()->CheckParm( "-reslistdir", &pszDir ) && pszDir )
	{
		char szDir[ MAX_PATH ];
		V_strncpy( szDir, pszDir, sizeof( szDir ) );
		V_StripTrailingSlash( szDir );
		V_FixSlashes( szDir );
		if ( V_strlen( szDir ) > 0 )
		{
			m_sResListDir = szDir;
		}
	}

	// game directory has not been established yet, must derive ourselves
	char path[MAX_PATH];
	V_snprintf( path, sizeof(path), "%s/%s", GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );
	V_FixSlashes( path );
	m_sFullGamePath = path;

	// create file to dump out to
	char szDir[ MAX_PATH ];
	V_snprintf( szDir, sizeof( szDir ), "%s\\%s", m_sFullGamePath.c_str(), m_sResListDir.c_str() );
	g_pFullFileSystem->CreateDirHierarchy( szDir, "GAME" );

	g_pFullFileSystem->AddLoggingFunc( &LogAllFilesFunc );

	if ( !CommandLine()->FindParm( "-startmap" ) && !CommandLine()->FindParm( "-startstage" ) )
	{
		m_Logged.clear();
		g_pFullFileSystem->RemoveFile( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.c_str(), m_sResListDir.c_str(), ALL_RESLIST_FILE ), "GAME" );
	}

	ks::platform::current_directory( m_szCurrentDir, sizeof(m_szCurrentDir) );
	V_strncat( m_szCurrentDir, "/", sizeof(m_szCurrentDir), 1 );
}

void CLogAllFiles::Shutdown()
{
	if ( !m_bActive )
		return;

	m_bActive = false;

	if ( CommandLine()->CheckParm( "-makereslists" ) )
	{
		g_pFullFileSystem->RemoveLoggingFunc( &LogAllFilesFunc );
	}

	// Now load and sort all.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.c_str(), m_sResListDir.c_str(), ALL_RESLIST_FILE ), "GAME" );
	// Now load and sort engine.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.c_str(), m_sResListDir.c_str(), ENGINE_RESLIST_FILE ), "GAME" );

	m_Logged.clear();
}

void CLogAllFiles::LogToAllReslist( char const *line )
{
	// Open for append, write data, close.
	FileHandle_t fh = g_pFullFileSystem->Open( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.c_str(), m_sResListDir.c_str(), ALL_RESLIST_FILE ), "at", "GAME" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write("\"", 1, fh);
		g_pFullFileSystem->Write( line, V_strlen(line), fh );
		g_pFullFileSystem->Write("\"\n", 2, fh);
		g_pFullFileSystem->Close( fh );
	}
}

void CLogAllFiles::LogFile(const char *fullPathFileName, const char *options)
{
	if ( !m_bActive )
	{
		contract_assert( 0 );
		return;
	}

	// write out to log file
	contract_assert( fullPathFileName[1] == ':' );

	if ( !m_Logged.insert( ks::nz( fullPathFileName ) ).second )
	{
		return;
	}

	// make it relative to our root directory
	const char *relative = V_stristr( fullPathFileName, GetBaseDirectory() );
	if ( relative )
	{
		relative += ( V_strlen( GetBaseDirectory() ) + 1 );

		char rel[ MAX_PATH ];
		V_strncpy( rel, relative, sizeof( rel ) );
		V_FixSlashes( rel );

		LogToAllReslist( rel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CLogAllFiles::LogAllFilesFunc(const char *fullPathFileName, const char *options)
{
	g_LogFiles.LogFile( fullPathFileName, options );
}

//-----------------------------------------------------------------------------
// Purpose: This is a bit of a hack because it appears
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool IsWin98OrOlder()
{
	bool retval = false;


	return retval;
}


//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load the file system
//-----------------------------------------------------------------------------
class CSourceAppSystemGroup : public CFSAppSystemGroup
{
public:
	// Methods of IApplication
	bool Create();
	bool PreInit();
	int Main();
	void PostShutdown();
	void Destroy();

private:
	const char *DetermineDefaultMod();
	const char *DetermineDefaultGame();
};


//-----------------------------------------------------------------------------
// The dirty disk error report function
//-----------------------------------------------------------------------------
void ReportDirtyDiskNoMaterialSystem()
{
}

//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CSourceAppSystemGroup::Create()
{
	COM_TimestampedLog( "CSourceAppSystemGroup::Create()" );

	double start, elapsed;
	start = Plat_FloatTime();

	IFileSystem *pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	pFileSystem->InstallDirtyDiskReportFunc( ReportDirtyDiskNoMaterialSystem );


	// Install the engine's ConVar linkage rules before any other system is added
	Cvar_InstallEngineHooks( static_cast<ICvar *>( FindSystem( CVAR_INTERFACE_VERSION ) ) );

	static const char * const pBuiltinSystems[] =
	{
		INPUTSYSTEM_INTERFACE_VERSION,
		INPUTSTACKSYSTEM_INTERFACE_VERSION,
		LOCALIZE_INTERFACE_VERSION,
		VPHYSICS_INTERFACE_VERSION,
		MATERIAL_SYSTEM_INTERFACE_VERSION,
		DATACACHE_INTERFACE_VERSION,
		MDLCACHE_INTERFACE_VERSION,
		STUDIO_DATA_CACHE_INTERFACE_VERSION,
		STUDIO_RENDER_INTERFACE_VERSION,
		SOUNDEMITTERSYSTEM_INTERFACE_VERSION,
		VSCRIPT_INTERFACE_VERSION,
		VENGINE_LAUNCHER_API_VERSION,
		ROCKETUI_INTERFACE_VERSION,
	};

    AddSystem( (ILauncherMgr *)CreateSDLMgr(),	SDLMGR_INTERFACE_VERSION );

	SetAssertDialogHandler( []( const char *pszTitle, const char *pszText, const char *const *pLabels, int nLabels, int nDefault, int nEscape )
	{
		return ks::desktop::message_box( ks::desktop::game_window(), pszTitle, pszText, pLabels, nLabels, nDefault, nEscape );
	} );

	AppModule_t builtinModule = LoadModule( Sys_GetFactoryThis() );
	for ( const char *pInterfaceName : pBuiltinSystems )
	{
		if ( !AddSystem( builtinModule, pInterfaceName ) )
			return false;
	}

	if ( CommandLine()->FindParm( "-dev" ) )
	{
		// Used to guarantee precache consistency
		IResourceAccessControl *pResourceAccess = (IResourceAccessControl*)AddSystem( builtinModule, RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION );
		if ( !pResourceAccess )
			return false;
	}

	// Connect to iterfaces loaded in AddSystems that we need locally
	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
		return false;

	g_pEngineAPI = (IEngineAPI*)FindSystem( VENGINE_LAUNCHER_API_VERSION );

	// The shader API is linked into this executable; this only selects it by name.
	// This has to be done before connection.
	char const *pDLLName = "shaderapidx9.so";
	pMaterialSystem->SetShaderAPI( pDLLName );

	elapsed = Plat_FloatTime() - start;
	COM_TimestampedLog( "CSourceAppSystemGroup::Create() - Took %.4f secs to load libraries and get factories.", (float)elapsed );

	return true;
}

bool CSourceAppSystemGroup::PreInit()
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConVar_Register( );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pMaterialSystem )
		return false;

	CFSSetupInfo setupInfo;
	setupInfo.m_bOnlyUseDirectoryName = true;
	setupInfo.m_pDirectoryName = DetermineDefaultMod();
	if ( !setupInfo.m_pDirectoryName )
	{
		setupInfo.m_pDirectoryName = DetermineDefaultGame();
		if ( !setupInfo.m_pDirectoryName )
		{
			Error( "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game specified." );
		}
	}
	if ( FileSystem_SetupEnvironment( setupInfo ) != FS_OK )
		return false;

	if ( FileSystem_SetBasePaths( g_pFullFileSystem ) != FS_OK )
		return false;

	Msg( "start timing %f\n", Plat_FloatTime() );
	char const *pVPKName = CommandLine()->ParmValue( "-vpk" );
	if ( pVPKName )
	{
		g_pFullFileSystem->AddVPKFile( pVPKName );
	}

	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );

	// This will get called multiple times due to being here, but only the first one will do anything
	reslistgenerator->Init( GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );

	// This MUST get called each time, but will actually fix up the command line as needed
	reslistgenerator->TickAndFixupCommandLine();

	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Init();


	StartupInfo_t info;
	info.m_pInstance = GetAppInstance();
	info.m_pBaseDirectory = GetBaseDirectory();
	info.m_pInitialMod = DetermineDefaultMod();
	info.m_pInitialGame = DetermineDefaultGame();
	info.m_pParentAppSystemGroup = this;
	info.m_bTextMode = g_bTextMode;

	return g_pEngineAPI->SetStartupInfo( info );
}

int CSourceAppSystemGroup::Main()
{
	return g_pEngineAPI->Run();
}

void CSourceAppSystemGroup::PostShutdown()
{
	Cvar_RemoveEngineHooks();

	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Shutdown();

	reslistgenerator->Shutdown();

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister( );
	DisconnectTier1Libraries();
}

void CSourceAppSystemGroup::Destroy()
{
	g_pEngineAPI = nullptr;
	g_pMaterialSystem = nullptr;

}


//-----------------------------------------------------------------------------
// Determines the initial mod to use at load time.
//-----------------------------------------------------------------------------
const char *CSourceAppSystemGroup::DetermineDefaultMod()
{
	return CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
}

const char *CSourceAppSystemGroup::DetermineDefaultGame()
{
	return CommandLine()->ParmValue( "-defaultgamedir", DEFAULT_HL2_GAMEDIR );
}

//-----------------------------------------------------------------------------
// MessageBox for OSX
//-----------------------------------------------------------------------------

int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType )
{
	static const char *const labels[] = { "OK" };
	ks::desktop::message_box( ks::desktop::game_window(), header, message, labels, 1, 0, 0 );
	return 0;
}


//-----------------------------------------------------------------------------
// Allow only one windowed source app to run at a time
//-----------------------------------------------------------------------------
ks::platform::file *g_lockfd = nullptr;
char g_lockFilename[MAX_PATH];

bool GrabSourceMutex()
{
	// Under OSX use flock in /tmp/source_engine_<game>.lock, create the file if it doesn't exist
	const char *pchGameParam = CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
	CRC32_t gameCRC;
	CRC32_Init(&gameCRC);
	CRC32_ProcessBuffer( &gameCRC, (void *)pchGameParam, V_strlen( pchGameParam ) );
	CRC32_Final( &gameCRC );

	/*
	 * Linux
	 */

	// Check TMPDIR environment variable for temp directory.
	const char *tmpdir = std::getenv( "TMPDIR" );

	// If it's NULL, or it doesn't exist, or it isn't a directory, fallback to /tmp.
	if( !tmpdir || !ks::platform::path_is_directory( tmpdir ) )
		tmpdir = "/tmp";

	V_snprintf( g_lockFilename, sizeof(g_lockFilename), "%s/source_engine_%lu.lock", tmpdir, gameCRC );

	g_lockfd = ks::platform::open_file( g_lockFilename, ks::platform::file_mode::read_write, nullptr );
	if ( g_lockfd == nullptr )
	{
		std::printf( "open(%s) failed\n", g_lockFilename );
		return false;
	}

	// In case we have a umask setting creation to something other than 0666,
	// force it to 0666 so we don't lock other users out of the game if
	// the game dies etc.
	ks::platform::set_file_mode( g_lockfd, 0666 );

	if ( !ks::platform::lock_file( g_lockfd, true ) )
	{
		std::printf( "lock for %s failed\n", g_lockFilename );
		return false;
	}

	return true;

	return true;
}

void ReleaseSourceMutex()
{
	// Don't allocate if in multirun mode
	if( CommandLine()->FindParm( "-allowmultiple" ) || CommandLine()->FindParm( "-multirun" ) )
	{
		return;
	}

	if ( g_lockfd != nullptr )
	{
		ks::platform::close_file( g_lockfd );
		g_lockfd = nullptr;
		ks::platform::remove_file( g_lockFilename, nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *param -
// Output : static char const
//-----------------------------------------------------------------------------
static char const *Cmd_TranslateFileAssociation(char const *param )
{
	static char sz[ 512 ];
	char *retval = nullptr;

	char temp[ 512 ];
	V_strncpy( temp, param, sizeof( temp ) );
	V_FixSlashes( temp );
	const char *extension = V_GetFileExtension(temp);
	// must have an extension to map
	if (!extension)
		return retval;
	extension--; // back up so we have the . in the extension

	int c = std::size( g_FileAssociations );
	for ( int i = 0; i < c; i++ )
	{
		FileAssociationInfo& info = g_FileAssociations[ i ];

		if ( ! V_strcmp( extension, info.extension ) &&
			! CommandLine()->FindParm(CFmtStr( "+%s", info.command_to_issue ) ) )
		{
			// Translate if haven't already got one of these commands
			V_strncpy( sz, temp, sizeof( sz ) );
			V_FileBase( sz, temp, sizeof( sz ) );

			V_snprintf( sz, sizeof( sz ), "%s %s", info.command_to_issue, temp );
			retval = sz;
			break;
		}
	}

	// return null if no translation, otherwise return commands
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Converts all the convar args into a convar command
// Input  : none
// Output : const char * series of convars
//-----------------------------------------------------------------------------
static const char *BuildCommand()
{
	static ks::text_writer build;
	build.clear();

	// arg[0] is the executable name
	for ( int i=1; i < CommandLine()->ParmCount(); i++ )
	{
		const char *szParm = CommandLine()->GetParm(i);
		if (!szParm) continue;

		if (szParm[0] == '-')
		{
			// skip -XXX options and eat their args
			const char *szValue = CommandLine()->ParmValue(szParm);
			if ( szValue ) i++;
			continue;
		}
		if (szParm[0] == '+')
		{
			// convert +XXX options and stuff them into the build buffer
			const char *szValue = CommandLine()->ParmValue(szParm);
			if (szValue)
			{
				build.printf("%s %s;", szParm+1, szValue);
				i++;
			}
			else
			{
				build.put(szParm+1);
				build.put(';');
			}
		}
		else
		{
			// singleton values, convert to command
			char const *translated = Cmd_TranslateFileAssociation( CommandLine()->GetParm( i ) );
			if (translated)
			{
				build.put(translated);
				build.put(';');
			}
		}
	}

	return build.c_str();
}

CLauncherLoggingListener g_LauncherLoggingListener;


// #define LOADING_MEMORY_WATCHDOG 100

// This block enables a thread that dumps memory stats at regular
// intervals from the moment the thread pool is initialized until the
// game halts. LOADING_MEMORY_WATCHDOG, if defined, specifies the
// interval in milliseconds

// this class automatically handles the initialization and uninitialization
// of the vpbdm library. It's RAII because we absolutely positively have
// to guarantee that the library cleans up after itself on shutdown,
// even if we early out of LauncherMain.

//-----------------------------------------------------------------------------
// Purpose: The real entry point for the application
// Input  : hInstance -
//			hPrevInstance -
//			lpCmdLine -
//			nCmdShow -
// Output : int APIENTRY
//-----------------------------------------------------------------------------
extern "C" int LauncherMain( int argc, char **argv )
{

	// must precede any thread creation: children inherit the affinity mask
	if ( !CommandLine()->FindParm( "-noaffinity" ) )
		ThreadPinToFastestCores();

	//  fix to stop us from crashing in printf/sscanf functions that don't expect
	//  localization to mess with your "." and "," float seperators. Mac OSX also sets LANG
	//  to en_US.UTF-8 before starting up (in info.plist I believe).
	// We need to double check that localization for libcef is handled correctly
	//  when we slam things to en_US.UTF-8.
	// Also check if C.UTF-8 exists and use it? This file: /usr/lib/locale/C.UTF-8.
	// It looks like it's only installed on Debian distros right now though.
        const char en_US[] = "en_US.UTF-8";

        ks::platform::set_environment( "LC_ALL", en_US, true );
        setlocale( LC_ALL, en_US );

        const char *CurrentLocale = setlocale( LC_ALL, nullptr );
        if ( V_stricmp( CurrentLocale, en_US ) )
        {
                Warning( "WARNING: setlocale('%s') failed, using locale:'%s'. International characters may not work.\n", en_US, CurrentLocale );
        }

	// Hook the debug output stuff.
	LoggingSystem_RegisterLoggingListener( &g_LauncherLoggingListener );

	CommandLine()->CreateCmdLine( argc, argv );


	// Figure out the directory the executable is running from
	UTIL_ComputeBaseDir();


	// GS - If we didn't specify a game name then default to CSGO
	// This is required for running from a HDD Boot Game package
	if ( CommandLine()->CheckParm( "-game") == nullptr )
	{
		CommandLine()->AppendParm( "-game", "csgo" );
	}



	bool bDvdDev, bSpewDllInfo, bWaitForConsole;
	bDvdDev         = CommandLine()->CheckParm( "-dvddev"    ) != nullptr;
	bSpewDllInfo    = CommandLine()->CheckParm( "-dllinfo"   ) != nullptr;
	bWaitForConsole = CommandLine()->CheckParm( "-vxconsole" ) != nullptr;


	// check for a named executable
	const char *exeFilename = GetExecutableFilename();
	if ( exeFilename[0] && V_strcmp( exeFilename, "hl2" ) && !CommandLine()->FindParm( "-game" ) )
	{
		CommandLine()->RemoveParm( "-game" );
		CommandLine()->AppendParm( "-game", exeFilename );
	}

	// Uncomment the following code to allow multiplayer on the Xbox 360 for trade shows.

	// If the game arg is currently portal2
	char const *game_param_val = nullptr;
	CommandLine()->CheckParm( "-game", &game_param_val );

	if( game_param_val && !V_strcmp( game_param_val, "portal2" ) )
	{
		// and if there is a portal2_sixense dir that contains a valid gameinfo.txt, then override the game parameter to use that instead
		if( !CommandLine()->CheckParm( "-nosixense" ) && DoesFileExistIn( ".", "portal2_sixense/gameinfo.txt" ) )
		{
			CommandLine()->RemoveParm( "-game" );
			CommandLine()->AppendParm( "-game", "portal2_sixense" );
		}
	}


	if ( CommandLine()->CheckParm( "-tslist" ) )
	{
		//TestThreads(1);
		int nTests = 10000;
		DevMsg("Running TSList tests\n");
		RunTSListTests( nTests );
		DevMsg("Running TSQueue tests\n");
		RunTSQueueTests( nTests );
		DevMsg("Running Thread Pool tests\n");
		RunThreadPoolTests();
	}


 	// Run in text mode? (No graphics or sound).
 	if ( CommandLine()->CheckParm( "-textmode" ) )
 	{
 	}
	else
	{
		if ( !GrabSourceMutex() )
		{
			::MessageBox(nullptr, "Only one instance of the game can be running at one time.", "Source - Warning", 0 );
			return -1;
		}
	}


	// Add -insecure in order to avoid client timeout message
	CommandLine()->AppendParm( "-insecure", nullptr );

	// Figure out the directory the executable is running from
	// and make that be the current working directory
	// on the PS3, however, there is no concept of current directories
	_chdir( GetBaseDirectory() );

	// When building cubemaps, we don't need sound and can't afford to have async I/O - cubemap writes to the BSP can collide with async bsp reads
	if ( CommandLine()->CheckParm( "-buildcubemaps") )
	{
		CommandLine()->AppendParm( "-nosound", nullptr );
		CommandLine()->AppendParm( "-noasync", nullptr );
	}

	g_LeakDump.m_bCheckLeaks = CommandLine()->CheckParm( "-leakcheck" ) ? true : false;

	bool bRestart = true;
	while ( bRestart )
	{
		bRestart = false;

		CSourceAppSystemGroup sourceSystems;
		CFSApplication application( &sourceSystems );
		int nRetval = application.Run();

		if ( application.GetCurrentStage() == CSourceAppSystemGroup::INITIALIZATION )
		{
			bRestart = (nRetval == INIT_RESTART);
		}
		else if ( nRetval == RUN_RESTART )
		{
			bRestart = true;
		}

		bool bReslistCycle = false;
		if ( !bRestart )
		{
			bReslistCycle = reslistgenerator->ShouldContinue();
			bRestart = bReslistCycle;
		}

		if ( !bReslistCycle )
		{
			// Remove any overrides in case settings changed
			CommandLine()->RemoveParm( "-w" );
			CommandLine()->RemoveParm( "-h" );
			CommandLine()->RemoveParm( "-width" );
			CommandLine()->RemoveParm( "-height" );
			CommandLine()->RemoveParm( "-sw" );
			CommandLine()->RemoveParm( "-startwindowed" );
			CommandLine()->RemoveParm( "-windowed" );
			CommandLine()->RemoveParm( "-window" );
			CommandLine()->RemoveParm( "-full" );
			CommandLine()->RemoveParm( "-fullscreen" );
			CommandLine()->RemoveParm( "-autoconfig" );
			CommandLine()->RemoveParm( "+mat_hdr_level" );
		}
	}


	// Allow other source apps to run
	ReleaseSourceMutex();


	return 0;
}
