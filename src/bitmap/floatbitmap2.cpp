// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

import ks.core;
import std;
#include "bitmap/floatbitmap.h"


static float ScaleValue( float f, float overbright )
{
	// map a value between 0..255 to the scale factor
	int ival = ( int )f;
	return ival * ( overbright / 255.0 );
}

static float IScaleValue( float f, float overbright )
{
	f *= ( 1.0 / overbright );
	int ival = ( int )std::min<std::common_type_t<int, double>>( 255, std::ceil( f * 255.0 ));
	return ival;
}

void MaybeSetScaleVaue( FloatBitMap_t const & orig, FloatBitMap_t & newbm, int x, int y,
					   float newscale, float overbright )
{
	// clamp the given scale value to the legal range for that pixel and regnerate the rgb
	// components.
	float maxc = std::max( std::max( orig.Pixel( x, y, 0, 0 ), orig.Pixel( x, y, 0, 1 )), orig.Pixel( x, y, 0, 2 ));
	if ( maxc == 0.0 )
	{
		// pixel is black. any scale value is fine.
		newbm.Pixel( x, y, 0, 3 ) = newscale;
		for( int c = 0;c < 3;c++ )
			newbm.Pixel( x, y, 0, c ) = 0;
	}
	else
	{
//		float desired_floatscale=maxc;
		float scale_we_will_get = ScaleValue( newscale, overbright );
//		if (scale_we_will_get >= desired_floatscale )
		{
			newbm.Pixel( x, y, 0, 3 ) = newscale;
			for( int c = 0;c < 3;c++ )
				newbm.Pixel( x, y, 0, c ) = orig.Pixel( x, y, 0, c ) / ( scale_we_will_get );
		}
	}
}


void FloatBitMap_t::Uncompress( float overbright )
{
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
		{
			int iactual_alpha_value = ( int )( 255.0 * Pixel( x, y, 0, 3 ) );
			float actual_alpha_value = iactual_alpha_value * ( 1.0 / 255.0 );
			for( int c = 0;c < 3;c++ )
			{
				int iactual_color_value = ( int )( 255.0 * Pixel( x, y, 0, c ) );
				float actual_color_value = iactual_color_value * ( 1.0 / 255.0 );
				Pixel( x, y, 0, c ) = actual_alpha_value * actual_color_value * overbright;
			}
		}
}

constexpr auto GAUSSIAN_WIDTH = 5;
#define SQ(x) ((x)*(x))

void FloatBitMap_t::CompressTo8Bits( float overbright )
{
	FloatBitMap_t TmpFBM( NumCols(), NumRows() );
	// first, saturate to max overbright
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
			for( int c = 0;c < 3;c++ )
				Pixel( x, y, 0, c ) = std::min( overbright, Pixel( x, y, 0, c ));
	// first pass - choose nominal scale values to convert to rgb,scale
	for( int y = 0;y < NumRows();y++ )
		for( int x = 0;x < NumCols();x++ )
		{
			// determine maximum component
			float maxc = std::max( std::max( Pixel( x, y, 0, 0 ), Pixel( x, y, 0, 1 )), Pixel( x, y, 0, 2 ));
			if ( maxc == 0 )
			{
				for( int c = 0;c < 4;c++ )
					TmpFBM.Pixel( x, y, 0, c ) = 0;
			}
			else
			{
				float desired_floatscale = maxc;
				float closest_iscale = IScaleValue( desired_floatscale, overbright );
				float scale_value_we_got = ScaleValue( closest_iscale, overbright );
				TmpFBM.Pixel( x, y, 0, 3 ) = closest_iscale;
				for( int c = 0;c < 3;c++ )
					TmpFBM.Pixel( x, y, 0, c ) = Pixel( x, y, 0, c ) / scale_value_we_got;
			}
		}
	// now, refine scale values

	CopyAttrFrom( TmpFBM, FBM_ATTR_RED );
	CopyAttrFrom( TmpFBM, FBM_ATTR_GREEN );
	CopyAttrFrom( TmpFBM, FBM_ATTR_BLUE );
	CopyAttrFrom( TmpFBM, FBM_ATTR_ALPHA );

	// now, map scale to real value
	for( int y = 0; y < NumRows(); y++ )
		for( int x = 0; x < NumCols(); x++ )
			Pixel( x, y, 0, 3 ) *= ( 1.0 / 255.0 );
}
