// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

import std;
import ks.core.random;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;
#include "bitmap/floatbitmap.h"


void FloatBitMap_t::InitializeWithRandomPixelsFromAnotherFloatBM( FloatBitMap_t const & other )
{
	for( int z = 0; z < NumSlices(); z++ )
	{
		for( int y = 0;y < NumRows();y++ )
		{
			for( int x = 0;x < NumCols();x++ )
			{
				int x1 = RandomInt( 0, other.NumCols() - 1 );
				int y1 = RandomInt( 0, other.NumRows() - 1 );
				int z1 = RandomInt( 0, other.NumSlices() - 1 );
				for( int c = 0; c < 4; c++ )
				{
					Pixel( x, y, z, c ) = other.Pixel( x1, y1, z1, c );
				}
			}
		}
	}
}


void FloatBitMap_t::QuarterSizeWithGaussian( FloatBitMap_t *pBitmap )
{
	// generate a new bitmap half on each axis, using a separable gaussian.
	static float kernel[]={.05, .25, .4, .25, .05};
	pBitmap->Init( NumCols() / 2, NumRows() / 2 );

	for( int y = 0;y < NumRows() / 2;y++ )
		for( int x = 0;x < NumCols() / 2;x++ )
		{
			for( int c = 0;c < 4;c++ )
			{
				float sum = 0;
				float sumweights = 0;							// for versatility in handling the
															// offscreen case
				for( int xofs =- 2;xofs <= 2;xofs++ )
				{
					int orig_x = std::max( 0, std::min( NumCols() - 1, x * 2 + xofs ));
					for( int yofs =- 2;yofs <= 2;yofs++ )
					{
						int orig_y = std::max( 0, std::min( NumRows() - 1, y * 2 + yofs ));
						float coeff = kernel[xofs + 2]* kernel[yofs + 2];
						sum += Pixel( orig_x, orig_y, 0, c ) * coeff;
						sumweights += coeff;
					}
				}
				pBitmap->Pixel( x, y, 0, c ) = sum / sumweights;
			}
		}
}

FloatImagePyramid_t::FloatImagePyramid_t( FloatBitMap_t const & src, ImagePyramidMode_t mode )
{
	std::memset( m_pLevels, 0, sizeof( m_pLevels ));
	m_nLevels = 1;
	m_pLevels[0]= new FloatBitMap_t( & src );
	ReconstructLowerResolutionLevels( 0 );
}

void FloatImagePyramid_t::ReconstructLowerResolutionLevels( int start_level )
{
	while( ( m_pLevels[start_level]-> NumCols() > 1 ) && ( m_pLevels[start_level]-> NumRows() > 1 ) )
	{
		if ( m_pLevels[start_level + 1] )
			delete m_pLevels[start_level + 1];
		m_pLevels[start_level + 1] = new FloatBitMap_t;
		m_pLevels[start_level]->QuarterSizeWithGaussian( m_pLevels[start_level + 1] );
		start_level++;
	}
	m_nLevels = start_level + 1;
}

float & FloatImagePyramid_t::Pixel( int x, int y, int component, int level ) const
{
	contract_assert( level < m_nLevels );
	x <<= level;
	y <<= level;
	return m_pLevels[level]-> Pixel( x, y, 0, component );
}

void FloatImagePyramid_t::WriteTGAs( char const * basename ) const
{
	for( int l = 0;l < m_nLevels;l++ )
	{
		char bname_out[1024];
		V_snprintf(bname_out,sizeof(bname_out),"%s_%02d.tga",basename,l);
		m_pLevels[l]-> WriteTGAFile( bname_out );
	}

}


FloatImagePyramid_t::~FloatImagePyramid_t( void )
{
	for( int l = 0;l < m_nLevels;l++ )
		if ( m_pLevels[l] )
			delete m_pLevels[l];
}
