// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include "entity_kinds_dump.h"
#include "isaverestore.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "dt_send.h"
#include "dt_utlvector_common.h"
#include "reflect_sendtable.h"
module ks.server:entity_2;

import std;
import :baseentity;
import ks.core.types;
import ks.core.vprof;
import ks.tier1.reflect_wirevar;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.ssequaternion;
import ks.math.vector;
import ks.platform;
import ks.simd;
import :player;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.registry;

class CBaseEntity;

// Generation of entity_kind_list.h (src/tools/entity_kinds/README.md): with KS_ENTITY_KINDS_DUMP defined,
// every registered entity class fails a static_assert whose message is the reflected class chain of the
// class (its bases and, per class, the virtual members and the members that share a name with a CBaseEntity
// virtual). Otherwise this header is empty.
#ifdef KS_ENTITY_KINDS_DUMP
namespace ks_dump
{
consteval bool derives( std::meta::info t, std::meta::info base )
{
	if ( t == base ) return true;
	if ( !std::meta::is_class_type( t ) || !std::meta::is_complete_type( t ) ) return false;
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
		if ( derives( std::meta::remove_cv( std::meta::type_of( b ) ), base ) ) return true;
	return false;
}
consteval std::string num( unsigned n ) { std::string r; do { r.insert( r.begin(), char( 48 + n % 10 ) ); n /= 10; } while ( n ); return r; }

consteval std::string cbe_names()
{
	std::string v = "|";
	for ( auto m : std::meta::members_of( ^^CBaseEntity, std::meta::access_context::unchecked() ) )
		if ( std::meta::is_function( m ) && std::meta::has_identifier( m ) && std::meta::is_virtual( m ) )
			{ v += std::meta::identifier_of( m ); v += "|"; }
	return v;
}
template <int = 0> inline constexpr const char *kCbeNames = std::define_static_string( cbe_names() );
template <int = 0> consteval bool is_cbe_virtual_name( std::string_view id )
{
	std::string key = "|"; key += id; key += "|";
	return std::string_view( kCbeNames<> ).find( key ) != std::string_view::npos;
}

template <class C>
consteval std::string describe()
{
	std::string s = "\nKS|REG|" + std::string( std::meta::display_string_of( ^^C ) ) + "\n";
	std::meta::info x = ^^C;
	for ( int depth = 0; depth < 40; ++depth )
	{
		auto loc = std::meta::source_location_of( x );
		s += "KS|CLASS|" + std::string( std::meta::display_string_of( x ) ) + "|" + ( std::meta::is_union_type( x ) ? "union" : "cls" ) + "|"
		   + std::string( loc.file_name() ) + "|" + num( loc.line() ) + "|" + ( std::meta::has_template_arguments( x ) ? "tmpl" : "plain" ) + "\n";
		int nbases = 0; std::meta::info next{};
		for ( auto b : std::meta::bases_of( x, std::meta::access_context::unchecked() ) )
		{
			s += "KS|BASE|" + std::string( std::meta::display_string_of( std::meta::type_of( b ) ) ) + "\n";
			if ( derives( std::meta::remove_cv( std::meta::type_of( b ) ), ^^CBaseEntity ) ) { ++nbases; next = std::meta::remove_cv( std::meta::type_of( b ) ); }
		}
		if ( x != ^^CBaseEntity )
		{
			for ( auto m : std::meta::members_of( x, std::meta::access_context::unchecked() ) )
			{
				if ( !std::meta::is_function( m ) || !std::meta::has_identifier( m ) ) continue;
				if ( std::meta::is_constructor( m ) || std::meta::is_destructor( m ) ) continue;
				std::string_view id = std::meta::identifier_of( m );
				bool v = std::meta::is_virtual( m );
				if ( !v && !is_cbe_virtual_name( id ) ) continue;
				s += std::string( "KS|FN|" ) + ( v ? "V" : "H" ) + "|" + std::string( id ) + "|" + std::string( std::meta::display_string_of( std::meta::type_of( m ) ) )
				   + "|" + ( std::meta::is_public( m ) ? "pub" : std::meta::is_protected( m ) ? "prot" : "priv" ) + "\n";
			}
		}
		if ( x == ^^CBaseEntity ) break;
		if ( nbases != 1 ) { s += "KS|BADCHAIN|" + num( nbases ) + "\n"; break; }
		x = next;
	}
	return s;
}
}
#else
#endif

