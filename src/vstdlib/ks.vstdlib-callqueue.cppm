// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2006 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib:callqueue;

import std;
import ks.tier1;
export import :jobthread;

export extern "C++" {

//-----------------------------------------------------
// Avert thy eyes! Imagine rather:
//
// void QueueCall( <function>, [args1, [arg2,]...]
// void QueueCall( <object>, <function>, [args1, [arg2,]...]
// void QueueRefCall( <object>, <<function>, [args1, [arg2,]...]
//
// DERIVED_TYPE must provide "void QueueFunctorInternal( CFunctor *pFunctor )".
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

	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	void QueueRefCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
	{
		static_cast<DERIVED_TYPE *>( this )->QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}

	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	void QueueRefCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
	{
		static_cast<DERIVED_TYPE *>( this )->QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... ) );
	}
};

//-----------------------------------------------------

template <typename QUEUE_TYPE = CTSQueue<CFunctor *> >
class CCallQueueT : public CCallQueueMethods< CCallQueueT<QUEUE_TYPE> >
{
public:
	friend class CCallQueueMethods< CCallQueueT<QUEUE_TYPE> >;
	CCallQueueT()
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
			pFunctor->Release();
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
			ParallelProcess( pPool, pDummy, nNumThreads, this, &CCallQueueT<>::ExecuteWrapper );
		}
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		contract_assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
			pFunctor->Release();
		}
	}

protected:
	void ExecuteWrapper( int &nDummy )						// to match paralell process function template
	{
		CallQueued();
	}

	void QueueFunctorInternal( CFunctor *pFunctor )
	{
		if ( !m_bNoQueue )
		{
			m_queue.PushItem( pFunctor );
		}
		else
		{
			(*pFunctor)();
			pFunctor->Release();
		}
	}

private:
	QUEUE_TYPE m_queue;
	bool m_bNoQueue;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};

class CCallQueue : public CCallQueueT<>
{
};

//-----------------------------------------------------
// Optional interface that can be bound to concrete CCallQueue
//-----------------------------------------------------

class ICallQueue : public CCallQueueMethods<ICallQueue>
{
public:
	friend class CCallQueueMethods<ICallQueue>;
	void QueueFunctor( CFunctor *pFunctor )
	{
		// [mhansen] If we grab an extra reference here then this functor never gets released.
		// That usually isn't too bad because the memory the functor is allocated in is cleared and
		// reused every frame.  But, if the functor contains a CUtlDataEnvelope with more than
		// 4 bytes of data it allocates its own memory and, in that case, we need the destructor to
		// be called to free it.  So, we don't want to grab the "extra" reference here.
		// The net result should be that after this call the pFunctor has a ref count of 1 (which
		// is held by the functor it gets nested in, which is stuck in the call queue) and after it
		// is executed the owning functor is destructed which causes it to release the reference
		// and this functor is then freed.  This happens in imatersysteminternal.h:112 where the
		// destructor is called explictly for the owning functor: pFunctor->~CFunctor();
		//QueueFunctorInternal( RetAddRef( pFunctor ) );
		QueueFunctorInternal( pFunctor );
	}

protected:
	virtual void QueueFunctorInternal( CFunctor *pFunctor ) = 0;
};

}
