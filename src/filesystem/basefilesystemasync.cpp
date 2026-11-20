// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: CBaseFileSystem Async Operation
//			The CBaseFileSystem methods implement the IFileSystem
//			asynchronous entry points. The model for reads currently is a
//			callback model where the callback can take place either in the
//			context of the main thread or the worker thread. It would be
//			easy to do a polled model later. Async operations return a
//			handle which is used to refer to the operation later. The
//			handle is actually a pointer to a reference counted "job"
//			object that holds all the context, status, and results of an
//			operation.

import std;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.random;
import ks.core.threadtools;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.refcount;
import ks.tier1.strtools;
import ks.vstdlib.jobthread;
#include "basefilesystem.h"

// VCR mode for now is handled by not running async.  This is primarily for
// performance reasons. VCR mode would preclude the use of a lock-free job
// retrieval. Can change if need in future, but it's best to do so if needed,
// and to make it a deliberate compile time choice to keep the fast path.
#undef WaitForSingleObject

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
ConVar async_mode( "async_mode", "0", 0, "Set the async filesystem mode (0 = async, 1 = synchronous)" );

//-----------------------------------------------------------------------------
// Async Modes
//-----------------------------------------------------------------------------
enum FSAsyncMode_t
{
	FSAM_ASYNC,
	FSAM_SYNC,
};

// If we are not in CERT, we are adding some latency to the async accesses (between 15 and 25 ms).
// 70 ms is an usual average seek time on DVD / BluRay (or increasing much more the seek time for HDD).
ConVar async_simulate_delay( "async_simulate_delay", "0", 0, "Simulate a delay of up to a set msec per file operation" );
ConVar async_allow_held_files( "async_allow_held_files", "1", 0, "Allow AsyncBegin/EndRead()" );

static FSAsyncMode_t GetAsyncMode( void )
{
	return (FSAsyncMode_t)( async_mode.GetInt() );
}


static bool AsyncAllowHeldFiles( void )
{
	return async_allow_held_files.GetBool();
}


static void async_suspend( const CCommand &args );
static ConCommand async_suspend_command( "async_suspend", async_suspend, "" );
static void async_suspend( const CCommand &args )
{
	BaseFileSystem()->AsyncSuspend();
}

static void async_resume( const CCommand &args );
static ConCommand async_resume_command( "async_resume", async_resume, "" );
static void async_resume( const CCommand &args )
{
	BaseFileSystem()->AsyncResume();
}

[[gnu::always_inline]] inline static void SimulateDelay( void )
{
	if ( async_simulate_delay.GetInt() == 0 || ThreadInMainThread() )
	{
	}
	else 
	{
		int nDelay = async_simulate_delay.GetInt();
		int nDelayMidRange = nDelay / 4;
		ThreadSleep( RandomInt( nDelay - nDelayMidRange, nDelay + nDelayMidRange ) );
	}
}

//-----------------------------------------------------------------------------
// Need to support old external. New implementation has less granular priority for efficiency
//-----------------------------------------------------------------------------

inline JobPriority_t ConvertPriority( int iFilesystemPriority )		
{
	if ( iFilesystemPriority == 0 )
	{
		return JP_NORMAL;
	}
	else if ( iFilesystemPriority > 0 )
	{
		return JP_HIGH;
	}
	return JP_LOW;
}

//-----------------------------------------------------------------------------
//
// Support for holding files open
//
//-----------------------------------------------------------------------------

struct AsyncOpenedFile_t // no mutex needed, under control of CAsyncOpenedFiles
{
	AsyncOpenedFile_t() : hFile(FILESYSTEM_INVALID_HANDLE) {}
	// Reference count, starting at one; the owner (CAsyncOpenedFiles) deletes at zero
	int AddRef() { return m_iRefs.fetch_add( 1 ) + 1; }
	int Release() { return m_iRefs.fetch_sub( 1 ) - 1; }
	std::atomic<int> m_iRefs{ 1 };
	FileHandle_t hFile;
	const std::string *m_pName = nullptr;	// this entry's key in CAsyncOpenedFiles
};

// Handles are the AsyncOpenedFile_t pointers; map nodes don't move.
class CAsyncOpenedFiles
{
public:
	FSAsyncFile_t FindOrAdd( const char *pszFilename )
	{
		char szFixedName[MAX_FILEPATH];
		V_strncpy( szFixedName, pszFilename, sizeof( szFixedName ) );
		V_FixSlashes( szFixedName );

		std::scoped_lock lock( m_mutex );

		auto iEntry = m_map.find( szFixedName );
		if ( iEntry == m_map.end() )
		{
			iEntry = m_map.emplace( szFixedName, std::make_unique< AsyncOpenedFile_t >() ).first;
			iEntry->second->m_pName = &iEntry->first;
		}
		else
		{
			iEntry->second->AddRef();
		}
		return (FSAsyncFile_t)iEntry->second.get();
	}

