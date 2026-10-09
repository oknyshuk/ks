// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import std;
import ks.tier1;

#include "common.h"

#include "cmd.h"
#include "render.h"



#include "gl_shader.h"

#include "cdll_engine_int.h"
#include "client_class.h"
#include "client.h"
#include "cl_main.h"

#include "con_nprint.h"
#include "demo.h"
#include "host.h"
#include "filesystem.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "dt_recv_eng.h"
#include "client_class.h"
#include "serializedentity.h"

static ConVar cl_showevents	( "cl_showevents", "0", FCVAR_CHEAT, "Print event firing info in the console" );

//-----------------------------------------------------------------------------
// Purpose: Show descriptive info about an event in the numbered console area
// Input  : slot - 
//			*eventname - 
//-----------------------------------------------------------------------------
void CL_DescribeEvent( int slot, CEventInfo *event, const char *eventname )
{
	int idx = (slot & 31);

	if ( !cl_showevents.GetInt() )
		return;

	if ( !eventname )
		return;

	con_nprint_t n;
	n.index = idx;
	n.fixed_width_font = true;
	n.time_to_live = 4.0f;
	n.color[0] = 0.8;
	n.color[1] = 0.8;
	n.color[2] = 1.0;

	CSerializedEntity *pEntity = (CSerializedEntity *)event->m_Packed;

	Con_NXPrintf( &n, "%02i %6.3ff %20s %03i bytes", slot, GetBaseLocalClient().GetTime(), eventname, pEntity ? Bits2Bytes( pEntity->GetFieldDataBitCount() ) : 0 );

	if ( cl_showevents.GetInt() == 2 )
	{
		DevMsg( "%02i %6.3ff %20s %03i bytes\n", slot, GetBaseLocalClient().GetTime(), eventname, pEntity ?  Bits2Bytes( pEntity->GetFieldDataBitCount() ) : 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Decode raw event data into underlying class structure using the specified data table
// Input  : *RawData - 
//			*pToData - 
//			*pRecvTable - 
//-----------------------------------------------------------------------------
void CL_DecodeEventDelta( SerializedEntityHandle_t handle, void *pToData, RecvTable *pRecvTable )
{
	// Make sure we have a decoder
	contract_assert( pRecvTable->m_pDecoder );

	// First, decode all properties as zeros since temp ents are delta'd from zeros.
	RecvTable_DecodeZeros( pRecvTable, pToData, -1 );

	RecvTable_Decode( pRecvTable, pToData, handle, -1 );
}

//-----------------------------------------------------------------------------
// Purpose: Once per frame, walk the client's event slots and look for any events
//  that are ready for playing.
//-----------------------------------------------------------------------------
void CL_FireEvents( void )
{
	CVProfScope _( "CL_FireEvents", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	CClientState &cl = GetBaseLocalClient();

	if ( !cl.IsActive() )
	{
		cl.events.clear();
		return;
	}
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);

	int nSlot = 0;
	for ( auto it = cl.events.begin(); it != cl.events.end(); ++nSlot )
	{
		CEventInfo *ei = &*it;
		if ( ei->classID == 0 )
		{
			it = cl.events.erase( it );
			continue;
		}

		// Delayed event!
		if ( ei->fire_delay && ( ei->fire_delay > cl.GetTime() ) )
		{
			++it;
			continue;
		}

		bool success = false;

		// Get the receive table if it exists
		contract_assert( ei->pClientClass );
				
		// Get pointer to the event.
		if( ei->pClientClass->m_pCreateEventFn )
		{
			IClientNetworkable *pCE = ei->pClientClass->m_pCreateEventFn();
			if(pCE)
			{
				// Prepare to copy in the data
				pCE->PreDataUpdate( DATA_UPDATE_CREATED );

				// Decode data into client event object
				CL_DecodeEventDelta( ei->m_Packed, pCE->GetDataTableBasePtr(), ei->pClientClass->m_pRecvTable );

				// Fire the event!!!
				pCE->PostDataUpdate( DATA_UPDATE_CREATED );

				// Spew to debug area if needed
				CL_DescribeEvent( nSlot, ei, ei->pClientClass->m_pNetworkName );

				success = true;
			}
		}

		if ( !success )
		{
			ConDMsg( "Failed to execute event for classId %i\n", ei->classID - 1 );
		}

		it = cl.events.erase( it );
	}
}
