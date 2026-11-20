// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Memory allocation!

import ks.core.dbg;
import ks.core.memalloc;
import ks.core.minidump;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import std;

constexpr auto DEF_REGION = 0;

// Record a list of memory callbacks for printing information
// about non-heap memory.
// Allow a fixed maximum number of memory callbacks. We can't use
// CUtlVector or other classes so a fixed maximum is necessary.
struct MemoryInfoCallback_t
{
	const void *m_pOwner;
	std::optional< std::function_ref< MemoryInfo_t() const > > m_Query;
};
static MemoryInfoCallback_t s_MemoryInfoCallbacks[100];
static std::size_t s_nMemoryInfoCallbacks;
// Don't modify the MemoryInfoCallbacks without acquiring this mutex
static std::recursive_mutex s_callbackMutex;

void AddMemoryInfoCallback( const void *pOwner, std::function_ref< MemoryInfo_t() const > query )
{
	std::scoped_lock lock( s_callbackMutex );
	// This is O(n^2) but that's okay because n is just 10-20
	for ( std::size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		if ( s_MemoryInfoCallbacks[ i ].m_pOwner == pOwner )
		{
			contract_assert( !"This pointer has already been added!" );
		}
	}

	if ( s_nMemoryInfoCallbacks < std::size( s_MemoryInfoCallbacks ) )
	{
		s_MemoryInfoCallbacks[ s_nMemoryInfoCallbacks ] = { pOwner, query };
		++s_nMemoryInfoCallbacks;
	}
}

void RemoveMemoryInfoCallback( const void *pOwner )
{
	std::scoped_lock lock( s_callbackMutex );
	for ( std::size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		if ( s_MemoryInfoCallbacks[ i ].m_pOwner == pOwner )
		{
			// Copy the last pointer into this slot and then decrement
			// the count of how many callbacks we have.
			s_MemoryInfoCallbacks[ i ] = s_MemoryInfoCallbacks[ s_nMemoryInfoCallbacks - 1 ];
			--s_nMemoryInfoCallbacks;
			return;
		}
	}
	contract_assert( !"Tried removing a callback that wasn't there!" );
}

// Dump a summary of all of the non-heap memory blocks that have been
// registered with AddMemoryInfoCallback.
void DumpMemoryInfoStats()
{
	std::scoped_lock lock( s_callbackMutex );
	std::size_t nTotalAllocatedBytes = 0;
	std::size_t nTotalPeakBytes = 0;
	std::size_t nTotalCommittedBytes = 0;
	std::size_t nTotalReservedBytes = 0;

	const double MB = 1024.0 * 1024.0;

	for ( std::size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		const MemoryInfo_t info = ( *s_MemoryInfoCallbacks[ i ].m_Query )();
		nTotalAllocatedBytes += info.allocatedBytes;
		nTotalPeakBytes += info.highestBytes;
		nTotalCommittedBytes += info.committedBytes;
		nTotalReservedBytes += info.reservedBytes;

		const char* name = info.name;
		if ( !name )
		{
			name = "Unknown memory";
		}
		if ( info.reservedBytes != 0 )
		{
			Msg( "%-40s: %4.1f MB allocated (%4.1f MB peak), %4.1f MB committed, %4.1f MB reserved\n",
						name,
						info.allocatedBytes / MB,
						info.highestBytes / MB,
						info.committedBytes / MB,
						info.reservedBytes / MB );
		}
	}

	Msg( "%-40s: %4.1f MB allocated (%4.1f MB peak), %4.1f MB committed, %4.1f MB reserved\n",
				"Extra memory totals",
				nTotalAllocatedBytes / MB, nTotalPeakBytes / MB,
				nTotalCommittedBytes / MB, nTotalReservedBytes / MB );
}

template <typename T1, typename T2> [[gnu::always_inline]] inline auto malloc_internal( T1 region, T2 bytes ) { return std::malloc(bytes); }


//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------


// Constant-initialised: shared libraries that the dynamic loader initialises before our static
// initialisers (SDL's audio back ends, for one) already allocate through g_pMemAlloc.
static constinit IMemAlloc s_StdMemAlloc;

IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;



//-----------------------------------------------------------------------------
// Internal versions
//-----------------------------------------------------------------------------


void *IMemAlloc::InternalAlloc( int region, std::size_t nSize )
{

	void *pMem = malloc_internal( region, nSize );
	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
		return nullptr;
	}

	return pMem;
}


void *IMemAlloc::InternalRealloc( void *pMem, std::size_t nSize )
{
	if ( !pMem )
	{
		return RegionAlloc( DEF_REGION, nSize );
	}


	void *pRet = std::realloc( pMem, nSize );
	if ( !pRet )
	{
		SetCRTAllocFailed( nSize );
	}

	return pRet;
}


