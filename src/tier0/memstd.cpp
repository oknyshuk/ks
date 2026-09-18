//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"


#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)


#include <malloc.h>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "mem_helpers.h"
#include "memstd.h"
#include "tier0/minidump.h"

#define DEF_REGION 0


// Record a list of memory callbacks for printing information
// about non-heap memory.
// Allow a fixed maximum number of memory callbacks. We can't use
// CUtlVector or other classes so a fixed maximum is necessary.
IMemoryInfo* s_MemoryInfoCallbacks[100];
static size_t s_nMemoryInfoCallbacks;
// Don't modify the MemoryInfoCallbacks without acquiring this mutex
static CThreadMutex s_callbackMutex;

void AddMemoryInfoCallback( IMemoryInfo* pMemoryInfo )
{
	CAutoLock locker( s_callbackMutex );
	// This is O(n^2) but that's okay because n is just 10-20
	for ( size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		if ( s_MemoryInfoCallbacks[ i ] == pMemoryInfo )
		{
			Assert( !"This pointer has already been added!" );
		}
	}

	if ( s_nMemoryInfoCallbacks < ARRAYSIZE( s_MemoryInfoCallbacks ) )
	{
		s_MemoryInfoCallbacks[ s_nMemoryInfoCallbacks ] = pMemoryInfo;
		++s_nMemoryInfoCallbacks;
	}
}

void RemoveMemoryInfoCallback( IMemoryInfo* pMemoryInfo )
{
	CAutoLock locker( s_callbackMutex );
	for ( size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		if ( s_MemoryInfoCallbacks[ i ] == pMemoryInfo )
		{
			// Copy the last pointer into this slot and then decrement
			// the count of how many callbacks we have.
			s_MemoryInfoCallbacks[ i ] = s_MemoryInfoCallbacks[ s_nMemoryInfoCallbacks - 1 ];
			--s_nMemoryInfoCallbacks;
			return;
		}
	}
	Assert( !"Tried removing a callback that wasn't there!" );
}

// Dump a summary of all of the non-heap memory blocks that have been
// registered with AddMemoryInfoCallback.
void DumpMemoryInfoStats()
{
	CAutoLock locker( s_callbackMutex );
	size_t nTotalAllocatedBytes = 0;
	size_t nTotalPeakBytes = 0;
	size_t nTotalCommittedBytes = 0;
	size_t nTotalReservedBytes = 0;

	const double MB = 1024.0 * 1024.0;

	for ( size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		IMemoryInfo* pMemoryInfo = s_MemoryInfoCallbacks[ i ];
		nTotalAllocatedBytes += pMemoryInfo->GetAllocatedBytes();
		nTotalPeakBytes += pMemoryInfo->GetHighestBytes();
		nTotalCommittedBytes += pMemoryInfo->GetCommittedBytes();
		nTotalReservedBytes += pMemoryInfo->GetReservedBytes();

		const char* name = pMemoryInfo->GetMemoryName();
		if ( !name )
		{
			name = "Unknown memory";
		}
		if ( pMemoryInfo->GetReservedBytes() != 0 )
		{
			Msg( "%-40s: %4.1f MB allocated (%4.1f MB peak), %4.1f MB committed, %4.1f MB reserved\n",
						name,
						pMemoryInfo->GetAllocatedBytes() / MB,
						pMemoryInfo->GetHighestBytes() / MB,
						pMemoryInfo->GetCommittedBytes() / MB,
						pMemoryInfo->GetReservedBytes() / MB );
		}
	}

	Msg( "%-40s: %4.1f MB allocated (%4.1f MB peak), %4.1f MB committed, %4.1f MB reserved\n",
				"Extra memory totals",
				nTotalAllocatedBytes / MB, nTotalPeakBytes / MB,
				nTotalCommittedBytes / MB, nTotalReservedBytes / MB );
}

#define malloc_internal( region, bytes) malloc(bytes)
#define realloc_internal realloc
#define free_internal free
#define msize_internal malloc_usable_size

#define PROFILE_ALLOC( name ) ((void)0)
#define PrintAllocTimes() ((void)0)



//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )


static CStdMemAlloc s_StdMemAlloc CONSTRUCT_EARLY;

IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;
void SetAllocatorObject( IMemAlloc* pAllocator )
{
	g_pMemAlloc = pAllocator;
}

CStdMemAlloc::CStdMemAlloc()
:	m_pfnFailHandler( DefaultFailHandler ),
	m_sMemoryAllocFailed( (size_t)0 )
{
}



//-----------------------------------------------------------------------------
// Internal versions
//-----------------------------------------------------------------------------


