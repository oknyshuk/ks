// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:slot_pool;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	template < class T, std::integral I = int >
	class slot_pool
	{
	public:
		using handle_type = I;
		static constexpr I invalid = I( ~I( 0 ) );

		template < class... A >
		I emplace( A &&...args )
		{
			I h;
			if ( !m_Free.empty() )
			{
				h = m_Free.back();
				m_Free.pop_back();
				m_Slots[h].emplace( std::forward< A >( args )... );
			}
			else
			{
				h = I( m_Slots.size() );
				m_Slots.emplace_back( std::in_place, std::forward< A >( args )... );
			}
			++m_nLive;
			return h;
		}

		void erase( I h )
		{
			m_Slots[h].reset();
			m_Free.push_back( h );
			--m_nLive;
		}

		bool contains( I h ) const	{ return h != invalid && std::size_t( h ) < m_Slots.size() && m_Slots[h].has_value(); }
		T &operator[]( I h )		{ return *m_Slots[h]; }
		const T &operator[]( I h ) const { return *m_Slots[h]; }

		std::size_t size() const			{ return m_nLive; }
		bool empty() const			{ return m_nLive == 0; }
		std::size_t slot_count() const	{ return m_Slots.size(); }	// one past the highest handle ever issued
		void clear()				{ m_Slots.clear(); m_Free.clear(); m_nLive = 0; }

		// Handle of an element obtained from this pool, or invalid.
		I handle_of( const T *p ) const
		{
			for ( std::size_t i = 0; i < m_Slots.size(); ++i )
				if ( m_Slots[i] && &*m_Slots[i] == p )
					return I( i );
			return invalid;
		}

		template < bool Const >
		class basic_iterator
		{
			using Slots = std::conditional_t< Const, const std::deque< std::optional< T > >, std::deque< std::optional< T > > >;
		public:
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			basic_iterator() = default;
			basic_iterator( Slots *s, std::size_t i ) : m_s( s ), m_i( i ) { skip(); }
			auto &operator*() const					{ return *( *m_s )[m_i]; }
			auto *operator->() const				{ return &*( *m_s )[m_i]; }
			I handle() const						{ return I( m_i ); }
			basic_iterator &operator++()			{ ++m_i; skip(); return *this; }
			basic_iterator operator++( int )		{ auto t = *this; ++*this; return t; }
			bool operator==( const basic_iterator &o ) const { return m_i == o.m_i; }
		private:
			void skip()								{ while ( m_i < m_s->size() && !( *m_s )[m_i] ) ++m_i; }
			Slots *m_s = nullptr;
			std::size_t m_i = 0;
		};
		using iterator = basic_iterator< false >;
		using const_iterator = basic_iterator< true >;

		iterator begin()				{ return { &m_Slots, 0 }; }
		iterator end()					{ return { &m_Slots, m_Slots.size() }; }
		const_iterator begin() const	{ return { &m_Slots, 0 }; }
		const_iterator end() const		{ return { &m_Slots, m_Slots.size() }; }

	private:
		std::deque< std::optional< T > > m_Slots;
		std::vector< I > m_Free;
		std::size_t m_nLive = 0;
	};

	// slot_pool that also keeps an order (insertion by default): O(1) insert/erase anywhere,
	// iteration in list order. Advance before erasing the element an iterator points at.
	template < class T, std::integral I = int >
	class linked_slot_pool
	{
	public:
		using handle_type = I;
		static constexpr I invalid = I( ~I( 0 ) );

		template < class... A > I emplace_back( A &&...args )	{ return insert_before( invalid, std::forward< A >( args )... ); }
		template < class... A > I emplace_front( A &&...args )	{ return insert_before( m_Head, std::forward< A >( args )... ); }

		// Links a new element before pos (invalid = at the end).
		template < class... A >
		I insert_before( I pos, A &&...args )
		{
			I h;
			if ( !m_Free.empty() )
			{
				h = m_Free.back();
				m_Free.pop_back();
				m_Nodes[h].v.emplace( std::forward< A >( args )... );
			}
			else
			{
				h = I( m_Nodes.size() );
				m_Nodes.emplace_back().v.emplace( std::forward< A >( args )... );
			}
			Node &n = m_Nodes[h];
			n.next = pos;
			n.prev = pos == invalid ? m_Tail : m_Nodes[pos].prev;
			( n.prev == invalid ? m_Head : m_Nodes[n.prev].next ) = h;
			( pos == invalid ? m_Tail : m_Nodes[pos].prev ) = h;
			++m_nLive;
			return h;
		}

		void erase( I h )
		{
			Node &n = m_Nodes[h];
			( n.prev == invalid ? m_Head : m_Nodes[n.prev].next ) = n.next;
			( n.next == invalid ? m_Tail : m_Nodes[n.next].prev ) = n.prev;
			n = Node{};
			m_Free.push_back( h );
			--m_nLive;
		}

		bool contains( I h ) const	{ return h != invalid && std::size_t( h ) < m_Nodes.size() && m_Nodes[h].v.has_value(); }
		T &operator[]( I h )		{ return *m_Nodes[h].v; }
		const T &operator[]( I h ) const { return *m_Nodes[h].v; }
		I head() const				{ return m_Head; }
		I tail() const				{ return m_Tail; }
		I next( I h ) const			{ return m_Nodes[h].next; }
		I prev( I h ) const			{ return m_Nodes[h].prev; }

		std::size_t size() const			{ return m_nLive; }
		bool empty() const			{ return m_nLive == 0; }
		std::size_t slot_count() const	{ return m_Nodes.size(); }
		void clear()				{ m_Nodes.clear(); m_Free.clear(); m_Head = m_Tail = invalid; m_nLive = 0; }

		template < bool Const >
		class basic_iterator
		{
			using Pool = std::conditional_t< Const, const linked_slot_pool, linked_slot_pool >;
		public:
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			basic_iterator() = default;
			basic_iterator( Pool *p, I h ) : m_p( p ), m_h( h ) {}
			auto &operator*() const					{ return ( *m_p )[m_h]; }
			auto *operator->() const				{ return &( *m_p )[m_h]; }
			I handle() const						{ return m_h; }
			basic_iterator &operator++()			{ m_h = m_p->next( m_h ); return *this; }
			basic_iterator operator++( int )		{ auto t = *this; ++*this; return t; }
			bool operator==( const basic_iterator &o ) const { return m_h == o.m_h; }
		private:
			Pool *m_p = nullptr;
			I m_h = invalid;
		};
		using iterator = basic_iterator< false >;
		using const_iterator = basic_iterator< true >;

		iterator begin()				{ return { this, m_Head }; }
		iterator end()					{ return { this, invalid }; }
		const_iterator begin() const	{ return { this, m_Head }; }
		const_iterator end() const		{ return { this, invalid }; }

	private:
		struct Node
		{
			std::optional< T > v;
			I prev = invalid;
			I next = invalid;
		};
		std::deque< Node > m_Nodes;
		std::vector< I > m_Free;
		I m_Head = invalid;
		I m_Tail = invalid;
		std::size_t m_nLive = 0;
	};
}

}
