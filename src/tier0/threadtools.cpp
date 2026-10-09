// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation


import ks.core;


import std;

				[[gnu::always_inline]] inline auto GetLastError() { return ks::platform::last_errno(); }
	typedef void *LPVOID;
		


// Need to ensure initialized before other clients call in for main thread ID


// thread creation counter.
// this is used to provide a unique threadid for each running thread in g_nThreadID ( a thread local variable ).

const int MAX_THREAD_IDS = 128;

static volatile bool s_bThreadIDAllocated[MAX_THREAD_IDS];

thread_local int g_nThreadID;


static std::recursive_mutex s_ThreadIDMutex;

void AllocateThreadID( void )
{
	std::scoped_lock lock( s_ThreadIDMutex );
	for( int i = 1; i < MAX_THREAD_IDS; i++ )
	{
		if ( ! s_bThreadIDAllocated[i] )
		{
			g_nThreadID = i;
			s_bThreadIDAllocated[i] = true;
			return;
		}
	}
	Error( "Out of thread ids. Decrease the number of threads or increase MAX_THREAD_IDS\n" );
}

void FreeThreadID( void )
{
	std::scoped_lock lock( s_ThreadIDMutex );
	int nThread = g_nThreadID;
	if ( nThread )
		s_bThreadIDAllocated[nThread] = false;
}
		

//-----------------------------------------------------------------------------
// Simple thread functions. 
// Because _beginthreadex uses stdcall, we need to convert to cdecl
//-----------------------------------------------------------------------------
struct ThreadProcInfo_t
{
	ThreadProcInfo_t( ThreadFunc_t pfnThread, void *pParam )
	  : pfnThread( pfnThread),
		pParam( pParam )
	{
	}
	
	ThreadFunc_t pfnThread;
	void *		 pParam;
};

//---------------------------------------------------------

static void* ThreadProcConvert( void *pParam )
{
	ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
	AllocateThreadID();
	delete ((ThreadProcInfo_t *)pParam);
	unsigned nRet = (*info.pfnThread)(info.pParam);
	FreeThreadID();
	return ( void * ) (uintp) nRet;
}


//---------------------------------------------------------

ThreadHandle_t * CreateTestThreads( ThreadFunc_t fnThread, int numThreads, int nProcessorsToDistribute )
{
	ThreadHandle_t *pHandles = (new ThreadHandle_t[numThreads+1]) + 1;
	pHandles[-1] = (ThreadHandle_t)INT_TO_POINTER( numThreads );
	for( int i = 0; i < numThreads; ++i )
	{
		//TestThreads();
		ThreadHandle_t hThread;
		const unsigned int nDefaultStackSize = 64 * 1024; // this stack size is used in case stackSize == 0
		hThread = CreateSimpleThread( fnThread, INT_TO_POINTER( i ), nDefaultStackSize );

		if ( nProcessorsToDistribute )
		{
			int32 mask = 1 << (i % nProcessorsToDistribute);
			ThreadSetAffinity( hThread, mask );
		}
		
/*		
		ThreadProcInfoUnion_t info;
		info.val.pfnThread = fnThread;
		info.val.pParam = (void*)(i);
		if ( int nError = sys_ppu_thread_create( &hThread, ThreadProcConvertUnion, info.val64, 1001, nDefaultStackSize, SYS_PPU_THREAD_CREATE_JOINABLE, "SimpleThread" ) != CELL_OK )
		{
			printf( "PROBLEM!\n" );
			Error( "Cannot create thread, error %d\n", nError );
			return 0;
		}
*/
		//ThreadHandle_t hThread = CreateSimpleThread( fnThread, (void*)i );
		pHandles[i] = hThread;
	}
//	printf("Finishinged CreateTestThreads(%p,%d)\n", (void*)fnThread,  numThreads );
	return pHandles;
}

