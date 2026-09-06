// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Emits a RecvTable for an annotated client class, from the same Net annotation the send
// emitter reads. Derives the arguments and calls the existing RecvPropXXX factories, so
// byte-identity reduces to argument derivation.
//
// RecvProp carries no bit count, range or networkvar flags -- the receive side takes those
// from the matching send prop -- so the annotation is read for its name override and flags
// only, and a client member needs no bits stated.

#ifndef KS_REFLECT_RECVTABLE_H
#define KS_REFLECT_RECVTABLE_H

#include "reflect.h"
#include "dt_recv.h"
#include "dt_utlvector_recv.h"
#include "recvproxy.h"
#include "client_class.h"
#include "reflect_table_check.h"

#include <vector>

namespace ks::reflect::recv
{

// The receive half of a string-named sub-table prop. A SubTable<>'s Table argument is `auto`,
// so the send side names &DT_X::g_SendTable and this side &DT_X::g_RecvTable; each emitter takes
// only the annotations carrying its own pointer type.
consteval std::vector<std::meta::info> recv_sub_tables( std::meta::info cls, bool at_head )
{
	std::vector<std::meta::info> out;
	for ( auto t : sub_tables( cls, at_head ) )
		if ( std::meta::remove_cv( std::meta::type_of( std::meta::template_arguments_of( t )[1] ) )
		     == ^^RecvTable * )
			out.push_back( t );
	return out;
}

// One sub-table prop. Unlike SendPropDataTable, the recv factory's default proxy carries no
// extra flag, so naming DataTableRecvProxy_StaticDataTable explicitly is the same as omitting it.
template <std::meta::info St>
RecvProp sub_table_prop()
{
	constexpr auto sa = std::define_static_array( std::meta::template_arguments_of( St ) );
	constexpr DataTableRecvVarProxyFn fn = extract_fn<DataTableRecvVarProxyFn>( sa[2] );
	if constexpr ( fn == nullptr )
		return RecvPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0, 0,
		                          std::meta::extract<RecvTable *>( sa[1] ) );
	else
		return RecvPropDataTable( intern( std::meta::extract<name_t>( sa[0] ) ), 0, 0,
		                          std::meta::extract<RecvTable *>( sa[1] ), fn );
}

enum PropKind
{
	PROP_INT, PROP_FLOAT, PROP_TIME, PROP_VECTOR, PROP_VECTORXY, PROP_BOOL, PROP_EHANDLE,
	PROP_STRING, PROP_TABLE, PROP_ARRAY,
};

struct PropDesc
{
	const char *name  = nullptr;
	PropKind    kind  = PROP_INT;
	int         offset = 0;
	int         size   = 0;
	int         flags  = 0;
	int         elements = 1;
	int         stride   = 0;
	RecvVarProxyFn proxy = nullptr;   // from a Proxy<F> annotation
};

// NetworkVarEmbedded<T,...> derives from T instead of boxing it in m_Value; descend through
// any class that adds no members of its own.
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
	case FIELD_EMBEDDED:         return PROP_TABLE;
	// RecvPropQAngles is a #define for RecvPropVector, so both land here.
	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:  return PROP_VECTOR;
	case FIELD_INTEGER:
	case FIELD_SHORT:
	case FIELD_CHARACTER:
	case FIELD_INTEGER64:
	case FIELD_COLOR32:
	case FIELD_MODELINDEX:
	case FIELD_TICK:             return PROP_INT;
	default:
		throw std::meta::exception( "no recv-prop kind for this field tag", where );
	}
}

consteval PropKind element_kind_of( std::meta::info m )
{
	// The encoding overrides the element type as well: CTimeline receives its int array
	// m_nValueCounts with RecvPropFloat, one element at a time.
	if ( get<Net>( m ).enc == ENC_FLOAT ) return PROP_FLOAT;
	if ( get<Net>( m ).enc == ENC_INT ) return PROP_INT;
	if ( get<Net>( m ).enc == ENC_STRING ) return PROP_STRING;
	const std::meta::info elem = std::meta::remove_extent( unwrap( std::meta::type_of( m ) ) );
	return kind_of_tag( tag_of_type( elem ), elem, m );
}

