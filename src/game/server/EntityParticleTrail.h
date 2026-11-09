#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "networkstringtable_gamedll.h"

#include "baseparticleentity.h"
#include "entityparticletrail_shared.h"

//-----------------------------------------------------------------------------
// Spawns particles after  the entity
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrail" } ]]
      [[= ks::reflect::From<"m_Info", ks::reflect::Net{}>{} ]]
      CEntityParticleTrail : public CBaseParticleEntity 
{
	DECLARE_CLASS( CEntityParticleTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

public:
	static CEntityParticleTrail	*Create( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info, CBaseEntity *pConstraint );
	static void Destroy( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info );

	void Spawn_Impl();
	void UpdateOnRemove_Impl();

	// Force our constraint entity to be trasmitted
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	// Clean up when the entity goes away.
	void NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

private:
	void	AttachToEntity( CBaseEntity *pTarget );
	void	IncrementRefCount();
	void	DecrementRefCount();
	
	CNetworkVar( int, m_iMaterialName, [[= ks::reflect::Net{ .bits = MAX_MATERIAL_STRING_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVarEmbedded( EntityParticleTrailInfo_t, m_Info );
	CNetworkHandle( CBaseEntity, m_hConstraintEntity, [[= ks::reflect::Net{} ]] );

	int	m_nRefCount;
};
