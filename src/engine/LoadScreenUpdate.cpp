// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.

import ks.core;
#include "LoadScreenUpdate.h"
import std;
#include "tier2/tier2.h"
#include "materialsystem/imaterialsystem.h"

//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
class CLoaderMemAlloc : public IMemAlloc
{
	// Methods of IMemAlloc
public:
	virtual void *Alloc( std::size_t nSize );
	virtual void *Realloc( void *pMem, std::size_t nSize );
	virtual void Free( void *pMem );
	void * Expand_NoLongerSupported( void * a1, std::size_t a2 )	{ return (m_pMemAlloc)->Expand_NoLongerSupported( a1, a2 ); };
	virtual void *Alloc( std::size_t nSize, const char *pFileName, int nLine );
	virtual void *Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine );
	virtual void  Free( void *pMem, const char *pFileName, int nLine );
	void* Expand_NoLongerSupported( void * a1, std::size_t a2, const char * a3, int a4 )	{ return (m_pMemAlloc)->Expand_NoLongerSupported( a1, a2, a3, a4 ); };
	std::size_t GetSize( void * a1 )	{ return (m_pMemAlloc)->GetSize( a1 ); };
	void PushAllocDbgInfo( const char * a1, int a2 )	{ (m_pMemAlloc)->PushAllocDbgInfo( a1, a2 ); };
	void PopAllocDbgInfo()	{ (m_pMemAlloc)->PopAllocDbgInfo(); };
	int32 CrtSetBreakAlloc( int32 a1 )	{ return (m_pMemAlloc)->CrtSetBreakAlloc( a1 ); };
	int CrtSetReportMode( int a1, int a2 )	{ return (m_pMemAlloc)->CrtSetReportMode( a1, a2 ); };
	int CrtIsValidHeapPointer( const void * a1 )	{ return (m_pMemAlloc)->CrtIsValidHeapPointer( a1 ); };
	int CrtIsValidPointer( const void * a1, unsigned int a2, int a3 )	{ return (m_pMemAlloc)->CrtIsValidPointer( a1, a2, a3 ); };
	int CrtCheckMemory()	{ return (m_pMemAlloc)->CrtCheckMemory(); };
	int CrtSetDbgFlag( int a1 )	{ return (m_pMemAlloc)->CrtSetDbgFlag( a1 ); };
	void CrtMemCheckpoint( _CrtMemState * a1 )	{ (m_pMemAlloc)->CrtMemCheckpoint( a1 ); };
	void DumpStats()	{ (m_pMemAlloc)->DumpStats(); };
	void DumpStatsFileBase( const char * a1, IMemAlloc::DumpStatsFormat_t a2 )	{ (m_pMemAlloc)->DumpStatsFileBase( a1, a2 ); };
	std::size_t ComputeMemoryUsedBy( const char * a1 )	{ return (m_pMemAlloc)->ComputeMemoryUsedBy( a1 ); };
	void* CrtSetReportFile( int a1, void* a2 )	{ return (m_pMemAlloc)->CrtSetReportFile( a1, a2 ); };
	void* CrtSetReportHook( void* a1 )	{ return (m_pMemAlloc)->CrtSetReportHook( a1 ); };
	int CrtDbgReport( int a1, const char * a2, int a3, const char * a4, const char * a5 )	{ return (m_pMemAlloc)->CrtDbgReport( a1, a2, a3, a4, a5 ); };
	int heapchk()	{ return (m_pMemAlloc)->heapchk(); };
	bool IsDebugHeap()	{ return (m_pMemAlloc)->IsDebugHeap(); };
	void GetActualDbgInfo( const char *& a1, int & a2 )	{ (m_pMemAlloc)->GetActualDbgInfo( a1, a2 ); };
	void RegisterAllocation( const char * a1, int a2, std::size_t a3, std::size_t a4, unsigned a5 )	{ (m_pMemAlloc)->RegisterAllocation( a1, a2, a3, a4, a5 ); };
	void RegisterDeallocation( const char * a1, int a2, std::size_t a3, std::size_t a4, unsigned a5 )	{ (m_pMemAlloc)->RegisterDeallocation( a1, a2, a3, a4, a5 ); };
	int GetVersion()	{ return (m_pMemAlloc)->GetVersion(); };
	void CompactHeap()	{ (m_pMemAlloc)->CompactHeap(); };
	MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t a1 )	{ return (m_pMemAlloc)->SetAllocFailHandler( a1 ); };
	void DumpBlockStats( void * a1 )	{ (m_pMemAlloc)->DumpBlockStats( a1 ); };
	void SetStatsExtraInfo( const char * a1, const char * a2 )	{ (m_pMemAlloc)->SetStatsExtraInfo( a1, a2 ); };
	std::size_t MemoryAllocFailed()	{ return (m_pMemAlloc)->MemoryAllocFailed(); };
	void CompactIncremental()	{ (m_pMemAlloc)->CompactIncremental(); };
	void OutOfMemory( std::size_t a1 )	{ (m_pMemAlloc)->OutOfMemory( a1 ); };
	void GlobalMemoryStatus( std::size_t * a1, std::size_t * a2 )	{ (m_pMemAlloc)->GlobalMemoryStatus( a1, a2 ); };
	IVirtualMemorySection * AllocateVirtualMemorySection( std::size_t a1 )	{ return (m_pMemAlloc)->AllocateVirtualMemorySection( a1 ); };
	int GetGenericMemoryStats( GenericMemoryStat_t ** a1 )	{ return (m_pMemAlloc)->GetGenericMemoryStats( a1 ); };

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	virtual void *RegionAlloc( int region, std::size_t nSize );
	virtual void *RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine );

	// Other public methods
