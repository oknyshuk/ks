//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPI_GLOBAL_H
#define SHADERAPI_GLOBAL_H


#include <mutex>
#include "tier0/threadtools.h"
#include "tier1/null_mutex.h"

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
#define MEM_ALLOC_D3D_CREDIT()	MEM_ALLOC_CREDIT_("D3D:" __FILE__)
#define BEGIN_D3D_ALLOCATION()	MemAlloc_PushAllocDbgInfo("D3D:" __FILE__, __LINE__)
#define END_D3D_ALLOCATION()	MemAlloc_PopAllocDbgInfo()


//-----------------------------------------------------------------------------
// Threading
//-----------------------------------------------------------------------------
extern bool g_bUseShaderMutex;

//#define USE_SHADER_DISALLOW 1
//#define STRICT_MT_SHADERAPI 1

#if defined(_DEBUG)
#if !defined(STRICT_MT_SHADERAPI)
#define UNCONDITIONAL_MT_SHADERAPI 1
#endif
#else
#if !defined(STRICT_MT_SHADERAPI) && !defined(UNCONDITIONAL_MT_SHADERAPI)
#define ST_SHADERAPI 1
#endif
#endif


#if defined(ST_SHADERAPI)
typedef ks::null_mutex CShaderMutex;
#elif defined(UNCONDITIONAL_MT_SHADERAPI)
typedef std::recursive_mutex CShaderMutex;
#else
class CShaderMutex
{
public:
	void lock()			{ if ( g_bUseShaderMutex ) m_mutex.lock(); }
	void unlock()		{ if ( g_bUseShaderMutex ) m_mutex.unlock(); }
	bool try_lock()		{ return g_bUseShaderMutex ? m_mutex.try_lock() : true; }

private:
	std::recursive_mutex m_mutex;
};
#endif

extern CShaderMutex g_ShaderMutex;

extern bool g_bShaderAccessDisallowed;

#ifdef USE_SHADER_DISALLOW
#define TestShaderPermission() do { if ( (!g_bUseShaderMutex || g_ShaderMutex.GetDepth() == 0) && g_bShaderAccessDisallowed ) { ExecuteOnce( DebuggerBreakIfDebugging() ); } } while (0)
#define LOCK_SHADERAPI() TestShaderPermission(); std::scoped_lock shaderLock( g_ShaderMutex )
#define LockShaderMutex() TestShaderPermission(); g_ShaderMutex.lock();
#define UnlockShaderMutex() TestShaderPermission(); g_ShaderMutex.unlock();
#else
#define TestShaderPermission() ((void)0)
#define LOCK_SHADERAPI() ((void)0)
#define LockShaderMutex() ((void)0)
#define UnlockShaderMutex() ((void)0)
#endif

#endif // SHADERAPI_GLOBAL_H
