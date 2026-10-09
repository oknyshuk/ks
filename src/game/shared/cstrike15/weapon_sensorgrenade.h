#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	#define CSensorGrenade C_SensorGrenade
#endif

//-----------------------------------------------------------------------------
// SensorGrenade grenades
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_SensorGrenade" } ]]
      CSensorGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CSensorGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSensorGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_TAGRENADE; }

#if !defined( CLIENT_DLL )

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	virtual void ShotDetonate( CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
#endif

private:
	CSensorGrenade( const CSensorGrenade& );
};
