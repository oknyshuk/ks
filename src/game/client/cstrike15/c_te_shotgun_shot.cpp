// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_baseplayer.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "fx_cs_shared.h"
#include "c_cs_player.h"
#include "c_basetempentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_TEFireBullets", .base = false } ]]
      C_TEFireBullets : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEFireBullets, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEFireBullets();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_iPlayer;
	[[= ks::reflect::Net{} ]] uint16			m_nItemDefIndex;
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] QAngle			m_vecAngles;
	[[= ks::reflect::Net{} ]] CSWeaponID		m_iWeaponID;
	[[= ks::reflect::Net{} ]] int				m_iMode;
	[[= ks::reflect::Net{} ]] int				m_iSeed;
	[[= ks::reflect::Net{} ]] float			m_fInaccuracy;
	[[= ks::reflect::Net{} ]] float			m_flRecoilIndex;
	[[= ks::reflect::Net{} ]] float			m_fSpread;
	[[= ks::reflect::Net{} ]] WeaponSound_t	m_iSoundType;
};

C_TEFireBullets::C_TEFireBullets()
{
	m_iPlayer = 0;
	m_nItemDefIndex = 0;
	m_iWeaponID = WEAPON_NONE;
	m_iMode = 0;
	m_iSeed = 0;
	m_fInaccuracy = 0;
	m_flRecoilIndex = 0;
	m_fSpread = 0;
	m_iSoundType = EMPTY;
}

void C_TEFireBullets::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.

	//
	// Vitaliy: Aug-2013 economy update note
	// fields m_nItemDefIndex and m_iSoundType are new and are not
	// networked in the previous demo recordings, force the sound type
	// to the default SINGLE value if it didn't network correctly
	//
	contract_assert( ( m_iSoundType == EMPTY ) ||	// pre-economy update ver. 1.23.1.4
		( m_iSoundType == SINGLE ) ||	// default sound for firing
		( m_iSoundType == SPECIAL1 ) ||	// silenced weapons use this sound
		0 );
	WeaponSound_t wpnSoundToUse = SINGLE;
	switch ( m_iSoundType )
	{
	case SPECIAL1:
		wpnSoundToUse = SPECIAL1;
		break;
	}
	
	m_vecAngles.z = 0;
	
	FX_FireBullets( 
		m_iPlayer+1,
		m_nItemDefIndex,
		m_vecOrigin,
		m_vecAngles,
		m_iWeaponID,
		m_iMode,
		m_iSeed,
		m_fInaccuracy,
		m_fSpread,
		0.0f,
		0,
		wpnSoundToUse,
		m_flRecoilIndex
		);
}


IMPLEMENT_CLIENTCLASS_EVENT( C_TEFireBullets, DT_TEFireBullets, CTEFireBullets );


IMPLEMENT_REFLECT_TABLE( C_TEFireBullets, DT_TEFireBullets );


class [[= ks::reflect::NetTable{ .name = "DT_TEPlantBomb", .base = false } ]]
      C_TEPlantBomb : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlantBomb, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int		m_iPlayer;
	[[= ks::reflect::Net{} ]] Vector	m_vecOrigin;
	[[= ks::reflect::Net{} ]] PlantBombOption_t	m_option;
};


void C_TEPlantBomb::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.
	FX_PlantBomb( m_iPlayer+1, m_vecOrigin, m_option );
}


IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlantBomb, DT_TEPlantBomb, CTEPlantBomb );


IMPLEMENT_REFLECT_TABLE( C_TEPlantBomb, DT_TEPlantBomb );
