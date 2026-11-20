// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "collisionutils.h"
#include "eiface.h"
#include "engine/ivdebugoverlay.h"
#include "igameevents.h"
#include "networkvar.h"
#include "shareddefs.h"
#include "declare_class.h"
#include "datacache/imdlcache.h"
#include "edict.h"
#include "tier2/tier2_logging.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.core.random;
import ks.core.tslist;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.memstack;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import ks.vstdlib.jobthread;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :nav_4;
import :entityutil;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.tier1.small_vector;
import ks.tier1.strtools;
import :cstrike15.cs_playeranimstate;
import :econ.econ_item_schema_minimal;

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CNavArea;
class CNavLadder;
class CNavMesh;
class CNavNode;
class CTeam;
struct Extent;
class HidingSpot;

using namespace ::ks::reflect;

// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

extern void HintMessageToAllPlayers( const char *message );

unsigned int CNavArea::m_nextID = 1;
NavAreaVector TheNavAreas;

uint32 CNavArea::m_masterMarker = 1;

CNavArea *CNavArea::m_openList = nullptr;
CNavArea *CNavArea::m_openListTail = nullptr;

bool CNavArea::m_isReset = false;
uint32 CNavArea::s_nCurrVisTestCounter = 0;

ConVar nav_coplanar_slope_limit( side_registry(), "nav_coplanar_slope_limit", "0.99", FCVAR_CHEAT );
ConVar nav_coplanar_slope_limit_displacement( side_registry(), "nav_coplanar_slope_limit_displacement", "0.7", FCVAR_CHEAT );
ConVar nav_split_place_on_ground( side_registry(), "nav_split_place_on_ground", "0", FCVAR_CHEAT, "If true, nav areas will be placed flush with the ground when split." );
ConVar nav_area_bgcolor( side_registry(), "nav_area_bgcolor", "0 0 0 30", FCVAR_CHEAT, "RGBA color to draw as the background color for nav areas while editing." );
ConVar nav_corner_adjust_adjacent( side_registry(), "nav_corner_adjust_adjacent", "18", FCVAR_CHEAT, "radius used to raise/lower corners in nearby areas when raising/lowering corners." );
ConVar nav_show_light_intensity( side_registry(), "nav_show_light_intensity", "0", FCVAR_CHEAT );
ConVar nav_debug_blocked( side_registry(), "nav_debug_blocked", "0", FCVAR_CHEAT );
ConVar nav_show_contiguous( side_registry(), "nav_show_continguous", "0", FCVAR_CHEAT, "Highlight non-contiguous connections" );

static const float DEF_NAV_VIEW_DISTANCE = 0.0;	// CS doesn't use nav vis

ConVar nav_max_view_distance( side_registry(), "nav_max_view_distance", "0", FCVAR_CHEAT, "Maximum range for precomputed nav mesh visibility (0 = default 1500 units)" );

ConVar nav_update_visibility_on_edit( side_registry(), "nav_update_visibility_on_edit", "0", FCVAR_CHEAT, "If nonzero editing the mesh will incrementally recompue visibility" );
ConVar nav_potentially_visible_dot_tolerance( side_registry(), "nav_potentially_visible_dot_tolerance", "0.98", FCVAR_CHEAT );
ConVar nav_show_potentially_visible( side_registry(), "nav_show_potentially_visible", "0", FCVAR_CHEAT, "Show areas that are potentially visible from the current nav area" );

Color s_selectedSetColor( 255, 255, 200, 96 );
Color s_selectedSetBorderColor( 100, 100, 0, 255 );
Color s_dragSelectionSetBorderColor( 50, 50, 50, 255 );
static void SelectedSetColorChaged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef colorVar( var->GetName() );

	Color *color = &s_selectedSetColor;
	if ( FStrEq( var->GetName(), "nav_selected_set_border_color" ) )
	{
		color = &s_selectedSetBorderColor;
	}

	// Xbox compiler needs these to be in this explicit form
	// likely due to sscanf expecting word aligned boundaries
	int r = color->r();
	int g = color->r();
	int b = color->b();
	int a = color->a();
	int numFound = std::sscanf( colorVar.GetString(), "%d %d %d %d", &r, &g, &b, &a );

	(*color)[0] = r;
	(*color)[1] = g;
	(*color)[2] = b;
	if ( numFound > 3 )
	{
		(*color)[3] = a;
	}
}
ConVar nav_selected_set_color( side_registry(), "nav_selected_set_color", "255 255 200 96", FCVAR_CHEAT, "Color used to draw the selected set background while editing.", false, 0.0f, false, 0.0f, SelectedSetColorChaged );
ConVar nav_selected_set_border_color( side_registry(), "nav_selected_set_border_color", "100 100 0 255", FCVAR_CHEAT, "Color used to draw the selected set borders while editing.", false, 0.0f, false, 0.0f, SelectedSetColorChaged );

//--------------------------------------------------------------------------------------------------------------

CMemoryStack CNavVectorNoEditAllocator::m_memory;
void *CNavVectorNoEditAllocator::m_pCurrent;
int CNavVectorNoEditAllocator::m_nBytesCurrent;

CNavVectorNoEditAllocator::CNavVectorNoEditAllocator()
{
	m_pCurrent = nullptr;
	m_nBytesCurrent = 0;
}

void CNavVectorNoEditAllocator::Reset()
{
	m_memory.FreeAll();
	m_pCurrent = nullptr;
	m_nBytesCurrent = 0;
}

void *CNavVectorNoEditAllocator::Alloc( std::size_t nSize )
{
	if ( !m_memory.GetBase() )
	{
		m_memory.Init( "CNavVectorNoEditAllocator::m_memory", 1024*1024, 0, 0, 4 );
	}
	m_pCurrent = (int *)m_memory.Alloc( nSize );
	m_nBytesCurrent = nSize;
	return m_pCurrent;
}

void *CNavVectorNoEditAllocator::Realloc( void *pMem, std::size_t nSize )
{
	if ( pMem != m_pCurrent )
	{
		contract_assert( 0 );
		Error( "Nav mesh cannot be mutated after load\n" );
	}
	if ( nSize > (std::size_t)m_nBytesCurrent )
	{
		m_memory.Alloc( nSize - m_nBytesCurrent );
		m_nBytesCurrent = nSize;
	}
	return m_pCurrent;
}

void CNavVectorNoEditAllocator::Free( void *pMem )
{
}

