// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The dispatchers of CBaseEntity (see entity_dispatch.h): for every method of the list a switch over
// the instantiable kinds that calls the kind's thunk.

#include "baseentity.h"

[[noreturn]] void EntityKindFailure( const char *pszWhat, const char *pszClassname, int nKind )
{
	Error( "entity kind contract violated: %s (entity '%s', kind %d): entities must be made by the entity factory (_CreateEntityTemplate)\n",
		pszWhat, pszClassname, nKind );
	__builtin_trap();
}

[[noreturn]] void EntityDispatchFailure( EntityKind kind, const char *pszMethod )
{
	EntityKindFailure( pszMethod, kind == EntityKind::None ? "(no kind)" : "(not an instantiable kind)", static_cast<int>( kind ) );
}

#define KS_ENT_CASE_1( cls, Name, Args ) \
	case EntityKind::cls: return entity_dispatch::thunk_##Name< EntityKind::cls >( KS_ENT_ARGS Args );
#define KS_ENT_CASE_0( cls, Name, Args )
#define KS_ENT_CASE_PICK( cls, parent, instantiable, Name, Args ) KS_ENT_CASE_##instantiable( cls, Name, Args )

#define KS_ENT_DEFINE_DISPATCHER( Ret, Name, Const, Params, Args, ... ) \
	Ret CBaseEntity::Name Params Const \
	{ \
		switch ( m_kind ) \
		{ \
		KS_ENTITY_KINDS( KS_ENT_CASE_PICK, Name, Args ) \
		default: EntityDispatchFailure( m_kind, #Name ); \
		} \
	}
KS_ENTITY_METHODS( KS_ENT_DEFINE_DISPATCHER )
#undef KS_ENT_DEFINE_DISPATCHER
