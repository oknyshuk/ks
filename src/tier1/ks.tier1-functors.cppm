// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2006 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.functors;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
export import ks.tier1.refcount;
export import ks.tier1.strtools;
export import ks.tier1.envelope;

export extern "C++" {

//-----------------------------------------------------------------------------
// An owned, type-erased call. Created by CreateFunctor() below (or any lambda).
//-----------------------------------------------------------------------------
using CFunctor = std::move_only_function<void()>;

//-----------------------------------------------------------------------------
// When calling through a functor, care needs to be taken to not pass objects that might go away.
// Since this code determines the type to store in the functor based on the actual arguments,
// this is achieved by changing the point of call.
//
// See also ks::envelope
//-----------------------------------------------------------------------------
// convert a reference to a passable value
template <typename T>
inline T RefToVal(const T &item)
{
   return item;
}

//-----------------------------------------------------------------------------
// This class can be used to pass into a functor a proxy object for a pointer
// to be resolved later. For example, you can execute a "call" on a resource
// whose actual value is not known until a later time
//-----------------------------------------------------------------------------
template <typename T>
class CLateBoundPtr
{
public:
	CLateBoundPtr( T **ppObject )
		: m_ppObject( ppObject )
	{
	}

	T *operator->() { return *m_ppObject;	}
	T &operator *() { return **m_ppObject;	}
	operator T *() const { return (T*)(*m_ppObject);	}
	operator void *() { return *m_ppObject;	}

private:
	T **m_ppObject;
};

//-----------------------------------------------------------------------------
//
// The factory: creates functors that bind a function and its arguments.
//
// Bound arguments are decay-copied into the functor at creation time (std::bind
// semantics): the call site's objects may be gone by the time the functor runs.
// Each is passed to the target as an lvalue.
//
// The result is a lambda; convert it to a CFunctor to store it type-erased.
//
//-----------------------------------------------------------------------------

// Free function
template <typename RET_TYPE, typename... PARAMS, typename... ARGS>
inline auto CreateFunctor( RET_TYPE (*pfnProxied)( PARAMS... ), ARGS &&... args )
{
	return [pfnProxied, ...boundArgs = std::forward<ARGS>( args )]() mutable -> void
	{
		pfnProxied( boundArgs... );
	};
}

// Member function
template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline auto CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
{
	return [pObject, pfnProxied, ...boundArgs = std::forward<ARGS>( args )]() mutable -> void
	{
		contract_assert( (void *)pObject != nullptr );
		( (*pObject).*pfnProxied )( boundArgs... );
	};
}

// Const member function
template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline auto CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
{
	return [pObject, pfnProxied, ...boundArgs = std::forward<ARGS>( args )]() mutable -> void
	{
		contract_assert( (void *)pObject != nullptr );
		( (*pObject).*pfnProxied )( boundArgs... );
	};
}

}
