// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Defines the entry point for the application.

import ks.tier1;
#include "reslistgenerator.h"
#include "filesystem.h"
import std;

bool SaveResList( const std::set< std::string, ks::iless > &list, char const *pchFileName, char const *pchSearchPath )
{
	FileHandle_t fh = g_pFullFileSystem->Open( pchFileName, "wt", pchSearchPath );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		for ( const std::string &s : list )
		{
			g_pFullFileSystem->Write( s.c_str(), V_strlen( s.c_str() ), fh );
			g_pFullFileSystem->Write( "\n", 1, fh );
		}

		g_pFullFileSystem->Close( fh );
		return true;
	}
	return false;
}

void LoadResList( std::set< std::string, ks::iless > &list, char const *pchFileName, char const *pchSearchPath )
{
	auto file = g_pFullFileSystem->ReadFile( pchFileName, pchSearchPath );
	if ( !file )
	{
		// does not exist
		return;
	}
	ks::text_reader buffer( ks::text_view( *file ) );

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// parse reslist
	char szToken[ MAX_PATH ];
	for ( ;; )
	{
		auto token = buffer.read_token( breakSet );
		if ( !token || token->empty() )
		{
			break;
		}
		const std::size_t nTokenSize = std::min( token->size(), sizeof( szToken ) - 1 );
		std::memcpy( szToken, token->data(), nTokenSize );
		szToken[ nTokenSize ] = 0;

		V_strlower( szToken );
		V_FixSlashes( szToken );

		// Ensure filename has "quotes" around it
		std::string s;
		if ( szToken[ 0 ] == '\"' )
		{
			contract_assert( V_strlen( szToken ) > 2 );
			contract_assert( szToken[ V_strlen( szToken ) - 1 ] == '\"' );
			s = szToken;
		}
		else
		{
			s = CFmtStr( "\"%s\"", szToken );
		}

		list.insert( std::move( s ) );
	}
}

void SortResList( char const *pchFileName, char const *pchSearchPath )
{
	std::set< std::string, ks::iless > sorted;
	LoadResList( sorted, pchFileName, pchSearchPath );

	// Now write it back out
	SaveResList( sorted, pchFileName, pchSearchPath );
}

void MergeResLists( std::vector< std::string > &fileNames, char const *pchOutputFile, char const *pchSearchPath )
{
	std::set< std::string, ks::iless > sorted;
	for ( int i = 0; i < std::ssize( fileNames ); ++i )
	{
		LoadResList( sorted, fileNames[ i ].c_str(), pchSearchPath );
	}

	// Now write it back out
	SaveResList( sorted, pchOutputFile, pchSearchPath );
}

class CWorkItem
{
public:
	CWorkItem()
	{
	}

	std::string		m_sSubDir;
	std::string		m_sAddCommands;
};
class CResListGenerator: public IResListGenerator
{
public:
	enum
	{
		STATE_SETUP = 0,
		STATE_BUILDINGRESLISTS,
		STATE_GENERATINGCACHES,
		STATE_MAXSTATES
	};

	CResListGenerator();

	virtual void Init( char const *pchBaseDir, char const *pchGameDir );
	virtual bool IsActive();
	virtual void Shutdown();
	virtual bool TickAndFixupCommandLine();
	virtual bool ShouldContinue();

private:

	bool InitCommandFile( char const *pchGameDir, char const *pchCommandFile );
	void LoadMapList( char const *pchGameDir, std::vector< std::string > &vecMaps, char const *pchMapFile );
	void CollateFiles( char const *pchResListFilename );
	void Collate();

	bool		m_bInitialized;
	bool		m_bActive;

	std::string	m_sBaseDir;
	std::string  m_sGameDir;
	std::string	m_sFullGamePath;

	std::string	m_sFinalDir;
	std::string	m_sWorkingDir;
	std::string	m_sBaseCommandLine;
	std::string	m_sOriginalCommandLine;
	std::string	m_sInitialStartMap;

	int			m_nCurrentWorkItem;
	std::vector< CWorkItem >	m_WorkItems;

	std::vector< std::string > m_MapList;
	int			m_nCurrentState;
};

