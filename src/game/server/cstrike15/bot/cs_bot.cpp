// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "networkvar.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../../ai_criteria.h"
#include "../../entity_dispatch.h"
#include "../../entity_dispatch_list.h"
#include "../../entity_kinds_dump.h"
#include "../../util.h"
#include "declare_class.h"
#include "edict.h"
#include "igameevents.h"
#include "tier2/tier2_logging.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.random;
import ks.core.wchartypes;
import ks.game.in_buttons;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.generichash;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.vprof;
import ks.core.dbg;
import ks.core.platform;
import ks.tier1.convar;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import :cstrike15.cs_team;

class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CHostage;
class CNavArea;
class CTakeDamageInfo;
class CUserCmd;
struct Extent;
class HidingSpot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

LINK_ENTITY_TO_CLASS( cs_bot, CCSBot );

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the number of bots following the given player
 */
int GetBotFollowCount( CCSPlayer *leader )
{
	int count = 0;

	for( int i=1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );

		if (entity == nullptr)
			continue;

		CBasePlayer *player = static_cast<CBasePlayer *>( entity );

		if (!player->IsBot())
			continue;

 		if (!player->IsAlive())
 			continue;

		CCSBot *bot = ToCSBot( player );
		if (bot && bot->GetFollowLeader() == leader)
			++count;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Change movement speed to walking
 */
void CCSBot::Walk( void )
{
	if (m_mustRunTimer.IsElapsed())
	{
		BaseClass::Walk();
	}
	else
	{
		// must run
		Run();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if jump was started.
 * This is extended from the base jump to disallow jumping when in a crouch area.
 */
bool CCSBot::Jump( bool mustJump )
{
	// prevent jumping if we're crouched, unless we're in a crouchjump area - jump wins
	bool inCrouchJumpArea = (m_lastKnownArea && 
		(m_lastKnownArea->GetAttributes() & NAV_MESH_CROUCH) &&
		(m_lastKnownArea->GetAttributes() & NAV_MESH_JUMP));

	if ( !IsUsingLadder() && IsDucked() && !inCrouchJumpArea )
	{
		return false;
	}

	if ( CSGameRules()->IsPlayingCoopMission() && !mustJump )
		return false;

	return BaseClass::Jump( mustJump );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when injured by something
 * NOTE: We dont want to directly call Attack() here, or the bots will have super-human reaction times when injured
 */
int CCSBot::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	m_bIsSleeping = false;

	CBaseEntity *attacker = info.GetInflictor();

	// getting hurt makes us alert
	BecomeAlert();
	StopWaiting();

	if ( info.GetDamageType() == DMG_BURN )
	{
		m_burnedByFlamesTimer.Start();
	}

	// if we were attacked by a teammate, rebuke
	if (attacker->IsPlayer())
	{
		CCSPlayer *player = static_cast<CCSPlayer *>( attacker );
		
		if ( IsOtherSameTeam( player->GetTeamNumber() ) && !IsOtherEnemy( player ) && !player->IsBot() )
		{
			// Response rules specifically needs to know if this is bullet or knife damage, so no need to do a fully general solution at this time
			const char *pDmgType = "OTHER";
			if ( info.GetDamageType() & DMG_BULLET )
			{
				pDmgType = "DMG_BULLET";
			}
			else if ( info.GetDamageType() & DMG_SLASH )
			{
				pDmgType = "DMG_SLASH";
			}
			GetChatter()->FriendlyFire( pDmgType );
		}
	}

	if (attacker->IsPlayer() && IsEnemy( attacker ))
	{
		// Track previous attacker so we don't try to panic multiple times for a shotgun blast
		CCSPlayer *lastAttacker = m_attacker;
		float lastAttackedTimestamp = m_attackedTimestamp;

		// keep track of our last attacker
		m_attacker = reinterpret_cast<CCSPlayer *>( attacker );
		m_attackedTimestamp = gpGlobals->curtime;

		// no longer safe
		AdjustSafeTime();

		if ( !IsSurprised() && (m_attacker != lastAttacker || m_attackedTimestamp != lastAttackedTimestamp) )
		{
			CCSPlayer *enemy = static_cast<CCSPlayer *>( attacker );

			// being hurt by an enemy we can't see causes panic
			if (!IsVisible( enemy, true ))
			{
				// if not attacking anything, look around to try to find attacker
				if (!IsAttacking())
				{
					Panic();
				}
				else	// we are attacking
				{
					if (!IsEnemyVisible())
					{
						// can't see our current enemy, panic to acquire new attacker
						Panic();
					}
				}
			}
		}
	}

	// extend
	return BaseClass::OnTakeDamage_Impl( info );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when killed
 */
void CCSBot::Event_Killed_Impl( const CTakeDamageInfo &info )
{ 
//	PrintIfWatched( "Killed( attacker = %s )\n", STRING(pevAttacker->netname) );

	GetChatter()->OnDeath();

	// increase the danger where we died
	const float deathDanger = 1.0f;
	const float deathDangerRadius = 500.0f;
	TheNavMesh->IncreaseDangerNearby( GetTeamNumber(), deathDanger, m_lastKnownArea, GetAbsOrigin(), deathDangerRadius );

	// end voice feedback
	m_voiceEndTimestamp = 0.0f;

	// extend
	BaseClass::Event_Killed_Impl( info );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if line segment intersects rectagular volume
 */
static constexpr auto HI_X = 0x01;
static constexpr auto LO_X = 0x02;
static constexpr auto HI_Y = 0x04;
static constexpr auto LO_Y = 0x08;
static constexpr auto HI_Z = 0x10;
static constexpr auto LO_Z = 0x20;

inline bool IsIntersectingBox( const Vector& start, const Vector& end, const Vector& boxMin, const Vector& boxMax )
{
	unsigned char startFlags = 0;
	unsigned char endFlags = 0;

	// classify start point
	if (start.x < boxMin.x)
		startFlags |= LO_X;
	if (start.x > boxMax.x)
		startFlags |= HI_X;

	if (start.y < boxMin.y)
		startFlags |= LO_Y;
	if (start.y > boxMax.y)
		startFlags |= HI_Y;

	if (start.z < boxMin.z)
		startFlags |= LO_Z;
	if (start.z > boxMax.z)
		startFlags |= HI_Z;

	// classify end point
	if (end.x < boxMin.x)
		endFlags |= LO_X;
	if (end.x > boxMax.x)
		endFlags |= HI_X;

	if (end.y < boxMin.y)
		endFlags |= LO_Y;
	if (end.y > boxMax.y)
		endFlags |= HI_Y;

	if (end.z < boxMin.z)
		endFlags |= LO_Z;
	if (end.z > boxMax.z)
		endFlags |= HI_Z;

	// trivial reject
	if (startFlags & endFlags)
		return false;

	/// @todo Do exact line/box intersection check

	return true;
}

extern void UTIL_DrawBox( Extent *extent, int lifetime, int red, int green, int blue );

//--------------------------------------------------------------------------------------------------------------
/**
 * When bot is touched by another entity.
 */
void CCSBot::Touch_Impl( CBaseEntity *other )
{
	// EXTEND
	BaseClass::Touch_Impl( other );

	// if we have touched a higher-priority player, make way
	/// @todo Need to account for reaction time, etc.
	if (other->IsPlayer())
	{
		// if we are defusing a bomb, don't move
		if (IsDefusingBomb())
			return;

		// if we are on a ladder, don't move
		if (IsUsingLadder())
			return;

		CCSPlayer *player = static_cast<CCSPlayer *>( other );

		// get priority of other player
		unsigned int otherPri = TheCSBots()->GetPlayerPriority( player );

		// get our priority
		unsigned int myPri = TheCSBots()->GetPlayerPriority( this );

		// if our priority is better, don't budge
		if (myPri < otherPri)
			return;

		// they are higher priority - make way, unless we're already making way for someone more important
		if (m_avoid != nullptr)
		{
			unsigned int avoidPri = TheCSBots()->GetPlayerPriority( static_cast<CBasePlayer *>( static_cast<CBaseEntity *>( m_avoid ) ) );
			if (avoidPri < otherPri)
			{
				// ignore 'other' because we're already avoiding someone better
				return;
			}
		}

		m_avoid = other;
		m_avoidTimestamp = gpGlobals->curtime;
	}

	// Check for breakables we're actually touching
	// If we're not stuck or crouched, we don't care
	if ( !m_isStuck && !IsCrouching() && !IsOnLadder() )
		return;

	// See if it's breakable
	if ( IsBreakableEntity( other ) )
	{
		// it's breakable - try to shoot it.
		SetLookAt( "Breakable", other->WorldSpaceCenter(), PRIORITY_HIGH, 0.1f, false, 5.0f, true );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are busy doing something important
 */
bool CCSBot::IsBusy( void ) const
{
	if (IsAttacking() || 
		IsBuying() ||
		IsDefusingBomb() || 
		GetTask() == PLANT_BOMB ||
		GetTask() == RESCUE_HOSTAGES ||
		IsSniping())
	{
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::BotDeathThink( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Try to join the given team
 */
void CCSBot::TryToJoinTeam( int team )
{
	m_desiredTeam = team;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assign given player as our current enemy to attack
 */
void CCSBot::SetBotEnemy( CCSPlayer *enemy )
{
	if (m_enemy != enemy)
	{
		m_enemy = enemy; 
		m_currentEnemyAcquireTimestamp = gpGlobals->curtime;

		PrintIfWatched( "SetBotEnemy: %s\n", (enemy) ? enemy->GetPlayerName() : "(NULL)" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If we are not on the navigation mesh (m_currentArea == NULL),
 * move towards last known area.
 * Return false if off mesh.
 */
bool CCSBot::StayOnNavMesh( void )
{
	if (m_currentArea == nullptr)
	{
		// move back onto the area map

		// if we have no lastKnownArea, we probably started off
		// of the nav mesh - find the closest nav area and use it
		CNavArea *goalArea;
		if (!m_currentArea && !m_lastKnownArea)
		{
			goalArea = TheNavMesh->GetNearestNavArea( GetCentroid( this ) );
			PrintIfWatched( "Started off the nav mesh - moving to closest nav area...\n" );
		}
		else
		{
			goalArea = m_lastKnownArea;
			PrintIfWatched( "Getting out of NULL area...\n" );
		}

		if (goalArea)
		{
			Vector pos;
			goalArea->GetClosestPointOnArea( GetCentroid( this ), &pos );

			// move point into area
			Vector to = pos - GetCentroid( this );
			to.NormalizeInPlace();

			const float stepInDist = 5.0f;		// how far to "step into" an area - must be less than min area size
			pos = pos + (stepInDist * to);

			MoveTowardsPosition( pos );
		}

		// if we're stuck, try to get un-stuck
		// do stuck movements last, so they override normal movement
		if (m_isStuck)
			Wiggle();
				
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we will do scenario-related tasks
 */
bool CCSBot::IsDoingScenario( void ) const
{
	// if we are deferring to humans, and there is a live human on our team, don't do the scenario
	if (cv_bot_defer_to_human_goals.GetBool())
	{
		if (UTIL_HumansOnTeam( GetTeamNumber(), IS_ALIVE ))
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we noticed the bomb on the ground or on the radar (for T's only)
 */
bool CCSBot::NoticeLooseBomb( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	if (ctrl->GetScenario() != CCSBotManager::SCENARIO_DEFUSE_BOMB)
		return false;

	CBaseEntity *bomb = ctrl->GetLooseBomb();

	if (bomb)
	{
		// T's can always see bomb on their radar
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if can see the bomb lying on the ground
 */
bool CCSBot::CanSeeLooseBomb( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	if (ctrl->GetScenario() != CCSBotManager::SCENARIO_DEFUSE_BOMB)
		return false;

	CBaseEntity *bomb = ctrl->GetLooseBomb();

	if (bomb)
	{
		if (IsVisible( bomb->GetAbsOrigin(), true ))
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if can see the planted bomb 
 */
bool CCSBot::CanSeePlantedBomb( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	if (ctrl->GetScenario() != CCSBotManager::SCENARIO_DEFUSE_BOMB)
		return false;

	if (!GetGameState()->IsBombPlanted())
		return false;

	const Vector *bombPos = GetGameState()->GetBombPosition();

	if (bombPos && IsVisible( *bombPos, true ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return last enemy that hurt us
 */
CCSPlayer *CCSBot::GetAttacker( void ) const
{
	if (m_attacker && m_attacker->IsAlive())
		return m_attacker;

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Immediately jump off of our ladder, if we're on one
 */
void CCSBot::GetOffLadder( void )
{
	if (IsUsingLadder())
	{
		Jump( true );
		DestroyPath();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return time when given spot was last checked
 */
float CCSBot::GetHidingSpotCheckTimestamp( HidingSpot *spot ) const
{
	for( int i=0; i<m_checkedHidingSpotCount; ++i )
		if (m_checkedHidingSpot[i].spot->GetID() == spot->GetID())
			return m_checkedHidingSpot[i].timestamp;

	return -999999.9f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the timestamp of the given spot to now.
 * If the spot is not in the set, overwrite the least recently checked spot.
 */
void CCSBot::SetHidingSpotCheckTimestamp( HidingSpot *spot )
{
	int leastRecent = 0;
	float leastRecentTime = gpGlobals->curtime + 1.0f;

	for( int i=0; i<m_checkedHidingSpotCount; ++i )
	{
		// if spot is in the set, just update its timestamp
		if (m_checkedHidingSpot[i].spot->GetID() == spot->GetID())
		{
			m_checkedHidingSpot[i].timestamp = gpGlobals->curtime;
			return;
		}

		// keep track of least recent spot
		if (m_checkedHidingSpot[i].timestamp < leastRecentTime)
		{
			leastRecentTime = m_checkedHidingSpot[i].timestamp;
			leastRecent = i;
		}
	}

	// if there is room for more spots, append this one
	if (m_checkedHidingSpotCount < MAX_CHECKED_SPOTS)
	{
		m_checkedHidingSpot[ m_checkedHidingSpotCount ].spot = spot;
		m_checkedHidingSpot[ m_checkedHidingSpotCount ].timestamp = gpGlobals->curtime;
		++m_checkedHidingSpotCount;
	}
	else
	{
		// replace the least recent spot
		m_checkedHidingSpot[ leastRecent ].spot = spot;
		m_checkedHidingSpot[ leastRecent ].timestamp = gpGlobals->curtime;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Periodic check of hostage count in case we lost some
 */
void CCSBot::UpdateHostageEscortCount( void )
{
	const float updateInterval = 1.0f;
	if (m_hostageEscortCount == 0 || gpGlobals->curtime - m_hostageEscortCountTimestamp < updateInterval)
		return;

	m_hostageEscortCountTimestamp = gpGlobals->curtime;

	// recount the hostages in case we lost some
	m_hostageEscortCount = 0;

	for( int i=0; i<std::ssize( g_Hostages ); ++i )
	{
		CHostage *hostage = g_Hostages[i];

		// skip dead or rescued hostages
		if ( !hostage->IsValid() || !hostage->IsAlive() )
			continue;

		// check if hostage has targeted us, and is following
		if ( hostage->IsFollowing( this ) )
			++m_hostageEscortCount;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are outnumbered by enemies
 */
bool CCSBot::IsOutnumbered( void ) const
{
	// Play panic lines more often in coop
	if ( CSGameRules()->IsPlayingCoopMission() )
		return GetNearbyFriendCount() <= GetNearbyEnemyCount();

	return (GetNearbyFriendCount() < GetNearbyEnemyCount()-1) ? true : false;		
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of enemies we are outnumbered by
 */
int CCSBot::OutnumberedCount( void ) const
{
	if (IsOutnumbered())
		return (GetNearbyEnemyCount()-1) - GetNearbyFriendCount();

	return 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest "important" enemy for the given scenario (bomb carrier, VIP, hostage escorter)
 */
CCSPlayer *CCSBot::GetImportantEnemy( bool checkVisibility ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );
	CCSPlayer *nearEnemy = nullptr;
	float nearDist = 999999999.9f;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );

		if (entity == nullptr)
			continue;

//		if (FNullEnt( entity->pev ))
//			continue;

//		if (FStrEq( STRING( entity->pev->netname ), "" ))
//			continue;

		// is it a player?
		if (!entity->IsPlayer())
			continue;

		CCSPlayer *player = static_cast<CCSPlayer *>( entity );

		// is it alive?
		if (!player->IsAlive())
			continue;

		// skip friends
		if ( player->IsOtherSameTeam( GetTeamNumber() ) && !player->IsOtherEnemy( entindex() ) )
			continue;

		// is it "important"
		if (!ctrl->IsImportantPlayer( player ))
			continue;

		// is it closest?
		Vector d = GetAbsOrigin() - player->GetAbsOrigin();
		float distSq = d.x*d.x + d.y*d.y + d.z*d.z;
		if (distSq < nearDist)
		{
			if (checkVisibility && !IsVisible( player, true ))
				continue;

			nearEnemy = player;
			nearDist = distSq;
		}
	}

	return nearEnemy;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Sets our current disposition
 */
void CCSBot::SetDisposition( DispositionType disposition ) 
{ 
	m_disposition = disposition;

	if (m_disposition != IGNORE_ENEMIES)
		m_ignoreEnemiesTimer.Invalidate();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return our current disposition
 */
CCSBot::DispositionType CCSBot::GetDisposition( void ) const
{
	if (!m_ignoreEnemiesTimer.IsElapsed())
		return IGNORE_ENEMIES;
	
	return m_disposition;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Ignore enemies for a short durationy
 */
void CCSBot::IgnoreEnemies( float duration )
{
	m_ignoreEnemiesTimer.Start( duration );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Increase morale one step
 */
void CCSBot::IncreaseMorale( void )
{
	if (m_morale < EXCELLENT)
		m_morale = static_cast<MoraleType>( m_morale + 1 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decrease morale one step
 */
void CCSBot::DecreaseMorale( void )
{
	if (m_morale > TERRIBLE)
		m_morale = static_cast<MoraleType>( m_morale - 1 );
}

float CCSBot::GetNoiseInvestigateChance( void ) const
{
	if ( CSGameRules()->IsPlayingCoopMission() )
		return 100.0f;

	// chance of investigating is inversely proportional to distance
	const float maxNoiseDist = 3000.0f;
	float chance = 100.0f * ( 1.0f - ( GetNoiseRange() / maxNoiseDist ) );

	// modify chance by number of friends remaining
	// if we have lots of friends, presumably one of them is closer and will check it out
	if ( GetFriendsRemaining() >= 3 )
	{
		float friendFactor = 5.0f * GetFriendsRemaining();
		if ( friendFactor > 50.0f )
			friendFactor = 50.0f;

		chance -= friendFactor;
	}
	return chance;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are acting like a rogue (not listening to teammates, not doing scenario goals)
 * @todo Account for morale
 */
bool CCSBot::IsRogue( void ) const
{ 
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );
	if (!ctrl->AllowRogues())
		return false;

	// periodically re-evaluate our rogue status
	if (m_rogueTimer.IsElapsed())
	{
		m_rogueTimer.Start( RandomFloat( 10.0f, 30.0f ) );

		// our chance of going rogue is inversely proportional to our teamwork attribute
		const float rogueChance = 100.0f * (1.0f - GetProfile()->GetTeamwork());

		m_isRogue = (RandomFloat( 0, 100 ) < rogueChance);
	}

	return m_isRogue; 
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are in a hurry 
 */
bool CCSBot::IsHurrying( void ) const
{
	if (!m_hurryTimer.IsElapsed())
		return true;

	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	// if the bomb has been planted, we are in a hurry, CT or T (they could be defusing it!)
	if (ctrl->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB && ctrl->IsBombPlanted())
		return true;
	
	// if we are a T and hostages are being rescued, we are in a hurry
	if (ctrl->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES && 
		GetTeamNumber() == TEAM_TERRORIST && 
		GetGameState()->AreAllHostagesBeingRescued())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if it is the early, "safe", part of the round
 */
bool CCSBot::IsSafe( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	if (ctrl->GetElapsedRoundTime() < m_safeTime)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if it is well past the early, "safe", part of the round
 */
bool CCSBot::IsWellPastSafe( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	if (ctrl->GetElapsedRoundTime() > 2.0f * m_safeTime)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we were in the safe time last update, but not now
 */
bool CCSBot::IsEndOfSafeTime( void ) const
{
	return m_wasSafe && !IsSafe();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the amount of "safe time" we have left
 */
float CCSBot::GetSafeTimeRemaining( void ) const
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	return m_safeTime - ctrl->GetElapsedRoundTime();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when enemy seen to adjust safe time for this round
 */
void CCSBot::AdjustSafeTime( void )
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	// if we spotted an enemy sooner than we thought possible, adjust our notion of "safe" time
	if (ctrl->GetElapsedRoundTime() < m_safeTime)
	{
		// since right now is not safe, adjust safe time to be a few seconds ago
		m_safeTime = ctrl->GetElapsedRoundTime() - 2.0f;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we haven't seen an enemy for "a long time"
 */
bool CCSBot::HasNotSeenEnemyForLongTime( void ) const
{
	const float longTime = 30.0f;
	return (GetTimeSinceLastSawEnemy() > longTime);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Pick a random zone and hide near it
 */
bool CCSBot::GuardRandomZone( float range )
{
	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheCSBots() );

	const CCSBotManager::Zone *zone = ctrl->GetRandomZone();
	if (zone)
	{
		CNavArea *rescueArea = ctrl->GetRandomAreaInZone( zone );
		if (rescueArea)
		{
			Hide( rescueArea, -1.0f, range );
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
class CollectRetreatSpotsFunctor
{
public:
	CollectRetreatSpotsFunctor( CCSBot *me, float range )
	{
		m_me = me;
		m_count = 0;
		m_range = range;
	}

	enum { MAX_SPOTS = 256 };

	bool operator() ( CNavArea *area )
	{
		// collect all the hiding spots in this area
		const HidingSpotVector *pSpots = area->GetHidingSpots();

		for ( const HidingSpot *spot : *pSpots )
		{

			if (m_count >= MAX_SPOTS)
				break;

			// make sure hiding spot is in range
			if (m_range > 0.0f)
				if ((spot->GetPosition() - GetCentroid( m_me )).IsLengthGreaterThan( m_range ))
					continue;

			// if a Player is using this hiding spot, don't consider it
			if (IsSpotOccupied( m_me, spot->GetPosition() ))
			{
				// player is in hiding spot
				/// @todo Check if player is moving or sitting still
				continue;
			}

			// don't select spot if an enemy can see it
			if (UTIL_IsVisibleToTeam( spot->GetPosition() + Vector( 0, 0, HalfHumanHeight ), OtherTeam( m_me->GetTeamNumber() ) ))
				continue;

			// don't select spot if it is closest to an enemy
			CBasePlayer *owner = UTIL_GetClosestPlayer( spot->GetPosition() );
			if (owner && !m_me->InSameTeam( owner ))
				continue;

			m_spot[ m_count++ ] = &spot->GetPosition();
		}

		// if we've filled up, stop searching
		if (m_count == MAX_SPOTS)
			return false;

		return true;
	}

	CCSBot *m_me;
	float m_range;

	const Vector *m_spot[ MAX_SPOTS ];
	int m_count;
};

/**
 * Do a breadth-first search to find a good retreat spot.
 * Don't pick a spot that a Player is currently occupying.
 */
const Vector *FindNearbyRetreatSpot( CCSBot *me, float maxRange )
{
	CNavArea *area = me->GetLastKnownArea();
	if (area == nullptr)
		return nullptr;

	// collect spots that enemies cannot see
	CollectRetreatSpotsFunctor collector( me, maxRange );
	SearchSurroundingAreas( area, GetCentroid( me ), collector, maxRange );

	if (collector.m_count == 0)
		return nullptr;

	// select a hiding spot at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_spot[ which ];
}

//--------------------------------------------------------------------------------------------------------------
class FarthestHostage
{
public:
	FarthestHostage( const CCSBot *me )
	{
		m_me = me;
		m_farRange = -1.0f;
	}

	bool operator() ( CHostage *hostage )
	{
		if (hostage->IsFollowing( m_me ))
		{
			float range = (hostage->GetAbsOrigin() - m_me->GetAbsOrigin()).Length();
			if (range > m_farRange)
			{
				m_farRange = range;
			}
		}

		return true;
	}

	const CCSBot *m_me;
	float m_farRange;
};

/**
 * Return euclidean distance to farthest escorted hostage.
 * Return -1 if no hostage is following us.
 */
float CCSBot::GetRangeToFarthestEscortedHostage( void ) const
{
	FarthestHostage away( this );

	ForEachHostage( away );

	return away.m_farRange;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return string describing current task
 * NOTE: This MUST be kept in sync with the CCSBot::TaskType enum
 */
const char *CCSBot::GetTaskName( void ) const
{
	static const char *name[ NUM_TASKS ] =
	{
		"SEEK_AND_DESTROY",
		"PLANT_BOMB",
		"FIND_TICKING_BOMB",
		"DEFUSE_BOMB",
		"GUARD_TICKING_BOMB",
		"GUARD_BOMB_DEFUSER",
		"GUARD_LOOSE_BOMB",
		"GUARD_BOMB_ZONE",
		"GUARD_INITIAL_ENCOUNTER",
		"ESCAPE_FROM_BOMB",
		"HOLD_POSITION",
		"FOLLOW",
		"VIP_ESCAPE",
		"GUARD_VIP_ESCAPE_ZONE",
		"COLLECT_HOSTAGES",
		"RESCUE_HOSTAGES",
		"GUARD_HOSTAGES",
		"GUARD_HOSTAGE_RESCUE_ZONE",
		"MOVE_TO_LAST_KNOWN_ENEMY_POSITION",
		"MOVE_TO_SNIPER_SPOT",
		"SNIPING",
		"ESCAPE_FROM_FLAMES",
	};

	return name[ (int)GetTask() ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return string describing current disposition
 * NOTE: This MUST be kept in sync with the CCSBot::DispositionType enum
 */
const char *CCSBot::GetDispositionName( void ) const
{
	static const char *name[ NUM_DISPOSITIONS ] =
	{
		"ENGAGE_AND_INVESTIGATE",
		"OPPORTUNITY_FIRE",
		"SELF_DEFENSE",
		"IGNORE_ENEMIES"
	};

	return name[ (int)GetDisposition() ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return string describing current morale
 * NOTE: This MUST be kept in sync with the CCSBot::MoraleType enum
 */
const char *CCSBot::GetMoraleName( void ) const
{
	static const char *name[ EXCELLENT - TERRIBLE + 1 ] =
	{
		"TERRIBLE",
		"BAD",
		"NEGATIVE",
		"NEUTRAL",
		"POSITIVE",
		"GOOD",
		"EXCELLENT"
	};

	return name[ (int)GetMorale() + 3 ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Fill in a CUserCmd with our data
 */
void CCSBot::BuildUserCmd( CUserCmd& cmd, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, int buttons, byte impulse )
{
	cmd.Reset();
	if ( !RunMimicCommand( cmd ) )
	{
		// Don't walk when ducked - it's painfully slow
		if ( m_Local.m_bDucked || m_Local.m_bDucking )
		{
			buttons &= ~IN_SPEED;
		}

		cmd.command_number = gpGlobals->tickcount;
		cmd.forwardmove = forwardmove;
		cmd.sidemove = sidemove;
		cmd.upmove = upmove;
		cmd.buttons = buttons;
		cmd.impulse = impulse;

		VectorCopy( viewangles, cmd.viewangles );
		cmd.random_seed = random_valve->RandomInt( 0, 0x7fffffff );
	}
}

//
// Returns a value in the -1 .. +1 range based on adding some cosines together. Cheap and sloppy.
float CCSBot::SlowNoise( float fTau ) const
{
	int iUniqueOffset = HashInt(entindex()) & 0xFF;
	float t = (float)iUniqueOffset;

	t = (t + gpGlobals->curtime / fTau) * std::numbers::pi * 2.0f;

	return 0.25f * ( std::cosf( fTau ) + cosf( fTau * 29.f / 47.f ) + cosf( fTau * 59.f / 137.f ) + cosf( fTau * 151.f / 499.f ) );
}

//--------------------------------------------------------------------------------------------------------------
// Some game types allow players to pass through each other, this method pushes them apart
void CCSBot::AvoidPlayers( CUserCmd *pCmd )
{
	Vector forward, right;
	EyeVectors( &forward, &right );

	std::vector< CCSPlayer * > playerVector;
	CollectPlayers( &playerVector, GetTeamNumber(), COLLECT_ONLY_LIVING_PLAYERS );

	Vector avoidVector = vec3_origin;

	float tooClose = 2.0f * HalfHumanWidth;

	for( int i=0; i<std::ssize( playerVector ); ++i )
	{
		CCSPlayer *them = playerVector[i];

		if ( entindex() == them->entindex() )
		{
			continue;
		}

		Vector between = GetAbsOrigin() - them->GetAbsOrigin();
		if ( between.IsLengthLessThan( tooClose ) )
		{
			float range = between.NormalizeInPlace();

			avoidVector += ( 1.0f - ( range / tooClose ) ) * between;
		}
	}

	if ( avoidVector.IsZero() )
	{
// 		m_Shared.SetSeparation( false );
// 		m_Shared.SetSeparationVelocity( vec3_origin );
		return;
	}

	avoidVector.NormalizeInPlace();

// 	m_Shared.SetSeparation( true );

	const float maxSpeed = 50.0f;
// 	m_Shared.SetSeparationVelocity( avoidVector * maxSpeed );

	float ahead = maxSpeed * DotProduct( forward, avoidVector );
	float side = maxSpeed * DotProduct( right, avoidVector );

	pCmd->forwardmove	+= ahead;
	pCmd->sidemove		+= side;
}

float CCSBot::GetFeetZ( void ) const
{
	return GetAbsOrigin().z;
}

const Vector *CCSBot::GetNoisePosition( void ) const
{
	if (m_noiseTimestamp > 0.0f)
		return &m_noisePosition;

	return nullptr;
}

bool CCSBot::IsAwareOfEnemyDeath( void ) const
{
	if (GetEnemyDeathTimestamp() == 0.0f)
		return false;

	if (m_enemy == nullptr)
		return true;

	if (!m_enemy->IsAlive() && gpGlobals->curtime - GetEnemyDeathTimestamp() > (1.0f - 0.8f * GetProfile()->GetSkill()))
		return true;

	return false;
}

void CCSBot::Panic( void )
{
	// we are stunned for a moment
	Surprise( RandomFloat( 0.2f, 0.3f ) );

	const float panicTime = 3.0f;
	m_panicTimer.Start( panicTime );

	const float panicRetreatRange = 300.0f;
	TryToRetreat( panicRetreatRange, 0.0f );

	PrintIfWatched( "*** PANIC ***\n" );
}

bool CCSBot::IsPanicking( void ) const
{
	return !m_panicTimer.IsElapsed();
}

void CCSBot::StopPanicking( void )
{
	m_panicTimer.Invalidate();
}

bool CCSBot::IsNotMoving( float minDuration ) const
{
	return (m_stillTimer.HasStarted() && m_stillTimer.GetElapsedTime() >= minDuration);
}

CWeaponCSBase *CCSBot::GetActiveCSWeapon( void ) const
{
	return reinterpret_cast<CWeaponCSBase *>( GetActiveWeapon() );
}

void CCSBot::SetLastCoopSpawnPoint( SpawnPointCoopEnemy *spawn )
{
	m_lastCoopSpawnPoint = spawn;
}

SpawnPointCoopEnemy *CCSBot::GetLastCoopSpawnPoint( void ) const
{
	return m_lastCoopSpawnPoint;
}

float CCSBot::GetCombatRange( void ) const
{ 
	return m_combatRange; 
}

void CCSBot::SetRogue( bool rogue )
{ 
	m_isRogue = rogue;
}

void CCSBot::Hurry( float duration )
{ 
	m_hurryTimer.Start( duration ); 
}

float CCSBot::GetSafeTime( void ) const
{
	return m_safeTime;
}

bool CCSBot::IsUnhealthy( void ) const
{
	return (GetHealth() <= 40);
}

bool CCSBot::IsAlert( void ) const
{
	return !m_alertTimer.IsElapsed();
}

void CCSBot::BecomeAlert( void )
{
	const float alertCooldownTime = 10.0f;
	m_alertTimer.Start( alertCooldownTime );
}

bool CCSBot::IsSneaking( void ) const
{
	return !m_sneakTimer.IsElapsed();
}

void CCSBot::Sneak( float duration )
{
	m_sneakTimer.Start( duration );
}

bool CCSBot::IsFollowing( void ) const
{ 
	return m_isFollowing;
}

CCSPlayer *CCSBot::GetFollowLeader( void ) const
{ 
	return m_leader;
}

float CCSBot::GetFollowDuration( void ) const
{
	return gpGlobals->curtime - m_followTimestamp;
}

bool CCSBot::CanAutoFollow( void ) const
{ 
	return (gpGlobals->curtime > m_allowAutoFollowTime);
}

void CCSBot::AimAtEnemy( void )
{ 
	m_isAimingAtEnemy = true;
}

void CCSBot::StopAiming( void )
{ 
	m_isAimingAtEnemy = false;
}

bool CCSBot::IsAimingAtEnemy( void ) const
{
	return m_isAimingAtEnemy;
}

float CCSBot::GetStateTimestamp( void ) const
{
	return m_stateTimestamp;
}

CSGameState *CCSBot::GetGameState( void )
{
	return &m_gameState;
}

const CSGameState *CCSBot::GetGameState( void ) const
{
	return &m_gameState;
}

bool CCSBot::IsAtBombsite( void )
{
	return m_bInBombZone;
}

void CCSBot::SetTask( TaskType task, CBaseEntity *entity )
{
	m_task = task;
	m_taskEntity = entity;

	if ( task == CCSBot::PLANT_BOMB )
	{
		// don't stop to attack - get the bomb there
		SetDisposition( CCSBot::SELF_DEFENSE );		
	}
}

CCSBot::TaskType CCSBot::GetTask( void ) const
{
	return m_task;
}

CBaseEntity *CCSBot::GetTaskEntity( void )
{
	return static_cast<CBaseEntity *>( m_taskEntity );
}

CCSBot::MoraleType CCSBot::GetMorale( void ) const
{
	return m_morale;
}

void CCSBot::Surprise( float duration )
{
	m_surpriseTimer.Start( duration );
}

bool CCSBot::IsSurprised( void ) const
{
	return !m_surpriseTimer.IsElapsed();
}

CNavArea *CCSBot::GetNoiseArea( void ) const
{
	return m_noiseArea;
}

void CCSBot::ForgetNoise( void )
{
	m_noiseTimestamp = 0.0f;
}

float CCSBot::GetNoiseRange( void ) const
{
	if (IsNoiseHeard())
		return m_noiseTravelDistance;

	return 999999999.9f;
}

PriorityType CCSBot::GetNoisePriority( void ) const
{ 
	return m_noisePriority;
}

BotChatterInterface *CCSBot::GetChatter( void )
{
	return m_pChatter;
}

CCSPlayer *CCSBot::GetBotEnemy( void ) const
{
	return m_enemy;
}

int CCSBot::GetNearbyEnemyCount( void ) const
{ 
	return std::min( GetEnemiesRemaining(), m_nearbyEnemyCount );
}

unsigned int CCSBot::GetEnemyPlace( void ) const
{
	return m_enemyPlace;
}

bool CCSBot::CanSeeBomber( void ) const
{
	return (m_bomber == nullptr) ? false : true;
}

CCSPlayer *CCSBot::GetBomber( void ) const
{
	return m_bomber;
}

int CCSBot::GetNearbyFriendCount( void ) const
{
	return std::min( GetFriendsRemaining(), m_nearbyFriendCount );
}

CCSPlayer *CCSBot::GetClosestVisibleFriend( void ) const
{
	return m_closestVisibleFriend;
}

CCSPlayer *CCSBot::GetClosestVisibleHumanFriend( void ) const
{
	return m_closestVisibleHumanFriend;
}

float CCSBot::GetTimeSinceAttacked( void ) const
{
	return gpGlobals->curtime - m_attackedTimestamp;
}

float CCSBot::GetFirstSawEnemyTimestamp( void ) const
{
	return m_firstSawEnemyTimestamp;
}

float CCSBot::GetLastSawEnemyTimestamp( void ) const
{
	return m_lastSawEnemyTimestamp;
}

float CCSBot::GetTimeSinceLastSawEnemy( void ) const
{
	return gpGlobals->curtime - m_lastSawEnemyTimestamp;
}

float CCSBot::GetTimeSinceAcquiredCurrentEnemy( void ) const
{
	return gpGlobals->curtime - m_currentEnemyAcquireTimestamp;
}

float CCSBot::GetTimeSinceBurnedByFlames( void ) const
{
	return m_burnedByFlamesTimer.GetElapsedTime();
}

const Vector &CCSBot::GetLastKnownEnemyPosition( void ) const
{
	return m_lastEnemyPosition;
}

bool CCSBot::IsEnemyVisible( void ) const			
{
	return m_isEnemyVisible;
}

float CCSBot::GetEnemyDeathTimestamp( void ) const	
{
	return m_enemyDeathTimestamp;
}

int CCSBot::GetLastVictimID( void ) const
{
	return m_lastVictimID;
}

bool CCSBot::CanSeeSniper( void ) const
{
	return m_isEnemySniperVisible;
}

bool CCSBot::HasSeenSniperRecently( void ) const
{
	return !m_sawEnemySniperTimer.IsElapsed();
}

float CCSBot::GetTravelDistanceToPlayer( CCSPlayer *player ) const
{
	if (player == nullptr)
		return -1.0f;

	if (!player->IsAlive())
		return -1.0f;

	return m_playerTravelDistance[ player->entindex() % MAX_PLAYERS ];
}

bool CCSBot::HasPath( void ) const
{
	return (m_pathLength) ? true : false;
}

void CCSBot::DestroyPath( void )		
{
	m_isStopping = false;
	m_pathLength = 0;
	m_pathLadder = nullptr;
}

const Vector &CCSBot::GetPathEndpoint( void ) const		
{
	return m_path[ m_pathLength-1 ].pos;
}

const Vector &CCSBot::GetPathPosition( int index ) const
{
	return m_path[ index ].pos;
}

bool CCSBot::IsUsingLadder( void ) const	
{
	return (m_pathLadder) ? true : false;
}

void CCSBot::SetGoalEntity( CBaseEntity *entity )	
{
	m_goalEntity = entity;
}

CBaseEntity *CCSBot::GetGoalEntity( void )
{
	return m_goalEntity;
}

void CCSBot::ForceRun( float duration )
{
	Run();
	m_mustRunTimer.Start( duration );
}

void CCSBot::Wait( float duration )			
{
	m_waitTimer.Start( duration );
}

bool CCSBot::IsWaiting( void ) const		
{
	return !m_waitTimer.IsElapsed();
}

void CCSBot::StopWaiting( void )			
{
	m_waitTimer.Invalidate();
}

bool CCSBot::HasVisitedEnemySpawn( void ) const		
{
	return m_hasVisitedEnemySpawn;
}

const Vector &CCSBot::EyePositionConst( void ) const		
{
	return m_eyePosition;
}

void CCSBot::SetLookAngles( float yaw, float pitch )
{
	m_lookYaw = yaw;
	m_lookPitch = pitch;
}

void CCSBot::SetForwardAngle( float angle ) 
{
	m_forwardAngle = angle;
}

void CCSBot::SetLookAheadAngle( float angle ) 
{
	m_lookAheadAngle = angle;
}

void CCSBot::ClearLookAt( void )
{ 
	//PrintIfWatched( "ClearLookAt()\n" );
	m_lookAtSpotState = NOT_LOOKING_AT_SPOT; 
	m_lookAtDesc = nullptr; 
}

bool CCSBot::IsLookingAtSpot( PriorityType pri ) const
{ 
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && m_lookAtSpotPriority >= pri)
		return true;

	return false;
}

bool CCSBot::IsViewMoving( float angleVelThreshold ) const
{
	if (m_lookYawVel < angleVelThreshold && m_lookYawVel > -angleVelThreshold &&
		m_lookPitchVel < angleVelThreshold && m_lookPitchVel > -angleVelThreshold)
	{
		return false;
	}

	return true;
}

bool CCSBot::HasViewBeenSteady( float duration ) const
{
	return (m_viewSteadyTimer.GetElapsedTime() > duration);
}

bool CCSBot::HasLookAtTarget( void ) const
{
	return (m_lookAtSpotState != NOT_LOOKING_AT_SPOT);
}

bool CCSBot::IsEnemyPartVisible( VisiblePartType part ) const
{ 
	CVProfScope _( "CCSBot::IsEnemyPartVisible", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	if (!IsEnemyVisible())
		return false;

	return (m_visibleEnemyParts & part) ? true : false;
}

bool CCSBot::IsSignificantlyCloser( const CCSPlayer *testPlayer, const CCSPlayer *referencePlayer ) const
{
	if ( !referencePlayer )
		return true;

	if ( !testPlayer )
		return false;

	float testDist = ( GetAbsOrigin() - testPlayer->GetAbsOrigin() ).Length();
	float referenceDist = ( GetAbsOrigin() - referencePlayer->GetAbsOrigin() ).Length();

	const float significantRangeFraction = 0.7f;
	if ( testDist < referenceDist * significantRangeFraction )
		return true;

	return false;
}

void CCSBot::ClearApproachPoints( void )	
{
	m_approachPointCount = 0;
}

const CNavArea *CCSBot::GetInitialEncounterArea( void ) const
{
	return m_initialEncounterArea;
}

void CCSBot::SetInitialEncounterArea( const CNavArea *area )		
{
	m_initialEncounterArea = area;
}

bool CCSBot::IsThrowingGrenade( void ) const		
{
	return m_grenadeTossState != NOT_THROWING;
}

bool CCSBot::IsAvoidingGrenade( void ) const
{
	return !m_isAvoidingGrenade.IsElapsed();
}

void CCSBot::PrimaryAttack( void )
{

	// for now the bots only secondary attack with the revolver until I teach them to hold the fire button
	CWeaponCSBase *weapon = GetActiveCSWeapon();
	if ( weapon && weapon->IsRevolver() && CanActiveWeaponFire() )
	{
		BaseClass::SecondaryAttack();
		return;
	}

	if ( IsUsingPistol() && !CanActiveWeaponFire() )
		return;

	BaseClass::PrimaryAttack();
}

CCSBot::ZoomType CCSBot::GetZoomLevel( void )
{
	if (GetFOV() > 60.0f)
		return NO_ZOOM;
	if (GetFOV() > 25.0f)
		return LOW_ZOOM;
	return HIGH_ZOOM;
}

bool CCSBot::IsWaitingForZoom( void ) const		
{
	return !m_zoomTimer.IsElapsed();
}

int CCSBot::GetHostageEscortCount( void ) const		
{
	return m_hostageEscortCount;
}

void CCSBot::IncreaseHostageEscortCount( void )		
{
	++m_hostageEscortCount;
}

void CCSBot::ResetWaitForHostagePatience( void )
{
	m_isWaitingForHostage = false;
	m_inhibitWaitingForHostageTimer.Invalidate();
}

bool CCSBot::IsUsingVoice() const
{
	return m_voiceEndTimestamp > gpGlobals->curtime; 
}

bool CCSBot::IsOpeningDoor( void ) const
{
	return m_isOpeningDoor;
}

float HideState::GetHideTime( void) const
{
	if (IsAtSpot())
	{
		return m_duration - m_hideTimer.GetRemainingTime();
	}

	return 0.0f;
}

PathCost::PathCost( CCSBot *bot, RouteType route)
{
	m_bot = bot;
	m_route = route;

	float baseDangerFactor = CSGameRules()->IsPlayingGunGameTRBomb() ? 0.25f : 100.0f;
	m_dangerFactor = (1.0f - (0.95f * m_bot->GetProfile()->GetAggression( ))) * baseDangerFactor;
}

class BotChatterInterface;
class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

// Use to toggle between old bot chatter system and response rules
extern ConVar bot_chatter_use_rr;

/**
 * @todo Fix this
 */

bool GetRandomSpotAtPlace( Place place, Vector *pPos )
{
	int count = 0;

	for ( CNavArea *area : TheNavAreas )
	{

		if (area->GetPlace() == place)
			++count;
	}

	if (count == 0)
		return false;
		
	int which = RandomInt( 0, count-1 );

	for ( CNavArea *area : TheNavAreas )
	{

		if (area->GetPlace() == place && which == 0)
		{
			*pPos = area->GetCenter();
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Cause the given bot to act on this meme (forwards to the concrete type according to the Kind)
 */
void BotMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	switch ( m_kind )
	{
	case Kind::Help:	static_cast< const BotHelpMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::BombsiteStatus:	static_cast< const BotBombsiteStatusMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::BombStatus:	static_cast< const BotBombStatusMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::Follow:	static_cast< const BotFollowMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::DefendHere:	static_cast< const BotDefendHereMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::WhereBomb:	static_cast< const BotWhereBombMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::RequestReport:	static_cast< const BotRequestReportMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::AllHostagesGone:	static_cast< const BotAllHostagesGoneMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::HostageBeingTaken:	static_cast< const BotHostageBeingTakenMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::HeardNoise:	static_cast< const BotHeardNoiseMeme * >( this )->Interpret( sender, receiver );	break;
	case Kind::WarnSniper:	static_cast< const BotWarnSniperMeme * >( this )->Interpret( sender, receiver );	break;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Delete a meme through its concrete type
 */
void BotMeme::Destroy( BotMeme *meme )
{
	if ( meme == nullptr )
		return;

	switch ( meme->m_kind )
	{
	case Kind::Help:	delete static_cast< BotHelpMeme * >( meme );	break;
	case Kind::BombsiteStatus:	delete static_cast< BotBombsiteStatusMeme * >( meme );	break;
	case Kind::BombStatus:	delete static_cast< BotBombStatusMeme * >( meme );	break;
	case Kind::Follow:	delete static_cast< BotFollowMeme * >( meme );	break;
	case Kind::DefendHere:	delete static_cast< BotDefendHereMeme * >( meme );	break;
	case Kind::WhereBomb:	delete static_cast< BotWhereBombMeme * >( meme );	break;
	case Kind::RequestReport:	delete static_cast< BotRequestReportMeme * >( meme );	break;
	case Kind::AllHostagesGone:	delete static_cast< BotAllHostagesGoneMeme * >( meme );	break;
	case Kind::HostageBeingTaken:	delete static_cast< BotHostageBeingTakenMeme * >( meme );	break;
	case Kind::HeardNoise:	delete static_cast< BotHeardNoiseMeme * >( meme );	break;
	case Kind::WarnSniper:	delete static_cast< BotWarnSniperMeme * >( meme );	break;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Transmit meme to other bots
 */
void BotMeme::Transmit( CCSBot *sender ) const
{
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer *player = static_cast<CCSPlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

//		if (FNullEnt( player->pev ))
//			continue;

//		if (FStrEq( STRING( player->pev->netname ), "" ))
//			continue;

		// skip self
		if (sender == player)
			continue;

		// ignore dead humans
		if (!player->IsBot() && !player->IsAlive())
			continue;

		// ignore enemies, since we can't hear them talk
		if (sender && sender->IsOtherEnemy( player ))
			continue;

		// if not a bot, fail the test
		if (!player->IsBot())
			continue;

		CCSBot *bot = ToCSBot( player );

		if ( !bot )
			continue;

		// allow bot to interpret our meme
		Interpret( sender, bot );		
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate called for help - respond
 */
void BotHelpMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	const float maxHelpRange = 3000.0f;		// 2000
	receiver->RespondToHelpRequest( sender, m_place, maxHelpRange );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about a bombsite
 */
void BotBombsiteStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	// remember this bombsite's status
	if (m_status == CLEAR)
		receiver->GetGameState()->ClearBombsite( m_zoneIndex );
	else
		receiver->GetGameState()->MarkBombsiteAsPlanted( m_zoneIndex );

	// if we were heading to the just-cleared bombsite, pick another one to search
	// if our target bombsite wasn't cleared, will will continue going to it, 
	// because GetNextBombsiteToSearch() will return the same zone (since its not cleared)
	// if the bomb was planted, we will head to that bombsite
	if (receiver->GetTask() == CCSBot::FIND_TICKING_BOMB)
	{
		receiver->Idle();
		receiver->GetChatter()->Affirmative();
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about the bomb
 */
void BotBombStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	// update our gamestate based on teammate's report
	switch( m_state )
	{
		case CSGameState::MOVING:
			receiver->GetGameState()->UpdateBomber( m_pos );

			// if we are hunting and see no enemies, respond
			if (!receiver->IsRogue() && receiver->IsHunting() && receiver->GetNearbyEnemyCount() == 0)
				receiver->RespondToHelpRequest( sender, TheNavMesh->GetPlace( m_pos ) );

			break;

		case CSGameState::LOOSE:
			receiver->GetGameState()->UpdateLooseBomb( m_pos );

			if (receiver->GetTask() == CCSBot::GUARD_BOMB_ZONE)
			{
				receiver->Idle();
				receiver->GetChatter()->Affirmative();		
			}
			break;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked that we follow him
 */
void BotFollowMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if (receiver->IsRogue())
		return;

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	// if we are too far away, ignore
	// compute actual travel distance
	Vector senderOrigin = GetCentroid( sender );
	PathCost cost( receiver );
	float travelDistance = NavAreaTravelDistance( receiver->GetLastKnownArea(), 
												  TheNavMesh->GetNearestNavArea( senderOrigin ),
												  cost );
	if (travelDistance < 0.0f)
		return;

	const float tooFar = 1000.0f;
	if (travelDistance > tooFar)
		return;

	// begin following
	receiver->Follow( sender );

	// acknowledge
	receiver->GetChatter()->Say( "CoveringFriend" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to defend a place
 */
void BotDefendHereMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if (receiver->IsRogue())
		return;

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	Place place = TheNavMesh->GetPlace( m_pos );
	if (place != UNDEFINED_PLACE)
	{
		// pick a random hiding spot in this place
		const Vector *spot = FindRandomHidingSpot( receiver, place, receiver->IsSniper() );
		if (spot)
		{
			receiver->SetTask( CCSBot::HOLD_POSITION );
			receiver->Hide( *spot );
			return;
		}
	}

	// hide nearby
	receiver->SetTask( CCSBot::HOLD_POSITION );
	receiver->Hide( TheNavMesh->GetNearestNavArea( m_pos ) );

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked where the bomb is planted
 */
void BotWhereBombMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	int zone = receiver->GetGameState()->GetPlantedBombsite();

	if (zone != CSGameState::UNKNOWN)
		receiver->GetChatter()->FoundPlantedBomb( zone );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to report in
 */
void BotRequestReportMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->ReportingIn();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us all the hostages are gone
 */
void BotAllHostagesGoneMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetGameState()->AllHostagesGone();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us a CT is talking to a hostage
 */
void BotHostageBeingTakenMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetGameState()->HostageWasTaken();

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	receiver->Idle();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate heard a noise, so we shouldn't report noises for a while
 */
void BotHeardNoiseMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->FriendHeardNoise();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate warned about snipers, so we shouldn't warn again for awhile
 */
void BotWarnSniperMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->FriendSpottedSniper();
}

//---------------------------------------------------------------------------------------------------------------
BotSpeakable::BotSpeakable()
{
	m_phrase = nullptr;
}

//---------------------------------------------------------------------------------------------------------------
BotSpeakable::~BotSpeakable()
{
	if ( m_phrase )
	{
		delete[] m_phrase;
		m_phrase = nullptr;
	}
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotPhrase::BotPhrase( bool isPlace )
{
	m_name = nullptr;
	m_place = UNDEFINED_PLACE;
	m_isPlace = isPlace;
	m_radioEvent = RADIO_INVALID;
	m_isImportant = false;
	ClearCriteria();
	m_numVoiceBanks = 0;
	InitVoiceBank( 0 );
}

BotPhrase::~BotPhrase()
{
	for( int bank=0; bank<std::ssize( m_voiceBank ); ++bank )
	{
		for( int speakable=0; speakable<std::ssize( *m_voiceBank[bank] ); ++speakable )
		{
			delete (*m_voiceBank[bank])[speakable];
		}
		delete m_voiceBank[bank];
	}

	if ( m_name )
		delete [] m_name;
}

void BotPhrase::InitVoiceBank( int bankIndex )
{
	while ( m_numVoiceBanks <= bankIndex )
	{
		m_count.push_back( 0 );
		m_index.push_back( 0 );
		m_voiceBank.push_back( new BotSpeakableVector );
		++m_numVoiceBanks;
	}
}

/**
 * Return a random speakable - avoid repeating
 */
char *BotPhrase::GetSpeakable( int bankIndex, float *duration ) const
{
	if (bankIndex < 0 || bankIndex >= m_numVoiceBanks || m_count[bankIndex] == 0)
	{
		if (duration)
			*duration = 0.0f;

		return nullptr;
	}

	// find phrase that meets the current criteria
	int start = m_index[bankIndex];
	while(true)
	{
		BotSpeakableVector *speakables = m_voiceBank[bankIndex];
		int& index = m_index[bankIndex];
		const BotSpeakable *speak = (*speakables)[index++];

		if (m_index[bankIndex] >= m_count[bankIndex])
			m_index[bankIndex] = 0;

		// check place criteria
		// if this speakable has a place criteria, it must match to be used
		// speakables with Place of ANY will match any place
		// speakables with a specific Place will only be used if Place matches
		// speakables with Place of UNDEFINED only match Place of UNDEFINED
		if (speak->m_place == ANY_PLACE || speak->m_place == m_placeCriteria)
		{
			// check count criteria
			// if this speakable has a count criteria, it must match to be used
			// if this speakable does not have a count criteria, we dont care what the count is set to
			if (speak->m_count == UNDEFINED_COUNT || speak->m_count == std::min<CountCriteria>( m_countCriteria, COUNT_MANY ))
			{
				if (duration)
					*duration = speak->m_duration;

				return speak->m_phrase;
			}
		}

		// check if we exhausted all speakables
		if (m_index[bankIndex] == start)
		{
			if (duration)
				*duration = 0.0f;

			return nullptr;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Randomly shuffle the speakable order
 */
void BotPhrase::Randomize( void )
{
	for ( int bank = 0; bank < std::ssize( m_voiceBank ); ++bank )
	{
		BotSpeakableVector *speakables = m_voiceBank[bank];
		if ( std::ssize( *speakables ) == 1 )
			continue;

		// A simple shuffle: for each array index, swap it with a random index
		for ( int index = 0; index < std::ssize( *speakables ); ++index )
		{
			int newIndex = RandomInt( 0, std::ssize( *speakables )-1 );

			BotSpeakable *speakable = (*speakables)[index];
			(*speakables)[index] = (*speakables)[newIndex];
			(*speakables)[newIndex] = speakable;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotPhraseManager *TheBotPhrases = nullptr;

BotPhraseManager::BotPhraseManager( void )
{
	m_placeCount = 0;
}

/**
 * Invoked when map changes
 */
void BotPhraseManager::OnMapChange( void )
{
	m_placeCount = 0;
}

/**
 * Removes everything from memory
 */
void BotPhraseManager::Reset( void )
{
	int i;

	// free phrase resources
	for( i=0; i<std::ssize( m_list ); ++i )
	{
		delete m_list[i];
	}

	for( i=0; i<std::ssize( m_placeList ); ++i )
	{
		delete m_placeList[i];
	}

	m_list.clear();
	m_placeList.clear();

	m_painPhrase = nullptr;
	m_agreeWithPlanPhrase = nullptr;
}

/**
 * Invoked when the round resets 
 */
void BotPhraseManager::OnRoundRestart( void )
{
	// effectively reset all interval timers
	m_placeCount = 0;

	// shuffle all the speakables
	int i;
	for( i=0; i<std::ssize( m_placeList ); ++i )
		m_placeList[i]->Randomize();

	for( i=0; i<std::ssize( m_list ); ++i )
		m_list[i]->Randomize();
}

BotChatterOutputType BotPhraseManager::GetOutputType( int voiceBank ) const
{
	if ( voiceBank >= 0 && voiceBank < std::ssize( m_output ) )
	{
		return m_output[voiceBank];
	}
	return BOT_CHATTER_RADIO;
}

/**
 * Initialize phrase system from database file
 */
bool BotPhraseManager::Initialize( const char *filename, int bankIndex )
{
	bool isDefault = (bankIndex == 0);

	FileHandle_t file = filesystem->Open( filename, "r" );
	if (!file)
	{
		Msg( "WARNING: Cannot access bot phrase database '%s'\n", filename );
		return false;
	}

	// BOTPORT: Redo file reading to avoid loading whole file into memory at once
	int phraseDataLength = filesystem->Size( filename );
	char *phraseDataFile = new char[ phraseDataLength ];
		
	int dataReadLength = filesystem->Read( phraseDataFile, phraseDataLength, file );	

	filesystem->Close( file );

	if ( dataReadLength > 0 )
	{
		// NULL-terminate based on the length read in, since Read() can transform \r\n to \n and
		// return fewer bytes than we were expecting.
		phraseDataFile[ dataReadLength - 1 ] = 0;
	}

	const char *phraseData = phraseDataFile;

	const int RadioPathLen = 128; // wav filenames need to be shorter than this to go over the net anyway.
	char baseDir[RadioPathLen] = "";
	char compositeFilename[RadioPathLen];

	//
	// Parse the BotChatter.db into BotPhrase collections
	//
	while( true )
	{
		phraseData = SharedParse( phraseData );
		if (!phraseData)
			break;

		char *token = SharedGetToken();

		if ( !stricmp( token, "Output" ) )
		{
			// get name of this output device
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
                Msg( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}

			while ( std::ssize( m_output ) <= bankIndex )
			{
				m_output.push_back( BOT_CHATTER_RADIO );
			}

			char *token = SharedGetToken();
			if ( !stricmp( token, "Voice" ) )
			{
				m_output[bankIndex] = BOT_CHATTER_VOICE;
			}
		}
		else if ( !stricmp( token, "BaseDir" ) )
		{
			// get name of this output device
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
                Msg( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}
			char *token = SharedGetToken();
			V_strncpy( baseDir, token, RadioPathLen );
			V_strncat( baseDir, "\\", RadioPathLen, -1 );
			baseDir[RadioPathLen-1] = 0;
		}
		else if (!stricmp( token, "Place" ) || !stricmp( token, "Chatter" ))
		{
			bool isPlace = (stricmp( token, "Place" )) ? false : true;

			// encountered a new phrase collection
			BotPhrase *phrase = nullptr;
			if ( isDefault )
			{
				phrase = new BotPhrase( isPlace );
			}

			// get name of this phrase
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
                Msg( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}
			if ( isDefault )
			{
				phrase->m_name = CloneString( SharedGetToken() );

				phrase->m_place = (isPlace) ? TheNavMesh->NameToPlace( phrase->m_name ) : UNDEFINED_PLACE;
			}
			else // look up the existing phrase
			{
				if ( isPlace )
				{
					phrase = const_cast<BotPhrase *>(GetPlace( SharedGetToken() ));
				}
				else
				{
					phrase = const_cast<BotPhrase *>(GetPhrase( SharedGetToken() ));
				}

				if ( !phrase )
				{
                    Msg( "Error parsing '%s' - phrase '%s' is invalid\n", filename, SharedGetToken() );
					delete [] phraseDataFile;
					return false;
				}
			}
			phrase->InitVoiceBank( bankIndex );

			PlaceCriteria placeCriteria = ANY_PLACE;
			CountCriteria countCriteria = UNDEFINED_COUNT;
			RadioType radioEvent = RADIO_INVALID;
			bool isImportant = false;

			// read attributes of this phrase
			while( true )
			{
				// get next token
				phraseData = SharedParse( phraseData );
				if (!phraseData)
				{
                    Msg( "Error parsing %s - expected 'End'\n", filename );
					delete [] phraseDataFile;
					return false;
				}
				token = SharedGetToken();

				// check for Place criteria
				if (!stricmp( token, "Place" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
                        Msg( "Error parsing %s - expected Place name\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					// update place criteria for subsequent speak lines
					// NOTE: this assumes places must be first in the chatter database

					// check for special identifiers
					if (!stricmp( "ANY", token ))
						placeCriteria = ANY_PLACE;
					else if (!stricmp( "UNDEFINED", token ))
						placeCriteria = UNDEFINED_PLACE;
					else
						placeCriteria = TheNavMesh->NameToPlace( token );

					continue;
				}

				// check for Count criteria
				if (!stricmp( token, "Count" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
                        Msg( "Error parsing %s - expected Count value\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					// update count criteria for subsequent speak lines
					if (!stricmp( token, "Many" ))
						countCriteria = COUNT_MANY;
					else 
						countCriteria = std::atoi( token );

					continue;
				}

				// check for radio equivalent
				if (!stricmp( token, "Radio" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
                        Msg( "Error parsing %s - expected radio event\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					RadioType event = NameToRadioEvent( token );
					if (event <= RADIO_START_1 || event >= RADIO_END)
					{
                        Msg( "Error parsing %s - invalid radio event '%s'\n", filename, token );
						delete [] phraseDataFile;
						return false;
					}

					radioEvent = event;

					continue;
				}

				// check for "important" flag
				if (!stricmp( token, "Important" ))
				{
					isImportant = true;
					continue;
				}

				// check for End delimiter
				if (!stricmp( token, "End" ))
					break;

				// found a phrase - add it to the collection
				BotSpeakable *speak = new BotSpeakable;
				if ( baseDir[0] )
				{
					V_snprintf( compositeFilename, RadioPathLen, "%s%s", baseDir, token );
					speak->m_phrase = CloneString( compositeFilename );
				}
				else
				{
					speak->m_phrase = CloneString( token );
				}
				speak->m_place = placeCriteria;
				speak->m_count = countCriteria;
				V_FixSlashes( speak->m_phrase );
				V_strlower( speak->m_phrase );

				// cstrike15 doesn't use bot chatter this way, the system redirects to RR, so no need to do anything other than slam this
				speak->m_duration = 1.0f;
				BotSpeakableVector * speakables = phrase->m_voiceBank[ bankIndex ];
				speakables->push_back( speak );

				++phrase->m_count[ bankIndex ];
			}

			if ( isDefault )
			{
				phrase->m_radioEvent = radioEvent;
				phrase->m_isImportant = isImportant;
			}

			// add phrase collection to the appropriate master list
			if (isPlace)
				m_placeList.push_back( phrase );
			else
				m_list.push_back( phrase );
		}
	}

	delete [] phraseDataFile;

	m_painPhrase = GetPhrase( "Pain" );
	m_agreeWithPlanPhrase = GetPhrase( "AgreeWithPlan" );

	return true;
}

BotPhraseManager::~BotPhraseManager()
{
	Reset();
}

/**
 * Given a name, return the associated phrase collection
 */
const BotPhrase *BotPhraseManager::GetPhrase( const char *name ) const
{
	for( int i=0; i<std::ssize( m_list ); ++i )
	{
		if (!stricmp( m_list[i]->m_name, name ))
			return m_list[i]; 
	}

	//Msg( "GetPhrase: ERROR - Invalid phrase '%s'\n", name );

	return nullptr;
}

/**
 * Given an id, return the associated phrase collection
 * @todo Store phrases in a vector to make this fast
 */
/*
const BotPhrase *BotPhraseManager::GetPhrase( unsigned int place ) const
{
	for( BotPhraseList::const_iterator iter = m_list.begin(); iter != m_list.end(); ++iter )
	{
		const BotPhrase *phrase = *iter;
		if (phrase->m_place == id)
			return phrase; 
	}

	Msg( "GetPhrase: ERROR - Invalid phrase id #%d\n", id );
	return NULL;
}
*/

/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( const char *name ) const
{
	if (name == nullptr)
		return nullptr;

	for( int i=0; i<std::ssize( m_placeList ); ++i )
	{
		if (!stricmp( m_placeList[i]->m_name, name ))
			return m_placeList[i];
	}

	return nullptr;
}

/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( PlaceCriteria place ) const
{
	if (place == UNDEFINED_PLACE)
		return nullptr;

	for( int i=0; i<std::ssize( m_placeList ); ++i )
	{
		if (m_placeList[i]->m_place == place)
			return m_placeList[i];
	}

	return nullptr;
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotStatement::BotStatement( BotChatterInterface *chatter, BotStatementType type, float expireDuration )
{
	m_chatter = chatter;

	m_next = nullptr;
	m_prev = nullptr;
	m_timestamp = gpGlobals->curtime;
	m_speakTimestamp = 0.0f;

	m_type = type;
	m_subject = UNDEFINED_SUBJECT;
	m_place = UNDEFINED_PLACE;
	m_meme = nullptr;

	m_startTime = gpGlobals->curtime;
	m_expireTime = gpGlobals->curtime + expireDuration;
	m_isSpeaking = false;

	m_nextTime = 0.0f;
	m_index = -1;
	m_count = 0;

	m_conditionCount = 0;
}

BotStatement::~BotStatement() 
{
    BotMeme::Destroy( m_meme );
}

//---------------------------------------------------------------------------------------------------------------
CCSBot *BotStatement::GetOwner( void ) const
{ 
	return m_chatter->GetOwner(); 
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Attach a meme to this statement, to be transmitted to other friendly bots when spoken
 */
void BotStatement::AttachMeme( BotMeme *meme )
{
	m_meme = meme;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Add a conditions that must be true for the statement to be spoken
 */
void BotStatement::AddCondition( ConditionType condition )
{
	if (m_conditionCount < MAX_BOT_CONDITIONS)
		m_condition[ m_conditionCount++ ] = condition;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is "important" and not personality chatter
 */
bool BotStatement::IsImportant( void ) const
{
	// if a statement contains any important phrases, it is important
	for( int i=0; i<m_count; ++i )
	{
		if (m_statement[i].isPhrase && m_statement[i].phrase->IsImportant())
			return true;

		// hack for now - phrases with enemy counts are important
		if (!m_statement[i].isPhrase && m_statement[i].context == BotStatement::CURRENT_ENEMY_COUNT)
			return true;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Verify all attached conditions 
 */
bool BotStatement::IsValid( void ) const
{
	for( int i=0; i<m_conditionCount; ++i )
	{
		switch( m_condition[i] )
		{
			case IS_IN_COMBAT:
			{
				// Don't speak in combat, unless we're playing a coop mission
				if ( !GetOwner()->IsAttacking() && !CSGameRules()->IsPlayingCoopMission() )
					return false;
				break;
			}

/*
			case RADIO_SILENCE:
			{
				if (GetOwner()->GetChatter()->GetRadioSilenceDuration() < 10.0f)
					return false;
				break;
			}
*/

			case ENEMIES_REMAINING:
			{
				if (GetOwner()->GetEnemiesRemaining() == 0)
					return false;
				break;
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is essentially the same as the given one
 */
bool BotStatement::IsRedundant( const BotStatement *say ) const
{
	// special cases
	if (GetType() == REPORT_MY_PLAN ||
			GetType() == REPORT_REQUEST_HELP ||
			GetType() == REPORT_CRITICAL_EVENT ||
			GetType() == REPORT_ACKNOWLEDGE)
		return false;

	// check if topics are different
	if (say->GetType() != GetType())
		return false;

	if (!say->HasPlace() && !HasPlace() && !say->HasSubject() && !HasSubject())
	{
		// neither has place or subject, so they are the same
		return true;
	}

	// check if subject matter is the same
	if (say->HasPlace() && HasPlace() && say->GetPlace() == GetPlace())
	{
		// talking about the same place
		return true;
	}

	if (say->HasSubject() && HasSubject() && say->GetSubject() == GetSubject())
	{
		// talking about the same player
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is no longer appropriate to say
 */
bool BotStatement::IsObsolete( void ) const
{
	// if the round is over, the only things we should say are emotes
	if (GetOwner()->GetGameState()->IsRoundOver())
	{
		if (m_type != REPORT_EMOTE)
			return true;
	}

	// If we're wanting to say "I lost him" but we've spotted another enemy,
	// we no longer need to report losing someone.
	if ( GetOwner()->GetChatter()->SeesAtLeastOneEnemy() && m_type == REPORT_ENEMY_LOST )
	{
		return true;
	}

	// check if statement lifetime has expired
	return (gpGlobals->curtime > m_expireTime);
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Possibly change what were going to say base on what teammate is saying
 */
void BotStatement::Convert( const BotStatement *say )
{
	if (GetType() == REPORT_MY_PLAN && say->GetType() == REPORT_MY_PLAN)
	{
		const BotPhrase *meToo = TheBotPhrases->GetAgreeWithPlanPhrase();

		// don't reconvert
		if (m_statement[0].phrase == meToo)
			return;

		// if our plans are the same, change our statement to "me too"
		if (m_statement[0].phrase == say->m_statement[0].phrase)
		{
			if (m_place == say->m_place)
			{
				// same plan at the same place - convert to "me too"
				m_statement[0].phrase = meToo;
				m_startTime = gpGlobals->curtime + RandomFloat( 0.5f, 1.0f );
			}
			else
			{
				// same plan at different place - wait a bit to allow others to respond "me too"
				m_startTime = gpGlobals->curtime + RandomFloat( 3.0f, 4.0f );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotStatement::AppendPhrase( const BotPhrase *phrase )
{
	if (phrase == nullptr)
		return;

	if (m_count < MAX_BOT_PHRASES)
	{
		m_statement[ m_count ].isPhrase = true;
		m_statement[ m_count++ ].phrase = phrase;
	}
}

/**
 * Special phrases that depend on the context
 */
void BotStatement::AppendPhrase( ContextType contextPhrase )
{
	if (m_count < MAX_BOT_PHRASES)
	{
		m_statement[ m_count ].isPhrase = false;
		m_statement[ m_count++ ].context = contextPhrase;
	}
}

/**
 * Say our statement
 * m_index refers to the phrase currently being spoken, or -1 if we havent started yet
 */
bool BotStatement::Update( void )
{
	CCSBot *me = GetOwner();

	// if all of our teammates are dead, the only non-redundant statements are emotes
	if (me->GetFriendsRemaining() == 0 && GetType() != REPORT_EMOTE && !me->HasHeavyArmor())
		return false;

	if (!m_isSpeaking)
	{
		m_isSpeaking = true;
		m_speakTimestamp = gpGlobals->curtime;
	}

	const int nMinEnemiesToNote = CSGameRules()->IsPlayingCoopMission() ? 1 : 3;

	// special case - context dependent delay
	if (m_index >= 0 && m_statement[ m_index ].context == ACCUMULATE_ENEMIES_DELAY)
	{
		// report if we see a lot of enemies, or if enough time has passed
		const float reportTime = 2.0f;		// 1
		if ( me->GetNearbyEnemyCount() > nMinEnemiesToNote || gpGlobals->curtime - m_speakTimestamp > reportTime )
		{
			// enough enemies have accumulated to expire this delay
			m_nextTime = 0.0f;
		}
	}

	if (gpGlobals->curtime > m_nextTime)
	{
		// check for end of statement
		if (++m_index == m_count)
		{
			// transmit any memes carried in this statement to our teammates
			if (m_meme)
				m_meme->Transmit( me );

			return false;
		}

		// start next part of statement
		float duration = 0.0f;
		const BotPhrase *phrase = nullptr;

		if (m_statement[ m_index ].isPhrase)
		{
			// normal phrase
			phrase = m_statement[ m_index ].phrase;
		}
		else
		{
			// context-dependant phrase
			switch( m_statement[ m_index ].context )
			{
				case CURRENT_ENEMY_COUNT:
				{
					if ( me->HasHeavyArmor() )
						break;

					int enemyCount = me->GetNearbyEnemyCount();
					const int nEnemySpottedThreshold = CSGameRules()->IsPlayingCoopMission() ? 0 : 1;

					// if we are outnumbered, ask for help
					if (enemyCount-1 > me->GetNearbyFriendCount())
					{
						phrase = TheBotPhrases->GetPhrase( "Help" );
						AttachMeme( new BotHelpMeme() );
					}
					else if ( enemyCount > nEnemySpottedThreshold )
					{
						phrase = TheBotPhrases->GetPhrase( "EnemySpotted" );
						phrase->SetCountCriteria( enemyCount );
					}
					break;
				}

				case REMAINING_ENEMY_COUNT:
				{
					if ( me->HasHeavyArmor() )
						break;

					static const char *speak[] = 
					{
						"NoEnemiesLeft", "OneEnemyLeft", "TwoEnemiesLeft", "ThreeEnemiesLeft"
					};

					int enemyCount = me->GetEnemiesRemaining();

					// dont report if there are lots of enemies left
					if (enemyCount < 0 || enemyCount > nMinEnemiesToNote )
					{
						phrase = nullptr;
					}
					else
					{
						phrase = TheBotPhrases->GetPhrase( speak[ enemyCount ] );
					}
					break;
				}

				case SHORT_DELAY:
				{
					m_nextTime = gpGlobals->curtime + RandomFloat( 0.1f, 0.5f );
					return true;
				}

				case LONG_DELAY:
				{
					m_nextTime = gpGlobals->curtime + RandomFloat( 1.0f, 2.0f );
					return true;
				}

				case ACCUMULATE_ENEMIES_DELAY:
				{
					// wait until test becomes true
					m_nextTime = 99999999.9f;
					return true;
				}
			}
		}

		if (phrase)
		{
			// if chatter system is in "standard radio" mode, send the equivalent radio command
			if (me->GetChatter()->GetVerbosity() == BotChatterInterface::RADIO)
			{
				RadioType radioEvent = phrase->GetRadioEquivalent();
				if (radioEvent == RADIO_INVALID)
				{
					// skip directly to the next phrase
					m_nextTime = 0.0f;
				}
				else
				{
					// use the standard radio
					me->GetChatter()->ResetRadioSilenceDuration();
					me->SendRadioMessage( radioEvent );
					duration = 2.0f;
				}
			}
			else
			{
				// set place criteria
				phrase->SetPlaceCriteria( m_place );

				bool sayIt = true;

				if (phrase->IsPlace())
				{
					// don't repeat the place if someone just mentioned it not too long ago
					float timeSince = TheBotPhrases->GetPlaceStatementInterval( phrase->GetPlace() );
					const float minRepeatTime = 20.0f;		// 30
					if (timeSince < minRepeatTime)
					{
						sayIt = false;
					}
					else
					{
						TheBotPhrases->ResetPlaceStatementInterval( phrase->GetPlace() );
					}
				}

				if (sayIt)
				{
					bool bSpoke = false;
					if ( bot_chatter_use_rr.GetBool() )
					{
						// Pipe the bot chatter through the response rules system
						int nPlace = phrase->IsPlace() ? phrase->GetPlace() : phrase->GetPlaceCriteria();
						const char *place = TheNavMesh->PlaceToName( nPlace );
						CountCriteria count = phrase->GetCountCriteria();

						AI_CriteriaSet &botCriteria = phrase->GetCriteriaSet();
						if ( count != UNDEFINED_COUNT )
						{
							botCriteria.AppendCriteria( "count", count );
						}
						if ( place )
						{
							botCriteria.AppendCriteria( "place", place );
						}
						if ( CSGameRules()->IsPlayingCoopMission()  )
						{
							botCriteria.AppendCriteria( "gamemode", "coop" );
						}
						if ( me->HasHeavyArmor() )
						{
							botCriteria.AppendCriteria( "isheavy", 1 );
						}

						// TODO: Need voice pitch as a criteria? 
						// me->GetProfile()->GetVoicePitch()

						bSpoke = me->SpeakAudioResponseRules( phrase->GetName(), &botCriteria, duration + 1.0f );
					}
					else
					{
						const char *filename = phrase->GetSpeakable( me->GetProfile()->GetVoiceBank(), &duration );
						// Msg( "%s: Radio( '%s' )\n", STRING( me->pev->netname ), filename );
						if ( filename )
						{
							me->SpeakAudio( filename, duration + 1.0f, me->GetProfile()->GetVoicePitch() );
							bSpoke = true;
						}
					}

					if ( !bSpoke )
					{
						RadioType radioEvent = phrase->GetRadioEquivalent();
						if (radioEvent == RADIO_INVALID)
						{
							// skip directly to the next phrase
							m_nextTime = 0.0f;
						}
						else
						{
							// use the standard radio
							me->SendRadioMessage( radioEvent );
							me->GetChatter()->ResetRadioSilenceDuration();
							duration = 2.0f;
						}
					}
					/* BOTPORT: Wire up bot voice over IP
					else if ( g_engfuncs.pfnPlayClientVoice && TheBotPhrases->GetOutputType( me->GetProfile()->GetVoiceBank() ) == BOT_CHATTER_VOICE )
					{
						me->GetChatter()->ResetRadioSilenceDuration();
						g_engfuncs.pfnPlayClientVoice( me->entindex() - 1, filename );
					}
					*/
				}
			}

			const float gap = 0.1f;
			m_nextTime = gpGlobals->curtime + duration + gap;
		}
		else
		{
			// skip directly to the next phrase
			m_nextTime = 0.0f;
		}
	}
	
	return true;
}

/**
 * If this statement refers to a specific place, return that place
 * Places can be implicit in the statement, or explicitly defined
 */
unsigned int BotStatement::GetPlace( void ) const
{
	// return any explicitly set place if we have one
	if (m_place != UNDEFINED_PLACE)
		return m_place;

	// look for an implicit place in our statement
	for( int i=0; i<m_count; ++i )
		if (m_statement[i].isPhrase && m_statement[i].phrase->IsPlace())
			return m_statement[i].phrase->GetPlace();
	
	return 0;
}

/**
 * Return true if this statement has an associated count
 */
bool BotStatement::HasCount( void ) const
{
	for( int i=0; i<m_count; ++i )
		if (!m_statement[i].isPhrase && m_statement[i].context == CURRENT_ENEMY_COUNT)
			return true;

	return false;
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

CountdownTimer BotChatterInterface::m_encourageTimer;
IntervalTimer BotChatterInterface::m_radioSilenceInterval[ 2 ];

enum PitchHack 
{ 
	P_HI, 
	P_NORMAL, 
	P_LOW 
};

static int nextPitch = P_HI;

BotChatterInterface::BotChatterInterface( CCSBot *me, Kind kind )
{
	m_kind = kind;
	m_me = me;
	m_statementList = nullptr;

	switch( nextPitch )
	{
		case P_HI:
			m_pitch = RandomInt( 105, 110 );
			break;

		case P_NORMAL:
			m_pitch = RandomInt( 95, 105 );	
			break;

		case P_LOW:
			m_pitch = RandomInt( 85, 95 );
			break;
	}

	nextPitch = (nextPitch + 1) % 3;

	Reset();
}

//---------------------------------------------------------------------------------------------------------------
// Use BotChatterCoop's version for Kind::Coop, the default one otherwise
void BotChatterInterface::EnemySpotted( void )
{
	if ( m_kind == Kind::Coop )
		static_cast< BotChatterCoop * >( this )->EnemySpotted();
	else
		EnemySpottedDefault();
}

void BotChatterInterface::KilledMyEnemy( int victimID )
{
	if ( m_kind == Kind::Coop )
		static_cast< BotChatterCoop * >( this )->KilledMyEnemy( victimID );
	else
		KilledMyEnemyDefault( victimID );
}

void BotChatterInterface::EnemiesRemaining( void )
{
	if ( m_kind == Kind::Coop )
		static_cast< BotChatterCoop * >( this )->EnemiesRemaining();
	else
		EnemiesRemainingDefault();
}

void BotChatterInterface::CelebrateWin( void )
{
	if ( m_kind == Kind::Coop )
		static_cast< BotChatterCoop * >( this )->CelebrateWin();
	else
		CelebrateWinDefault();
}

//---------------------------------------------------------------------------------------------------------------
BotChatterInterface::~BotChatterInterface()
{
	// free pending statements
	BotStatement *next;
	for( BotStatement *msg = m_statementList; msg; msg = next )
	{
		next = msg->m_next;
		delete msg;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Reset to initial state
 */
void BotChatterInterface::Reset( void )
{
	BotStatement *msg, *nextMsg;

	// removing pending statements - except for those about the round results
	for( msg = m_statementList; msg; msg = nextMsg )
	{
		nextMsg = msg->m_next;

		if (msg->GetType() != REPORT_ROUND_END)
			RemoveStatement( msg );
	}

	m_seeAtLeastOneEnemy = false;
	m_timeWhenSawFirstEnemy = 0.0f;
	m_reportedEnemies = false;
	m_requestedBombLocation = false;

	ResetRadioSilenceDuration();

	m_needBackupInterval.Invalidate();
	m_spottedBomberInterval.Invalidate();
	m_spottedLooseBombTimer.Invalidate();
	m_heardNoiseTimer.Invalidate();
	m_scaredInterval.Invalidate();
	m_planInterval.Invalidate();
	m_encourageTimer.Invalidate();
	m_escortingHostageTimer.Invalidate();
	m_warnSniperTimer.Invalidate();
	m_heavyTauntTimer.Invalidate();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Register a statement for speaking
 */
void BotChatterInterface::AddStatement( BotStatement *statement, bool mustAdd )
{
	// don't add statements if bot chatter is shut off
	if (GetVerbosity() == OFF)
	{
		delete statement;
		return;	
	}

	// if we only want mission-critical radio chatter, ignore non-important phrases
	if (GetVerbosity() == MINIMAL && !statement->IsImportant())
	{
		delete statement;
		return;
	}

	// don't add statements if we're dead
	if (!m_me->IsAlive() && !mustAdd)
	{
		delete statement;
		return;
	}

	// don't add empty statements
	if (statement->m_count == 0)
	{
		delete statement;
		return;
	}

	// don't add statements that are redundant with something we're already waiting to say
	BotStatement *s;
	for( s=m_statementList; s; s = s->m_next )
	{
		if (statement->IsRedundant( s ))
		{
			m_me->PrintIfWatched( "I tried to say something I'm already saying.\n" );
			delete statement;
			return;
		}
	}

	// keep statements in order of start time

	// check list is empty
	if (m_statementList == nullptr)
	{
		statement->m_next = nullptr;
		statement->m_prev = nullptr;
		m_statementList = statement;
		return;
	}

	// list has at least one statement on it

	// insert into list in order
	BotStatement *earlier = nullptr;
	for( s=m_statementList; s; s = s->m_next )
	{
		if (s->GetStartTime() > statement->GetStartTime())
			break;

		earlier = s;
	}

	// insert just after "earlier"
	if (earlier)
	{
		if (earlier->m_next)
			earlier->m_next->m_prev = statement;

		statement->m_next = earlier->m_next;

		earlier->m_next = statement;
		statement->m_prev = earlier;
	}
	else
	{
		// insert at head
		statement->m_prev = nullptr;
		statement->m_next = m_statementList;
		m_statementList->m_prev = statement;
		m_statementList = statement;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Remove a statement
 */
void BotChatterInterface::RemoveStatement( BotStatement *statement )
{
	if (statement->m_next)
		statement->m_next->m_prev = statement->m_prev;

	if (statement->m_prev)
		statement->m_prev->m_next = statement->m_next;
	else
		m_statementList = statement->m_next;

	delete statement;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Track nearby enemy count and report enemy activity
 */
void BotChatterInterface::ReportEnemies( void )
{
	if (!m_me->IsAlive())
		return;

	if (m_me->GetNearbyEnemyCount() == 0)
	{
		m_seeAtLeastOneEnemy = false;
		m_reportedEnemies = false;
	}
	else if (!m_seeAtLeastOneEnemy)
	{
		m_seeAtLeastOneEnemy = true;
		m_timeWhenSawFirstEnemy = gpGlobals->curtime;
	}

	// determine whether we should report enemy activity
	if (!m_reportedEnemies && m_seeAtLeastOneEnemy)
	{
		// request backup if we're outnumbered
		if (m_me->IsOutnumbered() && NeedBackup())
		{
			m_reportedEnemies = true;
			return;
		}

		m_me->GetChatter()->EnemySpotted();
		m_reportedEnemies = true;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Invoked when we die
 */
void BotChatterInterface::OnDeath( void )
{
	if (IsTalking())
	{
		if (m_me->GetChatter()->GetVerbosity() == BotChatterInterface::MINIMAL ||
				m_me->GetChatter()->GetVerbosity() == BotChatterInterface::NORMAL)
		{
			// we've died mid-sentance - emit a gargle of pain
			const BotPhrase *pain = TheBotPhrases->GetPainPhrase();
			if (pain)
			{
				/*
				if ( g_engfuncs.pfnPlayClientVoice && TheBotPhrases->GetOutputType( m_me->GetProfile()->GetVoiceBank() ) == BOT_CHATTER_VOICE )
				{
					g_engfuncs.pfnPlayClientVoice( m_me->entindex() - 1, pain->GetSpeakable(m_me->GetProfile()->GetVoiceBank()) );
					m_me->GetChatter()->ResetRadioSilenceDuration();
				}
				else
				*/
				{
					if ( bot_chatter_use_rr.GetBool() )
					{
						AI_CriteriaSet botCriteria;
						if ( CSGameRules()->IsPlayingCoopMission() )
						{
							botCriteria.AppendCriteria( "gamemode", "coop" );
						}
						if ( m_me->HasHeavyArmor() )
						{
							botCriteria.AppendCriteria( "isheavy", 1 );
						}

						m_me->SpeakAudioResponseRules( "DeathCry", &botCriteria, 0.0f );
					}
					else
					{
						m_me->SpeakAudio( pain->GetSpeakable( m_me->GetProfile()->GetVoiceBank() ), 0.0f, m_me->GetProfile()->GetVoicePitch() );
					}
				}
			}
		}
	}

	// remove all of our statements
	Reset();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Process ongoing chatter for this bot
 */
void BotChatterInterface::Update( void )
{
	// report enemy activity
	ReportEnemies();

	// ask team to report in if we havent heard anything in awhile
	if (ShouldSpeak())
	{
		const float longTime = 30.0f;
		if (m_me->GetEnemiesRemaining() > 0 && GetRadioSilenceDuration() > longTime)
		{
			ReportIn();
		}
	}

	// speak if it is our turn
	BotStatement *say = GetActiveStatement();

	if (say)
	{
		// if our statement is active, speak it
		if (say->GetOwner() == m_me)
		{
			if (say->Update() == false)
			{
				// this statement is complete - destroy it
				RemoveStatement( say );
			}
		}
	}

	//
	// Process active statements.
	// Removed expired statements, re-order statements according to their relavence and importance
	// Remove redundant statements (ie: our teammates already said them)
	//
	const BotStatement *friendSay = GetActiveStatement();
	if (friendSay && friendSay->GetOwner() == m_me)
		friendSay = nullptr;

	BotStatement *nextSay;
	for( say = m_statementList; say; say = nextSay )
	{
		nextSay = say->m_next;

		// check statement conditions
		if (!say->IsValid())
		{
			RemoveStatement( say );
			continue;
		}
			
		// don't interrupt ourselves
		if (say->IsSpeaking())
			continue;

		// check for obsolete statements
		if (say->IsObsolete())
		{
			m_me->PrintIfWatched( "Statement obsolete - removing.\n" );
			RemoveStatement( say );
			continue;
		}

		// if a teammate is saying what we were going to say, dont repeat it
		if (friendSay)
		{
			// convert what we're about to say based on what our teammate is currently saying
			say->Convert( friendSay );

			// don't say things our teammates have just said
			if (say->IsRedundant( friendSay ))
			{
				// thie statement is redundant - destroy it
				//m_me->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
				m_me->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
				RemoveStatement( say );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Returns the statement that is being spoken, or is next to be spoken if no-one is speaking now
 */
BotStatement *BotChatterInterface::GetActiveStatement( void )
{
	// keep track of statement waiting longest to be spoken - it is next
	BotStatement *earliest = nullptr;
	float earlyTime = 999999999.9f;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer *player = static_cast<CCSPlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		// ignore dead humans
		if (!player->IsBot() && !player->IsAlive())
			continue;

		// ignore enemies, since we can't hear them talk
		if (player->IsOtherEnemy( m_me->entindex() ))
			continue;

		CCSBot *bot = ToCSBot( player );

		// if not a bot, fail the test
		/// @todo Check if human is currently talking
		if (!bot)
			continue;

		for( BotStatement *say = bot->GetChatter()->m_statementList; say; say = say->m_next )
		{
			// if this statement is currently being spoken, return it		
			if (say->IsSpeaking())
				return say;

			// keep track of statement that has been waiting longest to be spoken of anyone on our team
			if (say->GetStartTime() < earlyTime)
			{
				earlyTime = say->GetTimestamp();
				earliest = say;
			}
		}
	}

	// make sure it is time to start this statement
	if (earliest && earliest->GetStartTime() > gpGlobals->curtime)
		return nullptr;

	return earliest;
}

/**
 * Return true if we speaking makes sense now
 */
bool BotChatterInterface::ShouldSpeak( void ) const
{
	// don't talk to non-existent friends
	if (m_me->GetFriendsRemaining() == 0)
		return false;

	// if everyone is together, no need to tell them what's going on
	if (m_me->GetNearbyFriendCount() == m_me->GetFriendsRemaining())
		return false;

	return true;
}

//---------------------------------------------------------------------------------------------------------------
float BotChatterInterface::GetRadioSilenceDuration( void )
{
	return m_radioSilenceInterval[ m_me->GetTeamNumber() % 2 ].GetElapsedTime();
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::ResetRadioSilenceDuration( void )
{
	m_radioSilenceInterval[ m_me->GetTeamNumber() % 2 ].Reset(); 
}

//---------------------------------------------------------------------------------------------------------------
inline const BotPhrase *GetPlacePhrase( CCSBot *me )
{
	Place place = me->GetPlace();
	if (place != UNDEFINED_PLACE)
		return TheBotPhrases->GetPlace( place );

	return nullptr;
}

inline void SayWhere( BotStatement *say, Place place )
{
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );
}

/**
 * Report enemy sightings
 */
void BotChatterInterface::EnemySpottedDefault( void )
{
	// NOTE: This could be a few seconds out of date (enemy is in an adjacent place)
	Place place = m_me->GetEnemyPlace();

	BotStatement *say = new BotStatement( this, REPORT_VISIBLE_ENEMIES, 10.0f );

	// where are the enemies
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );

	// how many are there
	say->AppendPhrase( BotStatement::ACCUMULATE_ENEMIES_DELAY );
	say->AppendPhrase( BotStatement::CURRENT_ENEMY_COUNT );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * If a friend warned of snipers, don't warn again for awhile
 */
void BotChatterInterface::FriendSpottedSniper( void )
{
	m_warnSniperTimer.Start( 60.0f );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Warn of an enemy sniper
 */
void BotChatterInterface::SpottedSniper( void )
{
	if (!m_warnSniperTimer.IsElapsed())
	{
		return;
	}

	if (m_me->GetFriendsRemaining() == 0)
	{
		// no-one to warn
		return;
	}

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "SniperWarning" ) );
	say->AttachMeme( new BotWarnSniperMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Clear( Place place )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	SayWhere( say, place );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Request enemy activity report
 */
void BotChatterInterface::ReportIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "RequestReport" ) );
	say->AddCondition( BotStatement::RADIO_SILENCE );
	say->AttachMeme( new BotRequestReportMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Report our situtation
 */
void BotChatterInterface::ReportingIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	SayWhere( say, place );

	// what are we doing
	switch( m_me->GetTask() )
	{
		case CCSBot::PLANT_BOMB:
		{
			m_me->GetChatter()->GoingToPlantTheBomb( UNDEFINED_PLACE );
			break;
		}

		case CCSBot::DEFUSE_BOMB:
		{
			m_me->GetChatter()->Say( "DefusingBomb" );
			break;
		}

		case CCSBot::GUARD_LOOSE_BOMB:
		{
			if (TheCSBots()->GetLooseBomb())
			{
				say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
				say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, TheCSBots()->GetLooseBomb()->GetAbsOrigin() ) );
			}
			break;
		}

		case CCSBot::GUARD_HOSTAGES:
		{
			m_me->GetChatter()->GuardingHostages( UNDEFINED_PLACE, !m_me->IsAtHidingSpot() );
			break;
		}

		case CCSBot::GUARD_HOSTAGE_RESCUE_ZONE:
		{
			m_me->GetChatter()->GuardingHostageEscapeZone( !m_me->IsAtHidingSpot() );
			break;
		}

		case CCSBot::COLLECT_HOSTAGES:
		{
			break;
		}

		case CCSBot::RESCUE_HOSTAGES:
		{
			m_me->GetChatter()->EscortingHostages();
			break;
		}

		case CCSBot::GUARD_VIP_ESCAPE_ZONE:
		{
			break;
		}

	}

	// what do we see
	if (m_me->IsAttacking())
	{
		if (m_me->IsOutnumbered())
		{
			// in trouble in a firefight
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
			say->AttachMeme( new BotHelpMeme( place ) );
		}
		else
		{
			// battling enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "InCombat" ) );
		}
	}
	else
	{
		// not in combat, start our report a little later
		say->SetStartTime( gpGlobals->curtime + 2.0f );

		const float recentTime = 10.0f;
		if (m_me->GetEnemyDeathTimestamp() < recentTime && m_me->GetEnemyDeathTimestamp() >= m_me->GetTimeSinceLastSawEnemy() + 0.5f)
		{
			// recently saw an enemy die
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemyDown" ) );
		}
		else if (m_me->GetTimeSinceLastSawEnemy() < recentTime)
		{
			// recently saw an enemy
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemySpotted" ) );
		}
		else
		{
			// haven't seen enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
		}
	}
	
	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
bool BotChatterInterface::NeedBackup( void )
{
	const float minRequestInterval = 10.0f;
	if (m_needBackupInterval.IsLessThen( minRequestInterval ))
		return false;

	if ( m_me->HasHeavyArmor() )
		return false;

	m_needBackupInterval.Reset();

	if (m_me->GetFriendsRemaining() == 0)
	{
		// we're all alone...
		Scared();
		return true;
	}
	else
	{
		// ask friends for help
		BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

		// where are we
		Place place = m_me->GetPlace();
		SayWhere( say, place );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
		say->AttachMeme( new BotHelpMeme( place ) );

		AddStatement( say );
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PinnedDown( void )
{
	// this is a form of "need backup"
	const float minRequestInterval = 10.0f;
	if (m_needBackupInterval.IsLessThen( minRequestInterval ))
		return;

	if ( m_me->HasHeavyArmor() )
		return;

	m_needBackupInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PinnedDown" ) );
	say->AttachMeme( new BotHelpMeme( place ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * If a friend said that they heard something, we don't want to say something similar
 * for a while.
 */
void BotChatterInterface::FriendHeardNoise( void )
{
	m_heardNoiseTimer.Start( 20.0f );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HeardNoise( const Vector &pos )
{
	if (TheCSBots()->IsRoundOver())
		return;

	if (m_heardNoiseTimer.IsElapsed())
	{
		// throttle frequency
		m_heardNoiseTimer.Start( 20.0f );

		// make rare, since many teammates may try to say this
		if (RandomFloat( 0, 100 ) < 33)
		{
			BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 5.0f );

			say->AppendPhrase( TheBotPhrases->GetPhrase( "HeardNoise" ) );
			say->SetPlace( TheNavMesh->GetPlace( pos ) );
			say->AttachMeme( new BotHeardNoiseMeme() );

			AddStatement( say );
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledMyEnemyDefault( int victimID )
{
	// only report if we killed the last enemy in the area
	if (m_me->GetNearbyEnemyCount() <= 1)
		return;

	BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
	say->SetSubject( victimID );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EnemiesRemainingDefault( void )
{
	// only report if we killed the last enemy in the area
	if (m_me->GetNearbyEnemyCount() > 1)
		return;

	BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
	say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 2.0f, 4.0f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Affirmative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Negative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "Negative" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GoingToPlantTheBomb( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "GoingToPlantBomb" ) );
	say->SetPlace( place );
	say->AttachMeme( new BotFollowMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PlantingTheBomb( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantingBomb" ) );
	say->SetPlace( place );

	Vector myOrigin = GetCentroid( m_me );
	say->AttachMeme( new BotDefendHereMeme( myOrigin ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TheyPickedUpTheBomb( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	// if we already know the bomb is not loose, this is old news
	if (!m_me->GetGameState()->IsBombLoose())
		return;

	// update our gamestate - use our own position for now
	const Vector &myOrigin = GetCentroid( m_me );
	m_me->GetGameState()->UpdateBomber( myOrigin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "TheyPickedUpTheBomb" ) );

	say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, myOrigin ) ); 

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedBomber( CBasePlayer *bomber )
{
	const Vector &bomberOrigin = GetCentroid( bomber );

	if (m_me->GetGameState()->IsBombMoving())
	{
		// if we knew where the bomber was, this is old news
		const Vector *bomberPos = m_me->GetGameState()->GetBombPosition();
		const float closeRangeSq = 1000.0f * 1000.0f;
		if (bomberPos && (bomberOrigin - *bomberPos).LengthSqr() < closeRangeSq)
			return;
	}

	// update our gamestate
	m_me->GetGameState()->UpdateBomber( bomberOrigin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomber
	Place place = TheNavMesh->GetPlace( bomberOrigin );
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedBomber" ) );

	say->SetSubject( bomber->entindex() );

	//say->AttachMeme( new BotHelpMeme( place ) );
	say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, bomberOrigin ) ); 

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedLooseBomb( CBaseEntity *bomb )
{
	if (TheCSBots()->IsRoundOver())
		return;

	// if we already know the bomb is loose, this is old news
	if (m_me->GetGameState()->IsBombLoose())
		return;

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( bomb->GetAbsOrigin() );

	if (m_spottedLooseBombTimer.IsElapsed())
	{
		// throttle frequency 
		m_spottedLooseBombTimer.Start( 10.0f );

		// tell our teammates
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

		// where is the bomb
		Place place = TheNavMesh->GetPlace( bomb->GetAbsOrigin() );
		SayWhere( say, place );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedLooseBomb" ) );

		if (TheCSBots()->GetLooseBomb())
			say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, bomb->GetAbsOrigin() ) );

		AddStatement( say );
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingLooseBomb( CBaseEntity *bomb )
{
	// if we already know the bomb is loose, this is old news
//	if (m_me->GetGameState()->IsBombLoose())
//		return;

	if (TheCSBots()->IsRoundOver() || !bomb)
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( bomb->GetAbsOrigin() );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomb
	Place place = TheNavMesh->GetPlace( bomb->GetAbsOrigin() );
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );

	if (TheCSBots()->GetLooseBomb())
		say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, bomb->GetAbsOrigin() ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::RequestBombLocation( void )
{
	// only ask once per round
	if (m_requestedBombLocation)
		return;

	m_requestedBombLocation = true;

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "WhereIsTheBomb" ) );

	say->AttachMeme( new BotWhereBombMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::BombsiteClear( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if (zone == nullptr)
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	SayWhere( say, TheNavMesh->GetPlace( zone->m_center ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "BombsiteClear" ) );

	say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, BotBombsiteStatusMeme::CLEAR ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FoundPlantedBomb( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if (zone == nullptr)
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantedBombPlace" ) );
	say->SetPlace( TheNavMesh->GetPlace( zone->m_center ) );

	say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, BotBombsiteStatusMeme::PLANTED ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Scared( void )
{
	const float minInterval = 10.0f;
	if (m_scaredInterval.IsLessThen( minInterval ))
		return;

	m_scaredInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 1.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "ScaredEmote" ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::CelebrateWinDefault( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );

	// wait a bit before speaking
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 2.0f, 5.0f ) );

	const float quickRound = 45.0f;

	if (m_me->GetFriendsRemaining() == 0)
	{
		// we were the last man standing
		if (TheCSBots()->GetElapsedRoundTime() < quickRound)
			say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRoundQuickly" ) );
		else if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
			say->AppendPhrase( TheBotPhrases->GetPhrase( "LastManStanding" ) );
	}
	else
	{
		if (TheCSBots()->GetElapsedRoundTime() < quickRound)
		{
			if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
				say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRoundQuickly" ) );
		}
		else if (RandomFloat( 0.0f, 100.0f ) < 10.0f)
		{
			say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRound" ) );
		}
	}

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::AnnouncePlan( const char *phraseName, Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( phraseName ) );
	say->SetPlace( place );

	// wait at least a short time after round start
	say->SetStartTime( TheCSBots()->GetRoundStartTime() + RandomFloat( 2.0, 3.0f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingBombsite( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	AnnouncePlan( "GoingToDefendBombsite", place );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostages( Place place, bool isPlan )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	if (isPlan)
		AnnouncePlan( "GoingToGuardHostages", place );
	else
		Say( "GuardingHostages" );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostageEscapeZone( bool isPlan )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	if (isPlan)
		AnnouncePlan( "GoingToGuardHostageEscapeZone", UNDEFINED_PLACE );
	else
		Say( "GuardingHostageEscapeZone" );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesBeingTaken( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesBeingTaken" ) );
	say->AttachMeme( new BotHostageBeingTakenMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesTaken( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesTaken" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TalkingToHostages( void )
{
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EscortingHostages( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	if (m_escortingHostageTimer.IsElapsed())
	{
		// throttle frequency
		m_escortingHostageTimer.Start( 10.0f );

		BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 5.0f );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "EscortingHostages" ) );

		AddStatement( say );
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostageDown( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostageDown" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Encourage( const char *phraseName, float repeatInterval, float lifetime )
{
	if (m_encourageTimer.IsElapsed())
	{
		Say( phraseName, lifetime );
		m_encourageTimer.Start( repeatInterval );
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledFriend( void )
{
	BotStatement *say = new BotStatement( this, REPORT_KILLED_FRIEND, 2.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledFriend" ) );

	// give them time to react
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 0.5f, 1.0f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FriendlyFire( const char *pDmgType )
{
	if ( !mp_friendlyfire.GetBool() )
		return;

	BotStatement *say = new BotStatement( this, REPORT_FRIENDLY_FIRE, 1.0f );

	AI_CriteriaSet botCriteria;
	if ( pDmgType )
	{
		botCriteria.AppendCriteria( "damagetype", pDmgType );
	}

	const BotPhrase *pPhrase = TheBotPhrases->GetPhrase( "FriendlyFire" );
	pPhrase->SetCriteriaSet( botCriteria );

	say->AppendPhrase( pPhrase );

	// give them time to react
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 0.3f, 0.5f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::DoPhoenixHeavyWakeTaunt( void )
{
	if ( m_heavyTauntTimer.IsElapsed() )
	{
 		m_heavyTauntTimer.Start( RandomInt( 9, 15 ) );

		Say( "HeavyWakeTaunt" );
	}
}

void BotPhrase::ClearCriteria( void ) const
{
	m_placeCriteria = ANY_PLACE;
	m_countCriteria = UNDEFINED_COUNT;
}

void BotPhrase::SetPlaceCriteria( PlaceCriteria place ) const
{
	m_placeCriteria = place;
}

void BotPhrase::SetCountCriteria( CountCriteria count ) const
{
	m_countCriteria = count;
}

void BotPhrase::SetCriteriaSet( AI_CriteriaSet &criteria ) const
{
	m_contexts.Reset();
	m_contexts.Merge( &criteria );
}

int BotPhraseManager::FindPlaceIndex( Place where ) const
{
	for( int i=0; i<m_placeCount; ++i )
		if (m_placeStatementHistory[i].placeID == where)
			return i;

	// no such place - allocate it
	if (m_placeCount < MAX_PLACES_PER_MAP)
	{
		m_placeStatementHistory[ m_placeCount ].placeID = where;
		m_placeStatementHistory[ m_placeCount ].timer.Invalidate();
		++m_placeCount;
		return m_placeCount-1;
	}

	// place directory is full
	return -1;
}

float BotPhraseManager::GetPlaceStatementInterval( Place place ) const
{
	int index = FindPlaceIndex( place );

	if (index < 0)
		return 999999.9f;

	if (index >= m_placeCount)
		return 999999.9f;

	return m_placeStatementHistory[ index ].timer.GetElapsedTime();
}

void BotPhraseManager::ResetPlaceStatementInterval( Place place )
{
	int index = FindPlaceIndex( place );

	if (index < 0)
		return;

	if (index >= m_placeCount)
		return;

	// update entry
	m_placeStatementHistory[ index ].timer.Reset();
}

BotChatterInterface::VerbosityType BotChatterInterface::GetVerbosity( void ) const
{
	const char *string = cv_bot_chatter.GetString();

	if (string == nullptr)
		return NORMAL;

	if (string[0] == 'm' || string[0] == 'M')
		return MINIMAL;

	if (string[0] == 'r' || string[0] == 'R')
		return RADIO;

	if (string[0] == 'o' || string[0] == 'O')
		return OFF;

	return NORMAL;
}

bool BotChatterInterface::IsTalking( void ) const	
{ 
	if (m_statementList)
		return m_statementList->IsSpeaking();

	return false;
}

BotStatement *BotChatterInterface::GetStatement( void ) const
{
	return m_statementList;
}

void BotChatterInterface::Say( const char *phraseName, float lifetime, float delay )
{
	BotStatement *say = new BotStatement( this, REPORT_MY_INTENTION, lifetime );

	say->AppendPhrase( TheBotPhrases->GetPhrase( phraseName ) );

	if (delay > 0.0f)
		say->SetStartTime( gpGlobals->curtime + delay );

	AddStatement( say );
}

BotBombsiteStatusMeme::BotBombsiteStatusMeme( int zoneIndex, StatusType status) : BotMeme( Kind::BombsiteStatus )
{
	m_zoneIndex = zoneIndex;
	m_status = status;
}

BotBombStatusMeme::BotBombStatusMeme( CSGameState::BombState state, const Vector &pos) : BotMeme( Kind::BombStatus )
{
	m_state = state;
	m_pos = pos;
}

class BotChatterInterface;
class CCSBot;

using namespace ::ks::reflect;

// Purpose: / Custom chatter rules for bots when playing in cooperative modes

BotChatterCoop::BotChatterCoop( CCSBot *me ) :
BotChatterInterface( me, Kind::Coop )
{
}
//---------------------------------------------------------------------------------------------------------------
void BotChatterCoop::KilledMyEnemy( int victimID )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
		say->SetSubject( victimID );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemiesRemaining( void )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
		say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
		say->SetStartTime( gpGlobals->curtime );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemySpotted( void )
{
	float flChance = RandomFloat();
	if( flChance < 0.3 )
	{
		EnemySpottedDefault();
	}
	else if ( flChance < 0.7 )
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "GoGoGo" ) );
		AddStatement( say );
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "Cheer" ) );
		AddStatement( say );
	}
}

void BotChatterCoop::CelebrateWin( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRound" ) );
	AddStatement( say );
}
