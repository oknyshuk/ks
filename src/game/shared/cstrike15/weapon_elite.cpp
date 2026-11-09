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

	#define CWeaponElite C_WeaponElite
	#include "c_cs_player.h"
	#include "c_te_effect_dispatch.h"

#else

	#include "cs_player.h"

#endif

class [[= ks::reflect::NetTable{ .name = "DT_WeaponElite" } ]]
      CWeaponElite : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponElite, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponElite();

	KS_VIRTUAL void Spawn_Impl();
	KS_VIRTUAL void Precache_Impl();

	virtual void PrimaryAttack();

	// We overload this so we can translate left/right fire activities
	virtual bool SendWeaponAnim( int iActivity );

	virtual void WeaponIdle();

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_ELITE; }
	virtual bool Deploy( void );

#ifdef CLIENT_DLL
	virtual int		GetMuzzleAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel );
	virtual int		GetMuzzleAttachmentIndex_3rdPerson( void );
	virtual int		GetEjectBrassAttachmentIndex_3rdPerson( void );
#endif
	
protected:
	bool FiringLeft() const;

private:
	
	CWeaponElite( const CWeaponElite & );

	bool m_inPrecache;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponElite, DT_WeaponElite )

IMPLEMENT_REFLECT_TABLE( CWeaponElite, DT_WeaponElite );

#if defined CLIENT_DLL
#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CWeaponElite );
#endif
#endif

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


#ifdef CLIENT_DLL

	int CWeaponElite::GetMuzzleAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel )
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

#endif
