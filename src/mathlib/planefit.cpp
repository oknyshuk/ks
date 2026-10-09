// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Code to compute the equation of a plane with a least-squares residual fit.

import ks.math;
import std;
using namespace std;

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

static const float DETERMINANT_EPSILON = 1e-6f;

template< int PRIMARY_AXIS >
bool ComputeLeastSquaresPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane );

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

bool ComputeLeastSquaresPlaneFitX( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<0>( pPoints, nNumPoints, pFitPlane );
}

bool ComputeLeastSquaresPlaneFitY( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<1>( pPoints, nNumPoints, pFitPlane );
}

bool ComputeLeastSquaresPlaneFitZ( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	return ComputeLeastSquaresPlaneFit<2>( pPoints, nNumPoints, pFitPlane );
}

float ComputeSquaredError( const Vector *pPoints, int nNumPoints, const VPlane *pFitPlane )
{
	float flSqrError = 0.0f;
	float flError = 0.0f;
	for ( int i = 0; i < nNumPoints; ++ i )
	{
		float flDist = pFitPlane->DistTo( pPoints[i] );
		flError += flDist;
		flSqrError += flDist * flDist;
	}

	return flSqrError;
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

// Because this is not a least-squares orthogonal distance fit, an axis must be specified along which residuals are computed.
// A traditional least-squares linear regression computes residuals along the y-axis and fits to a function of x, meaning that vertical lines cannot be properly fit.
// Similarly, this algorithm cannot properly fit planes which lie along a plane parallel to the primary axis
// 
// PRIMARY_AXIS 
// X = 0
// Y = 1
// Z = 2
template< int PRIMARY_AXIS >
bool ComputeLeastSquaresPlaneFit( const Vector *pPoints, int nNumPoints, VPlane *pFitPlane )
{
	memset( pFitPlane, 0, sizeof( VPlane ) );

	if ( nNumPoints < 3 )
	{
		// We must have at least 3 points to fit a plane
		return false;
	}

	Vector vCentroid( 0, 0, 0 ); // averages: x-bar, y-bar, z-bar
	Vector vSquaredSums( 0, 0, 0 ); // x => x*x, y => y*y, z => z*z
	Vector vCrossSums( 0, 0, 0 ); // x => y*z, y => x*z, z >= x*y
	float flNumPoints = ( float )nNumPoints;
	
	for ( int i = 0; i < nNumPoints; ++ i )
	{
		vCentroid += pPoints[i];
		vSquaredSums += pPoints[i] * pPoints[i];
		vCrossSums.x += pPoints[i].y * pPoints[i].z;
		vCrossSums.y += pPoints[i].x * pPoints[i].z;
		vCrossSums.z += pPoints[i].x * pPoints[i].y;
	}

	vCentroid /= ( float ) nNumPoints;
	
	if ( PRIMARY_AXIS == 0 )
	{
		// swap X and Z
		swap( vCentroid.x, vCentroid.z );
		swap( vSquaredSums.x, vSquaredSums.z );
		swap( vCrossSums.x, vCrossSums.z );
	}
	else if ( PRIMARY_AXIS == 1 )
	{
		// Swap Y and Z
		swap( vCentroid.y, vCentroid.z );
		swap( vSquaredSums.y, vSquaredSums.z );
		swap( vCrossSums.y, vCrossSums.z );
	}

	// Solve system of equations:
	// (example assumes primary axis is Z)
	//
	// A * ( sum( xi * xi ) - n * vCentroid.x^2 ) + B * ( sum( xi * yi ) - n * vCentroid.x * vCentroid.y ) - sum( xi * zi ) + n * vCentroid.x * vCentroid.z = 0
	// A * ( sum( xi * yi ) - n * vCentroid.x * vCentroid.y ) + B * ( sum( yi * yi ) - n * vCentroid.y ^ 2 ) - sum( yi * zi ) + n * vCentroid.y * vCentroid.z = 0
	// C = vCentroid.z - A * vCentroid.x - B * vCentroid.y
	//
	// where z = Ax + By + C
	//
	// Transform to:
	// [ m11 m12 ] [ A ]  =  [ c1 ]
	// [ m21 m22 ] [ B ]  =  [ c2 ]
	//
	// M * x = C
	// Take the inverse of M, post-multiply by C:
	// x = M_inverse * C

	float flM11 = vSquaredSums.x - flNumPoints * vCentroid.x * vCentroid.x;
	float flM12 = vCrossSums.z - flNumPoints * vCentroid.x * vCentroid.y;
	float flC1 = vCrossSums.y - flNumPoints * vCentroid.x * vCentroid.z;
	float flM21 = vCrossSums.z - flNumPoints * vCentroid.x * vCentroid.y;
	float flM22 = vSquaredSums.y - flNumPoints * vCentroid.y * vCentroid.y;
	float flC2 = vCrossSums.x - flNumPoints * vCentroid.y * vCentroid.z;

	float flDeterminant = flM11 * flM22 - flM12 * flM21;
	if ( fabsf( flDeterminant ) > DETERMINANT_EPSILON )
	{
		float flInvDeterminant = 1.0f / flDeterminant;
		float flA = flInvDeterminant * ( flM22 * flC1 - flM12 * flC2 );
		float flB = flInvDeterminant * ( -flM21 * flC1 + flM11 * flC2 );
		float flC = vCentroid.z - flA * vCentroid.x - flB * vCentroid.y;

		pFitPlane->m_Normal = Vector( -flA, -flB, 1.0f );
		float flScale = pFitPlane->m_Normal.NormalizeInPlace();
		pFitPlane->m_Dist = flC * 1.0f / flScale;

		if ( PRIMARY_AXIS == 0 )
		{
			// swap X and Z
			swap( pFitPlane->m_Normal.x, pFitPlane->m_Normal.z );
		}
		else if ( PRIMARY_AXIS == 1 )
		{
			// Swap Y and Z
			swap( pFitPlane->m_Normal.y, pFitPlane->m_Normal.z );
		}

		return true;
	}

	// Bad determinant
	return false;
}


struct Complex_t
{
	float r;
	float i;

	Complex_t() { }
	Complex_t( float flR, float flI ) : r( flR ), i( flI ) { }

	static Complex_t FromPolar( float flRadius, float flTheta )
	{
		return Complex_t( flRadius * cosf( flTheta ), flRadius * sinf( flTheta ) );
	}

	static Complex_t SquareRoot( float flValue )
	{
		if ( flValue < 0.0f )
		{
			return Complex_t( 0.0f, sqrtf( -flValue ) );
		}
		else
		{
			return Complex_t( sqrtf( flValue ), 0.0f );
		}
	}

	Complex_t operator+( const Complex_t &rhs ) const
	{
		return Complex_t( r + rhs.r, i + rhs.i );
	}

	Complex_t operator-( const Complex_t &rhs ) const
	{
		return Complex_t( r - rhs.r, i - rhs.i );
	}

	Complex_t operator*( const Complex_t &rhs ) const
	{
		return Complex_t( r * rhs.r - i * rhs.i, r * rhs.i + i * rhs.r );
	}

	Complex_t operator*( float rhs ) const
	{
		return Complex_t( r * rhs, i * rhs );
	}

	Complex_t operator/( float rhs ) const
	{
		return Complex_t( r / rhs, i / rhs );
	}

	Complex_t CubeRoot() const
	{
		float flRadius = sqrtf( r * r + i * i );
		float flTheta = atan2f( i, r );
		//if ( flTheta < 0.0f ) flTheta += 2.0f * 3.14159f;

		// Demoivre's theorem for principal root
		return FromPolar( powf( flRadius, 1.0f / 3.0f ), flTheta / 3.0f );
	}
};

// [kutta]
// This code is a work-in-progress; need to write code to robustly find an eigenvector given its eigenvalue.
