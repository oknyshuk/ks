//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "filters.h"
#include "entitylist.h"
#include "ai_squad.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ###################################################################
//	> BaseFilter
// ###################################################################
LINK_ENTITY_TO_CLASS(filter_base, CBaseFilter);

IMPLEMENT_REFLECT_DATAMAP( CBaseFilter )

//-----------------------------------------------------------------------------

bool CBaseFilter::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	return true;
}


bool CBaseFilter::PassesFilter( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	bool baseResult = PassesFilterImpl( pCaller, pEntity );
	return (m_bNegated) ? !baseResult : baseResult;
}


bool CBaseFilter::PassesDamageFilter(const CTakeDamageInfo &info)
{
	bool baseResult = PassesDamageFilterImpl(info);
	return (m_bNegated) ? !baseResult : baseResult;
}


bool CBaseFilter::PassesDamageFilterImpl( const CTakeDamageInfo &info )
{
	return PassesFilterImpl( NULL, info.GetAttacker() );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for testing the activator. If the activator passes the
//			filter test, the OnPass output is fired. If not, the OnFail output is fired.
//-----------------------------------------------------------------------------
void CBaseFilter::InputTestActivator( inputdata_t &inputdata )
{
	if ( PassesFilter( inputdata.pCaller, inputdata.pActivator ) )
	{
		m_OnPass.FireOutput( inputdata.pActivator, this );
	}
	else
	{
		m_OnFail.FireOutput( inputdata.pActivator, this );
	}
}


// ###################################################################
//	> FilterMultiple
//
//   Allows one to filter through mutiple filters
// ###################################################################
#define MAX_FILTERS 10
enum filter_t
{
	FILTER_AND,
	FILTER_OR,
};

class CFilterMultiple : public CBaseFilter
{
	DECLARE_CLASS( CFilterMultiple, CBaseFilter );
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "FilterType" } ]] filter_t	m_nFilterType;
	[[= ks::reflect::Key{ .name = "Filter10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Filter09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Filter08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Filter07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Filter06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Filter05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Filter04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Filter03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Filter02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Filter01", .index = 0 } ]] string_t	m_iFilterName[MAX_FILTERS];
	EHANDLE		m_hFilter[MAX_FILTERS];

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );
	bool PassesDamageFilterImpl(const CTakeDamageInfo &info);
	void Activate(void);
};

LINK_ENTITY_TO_CLASS(filter_multi, CFilterMultiple);

IMPLEMENT_REFLECT_DATAMAP( CFilterMultiple )



//------------------------------------------------------------------------------
// Purpose : Called after all entities have been loaded
//------------------------------------------------------------------------------
void CFilterMultiple::Activate( void )
{
	BaseClass::Activate();
	
	// We may reject an entity specified in the array of names, but we want the array of valid filters to be contiguous!
	int nNextFilter = 0;

	// Get handles to my filter entities
	for ( int i = 0; i < MAX_FILTERS; i++ )
	{
		if ( m_iFilterName[i] != NULL_STRING )
		{
			CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_iFilterName[i] );
			CBaseFilter *pFilter = dynamic_cast<CBaseFilter *>(pEntity);
			if ( pFilter == NULL )
			{
				Warning("filter_multi: Tried to add entity (%s) which is not a filter entity!\n", STRING( m_iFilterName[i] ) );
				continue;
			}

			// Take this entity and increment out array pointer
			m_hFilter[nNextFilter] = pFilter;
			nNextFilter++;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity passes our filter, false if not.
// Input  : pEntity - Entity to test.
//-----------------------------------------------------------------------------
bool CFilterMultiple::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	// Test against each filter
	if (m_nFilterType == FILTER_AND)
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (!pFilter->PassesFilter( pCaller, pEntity ) )
				{
					return false;
				}
			}
		}
		return true;
	}
	else  // m_nFilterType == FILTER_OR
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (pFilter->PassesFilter( pCaller, pEntity ) )
				{
					return true;
				}
			}
		}
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity passes our filter, false if not.
// Input  : pEntity - Entity to test.
//-----------------------------------------------------------------------------
bool CFilterMultiple::PassesDamageFilterImpl(const CTakeDamageInfo &info)
{
	// Test against each filter
	if (m_nFilterType == FILTER_AND)
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (!pFilter->PassesDamageFilter(info))
				{
					return false;
				}
			}
		}
		return true;
	}
	else  // m_nFilterType == FILTER_OR
	{
		for (int i=0;i<MAX_FILTERS;i++)
		{
			if (m_hFilter[i] != NULL)
			{
				CBaseFilter* pFilter = (CBaseFilter *)(m_hFilter[i].Get());
				if (pFilter->PassesDamageFilter(info))
				{
					return true;
				}
			}
		}
		return false;
	}
}


