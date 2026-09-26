//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Utilities for serialization/unserialization
//=============================================================================//

#ifndef SERIALIZATION_H
#define SERIALIZATION_H


#include "tier1/utlvector.h"
#include "tier1/byte_reader.h"
#include "tier1/byte_writer.h"
#include "tier1/text_reader.h"
#include "tier1/text_writer.h"
#include "tier1/string_escapes.h"
#include <deque>
#include <string>
#include <vector>


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
#define SERIALIZE_DECLARE( _type ) \
	bool Serialize( ks::byte_writer &buf, const _type &src ); \
	bool Unserialize( ks::byte_reader &buf, _type &dest ); \
	bool Serialize( ks::text_writer &buf, const _type &src ); \
	bool Unserialize( ks::text_reader &buf, _type &dest );

SERIALIZE_DECLARE( bool )
SERIALIZE_DECLARE( int )
SERIALIZE_DECLARE( float )
SERIALIZE_DECLARE( Vector2D )
SERIALIZE_DECLARE( Vector )
SERIALIZE_DECLARE( Vector4D )
SERIALIZE_DECLARE( QAngle )
SERIALIZE_DECLARE( Quaternion )
SERIALIZE_DECLARE( VMatrix )
SERIALIZE_DECLARE( Color )

// Binary blobs: raw bytes, or hex in text (not the per-element vector format).
SERIALIZE_DECLARE( std::vector< unsigned char > )

SERIALIZE_DECLARE( std::string )

#undef SERIALIZE_DECLARE


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
		::Serialize( buf, src[i] );
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
			::Serialize( buf, src[i] );
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
			::Serialize( buf, src[i] );
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

	MEM_ALLOC_CREDIT_FUNCTION();

	int nCount = buf.read< int >();
	if ( nCount )
	{
		if constexpr ( requires { dest.reserve( nCount ); } )
			dest.reserve( nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			dest.emplace_back();
			if ( !::Unserialize( buf, dest[i] ) )
				return false;
		}
	}
	return buf.ok();
}

template< class V > requires ks::detail::serial_vector< V >::value
bool Unserialize( ks::text_reader &buf, V &dest )
{
	dest.clear();

	MEM_ALLOC_CREDIT_FUNCTION();

	while ( true )
	{
		buf.skip_whitespace();
		if ( !buf.ok() || buf.at_end() )
			break;

		int i = std::ssize( dest ); dest.emplace_back();
		if ( ! ::Unserialize( buf, dest[i] ) )
			return false;
	}
	return true;
}

// std::vector<bool> has no addressable elements; unserialize through a deque, which does.
template< class A >
bool Unserialize( ks::byte_reader &buf, std::vector< bool, A > &dest )
{
	std::deque< bool > tmp;
	bool bOk = ::Unserialize( buf, tmp );
	dest.assign( tmp.begin(), tmp.end() );
	return bOk;
}

template< class A >
bool Unserialize( ks::text_reader &buf, std::vector< bool, A > &dest )
{
	std::deque< bool > tmp;
	bool bOk = ::Unserialize( buf, tmp );
	dest.assign( tmp.begin(), tmp.end() );
	return bOk;
}


#endif // SERIALIZATION_H
