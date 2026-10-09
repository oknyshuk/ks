#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "tier2/tier2.h"
#include "tier2/resourceprecacher.h"

//-----------------------------------------------------------------------------
// Resource access control API
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// IResourceAccessControl
// Purpose: Maintains lists of resources to use them as filters to prevent access
// to ensure proper precache behavior in game code
//-----------------------------------------------------------------------------
class IPrecacheSystem : public IAppSystem
{
public:
	// Precaches/uncaches all resources used by a particular system
	virtual void Cache( IPrecacheHandler *pPrecacheHandler, PrecacheSystem_t nSystem, 
		const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList ) = 0;

	virtual void UncacheAll( IPrecacheHandler *pPrecacheHandler ) = 0 ;

	virtual void Register( IResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem ) = 0;

	// Limits resource access to only resources used by this particular system
	// Use GLOBAL system, and NULL name to disable limited resource access
	virtual void LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName ) = 0;
	virtual void EndLimitedResourceAccess() = 0;
};

extern IPrecacheSystem *g_pPrecacheSystem;;
