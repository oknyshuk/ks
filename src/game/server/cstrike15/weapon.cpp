// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
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
#include "reflect_annotations.h"
#include "edict.h"
#include "reflect.h"
#include "shake.h"
#include "tier2/resourceprecacher.h"
#include "../baseentity.h"
#include "../reflect_datamap.h"
#include "irecipientfilter.h"
#include "usermessages_schema.h"
#include "../basecombatweapon_shared.h"
#include "../shareddefs.h"
#include "declare_class.h"
#include "../ai_activity.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.game.in_buttons;
import ks.game.iplayeranimstate;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :fish;
import :vscript;
import ks.game.ai_activity;
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
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.dbg;
import ks.core.random;
import ks.game.cstrike15.cs_achievement_constants;
import ks.game.cstrike15.cs_achievementdefs;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.tier1.vector_ops;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.tier1.fmtstr;
import ks.tier1.strtools;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;


IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenade, DT_BaseCSGrenade )

IMPLEMENT_REFLECT_TABLE( CBaseCSGrenade, DT_BaseCSGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_basecsgrenade, BaseCSGrenade );

ConVar sv_ignoregrenaderadio( side_registry(), "sv_ignoregrenaderadio", "0", FCVAR_RELEASE, "Turn off Fire in the hole messages" );

	constexpr auto GRENADE_SECONDARY_DAMPENING = 0.3f;
	constexpr auto GRENADE_SECONDARY_LOWER = 12.0f;
	constexpr auto GRENADE_SECONDARY_TRANSITION = 1.3f;
	constexpr auto GRENADE_SECONDARY_INTERP = 2.0f;

