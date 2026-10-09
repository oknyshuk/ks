// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:bit_ops;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	template < std::size_t N >
	inline constexpr int word_count = static_cast< int >( ( N + 31 ) / 32 );

	template < std::size_t N >
	std::uint32_t get_word( const std::bitset< N > &bits, int word )
	{
		return static_cast< std::uint32_t >( ( ( bits >> ( 32 * word ) ) & std::bitset< N >( 0xFFFFFFFFull ) ).to_ullong() );
	}

	template < std::size_t N >
	void set_word( std::bitset< N > &bits, int word, std::uint32_t value )
	{
		const std::bitset< N > mask( 0xFFFFFFFFull );
		bits &= ~( mask << ( 32 * word ) );
		bits |= std::bitset< N >( value ) << ( 32 * word );
	}

	template < std::size_t N >
	int next_set_bit( const std::bitset< N > &bits, int start )
	{
		if ( start < 0 || static_cast< std::size_t >( start ) >= N )
			return -1;
		const std::size_t at = start == 0 ? bits._Find_first() : bits._Find_next( start - 1 );
		return at < N ? static_cast< int >( at ) : -1;
	}

	template < class Alloc >
	int next_set_bit( const std::vector< bool, Alloc > &bits, int start )
	{
		for ( std::size_t i = start < 0 ? 0 : start; i < bits.size(); ++i )
		{
			if ( bits[i] )
				return static_cast< int >( i );
		}
		return -1;
	}

	template < std::size_t N >
	bool test_and_set( std::bitset< N > &bits, int index )
	{
		const bool was = bits[index];
		bits.set( index );
		return was;
	}

	template < class Alloc >
	bool test_and_set( std::vector< bool, Alloc > &bits, int index )
	{
		const bool was = bits[index];
		bits[index] = true;
		return was;
	}
}

}