// ###################################################################
//	> FilterName
// ###################################################################
class CFilterName : public CBaseFilter
{
	DECLARE_CLASS( CFilterName, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t m_iFilterName;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		// special check for !player as GetEntityName for player won't return "!player" as a name
		if (FStrEq(STRING(m_iFilterName), "!player"))
		{
			return pEntity->IsPlayer();
		}
		else
		{
			return pEntity->NameMatches( STRING(m_iFilterName) );
		}
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_name, CFilterName );

IMPLEMENT_REFLECT_DATAMAP( CFilterName )

// ###################################################################
//	> FilterModel
// ###################################################################
class CFilterModel : public CBaseFilter
{
	DECLARE_CLASS( CFilterModel, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "model" } ]] string_t m_iFilterModel;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		return ( FStrEq( STRING( m_iFilterModel ), STRING( pEntity->GetModelName() ) ) );

	}
};

LINK_ENTITY_TO_CLASS( filter_activator_model, CFilterModel );

IMPLEMENT_REFLECT_DATAMAP( CFilterModel )

// ###################################################################
//	> FilterContext
// ###################################################################
class CFilterContext : public CBaseFilter
{
	DECLARE_CLASS( CFilterContext, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "ResponseContext" } ]] string_t m_iFilterContext;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		int i = pEntity->FindContextByName( STRING( m_iFilterContext ) );
		return ( ( i != -1 && atoi( pEntity->GetContextValue( i ) ) > 0 ) );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_context, CFilterContext );

IMPLEMENT_REFLECT_DATAMAP( CFilterContext )

// ###################################################################
//	> FilterClass
// ###################################################################
class CFilterClass : public CBaseFilter
{
	DECLARE_CLASS( CFilterClass, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filterclass" } ]] string_t m_iFilterClass;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		return pEntity->ClassMatches( STRING(m_iFilterClass) );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_class, CFilterClass );

IMPLEMENT_REFLECT_DATAMAP( CFilterClass )


// ###################################################################
//	> FilterTeam
// ###################################################################
class FilterTeam : public CBaseFilter
{
	DECLARE_CLASS( FilterTeam, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filterteam" } ]] int		m_iFilterTeam;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
	 	return ( pEntity != NULL && pEntity->GetTeamNumber() == m_iFilterTeam );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_team, FilterTeam );

IMPLEMENT_REFLECT_DATAMAP( FilterTeam )


// ###################################################################
//	> FilterMassGreater
// ###################################################################
class CFilterMassGreater : public CBaseFilter
{
	DECLARE_CLASS( CFilterMassGreater, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filtermass" } ]] float m_fFilterMass;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		if ( pEntity->VPhysicsGetObject() == NULL )
			return false;

		return ( pEntity->VPhysicsGetObject()->GetMass() > m_fFilterMass );
	}
};

LINK_ENTITY_TO_CLASS( filter_activator_mass_greater, CFilterMassGreater );

IMPLEMENT_REFLECT_DATAMAP( CFilterMassGreater )


// ###################################################################
//	> FilterDamageType
// ###################################################################
class FilterDamageType : public CBaseFilter
{
	DECLARE_CLASS( FilterDamageType, CBaseFilter );
	DECLARE_DATADESC();

protected:

	bool PassesFilterImpl(CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		ASSERT( false );
	 	return true;
	}

	bool PassesDamageFilterImpl(const CTakeDamageInfo &info)
	{
	 	return (info.GetDamageType() & ~DMG_DIRECT) == m_iDamageType;
	}

	[[= ks::reflect::Key{ .name = "damagetype" } ]] int m_iDamageType;
};

LINK_ENTITY_TO_CLASS( filter_damage_type, FilterDamageType );

IMPLEMENT_REFLECT_DATAMAP( FilterDamageType )

// ###################################################################
//	> CFilterEnemy
// ###################################################################

#define SF_FILTER_ENEMY_NO_LOSE_AQUIRED	(1<<0)

class CFilterEnemy : public CBaseFilter
{
	DECLARE_CLASS( CFilterEnemy, CBaseFilter );
		// NOT SAVED	
		// m_iszPlayerName
	DECLARE_DATADESC();

public:

	virtual bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );
	virtual bool PassesDamageFilterImpl( const CTakeDamageInfo &info );

private:

	bool	PassesNameFilter( CBaseEntity *pCaller );
	bool	PassesProximityFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy );
	bool	PassesMobbedFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy );
#ifdef PORTAL2
	bool	PassesSizeFilter( CBaseEntity *pEnemy );
#endif // PORTAL2

	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t	m_iszEnemyName;				// Name or classname
	[[= ks::reflect::Key{ .name = "filter_radius" } ]] float		m_flRadius;					// Radius (enemies are acquired at this range)
	[[= ks::reflect::Key{ .name = "filter_outer_radius" } ]] float		m_flOuterRadius;			// Outer radius (enemies are LOST at this range)
	[[= ks::reflect::Key{ .name = "filter_max_per_enemy" } ]] int			m_nMaxSquadmatesPerEnemy;	// Maximum number of squadmates who may share the same enemy
	string_t	m_iszPlayerName;			// "!player"

#ifdef PORTAL2
	int			m_nObjectSize;				// Size the object must be
#endif // PORTAL2
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	if ( pCaller == NULL || pEntity == NULL )
		return false;

