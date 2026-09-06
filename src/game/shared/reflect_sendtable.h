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
#include "reflect_table_check.h"

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

enum PropKind
{
	PROP_INT, PROP_FLOAT, PROP_TIME, PROP_VECTOR, PROP_VECTORXY, PROP_QANGLES, PROP_ANGLE,
	PROP_MODELINDEX, PROP_BOOL, PROP_EHANDLE, PROP_STRING, PROP_STRINGT, PROP_TABLE, PROP_ARRAY,
};

struct PropDesc
{
	const char *name  = nullptr;
	PropKind    kind  = PROP_INT;
	int         offset = 0;        // networkvar flags OR'd into the high bits, as SENDINFO does
	int         size   = 0;
	int         bits   = -1;
	int         flags  = 0;
	float       low    = 0.f;
	float       high   = HIGH_DEFAULT;
	int         elements = 1;
	int         stride   = 0;
	SendVarProxyFn proxy = nullptr;   // from a Proxy<F> annotation; null means the factory default
	int         priority = -1;        // -1 means SENDPROP_DEFAULT_PRIORITY
};

// The wrapper's own type, before unwrap(), carries the CNetworkVector/XYZ/XY_SeparateZ
// distinction that SendPropFloat refuses to be wrong about.
template <class W>
consteval int networkvar_flags()
{
	if constexpr ( requires { W::GetNetworkVarFlags(); } ) return W::GetNetworkVarFlags();
	else return 0;
}

// NetworkVarEmbedded<T,...> derives from T instead of boxing it in m_Value, so the members
// belong to the base. Descend through any class that adds no members of its own.
consteval std::meta::info embedded_class_of( std::meta::info t )
{
	t = std::meta::remove_cv( t );
	if ( !std::meta::nonstatic_data_members_of( t, std::meta::access_context::unchecked() ).empty() )
		return t;
	auto bases = std::meta::bases_of( t, std::meta::access_context::unchecked() );
	if ( bases.size() == 1 ) return embedded_class_of( std::meta::type_of( bases[0] ) );
	return t;
}

