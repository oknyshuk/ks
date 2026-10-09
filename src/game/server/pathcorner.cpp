// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Used to create a path that can be followed by NPCs and trains.

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
#include "trains.h"
#include "entitylist.h"
#include "ndebugoverlay.h"

class CPathCorner : public CPointEntity
{
	DECLARE_CLASS( CPathCorner, CPointEntity );
public:

	void	Spawn( );
	float	GetDelay( void ) { return m_flWait; }
	int		DrawDebugTextOverlays(void);
	void	DrawDebugGeometryOverlays(void);

	// Input handlers	
	[[= ks::reflect::Input{ .name = "SetNextPathCorner", .type = FIELD_STRING } ]] void InputSetNextPathCorner( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "InPass", .type = FIELD_VOID } ]] void InputInPass( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "wait" } ]] float			m_flWait;
	[[= ks::reflect::Key{ .name = "OnPass" } ]] COutputEvent	m_OnPass;
};

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );


class CPathCornerCrash : public CPathCorner
{
	DECLARE_CLASS( CPathCornerCrash, CPathCorner );
};

LINK_ENTITY_TO_CLASS( path_corner_crash, CPathCornerCrash );


IMPLEMENT_REFLECT_DATAMAP( CPathCorner )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathCorner::Spawn( void )
{
	ASSERTSZ(GetEntityName() != NULL_STRING, "path_corner without a targetname");
}


//------------------------------------------------------------------------------
// Purpose: Sets the next path corner by name.
// Input  : String ID name of next path corner.
//-----------------------------------------------------------------------------
void CPathCorner::InputSetNextPathCorner( inputdata_t &inputdata )
{
	m_target = inputdata.value.StringID();
}


//-----------------------------------------------------------------------------
// Purpose: Fired by path followers as they pass the path corner.
//-----------------------------------------------------------------------------
void CPathCorner::InputInPass( inputdata_t &inputdata )
{
	m_OnPass.FireOutput( inputdata.pActivator, inputdata.pCaller, 0);
}


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CPathCorner::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// --------------
		// Print Target
		// --------------
		char tempstr[255];
		if (m_target!=NULL_STRING) 
		{
			V_snprintf(tempstr,sizeof(tempstr),"Target: %s",STRING(m_target));
		}
		else
		{
			V_strncpy(tempstr,"Target:   -  ",sizeof(tempstr));
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}


//-----------------------------------------------------------------------------
// Purpose: Override base class to add display of paths
//-----------------------------------------------------------------------------
void CPathCorner::DrawDebugGeometryOverlays(void) 
{
	// ----------------------------------------------
	// Draw line to next target is bbox is selected
	// ----------------------------------------------
	if (m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_ABSBOX_BIT))
	{
		NDebugOverlay::Box(GetAbsOrigin(), Vector(-10,-10,-10), Vector(10,10,10), 255, 100, 100, 0 ,0);

		if (m_target != NULL_STRING)
		{
			CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target );
			if (pTarget)
			{
				NDebugOverlay::Line(GetAbsOrigin(),pTarget->GetAbsOrigin(),255,100,100,true,0.0);
			}
		}
	}
	BaseClass::DrawDebugGeometryOverlays();
}
