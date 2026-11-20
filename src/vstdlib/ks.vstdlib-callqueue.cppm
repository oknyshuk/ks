// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2006 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib.callqueue;

import std;
import ks.core.basetypes;
import ks.core.tslist;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.functors;
import ks.tier1.memstack;
import ks.tier1.strtools;
export import ks.vstdlib.jobthread;

export extern "C++" {

//-----------------------------------------------------
// Avert thy eyes! Imagine rather:
//
// void QueueCall( <function>, [args1, [arg2,]...]
// void QueueCall( <object>, <function>, [args1, [arg2,]...]
//
// DERIVED_TYPE must provide "template <typename F> void QueueFunctorInternal( F &&f )".
//-----------------------------------------------------
template <typename DERIVED_TYPE>
class CCallQueueMethods
{
public:
	template <typename RET_TYPE, typename... PARAMS, typename... ARGS>
	void QueueCall( RET_TYPE (*pfnProxied)( PARAMS... ), ARGS &&... args )
	{
		static_cast<DERIVED_TYPE *>( this )->QueueFunctorInternal( CreateFunctor( pfnProxied, std::forward<ARGS>(args)... ) );
	}

	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	void QueueCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
	{
		static_cast<DERIVED_TYPE *>( this )->QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}

	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	void QueueCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
	{
		static_cast<DERIVED_TYPE *>( this )->QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}
};

//-----------------------------------------------------
// Thread-safe queue of calls, run on demand (optionally spread over a thread pool)
//-----------------------------------------------------
class CCallQueue : public CCallQueueMethods<CCallQueue>
{
public:
	friend class CCallQueueMethods<CCallQueue>;

	CCallQueue()
		: m_bNoQueue( false )
	{
	}

	void DisableQueue( bool bDisable )
	{
		if ( m_bNoQueue == bDisable )
		{
			return;
		}
		if ( !m_bNoQueue )
			CallQueued();

		m_bNoQueue = bDisable;
	}

	bool IsDisabled() const
	{
		return m_bNoQueue;
	}

	int Count()
	{
		return m_queue.Count();
	}

	void CallQueued()
	{
		if ( !m_queue.Count() )
		{
			return;
		}

		m_queue.PushItem( nullptr );

		CFunctor *pFunctor = nullptr;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
			(*pFunctor)();
			delete pFunctor;
		}

	}

	void ParallelCallQueued( IThreadPool *pPool = nullptr )
	{
		if ( ! pPool ) 
		{
			pPool = g_pThreadPool;
		}
		int nNumThreads = 1;

		if ( pPool )
		{
			nNumThreads = std::min( pPool->NumThreads(), std::max( 1, Count() ) );
		}
		
		if ( nNumThreads < 2 )
		{
			CallQueued();
		}
		else
		{
			int *pDummy = nullptr;
			ParallelProcess( pPool, pDummy, nNumThreads, this, &CCallQueue::ExecuteWrapper );
		}
	}

	void Flush()
	{
		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
			delete pFunctor;
		}
	}

protected:
	void ExecuteWrapper( int &nDummy )						// to match paralell process function template
	{
		CallQueued();
	}

	template <typename F>
	void QueueFunctorInternal( F &&f )
	{
		if ( !m_bNoQueue )
		{
			m_queue.PushItem( new CFunctor( std::forward<F>( f ) ) );
		}
		else
		{
			f();
		}
	}

private:
	CTSQueue<CFunctor *> m_queue;
	bool m_bNoQueue;
};

//-----------------------------------------------------
// Single-threaded call queue backed by a bump allocator. Both the queued calls and
// whatever the caller copies in with Copy()/CopyArray() live in the allocator, which
// is recycled in one go once the queue has been run. All calls are made, in order,
// after the last one was added.
//
// A queued call's bound arguments are destroyed right after it has run (or, if the
// queue is flushed, when it is discarded), exactly like an owning functor.
//-----------------------------------------------------
class ICallQueue : public CCallQueueMethods<ICallQueue>
{
public:
	friend class CCallQueueMethods<ICallQueue>;

	std::size_t GetMemoryUsed()
	{
		return m_Allocator.GetUsed();
	}

	int Count() const
	{
		int i = 0;
		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			i++;
			pCurrent = pCurrent->pNext;
		}
		return i;
	}

	template <typename T>
	T &Copy( T &item )
	{
		T *pCopy = (T *)m_Allocator.Alloc( sizeof(T) );
		std::memcpy( pCopy, &item, sizeof(T) );
		return *pCopy;
	}

	template <typename T>
	T *CopyArray( T *p, int n )
	{
		T *pCopy = (T *)m_Allocator.Alloc( sizeof(T) * n );
		std::memcpy( (void *)pCopy, p, sizeof(T) * n );
		return pCopy;
	}

	template <const char *>
	const char *Copy( const char *psz )
	{
		int len = V_strlen( psz );
		char *pCopy = (char *)m_Allocator.Alloc( len + 1 );
		std::memcpy( pCopy, psz, len + 1 );
		return pCopy;

	}

	void CallQueued()
	{
		if ( !m_pHead )
		{
			return;
		}

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			pCurrent->fn();
			Elem_t *pNext = pCurrent->pNext;
			std::destroy_at( pCurrent );
			pCurrent = pNext;
		}
		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = nullptr;
	}

	void Flush()
	{
		if ( !m_pHead )
		{
			return;
		}

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			Elem_t *pNext = pCurrent->pNext;
			std::destroy_at( pCurrent );
			pCurrent = pNext;
		}

		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = nullptr;
	}

protected:
	ICallQueue()
		: m_pHead( nullptr ), m_pTail( nullptr )
	{
	}

	template <typename F>
	void QueueFunctorInternal( F &&f )
	{
		typedef std::remove_cvref_t<F> Call_t;
		static_assert( alignof( Call_t ) <= 16, "queued calls are bump allocated with 16 byte alignment" );

		Call_t *pCall = new ( m_Allocator.Alloc( sizeof(Call_t) ) ) Call_t( std::forward<F>( f ) );
		Elem_t *pNew = new ( m_Allocator.Alloc( sizeof(Elem_t) ) ) Elem_t{ nullptr, CFunctor( CArenaCall<Call_t>( pCall ) ) };
		if ( m_pTail )
		{
			m_pTail->pNext = pNew;
			m_pTail = pNew;
		}
		else
		{
			m_pHead = m_pTail = pNew;
		}
	}

	CMemoryStack m_Allocator;

private:
	// Refers to a call living in the bump allocator; destroys it (but does not free the memory).
	template <typename CALL_TYPE>
	class CArenaCall
	{
	public:
		explicit CArenaCall( CALL_TYPE *pCall ) : m_pCall( pCall ) {}
		CArenaCall( CArenaCall &&src ) noexcept : m_pCall( std::exchange( src.m_pCall, nullptr ) ) {}
		~CArenaCall() { if ( m_pCall ) std::destroy_at( m_pCall ); }

		void operator()() { (*m_pCall)(); }

	private:
		CALL_TYPE *m_pCall;
	};

	struct Elem_t
	{
		Elem_t *pNext;
		CFunctor fn;
	};

	Elem_t *m_pHead;
	Elem_t *m_pTail;
};

}
