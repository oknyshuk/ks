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
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "func_areaportalbase.h"

// A sphere around the player used for backface culling of areaportals.
constexpr auto VIEWER_PADDING = 80;


std::vector<CFuncAreaPortalBase*> g_AreaPortals;



//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncAreaPortalBase )




CFuncAreaPortalBase::CFuncAreaPortalBase()
{
	m_portalNumber = -1;
	g_AreaPortals.push_back( this );
	m_iPortalVersion = 0;
}


CFuncAreaPortalBase::~CFuncAreaPortalBase()
{
	ks::erase_first( g_AreaPortals, this );
}


bool CFuncAreaPortalBase::UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient )
{
	// NOTE: We leave bIsOpenOnClient alone on purpose here. See the header for a description of why.
	
	if( m_portalNumber == -1 )
		return false;

	// See if the viewer is on the backside.
	for ( int i = 0; i < std::ssize( vecOrigins ); ++i )
	{
		const Vector &vOrigin = vecOrigins[ i ];
		VPlane plane;
		if( !engine->GetAreaPortalPlane( vOrigin, m_portalNumber, &plane ) )
			return true; // leave it open if there's an error here for some reason

		// If either players' origin is on the front, the areaportal is considered opened...
		if( plane.DistTo( vOrigin ) + VIEWER_PADDING > 0 )
			return true;
	}

	return false;
}
