#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Dispatch of the former virtual methods of CBaseEntity over the closed set of entity kinds
// (entity_kind.h).
//
// For every method M in entity_dispatch_list.h:
//   - CBaseEntity::M is not virtual: it switches on the kind of the entity (m_kind) and calls the
//     entity_dispatch::thunk_M<kind> of that kind (entity_dispatch.cpp, generated from the lists);
//   - the implementation every class used to override is named M_Impl. CBaseEntity::M_Impl is the
//     default; a class that overrides M defines M_Impl, and calls its base class' with
//     BaseClass::M_Impl() (a plain qualified call, exactly what BaseClass::M() was);
//   - the thunk of a kind is defined by LINK_ENTITY_TO_CLASS, in the translation unit that sees the
//     class' definition: `return static_cast<C *>( pSelfEntity )->C::M_Impl( args )`. The qualified call
//     finds the final overrider of M for C by ordinary name lookup: C's own M_Impl, or the nearest
//     base class' (CBaseEntity::M_Impl at the end).
// So calling M on any entity pointer or reference, of whatever static type, runs the M of the
// entity's most-derived class; M_Impl is only ever called as a qualified base-class call. A kind that
// is missing in a build is a link error (undefined thunk), a missing m_kind a fatal error at the call.

#include "entity_kind.h"
#include "entity_dispatch_list.h"

#define KS_ENT_UNPAREN( ... ) __VA_ARGS__
#define KS_ENT_ARGS( ... ) this __VA_OPT__( , ) __VA_ARGS__
#define KS_ENT_SIGNATURE( Const, ... ) Const CBaseEntity *pSelfEntity __VA_OPT__( , ) __VA_ARGS__
#define KS_ENT_FORWARD( ... ) pSelfEntity __VA_OPT__( , ) __VA_ARGS__

// Fatal: an entity whose kind is None or not instantiable received a dispatched call.
[[noreturn]] void EntityDispatchFailure( EntityKind kind, const char *pszMethod );

namespace entity_dispatch
{
	// one function template per method; its specializations are defined per class by KS_ENTITY_DEFINE_THUNKS
#define KS_ENT_DECLARE_THUNK( Ret, Name, Const, Params, Args, ... ) \
	template < EntityKind K > Ret thunk_##Name( KS_ENT_SIGNATURE( Const, KS_ENT_UNPAREN Params ) );
	KS_ENTITY_METHODS( KS_ENT_DECLARE_THUNK )
#undef KS_ENT_DECLARE_THUNK
}

// the thunks of class Class (the macro below, applied to every method of the list)
#define KS_ENT_DEFINE_THUNK( Ret, Name, Const, Params, Args, Class ) \
	template <> Ret entity_dispatch::thunk_##Name< kind_of<Class> >( KS_ENT_SIGNATURE( Const, KS_ENT_UNPAREN Params ) ) \
	{ \
		return static_cast< Const Class * >( pSelfEntity )->Class::Name##_Impl Args; \
	}

// Used by LINK_ENTITY_TO_CLASS: checks the registration against the kind list, defines the thunks.
#define KS_ENTITY_DEFINE_THUNKS( Class ) \
	static_assert( ( entity_kinds::check_registered_class< Class >(), true ) ); \
	KS_ENTITY_METHODS( KS_ENT_DEFINE_THUNK, Class )
