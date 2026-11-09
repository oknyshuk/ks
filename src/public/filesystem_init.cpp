// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation



#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#endif
#if defined(CLIENT_DLL)
import std;
import ks.core;
import ks.tier1;
import ks.vstdlib;
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#endif
#undef PROTECT_FILEIO_FUNCTIONS

import ks.platform;
#include "filesystem_init.h"
#include "appframework/IAppSystemGroup.h"


#define GAMEINFO_FILENAME			"gameinfo.txt"
inline constexpr const char *GAMEINFO_FILENAME_ALTERNATE = "gameinfo.txt";

static char g_FileSystemError[256];
static bool s_bUseVProjectBinDir = false;
static FSErrorMode_t g_FileSystemErrorMode = FS_ERRORMODE_VCONFIG;

// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir( bool bEnable )
{
	s_bUseVProjectBinDir = bEnable;
}

// ---------------------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------------------- //
void Q_getwd( char *out, int outSize )
{
	ks::platform::current_directory( out, (unsigned long)outSize );
	std::strcat( out, "/" );
	V_FixSlashes( out );
}

// ---------------------------------------------------------------------------------------------------- //
// Module interface.
// ---------------------------------------------------------------------------------------------------- //

CFSSearchPathsInit::CFSSearchPathsInit()
{
	m_pDirectoryName = nullptr;
	m_pLanguage = nullptr;
	m_pfnAddonSearchPaths = nullptr;
	m_ModPath[0] = 0;
}


CFSSetupInfo::CFSSetupInfo()
{
	m_pDirectoryName = nullptr;
	m_bOnlyUseDirectoryName = false;
}


CFSLoadModuleInfo::CFSLoadModuleInfo()
{
	m_pFileSystemDLLName = nullptr;
	m_pFileSystem = nullptr;
	m_pModule = nullptr;
}


const char *FileSystem_GetLastErrorString()
{
	return g_FileSystemError;
}


void AddLanguageGameDir( IFileSystem *pFileSystem, const char *pLocation, const char *pLanguage )
{
#if !defined( DEDICATED )
	char temp[MAX_PATH];
	V_snprintf( temp, sizeof(temp), "%s_%s", pLocation, pLanguage );
	pFileSystem->AddSearchPath( temp, "GAME", PATH_ADD_TO_TAIL );

	// also look in "..\localization\<folder>" if that directory exists
	char baseDir[MAX_PATH];
	char *tempPtr = nullptr, *gameDir = nullptr;

	V_strncpy( baseDir, pLocation, sizeof(baseDir) );
	tempPtr = V_strstr( baseDir, "/game/" );
	if ( tempPtr )
	{
		gameDir = tempPtr + V_strlen( "/game/" );
		*tempPtr = 0;
		V_snprintf( temp, sizeof(temp), "%s%clocalization%c%s_%s", baseDir, CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, gameDir, pLanguage );
		
		if ( pFileSystem->IsDirectory( temp ) )
		{
			pFileSystem->AddSearchPath( temp, "GAME", PATH_ADD_TO_TAIL );
		}
	}
#endif
}


void AddGameBinDir( IFileSystem *pFileSystem, const char *pLocation, bool bForceHead = false )
{
	char temp[MAX_PATH];
	V_snprintf( temp, sizeof(temp), "%s%cbin", pLocation, CORRECT_PATH_SEPARATOR );
	pFileSystem->AddSearchPath( temp, "GAMEBIN", bForceHead ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL );
}