void JoinTestThreads( ThreadHandle_t *pHandles )
{
	int nCount = POINTER_TO_INT( (uintp)pHandles[-1] );
// 	printf("Joining test threads @%p[%d]:\n", pHandles, nCount );
// 	for( int i = 0; i < nCount; ++i )
// 	{
// 		printf("    %p,\n", (void*)pHandles[i] );
// 	}
	for( int i = 0; i < nCount; ++i )
	{
//		printf( "Joining %p", (void*) pHandles[i] );
//		if( !i ) sys_timer_usleep(100000);
		ThreadJoin( pHandles[i] );
		ReleaseThreadHandle( pHandles[i] );
	}
	delete[]( pHandles - 1 );
}


ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void *pParam, unsigned stackSize )
{
	return (ThreadHandle_t)ks::platform::thread_create( ThreadProcConvert, new ThreadProcInfo_t( pfnThread, pParam ), stackSize );
}


bool ReleaseThreadHandle( ThreadHandle_t hThread )
{
	return true;
}

//-----------------------------------------------------------------------------
//
// Wrappers for other simple threading operations
//
//-----------------------------------------------------------------------------

void ThreadSleep(unsigned nMilliseconds)
{
   ks::platform::sleep_nanos( (unsigned long long)nMilliseconds * 1000000ull ); 
}

//-----------------------------------------------------------------------------
void ThreadNanoSleep(unsigned ns)
{
	ks::platform::sleep_nanos( ns );
}


//-----------------------------------------------------------------------------

ThreadId_t ThreadGetCurrentId()
{
	return (ThreadId_t)ks::platform::thread_current_id();
}

//-----------------------------------------------------------------------------
ThreadHandle_t ThreadGetCurrentHandle()
{
	return (ThreadHandle_t)ks::platform::thread_current();
}

// On PS3, this will return true for zombie threads
bool ThreadIsThreadIdRunning( ThreadId_t uThreadId )
{
	return ks::platform::thread_id_running( (unsigned long long)uThreadId );
}

//-----------------------------------------------------------------------------

int ThreadGetPriority( ThreadHandle_t hThread )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

	return 0;
}

//-----------------------------------------------------------------------------

bool ThreadSetPriority( ThreadHandle_t hThread, int priority )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

	return true;
}

//-----------------------------------------------------------------------------

void ThreadSetAffinity( ThreadHandle_t hThread, int nAffinityMask )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

	ks::platform::thread_set_affinity( (ks::platform::thread *)hThread, (unsigned long long)(unsigned int)nAffinityMask );

}

//-----------------------------------------------------------------------------

// How many CPUs the process was pinned to by ThreadPinToFastestCores(), or 0 if it was
// not pinned. Subsystems that size themselves around the fast cluster read this.
static int s_nPinnedFastCores = 0;

int ThreadPinToFastestCores()
{
	constexpr int kMaxCPUs = 256;
	const int nCPUs = ks::platform::processor_count();
	if ( nCPUs <= 0 || nCPUs > kMaxCPUs )
		return 0;

	long freq[ kMaxCPUs ] = {};
	long nTopFreq = 0;
	for ( int i = 0; i < nCPUs; i++ )
	{
		char szPath[ 96 ];
		std::snprintf( szPath, sizeof( szPath ), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i );
		if ( std::FILE *pFile = std::fopen( szPath, "r" ) )
		{
			if ( std::fscanf( pFile, "%ld", &freq[ i ] ) != 1 )
				freq[ i ] = 0;
			std::fclose( pFile );
			if ( freq[ i ] > nTopFreq )
				nTopFreq = freq[ i ];
		}
	}

	bool fast[ kMaxCPUs ] = {};
	int nFast = 0;
	for ( int i = 0; i < nCPUs; i++ )
	{
		fast[ i ] = freq[ i ] * 20 >= nTopFreq * 19;	// within 5% of the top clock == same frequency tier
		nFast += fast[ i ];
	}

	// no cpufreq or a uniform CPU sets every bit, and pinning below 4 cores trades stutter for starvation
	if ( nFast == nCPUs || nFast < 4 )
		return 0;

	if ( !ks::platform::set_process_affinity( fast, nCPUs ) )
		return 0;

	s_nPinnedFastCores = nFast;
	return nFast;
}

//-----------------------------------------------------------------------------

int GetPinnedFastCoreCount()
{
	return s_nPinnedFastCores;
}