// One typedescription_t builder, shared by the datamap emitter (server) and the prediction
// emitter (client). Nothing here reaches for entityoutput.h or predictable_entity.h, so both
// sides can include it.

namespace ksr
{
using namespace ::ks::reflect;

struct FieldDesc
{
	fieldtype_t type = FIELD_VOID;
	const char *name = nullptr;
	const char *external = nullptr;
	int         offset = 0;
	int         bytes  = 0;
	int         count  = 1;
	short       flags  = 0;
	float       tolerance = 0.f;
	ISaveRestoreOps *ops = nullptr;
	inputfunc_t input = nullptr;
	// The nested map an embedded entry points at: DEFINE_EMBEDDED, DEFINE_PRED_TYPEDESCRIPTION.
	datamap_t  *td = nullptr;
};

// The macros are aggregate initialisers that stop early and leave the trailing members to
// static zero-initialisation. These land in a heap vector, so every one has to be set.
inline typedescription_t MakeField( const FieldDesc &d )
{
	typedescription_t t = {};
	t.fieldType        = d.type;
	t.fieldName        = d.name;
	t.fieldOffset      = d.offset;
	t.fieldSize        = static_cast<unsigned short>( d.count );
	t.flags            = d.flags;
	t.externalName     = d.external;
	t.pSaveRestoreOps  = d.ops;
	t.inputFunc        = ToInputStorage( d.input );
	t.td               = d.td;
	t.fieldSizeInBytes = d.bytes;
	t.override_field   = nullptr;
	t.override_count   = 0;
	t.fieldTolerance   = d.tolerance;
	return t;
}

} // namespace ksr

// Emits datamap entries for annotated members. Unlike SendProp there is no factory to call --
// the DEFINE_* macros are aggregate initialisers -- so the emitter builds typedescription_t
// directly and byte-identity is a straight field comparison.
//
// One Key annotation covers both kinds, because which kind it is *is* derivable: a member
// whose type derives from CBaseEntityOutput is an output (DEFINE_OUTPUT), anything else is a
// keyfield (DEFINE_KEYFIELD). The map name is the only thing that cannot be derived.

extern ISaveRestoreOps *eventFuncs;	// the save/restore ops of entity outputs (server)

