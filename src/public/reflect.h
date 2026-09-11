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

// Forward declarations only: comparing reflections does not need a complete type. Must be at
// global scope -- declared inside ks::reflect they would name different types, and the tag
// would silently come out FIELD_EMBEDDED. Both are hardcoded rather than As-annotated because
// basehandle.h and basetypes.h are reached by nearly everything, and reflect_annotations.h
// would cost them datamap.h and mathlib.
class CBaseHandle;
struct color32_s;

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

// Repeatable annotations, in order. Exclude is the only one so far.
template <class A>
consteval std::vector<A> all( std::meta::info m )
{
	std::vector<A> out;
	for ( auto a : std::meta::annotations_of( m ) )
		if ( std::meta::type_of( a ) == ^^const A ) out.push_back( std::meta::extract<A>( a ) );
	return out;
}

// The template argument of a Proxy<F> annotation, or a null reflection. Returned as a
// reflection rather than a pointer so the caller picks the function pointer type: the send and
// recv proxy signatures differ.
// The proxy for one side: a Proxy<F> with no side serves both, and a member may carry one for each.
consteval std::meta::info proxy_arg_of( std::meta::info m, WireSide want )
{
	for ( auto a : std::meta::annotations_of( m ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( !std::meta::has_template_arguments( t ) || std::meta::template_of( t ) != ^^Proxy )
			continue;
		const auto args = std::meta::template_arguments_of( t );
		const WireSide s = args.size() > 1 ? std::meta::extract<WireSide>( args[1] ) : WIRE_BOTH;
		if ( s == WIRE_BOTH || s == want )
			return args[0];
	}
	return std::meta::info{};
}

consteval bool has_proxy( std::meta::info m, WireSide want )
{
	return proxy_arg_of( m, want ) != std::meta::info{};
}

// A function-pointer template argument, or null when it was left defaulted. extract<Fn> on the
// nullptr default fails outright, because the argument's type is nullptr_t rather than Fn.
template <class Fn>
consteval Fn extract_fn( std::meta::info arg )
{
	if ( arg == std::meta::info{} || std::meta::type_of( arg ) == ^^decltype( nullptr ) )
		return nullptr;
	return std::meta::extract<Fn>( arg );
}

// A name_t interns rather than views: a string_view into an annotation dangles, because
// an annotation is always a temporary.
consteval const char *intern( const name_t &n )
{
	return std::define_static_string( std::string_view{ n.data, __builtin_strlen( n.data ) } );
}
consteval const char *intern( std::string_view s ) { return std::define_static_string( s ); }

// ---- wire var unwrapping -----------------------------------------------------
// A networked member's declared type is a wrapper, not the value that crosses the wire:
// CNetworkVar( float, m_flFoo ) declares CNetworkVarBase<float, Notifier>. The wrapper says so
// itself with WireVar, and its payload is its single data member, found structurally. So the
// member's name stays private to the wrapper and the wrapper family is replaceable without
// editing this header.
//
// This used to match the member names "m_Value" and "m_Val", which had two costs: renaming a
// wrapper's payload silently changed every prop kind derived from it, and any unrelated type that
// happened to declare one of those names was silently peeled.
//
// The vector, quaternion, color32 and handle variants inherit the payload rather than declaring
// it, and nonstatic_data_members_of does not cross a base, so bases have to be searched too --
// otherwise every one of them reads as FIELD_EMBEDDED.
consteval std::meta::info payload_member_of( std::meta::info t )
{
	auto members = std::meta::nonstatic_data_members_of( t, std::meta::access_context::unchecked() );
	if ( members.size() != 1 )
		throw std::meta::exception( "a WireVar type must declare exactly one data member", t );
	return members[0];
}

consteval std::meta::info unwrap( std::meta::info t )
{
	t = std::meta::remove_cv( t );
	if ( !std::meta::is_class_type( t ) ) return t;
	if ( has<WireVar>( t ) ) return unwrap( std::meta::type_of( payload_member_of( t ) ) );
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
	{
		const std::meta::info u = unwrap( std::meta::type_of( b ) );
		if ( u != std::meta::remove_cv( std::meta::type_of( b ) ) ) return u;
	}
	return t;
}

consteval bool derives_from( std::meta::info t, std::meta::info base )
{
	if ( t == base ) return true;
	if ( !std::meta::is_class_type( t ) || !std::meta::is_complete_type( t ) ) return false;
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
		if ( derives_from( std::meta::remove_cv( std::meta::type_of( b ) ), base ) ) return true;
	return false;
}

// ---- type -> tag -------------------------------------------------------------
// Builtins are hardcoded; every other type declares its own tag with As. That keeps game
// types (the network var wrappers) out of this header.
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
	// The unsigned and width-named integers carry no distinct tag: the descriptor records a
	// width, and signedness travels in SPROP_UNSIGNED. Without these an `unsigned char` member
	// has no tag at all, which the macros never noticed because they derive everything from
	// sizeof.
	if ( t == ^^signed char || t == ^^unsigned char ) return FIELD_CHARACTER;
	if ( t == ^^unsigned short )                      return FIELD_SHORT;
	if ( t == ^^unsigned int || t == ^^long || t == ^^unsigned long ) return FIELD_INTEGER;
	if ( t == ^^long long || t == ^^unsigned long long )              return FIELD_INTEGER64;
	if ( t == ^^string_t ) return FIELD_STRING;
	// NO_STRING_T -- which the client defines -- makes string_t a plain `const char *`, and ^^string_t
	// is then an *alias* reflection that compares equal to nothing, so the line above is dead there
	// and a string member silently derived FIELD_VOID. C_PlayerResource::m_szName is the only one in
	// the tree, and its gate caught it as `fieldType legacy 2 vs generated 0`.
	if ( t == ^^const char * || t == ^^char * ) return FIELD_STRING;
	// An enum is networked as its underlying integer. The macros never needed a tag for these
	// either, since SendPropInt sizes itself from sizeof.
	if ( std::meta::is_enum_type( t ) ) return tag_of_type( std::meta::underlying_type( t ) );
	if ( t == ^^Vector )   return FIELD_VECTOR;
	if ( t == ^^QAngle )   return FIELD_VECTOR;
	if ( t == ^^color32_s ) return FIELD_COLOR32;
	// CHandle<T> and EHANDLE derive from CBaseHandle and add nothing.
	if ( derives_from( t, ^^CBaseHandle ) ) return FIELD_EHANDLE;
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
	// An index into a runtime table, refining a plain int.
	case FIELD_MODELINDEX:
	case FIELD_MATERIALINDEX:   return s == FIELD_INTEGER || s == FIELD_SHORT;
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

struct MemberRef
{
	std::meta::info member{};
	std::size_t     offset = 0;
	bool            found  = false;
	// Which element, when the path subscripted the leaf. -1 means the whole member.
	int             elem   = -1;
};

// A member named anywhere in cls or its bases, with base subobject offsets accumulated so the
// result is relative to cls. offset_of alone is relative to the declaring class, and
// nonstatic_data_members_of does not cross a base -- so without this a table cannot name a
// member it inherits, which is what every _NOBASE table does.
consteval MemberRef find_member( std::meta::info cls, std::string_view id, std::size_t base = 0 )
{
	// has_identifier has to be checked first: an anonymous union member or an unnamed bitfield has
	// no identifier, and identifier_of throws on it before the loop ever reaches the target.
	for ( auto m : std::meta::nonstatic_data_members_of( cls, std::meta::access_context::unchecked() ) )
		if ( std::meta::has_identifier( m ) && std::meta::identifier_of( m ) == id )
			return MemberRef{ m, base + byte_offset_of( m ), true };

	for ( auto b : std::meta::bases_of( cls, std::meta::access_context::unchecked() ) )
	{
		const MemberRef r = find_member( std::meta::type_of( b ), id,
		                                 base + static_cast<std::size_t>( std::meta::offset_of( b ).bytes ) );
		if ( r.found ) return r;
	}
	return MemberRef{};
}

// A dotted path names a member of a member: RECVINFO( m_fog.enable ) is one prop whose offset is
// the sum along the path and whose size is the leaf's. The macro spells the prop's wire name as
// the path itself, so resolving the path is all that is needed to reproduce it.
// A segment may subscript its member: RECVINFO( m_audio.localSound[0] ) names one element. Splits
// "localSound[3]" into the identifier and 3, or leaves the index at -1.
struct PathSegment
{
	std::string_view id;
	int              elem = -1;
};

consteval PathSegment split_subscript( std::string_view seg )
{
	const std::size_t open = seg.find( '[' );
	if ( open == std::string_view::npos || seg.back() != ']' ) return PathSegment{ seg, -1 };
	int n = 0;
	for ( std::size_t i = open + 1; i + 1 < seg.size(); ++i )
	{
		if ( seg[i] < '0' || seg[i] > '9' ) return PathSegment{ seg, -1 };
		n = n * 10 + ( seg[i] - '0' );
	}
	return PathSegment{ seg.substr( 0, open ), n };
}

consteval MemberRef find_member_path( std::meta::info cls, std::string_view path )
{
	MemberRef       r{};
	std::size_t     base = 0;
	std::meta::info cur  = cls;
	for ( ;; )
	{
		const std::size_t      dot  = path.find( '.' );
		const bool             last = dot == std::string_view::npos;
		const PathSegment      seg  = split_subscript( last ? path : path.substr( 0, dot ) );

		r = find_member( cur, seg.id, base );
		if ( !r.found )
			return MemberRef{};
		// A subscript shifts the offset by whole elements and narrows the type to the element.
		if ( seg.elem >= 0 )
			r.offset += (std::size_t)seg.elem
			          * std::meta::size_of( std::meta::remove_extent(
			                unwrap( std::meta::type_of( r.member ) ) ) );
		if ( last )
		{
			r.elem = seg.elem;
			return r;
		}

		base = r.offset;
		cur  = unwrap( std::meta::type_of( r.member ) );
		if ( seg.elem >= 0 ) cur = unwrap( std::meta::remove_extent( cur ) );
		path = path.substr( dot + 1 );
	}
}

consteval MemberRef require_member( std::meta::info cls, const name_t &id )
{
	const MemberRef r = find_member_path( cls, std::string_view{ id.data, __builtin_strlen( id.data ) } );
	if ( !r.found )
		throw std::meta::exception( "no member of that name in the class or its bases", cls );
	return r;
}

// `members_of` reports only what a class declares itself, while the legacy
// `Base::m_pClassSendTable` resolves through inheritance, so this has to recurse.
consteval bool has_static_member( std::meta::info cls, std::string_view id )
{
	for ( auto m : std::meta::members_of( cls, std::meta::access_context::unchecked() ) )
		if ( std::meta::is_variable( m ) && std::meta::identifier_of( m ) == id ) return true;
	for ( auto b : std::meta::bases_of( cls, std::meta::access_context::unchecked() ) )
		if ( has_static_member( std::meta::type_of( b ), id ) ) return true;
	return false;
}

// A named static data member of `cls`, found with unchecked access so a private one is still
// reachable. DECLARE_CLIENTCLASS leaves m_pClassRecvTable in whatever access section the class
// happens to be in, and the legacy macro only reaches it because ClientClassInit is a friend.
consteval std::meta::info static_member_of( std::meta::info cls, std::string_view id )
{
	for ( auto m : std::meta::members_of( cls, std::meta::access_context::unchecked() ) )
		if ( std::meta::is_variable( m ) && std::meta::identifier_of( m ) == id ) return m;
	for ( auto b : std::meta::bases_of( cls, std::meta::access_context::unchecked() ) )
		if ( has_static_member( std::meta::type_of( b ), id ) )
			return static_member_of( std::meta::type_of( b ), id );
	throw std::meta::exception( "no static member of that name", cls );
}

// The base carrying `id`, not merely the first base: a networked class may list a non-networked
// mixin first, and bases_of()[0] then names something with no table pointer at all.
consteval std::meta::info base_with_static_member( std::meta::info cls, std::string_view id )
{
	for ( auto b : std::meta::bases_of( cls, std::meta::access_context::unchecked() ) )
	{
		const std::meta::info t = std::meta::type_of( b );
		if ( has_static_member( t, id ) ) return t;
	}
	throw std::meta::exception( "no base with that static member", cls );
}

// The types of every From<...> annotation on a class, in order, so a caller can read their
// template arguments.
consteval std::vector<std::meta::info> from_annotations( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( std::meta::has_template_arguments( t ) && std::meta::template_of( t ) == ^^From )
			out.push_back( t );
	}
	return out;
}

