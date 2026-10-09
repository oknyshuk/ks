#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Used to influence the initial force for a dying NPC's ragdoll.
//			Passive entity. Just represents position in the world, radius, force

#include "reflect_annotations.h"
#include "baseentity.h"

#define SF_RAGDOLLMAGNET_BAR	0x00000002	// this is a bar magnet.

class CRagdollMagnet : public CPointEntity
{
public:
	DECLARE_CLASS( CRagdollMagnet, CPointEntity );
	DECLARE_DATADESC();

	Vector GetForceVector( CBaseEntity *pNPC );
	float GetRadius( void ) { return m_radius; }
	Vector GetAxisVector( void ) { return m_axis - GetAbsOrigin(); }
	float DistToPoint( const Vector &vecPoint );

	bool IsEnabled( void ) { return !m_bDisabled; }
	
	int IsBarMagnet( void ) { return (m_spawnflags & SF_RAGDOLLMAGNET_BAR); }

	static CRagdollMagnet *FindBestMagnet( CBaseEntity *pNPC );

	void Enable( bool bEnable ) { m_bDisabled = !bEnable; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;
	[[= ks::reflect::Key{ .name = "radius" } ]] float	m_radius;
	[[= ks::reflect::Key{ .name = "force" } ]] float	m_force;
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector	m_axis;
};
