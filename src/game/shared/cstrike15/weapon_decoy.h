//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef WEAPON_DECOY_H
#define WEAPON_DECOY_H

#include "reflect_annotations.h"

#ifdef _WIN32
#pragma once
#endif

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	#define CDecoyGrenade C_DecoyGrenade
#endif

//-----------------------------------------------------------------------------
// Decoy grenades
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_DecoyGrenade" } ]]
      CDecoyGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CDecoyGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CDecoyGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_DECOY; }

#if !defined( CLIENT_DLL )

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
#endif

private:
	CDecoyGrenade( const CDecoyGrenade& );
};

#endif // WEAPON_DECOY_H