CBaseCSGrenade::CBaseCSGrenade()
{
	m_bRedraw = false;
	m_bIsHeldByPlayer = false;
	m_bPinPulled = false;
	m_fThrowTime = 0;
	m_bLoopingSoundPlaying = false;
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;

	m_bHasEmittedProjectile = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::Precache_Impl()
{
	BaseClass::Precache_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Deploy()
{
	m_bRedraw = false;
	m_bIsHeldByPlayer = true;
	m_bPinPulled = false;
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;
	m_fThrowTime = 0;
	// if we're officially out of grenades, ditch this weapon
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		pPlayer->Weapon_Drop( this, nullptr, nullptr );
		UTIL_Remove(this);
		return false;
	}

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	// we don't want to set m_bIsHeldByPlayer to true because the weapon actually holsters before it's removed from the inventory after the last grenade has been thrown
	// this causes a visual bug in the weapon selection UI
	m_bPinPulled = false; // when this is holstered make sure the pin isnt pulled.
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;
	m_fThrowTime = 0;
	// If they attempt to switch weapons before the throw animation is done, 
	// allow it, but kill the weapon if we have to.
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		CBaseCombatCharacter *pOwner = (CBaseCombatCharacter *)pPlayer;
		pOwner->Weapon_Drop( this );
		UTIL_Remove(this);
	}

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::PrimaryAttack()
{
	if ( !m_bIsHeldByPlayer || m_bPinPulled || m_fThrowTime > 0.0f )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return;

	// Ensure that the player can use this grenade
	if ( !pPlayer->CanUseGrenade( GetCSWeaponID() ) )
	{
		return;
	}

	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_GRENADE_PULL_PIN );

	// The pull pin animation has to finish, then we wait until they aren't holding the primary
	// attack button, then throw the grenade.
	SendWeaponAnim( ACT_VM_PULLPIN );
	m_bPinPulled = true;

	// Don't let weapon idle interfere in the middle of a throw!
	MDLCACHE_CRITICAL_SECTION();
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::SecondaryAttack()
{

	if ( !m_bPinPulled )
	{
		m_flThrowStrength = 0.0f;
		m_flThrowStrengthClientSmooth = 0.0f;
	}

	if ( CSGameRules()->IsFreezePeriod() )	// Don't let Brian molotov the team during freezetime
		return;

	PrimaryAttack();
	return;

	/*if ( m_bRedraw )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	
	if ( pPlayer == NULL )
		return;

	//See if we're ducking
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		//Send the weapon animation
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	}
	else
	{
		//Send the weapon animation
		SendWeaponAnim( ACT_VM_HAULBACK );
	}

	// Don't let weapon idle interfere in the middle of a throw!
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Reload()
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();

		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
		
		//Mark this as done
	//	m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPicker - 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

	if ( pNewOwner )
	{
		m_bIsHeldByPlayer = true;
	}
}

void CBaseCSGrenade::ItemPreFrame()
{
	BaseClass::ItemPreFrame();

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == nullptr )
		return;

}

float CBaseCSGrenade::ApproachThrownStrength()
{
	 m_flThrowStrengthClientSmooth = Approach( 
			m_flThrowStrength, 
			m_flThrowStrengthClientSmooth, 
			gpGlobals->frametime * GRENADE_SECONDARY_INTERP
			);
	 return m_flThrowStrengthClientSmooth;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
	if ( !vm )
		return;

	bool bPrimaryHeld = (pPlayer->m_nButtons & IN_ATTACK) != 0;
	bool bSecondaryHeld = (pPlayer->m_nButtons & IN_ATTACK2) != 0;
	
	if ( m_bPinPulled && ( bPrimaryHeld || bSecondaryHeld ) )
	{
		float flIdealThrowStrength = 0.5f;

		if ( bPrimaryHeld )
			flIdealThrowStrength += 0.5f;

		if ( bSecondaryHeld )
			flIdealThrowStrength -= 0.5f;

		m_flThrowStrength = Approach( flIdealThrowStrength, m_flThrowStrength, gpGlobals->frametime * GRENADE_SECONDARY_TRANSITION );
	}

	// If they let go of the fire buttons, they want to throw the grenade.
	if ( m_bPinPulled && !(bPrimaryHeld) && !(bSecondaryHeld) ) 
	{
		if ( IsThrownUnderhand() )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE_UNDERHAND );
		}
		else
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );
		}

		StartGrenadeThrow();
		
		MDLCACHE_CRITICAL_SECTION();
		m_bPinPulled = false;
		
		if ( IsThrownUnderhand() )
		{
			SendWeaponAnim( ACT_VM_RELEASE );
		}
		else
		{
			SendWeaponAnim( ACT_VM_THROW );	
		}
		
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration(); // we're still throwing, so reset our next primary attack

		IGameEvent * event = gameeventmanager->CreateEvent( "weapon_fire" );
		if( event )
		{
			const char *weaponName = STRING( m_iClassname );
			if ( IsWeaponClassname( weaponName ) )
			{
				weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			event->SetBool( "silenced", false );
			gameeventmanager->FireEvent( event );
		}

	}
	else if ((m_fThrowTime > 0) && (m_fThrowTime < gpGlobals->curtime))
	{
		// only decrement our ammo when we actually create the projectile
		DecrementAmmo( pPlayer );

		ThrowGrenade();
	}
	else if( !m_bIsHeldByPlayer )
	{
		// Has the throw animation finished playing
		if( m_flTimeWeaponIdle < gpGlobals->curtime )
		{
			// if we're officially out of grenades, ditch this weapon
			int nAmmoCount = pPlayer->GetAmmoCount(m_iPrimaryAmmoType);
			if( nAmmoCount <= 0 )
			{
				pPlayer->Weapon_Drop( this, nullptr, nullptr );
				//pPlayer->RemoveWeaponOnPlayer( this );
				UTIL_Remove(this);
			}

			else
			{
				pPlayer->SwitchToNextBestWeapon( this );
			}
			return;	//don't animate this grenade any more!
		}	
	}
	else if( !m_bRedraw )
	{
		BaseClass::ItemPostFrame();
	}
}

	int CBaseCSGrenade::CapabilitiesGet()
	{
		return bits_CAP_WEAPON_RANGE_ATTACK1; 
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pOwner - 
	//-----------------------------------------------------------------------------
	void CBaseCSGrenade::DecrementAmmo( CBaseCombatCharacter *pOwner )
	{
		pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}

	void CBaseCSGrenade::StartGrenadeThrow()
	{
		m_fThrowTime = gpGlobals->curtime + 0.1f;

		CBroadcastRecipientFilter filter;
		CSoundParameters params;
		if ( GetParametersForSound( GetShootSound( SINGLE ), params, nullptr ) )
		{
			//CPASAttenuationFilter filter( this );
			EmitSound( filter, entindex(), GetShootSound( SINGLE )); 
		}

		//WeaponSound(SINGLE, gpGlobals->curtime + 3.0f);
	}

	void CBaseCSGrenade::ThrowGrenade()
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( !pPlayer )
		{
			contract_assert( false );
			return;
		}

		QAngle angThrow = pPlayer->GetFinalAimAngle();

		if ( angThrow[PITCH] > 90.0f )
		{
			angThrow[PITCH] -= 360.0f;
		}
		else if ( angThrow[PITCH] < -90.0f )
		{
			angThrow[PITCH] += 360.0f;
		}

		contract_assert( ( angThrow[PITCH] <= 90.0f && angThrow[PITCH] >= -90.0f ) && "Grenade throw pitch angle must be between -90 and 90 for the adustments to work." );

		// NB. a pitch of +90 is looking straight down, -90 is looking straight up

		// add a 10 degrees upwards angle to the throw when looking horizontal, lerp the upwards boost to 0 at the pitch extremes
		angThrow[PITCH] -= 10.0f * (90.0f - std::fabsf(angThrow[PITCH])) / 90.0f;

		const float kBaseVelocity = GetThrowVelocity();
		//const float kThrowVelocityClampRatio = 750.0f / 540.0f;	// from original CSS values
			
		//float flVel = clamp((90 - angThrow.x) / 90, 0.0f, kThrowVelocityClampRatio) * kBaseVelocity;
		float flVel = clamp( (kBaseVelocity * 0.9f), 15, 750 );

		//clamp the throw strength ranges just to be sure
		float flClampedThrowStrength = m_flThrowStrength;
		flClampedThrowStrength = clamp( flClampedThrowStrength, 0.0f, 1.0f );

		flVel *= Lerp( flClampedThrowStrength, GRENADE_SECONDARY_DAMPENING, 1.0f );
		Vector vForward;
		AngleVectors( angThrow, &vForward );

		Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();

		vecSrc += Vector(0, 0, Lerp( flClampedThrowStrength, -GRENADE_SECONDARY_LOWER, 0.0f ) );
		// We want to throw the grenade from 16 units out.  But that can cause problems if we're facing
		// a thin wall.  Do a hull trace to be safe.
		// Wills: Moved the trace length out to 22 inches, then subtract 6. This way we default to 16, 
		// but pull back 6 from wherever we hit, so we don't emit from EXACTLY inside the close surface, which can lead to 
		// the grenade penetrating the wall anyway.
		trace_t trace;
		Vector mins( -2, -2, -2 );
		Vector maxs(  2,  2,  2 );
		UTIL_TraceHull( vecSrc, vecSrc + vForward * 22, mins, maxs, MASK_SOLID | CONTENTS_GRENADECLIP, pPlayer, COLLISION_GROUP_NONE, &trace );
		vecSrc = trace.endpos - (vForward * 6);

		Vector vecThrow = vForward * flVel + (pPlayer->GetAbsVelocity() * 1.25);

		EmitGrenade( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random_valve->RandomInt(-1200,1200),0), pPlayer, GetCSWpnData() );

		m_bHasEmittedProjectile = true; // Flag the grenade weapon as having emitted a projectile. The 'grenade' is now flying away from the player, so we don't want to drop *this* grenade on death (that'll make a duplicate)

		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

		if ( pCSPlayer )
		{
			int iWeaponId = GetCSWeaponID();

			pCSPlayer->PlayerUsedGrenade( iWeaponId );

			if ( !sv_ignoregrenaderadio.GetBool() )
			{
				if ( iWeaponId == WEAPON_FLASHBANG )
					pCSPlayer->Radio( "Radio.Flashbang",   "#SFUI_TitlesTXT_Flashbang_in_the_hole", true );
				else if ( iWeaponId == WEAPON_SMOKEGRENADE )
					pCSPlayer->Radio( "Radio.Smoke",   "#SFUI_TitlesTXT_Smoke_in_the_hole", true );
				else if ( iWeaponId == WEAPON_MOLOTOV )
					pCSPlayer->Radio( "Radio.Molotov",   "#SFUI_TitlesTXT_Molotov_in_the_hole", true );
				else if ( iWeaponId == WEAPON_INCGRENADE )
					pCSPlayer->Radio( "Radio.Incendiary",   "#SFUI_TitlesTXT_Incendiary_in_the_hole", true );
				else if ( iWeaponId == WEAPON_DECOY )
					pCSPlayer->Radio( "Radio.Decoy",   "#SFUI_TitlesTXT_Decoy_in_the_hole", true );
				else
					pCSPlayer->Radio( "Radio.FireInTheHole",   "#SFUI_TitlesTXT_Fire_in_the_hole", true );
			}
			CCS_GameStats.IncrementStat( pCSPlayer, CSSTAT_GRENADES_THROWN, 1 );
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "grenade_thrown" );
		if ( event )
		{
			const char *weaponName = STRING( m_iClassname );
			if ( IsWeaponClassname( weaponName ) )
			{
				weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			gameeventmanager->FireEvent( event );
		}
	}

	void CBaseCSGrenade::DropGrenade()
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( !pPlayer )
		{
			contract_assert( false );
			return;
		}

		Vector vForward;
		pPlayer->EyeVectors( &vForward );
		Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset() + vForward * 16; 

		Vector vecVel = pPlayer->GetAbsVelocity();

		EmitGrenade( vecSrc, vec3_angle, vecVel, AngularImpulse(600,random_valve->RandomInt(-1200,1200),0), pPlayer, GetCSWpnData() );

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if( pCSPlayer )
		{
			CCS_GameStats.IncrementStat( pCSPlayer, CSSTAT_GRENADES_THROWN, 1 );
		}

		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;
	}

	void CBaseCSGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		contract_assert( 0 && "CBaseCSGrenade::EmitGrenade should not be called. Make sure to implement this in your subclass!\n" );
	}

	bool CBaseCSGrenade::AllowsAutoSwitchFrom( void ) const
	{
		return !m_bPinPulled;
	}

bool CBaseCSGrenade::IsPinPulled() const
{
	return m_bPinPulled;
}

class CBaseCombatWeapon;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: base class for belt items, eg pills and adrenaline

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseItem, DT_WeaponBaseItem )

IMPLEMENT_REFLECT_TABLE( CWeaponBaseItem, DT_WeaponBaseItem );

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

	int buttons = pPlayer->m_nButtons;

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
		CompleteUse( pPlayer );
		// BaseClass::ItemPostFrame() clears IN_ATTACK2, so we restore it here to prevent the next weapon from bashing immediately
		pPlayer->m_nButtons = buttons;

		m_bRedraw = true;
		m_UseTimer.Invalidate();

		// remove the ammo
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

		if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, nullptr, nullptr );
			UTIL_Remove( this );
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

bool CWeaponBaseItem::HasPrimaryAmmo( void )
{
	return true;
}

bool CWeaponBaseItem::CanBeSelected( void )
{
	return true;
}

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CConfigurationForHighPriorityUseEntity_t;
class CTakeDamageInfo;
class CUserCmd;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto BLINK_INTERVAL = 2.5;
inline constexpr const char *PLANTED_C4_MODEL = "models/weapons/w_ied_dropped.mdl";
static constexpr auto HEIST_MODE_C4_TIME = 25;

static constexpr auto WEAPON_C4_ARM_TIME = 3.0;

static constexpr auto WEAPON_C4_UPDATE_LAST_VALID_PLAYER_HELD_POSITION_INTERVAL = 0.2;

// amount of time a player can stop defusing and continue
static const float C4_DEFUSE_GRACE_PERIOD = 0.5f;

// amount of time a player is forced to continue defusing after not USEing. this effects other player's ability to interrupt
static const float C4_DEFUSE_LOCKIN_PERIOD = 0.05f;	

extern ConVar mp_anyone_can_pickup_c4;
extern ConVar mp_c4_cannot_be_defused;

static void clear_bombs( const CCommand &args );
static ConCommand clear_bombs_command( side_registry(), "clear_bombs", clear_bombs, "", FCVAR_CHEAT );
static void clear_bombs( const CCommand &args )
{
	for ( CPlantedC4 *pC4 : g_PlantedC4s )
	{
		if ( pC4 )
		{
			UTIL_Remove( pC4 );
		}
	}
	g_PlantedC4s.clear();
}

LINK_ENTITY_TO_CLASS( planted_c4, CPlantedC4 );
PRECACHE_REGISTER( planted_c4 );

IMPLEMENT_REFLECT_DATAMAP( CPlantedC4 )
	
IMPLEMENT_REFLECT_SERVERCLASS( CPlantedC4, DT_PlantedC4 )

	std::vector< CPlantedC4* > g_PlantedC4s;

	CPlantedC4::CPlantedC4()
	{
		g_PlantedC4s.push_back( this );

		// [tj] No planter initially
		m_pPlanter = nullptr;
		m_pBombDefuser = nullptr; //No Defuser Initially

		// [tj] Assume this is the original owner
		m_bPlantedAfterPickup = false;

		m_bTrainingPlacedByPlayer = false;
		m_bVoiceAlertFired = false;

		SetSpotRules( CCSEntitySpotting::SPOT_RULE_CT | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
	}

	CPlantedC4::~CPlantedC4()
	{
		ks::erase_first( g_PlantedC4s, this );

		RemoveControlPanels();
	}

	void CPlantedC4::Spawn_Impl()
	{
		BaseClass::Spawn_Impl();

		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_NONE );
		AddFlag( FL_OBJECT );

		SetModel( PLANTED_C4_MODEL );
		SetSequence(1);	// this sequence keeps the toggle switch in the 'up' position

		SetCollisionBounds( Vector( 0, 0, 0 ), Vector( 8, 8, 8 ) );

		// Detonate in "time" seconds
		SetThink( &CPlantedC4::C4Think );

		SetNextThink( gpGlobals->curtime + 0.1f );

		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && mp_anyone_can_pickup_c4.GetBool() )
			m_flTimerLength = 9999;
		else
			m_flTimerLength = mp_c4timer.GetInt();

		m_flC4Blow = gpGlobals->curtime + m_flTimerLength;
		m_fLastDefuseTime = 0.0f;
		m_bBeingDefused = false;
		m_bHasExploded = false;
		m_bBombDefused = false;

		SetFriction( 0.9 );

		m_flDefuseLength = 0.0f;

		SpawnControlPanels();

		VisibilityMonitor_AddEntity( this, 600.0f, nullptr, nullptr );
	}

	int CPlantedC4::UpdateTransmitState_Impl()
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CPlantedC4::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
	{
		// Terrorists always need this object for the radar
		// Everybody needs it for hiding the round timer and showing the planted C4 scenario icon
		return FL_EDICT_ALWAYS;
	}

	void CPlantedC4::Precache_Impl()
	{
		PrecacheModel( "models/weapons/w_c4_planted.mdl" ); // old, unused
		PrecacheModel( PLANTED_C4_MODEL );
		PrecacheModel( "models/props/de_overpass/balloon.mdl" );
		PrecacheParticleSystem( "weapon_confetti_balloons" );
		PrecacheModel( "models/weapons/w_eq_multimeter.mdl" );
	}

	void CPlantedC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_panel";
	}

	void CPlantedC4::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "vgui_screen";
	}

	//-----------------------------------------------------------------------------
	// This is called by the base object when it's time to spawn the control panels
	//-----------------------------------------------------------------------------
	void CPlantedC4::SpawnControlPanels()
	{
	}

	void CPlantedC4::RemoveControlPanels()
	{
	}

	void CPlantedC4::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
	{
		// Are we already marked for transmission?
		if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
			return;

		BaseClass::SetTransmit_Impl( pInfo, bAlways );
	}

	CPlantedC4* CPlantedC4::ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles )
	{
		CPlantedC4 *pGrenade;
		bool bTrainingPlacedByPlayer = false;
		if ( CSGameRules()->IsPlayingTraining() )
		{
			pGrenade = dynamic_cast< CPlantedC4Training* >( CreateEntityByName( "planted_c4_training" ) );
			bTrainingPlacedByPlayer = true;
		}
		else
		{
			pGrenade = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4" ) );
		}

		if ( pGrenade )
		{
			vecAngles[0] = 0;
			vecAngles[2] = 0;
			pGrenade->Init( pevOwner, vecStart, vecAngles, bTrainingPlacedByPlayer );
			return pGrenade;
		}
		else
		{
			Warning( "Can't create planted_c4 entity!\n" );
			return nullptr;
		}
	}

	void CPlantedC4::Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles, bool bTrainingPlacedByPlayer )
	{
		SetAbsOrigin( vecStart );
		SetAbsAngles( vecAngles );
		SetOwnerEntity( pevOwner );
		SetPlanter( pevOwner );

		m_bTrainingPlacedByPlayer = bTrainingPlacedByPlayer;
		if ( !m_bTrainingPlacedByPlayer )
			m_bBombTicking = true;

		Spawn();
	}

	void CPlantedC4::C4Think()
	{
		if (!IsInWorld())
		{
			UTIL_Remove( this );
			return;
		}

		// network the defuser handle
		if ( m_bBeingDefused )
		{
			m_hBombDefuser = m_pBombDefuser;
			SetBodygroupPreset( "show_clips" );
		}
		else
		{
			m_hBombDefuser = INVALID_EHANDLE;
			SetBodygroupPreset( "hide_clips" );
		}

		if ( m_bHasExploded )
		{
			if ( CSGameRules()->IsPlayingTraining() )
			{
				SetThink( &CBaseEntity::SUB_Remove );
				SetNextThink( gpGlobals->curtime + 1.0f );

				if ( m_pBombDefuser )
				{
					m_pBombDefuser->m_bIsDefusing = false;
					m_pBombDefuser->SetProgressBarTime( 0 );
					m_pBombDefuser->OnCanceledDefuse();
					m_pBombDefuser = nullptr;
				}

				m_bBeingDefused = false;
				m_flDefuseCountDown = 0;
				m_flDefuseLength = 0;
			}

			return;
		}

		//Bomb is not ticking, don't think anymore
		if( !IsBombActive() )
		{
			SetThink( nullptr );
			return;
		}
		// [hpe:jason] Decrease the latency between c4 think updates
		SetNextThink( gpGlobals->curtime + 0.05f );

		// let the bots hear the bomb beeping
		// BOTPORT: Emit beep events at same time as client effects
		IGameEvent * event = gameeventmanager->CreateEvent( "bomb_beep" );
		if( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}
		
		// 4 seconds before the bomb blows up, have anyone close by on each team say something about it going to blow
		if (m_flC4Blow - 3.0 <= gpGlobals->curtime && !m_bVoiceAlertFired)
		{
			CCSPlayer *pCT = nullptr;
			CCSPlayer *pT = nullptr;
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer && pPlayer->IsAlive() )
				{
					if ( !pCT && pPlayer->GetTeamNumber() == TEAM_CT && pCT != m_pBombDefuser )
					{
						if ((GetAbsOrigin() - pPlayer->GetAbsOrigin()).AsVector2D().IsLengthLessThan( 1200.0 ))
							pCT = pPlayer;
					}
					else if ( !pT && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
					{
						if ((GetAbsOrigin() - pPlayer->GetAbsOrigin()).AsVector2D().IsLengthLessThan( 1200.0 ))
							pT = pPlayer;
					}
				}

				// we have a player from both teams, don't need to find anymore
				if ( pCT && pT )
					break;
			}

			if ( pCT && (!m_bBeingDefused || (m_bBeingDefused && (m_flDefuseCountDown > m_flC4Blow) ) ) )
			{
				pCT->Radio( "Radio.GetOutOfThere", "", true );
				m_bVoiceAlertFired = true;
			}

			if ( pT )
			{
				pT->Radio( "Radio.GetOutOfThere", "", true );
				m_bVoiceAlertFired = true;
			}
		}

		// IF the timer has expired ! blow this bomb up!
		if (m_flC4Blow <= gpGlobals->curtime)
		{
			// kick off the person trying to defuse the bomb
			if ( m_pBombDefuser )
			{
				m_pBombDefuser->m_bIsDefusing = false;
				m_pBombDefuser->SetProgressBarTime( 0 );
				m_pBombDefuser->OnCanceledDefuse();
				m_pBombDefuser = nullptr;
				m_bBeingDefused = false;
			}

			// for the music, we added a tension filled second after the bomb has ceased to be defusable and explode 1 second after
			if (m_flC4Blow + 1.0f <= gpGlobals->curtime)
			{
				// give the bomber credit for planting the bomb
				CCSPlayer* pBombOwner = ToCSPlayer(GetOwnerEntity());

//				NOTE[pmf]: removed by design decision
// 				if ( pBombOwner )
// 				{
// 					if (CSGameRules()->m_iRoundWinStatus == WINNER_NONE)
// 						pBombOwner->IncrementFragCount( 3 );
// 				}

				CSGameRules()->m_bBombDropped = false;

				trace_t tr;
				Vector vecSpot = GetAbsOrigin();
				vecSpot[2] += 8;

				UTIL_TraceLine( vecSpot, vecSpot + Vector ( 0, 0, -40 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

				Explode( &tr, DMG_BLAST );

				CSGameRules()->m_bBombPlanted = false;

				CCS_GameStats.Event_BombExploded(pBombOwner);

				IGameEvent * event = gameeventmanager->CreateEvent( "bomb_exploded" );
				if( event )
				{
					event->SetInt( "userid", pBombOwner?pBombOwner->GetUserID():-1 );
					event->SetInt( "site", m_iBombSiteIndex );
					event->SetInt( "priority", 20 ); // bomb_exploded
					gameeventmanager->FireEvent( event );
				}

				RemoveControlPanels();

				// skip additional processing once the bomb has exploded
				return;
			}

		}

		// make sure our defuser exists
		if ( m_bBeingDefused && (m_pBombDefuser == nullptr) )
		{
			m_bBeingDefused = false;
		}

		//if the defusing process has started
		if ( m_bBeingDefused && (m_pBombDefuser != nullptr) && mp_c4_cannot_be_defused.GetBool() == false )
		{
			//if the defusing process has not ended yet
			if ( gpGlobals->curtime < m_flDefuseCountDown )
			{
				int iOnGround = FBitSet( m_pBombDefuser->GetFlags(), FL_ONGROUND );

				const CUserCmd *pCmd = m_pBombDefuser->GetLastUserCommand();
				bool bPlayerStoppedHoldingUse = !(pCmd->buttons & IN_USE) && (gpGlobals->curtime > m_fLastDefuseTime + C4_DEFUSE_LOCKIN_PERIOD);

				CConfigurationForHighPriorityUseEntity_t cfgUseEntity;
				bool bPlayerUseIsValidNow = m_pBombDefuser->GetUseConfigurationForHighPriorityUseEntity( this, cfgUseEntity ) &&
					( cfgUseEntity.m_pEntity == this ) && cfgUseEntity.UseByPlayerNow( m_pBombDefuser, cfgUseEntity.k_EPlayerUseType_Progress );

				//if the bomb defuser has stopped defusing the bomb
				if ( bPlayerStoppedHoldingUse || !bPlayerUseIsValidNow || !iOnGround )
				{
					if ( !iOnGround && m_pBombDefuser->IsAlive() )
						ClientPrint( m_pBombDefuser, HUD_PRINTCENTER, "#SFUI_Notice_C4_Defuse_Must_Be_On_Ground");

					// tell the bots someone has aborted defusing
					IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
					if( event )
					{
						event->SetInt("userid", m_pBombDefuser->GetUserID() );
						event->SetInt( "priority", 5 ); // bomb_abortdefuse
						gameeventmanager->FireEvent( event );
					}

					//cancel the progress bar
					m_pBombDefuser->SetProgressBarTime( 0 );
					m_pBombDefuser->OnCanceledDefuse();

					// release the player from being frozen
					m_pBombDefuser->m_bIsDefusing = false;
					m_bBeingDefused = false;
				}

				return;
			}
			// training has to pick elements
			else if ( m_pBombDefuser->IsAlive() && CSGameRules()->IsPlayingTraining() )
			{
				Vector soundPosition = m_pBombDefuser->GetAbsOrigin() + Vector( 0, 0, 5 );
				CPASAttenuationFilter filter( soundPosition );

				IGameEvent * event = gameeventmanager->CreateEvent( "bomb_defused" );
				if( event )
				{
					event->SetInt("userid", m_pBombDefuser->GetUserID() );
					event->SetInt("site", m_iBombSiteIndex );
					event->SetInt( "priority", 5 ); // bomb_defused
					gameeventmanager->FireEvent( event );
				}

				EmitSound( filter, 0, "c4.disarmfinish", &GetAbsOrigin() );
				
				// The bomb has just been disarmed.. Check to see if the round should end now
				m_bBombTicking = false;

				// release the player from being frozen
				m_pBombDefuser->m_bIsDefusing = false;

				// Clear their progress bar.
				m_pBombDefuser->SetProgressBarTime( 0 );

				m_pBombDefuser = nullptr;
				m_bBeingDefused = false;

				m_flDefuseLength = 10;

				m_OnBombDefused.FireOutput(this, m_pBombDefuser);
				return;
			}
			//if the defuse process has ended, kill the c4 (for safety we also check whether the Terrorists have already won the round in which case we cannot score the defuse!)
			else if ( m_pBombDefuser->IsAlive() && ( CSGameRules()->m_iRoundWinStatus != WINNER_TER ) )
			{
				bool roundWasAlreadyWon = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );	// This condition checks whether CTs already won the round
				// Check if there are Terrorists alive
				bool bTerroristsAlive = false;
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
					if ( !pCheckPlayer )
						continue;
					if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
						continue;
					if ( pCheckPlayer->IsAlive() )
					{
						bTerroristsAlive = true;
						break;
					}
				}

				// set down-to-the-wire defuse fun fact
				m_pBombDefuser->SetDefusedBombWithThisTimeRemaining( m_flC4Blow - gpGlobals->curtime );

				CCS_GameStats.Event_BombDefused(m_pBombDefuser);
				CSGameRules()->ScoreBombDefuse( m_pBombDefuser, ( bTerroristsAlive && !roundWasAlreadyWon ) );
				m_pBombDefuser->AddAccountAward( PlayerCashAward::BOMB_DEFUSED );

				// record in matchstats
				if ( CSGameRules()->ShouldRecordMatchStats() )
				{
					int iCurrentRound = CSGameRules()->GetTotalRoundsPlayed();
					++ m_pBombDefuser->m_iMatchStats_Objective.GetForModify( iCurrentRound );

					// Keep track of Match stats in QMM data
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_pBombDefuser->GetHumanPlayerAccountID() ) )
					{
						pQMM->m_iMatchStats_Objective[ iCurrentRound ] = m_pBombDefuser->m_iMatchStats_Objective.Get( iCurrentRound );
					}
				}

				if ( !roundWasAlreadyWon )
				{
					// All alive CTs also get assistance credit for bomb defuse.
					// This way if all Terrorists are eliminated then all alive CTs get 1pt and defuser gets 2pt;
					// if a Terrorist remains alive then the defuser gets 5 pts and all other alive
					// teammates get 1 pt for assist / suppressing fire.
					for ( int i = 1; i <= MAX_PLAYERS; i++ )
					{
						CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
						if ( !pCheckPlayer )
							continue;
						if ( pCheckPlayer->GetTeamNumber() != TEAM_CT )
							continue;
						if ( pCheckPlayer->IsAlive() )
							CSGameRules()->ScoreBombDefuse( pCheckPlayer, false );
					}
				}

				IGameEvent * event = gameeventmanager->CreateEvent( "bomb_defused" );
				if( event )
				{
					event->SetInt("userid", m_pBombDefuser->GetUserID() );
					event->SetInt("site", m_iBombSiteIndex );
					event->SetInt( "priority", 5 ); // bomb_defused
					gameeventmanager->FireEvent( event );

					m_pBombDefuser->AwardAchievement( CSWinBombDefuse );

					float timeToDetonation = ( m_flC4Blow - gpGlobals->curtime );

					if ( (timeToDetonation > 0.0f) && (timeToDetonation <= AchievementConsts::BombDefuseCloseCall_MaxTimeRemaining) )
					{
						// Give achievement for defusing with < 1 second before detonation
						m_pBombDefuser->AwardAchievement( CSBombDefuseCloseCall );
					}

					// [dwenger] Added for fun-fact support
					if ( m_pBombDefuser->PickedUpDefuser() )
					{
						// Defuser kit was picked up, so set the fun fact
						m_pBombDefuser->SetDefusedWithPickedUpKit( true );
					}
				}

				Vector soundPosition = m_pBombDefuser->GetAbsOrigin() + Vector( 0, 0, 5 );
				CPASAttenuationFilter filter( soundPosition );

				EmitSound( filter, 0, "c4.disarmfinish", &GetAbsOrigin() );

				// The bomb has just been disarmed.. Check to see if the round should end now
				m_bBombTicking = false;

				// release the player from being frozen
				m_pBombDefuser->m_bIsDefusing = false;

				CSGameRules()->m_bBombDefused = true;
				
				if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
				{
					DispatchParticleEffect( "weapon_confetti_balloons", GetAbsOrigin(), QAngle( 0, 0, 0 ) );
					CPASAttenuationFilter filter( this );
					filter.UsePredictionRules();
					EmitSound( filter, entindex(), "Weapon_PartyHorn.Single" );
					//EmitSound( filter, entindex(), "Birthday_PartyHorn.VO" );
					//C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, "Birthday_PartyHorn.VO");
				}

				// Setup MVP granting class in case round wasn't already won
				class CPlantedC4DefusedMVP : public CCSGameRules::ICalculateEndOfRoundMVPHook_t
				{
				public:
					virtual CCSPlayer* CalculateEndOfRoundMVP() override
					{
						if( m_pBombDefuser->HasControlledBotThisRound() )
						{ 
							// [dkorus] if we controlled a bot this round, use standard MVP conditions
							return CSGameRules()->CalculateEndOfRoundMVP();
						}

						bool bTerroristsAlive = false;
						for ( int i = 1; i <= MAX_PLAYERS; i++ )
						{
							CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
							if ( !pCheckPlayer )
								continue;
							if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
								continue;
							if ( pCheckPlayer->IsAlive() )
							{
								bTerroristsAlive = true;
								break;
							}
						}

						if ( bTerroristsAlive || ( m_pBombDefuser->GetNumRoundKills() && !m_pBombDefuser->m_iNumRoundTKs ) )
						{
							m_pBombDefuser->IncrementNumMVPs( CSMVP_BOMBDEFUSE );
							return m_pBombDefuser;
						}

						if ( CCSPlayer *pDefaultMvp = CSGameRules()->CalculateEndOfRoundMVP() )
							return pDefaultMvp;

						m_pBombDefuser->IncrementNumMVPs( CSMVP_BOMBDEFUSE );
						return m_pBombDefuser;
					}
					CHandle<CCSPlayer> m_pBombDefuser;
				} mvpHook;
				mvpHook.m_pBombDefuser = m_pBombDefuser;
				if ( !roundWasAlreadyWon )
					CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = &mvpHook;

				// [menglish] Give the bomb defuser an mvp if they ended the round
				CSGameRules()->CheckWinConditions();

				// Reset the MVP hook
				if ( !roundWasAlreadyWon )
					CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = nullptr;

//				NOTE[pmf]: removed by design decision
// 				// give the defuser credit for defusing the bomb
// 				m_pBombDefuser->IncrementFragCount( 3 );

				CSGameRules()->m_bBombDropped = false;
				CSGameRules()->m_bBombPlanted = false;
				
				// Clear their progress bar.
				m_pBombDefuser->SetProgressBarTime( 0 );

				m_pBombDefuser = nullptr;
				m_bBeingDefused = false;
				m_bBombDefused = true;

				m_flDefuseLength = 10;

				m_OnBombDefused.FireOutput(this, m_pBombDefuser);

				return;
			}

			//if it gets here then the previouse defuser has taken off or been killed
			m_OnBombDefuseAborted.FireOutput(this, m_pBombDefuser);

			// tell the bots someone has aborted defusing
			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
			if ( event )
			{
				event->SetInt("userid", m_pBombDefuser->GetUserID() );
				event->SetInt( "priority", 5 ); // bomb_abortdefuse
				gameeventmanager->FireEvent( event );
			}

			// release the player from being frozen
			m_pBombDefuser->m_bIsDefusing = false;
			m_bBeingDefused = false;
			m_pBombDefuser = nullptr;
		}
	}

	// Regular explosions
	void CPlantedC4::Explode( trace_t *pTrace, int bitsDamageType )
	{
		// Check to see if the round is over after the bomb went off...
		CSGameRules()->m_bTargetBombed = true;
		m_bBombTicking = false;
		m_bHasExploded = true;
		m_bBombDefused = false;

		bool roundWasAlreadyWon = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );

		// MVP hook to award the MVP to person who planted the bomb
		class CPlantedC4ExplodedMVP : public CCSGameRules::ICalculateEndOfRoundMVPHook_t
		{
		public:
			virtual CCSPlayer* CalculateEndOfRoundMVP() override
			{
				// All alive Terrorists also get credit for bomb exploding,
				// this intentionally may include the original planter.
				// This way if bomb planter survives until explosion he will
				// get 2 pts and all other alive teammates will get 1 pt
				// If bomb planter doesn't survive then he gets 1 pt and all
				// teammates who remained alive defending the bomb get 1 pt
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
					if ( !pCheckPlayer )
						continue;
					if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
						continue;
					if ( pCheckPlayer->IsAlive() )
						CSGameRules()->ScoreBombExploded( pCheckPlayer );
				}

				if ( !pBombOwner )
					return CSGameRules()->CalculateEndOfRoundMVP();

				// Person who planted the bomb gets credit for the explosion
				CSGameRules()->ScoreBombExploded( pBombOwner );

				if( pBombOwner->HasControlledBotThisRound() )
				{ 
					// [dkorus] if we controlled a bot this round, use standard MVP conditions
					return CSGameRules()->CalculateEndOfRoundMVP();
				}
				else 
				{
					pBombOwner->IncrementNumMVPs( CSMVP_BOMBPLANT );
					return pBombOwner;
				}
			}
			CCSPlayer *pBombOwner;
		} mvpHook;
		mvpHook.pBombOwner = ToCSPlayer( GetOwnerEntity() );

		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = &mvpHook;

		bool bWin = CSGameRules()->CheckWinConditions();
		if ( bWin && mvpHook.pBombOwner )
		{
			mvpHook.pBombOwner->AwardAchievement( CSWinBombPlant );

			//[tj]more specific achievement for planting the bomb after recovering it.
			if ( m_bPlantedAfterPickup )
			{
				mvpHook.pBombOwner->AwardAchievement( CSWinBombPlantAfterRecovery );
			}
		}

		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = nullptr;

		// Do the Damage
		float flBombRadius;
		
		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			flBombRadius = 300;
		}
		else
		{
			flBombRadius = 500;
		}

		if ( g_pMapInfo )
			flBombRadius = g_pMapInfo->m_flBombRadius;

		// Output to the bomb target ent
		CBaseEntity *pTarget = nullptr;
		variant_t emptyVariant;
		while ((pTarget = gEntList.FindEntityByClassname( pTarget, "func_bomb_target" )) != nullptr)
		{
			//Adrian - But only to the one we want!
			if ( pTarget->entindex() != m_iBombSiteIndex )
				 continue;
			
			pTarget->AcceptInput( "BombExplode", this, this, emptyVariant, 0 );
				break;
		}	

		// Pull out of the wall a bit
		if ( pTrace->fraction != 1.0 )
		{
			SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
		}

		{
			Vector pos = GetAbsOrigin() + Vector( 0,0,8 );

			// Make sure that the bomb exploding is a reliable effect that all clients on this server and GOTV receive
			CReliableBroadcastRecipientFilter filterBombExplodeReliable;

			// Try using the new particle system instead of temp ents
			QAngle	vecAngles;
			DispatchParticleEffect( "explosion_c4_500", pos, vecAngles, ( CBaseEntity * ) nullptr, int( -1 ), &filterBombExplodeReliable );
		}

		// Sound! for everyone
		CBroadcastRecipientFilter filter;
		EmitSound( filter, 0, "c4.explode", &GetAbsOrigin() );

		// Decal!
		UTIL_DecalTrace( pTrace, "Scorch" );

		// Shake!
		UTIL_ScreenShake( pTrace->endpos, 25.0, 150.0, 1.0, 3000, SHAKE_START );

		SetOwnerEntity( nullptr ); // can't traceline attack owner if this is set

		CSGameRules()->RadiusDamage( 
			CTakeDamageInfo( this, GetOwnerEntity(), flBombRadius, bitsDamageType ),
			GetAbsOrigin(),
			flBombRadius * 3.5,	//Matt - don't ask me, this is how CS does it.
			CLASS_NONE,
			true );	// IGNORE THE WORLD!!

		// send director message, that something important happed here
		/*
		MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
			WRITE_BYTE ( 9 );	// command length in bytes
			WRITE_BYTE ( DRC_CMD_EVENT );	// bomb explode
			WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
			WRITE_SHORT( 0 );	// index number of secondary entity
			WRITE_LONG( 15 | DRC_FLAG_FINAL );   // eventflags (priority and flags)
		MESSAGE_END();
		*/
	}

	// For CTs to defuse the c4
	void CPlantedC4::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		//Can't defuse if its already defused or if it has blown up (or training)
		if( !IsBombActive() || m_flC4Blow < gpGlobals->curtime || mp_c4_cannot_be_defused.GetBool() == true )
		{
			SetUse( nullptr );
			return;
		}

		CCSPlayer *player = dynamic_cast< CCSPlayer* >( pActivator );

		// Can't defuse a bomb if we're not CT or we're in a no defuse area.
		if ( !player || player->GetTeamNumber() != TEAM_CT || player->m_bInNoDefuseArea)
			return;

		// make sure human players can see the bomb (or the bomb's nearby line-of-sight points)
		// to ensure we aren't defusing from the room below or something
		if ( !player->IsBot() )
		{
			trace_t result;
			bool bFoundBomb = false;

			Vector vecLineOfSightPoints[] = {
				Vector(  0,  0,  1 ),
				Vector(  5,  5,  5 ),
				Vector( -5,  5,  5 ),
				Vector(  5, -5,  5 ),
				Vector( -5, -5,  5 ),
				Vector(  0,  0,  5 )
			};
			int nNumLOSpts = std::size( vecLineOfSightPoints );

			for ( int i=0; i<nNumLOSpts; i++ )
			{
				Vector vecLineOfSightTarget = VectorTransform( vecLineOfSightPoints[i], EntityToWorldTransform() );
				//debugoverlay->AddBoxOverlay( vecLineOfSightTarget, Vector(-0.5f,-0.5f,-0.5f), Vector(0.5f,0.5f,0.5f), QAngle(0,0,0), 0,0,255,255, 5 );

				UTIL_TraceLine( player->EyePosition(), vecLineOfSightTarget, CONTENTS_SOLID, this, COLLISION_GROUP_NONE, &result );

				if ( result.fraction < 1.0f )
				{
					//debugoverlay->AddLineOverlay( result.startpos, result.endpos, 255,0,0, 255, 0.15f, 10 );
					continue;
				}
				//debugoverlay->AddLineOverlay( result.startpos, result.endpos, 0,255,0, 255, 0.15f, 10 );

				// this trace succeeded, so we're ok to defuse.
				bFoundBomb = true;
				break;
			}

			if ( !bFoundBomb )
				return;
		}

		if ( m_bBeingDefused )
		{
			if ( player != m_pBombDefuser )
			{
				if ( player->m_iNextTimeCheck < gpGlobals->curtime )
				{
					ClientPrint( player, HUD_PRINTCENTER, "#SFUI_Notice_Bomb_Already_Being_Defused" );
					player->m_iNextTimeCheck = gpGlobals->curtime + 1.f;
				}
				return;
			}

			m_fLastDefuseTime = gpGlobals->curtime;
		}
		else
		{
			// freeze the player in place while defusing

			IGameEvent * event = gameeventmanager->CreateEvent("bomb_begindefuse" );
			if( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				if ( player->HasDefuser() )
				{
					event->SetInt( "haskit", 1 );
				}
				else
				{
					event->SetInt( "haskit", 0 );
				}
				event->SetInt( "priority", 5 ); // bomb_begindefuse
				gameeventmanager->FireEvent( event );
			}

			Vector soundPosition = player->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );
			EmitSound( filter, 0, "c4.disarmstart", &GetAbsOrigin() );

/*
			if ( m_pBombDefuser == player && gpGlobals->curtime < ( m_fLastDefuseTime + C4_DEFUSE_GRACE_PERIOD ) )
			{
				// if we're allowing the player to continue defusing, push the completion time ahead by the appropriate amount
				float fTimeGap = m_fLastDefuseTime - gpGlobals->curtime;
				m_flDefuseCountDown += fTimeGap;

				// we don't have a method for setting up the progress bar "in progress", so do it manually
				player->m_iProgressBarDuration = m_flDefuseLength;
				player->m_flProgressBarStartTime = m_flDefuseCountDown - m_flDefuseLength;
			}
			else
*/
			{
				m_flDefuseLength = player->HasDefuser() ? 5 : 10;
				m_flDefuseCountDown = gpGlobals->curtime + m_flDefuseLength;
				player->SetProgressBarTime( m_flDefuseLength );
			}

			m_pBombDefuser = player;
			m_bBeingDefused = true;
			player->m_bIsDefusing = true;
			
			m_fLastDefuseTime = gpGlobals->curtime;

			//start the progress bar

			player->OnStartedDefuse();

			m_OnBombBeginDefuse.FireOutput(this, player);
		}
	}

	void CPlantedC4::ActivateSetTimerLength( float flTimerLength )
	{
		if ( flTimerLength < 0 )
		{
			Error( "ActivateSetTimerLength value is less than 0!" );
			return;
		}

		// Detonate in "time" seconds
		m_flTimerLength = flTimerLength;
		m_flC4Blow = gpGlobals->curtime + m_flTimerLength;

		SetThink( &CPlantedC4::C4Think );
		SetNextThink( gpGlobals->curtime + 0.1f );

		m_bBombTicking = true;
	}

