// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "client_class.h"
#include "edict.h"
#include "../predictable_entity.h"
#include "../reflect_predmap.h"
#include "../reflect_recvtable.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "engine/ivmodelinfo.h"
#include "inputsystem/iinputsystem.h"
#include "irecipientfilter.h"
#include "platforminputdevice.h"
#include "../cdll_client_int.h"
#include "../cdll_util.h"
#include "../shareddefs.h"
#include "declare_class.h"
#include "materialsystem/imaterialvar.h"
#include "shaderapi/ishaderapi.h"
#include "view_shared.h"
#include "../baseentity_shared.h"
module ks.client;

import ks.core.platform;
import ks.game.in_buttons;
import ks.game.rumble_shared;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :iinput;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import :cstrike15.cs_shareddefs;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.game.iplayeranimstate;
import ks.tier1.convar;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.strtools;
import :econ.econ_item_schema_minimal;
import std;
import ks.core.dbg;
import ks.core.types;
import ks.math.vector2d;
import ks.tier1.bit_ops;
import ks.tier1.exprevaluator;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import ks.core.random;
import ks.math.vmatrix;

class CBaseHudWeaponSelection;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;

using namespace ::ks::reflect;


IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenade, DT_BaseCSGrenade )

IMPLEMENT_REFLECT_TABLE( CBaseCSGrenade, DT_BaseCSGrenade );

IMPLEMENT_REFLECT_PREDMAP( CBaseCSGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_basecsgrenade, BaseCSGrenade );

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

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::Precache()
{
	BaseClass::Precache();
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

	return BaseClass::Deploy();
}

int CBaseCSGrenade::DrawModel( int flags, const RenderableInstance_t &instance )
{
	//hide the grenade that's in the player's hand while playing grenade throwing animations
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		if ( !pPlayer->m_bUseNewAnimstate && pPlayer->m_PlayerAnimState && pPlayer->m_PlayerAnimState->ShouldHideGrenadeDuringThrow() )
		{
			return 0;
		}
	}
	
	return BaseClass::DrawModel( flags, instance );
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

}

void CBaseCSGrenade::ItemPreFrame()
{
	BaseClass::ItemPreFrame();

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == nullptr )
		return;

	//we want to control the grenade model's visibility so opt out of the fast path
	if (GetBaseAnimating())
		GetBaseAnimating()->SetAllowFastPath(false);

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

		RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), RUMBLE_CROWBAR_SWING, 0, RUMBLE_FLAG_RESTART );
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

	void CBaseCSGrenade::DecrementAmmo( CBaseCombatCharacter *pOwner )
	{
	}

	void CBaseCSGrenade::DropGrenade()
	{
		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;
	}

	void CBaseCSGrenade::ThrowGrenade()
	{
		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;

		CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
		if ( pHudSelection )
		{
			pHudSelection->OnWeaponDrop( this );
		}
	}

	void CBaseCSGrenade::StartGrenadeThrow()
	{
		m_fThrowTime = gpGlobals->curtime + 0.1f;
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

IMPLEMENT_REFLECT_PREDMAP( CWeaponBaseItem );

CWeaponBaseItem::CWeaponBaseItem()
{
	m_bRedraw = false;
}

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::Spawn( void )
{
	m_UseTimer.Invalidate();
	BaseClass::Spawn();
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
		m_UseTimer.Invalidate();

		// remove the ammo
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

		if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, nullptr, nullptr );
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

using namespace ::ks::reflect;

static constexpr auto BLINK_INTERVAL = 2.5;
inline constexpr const char *PLANTED_C4_MODEL = "models/weapons/w_ied_dropped.mdl";
static constexpr auto HEIST_MODE_C4_TIME = 25;

static constexpr auto WEAPON_C4_ARM_TIME = 3.0;

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( C4, DT_WeaponC4 )

IMPLEMENT_REFLECT_TABLE( CC4, DT_WeaponC4 );

IMPLEMENT_REFLECT_PREDMAP( CC4 );

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

	m_szScreenText[0] = '\0';
	m_bShowC4LED = false;

	m_bIsPlantingViaUse = false;
}

CC4::~CC4()
{
}

void CC4::Spawn()
{
	BaseClass::Spawn();

	//Don't allow players to shoot the C4 around
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	//Don't be damaged / moved by explosions
	m_takedamage = DAMAGE_NO;

	m_bBombPlanted = false;
	
	SetNextClientThink( gpGlobals->curtime + DROPPED_LIGHT_INTERVAL );

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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	void CC4::OnDataChanged( DataUpdateType_t type )
	{
		BaseClass::OnDataChanged( type );
	}

	void CC4::UpdateOnRemove( void )
	{
		BaseClass::UpdateOnRemove();
	}

	void CC4::ClientThink( void )
	{
		BaseClass::ClientThink();
		
		SetNextClientThink( gpGlobals->curtime + DROPPED_LIGHT_INTERVAL );
		
		// This think function is just for updating the blinking light of the dropped bomb.
		// So if we have an owner, we don't want to blink.
		if ( IsDormant() || nullptr != GetPlayerOwner() || !CSGameRules()->m_bBombDropped )
		{
			return;
		}
		
		int ledAttachmentIndex = LookupAttachment("led");
		DispatchParticleEffect( "c4_timer_light_dropped", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );

	}

	bool CC4::OnFireEvent( CBaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 7001 )
		{
			// "Custom code" c4 disabled
			//
			/*//if we're clearing the screen text, or have no custom description, set the text normally using the passed param.
			if (   !V_strcmp( options, "" )
				|| !V_strcmp( options, "*******" )
				|| GetEconItemView() == NULL
				|| CALL_ATTRIB_HOOK_BOOL( custom_bombcode ) == false
				|| GetEconItemView()->GetCustomDesc() == NULL
				)
			{
				V_strncpy( m_szScreenText, options, 16 );
			}
			else
			{
				//otherwise we have a custom code to display
				CEconItemView *pItem = GetEconItemView();
				if ( pItem && pItem->GetCustomDesc() )
				{
					if ( strlen(m_szScreenText) >= 7 )
					{
						//if the 7-char code is full, replace it with asterisks (this will happen at the end of the plant animation anyway)
						V_strncpy( m_szScreenText, "*******\0", 8 );
					}
					else
					{
						//we're still under 7 chars, so add an additional char of the custom code
						V_strncpy( m_szScreenText, pItem->GetCustomDesc(), strlen(m_szScreenText)+2 );
					}
				}
			}*/

			//set the screen text to the string in 'options'
			V_strncpy( m_szScreenText, options, 16 );

			return true;
		}
		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	char *CC4::GetScreenText( void )
	{
		return m_szScreenText;
	}

	void CC4::CreateLEDEffect( void )
	{
		if ( GetPlayerOwner() )
			GetPlayerOwner()->CreateC4Effect( GetWeaponForEffect(), true );
	}

	void CC4::RemoveLEDEffect( void )
	{
		if ( GetPlayerOwner() )
			GetPlayerOwner()->RemoveC4Effect( true );
	}

bool CC4::Deploy( )
{
	m_bShowC4LED = true;
	CreateLEDEffect();

	return BaseClass::Deploy();
}

bool CC4::Holster( CBaseCombatWeapon *pSwitchingTo )
{

	RemoveLEDEffect();

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

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );

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
			//don't allow the planting to start over again next frame.
			m_bBombPlanted = true;

			return;
		}
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Activated_At_Bomb_Spot" );

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

}

// HACK - Ask Mike Booth...

void CC4::Drop( const Vector &vecVelocity )
{

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

	// Clear the numbers from the screen if we just aborted.
	m_szScreenText[0] = '\0';

	//FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_ABORT );
}

class CBaseHudWeaponSelection;
class CCSWeaponInfo;
class CHudTexture;
class CIronSightController;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CBaseWeaponWorldModel;
class CCSPlayer;
class CEconItemView;
class CWeaponCSBase;
class FileWeaponInfo_t;

using namespace ::ks::reflect;

// Purpose: Laser Rifle & Shield combo

// CS-PRO TEST CHANGE: instant movement inaccuracy, curve exponent x^0.25
static constexpr auto MOVEMENT_WALK_CURVE01_EXPONENT = 0.85;
static constexpr auto MOVEMENT_CURVE01_EXPONENT = 0.25;

static constexpr auto SILENCER_VISIBLE = 0;
static constexpr auto SILENCER_HIDDEN = 1;

	//#include "glow_outline_effect.h"

	extern IVModelInfoClient* modelinfo;

	extern ConVar spec_show_xray;

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

ConVar cl_weapon_debug_print_accuracy(side_registry(), "cl_weapon_debug_print_accuracy", "0", FCVAR_RELEASE | FCVAR_CHEAT );
ConVar cl_weapon_debug_show_accuracy(side_registry(), "cl_weapon_debug_show_accuracy", "0", FCVAR_RELEASE | FCVAR_CHEAT, "Draws a circle representing the effective range with every shot." );
ConVar cl_weapon_debug_show_accuracy_duration( side_registry(), "cl_weapon_debug_show_accuracy_duration", "10", FCVAR_RELEASE | FCVAR_CHEAT );

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

int GetShellForAmmoType( const char *ammoname )
{
	if ( !V_strcmp( BULLET_PLAYER_762MM, ammoname ) )
		return CS_SHELL_762NATO;

	if ( !V_strcmp( BULLET_PLAYER_556MM, ammoname ) )
		return CS_SHELL_556;

	if ( !V_strcmp( BULLET_PLAYER_338MAG, ammoname ) )
		return CS_SHELL_338MAG;

	if ( !V_strcmp( BULLET_PLAYER_BUCKSHOT, ammoname ) )
		return CS_SHELL_12GAUGE;

	if ( !V_strcmp( BULLET_PLAYER_57MM, ammoname ) )
		return CS_SHELL_57;

	// default 9 mm
	return CS_SHELL_9MM;
}

// ----------------------------------------------------------------------------- //
// CWeaponCSBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBase, DT_WeaponCSBase )

IMPLEMENT_REFLECT_TABLE( CWeaponCSBase, DT_WeaponCSBase );