//-----------------------------------------------------------------------------

ThreadId_t InitMainThread()
{
	ThreadSetDebugName( "MainThrd" );

	return ThreadGetCurrentId();
}

ThreadId_t g_ThreadMainThreadID = InitMainThread();

bool ThreadInMainThread()
{
	return ( ThreadGetCurrentId() == g_ThreadMainThreadID );
}

void DeclareCurrentThreadIsMainThread()
{
	g_ThreadMainThreadID = ThreadGetCurrentId();
}


//-----------------------------------------------------------------------------
bool ThreadJoin( ThreadHandle_t hThread, unsigned timeout )
{
	if ( !hThread )
	{
		return false;
	}

	return ks::platform::thread_join( (ks::platform::thread *)hThread );
}

//-----------------------------------------------------------------------------
void ThreadSetDebugName( ThreadHandle_t hThread, const char *pszName )
{
}


//-----------------------------------------------------------------------------
// Used to thread LoadLibrary on the 360
//-----------------------------------------------------------------------------
static ThreadedLoadLibraryFunc_t s_ThreadedLoadLibraryFunc = 0;
void SetThreadedLoadLibraryFunc( ThreadedLoadLibraryFunc_t func )
{
	s_ThreadedLoadLibraryFunc = func;
}

ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc()
{
	return s_ThreadedLoadLibraryFunc;
}


//-----------------------------------------------------------------------------
//
//   CThreadSyncObject (note nothing uses this directly (I think) )
//
//-----------------------------------------------------------------------------


CThreadSyncObject::CThreadSyncObject()
  : m_bInitalized( false )
{
}

//---------------------------------------------------------

CThreadSyncObject::~CThreadSyncObject()
{
   if ( m_bInitalized )
   {
		ks::platform::condition_destroy( m_Condition );
		ks::platform::mutex_destroy( m_Mutex );
		m_Condition = nullptr;
		m_Mutex = nullptr;
		m_bInitalized = false;
   }
}

//---------------------------------------------------------

bool CThreadSyncObject::operator!() const
{
   return !m_bInitalized;
}

//---------------------------------------------------------

void CThreadSyncObject::AssertUseable()
{
   contract_assert( m_bInitalized && "Thread synchronization object is unuseable" );
}

//---------------------------------------------------------

bool CThreadSyncObject::Wait( uint32 dwTimeout )
{
	AssertUseable();
	ks::platform::mutex_lock( m_Mutex );
	bool bRet = false;
	if ( m_cSet > 0 )
	{
		bRet = true;
		m_bWakeForEvent = false;
	}
	else
	{
		bool timedOut = false;
		while ( !m_bWakeForEvent )
		{
			unsigned long long timeout = ( dwTimeout == TT_INFINITE ) ? ~0ull : (unsigned long long)dwTimeout * 1000000ull;
			if ( dwTimeout == TT_INFINITE && m_bManualReset )
				timeout = 10ull * 1000000ull;

			bool signaled = ks::platform::condition_wait( m_Condition, m_Mutex, timeout, &timedOut );
			bRet = signaled;

			if ( m_bManualReset )
			{
				if ( m_cSet )
					break;
				if ( dwTimeout == TT_INFINITE && timedOut )
					continue;
				break;
			}
			break;
		}
		if ( bRet )
			m_bWakeForEvent = false;
	}
	if ( !m_bManualReset && bRet )
		m_cSet = 0;
	ks::platform::mutex_unlock( m_Mutex );
	return bRet;
}

