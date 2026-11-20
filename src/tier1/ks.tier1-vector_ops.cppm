// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.vector_ops;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	// Makes at least n elements addressable; never shrinks.
	template < class V >
	void grow_to( V &v, std::size_t n )
	{
		if ( v.size() < n )
			v.resize( n );
	}

	// These compare with plain ==, like the CUtlVector methods they replace: std::ranges' concepts
	// reject the handle/pointer comparisons this code relies on.

	// Index of the first element equal to x, or -1.
	template < class V, class T >
	int find_index( const V &v, const T &x )
	{
		auto it = std::find( std::begin( v ), std::end( v ), x );
		return it == std::end( v ) ? -1 : (int)( it - std::begin( v ) );
	}

	template < class V, class T >
	bool contains( const V &v, const T &x )
	{
		return std::find( std::begin( v ), std::end( v ), x ) != std::end( v );
	}

	template < class V >
	bool is_valid_index( const V &v, int i )
	{
		return i >= 0 && i < (int)std::size( v );
	}

	// O(1) removal; the last element takes i's place.
	template < class V >
	void swap_remove( V &v, int i )
	{
		if ( i != (int)v.size() - 1 )
			v[i] = std::move( v.back() );
		v.pop_back();
	}

	template < class V, class T >
	bool erase_first( V &v, const T &x )
	{
		auto it = std::find( v.begin(), v.end(), x );
		if ( it == v.end() )
			return false;
		v.erase( it );
		return true;
	}

	template < class V, class T >
	bool swap_erase_first( V &v, const T &x )
	{
		int i = find_index( v, x );
		if ( i < 0 )
			return false;
		swap_remove( v, i );
		return true;
	}

	template < class V >
	void ensure_size( V &v, int n )
	{
		if ( (int)v.size() < n )
			v.resize( n );
	}

	// qsort with a (const T *, const T *) comparator. Unlike std::sort it tolerates comparators that
	// are not a strict weak ordering, which much of this code base's comparators are not.
	template < class V >
	void qsort( V &v, std::type_identity_t< int ( * )( const typename V::value_type *, const typename V::value_type * ) > cmp )
	{
		if ( v.size() > 1 )
			std::qsort( v.data(), v.size(), sizeof( typename V::value_type ), (int ( * )( const void *, const void * ))cmp );
	}

	// Binary max-heap on a vector (front = greatest under less). Unlike std::push_heap/pop_heap these
	// support O(log n) erase/re-key at any index and report every element move to on_move( elem, index ),
	// so elements can track their heap slot; the sift order is fixed, so equal keys pop deterministically.
	struct heap_no_index
	{
		template < class T > void operator()( T &, int ) const {}
	};

	template < class V, class Less, class OnMove = heap_no_index >
	int heap_sift_up( V &v, int i, Less less, OnMove on_move = {} )
	{
		if ( i >= (int)v.size() )
			return i;
		while ( i != 0 )
		{
			int parent = ( i - 1 ) / 2;
			if ( less( v[i], v[parent] ) )
				break;
			std::swap( v[i], v[parent] );
			on_move( v[i], i );
			on_move( v[parent], parent );
			i = parent;
		}
		return i;
	}

	template < class V, class Less, class OnMove = heap_no_index >
	int heap_sift_down( V &v, int i, Less less, OnMove on_move = {} )
	{
		int count = (int)v.size();
		int larger = i;
		while ( i < count / 2 )
		{
			int child = i * 2 + 1;
			if ( less( v[i], v[child] ) )
				larger = child;
			if ( ++child < count && less( v[larger], v[child] ) )
				larger = child;
			if ( larger == i )
				break;
			std::swap( v[i], v[larger] );
			on_move( v[i], i );
			on_move( v[larger], larger );
			i = larger;
		}
		return i;
	}

	// Restores heap order after v[i]'s key changed.
	template < class V, class Less, class OnMove = heap_no_index >
	void heap_update( V &v, int i, Less less, OnMove on_move = {} )
	{
		if ( heap_sift_down( v, i, less, on_move ) == i )
			heap_sift_up( v, i, less, on_move );
	}

	template < class V, class T, class Less, class OnMove = heap_no_index >
	void heap_push( V &v, T &&x, Less less, OnMove on_move = {} )
	{
		v.push_back( std::forward< T >( x ) );
		int i = (int)v.size() - 1;
		on_move( v[i], i );
		heap_sift_up( v, i, less, on_move );
	}

	template < class V, class Less, class OnMove = heap_no_index >
	void heap_erase( V &v, int i, Less less, OnMove on_move = {} )
	{
		swap_remove( v, i );
		if ( i < (int)v.size() )
			on_move( v[i], i );
		heap_update( v, i, less, on_move );
	}

	template < class V, class Less, class OnMove = heap_no_index >
	void heap_pop( V &v, Less less, OnMove on_move = {} )
	{
		heap_erase( v, 0, less, on_move );
	}

	// std::vector storage aligned to max( N, alignof( T ) ), for SIMD access to element arrays.
	template < class T, std::size_t N >
	struct aligned_allocator
	{
		using value_type = T;
		static constexpr std::align_val_t kAlign{ N > alignof( T ) ? N : alignof( T ) };
		template < class U > struct rebind { using other = aligned_allocator< U, N >; };
		aligned_allocator() = default;
		template < class U > aligned_allocator( const aligned_allocator< U, N > & ) {}
		T *allocate( std::size_t n )				{ return static_cast< T * >( ::operator new( n * sizeof( T ), kAlign ) ); }
		void deallocate( T *p, std::size_t n )		{ ::operator delete( p, n * sizeof( T ), kAlign ); }
		template < class U > bool operator==( const aligned_allocator< U, N > & ) const { return true; }
	};
	template < class T, std::size_t N >
	using aligned_vector = std::vector< T, aligned_allocator< T, N > >;

}

}
