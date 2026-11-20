// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "eiface.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "../predictable_entity.h"
#include "../reflect_sendtable.h"
#include "../util.h"
#include "tier2/resourceprecacher.h"
#include "declare_class.h"
module ks.server;

import ks.core.types;
import ks.game.iplayeranimstate;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import ks.game.ai_activity;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.math.compressed_vector;
import ks.math.vector;
import :cstrike15.Effects.chicken;
import :cstrike15.hostage.cs_simple_hostage;

class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

class CWeaponM3 : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponM3" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponM3, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponM3();

	virtual void PrimaryAttack();
	virtual bool Reload();
	virtual void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_M3; }

	virtual int GetShotgunReloadState( void ) { return m_reloadState; }

private:

	CWeaponM3( const CWeaponM3 & );

	float m_flPumpTime;
	CNetworkVar( int, m_reloadState, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponM3, DT_WeaponM3 )

IMPLEMENT_REFLECT_TABLE( CWeaponM3, DT_WeaponM3 );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_m3, WeaponM3 );
// PRECACHE_REGISTER( weapon_m3 );

CWeaponM3::CWeaponM3()
{
	m_flPumpTime = 0;
	m_reloadState = 0;
}

void CWeaponM3::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float flCycleTime = GetCycleTime();

	// don't fire underwater
	if (pPlayer->GetWaterLevel() == WL_Eyes)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15;
		return;
	}

	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 == 0 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}

		return;
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

    uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	// Dispatch the FX right away with full accuracy.
	float flCurAttack = CalculateNextAttackTime( flCycleTime );
	FX_FireBullets( 
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(), 
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed( true ) & 255, // wrap it for network traffic so it's the same between client and server
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		flCurAttack,
		SINGLE,
		m_flRecoilIndex );

	// are we firing the last round in the clip?
	if ( m_iClip1 == 1 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 0.875f );
	}
	else
	{
		m_flPumpTime = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 2.5f );
	}

	m_reloadState = 0;

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), Primary_Mode );

	// table driven recoil
	Recoil( Primary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponM3::Reload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 || m_iClip1 == GetMaxClip1())
		return true;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > gpGlobals->curtime)
		return true;
		
	// check to see if we're ready to reload
	if (m_reloadState == 0)
	{
		pPlayer->SetAnimation( PLAYER_RELOAD );

		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_reloadState = 1;
		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5 );

		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_START );

		return true;
	}
	else if (m_reloadState == 1)
	{
		if (m_flTimeWeaponIdle > gpGlobals->curtime)
			return true;
		// was waiting for gun to move to side
		m_reloadState = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5 );
		if ( m_iClip1 == 7 )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );
		}
		else
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
		}
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
		SendActivityEvents();
		
		CCSPlayer *pPlayer = GetPlayerOwner();

		if ( pPlayer )
			 GiveReserveAmmo( AMMO_POSITION_PRIMARY, -1 );

		m_reloadState = 1;
	}

	return true;
}

void CWeaponM3::WeaponIdle()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if (m_flPumpTime && m_flPumpTime < gpGlobals->curtime)
	{
		// play pumping sound
		m_flPumpTime = 0;
	}

	if (m_flTimeWeaponIdle < gpGlobals->curtime)
	{
		if (m_iClip1 == 0 && m_reloadState == 0 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ))
		{
			Reload( );
		}
		else if (m_reloadState != 0)
		{
			if (m_iClip1 != 8 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ))
			{
				Reload( );
			}
			else
			{
				// reload debounce has timed out
				//MIKETODO: shotgun anims
				SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );
				
				// play cocking sound
				m_reloadState = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5 );
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( MolotovGrenade, DT_MolotovGrenade )

IMPLEMENT_REFLECT_TABLE( CMolotovGrenade, DT_MolotovGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_molotov, MolotovGrenade );
PRECACHE_REGISTER( weapon_molotov );

void CMolotovGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	// [mlowrance] were throwing the grenade, be sure to remove flame sound effect
	SetLoopingSoundPlaying( false );
	StopSound( "Molotov.IdleLoop" ); 
	//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 2\n" );
	CMolotovProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CMolotovGrenade::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
	
	PrecacheScriptSound( "Molotov.IdleLoop" );
}

void CMolotovGrenade::Drop(const Vector& vecVelocity)
{
	CBaseCSGrenade::Drop(vecVelocity);
	StopSound( "Molotov.IdleLoop" ); 
	SetLoopingSoundPlaying( false );
}

IMPLEMENT_NETWORKCLASS_ALIASED( IncendiaryGrenade, DT_IncendiaryGrenade )

	IMPLEMENT_REFLECT_TABLE( CIncendiaryGrenade, DT_IncendiaryGrenade );

	//BEGIN_PREDICTION_DATA( CIncendiaryGrenade )
	//END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_incgrenade, IncendiaryGrenade );
PRECACHE_REGISTER( weapon_incgrenade );

void CIncendiaryGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	// [mlowrance] were throwing the grenade, be sure to remove flame sound effect
	SetLoopingSoundPlaying( false );
	StopSound( "Molotov.IdleLoop" ); 
	//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 2\n" );
	CMolotovProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CIncendiaryGrenade::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	PrecacheScriptSound( "Molotov.IdleLoop" );
}

class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

class CWeaponNOVA : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponNOVA" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponNOVA, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponNOVA( void );

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_NOVA; }

	virtual int GetShotgunReloadState( void ) { return m_reloadState; }

private:

	CWeaponNOVA( const CWeaponNOVA & );

	float m_flPumpTime;
	CNetworkVar( int, m_reloadState, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponNOVA, DT_WeaponNOVA )

IMPLEMENT_REFLECT_TABLE( CWeaponNOVA, DT_WeaponNOVA );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_nova, WeaponNOVA );
// PRECACHE_REGISTER( weapon_nova );

CWeaponNOVA::CWeaponNOVA( void )
{
	m_flPumpTime = 0.0f;
	m_reloadState = 0;
}

void CWeaponNOVA::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float flCycleTime = GetCycleTime();

	// don't fire underwater
	if ( pPlayer->GetWaterLevel() == WL_Eyes )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 == 0 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		}

		return;
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

    uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	// Dispatch the FX right away with full accuracy.
	float flCurAttack = CalculateNextAttackTime( flCycleTime );
	FX_FireBullets( 
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(), 
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed( true ) & 255, // wrap it for network traffic so it's the same between client and server
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		flCurAttack,
		SINGLE,
		m_flRecoilIndex );

	// are we firing the last round in the clip?
	if ( m_iClip1 == 1 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 0.875f );
	}
	else
	{
		m_flPumpTime = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 2.5f );
	}

	m_reloadState = 0;

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), Primary_Mode );

	// table driven recoil
	Recoil( Primary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponNOVA::Reload( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 || m_iClip1 == GetMaxClip1() )
		return true;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return true;
		
	// check to see if we're ready to reload
	if ( m_reloadState == 0 )
	{
		pPlayer->SetAnimation( PLAYER_RELOAD );

		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_reloadState = 1;
		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5f;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );

		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_START );

		return true;
	}
	else if ( m_reloadState == 1 )
	{
		if ( m_flTimeWeaponIdle > gpGlobals->curtime )
			return true;
		// was waiting for gun to move to side
		m_reloadState = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
		// [mlowrance] Only play the looping anim
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
		SendActivityEvents();
		
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -1, true );
		m_reloadState = 1;
	}

	return true;
}

