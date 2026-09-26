//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _MATH_PFNS_H_
#define _MATH_PFNS_H_

#include "tier0/platform.h"
#include <limits>

// YUP_ACTIVE is from Source2. It's (obviously) not supported on this branch, just including it here to help merge camera.cpp/.h and the CSM shadow code.
//#define YUP_ACTIVE 1

enum MatrixAxisType_t
{
#ifdef YUP_ACTIVE
	FORWARD_AXIS = 2,
	LEFT_AXIS = 0,
	UP_AXIS = 1,
#else
	FORWARD_AXIS = 0,
	LEFT_AXIS = 1,
	UP_AXIS = 2,
#endif

	X_AXIS = 0,
	Y_AXIS = 1,
	Z_AXIS = 2,
	ORIGIN = 3,
	PROJECTIVE = 3,
};


// If we are not PPC based or SPU based, then assumes it is SSE2. We should make this code cleaner.

#include <xmmintrin.h>


// These globals are initialized by mathlib and redirected based on available fpu features

// The following are not declared as macros because they are often used in limiting situations,
// and sometimes the compiler simply refuses to inline them for some reason
FORCEINLINE float VECTORCALL FastSqrt( float x )
{
	__m128 root = _mm_sqrt_ss( _mm_load_ss( &x ) );
	return *( reinterpret_cast<float *>( &root ) );
}

FORCEINLINE float VECTORCALL FastRSqrtFast( float x )
{
	// use intrinsics
	__m128 rroot = _mm_rsqrt_ss( _mm_load_ss( &x ) );
	return *( reinterpret_cast<float *>( &rroot ) );
}
// Single iteration NewtonRaphson reciprocal square root:
// 0.5 * rsqrtps * (3 - x * rsqrtps(x) * rsqrtps(x)) 	
// Very low error, and fine to use in place of 1.f / sqrtf(x).	
FORCEINLINE float VECTORCALL FastRSqrt( float x )
{
	float rroot = FastRSqrtFast( x );
	return (0.5f * rroot) * (3.f - (x * rroot) * rroot);
}

// DirectXMath's scalar sin/cos, ported bit-exact so this widely included header doesn't pull it in.
namespace math_scalar
{
	constexpr float kPi       = 3.141592654f;
	constexpr float kTwoPi    = 6.283185307f;
	constexpr float kInvTwoPi = 0.159154943f;
	constexpr float kPiOver2  = 1.570796327f;

	// Fold into [-pi/2,pi/2]; sign is cos()'s (sin(pi - y) == sin(y)).
	FORCEINLINE float Reduce( float value, float &sign )
	{
		float quotient = kInvTwoPi * value;
		if ( value >= 0.0f )
			quotient = static_cast< float >( static_cast< int >( quotient + 0.5f ) );
		else
			quotient = static_cast< float >( static_cast< int >( quotient - 0.5f ) );

		float y = value - kTwoPi * quotient;

		if ( y > kPiOver2 )
		{
			y = kPi - y;
			sign = -1.0f;
		}
		else if ( y < -kPiOver2 )
		{
			y = -kPi - y;
			sign = -1.0f;
		}
		else
		{
			sign = 1.0f;
		}
		return y;
	}
}

// 7-degree sin, 6-degree cos.
FORCEINLINE void FastSinCos( float x, float* s, float* c )
{
	float sign;
	float y  = math_scalar::Reduce( x, sign );
	float y2 = y * y;

	*s = ( ( ( -0.00018524670f * y2 + 0.0083139502f ) * y2 - 0.16665852f ) * y2 + 1.0f ) * y;
	float p = ( ( -0.0012712436f * y2 + 0.041493919f ) * y2 - 0.49992746f ) * y2 + 1.0f;
	*c = sign * p;
}

// 6-degree cos.
FORCEINLINE float FastCos( float x )
{
	float sign;
	float y  = math_scalar::Reduce( x, sign );
	float y2 = y * y;

	float p = ( ( -0.0012712436f * y2 + 0.041493919f ) * y2 - 0.49992746f ) * y2 + 1.0f;
	return sign * p;
}

// 11-degree sin, 10-degree cos (SinCos).
FORCEINLINE void MathSinCos( float x, float* s, float* c )
{
	float sign;
	float y  = math_scalar::Reduce( x, sign );
	float y2 = y * y;

	*s = ( ( ( ( ( -2.3889859e-08f * y2 + 2.7525562e-06f ) * y2 - 0.00019840874f ) * y2
	       + 0.0083333310f ) * y2 - 0.16666667f ) * y2 + 1.0f ) * y;
	float p = ( ( ( ( -2.6051615e-07f * y2 + 2.4760495e-05f ) * y2 - 0.0013888378f ) * y2
	       + 0.041666638f ) * y2 - 0.5f ) * y2 + 1.0f;
	*c = sign * p;
}



inline float FastRecip(float x) {return 1.0f / x;}
// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places 
// or so, so ok for closed transforms.  (ie, computing lighting normals)
inline float FastSqrtEst(float x) { return FastRSqrtFast(x) * x; }



#endif // _MATH_PFNS_H_
