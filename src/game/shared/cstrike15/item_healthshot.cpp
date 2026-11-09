// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: Healthshot Item, belt item

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
#include "const.h"
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
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#endif
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "item_healthshot.h"
#include "cs_gamerules.h"

#if defined( CLIENT_DLL )
#include "c_cs_player.h"
#else
#include "cs_player.h"
#endif // CLIENT_DLL

constexpr auto HEALTHSHOT_INJECT_TIME = 1.65f;

IMPLEMENT_NETWORKCLASS_ALIASED( Item_Healthshot, DT_Item_Healthshot )

IMPLEMENT_REFLECT_TABLE( CItem_Healthshot, DT_Item_Healthshot );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CItem_Healthshot );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_healthshot, Item_Healthshot );
PRECACHE_WEAPON_REGISTER( weapon_healthshot );

// #ifndef CLIENT_DLL
// BEGIN_DATADESC( CItem_Healthshot )
// END_DATADESC()
// #endif

ConVar healthshot_health( "healthshot_health", "50", FCVAR_CHEAT | FCVAR_REPLICATED );

void CItem_Healthshot::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;
	
	SendWeaponAnim( ACT_VM_IDLE );

	SetWeaponIdleTime( gpGlobals->curtime + 20 );
}

void CItem_Healthshot::Precache_Impl( void )
{
	//PrecacheScriptSound( "Healthshot.Shot" );

	BaseClass::Precache_Impl();
}

bool CItem_Healthshot::CanPrimaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( pPlayer == nullptr )
		return false;

	return CanUseOnSelf( pPlayer );
}

bool CItem_Healthshot::CanUseOnSelf( CCSPlayer *pPlayer )	
{ 
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetHealth() >= pPlayer->GetMaxHealth() )
	{
#ifndef CLIENT_DLL
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Healthshot_AlreadyAtMax" );
#endif
		return false;
	}

	return true;
}

void CItem_Healthshot::DropHealthshot( void )
{
#ifndef CLIENT_DLL
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	int iAmount = pPlayer->GetAmmoCount( GetPrimaryAmmoType() );
	if ( iAmount <= 1 )
	{
		pPlayer->CSWeaponDrop( this, false, true );
		return;
	}
	else	
	{
		pPlayer->RemoveAmmo( 1, AMMO_TYPE_HEALTHSHOT );

		CItem_Healthshot *pHealth = static_cast< CItem_Healthshot * >( CreateEntityByName( "weapon_healthshot" ) );

		if ( pHealth )
		{
			Vector vecWeaponThrowFromPos = pPlayer->EyePosition();
			QAngle angWeaponThrowFromAngle = pPlayer->EyeAngles();

 			Vector vForward;
 			AngleVectors(angWeaponThrowFromAngle, &vForward, nullptr, nullptr);
 			vecWeaponThrowFromPos = vecWeaponThrowFromPos + (vForward * 100);
			//NDebugOverlay::Box( vecWeaponThrowFromPos, Vector( 10, 10, 10 ), Vector( -10, -10, -10 ), 255, 0, 0, 200, 3 );

			DispatchSpawn( pHealth );

			// set it non-solid because it hits itself during the trace when trying to throw it
			pHealth->SetSolidFlags( FSOLID_NOT_SOLID );
			pHealth->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

			pPlayer->Weapon_Drop( pHealth, &vecWeaponThrowFromPos, nullptr );			

			pHealth->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
			pHealth->SetPreviousOwner( pPlayer );
			pHealth->m_flDroppedAtTime = gpGlobals->curtime;
			pHealth->AddPriorOwner( pPlayer );
		}
	}


#endif
}

//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::OnStartUse( CCSPlayer *pPlayer )
{
	SetWeaponIdleTime( gpGlobals->curtime + 20 ); // don't fidget for a bit
	BaseClass::OnStartUse( pPlayer );
}

#ifndef CLIENT_DLL
//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::CompleteUse( CCSPlayer *pPlayer )
{
	pPlayer->OnHealthshotUsed();

	// Give half health buffer
	pPlayer->SetHealth( Min( pPlayer->GetHealth() + healthshot_health.GetInt(), pPlayer->GetMaxHealth() ) );

	// emit event	
// 	IGameEvent *event = gameeventmanager->CreateEvent( "healthshot_used" );
// 	if( event )
// 	{
// 		int userID = pPlayer->GetUserID();
// 		event->SetInt( "userid", userID );
// 		event->SetInt( "subject", userID );
// 		gameeventmanager->FireEvent( event );
// 	}

	BaseClass::CompleteUse( pPlayer );
}
#endif	//CLIENT_DLL

float CItem_Healthshot::GetUseTimerDuration( void )
{
	return HEALTHSHOT_INJECT_TIME;
}
