// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "sharedvar.h"
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
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "weapon_csbase.h"
#include "gamerules.h"
#include "engine/IEngineSound.h"
#include "weapon_sensorgrenade.h"

#ifdef CLIENT_DLL
	
#else
	#include "cs_player.h"
	#include "items.h"
	#include "sensorgrenade_projectile.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( SensorGrenade, DT_SensorGrenade )

IMPLEMENT_REFLECT_TABLE( CSensorGrenade, DT_SensorGrenade );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CSensorGrenade );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_tagrenade, SensorGrenade );
PRECACHE_REGISTER( weapon_tagrenade );

#if !defined( CLIENT_DLL )


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

#endif // !CLIENT_DLL
