// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// proto2 wire encoding, derived from the members of a struct rather than from generated
// code. A schema annotates field numbers; the C++ type gives everything else:
//
//   optional<T> presence   vector<T> repetition   string bytes   class nested message
//   integral/enum varint   float fixed32          double fixed64
//
// byte_size/write/read mirror what the net layer already does, so a message can be sized
// into a packet before it is written.

#ifndef KS_PROTO_H
#define KS_PROTO_H

#include <meta>

#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "proto_types.h"

namespace ks::proto
{

enum class wire : uint8_t { varint = 0, fix64 = 1, bytes = 2, fix32 = 5 };

consteval int field_number( std::meta::info m )
{
	for ( auto a : std::meta::annotations_of( m ) )
		if ( std::meta::type_of( a ) == ^^const Pb )
			return std::meta::extract<Pb>( a ).num;
	return 0;
}

consteval auto members_of( std::meta::info t )
{
	return std::define_static_array(
		std::meta::nonstatic_data_members_of( t, std::meta::access_context::unchecked() ) );
}

// ---- varints ------------------------------------------------------------------

constexpr int varint_size( uint64_t v )
{
	int n = 1;
	for ( ; v >= 0x80; v >>= 7 ) ++n;
	return n;
}

constexpr std::byte *put_varint( std::byte *o, uint64_t v )
{
	for ( ; v >= 0x80; v >>= 7 )
		*o++ = std::byte( uint8_t( v ) | 0x80 );
	*o++ = std::byte( uint8_t( v ) );
	return o;
}

// Signed varints sign-extend to 64 bits, so -1 is ten bytes. Widening through int64_t
// rather than the field's own width is what makes that so.
template <std::integral T>
constexpr uint64_t zext( T v )
{
	if constexpr ( std::signed_integral<T> )
		return uint64_t( int64_t( v ) );
	else
		return uint64_t( v );
}

// ---- input --------------------------------------------------------------------

class reader
{
public:
	constexpr explicit reader( std::span<const std::byte> s )
		: m_p( s.data() ), m_end( s.data() + s.size() ) {}

	constexpr bool done() const { return m_p >= m_end; }
	constexpr bool ok() const { return m_ok; }
	constexpr void fail() { m_ok = false; m_p = m_end; }

	constexpr uint64_t varint()
	{
		uint64_t v = 0;
		for ( int shift = 0; shift < 64 && !done(); shift += 7 )
		{
			const uint8_t b = uint8_t( *m_p++ );
			v |= uint64_t( b & 0x7f ) << shift;
			if ( !( b & 0x80 ) ) return v;
		}
		fail();
		return 0;
	}

	template <class T> T little()
	{
		if ( std::size_t( m_end - m_p ) < sizeof( T ) ) { fail(); return T{}; }
		T v;
		std::memcpy( &v, m_p, sizeof( T ) );
		m_p += sizeof( T );
		return v;
	}

	std::span<const std::byte> block()
	{
		const uint64_t n = varint();
		if ( !m_ok || n > uint64_t( m_end - m_p ) ) { fail(); return {}; }
		const std::byte *p = std::exchange( m_p, m_p + n );
		return { p, std::size_t( n ) };
	}

