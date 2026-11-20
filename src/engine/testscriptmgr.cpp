// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.cmdline;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.core.random;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.strtools;
#include "sys.h"
#include "testscriptmgr.h"
#include "filesystem_engine.h"
#include "cmd.h"
#include "host.h"
import std;

inline const Color TESTSCRIPT_COLOR = Color( 0, 255, 255 );

CTestScriptMgr g_TestScriptMgr;

ConVar testscript_debug( "testscript_debug", "0", 0, "Debug test scripts." );
ConVar testscript_running( "testscript_running", "0", 0, "Set to true when test scripts are running" );


// --------------------------------------------------------------------------------------------------- //
// Global console commands the test script manager implements.
// --------------------------------------------------------------------------------------------------- //
void Test_Wait( const CCommand &args );
static ConCommand Test_Wait_command( "Test_Wait", Test_Wait, "", FCVAR_CHEAT );
void Test_Wait( const CCommand &args )
{
	if ( !CommandLine()->FindParm("-insecure") )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_Wait: requires seconds parameter." );
		return;
	}

	float flSeconds = std::atof( args[ 1 ] );
	GetTestScriptMgr()->SetWaitTime( flSeconds );
}

void Test_RunFrame( const CCommand &args );
static ConCommand Test_RunFrame_command( "Test_RunFrame", Test_RunFrame, "", FCVAR_CHEAT );
void Test_RunFrame( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	GetTestScriptMgr()->SetWaitCheckPoint( "frame_end" );
}

void Test_WaitForCheckPoint( const CCommand &args );
static ConCommand Test_WaitForCheckPoint_command( "Test_WaitForCheckPoint", Test_WaitForCheckPoint, "", FCVAR_CHEAT );
void Test_WaitForCheckPoint( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_WaitForCheckPoint <checkpoint name> [once]: requires checkpoint name." );
		return;
	}

	bool bOnce = ( args.ArgC() >= 3 && V_stricmp( args[2], "once" ) == 0 );
	GetTestScriptMgr()->SetWaitCheckPoint( args[ 1 ], bOnce );
}

void Test_StartLoop( const CCommand &args );
static ConCommand Test_StartLoop_command( "Test_StartLoop", Test_StartLoop, "Test_StartLoop <loop name> - Denote the start of a loop. Really just defines a named point you can jump to.", FCVAR_CHEAT );
void Test_StartLoop( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_StartLoop: requires a loop name." );
		return;
	}

	GetTestScriptMgr()->StartLoop( args[ 1 ] );
}

void Test_LoopCount( const CCommand &args );
static ConCommand Test_LoopCount_command( "Test_LoopCount", Test_LoopCount, "Test_LoopCount <loop name> <count> - loop back to the specified loop start point the specified # of times.", FCVAR_CHEAT );
void Test_LoopCount( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		Warning( "Test_LoopCount: requires a loop name and number of times to loop." );
		return;
	}

	GetTestScriptMgr()->LoopCount( args[ 1 ], std::atoi( args[ 2 ] ) );
}

void Test_Loop( const CCommand &args );
static ConCommand Test_Loop_command( "Test_Loop", Test_Loop, "Test_Loop <loop name> - loop back to the specified loop start point unconditionally.", FCVAR_CHEAT );
void Test_Loop( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_Loop: requires a loop name." );
		return;
	}

	GetTestScriptMgr()->LoopCount( args[ 1 ], -1 );
}

void Test_LoopForNumSeconds( const CCommand &args );
static ConCommand Test_LoopForNumSeconds_command( "Test_LoopForNumSeconds", Test_LoopForNumSeconds, "Test_LoopForNumSeconds <loop name> <time> - loop back to the specified start point for the specified # of seconds.", FCVAR_CHEAT );
void Test_LoopForNumSeconds( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		Warning( "Test_LoopLoopForNumSeconds: requires a loop name and number of seconds to loop." );
		return;
	}

	GetTestScriptMgr()->LoopForNumSeconds( args[ 1 ], std::atof( args[ 2 ] ) );
}

static void Test_RandomChance( const CCommand &args );
static ConCommand Test_RandomChance_command( "Test_RandomChance", Test_RandomChance, "Test_RandomChance <percent chance, 0-100> <token1> <token2...> - Roll the dice and maybe run the command following the percentage chance.", FCVAR_CHEAT );
static void Test_RandomChance( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 3 )
	{
		Warning( "Test_RandomChance: requires percentage chance parameter (0-100) followed by command to execute." );
		return;
	}

	float flPercent = std::atof( args[ 1 ] );
	if ( RandomFloat( 0, 100 ) < flPercent )
	{
		char newString[1024];
		newString[0] = 0;

		for ( int i=2; i < args.ArgC(); i++ )
		{
			V_strncat( newString, args[ i ], sizeof( newString ), COPY_ALL_CHARACTERS );
			V_strncat( newString, " ", sizeof( newString ), COPY_ALL_CHARACTERS );
		}

		Cbuf_InsertText( Cbuf_GetCurrentPlayer(), newString, args.Source() );
	}
}


