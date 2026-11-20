// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: A higher level link library for general use in the game and tools.

import std;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "materialsystem/ivballoctracker.h"

#include "rocketui/rocketui.h"


//-----------------------------------------------------------------------------
// Tier 1 libraries
//-----------------------------------------------------------------------------
ICvar *cvar = 0;
ICvar *g_pCVar = 0;
static bool s_bConnected = false;
ILocalize *g_pLocalize = 0;

// for utlsortvector.h
void *g_pUtlSortVectorQSortContext = nullptr;



//-----------------------------------------------------------------------------
// Tier 2 libraries
//-----------------------------------------------------------------------------
IResourceSystem *g_pResourceSystem = 0;
IFileSystem *g_pFullFileSystem = 0;
IMaterialSystem *materials = 0;
IMaterialSystem *g_pMaterialSystem = 0;
IInputSystem *g_pInputSystem = 0;
IInputStackSystem *g_pInputStackSystem = 0;
IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig = 0;
IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo = 0;
IVBAllocTracker *g_VBAllocTracker = 0;
IColorCorrectionSystem *colorcorrection = 0;
IP4 *p4 = 0;
IResourceAccessControl *g_pResourceAccessControl = 0;
IPrecacheSystem *g_pPrecacheSystem = 0;


IMatchFramework *g_pMatchFramework = 0;

IRocketUI *g_pRocketUI = 0;

//-----------------------------------------------------------------------------
// Tier3 libraries
//-----------------------------------------------------------------------------
IStudioRender *g_pStudioRender = 0;
IStudioRender *studiorender = 0;
IDataCache *g_pDataCache = 0;
IMDLCache *g_pMDLCache = 0;
IMDLCache *mdlcache = 0;
IPhysicsCollision *g_pPhysicsCollision = 0;
ISoundEmitterSystemBase *g_pSoundEmitterSystem = 0;


//-----------------------------------------------------------------------------
// Mapping of interface string to globals
//-----------------------------------------------------------------------------
struct InterfaceGlobals_t
{
	const char *m_pInterfaceName;
	void *m_ppGlobal;
};

static InterfaceGlobals_t g_pInterfaceGlobals[] =
{
	{ CVAR_INTERFACE_VERSION, &cvar },
	{ CVAR_INTERFACE_VERSION, &g_pCVar },
	{ FILESYSTEM_INTERFACE_VERSION, &g_pFullFileSystem },
	{ RESOURCESYSTEM_INTERFACE_VERSION, &g_pResourceSystem },
	{ MATERIAL_SYSTEM_INTERFACE_VERSION, &g_pMaterialSystem },
	{ MATERIAL_SYSTEM_INTERFACE_VERSION, &materials },
	{ INPUTSYSTEM_INTERFACE_VERSION, &g_pInputSystem },
	{ INPUTSTACKSYSTEM_INTERFACE_VERSION, &g_pInputStackSystem },
	{ MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, &g_pMaterialSystemHardwareConfig },
	{ DEBUG_TEXTURE_INFO_VERSION, &g_pMaterialSystemDebugTextureInfo },
	{ VB_ALLOC_TRACKER_INTERFACE_VERSION, &g_VBAllocTracker },
	{ COLORCORRECTION_INTERFACE_VERSION, &colorcorrection },
	{ P4_INTERFACE_VERSION, &p4 },
	{ RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION, &g_pResourceAccessControl },
	{ PRECACHE_SYSTEM_INTERFACE_VERSION, &g_pPrecacheSystem },
	{ STUDIO_RENDER_INTERFACE_VERSION, &g_pStudioRender },
	{ STUDIO_RENDER_INTERFACE_VERSION, &studiorender },
	{ LOCALIZE_INTERFACE_VERSION, &g_pLocalize },
	{ DATACACHE_INTERFACE_VERSION, &g_pDataCache },
	{ MDLCACHE_INTERFACE_VERSION, &g_pMDLCache },
	{ MDLCACHE_INTERFACE_VERSION, &mdlcache },
	{ VPHYSICS_COLLISION_INTERFACE_VERSION, &g_pPhysicsCollision },
	{ SOUNDEMITTERSYSTEM_INTERFACE_VERSION, &g_pSoundEmitterSystem },


	{ MATCHFRAMEWORK_INTERFACE_VERSION, &g_pMatchFramework },

	{ ROCKETUI_INTERFACE_VERSION, &g_pRocketUI },

};


//-----------------------------------------------------------------------------
// The # of times this DLL has been connected
//-----------------------------------------------------------------------------
static int s_nConnectionCount = 0;


