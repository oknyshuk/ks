#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	
	#define CHEGrenade C_HEGrenade

#endif

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_HEGrenade" } ]]
      CHEGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CHEGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHEGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_HEGRENADE; }

#ifdef CLIENT_DLL

#else

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	
#endif

	CHEGrenade( const CHEGrenade & ) {}
};