static void Test_SendKey( const CCommand &args );
static ConCommand Test_SendKey_command( "Test_SendKey", Test_SendKey, "", FCVAR_CHEAT );
static void Test_SendKey( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_SendKey: requires key to send.\n" );
		return;
	}

	Sys_TestSendKey( args[1] );
}

static void Test_StartScript( const CCommand &args );
static ConCommand Test_StartScript_command( "Test_StartScript", Test_StartScript, "Start a test script running..", FCVAR_CHEAT );
static void Test_StartScript( const CCommand &args )
{
	if ( !CommandLine()->FindParm( "-insecure" ) )
	{
		Warning( "Tests require that you launch the game with -insecure flag." );
		return;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "Test_StartScript: requires filename of script to start (file must be under testscripts directory).\n" );
		return;
	}

	GetTestScriptMgr()->StartTestScript( args[1] );
}


// --------------------------------------------------------------------------------------------------- //
// CTestScriptMgr implementation.
// --------------------------------------------------------------------------------------------------- //
CTestScriptMgr::CTestScriptMgr()
{
	m_hFile = FILESYSTEM_INVALID_HANDLE;
	m_NextCheckPoint[0] = 0;
	m_WaitUntil = Plat_FloatTime();
}


CTestScriptMgr::~CTestScriptMgr()
{
	Term();
}


bool CTestScriptMgr::StartTestScript( const char *pFilename )
{
	StopTestScript();

	char fullName[MAX_PATH];
 	V_snprintf( fullName, sizeof( fullName ), "testscripts\\%s", pFilename );	 
	V_DefaultExtension( fullName, ".vtest", sizeof( fullName ) );

	m_hFile = g_pFileSystem->Open( fullName, "rt", "GAME" ); 
	if ( m_hFile == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "StartTestScript( %s ) failed.\n", fullName );
		return false;
	}

	testscript_running.SetValue( 1 );
	RunCommands();	
	return true;
}


void CTestScriptMgr::StopTestScript( void )
{
	testscript_running.SetValue( 0 );
	Term();
}


void CTestScriptMgr::Term()
{
	if ( m_hFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( m_hFile );
		m_hFile = FILESYSTEM_INVALID_HANDLE;
	}
}


bool CTestScriptMgr::IsInitted() const
{
	return m_hFile != FILESYSTEM_INVALID_HANDLE;
}


void CTestScriptMgr::CheckPoint( const char *pName )
{
	if ( !IsInitted() || IsTimerWaiting() )
		return;

	if ( testscript_debug.GetInt() )
	{
		if ( stricmp( pName, "frame_end" ) != 0 )
		{
			ConColorMsg( TESTSCRIPT_COLOR, "TESTSCRIPT: CheckPoint -> '%s'.\n", pName );
		}
	}

	m_CheckPointsHit.insert( pName ); // Remember that we hit this checkpoint.

	if ( m_NextCheckPoint[0] )
	{
		if ( V_stricmp( m_NextCheckPoint, pName ) != 0 )
		{
			// This isn't the checkpoint you're looking for.
			return;
		}
	}

	// Either the timer expired, or we hit the checkpoint we were waiting for. Run some more commands.
	m_NextCheckPoint[0] = 0;
	RunCommands();
}


void CTestScriptMgr::RunCommands()
{
	contract_assert( IsInitted() );

	if ( Cbuf_IsProcessingCommands( Cbuf_GetCurrentPlayer() ) )
	{
		// Too bad, we got here from a concommand handler, any test script commands you think should be executed won't be executed correctly.
		// Cbuf_Execute() is not re-entrant, it won't crash, but it won't do the correct thing from the test script as
		// the commands get deferred causing the scripts to break/crash.
		// RunCommands() really can be only validly "ticked" from at least the CheckPoint( "frame_end" ) to be adherent
		// to the delays and still let frames pass.
		return;
	}

	while ( !IsTimerWaiting() && !IsCheckPointWaiting() )
	{
		// Parse out the next command.
		char curCommand[512];
		int iCurPos = 0;

		bool bInSlash = false;
		while ( 1 )
		{
			g_pFileSystem->Read( &curCommand[iCurPos], 1, m_hFile );
			if ( curCommand[iCurPos] == '/' )
			{
				if ( bInSlash )
				{
					// Ok, the rest of this line is a comment.
					char tempVal = !'\n';
					while ( tempVal != '\n' && !g_pFileSystem->EndOfFile( m_hFile ) )
					{
						g_pFileSystem->Read( &tempVal, 1, m_hFile );
					}

					--iCurPos;
					break;
				}
				else
				{
					bInSlash = true;
				}
			}
			else
			{
				bInSlash = false;

				if ( curCommand[iCurPos] == ';' || curCommand[iCurPos] == '\n' || g_pFileSystem->EndOfFile( m_hFile ) )
				{
					// End of this command.
					break;
				}
			}

			++iCurPos;
		}

		curCommand[iCurPos] = 0;
		
		// Did we hit the end of the file?
		if ( curCommand[0] == 0 )
		{
			if ( g_pFileSystem->EndOfFile( m_hFile ) )
			{
				StopTestScript();
				break;
			}
			else
			{
				continue;
			}			
		}
		
		if ( developer.GetBool() || testscript_debug.GetInt() )
		{
			ConColorMsg( TESTSCRIPT_COLOR, "TESTSCRIPT: Executing command from script: %s\n", curCommand );
		}

		Cbuf_AddText( Cbuf_GetCurrentPlayer(), curCommand );
		Cbuf_Execute();
	}
}


