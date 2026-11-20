// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.handle_table;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	using handle_t = std::uint32_t;
	inline constexpr handle_t invalid_handle = ~handle_t( 0 );

	template < class T, int HandleBits >
	class handle_table
	{
		static constexpr handle_t index_mask = ( handle_t( 1 ) << HandleBits ) - 1;
		static constexpr handle_t max_serial = ( handle_t( 1 ) << ( 32 - HandleBits ) ) - 1;

	public:
		// Allocates a handle that resolves to null until set().
		handle_t add()
		{
			std::uint32_t index;
			if ( !m_unused.empty() )
			{
				index = m_unused.front();
				m_unused.pop();
			}
			else
			{
				contract_assert( m_entries.size() <= index_mask );
				index = (std::uint32_t)m_entries.size();
				m_entries.emplace_back();
			}
			entry_t &e = m_entries[ index ];
			e.live = true;
			e.pData = nullptr;
			return ( e.serial << HandleBits ) | index;
		}

		void remove( handle_t h )
		{
			entry_t *e = lookup( h );
			if ( !e )
				return;
			e->live = false;
			e->pData = nullptr;
			++e->serial;
			if ( e->serial < max_serial )
				m_unused.push( h & index_mask );
		}

		void set( handle_t h, T *pData )
		{
			entry_t *e = lookup( h );
			contract_assert( e );
			if ( e )
				e->pData = pData;
		}

		T *get( handle_t h ) const
		{
			const entry_t *e = lookup( h );
			return e ? e->pData : nullptr;
		}

	private:
		struct entry_t
		{
			std::uint32_t serial = 0;
			bool live = false;
			T *pData = nullptr;
		};

		entry_t *lookup( handle_t h ) { return const_cast< entry_t * >( std::as_const( *this ).lookup( h ) ); }

		const entry_t *lookup( handle_t h ) const
		{
			if ( h == invalid_handle )
				return nullptr;
			const handle_t index = h & index_mask;
			contract_assert( index < m_entries.size() );
			if ( index >= m_entries.size() )
				return nullptr;
			const entry_t &e = m_entries[ index ];
			if ( !e.live || e.serial != ( h >> HandleBits ) )
				return nullptr;
			return &e;
		}

		std::vector< entry_t > m_entries;
		std::queue< std::uint32_t > m_unused;	// FIFO: delays reuse of a freed slot
	};
}

}
