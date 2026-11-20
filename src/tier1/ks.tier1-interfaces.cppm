// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.interfaces;

import std;
import ks.platform;
import ks.simd;

export extern "C++" {

typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ICvar;
class ILocalize;

class IColorCorrectionSystem;
class IDebugTextureInfo;
class IFileSystem;
class IInputSystem;
class IInputStackSystem;
class IMaterialSystem;
class IMaterialSystemHardwareConfig;
class IP4;
class IResourceAccessControl;
class IPrecacheSystem;
class IResourceSystem;
class IVBAllocTracker;
class IMatchFramework;
class IStudioRender;
class IMatSystemSurface;
class IDataCache;
class IMDLCache;
class IPhysicsCollision;
class ISoundEmitterSystemBase;

class IRocketUI;

//-----------------------------------------------------------------------------
// Fills out global DLL exported interface pointers
//-----------------------------------------------------------------------------
inline constexpr const char *CVAR_INTERFACE_VERSION = "VEngineCvar007";
extern ICvar *cvar;;
extern ICvar *g_pCVar;


inline constexpr const char *LOCALIZE_INTERFACE_VERSION = "Localize_001";
extern ILocalize *g_pLocalize;;

inline constexpr const char *FILESYSTEM_INTERFACE_VERSION = "VFileSystem017";
extern IFileSystem *g_pFullFileSystem;;


inline constexpr const char *RESOURCESYSTEM_INTERFACE_VERSION = "ResourceSystem004";
extern IResourceSystem *g_pResourceSystem;;

inline constexpr const char *MATERIAL_SYSTEM_INTERFACE_VERSION = "VMaterialSystem080";
extern IMaterialSystem *materials;;
extern IMaterialSystem *g_pMaterialSystem;;

inline constexpr const char *INPUTSYSTEM_INTERFACE_VERSION = "InputSystemVersion001";
extern IInputSystem *g_pInputSystem;;

inline constexpr const char *INPUTSTACKSYSTEM_INTERFACE_VERSION = "InputStackSystemVersion001";
extern IInputStackSystem *g_pInputStackSystem;;

inline constexpr const char *MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION = "MaterialSystemHardwareConfig013";
extern IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig;;

inline constexpr const char *DEBUG_TEXTURE_INFO_VERSION = "DebugTextureInfo001";
extern IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo;;

extern IVBAllocTracker *g_VBAllocTracker;;

inline constexpr const char *COLORCORRECTION_INTERFACE_VERSION = "COLORCORRECTION_VERSION_1";
extern IColorCorrectionSystem *colorcorrection;;

inline constexpr const char *P4_INTERFACE_VERSION = "VP4002";
extern IP4 *p4;;

inline constexpr const char *RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION = "VResourceAccessControl001";
extern IResourceAccessControl *g_pResourceAccessControl;;

inline constexpr const char *PRECACHE_SYSTEM_INTERFACE_VERSION = "VPrecacheSystem001";
extern IPrecacheSystem *g_pPrecacheSystem;;

inline constexpr const char *MATCHFRAMEWORK_INTERFACE_VERSION = "MATCHFRAMEWORK_001";
extern IMatchFramework *g_pMatchFramework;;

inline constexpr const char *ROCKETUI_INTERFACE_VERSION = "RocketUI001";
extern IRocketUI *g_pRocketUI;;

//-----------------------------------------------------------------------------
// Not exactly a global, but we're going to keep track of these here anyways
// NOTE: Appframework deals with connecting these bad boys. See materialsystem2app.cpp
//-----------------------------------------------------------------------------
inline constexpr const char *STUDIO_RENDER_INTERFACE_VERSION = "VStudioRender026";
extern IStudioRender *g_pStudioRender;;
extern IStudioRender *studiorender;;

inline constexpr const char *DATACACHE_INTERFACE_VERSION = "VDataCache003";
extern IDataCache *g_pDataCache;;	// FIXME: Should IDataCache be in tier2?

inline constexpr const char *MDLCACHE_INTERFACE_VERSION = "MDLCache004";
extern IMDLCache *g_pMDLCache;;
extern IMDLCache *mdlcache;;

inline constexpr const char *VPHYSICS_COLLISION_INTERFACE_VERSION = "VPhysicsCollision007";
extern IPhysicsCollision *g_pPhysicsCollision;;

inline constexpr const char *SOUNDEMITTERSYSTEM_INTERFACE_VERSION = "VSoundEmitter003";
extern ISoundEmitterSystemBase *g_pSoundEmitterSystem;;

//-----------------------------------------------------------------------------
// Fills out global DLL exported interface pointers
//-----------------------------------------------------------------------------
void ConnectInterfaces( CreateInterfaceFn *pFactoryList, int nFactoryCount );
void DisconnectInterfaces();

//-----------------------------------------------------------------------------
// Reconnects an interface
//-----------------------------------------------------------------------------
void ReconnectInterface( CreateInterfaceFn factory, const char *pInterfaceName );

}
