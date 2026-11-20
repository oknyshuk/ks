// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import std;
import ks.platform;
import ks.simd;






#include "client.h"


#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "cl_localnetworkbackdoor.h"
#include "LocalNetworkBackdoor.h"

static CLocalNetworkBackdoor s_LocalNetworkBackdoor;

void CL_SetupLocalNetworkBackDoor( bool bUseBackDoor )
{
	if ( bUseBackDoor )
	{
		if ( !g_pLocalNetworkBackdoor )
		{
			// start using local backdoor mode
			g_pLocalNetworkBackdoor = &s_LocalNetworkBackdoor;
			g_pLocalNetworkBackdoor->StartBackdoorMode();
		}
	}
	else
	{
		if ( g_pLocalNetworkBackdoor )
		{
			// stop using localbackdooor mode
			g_pLocalNetworkBackdoor->StopBackdoorMode();
			g_pLocalNetworkBackdoor = nullptr;
			GetBaseLocalClient().ForceFullUpdate( "cl_localnetworkbackdoor state change" ); // clear baselines, request full packet update
		}
	}
}