	FSAsyncFile_t Find( const char *pszFilename )
	{
		char szFixedName[MAX_FILEPATH];
		V_strncpy( szFixedName, pszFilename, sizeof( szFixedName ) );
		V_FixSlashes( szFixedName );

		std::scoped_lock lock( m_mutex );

		auto iEntry = m_map.find( szFixedName );
		if ( iEntry == m_map.end() )
		{
			return FS_INVALID_ASYNC_FILE;
		}
		iEntry->second->AddRef();
		return (FSAsyncFile_t)iEntry->second.get();
	}

	AsyncOpenedFile_t *Get( FSAsyncFile_t item )
	{
		if ( item == FS_INVALID_ASYNC_FILE)
		{
			return nullptr;
		}

		std::scoped_lock lock( m_mutex );

		AsyncOpenedFile_t *pFile = (AsyncOpenedFile_t *)item;
		pFile->AddRef();
		return pFile;
	}

	void AddRef( FSAsyncFile_t item )
	{
		if ( item == FS_INVALID_ASYNC_FILE)
		{
			return;
		}

		std::scoped_lock lock( m_mutex );

		( (AsyncOpenedFile_t *)item )->AddRef();
	}

	void Release( FSAsyncFile_t item )
	{
		if ( item == FS_INVALID_ASYNC_FILE)
		{
			return;
		}

		std::scoped_lock lock( m_mutex );

		AsyncOpenedFile_t *pFile = (AsyncOpenedFile_t *)item;
		if ( pFile->Release() == 0 )
		{
			if ( pFile->hFile != FILESYSTEM_INVALID_HANDLE )
			{
				BaseFileSystem()->Close( pFile->hFile );
			}
			auto iEntry = m_map.find( *pFile->m_pName );
			contract_assert( iEntry != m_map.end() && iEntry->second.get() == pFile );
			m_map.erase( iEntry );
		}
	}

private:
	std::recursive_mutex m_mutex;

	std::map< std::string, std::unique_ptr< AsyncOpenedFile_t >, ks::iless > m_map;
};

CAsyncOpenedFiles g_AsyncOpenedFiles;




//---------------------------------------------------------

static_assert( (int)FSASYNC_OK == (int)JOB_OK );
static_assert( (int)FSASYNC_STATUS_PENDING == (int)JOB_STATUS_PENDING );
static_assert( (int)FSASYNC_STATUS_INPROGRESS == (int)JOB_STATUS_INPROGRESS );
static_assert( (int)FSASYNC_STATUS_ABORTED == (int)JOB_STATUS_ABORTED );
static_assert( (int)FSASYNC_STATUS_UNSERVICED == (int)JOB_STATUS_UNSERVICED );

//---------------------------------------------------------
// The filesystem jobs are plain CJobs. Each job owns its work object (captured by the
// job's callable), which is destroyed with the job.
//---------------------------------------------------------

//---------------------------------------------------------
// A standard filesystem read job (and the file size job, which is a read that only
// asks for the size)
//---------------------------------------------------------
class CFileAsyncReadWork : public FileAsyncRequest_t
{
public:
	CFileAsyncReadWork( const FileAsyncRequest_t &fromRequest, bool bFileSizeOnly )
	  : FileAsyncRequest_t( fromRequest ),
		m_bFileSizeOnly( bFileSizeOnly ),
		m_pResultData( nullptr ),
		m_nResultSize( 0 ),
		m_pRealContext( fromRequest.pContext ),
		m_pfnRealCallback( fromRequest.pfnCallback )
	{
		pszFilename = MemAlloc_StrDup( fromRequest.pszFilename );
		V_FixSlashes( const_cast<char*>( pszFilename ) );

		pContext = this;
		pfnCallback = InterceptCallback;

		if ( hSpecificAsyncFile != FS_INVALID_ASYNC_FILE )
		{
			g_AsyncOpenedFiles.AddRef( hSpecificAsyncFile );
		}
#if defined(_DEBUG)
		m_pszAllocCreditFile = nullptr;
		m_nAllocCreditLine = 0;
#endif
	}