consteval PropKind kind_of_member( std::meta::info m, const Net &n )
{
	const std::meta::info t = std::meta::type_of( m );

	if ( n.index >= 0 )
	{
		if ( n.enc == ENC_INT ) return PROP_INT;
		if ( n.enc == ENC_FLOAT ) return PROP_FLOAT;
		// A real array's element follows the element's own type -- m_uchFrontColor[i] is one byte,
		// PROP_INT, not a float lane. A Vector or QAngle component is not addressable as a real C
		// array, and the receive side has no angle kind, so that case is simply PROP_FLOAT.
		if ( is_array_member( t ) ) return element_kind_of( m );
		return PROP_FLOAT;
	}
	if ( is_string_member( t ) ) return PROP_STRING;
	// One element of an array member is a scalar prop of the element type.
	if ( n.elem >= 0 && is_array_member( t ) ) return element_kind_of( m );
	if ( is_array_member( t ) )  return PROP_ARRAY;
	// ENC_INT does matter: DT_AnimTimeMustBeFirst receives the float m_flAnimTime with RecvPropInt,
	// and a float prop would decode a different type off the wire. And the reverse, ENC_FLOAT.
	if ( n.enc == ENC_INT ) return PROP_INT;
	if ( n.enc == ENC_FLOAT ) return PROP_FLOAT;
	if ( n.enc == ENC_VECTORXY ) return PROP_VECTORXY;
	return kind_of_tag( tag_of( m ), unwrap( t ), m );
}

consteval PropKind kind_of( std::meta::info m )
{
	return kind_of_member( m, get<Net>( m ) );
}

// A shared class carries one Net annotation for both directions, and the send side fills in the
// wire encoding. Those flags describe how a value is packed, which is the sender's business: the
// legacy RecvProp for such a member passes no flags at all, so applying them here made every
// DT_EnvWindShared int come out 0x1 against a legacy 0x0. Flags about receive-side behaviour
// (SPROP_PROXY_ALWAYS_YES, SPROP_CHANGES_OFTEN, ...) are kept.
constexpr int SEND_ONLY_FLAGS =
	SPROP_UNSIGNED | SPROP_COORD | SPROP_NOSCALE | SPROP_ROUNDDOWN | SPROP_ROUNDUP |
	SPROP_NORMAL | SPROP_COORD_MP | SPROP_COORD_MP_LOWPRECISION | SPROP_COORD_MP_INTEGRAL |
	SPROP_CELL_COORD | SPROP_CELL_COORD_LOWPRECISION | SPROP_CELL_COORD_INTEGRAL | SPROP_VARINT |
	// A send-side ordering hint: 53 legacy send props set it and no recv prop does.
	SPROP_CHANGES_OFTEN;

// One entry from a member reflection plus explicit wire parameters, for a From<> entry whose
// offset is relative to the receiving class rather than the declaring one.
// One component of a Vector or QAngle, as RECVINFO( m_vecViewOffset[0] ) spells it. The recv
// compare is keyed by name, so unlike the send side the order these are emitted in does not matter.
consteval const char *indexed_name( std::meta::info m, Net n )
{
	name_t out;
	const std::string_view id = n.wire.empty() ? std::meta::identifier_of( m )
	                                            : std::string_view( n.wire.data );
	int k = 0;
	for ( ; k < (int)id.size() && k < 60; ++k ) out.data[k] = id[k];
	out.data[k++] = '[';
	out.data[k++] = (char)( '0' + n.index );
	out.data[k++] = ']';
	return intern( out );
}

consteval int component_size( std::meta::info t )
{
	// A real array's stride is its own element size; a Vector or QAngle has no C array to ask, so
	// it is always three float-sized components.
	if ( is_array_member( t ) ) return (int)array_element_size( t );
	return (int)( std::meta::size_of( unwrap( t ) ) / 3 );
}

template <std::meta::info M>
consteval std::vector<Net> indexed_nets()
{
	std::vector<Net> out;
	for ( const Net &n : all<Net>( M ) )
		if ( n.index >= 0 && on_side( n, WIRE_RECV ) )
			out.push_back( n );
	return out;
}