void IMemAlloc::InternalFree( void *pMem )
{
	if ( !pMem )
	{
		return;
	}


	std::free( pMem );
}

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *IMemAlloc::Alloc( std::size_t nSize )
{
	return IMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *IMemAlloc::Realloc( void *pMem, std::size_t nSize )
{
	return IMemAlloc::InternalRealloc( pMem, nSize );
}

void  IMemAlloc::Free( void *pMem )
{
	IMemAlloc::InternalFree( pMem );
}

void *IMemAlloc::Expand_NoLongerSupported( void *pMem, std::size_t nSize )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *IMemAlloc::Alloc( std::size_t nSize, const char *pFileName, int nLine )
{
	return IMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *IMemAlloc::Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine )
{
	return IMemAlloc::InternalRealloc( pMem, nSize );
}

void  IMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	IMemAlloc::InternalFree( pMem );
}

void *IMemAlloc::Expand_NoLongerSupported( void *pMem, std::size_t nSize, const char *pFileName, int nLine )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Region support
//-----------------------------------------------------------------------------
void *IMemAlloc::RegionAlloc( int region, std::size_t nSize ) 
{
	return IMemAlloc::InternalAlloc( region, nSize );
}

void *IMemAlloc::RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine )
{
	return IMemAlloc::InternalAlloc( region, nSize );
}


//-----------------------------------------------------------------------------
// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
//-----------------------------------------------------------------------------
std::size_t IMemAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return 0;

	return ks::platform::allocation_size( pMem );
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void IMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
}

void IMemAlloc::PopAllocDbgInfo()
{
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
int32 IMemAlloc::CrtSetBreakAlloc( int32 lNewBreakAlloc )
{
	return 0;
}

int IMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
	return 0;
}

int IMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
	return 1;
}

int IMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	return 1;
}

int IMemAlloc::CrtCheckMemory( void )
{
	return 1;
}

int IMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
	return 0;
}

void IMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
}

// FIXME: Remove when we have our own allocator
void* IMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
	return 0;
}

void* IMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
	return 0;
}

int IMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
	return 0;
}

int IMemAlloc::heapchk()
{
	return 1;
}

void IMemAlloc::DumpStats() 
{ 
	DumpStatsFileBase( "memstats" );
}

void IMemAlloc::DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat )
{
}

std::size_t IMemAlloc::ComputeMemoryUsedBy( char const *pchSubStr )
{
	return 0;//dbg heap only.
}

static inline std::size_t ExtraDevkitMemory( void )
{
	return 0;
}

void IMemAlloc::GlobalMemoryStatus( std::size_t *pUsedMemory, std::size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;
}

constexpr auto MAX_GENERIC_MEMORY_STATS = 64;
GenericMemoryStat_t g_MemStats[MAX_GENERIC_MEMORY_STATS];
int g_nMemStats = 0;
static inline int AddGenericMemoryStat( const char *name, int value )
{
	contract_assert( g_nMemStats < MAX_GENERIC_MEMORY_STATS );
	if ( g_nMemStats < MAX_GENERIC_MEMORY_STATS )
	{
		g_MemStats[ g_nMemStats ].name  = name;
		g_MemStats[ g_nMemStats ].value = value;
		g_nMemStats++;
	}
	return g_nMemStats;
}

int IMemAlloc::GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
{
	if ( !ppMemoryStats )
		return 0;
	g_nMemStats = 0;


	*ppMemoryStats = &g_MemStats[0];
	return g_nMemStats;
}


MemAllocFailHandler_t IMemAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	MemAllocFailHandler_t pfnPrevious = m_pfnFailHandler;
	m_pfnFailHandler = pfnMemAllocFailHandler;
	return pfnPrevious;
}

std::size_t IMemAlloc::DefaultFailHandler( std::size_t nBytes )
{
	return 0;
}

void IMemAlloc::SetStatsExtraInfo( const char *pMapName, const char *pComment )
{
}

// Frees go straight to the underlying allocator, so there is nothing of our own
// to compact. Kept as no-ops because IMemAlloc exposes them and the engine calls
// CompactHeap() on level transitions. Deliberately not malloc_trim(): with an
// allocator swapped in that would trim a heap we are not allocating from.
void IMemAlloc::CompactHeap()
{
}

void IMemAlloc::CompactIncremental()
{
}

void IMemAlloc::SetCRTAllocFailed( std::size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	DebuggerBreakIfDebugging();

	char buffer[256];
	std::snprintf( buffer, sizeof( buffer ), "***** OUT OF MEMORY! attempted allocation size: %u ****\n", nSize );

	std::printf( "%s\n", buffer );
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
		Plat_ExitProcess( 1 );
	}
}

std::size_t IMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}
