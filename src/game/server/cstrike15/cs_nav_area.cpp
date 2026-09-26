//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "cbase.h"
#include "cs_nav_mesh.h"
#include "cs_nav_area.h"
#include "cs_gamerules.h"
#include "nav_pathfind.h"
#include "nav_colors.h"
#include "fmtstr.h"
#include "props_shared.h"
#include "func_breakablesurf.h"
#include "color.h"
#include "collisionutils.h"
#include "point_hiding_spot.h"



//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during normal runtime.
 */
CCSNavArea::CCSNavArea( void )
{
	m_approachCount = 0;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CCSNavArea::~CCSNavArea()
{
}


void CCSNavArea::OnServerActivate( void )
{
	CNavArea::OnServerActivate();

}

void CCSNavArea::OnRoundRestart( void )
{
	CNavArea::OnRoundRestart();
}


void CCSNavArea::Save( ks::byte_writer &fileBuffer, unsigned int version ) const
{
	CNavArea::Save( fileBuffer, version );

	//
	// Save the approach areas for this area
	//

	// save number of approach areas
	fileBuffer.write< unsigned char >( m_approachCount);

	// save approach area info
	for( int a=0; a<m_approachCount; ++a )
	{
		if (m_approach[a].here.area)
			fileBuffer.write< unsigned int >( m_approach[a].here.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);

		if (m_approach[a].prev.area)
			fileBuffer.write< unsigned int >( m_approach[a].prev.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);
		fileBuffer.write< unsigned char >( m_approach[a].prevToHereHow);

		if (m_approach[a].next.area)
			fileBuffer.write< unsigned int >( m_approach[a].next.area->GetID());
		else
			fileBuffer.write< unsigned int >( 0);
		fileBuffer.write< unsigned char >( m_approach[a].hereToNextHow);
	}
}

NavErrorType CCSNavArea::Load( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion )
{
	if ( version < 15 )
		return LoadLegacy(fileBuffer, version, subVersion);

	// load base class data
	NavErrorType error = CNavArea::Load( fileBuffer, version, subVersion );

	switch ( subVersion )
	{
	case 1:
		//
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.read< unsigned char >();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.read< unsigned int >();

			m_approach[a].prev.id = fileBuffer.read< unsigned int >();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.read< unsigned char >();

			m_approach[a].next.id = fileBuffer.read< unsigned int >();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.read< unsigned char >();
		}

		if ( !fileBuffer.ok() )
			error = NAV_INVALID_FILE;

		// fall through

	case 0:
		// legacy version
		break;

	default:
		Warning( "Unknown NavArea sub-version number\n" );
		error = NAV_INVALID_FILE;
	}

	return error;
}


NavErrorType CCSNavArea::PostLoad( void )
{
	NavErrorType error = CNavArea::PostLoad();

	// resolve approach area IDs
	for ( int a = 0; a < m_approachCount; ++a )
	{
		m_approach[a].here.area = TheNavMesh->GetNavAreaByID( m_approach[a].here.id );
		if (m_approach[a].here.id && m_approach[a].here.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (here).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].prev.area = TheNavMesh->GetNavAreaByID( m_approach[a].prev.id );
		if (m_approach[a].prev.id && m_approach[a].prev.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (prev).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].next.area = TheNavMesh->GetNavAreaByID( m_approach[a].next.id );
		if (m_approach[a].next.id && m_approach[a].next.area == nullptr)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (next).\n" );
			error = NAV_CORRUPT_DATA;
		}
	}
	return error;
}


void CCSNavArea::Draw( void ) const
{
	CNavArea::Draw();
}

void CCSNavArea::CustomAnalysis( bool isIncremental /*= false */ )
{
	ComputeApproachAreas();
}


//--------------------------------------------------------------------------------------------------------------
// return danger decay rate per second
float CCSNavArea::GetDangerDecayRate( void ) const
{
	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// decay danger faster in this aggressive bomb planting mode
		return 1.0f / 30.0f;
	}

	// one kill == 1.0, which we will forget about in two minutes
	return 1.0f / 120.0f;
}


float CCSNavArea::GetEarliestOccupyTime( int teamID ) const
{
	if ( CSGameRules()->IsPlayingCoopMission() )
		return 0.0f;

	return BaseClass::GetEarliestOccupyTime( teamID );
}

void CCSNavArea::UpdateBlocked( bool force /*= false*/, int teamID /*= TEAM_ANY */ )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		BaseClass::UpdateBlocked( force, teamID );
}

bool CCSNavArea::IsBlocked( int teamID, bool ignoreNavBlockers /*= false */ ) const
{
	return ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() ) ? BaseClass::IsBlocked( teamID, ignoreNavBlockers ) : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load legacy navigation area from the file
 */
NavErrorType CCSNavArea::LoadLegacy( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion )
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
		Assert( fileBuffer.ok() );

		m_connect[d].reserve( count );
		for( unsigned int i=0; i<count; ++i )
		{
			NavConnect connect;
			connect.id = fileBuffer.read< unsigned int >();
			Assert( fileBuffer.ok() );

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
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.read< unsigned char >();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.read< unsigned int >();

			m_approach[a].prev.id = fileBuffer.read< unsigned int >();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.read< unsigned char >();

			m_approach[a].next.id = fileBuffer.read< unsigned int >();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.read< unsigned char >();
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
			for ( const auto &ladderConnect : m_ladder[dir] )
			{
				if ( ladderConnect.id == connect.id )
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


CCSHidingSpot::~CCSHidingSpot()
{
	if ( m_pOwningEntity )
		m_pOwningEntity->DetachFromHidingSpot();
}
