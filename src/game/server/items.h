#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "entityoutput.h"
#include "player_pickup.h"
#include "vphysics/constraints.h"
#include "baseanimating.h"

// Armor given by a battery
inline constexpr auto MAX_NORMAL_BATTERY = 100;

// Ammo counts given by ammo items
inline constexpr auto SIZE_AMMO_PISTOL = 20;
inline constexpr auto SIZE_AMMO_PISTOL_LARGE = 100;
inline constexpr auto SIZE_AMMO_SMG1 = 45;
inline constexpr auto SIZE_AMMO_SMG1_LARGE = 225;
inline constexpr auto SIZE_AMMO_AR2 = 20;
inline constexpr auto SIZE_AMMO_AR2_LARGE = 100;
inline constexpr auto SIZE_AMMO_RPG_ROUND = 1;
inline constexpr auto SIZE_AMMO_SMG1_GRENADE = 1;
inline constexpr auto SIZE_AMMO_BUCKSHOT = 20;
inline constexpr auto SIZE_AMMO_357 = 6;
inline constexpr auto SIZE_AMMO_357_LARGE = 20;
inline constexpr auto SIZE_AMMO_CROSSBOW = 6;
inline constexpr auto SIZE_AMMO_AR2_ALTFIRE = 1;
inline constexpr auto SIZE_AMMO_FLECHETTE = 60;
inline constexpr auto SIZE_AMMO_URANIUM = 30;

inline constexpr auto SF_ITEM_START_CONSTRAINED = 0x00000001;
inline constexpr auto SF_ITEM_MUST_EXIST = 0x00000002; // prevent the procedural population system from modifying this item


class CItem : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CItem, CBaseAnimating );

	CItem();
	virtual ~CItem();

	void Spawn_Impl( void );
	void Precache_Impl();

	virtual bool HasBloatedCollision( void ) const { return true; } // Does this item increase its collision box to make it easier to pick up?

	CBaseEntity* Respawn_Impl( void );
	void ItemTouch( CBaseEntity *pOther );
	void ItemForceTouch( CBaseEntity *pOther );
	
	virtual void Materialize( void );
	virtual bool MyTouch( CBasePlayer *pPlayer ) { return false; };

	// Become touchable when we are at rest
	void OnEntityEvent_Impl( EntityEvent_t event, void *pEventData );

	// Activate when at rest, but don't allow pickup until then
	void ActivateWhenAtRest( float flTime = 0.5f );

	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );

	int	ObjectCaps_Impl() { return BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE | FCAP_WCEDIT_POSITION; };
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector	GetOriginalSpawnOrigin( void ) { return m_vOriginalSpawnOrigin;	}
	QAngle	GetOriginalSpawnAngles( void ) { return m_vOriginalSpawnAngles;	}
	void	SetOriginalSpawnOrigin( const Vector& origin ) { m_vOriginalSpawnOrigin = origin; }
	void	SetOriginalSpawnAngles( const QAngle& angles ) { m_vOriginalSpawnAngles = angles; }
	bool	CreateItemVPhysicsObject( void );
	virtual bool	ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer );
	DECLARE_DATADESC();
private:
	void ComeToRest( void );
	void ItemTouchInternal( CBaseEntity *pOther, bool bForceTouch );

private:
	bool		m_bActivateWhenAtRest;
	[[= ks::reflect::Key{ .name = "OnPlayerTouch" } ]] COutputEvent m_OnPlayerTouch;
	[[= ks::reflect::Key{ .name = "OnCacheInteraction" } ]] COutputEvent m_OnCacheInteraction;
	
	Vector		m_vOriginalSpawnOrigin;
	QAngle		m_vOriginalSpawnAngles;

	IPhysicsConstraint		*m_pConstraint;
};