template <std::meta::info M>
consteval PropDesc desc_from( Net n, int offset, RecvVarProxyFn proxy )
{
	const std::meta::info t = std::meta::type_of( M );

	PropDesc d;
	d.kind   = kind_of_member( M, n );
	d.name   = n.index >= 0 ? indexed_name( M, n )
	         : n.wire.empty() ? intern( std::meta::identifier_of( M ) ) : intern( n.wire );
	d.offset = n.index >= 0 ? offset + n.index * component_size( t ) : offset;
	d.flags  = n.flags & ~SEND_ONLY_FLAGS;
	d.proxy  = proxy;
	if ( n.index >= 0 )
	{
		d.size = component_size( t );
	}
	// An inherited member can be an array or a string as easily as a scalar, and its extent is
	// read from the member either way. Leaving these unset is what forced From<> to refuse them.
	else if ( d.kind == PROP_ARRAY || d.kind == PROP_STRING )
	{
		d.elements = static_cast<int>( array_extent_of( t ) );
		d.stride   = static_cast<int>( array_element_size( t ) );
		d.size     = d.kind == PROP_STRING ? d.elements : d.stride;
	}
	else
	{
		// An element prop is sized by the element, not by the whole array.
		d.size = static_cast<int>( n.elem >= 0 ? array_element_size( t )
		                                       : std::meta::size_of( t ) );
	}
	return d;
}

template <std::meta::info M>
consteval PropDesc desc_of()
{
	const std::meta::info t = std::meta::type_of( M );

	PropDesc d;
	d.kind   = kind_of( M );
	d.name   = get<Net>( M ).index >= 0 ? indexed_name( M, get<Net>( M ) )
	                                    : external_name<Net>( M );
	d.offset = static_cast<int>( byte_offset_of( M ) );
	d.flags  = get<Net>( M ).flags & ~SEND_ONLY_FLAGS;
	if constexpr ( has_proxy( M, WIRE_RECV ) )
		d.proxy = std::meta::extract<RecvVarProxyFn>( proxy_arg_of( M, WIRE_RECV ) );

	if ( d.kind == PROP_ARRAY || d.kind == PROP_STRING )
	{
		d.elements = static_cast<int>( array_extent_of( t ) );
		d.stride   = static_cast<int>( array_element_size( t ) );
		d.size     = d.kind == PROP_STRING ? d.elements : d.stride;
	}
	else
	{
		d.size = static_cast<int>( std::meta::size_of( t ) );
		if ( get<Net>( M ).index >= 0 ) d.size = component_size( t );
	}
	return d;
}

RecvProp make_prop( const PropDesc &d );

// One CUtlVector prop. Everything consteval happens in the template arguments: an ordinary call
// to a consteval function from the table lambda escalates the whole lambda to an immediate
// function (P2564) and the table then fails to build.
// One Send/RecvPropArray2 pair. Template arguments again, for the P2564 reason above.
template <name_t ElemName, name_t ArrayName, int Count, int Stride, int Size,
          Net N, auto ElemFn, auto LenFn>
void push_bare_array( std::vector<RecvProp> &out )
{
	static constexpr const char *en = intern( ElemName );
	static constexpr const char *an = intern( ArrayName );
	PropDesc d;
	d.name  = en;
	d.kind  = N.enc == ENC_FLOAT ? PROP_FLOAT : PROP_INT;
	d.offset = 0;
	d.size  = Size;
	d.flags = N.flags & ~SEND_ONLY_FLAGS;
	d.proxy = ElemFn;
	out.push_back( make_prop( d ) );
	out.push_back( InternalRecvPropArray( Count, Stride, an, LenFn ) );
}

template <class C, name_t Member, int Max, auto Table, auto Fn>
void push_utl_vec( std::vector<RecvProp> &out )
{
	static constexpr auto ref = require_member( ^^C, Member );
	static constexpr const char *nm = intern( Member );
	static constexpr int off = (int)ref.offset;
	using Vec = typename [: std::meta::type_of( ref.member ) :];
	Vec probe;   // only to deduce the thunks, as RECVINFO_UTLVECTOR does
	// The element prop is unnamed and at offset 0 either way; a null Table means the elements are
	// scalars, whose kind comes from the vector's element type (DT_SceneEntity holds EHANDLEs).
	RecvProp elem;
	if constexpr ( Table == nullptr )
	{
		static constexpr std::meta::info et = ^^typename Vec::ElemType_t;
		static constexpr PropDesc ed = { nullptr, kind_of_tag( tag_of_type( et ), et, ^^C ), 0, 0 };
		elem = make_prop( ed );
	}
	else
		elem = RecvPropDataTable( nullptr, 0, 0, Table );
	out.push_back( RecvPropUtlVector(
	    nm, off, (int)sizeof( typename Vec::ElemType_t ),
	    Fn != nullptr ? (ResizeUtlVectorFn)Fn : GetResizeUtlVectorTemplate( probe ),
	    GetEnsureCapacityTemplate( probe ), Max, elem ) );
}

