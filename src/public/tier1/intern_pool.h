// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Interned strings. intern() returns a view into pooled storage that stays valid (and
// null-terminated, so .data() works as a C string) until clear(). A case-insensitive pool keeps
// the first spelling it saw. Replaces CUtlSymbolTable / CUtlSymbolTableLarge ids.

#ifndef INTERN_POOL_H
#define INTERN_POOL_H

#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "tier1/map_ops.h"

namespace ks
{
	class intern_pool
	{
	public:
		explicit intern_pool( bool bCaseInsensitive = false )
			: m_Strings( 16, hasher{ bCaseInsensitive }, equal{ bCaseInsensitive } ) {}

		std::string_view intern( std::string_view s )		{ return *m_Strings.emplace( s ).first; }

		// The pooled view, or an empty view with a null data() when absent.
		std::string_view find( std::string_view s ) const
		{
			auto it = m_Strings.find( s );
			return it == m_Strings.end() ? std::string_view() : std::string_view( *it );
		}
		bool contains( std::string_view s ) const			{ return m_Strings.contains( s ); }

		size_t size() const									{ return m_Strings.size(); }
		void clear()										{ m_Strings.clear(); }

		auto begin() const									{ return m_Strings.begin(); }
		auto end() const									{ return m_Strings.end(); }

	private:
		struct hasher
		{
			using is_transparent = void;
			bool bCaseInsensitive;
			size_t operator()( std::string_view s ) const	{ return bCaseInsensitive ? ihash()( s ) : std::hash< std::string_view >()( s ); }
		};
		struct equal
		{
			using is_transparent = void;
			bool bCaseInsensitive;
			bool operator()( std::string_view a, std::string_view b ) const { return bCaseInsensitive ? iequal()( a, b ) : a == b; }
		};
		std::unordered_set< std::string, hasher, equal > m_Strings;
	};

	// Thread-safe pool; returned views stay valid without the lock until clear().
	class intern_pool_mt
	{
	public:
		explicit intern_pool_mt( bool bCaseInsensitive = false ) : m_Pool( bCaseInsensitive ) {}

		std::string_view intern( std::string_view s )
		{
			{
				std::shared_lock lock( m_Mutex );
				if ( auto v = m_Pool.find( s ); v.data() )
					return v;
			}
			std::unique_lock lock( m_Mutex );
			return m_Pool.intern( s );
		}
		std::string_view find( std::string_view s ) const	{ std::shared_lock lock( m_Mutex ); return m_Pool.find( s ); }
		bool contains( std::string_view s ) const			{ std::shared_lock lock( m_Mutex ); return m_Pool.contains( s ); }
		size_t size() const									{ std::shared_lock lock( m_Mutex ); return m_Pool.size(); }
		void clear()										{ std::unique_lock lock( m_Mutex ); m_Pool.clear(); }

	private:
		mutable std::shared_mutex m_Mutex;
		intern_pool m_Pool;
	};

	// Case-sensitive pool behind the old global CUtlSymbol( const char * ) table.
	inline intern_pool_mt &global_symbols()
	{
		static intern_pool_mt s_Pool;
		return s_Pool;
	}
}

#endif // INTERN_POOL_H
