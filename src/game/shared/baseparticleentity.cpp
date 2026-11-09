// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#endif
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "baseparticleentity.h"

#ifdef CLIENT_DLL
#include "toolframework_client.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( BaseParticleEntity, DT_BaseParticleEntity )

IMPLEMENT_REFLECT_TABLE( CBaseParticleEntity, DT_BaseParticleEntity );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CBaseParticleEntity );
#endif

#ifdef CLIENT_DLL
REGISTER_EFFECT( CBaseParticleEntity );
#endif

CBaseParticleEntity::CBaseParticleEntity( void )
{
#if defined( CLIENT_DLL )
	m_bSimulate = true;
	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
#endif
}

CBaseParticleEntity::~CBaseParticleEntity( void )
{
#if defined( CLIENT_DLL )
#endif
}

#if !defined( CLIENT_DLL )
int CBaseParticleEntity::UpdateTransmitState_Impl( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return SetTransmitState( FL_EDICT_DONTSEND );

	if ( IsEFlagSet( EFL_IN_SKYBOX ) )
		return SetTransmitState( FL_EDICT_ALWAYS );

	// cull against PVS
	return SetTransmitState( FL_EDICT_PVSCHECK );
}
#endif

void CBaseParticleEntity::Activate_Impl()
{
#if !defined( CLIENT_DLL )
	BaseClass::Activate_Impl();
#endif
}	


void CBaseParticleEntity::Think_Impl()
{
	Remove( );
}


void CBaseParticleEntity::FollowEntity(CBaseEntity *pEntity)
{
	BaseClass::FollowEntity( pEntity );
	SetLocalOrigin( vec3_origin );
}


void CBaseParticleEntity::SetLifetime(float lifetime)
{
	if(lifetime == -1)
		SetNextThink( TICK_NEVER_THINK );
	else
		SetNextThink( gpGlobals->curtime + lifetime );
}

#if defined( CLIENT_DLL )
const Vector &CBaseParticleEntity::GetSortOrigin()
{
	// By default, we do the cheaper behavior of getting the root parent's abs origin, so we don't have to
	// setup any bones along the way. If this screws anything up, we can always make it an option.
	return GetRootMoveParent()->GetAbsOrigin();
}

void CBaseParticleEntity::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	// If you derive from CBaseParticleEntity, you must implement simulation and rendering.
	contract_assert( false );
}

void CBaseParticleEntity::RenderParticles( CParticleRenderIterator *pIterator )
{
	// If you derive from CBaseParticleEntity, you must implement simulation and rendering.
	contract_assert( false );
}

#endif