// The thunks RECVINFO_UTLVECTOR derives, without the macro's null-deref lvalue.
template <class Vec>
inline EnsureCapacityFn ensure_capacity_of()
{
	Vec v;
	return GetEnsureCapacityTemplate( v );
}

template <class Vec>
inline ResizeUtlVectorFn resize_utlvector_of()
{
	Vec v;
	return GetResizeUtlVectorTemplate( v );
}

template <class C> RecvTable &table();

template <class C>
consteval const char *table_name()
{
	if ( !primary_net_table<C>().name.empty() ) return intern( primary_net_table<C>().name );
	throw std::meta::exception( "class needs a NetTable annotation naming its table", ^^C );
}

// One member's contribution to a table. Both generators had their own copy of this and the send
// side's copies had already drifted apart, so this is shared deliberately.
template <std::meta::info M>
void push_member_prop( std::vector<RecvProp> &out )
{
	if constexpr ( !indexed_nets<M>().empty() )
	{
		template for ( constexpr auto n : std::define_static_array( indexed_nets<M>() ) )
		{
			PropDesc e = desc_of<M>();
			e.name   = indexed_name( M, n );
			e.offset = (int)byte_offset_of( M ) + n.index * component_size(
			    std::meta::type_of( M ) );
			e.size   = component_size( std::meta::type_of( M ) );
			out.push_back( make_prop( e ) );
		}
		return;
	}
	constexpr PropDesc d = desc_of<M>();
	if constexpr ( d.kind == PROP_TABLE )
	{
		using Sub = typename [: embedded_class_of( std::meta::type_of( M ) ) :];
		out.push_back( RecvPropDataTable( d.name, d.offset, d.flags, &table<Sub>() ) );
	}
	else if constexpr ( d.kind == PROP_ARRAY && get<Net>( M ).varlen )
	{
		// RecvPropArray == RecvPropVariableLengthArray: the element template is a real prop of the
		// table. Unlike the send side it is named "m_arr[0]", which is what RECVINFO( m_arr[0] )
		// stringifies to; the engine skips it anyway via IsInsideArray().
		constexpr PropDesc elem = [] {
			PropDesc e = desc_of<M>();
			e.kind = element_kind_of( M );
			e.size = e.stride;
			e.name = indexed_name( M, Net{ .index = 0 } );
			return e;
		}();
		out.push_back( make_prop( elem ) );
		out.push_back( InternalRecvPropArray( d.elements, d.stride, d.name, nullptr ) );
	}
	else if constexpr ( d.kind == PROP_ARRAY )
	{
		constexpr PropDesc elem = [] {
			PropDesc e = desc_of<M>();
			e.kind = element_kind_of( M );
			e.size = e.stride;
			return e;
		}();
		out.push_back( RecvPropArray3( d.name, d.offset, d.stride, d.elements, make_prop( elem ) ) );
	}
	else
	{
		out.push_back( make_prop( d ) );
	}
}

// One of the class's named tables. Symmetric with the send side: the member says which table it
// belongs to, so a class can receive several (C_BaseCombatCharacter has its exclusive pair) while
// each member keeps its own spec.
template <name_t Table, class C>
consteval std::vector<std::meta::info> members_in()
{
	std::vector<std::meta::info> out;
	for ( auto m : tagged_members<Net>( ^^C ) )
		if ( same_name( get<Net>( m ).table, Table ) && on_side( get<Net>( m ), WIRE_RECV ) )
			out.push_back( m );
	return out;
}