	// If asked to, we'll never fail to pass an already acquired enemy
	//	This allows us to use test criteria to initially pick an enemy, then disregard the test until a new enemy comes along
	if ( HasSpawnFlags( SF_FILTER_ENEMY_NO_LOSE_AQUIRED ) && ( pEntity == pCaller->GetEnemy() ) )
		return true;

	// This is a little weird, but it's saying that if we're not the entity we're excluding the filter to, then just pass it through
	if ( PassesNameFilter( pEntity ) == false )
		return true;

	if ( PassesProximityFilter( pCaller, pEntity ) == false )
		return false;

	// NOTE: This can result in some weird NPC behavior if used improperly
	if ( PassesMobbedFilter( pCaller, pEntity ) == false )
		return false;

#ifdef PORTAL2
	if ( PassesSizeFilter( pEntity ) == false )
		return false;
#endif // PORTAL2

	// The filter has been passed, meaning:
	//	- If we wanted all criteria to fail, they have
	//  - If we wanted all criteria to succeed, they have

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesDamageFilterImpl( const CTakeDamageInfo &info )
{
	// NOTE: This function has no meaning to this implementation of the filter class!
	Assert( 0 );
	return false;
}

#ifdef PORTAL2
//-----------------------------------------------------------------------------
// Purpose: Tests the enemy's size against a desired size
// Input  : *pEnemy - Entity being assessed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesSizeFilter( CBaseEntity *pEnemy )
{
	CBaseAnimating *pAnim = pEnemy->GetBaseAnimating();
	if ( pAnim == NULL )
		return false;

	return ( pAnim->GetObjectScaleLevel() == m_nObjectSize );
}
#endif // PORTAL2

//-----------------------------------------------------------------------------
// Purpose: Tests the enemy's name or classname
// Input  : *pEnemy - Entity being assessed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesNameFilter( CBaseEntity *pEnemy )
{
	// If there is no name specified, we're not using it
	if ( m_iszEnemyName	== NULL_STRING )
		return true;

	// Cache off the special case player name
	if ( m_iszPlayerName == NULL_STRING )
	{
		m_iszPlayerName = FindPooledString( "!player" );
	}

	if ( m_iszEnemyName == m_iszPlayerName )
	{
		if ( pEnemy->IsPlayer() )
		{
			if ( m_bNegated )
				return false;

			return true;
		}
	}

	// May be either a targetname or classname
	bool bNameOrClassnameMatches = ( m_iszEnemyName == pEnemy->GetEntityName() || m_iszEnemyName == pEnemy->m_iClassname );

	// We only leave this code block in a state meaning we've "succeeded" in any context
	if ( m_bNegated )
	{
		// We wanted the names to not match, but they did
		if ( bNameOrClassnameMatches )
			return false;
	}
	else
	{
		// We wanted them to be the same, but they weren't
		if ( bNameOrClassnameMatches == false )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Tests the enemy's proximity to the caller's position
// Input  : *pCaller - Entity assessing the target
//			*pEnemy - Entity being assessed
// Output : Returns true if potential enemy passes this filter stage
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesProximityFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy )
{
	// If there is no radius specified, we're not testing it
	if ( m_flRadius <= 0.0f )
		return true;

	// We test the proximity differently when we've already picked up this enemy before
	bool bAlreadyEnemy = ( pCaller->GetEnemy() == pEnemy );

	// Get our squared length to the enemy from the caller
	float flDistToEnemySqr = ( pCaller->GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();

	// Two radii are used to control oscillation between true/false cases
	// The larger radius is either specified or defaulted to be double or half the size of the inner radius
	float flLargerRadius = m_flOuterRadius;
	if ( flLargerRadius == 0 )
	{
		flLargerRadius = ( m_bNegated ) ? (m_flRadius*0.5f) : (m_flRadius*2.0f);
	}

	float flSmallerRadius = m_flRadius;
	if ( flSmallerRadius > flLargerRadius )
	{
		V_swap( flLargerRadius, flSmallerRadius );
	}

	float flDist;	
	if ( bAlreadyEnemy )
	{
		flDist = ( m_bNegated ) ? flSmallerRadius : flLargerRadius;
	}
	else
	{
		flDist = ( m_bNegated ) ? flLargerRadius : flSmallerRadius;
	}

	// Test for success
	if ( flDistToEnemySqr <= (flDist*flDist) )
	{
		// We wanted to fail but didn't
		if ( m_bNegated )
			return false;

		return true;
	}
	
	// We wanted to succeed but didn't
	if ( m_bNegated == false )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to govern how many squad members can target any given entity
// Input  : *pCaller - Entity assessing the target
//			*pEnemy - Entity being assessed
// Output : Returns true if potential enemy passes this filter stage
//-----------------------------------------------------------------------------
bool CFilterEnemy::PassesMobbedFilter( CBaseEntity *pCaller, CBaseEntity *pEnemy )
{
	// Must be a valid candidate
	CAI_BaseNPC *pNPC = pCaller->MyNPCPointer();
	if ( pNPC == NULL || pNPC->GetSquad() == NULL )
		return true;

	// Make sure we're checking for this
	if ( m_nMaxSquadmatesPerEnemy <= 0 )
		return true;

	AISquadIter_t iter;
	int nNumMatchingSquadmates = 0;
	
	// Look through our squad members to see how many of them are already mobbing this entity
	for ( CAI_BaseNPC *pSquadMember = pNPC->GetSquad()->GetFirstMember( &iter ); pSquadMember != NULL; pSquadMember = pNPC->GetSquad()->GetNextMember( &iter ) )
	{
		// Disregard ourself
		if ( pSquadMember == pNPC )
			continue;

		// If the enemies match, count it
		if ( pSquadMember->GetEnemy() == pEnemy )
		{
			nNumMatchingSquadmates++;

			// If we're at or passed the max we stop
			if ( nNumMatchingSquadmates >= m_nMaxSquadmatesPerEnemy )
			{
				// We wanted to find more than allowed and we did
				if ( m_bNegated )
					return true;
				
				// We wanted to be less but we're not
				return false;
			}
		}
	}

	// We wanted to find more than the allowed amount but we didn't
	if ( m_bNegated )
		return false;

	return true;
}

LINK_ENTITY_TO_CLASS( filter_enemy, CFilterEnemy );

IMPLEMENT_REFLECT_DATAMAP( CFilterEnemy )

#ifdef PORTAL2

// ###################################################################
//	> FilterSize
// ###################################################################

class CFilterSize : public CBaseFilter
{
	DECLARE_CLASS( CFilterSize, CBaseFilter );
	DECLARE_DATADESC();

public:
	[[= ks::reflect::Key{ .name = "filtersize" } ]] int	m_nFilterSize;

	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		CBaseAnimating *pAnim = pEntity->GetBaseAnimating();
		if ( pAnim == NULL )
			return false;

		return ( pAnim->GetObjectScaleLevel() == m_nFilterSize );
	}
};

LINK_ENTITY_TO_CLASS( filter_size, CFilterSize );

IMPLEMENT_REFLECT_DATAMAP( CFilterSize )


// ###################################################################
//	> FilterPlayerHeld
// ###################################################################

class CFilterPlayerHeld : public CBaseFilter
{
	DECLARE_CLASS( CFilterPlayerHeld, CBaseFilter );

public:
	bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
	{
		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if( (pPhys != NULL) && (pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
		{
			return true;
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS( filter_player_held, CFilterPlayerHeld );


#endif // PORTAL2
