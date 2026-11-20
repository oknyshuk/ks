// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

// Purpose: generates 4 randum numbers in the range 0..1 quickly, using SIMD

import std;
import ks.core.threadtools;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.platform;
import ks.simd;

// see knuth volume 3 for insight.

class SIMDRandStreamContext
{
	fltx4 m_RandY[55];

	fltx4 *m_pRand_J, *m_pRand_K;


public:
	void Seed( uint32 seed )
	{
		m_pRand_J=m_RandY+23; m_pRand_K=m_RandY+54;
		for(int i=0;i<55;i++)
		{
			for(int j=0;j<4;j++)
			{
				SubFloat( m_RandY[i], j) = (seed>>16)/65536.0;
				seed=(seed+1)*3141592621u;
			}
		}
	}

	inline fltx4 RandSIMD( void )
	{
		// ret= rand[k]+rand[j]
		fltx4 retval=AddSIMD( *m_pRand_K, *m_pRand_J );
		
		// if ( ret>=1.0) ret-=1.0
		bi32x4 overflow_mask=CmpGeSIMD( retval, Four_Ones );
		retval=SubSIMD( retval, AndSIMD( Four_Ones, overflow_mask ) );
		
		*m_pRand_K = retval;
		
		// update pointers w/ wrap-around
		if ( --m_pRand_J < m_RandY )
			m_pRand_J=m_RandY+54;
		if ( --m_pRand_K < m_RandY )
			m_pRand_K=m_RandY+54;
		
		return retval;
	}
};

constexpr auto MAX_SIMULTANEOUS_RANDOM_STREAMS = 32;

static SIMDRandStreamContext s_SIMDRandContexts[MAX_SIMULTANEOUS_RANDOM_STREAMS];

static std::atomic<int> s_nRandContextsInUse[MAX_SIMULTANEOUS_RANDOM_STREAMS];

void SeedRandSIMD(uint32 seed)
{
	for( int i = 0; i<MAX_SIMULTANEOUS_RANDOM_STREAMS; i++)
		s_SIMDRandContexts[i].Seed( seed+i );
}

fltx4 RandSIMD( int nContextIndex )
{
	return s_SIMDRandContexts[nContextIndex].RandSIMD();
}

int GetSIMDRandContext( void )
{
	for(;;)
	{
		for(int i=0; i < NELEMS( s_SIMDRandContexts ); i++)
		{
			if ( ! s_nRandContextsInUse[i] )				// available?
			{
				// try to take it!
				int nExpected = 0;
				if ( s_nRandContextsInUse[i].compare_exchange_strong( nExpected, 1 ) )
				{
					std::atomic_thread_fence( std::memory_order_seq_cst );
					return i;								// done!
				}
			}
		}
		contract_assert( 0 );											// why don't we have enough buffers?
		ThreadSleep();
	}
}

void ReleaseSIMDRandContext( int nContext )
{
	std::atomic_thread_fence( std::memory_order_seq_cst );
	s_nRandContextsInUse[ nContext ] = 0;
}


fltx4 RandSIMD( void )
{
	return s_SIMDRandContexts[0].RandSIMD();
}
