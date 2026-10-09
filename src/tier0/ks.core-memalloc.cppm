// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:memalloc;

import std;
import ks.platform;
import :prelude;
import :color;
import :logging;
import :dbg;

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

// Virtual memory interface

/// This interface class is used to let the mem_dump command retrieve
/// information about memory allocations outside of the heap. It is currently
/// used by CMemoryStack to report on its allocations.
class IMemoryInfo
{
public:
	virtual const char* GetMemoryName() const = 0; // User friendly name for this stack or pool
	virtual std::size_t GetAllocatedBytes() const = 0; // Number of bytes currently allocated
	virtual std::size_t GetCommittedBytes() const = 0; // Bytes committed -- may be greater than allocated.
	virtual std::size_t GetReservedBytes() const = 0; // Bytes reserved -- may be greater than committed.
	virtual std::size_t GetHighestBytes() const = 0; // The maximum number of bytes allocated or committed.
};

// Add and remove callbacks used to get statistics on non-heap memory allocations.
void AddMemoryInfoCallback( IMemoryInfo* pMemoryInfo );
void RemoveMemoryInfoCallback( IMemoryInfo* pMemoryInfo );
// Display the memory statistics from the callbacks controlled by the above functions.
void DumpMemoryInfoStats();

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
class IMemAlloc
{
public:
	// Release versions
	virtual void *Alloc( std::size_t nSize ) = 0;
public:
	virtual void *Realloc( void *pMem, std::size_t nSize ) = 0;

	virtual void Free( void *pMem ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, std::size_t nSize ) = 0;

	// Debug versions
    virtual void *Alloc( std::size_t nSize, const char *pFileName, int nLine ) = 0;
public:
    virtual void *Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine ) = 0;
    virtual void  Free( void *pMem, const char *pFileName, int nLine ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, std::size_t nSize, const char *pFileName, int nLine ) = 0;

	inline void *IndirectAlloc( std::size_t nSize )										{ return Alloc( nSize ); }
	inline void *IndirectAlloc( std::size_t nSize, const char *pFileName, int nLine )	{ return Alloc( nSize, pFileName, nLine ); }

	// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
	virtual std::size_t GetSize( void *pMem ) = 0;

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine ) = 0;
    virtual void PopAllocDbgInfo() = 0;

	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	virtual int32 CrtSetBreakAlloc( int32 lNewBreakAlloc ) = 0;
	virtual	int CrtSetReportMode( int nReportType, int nReportMode ) = 0;
	virtual int CrtIsValidHeapPointer( const void *pMem ) = 0;
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) = 0;
	virtual int CrtCheckMemory( void ) = 0;
	virtual int CrtSetDbgFlag( int nNewFlag ) = 0;
	virtual void CrtMemCheckpoint( _CrtMemState *pState ) = 0;

	// FIXME: Make a better stats interface
	virtual void DumpStats() = 0;

	enum DumpStatsFormat_t
	{
		FORMAT_TEXT,
		FORMAT_HTML
	};

	virtual void DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat = FORMAT_TEXT ) = 0;
	virtual std::size_t ComputeMemoryUsedBy( char const *pchSubStr ) = 0;

	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nRptType, void* hFile ) = 0;
	virtual void* CrtSetReportHook( void* pfnNewHook ) = 0;
	virtual int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg ) = 0;

	virtual int heapchk() = 0;

	virtual bool IsDebugHeap() = 0;

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) = 0;
	virtual void RegisterAllocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) = 0;
	virtual void RegisterDeallocation( const char *pFileName, int nLine, std::size_t nLogicalSize, std::size_t nActualSize, unsigned nTime ) = 0;

	virtual int GetVersion() = 0;

	virtual void CompactHeap() = 0;

	// Function called when malloc fails or memory limits hit to attempt to free up memory (can come in any thread)
	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) = 0;

	virtual void DumpBlockStats( void * ) = 0;

	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment ) = 0;

	// Returns 0 if no failure, otherwise the size_t of the last requested chunk
	virtual std::size_t MemoryAllocFailed() = 0;

	virtual void CompactIncremental() = 0; 

	virtual void OutOfMemory( std::size_t nBytesAttempted = 0 ) = 0;

	// Region-based allocations
	virtual void *RegionAlloc( int region, std::size_t nSize ) = 0;
	virtual void *RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine ) = 0;

	// Replacement for ::GlobalMemoryStatus which accounts for unused memory in our system
	virtual void GlobalMemoryStatus( std::size_t *pUsedMemory, std::size_t *pFreeMemory ) = 0;

	// Obtain virtual memory manager interface
	virtual IVirtualMemorySection * AllocateVirtualMemorySection( std::size_t numMaxBytes ) = 0;

	// Request 'generic' memory stats (returns a list of N named values; caller should assume this list will change over time)
	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats ) = 0;

	virtual ~IMemAlloc() { };

	// handles storing allocation info for coroutines
	virtual uint32 GetDebugInfoSize() = 0;
	virtual void SaveDebugInfo( void *pvDebugInfo ) = 0;
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) = 0;	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) = 0;
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
