#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "imaterialsysteminternal.h"
import std;
#include "tier2/tier2.h"

#include "rocketui/rocketui.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;
class IShaderAPI;
class IHardwareConfigInternal;
class IShaderUtil;
class IShaderShadow;
class IShaderDeviceMgr;
class IShaderDevice;
class IShaderSystemInternal;
class IMaterialInternal;
class IColorCorrectionSystem;
class IMaterialVar;


//-----------------------------------------------------------------------------
// Constants used by the system
//-----------------------------------------------------------------------------

inline constexpr auto MATERIAL_MAX_PATH = 256;

// GR - limits for blured image (HDR stuff)
inline constexpr auto MAX_BLUR_IMAGE_WIDTH = 256;
inline constexpr auto MAX_BLUR_IMAGE_HEIGHT = 192;

template <typename T1> [[gnu::always_inline]] inline auto CLAMP_BLUR_IMAGE_WIDTH( T1 _w ) { return ( ( _w < MAX_BLUR_IMAGE_WIDTH ) ? _w : MAX_BLUR_IMAGE_WIDTH ); }
template <typename T1> [[gnu::always_inline]] inline auto CLAMP_BLUR_IMAGE_HEIGHT( T1 _h ) { return ( ( _h < MAX_BLUR_IMAGE_HEIGHT ) ? _h : MAX_BLUR_IMAGE_HEIGHT ); }

//-----------------------------------------------------------------------------
// Global structures
//-----------------------------------------------------------------------------
extern MaterialSystem_Config_t g_config;
extern uint32 g_nDebugVarsSignature;

//extern MaterialSystem_ErrorFunc_t	Error;
//extern MaterialSystem_WarningFunc_t Warning;

extern int				g_FrameNum;


extern IShaderAPI*	g_pShaderAPI;
extern IShaderDeviceMgr* g_pShaderDeviceMgr;
extern IShaderDevice*	g_pShaderDevice;
extern IShaderShadow* g_pShaderShadow;

extern IMaterialInternal *g_pErrorMaterial;

IShaderSystemInternal* ShaderSystem();
inline IShaderSystemInternal* ShaderSystem()
{
	extern IShaderSystemInternal *g_pShaderSystem;
	return g_pShaderSystem;
}

inline IHardwareConfigInternal *HardwareConfig()
{
	extern IHardwareConfigInternal* g_pHWConfig;
	return g_pHWConfig;
}


//-----------------------------------------------------------------------------
// Accessor to get at the material system
//-----------------------------------------------------------------------------
inline IMaterialSystemInternal* MaterialSystem()
{
	extern IMaterialSystemInternal *g_pInternalMaterialSystem;
	return g_pInternalMaterialSystem;
}

inline IShaderUtil* ShaderUtil()
{
	extern IShaderUtil *g_pShaderUtil;
	return g_pShaderUtil;
}

extern IColorCorrectionSystem *g_pColorCorrectionSystem;
inline IColorCorrectionSystem *ColorCorrectionSystem()
{
	return g_pColorCorrectionSystem;
}


//-----------------------------------------------------------------------------
// Global methods related to material vars
//-----------------------------------------------------------------------------
void EnableThreadedMaterialVarAccess( bool bEnable, IMaterialVar **ppParams, int nVarCount );
