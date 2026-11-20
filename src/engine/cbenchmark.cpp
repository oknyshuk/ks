// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.strtools;

#include "common.h"

#include "cmd.h"
#include "render.h"



#include "gl_shader.h"

#include "cdll_engine_int.h"
#include "client_class.h"
#include "client.h"
#include "cl_main.h"

#include "demo.h"
#include "host.h"
#include "filesystem.h"
#include "filesystem_engine.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif

#include "cbenchmark.h"
#include "filesystem_engine.h"
import std;

inline constexpr const char *DEFAULT_RESULTS_FOLDER = "results";
inline constexpr const char *DEFAULT_RESULTS_FILENAME = "results.txt";

CBenchmarkResults g_BenchmarkResults;
extern ConVar host_framerate;
extern void GetMaterialSystemConfigForBenchmarkUpload(KeyValues *dataToUpload);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBenchmarkResults::CBenchmarkResults()
{
	m_bIsTestRunning = false;
	m_szFilename[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBenchmarkResults::IsBenchmarkRunning()
{
	return m_bIsTestRunning;
}
	
//-----------------------------------------------------------------------------
// Purpose: starts recording data
//-----------------------------------------------------------------------------
void CBenchmarkResults::StartBenchmark( const CCommand &args )
{
	const char *pszFilename = DEFAULT_RESULTS_FILENAME;
	
	if ( args.ArgC() > 1 )
	{
		pszFilename = args[1];
	}

	// check path first
	if ( !COM_IsValidPath( pszFilename ) )
	{
		ConMsg( "bench_start %s: invalid path.\n", pszFilename );
		return;
	}

	m_bIsTestRunning = true;

	SetResultsFilename( pszFilename );

	// set any necessary settings
	host_framerate.SetValue( (float)(1.0f / host_state.interval_per_tick) );

	// get the current frame and time
	m_iStartFrame = host_framecount;
	m_flStartTime = realtime;
}
	
//-----------------------------------------------------------------------------
// Purpose: writes out results to file
//-----------------------------------------------------------------------------
void CBenchmarkResults::StopBenchmark()
{
	m_bIsTestRunning = false;

	// reset
	host_framerate.SetValue( 0 );

	// print out some stats
	int numticks = host_framecount - m_iStartFrame;
	float framerate = numticks / ( realtime - m_flStartTime );
	Msg( "Average framerate: %.2f\n", framerate );
	
	// work out where to write the file
	g_pFileSystem->CreateDirHierarchy( DEFAULT_RESULTS_FOLDER, "MOD" );
	char szFilename[256];
	V_snprintf( szFilename, sizeof( szFilename ), "%s\\%s", DEFAULT_RESULTS_FOLDER, m_szFilename );

	// write out the data as keyvalues
	KeyValues *kv = new KeyValues( "benchmark" );
	kv->SetFloat( "framerate", framerate );
	kv->SetInt( "build", build_number() );

	// get material system info
	GetMaterialSystemConfigForBenchmarkUpload( kv );

	// save
	kv->SaveToFile( g_pFileSystem, szFilename, "MOD" );
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Sets which file the results will be written to
//-----------------------------------------------------------------------------
void CBenchmarkResults::SetResultsFilename( const char *pFilename )
{
	V_strncpy( m_szFilename, pFilename, sizeof( m_szFilename ) );
	V_DefaultExtension( m_szFilename, ".txt", sizeof( m_szFilename ) );
}

static void bench_start( const CCommand &args );
static ConCommand bench_start_command( "bench_start", bench_start, "Starts gathering of info. Arguments: filename to write results into", FCVAR_CHEAT );
static void bench_start( const CCommand &args )
{
	GetBenchResultsMgr()->StartBenchmark( args );
}

static void bench_end( const CCommand &args );
static ConCommand bench_end_command( "bench_end", bench_end, "Ends gathering of info.", FCVAR_CHEAT );
static void bench_end( const CCommand &args )
{
	GetBenchResultsMgr()->StopBenchmark();
}
