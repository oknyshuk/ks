// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import std;
import ks.platform;
import ks.simd;

#include "iprediction.h"





#include "client.h"
#include "cl_pred.h"

#include "demo.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "demo.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : world_start_state - 
//			validframe - 
//			start_command - 
//			stop_command - 
//-----------------------------------------------------------------------------
void CL_Predict( int world_start_state, bool validframe, int start_command, int stop_command )
{
	// Allow client .dll to do prediction.
	g_pClientSidePrediction->Update( 
		world_start_state, 
		validframe, 
		start_command, 
		stop_command );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if we received a new message, which means we need to
//  redo prediction
//-----------------------------------------------------------------------------
void CL_RunPrediction( PREDICTION_REASON reason )
{

	CClientState &cl = GetBaseLocalClient();

	if ( !cl.IsActive() )
		return;

	if ( cl.m_nDeltaTick < 0 ) 
	{
		// no valid snapshot received yet
		return;
	}

	// Don't do prediction if skipping ahead during demo playback
	if ( demoplayer->IsSkipping() )
	{
		return;
	}

	//Msg( "%i pred reason %s\n", host_framecount, reason == PREDICTION_SIMULATION_RESULTS_ARRIVING_ON_SEND_FRAME ?
	//	"same frame" : "normal" );

	bool valid = cl.m_nDeltaTick > 0; // GetBaseLocalClient().GetReceiveList( GetLocalClient().delta_tick ) != NULL;

	//Msg( "%i/%i CL_RunPrediction:  last ack %i most recent %i\n", 
	//	host_framecount, GetBaseLocalClient().tickcount,
	//	GetBaseLocalClient().last_command_ack, 
	//	GetBaseLocalClient().netchan->m_nOutSequenceNr - 1 );

	CL_Predict( cl.m_nDeltaTick, 
		valid,  
		cl.last_command_ack, 
		cl.lastoutgoingcommand + cl.chokedcommands );
}
