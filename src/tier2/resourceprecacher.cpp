// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: A higher level link library for general use in the game and tools.

#include <tier2/tier2.h>
#include "resourceprecacher.h"
#include "datacache/iprecachesystem.h"
import ks.tier1.interfaces;

CBaseResourcePrecacher *CBaseResourcePrecacher::sm_pFirst[PRECACHE_SYSTEM_COUNT] = { 0 };

//-----------------------------------------------------------------------------
// Registers all resource precachers (created by PRECACHE_ macros) with precache system
//-----------------------------------------------------------------------------
void CBaseResourcePrecacher::RegisterAll()
{
	for ( int iSystem = 0; iSystem < PRECACHE_SYSTEM_COUNT; iSystem++ )
	{
		g_pPrecacheSystem->Register( sm_pFirst[iSystem], (PrecacheSystem_t) iSystem );
	}
}
