// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "base_transmit_proxy.h"
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
#include "entityblocker.h"

LINK_ENTITY_TO_CLASS( entity_blocker, CEntityBlocker );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&mins - 
//			&maxs - 
//			NULL - 
// Output : CEntityBlocker
//-----------------------------------------------------------------------------
CEntityBlocker *CEntityBlocker::Create( const Vector &origin, const Vector &mins, const Vector &maxs, CBaseEntity *pOwner, bool bBlockPhysics )
{
	CEntityBlocker *pBlocker = (CEntityBlocker *) CBaseEntity::Create( "entity_blocker", origin, vec3_angle, pOwner );
	
	if ( pBlocker != nullptr )
	{
		pBlocker->SetSize( mins, maxs );
		if ( bBlockPhysics )
		{
			pBlocker->VPhysicsInitStatic();
		}
	}

	return pBlocker;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityBlocker::Spawn( void )
{
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST );
}

//-----------------------------------------------------------------------------
// Purpose: Entity blockers don't block tracelines so they don't screw up weapon fire, etc
//-----------------------------------------------------------------------------
bool CEntityBlocker::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	return false;
}

//------------------------------------------------------------------------------
// Purpose : 
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_Test_Entity_Blocker( void )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	Vector vecForward;
	pPlayer->GetVectors( &vecForward, nullptr, nullptr );

	trace_t tr;
	Vector vecOrigin = pPlayer->GetAbsOrigin() + (vecForward * 256);
	UTIL_TraceHull( vecOrigin + Vector(0,0,256), vecOrigin - Vector(0,0,256), VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( !tr.allsolid && !tr.startsolid )
	{
		CEntityBlocker::Create( tr.endpos, VEC_HULL_MIN, VEC_HULL_MAX, nullptr, true );
		NDebugOverlay::Box( tr.endpos, VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 64, 1000.0 );
	}
}
static ConCommand test_entity_blocker("test_entity_blocker", CC_Test_Entity_Blocker, "Test command that drops an entity blocker out in front of the player.", FCVAR_CHEAT );
