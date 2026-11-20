// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2007 Valve Corporation
// Purpose: Functions for spherical geometry.
// Copyright (c) 2026 oknyshuk

export module ks.math.spherical_geometry;

import std;
import ks.platform;
import ks.simd;
export import ks.math.vector;

export extern "C++" {

// see http://mathworld.wolfram.com/SphericalTrigonometry.html

// return the spherical distance, in radians, between 2 points on the unit sphere.
[[gnu::always_inline]] inline float UnitSphereLineSegmentLength( Vector const &a, Vector const &b )
{
	// check unit length
	contract_assert( std::fabs( VectorLength( a ) - 1.0 ) < 1.0e-3 );
	contract_assert( std::fabs( VectorLength( b ) - 1.0 ) < 1.0e-3 );
	return std::acos( DotProduct( a, b ) );
}

// given 3 points on the unit sphere, return the spherical area (in radians) of the triangle they form.
// valid for "small" triangles.
[[gnu::always_inline]] inline float UnitSphereTriangleArea( Vector const &a, Vector const &b , Vector const &c )
{
	float flLengthA = UnitSphereLineSegmentLength( b, c );
	float flLengthB = UnitSphereLineSegmentLength( c, a );
	float flLengthC = UnitSphereLineSegmentLength( a, b );
	
	if ( ( flLengthA == 0. ) || ( flLengthB == 0. ) || ( flLengthC == 0. ) )
		return 0.;											// zero area triangle
			
	// now, find the 3 incribed angles for the triangle
	float flHalfSumLens = 0.5 * ( flLengthA + flLengthB + flLengthC );
	float flSinSums = std::sin( flHalfSumLens );
	float flSinSMinusA= std::sin( flHalfSumLens - flLengthA );
	float flSinSMinusB= std::sin( flHalfSumLens - flLengthB );
	float flSinSMinusC= std::sin( flHalfSumLens - flLengthC );
	
	float flTanAOver2 = std::sqrt ( ( flSinSMinusB * flSinSMinusC ) / ( flSinSums * flSinSMinusA ) );
	float flTanBOver2 = std::sqrt ( ( flSinSMinusA * flSinSMinusC ) / ( flSinSums * flSinSMinusB ) );
	float flTanCOver2 = std::sqrt ( ( flSinSMinusA * flSinSMinusB ) / ( flSinSums * flSinSMinusC ) );

	// Girards formula : area = sum of angles - pi.
	return 2.0 * ( std::atan( flTanAOver2 ) + std::atan( flTanBOver2 ) + std::atan( flTanCOver2 ) ) - std::numbers::pi;
}

// spherical harmonics-related functions. Best explanation at http://www.research.scea.com/gdc2003/spherical-harmonic-lighting.pdf

// Evaluate associated legendre polynomial P( l, m ) at flX, using recurrence relation
float AssociatedLegendrePolynomial( int nL, int nM, float flX );

// Evaluate order N spherical harmonic with spherical coordinates
// nL = band, 0..N
// nM = -nL .. nL
// theta = 0..M_PI
// phi = 0.. 2 * M_PHI
float SphericalHarmonic( int nL, int nM, float flTheta, float flPhi );

// evaluate spherical harmonic with normalized vector direction
float SphericalHarmonic( int nL, int nM, Vector const &vecDirection );

}
