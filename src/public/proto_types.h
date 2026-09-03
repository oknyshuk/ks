// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Vocabulary for proto2 messages.

#ifndef KS_PROTO_TYPES_H
#define KS_PROTO_TYPES_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace ks::proto
{

// A member's field number, and the only thing a schema states. Everything else follows
// from the member's type.
struct Pb { int num; };

// int32 alone cannot say whether it goes on the wire as a varint, zigzagged, or fixed
// width, so the ones that deviate from varint say so in the type.
template <std::integral T>
struct zigzag
{
	using value_type = T;
	T v{};
	constexpr zigzag() = default;
	constexpr explicit zigzag( T x ) : v( x ) {}
	constexpr operator T() const { return v; }
	auto operator<=>( const zigzag & ) const = default;
};

template <std::integral T>
struct fixed
{
	using value_type = T;
	T v{};
	constexpr fixed() = default;
	constexpr explicit fixed( T x ) : v( x ) {}
	constexpr operator T() const { return v; }
	auto operator<=>( const fixed & ) const = default;
};

// The plain value behind a wire encoding. Only one user-defined conversion is applied per
// expression, so opt<fixed<uint32_t>> has to reach uint32_t in a single step.
template <class T> struct plain            { using type = const T &; };
template <class T> struct plain<zigzag<T>> { using type = T; };
template <class T> struct plain<fixed<T>>  { using type = T; };

// A field that may be absent. It converts to its value, so reading one is just the member
// name; writing one is assignment. Absence is only interesting where the code asks, which
// is rare, so it does not get to complicate every use.
template <class T>
class opt
{
public:
	opt() = default;

	// Explicit, or a conditional expression between opt<T> and T has two viable
	// conversions and neither wins.
	explicit opt( T v ) : m_v( std::move( v ) ), m_set( true ) {}

	// Takes anything the field can be built from, so a wire-encoded field still assigns
	// from a plain value.
	template <class U> requires std::constructible_from<T, U &&>
	opt &operator=( U &&x )
	{
		m_v = T( std::forward<U>( x ) );
		m_set = true;
		return *this;
	}

	operator typename plain<T>::type() const { return m_v; }
	const T *operator->() const { return &m_v; }
	const T &get() const { return m_v; }

	// An absent field equals nothing.
	template <class U>
	friend bool operator==( const opt &a, const U &b ) { return a.m_set && a.m_v == b; }

	bool has_value() const { return m_set; }
	void reset() { *this = opt{}; }

	// Mutation is spelled out, so a write is never mistaken for a read.
	T &mut()
	{
		m_set = true;
		return m_v;
	}

private:
	T m_v{};
	bool m_set = false;
};

using sint32   = zigzag<int32_t>;
using sint64   = zigzag<int64_t>;
using fixed32  = fixed<uint32_t>;
using fixed64  = fixed<uint64_t>;
using sfixed32 = fixed<int32_t>;
using sfixed64 = fixed<int64_t>;

// Declared here and defined in proto.h, which needs <meta>. Schemas and their users get
// these four operations without paying for reflection in every translation unit; the
// instantiations live next to the schema.
template <class T> std::size_t byte_size( const T & );
template <class T> std::byte *write( const T &, std::byte * );
template <class T> bool read_bytes( T &, const void *, std::size_t );
template <class T> const char *type_name();
template <class T> std::string to_text( const T & );

} // namespace ks::proto

#endif // KS_PROTO_TYPES_H