//-----------------------------------------------------------------------------
// At each level of connection, we're going to keep track of which interfaces
// we filled in. When we disconnect, we'll clear those interface pointers out.
//-----------------------------------------------------------------------------
struct ConnectionRegistration_t
{
	void *m_ppGlobalStorage;
	int m_nConnectionPhase;
};

static int s_nRegistrationCount = 0;
static ConnectionRegistration_t s_pConnectionRegistration[ std::size( g_pInterfaceGlobals ) + 1 ];

void RegisterInterface( CreateInterfaceFn factory, const char *pInterfaceName, void **ppGlobal )
{
	if ( !(*ppGlobal) )
	{
		*ppGlobal = factory( pInterfaceName, nullptr );
		if ( *ppGlobal )
		{
			contract_assert( s_nRegistrationCount < std::size( s_pConnectionRegistration ) );
			ConnectionRegistration_t &reg = s_pConnectionRegistration[s_nRegistrationCount++];
			reg.m_ppGlobalStorage = ppGlobal;
			reg.m_nConnectionPhase = s_nConnectionCount;
		}
	}
}

void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName, void **ppGlobal )
{
	*ppGlobal = factory( pInterfaceName, nullptr );

	bool bFound = false;
	contract_assert( s_nRegistrationCount < std::size( s_pConnectionRegistration ) );
	for ( int i = 0; i < s_nRegistrationCount; ++i )
	{
		ConnectionRegistration_t &reg = s_pConnectionRegistration[i];
		if ( reg.m_ppGlobalStorage != ppGlobal )
			continue;

		reg.m_ppGlobalStorage = ppGlobal;
		bFound = true;
	}

	if ( !bFound && *ppGlobal )
	{
		contract_assert( s_nRegistrationCount < std::size( s_pConnectionRegistration ) );
		ConnectionRegistration_t &reg = s_pConnectionRegistration[s_nRegistrationCount++];
		reg.m_ppGlobalStorage = ppGlobal;
		reg.m_nConnectionPhase = s_nConnectionCount;
	}
}


//-----------------------------------------------------------------------------
// Call this to connect to all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectInterfaces( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	if ( s_nRegistrationCount < 0 )
	{
		Error( "APPSYSTEM: In ConnectInterfaces(), s_nRegistrationCount is %d!\n", s_nRegistrationCount );
	}
	else if ( s_nRegistrationCount == 0 )
	{
		for ( int i = 0; i < nFactoryCount; ++i )
		{
			for ( int j = 0; j < std::size( g_pInterfaceGlobals ); ++j )
			{
				RegisterInterface( pFactoryList[i], g_pInterfaceGlobals[j].m_pInterfaceName, (void**)g_pInterfaceGlobals[j].m_ppGlobal );
			}
		}
	}
	else
	{
		// This is no longer questionable: ConnectInterfaces() is expected to be called multiple times for a file that exports multiple interfaces.
		// Warning("APPSYSTEM: ConnectInterfaces() was called twice for the same DLL.\nThis is expected behavior in building reslists, but questionable otherwise.\n");
		for ( int i = 0; i < nFactoryCount; ++i )
		{
			for ( int j = 0; j < std::size( g_pInterfaceGlobals ); ++j )
			{
				ReconnectInterface( pFactoryList[i], g_pInterfaceGlobals[j].m_pInterfaceName, (void**)g_pInterfaceGlobals[j].m_ppGlobal );
			}
		}
	}
	++s_nConnectionCount;
}

void DisconnectInterfaces()
{
	contract_assert( s_nConnectionCount > 0 );
	if ( --s_nConnectionCount < 0 )
		return;

	for ( int i = 0; i < s_nRegistrationCount; ++i )
	{
		if ( s_pConnectionRegistration[i].m_nConnectionPhase != s_nConnectionCount )
			continue;

		// Disconnect!
		*(void**)(s_pConnectionRegistration[i].m_ppGlobalStorage) = 0;
	}
}


//-----------------------------------------------------------------------------
// Reloads an interface
//-----------------------------------------------------------------------------
void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName )
{
	for ( int i = 0; i < std::size( g_pInterfaceGlobals ); ++i )
	{
		if ( std::strcmp( g_pInterfaceGlobals[i].m_pInterfaceName, pInterfaceName ) )
			continue;		
		ReconnectInterface( factory, g_pInterfaceGlobals[i].m_pInterfaceName, (void**)g_pInterfaceGlobals[i].m_ppGlobal );
	}
}