///////////////////////////////////////////////////////////
// Training version of the C4 - doesn't explode
///////////////////////////////////////////////////////////

	LINK_ENTITY_TO_CLASS( planted_c4_training, CPlantedC4Training );
	PRECACHE_REGISTER( planted_c4_training );

	IMPLEMENT_REFLECT_DATAMAP( CPlantedC4Training )

	void CPlantedC4Training::InputActivateSetTimerLength( inputdata_t &inputdata )
	{
		ActivateSetTimerLength( inputdata.value.Float() );
	}

	void CPlantedC4Training::Explode( trace_t *pTrace, int bitsDamageType )
	{
		// Check to see if the round is over after the bomb went off...
		CSGameRules()->m_bTargetBombed = true;
		m_bBombTicking = false;
		m_bHasExploded = true;

		// Pull out of the wall a bit
		if ( pTrace->fraction != 1.0 )
		{
			SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
		}

		QAngle	vecAngles;
		DispatchParticleEffect( "c4_train_ground_effect", GetAbsOrigin(), vecAngles );

		// Sound! for everyone
		CBroadcastRecipientFilter filter;
		EmitSound( filter, 0, "tr.C4Explode", &GetAbsOrigin() );

		// Decal!
		//UTIL_DecalTrace( pTrace, "Scorch" );

		// Shake!
		//UTIL_ScreenShake( pTrace->endpos, 25.0, 150.0, 1.0, 3000, SHAKE_START );

		SetOwnerEntity( nullptr ); // can't traceline attack owner if this is set

		m_OnBombExploded.FireOutput(this, this);
	}

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( C4, DT_WeaponC4 )

IMPLEMENT_REFLECT_TABLE( CC4, DT_WeaponC4 );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_c4, C4 );
PRECACHE_REGISTER( weapon_c4 );

// -------------------------------------------------------------------------------- //
// Globals.
// -------------------------------------------------------------------------------- //

static const float DROPPED_LIGHT_INTERVAL = 1.0f;

// -------------------------------------------------------------------------------- //
// CC4 implementation.
// -------------------------------------------------------------------------------- //

CC4::CC4()
{
	m_bDroppedFromDeath = false;

	SetSpotRules( CCSEntitySpotting::SPOT_RULE_CT | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
	m_vecLastValidPlayerHeldPosition = Vector(0,0,0);

	m_bIsPlantingViaUse = false;
}

CC4::~CC4()
{
}

void CC4::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	//Don't allow players to shoot the C4 around
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	//Don't be damaged / moved by explosions
	m_takedamage = DAMAGE_NO;

	m_bBombPlanted = false;
	
	SetNextThink( gpGlobals->curtime );

}

void CC4::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Disable all the firing code.. the C4 grenade is all custom.
	if ( pPlayer->m_nButtons & IN_ATTACK || (pPlayer->m_nButtons & IN_USE && m_bIsPlantingViaUse) )
	{
		if ( gpGlobals->curtime >= m_flNextPrimaryAttack ) 
			PrimaryAttack();
	}
	else
	{
		WeaponIdle();
	}

	if ( !(pPlayer->m_nButtons & IN_USE) )
		m_bIsPlantingViaUse = false;
	
}

void CC4::WeaponReset( void )
{
	m_bShowC4LED = false;

	BaseClass::WeaponReset();
}

void CC4::PhysicsTouchTriggers_Impl(const Vector *pPrevAbsOrigin)
{
	// Normally items like ammo or weapons aren't expected to touch other triggers, but C4 is a special case
	edict_t *pEntity = edict();

	if (pEntity && !IsWorld())
	{
		contract_assert( CollisionProp() );

		//Dropped bombs are both solid and have no owner. In this state, unlike other weapons, they can  
		//now touch triggers so long they haven't had their position reset by a bomb reset trigger.
		if ( IsSolid() && (GetPlayerOwner() == nullptr) )
		{
			SetCheckUntouch(true);
			engine->SolidMoved(pEntity, CollisionProp(), pPrevAbsOrigin, sm_bAccurateTriggerBboxChecks);
		}
	}
}

	void CC4::Think_Impl()
	{
		//If the bomb is held by an alive player standing on the ground, then we can use this
		//position as the last known valid position to respawn the bomb if it gets reset.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer && pPlayer->IsAlive() && FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		{
			m_vecLastValidPlayerHeldPosition = pPlayer->GetAbsOrigin();
		}

		SetNextThink( gpGlobals->curtime + WEAPON_C4_UPDATE_LAST_VALID_PLAYER_HELD_POSITION_INTERVAL );
	}

	void CC4::ResetToLastValidPlayerHeldPosition()
	{
		//When reset, the bomb returns to its last known valid position.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer && GetAbsOrigin() != m_vecLastValidPlayerHeldPosition )
		{
			// Teleport the bomb facing up so the flashing light is clearly visible.
			Vector vecResetPos = m_vecLastValidPlayerHeldPosition + Vector(0,0,8);
			QAngle angResetAng = QAngle( 0, RandomInt(0,360), 0 );

			// trace to ground
			trace_t c4TeleportTrace;
			UTIL_TraceHull( vecResetPos, vecResetPos + Vector(0,0,-8), Vector(-3,-3,-1), Vector(3,3,1), MASK_PLAYERSOLID, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &c4TeleportTrace );
			if ( !c4TeleportTrace.startsolid && c4TeleportTrace.DidHit() )
			{
				vecResetPos += ( c4TeleportTrace.fraction * Vector(0,0,-8) );
			}

			Teleport( &vecResetPos, &angResetAng, nullptr );

			// Set the physics object asleep so it doesn't tumble off precarious ledges and keep resetting.
			IPhysicsObject *pObj = VPhysicsGetObject();
			if ( pObj )
				pObj->Sleep();

		}
	}

	unsigned int CC4::PhysicsSolidMaskForEntity_Impl( void ) const
	{
		return BaseClass::PhysicsSolidMaskForEntity_Impl() | CONTENTS_PLAYERCLIP;
	}

	void CC4::Precache_Impl()
	{
		PrecacheScriptSound( "c4.disarmfinish" );
		PrecacheScriptSound( "c4.explode" );
		PrecacheScriptSound( "c4.disarmstart" );
		PrecacheScriptSound( "c4.plant" );
		PrecacheScriptSound( "C4.PlantSound" );
		PrecacheScriptSound( "c4.click" );

		// training
		PrecacheParticleSystem( "c4_train_ground_effect" );
		PrecacheScriptSound( "tr.C4Explode" );

		BaseClass::Precache_Impl();
	}

	int CC4::UpdateTransmitState_Impl()
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CC4::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
	{
		CBasePlayer *pPlayer = ToBasePlayer( CBaseEntity::Instance( pInfo->m_pClientEnt ) );
		if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
		{
			return FL_EDICT_PVSCHECK;
		}
		else
		{
			// This is needed for the instructor message
			return FL_EDICT_ALWAYS;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Gets info about the control panels
	//-----------------------------------------------------------------------------
	void CC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_view_panel";
	}

	bool CC4::ShouldRemoveOnRoundRestart()
	{
		// Doesn't matter if we have an owner or not.. always remove the C4 when the round restarts.
		// The gamerules will give another C4 to some lucky player.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer && pPlayer->GetActiveWeapon() == this )
			engine->ClientCommand( pPlayer->edict(), "lastinv reset\n" );
		return true;
	}

bool CC4::Deploy( )
{
	m_bShowC4LED = true;

	return BaseClass::Deploy();
}

bool CC4::Holster( CBaseCombatWeapon *pSwitchingTo )
{

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
		pPlayer->SetProgressBarTime( 0 );

	if ( m_bStartedArming )
	{
		AbortBombPlant();
	}

	m_bShowC4LED = false;

	return BaseClass::Holster( pSwitchingTo );
}

void CC4::PrimaryAttack()
{
	bool	bArmingTimeSatisfied = false;
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	int onGround = FBitSet( pPlayer->GetFlags(), FL_ONGROUND );
	CBaseEntity *groundEntity = (onGround) ? pPlayer->GetGroundEntity() : nullptr;
	trace_t trPlant;
	if ( groundEntity )
	{
		// Don't let us stand on players, breakables, or pushaway physics objects to plant
		if ( groundEntity->IsPlayer() ||
			IsPushableEntity( groundEntity ) ||
			IsBreakableEntity( groundEntity ) ||
			IsPushAwayEntity( groundEntity ) )
		{
			onGround = false;
		}

		if ( onGround )
		{
			Vector vecStart = GetAbsOrigin() + Vector(0,0,8);
			Vector vecEnd = GetAbsOrigin() + Vector(0,0,-38);
			UTIL_TraceHull( vecStart, vecEnd, Vector( -3, -3, 0 ), Vector( 3, 3, 16 ), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &trPlant );
			if ( trPlant.fraction == 1.0 )
			{
				onGround = false;
			}
		}
	}

	if( m_bStartedArming == false && m_bBombPlanted == false )
	{
		if( pPlayer->m_bInBombZone && onGround )
		{
			m_bStartedArming = true;
			m_fArmedTime = gpGlobals->curtime + WEAPON_C4_ARM_TIME;
			m_bBombPlacedAnimation = false;

			pPlayer->m_bDuckOverride = true;

			pPlayer->SetAttemptedBombPlace();

			// init the beep flags
			int i;
			for( i=0;i<NUM_BEEPS;i++ )
				m_bPlayedArmingBeeps[i] = false;

			// freeze the player in place while planting

			// player "arming bomb" animation
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
			pPlayer->SetNextAttack( gpGlobals->curtime );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_beginplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 5 ); // bomb_beginplant
				gameeventmanager->FireEvent( event );
			}

			PlayPlantInitSound();

			if ( pPlayer && !pPlayer->IsBot() && pPlayer->m_flC4PlantTalkTimer < gpGlobals->curtime )
			{
				// for console, we don't want to show the chat text because it almost always overlaps 
				// with the bomb planted alert text in the center of the screen
					pPlayer->Radio( "PlantingBomb", "#Cstrike_TitlesTXT_Planting_Bomb", true );
				pPlayer->m_flC4PlantTalkTimer = gpGlobals->curtime + 10.0f;
			}

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );

			if ( pPlayer && !pPlayer->IsDormant() )
			{
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
			}
			//FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_PLANT );
		}
		else
		{
			if ( !pPlayer->GetUseEntity() )
			{
				if ( !pPlayer->m_bInBombZone )
				{
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_At_Bomb_Spot");
				}
				else
				{
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_Must_Be_On_Ground");
				}
			}

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}
	else
	{
		if ( !onGround || !pPlayer->m_bInBombZone )
		{
			if( !pPlayer->m_bInBombZone )
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Arming_Cancelled" );
			}
			else
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_Must_Be_On_Ground" );
			}

			AbortBombPlant();

			if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			{
				SendWeaponAnim( ACT_VM_DRAW );
			}
			else
			{
				SendWeaponAnim( ACT_VM_IDLE );
			}
			
			return;
		}
		else
		{
		// we no longer play arming beeps to all player, we only play the initialization sound
// #ifndef CLIENT_DLL
// 			PlayArmingBeeps();
// #endif

			if( gpGlobals->curtime >= m_fArmedTime ) //the c4 is ready to be armed
			{
				//check to make sure the player is still in the bomb target area
				bArmingTimeSatisfied = true;
			}
			else if( ( gpGlobals->curtime >= (m_fArmedTime - 0.75) ) && ( !m_bBombPlacedAnimation ) )
			{
				//call the c4 Placement animation 
				m_bBombPlacedAnimation = true;

				SendWeaponAnim( ACT_VM_SECONDARYATTACK );
				
			}
		}
	}

	if ( bArmingTimeSatisfied && m_bStartedArming )
	{
		m_bStartedArming = false;
		m_fArmedTime = 0;
		
		if( pPlayer->m_bInBombZone )
		{
			CPlantedC4 *pC4 = CPlantedC4::ShootSatchelCharge( pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsAngles() );

			if ( pC4 && trPlant.fraction < 1.0 )
			{
				pC4->SetBombSiteIndex( pPlayer->m_iBombSiteIndex );
				pC4->SetAbsOrigin( trPlant.endpos );

				//bomb aligns to planted surface normal within a threshold
				if ( fabs(trPlant.plane.normal.Dot(Vector(0,0,1))) > 0.65f )
				{
					//get the player forward vector
					Vector vecFlatForward;
					VectorCopy( pPlayer->Forward(), vecFlatForward );
					vecFlatForward.z = 0;

					//derive c4 forward and right
					Vector vecC4Right = CrossProduct( vecFlatForward.Normalized(), trPlant.plane.normal );
					Vector vecC4Forward = CrossProduct( vecC4Right, trPlant.plane.normal );
					
					QAngle C4Angle;
					VectorAngles( -vecC4Forward, trPlant.plane.normal, C4Angle );
					pC4->SetAbsAngles( C4Angle );
				}

				CBombTarget *pBombTarget = (CBombTarget*)UTIL_EntityByIndex( pPlayer->m_iBombSiteIndex );
				
				if ( pBombTarget )
				{
					CBaseEntity *pAttachPoint = gEntList.FindEntityByName( nullptr, pBombTarget->GetBombMountTarget() );

					if ( pAttachPoint )
					{
						pC4->SetAbsOrigin( pAttachPoint->GetAbsOrigin() );
						pC4->SetAbsAngles( pAttachPoint->GetAbsAngles() );
						pC4->SetParent( pAttachPoint );
					}

					variant_t emptyVariant;
					pBombTarget->AcceptInput( "BombPlanted", pC4, pC4, emptyVariant, 0 );
				}

				// [tj] If the bomb is planted by someone that picked it up after the 
				//      original owner was killed, pass that along to the planted bomb
				pC4->SetPlantedAfterPickup( m_bDroppedFromDeath );
			}

			// Determine how elapsed time from start of round until the bomb was planted
			float plantingTime = gpGlobals->curtime - CSGameRules()->GetRoundStartTime();

			// Award achievement to bomb planter if time <= 25 seconds
			if ( (plantingTime > 0.0f) && 
				 (plantingTime <= AchievementConsts::FastBombPlant_Time) &&
				 !CSGameRules()->IsPlayingGunGameTRBomb() )
			{
				pPlayer->AwardAchievement( CSPlantBombWithin25Seconds );
			}

			pPlayer->SetLastWeaponBeforeAutoSwitchToC4( nullptr ); // completed a bomb plant, this clears out our saved value for switching back to a saved weapon
			pPlayer->SetBombPlacedTime( gpGlobals->curtime );
			CCS_GameStats.Event_BombPlanted( pPlayer );
			CSGameRules()->ScoreBombPlant( pPlayer );

			// record in matchstats
			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				int iCurrentRound = CSGameRules()->GetTotalRoundsPlayed();
				++ pPlayer->m_iMatchStats_Objective.GetForModify( iCurrentRound );

				// Keep track of Match stats in QMM data
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pPlayer->GetHumanPlayerAccountID() ) )
				{
					pQMM->m_iMatchStats_Objective[ iCurrentRound ] = pPlayer->m_iMatchStats_Objective.Get( iCurrentRound );
				}
			}

			pPlayer->AddAccountAward( PlayerCashAward::BOMB_PLANTED );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_planted" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt("posx", pPlayer->GetAbsOrigin().x );
				event->SetInt("posy", pPlayer->GetAbsOrigin().y );
				event->SetInt( "priority", 5 ); // bomb_planted
				gameeventmanager->FireEvent( event );
			}

			// Fire a beep event also so the bots have a chance to hear the bomb
			event = gameeventmanager->CreateEvent( "bomb_beep" );

			if ( event )
			{
				event->SetInt( "entindex", entindex() );
				gameeventmanager->FireEvent( event );
			}

			pPlayer->SetProgressBarTime( 0 );

			CSGameRules()->m_bBombDropped = false;
			CSGameRules()->m_bBombPlanted = true;

			// Play the plant sound.
			// don't play a bomb plant sound for everyone anymore ?
			Vector plantPosition = pPlayer->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( plantPosition );
			EmitSound( filter, 0, "c4.plantquiet", &GetAbsOrigin() );

			// No more c4!
			pPlayer->Weapon_Drop( this, nullptr, nullptr );
			UTIL_Remove( this );

			pPlayer->m_bDuckOverride = false;
			//don't allow the planting to start over again next frame.
			m_bBombPlanted = true;

			if ( CSGameRules()->IsPlayingCooperativeGametype() )
				CSGameRules()->CheckWinConditions();// %plant bomb%

			return;
		}
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Activated_At_Bomb_Spot" );

			//pPlayer->SetAnimation( PLAYER_HOLDBOMB );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 5 ); // bomb_abortplant
				gameeventmanager->FireEvent( event );
			}

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
	SetWeaponIdleTime( gpGlobals->curtime + SharedRandomFloat("C4IdleTime", 10, 15 ) );
}

void CC4::WeaponIdle()
{
	// if the player releases the attack button cancel the arming sequence
	if ( m_bStartedArming )
	{
		AbortBombPlant();

		CCSPlayer *pPlayer = GetPlayerOwner();

		pPlayer->m_bDuckOverride = false;

		// TODO: make this use SendWeaponAnim and activities when the C4 has the activities hooked up.
		if ( pPlayer )
		{
			SendWeaponAnim( ACT_VM_IDLE );
			pPlayer->SetNextAttack( gpGlobals->curtime );
		}

		if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			SendWeaponAnim( ACT_VM_DRAW );
		else
			SendWeaponAnim( ACT_VM_IDLE );
	}
}

void CC4::UpdateShieldState( void )
{
	//ADRIANTODO
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	
	if ( pPlayer->HasShield() )
	{
		pPlayer->SetShieldDrawnState( false );

		CBaseViewModel *pVM = pPlayer->GetViewModel( 1 );

		if ( pVM )
		{
			pVM->AddEffects( EF_NODRAW );
		}
			//pPlayer->SetHitBoxSet( 3 );
	}
	else
		BaseClass::UpdateShieldState();
}

int m_iBeepFrames[NUM_BEEPS] = { 20, 29, 37, 44, 50, 59, 65 };
int iNumArmingAnimFrames = 83;

void CC4::PlayArmingBeeps( void )
{
	float flStartTime = m_fArmedTime - WEAPON_C4_ARM_TIME;

	float flProgress = ( gpGlobals->curtime - flStartTime ) / ( WEAPON_C4_ARM_TIME - 0.75 );

	int currentFrame = (int)( (float)iNumArmingAnimFrames * flProgress );

	int i;
	for( i=0;i<NUM_BEEPS;i++ )
	{
		if( currentFrame <= m_iBeepFrames[i] )
		{
			break;
		}
		else if( !m_bPlayedArmingBeeps[i] )
		{
			m_bPlayedArmingBeeps[i] = true;

			CCSPlayer *owner = GetPlayerOwner();
			if ( !owner && !owner->IsAlive() )
				break;

			Vector soundPosition = owner->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );

			filter.RemoveRecipient( owner );

			// remove anyone that is first person spec'ing the planter
			int i;
			CBasePlayer *pPlayer;
			for( i=1;i<=gpGlobals->maxClients;i++ )
			{
				pPlayer = UTIL_PlayerByIndex( i );

				if ( !pPlayer )
					continue;

				if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == GetOwner() )
				{
					filter.RemoveRecipient( pPlayer );
				}
			}

			EmitSound(filter, 0, "c4.click", &GetAbsOrigin() );
			
			break;
		}
	}
}

void CC4::PlayPlantInitSound( void )
{
	CCSPlayer *owner = GetPlayerOwner();
	if ( !owner && !owner->IsAlive() )
		return;

	Vector soundPosition = owner->GetAbsOrigin() + Vector( 0, 0, 5 );
	CPASAttenuationFilter filter( soundPosition );

	filter.RemoveRecipient( owner );

	// remove anyone that is first person spec'ing the planter
	int i;
	CBasePlayer *pPlayer;
	for( i=1;i<=gpGlobals->maxClients;i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == GetOwner() )
		{
			filter.RemoveRecipient( pPlayer );
		}
	}

	EmitSound(filter, 0, "c4.initiate", &GetAbsOrigin() );
}

float CC4::GetMaxSpeed() const
{
	if ( m_bStartedArming )
		return CS_PLAYER_SPEED_STOPPED;
	else
		return BaseClass::GetMaxSpeed();
}

void CC4::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

	CCSPlayer *pPlayer = dynamic_cast<CCSPlayer *>( pNewOwner );

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_pickup" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "priority", 5 ); // bomb_pickup
		gameeventmanager->FireEvent( event );
	}

	CSGameRules()->m_bBombDropped = false;

	if ( !CSGameRules()->IsPlayingTraining() )
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Got_Bomb" );

	pPlayer->SetBombPickupTime( gpGlobals->curtime );
}

// HACK - Ask Mike Booth...

void CC4::Drop( const Vector &vecVelocity )
{
	if ( !CSGameRules()->m_bBombPlanted ) // its not dropped if its planted
	{
		// tell the bots about the dropped bomb
		TheCSBots()->SetLooseBomb( this );

		CSGameRules()->m_bBombDropped = true;

		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(GetOwnerEntity());
		contract_assert( pPlayer );
		if ( pPlayer )
		{
			CCSPlayer *pCCSPlayer = dynamic_cast<CCSPlayer *>( pPlayer );

			pCCSPlayer->SetBombDroppedTime( gpGlobals->curtime );
			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_dropped" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetInt( "entindex", entindex() );
				event->SetInt( "priority", 5 ); // bomb_dropped
				gameeventmanager->FireEvent( event );

				//event->SetInt( "entindex", iTest );
			}
		}
	}

	if ( m_bStartedArming )
		AbortBombPlant();  // stop arming sequence

	BaseClass::Drop( vecVelocity );
}