void CWeaponNOVA::WeaponIdle( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_flPumpTime && m_flPumpTime < gpGlobals->curtime )
	{
		// play pumping sound
		m_flPumpTime = 0.0f;
	}

	if ( m_flTimeWeaponIdle < gpGlobals->curtime )
	{
		if ( m_iClip1 == 0 && m_reloadState == 0 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
		{
			Reload();
		}
		else if ( m_reloadState != 0 )
		{
			if ( m_iClip1 != GetMaxClip1() && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				//MIKETODO: shotgun anims
				SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );

				// [mlowrance] play the finish for 3rd person
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );
				// play cocking sound
				m_reloadState = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5f );
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

class CWeaponSawedoff : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponSawedoff" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponSawedoff, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSawedoff( void );

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_SAWEDOFF; }

	virtual int GetShotgunReloadState( void ) { return m_reloadState; }

private:

	CWeaponSawedoff( const CWeaponSawedoff & );

	float m_flPumpTime;
	CNetworkVar( int, m_reloadState, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSawedoff, DT_WeaponSawedoff )

IMPLEMENT_REFLECT_TABLE( CWeaponSawedoff, DT_WeaponSawedoff );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_sawedoff, WeaponSawedoff );
// PRECACHE_REGISTER( weapon_sawedoff );

CWeaponSawedoff::CWeaponSawedoff( void )
{
	m_flPumpTime = 0.0f;
	m_reloadState = 0;
}

void CWeaponSawedoff::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float flCycleTime = GetCycleTime();

	// don't fire underwater
	if ( pPlayer->GetWaterLevel() == WL_Eyes )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 == 0 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		}

		return;
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

    uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	// Dispatch the FX right away with full accuracy.
	float flCurAttack = CalculateNextAttackTime( flCycleTime );
	FX_FireBullets( 
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(), 
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed( true ) & 255, // wrap it for network traffic so it's the same between client and server
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		flCurAttack,
		SINGLE,
		m_flRecoilIndex );

	// are we firing the last round in the clip?
	if ( m_iClip1 == 1 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 0.875f );
	}
	else
	{
		m_flPumpTime = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 2.5f );
	}

	m_reloadState = 0;

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), Primary_Mode );

	// table driven recoil
	Recoil( Primary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponSawedoff::Reload( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 || m_iClip1 == GetMaxClip1() )
		return true;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return true;
		
	// check to see if we're ready to reload
	if ( m_reloadState == 0 )
	{
		pPlayer->SetAnimation( PLAYER_RELOAD );

		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_reloadState = 1;
		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5f;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );

		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_START );

		return true;
	}
	else if ( m_reloadState == 1 )
	{
		if ( m_flTimeWeaponIdle > gpGlobals->curtime )
			return true;
		// was waiting for gun to move to side
		m_reloadState = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
		// [mlowrance] Only play the looping anim
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
		SendActivityEvents();
		
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -1, true );
		m_reloadState = 1;
	}

	return true;
}

void CWeaponSawedoff::WeaponIdle( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_flPumpTime && m_flPumpTime < gpGlobals->curtime )
	{
		// play pumping sound
		m_flPumpTime = 0.0f;
	}

	if ( m_flTimeWeaponIdle < gpGlobals->curtime )
	{
		if ( m_iClip1 == 0 && m_reloadState == 0 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
		{
			Reload();
		}
		else if ( m_reloadState != 0 )
		{
			if ( m_iClip1 != GetMaxClip1() && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				//MIKETODO: shotgun anims
				SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );
				
				// [mlowrance] play the finish for 3rd person
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );
				// play cocking sound
				m_reloadState = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5f );
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( SensorGrenade, DT_SensorGrenade )

IMPLEMENT_REFLECT_TABLE( CSensorGrenade, DT_SensorGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_tagrenade, SensorGrenade );
PRECACHE_REGISTER( weapon_tagrenade );

void CSensorGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CSensorGrenade::ShotDetonate( CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile* pGrenade = CSensorGrenadeProjectile::Create( this->GetAbsOrigin(), this->GetAbsAngles(), Vector(0,0,0), AngularImpulse(0,0,0), pPlayer, weaponInfo );
	if ( pGrenade )
		pGrenade->DetonateOnNextThink();
}

class CCSPlayer;

using namespace ::ks::reflect;

class CWeaponSG552 : public CWeaponCSBaseGun
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponSG552" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponSG552, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSG552();

	virtual void PrimaryAttack();
	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_SG552; }