	~CFileAsyncReadWork()
	{
		if ( hSpecificAsyncFile != FS_INVALID_ASYNC_FILE )
		{
			g_AsyncOpenedFiles.Release( hSpecificAsyncFile );
		}

		if ( pszFilename )
			g_pMemAlloc->Free( (void *)pszFilename );
	}

	JobStatus_t Run()
	{
		SimulateDelay();

		if ( m_bFileSizeOnly )
		{
			return BaseFileSystem()->SyncGetFileSize( *this );
		}

#if defined(_DEBUG)
		if ( m_pszAllocCreditFile )
			MemAlloc_PushAllocDbgInfo( m_pszAllocCreditFile, m_nAllocCreditLine );
#endif

		JobStatus_t retval = BaseFileSystem()->SyncRead( *this );

#if defined(_DEBUG)
		if ( m_pszAllocCreditFile )
			MemAlloc_PopAllocDbgInfo();
#endif

		return retval;
	}

	void GetResult( void **ppData, int *pSize ) const
	{
		if ( m_pResultData )
		{
			*ppData = m_pResultData;
			*pSize = m_nResultSize;
		}
	}

	static void InterceptCallback( const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t result )
	{
		CFileAsyncReadWork *pWork = (CFileAsyncReadWork *)request.pContext;
		if ( result == FSASYNC_OK && !( request.flags & FSASYNC_FLAGS_FREEDATAPTR ) )
		{
			pWork->m_pResultData = request.pData;
			pWork->m_nResultSize = nBytesRead;
		}

		if ( pWork->m_pfnRealCallback )
		{
			// Going to slam the values. Not used after this point. Make temps if that changes
			FileAsyncRequest_t &temp = const_cast<FileAsyncRequest_t &>(request);
			temp.pfnCallback = pWork->m_pfnRealCallback;
			temp.pContext = pWork->m_pRealContext;

			(*pWork->m_pfnRealCallback)( temp, nBytesRead, result );
		}
	}

	void SetAllocCredit( const char *pszFile, int line )
	{
#if defined(_DEBUG)
		m_pszAllocCreditFile = pszFile;
		m_nAllocCreditLine = line;
#endif
	}

private:
	bool					m_bFileSizeOnly;
	void *					m_pResultData;
	int						m_nResultSize;
	void *					m_pRealContext;
	FSAsyncCallbackFunc_t	m_pfnRealCallback;
#if defined(_DEBUG)
	const char *			m_pszAllocCreditFile;
	int						m_nAllocCreditLine;
#endif
};

// The job carries its read work as user data, for AsyncGetResult
static CJob *CreateFileAsyncReadJob( const FileAsyncRequest_t &fromRequest, bool bFileSizeOnly, const char *pszFile, int line )
{
	std::unique_ptr<CFileAsyncReadWork> pWork = std::make_unique<CFileAsyncReadWork>( fromRequest, bFileSizeOnly );
	CFileAsyncReadWork *pRawWork = pWork.get();
#if defined(_DEBUG)
	pRawWork->SetAllocCredit( pszFile, line );
#endif
	CJob *pJob = new CJob( [pWork = std::move( pWork )]() { return pWork->Run(); }, ConvertPriority( fromRequest.priority ), pRawWork->pszFilename );
	pJob->SetFlags( pJob->GetFlags() | JF_IO );
	pJob->SetUserData( pRawWork );
	return pJob;
}

//---------------------------------------------------------
// Write to a file (or append to it)
//---------------------------------------------------------
static int g_nAsyncWriteJobs;

class CFileAsyncWriteWork
{
public:
	CFileAsyncWriteWork( const char *pszFilename, const void *pData, unsigned nBytes, bool bFreeMemory, bool bAppend )
	  : m_bFreeMemory( bFreeMemory ),
		m_pData( pData ),
		m_nBytes( nBytes ),
		m_bAppend( bAppend )
	{
		m_pszFilename = MemAlloc_StrDup( pszFilename );
		g_nAsyncWriteJobs++;
	}

	~CFileAsyncWriteWork()
	{
		g_nAsyncWriteJobs--;
		g_pMemAlloc->Free( (void *)m_pszFilename );
	}

	JobStatus_t Run()
	{
		SimulateDelay();
		JobStatus_t retval = BaseFileSystem()->SyncWrite( m_pszFilename, m_pData, m_nBytes, false, m_bAppend );

		return retval;
	}

	// Called once the job has been executed or discarded
	void Cleanup()
	{
		if ( m_pData && m_bFreeMemory )
			delete[] (char*)m_pData;
	}

	const char *Filename() const { return m_pszFilename; }

private:
	bool		m_bFreeMemory;
	const char *m_pszFilename;
	const void *m_pData;
	int			m_nBytes;
	bool		m_bAppend;
};