namespace ksr::dmap
{
using namespace ::ks::reflect;

consteval bool is_output_type( std::meta::info t )
{
	// An array of outputs is still outputs: DEFINE_OUTPUT( m_Output[0], ... ) names one element of
	// a COutputEvent[8], and the array type itself derives from nothing.
	return derives_from( std::meta::remove_extent( std::meta::remove_cv( t ) ), ^^CBaseEntityOutput );
}

consteval bool is_output( std::meta::info m )
{
	return is_output_type( std::meta::type_of( m ) );
}

template <std::meta::info M, std::size_t Base = 0>
consteval FieldDesc desc_of( const Key &k )
{
	const std::meta::info t = std::meta::type_of( M );

	FieldDesc d;
	d.name     = intern( std::meta::identifier_of( M ) );
	d.external = k.name.empty() ? intern( std::meta::identifier_of( M ) ) : intern( k.name );
	// Keyfields index the member itself, so the offset carries no networkvar flags.
	d.offset   = static_cast<int>( Base + byte_offset_of( M ) );
	d.bytes    = static_cast<int>( std::meta::size_of( t ) );

	if ( is_output( M ) )
	{
		d.type  = FIELD_CUSTOM;
		d.flags = FTYPEDESC_OUTPUT | FTYPEDESC_SAVE | FTYPEDESC_KEY;
		// DEFINE_OUTPUT stops at pSaveRestoreOps, so fieldSizeInBytes stays 0.
		d.bytes = 0;
		// One element of an output array, each with its own map name.
		if ( k.index >= 0 )
			d.offset += k.index * static_cast<int>( array_element_size( t ) );
	}
	else
	{
		d.type  = tag_of( M );
		d.flags = FTYPEDESC_KEY | FTYPEDESC_SAVE;
		if ( k.input )  d.flags |= FTYPEDESC_INPUT;
		// DEFINE_GLOBAL_KEYFIELD is DEFINE_KEYFIELD plus this one flag.
		if ( k.global ) d.flags |= FTYPEDESC_GLOBAL;
		if ( k.index >= 0 )
		{
			// One element of an array: the entry describes the element, at its own offset. The
			// array has to be reached through unwrap -- CNetworkArray holds it as m_Value[count],
			// and remove_extent on the wrapper yields the wrapper, whose size is the whole array.
			const std::meta::info e = std::meta::remove_extent( unwrap( t ) );
			d.type   = k.as != FIELD_VOID ? k.as : tag_of_type( std::meta::remove_cv( e ) );
			d.bytes  = static_cast<int>( std::meta::size_of( e ) );
			d.offset += k.index * static_cast<int>( std::meta::size_of( e ) );
			d.count  = 1;
		}
		else if ( is_array_member( t ) )
		{
			d.count = static_cast<int>( array_extent_of( t ) );
		}
	}
	return d;
}

consteval FieldDesc desc_from( std::meta::info m, const Key &k, int offset, const name_t &path )
{
	const std::meta::info t = std::meta::type_of( m );

	FieldDesc d;
	// The macro stringifies the whole member expression, so the path *is* the fieldName.
	d.name     = intern( path );
	d.external = k.name.empty() ? intern( std::meta::identifier_of( m ) ) : intern( k.name );
	d.offset   = offset;
	d.bytes    = static_cast<int>( std::meta::size_of( t ) );

	if ( is_output( m ) )
	{
		d.type  = FIELD_CUSTOM;
		d.flags = FTYPEDESC_OUTPUT | FTYPEDESC_SAVE | FTYPEDESC_KEY;
		d.bytes = 0;
	}
	else
	{
		// A path leaf lives in a shared struct, so it cannot carry a member-level As; the
		// refinement is stated on the annotation instead.
		d.type  = k.as != FIELD_VOID ? k.as : tag_of( m );
		d.flags = FTYPEDESC_KEY | FTYPEDESC_SAVE;
		if ( k.input )  d.flags |= FTYPEDESC_INPUT;
		if ( k.global ) d.flags |= FTYPEDESC_GLOBAL;
	}
	return d;
}

// eventFuncs is not a constant expression, so the output ops pointer is attached at runtime.
typedescription_t make_field( const FieldDesc &d, bool output );

// DEFINE_INPUTFUNC stops after inputFunc, so fieldSizeInBytes and the offset stay 0. The
// function pointer itself is attached at the call site, where the reflection can be spliced.
// Input is repeatable, so the annotation is passed in rather than looked up.
template <std::meta::info F>
consteval FieldDesc input_desc_of( const Input &in )
{

	FieldDesc d;
	d.type     = in.type;
	d.name     = intern( std::meta::identifier_of( F ) );
	d.external = in.name.empty() ? intern( std::meta::identifier_of( F ) ) : intern( in.name );
	d.flags    = FTYPEDESC_INPUT;
	return d;
}

template <class C>
std::vector<typedescription_t> &fields()
{
	static std::vector<typedescription_t> out = []
	{
		std::vector<typedescription_t> v;
		// Key is repeatable, so a member contributes one entry per annotation.
		template for ( constexpr auto m : members<Key, C> )
			template for ( constexpr auto k : std::define_static_array( all<Key>( m ) ) )
				v.push_back( make_field( desc_of<m, flattened_offset( ^^C, m )>( k ), is_output( m ) ) );

		// Entries reached through a dotted path, named at class level.
		template for ( constexpr auto ann : std::define_static_array( key_from_annotations( ^^C ) ) )
		{
			constexpr auto args = std::define_static_array( std::meta::template_arguments_of( ann ) );
			constexpr auto path = std::meta::extract<name_t>( args[0] );
			constexpr auto ref  = require_member( ^^C, path );
			constexpr Key  k    = std::meta::extract<Key>( args[1] );
			v.push_back( make_field( desc_from( ref.member, k, static_cast<int>( ref.offset ), path ),
			                         is_output( ref.member ) ) );
		}

		// Entity I/O is server-side: inputfunc_t is `void (CBaseEntity::*)(inputdata_t &)`, and on
		// the client nothing derives from that CBaseEntity, so the static_cast is ill-formed. Every
		// DEFINE_INPUTFUNC in a shared datadesc is guarded the same way.
		// Input is repeatable too: one handler can answer to several map names.
		template for ( constexpr auto f : std::define_static_array( tagged_functions<Input>( ^^C ) ) )
			template for ( constexpr auto in : std::define_static_array( all<Input>( f ) ) )
			{
				FieldDesc d = input_desc_of<f>( in );
				d.input = static_cast<inputfunc_t>( &[: f :] );
				v.push_back( MakeField( d ) );
			}
		// An empty datamap is not an empty array, exactly as for prediction. BEGIN_DATADESC_GUTS
		// opens the array with a dummy { FIELD_VOID, 0, ... } "so you can define empty tables" and
		// END_DATADESC reports dataNumFields = 1 for it; only when there *are* entries does it skip
		// past the dummy. 18 classes declare a datadesc purely to take their place in the baseMap
		// chain, and this is the faithful reproduction of one.
		if ( v.empty() )
			v.push_back( typedescription_t{} );
		return v;
	}();
	return out;
}

} // namespace ksr::dmap

