// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The reflection machinery: read the annotations from reflect_annotations.h and derive
// what the four metadata systems need. Include this only in translation units that
// actually emit tables -- <meta> is ~50k preprocessed lines. Entity headers include
// reflect_annotations.h instead.
//
// This header deliberately knows nothing about SendProp or typedescription_t. Emission
// lives next to the system being emitted (see game/shared/reflect_sendtable.h); here we
// only answer questions about members.
//
// Constraints whose diagnostics point nowhere near the cause:
//   - type_of(annotation) yields ^^const A, never ^^A.
//   - define_static_string is in std::, not std::meta::, and takes a range.
//   - narrowing offset_of(m).bytes (a ptrdiff_t) in braced-init is ill-formed and
//     surfaces as "reflect_constant failed".
//   - one ill-formed consteval function reports as "non-constant condition" at every
//     call site, naming none of them.

#ifndef KS_REFLECT_H
#define KS_REFLECT_H

#include "reflect_annotations.h"
#include "string_t.h"

#include <meta>
#include <span>
#include <vector>
#include <string_view>

namespace ks::reflect
{

// ---- annotation lookup -------------------------------------------------------
template <class A>
consteval bool has( std::meta::info m )
{
	for ( auto a : std::meta::annotations_of( m ) )
		if ( std::meta::type_of( a ) == ^^const A ) return true;
	return false;
}

template <class A>
consteval A get( std::meta::info m )
{
	for ( auto a : std::meta::annotations_of( m ) )
		if ( std::meta::type_of( a ) == ^^const A ) return std::meta::extract<A>( a );
	return A{};
}

// A name_t interns rather than views: a string_view into an annotation dangles, because
// an annotation is always a temporary.
consteval const char *intern( const name_t &n )
{
	return std::define_static_string( std::string_view{ n.data, __builtin_strlen( n.data ) } );
}
consteval const char *intern( std::string_view s ) { return std::define_static_string( s ); }

// ---- network var unwrapping --------------------------------------------------
// CNetworkVar( float, m_flFoo ) declares the member as CNetworkVarBase<float, Notifier>,
// so a networked member's own type is a wrapper. Every variant in the family
// (CNetworkVarBase, CNetworkVectorBase, CNetworkHandleBase, ...) stores the payload in
// m_Value, which is a more robust hook than matching template names.
consteval std::meta::info unwrap( std::meta::info t )
{
	t = std::meta::remove_cv( t );
	if ( !std::meta::is_class_type( t ) ) return t;
	for ( auto m : std::meta::nonstatic_data_members_of( t, std::meta::access_context::unchecked() ) )
		if ( std::meta::identifier_of( m ) == "m_Value" )
			return unwrap( std::meta::type_of( m ) );
	return t;
}

// ---- type -> tag -------------------------------------------------------------
// Builtins are hardcoded; every other type declares its own tag with As. That keeps game
// types (CHandle, the network var wrappers) out of this header.
consteval fieldtype_t tag_of_type( std::meta::info t )
{
	t = unwrap( t );
	// CNetworkString and CNetworkArray hold an array in m_Value; the tag is the element's.
	if ( std::meta::is_array_type( t ) ) t = std::meta::remove_extent( t );
	if ( std::meta::is_class_type( t ) )
		for ( auto a : std::meta::annotations_of( t ) )
			if ( std::meta::type_of( a ) == ^^const As ) return std::meta::extract<As>( a ).type;

	if ( t == ^^float )    return FIELD_FLOAT;
	if ( t == ^^int )      return FIELD_INTEGER;
	if ( t == ^^bool )     return FIELD_BOOLEAN;
	if ( t == ^^short )    return FIELD_SHORT;
	if ( t == ^^char )     return FIELD_CHARACTER;
	if ( t == ^^string_t ) return FIELD_STRING;
	if ( t == ^^Vector )   return FIELD_VECTOR;
	if ( t == ^^QAngle )   return FIELD_VECTOR;
	if ( std::meta::is_class_type( t ) ) return FIELD_EMBEDDED;
	return FIELD_VOID;
}

// A declared tag must be representable by the member's type. This is the check the
// macros never had: DEFINE_FIELD( m_flFoo, FIELD_INTEGER ) on a float compiles today and
// silently corrupts.
consteval bool tag_fits( fieldtype_t declared, std::meta::info t )
{
	const fieldtype_t s = tag_of_type( t );
	switch ( declared )
	{
	case FIELD_TIME:            return s == FIELD_FLOAT || s == FIELD_TIME;
	case FIELD_TICK:            return s == FIELD_INTEGER || s == FIELD_TICK;
	case FIELD_POSITION_VECTOR: return s == FIELD_VECTOR || s == FIELD_POSITION_VECTOR;
	case FIELD_MODELNAME:
	case FIELD_SOUNDNAME:       return s == FIELD_STRING;
	default:                    return declared == s;
	}
}

// One tag per member: from the type unless overridden at the field, validated either way.
consteval fieldtype_t tag_of( std::meta::info m )
{
	const fieldtype_t declared = get<As>( m ).type;
	if ( declared == FIELD_VOID ) return tag_of_type( std::meta::type_of( m ) );
	if ( !tag_fits( declared, std::meta::type_of( m ) ) )
		throw std::meta::exception( "declared field tag does not fit the member type", m );
	return declared;
}

// ---- member queries ----------------------------------------------------------
// Named to avoid ADL picking std::meta::offset_of, which any std::meta::info argument
// would otherwise make ambiguous.
consteval std::size_t byte_offset_of( std::meta::info m )
{
	return static_cast<std::size_t>( std::meta::offset_of( m ).bytes );
}

// The wire/external name: a Net or Key override if given, else the identifier.
template <class A>
consteval const char *external_name( std::meta::info m )
{
	if constexpr ( requires { A{}.wire; } )
		if ( !get<A>( m ).wire.empty() ) return intern( get<A>( m ).wire );
	if constexpr ( requires { A{}.name; } )
		if ( !get<A>( m ).name.empty() ) return intern( get<A>( m ).name );
	return intern( std::meta::identifier_of( m ) );
}

// Members carrying annotation A, in declaration order. Table order is wire-visible, so
// this must stay stable.
template <class A>
consteval std::vector<std::meta::info> tagged_members( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto m : std::meta::nonstatic_data_members_of( cls, std::meta::access_context::unchecked() ) )
		if ( has<A>( m ) ) out.push_back( m );
	return out;
}

