// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
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
	#define CDEagle C_DEagle
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
#endif

class [[= ks::reflect::NetTable{ .name = "DT_WeaponDEagle" } ]]
      CDEagle : public CWeaponCSBaseGun
{
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

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CDEagle );
#endif

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
