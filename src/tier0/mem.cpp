// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Memory allocation!

import ks.core.mem;
import ks.core.memalloc;
import ks.core.minidump;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;

// First include standard libraries
import std;


//#include <malloc.h>


enum 
{
	MAX_STACK_DEPTH = 32
};

static uint8 *s_pBuf = nullptr;
static int s_pBufStackDepth[MAX_STACK_DEPTH];
static int s_nBufDepth = -1;
static int s_nBufCurSize = 0;
static int s_nBufAllocSize = 0;

//-----------------------------------------------------------------------------
// Other DLL-exported methods for particular kinds of memory
//-----------------------------------------------------------------------------
void *MemAllocScratch( int nMemSize )
{	
	// Minimally allocate 1M scratch
	if (s_nBufAllocSize < s_nBufCurSize + nMemSize)
	{
		s_nBufAllocSize = s_nBufCurSize + nMemSize;
		if (s_nBufAllocSize < 2 * 1024)
		{
			s_nBufAllocSize = 2 * 1024;
		}

		if (s_pBuf)
		{
			s_pBuf = (uint8*)std::realloc( s_pBuf, s_nBufAllocSize );
			contract_assert( s_pBuf );	
		}
		else
		{
			s_pBuf = (uint8*)std::malloc( s_nBufAllocSize );
		}
	}

	int nBase = s_nBufCurSize;
	s_nBufCurSize += nMemSize;
	++s_nBufDepth;
	contract_assert( s_nBufDepth < MAX_STACK_DEPTH );
	s_pBufStackDepth[s_nBufDepth] = nMemSize;

	return &s_pBuf[nBase];
}

void MemFreeScratch()
{
	contract_assert( s_nBufDepth >= 0 );
	s_nBufCurSize -= s_pBufStackDepth[s_nBufDepth];
	--s_nBufDepth;
}

void ZeroMemory( void *mem, std::size_t length )
{
	std::memset( mem, 0x0, length );
}

void MemOutOfMemory( std::size_t nBytesAttempted )
{
	if ( Plat_IsInDebugSession() )
	{
		DebuggerBreak();
	}
	else
	{
		WriteMiniDump();
		Plat_ExitProcess( 1 );
	}
}


void *MemAllocAligned( std::size_t size, std::size_t align )
{
	return MemAlloc_AllocAlignedUnattributed( size, align );
}

void *MemAllocAlignedFileLine( std::size_t size, std::size_t align, const char *pszFile, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( size, align, pszFile, nLine );
}

void MemFreeAligned( void *pMemBlock )
{
	MemAlloc_FreeAligned( pMemBlock );
}

void MemFreeAlignedFileLine( void *pMemBlock, const char *pszFile, int nLine )
{
	MemAlloc_FreeAligned( pMemBlock, pszFile, nLine );
}
