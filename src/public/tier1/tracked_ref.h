// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Auto-nulling references: a tracked_ref<T> points at a T and goes null when that T is destroyed, without
// shared ownership. The referenced class holds a tracked_ref_list<T> (KS_TRACKED_CLASS declares one) and every
// tracked_ref to it is linked into that list, so destroying the object nulls them all. A tracked_ref may be
// freely copied and reassigned; the object itself need not know who refers to it. Single-threaded.
//
// Using it: add KS_TRACKED_CLASS( Foo ) to class Foo, hold ks::tracked_ref<Foo> where a Foo* could dangle, and
// test with IsValid() or against nullptr.

#ifndef TRACKED_REF_H
#define TRACKED_REF_H

#include "tier1/intrusive_list.h"

#include <cstddef>

namespace ks
{
	template < class T >
	class tracked_ref;

	// Lives inside the referenced object. On destruction it nulls every reference still linked to it.
	template < class T >
	class tracked_ref_list : public intrusive_dlist< tracked_ref< T > >
	{
	public:
		tracked_ref_list() = default;
		tracked_ref_list( const tracked_ref_list & ) = delete;
		tracked_ref_list &operator=( const tracked_ref_list & ) = delete;

		~tracked_ref_list()
		{
			tracked_ref< T > *p = this->m_pHead;
			while ( p )
			{
				tracked_ref< T > *next = p->m_pNext;
				p->m_pNext = nullptr;
				p->m_pPrev = nullptr;
				p->m_pObject = nullptr;
				p = next;
			}
			this->m_pHead = nullptr;
		}
	};

	template < class T >
	class tracked_ref
	{
	public:
		tracked_ref() = default;
		tracked_ref( T *pObj )						{ attach( pObj ); }
		tracked_ref( const tracked_ref &other )		{ attach( other.m_pObject ); }
		~tracked_ref()								{ detach(); }

		void Set( T *pObj )
		{
			if ( m_pObject != pObj )
			{
				detach();
				attach( pObj );
			}
		}

		tracked_ref &operator=( const tracked_ref &other )	{ Set( other.m_pObject ); return *this; }
		tracked_ref &operator=( T *pObj )					{ Set( pObj ); return *this; }

		T *operator()() const					{ return m_pObject; }
		bool IsValid() const					{ return m_pObject != nullptr; }
		operator T *()							{ return m_pObject; }
		operator const T *() const				{ return m_pObject; }
		T *GetObject()							{ return m_pObject; }
		const T *GetObject() const				{ return m_pObject; }
		T *operator->()							{ return m_pObject; }
		const T *operator->() const				{ return m_pObject; }

		bool operator==( std::nullptr_t ) const				{ return m_pObject == nullptr; }
		bool operator==( const T *pOther ) const			{ return m_pObject == pOther; }
		bool operator==( const tracked_ref &o ) const		{ return m_pObject == o.m_pObject; }

		// Intrusive list links; the referenced object's tracked_ref_list threads through these.
		tracked_ref *m_pNext = nullptr;
		tracked_ref *m_pPrev = nullptr;
		T *m_pObject = nullptr;

	private:
		void attach( T *pObj )
		{
			m_pObject = pObj;
			if ( pObj )
				pObj->m_References.push_front( this );
		}

		void detach()
		{
			if ( m_pObject )
			{
				m_pObject->m_References.erase( this );
				m_pObject = nullptr;
			}
		}
	};
}

// Put this in classes that are referenced by ks::tracked_ref.
#define KS_TRACKED_CLASS( _className )						\
	private:												\
		ks::tracked_ref_list< _className > m_References;	\
		template < class T > friend class ks::tracked_ref;

#endif // TRACKED_REF_H
