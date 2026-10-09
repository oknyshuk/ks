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
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "game.h"
#include "ai_looktarget.h"

// Mothballing this entity to get rid of it. info_hint does its job better (sjb)
//LINK_ENTITY_TO_CLASS( ai_looktarget, CAI_LookTarget );

IMPLEMENT_REFLECT_DATAMAP( CAI_LookTarget )

//---------------------------------------------------------
//---------------------------------------------------------
int CAI_LookTarget::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_BBOX_BIT)
	{
		int color = random->RandomInt( 50, 255 );
		NDebugOverlay::Cross3D( GetAbsOrigin(), 12, color, color, color, false, 0.1 );
	}

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		if( !IsEnabled() )
		{
			V_snprintf(tempstr,sizeof(tempstr),"DISABLED" );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		if( IsEligible( nullptr ) )
		{
			V_snprintf(tempstr,sizeof(tempstr),"Eligible" );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
		else
		{
			V_snprintf(tempstr,sizeof(tempstr),"NOT Eligible for selection");
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
	}
	return text_offset;
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_LookTarget::IsEligible( CBaseEntity *pLooker )
{
	if( !IsEnabled() )
		return false;

	if( !IsAvailable() )
		return false;

	if( pLooker )
	{
		float maxdistsqr = m_flMaxDist * m_flMaxDist;
		
		Vector vecPos = GetAbsOrigin();

		if( vecPos.DistToSqr( pLooker->WorldSpaceCenter() ) > maxdistsqr )
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------
// Someone's reserving this entity because they're going 
// to attempt to look at it for flDuration seconds. We'll
// make it unavailable to anyone else for that time.
//---------------------------------------------------------
void CAI_LookTarget::Reserve( float flDuration )
{
	m_flTimeNextAvailable = gpGlobals->curtime + flDuration;

	if( HasSpawnFlags( SF_LOOKTARGET_ONLYONCE ) )
	{
		// No one will look at this again.
		Disable();
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
CAI_LookTarget *CAI_LookTarget::GetFirstLookTarget()
{
	CBaseEntity		*pEnt;

	string_t iszLookTarget = FindPooledString( "ai_looktarget" );
	if( iszLookTarget == NULL_STRING )
	{
		return nullptr;
	}

	pEnt = gEntList.FirstEnt();
	while( pEnt && pEnt->m_iClassname != iszLookTarget )
	{
		pEnt = gEntList.NextEnt( pEnt );
	}

	return (CAI_LookTarget*)pEnt;
}

//---------------------------------------------------------
//---------------------------------------------------------
CAI_LookTarget *CAI_LookTarget::GetNextLookTarget( CAI_LookTarget *pCurrentTarget )
{
	CBaseEntity		*pEnt;

	string_t iszLookTarget = FindPooledString( "ai_looktarget" );
	if( iszLookTarget == NULL_STRING )
	{
		return nullptr;
	}

	pEnt = gEntList.NextEnt( pCurrentTarget );
	while( pEnt && pEnt->m_iClassname != iszLookTarget )
	{
		pEnt = gEntList.NextEnt( pEnt );
	}

	return (CAI_LookTarget*)pEnt;
}
