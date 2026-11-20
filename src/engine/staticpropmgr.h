#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This file contains a little interface to deal with static prop collisions

import std;
import ks.core.basetypes;
import ks.core.logging;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
#include "engine/IStaticPropMgr.h"

// FIXME: Remove! Only here for the test against old code
#include "enginetrace.h"

//-----------------------------------------------------------------------------
// foward declarations
//-----------------------------------------------------------------------------
class ICollideable;
struct LightCacheHandle_t__;
using LightCacheHandle_t = LightCacheHandle_t__ *;
class IPooledVBAllocator;

extern LoggingChannelID_t LOG_StaticPropManager;;

//-----------------------------------------------------------------------------
// The engine's static prop manager
//-----------------------------------------------------------------------------
class IStaticPropMgrEngine
{
public:
	bool Init();
	void Shutdown();

	// Call at the beginning of the level, will unserialize all static
	// props and add them to the main collision tree
	void LevelInit();

	// Call this when there's a client, *after* LevelInit, and after the world entity is loaded
	void LevelInitClient();

	// Call this when there's a client, *before* LevelShutdown
	void LevelShutdownClient();

	// Call at the end of the level, cleans up the static props
	void LevelShutdown();

	// Call this to recompute static prop lighting when necessary
	void RecomputeStaticLighting();

	// Check if a static prop is in a particular PVS.
	bool IsPropInPVS( IHandleEntity *pHandleEntity, const byte *pVis ) const;

	// temp - loadtime setting of flag to disable CSM rendering for static prop.
	void DisableCSMRenderingForStaticProp( int staticPropIndex );

	// returns a collideable interface to static props
	ICollideable *GetStaticProp( IHandleEntity *pHandleEntity );

	// returns the lightcache handle associated with a static prop
	LightCacheHandle_t GetLightCacheHandleForStaticProp( IHandleEntity *pHandleEntity );

	// Is a base handle a static prop?
	bool IsStaticProp( IHandleEntity *pHandleEntity ) const;
	bool IsStaticProp( CBaseHandle handle ) const;

	// Returns the static prop index (useful for networking)
	int GetStaticPropIndex( IHandleEntity *pHandleEntity ) const;

	void ConfigureSystemLevel( int nCPULevel, int nGPULevel );

	void RestoreStaticProps();
};


//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IStaticPropMgrEngine* StaticPropMgr();