public:
	CLoaderMemAlloc();
	void Start( MaterialNonInteractiveMode_t mode );
	void Stop();
	void AbortDueToShutdown();
	void Pause( bool bPause );

	// Check if we need to call swap. Do so if necessary.
	void CheckSwap( );

private:
	IMemAlloc	*m_pMemAlloc;
	bool		m_bPaused;
};


//-----------------------------------------------------------------------------
// Activate, deactivate loadermemalloc
//-----------------------------------------------------------------------------
static bool s_bLoadingUpdatesEnabled = true;
static CLoaderMemAlloc s_LoaderMemAlloc;

void AbortLoadingUpdatesDueToShutdown()
{
	s_bLoadingUpdatesEnabled = false;
}

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode )
{
}

void RefreshScreenIfNecessary()
{
}

void PauseLoadingUpdates( bool bPause )
{
}

void EndLoadingUpdates()
{
}

static int LoadLibraryThreadFunc()
{
	RefreshScreenIfNecessary();
	return 15;
}


//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
CLoaderMemAlloc::CLoaderMemAlloc() 
{
	m_pMemAlloc = 0;
	m_bPaused = false;
}

void CLoaderMemAlloc::Start( MaterialNonInteractiveMode_t mode )
{
	contract_assert( ThreadInMainThread() );
	if ( g_pMemAlloc == this || ( mode == MATERIAL_NON_INTERACTIVE_MODE_NONE ) )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( mode );

	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_STARTUP )
	{
		SetThreadedLoadLibraryFunc( LoadLibraryThreadFunc );
	}

	// NOTE: This is necessary to avoid a one-frame black flash
	// since Present is what copies the back buffer into the temp buffer
	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD )
	{
		extern void V_RenderUIOnly( void );
		V_RenderUIOnly();
	}

	m_pMemAlloc = g_pMemAlloc;
	g_pMemAlloc = this;
	m_bPaused = false;
}

void CLoaderMemAlloc::Stop()
{
	contract_assert( ThreadInMainThread() );

	if ( g_pMemAlloc != this )
		return;

	g_pMemAlloc = m_pMemAlloc;
	m_bPaused = false;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( MATERIAL_NON_INTERACTIVE_MODE_NONE );
	SetThreadedLoadLibraryFunc( nullptr );
}

void CLoaderMemAlloc::AbortDueToShutdown()
{
	contract_assert( ThreadInMainThread() );

	if ( g_pMemAlloc != this )
		return;

	g_pMemAlloc = m_pMemAlloc;
	m_bPaused = false;
}

void CLoaderMemAlloc::Pause( bool bPause )
{
	if ( g_pMemAlloc != this )
		return;

	m_bPaused = bPause;
}

//-----------------------------------------------------------------------------
// Check if we need to call swap. Do so if necessary.
//-----------------------------------------------------------------------------
void CLoaderMemAlloc::CheckSwap( )
{
	if ( g_pMemAlloc != this || m_bPaused )
		return;

	g_pMaterialSystem->RefreshFrontBufferNonInteractive();
}

//-----------------------------------------------------------------------------
// Hook allocations, render when appropriate
//-----------------------------------------------------------------------------
void *CLoaderMemAlloc::Alloc( std::size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->IndirectAlloc( nSize );
}

void *CLoaderMemAlloc::Realloc( void *pMem, std::size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize );
}

void CLoaderMemAlloc::Free( void *pMem )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem );
}

void *CLoaderMemAlloc::Alloc( std::size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine );
}

void *CLoaderMemAlloc::Realloc( void *pMem, std::size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize, pFileName, nLine );
}

void  CLoaderMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem, pFileName, nLine );
}

void *CLoaderMemAlloc::RegionAlloc( int region, std::size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->RegionAlloc( region, nSize );
}

void *CLoaderMemAlloc::RegionAlloc( int region, std::size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->RegionAlloc( region, nSize, pFileName, nLine );
}
