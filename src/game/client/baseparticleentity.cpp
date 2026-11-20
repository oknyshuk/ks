// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "client_class.h"
#include "edict.h"
#include "particlemgr.h"
#include "predictable_entity.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
module ks.client;

import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :props;
import :util;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :sharedvar;
import :achievements_and_stats_interface;
import ks.game.mapentities_shared;

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( BaseParticleEntity, DT_BaseParticleEntity )

IMPLEMENT_REFLECT_TABLE( CBaseParticleEntity, DT_BaseParticleEntity );

IMPLEMENT_REFLECT_PREDMAP( CBaseParticleEntity );

REGISTER_EFFECT( CBaseParticleEntity );

CBaseParticleEntity::CBaseParticleEntity( void )
{
	m_bSimulate = true;
	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
}

CBaseParticleEntity::~CBaseParticleEntity( void )
{
}

void CBaseParticleEntity::Activate()
{
}	

void CBaseParticleEntity::Think()
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

int CBaseParticleEntity::GetToolParticleEffectId() const
{
	return m_nToolParticleEffectId;
}

int CBaseParticleEntity::AllocateToolParticleEffectId()
{
	m_nToolParticleEffectId = ParticleMgr()->AllocateToolParticleEffectId();
	return m_nToolParticleEffectId;
}
