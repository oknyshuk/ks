// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Many small doubly linked chains sharing one ks::slot_pool, for pools whose owners each keep the head handle of
// their own chain. Handles stay valid until freed, freed slots are reused most-recent first, elements never move.

#ifndef CHAIN_POOL_H
#define CHAIN_POOL_H

#include "tier1/slot_pool.h"

#include <utility>

namespace ks
{
	template < class T, std::integral I >
	class chain_pool
	{
		struct Node
		{
			template < class... A >
			explicit Node( A &&...args ) : value( std::forward< A >( args )... ) {}
			T value;
			I prev = I( ~I( 0 ) );
			I next = I( ~I( 0 ) );
		};

	public:
		using handle_type = I;
		static constexpr I invalid = slot_pool< Node, I >::invalid;

		// A new element that is not in any chain yet.
		template < class... A >
		I alloc( A &&...args )		{ return m_Pool.emplace( std::forward< A >( args )... ); }

		// Links h, which must be unlinked, in front of head and makes it the new head.
		void push_front( I &head, I h )
		{
			Node &n = m_Pool[h];
			n.prev = invalid;
			n.next = head;
			if ( head != invalid )
				m_Pool[head].prev = h;
			head = h;
		}

		// Links h, which must be unlinked, right after 'after'.
		void insert_after( I after, I h )
		{
			Node &a = m_Pool[after];
			Node &n = m_Pool[h];
			n.prev = after;
			n.next = a.next;
			if ( a.next != invalid )
				m_Pool[a.next].prev = h;
			a.next = h;
		}

		// Takes h out of the chain that starts at head.
		void unlink( I &head, I h )
		{
			Node &n = m_Pool[h];
			if ( n.prev != invalid )
				m_Pool[n.prev].next = n.next;
			else if ( head == h )
				head = n.next;
			if ( n.next != invalid )
				m_Pool[n.next].prev = n.prev;
			n.prev = n.next = invalid;
		}

		// Unlinks h from the chain that starts at head and frees it.
		void free( I &head, I h )
		{
			unlink( head, h );
			m_Pool.erase( h );
		}

		// Frees every element of the chain that starts at head and empties it.
		void free_chain( I &head )
		{
			for ( I h = head; h != invalid; )
			{
				I next = m_Pool[h].next;
				m_Pool.erase( h );
				h = next;
			}
			head = invalid;
		}

		I next( I h ) const					{ return m_Pool[h].next; }
		I prev( I h ) const					{ return m_Pool[h].prev; }
		T &operator[]( I h )				{ return m_Pool[h].value; }
		const T &operator[]( I h ) const	{ return m_Pool[h].value; }
		bool contains( I h ) const			{ return m_Pool.contains( h ); }
		size_t size() const					{ return m_Pool.size(); }
		bool empty() const					{ return m_Pool.empty(); }
		size_t slot_count() const			{ return m_Pool.slot_count(); }
		bool full() const					{ return m_Pool.size() >= size_t( invalid ); }	// every handle below invalid is live
		void clear()						{ m_Pool.clear(); }

	private:
		slot_pool< Node, I > m_Pool;
	};
}

#endif // CHAIN_POOL_H