template <class C, name_t Table>
std::vector<RecvProp> &table_props_in()
{
	static std::vector<RecvProp> props = []
	{
		std::vector<RecvProp> out;
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
		template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, Table, WIRE_RECV ) ) )
		{
			constexpr auto args = std::define_static_array( std::meta::template_arguments_of( ann ) );
			// A dotted path is the prop's wire name, as SENDINFO/RECVINFO stringify it.
			constexpr auto ref  = require_member( ^^C, std::meta::extract<name_t>( args[0] ) );
			// A bracketed path resolved to one element; the Net has to say so, because the kind
			// and the size are the element's, not the array's.
			constexpr Net  n    = with_elem( wire_from_path( std::meta::extract<Net>( args[1] ),
			                                                std::meta::extract<name_t>( args[0] ) ),
			                                 ref.elem );
			constexpr PropDesc d = desc_from<ref.member>(
			    n, static_cast<int>( ref.offset ), extract_fn<RecvVarProxyFn>( args[2] ) );
			out.push_back( make_prop( d ) );
		}
		return out;
	}();
	return props;
}

template <class C, name_t Table>
RecvTable &table_in()
{
	static RecvTable t = [] {
		auto &p = table_props_in<C, Table>();
		return RecvTable( p.data(), static_cast<int>( p.size() ), intern( Table ) );
	}();
	return t;
}

template <class C>
std::vector<RecvProp> &table_props()
{
	static std::vector<RecvProp> props = []
	{
		std::vector<RecvProp> out;

		// The base is taken from bases_of, and its table pointer through a reflection obtained
		// with unchecked access: DECLARE_CLIENTCLASS leaves m_pClassRecvTable private in some
		// classes, and the legacy macro only reaches it because ClientClassInit is a friend.
		if constexpr ( primary_net_table<C>().base )
		{
			constexpr auto base = base_with_static_member( ^^C, "m_pClassRecvTable" );
			out.push_back( RecvPropDataTable( "baseclass", 0, 0,
			                                  [: static_member_of( base, "m_pClassRecvTable" ) :],
			                                  DataTableRecvProxy_StaticDataTable ) );
		}

		// Sub-table props named by a wire string rather than by a member, at the head of the table.
		template for ( constexpr auto st : std::define_static_array( recv_sub_tables( ^^C, true ) ) )
			out.push_back( sub_table_prop<st>() );

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

		// Props named by a bare wire string with no member behind them.
		template for ( constexpr auto bp : std::define_static_array( bare_props( ^^C ) ) )
		{
			constexpr auto ba = std::define_static_array( std::meta::template_arguments_of( bp ) );
			constexpr Net  bn = std::meta::extract<Net>( ba[1] );
			PropDesc d;
			d.name   = intern( std::meta::extract<name_t>( ba[0] ) );
			d.kind   = bn.enc == ENC_INT ? PROP_INT : PROP_FLOAT;
			d.offset = 0;
			d.size   = SIZEOF_IGNORE;
			d.flags  = bn.flags & ~SEND_ONLY_FLAGS;
			d.proxy  = extract_fn<RecvVarProxyFn>( ba[2] );
			out.push_back( make_prop( d ) );
		}

		// Only the members that do not single out another of the class's tables.
		template for ( constexpr auto m : std::define_static_array( members_in<name_t{}, C>() ) )
			push_member_prop<m>( out );

		template for ( constexpr auto st : std::define_static_array( recv_sub_tables( ^^C, false ) ) )
			out.push_back( sub_table_prop<st>() );

		// Members declared in a base that this table receives itself. Order is not observable
		// here -- the decoder resolves by name -- so these are simply appended.
		template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, name_t{}, WIRE_RECV ) ) )
		{
			constexpr auto args = std::define_static_array( std::meta::template_arguments_of( ann ) );
			// A dotted path is the prop's wire name, as SENDINFO/RECVINFO stringify it.
			constexpr auto ref  = require_member( ^^C, std::meta::extract<name_t>( args[0] ) );
			// A bracketed path resolved to one element; the Net has to say so, because the kind
			// and the size are the element's, not the array's.
			constexpr Net  n    = with_elem( wire_from_path( std::meta::extract<Net>( args[1] ),
			                                                std::meta::extract<name_t>( args[0] ) ),
			                                 ref.elem );
			constexpr PropDesc d = desc_from<ref.member>( n, static_cast<int>( ref.offset ),
			                                              extract_fn<RecvVarProxyFn>( args[2] ) );
			if constexpr ( d.kind == PROP_TABLE )
			{
				// As on the send side: either the embedded type is migrated, or the legacy
				// sub-table was named. Generating an empty table for an unmigrated type is the
				// one outcome worth refusing outright.
				constexpr RecvTable *pNamed = extract_fn<RecvTable *>( args[3] );
				if constexpr ( pNamed != nullptr )
					out.push_back( RecvPropDataTable( d.name, d.offset, d.flags, pNamed ) );
				else
				{
					// ^^ on an alias reflects the alias, whose annotations are empty; check the class.
					constexpr std::meta::info sub =
					    embedded_class_of( std::meta::type_of( ref.member ) );
					static_assert( has<NetTable>( sub ),
					    "embedded member is neither migrated nor given its legacy sub-table" );
					out.push_back( RecvPropDataTable( d.name, d.offset, d.flags,
					                                  &table<typename [: sub :]>() ) );
				}
			}
			else if constexpr ( d.kind == PROP_ARRAY )
			{
				constexpr PropDesc elem = [&args] {
					PropDesc e = desc_from<ref.member>( n, static_cast<int>( ref.offset ),
					                                    extract_fn<RecvVarProxyFn>( args[2] ) );
					e.kind = element_kind_of( ref.member );
					e.size = e.stride;
					return e;
				}();
				out.push_back( RecvPropArray3( d.name, d.offset, d.stride, d.elements,
				                               make_prop( elem ) ) );
			}
			else
			{
				out.push_back( make_prop( d ) );
			}
		}
		return out;
	}();
	return props;
}

