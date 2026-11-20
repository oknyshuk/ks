// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.memalloc;

import std;
import ks.platform;
import ks.core.basetypes;
import ks.core.types;
import ks.core.wchartypes;
import ks.core.color;
import ks.core.logging;
import ks.core.dbg;

export extern "C++" {

inline constexpr auto MEM_DEBUG_CLASSNAME = 1;

struct _CrtMemState;

inline constexpr auto MEMALLOC_VERSION = 1;

typedef std::size_t (*MemAllocFailHandler_t)( std::size_t );

struct GenericMemoryStat_t
{
	const char *name;
	int value;
};

// Non-heap memory statistics

/// Describes one non-heap allocation owner (currently CMemoryStack) for the mem_dump command.
struct MemoryInfo_t
{
	const char* name; // User friendly name for this stack or pool
	std::size_t allocatedBytes; // Number of bytes currently allocated
	std::size_t committedBytes; // Bytes committed -- may be greater than allocated.
	std::size_t reservedBytes; // Bytes reserved -- may be greater than committed.
	std::size_t highestBytes; // The maximum number of bytes allocated or committed.
};

// Add and remove callbacks used to get statistics on non-heap memory allocations.
// The owner pointer identifies the registration; the query borrows the owner's state,
// so the owner must remove the callback before it dies.
void AddMemoryInfoCallback( const void *pOwner, std::function_ref< MemoryInfo_t() const > query );
void RemoveMemoryInfoCallback( const void *pOwner );
// Display the memory statistics from the callbacks controlled by the above functions.
void DumpMemoryInfoStats();

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//
// There is exactly one allocator: the std/C-library backed one (memstd.cpp). Its state
// is constant-initialised so it is usable before any static initialiser runs.
//-----------------------------------------------------------------------------
class IMemAlloc
{
public:
	constexpr IMemAlloc() : m_pfnFailHandler( DefaultFailHandler ), m_sMemoryAllocFailed( 0 ) {}

	// Release versions
	void *Alloc( std::size_t nSize );
	void *Realloc( void *pMem, std::size_t nSize );
	void Free( void *pMem );
	void *Expand_NoLongerSupported( void *pMem, std::size_t nSize );

	// Debug versions
	void *Alloc( std::size_t nSize, const char *pFileName, int nLine );
	void *Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine );
	void  Free( void *pMem, const char *pFileName, int nLine );
	void *Expand_NoLongerSupported( void *pMem, std::size_t nSize, const char *pFileName, int nLine );

	inline void *IndirectAlloc( std::size_t nSize )										{ return Alloc( nSize ); }
	inline void *IndirectAlloc( std::size_t nSize, const char *pFileName, int nLine )	{ return Alloc( nSize, pFileName, nLine ); }

	// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
	std::size_t GetSize( void *pMem );

	// Force file + line information for an allocation
	void PushAllocDbgInfo( const char *pFileName, int nLine );
	void PopAllocDbgInfo();

	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	int32 CrtSetBreakAlloc( int32 lNewBreakAlloc );
	int CrtSetReportMode( int nReportType, int nReportMode );
	int CrtIsValidHeapPointer( const void *pMem );
	int CrtIsValidPointer( const void *pMem, unsigned int size, int access );
	int CrtCheckMemory( void );
	int CrtSetDbgFlag( int nNewFlag );
	void CrtMemCheckpoint( _CrtMemState *pState );

	// FIXME: Make a better stats interface
	void DumpStats();

	enum DumpStatsFormat_t
	{
		FORMAT_TEXT,
		FORMAT_HTML
	};

	void DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat = FORMAT_TEXT );
	std::size_t ComputeMemoryUsedBy( char const *pchSubStr );

	// FIXME: Remove when we have our own allocator
	void* CrtSetReportFile( int nRptType, void* hFile );
	void* CrtSetReportHook( void* pfnNewHook );
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg );

	int heapchk();

	bool IsDebugHeap() { return false; }

	void GetActualDbgInfo( const char *&pFileName, int &nLine ) {}
	void RegisterAllocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) {}
	void RegisterDeallocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) {}

	int GetVersion() { return MEMALLOC_VERSION; }

	void CompactHeap();

	// Function called when malloc fails or memory limits hit to attempt to free up memory (can come in any thread)
	MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler );
	std::size_t CallAllocFailHandler( std::size_t nBytes ) { return (*m_pfnFailHandler)( nBytes); }

	void DumpBlockStats( void * ) {}

	void SetStatsExtraInfo( const char *pMapName, const char *pComment );

	// Returns 0 if no failure, otherwise the size_t of the last requested chunk
	std::size_t MemoryAllocFailed();

	void CompactIncremental();

	void OutOfMemory( std::size_t nBytesAttempted = 0 ) { SetCRTAllocFailed( nBytesAttempted ); }

	// Region-based allocations
	void *RegionAlloc( int region, std::size_t nSize );
	void *RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine );

	// Replacement for ::GlobalMemoryStatus which accounts for unused memory in our system
	void GlobalMemoryStatus( std::size_t *pUsedMemory, std::size_t *pFreeMemory );

	// Request 'generic' memory stats (returns a list of N named values; caller should assume this list will change over time)
	int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats );

	// handles storing allocation info for coroutines
	uint32 GetDebugInfoSize() { return 0; }
	void SaveDebugInfo( void *pvDebugInfo ) { }
	void RestoreDebugInfo( const void *pvDebugInfo ) {}
	void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	static std::size_t DefaultFailHandler( std::size_t );

	void SetCRTAllocFailed( std::size_t nMemSize );