private:

	CWeaponSG552( const CWeaponSG552 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSG552, DT_WeaponSG552 )

IMPLEMENT_REFLECT_TABLE( CWeaponSG552, DT_WeaponSG552 );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_sg552, WeaponSG552 );
// PRECACHE_REGISTER( weapon_sg552 );

CWeaponSG552::CWeaponSG552()
{
}

void CWeaponSG552::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	bool bZoomed = pPlayer->GetFOV() < pPlayer->GetDefaultFOV();

	float flCycleTime = GetCycleTime();

	if ( bZoomed )
		flCycleTime = 0.135f;

	CSBaseGunFire( flCycleTime, m_weaponMode );
}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( SmokeGrenade, DT_SmokeGrenade )

IMPLEMENT_REFLECT_TABLE( CSmokeGrenade, DT_SmokeGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_smokegrenade, SmokeGrenade );
PRECACHE_REGISTER( weapon_smokegrenade );

	void CSmokeGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CSmokeGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
	}

class CBaseCombatWeapon;

using namespace ::ks::reflect;

inline constexpr const char *TASER_BIRTHDAY_PARTICLES = "weapon_confetti";
inline constexpr const char *TASER_BIRTHDAY_SOUND = "Weapon_PartyHorn.Single";

class CWeaponTaser : public CWeaponCSBaseGun
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponTaser" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponTaser, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponTaser();

	 void Precache_Impl();
	virtual void PrimaryAttack( void );
	virtual const char* GetMuzzleFlashEffectName_1stPerson( void );
	virtual const char* GetMuzzleFlashEffectName_3rdPerson( void );

	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void ItemPostFrame();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_TASER; }

private:
	CWeaponTaser( const CWeaponTaser& );
	float	m_fFireTime;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTaser, DT_WeaponTaser )

IMPLEMENT_REFLECT_TABLE( CWeaponTaser, DT_WeaponTaser );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_taser, WeaponTaser );
// PRECACHE_REGISTER( weapon_taser );

CWeaponTaser::CWeaponTaser() :
	m_fFireTime(0.0f)
{
}

void CWeaponTaser::Precache_Impl()
{
	BaseClass::Precache_Impl();

	PrecacheParticleSystem( TASER_BIRTHDAY_PARTICLES );
	PrecacheScriptSound( TASER_BIRTHDAY_SOUND );
}

void CWeaponTaser::PrimaryAttack( void )
{
	if ( !CSBaseGunFire( GetCycleTime(), Primary_Mode ) )
		return;

	m_fFireTime = gpGlobals->curtime;

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		//CPASAttenuationFilter filter( this, params.soundlevel );
		//EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND, &GetLocalOrigin(), 0.0f );

		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND );
	}
}

const char* CWeaponTaser::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		return TASER_BIRTHDAY_PARTICLES;
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponTaser::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		return TASER_BIRTHDAY_PARTICLES;
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
}

bool CWeaponTaser::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( HasAmmo() == false )
	{
		// just drop it if it's out of ammo and we're trying to switch away
		GetPlayerOwner()->CSWeaponDrop( this );
	}

	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponTaser::ItemPostFrame()
{
	const float kTaserDropDelay = 0.5f;
	BaseClass::ItemPostFrame();

	if ( HasAmmo() == false && gpGlobals->curtime >= m_fFireTime + kTaserDropDelay )
	{
		GetPlayerOwner()->Weapon_Drop( this, nullptr, nullptr );
		//pPlayer->RemoveWeaponOnPlayer( this );
		UTIL_Remove(this);
	}
}

