// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2006 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:functors;

import std;
import ks.math;
export import :refcount;
export import :strtools;
export import :envelope;

export extern "C++" {

//-----------------------------------------------------------------------------
//
// Purpose: Base class of all function objects
//
//-----------------------------------------------------------------------------
class CFunctor : public IRefCounted
{
public:
	CFunctor()
	{
	}
	virtual ~CFunctor() {}
	virtual void operator()() = 0;

	unsigned m_nUserID; // For debugging
};

//-----------------------------------------------------------------------------
//
// NOTE: Functor data + functor callback are tied together
// The basic idea is that someone creates the functor data. At a later point,
// the functor data is passed to a functor callback.
//
// The argument types are carried in the C++ type system, so a mismatch between
// the data and the callback is a compile-time error rather than a debug-build
// string comparison.
//
//-----------------------------------------------------------------------------
class CFunctorData : public IRefCounted
{
public:
	virtual ~CFunctorData() {}

	// Retained for source compatibility with callers that log functor contents.
	virtual void ComputeValidationString( char *pString, std::size_t nBufLen ) const
	{
		if ( pString && nBufLen )
			pString[0] = '\0';
	}
};

class CFunctorCallback : public IRefCounted
{
public:
	virtual ~CFunctorCallback() {}

	virtual bool IsEqual( CFunctorCallback *pSrc ) const = 0;
	virtual void operator()( CFunctorData *pData ) = 0;
	virtual const void *GetTarget() const = 0;

	virtual void ComputeValidationString( char *pString, std::size_t nBufLen ) const
	{
		if ( pString && nBufLen )
			pString[0] = '\0';
	}
	virtual const char *GetImplClassName() const { return typeid( *this ).name(); }
};

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
// Purpose: Classes to define memory management policies when operating
//			on pointers to members
//
//-----------------------------------------------------------------------------
class CFuncMemPolicyNone
{
public:
	static void OnAcquire(void *pObject)	{}
	static void OnRelease(void *pObject)	{}
};

template <class OBJECT_TYPE_PTR = IRefCounted *>
class CFuncMemPolicyRefCount
{
public:
	static void OnAcquire(OBJECT_TYPE_PTR pObject)	{ pObject->AddRef(); }
	static void OnRelease(OBJECT_TYPE_PTR pObject)	{ pObject->Release(); }
};

//-----------------------------------------------------------------------------
//
// Purpose: Function proxy is a generic facility for holding a function
//			pointer. Can be used on own, though primarily for use
//			by this file
//
//-----------------------------------------------------------------------------
template <class OBJECT_TYPE_PTR, typename FUNCTION_TYPE, class MEM_POLICY = CFuncMemPolicyNone >
class CMemberFuncProxyBase
{
public:
	bool operator==( const CMemberFuncProxyBase &src ) const
	{
		return m_pfnProxied == src.m_pfnProxied && m_pObject == src.m_pObject;
	}

	const void *GetTarget() const
	{
		return m_pObject;
	}

protected:
	CMemberFuncProxyBase( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied )
	  : m_pObject( pObject ),
		m_pfnProxied( pfnProxied )
	{
		MEM_POLICY::OnAcquire(m_pObject);
	}

	~CMemberFuncProxyBase()
	{
		MEM_POLICY::OnRelease(m_pObject);
	}

	void Set( OBJECT_TYPE_PTR pObject, FUNCTION_TYPE pfnProxied )
	{
		m_pfnProxied = pfnProxied;
		m_pObject = pObject;
	}

	void OnCall()
	{
		contract_assert( (void *)m_pObject != nullptr );
	}

	FUNCTION_TYPE m_pfnProxied;
	OBJECT_TYPE_PTR m_pObject;
};

// Variadic replacement for the generated CMemberFuncProxyN family.
template <class OBJECT_TYPE_PTR, typename FUNCTION_TYPE, class MEM_POLICY = CFuncMemPolicyNone>
class CMemberFuncProxy : public CMemberFuncProxyBase<OBJECT_TYPE_PTR, FUNCTION_TYPE, MEM_POLICY>
{
public:
	CMemberFuncProxy( OBJECT_TYPE_PTR pObject = nullptr, FUNCTION_TYPE pfnProxied = nullptr )
		: CMemberFuncProxyBase<OBJECT_TYPE_PTR, FUNCTION_TYPE, MEM_POLICY >( pObject, pfnProxied )
	{
	}

	template <typename... ARGS>
	void operator()( ARGS &&... args )
	{
		this->OnCall();
		((*this->m_pObject).*this->m_pfnProxied)( std::forward<ARGS>(args)... );
	}
};

//-----------------------------------------------------------------------------
//
// Purpose: Type traits describing a member function pointer
//
//-----------------------------------------------------------------------------
template <typename MEMBER_FN>
struct CMemberFnTraits;

template <typename CLASS_TYPE, typename RET_TYPE, typename... ARGS>
struct CMemberFnTraits< RET_TYPE (CLASS_TYPE::*)( ARGS... ) >
{
	typedef CLASS_TYPE ClassType;
	typedef RET_TYPE RetType;
	typedef std::tuple<ARGS...> ArgsTuple;
};

template <typename CLASS_TYPE, typename RET_TYPE, typename... ARGS>
struct CMemberFnTraits< RET_TYPE (CLASS_TYPE::*)( ARGS... ) const >
{
	typedef CLASS_TYPE ClassType;
	typedef RET_TYPE RetType;
	typedef std::tuple<ARGS...> ArgsTuple;
};

//-----------------------------------------------------------------------------
//
// The actual functor implementation
//
//-----------------------------------------------------------------------------
typedef CRefCounted1<CFunctor, CRefCountServiceMT> CFunctorBase;

// A callable that invokes a free function
template <typename FUNCTION_TYPE>
struct CFreeFunctionCallable
{
	FUNCTION_TYPE m_pfnProxied;

	template <typename... ARGS>
	auto operator()( ARGS &&... args ) const
	{
		return m_pfnProxied( std::forward<ARGS>(args)... );
	}
};

// A callable that invokes a member function on a possibly ref-counted object
template <typename OBJECT_TYPE_PTR, typename MEMBER_FN, class MEM_POLICY = CFuncMemPolicyNone>
class CMemberFunctionCallable
{
public:
	CMemberFunctionCallable( OBJECT_TYPE_PTR pObject, MEMBER_FN pfnProxied )
		: m_pObject( pObject ), m_pfnProxied( pfnProxied )
	{
		MEM_POLICY::OnAcquire( m_pObject );
	}

	CMemberFunctionCallable( const CMemberFunctionCallable &src )
		: m_pObject( src.m_pObject ), m_pfnProxied( src.m_pfnProxied )
	{
		MEM_POLICY::OnAcquire( m_pObject );
	}

	~CMemberFunctionCallable()
	{
		MEM_POLICY::OnRelease( m_pObject );
	}

	bool operator==( const CMemberFunctionCallable &src ) const
	{
		return m_pfnProxied == src.m_pfnProxied && m_pObject == src.m_pObject;
	}

	const void *GetTarget() const { return m_pObject; }

	template <typename... ARGS>
	auto operator()( ARGS &&... args ) const
	{
		contract_assert( (void *)m_pObject != nullptr );
		return ( m_pObject->*m_pfnProxied )( std::forward<ARGS>(args)... );
	}

private:
	OBJECT_TYPE_PTR m_pObject;
	MEMBER_FN m_pfnProxied;
};

// A functor that owns a callable and a tuple of bound argument values.
template <class FUNCTOR_BASE, typename CALLABLE_TYPE, typename... BOUND_ARGS>
class CBoundFunctor : public FUNCTOR_BASE
{
public:
	// Bound arguments are decay-copied into the functor (std::bind semantics): the call site's
	// objects may be gone by the time the functor runs. A reference here would dangle, so forbid it.
	static_assert( ( !std::is_reference_v<BOUND_ARGS> && ... ),
		"CBoundFunctor stores bound arguments by value; bind with std::decay_t<ARGS>..." );

	template <typename... ARGS>
	CBoundFunctor( CALLABLE_TYPE callable, ARGS &&... args )
		: m_callable( callable ), m_args( static_cast<BOUND_ARGS>( std::forward<ARGS>(args) )... )
	{
	}

	void operator()() override
	{
		std::apply( m_callable, m_args );
	}

private:
	CALLABLE_TYPE m_callable;
	std::tuple<BOUND_ARGS...> m_args;
};

//-----------------------------------------------------------------------------
//
// Purpose: Functor data holds the argument values bound at creation time.
//
//-----------------------------------------------------------------------------
typedef CRefCounted1<CFunctorData, CRefCountServiceMT> CFunctorDataBase;

template <typename... ARGS>
class CFunctorDataT : public CFunctorDataBase
{
public:
	explicit CFunctorDataT( ARGS... args )
		: m_args( std::move( args )... )
	{
	}

	std::tuple<ARGS...> m_args;
};

template <typename... ARGS>
inline CFunctorData *CreateFunctorData( ARGS &&... args )
{
	return new CFunctorDataT< std::decay_t<ARGS>... >( std::forward<ARGS>(args)... );
}

inline CFunctorData *CreateFunctorData()
{
	return new CFunctorDataT<>();
}

//-----------------------------------------------------------------------------
//
// Purpose: Functor callbacks invoke a function with the arguments held by
//			the functor data.
//
//-----------------------------------------------------------------------------
class CFunctorCallbackBase : public CRefCounted1<CFunctorCallback, CRefCountServiceMT>
{
protected:
	template <typename EXPECTED_DATA>
	static EXPECTED_DATA *ValidateFunctorData( CFunctorData *pData )
	{
		return static_cast< EXPECTED_DATA * >( pData );
	}
};

// A callback that calls a free function
template <typename... ARGS>
class CFunctionFunctorCallback : public CFunctorCallbackBase
{
	typedef void (*Callback_t)( ARGS... );

public:
	explicit CFunctionFunctorCallback( Callback_t pfnProxied )
		: m_pfnProxied( pfnProxied )
	{
	}

	void operator()( CFunctorData *pFunctorDataBase ) override
{
		auto *pFunctorData = ValidateFunctorData< CFunctorDataT<ARGS...> >( pFunctorDataBase );
		std::apply( [this]( auto &&... args ) { m_pfnProxied( std::forward<decltype(args)>(args)... ); }, pFunctorData->m_args );
	}

	bool IsEqual( CFunctorCallback *pSrc ) const override
	{
		auto *pOther = dynamic_cast< CFunctionFunctorCallback * >( pSrc );
		return pOther && pOther->m_pfnProxied == m_pfnProxied;
	}

	const void *GetTarget() const override { return (const void *)m_pfnProxied; }

private:
	Callback_t m_pfnProxied;
};

// A callback that calls a member function. The argument types are passed
// explicitly so that the matching functor data type can be named.
template <typename FUNCTION_CLASS, typename MEMBER_FN, typename... ARGS>
class CMemberFunctorCallback : public CFunctorCallbackBase
{
public:
	CMemberFunctorCallback( FUNCTION_CLASS *pObject, MEMBER_FN pfnProxied )
		: m_Proxy( pObject, pfnProxied )
	{
	}

	void operator()( CFunctorData *pFunctorDataBase ) override
	{
		auto *pFunctorData = ValidateFunctorData< CFunctorDataT<ARGS...> >( pFunctorDataBase );
		std::apply( [this]( auto &&... args ) { m_Proxy( std::forward<decltype(args)>(args)... ); }, pFunctorData->m_args );
	}

	bool IsEqual( CFunctorCallback *pSrc ) const override
	{
		auto *pOther = dynamic_cast< CMemberFunctorCallback * >( pSrc );
		return pOther && pOther->m_Proxy == m_Proxy;
	}

	const void *GetTarget() const override { return m_Proxy.GetTarget(); }

private:
	CMemberFuncProxy< FUNCTION_CLASS *, MEMBER_FN > m_Proxy;
};

// Expands the member function's argument tuple into the callback's parameters.
template <typename FUNCTION_CLASS, typename MEMBER_FN, typename ARGS_TUPLE>
struct CMemberFunctorCallbackMaker;

template <typename FUNCTION_CLASS, typename MEMBER_FN, typename... ARGS>
struct CMemberFunctorCallbackMaker< FUNCTION_CLASS, MEMBER_FN, std::tuple<ARGS...> >
{
	static CFunctorCallback *Make( FUNCTION_CLASS *pObject, MEMBER_FN pfnProxied )
	{
		return new CMemberFunctorCallback< FUNCTION_CLASS, MEMBER_FN, ARGS... >( pObject, pfnProxied );
	}
};

//-----------------------------------------------------------------------------
// Creating functor data / callbacks
//-----------------------------------------------------------------------------
template <typename... ARGS>
inline CFunctorCallback *CreateFunctorCallback( void (*pfnProxied)( ARGS... ) )
{
	return new CFunctionFunctorCallback<ARGS...>( pfnProxied );
}

template <typename FUNCTION_CLASS, typename MEMBER_FN>
inline CFunctorCallback *CreateFunctorCallback( FUNCTION_CLASS *pObject, MEMBER_FN pfnProxied )
{
	return CMemberFunctorCallbackMaker< FUNCTION_CLASS, MEMBER_FN,
		typename CMemberFnTraits<MEMBER_FN>::ArgsTuple >::Make( pObject, pfnProxied );
}

//-----------------------------------------------------------------------------
//
// Templates to assist early-out direct call code
//
//-----------------------------------------------------------------------------
template <typename RET_TYPE, typename... ARGS>
inline void FunctorDirectCall( RET_TYPE (*pfnProxied)( ARGS... ), ARGS... args )
{
	(*pfnProxied)( std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline void FunctorDirectCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ), ARGS... args )
{
	((*pObject).*pfnProxied)( std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline void FunctorDirectCall( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ) const, ARGS... args )
{
	((*pObject).*pfnProxied)( std::forward<ARGS>(args)... );
}

//-----------------------------------------------------------------------------
//
// The factory: creates functors that bind a function and its arguments.
//
//-----------------------------------------------------------------------------
template <class DERIVED_TYPE>
class CFunctorFactoryBase
{
public:
	// Free function
	template <typename RET_TYPE, typename... PARAMS, typename... ARGS>
	CFunctor *CreateFunctor( RET_TYPE (*pfnProxied)( PARAMS... ), ARGS &&... args )
	{
		typedef CFreeFunctionCallable< RET_TYPE (*)( PARAMS... ) > Callable_t;
		typedef CBoundFunctor< typename DERIVED_TYPE::FunctorBase, Callable_t, std::decay_t<ARGS>... > Functor_t;
		return static_cast<DERIVED_TYPE *>(this)->template Allocate<Functor_t>( Callable_t{ pfnProxied }, std::forward<ARGS>(args)... );
	}

	// Member function
	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CFunctor *CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
	{
		typedef RET_TYPE ( FUNCTION_CLASS::*MemberFn_t )( PARAMS... );
		typedef CMemberFunctionCallable< OBJECT_TYPE_PTR, MemberFn_t, CFuncMemPolicyNone > Callable_t;
		typedef CBoundFunctor< typename DERIVED_TYPE::FunctorBase, Callable_t, std::decay_t<ARGS>... > Functor_t;
		return static_cast<DERIVED_TYPE *>(this)->template Allocate<Functor_t>( Callable_t{ pObject, pfnProxied }, std::forward<ARGS>(args)... );
	}

	// Const member function
	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CFunctor *CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
	{
		typedef RET_TYPE ( FUNCTION_CLASS::*MemberFn_t )( PARAMS... ) const;
		typedef CMemberFunctionCallable< OBJECT_TYPE_PTR, MemberFn_t, CFuncMemPolicyNone > Callable_t;
		typedef CBoundFunctor< typename DERIVED_TYPE::FunctorBase, Callable_t, std::decay_t<ARGS>... > Functor_t;
		return static_cast<DERIVED_TYPE *>(this)->template Allocate<Functor_t>( Callable_t{ pObject, pfnProxied }, std::forward<ARGS>(args)... );
	}

	// Ref-counting member function
	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
	{
		typedef RET_TYPE ( FUNCTION_CLASS::*MemberFn_t )( PARAMS... );
		typedef CMemberFunctionCallable< OBJECT_TYPE_PTR, MemberFn_t, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> > Callable_t;
		typedef CBoundFunctor< typename DERIVED_TYPE::FunctorBase, Callable_t, std::decay_t<ARGS>... > Functor_t;
		return static_cast<DERIVED_TYPE *>(this)->template Allocate<Functor_t>( Callable_t{ pObject, pfnProxied }, std::forward<ARGS>(args)... );
	}

	// Ref-counting const member function
	template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
	CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
	{
		typedef RET_TYPE ( FUNCTION_CLASS::*MemberFn_t )( PARAMS... ) const;
		typedef CMemberFunctionCallable< OBJECT_TYPE_PTR, MemberFn_t, CFuncMemPolicyRefCount<OBJECT_TYPE_PTR> > Callable_t;
		typedef CBoundFunctor< typename DERIVED_TYPE::FunctorBase, Callable_t, std::decay_t<ARGS>... > Functor_t;
		return static_cast<DERIVED_TYPE *>(this)->template Allocate<Functor_t>( Callable_t{ pObject, pfnProxied }, std::forward<ARGS>(args)... );
	}
};

class CDefaultFunctorFactory : public CFunctorFactoryBase<CDefaultFunctorFactory>
{
public:
	typedef CFunctorBase FunctorBase;

	template <typename FUNCTOR_TYPE, typename... ARGS>
	FUNCTOR_TYPE *Allocate( ARGS &&... args )
	{
		return new FUNCTOR_TYPE( std::forward<ARGS>(args)... );
	}
};

template <class CAllocator, class CCustomFunctorBase = CFunctorBase>
class CCustomizedFunctorFactory : public CFunctorFactoryBase< CCustomizedFunctorFactory<CAllocator, CCustomFunctorBase> >
{
public:
	typedef CCustomFunctorBase FunctorBase;

	void SetAllocator( CAllocator *pAllocator )
	{
		m_pAllocator = pAllocator;
	}

	template <typename FUNCTOR_TYPE, typename... ARGS>
	FUNCTOR_TYPE *Allocate( ARGS &&... args )
	{
		void *pMemory = m_pAllocator ? m_pAllocator->Alloc( sizeof(FUNCTOR_TYPE) ) : ::operator new( sizeof(FUNCTOR_TYPE) );
		return new ( pMemory ) FUNCTOR_TYPE( std::forward<ARGS>(args)... );
	}

private:
	CAllocator *m_pAllocator = nullptr;
};

//-----------------------------------------------------------------------------
//
// Free-function entry points, equivalent to using a default functor factory.
//
//-----------------------------------------------------------------------------
inline CDefaultFunctorFactory &GetDefaultFunctorFactory()
{
	static CDefaultFunctorFactory s_DefaultFunctorFactory;
	return s_DefaultFunctorFactory;
}

template <typename RET_TYPE, typename... PARAMS, typename... ARGS>
inline CFunctor *CreateFunctor( RET_TYPE (*pfnProxied)( PARAMS... ), ARGS &&... args )
{
	return GetDefaultFunctorFactory().CreateFunctor( pfnProxied, std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline CFunctor *CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
{
	return GetDefaultFunctorFactory().CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline CFunctor *CreateFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
{
	return GetDefaultFunctorFactory().CreateFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ), ARGS &&... args )
{
	return GetDefaultFunctorFactory().CreateRefCountingFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... PARAMS, typename... ARGS>
inline CFunctor *CreateRefCountingFunctor( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( PARAMS... ) const, ARGS &&... args )
{
	return GetDefaultFunctorFactory().CreateRefCountingFunctor( pObject, pfnProxied, std::forward<ARGS>(args)... );
}

}