private:
	// Internal versions
	void *InternalAlloc( int region, std::size_t nSize );
	void *InternalRealloc( void *pMem, std::size_t nSize );
	void  InternalFree( void *pMem );

	MemAllocFailHandler_t m_pfnFailHandler;
	std::size_t m_sMemoryAllocFailed;
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------

extern IMemAlloc *g_pMemAlloc;

//-----------------------------------------------------------------------------

inline void *MemAlloc_Alloc( std::size_t nSize )
{ 
	return g_pMemAlloc->IndirectAlloc( nSize );
}

inline void *MemAlloc_Alloc( std::size_t nSize, const char *pFileName, int nLine )
{ 
	return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine );
}

//-----------------------------------------------------------------------------

inline bool ValueIsPowerOfTwo( std::size_t value )			// don't clash with mathlib definition
{
	return (value & ( value - 1 )) == 0;
}

inline void *MemAlloc_AllocAlignedUnattributed( std::size_t size, std::size_t align )
{

	unsigned char *pAlloc, *pResult;

	if (!ValueIsPowerOfTwo(align))
		return nullptr;

	align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

	if ( (pAlloc = (unsigned char*)MemAlloc_Alloc( sizeof(void *) + align + size ) ) == (unsigned char*)nullptr)
		return nullptr;

	pResult = (unsigned char*)( (std::size_t)(pAlloc + sizeof(void *) + align ) & ~align );
	((unsigned char**)(pResult))[-1] = pAlloc;

	return (void *)pResult;

}

inline void *MemAlloc_AllocAlignedFileLine( std::size_t size, std::size_t align, const char *pszFile, int nLine )
{

	unsigned char *pAlloc, *pResult;

	if (!ValueIsPowerOfTwo(align))
		return nullptr;

	align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

	if ( (pAlloc = (unsigned char*)MemAlloc_Alloc( sizeof(void *) + align + size, pszFile, nLine ) ) == (unsigned char*)nullptr)
		return nullptr;

	pResult = (unsigned char*)( (std::size_t)(pAlloc + sizeof(void *) + align ) & ~align );
	((unsigned char**)(pResult))[-1] = pAlloc;

	return (void *)pResult;

}

