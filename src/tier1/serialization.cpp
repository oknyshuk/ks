//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// $Header: $
// $NoKeywords: $
//
// Serialization
//===========================================================================//


#include "tier1/serialization.h"
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector4d.h"
#include "mathlib/vmatrix.h"
#include "color.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include "tier1/utlstring.h"
#include "tier1/strtools.h"
#include "tier1/characterset.h"


//-----------------------------------------------------------------------------
// For serialization, set the delimiter rules
//-----------------------------------------------------------------------------
const ks::string_escapes *s_pConv = nullptr;
const char *s_pUtlBufferUtilArrayDelim = nullptr;
void SetSerializationDelimiter( const ks::string_escapes *pEscapes )
{
	s_pConv = pEscapes;
}

void SetSerializationArrayDelimiter( const char *pDelimiter )
{
	s_pUtlBufferUtilArrayDelim = pDelimiter;
}


//-----------------------------------------------------------------------------
// Serialize a floating point number in text mode in a readably friendly fashion
//-----------------------------------------------------------------------------
static void SerializeFloat( ks::text_writer &buf, float f )
{
	// FIXME: Print this in a way that we never lose precision
	char pTemp[256];
	int nLen = Q_snprintf( pTemp, sizeof(pTemp), "%.10f", f );
	while ( nLen > 0 && pTemp[nLen-1] == '0' )
	{
		--nLen;
		pTemp[nLen] = 0;
	}
	if ( nLen > 0 && pTemp[nLen-1] == '.' )
	{
		--nLen;
		pTemp[nLen] = 0;
	}
	buf.put( pTemp );
}

static void SerializeFloats( ks::text_writer &buf, int nCount, const float *pFloats )
{
	for ( int i = 0; i < nCount; ++i )
	{
		SerializeFloat( buf, pFloats[i] );
		if ( i != nCount-1 )
		{
			buf.put( ' ' );
		}
	}
}


//-----------------------------------------------------------------------------
// Serialization methods for basic types
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const bool &src )
{
	buf.write< char >( src );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const bool &src )
{
	buf.printf( "%d", src );
	return true;
}

bool Unserialize( ks::byte_reader &buf, bool &dest )
{
	dest = ( buf.read< char >() != 0 );
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, bool &dest )
{
	int nValue = 0;
	int nRetVal = buf.scanf( "%d", &nValue );
	dest = ( nValue != 0 );
	return (nRetVal == 1) && buf.ok();
}


bool Serialize( ks::byte_writer &buf, const int &src )
{
	buf.write< int >( src );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const int &src )
{
	buf.printf( "%d", src );
	return true;
}

bool Unserialize( ks::byte_reader &buf, int &dest )
{
	dest = buf.read< int >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, int &dest )
{
	int nRetVal = buf.scanf( "%d", &dest );
	return (nRetVal == 1) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const float &src )
{
	buf.write< float >( src );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const float &src )
{
	SerializeFloat( buf, src );
	return true;
}

bool Unserialize( ks::byte_reader &buf, float &dest )
{
	dest = buf.read< float >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, float &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f", &dest );
	return (nRetVal == 1) && buf.ok();
}


//-----------------------------------------------------------------------------
// Attribute types related to vector math
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const Vector2D &src )
{
	buf.write< float >( src.x );
	buf.write< float >( src.y );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const Vector2D &src )
{
	SerializeFloats( buf, 2, src.Base() );
	return true;
}

bool Unserialize( ks::byte_reader &buf, Vector2D &dest )
{
	dest.x = buf.read< float >();
	dest.y = buf.read< float >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, Vector2D &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f %f", &dest.x, &dest.y );
	return (nRetVal == 2) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const Vector &src )
{
	buf.write< float >( src.x );
	buf.write< float >( src.y );
	buf.write< float >( src.z );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const Vector &src )
{
	SerializeFloats( buf, 3, src.Base() );
	return true;
}

bool Unserialize( ks::byte_reader &buf, Vector &dest )
{
	dest.x = buf.read< float >();
	dest.y = buf.read< float >();
	dest.z = buf.read< float >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, Vector &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f %f %f", &dest.x, &dest.y, &dest.z );
	return (nRetVal == 3) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const Vector4D &src )
{
	buf.write< float >( src.x );
	buf.write< float >( src.y );
	buf.write< float >( src.z );
	buf.write< float >( src.w );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const Vector4D &src )
{
	SerializeFloats( buf, 4, src.Base() );
	return true;
}

