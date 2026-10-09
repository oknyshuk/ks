// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
import ks.vstdlib;
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "ndebugoverlay.h"
#endif
#include "player_pickup.h"

#include "igameevents.h" 

// player pickup utility routine
void Pickup_ForcePlayerToDropThisObject( CBaseEntity *pTarget )
{
	Warning ( "Failing to force player to drop object.\n" );
	contract_assert( false && "This function is assumed to not be needed in portal 2, if this assert fires we need to fix it." );
}


void Pickup_OnPhysGunDrop( CBaseEntity *pDroppedObject, CBasePlayer *pPlayer, PhysGunDrop_t Reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pDroppedObject);
	if ( pPickup )
	{
		pPickup->OnPhysGunDrop( pPlayer, Reason );
	}
}


void Pickup_OnPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		pPickup->OnPhysGunPickup( pPlayer, reason );
	}

	// send phys gun pickup item event, but only in single player
	if ( !g_pGameRules->IsMultiplayer() )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "physgun_pickup" );
		if ( event )
		{
			event->SetInt( "entindex", pPickedUpObject->entindex() );
			gameeventmanager->FireEvent( event );
		}
	}
}

bool Pickup_OnAttemptPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		return pPickup->OnAttemptPhysGunPickup( pPlayer, reason );
	}
	return true;
}

CBaseEntity	*Pickup_OnFailedPhysGunPickup( CBaseEntity *pPickedUpObject, Vector vPhysgunPos )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		return pPickup->OnFailedPhysGunPickup( vPhysgunPos );
	}

	return nullptr;
}

bool Pickup_GetPreferredCarryAngles( CBaseEntity *pObject, CBasePlayer *pPlayer, matrix3x4_t &localToWorld, QAngle &outputAnglesWorldSpace )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		if ( pPickup->HasPreferredCarryAnglesForPlayer( pPlayer ) )
		{
			outputAnglesWorldSpace = TransformAnglesToWorldSpace( pPickup->PreferredCarryAngles(), localToWorld );
			return true;
		}
	}
	return false;
}

bool Pickup_ForcePhysGunOpen( CBaseEntity *pObject, CBasePlayer *pPlayer )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		return pPickup->ForcePhysgunOpen( pPlayer );
	}
	return false;
}

AngularImpulse Pickup_PhysGunLaunchAngularImpulse( CBaseEntity *pObject, PhysGunForce_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup != nullptr && pPickup->ShouldPuntUseLaunchForces( reason ) )
	{
		return pPickup->PhysGunLaunchAngularImpulse();
	}
	return RandomAngularImpulse( -600, 600 );
}

Vector Pickup_DefaultPhysGunLaunchVelocity( const Vector &vecForward, float flMass )
{

	// Do the simple calculation
	return ( vecForward * flMass );
}

Vector Pickup_PhysGunLaunchVelocity( CBaseEntity *pObject, const Vector &vecForward, PhysGunForce_t reason )
{
	// The object must be valid
	if ( pObject == nullptr )
	{
		contract_assert( 0 );
		return vec3_origin;
	}

	// Shouldn't ever get here with a non-vphysics object.
	IPhysicsObject *pPhysicsObject = pObject->VPhysicsGetObject();
	if ( pPhysicsObject == nullptr )
	{
		contract_assert( 0 );
		return vec3_origin;
	}

	// Call the pickup entity's callback
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup != nullptr && pPickup->ShouldPuntUseLaunchForces( reason ) )
		return pPickup->PhysGunLaunchVelocity( vecForward, pPhysicsObject->GetMass() );

	// Do our default behavior
	return Pickup_DefaultPhysGunLaunchVelocity(	vecForward, pPhysicsObject->GetMass() );
}

bool Pickup_ShouldPuntUseLaunchForces( CBaseEntity *pObject, PhysGunForce_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		return pPickup->ShouldPuntUseLaunchForces( reason );
	}
	return false;
}
