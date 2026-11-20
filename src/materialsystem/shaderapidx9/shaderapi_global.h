#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.memalloc;
import ks.platform;
import ks.simd;
import ks.tier1.null_mutex;

//-----------------------------------------------------------------------------
// Use this to fill in structures with the current board state
//-----------------------------------------------------------------------------
#ifdef _DEBUG

#define DEBUG_BOARD_STATE 0
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class CShaderDeviceBase;
class CShaderDeviceMgrBase;
class CShaderAPIBase;
class IShaderShadow;


//-----------------------------------------------------------------------------
// Global interfaces
//-----------------------------------------------------------------------------
extern IShaderUtil* g_pShaderUtil;
inline IShaderUtil* ShaderUtil()
{
	return g_pShaderUtil;
}

extern CShaderDeviceBase *g_pShaderDeviceBase;
extern CShaderDeviceMgrBase *g_pShaderDeviceMgrBase;
extern CShaderAPIBase *g_pShaderAPIBase;
extern IShaderShadow *g_pShaderShadow;


//-----------------------------------------------------------------------------
// Memory debugging 
//-----------------------------------------------------------------------------
#define BEGIN_D3D_ALLOCATION()	MemAlloc_PushAllocDbgInfo("D3D:" __FILE__, __LINE__)
[[gnu::always_inline]] inline auto END_D3D_ALLOCATION() { return MemAlloc_PopAllocDbgInfo(); }


//-----------------------------------------------------------------------------
// Threading
//-----------------------------------------------------------------------------
extern bool g_bUseShaderMutex;

//#define USE_SHADER_DISALLOW 1
//#define STRICT_MT_SHADERAPI 1

#if defined(_DEBUG)
#else
#endif


typedef ks::null_mutex CShaderMutex;

extern CShaderMutex g_ShaderMutex;

extern bool g_bShaderAccessDisallowed;