static CResListGenerator g_ResListGenerator;
IResListGenerator *reslistgenerator = &g_ResListGenerator;

CResListGenerator::CResListGenerator() :
	m_bInitialized( false ),
	m_bActive( false ),
	m_nCurrentWorkItem( 0 ),
	m_nCurrentState( STATE_SETUP )
{

	m_sFinalDir = "reslists";
	m_sWorkingDir = "reslists_work";
}

void CResListGenerator::CollateFiles( char const *pchResListFilename )
{
	std::vector< std::string > vecReslists;

	for ( int i = 0; i < std::ssize( m_WorkItems ); ++i )
	{
		char fn[ MAX_PATH ];
		V_snprintf( fn, sizeof( fn ), "%s\\%s\\%s\\%s", m_sFullGamePath.c_str(), m_sWorkingDir.c_str(), m_WorkItems[ i ].m_sSubDir.c_str(), pchResListFilename );
		vecReslists.push_back( fn );
	}

	MergeResLists( vecReslists, CFmtStr( "%s\\%s\\%s", m_sFullGamePath.c_str(), m_sFinalDir.c_str(), pchResListFilename ), "GAME" );
}

void CResListGenerator::Init( char const *pchBaseDir, char const *pchGameDir )
{

	// Because we have to call this inside the first Apps "PreInit", we need only Init on the very first call
	if ( m_bInitialized )
	{
		return;
	}

	m_bInitialized = true;

	m_sBaseDir = ks::nz( pchBaseDir );
	m_sGameDir = ks::nz( pchGameDir );

	char path[MAX_PATH];
	V_snprintf( path, sizeof(path), "%s/%s", m_sBaseDir.c_str(), m_sGameDir.c_str() );
	V_FixSlashes( path );
	V_strlower( path );
	m_sFullGamePath = path;

	const char *pchCommandFile = nullptr;
	if ( CommandLine()->CheckParm( "-makereslists", &pchCommandFile ) && pchCommandFile )
	{
		// base path setup, now can get and parse command file
		// one time setup ONLY
		InitCommandFile( path, pchCommandFile );
	}
}

void CResListGenerator::Shutdown()
{
	if ( !m_bActive )
		return;
}

bool CResListGenerator::IsActive()
{
	return m_bInitialized && m_bActive;
}

void CResListGenerator::Collate()
{
	char szDir[MAX_PATH];
	V_snprintf( szDir, sizeof( szDir ), "%s\\%s", m_sFullGamePath.c_str(), m_sFinalDir.c_str() );
	g_pFullFileSystem->CreateDirHierarchy( szDir, "GAME" );

	// Now create the collated/merged data
	CollateFiles( "all.lst" );
	CollateFiles( "engine.lst" );
	for ( int i = 0 ; i < std::ssize( m_MapList ); ++i )
	{
		CollateFiles( CFmtStr( "%s.lst", m_MapList[ i ].c_str() ) );
	}
}