// PredFrom<> entries, the prediction counterpart of from_annotations.
consteval std::vector<std::meta::info> pred_from_annotations( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( std::meta::has_template_arguments( t ) && std::meta::template_of( t ) == ^^PredFrom )
			out.push_back( t );
	}
	return out;
}

// KeyFrom<> entries, the datamap counterpart.
consteval std::vector<std::meta::info> key_from_annotations( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( std::meta::has_template_arguments( t ) && std::meta::template_of( t ) == ^^KeyFrom )
			out.push_back( t );
	}
	return out;
}

// The SubTable<> entries of a class, in declaration order, split by where they sit in the table.
consteval std::vector<std::meta::info> utl_vecs( std::meta::info cls, const name_t &table )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( !std::meta::has_template_arguments( t ) || std::meta::template_of( t ) != ^^UtlVec )
			continue;
		const auto args = std::meta::template_arguments_of( t );
		if ( same_name( std::meta::extract<name_t>( args[4] ), table ) )
			out.push_back( t );
	}
	return out;
}

consteval std::vector<std::meta::info> bare_arrays( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( std::meta::has_template_arguments( t ) && std::meta::template_of( t ) == ^^BareArray )
			out.push_back( t );
	}
	return out;
}

consteval std::vector<std::meta::info> bare_props( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( std::meta::has_template_arguments( t ) && std::meta::template_of( t ) == ^^Bare )
			out.push_back( t );
	}
	return out;
}

