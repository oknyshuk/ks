// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:refcount;

import std;
import ks.math;

export extern "C++" {

//-----------------------------------------------------------------------------
// Purpose:	Release a pointer and mark it NULL
//-----------------------------------------------------------------------------

template <class REFCOUNTED_ITEM_PTR>
inline int SafeRelease( REFCOUNTED_ITEM_PTR &pRef )
{
	// Use funny syntax so that this works on "auto pointers"
	REFCOUNTED_ITEM_PTR *ppRef = &pRef;
	if ( *ppRef )
	{
		int result = (*ppRef)->Release();
		*ppRef = nullptr;
		return result;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Do a an inline AddRef then return the pointer, useful when
//			returning an object from a function
//-----------------------------------------------------------------------------

template <typename T1> [[gnu::always_inline]] inline auto RetAddRef( T1 p ) { return ( (p)->AddRef(), (p) ); }
template <typename T1> [[gnu::always_inline]] inline auto InlineAddRef( T1 p ) { return ( (p)->AddRef(), (p) ); }

//-----------------------------------------------------------------------------
// Purpose:	Traits classes defining reference count threading model
//-----------------------------------------------------------------------------

class CRefMT
{
public:
	static int Increment( int *p) { return ++std::atomic_ref<int>( *p ); }
	static int Decrement( int *p) { return --std::atomic_ref<int>( *p ); }
};

class CRefST
{
public:
	static int Increment( int *p) { return ++(*p); }
	static int Decrement( int *p) { return --(*p); }
};

//-----------------------------------------------------------------------------
// Purpose:	Intrusive reference count (CRTP, no virtual functions). Derive from
//			CRefCounted<Derived>; the final Release() destroys the object with
//			`delete static_cast<Derived *>( this )`, so Derived must be the
//			dynamic type of every object created (a class hierarchy below
//			Derived needs its own destruction path, e.g. a virtual destructor
//			in Derived). Derived may provide a public `bool OnFinalRelease()`
//			that runs before deletion; returning false keeps the object alive
//			(it was recycled, e.g. returned to a pool).
//-----------------------------------------------------------------------------

template < class Derived, typename CRefThreading = CRefMT >
class CRefCounted
{
public:
	int AddRef()
	{
		return CRefThreading::Increment( &m_iRefs );
	}

	int Release()
	{
		int result = CRefThreading::Decrement( &m_iRefs );
		if ( result )
			return result;
		Derived *pDerived = static_cast< Derived * >( this );
		if constexpr ( requires { pDerived->OnFinalRelease(); } )
		{
			if ( !pDerived->OnFinalRelease() )
				return 0;
		}
		delete pDerived;
		return 0;
	}

protected:
	CRefCounted()
	  : m_iRefs( 1 )
	{
	}

	int GetRefCount() const
	{
		return m_iRefs;
	}

private:
	int m_iRefs;
};

}
