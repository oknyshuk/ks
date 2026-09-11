//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator
//
// $NoKeywords: $
//=============================================================================//


#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#define AVOID_INCLUDING_ALGORITHM

#undef PROTECTED_THINGS_ENABLE   // allow use of _vsnprintf

#include <stdlib.h>
#include "platform.h"
extern "C" void __cdecl WriteMiniDump( void	);
inline void __cdecl VPurecallHandler()
{
	DebuggerBreakIfDebugging();	// give the debugger a chance to catch first
	WriteMiniDump();
	Plat_ExitProcess( EXIT_FAILURE );
}

// set OSX/Linux pure virtual handler
extern "C" void __cxa_pure_virtual() { VPurecallHandler(); }

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include <string.h>
#include <stdio.h>
#include "memdbgoff.h"

#define __cdecl

#if defined(USE_MEM_DEBUG)
#pragma optimize( "", off )
#define inline
#endif

const char *g_pszModule = MKSTRING( MEMOVERRIDE_MODULE );
inline void *AllocUnattributed( size_t nSize )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	return MemAlloc_Alloc(nSize);
#else
	return MemAlloc_Alloc(nSize, ::g_pszModule, 0);
#endif
}

inline void *ReallocUnattributed( void *pMem, size_t nSize )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	return g_pMemAlloc->Realloc(pMem, nSize);
#else
	return g_pMemAlloc->Realloc(pMem, nSize, ::g_pszModule, 0);
#endif
}

#undef inline


//-----------------------------------------------------------------------------
// Standard functions in the CRT that we're going to override to call our allocator
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Prevents us from using an inappropriate new or delete method,
// ensures they are here even when linking against debug or release static libs
//-----------------------------------------------------------------------------
#ifndef NO_MEMOVERRIDE_NEW_DELETE

void *__cdecl operator new( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine );
}

void *__cdecl operator new[] ( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new[] ( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}


void __cdecl operator delete( void *pMem ) throw()
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

void __cdecl operator delete[] ( void *pMem ) throw()
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}
#endif


//-----------------------------------------------------------------------------
// Override some debugging allocation methods in MSVC
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!
//-----------------------------------------------------------------------------
#define AttribIfCrt()


extern "C"
{
	
void *__cdecl _nh_malloc_dbg( size_t nSize, int nFlag, int nBlockUse,
								const char *pFileName, int nLine )
{
	AttribIfCrt();
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}

void *__cdecl _malloc_dbg( size_t nSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}


void *__cdecl _calloc_dbg( size_t nNum, size_t nSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	void *pMem = MemAlloc_Alloc(nSize * nNum, pFileName, nLine);
	memset(pMem, 0, nSize * nNum);
	return pMem;
}

void *__cdecl _realloc_dbg( void *pMem, size_t nNewSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	return g_pMemAlloc->Realloc(pMem, nNewSize, pFileName, nLine);
}

void *__cdecl _expand_dbg( void *pMem, size_t nNewSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	Assert( 0 );
	return NULL;
}

void __cdecl _free_dbg( void *pMem, int nBlockUse )
{
	AttribIfCrt();
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

size_t __cdecl _msize_dbg( void *pMem, int nBlockUse )
{
	Assert( "_msize_dbg unsupported" );
	return 0;
}



} // end extern "C"


//-----------------------------------------------------------------------------
// Override some the _CRT debugging allocation methods in MSVC
//-----------------------------------------------------------------------------

// Most files include this file, so when it's used it adds an extra .ValveDbg section,
// to help identify debug binaries.


// Extras added prevent dbgheap.obj from being included - DAL

#endif // _WIN32