KeyValues* ReadKeyValuesFile( const char *pFilename )
{
	// Read in the gameinfo.txt file and null-terminate it.
	std::FILE *fp = std::fopen( pFilename, "rb" );
	if ( !fp )
		return nullptr;
	std::vector< char > buf;
	std::fseek( fp, 0, k_nSeekEnd );
	buf.assign( std::ftell( fp ) + 1, {} );
	std::fseek( fp, 0, k_nSeekSet );
	std::fread( buf.data(), 1, std::ssize( buf )-1, fp );
	std::fclose( fp );
	buf[std::ssize( buf )-1] = 0;

	KeyValues *kv = new KeyValues( "" );
	if ( !kv->LoadFromBuffer( pFilename, buf.data() ) )
	{
		kv->deleteThis();
		return nullptr;
	}
	
	return kv;
}

static bool Sys_GetExecutableName( char *out, int len )
{


	if ( CommandLine()->GetParm(0) )
	{
		V_MakeAbsolutePath( out, len, CommandLine()->GetParm(0) );
	}
	else
	{
		return false;
	}

	
	return true;
}

bool FileSystem_GetExecutableDir( char *exedir, int exeDirLen )
{
	exedir[0] = 0;

	if ( s_bUseVProjectBinDir )
	{
		const char *pProject = GetVProjectCmdLineValue();
		if ( !pProject )
		{
			// Check their registry.
			pProject = std::getenv( GAMEDIR_TOKEN );
		}
		if ( pProject )
		{
			V_snprintf( exedir, exeDirLen, "%s%c..%cbin", pProject, CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR );
			return true;
		}
		return false;
	}

	if ( !Sys_GetExecutableName( exedir, exeDirLen ) )
		return false;
	V_StripFilename( exedir );


	V_FixSlashes( exedir );

	// Return the bin directory as the executable dir if it's not in there
	// because that's really where we're running from...
	char ext[MAX_PATH];
	V_StrRight( exedir, 4, ext, sizeof( ext ) );
	if ( ext[0] != CORRECT_PATH_SEPARATOR || V_stricmp( ext+1, "bin" ) != 0 )
	{
		V_strncat( exedir, CORRECT_PATH_SEPARATOR_S, exeDirLen, COPY_ALL_CHARACTERS );
		V_strncat( exedir, "bin", exeDirLen, COPY_ALL_CHARACTERS );

		V_FixSlashes( exedir );
	}
	
	return true;
}

static bool FileSystem_GetBaseDir( char *baseDir, int baseDirLen )
{
	if ( FileSystem_GetExecutableDir( baseDir, baseDirLen ) )
	{
		V_StripFilename( baseDir );
		return true;
	}

	return false;
}

void LaunchVConfig()
{
}

const char* GetVProjectCmdLineValue()
{
	return CommandLine()->ParmValue( "-vproject", CommandLine()->ParmValue( "-game" ) );
}

FSReturnCode_t SetupFileSystemError( bool bRunVConfig, FSReturnCode_t retVal, const char *pMsg, ... )
{
	std::va_list marker;
	__builtin_va_start( marker, pMsg );
	V_vsnprintf( g_FileSystemError, sizeof( g_FileSystemError ), pMsg, marker );
	__builtin_va_end( marker );

	Warning( "%s\n", g_FileSystemError );

	// Run vconfig?
	// Don't do it if they specifically asked for it not to, or if they manually specified a vconfig with -game or -vproject.
	if ( bRunVConfig && g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG && !CommandLine()->FindParm( CMDLINEOPTION_NOVCONFIG ) && !GetVProjectCmdLineValue() )
	{
		LaunchVConfig();
	}

	if ( g_FileSystemErrorMode == FS_ERRORMODE_AUTO || g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG )
	{
		// this may happen when we eject disk while loading the game. 
		{
			Error( "%s\n", g_FileSystemError );
		}
	}
	
	return retVal;
}