std::size_t CNavVectorNoEditAllocator::GetSize( void *pMem )
{
	if ( pMem != m_pCurrent )
	{
		contract_assert( 0 );
		Error( "Nav mesh cannot be mutated after load\n" );
	}
	return m_nBytesCurrent;
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::CompressIDs( void )
{
	m_nextID = 1;

	for ( int id = 0; id < std::ssize( TheNavAreas ); id++ )
	{
		CNavArea *area = TheNavAreas[id];
		area->m_id = m_nextID++;

		// remove and re-add the area from the nav mesh to update the hashed ID
		TheNavMesh->RemoveNavArea( area );
		TheNavMesh->AddNavArea( area );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during normal runtime.
 */
CNavArea::CNavArea( void )
{
	m_approachCount = 0;

	m_marker = 0;
	m_nearNavSearchMarker = 0;
	m_damagingTickCount = 0;
	m_openMarker = 0;

	m_parent = nullptr;
	m_parentHow = GO_NORTH;
	m_attributeFlags = 0;
	m_place = TheNavMesh->GetNavPlace();
	m_isUnderwater = false;
	m_avoidanceObstacleHeight = 0.0f;

	m_totalCost = 0.0f;

	ResetNodes();

	int i;
	for ( i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_isBlocked[i] = false;

		m_danger[i] = 0.0f;
		m_dangerTimestamp = 0.0f;

		m_clearedTimestamp[i] = 0.0f;

		m_earliestOccupyTime[i] = 0.0f;
	
		m_playerCount[i] = 0;
	}

	// set an ID for splitting and other interactive editing - loads will overwrite this
	m_id = m_nextID++;
	
	m_prevHash = nullptr;
	m_nextHash = nullptr;

	for( i = 0; i<NUM_DIRECTIONS; ++i )
	{
		m_connect[i].clear();
	}

	for( i=0; i<CNavLadder::NUM_LADDER_DIRECTIONS; ++i )
	{
		m_ladder[i].clear();
	}

	for ( i=0; i<NUM_CORNERS; ++i )
	{
		m_lightIntensity[i] = 1.0f;
	}

	m_invDxCorners = 0;
	m_invDyCorners = 0;

	m_inheritVisibilityFrom.area = nullptr;
	m_isInheritedFrom = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assumes Z is flat
 */
void CNavArea::Build( const Vector &corner, const Vector &otherCorner )
{
	if (corner.x < otherCorner.x)
	{
		m_nwCorner.x = corner.x;
		m_seCorner.x = otherCorner.x;
	}
	else
	{
		m_seCorner.x = corner.x;
		m_nwCorner.x = otherCorner.x;
	}

	if (corner.y < otherCorner.y)
	{
		m_nwCorner.y = corner.y;
		m_seCorner.y = otherCorner.y;
	}
	else
	{
		m_seCorner.y = corner.y;
		m_nwCorner.y = otherCorner.y;
	}

	m_nwCorner.z = corner.z;
	m_seCorner.z = corner.z;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	m_neZ = corner.z;
	m_swZ = otherCorner.z;

	CalcDebugID();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Build a nav area given the positions of its four corners.
 */
void CNavArea::Build( const Vector &nwCorner, const Vector &neCorner, const Vector &seCorner, const Vector &swCorner )
{
	m_nwCorner = nwCorner;
	m_seCorner = seCorner;

	m_neZ = neCorner.z;
	m_swZ = swCorner.z;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	CalcDebugID();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Used during generation phase to build nav areas from sampled nodes.
 */
void CNavArea::Build( CNavNode *nwNode, CNavNode *neNode, CNavNode *seNode, CNavNode *swNode )
{
	m_nwCorner = *nwNode->GetPosition();
	m_seCorner = *seNode->GetPosition();

	m_neZ = neNode->GetPosition()->z;
	m_swZ = swNode->GetPosition()->z;

	m_node[ NORTH_WEST ] = nwNode;
	m_node[ NORTH_EAST ] = neNode;
	m_node[ SOUTH_EAST ] = seNode;
	m_node[ SOUTH_WEST ] = swNode;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	// mark internal nodes as part of this area
	AssignNodes( this );

	CalcDebugID();
}

//--------------------------------------------------------------------------------------------------------------
// Return a computed extent (XY is in m_nwCorner and m_seCorner, Z is computed)
void CNavArea::GetExtent( Extent *extent ) const
{
	extent->lo = m_nwCorner;
	extent->hi = m_seCorner;

	extent->lo.z = std::min( extent->lo.z, m_nwCorner.z );
	extent->lo.z = std::min( extent->lo.z, m_seCorner.z );
	extent->lo.z = std::min( extent->lo.z, m_neZ );
	extent->lo.z = std::min( extent->lo.z, m_swZ );

	extent->hi.z = std::max( extent->hi.z, m_nwCorner.z );
	extent->hi.z = std::max( extent->hi.z, m_seCorner.z );
	extent->hi.z = std::max( extent->hi.z, m_neZ );
	extent->hi.z = std::max( extent->hi.z, m_swZ );
}

//--------------------------------------------------------------------------------------------------------------
// returns the closest node along the given edge to the given point
CNavNode *CNavArea::FindClosestNode( const Vector &pos, NavDirType dir ) const
{
	if ( !HasNodes() )
		return nullptr;

	std::vector< CNavNode * > nodes;
	GetNodes( dir, &nodes );

	CNavNode *bestNode = nullptr;
	float bestDistanceSq = std::numeric_limits<float>::max();

	for ( int i=0; i<std::ssize( nodes ); ++i )
	{
		float distSq = pos.DistToSqr( *nodes[i]->GetPosition() );
		if ( distSq < bestDistanceSq )
		{
			bestDistanceSq = distSq;
			bestNode = nodes[i];
		}
	}

	return bestNode;
}

//--------------------------------------------------------------------------------------------------------------
// build a vector of nodes along the given direction
void CNavArea::GetNodes( NavDirType dir, std::vector< CNavNode * > *nodes ) const
{
	if ( !nodes )
		return;

	nodes->clear();

	NavCornerType startCorner;
	NavCornerType endCorner;
	NavDirType traversalDirection;

	switch ( dir )
	{
	case NORTH:
		startCorner = NORTH_WEST;
		endCorner = NORTH_EAST;
		traversalDirection = EAST;
		break;

	case SOUTH:
		startCorner = SOUTH_WEST;
		endCorner = SOUTH_EAST;
		traversalDirection = EAST;
		break;

	case EAST:
		startCorner = NORTH_EAST;
		endCorner = SOUTH_EAST;
		traversalDirection = SOUTH;
		break;

	case WEST:
		startCorner = NORTH_WEST;
		endCorner = SOUTH_WEST;
		traversalDirection = SOUTH;
		break;

	default:
		return;
	}

	CNavNode *node;
	for ( node = m_node[ startCorner ]; node && node != m_node[ endCorner ]; node = node->GetConnectedNode( traversalDirection ) )
	{
		nodes->push_back( node );
	}
	if ( node && node == m_node[ endCorner ] )
	{
		nodes->push_back( node );
	}
}

//--------------------------------------------------------------------------------------------------------------
class ForgetArea
{
public:
	ForgetArea( CNavArea *area )
	{
		m_area = area;
	}
	
	bool operator() ( CBasePlayer *player )
	{
		player->OnNavAreaRemoved( m_area );
		
		return true;
	}

	bool operator() ( CBaseCombatCharacter *player )
	{
		player->OnNavAreaRemoved( m_area );

		return true;
	}

	CNavArea *m_area;
};

//--------------------------------------------------------------------------------------------------------------
class AreaDestroyNotification
{
	CNavArea *m_area;

public:
	AreaDestroyNotification( CNavArea *area )
	{
		m_area = area;
	}

	bool operator()( CNavLadder *ladder )
	{
		ladder->OnDestroyNotify( m_area );
		return true;
	}

	bool operator()( CNavArea *area )
	{
		if ( area != m_area )
		{
			area->OnDestroyNotify( m_area );
		}
		return true;
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CNavArea::~CNavArea()
{
	// if we are resetting the system, don't bother cleaning up - all areas are being destroyed
	if (m_isReset)
		return;

	// tell the other areas and ladders we are going away
	AreaDestroyNotification notification( this );
	TheNavMesh->ForAllAreas( notification );
	TheNavMesh->ForAllLadders( notification );

	// remove the area from the grid
	TheNavMesh->RemoveNavArea( this );
	
	// make sure no players keep a pointer to this area
	ForgetArea forget( this );
	ForEachActor( forget );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find elevator connections between areas
 */
void CNavArea::ConnectElevators( void )
{
	m_attributeFlags &= ~NAV_MESH_HAS_ELEVATOR;

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when map is initially loaded
 */
void CNavArea::OnServerActivate( void )
{
	ConnectElevators();
	m_damagingTickCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked for each area when the round restarts
 */
void CNavArea::OnRoundRestart( void )
{
	// need to redo this here since func_elevators are deleted and recreated at round restart
	ConnectElevators();
	m_damagingTickCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This is invoked at the start of an incremental nav generation on pre-existing areas.
 */
void CNavArea::ResetNodes( void )
{
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		m_node[i] = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::HasNodes( void ) const
{
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		if ( m_node[i] )
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This is invoked when an area is going away.
 * Remove any references we have to it.
 */
void CNavArea::OnDestroyNotify( CNavArea *dead )
{
	NavConnect con;
	con.area = dead;
	for( int d=0; d<NUM_DIRECTIONS; ++d )
	{
		ks::erase_first( m_connect[ d ], con );
		ks::erase_first( m_incomingConnect[ d ], con );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This is invoked when a ladder is going away.
 * Remove any references we have to it.
 */
void CNavArea::OnDestroyNotify( CNavLadder *dead )
{
	Disconnect( dead );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Connect this area to given area in given direction
 */
void CNavArea::ConnectTo( CNavArea *area, NavDirType dir )
{
	// don't allow self-referential connections
	if ( area == this )
		return;

	// check if already connected
	for ( const NavConnect &existing : m_connect[ dir ] )
	{
		if (existing.area == area)
			return;
	}

	NavConnect con;
	con.area = area;
	con.length = ( area->GetCenter() - GetCenter() ).Length();
	m_connect[ dir ].push_back( con );
	ks::erase_first( m_incomingConnect[ dir ], con );

	NavDirType dirOpposite = OppositeDirection( dir );
	con.area = this;
	if ( ks::find_index( area->m_connect[ dirOpposite ], con ) == -1 )
	{
		area->AddIncomingConnection( this, dirOpposite );
	}	
	
	//static char *dirName[] = { "NORTH", "EAST", "SOUTH", "WEST" };
	//Msg( "  Connected area #%d to #%d, %s\n", m_id, area->m_id, dirName[ dir ] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Connect this area to given ladder
 */
void CNavArea::ConnectTo( CNavLadder *ladder )
{
	float center = (ladder->m_top.z + ladder->m_bottom.z) * 0.5f;

	Disconnect( ladder ); // just in case

	if ( GetCenter().z > center )
	{
		AddLadderDown( ladder );
	}
	else
	{
		AddLadderUp( ladder );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Disconnect this area from given area
 */
void CNavArea::Disconnect( CNavArea *area )
{
	NavConnect connect;
	connect.area = area;

	for( int i = 0; i<NUM_DIRECTIONS; i++ )
	{
		NavDirType dir = (NavDirType) i;
		NavDirType dirOpposite = OppositeDirection( dir );
		int index = ks::find_index( m_connect[ dir ], connect );
		if ( index != -1 )
		{
			m_connect[ dir ].erase( m_connect[ dir ].begin() + index );
			if ( area->IsConnected( this, dirOpposite ) )
			{
				AddIncomingConnection( area, dir );
			}
			else
			{
				connect.area = this;
				ks::erase_first( area->m_incomingConnect[ dirOpposite ], connect );
			}
		}		
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Disconnect this area from given ladder
 */
void CNavArea::Disconnect( CNavLadder *ladder )
{
	NavLadderConnect con;
	con.ladder = ladder;

	for( int i=0; i<CNavLadder::NUM_LADDER_DIRECTIONS; ++i )
	{
		ks::erase_first( m_ladder[i], con );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::AddLadderUp( CNavLadder *ladder )
{
	Disconnect( ladder ); // just in case

	NavLadderConnect tmp;
	tmp.ladder = ladder;
	m_ladder[ CNavLadder::LADDER_UP ].push_back( tmp );
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::AddLadderDown( CNavLadder *ladder )
{
	Disconnect( ladder ); // just in case

	NavLadderConnect tmp;
	tmp.ladder = ladder;
	m_ladder[ CNavLadder::LADDER_DOWN ].push_back( tmp );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Recompute internal data once nodes have been adjusted during merge
 * Destroy adjArea.
 */
void CNavArea::FinishMerge( CNavArea *adjArea )
{
	// update extent
	m_nwCorner = *m_node[ NORTH_WEST ]->GetPosition();
	m_seCorner = *m_node[ SOUTH_EAST ]->GetPosition();

	m_neZ = m_node[ NORTH_EAST ]->GetPosition()->z;
	m_swZ = m_node[ SOUTH_WEST ]->GetPosition()->z;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	// reassign the adjacent area's internal nodes to the final area
	adjArea->AssignNodes( this );

	// merge adjacency links - we gain all the connections that adjArea had
	MergeAdjacentConnections( adjArea );

	// remove subsumed adjacent area
	ks::erase_first( TheNavAreas, adjArea );
	TheNavMesh->OnEditDestroyNotify( adjArea );
	TheNavMesh->DestroyArea( adjArea );
}

//--------------------------------------------------------------------------------------------------------------
class LadderConnectionReplacement
{
	CNavArea *m_originalArea;
	CNavArea *m_replacementArea;

public:
	LadderConnectionReplacement( CNavArea *originalArea, CNavArea *replacementArea )
	{
		m_originalArea = originalArea;
		m_replacementArea = replacementArea;
	}

	bool operator()( CNavLadder *ladder )
	{
		if ( ladder->m_topForwardArea == m_originalArea )
			ladder->m_topForwardArea = m_replacementArea;

		if ( ladder->m_topRightArea == m_originalArea )
			ladder->m_topRightArea = m_replacementArea;

		if ( ladder->m_topLeftArea == m_originalArea )
			ladder->m_topLeftArea = m_replacementArea;

		if ( ladder->m_topBehindArea == m_originalArea )
			ladder->m_topBehindArea = m_replacementArea;

		if ( ladder->m_bottomArea == m_originalArea )
			ladder->m_bottomArea = m_replacementArea;

		return true;
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * For merging with "adjArea" - pick up all of "adjArea"s connections
 */
void CNavArea::MergeAdjacentConnections( CNavArea *adjArea )
{
	// merge adjacency links - we gain all the connections that adjArea had
	int dir;
	for( dir = 0; dir<NUM_DIRECTIONS; dir++ )
	{
		for ( const NavConnect &adjConnect : adjArea->m_connect[ dir ] )
		{
			NavConnect connect = adjConnect;

			if (connect.area != adjArea && connect.area != this)
				ConnectTo( connect.area, (NavDirType)dir );
		}
	}

	// remove any references from this area to the adjacent area, since it is now part of us
	Disconnect( adjArea );
	
	// Change other references to adjArea to refer instead to us
	// We can't just replace existing connections, as several adjacent areas may have been merged into one,
	// resulting in a large area adjacent to all of them ending up with multiple redunandant connections
	// into the merged area, one for each of the adjacent subsumed smaller ones.
	// If an area has a connection to the merged area, we must remove all references to adjArea, and add
	// a single connection to us.
	for ( CNavArea *area : TheNavAreas )
	{
		if (area == this || area == adjArea)
			continue;

		for( dir = 0; dir<NUM_DIRECTIONS; dir++ )
		{
			// check if there are any references to adjArea in this direction
			bool connected = false;
			for ( const NavConnect &areaConnect : area->m_connect[ dir ] )
			{
				NavConnect connect = areaConnect;

				if (connect.area == adjArea)
				{
					connected = true;
					break;
				}
			}

			if (connected)
			{
				// remove all references to adjArea
				area->Disconnect( adjArea );

				// remove all references to the new area
				area->Disconnect( this );

				// add a single connection to the new area
				area->ConnectTo( this, (NavDirType) dir );
			}
		}
	}

	// We gain all ladder connections adjArea had
	for( dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		for ( const NavLadderConnect &adjLadder : adjArea->m_ladder[ dir ] )
		{
			ConnectTo( adjLadder.ladder );
		}
	}

	// All ladders that point to adjArea should point to us now
	LadderConnectionReplacement replacement( adjArea, this );
	TheNavMesh->ForAllLadders( replacement );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assign internal nodes to the given area
 * NOTE: "internal" nodes do not include the east or south border nodes
 */
void CNavArea::AssignNodes( CNavArea *area )
{
	CNavNode *horizLast = m_node[ NORTH_EAST ];

	for( CNavNode *vertNode = m_node[ NORTH_WEST ]; vertNode != m_node[ SOUTH_WEST ]; vertNode = vertNode->GetConnectedNode( SOUTH ) )
	{
		for( CNavNode *horizNode = vertNode; horizNode != horizLast; horizNode = horizNode->GetConnectedNode( EAST ) )
		{
			horizNode->AssignArea( area );
		}

		horizLast = horizLast->GetConnectedNode( SOUTH );
	}
}

//--------------------------------------------------------------------------------------------------------------
class SplitNotification
{
	CNavArea *m_originalArea;
	CNavArea *m_alphaArea;
	CNavArea *m_betaArea;

public:
	SplitNotification( CNavArea *originalArea, CNavArea *alphaArea, CNavArea *betaArea )
	{
		m_originalArea = originalArea;
		m_alphaArea = alphaArea;
		m_betaArea = betaArea;
	}

	bool operator()( CNavLadder *ladder )
	{
		ladder->OnSplit( m_originalArea, m_alphaArea, m_betaArea );
		return true;
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Split this area into two areas at the given edge.
 * Preserve all adjacency connections.
 * NOTE: This does not update node connections, only areas.
 */
bool CNavArea::SplitEdit( bool splitAlongX, float splitEdge, CNavArea **outAlpha, CNavArea **outBeta )
{
	CNavArea *alpha = nullptr;
	CNavArea *beta = nullptr;

	if (splitAlongX)
	{
		// +-----+->X
		// |  A  |
		// +-----+
		// |  B  |
		// +-----+
		// |
		// Y

		// don't do split if at edge of area
		if (splitEdge <= m_nwCorner.y + 1.0f)
			return false;

		if (splitEdge >= m_seCorner.y - 1.0f)
			return false;

		alpha = TheNavMesh->CreateArea();
		alpha->m_nwCorner = m_nwCorner;

		alpha->m_seCorner.x = m_seCorner.x;
		alpha->m_seCorner.y = splitEdge;
		alpha->m_seCorner.z = GetZ( alpha->m_seCorner );

		beta = TheNavMesh->CreateArea();
		beta->m_nwCorner.x = m_nwCorner.x;
		beta->m_nwCorner.y = splitEdge;
		beta->m_nwCorner.z = GetZ( beta->m_nwCorner );

		beta->m_seCorner = m_seCorner;

		alpha->ConnectTo( beta, SOUTH );
		beta->ConnectTo( alpha, NORTH );

		FinishSplitEdit( alpha, SOUTH );
		FinishSplitEdit( beta, NORTH );
	}
	else
	{
		// +--+--+->X
		// |  |  |
		// | A|B |
		// |  |  |
		// +--+--+
		// |
		// Y

		// don't do split if at edge of area
		if (splitEdge <= m_nwCorner.x + 1.0f)
			return false;

		if (splitEdge >= m_seCorner.x - 1.0f)
			return false;

		alpha = TheNavMesh->CreateArea();
		alpha->m_nwCorner = m_nwCorner;

		alpha->m_seCorner.x = splitEdge;
		alpha->m_seCorner.y = m_seCorner.y;
		alpha->m_seCorner.z = GetZ( alpha->m_seCorner );

		beta = TheNavMesh->CreateArea();
		beta->m_nwCorner.x = splitEdge;
		beta->m_nwCorner.y = m_nwCorner.y;
		beta->m_nwCorner.z = GetZ( beta->m_nwCorner );

		beta->m_seCorner = m_seCorner;

		alpha->ConnectTo( beta, EAST );
		beta->ConnectTo( alpha, WEST );

		FinishSplitEdit( alpha, EAST );
		FinishSplitEdit( beta, WEST );
	}

	if ( !TheNavMesh->IsGenerating() && nav_split_place_on_ground.GetBool() )
	{
		alpha->PlaceOnGround( NUM_CORNERS );
		beta->PlaceOnGround( NUM_CORNERS );
	}

	// For every ladder we pointed to, alpha or beta should point to it, based on
	// their distance to the ladder
	int dir;
	for( dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		for ( const NavLadderConnect &ladderConnect : m_ladder[ dir ] )
		{
			CNavLadder *ladder = ladderConnect.ladder;
			Vector ladderPos = ladder->m_top; // doesn't matter if we choose top or bottom

			float alphaDistance = alpha->GetDistanceSquaredToPoint( ladderPos );
			float betaDistance = beta->GetDistanceSquaredToPoint( ladderPos );

			if ( alphaDistance < betaDistance )
			{
				alpha->ConnectTo( ladder );
			}
			else
			{
				beta->ConnectTo( ladder );
			}
		}
	}

	// For every ladder that pointed to us, connect that ladder to the closer of alpha and beta
	SplitNotification notify( this, alpha, beta );
	TheNavMesh->ForAllLadders( notify );

	// return new areas
	if (outAlpha)
		*outAlpha = alpha;

	if (outBeta)
		*outBeta = beta;

	TheNavMesh->OnEditCreateNotify( alpha );
	TheNavMesh->OnEditCreateNotify( beta );
	if ( TheNavMesh->IsInSelectedSet( this ) )
	{
		TheNavMesh->AddToSelectedSet( alpha );
		TheNavMesh->AddToSelectedSet( beta );
	}

	// remove original area
	TheNavMesh->OnEditDestroyNotify( this );
	ks::erase_first( TheNavAreas, this );
	TheNavMesh->RemoveFromSelectedSet( this );
	TheNavMesh->DestroyArea( this );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given ladder is connected in given direction
 * @todo Formalize "asymmetric" flag on connections
 */
bool CNavArea::IsConnected( const CNavLadder *ladder, CNavLadder::LadderDirectionType dir ) const
{
	for ( const NavLadderConnect &ladderConnect : m_ladder[ dir ] )
	{
		if ( ladder == ladderConnect.ladder )
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given area is connected in given direction
 * if dir == NUM_DIRECTIONS, check all directions (direction is unknown)
 * @todo Formalize "asymmetric" flag on connections
 */
bool CNavArea::IsConnected( const CNavArea *area, NavDirType dir ) const
{
	// we are connected to ourself
	if (area == this)
		return true;

	if (dir == NUM_DIRECTIONS)
	{
		// search all directions
		for( int d=0; d<NUM_DIRECTIONS; ++d )
		{
			for ( const NavConnect &connect : m_connect[ d ] )
			{
				if (area == connect.area)
					return true;
			}
		}

		// check ladder connections
		for ( const NavLadderConnect &ladderConnect : m_ladder[ CNavLadder::LADDER_UP ] )
		{
			CNavLadder *ladder = ladderConnect.ladder;

			if (ladder->m_topBehindArea == area ||
				ladder->m_topForwardArea == area ||
				ladder->m_topLeftArea == area ||
				ladder->m_topRightArea == area)
				return true;
		}

		for ( const NavLadderConnect &ladderConnect : m_ladder[ CNavLadder::LADDER_DOWN ] )
		{
			CNavLadder *ladder = ladderConnect.ladder;

			if (ladder->m_bottomArea == area)
				return true;
		}
	}
	else
	{
		// check specific direction
		for ( const NavConnect &connect : m_connect[ dir ] )
		{
			if (area == connect.area)
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute change in actual ground height from this area to given area
 */
float CNavArea::ComputeGroundHeightChange( const CNavArea *area )
{
	CVProfScope _( "CNavArea::ComputeHeightChange", 0, "NextBot", false, BUDGETFLAG_OTHER );

	Vector closeFrom, closeTo;
	area->GetClosestPointOnArea( GetCenter(), &closeTo );
	GetClosestPointOnArea( area->GetCenter(), &closeFrom );

	// find actual ground height at each point in case 
	// areas are below/above actual terrain
	float toZ, fromZ;
	if ( TheNavMesh->GetSimpleGroundHeight( closeTo + Vector( 0, 0, StepHeight ), &toZ ) == false )
	{
		return 0.0f;
	}

	if ( TheNavMesh->GetSimpleGroundHeight( closeFrom + Vector( 0, 0, StepHeight ), &fromZ ) == false )
	{
		return 0.0f;
	}

	return toZ - fromZ;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The area 'source' is connected to us along our 'incomingEdgeDir' edge
 */
void CNavArea::AddIncomingConnection( CNavArea *source, NavDirType incomingEdgeDir )
{
	NavConnect con;
	con.area = source;
	if ( ks::find_index( m_incomingConnect[ incomingEdgeDir ], con ) == -1 )
	{
		con.length = ( source->GetCenter() - GetCenter() ).Length();
		m_incomingConnect[ incomingEdgeDir ].push_back( con );
	}	
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given the portion of the original area, update its internal data
 * The "ignoreEdge" direction defines the side of the original area that the new area does not include
 */
void CNavArea::FinishSplitEdit( CNavArea *newArea, NavDirType ignoreEdge )
{
	newArea->InheritAttributes( this );

	newArea->m_neZ = GetZ( newArea->m_seCorner.x, newArea->m_nwCorner.y );
	newArea->m_swZ = GetZ( newArea->m_nwCorner.x, newArea->m_seCorner.y );

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		newArea->m_invDxCorners = 1.0f / ( newArea->m_seCorner.x - newArea->m_nwCorner.x );
		newArea->m_invDyCorners = 1.0f / ( newArea->m_seCorner.y - newArea->m_nwCorner.y );
	}
	else
	{
		newArea->m_invDxCorners = newArea->m_invDyCorners = 0;
	}

	// connect to adjacent areas
	for( int d=0; d<NUM_DIRECTIONS; ++d )
	{
		if (d == ignoreEdge)
			continue;

		int count = GetAdjacentCount( (NavDirType)d );

		for( int a=0; a<count; ++a )
		{
			CNavArea *adj = GetAdjacentArea( (NavDirType)d, a );

			switch( d )
			{
				case NORTH:
				case SOUTH:
					if (newArea->IsOverlappingX( adj ))
					{
						newArea->ConnectTo( adj, (NavDirType)d );			

						// add reciprocal connection if needed
						if (adj->IsConnected( this, OppositeDirection( (NavDirType)d )))
							adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;

				case EAST:
				case WEST:
					if (newArea->IsOverlappingY( adj ))
					{
						newArea->ConnectTo( adj, (NavDirType)d );			

						// add reciprocal connection if needed
						if (adj->IsConnected( this, OppositeDirection( (NavDirType)d )))
							adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;
			}

			for ( int a = 0; a < std::ssize( m_incomingConnect[d] ); a++ )
			{			
				CNavArea *adj = m_incomingConnect[d][a].area;

				switch( d )
				{
				case NORTH:
				case SOUTH:
					if (newArea->IsOverlappingX( adj ))
					{
						adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;

				case EAST:
				case WEST:
					if (newArea->IsOverlappingY( adj ))
					{
						adj->ConnectTo( newArea, OppositeDirection( (NavDirType)d ) );
					}
					break;
				}
			}			
		}
	}

	TheNavAreas.push_back( newArea );
	TheNavMesh->AddNavArea( newArea );

	// Assign nodes
	if ( HasNodes() )
	{
		// first give it all our nodes...
		newArea->m_node[ NORTH_WEST ] = m_node[ NORTH_WEST ];
		newArea->m_node[ NORTH_EAST ] = m_node[ NORTH_EAST ];
		newArea->m_node[ SOUTH_EAST ] = m_node[ SOUTH_EAST ];
		newArea->m_node[ SOUTH_WEST ] = m_node[ SOUTH_WEST ];

		// ... then pull in one edge...
		NavDirType dir = NUM_DIRECTIONS;
		NavCornerType corner[2] = { NUM_CORNERS, NUM_CORNERS };

		switch ( ignoreEdge )
		{
		case NORTH:
			dir = SOUTH;
			corner[0] = NORTH_WEST;
			corner[1] = NORTH_EAST;
			break;
		case SOUTH:
			dir = NORTH;
			corner[0] = SOUTH_WEST;
			corner[1] = SOUTH_EAST;
			break;
		case EAST:
			dir = WEST;
			corner[0] = NORTH_EAST;
			corner[1] = SOUTH_EAST;
			break;
		case WEST:
			dir = EAST;
			corner[0] = NORTH_WEST;
			corner[1] = SOUTH_WEST;
			break;
		}

		while ( !newArea->IsOverlapping( *newArea->m_node[ corner[0] ]->GetPosition(), GenerationStepSize/2 ) )
		{
			for ( int i=0; i<2; ++i )
			{
				contract_assert( newArea->m_node[ corner[i] ] );
				contract_assert( newArea->m_node[ corner[i] ]->GetConnectedNode( dir ) );
				newArea->m_node[ corner[i] ] = newArea->m_node[ corner[i] ]->GetConnectedNode( dir );
			}
		}

		// assign internal nodes...
		newArea->AssignNodes( newArea );

		// ... and grab the node heights for our corner heights.
		newArea->m_neZ			= newArea->m_node[ NORTH_EAST ]->GetPosition()->z;
		newArea->m_nwCorner.z	= newArea->m_node[ NORTH_WEST ]->GetPosition()->z;
		newArea->m_swZ			= newArea->m_node[ SOUTH_WEST ]->GetPosition()->z;
		newArea->m_seCorner.z	= newArea->m_node[ SOUTH_EAST ]->GetPosition()->z;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Create a new area between this area and given area 
 */
bool CNavArea::SpliceEdit( CNavArea *other )
{
	CNavArea *newArea = nullptr;
	Vector nw, ne, se, sw;

	if (m_nwCorner.x > other->m_seCorner.x)
	{
		// 'this' is east of 'other'
		float top = std::max( m_nwCorner.y, other->m_nwCorner.y );
		float bottom = std::min( m_seCorner.y, other->m_seCorner.y );

		nw.x = other->m_seCorner.x;
		nw.y = top;
		nw.z = other->GetZ( nw );

		se.x = m_nwCorner.x;
		se.y = bottom;
		se.z = GetZ( se );

		ne.x = se.x;
		ne.y = nw.y;
		ne.z = GetZ( ne );

		sw.x = nw.x;
		sw.y = se.y;
		sw.z = other->GetZ( sw );

		newArea = TheNavMesh->CreateArea();
		if (newArea == nullptr)
		{
			Warning( "SpliceEdit: Out of memory.\n" );
			return false;
		}
		
		newArea->Build( nw, ne, se, sw );

		this->ConnectTo( newArea, WEST );
		newArea->ConnectTo( this, EAST );

		other->ConnectTo( newArea, EAST );
		newArea->ConnectTo( other, WEST );
	}
	else if (m_seCorner.x < other->m_nwCorner.x)
	{
		// 'this' is west of 'other'
		float top = std::max( m_nwCorner.y, other->m_nwCorner.y );
		float bottom = std::min( m_seCorner.y, other->m_seCorner.y );

		nw.x = m_seCorner.x;
		nw.y = top;
		nw.z = GetZ( nw );

		se.x = other->m_nwCorner.x;
		se.y = bottom;
		se.z = other->GetZ( se );

		ne.x = se.x;
		ne.y = nw.y;
		ne.z = other->GetZ( ne );

		sw.x = nw.x;
		sw.y = se.y;
		sw.z = GetZ( sw );

		newArea = TheNavMesh->CreateArea();
		if (newArea == nullptr)
		{
			Warning( "SpliceEdit: Out of memory.\n" );
			return false;
		}
		
		newArea->Build( nw, ne, se, sw );

		this->ConnectTo( newArea, EAST );
		newArea->ConnectTo( this, WEST );

		other->ConnectTo( newArea, WEST );
		newArea->ConnectTo( other, EAST );
	}
	else	// 'this' overlaps in X
	{
		if (m_nwCorner.y > other->m_seCorner.y)
		{
			// 'this' is south of 'other'
			float left = std::max( m_nwCorner.x, other->m_nwCorner.x );
			float right = std::min( m_seCorner.x, other->m_seCorner.x );

			nw.x = left;
			nw.y = other->m_seCorner.y;
			nw.z = other->GetZ( nw );

			se.x = right;
			se.y = m_nwCorner.y;
			se.z = GetZ( se );

			ne.x = se.x;
			ne.y = nw.y;
			ne.z = other->GetZ( ne );

			sw.x = nw.x;
			sw.y = se.y;
			sw.z = GetZ( sw );

			newArea = TheNavMesh->CreateArea();
			if (newArea == nullptr)
			{
				Warning( "SpliceEdit: Out of memory.\n" );
				return false;
			}
			
			newArea->Build( nw, ne, se, sw );

			this->ConnectTo( newArea, NORTH );
			newArea->ConnectTo( this, SOUTH );

			other->ConnectTo( newArea, SOUTH );
			newArea->ConnectTo( other, NORTH );
		}
		else if (m_seCorner.y < other->m_nwCorner.y)
		{
			// 'this' is north of 'other'
			float left = std::max( m_nwCorner.x, other->m_nwCorner.x );
			float right = std::min( m_seCorner.x, other->m_seCorner.x );

			nw.x = left;
			nw.y = m_seCorner.y;
			nw.z = GetZ( nw );

			se.x = right;
			se.y = other->m_nwCorner.y;
			se.z = other->GetZ( se );

			ne.x = se.x;
			ne.y = nw.y;
			ne.z = GetZ( ne );

			sw.x = nw.x;
			sw.y = se.y;
			sw.z = other->GetZ( sw );

			newArea = TheNavMesh->CreateArea();
			if (newArea == nullptr)
			{
				Warning( "SpliceEdit: Out of memory.\n" );
				return false;
			}
			
			newArea->Build( nw, ne, se, sw );

			this->ConnectTo( newArea, SOUTH );
			newArea->ConnectTo( this, NORTH );

			other->ConnectTo( newArea, NORTH );
			newArea->ConnectTo( other, SOUTH );
		}
		else
		{
			// areas overlap
			return false;
		}
	}

	newArea->InheritAttributes( this, other );

	TheNavAreas.push_back( newArea );
	TheNavMesh->AddNavArea( newArea );
	
	TheNavMesh->OnEditCreateNotify( newArea );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
* Calculates a constant ID for an area at this location, for debugging
*/
void CNavArea::CalcDebugID()
{
// 	if ( m_debugid == 0 )
// 	{
// 		// calculate a debug ID which will be constant for this nav area across generation runs
// 		int coord[6] = { (int) m_nwCorner.x, (int) m_nwCorner.x, (int) m_nwCorner.z, (int) m_seCorner.x, (int) m_seCorner.y, (int) m_seCorner.z };
// 		m_debugid = CRC32_ProcessSingleBuffer( &coord, sizeof( coord ) );
// 	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Merge this area and given adjacent area 
 */
bool CNavArea::MergeEdit( CNavArea *adj )
{
	// can only merge if attributes of both areas match

	// check that these areas can be merged
	const float tolerance = 1.0f;
	bool merge = false;
	if (fabs( m_nwCorner.x - adj->m_nwCorner.x ) < tolerance && 
		fabs( m_seCorner.x - adj->m_seCorner.x ) < tolerance)
		merge = true;

	if (fabs( m_nwCorner.y - adj->m_nwCorner.y ) < tolerance && 
		fabs( m_seCorner.y - adj->m_seCorner.y ) < tolerance)
		merge = true;

	if (merge == false)
		return false;

	Vector originalNWCorner = m_nwCorner;
	Vector originalSECorner = m_seCorner;
	
	// update extent
	if (m_nwCorner.x > adj->m_nwCorner.x || m_nwCorner.y > adj->m_nwCorner.y)
		m_nwCorner = adj->m_nwCorner;

	if (m_seCorner.x < adj->m_seCorner.x || m_seCorner.y < adj->m_seCorner.y)
		m_seCorner = adj->m_seCorner;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	if (m_seCorner.x > originalSECorner.x || m_nwCorner.y < originalNWCorner.y)
		m_neZ = adj->GetZ( m_seCorner.x, m_nwCorner.y );
	else
		m_neZ = GetZ( m_seCorner.x, m_nwCorner.y );

	if (m_nwCorner.x < originalNWCorner.x || m_seCorner.y > originalSECorner.y)
		m_swZ = adj->GetZ( m_nwCorner.x, m_seCorner.y );
	else
		m_swZ = GetZ( m_nwCorner.x, m_seCorner.y );

	// merge adjacency links - we gain all the connections that adjArea had
	MergeAdjacentConnections( adj );

	InheritAttributes( adj );

	// remove subsumed adjacent area
	ks::erase_first( TheNavAreas, adj );
	TheNavMesh->OnEditDestroyNotify( adj );
	TheNavMesh->DestroyArea( adj );
	
	TheNavMesh->OnEditCreateNotify( this );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::InheritAttributes( CNavArea *first, CNavArea *second )
{
	if ( first && second )
	{
		SetAttributes( first->GetAttributes() | second->GetAttributes() );

		// if both areas have the same place, the new area inherits it
		if ( first->GetPlace() == second->GetPlace() )
		{
			SetPlace( first->GetPlace() );
		}
		else if ( first->GetPlace() == UNDEFINED_PLACE )
		{
			SetPlace( second->GetPlace() );
		}
		else if ( second->GetPlace() == UNDEFINED_PLACE )
		{
			SetPlace( first->GetPlace() );
		}
		else
		{
			// both have valid, but different places - pick on at random
			if ( RandomInt( 0, 100 ) < 50 )
				SetPlace( first->GetPlace() );
			else
				SetPlace( second->GetPlace() );
		}
	}
	else if ( first )
	{
		SetAttributes( GetAttributes() | first->GetAttributes() );
		if ( GetPlace() == UNDEFINED_PLACE )
		{
			SetPlace( first->GetPlace() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void ApproachAreaAnalysisPrep( void )
{
}

//--------------------------------------------------------------------------------------------------------------
void CleanupApproachAreaAnalysisPrep( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove "analyzed" data from nav area
 */
void CNavArea::Strip( void )
{
	m_spotEncounters.clear();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if area is more or less square.
 * This is used when merging to prevent long, thin, areas being created.
 */
bool CNavArea::IsRoughlySquare( void ) const
{
	float aspect = GetSizeX() / GetSizeY();

	const float maxAspect = 3.01;
	const float minAspect = 1.0f / maxAspect;
	if (aspect < minAspect || aspect > maxAspect)
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'pos' is within 2D extents of area.
 */
bool CNavArea::IsOverlapping( const Vector &pos, float tolerance ) const
{
	if (pos.x + tolerance >= m_nwCorner.x && pos.x - tolerance <= m_seCorner.x &&
		pos.y + tolerance >= m_nwCorner.y && pos.y - tolerance <= m_seCorner.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our 2D extents
 */
bool CNavArea::IsOverlapping( const CNavArea *area ) const
{
	if (area->m_nwCorner.x < m_seCorner.x && area->m_seCorner.x > m_nwCorner.x && 
		area->m_nwCorner.y < m_seCorner.y && area->m_seCorner.y > m_nwCorner.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'extent' overlaps our 2D extents
 */
bool CNavArea::IsOverlapping( const Extent &extent ) const
{
	return ( extent.lo.x < m_seCorner.x && extent.hi.x > m_nwCorner.x && 
			 extent.lo.y < m_seCorner.y && extent.hi.y > m_nwCorner.y );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our X extent
 */
bool CNavArea::IsOverlappingX( const CNavArea *area ) const
{
	if (area->m_nwCorner.x < m_seCorner.x && area->m_seCorner.x > m_nwCorner.x)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'area' overlaps our Y extent
 */
bool CNavArea::IsOverlappingY( const CNavArea *area ) const
{
	if (area->m_nwCorner.y < m_seCorner.y && area->m_seCorner.y > m_nwCorner.y)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
class COverlapCheck
{
public:
	COverlapCheck( const CNavArea *me, const Vector &pos ) : m_pos( pos )
	{
		m_me = me;
		m_myZ = me->GetZ( pos );
	}

	bool operator() ( CNavArea *area )
	{
		// skip self
		if ( area == m_me )
			return true;

		// check 2D overlap
		if ( !area->IsOverlapping( m_pos ) )
			return true;

		float theirZ = area->GetZ( m_pos );
		if ( theirZ > m_pos.z )
		{
			// they are above the point
			return true;
		}

		if ( theirZ > m_myZ )
		{
			// we are below an area that is beneath the given position
			return false;
		}

		return true;
	}

	const CNavArea *m_me;
	float m_myZ;
	const Vector &m_pos;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given point is on or above this area, but no others
 */
bool CNavArea::Contains( const Vector &pos ) const
{
	// check 2D overlap
	if (!IsOverlapping( pos ))
		return false;

	// the point overlaps us, check that it is above us, but not above any areas that overlap us
	float myZ = GetZ( pos );

	// if the nav area is above the given position, fail
	// allow nav area to be as much as a step height above the given position
	if (myZ - StepHeight > pos.z)
		return false;

	Extent areaExtent;
	GetExtent( &areaExtent );

	COverlapCheck overlap( this, pos );
	return TheNavMesh->ForAllAreasOverlappingExtent( overlap, areaExtent );
}

//--------------------------------------------------------------------------------------------------------------
/**
* Returns true if area completely contains other area
*/
bool CNavArea::Contains( const CNavArea *area ) const
{
	return ( ( m_nwCorner.x <= area->m_nwCorner.x ) && ( m_seCorner.x >= area->m_seCorner.x ) &&
		( m_nwCorner.y <= area->m_nwCorner.y ) && ( m_seCorner.y >= area->m_seCorner.y ) &&
		( m_nwCorner.z <= area->m_nwCorner.z ) && ( m_seCorner.z >= area->m_seCorner.z ) );
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::ComputeNormal( Vector *normal, bool alternate ) const
{
	if ( !normal )
		return;

	Vector u, v;

	if ( !alternate )
	{
		u.x = m_seCorner.x - m_nwCorner.x;
		u.y = 0.0f;
		u.z = m_neZ - m_nwCorner.z;

		v.x = 0.0f;
		v.y = m_seCorner.y - m_nwCorner.y;
		v.z = m_swZ - m_nwCorner.z;
	}
	else
	{
		u.x = m_nwCorner.x - m_seCorner.x;
		u.y = 0.0f;
		u.z = m_swZ - m_seCorner.z;

		v.x = 0.0f;
		v.y = m_nwCorner.y - m_seCorner.y;
		v.z = m_neZ - m_seCorner.z;
	}

	*normal = CrossProduct( u, v );
	normal->NormalizeInPlace();
}

//--------------------------------------------------------------------------------------------------------------
/**
* Removes all connections in directions to left and right of specified direction
*/
void CNavArea::RemoveOrthogonalConnections( NavDirType dir )
{
	NavDirType dirToRemove[2];
	dirToRemove[0] = DirectionLeft( dir );
	dirToRemove[1] = DirectionRight( dir );
	for ( int i = 0; i < 2; i++ )
	{
		dir = dirToRemove[i];
		while ( GetAdjacentCount( dir ) > 0 )
		{
			CNavArea *adj = GetAdjacentArea( dir, 0 );
			Disconnect( adj );
			adj->Disconnect( this );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the area is approximately flat, using normals computed from opposite corners
 */
bool CNavArea::IsFlat( void ) const
{
	Vector normal, otherNormal;
	ComputeNormal( &normal );
	ComputeNormal( &otherNormal, true );

	float tolerance = nav_coplanar_slope_limit.GetFloat();
	if ( ( m_node[ NORTH_WEST ] && m_node[ NORTH_WEST ]->IsOnDisplacement() ) ||
		( m_node[ NORTH_EAST ] && m_node[ NORTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_EAST ] && m_node[ SOUTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_WEST ] && m_node[ SOUTH_WEST ]->IsOnDisplacement() ) )
	{
		tolerance = nav_coplanar_slope_limit_displacement.GetFloat();
	}

	if (DotProduct( normal, otherNormal ) > tolerance)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this area and given area are approximately co-planar
 */
bool CNavArea::IsCoplanar( const CNavArea *area ) const
{
	Vector u, v;

	bool isOnDisplacement = ( m_node[ NORTH_WEST ] && m_node[ NORTH_WEST ]->IsOnDisplacement() ) ||
		( m_node[ NORTH_EAST ] && m_node[ NORTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_EAST ] && m_node[ SOUTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_WEST ] && m_node[ SOUTH_WEST ]->IsOnDisplacement() );

	if ( !isOnDisplacement && !IsFlat() )
		return false;

	bool areaIsOnDisplacement = ( area->m_node[ NORTH_WEST ] && area->m_node[ NORTH_WEST ]->IsOnDisplacement() ) ||
		( area->m_node[ NORTH_EAST ] && area->m_node[ NORTH_EAST ]->IsOnDisplacement() ) ||
		( area->m_node[ SOUTH_EAST ] && area->m_node[ SOUTH_EAST ]->IsOnDisplacement() ) ||
		( area->m_node[ SOUTH_WEST ] && area->m_node[ SOUTH_WEST ]->IsOnDisplacement() );

	if ( !areaIsOnDisplacement && !area->IsFlat() )
		return false;

	// compute our unit surface normal
	Vector normal, otherNormal;
	ComputeNormal( &normal );
	area->ComputeNormal( &otherNormal );

	// can only merge areas that are nearly planar, to ensure areas do not differ from underlying geometry much
	float tolerance = nav_coplanar_slope_limit.GetFloat();
	if ( ( m_node[ NORTH_WEST ] && m_node[ NORTH_WEST ]->IsOnDisplacement() ) ||
		( m_node[ NORTH_EAST ] && m_node[ NORTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_EAST ] && m_node[ SOUTH_EAST ]->IsOnDisplacement() ) ||
		( m_node[ SOUTH_WEST ] && m_node[ SOUTH_WEST ]->IsOnDisplacement() ) )
	{
		tolerance = nav_coplanar_slope_limit_displacement.GetFloat();
	}

	if (DotProduct( normal, otherNormal ) > tolerance)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return Z of area at (x,y) of 'pos'
 * Trilinear interpolation of Z values at quad edges.
 * NOTE: pos->z is not used.
 */

float CNavArea::GetZ( float x, float y ) const __restrict__
{
	// guard against division by zero due to degenerate areas
	if (m_invDxCorners == 0.0f || m_invDyCorners == 0.0f)
		return m_neZ;

	float u = (x - m_nwCorner.x) * m_invDxCorners;
	float v = (y - m_nwCorner.y) * m_invDyCorners;

	// clamp Z values to (x,y) volume
	
	u = fsel( u, u, 0 );			// u >= 0 ? u : 0
	u = fsel( u - 1.0f, 1.0f, u );	// u >= 1 ? 1 : u

	v = fsel( v, v, 0 );			// v >= 0 ? v : 0
	v = fsel( v - 1.0f, 1.0f, v );	// v >= 1 ? 1 : v

	float northZ = m_nwCorner.z + u * (m_neZ - m_nwCorner.z);
	float southZ = m_swZ + u * (m_seCorner.z - m_swZ);

	return northZ + v * (southZ - northZ);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return closest point to 'pos' on 'area'.
 * Returned point is in 'close'.
 */
void CNavArea::GetClosestPointOnArea( const Vector * __restrict__ pPos, Vector *close ) const __restrict__ 
{
	float x, y, z;

	// Using fsel rather than compares, as much faster on 360 [7/28/2008 tom]
	x = fsel( pPos->x - m_nwCorner.x, pPos->x, m_nwCorner.x );
	x = fsel( x - m_seCorner.x, m_seCorner.x, x );

	y = fsel( pPos->y - m_nwCorner.y, pPos->y, m_nwCorner.y );
	y = fsel( y - m_seCorner.y, m_seCorner.y, y );

	z = GetZ( x, y );

	close->Init( x, y, z );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return shortest distance squared between point and this area
 */
float CNavArea::GetDistanceSquaredToPoint( const Vector &pos ) const
{
	if (pos.x < m_nwCorner.x)
	{
		if (pos.y < m_nwCorner.y)
		{
			// position is north-west of area
			return (m_nwCorner - pos).LengthSqr();
		}
		else if (pos.y > m_seCorner.y)
		{
			// position is south-west of area
			Vector d;
			d.x = m_nwCorner.x - pos.x;
			d.y = m_seCorner.y - pos.y;
			d.z = m_swZ - pos.z;
			return d.LengthSqr();
		}
		else
		{
			// position is west of area
			float d = m_nwCorner.x - pos.x;
			return d * d;
		}
	}
	else if (pos.x > m_seCorner.x)
	{
		if (pos.y < m_nwCorner.y)
		{
			// position is north-east of area
			Vector d;
			d.x = m_seCorner.x - pos.x;
			d.y = m_nwCorner.y - pos.y;
			d.z = m_neZ - pos.z;
			return d.LengthSqr();
		}
		else if (pos.y > m_seCorner.y)
		{
			// position is south-east of area
			return (m_seCorner - pos).LengthSqr();
		}
		else
		{
			// position is east of area
			float d = pos.x - m_seCorner.x;
			return d * d;
		}
	}
	else if (pos.y < m_nwCorner.y)
	{
		// position is north of area
		float d = m_nwCorner.y - pos.y;
		return d * d;
	}
	else if (pos.y > m_seCorner.y)
	{
		// position is south of area
		float d = pos.y - m_seCorner.y;
		return d * d;
	}
	else
	{
		// position is inside of 2D extent of area - find delta Z
		float z = GetZ( pos );
		float d = z - pos.z;
		return d * d;
	}
}

//--------------------------------------------------------------------------------------------------------------
CNavArea *CNavArea::GetRandomAdjacentArea( NavDirType dir ) const
{
	int count = std::ssize( m_connect[ dir ] );
	int which = RandomInt( 0, count-1 );

	int i = 0;
	for ( const NavConnect &connect : m_connect[ dir ] )
	{
		if (i == which)
			return connect.area;

		++i;
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute "portal" between two adjacent areas. 
 * Return center of portal opening, and half-width defining sides of portal from center.
 * NOTE: center->z is unset.
 */
void CNavArea::ComputePortal( const CNavArea *to, NavDirType dir, Vector *center, float *halfWidth ) const
{
	if ( dir == NORTH || dir == SOUTH )
	{
		if ( dir == NORTH )
		{
			center->y = m_nwCorner.y;
		}
		else
		{
			center->y = m_seCorner.y;
		}

		float left = std::max( m_nwCorner.x, to->m_nwCorner.x );
		float right = std::min( m_seCorner.x, to->m_seCorner.x );

		// clamp to our extent in case areas are disjoint
		if ( left < m_nwCorner.x )
		{
			left = m_nwCorner.x;
		}
		else if ( left > m_seCorner.x )
		{
			left = m_seCorner.x;
		}

		if ( right < m_nwCorner.x )
		{
			right = m_nwCorner.x;
		}
		else if ( right > m_seCorner.x )
		{
			right = m_seCorner.x;
		}

		center->x = ( left + right )/2.0f;
		*halfWidth = ( right - left )/2.0f;
	}
	else	// EAST or WEST
	{
		if ( dir == WEST )
		{
			center->x = m_nwCorner.x;
		}
		else
		{
			center->x = m_seCorner.x;
		}

		float top = std::max( m_nwCorner.y, to->m_nwCorner.y );
		float bottom = std::min( m_seCorner.y, to->m_seCorner.y );

		// clamp to our extent in case areas are disjoint
		if ( top < m_nwCorner.y )
		{
			top = m_nwCorner.y;
		}
		else if ( top > m_seCorner.y )
		{
			top = m_seCorner.y;
		}

		if ( bottom < m_nwCorner.y )
		{
			bottom = m_nwCorner.y;
		}
		else if ( bottom > m_seCorner.y )
		{
			bottom = m_seCorner.y;
		}

		center->y = (top + bottom)/2.0f;
		*halfWidth = (bottom - top)/2.0f;
	}

	center->z = GetZ( center->x, center->y );
}

//--------------------------------------------------------------------------------------------------------------
// compute largest portal to adjacent area, returning direction
NavDirType CNavArea::ComputeLargestPortal( const CNavArea *to, Vector *center, float *halfWidth ) const
{
	NavDirType bestDir = NUM_DIRECTIONS;
	Vector bestCenter( vec3_origin );
	float bestHalfWidth = 0.0f;

	Vector centerDir = to->GetCenter() - GetCenter();

	for ( int i=0; i<NUM_DIRECTIONS; ++i )
	{
		NavDirType testDir = (NavDirType)i;
		Vector testCenter;
		float testHalfWidth;

		// Make sure we're not picking the opposite direction
		switch ( testDir )
		{
		case NORTH:	// -y
			if ( centerDir.y >= 0.0f )
				continue;
			break;
		case SOUTH:	// +y
			if ( centerDir.y <= 0.0f )
				continue;
			break;
		case WEST:	// -x
			if ( centerDir.x >= 0.0f )
				continue;
			break;
		case EAST:	// +x
			if ( centerDir.x <= 0.0f )
				continue;
			break;
		}

		ComputePortal( to, testDir, &testCenter, &testHalfWidth );
		if ( testHalfWidth > bestHalfWidth )
		{
			bestDir = testDir;
			bestCenter = testCenter;
			bestHalfWidth = testHalfWidth;
		}
	}

	*center = bestCenter;
	*halfWidth = bestHalfWidth;
	return bestDir;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute closest point within the "portal" between to adjacent areas. 
 */
void CNavArea::ComputeClosestPointInPortal( const CNavArea *to, NavDirType dir, const Vector &fromPos, Vector *closePos ) const
{
//	const float margin = 0.0f; //GenerationStepSize/2.0f;  // causes trouble with very small/narrow nav areas
	const float margin = GenerationStepSize;

	if ( dir == NORTH || dir == SOUTH )
	{
		if ( dir == NORTH )
		{
			closePos->y = m_nwCorner.y;
		}
		else
		{
			closePos->y = m_seCorner.y;
		}

		float left = std::max( m_nwCorner.x, to->m_nwCorner.x );
		float right = std::min( m_seCorner.x, to->m_seCorner.x );

		// clamp to our extent in case areas are disjoint
		// no good - need to push into to area for margins
		/*
		if (left < m_nwCorner.x)
			left = m_nwCorner.x;
		else if (left > m_seCorner.x)
			left = m_seCorner.x;

		if (right < m_nwCorner.x)
			right = m_nwCorner.x;
		else if (right > m_seCorner.x)
			right = m_seCorner.x;
			*/

		// keep margin if against edge
		/// @todo Need better check whether edge is outer edge or not - partial overlap is missed
		float leftMargin = ( to->IsEdge( WEST ) ) ? ( left + margin ) : left;
		float rightMargin = ( to->IsEdge( EAST ) ) ? ( right - margin ) : right;
		
		// if area is narrow, margins may have crossed
		if ( leftMargin > rightMargin )
		{
			// use midline
			float mid = ( left + right )/2.0f;
			leftMargin = mid;
			rightMargin = mid;
		}

		// limit x to within portal
		if ( fromPos.x < leftMargin )
		{
			closePos->x = leftMargin;
		}
		else if ( fromPos.x > rightMargin )
		{
			closePos->x = rightMargin;
		}
		else
		{
			closePos->x = fromPos.x;
		}
	}
	else	// EAST or WEST
	{
		if ( dir == WEST )
		{
			closePos->x = m_nwCorner.x;
		}
		else
		{
			closePos->x = m_seCorner.x;
		}

		float top = std::max( m_nwCorner.y, to->m_nwCorner.y );
		float bottom = std::min( m_seCorner.y, to->m_seCorner.y );

		// clamp to our extent in case areas are disjoint
		// no good - need to push into to area for margins
		/*
		if (top < m_nwCorner.y)
			top = m_nwCorner.y;
		else if (top > m_seCorner.y)
			top = m_seCorner.y;

		if (bottom < m_nwCorner.y)
			bottom = m_nwCorner.y;
		else if (bottom > m_seCorner.y)
			bottom = m_seCorner.y;
		*/
		
		// keep margin if against edge
		float topMargin = ( to->IsEdge( NORTH ) ) ? ( top + margin ) : top;
		float bottomMargin = ( to->IsEdge( SOUTH ) ) ? ( bottom - margin ) : bottom;

		// if area is narrow, margins may have crossed
		if ( topMargin > bottomMargin )
		{
			// use midline
			float mid = ( top + bottom )/2.0f;
			topMargin = mid;
			bottomMargin = mid;
		}

		// limit y to within portal
		if ( fromPos.y < topMargin )
		{
			closePos->y = topMargin;
		}
		else if ( fromPos.y > bottomMargin )
		{
			closePos->y = bottomMargin;
		}
		else
		{
			closePos->y = fromPos.y;
		}
	}

	closePos->z = GetZ( closePos->x, closePos->y );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given area and 'other' share a colinear edge (ie: no drop-down or step/jump/climb)
 */
bool CNavArea::IsContiguous( const CNavArea *other ) const
{
	CVProfScope _( "CNavArea::IsContiguous", 0, "NextBot", false, BUDGETFLAG_OTHER );

	// find which side it is connected on
	int dir;
	for( dir=0; dir<NUM_DIRECTIONS; ++dir )
	{
		if ( IsConnected( other, (NavDirType)dir ) )
			break;
	}

	if ( dir == NUM_DIRECTIONS )
		return false;

	Vector myEdge;
	float halfWidth;
	ComputePortal( other, (NavDirType)dir, &myEdge, &halfWidth );

	Vector otherEdge;
	other->ComputePortal( this, OppositeDirection( (NavDirType)dir ), &otherEdge, &halfWidth );

	// must use stepheight because rough terrain can have gaps/cracks between adjacent nav areas
	return ( myEdge - otherEdge ).IsLengthLessThan( StepHeight );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return height change between edges of adjacent nav areas (not actual underlying ground)
 */
float CNavArea::ComputeAdjacentConnectionHeightChange( const CNavArea *destinationArea ) const
{
	CVProfScope _( "CNavArea::ComputeAdjacentConnectionHeightChange", 0, "NextBot", false, BUDGETFLAG_OTHER );

	// find which side it is connected on
	int dir;
	for( dir=0; dir<NUM_DIRECTIONS; ++dir )
	{
		if ( IsConnected( destinationArea, (NavDirType)dir ) )
			break;
	}

	if ( dir == NUM_DIRECTIONS )
		return std::numeric_limits<float>::max();

	Vector myEdge;
	float halfWidth;
	ComputePortal( destinationArea, (NavDirType)dir, &myEdge, &halfWidth );

	Vector otherEdge;
	destinationArea->ComputePortal( this, OppositeDirection( (NavDirType)dir ), &otherEdge, &halfWidth );

	return otherEdge.z - myEdge.z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if there are no bi-directional links on the given side
 */
bool CNavArea::IsEdge( NavDirType dir ) const
{
	for ( const NavConnect &connect : m_connect[ dir ] )
	{

		if (connect.area->IsConnected( this, OppositeDirection( dir ) ))
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return direction from this area to the given point
 */
NavDirType CNavArea::ComputeDirection( Vector *point ) const
{
	if (point->x >= m_nwCorner.x && point->x <= m_seCorner.x)
	{
		if (point->y < m_nwCorner.y)
			return NORTH;
		else if (point->y > m_seCorner.y)
			return SOUTH;
	}
	else if (point->y >= m_nwCorner.y && point->y <= m_seCorner.y)
	{
		if (point->x < m_nwCorner.x)
			return WEST;
		else if (point->x > m_seCorner.x)
			return EAST;
	}

	// find closest direction
	Vector to = *point - GetCenter();

	if (fabs(to.x) > fabs(to.y))
	{
		if (to.x > 0.0f)
			return EAST;
		return WEST;
	}
	else
	{
		if (to.y > 0.0f)
			return SOUTH;
		return NORTH;
	}

	return NUM_DIRECTIONS;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::GetCornerHotspot( NavCornerType corner, Vector hotspot[NUM_CORNERS] ) const
{
	Vector nw = GetCorner( NORTH_WEST );
	Vector ne = GetCorner( NORTH_EAST );
	Vector sw = GetCorner( SOUTH_WEST );
	Vector se = GetCorner( SOUTH_EAST );

	float size = 9.0f;
	size = std::min( size, GetSizeX()/3 );	// make sure the hotspot doesn't extend outside small areas
	size = std::min( size, GetSizeY()/3 );

	switch ( corner )
	{
	case NORTH_WEST:
		hotspot[0] = nw;
		hotspot[1] = hotspot[0] + Vector( size, 0, 0 );
		hotspot[2] = hotspot[0] + Vector( size, size, 0 );
		hotspot[3] = hotspot[0] + Vector( 0, size, 0 );
		break;
	case NORTH_EAST:
		hotspot[0] = ne;
		hotspot[1] = hotspot[0] + Vector( -size, 0, 0 );
		hotspot[2] = hotspot[0] + Vector( -size, size, 0 );
		hotspot[3] = hotspot[0] + Vector( 0, size, 0 );
		break;
	case SOUTH_WEST:
		hotspot[0] = sw;
		hotspot[1] = hotspot[0] + Vector( size, 0, 0 );
		hotspot[2] = hotspot[0] + Vector( size, -size, 0 );
		hotspot[3] = hotspot[0] + Vector( 0, -size, 0 );
		break;
	case SOUTH_EAST:
		hotspot[0] = se;
		hotspot[1] = hotspot[0] + Vector( -size, 0, 0 );
		hotspot[2] = hotspot[0] + Vector( -size, -size, 0 );
		hotspot[3] = hotspot[0] + Vector( 0, -size, 0 );
		break;
	default:
		return false;
	}

	for ( int i=1; i<NUM_CORNERS; ++i )
	{
		hotspot[i].z = GetZ( hotspot[i] );
	}

	Vector eyePos, eyeForward;
	TheNavMesh->GetEditVectors( &eyePos, &eyeForward );

	Ray_t ray;
	ray.Init( eyePos, eyePos + 10000.0f * eyeForward, vec3_origin, vec3_origin );

	float dist = IntersectRayWithTriangle( ray, hotspot[0], hotspot[1], hotspot[2], false );
	if ( dist > 0 )
	{
		return true;
	}

	dist = IntersectRayWithTriangle( ray, hotspot[2], hotspot[3], hotspot[0], false );
	if ( dist > 0 )
	{
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
NavCornerType CNavArea::GetCornerUnderCursor( void ) const
{
	Vector eyePos, eyeForward;
	TheNavMesh->GetEditVectors( &eyePos, &eyeForward );

	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		Vector hotspot[NUM_CORNERS];
		if ( GetCornerHotspot( (NavCornerType)i, hotspot ) )
		{
			return (NavCornerType)i;
		}
	}

	return NUM_CORNERS;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw area for debugging
 */
void CNavArea::Draw( void ) const
{
	NavEditColor color;
	bool useAttributeColors = true;

	const float DebugDuration = NDEBUG_PERSIST_TILL_NEXT_SERVER;

	if ( TheNavMesh->IsEditMode( CNavMesh::PLACE_PAINTING ) )
	{
		useAttributeColors = false;

		if ( m_place == UNDEFINED_PLACE )
		{
			color = NavNoPlaceColor;
		}
		else if ( TheNavMesh->GetNavPlace() == m_place )
		{
			color = NavSamePlaceColor;
		}
		else
		{
			color = NavDifferentPlaceColor;
		}
	}
	else
	{
		// normal edit mode
		if ( this == TheNavMesh->GetMarkedArea() )
		{
			useAttributeColors = false;
			color = NavMarkedColor;
		}
		else if ( this == TheNavMesh->GetSelectedArea() )
		{
			color = NavSelectedColor;
		}
		else
		{
			color = NavNormalColor;
		}
	}

	if ( IsDegenerate() )
	{
		static IntervalTimer blink;
		static bool blinkOn = false;

		if (blink.GetElapsedTime() > 1.0f)
		{
			blink.Reset();
			blinkOn = !blinkOn;
		}

		useAttributeColors = false;

		if (blinkOn)
			color = NavDegenerateFirstColor;
		else
			color = NavDegenerateSecondColor;

		NDebugOverlay::Text( GetCenter(), UTIL_VarArgs( "Degenerate area %d", GetID() ), true, DebugDuration );
	}

	Vector nw, ne, sw, se;

	nw = m_nwCorner;
	se = m_seCorner;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	if ( nav_show_light_intensity.GetBool() )
	{
		for ( int i=0; i<NUM_CORNERS; ++i )
		{
			Vector pos = GetCorner( (NavCornerType)i );
			Vector end = pos;
			float lightIntensity = GetLightIntensity(pos);
			end.z += HumanHeight*lightIntensity;
			lightIntensity *= 255; // for color
			NDebugOverlay::Line( end, pos, lightIntensity, lightIntensity, std::max<std::common_type_t<int, float>>( 192, lightIntensity ), true, DebugDuration );
		}
	}

	int bgcolor[4];
	if ( 4 == std::sscanf( nav_area_bgcolor.GetString(), "%d %d %d %d", &(bgcolor[0]), &(bgcolor[1]), &(bgcolor[2]), &(bgcolor[3]) ) )
	{
		for ( int i=0; i<4; ++i )
			bgcolor[i] = clamp( bgcolor[i], 0, 255 );

		if ( bgcolor[3] > 0 )
		{
			const Vector offset( 0, 0, 0.8f );
			NDebugOverlay::Triangle( nw+offset, se+offset, ne+offset, bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3], true, DebugDuration );
			NDebugOverlay::Triangle( se+offset, nw+offset, sw+offset, bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3], true, DebugDuration );
		}
	}

	const float inset = 0.2f;
	nw.x += inset;
	nw.y += inset;
	ne.x -= inset;
	ne.y += inset;
	sw.x += inset;
	sw.y -= inset;
	se.x -= inset;
	se.y -= inset;

	if ( GetAttributes() & NAV_MESH_TRANSIENT )
	{
		NavDrawDashedLine( nw, ne, color );
		NavDrawDashedLine( ne, se, color );
		NavDrawDashedLine( se, sw, color );
		NavDrawDashedLine( sw, nw, color );
	}
	else
	{
		NavDrawLine( nw, ne, color );
		NavDrawLine( ne, se, color );
		NavDrawLine( se, sw, color );
		NavDrawLine( sw, nw, color );
	}

	if ( this == TheNavMesh->GetMarkedArea() && TheNavMesh->m_markedCorner != NUM_CORNERS )
	{
		Vector p[NUM_CORNERS];
		GetCornerHotspot( TheNavMesh->m_markedCorner, p );

		NavDrawLine( p[1], p[2], NavMarkedColor );
		NavDrawLine( p[2], p[3], NavMarkedColor );
	}
	if ( this != TheNavMesh->GetMarkedArea() && this == TheNavMesh->GetSelectedArea() && TheNavMesh->IsEditMode( CNavMesh::NORMAL ) )
	{
		NavCornerType bestCorner = GetCornerUnderCursor();

		Vector p[NUM_CORNERS];
		if ( GetCornerHotspot( bestCorner, p ) )
		{
			NavDrawLine( p[1], p[2], NavSelectedColor );
			NavDrawLine( p[2], p[3], NavSelectedColor );
		}
	}

	if (GetAttributes() & NAV_MESH_CROUCH)
	{
		if ( useAttributeColors )
			color = NavAttributeCrouchColor;

		NavDrawLine( nw, se, color );
	}

	if (GetAttributes() & NAV_MESH_JUMP)
	{
		if ( useAttributeColors )
			color = NavAttributeJumpColor;

		if ( !(GetAttributes() & NAV_MESH_CROUCH) )
		{
			NavDrawLine( nw, se, color );
		}
		NavDrawLine( ne, sw, color );
	}

	if (GetAttributes() & NAV_MESH_PRECISE)
	{
		if ( useAttributeColors )
			color = NavAttributePreciseColor;

		float size = 8.0f;
		Vector m_center = GetCenter();
		Vector up( m_center.x, m_center.y - size, m_center.z );
		Vector down( m_center.x, m_center.y + size, m_center.z );
		NavDrawLine( up, down, color );

		Vector left( m_center.x - size, m_center.y, m_center.z );
		Vector right( m_center.x + size, m_center.y, m_center.z );
		NavDrawLine( left, right, color );
	}

	if (GetAttributes() & NAV_MESH_NO_JUMP)
	{
		if ( useAttributeColors )
			color = NavAttributeNoJumpColor;

		float size = 8.0f;
		Vector m_center = GetCenter();
		Vector up( m_center.x, m_center.y - size, m_center.z );
		Vector down( m_center.x, m_center.y + size, m_center.z );
		Vector left( m_center.x - size, m_center.y, m_center.z );
		Vector right( m_center.x + size, m_center.y, m_center.z );
		NavDrawLine( up, right, color );
		NavDrawLine( right, down, color );
		NavDrawLine( down, left, color );
		NavDrawLine( left, up, color );
	}

	if (GetAttributes() & NAV_MESH_STAIRS)
	{
		if ( useAttributeColors )
			color = NavAttributeStairColor;

		float northZ = ( GetCorner( NORTH_WEST ).z + GetCorner( NORTH_EAST ).z ) / 2.0f;
		float southZ = ( GetCorner( SOUTH_WEST ).z + GetCorner( SOUTH_EAST ).z ) / 2.0f;
		float westZ = ( GetCorner( NORTH_WEST ).z + GetCorner( SOUTH_WEST ).z ) / 2.0f;
		float eastZ = ( GetCorner( NORTH_EAST ).z + GetCorner( SOUTH_EAST ).z ) / 2.0f;

		float deltaEastWest = abs( westZ - eastZ );
		float deltaNorthSouth = abs( northZ - southZ );

		float stepSize = StepHeight / 2.0f;
		float t;

		if ( deltaEastWest > deltaNorthSouth )
		{
			float inc = stepSize / GetSizeX();

			for( t = 0.0f; t <= 1.0f; t += inc )
			{
				float x = m_nwCorner.x + t * GetSizeX();
				
				NavDrawLine( Vector( x, m_nwCorner.y, GetZ( x, m_nwCorner.y ) ), 
							 Vector( x, m_seCorner.y, GetZ( x, m_seCorner.y ) ),
							 color );
			}
		}
		else
		{
			float inc = stepSize / GetSizeY();

			for( t = 0.0f; t <= 1.0f; t += inc )
			{
				float y = m_nwCorner.y + t * GetSizeY();

				NavDrawLine( Vector( m_nwCorner.x, y, GetZ( m_nwCorner.x, y ) ),
							 Vector( m_seCorner.x, y, GetZ( m_seCorner.x, y ) ),
							 color );
			}
		}
	}

	// Stop is represented by an octagon
	if (GetAttributes() & NAV_MESH_STOP)
	{
		if ( useAttributeColors )
			color = NavAttributeStopColor;

		float dist = 8.0f;
		float length = dist/2.5f;
		Vector start, end;

		Vector m_center = GetCenter();

		start =	m_center + Vector( dist, -length, 0 );
		end =	m_center + Vector( dist,  length, 0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector(   dist, length, 0 );
		end =	m_center + Vector( length, dist,   0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( -dist, -length, 0 );
		end =	m_center + Vector( -dist,  length, 0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( -dist,   length, 0 );
		end =	m_center + Vector( -length, dist,   0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( -length,  dist, 0 );
		end =	m_center + Vector(  length,  dist, 0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( -dist,   -length, 0 );
		end =	m_center + Vector( -length, -dist,   0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( -length, -dist, 0 );
		end =	m_center + Vector(  length, -dist, 0 );
		NavDrawLine( start, end, color );

		start =	m_center + Vector( length, -dist,   0 );
		end =	m_center + Vector( dist,   -length, 0 );
		NavDrawLine( start, end, color );
	}

	// Walk is represented by an arrow
	if (GetAttributes() & NAV_MESH_WALK)
	{
		if ( useAttributeColors )
			color = NavAttributeWalkColor;

		float size = 8.0f;
		Vector m_center = GetCenter();
		NavDrawHorizontalArrow( m_center + Vector( -size, 0, 0 ), m_center + Vector( size, 0, 0 ), 4, color );
	}

	// Walk is represented by a double arrow
	if (GetAttributes() & NAV_MESH_RUN)
	{
		if ( useAttributeColors )
			color = NavAttributeRunColor;

		float size = 8.0f;
		float dist = 4.0f;
		Vector m_center = GetCenter();
		NavDrawHorizontalArrow( m_center + Vector( -size,  dist, 0 ), m_center + Vector( size,  dist, 0 ), 4, color );
		NavDrawHorizontalArrow( m_center + Vector( -size, -dist, 0 ), m_center + Vector( size, -dist, 0 ), 4, color );
	}

	// Avoid is represented by an exclamation point
	if (GetAttributes() & NAV_MESH_AVOID)
	{
		if ( useAttributeColors )
			color = NavAttributeAvoidColor;

		float topHeight = 8.0f;
		float topWidth = 3.0f;
		float bottomHeight = 3.0f;
		float bottomWidth = 2.0f;
		Vector m_center = GetCenter();
		NavDrawTriangle( m_center, m_center + Vector( -topWidth, topHeight, 0 ), m_center + Vector( +topWidth, topHeight, 0 ), color );
		NavDrawTriangle( m_center + Vector( 0, -bottomHeight, 0 ), m_center + Vector( -bottomWidth, -bottomHeight*2, 0 ), m_center + Vector( bottomWidth, -bottomHeight*2, 0 ), color );
	}

	if ( IsBlocked( TEAM_ANY ) || HasAvoidanceObstacle() || IsDamaging() )
	{
		NavEditColor color = (IsBlocked( TEAM_ANY ) && ( m_attributeFlags & NAV_MESH_NAV_BLOCKER ) ) ? NavBlockedByFuncNavBlockerColor : NavBlockedByDoorColor;
		const float blockedInset = 4.0f;
		nw.x += blockedInset;
		nw.y += blockedInset;
		ne.x -= blockedInset;
		ne.y += blockedInset;
		sw.x += blockedInset;
		sw.y -= blockedInset;
		se.x -= blockedInset;
		se.y -= blockedInset;
		NavDrawLine( nw, ne, color );
		NavDrawLine( ne, se, color );
		NavDrawLine( se, sw, color );
		NavDrawLine( sw, nw, color );
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * Draw area as a filled rect of the given color
 */
void CNavArea::DrawFilled( int r, int g, int b, int a, float deltaT, bool noDepthTest, float margin ) const
{
	Vector nw = GetCorner( NORTH_WEST ) + Vector( margin, margin, 0.0f );
	Vector ne = GetCorner( NORTH_EAST ) + Vector( -margin, margin, 0.0f );
	Vector sw = GetCorner( SOUTH_WEST ) + Vector( margin, -margin, 0.0f );
	Vector se = GetCorner( SOUTH_EAST ) + Vector( -margin, -margin, 0.0f );

	NDebugOverlay::Triangle( nw, se, ne, r, g, b, a, noDepthTest, deltaT );
	NDebugOverlay::Triangle( se, nw, sw, r, g, b, a, noDepthTest, deltaT );

	// backside
// 	NDebugOverlay::Triangle( nw, ne, se, r, g, b, a, noDepthTest, deltaT );
// 	NDebugOverlay::Triangle( se, sw, nw, r, g, b, a, noDepthTest, deltaT );
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::DrawSelectedSet( const Vector &shift ) const
{
	const float deltaT = NDEBUG_PERSIST_TILL_NEXT_SERVER;
	int r = s_selectedSetColor.r();
	int g = s_selectedSetColor.g();
	int b = s_selectedSetColor.b();
	int a = s_selectedSetColor.a();

	Vector nw = GetCorner( NORTH_WEST ) + shift;
	Vector ne = GetCorner( NORTH_EAST ) + shift;
	Vector sw = GetCorner( SOUTH_WEST ) + shift;
	Vector se = GetCorner( SOUTH_EAST ) + shift;

	NDebugOverlay::Triangle( nw, se, ne, r, g, b, a, true, deltaT );
	NDebugOverlay::Triangle( se, nw, sw, r, g, b, a, true, deltaT );

	r = s_selectedSetBorderColor.r();
	g = s_selectedSetBorderColor.g();
	b = s_selectedSetBorderColor.b();
	NDebugOverlay::Line( nw, ne, r, g, b, true, deltaT );
	NDebugOverlay::Line( nw, sw, r, g, b, true, deltaT );
	NDebugOverlay::Line( sw, se, r, g, b, true, deltaT );
	NDebugOverlay::Line( se, ne, r, g, b, true, deltaT );
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::DrawDragSelectionSet( Color &dragSelectionSetColor ) const
{
	const float deltaT = NDEBUG_PERSIST_TILL_NEXT_SERVER;
	int r = dragSelectionSetColor.r();
	int g = dragSelectionSetColor.g();
	int b = dragSelectionSetColor.b();
	int a = dragSelectionSetColor.a();

	Vector nw = GetCorner( NORTH_WEST );
	Vector ne = GetCorner( NORTH_EAST );
	Vector sw = GetCorner( SOUTH_WEST );
	Vector se = GetCorner( SOUTH_EAST );

	NDebugOverlay::Triangle( nw, se, ne, r, g, b, a, true, deltaT );
	NDebugOverlay::Triangle( se, nw, sw, r, g, b, a, true, deltaT );

	r = s_dragSelectionSetBorderColor.r();
	g = s_dragSelectionSetBorderColor.g();
	b = s_dragSelectionSetBorderColor.b();
	NDebugOverlay::Line( nw, ne, r, g, b, true, deltaT );
	NDebugOverlay::Line( nw, sw, r, g, b, true, deltaT );
	NDebugOverlay::Line( sw, se, r, g, b, true, deltaT );
	NDebugOverlay::Line( se, ne, r, g, b, true, deltaT );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw navigation areas and edit them
 */
void CNavArea::DrawHidingSpots( void ) const
{
	const HidingSpotVector *hidingSpots = GetHidingSpots();

	for ( const HidingSpot *spot : *hidingSpots )
	{

		NavEditColor color;

		if (spot->IsIdealSniperSpot())
		{
			color = NavIdealSniperColor;
		}
		else if (spot->IsGoodSniperSpot())
		{
			color = NavGoodSniperColor;
		}
		else if (spot->HasGoodCover())
		{
			color = NavGoodCoverColor;
		}
		else
		{
			color = NavExposedColor;
		}

		NavDrawLine( spot->GetPosition(), spot->GetPosition() + Vector( 0, 0, 50 ), color );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw ourselves and adjacent areas
 */
void CNavArea::DrawConnectedAreas( void ) const
{
	int i;

	CBasePlayer *player = UTIL_GetListenServerHost();
	if (player == nullptr)
		return;

	// draw self
	if (TheNavMesh->IsEditMode( CNavMesh::PLACE_PAINTING ))
	{
		Draw();
	}
	else
	{
		Draw();
		DrawHidingSpots();
	}

	// draw connected ladders
	{
		for ( const NavLadderConnect &ladderConnect : m_ladder[ CNavLadder::LADDER_UP ] )
		{
			CNavLadder *ladder = ladderConnect.ladder;

			ladder->DrawLadder();

			if ( !ladder->IsConnected( this, CNavLadder::LADDER_DOWN ) )
			{
				NavDrawLine( GetCenter(), ladder->m_bottom + Vector( 0, 0, GenerationStepSize ), NavConnectedOneWayColor );
			}
		}
	}
	{
		for ( const NavLadderConnect &ladderConnect : m_ladder[ CNavLadder::LADDER_DOWN ] )
		{
			CNavLadder *ladder = ladderConnect.ladder;

			ladder->DrawLadder();

			if ( !ladder->IsConnected( this, CNavLadder::LADDER_UP ) )
			{
				NavDrawLine( GetCenter(), ladder->m_top, NavConnectedOneWayColor );
			}
		}
	}

	// draw connected areas
	for( i=0; i<NUM_DIRECTIONS; ++i )
	{
		NavDirType dir = (NavDirType)i;

		int count = GetAdjacentCount( dir );

		for( int a=0; a<count; ++a )
		{
			CNavArea *adj = GetAdjacentArea( dir, a );

			adj->Draw();

			if ( !TheNavMesh->IsEditMode( CNavMesh::PLACE_PAINTING ) )
			{
				adj->DrawHidingSpots();

				Vector from, to;
				Vector hookPos;
				float halfWidth;
				float size = 5.0f;
				ComputePortal( adj, dir, &hookPos, &halfWidth );

				switch( dir )
				{
					case NORTH:
						from = hookPos + Vector( 0.0f, size, 0.0f );
						to = hookPos + Vector( 0.0f, -size, 0.0f );
						break;
					case SOUTH:
						from = hookPos + Vector( 0.0f, -size, 0.0f );
						to = hookPos + Vector( 0.0f, size, 0.0f );
						break;
					case EAST:
						from = hookPos + Vector( -size, 0.0f, 0.0f );
						to = hookPos + Vector( +size, 0.0f, 0.0f );
						break;
					case WEST:
						from = hookPos + Vector( size, 0.0f, 0.0f );
						to = hookPos + Vector( -size, 0.0f, 0.0f );
						break;
				}

				from.z = GetZ( from );
				to.z = adj->GetZ( to );

				Vector drawTo;
				adj->GetClosestPointOnArea( to, &drawTo );

				if ( nav_show_contiguous.GetBool() )
				{
					if ( IsContiguous( adj ) )
						NavDrawLine( from, drawTo, NavConnectedContiguous );
					else
						NavDrawLine( from, drawTo, NavConnectedNonContiguous );
				}
				else
				{
					if ( adj->IsConnected( this, OppositeDirection( dir ) ) )
						NavDrawLine( from, drawTo, NavConnectedTwoWaysColor );
					else
						NavDrawLine( from, drawTo, NavConnectedOneWayColor );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add to open list in decreasing value order
 */
void CNavArea::AddToOpenList( void )
{
	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );

	if ( IsOpen() )
	{
		// already on list
		return;
	}

	// mark as being on open list for quick check
	m_openMarker = m_masterMarker;

	// if list is empty, add and return
	if ( m_openList == nullptr )
	{
		m_openList = this;
		m_openListTail = this;
		this->m_prevOpen = nullptr;
		this->m_nextOpen = nullptr;
		return;
	}

	// insert self in ascending cost order
	// Since costs are positive, IEEE754 let's us compare as integers (see http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm)
	CNavArea *area, *last = nullptr;
	int thisCostBits = *reinterpret_cast<const int *>(&m_totalCost);

	contract_assert( m_totalCost >= 0.0f );
	for( area = m_openList; area; area = area->m_nextOpen )
	{
		contract_assert( area->GetTotalCost() >= 0.0f );
		int thoseCostBits = *reinterpret_cast<const int *>(&area->m_totalCost);
		if ( thisCostBits < thoseCostBits )
		{
			break;
		}
		last = area;
	}

	if ( area )
	{
		// insert before this area
		this->m_prevOpen = area->m_prevOpen;

		if ( this->m_prevOpen )
		{
			this->m_prevOpen->m_nextOpen = this;
		}
		else
		{
			m_openList = this;
		}

		this->m_nextOpen = area;
		area->m_prevOpen = this;
	}
	else
	{
		// append to end of list
		last->m_nextOpen = this;
		this->m_prevOpen = last;
	
		this->m_nextOpen = nullptr;

		m_openListTail = this;
	}

	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add to tail of the open list
 */
void CNavArea::AddToOpenListTail( void )
{
	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );

	if ( IsOpen() )
	{
		// already on list
		return;
	}

	// mark as being on open list for quick check
	m_openMarker = m_masterMarker;

	// if list is empty, add and return
	if ( m_openList == nullptr )
	{
		m_openList = this;
		m_openListTail = this;
		this->m_prevOpen = nullptr;
		this->m_nextOpen = nullptr;

		contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );
		return;
	}

	// append to end of list
	m_openListTail->m_nextOpen = this;

	this->m_prevOpen = m_openListTail;
	this->m_nextOpen = nullptr;

	m_openListTail = this;

	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A smaller value has been found, update this area on the open list
 * @todo "bubbling" does unnecessary work, since the order of all other nodes will be unchanged - only this node is altered
 */
void CNavArea::UpdateOnOpenList( void )
{
	// since value can only decrease, bubble this area up from current spot
	while( m_prevOpen && this->GetTotalCost() < m_prevOpen->GetTotalCost() )
	{
		// swap position with predecessor
		CNavArea *other = m_prevOpen;
		CNavArea *before = other->m_prevOpen;
		CNavArea *after  = this->m_nextOpen;

		this->m_nextOpen = other;
		this->m_prevOpen = before;

		other->m_prevOpen = this;
		other->m_nextOpen = after;

		if ( before )
		{
			before->m_nextOpen = this;
		}
		else
		{
			m_openList = this;
		}

		if ( after )
		{
			after->m_prevOpen = other;
		}
		else
		{
			m_openListTail = this;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::RemoveFromOpenList( void )
{
	if ( m_openMarker == 0 )
	{
		// not on the list
		return;
	}

	if ( m_prevOpen )
	{
		m_prevOpen->m_nextOpen = m_nextOpen;
	}
	else
	{
		m_openList = m_nextOpen;
	}
	
	if ( m_nextOpen )
	{
		m_nextOpen->m_prevOpen = m_prevOpen;
	}
	else
	{
		m_openListTail = m_prevOpen;
	}
	
	// zero is an invalid marker
	m_openMarker = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clears the open and closed lists for a new search
 */
void CNavArea::ClearSearchLists( void )
{
	// effectively clears all open list pointers and closed flags
	CNavArea::MakeNewMarker();

	m_openList = nullptr;
	m_openListTail = nullptr;
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::SetCorner( NavCornerType corner, const Vector& newPosition )
{
	switch( corner )
	{
		case NORTH_WEST:
			m_nwCorner = newPosition;
			break;

		case NORTH_EAST:
			m_seCorner.x = newPosition.x;
			m_nwCorner.y = newPosition.y;
			m_neZ = newPosition.z;
			break;

		case SOUTH_WEST:
			m_nwCorner.x = newPosition.x;
			m_seCorner.y = newPosition.y;
			m_swZ = newPosition.z;
			break;

		case SOUTH_EAST:
			m_seCorner = newPosition;
			break;

		default:
		{
			Vector oldPosition = GetCenter();
			Vector delta = newPosition - oldPosition;
			m_nwCorner += delta;
			m_seCorner += delta;
			m_neZ += delta.z;
			m_swZ += delta.z;
		}
	}

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	CalcDebugID();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if an existing hiding spot is too close to given position
 */
bool CNavArea::IsHidingSpotCollision( const Vector &pos ) const
{
	const float collisionRange = 30.0f;

	for ( const HidingSpot *spot : m_hidingSpots )
	{
		if ((spot->GetPosition() - pos).IsLengthLessThan( collisionRange ))
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsHidingSpotInCover( const Vector &spot )
{
	int coverCount = 0;
	trace_t result;

	Vector from = spot;
	from.z += HalfHumanHeight;

	Vector to;

	// if we are crouched underneath something, that counts as good cover
	to = from + Vector( 0, 0, 20.0f );
	UTIL_TraceLine( from, to, MASK_NPCSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );
	if (result.fraction != 1.0f)
		return true;

	const float coverRange = 100.0f;
	const float inc = std::numbers::pi / 8.0f;

	for( float angle = 0.0f; angle < 2.0f * std::numbers::pi; angle += inc )
	{
		to = from + Vector( coverRange * (float)std::cos(angle), coverRange * (float)std::sin(angle), HalfHumanHeight );

		UTIL_TraceLine( from, to, MASK_NPCSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );

		// if traceline hit something, it hit "cover"
		if (result.fraction != 1.0f)
			++coverCount;
	}

	// if more than half of the circle has no cover, the spot is not "in cover"
	const int halfCover = 8;
	if (coverCount < halfCover)
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Finds the hiding spot position in a corner's area.  If the typical inset is off the nav area (small
 * hand-constructed areas), it tries to fit the position inside the area.
 */
static Vector FindPositionInArea( CNavArea *area, NavCornerType corner )
{
	int multX = 1, multY = 1;
	switch ( corner )
	{
	case NORTH_WEST:
		break;
	case NORTH_EAST:
		multX = -1;
		break;
	case SOUTH_WEST:
		multY = -1;
		break;
	case SOUTH_EAST:
		multX = -1;
		multY = -1;
		break;
	}

	const float offset = 12.5f;
	Vector cornerPos = area->GetCorner( corner );

	// Try the basic inset
	Vector pos = cornerPos + Vector(  offset*multX,  offset*multY, 0.0f );
	if ( !area->IsOverlapping( pos ) )
	{
		// Try pulling the Y offset to the area's center
		pos = cornerPos + Vector(  offset*multX,  area->GetSizeY()*0.5f*multY, 0.0f );
		if ( !area->IsOverlapping( pos ) )
		{
			// Try pulling the X offset to the area's center
			pos = cornerPos + Vector(  area->GetSizeX()*0.5f*multX,  offset*multY, 0.0f );
			if ( !area->IsOverlapping( pos ) )
			{
				// Try pulling the X and Y offsets to the area's center
				pos = cornerPos + Vector(  area->GetSizeX()*0.5f*multX,  area->GetSizeY()*0.5f*multY, 0.0f );
				if ( !area->IsOverlapping( pos ) )
				{
					contract_assert( false );

					// Just pull the position to a small offset
					pos = cornerPos + Vector(  1.0f*multX,  1.0f*multY, 0.0f );
					if ( !area->IsOverlapping( pos ) )
					{
						// Nothing is working (degenerate area?), so just put it directly on the corner
						pos = cornerPos;
					}
				}
			}
		}
	}

	return pos;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Analyze local area neighborhood to find "hiding spots" for this area
 */
void CNavArea::ComputeHidingSpots( void )
{
	struct
	{
		float lo, hi;
	}
	extent;

	for ( HidingSpot *spot : m_hidingSpots )
		DestroyHidingSpot( spot );	// deletes and unregisters
	m_hidingSpots.clear();

	// "jump areas" cannot have hiding spots
	if ( GetAttributes() & NAV_MESH_JUMP )
		return;

	// "don't hide areas" cannot have hiding spots
	if ( GetAttributes() & NAV_MESH_DONT_HIDE )
		return;

	int cornerCount[NUM_CORNERS];
	for( int i=0; i<NUM_CORNERS; ++i )
		cornerCount[i] = 0;

	const float cornerSize = 20.0f;

	// for each direction, find extents of adjacent areas along the wall
	for( int d=0; d<NUM_DIRECTIONS; ++d )
	{
		extent.lo = 999999.9f;
		extent.hi = -999999.9f;

		bool isHoriz = (d == NORTH || d == SOUTH) ? true : false;

		for ( const NavConnect &connect : m_connect[d] )
		{

			// if connection is only one-way, it's a "jump down" connection (ie: a discontinuity that may mean cover) 
			// ignore it
			if (connect.area->IsConnected( this, OppositeDirection( static_cast<NavDirType>( d ) ) ) == false)
				continue;

			// ignore jump areas
			if (connect.area->GetAttributes() & NAV_MESH_JUMP)
				continue;

			if (isHoriz)
			{
				if (connect.area->m_nwCorner.x < extent.lo)
					extent.lo = connect.area->m_nwCorner.x;

				if (connect.area->m_seCorner.x > extent.hi)
					extent.hi = connect.area->m_seCorner.x;			
			}
			else
			{
				if (connect.area->m_nwCorner.y < extent.lo)
					extent.lo = connect.area->m_nwCorner.y;

				if (connect.area->m_seCorner.y > extent.hi)
					extent.hi = connect.area->m_seCorner.y;
			}
		}

		switch( d )
		{
			case NORTH:
				if (extent.lo - m_nwCorner.x >= cornerSize)
					++cornerCount[ NORTH_WEST ];

				if (m_seCorner.x - extent.hi >= cornerSize)
					++cornerCount[ NORTH_EAST ];
				break;

			case SOUTH:
				if (extent.lo - m_nwCorner.x >= cornerSize)
					++cornerCount[ SOUTH_WEST ];

				if (m_seCorner.x - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_EAST ];
				break;

			case EAST:
				if (extent.lo - m_nwCorner.y >= cornerSize)
					++cornerCount[ NORTH_EAST ];

				if (m_seCorner.y - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_EAST ];
				break;

			case WEST:
				if (extent.lo - m_nwCorner.y >= cornerSize)
					++cornerCount[ NORTH_WEST ];

				if (m_seCorner.y - extent.hi >= cornerSize)
					++cornerCount[ SOUTH_WEST ];
				break;
		}
	}

	for ( int c=0; c<NUM_CORNERS; ++c )
	{
		// if a corner count is 2, then it really is a corner (walls on both sides)
		if (cornerCount[c] == 2)
		{
			Vector pos = FindPositionInArea( this, (NavCornerType)c );
			if ( !c || !IsHidingSpotCollision( pos ) )
			{
				HidingSpot *spot = TheNavMesh->CreateHidingSpot();
				spot->SetPosition( pos );
				spot->SetFlags( IsHidingSpotInCover( pos ) ? HidingSpot::IN_COVER : HidingSpot::EXPOSED );
				m_hidingSpots.push_back( spot );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine how much walkable area we can see from the spot, and how far away we can see.
 */
void ClassifySniperSpot( HidingSpot *spot )
{
	Vector eye = spot->GetPosition();

	CNavArea *hidingArea = TheNavMesh->GetNavArea( spot->GetPosition() );
	if (hidingArea && (hidingArea->GetAttributes() & NAV_MESH_STAND))
	{
		// we will be standing at this hiding spot
		eye.z += HumanEyeHeight;
	}
	else
	{
		// we are crouching when at this hiding spot
		eye.z += HumanCrouchEyeHeight;
	}

	Vector walkable;
	trace_t result;

	Extent sniperExtent;
	float farthestRangeSq = 0.0f;
	const float minSniperRangeSq = 1000.0f * 1000.0f;
	bool found = false;

	// to make compiler stop warning me
	sniperExtent.lo = Vector( 0.0f, 0.0f, 0.0f );
	sniperExtent.hi = Vector( 0.0f, 0.0f, 0.0f );

	Extent areaExtent;
	for ( CNavArea *area : TheNavAreas )
	{
		area->GetExtent( &areaExtent );

		// scan this area
		for( walkable.y = areaExtent.lo.y + GenerationStepSize/2.0f; walkable.y < areaExtent.hi.y; walkable.y += GenerationStepSize )
		{
			for( walkable.x = areaExtent.lo.x + GenerationStepSize/2.0f; walkable.x < areaExtent.hi.x; walkable.x += GenerationStepSize )
			{
				walkable.z = area->GetZ( walkable ) + HalfHumanHeight;
				
				// check line of sight
				UTIL_TraceLine( eye, walkable, CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_PLAYERCLIP, nullptr, COLLISION_GROUP_NONE, &result );

				if (result.fraction == 1.0f && !result.startsolid)
				{
					// can see this spot

					// keep track of how far we can see
					float rangeSq = (eye - walkable).LengthSqr();
					if (rangeSq > farthestRangeSq)
					{
						farthestRangeSq = rangeSq;

						if (rangeSq >= minSniperRangeSq)
						{
							// this is a sniper spot
							// determine how good of a sniper spot it is by keeping track of the snipable area
							if (found)
							{
								if (walkable.x < sniperExtent.lo.x)
									sniperExtent.lo.x = walkable.x;
								if (walkable.x > sniperExtent.hi.x)
									sniperExtent.hi.x = walkable.x;

								if (walkable.y < sniperExtent.lo.y)
									sniperExtent.lo.y = walkable.y;
								if (walkable.y > sniperExtent.hi.y)
									sniperExtent.hi.y = walkable.y;
							}
							else
							{
								sniperExtent.lo = walkable;
								sniperExtent.hi = walkable;
								found = true;
							}
						}
					}
				}	
			}
		}
	}

	if (found)
	{
		// if we can see a large snipable area, it is an "ideal" spot
		float snipableArea = sniperExtent.Area();

		const float minIdealSniperArea = 200.0f * 200.0f;
		const float longSniperRangeSq = 1500.0f * 1500.0f;

		if (snipableArea >= minIdealSniperArea || farthestRangeSq >= longSniperRangeSq)
			spot->m_flags |= HidingSpot::IDEAL_SNIPER_SPOT;
		else
			spot->m_flags |= HidingSpot::GOOD_SNIPER_SPOT;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Analyze local area neighborhood to find "sniper spots" for this area
 */
void CNavArea::ComputeSniperSpots( void )
{
	if (nav_quicksave.GetBool())
		return;

	for ( HidingSpot *spot : m_hidingSpots )
	{
		ClassifySniperSpot( spot );
	}
}

void CNavArea::AddHidingSpot( HidingSpot * pSpot )
{
	contract_assert( ks::find_index( m_hidingSpots, pSpot ) == -1 );
	m_hidingSpots.push_back( pSpot );
}

void CNavArea::RemoveHidingSpot( HidingSpot * pSpot )
{
	bool bSuccess = ks::erase_first( m_hidingSpots, pSpot );
	contract_assert( bSuccess );
	(void)bSuccess;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given the areas we are moving between, return the spots we will encounter
 */
SpotEncounter *CNavArea::GetSpotEncounter( const CNavArea *from, const CNavArea *to )
{
	if (from && to)
	{
		SpotEncounter *e;

		for ( const auto &encounter : m_spotEncounters )
		{
			e = encounter.get();

			if (e->from.area == from && e->to.area == to)
				return e;
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add spot encounter data when moving from area to area
 */
void CNavArea::AddSpotEncounters( const CNavArea *from, NavDirType fromDir, const CNavArea *to, NavDirType toDir )
{
	auto e = std::make_unique< SpotEncounter >();

	e->from.area = const_cast<CNavArea *>( from );
	e->fromDir = fromDir;

	e->to.area = const_cast<CNavArea *>( to );
	e->toDir = toDir;

	float halfWidth;
	ComputePortal( to, toDir, &e->path.to, &halfWidth );
	ComputePortal( from, fromDir, &e->path.from, &halfWidth );

	const float eyeHeight = HumanEyeHeight;
	e->path.from.z = from->GetZ( e->path.from ) + eyeHeight;
	e->path.to.z = to->GetZ( e->path.to ) + eyeHeight;

	// step along ray and track which spots can be seen
	Vector dir = e->path.to - e->path.from;
	float length = dir.NormalizeInPlace();

	// create unique marker to flag used spots
	HidingSpot::ChangeMasterMarker();

	const float stepSize = 25.0f;		// 50
	const float seeSpotRange = 2000.0f;	// 3000
	trace_t result;

	Vector eye, delta;
	HidingSpot *spot;
	SpotOrder spotOrder;

	// step along path thru this area
	bool done = false;
	for( float along = 0.0f; !done; along += stepSize )
	{
		// make sure we check the endpoint of the path segment
		if (along >= length)
		{
			along = length;
			done = true;
		}

		// move the eyepoint along the path segment
		eye = e->path.from + along * dir;

		// check each hiding spot for visibility
		for ( const auto &hidingSpot : TheHidingSpots )
		{
			spot = hidingSpot.get();

			// only look at spots with cover (others are out in the open and easily seen)
			if (!spot->HasGoodCover())
				continue;

			if (spot->IsMarked())
				continue;

			const Vector &spotPos = spot->GetPosition();

			delta.x = spotPos.x - eye.x;
			delta.y = spotPos.y - eye.y;
			delta.z = (spotPos.z + eyeHeight) - eye.z;

			// check if in range
			if (delta.IsLengthGreaterThan( seeSpotRange ))
				continue;

			// check if we have LOS
			// BOTPORT: ignore glass here
			UTIL_TraceLine( eye, Vector( spotPos.x, spotPos.y, spotPos.z + HalfHumanHeight ), MASK_NPCSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );
			if (result.fraction != 1.0f)
				continue;

			// if spot is in front of us along our path, ignore it
			delta.NormalizeInPlace();
			float dot = DotProduct( dir, delta );
			if (dot < 0.7071f && dot > -0.7071f)
			{
				// we only want to keep spots that BECOME visible as we walk past them
				// therefore, skip ALL visible spots at the start of the path segment
				if (along > 0.0f)
				{
					// add spot to encounter
					spotOrder.spot = spot;
					spotOrder.t = along/length;
					e->spots.push_back( spotOrder );
				}
			}

			// mark spot as encountered
			spot->Mark();
		}
	}

	// add encounter to list
	m_spotEncounters.push_back( std::move( e ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute "spot encounter" data. This is an ordered list of spots to look at 
 * for each possible path thru a nav area.
 */
void CNavArea::ComputeSpotEncounters( void )
{
	m_spotEncounters.clear();

	if (nav_quicksave.GetBool())
		return;

	// for each adjacent area
	for( int fromDir=0; fromDir<NUM_DIRECTIONS; ++fromDir )
	{
		for ( NavConnect &fromConnect : m_connect[ fromDir ] )
		{
			NavConnect *fromCon = &fromConnect;

			// compute encounter data for path to each adjacent area
			for( int toDir=0; toDir<NUM_DIRECTIONS; ++toDir )
			{
				for ( NavConnect &toConnect : m_connect[ toDir ] )
				{
					NavConnect *toCon = &toConnect;

					if (toCon == fromCon)
						continue;

					// just do our direction, as we'll loop around for other direction
					AddSpotEncounters( fromCon->area, (NavDirType)fromDir, toCon->area, (NavDirType)toDir );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decay the danger values
 */
void CNavArea::DecayDanger( void )
{
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		float deltaT = gpGlobals->curtime - m_dangerTimestamp;
		float decayAmount = GetDangerDecayRate() * deltaT;

		m_danger[i] -= decayAmount;
		if (m_danger[i] < 0.0f)
			m_danger[i] = 0.0f;

		// update timestamp
		m_dangerTimestamp = gpGlobals->curtime;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Increase the danger of this area for the given team
 */
void CNavArea::IncreaseDanger( int teamID, float amount )
{
	// before we add the new value, decay what's there
	DecayDanger();

	int teamIdx = teamID % MAX_NAV_TEAMS;

	m_danger[ teamIdx ] += amount;
	m_dangerTimestamp = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the danger of this area (decays over time)
 */
float CNavArea::GetDanger( int teamID )
{
	DecayDanger();

	int teamIdx = teamID % MAX_NAV_TEAMS;
	return m_danger[ teamIdx ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a 0..1 light intensity for the given point
 */

float CNavArea::GetLightIntensity( const Vector &pos ) const
{
	Vector testPos;
	testPos.x = clamp( pos.x, m_nwCorner.x, m_seCorner.x );
	testPos.y = clamp( pos.y, m_nwCorner.y, m_seCorner.y );
	testPos.z = pos.z;

	float dX = (testPos.x - m_nwCorner.x) / (m_seCorner.x - m_nwCorner.x);
	float dY = (testPos.y - m_nwCorner.y) / (m_seCorner.y - m_nwCorner.y);

	float northLight = m_lightIntensity[ NORTH_WEST ] * ( 1 - dX ) + m_lightIntensity[ NORTH_EAST ] * dX;
	float southLight = m_lightIntensity[ SOUTH_WEST ] * ( 1 - dX ) + m_lightIntensity[ SOUTH_EAST ] * dX;
	float light = northLight * ( 1 - dY ) + southLight * dY;

	return light;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a 0..1 light intensity for the given point
 */
float CNavArea::GetLightIntensity( float x, float y ) const
{
	return GetLightIntensity( Vector( x, y, 0 ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a 0..1 light intensity averaged over the whole area
 */
float CNavArea::GetLightIntensity( void ) const
{
	float light = m_lightIntensity[ NORTH_WEST ];
	light += m_lightIntensity[ NORTH_EAST ];
	light += m_lightIntensity[ SOUTH_WEST];
	light += m_lightIntensity[ SOUTH_EAST ];
	return light / 4.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute light intensity at corners and center (requires client via listenserver)
 */
bool CNavArea::ComputeLighting( void )
{
	if ( engine->IsDedicatedServer() )
	{
		for ( int i=0; i<NUM_CORNERS; ++i )
		{
			m_lightIntensity[i] = 1.0f;
		}

		return true;
	}

	// Calculate light at the corners
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		Vector pos = FindPositionInArea( this, (NavCornerType)i );
		pos.z = GetZ( pos ) + HalfHumanHeight - StepHeight;	// players light from their centers, and we light from slightly below that, to allow for low ceilings
		float height;
		if ( TheNavMesh->GetGroundHeight( pos, &height ) )
		{
			pos.z = height + HalfHumanHeight - StepHeight;	// players light from their centers, and we light from slightly below that, to allow for low ceilings
		}

		Vector light( 0, 0, 0 );
		// FIXMEL4DTOMAINMERGE
		//if ( !engine->GetLightForPointListenServerOnly( pos, false, &light ) )
		//{
			//NDebugOverlay::Line( pos, pos + Vector( 0, 0, -100 ), 255, 0, 0, false, 100.0f );
		//	return false;
		//}

		Vector ambientColor;
		// FIXMEL4DTOMAINMERGE
		//if ( !GetTerrainAmbientLightAtPoint( pos, &ambientColor ) )
		{
			//NDebugOverlay::Line( pos, pos + Vector( 0, 0, -100 ), 255, 127, 0, false, 100.0f );
			return false;
		}

		//NDebugOverlay::Line( pos, pos + Vector( 0, 0, -100 ), 0, 255, 127, false, 100.0f );

		float ambientIntensity = ambientColor.x + ambientColor.y + ambientColor.z;
		float lightIntensity = light.x + light.y + light.z;
		lightIntensity = clamp( lightIntensity, 0, 1 );	// sum can go well over 1.0, but it's the lower region we care about.  if it's bright, we don't need to know *how* bright.

		lightIntensity = std::max( lightIntensity, ambientIntensity );

		m_lightIntensity[i] = lightIntensity;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
static void nav_update_lighting( const CCommand &args );
static ConCommand nav_update_lighting_command( side_registry(), "nav_update_lighting", nav_update_lighting, "Recomputes lighting values", FCVAR_CHEAT );
static void nav_update_lighting( const CCommand &args )
{
	int numComputed = 0;
	if ( args.ArgC() == 2 )
	{
		int areaID = std::atoi( args[1] );
		CNavArea *area = TheNavMesh->GetNavAreaByID( areaID );
		if ( area )
		{
			if ( area->ComputeLighting() )
			{
				++numComputed;
			}
		}
	}
	else
	{
		for ( CNavArea *area : TheNavAreas )
		{
			if ( area->ComputeLighting() )
			{
				++numComputed;
			}
		}
	}
	DevMsg( "Computed lighting for %d/%d areas\n", numComputed, (int)std::ssize( TheNavAreas ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Raise/lower a corner
 */
void CNavArea::RaiseCorner( NavCornerType corner, int amount, bool raiseAdjacentCorners )
{
	if ( corner == NUM_CORNERS )
	{
		RaiseCorner( NORTH_WEST, amount, raiseAdjacentCorners );
		RaiseCorner( NORTH_EAST, amount, raiseAdjacentCorners );
		RaiseCorner( SOUTH_WEST, amount, raiseAdjacentCorners );
		RaiseCorner( SOUTH_EAST, amount, raiseAdjacentCorners );
		return;
	}

	// Move the corner
	switch (corner)
	{
	case NORTH_WEST:
		m_nwCorner.z += amount;
		break;
	case NORTH_EAST:
		m_neZ += amount;
		break;
	case SOUTH_WEST:
		m_swZ += amount;
		break;
	case SOUTH_EAST:
		m_seCorner.z += amount;
		break;
	}

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;
	}

	if ( !raiseAdjacentCorners || nav_corner_adjust_adjacent.GetFloat() <= 0.0f )
	{
		return;
	}

	// Find nearby areas that share the corner
	CNavArea::MakeNewMarker();
	Mark();

	const float tolerance = nav_corner_adjust_adjacent.GetFloat();

	Vector cornerPos = GetCorner( corner );
	cornerPos.z -= amount; // use the pre-adjustment corner for adjacency checks

	int gridX = TheNavMesh->WorldToGridX( cornerPos.x );
	int gridY = TheNavMesh->WorldToGridY( cornerPos.y );

	const int shift = 1; // try a 3x3 set of grids in case we're on the edge

	for( int x = gridX - shift; x <= gridX + shift; ++x )
	{
		if (x < 0 || x >= TheNavMesh->m_gridSizeX)
			continue;

		for( int y = gridY - shift; y <= gridY + shift; ++y )
		{
			if (y < 0 || y >= TheNavMesh->m_gridSizeY)
				continue;

			NavAreaVector *areas = &TheNavMesh->m_grid[ x + y*TheNavMesh->m_gridSizeX ];

			// find closest area in this cell
			for ( CNavArea *area : *areas )
			{

				// skip if we've already visited this area
				if (area->IsMarked())
					continue;

				area->Mark();

				Vector areaPos;
				for ( int i=0; i<NUM_CORNERS; ++i )
				{
					areaPos = area->GetCorner( NavCornerType(i) );
					if ( areaPos.DistTo( cornerPos ) < tolerance )
					{
						float heightDiff = (cornerPos.z + amount ) - areaPos.z;
						area->RaiseCorner( NavCornerType(i), heightDiff, false );
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * FindGroundZFromPoint walks from the start position to the end position in GenerationStepSize increments,
 * checking the ground height along the way.
 */
float FindGroundZFromPoint( const Vector& end, const Vector& start )
{
	Vector step( 0, 0, StepHeight );
	if ( fabs( end.x - start.x ) > fabs( end.y - start.y ) )
	{
		step.x = GenerationStepSize;
		if ( end.x < start.x )
		{
			step.x = -step.x;
		}
	}
	else
	{
		step.y = GenerationStepSize;
		if ( end.y < start.y )
		{
			step.y = -step.y;
		}
	}

	// step towards our end point
	Vector point = start;
	float z;
	while ( point.AsVector2D().DistTo( end.AsVector2D() ) > GenerationStepSize )
	{
		point = point + step;
		z = point.z;
		if ( TheNavMesh->GetGroundHeight( point, &z ) )
		{
			point.z = z;
		}
		else
		{
			point.z -= step.z;
		}
	}

	// now do the exact one once we're within GenerationStepSize of it
	z = point.z + step.z;
	point = end;
	point.z = z;
	if ( TheNavMesh->GetGroundHeight( point, &z ) )
	{
		point.z = z;
	}
	else
	{
		point.z -= step.z;
	}

	return point.z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Finds the Z value for a corner given two other corner points.  This walks along the edges of the nav area
 * in GenerationStepSize increments, to increase accuracy.
 */
float FindGroundZ( const Vector& original, const Vector& corner1, const Vector& corner2 )
{
	float first = FindGroundZFromPoint( original, corner1 );
	float second = FindGroundZFromPoint( original, corner2 );

	if ( fabs( first - second ) > StepHeight )
	{
		// approaching the point from the two directions didn't agree.  Take the one closest to the original z.
		if ( fabs( original.z - first ) > fabs( original.z - second ) )
		{
			return second;
		}
		else
		{
			return first;
		}
	}

	return first;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Places a corner (or all corners if corner == NUM_CORNERS) on the ground
 */
void CNavArea::PlaceOnGround( NavCornerType corner, float inset )
{
	trace_t result;
	Vector from, to;

	Vector nw = m_nwCorner + Vector ( inset, inset, 0 );
	Vector se = m_seCorner + Vector ( -inset, -inset, 0 );
	Vector ne, sw;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	if ( corner == NORTH_WEST || corner == NUM_CORNERS )
	{
		float newZ = FindGroundZ( nw, ne, sw );
		RaiseCorner( NORTH_WEST, newZ - nw.z );
	}

	if ( corner == NORTH_EAST || corner == NUM_CORNERS )
	{
		float newZ = FindGroundZ( ne, nw, se );
		RaiseCorner( NORTH_EAST, newZ - ne.z );
	}

	if ( corner == SOUTH_WEST || corner == NUM_CORNERS )
	{
		float newZ = FindGroundZ( sw, nw, se );
		RaiseCorner( SOUTH_WEST, newZ - sw.z );
	}

	if ( corner == SOUTH_EAST || corner == NUM_CORNERS )
	{
		float newZ = FindGroundZ( se, ne, sw );
		RaiseCorner( SOUTH_EAST, newZ - se.z );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Shift the nav area
 */
void CNavArea::Shift( const Vector &shift )
{
	m_nwCorner += shift;
	m_seCorner += shift;
}

//--------------------------------------------------------------------------------------------------------------
static void CommandNavUpdateBlocked( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( TheNavMesh->GetMarkedArea() )
	{
		CNavArea *area = TheNavMesh->GetMarkedArea();
		area->UpdateBlocked( true );
		if ( area->IsBlocked( TEAM_ANY ) )
		{
			DevMsg( "Area #%d %s is blocked\n", area->GetID(), VecToString(area->GetCenter() + Vector( 0, 0, HalfHumanHeight )).c_str() );
		}
	}
	else
	{
		float start = Plat_FloatTime();
		CNavArea *blockedArea = nullptr;
		for ( CNavArea *area : TheNavAreas )
		{
			area->UpdateBlocked( true );
			if ( area->IsBlocked( TEAM_ANY ) )
			{
				DevMsg( "Area #%d %s is blocked\n", area->GetID(), VecToString(area->GetCenter() + Vector( 0, 0, HalfHumanHeight )).c_str() );
				if ( !blockedArea )
				{
					blockedArea = area;
				}
			}
		}

		float end = Plat_FloatTime();
		float time = (end - start) * 1000.0f;
		DevMsg( "nav_update_blocked took %2.2f ms\n", time );

		if ( blockedArea )
		{
			CBasePlayer *player = UTIL_GetListenServerHost();
			if ( player )
			{
				if ( ( player->IsDead() || player->IsObserver() ) && player->GetObserverMode() == OBS_MODE_ROAMING )
				{
					Vector origin = blockedArea->GetCenter() + Vector( 0, 0, 0.75f * HumanHeight );
					UTIL_SetOrigin( player, origin );
				}
			}
		}
	}
}
static ConCommand nav_update_blocked( side_registry(), "nav_update_blocked", CommandNavUpdateBlocked, "Updates the blocked/unblocked status for every nav area.", FCVAR_GAMEDLL );

//--------------------------------------------------------------------------------------------------------
bool CNavArea::IsBlockedBase( int teamID, bool ignoreNavBlockers ) const
{
	if ( ignoreNavBlockers && ( m_attributeFlags & NAV_MESH_NAV_BLOCKER ) )
	{
		return false;
	}

	if ( teamID == TEAM_ANY )
	{
		bool isBlocked = false;
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			isBlocked |= !!m_isBlocked[ i ];
		}

		return isBlocked;
	}

	int teamIdx = teamID % MAX_NAV_TEAMS;
	return !!m_isBlocked[ teamIdx ];
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::MarkAsBlocked( int teamID, CBaseEntity *blocker, bool bGenerateEvent )
{
	if ( blocker )
	{
		if ( blocker->ClassMatches( "func_nav_blocker" )  )
		{
			m_attributeFlags |= NAV_MESH_NAV_BLOCKER;
		}
		else if ( blocker->ClassMatches( "prop_door_rotating" ) )
		{
			m_attributeFlags |= NAV_MESH_BLOCKED_PROPDOOR;
		}
	}
	
	bool wasBlocked = false;
	if ( teamID == TEAM_ANY )
	{
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			wasBlocked |= !!m_isBlocked[ i ];
			m_isBlocked[ i ] = true;
		}
	}
	else
	{
		int teamIdx = teamID % MAX_NAV_TEAMS;
		wasBlocked |= !!m_isBlocked[ teamIdx ];
		m_isBlocked[ teamIdx ] = true;
	}

	if ( !wasBlocked )
	{
		if ( bGenerateEvent )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "nav_blocked" );
			if ( event )
			{
				event->SetInt( "area", m_id );
				event->SetInt( "blocked", 1 );
				gameeventmanager->FireEvent( event );
			}
		}

		if ( nav_debug_blocked.GetBool() )
		{
			if ( blocker )
			{
				ConColorMsg( Color( 0, 255, 128, 255 ), "%s %d blocked area %d\n", blocker->GetDebugName(), blocker->entindex(), GetID() );
			}
			else
			{
				ConColorMsg( Color( 0, 255, 128, 255 ), "non-entity blocked area %d\n", GetID() );
			}
		}
		TheNavMesh->OnAreaBlocked( this );
	}
	else
	{
		if ( nav_debug_blocked.GetBool() )
		{
			if ( blocker )
			{
				ConColorMsg( Color( 0, 255, 128, 255 ), "DUPE: %s %d blocked area %d\n", blocker->GetDebugName(), blocker->entindex(), GetID() );
			}
			else
			{
				ConColorMsg( Color( 0, 255, 128, 255 ), "DUPE: non-entity blocked area %d\n", GetID() );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::MarkAsUnblocked( int teamID, bool bGenerateEvent )
{
	m_attributeFlags &= ~NAV_MESH_NAV_BLOCKER;
	m_attributeFlags &= ~NAV_MESH_BLOCKED_PROPDOOR;

	for ( int i = 0; i < MAX_NAV_TEAMS; ++i )
	{
		m_isBlocked[i] = false;
	}

	bool wasBlocked = false;
	if ( teamID == TEAM_ANY )
	{
		for ( int i = 0; i < MAX_NAV_TEAMS; ++i )
		{
			m_isBlocked[i] = false;
		}
	}
	else
	{
		int teamIdx = teamID % MAX_NAV_TEAMS;
		wasBlocked |= !!m_isBlocked[teamIdx];
		m_isBlocked[teamIdx] = false;
	}

	if ( wasBlocked )
	{
		if ( bGenerateEvent )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "nav_blocked" );
			if ( event )
			{
				event->SetInt( "area", m_id );
				event->SetInt( "blocked", 0 );
				gameeventmanager->FireEvent( event );
			}
		}

		if ( nav_debug_blocked.GetBool() )
		{
			ConColorMsg( Color( 0, 128, 255, 255 ), "area %d is unblocked by a nav blocker\n", GetID() );
		}
		TheNavMesh->OnAreaUnblocked( this );
	}
}

//--------------------------------------------------------------------------------------------------------
// checks if any func_nav_blockers are still blocking the area
void CNavArea::UpdateBlockedFromNavBlockers( void )
{
	CVProfScope _( "CNavArea::UpdateBlockedFromNavBlockers", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	Extent bounds;
	GetExtent( &bounds );

	// Save off old values, reset to not blocked state
	m_attributeFlags &= ~NAV_MESH_NAV_BLOCKER;
	bool oldBlocked[MAX_NAV_TEAMS];
	bool wasBlocked = false;
	for ( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		oldBlocked[i] = !!m_isBlocked[i];
		wasBlocked = wasBlocked || oldBlocked[i];
		m_isBlocked[i] = false;
	}

	bool tempIsBlocked[MAX_NAV_TEAMS];
	bool isBlocked = CFuncNavBlocker::CalculateBlocked( tempIsBlocked, bounds.lo, bounds.hi );
	
	for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		m_isBlocked[i] = tempIsBlocked[i];

	if ( isBlocked )
	{
		m_attributeFlags |= NAV_MESH_NAV_BLOCKER;
	}

	// If we're unblocked, fire a nav_blocked event.
	if ( wasBlocked != isBlocked )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "nav_blocked" );
		if ( event )
		{
			event->SetInt( "area", m_id );
			event->SetInt( "blocked", isBlocked );
			gameeventmanager->FireEvent( event );
		}

		if ( isBlocked )
		{
			if ( nav_debug_blocked.GetBool() )
			{
				ConColorMsg( Color( 0, 255, 128, 255 ), "area %d is blocked by a nav blocker\n", GetID() );
			}
			TheNavMesh->OnAreaBlocked( this );
		}
		else
		{
			if ( nav_debug_blocked.GetBool() )
			{
				ConColorMsg( Color( 0, 128, 255, 255 ), "area %d is unblocked by a nav blocker\n", GetID() );
			}
			TheNavMesh->OnAreaUnblocked( this );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::UnblockArea( void )
{
	bool wasBlocked = IsBlocked( TEAM_ANY );

	for ( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_isBlocked[ i ] = false;
	}

	if ( wasBlocked )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "nav_blocked" );
		if ( event )
		{
			event->SetInt( "area", m_id );
			event->SetInt( "blocked", false );
			gameeventmanager->FireEvent( event );
		}

		if ( nav_debug_blocked.GetBool() )
		{
			ConColorMsg( Color( 255, 0, 128, 255 ), "area %d is unblocked by UnblockArea\n", GetID() );
		}
		TheNavMesh->OnAreaUnblocked( this );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Updates the (un)blocked status of the nav area
 * The semantics of this method have gotten very muddled - needs refactoring (MSB 5/7/09)
 */
void CNavArea::UpdateBlockedBase( bool force, int teamID )
{
	CVProfScope _( "CNavArea::UpdateBlocked", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( !force && !m_blockedTimer.IsElapsed() )
	{
		return;
	}

	// update blocked is called on all nav areas for many frames with the force param set true, 
	// causing this timer to grow until this is forced to run after a delay at the start of the round
// 	const float MaxBlockedCheckInterval = 1;
// 	float interval = m_blockedTimer.GetCountdownDuration() + 1;
// 	if ( interval > MaxBlockedCheckInterval )
// 	{
// 		interval = MaxBlockedCheckInterval;
// 	}
	m_blockedTimer.Start( 1 );

	if ( ( m_attributeFlags & NAV_MESH_NAV_BLOCKER ) )
	{
		if ( force )
		{
			UpdateBlockedFromNavBlockers();
		}
		return;
	}
	if ( ( m_attributeFlags & NAV_MESH_BLOCKED_PROPDOOR ) )
	{
		return;
	}

	Vector origin = GetCenter();
	origin.z += HalfHumanHeight;

	const float sizeX = std::max<std::common_type_t<int, float>>( 1, std::min<std::common_type_t<float, int>>( GetSizeX()/2 - 5, HalfHumanWidth ) );
	const float sizeY = std::max<std::common_type_t<int, float>>( 1, std::min<std::common_type_t<float, int>>( GetSizeY()/2 - 5, HalfHumanWidth ) );
	Extent bounds;
	bounds.lo.Init( -sizeX, -sizeY, 0 );
	bounds.hi.Init( sizeX, sizeY, VEC_DUCK_HULL_MAX.z - HalfHumanHeight );

	bool wasBlocked = IsBlocked( TEAM_ANY );

	// See if spot is valid
	CTraceFilterWalkableEntities filter( nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );
	trace_t tr;
	{
	CVProfScope _( "CNavArea::UpdateBlocked-Trace", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	UTIL_TraceHull(
		origin,
		origin,
		bounds.lo,
		bounds.hi,
		MASK_NPCSOLID_BRUSHONLY,
		&filter,
		&tr );

	}
	if ( force )
	{
		if ( teamID == TEAM_ANY )
		{
			for ( int i = 0; i < MAX_NAV_TEAMS; ++i )
			{
				m_isBlocked[i] = true;
			}
		}
		else
		{
			int teamIdx = teamID % MAX_NAV_TEAMS;
			m_isBlocked[teamIdx] = true;
		}
	}
	else if ( !tr.startsolid )
	{
		// unblock ourself
		{
			for ( int i = 0; i < MAX_NAV_TEAMS; ++i )
			{
				m_isBlocked[i] = false;
			}
		}
	}

	bool isBlocked = IsBlocked( TEAM_ANY );

	if ( wasBlocked != isBlocked )
	{
		CVProfScope _( "CNavArea::UpdateBlocked-Event", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		IGameEvent * event = gameeventmanager->CreateEvent( "nav_blocked" );
		if ( event )
		{
			event->SetInt( "area", m_id );
			event->SetInt( "blocked", isBlocked );
			gameeventmanager->FireEvent( event );
		}

		if ( isBlocked )
		{
			TheNavMesh->OnAreaBlocked( this );
		}
		else
		{
			TheNavMesh->OnAreaUnblocked( this );
		}
	}

	if ( TheNavMesh->GetMarkedArea() == this )
	{
		if ( IsBlocked( teamID ) )
		{
			NDebugOverlay::Box( origin, bounds.lo, bounds.hi, 255, 0, 0, 64, 3.0f );
		}
		else
		{
			NDebugOverlay::Box( origin, bounds.lo, bounds.hi, 0, 255, 0, 64, 3.0f );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Checks if there is a floor under the nav area, in case a breakable floor is gone
 */
void CNavArea::CheckFloor( CBaseEntity *ignore )
{
	if ( IsBlocked( TEAM_ANY ) )
		return;

	Vector origin = GetCenter();
	origin.z -= JumpCrouchHeight;

	const float size = GenerationStepSize * 0.5f;
	Vector mins = Vector( -size, -size, 0 );
	Vector maxs = Vector( size, size, JumpCrouchHeight + 10.0f );

	// See if spot is valid
	trace_t tr;
	UTIL_TraceHull(
		origin,
		origin,
		mins,
		maxs,
		MASK_NPCSOLID_BRUSHONLY,
		ignore,
		COLLISION_GROUP_PLAYER_MOVEMENT,
		&tr );

	// If the center is open space, we're effectively blocked
	if ( !tr.startsolid )
	{
		MarkAsBlocked( TEAM_ANY, nullptr );
	}

	/*
	if ( IsBlocked( TEAM_ANY ) )
	{
		NDebugOverlay::Box( origin, mins, maxs, 255, 0, 0, 64, 3.0f );
	}
	else
	{
		NDebugOverlay::Box( origin, mins, maxs, 0, 255, 0, 64, 3.0f );
	}
	*/
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::MarkObstacleToAvoid( float obstructionHeight )
{
	if ( m_avoidanceObstacleHeight < obstructionHeight )
	{
		if ( m_avoidanceObstacleHeight == 0 )
		{
			TheNavMesh->OnAvoidanceObstacleEnteredArea( this );
		}

		m_avoidanceObstacleHeight = obstructionHeight;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Updates the (un)obstructed status of the nav area
 */
void CNavArea::UpdateAvoidanceObstacles( void )
{
	if ( !m_avoidanceObstacleTimer.IsElapsed() )
	{
		return;
	}

	const float MaxBlockedCheckInterval = 5;
	float interval = m_blockedTimer.GetCountdownDuration() + 1;
	if ( interval > MaxBlockedCheckInterval )
	{
		interval = MaxBlockedCheckInterval;
	}
	m_avoidanceObstacleTimer.Start( interval );

	Vector mins = m_nwCorner;
	Vector maxs = m_seCorner;

	mins.z = std::min( m_nwCorner.z, m_seCorner.z );
	maxs.z = std::max( m_nwCorner.z, m_seCorner.z ) + HumanCrouchHeight;

	float obstructionHeight = 0.0f;
	for ( int i=0; i<std::ssize( TheNavMesh->GetObstructions() ); ++i )
	{
		INavAvoidanceObstacle *obstruction = TheNavMesh->GetObstructions()[i];
		CBaseEntity *obstructingEntity = obstruction->GetObstructingEntity();
		if ( !obstructingEntity )
			continue;

		// check if the aabb intersects the search aabb.
		Vector vecSurroundMins, vecSurroundMaxs;
		obstructingEntity->CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		if ( !IsBoxIntersectingBox( mins, maxs, vecSurroundMins, vecSurroundMaxs ) )
			continue;

		if ( !obstruction->CanObstructNavAreas() )
			continue;

		float propHeight = obstruction->GetNavObstructionHeight();

		obstructionHeight = std::max( obstructionHeight, propHeight );
	}

	m_avoidanceObstacleHeight = obstructionHeight;

	if ( m_avoidanceObstacleHeight == 0.0f )
	{
		TheNavMesh->OnAvoidanceObstacleLeftArea( this );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::CheckWaterLevel( void )
{
	Vector pos( GetCenter() );
	if ( !TheNavMesh->GetGroundHeight( pos, &pos.z ) )
	{
		m_isUnderwater = false;
		return;
	}

	pos.z += 1;
	m_isUnderwater = (enginetrace->GetPointContents( pos ) & MASK_WATER ) != 0;
}

//--------------------------------------------------------------------------------------------------------------
static void CommandNavCheckFloor( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( TheNavMesh->GetMarkedArea() )
	{
		CNavArea *area = TheNavMesh->GetMarkedArea();
		area->CheckFloor( nullptr );
		if ( area->IsBlocked( TEAM_ANY ) )
		{
			DevMsg( "Area #%d %s is blocked\n", area->GetID(), VecToString(area->GetCenter() + Vector( 0, 0, HalfHumanHeight )).c_str() );
		}
	}
	else
	{
		float start = Plat_FloatTime();
		for ( CNavArea *area : TheNavAreas )
		{
			area->CheckFloor( nullptr );
			if ( area->IsBlocked( TEAM_ANY ) )
			{
				DevMsg( "Area #%d %s is blocked\n", area->GetID(), VecToString(area->GetCenter() + Vector( 0, 0, HalfHumanHeight )).c_str() );
			}
		}

		float end = Plat_FloatTime();
		float time = (end - start) * 1000.0f;
		DevMsg( "nav_check_floor took %2.2f ms\n", time );
	}
}
static ConCommand nav_check_floor( side_registry(), "nav_check_floor", CommandNavCheckFloor, "Updates the blocked/unblocked status for every nav area.", FCVAR_GAMEDLL );

//--------------------------------------------------------------------------------------------------------------
bool SelectOverlappingAreas::operator()( CNavArea *area )
{
	CNavArea *overlappingArea = nullptr;
	CNavLadder *overlappingLadder = nullptr;

	Vector nw = area->GetCorner( NORTH_WEST );
	Vector se = area->GetCorner( SOUTH_EAST );
	Vector start = nw;
	start.x += GenerationStepSize/2;
	start.y += GenerationStepSize/2;

	while ( start.x < se.x )
	{
		start.y = nw.y + GenerationStepSize/2;
		while ( start.y < se.y )
		{
			start.z = area->GetZ( start.x, start.y );
			Vector end = start;
			start.z -= StepHeight;
			end.z += HalfHumanHeight;

			if ( TheNavMesh->FindNavAreaOrLadderAlongRay( start, end, &overlappingArea, &overlappingLadder, area ) )
			{
				if ( overlappingArea )
				{
					TheNavMesh->AddToSelectedSet( overlappingArea );
					TheNavMesh->AddToSelectedSet( area );
				}
			}

			start.y += GenerationStepSize;
		}
		start.x += GenerationStepSize;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------
static void CommandNavSelectOverlapping( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	TheNavMesh->ClearSelectedSet();

	SelectOverlappingAreas overlapCheck;
	TheNavMesh->ForAllAreas( overlapCheck );

	Msg( "%d overlapping areas selected\n", TheNavMesh->GetSelecteSetSize() );
}
static ConCommand nav_select_overlapping( side_registry(), "nav_select_overlapping", CommandNavSelectOverlapping, "Selects nav areas that are overlapping others.", FCVAR_GAMEDLL );

//--------------------------------------------------------------------------------------------------------
static byte m_PVS[PAD_NUMBER( MAX_MAP_CLUSTERS,8 ) / 8];
static int m_nPVSSize;		// PVS size in bytes

NavVisPairSet g_NavVisPairHash;

inline constexpr auto MASK_NAV_VISION = (MASK_BLOCKLOS_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE);

//--------------------------------------------------------------------------------------------------------
/**
 * Set PVS to only include the Potentially Visible Set as seen from anywhere
 * within this nav area
 */
void CNavArea::SetupPVS( void ) const
{
	m_nPVSSize = sizeof( m_PVS );
	engine->ResetPVS( m_PVS, m_nPVSSize );

	const float margin = GenerationStepSize/2.0f;
	Vector eye( 0, 0, 0.75f * HumanHeight );

	// step across area checking visibility to given area
	Vector shift( eye );
	for( shift.y = margin; shift.y <= GetSizeY() - margin; shift.y += GenerationStepSize )
	{
		for( shift.x = margin; shift.x <= GetSizeX() - margin; shift.x += GenerationStepSize )
		{
			// Optimization:
			// If we are already POTENTIALLY_VISIBLE, and no longer COMPLETELY_VISIBLE, there's
			// no way for vis to change again.
			Vector testPos( GetCorner( NORTH_WEST ) + shift );
			testPos.z = GetZ( testPos ) + eye.z;

			engine->AddOriginToPVS( testPos );
		}
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * Return true if this area is within the current PVS
 */
bool CNavArea::IsInPVS( void ) const
{
	Vector eye( 0, 0, 0.75f * HumanHeight );

	Extent areaExtent;
	
	areaExtent.lo = GetCenter() + eye;
	areaExtent.hi = areaExtent.lo;

	areaExtent.Encompass( GetCorner( NORTH_WEST ) + eye );
	areaExtent.Encompass( GetCorner( NORTH_EAST ) + eye );
	areaExtent.Encompass( GetCorner( SOUTH_WEST ) + eye );
	areaExtent.Encompass( GetCorner( SOUTH_EAST ) + eye );

	return engine->CheckBoxInPVS( areaExtent.lo, areaExtent.hi, m_PVS, m_nPVSSize );
}

//--------------------------------------------------------------------------------------------------------
/**
 * Do actual line-of-sight traces to determine if any part of given area is visible from this area
 */
CNavArea::VisibilityType CNavArea::ComputeVisibility( const CNavArea *area, bool isPVSValid, bool bCheckPVS, bool *pOutsidePVS ) const
{
	float distanceSq = area->GetCenter().DistToSqr( GetCenter() );

	if ( nav_max_view_distance.GetFloat() > 0.00001f )
	{
		// limit range of visibility check
		if ( distanceSq > Sqr( nav_max_view_distance.GetFloat() ) )
		{
			// too far to be visible
			return NOT_VISIBLE;
		}
	}

	if ( !isPVSValid )
	{
		SetupPVS();
	}

	Vector eye( 0, 0, 0.75f * HumanHeight );

	if ( bCheckPVS )
	{
		Extent areaExtent;
		areaExtent.lo = areaExtent.hi = area->GetCenter() + eye;
		areaExtent.Encompass( area->GetCorner( NORTH_WEST ) + eye );
		areaExtent.Encompass( area->GetCorner( NORTH_EAST ) + eye );
		areaExtent.Encompass( area->GetCorner( SOUTH_WEST ) + eye );
		areaExtent.Encompass( area->GetCorner( SOUTH_EAST ) + eye );
		if ( !engine->CheckBoxInPVS( areaExtent.lo, areaExtent.hi, m_PVS, m_nPVSSize ) )
		{
			if ( pOutsidePVS )
				*pOutsidePVS = true;
			return NOT_VISIBLE;
		}

		if ( pOutsidePVS )
			*pOutsidePVS = false;
	}

	//------------------------------------
	Vector vThisNW = GetCorner( NORTH_WEST ) + eye;
	Vector vThisNE = GetCorner( NORTH_EAST ) + eye;
	Vector vThisSW = GetCorner( SOUTH_WEST ) + eye;
	Vector vThisSE = GetCorner( SOUTH_EAST ) + eye;
	Vector vThisCenter = GetCenter() + eye;

	Vector vTraceMins( vThisNW );
	Vector vTraceMaxs( vThisSE );

	vTraceMins.z = std::min( std::min( std::min( vThisNW.z, vThisNE.z ), vThisSE.z ), vThisSW.z );
	vTraceMaxs.z = std::max( std::max( std::max( vThisNW.z, vThisNE.z ), vThisSE.z ), vThisSW.z ) + 0.1;

	vTraceMins -= vThisCenter;
	vTraceMaxs -= vThisCenter;

	Vector vOtherMins( area->GetCorner( NORTH_WEST) );
	Vector vOtherMaxs( area->GetCorner( SOUTH_EAST) );

	Vector vTarget;
	CalcClosestPointOnAABB( vOtherMins, vOtherMaxs, vThisCenter, vTarget );
	vTarget.z = area->GetZ( vTarget ) + eye.z;

	trace_t tr;
	CTraceFilterNoNPCsOrPlayer traceFilter( nullptr, COLLISION_GROUP_NONE );

	UTIL_TraceHull( vThisCenter, vTarget, vTraceMins, vTraceMaxs, MASK_NAV_VISION, &traceFilter, &tr );

	if ( tr.fraction == 1.0 ||  ( tr.endpos.x > vOtherMins.x && tr.endpos.x < vOtherMaxs.x && tr.endpos.y > vOtherMins.y && tr.endpos.y < vOtherMaxs.y ) )
	{
		return COMPLETELY_VISIBLE; // Counter-intuitive: the way this function was written, "COMPLETELY_VISIBLE" actually means "I am completely visible to the other"
	}

	//------------------------------------
	// check line of sight between areas
	unsigned char vis = COMPLETELY_VISIBLE;

	const float margin = GenerationStepSize/2.0f;

	Vector shift( 0, 0, 0.75f * HumanHeight );

	// always check center to catch very small areas
	if ( area->IsPartiallyVisible( GetCenter() + eye ) )
	{
		vis |= POTENTIALLY_VISIBLE;
	}
	else
	{
		vis &= ~COMPLETELY_VISIBLE;
	}

	Vector eyeToCenter( GetCenter() - area->GetCenter() );
	eyeToCenter.NormalizeInPlace();
	float angleTolerance = nav_potentially_visible_dot_tolerance.GetFloat();	// if corner-to-eye angles are this close to center-to-eye angles, assume the same result and skip the trace

	// step across area checking visibility to given area
	for( shift.y = margin; shift.y <= GetSizeY() - margin; shift.y += GenerationStepSize )
	{
		for( shift.x = margin; shift.x <= GetSizeX() - margin; shift.x += GenerationStepSize )
		{
			// Optimization:
			// If we are already POTENTIALLY_VISIBLE, and no longer COMPLETELY_VISIBLE, there's
			// no way for vis to change again.
			if ( vis == POTENTIALLY_VISIBLE )
				return POTENTIALLY_VISIBLE;

			Vector testPos( GetCorner( NORTH_WEST ) + shift );
			testPos.z = GetZ( testPos ) + eye.z;

			// Optimization - treat long-distance traces that are effectively collinear as the same
			if ( distanceSq > Sqr( 1000 ) )
			{
				Vector eyeToCorner( testPos - (GetCenter() + eye) );
				eyeToCorner.NormalizeInPlace();
				if ( eyeToCorner.Dot( eyeToCenter ) >= angleTolerance )
				{
					continue;
				}
			}

			if ( area->IsPartiallyVisible( testPos ) )
			{
				vis |= POTENTIALLY_VISIBLE;
			}
			else
			{
				vis &= ~COMPLETELY_VISIBLE;
			}
		}
	}

	return (VisibilityType)vis;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Return a list of the delta between our visibility list and the given adjacent area
 */
const CNavArea::CAreaBindInfoArray &CNavArea::ComputeVisibilityDelta( const CNavArea *other ) const
{
	static CAreaBindInfoArray delta;

	delta.clear();
	
	// do not delta from a delta - if 'other' is already inheriting, use its inherited source directly
	if ( other->m_inheritVisibilityFrom.area != nullptr )
	{
		contract_assert( false && "Visibility inheriting from inherited area" );

		delta = m_potentiallyVisibleAreas;
		return delta;
	}

	// add any visible areas in my list that are not in 'others' list into the delta
	int i, j;
	for( i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
	{
		if ( m_potentiallyVisibleAreas[i].area )
		{
			// is my visible area also in adjacent area's vis list
			for( j=0; j<std::ssize( other->m_potentiallyVisibleAreas ); ++j )
			{
				if ( m_potentiallyVisibleAreas[i].area == other->m_potentiallyVisibleAreas[j].area &&
					 m_potentiallyVisibleAreas[i].attributes == other->m_potentiallyVisibleAreas[j].attributes )
				{
					// mutually identically visible
					break;
				}
			}

			if ( j == std::ssize( other->m_potentiallyVisibleAreas ) )
			{
				// my vis area not in adjacent area's vis list or has different visibility attributes - add to delta
				delta.push_back( m_potentiallyVisibleAreas[i] );
			}
		}
	}

	// add explicit NOT_VISIBLE references to areas in 'others' list that are NOT in mine
	for( j=0; j<std::ssize( other->m_potentiallyVisibleAreas ); ++j )
	{
		if ( other->m_potentiallyVisibleAreas[j].area )
		{
			for( i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
			{
				if ( m_potentiallyVisibleAreas[i].area == other->m_potentiallyVisibleAreas[j].area )
				{
					// area in both lists - already handled in delta above
					break;
				}
			}

			if ( i == std::ssize( m_potentiallyVisibleAreas ) )
			{
				// 'other' has area in their list that we don't - mark it explicitly NOT_VISIBLE
				AreaBindInfo info;
				info.area = other->m_potentiallyVisibleAreas[j].area;
				info.attributes = NOT_VISIBLE;

				delta.push_back( info );
			}
		}
	}

	return delta;
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::ResetPotentiallyVisibleAreas()
{
	m_potentiallyVisibleAreas.clear();
}

//--------------------------------------------------------------------------------------------------------
/**
 * Determine visibility between areas.
 * Compute full list of all areas visible for each area.  This list will be compressed into deltas
 * in the PostCustomAnalysis() step.
 */

CNavArea *g_pCurVisArea;
CTSListWithFreeList< CNavArea::AreaBindInfo > g_ComputedVis;

void CNavArea::ComputeVisToArea( CNavArea *&pOtherArea )
{
	CNavArea *area = static_cast< CNavArea * >( pOtherArea );
	VisibilityType visThisToOther = ( area == g_pCurVisArea ) ? COMPLETELY_VISIBLE : NOT_VISIBLE;
	VisibilityType visOtherToThis = NOT_VISIBLE;

	if ( area != g_pCurVisArea )
	{
		bool bOutsidePVS;

		visOtherToThis = g_pCurVisArea->ComputeVisibility( area, true, true, &bOutsidePVS ); // TODO: Hacky right now. Compute visibility for the "complete" case actually returns how completely visible the area is to the other. Should fix it to be more clear [1/30/2009 tom]

		if ( !bOutsidePVS && ( visOtherToThis || ( g_pCurVisArea->GetCenter() - area->GetCenter() ).LengthSqr() < Sqr( nav_max_view_distance.GetFloat() ) ) )
		{
			visThisToOther = area->ComputeVisibility( g_pCurVisArea, true, false );
		}

		if ( !visOtherToThis && visThisToOther )
		{
			visOtherToThis = POTENTIALLY_VISIBLE;
		}

		if ( !visThisToOther && visOtherToThis )
		{
			visThisToOther = POTENTIALLY_VISIBLE;
		}
	}

	CNavArea::AreaBindInfo info;
	if ( visThisToOther != NOT_VISIBLE )
	{
		info.area = area;
		info.attributes = visThisToOther;
		g_ComputedVis.PushItem( info );
	}

	if ( visOtherToThis != NOT_VISIBLE )
	{
		info.area = g_pCurVisArea;
		info.attributes = visOtherToThis;
		area->m_potentiallyVisibleAreas.push_back( info );
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * Determine visibility from this area to all potentially/completely visible areas in the mesh
 */

void CNavArea::ComputeVisibilityToMesh( void )
{
	m_inheritVisibilityFrom.area = nullptr;
	m_isInheritedFrom = false;

	// collect all possible nav areas that could be visible from this area
	NavAreaCollector collector;
	float radius = nav_max_view_distance.GetFloat();
	if ( radius == 0.0f )
	{
		radius = DEF_NAV_VIEW_DISTANCE;
	}
	collector.m_area.reserve( 1000 );
	TheNavMesh->ForAllAreasInRadius( collector, GetCenter(), radius );

	NavVisPair_t visPair;

	// First eliminate the ones already calculated
	for ( int i = std::ssize( collector.m_area ) - 1; i >= 0; --i )
	{
		visPair.SetPair( this, collector.m_area[i] );

		if ( g_NavVisPairHash.contains( visPair ) )
		{
			ks::swap_remove( collector.m_area, i );
		}
	}

	SetupPVS();

	g_pCurVisArea = this;
	ParallelProcess( collector.m_area.data(), std::ssize( collector.m_area ), &ComputeVisToArea );

	m_potentiallyVisibleAreas.reserve( g_ComputedVis.Count() );
	while ( g_ComputedVis.Count() )
	{
		g_ComputedVis.PopItem( &m_potentiallyVisibleAreas.emplace_back() );
	}

	for ( auto collectedArea : collector.m_area )
	{
		visPair.SetPair( this, (CNavArea *)collectedArea );
		[[maybe_unused]] bool bInserted = g_NavVisPairHash.insert( visPair ).second;
		contract_assert( bInserted );
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * The center and all four corners must ALL be visible
 */
bool CNavArea::IsEntirelyVisible( const Vector &eye, CBaseEntity *ignore ) const
{
	Vector corner;
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( ignore, COLLISION_GROUP_NONE );
	const float offset = 0.75f * HumanHeight;

	// check center
	UTIL_TraceLine( eye, GetCenter() + Vector( 0, 0, offset ), MASK_NAV_VISION, &traceFilter, &result );
	if (result.fraction < 1.0f)
	{
		return false;
	}

	for( int c=0; c<NUM_CORNERS; ++c )
	{
		corner = GetCorner( (NavCornerType)c );
		UTIL_TraceLine( eye, corner + Vector( 0, 0, offset ), MASK_NAV_VISION, &traceFilter, &result );
		if (result.fraction < 1.0f)
		{
			return false;
		}
	}

	// all points are visible
	return true;
}

//--------------------------------------------------------------------------------------------------------
/**
 * The center or any of the four corners may be visible
 */
bool CNavArea::IsPartiallyVisible( const Vector &eye, CBaseEntity *ignore ) const
{
	Vector corner;
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( ignore, COLLISION_GROUP_NONE );
	const float offset = 0.75f * HumanHeight;

	// check center
	UTIL_TraceLine( eye, GetCenter() + Vector( 0, 0, offset ), MASK_NAV_VISION, &traceFilter, &result );
	if (result.fraction >= 1.0f)
	{
		return true;
	}

	Vector eyeToCenter( GetCenter() + Vector( 0, 0, offset ) - eye );
	eyeToCenter.NormalizeInPlace();
	float angleTolerance = nav_potentially_visible_dot_tolerance.GetFloat();	// if corner-to-eye angles are this close to center-to-eye angles, assume the same result and skip the trace

	for( int c=0; c<NUM_CORNERS; ++c )
	{
		corner = GetCorner( (NavCornerType)c ) + Vector( 0, 0, offset );

		// Optimization - treat traces that are effectively collinear as the same
		Vector eyeToCorner( corner - eye );
		eyeToCorner.NormalizeInPlace();
		if ( eyeToCorner.Dot( eyeToCenter ) >= angleTolerance )
		{
			continue;
		}

		UTIL_TraceLine( eye, corner + Vector( 0, 0, offset ), MASK_NAV_VISION, &traceFilter, &result );
		if (result.fraction >= 1.0f)
		{
			return true;
		}
	}

	// nothing is visible
	return false;
}

//--------------------------------------------------------------------------------------------------------
bool CNavArea::IsPotentiallyVisible( const CNavArea *viewedArea ) const
{
	CVProfScope _( "CNavArea::IsPotentiallyVisible", 0, "NextBot", false, BUDGETFLAG_OTHER );

	if ( viewedArea == nullptr )
	{
		return false;
	}

	// can always see ourselves
	if ( viewedArea == this )
	{
		return true;
	}

	// normal visibility check
	for ( int i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
	{
		if ( m_potentiallyVisibleAreas[i].area == viewedArea )
		{
			// Found area in our list. We might be a delta from another list, 
			// and NOT_VISIBLE overrides that list.
			return ( m_potentiallyVisibleAreas[i].attributes != NOT_VISIBLE );
		}
	}

	// viewedArea is not in our visibility list, check inherited set
	if ( m_inheritVisibilityFrom.area )
	{
		CAreaBindInfoArray &inherited = m_inheritVisibilityFrom.area->m_potentiallyVisibleAreas;

		for ( int i=0; i<std::ssize( inherited ); ++i )
		{
			if ( inherited[i].area == viewedArea )
			{
				return ( inherited[i].attributes != NOT_VISIBLE );
			}
		}
	}
	
	return false;
}

//--------------------------------------------------------------------------------------------------------
bool CNavArea::IsCompletelyVisible( const CNavArea *viewedArea ) const
{
	CVProfScope _( "CNavArea::IsCompletelyVisible", 0, "NextBot", false, BUDGETFLAG_OTHER );

	if ( viewedArea == nullptr )
	{
		return false;
	}

	// can always see ourselves
	if ( viewedArea == this )
	{
		return true;
	}

	// normal visibility check
	for ( int i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
	{
		if ( m_potentiallyVisibleAreas[i].area == viewedArea )
		{
			// our list is definitive - viewedArea is in our list, but is not completely visible
			return ( m_potentiallyVisibleAreas[i].attributes & COMPLETELY_VISIBLE ) ? true : false;
		}
	}

	// viewedArea is not in our visibility list, check inherited set
	if ( m_inheritVisibilityFrom.area )
	{
		CAreaBindInfoArray &inherited = m_inheritVisibilityFrom.area->m_potentiallyVisibleAreas;

		for ( int i=0; i<std::ssize( inherited ); ++i )
		{
			if ( inherited[i].area == viewedArea )
			{
				return ( inherited[i].attributes & COMPLETELY_VISIBLE ) ? true : false;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Return true if any portion of this area is visible to anyone on the given team
 */
bool CNavArea::IsPotentiallyVisibleToTeam( int teamIndex ) const
{
	CVProfScope _( "CNavArea::IsPotentiallyVisibleToTeam", 0, "NextBot", false, BUDGETFLAG_OTHER );

	CTeam *team = GetGlobalTeam( teamIndex );

	for( int i = 0; i < team->GetNumPlayers(); ++i )
	{
		if ( team->GetPlayer(i)->IsAlive() )
		{
			CNavArea *from = (CNavArea *)team->GetPlayer(i)->GetLastKnownArea();
			
			if ( from && from->IsPotentiallyVisible( this ) )
			{
				return true;
			}
		}
	}
	
	return false;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Return true if given area is completely visible from somewhere in this area by someone on the team (very fast)
 */
bool CNavArea::IsCompletelyVisibleToTeam( int teamIndex ) const
{
	CVProfScope _( "CNavArea::IsCompletelyVisibleToTeam", 0, "NextBot", false, BUDGETFLAG_OTHER );

	CTeam *team = GetGlobalTeam( teamIndex );

	for( int i = 0; i < team->GetNumPlayers(); ++i )
	{
		if ( team->GetPlayer(i)->IsAlive() )
		{
			CNavArea *from = (CNavArea *)team->GetPlayer(i)->GetLastKnownArea();

			if ( from && from->IsCompletelyVisible( this ) )
			{
				return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
Vector CNavArea::GetRandomPoint( void ) const
{
	Extent extent;
	GetExtent( &extent );

	Vector spot;
	spot.x = RandomFloat( extent.lo.x, extent.hi.x ); 
	spot.y = RandomFloat( extent.lo.y, extent.hi.y );
	spot.z = GetZ( spot.x, spot.y );

	return spot;
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
void CNavArea::ClearPlayerCount( void )
{
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_playerCount[ i ] = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsDegenerate( void ) const
{
	return (m_nwCorner.x >= m_seCorner.x || m_nwCorner.y >= m_seCorner.y);
}

//--------------------------------------------------------------------------------------------------------------
CNavArea *CNavArea::GetAdjacentArea( NavDirType dir, int i ) const
{
	for( int iter = 0; iter < std::ssize( m_connect[dir] ); ++iter )
	{
		if (i == 0)
			return m_connect[dir][iter].area;
		--i;
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsOpen( void ) const
{
	return (m_openMarker == m_masterMarker) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsOpenListEmpty( void )
{
	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );
	return (m_openList) ? false : true;
}

//--------------------------------------------------------------------------------------------------------------
CNavArea *CNavArea::PopOpenList( void )
{
	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );

	if ( m_openList )
	{
		CNavArea *area = m_openList;
	
		// disconnect from list
		area->RemoveFromOpenList();
		area->m_prevOpen = nullptr;
		area->m_nextOpen = nullptr;

		contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );

		return area;
	}

	contract_assert( (m_openList && m_openList->m_prevOpen == nullptr) || m_openList == nullptr );

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsClosed( void ) const
{
	if (IsMarked() && !IsOpen())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::AddToClosedList( void )
{
	Mark();
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::RemoveFromClosedList( void )
{
	// since "closed" is defined as visited (marked) and not on open list, do nothing
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::SetClearedTimestamp( int teamID )
{
	m_clearedTimestamp[ teamID % MAX_NAV_TEAMS ] = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
float CNavArea::GetClearedTimestamp( int teamID ) const
{ 
	return m_clearedTimestamp[ teamID % MAX_NAV_TEAMS ];
}

//--------------------------------------------------------------------------------------------------------------
float CNavArea::GetEarliestOccupyTimeBase( int teamID ) const
{
	return m_earliestOccupyTime[ teamID % MAX_NAV_TEAMS ];
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsDamaging( void ) const
{
	return ( gpGlobals->tickcount <= m_damagingTickCount );
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::MarkAsDamaging( float duration )
{
	m_damagingTickCount = gpGlobals->tickcount + TIME_TO_TICKS( duration );
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::HasAvoidanceObstacle( float maxObstructionHeight ) const
{
	return m_avoidanceObstacleHeight > maxObstructionHeight;
}

//--------------------------------------------------------------------------------------------------------------
float CNavArea::GetAvoidanceObstacleHeight( void ) const
{
	return m_avoidanceObstacleHeight;
}

//--------------------------------------------------------------------------------------------------------------
bool CNavArea::IsVisible( const Vector &eye, Vector *visSpot ) const
{
	Vector corner;
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( nullptr, COLLISION_GROUP_NONE );
	const float offset = 0.75f * HumanHeight;

	// check center first
	UTIL_TraceLine( eye, GetCenter() + Vector( 0, 0, offset ), MASK_BLOCKLOS_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE, &traceFilter, &result );
	if (result.fraction == 1.0f)
	{
		// we can see this area
		if (visSpot)
		{
			*visSpot = GetCenter();
		}
		return true;
	}

	for( int c=0; c<NUM_CORNERS; ++c )
	{
		corner = GetCorner( (NavCornerType)c );
		UTIL_TraceLine( eye, corner + Vector( 0, 0, offset ), MASK_BLOCKLOS_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE, &traceFilter, &result );
		if (result.fraction == 1.0f)
		{
			// we can see this area
			if (visSpot)
			{
				*visSpot = corner;
			}
			return true;
		}
	}

	return false;
}

void CNavArea::IncrementPlayerCount( int teamID, int entIndex )
{
	teamID = teamID % MAX_NAV_TEAMS;

	if (m_playerCount[ teamID ] == 255)
	{
		DevMsg( "CNavArea::IncrementPlayerCount: Overflow\n" );
		return;
	}

	++m_playerCount[ teamID ];
}

void CNavArea::DecrementPlayerCount( int teamID, int entIndex )
{
	teamID = teamID % MAX_NAV_TEAMS;

	if (m_playerCount[ teamID ] == 0)
	{
		DevWarning( "CNavArea::IncrementPlayerCount: Underflow\n" );
		return;
	}

	--m_playerCount[ teamID ];
}

unsigned char CNavArea::GetPlayerCount( int teamID ) const
{
	if (teamID)
	{
		return m_playerCount[ teamID % MAX_NAV_TEAMS ];
	}

	// sum all players
	unsigned char total = 0;
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		total += m_playerCount[i];
	}

	return total;
}

Vector CNavArea::GetCorner( NavCornerType corner ) const
{
	// @TODO: Confirm compiler does the "right thing" in release builds, or change this function to to take a pointer [2/4/2009 tom]
	Vector pos;

	switch( corner )
	{
	default:
		contract_assert( false && "GetCorner: Invalid type" );
	case NORTH_WEST:
		return m_nwCorner;

	case NORTH_EAST:
		pos.x = m_seCorner.x;
		pos.y = m_nwCorner.y;
		pos.z = m_neZ;
		return pos;

	case SOUTH_WEST:
		pos.x = m_nwCorner.x;
		pos.y = m_seCorner.y;
		pos.z = m_swZ;
		return pos;

	case SOUTH_EAST:
		return m_seCorner;
	}
}

NavConnect::NavConnect()
{
	id = 0;
	length = -1;
}

class CBaseEntity;
class CNavArea;
class CNavLadder;
class CNavMesh;
struct Extent;
class HidingSpot;

using namespace ::ks::reflect;

// nav_file.cpp
// Reading and writing nav files
// Author: Michael S. Booth (mike@turtlerockstudios.com), January-September 2003

//--------------------------------------------------------------------------------------------------------------
/// The current version of the nav file format

/// IMPORTANT: If this version changes, the swap function in makegamedata 
/// must be updated to match. If not, this will break the Xbox 360.
// TODO: Was changed from 15, update when latest 360 code is integrated (MSB 5/5/09)
static const int NavCurrentVersion = 16;

//--------------------------------------------------------------------------------------------------------------
//
// The 'place directory' is used to save and load places from
// nav files in a size-efficient manner that also allows for the 
// order of the place ID's to change without invalidating the
// nav files.
//
// The place directory is stored in the nav file as a list of 
// place name strings.  Each nav area then contains an index
// into that directory, or zero if no place has been assigned to 
// that area.
//
PlaceDirectory::PlaceDirectory( void )
{
	Reset();
}

void PlaceDirectory::Reset( void )
{
	m_directory.clear();
	m_hasUnnamedAreas = false;
}

/// return true if this place is already in the directory
bool PlaceDirectory::IsKnown( Place place ) const
{
	return ks::contains( m_directory, place );
}

/// return the directory index corresponding to this Place (0 = no entry)
PlaceDirectory::IndexType PlaceDirectory::GetIndex( Place place ) const
{
	if (place == UNDEFINED_PLACE)
		return 0;

	int i = ks::find_index( m_directory, place );

	if (i < 0)
	{
		contract_assert( false && "PlaceDirectory::GetIndex failure" );
		return 0;
	}

	return (IndexType)(i+1);
}

/// add the place to the directory if not already known
void PlaceDirectory::AddPlace( Place place )
{
	if (place == UNDEFINED_PLACE)
	{
		m_hasUnnamedAreas = true;
		return;
	}

	contract_assert( place < 1000 );

	if (IsKnown( place ))
		return;

	m_directory.push_back( place );
}

/// given an index, return the Place
Place PlaceDirectory::IndexToPlace( IndexType entry ) const
{
	if (entry == 0)
		return UNDEFINED_PLACE;

	int i = entry-1;

	if (i >= std::ssize( m_directory ))
	{
		contract_assert( false && "PlaceDirectory::IndexToPlace: Invalid entry" );
		return UNDEFINED_PLACE;
	}

	return m_directory[ i ];
}

/// store the directory
void PlaceDirectory::Save( ks::byte_writer &fileBuffer )
{
	// store number of entries in directory
	IndexType count = (IndexType)std::ssize( m_directory );
	fileBuffer.write< unsigned short >( count );

	// store entries		
	for( int i=0; i<std::ssize( m_directory ); ++i )
	{
		const char *placeName = TheNavMesh->PlaceToName( m_directory[i] );

		// store string length followed by string itself
		unsigned short len = (unsigned short)(std::strlen( placeName ) + 1);
		fileBuffer.write< unsigned short >( len );
		fileBuffer.write_bytes( placeName, len );
	}

	fileBuffer.write< unsigned char >( m_hasUnnamedAreas );
}

/// load the directory
void PlaceDirectory::Load( ks::byte_reader &fileBuffer, int version )
{
	// read number of entries
	IndexType count = fileBuffer.read< unsigned short >();

	m_directory.clear();

	// read each entry
	char placeName[256];
	unsigned short len;
	for( int i=0; i<count; ++i )
	{
		len = fileBuffer.read< unsigned short >();
		fileBuffer.read_bytes( placeName, std::min<std::common_type_t<long unsigned int, short unsigned int>>( sizeof( placeName ), len ) );

		Place place = TheNavMesh->NameToPlace( placeName );
		if (place == UNDEFINED_PLACE)
		{
			Warning( "Warning: NavMesh place %s is undefined?\n", placeName );
		}
		AddPlace( place );
	}

	if ( version > 11 )
	{
		m_hasUnnamedAreas = fileBuffer.read< unsigned char >() != 0;
	}
}

PlaceDirectory placeDirectory;

inline constexpr const char *FORMAT_BSPFILE = "maps\\%s"".bsp";
#define FORMAT_NAVFILE "maps\\%s"".nav"

//--------------------------------------------------------------------------------------------------------------
/**
 * Replace extension with "bsp"
 */
char *GetBspFilename( const char *navFilename )
{
	static char bspFilename[256];

	V_snprintf( bspFilename, sizeof( bspFilename ), FORMAT_BSPFILE, STRING( gpGlobals->mapname ) );

	int len = std::strlen( bspFilename );
	if (len < 3)
		return nullptr;

	bspFilename[ len-3 ] = 'b';
	bspFilename[ len-2 ] = 's';
	bspFilename[ len-1 ] = 'p';

	return bspFilename;
}

unsigned char CNavArea::GetSavedHidingSpotCount( void ) const
{
	unsigned char count = 0;
	for ( const HidingSpot *pSpot : m_hidingSpots )
	{
		if ( count == 0xff )
			break;

		if ( pSpot->IsSaved() )
			count++;
	}
	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Save a navigation area to the opened binary stream
 */
void CNavArea::SaveBase( ks::byte_writer &fileBuffer, unsigned int version ) const
{
	// save ID
	fileBuffer.write< unsigned int >( m_id );

	// save attribute flags
	fileBuffer.write< int >( m_attributeFlags );

	// save extent of area
	fileBuffer.write_bytes( &m_nwCorner, 3*sizeof(float) );
	fileBuffer.write_bytes( &m_seCorner, 3*sizeof(float) );

	// save heights of implicit corners
	fileBuffer.write< float >( m_neZ );
	fileBuffer.write< float >( m_swZ );

	// save connections to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// save number of connections for this direction
		unsigned int count = std::ssize( m_connect[d] );
		fileBuffer.write< unsigned int >( count );

		for ( NavConnect connect : m_connect[d] )
		{
			fileBuffer.write< unsigned int >( connect.area->m_id );
		}
	}

	//
	// Store hiding spots for this area
	//
	unsigned char count = GetSavedHidingSpotCount();
	if ( count > 255 )
	{
		count = 255;
		Warning( "Warning: NavArea #%d: Truncated hiding spot list to 255\n", m_id );
	}
	fileBuffer.write< unsigned char >( count );

	// store HidingSpot objects
	unsigned int saveCount = 0;
	for ( HidingSpot *spot : m_hidingSpots )
	{
		spot->Save( fileBuffer, version );

		// overflow check
		if (++saveCount == count)
			break;
	}

	//
	// Save encounter spots for this area
	//
	{
		// save number of encounter paths for this area
		unsigned int count = std::ssize( m_spotEncounters );
		fileBuffer.write< unsigned int >( count );

		SpotEncounter *e;
		for ( const auto &pEncounter : m_spotEncounters )
		{
			e = pEncounter.get();

			if (e->from.area)
				fileBuffer.write< unsigned int >( e->from.area->m_id );
			else
				fileBuffer.write< unsigned int >( 0 );

			unsigned char dir = (unsigned char)e->fromDir;
			fileBuffer.write< unsigned char >( dir );

			if (e->to.area)
				fileBuffer.write< unsigned int >( e->to.area->m_id );
			else
				fileBuffer.write< unsigned int >( 0 );

			dir = (unsigned char)e->toDir;
			fileBuffer.write< unsigned char >( dir );

			// write list of spots along this path
			unsigned char spotCount;
			if (std::ssize( e->spots ) > 255)
			{
				spotCount = 255;
				Warning( "Warning: NavArea #%d: Truncated encounter spot list to 255\n", m_id );
			}
			else
			{
				spotCount = (unsigned char)std::ssize( e->spots );
			}
			fileBuffer.write< unsigned char >( spotCount );
		
			saveCount = 0;
			for ( SpotOrder &spotOrder : e->spots )
			{
				SpotOrder *order = &spotOrder;

				// order->spot may be NULL if we've loaded a nav mesh that has been edited but not re-analyzed
				unsigned int id = (order->spot) ? order->spot->GetID() : 0;
				fileBuffer.write< unsigned int >( id );

				unsigned char t = (unsigned char)(255 * order->t);
				fileBuffer.write< unsigned char >( t );

				// overflow check
				if (++saveCount == spotCount)
					break;
			}
		}
	}

	// store place dictionary entry
	PlaceDirectory::IndexType entry = placeDirectory.GetIndex( GetPlace() );
	fileBuffer.write_bytes( &entry, sizeof(entry) );

	// write out ladder info
	int i;
	for ( i=0; i<CNavLadder::NUM_LADDER_DIRECTIONS; ++i )
	{
		// save number of encounter paths for this area
		unsigned int count = std::ssize( m_ladder[i] );
		fileBuffer.write< unsigned int >( count );

		NavLadderConnect ladder;
		for ( const NavLadderConnect &ladderConnect : m_ladder[i] )
		{
			ladder = ladderConnect;

			unsigned int id = ladder.ladder->GetID();
			fileBuffer.write< unsigned int >( id );
		}
	}

	// save earliest occupy times
	for( i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		fileBuffer.write_bytes( &m_earliestOccupyTime[i], sizeof(m_earliestOccupyTime[i]) );
	}

	// save light intensity
	for ( i=0; i<NUM_CORNERS; ++i )
	{
		fileBuffer.write< float >( m_lightIntensity[i] );
	}

	// save visible area set
	unsigned int visibleAreaCount = std::ssize( m_potentiallyVisibleAreas );
	fileBuffer.write< unsigned int >( visibleAreaCount );

	for ( int vit=0; vit<std::ssize( m_potentiallyVisibleAreas ); ++vit )
	{
		CNavArea *area = m_potentiallyVisibleAreas[ vit ].area;

		unsigned int id = area ? area->GetID() : 0;

		fileBuffer.write< unsigned int >( id );
		fileBuffer.write< unsigned char >( m_potentiallyVisibleAreas[ vit ].attributes );
	}

	// store area we inherit visibility from
	unsigned int id = ( m_inheritVisibilityFrom.area ) ? m_inheritVisibilityFrom.area->GetID() : 0;
	fileBuffer.write< unsigned int >( id );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load a navigation area from the file
 */
NavErrorType CNavArea::LoadBase( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion )
{
	// load ID
	m_id = fileBuffer.read< unsigned int >();

	// update nextID to avoid collisions
	if (m_id >= m_nextID)
		m_nextID = m_id+1;

	// load attribute flags
	if ( version <= 8 )
	{
		m_attributeFlags = fileBuffer.read< unsigned char >();
	}
	else if ( version < 13 )
	{
		m_attributeFlags = fileBuffer.read< unsigned short >();
	}
	else
	{
		m_attributeFlags = fileBuffer.read< int >();
	}

	// load extent of area
	fileBuffer.read_bytes( &m_nwCorner, 3*sizeof(float) );
	fileBuffer.read_bytes( &m_seCorner, 3*sizeof(float) );

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;

		DevWarning( "Degenerate Navigation Area #%d at setpos %g %g %g\n", 
			m_id, m_nwCorner.x, m_nwCorner.y, m_nwCorner.z );
	}

	// load heights of implicit corners
	m_neZ = fileBuffer.read< float >();
	m_swZ = fileBuffer.read< float >();

	CheckWaterLevel();

	// load connections (IDs) to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// load number of connections for this direction
		unsigned int count = fileBuffer.read< unsigned int >();
		contract_assert( fileBuffer.ok() );

		m_connect[d].reserve( count );
		for( unsigned int i=0; i<count; ++i )
		{
			NavConnect connect;
			connect.id = fileBuffer.read< unsigned int >();
			contract_assert( fileBuffer.ok() );

			// don't allow self-referential connections
			if ( connect.id != m_id )
			{
				m_connect[d].push_back( connect );
			}
		}
	}

	//
	// Load hiding spots
	//

	// load number of hiding spots
	unsigned char hidingSpotCount = fileBuffer.read< unsigned char >();

	if (version == 1)
	{
		// load simple vector array
		Vector pos;
		for( int h=0; h<hidingSpotCount; ++h )
		{
			fileBuffer.read_bytes( &pos, 3 * sizeof(float) );

			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();
			spot->SetPosition( pos );
			spot->SetFlags( HidingSpot::IN_COVER );
			m_hidingSpots.push_back( spot );
		}
	}
	else
	{
		// load HidingSpot objects for this area
		for( int h=0; h<hidingSpotCount; ++h )
		{
			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();

			spot->Load( fileBuffer, version );
			
			m_hidingSpots.push_back( spot );
		}
	}

	if ( version < 15 )
	{
		//
		// Eat the approach areas
		//
		int nToEat = fileBuffer.read< unsigned char >();

		// load approach area info (IDs)
		for( int a=0; a<nToEat; ++a )
		{
			fileBuffer.read< unsigned int >();
			fileBuffer.read< unsigned int >();
			fileBuffer.read< unsigned char >();
			fileBuffer.read< unsigned int >();
			fileBuffer.read< unsigned char >();
		}
	}

	//
	// Load encounter paths for this area
	//
	unsigned int count = fileBuffer.read< unsigned int >();

	if (version < 3)
	{
		// old data, read and discard
		for( unsigned int e=0; e<count; ++e )
		{
			SpotEncounter encounter;

			encounter.from.id = fileBuffer.read< unsigned int >();
			encounter.to.id = fileBuffer.read< unsigned int >();

			fileBuffer.read_bytes( &encounter.path.from.x, 3 * sizeof(float) );
			fileBuffer.read_bytes( &encounter.path.to.x, 3 * sizeof(float) );

			// read list of spots along this path
			unsigned char spotCount = fileBuffer.read< unsigned char >();
		
			for( int s=0; s<spotCount; ++s )
			{
				fileBuffer.read< float >();
				fileBuffer.read< float >();
				fileBuffer.read< float >();
				fileBuffer.read< float >();
			}
		}
		return NAV_OK;
	}

	for( unsigned int e=0; e<count; ++e )
	{
		auto encounter = std::make_unique< SpotEncounter >();

		encounter->from.id = fileBuffer.read< unsigned int >();

		unsigned char dir = fileBuffer.read< unsigned char >();
		encounter->fromDir = static_cast<NavDirType>( dir );

		encounter->to.id = fileBuffer.read< unsigned int >();

		dir = fileBuffer.read< unsigned char >();
		encounter->toDir = static_cast<NavDirType>( dir );

		// read list of spots along this path
		unsigned char spotCount = fileBuffer.read< unsigned char >();
	
		SpotOrder order;
		for( int s=0; s<spotCount; ++s )
		{
			order.id = fileBuffer.read< unsigned int >();

			unsigned char t = fileBuffer.read< unsigned char >();

			order.t = (float)t/255.0f;

			encounter->spots.push_back( order );
		}

		m_spotEncounters.push_back( std::move( encounter ) );
	}

	if (version < 5)
		return NAV_OK;

	//
	// Load Place data
	//
	PlaceDirectory::IndexType entry = fileBuffer.read< unsigned short >();

	// convert entry to actual Place
	SetPlace( placeDirectory.IndexToPlace( entry ) );

	if ( version < 7 )
		return NAV_OK;

	// load ladder data
	for ( int dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		count = fileBuffer.read< unsigned int >();
		for( unsigned int i=0; i<count; ++i )
		{
			NavLadderConnect connect;
			connect.id = fileBuffer.read< unsigned int >();

			bool alreadyConnected = false;
			for ( const NavLadderConnect &existing : m_ladder[dir] )
			{
				if ( existing.id == connect.id )
				{
					alreadyConnected = true;
					break;
				}
			}

			if ( !alreadyConnected )
			{
				m_ladder[dir].push_back( connect );
			}
		}
	}

	if ( version < 8 )
		return NAV_OK;

	// load earliest occupy times
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		m_earliestOccupyTime[i] = fileBuffer.read< float >();
	}

	if ( version < 11 )
		return NAV_OK;

	// load light intensity
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		m_lightIntensity[i] = fileBuffer.read< float >();
	}

	if ( version < 16 )
		return NAV_OK;

	// load visibility information
	unsigned int visibleAreaCount = fileBuffer.read< unsigned int >();

	m_potentiallyVisibleAreas.reserve( visibleAreaCount );

	for( unsigned int j=0; j<visibleAreaCount; ++j )
	{
		AreaBindInfo info;
		info.id = fileBuffer.read< unsigned int >();
		info.attributes = fileBuffer.read< unsigned char >();

		m_potentiallyVisibleAreas.push_back( info );
	}

	// read area from which we inherit visibility
	m_inheritVisibilityFrom.id = fileBuffer.read< unsigned int >();

	return NAV_OK;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Convert loaded IDs to pointers
 * Make sure all IDs are converted, even if corrupt data is encountered.
 */
NavErrorType CNavArea::PostLoadBase( void )
{
	NavErrorType error = NAV_OK;

	for ( int dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		for ( NavLadderConnect &connect : m_ladder[dir] )
		{
			unsigned int id = connect.id;

			if ( ks::find_index( TheNavMesh->GetLadders(), connect.ladder ) == -1 )
			{
				connect.ladder = TheNavMesh->GetLadderByID( id );
			}

			if (id && connect.ladder == nullptr)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation ladder data. Cannot connect Navigation Areas.\n" );
				error = NAV_CORRUPT_DATA;
			}
		}
	}

	// connect areas together
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		for ( NavConnect &navConnect : m_connect[d] )
		{
			NavConnect *connect = &navConnect;

			// convert connect ID into an actual area
			unsigned int id = connect->id;
			connect->area = TheNavMesh->GetNavAreaByID( id );
			if (id && connect->area == nullptr)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation data. Cannot connect Navigation Areas.\n" );
				error = NAV_CORRUPT_DATA;
			}
			connect->length = ( connect->area->GetCenter() - GetCenter() ).Length();
		}
	}

	// resolve spot encounter IDs
	SpotEncounter *e;
	for ( const auto &pEncounter : m_spotEncounters )
	{
		e = pEncounter.get();

		e->from.area = TheNavMesh->GetNavAreaByID( e->from.id );
		if (e->from.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing \"from\" Navigation Area for Encounter Spot.\n" );
			error = NAV_CORRUPT_DATA;
		}

		e->to.area = TheNavMesh->GetNavAreaByID( e->to.id );
		if (e->to.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing \"to\" Navigation Area for Encounter Spot.\n" );
			error = NAV_CORRUPT_DATA;
		}

		if (e->from.area && e->to.area)
		{
			// compute path
			float halfWidth;
			ComputePortal( e->to.area, e->toDir, &e->path.to, &halfWidth );
			ComputePortal( e->from.area, e->fromDir, &e->path.from, &halfWidth );

			const float eyeHeight = HalfHumanHeight;
			e->path.from.z = e->from.area->GetZ( e->path.from ) + eyeHeight;
			e->path.to.z = e->to.area->GetZ( e->path.to ) + eyeHeight;
		}

		// resolve HidingSpot IDs
		for ( SpotOrder &spotOrder : e->spots )
		{
			SpotOrder *order = &spotOrder;

			order->spot = GetHidingSpotByID( order->id );
			if (order->spot == nullptr)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Hiding Spot\n" );
				error = NAV_CORRUPT_DATA;
			}
		}
	}

	// convert visible ID's to pointers to actual areas
	for ( int it=0; it<std::ssize( m_potentiallyVisibleAreas ); ++it )
	{
		AreaBindInfo &info = m_potentiallyVisibleAreas[ it ];

		info.area = TheNavMesh->GetNavAreaByID( info.id );
		if ( info.area == nullptr )
		{
			Warning( "Invalid area in visible set for area #%d\n", GetID() );
		}		
	}

	m_inheritVisibilityFrom.area = TheNavMesh->GetNavAreaByID( m_inheritVisibilityFrom.id );
	contract_assert( m_inheritVisibilityFrom.area != this );

	// remove any invalid areas from the list
	AreaBindInfo bad;
	bad.area = nullptr;
	std::erase( m_potentiallyVisibleAreas, bad );

	return error;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Determine the earliest time this hiding spot can be reached by either team
 */
void CNavArea::ComputeEarliestOccupyTimes( void )
{
	/// @todo Derive cstrike-specific navigation classes

	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		m_earliestOccupyTime[i] = 120.0f;
	}

	if (nav_quicksave.GetBool())
		return;

	// maximum player speed in units/second
	const float playerSpeed = 240.0f;

	ShortestPathCost cost;
	CBaseEntity *spot;

	// determine the shortest time it will take a Terrorist to reach this area
	int team = TEAM_TERRORIST % MAX_NAV_TEAMS;
	for( spot = gEntList.FindEntityByClassname( nullptr, "info_player_terrorist" );
		 spot;
		 spot = gEntList.FindEntityByClassname( spot, "info_player_terrorist" ) )
	{
		float travelDistance = NavAreaTravelDistance( spot->GetAbsOrigin(), GetCenter(), cost );
		if (travelDistance < 0.0f)
			continue;

		float travelTime = travelDistance / playerSpeed;
		if (travelTime < m_earliestOccupyTime[ team ])
		{
			m_earliestOccupyTime[ team ] = travelTime;
		}
	}

	// determine the shortest time it will take a CT to reach this area
	team = TEAM_CT % MAX_NAV_TEAMS;
	for( spot = gEntList.FindEntityByClassname( nullptr, "info_player_counterterrorist" );
		 spot;
		 spot = gEntList.FindEntityByClassname( spot, "info_player_counterterrorist" ) )
	{
		float travelDistance = NavAreaTravelDistance( spot->GetAbsOrigin(), GetCenter(), cost );
		if (travelDistance < 0.0f)
			continue;

		float travelTime = travelDistance / playerSpeed;
		if (travelTime < m_earliestOccupyTime[ team ])
		{
			m_earliestOccupyTime[ team ] = travelTime;
		}
	}

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine if this area is a "battlefront" area - where two rushing teams first meet.
 */
void CNavMesh::ComputeBattlefrontAreas( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the filename for this map's "nav map" file
 */
const char *CNavMesh::GetFilename( void ) const
{
	// filename is local to game dir, so we need to prepend game dir for regular file save
	char gamePath[256];
	engine->GetGameDir( gamePath, 256 );

	// persistant return value
	static char filename[256];
	V_snprintf( filename, sizeof( filename ), "%s\\" FORMAT_NAVFILE, gamePath, STRING( gpGlobals->mapname ) );

	return filename;
}

//--------------------------------------------------------------------------------------------------------------
/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
inline void COM_FixSlashes( char *pname )
{
	while ( *pname ) 
	{
		if ( *pname == '\\' )
			*pname = '/';
		pname++;
	}
}

static void WarnIfMeshNeedsAnalysis( int version )
{
	// Quick check to warn about needing to analyze: nav_strip, nav_delete, etc set
	// every CNavArea's m_approachCount to 0, and delete their m_spotEncounterList.
	// So, if no area has either, odds are good we need an analyze.

	if ( version >= 14 )
	{
		if ( !TheNavMesh->IsAnalyzed() )
		{
			Warning( "The nav mesh needs a full nav_analyze\n" );
			return;
		}
	}
	else
	{
		bool hasApproachAreas = false;
		bool hasSpotEncounters = false;

		for ( CNavArea *pNavArea : TheNavAreas )
		{
			CNavArea *area = pNavArea;
			if ( area )
			{
				if ( area->GetApproachInfoCount() )
				{
					hasApproachAreas = true;
				}

				if ( area->GetSpotEncounterCount() )
				{
					hasSpotEncounters = true;
				}
			}
		}

		if ( !hasApproachAreas || !hasSpotEncounters )
		{
			Warning( "The nav mesh needs a full nav_analyze\n" );
		}
	}
}

/**
 * Store Navigation Mesh to a file
 */
bool CNavMesh::Save( void ) const
{
	WarnIfMeshNeedsAnalysis( NavCurrentVersion );

	const char *filename = GetFilename();
	if (filename == nullptr)
		return false;

	//
	// Store the NAV file
	//
	COM_FixSlashes( const_cast<char *>(filename) );

	// get size of source bsp file for later (before we open the nav file for writing, in
	// case of failure)
	char *bspFilename = GetBspFilename( filename );
	if (bspFilename == nullptr)
	{
		return false;
	}

	ks::byte_writer fileBuffer( 4096 );

	// store "magic number" to help identify this kind of file
	unsigned int magic = NAV_MAGIC_NUMBER;
	fileBuffer.write< unsigned int >( magic );

	// store version number of file
	// 1 = hiding spots as plain vector array
	// 2 = hiding spots as HidingSpot objects
	// 3 = Encounter spots use HidingSpot ID's instead of storing vector again
	// 4 = Includes size of source bsp file to verify nav data correlation
	// ---- Beta Release at V4 -----
	// 5 = Added Place info
	// ---- Conversion to Src ------
	// 6 = Added Ladder info
	// 7 = Areas store ladder ID's so ladders can have one-way connections
	// 8 = Added earliest occupy times (2 floats) to each area
	// 9 = Promoted CNavArea's attribute flags to a short
	// 10 - Added sub-version number to allow game-specific area data
	// 11 - Added light intensity to each area
	// 12 - Storing presence of unnamed areas in the PlaceDirectory
	// 13 - Widened NavArea attribute bits from unsigned short to int
	// 14 - Added a bool for if the nav needs analysis
	// 15 - removed approach areas
	// 16 - Added visibility data to the base mesh
	fileBuffer.write< unsigned int >( NavCurrentVersion );

	// The sub-version number tracks the format of the game-specific (Counter-Strike) mesh and area data,
	// just as the version above tracks the base format
	fileBuffer.write< unsigned int >( GetSubVersionNumber() );
	
	// store the size of source bsp file in the nav file
	// so we can test if the bsp changed since the nav file was made
	unsigned int bspSize = filesystem->Size( bspFilename );
	DevMsg( "Size of bsp file '%s' is %u bytes.\n", bspFilename, bspSize );

	fileBuffer.write< unsigned int >( bspSize );

	// Store the analysis state
	fileBuffer.write< unsigned char >( m_isAnalyzed );

	//
	// Build a directory of the Places in this map
	//
	placeDirectory.Reset();

	for ( CNavArea *area : TheNavAreas )
	{
		Place place = area->GetPlace();
		placeDirectory.AddPlace( place );
	}

	placeDirectory.Save( fileBuffer );

	SaveCustomDataPreArea( fileBuffer );

	//
	// Store navigation areas
	//
	{
		// store number of areas
		unsigned int count = std::ssize( TheNavAreas );
		fileBuffer.write< unsigned int >( count );

		// store each area
		for ( CNavArea *area : TheNavAreas )
		{
			area->Save( fileBuffer, NavCurrentVersion );
		}
	}

	//
	// Store ladders
	//
	{
		// store number of ladders
		unsigned int count = std::ssize( m_ladders );
		fileBuffer.write< unsigned int >( count );

		// store each ladder
		for ( int i=0; i<std::ssize( m_ladders ); ++i )
		{
			CNavLadder *ladder = m_ladders[i];
			ladder->Save( fileBuffer, NavCurrentVersion );
		}
	}
	
	//
	// Store game-specific mesh info
	//
	SaveCustomData( fileBuffer );

	if ( !filesystem->WriteFile( filename, "MOD", fileBuffer.bytes() ) )
	{
		Warning( "Unable to save %d bytes to %s\n", fileBuffer.size(), filename );
		return false;
	}

	unsigned int navSize = filesystem->Size( filename );
	DevMsg( "Size of nav file '%s' is %u bytes.\n", filename, navSize );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
static NavErrorType CheckNavFile( const char *bspFilename )
{
	if ( !bspFilename )
		return NAV_CANT_ACCESS_FILE;

	char baseName[256];
	V_StripExtension(bspFilename,baseName,sizeof(baseName));
	char bspPathname[256];
	V_snprintf(bspPathname,sizeof(bspPathname), FORMAT_BSPFILE, baseName);
	char filename[256];
	V_snprintf(filename,sizeof(filename), FORMAT_NAVFILE, baseName);

	bool navIsInBsp = false;
	FileHandle_t file = filesystem->Open( filename, "rb", "MOD" );	// this ignores .nav files embedded in the .bsp ...
	if ( !file )
	{
		navIsInBsp = true;
		file = filesystem->Open( filename, "rb", "GAME" );	// ... and this looks for one if it's the only one around.
	}

	if (!file)
	{
		return NAV_CANT_ACCESS_FILE;
	}

	// check magic number
	int result;
	unsigned int magic;
	result = filesystem->Read( &magic, sizeof(unsigned int), file );
	if (!result || magic != NAV_MAGIC_NUMBER)
	{
		filesystem->Close( file );
		return NAV_INVALID_FILE;
	}

	// read file version number
	unsigned int version;
	result = filesystem->Read( &version, sizeof(unsigned int), file );
	if (!result || version > NavCurrentVersion || version < 4)
	{
		filesystem->Close( file );
		return NAV_BAD_FILE_VERSION;
	}

	// get size of source bsp file and verify that the bsp hasn't changed
	unsigned int saveBspSize;
	filesystem->Read( &saveBspSize, sizeof(unsigned int), file );

	// verify size
	unsigned int bspSize = filesystem->Size( bspPathname );

	if (bspSize != saveBspSize && !navIsInBsp)
	{
		return NAV_FILE_OUT_OF_DATE;
	}

	return NAV_OK;
}

//--------------------------------------------------------------------------------------------------------------
void CommandNavCheckFileConsistency( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	FileFindHandle_t findHandle;
	const char *bspFilename = filesystem->FindFirstEx( "maps/*.bsp", "MOD", &findHandle );
	while ( bspFilename )
	{
		switch ( CheckNavFile( bspFilename ) )
		{
		case NAV_CANT_ACCESS_FILE:
			Warning( "Missing nav file for %s\n", bspFilename );
			break;
		case NAV_INVALID_FILE:
			Warning( "Invalid nav file for %s\n", bspFilename );
			break;
		case NAV_BAD_FILE_VERSION:
			Warning( "Old nav file for %s\n", bspFilename );
			break;
		case NAV_FILE_OUT_OF_DATE:
			Warning( "The nav file for %s is built from an old version of the map\n", bspFilename );
			break;
		case NAV_OK:
			Msg( "The nav file for %s is up-to-date\n", bspFilename );
			break;
		}

		bspFilename = filesystem->FindNext( findHandle );
	}
	filesystem->FindClose( findHandle );
}
static ConCommand nav_check_file_consistency( side_registry(), "nav_check_file_consistency", CommandNavCheckFileConsistency, "Scans the maps directory and reports any missing/out-of-date navigation files.", FCVAR_GAMEDLL | FCVAR_CHEAT );

//--------------------------------------------------------------------------------------------------------------
/**
 * Reads the used place names from the nav file (can be used to selectively precache before the nav is loaded)
 */
const std::vector< Place > *CNavMesh::GetPlacesFromNavFile( bool *hasUnnamedPlaces )
{
	placeDirectory.Reset();
	// nav filename is derived from map filename
	char filename[256];
	V_snprintf( filename, sizeof( filename ), FORMAT_NAVFILE, STRING( gpGlobals->mapname ) );

	auto fileData = filesystem->ReadFile( filename, "GAME" );	// this ignores .nav files embedded in the .bsp ...
	if ( !fileData )
	{
		fileData = filesystem->ReadFile( filename, "BSP" );	// ... and this looks for one if it's the only one around.
		if ( !fileData )
		{
			return nullptr;
		}
	}

	ks::byte_reader fileBuffer( *fileData );

	// check magic number
	unsigned int magic = fileBuffer.read< unsigned int >();
	if ( !fileBuffer.ok() || magic != NAV_MAGIC_NUMBER )
	{
		return nullptr;	// Corrupt nav file?
	}

	// read file version number
	unsigned int version = fileBuffer.read< unsigned int >();
	if ( !fileBuffer.ok() || version > NavCurrentVersion )
	{
		return nullptr;	// Unknown nav file version
	}

	if ( version < 5 )
	{
		return nullptr;	// Too old to have place names
	}

	unsigned int subVersion = 0;
	if ( version >= 10 )
	{
		subVersion = fileBuffer.read< unsigned int >();
		if ( !fileBuffer.ok() )
		{
			return nullptr;	// No sub-version
		}
	}

	fileBuffer.read< unsigned int >();	// skip BSP file size
	if ( version >= 14 )
	{
		fileBuffer.read< unsigned char >();	// skip m_isAnalyzed
	}

	placeDirectory.Load( fileBuffer, version );

	LoadCustomDataPreArea( fileBuffer, subVersion );

	if ( hasUnnamedPlaces )
	{
		*hasUnnamedPlaces = placeDirectory.HasUnnamedPlaces();
	}

	return placeDirectory.GetPlaces();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load AI navigation data from a file
 */
NavErrorType CNavMesh::Load( void )
{
	MDLCACHE_CRITICAL_SECTION();

	// free previous navigation mesh data
	Reset();
	placeDirectory.Reset();
	CNavVectorNoEditAllocator::Reset();

	GameRules()->OnNavMeshLoad();

	CNavArea::m_nextID = 1;

	// nav filename is derived from map filename
	char filename[256];
	V_snprintf( filename, sizeof( filename ), FORMAT_NAVFILE, STRING( gpGlobals->mapname ) );

	bool navIsInBsp = false;
	auto fileData = filesystem->ReadFile( filename, "MOD" );	// this ignores .nav files embedded in the .bsp ...
	if ( !fileData )
	{
		navIsInBsp = true;
		fileData = filesystem->ReadFile( filename, "BSP" );	// ... and this looks for one if it's the only one around.
		if ( !fileData )
		{
			return NAV_CANT_ACCESS_FILE;
		}
	}

	ks::byte_reader fileBuffer( *fileData );

	// check magic number
	unsigned int magic = fileBuffer.read< unsigned int >();
	if ( !fileBuffer.ok() || magic != NAV_MAGIC_NUMBER )
	{
		Msg( "Invalid navigation file '%s'.\n", filename );
		return NAV_INVALID_FILE;
	}

	// read file version number
	unsigned int version = fileBuffer.read< unsigned int >();
	if ( !fileBuffer.ok() || version > NavCurrentVersion )
	{
		Msg( "Unknown navigation file version.\n" );
		return NAV_BAD_FILE_VERSION;
	}
	
	unsigned int subVersion = 0;
	if ( version >= 10 )
	{
		subVersion = fileBuffer.read< unsigned int >();
		if ( !fileBuffer.ok() )
		{
			Msg( "Error reading sub-version number.\n" );
			return NAV_INVALID_FILE;
		}
	}

	if ( version >= 4 )
	{
		// get size of source bsp file and verify that the bsp hasn't changed
		unsigned int saveBspSize = fileBuffer.read< unsigned int >();

		// verify size
		char *bspFilename = GetBspFilename( filename );
		if ( bspFilename == nullptr )
		{
			return NAV_INVALID_FILE;
		}

		unsigned int bspSize = filesystem->Size( bspFilename );

		if ( bspSize != saveBspSize && !navIsInBsp )
		{     
			if ( engine->IsDedicatedServer() )
			{
				// Warning doesn't print to the dedicated server console, so we'll use Msg instead
				DevMsg( "The Navigation Mesh was built using a different version of this map.\n" );
			}
			else
			{
				DevWarning( "The Navigation Mesh was built using a different version of this map.\n" );
			}
			m_isOutOfDate = true;
		}
	}

	if ( version >= 14 )
	{
		m_isAnalyzed = fileBuffer.read< unsigned char >() != 0;
	}
	else
	{
		m_isAnalyzed = false;
	}

	// load Place directory
	if ( version >= 5 )
	{
		placeDirectory.Load( fileBuffer, version );
	}

	LoadCustomDataPreArea( fileBuffer, subVersion );

	// get number of areas
	unsigned int count = fileBuffer.read< unsigned int >();
	unsigned int i;

	if ( count == 0 )
	{
		return NAV_INVALID_FILE;
	}

	Extent extent;
	extent.lo.x = 9999999999.9f;
	extent.lo.y = 9999999999.9f;
	extent.hi.x = -9999999999.9f;
	extent.hi.y = -9999999999.9f;

	// load the areas and compute total extent
	TheNavMesh->PreLoadAreas( count );
	Extent areaExtent;
	for( i=0; i<count; ++i )
	{
		CNavArea *area = TheNavMesh->CreateArea();
		area->Load( fileBuffer, version, subVersion );
		TheNavAreas.push_back( area );

		area->GetExtent( &areaExtent );

		if (areaExtent.lo.x < extent.lo.x)
			extent.lo.x = areaExtent.lo.x;
		if (areaExtent.lo.y < extent.lo.y)
			extent.lo.y = areaExtent.lo.y;
		if (areaExtent.hi.x > extent.hi.x)
			extent.hi.x = areaExtent.hi.x;
		if (areaExtent.hi.y > extent.hi.y)
			extent.hi.y = areaExtent.hi.y;
	}

	// add the areas to the grid
	AllocateGrid( extent.lo.x, extent.hi.x, extent.lo.y, extent.hi.y );

	for ( CNavArea *area : TheNavAreas )
	{
		AddNavArea( area );
	}

	//
	// Set up all the ladders
	//
	if (version >= 6)
	{
		count = fileBuffer.read< unsigned int >();
		m_ladders.reserve( count );

		// load the ladders
		for( i=0; i<count; ++i )
		{
			CNavLadder *ladder = new CNavLadder;
			ladder->Load( fileBuffer, version );
			m_ladders.push_back( ladder );
		}
	}
	else
	{
		BuildLadders();
	}

	// mark stairways (TODO: this can be removed once all maps are re-saved with this attribute in them)
	MarkStairAreas();

	//
	// Load game-specific mesh info
	//
	LoadCustomData( fileBuffer, subVersion );

	//
	// Bind pointers, etc
	//
	NavErrorType loadResult = PostLoad( version );

	WarnIfMeshNeedsAnalysis( version );

	return loadResult;
}

struct OneWayLink_t
{
	CNavArea *destArea;
	CNavArea *area;
	int backD;

	static int Compare(const OneWayLink_t *lhs, const OneWayLink_t *rhs )
	{
		int result = ( lhs->destArea - rhs->destArea );
		if ( result != 0 )
		{
			return result;
		}
		return ( lhs->backD - rhs->backD );
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked after all areas have been loaded - for pointer binding, etc
 */
NavErrorType CNavMesh::PostLoadBase( unsigned int version )
{
	// allow areas to connect to each other, etc
	for ( CNavArea *area : TheNavAreas )
	{
		area->PostLoad();
	}

	// allow hiding spots to compute information
	for ( const auto &pSpot : TheHidingSpots )
	{
		HidingSpot *spot = pSpot.get();
		spot->PostLoad();
	}

	if ( version < 8 )
	{
		// Old nav meshes need to compute earliest occupy times
		for ( CNavArea *area : TheNavAreas )
		{
			area->ComputeEarliestOccupyTimes();
		}
	}

	ComputeBattlefrontAreas();
	
	//
	// Allow each nav area to know what other areas have one-way connections to it. Need to gather
	// then sort due to allocation restrictions on the 360
	//

	OneWayLink_t oneWayLink;
	ks::small_vector< OneWayLink_t, 512 > oneWayLinks;

	for ( CNavArea *pNavArea : TheNavAreas )
	{
		oneWayLink.area = pNavArea;
	
		for( int d=0; d<NUM_DIRECTIONS; d++ )
		{
			const NavConnectVector *connectList = oneWayLink.area->GetAdjacentAreas( (NavDirType)d );

			for ( NavConnect connect : *connectList )
			{
				oneWayLink.destArea = connect.area;
			
				// if the area we connect to has no connection back to us, allow that area to remember us as an incoming connection
				oneWayLink.backD = OppositeDirection( (NavDirType)d );		
				const NavConnectVector *backConnectList = oneWayLink.destArea->GetAdjacentAreas( (NavDirType)oneWayLink.backD );
				bool isOneWay = true;
				for ( NavConnect backConnect : *backConnectList )
				{
					if (backConnect.area->GetID() == oneWayLink.area->GetID())
					{
						isOneWay = false;
						break;
					}
				}
				
				if (isOneWay)
				{
					oneWayLinks.push_back( oneWayLink );
				}
			}
		}
	}

	ks::qsort( oneWayLinks, &OneWayLink_t::Compare );

	for ( int i = 0; i < std::ssize( oneWayLinks ); i++ )
	{
		// add this one-way connection
		oneWayLinks[i].destArea->AddIncomingConnection( oneWayLinks[i].area, (NavDirType)oneWayLinks[i].backD );	
	}

	ValidateNavAreaConnections();

	// TERROR: loading into a map directly creates entities before the mesh is loaded.  Tell the preexisting
	// entities now that the mesh is loaded so they can update areas.
	for ( int i=0; i<std::ssize( m_avoidanceObstacles ); ++i )
	{
		m_avoidanceObstacles[i]->OnNavMeshLoaded();
	}

	// the Navigation Mesh has been successfully loaded
	m_isLoaded = true;
	
	return NAV_OK;
}

#undef FORMAT_NAVFILE
