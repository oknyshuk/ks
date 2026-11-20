// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdlib.h>
#include "engine/ivdebugoverlay.h"
#include "collisionutils.h"
#include "eiface.h"
#include "networkvar.h"
#include "tier2/resourceprecacher.h"
#include "declare_class.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "ai_criteria.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
module ks.server;

import std;
import ks.core.color;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :nav_4;
import :sharedvar;
import :baseentity;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.math.mathlib;
import ks.math.vector2d;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :vscript;
import :fish;
import :player;
import :entityutil;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.tier1.fmtstr;
import :entity_dispatch;
import :entity_kind;

using namespace ::ks::reflect;

// Central point for defining colors and drawing routines for Navigation Mesh edit mode
// Author: Matthew Campbell, 2004

//--------------------------------------------------------------------------------------------------------------
/**
 * This MUST be kept in sync with the NavEditColor definition
 */
Color NavColors[] =
{
	// Degenerate area colors
	Color( 255, 255, 255 ),		// NavDegenerateFirstColor
	Color( 255, 0, 255 ),		// NavDegenerateSecondColor

	// Place painting color
	Color( 0, 255, 0 ),			// NavSamePlaceColor
	Color( 0, 0, 255 ),			// NavDifferentPlaceColor
	Color( 255, 0, 0 ),			// NavNoPlaceColor

	// Normal colors
	Color( 255, 255, 0 ),		// NavSelectedColor
	Color( 0, 255, 255 ),		// NavMarkedColor
	Color( 255, 0, 0 ),			// NavNormalColor
	Color( 0, 0, 255 ),			// NavCornerColor
	Color( 0, 0, 255 ),			// NavBlockedByDoorColor
	Color( 0, 255, 255 ),		// NavBlockedByFuncNavBlockerColor

	// Hiding spot colors
	Color( 255, 0, 0 ),			// NavIdealSniperColor
	Color( 255, 0, 255 ),		// NavGoodSniperColor
	Color( 0, 255, 0 ),			// NavGoodCoverColor
	Color( 255, 0, 255 ),		// NavExposedColor
	Color( 255, 100, 0 ),		// NavApproachPointColor

	// Connector colors
	Color( 0, 255, 255 ),		// NavConnectedTwoWaysColor
	Color( 0, 0, 255 ),			// NavConnectedOneWayColor
	Color( 0, 255, 0 ),			// NavConnectedContiguous
	Color( 255, 0, 0 ),			// NavConnectedNonContiguous

	// Editing colors
	Color( 255, 255, 255 ),		// NavCursorColor
	Color( 255, 255, 255 ),		// NavSplitLineColor
	Color( 0, 255, 255 ),		// NavCreationColor
	Color( 255, 0, 0 ),			// NavInvalidCreationColor
	Color( 0, 64, 64 ),			// NavGridColor
	Color( 255, 255, 255 ),		// NavDragSelectionColor

	// Nav attribute colors
	Color( 0, 0, 255 ),			// NavAttributeCrouchColor
	Color( 0, 255, 0 ),			// NavAttributeJumpColor
	Color( 0, 255, 0 ),			// NavAttributePreciseColor
	Color( 255, 0, 0 ),			// NavAttributeNoJumpColor
	Color( 255, 0, 0 ),			// NavAttributeStopColor
	Color( 0, 0, 255 ),			// NavAttributeRunColor
	Color( 0, 255, 0 ),			// NavAttributeWalkColor
	Color( 255, 0, 0 ),			// NavAttributeAvoidColor
	Color( 0, 200, 0 ),			// NavAttributeStairColor
};

//--------------------------------------------------------------------------------------------------------------
void NavDrawLine( const Vector& from, const Vector& to, NavEditColor navColor )
{
	const Vector offset( 0, 0, 1 );

	Color color = NavColors[navColor];
	NDebugOverlay::Line( from + offset, to + offset, color[0]/2, color[1]/2, color[2]/2, true, 0.1f );
	NDebugOverlay::Line( from + offset, to + offset, color[0], color[1], color[2], false, 0.15f );
}

//--------------------------------------------------------------------------------------------------------------
void NavDrawTriangle( const Vector& point1, const Vector& point2, const Vector& point3, NavEditColor navColor )
{
	NavDrawLine( point1, point2, navColor );
	NavDrawLine( point2, point3, navColor );
	NavDrawLine( point1, point3, navColor );
}

