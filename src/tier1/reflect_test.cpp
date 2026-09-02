// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Tests for public/reflect.h. Everything here is a static_assert, so building this file
// is running the suite; there is nothing to invoke and nothing to link.
//
// These use synthetic types, which only proves the machinery is self-consistent. The
// test that matters is generating a byte-identical table for a real entity class; see
// game/shared/reflect_sendtable.h.

#include "reflect.h"

namespace
{

using namespace ks::reflect;

struct FireBurst { int count; float interval; };   // embedded, unannotated members

// Shaped like the CNetworkVar family: the payload lives in m_Value.
template <class T>
struct MockNetworkVar { T m_Value{}; };

// A class template carrying its own tag -- the shape CHandle will take. Also checks that
// an annotation on a primary template is visible from a specialization.
template <class T>
class [[= As{ FIELD_EHANDLE } ]] Handle { unsigned m_Index{}; };
class CBaseEntity;

struct Weapon
{
	[[= Net{ .bits = 20 } ]] [[= Pred{ .tolerance = 0.01f } ]]
	MockNetworkVar<GameTime> m_flNextPrimaryAttack;

	[[= Net{ .flags = SPROP_NOSCALE } ]]
	MockNetworkVar<WorldPos> m_vecOrigin;

	[[= Key{ .name = "clip" } ]]
	int m_iClip1;

	[[= Net{ .wire = "m_hOwner" } ]]
	Handle<CBaseEntity> m_hOwnerEntity;

	[[= Key{} ]]
	FireBurst m_burst;