consteval PropKind kind_of_tag( fieldtype_t tag, std::meta::info payload, std::meta::info where )
{
	switch ( tag )
	{
	case FIELD_FLOAT:            return PROP_FLOAT;
	case FIELD_TIME:             return PROP_TIME;
	case FIELD_BOOLEAN:          return PROP_BOOL;
	case FIELD_EHANDLE:          return PROP_EHANDLE;
	case FIELD_STRING:           return PROP_STRINGT;
	case FIELD_EMBEDDED:         return PROP_TABLE;
	case FIELD_MODELINDEX:       return PROP_MODELINDEX;
	// color32 goes on the wire as a 32-bit int through a packing proxy.
	case FIELD_COLOR32:          return PROP_INT;
	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:  return payload == ^^QAngle ? PROP_QANGLES : PROP_VECTOR;
	case FIELD_INTEGER:
	case FIELD_SHORT:
	case FIELD_CHARACTER:
	case FIELD_INTEGER64:
	case FIELD_TICK:             return PROP_INT;
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

	if ( is_string_member( t ) ) return PROP_STRING;
	if ( is_array_member( t ) && n.elem < 0 ) return PROP_ARRAY;
	// One component of a Vector is a float, and one component of a QAngle is an angle -- which is
	// why this cannot be a blanket PROP_FLOAT: SENDINFO_VECTORELEM on a QAngle is written with
	// SendPropAngle, whose range and SPROP_ROUNDDOWN a float prop does not reproduce.
	if ( n.index >= 0 && n.enc == ENC_AUTO )
		return unwrap( te ) == ^^QAngle ? PROP_ANGLE : PROP_FLOAT;
	switch ( n.enc )
	{
	case ENC_ANGLE:      return PROP_ANGLE;
	case ENC_MODELINDEX: return PROP_MODELINDEX;
	case ENC_VECTOR:     return PROP_VECTOR;
	case ENC_VECTORXY:   return PROP_VECTORXY;
	case ENC_QANGLES:    return PROP_QANGLES;
	// A float member can still be sent as an int: DT_AnimTimeMustBeFirst packs m_flAnimTime into 8
	// bits with SendPropInt. Which factory the wire wants is not a property of the C++ type.
	case ENC_INT:        return PROP_INT;
	// And the reverse: CTimeline's int array m_nValueCounts is sent with SendPropFloat.
	case ENC_FLOAT:      return PROP_FLOAT;
	default:             break;
	}
	const fieldtype_t tag = tag_of( m );
	// A bool with an explicit width is a SendPropInt, not a SendPropBool: SendPropBool hardcodes
	// one bit, and DT_Embers sends its flag in two.
	if ( tag == FIELD_BOOLEAN && n.bits != BITS_DEFAULT && n.bits != 1 ) return PROP_INT;
	return kind_of_tag( tag, unwrap( te ), m );
}

// The wire name: an override if given, else the identifier. Takes the Net explicitly so a From<>
// entry can name a member it does not own.
consteval const char *wire_name_of( std::meta::info m, const Net &n )
{
	if ( !n.wire.empty() ) return intern( n.wire );
	// SENDINFO_VECTORELEM stringifies to "m_angRotation[1]", so an indexed prop is named for its
	// component rather than for the member.
	if ( n.index >= 0 )
	{
		name_t indexed;
		const std::string_view id = std::meta::identifier_of( m );
		int k = 0;
		for ( ; k < (int)id.size() && k < 60; ++k ) indexed.data[k] = id[k];
		indexed.data[k++] = '[';
		indexed.data[k++] = (char)( '0' + n.index );
		indexed.data[k++] = ']';
		return intern( indexed );
	}
	return intern( std::meta::identifier_of( m ) );
}

consteval PropKind element_kind_of( std::meta::info m )
{
	// The encoding overrides the element type as well as the member type: CTimeline sends its
	// int array m_nValueCounts through SendPropFloat, one element at a time.
	if ( get<Net>( m ).enc == ENC_FLOAT ) return PROP_FLOAT;
	if ( get<Net>( m ).enc == ENC_INT )   return PROP_INT;
	if ( get<Net>( m ).enc == ENC_STRING ) return PROP_STRING;
	const std::meta::info elem = std::meta::remove_extent( unwrap( std::meta::type_of( m ) ) );
	return kind_of_tag( tag_of_type( elem ), elem, m );
}

// The value the legacy call site would have passed when the annotation says nothing. Float and
// vector call sites overwhelmingly pass 0; SendPropInt's own default is -1, which auto-sizes
// from sizeofVar; SendPropAngle's is 32. Any class where the guess is wrong fails the
// byte-compare, loudly.
consteval int default_bits( PropKind k )
{
	if ( k == PROP_INT ) return -1;
	if ( k == PROP_ANGLE ) return 32;
	return 0;
}

// One entry, from a member reflection plus the wire parameters that apply to it. The offset is
// passed in because a From<> entry's offset is relative to the sending class, not the declaring
// one, and only the caller knows which.
// The indexed Net annotations of a member, in declaration order. SENDINFO_VECTORELEM props must
// stay in order and adjacent: the engine detects the second and third components by their offsets.
template <std::meta::info M>
consteval std::vector<Net> indexed_nets()
{
	std::vector<Net> out;
	for ( const Net &n : all<Net>( M ) )
		if ( n.index >= 0 && on_side( n, WIRE_SEND ) )
			out.push_back( n );
	return out;
}

template <std::meta::info M>
consteval PropDesc desc_of( Net n, int offset, SendVarProxyFn proxy )
{
	const std::meta::info t = std::meta::type_of( M );

	PropDesc d;
	d.kind   = kind_of_member( M, n );
	d.name   = wire_name_of( M, n );
	d.offset = offset;
	d.bits   = n.bits == BITS_DEFAULT ? default_bits( d.kind ) : n.bits;
	d.flags  = n.flags;
	d.low    = n.low;
	d.high   = n.high;
	d.proxy  = proxy;
	d.priority = n.priority;

	if ( d.kind == PROP_ARRAY || d.kind == PROP_STRING )
	{
		d.elements = static_cast<int>( array_extent_of( t ) );
		d.stride   = static_cast<int>( array_element_size( t ) );
		d.size     = d.kind == PROP_STRING ? d.elements : d.stride;
		// SENDINFO_ARRAY/_ARRAY3 do not OR in the networkvar flags the way SENDINFO does.
		if ( d.kind == PROP_ARRAY ) d.offset &= SENDPROP_OFFSET_MASK;
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

	if ( d.kind == PROP_EHANDLE && d.flags != 0 )
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
	if constexpr ( has_proxy( M, WIRE_SEND ) ) return std::meta::extract<SendVarProxyFn>( proxy_arg_of( M, WIRE_SEND ) );
	else return nullptr;
}

SendProp make_prop( const PropDesc &d );

// One CUtlVector prop. Everything consteval happens in the template arguments: an ordinary call
// to a consteval function from the table lambda escalates the whole lambda to an immediate
// function (P2564) and the table then fails to build.
// One Send/RecvPropArray2 pair. Template arguments again, for the P2564 reason above.
template <name_t ElemName, name_t ArrayName, int Count, int Stride, int Size,
          Net N, auto ElemFn, auto LenFn>
void push_bare_array( std::vector<SendProp> &out )
{
	static constexpr const char *en = intern( ElemName );
	static constexpr const char *an = intern( ArrayName );
	PropDesc d;
	d.name  = en;
	d.kind  = N.enc == ENC_FLOAT ? PROP_FLOAT : PROP_INT;
	d.offset = 0;
	d.size  = Size;
	d.bits  = N.bits;
	d.flags = N.flags;
	d.proxy = ElemFn;
	out.push_back( make_prop( d ) );
	out.push_back( InternalSendPropArray( Count, Stride, an, LenFn ) );
}

template <class C, name_t Member, int Max, auto Table, auto Fn>
void push_utl_vec( std::vector<SendProp> &out )
{
	static constexpr auto ref = require_member( ^^C, Member );
	static constexpr const char *nm = intern( Member );
	static constexpr int off = (int)ref.offset;
	using Vec = typename [: std::meta::type_of( ref.member ) :];
	Vec probe;   // only to deduce the thunk, as SENDINFO_UTLVECTOR does
	// The element prop is unnamed and at offset 0 either way; a null Table means the elements are
	// scalars, whose kind comes from the vector's element type (DT_SceneEntity holds EHANDLEs).
	if constexpr ( Table == nullptr )
	{
		static constexpr std::meta::info et = ^^typename Vec::ElemType_t;
		static constexpr PropDesc ed = { nullptr, kind_of_tag( tag_of_type( et ), et, ^^C ), 0, 0 };
		out.push_back( SendPropUtlVector( nm, off, (int)sizeof( typename Vec::ElemType_t ),
		    GetEnsureCapacityTemplate( probe ), Max, make_prop( ed ) ) );
	}
	else
		out.push_back( SendPropUtlVector( nm, off, (int)sizeof( typename Vec::ElemType_t ),
		    GetEnsureCapacityTemplate( probe ), Max,
		    SendPropDataTable( nullptr, 0, Table ) ) );
}

template <class C> SendTable &table();

// A class may send several tables: CBaseCombatCharacter has DT_BCCLocalPlayerExclusive and
// DT_BCCNonLocalPlayerExclusive besides DT_BaseCombatCharacter, and CBasePlayer sends
// DT_LocalPlayerExclusive as its "localdata" prop. Each member says which one it belongs to, so
// the wire spec stays on the member; a table is then the members that name it.
template <name_t Table, class C>
consteval std::vector<std::meta::info> members_in()
{
	std::vector<std::meta::info> out;
	for ( auto m : tagged_members<Net>( ^^C ) )
		if ( same_name( get<Net>( m ).table, Table ) && on_side( get<Net>( m ), WIRE_SEND ) )
			out.push_back( m );
	return out;
}

template <name_t Table, class C>
consteval const NetTable table_annotation()
{
	for ( auto t : all<NetTable>( ^^C ) )
		if ( same_name( t.name, Table ) )
			return t;
	throw std::meta::exception( "no NetTable annotation names that table", ^^C );
}

template <class C>
consteval const char *table_name()
{
	if ( !primary_net_table<C>().name.empty() ) return intern( primary_net_table<C>().name );
	throw std::meta::exception( "class needs a NetTable annotation naming its send table", ^^C );
}

// One member's contribution to a table. The primary and named-table generators each had their own
// copy of this, and they diverged: the named one called make_prop directly, so the first array
// member in a secondary table (CCSPlayer::m_bPlayerDominated) exited 70 on an unhandled kind.
template <std::meta::info M>
void push_member_prop( std::vector<SendProp> &out )
{
	constexpr PropDesc d = desc_of<M>( get<Net>( M ),
	                                   offset_with_nv_flags<M>( byte_offset_of( M ) ),
	                                   proxy_of<M>() );
	if constexpr ( d.kind == PROP_TABLE )
	{
		using Sub = typename [: embedded_class_of( std::meta::type_of( M ) ) :];
		out.push_back( SendPropDataTable( d.name, d.offset & SENDPROP_OFFSET_MASK, &table<Sub>() ) );
	}
	else if constexpr ( d.kind == PROP_ARRAY && get<Net>( M ).varlen )
	{
		// SendPropArray == SendPropVariableLengthArray: the element template is a real prop of the
		// table, carrying the array's own name and offset, and the array prop follows it.
		constexpr PropDesc elem = [] {
			PropDesc e = desc_of<M>( get<Net>( M ), byte_offset_of( M ), proxy_of<M>() );
			e.kind = element_kind_of( M );
			e.size = e.stride;
			e.bits = get<Net>( M ).bits == BITS_DEFAULT ? default_bits( e.kind )
			                                           : get<Net>( M ).bits;
			e.offset &= SENDPROP_OFFSET_MASK;
			return e;
		}();
		out.push_back( make_prop( elem ) );
		out.push_back( InternalSendPropArray( d.elements, d.stride, d.name, nullptr ) );
	}
	else if constexpr ( d.kind == PROP_ARRAY )
	{
		// SENDINFO_ARRAY3: the element prop carries the array's name and a zero offset;
		// SendPropArray3 renumbers and re-offsets the copies it makes.
		constexpr PropDesc elem = [] {
			PropDesc e = desc_of<M>( get<Net>( M ), byte_offset_of( M ), proxy_of<M>() );
			e.kind = element_kind_of( M );
			e.size = e.stride;
			e.bits = get<Net>( M ).bits == BITS_DEFAULT ? default_bits( e.kind )
			                                           : get<Net>( M ).bits;
			return e;
		}();
		out.push_back( SendPropArray3( d.name, d.offset & SENDPROP_OFFSET_MASK,
		                               d.stride, d.elements, make_prop( elem ) ) );
	}
	else
	{
		out.push_back( make_prop( d ) );
	}
}

// The props of one named table. The primary table is the one no member singles out, so it is
// spelled as the empty name and picks up every member that does not name another.
template <class C, name_t Table = name_t{}>
std::vector<SendProp> &table_props_in()
{
	static std::vector<SendProp> props = []
	{
		std::vector<SendProp> out;
		constexpr NetTable nt = table_annotation<Table, C>();

		if constexpr ( nt.base )
		{
			using Base = typename [: base_with_static_member( ^^C, "m_pClassSendTable" ) :];
			out.push_back( SendPropDataTable( "baseclass", 0, Base::m_pClassSendTable,
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

		template for ( constexpr auto m : std::define_static_array( members_in<Table, C>() ) )
			push_member_prop<m>( out );

		template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, Table, WIRE_SEND ) ) )
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
			out.push_back( make_prop( d ) );
		}
		return out;
	}();
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
std::vector<SendProp> &table_props()
{
	static std::vector<SendProp> props = []
	{
		std::vector<SendProp> out;

		// The base is taken from bases_of rather than from the class's BaseClass typedef: that
		// typedef is private in some classes, and the legacy macro only reaches it because
		// ServerClassInit is a friend. m_pClassSendTable itself is public.
		if constexpr ( primary_net_table<C>().base )
		{
			using Base = typename [: base_with_static_member( ^^C, "m_pClassSendTable" ) :];
			out.push_back( SendPropDataTable( "baseclass", 0, Base::m_pClassSendTable,
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
		template for ( constexpr auto uv : std::define_static_array( utl_vecs( ^^C, name_t{} ) ) )
		{
			constexpr auto ua = std::define_static_array( std::meta::template_arguments_of( uv ) );
			push_utl_vec<C, ([: ua[0] :]), ([: ua[1] :]), ([: ua[2] :]), ([: ua[3] :])>( out );
		}

		// Excludes sit at the head of the table, which is where the macro lists put them.
		template for ( constexpr auto ex : std::define_static_array( all<Exclude>( ^^C ) ) )
			out.push_back( SendPropExclude( intern( ex.table ), intern( ex.prop ) ) );

		// A sub-table prop named by a string rather than derived from a member.
		template for ( constexpr auto st : std::define_static_array( send_sub_tables( ^^C, true ) ) )
		{
			constexpr auto sa = std::define_static_array( std::meta::template_arguments_of( st ) );
			// Omitting the proxy means the factory's default, SendProxy_DataTableToDataTable,
			// which also carries SPROP_PROXY_ALWAYS_YES; passing nullptr would override both.
			constexpr SendTableProxyFn fn = extract_fn<SendTableProxyFn>( sa[2] );
			if constexpr ( fn == nullptr )
				out.push_back( SendPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
				                                  std::meta::extract<SendTable *>( sa[1] ) ) );
			else
				out.push_back( SendPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
				                                  std::meta::extract<SendTable *>( sa[1] ), fn ) );
		}

		// Only the members that do not single out another of the class's tables.
		template for ( constexpr auto m : std::define_static_array( members_in<name_t{}, C>() ) )
		{
			if constexpr ( !indexed_nets<m>().empty() )
			{
				template for ( constexpr auto n : std::define_static_array( indexed_nets<m>() ) )
					out.push_back( make_prop( desc_of<m>(
					    n, offset_with_nv_flags<m>( byte_offset_of( m )
					        + n.index * (int)( std::meta::size_of(
					              unwrap( std::meta::type_of( m ) ) ) / 3 ) ),
					    proxy_of<m>() ) ) );
				continue;
			}
			push_member_prop<m>( out );
		}

		// Members declared in a base that this table sends itself. Appended rather than
		// interleaved: the compare is keyed by name, so position does not matter.
		template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, name_t{}, WIRE_SEND ) ) )
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

			if constexpr ( d.kind == PROP_TABLE )
			{
				// Either the embedded type is migrated and we generate its table, or the legacy
				// sub-table was named in the annotation. Silently generating an empty table for an
				// unmigrated type would be the worst of the three.
				constexpr SendTable *pNamed = extract_fn<SendTable *>( args[3] );
				if constexpr ( pNamed != nullptr )
					out.push_back( SendPropDataTable( d.name, d.offset & SENDPROP_OFFSET_MASK, pNamed ) );
				else
				{
					// ^^Sub would reflect the *alias*, whose own annotation list is empty, so the
					// check has to be made on the reflection of the class itself.
					constexpr std::meta::info sub =
					    embedded_class_of( std::meta::type_of( ref.member ) );
					static_assert( has<NetTable>( sub ),
					    "embedded member is neither migrated nor given its legacy sub-table" );
					out.push_back( SendPropDataTable( d.name, d.offset & SENDPROP_OFFSET_MASK,
					                                 &table<typename [: sub :]>() ) );
				}
			}
			else if constexpr ( d.kind == PROP_ARRAY )
			{
				// An inherited array cannot be annotated on the member: that would put the prop in
				// the *base* class's table. CBasePlayer re-sends CBaseCombatCharacter::m_iAmmo.
				constexpr PropDesc elem = [&args] {
					PropDesc e = desc_of<ref.member>( n, (int)ref.offset,
					                                  extract_fn<SendVarProxyFn>( args[2] ) );
					e.kind = element_kind_of( ref.member );
					e.size = e.stride;
					e.bits = n.bits == BITS_DEFAULT ? default_bits( e.kind ) : n.bits;
					e.offset &= SENDPROP_OFFSET_MASK;
					return e;
				}();
				out.push_back( SendPropArray3( d.name, d.offset & SENDPROP_OFFSET_MASK,
				                               d.stride, d.elements, make_prop( elem ) ) );
			}
			else
			{
				static_assert( d.kind != PROP_STRING,
				    "From<> does not build string props; annotate the member instead" );
				out.push_back( make_prop( d ) );
			}
		}

		// A sub-table prop named by a string rather than derived from a member.
		template for ( constexpr auto st : std::define_static_array( send_sub_tables( ^^C, false ) ) )
		{
			constexpr auto sa = std::define_static_array( std::meta::template_arguments_of( st ) );
			// Omitting the proxy means the factory's default, SendProxy_DataTableToDataTable,
			// which also carries SPROP_PROXY_ALWAYS_YES; passing nullptr would override both.
			constexpr SendTableProxyFn fn = extract_fn<SendTableProxyFn>( sa[2] );
			if constexpr ( fn == nullptr )
				out.push_back( SendPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
				                                  std::meta::extract<SendTable *>( sa[1] ) ) );
			else
				out.push_back( SendPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0,
				                                  std::meta::extract<SendTable *>( sa[1] ), fn ) );
		}
		return out;
	}();
	return props;
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
	int className::YouForgotToImplementOrDeclareServerClass() { return 0; }                        \
	static void ReflectCheckSend_##className()                                                   \
	{                                                                                            \
		const int expect = (int)ks::reflect::net::table_props<className>().size();                 \
		if ( tableName::g_SendTable.GetNumProps() != expect )                                      \
			ks::reflect::ReportDiff( #tableName, "(table)", "live",                               \
			                         "reflect send table was not constructed" );                 \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar g_ReflectCheckSend_##className( ReflectCheckSend_##className );

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
	}();                                                                                         \
	static void ReflectCheck_##className##_##tableName()                                          \
	{                                                                                            \
		const int expect = (int)ks::reflect::net::table_props_in<className, #tableName>().size();    \
		if ( tableName::g_SendTable.GetNumProps() != expect )                                      \
			ks::reflect::ReportDiff( #tableName, "(table)", "live",                               \
			                         "reflect send table was not constructed" );                 \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar                                                           \
	    g_ReflectCheck_##className##_##tableName( ReflectCheck_##className##_##tableName );

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
	}();                                                                                         \
	static void ReflectCheckSend_##className()                                                   \
	{                                                                                            \
		const int expect = (int)ks::reflect::net::table_props<className>().size();                 \
		if ( tableName::g_SendTable.GetNumProps() != expect )                                      \
			ks::reflect::ReportDiff( #tableName, "(table)", "live",                               \
			                         "reflect send table was not constructed" );                 \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar g_ReflectCheckSend_##className( ReflectCheckSend_##className );

#endif // KS_REFLECT_SENDTABLE_H
