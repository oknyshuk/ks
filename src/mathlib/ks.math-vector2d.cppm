// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// The declarations are attached to the global module (extern "C++") so their symbols and forward
// declarations elsewhere are unchanged.

export module ks.math:vector2d;

import std;
import ks.core;
export import :pfns;

export extern "C++" {

// For vec_t, put this somewhere else?

// For RandomFloat()

inline constexpr float M_PI_F = std::numbers::pi_v<float>;

// Inlined so floats are not autopromoted to doubles.
[[gnu::always_inline]] inline constexpr float RAD2DEG( float x ) { return x * ( 180.f / M_PI_F ); }
[[gnu::always_inline]] inline constexpr float DEG2RAD( float x ) { return x * ( M_PI_F / 180.f ); }

[[gnu::always_inline]] inline void SinCos( float radians, float * __restrict__ sine, float * __restrict__ cosine ) { MathSinCos( radians, sine, cosine ); }

//=========================================================
// 2D Vector2D
//=========================================================

class Vector2D					
{
public:
	// Members
	vec_t x, y;

	// Construction/destruction
	Vector2D();
	Vector2D(vec_t X, vec_t Y);
	explicit Vector2D(const float *pFloat);

	// Initialization
	void Init(vec_t ix=0.0f, vec_t iy=0.0f);

	// Got any nasty NAN's?
	bool IsValid() const;

	// array access...
	vec_t operator[](int i) const;
	vec_t& operator[](int i);

	// Base address...
	vec_t* Base();
	vec_t const* Base() const;

	// Initialization methods
	void Random( float minVal, float maxVal );

	// equality
	bool operator==(const Vector2D& v) const;
	bool operator!=(const Vector2D& v) const;	

	// arithmetic operations
	Vector2D&	operator+=(const Vector2D &v);			
	Vector2D&	operator-=(const Vector2D &v);		
	Vector2D&	operator*=(const Vector2D &v);			
	Vector2D&	operator*=(float s);
	Vector2D&	operator/=(const Vector2D &v);		
	Vector2D&	operator/=(float s);					

	// negate the Vector2D components
	void	Negate(); 

	// Get the Vector2D's magnitude.
	vec_t	Length() const;

	// Get the Vector2D's magnitude squared.
	vec_t	LengthSqr(void) const;

	// return true if this vector is (0,0) within tolerance
	bool IsZero( float tolerance = 0.01f ) const
	{
		return (x > -tolerance && x < tolerance &&
				y > -tolerance && y < tolerance);
	}

	// Normalize in place and return the old length.
	vec_t	NormalizeInPlace();

	// Compare length.
	bool	IsLengthGreaterThan( float val ) const;
	bool	IsLengthLessThan( float val ) const;

	// Get the distance from this Vector2D to the other one.
	vec_t	DistTo(const Vector2D &vOther) const;

	// Get the distance from this Vector2D to the other one squared.
	vec_t	DistToSqr(const Vector2D &vOther) const;		

	// Copy
	void	CopyToArray(float* rgfl) const;	

	// Multiply, add, and assign to this (ie: *this = a + b * scalar). This
	// is about 12% faster than the actual Vector2D equation (because it's done per-component
	// rather than per-Vector2D).
	void	MulAdd(const Vector2D& a, const Vector2D& b, float scalar);	

	// Dot product.
	vec_t	Dot(const Vector2D& vOther) const;			

	// assignment
	Vector2D& operator=(const Vector2D &vOther);

	// copy constructors
	Vector2D(const Vector2D &vOther);

	// arithmetic operations
	Vector2D	operator-(void) const;
				
	Vector2D	operator+(const Vector2D& v) const;	
	Vector2D	operator-(const Vector2D& v) const;	
	Vector2D	operator*(const Vector2D& v) const;	
	Vector2D	operator/(const Vector2D& v) const;	
	Vector2D	operator*(float fl) const;
	Vector2D	operator/(float fl) const;			
	
	// Cross product between two vectors.
	Vector2D	Cross(const Vector2D &vOther) const;		

	// Returns a Vector2D with the min or max in X, Y, and Z.
	Vector2D	Min(const Vector2D &vOther) const;
	Vector2D	Max(const Vector2D &vOther) const;

};

//-----------------------------------------------------------------------------

inline const Vector2D vec2_origin(0,0);
inline const Vector2D vec2_invalid( std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );

//-----------------------------------------------------------------------------
// Vector2D related operations
//-----------------------------------------------------------------------------

// Vector2D clear
void Vector2DClear( Vector2D& a );

// Copy
void Vector2DCopy( const Vector2D& src, Vector2D& dst );

// Vector2D arithmetic
void Vector2DAdd( const Vector2D& a, const Vector2D& b, Vector2D& result );
void Vector2DSubtract( const Vector2D& a, const Vector2D& b, Vector2D& result );
void Vector2DMultiply( const Vector2D& a, vec_t b, Vector2D& result );
void Vector2DMultiply( const Vector2D& a, const Vector2D& b, Vector2D& result );
void Vector2DDivide( const Vector2D& a, vec_t b, Vector2D& result );
void Vector2DDivide( const Vector2D& a, const Vector2D& b, Vector2D& result );
void Vector2DMA( const Vector2D& start, float s, const Vector2D& dir, Vector2D& result );

// Store the min or max of each of x, y, and z into the result.
void Vector2DMin( const Vector2D &a, const Vector2D &b, Vector2D &result );
void Vector2DMax( const Vector2D &a, const Vector2D &b, Vector2D &result );

// Normalization
vec_t Vector2DNormalize( Vector2D& v );

// Length
vec_t Vector2DLength( const Vector2D& v );

// Dot Product
vec_t DotProduct2D(const Vector2D& a, const Vector2D& b);

// Linearly interpolate between two vectors
void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, vec_t t, Vector2D& dest );