FSReturnCode_t LoadGameInfoFile( 
	const char *pDirectoryName, 
	KeyValues *&pMainFile, 
	KeyValues *&pFileSystemInfo, 
	KeyValues *&pSearchPaths )
{
	// If GameInfo.txt exists under pBaseDir, then this is their game directory.
	// All the filesystem mappings will be in this file.
	char gameinfoFilename[MAX_PATH];
	V_strncpy( gameinfoFilename, pDirectoryName, sizeof( gameinfoFilename ) );
	V_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
	V_strncat( gameinfoFilename, GAMEINFO_FILENAME, sizeof( gameinfoFilename ), COPY_ALL_CHARACTERS );
	V_FixSlashes( gameinfoFilename );
	pMainFile = ReadKeyValuesFile( gameinfoFilename );
	if ( !pMainFile )
	{
		return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "%s is missing.", gameinfoFilename );
	}

	pFileSystemInfo = pMainFile->FindKey( "FileSystem" );
	if ( !pFileSystemInfo )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}

	// Now read in all the search paths.
	pSearchPaths = pFileSystemInfo->FindKey( "SearchPaths" );
	if ( !pSearchPaths )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}
	return FS_OK;
}

// checks the registry for the low violence setting
// Check "HKEY_CURRENT_USER\Software\Valve\Source\Settings" and "User Token 2" or "User Token 3"
bool IsLowViolenceBuild( void )
{

	// Users can opt into low violence mode on the command-line.
	if ( CommandLine()->FindParm( "-lv" ) != 0 )
		return true;

	return false;
}

static void FileSystem_AddLoadedSearchPath( 
	CFSSearchPathsInit &initInfo, 
	const char *pPathID, 
	bool *bFirstGamePath, 
	const char *pBaseDir, 
	const char *pLocation,
	bool bLowViolence )
{
	char fullLocationPath[MAX_PATH];
	V_MakeAbsolutePath( fullLocationPath, sizeof( fullLocationPath ), pLocation, pBaseDir );

	// Now resolve any ./'s.
	V_FixSlashes( fullLocationPath );
	if ( !V_RemoveDotSlashes( fullLocationPath ) )
		Error( "FileSystem_AddLoadedSearchPath - Can't resolve pathname for '%s'", fullLocationPath );

	// Add language, mod, and gamebin search paths automatically.
	if ( V_stricmp( pPathID, "game" ) == 0 )
	{
		bool bDoAllPaths = true;

		// add the language path, needs to be topmost, generally only contains audio
		// and the language localized movies (there are 2 version one normal, one LV)
		// this trumps the LV english movie as desired for the language
		if ( initInfo.m_pLanguage && bDoAllPaths )
		{
			AddLanguageGameDir( initInfo.m_pFileSystem, fullLocationPath, initInfo.m_pLanguage );
		}

		// next add the low violence path
		if ( bLowViolence && bDoAllPaths )
		{
			char szPath[MAX_PATH];
			V_snprintf( szPath, sizeof(szPath), "%s_lv", fullLocationPath );
			initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
		}
		
		if ( CommandLine()->FindParm( "-tempcontent" ) != 0 && bDoAllPaths )
		{
			char szPath[MAX_PATH];
			V_snprintf( szPath, sizeof(szPath), "%s_tempcontent", fullLocationPath );
			initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
		}

		// mark the first "game" dir as the "MOD" dir
		if ( *bFirstGamePath )
		{
			*bFirstGamePath = false;
			initInfo.m_pFileSystem->AddSearchPath( fullLocationPath, "MOD", PATH_ADD_TO_TAIL );
			V_strncpy( initInfo.m_ModPath, fullLocationPath, sizeof( initInfo.m_ModPath ) );
		}
	
		if ( bDoAllPaths )
		{
			// The game is linked into the executable; the shared libraries live in the
			// executable's own bin dir, so the mod's bin/ is deliberately not searched -- it would shadow them.
			char binBase[MAX_PATH];
			if ( FileSystem_GetBaseDir( binBase, sizeof( binBase ) ) )
				AddGameBinDir( initInfo.m_pFileSystem, binBase );
		}
	}

	initInfo.m_pFileSystem->AddSearchPath( fullLocationPath, pPathID, PATH_ADD_TO_TAIL );
}