	float m_flNotReflected;
};

// ---- unwrapping --------------------------------------------------------------
// A networked member's own type is a wrapper; the tag comes from the payload. Unwrapping
// stops at a type that carries its own tag, so As stays readable.
static_assert( unwrap( ^^MockNetworkVar<GameTime> ) == ^^GameTime );
static_assert( unwrap( ^^MockNetworkVar<int> ) == ^^int );
static_assert( unwrap( ^^MockNetworkVar<MockNetworkVar<float>> ) == ^^float );
static_assert( unwrap( ^^int ) == ^^int );

// ---- participation -----------------------------------------------------------
static_assert( members<Net,  Weapon>.size() == 3 );
static_assert( members<Pred, Weapon>.size() == 1 );
static_assert( members<Key,  Weapon>.size() == 2 );

// ---- tags derive from the member's type, with nothing stated at the field -----
static_assert( tag_of( members<Net,  Weapon>[0] ) == FIELD_TIME );
static_assert( tag_of( members<Pred, Weapon>[0] ) == FIELD_TIME );   // not restated
static_assert( tag_of( members<Net,  Weapon>[1] ) == FIELD_POSITION_VECTOR );
static_assert( tag_of( members<Net,  Weapon>[2] ) == FIELD_EHANDLE );
static_assert( tag_of( members<Key,  Weapon>[0] ) == FIELD_INTEGER );
static_assert( tag_of( members<Key,  Weapon>[1] ) == FIELD_EMBEDDED );

// an embedded member is one table entry, not its leaves
static_assert( is_embedded( members<Key, Weapon>[1] ) );
static_assert( !is_embedded( members<Key, Weapon>[0] ) );

// a semantic type costs no space over the type it refines
static_assert( sizeof( GameTime ) == sizeof( float ) );
static_assert( sizeof( TickCount ) == sizeof( int ) );
static_assert( sizeof( WorldPos ) == sizeof( Vector ) );

// ---- names and wire parameters -----------------------------------------------
static_assert( std::string_view{ external_name<Net>( members<Net, Weapon>[0] ) } == "m_flNextPrimaryAttack" );
static_assert( std::string_view{ external_name<Net>( members<Net, Weapon>[2] ) } == "m_hOwner" );
static_assert( std::string_view{ external_name<Key>( members<Key, Weapon>[0] ) } == "clip" );
static_assert( get<Net>( members<Net, Weapon>[0] ).bits == 20 );
static_assert( get<Net>( members<Net, Weapon>[1] ).flags == SPROP_NOSCALE );
static_assert( get<Pred>( members<Pred, Weapon>[0] ).tolerance == 0.01f );

// ---- offsets agree with the language -----------------------------------------
// __builtin_offsetof, not offsetof: dt_common.h hijacks the macro with a null-pointer
// dereference, which is not a constant expression. That form cannot be replaced
// wholesale -- 38 sites rely on it invoking an overloaded operator[] through a null
// pointer -- so constexpr code has to bypass the macro.
static_assert( byte_offset_of( members<Net, Weapon>[1] ) == __builtin_offsetof( Weapon, m_vecOrigin ) );
static_assert( byte_offset_of( members<Net, Weapon>[2] ) == __builtin_offsetof( Weapon, m_hOwnerEntity ) );

// an unannotated member appears in no table
static_assert( []consteval {
	for ( auto m : members<Net, Weapon> )
		if ( std::meta::identifier_of( m ) == "m_flNotReflected" ) return false;
	return true;
}() );

// ---- the validation gate rejects a tag the type cannot represent --------------
static_assert( !tag_fits( FIELD_TIME, ^^int ) );
static_assert( !tag_fits( FIELD_POSITION_VECTOR, ^^float ) );
static_assert( !tag_fits( FIELD_INTEGER, ^^float ) );      // the silent-corruption case
static_assert( tag_fits( FIELD_TIME, ^^float ) );
static_assert( tag_fits( FIELD_POSITION_VECTOR, ^^Vector ) );

// ---- arrays and strings ------------------------------------------------------
// CNetworkString( name, len ) and CNetworkArray( type, name, count ) make the member an
// instance of the macro's nested class, whose m_Value is an array. A char array is a
// string; anything else is an array of the element's tag.
template <class T, int N> struct MockNetworkArray { T m_Value[N]; };

static_assert( is_array_member( ^^MockNetworkArray<int, 4> ) );
static_assert( !is_array_member( ^^MockNetworkVar<int> ) );
static_assert( is_string_member( ^^MockNetworkArray<char, 32> ) );
static_assert( !is_string_member( ^^MockNetworkArray<int, 4> ) );
static_assert( array_extent_of( ^^MockNetworkArray<int, 4> ) == 4 );
static_assert( array_element_size( ^^MockNetworkArray<int, 4> ) == sizeof( int ) );
static_assert( tag_of_type( ^^MockNetworkArray<int, 4> ) == FIELD_INTEGER );   // element's tag

// ---- leaf walk: for hashing a live object, not for shaping tables ------------
consteval std::vector<Leaf> burst_leaves()
{
	std::vector<Leaf> out;
	leaf_walk( ^^FireBurst, 0, "m_burst", out );
	return out;
}
inline constexpr auto burst = std::define_static_array( burst_leaves() );
static_assert( burst.size() == 2 );
static_assert( std::string_view{ burst[0].name } == "count" );
static_assert( burst[0].type == FIELD_INTEGER && burst[0].offset == 0 );
static_assert( burst[1].type == FIELD_FLOAT && burst[1].offset == 4 );

// ---- splices reach the members a walk names ----------------------------------
// The member form is p.[:m:]; there is no type-scope form. Vector is deliberately
// absent: mathlib's default constructor is not constexpr, so no object containing one
// can be built at compile time.
struct Plain
{
	[[= Net{} ]] float a;
	[[= Net{} ]] int b;
	double unreflected;
};

consteval std::size_t net_bytes()
{
	Plain p{};
	std::size_t n = 0;
	template for ( constexpr auto m : members<Net, Plain> )
		n += sizeof( p.[:m:] );
	return n;
}
static_assert( net_bytes() == sizeof( float ) + sizeof( int ) );

} // namespace
