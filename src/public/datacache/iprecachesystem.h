#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
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
class IPrecacheSystem
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	// Precaches/uncaches all resources used by a particular system
	void Cache( PrecacheHandler_t pPrecacheHandler, PrecacheSystem_t nSystem, 
		const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList );

	void UncacheAll( PrecacheHandler_t pPrecacheHandler );

	void Register( CBaseResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem );

	// Limits resource access to only resources used by this particular system
	// Use GLOBAL system, and NULL name to disable limited resource access
	void LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName );
	void EndLimitedResourceAccess();
};

extern IPrecacheSystem *g_pPrecacheSystem;;
