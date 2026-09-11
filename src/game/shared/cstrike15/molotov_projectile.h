//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if !defined( MOLOTOV_PROJECTILE_H )
#define MOLOTOV_PROJECTILE_H

#include "reflect_annotations.h"

#if defined( _WIN32 )
	#pragma once
#endif

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
	CUtlReference<CNewParticleEffect> m_molotovParticleEffect;

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
	virtual void Spawn( void );
	virtual void Precache( void );

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

#endif // MOLOTOV_PROJECTILE_H