//--------------------------------------------------------------------------------------------------------------
void NavDrawFilledTriangle( const Vector& point1, const Vector& point2, const Vector& point3, NavEditColor navColor, bool dark )
{
	Color color = NavColors[navColor];
	if ( dark )
	{
		color[0] = color[0] / 2;
		color[1] = color[1] / 2;
		color[2] = color[2] / 2;
	}
	NDebugOverlay::Triangle( point1, point2, point3, color[0], color[1], color[2], 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

//--------------------------------------------------------------------------------------------------------------
void NavDrawHorizontalArrow( const Vector& from, const Vector& to, float width, NavEditColor navColor )
{
	const Vector offset( 0, 0, 1 );

	Color color = NavColors[navColor];
	NDebugOverlay::HorzArrow( from + offset, to + offset, width, color[0]/2, color[1]/2, color[2]/2, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	NDebugOverlay::HorzArrow( from + offset, to + offset, width, color[0], color[1], color[2], 255, false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

//--------------------------------------------------------------------------------------------------------------
void NavDrawDashedLine( const Vector& from, const Vector& to, NavEditColor navColor )
{
	const Vector offset( 0, 0, 1 );

	Color color = NavColors[navColor];

	const float solidLen = 7.0f;
	const float   gapLen = 3.0f;

	Vector unit = (to - from);
	const float totalDistance = unit.NormalizeInPlace();

	float distance = 0.0f;

	while ( distance < totalDistance )
	{
		Vector start = from + unit * distance;
		float endDistance = distance + solidLen;
		endDistance = std::min( endDistance, totalDistance );
		Vector end = from + unit * endDistance;

		distance += solidLen + gapLen;

		NDebugOverlay::Line( start + offset, end + offset, color[0]/2, color[1]/2, color[2]/2, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		NDebugOverlay::Line( start + offset, end + offset, color[0], color[1], color[2], false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	}
}

//--------------------------------------------------------------------------------------------------------------
void NavDrawVolume( const Vector &vMin, const Vector &vMax, int zMidline, NavEditColor navColor )
{
	// Center rectangle
	NavDrawLine( Vector( vMax.x, vMax.y, zMidline ), Vector( vMin.x, vMax.y, zMidline ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, zMidline ), Vector( vMin.x, vMax.y, zMidline ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, zMidline ), Vector( vMax.x, vMin.y, zMidline ),	navColor );
	NavDrawLine( Vector( vMax.x, vMax.y, zMidline ), Vector( vMax.x, vMin.y, zMidline ),	navColor );

	// Bottom rectangle
	NavDrawLine( Vector( vMax.x, vMax.y, vMin.z ), Vector( vMin.x, vMax.y, vMin.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, vMin.z ), Vector( vMin.x, vMax.y, vMin.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, vMin.z ), Vector( vMax.x, vMin.y, vMin.z ),	navColor );
	NavDrawLine( Vector( vMax.x, vMax.y, vMin.z ), Vector( vMax.x, vMin.y, vMin.z ),	navColor );

	// Top rectangle
	NavDrawLine( Vector( vMax.x, vMax.y, vMax.z ), Vector( vMin.x, vMax.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, vMax.z ), Vector( vMin.x, vMax.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, vMax.z ), Vector( vMax.x, vMin.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMax.x, vMax.y, vMax.z ), Vector( vMax.x, vMin.y, vMax.z ),	navColor );

	// Edges
	NavDrawLine( Vector( vMax.x, vMax.y, vMin.z ), Vector( vMax.x, vMax.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMin.y, vMin.z ), Vector( vMin.x, vMin.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMax.x, vMin.y, vMin.z ), Vector( vMax.x, vMin.y, vMax.z ),	navColor );
	NavDrawLine( Vector( vMin.x, vMax.y, vMin.z ), Vector( vMin.x, vMax.y, vMax.z ),	navColor );
}

//--------------------------------------------------------------------------------------------------------------

class CBaseEntity;
class CBasePlayer;
class CNavArea;
class CNavLadder;
class CNavMesh;
class CNavNode;
class CWorld;
struct Extent;

using namespace ::ks::reflect;

// nav_edit.cpp
// Implementation of Navigation Mesh edit mode
// Author: Michael Booth, 2003-2004

ConVar nav_show_area_info( side_registry(), "nav_show_area_info", "0.5", FCVAR_CHEAT, "Duration in seconds to show nav area ID and attributes while editing" );
ConVar nav_snap_to_grid( side_registry(), "nav_snap_to_grid", "0", FCVAR_CHEAT, "Snap to the nav generation grid when creating new nav areas" );
ConVar nav_create_place_on_ground( side_registry(), "nav_create_place_on_ground", "0", FCVAR_CHEAT, "If true, nav areas will be placed flush with the ground when created by hand." );
#ifdef DEBUG
ConVar nav_draw_limit( side_registry(), "nav_draw_limit", "50", FCVAR_CHEAT, "The maximum number of areas to draw in edit mode" );
#else
ConVar nav_draw_limit( side_registry(), "nav_draw_limit", "500", FCVAR_CHEAT, "The maximum number of areas to draw in edit mode" );
#endif
ConVar nav_solid_props( side_registry(), "nav_solid_props", "0", FCVAR_CHEAT, "Make props solid to nav generation/editing" );
ConVar nav_create_area_at_feet( side_registry(), "nav_create_area_at_feet", "0", FCVAR_CHEAT, "Anchor nav_begin_area Z to editing player's feet" );

ConVar nav_drag_selection_volume_zmax_offset( side_registry(), "nav_drag_selection_volume_zmax_offset", "32", FCVAR_REPLICATED, "The offset of the nav drag volume top from center" );
ConVar nav_drag_selection_volume_zmin_offset( side_registry(), "nav_drag_selection_volume_zmin_offset", "32", FCVAR_REPLICATED, "The offset of the nav drag volume bottom from center" );

extern void GetNavUIEditVectors( Vector *pos, Vector *forward );

Color s_dragSelectionSetAddColor( 100, 255, 100, 96 );
Color s_dragSelectionSetDeleteColor( 255, 100, 100, 96 );

#if DEBUG_NAV_NODES
extern ConVar nav_show_nodes;
#endif // DEBUG_NAV_NODES

//--------------------------------------------------------------------------------------------------------------

PRECACHE_REGISTER_BEGIN( GLOBAL, EditNav_Precache )
	PRECACHE( GAMESOUND, "Bot.EditSwitchOn" )
	PRECACHE( GAMESOUND, "EDIT_TOGGLE_PLACE_MODE" )
	PRECACHE( GAMESOUND, "Bot.EditSwitchOff" )
	PRECACHE( GAMESOUND, "EDIT_PLACE_PICK" )
	PRECACHE( GAMESOUND, "EDIT_DELETE" )
	PRECACHE( GAMESOUND, "EDIT.ToggleAttribute" )
	PRECACHE( GAMESOUND, "EDIT_SPLIT.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_SPLIT.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_MERGE.Enable" )
	PRECACHE( GAMESOUND, "EDIT_MERGE.Disable" )
	PRECACHE( GAMESOUND, "EDIT_MARK.Enable" )
	PRECACHE( GAMESOUND, "EDIT_MARK.Disable" )
	PRECACHE( GAMESOUND, "EDIT_MARK_UNNAMED.Enable" )
	PRECACHE( GAMESOUND, "EDIT_MARK_UNNAMED.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_MARK_UNNAMED.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_CONNECT.AllDirections" )
	PRECACHE( GAMESOUND, "EDIT_CONNECT.Added" )
	PRECACHE( GAMESOUND, "EDIT_DISCONNECT.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_DISCONNECT.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_SPLICE.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_SPLICE.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_SELECT_CORNER.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_SELECT_CORNER.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_MOVE_CORNER.MarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_MOVE_CORNER.NoMarkedArea" )
	PRECACHE( GAMESOUND, "EDIT_BEGIN_AREA.Creating" )
	PRECACHE( GAMESOUND, "EDIT_BEGIN_AREA.NotCreating" )
	PRECACHE( GAMESOUND, "EDIT_END_AREA.Creating" )
	PRECACHE( GAMESOUND, "EDIT_END_AREA.NotCreating" )
	PRECACHE( GAMESOUND, "EDIT_WARP_TO_MARK" )
PRECACHE_REGISTER_END()

//--------------------------------------------------------------------------------------------------------------
int GetGridSize( bool forceGrid = false )
{
	if ( TheNavMesh->IsGenerating() )
	{
		return (int)GenerationStepSize;
	}

	int snapVal = nav_snap_to_grid.GetInt();
	if ( forceGrid && !snapVal )
	{
		snapVal = 1;
	}

	if ( snapVal == 0 )
	{
		return 0;
	}

	int scale = (int)GenerationStepSize;
	switch ( snapVal )
	{
	case 3:
		scale = 1;
		break;
	case 2:
		scale = 5;
		break;
	case 1:
	default:
		break;
	}

	return scale;
}

//--------------------------------------------------------------------------------------------------------------
Vector CNavMesh::SnapToGrid( const Vector& in, bool snapX, bool snapY, bool forceGrid ) const
{
	int scale = GetGridSize( forceGrid );
	if ( !scale )
	{
		return in;
	}

	Vector out( in );

	if ( snapX )
	{
		out.x = RoundToUnits( in.x, scale );
	}

	if ( snapY )
	{
		out.y = RoundToUnits( in.y, scale );
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------
float CNavMesh::SnapToGrid( float x, bool forceGrid ) const
{
	int scale = GetGridSize( forceGrid );
	if ( !scale )
	{
		return x;
	}

	x = RoundToUnits( x, scale );
	return x;
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::GetEditVectors( Vector *pos, Vector *forward )
{
	if ( !pos || !forward )
	{
		return;
	}

	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( !player )
	{
		return;
	}

	//DOTA places the edit cursor where the 2D cursor is located.
	Vector dir;
	AngleVectors( player->EyeAngles() + player->GetViewPunchAngle(), forward );

	*pos = player->EyePosition();

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Change the edit mode
 */
void CNavMesh::SetEditMode( EditModeType mode )
{
	m_markedLadder = nullptr;
	m_markedArea = nullptr;
	m_markedCorner = NUM_CORNERS;
	
	m_editMode = mode;

	m_isContinuouslySelecting = false;
	m_isContinuouslyDeselecting = false;
	m_bIsDragDeselecting = false;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavMesh::FindNavAreaOrLadderAlongRay( const Vector &start, const Vector &end, CNavArea **bestArea, CNavLadder **bestLadder, CNavArea *ignore )
{
	if ( m_grid.empty() )
		return false;

	Ray_t ray;
	ray.Init( start, end, vec3_origin, vec3_origin );

	*bestArea = nullptr;
	*bestLadder = nullptr;

	float bestDist = 1.0f; // 0..1 fraction

	for ( int i=0; i<std::ssize( m_ladders ); ++i )
	{
		CNavLadder *ladder = m_ladders[i];

		Vector left( 0, 0, 0), right(0, 0, 0), up( 0, 0, 0);
		VectorVectors( ladder->GetNormal(), right, up );
		right *= ladder->m_width * 0.5f;
		left = -right;

		Vector c1 = ladder->m_top + right;
		Vector c2 = ladder->m_top + left;
		Vector c3 = ladder->m_bottom + right;
		Vector c4 = ladder->m_bottom + left;
		float dist = IntersectRayWithTriangle( ray, c1, c2, c4, false );
		if ( dist > 0 && dist < bestDist )
		{
			*bestLadder = ladder;
			bestDist = dist;
		}

		dist = IntersectRayWithTriangle( ray, c1, c4, c3, false );
		if ( dist > 0 && dist < bestDist )
		{
			*bestLadder = ladder;
			bestDist = dist;
		}
	}

	Extent extent;
	extent.lo = extent.hi = start;
	extent.Encompass( end );

	int loX = WorldToGridX( extent.lo.x );
	int loY = WorldToGridY( extent.lo.y );
	int hiX = WorldToGridX( extent.hi.x );
	int hiY = WorldToGridY( extent.hi.y );

	for( int y = loY; y <= hiY; ++y )
	{
		for( int x = loX; x <= hiX; ++x )
		{
			NavAreaVector &areaGrid = m_grid[ x + y*m_gridSizeX ];

			for ( CNavArea *area : areaGrid )
			{
				if ( area == ignore )
					continue;

				Vector nw = area->m_nwCorner;
				Vector se = area->m_seCorner;
				Vector ne, sw;
				ne.x = se.x;
				ne.y = nw.y;
				ne.z = area->m_neZ;
				sw.x = nw.x;
				sw.y = se.y;
				sw.z = area->m_swZ;

				float dist = IntersectRayWithTriangle( ray, nw, ne, se, false );
				if ( dist > 0 && dist < bestDist )
				{
					*bestArea = area;
					bestDist = dist;
				}

				dist = IntersectRayWithTriangle( ray, se, sw, nw, false );
				if ( dist > 0 && dist < bestDist )
				{
					*bestArea = area;
					bestDist = dist;
				}
			}
		}
	}

	if ( *bestArea )
	{
		*bestLadder = nullptr;
	}

	return bestDist < 1.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Convenience function to find the nav area a player is looking at, for editing commands
 */
bool CNavMesh::FindActiveNavArea( void )
{
	CVProfScope _( "CNavMesh::FindActiveNavArea", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	m_splitAlongX = false;
	m_splitEdge = 0.0f;
	m_selectedArea = nullptr;
	m_climbableSurface = false;
	m_selectedLadder = nullptr;

	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( player == nullptr )
		return false;

	Vector from, dir;
	GetEditVectors( &from, &dir );

	float maxRange = 2000.0f;		// 500
	bool isClippingRayAtFeet = false;
	if ( nav_create_area_at_feet.GetBool() )
	{
		if ( dir.z < 0 )
		{
			float eyeHeight = player->GetViewOffset().z;
			if ( eyeHeight != 0.0f )
			{
				float rayHeight = -dir.z * maxRange;
				maxRange = maxRange * eyeHeight / rayHeight;
				isClippingRayAtFeet = true;
			}
		}
	}

	Vector to = from + maxRange * dir;

	trace_t result;
	CTraceFilterWalkableEntities filter( nullptr, COLLISION_GROUP_NONE, WALK_THRU_EVERYTHING );
	UTIL_TraceLine( from, to, (nav_solid_props.GetBool()) ? MASK_NPCSOLID : MASK_NPCSOLID_BRUSHONLY, &filter, &result );

	if (result.fraction != 1.0f)
	{
		if ( !IsEditMode( CREATING_AREA ) )
		{
			m_climbableSurface = physprops->GetSurfaceData( result.surface.surfaceProps )->game.climbable != 0;
			if ( !m_climbableSurface )
			{
				m_climbableSurface = (result.contents & CONTENTS_LADDER) != 0;
			}
			m_surfaceNormal = result.plane.normal;

			if ( m_climbableSurface )
			{
				// check if we're on the same plane as the original point when we're building a ladder
				if ( IsEditMode( CREATING_LADDER ) )
				{
					if ( m_surfaceNormal != m_ladderNormal )
					{
						m_climbableSurface = false;
					}
				}

				if ( m_surfaceNormal.z > 0.9f )
				{
					m_climbableSurface = false; // don't try to build ladders on flat ground
				}
			}
		}

		if ( ( m_climbableSurface && !IsEditMode( CREATING_LADDER ) ) || !IsEditMode( CREATING_AREA ) )
		{
			float closestDistSqr = 200.0f * 200.0f;

			for ( int i=0; i<std::ssize( m_ladders ); ++i )
			{
				CNavLadder *ladder = m_ladders[i];

				Vector absMin = ladder->m_bottom;
				Vector absMax = ladder->m_top;

				Vector left( 0, 0, 0), right(0, 0, 0), up( 0, 0, 0);
				VectorVectors( ladder->GetNormal(), right, up );
				right *= ladder->m_width * 0.5f;
				left = -right;

				absMin.x += std::min( left.x, right.x );
				absMin.y += std::min( left.y, right.y );

				absMax.x += std::max( left.x, right.x );
				absMax.y += std::max( left.y, right.y );

				Extent e;
				e.lo = absMin + Vector( -5, -5, -5 );
				e.hi = absMax + Vector( 5, 5, 5 );

				if ( e.Contains( m_editCursorPos ) )
				{
					m_selectedLadder = ladder;
					break;
				}

				if ( !m_climbableSurface )
					continue;

				Vector p1 = (ladder->m_bottom + ladder->m_top)/2;
				Vector p2 = m_editCursorPos;
				float distSqr = p1.DistToSqr( p2 );

				if ( distSqr < closestDistSqr )
				{
					m_selectedLadder = ladder;
					closestDistSqr = distSqr;
				}
			}
		}

		m_editCursorPos = result.endpos;

		// find the area the player is pointing at
		if ( !m_climbableSurface && !m_selectedLadder )
		{
			// Try to clip our trace to nav areas
			FindNavAreaOrLadderAlongRay( result.startpos, result.endpos + 100.0f * dir, &m_selectedArea, &m_selectedLadder ); // extend a few units into the ground

			// Failing that, get the closest area to the endpoint
			if ( !m_selectedArea && !m_selectedLadder )
			{
				m_selectedArea = TheNavMesh->GetNearestNavArea( result.endpos, false, 500.0f );
			}
		}

		if ( m_selectedArea )
		{
			float yaw = player->EyeAngles().y;
			while( yaw > 360.0f )
				yaw -= 360.0f;

			while( yaw < 0.0f )
				yaw += 360.0f;

			if ((yaw < 45.0f || yaw > 315.0f) || (yaw > 135.0f && yaw < 225.0f))
			{
				m_splitEdge = SnapToGrid( result.endpos.y, true );
				m_splitAlongX = true;
			}
			else
			{
				m_splitEdge = SnapToGrid( result.endpos.x, true );
				m_splitAlongX = false;
			}
		}

		if ( !m_climbableSurface && !IsEditMode( CREATING_LADDER ) )
		{
			m_editCursorPos = SnapToGrid( m_editCursorPos );
		}

		return true;
	}
	else if ( isClippingRayAtFeet )
	{
		m_editCursorPos = SnapToGrid( result.endpos );
	}

	if ( IsEditMode( CREATING_LADDER ) || IsEditMode( CREATING_AREA ) )
		return false;

	// We started solid.  Look for areas in front of us.
	FindNavAreaOrLadderAlongRay( from, to, &m_selectedArea, &m_selectedLadder );

	return (m_selectedArea != nullptr || m_selectedLadder != nullptr || isClippingRayAtFeet);
}

//--------------------------------------------------------------------------------------------------------------
bool CNavMesh::FindLadderCorners( Vector *corner1, Vector *corner2, Vector *corner3 )
{
	if ( !corner1 || !corner2 || !corner3 )
		return false;

	Vector ladderRight, ladderUp;
	VectorVectors( m_ladderNormal, ladderRight, ladderUp );

	Vector from, dir;
	GetEditVectors( &from, &dir );

	const float maxDist = 100000.0f;

	Ray_t ray;
	ray.Init( from, from + dir * maxDist, vec3_origin, vec3_origin );

	*corner1 = m_ladderAnchor + ladderUp * maxDist + ladderRight * maxDist;
	*corner2 = m_ladderAnchor + ladderUp * maxDist - ladderRight * maxDist;
	*corner3 = m_ladderAnchor - ladderUp * maxDist - ladderRight * maxDist;
	float dist = IntersectRayWithTriangle( ray, *corner1, *corner2, *corner3, false );
	if ( dist < 0 )
	{
		*corner2 = m_ladderAnchor - ladderUp * maxDist + ladderRight * maxDist;
		dist = IntersectRayWithTriangle( ray, *corner1, *corner2, *corner3, false );
	}

	*corner3 = m_editCursorPos;
	if ( dist > 0 && dist < maxDist )
	{
		*corner3 = from + dir * dist * maxDist;

		float vertDistance = corner3->z - m_ladderAnchor.z;
		float val = vertDistance / ladderUp.z;

		*corner1 = m_ladderAnchor + val * ladderUp;
		*corner2 = *corner3 - val * ladderUp;

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool CheckForClimbableSurface( const Vector &start, const Vector &end )
{
	trace_t result;
	UTIL_TraceLine( start, end, MASK_NPCSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );

	bool climbableSurface = false;
	if (result.fraction != 1.0f)
	{
		climbableSurface = physprops->GetSurfaceData( result.surface.surfaceProps )->game.climbable != 0;
		if ( !climbableSurface )
		{
			climbableSurface = (result.contents & CONTENTS_LADDER) != 0;
		}
	}

	return climbableSurface;
}

//--------------------------------------------------------------------------------------------------------------
void StepAlongClimbableSurface( Vector &pos, const Vector &increment, const Vector &probe )
{
	while ( CheckForClimbableSurface( pos + increment - probe, pos + increment + probe ) )
	{
		pos += increment;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavBuildLadder( void )
{
	if ( !IsEditMode( NORMAL ) || !m_climbableSurface )
	{
		return;
	}

	// We've got a ladder at m_editCursorPos, with a normal of m_surfaceNormal
	Vector right, up;
	VectorVectors( -m_surfaceNormal, right, up );

	m_ladderNormal = m_surfaceNormal;

	Vector startPos = m_editCursorPos;

	Vector leftEdge = startPos;
	Vector rightEdge = startPos;

	// trace to the sides to find the width
	Vector probe = m_surfaceNormal * -HalfHumanWidth;
	const float StepSize = 1.0f;
	StepAlongClimbableSurface( leftEdge, right * -StepSize, probe );
	StepAlongClimbableSurface( rightEdge, right * StepSize, probe );

	Vector topEdge = (leftEdge + rightEdge) * 0.5f;
	Vector bottomEdge = topEdge;
	StepAlongClimbableSurface( topEdge, up * StepSize, probe );
	StepAlongClimbableSurface( bottomEdge, up * -StepSize, probe );

	Vector top = (leftEdge + rightEdge) * 0.5f;
	top.z = topEdge.z;

	Vector bottom = top;
	bottom.z = bottomEdge.z;

	CreateLadder( topEdge, bottomEdge, leftEdge.DistTo( rightEdge ), m_ladderNormal.AsVector2D(), 0.0f );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Flood fills all areas with current place
 */
class PlaceFloodFillFunctor
{
public:
	PlaceFloodFillFunctor( CNavArea *area )
	{
		m_initialPlace = area->GetPlace();
	}

	bool operator() ( CNavArea *area )
	{
		if (area->GetPlace() != m_initialPlace)
			return false;

		area->SetPlace( TheNavMesh->GetNavPlace() );

		return true;
	}

private:
	unsigned int m_initialPlace;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when edit mode has just been enabled
 */
void CNavMesh::OnEditModeStart( void )
{
	ClearSelectedSet();
	m_isContinuouslySelecting = false;
	m_isContinuouslyDeselecting = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when edit mode has just been disabled
 */
void CNavMesh::OnEditModeEnd( void )
{
}

//--------------------------------------------------------------------------------------------------------------
class DrawSelectedSet
{
public:
	DrawSelectedSet( const Vector &shift )
	{
		m_count = 0;
		m_shift = shift;
	}
	
	bool operator() ( CNavArea *area )
	{
		if (TheNavMesh->IsInSelectedSet( area ))
		{
			area->DrawSelectedSet( m_shift );
			++m_count;
		}
		
		return (m_count < nav_draw_limit.GetInt());
	}
	
	int m_count;
	Vector m_shift;
};

//--------------------------------------------------------------------------------------------------------
class AddToDragSet
{
public:
	AddToDragSet( Extent &area, int zMin, int zMax, bool bDragDeselecting )
	{
		m_nTolerance = 1;
		m_dragArea = area;
		m_zMin = zMin - m_nTolerance;
		m_zMax = zMax + m_nTolerance;
		m_bDragDeselecting = bDragDeselecting;
	}

	bool operator() ( CNavArea *area )
	{
		bool bShouldBeInSelectedSet = m_bDragDeselecting;
		if ( ( TheNavMesh->IsInSelectedSet( area ) == bShouldBeInSelectedSet ) && area->IsOverlapping( m_dragArea ) && area->GetCenter().z >= m_zMin && area->GetCenter().z <= m_zMax )
		{
			TheNavMesh->AddToDragSelectionSet( area );
		}
		return true;
	}

	Extent m_dragArea;
	int m_zMin;
	int m_zMax;
	int m_nTolerance;
	bool m_bDragDeselecting;
};

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::UpdateDragSelectionSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	Extent dragArea;
	int xmin = std::min( m_anchor.x, m_editCursorPos.x );
	int xmax = std::max( m_anchor.x, m_editCursorPos.x );
	int ymin = std::min( m_anchor.y, m_editCursorPos.y );
	int ymax = std::max( m_anchor.y, m_editCursorPos.y );

	dragArea.lo = Vector( xmin, ymin, m_anchor.z );
	dragArea.hi = Vector( xmax, ymax, m_anchor.z );

	ClearDragSelectionSet();

	AddToDragSet add( dragArea, m_anchor.z - m_nDragSelectionVolumeZMin, m_anchor.z + m_nDragSelectionVolumeZMax, m_bIsDragDeselecting );
	ForAllAreas( add );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw navigation areas and edit them
 * @todo Clean the whole edit system up - its structure is legacy from peculiarities in GoldSrc.
 */
ConVar nav_show_compass( side_registry(), "nav_show_compass", "0", FCVAR_CHEAT );
void CNavMesh::DrawEditMode( void )
{
	CVProfScope _( "CNavMesh::DrawEditMode", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( IsGenerating() )
		return;

	// TODO: remove this when host_thread_mode 1 stops breaking NDEBUG_PERSIST_TILL_NEXT_SERVER overlays
	static ConVarRef host_thread_mode( "host_thread_mode" );
	host_thread_mode.SetValue( 0 );

	static ConVarRef sb_perf_collect( "sb_perf_collect" );
	sb_perf_collect.SetValue( 0 );

	const float maxRange = 1000.0f;		// 500

#if DEBUG_NAV_NODES
	if ( nav_show_nodes.GetBool() )
	{
		for ( CNavNode *node = CNavNode::GetFirst(); node != nullptr; node = node->GetNext() )
		{
			if ( m_editCursorPos.DistToSqr( *node->GetPosition() ) < 150*150 )
			{
				node->Draw();
			}
		}
	}
#endif // DEBUG_NAV_NODES

	Vector from, dir;
	GetEditVectors( &from, &dir );

	Vector to = from + maxRange * dir;

	/* IN_PROGRESS:
	if ( !IsEditMode( PLACE_PAINTING ) && nav_snap_to_grid.GetBool() )
	{
		Vector center = SnapToGrid( m_editCursorPos );

		const int GridCount = 3;
		const int GridArraySize = GridCount * 2 + 1;
		const int GridSize = GetGridSize();

		// fill in an array of heights for the grid
		Vector pos[GridArraySize][GridArraySize];
		int x, y;
		for ( x=0; x<GridArraySize; ++x )
		{
			for ( y=0; y<GridArraySize; ++y )
			{
				pos[x][y] = center;
				pos[x][y].x += (x-GridCount) * GridSize;
				pos[x][y].y += (y-GridCount) * GridSize;
				pos[x][y].z += 36.0f;

				GetGroundHeight( pos[x][y], &pos[x][y].z );
			}
		}

		for ( x=1; x<GridArraySize; ++x )
		{
			for ( y=1; y<GridArraySize; ++y )
			{
				NavDrawLine( pos[x-1][y-1], pos[x-1][y], NavGridColor );
				NavDrawLine( pos[x-1][y-1], pos[x][y-1], NavGridColor );

				if ( x == GridArraySize-1 )
				{
					NavDrawLine( pos[x][y-1], pos[x][y], NavGridColor );
				}

				if ( y == GridArraySize-1 )
				{
					NavDrawLine( pos[x-1][y], pos[x][y], NavGridColor );
				}
			}
		}
	}
	*/

	if ( FindActiveNavArea() || m_markedArea || m_markedLadder || !IsSelectedSetEmpty() || IsEditMode( CREATING_AREA ) || IsEditMode( CREATING_LADDER ) )
	{
		// draw cursor
		float cursorSize = 10.0f;

		if ( m_climbableSurface )
		{
			NDebugOverlay::Cross3D( m_editCursorPos, cursorSize, 0, 255, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
		else
		{
			NavDrawLine( m_editCursorPos + Vector( 0, 0, cursorSize ), m_editCursorPos,								NavCursorColor );
			NavDrawLine( m_editCursorPos + Vector( cursorSize, 0, 0 ), m_editCursorPos + Vector( -cursorSize, 0, 0 ),	NavCursorColor );
			NavDrawLine( m_editCursorPos + Vector( 0, cursorSize, 0 ), m_editCursorPos + Vector( 0, -cursorSize, 0 ),	NavCursorColor );

			if ( nav_show_compass.GetBool() )
			{
				const float offset = cursorSize * 1.5f;
				Vector pos;

				pos = m_editCursorPos;
				AddDirectionVector( &pos, NORTH, offset );
				NDebugOverlay::Text( pos, "N", false, NDEBUG_PERSIST_TILL_NEXT_SERVER );

				pos = m_editCursorPos;
				AddDirectionVector( &pos, SOUTH, offset );
				NDebugOverlay::Text( pos, "S", false, NDEBUG_PERSIST_TILL_NEXT_SERVER );

				pos = m_editCursorPos;
				AddDirectionVector( &pos, EAST, offset );
				NDebugOverlay::Text( pos, "E", false, NDEBUG_PERSIST_TILL_NEXT_SERVER );

				pos = m_editCursorPos;
				AddDirectionVector( &pos, WEST, offset );
				NDebugOverlay::Text( pos, "W", false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
			}
		}

		// show drag rectangle when creating areas and ladders
		if ( IsEditMode( CREATING_AREA ) )
		{
			float z = m_anchor.z + 2.0f;
			NavDrawLine( Vector( m_editCursorPos.x, m_editCursorPos.y, z ), Vector( m_anchor.x, m_editCursorPos.y, z ),	NavCreationColor );
			NavDrawLine( Vector( m_anchor.x, m_anchor.y, z ), Vector( m_anchor.x, m_editCursorPos.y, z ),					NavCreationColor );
			NavDrawLine( Vector( m_anchor.x, m_anchor.y, z ), Vector( m_editCursorPos.x, m_anchor.y, z ),					NavCreationColor );
			NavDrawLine( Vector( m_editCursorPos.x, m_editCursorPos.y, z ), Vector( m_editCursorPos.x, m_anchor.y, z ),	NavCreationColor );
		}
		else if ( IsEditMode( DRAG_SELECTING ) )
		{
			float z1 = m_anchor.z + m_nDragSelectionVolumeZMax;
			float z2 = m_anchor.z - m_nDragSelectionVolumeZMin;

			// Draw the drag select volume
			Vector vMin( m_anchor.x, m_anchor.y, z1 );
			Vector vMax( m_editCursorPos.x, m_editCursorPos.y, z2 );
			NavDrawVolume( vMin, vMax, m_anchor.z, NavDragSelectionColor );

			UpdateDragSelectionSet();

			Color dragSelectionColor = m_bIsDragDeselecting ? s_dragSelectionSetDeleteColor : s_dragSelectionSetAddColor;

			// Draw the drag selection set
			for ( CNavArea *area : m_dragSelectionSet )
			{
				area->DrawDragSelectionSet( dragSelectionColor );
			}
		}
		else if ( IsEditMode( CREATING_LADDER ) )
		{
			Vector corner1, corner2, corner3;
			if ( FindLadderCorners( &corner1, &corner2, &corner3 ) )
			{
				NavEditColor color = NavCreationColor;
				if ( !m_climbableSurface )
				{
					color = NavInvalidCreationColor;
				}

				NavDrawLine( m_ladderAnchor, corner1, color );
				NavDrawLine( corner1, corner3, color );
				NavDrawLine( corner3, corner2, color );
				NavDrawLine( corner2, m_ladderAnchor, color );
			}
		}

		if ( m_selectedLadder )
		{
			m_lastSelectedArea = nullptr;

			// if ladder changed, print its ID
			if (m_selectedLadder != m_lastSelectedLadder || nav_show_area_info.GetBool())
			{
				m_lastSelectedLadder = m_selectedLadder;

				// print ladder info
				char buffer[80];

				CBaseEntity *ladderEntity = m_selectedLadder->GetLadderEntity();
				if ( ladderEntity )
				{
					V_snprintf( buffer, sizeof( buffer ), "Ladder #%d (Team %s)\n", m_selectedLadder->GetID(), GetGlobalTeam( ladderEntity->GetTeamNumber() )->GetName() );
				}
				else
				{
					V_snprintf( buffer, sizeof( buffer ), "Ladder #%d\n", m_selectedLadder->GetID() );
				}
				NDebugOverlay::ScreenText( 0.5, 0.53, buffer, 255, 255, 0, 128, nav_show_area_info.GetBool() ? 0.1 : 0.5 );
			}

			// draw the ladder we are pointing at and all connected areas
			m_selectedLadder->DrawLadder();
			m_selectedLadder->DrawConnectedAreas();
		}

		if ( m_markedLadder && !IsEditMode( PLACE_PAINTING ) )
		{
			// draw the "marked" ladder
			m_markedLadder->DrawLadder();
		}

		if ( m_markedArea && !IsEditMode( PLACE_PAINTING ) )
		{
			// draw the "marked" area
			m_markedArea->Draw();
		}

		// find the area the player is pointing at
		if (m_selectedArea)
		{
			m_lastSelectedLadder = nullptr;
	
			// if area changed, print its ID
			if ( m_selectedArea != m_lastSelectedArea )
			{
				m_showAreaInfoTimer.Start( nav_show_area_info.GetFloat() );
				m_lastSelectedArea = m_selectedArea;
			}

			if (m_showAreaInfoTimer.HasStarted() && !m_showAreaInfoTimer.IsElapsed() )
			{
				char buffer[80];
				char attrib[80];
				char locName[80];

				if (m_selectedArea->GetPlace())
				{
					const char *name = TheNavMesh->PlaceToName( m_selectedArea->GetPlace() );
					if (name)
						std::strcpy( locName, name );
					else
						std::strcpy( locName, "ERROR" );
				}
				else
				{
					locName[0] = '\000';
				}

				if (IsEditMode( PLACE_PAINTING ))
				{
					attrib[0] = '\000';
				}
				else
				{
					attrib[0] = 0;
					int attributes = m_selectedArea->GetAttributes();
					if ( attributes & NAV_MESH_CROUCH )		V_strncat( attrib, "CROUCH ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_JUMP )		V_strncat( attrib, "JUMP ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_PRECISE )	V_strncat( attrib, "PRECISE ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_NO_JUMP )	V_strncat( attrib, "NO_JUMP ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_STOP )		V_strncat( attrib, "STOP ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_RUN )		V_strncat( attrib, "RUN ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_WALK )		V_strncat( attrib, "WALK ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_AVOID )		V_strncat( attrib, "AVOID ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_TRANSIENT )	V_strncat( attrib, "TRANSIENT ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_DONT_HIDE )	V_strncat( attrib, "DONT_HIDE ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_STAND )		V_strncat( attrib, "STAND ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_NO_HOSTAGES )V_strncat( attrib, "NO HOSTAGES ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_STAIRS )		V_strncat( attrib, "STAIRS ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_OBSTACLE_TOP ) V_strncat( attrib, "OBSTACLE ", sizeof( attrib ), -1 );
					if ( attributes & NAV_MESH_CLIFF )		V_strncat( attrib, "CLIFF ", sizeof( attrib ), -1 );
					if ( m_selectedArea->IsBlocked( TEAM_ANY ) ) V_strncat( attrib, "BLOCKED ", sizeof( attrib ), -1 );
					if ( m_selectedArea->HasAvoidanceObstacle() )	V_strncat( attrib, "OBSTRUCTED ", sizeof( attrib ), -1 );
					if ( m_selectedArea->IsDamaging() )		V_strncat( attrib, "DAMAGING ", sizeof( attrib ), -1 );
					if ( m_selectedArea->IsUnderwater() )	V_strncat( attrib, "UNDERWATER ", sizeof( attrib ), -1 );

					int connected = 0;
					connected += m_selectedArea->GetAdjacentCount( NORTH );
					connected += m_selectedArea->GetAdjacentCount( SOUTH );
					connected += m_selectedArea->GetAdjacentCount( EAST );
					connected += m_selectedArea->GetAdjacentCount( WEST );
					V_strncat( attrib, UTIL_VarArgs( "%d Connections ", connected ), sizeof( attrib ), -1 );
				}

				V_snprintf( buffer, sizeof( buffer ), "Area #%d %s %s\n", m_selectedArea->GetID(), locName, attrib );
				NDebugOverlay::ScreenText( 0.5, 0.53, buffer, 255, 255, 0, 128, NDEBUG_PERSIST_TILL_NEXT_SERVER );

				// do "place painting"
				if ( m_isPlacePainting )
				{
					if (m_selectedArea->GetPlace() != TheNavMesh->GetNavPlace())
					{
						m_selectedArea->SetPlace( TheNavMesh->GetNavPlace() );
						player->EmitSound( "Bot.EditSwitchOn" );
					}
				}
			}
			
			// do continuous selecting into selected set
			if ( m_isContinuouslySelecting )
			{
				AddToSelectedSet( m_selectedArea );
			}
			else if ( m_isContinuouslyDeselecting )
			{
				// do continuous de-selecting into selected set
				RemoveFromSelectedSet( m_selectedArea );
			}

			if (IsEditMode( PLACE_PAINTING ))
			{
				m_selectedArea->DrawConnectedAreas();
			}
			else	// normal editing mode
			{
				// draw split line
				Extent extent;
				m_selectedArea->GetExtent( &extent );

				float yaw = player->EyeAngles().y;
				while( yaw > 360.0f )
					yaw -= 360.0f;

				while( yaw < 0.0f )
					yaw += 360.0f;

				if (m_splitAlongX)
				{
					from.x = extent.lo.x;
					from.y = m_splitEdge;
					from.z = m_selectedArea->GetZ( from );

					to.x = extent.hi.x;
					to.y = m_splitEdge;
					to.z = m_selectedArea->GetZ( to );
				}
				else
				{
					from.x = m_splitEdge;
					from.y = extent.lo.y;
					from.z = m_selectedArea->GetZ( from );

					to.x = m_splitEdge;
					to.y = extent.hi.y;
					to.z = m_selectedArea->GetZ( to );
				}

				NavDrawLine( from, to, NavSplitLineColor );

				// draw the area we are pointing at and all connected areas
				m_selectedArea->DrawConnectedAreas();
			}
		}
		
		// render the selected set
		if (!IsSelectedSetEmpty())
		{
			Vector shift( 0, 0, 0 );
			
			if (IsEditMode( SHIFTING_XY ))
			{
				shift = m_editCursorPos - m_anchor;
				shift.z = 0.0f;
			}

			DrawSelectedSet draw( shift );

			// if the selected set is small, just blast it out
			if (std::ssize( m_selectedSet ) < nav_draw_limit.GetInt())
			{
				for ( CNavArea *area : m_selectedSet )
				{
					draw( area );
				}
			}
			else
			{
				// draw the part nearest the player
				CNavArea *nearest = nullptr;
				float nearRange = 9999999999.9f;

				for ( CNavArea *area : m_selectedSet )
				{
					float range = (player->GetAbsOrigin() - area->GetCenter()).LengthSqr();
					if (range < nearRange)
					{
						nearRange = range;
						nearest = area;
					}
				}

				SearchSurroundingAreas( nearest, nearest->GetCenter(), draw, -1, INCLUDE_INCOMING_CONNECTIONS | INCLUDE_BLOCKED_AREAS );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::SetMarkedLadder( CNavLadder *ladder )
{
	m_markedLadder = ladder;
	m_markedArea = nullptr;
	m_markedCorner = NUM_CORNERS;
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::SetMarkedArea( CNavArea *area )
{
	m_markedLadder = nullptr;
	m_markedArea = area;
	m_markedCorner = NUM_CORNERS;
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavDelete( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	if (IsSelectedSetEmpty())
	{
		// the old way
		CNavArea *markedArea = GetMarkedArea(); 
		CNavLadder *markedLadder = GetMarkedLadder(); 
		FindActiveNavArea();

		if( markedArea )
		{
			player->EmitSound( "EDIT_DELETE" );
			ks::erase_first( TheNavAreas, markedArea );
			TheNavMesh->OnEditDestroyNotify( markedArea );
			TheNavMesh->DestroyArea( markedArea );
		}
		else if( markedLadder )
		{
			player->EmitSound( "EDIT_DELETE" );
			ks::erase_first( m_ladders, markedLadder );
			OnEditDestroyNotify( markedLadder );
			delete markedLadder;
		} 
		else if ( m_selectedArea )
		{
			player->EmitSound( "EDIT_DELETE" );
			ks::erase_first( TheNavAreas, m_selectedArea );
			CNavArea *deadArea = m_selectedArea;
			OnEditDestroyNotify( deadArea );
			TheNavMesh->DestroyArea( deadArea );
		}
		else if ( m_selectedLadder )
		{
			player->EmitSound( "EDIT_DELETE" );
			ks::erase_first( m_ladders, m_selectedLadder );
			CNavLadder *deadLadder = m_selectedLadder;
			OnEditDestroyNotify( deadLadder );
			delete deadLadder;
		}
	}
	else
	{
		// delete all areas in the selected set
		player->EmitSound( "EDIT_DELETE" );
		
		for ( CNavArea *area : m_selectedSet )
		{
			ks::erase_first( TheNavAreas, area );
			
			OnEditDestroyNotify( area );
			
			TheNavMesh->DestroyArea( area );
		}
		
		Msg( "Deleted %d areas\n", (int)std::ssize( m_selectedSet ) );
		
		ClearSelectedSet();		
	}
	
	StripNavigationAreas();

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
class SelectCollector
{
public:
	SelectCollector( void )
	{
		m_count =  0;
	}
	
	bool operator() ( CNavArea *area )
	{
		// already selected areas terminate flood select
		if (TheNavMesh->IsInSelectedSet( area ))
			return false;		
		
		TheNavMesh->AddToSelectedSet( area );
		++m_count;
		
		return true;
	}

	int m_count;
};

//-------------------------------------------------------------------------------------------------------------- 
void CNavMesh::CommandNavDeleteMarked( void ) 
{ 
	CBasePlayer *player = UTIL_GetListenServerHost(); 
	if (player == nullptr) 
		return; 

	if ( !IsEditMode( NORMAL ) )
		return; 

	CNavArea *markedArea = GetMarkedArea(); 
	if( markedArea ) 
	{ 
		player->EmitSound( "EDIT_DELETE" ); 
		TheNavMesh->OnEditDestroyNotify( markedArea );
		ks::erase_first( TheNavAreas, markedArea ); 
		TheNavMesh->DestroyArea( markedArea ); 
	} 

	CNavLadder *markedLadder = GetMarkedLadder(); 
	if( markedLadder ) 
	{ 
		player->EmitSound( "EDIT_DELETE" );
		ks::erase_first( m_ladders, markedLadder );
		delete markedLadder; 
	} 

	StripNavigationAreas(); 

	ClearSelectedSet();		

	SetMarkedArea( nullptr );				// unmark the mark area 
	SetMarkedLadder( nullptr );			// unmark the mark ladder
	m_markedCorner = NUM_CORNERS;		// clear the corner selection 
} 

//--------------------------------------------------------------------------------------------------------------
/**
 * Select the current nav area and all recursively connected areas
 */
void CNavMesh::CommandNavFloodSelect( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	CNavArea *start = m_selectedArea;
	if ( !start )
	{
		start = m_markedArea;
	}

	if ( start )
	{
		player->EmitSound( "EDIT_DELETE" );

		int connections = INCLUDE_BLOCKED_AREAS | INCLUDE_INCOMING_CONNECTIONS;
		if ( args.ArgC() == 2 && FStrEq( "out", args[1] ) )
		{
			connections = INCLUDE_BLOCKED_AREAS;
		}
		if ( args.ArgC() == 2 && FStrEq( "in", args[1] ) )
		{
			connections = INCLUDE_BLOCKED_AREAS | INCLUDE_INCOMING_CONNECTIONS | EXCLUDE_OUTGOING_CONNECTIONS;
		}

		// collect all areas connected to this area
		SelectCollector collector;
		SearchSurroundingAreas( start, start->GetCenter(), collector, -1, connections );
		
		Msg( "Selected %d areas.\n", collector.m_count );
	}

	SetMarkedArea( nullptr );			// unmark the mark area
}

//--------------------------------------------------------------------------------------------------------------
/**
* Toggles all areas into/out of the selected set
*/
void CNavMesh::CommandNavToggleSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	player->EmitSound( "EDIT_DELETE" );

	NavAreaVector notInSelectedSet;

	// Build a list of all areas not in the selected set
	for ( CNavArea *area : TheNavAreas )
	{
		if ( !IsInSelectedSet( area ) )
		{
			notInSelectedSet.push_back( area );
		}
	}

	// Clear out the selected set
	ClearSelectedSet();

	// Add areas back into the selected set
	for ( CNavArea *area : notInSelectedSet )
	{
		AddToSelectedSet( area );
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( notInSelectedSet ) );

	SetMarkedArea( nullptr );			// unmark the mark area
}

//--------------------------------------------------------------------------------------------------------------
/**
* Saves the current selected set for later retrieval.
*/
void CNavMesh::CommandNavStoreSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	player->EmitSound( "EDIT_DELETE" );

	m_storedSelectedSet.clear();
	for ( CNavArea *area : m_selectedSet )
	{
		m_storedSelectedSet.push_back( area->GetID() );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
* Restores an older selected set.
*/
void CNavMesh::CommandNavRecallSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	player->EmitSound( "EDIT_DELETE" );

	ClearSelectedSet();

	for ( int i=0; i<std::ssize( m_storedSelectedSet ); ++i )
	{
		CNavArea *area = GetNavAreaByID( m_storedSelectedSet[i] );
		if ( area )
		{
			AddToSelectedSet( area );
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add current area to selected set
 */
void CNavMesh::CommandNavAddToSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		AddToSelectedSet( m_selectedArea );
		player->EmitSound( "EDIT_MARK.Enable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
* Add area ID to selected set
*/
void CNavMesh::CommandNavAddToSelectedSetByID( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) || args.ArgC() < 2 )
		return;

	int id = std::atoi( args[1] );
	CNavArea *area = GetNavAreaByID( id );
	if ( area )
	{
		AddToSelectedSet( area );
		player->EmitSound( "EDIT_MARK.Enable" );
		Msg( "Added area %d.  ( to go there: setpos %f %f %f )\n", id, area->GetCenter().x, area->GetCenter().y, area->GetCenter().z + 5 );
	}
	else
	{
		Msg( "No area with id %d\n", id );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove current area from selected set
 */
void CNavMesh::CommandNavRemoveFromSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		RemoveFromSelectedSet( m_selectedArea );
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
* Add/remove current area from selected set
*/
void CNavMesh::CommandNavToggleInSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (IsInSelectedSet( m_selectedArea ))
		{
			RemoveFromSelectedSet( m_selectedArea );
		}
		else
		{
			AddToSelectedSet( m_selectedArea );
		}
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clear the selected set to empty
 */
void CNavMesh::CommandNavClearSelectedSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	ClearSelectedSet();
	player->EmitSound( "EDIT_MARK.Disable" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Start continuously selecting areas into the selected set
 */
void CNavMesh::CommandNavBeginSelecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslySelecting = true;
	m_isContinuouslyDeselecting = false;
	
	player->EmitSound( "EDIT_BEGIN_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Stop continuously selecting areas into the selected set
 */
void CNavMesh::CommandNavEndSelecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslySelecting = false;
	m_isContinuouslyDeselecting = false;

	player->EmitSound( "EDIT_END_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavBeginDragSelecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) && !IsEditMode( DRAG_SELECTING ) )
		return;

	FindActiveNavArea();

	if ( IsEditMode( DRAG_SELECTING ) )
	{
		ClearDragSelectionSet();
		SetEditMode( NORMAL );
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
	}
	else
	{
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
		
		SetEditMode( DRAG_SELECTING );

		// m_anchor starting corner
		m_anchor = m_editCursorPos;
		m_nDragSelectionVolumeZMax = nav_drag_selection_volume_zmax_offset.GetInt();
		m_nDragSelectionVolumeZMin = nav_drag_selection_volume_zmin_offset.GetInt();
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavEndDragSelecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( IsEditMode( DRAG_SELECTING ) )
	{
		// Transfer drag selected areas to the selected set
		for ( CNavArea *area : m_dragSelectionSet )
		{
			AddToSelectedSet( area );
		}
		SetEditMode( NORMAL );
	}
	else
	{
		player->EmitSound( "EDIT_END_AREA.NotCreating" );
	}

	ClearDragSelectionSet();
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavBeginDragDeselecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) && !IsEditMode( DRAG_SELECTING ) )
		return;

	FindActiveNavArea();

	if ( IsEditMode( DRAG_SELECTING ) )
	{
		ClearDragSelectionSet();
		SetEditMode( NORMAL );
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
	}
	else
	{
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
		
		SetEditMode( DRAG_SELECTING );
		m_bIsDragDeselecting = true;

		// m_anchor starting corner
		m_anchor = m_editCursorPos;
		m_nDragSelectionVolumeZMax = nav_drag_selection_volume_zmax_offset.GetInt();
		m_nDragSelectionVolumeZMin = nav_drag_selection_volume_zmin_offset.GetInt();
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavEndDragDeselecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( IsEditMode( DRAG_SELECTING ) )
	{
		// Remove drag selected areas from the selected set
		for ( CNavArea *area : m_dragSelectionSet )
		{
			RemoveFromSelectedSet( area );
		}
		SetEditMode( NORMAL );
	}
	else
	{
		player->EmitSound( "EDIT_END_AREA.NotCreating" );
	}

	ClearDragSelectionSet();
	m_bIsDragDeselecting = false;
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavRaiseDragVolumeMax( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	m_nDragSelectionVolumeZMax += 32;
	nav_drag_selection_volume_zmax_offset.SetValue( m_nDragSelectionVolumeZMax );
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavLowerDragVolumeMax( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	m_nDragSelectionVolumeZMax = std::max( 0, m_nDragSelectionVolumeZMax - 32 );
	nav_drag_selection_volume_zmax_offset.SetValue( m_nDragSelectionVolumeZMax );
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavRaiseDragVolumeMin( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	m_nDragSelectionVolumeZMin = std::max( 0, m_nDragSelectionVolumeZMin - 32 );
	nav_drag_selection_volume_zmin_offset.SetValue( m_nDragSelectionVolumeZMin );
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavLowerDragVolumeMin( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	m_nDragSelectionVolumeZMin += 32;
	nav_drag_selection_volume_zmin_offset.SetValue( m_nDragSelectionVolumeZMin );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Start/stop continuously selecting areas into the selected set
 */
void CNavMesh::CommandNavToggleSelecting( bool playSound )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslySelecting = !m_isContinuouslySelecting;
	m_isContinuouslyDeselecting = false;

	if ( playSound )
	{
		player->EmitSound( "EDIT_END_AREA.Creating" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Start continuously de-selecting areas from the selected set
 */
void CNavMesh::CommandNavBeginDeselecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslyDeselecting = true;
	m_isContinuouslySelecting = false;
	
	player->EmitSound( "EDIT_BEGIN_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Stop continuously de-selecting areas from the selected set
 */
void CNavMesh::CommandNavEndDeselecting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslyDeselecting = false;
	m_isContinuouslySelecting = false;

	player->EmitSound( "EDIT_END_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Start/stop continuously de-selecting areas from the selected set
 */
void CNavMesh::CommandNavToggleDeselecting( bool playSound )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	m_isContinuouslyDeselecting = !m_isContinuouslyDeselecting;
	m_isContinuouslySelecting = false;

	if ( playSound )
	{
		player->EmitSound( "EDIT_END_AREA.Creating" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Selects all areas that intersect the half-space
 * Arguments: "+z 100", or "-x 30", etc.
 */
void CNavMesh::CommandNavSelectHalfSpace( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) && !IsEditMode( PLACE_PAINTING ) )
		return;

	if ( args.ArgC() != 3 )
	{
		Warning( "Error:  <+X|-X|+Y|-Y|+Z|-Z> <value>\n" );
		return;
	}

	enum HalfSpaceType
	{
		PLUS_X, MINUS_X,
		PLUS_Y, MINUS_Y,
		PLUS_Z, MINUS_Z,	
	}
	halfSpace = PLUS_X;
	
	if (FStrEq( "+x", args[1] ))
	{
		halfSpace = PLUS_X;
	}
	else if (FStrEq( "-x", args[1] ))
	{
		halfSpace = MINUS_X;
	}
	else if (FStrEq( "+y", args[1] ))
	{
		halfSpace = PLUS_Y;
	}
	else if (FStrEq( "-y", args[1] ))
	{
		halfSpace = MINUS_Y;
	}
	else if (FStrEq( "+z", args[1] ))
	{
		halfSpace = PLUS_Z;
	}
	else if (FStrEq( "-z", args[1] ))
	{
		halfSpace = MINUS_Z;
	}

	float value = std::atof( args[2] );
	
	Extent extent;
	for ( CNavArea *area : TheNavAreas )
	{
		area->GetExtent( &extent );
		
		switch( halfSpace )
		{
			case PLUS_X:
				if (extent.lo.x < value && extent.hi.x < value)
				{
					continue;
				}
				break;

			case PLUS_Y:
				if (extent.lo.y < value && extent.hi.y < value)
				{
					continue;
				}
				break;

			case PLUS_Z:
				if (extent.lo.z < value && extent.hi.z < value)
				{
					continue;
				}
				break;

			case MINUS_X:
				if (extent.lo.x > value && extent.hi.x > value)
				{
					continue;
				}
				break;

			case MINUS_Y:
				if (extent.lo.y > value && extent.hi.y > value)
				{
					continue;
				}
				break;

			case MINUS_Z:
				if (extent.lo.z > value && extent.hi.z > value)
				{
					continue;
				}
				break;
		}

		// toggle membership		
		if ( IsInSelectedSet( area ) )
		{
			RemoveFromSelectedSet( area );
		}
		else
		{
			AddToSelectedSet( area );	
		}
	}

	player->EmitSound( "EDIT_DELETE" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin shifting selected set in the XY plane
 */
void CNavMesh::CommandNavBeginShiftXY( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if (GetEditMode() == SHIFTING_XY)
	{
		SetEditMode( NORMAL );
		player->EmitSound( "EDIT_END_AREA.Creating" );
		return;
	}
	else
	{
		SetEditMode( SHIFTING_XY );
		player->EmitSound( "EDIT_BEGIN_AREA.Creating" );
	}
	
	// m_anchor starting corner
	m_anchor = m_editCursorPos;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Shift a set of areas, and all connected ladders
 */
class ShiftSet
{
public:
	ShiftSet( const Vector &shift )
	{
		m_shift = shift;
	}

	bool operator()( CNavArea *area )
	{
		area->Shift( m_shift );

		const NavLadderConnectVector *ladders = area->GetLadders( CNavLadder::LADDER_UP );
		int it;
		for( it = 0; it < std::ssize( *ladders ); ++it )
		{
			CNavLadder *ladder = (*ladders)[ it ].ladder;
			if ( !ks::contains( m_ladders, ladder ) )
			{
				ladder->Shift( m_shift );
				m_ladders.push_back( ladder );
			}
		}

		ladders = area->GetLadders( CNavLadder::LADDER_DOWN );
		for( it = 0; it < std::ssize( *ladders ); ++it )
		{
			CNavLadder *ladder = (*ladders)[ it ].ladder;
			if ( !ks::contains( m_ladders, ladder ) )
			{
				ladder->Shift( m_shift );
				m_ladders.push_back( ladder );
			}
		}

		return true;
	}

private:
	std::vector< CNavLadder * > m_ladders;
	Vector m_shift;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * End shifting selected set in the XY plane
 */
void CNavMesh::CommandNavEndShiftXY( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	SetEditMode( NORMAL );

	Vector shiftAmount = m_editCursorPos - m_anchor;
	shiftAmount.z = 0.0f;

	ShiftSet shift( shiftAmount );

	// update the position of all areas in the selected set
	TheNavMesh->ForAllSelectedAreas( shift );

	player->EmitSound( "EDIT_END_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------
static void nav_shift( const CCommand &args );
static ConCommand nav_shift_command( side_registry(), "nav_shift", nav_shift, "Shifts the selected areas by the specified amount", FCVAR_CHEAT );
static void nav_shift( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	TheNavMesh->SetEditMode( CNavMesh::NORMAL );

	Vector shiftAmount( vec3_origin );
	if ( args.ArgC() > 1 )
	{
		shiftAmount.x = std::atoi( args[1] );

		if ( args.ArgC() > 2 )
		{
			shiftAmount.y = atoi( args[2] );

			if ( args.ArgC() > 3 )
			{
				shiftAmount.z = atoi( args[3] );
			}
		}
	}

	ShiftSet shift( shiftAmount );

	// update the position of all areas in the selected set
	TheNavMesh->ForAllSelectedAreas( shift );

	player->EmitSound( "EDIT_END_AREA.Creating" );
}

//--------------------------------------------------------------------------------------------------------
void CommandNavCenterInWorld( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	TheNavMesh->SetEditMode( CNavMesh::NORMAL );

	// Build the nav mesh's extent
	Extent navExtent;
	bool first = true;
	for ( CNavArea *area : TheNavAreas )
	{
		if ( first )
		{
			area->GetExtent( &navExtent );
			first = false;
		}
		else
		{
			navExtent.Encompass( area->GetCorner( NORTH_WEST ) );
			navExtent.Encompass( area->GetCorner( NORTH_EAST ) );
			navExtent.Encompass( area->GetCorner( SOUTH_WEST ) );
			navExtent.Encompass( area->GetCorner( SOUTH_EAST ) );
		}
	}

	// Get the world's extent
	CWorld *world = static_cast< CWorld * >( CBaseEntity::Instance( INDEXENT( 0 ) ) );	// edict 0 is the world
	if ( !world )
		return;

	Extent worldExtent;
	world->GetWorldBounds( worldExtent.lo, worldExtent.hi );

	// Compute the difference, and shift in XY
	Vector navCenter = ( navExtent.lo + navExtent.hi ) * 0.5f;
	Vector worldCenter = ( worldExtent.lo + worldExtent.hi ) * 0.5f;
	Vector shift = worldCenter - navCenter;
	shift.z = 0.0f;

	// update the position of all areas
	for ( CNavArea *area : TheNavAreas )
	{
		area->Shift( shift );
	}

	player->EmitSound( "EDIT_END_AREA.Creating" );

	Msg( "Shifting mesh by %f,%f\n", shift.x, shift.y );
}
ConCommand nav_world_center( side_registry(), "nav_world_center", CommandNavCenterInWorld, "Centers the nav mesh in the world", FCVAR_CHEAT );

//--------------------------------------------------------------------------------------------------------------
/**
 * Add invalid areas to selected set
 */
void CNavMesh::CommandNavSelectInvalidAreas( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	ClearSelectedSet();

	Extent areaExtent;
	for ( CNavArea *area : TheNavAreas )
	{
		if ( area )
		{
			area->GetExtent( &areaExtent );
			for ( float x = areaExtent.lo.x; x + GenerationStepSize <= areaExtent.hi.x; x += GenerationStepSize )
			{
				for ( float y = areaExtent.lo.y; y + GenerationStepSize <= areaExtent.hi.y; y += GenerationStepSize )
				{
					float nw = area->GetZ( x, y );
					float ne = area->GetZ( x + GenerationStepSize, y );
					float sw = area->GetZ( x, y + GenerationStepSize );
					float se = area->GetZ( x + GenerationStepSize, y + GenerationStepSize );

					if ( !IsHeightDifferenceValid( nw, ne, sw, se ) ||
						!IsHeightDifferenceValid( ne, nw, sw, se ) ||
						!IsHeightDifferenceValid( sw, ne, nw, se ) ||
						!IsHeightDifferenceValid( se, ne, sw, nw ) )
					{
						AddToSelectedSet( area );
					}
				}
			}
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );

	if ( !m_selectedSet.empty() )
	{
		player->EmitSound( "EDIT_MARK.Enable" );
	}
	else
	{
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add blocked areas to selected set
 */
void CNavMesh::CommandNavSelectBlockedAreas( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	ClearSelectedSet();

	for ( CNavArea *area : TheNavAreas )
	{
		if ( area && area->IsBlocked( TEAM_ANY ) )
		{
			AddToSelectedSet( area );
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );

	if ( !m_selectedSet.empty() )
	{
		player->EmitSound( "EDIT_MARK.Enable" );
	}
	else
	{
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add obstructed areas to selected set
 */
void CNavMesh::CommandNavSelectObstructedAreas( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	ClearSelectedSet();

	for ( CNavArea *area : TheNavAreas )
	{
		if ( area && area->HasAvoidanceObstacle() )
		{
			AddToSelectedSet( area );
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );

	if ( !m_selectedSet.empty() )
	{
		player->EmitSound( "EDIT_MARK.Enable" );
	}
	else
	{
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add damaging areas to selected set
 */
void CNavMesh::CommandNavSelectDamagingAreas( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	ClearSelectedSet();

	for ( CNavArea *area : TheNavAreas )
	{
		if ( area && area->IsDamaging() )
		{
			AddToSelectedSet( area );
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );

	if ( !m_selectedSet.empty() )
	{
		player->EmitSound( "EDIT_MARK.Enable" );
	}
	else
	{
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Adds stairs areas to the selected set
 */
void CNavMesh::CommandNavSelectStairs( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( player == nullptr )
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	ClearSelectedSet();

	for ( CNavArea *area : TheNavAreas )
	{
		if ( area && area->HasAttributes( NAV_MESH_STAIRS ) )
		{
			AddToSelectedSet( area );
		}
	}

	Msg( "Selected %d areas.\n", (int)std::ssize( m_selectedSet ) );

	if ( !m_selectedSet.empty() )
	{
		player->EmitSound( "EDIT_MARK.Enable" );
	}
	else
	{
		player->EmitSound( "EDIT_MARK.Disable" );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavSplit( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (m_selectedArea->SplitEdit( m_splitAlongX, m_splitEdge ))
			player->EmitSound( "EDIT_SPLIT.MarkedArea" );
		else
			player->EmitSound( "EDIT_SPLIT.NoMarkedArea" );
	}

	StripNavigationAreas();

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
bool MakeSniperSpots( CNavArea *area )
{
	if ( !area )
		return false;

	bool splitAlongX;
	float splitEdge;

	const float minSplitSize = 2.0f; // ensure the first split is larger than this

	float sizeX = area->GetSizeX();
	float sizeY = area->GetSizeY();

	if ( sizeX > GenerationStepSize && sizeX > sizeY )
	{
		splitEdge = RoundToUnits( area->GetCorner( NORTH_WEST ).x, GenerationStepSize );
		if ( splitEdge < area->GetCorner( NORTH_WEST ).x + minSplitSize )
			splitEdge += GenerationStepSize;
		splitAlongX = false;
	}
	else if ( sizeY > GenerationStepSize && sizeY > sizeX )
	{
		splitEdge = RoundToUnits( area->GetCorner( NORTH_WEST ).y, GenerationStepSize );
		if ( splitEdge < area->GetCorner( NORTH_WEST ).y + minSplitSize )
			splitEdge += GenerationStepSize;
		splitAlongX = true;
	}
	else
	{
		return false;
	}

	CNavArea *first, *second;
	if ( !area->SplitEdit( splitAlongX, splitEdge, &first, &second ) )
	{
		return false;
	}

	first->Disconnect( second );
	second->Disconnect( first );

	MakeSniperSpots( first );
	MakeSniperSpots( second );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavMakeSniperSpots( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		// recursively split the area
		if ( MakeSniperSpots( m_selectedArea ) )
		{
			player->EmitSound( "EDIT_SPLIT.MarkedArea" );
		}
		else
		{
			player->EmitSound( "EDIT_SPLIT.NoMarkedArea" );
		}
	}
	else
	{
		player->EmitSound( "EDIT_SPLIT.NoMarkedArea" );
	}

	StripNavigationAreas();

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavMerge( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		CNavArea *other = m_markedArea;
		if ( !m_markedArea && std::ssize( m_selectedSet ) == 1 )
		{
			other = m_selectedSet[0];
		}

		if ( other && other != m_selectedArea )
		{
			if ( m_selectedArea->MergeEdit( other ) )
				player->EmitSound( "EDIT_MERGE.Enable" );
			else
				player->EmitSound( "EDIT_MERGE.Disable" );
		}
		else
		{
			Msg( "To merge, mark an area, highlight a second area, then invoke the merge command" );
			player->EmitSound( "EDIT_MERGE.Disable" );
		}
	}

	StripNavigationAreas();

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
	ClearSelectedSet();
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavMark( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;
		
	if (!IsSelectedSetEmpty())
	{
		// add or remove areas from the selected set
		if (IsInSelectedSet( m_selectedArea ))
		{
			// remove from set
			player->EmitSound( "EDIT_MARK.Disable" );
			RemoveFromSelectedSet( m_selectedArea );
		}
		else
		{
			// add to set
			player->EmitSound( "EDIT_MARK.Enable" );
			AddToSelectedSet( m_selectedArea );
		}
		return;
	}

	FindActiveNavArea();

	if ( m_markedArea || m_markedLadder )
	{
		// Unmark area or ladder
		player->EmitSound( "EDIT_MARK.Enable" );
		Msg("Area unmarked.\n");
		SetMarkedArea( nullptr );
	}
	else if ( args.ArgC() > 1 )
	{
		if ( FStrEq( args[1], "ladder" ) )
		{
			if ( args.ArgC() > 2 )
			{
				const char *ladderIDNameToMark = args[2];
				if ( ladderIDNameToMark )
				{
					unsigned int ladderIDToMark = std::atoi( ladderIDNameToMark );
					if ( ladderIDToMark != 0 )
					{
						CNavLadder *ladder = TheNavMesh->GetLadderByID( ladderIDToMark );
						if ( ladder )
						{
							player->EmitSound( "EDIT_MARK.Disable" );
							SetMarkedLadder( ladder );

							int connected = 0;
							connected += m_markedLadder->m_topForwardArea != nullptr;
							connected += m_markedLadder->m_topLeftArea != nullptr;
							connected += m_markedLadder->m_topRightArea != nullptr;
							connected += m_markedLadder->m_topBehindArea != nullptr;
							connected += m_markedLadder->m_bottomArea != nullptr;

							Msg( "Marked Ladder is connected to %d Areas\n", connected );
						}
					}
				}
			}
		}
		else
		{
			const char *areaIDNameToMark = args[1];
			if( areaIDNameToMark != nullptr )
			{
				unsigned int areaIDToMark = std::atoi(areaIDNameToMark);
				if( areaIDToMark != 0 )
				{
					CNavArea *areaToMark = nullptr;
					for ( CNavArea *area : TheNavAreas )
					{
						if( area->GetID() == areaIDToMark )
						{
							areaToMark = area;
							break;
						}
					}
					if( areaToMark )
					{
						player->EmitSound( "EDIT_MARK.Disable" );
						SetMarkedArea( areaToMark );

						int connected = 0;
						connected += GetMarkedArea()->GetAdjacentCount( NORTH );
						connected += GetMarkedArea()->GetAdjacentCount( SOUTH );
						connected += GetMarkedArea()->GetAdjacentCount( EAST );
						connected += GetMarkedArea()->GetAdjacentCount( WEST );

						Msg( "Marked Area is connected to %d other Areas\n", connected );
					}
				}
			}
		}
	}
	else if ( m_selectedArea )
	{
		// Mark an area
		player->EmitSound( "EDIT_MARK.Disable" );
		SetMarkedArea( m_selectedArea );

		int connected = 0;
		connected += GetMarkedArea()->GetAdjacentCount( NORTH );
		connected += GetMarkedArea()->GetAdjacentCount( SOUTH );
		connected += GetMarkedArea()->GetAdjacentCount( EAST );
		connected += GetMarkedArea()->GetAdjacentCount( WEST );

		Msg( "Marked Area is connected to %d other Areas\n", connected );
	}
	else if ( m_selectedLadder )
	{
		// Mark a ladder
		player->EmitSound( "EDIT_MARK.Disable" );
		SetMarkedLadder( m_selectedLadder );

		int connected = 0;
		connected += m_markedLadder->m_topForwardArea != nullptr;
		connected += m_markedLadder->m_topLeftArea != nullptr;
		connected += m_markedLadder->m_topRightArea != nullptr;
		connected += m_markedLadder->m_topBehindArea != nullptr;
		connected += m_markedLadder->m_bottomArea != nullptr;

		Msg( "Marked Ladder is connected to %d Areas\n", connected );
	}

	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavUnmark( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	player->EmitSound( "EDIT_MARK.Enable" );
	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavBeginArea( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !(IsEditMode( CREATING_AREA ) || IsEditMode( CREATING_LADDER ) || IsEditMode( NORMAL )) )
	{
		player->EmitSound( "EDIT_END_AREA.NotCreating" );
		return;
	}

	FindActiveNavArea();

	if ( IsEditMode( CREATING_AREA ) )
	{
		SetEditMode( NORMAL );
		player->EmitSound( "EDIT_BEGIN_AREA.Creating" );
	}
	else if ( IsEditMode( CREATING_LADDER ) )
	{
		SetEditMode( NORMAL );
		player->EmitSound( "EDIT_BEGIN_AREA.Creating" );
	}
	else if ( m_climbableSurface )
	{
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
		
		SetEditMode( CREATING_LADDER );

		// m_ladderAnchor starting corner
		m_ladderAnchor = m_editCursorPos;
		m_ladderNormal = m_surfaceNormal;
	}
	else
	{
		player->EmitSound( "EDIT_BEGIN_AREA.NotCreating" );
		
		SetEditMode( CREATING_AREA );

		// m_anchor starting corner
		m_anchor = m_editCursorPos;
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavEndArea( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !(IsEditMode( CREATING_AREA ) || IsEditMode( CREATING_LADDER ) || IsEditMode( NORMAL )) )
	{
		player->EmitSound( "EDIT_END_AREA.NotCreating" );
		return;
	}

	if ( IsEditMode( CREATING_AREA ) )
	{
		SetEditMode( NORMAL );
		
		// create the new nav area
		Vector endPos = m_editCursorPos;
		endPos.z = m_anchor.z;
		
		// We're a manually-created area, so let's look around to see what's nearby
		CNavArea *nearby = GetMarkedArea();
		if ( !nearby )
		{
			nearby = TheNavMesh->GetNearestNavArea( m_editCursorPos + Vector( 0, 0, HalfHumanHeight ), false, 10000.0f, true );
		}
		if ( !nearby )
		{
			nearby = TheNavMesh->GetNearestNavArea( endPos + Vector( 0, 0, HalfHumanHeight ), false, 10000.0f, true );
		}
		if ( !nearby )
		{
			nearby = TheNavMesh->GetNearestNavArea( m_editCursorPos );
		}
		if ( !nearby )
		{
			nearby = TheNavMesh->GetNearestNavArea( endPos );
		}

		CNavArea *newArea = CreateArea();
		if (newArea == nullptr)
		{
			Warning( "NavEndArea: Out of memory\n" );
			player->EmitSound( "EDIT_END_AREA.NotCreating" );
			return;
		}
		
		newArea->Build( m_anchor, endPos );

		if ( nearby )
		{
			newArea->InheritAttributes( nearby );	// inherit from the nearby area
		}

		TheNavAreas.push_back( newArea );
		TheNavMesh->AddNavArea( newArea );
		player->EmitSound( "EDIT_END_AREA.Creating" );

		if ( nav_create_place_on_ground.GetBool() )
		{
			newArea->PlaceOnGround( NUM_CORNERS );
		}

		// if we have a marked area, inter-connect the two
		if (GetMarkedArea())
		{
			Extent extent;
			GetMarkedArea()->GetExtent( &extent );

			if (m_anchor.x > extent.hi.x && m_editCursorPos.x > extent.hi.x)
			{
				GetMarkedArea()->ConnectTo( newArea, EAST );
				newArea->ConnectTo( GetMarkedArea(), WEST );
			}
			else if (m_anchor.x < extent.lo.x && m_editCursorPos.x < extent.lo.x)
			{
				GetMarkedArea()->ConnectTo( newArea, WEST );
				newArea->ConnectTo( GetMarkedArea(), EAST );
			}
			else if (m_anchor.y > extent.hi.y && m_editCursorPos.y > extent.hi.y)
			{
				GetMarkedArea()->ConnectTo( newArea, SOUTH );
				newArea->ConnectTo( GetMarkedArea(), NORTH );
			}
			else if (m_anchor.y < extent.lo.y && m_editCursorPos.y < extent.lo.y)
			{
				GetMarkedArea()->ConnectTo( newArea, NORTH );
				newArea->ConnectTo( GetMarkedArea(), SOUTH );
			}

			// propogate marked area to new area
			SetMarkedArea( newArea );
		}

		TheNavMesh->OnEditCreateNotify( newArea );
	}
	else if ( IsEditMode( CREATING_LADDER ) )
	{
		SetEditMode( NORMAL );
	
		player->EmitSound( "EDIT_END_AREA.Creating" );

		Vector corner1, corner2, corner3;
		if ( m_climbableSurface && FindLadderCorners( &corner1, &corner2, &corner3 ) )
		{
			// m_ladderAnchor and corner2 are at the same Z, and corner1 & corner3 share Z.
			Vector top = (m_ladderAnchor + corner2) * 0.5f;
			Vector bottom = (corner1 + corner3) * 0.5f;
			if ( top.z < bottom.z )
			{
				Vector tmp = top;
				top = bottom;
				bottom = tmp;
			}

			float width = m_ladderAnchor.DistTo( corner2 );
			Vector2D ladderDir = m_surfaceNormal.AsVector2D();

			CreateLadder( top, bottom, width, ladderDir, HumanHeight );
		}
		else
		{
			player->EmitSound( "EDIT_END_AREA.NotCreating" );
		}
	}
	else
	{
		player->EmitSound( "EDIT_END_AREA.NotCreating" );
	}

	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavConnect( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	Vector center;
	float halfWidth;
	if ( std::ssize( m_selectedSet ) > 1 )
	{
		bool bValid = true;
		for ( int i = 1; i < std::ssize( m_selectedSet ); ++i )
		{
			// Make sure all connections are valid
			CNavArea *first = m_selectedSet[0];
			CNavArea *second = m_selectedSet[i];

			NavDirType dir = second->ComputeLargestPortal( first, &center, &halfWidth );
			if (dir == NUM_DIRECTIONS)
			{
				player->EmitSound( "EDIT_CONNECT.AllDirections" );
				bValid = false;
				break;
			}

			dir = first->ComputeLargestPortal( second, &center, &halfWidth );
			if (dir == NUM_DIRECTIONS)
			{
				player->EmitSound( "EDIT_CONNECT.AllDirections" );
				bValid = false;
				break;
			}
		}

		if ( bValid )
		{
			for ( int i = 1; i < std::ssize( m_selectedSet ); ++i )
			{
				CNavArea *first = m_selectedSet[0];
				CNavArea *second = m_selectedSet[i];

				NavDirType dir = second->ComputeLargestPortal( first, &center, &halfWidth );
				second->ConnectTo( first, dir );

				dir = first->ComputeLargestPortal( second, &center, &halfWidth );
				first->ConnectTo( second, dir );
				player->EmitSound( "EDIT_CONNECT.Added" );
			}
		}
	}
	else if ( m_selectedArea )
	{
		if ( m_markedLadder )
		{
			m_markedLadder->ConnectTo( m_selectedArea );
			player->EmitSound( "EDIT_CONNECT.Added" );
		}
		else if ( m_markedArea )
		{
			NavDirType dir = GetMarkedArea()->ComputeLargestPortal( m_selectedArea, &center, &halfWidth );
			if (dir == NUM_DIRECTIONS)
			{
				player->EmitSound( "EDIT_CONNECT.AllDirections" );
			}
			else
			{
				m_markedArea->ConnectTo( m_selectedArea, dir );
				player->EmitSound( "EDIT_CONNECT.Added" );
			}
		}
		else
		{
			if ( std::ssize( m_selectedSet ) == 1 )
			{
				CNavArea *area = m_selectedSet[0];
				NavDirType dir = area->ComputeLargestPortal( m_selectedArea, &center, &halfWidth );
				if (dir == NUM_DIRECTIONS)
				{
					player->EmitSound( "EDIT_CONNECT.AllDirections" );
				}
				else
				{
					area->ConnectTo( m_selectedArea, dir );
					player->EmitSound( "EDIT_CONNECT.Added" );
				}
			}
			else
			{
				Msg( "To connect areas, mark an area, highlight a second area, then invoke the connect command. Make sure the cursor is directly north, south, east, or west of the marked area." );
				player->EmitSound( "EDIT_CONNECT.AllDirections" );
			}
		}
	}
	else if ( m_selectedLadder )
	{
		if ( m_markedArea )
		{
			m_markedArea->ConnectTo( m_selectedLadder );
			player->EmitSound( "EDIT_CONNECT.Added" );
		}
		else
		{
			Msg( "To connect areas, mark an area, highlight a second area, then invoke the connect command. Make sure the cursor is directly north, south, east, or west of the marked area." );
			player->EmitSound( "EDIT_CONNECT.AllDirections" );
		}
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
	ClearSelectedSet();
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavDisconnect( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( std::ssize( m_selectedSet ) > 1 )
	{
		bool bValid = true;
		for ( int i = 1; i < std::ssize( m_selectedSet ); ++i )
		{
			// 2 areas are selected, so connect them bi-directionally
			CNavArea *first = m_selectedSet[0];
			CNavArea *second = m_selectedSet[i];
			if ( !first->IsConnected( second, NUM_DIRECTIONS ) && !second->IsConnected( first, NUM_DIRECTIONS ) )
			{
				player->EmitSound( "EDIT_CONNECT.AllDirections" );
				bValid = false;
				break;
			}
		}

		if ( bValid )
		{
			for ( int i = 1; i < std::ssize( m_selectedSet ); ++i )
			{
				// 2 areas are selected, so connect them bi-directionally
				CNavArea *first = m_selectedSet[0];
				CNavArea *second = m_selectedSet[i];
				first->Disconnect( second );
				second->Disconnect( first );
			}
			player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
		}
	}
	else if ( m_selectedArea )
	{
		if ( m_markedArea )
		{
			m_markedArea->Disconnect( m_selectedArea );
			m_selectedArea->Disconnect( m_markedArea );
			player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
		}
		else if ( std::ssize( m_selectedSet ) == 1 )
		{
			m_selectedSet[0]->Disconnect( m_selectedArea );
			m_selectedArea->Disconnect( m_selectedSet[0] );
			player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
		}
		else
		{
			if ( m_markedLadder )
			{
				m_markedLadder->Disconnect( m_selectedArea );
				m_selectedArea->Disconnect( m_markedLadder );
				player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
			}
			else
			{
				Msg( "To disconnect areas, mark an area, highlight a second area, then invoke the disconnect command. This will remove all connections between the two areas." );
				player->EmitSound( "EDIT_DISCONNECT.NoMarkedArea" );
			}
		}
	}
	else if ( m_selectedLadder )
	{
		if ( m_markedArea )
		{
			m_markedArea->Disconnect( m_selectedLadder );
			m_selectedLadder->Disconnect( m_markedArea );
			player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
		}
		if ( std::ssize( m_selectedSet ) == 1 )
		{
			m_selectedSet[0]->Disconnect( m_selectedLadder );
			m_selectedLadder->Disconnect( m_selectedSet[0] );
			player->EmitSound( "EDIT_DISCONNECT.MarkedArea" );
		}
		else
		{
			Msg( "To disconnect areas, mark an area, highlight a second area, then invoke the disconnect command. This will remove all connections between the two areas." );
			player->EmitSound( "EDIT_DISCONNECT.NoMarkedArea" );
		}
	}

	ClearSelectedSet();
	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavSplice( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (GetMarkedArea())
		{
			if (m_selectedArea->SpliceEdit( GetMarkedArea() ))
				player->EmitSound( "EDIT_SPLICE.MarkedArea" );
			else
				player->EmitSound( "EDIT_SPLICE.NoMarkedArea" );
		}
		else
		{
			Msg( "To splice, mark an area, highlight a second area, then invoke the splice command to create an area between them" );
			player->EmitSound( "EDIT_SPLICE.NoMarkedArea" );
		}
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	ClearSelectedSet();
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Toggle an attribute on given area
 */
void CNavMesh::DoToggleAttribute( CNavArea *area, NavAttributeType attribute )
{
	area->SetAttributes( area->GetAttributes() ^ attribute );

	// keep a list of all "transient" nav areas
	if ( attribute == NAV_MESH_TRANSIENT )
	{
		if ( area->GetAttributes() & NAV_MESH_TRANSIENT )
		{
			m_transientAreas.push_back( area );
		}
		else
		{
			ks::erase_first( m_transientAreas, area );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavToggleAttribute( NavAttributeType attribute )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	if ( IsSelectedSetEmpty() )
	{
		// the old way
		FindActiveNavArea();

		if ( m_selectedArea )
		{
			player->EmitSound( "EDIT.ToggleAttribute" );
			DoToggleAttribute( m_selectedArea, attribute );			
		}
	}
	else
	{
		// toggle the attribute in all areas in the selected set
		player->EmitSound( "EDIT.ToggleAttribute" );

		for ( CNavArea *area : m_selectedSet )
		{
			DoToggleAttribute( area, attribute );			
		}

		Msg( "Changed attribute in %d areas\n", (int)std::ssize( m_selectedSet ) );

		ClearSelectedSet();		
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavTogglePlaceMode( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( IsEditMode( PLACE_PAINTING ) )
	{
		SetEditMode( NORMAL );
	}
	else
	{
		SetEditMode( PLACE_PAINTING );
	}

	player->EmitSound( "EDIT_TOGGLE_PLACE_MODE" );

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavPlaceFloodFill( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		PlaceFloodFillFunctor pff( m_selectedArea );
		SearchSurroundingAreas( m_selectedArea, m_selectedArea->GetCenter(), pff );
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavPlaceSet( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( PLACE_PAINTING ) )
		return;

	if ( !IsSelectedSetEmpty() )
	{
		for ( CNavArea *area : m_selectedSet )
		{
			area->SetPlace( TheNavMesh->GetNavPlace() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavPlacePick( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		player->EmitSound( "EDIT_PLACE_PICK" );
		TheNavMesh->SetNavPlace( m_selectedArea->GetPlace() );
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavTogglePlacePainting( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( PLACE_PAINTING ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (m_isPlacePainting)
		{
			m_isPlacePainting = false;
			player->EmitSound( "Bot.EditSwitchOff" );
		}
		else
		{
			m_isPlacePainting = true;

			player->EmitSound( "Bot.EditSwitchOn" );

			// paint the initial area
			m_selectedArea->SetPlace( TheNavMesh->GetNavPlace() );
		}
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavMarkUnnamed( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (GetMarkedArea())
		{
			player->EmitSound( "EDIT_MARK_UNNAMED.Enable" );
			SetMarkedArea( nullptr );
		}
		else
		{
			SetMarkedArea( nullptr );
			for ( CNavArea *area : TheNavAreas )
			{
				if ( area->GetPlace() == 0 )
				{
					SetMarkedArea( area );
					break;
				}
			}
			if ( !GetMarkedArea() )
			{
				player->EmitSound( "EDIT_MARK_UNNAMED.NoMarkedArea" );
			}
			else
			{
				player->EmitSound( "EDIT_MARK_UNNAMED.MarkedArea" );

				int connected = 0;
				connected += GetMarkedArea()->GetAdjacentCount( NORTH );
				connected += GetMarkedArea()->GetAdjacentCount( SOUTH );
				connected += GetMarkedArea()->GetAdjacentCount( EAST );
				connected += GetMarkedArea()->GetAdjacentCount( WEST );

				int totalUnnamedAreas = 0;
				for ( CNavArea *area : TheNavAreas )
				{
					if ( area->GetPlace() == 0 )
					{
						++totalUnnamedAreas;
					}
				}

				Msg( "Marked Area is connected to %d other Areas - there are %d total unnamed areas\n", connected, totalUnnamedAreas );
			}
		}
	}

	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavCornerSelect( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedArea )
	{
		if (GetMarkedArea())
		{
			int corner = (m_markedCorner + 1) % (NUM_CORNERS + 1);
			m_markedCorner = (NavCornerType)corner;
			player->EmitSound( "EDIT_SELECT_CORNER.MarkedArea" );
		}
		else
		{
			player->EmitSound( "EDIT_SELECT_CORNER.NoMarkedArea" );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavCornerRaise( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	int amount = 1;
	if ( args.ArgC() > 1 )
	{
		amount = std::atoi( args[1] );
	}

	if (IsSelectedSetEmpty())
	{
		// the old way
		FindActiveNavArea();

		if ( m_selectedArea )
		{
			if (GetMarkedArea())
			{
				GetMarkedArea()->RaiseCorner( m_markedCorner, amount );
				player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );
			}
			else
			{
				player->EmitSound( "EDIT_MOVE_CORNER.NoMarkedArea" );
			}
		}
	}
	else
	{
		// raise all areas in the selected set
		player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );

		for ( CNavArea *area : m_selectedSet )
		{
			area->RaiseCorner( NUM_CORNERS, amount, false );
		}

		Msg( "Raised %d areas\n", (int)std::ssize( m_selectedSet ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavCornerLower( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	int amount = -1;
	if ( args.ArgC() > 1 )
	{
		amount = -std::atoi( args[1] );
	}

	if (IsSelectedSetEmpty())
	{
		// the old way
		FindActiveNavArea();

		if ( m_selectedArea )
		{
			if (GetMarkedArea())
			{
				GetMarkedArea()->RaiseCorner( m_markedCorner, amount );
				player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );
			}
			else
			{
				player->EmitSound( "EDIT_MOVE_CORNER.NoMarkedArea" );
			}
		}
	}
	else
	{
		// raise all areas in the selected set
		player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );

		for ( CNavArea *area : m_selectedSet )
		{
			area->RaiseCorner( NUM_CORNERS, amount, false );
		}

		Msg( "Lowered %d areas\n", (int)std::ssize( m_selectedSet ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavCornerPlaceOnGround( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	float inset = 0.0f;
	if ( args.ArgC() == 2 )
	{
		inset = std::atof(args[1]);
	}

	if (IsSelectedSetEmpty())
	{
		// the old way
		FindActiveNavArea();

		if ( m_selectedArea )
		{
			if ( m_markedArea )
			{
				m_markedArea->PlaceOnGround( m_markedCorner, inset );
			}
			else
			{
				m_selectedArea->PlaceOnGround( NUM_CORNERS, inset );
			}
			player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );
		}
		else
		{
			player->EmitSound( "EDIT_MOVE_CORNER.NoMarkedArea" );
		}
	}
	else
	{
		// snap all areas in the selected set to the ground
		player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );

		for ( CNavArea *area : m_selectedSet )
		{
			area->PlaceOnGround( NUM_CORNERS, inset );
		}

		Msg( "Placed %d areas on the ground\n", (int)std::ssize( m_selectedSet ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavWarpToMark( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	CNavArea *targetArea = GetMarkedArea();
	if ( !targetArea && !IsSelectedSetEmpty() )
	{
		targetArea = m_selectedSet[0];
	}

	if ( targetArea )
	{
		Vector origin = targetArea->GetCenter() + Vector( 0, 0, 0.75f * HumanHeight );
		QAngle angles = player->GetAbsAngles();

		if ( ( player->IsDead() || player->IsObserver() ) && player->GetObserverMode() == OBS_MODE_ROAMING )
		{
			UTIL_SetOrigin( player, origin );
			player->EmitSound( "EDIT_WARP_TO_MARK" );
		}
		else
		{
			player->Teleport( &origin, &angles, &vec3_origin );
			player->EmitSound( "EDIT_WARP_TO_MARK" );
		}
	}
	else if ( GetMarkedLadder() )
	{
		CNavLadder *ladder = GetMarkedLadder();

		QAngle angles = player->GetAbsAngles();
		Vector origin = (ladder->m_top + ladder->m_bottom)/2;
		origin.x += ladder->GetNormal().x * GenerationStepSize;
		origin.y += ladder->GetNormal().y * GenerationStepSize;

		if ( ( player->IsDead() || player->IsObserver() ) && player->GetObserverMode() == OBS_MODE_ROAMING )
		{
			UTIL_SetOrigin( player, origin );
			player->EmitSound( "EDIT_WARP_TO_MARK" );
		}
		else
		{
			player->Teleport( &origin, &angles, &vec3_origin );
			player->EmitSound( "EDIT_WARP_TO_MARK" );
		}
	}
	else
	{
		player->EmitSound( "EDIT_WARP_TO_MARK" );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavLadderFlip( void )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	if ( !IsEditMode( NORMAL ) )
		return;

	FindActiveNavArea();

	if ( m_selectedLadder )
	{
		CNavArea *area;

		// flip direction
		player->EmitSound( "EDIT_MOVE_CORNER.MarkedArea" );
		m_selectedLadder->SetDir( OppositeDirection( m_selectedLadder->GetDir() ) );

		// and reverse ladder's area pointers
		area = m_selectedLadder->m_topBehindArea;
		m_selectedLadder->m_topBehindArea = m_selectedLadder->m_topForwardArea;
		m_selectedLadder->m_topForwardArea = area;

		area = m_selectedLadder->m_topRightArea;
		m_selectedLadder->m_topRightArea = m_selectedLadder->m_topLeftArea;
		m_selectedLadder->m_topLeftArea = area;
	}

	SetMarkedArea( nullptr );			// unmark the mark area
	m_markedCorner = NUM_CORNERS;	// clear the corner selection
}

//--------------------------------------------------------------------------------------------------------------
class RadiusSelect
{
	Vector m_origin;
	float m_radiusSquared;
	int m_selected;

public:
	RadiusSelect( const Vector &origin, float radius )
	{
		m_origin = origin;
		m_radiusSquared = radius * radius;
		m_selected = 0;
	}

	bool operator()( CNavArea *area )
	{
		if ( TheNavMesh->IsInSelectedSet( area ) )
			return true;

		Vector close;
		area->GetClosestPointOnArea( m_origin, &close );
		if ( close.DistToSqr( m_origin ) < m_radiusSquared )
		{
			TheNavMesh->AddToSelectedSet( area );
			++m_selected;
		}

		return true;
	}

	int GetNumSelected( void ) const
	{
		return m_selected;
	}
};

//--------------------------------------------------------------------------------------------------------------
static void nav_select_radius( const CCommand &args );
static ConCommand nav_select_radius_command( side_registry(), "nav_select_radius", nav_select_radius, "Adds all areas in a radius to the selection set", FCVAR_CHEAT );
static void nav_select_radius( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() || engine->IsDedicatedServer() )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg( "Needs a radius\n" );
		return;
	}

	float radius = std::atof( args[ 1 ] );
	CBasePlayer *host = UTIL_GetListenServerHost();
	if ( !host )
		return;

	RadiusSelect select( host->GetAbsOrigin(), radius );
	TheNavMesh->ForAllAreas( select );

	Msg( "%d areas added to selection\n", select.GetNumSelected() );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add area to the currently selected set
 */
void CNavMesh::AddToSelectedSet( CNavArea *area )
{
	if ( !area )
		return;

	// make sure area is not already in list
	if (ks::find_index( m_selectedSet, area ) != -1)
		return;
		
	m_selectedSet.push_back( area );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove area from the currently selected set
 */
void CNavMesh::RemoveFromSelectedSet( CNavArea *area )
{
	ks::erase_first( m_selectedSet, area );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add area to the drag selection set
 */
void CNavMesh::AddToDragSelectionSet( CNavArea *area )
{
	if ( !area )
		return;

	// make sure area is not already in list
	if (ks::find_index( m_dragSelectionSet, area ) != -1)
		return;
		
	m_dragSelectionSet.push_back( area );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove area from the drag selection set
 */
void CNavMesh::RemoveFromDragSelectionSet( CNavArea *area )
{
	ks::erase_first( m_dragSelectionSet, area );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clear the currently selected set to empty
 */
void CNavMesh::ClearDragSelectionSet( void )
{
	m_dragSelectionSet.clear();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clear the currently selected set to empty
 */
void CNavMesh::ClearSelectedSet( void )
{
	m_selectedSet.clear();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the selected set is empty
 */
bool CNavMesh::IsSelectedSetEmpty( void ) const
{
	return (m_selectedSet.empty());
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return size of the selected set
 */
int CNavMesh::GetSelecteSetSize( void ) const
{
	return std::ssize( m_selectedSet );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the selected set
 */
const NavAreaVector &CNavMesh::GetSelectedSet( void ) const
{
	return m_selectedSet;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given area is in the selected set
 */
bool CNavMesh::IsInSelectedSet( const CNavArea *area ) const
{
	for ( const CNavArea *setArea : m_selectedSet )
	{
		if (setArea == area)
			return true;
	}
	
	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when given area has just been added to the mesh in edit mode
 */
void CNavMesh::OnEditCreateNotify( CNavArea *newArea )
{
	for ( CNavArea *area : TheNavAreas )
	{
		area->OnEditCreateNotify( newArea );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when given area has just been deleted from the mesh in edit mode
 */
void CNavMesh::OnEditDestroyNotify( CNavArea *deadArea )
{
	// clean up any edit hooks
	m_markedArea = nullptr;
	m_selectedArea = nullptr;
	m_lastSelectedArea = nullptr;
	m_selectedLadder = nullptr;
	m_lastSelectedLadder = nullptr;
	m_markedLadder = nullptr;

	ks::erase_first( m_avoidanceObstacleAreas, deadArea );
	ks::erase_first( m_blockedAreas, deadArea );

	for ( CNavArea *area : TheNavAreas )
	{
		area->OnEditDestroyNotify( deadArea );
	}

	EditDestroyNotification notification( deadArea );
	ForEachActor( notification );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when given ladder has just been deleted from the mesh in edit mode
 * @TODO: Implement me
 */
void CNavMesh::OnEditDestroyNotify( CNavLadder *deadLadder )
{
}

class CBaseEntity;
class CNavArea;
class CTeam;
struct Extent;
struct inputdata_t;

using namespace ::ks::reflect;

// nav_entities.cpp
// AI Navigation entities
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncNavBlocker )

LINK_ENTITY_TO_CLASS( func_nav_blocker, CFuncNavBlocker );

std::vector<CFuncNavBlocker *> CFuncNavBlocker::gm_NavBlockers;

//-----------------------------------------------------------------------------------------------------
int CFuncNavBlocker::DrawDebugTextOverlays_Impl( void )
{
	int offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		CFmtStr str;

		// FIRST_GAME_TEAM skips TEAM_SPECTATOR and TEAM_UNASSIGNED, so we can print
		// useful team names in a non-game-specific fashion.
		for ( int i=FIRST_GAME_TEAM; i<FIRST_GAME_TEAM + MAX_NAV_TEAMS; ++i )
		{
			if ( IsBlockingNav( i ) )
			{
				CTeam *team = GetGlobalTeam( i );
				if ( team )
				{
					EntityText( offset++, str.sprintf( "blocking team %s", team->GetName() ), 0 );
				}
				else
				{
					EntityText( offset++, str.sprintf( "blocking team %d", i ), 0 );
				}
			}
		}

		NavAreaCollector collector( true );
		Extent extent;
		extent.Init( this );
		TheNavMesh->ForAllAreasOverlappingExtent( collector, extent );

		for ( int i=0; i<std::ssize( collector.m_area ); ++i )
		{
			CNavArea *area = collector.m_area[i];
			Extent areaExtent;
			area->GetExtent( &areaExtent );
			debugoverlay->AddBoxOverlay( vec3_origin, areaExtent.lo, areaExtent.hi, vec3_angle, 0, 255, 0, 10, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
	}

	return offset;
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::UpdateBlocked()
{
	NavAreaCollector collector( true );
	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( collector, extent );

	for ( int i=0; i<std::ssize( collector.m_area ); ++i )
	{
		CNavArea *area = collector.m_area[i];
		area->UpdateBlocked( true );
	}

}

//--------------------------------------------------------------------------------------------------------
// Forces nav areas to unblock when the nav blocker is deleted (round restart) so flow can compute properly
void CFuncNavBlocker::UpdateOnRemove_Impl( void )
{
	UnblockNav();

	ks::erase_first( gm_NavBlockers, this );

	BaseClass::UpdateOnRemove_Impl();
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::Spawn_Impl( void )
{
	gm_NavBlockers.push_back( this );

	if ( !m_blockedTeamNumber )
		m_blockedTeamNumber = TEAM_ANY;

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	CollisionProp()->WorldSpaceAABB( &m_CachedMins, &m_CachedMaxs );
	if ( m_bDisabled )
	{
		UnblockNav();
	}
	else
	{
		BlockNav();
	}
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::InputBlockNav( inputdata_t &inputdata )
{
	BlockNav();
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::InputUnblockNav( inputdata_t &inputdata )
{
	UnblockNav();
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::BlockNav( void )
{
	if ( m_blockedTeamNumber == TEAM_ANY )
	{
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			m_isBlockingNav[ i ] = true;
		}
	}
	else
	{
		int teamNumber = m_blockedTeamNumber % MAX_NAV_TEAMS;
		m_isBlockingNav[ teamNumber ] = true;
	}

	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( *this, extent );

	UpdateBlocked();
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::UnblockNav( void )
{
	if ( m_blockedTeamNumber == TEAM_ANY )
	{
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			m_isBlockingNav[ i ] = false;
		}
	}
	else
	{
		int teamNumber = m_blockedTeamNumber % MAX_NAV_TEAMS;
		m_isBlockingNav[ teamNumber ] = false;
	}

	UpdateBlocked();
}

//--------------------------------------------------------------------------------------------------------
// functor that blocks areas in our extent
bool CFuncNavBlocker::operator()( CNavArea *area )
{
	area->MarkAsBlocked( m_blockedTeamNumber, this );
	return true;
}

//--------------------------------------------------------------------------------------------------------
bool CFuncNavBlocker::CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs )
{
	int nTeamsBlocked = 0;
	int i;
	bool bBlocked = false;
	for ( i=0; i<MAX_NAV_TEAMS; ++i )
	{
		pResultByTeam[i] = false;
	}

	for ( CFuncNavBlocker *pBlocker : gm_NavBlockers )
	{
		bool bIsIntersecting = false;

		for ( i=0; i<MAX_NAV_TEAMS; ++i )
		{
			if ( pBlocker->m_isBlockingNav[i] )
			{
				if ( !pResultByTeam[i] )
				{
					if ( bIsIntersecting || ( bIsIntersecting = IsBoxIntersectingBox( pBlocker->m_CachedMins, pBlocker->m_CachedMaxs, vecMins, vecMaxs ) ) != false )
					{
						bBlocked = true;
						pResultByTeam[i] = true;
						nTeamsBlocked++;
					}
					else
					{
						continue;
					}
				}
			}
		}

		if ( nTeamsBlocked == MAX_NAV_TEAMS )
		{
			break;
		}
 	}
	return bBlocked;
}

//-----------------------------------------------------------------------------------------------------
/**
  * An entity that can obstruct nav areas.  This is meant for semi-transient areas that obstruct
  * pathfinding but can be ignored for longer-term queries like computing L4D flow distances and
  * escape routes.
  */
class CFuncNavObstruction : public CBaseEntity, public INavAvoidanceObstacle
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncNavObstruction, CBaseEntity );

public:
	CFuncNavObstruction( void ) : INavAvoidanceObstacle( INavAvoidanceObstacle::Kind::FuncNavObstruction ) { }

	void Spawn_Impl();
	void UpdateOnRemove_Impl( void );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	bool IsPotentiallyAbleToObstructNavAreas( void ) const { return true; }		// could we at some future time obstruct nav?
	float GetNavObstructionHeight( void ) const { return JumpCrouchHeight; }	// height at which to obstruct nav areas
	bool CanObstructNavAreas( void ) const { return !m_bDisabled; }				// can we obstruct nav right this instant?
	CBaseEntity *GetObstructingEntity( void ) { return this; }
	void OnNavMeshLoaded( void )
	{
		if ( !m_bDisabled )
		{
			ObstructNavAreas();
		}
	}

	int DrawDebugTextOverlays_Impl( void );

	bool operator()( CNavArea *area );	// functor that obstructs areas in our extent

private:

	void ObstructNavAreas( void );
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
};

//--------------------------------------------------------------------------------------------------------
// INavAvoidanceObstacle methods: forward to the concrete class (CPhysicsProp or CFuncNavObstruction) according to the Kind
//
bool INavAvoidanceObstacle::IsPotentiallyAbleToObstructNavAreas( void ) const
{
	switch ( m_obstacleKind )
	{
	case Kind::PhysicsProp:			return static_cast< const CPhysicsProp * >( this )->IsPotentiallyAbleToObstructNavAreas();
	case Kind::FuncNavObstruction:	return static_cast< const CFuncNavObstruction * >( this )->IsPotentiallyAbleToObstructNavAreas();
	}

	std::unreachable();
}

float INavAvoidanceObstacle::GetNavObstructionHeight( void ) const
{
	switch ( m_obstacleKind )
	{
	case Kind::PhysicsProp:			return static_cast< const CPhysicsProp * >( this )->GetNavObstructionHeight();
	case Kind::FuncNavObstruction:	return static_cast< const CFuncNavObstruction * >( this )->GetNavObstructionHeight();
	}

	std::unreachable();
}

bool INavAvoidanceObstacle::CanObstructNavAreas( void ) const
{
	switch ( m_obstacleKind )
	{
	case Kind::PhysicsProp:			return static_cast< const CPhysicsProp * >( this )->CanObstructNavAreas();
	case Kind::FuncNavObstruction:	return static_cast< const CFuncNavObstruction * >( this )->CanObstructNavAreas();
	}

	std::unreachable();
}

CBaseEntity *INavAvoidanceObstacle::GetObstructingEntity( void )
{
	switch ( m_obstacleKind )
	{
	case Kind::PhysicsProp:			return static_cast< CPhysicsProp * >( this )->GetObstructingEntity();
	case Kind::FuncNavObstruction:	return static_cast< CFuncNavObstruction * >( this )->GetObstructingEntity();
	}

	std::unreachable();
}

void INavAvoidanceObstacle::OnNavMeshLoaded( void )
{
	switch ( m_obstacleKind )
	{
	case Kind::PhysicsProp:			static_cast< CPhysicsProp * >( this )->OnNavMeshLoaded();			break;
	case Kind::FuncNavObstruction:	static_cast< CFuncNavObstruction * >( this )->OnNavMeshLoaded();	break;
	}
}

//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncNavObstruction )

LINK_ENTITY_TO_CLASS( func_nav_avoidance_obstacle, CFuncNavObstruction );

//-----------------------------------------------------------------------------------------------------
int CFuncNavObstruction::DrawDebugTextOverlays_Impl( void )
{
	int offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if ( CanObstructNavAreas() )
		{
			EntityText( offset++, "Obstructing nav", NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
		else
		{
			EntityText( offset++, "Not obstructing nav", NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
	}

	return offset;
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::UpdateOnRemove_Impl( void )
{
	TheNavMesh->UnregisterAvoidanceObstacle( this );

	BaseClass::UpdateOnRemove_Impl();
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::Spawn_Impl( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	if ( !m_bDisabled )
	{
		ObstructNavAreas();
		TheNavMesh->RegisterAvoidanceObstacle( this );
	}
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
	ObstructNavAreas();
	TheNavMesh->RegisterAvoidanceObstacle( this );
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	TheNavMesh->UnregisterAvoidanceObstacle( this );
}

//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::ObstructNavAreas( void )
{
	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( *this, extent );
}

//--------------------------------------------------------------------------------------------------------
// functor that blocks areas in our extent
bool CFuncNavObstruction::operator()( CNavArea *area )
{
	area->MarkObstacleToAvoid( GetNavObstructionHeight() );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