template <class C>
RecvTable &table()
{
	static RecvTable t = [] {
		auto &p = table_props<C>();
		return RecvTable( p.data(), static_cast<int>( p.size() ), table_name<C>() );
	}();
	return t;
}

} // namespace ks::reflect::recv


// Replaces IMPLEMENT_CLIENTCLASS_DT plus its prop list once the gate has proven the generated table
// identical. Mirrors the legacy macro exactly: the dataTable::g_RecvTable global stays (the
// ClientClass registration points at it, and the engine resolves props through it), the entity
// factory is the same new-and-Init, and Construct runs in a static initializer as ClientClassInit
// did. Defining m_pClassRecvTable out of line is allowed even where it is private -- only reading
// it needs the unchecked reflection the gate uses.
#define IMPLEMENT_REFLECT_CLIENTCLASS( clientClassName, dataTable, serverClassName )             \
	namespace dataTable { RecvTable g_RecvTable; }                                               \
	static const int g_##clientClassName##_ReflectRecvInit = []                                  \
	{                                                                                            \
		auto &p = ks::reflect::recv::table_props<clientClassName>();                             \
		dataTable::g_RecvTable.Construct( p.data(), (int)p.size(), #dataTable );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static IClientNetworkable *_##clientClassName##_CreateObject( int entnum, int serialNum )     \
	{                                                                                            \
		clientClassName *pRet = new clientClassName;                                             \
		if ( !pRet ) return 0;                                                                    \
		pRet->Init( entnum, serialNum );                                                          \
		return pRet;                                                                             \
	}                                                                                            \
	ClientClass __g_##clientClassName##ClientClass( #serverClassName,                             \
	                                                _##clientClassName##_CreateObject,            \
	                                                NULL, &dataTable::g_RecvTable );               \
	RecvTable *clientClassName::m_pClassRecvTable = &dataTable::g_RecvTable;                       \
	int clientClassName::YouForgotToImplementOrDeclareClientClass() { return 0; }                  \
	ClientClass *clientClassName::GetClientClass() { return &__g_##clientClassName##ClientClass; } \
	static void ReflectCheckRecv_##clientClassName()                                              \
	{                                                                                            \
		const int expect = (int)ks::reflect::recv::table_props<clientClassName>().size();           \
		if ( dataTable::g_RecvTable.GetNumProps() != expect )                                       \
			ks::reflect::ReportDiff( #dataTable, "(table)", "live",                                \
			                         "reflect recv table was not constructed" );                   \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar                                                           \
	    g_ReflectCheckRecv_##clientClassName( ReflectCheckRecv_##clientClassName );

// A client event class: one static singleton rather than a per-entity allocation, and the factory
// goes in the ClientClass's third slot, not its second. Temp entities use this for all 32 of their
// tables, which is why it needs a macro of its own rather than a flag on the one above.
#define IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( clientClassName, dataTable, serverClassName )        \
	namespace dataTable { RecvTable g_RecvTable; }                                               \
	static const int g_##clientClassName##_ReflectRecvInit = []                                  \
	{                                                                                            \
		auto &p = ks::reflect::recv::table_props<clientClassName>();                             \
		dataTable::g_RecvTable.Construct( p.data(), (int)p.size(), #dataTable );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static clientClassName __g_##clientClassName;                                                \
	static IClientNetworkable *_##clientClassName##_CreateObject()                                \
	{                                                                                            \
		return &__g_##clientClassName;                                                           \
	}                                                                                            \
	ClientClass __g_##clientClassName##ClientClass( #serverClassName, NULL,                       \
	                                                _##clientClassName##_CreateObject,            \
	                                                &dataTable::g_RecvTable );                     \
	RecvTable *clientClassName::m_pClassRecvTable = &dataTable::g_RecvTable;                       \
	int clientClassName::YouForgotToImplementOrDeclareClientClass() { return 0; }                  \
	ClientClass *clientClassName::GetClientClass() { return &__g_##clientClassName##ClientClass; } \
	static void ReflectCheckRecv_##clientClassName()                                              \
	{                                                                                            \
		const int expect = (int)ks::reflect::recv::table_props<clientClassName>().size();           \
		if ( dataTable::g_RecvTable.GetNumProps() != expect )                                       \
			ks::reflect::ReportDiff( #dataTable, "(table)", "live",                                \
			                         "reflect recv table was not constructed" );                   \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar                                                           \
	    g_ReflectCheckRecv_##clientClassName( ReflectCheckRecv_##clientClassName );

// One of a class's *secondary* tables: the members that name it, rather than the class's own set.
// CBaseCombatCharacter sends DT_BCCLocalPlayerExclusive and DT_BCCNonLocalPlayerExclusive besides
// its own table, and each is a separate namespace global with no class registration behind it.
#define IMPLEMENT_REFLECT_TABLE_IN( className, tableName )                                        \
	namespace tableName { RecvTable g_RecvTable; }                                                       \
	static const int g_##className##_##tableName##_ReflectInit = []                               \
	{                                                                                            \
		auto &p = ks::reflect::recv::table_props_in<className, #tableName>();                        \
		tableName::g_RecvTable.Construct( p.data(), (int)p.size(), #tableName );                    \
		return 1;                                                                                \
	}();                                                                                         \
	static void ReflectCheck_##className##_##tableName()                                          \
	{                                                                                            \
		const int expect = (int)ks::reflect::recv::table_props_in<className, #tableName>().size();    \
		if ( tableName::g_RecvTable.GetNumProps() != expect )                                      \
			ks::reflect::ReportDiff( #tableName, "(table)", "live",                               \
			                         "reflect recv table was not constructed" );                 \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar                                                           \
	    g_ReflectCheck_##className##_##tableName( ReflectCheck_##className##_##tableName );

// The client half of IMPLEMENT_REFLECT_TABLE; see the send emitter for why one name is
// defined in both. INTERNAL_IMPLEMENT_CLIENTCLASS_PROLOGUE supplies everything but the table.
#define IMPLEMENT_REFLECT_TABLE( className, tableName )                                   \
	namespace tableName { RecvTable g_RecvTable; }                                               \
	static const int g_##className##_ReflectRecvInit = []                                        \
	{                                                                                            \
		auto &p = ks::reflect::recv::table_props<className>();                                    \
		tableName::g_RecvTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static void ReflectCheckRecv_##className()                                                   \
	{                                                                                            \
		const int expect = (int)ks::reflect::recv::table_props<className>().size();                 \
		if ( tableName::g_RecvTable.GetNumProps() != expect )                                      \
			ks::reflect::ReportDiff( #tableName, "(table)", "live",                               \
			                         "reflect recv table was not constructed" );                 \
	}                                                                                            \
	static ks::reflect::VerifyRegistrar g_ReflectCheckRecv_##className( ReflectCheckRecv_##className );

#endif // KS_REFLECT_RECVTABLE_H