bool FileSystem_IsHldsUpdateToolDedicatedServer()
{
	// To determine this, we see if the directory our executable was launched from is "orangebox".
	// We only are under "orangebox" if we're run from hldsupdatetool.
	char baseDir[MAX_PATH];
	if ( !FileSystem_GetBaseDir( baseDir, sizeof( baseDir ) ) )
		return false;

	V_FixSlashes( baseDir );
	V_StripTrailingSlash( baseDir );
	const char *pLastDir = V_UnqualifiedFileName( baseDir );
	return ( pLastDir && V_stricmp( pLastDir, "orangebox" ) == 0 );
}

FSReturnCode_t FileSystem_LoadSearchPaths( CFSSearchPathsInit &initInfo )
{
	ks::ScratchArena<> scratch;
	if ( !initInfo.m_pFileSystem || !initInfo.m_pDirectoryName )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_LoadSearchPaths: Invalid parameters specified." );

	KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
	FSReturnCode_t retVal = LoadGameInfoFile( initInfo.m_pDirectoryName, pMainFile, pFileSystemInfo, pSearchPaths );
	if ( retVal != FS_OK )
		return retVal;
	
	// All paths except those marked with |gameinfo_path| are relative to the base dir.
	char baseDir[MAX_PATH];
	if ( !FileSystem_GetBaseDir( baseDir, sizeof( baseDir ) ) )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetBaseDir failed." );

	initInfo.m_ModPath[0] = 0;

	#define GAMEINFOPATH_TOKEN		"|gameinfo_path|"
	#define BASESOURCEPATHS_TOKEN	"|all_source_engine_paths|"

	bool bLowViolence = IsLowViolenceBuild();
	bool bFirstGamePath = true;
	
	for ( KeyValues *pCur=pSearchPaths->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		const char *pPathID = pCur->GetName();
		const char *pLocation = pCur->GetString();
		
		if ( V_stristr( pLocation, GAMEINFOPATH_TOKEN ) == pLocation )
		{
			pLocation += std::strlen( GAMEINFOPATH_TOKEN );
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, initInfo.m_pDirectoryName, pLocation, bLowViolence );
		}
		else if ( V_stristr( pLocation, BASESOURCEPATHS_TOKEN ) == pLocation )
		{
			// This is a special identifier that tells it to add the specified path for all source engine versions equal to or prior to this version.
			// So in Orange Box, if they specified:
			//		|all_source_engine_paths|hl2
			// it would add the ep2\hl2 folder and the base (ep1-era) hl2 folder.
			//
			// We need a special identifier in the gameinfo.txt here because the base hl2 folder exists in different places.
			// In the case of an hldsupdatetool dedicated server, the base hl2 folder is "..\..\hl2" (since we're up in the 'orangebox' folder).
												   
			pLocation += std::strlen( BASESOURCEPATHS_TOKEN );

			// Add the Orange-box path.
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, pLocation, bLowViolence );

			if ( FileSystem_IsHldsUpdateToolDedicatedServer() )
			{			
				// If we're using the hldsupdatetool dedicated server, then go up a directory to get the ep1-era files too.
				char ep1EraPath[MAX_PATH];
				V_snprintf( ep1EraPath, sizeof( ep1EraPath ), "..%c%s", CORRECT_PATH_SEPARATOR, pLocation );
				FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, ep1EraPath, bLowViolence );
			}
		}
		else
		{
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, pLocation, bLowViolence );
		}
	}


	pMainFile->deleteThis();

	//
	// Set up search paths for add-ons
	//
	if ( initInfo.m_pfnAddonSearchPaths )
	{
		initInfo.m_pfnAddonSearchPaths( initInfo.m_pFileSystem );
	}

	// Add the "platform" directory as a game searchable path
	char pPlatformPath[MAX_PATH];
	V_ComposeFileName( baseDir, "platform", pPlatformPath, sizeof(pPlatformPath) );
	initInfo.m_pFileSystem->AddSearchPath( pPlatformPath, "GAME", PATH_ADD_TO_TAIL );

	// these specialized tool paths are not used on 360 and cause a costly constant perf tax, so inhibited
	// Create a content search path based on the game search path
	char szContentRoot[MAX_PATH];
	V_strncpy( szContentRoot, baseDir, sizeof(szContentRoot) );
	char *pRootEnd = V_strrchr( szContentRoot, '\\' );
	if ( pRootEnd )
	{
		*pRootEnd = '\0';
	}

	V_strncat( szContentRoot, "\\content", sizeof( szContentRoot ) );

	int nLen = initInfo.m_pFileSystem->GetSearchPath( "GAME", false, nullptr, 0 );
	char *pSearchPath = scratch.Alloc<char>( nLen );
	initInfo.m_pFileSystem->GetSearchPath( "GAME", false, pSearchPath, nLen );
	char *pPath = pSearchPath;
	while( pPath )
	{
		char *pSemiColon = std::strchr( pPath, ';' );
		if ( pSemiColon )
		{
			*pSemiColon = 0;
		}

		V_StripTrailingSlash( pPath );
		V_FixSlashes( pPath );

		const char *pCurPath = pPath;
		pPath = pSemiColon ? pSemiColon + 1 : nullptr;

		char pRelativePath[MAX_PATH];
		char pContentPath[MAX_PATH];
		if ( !V_MakeRelativePath( pCurPath, baseDir, pRelativePath, sizeof(pRelativePath) ) )
			continue;

		V_ComposeFileName( szContentRoot, pRelativePath, pContentPath, sizeof(pContentPath) );
		initInfo.m_pFileSystem->AddSearchPath( pContentPath, "CONTENT" );
	}

	// Also, mark specific path IDs as "by request only". That way, we won't waste time searching in them
	// when people forget to specify a search path.
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "content", true );

	// Also, mark specific path IDs as "by request only". That way, we won't waste time searching in them
	// when people forget to specify a search path.
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "executable_path", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "gamebin", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "mod", true );

	// Add the write path last.
	if ( initInfo.m_ModPath[0] != 0 )
	{
		initInfo.m_pFileSystem->AddSearchPath( initInfo.m_ModPath, "DEFAULT_WRITE_PATH", PATH_ADD_TO_TAIL );
	}

