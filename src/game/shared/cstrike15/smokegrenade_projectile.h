//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SMOKEGRENADE_PROJECTILE_H
#define SMOKEGRENADE_PROJECTILE_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "basecsgrenade_projectile.h"

#if defined( CLIENT_DLL )

class [[= ks::reflect::NetTable{ .name = "DT_SmokeGrenadeProjectile" } ]]
      [[= ks::reflect::From<"m_bDidSmokeEffect", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_nSmokeEffectTickBegin", ks::reflect::Net{}>{} ]]
      C_SmokeGrenadeProjectile : public C_BaseCSGrenadeProjectile
{
	public:
	DECLARE_CLASS( C_SmokeGrenadeProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	C_SmokeGrenadeProjectile() {}
	virtual ~C_SmokeGrenadeProjectile();

	virtual void PostDataUpdate( DataUpdateType_t updateType ) OVERRIDE;
	virtual void OnDataChanged( DataUpdateType_t updateType ) OVERRIDE;

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }
	void SpawnSmokeEffect();

	CNetworkVar( int, m_nSmokeEffectTickBegin );
	CNetworkVar( bool, m_bDidSmokeEffect );

	bool m_bSmokeEffectSpawned;
};

#else // GAME_DLL
class [[= ks::reflect::NetTable{ .name = "DT_SmokeGrenadeProjectile" } ]]
      CSmokeGrenadeProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CSmokeGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

public:
// Overrides.
public:
	virtual void Spawn();
	virtual void Precache();
	virtual void Detonate();
	virtual void OnBounced( void );
	virtual void BounceSound( void );

	void Think_Detonate();
	void Think_Fade();
	void Think_Remove();

	void SmokeDetonate( void );
	void RemoveGrenadeFromLists( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

// Grenade stuff.
public:

	static CSmokeGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );

	void SetTimer( float timer );

	CNetworkVar( int, m_nSmokeEffectTickBegin, [[= ks::reflect::Net{ .bits = -1 } ]] );
	CNetworkVar( bool, m_bDidSmokeEffect, [[= ks::reflect::Net{} ]] );
	Vector m_vSmokeColor;
	float m_flLastBounce;
};
#endif // GAME_DLL

#endif // SMOKEGRENADE_PROJECTILE_H
