// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Spatial entity with simple radial falloff

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

#include "c_spatialentity.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_SpatialEntity, DT_SpatialEntity, CSpatialEntity )


void C_SpatialEntity::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitSpatialEntity();
	}
}

void C_SpatialEntity::InitSpatialEntity()
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	AddToPersonalSpatialEntityMgr();
	m_flWeight = 0.0f;
	m_flInfluence = 0.0f;
}

void C_SpatialEntity::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	RemoveFromPersonalSpatialEntityMgr();
}


//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_SpatialEntity::ShouldDraw()
{
	return false;
}

void C_SpatialEntity::ClientThink()
{
	if( !m_bEnabled && m_flCurWeight == 0.0f )
	{
		m_flWeight = 0.0f;
		return;
	}

	CBaseEntity *pPlayer = C_BasePlayer::GetLocalPlayer( 0 );
	if( !pPlayer )
		return;

	Vector playerOrigin = pPlayer->GetAbsOrigin();

	m_flWeight = 0.0f;

	if ( ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff )
	{
		float dist = (playerOrigin - m_vecOrigin).Length();
		m_flWeight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		m_flWeight = fpmax( 0.0f, m_flWeight );
		m_flWeight = fpmin( 1.0f, m_flWeight );

		m_flInfluence = fpmax( 0.0f, m_minFalloff - dist );
	}

	m_flWeight = m_flCurWeight * ( 1.0f - m_flWeight );

	BaseClass::ClientThink();
}