IMPLEMENT_REFLECT_PREDMAP( CWeaponCSBase );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_cs_base, WeaponCSBase );

	ConVar cl_crosshairstyle( side_registry(), "cl_crosshairstyle", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "0 = DEFAULT, 1 = DEFAULT STATIC, 2 = ACCURATE SPLIT (accurate recoil/spread feedback with a fixed inner part), 3 = ACCURATE DYNAMIC (accurate recoil/spread feedback), 4 = CLASSIC STATIC, 5 = OLD CS STYLE (fake recoil - inaccurate feedback)" );
	ConVar cl_crosshaircolor( side_registry(), "cl_crosshaircolor", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Set crosshair color as defined in game_options.consoles.txt" );
	//ConVar cl_dynamiccrosshair( "cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_scalecrosshair( side_registry(), "cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Enable crosshair scaling (deprecated)" );
	ConVar cl_crosshairscale( side_registry(), "cl_crosshairscale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Crosshair scaling factor (deprecated)" );
	ConVar cl_crosshairalpha( side_registry(), "cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairusealpha( side_registry(), "cl_crosshairusealpha", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairgap( side_registry(), "cl_crosshairgap", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairgap_useweaponvalue( side_registry(), "cl_crosshairgap_useweaponvalue", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If set to 1, the gap will update dynamically based on which weapon is currently equipped" );
	ConVar cl_crosshairsize( side_registry(), "cl_crosshairsize", "5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairthickness( side_registry(), "cl_crosshairthickness", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairdot( side_registry(), "cl_crosshairdot", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_r( side_registry(), "cl_crosshaircolor_r", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_g( side_registry(), "cl_crosshaircolor_g", "250", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_b( side_registry(), "cl_crosshaircolor_b", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar weapon_debug_spread_show( side_registry(), "weapon_debug_spread_show", "0", FCVAR_CLIENTDLL | FCVAR_SS | FCVAR_CHEAT, "Enables display of weapon accuracy; 1: show accuracy box, 3: show accuracy with dynamic crosshair" );
	ConVar weapon_debug_spread_gap( side_registry(), "weapon_debug_spread_gap", "0.67", FCVAR_CLIENTDLL  | FCVAR_SS | FCVAR_CHEAT );
	ConVar cl_crosshair_drawoutline( side_registry(), "cl_crosshair_drawoutline", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Draws a black outline around the crosshair for better visibility" );
	ConVar cl_crosshair_outlinethickness( side_registry(), "cl_crosshair_outlinethickness", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Set how thick you want your crosshair outline to draw (0.1-3)", true, 0.1, true, 3 );

	ConVar cl_crosshair_dynamic_splitdist(side_registry(), "cl_crosshair_dynamic_splitdist", "7", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the distance that the crosshair pips will split into 2. (default is 7)"/*, true, 10, true, 3*/);
	ConVar cl_crosshair_dynamic_splitalpha_innermod(side_registry(), "cl_crosshair_dynamic_splitalpha_innermod", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the alpha modification that will be used for the INNER crosshair pips once they've split. [0 - 1]", true, 0, true, 1);
	ConVar cl_crosshair_dynamic_splitalpha_outermod(side_registry(), "cl_crosshair_dynamic_splitalpha_outermod", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the alpha modification that will be used for the OUTER crosshair pips once they've split. [0.3 - 1]", true, 0.3, true, 1);
	ConVar cl_crosshair_dynamic_maxdist_splitratio(side_registry(), "cl_crosshair_dynamic_maxdist_splitratio", "0.35", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the ratio used to determine how long the inner and outer xhair pips will be. [inner = cl_crosshairsize*(1-cl_crosshair_dynamic_maxdist_splitratio), outer = cl_crosshairsize*cl_crosshair_dynamic_maxdist_splitratio]  [0 - 1]", true, 0, true, 1);

void DrawCrosshairRect( int r, int g, int b, int a, int x0, int y0, int x1, int y1, bool bAdditive )
{
}

// must be included after the above macros

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation.
// ----------------------------------------------------------------------------- //
CWeaponCSBase::CWeaponCSBase()
	: m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
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

	m_bOldFirstPersonSpectatedState = false;

	// This will make it so the weapons get lit with the same ambient cube that the player gets lit with.
	SetUseParentLightingOrigin( true );
	m_iCrosshairTextureID = 0;
	m_flGunAccuracyPosition = 0;

	m_bVisualsDataSet = false;

	m_flLastClientFireBulletTime = 0;

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

}

void CWeaponCSBase::ResetGunHeat()
{

	m_gunHeat = 0.0f;
	m_smokeAttachments = 0x0;
	m_lastSmokeTime = 0.0f;

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

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
CBaseEntity *CWeaponCSBase::GetWeaponForEffect()
{
	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return nullptr;

	bool bThird = false;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		if ( GetPlayerOwner()->GetPlayerRenderMode( nSlot ) == PLAYER_RENDER_THIRDPERSON )
		{
			bThird = true;
		}
	}

	if ( pLocalPlayer == GetPlayerOwner() && !bThird )
		return pLocalPlayer->GetViewModel();

	return this;
}

void CWeaponCSBase::UpdateOutlineGlow( void )
{
	Vector glowColor;
	bool bRender = false;
	if ( !GetPlayerOwner() )
	{
		bool bRenderForSpectator = CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt();
		if ( bRenderForSpectator && (GetWeaponType() == WEAPONTYPE_C4) )
		{
			glowColor.x = (240.0f/255.0f);
			glowColor.y = (225.0f/255.0f);
			glowColor.z = (90.0f/255.0f);
			bRender = true;
		}
		else if ( (m_iClip1 > 0 || m_iClip2 > 0) && mp_weapons_glow_on_ground.GetBool() )
		{
			glowColor.x = (224.0f/255.0f);
			glowColor.y = (224.0f/255.0f);
			glowColor.z = (224.0f/255.0f);
			bRender = true;
		}
	}

	// Start glowing
	m_GlowObject.SetRenderFlags( bRender, false );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( bRender ? 0.8f : 0.0f );
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

		if ( !bIsLookingAt )
		{
			//Fade down stat trak glow if we're doing anything other than inspecting
			vm->SetStatTrakGlowMultiplier( 0.0f );
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

	static const float SECONDS_FOR_COOL_DOWN = 2.0f;
	static const float MIN_TIME_BETWEEN_SMOKES = 4.0f;

	float currentShotTime = gpGlobals->curtime;
	float timeSinceLastShot = currentShotTime - m_fLastShotTime;

	// Drain off any heat from prior shots.
	m_gunHeat -= timeSinceLastShot * ( 1.0f/SECONDS_FOR_COOL_DOWN );
	if ( m_gunHeat <= 0.0f )
	{
		m_gunHeat = 0.0f;
	}

	// Add the new heat to the gun.
	m_gunHeat += heat;
	if ( m_gunHeat > 1.0f )
	{
		// Reset the heat so we have to build up to it again.
		m_gunHeat = 0.0f;
		m_smokeAttachments |= ( 0x1 << iAttachmentIndex );
	}

	// Logic for the gun smoke.
	if ( m_smokeAttachments != 0x0 )
	{
		// We don't want to hammer the smoke effect too much, so prevent smoke from spawning too soon after the last smoke.
		if ( currentShotTime - m_lastSmokeTime > MIN_TIME_BETWEEN_SMOKES )
		{
			const char *pszHeatEffect = GetCSWpnData().GetHeatEffectName();

			if ( pszHeatEffect && V_strlen( pszHeatEffect ) > 0 )
			{
				static const int MAX_SMOKE_ATTACHMENT_INDEX = 16;
				for ( int i=0; i<MAX_SMOKE_ATTACHMENT_INDEX && m_smokeAttachments > 0x0; ++i )
				{
					int attachmentFlag = ( 0x1<<i );
					if ( ( attachmentFlag & m_smokeAttachments ) > 0x0 )
					{
						//Remove the attachment flag from the smoke attachments since we are firing it off.
						m_smokeAttachments = ( m_smokeAttachments & ( ~attachmentFlag ) );

						CCSPlayer *pPlayer = GetPlayerOwner();
						if ( pPlayer )
						{
							CBaseViewModel *pViewModel = pPlayer->GetViewModel();
							if ( pViewModel )
							{
								// Dispatch this effect to the split screens that are rendering this first person view model.
								int nSlot = GetViewModelSplitScreenSlot( pViewModel );
								DispatchParticleEffect( pszHeatEffect, PATTACH_POINT_FOLLOW, pViewModel, i, false, nSlot );
								m_lastSmokeTime = currentShotTime;
							}
						}
					}
				}
			}
			//Reset the smoke attachments so that we can start doing a smoke effect for later shots.
			m_smokeAttachments = 0x0;
		}

	}

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

	if ( gpGlobals->curtime > m_flNextPrimaryAttack && m_iClip1 == 0 && IsUseable() && !( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) && !m_bInReload )
	{
		// Reload if current clip is empty and weapon has waited as long as it has to after firing
		Reload();
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

void CWeaponCSBase::Precache( void )
{
	BaseClass::Precache();

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

	m_iAlpha =  80;

	if ( cl_crosshairstyle.GetInt() == 4 || cl_crosshairstyle.GetInt() == 5 )
		m_flCrosshairDistance = 1;

	// Stop glowing
	//if ( m_nGlowIndex != -1 )
	//{
	//	g_GlowObjectManager.UnregisterGlowObject( m_nGlowIndex );
	//	m_nGlowIndex = -1;
	//}

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

ConVar mp_weapon_prev_owner_touch_time( side_registry(), "mp_weapon_prev_owner_touch_time", "1.5", FCVAR_CHEAT | FCVAR_REPLICATED );
void CWeaponCSBase::Drop( const Vector &vecVelocity )
{
	BaseClass::Drop( vecVelocity );

	CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
	if ( pHudSelection )
	{
		pHudSelection->OnWeaponDrop( this );
	}

	UpdateOutlineGlow();

	return;
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
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
int CWeaponCSBase::GetReticleWeaponSpread( void )
{
	int result = 0;
	CCSPlayer* pPlayer = ( CCSPlayer* )CBasePlayer::GetLocalPlayer();

	if ( pPlayer )
	{
		if ( GetWeaponType() == WEAPONTYPE_GRENADE ||  GetCSWeaponID() == WEAPON_C4 || GetCSWeaponID() == WEAPON_KNIFE || GetCSWeaponID() == WEAPON_KNIFE_GG )
		{
			return -1; //early out since we don't want a gap and don't want to change the sentinel value with the math later.
		}

		float fHalfFov = DEG2RAD( pPlayer->GetFOV() ) * 0.5f;
		float fSpreadDistance = ( GetInaccuracy() + GetSpread() ) * 320.0f / std::tanf( fHalfFov );
		result = RoundFloatToInt( YRES( fSpreadDistance ));
	}

	return result;

}

int CWeaponCSBase::GetReticleCrosshairGap( void )
{
	int result = 0;

	CCSPlayer* pPlayer = ( CCSPlayer* )CBasePlayer::GetLocalPlayer();

	if ( pPlayer )
	{
		int crosshairGap;

		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
			crosshairGap = 30;
		else if ( pPlayer->GetFlags() & FL_DUCKING )
			crosshairGap = 3;
		else if ( pPlayer->GetAbsVelocity().Length() > 100 )
			crosshairGap = 20;
		else
			crosshairGap = 10;

		result = ScreenHeight() * crosshairGap / 768;
	}

	return result;
}

bool CWeaponCSBase::WantReticleShown( void )
{
	CCSPlayer* pPlayer = ( CCSPlayer* )CBasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	if ( pPlayer->HasShield() && pPlayer->IsShieldDrawn() == true )
		return false;

	bool isAKnife = ( GetWeaponType() == WEAPONTYPE_KNIFE && !IsA( WEAPON_TASER ) );

	bool isUsingMotionController = PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() );

	// we hide the sniper Reticle UNLESS we're using a motion controller AND we're not zoomed
	bool hideSniperReticle = GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE &&
							 (pPlayer->m_bIsScoped || !isUsingMotionController);

	// no crosshair for sniper rifles
	if ( hideSniperReticle ||
		 ( GetWeaponType() == WEAPONTYPE_C4 ) ||
		 ( isAKnife && !weapon_reticle_knife_show.GetBool() ) )
		return false;

	return true;
}

ConVar cl_cam_driver_compensation_scale( side_registry(), "cl_cam_driver_compensation_scale", "0.75", FCVAR_RELEASE, "" );

void CWeaponCSBase::DrawCrosshair()
{
	if ( !crosshair.GetInt() )
		return;

	CCSPlayer* pPlayer = ( CCSPlayer* )CBasePlayer::GetLocalPlayer();

	if ( !pPlayer || GetPlayerOwner() == nullptr )
		return;

	contract_assert( (pPlayer == GetPlayerOwner()) || ( pPlayer->GetObserverMode()==OBS_MODE_IN_EYE) );
	if ( pPlayer->IsInVGuiInputMode() )
		return;

	if ( pPlayer->GetObserverInterpState() == CCSPlayer::OBSERVER_INTERP_TRAVELING )
		return;

	// no crosshair for sniper rifles

	// localplayer must be owner if not in Spec mode

	int	r, g, b;
	switch ( cl_crosshaircolor.GetInt() )
	{
	case 0 :	r = 250;	g = 50;		b = 50;		break;
	case 1 :	r = 50;		g = 250;	b = 50;		break;
	case 2 :	r = 250;	g = 250;	b = 50;		break;
	case 3 :	r = 50;		g = 50;		b = 250;	break;
	case 4 :	r = 50;		g = 250;	b = 250;	break;
	case 5 :
		r = cl_crosshaircolor_r.GetInt();
		g = cl_crosshaircolor_g.GetInt();
		b = cl_crosshaircolor_b.GetInt();
		break;
	default :	r = 50;		g = 250;	b = 50;		break;
	}

	// if user is using nightvision, make the crosshair red.
	if ( pPlayer->m_bNightVisionOn )
	{
		r = 250;
		g = 50;
		b = 50;
	}

	int alpha = clamp( cl_crosshairalpha.GetInt(), 0, 255 );

	if ( !m_iCrosshairTextureID )
	{
		CHudTexture *pTexture = HudIcons().GetIcon( "whiteAdditive" );
		if ( pTexture )
		{
			m_iCrosshairTextureID = pTexture->textureId;
		}
	}

	bool bAdditive = !cl_crosshairusealpha.GetBool() && !pPlayer->m_bNightVisionOn;
	if ( bAdditive )
	{
		alpha = 200;
	}

	if ( GetIronSightController() && !weapon_debug_spread_show.GetBool() && GetIronSightController()->ShouldHideCrossHair() )
	{
		alpha = 0;
	}

	if ( pPlayer->HasShield() && pPlayer->IsShieldDrawn() == true )
		return;
	if ( GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE && !weapon_debug_spread_show.GetBool() )
		return;
	float fHalfFov = DEG2RAD( pPlayer->GetFOV() ) * 0.5f;
	float flInaccuracy = GetInaccuracy();
	float flSpread = GetSpread();

//	float flCrossDistanceScale = cl_crosshair_dynamic_distancescale.GetFloat();
	float fSpreadDistance = ((flInaccuracy + flSpread) * 320.0f / std::tanf(fHalfFov))/* * flCrossDistanceScale*/;
	float flCappedSpreadDistance = fSpreadDistance;
	float flMaxCrossDistance = cl_crosshair_dynamic_splitdist.GetFloat();
 	if (fSpreadDistance > flMaxCrossDistance)
 	{
// 		bHitMax = true;
// 		//flLineAlpha = alpha * std::max(0, 1 - ((fSpreadDistance - flMaxCrossDistance) / 20));
 		flCappedSpreadDistance = flMaxCrossDistance;
 	}

	int iSpreadDistance = cl_crosshairstyle.GetInt() < 4 ? RoundFloatToInt( YRES( fSpreadDistance )) : 2;
	int iCappedSpreadDistance = cl_crosshairstyle.GetInt() < 4 ? RoundFloatToInt(YRES(flCappedSpreadDistance)) : 2;

	float flAccuracyFishtail = GetAccuracyFishtail();

	if (cl_weapon_debug_print_accuracy.GetInt() == 1)
	{
		float flVel = pPlayer->GetLocalVelocity().Length2D();
		if (flVel > 0)
			Msg("Inaccuracy =\t%f\tSpread =\t%f\tSpreadDistance =\t%f\tPlayer Velocity =\t%f\n", flInaccuracy, flSpread, fSpreadDistance, flVel);
	}

	int iDeltaDistance = GetCSWpnData().GetCrosshairDeltaDistance( GetEconItemView() ); // Amount by which the crosshair expands when shooting ( per frame )
	float fCrosshairDistanceGoal = cl_crosshairgap_useweaponvalue.GetBool() ? GetCSWpnData().GetCrosshairMinDistance( GetEconItemView() ) : 4; // The minimum distance the crosshair can achieve...

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	//if ( cl_dynamiccrosshair.GetBool() )
	if ( cl_crosshairstyle.GetInt() != 4 && (cl_crosshairstyle.GetInt() == 2 || cl_crosshairstyle.GetInt() == 3) )
	{
		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
			fCrosshairDistanceGoal *= 2.0f;
		else if ( pPlayer->GetFlags() & FL_DUCKING )
			fCrosshairDistanceGoal *= 0.5f;
		else if ( pPlayer->GetAbsVelocity().Length() > 100 )
			fCrosshairDistanceGoal *= 1.5f;
	}

	// [jpaquin] changed to only bump up the crosshair size if the player is still shooting or is spectating someone else
	if ( pPlayer->m_iShotsFired > m_iAmmoLastCheck && ( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2 )) && m_iClip1 >= 0 )
	{
		if ( cl_crosshairstyle.GetInt() == 5 )
			m_flCrosshairDistance += (GetRecoilMagnitude( m_weaponMode ) / 3.5);
		else if ( cl_crosshairstyle.GetInt() != 4 )
			fCrosshairDistanceGoal += iDeltaDistance;
	}

	m_iAmmoLastCheck = pPlayer->m_iShotsFired;

	if ( m_flCrosshairDistance > fCrosshairDistanceGoal )
	{
		if ( cl_crosshairstyle.GetInt() == 5 )
		{
			//float flPer = ( gpGlobals->frametime / 0.025f );
			//m_flCrosshairDistance -= (0.11 + ( m_flCrosshairDistance * 0.01 )) * gpGlobals->frametime;
			m_flCrosshairDistance -= 42.0 * gpGlobals->frametime;
		}
		else
			m_flCrosshairDistance = Lerp( ( gpGlobals->frametime / 0.025f ), fCrosshairDistanceGoal, m_flCrosshairDistance );
	}

	// clamp max crosshair expansion
	m_flCrosshairDistance = clamp( m_flCrosshairDistance, fCrosshairDistanceGoal, 25.0f );

	int iCrosshairDistance;
	int iCappedCrosshairDistance = 0;
	int iBarSize;
	int iBarThickness;

	iCrosshairDistance = RoundFloatToInt((m_flCrosshairDistance * ScreenHeight() / 1200.0f) + cl_crosshairgap.GetFloat());
	iBarSize = RoundFloatToInt( YRES( cl_crosshairsize.GetFloat() ));
	iBarThickness = std::max( 1, RoundFloatToInt( YRES( cl_crosshairthickness.GetFloat() )) );

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	//if ( weapon_debug_spread_show.GetInt() == 2 )
	if ( weapon_debug_spread_show.GetInt( ) == 2 || (iSpreadDistance > 0 && ( cl_crosshairstyle.GetInt( ) == 2 || cl_crosshairstyle.GetInt( ) == 3 ) ) )
	{
		iCrosshairDistance = iSpreadDistance + cl_crosshairgap.GetFloat();

		if ( cl_crosshairstyle.GetInt( ) == 2 )
			iCappedCrosshairDistance = iCappedSpreadDistance + cl_crosshairgap.GetFloat();
	}
	else if ( cl_crosshairstyle.GetInt( ) == 4 || ( iSpreadDistance == 0 && ( cl_crosshairstyle.GetInt( ) == 2 || cl_crosshairstyle.GetInt( ) == 3 ) ) )
	{
		iCrosshairDistance = fCrosshairDistanceGoal + cl_crosshairgap.GetFloat();
		iCappedCrosshairDistance = 4 + cl_crosshairgap.GetFloat();
	}

	int iCenterX = ScreenWidth() / 2;
	int iCenterY = ScreenHeight() / 2;

	float flAngleToScreenPixel = 0;

	const float VIEWPUNCH_COMPENSATE_MAGIC_SCALAR = 0.65f; // cl_flinch_scale.GetFloat()

	// subtract a ratio of cam driver motion from crosshair according to cl_cam_driver_compensation_scale
	if ( cl_cam_driver_compensation_scale.GetFloat() != 0 )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm && vm->m_flCamDriverWeight > 0 )
			{
				QAngle angCamDriver = vm->m_flCamDriverWeight * vm->m_angCamDriverLastAng * clamp( cl_cam_driver_compensation_scale.GetFloat(), -10.0f, 10.0f );
				if ( angCamDriver.x != 0 || angCamDriver.y != 0  )
				{
					flAngleToScreenPixel = VIEWPUNCH_COMPENSATE_MAGIC_SCALAR * 2 * ( ScreenHeight() / ( 2.0f * tanf(DEG2RAD( pPlayer->GetFOV() ) / 2.0f) ) );
					iCenterY -= ( flAngleToScreenPixel * std::sinf( DEG2RAD( angCamDriver.x ) ) ) ;
					iCenterX += ( flAngleToScreenPixel * sinf( DEG2RAD( angCamDriver.y ) ) ) ;
				}
			}
		}
	}

/*
	// Optionally subtract out viewangle since it doesn't affect shooting.
	if ( cl_flinch_compensate_crosshair.GetBool() )
	{
		QAngle viewPunch = pPlayer->GetViewPunchAngle();

		if ( viewPunch.x != 0 || viewPunch.y != 0 )
		{
			if ( flAngleToScreenPixel == 0 )
				flAngleToScreenPixel = VIEWPUNCH_COMPENSATE_MAGIC_SCALAR * 2 * ( ScreenHeight() / ( 2.0f * tanf(DEG2RAD( pPlayer->GetFOV() ) / 2.0f) ) );

			iCenterY -= flAngleToScreenPixel * sinf( DEG2RAD( viewPunch.x ) );
			iCenterX += flAngleToScreenPixel * sinf( DEG2RAD( viewPunch.y ) );
		}
	}
*/

	int nFishTaleShift = ( flAccuracyFishtail * ( ScreenHeight() / 500.0f ) );

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	if ( cl_crosshairstyle.GetInt() == 4 )
	{
		nFishTaleShift = 0;
	}

	float flAlphaSplitInner = cl_crosshair_dynamic_splitalpha_innermod.GetFloat();
	float flAlphaSplitOuter = cl_crosshair_dynamic_splitalpha_outermod.GetFloat();
	float flSplitRatio = cl_crosshair_dynamic_maxdist_splitratio.GetFloat();
	int iInnerCrossDist = iCrosshairDistance;
	float flLineAlphaInner = alpha;
	float flLineAlphaOuter = alpha;
	int iBarSizeInner = iBarSize;
	int iBarSizeOuter = iBarSize;

	// draw the crosshair that splits off from the main xhair
	if (cl_crosshairstyle.GetInt() == 2 && fSpreadDistance > flMaxCrossDistance)
	{
		iInnerCrossDist = iCappedCrosshairDistance;
		flLineAlphaInner = alpha * flAlphaSplitInner;
		flLineAlphaOuter = alpha * flAlphaSplitOuter;
		iBarSizeInner = std::ceil((float)iBarSize * (1.0f - flSplitRatio));
		iBarSizeOuter = std::floor((float)iBarSize * flSplitRatio);

		// draw horizontal crosshair lines
		int iInnerLeft = (iCenterX - iCrosshairDistance - iBarThickness / 2 + nFishTaleShift) - iBarSizeInner;
		int iInnerRight = iInnerLeft + 2 * (iCrosshairDistance + iBarSizeInner) + iBarThickness + nFishTaleShift;
		int iOuterLeft = iInnerLeft - iBarSizeOuter;
		int iOuterRight = iInnerRight + iBarSizeOuter;
		int y0 = iCenterY - iBarThickness / 2;
		int y1 = y0 + iBarThickness;
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, iOuterLeft, y0, iInnerLeft, y1, bAdditive);
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, iInnerRight, y0, iOuterRight, y1, bAdditive);

		// draw vertical crosshair lines
		int iInnerTop = (iCenterY - iCrosshairDistance - iBarThickness / 2) - iBarSizeInner;
		int iInnerBottom = iInnerTop + 2 * (iCrosshairDistance + iBarSizeInner) + iBarThickness;
		int iOuterTop = iInnerTop - iBarSizeOuter;
		int iOuterBottom = iInnerBottom + iBarSizeOuter;
		int x0 = iCenterX - iBarThickness / 2;
		int x1 = x0 + iBarThickness;
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, x0, iOuterTop, x1, iInnerTop, bAdditive);
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, x0, iInnerBottom, x1, iOuterBottom, bAdditive);
	}

	// draw horizontal crosshair lines
	int iInnerLeft = iCenterX - iInnerCrossDist - iBarThickness / 2 + nFishTaleShift;
	int iInnerRight = iInnerLeft + 2 * iInnerCrossDist + iBarThickness + nFishTaleShift;
	int iOuterLeft = iInnerLeft - iBarSizeInner;
	int iOuterRight = iInnerRight + iBarSizeInner;
	int y0 = iCenterY - iBarThickness / 2;
	int y1 = y0 + iBarThickness;
	DrawCrosshairRect(r, g, b, flLineAlphaInner, iOuterLeft, y0, iInnerLeft, y1, bAdditive);
	DrawCrosshairRect(r, g, b, flLineAlphaInner, iInnerRight, y0, iOuterRight, y1, bAdditive);

	// draw vertical crosshair lines
	int iInnerTop = iCenterY - iInnerCrossDist - iBarThickness / 2;
	int iInnerBottom = iInnerTop + 2 * iInnerCrossDist + iBarThickness;
	int iOuterTop = iInnerTop - iBarSizeInner;
	int iOuterBottom = iInnerBottom + iBarSizeInner;
	int x0 = iCenterX - iBarThickness / 2;
	int x1 = x0 + iBarThickness;
	DrawCrosshairRect(r, g, b, flLineAlphaInner, x0, iOuterTop, x1, iInnerTop, bAdditive);
	DrawCrosshairRect(r, g, b, flLineAlphaInner, x0, iInnerBottom, x1, iOuterBottom, bAdditive);

	// draw dot
	if ( cl_crosshairdot.GetBool() )
	{
		int x0 = iCenterX - iBarThickness / 2;
		int x1 = x0 + iBarThickness;
		int y0 = iCenterY - iBarThickness / 2;
		int y1 = y0 + iBarThickness;
		DrawCrosshairRect( r, g, b, alpha, x0, y0, x1, y1, bAdditive );
	}

	if ( weapon_debug_spread_show.GetInt() == 1 )
	{
		// colors
		r = 250;
		g = 250;
		b = 50;

		int iBarThickness = std::max( 1, RoundFloatToInt( YRES( cl_crosshairthickness.GetFloat() )) );

		// draw vertical spread lines
		int iInnerLeft	= iCenterX - iSpreadDistance;
		int iInnerRight	= iCenterX + iSpreadDistance;
		int iOuterLeft	= iInnerLeft - iBarThickness;
		int iOuterRight	= iInnerRight + iBarThickness;
		int iInnerTop		= iCenterY - iSpreadDistance;
		int iInnerBottom	= iCenterY + iSpreadDistance;
		int iOuterTop		= iInnerTop - iBarThickness;
		int iOuterBottom	= iInnerBottom + iBarThickness;

		int iGap = RoundFloatToInt( weapon_debug_spread_gap.GetFloat() * iSpreadDistance );

		// draw horizontal lines
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iOuterTop, iCenterX - iGap, iInnerTop, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iCenterX + iGap, iOuterTop, iOuterRight, iInnerTop, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iInnerBottom, iCenterX - iGap, iOuterBottom, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iCenterX + iGap, iInnerBottom, iOuterRight, iOuterBottom, bAdditive );

		// draw vertical lines
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iOuterTop, iInnerLeft, iCenterY - iGap, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iCenterY + iGap, iInnerLeft, iOuterBottom, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iInnerRight, iOuterTop, iOuterRight, iCenterY - iGap, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iInnerRight, iCenterY + iGap, iOuterRight, iOuterBottom, bAdditive );
	}

}

void CWeaponCSBase::OnDataChanged( DataUpdateType_t type )
{
	if ( type == DATA_UPDATE_CREATED )
	{
		m_nWeaponID = WeaponIdFromString( GetClassname() );
	}

	// Sync econ item view from networked def index
	if ( m_nEconItemDefIndex > 0 && m_EconItemView.GetItemIndex() != (uint16)(int)m_nEconItemDefIndex )
	{
		m_EconItemView.Init( (uint16)(int)m_nEconItemDefIndex );
	}

	bool bChangedCarryState = false;
	if ( ( m_iState != WEAPON_NOT_CARRIED && m_iOldState == WEAPON_NOT_CARRIED ) ||
	     ( m_iState == WEAPON_NOT_CARRIED && m_iOldState != WEAPON_NOT_CARRIED ) )
	{
		bChangedCarryState = true;
	}

	CBaseCombatCharacter *pOwner = GetPreviousOwner();

	if ( GetWeaponType() == WEAPONTYPE_GRENADE )
	{
		pOwner = ( (CBaseGrenade *) this )->GetThrower();
	}

	if ( pOwner )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pOwner );
		CCSPlayer *pObservedPlayer = GetHudPlayer();

		// check if weapon was dropped by local player or the player we are observing
		if ( pObservedPlayer == pPlayer )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( CBasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

			if ( m_iState == WEAPON_NOT_CARRIED && m_iOldState != WEAPON_NOT_CARRIED )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponDrop( this );
				}
			}
		}
	}

	BaseClass::OnDataChanged( type );

	bool bFirstPersonSpectatedState = IsFirstPersonSpectated();
	if ( ( bFirstPersonSpectatedState && !m_bOldFirstPersonSpectatedState ) ||
		 ( !bFirstPersonSpectatedState && m_bOldFirstPersonSpectatedState ) )
	{
		bChangedCarryState = true;
	}

	if ( type == DATA_UPDATE_CREATED )
	{
		// this will trigger the custom material to start making itself (if needed) the weapon will render with
		// the original material for a few frames, then switch to the custom material when it's ready
		UpdateCustomMaterial();
		UpdateOutlineGlow();
	}
	else if ( bChangedCarryState )
	{
		CheckCustomMaterial();
		UpdateOutlineGlow();
	}

	UpdateIronSightController();

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}

