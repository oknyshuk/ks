// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.serialization;

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
export import ks.tier1.utlvector;
export import ks.tier1.byte_writer;
export import ks.tier1.text_reader;
export import ks.tier1.text_writer;

export extern "C++" {

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class Vector2D;
class Vector;
class Vector4D;
class QAngle;
class Quaternion;
class VMatrix;
class Color;

//-----------------------------------------------------------------------------
// For string serialization, set the delimiter rules (null means undelimited)
//-----------------------------------------------------------------------------
void SetSerializationDelimiter( const ks::string_escapes *pEscapes );
void SetSerializationArrayDelimiter( const char *pDelimiter );

//-----------------------------------------------------------------------------
// Standard serialization methods for basic types
//-----------------------------------------------------------------------------
bool Serialize( ks::byte_writer &buf, const bool &src );
bool Unserialize( ks::byte_reader &buf, bool &dest );
bool Serialize( ks::text_writer &buf, const bool &src );
bool Unserialize( ks::text_reader &buf, bool &dest );

bool Serialize( ks::byte_writer &buf, const int &src );
bool Unserialize( ks::byte_reader &buf, int &dest );
bool Serialize( ks::text_writer &buf, const int &src );
bool Unserialize( ks::text_reader &buf, int &dest );

bool Serialize( ks::byte_writer &buf, const float &src );
bool Unserialize( ks::byte_reader &buf, float &dest );
bool Serialize( ks::text_writer &buf, const float &src );
bool Unserialize( ks::text_reader &buf, float &dest );

bool Serialize( ks::byte_writer &buf, const Vector2D &src );
bool Unserialize( ks::byte_reader &buf, Vector2D &dest );
bool Serialize( ks::text_writer &buf, const Vector2D &src );
bool Unserialize( ks::text_reader &buf, Vector2D &dest );

bool Serialize( ks::byte_writer &buf, const Vector &src );
bool Unserialize( ks::byte_reader &buf, Vector &dest );
bool Serialize( ks::text_writer &buf, const Vector &src );
bool Unserialize( ks::text_reader &buf, Vector &dest );

bool Serialize( ks::byte_writer &buf, const Vector4D &src );
bool Unserialize( ks::byte_reader &buf, Vector4D &dest );
bool Serialize( ks::text_writer &buf, const Vector4D &src );
bool Unserialize( ks::text_reader &buf, Vector4D &dest );

bool Serialize( ks::byte_writer &buf, const QAngle &src );
bool Unserialize( ks::byte_reader &buf, QAngle &dest );
bool Serialize( ks::text_writer &buf, const QAngle &src );
bool Unserialize( ks::text_reader &buf, QAngle &dest );

bool Serialize( ks::byte_writer &buf, const Quaternion &src );
bool Unserialize( ks::byte_reader &buf, Quaternion &dest );
bool Serialize( ks::text_writer &buf, const Quaternion &src );
bool Unserialize( ks::text_reader &buf, Quaternion &dest );

bool Serialize( ks::byte_writer &buf, const VMatrix &src );
bool Unserialize( ks::byte_reader &buf, VMatrix &dest );
bool Serialize( ks::text_writer &buf, const VMatrix &src );
bool Unserialize( ks::text_reader &buf, VMatrix &dest );

bool Serialize( ks::byte_writer &buf, const Color &src );
bool Unserialize( ks::byte_reader &buf, Color &dest );
bool Serialize( ks::text_writer &buf, const Color &src );
bool Unserialize( ks::text_reader &buf, Color &dest );

bool Serialize( ks::byte_writer &buf, const std::vector< unsigned char > &src );
bool Unserialize( ks::byte_reader &buf, std::vector< unsigned char > &dest );
bool Serialize( ks::text_writer &buf, const std::vector< unsigned char > &src );
bool Unserialize( ks::text_reader &buf, std::vector< unsigned char > &dest );

bool Serialize( ks::byte_writer &buf, const std::string &src );
bool Unserialize( ks::byte_reader &buf, std::string &dest );
bool Serialize( ks::text_writer &buf, const std::string &src );
bool Unserialize( ks::text_reader &buf, std::string &dest );

//-----------------------------------------------------------------------------
// You can use this to check if a type serializes on multiple lines
//-----------------------------------------------------------------------------
template< class T >
inline bool SerializesOnMultipleLines()
{
	return false;
}

template< >
inline bool SerializesOnMultipleLines<VMatrix>()
{
	return true;
}

template< >
inline bool SerializesOnMultipleLines<std::vector< unsigned char >>()
{
	return true;
}

//-----------------------------------------------------------------------------
// Vector serialization.
//-----------------------------------------------------------------------------
namespace ks::detail
{
	template < class V > struct serial_vector { static constexpr bool value = false; };
	template < class T, class A > struct serial_vector< std::vector< T, A > > { static constexpr bool value = true; using elem = T; };
	template < class T, class A > struct serial_vector< std::deque< T, A > > { static constexpr bool value = true; using elem = T; };
}

template< class V > requires ks::detail::serial_vector< V >::value
bool Serialize( ks::byte_writer &buf, const V &src )
{
	int nCount = std::ssize( src );

	buf.write< int >( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		Serialize( buf, src[i] );
	}
	return buf.ok();
}

template< class V > requires ks::detail::serial_vector< V >::value
bool Serialize( ks::text_writer &buf, const V &src )
{
	using T = typename ks::detail::serial_vector< V >::elem;
	extern const char *s_pUtlBufferUtilArrayDelim;

	int nCount = std::ssize( src );

	if ( !SerializesOnMultipleLines<T>() )
	{
		buf.put( '\n' );
		for ( int i = 0; i < nCount; ++i )
		{
			Serialize( buf, src[i] );
			if ( s_pUtlBufferUtilArrayDelim && (i != nCount-1) )
			{
				buf.put( s_pUtlBufferUtilArrayDelim );
			}
			buf.put( '\n' );
		}
	}
	else
	{
		for ( int i = 0; i < nCount; ++i )
		{
			Serialize( buf, src[i] );
			if ( s_pUtlBufferUtilArrayDelim && (i != nCount-1) )
			{
				buf.put( s_pUtlBufferUtilArrayDelim );
			}
			buf.put( ' ' );
		}
	}

	return true;
}

template< class V > requires ks::detail::serial_vector< V >::value
bool Unserialize( ks::byte_reader &buf, V &dest )
{
	dest.clear();

	int nCount = buf.read< int >();
	if ( nCount )
	{
		if constexpr ( requires { dest.reserve( nCount ); } )
			dest.reserve( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			dest.emplace_back();
			if ( !Unserialize( buf, dest[i] ) )
				return false;
		}
	}
	return buf.ok();
}

template< class V > requires ks::detail::serial_vector< V >::value
bool Unserialize( ks::text_reader &buf, V &dest )
{
	dest.clear();

	while ( true )
	{
		buf.skip_whitespace();
		if ( !buf.ok() || buf.at_end() )
			break;

		int i = std::ssize( dest ); dest.emplace_back();
		if ( ! Unserialize( buf, dest[i] ) )
			return false;
	}
	return true;
}

// std::vector<bool> has no addressable elements; unserialize through a deque, which does.
template< class A >
bool Unserialize( ks::byte_reader &buf, std::vector< bool, A > &dest )
{
	std::deque< bool > tmp;
	bool bOk = Unserialize( buf, tmp );
	dest.assign( tmp.begin(), tmp.end() );
	return bOk;
}

template< class A >
bool Unserialize( ks::text_reader &buf, std::vector< bool, A > &dest )
{
	std::deque< bool > tmp;
	bool bOk = Unserialize( buf, tmp );
	dest.assign( tmp.begin(), tmp.end() );
	return bOk;
}

}
