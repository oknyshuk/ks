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
extern "C" void WriteMiniDump( void	);
inline void VPurecallHandler()
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

#define __cdecl

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

#endif // NO_MEMOVERRIDE_NEW_DELETE

#endif // !STEAM && !NO_MALLOC_OVERRIDE