// Replaces the legacy BEGIN_DATADESC/END_DATADESC emitters, which are gone. The two definitions
// are what BEGIN_DATADESC provided; the map is filled in a static initializer, which is when the
// legacy DataMapInit ran, so the timing is unchanged. m_DataMap is reached through unchecked
// reflection because DECLARE_DATADESC leaves it in whatever access section the class happens to be
// in -- usually private. The base map is taken from bases_of for the same reason, rather than from
// the class's BaseClass typedef, which is private in some classes.

// For a class of a shared library: the library defines the (empty) m_DataMap, the game that
// uses the class fills it in.

// The base-less mirror: BEGIN_SIMPLE_DATADESC / BEGIN_DATADESC_NO_BASE both have no base to walk.

// BEGIN_SIMPLE_DATADESC_ is the one form that walks a named base *without* defining the virtual
// GetDataDescMap() -- so it is neither _SIMPLE (no base) nor _NO_BASE (which does define the
// virtual). baseMap is assigned here because the legacy BEGIN_DATADESC_GUTS assigned it too.

// BEGIN_DATADESC_NO_BASE differs from BEGIN_SIMPLE_DATADESC by one thing: it still defines the
// virtual GetDataDescMap() override, because a class reached with DECLARE_DATADESC() (rather than
// DECLARE_SIMPLE_DATADESC(), which does not declare it at all) needs one. Omitting it left that
// virtual's key-function homing without a definition, and the linker reported the whole vtable
// missing rather than one function: undefined symbol: _ZTV24CFourWheelVehiclePhysics.

// Emits prediction descriptor entries for members carrying Pred. Client-only, like the
// prediction system itself. DEFINE_PRED_FIELD names no external string, so unlike the datamap
// nothing here needs a name -- only the flags and tolerance, which are the two things that
// genuinely cannot be derived.

namespace ksr::pred
{
using namespace ::ks::reflect;

// An embedded entry: the member's type has a prediction map of its own, which is what
// DEFINE_PRED_TYPEDESCRIPTION names. The flags are the macro's own, and it sets no size.
consteval bool is_embedded_map( std::meta::info m )
{
	return tag_of( m ) == FIELD_EMBEDDED
	       && has_static_member( unwrap( std::meta::type_of( m ) ), "m_PredMap" );
}

template <std::meta::info M, std::size_t Base = 0>
consteval FieldDesc desc_of()
{
	const std::meta::info t = std::meta::type_of( M );
	const Pred p = get<Pred>( M );

	FieldDesc d;
	d.type      = tag_of( M );
	d.name      = intern( std::meta::identifier_of( M ) );
	d.external  = nullptr;
	d.offset    = static_cast<int>( Base + byte_offset_of( M ) );
	d.bytes     = static_cast<int>( std::meta::size_of( t ) );
	d.flags     = static_cast<short>( p.flags );
	d.tolerance = p.tolerance;
	if ( is_array_member( t ) ) d.count = static_cast<int>( array_extent_of( t ) );
	if ( is_embedded_map( M ) )
	{
		d.flags = FTYPEDESC_SAVE | FTYPEDESC_KEY;
		d.bytes = 0;
	}
	return d;
}

// One entry for a member named by a class-level PredFrom<>, whose offset is relative to the
// predicting class rather than the declaring one.
consteval FieldDesc desc_from( std::meta::info m, Pred p, int offset )
{
	const std::meta::info t = std::meta::type_of( m );

	FieldDesc d;
	d.type      = tag_of( m );
	d.name      = intern( std::meta::identifier_of( m ) );
	d.offset    = offset;
	d.bytes     = static_cast<int>( std::meta::size_of( t ) );
	d.flags     = static_cast<short>( p.flags );
	d.tolerance = p.tolerance;
	if ( is_array_member( t ) ) d.count = static_cast<int>( array_extent_of( t ) );
	return d;
}

template <class C>
std::vector<typedescription_t> &fields()
{
	static std::vector<typedescription_t> out = []
	{
		std::vector<typedescription_t> v;
		template for ( constexpr auto m : members<Pred, C> )
		{
			if constexpr ( is_embedded_map( m ) )
			{
				FieldDesc d = desc_of<m, flattened_offset( ^^C, m )>();
				d.td = &[: static_member_of( unwrap( std::meta::type_of( m ) ), "m_PredMap" ) :];
				v.push_back( MakeField( d ) );
			}
			else
			{
				v.push_back( MakeField( desc_of<m, flattened_offset( ^^C, m )>() ) );
			}
		}
		// Members declared in a base that this class predicts itself. Order is not observable, so
		// these are simply appended.
		template for ( constexpr auto ann : std::define_static_array( pred_from_annotations( ^^C ) ) )
		{
			constexpr auto args = std::define_static_array( std::meta::template_arguments_of( ann ) );
			constexpr auto ref  = require_member( ^^C, std::meta::extract<name_t>( args[0] ) );
			v.push_back( MakeField( desc_from( ref.member, std::meta::extract<Pred>( args[1] ),
			                                   static_cast<int>( ref.offset ) ) ) );
		}
		// An empty prediction map is not an empty array. END_PREDICTION_DATA keeps the dummy
		// entry the macro opens with -- "so you can define empty tables" -- and reports
		// dataNumFields = 1 rather than 0, so the faithful reproduction is that one field.
		if ( v.empty() )
			v.push_back( typedescription_t{} );
		return v;
	}();
	return out;
}

} // namespace ksr::pred

