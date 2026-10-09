// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:string_token;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	// Case-insensitive (ASCII) MurmurHash2 of a name; values are persisted and compared against precomputed tokens.
	constexpr std::uint32_t string_token( std::string_view s, std::uint32_t seed = 0x31415926 ) noexcept
	{
		constexpr std::uint32_t m = 0x5bd1e995;
		auto lower = []( char c ) constexpr -> std::uint32_t
		{
			return static_cast< unsigned char >( c >= 'A' && c <= 'Z' ? c + 32 : c );
		};
		std::size_t len = s.size();
		std::uint32_t h = seed ^ static_cast< std::uint32_t >( len );
		std::size_t i = 0;
		for ( ; len - i >= 4; i += 4 )
		{
			std::uint32_t k = lower( s[ i ] ) | lower( s[ i + 1 ] ) << 8 | lower( s[ i + 2 ] ) << 16 | lower( s[ i + 3 ] ) << 24;
			k *= m;
			k ^= k >> 24;
			k *= m;
			h *= m;
			h ^= k;
		}
		switch ( len - i )
		{
		case 3: h ^= lower( s[ i + 2 ] ) << 16; [[fallthrough]];
		case 2: h ^= lower( s[ i + 1 ] ) << 8; [[fallthrough]];
		case 1: h ^= lower( s[ i ] ); h *= m;
		}
		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;
		return h;
	}
}

}