uint32 CThreadSyncObject::WaitForMultiple( int nObjects, CThreadSyncObject **ppObjects, bool bWaitAll, uint32 dwTimeout )
{
	ks::ScratchArena<> scratch;
	
	// TODO: Need a more efficient implementation of this.
	uint32 dwStartTime = 0;
	
	if ( dwTimeout != TT_INFINITE )
		dwStartTime = Plat_MSTime();
	
	// If bWaitAll = true, then we need to track which ones were triggered.
	char *pWasTriggered = nullptr;
	int nTriggered = 0;
	if ( bWaitAll )
	{
		pWasTriggered = (char*)scratch.AllocBytes( nObjects );
		std::memset( pWasTriggered, 0, nObjects );
	}

	while ( 1 )
	{
		for ( int i=0; i < nObjects; i++ )
		{
			if ( bWaitAll && pWasTriggered[i] )
				continue;

			if ( ppObjects[i]->Wait( 0 ) )
			{
				++nTriggered;
				if ( bWaitAll )
				{
					if ( nTriggered == nObjects )
						return 0;
					else
						pWasTriggered[i] = 1;
				}
				else
				{
					return i;
				}
			}
		}

		// Timeout?
		if ( dwTimeout != TT_INFINITE )
		{
			if ( Plat_MSTime() - dwStartTime >= dwTimeout )
				return TW_TIMEOUT;
		}

		ThreadSleep( 0 );
	}

}

uint32 CThreadSyncObject::WaitForMultiple( int nObjects, CThreadSyncObject *pObjects, bool bWaitAll, uint32 dwTimeout )
{
	ks::ScratchArena<> scratch;

	CThreadSyncObject **ppObjects = scratch.Alloc<CThreadSyncObject*>( nObjects );
	for ( int i=0; i < nObjects; i++ )
	{
		ppObjects[i] = &pObjects[i];
	}

	return WaitForMultiple( nObjects, ppObjects, bWaitAll, dwTimeout );

}

// To implement these, I need to check that casts are safe
uint32 CThreadEvent::WaitForMultiple( int nObjects, CThreadEvent *pObjects, bool bWaitAll, uint32 dwTimeout )
{
	// If data ever gets added to CThreadEvent, then we need a different implementation.
	static_assert( sizeof( CThreadSyncObject ) == 0 || sizeof( CThreadEvent ) == sizeof( CThreadSyncObject ) );
	return CThreadSyncObject::WaitForMultiple( nObjects, (CThreadSyncObject*)pObjects, bWaitAll, dwTimeout );
}


uint32 CThreadEvent::WaitForMultiple( int nObjects, CThreadEvent **ppObjects, bool bWaitAll, uint32 dwTimeout )
{
	// If data ever gets added to CThreadEvent, then we need a different implementation.
	static_assert( sizeof( CThreadSyncObject )== 0 || sizeof( CThreadEvent ) == sizeof( CThreadSyncObject ) );
	return CThreadSyncObject::WaitForMultiple( nObjects, (CThreadSyncObject**)ppObjects, bWaitAll, dwTimeout );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadEvent::CThreadEvent( bool bManualReset )
{
    m_Mutex = ks::platform::mutex_create();
    m_Condition = ks::platform::condition_create();
    m_bInitalized = true;
    m_cSet = 0;
	m_bWakeForEvent = false;
    m_bManualReset = bManualReset;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------


//---------------------------------------------------------

bool CThreadEvent::Set()
{
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
   AssertUseable();
   ks::platform::mutex_lock( m_Mutex );
	m_cSet = 1;
	m_bWakeForEvent = true;
	ks::platform::condition_signal( m_Condition );
   ks::platform::mutex_unlock( m_Mutex );
   return true;


//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
}

//---------------------------------------------------------

bool CThreadEvent::Reset()
{
   AssertUseable();
	ks::platform::mutex_lock( m_Mutex );
	m_cSet = 0;
	m_bWakeForEvent = false;
	ks::platform::mutex_unlock( m_Mutex );
	return true; 
}

//---------------------------------------------------------

bool CThreadEvent::Check()
{
   AssertUseable();
	return Wait( 0 );
}


bool CThreadEvent::Wait( uint32 dwTimeout )
{
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


	return CThreadSyncObject::Wait( dwTimeout );

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------


long ThreadInterlockedIncrement( long volatile *pDest )
{
	return __sync_fetch_and_add( pDest, 1 ) + 1;
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	return __sync_fetch_and_sub( pDest, 1 ) - 1;
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	return __sync_lock_test_and_set( pDest, value );
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	return  __sync_fetch_and_add( pDest, value );
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	return  __sync_val_compare_and_swap( pDest, comperand, value );
}

bool ThreadInterlockedAssignIf( long volatile *pDest, long value, long comperand )
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}

void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	return __sync_lock_test_and_set( pDest, value );
}

void *ThreadInterlockedCompareExchangePointer( void * volatile *p, void *value, void *comparand ) {
	return (void *)( ( intp )ThreadInterlockedCompareExchange64( reinterpret_cast<intp volatile *>(p), reinterpret_cast<intp>(value), reinterpret_cast<intp>(comparand) ) );
}

int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
	return __sync_val_compare_and_swap( pDest, comperand, value  );
}

