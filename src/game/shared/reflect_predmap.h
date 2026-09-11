// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Emits prediction descriptor entries for members carrying Pred. Client-only, like the
// prediction system itself. DEFINE_PRED_FIELD names no external string, so unlike the datamap
// nothing here needs a name -- only the flags and tolerance, which are the two things that
// genuinely cannot be derived.

#ifndef KS_REFLECT_PREDMAP_H
#define KS_REFLECT_PREDMAP_H

#include "reflect.h"
#include "reflect_fielddesc.h"
#include "reflect_table_check.h"

#include <vector>

namespace ks::reflect::pred
{

// An embedded entry: the member's type has a prediction map of its own, which is what
// DEFINE_PRED_TYPEDESCRIPTION names. The flags are the macro's own, and it sets no size.
consteval bool is_embedded_map( std::meta::info m )
{
	return tag_of( m ) == FIELD_EMBEDDED
	       && has_static_member( unwrap( std::meta::type_of( m ) ), "m_PredMap" );
}

template <std::meta::info M>
consteval FieldDesc desc_of()
{
	const std::meta::info t = std::meta::type_of( M );
	const Pred p = get<Pred>( M );

	FieldDesc d;
	d.type      = tag_of( M );
	d.name      = intern( std::meta::identifier_of( M ) );
	d.external  = nullptr;
	d.offset    = static_cast<int>( byte_offset_of( M ) );
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
				FieldDesc d = desc_of<m>();
				d.td = &[: static_member_of( unwrap( std::meta::type_of( m ) ), "m_PredMap" ) :];
				v.push_back( MakeField( d ) );
			}
			else
			{
				v.push_back( MakeField( desc_of<m>() ) );
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

} // namespace ks::reflect::pred

// Replaces BEGIN_PREDICTION_DATA / END_PREDICTION_DATA. m_PredMap is public -- DECLARE_PREDICTABLE
// opens with `public:` -- so unlike the datamap it can be named directly. BaseClass resolves here
// because the initialiser of a static data member is in its class's scope, which is how the macro
// this replaces spelled it too.
#define IMPLEMENT_REFLECT_PREDMAP_BASE( className, base )                                         \
	datamap_t className::m_PredMap = { 0, 0, #className, base };                                   \
	datamap_t *className::GetPredDescMap( void ) { return &m_PredMap; }                            \
	namespace className##_ReflectPredInit                                                          \
	{                                                                                              \
		static const bool g_filled = []                                                             \
		{                                                                                            \
			auto &f = ks::reflect::pred::fields<className>();                                          \
			className::m_PredMap.dataDesc      = f.data();                                             \
			className::m_PredMap.dataNumFields = (int)f.size();                                        \
			return true;                                                                               \
		}();                                                                                        \
		static void CheckFilled()                                                                    \
		{                                                                                            \
			const int expect = (int)ks::reflect::pred::fields<className>().size();                      \
			if ( className::m_PredMap.dataNumFields != expect )                                        \
				ks::reflect::ReportDiff( #className, "(predmap)", "live",                                \
				                         "reflect prediction map was not filled" );                      \
		}                                                                                            \
		static ks::reflect::VerifyRegistrar g_check( CheckFilled );                                   \
	}

#define IMPLEMENT_REFLECT_PREDMAP( className )                                                     \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, &BaseClass::m_PredMap )

#define IMPLEMENT_REFLECT_PREDMAP_NO_BASE( className )                                            \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, nullptr )

#endif // KS_REFLECT_PREDMAP_H
