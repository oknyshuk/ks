// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: perform initialization needed in most command line programs

#include <tier2/tier2.h>
import std;
import ks.tier1;



static void PrintFReportHandler(char const *job_name, int total_units_to_do, int n_units_completed)
{
	static bool work_in_progress=false;
	static char LastJobName[1024];
	if ( V_strncmp( LastJobName, job_name, sizeof( LastJobName ) ) )
	{
		if ( work_in_progress )
			std::printf("..done\n");
		V_strncpy( LastJobName, job_name, sizeof( LastJobName ) );
	}
 	if ( (total_units_to_do > 0 ) && (total_units_to_do >= n_units_completed) )
	{
		int percent_done=(100*n_units_completed)/total_units_to_do;
		std::printf("\r%s : %d%%",LastJobName, percent_done );
		work_in_progress = true;
	}
	else
	{
		std::printf("%s\n",LastJobName);
		work_in_progress = false;
	}
}

void InitCommandLineProgram( int &argc, char ** &argv )
{
	MathLib_Init( 1,1,1,0,true,true,true);
	CommandLine()->CreateCmdLine( argc, argv );
	InitDefaultFileSystem();
	InstallProgressReportHandler( PrintFReportHandler );
	// handle -allowdebug transparently
	if ( ( argc > 1 ) && ( !std::strcmp( argv[1], "-allowdebug" ) ) )
	{
		argv++;												// messes up argv[0]
		argc--;
	}

}
