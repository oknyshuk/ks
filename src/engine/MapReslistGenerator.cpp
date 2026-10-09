// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "MapReslistGenerator.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "cmd.h"
#include "common.h"
#include "quakedef.h"
#include "vengineserver_impl.h"
import std;
#include "host.h"
#include "host_state.h"

constexpr auto PAUSE_FRAMES_BETWEEN_MAPS = 300;
constexpr auto PAUSE_TIME_BETWEEN_MAPS = 2.0f;

extern engineparms_t host_parms;

#define ENGINE_RESLIST_FILE "engine.lst"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MapReslistGenerator_Usage()
{ 
	Msg( "-makereslists usage:\n" );
	Msg( "  [ -makereslists <optionalscriptfile> ] -- script file to control more complex makereslists operations (multiple passes, etc.)\n" );
	Msg( "  [ -usereslistfile filename ] -- get map list from specified file, default is to build for maps/*.bsp\n" );
	Msg( "  [ -startmap mapname ] -- restart generation at specified map (after crash, implies resume)\n" );
	Msg( "  [ -condebug ] -- prepend console.log entries with mapname or engine if not in a map\n" );
	Msg( "  [ -reslistmap mapname ] -- generate reslists for specified map and exit after that map\n" );
	Msg( "  [ -rebuildaudio ] -- force rebuild of _other_rebuild.cache (metacache) file at exit\n" );
	Msg( "  [ -forever ] -- when you get to the end of the maplist, start over from the top\n" );
	Msg( "  [ -stringtables ] -- force rebuild of the .bsp's stringtable dictionary\n" );
	Msg( "  [ -reslistdir ] -- default is 'reslists', use this to override\n" );
	Msg( "  [ -startstage nnn ] -- when running from script file, this starts at specified stage\n" );
	Msg( "  [ -collate ] -- skip everything, just merge the reslist from temp folders to the final folder again\n" );
}

void MapReslistGenerator_Init()
{
	// check for reslist generation
	if ( CommandLine()->FindParm("-makereslists") )
	{
		bool usemaplistfile = false;
		if ( CommandLine()->FindParm("-usereslistfile") )
		{
			usemaplistfile = true;
		}
		MapReslistGenerator().EnableReslistGeneration( usemaplistfile );
	}
	else if ( CommandLine()->FindParm( "-rebuildaudio" ) )
	{
		MapReslistGenerator().SetAutoQuit( true );
	}

	if ( CommandLine()->FindParm( "-trackdeletions" ) )
	{
		MapReslistGenerator().EnableDeletionsTracking();
	}

	if ( CommandLine()->FindParm( "-autoquit" ) )
	{
		MapReslistGenerator().SetAutoQuit( true );
	}
}

void MapReslistGenerator_Shutdown()
{
	MapReslistGenerator().Shutdown();
}

void MapReslistGenerator_BuildMapList()
{
	MapReslistGenerator().BuildMapList();
}

