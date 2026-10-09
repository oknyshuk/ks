// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:interfaces;

import std;
import ks.math;

export extern "C++" {

typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ICvar;
class IProcessUtils;
class ILocalize;
class IPhysics2;
class IPhysics2ActorManager;
class IPhysics2ResourceManager;
class IEventSystem;

class IAsyncFileSystem;
class IColorCorrectionSystem;
class IDebugTextureInfo;
class IFileSystem;
class IRenderHardwareConfig;
class IInputSystem;
class IInputStackSystem;
class IMaterialSystem;
class IMaterialSystem2;
class IMaterialSystemHardwareConfig;
class IMdlLib;
class INetworkSystem;
class IP4;
class IResourceAccessControl;
class IPrecacheSystem;
class IRenderDevice;
class IRenderDeviceMgr;
class IResourceSystem;
class IVBAllocTracker;
class IMatchFramework;
class ISoundSystem;
class IStudioRender;
class IMatSystemSurface;
class IGameUISystemMgr;
class IDataCache;
class IMDLCache;
class IAvi;
class IBik;
class IDmeMakefileUtils;
class IPhysicsCollision;
class ISoundEmitterSystemBase;
class IMeshSystem;
class IWorldRendererMgr;
class ISceneSystem;
class IVGuiRenderSurface;

class IRocketUI;

//-----------------------------------------------------------------------------
// Fills out global DLL exported interface pointers
//-----------------------------------------------------------------------------
inline constexpr const char *CVAR_INTERFACE_VERSION = "VEngineCvar007";
extern ICvar *cvar;;
extern ICvar *g_pCVar;

inline constexpr const char *PROCESS_UTILS_INTERFACE_VERSION = "VProcessUtils002";
extern IProcessUtils *g_pProcessUtils;;

inline constexpr const char *VPHYSICS2_INTERFACE_VERSION = "Physics2 Interface v0.3";
extern IPhysics2 *g_pPhysics2;;

inline constexpr const char *VPHYSICS2_ACTOR_MGR_INTERFACE_VERSION = "Physics2 Interface ActorMgr v0.1";
extern IPhysics2ActorManager *g_pPhysics2ActorManager;;

inline constexpr const char *VPHYSICS2_RESOURCE_MGR_INTERFACE_VERSION = "Physics2 Interface ResourceMgr v0.1";
extern IPhysics2ResourceManager *g_pPhysics2ResourceManager;;

inline constexpr const char *EVENTSYSTEM_INTERFACE_VERSION = "EventSystem001";
extern IEventSystem *g_pEventSystem;;

inline constexpr const char *LOCALIZE_INTERFACE_VERSION = "Localize_001";
extern ILocalize *g_pLocalize;;

inline constexpr const char *RENDER_DEVICE_MGR_INTERFACE_VERSION = "RenderDeviceMgr001";
extern IRenderDeviceMgr *g_pRenderDeviceMgr;;

inline constexpr const char *FILESYSTEM_INTERFACE_VERSION = "VFileSystem017";
extern IFileSystem *g_pFullFileSystem;;

inline constexpr const char *ASYNCFILESYSTEM_INTERFACE_VERSION = "VNewAsyncFileSystem001";
extern IAsyncFileSystem *g_pAsyncFileSystem;;

inline constexpr const char *RESOURCESYSTEM_INTERFACE_VERSION = "ResourceSystem004";
extern IResourceSystem *g_pResourceSystem;;

inline constexpr const char *MATERIAL_SYSTEM_INTERFACE_VERSION = "VMaterialSystem080";
extern IMaterialSystem *materials;;
extern IMaterialSystem *g_pMaterialSystem;;

inline constexpr const char *MATERIAL_SYSTEM2_INTERFACE_VERSION = "VMaterialSystem2_001";
extern IMaterialSystem2 *g_pMaterialSystem2;;

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

inline constexpr const char *MDLLIB_INTERFACE_VERSION = "VMDLLIB001";
extern IMdlLib *mdllib;;

inline constexpr const char *RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION = "VResourceAccessControl001";
extern IResourceAccessControl *g_pResourceAccessControl;;

inline constexpr const char *PRECACHE_SYSTEM_INTERFACE_VERSION = "VPrecacheSystem001";
extern IPrecacheSystem *g_pPrecacheSystem;;

inline constexpr const char *MATCHFRAMEWORK_INTERFACE_VERSION = "MATCHFRAMEWORK_001";
extern IMatchFramework *g_pMatchFramework;;

extern IGameUISystemMgr *g_pGameUISystemMgr;;

inline constexpr const char *ROCKETUI_INTERFACE_VERSION = "RocketUI001";
extern IRocketUI *g_pRocketUI;;

//-----------------------------------------------------------------------------
// Not exactly a global, but we're going to keep track of these here anyways
// NOTE: Appframework deals with connecting these bad boys. See materialsystem2app.cpp
//-----------------------------------------------------------------------------
inline constexpr const char *RENDER_DEVICE_INTERFACE_VERSION = "RenderDevice001";
extern IRenderDevice *g_pRenderDevice;;

inline constexpr const char *RENDER_HARDWARECONFIG_INTERFACE_VERSION = "RenderHardwareConfig001";
extern IRenderHardwareConfig *g_pRenderHardwareConfig;;

inline constexpr const char *SOUNDSYSTEM_INTERFACE_VERSION = "SoundSystem001";
extern ISoundSystem *g_pSoundSystem;;

inline constexpr const char *MESHSYSTEM_INTERFACE_VERSION = "MeshSystem001";
extern IMeshSystem *g_pMeshSystem;;

inline constexpr const char *STUDIO_RENDER_INTERFACE_VERSION = "VStudioRender026";
extern IStudioRender *g_pStudioRender;;
extern IStudioRender *studiorender;;

inline constexpr const char *MAT_SYSTEM_SURFACE_INTERFACE_VERSION = "MatSystemSurface006";
extern IMatSystemSurface *g_pMatSystemSurface;;

inline constexpr const char *RENDER_SYSTEM_SURFACE_INTERFACE_VERSION = "RenderSystemSurface001";
extern IVGuiRenderSurface *g_pVGuiRenderSurface;;

inline constexpr const char *SCENESYSTEM_INTERFACE_VERSION = "SceneSystem_001";
extern ISceneSystem *g_pSceneSystem;;

inline constexpr const char *DATACACHE_INTERFACE_VERSION = "VDataCache003";
extern IDataCache *g_pDataCache;;	// FIXME: Should IDataCache be in tier2?

inline constexpr const char *MDLCACHE_INTERFACE_VERSION = "MDLCache004";
extern IMDLCache *g_pMDLCache;;
extern IMDLCache *mdlcache;;

inline constexpr const char *AVI_INTERFACE_VERSION = "VAvi001";
extern IAvi *g_pAVI;;

inline constexpr const char *BIK_INTERFACE_VERSION = "VBik001";
extern IBik *g_pBIK;;

inline constexpr const char *DMEMAKEFILE_UTILS_INTERFACE_VERSION = "VDmeMakeFileUtils001";
extern IDmeMakefileUtils *g_pDmeMakefileUtils;;

inline constexpr const char *VPHYSICS_COLLISION_INTERFACE_VERSION = "VPhysicsCollision007";
extern IPhysicsCollision *g_pPhysicsCollision;;

inline constexpr const char *SOUNDEMITTERSYSTEM_INTERFACE_VERSION = "VSoundEmitter003";
extern ISoundEmitterSystemBase *g_pSoundEmitterSystem;;

inline constexpr const char *WORLD_RENDERER_MGR_INTERFACE_VERSION = "WorldRendererMgr001";
extern IWorldRendererMgr *g_pWorldRendererMgr;;

inline constexpr const char *NETWORKSYSTEM_INTERFACE_VERSION = "NetworkSystemVersion001";
extern INetworkSystem *g_pNetworkSystem;;

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
