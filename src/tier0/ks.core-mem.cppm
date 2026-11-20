// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Memory allocation!

export module ks.core.mem;

import std;

export extern "C++" {

//-----------------------------------------------------------------------------
// DLL-exported methods for particular kinds of memory
//-----------------------------------------------------------------------------
void *MemAllocScratch( int nMemSize );
void MemFreeScratch();

void MemOutOfMemory( std::size_t nBytesAttempted );

void *MemAllocAligned( std::size_t size, std::size_t align );
void *MemAllocAlignedFileLine( std::size_t size, std::size_t align, const char *pszFile, int nLine );
void MemFreeAligned( void *pMemBlock );
void MemFreeAlignedFileLine( void *pMemBlock, const char *pszFile, int nLine );

struct aligned_tmp_t
{
	// empty base class
};

// template here to allow adding alignment at levels of hierarchy that aren't the base
template< int bytesAlignment = 16, class T = aligned_tmp_t >
class CAlignedNewDelete : public T
{
public:
	void *operator new( std::size_t nSize )
	{
		return MemAllocAligned( nSize, bytesAlignment );
	}

	void operator delete( void *pData )
	{
		MemFreeAligned( pData );
	}
};

}