#ifdef _DEBUG	
	initInfo.m_pFileSystem->PrintSearchPaths();
#endif

	return FS_OK;
}

bool DoesFileExistIn( const char *pDirectoryName, const char *pFilename )
{
	char filename[MAX_PATH];

	V_strncpy( filename, pDirectoryName, sizeof( filename ) );
	V_AppendSlash( filename, sizeof( filename ) );
	V_strncat( filename, pFilename, sizeof( filename ), COPY_ALL_CHARACTERS );
	V_FixSlashes( filename );
	bool bExist = ks::platform::path_exists( filename );

	return ( bExist );
}

namespace
{
	SuggestGameInfoDirFn_t & GetSuggestGameInfoDirFn( void )
	{
		static SuggestGameInfoDirFn_t s_pfnSuggestGameInfoDir = nullptr;
		return s_pfnSuggestGameInfoDir;
	}
}; // `anonymous` namespace

SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn( SuggestGameInfoDirFn_t pfnNewFn )
{
	SuggestGameInfoDirFn_t &rfn = GetSuggestGameInfoDirFn();
	SuggestGameInfoDirFn_t pfnOldFn = rfn;
	rfn = pfnNewFn;
	return pfnOldFn;
}

static FSReturnCode_t TryLocateGameInfoFile( char *pOutDir, int outDirLen, bool bBubbleDir )
{
	// Retain a copy of suggested path for further attempts
	std::unique_ptr< char[] > spchCopyNameBuffer( new char [ outDirLen ] );
	V_strncpy( spchCopyNameBuffer.get(), pOutDir, outDirLen );
	spchCopyNameBuffer[ outDirLen - 1 ] = 0;

	// Make appropriate slashes ('/' - Linux style)
	for ( char *pchFix = spchCopyNameBuffer.get(),
		*pchEnd = pchFix + outDirLen;
		pchFix < pchEnd; ++ pchFix )
	{
		if ( '\\' == *pchFix )
		{
			*pchFix = '/';
		}
	}

	// Have a look in supplied path
	do
	{
		if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
		{
			return FS_OK;
		}
	} 
	while ( bBubbleDir && V_StripLastDir( pOutDir, outDirLen ) );

	// Make an attempt to resolve from "content -> game" directory
	V_strncpy( pOutDir, spchCopyNameBuffer.get(), outDirLen );
	pOutDir[ outDirLen - 1 ] = 0;
	if ( char *pchContentFix = V_stristr( pOutDir, "/content/" ) )
	{
		V_strcpy( pchContentFix, "/game/" );
		std::memmove( pchContentFix + 6, pchContentFix + 9, pOutDir + outDirLen - (pchContentFix + 9) );

		// Try in the mapped "game" directory
		do
		{
			if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
			{
				return FS_OK;
			}
		} 
		while ( bBubbleDir && V_StripLastDir( pOutDir, outDirLen ) );
	}

	// Could not find it here
	return FS_MISSING_GAMEINFO_FILE;
}

