// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// nav_entities.cpp
// AI Navigation entities
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

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
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "te.h"
#include "ndebugoverlay.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

#include "nav_mesh.h"
#include "nav_node.h"
#include "nav_pathfind.h"
#include "props_shared.h"
#include "func_breakablesurf.h"

#include "collisionutils.h"
#include "team.h"
#include "nav_entities.h"
#include "props.h"

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
