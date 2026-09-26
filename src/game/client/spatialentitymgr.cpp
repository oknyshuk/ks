//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose : Singleton manager for color correction on the client
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"

#include "spatialentitymgr.h"
#include "c_spatialentity.h"


// Gets called each frame
void CSpatialEntityMgr::Update( float frametime )
{
	if ( std::ssize( m_SpatialEntities ) <= 0 )
		return;

	m_SpatialEntities[ 0 ]->ResetAccumulation();

	for ( int i = 0; i < std::ssize( m_SpatialEntities ); ++i )
	{
		m_SpatialEntities[ i ]->Accumulate();
	}

	m_SpatialEntities[ 0 ]->ApplyAccumulation();
}

//------------------------------------------------------------------------------
// Creates, destroys spatial entities
//------------------------------------------------------------------------------
void CSpatialEntityMgr::AddSpatialEntity( C_SpatialEntity *pSpatialEntity )
{
	m_SpatialEntities.push_back( pSpatialEntity );
}

void CSpatialEntityMgr::RemoveSpatialEntity( C_SpatialEntity *pSpatialEntity )
{
	ks::erase_first( m_SpatialEntities, pSpatialEntity );
}