bool CWeaponCSBase::ShouldPredict()
{
	if ( CBasePlayer::IsLocalPlayer( GetOwner() ) )
		return true;

	return BaseClass::ShouldPredict();
}

void CWeaponCSBase::ProcessMuzzleFlashEvent()
{
	// This is handled from the player's animstate, so it can match up to the beginning of the fire animation
}

int CWeaponCSBase::GetViewModelSplitScreenSlot( CBaseViewModel *pViewModel )
{
	uint32 visBits = ks::get_word( pViewModel->m_VisibilityBits, 0 ) & 0xff;
	int nSlot = -1;

	if ( visBits < 3 )
	{
		// If visBits is 3 then it is both slots 1 and 2 meaning all users can see this viewmodel (as indicated via -1).
		// If visBits is 1 or 2 then the view model is only seen in that specific slot either slot (but zero based means slot 0 or 1).
		// If visBits is 0, then split screen is not active so we should not be drawing this view model at all.  Currently not handled here but should be handled elsewhere.
		nSlot = visBits-1;
	}
	return nSlot;
}

bool CWeaponCSBase::OnFireEvent( CBaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	int nSlot = GetViewModelSplitScreenSlot( pViewModel );

	if( event == 5001 )
	{
		CCSPlayer *pPlayer = ToCSPlayer( GetOwner() );

		if ( !pPlayer )
			return true;

		if( pPlayer && pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped && DoesHideViewModelWhenZoomed() )
			return true;

		// hide particle effects when we're interpolating between observer targets
		CBasePlayer* pLocalPlayer = CBasePlayer::GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetObserverTarget() == pPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverInterpState() == CBasePlayer::OBSERVER_INTERP_TRAVELING )
			return true;

		bool bLocalThirdPerson = ( (pPlayer == pLocalPlayer) && pPlayer->ShouldDraw() );

		Vector origin;
		int iAttachmentIndex = GetMuzzleAttachmentIndex_1stPerson( pViewModel );
		const char *pszEffect = GetMuzzleFlashEffectName_1stPerson();

		if ( pszEffect && V_strlen( pszEffect ) > 0 && iAttachmentIndex >= 0 )
		{
			// The view model fixes up the split screen visibility of any effects spawned off of it.
			if ( !bLocalThirdPerson )
				DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pViewModel, iAttachmentIndex, false, nSlot );

			// we can't trust this position
			//pViewModel->GetAttachment( iAttachmentIndex, origin );

			//silencers produce no light at all - even smaller lights would illuminate smoke or cause unwanted visual effects
			if ( !(HasSilencer() && IsSilenced()) )
			{
				CPVSFilter filter( origin );
				origin = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( pPlayer );
				QAngle	vangles;
				Vector	vforward, vright, vup;
				engine->GetViewAngles( vangles );
				AngleVectors( vangles, &vforward, &vright, &vup );
				VectorMA( origin, cl_righthand.GetBool() ? 4 : -4, vright, origin );
				VectorMA( origin, 31, vforward, origin );
				origin[2] += 3.0f;

				TE_DynamicLight( filter, 0.0, &origin, 255, 186, 64, 5, 70, 0.05, 768 );
			}

			UpdateGunHeat( GetCSWpnData().GetHeatPerShot(), iAttachmentIndex  );
		}

		return true;
	}
	else if ( event == AE_CLIENT_EJECT_BRASS )
	{
		CCSPlayer *pPlayer = ToCSPlayer( GetOwner() );
		if( pPlayer && pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped && DoesHideViewModelWhenZoomed() )
			return true;

		Vector origin;
		const char *pszEffect = GetEjectBrassEffectName();
		int iAttachmentIndex = -1;

		// If options is non-zero in length, treat as an attachment name to use for this particle effect.
		if ( options && V_strlen( options ) > 0 )
		{
			iAttachmentIndex = pViewModel->LookupAttachment( options );
		}
		else
		{
			iAttachmentIndex = GetEjectBrassAttachmentIndex_1stPerson( pViewModel );
		}

		if ( pszEffect && V_strlen( pszEffect ) > 0 && iAttachmentIndex >= 0 )
		{

			CBasePlayer* pLocalPlayer = CBasePlayer::GetLocalPlayer();
			bool bLocalThirdPerson = ( (pPlayer == pLocalPlayer) && pPlayer->ShouldDraw() );

			// The view model fixes up the split screen visibility of any effects spawned off of it.
			if ( !bLocalThirdPerson )
				DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pViewModel, iAttachmentIndex, false, nSlot );
		}

		return true;
	}
	else if ( event == AE_CL_SET_STATTRAK_GLOW )
	{
		pViewModel->SetStatTrakGlowMultiplier( atof( options ) );
		return true;
	}
	else if ( event == AE_WPN_NEXTCLIP_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the clip amount after an impending reload
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				int iNextClip = std::min( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
				vm->SetPoseParameter( options, 1.0f - (((float)iNextClip) / ((float)GetMaxClip1())) );
			}
		}
		return true;
	}
	else if ( event == AE_WPN_CLIP_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the clip amount
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				vm->SetPoseParameter( options, 1.0f - (((float)m_iClip1) / ((float)GetMaxClip1())) );
			}
		}
		return true;
	}
	else if ( event == AE_WPN_EMPTYSHOTS_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the number of empty shots
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				vm->SetPoseParameter( options, std::fmod( m_iNumEmptyAttacks, (float)GetMaxClip1() ) / (float)GetMaxClip1() );
			}
		}
		return true;
	}

	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
}