void CC4::AbortBombPlant()
{
	m_bStartedArming = false; 

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;

	pPlayer->SetProgressBarTime( 0 );

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
	if( event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		event->SetInt("site", pPlayer->m_iBombSiteIndex );
		event->SetInt( "priority", 5 ); // bomb_abortplant
		gameeventmanager->FireEvent( event );
	}

	if( pPlayer->GetLastWeaponBeforeAutoSwitchToC4() != nullptr )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel();
		if ( vm )
		{
			vm->AddEffects( EF_NODRAW );
		}

		pPlayer->Weapon_Switch( pPlayer->GetLastWeaponBeforeAutoSwitchToC4() );
		pPlayer->SetLastWeaponBeforeAutoSwitchToC4( nullptr );
	}

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_CLEAR_FIRING );
	}
	//FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_ABORT );
}

KS_ENTITY_ABSTRACT_KIND( CWeaponBaseItem )

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CEffectData;
class CIronSightController;
class CWeaponCSBase;
struct animevent_t;

using namespace ::ks::reflect;

// Purpose: Laser Rifle & Shield combo

// CS-PRO TEST CHANGE: instant movement inaccuracy, curve exponent x^0.25
static constexpr auto MOVEMENT_WALK_CURVE01_EXPONENT = 0.85;
static constexpr auto MOVEMENT_CURVE01_EXPONENT = 0.25;

static constexpr auto SILENCER_VISIBLE = 0;
static constexpr auto SILENCER_HIDDEN = 1;

	extern IVModelInfo* modelinfo;

extern ConVar crosshair;

extern WeaponRecoilData g_WeaponRecoilData;

extern ConVar cl_righthand;
extern ConVar mp_weapons_allow_map_placed;
extern ConVar mp_weapons_glow_on_ground;
extern ConVar sv_jump_impulse;

ConVar weapon_land_dip_amt( side_registry(), "weapon_land_dip_amt", "20.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "The amount the gun should dip when the player lands after a jump." );

ConVar weapon_recoil_decay1( side_registry(), "weapon_recoil_decay1_exp", "3.5", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for weapon recoil" );
ConVar weapon_recoil_decay2_exp( side_registry(), "weapon_recoil_decay2_exp", "8", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for weapon recoil" );
ConVar weapon_recoil_decay2_lin( side_registry(), "weapon_recoil_decay2_lin", "18", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor (linear term) for weapon recoil" );
ConVar weapon_recoil_vel_decay( side_registry(), "weapon_recoil_vel_decay", "4.5", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor for weapon recoil velocity" );

ConVar weapon_recoil_decay_coefficient( side_registry(), "weapon_recoil_decay_coefficient", "2.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "" );

ConVar weapon_accuracy_nospread( side_registry(), "weapon_accuracy_nospread", "0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Disable weapon inaccuracy spread" );
ConVar weapon_recoil_cooldown( side_registry(), "weapon_recoil_cooldown", "0.55", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "DEPRECATED. Recoil now decays using weapon_recoil_decay_coefficient");
ConVar weapon_recoil_scale( side_registry(), "weapon_recoil_scale", "2.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Overall scale factor for recoil. Used to reduce recoil on specific platforms");
ConVar weapon_recoil_scale_motion_controller( side_registry(), "weapon_recoil_scale_motion_controller", "1.0", FCVAR_RELEASE | FCVAR_CHEAT |FCVAR_REPLICATED, "Overall scale factor for recoil. Used to reduce recoil.  Only for motion controllers");
ConVar weapon_air_spread_scale( side_registry(), "weapon_air_spread_scale", "1.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Scale factor for jumping inaccuracy, set to 0 to make jumping accuracy equal to standing", true, 0.0f, false, 1.0f );

ConVar weapon_legacy_recoiltable( side_registry(), "weapon_legacy_recoiltable", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar weapon_reticle_knife_show( side_registry(), "weapon_reticle_knife_show", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "When enabled will show knife reticle on clients. Used for game modes requiring target id display when holding a knife." );

ConVar weapon_auto_cleanup_time( side_registry(), "weapon_auto_cleanup_time", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "If set to non-zero, weapons will delete themselves after the specified time (in seconds) if no players are near." );
ConVar weapon_max_before_cleanup( side_registry(), "weapon_max_before_cleanup", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "If set to non-zero, will remove the oldest dropped weapon to maintain the specified number of dropped weapons in the world." );

/*
// TODO[pmf] make these work with recoil tables
ConVar weapon_recoil_seed( side_registry(), "weapon_recoil_seed", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED  );

#if defined( GAME_DLL )
static void weapon_recoil_reseed( const CCommand &args );
static ConCommand weapon_recoil_reseed_command( side_registry(), "weapon_recoil_reseed", weapon_recoil_reseed, "Randomly generate a new recoil seed for the current weapon", FCVAR_DEVELOPMENTONLY );
static void weapon_recoil_reseed( const CCommand &args )
{
	int iNewSeed = RandomInt(0, 0xFFFF);
	weapon_recoil_seed.SetValue(iNewSeed);

	Msg("Recoil seed set to %i\n", iNewSeed);
}
#endif
*/

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

struct WeaponAliasTranslationInfoStruct
{
	const char* alias;
	const char* translatedAlias;
};

static const WeaponAliasTranslationInfoStruct s_WeaponAliasTranslationInfo[] =
{
	{ "cv47", "ak47" },
	{ "defender", "galil" },
	{ "krieg552", "sg552" },
	{ "magnum", "awp" },
	{ "d3au1", "g3sg1" },
	{ "clarion", "famas" },
	{ "bullpup", "aug" },
	{ "krieg550", "sg550" },
	{ "9x19mm", "glock" },
	{ "km45", "usp" },
	{ "228compact", "p228" },
	{ "nighthawk", "deagle" },
	{ "elites", "elite" },
	{ "fn57", "fiveseven" },
	{ "12gauge", "m3" },
	{ "autoshotgun", "xm1014" },
	{ "mp", "tmp" },
	{ "smg", "mp5navy" },
	{ "mp5", "mp5navy" },
	{ "c90", "p90" },
	{ "vest", "kevlar" },
	{ "vesthelm", "assaultsuit" },
	{ "nvgs", "nightvision" },

	{ "", "" } // this needs to be last
};

struct WeaponAliasInfo
{
	CSWeaponID id;
	const char* alias;
};

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the translated alias.
//
const char * GetTranslatedWeaponAlias( const char *szAlias )
{
	for ( int i = 0; i < std::size( s_WeaponAliasTranslationInfo ); ++i )
	{
		if ( V_stricmp( s_WeaponAliasTranslationInfo[i].alias, szAlias ) == 0 )
		{
			return s_WeaponAliasTranslationInfo[i].translatedAlias;
		}
	}

	return szAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a translated alias, return the alias.
//
const char * GetWeaponAliasFromTranslated( const char *translatedAlias )
{
	int i = 0;
	const WeaponAliasTranslationInfoStruct *info = &( s_WeaponAliasTranslationInfo[i] );

	while ( info->alias[0] != 0 )
	{
		if ( V_stricmp( translatedAlias, info->translatedAlias ) == 0 )
		{
			return info->alias;
		}
		info = &( s_WeaponAliasTranslationInfo[++i] );
	}

	return translatedAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a primary weapon
//
bool IsPrimaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
	{
		return pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a secondary weapon
//
bool IsSecondaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
		return pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL;

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a grenade weapon
//
bool IsGrenadeWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
	{
		return pWeaponInfo->iSlot == WEAPON_SLOT_GRENADES;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is armor
//
bool IsArmor( CSWeaponID id )
{
	return id == ITEM_ASSAULTSUIT || id == ITEM_KEVLAR;
}

// ----------------------------------------------------------------------------- //
// CWeaponCSBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBase, DT_WeaponCSBase )

IMPLEMENT_REFLECT_TABLE( CWeaponCSBase, DT_WeaponCSBase );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_cs_base, WeaponCSBase );

inline constexpr const char *REMOVEUNOWNEDWEAPON_THINK_CONTEXT = "WeaponCSBase_RemoveUnownedWeaponThink";
static constexpr auto REMOVEUNOWNEDWEAPON_THINK_INTERVAL = 0.2;
static constexpr auto REMOVEUNOWNEDWEAPON_THINK_REMOVE = 3;

	IMPLEMENT_REFLECT_DATAMAP( CWeaponCSBase )

// must be included after the above macros

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation.
// ----------------------------------------------------------------------------- //
CWeaponCSBase::CWeaponCSBase()
{
	m_nWeaponID = WEAPON_NONE;
	SetPredictionEligible( true );
	m_nextOwnerTouchTime = 0.0f;
	m_nextPrevOwnerTouchTime = 0.0f;
	m_hPrevOwner = nullptr;
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

	m_bCanBePickedUp = true;

	m_fAccuracyPenalty = 0.0f;
	m_fLastShotTime = 0.0f;
	m_weaponMode = Primary_Mode;
	m_fAccuracySmoothedForZoom = 0.0f;
	m_fScopeZoomEndTime = 0.0f;
	m_bBurstMode = false;

	ResetPostponeFireReadyTime();

	m_bWasOwnedByCT = false;
	m_bWasOwnedByTerrorist = false;

	m_iDefaultExtraAmmo = 0;
	m_bFiredOutOfAmmoEvent = false;
	m_numRemoveUnownedWeaponThink = 0;

	m_nEconItemDefIndex = 0;
	m_bReloadVisuallyComplete = false;

	m_bSilencerOn = false;
	m_flDoneSwitchingSilencer = 0.0f;

	m_iOriginalTeamNumber = 0;

	ResetGunHeat();

	m_iIronSightMode = IronSight_should_approach_unsighted;
	m_IronSightController = nullptr;
	UpdateIronSightController();

}

CWeaponCSBase::~CWeaponCSBase()
{

	delete m_IronSightController;
	m_IronSightController = nullptr;

	if ( CSGameRules() )
		CSGameRules()->RemoveDroppedWeaponFromList( this );
}

void CWeaponCSBase::ResetGunHeat()
{

}

bool CWeaponCSBase::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue_Impl( szKeyName, szValue ) )
	{
		if ( FStrEq( szKeyName, "ammo" ) )
		{
			int bullets = std::atoi( szValue );
			if ( bullets < 0 )
				return false;

			m_iDefaultExtraAmmo = bullets;

			return true;
		}
	}

	return false;
}

bool CWeaponCSBase::IsPredicted() const
{
	return true;
}

bool CWeaponCSBase::IsPistol() const
{
	return GetWeaponType() == WEAPONTYPE_PISTOL;
}

bool CWeaponCSBase::PlayEmptySound()
{
	//MIKETODO: certain weapons should override this to make it empty:
	//	C4
	//	Flashbang
	//	HE Grenade
	//	Smoke grenade

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	if ( IsPistol() )
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Pistol" );
	}
	else
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	}

	return 0;
}

const char *CWeaponCSBase::GetShootSound( int iIndex ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		const char *pszSound = pItem->GetStaticData()->GetWeaponReplacementSound( (WeaponSound_t)iIndex );
		if ( pszSound )
		{
			return pszSound;
		}
	}

	return BaseClass::GetShootSound(iIndex);
}

const char *CWeaponCSBase::GetPlayerAnimationExtension( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		return GetCSWpnData().GetPlayerAnimationExtension( pItem );
	}

	return GetCSWpnData().GetPlayerAnimationExtension();
}

const char *CWeaponCSBase::GetAddonModel( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		return GetCSWpnData().GetAddonModel( pItem );
	}

	return GetCSWpnData().GetAddonModel();
}

CCSPlayer* CWeaponCSBase::GetPlayerOwner() const
{
	return dynamic_cast< CCSPlayer* >( GetOwner() );
}

void CWeaponCSBase::AddToPriorOwnerList( CCSPlayer* pPlayer )
{
	if ( !IsAPriorOwner( pPlayer ) )
	{
		// Add player to prior owner list
		m_PriorOwners.push_back( pPlayer );
	}
}

void CWeaponCSBase::AddPriorOwner( CCSPlayer* pPlayer )
{
	if ( pPlayer )
	{
		if ( !IsAPriorOwner( pPlayer ) )
			AddToPriorOwnerList( pPlayer );

		// Assign the team that once owned this weapon
		if ( pPlayer->GetTeamNumber() == TEAM_CT )
		{
			m_bWasOwnedByCT = true;
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			m_bWasOwnedByTerrorist = true;
		}
	}
}

bool CWeaponCSBase::WasOwnedByTeam( int teamNumber )
{
	// Verify if the incoming team matches the team of a previous owner
	switch ( teamNumber )
	{
	case TEAM_CT:
		return m_bWasOwnedByCT;

	case TEAM_TERRORIST:
		return m_bWasOwnedByTerrorist;
	}

	return false;
}

bool CWeaponCSBase::IsAPriorOwner( CCSPlayer* pPlayer ) const
{
	return ( ks::find_index( m_PriorOwners, pPlayer ) != -1 );
}

void CWeaponCSBase::SecondaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( pPlayer->HasShield() == false )
		 BaseClass::SecondaryAttack();
	else
	{
		pPlayer->SetShieldDrawnState( !pPlayer->IsShieldDrawn() );

		if ( pPlayer->IsShieldDrawn() )
			 SendWeaponAnim( ACT_SHIELD_UP );
		else
			 SendWeaponAnim( ACT_SHIELD_DOWN );

		m_flNextSecondaryAttack = gpGlobals->curtime + 0.4;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.4;
	}
}

bool CWeaponCSBase::SendWeaponAnim( int iActivity )
{

	return BaseClass::SendWeaponAnim( iActivity );
}

void CWeaponCSBase::SendViewModelAnim( int nSequence )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || pPlayer->IsTaunting() || pPlayer->IsLookingAtWeapon() )
		return;

	CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
	if (vm)
	{
		bool bIsLookingAt = ( vm->GetSequence() != ACT_INVALID && V_stristr( vm->GetSequenceName(vm->GetSequence()), "lookat" ) );

		if ( vm->GetCycle() < 0.98f && bIsLookingAt && V_stristr( vm->GetSequenceName( nSequence ), "idle" ) )
		{
			// Don't switch from taunt to idle
			return;
		}

	}

	BaseClass::SendViewModelAnim( nSequence );
}

// Common code put here to support separate zoom from silencer/burst
void CWeaponCSBase::CallSecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( m_iClip2 != -1 && !GetReserveAmmoCount( AMMO_POSITION_SECONDARY ) )
	{
		m_bFireOnEmpty = true;
	}

	if ( pPlayer->HasShield() )
		CWeaponCSBase::SecondaryAttack();
	else
		SecondaryAttack();
}

void CWeaponCSBase::UpdateGunHeat( float heat, int iAttachmentIndex )
{

}

void CWeaponCSBase::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	UpdateAccuracyPenalty();

	UpdateShieldState();

	if ( ( m_bInReload ) && ( pPlayer->m_flNextAttack <= gpGlobals->curtime ))
	{
		// the AE_WPN_COMPLETE_RELOAD event should handle the stocking the clip, but in case it's missing, we can do it here as well
		int j = std::min( GetMaxClip1() - m_iClip1, GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
		// Add them to the clip
		m_iClip1 += j;
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -j, true );

		m_bInReload = false;
	}

	if ( (pPlayer->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
	{
		ItemPostFrame_ProcessPrimaryAttack( pPlayer );
	}
	else if ( ( pPlayer->m_nButtons & IN_ZOOM ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		if ( ItemPostFrame_ProcessZoomAction( pPlayer ) )
			pPlayer->m_nButtons &= ~IN_ZOOM;
	}
	else if ( (pPlayer->m_nButtons & IN_ATTACK2 ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ))
	{
		if ( ItemPostFrame_ProcessSecondaryAttack( pPlayer ) )
			pPlayer->m_nButtons &= ~IN_ATTACK2;
	}
	else if ( pPlayer->m_nButtons & IN_RELOAD && GetMaxClip1() != WEAPON_NOCLIP && !m_bInReload && m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		ItemPostFrame_ProcessReloadAction( pPlayer );
	}
	else if ( !( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2|IN_ZOOM ) ) )
	{
		ItemPostFrame_ProcessIdleNoAction( pPlayer );
	}
}

void CWeaponCSBase::ItemPostFrame_ProcessPrimaryAttack( CCSPlayer *pPlayer )
{
	if ( ( m_iClip1 == 0 ) || ( GetMaxClip1() == -1 && !GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) ) )
	{
		m_bFireOnEmpty = true;
	}

	if ( CSGameRules()->IsFreezePeriod() )	// Can't shoot during the freeze period
		return;

	if ( pPlayer->m_bIsDefusing )
		return;

	if ( pPlayer->State_Get() != STATE_ACTIVE )
		return;

	if ( pPlayer->IsShieldDrawn() )
		return;

	// don't repeat fire if this is not a full auto weapon or it's clip is empty
	if ( pPlayer->m_iShotsFired > 0 && ( !IsFullAuto() || m_iClip1 == 0 ) )
		return;

	// ignore attack if we're waiting for the attack button to be released
	if ( pPlayer->m_bWaitForNoAttack )
		return;

	if ( !CanPrimaryAttack() )
		return;

	if ( IsRevolver() ) // holding primary, will fire when time is elapsed
	{

		// don't allow a rapid fire shot instantly in the middle of a haul back hold, let the hammer return first
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;

		if ( GetActivity() != ACT_VM_HAULBACK )
		{
			ResetPostponeFireReadyTime();
			BaseClass::SendWeaponAnim( ACT_VM_HAULBACK );
			return;
		}

		m_weaponMode = Primary_Mode;

		if ( !IsPostponFireReadyTimeElapsed() )
			return;

		if ( m_bFireOnEmpty )
		{
			ResetPostponeFireReadyTime();
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		}

		// we're going to fire after this point

	}

	PrimaryAttack();
	m_fLastShotTime = gpGlobals->curtime;

	if ( IsRevolver() )
	{
		// we just fired.
		// there's a bit of a cool-off before you can alt-fire at normal alt-fire rate
		m_flNextSecondaryAttack = gpGlobals->curtime + ( GetCycleTime( Secondary_Mode ) * 1.7f );
	}
}

bool CWeaponCSBase::ItemPostFrame_ProcessZoomAction( CCSPlayer *pPlayer )
{
	if ( IsRevolver() )	// Revolver treats zoom as secondary fire
		return ItemPostFrame_ProcessSecondaryAttack( pPlayer );

	if ( HasZoom() || IsKindOf( WEAPONTYPE_KNIFE ) )
	{
		CallSecondaryAttack();
	}
	return true;
}

bool CWeaponCSBase::ItemPostFrame_ProcessSecondaryAttack( CCSPlayer *pPlayer )
{
	if ( IsRevolver() )
	{
		if ( CSGameRules()->IsFreezePeriod() )	// you can't fire the revolver in freezetime
			return false;

		if ( pPlayer->m_bIsDefusing )
			return false;

		if ( pPlayer->State_Get() != STATE_ACTIVE )
			return false;

		if ( ( m_iClip1 == 0 ) || ( GetMaxClip1() == -1 && !GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) ) )
		{
			m_bFireOnEmpty = true;
		}

		m_weaponMode = Secondary_Mode;

		if ( !m_bFireOnEmpty )
		{
			ResetPostponeFireReadyTime();

			if ( GetActivity() == ACT_VM_HAULBACK )
			{
				BaseClass::SendWeaponAnim( ACT_VM_IDLE );
				return false;
			}
		}

		if ( pPlayer->m_iShotsFired > 0 ) // revolver secondary isn't full-auto even though primary is
			return false; // shots fired is zeroed when the buttons release

		if ( m_bFireOnEmpty )
		{
			if ( GetActivity() != ACT_VM_HAULBACK )
			{
				ResetPostponeFireReadyTime();
				BaseClass::SendWeaponAnim( ACT_VM_HAULBACK );
				return false;
			}

			if ( !IsPostponFireReadyTimeElapsed() )
				return false;
		}
	}

	CallSecondaryAttack();
	return true;
}