// Replaces BEGIN_PREDICTION_DATA / END_PREDICTION_DATA. m_PredMap is public -- DECLARE_PREDICTABLE
// opens with `public:` -- so unlike the datamap it can be named directly. BaseClass resolves here
// because the initialiser of a static data member is in its class's scope, which is how the macro
// this replaces spelled it too.
#define IMPLEMENT_REFLECT_PREDMAP_BASE( className, base )                                         \
	datamap_t className::m_PredMap = { .dataClassName = #className, .baseMap = base };                                   \
	datamap_t *className::GetPredDescMap( void ) { return &m_PredMap; }                            \
	namespace className##_ReflectPredInit                                                          \
	{                                                                                              \
		static const bool g_filled = []                                                             \
		{                                                                                            \
			auto &f = ksr::pred::fields<className>();                                          \
			className::m_PredMap.dataDesc      = f.data();                                             \
			className::m_PredMap.dataNumFields = (int)f.size();                                        \
			return true;                                                                               \
		}();                                                                                        \
	}

#define IMPLEMENT_REFLECT_PREDMAP( className )                                                     \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, &BaseClass::m_PredMap )

#define IMPLEMENT_REFLECT_PREDMAP_NO_BASE( className )                                            \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, nullptr )

#undef IMPLEMENT_REFLECT_PREDMAP_BASE
#undef IMPLEMENT_REFLECT_PREDMAP
#undef IMPLEMENT_REFLECT_PREDMAP_NO_BASE

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

namespace ksr::net
{
using namespace ::ks::reflect;

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
	WireVecOps              vec;                 // UtlVector
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
	d.kind     = PropKind::UtlVector;
	d.name     = nm;
	d.offset   = off;
	d.size     = (int)sizeof( typename WV::elem_type );
	d.vec      = WV::ops();
	d.max      = Max;
	d.elem     = (int)out.size() - 1;
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
template <std::meta::info M, std::size_t Base = 0>
constexpr void push_member_prop( std::vector<PropDesc> &out )
{
	// Indexed props are handled here, as on the recv side, not by build_desc's loop.
	if constexpr ( !indexed_nets<M>( WireSide::Send ).empty() )
	{
		template for ( constexpr auto n : std::define_static_array( indexed_nets<M>( WireSide::Send ) ) )
			out.push_back( desc_of<M>(
			    n, offset_with_nv_flags<M>( Base + byte_offset_of( M )
			        + n.index * (int)( std::meta::size_of( unwrap( std::meta::type_of( M ) ) ) / 3 ) ),
			    proxy_of<M>() ) );
		return;
	}

	constexpr Net n = get<Net>( M );
	constexpr PropDesc d = desc_of<M>( n, offset_with_nv_flags<M>( Base + byte_offset_of( M ) ),
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
			PropDesc e = desc_of<M>( mn, Base + byte_offset_of( M ), proxy_of<M>() );
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
		push_member_prop<m, flattened_offset( ^^C, m )>( out );

	// Members declared in a base that this table sends itself. Appended rather than
	// interleaved: the compare is keyed by name, so position does not matter.
	template for ( constexpr auto ann : std::define_static_array( from_annotations_in( ^^C, Table, WireSide::Send ) ) )
	{
		constexpr FromEntry fe = from_entry( ann );
		// A dotted path is the prop's wire name, as SENDINFO/RECVINFO stringify it.
		constexpr auto ref  = require_member( ^^C, fe.name );
		// A bracketed path resolved to one element; the Net has to say so, because the kind
		// and the size are the element's, not the array's.
		constexpr Net  n    = with_elem( wire_from_path( fe.net, fe.name ), ref.elem );
		// An inherited member can be indexed too: CFuncRotating re-sends the three components
		// of CBaseEntity's m_angRotation. desc_of already names and sizes a component; the
		// offset is the one thing it cannot know, because it is relative to the sending class.
		constexpr int comp = n.index < 0 ? 0
		    : n.index * (int)( std::meta::size_of( unwrap( std::meta::type_of( ref.member ) ) ) / 3 );
		constexpr int off = n.raw ? (int)ref.offset + comp
		                          : offset_with_nv_flags<ref.member>( (int)ref.offset + comp );
		constexpr PropDesc d = desc_of<ref.member>(
		    n, off, extract_fn<SendVarProxyFn>( fe.proxy ) );

		if constexpr ( fe.table != std::meta::info{} )
		{
			// FromTable<>: use the named table rather than regenerating one.
			static_assert( d.kind == PropKind::Table, "FromTable<> on a non-embedded member" );
			constexpr SendTable *pTable = std::meta::extract<SendTable *>( fe.table );
			out.push_back( table_prop( d.name, d.offset & SENDPROP_OFFSET_MASK,
			                           +[]() -> SendTable * { return pTable; }, nullptr ) );
		}
		else if constexpr ( d.kind == PropKind::Table )
		{
			// The embedded type must be migrated. ^^Sub would reflect the *alias*, whose own
			// annotation list is empty, so check the class's reflection.
			constexpr std::meta::info sub =
			    embedded_class_of( std::meta::type_of( ref.member ) );
			static_assert( has<NetTable>( sub ),
			    "embedded member is neither migrated nor given its legacy sub-table" );
			out.push_back( table_prop( d.name, d.offset & SENDPROP_OFFSET_MASK,
			                           +[]() -> SendTable * { return &table<typename [: sub :]>(); },
			                           nullptr ) );
		}
		else if constexpr ( d.kind == PropKind::Array )
		{
			// An inherited array cannot be annotated on the member: that would put the prop in
			// the *base* class's table. CBasePlayer re-sends CBaseCombatCharacter::m_iAmmo.
			constexpr PropDesc elem = [&fe] {
				PropDesc e = desc_of<ref.member>( n, (int)ref.offset,
				                                  extract_fn<SendVarProxyFn>( fe.proxy ) );
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

} // namespace ksr::net

// Replaces IMPLEMENT_SERVERCLASS_ST plus its prop list once the gate has proven the generated
// table identical. The DT_x::g_SendTable global stays and is filled in a static initializer, which
// is what BEGIN_SEND_TABLE's ServerClassInit did -- REFERENCE_SEND_TABLE names that global, and 25
// tables reach another table through it.

// One of a class's *secondary* tables: the members that name it, rather than the class's own set.
// CBaseCombatCharacter sends DT_BCCLocalPlayerExclusive and DT_BCCNonLocalPlayerExclusive besides
// its own table, and each is a separate namespace global with no class registration behind it.

// The table alone, for a class whose registration is written separately -- which is every form that
// opens a table without declaring a class: BEGIN_NETWORK_TABLE in shared code, and a bare
// BEGIN_SEND_TABLE or BEGIN_RECV_TABLE beside an IMPLEMENT_CLIENTCLASS, _FACTORY or _EVENT line.
// BEGIN_NETWORK_TABLE expands to BEGIN_SEND_TABLE on this side and BEGIN_RECV_TABLE on the other,
// so this is defined in both emitters under one name and the call site needs no guard.
// IMPLEMENT_SERVERCLASS_INTERNAL already supplies the ServerClass, GetServerClass,
// m_pClassSendTable and the forgot-to-declare stub; only the SendTable itself is missing. _NOBASE
// needs no separate macro: whether the baseclass prop is emitted is what NetTable.base records.

// What the server registers, its InterfaceRegs and ConVars: `ConVar x( side_registry(), ... )`.
ks::registry &side_registry();
