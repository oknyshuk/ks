// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:refcount;

import std;
import ks.math;

export extern "C++" {

//-----------------------------------------------------------------------------
// Purpose:	Implement a standard reference counted interface. Use of this
//			is optional insofar as all the concrete tools only require
//			at compile time that the function signatures match.
//-----------------------------------------------------------------------------

class IRefCounted
{
public:
	virtual int AddRef() = 0;
	virtual int Release() = 0;
};

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
// Purpose:	Actual reference counting implementation. Pulled out to reduce
//			code bloat.
//-----------------------------------------------------------------------------

template <const bool bSelfDelete, typename CRefThreading = CRefMT>
class CRefCountServiceBase
{
protected:
	CRefCountServiceBase()
	  : m_iRefs( 1 )
	{
	}

	virtual ~CRefCountServiceBase()
	{
	}

	virtual bool OnFinalRelease()
	{
		return true;
	}

	int GetRefCount() const
	{
		return m_iRefs;
	}

	int DoAddRef()
	{
		return CRefThreading::Increment( &m_iRefs );
	}

	int DoRelease()
	{
		int result = CRefThreading::Decrement( &m_iRefs );
		if ( result )
			return result;
		if ( OnFinalRelease() && bSelfDelete )
			delete this;
		return 0;
	}

private:
	int m_iRefs;
};

class CRefCountServiceNull
{
protected:
	static int DoAddRef() { return 1; }
	static int DoRelease() { return 1; }
};

template <typename CRefThreading = CRefMT>
class CRefCountServiceDestruct
{
protected:
	CRefCountServiceDestruct()
		: m_iRefs( 1 )
	{
	}

	virtual ~CRefCountServiceDestruct()
	{
	}

	int GetRefCount() const
	{
		return m_iRefs;
	}

	int DoAddRef()
	{
		return CRefThreading::Increment( &m_iRefs );
	}

	int DoRelease()
	{
		int result = CRefThreading::Decrement( &m_iRefs );
		if ( result )
			return result;
		this->~CRefCountServiceDestruct();
		return 0;
	}

private:
	int m_iRefs;
};


typedef CRefCountServiceBase<true, CRefMT>	CRefCountServiceMT;
typedef CRefCountServiceBase<false, CRefMT> CRefCountServiceNoDeleteMT;

// Default to threadsafe
typedef CRefCountServiceNoDeleteMT			CRefCountServiceNoDelete;
typedef CRefCountServiceMT					CRefCountService;

//-----------------------------------------------------------------------------
// Purpose:	Base classes to implement reference counting
//-----------------------------------------------------------------------------

template < class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted : public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted1 : public BASE1,
							   public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted1()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted2 : public BASE1, public BASE2,
							   public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted2()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted3 : public BASE1, public BASE2, public BASE3,
							   public REFCOUNT_SERVICE
{
	virtual ~CRefCounted3()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class BASE4, class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted4 : public BASE1, public BASE2, public BASE3, public BASE4,
							   public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted4()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class BASE4, class BASE5, class REFCOUNT_SERVICE = CRefCountService > 
class CRefCounted5 : public BASE1, public BASE2, public BASE3, public BASE4, public BASE5,
							   public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted5()	{}
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-----------------------------------------------------------------------------
// Purpose:	Class to throw around a reference counted item to debug
//			referencing problems
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

}