static CJob *CreateFileAsyncWriteJob( const char *pszFilename, const void *pData, unsigned nBytes, bool bFreeMemory, bool bAppend )
{
	std::unique_ptr<CFileAsyncWriteWork> pWork = std::make_unique<CFileAsyncWriteWork>( pszFilename, pData, nBytes, bFreeMemory, bAppend );
	CFileAsyncWriteWork *pRawWork = pWork.get();
	CJob *pJob = new CJob( [pWork = std::move( pWork )]() { return pWork->Run(); }, JP_LOW, pRawWork->Filename() );
	pJob->SetFlags( pJob->GetFlags() | JF_IO | JF_SERIAL | JF_WRITE );
	pJob->SetCleanup( [pRawWork]() { pRawWork->Cleanup(); } );
	return pJob;
}

//---------------------------------------------------------
// Append two files
//---------------------------------------------------------
class CFileAsyncAppendFileWork
{
public:
	CFileAsyncAppendFileWork( const char *pszAppendTo, const char *pszAppendFrom )
	{
		m_pszAppendTo = MemAlloc_StrDup( pszAppendTo );
		m_pszAppendFrom = MemAlloc_StrDup( pszAppendFrom );
		V_FixSlashes( const_cast<char*>( m_pszAppendTo ) );
		V_FixSlashes( const_cast<char*>( m_pszAppendFrom ) );
		g_nAsyncWriteJobs++;
	}

	~CFileAsyncAppendFileWork()
	{
		g_nAsyncWriteJobs--;
	}

	JobStatus_t Run()
	{
		SimulateDelay();
		JobStatus_t retval = BaseFileSystem()->SyncAppendFile( m_pszAppendTo, m_pszAppendFrom );

		return retval;
	}

	const char *AppendTo() const { return m_pszAppendTo; }

private:
	const char *m_pszAppendTo;
	const char *m_pszAppendFrom;
};

static CJob *CreateFileAsyncAppendFileJob( const char *pszAppendTo, const char *pszAppendFrom )
{
	std::unique_ptr<CFileAsyncAppendFileWork> pWork = std::make_unique<CFileAsyncAppendFileWork>( pszAppendTo, pszAppendFrom );
	CFileAsyncAppendFileWork *pRawWork = pWork.get();
	CJob *pJob = new CJob( [pWork = std::move( pWork )]() { return pWork->Run(); }, JP_LOW, pRawWork->AppendTo() );
	pJob->SetFlags( pJob->GetFlags() | JF_IO | JF_SERIAL | JF_WRITE );
	return pJob;
}

//---------------------------------------------------------
// Scan a directory
//---------------------------------------------------------
class CFileAsyncDirectoryScanWork
{
public:
	CFileAsyncDirectoryScanWork( const char* pSearchSpec, bool bRecurseDirs, void* pContext, FSAsyncScanAddFunc_t pAddCallback, FSAsyncScanCompleteFunc_t pDoneCallback )
	{
		contract_assert( pSearchSpec != nullptr && V_strlen( pSearchSpec )  < MAX_PATH && pAddCallback != nullptr && pDoneCallback != nullptr );
		m_pAddResultCallback = pAddCallback;
		m_pCompletionCallback = pDoneCallback;
		m_bRecurse = bRecurseDirs;
		m_pContext = pContext;
		V_strcpy( m_SearchSpec, pSearchSpec );
		V_memset( m_CurPath, 0, sizeof (m_CurPath) );
	}

	JobStatus_t Run();

	const char *SearchSpec() const { return m_SearchSpec; }

private:
	FSAsyncScanAddFunc_t		m_pAddResultCallback;
	FSAsyncScanCompleteFunc_t	m_pCompletionCallback;
	void*						m_pContext;
	bool						m_bRecurse;
	char						m_SearchSpec[MAX_PATH];
	char						m_CurPath[MAX_PATH];
};

JobStatus_t CFileAsyncDirectoryScanWork::Run()
{

	FileFindHandle_t findSession = -1;
	
	// Save off the current path
	V_strcpy( m_CurPath, m_SearchSpec );
	V_StripFilename( m_CurPath );
	
	const char *pFileName = g_pFullFileSystem->FindFirst( m_SearchSpec, &findSession );

	while ( pFileName != nullptr)
	{
		(*m_pAddResultCallback)( m_pContext, m_CurPath, ( char* ) pFileName );
		
		pFileName = g_pFullFileSystem->FindNext( findSession );
	}

	g_pFullFileSystem->FindClose( findSession );
	
	(*m_pCompletionCallback)( m_pContext, FSASYNC_OK );

	return FSASYNC_OK;

}

