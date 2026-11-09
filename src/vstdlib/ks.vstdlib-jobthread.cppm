// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib:jobthread;

import std;
import ks.tier1;

export extern "C++" {

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CJob;


enum JobStatusEnum_t
{
	// Use negative for errors
	JOB_OK,						// operation is successful
	JOB_STATUS_PENDING,			// file is properly queued, waiting for service
	JOB_STATUS_INPROGRESS,		// file is being accessed
	JOB_STATUS_ABORTED,			// file was aborted by caller
	JOB_STATUS_UNSERVICED,		// file is not yet queued
};

typedef int JobStatus_t;


enum JobFlags_t
{
	JF_IO				= ( 1 << 0 ),	// The job primarily blocks on IO or hardware
	JF_BOOST_THREAD		= ( 1 << 1 ),	// Up the thread priority to max allowed while processing task
	JF_SERIAL			= ( 1 << 2 ),	// Job cannot be executed out of order relative to other "strict" jobs
	JF_QUEUE			= ( 1 << 3 ),	// Queue it, even if not an IO job
	JF_WRITE			= ( 1 << 4 ),	// The job writes to disk; the filesystem flushes these before shutdown
};

enum JobPriority_t
{
	JP_LOW,
	JP_NORMAL,
	JP_HIGH,
	JP_IMMEDIATE,

	JP_NUM_PRIORITIES,

	// Priority aliases for game jobs
	JP_FRAME			= JP_NORMAL,
	JP_FRAME_SEGMENT	= JP_HIGH,
};

inline constexpr int TP_MAX_POOL_THREADS = 64;
struct ThreadPoolStartParams_t
{
	ThreadPoolStartParams_t( bool bIOThreads = false, unsigned nThreads = (unsigned)-1, int *pAffinities = nullptr, ThreeState_t fDistribute = TRS_NONE, unsigned nStackSize = (unsigned)-1, int iThreadPriority = std::numeric_limits<short>::min() )
		: bIOThreads( bIOThreads ), nThreads( nThreads ), nThreadsMax( -1 ), fDistribute( fDistribute ), nStackSize( nStackSize ), iThreadPriority( iThreadPriority )
	{
		bExecOnThreadPoolThreadsOnly = false;

		bUseAffinityTable = ( pAffinities != nullptr ) && ( fDistribute == TRS_TRUE ) && ( nThreads != (unsigned)-1 );
		if ( bUseAffinityTable )
		{
			// user supplied an optional 1:1 affinity mapping to override normal distribute behavior
			nThreads = std::min<std::common_type_t<int, unsigned int>>( TP_MAX_POOL_THREADS, nThreads );
			for ( unsigned int i = 0; i < nThreads; i++ )
			{
				iAffinityTable[i] = pAffinities[i];
			}
		}
	}

	int				nThreads;
	int				nThreadsMax;
	ThreeState_t	fDistribute;
	int				nStackSize;
	int				iThreadPriority;
	int				iAffinityTable[TP_MAX_POOL_THREADS];

	bool			bIOThreads : 1;
	bool			bUseAffinityTable : 1;
	bool			bExecOnThreadPoolThreadsOnly : 1;
};

//-----------------------------------------------------------------------------
//
// IThreadPool
//
//-----------------------------------------------------------------------------

typedef bool (*JobFilter_t)( CJob * );


//---------------------------------------------------------

enum ThreadPoolMessages_t
{
	TPM_EXIT,		// Exit the thread
	TPM_SUSPEND,		// Suspend after next operation
};
class CJobQueue;
class CJobThread;

//-----------------------------------------------------------------------------
//
// IThreadPool: a pool of worker threads that service CJobs.
//
// The pool is reference counted. The global pool (g_pThreadPool) is sized by the
// engine (see Start) and must never be released to zero.
//
//-----------------------------------------------------------------------------
class IThreadPool
{
public:
	explicit IThreadPool( bool bGlobalPool = false );
	~IThreadPool();

	//-----------------------------------------------------
	// Reference counting
	//-----------------------------------------------------
	int AddRef()									{ return ++m_iRefs; }
	int Release();

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	bool Start( const ThreadPoolStartParams_t &startParams = ThreadPoolStartParams_t() );
	bool Start( const ThreadPoolStartParams_t &startParams, const char *pszNameOverride );
	bool Stop( int timeout = TT_INFINITE );
	void Distribute( bool bDistribute = true, int *pAffinityTable = nullptr );

	//-----------------------------------------------------
	// Functions for any thread
	//-----------------------------------------------------
	unsigned GetJobCount()							{ return m_nJobs.load(); }
	int NumThreads();
	int NumIdleThreads();

	//-----------------------------------------------------
	// Pause/resume processing jobs
	//-----------------------------------------------------
	int SuspendExecution();
	int ResumeExecution();

	//-----------------------------------------------------
	// Offer the current thread to the pool
	//-----------------------------------------------------
	int YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	int YieldWait( CJob **, int nJobs, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	void Yield( unsigned timeout );

	bool YieldWait( CThreadEvent &event, unsigned timeout = TT_INFINITE );
	bool YieldWait( CJob *, unsigned timeout = TT_INFINITE );

	int YieldWaitPerFrameJobs( );

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	// See AddPerFrameJob below if you want to add a job that
	// wants to be run before the end of the frame
	//-----------------------------------------------------
	void AddJob( CJob * );

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	// Call YieldWaitPerFrameJobs() to wait only until all per-frame jobs are done
	//-----------------------------------------------------
	void AddPerFrameJob( CJob * );

	//-----------------------------------------------------
	// Change the priority of an active job
	//-----------------------------------------------------
	void ChangePriority( CJob *p, JobPriority_t priority );

	//-----------------------------------------------------
	// Bulk job manipulation (blocking)
	//-----------------------------------------------------
	int ExecuteAll( JobFilter_t pfnFilter = nullptr )	{ return ExecuteToPriority( JP_LOW, pfnFilter ); }
	int ExecuteToPriority( JobPriority_t toPriority, JobFilter_t pfnFilter = nullptr  );
	int AbortAll();

	//-----------------------------------------------------
	// Add an arbitrary call to the queue (master thread) 
	//
	// Avert thy eyes! Imagine rather:
	//
	// CJob *QueueCall( <function>, [args1, [arg2,]...]
	// CJob *QueueCall( <object>, <function>, [args1, [arg2,]...]
	//
	// The returned job carries a reference for the caller.
	//-----------------------------------------------------
	template <typename RET_TYPE, typename... PARAMS, typename... ARGS>
	CJob *QueueCall( RET_TYPE (*pfnProxied)( PARAMS... ), ARGS &&... args )
	{
		return AddFunctorJob( CreateFunctor( pfnProxied, std::forward<ARGS>(args)... ) );
	}

	//-------------------------------------

	template <typename OBJECT_TYPE, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CJob *QueueCall( OBJECT_TYPE *pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
	{
		return AddFunctorJob( CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}

	//-------------------------------------

	template <typename OBJECT_TYPE, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CJob *QueueCall( OBJECT_TYPE *pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
	{
		return AddFunctorJob( CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}

private:
	template <typename FUNCTOR_TYPE>
	CJob *AddFunctorJob( FUNCTOR_TYPE &&functor );

	enum
	{
		IO_STACKSIZE = ( 64 * 1024 ),
		COMPUTATION_STACKSIZE = 0,
	};

	void InsertJobInQueue( CJob * );

private:
	friend class CJobThread;
	friend class CJob;

	std::atomic<int>			m_iRefs;
	// Global pool: started with the engine's own sizing, and never freed
	bool						m_bGlobalPool;

	std::unique_ptr<CJobQueue>	m_pSharedQueue;
	std::atomic<int>			m_nIdleThreads;
	std::vector< CJobThread * > m_Threads;

	std::recursive_mutex		m_SuspendMutex;
	int							m_nSuspend;
	std::atomic<int>			m_nJobs;

	// Some jobs should only be executed on the threadpool thread(s). Ie: the rendering thread has the GL context
	//	and the main thread coming in and "helping" with jobs breaks that pretty nicely. This flag states that
	//	only the threadpool threads should execute these jobs.
	bool						m_bExecOnThreadPoolThreadsOnly;

	std::recursive_mutex		m_PerFrameJobListMutex;
	ks::small_vector< CJob *, 2048 >	m_PerFrameJobs;
};

//-----------------------------------------------------------------------------

IThreadPool *CreateNewThreadPool();
void DestroyThreadPool( IThreadPool *pPool );

//-------------------------------------

void RunThreadPoolTests();

//-----------------------------------------------------------------------------

extern IThreadPool *g_pThreadPool;

//-----------------------------------------------------------------------------
// A unit of work for a thread pool: metadata about the operation (priority, flags,
// status, completion event) plus the operation itself. Reference counted; the
// operation, and everything it captured, is owned by the job and goes away with it.
//
// The operation is any callable returning void (the job then completes with JOB_OK)
// or a JobStatus_t. Anything a job needs to keep around beyond its operation (a state
// object others look at, data to free once it has run) is owned by the callable.
//-----------------------------------------------------------------------------
struct ThreadPoolData_t__ { int unused; };
using ThreadPoolData_t = ThreadPoolData_t__ *;
inline const ThreadPoolData_t JOB_NO_DATA = reinterpret_cast<ThreadPoolData_t>( static_cast<std::intptr_t>( -1 ) );

class CJob
{
public:
	template <typename F>
		requires ( !std::is_same_v< std::remove_cvref_t<F>, CJob > )
	explicit CJob( F &&fn, JobPriority_t priority = JP_NORMAL, const char *pszDescription = nullptr )
	  : m_iRefs( 1 ),
		m_status( JOB_STATUS_UNSERVICED ),
		m_ThreadPoolData( JOB_NO_DATA ),
		m_priority( priority ),
		m_flags( 0 ),
		m_pThreadPool( nullptr ),
		m_CompleteEvent( true ),
		m_iServicingThread( -1 ),
		m_pUserData( nullptr )
	{
		if constexpr ( std::is_void_v< std::invoke_result_t< std::remove_cvref_t<F> & > > )
		{
			m_fnExecute = [fnWork = std::forward<F>( fn )]() mutable -> JobStatus_t { fnWork(); return JOB_OK; };
		}
		else
		{
			m_fnExecute = std::forward<F>( fn );
		}

		if ( pszDescription )
		{
			m_strDescription = pszDescription;
		}
	}

	//-----------------------------------------------------
	// Reference counting
	//-----------------------------------------------------
	int AddRef()									{ return ++m_iRefs; }
	int Release()
	{
		int result = --m_iRefs;
		if ( result == 0 )
		{
			delete this;
		}
		return result;
	}

	//-----------------------------------------------------
	// Priority (not thread safe)
	//-----------------------------------------------------
	void SetPriority( JobPriority_t priority )		{ m_priority = priority; }
	JobPriority_t GetPriority() const				{ return m_priority; }

	//-----------------------------------------------------

	void SetFlags( unsigned flags )					{ m_flags = flags; }
	unsigned GetFlags() const						{ return m_flags; }

	//-----------------------------------------------------

	void SetServiceThread( int iServicingThread )	{ m_iServicingThread = (char)iServicingThread; }
	int GetServiceThread() const					{ return m_iServicingThread; }
	void ClearServiceThread()						{ m_iServicingThread = -1; }

	//-----------------------------------------------------
	// Fast queries
	//-----------------------------------------------------
	bool Executed() const							{ return ( m_status == JOB_OK );	}
	bool CanExecute() const							{ return ( m_status == JOB_STATUS_PENDING || m_status == JOB_STATUS_UNSERVICED ); }
	bool IsFinished() const							{ return ( m_status != JOB_STATUS_PENDING && m_status != JOB_STATUS_INPROGRESS && m_status != JOB_STATUS_UNSERVICED ); }
	JobStatus_t GetStatus() const					{ return m_status; }
	
	//-----------------------------------------------------
	// Try to acquire ownership (to satisfy). If you take the lock, you must either execute or abort.
	//-----------------------------------------------------
	bool TryLock()										{ return m_mutex.try_lock(); }
	void Lock()											{ m_mutex.lock(); }
	void Unlock()										{ m_mutex.unlock(); }

	//-----------------------------------------------------
	// Thread event support (safe for NULL this to simplify code )
	//-----------------------------------------------------
	bool WaitForFinish( uint32 dwTimeout = TT_INFINITE ) { if (!this) return true; return ( !IsFinished() ) ? g_pThreadPool->YieldWait( this, dwTimeout ) : true; }
	bool WaitForFinishAndRelease( uint32 dwTimeout = TT_INFINITE ) { if (!this) return true; bool bResult = WaitForFinish( dwTimeout); Release(); return bResult; }
	CThreadEvent *AccessEvent()						{ return &m_CompleteEvent; }

	//-----------------------------------------------------
	// Perform the job
	//-----------------------------------------------------
	JobStatus_t Execute();
	JobStatus_t TryExecute();
	JobStatus_t ExecuteAndRelease()					{ JobStatus_t status = Execute(); Release(); return status;	}
	JobStatus_t TryExecuteAndRelease()				{ JobStatus_t status = TryExecute(); Release(); return status;	}

	//-----------------------------------------------------
	// Terminate the job, discard if partially or wholly fulfilled
	//-----------------------------------------------------
	JobStatus_t Abort( bool bDiscard = true );

	//-----------------------------------------------------
	// Optional cleanup, run once the job has been executed or (when discarded) aborted
	//-----------------------------------------------------
	void SetCleanup( std::move_only_function<void()> fnCleanup )	{ m_fnCleanup = std::move( fnCleanup ); }

	//-----------------------------------------------------
	// Optional pointer to state owned by the job's callable, for whoever created the job
	// to get at; valid for as long as the job is alive (not thread safe)
	//-----------------------------------------------------
	void SetUserData( void *pData )					{ m_pUserData = pData; }
	void *GetUserData() const						{ return m_pUserData; }

	const char *Describe() const					{ return m_strDescription.c_str(); }

private:
	//-----------------------------------------------------
	friend class IThreadPool;

	std::atomic<int>	m_iRefs;

	// Written by the servicing thread under m_mutex, read from others without it.
	std::atomic<JobStatus_t>		m_status;
	JobPriority_t		m_priority;
	std::recursive_mutex	m_mutex;
	unsigned char		m_flags;
	char				m_iServicingThread;
	short				m_reserved;
	ThreadPoolData_t	m_ThreadPoolData;
	IThreadPool *		m_pThreadPool;
	CThreadEvent		m_CompleteEvent;

	std::move_only_function<JobStatus_t()>	m_fnExecute;
	std::move_only_function<void()>			m_fnCleanup;
	std::string			m_strDescription;
	void *				m_pUserData;

private:
	//-----------------------------------------------------
	CJob( const CJob &fromRequest ) = delete;
	void operator=(const CJob &fromRequest ) = delete;
};

//-----------------------------------------------------------------------------

template <typename FUNCTOR_TYPE>
inline CJob *IThreadPool::AddFunctorJob( FUNCTOR_TYPE &&functor )
{
	CJob *pJob = new CJob( std::forward<FUNCTOR_TYPE>( functor ) );
	pJob->SetFlags( JF_QUEUE );
	AddJob( pJob );
	return pJob;
}

//-----------------------------------------------------------------------------
// Work splitting: competitive, best when cost per item varies a lot
//-----------------------------------------------------------------------------

template <typename T>
class CJobItemProcessor
{
public:
	typedef T ItemType_t;
	void Begin() {}
	// void Process( ItemType_t & ) {}
	void End() {}
};

template <typename T>
class CFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init(void (*pfnProcess)( T & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr )
	{
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	//CFuncJobItemProcessor(OBJECT_TYPE_PTR pObject, void (FUNCTION_CLASS::*pfnProcess)( ITEM_TYPE & ), void (*pfnBegin)() = NULL, void (*pfnEnd)() = NULL );
	void Begin()						{ if ( m_pfnBegin ) (*m_pfnBegin)(); }
	void Process( T &item )	{ (*m_pfnProcess)( item ); }
	void End()							{ if ( m_pfnEnd ) (*m_pfnEnd)(); }

protected:
	void (*m_pfnProcess)( T & );
	void (*m_pfnBegin)();
	void (*m_pfnEnd)();
};

template <typename T, class OBJECT_TYPE, class FUNCTION_CLASS = OBJECT_TYPE >
class CMemberFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init( OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( T & ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr )
	{
		m_pObject = pObject;
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	void Begin()						{ if ( m_pfnBegin ) ((*m_pObject).*m_pfnBegin)(); }
	void Process( T &item )	            { ((*m_pObject).*m_pfnProcess)( item ); }
	void End()							{ if ( m_pfnEnd ) ((*m_pObject).*m_pfnEnd)(); }

protected:
	OBJECT_TYPE *m_pObject;

	void (FUNCTION_CLASS::*m_pfnProcess)( T & );
	void (FUNCTION_CLASS::*m_pfnBegin)();
	void (FUNCTION_CLASS::*m_pfnEnd)();
};

template <typename T>
class CLoopFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init(void (*pfnProcess)( T*, int, int ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr )
	{
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	void Begin()									{ if ( m_pfnBegin ) (*m_pfnBegin)(); }
	void Process( T* pContext, int nFirst, int nCount )	{ (*m_pfnProcess)( pContext, nFirst, nCount ); }
	void End()										{ if ( m_pfnEnd ) (*m_pfnEnd)(); }

protected:
	void (*m_pfnProcess)( T*, int, int );
	void (*m_pfnBegin)();
	void (*m_pfnEnd)();
};

template <typename T, class OBJECT_TYPE, class FUNCTION_CLASS = OBJECT_TYPE >
class CLoopMemberFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init( OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( T*, int, int ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr )
	{
		m_pObject = pObject;
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	void Begin()									{ if ( m_pfnBegin ) ((*m_pObject).*m_pfnBegin)(); }
	void Process( T *item, int nFirst, int nCount )	{ ((*m_pObject).*m_pfnProcess)( item, nFirst, nCount ); }
	void End()										{ if ( m_pfnEnd ) ((*m_pObject).*m_pfnEnd)(); }

protected:
	OBJECT_TYPE *m_pObject;

	void (FUNCTION_CLASS::*m_pfnProcess)( T*, int, int );
	void (FUNCTION_CLASS::*m_pfnBegin)();
	void (FUNCTION_CLASS::*m_pfnEnd)();
};

template <typename ITEM_TYPE, class ITEM_PROCESSOR_TYPE, int ID_TO_PREVENT_COMDATS_IN_PROFILES = 1>
class CParallelProcessor
{
public:
	CParallelProcessor()
	{
		m_pItems = m_pLimit= 0;
	}

	void Run( ITEM_TYPE *pItems, unsigned nItems, int nChunkSize = 1, int nMaxParallel = std::numeric_limits<int>::max(), IThreadPool *pThreadPool = nullptr )
	{
		if ( nItems == 0 )
			return;

		m_nChunkSize = nChunkSize;
		if ( !pThreadPool )
		{
			pThreadPool = g_pThreadPool;
		}

		m_pItems = pItems;
		m_pLimit = pItems + nItems;

		int nJobs = nItems - 1;

		if ( nJobs > nMaxParallel )
		{
			nJobs = nMaxParallel;
		}

		if (! pThreadPool )									// only possible on linux
		{
			DoExecute( );
			return;
		}

		int nThreads = pThreadPool->NumThreads();
		if ( nJobs > nThreads )
		{
			nJobs = nThreads;
		}

		if ( nJobs > 0 )
		{
			ks::ScratchArena<> scratch;
			CJob **jobs = (CJob **)scratch.AllocBytes( nJobs * sizeof(CJob **) );
			int i = nJobs;

			while( i-- )
			{
				jobs[i] = pThreadPool->QueueCall( this, &CParallelProcessor<ITEM_TYPE, ITEM_PROCESSOR_TYPE, ID_TO_PREVENT_COMDATS_IN_PROFILES>::DoExecute );
			}

			DoExecute();

			for ( i = 0; i < nJobs; i++ )
			{
				jobs[i]->Abort(); // will either abort ones that never got a thread, or noop on ones that did
				jobs[i]->Release();
			}
		}
		else
		{
			DoExecute();
		}
	}

	ITEM_PROCESSOR_TYPE m_ItemProcessor;

private:
	void DoExecute()
	{
		if ( m_pItems < m_pLimit )
		{
			m_ItemProcessor.Begin();

			ITEM_TYPE *pLimit = m_pLimit;

			int nChunkSize = m_nChunkSize;
			for (;;)
			{
				ITEM_TYPE *pCurrent = m_pItems.fetch_add( nChunkSize );
				ITEM_TYPE *pLast = std::min( pLimit, pCurrent + nChunkSize );
				while( pCurrent < pLast )
				{
					m_ItemProcessor.Process( *pCurrent );
					pCurrent++;
				}
				if ( pCurrent >= pLimit )
				{
					break;
				}
			}
			m_ItemProcessor.End();
		}
	}
	std::atomic<ITEM_TYPE *>	m_pItems;
	ITEM_TYPE *					m_pLimit;
	int m_nChunkSize;

};

template <typename ITEM_TYPE> 
inline void ParallelProcess( ITEM_TYPE *pItems, unsigned nItems, void (*pfnProcess)( ITEM_TYPE & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelProcessor<ITEM_TYPE, CFuncJobItemProcessor<ITEM_TYPE> > processor;
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, 1, nMaxParallel );
}

template <typename ITEM_TYPE, typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelProcess( ITEM_TYPE *pItems, unsigned nItems, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( ITEM_TYPE & ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelProcessor<ITEM_TYPE, CMemberFuncJobItemProcessor<ITEM_TYPE, OBJECT_TYPE, FUNCTION_CLASS> > processor;
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, 1, nMaxParallel );
}

// Parallel Process that lets you specify threadpool
template <typename ITEM_TYPE> 
inline void ParallelProcess( IThreadPool *pPool, ITEM_TYPE *pItems, unsigned nItems, void (*pfnProcess)( ITEM_TYPE & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelProcessor<ITEM_TYPE, CFuncJobItemProcessor<ITEM_TYPE> > processor;
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, 1, nMaxParallel, pPool );
}

template <typename ITEM_TYPE, typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelProcess( IThreadPool *pPool, ITEM_TYPE *pItems, unsigned nItems, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( ITEM_TYPE & ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelProcessor<ITEM_TYPE, CMemberFuncJobItemProcessor<ITEM_TYPE, OBJECT_TYPE, FUNCTION_CLASS> > processor;
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, 1, nMaxParallel, pPool );
}


template <class CONTEXT_TYPE, class ITEM_PROCESSOR_TYPE>
class CParallelLoopProcessor
{
public:
	CParallelLoopProcessor()
	{
		m_nIndex = m_nLimit = 0;
		m_nChunkCount = 0;
		m_nActive = 0;
	}

	void Run( CONTEXT_TYPE *pContext, int nBegin, int nItems, int nChunkCount, int nMaxParallel = std::numeric_limits<int>::max(), IThreadPool *pThreadPool = nullptr )
	{
		if ( !nItems )
			return;

		if ( !pThreadPool )
		{
			pThreadPool = g_pThreadPool;
		}

		m_pContext = pContext;
		m_nIndex = nBegin;
		m_nLimit = nBegin + nItems;
		nChunkCount = std::max( std::min( nItems, nChunkCount ), 1 );
		m_nChunkCount = ( nItems + nChunkCount - 1 ) / nChunkCount;
		int nJobs = ( nItems + m_nChunkCount - 1 ) / m_nChunkCount;
		if ( nJobs > nMaxParallel )
		{
			nJobs = nMaxParallel;
		}

		if ( !pThreadPool )									// only possible on linux
		{
			DoExecute( );
			return;
		}

		int nThreads = pThreadPool->NumThreads();
		if ( nJobs > nThreads )
		{
			nJobs = nThreads;
		}

		if ( nJobs > 0 )
		{
			ks::ScratchArena<> scratch;
			CJob **jobs = (CJob **)scratch.AllocBytes( nJobs * sizeof(CJob **) );
			int i = nJobs;

			while( i-- )
			{
				jobs[i] = pThreadPool->QueueCall( this, &CParallelLoopProcessor<CONTEXT_TYPE, ITEM_PROCESSOR_TYPE>::DoExecute );
			}

			DoExecute();

			for ( i = 0; i < nJobs; i++ )
			{
				jobs[i]->Abort(); // will either abort ones that never got a thread, or noop on ones that did
				jobs[i]->Release();
			}
		}
		else
		{
			DoExecute();
		}
	}

	ITEM_PROCESSOR_TYPE m_ItemProcessor;

private:
	void DoExecute()
	{
		m_ItemProcessor.Begin();
		for (;;)
		{
			int nIndex = m_nIndex.fetch_add( m_nChunkCount );
			if ( nIndex < m_nLimit )
			{
				int nCount = std::min( m_nChunkCount, m_nLimit - nIndex );
				m_ItemProcessor.Process( m_pContext, nIndex, nCount );
			}
			else
			{
				break;
			}
		}
		m_ItemProcessor.End();
		--m_nActive;
	}

	CONTEXT_TYPE				*m_pContext;
	std::atomic<int>			m_nIndex;
	int							m_nLimit;
	int							m_nChunkCount;
	std::atomic<int>			m_nActive;
};

template < typename CONTEXT_TYPE > 
inline void ParallelLoopProcess( IThreadPool *pPool, CONTEXT_TYPE *pContext, int nStart, int nCount, void (*pfnProcess)( CONTEXT_TYPE*, int, int ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelLoopProcessor< CONTEXT_TYPE, CLoopFuncJobItemProcessor< CONTEXT_TYPE > > processor;
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pContext, nStart, nCount, 1, nMaxParallel, pPool );
}

template < typename CONTEXT_TYPE, typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelLoopProcess( IThreadPool *pPool, CONTEXT_TYPE *pContext, int nStart, int nCount, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( CONTEXT_TYPE*, int, int ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelLoopProcessor< CONTEXT_TYPE, CLoopMemberFuncJobItemProcessor<CONTEXT_TYPE, OBJECT_TYPE, FUNCTION_CLASS> > processor;
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pContext, nStart, nCount, 1, nMaxParallel, pPool );
}

template < typename CONTEXT_TYPE > 
inline void ParallelLoopProcessChunks( IThreadPool *pPool, CONTEXT_TYPE *pContext, int nStart, int nCount, int nChunkSize, void (*pfnProcess)( CONTEXT_TYPE*, int, int ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelLoopProcessor< CONTEXT_TYPE, CLoopFuncJobItemProcessor< CONTEXT_TYPE > > processor;
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pContext, nStart, nCount, nChunkSize, nMaxParallel, pPool );
}

template < typename CONTEXT_TYPE, typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelLoopProcessChunks( IThreadPool *pPool, CONTEXT_TYPE *pContext, int nStart, int nCount, int nChunkSize, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( CONTEXT_TYPE*, int, int ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr, int nMaxParallel = std::numeric_limits<int>::max() )
{
	CParallelLoopProcessor< CONTEXT_TYPE, CLoopMemberFuncJobItemProcessor<CONTEXT_TYPE, OBJECT_TYPE, FUNCTION_CLASS> > processor;
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pContext, nStart, nCount, nChunkSize, nMaxParallel, pPool );
}


//-----------------------------------------------------------------------------
// Raw thread launching
//-----------------------------------------------------------------------------

inline uintp FunctorExecuteThread( void *pParam )
{
	std::unique_ptr<CFunctor> pFunctor( static_cast<CFunctor *>( pParam ) );
	(*pFunctor)();
	return 0;
}

inline ThreadHandle_t ThreadExecuteSoloImpl( CFunctor functor, const char *pszName = nullptr )
{
	ThreadHandle_t hThread;
	hThread = CreateSimpleThread( FunctorExecuteThread, new CFunctor( std::move( functor ) ) );
	if ( pszName )
	{
		ThreadSetDebugName( hThread, pszName );
	}
	return hThread;
}

inline ThreadHandle_t ThreadExecuteSolo( CJob *pJob ) { return ThreadExecuteSoloImpl( CreateFunctor( pJob, &CJob::Execute ), pJob->Describe()  ); }

// ThreadExecuteSolo( "name", <function>, [args1, [arg2,]...] )
// ThreadExecuteSolo( "name", <object>, <function>, [args1, [arg2,]...] )
template <typename T1, typename... ARGS>
inline ThreadHandle_t ThreadExecuteSolo( const char *pszName, T1 a1, ARGS... args ) { return ThreadExecuteSoloImpl( CreateFunctor( a1, args... ), pszName  ); }

//-----------------------------------------------------------------------------

inline bool IThreadPool::YieldWait( CThreadEvent &theEvent, unsigned timeout )
{
	CThreadEvent *pEvent = &theEvent;
	return ( YieldWait( &pEvent, 1, true, timeout ) != TW_TIMEOUT );
}

inline bool IThreadPool::YieldWait( CJob *pJob, unsigned timeout )
{
	return ( YieldWait( &pJob, 1, true, timeout ) != TW_TIMEOUT );
}

//-----------------------------------------------------------------------------

inline int IThreadPool::Release()
{
	int result = --m_iRefs;
	if ( result == 0 && !m_bGlobalPool )
	{
		delete this;
	}
	else if ( result == 0 )
	{
		contract_assert( false && "Releasing global thread pool object!" );
	}
	return result;
}

//-----------------------------------------------------------------------------

inline JobStatus_t CJob::Execute()
{
	if ( IsFinished() )
	{
		return m_status;
	}

	std::scoped_lock lock( m_mutex );
	AddRef();

	JobStatus_t result;

	switch ( m_status )
	{
	case JOB_STATUS_UNSERVICED:
	case JOB_STATUS_PENDING:
		{
			// Service it
			m_status = JOB_STATUS_INPROGRESS;

			result = m_status = m_fnExecute();
			
			if ( m_fnCleanup )
				m_fnCleanup();
			m_CompleteEvent.Set();
			break;
		}

	case JOB_STATUS_INPROGRESS:
		contract_assert( false && "Mutex Should have protected use while processing" );
		// fall through...

	case JOB_OK:
	case JOB_STATUS_ABORTED:
		result = m_status;
		break;

	default:
		contract_assert( ( m_status < JOB_OK ) && "Unknown job state" );
		result = m_status;
	}

	Release();

	return result;
}

//---------------------------------------------------------

inline JobStatus_t CJob::TryExecute()
{
	// TryLock() would only fail if another thread has entered
	// Execute() or Abort()
	if ( !IsFinished() && TryLock() )
	{
		// ...service the request
		Execute();
		Unlock();
	}
	return m_status;
}

//---------------------------------------------------------

inline JobStatus_t CJob::Abort( bool bDiscard )
{
	if ( IsFinished() )
	{
		return m_status;
	}

	std::scoped_lock lock( m_mutex );
	AddRef();

	JobStatus_t result;

	switch ( m_status )
	{
	case JOB_STATUS_UNSERVICED:
	case JOB_STATUS_PENDING:
		{
			result = m_status = JOB_STATUS_ABORTED;
			if ( bDiscard && m_fnCleanup )
				m_fnCleanup();
			m_CompleteEvent.Set();
		}
		break;

	case JOB_STATUS_ABORTED:
	case JOB_STATUS_INPROGRESS:
	case JOB_OK:
		result = m_status;
		break;

	default:
		contract_assert( ( m_status < JOB_OK ) && "Unknown job state" );
		result = m_status;
	}

	Release();

	return result;
}

//-----------------------------------------------------------------------------

}
