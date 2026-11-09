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
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "ai_navtype.h"
#include "ai_node.h"
#include "ai_waypoint.h"

inline constexpr int WAYPOINT_POOL_SIZE = 512;

//-----------------------------------------------------------------------------
// Init static variables
//-----------------------------------------------------------------------------

ks::fixed_pool AI_Waypoint_t::s_Allocator( sizeof(AI_Waypoint_t), WAYPOINT_POOL_SIZE, ks::fixed_pool::grow::fast, "AI_Waypoint_t pool", alignof(AI_Waypoint_t) );

//-------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( AI_Waypoint_t )

//-------------------------------------

AI_Waypoint_t::AI_Waypoint_t()
{
	std::memset( this, 0, sizeof(*this) );
	vecLocation	= vec3_invalid;
	iNodeID		= NO_NODE;
	flPathDistGoal = -1;
}

//-------------------------------------

AI_Waypoint_t::AI_Waypoint_t( const Vector &initPosition, float initYaw, Navigation_t initNavType, int initWaypointFlags, int initNodeID )
{
	std::memset( this, 0, sizeof(*this) );

	// A Route of length one to the endpoint
	vecLocation	= initPosition;
	flYaw		= initYaw;
	m_iWPType	= initNavType;
	m_fWaypointFlags = initWaypointFlags;
	iNodeID		= initNodeID;

	flPathDistGoal = -1;
}

//-------------------------------------

AI_Waypoint_t *	AI_Waypoint_t::GetLast()
{
	contract_assert( !pNext || pNext->pPrev == this ); 
	AI_Waypoint_t *pCurr = this;
	while (pCurr->GetNext())
	{
		pCurr = pCurr->GetNext();
	}

	return pCurr;
}


//-----------------------------------------------------------------------------

void CAI_WaypointList::RemoveAll()
{
	DeleteAll( &m_pFirstWaypoint );
	contract_assert( m_pFirstWaypoint == nullptr );
}

//-------------------------------------

void CAI_WaypointList::PrependWaypoints( AI_Waypoint_t *pWaypoints )
{
	AddWaypointLists( pWaypoints, GetFirst() );
	Set( pWaypoints );
}

//-------------------------------------

void CAI_WaypointList::PrependWaypoint( const Vector &newPoint, Navigation_t navType, unsigned waypointFlags, float flYaw )
{
	PrependWaypoints( new AI_Waypoint_t( newPoint, flYaw, navType, waypointFlags, NO_NODE ) );
}

//-------------------------------------

void CAI_WaypointList::Set(AI_Waypoint_t* route)
{
	m_pFirstWaypoint = route;
}

//-------------------------------------

AI_Waypoint_t *CAI_WaypointList::GetLast()
{
	AI_Waypoint_t *p = GetFirst();
	if (!p)
		return nullptr;
	while ( p->GetNext() )
		p = p->GetNext();

	return p;
}

//-------------------------------------

const AI_Waypoint_t *CAI_WaypointList::GetLast() const
{
	return const_cast<CAI_WaypointList *>(this)->GetLast();
}

//-------------------------------------

#ifdef DEBUG
void AssertRouteValid( AI_Waypoint_t* route )
{
	// Check that the goal wasn't just clobbered
	if (route) 
	{
		AI_Waypoint_t* waypoint = route;

		while (waypoint)
		{
			waypoint->AssertValid();
			waypoint = waypoint->GetNext();
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Deletes a waypoint linked list
//-----------------------------------------------------------------------------
void DeleteAll( AI_Waypoint_t *pWaypointList )
{
	while ( pWaypointList )
	{
		AI_Waypoint_t *pPrevWaypoint = pWaypointList;
		pWaypointList = pWaypointList->GetNext();
		delete pPrevWaypoint;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds addRoute to the end of oldRoute
//-----------------------------------------------------------------------------
void AddWaypointLists(AI_Waypoint_t *oldRoute, AI_Waypoint_t *addRoute)
{
	// Add to the end of the route
	AI_Waypoint_t *waypoint = oldRoute;

	while (waypoint->GetNext()) 
	{
		waypoint = waypoint->GetNext();
	}

	waypoint->ModifyFlags( bits_WP_TO_GOAL, false );

	// Check for duplication, but copy the type
	if (waypoint->iNodeID != NO_NODE			&&
		waypoint->iNodeID == addRoute->iNodeID	)
	{
//		waypoint->iWPType = addRoute->iWPType; <<TODO>> found case where this was bad
		AI_Waypoint_t *pNext = addRoute->GetNext();
		delete addRoute;
		waypoint->SetNext(pNext);
	}
	else
	{
		waypoint->SetNext(addRoute);
	}

	while (waypoint->GetNext()) 
	{
		waypoint = waypoint->GetNext();
	}

	waypoint->ModifyFlags( bits_WP_TO_GOAL, true );

}

//-----------------------------------------------------------------------------
