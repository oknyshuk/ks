// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Emits a SendTable for an annotated class. Derives the arguments and then calls the same
// SendPropXXX factories the macros call, so byte-identity with the legacy table reduces to
// argument derivation -- the only part reflection is actually replacing.
//
// An unhandled prop kind is a compile error, never a wrong prop.
//
// Known legacy quirk reproduced deliberately: SendPropEHandle's third parameter is `flags`,
// but every call site passes SENDINFO's (name, offset, size) positionally, so every ehandle
// prop in the tree carries flags = sizeof(CBaseHandle) = SPROP_NOSCALE. Harmless for an int
// prop, but it is in the descriptor, so byte-identity requires emitting it too. An ehandle
// member may therefore not carry Net::flags.

#ifndef KS_REFLECT_SENDTABLE_H
#define KS_REFLECT_SENDTABLE_H

#include "reflect.h"
#include "dt_send.h"
#include "dt_utlvector_send.h"
#include "sendproxy.h"
#include "networkvar.h"

#include <span>
#include <vector>

namespace ks::reflect::net
{

// A SubTable<>'s Table argument is `auto`, so one annotation vocabulary serves both
// directions: the send side names &DT_X::g_SendTable and the receive side
// &DT_X::g_RecvTable. Each emitter therefore takes only the annotations whose pointer type is
// its own -- extracting the other side's does not merely give the wrong answer, it fails to
// compile.
consteval std::vector<std::meta::info> send_sub_tables( std::meta::info cls, bool at_head )
{
	std::vector<std::meta::info> out;
	for ( auto t : sub_tables( cls, at_head ) )
		if ( std::meta::remove_cv( std::meta::type_of( std::meta::template_arguments_of( t )[1] ) )
		     == ^^SendTable * )
			out.push_back( t );
	return out;
}

enum class PropKind
{
	Int, Float, Time, Vector, VectorXY, QAngles, Angle, ModelIndex, Bool, EHandle, String,
	StringT,
	Table,       // SendPropDataTable
	Array,       // SendPropArray3, which takes its element prop by value
	ArrayInner,  // InternalSendPropArray, whose element prop is a sibling in the table
	UtlVector,   // SendPropUtlVector, which also takes its element prop by value
	Exclude,     // SendPropExclude
};

// Everything a SendProp factory needs, as plain data. This is the whole point of the split: the
// table is described at compile time (see table_desc below) and make_prop turns one description
// into one engine prop at load time. Only the factories need SendProp, and SendProp has a virtual
// destructor, so it can never be the compile-time artifact itself.
struct PropDesc
{
	const char *name  = nullptr;
	PropKind    kind  = PropKind::Int;
	int         offset = 0;        // networkvar flags OR'd into the high bits, as SENDINFO does
	int         size   = 0;
	int         bits   = -1;
	int         flags  = 0;
	float       low    = 0.f;
	float       high   = kHighDefault;
	int         elements = 1;
	int         stride   = 0;
	SendVarProxyFn proxy = nullptr;   // from a Proxy<F> annotation; null means the factory default
	int         priority = -1;        // -1 means SENDPROP_DEFAULT_PRIORITY

	// Table. The table is named by a thunk because the three ways to reach one differ in kind:
	// a base class's m_pClassSendTable is read at load time, an embedded class's is &table<T>(),
	// and a SubTable<> annotation carries the pointer as a constant.
	SendTable *(*subtable)() = nullptr;
	// Null means the factory's own default, which also carries SPROP_PROXY_ALWAYS_YES; passing a
	// null proxy explicitly would override both, so the two cases cannot share one field.
	SendTableProxyFn tableProxy = nullptr;

	ArrayLengthSendProxyFn lenProxy = nullptr;   // ArrayInner
	EnsureCapacityFn        ensure   = nullptr;  // UtlVector
	int                     max      = 0;        // UtlVector: nMaxElements
	const char             *excludeProp = nullptr;  // Exclude: the prop being excluded