void CWeaponCSBase::ItemPostFrame_ProcessReloadAction( CCSPlayer *pPlayer )
{
	// reload when reload is pressed, or if no buttons are down and weapon is empty.

	//MIKETODO: add code for shields...
	//if ( !FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )

	ItemPostFrame_RevolverResetHaulback();

	if ( !pPlayer->IsShieldDrawn() )
	{
		if ( Reload() )
		{
			// allow the bots to react to the reload
			pPlayer->SetHasReloaded();
			IGameEvent * event = gameeventmanager->CreateEvent( "weapon_reload" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
	}
}

void CWeaponCSBase::ItemPostFrame_ProcessIdleNoAction( CCSPlayer *pPlayer )
{
	ItemPostFrame_RevolverResetHaulback();

	// no fire buttons down
	m_bFireOnEmpty = false;

	// we can fire after this
	pPlayer->m_bWaitForNoAttack = false;

	// set the shots fired to 0 after the player releases a button
	pPlayer->m_iShotsFired = 0;

	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
	{
		if ( !m_bFiredOutOfAmmoEvent && m_iClip1 == 0 && !m_bInReload && !pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "weapon_outofammo" );
			if ( event )
			{
				m_bFiredOutOfAmmoEvent = true;
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
	}

	if ( gpGlobals->curtime > m_flNextPrimaryAttack && m_iClip1 == 0 && IsUseable() && !( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) && !m_bInReload )
	{
		// Reload if current clip is empty and weapon has waited as long as it has to after firing
		Reload();
		pPlayer->SetHasReloaded();
		return;
	}

	UpdateIronSightController();
	if ( m_iIronSightMode == IronSight_viewmodel_is_deploying && GetActivity() != GetDeployActivity() )
	{
		m_iIronSightMode = IronSight_should_approach_unsighted;
	}

	WeaponIdle();
}

void CWeaponCSBase::ItemPostFrame_RevolverResetHaulback()
{
	if ( IsRevolver() ) // not holding any weapon buttons
	{
		m_weaponMode = Secondary_Mode;
		ResetPostponeFireReadyTime();
		if ( GetActivity() == ACT_VM_HAULBACK )
		{
			BaseClass::SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

void CWeaponCSBase::ItemBusyFrame()
{
	UpdateAccuracyPenalty();

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && !( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2|IN_ZOOM ) ) )
	{
		// we can fire after this
		pPlayer->m_bWaitForNoAttack = false;
	}

	BaseClass::ItemBusyFrame();
}

float CWeaponCSBase::GetInaccuracy() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return 0.0f;

	if ( weapon_accuracy_nospread.GetBool() )
		return 0.0f;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float fMaxSpeed = GetMaxSpeed();
	if ( fMaxSpeed == 0.0f )
		fMaxSpeed = GetCSWpnData().GetMaxSpeed( GetEconItemView(), 0 );

	float fAccuracy = m_fAccuracyPenalty;

	// CS-PRO TEST FEATURE
	// Adding movement penalty here results in an instaneous penalty that doesn't persist.
	float flVerticalSpeed = abs( pPlayer->GetAbsVelocity().z );

	float flMovementInaccuracyScale = RemapValClamped(pPlayer->GetAbsVelocity().Length2D(),
		fMaxSpeed * CS_PLAYER_SPEED_DUCK_MODIFIER,
		fMaxSpeed * 0.95f,							// max out at 95% of run speed to avoid jitter near max speed
		0.0f, 1.0f );

	if ( flMovementInaccuracyScale > 0.0f )
	{
		// power curve only applies at speeds greater than walk
		if (( true ) && ( pPlayer->m_bIsWalking ) )
		{
			//flMovementInaccuracyScale *= 1.0;	// reduce inaccuracy when walking or slower. This is commented out because at 1.0, it's a noop but preserved in case a different value is desired.
			//flMovementInaccuracyScale = powf( flMovementInaccuracyScale, float( MOVEMENT_WALK_CURVE01_EXPONENT ) );
		}
		else
		{
			flMovementInaccuracyScale = std::powf( flMovementInaccuracyScale, float( MOVEMENT_CURVE01_EXPONENT ));
		}

		fAccuracy += flMovementInaccuracyScale * weaponInfo.GetInaccuracyMove( GetEconItemView(), m_weaponMode );
	}

	// If we are in the air/on ladder, add inaccuracy based on vertical speed (maximum accuracy at apex of jump)
	if ( pPlayer->GetGroundEntity() == nullptr )
	{
		float flInaccuracyJumpInitial = weaponInfo.GetInaccuracyJumpInitial( GetEconItemView() ) * weapon_air_spread_scale.GetFloat();
		static const float kMaxFallingPenalty = 2.0f;	// Accuracy is never worse than 2x starting penalty

		// Use sqrt here to make the curve more "sudden" around the accurate point at the apex of the jump
		float fSqrtMaxJumpSpeed = std::sqrtf( sv_jump_impulse.GetFloat() );
		float fSqrtVerticalSpeed = sqrtf( flVerticalSpeed );

		float flAirSpeedInaccuracy = RemapVal( fSqrtVerticalSpeed,
			fSqrtMaxJumpSpeed * 0.25f,	// Anything less than 6.25% of maximum speed has no additional accuracy penalty for z-motion (6.25% = .25 * .25)
			fSqrtMaxJumpSpeed,			// Penalty at max jump speed
			0.0f,						// No movement-related penalty when close to stopped
			flInaccuracyJumpInitial );	// Movement-penalty at start of jump

		// Clamp to min/max values.  (Don't use RemapValClamped because it makes clamping to > kJumpMovePenalty hard)
		if ( flAirSpeedInaccuracy < 0 )
			flAirSpeedInaccuracy = 0;
		else if ( flAirSpeedInaccuracy > ( kMaxFallingPenalty * flInaccuracyJumpInitial ) )
			flAirSpeedInaccuracy = kMaxFallingPenalty * flInaccuracyJumpInitial;

		// Apply air velocity inaccuracy penalty
		// (There is an additional penalty for being in the air at all applied in UpdateAccuracyPenalty())
		fAccuracy += flAirSpeedInaccuracy;
	}

	if ( fAccuracy > 1.0f )
		fAccuracy = 1.0f;

	return fAccuracy;
}

int CWeaponCSBase::GetRecoilSeed( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem->IsValid() )
	{
		return GetCSWpnData().GetRecoilSeed( pItem );
	}

	return GetCSWpnData().GetRecoilSeed();
}

const CCSWeaponInfo &CWeaponCSBase::GetCSWpnData() const
{
	// If we don't have a weapon file the below code will do a bogus cast.
	// There is a ton of code that assumes this function can't fail so if somebody
	// is bored they can do a massive find/replace and allow this function to return null.
	// For now, just crash now instead of creating a time bomb for later.
	if ( GetWeaponFileInfoHandle() == GetInvalidWeaponInfoHandle() )
	{
		CFmtStr outputStr( "Weapon '%s' script file not found, but its data was accessed. This error is fatal.\n", GetName() ? GetName() : "<unknown>" );
		contract_assert( 0 );
	}

	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CCSWeaponInfo *pCSInfo = static_cast< const CCSWeaponInfo* >( pWeaponInfo );
	return *pCSInfo;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CWeaponCSBase::GetViewModel( int /*viewmodelindex = 0 -- this is ignored in the base class here*/ ) const
{
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == nullptr )
	{
		 return BaseClass::GetViewModel();
	}

	if ( pOwner->HasShield() && CanBeUsedWithShield() )
		 return GetShieldViewModel();
	else
		 return BaseClass::GetViewModel();
}

// bool CWeaponCSBase::AllowTaunts( void )
// {
// 	//return true; // hack: all weapons can taunt for now
// 	return CALL_ATTRIB_HOOK_BOOL( taunt_id );
// }

void CWeaponCSBase::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	if ( GetSilencerModel()[0] != 0 )
	{
		PrecacheModel( GetSilencerModel() );
	}

	PrecacheScriptSound( "Default.ClipEmpty_Pistol" );
	PrecacheScriptSound( "Default.ClipEmpty_Rifle" );
	if ( GetZoomInSound() && GetZoomInSound()[0] )
	{
		PrecacheScriptSound( GetZoomInSound() );
	}
	if ( GetZoomOutSound() && GetZoomOutSound()[0] )
	{
		PrecacheScriptSound( GetZoomOutSound() );
	}

	const char *pMuzzleFlashEffectName_1stPerson = GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	if ( pMuzzleFlashEffectName_1stPerson && pMuzzleFlashEffectName_1stPerson[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_1stPerson );
	}

	const char *pMuzzleFlashEffectName_1stPersonAlt = GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	if ( pMuzzleFlashEffectName_1stPersonAlt && pMuzzleFlashEffectName_1stPersonAlt[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_1stPersonAlt );
	}

	const char *pMuzzleFlashEffectName_3rdPerson = GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	if ( pMuzzleFlashEffectName_3rdPerson && pMuzzleFlashEffectName_3rdPerson[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_3rdPerson );
	}

	const char *pMuzzleFlashEffectName_3rdPersonAlt = GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	if ( pMuzzleFlashEffectName_3rdPersonAlt && pMuzzleFlashEffectName_3rdPersonAlt[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_3rdPersonAlt );
	}

	const char *pEjectBrassEffectName = GetCSWpnData().GetEjectBrassEffectName( GetEconItemView() );
	if ( pEjectBrassEffectName && pEjectBrassEffectName[0] )
	{
		PrecacheEffect( pEjectBrassEffectName );
	}

	const char *pHeatEffectName = GetCSWpnData().GetHeatEffectName( GetEconItemView() );
	if ( pHeatEffectName && pHeatEffectName[0] )
	{
		PrecacheEffect( pHeatEffectName );
	}

	PrecacheEffect( "gunshotsplash" );

}

Activity CWeaponCSBase::GetDeployActivity( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (pOwner)
	{
		if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 && LookupActivity( "ACT_VM_EMPTY_DRAW" ) > 0 )
		{
			return ACT_VM_EMPTY_DRAW;
		}
	}
	return ACT_VM_DRAW;
}

bool CWeaponCSBase::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	// Msg( "deploy %s at %f\n", GetClassname(), gpGlobals->curtime );
	CCSPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
	{
		return false;
	}

	pOwner->SetAnimationExtension( szAnimExt );

	SetViewModel();
	SendWeaponAnim( GetDeployActivity() );

	pOwner->SetNextAttack( gpGlobals->curtime + SequenceDuration() );
	m_flNextPrimaryAttack	= gpGlobals->curtime;
	m_flNextSecondaryAttack	= gpGlobals->curtime;

	SetWeaponVisible( true );
	pOwner->SetShieldDrawnState( false );

	if ( pOwner->HasShield() == true )
		 SetWeaponModelIndex( SHIELD_WORLD_MODEL );
	else
		 SetWeaponModelIndex( szWeaponModel );

//#ifndef CLIENT_DLL
//	SetWeaponModules();
//#endif

	m_iIronSightMode = IronSight_viewmodel_is_deploying;

	return true;
}

void CWeaponCSBase::UpdateShieldState( void )
{
	//empty by default.
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == nullptr )
		 return;

	//ADRIANTODO
	//Make the hitbox set switches here!!!
	if ( pOwner->HasShield() == false )
	{

		pOwner->SetShieldDrawnState( false );
		//pOwner->SetHitBoxSet( 0 );
		return;
	}
	else
	{
		//pOwner->SetHitBoxSet( 1 );
	}
}

void CWeaponCSBase::SetWeaponModelIndex( const char *pName )
{
 	 m_iWorldModelIndex = modelinfo->GetModelIndex( pName );
}

bool CWeaponCSBase::CanBeSelected( void )
{
	if ( !VisibleInWeaponSelection() )
		return false;

	return true;
}

bool CWeaponCSBase::CanDeploy( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer->HasShield() && CanBeUsedWithShield() == false )
		 return false;

	return BaseClass::CanDeploy();
}

float CWeaponCSBase::CalculateNextAttackTime( float fCycleTime )
{
	float fCurAttack = m_flNextPrimaryAttack;
	float fDeltaAttack = gpGlobals->curtime - fCurAttack;
	if ( fDeltaAttack < 0 || fDeltaAttack > gpGlobals->interval_per_tick )
	{
		fCurAttack = gpGlobals->curtime;
	}
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = fCurAttack + fCycleTime;

	return fCurAttack;
}

bool CWeaponCSBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer )
		pPlayer->SetFOV( pPlayer, 0, 0.0f ); // reset the default FOV.

	if ( pPlayer )
		pPlayer->SetShieldDrawnState( false );

	ResetGunHeat();

	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponCSBase::Deploy()
{

	if ( GetIronSightController() )
		GetIronSightController()->SetState( IronSight_viewmodel_is_deploying );

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		pPlayer->m_iShotsFired = 0;
		pPlayer->m_bResumeZoom = false;
		pPlayer->m_bIsScoped = false;
		pPlayer->SetFOV( pPlayer, 0 );
		pPlayer->m_bWaitForNoAttack = true;
	}
	m_fAccuracyPenalty = 0.0f;
//	m_iRecoilIndex = 0;	DEPRECATED
	m_flRecoilIndex = 0.0f;

	ResetGunHeat();

	return BaseClass::Deploy();
}

bool CWeaponCSBase::IsRemoveable()
{
	if ( BaseClass::IsRemoveable() == true )
	{
		if ( m_nextPrevOwnerTouchTime > gpGlobals->curtime || m_nextOwnerTouchTime > gpGlobals->curtime )
		{
			return false;
		}
	}

	// currently held or never had an owner (level designer placed)
	if ( GetOwner() || !m_hPrevOwner.m_Value.IsValid() )
		return false;

	return BaseClass::IsRemoveable();
}

void CWeaponCSBase::RemoveUnownedWeaponThink()
{
	// Keep thinking in case we need to remove ourselves
	SetContextThink( &CWeaponCSBase::RemoveUnownedWeaponThink, gpGlobals->curtime + REMOVEUNOWNEDWEAPON_THINK_INTERVAL, REMOVEUNOWNEDWEAPON_THINK_CONTEXT );

	if ( GetOwner() )	// owned weapons don't get deleted, reset counter
	{
		if ( m_numRemoveUnownedWeaponThink )
		{
			m_numRemoveUnownedWeaponThink = 0;
		}
		return;
	}

	if ( weapon_auto_cleanup_time.GetFloat() > 0  )
	{
		if ( !GetGroundEntity() )
			return; // gun is falling, and might land near a player - don't remove until we're on the ground

		float flSpawnTime = m_nextOwnerTouchTime;
//		if ( m_nextOwnerTouchTime > flSpawnTime )
//			flSpawnTime = m_nextOwnerTouchTime;

		float flTime = weapon_auto_cleanup_time.GetFloat();

		// check if it's out of ammo, if so clean it up super quick
		if ( m_iClip1 == 0  )
			flTime = std::min<std::common_type_t<int, float>>( 5, flTime ) ;

		if ( flSpawnTime > 0 && (gpGlobals->curtime - flSpawnTime) < flTime )
			return;

		// check to see if any players are close
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer )
				continue;

			if ( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() < 1200 )
				return;
		}
	}
	else if ( m_numRemoveUnownedWeaponThink <= REMOVEUNOWNEDWEAPON_THINK_REMOVE )
	{
		++ m_numRemoveUnownedWeaponThink;
		return; // let the server think a few frames while the owner might acquire this weapon
	}

	// Schedule this weapon to be removed
	UTIL_Remove( this );
}

ConVar mp_weapon_prev_owner_touch_time( side_registry(), "mp_weapon_prev_owner_touch_time", "1.5", FCVAR_CHEAT | FCVAR_REPLICATED );
void CWeaponCSBase::Drop( const Vector &vecVelocity )
{

	// Once somebody drops a gun, it's fair game for removal when/if
	// a game_weapon_manager does a cleanup on surplus weapons in the
	// world.
	SetRemoveable( true );

	StopAnimation();
	StopFollowingEntity( );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	// clear follow stuff, setup for collision
	SetGravity( 1.0 );
	m_iState = WEAPON_NOT_CARRIED;
	RemoveEffects( EF_NODRAW );
	SetGroundEntity( nullptr );

	m_bInReload = false; // stop reloading
	m_bReloadVisuallyComplete = false;

	SetThink( nullptr );
	m_nextPrevOwnerTouchTime = gpGlobals->curtime + mp_weapon_prev_owner_touch_time.GetFloat();
	// [msmith] There is an issue where the model index does not get updated on the client if we let a player
	// pick up a weapon in the same frame that it is thrown down by a different player.
	// The m_nextOwnerTouchTime delay fixes that.
	m_nextOwnerTouchTime = gpGlobals->curtime + 0.1f;
	m_hPrevOwner = GetPlayerOwner();

	SetOwnerEntity( nullptr );
	SetOwner( nullptr );

	VerifyAndSetContextSensitiveWeaponModel();

	FallInit();

	if ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// Don't allow non-c4 weapon pickups in gun game progressive mode
		if ( !IsA( WEAPON_C4 ) )
		{
			SetTouch( nullptr );
		}
		else
		{
			SetTouch( &CWeaponCSBase::DefaultTouch );
		}
	}
	else
	{
		SetTouch( &CWeaponCSBase::DefaultTouch );
	}

	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj != nullptr )
	{
		AngularImpulse	angImp( 100, 100, 100 );
		pObj->AddVelocity( &vecVelocity, &angImp );
	}
	else
	{
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( gpGlobals->curtime );

	// add it to the list of dropped weapons
	if ( CSGameRules() )
		CSGameRules()->AddDroppedWeaponToList( this );

	if ( GetIronSightController() )
		GetIronSightController()->SetState( IronSight_weapon_is_dropped );

}

// whats going on here is that if the player drops this weapon, they shouldn't take it back themselves
// for a little while.  But if they throw it at someone else, the other player should get it immediately.
void CWeaponCSBase::DefaultTouch( CBaseEntity *pOther )
{
	if ( m_bCanBePickedUp == false )
		return;

	if ( pOther->GetFlags() & FL_CONVEYOR )
	{
		pOther->StartTouch( this );
	}

	// Add a small delay in general so the networking has a chance to update the values on the client side before someone
	// picks up the weapon.
	if ( gpGlobals->curtime < m_nextOwnerTouchTime )
	{
		return;
	}

	if ( ( m_hPrevOwner != nullptr ) && ( pOther == m_hPrevOwner ) && ( gpGlobals->curtime < m_nextPrevOwnerTouchTime ) )
	{
		return;
	}

	BaseClass::DefaultTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponCSBase::GetBulletSpread()
{
	static Vector cone = VECTOR_CONE_8DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCSBase::GetDefaultAnimSpeed()
{
	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the laser rifle effect
//-----------------------------------------------------------------------------
void CWeaponCSBase::BulletWasFired( const Vector &vecStart, const Vector &vecEnd )
{
}

bool CWeaponCSBase::ShouldRemoveOnRoundRestart()
{
	if ( GetPlayerOwner() )
		return false;
	else
		return true;
}

void CWeaponCSBase::OnRoundRestart( void )
{
	// Clear out the list of prior owners
//	m_PriorOwners.RemoveAll();
}

//=========================================================
// Materialize - make a CWeaponCSBase visible and tangible
//=========================================================
void CWeaponCSBase::Materialize()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	AddSolidFlags( FSOLID_TRIGGER );

	//SetTouch( &CWeaponCSBase::DefaultTouch );

	SetThink( nullptr );

}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CWeaponCSBase::AttemptToMaterialize()
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	SetNextThink( gpGlobals->curtime + time );
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should
// it respawn?
//=========================================================
void CWeaponCSBase::CheckRespawn()
{
	//GOOSEMAN : Do not respawn weapons!
	return;
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CWeaponCSBase::Respawn_Impl()
{
	// make a copy of this weapon that is invisible and inaccessible to players ( no touch function ). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), GetOwner() );

	if ( pNewWeapon )
	{
		pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
		pNewWeapon->SetTouch( nullptr );// no touch
		pNewWeapon->SetThink( &CWeaponCSBase::AttemptToMaterialize );

		UTIL_DropToFloor( this, MASK_SOLID );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
	}
	else
	{
		Msg( "Respawn failed to create %s!\n", GetClassname() );
	}

	return pNewWeapon;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCSBase::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer )
	{
		m_OnPlayerUse.FireOutput( pActivator, pCaller );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pEvent -
//			*pOperator -
//-----------------------------------------------------------------------------
void CWeaponCSBase::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	int nEvent = pEvent->Event();

	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_SERVER) )
	{
		if ( nEvent == AE_WPN_COMPLETE_RELOAD )
		{
			m_bReloadVisuallyComplete = true;
			CCSPlayer *pPlayer = GetPlayerOwner();

			if ( pPlayer )
			{
				int j = std::min( GetMaxClip1() - m_iClip1, GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );

				// Add them to the clip
				m_iClip1 += j;
				GiveReserveAmmo( AMMO_POSITION_PRIMARY, -j, true );

				m_flRecoilIndex = 0;
			}
		}
		else if ( nEvent == AE_BEGIN_TAUNT_LOOP )
		{
			CCSPlayer *pPlayer = GetPlayerOwner();

			if ( pPlayer && pPlayer->IsLookingAtWeapon() && pPlayer->IsHoldingLookAtWeapon() )
			{
				CBaseViewModel *pViewModel = pPlayer->GetViewModel();

				float flPrevCycle = pViewModel->GetCycle();
				float flNewCycle = V_atof( pEvent->options );
				pViewModel->SetCycle( flNewCycle );

				float flSequenceDuration = pViewModel->SequenceDuration( pViewModel->GetSequence() );
				pPlayer->ModifyTauntDuration( ( flNewCycle - flPrevCycle ) * flSequenceDuration );
			}

 			//if ( pPlayer && pPlayer->IsTaunting() && pPlayer->IsHoldingTaunt() )
 			//{
 			//	CBaseViewModel *pViewModel = pPlayer->GetViewModel();
 			//
 			//	float flPrevCycle = pViewModel->GetCycle();
 			//	float flNewCycle = V_atof( pEvent->options );
 			//	pViewModel->SetCycle( flNewCycle );
 			//
 			//	float flSequenceDuration = pViewModel->SequenceDuration( pViewModel->GetSequence() );
 			//	pPlayer->ModifyTauntDuration( ( flNewCycle - flPrevCycle ) * flSequenceDuration );
 			//}

			return;
		}
		//update the bullet bodygroup on the client
		else if ( nEvent == AE_CL_BODYGROUP_SET_TO_CLIP )
		{
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNumBodygroupIndices = vm->GetNumBodyGroups();

					for ( int iGroup=1; iGroup<iNumBodygroupIndices; iGroup++ )
					{
						vm->SetBodygroup( iGroup, (m_iClip1 >= iGroup) ? 0 : 1 );
					}
				}
			}
		}
		else if ( nEvent == AE_CL_BODYGROUP_SET_TO_NEXTCLIP )
		{
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNextClip = std::min( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
					int iNumBodygroupIndices = vm->GetNumBodyGroups();
					for ( int iGroup=1; iGroup<iNumBodygroupIndices; iGroup++ )
					{
						vm->SetBodygroup( iGroup, (iNextClip >= iGroup) ? 0 : 1 );
					}
				}
			}
		}
		else if ( nEvent == AE_WPN_NEXTCLIP_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the clip amount after an impending reload
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNextClip = std::min( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
					vm->SetPoseParameter( pEvent->options, 1.0f - (((float)iNextClip) / ((float)GetMaxClip1())) );
				}
			}
		}
		else if ( nEvent == AE_WPN_CLIP_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the clip amount
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					vm->SetPoseParameter( pEvent->options, 1.0f - (((float)m_iClip1) / ((float)GetMaxClip1())) );
				}
			}
		}
		else if ( nEvent == AE_WPN_EMPTYSHOTS_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the number of empty shots
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					vm->SetPoseParameter( pEvent->options, std::fmod( m_iNumEmptyAttacks, (float)GetMaxClip1() ) / (float)GetMaxClip1() );
				}
			}
		}
		else if ( nEvent == AE_CL_SHOW_SILENCER )
		{
			if ( CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() ) )
			{
				if ( CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "silencer" ), SILENCER_VISIBLE );
				}
			}

			//world model
			CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
			if ( pWorldModel )
			{
				pWorldModel->SetBodygroupPreset( "show_silencer" );
			}
			else
			{
				SetBodygroup( FindBodygroupByName( "silencer" ), SILENCER_VISIBLE );
			}
		}
		else if ( nEvent == AE_CL_HIDE_SILENCER )
		{
			if ( CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() ) )
			{
				if ( CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "silencer" ), SILENCER_HIDDEN );
				}
			}

			//world model
			CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
			if ( pWorldModel )
			{
				pWorldModel->SetBodygroupPreset( "hide_silencer" );
			}
			else
			{
				SetBodygroup( FindBodygroupByName( "silencer" ), SILENCER_HIDDEN );
			}
		}
		else if ( nEvent == AE_CL_ATTACH_SILENCER_COMPLETE )
		{
			CBasePlayer *pPlayer = ToBasePlayer( GetPlayerOwner() );
			m_bSilencerOn = true;
			m_weaponMode = Secondary_Mode;
			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_on" );
			if ( event && pPlayer )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
		else if ( nEvent == AE_CL_DETACH_SILENCER_COMPLETE )
		{
			CBasePlayer *pPlayer = ToBasePlayer( GetPlayerOwner() );
			m_bSilencerOn = false;
			m_weaponMode = Primary_Mode;
			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_off" );
			if ( event && pPlayer )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
		else if ( nEvent == AE_WPN_CZ_DUMP_CURRENT_MAG )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				//m_iClip1 = 0;
				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "front_mag" ), 1 );
					//world model
					CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
					if ( pWorldModel )
					{
						pWorldModel->SetBodygroup( pWorldModel->FindBodygroupByName( "front_mag" ), 1 );
					}
					else
					{
						SetBodygroup( FindBodygroupByName( "front_mag" ), 1 );
					}

					//if the front mag is removed, all subsequent anims use the non-front mag reload
					m_iReloadActivityIndex = ACT_SECONDARY_VM_RELOAD;
				}
			}
		}
		else if ( nEvent == AE_WPN_CZ_UPDATE_BODYGROUP )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				int iGroupNum = ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0) ? 1 : 0;

				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "front_mag" ), iGroupNum );
					//world model
					CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
					if ( pWorldModel )
					{
						pWorldModel->SetBodygroup( pWorldModel->FindBodygroupByName( "front_mag" ), iGroupNum );
					}
					else
					{
						SetBodygroup( FindBodygroupByName( "front_mag" ), iGroupNum );
					}

					//if the front mag is removed, all subsequent anims use the non-front mag reload
					m_iReloadActivityIndex = ( iGroupNum == 0 ) ? ACT_VM_RELOAD : ACT_SECONDARY_VM_RELOAD;

				}
			}
		}
		else if ( nEvent == AE_WPN_PRIMARYATTACK )
		{

			// delay time
			const char *p = pEvent->options;
			char token[256];
			p = nexttoken(token, p, ' ');
			if ( token )
			{
				SetPostponeFireReadyTime( gpGlobals->curtime + std::atof( token ) );

				// play the prepare warning sound to other players
				CCSPlayer *pCSPlayer = GetPlayerOwner();
				if ( pCSPlayer )
				{
					Vector soundPosition = pCSPlayer->GetAbsOrigin() + Vector( 0, 0, 50 );
					CPASAttenuationFilter filter( soundPosition );
					filter.RemoveRecipient( pCSPlayer );

					// remove anyone that is first person spectating this player, since the viewmodel should play this sound for them on their client.
					int i;
					CBasePlayer *pPlayer;
					for( i=1;i<=gpGlobals->maxClients;i++ )
					{
						pPlayer = UTIL_PlayerByIndex( i );

						if ( !pPlayer )
							continue;

						if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == pCSPlayer )
						{
							filter.RemoveRecipient( pPlayer );
						}
					}

					EmitSound( filter, entindex(), "Weapon_Revolver.Prepare" );
				}

			}

			return;
		}
		else if ( nEvent == AE_CL_BODYGROUP_SET_VALUE )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					char szBodygroupName[256];
					int value = 0;

					char token[256];

					const char *p = pEvent->options;

					// Bodygroup Name
					p = nexttoken(token, p, ' ');
					if ( token )
					{
						V_strncpy( szBodygroupName, token, sizeof(szBodygroupName) );
					}

					// Get the desired value
					p = nexttoken(token, p, ' ');
					if ( token )
					{
						value = std::atoi( token );
					}

					int index = vm->FindBodygroupByName( szBodygroupName );
					if ( index >= 0 )
					{
						vm->SetBodygroup( index, value );
					}
				}
			}
			return;
		}

	}

	BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
}