inline void *MemAlloc_ReallocAligned( void *ptr, std::size_t size, std::size_t align )
{
	if ( !ValueIsPowerOfTwo( align ) )
		return nullptr;

	// Don't change alignment between allocation + reallocation.
	if ( ( (std::size_t)ptr & ( align - 1 ) ) != 0 )
		return nullptr;

	if ( !ptr )
		return MemAlloc_AllocAlignedUnattributed( size, align );

	void *pAlloc, *pResult;

	// Figure out the actual allocation point
	pAlloc = ptr;
	pAlloc = (void *)(((std::size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));
	pAlloc = *( (void **)pAlloc );

	// See if we have enough space
	std::size_t nOffset = (std::size_t)ptr - (std::size_t)pAlloc;
	std::size_t nOldSize = g_pMemAlloc->GetSize( pAlloc );
	if ( nOldSize >= size + nOffset )
		return ptr;

	pResult = MemAlloc_AllocAlignedUnattributed( size, align );
	std::memcpy( pResult, ptr, nOldSize - nOffset );
	g_pMemAlloc->Free( pAlloc );
	return pResult;

}

inline void MemAlloc_FreeAligned( void *pMemBlock )
{

	void *pAlloc;

	if ( pMemBlock == nullptr )
		return;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((std::size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *( (void **)pAlloc );

	g_pMemAlloc->Free( pAlloc );

}

inline void MemAlloc_FreeAligned( void *pMemBlock, const char *pszFile, int nLine )
{

	void *pAlloc;

	if ( pMemBlock == nullptr )
		return;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((std::size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *( (void **)pAlloc );
	g_pMemAlloc->Free( pAlloc, pszFile, nLine );

}

inline void MemAlloc_Free(void *pMemBlock)
{
	g_pMemAlloc->Free(pMemBlock);
}

// Zeroes an already-allocated block in place and returns it, for call sites that allocate
// (e.g. with file/line info) and zero as separate steps.
inline void *MemAlloc_InlineCallocMemset( void *pMem, std::size_t nCount, std::size_t nElementSize )
{
	std::memset( pMem, 0, nElementSize * nCount );
	return pMem;
}

// calloc/strdup/wcsdup equivalents that route through g_pMemAlloc.
inline void *MemAlloc_Calloc( std::size_t nCount, std::size_t nElementSize )
{
	void *pMem = MemAlloc_Alloc( nCount * nElementSize );
	if ( pMem )
	{
		std::memset( pMem, 0, nCount * nElementSize );
	}
	return pMem;
}

inline char *MemAlloc_StrDup( const char *pString )
{
	if ( !pString )
		return nullptr;

	std::size_t len = std::strlen( pString ) + 1;
	char *pMemory = (char *)MemAlloc_Alloc( len );
	if ( pMemory )
	{
		return std::strcpy( pMemory, pString );
	}
	return nullptr;
}

inline wchar_t *MemAlloc_WcStrDup( const wchar_t *pString )
{
	if ( !pString )
		return nullptr;

	std::size_t len = std::wcslen( pString ) + 1;
	wchar_t *pMemory = (wchar_t *)MemAlloc_Alloc( len * sizeof( wchar_t ) );
	if ( pMemory )
	{
		return std::wcscpy( pMemory, pString );
	}
	return nullptr;
}

inline std::size_t MemAlloc_GetSizeAligned( void *pMemBlock )
{

	void *pAlloc;

	if ( pMemBlock == nullptr )
		return 0;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((std::size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *((void **)pAlloc );
	return g_pMemAlloc->GetSize( pAlloc ) - ( (byte *)pMemBlock - (byte *)pAlloc );

}

//-----------------------------------------------------------------------------

inline void MemAlloc_PushAllocDbgInfo( const char *, int ) {}
inline void MemAlloc_PopAllocDbgInfo() {}
inline void MemAlloc_RegisterAllocation( const char *, int, int, int, double ) {}
inline void MemAlloc_RegisterDeallocation( const char *, int, int, int, double ) {}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

template <typename T> inline const char *MEM_ALLOC_CLASSNAME() { return nullptr; }

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

// ApproximateProcessMemoryUsage returns the approximate memory footprint of this process.
std::size_t ApproximateProcessMemoryUsage( void );

}