CMapReslistGenerator g_MapReslistGenerator;
CMapReslistGenerator &MapReslistGenerator()
{
	return g_MapReslistGenerator;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMapReslistGenerator::CMapReslistGenerator() : 
	m_AlreadyWrittenFileNames( true ),
	m_bAutoQuit( false )
{

	m_bUsingMapList = false;
	m_bTrackingDeletions = false;
	m_bLoggingEnabled = false;
	m_iCurrentMap = 0;
	m_flNextMapRunTime = 0.0f;
	m_szPrefix[0] = '\0';
	m_szLevelName[0] = '\0';
	m_iPauseTimeBetweenMaps = PAUSE_TIME_BETWEEN_MAPS;
	m_bRestartOnTransition = false;
	m_bLogToEngineList = true;
	m_sResListDir = "reslists";
}

void CMapReslistGenerator::SetAutoQuit( bool bState )
{
	m_bAutoQuit = bState;
}

void CMapReslistGenerator::BuildMapList()
{
	if ( !IsEnabled() )
		return;

	if ( CommandLine()->FindParm( "+map" ) != 0 )
	{
		// This entire module is broken in about N different ways.
		// The scripting features have heavily rotted, plus tie in with stringtables (which gets confused with multiple restarts).
		// The whole thing barely works. I am narrowing it to work in exactly the one remaining way necesary for shipping.
		Error( "CMapReslistGenerator Incompatible with normal map loading. Use -reslistmap" );
	}

	Msg( "********************\n" );
	Msg( "Building Reslists\n" );
	Msg( "********************\n" );

	// Get the maplist file, if any
	const char *pMapFile = nullptr;
	CommandLine()->CheckParm( "-usereslistfile", &pMapFile );

	// -reslistmap argument precludes using a maplist file
	bool bUseMap = CommandLine()->FindParm( "-reslistmap" ) != 0;
	bool bUseMapListFile = bUseMap ? false : CommandLine()->FindParm("-usereslistfile") != 0;

	// Build the map list
	if ( !BuildGeneralMapList( &m_Maps, bUseMapListFile, pMapFile, "reslists", &m_iCurrentMap ) )
	{
		m_bLoggingEnabled = false;
	}
}

bool BuildGeneralMapList( std::vector< maplist_map_t > *aMaps, bool bUseMapListFile, const char *pMapFile, const char *pSystemMsg, int *iCurrentMap )
{
	if ( !bUseMapListFile )
	{
		// If the user passed in a -reslistmap parameter, just use that single map
		char const *pMapName = nullptr;
		if ( CommandLine()->CheckParm( "-reslistmap", &pMapName ) && pMapName )
		{
			// ensure validity
			if (g_pVEngineServer->IsMapValid(pMapName))
			{
				// add to list
				maplist_map_t newMap;
				V_strncpy(newMap.name, pMapName, sizeof(newMap.name));
				aMaps->push_back( newMap );
			}

			CommandLine()->RemoveParm( "-reslistmap" );
		}
		else
		{
			// build the list of all the levels to scan
			// Search the directory structure.
			const char *mapwild = "maps/*.bsp";
			char const *findfn = Sys_FindFirst( mapwild, nullptr, 0 );
			while ( findfn )
			{
				// make sure that it's in the mod filesystem
				if ( !g_pFileSystem->FileExists( va("maps/%s", findfn), "MOD" ) )
				{
					findfn = Sys_FindNext( nullptr, 0 );
					continue;
				}

				// strip extension
				char sz[ MAX_PATH ];
				V_strncpy( sz, findfn, sizeof( sz ) );
				char *ext = std::strchr( sz, '.' );
				if (ext)
				{
					ext[0] = 0;
				}

				// move to next item
				findfn = Sys_FindNext( nullptr, 0  );

				// ensure validity
				if (!g_pVEngineServer->IsMapValid(sz))
					continue;

				// add to list
				maplist_map_t newMap;
				V_strncpy(newMap.name, sz, sizeof(newMap.name));
				aMaps->push_back( newMap );
			}

			Sys_FindClose();
		}
	}
	else
	{
		// Read from file
		if ( pMapFile )
		{
			// Load them in
			FileHandle_t resfilehandle;
			resfilehandle = g_pFileSystem->Open( pMapFile, "rb" );
			if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
			{
				// Read in and parse mapcycle.txt
				int length = g_pFileSystem->Size(resfilehandle);
				if ( length > 0 )
				{
					char *pStart = (char *)new char[ length + 1 ];
					if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) ) )
					{
						pStart[ length ] = 0;
						const char *pFileList = pStart;

						while ( 1 )
						{
							char szMap[ MAX_OSPATH ];

							pFileList = COM_Parse( pFileList );
							if ( std::strlen( com_token ) <= 0 )
								break;

							V_strncpy(szMap, com_token, sizeof(szMap));

							// ensure validity
							if (!g_pVEngineServer->IsMapValid(szMap))
								continue;

							// Any more tokens on this line?
							while ( COM_TokenWaiting( pFileList ) )
							{
								pFileList = COM_Parse( pFileList );
							}

							maplist_map_t newMap;
							V_strncpy(newMap.name, szMap, sizeof(newMap.name));
							aMaps->push_back( newMap );
						}
					}
					delete[] pStart;
				}

				g_pFileSystem->Close(resfilehandle);
			}
			else
			{
				Error( "Unable to load %s maplist file: %s\n", pSystemMsg, pMapFile );
				return false;
			}

		}
		else
		{
			Error( "Unable to find %s maplist filename\n", pSystemMsg );
			return false;
		}
	}

	int c = std::ssize( *aMaps );
	if ( c == 0 )
	{
		Msg( "%s: No maps found\n", pSystemMsg );
		return false;
	}

	Msg( "%s: Creating for:\n", pSystemMsg );

	// Determine the current map (-startmap allows starts mid-maplist)
	*iCurrentMap = 0;
	char const *startmap = nullptr;
	if ( CommandLine()->CheckParm( "-startmap", &startmap ) && startmap )
	{
		for ( int i = 0 ; i < c; ++i )
		{
			if ( !V_stricmp( (*aMaps)[ i ].name, startmap ) )
			{
				*iCurrentMap = i;
			}
		}
	}

	for ( int i = 0 ; i < c; ++i )
	{
		if ( i < *iCurrentMap )
		{
			Msg( "-  %s\n", (*aMaps)[ i ].name );
		}
		else
		{
			Msg( "+  %s\n", (*aMaps)[ i ].name );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reconstructs engine log dictionary from existing engine reslist.
// This is used to restore state after a restart, otherwise the engine log
// would aggregate duplicate files.
//-----------------------------------------------------------------------------
void CMapReslistGenerator::BuildEngineLogFromReslist()
{
	m_EngineLog.clear();

	auto data = g_pFileSystem->ReadFile( CFmtStr( "%s\\%s", m_sResListDir.c_str(), ENGINE_RESLIST_FILE ), "DEFAULT_WRITE_PATH" );
	if ( !data )
	{
		// does not exist
		return;
	}
	ks::text_reader buffer( ks::text_view( *data ) );

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// parse reslist
	for ( ;; )
	{
		auto token = buffer.read_token( breakSet );
		if ( !token || token->empty() )
		{
			break;
		}

		m_EngineLog.emplace( *token );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends specified line to the engine reslist.
//-----------------------------------------------------------------------------
void CMapReslistGenerator::LogToEngineReslist( char const *pLine )
{
	// prevent unecessary duplication due to file appending
	if ( !m_EngineLog.insert( ks::nz( pLine ) ).second )
	{
		// already logged
		return;
	}

	// Open for append, write data, close.
	FileHandle_t fh = g_pFileSystem->Open( CFmtStr( "%s\\%s", m_sResListDir.c_str(), ENGINE_RESLIST_FILE ), "at", "DEFAULT_WRITE_PATH" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Write( "\"", 1, fh );
		g_pFileSystem->Write( pLine, V_strlen( pLine  ), fh );
		g_pFileSystem->Write( "\"\n", 2, fh );
		g_pFileSystem->Close( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: initializes the object to enable reslist generation
//-----------------------------------------------------------------------------
void CMapReslistGenerator::EnableReslistGeneration( bool usemaplistfile )
{
	// hackhack !!!! This is a work-around until CS precaches things on level start, not player spawn 
	if ( !V_stricmp( "cstrike", GetCurrentMod() ) || !V_stricmp( "csgo", GetCurrentMod() ) )
	{
		// the CS UI basically broke this, all sorts of waiting for team selection player input in UI
		// 5 for the loading map screen, 10 for team selection, 15 for loading in general.
		m_iPauseTimeBetweenMaps = 5 + 10 + 15;
	}

	m_bUsingMapList = usemaplistfile;

	m_bLoggingEnabled = true;

	char const *pszDir = nullptr;
	if ( CommandLine()->CheckParm( "-reslistdir", &pszDir ) && pszDir )
	{
		char szDir[ MAX_PATH ];
		V_strncpy( szDir, pszDir, sizeof( szDir ) );
		V_StripTrailingSlash( szDir );
		V_strlower( szDir );
		V_FixSlashes( szDir );

		if ( V_strlen( szDir ) > 0 )
		{
			m_sResListDir = szDir;
		}
	}


	// create file to dump out to
	g_pFileSystem->CreateDirHierarchy( m_sResListDir.c_str() , "DEFAULT_WRITE_PATH" );

	// Leave the existing one if resuming from a specific map, otherwise, blow it away
	if ( !CommandLine()->FindParm( "-startmap" ) )
	{
		g_pFileSystem->RemoveFile( CFmtStr( "%s\\%s", m_sResListDir.c_str(), ENGINE_RESLIST_FILE ), "DEFAULT_WRITE_PATH" );
		m_EngineLog.clear();
	}
	else
	{
		BuildEngineLogFromReslist();
	}

	// add logging function
	g_pFileSystem->AddLoggingFunc(&FileSystemLoggingFunc);
}

//-----------------------------------------------------------------------------
// Purpose: starts the first map
//-----------------------------------------------------------------------------
void CMapReslistGenerator::StartReslistGeneration()
{
	// wait for the main menu to stabilize then start the first map loading
	m_iCurrentMap = 0;
	m_flNextMapRunTime = Plat_FloatTime() + 10;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mapname - 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::SetPrefix( char const *mapname )
{
	V_snprintf( m_szPrefix, sizeof( m_szPrefix ), "%s:  ", mapname );
}

void CMapReslistGenerator::OnLevelShutdown()
{
	m_bLogToEngineList = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CMapReslistGenerator::LogPrefix()
{
	// If not recording stuff to file, then use the "default" prefix.
	if ( m_bLogToEngineList )
	{
		return "engine:  ";
	}

	return m_szPrefix;
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnLevelLoadStart(const char *levelName)
{
	// prepare for map logging
	m_bLogToEngineList = false;
	V_strncpy( m_szLevelName, levelName, sizeof( m_szLevelName ) );

	if ( !IsEnabled() )
	{
		char basename[ MAX_PATH ];
		V_FileBase( levelName, basename, sizeof( basename ) );
		V_strlower( basename );
		SetPrefix( basename );
		return;
	}

	// reset the duplication list
	m_AlreadyWrittenFileNames.clear();

	m_MapLog.clear();

	// add in the bsp file to the list, and its node graph
	char path[MAX_PATH];
	V_snprintf( path, sizeof( path ), "maps\\%s.bsp", levelName );
	OnResourcePrecached( path );

	bool useNodeGraph = true;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		useNodeGraph = modinfo->GetInt( "nodegraph", 1 ) != 0;
	}
	modinfo->deleteThis();

	if ( useNodeGraph )
	{
		V_snprintf(path, sizeof(path), "maps\\graphs\\%s.ain", levelName);
		OnResourcePrecached(path);
	}
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnLevelLoadEnd()
{
}

void CMapReslistGenerator::OnPlayerSpawn()
{
}

void CMapReslistGenerator::OnFullyConnected()
{
	if ( !IsEnabled() )
		return;

	// initiate the next level
	m_flNextMapRunTime = Plat_FloatTime() + m_iPauseTimeBetweenMaps;
}

bool CMapReslistGenerator::ShouldRebuildCaches()
{
	if ( !IsEnabled() )
	{
		return CommandLine()->FindParm( "-rebuildaudio" ) != 0;
	}

	if ( !CommandLine()->FindParm( "-norebuildaudio" ) )
		return true;
	return false;
}

char const *CMapReslistGenerator::GetResListDirectory() const
{
	return m_sResListDir.c_str();
}

void CMapReslistGenerator::DoQuit()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
	// remove the logging
	g_pFileSystem->RemoveLoggingFunc(&FileSystemLoggingFunc);
	m_bLogToEngineList = true;
}

//-----------------------------------------------------------------------------
// Purpose: call every frame if we're enabled, just so that the next map can be triggered at the right time
//-----------------------------------------------------------------------------
void CMapReslistGenerator::RunFrame()
{
	if ( !IsEnabled() )
	{
		if ( m_bAutoQuit )
		{
			m_bAutoQuit = false;
			DoQuit();
		}
		return;
	}

	if ( m_flNextMapRunTime && m_flNextMapRunTime < Plat_FloatTime() )
	{
		// about to transition or terminate, emit the current map log
		WriteMapLog();

		if ( ks::is_valid_index( m_Maps, m_iCurrentMap ) )
		{
			// will start counting again after the level loads
			m_flNextMapRunTime = 0.0f;

			if ( !m_bRestartOnTransition )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "map %s\n", m_Maps[m_iCurrentMap].name ) );

				SetPrefix( m_Maps[m_iCurrentMap].name );

				++m_iCurrentMap;
				if ( ks::is_valid_index( m_Maps, m_iCurrentMap ) )
				{
					// cause a full engine restart on the transition to the next map
					// ensure that one-time init code logs correctly to each map reslist
					m_bRestartOnTransition = true;
				}
			}
			else
			{
				// restart at specified map
				CommandLine()->RemoveParm( "-startmap" );
				CommandLine()->AppendParm( "-startmap", m_Maps[m_iCurrentMap].name );	
				HostState_Restart();
			}
		}
		else
		{
			// no more levels, just quit
			if ( !CommandLine()->FindParm( "-forever" ) )
			{
				DoQuit();
			}
			else
			{
				StartReslistGeneration();
				m_bRestartOnTransition = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs and handles mdl files being precaches
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnModelPrecached(const char *relativePathFileName)
{
	if ( !IsEnabled() )
		return;

	if (std::strstr(relativePathFileName, ".vmt"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		char file[k_cMaxPath];

		if ( StringHasPrefix( relativePathFileName, "materials" ) )
		{
			V_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			V_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}
		OnResourcePrecached(file);

		// get the matching vtf file
		char *ext = strstr(file, ".vmt");
		if (ext)
		{
			V_strncpy(ext, ".vtf", 5);
			OnResourcePrecached(file);
		}
	}
	else
	{
		OnResourcePrecached(relativePathFileName);
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs sound file access
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnSoundPrecached(const char *relativePathFileName)
{
	// skip any special characters
	if (!V_isalnum(relativePathFileName[0]))
	{
		++relativePathFileName;
	}

	// prepend the sound/ directory if necessary
	char file[k_cMaxPath];
	if ( StringHasPrefix( relativePathFileName, "sound" ) )
	{
		V_strncpy(file, relativePathFileName, sizeof(file));
	}
	else
	{
		// prepend the sound directory
		V_snprintf(file, sizeof(file), "sound\\%s", relativePathFileName);
	}

	OnResourcePrecached(file);
}

//-----------------------------------------------------------------------------
// Purpose: logs the precache as a file access
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnResourcePrecached(const char *relativePathFileName)
{
	if ( !IsEnabled() )
		return;

	// ignore empty string
	if (relativePathFileName[0] == 0)
		return;

	// ignore files that start with '*' since they signify special models
	if (relativePathFileName[0] == '*')
		return;

	char fullPath[k_cMaxPath];
	if (g_pFileSystem->GetLocalPath(relativePathFileName, fullPath, sizeof(fullPath)))
	{
		OnResourcePrecachedFullPath(fullPath);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Logs out file access to a file
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnResourcePrecachedFullPath(const char *fullPathFileName)
{
	char fixed[ MAX_PATH ];
	V_strncpy( fixed, fullPathFileName, sizeof( fixed ) );
	V_strlower( fixed );
	V_FixSlashes( fixed );

	// make sure the filename hasn't already been written
	if ( m_AlreadyWrittenFileNames.contains( fixed ) )
		return;

	// record in list, so we don't write it again
	m_AlreadyWrittenFileNames.intern( fixed );

	// add extras for mdl's
	if (std::strstr(fixed, ".mdl"))
	{
		// it's a model, get it's other files as well
		char file[k_cMaxPath];
		V_strncpy(file, fixed, sizeof(file) - 10);
		char *ext = strstr(file, ".mdl");

		V_strncpy(ext, ".vvd", 10);
		OnResourcePrecachedFullPath(file);
		V_strncpy(ext, ".ani", 10);
		OnResourcePrecachedFullPath(file);
		V_strncpy(ext, ".dx90.vtx", 10);
		OnResourcePrecachedFullPath(file);
		V_strncpy(ext, ".phy", 10);
		OnResourcePrecachedFullPath(file);
		V_strncpy(ext, ".jpg", 10);
		OnResourcePrecachedFullPath(file);
	}

	// strip it down relative to the root directory of the game
	char const *relativeFileName = V_stristr( fixed, GetBaseDirectory() );
	if ( relativeFileName )
	{
		// Skip the basedir and slash
		relativeFileName += ( V_strlen( GetBaseDirectory() ) + 1 );
	}

	if ( !relativeFileName )
	{
		return;
	}

	if ( m_bLogToEngineList )
	{
		LogToEngineReslist( relativeFileName );
	}
	else
	{
		// find or add to sorted tree
		m_MapLog.insert( ks::nz( relativeFileName ) );
	}
}

void CMapReslistGenerator::WriteMapLog()
{
	if ( !m_szLevelName[0] )
	{
		// log has not been established yet
		return;
	}

	// write the sorted map log, allows for easier diffs between revisions
	char path[k_cMaxPath];
	V_snprintf( path, sizeof( path ), "%s\\%s.lst", m_sResListDir.c_str(), m_szLevelName );
	FileHandle_t fh = g_pFileSystem->Open( path, "wt", "DEFAULT_WRITE_PATH" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		for ( const std::string &line : m_MapLog )
		{
			const char *pLine = line.c_str();
			g_pFileSystem->Write( "\"", 1, fh );
			g_pFileSystem->Write( pLine, V_strlen( pLine ), fh );
			g_pFileSystem->Write( "\"\n", 2, fh );
		}
		g_pFileSystem->Close( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CMapReslistGenerator::FileSystemLoggingFunc(const char *fullPathFileName, const char *options)
{
	g_MapReslistGenerator.OnResourcePrecachedFullPath(fullPathFileName);
}

inline constexpr const char *DELETIONS_BATCH_FILE = "deletions.bat";
inline constexpr const char *DELETIONS_WARNINGS_FILE = "undelete.lst";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::EnableDeletionsTracking()
{
	unsigned int deletions = 0;
	unsigned int warnings = 0;

	// Load deletions file and build dictionary
	m_bTrackingDeletions =  true;;

	// Open up deletions.bat and parse out all filenames
	// Load them in
	FileHandle_t deletionsfile;
	deletionsfile = g_pFileSystem->Open( DELETIONS_BATCH_FILE, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != deletionsfile )
	{
		// Read in and parse mapcycle.txt
		int length = g_pFileSystem->Size(deletionsfile);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, deletionsfile) ) )
			{
				pStart[ length ] = 0;
				const char *pFileList = pStart;

				while ( 1 )
				{
					char filename[ MAX_OSPATH ];

					pFileList = COM_Parse( pFileList );
					if ( std::strlen( com_token ) <= 0 )
						break;

					if ( !V_stricmp( com_token, "del" ) )
						continue;

					V_snprintf(filename, sizeof( filename ), "%s/%s", com_gamedir, com_token );

					// Any more tokens on this line?
					while ( COM_TokenWaiting( pFileList ) )
					{
						pFileList = COM_Parse( pFileList );
					}

					V_FixSlashes( filename );
					V_strlower( filename );
	
					m_DeletionList.intern( filename );

					++deletions;
				}
			}
			delete[] pStart;
		}

		g_pFileSystem->Close(deletionsfile);
	}
	else
	{
		Warning( "Unable to load deletions.bat file %s\n", DELETIONS_BATCH_FILE );
		m_bTrackingDeletions = false;
		return;
	}

	FileHandle_t warningsfile = g_pFileSystem->Open( DELETIONS_WARNINGS_FILE, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != warningsfile )
	{
		// Read in and parse mapcycle.txt
		int length = g_pFileSystem->Size(warningsfile);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, warningsfile) ) )
			{
				pStart[ length ] = 0;
				const char *pFileList = pStart;

				while ( 1 )
				{
					pFileList = COM_Parse( pFileList );
					if ( std::strlen( com_token ) <= 0 )
						break;

					V_FixSlashes( com_token );
					V_strlower( com_token );
	
					if ( !m_DeletionListWarningsSymbols.contains( com_token ) )
					{
						m_DeletionListWarnings.push_back( m_DeletionListWarningsSymbols.intern( com_token ) );
						++warnings;
					}
				}
			}
			delete[] pStart;
		}

		g_pFileSystem->Close(warningsfile);
	}
	
	// Hook up logging function
	g_pFileSystem->AddLoggingFunc( &TrackDeletionsLoggingFunc );

	Msg( "Tracking deletions (%u files in deletion list in '%s', %u previous warnings loaded from '%s'\n",
		deletions, DELETIONS_BATCH_FILE, warnings, DELETIONS_WARNINGS_FILE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fullPathFileName - 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::TrackDeletions( const char *fullPathFileName )
{

	char test[ k_cMaxPath ];
	V_strncpy( test, fullPathFileName, sizeof( test ) );
	V_FixSlashes( test );
	V_strlower( test );

	if ( m_DeletionList.contains( test ) )
	{
		if ( !m_DeletionListWarningsSymbols.contains( test ) )
		{
			m_DeletionListWarnings.push_back( m_DeletionListWarningsSymbols.intern( test ) );
			Msg( "--> Referenced file marked for deletion \"%s\"\n", test );
		}
	}

	// add extras for mdl's
	if (std::strstr(test, ".mdl"))
	{
		// it's a model, get it's other files as well
		char file[k_cMaxPath];
		V_strncpy(file, test, sizeof(file) - 10);
		char *ext = strstr(file, ".mdl");

		V_strncpy(ext, ".vvd", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".ani", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".dx80.vtx", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".dx90.vtx", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".sw.vtx", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".phy", 10);
		TrackDeletions(file);
		V_strncpy(ext, ".jpg", 10);
		TrackDeletions(file);
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CMapReslistGenerator::TrackDeletionsLoggingFunc(const char *fullPathFileName, const char *options)
{
	g_MapReslistGenerator.TrackDeletions(fullPathFileName);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::Shutdown()
{
	if ( m_bTrackingDeletions )
	{
		SpewTrackedDeletionsLog();

		g_pFileSystem->RemoveLoggingFunc( &TrackDeletionsLoggingFunc );
		m_DeletionList.clear();
		m_DeletionListWarnings.clear();
		m_DeletionListWarningsSymbols.clear();
		m_bTrackingDeletions = false;
	}
}

void CMapReslistGenerator::SpewTrackedDeletionsLog()
{
	if ( !m_bTrackingDeletions )
		return;

	FileHandle_t hUndeleteFile = g_pFileSystem->Open( DELETIONS_WARNINGS_FILE, "wt", "DEFAULT_WRITE_PATH" );
	if ( FILESYSTEM_INVALID_HANDLE == hUndeleteFile )
	{
		return;
	}

	for ( std::string_view filename : m_DeletionListWarnings )
	{
		g_pFileSystem->Write("\"", 1, hUndeleteFile);
		g_pFileSystem->Write(filename.data(), filename.size(), hUndeleteFile);
		g_pFileSystem->Write("\"\n", 2, hUndeleteFile);
	}

	g_pFileSystem->Close( hUndeleteFile );
}
