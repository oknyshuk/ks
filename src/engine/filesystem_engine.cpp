// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "quakedef.h" // for max_ospath
#include "filesystem.h"
#include "bitmap/tgawriter.h"
#include <tier2/tier2.h>
import ks.tier1;
import std;
#include "filesystem_init.h"
#include "host.h"

inline constexpr const char *ADDONLIST_FILENAME = "addonlist.txt";
inline constexpr const char *ADDONS_DIRNAME = "addons";

IFileSystem *g_pFileSystem = nullptr;

// This comes is in filesystem_init.cpp
extern KeyValues* ReadKeyValuesFile( const char *pFilename );

void fs_whitelist_spew_flags_changefn( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( g_pFileSystem )
	{
		ConVarRef var( pConVar );
		g_pFileSystem->SetWhitelistSpewFlags( var.GetInt() );
	}
}

#if defined( _DEBUG )
ConVar fs_whitelist_spew_flags( "fs_whitelist_spew_flags", "0", 0,
	"Set whitelist spew flags to a combination of these values:\n"
	"   0x0001 - list files as they are added to the CRC tracker\n"
	"   0x0002 - show files the filesystem is telling the engine to reload\n"
	"   0x0004 - show files the filesystem is NOT telling the engine to reload",
	fs_whitelist_spew_flags_changefn );
#endif

static void path( const CCommand &args );
static ConCommand path_command( "path", path, "Show the engine filesystem path." );
static void path( const CCommand &args )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintSearchPaths();
	}
}

static void fs_printopenfiles( const CCommand &args );
static ConCommand fs_printopenfiles_command( "fs_printopenfiles", fs_printopenfiles, "Show all files currently opened by the engine." );
static void fs_printopenfiles( const CCommand &args )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintOpenedFiles();
	}
}