bool CWeaponCSBase::Reload()
{
	m_bFiredOutOfAmmoEvent = false;
	m_bReloadVisuallyComplete = false;

	m_iIronSightMode = IronSight_should_approach_unsighted;

	return true;//BaseClass::Reload();
}

void CWeaponCSBase::SetEconItemDefinition( CEconItemDefinition *pDef )
{
	if ( !pDef )
		return;

	m_EconItemView.Init( pDef );
	m_nEconItemDefIndex = pDef->GetDefinitionIndex();
}

void CWeaponCSBase::InitializeEconItemView( const char *pszItemName )
{
	if ( m_EconItemView.IsValid() )
		return;

	CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszItemName );
	if ( pDef )
		SetEconItemDefinition( pDef );
}

void CWeaponCSBase::Spawn_Impl()
{
	m_nWeaponID = WeaponIdFromString( GetClassname() );
	InitializeEconItemView( GetClassname() );

	BaseClass::Spawn_Impl();

	// Override the bloat that our base class sets as it's a little bit bigger than we want.
	// If it's too big, you drop a weapon and its box is so big that you're still touching it
	// when it falls and you pick it up again right away.
	CollisionProp()->UseTriggerBounds( true, 30 );

	// Set this here to allow players to shoot dropped weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	SetExtraAmmoCount( m_iDefaultExtraAmmo );	//Start with no additional ammo

	ResetGunHeat();

	m_nextOwnerTouchTime = 0.0f;
	m_nextPrevOwnerTouchTime = 0.0f;

	m_hPrevOwner = nullptr;

	// [tj] initialize donor of this weapon
	m_donor = nullptr;
	m_donated = false;

	m_bSilencerOn = HasSilencer();
	m_weaponMode = HasSilencer() ? Secondary_Mode : Primary_Mode;

	VisibilityMonitor_AddEntity( this, 400.0f, nullptr, nullptr );

	UpdateIronSightController();

	if (  mp_death_drop_gun.GetInt() == 0 && !IsA( WEAPON_C4 ) )
	{
		SetContextThink( &CWeaponCSBase::RemoveUnownedWeaponThink, gpGlobals->curtime + REMOVEUNOWNEDWEAPON_THINK_INTERVAL, REMOVEUNOWNEDWEAPON_THINK_CONTEXT );
	}

}

bool CWeaponCSBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
{
	if ( BaseClass::DefaultReload( iClipSize1, iClipSize2, iActivity ) )
	{
		SendActivityEvents();
		return true;
	}
	else
	{
		return false;
	}
}

void CWeaponCSBase::SendActivityEvents( int nActEvents )
{
	CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( GetOwner() );
	if ( !pPlayer )
		return;

	// Send a message to any clients that have this entity to play the reload.
	CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );

	ks::net::CCSUsrMsg_ReloadEffect msg;
	msg.entidx = pPlayer->entindex();
	msg.actanim = nActEvents;

	const Vector& origin = pPlayer->GetAbsOrigin();
	msg.origin_x = origin.x;
	msg.origin_y = origin.y;
	msg.origin_z = origin.z;

	SendUserMessage( filter, ks::net::CS_UM_ReloadEffect, msg );

	if ( nActEvents == ACT_VM_RELOAD || nActEvents == ACT_SECONDARY_VM_RELOAD )
	{
		// Make the player play his reload animation.
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
	}
}

bool CWeaponCSBase::DefaultPistolReload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 )
		return true;

	if ( !DefaultReload( GetCSWpnData().iDefaultClip1, 0, ACT_VM_RELOAD ) )
		return false;

	pPlayer->m_iShotsFired = 0;

	return true;
}

bool CWeaponCSBase::IsUseable()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( Clip1() <= 0 )
	{
		if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 && GetMaxClip1() != -1 )
		{
			// clip is empty ( or nonexistant ) and the player has no more ammo of this type.
			return false;
		}
	}

	return true;
}

void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{

}

float CWeaponCSBase::CalcViewmodelBob( void )
{
	return 0.0f;
}

bool CWeaponCSBase::PhysicsSplash_Impl( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed )
{
	if ( rawSpeed > 20 )
	{

		float size = 4.0f;
		if ( !IsPistol() )
			size += 2.0f;

		// adjust splash size based on speed
		size += RemapValClamped( rawSpeed, 0, 400, 0, 3 );

		CEffectData	data;
		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		data.m_flScale = random_valve->RandomFloat( size, size + 1.0f );

		if ( GetWaterType() & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "gunshotsplash", data );

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pPicker -
//-----------------------------------------------------------------------------
void CWeaponCSBase::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	RemoveEffects( EF_ITEM_BLINK );

	if( pNewOwner->IsPlayer() && pNewOwner->IsAlive() )
	{
		m_OnPlayerPickup.FireOutput( pNewOwner, this );

		m_bFiredOutOfAmmoEvent = false;

		// Robin: We don't want to delete weapons the player has picked up, so
		// clear the name of the weapon. This prevents wildcards that are meant
		// to find NPCs finding weapons dropped by the NPCs as well.
		SetName( NULL_STRING );

		if ( m_hPrevOwner == nullptr )
		{
			CCSPlayer *pPlayer = ToCSPlayer( pNewOwner );
			m_iOriginalTeamNumber = pPlayer ? pPlayer->GetTeamNumber() : 0;
		}

		if ( CSGameRules() )
			CSGameRules()->RemoveDroppedWeaponFromList( this );
	}

	// Someone picked me up, so make it so that I can't be removed.
	SetRemoveable( false );

	UpdateIronSightController();

}

void CWeaponCSBase::UpdateAccuracyPenalty( )
{
	CCSPlayer *pPlayer = GetPlayerOwner( );
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData( );

	float fNewPenalty = 0.0f;

	// Adding movement penalty here results in a penalty that persists and requires decay to eliminate.

	// on ladder?
	if ( pPlayer->GetMoveType( ) == MOVETYPE_LADDER )
	{
		fNewPenalty += weaponInfo.GetInaccuracyLadder( GetEconItemView( ), m_weaponMode ) + weaponInfo.GetInaccuracyLadder( GetEconItemView( ), Primary_Mode );
	}
	// in the air?
	else if ( pPlayer->GetGroundEntity() == nullptr )
	// 	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
	{
		fNewPenalty += weaponInfo.GetInaccuracyStand( GetEconItemView(), m_weaponMode );
		fNewPenalty += weaponInfo.GetInaccuracyJump( GetEconItemView(), m_weaponMode ) * weapon_air_spread_scale.GetFloat();
	}
	else if ( FBitSet( pPlayer->GetFlags( ), FL_DUCKING ) )
	{
		fNewPenalty += weaponInfo.GetInaccuracyCrouch( GetEconItemView( ), m_weaponMode );
	}
	else
	{
		fNewPenalty += weaponInfo.GetInaccuracyStand( GetEconItemView( ), m_weaponMode );
	}

	if ( m_bInReload )
	{
		fNewPenalty += weaponInfo.GetInaccuracyReload( GetEconItemView( ) );
	}

	if ( fNewPenalty > m_fAccuracyPenalty )
	{
		m_fAccuracyPenalty = fNewPenalty;
	}
	else
	{
		float fDecayFactor = std::logf( 10.0f ) / GetRecoveryTime( );

		m_fAccuracyPenalty = Lerp( std::expf( TICK_INTERVAL * -fDecayFactor ), fNewPenalty, ( float ) m_fAccuracyPenalty );
	}

static constexpr auto WEAPON_RECOIL_DECAY_THRESHOLD = 1.10;
	// Decay the recoil index if a little more than cycle time has elapsed since the last shot. In other words,
	// don't decay if we're firing full-auto.
	if ( gpGlobals->curtime > m_fLastShotTime + ( GetCycleTime() * WEAPON_RECOIL_DECAY_THRESHOLD ) )
	{
		float fDecayFactor = std::logf( 10.0f ) * weapon_recoil_decay_coefficient.GetFloat( );

		m_flRecoilIndex = Lerp( std::expf( TICK_INTERVAL * -fDecayFactor ), 0.0f, ( float ) m_flRecoilIndex );
	}
}

float CWeaponCSBase::GetRecoveryTime( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner( );
	if ( !pPlayer )
		return -1.0f;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData( );

	if ( pPlayer->GetMoveType( ) == MOVETYPE_LADDER )
	{
		return weaponInfo.GetRecoveryTimeStand( GetEconItemView( ) );
	}
	else if ( !FBitSet( pPlayer->GetFlags( ), FL_ONGROUND ) )	// in air
	{
		// enforce a large recovery speed penalty (400%) for players in the air; this helps to provide
		// comparable in-air accuracy to the old weapon model

		return weaponInfo.GetRecoveryTimeCrouch( GetEconItemView( ) ) * 4.0f;
	}
	else if ( FBitSet( pPlayer->GetFlags( ), FL_DUCKING ) )
	{
		float flRecoveryTime = weaponInfo.GetRecoveryTimeCrouch( GetEconItemView( ) );
		float flRecoveryTimeFinal = weaponInfo.GetRecoveryTimeCrouchFinal( GetEconItemView( ) );

		if ( flRecoveryTimeFinal != -1.0f )	// uninitialized final recovery values are set to -1.0 from the weapon_base prefab in schema
		{
			int nRecoilIndex = m_flRecoilIndex;

			flRecoveryTime = RemapValClamped( nRecoilIndex, weaponInfo.GetRecoveryTransitionStartBullet( GetEconItemView( ) ), weaponInfo.GetRecoveryTransitionEndBullet( GetEconItemView( ) ), flRecoveryTime, flRecoveryTimeFinal );
		}

		return flRecoveryTime;
	}
	else
	{
		float flRecoveryTime = weaponInfo.GetRecoveryTimeStand( GetEconItemView( ) );
		float flRecoveryTimeFinal = weaponInfo.GetRecoveryTimeStandFinal( GetEconItemView( ) );

		if ( flRecoveryTimeFinal != -1.0f )	// uninitialized final recovery values are set to -1.0 from the weapon_base prefab in schema
		{
			int nRecoilIndex = m_flRecoilIndex;

			flRecoveryTime = RemapValClamped( nRecoilIndex, weaponInfo.GetRecoveryTransitionStartBullet( GetEconItemView( ) ), weaponInfo.GetRecoveryTransitionEndBullet( GetEconItemView( ) ), flRecoveryTime, flRecoveryTimeFinal );
		}

		return flRecoveryTime;
	}
}

void CWeaponCSBase::OnJump( float fImpulse )
{
}

void CWeaponCSBase::OnLand( float fVelocity )
{
	float fPenalty = GetCSWpnData().GetInaccuracyLand( GetEconItemView(), m_weaponMode ) * fVelocity;
	m_fAccuracyPenalty += fPenalty;
	fPenalty = clamp( fPenalty, -1.0f, 1.0f );

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// NOTE: do NOT call GetAimPunchAngle() here because it may be adjusted by some recoil scalar.
	// We just want to update the raw punch angle.
	QAngle angle = pPlayer->GetRawAimPunchAngle();
	float fVKick = RAD2DEG(std::asinf(fPenalty)) * 0.2f;
	float fHKick = SharedRandomFloat("LandPunchAngleYaw", -1.0f, +1.0f) * fVKick * 0.1f;

	angle.x += fVKick;	// pitch
	angle.y += fHKick;	// yaw

	pPlayer->SetAimPunchAngle( angle );
}

void CWeaponCSBase::Recoil( CSWeaponMode weaponMode )
{
	/** Removed for partner depot **/
    //lwss - rebuilt this function from reversing retail bins
    float angle;
    float magnitude;
    int seed;
    CCSPlayer *pPlayer = GetPlayerOwner();

    if ( !pPlayer )
        return;

    //update: Special Thanks to PiMoNFeeD for noticing I missed an if-statement here with the vfunc IsFullAuto().
    // The recoil was a bit wonky.
    if( !IsFullAuto() )
    {
        seed = GetPredictionRandomSeed();
    }
    else
    {
        seed = (int) m_flRecoilIndex;
    }

    if( weapon_legacy_recoiltable.GetBool() )
    {
        GetCSWpnData().GetRecoilOffsets( weaponMode, seed, angle, magnitude );
    }
    else
    {
        g_WeaponRecoilData.GetRecoilOffsets( this, weaponMode, seed, angle, magnitude );
    }

    pPlayer->KickBack( angle, magnitude );
    //lwss end
}

CIronSightController *CWeaponCSBase::GetIronSightController( void )
{
	if ( m_IronSightController && m_IronSightController->IsInitializedAndAvailable() )
	{
		return m_IronSightController;
	}
	return nullptr;
}

void CWeaponCSBase::UpdateIronSightController()
{
	if (!m_IronSightController)
		m_IronSightController = new CIronSightController();

	if (m_IronSightController)
		m_IronSightController->Init(this);
}

int CWeaponCSBase::GetZoomFOV( int nZoomLevel ) const
{
	switch( nZoomLevel )
	{
	case 0: return 0; // not used -- always default FoV
	case 1:	return GetCSWpnData().GetZoomFOV1();
	case 2:	return GetCSWpnData().GetZoomFOV2();
	default: contract_assert( 0 ); return 0;
	}
}

float CWeaponCSBase::GetZoomTime( int nZoomLevel ) const
{
	switch ( nZoomLevel )
	{
	case 0:	return GetCSWpnData().GetZoomTime0();
	case 1:	return GetCSWpnData().GetZoomTime1();
	case 2:	return GetCSWpnData().GetZoomTime2();
	default: contract_assert( 0 ); return 0;
	}
}

CCSPlayer * CWeaponCSBase::GetOriginalOwner()
{
	if ( !m_PriorOwners.empty() )
		return m_PriorOwners[ 0 ];

	return nullptr;
}

BobState_t::BobState_t()
{
	m_flBobTime = 0; 
	m_flLastBobTime = 0;
	m_flLastSpeed = 0;
	m_flVerticalBob = 0;
	m_flLateralBob = 0;
	m_flRawVerticalBob = 0;
	m_flRawLateralBob = 0;
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBaseViewModel;
class CCSPlayer;
class CCSWeaponInfo;
class CIronSightController;

using namespace ::ks::reflect;

static constexpr auto SILENCER_BODYGROUP_UNSET = -2;

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBaseGun, DT_WeaponCSBaseGun )

	IMPLEMENT_REFLECT_TABLE( CWeaponCSBaseGun, DT_WeaponCSBaseGun );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_csbase_gun, WeaponCSBaseGun );

LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( AK47, WeaponCSBaseGun, DT_WeaponAK47, weapon_ak47 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponAug, WeaponCSBaseGun, DT_WeaponAug, weapon_aug );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponAWP, WeaponCSBaseGun, DT_WeaponAWP, weapon_awp );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponBizon, WeaponCSBaseGun, DT_WeaponBizon, weapon_bizon );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponFamas, WeaponCSBaseGun, DT_WeaponFamas, weapon_famas );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponFiveSeven, WeaponCSBaseGun, DT_WeaponFiveSeven, weapon_fiveseven );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponG3SG1, WeaponCSBaseGun, DT_WeaponG3SG1, weapon_g3sg1 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGalil, WeaponCSBaseGun, DT_WeaponGalil, weapon_galil );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGalilAR, WeaponCSBaseGun, DT_WeaponGalilAR, weapon_galilar );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGlock, WeaponCSBaseGun, DT_WeaponGlock, weapon_glock );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponHKP2000, WeaponCSBaseGun, DT_WeaponHKP2000, weapon_hkp2000 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponM4A1, WeaponCSBaseGun, DT_WeaponM4A1, weapon_m4a1 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMAC10, WeaponCSBaseGun, DT_WeaponMAC10, weapon_mac10 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMag7, WeaponCSBaseGun, DT_WeaponMag7, weapon_mag7 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP5Navy, WeaponCSBaseGun, DT_WeaponMP5Navy, weapon_mp5navy );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP7, WeaponCSBaseGun, DT_WeaponMP7, weapon_mp7 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP9, WeaponCSBaseGun, DT_WeaponMP9, weapon_mp9 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponNegev, WeaponCSBaseGun, DT_WeaponNegev, weapon_negev );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP228, WeaponCSBaseGun, DT_WeaponP228, weapon_p228 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP250, WeaponCSBaseGun, DT_WeaponP250, weapon_p250 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP90, WeaponCSBaseGun, DT_WeaponP90, weapon_p90 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( SCAR17, WeaponCSBaseGun, DT_WeaponSCAR17, weapon_scar17 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSCAR20, WeaponCSBaseGun, DT_WeaponSCAR20, weapon_scar20 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponScout, WeaponCSBaseGun, DT_WeaponScout, weapon_scout );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSG550, WeaponCSBaseGun, DT_WeaponSG550, weapon_sg550 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSG556, WeaponCSBaseGun, DT_WeaponSG556, weapon_sg556 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSSG08, WeaponCSBaseGun, DT_WeaponSSG08, weapon_ssg08 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponTec9, WeaponCSBaseGun, DT_WeaponTec9, weapon_tec9 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponTMP, WeaponCSBaseGun, DT_WeaponTMP, weapon_tmp );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponUMP45, WeaponCSBaseGun, DT_WeaponUMP45, weapon_ump45 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponM249, WeaponCSBaseGun, DT_WeaponM249, weapon_m249 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponUSP, WeaponCSBaseGun, DT_WeaponUSP, weapon_usp );

CWeaponCSBaseGun::CWeaponCSBaseGun()
{
	m_pWeaponInfo = nullptr;
	m_zoomLevel = 0;
	m_inPrecache = false;
}

void CWeaponCSBaseGun::Spawn_Impl( )
{
	BaseClass::Spawn_Impl();

	m_bBurstMode = false;
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;	
	ResetPostponeFireReadyTime();
}

void CWeaponCSBaseGun::Precache_Impl()
{
	m_inPrecache = true;
	BaseClass::Precache_Impl();
	m_inPrecache = false;
}

const char * CWeaponCSBaseGun::GetWorldModel( void ) const
{
	return BaseClass::GetWorldModel();
}

Activity CWeaponCSBaseGun::GetDeployActivity( void )
{
	if( IsSilenced() )
	{
		return ACT_VM_DRAW_SILENCED;
	}
	else
	{
		return BaseClass::GetDeployActivity();
	}
}

void CWeaponCSBaseGun::Drop( const Vector &vecVelocity )
{
	// re-deploying the weapon is punishment enough for canceling a silencer attach/detach before completion
	if ( (GetActivity() == ACT_VM_ATTACH_SILENCER && m_bSilencerOn == false) ||
		 (GetActivity() == ACT_VM_DETACH_SILENCER && m_bSilencerOn == true ) )
	{
		m_flDoneSwitchingSilencer = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}

	//make sure the world-model silencer bodygroup is correct, we might have hidden/unhidden it prematurely to make the 3rd-person animation look correct
	else if ( (GetActivity() == ACT_VM_ATTACH_SILENCER) || (GetActivity() == ACT_VM_DETACH_SILENCER) )
	{
		int iBodyGroup = FindBodygroupByName( "silencer" );
		if ( iBodyGroup != -1 )
			SetBodygroup( iBodyGroup, m_bSilencerOn ? 0 : 1 );
	}

	BaseClass::Drop( vecVelocity );
}

void CWeaponCSBaseGun::ItemBusyFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	// if we're scoped during a reload, pull us out of the scope for the duration (and set resumezoom so we'll re-zoom when reloading is done)
	if ( HasZoom() && (IsZoomed() || pPlayer->m_bIsScoped) && m_bInReload )
	{
		//m_zoomLevel = 0; //don't affect zoom level, so it'll restore when reloading is done
		pPlayer->m_bIsScoped = false;
		pPlayer->m_bResumeZoom = true;
		pPlayer->SetFOV( pPlayer, GetZoomFOV( 0 ), GetZoomTime( 0 ) );
		m_weaponMode = Primary_Mode;
	}

	BaseClass::ItemBusyFrame();
}

