// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.halton;
import ks.math.mathlib;
import ks.platform;
import ks.simd;

HaltonSequenceGenerator_t::HaltonSequenceGenerator_t(int b)
{
	base=b;
	fbase=(float) b;
	seed=1;

}

float HaltonSequenceGenerator_t::GetElement(int elem)
{
	int tmpseed=seed;
	float ret=0.0;
	float base_inv=1.0/fbase;
	while(tmpseed)
	{
		int dig=tmpseed % base;
		ret+=((float) dig)*base_inv;
		base_inv/=fbase;
		tmpseed/=base;
	}
	return ret;
}


int InsideOut( int nTotal, int nCounter )
{
    int b = 0;
	for ( int m = nTotal, k = 1; k < nTotal; k <<= 1 )
	{
		if ( nCounter << 1 >= m )
		{
			b += k;
			nCounter -= ( m + 1 ) >> 1;
			m >>= 1;
		}
		else 
		{
			m = ( m + 1 ) >> 1;
		}
	}
	contract_assert( ( b >= 0 ) && ( b < nTotal ) );
	return b;
}
