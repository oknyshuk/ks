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
#include <new>
#include <string.h>
#include <stdio.h>
#include "memdbgoff.h"

#define __cdecl

#if defined(USE_MEM_DEBUG)
#pragma optimize( "", off )
#define inline
#endif

//-----------------------------------------------------------------------------
// Every replaceable global operator, so that a new/delete pair can never
// straddle two allocators.
//
// The sized (C++14) and aligned (C++17) forms are the ones that matter: the
// compiler emits them routinely, and left to libstdc++ they release with the C
// library's free() while the matching new went through g_pMemAlloc. That is
// harmless only for as long as the backend *is* the C library's allocator.
//
// Out of memory is g_pMemAlloc's problem: its fail handler dumps and exits.
//-----------------------------------------------------------------------------
#ifndef NO_MEMOVERRIDE_NEW_DELETE

namespace
{
	inline void *AllocMem( size_t nSize ) noexcept
	{
		return MemAlloc_Alloc( nSize );
	}

	// IMemAlloc only exposes AllocAlign when MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	// is set, so aligned requests take the over-allocate-and-stash-the-base-pointer
	// path instead.
	inline void *AllocMemAligned( size_t nSize, size_t nAlign ) noexcept
	{
		return MemAlloc_AllocAligned( nSize, nAlign );
	}

	inline void FreeMem( void *pMem ) noexcept
	{
		if ( pMem )
		{
			g_pMemAlloc->Free( pMem );
		}
	}
}

void *operator new( size_t nSize ) { return AllocMem( nSize ); }
void *operator new[]( size_t nSize ) { return AllocMem( nSize ); }
void *operator new( size_t nSize, const std::nothrow_t & ) noexcept { return AllocMem( nSize ); }
void *operator new[]( size_t nSize, const std::nothrow_t & ) noexcept { return AllocMem( nSize ); }

void *operator new( size_t nSize, std::align_val_t nAlign ) { return AllocMemAligned( nSize, (size_t)nAlign ); }
void *operator new[]( size_t nSize, std::align_val_t nAlign ) { return AllocMemAligned( nSize, (size_t)nAlign ); }
void *operator new( size_t nSize, std::align_val_t nAlign, const std::nothrow_t & ) noexcept { return AllocMemAligned( nSize, (size_t)nAlign ); }
void *operator new[]( size_t nSize, std::align_val_t nAlign, const std::nothrow_t & ) noexcept { return AllocMemAligned( nSize, (size_t)nAlign ); }

void operator delete( void *pMem ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem ) noexcept { FreeMem( pMem ); }
void operator delete( void *pMem, size_t ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem, size_t ) noexcept { FreeMem( pMem ); }
void operator delete( void *pMem, const std::nothrow_t & ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem, const std::nothrow_t & ) noexcept { FreeMem( pMem ); }

void operator delete( void *pMem, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete( void *pMem, size_t, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, size_t, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete( void *pMem, std::align_val_t, const std::nothrow_t & ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, std::align_val_t, const std::nothrow_t & ) noexcept { MemAlloc_FreeAligned( pMem ); }

// MSVC debug CRT placement form. Dormant -- there is no debug build type -- but
// memdbgon.h still declares it.
void *operator new( size_t nSize, int, const char *, int ) { return AllocMem( nSize ); }
void *operator new[]( size_t nSize, int, const char *, int ) { return AllocMem( nSize ); }

#endif // NO_MEMOVERRIDE_NEW_DELETE


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
	return nullptr;
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


