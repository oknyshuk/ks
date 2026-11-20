// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.map_ops;

import std;
import ks.core.basetypes;
import ks.core.vprof;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	namespace detail
	{
		constexpr unsigned char ascii_lower( unsigned char c ) { return ( c >= 'A' && c <= 'Z' ) ? c + ( 'a' - 'A' ) : c; }
		constexpr unsigned char path_fold( unsigned char c ) { return c == '\\' ? '/' : ascii_lower( c ); }

		template < unsigned char ( *Fold )( unsigned char ) >
		constexpr bool fold_less( std::string_view a, std::string_view b )
		{
			std::size_t n = a.size() < b.size() ? a.size() : b.size();
			for ( std::size_t i = 0; i < n; ++i )
			{
				unsigned char x = Fold( (unsigned char)a[i] ), y = Fold( (unsigned char)b[i] );
				if ( x != y )
					return x < y;
			}
			return a.size() < b.size();
		}

		template < unsigned char ( *Fold )( unsigned char ) >
		constexpr bool fold_equal( std::string_view a, std::string_view b )
		{
			if ( a.size() != b.size() )
				return false;
			for ( std::size_t i = 0; i < a.size(); ++i )
				if ( Fold( (unsigned char)a[i] ) != Fold( (unsigned char)b[i] ) )
					return false;
			return true;
		}

		// FNV-1a over the folded characters.
		template < unsigned char ( *Fold )( unsigned char ) >
		constexpr std::size_t fold_hash( std::string_view s )
		{
			std::uint64_t h = 14695981039346656037ull;
			for ( char c : s )
				h = ( h ^ Fold( (unsigned char)c ) ) * 1099511628211ull;
			return (std::size_t)h;
		}
	}

	// Case-insensitive (ASCII) ordering, like V_stricmp. Transparent: find( "name" ) builds no std::string.
	struct iless
	{
		using is_transparent = void;
		constexpr bool operator()( std::string_view a, std::string_view b ) const { return detail::fold_less< detail::ascii_lower >( a, b ); }
	};

	// Filenames: case-insensitive, and '/' matches '\'.
	struct path_iless
	{
		using is_transparent = void;
		constexpr bool operator()( std::string_view a, std::string_view b ) const { return detail::fold_less< detail::path_fold >( a, b ); }
	};

	// Case-insensitive (ASCII) hash and equality for unordered containers. Transparent, like iless.
	struct ihash
	{
		using is_transparent = void;
		constexpr std::size_t operator()( std::string_view s ) const { return detail::fold_hash< detail::ascii_lower >( s ); }
	};
	struct iequal
	{
		using is_transparent = void;
		constexpr bool operator()( std::string_view a, std::string_view b ) const { return detail::fold_equal< detail::ascii_lower >( a, b ); }
	};

	// For const char * keys that point at strings owned elsewhere.
	struct cstr_less
	{
		bool operator()( const char *a, const char *b ) const { return std::strcmp( a, b ) < 0; }
	};
	struct cstr_iless
	{
		bool operator()( const char *a, const char *b ) const { return iless()( a, b ); }
	};

	// CUtlDict's default: owns its keys, ordered case-insensitively.
	template < class T >
	using idict = std::map< std::string, T, iless >;

	// Unordered, case-insensitive string keys.
	template < class T >
	using ihash_map = std::unordered_map< std::string, T, ihash, iequal >;

	// The mapped value for k, or null.
	template < class M, class K >
	auto *find_ptr( M &m, const K &k )
	{
		auto it = m.find( k );
		return it == m.end() ? nullptr : &it->second;
	}
}

}
