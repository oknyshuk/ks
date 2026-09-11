//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
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

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

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