static void fs_warning_level( const CCommand &args );
static ConCommand fs_warning_level_command( "fs_warning_level", fs_warning_level, "Set the filesystem warning level." );
static void fs_warning_level( const CCommand &args )
{
	if( args.ArgC() != 2 )
	{
		Warning( "\"fs_warning_level n\" where n is one of:\n" );
		Warning( "\t0:\tFILESYSTEM_WARNING_QUIET\n" );
		Warning( "\t1:\tFILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		Warning( "\t2:\tFILESYSTEM_WARNING_REPORTUSAGE\n" );
		Warning( "\t3:\tFILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		Warning( "\t4:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		Warning( "\t5:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		Warning( "\t6:\tFILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		return;
	}

	int level = std::atoi( args[ 1 ] );
	switch( level )
	{
	case FILESYSTEM_WARNING_QUIET:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_QUIET\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUNCLOSED:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUSAGE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUSAGE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READ:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		break;

	default:
		Warning( "fs_warning_level = UNKNOWN!!!!!!!\n" );
		return;
		break;
	}
	g_pFileSystem->SetWarningLevel( ( FileWarningLevel_t )level );
}


//-----------------------------------------------------------------------------
// Purpose: Wrap Sys_LoadModule() with a filesystem GetLocalCopy() call to
//			ensure have the file to load.
//-----------------------------------------------------------------------------
CSysModule *FileSystem_LoadModule(const char *path)
{
	if ( g_pFileSystem )
		return g_pFileSystem->LoadModule( path );
	else
		return Sys_LoadModule(path);
}

//-----------------------------------------------------------------------------
// Purpose: Provided for symmetry sake with FileSystem_LoadModule()...
//-----------------------------------------------------------------------------
void FileSystem_UnloadModule(CSysModule *pModule)
{
	Sys_UnloadModule(pModule);
}


void FileSystem_SetWhitelistSpewFlags()
{
#if defined( _DEBUG )
	if ( !g_pFileSystem )
	{
		contract_assert( !"FileSystem_InitSpewFlags - no filesystem." );
		return;
	}
	
	g_pFileSystem->SetWhitelistSpewFlags( fs_whitelist_spew_flags.GetInt() );
#endif
}

static void fs_syncdvddevcache( const CCommand &args );
static ConCommand fs_syncdvddevcache_command( "fs_syncdvddevcache", fs_syncdvddevcache, "Force the 360 to get updated files that are in your p4 changelist(s) from the host PC when running with -dvddev." );
static void fs_syncdvddevcache( const CCommand &args )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->SyncDvdDevCache();
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Loads the optional addonlist.txt file which lives in the same location as gameinfo.txt and defines additional search
// paths for content add-ons to mods.
//---------------------------------------------------------------------------------------------------------------------
bool LoadAddonListFile( const char *pDirectoryName, KeyValues *&pAddons )
{
	char addoninfoFilename[MAX_PATH];

	V_snprintf( addoninfoFilename, sizeof( addoninfoFilename), "%s%s", pDirectoryName, ADDONLIST_FILENAME );
	pAddons = ReadKeyValuesFile( addoninfoFilename );

	return ( pAddons != nullptr );
}

//---------------------------------------------------------------------------------------------------------------------
// Reconciles the contents of the addonlist.txt file with the addon folders located under <MODPATH>/addons. If the 
// contains directory names that no longer exist they are removed. If there are directories present that are not in
// the file they are added. Added directories default to the disabled state in the file.
//---------------------------------------------------------------------------------------------------------------------
void ReconcileAddonListFile( IFileSystem *pFileSystem, const char *pModPath )
{
	KeyValues *pAddonList;

	// Load the existing addonlist.txt file 
	LoadAddonListFile( pModPath, pAddonList );

	// If there is no addonlist.txt then create an empty KeyValues
	if ( !pAddonList )
	{
		pAddonList = new KeyValues( "AddonList" );
	}

	// Get the list of subdirectories of addons
	char addonsWildcard[MAX_PATH];

	FileFindHandle_t findHandleDir;

	//
	// Loop through the .vpk files
	//
	std::vector< std::string > vecAddonVPKs;
	V_snprintf( addonsWildcard, sizeof( addonsWildcard ), "%s%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, "*.vpk" );
	const char *pFileName = pFileSystem->FindFirst( addonsWildcard, &findHandleDir );

	while ( pFileName )
	{	
		vecAddonVPKs.push_back( std::string( ks::nz( pFileName ) ) );
		pFileName = pFileSystem->FindNext( findHandleDir );
	}

	pFileSystem->FindClose( findHandleDir );

	//
	// Loop through the loose directories
	//
	std::vector< std::string > vecAddonDirs;
	V_snprintf( addonsWildcard, sizeof( addonsWildcard ), "%s%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, "*.*" );
	pFileName = pFileSystem->FindFirst( addonsWildcard, &findHandleDir );

	while ( pFileName )
	{	
		// We only want directories that is not already represented by a .vpk and that contains a valid addoninfo.txt
		if ( pFileSystem->FindIsDirectory( findHandleDir ) && ( pFileName[0] != '.' ) )
		{
			char szVPKized[MAX_PATH];

			V_snprintf( szVPKized, sizeof( szVPKized ), "%s.vpk", pFileName );

			if ( !ks::is_valid_index( vecAddonVPKs, ks::find_index( vecAddonVPKs, std::string( szVPKized ) ) ) )
			{
				char addonsInfoFile[MAX_PATH];
				FileFindHandle_t findHandleConfig;

				V_snprintf( addonsInfoFile, sizeof( addonsInfoFile ), "%s%s%c%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pFileName, CORRECT_PATH_SEPARATOR, "addoninfo.txt" );

				if ( pFileSystem->FindFirst( addonsInfoFile, &findHandleConfig ) )
				{
					vecAddonDirs.push_back( std::string( ks::nz( pFileName ) ) ); 
				}

				pFileSystem->FindClose( findHandleConfig );
			}
		}
		pFileName = pFileSystem->FindNext( findHandleDir );
	}

	pFileSystem->FindClose( findHandleDir );

	// Add missing, existing directories to the KeyValues
	for ( const auto &addonDir : vecAddonDirs )
	{
		// We found a directory that wasn't included in the file - add it
		if ( !pAddonList->FindKey( addonDir.c_str() ) )
		{
			pAddonList->SetInt( addonDir.c_str(), 1 );
		}
	}

	// Add missing, existing VPKs to the KeyValues
	for ( const auto &addonVPK : vecAddonVPKs )
	{
		// We found a VPK that wasn't included in the file - add it
		if ( !pAddonList->FindKey( addonVPK.c_str() ) )
		{
			pAddonList->SetInt( addonVPK.c_str(), 1 );
		}
	}

	// Remove any non-existent directories from the KeyValues
	KeyValues* pIter = pAddonList->GetFirstSubKey();
	std::vector< KeyValues* > vecDoomedSubkeys;

	while( pIter )
	{
		if ( !ks::is_valid_index( vecAddonDirs, ks::find_index( vecAddonDirs, std::string( pIter->GetName() ) ) ) && 
			!ks::is_valid_index( vecAddonVPKs, ks::find_index( vecAddonVPKs, std::string( pIter->GetName() ) ) ) )
		{
			vecDoomedSubkeys.push_back( pIter );
		}

		pIter = pIter->GetNextKey();
	} 

	// Now actually delete the missing directories
	for ( KeyValues *pDoomedSubkey : vecDoomedSubkeys )
	{
		pAddonList->RemoveSubKey( pDoomedSubkey );
		pDoomedSubkey->deleteThis();
	}

	// Persist and dispose
	char addoninfoFilename[MAX_PATH];

	V_snprintf( addoninfoFilename, sizeof( addoninfoFilename), "%s%s", pModPath, ADDONLIST_FILENAME );

	if ( pAddonList->GetFirstSubKey() )
	{
		pAddonList->SaveToFile( pFileSystem, addoninfoFilename );	
	}
	else
	{
		if ( pFileSystem->FileExists( addoninfoFilename ) )
		{
			pFileSystem->RemoveFile( addoninfoFilename );
		}
	}
	pAddonList->deleteThis();
}

//---------------------------------------------------------------------------------------------------------------------
// Adds enabled addons to the GAME search path after removing any existing addons from the GAME path.
//---------------------------------------------------------------------------------------------------------------------
void FileSystem_UpdateAddonSearchPaths( IFileSystem *pFileSystem )
{
	// Get the path to the mod dir
	char modPath[MAX_PATH];

	pFileSystem->GetSearchPath( "MOD", false, modPath, sizeof( modPath ) );

	//
	// Remove any existing addons from the search path
	//
	char gameSearchPath[10*MAX_PATH];
	char addonSearchString[MAX_PATH];
	std::vector< std::string > gameSearchPathList;

	// Construct the search string for determining whether the search path component is an add-on
	V_snprintf( addonSearchString, sizeof( addonSearchString ), "%s%s", modPath, ADDONS_DIRNAME );

	pFileSystem->GetSearchPath( "GAME", false, gameSearchPath, sizeof( gameSearchPath ) );
	V_SplitString(gameSearchPath, ";", gameSearchPathList );

	for ( const auto &searchPath : gameSearchPathList )
	{
		if ( V_stristr( searchPath.c_str(), addonSearchString ) )
		{
			pFileSystem->RemoveSearchPath( searchPath.c_str(), "GAME" );
		}
	}

	// Unmount any VPK addons
	std::vector< std::string > loadedVPKs;

	pFileSystem->GetVPKFileNames( loadedVPKs );

	for ( const auto &vpkName : loadedVPKs )
	{
		if ( V_stristr( vpkName.c_str(), addonSearchString ) )
		{
			pFileSystem->RemoveVPKFile( vpkName.c_str() );
		}
	}

	//
	// Reconcile the addons file and add any newly added ones to the list
	//
	ReconcileAddonListFile( pFileSystem, modPath );

	//
	// Add any enabled addons to the GAME search path
	//
	KeyValues *pAddonList;
	if ( LoadAddonListFile( modPath, pAddonList ) )
	{
		for ( KeyValues *pCur=pAddonList->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
		{
			const char *pszAddonName = pCur->GetName();
			const bool bAddonActivated = pCur->GetInt() != 0;

			if ( bAddonActivated )
			{
				char addOnPath[MAX_PATH];

				V_snprintf( addOnPath, sizeof( addOnPath ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pszAddonName );

				if ( V_stristr( pszAddonName, ".vpk" ) )
				{
					pFileSystem->AddVPKFile( addOnPath, PATH_ADD_TO_TAIL );
				}
				else
				{
					pFileSystem->AddSearchPath( addOnPath, "GAME", PATH_ADD_TO_TAIL );
				}
			}
		}

		pAddonList->deleteThis();
	}

	modelloader->Studio_ReloadModels( IModelLoader::RELOAD_EVERYTHING );
	materials->UncacheAllMaterials();
}
static void update_addon_paths( const CCommand &args );
static ConCommand update_addon_paths_command( "update_addon_paths", update_addon_paths, "Reloads the search paths for game addons." );
static void update_addon_paths( const CCommand &args )
{
	if( g_pFileSystem )
	{
		FileSystem_UpdateAddonSearchPaths( g_pFileSystem );
	}
}

static void unload_all_addons( const CCommand &args );
static ConCommand unload_all_addons_command( "unload_all_addons", unload_all_addons, "Reloads the search paths for game addons." );
static void unload_all_addons( const CCommand &args )
{
	//
	// Unmount any VPK addons
	//
	if( g_pFileSystem )
	{
		char addonSearchString[MAX_PATH];
		char modPath[MAX_PATH];
		std::vector< std::string > loadedVPKs;

		// Get the path to the mod dir
		g_pFileSystem->GetSearchPath( "MOD", false, modPath, sizeof( modPath ) );

		// Construct the search string for determining whether the search path component is an add-on
		V_snprintf( addonSearchString, sizeof( addonSearchString ), "%s%s", modPath, ADDONS_DIRNAME );

		g_pFileSystem->GetVPKFileNames( loadedVPKs );

		for ( const auto &vpkName : loadedVPKs )
		{
			if ( V_stristr( vpkName.c_str(), addonSearchString ) )
			{
				g_pFileSystem->RemoveVPKFile( vpkName.c_str() );
			}
		}
	}
}