	// Array and UtlVector take their element prop by value, so the element is one more descriptor
	// in the same array and this is its index. It is marked `nested` so the loop that builds the
	// table's props knows not to emit it as a prop of its own.
	int  elem   = -1;
	bool nested = false;
};

// The wrapper's own type, before unwrap(), carries the CNetworkVector/XYZ/XY_SeparateZ
// distinction that SendPropFloat refuses to be wrong about.
template <class W>
consteval int networkvar_flags()
{
	if constexpr ( requires { W::GetNetworkVarFlags(); } ) return W::GetNetworkVarFlags();
	else return 0;
}

consteval PropKind kind_of_tag( fieldtype_t tag, std::meta::info payload, std::meta::info where )
{
	switch ( tag )
	{
	case FIELD_FLOAT:            return PropKind::Float;
	case FIELD_TIME:             return PropKind::Time;
	case FIELD_BOOLEAN:          return PropKind::Bool;
	case FIELD_EHANDLE:          return PropKind::EHandle;
	case FIELD_STRING:           return PropKind::StringT;
	case FIELD_EMBEDDED:         return PropKind::Table;
	case FIELD_MODELINDEX:       return PropKind::ModelIndex;
	// color32 goes on the wire as a 32-bit int through a packing proxy.
	case FIELD_COLOR32:          return PropKind::Int;
	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:  return payload == ^^QAngle ? PropKind::QAngles : PropKind::Vector;
	case FIELD_INTEGER:
	case FIELD_SHORT:
	case FIELD_CHARACTER:
	case FIELD_INTEGER64:
	case FIELD_TICK:             return PropKind::Int;
	default:
		throw std::meta::exception( "no send-prop kind for this field tag", where );
	}
}

consteval PropKind kind_of_member( std::meta::info m, const Net &n )
{
	const std::meta::info t = std::meta::type_of( m );
	// One element of an array member is a scalar prop of the element type, so everything below has
	// to reason about the element rather than the array.
	const std::meta::info te = n.elem >= 0 ? std::meta::remove_extent( unwrap( t ) ) : t;

	if ( is_string_member( t ) ) return PropKind::String;
	if ( is_array_member( t ) && n.elem < 0 ) return PropKind::Array;
	// One component of a Vector is a float, and one component of a QAngle is an angle -- which is
	// why this cannot be a blanket PropKind::Float: SENDINFO_VECTORELEM on a QAngle is written with
	// SendPropAngle, whose range and SPROP_ROUNDDOWN a float prop does not reproduce.
	if ( n.index >= 0 && n.enc == WireEnc::Auto )
		return unwrap( te ) == ^^QAngle ? PropKind::Angle : PropKind::Float;
	switch ( n.enc )
	{
	case WireEnc::Angle:      return PropKind::Angle;
	case WireEnc::ModelIndex: return PropKind::ModelIndex;
	case WireEnc::Vector:     return PropKind::Vector;
	case WireEnc::VectorXY:   return PropKind::VectorXY;
	case WireEnc::QAngles:    return PropKind::QAngles;
	// A float member can still be sent as an int: DT_AnimTimeMustBeFirst packs m_flAnimTime into 8
	// bits with SendPropInt. Which factory the wire wants is not a property of the C++ type.
	case WireEnc::Int:        return PropKind::Int;
	// And the reverse: CTimeline's int array m_nValueCounts is sent with SendPropFloat.
	case WireEnc::Float:      return PropKind::Float;
	default:             break;
	}
	const fieldtype_t tag = tag_of( m );
	// A bool with an explicit width is a SendPropInt, not a SendPropBool: SendPropBool hardcodes
	// one bit, and DT_Embers sends its flag in two.
	if ( tag == FIELD_BOOLEAN && n.bits != kBitsDefault && n.bits != 1 ) return PropKind::Int;
	return kind_of_tag( tag, unwrap( te ), m );
}

// The wire name: an override if given, else the identifier. Takes the Net explicitly so a From<>
// entry can name a member it does not own.
consteval const char *wire_name_of( std::meta::info m, const Net &n )
{
	if ( !n.wire.empty() ) return intern( n.wire );
	// SENDINFO_VECTORELEM stringifies to "m_angRotation[1]", so an indexed prop is named for its
	// component rather than for the member.
	if ( n.index >= 0 ) return indexed_wire_name( m, std::meta::identifier_of( m ), n.index );
	return intern( std::meta::identifier_of( m ) );
}

consteval PropKind element_kind_of( std::meta::info m )
{
	// The encoding overrides the element type as well as the member type: CTimeline sends its
	// int array m_nValueCounts through SendPropFloat, one element at a time.
	if ( get<Net>( m ).enc == WireEnc::Float ) return PropKind::Float;
	if ( get<Net>( m ).enc == WireEnc::Int )   return PropKind::Int;
	if ( get<Net>( m ).enc == WireEnc::String ) return PropKind::String;
	const std::meta::info elem = std::meta::remove_extent( unwrap( std::meta::type_of( m ) ) );
	return kind_of_tag( tag_of_type( elem ), elem, m );
}

// The value the legacy call site would have passed when the annotation says nothing. Float and
// vector call sites overwhelmingly pass 0; SendPropInt's own default is -1, which auto-sizes
// from sizeofVar; SendPropAngle's is 32. Any class where the guess is wrong fails the
// byte-compare, loudly.
consteval int default_bits( PropKind k )
{
	if ( k == PropKind::Int ) return -1;
	if ( k == PropKind::Angle ) return 32;
	return 0;
}

// One entry, from a member reflection plus the wire parameters that apply to it. The offset is
// passed in because a From<> entry's offset is relative to the sending class, not the declaring
// one, and only the caller knows which.
template <std::meta::info M>
consteval PropDesc desc_of( Net n, int offset, SendVarProxyFn proxy )
{
	const std::meta::info t = std::meta::type_of( M );

	PropDesc d;
	d.kind   = kind_of_member( M, n );
	d.name   = wire_name_of( M, n );
	d.offset = offset;
	d.bits   = n.bits == kBitsDefault ? default_bits( d.kind ) : n.bits;
	d.flags  = n.flags;
	d.low    = n.low;
	d.high   = n.high;
	d.proxy  = proxy;
	d.priority = n.priority;

	if ( d.kind == PropKind::Array || d.kind == PropKind::String )
	{
		d.elements = static_cast<int>( array_extent_of( t ) );
		d.stride   = static_cast<int>( array_element_size( t ) );
		d.size     = d.kind == PropKind::String ? d.elements : d.stride;
		// SENDINFO_ARRAY/_ARRAY3 do not OR in the networkvar flags the way SENDINFO does.
		if ( d.kind == PropKind::Array ) d.offset &= SENDPROP_OFFSET_MASK;
	}
	else
	{
		// An element prop is sized by the element, not by the whole array.
		d.size = static_cast<int>( n.elem >= 0 ? array_element_size( t ) : std::meta::size_of( t ) );
		// A component prop is one lane of a Vector or QAngle, both of which are three of them.
		if ( n.index >= 0 )
			d.size = static_cast<int>( std::meta::size_of( unwrap( t ) ) ) / 3;
		// SENDINFO_STRUCTARRAYELEM takes the raw offset, unlike SENDINFO_STRUCTELEM.
		if ( n.elem >= 0 ) d.offset &= SENDPROP_OFFSET_MASK;
	}

	if ( d.kind == PropKind::EHandle && d.flags != 0 )
		throw std::meta::exception( "ehandle props cannot carry Net::flags; see header", M );

	return d;
}

// SENDINFO ORs the networkvar flags into the offset; SENDINFO_ARRAY and friends do not.
template <std::meta::info M>
consteval int offset_with_nv_flags( std::size_t off )
{
	using Wrapper = typename [: std::meta::type_of( M ) :];
	return static_cast<int>( off )
	     | ( networkvar_flags<Wrapper>() << SENDPROP_NETWORKVAR_FLAGS_SHIFT );
}

template <std::meta::info M>
consteval SendVarProxyFn proxy_of()
{
	if constexpr ( has_proxy( M, WireSide::Send ) ) return std::meta::extract<SendVarProxyFn>( proxy_arg_of( M, WireSide::Send ) );
	else return nullptr;
}

// One description to one engine prop. `all` resolves the element of an Array or a UtlVector,
// which is a descriptor in the same array rather than something owned.
SendProp make_prop( const PropDesc &d, std::span<const PropDesc> all );

// One Send/RecvPropArray2 pair. The element prop is a prop of the table itself; the array prop
// follows it. Template arguments again, for the P2564 reason above.
template <name_t ElemName, name_t ArrayName, int Count, int Stride, int Size,
          Net N, auto ElemFn, auto LenFn>
constexpr void push_bare_array( std::vector<PropDesc> &out )
{
	constexpr const char *en = intern( ElemName );
	constexpr const char *an = intern( ArrayName );
	PropDesc d;
	d.name  = en;
	d.kind  = N.enc == WireEnc::Float ? PropKind::Float : PropKind::Int;
	d.offset = 0;
	d.size  = Size;
	d.bits  = N.bits;
	d.flags = N.flags;
	d.proxy = ElemFn;
	out.push_back( d );

	PropDesc a;
	a.kind     = PropKind::ArrayInner;
	a.name     = an;
	a.elements = Count;
	a.stride   = Stride;
	a.lenProxy = LenFn;
	out.push_back( a );
}

template <class C, name_t Member, int Max, auto Table, auto Fn>
constexpr void push_utl_vec( std::vector<PropDesc> &out )
{
	constexpr auto ref = require_member( ^^C, Member );
	constexpr const char *nm = intern( Member );
	constexpr int off = (int)ref.offset;
	using Vec = typename [: std::meta::type_of( ref.member ) :];
	using WV  = WireVec<Vec>;

	// The element prop is unnamed and at offset 0 either way; a null Table means the elements are
	// scalars, whose kind comes from the vector's element type (DT_SceneEntity holds EHANDLEs).
	if constexpr ( Table == nullptr )
	{
		constexpr std::meta::info et = ^^typename WV::elem_type;
		constexpr PropKind ek = kind_of_tag( tag_of_type( et ), et, ^^C );
		PropDesc ed;
		ed.kind   = ek;
		ed.nested = true;
		out.push_back( ed );
	}
	else
	{
		PropDesc ed;
		ed.kind     = PropKind::Table;
		ed.subtable = +[]() -> SendTable * { return Table; };
		ed.nested   = true;
		out.push_back( ed );
	}

	PropDesc d;
	d.kind   = PropKind::UtlVector;
	d.name   = nm;
	d.offset = off;
	d.size   = (int)sizeof( typename WV::elem_type );
	d.ensure = WV::ensure();
	d.max    = Max;
	d.elem   = (int)out.size() - 1;
	out.push_back( d );
}

template <class C> SendTable &table();

// A class may send several tables: CBaseCombatCharacter has DT_BCCLocalPlayerExclusive and
// DT_BCCNonLocalPlayerExclusive besides DT_BaseCombatCharacter, and CBasePlayer sends
// DT_LocalPlayerExclusive as its "localdata" prop. Each member says which one it belongs to, so
// the wire spec stays on the member; a table is then the members that name it.
template <name_t Table, class C>
consteval const NetTable table_annotation()
{
	for ( auto t : all<NetTable>( ^^C ) )
		if ( same_name( t.name, Table ) )
			return t;
	throw std::meta::exception( "no NetTable annotation names that table", ^^C );
}

// One member's contribution to a table. The primary and named-table generators each had their own
// copy of this, and they diverged: the named one called make_prop directly, so the first array
// member in a secondary table (CCSPlayer::m_bPlayerDominated) exited 70 on an unhandled kind.
template <std::meta::info M>
constexpr void push_member_prop( std::vector<PropDesc> &out )
{
	// Indexed props are handled here, as on the recv side, not by build_desc's loop.
	if constexpr ( !indexed_nets<M>( WireSide::Send ).empty() )
	{
		template for ( constexpr auto n : std::define_static_array( indexed_nets<M>( WireSide::Send ) ) )
			out.push_back( desc_of<M>(
			    n, offset_with_nv_flags<M>( byte_offset_of( M )
			        + n.index * (int)( std::meta::size_of( unwrap( std::meta::type_of( M ) ) ) / 3 ) ),
			    proxy_of<M>() ) );
		return;
	}

	constexpr Net n = get<Net>( M );
	constexpr PropDesc d = desc_of<M>( n, offset_with_nv_flags<M>( byte_offset_of( M ) ),
	                                   proxy_of<M>() );
	if constexpr ( d.kind == PropKind::Table )
	{
		using Sub = typename [: embedded_class_of( std::meta::type_of( M ) ) :];
		PropDesc t = d;
		t.offset   = d.offset & SENDPROP_OFFSET_MASK;
		t.subtable = +[]() -> SendTable * { return &table<Sub>(); };
		out.push_back( t );
	}
	else if constexpr ( d.kind == PropKind::Array )
	{
		// Both array forms want the same element description; they differ in where it goes.
		constexpr PropDesc elem = [] {
			constexpr Net mn = get<Net>( M );
			PropDesc e = desc_of<M>( mn, byte_offset_of( M ), proxy_of<M>() );
			e.kind = element_kind_of( M );
			e.size = e.stride;
			e.bits = mn.bits == kBitsDefault ? default_bits( e.kind ) : mn.bits;
			return e;
		}();

		if constexpr ( n.varlen )
		{
			// SendPropArray == SendPropVariableLengthArray: the element template is a real prop of the
			// table, carrying the array's own name and offset, and the array prop follows it.
			PropDesc e = elem;
			e.offset &= SENDPROP_OFFSET_MASK;
			out.push_back( e );

			PropDesc a = d;
			a.kind     = PropKind::ArrayInner;
			a.offset   = 0;   // InternalSendPropArray takes no offset
			a.lenProxy = nullptr;
			out.push_back( a );
		}
		else
		{
			// SENDINFO_ARRAY3: the element prop carries the array's name and a zero offset;
			// SendPropArray3 renumbers and re-offsets the copies it makes.
			PropDesc a = d;
			a.offset = d.offset & SENDPROP_OFFSET_MASK;
			a.elem   = (int)out.size();
			PropDesc e = elem;
			e.nested = true;
			out.push_back( e );
			out.push_back( a );
		}
	}
	else
	{
		out.push_back( d );
	}
}

// One table's descriptions become the engine's props, in order. A nested descriptor is the input
// to the prop that owns it, never a prop of the table itself.
inline std::vector<SendProp> materialize( std::span<const PropDesc> all )
{
	std::vector<SendProp> out;
	out.reserve( all.size() );
	for ( const PropDesc &d : all )
		if ( !d.nested ) out.push_back( make_prop( d, all ) );
	return out;
}

// A prop that sends a sub-table. The table is reached through a thunk because the three ways to
// name one differ in kind: a base class's m_pClassSendTable is read at load time, an embedded
// class's is &table<T>(), and a SubTable<> annotation carries the pointer as a constant.
constexpr PropDesc table_prop( const char *name, int offset, SendTable *(*get)(), SendTableProxyFn proxy )
{
	PropDesc d;
	d.kind       = PropKind::Table;
	d.name       = name;
	d.offset     = offset;
	d.subtable   = get;
	d.tableProxy = proxy;
	return d;
}

// Every table's description, computed entirely at compile time. A class may send several: the
// primary table is the NetTable annotation no member singles out, and every member that names
// another table belongs to that one instead. Only a primary table carries the excludes, the
// string-named sub-tables and the indexed Nets.
//
// The named tables used to be a second, smaller copy of this function, and the copies diverged:
// the copy built its props by hand, so the first array member in a secondary table exited 70 on a
// kind it did not handle.
template <class C, name_t Table = name_t{}, bool Primary = false>
consteval std::vector<PropDesc> build_desc()
{
	std::vector<PropDesc> out;
	constexpr NetTable nt = Primary ? primary_net_table<C>() : table_annotation<Table, C>();

	// The base is taken from bases_of rather than from the class's BaseClass typedef: that
	// typedef is private in some classes, and the legacy macro only reaches it because
	// ServerClassInit is a friend. m_pClassSendTable itself is public.
	if constexpr ( nt.base )
	{
		using Base = typename [: base_with_static_member( ^^C, "m_pClassSendTable" ) :];
		// The name has to come from intern(): a pointer to a string literal cannot be a reflection
		// constant, only one to a define_static_string.
		out.push_back( table_prop( intern( std::string_view( "baseclass" ) ), 0,
		                           +[]() -> SendTable * { return Base::m_pClassSendTable; },
		                           SendProxy_DataTableToDataTable ) );
	}

	// Send/RecvPropArray2 pairs, which have no member behind them.
	template for ( constexpr auto ba : std::define_static_array( bare_arrays( ^^C ) ) )
	{
		constexpr auto aa = std::define_static_array( std::meta::template_arguments_of( ba ) );
		push_bare_array<([: aa[0] :]), ([: aa[1] :]), ([: aa[2] :]), ([: aa[3] :]),
		                ([: aa[4] :]), ([: aa[5] :]), ([: aa[6] :]), ([: aa[7] :])>( out );
	}

	// CUtlVector props.
	template for ( constexpr auto uv : std::define_static_array( utl_vecs( ^^C, Table ) ) )
	{
		constexpr auto ua = std::define_static_array( std::meta::template_arguments_of( uv ) );
		push_utl_vec<C, ([: ua[0] :]), ([: ua[1] :]), ([: ua[2] :]), ([: ua[3] :])>( out );
	}

	// Excludes sit at the head of the table, which is where the macro lists put them. They are the
	// class's, so a member's table never carries them.
	if constexpr ( Primary )
		template for ( constexpr auto ex : std::define_static_array( all<Exclude>( ^^C ) ) )
	{
		PropDesc d;
		d.kind        = PropKind::Exclude;
		d.name        = intern( ex.table );
		d.excludeProp = intern( ex.prop );
		out.push_back( d );
	}

	// A sub-table prop named by a string rather than derived from a member.
	if constexpr ( Primary )
		template for ( constexpr auto st : std::define_static_array( send_sub_tables( ^^C, true ) ) )
	{
		constexpr auto sa = std::define_static_array( std::meta::template_arguments_of( st ) );
		// Omitting the proxy means the factory's default, SendProxy_DataTableToDataTable,
		// which also carries SPROP_PROXY_ALWAYS_YES; passing nullptr would override both.
		constexpr SendTableProxyFn fn = extract_fn<SendTableProxyFn>( sa[2] );
		constexpr SendTable *pTable = std::meta::extract<SendTable *>( sa[1] );
		out.push_back( table_prop( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
		                           +[]() -> SendTable * { return pTable; }, fn ) );
	}

	// Only the members that do not single out another of the class's tables.
	template for ( constexpr auto m : std::define_static_array( members_in<Table, C>( WireSide::Send ) ) )
		push_member_prop<m>( out );

	// Members declared in a base that this table sends itself. Appended rather than
	// interleaved: the compare is keyed by name, so position does not matter.
	template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, Table, WireSide::Send ) ) )
	{
		constexpr auto args = std::define_static_array( std::meta::template_arguments_of( ann ) );
		// A dotted path is the prop's wire name, as SENDINFO/RECVINFO stringify it.
		constexpr auto ref  = require_member( ^^C, std::meta::extract<name_t>( args[0] ) );
		// A bracketed path resolved to one element; the Net has to say so, because the kind
		// and the size are the element's, not the array's.
		constexpr Net  n    = with_elem( wire_from_path( std::meta::extract<Net>( args[1] ),
		                                                std::meta::extract<name_t>( args[0] ) ),
		                                 ref.elem );
		// An inherited member can be indexed too: CFuncRotating re-sends the three components
		// of CBaseEntity's m_angRotation. desc_of already names and sizes a component; the
		// offset is the one thing it cannot know, because it is relative to the sending class.
		constexpr int comp = n.index < 0 ? 0
		    : n.index * (int)( std::meta::size_of( unwrap( std::meta::type_of( ref.member ) ) ) / 3 );
		constexpr int off = n.raw ? (int)ref.offset + comp
		                          : offset_with_nv_flags<ref.member>( (int)ref.offset + comp );
		constexpr PropDesc d = desc_of<ref.member>(
		    n, off, extract_fn<SendVarProxyFn>( args[2] ) );

		if constexpr ( d.kind == PropKind::Table )
		{
			// Either the embedded type is migrated and we generate its table, or the legacy
			// sub-table was named in the annotation. Silently generating an empty table for an
			// unmigrated type would be the worst of the three.
			constexpr SendTable *pNamed = extract_fn<SendTable *>( args[3] );
			if constexpr ( pNamed != nullptr )
				out.push_back( table_prop( d.name, d.offset & SENDPROP_OFFSET_MASK,
				                           +[]() -> SendTable * { return pNamed; }, nullptr ) );
			else
			{
				// ^^Sub would reflect the *alias*, whose own annotation list is empty, so the
				// check has to be made on the reflection of the class itself.
				constexpr std::meta::info sub =
				    embedded_class_of( std::meta::type_of( ref.member ) );
				static_assert( has<NetTable>( sub ),
				    "embedded member is neither migrated nor given its legacy sub-table" );
				out.push_back( table_prop( d.name, d.offset & SENDPROP_OFFSET_MASK,
				                           +[]() -> SendTable * { return &table<typename [: sub :]>(); },
				                           nullptr ) );
			}
		}
		else if constexpr ( d.kind == PropKind::Array )
		{
			// An inherited array cannot be annotated on the member: that would put the prop in
			// the *base* class's table. CBasePlayer re-sends CBaseCombatCharacter::m_iAmmo.
			constexpr PropDesc elem = [&args] {
				PropDesc e = desc_of<ref.member>( n, (int)ref.offset,
				                                  extract_fn<SendVarProxyFn>( args[2] ) );
				e.kind = element_kind_of( ref.member );
				e.size = e.stride;
				e.bits = n.bits == kBitsDefault ? default_bits( e.kind ) : n.bits;
				return e;
			}();
			PropDesc a = d;
			a.offset = d.offset & SENDPROP_OFFSET_MASK;
			a.elem   = (int)out.size();
			PropDesc e = elem;
			e.nested = true;
			out.push_back( e );
			out.push_back( a );
		}
		else
		{
			static_assert( d.kind != PropKind::String,
			    "From<> does not build string props; annotate the member instead" );
			out.push_back( d );
		}
	}

	// A sub-table prop named by a string rather than derived from a member.
	if constexpr ( Primary )
		template for ( constexpr auto st : std::define_static_array( send_sub_tables( ^^C, false ) ) )
	{
		constexpr auto sa = std::define_static_array( std::meta::template_arguments_of( st ) );
		// Omitting the proxy means the factory's default, SendProxy_DataTableToDataTable,
		// which also carries SPROP_PROXY_ALWAYS_YES; passing nullptr would override both.
		constexpr SendTableProxyFn fn = extract_fn<SendTableProxyFn>( sa[2] );
		constexpr SendTable *pTable = std::meta::extract<SendTable *>( sa[1] );
		out.push_back( table_prop( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
		                           +[]() -> SendTable * { return pTable; }, fn ) );
	}
	return out;
}

