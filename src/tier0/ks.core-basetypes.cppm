// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.basetypes;

import std;
import ks.core.types;
import ks.core.numeric;
import ks.core.wchartypes;

export extern "C++" {

// Purpose: The module extension the build targets, e.g. ".so".

inline constexpr char DLL_EXT_STRING[] = ".so";

// character types come from ks.core (:wchartypes)

//-----------------------------------------------------------------------------
// Compile-time string concatenation. Adjacent string literals are a preprocessor
// feature; a consteval function gives the same result with ordinary C++.
//-----------------------------------------------------------------------------
namespace ks
{
	template <std::size_t N>
	struct FixedString
	{
		char m_data[N] {};

		constexpr FixedString() = default;
		consteval FixedString( const char ( &s )[N] ) { for ( std::size_t i = 0; i < N; ++i ) m_data[i] = s[i]; }

		constexpr const char *c_str() const { return m_data; }
		constexpr std::size_t size() const { return N - 1; }
	};

	// Compile-time decimal rendering of a value, replacing preprocessor stringification.
	template <int VALUE>
	consteval FixedString<12> IntToString()
	{
		char buffer[12] {};
		int n = VALUE;
		bool bNegative = n < 0;
		unsigned int magnitude = bNegative ? static_cast<unsigned int>( -n ) : static_cast<unsigned int>( n );
		int i = 11;
		do { buffer[--i] = static_cast<char>( '0' + ( magnitude % 10 ) ); magnitude /= 10; } while ( magnitude );
		if ( bNegative ) buffer[--i] = '-';
		FixedString<12> result{};
		int out = 0;
		while ( i < 12 ) result.m_data[out++] = buffer[i++];
		return result;
	}

	template <std::size_t A, std::size_t B>
	consteval FixedString<A + B - 1> Concat( const char ( &a )[A], const char ( &b )[B] )
	{
		FixedString<A + B - 1> result{};
		for ( std::size_t i = 0; i < A - 1; ++i )
			result.m_data[i] = a[i];
		for ( std::size_t i = 0; i < B; ++i )
			result.m_data[A - 1 + i] = b[i];
		return result;
	}
}

template<class T>
T abs( const T &a )
{
	if ( a < 0 )
		return -a;
	else
		return a;
}

template <typename F>
inline void ExecuteNTimes( int nTimes, F &&fn )
{
	static int executeCount = 0;
	if ( executeCount < nTimes )
	{
		++executeCount;
		fn();
	}
}

template <typename F>
inline void ExecuteOnce( F &&fn )
{
	ExecuteNTimes( 1, static_cast<F &&>( fn ) );
}

// Pad a number so it lies on an N byte boundary.
// So PAD_NUMBER(0,4) is 0 and PAD_NUMBER(1,4) is 4
template <typename N, typename B>
inline constexpr N PAD_NUMBER( N number, B boundary ) { return ( ( number + boundary - 1 ) / boundary ) * boundary; }

template <typename T>
inline constexpr T COMPILETIME_MAX( T a, T b ) { return ( a > b ) ? a : b; }

template< class T, class Y, class X >
inline T clamp( T const &val, Y const &minVal, X const &maxVal )
{
	if( val < minVal )
		return minVal;
	else if( val > maxVal )
		return maxVal;
	else
		return val;
}

// This is the preferred clamp operator. Using the clamp macro can lead to
// unexpected side-effects or more expensive code. Even the clamp (all
// lower-case) function can generate more expensive code because of the
// mixed types involved.
template< class T >
T Clamp( T const &val, T const &minVal, T const &maxVal )
{
	if( val < minVal )
		return minVal;
	else if( val > maxVal )
		return maxVal;
	else
		return val;
}

// This is the preferred Min operator. Using the MIN macro can lead to unexpected
// side-effects or more expensive code.
template< class T >
T Min( T const &val1, T const &val2 )
{
	return val1 < val2 ? val1 : val2;
}

// This is the preferred Max operator. Using the MAX macro can lead to unexpected
// side-effects or more expensive code.
template< class T >
T Max( T const &val1, T const &val2 )
{
	return val1 > val2 ? val1 : val2;
}

template <typename T>
void Swap( T &a, T &b )
{
	T temp = std::move( a );
	a = std::move( b );
	b = std::move( temp );
}

//-----------------------------------------------------------------------------
// Conditional selects. These were macros; as templates the operands are bound to types, so a
// mismatched pair is a compile error instead of a substitution that happens to work.
//-----------------------------------------------------------------------------

template <typename C, typename X, typename Y>
inline constexpr auto fsel( C c, X x, Y y ) { return c >= 0 ? x : y; }

// integer conditional move: if a >= 0, return x, else y
template <typename A, typename X, typename Y>
inline constexpr auto isel( A a, X x, Y y ) { return a >= 0 ? x : y; }

// if x == y, return a, else b
template <typename X, typename Y, typename A, typename B>
inline constexpr auto ieqsel( X x, Y y, A a, B b ) { return x == y ? a : b; }

// if the nth bit of a is set (counting with 0 = LSB), return x, else y
template <typename A, typename N, typename X, typename Y>
inline constexpr auto ibitsel( A a, N nbit, X x, Y y ) { return ( ( a & ( A( 1 ) << nbit ) ) != 0 ) ? x : y; }

// At some point, we will need a unified API.
template <typename T>
inline constexpr auto imin( T x, T y ) { return x < y ? x : y; }
template <typename T>
inline constexpr auto imax( T x, T y ) { return x > y ? x : y; }

// Type names shared by the platform, math and interface headers.
using BOOL = int;
using BYTE = uint8;
using byte = uint8;
using word = uint16;
using qboolean = int;

typedef unsigned short ucs2;

enum ThreeState_t
{
	TRS_FALSE,
	TRS_TRUE,
	TRS_NONE,
};

template <typename T>
inline T AlignValue( T val, uintp alignment )
{
	return (T)( ( (uintp)val + alignment - 1 ) & ~( alignment - 1 ) );
}

//-----------------------------------------------------------------------------
// look for NANs, infinities, and underflows.
// This assumes the ANSI/IEEE 754-1985 standard
//-----------------------------------------------------------------------------

// FIXME: why are these here?  Hardly anyone actually needs them.
struct color24
{
	byte r, g, b;
};

typedef struct color32_s
{
	bool operator!=( const struct color32_s &other ) const;
	byte r, g, b, a;

	// assign and copy by using the whole register rather
	// than byte-by-byte copy. (No, the compiler is not
	// smart enough to do this for you. /FAcs if you
	// don't believe me.)
	inline unsigned *asInt(void) { return reinterpret_cast<unsigned*>(this); }
	inline const unsigned *asInt(void) const { return reinterpret_cast<const unsigned*>(this); }
	// This thing is in a union elsewhere, and union members can't have assignment
	// operators, so you have to explicitly assign using this, or be slow. SUCK.
	inline void Copy(const color32_s &rhs)
	{
		*asInt() = *rhs.asInt();
	}

} color32;

inline void EnsureValidValue( color32_s &x ) { x.r = x.g = x.b = x.a = 0; }

inline bool color32::operator!=( const color32 &other ) const
{
	return r != other.r || g != other.g || b != other.b || a != other.a;
}

struct colorVec
{
	unsigned r, g, b, a;
};

struct vrect_t
{
	int				x,y,width,height;
	vrect_t			*pnext;
};

//-----------------------------------------------------------------------------
// MaterialRect_t struct - used for DrawDebugText
//-----------------------------------------------------------------------------
struct Rect_t
{
	int x, y;
	int width, height;
};

struct Rect3D_t
{
	int x, y, z;
	int width, height, depth;

	[[gnu::always_inline]] inline Rect3D_t( int nX, int nY, int nZ, int nWidth, int nHeight, int nDepth )
	{
		x = nX;
		y = nY;
		z = nZ;
		width = nWidth;
		height = nHeight;
		depth = nDepth;
	}

	[[gnu::always_inline]] inline Rect3D_t( void )
	{
	}

};

//-----------------------------------------------------------------------------
// Interval, used by soundemittersystem + the game
//-----------------------------------------------------------------------------
struct interval_t
{
	float start;
	float range;
};

//-----------------------------------------------------------------------------
// Declares a type-safe handle type; you can't assign one handle to the next
//-----------------------------------------------------------------------------

// 32-bit pointer handles.

// Typesafe 16-bit and 32-bit handles.
template< class HandleType >
class CBaseIntHandle
{
public:

	inline bool			operator==( const CBaseIntHandle &other )	{ return m_Handle == other.m_Handle; }
	inline bool			operator!=( const CBaseIntHandle &other )	{ return m_Handle != other.m_Handle; }

	// Only the code that doles out these handles should use these functions.
	// Everyone else should treat them as a transparent type.
	inline HandleType	GetHandleValue()					{ return m_Handle; }
	inline void			SetHandleValue( HandleType val )	{ m_Handle = val; }

	typedef HandleType	HANDLE_TYPE;

protected:

	HandleType	m_Handle;
};

template< class DummyType >
class CIntHandle16 : public CBaseIntHandle< unsigned short >
{
public:
	inline			CIntHandle16() {}

	static inline	CIntHandle16<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle16<DummyType>( val );
	}

protected:
	inline			CIntHandle16( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};


template< class DummyType >
class CIntHandle32 : public CBaseIntHandle< uint32 >
{
public:
	inline			CIntHandle32() {}

	static inline	CIntHandle32<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle32<DummyType>( val );
	}

protected:
	inline			CIntHandle32( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};

template <typename T> struct EnableBitwiseOps : std::false_type {};

template <typename T>
concept BitwiseEnum = std::is_enum_v<T> && EnableBitwiseOps<T>::value;

template <BitwiseEnum T> constexpr T operator|( T a, T b ) { return T( int( a ) | int( b ) ); }
template <BitwiseEnum T> constexpr T operator&( T a, T b ) { return T( int( a ) & int( b ) ); }
template <BitwiseEnum T> constexpr T operator^( T a, T b ) { return T( int( a ) ^ int( b ) ); }
template <BitwiseEnum T> constexpr T operator<<( T a, int b ) { return T( int( a ) << b ); }
template <BitwiseEnum T> constexpr T operator>>( T a, int b ) { return T( int( a ) >> b ); }
template <BitwiseEnum T> constexpr T &operator|=( T &a, T b ) { return a = a | b; }
template <BitwiseEnum T> constexpr T &operator&=( T &a, T b ) { return a = a & b; }
template <BitwiseEnum T> constexpr T &operator^=( T &a, T b ) { return a = a ^ b; }
template <BitwiseEnum T> constexpr T &operator<<=( T &a, int b ) { return a = a << b; }
template <BitwiseEnum T> constexpr T &operator>>=( T &a, int b ) { return a = a >> b; }
template <BitwiseEnum T> constexpr T operator~( T a ) { return T( ~int( a ) ); }

// defines increment/decrement operators for enums for easy iteration

// Max 2 player splitscreen in portal (don't merge this back), saves a bunch of memory [8/31/2010 tom]
inline constexpr auto MAX_SPLITSCREEN_CLIENT_BITS = 1;
// this should == MAX_JOYSTICKS in InputEnums.h
inline constexpr int MAX_SPLITSCREEN_CLIENTS = 1 << MAX_SPLITSCREEN_CLIENT_BITS;

// Makes a 4-byte "packed ID" int out of 4 characters
inline constexpr int MAKEID( int d, int c, int b, int a ) { return ( ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | d ); }

// Bit-flag helpers. These were the SETBITS/CLEARBITS/FBitSet macros. As functions the flag word is
// bound to a type, so using them on something that is not an integer flags word is a compile error
// instead of a substitution that happens to work.
template <typename T, typename B>
inline constexpr void SETBITS( T &nFlags, B bits ) { nFlags |= static_cast<T>( bits ); }

template <typename T, typename B>
inline constexpr void CLEARBITS( T &nFlags, B bits ) { nFlags &= static_cast<T>( ~static_cast<T>( bits ) ); }

template <typename T, typename B>
inline constexpr auto FBitSet( const T &nFlags, B bits ) { return nFlags & bits; }

// Compares a string with a 4-byte packed ID constant

template <typename T>
inline bool IsPowerOfTwo( T value )
{
	return (value & ( value - (T)1 )) == (T)0;
}

// Using ARRAYSIZE implementation from winnt.h:

// Return the number of elements in a statically sized array.
//   DWORD Buffer[100];
//   RTL_NUMBER_OF(Buffer) == 100
// This is also popularly known as: NUMBER_OF, ARRSIZE, _countof, NELEM, etc.
//

// ARRAYSIZE is more readable version of RTL_NUMBER_OF_V2
// _ARRAYSIZE is a version useful for anonymous types

template< typename IndexType, typename T, unsigned int N >
IndexType ClampedArrayIndex( const T (&buffer)[N], IndexType index )
{
	(void)buffer;
	return clamp( index, 0, (IndexType)N - 1 );
}

template< typename T, unsigned int N >
T ClampedArrayElement( const T (&buffer)[N], unsigned int uIndex )
{
	// Put index in an unsigned type to halve the clamping.
	if ( uIndex >= N )
		uIndex = N - 1;
	return buffer[ uIndex ];
}

}
