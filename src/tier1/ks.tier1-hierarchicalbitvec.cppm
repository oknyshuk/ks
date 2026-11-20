// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.hierarchicalbitvec;

import std;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;

export extern "C++" {

class CHierarchicalBitVector
{
public:
	CHierarchicalBitVector( ){}
	CHierarchicalBitVector( int nInitialBitCount )
	{
		if ( nInitialBitCount > 0 )
		{
			EnsureBitExists( nInitialBitCount - 1 );
		}
	}
	CHierarchicalBitVector( const CHierarchicalBitVector &other )
	{
		m_Level0.assign( other.m_Level0.begin(), other.m_Level0.end() );
		m_Level1.assign( other.m_Level1.begin(), other.m_Level1.end() );
	}

	void Set( int nBit )
	{
		m_Level0[ nBit >>  5 ] |= 1 << (   nBit & 31 );
		m_Level1[ nBit >> 10 ] |= 1 << ( ( nBit >> 5 ) & 31 );
	}
	void Reset( int nBit )
	{
		m_Level0[ nBit >> 5 ] &= ~( 1 << ( nBit & 31 ) );
	}

	uint32 operator[] ( int nBit ) const
	{
		uint32 nBitSet = ( m_Level0[ nBit >> 5 ] >> ( nBit & 31 ) ) & 1;
		contract_assert( ( ( m_Level1[ nBit >> 10 ] >> ( ( nBit >> 5 ) & 31 ) ) & 1 ) || !m_Level0[ nBit >> 5 ] );
		return nBitSet;
	}
	const uint32 * Base() const
	{
		return m_Level0.data();
	}

	void Validate()const
	{
	}
	
	void EnsureBitExists( int nBit ) // reallocate if necessary
	{
		int nLevel0Size = ( nBit >> 5 ) + 1, nLevel0OldSize = std::ssize( m_Level0 );
		if( nLevel0OldSize < nLevel0Size )
		{
			m_Level0.resize( m_Level0.size() + ( nLevel0Size - nLevel0OldSize ) );
			for( int i = nLevel0OldSize; i < nLevel0Size; ++i )
			{
				m_Level0[i] = 0;
			}
			int nLevel1Size = ( nBit >> 10 ) + 1, nLevel1OldSize = std::ssize( m_Level1 );
			if( nLevel1OldSize < nLevel1Size )
			{
				m_Level1.resize( m_Level1.size() + ( nLevel1Size - nLevel1OldSize ) );
				for( int i = nLevel1OldSize; i < nLevel1Size; ++i )
				{
					m_Level1[i] = 0;
				}
			}
		}
	}

	int Count()
	{
		return std::ssize( m_Level0 ) << 5;
	}

	template < typename Functor >
	void ScanBits( Functor functor )
	{
		for( int nItLevel1 = 0; nItLevel1 < std::ssize( m_Level1 ); ++nItLevel1 )
		{
			uint nLevel1Bits = m_Level1[ nItLevel1 ];
			uint nBaseLevel1 = nItLevel1 * 32;
			while( nLevel1Bits )
			{
				uint32 nOffsetLevel1 = __builtin_ctz( nLevel1Bits );
				contract_assert( nLevel1Bits & ( 1 << nOffsetLevel1 ) );
				nLevel1Bits ^= 1 << nOffsetLevel1;

				uint nItLevel0 = nBaseLevel1 + nOffsetLevel1;
				uint nLevel0Bits = m_Level0[ nItLevel0 ];
				if( nLevel0Bits != 0 ) // if the bit is set in level1, it doesn't mean the uint32 in level0 is always non-zero: it may have been reset
				{
					uint nBaseLevel0 = nItLevel0 * 32;
					do
					{
						uint32 nOffsetLevel0 = __builtin_ctz( nLevel0Bits );
						contract_assert( nLevel0Bits & ( 1 << nOffsetLevel0 ) );
						nLevel0Bits ^= 1 << nOffsetLevel0;
						// Perform tree queries for all moving objects
						functor( nBaseLevel0 + nOffsetLevel0 );
					}
					while( nLevel0Bits );
				}
			}
		}
	}
	void Clear( )
	{
		std::ranges::fill( m_Level0, 0 );
		std::ranges::fill( m_Level1, 0 );
	}
protected:
	std::vector< uint32 > m_Level1; // one bit for each 32 bits  in m_MoveBufferLevel0
	std::vector< uint32 > m_Level0; // one bit for each proxy

public:
};

}
