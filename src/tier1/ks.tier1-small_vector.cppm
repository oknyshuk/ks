// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:small_vector;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	template < class T, std::size_t N >
	class small_vector
	{
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference = T &;
		using const_reference = const T &;
		using pointer = T *;
		using const_pointer = const T *;
		using iterator = T *;
		using const_iterator = const T *;

		small_vector() = default;
		explicit small_vector( size_type n ) { resize( n ); }
		small_vector( size_type n, const T &v ) { assign( n, v ); }
		small_vector( std::initializer_list< T > il ) { assign( il.begin(), il.end() ); }
		template < std::input_iterator It > small_vector( It first, It last ) { assign( first, last ); }
		small_vector( const small_vector &o ) { assign( o.begin(), o.end() ); }
		small_vector( small_vector &&o ) noexcept( std::is_nothrow_move_constructible_v< T > ) { steal( o ); }
		~small_vector() { std::destroy( begin(), end() ); release(); }

		small_vector &operator=( const small_vector &o )
		{
			if ( this != &o )
				assign( o.begin(), o.end() );
			return *this;
		}
		small_vector &operator=( small_vector &&o ) noexcept( std::is_nothrow_move_constructible_v< T > )
		{
			if ( this != &o )
			{
				clear();
				release();
				m_pData = inline_data(); m_nCap = N;
				steal( o );
			}
			return *this;
		}

		iterator begin()						{ return m_pData; }
		const_iterator begin() const			{ return m_pData; }
		iterator end()							{ return m_pData + m_nSize; }
		const_iterator end() const				{ return m_pData + m_nSize; }
		const_iterator cbegin() const			{ return begin(); }
		const_iterator cend() const				{ return end(); }

		size_type size() const					{ return m_nSize; }
		size_type capacity() const				{ return m_nCap; }
		bool empty() const						{ return m_nSize == 0; }
		T *data()								{ return m_pData; }
		const T *data() const					{ return m_pData; }
		T &operator[]( size_type i )			{ return m_pData[i]; }
		const T &operator[]( size_type i ) const	{ return m_pData[i]; }
		T &front()								{ return m_pData[0]; }
		const T &front() const					{ return m_pData[0]; }
		T &back()								{ return m_pData[m_nSize - 1]; }
		const T &back() const					{ return m_pData[m_nSize - 1]; }

		void reserve( size_type n )				{ if ( n > m_nCap ) regrow( n ); }
		void shrink_to_fit()					{}

		template < class... A > T &emplace_back( A &&...a )
		{
			if ( m_nSize == m_nCap )
			{
				T tmp( std::forward< A >( a )... );		// an argument may alias an element
				regrow( m_nCap * 2 );
				std::construct_at( end(), std::move( tmp ) );
			}
			else
			{
				std::construct_at( end(), std::forward< A >( a )... );
			}
			++m_nSize;
			return back();
		}
		void push_back( const T &v )			{ emplace_back( v ); }
		void push_back( T &&v )					{ emplace_back( std::move( v ) ); }
		void pop_back()							{ std::destroy_at( &back() ); --m_nSize; }
		void clear()							{ std::destroy( begin(), end() ); m_nSize = 0; }

		void resize( size_type n )
		{
			if ( n < m_nSize ) { std::destroy( begin() + n, end() ); m_nSize = n; return; }
			reserve( n );
			std::uninitialized_value_construct( end(), begin() + n );
			m_nSize = n;
		}
		void resize( size_type n, const T &v )
		{
			if ( n < m_nSize ) { std::destroy( begin() + n, end() ); m_nSize = n; return; }
			T tmp( v );
			reserve( n );
			std::uninitialized_fill( end(), begin() + n, tmp );
			m_nSize = n;
		}
		void assign( size_type n, const T &v )
		{
			T tmp( v );
			clear(); reserve( n );
			std::uninitialized_fill_n( begin(), n, tmp );
			m_nSize = n;
		}
		template < std::input_iterator It > void assign( It first, It last )
		{
			clear();
			for ( ; first != last; ++first )
				emplace_back( *first );
		}
		template < class R > void append_range( R &&r )
		{
			for ( auto &&x : r )
				emplace_back( std::forward< decltype( x ) >( x ) );
		}

		template < class... A > iterator emplace( const_iterator pos, A &&...a )
		{
			size_type i = pos - begin();
			emplace_back( std::forward< A >( a )... );
			std::rotate( begin() + i, end() - 1, end() );
			return begin() + i;
		}
		iterator insert( const_iterator pos, const T &v )	{ return emplace( pos, v ); }
		iterator insert( const_iterator pos, T &&v )		{ return emplace( pos, std::move( v ) ); }
		iterator insert( const_iterator pos, size_type n, const T &v )
		{
			size_type i = pos - begin(), old = m_nSize;
			T tmp( v );
			for ( size_type k = 0; k < n; ++k )
				emplace_back( tmp );
			std::rotate( begin() + i, begin() + old, end() );
			return begin() + i;
		}
		template < std::input_iterator It > iterator insert( const_iterator pos, It first, It last )
		{
			size_type i = pos - begin(), old = m_nSize;
			for ( ; first != last; ++first )
				emplace_back( *first );
			std::rotate( begin() + i, begin() + old, end() );
			return begin() + i;
		}
		iterator erase( const_iterator pos )				{ return erase( pos, pos + 1 ); }
		iterator erase( const_iterator first, const_iterator last )
		{
			iterator f = begin() + ( first - begin() ), l = begin() + ( last - begin() );
			iterator e = std::move( l, end(), f );
			std::destroy( e, end() );
			m_nSize -= l - f;
			return f;
		}
		void swap( small_vector &o )						{ small_vector t( std::move( o ) ); o = std::move( *this ); *this = std::move( t ); }

	private:
		static constexpr bool kOverAligned = alignof( T ) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;

		T *inline_data()							{ return reinterpret_cast< T * >( m_Inline ); }
		bool is_inline() const						{ return m_pData == reinterpret_cast< const T * >( m_Inline ); }

		static T *allocate( size_type n )
		{
			if constexpr ( kOverAligned )
				return static_cast< T * >( ::operator new( n * sizeof( T ), std::align_val_t( alignof( T ) ) ) );
			else
				return static_cast< T * >( ::operator new( n * sizeof( T ) ) );
		}
		static void deallocate( T *p, size_type n )
		{
			if constexpr ( kOverAligned )
				::operator delete( p, n * sizeof( T ), std::align_val_t( alignof( T ) ) );
			else
				::operator delete( p, n * sizeof( T ) );
		}
		void release()								{ if ( !is_inline() ) deallocate( m_pData, m_nCap ); }

		void regrow( size_type n )
		{
			T *p = allocate( n );
			std::uninitialized_move( begin(), end(), p );
			std::destroy( begin(), end() );
			release();
			m_pData = p; m_nCap = n;
		}

		// *this is empty and inline.
		void steal( small_vector &o )
		{
			if ( o.is_inline() )
			{
				std::uninitialized_move( o.begin(), o.end(), inline_data() );
				m_nSize = o.m_nSize;
				o.clear();
			}
			else
			{
				m_pData = o.m_pData; m_nSize = o.m_nSize; m_nCap = o.m_nCap;
				o.m_pData = o.inline_data(); o.m_nSize = 0; o.m_nCap = N;
			}
		}

		T *m_pData = inline_data();
		size_type m_nSize = 0;
		size_type m_nCap = N;
		alignas( T ) std::byte m_Inline[N * sizeof( T )];
	};
}

}