void CWeaponCSBaseGun::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	// smoother out the accuracy a bit
	//float flFOV = GetFOVForAccuracy();

	//GOOSEMAN : Return zoom level back to previous zoom level before we fired a shot. This is used only for the AWP.
	// And Scout.
	if ( (m_flNextPrimaryAttack <= gpGlobals->curtime) && (pPlayer->m_bResumeZoom == true) 
		&& m_zoomLevel > 0 ) // only need to re-zoom the zoom when there's a zoom to re-zoom to. who knew?
	{
		if ( m_iClip1 != 0 || ( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) )
		{
			m_weaponMode = Secondary_Mode;
			// the zoom amount is taking care of below
			pPlayer->SetFOV( pPlayer, GetZoomFOV( m_zoomLevel ), 0.1f );
			m_fScopeZoomEndTime = gpGlobals->curtime + 0.1;
			pPlayer->m_bIsScoped = true;
		}

		pPlayer->m_bResumeZoom = false;
	}

	/*
	// do this for sniper rifles only and only when the initial zoom has finished zooming
	if ( GetCSWpnData().m_iZoomLevels >= 2 && (m_fScopeZoomEndTime <= gpGlobals->curtime) && m_weaponMode == Secondary_Mode )
	{
		// if we're zoomed in
		if ( IsZoomed() )
		{
			// this is the zoom we are suppoed to be at if we're standing still

			float flFOVDiff = std::max( 0, flFOV - pPlayer->GetFOV() );

			flFOV = ceil( flFOV );
			if ( flFOV < 0 )
				flFOV *= -1;

			if ( flFOVDiff >= 1.0f )
			{
				flFOVDiff = std::min( flFOVDiff, 10.0f )/50;
				if ( flFOVDiff < 0.05f )
					flFOVDiff = 0;

				pPlayer->SetFOV( pPlayer, flFOV, flFOVDiff );
			}
		}
		else
		{
			m_fAccuracySmoothedForZoom = 0;
		}
	}
	*/

	if ( WeaponHasBurst() )
	{
		if ( m_iBurstShotsRemaining > 0 && gpGlobals->curtime >= m_fNextBurstShot )
		{
			BurstFireRemaining();
		}
	}

	BaseClass::ItemPostFrame();
}

bool CWeaponCSBaseGun::SendWeaponAnim( int iActivity )
{
	// firing or reloading should interrupt weapon inspection
	if ( iActivity == ACT_VM_PRIMARYATTACK || iActivity == ACT_VM_RELOAD || iActivity == ACT_SECONDARY_VM_RELOAD || iActivity == ACT_VM_ATTACH_SILENCER || iActivity == ACT_VM_DETACH_SILENCER )
	{
		if ( CCSPlayer *pPlayer = GetPlayerOwner() )
		{
			pPlayer->StopLookingAtWeapon();
		}
	}

	return BaseClass::SendWeaponAnim( iActivity );
}

float CWeaponCSBaseGun::GetFOVForAccuracy( void )
{
	const CCSWeaponInfo& weaponInfo = GetCSWpnData();
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return 0;

	float flDefaultAccuracy = weaponInfo.GetInaccuracyStand( GetEconItemView(), m_weaponMode );
// 	if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
// 	{
// 		flDefaultAccuracy = weaponInfo.GetInaccuracyLadder( m_weaponMode, GetEconItemView() ) + weaponInfo.GetInaccuracyLadder( 0, GetEconItemView() );
// 	}
	if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
 	{
 		flDefaultAccuracy = weaponInfo.GetInaccuracyCrouch( GetEconItemView(), m_weaponMode );
 	}

	m_fAccuracySmoothedForZoom = Approach( GetInaccuracy(), m_fAccuracySmoothedForZoom, gpGlobals->frametime * 10.0f );
	float flTargetFOVForZoom = GetZoomFOV( m_zoomLevel );
	float flFOV = flTargetFOVForZoom;

	// and apply it to the player's fov
	if ( m_fAccuracySmoothedForZoom >= 0 )
	{
		flFOV = flTargetFOVForZoom - ( m_fAccuracySmoothedForZoom - flDefaultAccuracy ) * 10;//std::min( flTargetFOVForZoom * ( 1 + ( m_fAccuracySmoothedForZoom*2 ) ), flTargetFOVForZoom * ( 1 + ( m_fAccuracySmoothedForZoom ) ) );
		//Msg( "flFOV = %f\n", flFOV );
	}

	return flFOV;
}

void CWeaponCSBaseGun::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( CannotShootUnderwater() )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	float flCycleTime = GetCycleTime();

	// change a few things if we're in burst mode
	if ( IsInBurstMode() )
	{
        //lwss - replaced usage of CALL_ATTRIB_HOOK* for GET_WEAPON_ATTR_FUNC() instead.
        flCycleTime = GetBurstModeCycleTime();
		m_iBurstShotsRemaining = 2;
		m_fNextBurstShot = gpGlobals->curtime + GetBurstModeTimeBetweenShots();
	}

	if ( IsZoomed() )
	{
	}
																	
	if ( !CSBaseGunFire( flCycleTime, m_weaponMode ) )								// <--	'PEW PEW' HAPPENS HERE
		return;

	if ( IsSilenced() )
		SendWeaponAnim( ACT_VM_PRIMARYATTACK_SILENCED );

	// Does this gun unzoom after a shot, as in a bolt action rifle?
	if ( IsZoomed() && ( DoesUnzoomAfterShot() ) )
	{
		pPlayer->m_bIsScoped = false;
		pPlayer->m_bResumeZoom = true;
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.05f );
		m_weaponMode = Primary_Mode;
	}
}

void CWeaponCSBaseGun::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer == nullptr )
	{
		contract_assert( pPlayer != nullptr );
		return;
	}

	if ( HasZoom() )
	{
		if ( ++m_zoomLevel > GetZoomLevels() )
			m_zoomLevel = 0;

		bool bIsSniperRifle = GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE;

		if ( IsZoomed() )
		{
			m_weaponMode = Secondary_Mode;

			//float flFOV = GetFOVForAccuracy();

			if ( bIsSniperRifle )
				pPlayer->SetFOV( pPlayer, GetZoomFOV( m_zoomLevel ), GetZoomTime( m_zoomLevel) );
			
			m_fAccuracyPenalty += GetCSWpnData().GetInaccuracyAltSwitch();
			m_fAccuracySmoothedForZoom = 0;

			pPlayer->m_bIsScoped = true;

			if ( pPlayer->GetActiveCSWeapon() )
			{
				CIronSightController *pIronSightController = pPlayer->GetActiveCSWeapon()->GetIronSightController();
				if (pIronSightController)
				{
					pPlayer->GetActiveCSWeapon()->UpdateIronSightController();
					pPlayer->SetFOV(pPlayer, pIronSightController->GetIronSightIdealFOV(), pIronSightController->GetIronSightPullUpDuration());
					pIronSightController->SetState( IronSight_should_approach_sighted );

					//stop looking at weapon when going into ironsights
					pPlayer->StopLookingAtWeapon();

					//force idle animation
					CBaseViewModel *pViewModel = pPlayer->GetViewModel();
					if (pViewModel)
					{
						int nSequence = pViewModel->LookupSequence("idle");
						if (nSequence != ACTIVITY_NOT_AVAILABLE)
						{
							pViewModel->ForceCycle(0);
							pViewModel->ResetSequence(nSequence);
						}
					}
				}
			}
			
		}
		else
		{
			m_weaponMode = Primary_Mode;

			if ( bIsSniperRifle )
			{
				int iFOV = FBitSet( pPlayer->GetFlags(), FL_DUCKING ) ? pPlayer->GetDefaultCrouchedFOV() : pPlayer->GetDefaultFOV();
				pPlayer->SetFOV( pPlayer, iFOV, GetZoomTime( 0 ));
			}

			m_fAccuracySmoothedForZoom = 0;
			pPlayer->m_bIsScoped = false;

			if ( pPlayer->GetActiveCSWeapon() )
			{
				CIronSightController *pIronSightController = pPlayer->GetActiveCSWeapon()->GetIronSightController();
				if (pIronSightController)
				{
					pPlayer->GetActiveCSWeapon()->UpdateIronSightController();
					int iFOV = FBitSet(pPlayer->GetFlags(), FL_DUCKING) ? pPlayer->GetDefaultCrouchedFOV() : pPlayer->GetDefaultFOV();
					pPlayer->SetFOV(pPlayer, iFOV, pIronSightController->GetIronSightPutDownDuration());
					pIronSightController->SetState(IronSight_should_approach_unsighted);
					SendWeaponAnim(ACT_VM_FIDGET);
				}
			}

		}

		// If this isn't guarded, the sound will be emitted twice, once by the server and once by the client.
		// Let the server play it since if only the client plays it, it's liable to get played twice cause of
		// a prediction error. joy.

		// [tj] Playing this from the player so that we don't try to play the sound outside the level.
		if ( GetPlayerOwner() )
		{
			if ( IsZoomed() )
			{
				const char *pszZoomSound = GetZoomInSound();
				if ( pszZoomSound && pszZoomSound[0] )
				{
					GetPlayerOwner()->EmitSound( pszZoomSound );
				}

				//if ( !bIsSniperRifle )
				//{
				//	color32 clr = {0, 0, 0, 200};
				//	float flZoomTime = weaponInfo.m_fZoomTime[m_zoomLevel];
				//	float flBlackTime = std::max( flZoomTime/15, 0.02 );
				//	UTIL_ScreenFade( pPlayer, clr, flBlackTime, (flZoomTime - (flZoomTime/5)) - flBlackTime, FFADE_IN );
				//}
			}
			else
			{
				const char *pszZoomSound = GetZoomOutSound();
				if ( pszZoomSound && pszZoomSound[0] )
				{
					GetPlayerOwner()->EmitSound( pszZoomSound );
				}

				//if ( !bIsSniperRifle )
				//{
				//	color32 clr = {0, 0, 0, 175};
				//	float flZoomTime = weaponInfo.m_fZoomTime[0];
				//	float flBlackTime = std::max( flZoomTime/15, 0.02 );
				//	UTIL_ScreenFade( pPlayer, clr, flBlackTime, flZoomTime - flBlackTime, FFADE_OUT );
				//}
			}

			if ( bIsSniperRifle )
			{
				// let the bots hear the sniper rifle zoom
				IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
			else
			{
				// exists for the game instructor to let it know when the player zoomed in with a regular rifle
				// different from the above weapon_zoom because we don't use this event to notify bots
				IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom_rifle" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}

		m_fScopeZoomEndTime = gpGlobals->curtime + GetZoomTime( m_zoomLevel );
	}
	else if ( WeaponHasBurst() )
	{
		if ( IsInBurstMode() )
		{
			pPlayer->HintMessage( "#Cstrike_TitlesTXT_Switch_To_FullAuto", false );
			m_bBurstMode = false;
			m_weaponMode = Primary_Mode;
		}
		else
		{
			pPlayer->HintMessage( "#Cstrike_TitlesTXT_Switch_To_BurstFire", false );
			m_bBurstMode = true;
			m_weaponMode = Secondary_Mode;
		}

		pPlayer->EmitSound( "Weapon.AutoSemiAutoSwitch" );
	}
	else if ( HasSilencer() && m_flDoneSwitchingSilencer <= gpGlobals->curtime )
	{
		if ( m_bSilencerOn )
		{
			SendWeaponAnim( ACT_VM_DETACH_SILENCER );

			SendActivityEvents( ACT_VM_DETACH_SILENCER );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SILENCER_DETACH );

			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_detach" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}

		}
		else
		{
			SendWeaponAnim( ACT_VM_ATTACH_SILENCER );

			SendActivityEvents( ACT_VM_ATTACH_SILENCER );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SILENCER_ATTACH );
		}

		float nextAttackTime = gpGlobals->curtime + SequenceDuration();

		m_flDoneSwitchingSilencer = nextAttackTime;
		m_flNextSecondaryAttack = nextAttackTime;
		m_flNextPrimaryAttack = nextAttackTime;
		SetWeaponIdleTime( nextAttackTime );

	}
	else if ( IsRevolver() && m_flNextSecondaryAttack < gpGlobals->curtime )
	{
		float flCycletimeAlt = GetCycleTime( Secondary_Mode );
		m_weaponMode = Secondary_Mode;
		UpdateAccuracyPenalty();
		// Logic for weapon_fire event mimics weapon_csbase.cpp CWeaponCSBase::ItemPostFrame() primary fire implementation
		IGameEvent * event = gameeventmanager->CreateEvent( ( HasAmmo() ) ? "weapon_fire" : "weapon_fire_on_empty" );
		if ( event )
		{
			const char *weaponName = GetDefinitionName();
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			event->SetBool( "silenced", IsSilenced() );
			gameeventmanager->FireEvent( event );
		}
		CSBaseGunFire( flCycletimeAlt, Secondary_Mode );								// <--	'PEW PEW' HAPPENS HERE
		m_flNextSecondaryAttack = gpGlobals->curtime + flCycletimeAlt;
		return;
	}

	else
	{
		BaseClass::SecondaryAttack();
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
}

void CWeaponCSBaseGun::BurstFireRemaining()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || m_iClip1 <= 0 )
	{
		m_iClip1 = 0;
		m_iBurstShotsRemaining = 0;
		m_fNextBurstShot = 0.0f;
		return;
	}

    const CCSWeaponInfo& weaponInfo = GetCSWpnData();
    uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	FX_FireBullets(
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(),
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Secondary_Mode,
		CBaseEntity::GetPredictionRandomSeed(  ) & 255,
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		m_fNextBurstShot,
		(HasSilencer() && IsSilenced()) ? SPECIAL1 : SINGLE,
		m_flRecoilIndex );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	--m_iBurstShotsRemaining;

	if ( m_iBurstShotsRemaining > 0 )
	{
	    //lwss - replaced usage of CALL_ATTRIB_HOOK* for GET_WEAPON_ATTR_FUNC() instead.
		m_fNextBurstShot = gpGlobals->curtime + weaponInfo.GetBurstModeTimeBetweenShots( GetEconItemView() );
	}
	else
	{
		m_fNextBurstShot = 0.0f;
	}

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), m_weaponMode );

	// table driven recoil
	Recoil( Secondary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponCSBaseGun::CSBaseGunFire( float flCycleTime, CSWeaponMode weaponMode )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	if ( m_iClip1 == 0 )
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();

			m_iNumEmptyAttacks++;

			// NOTE[pmf]: we don't want to actually play the dry fire animations, as most seem to depict the weapon actually firing.
			// SendWeaponAnim( ACT_VM_DRYFIRE );

			//++pPlayer->m_iShotsFired;	// don't play "auto" empty clicks -- make the player release the trigger before clicking again
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;

			if ( IsRevolver() )
			{
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetCycleTime( weaponMode );
				BaseClass::SendWeaponAnim( ACT_VM_DRYFIRE ); // empty!
			}

		}

		return false;
	}

	float flCurAttack = CalculateNextAttackTime( flCycleTime );

	if ( (GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE && IsZoomed()) || (IsRevolver() && weaponMode == Secondary_Mode) )
	{
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	}
	else if ( IsRevolver() )
	{
		BaseClass::SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else
	{
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	uint16 nItemDefIndex = GetEconItemView()->GetItemIndex();

	FX_FireBullets(
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(),
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		weaponMode,
		CBaseEntity::GetPredictionRandomSeed(  ) & 255,
		GetInaccuracy(),
		GetSpread(), 
		GetAccuracyFishtail(),
		flCurAttack,
		(HasSilencer() && IsSilenced()) ? SPECIAL1 : SINGLE,
		m_flRecoilIndex );

	DoFireEffects();

	SetWeaponIdleTime( gpGlobals->curtime + weaponInfo.GetTimeToIdleAfterFire( GetEconItemView() ) );

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), weaponMode );

	// table driven recoil
	Recoil( weaponMode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;

	return true;
}

bool CWeaponCSBaseGun::IsFullAuto() const
{ 
	if ( BaseClass::IsFullAuto() )
	{
		return !IsInBurstMode(); 
	}
	else
	{
		return false;
	}
}

void CWeaponCSBaseGun::DoFireEffects()
{
	if ( IsSilenced() )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer )
		pPlayer->DoMuzzleFlash();
}

bool CWeaponCSBaseGun::Reload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 )
		return false;

	int iResult = 0;

		iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), m_iReloadActivityIndex );

	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	if ( pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped)
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.0f );
		pPlayer->m_bIsScoped = false;
	}

	pPlayer->m_iShotsFired = 0;
	m_flRecoilIndex += 1.0f;

	return BaseClass::Reload();
}

void CWeaponCSBaseGun::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + GetCSWpnData().GetIdleInterval( GetEconItemView() ) );

		//silencers are bodygroups, so there is no longer a silencer-specific idle.
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

bool CWeaponCSBaseGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// re-deploying the weapon is punishment enough for canceling a silencer attach/detach before completion
	if ( (GetActivity() == ACT_VM_ATTACH_SILENCER && m_bSilencerOn == false) ||
		 (GetActivity() == ACT_VM_DETACH_SILENCER && m_bSilencerOn == true ) )
	{
		m_flDoneSwitchingSilencer = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	// not sure we want to fully support animation cancelling
	if ( m_bInReload && !m_bReloadVisuallyComplete )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime;
	}
	return BaseClass::Holster(pSwitchingTo);
}

bool CWeaponCSBaseGun::Deploy()
{
	// don't allow weapon switching to shortcut cycle time (quickswitch exploit)
	float fOldNextPrimaryAttack	= m_flNextPrimaryAttack;
	float fOldNextSecondaryAttack = m_flNextSecondaryAttack;

	m_flDoneSwitchingSilencer = 0.0f;
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;

	if ( !BaseClass::Deploy() )
		return false;

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	if ( IsRevolver() )
	{
		m_weaponMode = Secondary_Mode;
	}

	m_flNextPrimaryAttack	= Max( m_flNextPrimaryAttack.Get(), fOldNextPrimaryAttack );
	m_flNextSecondaryAttack	= Max( m_flNextSecondaryAttack.Get(), fOldNextSecondaryAttack );
	return true;
}

bool CWeaponCSBaseGun::HasZoom()
{
	return GetZoomLevels() != 0;
}

CCSWeaponInfo const	& CWeaponCSBaseGun::GetCSWpnData() const
{
	if ( m_pWeaponInfo != nullptr )
		return *m_pWeaponInfo;
	else
		return BaseClass::GetCSWpnData();
}

bool CWeaponCSBaseGun::IsInBurstMode() const
{
	return m_bBurstMode;
}

bool CWeaponCSBaseGun::IsZoomed( void ) const
{
	return ( m_zoomLevel > 0 );
}

using namespace ::ks::reflect;

class CDEagle : public CWeaponCSBaseGun
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponDEagle" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDEagle, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CDEagle();

	// overload for dryfire animation
	virtual bool SendWeaponAnim( int iActivity );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_DEAGLE; }

private:
	CDEagle( const CDEagle & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( DEagle, DT_WeaponDEagle )

IMPLEMENT_REFLECT_TABLE( CDEagle, DT_WeaponDEagle );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_deagle, DEagle );
// PRECACHE_REGISTER( weapon_deagle );

CDEagle::CDEagle()
{
}

bool CDEagle::SendWeaponAnim( int iActivity )
{
	if ( iActivity == ACT_VM_PRIMARYATTACK && m_iClip1 == 1 )
		iActivity = ACT_VM_DRYFIRE;
	return BaseClass::SendWeaponAnim( iActivity );
}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( DecoyGrenade, DT_DecoyGrenade )

IMPLEMENT_REFLECT_TABLE( CDecoyGrenade, DT_DecoyGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_decoy, DecoyGrenade );
PRECACHE_REGISTER( weapon_decoy );

void CDecoyGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CDecoyProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

using namespace ::ks::reflect;

class CWeaponElite : public CWeaponCSBaseGun
{
	[[= ks::reflect::NetTable{ .name = "DT_WeaponElite" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWeaponElite, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponElite();

	 void Spawn_Impl();
	 void Precache_Impl();

	virtual void PrimaryAttack();

	// We overload this so we can translate left/right fire activities
	virtual bool SendWeaponAnim( int iActivity );

	virtual void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_ELITE; }
	virtual bool Deploy( void );

protected:
	bool FiringLeft() const;

private:
	
	CWeaponElite( const CWeaponElite & );

	bool m_inPrecache;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponElite, DT_WeaponElite )

IMPLEMENT_REFLECT_TABLE( CWeaponElite, DT_WeaponElite );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_elite, WeaponElite );
// PRECACHE_REGISTER( weapon_elite );

CWeaponElite::CWeaponElite()
{
	m_inPrecache = false;
}

void CWeaponElite::Spawn_Impl( )
{
	BaseClass::Spawn_Impl();
}

void CWeaponElite::Precache_Impl()
{
	m_inPrecache = true;

	PrecacheModel( "models/weapons/w_pist_elite.mdl" );
	PrecacheModel( "models/weapons/w_eq_eholster_elite.mdl" );
	PrecacheModel( "models/weapons/w_eq_eholster.mdl" );
	PrecacheModel( "models/weapons/w_pist_elite_single.mdl" );

	BaseClass::Precache_Impl();

	PrecacheEffect( "CS_MuzzleFlash" );
	m_inPrecache = false;
}

bool CWeaponElite::Deploy( void )
{
	m_iState = WEAPON_IS_CARRIED_BY_PLAYER;
	return BaseClass::Deploy();
}

bool CWeaponElite::FiringLeft() const
{
	// fire left-hand gun with even number of bullets left
	return (m_iClip1 & 1) == 0;
}

void CWeaponElite::PrimaryAttack()
{
	CSBaseGunFire( GetCycleTime(), FiringLeft() ? Primary_Mode : Secondary_Mode);
}

bool CWeaponElite::SendWeaponAnim( int iActivity )
{
	if ( iActivity == ACT_VM_PRIMARYATTACK )
	{
		if ( FiringLeft() )
		{
			if ( m_iClip1 > 2 )
				iActivity = ACT_VM_PRIMARYATTACK;
			else
				iActivity = ACT_VM_DRYFIRE_LEFT;
		}
		else
		{
			if ( m_iClip1 > 2 )
				iActivity = ACT_VM_SECONDARYATTACK;
			else
				iActivity = ACT_VM_DRYFIRE;
		}
	}
	return BaseClass::SendWeaponAnim( iActivity );
}

void CWeaponElite::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if ( m_iClip1 < 2 )
		return;

//	NB. If we show one or more empty guns when idling, we'll get visual pops when holstering or drawing weapons
// 	if ( m_iClip1 == 1 )
// 		SendWeaponAnim( ACT_VM_IDLE_EMPTY_LEFT );

	SendWeaponAnim( ACT_VM_IDLE );
	SetWeaponIdleTime( gpGlobals->curtime + GetCSWpnData().GetIdleInterval() );
}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