template <class A, class T>
inline constexpr auto members = std::define_static_array( tagged_members<A>( ^^T ) );

// An embedded member becomes one entry pointing at a sub-table, in both SendTables
// (SendPropDataTable) and datamaps (DEFINE_EMBEDDED). So recursion happens in the
// emitter, per nested type -- descriptors never flatten.
consteval bool is_embedded( std::meta::info m ) { return tag_of( m ) == FIELD_EMBEDDED; }

// CNetworkString( name, len ) and CNetworkArray( type, name, count ) both store an array
// in m_Value, but a char array is a string prop and anything else is an array prop, so the
// two have to be told apart before dispatching on the tag.
consteval bool is_array_member( std::meta::info t )
{
	return std::meta::is_array_type( unwrap( t ) );
}
consteval bool is_string_member( std::meta::info t )
{
	t = unwrap( t );
	return std::meta::is_array_type( t ) && std::meta::remove_extent( t ) == ^^char;
}
consteval std::size_t array_extent_of( std::meta::info t )
{
	return std::meta::extent( unwrap( t ) );
}
consteval std::size_t array_element_size( std::meta::info t )
{
	return std::meta::size_of( std::meta::remove_extent( unwrap( t ) ) );
}

// ---- leaf walk ---------------------------------------------------------------
// Flattening to scalar leaves, with accumulated offsets. This is *not* how tables are
// shaped; it is for walking a live object -- hashing or diffing world state.
struct Leaf
{
	const char *name;
	fieldtype_t type;
	std::size_t offset;
	std::size_t size;
};

consteval void leaf_walk( std::meta::info type, std::size_t base, std::string_view name,
                          std::vector<Leaf> &out )
{
	if ( tag_of_type( type ) != FIELD_EMBEDDED )
	{
		out.push_back( Leaf{ intern( name ), tag_of_type( type ), base, std::meta::size_of( type ) } );
		return;
	}
	for ( auto m : std::meta::nonstatic_data_members_of( type, std::meta::access_context::unchecked() ) )
		leaf_walk( std::meta::type_of( m ), base + byte_offset_of( m ), std::meta::identifier_of( m ), out );
}

} // namespace ks::reflect

#endif // KS_REFLECT_H