//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------

inline Vector2D::Vector2D()									
{ 
}

inline Vector2D::Vector2D(vec_t X, vec_t Y)						
{ 
	x = X; y = Y;
	contract_assert( IsValid() );
}

inline Vector2D::Vector2D(const float *pFloat)					
{
	contract_assert( pFloat );
	x = pFloat[0]; y = pFloat[1];	
	contract_assert( IsValid() );
}

//-----------------------------------------------------------------------------
// copy constructor
//-----------------------------------------------------------------------------

inline Vector2D::Vector2D(const Vector2D &vOther)					
{ 
	contract_assert( vOther.IsValid() );
	x = vOther.x; y = vOther.y;
}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

inline void Vector2D::Init( vec_t ix, vec_t iy )    
{ 
	x = ix; y = iy;
	contract_assert( IsValid() );
}

inline void Vector2D::Random( float minVal, float maxVal )
{
	x = RandomFloat( minVal , maxVal );
	y = RandomFloat( minVal , maxVal );
}

inline void Vector2DClear( Vector2D& a )
{
	a.x = a.y = 0.0f;
}

//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------

inline Vector2D& Vector2D::operator=(const Vector2D &vOther)	
{
	contract_assert( vOther.IsValid() );
	x=vOther.x; y=vOther.y;
	return *this; 
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------

inline vec_t& Vector2D::operator[](int i)
{
	contract_assert( (i >= 0) && (i < 2) );
	return ((vec_t*)this)[i];
}

inline vec_t Vector2D::operator[](int i) const
{
	contract_assert( (i >= 0) && (i < 2) );
	return ((vec_t*)this)[i];
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------

inline vec_t* Vector2D::Base()
{
	return (vec_t*)this;
}

inline vec_t const* Vector2D::Base() const
{
	return (vec_t const*)this;
}

//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------

inline bool Vector2D::IsValid() const
{
	return IsFinite(x) && IsFinite(y);
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool Vector2D::operator==( const Vector2D& src ) const
{
	contract_assert( src.IsValid() && IsValid() );
	return (src.x == x) && (src.y == y);
}

inline bool Vector2D::operator!=( const Vector2D& src ) const
{
	contract_assert( src.IsValid() && IsValid() );
	return (src.x != x) || (src.y != y);
}

//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------

inline void Vector2DCopy( const Vector2D& src, Vector2D& dst )
{
	contract_assert( src.IsValid() );
	dst.x = src.x;
	dst.y = src.y;
}

inline void	Vector2D::CopyToArray(float* rgfl) const		
{ 
	contract_assert( IsValid() );
	contract_assert( rgfl );
	rgfl[0] = x; rgfl[1] = y; 
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

inline void Vector2D::Negate()
{ 
	contract_assert( IsValid() );
	x = -x; y = -y;
} 

inline Vector2D& Vector2D::operator+=(const Vector2D& v)	
{ 
	contract_assert( IsValid() && v.IsValid() );
	x+=v.x; y+=v.y;	
	return *this;
}

inline Vector2D& Vector2D::operator-=(const Vector2D& v)	
{ 
	contract_assert( IsValid() && v.IsValid() );
	x-=v.x; y-=v.y;	
	return *this;
}

inline Vector2D& Vector2D::operator*=(float fl)	
{
	x *= fl;
	y *= fl;
	contract_assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator*=(const Vector2D& v)	
{ 
	x *= v.x;
	y *= v.y;
	contract_assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator/=(float fl)	
{
	contract_assert( fl != 0.0f );
	float oofl = 1.0f / fl;
	x *= oofl;
	y *= oofl;
	contract_assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator/=(const Vector2D& v)	
{ 
	contract_assert( v.x != 0.0f && v.y != 0.0f );
	x /= v.x;
	y /= v.y;
	contract_assert( IsValid() );
	return *this;
}

inline void Vector2DAdd( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	contract_assert( a.IsValid() && b.IsValid() );
	c.x = a.x + b.x;
	c.y = a.y + b.y;
}

inline void Vector2DSubtract( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	contract_assert( a.IsValid() && b.IsValid() );
	c.x = a.x - b.x;
	c.y = a.y - b.y;
}

inline void Vector2DMultiply( const Vector2D& a, vec_t b, Vector2D& c )
{
	contract_assert( a.IsValid() && IsFinite(b) );
	c.x = a.x * b;
	c.y = a.y * b;
}

inline void Vector2DMultiply( const Vector2D& a, const Vector2D& b, Vector2D& c )
{				  
	contract_assert( a.IsValid() && b.IsValid() );
	c.x = a.x * b.x;
	c.y = a.y * b.y;
}

inline void Vector2DDivide( const Vector2D& a, vec_t b, Vector2D& c )
{
	contract_assert( a.IsValid() );
	contract_assert( b != 0.0f );
	vec_t oob = 1.0f / b;
	c.x = a.x * oob;
	c.y = a.y * oob;
}

inline void Vector2DDivide( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	contract_assert( a.IsValid() );
	contract_assert( (b.x != 0.0f) && (b.y != 0.0f) );
	c.x = a.x / b.x;
	c.y = a.y / b.y;
}

inline void Vector2DRotate( const Vector2D& vIn, float flDegrees, Vector2D& vOut )
{
	float c, s;
	SinCos( ( (float)( flDegrees ) * (float)( M_PI_F / 180.f ) ), &s, &c );

	vOut.x = vIn.x*c - vIn.y*s;
	vOut.y = vIn.x*s + vIn.y*c;
}

inline void Vector2DMA( const Vector2D& start, float s, const Vector2D& dir, Vector2D& result )
{
	contract_assert( start.IsValid() && IsFinite(s) && dir.IsValid() );
	result.x = start.x + s*dir.x;
	result.y = start.y + s*dir.y;
}

// FIXME: Remove
// For backwards compatability
inline void	Vector2D::MulAdd(const Vector2D& a, const Vector2D& b, float scalar)
{
	x = a.x + b.x * scalar;
	y = a.y + b.y * scalar;
}

inline void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, vec_t t, Vector2D& dest )
{
	dest[0] = src1[0] + (src2[0] - src1[0]) * t;
	dest[1] = src1[1] + (src2[1] - src1[1]) * t;
}

//-----------------------------------------------------------------------------
// dot, cross
//-----------------------------------------------------------------------------
inline vec_t DotProduct2D(const Vector2D& a, const Vector2D& b) 
{ 
	contract_assert( a.IsValid() && b.IsValid() );
	return( a.x*b.x + a.y*b.y ); 
}

// for backwards compatability
inline vec_t Vector2D::Dot( const Vector2D& vOther ) const
{
	return DotProduct2D( *this, vOther );
}

//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------
inline vec_t Vector2DLength( const Vector2D& v )
{
	contract_assert( v.IsValid() );
	return (vec_t)FastSqrt(v.x*v.x + v.y*v.y);		
}

inline vec_t Vector2D::LengthSqr(void) const	
{ 
	contract_assert( IsValid() );
	return (x*x + y*y);		
}

inline vec_t Vector2D::NormalizeInPlace()
{
	return Vector2DNormalize( *this );
}

inline bool Vector2D::IsLengthGreaterThan( float val ) const
{
	return LengthSqr() > val*val;
}

inline bool Vector2D::IsLengthLessThan( float val ) const
{
	return LengthSqr() < val*val;
}

inline vec_t Vector2D::Length(void) const	
{
	return Vector2DLength( *this );
}

inline void Vector2DMin( const Vector2D &a, const Vector2D &b, Vector2D &result )
{
	result.x = (a.x < b.x) ? a.x : b.x;
	result.y = (a.y < b.y) ? a.y : b.y;
}

inline void Vector2DMax( const Vector2D &a, const Vector2D &b, Vector2D &result )
{
	result.x = (a.x > b.x) ? a.x : b.x;
	result.y = (a.y > b.y) ? a.y : b.y;
}

//-----------------------------------------------------------------------------
// Normalization
//-----------------------------------------------------------------------------
inline vec_t Vector2DNormalize( Vector2D& v )
{
	contract_assert( v.IsValid() );
	vec_t l = v.Length();
	if (l != 0.0f)
	{
		v /= l;
	}
	else
	{
		v.x = v.y = 0.0f; 
	}
	return l;
}

//-----------------------------------------------------------------------------
// Get the distance from this Vector2D to the other one 
//-----------------------------------------------------------------------------
inline vec_t Vector2D::DistTo(const Vector2D &vOther) const
{
	Vector2D delta;
	Vector2DSubtract( *this, vOther, delta );
	return delta.Length();
}

inline vec_t Vector2D::DistToSqr(const Vector2D &vOther) const
{
	Vector2D delta;
	Vector2DSubtract( *this, vOther, delta );
	return delta.LengthSqr();
}

//-----------------------------------------------------------------------------
// Computes the closest point to vecTarget no farther than flMaxDist from vecStart
//-----------------------------------------------------------------------------
inline void ComputeClosestPoint2D( const Vector2D& vecStart, float flMaxDist, const Vector2D& vecTarget, Vector2D *pResult )
{
	Vector2D vecDelta;
	Vector2DSubtract( vecTarget, vecStart, vecDelta );
	float flDistSqr = vecDelta.LengthSqr();
	if ( flDistSqr <= flMaxDist * flMaxDist )
	{
		*pResult = vecTarget;
	}
	else
	{
		vecDelta /= FastSqrt( flDistSqr );
		Vector2DMA( vecStart, flMaxDist, vecDelta, *pResult );
	}
}

//-----------------------------------------------------------------------------
// Returns a Vector2D with the min or max in X, Y, and Z.
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::Min(const Vector2D &vOther) const
{
	return Vector2D(x < vOther.x ? x : vOther.x, 
		y < vOther.y ? y : vOther.y);
}

inline Vector2D Vector2D::Max(const Vector2D &vOther) const
{
	return Vector2D(x > vOther.x ? x : vOther.x, 
		y > vOther.y ? y : vOther.y);
}

//-----------------------------------------------------------------------------
// arithmetic operations
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::operator-(void) const
{ 
	return Vector2D(-x,-y);				
}

inline Vector2D Vector2D::operator+(const Vector2D& v) const	
{ 
	Vector2D res;
	Vector2DAdd( *this, v, res );
	return res;	
}

inline Vector2D Vector2D::operator-(const Vector2D& v) const	
{ 
	Vector2D res;
	Vector2DSubtract( *this, v, res );
	return res;	
}

inline Vector2D Vector2D::operator*(float fl) const	
{ 
	Vector2D res;
	Vector2DMultiply( *this, fl, res );
	return res;	
}

inline Vector2D Vector2D::operator*(const Vector2D& v) const	
{ 
	Vector2D res;
	Vector2DMultiply( *this, v, res );
	return res;	
}

inline Vector2D Vector2D::operator/(float fl) const	
{ 
	Vector2D res;
	Vector2DDivide( *this, fl, res );
	return res;	
}

inline Vector2D Vector2D::operator/(const Vector2D& v) const	
{ 
	Vector2D res;
	Vector2DDivide( *this, v, res );
	return res;	
}

inline Vector2D operator*(float fl, const Vector2D& v)	
{ 
	return v * fl; 
}

}