template <class C>
inline constexpr std::span<const PropDesc> table_desc =
    std::define_static_array( build_desc<C, name_t{}, true>() );

template <class C>
std::vector<SendProp> &table_props()
{
	static std::vector<SendProp> props = materialize( table_desc<C> );
	return props;
}

template <class C, name_t Table = name_t{}>
inline constexpr std::span<const PropDesc> table_desc_in =
    std::define_static_array( build_desc<C, Table>() );

template <class C, name_t Table = name_t{}>
std::vector<SendProp> &table_props_in()
{
	static std::vector<SendProp> props = materialize( table_desc_in<C, Table> );
	return props;
}

template <class C, name_t Table>
SendTable &table_in()
{
	static SendTable t = [] {
		auto &p = table_props_in<C, Table>();
		return SendTable( p.data(), static_cast<int>( p.size() ), intern( Table ) );
	}();
	return t;
}

template <class C>
SendTable &table()
{
	static SendTable t = [] {
		auto &p = table_props<C>();
		return SendTable( p.data(), static_cast<int>( p.size() ), table_name<C>() );
	}();
	return t;
}

} // namespace ks::reflect::net


// Replaces IMPLEMENT_SERVERCLASS_ST plus its prop list once the gate has proven the generated
// table identical. The DT_x::g_SendTable global stays and is filled in a static initializer, which
// is what BEGIN_SEND_TABLE's ServerClassInit did -- REFERENCE_SEND_TABLE names that global, and 25
// tables reach another table through it.
#define IMPLEMENT_REFLECT_SERVERCLASS( className, tableName )                                    \
	namespace tableName { SendTable g_SendTable; }                                               \
	static const int g_##className##_ReflectSendInit = []                                        \
	{                                                                                            \
		auto &p = ks::reflect::net::table_props<className>();                                    \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static ServerClass g_##className##_ClassReg( #className, &tableName::g_SendTable );           \
	ServerClass *className::GetServerClass() { return &g_##className##_ClassReg; }                \
	SendTable *className::m_pClassSendTable = &tableName::g_SendTable;                             \
	int className::YouForgotToImplementOrDeclareServerClass() { return 0; }

// One of a class's *secondary* tables: the members that name it, rather than the class's own set.
// CBaseCombatCharacter sends DT_BCCLocalPlayerExclusive and DT_BCCNonLocalPlayerExclusive besides
// its own table, and each is a separate namespace global with no class registration behind it.
#define IMPLEMENT_REFLECT_TABLE_IN( className, tableName )                                        \
	namespace tableName { SendTable g_SendTable; }                                                       \
	static const int g_##className##_##tableName##_ReflectInit = []                               \
	{                                                                                            \
		auto &p = ks::reflect::net::table_props_in<className, #tableName>();                        \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                    \
		return 1;                                                                                \
	}();

// The table alone, for a class whose registration is written separately -- which is every form that
// opens a table without declaring a class: BEGIN_NETWORK_TABLE in shared code, and a bare
// BEGIN_SEND_TABLE or BEGIN_RECV_TABLE beside an IMPLEMENT_CLIENTCLASS, _FACTORY or _EVENT line.
// BEGIN_NETWORK_TABLE expands to BEGIN_SEND_TABLE on this side and BEGIN_RECV_TABLE on the other,
// so this is defined in both emitters under one name and the call site needs no guard.
// IMPLEMENT_SERVERCLASS_INTERNAL already supplies the ServerClass, GetServerClass,
// m_pClassSendTable and the forgot-to-declare stub; only the SendTable itself is missing. _NOBASE
// needs no separate macro: whether the baseclass prop is emitted is what NetTable.base records.
#define IMPLEMENT_REFLECT_TABLE( className, tableName )                                   \
	namespace tableName { SendTable g_SendTable; }                                               \
	static const int g_##className##_ReflectSendInit = []                                        \
	{                                                                                            \
		auto &p = ks::reflect::net::table_props<className>();                                    \
		tableName::g_SendTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();

#endif // KS_REFLECT_SENDTABLE_H