FSReturnCode_t LocateGameInfoFile( const CFSSetupInfo &fsInfo, char *pOutDir, int outDirLen )
{
	// Engine and Hammer don't want to search around for it.
	if ( fsInfo.m_bOnlyUseDirectoryName )
	{
		if ( !fsInfo.m_pDirectoryName )
			return SetupFileSystemError( false, FS_MISSING_GAMEINFO_FILE, "bOnlyUseDirectoryName=1 and pDirectoryName=NULL." );

		bool bExists = DoesFileExistIn( fsInfo.m_pDirectoryName, GAMEINFO_FILENAME );
		if ( !bExists )
		{

			return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "Setup file '%s' doesn't exist in subdirectory '%s'.\nCheck your -game parameter or VCONFIG setting.", GAMEINFO_FILENAME, fsInfo.m_pDirectoryName );
		}

		V_strncpy( pOutDir, fsInfo.m_pDirectoryName, outDirLen );
		return FS_OK;
	}

	// First, check for overrides on the command line or environment variables.
	const char *pProject = GetVProjectCmdLineValue();

	if ( pProject )
	{
		if ( DoesFileExistIn( pProject, GAMEINFO_FILENAME ) )
		{
			V_MakeAbsolutePath( pOutDir, outDirLen, pProject );
			return FS_OK;
		}


		// They either specified vproject on the command line or it's in their registry. Either way,
		// we don't want to continue if they've specified it but it's not valid.
		goto ShowError;
	}

	// Ask the application if it can provide us with a game info directory
	{
		bool bBubbleDir = true;
		SuggestGameInfoDirFn_t pfnSuggestGameInfoDirFn = GetSuggestGameInfoDirFn();
		if ( pfnSuggestGameInfoDirFn &&
			( * pfnSuggestGameInfoDirFn )( &fsInfo, pOutDir, outDirLen, &bBubbleDir ) &&
			FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, bBubbleDir ) )
			return FS_OK;
	}

	// Try to use the environment variable / registry
	if ( ( pProject = std::getenv( GAMEDIR_TOKEN ) ) != nullptr &&
		 ( V_MakeAbsolutePath( pOutDir, outDirLen, pProject ), 1 ) &&
		 FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, false ) )
		return FS_OK;

	Warning( "Warning: falling back to auto detection of vproject directory.\n" );
	
	// Now look for it in the directory they passed in.
	if ( fsInfo.m_pDirectoryName )
		V_MakeAbsolutePath( pOutDir, outDirLen, fsInfo.m_pDirectoryName );
	else
		V_MakeAbsolutePath( pOutDir, outDirLen, "." );

	if ( FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, true ) )
		return FS_OK;

	// Use the CWD
	Q_getwd( pOutDir, outDirLen );
	if ( FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, true ) )
		return FS_OK;

