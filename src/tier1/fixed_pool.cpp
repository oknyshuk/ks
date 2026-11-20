// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

import std;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.fixed_pool;

namespace ks
{
	fixed_pool::fixed_pool( std::size_t nBlockSize, int nBlocksPerBlob, grow eGrow, const char *pszOwner, std::size_t nAlign )
		: m_nAlign( std::max( nAlign, alignof( void * ) ) ), m_nBlocksPerBlob( nBlocksPerBlob ), m_eGrow( eGrow ),
		  m_pszOwner( pszOwner ? pszOwner : __FILE__ )
	{
		contract_assert( ( m_nAlign & ( m_nAlign - 1 ) ) == 0 );
		// Every block must hold the free-list link and keep the next block aligned.
		m_nBlockSize = ( std::max( nBlockSize, sizeof( void * ) ) + m_nAlign - 1 ) & ~( m_nAlign - 1 );
		add_blob();
	}

	void fixed_pool::blob_delete::operator()( std::byte *p ) const
	{
		if ( nAlign > __STDCPP_DEFAULT_NEW_ALIGNMENT__ )
			::operator delete( p, std::align_val_t( nAlign ) );
		else
			::operator delete( p );
	}

	bool fixed_pool::add_blob()
	{
		if ( m_eGrow == grow::none && !m_Blobs.empty() )
			return false;

		const int nBlocks = blob_blocks( m_Blobs.size() );
		contract_assert( nBlocks > 0 );
		const std::size_t nBytes = m_nBlockSize * nBlocks;
		void *pMem = m_nAlign > __STDCPP_DEFAULT_NEW_ALIGNMENT__ ? ::operator new( nBytes, std::align_val_t( m_nAlign ) ) : ::operator new( nBytes );
		std::byte *p = static_cast< std::byte * >( pMem );
		m_Blobs.emplace_back( p, blob_delete{ m_nAlign } );

		// Only called with the free list empty.
		for ( int i = 0; i < nBlocks - 1; ++i )
			*reinterpret_cast< void ** >( p + i * m_nBlockSize ) = p + ( i + 1 ) * m_nBlockSize;
		*reinterpret_cast< void ** >( p + ( nBlocks - 1 ) * m_nBlockSize ) = nullptr;
		m_pFree = p;
		return true;
	}

	void fixed_pool::clear()
	{
		m_Blobs.clear();
		m_pFree = nullptr;
		m_nLive = 0;
	}

	std::size_t fixed_pool::size() const
	{
		std::size_t n = 0;
		for ( std::size_t i = 0; i < m_Blobs.size(); ++i )
			n += blob_blocks( i ) * m_nBlockSize;
		return n;
	}

	bool fixed_pool::owns( const void *pMem ) const
	{
		const std::byte *p = static_cast< const std::byte * >( pMem );
		for ( std::size_t i = 0; i < m_Blobs.size(); ++i )
		{
			const std::byte *pBlob = m_Blobs[i].get();
			if ( p >= pBlob && p < pBlob + blob_blocks( i ) * m_nBlockSize )
				return std::size_t( p - pBlob ) % m_nBlockSize == 0;
		}
		return false;
	}
}