	// Unknown fields only arise across a version skew; skipping keeps that recoverable.
	void skip( wire w )
	{
		switch ( w )
		{
		case wire::varint: varint(); break;
		case wire::fix64:  little<uint64_t>(); break;
		case wire::fix32:  little<uint32_t>(); break;
		case wire::bytes:  block(); break;
		default:           fail(); break;
		}
	}

private:
	const std::byte *m_p;
	const std::byte *m_end;
	bool m_ok = true;
};

// ---- per-type encoding --------------------------------------------------------
// One specialization per wire representation, both directions together. The
// unspecialized template is a nested message, the only case that recurses.

template <class T> struct codec;

template <class T> std::size_t byte_size( const T & );
template <class T> std::byte *write( const T &, std::byte * );
template <class T> bool read( T &, reader & );

template <std::integral T> requires ( !std::same_as<T, bool> )
struct codec<T>
{
	static constexpr wire tag = wire::varint;
	static std::size_t size( T v ) { return varint_size( zext( v ) ); }
	static std::byte *put( std::byte *o, T v ) { return put_varint( o, zext( v ) ); }
	static void get( reader &r, T &v ) { v = T( r.varint() ); }
};

template <> struct codec<bool>
{
	static constexpr wire tag = wire::varint;
	static std::size_t size( bool ) { return 1; }
	static std::byte *put( std::byte *o, bool v ) { return put_varint( o, v ); }
	static void get( reader &r, bool &v ) { v = r.varint() != 0; }
};

template <class T> requires std::is_enum_v<T>
struct codec<T>
{
	using rep = std::underlying_type_t<T>;
	static constexpr wire tag = wire::varint;
	static std::size_t size( T v ) { return varint_size( zext( std::to_underlying( v ) ) ); }
	static std::byte *put( std::byte *o, T v ) { return put_varint( o, zext( std::to_underlying( v ) ) ); }
	static void get( reader &r, T &v ) { v = T( rep( r.varint() ) ); }
};

template <std::integral T> struct codec<zigzag<T>>
{
	static constexpr wire tag = wire::varint;
	static constexpr uint64_t fold( T v )
	{
		return ( uint64_t( v ) << 1 ) ^ zext( T( v >> ( sizeof( T ) * 8 - 1 ) ) );
	}
	static std::size_t size( zigzag<T> v ) { return varint_size( fold( v.v ) ); }
	static std::byte *put( std::byte *o, zigzag<T> v ) { return put_varint( o, fold( v.v ) ); }
	static void get( reader &r, zigzag<T> &v )
	{
		const uint64_t n = r.varint();
		v.v = T( ( n >> 1 ) ^ ( uint64_t{ 0 } - ( n & 1 ) ) );
	}
};

// Fixed-width fields are little-endian, which every target here is.
template <class T> struct little_endian
{
	static std::size_t size( T ) { return sizeof( T ); }
	static std::byte *put( std::byte *o, T v )
	{
		std::memcpy( o, &v, sizeof( T ) );
		return o + sizeof( T );
	}
	static void get( reader &r, T &v ) { v = r.little<T>(); }
};

template <std::integral T> struct codec<fixed<T>>
{
	static constexpr wire tag = sizeof( T ) == 8 ? wire::fix64 : wire::fix32;
	static std::size_t size( fixed<T> ) { return sizeof( T ); }
	static std::byte *put( std::byte *o, fixed<T> v ) { return little_endian<T>::put( o, v.v ); }
	static void get( reader &r, fixed<T> &v ) { little_endian<T>::get( r, v.v ); }
};

template <> struct codec<float> : little_endian<float>
{
	static constexpr wire tag = wire::fix32;
};

template <> struct codec<double> : little_endian<double>
{
	static constexpr wire tag = wire::fix64;
};

// bytes and string are the same on the wire.
template <> struct codec<std::string>
{
	static constexpr wire tag = wire::bytes;
	static std::size_t size( const std::string &v ) { return varint_size( v.size() ) + v.size(); }
	static std::byte *put( std::byte *o, const std::string &v )
	{
		o = put_varint( o, v.size() );
		std::memcpy( o, v.data(), v.size() );
		return o + v.size();
	}
	static void get( reader &r, std::string &v )
	{
		const auto b = r.block();
		v.assign( reinterpret_cast<const char *>( b.data() ), b.size() );
	}
};

template <class T> struct codec
{
	static constexpr wire tag = wire::bytes;
	// Nested length has to be known before the body, and is recomputed rather than
	// cached: depth is shallow and a cache would have to live somewhere.
	static std::size_t size( const T &v )
	{
		const std::size_t n = byte_size( v );
		return varint_size( n ) + n;
	}
	static std::byte *put( std::byte *o, const T &v )
	{
		return write( v, put_varint( o, byte_size( v ) ) );
	}
	static void get( reader &r, T &v )
	{
		reader sub{ r.block() };
		if ( !r.ok() || !read( v, sub ) ) r.fail();
	}
};

// ---- members ------------------------------------------------------------------

// How many values a member holds, which is the only thing its outer type says.
enum class card { one, maybe, many };

template <class T> struct field                 { using value = T; static constexpr card n = card::one; };
template <class T> struct field<opt<T>>         { using value = T; static constexpr card n = card::maybe; };
template <class T> struct field<std::vector<T>> { using value = T; static constexpr card n = card::many; };

template <class T> using value_of = typename field<T>::value;

template <class T>
constexpr uint64_t tag_of( int num )
{
	return ( uint64_t( num ) << 3 ) | uint64_t( codec<value_of<T>>::tag );
}

// Presence and repetition both reduce to "for each value actually there".
template <class M, class F>
constexpr void for_each_value( const M &f, F &&fn )
{
	if constexpr ( field<M>::n == card::maybe )
	{
		if ( f.has_value() ) fn( f.get() );
	}
	else if constexpr ( field<M>::n == card::many )
	{
		for ( const auto &e : f ) fn( e );
	}
	else
		fn( f );
}

template <class T>
std::size_t byte_size( const T &msg )
{
	std::size_t n = 0;
	template for ( constexpr auto m : members_of( ^^T ) )
	{
		using M = [:std::meta::type_of( m ):];
		static_assert( field_number( m ) > 0, "schema member needs [[= Pb{n} ]]" );

		// Hoisted: a std::meta::info may not appear inside the lambda body.
		constexpr std::size_t tag_bytes = varint_size( tag_of<M>( field_number( m ) ) );

		for_each_value( msg.[:m:],
		                [&]( const auto &v ) { n += tag_bytes + codec<value_of<M>>::size( v ); } );
	}
	return n;
}

template <class T>
std::byte *write( const T &msg, std::byte *o )
{
	template for ( constexpr auto m : members_of( ^^T ) )
	{
		using M = [:std::meta::type_of( m ):];
		constexpr uint64_t t = tag_of<M>( field_number( m ) );

		for_each_value( msg.[:m:],
		                [&]( const auto &v ) { o = codec<value_of<M>>::put( put_varint( o, t ), v ); } );
	}
	return o;
}

template <class T>
bool read( T &msg, reader &r )
{
	while ( !r.done() )
	{
		const uint64_t t = r.varint();
		if ( !r.ok() ) return false;

		const int num = int( t >> 3 );
		bool matched = false;

		template for ( constexpr auto m : members_of( ^^T ) )
		{
			using M = [:std::meta::type_of( m ):];
			if ( !matched && field_number( m ) == num )
			{
				matched = true;
				auto &f = msg.[:m:];
				if constexpr ( field<M>::n == card::maybe )
					codec<value_of<M>>::get( r, f.mut() );
				else if constexpr ( field<M>::n == card::many )
					codec<value_of<M>>::get( r, f.emplace_back() );
				else
					codec<value_of<M>>::get( r, f );
			}
		}

		if ( !matched ) r.skip( wire( t & 7 ) );
		if ( !r.ok() ) return false;
	}
	return true;
}

// ---- whole messages -----------------------------------------------------------

template <class T>
bool read_bytes( T &msg, const void *p, std::size_t n )
{
	reader r{ std::span{ static_cast<const std::byte *>( p ), n } };
	return read( msg, r ) && r.ok();
}

template <class T>
const char *type_name()
{
	return std::define_static_string( std::meta::identifier_of( ^^T ) );
}

// Field dump for logs. Absent fields are simply missing, which is the useful part.
template <class T>
std::string to_text( const T &msg )
{
	std::string s;
	template for ( constexpr auto m : members_of( ^^T ) )
	{
		constexpr const char *name = std::define_static_string( std::meta::identifier_of( m ) );

		for_each_value( msg.[:m:], [&]( const auto &v ) {
			using V = std::remove_cvref_t<decltype( v )>;
			s += name;
			s += ": ";
			if constexpr ( std::same_as<V, std::string> )
				s += std::to_string( v.size() ) + " bytes";
			else if constexpr ( requires { std::to_string( v ); } )
				s += std::to_string( v );
			else if constexpr ( requires { v.v; } )
				s += std::to_string( v.v );
			else
				s += "{ " + to_text( v ) + "}";
			s += ' ';
		} );
	}
	return s;
}

} // namespace ks::proto

// One .cpp beside a schema instantiates these, so no other translation unit parses <meta>
// in order to use it.
#define KS_PROTO_INSTANTIATE( T )                                            \
	template std::size_t ks::proto::byte_size<T>( const T & );                \
	template std::byte *ks::proto::write<T>( const T &, std::byte * );        \
	template bool ks::proto::read_bytes<T>( T &, const void *, std::size_t ); \
	template const char *ks::proto::type_name<T>();                          \
	template std::string ks::proto::to_text<T>( const T & );

#endif // KS_PROTO_H
