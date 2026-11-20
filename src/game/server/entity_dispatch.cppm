// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include "eiface.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "ai_criteria.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
module ks.server:entity_dispatch;

import ks.platform;
import ks.simd;
import :entity_kind;
import std;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import ks.game.shareddefs;

// Dispatch of the former virtual methods of CBaseEntity over the closed set of entity kinds
// (entity_kind.h).
//
// For every method M in entity_dispatch_list.h:
//   - CBaseEntity::M is not virtual: it calls table_M[owner_M[m_kind]] (datamap.cpp);
//   - the implementation every class used to override is named M_Impl. CBaseEntity::M_Impl is the
//     default; a class that overrides M defines M_Impl, and calls its base class' with
//     BaseClass::M_Impl() (a plain qualified call, exactly what BaseClass::M() was);
//   - LINK_ENTITY_TO_CLASS registers the kind (register_kind, at static initialization, in the
//     translation unit that sees the class' definition): for each M that its class declares, the thunk
//     `static_cast<C *>( pSelfEntity )->C::M_Impl( args )`; for the others, the listed base kind that
//     declares M (found by reflection), whose thunk runs it.
// So calling M on any entity pointer or reference, of whatever static type, runs the M of the
// entity's most-derived class; M_Impl is only ever called as a qualified base-class call. A kind that
// lends thunks but is never registered is a link error (entity_dispatch::registered), a missing
// m_kind a fatal error at the call.

// Fatal: an entity whose kind is None or not instantiable received a dispatched call.
[[noreturn]] void EntityDispatchFailure( EntityKind kind, const char *pszMethod );

namespace entity_dispatch
{
// Per method: the thunks by kind and, per instantiable kind, the kind whose thunk runs the method for it.
// A kind only has a thunk for the methods its class (or an unlisted class between it and its listed
// base, such as a CAI_ExpresserHost<> mixin) declares; the others run the thunk of the listed base that
// declares them, so a class costs a thunk per method it overrides, not per method of the list. The tables
// are keyed by the method's name and thunk type, so overloads get their own.
template < const char *Name, class Fn > inline constinit Fn table[entity_kinds::kCount] {};
template < const char *Name, class Fn > inline constinit EntityKind owner[entity_kinds::kCount] {};

consteval std::meta::info entity_base_of( std::meta::info t )
{
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
		if ( const std::meta::info bt = std::meta::remove_cv( std::meta::type_of( b ) ); entity_kinds::derives_from_entity( bt ) )
			return bt;
	return {};
}

// t and its entity bases up to CBaseEntity.
consteval std::vector<std::meta::info> chain_of( std::meta::info t )
{
	std::vector<std::meta::info> chain;
	for ( ; t != std::meta::info {}; t = entity_base_of( t ) ) chain.push_back( t );
	return chain;
}

// The _Impl members class t declares itself.
consteval std::vector<const char *> declared_impls_of( std::meta::info t )
{
	std::vector<const char *> names;
	for ( auto m : std::meta::members_of( t, std::meta::access_context::unchecked() ) )
		if ( std::meta::has_identifier( m ) && std::meta::identifier_of( m ).ends_with( "_Impl" ) )
			names.push_back( std::define_static_string( std::meta::identifier_of( m ) ) );
	return names;
}
template < std::meta::info T > inline constexpr std::span<const char *const> declared_impls = std::define_static_array( declared_impls_of( T ) );

// The names of the list's methods' _Impl, in list order (overloads repeat a name).
#define KS_ENT_IMPL_NAME( Ret, Name, ... ) #Name "_Impl",
inline constexpr std::string_view kImplNames[] = { KS_ENTITY_METHODS( KS_ENT_IMPL_NAME ) };
#undef KS_ENT_IMPL_NAME
inline constexpr std::size_t kMethodCount = std::size( kImplNames );

// Per class C, for every entry of the list, the kind whose thunk runs it: the nearest listed class at or
// below the class that declares the entry's _Impl (CBaseEntity, which declares them all, at the end).
template < class C > consteval std::array<EntityKind, kMethodCount> owner_kinds_of()
{
	std::array<EntityKind, kMethodCount> kinds;
	kinds.fill( EntityKind::CBaseEntity );
	std::array<bool, kMethodCount> found {};
	EntityKind listed = EntityKind::None;
	template for ( constexpr std::meta::info t : std::define_static_array( chain_of( ^^C ) ) )
	{
		if constexpr ( t != ^^CBaseEntity )
		{
			if ( const EntityKind k = entity_kinds::listed_kind_of( t ); k != EntityKind::None ) listed = k;
			for ( const char *n : declared_impls<t> )
				for ( std::size_t i = 0; i < kMethodCount; ++i )
					if ( !found[i] && kImplNames[i] == n ) { found[i] = true; kinds[i] = listed; }
		}
	}
	return kinds;
}
template < class C > inline constexpr auto owner_kinds = owner_kinds_of<C>();

// Defined by the registration of kind K (KS_ENTITY_DEFINE_THUNKS / KS_ENTITY_ABSTRACT_KIND). A registration
// references the ones of the kinds it borrows thunks from: a kind that is never registered is a link error.
template < EntityKind K > extern bool registered;

#define KS_ENT_REGISTER( Ret, Name, Const, Params, Args, C ) \
	{ \
		constexpr EntityKind from = owner_kinds<C>[__COUNTER__ - first - 1]; \
		owner< KS_ENT_MEMBER( Ret, Name, Const, Params ) >[k] = from; \
		if constexpr ( from == kind_of<C> ) \
			table< KS_ENT_MEMBER( Ret, Name, Const, Params ) >[k] = []( KS_ENT_SIGNATURE( Const, KS_ENT_UNPAREN Params ) ) -> Ret \
				{ return static_cast< Const C * >( pSelfEntity )->C::Name##_Impl Args; }; \
		else \
			( void )&registered<from>; \
	}

template < class C > bool register_kind()
{
	constexpr int k = static_cast<int>( kind_of<C> );
	constexpr int first = __COUNTER__; // the entries below number themselves in list order
	KS_ENTITY_METHODS( KS_ENT_REGISTER, C )
	return true;
}
#undef KS_ENT_REGISTER
}
