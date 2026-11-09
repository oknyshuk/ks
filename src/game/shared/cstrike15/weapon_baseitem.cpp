// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: base class for belt items, eg pills and adrenaline

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#endif
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_predmap.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "weapon_baseitem.h"
#include "cs_gamerules.h"

#if defined( CLIENT_DLL )
#include "c_cs_player.h"
#else
#include "cs_player.h"
#endif // CLIENT_DLL

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseItem, DT_WeaponBaseItem )

IMPLEMENT_REFLECT_TABLE( CWeaponBaseItem, DT_WeaponBaseItem );

#if defined CLIENT_DLL
#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CWeaponBaseItem );
#endif
#endif


#ifndef CLIENT_DLL
#endif

CWeaponBaseItem::CWeaponBaseItem()
{
	m_bRedraw = false;
}

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::Spawn_Impl( void )
{
	m_UseTimer.Invalidate();
	BaseClass::Spawn_Impl();
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Deploy( void )
{
	m_bRedraw = false;
	m_UseTimer.Invalidate();
	return BaseClass::Deploy();
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_UseTimer.Invalidate();
	return BaseClass::Holster( pSwitchingTo );
}

//--------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::CanExtendHelpingHand( void ) const
// {
// 	return !m_UseTimer.HasStarted() && BaseClass::CanExtendHelpingHand();
// }


//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == nullptr)
		return;

	if ( m_UseTimer.HasStarted() )
	{
		return;
	}

// 	if ( HelpingHandPrimaryAttack() )
// 	{
// 		return;
// 	}

	if ( !CanUseOnSelf( pPlayer ) )
		return;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	OnStartUse( pPlayer );

	m_UseTimer.Start( GetUseTimerDuration() );
}

float CWeaponBaseItem::GetUseTimerDuration( void )
{
	return SequenceDuration();
}


extern ConVar z_use_belt_item_tolerance;

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::SecondaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == nullptr)
		return;

	if ( m_UseTimer.HasStarted() )
		return;

// 	static const float GiveRange = 256.0f;
// 	CCSPlayer *target = ToCSPlayer( pPlayer->FindUseEntity( GiveRange, 0.0f, z_use_belt_item_tolerance.GetFloat(), NULL, true ) ); // Prefer to hit players
// 	if ( target && target->IsOnASurvivorTeam() && !target->IsIncapacitated() )
// 	{
// #ifdef GAME_DLL
// 		pPlayer->GiveActiveWeapon( target );
// #endif
// 		return;
// 	}

	BaseClass::SecondaryAttack();
}


//--------------------------------------------------------------------------------------------------------
/**
* Called when no buttons are pressed
*/
void CWeaponBaseItem::WeaponIdle( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponBaseItem::Reload()
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();

		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

		//Mark this as done
		//	m_bRedraw = false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------
/**
* Called each frame by the player PostThink
*/
void CWeaponBaseItem::ItemPostFrame( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	int buttons = pPlayer->m_nButtons;
#endif

	BaseClass::ItemPostFrame();

	if ( m_UseTimer.HasStarted() && m_UseTimer.IsElapsed() )
	{
		// pills can only help you so much
		CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
		if ( !pPlayer || !pPlayer->IsAlive() )
		{
			m_UseTimer.Invalidate();
			return;
		}
#ifndef CLIENT_DLL		
		CompleteUse( pPlayer );
		// BaseClass::ItemPostFrame() clears IN_ATTACK2, so we restore it here to prevent the next weapon from bashing immediately
		pPlayer->m_nButtons = buttons;

		m_bRedraw = true;
#endif
		m_UseTimer.Invalidate();

		// remove the ammo
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

		if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, nullptr, nullptr );
#ifndef CLIENT_DLL	
			UTIL_Remove( this );
#endif
		}
		else
		{
			pPlayer->SwitchToNextBestWeapon( this );
		}
	}

}

// //--------------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::OnHit( trace_t &trace, const Vector &swingVector, bool firstTime ) 
// {
// 	if ( trace.Ent<CBaseEntity>() && trace.Ent<CBaseEntity>()->IsPlayer() && IsASurvivorTeam( trace.Ent<CBaseEntity>()->GetTeamNumber() ) )
// 		return false;	// don't hit survivors who are outside of heal range if we're trying to get close and heal them.
// 
// 	return BaseClass::OnHit( trace, swingVector, firstTime );
// }

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::SendWeaponAnim( int iActivity )
{
	//iActivity = TranslateViewmodelActivity( (Activity)iActivity );
	return BaseClass::SendWeaponAnim( iActivity );
}

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::CanFidget( void )
{
	return false;
}


//--------------------------------------------------------------------------------------------------------
