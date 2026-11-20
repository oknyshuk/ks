// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.intrusive_list;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	template < class T >
	class intrusive_dlist_with_tail;

	template < class T >
	class intrusive_dlist
	{
	public:
		class iterator
		{
		public:
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			iterator() = default;
			explicit iterator( T *p ) : m_p( p ) {}
			T &operator*() const					{ return *m_p; }
			T *operator->() const					{ return m_p; }
			iterator &operator++()					{ m_p = m_p->m_pNext; return *this; }
			iterator operator++( int )				{ auto t = *this; m_p = m_p->m_pNext; return t; }
			bool operator==( const iterator & ) const = default;
		private:
			T *m_p = nullptr;
		};

		T *head() const							{ return m_pHead; }
		bool empty() const						{ return m_pHead == nullptr; }
		iterator begin() const					{ return iterator( m_pHead ); }
		iterator end() const					{ return iterator(); }

		// O(n).
		std::size_t size() const
		{
			std::size_t n = 0;
			for ( T *p = m_pHead; p; p = p->m_pNext )
				++n;
			return n;
		}

		void push_front( T *node )
		{
			node->m_pNext = m_pHead;
			node->m_pPrev = nullptr;
			if ( m_pHead )
				m_pHead->m_pPrev = node;
			m_pHead = node;
		}

		// O(n): there is no tail pointer.
		void push_back( T *node )
		{
			node->m_pNext = nullptr;
			if ( !m_pHead )
			{
				node->m_pPrev = nullptr;
				m_pHead = node;
				return;
			}
			T *p = m_pHead;
			while ( p->m_pNext )
				p = p->m_pNext;
			p->m_pNext = node;
			node->m_pPrev = p;
		}

		// Unlinks node and clears its links. A node without a predecessor that is not the head is not on this
		// list; it is left unlinked.
		void erase( T *node )
		{
			if ( node->m_pPrev )
			{
				contract_assert( node->m_pPrev->m_pNext == node );
				node->m_pPrev->m_pNext = node->m_pNext;
				if ( node->m_pNext )
				{
					contract_assert( node->m_pNext->m_pPrev == node );
					node->m_pNext->m_pPrev = node->m_pPrev;
				}
			}
			else if ( m_pHead == node )
			{
				m_pHead = node->m_pNext;
				if ( m_pHead )
					m_pHead->m_pPrev = nullptr;
			}
			node->m_pNext = node->m_pPrev = nullptr;
		}

		// Unlinks and returns the head; its own links are left as they were.
		T *pop_front()
		{
			T *node = m_pHead;
			if ( node )
			{
				m_pHead = node->m_pNext;
				if ( m_pHead )
					m_pHead->m_pPrev = nullptr;
			}
			return node;
		}

		// Forgets every node without touching them.
		void clear()							{ m_pHead = nullptr; }

		// Deletes every node, head first; a destructor may unlink its own node.
		void delete_all()
		{
			while ( m_pHead )
			{
				T *next = m_pHead->m_pNext;
				delete m_pHead;
				m_pHead = next;
			}
		}

		// Moves all of src in front of this list's nodes and leaves src empty.
		void splice_front( intrusive_dlist_with_tail< T > &src );

	protected:
		T *m_pHead = nullptr;
	};

	// Collects nodes at O(1) for a later intrusive_dlist::splice_front; tracks its tail for that.
	template < class T >
	class intrusive_dlist_with_tail : private intrusive_dlist< T >
	{
		using base = intrusive_dlist< T >;
		friend class intrusive_dlist< T >;
	public:
		using base::head;
		using base::empty;
		using base::begin;
		using base::end;
		using base::size;
		T *tail() const							{ return m_pTail; }

		void push_front( T *node )
		{
			if ( !this->m_pHead )
				m_pTail = node;
			base::push_front( node );
		}

		// Forgets every node without touching them.
		void clear()							{ this->m_pHead = m_pTail = nullptr; }

	private:
		T *m_pTail = nullptr;
	};

	template < class T >
	void intrusive_dlist< T >::splice_front( intrusive_dlist_with_tail< T > &src )
	{
		if ( !src.m_pHead )
			return;
		src.m_pTail->m_pNext = m_pHead;
		if ( m_pHead )
			m_pHead->m_pPrev = src.m_pTail;
		m_pHead = src.m_pHead;
		src.m_pHead = src.m_pTail = nullptr;
	}
}

}
