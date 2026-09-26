//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "func_areaportalbase.h"
#include "tier1/vector_ops.h"

// A sphere around the player used for backface culling of areaportals.
#define VIEWER_PADDING	80


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




