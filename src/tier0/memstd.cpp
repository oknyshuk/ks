// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Memory allocation!

import ks.core;
import std;

constexpr auto DEF_REGION = 0;

//-----------------------------------------------------------------------------
// CStdMemAlloc is a thin adapter over the C library allocator (or whatever
// allocator the executable linked in ahead of it), plus the accounting and
// out-of-memory policy the engine expects from IMemAlloc. Valve's small block
// heap and the vendored dlmalloc are gone; they were already compiled out.
//-----------------------------------------------------------------------------
class CStdMemAlloc : public IMemAlloc
{
public:
	CStdMemAlloc();

	// Internal versions
	void *InternalAlloc( int region, std::size_t nSize );
	void *InternalRealloc( void *pMem, std::size_t nSize );
	void  InternalFree( void *pMem );

	// Release versions
	virtual void *Alloc( std::size_t nSize );
	virtual void *Realloc( void *pMem, std::size_t nSize );
	virtual void  Free( void *pMem );
    virtual void *Expand_NoLongerSupported( void *pMem, std::size_t nSize );

	// Debug versions
    virtual void *Alloc( std::size_t nSize, const char *pFileName, int nLine );
    virtual void *Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine );
    virtual void  Free( void *pMem, const char *pFileName, int nLine );
    virtual void *Expand_NoLongerSupported( void *pMem, std::size_t nSize, const char *pFileName, int nLine );

	virtual void *RegionAlloc( int region, std::size_t nSize );
	virtual void *RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine );

	// Returns size of a particular allocation
	virtual std::size_t GetSize( void *pMem );

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine );
    virtual void PopAllocDbgInfo();

	virtual int32 CrtSetBreakAlloc( int32 lNewBreakAlloc );
	virtual	int CrtSetReportMode( int nReportType, int nReportMode );
	virtual int CrtIsValidHeapPointer( const void *pMem );
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access );
	virtual int CrtCheckMemory( void );
	virtual int CrtSetDbgFlag( int nNewFlag );
	virtual void CrtMemCheckpoint( _CrtMemState *pState );
	void* CrtSetReportFile( int nRptType, void* hFile );
	void* CrtSetReportHook( void* pfnNewHook );
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg );
	virtual int heapchk();

	virtual void DumpStats();
	virtual void DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat = FORMAT_TEXT ) override;
	virtual std::size_t ComputeMemoryUsedBy( char const *pchSubStr );
	virtual void GlobalMemoryStatus( std::size_t *pUsedMemory, std::size_t *pFreeMemory );

	virtual bool IsDebugHeap() { return false; }

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) {}
	virtual void RegisterAllocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) {}
	virtual void RegisterDeallocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) {}

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void OutOfMemory( std::size_t nBytesAttempted = 0 ) { SetCRTAllocFailed( nBytesAttempted ); }

	virtual IVirtualMemorySection * AllocateVirtualMemorySection( std::size_t numMaxBytes );

	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats );

	virtual void CompactHeap();
	virtual void CompactIncremental(); 

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler );
	std::size_t CallAllocFailHandler( std::size_t nBytes ) { return (*m_pfnFailHandler)( nBytes); }

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	static std::size_t DefaultFailHandler( std::size_t );
	void DumpBlockStats( void *p ) {}

	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment );

	virtual std::size_t MemoryAllocFailed();

	void		SetCRTAllocFailed( std::size_t nMemSize );

	MemAllocFailHandler_t m_pfnFailHandler;
	std::size_t				m_sMemoryAllocFailed;
};


// Record a list of memory callbacks for printing information
// about non-heap memory.
// Allow a fixed maximum number of memory callbacks. We can't use
// CUtlVector or other classes so a fixed maximum is necessary.
IMemoryInfo* s_MemoryInfoCallbacks[100];
static std::size_t s_nMemoryInfoCallbacks;
// Don't modify the MemoryInfoCallbacks without acquiring this mutex
static std::recursive_mutex s_callbackMutex;

void AddMemoryInfoCallback( IMemoryInfo* pMemoryInfo )
{
	std::scoped_lock lock( s_callbackMutex );
	// This is O(n^2) but that's okay because n is just 10-20
	for ( std::size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
	{
		if ( s_MemoryInfoCallbacks[ i ] == pMemoryInfo )
		{
			contract_assert( !"This pointer has already been added!" );
		}
	}

	if ( s_nMemoryInfoCallbacks < std::size( s_MemoryInfoCallbacks ) )
	{
		s_MemoryInfoCallbacks[ s_nMemoryInfoCallbacks ] = pMemoryInfo;
		++s_nMemoryInfoCallbacks;
	}
}

void RemoveMemoryInfoCallback( IMemoryInfo* pMemoryInfo )
{
	std::scoped_lock lock( s_callbackMutex );
	for ( std::size_t i = 0; i < s_nMemoryInfoCallbacks; ++i )
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

template <typename T1, typename T2> [[gnu::always_inline]] inline auto malloc_internal( T1 region, T2 bytes ) { return std::malloc(bytes); }


//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------


static CStdMemAlloc s_StdMemAlloc [[gnu::init_priority(101)]];

IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;
void SetAllocatorObject( IMemAlloc* pAllocator )
{
	g_pMemAlloc = pAllocator;
}

CStdMemAlloc::CStdMemAlloc()
:	m_pfnFailHandler( DefaultFailHandler ),
	m_sMemoryAllocFailed( (std::size_t)0 )
{
}


//-----------------------------------------------------------------------------
// Internal versions
//-----------------------------------------------------------------------------


void *CStdMemAlloc::InternalAlloc( int region, std::size_t nSize )
{

	void *pMem = malloc_internal( region, nSize );
	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
		return nullptr;
	}

	return pMem;
}


void *CStdMemAlloc::InternalRealloc( void *pMem, std::size_t nSize )
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


void CStdMemAlloc::InternalFree( void *pMem )
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

void *CStdMemAlloc::Alloc( std::size_t nSize )
{
	return CStdMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, std::size_t nSize )
{
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

void  CStdMemAlloc::Free( void *pMem )
{
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, std::size_t nSize )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc( std::size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalAlloc( DEF_REGION, nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

void  CStdMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, std::size_t nSize, const char *pFileName, int nLine )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Region support
//-----------------------------------------------------------------------------
void *CStdMemAlloc::RegionAlloc( int region, std::size_t nSize ) 
{
	return CStdMemAlloc::InternalAlloc( region, nSize );
}

void *CStdMemAlloc::RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::InternalAlloc( region, nSize );
}


//-----------------------------------------------------------------------------
// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
//-----------------------------------------------------------------------------
std::size_t CStdMemAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return 0;

	return ks::platform::allocation_size( pMem );
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

IVirtualMemorySection * CStdMemAlloc::AllocateVirtualMemorySection( std::size_t numMaxBytes )
{
	return nullptr;
}

std::size_t CStdMemAlloc::ComputeMemoryUsedBy( char const *pchSubStr )
{
	return 0;//dbg heap only.
}

static inline std::size_t ExtraDevkitMemory( void )
{
	return 0;
}

void CStdMemAlloc::GlobalMemoryStatus( std::size_t *pUsedMemory, std::size_t *pFreeMemory )
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

std::size_t CStdMemAlloc::DefaultFailHandler( std::size_t nBytes )
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

void CStdMemAlloc::SetCRTAllocFailed( std::size_t nSize )
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

std::size_t CStdMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}