//-----------------------------------------------------------------------------
// Called at each restart invocation, clocks the state.
// Returns true if caller should proceed with command line, false otherwise.
// false is used to stop the reslist process which requires additional post passes.
//-----------------------------------------------------------------------------
bool CResListGenerator::TickAndFixupCommandLine()
{
	if ( !m_bActive )
	{
		return true;
	}

	// clock the state
	switch ( m_nCurrentState )
	{
	default:
		m_bActive = false;
		break;

	case STATE_SETUP:
		// first time
		m_nCurrentState = STATE_BUILDINGRESLISTS;
		break;

	case STATE_BUILDINGRESLISTS:
		{ 
			CommandLine()->RemoveParm( "-startmap" );

			// Advance to next item
			++m_nCurrentWorkItem;
			if ( m_nCurrentWorkItem >= std::ssize( m_WorkItems ) )
			{
				// out of work, finalize
				Collate();

				// advance to next state
				++m_nCurrentState;
			}
		}
		break;

	case STATE_GENERATINGCACHES:
		++m_nCurrentState;
		break;
	}

	switch ( m_nCurrentState )
	{
	default:
		m_bActive = false;
		break;

	case STATE_BUILDINGRESLISTS:
		{
			contract_assert( m_nCurrentWorkItem < std::ssize( m_WorkItems ) );

			const CWorkItem &work = m_WorkItems[ m_nCurrentWorkItem ];

			// Clean the working dir
			char szWorkingDir[ 512 ];
			V_snprintf( szWorkingDir, sizeof( szWorkingDir ), "%s\\%s", m_sWorkingDir.c_str(), work.m_sSubDir.c_str() );

			char szFullWorkingDir[MAX_PATH];
			V_snprintf( szFullWorkingDir, sizeof( szFullWorkingDir ), "%s\\%s", m_sFullGamePath.c_str(), szWorkingDir );
			g_pFullFileSystem->CreateDirHierarchy( szFullWorkingDir, "GAME" );

			// Preserve startmap
			char const *pszStartMap = nullptr;
			CommandLine()->CheckParm( "-startmap", &pszStartMap );
			char szMap[ MAX_PATH ] = { 0 };
			if ( pszStartMap )
			{
				V_strncpy( szMap, pszStartMap, sizeof( szMap ) );
			}

			// Prepare stuff
			// Reset command line based on current state
			char szCmd[ 512 ];
			V_snprintf( szCmd, sizeof( szCmd ), "%s %s %s -reslistdir %s", m_sOriginalCommandLine.c_str(), m_sBaseCommandLine.c_str(), work.m_sAddCommands.c_str(), szWorkingDir );
			CommandLine()->CreateCmdLine( szCmd );

			// Never rebuild caches by default, inly do it in STATE_GENERATINGCACHES
			CommandLine()->AppendParm( "-norebuildaudio", nullptr );
			if ( szMap[ 0 ] )
			{
				CommandLine()->AppendParm( "-startmap", szMap );
			}

			Warning( "Generating Reslists: Setting command line:\n'%s'\n", CommandLine()->GetCmdLine() );
		}
		break;

	case STATE_GENERATINGCACHES:
		{
			// Prepare stuff
			// Reset command line based on current state
			char szCmd[ 512 ];
			V_snprintf( szCmd, sizeof( szCmd ), "%s -reslistdir %s -rebuildaudio", m_sOriginalCommandLine.c_str(), m_sFinalDir.c_str());
			CommandLine()->CreateCmdLine( szCmd );
			
			CommandLine()->RemoveParm( "-norebuildaudio" );
			CommandLine()->RemoveParm( "-makereslists" );

			Warning( "Generating Caches: Setting command line:\n'%s'\n", CommandLine()->GetCmdLine() );
		}
		break;
	}

	if ( !m_bActive )
	{
		// no further processing required, make the engine shut down cleanly in this pass
		CommandLine()->RemoveParm( "-makereslists" );
		CommandLine()->AppendParm( "-autoquit", nullptr );
	}

	// continue
	return m_bActive;
}

bool CResListGenerator::ShouldContinue()
{
	// some states require post processing
	// let the state processing determine the final quit state
	if ( !m_bActive || m_nCurrentState >= STATE_MAXSTATES )
	{
		return false;
	}

	return true;
}

void CResListGenerator::LoadMapList( char const *pchGameDir, std::vector< std::string > &vecMaps, char const *pchMapFile )
{
	char fullpath[ 512 ];
	V_snprintf( fullpath, sizeof( fullpath ), "%s/%s", pchGameDir, pchMapFile );

	// Load them in
	auto file = g_pFullFileSystem->ReadFile( fullpath, "GAME" );

	if ( file )
	{
		ks::text_reader buf( ks::text_view( *file ) );
		char szMap[ MAX_PATH ];
		while ( true )
		{
			const std::string_view line = buf.read_line().substr( 0, sizeof( szMap ) - 1 );
			if ( line.empty() )
				break;
			std::memcpy( szMap, line.data(), line.size() );
			szMap[ line.size() ] = 0;

			// Strip trailing CR/LF chars
			int len = V_strlen( szMap );
			while ( len >= 1 && ( ( szMap[ len - 1 ] == '\n' ) || ( szMap[ len - 1 ] == '\r' ) ) )
			{
				szMap[ len - 1 ] = 0;
				len = V_strlen( szMap );
			}

			std::string newMap;
			newMap = szMap;
			vecMaps.push_back( newMap );
		}
	}
	else
	{
		Error( "Unable to maplist file %s\n", fullpath );
	}
}

