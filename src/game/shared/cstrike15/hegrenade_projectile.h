#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "basecsgrenade_projectile.h"

class CHEGrenadeProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CHEGrenadeProjectile, CBaseCSGrenadeProjectile );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

// Overrides.
public:
	CHEGrenadeProjectile() {}

	KS_VIRTUAL void Spawn_Impl();
	KS_VIRTUAL void Precache_Impl();
	virtual void BounceSound( void );
	virtual void Detonate();
	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

// Grenade stuff.
public:

	static CHEGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner, 
		const CCSWeaponInfo& weaponInfo,
		float timer );

	void SetTimer( float timer );

	[[= ks::reflect::Input{ .name = "InitializeSpawnFromWorld", .type = FIELD_VOID } ]] void	InitializeSpawnFromWorld( inputdata_t &inputdata );

private:
	float m_flDetonateTime;
};
