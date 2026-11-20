// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: 						ZONE MEMORY ALLOCATION
// There is never any space between memblocks, and there will never be two
// contiguous free memblocks.
// The rover can be left pointing at a non-empty block
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.

import ks.core.dbg;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.map_ops;
import ks.tier1.memstack;
#include "zone.h"
#include "host.h"
import std;
#include "datacache/idatacache.h"
#include "tier3/tier3.h"

constexpr auto KB = (1024);
#define MB (1024*1024)

constexpr auto MINIMUM_WIN_MEMORY = (48*MB); // FIXME: copy from sys_dll.cpp, find a common header at some point

// PORTAL 2 SHIPPING CHANGE
// We're sacrificing a little perf (~1% in pathological case) for 12 MB of memory back

CMemoryStack g_HunkMemoryStack;

const int HUNK_COMMIT_FLOOR = ( 40 )*MB;


ConVar hunk_track_allocation_types( "hunk_track_allocation_types", "1", FCVAR_CHEAT );
ks::idict< int > g_HunkAllocationsByName;
struct hunkalloc_t { const char *name; int size; };
int HunkAllocSortFunc( const void *a, const void *b )
{
	const hunkalloc_t *A = (const hunkalloc_t *)a, *B = (const hunkalloc_t *)b;
	return ( A->size > B->size ) ? -1 : +1;
}
static void hunk_print_allocations( const CCommand &args );
static ConCommand hunk_print_allocations_command( "hunk_print_allocations", hunk_print_allocations, "", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void hunk_print_allocations( const CCommand &args )
{
	Msg( "Hunk allocations:\n");
	hunkalloc_t *items = new hunkalloc_t[ g_HunkAllocationsByName.size() ];
	int numItems = 0, total = 0;
	for ( const auto &[name, size] : g_HunkAllocationsByName )
	{
		if ( !size )
			continue;
		hunkalloc_t item = { name.c_str(), size };
		items[numItems++] = item;
		total += item.size;
	}
	std::qsort( items, numItems, sizeof( hunkalloc_t ), HunkAllocSortFunc );
	Msg( "    %55s:%10d\n", "TOTAL:", total );
	for ( int i = 0; i < numItems; i++ )
	{
		Msg( "    %55s:%10d\n", items[i].name, items[i].size );
	}
	delete [] items;
	

}



static int GetTargetCacheSize()
{
	int nMemLimit = host_parms.memsize - Hunk_Size();
	if ( nMemLimit < 0x100000 )
	{
		nMemLimit = 0x100000;
	}
	return nMemLimit;
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName(int size, const char *name, bool bClear)
{
	if ( hunk_track_allocation_types.GetBool() )
	{
		if ( !name )
		{
			name = "unknown";
		}

		auto it = g_HunkAllocationsByName.find( name );
		if ( it == g_HunkAllocationsByName.end() )
		{
			g_HunkAllocationsByName.emplace( name, size );
		}
		else
		{
			it->second += size;
		}
	}
	void *p = g_HunkMemoryStack.Alloc( size, bClear );
	if ( p )
		return p;
	Error( "Engine hunk overflow!\n" );
	return nullptr;
}

/*
===================
Hunk_Alloc
===================
*/

int	Hunk_LowMark(void)
{
	return (int)( g_HunkMemoryStack.GetCurrentAllocPoint() );
}

void Hunk_FreeToLowMark(int mark)
{
	contract_assert( mark < g_HunkMemoryStack.GetSize() );
	g_HunkMemoryStack.FreeToAllocPoint( mark, false );
	g_HunkAllocationsByName.clear();
}

int Hunk_MallocSize()
{
	return g_HunkMemoryStack.GetSize();
}

int Hunk_Size()
{
	return g_HunkMemoryStack.GetUsed();
}

void Hunk_Print()
{
	Msg( "Total used memory:      %d\n", Hunk_Size() );
	Msg( "Total committed memory: %d\n", Hunk_MallocSize() );
}



void Hunk_OnMapStart( int nEstimatedBytes )
{
	int nToCommit = std::max( nEstimatedBytes, HUNK_COMMIT_FLOOR );

	CMemoryStack *pStack = &g_HunkMemoryStack;

	if ( developer.GetBool() )
	{
		DevMsg( "Hunk_OnMapStart: %d\n", nToCommit );
	}
	if ( nToCommit > 0 )
	{
		pStack->CommitSize( nToCommit );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Init( void )
{
	int nMaxBytes = 128*MB;
	const int commitIncrement = 64*KB;
	const int nInitialCommit = std::min( HUNK_COMMIT_FLOOR, nMaxBytes );
	while ( !g_HunkMemoryStack.Init( "g_HunkMemoryStack", nMaxBytes, commitIncrement, nInitialCommit ) )	 
	{
		Warning( "Unable to allocate %d MB of memory, trying %d MB instead\n", nMaxBytes, nMaxBytes/2 );
		nMaxBytes /= 2;
		if ( nMaxBytes < MINIMUM_WIN_MEMORY )
		{
			Error( "Failed to allocate minimum memory requirement for game (%d MB)\n", MINIMUM_WIN_MEMORY/MB);
		}
	}
	g_pDataCache->SetSize( GetTargetCacheSize() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Shutdown( void )
{
	g_HunkMemoryStack.FreeAll();

	// This disconnects the engine data cache
	g_pDataCache->SetSize( 0 );
}
