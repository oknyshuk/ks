// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "ndebugoverlay.h"
#include "entitylist.h"

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission:public CPointEntity
{
public:
	DECLARE_CLASS( CInfoIntermission, CPointEntity );

	void Spawn_Impl( void );
	void Think_Impl( void );
};

void CInfoIntermission::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetLocalAngles( vec3_angle );
	SetNextThink( gpGlobals->curtime + 2 );// let targets spawn !
}

void CInfoIntermission::Think_Impl ( void )
{
	CBaseEntity *pTarget;

	// find my target
	pTarget = gEntList.FindEntityByName( nullptr, m_target );

	if ( pTarget )
	{
		Vector dir = pTarget->GetLocalOrigin() - GetLocalOrigin();
		VectorNormalize( dir );
		QAngle angles;
		VectorAngles( dir, angles );
		SetLocalAngles( angles );
	}
}

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );
