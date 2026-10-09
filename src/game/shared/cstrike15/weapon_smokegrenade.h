#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	
	#define CSmokeGrenade C_SmokeGrenade

#endif

//-----------------------------------------------------------------------------
// Smoke grenades
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_SmokeGrenade" } ]]
      CSmokeGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CSmokeGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSmokeGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_SMOKEGRENADE; }

#ifdef CLIENT_DLL

#else

	void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );

#endif

	CSmokeGrenade( const CSmokeGrenade & ) {}
};
