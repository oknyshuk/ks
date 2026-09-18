//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
#include "pch_tier0.h"


//#include <malloc.h>
#include <algorithm>
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "mem_helpers.h"

#pragma pack(4)

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
	void *InternalAlloc( int region, size_t nSize );
	void *InternalRealloc( void *pMem, size_t nSize );
	void  InternalFree( void *pMem );

	// Release versions
	virtual void *Alloc( size_t nSize );
	virtual void *Realloc( void *pMem, size_t nSize );
	virtual void  Free( void *pMem );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize );

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
    virtual void  Free( void *pMem, const char *pFileName, int nLine );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine );

	virtual void *RegionAlloc( int region, size_t nSize );
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine );

	// Returns size of a particular allocation
	virtual size_t GetSize( void *pMem );

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
	virtual void DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat = FORMAT_TEXT ) OVERRIDE;
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr );
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory );

	virtual bool IsDebugHeap() { return false; }

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) {}
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) { SetCRTAllocFailed( nBytesAttempted ); }

	virtual IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes );

	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats );

	virtual void CompactHeap();
	virtual void CompactIncremental(); 

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler );
	size_t CallAllocFailHandler( size_t nBytes ) { return (*m_pfnFailHandler)( nBytes); }

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	static size_t DefaultFailHandler( size_t );
	void DumpBlockStats( void *p ) {}

	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment );

	virtual size_t MemoryAllocFailed();

	void		SetCRTAllocFailed( size_t nMemSize );

	MemAllocFailHandler_t m_pfnFailHandler;
	size_t				m_sMemoryAllocFailed;
};

#pragma pack()