bool CResListGenerator::InitCommandFile( char const *pchGameDir, char const *pchCommandFile )
{
	if ( *pchCommandFile == '+' ||
		 *pchCommandFile == '-' )
	{
		Msg( "CResListGenerator: Falling back to legacy reslists system.\n" );
		return false;
	}

	char fullpath[ 512 ];
	V_snprintf( fullpath, sizeof( fullpath ), "%s/%s", pchGameDir, pchCommandFile );

	auto file = g_pFullFileSystem->ReadFile( fullpath, "GAME" );
	if ( !file )
	{
		Error( "Unable to load '%s'\n", fullpath );
		return false;
	}

	KeyValues *kv = new KeyValues( "reslists" );
	if ( !kv->LoadFromBuffer( "reslists", ks::text_view( *file ) ) )
	{
		Error( "Unable to parse keyvalues from '%s'\n", fullpath );
		kv->deleteThis();
		return false;
	}

	std::string sMapListFile = kv->GetString( "maplist", "maplist.txt" );
	LoadMapList( pchGameDir, m_MapList, sMapListFile.c_str() );
	if ( std::ssize( m_MapList ) <= 0 )
	{
		Error( "Maplist file '%s' empty or missing!!!\n", sMapListFile.c_str() );
		kv->deleteThis();
		return false;
	}

	char const *pszSolo = nullptr;
	if ( CommandLine()->CheckParm( "-reslistmap", &pszSolo ) && pszSolo )
	{
		m_MapList.clear(); m_MapList.shrink_to_fit();

		std::string newMap;
		newMap = ks::nz( pszSolo );
		m_MapList.push_back( newMap );
	}
	
	m_nCurrentWorkItem = CommandLine()->ParmValue( "-startstage", 0 );

	char const *pszStartMap = nullptr;
	CommandLine()->CheckParm( "-startmap", &pszStartMap );
	if ( pszStartMap )
	{
		m_sInitialStartMap = ks::nz( pszStartMap );
	}

	CommandLine()->RemoveParm( "-startstage" );
	CommandLine()->RemoveParm( "-makereslists" );
	CommandLine()->RemoveParm( "-reslistdir" );
	CommandLine()->RemoveParm( "-norebuildaudio" );
	CommandLine()->RemoveParm( "-startmap" );

	m_sOriginalCommandLine = ks::nz( CommandLine()->GetCmdLine() );

	// Add it back in for first map
	if ( pszStartMap )
	{
		CommandLine()->AppendParm( "-startmap", m_sInitialStartMap.c_str() );
	}

	m_sBaseCommandLine = kv->GetString( "basecommandline", "" );
	m_sFinalDir = kv->GetString( "finaldir", m_sFinalDir.c_str() );
	m_sWorkingDir = kv->GetString( "workdir", m_sWorkingDir.c_str() );

	int i = 0;
	do
	{
		char sz[ 32 ];
		V_snprintf( sz, sizeof( sz ), "%i", i );
		KeyValues *subKey = kv->FindKey( sz, false );
		if ( !subKey )
			break;

		CWorkItem work;

		work.m_sSubDir = subKey->GetString( "subdir", "" );
		work.m_sAddCommands = subKey->GetString( "addcommands", "" );

		if ( std::ssize( work.m_sSubDir ) > 0 )
		{
			m_WorkItems.push_back( work );
		}
		else
		{
			Error( "%s: failed to specify 'subdir' for item %s\n", fullpath, sz );
		}

		++i;
	} while ( true );

	m_bActive = !m_WorkItems.empty();
	
	m_nCurrentWorkItem = clamp( m_nCurrentWorkItem, 0, std::ssize( m_WorkItems ) - 1 );

	bool bCollate = CommandLine()->CheckParm( "-collate" ) ? true : false;
	if ( bCollate )
	{
		Collate();
		m_bActive = false;
		std::exit( -1 );
	}

	kv->deleteThis();

	return m_bActive;
}