const char* CWeaponCSBase::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBase::GetHeatEffectName( void )
{
	return GetCSWpnData().GetHeatEffectName( GetEconItemView() );
}

const char* CWeaponCSBase::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBase::GetEjectBrassEffectName( void )
{
	return GetCSWpnData().GetEjectBrassEffectName( GetEconItemView() );
}

int CWeaponCSBase::GetMuzzleAttachmentIndex_1stPerson( CBaseViewModel *pViewModel )
{
	if ( pViewModel )
	{
		if ( HasSilencer() && IsSilenced() )
		{
			return pViewModel->LookupAttachment( "muzzle_flash2" );
		}
		return pViewModel->LookupAttachment( "1" );
	}
	return -1;
}

int CWeaponCSBase::GetMuzzleAttachmentIndex_3rdPerson( void )
{
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	if ( pWeaponWorldModel )
	{
		if ( HasSilencer() && IsSilenced() )
		{
			return pWeaponWorldModel->LookupAttachment( "muzzle_flash2" );
		}
		return pWeaponWorldModel->LookupAttachment( "muzzle_flash" );
	}
	return -1;
}

int CWeaponCSBase::GetEjectBrassAttachmentIndex_1stPerson( CBaseViewModel *pViewModel )
{
	if ( pViewModel )
		return pViewModel->LookupAttachment( "2" );

	return -1;
}