bool ThreadInterlockedAssignIf64( int64 volatile * pDest, int64 value, int64 comperand ) 
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}


int64 ThreadInterlockedExchange64( int64 volatile *pDest, int64 value )
{
	contract_assert( (std::size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

	return Old;
}


//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock
//
//-----------------------------------------------------------------------------

void CThreadSpinRWLock::SpinLockForWrite()
{
	int i;

	if ( TryLockForWrite_UnforcedInline() )
	{
		return;
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::SpinLockForRead()
{
	int i;
	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

// The CThread implementation needs to be inlined for performance on the PS3 - It makes a difference of more than 1ms/frame
// for other platforms, we include the .inl in the .cpp file where it existed before

// This file is included in threadtools.h for PS3 and threadtools.cpp for all other platforms
//
// Do not #include other files here


// this is defined in the .cpp for the PS3 to avoid introducing a dependency for files including the header
thread_local CThread * g_pCurThread;


CThread::CThread() :	
m_threadId( 0 ),
m_threadZombieId( 0 ) ,
m_result( 0 ),
m_flags( 0 )
{
	m_szName[0] = 0;
	m_NotSuspendedEvent.Set();
}

//---------------------------------------------------------

CThread::~CThread()
{
	if ( m_threadId )
	{
		if ( IsAlive() )
		{
			Msg( "Illegal termination of worker thread! Threads must negotiate an end to the thread before the CThread object is destroyed.\n" ); 
			if ( GetCurrentCThread() == this )
			{
				Stop(); // BUGBUG: Alfred - this doesn't make sense, this destructor fires from the hosting thread not the thread itself!!
			}
		}
	}
	if ( m_threadZombieId )
	{
		// just clean up zombie threads immediately (the destructor is fired from the hosting thread)
		Join();
	}
}


//---------------------------------------------------------

const char *CThread::GetName()
{
	std::scoped_lock lock( m_Lock );
	if ( !m_szName[0] )
	{
		std::snprintf( m_szName, sizeof(m_szName) - 1, "Thread(%p/0x%p)", this, m_threadId );
		m_szName[sizeof(m_szName) - 1] = 0;
	}
	return m_szName;
}

//---------------------------------------------------------

void CThread::SetName(const char *pszName)
{
	std::scoped_lock lock( m_Lock );
	std::strncpy( m_szName, pszName, sizeof(m_szName) - 1 );
	m_szName[sizeof(m_szName) - 1] = 0;
}

//-----------------------------------------------------
// Functions for the other threads
//-----------------------------------------------------

// Start thread running  - error if already running
bool CThread::Start( unsigned nBytesStack, ThreadPriorityEnum_t nPriority )
{
	std::scoped_lock lock( m_Lock );

	if ( IsAlive() )
	{
		contract_assert( false && "Tried to create a thread that has already been created!" );
		return false;
	}

	bool  bInitSuccess = false;
	CThreadEvent createComplete;
	ThreadInit_t init = { this, &createComplete, &bInitSuccess };

	m_threadInit = ThreadInit_t( init );
	m_threadId = ks::platform::thread_create( (void *(*)(void *))GetThreadProc(), &m_threadInit, std::max( nBytesStack, 1024u*1024 ) );
	if ( m_threadId == nullptr )
	{
		contract_assert( false && "Failed to create thread (error 0x%x)" );
		return false;
	}

	// bInitSuccess is written by the new thread (via init.pfInitSuccess, which points at this
	// stack slot) just before it signals createComplete, and read below after waiting on that
	// event -- the event's mutex is what orders the two. There used to be a "bInitSuccess = true;"
	// here, before the wait: it raced with the worker's write, and because it ran unconditionally
	// the "Thread failed to initialize" check below could never fire.


	if ( !WaitForCreateComplete( &createComplete ) )
	{
		Msg( "Thread failed to initialize\n" );

		return false;
	}

	if ( !bInitSuccess )
	{
		Msg( "Thread failed to initialize\n" );
		m_threadId = 0;
		m_threadZombieId = 0;
		return false;
	}


	return !!m_threadId;
}

//---------------------------------------------------------
//
// Return true if the thread has been created and hasn't yet exited
//

bool CThread::IsAlive()
{
	return !!m_threadId;
}

// This method causes the current thread to wait until this thread
// is no longer alive.
bool CThread::Join( unsigned timeout )
{
	if ( m_threadId || m_threadZombieId )
	{
		contract_assert( ( GetCurrentCThread() != this ) && "Thread cannot be joined with self" );

		ThreadHandle_t handle = ( m_threadId != nullptr ) ? (ThreadHandle_t)m_threadId : (ThreadHandle_t)(std::uintptr_t)m_threadZombieId;
		bool ret = ThreadJoin( handle, timeout );
		m_threadZombieId = 0;
		return ret;
	}
	return true;
}

//---------------------------------------------------------

ThreadHandle_t CThread::GetThreadHandle()
{
	return (ThreadHandle_t)m_threadId;
}


//---------------------------------------------------------

int CThread::GetResult()
{
	return m_result;
}

//-----------------------------------------------------
// Functions for both this, and maybe, and other threads
//-----------------------------------------------------

// Forcibly, abnormally, but relatively cleanly stop the thread
//

void CThread::Stop(int exitCode)
{
	if ( !IsAlive() )
		return;

	if ( GetCurrentCThread() == this )
	{
		m_result = exitCode;
		if ( !( m_flags & SUPPORT_STOP_PROTOCOL ) )
		{
			OnExit();
			g_pCurThread = nullptr;

			m_threadId = 0;
			m_threadZombieId = 0;
		}
		else
		{
			throw exitCode;
		}
	}
	else
		contract_assert( false && "Only thread can stop self: Use a higher-level protocol" );
}

//---------------------------------------------------------

// Get the priority
int CThread::GetPriority() const
{
	return ks::platform::thread_priority( m_threadId );
}

//---------------------------------------------------------

// Set the priority
bool CThread::SetPriority(int priority)
{
	return ThreadSetPriority( (ThreadHandle_t)m_threadId, priority );
}

//---------------------------------------------------------

// Suspend a thread
unsigned CThread::Suspend()
{
	contract_assert( ( ThreadGetCurrentId() == (ThreadId_t)m_threadId ) && "Cannot call CThread::Suspend from outside thread" );

	if ( ThreadGetCurrentId() != (ThreadId_t)m_threadId )
	{
		DebuggerBreakIfDebugging();
	}

	m_NotSuspendedEvent.Reset();
	m_NotSuspendedEvent.Wait();

	return 0;
}


//---------------------------------------------------------

unsigned CThread::Resume()
{
	if ( m_NotSuspendedEvent.Check() )
	{
		DevWarning( "Called Resume() on a thread that is not suspended!\n" );
	}
	m_NotSuspendedEvent.Set();
	return 0;
}

//---------------------------------------------------------

// Force hard-termination of thread.  Used for critical failures.
bool CThread::Terminate(int exitCode)
{
	ks::platform::thread_terminate( m_threadId );
	m_threadId = nullptr;
	return true;
}

//-----------------------------------------------------
// Global methods
//-----------------------------------------------------

// Get the Thread object that represents the current thread, if any.
// Can return NULL if the current thread was not created using
// CThread
//

CThread *CThread::GetCurrentCThread()
{
	return g_pCurThread;
}

//---------------------------------------------------------
//
// Offer a context switch. Under Win32, equivalent to Sleep(0)
//

void CThread::Yield()
{
	ks::platform::thread_yield();
}

//---------------------------------------------------------
//
// This method causes the current thread to yield and not to be
// scheduled for further execution until a certain amount of real
// time has elapsed, more or less. Duration is in milliseconds

void CThread::Sleep( unsigned duration )
{
	ks::platform::sleep_nanos( (unsigned long long)duration * 1000000ull );
}

//---------------------------------------------------------

// Optional pre-run call, with ability to fail-create. Note Init()
// is forced synchronous with Start()
bool CThread::Init()
{
	return true;
}

//---------------------------------------------------------


// Called when the thread exits
void CThread::OnExit() { }

// Allow for custom start waiting
bool CThread::WaitForCreateComplete( CThreadEvent *pEvent )
{
	// Force serialized thread creation...
	if (!pEvent->Wait(60000))
	{
		contract_assert( false && "Probably deadlock or failure waiting for thread to initialize." );
		return false;
	}
	return true;
}

bool CThread::IsThreadRunning()
{
	return ThreadIsThreadIdRunning( (ThreadId_t)m_threadId );
}

//---------------------------------------------------------
CThread::ThreadProc_t CThread::GetThreadProc()
{
	return ThreadProc;
}

void CThread::ThreadProcRunWithMinidumpHandler( void *pv )
{
	ThreadInit_t *pInit = reinterpret_cast<ThreadInit_t*>(pv);
	pInit->pThread->m_result = pInit->pThread->Run();
}

void* CThread::ThreadProc(LPVOID pv)
{
	ThreadInit_t *pInit = reinterpret_cast<ThreadInit_t*>(pv);

	AllocateThreadID();

	CThread *pThread = pInit->pThread;
	g_pCurThread = pThread;

	pThread->m_pStackBase = AlignValue( &pThread, 4096 );

	pInit->pThread->m_result = -1;

	bool bInitSuccess = true;
	if ( pInit->pfInitSuccess )
		*(pInit->pfInitSuccess) = false;

	try
	{
		bInitSuccess = pInit->pThread->Init();
	}

	catch (...)
	{
		pInit->pInitCompleteEvent->Set();
		throw;
	}

	if ( pInit->pfInitSuccess )
		*(pInit->pfInitSuccess) = bInitSuccess;
	pInit->pInitCompleteEvent->Set();
	if (!bInitSuccess)
		return 0;

	if ( !Plat_IsInDebugSession() && (pInit->pThread->m_flags & SUPPORT_STOP_PROTOCOL) )
	{
		try
		{
			pInit->pThread->m_result = pInit->pThread->Run();
		}

		catch (...)
		{
		}
	}
	else
	{
		pInit->pThread->m_result = pInit->pThread->Run();
	}

	pInit->pThread->OnExit();
	g_pCurThread = nullptr;
	FreeThreadID();

	std::scoped_lock lock( pThread->m_Lock );
	pThread->m_threadZombieId = (uint64)(std::uintptr_t)pThread->m_threadId;
	pThread->m_threadId = nullptr;

	pThread->m_ExitEvent.Set();

	return (void*)(uintp)pInit->pThread->m_result;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CWorkerThread::CWorkerThread()
:	m_EventSend(true),                 // must be manual-reset for PeekCall()
	m_EventComplete(true),             // must be manual-reset to handle multiple wait with thread properly
	m_Param(0),
	m_ReturnVal(0)
{
}

//---------------------------------------------------------

int CWorkerThread::CallWorker(unsigned dw, unsigned timeout, bool fBoostWorkerPriorityToMaster)
{
	return Call(dw, timeout, fBoostWorkerPriorityToMaster);
}

//---------------------------------------------------------

int CWorkerThread::CallMaster(unsigned dw, unsigned timeout)
{
	return Call(dw, timeout, false);
}

//---------------------------------------------------------

CThreadEvent &CWorkerThread::GetCallHandle()
{
	return m_EventSend;
}

//---------------------------------------------------------

unsigned CWorkerThread::GetCallParam() const
{
	return m_Param;
}

//---------------------------------------------------------

int CWorkerThread::BoostPriority()
{
	int iInitialPriority = GetPriority();

	const int iNewPriority = ThreadGetPriority( (ThreadHandle_t)GetThreadID() );
	if (iNewPriority > iInitialPriority)
		ThreadSetPriority( (ThreadHandle_t)GetThreadID(), iNewPriority);

	return iInitialPriority;
}

//---------------------------------------------------------
static uint32 DefaultWaitFunc( uint32 nHandles, CThreadEvent** ppHandles, int bWaitAll, uint32 timeout )
{
	return CThreadEvent::WaitForMultiple( nHandles, ppHandles, bWaitAll!=0, timeout ) ;
}


int CWorkerThread::Call(unsigned dwParam, unsigned timeout, bool fBoostPriority, WaitFunc_t waitFunc)
{

	std::scoped_lock lock( m_Lock );

	if (!IsAlive())
		return WTCR_FAIL;

	int iInitialPriority = 0;
	if (fBoostPriority)
	{
		iInitialPriority = BoostPriority();
	}

	// set the parameter, signal the worker thread, wait for the completion to be signaled
	m_Param = dwParam;

	m_EventComplete.Reset();
	m_EventSend.Set();

	// Return what WaitForReply decided rather than re-reading m_ReturnVal: on the timeout paths
	// that field was deliberately left alone (see WaitForReply).
	int iReturnVal = WaitForReply( timeout, waitFunc );

	if (fBoostPriority)
		SetPriority(iInitialPriority);

	return iReturnVal;
}

//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------
int CWorkerThread::WaitForReply( unsigned timeout )
{
	return WaitForReply( timeout, nullptr );
}

int CWorkerThread::WaitForReply( unsigned timeout, WaitFunc_t pfnWait )
{
	if (!pfnWait)
	{
		pfnWait = &DefaultWaitFunc;
	}

	CThreadEvent *waits[] =
	{
		&m_EventComplete,
		&m_ExitEvent
	};

	unsigned result;
	bool bInDebugger = Plat_IsInDebugSession();

	uint32 dwActualTimeout = ( (timeout==TT_INFINITE) ? 30000 : timeout );

	do
	{

		result = (*pfnWait)( std::size( waits ), waits, false, dwActualTimeout );

		contract_assert( ( timeout != TT_INFINITE || result != TW_TIMEOUT ) && "Possible hung thread, call to thread timed out" );

	} while ( bInDebugger && ( timeout == TT_INFINITE && result == TW_TIMEOUT ) );

	if ( result != 0 )
	{
		// These return the sentinel directly rather than storing it in m_ReturnVal. The worker
		// writes that field in Reply(), and Reply() cannot take m_Lock -- Call() holds it while
		// waiting here, so it would deadlock -- which made this a write-write race with the
		// worker whenever a call timed out and the worker ran on anyway. On the success path
		// below the event supplies the ordering; on these paths nothing does, so the caller must
		// not touch the field.
		if (result == TW_TIMEOUT)
		{
			return WTCR_TIMEOUT;
		}
		else if (result == 1)
		{
			DevMsg( 2, "Thread failed to respond, probably exited\n");
			m_EventSend.Reset();
			return WTCR_TIMEOUT;
		}
		else
		{
			m_EventSend.Reset();
			return WTCR_THREAD_GONE;
		}
	}

	return m_ReturnVal;
}


//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned * pResult)
{
	return WaitForCall(TT_INFINITE, pResult);
}

//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned dwTimeout, unsigned * pResult)
{
	bool returnVal = m_EventSend.Wait(dwTimeout);
	if (pResult)
		*pResult = m_Param;
	return returnVal;
}

//---------------------------------------------------------
//
// is there a request?
//

bool CWorkerThread::PeekCall(unsigned * pParam)
{
	if (!m_EventSend.Check())
	{
		return false;
	}
	else
	{
		if (pParam)
		{
			*pParam = m_Param;
		}
		return true;
	}
}

//---------------------------------------------------------
//
// Reply to the request
//

void CWorkerThread::Reply(unsigned dw)
{
	m_Param = 0;
	m_ReturnVal = dw;

	// The request is now complete so PeekCall() should fail from
	// now on
	//
	// This event should be reset BEFORE we signal the client
	m_EventSend.Reset();

	// Tell the client we're finished
	m_EventComplete.Set();
}


//-----------------------------------------------------------------------------