bool Unserialize( ks::byte_reader &buf, Vector4D &dest )
{
	dest.x = buf.read< float >();
	dest.y = buf.read< float >();
	dest.z = buf.read< float >();
	dest.w = buf.read< float >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, Vector4D &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f %f %f %f", &dest.x, &dest.y, &dest.z, &dest.w );
	return (nRetVal == 4) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const QAngle &src )
{
	buf.write< float >( src.x );
	buf.write< float >( src.y );
	buf.write< float >( src.z );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const QAngle &src )
{
	SerializeFloats( buf, 3, src.Base() );
	return true;
}

bool Unserialize( ks::byte_reader &buf, QAngle &dest )
{
	dest.x = buf.read< float >();
	dest.y = buf.read< float >();
	dest.z = buf.read< float >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, QAngle &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f %f %f", &dest.x, &dest.y, &dest.z );
	return (nRetVal == 3) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const Quaternion &src )
{
	buf.write< float >( src.x );
	buf.write< float >( src.y );
	buf.write< float >( src.z );
	buf.write< float >( src.w );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const Quaternion &src )
{
	SerializeFloats( buf, 4, &src.x );
	return true;
}

bool Unserialize( ks::byte_reader &buf, Quaternion &dest )
{
	dest.x = buf.read< float >();
	dest.y = buf.read< float >();
	dest.z = buf.read< float >();
	dest.w = buf.read< float >();
	QuaternionNormalize( dest );
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, Quaternion &dest )
{
	// FIXME: Print this in a way that we never lose precision
	int nRetVal = buf.scanf( "%f %f %f %f", &dest.x, &dest.y, &dest.z, &dest.w );
	QuaternionNormalize( dest );
	return (nRetVal == 4) && buf.ok();
}

bool Serialize( ks::byte_writer &buf, const VMatrix &src )
{
	buf.write_bytes( &src, sizeof(VMatrix) );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const VMatrix &src )
{
	buf.put( '\n' );
	SerializeFloats( buf, 4, src[0] );
	buf.put( '\n' );
	SerializeFloats( buf, 4, src[1] );
	buf.put( '\n' );
	SerializeFloats( buf, 4, src[2] );
	buf.put( '\n' );
	SerializeFloats( buf, 4, src[3] );
	buf.put( '\n' );
	return true;
}

bool Unserialize( ks::byte_reader &buf, VMatrix &dest )
{
	if ( !buf.ok() )
		return false;

	return buf.read_bytes( &dest, sizeof(VMatrix) );
}

bool Unserialize( ks::text_reader &buf, VMatrix &dest )
{
	if ( !buf.ok() )
		return false;

	int nRetVal = buf.scanf( "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
		&dest[ 0 ][ 0 ], &dest[ 0 ][ 1 ], &dest[ 0 ][ 2 ], &dest[ 0 ][ 3 ],
		&dest[ 1 ][ 0 ], &dest[ 1 ][ 1 ], &dest[ 1 ][ 2 ], &dest[ 1 ][ 3 ],
		&dest[ 2 ][ 0 ], &dest[ 2 ][ 1 ], &dest[ 2 ][ 2 ], &dest[ 2 ][ 3 ],
		&dest[ 3 ][ 0 ], &dest[ 3 ][ 1 ], &dest[ 3 ][ 2 ], &dest[ 3 ][ 3 ] );
	return (nRetVal == 16);
}


//-----------------------------------------------------------------------------
// Color attribute
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const Color &src )
{
	buf.write< unsigned char >( src[0] );
	buf.write< unsigned char >( src[1] );
	buf.write< unsigned char >( src[2] );
	buf.write< unsigned char >( src[3] );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const Color &src )
{
	buf.printf( "%d %d %d %d", src[0], src[1], src[2], src[3] );
	return true;
}

bool Unserialize( ks::byte_reader &buf, Color &dest )
{
	dest[0] = buf.read< unsigned char >();
	dest[1] = buf.read< unsigned char >();
	dest[2] = buf.read< unsigned char >();
	dest[3] = buf.read< unsigned char >();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, Color &dest )
{
	int r = 0, g = 0, b = 0, a = 255;
	int nRetVal = buf.scanf( "%d %d %d %d", &r, &g, &b, &a );
	dest.SetColor( r, g, b, a );
	return (nRetVal == 4) && buf.ok();
}