int CWeaponCSBase::GetEjectBrassAttachmentIndex_3rdPerson( void )
{
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();

	if ( pWeaponWorldModel )
		return pWeaponWorldModel->LookupAttachment( "shell_eject" );

	return -1;
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

float	g_lateralBob = 0;
float	g_verticalBob = 0;

static ConVar	cl_bob_version( side_registry(), "cl_bob_version","0", FCVAR_CHEAT );
static ConVar	cl_bobcycle( side_registry(), "cl_bobcycle","0.98", FCVAR_ARCHIVE, "the frequency at which the viewmodel bobs.", true, 0.1, true, 2.0 );
//static ConVar	cl_bob( "cl_bob","0.002", FCVAR_ARCHIVE );
static ConVar	cl_bobup( side_registry(), "cl_bobup","0.5", FCVAR_CHEAT );

ConVar	cl_use_new_headbob( side_registry(), "cl_use_new_headbob","1", FCVAR_CHEAT );
static ConVar	cl_bobamt_vert( side_registry(), "cl_bobamt_vert","0.25", FCVAR_ARCHIVE, "The amount the viewmodel moves up and down when running", true, 0.1, true, 2 );
static ConVar	cl_bobamt_lat( side_registry(), "cl_bobamt_lat","0.4", FCVAR_ARCHIVE, "The amount the viewmodel moves side to side when running", true, 0.1, true, 2 );
static ConVar	cl_bob_lower_amt( side_registry(), "cl_bob_lower_amt","21", FCVAR_ARCHIVE, "The amount the viewmodel lowers when running", true, 5, true, 30 );

static ConVar	cl_viewmodel_shift_left_amt( side_registry(), "cl_viewmodel_shift_left_amt","1.5", FCVAR_ARCHIVE, "The amount the viewmodel shifts to the left when shooting accuracy increases.", true, 0.5, true, 2.0 );
static ConVar	cl_viewmodel_shift_right_amt( side_registry(), "cl_viewmodel_shift_right_amt","0.75", FCVAR_ARCHIVE, "The amount the viewmodel shifts to the right when shooting accuracy decreases.", true, 0.25, true, 2.0 );

//-----------------------------------------------------------------------------
// Purpose: Helper function to calculate head bob
//-----------------------------------------------------------------------------
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState, int nVMIndex )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return 0;

	contract_assert( pBobState );
	if ( !pBobState )
		return 0;

	float	cycle;

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == nullptr ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();

	// do the dip before speed gets clamped
	float flSpeedFactor;

	float flRunAddAmt = 0.0f;

	float flmaxSpeedDelta = std::max<std::common_type_t<int, float>>( 0, (gpGlobals->curtime - pBobState->m_flLastBobTime ) * 640.0f );//320

//	Msg( "speed raw = %f\n", speed );

	// don't allow too big speed changes
	speed = clamp( speed, pBobState->m_flLastSpeed-flmaxSpeedDelta, pBobState->m_flLastSpeed+flmaxSpeedDelta );
	speed = clamp( speed, -320.0f, 320.0f );

	pBobState->m_flLastSpeed = speed;

	CBaseViewModel *pViewModel = player ? player->GetViewModel(nVMIndex) : nullptr;
	bool bShouldIgnoreOffsetAndAccuracy = (pViewModel && pViewModel->m_bShouldIgnoreOffsetAndAccuracy);

	// when the player is moving forward, the gun lowers a bit -mtw
	CCSPlayer *pPlayer = ToCSPlayer( player );
	if ( pPlayer && !pPlayer->IsZoomed() )
	{
		flSpeedFactor = speed * 0.006f;
		clamp( flSpeedFactor, 0.0f, 0.5f );
		float flLowerAmt = cl_bob_lower_amt.GetFloat() * 0.2;

		// HACK until we get 'ACT_VM_IDLE_LOWERED' anims
		//if ( pPlayer->m_Shared.IsSprinting() )
			//flLowerAmt *= 10.0f;

		if ( bShouldIgnoreOffsetAndAccuracy )
			flLowerAmt *= 0.1;

		flRunAddAmt = ( flLowerAmt * flSpeedFactor );
	}

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	float bob_offset = RemapVal( speed, 0.0f, 320.0f, 0.0f, 1.0f );

	pBobState->m_flBobTime += ( gpGlobals->curtime - pBobState->m_flLastBobTime ) * bob_offset;
	pBobState->m_flLastBobTime = gpGlobals->curtime;

	//Msg( "speed = %f\n", speed );

	float flBobCycle = 0.5f;
	float flAccuracyDiff = 0;
	float flGunAccPos = 0;

	CWeaponCSBase *pWeapon = ( ( bShouldIgnoreOffsetAndAccuracy || !pPlayer ) ? nullptr : pPlayer->GetActiveCSWeapon() );
	if ( pPlayer && pWeapon )
	{
		float flMaxSpeed = pWeapon->GetMaxSpeed();
		flBobCycle = (((1000.0f - flMaxSpeed) / 3.5f) * 0.001f) * cl_bobcycle.GetFloat();

		float flAccuracy = 0.0f;

		bool bIsElites = ( pWeapon->GetCSWeaponID() == WEAPON_ELITE ) ? true : false;

		// don't move the gun around based on accuracy when reloading
		if ( !pWeapon->m_bInReload && !bIsElites )
		{
			// move the gun left or right based on the players current accuracy
			float flCrouchAccuracy = pWeapon->GetInaccuracyCrouch();
			float flBaseAccuracy = pWeapon->GetInaccuracyStand();
			if ( pPlayer->m_Local.m_bDucking || pPlayer->m_Local.m_bDucked )
				flAccuracy = flCrouchAccuracy;
			else
				flAccuracy = pWeapon->GetInaccuracy();

			bool bIsSniper = ( pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE ) ? true : false;

			float flMultiplier = 1;
			if ( flAccuracy < flBaseAccuracy )
			{
				if ( !bIsSniper )
					flMultiplier = 18;
				else
					flMultiplier = 0.15;

				flMultiplier *= cl_viewmodel_shift_left_amt.GetFloat();
			}
			else
			{
				flAccuracy = std::min( flAccuracy, 0.082f );
				flMultiplier *= cl_viewmodel_shift_right_amt.GetFloat();
			}

			// clamp the crouch accuracy
			flAccuracyDiff = std::max<std::common_type_t<float, double>>( (flAccuracy - flBaseAccuracy) * flMultiplier, -0.1);
		}

/*
		if ( pWeapon->m_flGunAccuracyPosition > 0.0f )
			Msg( "m_flGunAccuracyPosition1 = %f, flAccuracy = %f\n", pWeapon->m_flGunAccuracyPosition, flAccuracy );

		pWeapon->m_flGunAccuracyPosition = Lerp( ( gpGlobals->frametime / 0.02f ), (flAccuracyDiff*20), pWeapon->m_flGunAccuracyPosition );

		if ( pWeapon->m_flGunAccuracyPosition > 0.0f )
			Msg( "m_flGunAccuracyPosition1 = %f, flAccuracy = %f\n", pWeapon->m_flGunAccuracyPosition, flAccuracy );
*/
		pWeapon->m_flGunAccuracyPosition = Approach( (flAccuracyDiff*80), pWeapon->m_flGunAccuracyPosition, abs( ( (flAccuracyDiff*80)-pWeapon->m_flGunAccuracyPosition )*gpGlobals->frametime ) * 4.0f );
		flGunAccPos = pWeapon->m_flGunAccuracyPosition;
	}
	else
	{
		flBobCycle = (((1000.0f - 150) / 3.5f) * 0.001f) * cl_bobcycle.GetFloat();
	}

	//Msg( "flBobCycle = %f\n", flBobCycle );
	//Calculate the vertical bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/flBobCycle)*flBobCycle;
	cycle /= flBobCycle;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = std::numbers::pi * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = std::numbers::pi + std::numbers::pi*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}
	float flBobMultiplier = 0.00625f;
	// if we're in the air, slow our bob down a bit
	if ( player->GetGroundEntity() == nullptr )
		flBobMultiplier = 0.00125f;

	float flBobVert = bShouldIgnoreOffsetAndAccuracy ? 0.3 : cl_bobamt_vert.GetFloat();
	pBobState->m_flVerticalBob = speed*( flBobMultiplier * flBobVert );
	pBobState->m_flVerticalBob = ( pBobState->m_flVerticalBob*0.3 + pBobState->m_flVerticalBob*0.7*std::sin(cycle) );
	pBobState->m_flRawVerticalBob = pBobState->m_flVerticalBob;

	pBobState->m_flVerticalBob = clamp( ( pBobState->m_flVerticalBob - ( flRunAddAmt - (flGunAccPos*0.2) ) ), -7.0f, 4.0f );

	//Msg( "pBobState->m_flVerticalBob = %f, cycle = %f, pBobState->m_flBobTime = %f\n", pBobState->m_flVerticalBob, cycle, pBobState->m_flBobTime );

	//Calculate the lateral bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/flBobCycle*2)*flBobCycle*2;
	cycle /= flBobCycle*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = std::numbers::pi * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = std::numbers::pi + std::numbers::pi*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	float flBobLat = bShouldIgnoreOffsetAndAccuracy ? 0.5 : cl_bobamt_lat.GetFloat();
	if ( pPlayer && pWeapon )
	{
		pBobState->m_flLateralBob = speed*( flBobMultiplier * flBobLat );
		pBobState->m_flLateralBob = pBobState->m_flLateralBob*0.3 + pBobState->m_flLateralBob*0.7*sin(cycle);
		pBobState->m_flRawLateralBob = pBobState->m_flLateralBob;

		pBobState->m_flLateralBob = clamp( (pBobState->m_flLateralBob + flGunAccPos*0.25), -8.0f, 8.0f );
		//Msg( "flAccuracyDiff = %f\n", flAccuracyDiff );
	}

	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to add head bob
//-----------------------------------------------------------------------------
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return;

	contract_assert( pBobState );
	if ( !pBobState )
		return;

	Vector	forward, right;
	AngleVectors( angles, &forward, &right, nullptr );

	// Apply bob, but scaled down to 40%
	VectorMA( origin, pBobState->m_flVerticalBob * 0.4f, forward, origin );

	// Z bob a bit more
	origin[2] += pBobState->m_flVerticalBob * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= pBobState->m_flVerticalBob * 0.5f;
	angles[ PITCH ]	-= pBobState->m_flVerticalBob * 0.4f;
	angles[ YAW ]	-= pBobState->m_flLateralBob  * 0.3f;

	VectorMA( origin, pBobState->m_flLateralBob * 0.2f, right, origin );
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : float
//-----------------------------------------------------------------------------
float CWeaponCSBase::CalcViewmodelBob( void )
{
	if ( cl_use_new_headbob.GetBool() == true )
	{
		CBasePlayer *player = ToBasePlayer( GetOwner() );
		//Assert( player );
		BobState_t *pBobState = GetBobState();
		if ( pBobState )
		{
			return ::CalcViewModelBobHelper( player, pBobState );
		}
		else
			return 0;
	}

	static	float bobtime;
	static	float lastbobtime;
	static  float lastspeed;
	float	cycle;

	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) ||
		( player == nullptr ) ||
		( cl_bobcycle.GetFloat() <= 0.0f ) ||
		( cl_bobup.GetFloat() <= 0.0f ) ||
		( cl_bobup.GetFloat() >= 1.0f ) )
	{
		//NOTENOTE: We don't use this return value in our case ( need to restructure the calculation function setup! )
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();
	float flmaxSpeedDelta = std::max<std::common_type_t<int, float>>( 0, ( gpGlobals->curtime - lastbobtime ) * 320.0f );

	// don't allow too big speed changes
	speed = clamp( speed, lastspeed-flmaxSpeedDelta, lastspeed+flmaxSpeedDelta );
	speed = clamp( speed, -320, 320 );

	lastspeed = speed;

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );

	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - ( int )( bobtime/cl_bobcycle.GetFloat() )*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = std::numbers::pi * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = std::numbers::pi + std::numbers::pi*( cycle-cl_bobup.GetFloat() )/( 1.0 - cl_bobup.GetFloat() );
	}

	g_verticalBob = speed*0.005f;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*std::sin( cycle );

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - ( int )( bobtime/cl_bobcycle.GetFloat()*2 )*cl_bobcycle.GetFloat()*2;
	cycle /= cl_bobcycle.GetFloat()*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = std::numbers::pi * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = std::numbers::pi + std::numbers::pi*( cycle-cl_bobup.GetFloat() )/( 1.0 - cl_bobup.GetFloat() );
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin( cycle );
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );

	//NOTENOTE: We don't use this return value in our case ( need to restructure the calculation function setup! )
	return 0.0f;

}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &origin -
//			&angles -
//			viewmodelindex -
//-----------------------------------------------------------------------------
void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	if ( cl_use_new_headbob.GetBool() == true )
	{
		// call helper functions to do the calculation
		BobState_t *pBobState = GetBobState();
		if ( pBobState )
		{
			CalcViewmodelBob();
			::AddViewModelBobHelper( origin, angles, pBobState );
		}
		return;
	}

	Vector	forward, right;
	AngleVectors( angles, &forward, &right, nullptr );

	CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.4f, forward, origin );

	// Z bob a bit more
	origin[2] += g_verticalBob * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= g_verticalBob * 0.5f;
	angles[ PITCH ]	-= g_verticalBob * 0.4f;

	angles[ YAW ]	-= g_lateralBob  * 0.3f;

	//	VectorMA( origin, g_lateralBob * 0.2f, right, origin );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the head bob state for this weapon, which is stored