void *CStdMemAlloc::InternalAlloc( int region, size_t nSize )
{
	PROFILE_ALLOC(Malloc);

	void *pMem = malloc_internal( region, nSize );
	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
		return nullptr;
	}

	ApplyMemoryInitializations( pMem, nSize );
	return pMem;
}


void *CStdMemAlloc::InternalRealloc( void *pMem, size_t nSize )
{
	if ( !pMem )
	{
		return RegionAlloc( DEF_REGION, nSize );
	}

	PROFILE_ALLOC(Realloc);

	void *pRet = realloc_internal( pMem, nSize );
	if ( !pRet )
	{
		SetCRTAllocFailed( nSize );
	}

	return pRet;
}


void CStdMemAlloc::InternalFree( void *pMem )
{
	if ( !pMem )
	{
		return;
	}

	PROFILE_ALLOC(Free);

	free_internal( pMem );
}

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CStdMemAlloc::Alloc( size_t nSize )
{
	return CStdMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize )
{
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

void  CStdMemAlloc::Free( void *pMem )
{
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

void  CStdMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Region support
//-----------------------------------------------------------------------------
void *CStdMemAlloc::RegionAlloc( int region, size_t nSize ) 
{
	return CStdMemAlloc::InternalAlloc( region, nSize );
}

void *CStdMemAlloc::RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalAlloc( region, nSize );
}


//-----------------------------------------------------------------------------
// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
//-----------------------------------------------------------------------------
size_t CStdMemAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return CalcHeapUsed();

	return msize_internal( pMem );
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CStdMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
}

void CStdMemAlloc::PopAllocDbgInfo()
{
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
int32 CStdMemAlloc::CrtSetBreakAlloc( int32 lNewBreakAlloc )
{
	return 0;
}

int CStdMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
	return 0;
}

int CStdMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
	return 1;
}

int CStdMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	return 1;
}

int CStdMemAlloc::CrtCheckMemory( void )
{
	return 1;
}

int CStdMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
	return 0;
}

void CStdMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
}

// FIXME: Remove when we have our own allocator
void* CStdMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
	return 0;
}

void* CStdMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
	return 0;
}

int CStdMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
	return 0;
}

int CStdMemAlloc::heapchk()
{
	return 1;
}

void CStdMemAlloc::DumpStats() 
{ 
	DumpStatsFileBase( "memstats" );
}

void CStdMemAlloc::DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat )
{
}

IVirtualMemorySection * CStdMemAlloc::AllocateVirtualMemorySection( size_t numMaxBytes )
{
	return nullptr;
}

size_t CStdMemAlloc::ComputeMemoryUsedBy( char const *pchSubStr )
{
	return 0;//dbg heap only.
}

static inline size_t ExtraDevkitMemory( void )
{
	return 0;
}

void CStdMemAlloc::GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;
}

#define MAX_GENERIC_MEMORY_STATS 64
GenericMemoryStat_t g_MemStats[MAX_GENERIC_MEMORY_STATS];
int g_nMemStats = 0;
static inline int AddGenericMemoryStat( const char *name, int value )
{
	Assert( g_nMemStats < MAX_GENERIC_MEMORY_STATS );
	if ( g_nMemStats < MAX_GENERIC_MEMORY_STATS )
	{
		g_MemStats[ g_nMemStats ].name  = name;
		g_MemStats[ g_nMemStats ].value = value;
		g_nMemStats++;
	}
	return g_nMemStats;
}

int CStdMemAlloc::GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
{
	if ( !ppMemoryStats )
		return 0;
	g_nMemStats = 0;



	*ppMemoryStats = &g_MemStats[0];
	return g_nMemStats;
}



MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	MemAllocFailHandler_t pfnPrevious = m_pfnFailHandler;
	m_pfnFailHandler = pfnMemAllocFailHandler;
	return pfnPrevious;
}

size_t CStdMemAlloc::DefaultFailHandler( size_t nBytes )
{
	return 0;
}

void CStdMemAlloc::SetStatsExtraInfo( const char *pMapName, const char *pComment )
{
}

// Frees go straight to the underlying allocator, so there is nothing of our own
// to compact. Kept as no-ops because IMemAlloc exposes them and the engine calls
// CompactHeap() on level transitions. Deliberately not malloc_trim(): with an
// allocator swapped in that would trim a heap we are not allocating from.
void CStdMemAlloc::CompactHeap()
{
}

void CStdMemAlloc::CompactIncremental()
{
}

void CStdMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	DebuggerBreakIfDebugging();

	char buffer[256];
	_snprintf( buffer, sizeof( buffer ), "***** OUT OF MEMORY! attempted allocation size: %u ****\n", nSize );

	printf( "%s\n", buffer );
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
		Plat_ExitProcess( EXIT_FAILURE );
	}
}

size_t CStdMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}


#endif // STEAM
