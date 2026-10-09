//-----------------------------------------------------------------------------
// class CPointEntityFinder
//
// Purpose: Finds an entity using a specified heuristic and outputs it as !caller
//			with the OnFoundEntity output.
//-----------------------------------------------------------------------------

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
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
#include "base_transmit_proxy.h"
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
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "filters.h"


enum EntFinderMethod_t
{
	ENT_FIND_METHOD_NEAREST = 0,
	ENT_FIND_METHOD_FARTHEST,
	ENT_FIND_METHOD_RANDOM,
};

class CPointEntityFinder : public CBaseEntity
{
	void Activate( void );

	DECLARE_CLASS( CPointEntityFinder, CBaseEntity );

private:

	EHANDLE						m_hEntity;
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t					m_iFilterName;
	CHandle<class CBaseFilter>	m_hFilter;
	[[= ks::reflect::Key{ .name = "referencename" } ]] string_t					m_iRefName;
	EHANDLE						m_hReference;

	[[= ks::reflect::Key{ .name = "method" } ]] EntFinderMethod_t			m_FindMethod;

	void FindEntity( void );
	void FindByDistance( void );
	void FindByRandom( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "FindEntity", .type = FIELD_VOID } ]] void InputFindEntity( inputdata_t &inputdata );

	// Output handlers
	[[= ks::reflect::Key{ .name = "OnFoundEntity" } ]] COutputEvent m_OnFoundEntity;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_entity_finder, CPointEntityFinder );

IMPLEMENT_REFLECT_DATAMAP( CPointEntityFinder )


void CPointEntityFinder::Activate( void )
{
	// Get the filter, if it exists.
	if (m_iFilterName != NULL_STRING)
	{
		m_hFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( nullptr, m_iFilterName ));
	}

	BaseClass::Activate();
}


void CPointEntityFinder::FindEntity( void )
{
	// Get the reference entity, if it exists.
	if (m_iRefName != NULL_STRING)
	{
		m_hReference = gEntList.FindEntityByName( nullptr, m_iRefName );
	}

	switch ( m_FindMethod )
	{

	case ( ENT_FIND_METHOD_NEAREST ):
		FindByDistance();
		break;
	case ( ENT_FIND_METHOD_FARTHEST ):
		FindByDistance();
		break;
	case ( ENT_FIND_METHOD_RANDOM ):
		FindByRandom();
		break;
	}
}

void CPointEntityFinder::FindByDistance( void )
{
	m_hEntity = nullptr;
	CBaseFilter *pFilter = m_hFilter.Get();

// go through each entity and determine whether it's closer or farther from the current entity.  Pick according to Method selected.

	float flBestDist = 0;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( FStrEq( STRING( pEntity->m_iClassname ), "worldspawn" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "soundent" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "player_manager" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "bodyque" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "ai_network" ) 
			|| pEntity == this
			|| ( pFilter && !( pFilter->PassesFilter( this, pEntity ) ) ) )	   
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		// if we have a reference entity, use that, otherwise, check against 'this'
		Vector vecStart;
		if ( m_hReference )
		{
			vecStart = m_hReference->GetAbsOrigin();
		}
		else
		{
			vecStart = GetAbsOrigin();
		}

		// init m_hEntity with a valid entity.
		if (m_hEntity == nullptr )
		{
			m_hEntity = pEntity;
			flBestDist = ( pEntity->GetAbsOrigin() - vecStart ).LengthSqr();
		}

		float flNewDist = ( pEntity->GetAbsOrigin() - vecStart ).LengthSqr();

		switch ( m_FindMethod )
		{

		case ( ENT_FIND_METHOD_NEAREST ):
			if ( flNewDist < flBestDist )
			{
				m_hEntity = pEntity;
				flBestDist = flNewDist;
			}
			break;

		case ( ENT_FIND_METHOD_FARTHEST ):
			if ( flNewDist > flBestDist )
			{
				m_hEntity = pEntity;
				flBestDist = flNewDist;
			}
			break;

		default:
			contract_assert( false );
			break;
		}

		pEntity = gEntList.NextEnt( pEntity );
	}
}

void CPointEntityFinder::FindByRandom( void )
{
	// TODO: optimize the case where there is no filter
	m_hEntity = nullptr;
	CBaseFilter *pFilter = m_hFilter.Get();
	std::vector< CBaseEntity * > ValidEnts;

	CBaseEntity *pEntity = gEntList.FirstEnt();
	do	   // note all valid entities.
	{
		if ( pFilter &&  pFilter->PassesFilter( this, pEntity ) )
		{
			ValidEnts.push_back( pEntity );
		}

		pEntity = gEntList.NextEnt( pEntity );

	} while ( pEntity );

	// pick one at random
	if ( !ValidEnts.empty() )
	{
		m_hEntity = ValidEnts[ RandomInt( 0, std::ssize( ValidEnts ) - 1 )];
	}
}

void CPointEntityFinder::InputFindEntity( inputdata_t &inputdata )
{
	FindEntity();

	m_OnFoundEntity.FireOutput( inputdata.pActivator, m_hEntity );
}