//			in the view model.  Note that this this function can return
//			NULL if the player is dead or the view model is otherwise not present.
//-----------------------------------------------------------------------------
BobState_t *CWeaponCSBase::GetBobState()
{
	// get the view model for this weapon
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == nullptr )
		return nullptr;
	CBaseViewModel *baseViewModel = pOwner->GetViewModel( m_nViewModelIndex );
	if ( baseViewModel == nullptr )
		return nullptr;
	CPredictedViewModel *viewModel = dynamic_cast<CPredictedViewModel *>(baseViewModel);
	if ( viewModel == nullptr )
		return nullptr;

	//Assert( viewModel );

	// get the bob state out of the view model
	return &( viewModel->GetBobState() );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pPicker -
//-----------------------------------------------------------------------------
void CWeaponCSBase::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	UpdateOutlineGlow();

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

void WeaponSaveCustomTextures( void )
{
	CBasePlayer *local = CBasePlayer::GetLocalPlayer();
	if ( local )
	{
		CWeaponCSBase *pWeapon = ( CWeaponCSBase* )local->GetActiveWeapon();
		pWeapon->SaveCustomMaterialsTextures();
	}
}
ConCommand cl_saveweaponcustomtextures( side_registry(), "cl_saveweaponcustomtextures", WeaponSaveCustomTextures, "Save custom textures of current weapon." );

void CWeaponCSBase::SaveCustomMaterialsTextures( )
{
	/** Removed for partner depot **/
}

void CWeaponCSBase::UpdateCustomMaterial( void )
{
	/** Removed for partner depot **/
}

void CWeaponCSBase::CheckCustomMaterial( void )
{
	/** Removed for partner depot **/
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

class CCSWeaponInfo;
class CIronSightController;
class CBaseCombatWeapon;
class CBaseEntity;
class CCSPlayer;

using namespace ::ks::reflect;

static constexpr auto SILENCER_BODYGROUP_UNSET = -2;

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBaseGun, DT_WeaponCSBaseGun )

	IMPLEMENT_REFLECT_TABLE( CWeaponCSBaseGun, DT_WeaponCSBaseGun );

IMPLEMENT_REFLECT_PREDMAP( CWeaponCSBaseGun );

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
	m_iSilencerBodygroup = SILENCER_BODYGROUP_UNSET;
}

void CWeaponCSBaseGun::Spawn( )
{
	BaseClass::Spawn();

	m_bBurstMode = false;
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;	
	ResetPostponeFireReadyTime();
}

void CWeaponCSBaseGun::Precache()
{
	m_inPrecache = true;
	BaseClass::Precache();
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
			/*
			ScreenFade_t		fade;
			fade.duration = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0.175 );
			fade.holdTime = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0 );

			fade.fadeFlags = 0;
			fade.fadeFlags |= FFADE_IN;

			fade.r = 0;
			fade.g = 0;
			fade.b = 0;
			fade.a = 255;

			clientdll->View_Fade( &fade );
			*/
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

		/*
		if ( GetPlayerOwner() && ( bIsSniperRifle && IsZoomed() && m_zoomLevel == 1 ) )
		{
			ScreenFade_t		fade;
			fade.duration = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0.22 );
			fade.holdTime = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0 );

			fade.fadeFlags = 0;
			fade.fadeFlags |= FFADE_IN;

			fade.r = 0;
			fade.g = 0;
			fade.b = 0;
			fade.a = 255;

			clientdll->View_Fade( &fade );
		}
		*/

		m_fScopeZoomEndTime = gpGlobals->curtime + GetZoomTime( m_zoomLevel );
	}
	else if ( HasSilencer() && m_flDoneSwitchingSilencer <= gpGlobals->curtime )
	{
		if ( m_bSilencerOn )
		{
			SendWeaponAnim( ACT_VM_DETACH_SILENCER );

		}
		else
		{
			SendWeaponAnim( ACT_VM_ATTACH_SILENCER );

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

	if ( GetIronSightController() )
	{
		GetIronSightController()->IncreaseDotBlur( RandomFloat( 0.22f, 0.28f ) );
	}

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

const char* CWeaponCSBaseGun::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBaseGun::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
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

IMPLEMENT_REFLECT_PREDMAP( CDEagle );

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

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( DecoyGrenade, DT_DecoyGrenade )

IMPLEMENT_REFLECT_TABLE( CDecoyGrenade, DT_DecoyGrenade );

IMPLEMENT_REFLECT_PREDMAP( CDecoyGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_decoy, DecoyGrenade );
PRECACHE_REGISTER( weapon_decoy );

class CBaseViewModel;
class CBaseWeaponWorldModel;

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

	virtual void Spawn();
	virtual void Precache();

	virtual void PrimaryAttack();

	// We overload this so we can translate left/right fire activities
	virtual bool SendWeaponAnim( int iActivity );

	virtual void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_ELITE; }
	virtual bool Deploy( void );

	virtual int		GetMuzzleAttachmentIndex_1stPerson( CBaseViewModel *pViewModel );
	virtual int		GetMuzzleAttachmentIndex_3rdPerson( void );
	virtual int		GetEjectBrassAttachmentIndex_3rdPerson( void );
	
protected:
	bool FiringLeft() const;

private:
	
	CWeaponElite( const CWeaponElite & );

	bool m_inPrecache;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponElite, DT_WeaponElite )

IMPLEMENT_REFLECT_TABLE( CWeaponElite, DT_WeaponElite );

IMPLEMENT_REFLECT_PREDMAP( CWeaponElite );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_elite, WeaponElite );
// PRECACHE_REGISTER( weapon_elite );

CWeaponElite::CWeaponElite()
{
	m_inPrecache = false;
}

void CWeaponElite::Spawn( )
{
	BaseClass::Spawn();
}

void CWeaponElite::Precache()
{
	m_inPrecache = true;

	PrecacheModel( "models/weapons/w_pist_elite.mdl" );
	PrecacheModel( "models/weapons/w_eq_eholster_elite.mdl" );
	PrecacheModel( "models/weapons/w_eq_eholster.mdl" );
	PrecacheModel( "models/weapons/w_pist_elite_single.mdl" );

	BaseClass::Precache();

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

	int CWeaponElite::GetMuzzleAttachmentIndex_1stPerson( CBaseViewModel *pViewModel )
	{
		if ( FiringLeft() )
			return pViewModel->LookupAttachment( "1" );
		else
			return pViewModel->LookupAttachment( "2" );
	}

	int CWeaponElite::GetMuzzleAttachmentIndex_3rdPerson( void )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
		if ( !pWeaponWorldModel )
			return -1;

		if ( FiringLeft() )
			return pWeaponWorldModel->LookupAttachment( "muzzle_flash2" );
		else
			return pWeaponWorldModel->LookupAttachment( "muzzle_flash" );	
	}

	int CWeaponElite::GetEjectBrassAttachmentIndex_3rdPerson( void )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
		if ( !pWeaponWorldModel )
			return -1;

		if ( FiringLeft() )
			return pWeaponWorldModel->LookupAttachment( "shell_eject2" );
		else
			return pWeaponWorldModel->LookupAttachment( "shell_eject" );	
	}

using namespace ::ks::reflect;


IMPLEMENT_NETWORKCLASS_ALIASED( Flashbang, DT_Flashbang )

IMPLEMENT_REFLECT_TABLE( CFlashbang, DT_Flashbang );

IMPLEMENT_REFLECT_PREDMAP( CFlashbang );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_flashbang, Flashbang );
PRECACHE_REGISTER( weapon_flashbang );

using namespace ::ks::reflect;


IMPLEMENT_NETWORKCLASS_ALIASED( HEGrenade, DT_HEGrenade )

IMPLEMENT_REFLECT_TABLE( CHEGrenade, DT_HEGrenade );

IMPLEMENT_REFLECT_PREDMAP( CHEGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_hegrenade, HEGrenade );
PRECACHE_REGISTER( weapon_hegrenade );

class CIronSightController;
class CBaseCombatCharacter;
class CCSPlayer;
class CEconItemView;
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

	m_angViewLast.Init();
	for ( int i=0; i<IRONSIGHT_ANGLE_AVERAGE_SIZE; i++ )
	{
		m_angDeltaAverage[i].Init();
	}
	m_vecDotCoords.Init();
	m_flDotBlur						= 0.0f;
	m_flSpeedRatio					= 0.0f;
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

void CIronSightController::IncreaseDotBlur(float flAmount)
{
	if ( IsInIronSight() && prediction->IsFirstTimePredicted() )
		m_flDotBlur = clamp(m_flDotBlur + flAmount, 0, 1);
}

float CIronSightController::GetDotBlur(void)
{
	return Bias(1.0f - Max(m_flDotBlur, m_flSpeedRatio * 0.5f), 0.2f);
}

float CIronSightController::GetDotWidth(void)
{
	return (32 + (256 * Max(m_flDotBlur, m_flSpeedRatio * 0.3f)));

}
Vector2D CIronSightController::GetDotCoords(void)
{
	return m_vecDotCoords;
}

bool CIronSightController::ShouldHideCrossHair( void )
{
	return ( (IsApproachingSighted() || IsApproachingUnSighted()) && GetIronSightAmount() > IRONSIGHT_HIDE_CROSSHAIR_THRESHOLD );
}

const char *CIronSightController::GetDotMaterial( void )
{
	//TODO: convert to schema attribute
	if ( m_pAttachedWeapon && m_pAttachedWeapon->GetCSWeaponID() == WEAPON_SG556 )
		return "models/weapons/shared/scope/scope_dot_red";

	return "models/weapons/shared/scope/scope_dot_green";
}

QAngle CIronSightController::QAngleDiff( QAngle &angTarget, QAngle &angSrc )
{
	return QAngle(	AngleDiff( angTarget.x, angSrc.x ),
					AngleDiff( angTarget.y, angSrc.y ),
					AngleDiff( angTarget.z, angSrc.z ) );
}

QAngle CIronSightController::GetAngleAverage( void )
{
	QAngle temp;
	temp.Init();

	if (GetIronSightAmount() < 1.0f)
		return temp;

	for ( int i=0; i<IRONSIGHT_ANGLE_AVERAGE_SIZE; i++ )
	{
		temp += m_angDeltaAverage[i];
	}

	return ( temp * IRONSIGHT_ANGLE_AVERAGE_DIVIDE );
}

void CIronSightController::AddToAngleAverage( QAngle newAngle )
{

	if (GetIronSightAmount() < 1.0f)
		return;

	newAngle.x = clamp( newAngle.x, -2, 2 );
	newAngle.y = clamp( newAngle.y, -2, 2 );
	newAngle.z = clamp( newAngle.z, -2, 2 );

	for ( int i=IRONSIGHT_ANGLE_AVERAGE_SIZE-1; i>0; i-- )
	{
		m_angDeltaAverage[i] = m_angDeltaAverage[i-1];
	}

	m_angDeltaAverage[0] = newAngle;
}

