// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Emits datamap entries for annotated members. Unlike SendProp there is no factory to call --
// the DEFINE_* macros are aggregate initialisers -- so the emitter builds typedescription_t
// directly and byte-identity is a straight field comparison.
//
// One Key annotation covers both kinds, because which kind it is *is* derivable: a member
// whose type derives from CBaseEntityOutput is an output (DEFINE_OUTPUT), anything else is a
// keyfield (DEFINE_KEYFIELD). The map name is the only thing that cannot be derived.

#ifndef KS_REFLECT_DATAMAP_H
#define KS_REFLECT_DATAMAP_H

#include "reflect.h"
#include "reflect_fielddesc.h"
#include "reflect_table_check.h"
#include "datamap.h"
#ifdef GAME_DLL
#include "entityoutput.h"
#else
// Outputs are server-side entity I/O, so on the client this stays incomplete and
// derives_from answers false for every member -- which is correct, not a degradation.
class CBaseEntityOutput;
#endif

#include <vector>

namespace ks::reflect::dmap
{

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

template <std::meta::info M>
consteval FieldDesc desc_of( const Key &k )
{
	const std::meta::info t = std::meta::type_of( M );

	FieldDesc d;
	d.name     = intern( std::meta::identifier_of( M ) );
	d.external = k.name.empty() ? intern( std::meta::identifier_of( M ) ) : intern( k.name );
	// Keyfields index the member itself, so the offset carries no networkvar flags.
	d.offset   = static_cast<int>( byte_offset_of( M ) );
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
				v.push_back( make_field( desc_of<m>( k ), is_output( m ) ) );

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

#ifdef GAME_DLL
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
#endif
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

} // namespace ks::reflect::dmap


// Replaces BEGIN_DATADESC/END_DATADESC once the equality gate has proven the generated map
// interchangeable with the legacy one. The three definitions are what BEGIN_DATADESC provided; the
// map is filled in a static initializer, which is when BEGIN_DATADESC_GUTS filled its own, so the
// timing is unchanged. m_DataMap is reached through unchecked reflection because DECLARE_DATADESC
// leaves it in whatever access section the class happens to be in -- usually private.
#define IMPLEMENT_REFLECT_DATAMAP( className )                                                  \
	datamap_t className::m_DataMap = { 0, 0, #className, nullptr };                                  \
	datamap_t *className::GetDataDescMap( void ) { return &m_DataMap; }                            \
	datamap_t *className::GetBaseMap()                                                            \
	{                                                                                             \
		datamap_t *pResult;                                                                       \
		DataMapAccess( (BaseClass *)nullptr, &pResult );                                             \
		return pResult;                                                                           \
	}                                                                                             \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                           \
		{                                                                                         \
			auto &f = ks::reflect::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			Map().baseMap       = &[: ks::reflect::static_member_of(                               \
			    ks::reflect::base_with_static_member( ^^className, "m_DataMap" ),                  \
			    "m_DataMap" ) :];                                                                   \
			return true;                                                                           \
		}();                                                                                     \
		static void CheckFilled()                                                                 \
		{                                                                                         \
			if ( !Map().dataDesc || Map().dataNumFields <= 0 )                                     \
				ks::reflect::ReportDiff( #className, "(map)", "live",                             \
				                         "reflect datamap is empty at runtime" );                  \
		}                                                                                         \
		static ks::reflect::VerifyRegistrar g_check( CheckFilled );                                \
	}

// The base-less mirror: BEGIN_SIMPLE_DATADESC / BEGIN_DATADESC_NO_BASE both set GetBaseMap() to
// return NULL rather than walk BaseClass, since a class using either has none to walk.
#define IMPLEMENT_REFLECT_DATAMAP_SIMPLE( className )                                             \
	datamap_t className::m_DataMap = { 0, 0, #className, nullptr };                                  \
	datamap_t *className::GetBaseMap() { return nullptr; }                                           \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ks::reflect::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			return true;                                                                           \
		}();                                                                                     \
		static void CheckFilled()                                                                 \
		{                                                                                         \
			if ( !Map().dataDesc || Map().dataNumFields <= 0 )                                     \
				ks::reflect::ReportDiff( #className, "(map)", "live",                             \
				                         "reflect datamap is empty at runtime" );                  \
		}                                                                                         \
		static ks::reflect::VerifyRegistrar g_check( CheckFilled );                                \
	}


// BEGIN_SIMPLE_DATADESC_ is the one form that walks a named base *without* defining the virtual
// GetDataDescMap() -- so it is neither _SIMPLE (GetBaseMap returns NULL) nor _NO_BASE (which does
// define the virtual). baseMap is assigned here because BEGIN_DATADESC_GUTS assigned it too, and
// through DataMapAccess rather than className::GetBaseMap(): that member is protected, and unlike
// the legacy DataMapInit this lambda is not a friend.
#define IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( className, baseClass )                                  \
	datamap_t className::m_DataMap = { 0, 0, #className, nullptr };                                  \
	datamap_t *className::GetBaseMap()                                                            \
	{                                                                                             \
		datamap_t *pResult;                                                                       \
		DataMapAccess( (baseClass *)nullptr, &pResult );                                             \
		return pResult;                                                                           \
	}                                                                                             \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ks::reflect::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			datamap_t *pBase = nullptr;                                                              \
			DataMapAccess( (baseClass *)nullptr, &pBase );                                           \
			Map().baseMap       = pBase;                                                          \
			return true;                                                                           \
		}();                                                                                     \
		static void CheckFilled()                                                                 \
		{                                                                                         \
			if ( !Map().dataDesc || Map().dataNumFields <= 0 )                                     \
				ks::reflect::ReportDiff( #className, "(map)", "live",                             \
				                         "reflect datamap is empty at runtime" );                  \
		}                                                                                         \
		static ks::reflect::VerifyRegistrar g_check( CheckFilled );                                \
	}


// BEGIN_DATADESC_NO_BASE differs from BEGIN_SIMPLE_DATADESC by one thing: it still defines the
// virtual GetDataDescMap() override, because a class reached with DECLARE_DATADESC() (rather than
// DECLARE_SIMPLE_DATADESC(), which does not declare it at all) needs one. Omitting it left that
// virtual's key-function homing without a definition, and the linker reported the whole vtable
// missing rather than one function: undefined symbol: _ZTV24CFourWheelVehiclePhysics.
#define IMPLEMENT_REFLECT_DATAMAP_NO_BASE( className )                                             \
	datamap_t className::m_DataMap = { 0, 0, #className, nullptr };                                  \
	datamap_t *className::GetDataDescMap( void ) { return &m_DataMap; }                            \
	datamap_t *className::GetBaseMap() { return nullptr; }                                           \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ks::reflect::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			return true;                                                                           \
		}();                                                                                     \
		static void CheckFilled()                                                                 \
		{                                                                                         \
			if ( !Map().dataDesc || Map().dataNumFields <= 0 )                                     \
				ks::reflect::ReportDiff( #className, "(map)", "live",                             \
				                         "reflect datamap is empty at runtime" );                  \
		}                                                                                         \
		static ks::reflect::VerifyRegistrar g_check( CheckFilled );                                \
	}

#endif // KS_REFLECT_DATAMAP_H
