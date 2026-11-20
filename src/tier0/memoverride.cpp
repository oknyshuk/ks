// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator

import std;
import ks.core.memalloc;
import ks.core.minidump;
import ks.core.platform;
import ks.platform;
import ks.simd;
inline void VPurecallHandler()
{
	DebuggerBreakIfDebugging();	// give the debugger a chance to catch first
	WriteMiniDump();
	Plat_ExitProcess( 1 );
}

// set OSX/Linux pure virtual handler
extern "C" void __cxa_pure_virtual() { VPurecallHandler(); }


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

namespace
{
	inline void *AllocMem( std::size_t nSize ) noexcept
	{
		return MemAlloc_Alloc( nSize );
	}

	// IMemAlloc only exposes AllocAlign when MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	// is set, so aligned requests take the over-allocate-and-stash-the-base-pointer
	// path instead.
	inline void *AllocMemAligned( std::size_t nSize, std::size_t nAlign ) noexcept
	{
		return MemAlloc_AllocAlignedUnattributed( nSize, nAlign );
	}

	inline void FreeMem( void *pMem ) noexcept
	{
		if ( pMem )
		{
			g_pMemAlloc->Free( pMem );
		}
	}
}

void *operator new( std::size_t nSize ) { return AllocMem( nSize ); }
void *operator new[]( std::size_t nSize ) { return AllocMem( nSize ); }
void *operator new( std::size_t nSize, const std::nothrow_t & ) noexcept { return AllocMem( nSize ); }
void *operator new[]( std::size_t nSize, const std::nothrow_t & ) noexcept { return AllocMem( nSize ); }

void *operator new( std::size_t nSize, std::align_val_t nAlign ) { return AllocMemAligned( nSize, (std::size_t)nAlign ); }
void *operator new[]( std::size_t nSize, std::align_val_t nAlign ) { return AllocMemAligned( nSize, (std::size_t)nAlign ); }
void *operator new( std::size_t nSize, std::align_val_t nAlign, const std::nothrow_t & ) noexcept { return AllocMemAligned( nSize, (std::size_t)nAlign ); }
void *operator new[]( std::size_t nSize, std::align_val_t nAlign, const std::nothrow_t & ) noexcept { return AllocMemAligned( nSize, (std::size_t)nAlign ); }

void operator delete( void *pMem ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem ) noexcept { FreeMem( pMem ); }
void operator delete( void *pMem, std::size_t ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem, std::size_t ) noexcept { FreeMem( pMem ); }
void operator delete( void *pMem, const std::nothrow_t & ) noexcept { FreeMem( pMem ); }
void operator delete[]( void *pMem, const std::nothrow_t & ) noexcept { FreeMem( pMem ); }

void operator delete( void *pMem, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete( void *pMem, std::size_t, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, std::size_t, std::align_val_t ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete( void *pMem, std::align_val_t, const std::nothrow_t & ) noexcept { MemAlloc_FreeAligned( pMem ); }
void operator delete[]( void *pMem, std::align_val_t, const std::nothrow_t & ) noexcept { MemAlloc_FreeAligned( pMem ); }