static constexpr auto GRENADE_TIMER = 3.0f; //Seconds

IMPLEMENT_NETWORKCLASS_ALIASED( Flashbang, DT_Flashbang )

IMPLEMENT_REFLECT_TABLE( CFlashbang, DT_Flashbang );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_flashbang, Flashbang );
PRECACHE_REGISTER( weapon_flashbang );

	void CFlashbang::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CFlashbangProjectile::Create( 
			vecSrc,
			vecAngles,
			vecVel,
			angImpulse,
			pPlayer,
			weaponInfo );
	}

class CBasePlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;


IMPLEMENT_NETWORKCLASS_ALIASED( HEGrenade, DT_HEGrenade )

IMPLEMENT_REFLECT_TABLE( CHEGrenade, DT_HEGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_hegrenade, HEGrenade );
PRECACHE_REGISTER( weapon_hegrenade );

	void CHEGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CHEGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo, GRENADE_TIMER );
	}

class CCSPlayer;
class CEconItemView;
class CIronSightController;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: Class to control 'aim-down-sights' aka "IronSight" weapon functionality

static constexpr auto IRONSIGHT_VIEWMODEL_BOB_MULT_X = 0.05;
static constexpr auto IRONSIGHT_VIEWMODEL_BOB_PERIOD_X = 6;
static constexpr auto IRONSIGHT_VIEWMODEL_BOB_MULT_Y = 0.1;
static constexpr auto IRONSIGHT_VIEWMODEL_BOB_PERIOD_Y = 10;

#ifdef DEBUG
	ConVar ironsight_override(				side_registry(), "ironsight_override",			"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_position(				side_registry(), "ironsight_position",			"0 0 0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_angle(					side_registry(), "ironsight_angle",				"0 0 0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_fov(					side_registry(), "ironsight_fov",				"60",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_pivot_forward(			side_registry(), "ironsight_pivot_forward",		"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_looseness(				side_registry(), "ironsight_looseness",			"0.1",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_speed_bringup(			side_registry(), "ironsight_speed_bringup",		"4.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_speed_putdown(			side_registry(), "ironsight_speed_putdown",		"2.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	
	ConVar ironsight_catchupspeed(			side_registry(), "ironsight_catchupspeed",		"60.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_running_looseness(		side_registry(), "ironsight_running_looseness",	"0.3",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

	ConVar ironsight_spew_amount(			side_registry(), "ironsight_spew_amount",		"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#else
	constexpr auto ironsight_catchupspeed = 60.0f;
#endif

CIronSightController::CIronSightController( void )
{
	m_bIronSightAvailable			= false;
	m_angPivotAngle.Init();
	m_vecEyePos.Init();
	m_flIronSightAmount				= 0.0f;
	m_flIronSightPullUpSpeed		= 8.0f;
	m_flIronSightPutDownSpeed		= 4.0f;
	m_flIronSightFOV				= 80.0f;
	m_flIronSightPivotForward		= 10.0f;
	m_flIronSightLooseness			= 0.5f;
	m_pAttachedWeapon				= nullptr;	

}

void CIronSightController::SetState( CSIronSightMode newState )
{
	if ( newState == IronSight_viewmodel_is_deploying || newState == IronSight_weapon_is_dropped )
	{
		m_flIronSightAmount = 0.0f;
	}
	if ( m_pAttachedWeapon )
	{
		m_pAttachedWeapon->m_iIronSightMode = newState;
	}
}

bool CIronSightController::IsApproachingSighted(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_should_approach_sighted);
}

bool CIronSightController::IsApproachingUnSighted(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_should_approach_unsighted);
}

bool CIronSightController::IsDeploying(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_viewmodel_is_deploying);
}

bool CIronSightController::IsDropped(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_weapon_is_dropped);
}

void CIronSightController::UpdateIronSightAmount( void )
{

	if (!m_pAttachedWeapon || IsDeploying() || IsDropped())
	{
		//ignore and discard any lingering ironsight amount.
		m_flIronSightAmount = 0.0f;
		m_flIronSightAmountGained = 0.0f;
		return;
	}

	//first determine if we are going into or out of ironsights, and set m_flIronSightAmount accordingly
	float flIronSightAmountTarget = IsApproachingSighted() ? 1.0f : 0.0f;
	float flIronSightUpdOrDownSpeed = IsApproachingSighted() ? m_flIronSightPullUpSpeed : m_flIronSightPutDownSpeed;

#ifdef DEBUG
	if (ironsight_override.GetBool())
		flIronSightUpdOrDownSpeed = IsApproachingSighted() ? ironsight_speed_bringup.GetFloat() : ironsight_speed_putdown.GetFloat();
#endif

	m_flIronSightAmount = Approach(flIronSightAmountTarget, m_flIronSightAmount, gpGlobals->frametime * flIronSightUpdOrDownSpeed);

	m_flIronSightAmountGained = Gain( m_flIronSightAmount, 0.8f );
	m_flIronSightAmountBiased = Bias( m_flIronSightAmount, 0.2f );

#ifdef DEBUG
	if ( ironsight_spew_amount.GetBool() )
		DevMsg( "Ironsight amount: %f, Gained: %f, Biased: %f\n", m_flIronSightAmount, m_flIronSightAmountGained, m_flIronSightAmountBiased );
#endif

}

bool CIronSightController::IsInIronSight( void )
{
	if ( m_pAttachedWeapon )
	{
		if ( IsDeploying() ||
			 IsDropped() ||
			 m_pAttachedWeapon->m_bInReload || 
			 m_pAttachedWeapon->IsSwitchingSilencer() )
			return false;

		CCSPlayer *pPlayer = ToCSPlayer( m_pAttachedWeapon->GetOwner() );
		if ( pPlayer && pPlayer->IsLookingAtWeapon() )
			return false;

		if ( GetIronSightAmount() > 0  && (IsApproachingSighted() || IsApproachingUnSighted()) )
			return true;
	}

	return false;
}

float CIronSightController::GetIronSightFOV( float flDefaultFOV, bool bUseBiasedValue /*= false*/ )
{
	//sets biased value between the current FOV and the ideal IronSight FOV based on how 'ironsighted' the weapon currently is
	
	if ( !IsInIronSight() )
		return flDefaultFOV;

	float flIronSightFOVAmount = bUseBiasedValue ? GetIronSightAmountBiased() : GetIronSightAmount();

#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		return Lerp( flIronSightFOVAmount, flDefaultFOV, ironsight_fov.GetFloat() );
	}
#endif
	
	return Lerp( flIronSightFOVAmount, flDefaultFOV, GetIronSightIdealFOV() );
}

bool CIronSightController::Init( CWeaponCSBase *pWeaponToMonitor )
{
	if ( IsInitializedAndAvailable() )
	{
		return true;
	}
	else if( pWeaponToMonitor )
	{
		// currently only the following hardcoded weapon types support ironsights:
		if (pWeaponToMonitor->GetCSWeaponID() != WEAPON_AUG &&
			pWeaponToMonitor->GetCSWeaponID() != WEAPON_SG556)
		{
			return false;
		}

		if ( pWeaponToMonitor->GetCSWeaponID() == WEAPON_AUG )
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			m_bIronSightAvailable			= true;
			m_flIronSightLooseness			= 0.03f;
			m_flIronSightPullUpSpeed		= 10.0f;
			m_flIronSightPutDownSpeed		= 8.0f;
			m_flIronSightFOV				= 45.0f;
			m_flIronSightPivotForward		= 10.0f;
			m_vecEyePos						= Vector( -1.56, -3.6, -0.07 );
			m_angPivotAngle					= QAngle( 0.78, -0.1, -0.03 );
			return true;
		}
		else if (pWeaponToMonitor->GetCSWeaponID() == WEAPON_SG556)
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			m_bIronSightAvailable			= true;
			m_flIronSightLooseness			= 0.03f;
			m_flIronSightPullUpSpeed		= 10.0f;
			m_flIronSightPutDownSpeed		= 8.0f;
			m_flIronSightFOV				= 45.0f;
			m_flIronSightPivotForward		= 8.0f;
			m_vecEyePos						= Vector(0.72, -5.12, -1.33);
			m_angPivotAngle					= QAngle(0.52, 0.04, 0.72);
			return true;
		}

		CEconItemView *pEconItem = ( (CWeaponCSBase *)pWeaponToMonitor )->GetEconItemView();

		if ( pEconItem && pEconItem->IsValid() )
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			float flTemp;
			uint32 unTemp;

			static CSchemaAttributeDefHandle pAttrbIronSightCapable("aimsight capable");
			if (pEconItem->FindAttribute(pAttrbIronSightCapable, &unTemp))
			{
				m_bIronSightAvailable = (unTemp != 0);
				if (m_bIronSightAvailable)
				{
					static CSchemaAttributeDefHandle pAttrflIronSightLooseness("aimsight looseness");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightLooseness, &flTemp);
					m_flIronSightLooseness = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPullUpSpeed("aimsight speed up");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPullUpSpeed, &flTemp);
					m_flIronSightPullUpSpeed = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPutDownSpeed("aimsight speed down");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPutDownSpeed, &flTemp);
					m_flIronSightPutDownSpeed = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightFOV("aimsight fov");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightFOV, &flTemp);
					m_flIronSightFOV = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPivotForward("aimsight pivot forward");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPivotForward, &flTemp);
					m_flIronSightPivotForward = flTemp;

					static CSchemaAttributeDefHandle pAttrDef_IronSightEyePos("aimsight eye pos");
					pEconItem->FindAttribute(pAttrDef_IronSightEyePos, &m_vecEyePos);

					static CSchemaAttributeDefHandle pAttrDef_IronSightPivotAngle("aimsight pivot angle");
					Vector temp;
					pEconItem->FindAttribute(pAttrDef_IronSightPivotAngle, &temp);
					m_angPivotAngle.x = temp.x;
					m_angPivotAngle.y = temp.y;
					m_angPivotAngle.z = temp.z;

					return true;
				}

			}

		}

	}
	
	return false;
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CEffectData;
class CTakeDamageInfo;

using namespace ::ks::reflect;

static constexpr auto KNIFE_BODYHIT_VOLUME = 128;
static constexpr auto KNIFE_WALLHIT_VOLUME = 512;

static constexpr auto KNIFE_RANGE_SHORT = 32;
static constexpr auto KNIFE_RANGE_LONG = 48;

Vector head_hull_mins( -16, -16, -18 );
Vector head_hull_maxs( 16, 16, 18 );

	//-----------------------------------------------------------------------------
	// Purpose: Only send to local player if this weapon is the active weapon
	// Input  : *pStruct - 
	//			*pVarData - 
	//			*pRecipients - 
	//			objectID - 
	// Output : void*
	//-----------------------------------------------------------------------------
	void* SendProxy_SendActiveLocalKnifeDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
	{
		// Get the weapon entity
		CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon*)pVarData;
		if ( pWeapon )
		{
			// Only send this chunk of data to the player carrying this weapon
			CBasePlayer *pPlayer = ToBasePlayer( pWeapon->GetOwner() );
			if ( pPlayer /*&& pPlayer->GetActiveWeapon() == pWeapon*/ )
			{
				SendProxyRecipients_SetOnly( pRecipients,  pPlayer->GetClientIndex() );
				return (void*)pVarData;
			}
		}
		
		return nullptr;
	}
	REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendActiveLocalKnifeDataTable );

	//-----------------------------------------------------------------------------
	// Purpose: Only send to local player if this weapon is the active weapon
	// Input  : *pStruct - 
	//			*pVarData - 
	//			*pRecipients - 
	//			objectID - 
	// Output : void*
	//-----------------------------------------------------------------------------
	void* SendProxy_SendActiveLocalKnifeGGDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
	{
		// Get the weapon entity
		CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon*)pVarData;
		if ( pWeapon )
		{
			// Only send this chunk of data to the player carrying this weapon
			CBasePlayer *pPlayer = ToBasePlayer( pWeapon->GetOwner() );
			if ( pPlayer /*&& pPlayer->GetActiveWeapon() == pWeapon*/ )
			{
				SendProxyRecipients_SetOnly( pRecipients,  pPlayer->GetClientIndex() );
				return (void*)pVarData;
			}
		}

		return nullptr;
	}
	REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendActiveLocalKnifeGGDataTable );

// ----------------------------------------------------------------------------- //
// CKnife tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( Knife, DT_WeaponKnife )

IMPLEMENT_REFLECT_TABLE( CKnife, DT_WeaponKnife );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_knife, Knife );
PRECACHE_REGISTER( weapon_knife );

// ----------------------------------------------------------------------------- //
// CKnife implementation.
// ----------------------------------------------------------------------------- //

CKnife::CKnife()
{

	m_swingLeft = true;

}

bool CKnife::HasPrimaryAmmo()
{
	return true;
}

bool CKnife::CanBeSelected()
{
	return true;
}

void CKnife::Precache_Impl()
{
	BaseClass::Precache_Impl();

	PrecacheScriptSound( "Weapon_Knife.Deploy" );
	PrecacheScriptSound( "Weapon_Knife.Slash" );
	PrecacheScriptSound( "Weapon_Knife.Stab" );
	PrecacheScriptSound( "Weapon_Knife.Hit" );
	PrecacheEffect( "KnifeSlash" );
}

void CKnife::Spawn_Impl()
{
	m_iClip1 = -1;
	BaseClass::Spawn_Impl();
}

//
// Different knives seem to have a slightly different deploy timing on the animation
// we hardcode the next attack to be consistent with any knife
//
static constexpr auto hardcoded_knife_deploy_time = 1.0f;
bool CKnife::Deploy()
{
	// NOTE (wills): Knives no longer use model bodygroups to change their appearance 
	// between CT and T versions. Team-specific knives now support team-specific
	// viewmodel and world animations, so they are stored as unique models.
	// If a knife needs to look aesthetically different between CT/T teams,
	// add an asset_modifier block to the item definition to divert the whole model.

	/*
	// We need to set the model for the knife based on what team the player is on.
	// We must do this after running deploy to avoid frame locking issues.
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer != NULL )
	{
		MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
		int bodyPartID = ( pPlayer->GetTeamNumber() == TEAM_TERRORIST ) ? 0 : 1;
		SetBodygroup( 0 , bodyPartID );
	}
	*/
	
	if ( !BaseClass::Deploy() )
		return false;

	// Fix for different knife models having different deploy times.  If it's short,
	// you just idle a bit before you attack.  If it's long, we animation-cancel the
	// deploy animation and go straight into the swing/stab after a fixed amount of
	// time.
	if ( CBasePlayer *pOwner = ToBasePlayer( GetOwner() ) )
		pOwner->SetNextAttack( gpGlobals->curtime + hardcoded_knife_deploy_time );

	return true;
}

void CKnife::WeaponAnimation ( int iAnimation )
{
	/*
	int flag;
	#if defined( CLIENT_WEAPONS )
		flag = FEV_NOTHOST;
	#else
		flag = 0;
	#endif

	PLAYBACK_EVENT_FULL( flag, pPlayer->edict(), m_usKnife,
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, 
		0.0,
		0.0,
		iAnimation, 2, 3, 4 );
	*/
}

void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity )
{
	int			i, j, k;
	float		distance;
	Vector minmaxs[2] = {mins, maxs};
	trace_t tmpTrace;
	Vector		vecHullEnd = tr.endpos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
				if ( tmpTrace.fraction < 1.0 )
				{
					float thisDistance = (tmpTrace.endpos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

void CKnife::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		// Move other players back to history positions based on local player's lag
		lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, pPlayer->EyePosition(), pPlayer->EyeAngles(), KNIFE_RANGE_LONG );
		SwingOrStab( Primary_Mode );
		lagcompensation->FinishLagCompensation( pPlayer );
	}
}

void CKnife::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && !pPlayer->m_bIsDefusing && !CSGameRules()->IsFreezePeriod() )
	{
		// Move other players back to history positions based on local player's lag
		lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, pPlayer->EyePosition(), pPlayer->EyeAngles(), KNIFE_RANGE_SHORT );
		SwingOrStab( Secondary_Mode );
		lagcompensation->FinishLagCompensation( pPlayer );
	}
}

void CKnife::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->IsShieldDrawn() )
		 return;

	SetWeaponIdleTime( gpGlobals->curtime + 20 );

	// only idle if the slid isn't back
	SendWeaponAnim( ACT_VM_IDLE );
}

// [tj] Hacky cheat code to control knife damage
	ConVar KnifeDamageScale( side_registry(), "knife_damage_scale", "100", FCVAR_DEVELOPMENTONLY );

bool CKnife::SwingOrStab( CSWeaponMode weaponMode )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	pPlayer->PlayerUsedKnife();

	// bStab: false=primary, true=secondary
	float fRange = (weaponMode == Primary_Mode) ? KNIFE_RANGE_LONG : KNIFE_RANGE_SHORT; // knife range
	
	Vector vForward; AngleVectors( pPlayer->EyeAngles(), &vForward );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * fRange;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	//check for hitting glass - TODO - fix this hackiness, doesn't always line up with what FindHullIntersection returns
	CTakeDamageInfo glassDamage( pPlayer, pPlayer, 42.0f, DMG_BULLET | DMG_NEVERGIB );
	TraceAttackToTriggers( glassDamage, tr.startpos, tr.endpos, vForward );

	if ( tr.fraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.Ent<CBaseEntity>();
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	bool bDidHit = tr.fraction < 1.0f;

	bool bFirstSwing = (m_flNextPrimaryAttack + 0.4) < gpGlobals->curtime;
	if ( bFirstSwing )
	{
		m_swingLeft = true;
	}

	float fPrimDelay, fSecDelay;

	if ( weaponMode == Secondary_Mode )
	{
		fPrimDelay = fSecDelay = bDidHit ? 1.1f : 1.0f;
	}
	else // swing
	{
		fPrimDelay = bDidHit ? 0.5f : 0.4f;
		fSecDelay = bDidHit ? 0.5f : 0.5f;
	}

	if ( pPlayer->HasShield() )
	{
		fPrimDelay += 0.7f; // 0.7 seconds slower if we carry a shield
		fSecDelay += 0.7f;
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + fPrimDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + fSecDelay;
	SetWeaponIdleTime( gpGlobals->curtime + 2 );
	
	bool bBackStab = false;

	if ( bDidHit )
	{
		// server side damage calculations
		CBaseEntity *pEntity = tr.Ent<CBaseEntity>();

		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage();

		float flDamage = 0.f;	// set below
		if ( pEntity && pEntity->IsPlayer() )
		{
			Vector vTragetForward;

			AngleVectors( pEntity->GetAbsAngles(), &vTragetForward );

			Vector2D vecLOS = (pEntity->GetAbsOrigin() - pPlayer->GetAbsOrigin()).AsVector2D();
			Vector2DNormalize( vecLOS );

			float flDot = vecLOS.Dot( vTragetForward.AsVector2D() );

			//Triple the damage if we are stabbing them in the back.
			if ( flDot > 0.475f )
				bBackStab = true;
		}

		if ( weaponMode == Secondary_Mode )
		{
			if ( bBackStab )
			{
				flDamage = 180.0f;
			}
			else
			{
				flDamage = 65.0f;
			}
		}
		else
		{
			if ( bBackStab )
			{
				flDamage = 90;
			}
			else if ( bFirstSwing )
			{
				// first swing does full damage
				flDamage = 40;
			}
			else
			{
				// subsequent swings do less	
				flDamage = 25;
			}
		}

		// [tj] Hacky cheat to lower knife damage for testing
		flDamage *= ( KnifeDamageScale.GetInt() / 100.0f );
	
		if ( weaponMode == Secondary_Mode )
		{
			SendWeaponAnim( bBackStab ? ACT_VM_SWINGHARD : ACT_VM_HITCENTER2 );
		}
		else // swing
		{
			SendWeaponAnim( bBackStab ? ACT_VM_SWINGHIT : ACT_VM_HITCENTER );
		}

		CCSPlayer::StartNewBulletGroup();
		CTakeDamageInfo info( pPlayer, pPlayer, this, flDamage, DMG_SLASH | DMG_NEVERGIB );

		CalculateMeleeDamageForce( &info, vForward, tr.endpos, 1.0f/flDamage );
		pEntity->DispatchTraceAttack( info, vForward, &tr ); 
		ApplyMultiDamage();

		if ( tr.Ent<CBaseEntity>() )
		{
			CPASAttenuationFilter filter( this );
			filter.UsePredictionRules();

			if ( tr.Ent<CBaseEntity>()->IsPlayer()  )
			{
				EmitSound( filter, entindex(),  (weaponMode == Secondary_Mode) ? "Weapon_Knife.Stab" : "Weapon_Knife.Hit" );
			}
			else
			{
				EmitSound( filter, entindex(), "Weapon_Knife.HitWall" );
			}
		}

		CEffectData data;
		data.m_vOrigin = tr.endpos;
		data.m_vStart = tr.startpos;
		data.m_nSurfaceProp = tr.surface.surfaceProps;
		data.m_nDamageType = DMG_SLASH;
		data.m_nHitBox = tr.hitbox;
		data.m_nEntIndex = tr.Ent<CBaseEntity>()->entindex();

		CPASFilter filter( data.m_vOrigin );

		filter.RemoveRecipient( pPlayer );

		data.m_vAngles = pPlayer->GetAbsAngles();
		data.m_fFlags = 0x1;	//IMPACT_NODECAL;
		DispatchEffect( filter, 0.0, "KnifeSlash", data );
	}
	else
	{
		// play wiff or swish sound
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), "Weapon_Knife.Slash" );

		if ( weaponMode == Secondary_Mode )
		{
			SendWeaponAnim( ACT_VM_MISSCENTER2 );
		}
		else // swing
		{
			SendWeaponAnim( ACT_VM_MISSCENTER );
		}
	}

	if ( weaponMode == Secondary_Mode )
	{
		pPlayer->DoAnimationEvent( bBackStab ? PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1 : PLAYERANIMEVENT_FIRE_GUN_SECONDARY );
		m_swingLeft = true;
	}
	else // swing
	{
		// See if we are back stabbing and if we're swinging left (opt) or right.
		pPlayer->DoAnimationEvent( bBackStab ? ( m_swingLeft ? PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1 : PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1 ) : ( m_swingLeft ? PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT : PLAYERANIMEVENT_FIRE_GUN_PRIMARY) );
		m_swingLeft = !m_swingLeft;
	}

	return bDidHit;
}

bool CKnife::CanDrop()
{
	return false;
}

// ----------------------------------------------------------------------------- //
// CKnifeGG tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( KnifeGG, DT_WeaponKnifeGG )

IMPLEMENT_REFLECT_TABLE( CKnifeGG, DT_WeaponKnifeGG );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_knifegg, KnifeGG );
PRECACHE_REGISTER( weapon_knifegg );
