// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.tracked_ref;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;
export import ks.tier1.intrusive_list;

export extern "C++" {

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

}