extern ConVar cl_righthand;
void CIronSightController::ApplyIronSightPositioning( Vector &vecPosition, QAngle &angAngle, const Vector &vecBobbedEyePosition, const QAngle &angBobbedEyeAngle )
{
	UpdateIronSightAmount();

	if ( m_flIronSightAmount == 0 )
		return;

	//check if the player is moving and save off a usable movement speed ratio for later
	if ( m_pAttachedWeapon->GetOwner() )
	{
		CBaseCombatCharacter *pPlayer = m_pAttachedWeapon->GetOwner();
		m_flSpeedRatio = Approach( pPlayer->GetAbsVelocity().Length() / m_pAttachedWeapon->GetMaxSpeed(), m_flSpeedRatio, gpGlobals->frametime * 10.0f );
	}

	//if we're more than 10% ironsighted, apply looseness.
	if ( m_flIronSightAmount > 0.1f )
	{
		//get the difference between current angles and last angles
		QAngle angDelta = QAngleDiff( m_angViewLast, angAngle );
	
		//dampen the delta to simulate 'looseness', but the faster we move, the more looseness approaches ironsight_running_looseness.GetFloat(), which is as waggly as possible
#ifdef DEBUG
		if ( ironsight_override.GetBool() )
		{
			AddToAngleAverage( angDelta * Lerp(m_flSpeedRatio, ironsight_looseness.GetFloat(), ironsight_running_looseness.GetFloat()) );
		}
		else
#endif
		{
			AddToAngleAverage( angDelta * Lerp(m_flSpeedRatio, m_flIronSightLooseness, 0.3f ) );
		}

		//m_angViewLast tries to catch up to angAngle
#ifdef DEBUG
		m_angViewLast -= angDelta * clamp( gpGlobals->frametime * ironsight_catchupspeed.GetFloat(), 0, 1 );
#else
		m_angViewLast -= angDelta * clamp( gpGlobals->frametime * ironsight_catchupspeed, 0, 1 );
#endif

	}
	else
	{
		m_angViewLast = angAngle;
	}
		
	//now the fun part - move the viewmodel to look down the sights

	//create a working matrix at the current eye position and angles
	VMatrix matIronSightMatrix = SetupMatrixOrgAngles( vecPosition, angAngle );

	//offset the matrix by the ironsight eye position
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		Vector vecTemp; //when overridden use convar values instead of schema driven ones so we can iterate on the values quickly while authoring
		if ( sscanf( ironsight_position.GetString(), "%f %f %f", &vecTemp.x, &vecTemp.y, &vecTemp.z ) == 3 )
			MatrixTranslate( matIronSightMatrix, (-vecTemp) * GetIronSightAmountGained() );
	}
	else
#endif
	{
		//use schema defined offset
		MatrixTranslate( matIronSightMatrix, (-m_vecEyePos) * GetIronSightAmountGained() );
	}
	
	//additionally offset by the ironsight origin of rotation, the weapon will pivot around this offset from the eye
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		MatrixTranslate( matIronSightMatrix, Vector( ironsight_pivot_forward.GetFloat(), 0, 0 ) );
	}
	else
#endif
	{
		MatrixTranslate( matIronSightMatrix, Vector( m_flIronSightPivotForward, 0, 0 ) );
	}

	QAngle angDeltaAverage = GetAngleAverage();

	if ( !cl_righthand.GetBool() )
		angDeltaAverage = -angDeltaAverage;

	//apply ironsight eye rotation
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		QAngle angTemp;  //when overridden use convar values instead of schema driven ones so we can iterate on the values quickly while authoring
		if ( sscanf( ironsight_angle.GetString(), "%f %f %f", &angTemp.x, &angTemp.y, &angTemp.z ) == 3 )
		{
			MatrixRotate( matIronSightMatrix, Vector( 1, 0, 0 ), (angDeltaAverage.z + angTemp.z) * GetIronSightAmountGained() );
			MatrixRotate( matIronSightMatrix, Vector( 0, 1, 0 ), (angDeltaAverage.x + angTemp.x) * GetIronSightAmountGained() );
			MatrixRotate( matIronSightMatrix, Vector( 0, 0, 1 ), (angDeltaAverage.y + angTemp.y) * GetIronSightAmountGained() );
		}
	}
	else
#endif
	{
		
		//use schema defined angles
		MatrixRotate( matIronSightMatrix, Vector( 1, 0, 0 ), (angDeltaAverage.z + m_angPivotAngle.z) * GetIronSightAmountGained() );
		MatrixRotate( matIronSightMatrix, Vector( 0, 1, 0 ), (angDeltaAverage.x + m_angPivotAngle.x) * GetIronSightAmountGained() );
		MatrixRotate( matIronSightMatrix, Vector( 0, 0, 1 ), (angDeltaAverage.y + m_angPivotAngle.y) * GetIronSightAmountGained() );

	}
	
	if ( !cl_righthand.GetBool() )
		angDeltaAverage = -angDeltaAverage;

	//move the weapon back to the ironsight eye position
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		MatrixTranslate( matIronSightMatrix, Vector( -ironsight_pivot_forward.GetFloat(), 0, 0 ) );
	}
	else
#endif
	{
		MatrixTranslate( matIronSightMatrix, Vector( -m_flIronSightPivotForward, 0, 0 ) );
	}	

	//if the player is moving, pull down and re-bob the weapon
	if ( m_pAttachedWeapon->GetOwner() )
	{
		//magic bob value, replace me
		Vector vecIronSightBob = Vector(
			1,
			IRONSIGHT_VIEWMODEL_BOB_MULT_X * std::sin( gpGlobals->curtime * IRONSIGHT_VIEWMODEL_BOB_PERIOD_X ),			
			IRONSIGHT_VIEWMODEL_BOB_MULT_Y * sin( gpGlobals->curtime * IRONSIGHT_VIEWMODEL_BOB_PERIOD_Y ) - IRONSIGHT_VIEWMODEL_BOB_MULT_Y
			);
	
		m_vecDotCoords.x = -vecIronSightBob.y;
		m_vecDotCoords.y = -vecIronSightBob.z;
		m_vecDotCoords *= 0.1f;
		m_vecDotCoords.x -= angDeltaAverage.y * 0.03f;
		m_vecDotCoords.y += angDeltaAverage.x * 0.03f;
		m_vecDotCoords *= m_flSpeedRatio;

		if ( !cl_righthand.GetBool() )
			vecIronSightBob.y = -vecIronSightBob.y;

		MatrixTranslate( matIronSightMatrix, vecIronSightBob * m_flSpeedRatio );
	}

	//extract the final position and angles and apply them as differences from the passed in values
	vecPosition -= ( vecPosition - matIronSightMatrix.GetTranslation() );

	QAngle angIronSightAngles;
	MatrixAngles( matIronSightMatrix.As3x4(), angIronSightAngles );
	angAngle -= QAngleDiff( angAngle, angIronSightAngles );

	//dampen dot blur
	m_flDotBlur = Approach(0.0f, m_flDotBlur, gpGlobals->frametime * 2.0f);

}

static void SetRenderTargetAndViewPort(ITexture *rt)
{
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->SetRenderTarget(rt);
	if (rt)
	{
		pRenderContext->Viewport(0, 0, rt->GetActualWidth(), rt->GetActualHeight());
	}
}

#ifdef DEBUG
ConVar r_ironsight_scope_effect(side_registry(), "r_ironsight_scope_effect", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar ironsight_laser_dot_render_tweak1(side_registry(), "ironsight_laser_dot_render_tweak1", "61", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar ironsight_laser_dot_render_tweak2(side_registry(), "ironsight_laser_dot_render_tweak2", "64", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
#endif

bool CIronSightController::PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )
{
#ifdef DEBUG
	if (!r_ironsight_scope_effect.GetBool())
		return false;
#endif

	if (!IsInIronSight())
		return false;

	Rect_t actualRect;
	UpdateScreenEffectTexture(0, x, y, w, h, false, &actualRect);
	ITexture *pRtFullFrame = GetFullFrameFrameBufferTexture(0);

	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->PushRenderTargetAndViewport();

	// DOWNSAMPLE _rt_FullFrameFB TO _rt_SmallFB0
	IMaterial *pMatDownsample = materials->FindMaterial("dev/scope_downsample", TEXTURE_GROUP_OTHER, true);
	ITexture *pRtQuarterSize0 = materials->FindTexture("_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET);
	SetRenderTargetAndViewPort(pRtQuarterSize0);

	int nSrcWidth = pViewSetup->width;
	int nSrcHeight = pViewSetup->height;

	pRenderContext->DrawScreenSpaceRectangle(pMatDownsample, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth - 4, nSrcHeight - 4,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());

	//horizontally blur pRtQuarterSize0 over to pRtQuarterSize1
	IMaterial *pMatBlurX = materials->FindMaterial("dev/scope_blur_x", TEXTURE_GROUP_OTHER, true);
	ITexture *pRtQuarterSize1 = materials->FindTexture("_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET);
	SetRenderTargetAndViewPort(pRtQuarterSize1);

	pRenderContext->DrawScreenSpaceRectangle(pMatBlurX, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight());

	//vertically blur pRtQuarterSize1 over to pRtQuarterSize0
	IMaterial *pMatBlurY = materials->FindMaterial("dev/scope_blur_y", TEXTURE_GROUP_OTHER, true);
	SetRenderTargetAndViewPort(pRtQuarterSize0);

	pRenderContext->DrawScreenSpaceRectangle(pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight());

	pRenderContext->PopRenderTargetAndViewport();

	//Prepare to render the scope lens mask shape into the stencil buffer.
	//The weapon itself will take care of using the correct blend mode and override material.

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;

	stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;

	pRenderContext->SetStencilState(stencilState);

	return true;
}

void CIronSightController::RenderScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )
{

#ifdef DEBUG
	if ( !r_ironsight_scope_effect.GetBool() )
		return;
#endif

	if ( !IsInIronSight() )
		return;

	// apply the blur effect to the screen while masking out the scope lens

	CMatRenderContextPtr pRenderContext(materials);

	ShaderStencilState_t stencilState_skip_scope_lens_pixels;
	stencilState_skip_scope_lens_pixels.m_bEnable = true;
	stencilState_skip_scope_lens_pixels.m_nReferenceValue = 1;
	stencilState_skip_scope_lens_pixels.m_CompareFunc = SHADER_STENCILFUNC_NOTEQUAL;
	stencilState_skip_scope_lens_pixels.m_PassOp = SHADER_STENCILOP_KEEP;
	stencilState_skip_scope_lens_pixels.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState_skip_scope_lens_pixels.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState(stencilState_skip_scope_lens_pixels);

	// RENDER _rt_SmallFB0 to screen
	IMaterial *pBlurOverlayMaterial = materials->FindMaterial("dev/scope_bluroverlay", TEXTURE_GROUP_OTHER, true);

	//set alpha to the amount of ironsightedness
	IMaterialVar *pAlphaVar = pBlurOverlayMaterial->FindVar("$alpha", 0);
	if (pAlphaVar != nullptr)
	{
		pAlphaVar->SetFloatValue(Bias( GetIronSightAmount(), 0.2f));
	}
	pRenderContext->DrawScreenSpaceQuad(pBlurOverlayMaterial);

	// now draw the laser dot, masked to ONLY render on the lens

	Vector2D dotCoords = GetDotCoords();
	dotCoords.x *= engine->GetScreenAspectRatio(w, h);

	//CMatRenderContextPtr pRenderContext(materials);
	IMaterial *pMatDot = materials->FindMaterial(GetDotMaterial(), TEXTURE_GROUP_OTHER, true);

	pRenderContext->OverrideDepthEnable(true, false, false);

	ShaderStencilState_t stencilState_use_only_scope_lens_pixels;
	stencilState_use_only_scope_lens_pixels.m_bEnable = true;
	stencilState_use_only_scope_lens_pixels.m_nReferenceValue = 1;
	stencilState_use_only_scope_lens_pixels.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
	stencilState_use_only_scope_lens_pixels.m_PassOp = SHADER_STENCILOP_KEEP;
	stencilState_use_only_scope_lens_pixels.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState_use_only_scope_lens_pixels.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState(stencilState_use_only_scope_lens_pixels);

	int iWidth = GetDotWidth();

	IMaterialVar *pAlphaVar2 = pMatDot->FindVar("$alpha", 0);
	if (pAlphaVar2 != nullptr)
	{
		pAlphaVar2->SetFloatValue(GetDotBlur());
	}

	dotCoords.x += 0.5f;
	dotCoords.y += 0.5f;
#ifdef DEBUG
	pRenderContext->DrawScreenSpaceRectangle(pMatDot, (w * dotCoords.x) - (iWidth / 2), (h * dotCoords.y) - (iWidth / 2), iWidth, iWidth,
		0, 0, ironsight_laser_dot_render_tweak1.GetInt(), ironsight_laser_dot_render_tweak1.GetInt(),
		ironsight_laser_dot_render_tweak2.GetInt(), ironsight_laser_dot_render_tweak2.GetInt());
#else
	pRenderContext->DrawScreenSpaceRectangle(pMatDot, (w * dotCoords.x) - (iWidth / 2), (h * dotCoords.y) - (iWidth / 2), iWidth, iWidth,
		0, 0, 61, 61,
		64, 64);
#endif

	pRenderContext->OverrideDepthEnable(false, true);

	// restore a disabled stencil state

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	pRenderContext->SetStencilState(stencilStateDisable);

	//clean up stencil buffer once we're done so render elements like the glow pass draw correctly
	pRenderContext->ClearBuffers(false, false, true);

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

		CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetObserverTarget() == pPlayer && pLocalPlayer->GetObserverInterpState() == CCSPlayer::OBSERVER_INTERP_TRAVELING )
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

class CEffectData;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

static constexpr auto KNIFE_BODYHIT_VOLUME = 128;
static constexpr auto KNIFE_WALLHIT_VOLUME = 512;

static constexpr auto KNIFE_RANGE_SHORT = 32;
static constexpr auto KNIFE_RANGE_LONG = 48;

Vector head_hull_mins( -16, -16, -18 );
Vector head_hull_maxs( 16, 16, 18 );

// ----------------------------------------------------------------------------- //
// CKnife tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( Knife, DT_WeaponKnife )

IMPLEMENT_REFLECT_TABLE( CKnife, DT_WeaponKnife );

IMPLEMENT_REFLECT_PREDMAP( CKnife );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_knife, Knife );
PRECACHE_REGISTER( weapon_knife );

// ----------------------------------------------------------------------------- //
// CKnife implementation.
// ----------------------------------------------------------------------------- //

CKnife::CKnife()
{

}

bool CKnife::HasPrimaryAmmo()
{
	return true;
}

bool CKnife::CanBeSelected()
{
	return true;
}

void CKnife::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Weapon_Knife.Deploy" );
	PrecacheScriptSound( "Weapon_Knife.Slash" );
	PrecacheScriptSound( "Weapon_Knife.Stab" );
	PrecacheScriptSound( "Weapon_Knife.Hit" );
	PrecacheEffect( "KnifeSlash" );
}

void CKnife::Spawn()
{
	m_iClip1 = -1;
	BaseClass::Spawn();
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
		SwingOrStab( Primary_Mode );
	}
}

