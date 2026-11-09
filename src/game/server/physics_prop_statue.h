#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "props.h"

struct outer_collision_obb_t
{
	bool	bDirty;
	Vector	vecPos;
	Vector	vecMins;
	Vector	vecMaxs;
	QAngle	angAngles;

	std::vector< short >	iSpheres;
};


//-----------------------------------------------------------------------------
// Purpose: entity class for simple ragdoll physics
//-----------------------------------------------------------------------------

// UNDONE: Move this to a private header
class [[= ks::reflect::NetTable{ .name = "DT_StatueProp" } ]]
      CStatueProp : public CPhysicsProp
{
	DECLARE_CLASS( CStatueProp, CPhysicsProp );
	DECLARE_SERVERCLASS();

public:
	CStatueProp( void );

	void	Spawn_Impl( void );
	void	Precache_Impl();

	bool	CreateVPhysics_Impl();
	void	VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );

	//virtual float	GetAutoAimRadius() { return 24.0f; }

	void	ComputeWorldSpaceSurroundingBox_Impl( Vector *pMins, Vector *pMaxs );
	bool	TestCollision_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void	Event_Killed_Impl( const CTakeDamageInfo &info );

	virtual void	Freeze( float flFreezeAmount = -1.0f, CBaseEntity *pFreezer = nullptr, Ray_t *pFreezeRay = nullptr );

	void	CollisionPartnerThink( void );

private:

	bool	CreateVPhysicsFromHitBoxes( CBaseAnimating *pInitBaseAnimating );
	bool	CreateVPhysicsFromOBBs( CBaseAnimating *pInitBaseAnimating );

public:

	CNetworkHandle( CBaseAnimating,	m_hInitBaseAnimating, [[= ks::reflect::Net{} ]] );

	CNetworkVar( bool, m_bShatter, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nShatterFlags, [[= ks::reflect::Net{ .bits = 3 } ]] );
	CNetworkVector( m_vShatterPosition, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vShatterForce, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	const std::vector< outer_collision_obb_t >	*m_pInitOBBs;
};


CBaseEntity *CreateServerStatue( CBaseAnimating *pAnimating, int collisionGroup );
CBaseEntity *CreateServerStatueFromOBBs( const std::vector< outer_collision_obb_t > &vecSphereOrigins, CBaseAnimating *pChildEntity );
