// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


#if !defined(DEDICATED)
#endif

#include "quakedef.h"
#include "host.h"
#include "server.h"
#include "vengineserver_impl.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "sys_dll.h"
#include "ivideomode.h"
#include "host_cmd.h"
#include "sv_log.h"
#include "sv_main.h"
#include "traceinit.h"
#include "keys.h"
#include "gl_matsysiface.h"
#include "cmd.h"
#include <ihltvdirector.h>
import std;
import ks.core;
import ks.desktop;
#include "MapReslistGenerator.h"
#include "game_entry.h"
#include "DevShotGenerator.h"
#include "cdll_engine_int.h"
#include "dt_send.h"
#include "idedicatedexports.h"
#include "cvar.h"
#include "tier2/tier2_logging.h"

#include "engineui.h"

constexpr auto ONE_HUNDRED_TWENTY_EIGHT_MB = (128 * 1024 * 1024);

ConVar mem_min_heapsize( "mem_min_heapsize", "48", 0, "Minimum amount of memory to dedicate to engine hunk and datacache (in mb)" );
ConVar mem_max_heapsize( "mem_max_heapsize", "512", 0, "Maximum amount of memory to dedicate to engine hunk and datacache (in mb)" );
ConVar mem_max_heapsize_dedicated( "mem_max_heapsize_dedicated", "64", 0, "Maximum amount of memory to dedicate to engine hunk and datacache, for dedicated server (in mb)" );

#define MINIMUM_WIN_MEMORY			(unsigned)(mem_min_heapsize.GetInt()*1024*1024)
#define MAXIMUM_WIN_MEMORY			std::max( (unsigned)(mem_max_heapsize.GetInt()*1024*1024), MINIMUM_WIN_MEMORY )
inline unsigned MAXIMUM_DEDICATED_MEMORY() { return (unsigned)(mem_max_heapsize_dedicated.GetInt()*1024*1024); }

LoggingChannelID_t LOG_SERVER_LOG = LoggingSystem_RegisterLoggingChannel( "ServerLog", nullptr, LCF_DO_NOT_ECHO );;

char *CheckParm(const char *psz, char **ppszValue = nullptr);
void SeedRandomNumberGenerator( bool random_invariant );
void Con_ColorPrintf( const Color& clr, const char *fmt, ... );

void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );

modinfo_t			gmodinfo;

extern HWND			*pmainwindow;
char				gszDisconnectReason[256];
char				gszExtendedDisconnectReason[256];
bool				gfExtendedError = false;
CreateInterfaceFn	g_AppSystemFactory = nullptr;

static bool			s_bIsDedicated = false;
ConVar *sv_noclipduringpause = nullptr;

// Special mode where the client uses a console window and has no graphics. Useful for stress-testing a server
// without having to round up 32 people.
bool g_bTextMode = false;

// Set to true when we exit from an error.
bool g_bInErrorExit = false;

static FileFindHandle_t	g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;

// The extension DLL directory--one entry per loaded DLL

// Prototype of an global method function
typedef void ( * PFN_GlobalMethod)( edict_t *pEntity );

IServerGameDLL	*serverGameDLL = nullptr;
bool g_bServerGameDLLGreaterThanV5;
IServerGameEnts *serverGameEnts = nullptr;

IServerGameClients *serverGameClients = nullptr;
int g_iServerGameClientsVersion = 0;	// This matches the number at the end of the interface name (so for "ServerGameClients004", this would be 4).

IHLTVDirector	*serverGameDirector = nullptr;

IServerGameTags *serverGameTags = nullptr;

void Sys_InitArgv( char *lpCmdLine );
void Sys_ShutdownArgv( void );

extern bool s_bIsDedicatedServer;

