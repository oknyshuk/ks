// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once

#define KS_ENT_UNPAREN( ... ) __VA_ARGS__
#define KS_ENT_ARGS( ... ) this __VA_OPT__( , ) __VA_ARGS__
#define KS_ENT_SIGNATURE( Const, ... ) Const CBaseEntity *pSelfEntity __VA_OPT__( , ) __VA_ARGS__
// the key of the tables of an entry of the list
#define KS_ENT_MEMBER( Ret, Name, Const, Params ) std::define_static_string( #Name ), Ret ( * )( KS_ENT_SIGNATURE( Const, KS_ENT_UNPAREN Params ) )
#define KS_ENTITY_DEFINE_THUNKS( Class ) \
	static_assert( ( entity_kinds::check_registered_class< Class >(), true ) ); \
	template <> bool entity_dispatch::registered< kind_of<Class> > = entity_dispatch::register_kind< Class >();
// a listed class that is not instantiable but declares methods of the list its descendants run
#define KS_ENTITY_ABSTRACT_KIND( Class ) \
	template <> bool entity_dispatch::registered< kind_of<Class> > = entity_dispatch::register_kind< Class >();
