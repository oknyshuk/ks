#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	#define CFlashbang C_Flashbang
#endif

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_Flashbang" } ]]
      CFlashbang : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CFlashbang, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CFlashbang() {}

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_FLASHBANG; }


#ifdef CLIENT_DLL

#else

		virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
#endif

	CFlashbang( const CFlashbang & ) {}
};
