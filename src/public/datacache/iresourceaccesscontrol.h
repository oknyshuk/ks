#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "tier2/resourceprecacher.h"

//-----------------------------------------------------------------------------
// Resource access control API
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// IResourceAccessControl
// Purpose: Maintains lists of resources to use them as filters to prevent access
// to ensure proper precache behavior in game code
//-----------------------------------------------------------------------------
class IResourceAccessControl
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

	// Creates, destroys a resource list
	ResourceList_t CreateResourceList( const char *pDebugName );
	void DestroyAllResourceLists( );

	// Adds a resource to a resource list
	void AddResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName );

	// Prevents access to anything except the specified resource list
	// Pass RESOURCE_LIST_INVALID to allow access to all resources
	void LimitAccess( ResourceList_t hResourceList );

	// Is access to this resource allowed?
	bool IsAccessAllowed( ResourceTypeOld_t nType, const char *pResource );
};