static CJob *CreateFileAsyncDirectoryScanJob( const char* pSearchSpec, bool bRecurseDirs, void* pContext, FSAsyncScanAddFunc_t pAddCallback, FSAsyncScanCompleteFunc_t pDoneCallback )
{
	std::unique_ptr<CFileAsyncDirectoryScanWork> pWork = std::make_unique<CFileAsyncDirectoryScanWork>( pSearchSpec, bRecurseDirs, pContext, pAddCallback, pDoneCallback );
	CFileAsyncDirectoryScanWork *pRawWork = pWork.get();
	CJob *pJob = new CJob( [pWork = std::move( pWork )]() { return pWork->Run(); }, JP_NORMAL, pRawWork->SearchSpec() );
	pJob->SetFlags( pJob->GetFlags() | JF_IO );
	return pJob;
}




//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseFileSystem::InitAsync()
{
	contract_assert( !m_pThreadPool );
	if ( m_pThreadPool )
		return;

	if ( CommandLine()->FindParm( "-noasync" ) )
	{
		Msg( "Async I/O disabled from command line\n" );
		return;
	}

	bool b360EmulatePS3 = false;

	// create the i/o thread pool
	m_pThreadPool = CreateNewThreadPool();

	ThreadPoolStartParams_t params;
	params.iThreadPriority = 0;
	params.bIOThreads = true;

	params.nThreads = std::min<std::common_type_t<uint8, int>>( GetCPUInformation().m_nLogicalProcessors, 3 ); // > 3 threads doing IO on one drive, are you crazy?
	params.nStackSize = 256 * 1024;

	if ( b360EmulatePS3 )
	{
		// override defaults
		// PS3 has a single i/o thread
		params.nThreads = 1;
		params.fDistribute = TRS_FALSE;
		params.bUseAffinityTable = false;
	}

	if ( !m_pThreadPool->Start( params, "FsAsyncIO" ) )
	{
		SafeRelease( m_pThreadPool );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseFileSystem::ShutdownAsync()
{
	if ( m_pThreadPool )
	{
		AsyncFlush();
		m_pThreadPool->Stop();
		SafeRelease( m_pThreadPool );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncReadMultiple( const FileAsyncRequest_t *pRequests, int nRequests, FSAsyncControl_t *phControls )
{
	return AsyncReadMultipleCreditAlloc( pRequests, nRequests, nullptr, 0, phControls );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncReadMultipleCreditAlloc( const FileAsyncRequest_t *pRequests, int nRequests, const char *pszFile, int line, FSAsyncControl_t *phControls )
{
	bool bAsyncMode = ( GetAsyncMode() == FSAM_ASYNC );
	bool bSynchronous = ( !bAsyncMode || ( pRequests[0].flags & FSASYNC_FLAGS_SYNC ) || !m_pThreadPool );

	if ( !bAsyncMode )
	{
		AsyncFinishAll();
	}

	CJob *pJob;

	for ( int i = 0; i < nRequests; i++ )
	{
		pJob = CreateFileAsyncReadJob( pRequests[i], pRequests[i].nBytes < 0, pszFile, line );

		if ( !bSynchronous )
		{
			// async mode, queue request
			m_pThreadPool->AddJob( pJob );
		}
		else
		{
			// synchronous mode, execute now
			pJob->Execute();
		}

		if ( phControls )
		{
			phControls[i] = (FSAsyncControl_t)pJob;
		}
		else
		{
			pJob->Release();
		}
	}

	return FSASYNC_OK;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncWrite(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
{
	bool bAsyncMode = ( GetAsyncMode() == FSAM_ASYNC );
	bool bSynchronous = ( !bAsyncMode || !m_pThreadPool );

	if ( !bAsyncMode )
	{
		AsyncFinishAll();
	}

	CJob *pJob = CreateFileAsyncWriteJob( pFileName, pSrc, nSrcBytes, bFreeMemory, bAppend );

	if ( !bSynchronous )
	{
		m_pThreadPool->AddJob( pJob );
	}
	else
	{
		pJob->Execute();
	}

	if ( pControl )
	{
		*pControl = (FSAsyncControl_t)pJob;
	}
	else
	{
		pJob->Release();
	}

	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncAppendFile(const char *pAppendToFileName, const char *pAppendFromFileName, FSAsyncControl_t *pControl )
{
	bool bAsyncMode = ( GetAsyncMode() == FSAM_ASYNC );
	bool bSynchronous = ( !bAsyncMode || !m_pThreadPool );

	if ( !bAsyncMode )
	{
		AsyncFinishAll();
	}

	CJob *pJob = CreateFileAsyncAppendFileJob( pAppendToFileName, pAppendFromFileName );

	if ( !bSynchronous )
	{
		m_pThreadPool->AddJob( pJob );
	}
	else
	{
		pJob->Execute();
	}

	if ( pControl )
	{
		*pControl = (FSAsyncControl_t)pJob;
	}
	else
	{
		pJob->Release();
	}

	return FSASYNC_OK;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
std::recursive_mutex g_AsyncFinishMutex;
void CBaseFileSystem::AsyncFinishAll( int iToPriority )
{
	if ( m_pThreadPool)
	{
		std::scoped_lock lock( g_AsyncFinishMutex );
		m_pThreadPool->ExecuteToPriority( ConvertPriority( iToPriority ) );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

static bool AsyncWriteJobFilter( CJob *pJob )
{
	return ( pJob->GetFlags() & JF_WRITE ) != 0;
}

void CBaseFileSystem::AsyncFinishAllWrites()
{
	if ( m_pThreadPool && g_nAsyncWriteJobs )
	{
		std::scoped_lock lock( g_AsyncFinishMutex );
		m_pThreadPool->ExecuteAll( AsyncWriteJobFilter );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AsyncSuspend()
{
	if ( m_pThreadPool )
	{
		m_pThreadPool->SuspendExecution();
	}

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AsyncResume()
{
	if ( m_pThreadPool )
	{
		m_pThreadPool->ResumeExecution();
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncBeginRead( const char *pszFile, FSAsyncFile_t *phFile )
{
#if !defined(DEDICATED)
	if ( AsyncAllowHeldFiles() )
	{
		*phFile = g_AsyncOpenedFiles.FindOrAdd( pszFile );
		return FSASYNC_OK;
	}
#endif
	*phFile = FS_INVALID_ASYNC_FILE;
	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncEndRead( FSAsyncFile_t hFile )
{
#if !defined(DEDICATED)
	if ( hFile != FS_INVALID_ASYNC_FILE )
		g_AsyncOpenedFiles.Release( hFile );
#endif

	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncFinish( FSAsyncControl_t hControl, bool wait )
{
	if ( wait  )
	{
		CJob *pJob = (CJob *)hControl;
		if ( !pJob )
		{
			return FSASYNC_ERR_FAILURE;
		}

		FSAsyncStatus_t retval = (FSAsyncStatus_t)pJob->Execute();

		return retval;
	}

	AsyncSetPriority( hControl, std::numeric_limits<int>::max() );
	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncGetResult( FSAsyncControl_t hControl, void **ppData, int *pSize )
{
	if ( ppData )
	{
		*ppData = nullptr;
	}
	if ( pSize )
	{
		*pSize = 0;
	}

	CJob *pJob = (CJob *)hControl;
	if ( !pJob )
	{
		return FSASYNC_ERR_FAILURE;
	}
	if ( pJob->IsFinished() )
	{
		// Only read jobs have a result to hand out
		const CFileAsyncReadWork *pReadWork = static_cast<const CFileAsyncReadWork *>( pJob->GetUserData() );
		if ( pReadWork )
		{
			pReadWork->GetResult( ppData, pSize );
		}
		return (FSAsyncStatus_t)pJob->GetStatus();
	}
	return FSASYNC_STATUS_PENDING;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncAbort( FSAsyncControl_t hControl )
{
	CJob *pJob = (CJob *)hControl;
	if ( !pJob )
	{
		return FSASYNC_ERR_FAILURE;
	}
	return (FSAsyncStatus_t)pJob->Abort();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncStatus( FSAsyncControl_t hControl )
{
	CJob *pJob = (CJob *)hControl;
	if ( !pJob )
	{
		return FSASYNC_ERR_FAILURE;
	}
	return (FSAsyncStatus_t)pJob->GetStatus();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncFlush()
{
	if ( m_pThreadPool )
	{
		m_pThreadPool->AbortAll();
	}

	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncSetPriority(FSAsyncControl_t hControl, int newPriority)
{
	if ( m_pThreadPool )
	{
		CJob *pJob = (CJob *)hControl;

		if ( !pJob )
		{
			return FSASYNC_ERR_FAILURE;
		}

		JobPriority_t internalPriority = ConvertPriority( newPriority );
		if ( internalPriority != pJob->GetPriority() )
		{
			m_pThreadPool->ChangePriority( pJob, internalPriority );
		}

	}
	return FSASYNC_OK;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AsyncAddRef( FSAsyncControl_t hControl )
{
	CJob *pJob = (CJob *)hControl;
	if ( pJob )
	{
		pJob->AddRef();
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AsyncRelease( FSAsyncControl_t hControl )
{
	CJob *pJob = (CJob *)hControl;
	if ( pJob )
	{
		pJob->Release();
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::AsyncDirectoryScan( const char* pSearchSpec, bool recurseFolders, void* pContext, FSAsyncScanAddFunc_t pfnAdd, FSAsyncScanCompleteFunc_t pfnDone, FSAsyncControl_t *pControl )
{
	bool bAsyncMode = ( GetAsyncMode() == FSAM_ASYNC );
	bool bSynchronous = ( !bAsyncMode || !m_pThreadPool );

	if ( !bAsyncMode )
	{
		AsyncFinishAll();
	}

	CJob *pJob = CreateFileAsyncDirectoryScanJob( pSearchSpec, recurseFolders, pContext, pfnAdd, pfnDone );

	if ( !bSynchronous )
	{
		m_pThreadPool->AddJob( pJob );
	}
	else
	{
		pJob->Execute();
	}

	if ( pControl )
	{
		*pControl = (FSAsyncControl_t)pJob;
	}
	else
	{
		pJob->Release();
	}

	return FSASYNC_OK;


}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static void *GetDest( const FileAsyncRequest_t &request, bool bTryUnbuffered, unsigned *pBytesBuffer, unsigned *pBytesRead )
{
	contract_assert( false ); // This function is not implemented
	return nullptr;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

FSAsyncStatus_t CBaseFileSystem::SyncRead( const FileAsyncRequest_t &request )
{
	contract_assert( request.nBytes >=0 );

	if ( request.nBytes < 0 || request.nOffset < 0 )
	{
		Msg( "Invalid async read of %s\n", request.pszFilename );
		DoAsyncCallback( request, nullptr, 0, FSASYNC_ERR_FILEOPEN );
		return FSASYNC_ERR_FILEOPEN;
	}

	FSAsyncStatus_t result;

	AsyncOpenedFile_t *pHeldFile = ( request.hSpecificAsyncFile != FS_INVALID_ASYNC_FILE ) ? g_AsyncOpenedFiles.Get( request.hSpecificAsyncFile ) : nullptr;

	FileHandle_t hFile;
	
	if ( !pHeldFile || pHeldFile->hFile == FILESYSTEM_INVALID_HANDLE )
	{
		hFile = OpenEx( request.pszFilename, "rb", 0, request.pszPathID );
		if ( pHeldFile )
		{
			pHeldFile->hFile = hFile;
		}
	}
	else
	{
		hFile = pHeldFile->hFile;
	}

	if ( hFile )
	{
		// ------------------------------------------------------
		int nBytesToRead = ( request.nBytes ) ? request.nBytes : Size( hFile ) - request.nOffset;
		int nBytesBuffer;
		void *pDest;

		if ( nBytesToRead < 0 )
		{
			nBytesToRead = 0; // bad offset?
		}

		if ( request.pData )
		{
			// caller provided buffer
			contract_assert( !( request.flags & FSASYNC_FLAGS_NULLTERMINATE ) );
			pDest = request.pData;
			nBytesBuffer = nBytesToRead;
		}
		else
		{
			// allocate an optimal buffer
			unsigned nOffsetAlign;
			nBytesBuffer = nBytesToRead + ( ( request.flags & FSASYNC_FLAGS_NULLTERMINATE ) ? 1 : 0 );
			if ( GetOptimalIOConstraints( hFile, &nOffsetAlign, nullptr, nullptr) && ( request.nOffset % nOffsetAlign == 0 ) )
			{
				nBytesBuffer = GetOptimalReadSize( hFile, nBytesBuffer );
			}

			if ( !request.pfnAlloc )
			{
				pDest = AllocOptimalReadBuffer( hFile, nBytesBuffer, request.nOffset );
			}
			else
			{
				pDest = (*request.pfnAlloc)( request.pszFilename, nBytesBuffer );
			}
		}

		SetBufferSize( hFile, 0 ); // TODO: what if it's a pack file? restore buffer size?

		if ( request.nOffset > 0 )
		{
			Seek( hFile, request.nOffset, FILESYSTEM_SEEK_HEAD );
		}

		// perform the read operation
		int iPrevPriority = ThreadGetPriority();
		if ( iPrevPriority < 2 )
		{
			ThreadSetPriority( 2 );
		}
		int nBytesRead = ReadEx( pDest, nBytesBuffer, nBytesToRead, hFile );
		if ( iPrevPriority < 2 )
		{
			ThreadSetPriority( iPrevPriority );
		}

		if ( !pHeldFile )
		{
			Close( hFile );
		}

		if ( request.flags & FSASYNC_FLAGS_NULLTERMINATE )
		{
			((char *)pDest)[nBytesRead] = 0;
		}

		result = ( ( nBytesRead == 0 ) && ( nBytesToRead != 0 ) ) ? FSASYNC_ERR_READING : FSASYNC_OK;
		DoAsyncCallback( request, pDest, std::min( nBytesRead, nBytesToRead ), result );
	}
	else
	{
		DoAsyncCallback( request, nullptr, 0, FSASYNC_ERR_FILEOPEN );
		result = FSASYNC_ERR_FILEOPEN;
	}

	if ( pHeldFile )
	{
		g_AsyncOpenedFiles.Release( request.hSpecificAsyncFile );
	}

	if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC )
	{
		LogAccessToFile( "async", request.pszFilename, "" );
	}

	return result;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::SyncGetFileSize( const FileAsyncRequest_t &request )
{
	int size = Size( request.pszFilename, request.pszPathID );

	DoAsyncCallback( request, nullptr, size, ( size ) ? FSASYNC_OK : FSASYNC_ERR_FILEOPEN );

	return FSASYNC_OK;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::SyncWrite(const char *pszFilename, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend )
{
	FileHandle_t hFile = OpenEx( pszFilename, ( bAppend ) ? "ab+" : "wb", 0, nullptr );
	if ( hFile )
	{
		SetBufferSize( hFile, 0 );
		Write( pSrc, nSrcBytes, hFile );
		Close( hFile );
		if ( bFreeMemory )
			delete[] (char*)pSrc;

		if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC )
			LogAccessToFile( "asyncwrite", pszFilename, "" );

		return FSASYNC_OK;
	}

	return FSASYNC_ERR_FILEOPEN;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FSAsyncStatus_t CBaseFileSystem::SyncAppendFile(const char *pAppendToFileName, const char *pAppendFromFileName )
{
	FileHandle_t hDestFile = OpenEx( pAppendToFileName, "ab+", 0, nullptr );
	FSAsyncStatus_t result = FSASYNC_ERR_FAILURE;
	if ( hDestFile )
	{
		SetBufferSize( hDestFile, 0 );
		FileHandle_t hSourceFile = OpenEx( pAppendFromFileName, "rb", 0, nullptr );
		if ( hSourceFile )
		{
			SetBufferSize( hSourceFile, 0 );
			const int BUFSIZE = 128 * 1024;
			int fileSize = Size( hSourceFile );
			char *buf = (char *)MemAlloc_Alloc( BUFSIZE );
			int size;

			while ( fileSize > 0 )
			{
				if ( fileSize > BUFSIZE )
					size = BUFSIZE;
				else
					size = fileSize;
				Read( buf, size, hSourceFile );
				Write( buf, size, hDestFile );

				fileSize -= size;
			}

			g_pMemAlloc->Free(buf);
			Close( hSourceFile );
			result = FSASYNC_OK;
		}
		Close( hDestFile );
	}

	if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC )
	{
		LogAccessToFile( "asyncappend", pAppendToFileName, "" );
	}

	return result;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseFileSystem::DoAsyncCallback( const FileAsyncRequest_t &request, void *pData, int nBytesRead, FSAsyncStatus_t result )
{
	void *pDataToFree = nullptr;

	if ( request.pfnCallback )
	{
		if ( pData && request.pData != pData )
		{
			// Allocated the data here
			FileAsyncRequest_t temp = request;
			temp.pData = pData;
			{
				(*request.pfnCallback)( temp, nBytesRead, result );
			}
			if ( !( request.flags & FSASYNC_FLAGS_ALLOCNOFREE ) )
			{
				pDataToFree = pData;
			}
		}
		else
		{
			{
				(*request.pfnCallback)( request, nBytesRead, result );
			}
			if ( ( request.flags & FSASYNC_FLAGS_FREEDATAPTR ) )
			{
				pDataToFree = request.pData;
			}
		}
	}

	if ( pDataToFree  )
	{
		contract_assert( !request.pfnAlloc );
		// The ugly delete[] (void*) method generates a compile warning on osx, as it should.
		g_pMemAlloc->Free( pDataToFree );
	}
}