//-----------------------------------------------------------------------------
// Purpose: Compare file times
// Input  : ft1 - 
//			ft2 - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_CompareFileTime( long ft1, long ft2 )
{
	if ( ft1 < ft2 )
	{
		return -1;
	}
	else if ( ft1 > ft2 )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Is slash?
//-----------------------------------------------------------------------------
inline bool IsSlash( char c )
{
	return ( c == '\\') || ( c == '/' );
}


//-----------------------------------------------------------------------------
// Purpose: Create specified directory
// Input  : *path - 
// Output : void Sys_mkdir
//-----------------------------------------------------------------------------
void Sys_mkdir( const char *path, const char *pPathID /*= 0*/ )
{
	char testpath[ MAX_OSPATH ];

	// Remove any terminal backslash or /
	V_strncpy( testpath, path, sizeof( testpath ) );
	int nLen = V_strlen( testpath );
	if ( (nLen > 0) && IsSlash( testpath[ nLen - 1 ] ) )
	{
		testpath[ nLen - 1 ] = 0;
	}

	// Look for URL
	if ( !pPathID )
	{
		pPathID = "MOD";
	}

	if ( IsSlash( testpath[0] ) && IsSlash( testpath[1] ) )
	{
		pPathID = nullptr;
	}

	V_FixSlashes( testpath );

	if ( g_pFileSystem->FileExists( testpath, pPathID ) )
	{
		// if there is a file of the same name as the directory we want to make, just kill it
		if ( !g_pFileSystem->IsDirectory( testpath, pPathID ) )
		{
			g_pFileSystem->RemoveFile( testpath, pPathID );
		}
	}

	g_pFileSystem->CreateDirHierarchy( path, pPathID );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*basename - 
// Output : char *Sys_FindFirst
//-----------------------------------------------------------------------------
const char *Sys_FindFirst(const char *path, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirst(path, &g_hfind);
	if (basename && psz)
	{
		V_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: Sys_FindFirst with a path ID filter.
//-----------------------------------------------------------------------------
const char *Sys_FindFirstEx( const char *pWildcard, const char *pPathID, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirstEx( pWildcard, pPathID, &g_hfind);
	if (basename && psz)
	{
		V_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *basename - 
// Output : char *Sys_FindNext
//-----------------------------------------------------------------------------
const char* Sys_FindNext(char *basename, int namelength)
{
	const char *psz = g_pFileSystem->FindNext(g_hfind);
	if ( basename && psz )
	{
		V_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Sys_FindClose
//-----------------------------------------------------------------------------

void Sys_FindClose(void)
{
	if ( FILESYSTEM_INVALID_FIND_HANDLE != g_hfind )
	{
		g_pFileSystem->FindClose(g_hfind);
		g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: OS Specific initializations
//-----------------------------------------------------------------------------
void Sys_Init( void )
{
	// Set default FPU control word to truncate (chop) mode for optimized _ftol()
	// This does not "stick", the mode is restored somewhere down the line.
//	Sys_TruncateFPU();	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_Shutdown( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Print to system console
// Input  : *fmt - 
//			... - 
// Output : void Sys_Printf
//-----------------------------------------------------------------------------
void Sys_Printf(const char *fmt, ...)
{
	std::va_list		argptr;
	char		text[1024];

	__builtin_va_start (argptr,fmt);
	V_vsnprintf (text, sizeof( text ), fmt, argptr);
	__builtin_va_end (argptr);
		
	if ( developer.GetInt() )
	{
		std::fprintf( stderr, "%s", text );
	}

	if ( s_bIsDedicated )
	{
		std::printf( "%s", text );
	}
}


bool Sys_MessageBox(const char *title, const char *info, bool bShowOkAndCancel)
{
#if   defined( LINUX ) && !defined( DEDICATED )

	static const char *const labels[] = { "OK", "Cancel" };
	const int chosen = ks::desktop::message_box( ks::desktop::game_window(), title, info,
		labels, bShowOkAndCancel ? 2 : 1, 0, bShowOkAndCancel ? 1 : -1 );
	return ( chosen == 0 );

#else

	Warning( "%s\n", info );
	return true;

#endif
}

bool g_bUpdateMinidumpComment = true;


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Error_Internal( bool bMinidump, const char *error, std::va_list argsList )
{
	char		text[1024];
	static      bool bReentry = false; // Don't meltdown

	V_vsnprintf( text, sizeof( text ), error, argsList );

	if ( bReentry )
	{
		std::fprintf( stderr, "%s\n", text );
		return;
	}

	bReentry = true;

	if ( s_bIsDedicated )
	{
		std::printf( "%s\n", text );
	}
	else
	{
		Sys_Printf( "%s\n", text );
	}

	g_bInErrorExit = true;
	
#if !defined( DEDICATED )
	if ( videomode )
		videomode->Shutdown();
#endif

	if ( 		 !CommandLine()->FindParm( "-makereslists" ) &&
		 !CommandLine()->FindParm( "-nomessagebox" ) )
	{
		Sys_MessageBox( "Engine Error", text, false );
	}

	DebuggerBreakIfDebugging();



	if ( bMinidump && !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
	int *p = 0;
	*p = 0xdeadbeef;
	}

	host_initialized = false;
	Plat_ExitProcess( 100 );
}


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Error(const char *error, ...)
{
	std::va_list		argptr;
	
	__builtin_va_start( argptr, error );
	Sys_Error_Internal( true, error, argptr );
	__builtin_va_end( argptr );
	
}


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Exit(const char *error, ...)
{
	std::va_list		argptr;
	
	__builtin_va_start( argptr, error );
	Sys_Error_Internal( false, error, argptr );
	__builtin_va_end( argptr );
	
}


bool IsInErrorExit()
{
	return g_bInErrorExit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msec - 
// Output : void Sys_Sleep
//-----------------------------------------------------------------------------
void Sys_Sleep( int msec )
{
	ThreadSleep( msec );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hInst - 
//			ulInit - 
//			lpReserved - 
// Output : BOOL WINAPI   DllMain
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Allocate memory for engine hunk
// Input  : *parms - 
//-----------------------------------------------------------------------------
void Sys_InitMemory( void )
{
	// Allow overrides
	if ( CommandLine()->FindParm( "-minmemory" ) )
	{
		host_parms.memsize = MINIMUM_WIN_MEMORY;
		return;
	}

	host_parms.memsize = 0;

#if   defined ( DEDICATED )
	// hard code 32 mb for dedicated servers
	host_parms.memsize = MAXIMUM_DEDICATED_MEMORY();

#else
	std::uint64_t memsize = ONE_HUNDRED_TWENTY_EIGHT_MB;

	ks::platform::file_error fileError = ks::platform::file_error::none;
	ks::platform::file *fd = ks::platform::open_file( "/proc/meminfo", ks::platform::file_mode::read, &fileError );
	if ( fd == nullptr )
	{
		Sys_Error( "Can't open /proc/meminfo!\n" );
	}

	char buf[1024 * 16];
	const long br = ks::platform::read_file( fd, buf, sizeof (buf), &fileError );
	ks::platform::close_file( fd );
	if ( br < 0 )
	{
		Sys_Error( "Can't read /proc/meminfo!\n" );
	}
	buf[br] = '\0';

	// Split up the buffer by lines...
	char *line = buf;
	for (char *ptr = buf; *ptr; ptr++)
	{
		if (*ptr == '\n')
		{
			// we've got a complete line.
			*ptr = '\0';
			unsigned long long ull = 0;
			if (std::sscanf(line, "MemTotal: %llu kB", &ull) == 1)
			{
				// found it!
				memsize = ((uint64_t) ull) * 1024;
				break;
			}
			line = ptr;
		}
	}


	if ( memsize > 0xFFFFFFFFUL )
	{
		host_parms.memsize = 0xFFFFFFFFUL;
	}
	else
	{
		host_parms.memsize = memsize;
	}
	
	if ( host_parms.memsize < ONE_HUNDRED_TWENTY_EIGHT_MB )
	{
		Sys_Error( "Available memory less than 128MB!!! %i\n", host_parms.memsize );
	}
	
	// take one quarter the physical memory
	if ( host_parms.memsize <= 512*1024*1024)
	{
		host_parms.memsize >>= 2;
		// Apply cap of 64MB for 512MB systems
		// this keeps the code the same as HL2 gold
		// but allows us to use more memory on 1GB+ systems
		if (host_parms.memsize > MAXIMUM_DEDICATED_MEMORY())
		{
			host_parms.memsize = MAXIMUM_DEDICATED_MEMORY();
		}
	}
	else
	{
		// just take one quarter, no cap
		host_parms.memsize >>= 2;
	}
	
	// At least MINIMUM_WIN_MEMORY mb, even if we have to swap a lot.
	if (host_parms.memsize < MINIMUM_WIN_MEMORY)
	{
		host_parms.memsize = MINIMUM_WIN_MEMORY;
	}
	
	// Apply cap
	if (host_parms.memsize > MAXIMUM_WIN_MEMORY)
	{
		host_parms.memsize = MAXIMUM_WIN_MEMORY;
	}
	
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parms - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void Sys_ShutdownMemory( void )
{
	host_parms.memsize = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_InitAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Debug library spew output
//-----------------------------------------------------------------------------
thread_local int g_bInSpew;


static ConVar sys_minidumpspewlines( "sys_minidumpspewlines", "500", FCVAR_RELEASE, "Lines of crash dump console spew to keep." );

static std::deque< std::string > g_SpewHistory;
static int g_nSpewLines = 1;
static std::recursive_mutex g_SpewMutex;

static void AddSpewRecord( char const *pMsg )
{
#if !defined( DEDICATED )
	std::string str;
	str = ks::format_printf( "%d(%f):  %s", g_nSpewLines, Plat_FloatTime(), pMsg );

	std::scoped_lock lock( g_SpewMutex );
	++g_nSpewLines;
	if ( std::ssize( g_SpewHistory ) > sys_minidumpspewlines.GetInt() )
	{
		g_SpewHistory.pop_front();
	}


	g_SpewHistory.push_back( std::move( str ) );
#endif
}

void GetSpew( char *buf, std::size_t buflen )
{
	std::scoped_lock lock( g_SpewMutex );

	// Walk list backward
	char *pcur = buf;
	int remainder = (int)buflen - 1;

	// Walk backward(
	for ( const std::string &rec : g_SpewHistory | std::views::reverse )
	{
		int len = std::ssize( rec );
		int tocopy = std::min( len, remainder );

		if ( tocopy <= 0 )
			break;
		
		V_memcpy( pcur, rec.c_str(), tocopy );
		remainder -= tocopy;
		pcur += tocopy;

		if ( remainder <= 0 )
			break;
	}
	*pcur = 0;
}




class CEngineConsoleLoggingListener : public ILoggingListener
{
public:
	CEngineConsoleLoggingListener() : ILoggingListener( [this]( const LoggingContext_t *pContext, const tchar *pMessage ) { Log( pContext, pMessage ); } ) {}

	void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( ( pContext->m_Flags & LCF_DO_NOT_ECHO ) != 0 )
		{
			if ( pContext->m_ChannelID == LOG_SERVER_LOG )
			{
				g_Log.Print( pMessage );
			}
			return;
		}
		

		bool suppress = g_bInSpew ? true : false;

		g_bInSpew = true;

		AddSpewRecord( pMessage );

		if ( !suppress )
		{
			// If this is a dedicated server, then we have taken over its spew function, but we still
			// want its vgui console to show the spew, so pass it into the dedicated server.
			if ( dedicated )
			{
				if ( ! IsChildProcess() )							// do NOT let subprocesses output to stdout.
				{
					// This is not actually a varargs-style printf function; it simply takes a char*
					dedicated->Sys_Printf( pMessage );
				}
				else
				{
					SendStringToParentProcess( va( "#%02d:%s", g_nForkID, pMessage ) );
				}

			}

			if ( g_bTextMode )
			{
				std::printf( "%s", pMessage );
			}
			Color spewColor = pContext->m_Color;
			switch ( pContext->m_Severity )
			{
#ifndef DEDICATED
			case LS_MESSAGE:
				if ( pContext->m_Color == UNSPECIFIED_LOGGING_COLOR )
				{
					spewColor.SetColor( 255, 255, 255, 255 );
				}
				break;
			case LS_WARNING:
				spewColor.SetColor( 255, 90, 90, 255 );
				break;
			case LS_ASSERT:
				spewColor.SetColor( 255, 20, 20, 255 );
				break;
			case LS_ERROR:
				spewColor.SetColor( 20, 70, 255, 255 );
				break;
#endif
			}
			
			Con_ColorPrintf( spewColor, "%s", pMessage );
		}

		g_bInSpew = false;

		if ( pContext->m_Severity == LS_ERROR )
		{
			Sys_Error( "%s", pMessage );
		}
	}
};

static CEngineConsoleLoggingListener s_EngineLoggingListener;
static CFileLoggingListener s_FileLoggingListener;
static InterfaceReg s_Reg_CFileLoggingListener_CFileLoggingListener( []() -> void * { return static_cast<CFileLoggingListener *>( &s_FileLoggingListener ); }, FILELOGGINGLISTENER_INTERFACE_VERSION );;

void DeveloperChangeCallback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Set the "developer" spew group to the value...
	ConVarRef var( pConVar );
	int val = var.GetInt();

	LoggingSystem_SetChannelSpewLevelByTag( "Developer", val >= 1 ? LS_MESSAGE : LS_ERROR );
	LoggingSystem_SetChannelSpewLevelByTag( "DeveloperVerbose", val >= 2 ? LS_MESSAGE : LS_ERROR );
}

//-----------------------------------------------------------------------------
// Purpose: factory comglomerator, gets the client, server, and gameui dlls together
//-----------------------------------------------------------------------------
void *GameFactory( const char *pName, int *pReturnCode )
{
	void *pRetVal = nullptr;

	// first ask the app factory
	pRetVal = g_AppSystemFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

	// ask matchmaking
	extern CreateInterfaceFn g_pfnMatchmakingFactory;
	pRetVal = g_pfnMatchmakingFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

#ifndef DEDICATED
	// now ask the client dll
	if (ClientDLL_GetFactory())
	{
		pRetVal = ClientDLL_GetFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}

	// gameui.dll
	if (EngineUI()->GetGameUIFactory())
	{
		pRetVal = EngineUI()->GetGameUIFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}
#endif	
	// server dll factory access would go here when needed
	
	// ask vjobs

	return nullptr;
}



// factory instance
CreateInterfaceFn g_GameSystemFactory = GameFactory;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *lpOrgCmdLine - 
//			launcherFactory - 
//			*pwnd - 
//			bIsDedicated - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_InitGame( CreateInterfaceFn appSystemFactory, const char* pBaseDir, void *pwnd, int bIsDedicated )
{

	extern void InitMathlib( void );
	InitMathlib();
	
	FileSystem_SetWhitelistSpewFlags();

	// Activate console, non-dev spew
	// Must happen before developer.InstallChangeCallback because that callback may reset it 
	LoggingSystem_SetChannelSpewLevelByTag( "Console", LS_MESSAGE );
	LoggingSystem_PushLoggingState();
	LoggingSystem_RegisterLoggingListener( &s_EngineLoggingListener );
	LoggingSystem_RegisterLoggingListener( &s_FileLoggingListener );

	// Install debug spew output....
	developer.InstallChangeCallback( DeveloperChangeCallback );
	
	// Assume failure
	host_initialized = false;
	// Grab main window pointer
	pmainwindow = (HWND *)pwnd;

	// Remember that this is a dedicated server
	s_bIsDedicated = bIsDedicated ? true : false;

	std::memset( &gmodinfo, 0, sizeof( modinfo_t ) );

	static char s_pBaseDir[256];
	V_strncpy( s_pBaseDir, pBaseDir, sizeof( s_pBaseDir ) );
	V_strlower( s_pBaseDir );
	V_FixSlashes( s_pBaseDir );
	host_parms.basedir = s_pBaseDir;

	if ( CommandLine()->FindParm ( "-pidfile" ) )
	{	
		FileHandle_t pidFile = g_pFileSystem->Open( CommandLine()->ParmValue ( "-pidfile", "srcds.pid" ), "w+" );
		if ( pidFile )
		{
			g_pFileSystem->FPrintf( pidFile, "%i\n", (int)ks::platform::process_id() );
			g_pFileSystem->Close(pidFile);
		}
		else
		{
			Warning("Unable to open pidfile (%s)\n", CommandLine()->CheckParm ( "-pidfile" ));
		}
	}


	// Initialize clock
	TRACEINIT( Sys_Init(), Sys_Shutdown() );

	// NOTE: Can't use COM_CheckParm here because it hasn't been set up yet.
	SeedRandomNumberGenerator( CommandLine()->FindParm( "-random_invariant" ) != 0 );

	TRACEINIT( Sys_InitMemory(), Sys_ShutdownMemory() );

	TRACEINIT( Host_Init( s_bIsDedicated ), Host_Shutdown() );

	if ( !host_initialized )
	{
		return 0;
	}

	TRACEINIT( Sys_InitAuthentication(), Sys_ShutdownAuthentication() );

	MapReslistGenerator_BuildMapList();



	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void )
{
	TRACESHUTDOWN( Sys_ShutdownAuthentication() );

	TRACESHUTDOWN( Host_Shutdown() );

	TRACESHUTDOWN( Sys_ShutdownMemory() );

	// TRACESHUTDOWN( Sys_ShutdownArgv() );

	TRACESHUTDOWN( Sys_Shutdown() );

	// Remove debug spew output....
	developer.RemoveChangeCallback( DeveloperChangeCallback );
	LoggingSystem_PopLoggingState();
}

//
// Get the interfaces of the game server linked into this executable.
//

CreateInterfaceFn g_ServerFactory;


static bool LoadServerGame()
{
	// Interface factory and the interfaces exported by the game server
	g_ServerFactory = ServerGameFactory();
	if ( g_ServerFactory )
	{
		g_bServerGameDLLGreaterThanV5 = true;
		serverGameDLL = (IServerGameDLL*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL, nullptr);
		if ( !serverGameDLL )
		{
			Msg( "Could not get IServerGameDLL interface from the game server" );
			goto IgnoreServerGame;
		}

		serverGameEnts = (IServerGameEnts*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEENTS, nullptr);
		if ( !serverGameEnts )
		{
			ConMsg( "Could not get IServerGameEnts interface from the game server" );
			goto IgnoreServerGame;
		}
		
		serverGameClients = (IServerGameClients*)g_ServerFactory(INTERFACEVERSION_SERVERGAMECLIENTS, nullptr);
		if ( serverGameClients )
		{
			g_iServerGameClientsVersion = 4;
		}
		else
		{
			// Try the previous version.
			const char *pINTERFACEVERSION_SERVERGAMECLIENTS_V3 = "ServerGameClients003";
			serverGameClients = (IServerGameClients*)g_ServerFactory(pINTERFACEVERSION_SERVERGAMECLIENTS_V3, nullptr);
			if ( serverGameClients )
			{
				g_iServerGameClientsVersion = 3;
			}
			else
			{
				ConMsg( "Could not get IServerGameClients interface from the game server" );
				goto IgnoreServerGame;
			}
		}
		serverGameDirector = (IHLTVDirector*)g_ServerFactory(INTERFACEVERSION_HLTVDIRECTOR, nullptr);
		if ( !serverGameDirector )
		{
			ConMsg( "Could not get IHLTVDirector interface from the game server" );
			// this is not a critical 
		}

		serverGameTags = (IServerGameTags*)g_ServerFactory(INTERFACEVERSION_SERVERGAMETAGS, nullptr);
		// Possible that this is NULL - optional interface
	}
	else
	{
		ConMsg( "No game server in this executable" );
		goto IgnoreServerGame;
	}

	return true;

IgnoreServerGame:
	g_ServerFactory = nullptr;
	serverGameDLL = nullptr;
	serverGameEnts = nullptr;
	serverGameClients = nullptr;
	return false;
}

//
// Set up the game server.
//
void LoadEntityDLLs( const char *szBaseDir, bool bServerOnly )
{
	std::memset( &gmodinfo, 0, sizeof( modinfo_t ) );
	gmodinfo.version = 1;
	gmodinfo.svonly  = true;

	sv_noclipduringpause = nullptr;

	// Listing file for this game.
	KeyValues *modinfo = new KeyValues("modinfo");
	if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
	{
		V_strncpy( gmodinfo.szInfo, modinfo->GetString("url_info"), sizeof( gmodinfo.szInfo ) );
		V_strncpy( gmodinfo.szDL, modinfo->GetString("url_dl"), sizeof( gmodinfo.szDL ) );
		gmodinfo.version = modinfo->GetInt("version");
		gmodinfo.size = modinfo->GetInt("size");
		gmodinfo.svonly = modinfo->GetBool("svonly");
		gmodinfo.cldll = modinfo->GetBool("cldll");
		V_strncpy( gmodinfo.szHLVersion, modinfo->GetString("hlversion"), sizeof( gmodinfo.szHLVersion ) );
	}
	modinfo->deleteThis();
	
	LoadServerGame();

	if ( serverGameDLL )
	{
		Msg("Game.dll loaded for \"%s\"\n", (char *)serverGameDLL->GetGameDescription());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves a string value from the registry
//-----------------------------------------------------------------------------

void Sys_GetRegKeyValue( char *pszSubKey, const char *pszElement, char *pszReturnString, int nReturnLength, char *pszDefaultValue )
{
	//hushed Assert( !"Impl me" );
	V_strncpy( pszReturnString, pszDefaultValue, nReturnLength );
}

void Sys_GetRegKeyValueInt( char *pszSubKey, char *pszElement, long *plReturnValue, long lDefaultValue)
{
	//hushed Assert( !"Impl me" );
	*plReturnValue = lDefaultValue;
}

void Sys_SetRegKeyValue( const char *pszSubKey, const char *pszElement,	const char *pszValue )
{
	//hushed Assert( !"Impl me" );
}

inline constexpr const char *SOURCE_ENGINE_APP_CLASS = "Valve.Source";

void Sys_CreateFileAssociations( int count, FileAssociationInfo *list )
{
}

void Sys_NoCrashDialog()
{
}

void Sys_TestSendKey( const char *pKey )
{
}

void Sys_OutputDebugString(const char *msg)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UnloadEntityDLLs( void )
{
	if ( !g_ServerFactory )
		return;

	g_ServerFactory = nullptr;
	serverGameDLL = nullptr;
	serverGameEnts = nullptr;
	serverGameClients = nullptr;
	sv_noclipduringpause = nullptr;
}

static void star_memory( const CCommand &args );
static ConCommand star_memory_command( "star_memory", star_memory, "Dump memory stats" );
static void star_memory( const CCommand &args )
{
	// get a current stat of available memory
	// 32 MB is reserved and fixed by OS, so not reporting to allow memory loggers sync
	const ks::platform::heap_info memstats = ks::platform::heap_usage();
	Msg( "sbrk size: %.2f MB, Used: %.2f MB, #mallocs = %lu\n",
		 memstats.arena / ( 1024.0 * 1024.0), memstats.in_use / ( 1024.0 * 1024.0 ), memstats.mapped_blocks );
}