/*
class CWeaponPartyPopper : public CWeaponTaser
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponPartyPopper" } ]]
	static constexpr bool ks_class = true;

	public:
	DECLARE_CLASS( CWeaponPartyPopper, CWeaponTaser );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	//CWeaponPartyPopper();

	//virtual void PrimaryAttack( void );

	//virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_TASER; }

	private:
	//CWeaponPartyPopper( const CWeaponPartyPopper& );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPartyPopper, DT_WeaponPartyPopper )

IMPLEMENT_REFLECT_TABLE( CWeaponPartyPopper, DT_WeaponPartyPopper );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CWeaponPartyPopper );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_partypopper, WeaponPartyPopper );

void CWeaponPartyPopper::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND );
		//EmitSound( filter, entindex(), "Weapon_MAC10.Single" );
	}
}
*/

class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

class CWeaponXM1014 : public CWeaponCSBase
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponXM1014" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponXM1014, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponXM1014( void );

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_XM1014; }

	virtual int GetShotgunReloadState( void ) { return m_reloadState; }

private:

	CWeaponXM1014( const CWeaponXM1014 & );

	float m_flPumpTime;
	CNetworkVar( int, m_reloadState, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// special reload state for shotgun

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponXM1014, DT_WeaponXM1014 )

IMPLEMENT_REFLECT_TABLE( CWeaponXM1014, DT_WeaponXM1014 );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_xm1014, WeaponXM1014 );
// PRECACHE_REGISTER( weapon_xm1014 );

CWeaponXM1014::CWeaponXM1014( void )
{
	m_flPumpTime = 0;
	m_reloadState = 0;
}

void CWeaponXM1014::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float flCycleTime = GetCycleTime();

	// don't fire underwater
	if ( pPlayer->GetWaterLevel() == WL_Eyes )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		Reload();
		
		if ( m_iClip1 == 0 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
		}

		return;
	}

	 SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	 
	//pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	//pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

    uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	// Dispatch the FX right away with full accuracy.
	float flCurAttack = CalculateNextAttackTime( flCycleTime );
	FX_FireBullets( 
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(), 
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed( true ) & 255, // wrap it for network traffic so it's the same between client and server
		GetInaccuracy(),
		GetSpread(), // flSpread
		GetAccuracyFishtail(),
		flCurAttack,
		SINGLE,
		m_flRecoilIndex );

	// are we firing the last round in the clip?
	if ( m_iClip1 == 1 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 0.25f );
	}
	else
	{
		m_flPumpTime = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 2.5f );
	}
		
	m_reloadState = 0;

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), Primary_Mode );

	// table driven recoil
	Recoil( Primary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponXM1014::Reload( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 || m_iClip1 == GetMaxClip1() )
		return true;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return true;
	
	//MIKETODO: shotgun reloading (wait until we get content)
	
	// check to see if we're ready to reload
	if ( m_reloadState == 0 )
	{
		pPlayer->SetAnimation( PLAYER_RELOAD );

		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_reloadState = 1;
		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_START );

		return true;
	}
	else if ( m_reloadState == 1 )
	{
		if ( m_flTimeWeaponIdle > gpGlobals->curtime )
			return true;
		// was waiting for gun to move to side
		m_reloadState = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
		// [mlowrance] Only play the looping anim
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
		SendActivityEvents();
		
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -1, true );
		m_reloadState = 1;
	}
	
	return true;
}

void CWeaponXM1014::WeaponIdle( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_flPumpTime && m_flPumpTime < gpGlobals->curtime )
	{
		// play pumping sound
		m_flPumpTime = 0;
	}

	if ( m_flTimeWeaponIdle < gpGlobals->curtime )
	{
		if ( m_iClip1 == 0 && m_reloadState == 0 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
		{
			Reload();
		}
		else if ( m_reloadState != 0 )
		{
			if ( m_iClip1 != GetMaxClip1() && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				//MIKETODO: shotgun anims
				SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );
				
				// [mlowrance] play the finish for 3rd person
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );
				// play cocking sound
				m_reloadState = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5f );
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}
