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
import ks.math;
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
#include "weapon_flashbang.h"

#ifdef CLIENT_DLL

#else

	#include "cs_player.h"
	#include "items.h"
	#include "flashbang_projectile.h"

#endif

constexpr auto GRENADE_TIMER = 3.0f; //Seconds



IMPLEMENT_NETWORKCLASS_ALIASED( Flashbang, DT_Flashbang )

IMPLEMENT_REFLECT_TABLE( CFlashbang, DT_Flashbang );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CFlashbang );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_flashbang, Flashbang );
PRECACHE_REGISTER( weapon_flashbang );


#ifndef CLIENT_DLL


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

#endif
