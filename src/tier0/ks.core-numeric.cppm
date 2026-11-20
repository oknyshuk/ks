// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Scalar float helpers.

export module ks.core.numeric;

import ks.core.types;

export extern "C++" {

[[gnu::always_inline]] inline double fpmin( double a, double b )
{
	return a > b  ? b : a;
}

[[gnu::always_inline]] inline double fpmax( double a, double b )
{
	return a >= b ? a : b;
}

// clamp x to lie inside [a,b]. Assumes b>a
[[gnu::always_inline]] inline float fclamp( float x, float a, float b )
{
	return fpmin( fpmax( x, a ), b );
}
// clamp x to lie inside [a,b]. Assumes b>a
[[gnu::always_inline]] inline double fclamp( double x, double a, double b )
{
	return fpmin( fpmax( x, a ), b );
}

typedef float vec_t;

//lwss these function used long, which is 64bits on linux.
//inline unsigned long& FloatBits( vec_t& f )
inline unsigned int& FloatBits( vec_t& f )
{
	//return *reinterpret_cast<unsigned long*>(&f);
	return *reinterpret_cast<unsigned int*>(&f);
}

//inline unsigned long const& FloatBits( vec_t const& f )
inline unsigned int const& FloatBits( vec_t const& f )
{
	//return *reinterpret_cast<unsigned long const*>(&f);
	return *reinterpret_cast<unsigned int const*>(&f);
}
//lwss end

inline vec_t BitsToFloat( unsigned long i )
{
	return *reinterpret_cast<vec_t*>(&i);
}

inline bool IsFinite( const vec_t &f )
{
	return ((FloatBits(f) & 0x7F800000) != 0x7F800000);
}

inline float FloatMakePositive( vec_t f )
{
	return __builtin_fabsf( f ); // was since 2002: BitsToFloat( FloatBits(f) & 0x7FFFFFFF ); fixed in 2010
}

inline float FloatNegate( vec_t f )
{
	return -f; //BitsToFloat( FloatBits(f) ^ 0x80000000 );
}

inline constexpr uint32 FLOAT32_NAN_BITS = 0x7FC00000;	// not a number!
inline const float FLOAT32_NAN = BitsToFloat( FLOAT32_NAN_BITS );
inline const float VEC_T_NAN = FLOAT32_NAN;

inline float FloatMakeNegative( vec_t f )
{
	return -__builtin_fabsf( f );// was since 2002: BitsToFloat( FloatBits(f) | 0x80000000 ); fixed in 2010
}

}
