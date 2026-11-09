// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An effect for a single bullet passing through a body of water.
//			The slug quickly decelerates, leaving a trail of bubbles behind it.
//			TODO: make clientside

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
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "waterbullet.h"
#include "ndebugoverlay.h"

constexpr auto WATERBULLET_INITIAL_SPEED = 1000.0;
constexpr auto WATERBULLET_STOP_TIME = 0.5; // how long it takes a bullet in water to come to a stop!

inline constexpr auto WATERBULLET_DECAY = ( WATERBULLET_INITIAL_SPEED / WATERBULLET_STOP_TIME );


LINK_ENTITY_TO_CLASS( waterbullet, CWaterBullet );

IMPLEMENT_REFLECT_SERVERCLASS( CWaterBullet, DT_WaterBullet )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Precache_Impl()
{
	PrecacheModel( "models/weapons/w_bullet.mdl" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Spawn( const Vector &vecOrigin, const Vector &vecDir )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetModel( "models/weapons/w_bullet.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetMoveType( MOVETYPE_FLY );

	SetGravity( 0.0 );

	QAngle angles;
	SetAbsOrigin( vecOrigin );
	
	SetAbsVelocity( vecDir * 1500.0f );
	VectorAngles( GetAbsVelocity(), angles );
	SetAbsAngles( angles );

	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	SetTouch( &CWaterBullet::Touch );

	SetThink( &CWaterBullet::BulletThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWaterBullet::BulletThink()
{
	//NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() - GetAbsVelocity() * 0.1, 255, 255, 255, false, 1 );
	SetNextThink( gpGlobals->curtime + 0.05 );

/*
	QAngle angles = GetAbsAngles();
	angles.x += random->RandomInt( -6, 6 );
	angles.y += random->RandomInt( -6, 6 );
	SetAbsAngles( angles );
*/

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	SetAbsVelocity( forward * 1500.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Touch_Impl( CBaseEntity *pOther )
{
	Vector	vecDir = GetAbsVelocity();
	float speed = VectorNormalize( vecDir );

	Vector	vecStart = GetAbsOrigin() - ( vecDir * 8 );
	Vector	vecEnd = GetAbsOrigin() + ( vecDir * speed );

	trace_t	tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, nullptr, &tr );
	UTIL_ImpactTrace( &tr, DMG_BULLET );

	UTIL_Remove( this );
}
