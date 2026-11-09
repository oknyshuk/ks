// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:threadtools;

import std;
import ks.platform;
import :prelude;
import :color;
import :logging;
import :dbg;
import :memalloc;

export extern "C++" {

inline constexpr unsigned WAIT_OBJECT_0 = 0x00000000u;
inline constexpr unsigned WAIT_TIMEOUT = 0x00000102u;
inline constexpr unsigned WAIT_FAILED = 0xFFFFFFFFu;

// #define THREAD_PROFILER 1

// maximum number of threads that can wait on one object
inline constexpr auto CTHREADEVENT_MAX_WAITING_THREADS = 4;

// Start thread running  - error if already running
enum ThreadPriorityEnum_t
{
    // We can use nice on Linux threads to change scheduling.
    // pthreads on Linux only allows priority setting on
    // real-time threads.
    // NOTE: Lower numbers are higher priority, thus the need
    // for TP_IS_PRIORITY_HIGHER.
	TP_PRIORITY_DEFAULT = 0,
	TP_PRIORITY_NORMAL = 0,
	TP_PRIORITY_HIGH = -10,
	TP_PRIORITY_LOW = 10,
	TP_PRIORITY_HIGHEST = -20,
	TP_PRIORITY_LOWEST = 19,
};

extern bool gbCheckNotMultithreaded;

inline constexpr auto MAX_THREADS_SUPPORTED = 32;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

inline constexpr unsigned TT_INFINITE = 0xffffffff;

typedef uint64 ThreadId_t;

//-----------------------------------------------------------------------------
//
// Simple thread creation. Differs from VCR mode/CreateThread/_beginthreadex
// in that it accepts a standard C function rather than compiler specific one.
//
//-----------------------------------------------------------------------------
struct ThreadHandle_t__;
using ThreadHandle_t = ThreadHandle_t__ *;
typedef uintp (*ThreadFunc_t)( void *pParam );

ThreadHandle_t CreateSimpleThread( ThreadFunc_t, void *pParam, ThreadId_t *pID, unsigned stackSize = 0 );
ThreadHandle_t CreateSimpleThread( ThreadFunc_t, void *pParam, unsigned stackSize = 0 );
bool ReleaseThreadHandle( ThreadHandle_t );

//-----------------------------------------------------------------------------

void ThreadSleep(unsigned duration = 0);
void ThreadNanoSleep(unsigned ns);
ThreadId_t ThreadGetCurrentId();
ThreadHandle_t ThreadGetCurrentHandle();
int ThreadGetPriority( ThreadHandle_t hThread = nullptr );
bool ThreadSetPriority( ThreadHandle_t hThread, int priority );
inline		 bool ThreadSetPriority( int priority ) { return ThreadSetPriority( nullptr, priority ); }
bool ThreadInMainThread();
void DeclareCurrentThreadIsMainThread();

// NOTE: ThreadedLoadLibraryFunc_t needs to return the sleep time in milliseconds or TT_INFINITE
typedef int (*ThreadedLoadLibraryFunc_t)(); 
void SetThreadedLoadLibraryFunc( ThreadedLoadLibraryFunc_t func );
ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc();

inline void ThreadPause()
{
	__asm __volatile( "pause" );
}

bool ThreadJoin( ThreadHandle_t, unsigned timeout = TT_INFINITE );

void ThreadSetDebugName( ThreadHandle_t hThread, const char *pszName );
inline		 void ThreadSetDebugName( const char *pszName ) { ThreadSetDebugName( nullptr, pszName ); }

void ThreadSetAffinity( ThreadHandle_t hThread, int nAffinityMask );
int ThreadPinToFastestCores();	// returns the number of CPUs pinned to, 0 if it did nothing
int GetPinnedFastCoreCount();	// the count ThreadPinToFastestCores() succeeded with, else 0

//-----------------------------------------------------------------------------
//
// Interlock methods. These perform very fast atomic thread
// safe operations. These are especially relevant in a multi-core setting.
//
//-----------------------------------------------------------------------------

// Prevent compiler reordering across this barrier. This is
// sufficient for most purposes on x86/x64.
// http://preshing.com/20120625/memory-ordering-at-compile-time
inline void ThreadMemoryBarrier() { asm volatile( "" ::: "memory" ); }

// linux implementation
inline int32 ThreadInterlockedIncrement( int32 volatile *p )
{
	contract_assert( (std::size_t)p % 4 == 0 );
	return __sync_fetch_and_add( p, 1 ) + 1;
}

inline int32 ThreadInterlockedDecrement( int32 volatile *p )
{
	contract_assert( (std::size_t)p % 4 == 0 ); 
	return __sync_fetch_and_add( p, -1 ) - 1;
}

inline int32 ThreadInterlockedExchange( int32 volatile *p, int32 value )
{
	contract_assert( (std::size_t)p % 4 == 0 );
	// seq_cst exchange, not __sync_lock_test_and_set, which is only an acquire barrier and cannot publish state.
	return __atomic_exchange_n( p, value, __ATOMIC_SEQ_CST );
}

inline int32 ThreadInterlockedExchangeAdd( int32 volatile *p, int32 value )
{
	contract_assert( (std::size_t)p % 4 == 0 ); 
	return __sync_fetch_and_add( p, value );
}
inline int64 ThreadInterlockedExchangeAdd64( int64 volatile *p, int64 value )
{
	contract_assert( ( (std::size_t)p ) % 8 == 0 ); 
	return __sync_fetch_and_add( p, value );
}
inline int32 ThreadInterlockedCompareExchange( int32 volatile *p, int32 value, int32 comperand )
{
	contract_assert( (std::size_t)p % 4 == 0 ); 
	return __sync_val_compare_and_swap( p, comperand, value );
}

inline bool ThreadInterlockedAssignIf( int32 volatile *p, int32 value, int32 comperand )
{
	contract_assert( (std::size_t)p % 4 == 0 );
	return __sync_bool_compare_and_swap( p, comperand, value );
}

[[gnu::noinline]] void *ThreadInterlockedExchangePointer( void * volatile *, void *value );
[[gnu::noinline]] void *ThreadInterlockedCompareExchangePointer( void * volatile *, void *value, void *comperand );
[[gnu::noinline]] bool ThreadInterlockedAssignPointerIf( void * volatile *, void *value, void *comperand );

inline unsigned ThreadInterlockedExchangeSubtract( int32 volatile *p, int32 value )	{ return ThreadInterlockedExchangeAdd( (int32 volatile *)p, -value ); }

inline void const *ThreadInterlockedExchangePointerToConst( void const * volatile *p, void const *value )							{ return ThreadInterlockedExchangePointer( const_cast < void * volatile * > ( p ), const_cast < void * > ( value ) );  }
inline void const *ThreadInterlockedCompareExchangePointerToConst( void const * volatile *p, void const *value, void const *comperand )	{ return ThreadInterlockedCompareExchangePointer( const_cast < void * volatile * > ( p ), const_cast < void * > ( value ), const_cast < void * > ( comperand ) ); }
inline bool ThreadInterlockedAssignPointerToConstIf( void const * volatile *p, void const *value, void const *comperand )			{ return ThreadInterlockedAssignPointerIf( const_cast < void * volatile * > ( p ), const_cast < void * > ( value ), const_cast < void * > ( comperand ) ); }

[[gnu::noinline]] int64 ThreadInterlockedCompareExchange64( int64 volatile *, int64 value, int64 comperand );
[[gnu::noinline]] bool ThreadInterlockedAssignIf64( volatile int64 *pDest, int64 value, int64 comperand );

[[gnu::noinline]] int64 ThreadInterlockedExchange64( int64 volatile *, int64 value );

inline int64 ThreadInterlockedIncrement64( int64 volatile *p )
{
	contract_assert( (std::size_t)p % 8 == 0 );
	return __sync_fetch_and_add( p, 1 ) + 1;
}

inline int64 ThreadInterlockedDecrement64( int64 volatile *p )
{
	contract_assert( (std::size_t)p % 8 == 0 );
	return __sync_fetch_and_add( p, -1 ) - 1;
}

inline unsigned ThreadInterlockedExchangeSubtract( uint32 volatile *p, uint32 value )					{ return ThreadInterlockedExchangeAdd( (int32 volatile *)p, value ); }

inline unsigned ThreadInterlockedIncrement( uint32 volatile *p )										{ return ThreadInterlockedIncrement( (int32 volatile *)p ); }
inline unsigned ThreadInterlockedDecrement( uint32 volatile *p )										{ return ThreadInterlockedDecrement( (int32 volatile *)p ); }
inline unsigned ThreadInterlockedExchange( uint32 volatile *p, uint32 value )							{ return ThreadInterlockedExchange( (int32 volatile *)p, value ); }
inline unsigned ThreadInterlockedExchangeAdd( uint32 volatile *p, uint32 value )						{ return ThreadInterlockedExchangeAdd( (int32 volatile *)p, value ); }
inline unsigned ThreadInterlockedCompareExchange( uint32 volatile *p, uint32 value, uint32 comperand )	{ return ThreadInterlockedCompareExchange( (int32 volatile *)p, value, comperand ); }
inline bool ThreadInterlockedAssignIf( uint32 volatile *p, uint32 value, uint32 comperand )				{ return ThreadInterlockedAssignIf( (int32 volatile *)p, value, comperand ); }

//inline int ThreadInterlockedExchangeSubtract( int volatile *p, int value )	{ return ThreadInterlockedExchangeAdd( (int32 volatile *)p, value ); }
//inline int ThreadInterlockedIncrement( int volatile *p )	{ return ThreadInterlockedIncrement( (int32 volatile *)p ); }
//inline int ThreadInterlockedDecrement( int volatile *p )	{ return ThreadInterlockedDecrement( (int32 volatile *)p ); }
//inline int ThreadInterlockedExchange( int volatile *p, int value )	{ return ThreadInterlockedExchange( (int32 volatile *)p, value ); }
//inline int ThreadInterlockedExchangeAdd( int volatile *p, int value )	{ return ThreadInterlockedExchangeAdd( (int32 volatile *)p, value ); }
//inline int ThreadInterlockedCompareExchange( int volatile *p, int value, int comperand )	{ return ThreadInterlockedCompareExchange( (int32 volatile *)p, value, comperand ); }
//inline bool ThreadInterlockedAssignIf( int volatile *p, int value, int comperand )	{ return ThreadInterlockedAssignIf( (int32 volatile *)p, value, comperand ); }

//-----------------------------------------------------------------------------
// Access to VTune thread profiling
//-----------------------------------------------------------------------------

extern thread_local int g_nThreadID;

//-----------------------------------------------------------------------------
//
// Base class for event, semaphore and mutex objects.
//
//-----------------------------------------------------------------------------

// TW_TIMEOUT must match WAIT_TIMEOUT definition
inline constexpr auto TW_TIMEOUT = 0x00000102;
// TW_FAILED must match WAIT_FAILED definition
inline constexpr auto TW_FAILED = 0xFFFFFFFF;

class CThreadSyncObject
{
public:
	~CThreadSyncObject();

	//-----------------------------------------------------
	// Query if object is useful
	//-----------------------------------------------------
	bool operator!() const;

	//-----------------------------------------------------
	// Access handle
	//-----------------------------------------------------
	//-----------------------------------------------------
	// Wait for a signal from the object
	//-----------------------------------------------------
	bool Wait( uint32 dwTimeout = TT_INFINITE );

	//-----------------------------------------------------
	// Wait for a signal from any of the specified objects.
	//
	// Returns the index of the object that signaled the event
	// or THREADSYNC_TIMEOUT if the timeout was hit before the wait condition was met.
	//
	// Returns TW_FAILED if an incoming object is invalid.
	//
	// If bWaitAll=true, then it'll return 0 if all the objects were set.
	//-----------------------------------------------------
	static uint32 WaitForMultiple( int nObjects, CThreadSyncObject **ppObjects, bool bWaitAll, uint32 dwTimeout = TT_INFINITE );
	
	// This builds a list of pointers and calls straight through to the other WaitForMultiple.
	static uint32 WaitForMultiple( int nObjects, CThreadSyncObject *ppObjects, bool bWaitAll, uint32 dwTimeout = TT_INFINITE );

protected:
	CThreadSyncObject();
	void AssertUseable();

	ks::platform::mutex *m_Mutex = nullptr;
	ks::platform::condition *m_Condition = nullptr;
	bool m_bInitalized;
	int m_cSet;
	bool m_bManualReset;
	bool m_bWakeForEvent;

private:
	CThreadSyncObject( const CThreadSyncObject & );
	CThreadSyncObject &operator=( const CThreadSyncObject & );
};

//-----------------------------------------------------------------------------
//
// CThreadSemaphore
//
//-----------------------------------------------------------------------------

enum NamedEventResult_t
{
	TT_EventDoesntExist	= 0,
	TT_EventNotSignaled,
	TT_EventSignaled
};

class CThreadEvent : public CThreadSyncObject
{
public:
	CThreadEvent( bool fManualReset = false );
	//-----------------------------------------------------
	// Set the state to signaled
	//-----------------------------------------------------
	bool Set();

	//-----------------------------------------------------
	// Set the state to nonsignaled
	//-----------------------------------------------------
	bool Reset();

	//-----------------------------------------------------
	// Check if the event is signaled
	//-----------------------------------------------------
	bool Check(); // Please, use for debugging only!

	bool Wait( uint32 dwTimeout = TT_INFINITE );

	// See CThreadSyncObject for definitions of these functions.
	static uint32 WaitForMultiple( int nObjects, CThreadEvent **ppObjects, bool bWaitAll, uint32 dwTimeout = TT_INFINITE );
	// To implement these, I need to check that casts are safe
	static uint32 WaitForMultiple( int nObjects, CThreadEvent *ppObjects, bool bWaitAll, uint32 dwTimeout = TT_INFINITE );

private:
	CThreadEvent( const CThreadEvent & );
	CThreadEvent &operator=( const CThreadEvent & );
};

// Hard-wired manual event for use in array declarations
class CThreadManualEvent : public CThreadEvent
{
public:
	CThreadManualEvent()
	 :	CThreadEvent( true )
	{
	}
};

int ThreadWaitForObjects( int nEvents, const HANDLE *pHandles, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
inline int ThreadWaitForEvents( int nEvents, const CThreadEvent *pEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE ) { return ThreadWaitForObjects( nEvents, (const HANDLE *)pEvents, bWaitAll, timeout ); }

//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock
//
//-----------------------------------------------------------------------------

class alignas(8) CThreadSpinRWLock
{
public:
	CThreadSpinRWLock()
	{ 
		m_lockInfo.m_i32 = 0;
		m_writerId = 0;
	}

	bool IsLockedForWrite();
	bool IsLockedForRead();

	[[gnu::always_inline]] inline bool TryLockForWrite();
	bool TryLockForWrite_UnforcedInline();

	void LockForWrite();
	void SpinLockForWrite();

	[[gnu::always_inline]] inline bool TryLockForRead();
	bool TryLockForRead_UnforcedInline();

	void LockForRead();
	void SpinLockForRead();

	void UnlockWrite();
	void UnlockRead();

	bool TryLockForWrite() const { return const_cast<CThreadSpinRWLock *>(this)->TryLockForWrite(); }
	bool TryLockForRead() const { return const_cast<CThreadSpinRWLock *>(this)->TryLockForRead(); }
	void LockForRead() const { const_cast<CThreadSpinRWLock *>(this)->LockForRead(); }
	void UnlockRead() const { const_cast<CThreadSpinRWLock *>(this)->UnlockRead(); }
	void LockForWrite() const { const_cast<CThreadSpinRWLock *>(this)->LockForWrite(); }
	void UnlockWrite() const { const_cast<CThreadSpinRWLock *>(this)->UnlockWrite(); }

private:
	enum
	{
		THREAD_SPIN = (8*1024)
	};

	union LockInfo_t
	{
		struct
		{
			uint16 m_nReaders;
			uint16 m_fWriting;
		};
		uint32 m_i32;
	};

	LockInfo_t	m_lockInfo;
	ThreadId_t		m_writerId;
};

//-----------------------------------------------------------------------------
//
// A thread wrapper similar to a Java thread.
//
//-----------------------------------------------------------------------------

class CThread
{
public:
	// fnRun is the thread's body: it runs on the new thread once Start() has been called and its
	// return value is the thread's result.  Classes that own a thread and want the body to be a
	// member pass a lambda that forwards to it.
	explicit CThread( std::move_only_function< int() > fnRun );
	~CThread();

	//-----------------------------------------------------

	const char *GetName();
	void SetName( const char *pszName );

	std::size_t CalcStackDepth( void *pStackVariable )		{ return ((byte *)m_pStackBase - (byte *)pStackVariable); }

	//-----------------------------------------------------
	// Functions for the other threads
	//-----------------------------------------------------

	// Start thread running  - error if already running
	bool Start( unsigned nBytesStack = 0, ThreadPriorityEnum_t nPriority = TP_PRIORITY_DEFAULT );

	// Returns true if thread has been created and hasn't yet exited
	bool IsAlive();

	// This method causes the current thread to wait until this thread
	// is no longer alive.
	bool Join( unsigned timeout = TT_INFINITE );

	// Access the thread handle directly
	ThreadHandle_t GetThreadHandle();

	//-----------------------------------------------------

	int GetResult();

	//-----------------------------------------------------
	// Functions for both this, and maybe, and other threads
	//-----------------------------------------------------

	// Forcibly, abnormally, but relatively cleanly stop the thread
	void Stop( int exitCode = 0 );

	// Get the priority
	int GetPriority() const;

	// Set the priority
	bool SetPriority( int priority );

	// Suspend a thread, can only call from the thread itself
	unsigned Suspend();

	// Resume a suspended thread
	unsigned Resume();

	// Check if thread is suspended
	bool IsSuspended()	{ return !m_NotSuspendedEvent.Check(); }

	// Force hard-termination of thread.  Used for critical failures.
	bool Terminate( int exitCode = 0 );

	//-----------------------------------------------------
	// Global methods
	//-----------------------------------------------------

	// Get the Thread object that represents the current thread, if any.
	// Can return NULL if the current thread was not created using
	// CThread
	static CThread *GetCurrentCThread();

	// Offer a context switch. Under Win32, equivalent to Sleep(0)
	static void Yield();

	// This method causes the current thread to yield and not to be
	// scheduled for further execution until a certain amount of real
	// time has elapsed, more or less. Duration is in milliseconds
	static void Sleep( unsigned duration );

protected:

	const ThreadId_t GetThreadID() const { return (ThreadId_t)ks::platform::thread_id( m_threadId ); }

	static void* ThreadProc( void * pv );
	typedef void* (*ThreadProc_t)( void * pv );
	static void ThreadProcRunWithMinidumpHandler( void *pv );

	bool IsThreadRunning();

	std::recursive_mutex m_Lock;
	CThreadEvent m_ExitEvent;	// Set right before the thread's function exits.

private:
	enum Flags
	{
		SUPPORT_STOP_PROTOCOL = 1 << 0
	};

	// Thread initially runs this. param is actually 'this'. function
	// just gets this and calls ThreadProc
	struct ThreadInit_t
	{
		CThread *     pThread;
		CThreadEvent *pInitCompleteEvent;
		bool *        pfInitSuccess;
	};

	// make copy constructor and assignment operator inaccessible
	CThread( const CThread & );
	CThread &operator=( const CThread & );

	std::move_only_function< int() > m_fnRun;

	ks::platform::thread *m_threadId = nullptr;
	volatile uint64 m_threadZombieId = 0;
	//lwss add - Thread params. These were previously allocated on the heap and leaked.
    ThreadInit_t m_threadInit;
    //lwss end
	int		m_result;
	char	m_szName[32];
	void *	m_pStackBase;
	unsigned m_flags;
	CThreadManualEvent m_NotSuspendedEvent;
};

//-----------------------------------------------------------------------------
// Simple thread class encompasses the notion of a worker thread, handing
// synchronized communication.
//-----------------------------------------------------------------------------

// These are internal reserved error results from a call attempt
enum WTCallResult_t
{
	WTCR_FAIL			= -1,
	WTCR_TIMEOUT		= -2,
	WTCR_THREAD_GONE	= -3,
};

class CWorkerThread : public CThread
{
public:
	explicit CWorkerThread( std::move_only_function< int() > fnRun );

	//-----------------------------------------------------
	//
	// Inter-thread communication
	//
	// Calls in either direction take place on the same "channel."
	// Seperate functions are specified to make identities obvious
	//
	//-----------------------------------------------------

	// Master: Signal the thread, and block for a response
	int CallWorker( unsigned, unsigned timeout = TT_INFINITE, bool fBoostWorkerPriorityToMaster = true );

	// Worker: Signal the thread, and block for a response
	int CallMaster( unsigned, unsigned timeout = TT_INFINITE );

	// Wait for the next request
	bool WaitForCall( unsigned dwTimeout, unsigned *pResult = nullptr );
	bool WaitForCall( unsigned *pResult = nullptr );

	// Is there a request?
	bool PeekCall( unsigned *pParam = nullptr );

	// Reply to the request
	void Reply( unsigned );

	// Wait for a reply in the case when CallWorker() with timeout != TT_INFINITE
	int WaitForReply( unsigned timeout = TT_INFINITE );

	// If you want to do WaitForMultipleObjects you'll need to include
	// this handle in your wait list or you won't be responsive
	CThreadEvent& GetCallHandle();	// (returns m_EventSend)

	// Find out what the request was
	unsigned GetCallParam() const;

	// Boost the worker thread to the master thread, if worker thread is lesser, return old priority
	int BoostPriority();

protected:
	typedef uint32 (        *WaitFunc_t)( uint32 nHandles, CThreadEvent** ppHandles, int bWaitAll, uint32 timeout );
	int Call( unsigned, unsigned timeout, bool fBoost, WaitFunc_t = nullptr );
	int WaitForReply( unsigned timeout, WaitFunc_t );

private:
	CWorkerThread( const CWorkerThread & );
	CWorkerThread &operator=( const CWorkerThread & );

	CThreadEvent	m_EventSend;
	CThreadEvent	m_EventComplete;

	unsigned        m_Param;
	int				m_ReturnVal;
};

//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock inline functions
//
//-----------------------------------------------------------------------------

inline bool CThreadSpinRWLock::IsLockedForWrite()
{
	return ( m_lockInfo.m_fWriting == 1 );
}

inline bool CThreadSpinRWLock::IsLockedForRead()
{
	return ( m_lockInfo.m_nReaders > 0 );
}

[[gnu::always_inline]] inline bool CThreadSpinRWLock::TryLockForWrite()
{
	volatile LockInfo_t &curValue = m_lockInfo;
	if ( !( curValue.m_i32 & 0x00010000 ) && ThreadInterlockedAssignIf( &curValue.m_i32, 0x00010000, 0  ) ) 
	{
		ThreadMemoryBarrier();
		m_writerId = ThreadGetCurrentId();
		return true;
	}

	return false;
}

inline bool CThreadSpinRWLock::TryLockForWrite_UnforcedInline()
{
	if ( TryLockForWrite() )
	{
		return true;
	}

	return false;
}

[[gnu::always_inline]] inline void CThreadSpinRWLock::LockForWrite()
{
	if ( !TryLockForWrite() )
	{
		SpinLockForWrite();
	}
}

[[gnu::always_inline]] inline bool CThreadSpinRWLock::TryLockForRead()
{
	volatile LockInfo_t &curValue = m_lockInfo;
	if ( !( curValue.m_i32 & 0x00010000 ) ) // !m_lockInfo.m_fWriting
	{
		LockInfo_t oldValue; 
		LockInfo_t newValue;
		oldValue.m_i32 = ( curValue.m_i32 & 0xffff );
		newValue.m_i32 = oldValue.m_i32 + 1;

		if ( ThreadInterlockedAssignIf( &m_lockInfo.m_i32, newValue.m_i32, oldValue.m_i32 ) )
		{
			ThreadMemoryBarrier();
			return true;
		}
	}
	return false;
}

inline bool CThreadSpinRWLock::TryLockForRead_UnforcedInline()
{
	return TryLockForRead();
}

[[gnu::always_inline]] inline void CThreadSpinRWLock::LockForRead()
{
	if ( !TryLockForRead() )
	{
		SpinLockForRead();
	}
}

[[gnu::always_inline]] inline void CThreadSpinRWLock::UnlockWrite()
{
	{
		m_writerId = 0;
		ThreadMemoryBarrier();
		m_lockInfo.m_i32 = 0;
	}
}

[[gnu::always_inline]] inline
void CThreadSpinRWLock::UnlockRead()
{
	{
		ThreadMemoryBarrier();
		ThreadInterlockedDecrement( &m_lockInfo.m_i32 );
	}
}

// read data from a memory address
template<class T> [[gnu::always_inline]] inline T ReadVolatileMemory( T const *pPtr )
{
	volatile const T * pVolatilePtr = ( volatile const T * ) pPtr;
	return *pVolatilePtr;
}

//-----------------------------------------------------------------------------

}
