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

// Shaped like the CNetworkVar family: WireVar marks the wrapper and the payload is its single
// data member. The member is named m_Value here only because the real ones are -- nothing in
// reflect.h reads the name.
template <class T>
struct [[= WireVar{} ]] MockNetworkVar { T m_Value{}; };

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

// The vector, quaternion and color32 wrappers inherit m_Value instead of declaring it.
// Missing this reads every one of them as FIELD_EMBEDDED.
struct MockVectorBase : MockNetworkVar<Vector> {};
struct MockVectorDerived : MockVectorBase {};
static_assert( unwrap( ^^MockVectorBase ) == ^^Vector );
static_assert( unwrap( ^^MockVectorDerived ) == ^^Vector );
static_assert( tag_of_type( ^^MockVectorDerived ) == FIELD_VECTOR );

// A class that genuinely has no payload stays itself, so embedded members still resolve.
static_assert( unwrap( ^^FireBurst ) == ^^FireBurst );
static_assert( tag_of_type( ^^FireBurst ) == FIELD_EMBEDDED );

// WireVar asserts the wrapper's shape: exactly one data member is the payload. A wrapper that
// grows a second member is a contract violation rather than the silent misread the old name match
// produced, and an unrelated type that happens to declare m_Value is no longer peeled at all.
template <class T>
consteval bool unwrap_rejected()
{
	try { (void)unwrap( ^^T ); return false; }
	catch ( const std::meta::exception & ) { return true; }
}
struct [[= WireVar{} ]] MockTwoMembers { int m_Value; int m_Extra; };
static_assert( unwrap_rejected<MockTwoMembers>() );

// A type with no annotation is never peeled, however its members are named.
struct MockUnannotated { int m_Value; };
static_assert( unwrap( ^^MockUnannotated ) == ^^MockUnannotated );

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
// instance of the macro's nested class, whose payload is an array. A char array is a
// string; anything else is an array of the element's tag.
template <class T, int N> struct [[= WireVar{} ]] MockNetworkArray { T m_Value[N]; };

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

// ---- inherited members -------------------------------------------------------
// A table may send a member declared in a base -- every _NOBASE table does. offset_of is
// relative to the declaring class and nonstatic_data_members_of does not cross a base, so the
// lookup has to accumulate base subobject offsets to land in the derived class.
struct PlainBase { int m_first; float m_second; };
struct PlainMid : PlainBase { short m_third; };
struct PlainLeaf : PlainMid { double m_fourth; };

static_assert( find_member( ^^PlainLeaf, "m_first" ).found );
static_assert( !find_member( ^^PlainLeaf, "m_absent" ).found );
static_assert( find_member( ^^PlainLeaf, "m_first" ).offset == __builtin_offsetof( PlainLeaf, m_first ) );
static_assert( find_member( ^^PlainLeaf, "m_second" ).offset == __builtin_offsetof( PlainLeaf, m_second ) );
static_assert( find_member( ^^PlainLeaf, "m_third" ).offset == __builtin_offsetof( PlainLeaf, m_third ) );
static_assert( find_member( ^^PlainLeaf, "m_fourth" ).offset == __builtin_offsetof( PlainLeaf, m_fourth ) );

// The member reflection comes back too, so its type drives the tag exactly as an own member's does.
static_assert( tag_of_type( std::meta::type_of( find_member( ^^PlainLeaf, "m_second" ).member ) ) == FIELD_FLOAT );

// A private base member is still reachable: the lookup enumerates with unchecked access, which
// is what lets a derived table name it the way SENDINFO did.
class PrivBase { int m_hidden; public: int m_open; };
struct PrivLeaf : PrivBase {};
static_assert( find_member( ^^PrivLeaf, "m_hidden" ).found );
static_assert( find_member( ^^PrivLeaf, "m_open" ).offset == __builtin_offsetof( PrivLeaf, m_open ) );

// A derived member of the same name shadows the base one, matching normal lookup.
struct ShadowBase { int m_x; int m_pad; };
struct ShadowLeaf : ShadowBase { int m_x; };
static_assert( find_member( ^^ShadowLeaf, "m_x" ).offset == __builtin_offsetof( ShadowLeaf, m_x ) );

// The unsigned and width-named integers need tags too: an unsigned char member otherwise has
// none at all, which the macros never noticed because they derive everything from sizeof.
static_assert( tag_of_type( ^^unsigned char ) == FIELD_CHARACTER );
static_assert( tag_of_type( ^^signed char ) == FIELD_CHARACTER );
static_assert( tag_of_type( ^^unsigned short ) == FIELD_SHORT );
static_assert( tag_of_type( ^^unsigned int ) == FIELD_INTEGER );
static_assert( tag_of_type( ^^long long ) == FIELD_INTEGER64 );
static_assert( tag_of_type( ^^double ) == FIELD_VOID );   // still genuinely unmapped

// Enums are networked as their underlying integer; ShatterSurface_t and friends are declared
// exactly this way.
enum PlainEnum { PE_A };
enum class ByteEnum : unsigned char { BE_A };
static_assert( tag_of_type( ^^PlainEnum ) == FIELD_INTEGER );
static_assert( tag_of_type( ^^ByteEnum ) == FIELD_CHARACTER );

} // namespace