consteval std::vector<std::meta::info> sub_tables( std::meta::info cls, bool at_head )
{
	std::vector<std::meta::info> out;
	for ( auto a : std::meta::annotations_of( cls ) )
	{
		const std::meta::info t = std::meta::remove_cv( std::meta::type_of( a ) );
		if ( !std::meta::has_template_arguments( t ) || std::meta::template_of( t ) != ^^SubTable )
			continue;
		const auto args = std::meta::template_arguments_of( t );
		if ( std::meta::extract<bool>( args[3] ) == at_head )
			out.push_back( t );
	}
	return out;
}

// The From<> entries belonging to one of the class's tables. A From<> carries its Net as a template
// argument, so which table it belongs to is read from there, exactly as for an annotated member.
consteval std::vector<std::meta::info> from_annotations_in( std::meta::info cls, const name_t &table,
                                                            WireSide want )
{
	std::vector<std::meta::info> out;
	for ( auto t : from_annotations( cls ) )
	{
		const auto args = std::meta::template_arguments_of( t );
		const Net n = std::meta::extract<Net>( args[1] );
		if ( same_name( n.table, table ) && on_side( n, want ) )
			out.push_back( t );
	}
	return out;
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

// The class's own table, as opposed to its secondary ones. `get<NetTable>` returns whichever
// annotation comes first, which stopped being meaningful once a class could carry several: the
// primary table picked up a secondary's `.base = false` and silently lost its baseclass prop. The
// primary is the one nothing else singles out -- no member, no From<>, and no SubTable<>. The
// SubTable case matters because a secondary table can be *empty*: C_BaseCombatCharacter's
// DT_BCCNonLocalPlayerExclusive has no members at all, so without it that table and the real
// primary are indistinguishable and whichever was declared first would win.
template <class C>
consteval NetTable primary_net_table()
{
	std::vector<name_t> claimed;
	for ( auto m : tagged_members<Net>( ^^C ) )
		if ( !get<Net>( m ).table.empty() )
			claimed.push_back( get<Net>( m ).table );
	for ( bool head : { true, false } )
		for ( auto t : sub_tables( ^^C, head ) )
			claimed.push_back( std::meta::extract<name_t>(
			    std::meta::template_arguments_of( t )[0] ) );
	for ( auto t : from_annotations( ^^C ) )
	{
		const Net n = std::meta::extract<Net>( std::meta::template_arguments_of( t )[1] );
		if ( !n.table.empty() )
			claimed.push_back( n.table );
	}
	for ( auto t : all<NetTable>( ^^C ) )
	{
		bool secondary = false;
		for ( const auto &c : claimed )
			if ( same_name( c, t.name ) ) { secondary = true; break; }
		if ( !secondary )
			return t;
	}
	throw std::meta::exception( "every NetTable on this class is claimed by a member", ^^C );
}


// Member functions carrying A, in declaration order. DEFINE_INPUTFUNC names a function rather
// than a field, so inputs need this rather than tagged_members.
template <class A>
consteval std::vector<std::meta::info> tagged_functions( std::meta::info cls )
{
	std::vector<std::meta::info> out;
	for ( auto m : std::meta::members_of( cls, std::meta::access_context::unchecked() ) )
		if ( std::meta::is_function( m ) && has<A>( m ) ) out.push_back( m );
	return out;
}

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