ShowError:
	return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, 
		"Unable to find %s. Solutions:\n\n"
		"1. Read http://www.valve-erc.com/srcsdk/faq.html#NoGameDir\n"
		"2. Run vconfig to specify which game you're working on.\n"
		"3. Add -game <path> on the command line where <path> is the directory that %s is in.\n",
		GAMEINFO_FILENAME, GAMEINFO_FILENAME );
}

FSReturnCode_t FileSystem_SetBasePaths( IFileSystem *pFileSystem )
{
	pFileSystem->RemoveSearchPaths( "EXECUTABLE_PATH" );

	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );

	pFileSystem->AddSearchPath( executablePath, "EXECUTABLE_PATH" );
	return FS_OK;
}

//-----------------------------------------------------------------------------
// Returns the name of the file system DLL to use
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_GetFileSystemDLLName( char *pFileSystemDLL, int nMaxLen )
{
	// Inside of here, we don't have a filesystem yet, so we have to assume that the filesystem_stdio
	// is in this same directory with us.
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );

	V_snprintf( pFileSystemDLL, nMaxLen, "%s%clibfilesystem_stdio.so", executablePath, CORRECT_PATH_SEPARATOR );

	return FS_OK;
}

//-----------------------------------------------------------------------------
// Sets up the environment + gets back the gameinfo.txt path
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupEnvironment( CFSSetupInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	FSReturnCode_t ret = LocateGameInfoFile( fsInfo, fsInfo.m_GameInfoPath, sizeof( fsInfo.m_GameInfoPath ) );
	if ( ret != FS_OK )
		return ret;

	// This is so that processes spawned by this application will have the same VPROJECT
	ks::platform::set_environment( GAMEDIR_TOKEN, fsInfo.m_GameInfoPath, true );

	return FS_OK;
}


//-----------------------------------------------------------------------------
// Loads the file system module
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_LoadFileSystemModule( CFSLoadModuleInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	FSReturnCode_t ret = FileSystem_SetupEnvironment( fsInfo );
	if ( ret != FS_OK )
		return ret;

	// Now that the environment is setup, load the filesystem module.
	if ( !Sys_LoadInterface(
		fsInfo.m_pFileSystemDLLName,
		FILESYSTEM_INTERFACE_VERSION,
		&fsInfo.m_pModule,
		(void**)&fsInfo.m_pFileSystem ) )
	{
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Can't load %s.", fsInfo.m_pFileSystemDLLName );
	}

	if ( !fsInfo.m_pFileSystem->Connect( fsInfo.m_ConnectFactory ) )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Connect failed.", fsInfo.m_pFileSystemDLLName );

	if ( fsInfo.m_pFileSystem->Init() != INIT_OK )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Init failed.", fsInfo.m_pFileSystemDLLName );

	return FS_OK;
}


void FileSystem_SetErrorMode( FSErrorMode_t errorMode )
{
	g_FileSystemErrorMode = errorMode;
}

//-----------------------------------------------------------------------------
// Adds the platform folder to the search path.
//-----------------------------------------------------------------------------
void FileSystem_AddSearchPath_Platform( IFileSystem *pFileSystem, const char *szGameInfoPath )
{
	char platform[MAX_PATH];
	V_strncpy( platform, szGameInfoPath, MAX_PATH );
	V_StripTrailingSlash( platform );
	V_strncat( platform, "/../platform", MAX_PATH, MAX_PATH );

	pFileSystem->AddSearchPath( platform, "PLATFORM" );
}