bool CTestScriptMgr::IsTimerWaiting() const
{
	if ( Plat_FloatTime() < m_WaitUntil )
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool CTestScriptMgr::IsCheckPointWaiting() const
{
	return m_NextCheckPoint[0] != 0;
}


void CTestScriptMgr::SetWaitTime( float flSeconds )
{
	m_WaitUntil = Plat_FloatTime() + flSeconds;
}


CLoopInfo* CTestScriptMgr::FindLoop( const char *pLoopName )
{
	for ( auto &pLoop : m_Loops )
	{
		if ( V_stricmp( pLoopName, pLoop->m_Name ) == 0 )
			return pLoop.get();
	}
	return nullptr;
}


void CTestScriptMgr::StartLoop( const char *pLoopName )
{
	ErrorIfNotInitted();

	if ( FindLoop( pLoopName ) )
	{
		Error( "CTestScriptMgr::StartLoop( %s ): loop already exists.", pLoopName );
	}

	CLoopInfo *pLoop = m_Loops.emplace_back( std::make_unique< CLoopInfo >() ).get();
	V_strncpy( pLoop->m_Name, pLoopName, sizeof( pLoop->m_Name ) );
	pLoop->m_nCount = 0;
	pLoop->m_flStartTime = Plat_FloatTime();
	pLoop->m_iNextCommandPos = g_pFileSystem->Tell( m_hFile );
}


void CTestScriptMgr::LoopCount( const char *pLoopName, int nTimes )
{
	ErrorIfNotInitted();

	CLoopInfo *pLoop = FindLoop( pLoopName );
	if ( !pLoop )
	{
		Warning( "CTestScriptMgr::LoopCount( %s ): no loop with this name exists.", pLoopName );
		return;
	}

	++pLoop->m_nCount;
	if ( pLoop->m_nCount < nTimes || nTimes == -1 )
	{
		ConColorMsg( TESTSCRIPT_COLOR, "***************************************************\n" );
		ConColorMsg( TESTSCRIPT_COLOR, "TESTSCRIPT: Performing loop to %s (%d iterations)\n", pLoopName, pLoop->m_nCount );
		ConColorMsg( TESTSCRIPT_COLOR, "***************************************************\n" );
		g_pFileSystem->Seek( m_hFile, pLoop->m_iNextCommandPos, FILESYSTEM_SEEK_HEAD );
	}
	else
	{
		std::erase_if( m_Loops, [pLoop]( const auto &p ) { return p.get() == pLoop; } );
	}
}


void CTestScriptMgr::LoopForNumSeconds( const char *pLoopName, double nSeconds )
{
	ErrorIfNotInitted();

	CLoopInfo *pLoop = FindLoop( pLoopName );
	if ( !pLoop )
	{
		Error( "CTestScriptMgr::LoopForNumSeconds( %s ): no loop with this name exists.", pLoopName );
	}

	if ( Plat_FloatTime() - pLoop->m_flStartTime < nSeconds )
	{
		ConColorMsg( TESTSCRIPT_COLOR, "***************************************************\n" );
		ConColorMsg( TESTSCRIPT_COLOR, "TESTSCRIPT: Performing loop to %s\n", pLoopName );
		ConColorMsg( TESTSCRIPT_COLOR, "***************************************************\n" );
		g_pFileSystem->Seek( m_hFile, pLoop->m_iNextCommandPos, FILESYSTEM_SEEK_HEAD );
	}
	else
	{
		std::erase_if( m_Loops, [pLoop]( const auto &p ) { return p.get() == pLoop; } );
	}
}


void CTestScriptMgr::ErrorIfNotInitted()
{
	if ( !IsInitted() )
	{
		Error( "CTestScriptMgr: not initialized." );
	}
}


void CTestScriptMgr::SetWaitCheckPoint( const char *pCheckPointName, bool bOnce )
{
	if ( testscript_debug.GetInt() )
	{
		if ( stricmp( pCheckPointName, "frame_end" ) != 0 )
		{
			ConColorMsg( TESTSCRIPT_COLOR, "TESTSCRIPT: waiting for checkpoint '%s'%s\n", pCheckPointName, bOnce ? " (once)." : "." );
		}
	}
	
	// Don't wait on this checkpoint if we alereayd
	if ( bOnce && m_CheckPointsHit.contains( pCheckPointName ) )
		return;

	V_strncpy( m_NextCheckPoint, pCheckPointName, sizeof( m_NextCheckPoint ) );
}