//-----------------------------------------------------------------------------
// Binary buffer attribute
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const std::vector< unsigned char > &src )
{
	int nLength = (int)src.size();
	buf.write< int >( nLength );
	if ( nLength != 0 )
	{
		buf.write_bytes( src.data(), nLength );
	}
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const std::vector< unsigned char > &src )
{
	int nLength = (int)src.size();

	// Writes out uuencoded binaries
	for ( int i = 0; i < nLength; ++i )
	{
		if ( (i % 40) == 0 )
		{
			buf.put( '\n' );
		}

		char b1 = src[i] & 0xF;
		char b2 = src[i] >> 4;

		char c1 = ( b1 <= 9 ) ? b1 + '0' : b1 - 10 + 'A';
		char c2 = ( b2 <= 9 ) ? b2 + '0' : b2 - 10 + 'A';

		buf.put( c2 );
		buf.put( c1 );
	}

	buf.put( '\n' );
	return true;
}

static int CountBinaryBytes( ks::text_reader &buf, int *pEndGet )
{
	// This counts the number of bytes in the uuencoded text
	size_t nStartGet = buf.tell();
	buf.skip_whitespace();
	*pEndGet = (int)buf.tell();
	int nByteCount = 0;
	while ( buf.ok() && !buf.at_end() )
	{
		char c1 = buf.get_char();
		char c2 = buf.get_char();

		bool bIsNum1 = ( c1 >= '0' ) && ( c1 <= '9' );
		bool bIsNum2 = ( c2 >= '0' ) && ( c2 <= '9' );

		bool bIsAlpha1 = (( c1 >= 'A' ) && ( c1 <= 'F' )) || (( c1 >= 'a' ) && ( c1 <= 'f' ));
		bool bIsAlpha2 = (( c2 >= 'A' ) && ( c2 <= 'F' )) || (( c2 >= 'a' ) && ( c2 <= 'f' ));

		if ( !(bIsNum1 || bIsAlpha1) || !(bIsNum2 || bIsAlpha2) )
			break;

		buf.skip_whitespace();
		*pEndGet = (int)buf.tell();
		++nByteCount;
	}
	buf.seek( nStartGet );
	return nByteCount;
}

inline static unsigned char HexCharToInt( int c1 )
{
	if (( c1 >= '0' ) && ( c1 <= '9' ))
		return c1 - '0';

	if (( c1 >= 'A' ) && ( c1 <= 'F' ))
		return 10 + c1 - 'A';

	if (( c1 >= 'a' ) && ( c1 <= 'f' ))
		return 10 + c1 - 'a';

	return 0xFF;
}

bool Unserialize( ks::byte_reader &buf, std::vector< unsigned char > &dest )
{
	int nLen = buf.read< int >();
	if ( nLen < 0 )
		return false;
	if ( (size_t)nLen > buf.remaining() )
	{
		buf.fail();
		return false;
	}
	dest.resize( nLen );
	if ( nLen != 0 )
		buf.read_bytes( dest.data(), nLen );
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, std::vector< unsigned char > &dest )
{
	int nEndGet = 0;
	int nByteCount = CountBinaryBytes( buf, &nEndGet );
	if ( nByteCount < 0 )
		return false;

	buf.skip_whitespace();
	int nDest = 0;
	dest.resize( nByteCount );
	while( (int)buf.tell() < nEndGet )
	{
		char c1 = buf.get_char();
		char c2 = buf.get_char();

		unsigned char b1 = HexCharToInt( c1 );
		unsigned char b2 = HexCharToInt( c2 );
		if ( b1 == 0xFF || b2 == 0xFF )
			return false;

		dest[ nDest++ ] = b2 | ( b1 << 4 );
		buf.skip_whitespace();
	}

	return true;
}


//-----------------------------------------------------------------------------
// String attribute
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const std::string &src )
{
	buf.write_cstring( src.c_str() );
	return buf.ok();
}

bool Serialize( ks::text_writer &buf, const std::string &src )
{
	if ( s_pConv )
		buf.put_quoted( *s_pConv, src.c_str() );
	else
		buf.put( src.c_str() );
	return true;
}

bool Unserialize( ks::byte_reader &buf, std::string &dest )
{
	dest = buf.read_cstring();
	return buf.ok();
}

bool Unserialize( ks::text_reader &buf, std::string &dest )
{
	if ( !s_pConv )
	{
		dest = buf.read_word();
		return buf.ok();
	}

	if ( buf.peek_quoted_length( *s_pConv ) == 0 )
		return false;
	dest = buf.read_quoted( *s_pConv );
	return buf.ok();
}
