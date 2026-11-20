// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <string.h>
#include <ctype.h>
#include "edict.h"
#include "networkvar.h"
#include "tier2/tier2_logging.h"
#include "mathlib/femodeldesc.h"
#include "eiface.h"
module ks.server;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.cs_playeranimstate;
import :baseentity;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :econ.econ_item_schema_minimal;
import ks.core.random;
import ks.tier1.interfaces;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import std;
import ks.core.vprof;
import ks.tier1.strtools;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.tier1.keyvalues;
import ks.core.wchartypes;
import ks.math.mathlib;
import ks.tier1.convar;

class BotProfile;
class CBaseGrenade;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), Leon Hartwig, 2003

/// @todo Remove this nasty hack - CreateFakeClient() calls CBot::Spawn, which needs the profile and team
const BotProfile *g_botInitProfile = nullptr;
int g_botInitTeam = 0;

//
// NOTE: Because CBot had to be templatized, the code was moved into bot.h
//

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------

ActiveGrenade::ActiveGrenade( CBaseGrenade *grenadeEntity )
{
	m_entity = grenadeEntity;
	m_detonationPosition = grenadeEntity->GetAbsOrigin();
	m_dieTimestamp = 0.0f;

	m_isSmoke = false;
	m_isFlashbang = false;
	m_isMolotov = false;
	m_isDecoy = false;

	switch ( grenadeEntity->GetGrenadeType() )
	{
	case GRENADE_TYPE_EXPLOSIVE:	m_radius = HEGrenadeRadius;									break;
	case GRENADE_TYPE_FLASH:		m_radius = FlashbangGrenadeRadius; m_isFlashbang = true;	break;
	case GRENADE_TYPE_FIRE:			m_radius = MolotovGrenadeRadius; m_isMolotov = true;		break;
	case GRENADE_TYPE_DECOY:		m_radius = DecoyGrenadeRadius; m_isDecoy = true;			break;
	case GRENADE_TYPE_SMOKE:		m_radius = SmokeGrenadeRadius; m_isSmoke = true;			break;
	case GRENADE_TYPE_SENSOR:		m_radius = MolotovGrenadeRadius; m_isSensor = true;			break;
	default:
		contract_assert( false && "Invalid grenade type!\n" );
		m_radius = HEGrenadeRadius;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when the grenade in the world goes away
 */
void ActiveGrenade::OnEntityGone( void )
{
	if (m_isSmoke)
	{
		// smoke lingers after grenade is gone
		const float smokeLingerTime = 4.0f;
		m_dieTimestamp = gpGlobals->curtime + smokeLingerTime;
	}

	m_entity = nullptr;
}

//--------------------------------------------------------------------------------------------------------------
void ActiveGrenade::Update( void )
{
	if (m_entity != nullptr)
	{
		m_detonationPosition = m_entity->GetAbsOrigin();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this grenade is valid
 */
bool ActiveGrenade::IsValid( void ) const
{
	if ( m_isSmoke )
	{
		if ( m_entity == nullptr && gpGlobals->curtime > m_dieTimestamp )
		{
			return false;
		}
	}
	else
	{
		if ( m_entity == nullptr )
		{
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
const Vector &ActiveGrenade::GetPosition( void ) const
{
	// smoke grenades can vanish before the smoke itself does - refer to the detonation position
	if (m_entity == nullptr)
		return GetDetonationPosition();

	return m_entity->GetAbsOrigin();
}

class CBaseEntity;
class CBasePlayer;
class CNavArea;
class HidingSpot;

using namespace ::ks::reflect;

// bot_hide.cpp
// Mechanisms for using Hiding Spots in the Navigation Mesh
// Author: Michael Booth, 2003-2004

//--------------------------------------------------------------------------------------------------------------
/**
 * If a player is at the given spot, return true
 */
bool IsSpotOccupied( CBaseEntity *me, const Vector &pos )
{
	const float closeRange = 75.0f;		// 50

	// is there a player in this spot
	float range;
	CBasePlayer *player = UTIL_GetClosestPlayer( pos, &range );

	if (player != me)
	{
		if (player && range < closeRange)
			return true;
	}

	// is there is a hostage in this spot
	// BOTPORT: Implement hostage manager
	/*
	if (g_pHostages)
	{
		CHostage *hostage = g_pHostages->GetClosestHostage( *pos, &range );
		if (hostage && hostage != me && range < closeRange)
			return true;
	}
	*/

	return false;
}

//--------------------------------------------------------------------------------------------------------------
class CollectHidingSpotsFunctor
{
public:
	CollectHidingSpotsFunctor( CBaseEntity *me, const Vector &origin, float range, int flags, Place place = UNDEFINED_PLACE ) : m_origin( origin )
	{
		m_me = me;
		m_count = 0;
		m_range = range;
		m_flags = (unsigned char)flags;
		m_place = place;
		m_totalWeight = 0;
	}

	enum { MAX_SPOTS = 256 };

	bool operator() ( CNavArea *area )
	{
		// if a place is specified, only consider hiding spots from areas in that place
		if (m_place != UNDEFINED_PLACE && area->GetPlace() != m_place)
			return true;

		// collect all the hiding spots in this area
		const HidingSpotVector *pSpots = area->GetHidingSpots();
		
		for ( const HidingSpot *spot : *pSpots )
		{
			// if we've filled up, stop searching
			if (m_count == MAX_SPOTS)
			{
				return false;
			}

			// make sure hiding spot is in range
			if (m_range > 0.0f)
			{
				if ((spot->GetPosition() - m_origin).IsLengthGreaterThan( m_range ))
				{
					continue;
				}
			}

			// if a Player is using this hiding spot, don't consider it
			if (IsSpotOccupied( m_me, spot->GetPosition() ))
			{
				// player is in hiding spot
				/// @todo Check if player is moving or sitting still
				continue;
			}

			if (spot->GetArea() && (spot->GetArea()->GetAttributes() & NAV_MESH_DONT_HIDE))
			{
				// the area has been marked as DONT_HIDE since the last analysis, so let's ignore it
				continue;
			}

			// only collect hiding spots with matching flags
			if (m_flags & spot->GetFlags())
			{
				m_hidingSpot[ m_count ] = &spot->GetPosition();
				m_hidingSpotWeight[ m_count ] = m_totalWeight;

				// if it's an 'avoid' area, give it a low weight
				if ( spot->GetArea() && ( spot->GetArea()->GetAttributes() & NAV_MESH_AVOID ) )
				{
					m_totalWeight += 1;
				}
				else
				{
					m_totalWeight += 2;
				}

				++m_count;
			}
		}

		return (m_count < MAX_SPOTS);
	}

	/**
	 * Remove the spot at index "i"
	 */
	void RemoveSpot( int i )
	{
		if (m_count == 0)
			return;

		for( int j=i+1; j<m_count; ++j )
			m_hidingSpot[j-1] = m_hidingSpot[j];

		--m_count;
	}

	int GetRandomHidingSpot( void )
	{
		int weight = RandomInt( 0, m_totalWeight-1 );
		for ( int i=0; i<m_count-1; ++i )
		{
			// if the next spot's starting weight is over the target weight, this spot is the one
			if ( m_hidingSpotWeight[i+1] >= weight )
			{
				return i;
			}
		}

		// if we didn't find any, it's the last one
		return m_count - 1;
	}

	CBaseEntity *m_me;
	const Vector &m_origin;
	float m_range;

	const Vector *m_hidingSpot[ MAX_SPOTS ];
	int m_hidingSpotWeight[ MAX_SPOTS ];
	int m_totalWeight;
	int m_count;

	unsigned char m_flags;

	Place m_place;
};

/**
 * Do a breadth-first search to find a nearby hiding spot and return it.
 * Don't pick a hiding spot that a Player is currently occupying.
 * @todo Clean up this mess
 */
const Vector *FindNearbyHidingSpot( CBaseEntity *me, const Vector &pos, float maxRange, bool isSniper, bool useNearest )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( pos );
	if (startArea == nullptr)
		return nullptr;

	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::IDEAL_SNIPER_SPOT );
		SearchSurroundingAreas( startArea, pos, collector, maxRange );

		if (collector.m_count)
		{
			int which = collector.GetRandomHidingSpot();
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::GOOD_SNIPER_SPOT );
			SearchSurroundingAreas( startArea, pos, collector, maxRange );

			if (collector.m_count)
			{
				int which = collector.GetRandomHidingSpot();
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::IN_COVER );
	SearchSurroundingAreas( startArea, pos, collector, maxRange );

	if (collector.m_count == 0)
	{
		// no hiding spots at all - if we're not a sniper, try to find a sniper spot to use instead
		if (!isSniper)
		{
			return FindNearbyHidingSpot( me, pos, maxRange, true, useNearest );
		}

		return nullptr;
	}

	if (useNearest)
	{
		// return closest hiding spot
		const Vector *closest = nullptr;
		float closeRangeSq = 9999999999.9f;
		for( int i=0; i<collector.m_count; ++i )
		{
			float rangeSq = (*collector.m_hidingSpot[i] - pos).LengthSqr();
			if (rangeSq < closeRangeSq)
			{
				closeRangeSq = rangeSq;
				closest = collector.m_hidingSpot[i];
			}
		}

		return closest;
	}

	// select a hiding spot at random
	int which = collector.GetRandomHidingSpot();
	return collector.m_hidingSpot[ which ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Select a random hiding spot among the nav areas that are tagged with the given place
 */
const Vector *FindRandomHidingSpot( CBaseEntity *me, Place place, bool isSniper )
{
	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::IDEAL_SNIPER_SPOT, place );
		TheNavMesh->ForAllAreas( collector );

		if (collector.m_count)
		{
			int which = RandomInt( 0, collector.m_count-1 );
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::GOOD_SNIPER_SPOT, place );
			TheNavMesh->ForAllAreas( collector );

			if (collector.m_count)
			{
				int which = RandomInt( 0, collector.m_count-1 );
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::IN_COVER, place );
	TheNavMesh->ForAllAreas( collector );

	if (collector.m_count == 0)
		return nullptr;

	// select a hiding spot at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}

//--------------------------------------------------------------------------------------------------------------------
/**
 * Select a nearby retreat spot.
 * Don't pick a hiding spot that a Player is currently occupying.
 * If "avoidTeam" is nonzero, avoid getting close to members of that team.
 */
const Vector *FindNearbyRetreatSpot( CBaseEntity *me, const Vector &start, float maxRange, int avoidTeam )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( start );
	if (startArea == nullptr)
		return nullptr;

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, start, maxRange, HidingSpot::IN_COVER );
	SearchSurroundingAreas( startArea, start, collector, maxRange );

	if (collector.m_count == 0)
		return nullptr;

	// find the closest unoccupied hiding spot that crosses the least lines of fire and has the best cover
	for( int i=0; i<collector.m_count; ++i )
	{
		// check if we would have to cross a line of fire to reach this hiding spot
		if (IsCrossingLineOfFire( start, *collector.m_hidingSpot[i], me ))
		{
			collector.RemoveSpot( i );

			// back up a step, so iteration won't skip a spot
			--i;

			continue;
		}

		// check if there is someone on the avoidTeam near this hiding spot
		if (avoidTeam)
		{
			float range;
			if (UTIL_GetClosestPlayer( *collector.m_hidingSpot[i], avoidTeam, &range ))
			{
				const float dangerRange = 150.0f;
				if (range < dangerRange)
				{
					// there is an avoidable player too near this spot - remove it
					collector.RemoveSpot( i );

					// back up a step, so iteration won't skip a spot
					--i;

					continue;
				}
			}
		}
	}

	if (collector.m_count <= 0)
		return nullptr;

	// all remaining spots are ok - pick one at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}

//--------------------------------------------------------------------------------------------------------------------
/**
 * Functor to collect all hiding spots in range that we can reach before the enemy arrives.
 * NOTE: This only works for the initial rush.
 */
class CollectArriveFirstSpotsFunctor
{
public:
	CollectArriveFirstSpotsFunctor( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float range, int flags ) : m_searchOrigin( searchOrigin )
	{
		m_me = me;
		m_count = 0;
		m_range = range;
		m_flags = (unsigned char)flags;
		m_enemyArriveTime = enemyArriveTime;
	}

	enum { MAX_SPOTS = 256 };

	bool operator() ( CNavArea *area )
	{
		const HidingSpotVector *pSpots = area->GetHidingSpots();
		
		for ( const HidingSpot *spot : *pSpots )
		{
			// make sure hiding spot is in range
			if (m_range > 0.0f)
			{
				if ((spot->GetPosition() - m_searchOrigin).IsLengthGreaterThan( m_range ))
				{
					continue;
				}
			}

			// if a Player is using this hiding spot, don't consider it
			if (IsSpotOccupied( m_me, spot->GetPosition() ))
			{
				// player is in hiding spot
				/// @todo Check if player is moving or sitting still
				continue;
			}

			// only collect hiding spots with matching flags
			if (!(m_flags & spot->GetFlags()))
			{
				continue;
			}

			// only collect this hiding spot if we can reach it before the enemy arrives
			// NOTE: This assumes the area is fairly small and the difference of moving to the corner vs the center is small
			const float settleTime = 1.0f;

			if(!spot->GetArea())
			{
				contract_assert( false && "Check console spew for Hiding Spot off the Nav Mesh errors." );
				continue;
			}

			if (spot->GetArea()->GetEarliestOccupyTime( m_me->GetTeamNumber() ) + settleTime < m_enemyArriveTime)
			{
				m_hidingSpot[ m_count++ ] = spot;
			}
		}

		// if we've filled up, stop searching
		if (m_count == MAX_SPOTS)
			return false;

		return true;
	}

	CBaseEntity *m_me;
	const Vector &m_searchOrigin;

	float m_range;
	float m_enemyArriveTime;
	unsigned char m_flags;

	const HidingSpot *m_hidingSpot[ MAX_SPOTS ];
	int m_count;
};

/**
 * Select a hiding spot that we can reach before the enemy arrives.
 * NOTE: This only works for the initial rush.
 */
const HidingSpot *FindInitialEncounterSpot( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float maxRange, bool isSniper )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( searchOrigin );
	if (startArea == nullptr)
		return nullptr;

	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::IDEAL_SNIPER_SPOT );
		SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

		if (collector.m_count)
		{
			int which = RandomInt( 0, collector.m_count-1 );
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::GOOD_SNIPER_SPOT );
			SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

			if (collector.m_count)
			{
				int which = RandomInt( 0, collector.m_count-1 );
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::IN_COVER | HidingSpot::EXPOSED );
	SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

	if (collector.m_count == 0)
		return nullptr;

	// select a hiding spot at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}

class CBaseGrenade;
class CBasePlayer;
class CBotManager;
class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

float g_BotUpkeepInterval = 0.0f;
float g_BotUpdateInterval = 0.0f;

//--------------------------------------------------------------------------------------------------------------
CBotManager::CBotManager()
{
	InitBotTrig();
}

//--------------------------------------------------------------------------------------------------------------
CBotManager::~CBotManager()
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when the round is restarting
 */
void CBotManager::RestartRoundBase( void )
{
	DestroyAllGrenades();
	ClearDebugMessages();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked at the start of each frame
 */
void CBotManager::StartFrameBase( void )
{
	CVProfScope _( "CBotManager::StartFrame", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	ValidateActiveGrenades();

	// debug smoke grenade visualization
	if (cv_bot_debug.GetInt() == 5)
	{
		Vector edge, lastEdge;

		for ( const auto &pGrenade : m_activeGrenadeList )
		{
			ActiveGrenade *ag = pGrenade.get();

			const Vector &pos = ag->GetDetonationPosition();

			UTIL_DrawBeamPoints( pos, pos + Vector( 0, 0, 50 ), 1, 255, 100, 0 );

			lastEdge = Vector( ag->GetRadius() + pos.x, pos.y, pos.z );
			float angle;
			for( angle=0.0f; angle <= 180.0f; angle += 22.5f )
			{
				edge.x = ag->GetRadius() * BotCOS( angle ) + pos.x;
				edge.y = pos.y;
				edge.z = ag->GetRadius() * BotSIN( angle ) + pos.z;

				UTIL_DrawBeamPoints( edge, lastEdge, 1, 255, 50, 0 );

				lastEdge = edge;
			}

			lastEdge = Vector( pos.x, ag->GetRadius() + pos.y, pos.z );
			for( angle=0.0f; angle <= 180.0f; angle += 22.5f )
			{
				edge.x = pos.x;
				edge.y = ag->GetRadius() * BotCOS( angle ) + pos.y;
				edge.z = ag->GetRadius() * BotSIN( angle ) + pos.z;

				UTIL_DrawBeamPoints( edge, lastEdge, 1, 255, 50, 0 );

				lastEdge = edge;
			}
		}
	}

	// set frame duration
	g_BotUpkeepInterval = m_frameTimer.GetElapsedTime();
	m_frameTimer.Start();

	g_BotUpdateInterval = (g_BotUpdateSkipCount+1) * g_BotUpkeepInterval;

	//
	// Process each active bot
	//
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!player)
			continue;

		// Hack for now so the temp bot code works. The temp bots are very useful for debugging
		// because they can be setup to mimic the player's usercmds.
		if (player->IsBot() && IsEntityValid( player ) )
		{
			CCSBot *bot = ToCSBot( player );

			if ( bot )
			{
				{
					bot->Upkeep();
				}

				if (((gpGlobals->tickcount + bot->entindex()) % g_BotUpdateSkipCount) == 0)
				{
					{
						bot->ResetCommand();
					}
					
					{
						bot->Update();
					}
				}

				{
					bot->UpdatePlayer();
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add an active grenade to the bot's awareness
 */
void CBotManager::AddGrenade( CBaseGrenade *grenade )
{
	m_activeGrenadeList.push_back( std::make_unique<ActiveGrenade>( grenade ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The grenade entity in the world is going away
 */
void CBotManager::RemoveGrenade( CBaseGrenade *grenade )
{
	for ( auto it = m_activeGrenadeList.begin(); it != m_activeGrenadeList.end(); ++it )
	{
		ActiveGrenade *ag = it->get();

		if (ag->IsEntity( grenade ))
		{
			ag->OnEntityGone();
			m_activeGrenadeList.erase( it );
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The grenade entity has changed its radius
 */
void CBotManager::SetGrenadeRadius( CBaseGrenade *grenade, float radius )
{
	for ( const auto &pGrenade : m_activeGrenadeList )
	{
		ActiveGrenade *ag = pGrenade.get();

		if (ag->IsEntity( grenade ))
		{
			ag->SetRadius( radius );
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Destroy any invalid active grenades
 */
void CBotManager::ValidateActiveGrenades( void )
{
	for ( auto it = m_activeGrenadeList.begin(); it != m_activeGrenadeList.end(); )
	{
		ActiveGrenade *ag = it->get();

		// lazy validation
		if (!ag->IsValid())
		{
			it = m_activeGrenadeList.erase( it );
			continue;
		}
		else
		{
			ag->Update();
		}
		++it;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CBotManager::DestroyAllGrenades( void )
{
	m_activeGrenadeList.clear();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if position is inside a smoke cloud
 */
bool CBotManager::IsInsideSmokeCloud( const Vector *pos, float radius )
{
	for ( auto it = m_activeGrenadeList.begin(); it != m_activeGrenadeList.end(); )
	{
		ActiveGrenade *ag = it->get();

		// lazy validation
		if (!ag->IsValid())
		{
			it = m_activeGrenadeList.erase( it );
			continue;
		}
		++it;

		if (ag->IsSmoke())
		{
			const Vector &smokeOrigin = ag->GetDetonationPosition();

			if ((smokeOrigin - *pos).IsLengthLessThan( ag->GetRadius() + radius ))
				return true;			
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if line intersects smoke volume
 * Determine the length of the line of sight covered by each smoke cloud, 
 * and sum them (overlap is additive for obstruction).
 * If the overlap exceeds the threshold, the bot can't see through.
 */
bool CBotManager::IsLineBlockedBySmoke( const Vector &from, const Vector &to, float grenadeBloat )
{
	CVProfScope _( "CBotManager::IsLineBlockedBySmoke", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	float totalSmokedLength = 0.0f;	// distance along line of sight covered by smoke

	// compute unit vector and length of line of sight segment
	//Vector sightDir = to - from;
	//float sightLength = sightDir.NormalizeInPlace();

	for ( const auto &pGrenade : m_activeGrenadeList )
	{
		ActiveGrenade *ag = pGrenade.get();
		const float smokeRadiusSq = ag->GetRadius() * ag->GetRadius() * grenadeBloat * grenadeBloat;

		if ( ag->IsSmoke() && CSGameRules() )
		{
			float flLengthAdd = CSGameRules()->CheckTotalSmokedLength( smokeRadiusSq, ag->GetDetonationPosition(), from, to );
			// get the totalSmokedLength and check to see if the line starts or stops in smoke.  If it does this will return -1 and we should just bail early
			if ( flLengthAdd == -1 )
				return true;

			totalSmokedLength += flLengthAdd;
		}
	}

	// define how much smoke a bot can see thru
	const float maxSmokedLength = 0.7f * SmokeGrenadeRadius;

	// return true if the total length of smoke-covered line-of-sight is too much
	return (totalSmokedLength > maxSmokedLength);
}

//--------------------------------------------------------------------------------------------------------------
void CBotManager::ClearDebugMessages( void )
{
	m_debugMessageCount = 0;
	m_currentDebugMessage = -1;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add a new debug message to the message history
 */
void CBotManager::AddDebugMessage( const char *msg )
{
	if (++m_currentDebugMessage >= MAX_DBG_MSGS)
	{
		m_currentDebugMessage = 0;
	}

	if (m_debugMessageCount < MAX_DBG_MSGS)
	{
		++m_debugMessageCount;
	}

	V_strncpy( m_debugMessage[ m_currentDebugMessage ].m_string, msg, MAX_DBG_MSG_SIZE );
	m_debugMessage[ m_currentDebugMessage ].m_age.Start();
}

int CBotManager::GetDebugMessageCount( void ) const
{
	return m_debugMessageCount;
}

const CBotManager::DebugMessage *CBotManager::GetDebugMessage( int which ) const
{
	if (which >= m_debugMessageCount)
		return nullptr;

	int i = m_currentDebugMessage - which;
	if (i < 0)
		i += MAX_DBG_MSGS;

	return &m_debugMessage[ i ];
}

class BotProfile;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#define DEFINE_DIFFICULTY_NAMES

BotProfileManager *TheBotProfiles = nullptr;

//--------------------------------------------------------------------------------------------------------
/**
 * Generates a filename-decorated skin name
 */
static const char * GetDecoratedSkinName( const char *name, const char *filename )
{
	const int BufLen = k_cMaxPath + 64;
	static char buf[BufLen];
	V_snprintf( buf, sizeof( buf ), "%s/%s", filename, name );
	return buf;
}

//--------------------------------------------------------------------------------------------------------------
const char* BotProfile::GetWeaponPreferenceAsString( int i ) const
{
	if ( i < 0 || i >= m_weaponPreferenceCount )
		return nullptr;

	return WeaponIDToAlias( m_weaponPreference[ i ] );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile has a primary weapon preference
 */
bool BotProfile::HasPrimaryPreference() const
{
	for ( int i=0; i < m_weaponPreferenceCount; ++i )
	{
		if ( IsPrimaryWeapon( m_weaponPreference[i] ) )
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile has a pistol weapon preference
 */
bool BotProfile::HasPistolPreference() const
{
	for ( int i=0; i < m_weaponPreferenceCount; ++i )
		if ( IsSecondaryWeapon( m_weaponPreference[i] ) )
			return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile is valid for the specified team
 */
bool BotProfile::IsValidForTeam( int team ) const
{
	return ( team == TEAM_UNASSIGNED || m_teams == TEAM_UNASSIGNED || team == m_teams );
}

//--------------------------------------------------------------------------------------------------------------
/**
* Return true if this profile inherits from the specified template
*/
bool BotProfile::InheritsFrom( const char *name ) const
{
	if ( WildcardMatch( name, GetName() ) )
		return true;

	for ( int i=0; i<std::ssize( m_templates ); ++i )
	{
		const BotProfile *queryTemplate = m_templates[i];
		if ( queryTemplate && queryTemplate->InheritsFrom( name ) )
		{
			return true;
		}
	}
	return false;
}

void BotProfile::SetName( const char* name )
{
	if ( name )
	{
		if ( m_name )
		{
			// Delete the current name if it exists
			delete [] m_name;
		}

		m_name = CloneString( name );
	}
}

const BotProfile* BotProfile::GetTemplate( int index ) const
{
	if ( index > 0 && index < std::ssize( m_templates ) )
	{
		return m_templates[index];
	}

	return nullptr;
}

void BotProfile::Clone( const BotProfile* source_profile )
{
	if ( source_profile )
	{
		SetName( source_profile->GetName() );

		for ( int device = 0; device < BotProfileInputDevice::COUNT; ++device )
		{
			m_aggression = source_profile->GetAggression();

			m_skill = source_profile->GetSkill();

			m_teamwork = source_profile->GetTeamwork();

			m_aimFocusInitial = source_profile->GetAimFocusInitial();
			m_aimFocusDecay = source_profile->GetAimFocusDecay();
			m_aimFocusOffsetScale = source_profile->GetAimFocusOffsetScale();
			m_aimFocusInterval = source_profile->GetAimFocusInterval();

			m_weaponPreferenceCount = source_profile->GetWeaponPreferenceCount();
			for( int i = 0; i < source_profile->GetWeaponPreferenceCount(); ++i )
			{
				m_weaponPreference[i] = source_profile->GetWeaponPreference(i);
			}

			m_cost = source_profile->GetCost();

			m_skin = source_profile->GetSkin();

			m_difficultyFlags = source_profile->GetDifficultyFlags();

			m_voicePitch = source_profile->GetVoicePitch();

			m_reactionTime = source_profile->GetReactionTime();

			m_attackDelay = source_profile->GetAttackDelay();

			m_lookAngleMaxAccelNormal = source_profile->GetLookAngleMaxAccelerationNormal();
			m_lookAngleStiffnessNormal = source_profile->GetLookAngleStiffnessNormal();
			m_lookAngleDampingNormal = source_profile->GetLookAngleDampingNormal();
		
			m_lookAngleMaxAccelAttacking = source_profile->GetLookAngleMaxAccelerationAttacking();
			m_lookAngleStiffnessAttacking = source_profile->GetLookAngleStiffnessAttacking();
			m_lookAngleDampingAttacking = source_profile->GetLookAngleDampingAttacking();
		}

		m_teams = source_profile->GetTeams();

		m_voiceBank = source_profile->GetVoiceBank();

		m_prefersSilencer = source_profile->PrefersSilencer();

		for ( int i = 0; i < source_profile->GetTemplatesCount(); ++i )
		{
			m_templates.push_back( source_profile->GetTemplate( i ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor
 */
BotProfileManager::BotProfileManager( void )
{
	m_nextSkin = 0;
	for (int i=0; i<NumCustomSkins; ++i)
	{
		m_skins[i] = nullptr;
		m_skinFilenames[i] = nullptr;
		m_skinModelnames[i] = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load the bot profile database
 */
void BotProfileManager::Init( const char *filename, unsigned int *checksum )
{
	FileHandle_t file = filesystem->Open( filename, "r" );

	if (!file)
	{
        Msg( "WARNING: Cannot access bot profile database '%s'\n", filename );
		return;
	}

	int dataLength = filesystem->Size( filename );
	char *dataPointer = new char[ dataLength ];
	int dataReadLength = filesystem->Read( dataPointer, dataLength, file );
	filesystem->Close( file );
	if ( dataReadLength > 0 )
	{
		// NULL-terminate based on the length read in, since Read() can transform \r\n to \n and
		// return fewer bytes than we were expecting.
		dataPointer[ dataReadLength - 1 ] = 0;
	}

	const char *dataFile = dataPointer;

	// compute simple checksum
	if (checksum)
	{
		*checksum = 0; // ComputeSimpleChecksum( (const unsigned char *)dataPointer, dataLength );
	}

	BotProfile defaultProfile;

	//
	// Parse the BotProfile.db into BotProfile instances
	//
	while( true )
	{
		dataFile = SharedParse( dataFile );
		if (!dataFile)
			break;

		char *token = SharedGetToken();

		bool isDefault = ( !stricmp( token, "Default" ));
		bool isTemplate = ( !stricmp( token, "Template" ));
		bool isCustomSkin = ( !stricmp( token, "Skin" ));

		if ( isCustomSkin )
		{
			const int BufLen = 64;
			char skinName[BufLen];

			// get skin name
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
                Msg( "Error parsing %s - expected skin name\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			V_snprintf( skinName, sizeof( skinName ), "%s", token );

			// get attribute name
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
                Msg( "Error parsing %s - expected 'Model'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (stricmp( "Model", token ))
			{
                Msg( "Error parsing %s - expected 'Model'\n", filename );
				delete [] dataPointer;
				return;
			}

			// eat '='
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
                Msg( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (std::strcmp( "=", token ))
			{
                Msg( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			// get attribute value
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing %s - expected attribute value\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			const char *decoratedName = GetDecoratedSkinName( skinName, filename );
			bool skinExists = GetCustomSkinIndex( decoratedName ) > 0;
			if ( m_nextSkin < NumCustomSkins && !skinExists )
			{
				// decorate the name
				m_skins[ m_nextSkin ] = CloneString( decoratedName );

				// construct the model filename
				m_skinModelnames[ m_nextSkin ] = CloneString( token );
				m_skinFilenames[ m_nextSkin ] = new char[ std::strlen( token )*2 + strlen("models/player//.mdl") + 1 ];
				V_snprintf( m_skinFilenames[ m_nextSkin ], sizeof( m_skinFilenames[ m_nextSkin ] ), "models/player/%s/%s.mdl", token, token );
				++m_nextSkin;
			}

			// eat 'End'
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (std::strcmp( "End", token ))
			{
				Msg( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}

			continue; // it's just a custom skin - no need to do inheritance on a bot profile, etc.
		}

		// encountered a new profile
		BotProfile *profile;

		if (isDefault)
		{
			profile = &defaultProfile;
		}
		else
		{
			profile = new BotProfile;

			// always inherit from Default
			*profile = defaultProfile;
		}

		// do inheritance in order of appearance
		if (!isTemplate && !isDefault)
		{
			const BotProfile *inherit = nullptr;

			// template names are separated by "+"
			while(true)
			{
				char *c = std::strchr( token, '+' );
				if (c)
					*c = '\000';

				// find the given template name
				for ( const auto &pTemplate : m_templateList )
				{
					const BotProfile *profile = pTemplate.get();
					if ( !stricmp( profile->GetName(), token ))
					{
						inherit = profile;
						break;
					}
				}

				if (inherit == nullptr)
				{
					Msg( "Error parsing '%s' - invalid template reference '%s'\n", filename, token );
					delete [] dataPointer;
					return;
				}

				// inherit the data
				profile->Inherit( inherit, &defaultProfile );

				if (c == nullptr)
					break;
				
				token = c+1;
			}
		}

		// get name of this profile
		if (!isDefault)
		{
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing '%s' - expected name\n", filename );
				delete [] dataPointer;
				return;
			}
			profile->m_name = CloneString( SharedGetToken() );

			/**
			 * HACK HACK
			 * Until we have a generalized means of storing bot preferences, we're going to hardcode the bot's
			 * preference towards silencers based on his name.
			 */
			if ( profile->m_name[0] % 2 )
			{
				profile->m_prefersSilencer = true;
			}
		}

		// read attributes for this profile
		bool isFirstWeaponPref = true;
		while( true )
		{
			// get next token
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			// check for End delimiter
			if ( !stricmp( token, "End" ))
				break;

			// found attribute name - keep it
			char attributeName[64];
			std::strcpy( attributeName, token );

			// eat '='
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			token = SharedGetToken();
			if (std::strcmp( "=", token ))
			{
				Msg( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			// get attribute value
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				Msg( "Error parsing %s - expected attribute value\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			// store value in appropriate attribute
			if ( !stricmp( "Aggression", attributeName ) )
			{
				profile->m_aggression = (float)std::atof( token ) / 100.0f;
			}
			else if ( !stricmp( "Skill", attributeName ) )
			{
				profile->m_skill = (float)std::atof( token ) / 100.0f;
			}
			else if ( !stricmp( "Skin", attributeName ) )
			{
				profile->m_skin = std::atoi( token );
				if ( profile->m_skin == 0 )
				{
					// atoi() failed - try to look up a custom skin by name
					profile->m_skin = GetCustomSkinIndex( token, filename );
				}
			}
			else if ( !stricmp( "Teamwork", attributeName ))
			{
				profile->m_teamwork = (float)std::atof( token ) / 100.0f;
			}
			else if ( !V_stricmp( "AimFocusInitial", attributeName ) )
			{
				profile->m_aimFocusInitial = (float)std::atof( token );
			}
			else if ( !V_stricmp( "AimFocusDecay", attributeName ) )
			{
				profile->m_aimFocusDecay = (float)std::atof( token );
			}
			else if ( !V_stricmp( "AimFocusOffsetScale", attributeName ) )
			{
				profile->m_aimFocusOffsetScale = (float)std::atof( token );
			}
			else if ( !V_stricmp( "AimFocusInterval", attributeName ) )
			{
				profile->m_aimFocusInterval = (float)std::atof( token );
			}
			else if ( !stricmp( "Cost", attributeName ) )
			{
				profile->m_cost = std::atoi( token );
			}
			else if ( !stricmp( "Rank", attributeName ) )
			{
				profile->m_rank = std::atoi( token );
			}
			else if ( !stricmp( "VoicePitch", attributeName ) )
			{
				profile->m_voicePitch = std::atoi( token );
			}
			else if ( !stricmp( "VoiceBank", attributeName ) )
			{
				profile->m_voiceBank = FindVoiceBankIndex( token );
			}
			else if ( !stricmp( "WeaponPreference", attributeName ) )
			{
				ParseWeaponPreference( isFirstWeaponPref, profile->m_weaponPreferenceCount, profile->m_weaponPreference, token );
			}
			else if ( !stricmp( "ReactionTime", attributeName ) )
			{
				profile->m_reactionTime = (float)std::atof( token );

			}
			else if ( !stricmp( "AttackDelay", attributeName ))
			{
				profile->m_attackDelay = (float)std::atof( token );
			}
			else if ( !stricmp( "Difficulty", attributeName ))
			{
				// override inheritance
				ParseDifficultySetting( profile->m_difficultyFlags, token );
			}
			else if ( !stricmp( "Team", attributeName ))
			{
				if ( !stricmp( token, "T" ) )
				{
					profile->m_teams = TEAM_TERRORIST;
				}
				else if ( !stricmp( token, "CT" ) )
				{
					profile->m_teams = TEAM_CT;
				}
				else
				{
					profile->m_teams = TEAM_UNASSIGNED;
				}
			}
			else if ( !stricmp( "LookAngleMaxAccelNormal", attributeName ) )
			{
				profile->m_lookAngleMaxAccelNormal = std::atof( token );
			}
			else if ( !stricmp( "LookAngleStiffnessNormal", attributeName ) )
			{
				profile->m_lookAngleStiffnessNormal = std::atof( token );
			}
			else if ( !stricmp( "LookAngleDampingNormal", attributeName ) )
			{
				profile->m_lookAngleDampingNormal = std::atof( token );
			}
			else if ( !stricmp( "LookAngleMaxAccelAttacking", attributeName ) )
			{
				profile->m_lookAngleMaxAccelAttacking = std::atof( token );
			}
			else if ( !stricmp( "LookAngleStiffnessAttacking", attributeName ) )
			{
				profile->m_lookAngleStiffnessAttacking = std::atof( token );
			}
			else if ( !stricmp( "LookAngleDampingAttacking", attributeName ) )
			{
				profile->m_lookAngleDampingAttacking = std::atof( token );
			}
			else
			{
				Msg( "Error parsing %s - unknown attribute '%s'\n", filename, attributeName );
			}
		}

		if (!isDefault)
		{
			if (isTemplate)
			{
				// add to template list
				m_templateList.emplace_back( profile );
			}
			else
			{
				// add profile to the master list
				m_profileList.emplace_back( profile );
			}
		}
	}

	delete [] dataPointer;
}

void BotProfileManager::ParseDifficultySetting( unsigned char &difficultyFlags, char* token)
{
	// override inheritance
	difficultyFlags = 0;

	// parse bit flags
	while(true)
	{
		char *c = std::strchr( token, '+' );
		if (c)
		{
			*c = '\000';
		}

		for( int i=0; i<NUM_DIFFICULTY_LEVELS; ++i )
		{
			if ( !stricmp( BotDifficultyName[i], token ))
			{
				difficultyFlags |= (1 << i);
			}
		}

		if (c == nullptr)
		{
			break;
		}
					
		token = c+1;
	}
}

void BotProfileManager::ParseWeaponPreference( bool &isFirstWeaponPref, int &weaponPreferenceCount, CSWeaponID* weaponPreference, char* token )
{
	// weapon preferences override parent prefs
	if (isFirstWeaponPref)
	{
		isFirstWeaponPref = false;
		weaponPreferenceCount = 0;
	}

	if ( !stricmp( token, "none" ))
	{
		weaponPreferenceCount = 0;
	}
	else
	{
		if (weaponPreferenceCount < BotProfile::MAX_WEAPON_PREFS)
		{
			weaponPreference[ weaponPreferenceCount++ ] = AliasToWeaponID( token );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
BotProfileManager::~BotProfileManager( void )
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Free all bot profiles
 */
void BotProfileManager::Reset( void )
{
	m_profileList.clear();
	m_templateList.clear();

	int i;

	for (i=0; i<NumCustomSkins; ++i)
	{
		if ( m_skins[i] )
		{
			delete[] m_skins[i];
			m_skins[i] = nullptr;
		}
		if ( m_skinFilenames[i] )
		{
			delete[] m_skinFilenames[i];
			m_skinFilenames[i] = nullptr;
		}
		if ( m_skinModelnames[i] )
		{
			delete[] m_skinModelnames[i];
			m_skinModelnames[i] = nullptr;
		}
	}

	for ( i=0; i<std::ssize( m_voiceBanks ); ++i )
	{
		delete[] m_voiceBanks[i];
	}
	m_voiceBanks.clear();
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin name at a particular index
 */
const char * BotProfileManager::GetCustomSkin( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return nullptr;
	}

	return m_skins[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin filename at a particular index
 */
const char * BotProfileManager::GetCustomSkinFname( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return nullptr;
	}

	return m_skinFilenames[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin modelname at a particular index
 */
const char * BotProfileManager::GetCustomSkinModelname( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return nullptr;
	}

	return m_skinModelnames[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Looks up a custom skin index by filename-decorated name (will decorate the name if filename is given)
 */
int BotProfileManager::GetCustomSkinIndex( const char *name, const char *filename )
{
	const char * skinName = name;
	if ( filename )
	{
		skinName = GetDecoratedSkinName( name, filename );
	}

	for (int i=0; i<NumCustomSkins; ++i)
	{
		if ( m_skins[i] )
		{
			if ( !stricmp( skinName, m_skins[i] ) )
			{
				return FirstCustomSkin + i;
			}
		}
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------------
/**
 * return index of the (custom) bot phrase db, inserting it if needed
 */
int BotProfileManager::FindVoiceBankIndex( const char *filename )
{
	int index = 0;

	for ( int i=0; i<std::ssize( m_voiceBanks ); ++i )
	{
		if ( !stricmp( filename, m_voiceBanks[i] ) )
		{
			return index;
		}
	}

	m_voiceBanks.push_back( CloneString( filename ) );
	return index;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return random unused profile that matches the given difficulty level
 */
const BotProfile *BotProfileManager::GetRandomProfile( BotDifficultyType difficulty, int team, CSWeaponType weaponType, bool forceMatchHighestDifficulty ) const
{
	// count up valid profiles
	std::vector< const BotProfile * > profiles;
	for ( const auto &pProfile : m_profileList )
	{
		const BotProfile *profile = pProfile.get();

		// Match difficulty
		if ( forceMatchHighestDifficulty )
		{
			if ( !profile->IsMaxDifficulty( difficulty ) )
			{
				continue;
			}
		}
		else
		{
			if ( !profile->IsDifficulty( difficulty ) )
				continue;
		}

		// Prevent duplicate names
		if ( UTIL_IsNameTaken( profile->GetName() ) )
			continue;

		// Match team choice
		if ( !profile->IsValidForTeam( team ) )
			continue;

		// Match desired weapon
		if ( weaponType != WEAPONTYPE_UNKNOWN )
		{
			if ( !profile->GetWeaponPreferenceCount() )
				continue;

			if ( weaponType != WeaponClassFromWeaponID( (CSWeaponID)profile->GetWeaponPreference( 0 ) ) )
				continue;
		}

		profiles.push_back( profile );
	}

	if ( profiles.empty() )
		return nullptr;

	// select one at random
	int which = RandomInt( 0, std::ssize( profiles )-1 );
	return profiles[which];
}

int BotProfile::GetMaxDifficulty() const
{
	for ( int i = NUM_DIFFICULTY_LEVELS - 1; i >= BOT_EASY; --i )
	{
		if ( m_difficultyFlags & ( 1 << i ) )
		{
			return i;
		}
	}

	return BOT_EASY;
}

bool BotProfile::IsDifficulty( BotDifficultyType diff ) const
{
	return (m_difficultyFlags & (1 << diff)) ? true : false;
}

bool BotProfile::IsMaxDifficulty( BotDifficultyType diff ) const
{
	return ( ( m_difficultyFlags >> diff ) == 1 );
}

float BotProfile::GetAggression() const
{
	return m_aggression;
}

float BotProfile::GetSkill() const
{
	return m_skill;
}

float BotProfile::GetTeamwork() const
{
	return m_teamwork;
}

CSWeaponID BotProfile::GetWeaponPreference( int i ) const
{
	return m_weaponPreference[ i ];
}

int BotProfile::GetWeaponPreferenceCount() const
{
	return m_weaponPreferenceCount;
}

int BotProfile::GetCost() const
{
	return m_cost;
}

int BotProfile::GetSkin() const
{
	return m_skin;
}

int BotProfile::GetRank() const
{
	return m_rank;
}

int BotProfile::GetVoicePitch() const
{
	return m_voicePitch;
}

float BotProfile::GetReactionTime() const
{
	return m_reactionTime;
}

float BotProfile::GetAttackDelay() const
{
	return m_attackDelay;
}

unsigned char BotProfile::GetDifficultyFlags() const
{
	return m_difficultyFlags;
}

float BotProfile::GetLookAngleMaxAccelerationNormal() const
{
	return m_lookAngleMaxAccelNormal;
}

float BotProfile::GetLookAngleStiffnessNormal( ) const
{
	return m_lookAngleStiffnessNormal;
}

float BotProfile::GetLookAngleDampingNormal() const
{
	return m_lookAngleDampingNormal;
}

float BotProfile::GetLookAngleMaxAccelerationAttacking() const
{
	return m_lookAngleMaxAccelAttacking;
}

float BotProfile::GetLookAngleStiffnessAttacking() const
{
	return m_lookAngleStiffnessAttacking;
}

float BotProfile::GetLookAngleDampingAttacking() const
{
	return m_lookAngleDampingAttacking;
}

void BotProfile::SetVoicePitch( int pitch )
{
	m_voicePitch = pitch;
}

void BotProfile::Inherit( const BotProfile *parent, const BotProfile *baseline )
{
	if ( parent->m_aggression != baseline->m_aggression )
		m_aggression = parent->m_aggression;

	if ( parent->m_skill != baseline->m_skill )
		m_skill = parent->m_skill;

	if ( parent->m_teamwork != baseline->m_teamwork )
		m_teamwork = parent->m_teamwork;

	if ( parent->m_aimFocusInitial != baseline->m_aimFocusInitial )
		m_aimFocusInitial = parent->m_aimFocusInitial;

	if ( parent->m_aimFocusDecay != baseline->m_aimFocusDecay )
		m_aimFocusDecay = parent->m_aimFocusDecay;

	if ( parent->m_aimFocusOffsetScale != baseline->m_aimFocusOffsetScale )
		m_aimFocusOffsetScale = parent->m_aimFocusOffsetScale;

	if ( parent->m_aimFocusInterval != baseline->m_aimFocusInterval )
		m_aimFocusInterval = parent->m_aimFocusInterval;

	if (parent->m_weaponPreferenceCount != baseline->m_weaponPreferenceCount)
	{
		m_weaponPreferenceCount = parent->m_weaponPreferenceCount;
		for( int i=0; i<parent->m_weaponPreferenceCount; ++i )
			m_weaponPreference[i] = parent->m_weaponPreference[i];
	}

	if (parent->m_cost != baseline->m_cost)
		m_cost = parent->m_cost;

	if (parent->m_skin != baseline->m_skin)
		m_skin = parent->m_skin;

	if (parent->m_rank != baseline->m_rank)
		m_rank = parent->m_rank;

	if (parent->m_difficultyFlags != baseline->m_difficultyFlags)
		m_difficultyFlags = parent->m_difficultyFlags;

	if (parent->m_voicePitch != baseline->m_voicePitch)
		m_voicePitch = parent->m_voicePitch;

	if (parent->m_reactionTime != baseline->m_reactionTime)
		m_reactionTime = parent->m_reactionTime;

	if (parent->m_attackDelay != baseline->m_attackDelay)
		m_attackDelay = parent->m_attackDelay;

	if (parent->m_teams != baseline->m_teams)
		m_teams = parent->m_teams;

	if (parent->m_voiceBank != baseline->m_voiceBank)
		m_voiceBank = parent->m_voiceBank;

	m_templates.push_back( parent );
}

BotProfile::BotProfile( void)
{
	m_name = nullptr;

	m_aggression = 0.0f;
	m_skill = 0.0f;
	m_teamwork = 0.0f;
	m_weaponPreferenceCount = 0;

	m_aimFocusInitial = 0.0f;
	m_aimFocusDecay = 1.0f;
	m_aimFocusOffsetScale = 0.0f;
	m_aimFocusInterval = .5f;

	m_cost = 0;
	m_skin = 0;	
	m_rank = 0;
	m_difficultyFlags = 0;
	m_voicePitch = 100;
	m_reactionTime = 0.3f;
	m_attackDelay = 0.0f;
	m_lookAngleMaxAccelNormal = 0.0f;
	m_lookAngleStiffnessNormal = 0.0f;
	m_lookAngleDampingNormal = 0.0f;
	m_lookAngleMaxAccelAttacking = 0.0f;
	m_lookAngleStiffnessAttacking = 0.0f;
	m_lookAngleDampingAttacking = 0.0f;

	m_teams = TEAM_UNASSIGNED;
	m_voiceBank = 0;
	m_prefersSilencer = false;

}

BotProfile::~BotProfile( void)
{
	if ( m_name )
		delete [] m_name;
}

#undef DEFINE_DIFFICULTY_NAMES

class BotProfile;
class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSWeaponInfo;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

static int s_iBeamSprite = 0;

extern ConVar mp_randomspawn_dist;
extern ConVar mp_randomspawn_los;

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given name is already in use by another player
 */
bool UTIL_IsNameTaken( const char *name, bool ignoreHumans )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (player->IsPlayer() && player->IsBot())
		{
			// bots can have prefixes so we need to check the name
			// against the profile name instead.
			CCSBot *bot = ToCSBot( player );
			if ( bot && bot->GetProfile()->GetName() && FStrEq(name, bot->GetProfile()->GetName()))
			{
				return true;
			}
		}
		else
		{
			if (!ignoreHumans)
			{
				if (FStrEq( name, player->GetPlayerName() ))
					return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
int UTIL_ClientsInGame( void )
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if (player == nullptr)
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the number of non-bots on the given team
 */
int UTIL_HumansOnTeam( int teamID, bool isAlive )
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );

		if ( entity == nullptr )
			continue;

		CBasePlayer *player = static_cast<CBasePlayer *>( entity );

		if (player->IsBot())
			continue;

		if (player->GetTeamNumber() != teamID)
			continue;

		if (isAlive && !player->IsAlive())
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
int UTIL_BotsInGame( void )
{
	int count = 0;

	for (int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));

		if ( player == nullptr )
			continue;

		if ( !player->IsBot() )
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Kick a bot from the given team. If no bot exists on the team, return false.
 */
bool UTIL_KickBotFromTeam( int kickTeam, bool bQueue )
{
	int i;

	// try to kick a dead bot first
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsBot())
			continue;

		if ( player->GetPendingTeamNumber() == TEAM_SPECTATOR )
		{
			// bot has already been flagged for kicking
			continue;
		}

		if ( !player->IsAlive() )
		{
			// Address issue with bots getting kicked during half-time (this was resulting in bots from the wrong team being kicked)
			if ( player->CanKickFromTeam( kickTeam ) )
			{
				if ( bQueue )
				{
					// bots flagged as spectators will be kicked at the beginning of the next round restart
					player->SetPendingTeamNum( TEAM_SPECTATOR );
				}
				else
				{
					// its a bot on the right team - kick it
					engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( player->edict() ) ) );
				}
			
				return true;
			}
		}
	}

	// no dead bots, kick any bot on the given team
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsBot())
			continue;

		if ( player->GetPendingTeamNumber() == TEAM_SPECTATOR )
		{
			// bot has already been flagged for kicking
			continue;
		}

		// Address issue with bots getting kicked during half-time (this was resulting in bots from the wrong team being kicked)
		if ( player->CanKickFromTeam( kickTeam ) )
		{
			if ( bQueue )
			{
				// bots flagged as spectators will be kicked at the beginning of the next round restart
				player->SetPendingTeamNum( TEAM_SPECTATOR );
			}
			else
			{
				// its a bot on the right team - kick it
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( player->edict() ) ) );
			}

			return true;

		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if all of the members of the given team are bots
 */
bool UTIL_IsTeamAllBots( int team )
{
	int botCount = 0;

	for( int i=1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		// skip players on other teams
		if (player->GetTeamNumber() != team)
			continue;

		// if not a bot, fail the test
		if (!player->IsBot())
			return false;

		// is a bot on given team
		++botCount;
	}

	// if team is empty, there are no bots
	return (botCount) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest active player to the given position.
 * If 'distance' is non-NULL, the distance to the closest player is returned in it.
 */
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, float *distance )
{
	CBasePlayer *closePlayer = nullptr;
	float closeDistSq = 999999999999.9f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!IsEntityValid( player ))
			continue;

		if (!player->IsAlive())
			continue;

		Vector playerOrigin = GetCentroid( player );
		float distSq = (playerOrigin - pos).LengthSqr();
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			closePlayer = static_cast<CBasePlayer *>( player );
		}
	}
	
	if (distance)
		*distance = (float)std::sqrt( closeDistSq );

	return closePlayer;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest active player on the given team to the given position.
 * If 'distance' is non-NULL, the distance to the closest player is returned in it.
 */
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, int team, float *distance )
{
	CBasePlayer *closePlayer = nullptr;
	float closeDistSq = 999999999999.9f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!IsEntityValid( player ))
			continue;

		if (!player->IsAlive())
			continue;

		if (player->GetTeamNumber() != team)
			continue;

		Vector playerOrigin = GetCentroid( player );
		float distSq = (playerOrigin - pos).LengthSqr();
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			closePlayer = static_cast<CBasePlayer *>( player );
		}
	}
	
	if (distance)
		*distance = (float)std::sqrt( closeDistSq );

	return closePlayer;
}

//--------------------------------------------------------------------------------------------------------------
// Takes the bot pointer and constructs the net name using the current bot name prefix.
void UTIL_ConstructBotNetName( char *name, int nameLength, const BotProfile *profile )
{
	if (profile == nullptr)
	{
		name[0] = 0;
		return;
	}

	// if there is no bot prefix just use the profile name.
	if ((cv_bot_prefix.GetString() == nullptr) || (std::strlen(cv_bot_prefix.GetString()) == 0))
	{
		V_strncpy( name, profile->GetName(), nameLength );
		return;
	}

	// find the highest difficulty
	const char *diffStr = BotDifficultyName[0];
	for ( int i=BOT_EXPERT; i>0; --i )
	{
		if ( profile->IsDifficulty( (BotDifficultyType)i ) )
		{
			diffStr = BotDifficultyName[i];
			break;
		}
	}

	const char *weaponStr = nullptr;
	if ( profile->GetWeaponPreferenceCount() )
	{
		weaponStr = profile->GetWeaponPreferenceAsString( 0 );

		const char *translatedAlias = GetTranslatedWeaponAlias( weaponStr );

		char wpnName[128];
		V_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", translatedAlias );
		WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );
		if ( hWpnInfo != GetInvalidWeaponInfoHandle() )
		{
			CCSWeaponInfo *pWeaponInfo = static_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
			if ( pWeaponInfo )
			{
				CSWeaponType weaponType = pWeaponInfo->GetWeaponType();
				weaponStr = WeaponClassAsString( weaponType );
			}
		}
	}
	if ( !weaponStr )
	{
		weaponStr = "";
	}

	char skillStr[16];
	V_snprintf( skillStr, sizeof( skillStr ), "%.0f", profile->GetSkill()*100 );

	char temp[MAX_PLAYER_NAME_LENGTH*2];
	char prefix[MAX_PLAYER_NAME_LENGTH*2];
	V_strncpy( temp, cv_bot_prefix.GetString(), sizeof( temp ) );
	V_StrSubst( temp, "<difficulty>", diffStr, prefix, sizeof( prefix ) );
	V_StrSubst( prefix, "<weaponclass>", weaponStr, temp, sizeof( temp ) );
	V_StrSubst( temp, "<skill>", skillStr, prefix, sizeof( prefix ) );
	V_snprintf( name, nameLength, "%s %s", prefix, profile->GetName() );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if anyone on the given team can see the given spot
 */
bool UTIL_IsVisibleToTeam( const Vector &spot, int team )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsAlive())
			continue;

		if (player->GetTeamNumber() != team)
			continue;

		trace_t result;
		UTIL_TraceLine( player->EyePosition(), spot, CONTENTS_SOLID, player, COLLISION_GROUP_NONE, &result );

		if ( result.fraction == 1.0f )
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if anyone on the given team can see the given spot
 */
bool UTIL_IsRandomSpawnFarEnoughAwayFromTeam( const Vector &spot, int team )
{
	if ( mp_randomspawn_dist.GetInt() <= 0 )
		return true;

	if ( mp_randomspawn_los.GetInt() == 0 )
		return true;

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsAlive())
			continue;

		if (player->GetTeamNumber() != team)
			continue;

		if ( mp_randomspawn_dist.GetInt() > 0 && (player->GetAbsOrigin()).DistTo( spot ) < mp_randomspawn_dist.GetInt() )
		{
			//NDebugOverlay::Line( player->EyePosition(), spot + Vector( 0, 0, 32 ), 255, 0, 0, true, 4 );
			//NDebugOverlay::Line( spot, spot + Vector( 0, 0, 64 ), 255, 128, 0, true, 4 );
			continue;
		}
		else
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------
void UTIL_DrawBeamFromEnt( int i, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue )
{
/* BOTPORT: What is the replacement for MESSAGE_BEGIN?
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecEnd );   // vecEnd = origin???
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( i );
					WRITE_COORD( vecEnd.x );
					WRITE_COORD( vecEnd.y );
					WRITE_COORD( vecEnd.z );
					WRITE_SHORT( s_iBeamSprite );
					WRITE_BYTE( 0 );		 // startframe
					WRITE_BYTE( 0 );		 // framerate
					WRITE_BYTE( iLifetime ); // life
					WRITE_BYTE( 10 );		 // width
					WRITE_BYTE( 0 );		 // noise
					WRITE_BYTE( bRed );		 // r, g, b
					WRITE_BYTE( bGreen );		 // r, g, b
					WRITE_BYTE( bBlue );    // r, g, b
					WRITE_BYTE( 255 );	 // brightness
					WRITE_BYTE( 0 );		 // speed
					MESSAGE_END();
					*/
}

//------------------------------------------------------------------------------------------------------------
void UTIL_DrawBeamPoints( Vector vecStart, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue )
{
	NDebugOverlay::Line( vecStart, vecEnd, bRed, bGreen, bBlue, true, 0.1f );

	/*
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecStart );
					WRITE_BYTE( TE_BEAMPOINTS );
					WRITE_COORD( vecStart.x );
					WRITE_COORD( vecStart.y );
					WRITE_COORD( vecStart.z );
					WRITE_COORD( vecEnd.x );
					WRITE_COORD( vecEnd.y );
					WRITE_COORD( vecEnd.z );
					WRITE_SHORT( s_iBeamSprite );
					WRITE_BYTE( 0 );		 // startframe
					WRITE_BYTE( 0 );		 // framerate
					WRITE_BYTE( iLifetime ); // life
					WRITE_BYTE( 10 );		 // width
					WRITE_BYTE( 0 );		 // noise
					WRITE_BYTE( bRed );		 // r, g, b
					WRITE_BYTE( bGreen );		 // r, g, b
					WRITE_BYTE( bBlue );    // r, g, b
					WRITE_BYTE( 255 );	 // brightness
					WRITE_BYTE( 0 );		 // speed
					MESSAGE_END();
					*/
}

//------------------------------------------------------------------------------------------------------------
void BotPrecache( void )
{
	s_iBeamSprite = CBaseEntity::PrecacheModel( "sprites/smoke.vmt" );
}

//------------------------------------------------------------------------------------------------------------
static constexpr auto COS_TABLE_SIZE = 256;
static float cosTable[ COS_TABLE_SIZE ];

void InitBotTrig( void )
{
	for( int i=0; i<COS_TABLE_SIZE; ++i )
	{
		float angle = (float)(2.0f * std::numbers::pi * i / (float)(COS_TABLE_SIZE-1));
		cosTable[i] = (float)std::cos( angle ); 
	}
}

float BotCOS( float angle )
{
	angle = AngleNormalizePositive( angle );
	int i = (int)( angle * (COS_TABLE_SIZE-1) / 360.0f );
	return cosTable[i];
}

float BotSIN( float angle )
{
	angle = AngleNormalizePositive( angle - 90 );
	int i = (int)( angle * (COS_TABLE_SIZE-1) / 360.0f );
	return cosTable[i];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Send a "hint" message to all players, dead or alive.
 */
void HintMessageToAllPlayers( const char *message )
{
	hudtextparms_t textParms;

	textParms.x = -1.0f;
	textParms.y = -1.0f;
	textParms.fadeinTime = 1.0f;
	textParms.fadeoutTime = 5.0f;
	textParms.holdTime = 5.0f;
	textParms.fxTime = 0.0f;
	textParms.r1 = 100;
	textParms.g1 = 255;
	textParms.b1 = 100;
	textParms.r2 = 255;
	textParms.g2 = 255;
	textParms.b2 = 255;
	textParms.effect = 0;
	textParms.channel = 0;

	UTIL_HudMessageAll( textParms, message );
}

//--------------------------------------------------------------------------------------------------------------------
/**
 * Return true if moving from "start" to "finish" will cross a player's line of fire.
 * The path from "start" to "finish" is assumed to be a straight line.
 * "start" and "finish" are assumed to be points on the ground.
 */
bool IsCrossingLineOfFire( const Vector &start, const Vector &finish, CBaseEntity *ignore, int ignoreTeam  )
{
	for ( int p=1; p <= gpGlobals->maxClients; ++p )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( p ) );

		if (!IsEntityValid( player ))
			continue;

		if (player == ignore)
			continue;

		if (!player->IsAlive())
			continue;

		if (ignoreTeam && player->GetTeamNumber() == ignoreTeam)
			continue;

		// compute player's unit aiming vector 
		Vector viewForward;
		AngleVectors( player->GetFinalAimAngle(), &viewForward );

		const float longRange = 5000.0f;
		Vector playerOrigin = GetCentroid( player );
		Vector playerTarget = playerOrigin + longRange * viewForward;

		Vector result( 0, 0, 0 );
		if (IsIntersecting2D( start, finish, playerOrigin, playerTarget, &result ))
		{
			// simple check to see if intersection lies in the Z range of the path
			float loZ, hiZ;

			if (start.z < finish.z)
			{
				loZ = start.z;
				hiZ = finish.z;
			}
			else
			{
				loZ = finish.z;
				hiZ = start.z;
			}

			if (result.z >= loZ && result.z <= hiZ + HumanHeight)
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
* Performs a simple case-insensitive string comparison, honoring trailing * wildcards
*/
bool WildcardMatch( const char *query, const char *test )
{
	if ( !query || !test )
		return false;

	while ( *test && *query )
	{
		char nameChar = *test;
		char queryChar = *query;
		if ( std::tolower(nameChar) != tolower(queryChar) ) // case-insensitive
			break;
		++test;
		++query;
	}

	if ( *query == 0 && *test == 0 )
		return true;

	// Support trailing *
	if ( *query == '*' )
		return true;

	return false;
}
