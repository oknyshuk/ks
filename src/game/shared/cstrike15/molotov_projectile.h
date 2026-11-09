#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "basecsgrenade_projectile.h"

#if defined( CLIENT_DLL )
	
class [[= ks::reflect::NetTable{ .name = "DT_MolotovProjectile" } ]]
      [[= ks::reflect::From<"m_bIsIncGrenade", ks::reflect::Net{}>{} ]]
      C_MolotovProjectile : public C_BaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( C_MolotovProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

protected:
	CNetworkVar( bool, m_bIsIncGrenade );

private:
	ks::tracked_ref<CNewParticleEffect> m_molotovParticleEffect;

};

#else // GAME_DLL

class [[= ks::reflect::NetTable{ .name = "DT_MolotovProjectile" } ]]
      CMolotovProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CMolotovProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

// Overrides.
public:
	KS_VIRTUAL void Spawn_Impl( void );
	KS_VIRTUAL void Precache_Impl( void );

	virtual void BounceTouch( CBaseEntity *other );
	virtual void BounceSound( void );

	virtual void Detonate( void );

//	virtual int GetDamageType( void ) const { return DMG_BURN; }
	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_FIRE; }

	void SetIsIncGrenade( bool bIsIncGrenade ) { m_bIsIncGrenade = bIsIncGrenade; }

// Grenade stuff.
public:

	static CMolotovProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );

	[[= ks::reflect::Input{ .name = "InitializeSpawnFromWorld", .type = FIELD_VOID } ]] void	InitializeSpawnFromWorld( inputdata_t &inputdata );

protected:
	CNetworkVar( bool, m_bIsIncGrenade, [[= ks::reflect::Net{} ]] );

private:
	void DetonateThink( void );
	void SetTimer( float timer );

	IntervalTimer m_stillTimer;
	IntervalTimer m_throwDetTimer;
	float m_flDetonateTime;
};

#endif // GAME_DLL