void CKnife::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && !pPlayer->m_bIsDefusing && !CSGameRules()->IsFreezePeriod() )
	{
		SwingOrStab( Secondary_Mode );
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

bool CKnife::SwingOrStab( CSWeaponMode weaponMode )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	// bStab: false=primary, true=secondary
	float fRange = (weaponMode == Primary_Mode) ? KNIFE_RANGE_LONG : KNIFE_RANGE_SHORT; // knife range
	
	Vector vForward; AngleVectors( pPlayer->EyeAngles(), &vForward );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * fRange;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	//check for hitting glass - TODO - fix this hackiness, doesn't always line up with what FindHullIntersection returns

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
			SendWeaponAnim( bBackStab ? ACT_VM_SWINGHARD : ACT_VM_HITCENTER2 );
		}
		else // swing
		{
			SendWeaponAnim( bBackStab ? ACT_VM_SWINGHIT : ACT_VM_HITCENTER );
		}

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
		data.m_hEntity = tr.Ent<CBaseEntity>()->GetRefEHandle();

		CPASFilter filter( data.m_vOrigin );

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

	RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), !bDidHit ? RUMBLE_CROWBAR_SWING : RUMBLE_AR2, 0, RUMBLE_FLAG_RESTART );

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

IMPLEMENT_REFLECT_PREDMAP( CKnifeGG );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_knifegg, KnifeGG );
PRECACHE_REGISTER( weapon_knifegg );

class CCSWeaponInfo;
class CBaseEntity;
class CCSPlayer;
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

IMPLEMENT_REFLECT_PREDMAP( CWeaponM3 );

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
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255, // wrap it for network traffic so it's the same between client and server
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
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
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

class CNewParticleEffect;
class CBaseAnimating;
class CBaseWeaponWorldModel;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( MolotovGrenade, DT_MolotovGrenade )

IMPLEMENT_REFLECT_TABLE( CMolotovGrenade, DT_MolotovGrenade );

IMPLEMENT_REFLECT_PREDMAP( CMolotovGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_molotov, MolotovGrenade );
PRECACHE_REGISTER( weapon_molotov );

static int s_nMolotovFire;
PRECACHE_REGISTER_BEGIN( GLOBAL, CMolotovGrenade )
	PRECACHE_INDEX( PARTICLE_SYSTEM, "weapon_molotov_held", s_nMolotovFire );
PRECACHE_REGISTER_END()

void CMolotovGrenade::UpdateParticles( void )
{
	// FIXME: This is bogus; we need to make the particle property have particle system types:
	// first person, third person, owner, and logic in the particle property to know whether
	// to render a given system given these rules and knowledge of what mode the owner is in
	CCSPlayer *pPlayer = ToCSPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	int nRenderFlags = 0;

	CWeaponCSBase *pCSWeapon = (CWeaponCSBase*)pPlayer->GetActiveWeapon();
	if ( !pCSWeapon )
		return;

	int iWeaponId = pCSWeapon->GetCSWeaponID();

	if ( iWeaponId == WEAPON_MOLOTOV )
	{
		bool bIsFirstOrThirdpersonMolotovVisible = pCSWeapon->IsWeaponVisible();
		
		CBaseAnimating *pWeaponBaseAnimating = pCSWeapon->GetBaseAnimating();

		CBaseWeaponWorldModel *pWeaponWorldModel = pCSWeapon->GetWeaponWorldModel();
		if ( pWeaponWorldModel && pWeaponWorldModel->ShouldDraw() )
		{
			pWeaponBaseAnimating = pWeaponWorldModel->GetBaseAnimating();
			bIsFirstOrThirdpersonMolotovVisible = true;
		}

		if ( m_molotovParticleEffect.IsValid() ) 
		{
//			m_molotovParticleEffect->SetDormant( pPlayer->GetPlayerAnimState()->ShouldHideWeapon() ); // ShouldHideWeapon is a Terror Codebase function, not CStrike15
			m_molotovParticleEffect->SetDormant( !bIsFirstOrThirdpersonMolotovVisible ); // Is the weapon Hidden?
		}

		if ( bIsFirstOrThirdpersonMolotovVisible )
		{	
			if ( m_bPinPulled )
			{
				if ( !m_molotovParticleEffect() )
				{
					// TEST: [mlowrance] This is to test for attachment.
					int iAttachment = -1;
					if ( pWeaponBaseAnimating )
						iAttachment = pWeaponBaseAnimating->LookupAttachment( "Wick" );

					if ( iAttachment >= 0 )
					{
						// FIXME: Precache 'Wick' attachment index
						m_molotovParticleEffect = pWeaponBaseAnimating->ParticleProp()->CreatePrecached( s_nMolotovFire, PATTACH_POINT_FOLLOW, iAttachment );
						EmitSound( "Molotov.IdleLoop" );
						SetLoopingSoundPlaying( true );

						//DevMsg( 1, "++++++++++>Playing Molotov.IdleLoop 1\n" );

					}
				}

				if ( m_molotovParticleEffect() )
				{
					if ( nRenderFlags != 3 )
					{
						m_molotovParticleEffect->SetDrawOnlyForSplitScreenUser( nRenderFlags - 1 );
					}
					else
					{
						m_molotovParticleEffect->SetDrawOnlyForSplitScreenUser( -1 );
					}
				}
			}
		}
	}
	else
	{
		if ( m_molotovParticleEffect.IsValid() )
		{
			StopSound( "Molotov.IdleLoop" );
			//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 1\n" );
			m_molotovParticleEffect->StopEmission( false, false );
			m_molotovParticleEffect->SetRemoveFlag();
			m_molotovParticleEffect = nullptr;
		}
	}
}

bool CMolotovGrenade::Simulate( void )
{
	UpdateParticles();
	return BaseClass::Simulate();
}

//--------------------------------------------------------------------------------------------------------
void CMolotovGrenade::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_molotovParticleEffect() == pParticleEffect )
	{
		m_molotovParticleEffect = nullptr;
	}
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

class CCSWeaponInfo;
class CBaseEntity;
class CCSPlayer;
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

IMPLEMENT_REFLECT_PREDMAP( CWeaponNOVA );

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
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255, // wrap it for network traffic so it's the same between client and server
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
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
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

class CCSWeaponInfo;
class CBaseEntity;
class CCSPlayer;
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

IMPLEMENT_REFLECT_PREDMAP( CWeaponSawedoff );

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
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255, // wrap it for network traffic so it's the same between client and server
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
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
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

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( SensorGrenade, DT_SensorGrenade )

IMPLEMENT_REFLECT_TABLE( CSensorGrenade, DT_SensorGrenade );

IMPLEMENT_REFLECT_PREDMAP( CSensorGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_tagrenade, SensorGrenade );
PRECACHE_REGISTER( weapon_tagrenade );

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

	virtual bool	DoesHideViewModelWhenZoomed( void ) { return false; }

private:

	CWeaponSG552( const CWeaponSG552 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSG552, DT_WeaponSG552 )

IMPLEMENT_REFLECT_TABLE( CWeaponSG552, DT_WeaponSG552 );

IMPLEMENT_REFLECT_PREDMAP( CWeaponSG552 );

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

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( SmokeGrenade, DT_SmokeGrenade )

IMPLEMENT_REFLECT_TABLE( CSmokeGrenade, DT_SmokeGrenade );

IMPLEMENT_REFLECT_PREDMAP( CSmokeGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_smokegrenade, SmokeGrenade );
PRECACHE_REGISTER( weapon_smokegrenade );

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

	virtual void Precache();
	virtual void PrimaryAttack( void );
	virtual const char* GetMuzzleFlashEffectName_1stPerson( void );
	virtual const char* GetMuzzleFlashEffectName_3rdPerson( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_TASER; }

private:
	CWeaponTaser( const CWeaponTaser& );
	float	m_fFireTime;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTaser, DT_WeaponTaser )

IMPLEMENT_REFLECT_TABLE( CWeaponTaser, DT_WeaponTaser );

IMPLEMENT_REFLECT_PREDMAP( CWeaponTaser );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_taser, WeaponTaser );
// PRECACHE_REGISTER( weapon_taser );

CWeaponTaser::CWeaponTaser() :
	m_fFireTime(0.0f)
{
}

void CWeaponTaser::Precache()
{
	BaseClass::Precache();

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

class CCSWeaponInfo;
class CBaseEntity;
class CCSPlayer;
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

IMPLEMENT_REFLECT_PREDMAP( CWeaponXM1014 );

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
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255, // wrap it for network traffic so it's the same between client and server
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
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
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
