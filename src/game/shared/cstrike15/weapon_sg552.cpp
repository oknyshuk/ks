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
#include "const.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
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
#include "weapon_csbasegun.h"

#if defined( CLIENT_DLL )

	#define CWeaponSG552 C_WeaponSG552
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif

class [[= ks::reflect::NetTable{ .name = "DT_WeaponSG552" } ]]
      CWeaponSG552 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponSG552, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSG552();

	virtual void PrimaryAttack();
	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_SG552; }

#ifdef CLIENT_DLL
	virtual bool	DoesHideViewModelWhenZoomed( void ) { return false; }
#endif

private:

	CWeaponSG552( const CWeaponSG552 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSG552, DT_WeaponSG552 )

IMPLEMENT_REFLECT_TABLE( CWeaponSG552, DT_WeaponSG552 );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CWeaponSG552 );
#endif

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
