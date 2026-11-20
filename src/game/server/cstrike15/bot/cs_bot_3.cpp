// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "networkvar.h"
#include "edict.h"
#include "responserules/response_types.h"
#include "usermessages_schema.h"
#include "../../ai_criteria.h"
module ks.server;

import std;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :nav_4;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_playeranimstate;
import ks.game.shareddefs;
import :cstrike15.cs_player;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.vprof;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.tier1.strtools;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;

class CBaseDoor;
class CBaseEntity;
class CBasePlayer;
class CBasePropDoor;
class CCSBot;
class CNavArea;
class CPushAwayEnumerator;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

static const float NearBreakableCheckDist = 20.0f;
static const float FarBreakableCheckDist = 300.0f;

static constexpr auto DEBUG_DOORS = 0;

//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
CBaseEntity * CheckForEntitiesAlongSegment( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, CPushAwayEnumerator *enumerator )
{
	CBaseEntity *entity = nullptr;

	Ray_t ray;
	ray.Init( start, end, mins, maxs );

	::partition->EnumerateElementsAlongRay( PARTITION_ENGINE_SOLID_EDICTS, ray, false, enumerator );
	if ( enumerator->m_nAlreadyHit > 0 )
	{
		entity = enumerator->m_AlreadyHit[0];
	}

	return entity;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Look up to 'distance' units ahead on the bot's path for entities.  Returns the closest one.
 */
CBaseEntity * CCSBot::FindEntitiesOnPath( float distance, CPushAwayEnumerator *enumerator, bool checkStuck )
{
	Vector goal;

	int pathIndex = FindPathPoint( distance, &goal, nullptr );
	bool isDegeneratePath = ( pathIndex == m_pathLength );
	if ( isDegeneratePath )
	{
		goal = m_goalPosition;
	}
	goal.z += HalfHumanHeight;

	Vector mins, maxs;
	mins = Vector( 0, 0, -HalfHumanWidth );
	maxs = Vector( 0, 0, HalfHumanHeight );

	if ( distance <= NearBreakableCheckDist && m_isStuck && checkStuck )
	{
		mins = Vector( -HalfHumanWidth, -HalfHumanWidth, -HalfHumanWidth );
		maxs = Vector( HalfHumanWidth, HalfHumanWidth, HalfHumanHeight );
	}

	CBaseEntity *entity = nullptr;
	if ( isDegeneratePath )
	{
		entity = CheckForEntitiesAlongSegment( WorldSpaceCenter(), m_goalPosition + Vector( 0, 0, HalfHumanHeight ), mins, maxs, enumerator );
	}
	else
	{
		int startIndex = std::max( 0, m_pathIndex );
		float distanceLeft = distance;
		// HACK: start with an index one lower than normal, so we can trace from the bot's location to the
		// start of the path nodes.
		for( int i=startIndex-1; i<m_pathLength-1; ++i )
		{
			Vector start, end;
			if ( i == startIndex - 1 )
			{
				start = GetAbsOrigin();
				end = m_path[i+1].pos;
			}
			else
			{
				start = m_path[i].pos;
				end = m_path[i+1].pos;

				if ( m_path[i+1].how == GO_LADDER_UP )
				{
					// Need two checks.  First we'll check along the ladder
					start = m_path[i].pos;
					end = m_path[i+1].ladder->m_top;
				}
				else if ( m_path[i].how == GO_LADDER_UP )
				{
					start = m_path[i].ladder->m_top;
				}
				else if ( m_path[i+1].how == GO_LADDER_DOWN )
				{
					// Need two checks.  First we'll check along the ladder
					start = m_path[i].pos;
					end = m_path[i+1].ladder->m_bottom;
				}
				else if ( m_path[i].how == GO_LADDER_DOWN )
				{
					start = m_path[i].ladder->m_bottom;
				}
			}

			float segmentLength = (start - end).Length();
			if ( distanceLeft - segmentLength < 0 )
			{
				// scale our segment back so we don't look too far
				Vector direction = end - start;
				direction.NormalizeInPlace();

				end = start + direction * distanceLeft;
			}
			entity = CheckForEntitiesAlongSegment( start + Vector( 0, 0, HalfHumanHeight ), end + Vector( 0, 0, HalfHumanHeight ), mins, maxs, enumerator );
			if ( entity )
			{
				break;
			}

			if ( m_path[i].ladder && !IsOnLadder() && distance > NearBreakableCheckDist )	// don't try to break breakables on the other end of a ladder
				break;

			distanceLeft -= segmentLength;
			if ( distanceLeft < 0 )
				break;

			if ( i != startIndex - 1 && m_path[i+1].ladder )
			{
				// Now we'll check from the ladder out to the endpoint
				start = ( m_path[i+1].how == GO_LADDER_DOWN ) ? m_path[i+1].ladder->m_bottom : m_path[i+1].ladder->m_top;
				end = m_path[i+1].pos;

				entity = CheckForEntitiesAlongSegment( start + Vector( 0, 0, HalfHumanHeight ), end + Vector( 0, 0, HalfHumanHeight ), mins, maxs, enumerator );
				if ( entity )
				{
					break;
				}
			}
		}
	}

	if ( entity && !IsVisible( entity->WorldSpaceCenter(), false, entity ) )
		return nullptr;

	return entity;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::PushawayTouch( CBaseEntity *pOther )
{

	// if we're not stuck or crouched, we don't care
	if ( !m_isStuck && !IsCrouching() )
		return;

	// See if it's breakable
	CBaseEntity *props[1];
	CBotBreakableEnumerator enumerator( props, std::size( props ) );
	enumerator.EnumElement( pOther );

	if ( enumerator.m_nAlreadyHit == 1 )
	{
		// it's breakable - try to shoot it.
		SetLookAt( "Breakable", pOther->WorldSpaceCenter(), PRIORITY_HIGH, 0.1f, false, 5.0f, true );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check for breakable physics props and other breakable entities.  We do this here instead of catching them
 * in OnTouch() because players don't collide with physics props, so OnTouch() doesn't get called.  Also,
 * looking ahead like this lets us anticipate when we'll need to break something, and do it before being
 * stopped by it.
 */
void CCSBot::BreakablesCheck( void )
{
	if ( !TheCSBots()->AllowedToDoExpensiveBotOperationThisFrame() )
		return;

	if ( IsAttacking() )
	{
		// make sure we aren't running into a breakable trying to knife an enemy
		if ( IsUsingKnife() && m_enemy != nullptr )
		{
			CBaseEntity *breakables[1];
			CBotBreakableEnumerator enumerator( breakables, std::size( breakables ) );

			CBaseEntity *breakable = nullptr;
			Vector mins = Vector( -HalfHumanWidth, -HalfHumanWidth, -HalfHumanWidth );
			Vector maxs = Vector( HalfHumanWidth, HalfHumanWidth, HalfHumanHeight );
			breakable = CheckForEntitiesAlongSegment( WorldSpaceCenter(), m_enemy->WorldSpaceCenter(), mins, maxs, &enumerator );
			if ( breakable )
			{

				// look at it (chances are we'll already be looking at it, since it's between us and our enemy)
				SetLookAt( "Breakable", breakable->WorldSpaceCenter(), PRIORITY_HIGH, 0.1f, false, 5.0f, true );

				// break it (again, don't wait: we don't have ammo, since we're using the knife, and we're looking mostly at it anyway)
				PrimaryAttack();
			}
		}
		return;
	}

	if ( !HasPath() )
		return;

	bool isNear = true;

	// Check just in front of us on the path
	CBaseEntity *breakables[4];
	CBotBreakableEnumerator enumerator( breakables, std::size( breakables ) );
	CBaseEntity *breakable = FindEntitiesOnPath( NearBreakableCheckDist, &enumerator, true );

	// If we don't have an object right in front of us, check a ways out
	if ( !breakable )
	{
		breakable = FindEntitiesOnPath( FarBreakableCheckDist, &enumerator, false );
		isNear = false;
	}

	// Try to shoot a breakable we know about
	if ( breakable )
	{
		// look at it
		SetLookAt( "Breakable", breakable->WorldSpaceCenter(), PRIORITY_HIGH, 0.1f, false, 5.0f, true );
	}

	// break it
	if ( IsLookingAtSpot( PRIORITY_HIGH ) && m_lookAtSpotAttack )
	{
		if ( IsUsingGrenade() || ( !isNear && IsUsingKnife() ) )
		{
			EquipBestWeapon( true );
		}
		else if ( GetActiveWeapon() && GetActiveWeapon()->m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			bool shouldShoot = IsLookingAtPosition( m_lookAtSpot, 10.0f );

			if ( !shouldShoot )
			{
				CBaseEntity *breakables[ 1 ];
				CBotBreakableEnumerator LOSbreakable( breakables, std::size( breakables ) );

				// compute the unit vector along our view
				Vector aimDir = GetViewVector();

				// trace the potential bullet's path
				trace_t result;
				UTIL_TraceLine( EyePosition(), EyePosition() + FarBreakableCheckDist * aimDir, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );
				if ( TraceDidHitNonWorldEntity( result ) )
				{
					LOSbreakable.EnumElement( result.Ent<CBaseEntity>() );
					if ( LOSbreakable.m_nAlreadyHit == 1 && LOSbreakable.m_AlreadyHit[ 0 ] == breakable )
					{
						shouldShoot = true;
					}
				}
			}

			// Shoot out breakable if the LOS is clear, or if we're playing a mode with no friendly fire
			shouldShoot = shouldShoot && ( !TheCSBots()->AllowFriendlyFireDamage() || !IsFriendInLineOfFire() );

			if ( shouldShoot )
			{
				PrimaryAttack();
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check for doors that need +use to open.
 */
void CCSBot::DoorCheck( void )
{
	if ( !TheCSBots()->AllowedToDoExpensiveBotOperationThisFrame() )
		return;

	if ( IsAttacking() && !IsUsingKnife() )
	{
		// If we're attacking with a gun or nade, don't bother with doors.  If we're trying to
		// knife someone, we might need to open a door.
		m_isOpeningDoor = false;
		return;
	}

	if ( !HasPath() )
		return;

	// Find any doors that need a +use to open just in front of us along the path.
	CBaseEntity *doors[4];
	CBotDoorEnumerator enumerator( doors, std::size( doors ) );
	CBaseEntity *door = FindEntitiesOnPath( NearBreakableCheckDist, &enumerator, false );

	if ( door )
	{
		if ( !IsLookingAtSpot( PRIORITY_UNINTERRUPTABLE ) )
		{
			if ( !IsOpeningDoor() )
			{
				// CBotDoorEnumerator only collects "func_door*" (CBaseDoor) and "prop_door*" (CBasePropDoor) entities
				if ( FClassnameIs( door, "prop_door*" ) )
				{
					CBasePropDoor *pPropDoor = static_cast<CBasePropDoor*>( door );
					if ( pPropDoor->IsDoorLocked() )
						return;
				}
				else if ( FClassnameIs( door, "func_door*" ) )
				{
					CBaseDoor *pFuncDoor = static_cast< CBaseDoor * >( door );
					if ( pFuncDoor->m_bLocked )
						return;
				}

				OpenDoor( door );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the stuck-checker.
 */
void CCSBot::ResetStuckMonitor( void )
{
	if (m_isStuck)
	{
		if (IsLocalPlayerWatchingMe() && cv_bot_debug.GetBool() && UTIL_GetListenServerHost())
		{
			CBasePlayer *localPlayer = UTIL_GetListenServerHost();
			CSingleUserRecipientFilter filter( localPlayer );
			EmitSound( filter, localPlayer->entindex(), "Bot.StuckSound" );
		}
	}

	m_isStuck = false;
	m_stuckTimestamp = 0.0f;
	m_stuckJumpTimer.Invalidate();
	m_avgVelIndex = 0;
	m_avgVelCount = 0;
	m_nextCleanupCheckTimestamp = 0.0f;

	m_areaEnteredTimestamp = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Test if we have become stuck
 */
void CCSBot::StuckCheck( void )
{

	if (m_isStuck)
	{
		// we are stuck - see if we have moved far enough to be considered unstuck
		Vector delta = GetAbsOrigin() - m_stuckSpot;

		const float unstuckRange = 75.0f;
		if (delta.IsLengthGreaterThan( unstuckRange ))
		{
			// we are no longer stuck
			ResetStuckMonitor();
			PrintIfWatched( "UN-STUCK\n" );
		}
	}
	else
	{
		// check if we are stuck

		// compute average velocity over a short period (for stuck check)
		Vector vel = GetAbsOrigin() - m_lastOrigin;

		// if we are jumping, ignore Z
		if (IsJumping())
			vel.z = 0.0f;

		// cannot be Length2D, or will break ladder movement (they are only Z)
		float moveDist = vel.Length();

		float deltaT = g_BotUpdateInterval;

		m_avgVel[ m_avgVelIndex++ ] = moveDist/deltaT;

		if (m_avgVelIndex == MAX_VEL_SAMPLES)
			m_avgVelIndex = 0;

		if (m_avgVelCount < MAX_VEL_SAMPLES)
		{
			m_avgVelCount++;
		}
		else
		{
			// we have enough samples to know if we're stuck

			float avgVel = 0.0f;
			for( int t=0; t<m_avgVelCount; ++t )
				avgVel += m_avgVel[t];

			avgVel /= m_avgVelCount;

			// cannot make this velocity too high, or bots will get "stuck" when going down ladders
			//(IsUsingLadder()) ? 10.0f : 20.0f;
			float stuckVel = ( IsRunning() ) ? 10.0f : 5.0f;

			if (avgVel < stuckVel)
			{
				// we are stuck - note when and where we initially become stuck
				m_stuckTimestamp = gpGlobals->curtime;			
				m_stuckSpot = GetAbsOrigin();
				m_stuckJumpTimer.Start( RandomFloat( 0.3f, 0.75f ) );		// 1.0

				PrintIfWatched( "STUCK\n" );
				if  (IsLocalPlayerWatchingMe() && cv_bot_debug.GetInt() > 0 && UTIL_GetListenServerHost() )
				{
					CBasePlayer *localPlayer = UTIL_GetListenServerHost();
					CSingleUserRecipientFilter filter( localPlayer );
					EmitSound( filter, localPlayer->entindex(), "Bot.StuckStart" );
				}

				m_isStuck = true;
			}
		}
	}

	// always need to track this
	m_lastOrigin = GetAbsOrigin();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check if we need to jump due to height change
 */
bool CCSBot::DiscontinuityJump( float ground, bool onlyJumpDown, bool mustJump )
{
	// Don't try to jump if in the air.
	if( !(GetFlags() & FL_ONGROUND) )
	{
		return false;
	}

	float dz = ground - GetFeetZ();

	if (dz > StepHeight && !onlyJumpDown)
	{
		// dont restrict jump time when going up
		if (Jump( true ))
		{
			return true;
		}
	}
	else if (!IsUsingLadder() && dz < -JumpHeight)
	{
		if (Jump( mustJump ))
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find "simple" ground height, treating current nav area as part of the floor
 */
bool CCSBot::GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal )
{
	if (TheNavMesh->GetSimpleGroundHeight( pos, height, normal ))
	{
		// our current nav area also serves as a ground polygon
		if (m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ))
			*height = std::max( (*height), m_lastKnownArea->GetZ( pos ) );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Get our current radio chatter place
 */
Place CCSBot::GetPlace( void ) const
{
	if (m_lastKnownArea)
		return m_lastKnownArea->GetPlace();

	return UNDEFINED_PLACE;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move towards position, independant of view angle
 */
void CCSBot::MoveTowardsPosition( const Vector &pos )
{
	Vector myOrigin = GetCentroid( this );

	//
	// Jump up on ledges
	// Because we may not be able to get to our goal position and enter the next
	// area because our extent collides with a nearby vertical ledge, make sure
	// we look far enough ahead to avoid this situation.
	// Can't look too far ahead, or bots will try to jump up slopes.
	//
	// NOTE: We need to do this frequently to catch edges at the right time
	// @todo Look ahead *along path* instead of straight line 
	//
	if ( (m_lastKnownArea == nullptr || !( m_lastKnownArea->GetAttributes() & ( NAV_MESH_NO_JUMP | NAV_MESH_STAIRS ) ) ) &&
		!IsOnLadder())
	{
		float ground;
		Vector aheadRay( pos.x - myOrigin.x, pos.y - myOrigin.y, 0 );
		aheadRay.NormalizeInPlace();

		// look far ahead to allow us to smoothly jump over gaps, ledges, etc
		// only jump if ground is flat at lookahead spot to avoid jumping up slopes
		bool jumped = false;
		if (IsRunning())
		{
			const float farLookAheadRange = 80.0f;	// 60
			Vector normal;
			Vector stepAhead = myOrigin + farLookAheadRange * aheadRay;
			stepAhead.z += HalfHumanHeight;

			if (GetSimpleGroundHeightWithFloor( stepAhead, &ground, &normal ))
			{
				if (normal.z > 0.9f)
					jumped = DiscontinuityJump( ground, true );
			}
		}

		if (!jumped)
		{
			// close up jumping
			const float lookAheadRange = 30.0f;	// cant be less or will miss jumps over low walls
			Vector stepAhead = myOrigin + lookAheadRange * aheadRay;
			stepAhead.z += HalfHumanHeight;
			if (GetSimpleGroundHeightWithFloor( stepAhead, &ground ))
			{
				jumped = DiscontinuityJump( ground );
			}
		}

		if (!jumped)
		{
			// about to fall gap-jumping
			const float lookAheadRange = 10.0f;
			Vector stepAhead = myOrigin + lookAheadRange * aheadRay;
			stepAhead.z += HalfHumanHeight;
			if (GetSimpleGroundHeightWithFloor( stepAhead, &ground ))
			{
				jumped = DiscontinuityJump( ground, true, true );
			}
		}
	}

	// compute our current forward and lateral vectors
	float angle = EyeAngles().y;

	Vector2D dir( BotCOS(angle), BotSIN(angle) );
	Vector2D lat( -dir.y, dir.x );

	// compute unit vector to goal position
	Vector2D to( pos.x - myOrigin.x, pos.y - myOrigin.y );
	to.NormalizeInPlace();

	// move towards the position independant of our view direction
	float toProj = to.x * dir.x + to.y * dir.y;
	float latProj = to.x * lat.x + to.y * lat.y;

	const float c = 0.25f;	// 0.5
	if (toProj > c)
		MoveForward();
	else if (toProj < -c)
		MoveBackward();

	// if we are avoiding someone via strafing, don't override
	if (m_avoid != nullptr)
		return;

	if (latProj >= c)
		StrafeLeft();
	else if (latProj <= -c)
		StrafeRight();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move away from position, independant of view angle
 */
void CCSBot::MoveAwayFromPosition( const Vector &pos )
{
	// compute our current forward and lateral vectors
	float angle = EyeAngles().y;

	Vector2D dir( BotCOS(angle), BotSIN(angle) );
	Vector2D lat( -dir.y, dir.x );

	// compute unit vector to goal position
	Vector2D to( pos.x - GetAbsOrigin().x, pos.y - GetAbsOrigin().y );
	to.NormalizeInPlace();

	// move away from the position independant of our view direction
	float toProj = to.x * dir.x + to.y * dir.y;
	float latProj = to.x * lat.x + to.y * lat.y;

	const float c = 0.5f;
	if (toProj > c)
		MoveBackward();
	else if (toProj < -c)
		MoveForward();

	if (latProj >= c)
		StrafeRight();
	else if (latProj <= -c)
		StrafeLeft();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Strafe (sidestep) away from position, independant of view angle
 */
void CCSBot::StrafeAwayFromPosition( const Vector &pos )
{
	// compute our current forward and lateral vectors
	float angle = EyeAngles().y;

	Vector2D dir( BotCOS(angle), BotSIN(angle) );
	Vector2D lat( -dir.y, dir.x );

	// compute unit vector to goal position
	Vector2D to( pos.x - GetAbsOrigin().x, pos.y - GetAbsOrigin().y );
	to.NormalizeInPlace();

	float latProj = to.x * lat.x + to.y * lat.y;

	if (latProj >= 0.0f)
		StrafeRight();
	else
		StrafeLeft();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * For getting un-stuck
 */
void CCSBot::Wiggle( void )
{
	if (IsCrouching())
	{
		return;
	}

	// for wiggling
	if (m_wiggleTimer.IsElapsed())
	{
		m_wiggleDirection = (NavRelativeDirType)RandomInt( 0, 3 );
		m_wiggleTimer.Start( RandomFloat( 0.3f, 0.5f ) );		// 0.3, 0.5
	}

	Vector forward, right;
	EyeVectors( &forward, &right );

	const float lookAheadRange = (m_lastKnownArea && (m_lastKnownArea->GetAttributes() & NAV_MESH_WALK)) ? 5.0f : 30.0f;
	float ground;

	switch( m_wiggleDirection )
	{
		case LEFT:
		{
			// don't move left if we will fall
			Vector pos = GetAbsOrigin() - (lookAheadRange * right);

			if (GetSimpleGroundHeightWithFloor( pos, &ground ))
			{
				if (GetAbsOrigin().z - ground < StepHeight)
				{
					StrafeLeft();
				}
			}
			break;
		}

		case RIGHT:
		{
			// don't move right if we will fall
			Vector pos = GetAbsOrigin() + (lookAheadRange * right);

			if (GetSimpleGroundHeightWithFloor( pos, &ground ))
			{
				if (GetAbsOrigin().z - ground < StepHeight)
				{
					StrafeRight();
				}
			}
			break;
		}

		case FORWARD:
		{
			// don't move forward if we will fall
			Vector pos = GetAbsOrigin() + (lookAheadRange * forward);

			if (GetSimpleGroundHeightWithFloor( pos, &ground ))
			{
				if (GetAbsOrigin().z - ground < StepHeight)
				{
					MoveForward();
				}
			}
			break;
		}

		case BACKWARD:
		{
			// don't move backward if we will fall
			Vector pos = GetAbsOrigin() - (lookAheadRange * forward);

			if (GetSimpleGroundHeightWithFloor( pos, &ground ))
			{
				if (GetAbsOrigin().z - ground < StepHeight)
				{
					MoveBackward();
				}
			}
			break;
		}
	}

	if (m_stuckJumpTimer.IsElapsed() && m_lastKnownArea && !(m_lastKnownArea->GetAttributes() & NAV_MESH_NO_JUMP))
	{
		if (Jump())
		{
			m_stuckJumpTimer.Start( RandomFloat( 1.0f, 2.0f ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine approach points from eye position and approach areas of current area
 */
void CCSBot::ComputeApproachPoints( void )
{
	m_approachPointCount = 0;

	if (m_lastKnownArea == nullptr)
	{
		return;
	}

	// assume we're crouching for now
	Vector eye = GetCentroid( this ); // + pev->view_ofs;	// eye position

	Vector ap;
	float halfWidth;
	for( int i=0; i<m_lastKnownArea->GetApproachInfoCount() && m_approachPointCount < MAX_APPROACH_POINTS; ++i )
	{
		const CNavArea::ApproachInfo *info = m_lastKnownArea->GetApproachInfo( i );

		if (info->here.area == nullptr || info->prev.area == nullptr)
		{
			continue;
		}

		// compute approach point (approach area is "info->here")
		if (info->prevToHereHow <= GO_WEST)
		{
			info->prev.area->ComputePortal( info->here.area, (NavDirType)info->prevToHereHow, &ap, &halfWidth );
			ap.z = info->here.area->GetZ( ap );
		}
		else
		{
			// use the area's center as an approach point
			ap = info->here.area->GetCenter();
		}

		// "bend" our line of sight around corners until we can see the approach point
		Vector bendPoint;
		if (BendLineOfSight( eye, ap + Vector( 0, 0, HalfHumanHeight ), &bendPoint ))
		{
			// put point on the ground
			if (TheNavMesh->GetGroundHeight( bendPoint, &bendPoint.z ) == false)
			{
				bendPoint.z = ap.z;
			}

			m_approachPoint[ m_approachPointCount ].m_pos = bendPoint;
			m_approachPoint[ m_approachPointCount ].m_area = info->here.area;
			++m_approachPointCount;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::DrawApproachPoints( void ) const
{
	for( int i=0; i<m_approachPointCount; ++i )
	{
		if (TheCSBots()->GetElapsedRoundTime() >= m_approachPoint[i].m_area->GetEarliestOccupyTime( OtherTeam( GetTeamNumber() ) ))
			NDebugOverlay::Cross3D( m_approachPoint[i].m_pos, 10.0f, 255, 0, 255, true, 0.1f );
		else
			NDebugOverlay::Cross3D( m_approachPoint[i].m_pos, 10.0f, 100, 100, 100, true, 0.1f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find the approach point that is nearest to our current path, ahead of us
 */
bool CCSBot::FindApproachPointNearestPath( Vector *pos )
{
	if (!HasPath())
		return false;

	// make sure approach points are accurate
	ComputeApproachPoints();

	if (m_approachPointCount == 0)
		return false;

	Vector target = Vector( 0, 0, 0 ), close;
	float targetRangeSq = 0.0f;
	bool found = false;

	int start = m_pathIndex;
	int end = m_pathLength;

	//
	// We dont want the strictly closest point, but the farthest approach point
	// from us that is near our path
	//
	const float nearPathSq = 10000.0f;	// (100)

	for( int i=0; i<m_approachPointCount; ++i )
	{
		if (FindClosestPointOnPath( m_approachPoint[i].m_pos, start, end, &close ) == false)
			continue;

		float rangeSq = (m_approachPoint[i].m_pos - close).LengthSqr();
		if (rangeSq > nearPathSq)
			continue;

		if (rangeSq > targetRangeSq)
		{
			target = close;
			targetRangeSq = rangeSq;
			found = true;
		}
	}

	if (found)
	{
		*pos = target + Vector( 0, 0, HalfHumanHeight );
		return true;	
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are at the/an enemy spawn right now
 */
bool CCSBot::IsAtEnemySpawn( void ) const
{
	CBaseEntity *spot;
	const char *spawnName = (GetTeamNumber() == TEAM_TERRORIST) ? "info_player_counterterrorist" : "info_player_terrorist";

	// check if we are at any of the enemy's spawn points
	for( spot = gEntList.FindEntityByClassname( nullptr, spawnName ); spot; spot = gEntList.FindEntityByClassname( spot, spawnName ) )
	{
		CNavArea *area = TheNavMesh->GetNearestNavArea( spot->WorldSpaceCenter() );
		if (area && GetLastKnownArea() == area)
		{
			return true;
		}
	}

	return false;
}

class CBasePlayer;
class CCSBot;
class CCSPlayer;
class CNavArea;
class CNavLadder;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Finds a point from which we can approach a descending ladder.  First it tries behind the ladder,
 * then in front of ladder, based on LOS.  Once we know the direction, we snap to the aproaching nav
 * area.  Returns true if we're approaching from behind the ladder.
 */
static bool FindDescendingLadderApproachPoint( const CNavLadder *ladder, const CNavArea *area, Vector *pos )
{
	*pos = ladder->m_top - ladder->GetNormal() * 2.0f * HalfHumanWidth;

	trace_t result;
	UTIL_TraceLine( ladder->m_top, *pos, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );
	if (result.fraction < 1.0f)
	{
		*pos = ladder->m_top + ladder->GetNormal() * 2.0f * HalfHumanWidth;

		area->GetClosestPointOnArea( *pos, pos );
	}

	// Use a cross product to determine which side of the ladder 'pos' is on
	Vector posToLadder = *pos - ladder->m_top;
	float dot = posToLadder.Dot( ladder->GetNormal() );
	return ( dot < 0.0f );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine actual path positions bot will move between along the path
 */
bool CCSBot::ComputePathPositions( void )
{
	if (m_pathLength == 0)
		return false;

	// start in first area's center
	m_path[0].pos = m_path[0].area->GetCenter();
	m_path[0].ladder = nullptr;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	for( int i=1; i<m_pathLength; ++i )
	{
		const ConnectInfo *from = &m_path[ i-1 ];
		ConnectInfo *to = &m_path[ i ];

		if (to->how <= GO_WEST)		// walk along the floor to the next area
		{
			to->ladder = nullptr;

			// compute next point, keeping path as straight as possible
			from->area->ComputeClosestPointInPortal( to->area, (NavDirType)to->how, from->pos, &to->pos );

			// move goal position into the goal area a bit
			const float stepInDist = 5.0f;		// how far to "step into" an area - must be less than min area size
			AddDirectionVector( &to->pos, (NavDirType)to->how, stepInDist );

			// we need to walk out of "from" area, so keep Z where we can reach it
			to->pos.z = from->area->GetZ( to->pos );

			// if this is a "jump down" connection, we must insert an additional point on the path
			if (to->area->IsConnected( from->area, NUM_DIRECTIONS ) == false)
			{
				// this is a "jump down" link

				// compute direction of path just prior to "jump down"
				Vector2D dir;
				DirectionToVector2D( (NavDirType)to->how, &dir );

				// shift top of "jump down" out a bit to "get over the ledge"
				const float pushDist = 75.0f; // 25.0f;
				to->pos.x += pushDist * dir.x;
				to->pos.y += pushDist * dir.y;

				// insert a duplicate node to represent the bottom of the fall
				if (m_pathLength < MAX_PATH_LENGTH-1)
				{
					// copy nodes down
					for( int j=m_pathLength; j>i; --j )
						m_path[j] = m_path[j-1];

					// path is one node longer
					++m_pathLength;

					// move index ahead into the new node we just duplicated
					++i;

					m_path[i].pos.x = to->pos.x;
					m_path[i].pos.y = to->pos.y;

					// put this one at the bottom of the fall
					m_path[i].pos.z = to->area->GetZ( m_path[i].pos );
				}
			}
		}
		else if (to->how == GO_LADDER_UP)		// to get to next area, must go up a ladder
		{
			// find our ladder
			const NavLadderConnectVector *pLadders = from->area->GetLadders( CNavLadder::LADDER_UP );
			int it;
			for ( it = 0; it < std::ssize( *pLadders ); ++it)
			{
				CNavLadder *ladder = (*pLadders)[ it ].ladder;

				// can't use "behind" area when ascending...
				if (ladder->m_topForwardArea == to->area ||
					ladder->m_topLeftArea == to->area ||
					ladder->m_topRightArea == to->area)
				{
					to->ladder = ladder;
					to->pos = ladder->m_bottom + ladder->GetNormal() * 2.0f * HalfHumanWidth;
					break;
				}
			}

			if (it == std::ssize( *pLadders ))
			{
				PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
		}
		else if (to->how == GO_LADDER_DOWN)		// to get to next area, must go down a ladder
		{
			// find our ladder
			const NavLadderConnectVector *pLadders = from->area->GetLadders( CNavLadder::LADDER_DOWN );
			int it;
			for ( it = 0; it < std::ssize( *pLadders ); ++it)
			{
				CNavLadder *ladder = (*pLadders)[ it ].ladder;

				if (ladder->m_bottomArea == to->area)
				{
					to->ladder = ladder;

					FindDescendingLadderApproachPoint( to->ladder, from->area, &to->pos );
					break;
				}
			}

			if (it == std::ssize( *pLadders ))
			{
				PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If next step of path uses a ladder, prepare to traverse it
 */
void CCSBot::SetupLadderMovement( void )
{
	if (m_pathIndex < 1 || m_pathLength == 0)
		return;

	const ConnectInfo *to = &m_path[ m_pathIndex ];
	const ConnectInfo *from = &m_path[ m_pathIndex - 1 ];

	if (to->ladder)
	{
		m_spotEncounter = nullptr;
		m_areaEnteredTimestamp = gpGlobals->curtime;

		m_pathLadder = to->ladder;
		m_pathLadderTimestamp = gpGlobals->curtime;

		QAngle ladderAngles;
		VectorAngles( m_pathLadder->GetNormal(), ladderAngles );

		// to get to next area, we must traverse a ladder
		if (to->how == GO_LADDER_UP)
		{
			m_pathLadderState = APPROACH_ASCENDING_LADDER;
			m_pathLadderFaceIn = true;
			PrintIfWatched( "APPROACH_ASCENDING_LADDER\n" );
			m_goalPosition = m_pathLadder->m_bottom + m_pathLadder->GetNormal() * 2.0f * HalfHumanWidth;
			m_lookAheadAngle = AngleNormalizePositive( ladderAngles[ YAW ] + 180.0f );
		}
		else
		{
			// try to mount ladder "face out" first
			bool behind = FindDescendingLadderApproachPoint( m_pathLadder, from->area, &m_goalPosition );

			if ( behind )
			{
				PrintIfWatched( "APPROACH_DESCENDING_LADDER (face out)\n" );
				m_pathLadderState = APPROACH_DESCENDING_LADDER;
				m_pathLadderFaceIn = false;
				m_lookAheadAngle = ladderAngles[ YAW ];
			}
			else
			{
				PrintIfWatched( "APPROACH_DESCENDING_LADDER (face in)\n" );
				m_pathLadderState = APPROACH_DESCENDING_LADDER;
				m_pathLadderFaceIn = true;
				m_lookAheadAngle = AngleNormalizePositive( ladderAngles[ YAW ] + 180.0f );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/// @todo What about ladders whose top AND bottom are messed up?
void CCSBot::ComputeLadderEndpoint( bool isAscending )
{
	trace_t result;
	Vector from, to;

	if (isAscending)
	{
		// find actual top in case m_pathLadder penetrates the ceiling
		// trace from our chest height at m_pathLadder base
		from = m_pathLadder->m_bottom + m_pathLadder->GetNormal() * HalfHumanWidth;
		from.z = GetAbsOrigin().z + HalfHumanHeight;
		to = m_pathLadder->m_top;
	}
	else
	{
		// find actual bottom in case m_pathLadder penetrates the floor
		// trace from our chest height at m_pathLadder top
		from = m_pathLadder->m_top + m_pathLadder->GetNormal() * HalfHumanWidth;
		from.z = GetAbsOrigin().z + HalfHumanHeight;
		to = m_pathLadder->m_bottom;
	}

	UTIL_TraceLine( from, m_pathLadder->m_bottom, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &result );

	if (result.fraction == 1.0f)
		m_pathLadderEnd = to.z;
	else
		m_pathLadderEnd = from.z + result.fraction * (to.z - from.z);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Navigate our current ladder. Return true if we are doing ladder navigation.
 * @todo Need Push() and Pop() for run/walk context to keep ladder speed contained.
 */
bool CCSBot::UpdateLadderMovement( void )
{
	if (m_pathLadder == nullptr)
		return false;

	bool giveUp = false;

	// check for timeout
	const float ladderTimeoutDuration = 10.0f;
	if (gpGlobals->curtime - m_pathLadderTimestamp > ladderTimeoutDuration && !cv_bot_debug.GetBool())
	{
		PrintIfWatched( "Ladder timeout!\n" );
		giveUp = true;
	}
	else if (m_pathLadderState == APPROACH_ASCENDING_LADDER || 
			 m_pathLadderState == APPROACH_DESCENDING_LADDER || 
			 m_pathLadderState == ASCEND_LADDER || 
			 m_pathLadderState == DESCEND_LADDER || 
			 m_pathLadderState == DISMOUNT_ASCENDING_LADDER ||
			 m_pathLadderState == MOVE_TO_DESTINATION)
	{
		if (m_isStuck)
		{
			PrintIfWatched( "Giving up ladder - stuck\n" );
			giveUp = true;
		}
	}

	if (giveUp)
	{
		// jump off ladder and give up
		Jump( true );
		Wiggle();
		ResetStuckMonitor();
		DestroyPath();
		Run();
		return false;
	}
	else
	{
		ResetStuckMonitor();
	}

	Vector myOrigin = GetCentroid( this );

	// check if somehow we totally missed the ladder
	switch( m_pathLadderState )
	{
		case MOUNT_ASCENDING_LADDER:
		case MOUNT_DESCENDING_LADDER:
		case ASCEND_LADDER:
		case DESCEND_LADDER:
		{
			const float farAway = 200.0f;
			const Vector &ladderPos = (m_pathLadderState == MOUNT_ASCENDING_LADDER ||
				m_pathLadderState == ASCEND_LADDER) ? m_pathLadder->m_bottom : m_pathLadder->m_top;
			if ((ladderPos.AsVector2D() - myOrigin.AsVector2D()).IsLengthGreaterThan( farAway ))
			{
				PrintIfWatched( "Missed ladder\n" );
				Jump( true );
				DestroyPath();
				Run();
				return false;
			}
			break;
		}
	}

	m_areaEnteredTimestamp = gpGlobals->curtime;

	const float tolerance = 10.0f;
	const float closeToGoal = 25.0f;

	switch( m_pathLadderState )
	{
		case APPROACH_ASCENDING_LADDER:
		{
			bool approached = false;

			Vector2D d( myOrigin.x - m_goalPosition.x, myOrigin.y - m_goalPosition.y );

			if (d.x * m_pathLadder->GetNormal().x + d.y * m_pathLadder->GetNormal().y < 0.0f)
			{
				Vector2D perp( -m_pathLadder->GetNormal().y, m_pathLadder->GetNormal().x );

				if (fabs(d.x * perp.x + d.y * perp.y) < tolerance && d.Length() < closeToGoal)
					approached = true;
			}

			// small radius will just slow them down a little for more accuracy in hitting their spot
			const float walkRange = 50.0f;
			if (d.IsLengthLessThan( walkRange ))
			{
				Walk();
				StandUp();
			}

			if ( d.IsLengthLessThan( 100.0f ) )
			{
				if ( !IsOnLadder() && (m_pathLadder->m_bottom.z - GetAbsOrigin().z > JumpCrouchHeight ) )
				{
					// find yaw to directly aim at ladder
					QAngle idealAngle;
					VectorAngles( GetAbsVelocity(), idealAngle );
					const float angleTolerance = 15.0f;
					if (AnglesAreEqual( EyeAngles().y, idealAngle.y, angleTolerance ))
					{
						Jump();
					}
				}
			}

			/// @todo Check that we are on the ladder we think we are
			if (IsOnLadder())
			{
				m_pathLadderState = ASCEND_LADDER;
				PrintIfWatched( "ASCEND_LADDER\n" );

				// find actual top in case m_pathLadder penetrates the ceiling
				ComputeLadderEndpoint( true );
			}
			else if (approached)
			{
				// face the m_pathLadder
				m_pathLadderState = FACE_ASCENDING_LADDER;
				PrintIfWatched( "FACE_ASCENDING_LADDER\n" );
			}
			else
			{
				// move toward ladder mount point
				MoveTowardsPosition( m_goalPosition );
			}
			break;
		}

		case APPROACH_DESCENDING_LADDER:
		{
			// fall check
			if (GetFeetZ() <= m_pathLadder->m_bottom.z + HalfHumanHeight)
			{
				PrintIfWatched( "Fell from ladder.\n" );

				m_pathLadderState = MOVE_TO_DESTINATION;
				m_path[ m_pathIndex ].area->GetClosestPointOnArea( m_pathLadder->m_bottom, &m_goalPosition );
				m_goalPosition += m_pathLadder->GetNormal() * HalfHumanWidth;

				PrintIfWatched( "MOVE_TO_DESTINATION\n" );
			}
			else
			{
				bool approached = false;

				Vector2D d( myOrigin.x - m_goalPosition.x, myOrigin.y - m_goalPosition.y );

				if (d.x * m_pathLadder->GetNormal().x + d.y * m_pathLadder->GetNormal().y > 0.0f)
				{
					Vector2D perp( -m_pathLadder->GetNormal().y, m_pathLadder->GetNormal().x );

					if (fabs(d.x * perp.x + d.y * perp.y) < tolerance && d.Length() < closeToGoal)
						approached = true;
				}

				// if approaching ladder from the side or "ahead", walk
				if (m_pathLadder->m_topBehindArea != m_lastKnownArea)
				{
					const float walkRange = 150.0f;
					if (!IsCrouching() && d.IsLengthLessThan( walkRange ))
						Walk();
				}

				/// @todo Check that we are on the ladder we think we are
				if (IsOnLadder())
				{
					// we slipped onto the ladder - climb it
					m_pathLadderState = DESCEND_LADDER;
					Run();
					PrintIfWatched( "DESCEND_LADDER\n" );

					// find actual bottom in case m_pathLadder penetrates the floor
					ComputeLadderEndpoint( false );
				}
				else if (approached)
				{
					// face the ladder
					m_pathLadderState = FACE_DESCENDING_LADDER;
					PrintIfWatched( "FACE_DESCENDING_LADDER\n" );
				}
				else
				{
					// move toward ladder mount point
					MoveTowardsPosition( m_goalPosition );
				}
			}
			break;
		}

		case FACE_ASCENDING_LADDER:
		{
			// find yaw to directly aim at ladder
			Vector to = m_pathLadder->GetPosAtHeight(myOrigin.z) - myOrigin;

			QAngle idealAngle;
			VectorAngles( to, idealAngle );

			if (m_path[ m_pathIndex ].area == m_pathLadder->m_topForwardArea)
			{
				m_pathLadderDismountDir = FORWARD;
			}
			else if (m_path[ m_pathIndex ].area == m_pathLadder->m_topLeftArea)
			{
				m_pathLadderDismountDir = LEFT;
				idealAngle[ YAW ] = AngleNormalizePositive( idealAngle[ YAW ] + 90.0f );
			}
			else if (m_path[ m_pathIndex ].area == m_pathLadder->m_topRightArea)
			{
				m_pathLadderDismountDir = RIGHT;
				idealAngle[ YAW ] = AngleNormalizePositive( idealAngle[ YAW ] - 90.0f );
			}

			const float angleTolerance = 5.0f;
			if (AnglesAreEqual( EyeAngles().y, idealAngle.y, angleTolerance ))
			{
				// move toward ladder until we become "on" it
				Run();
				ResetStuckMonitor();
				m_pathLadderState = MOUNT_ASCENDING_LADDER;
				switch (m_pathLadderDismountDir)
				{
					case LEFT:		PrintIfWatched( "MOUNT_ASCENDING_LADDER LEFT\n" );		break;
					case RIGHT:		PrintIfWatched( "MOUNT_ASCENDING_LADDER RIGHT\n" );		break;
					default:		PrintIfWatched( "MOUNT_ASCENDING_LADDER FORWARD\n" );	break;
				}
			}
			break;
		}

		case FACE_DESCENDING_LADDER:
		{
			// find yaw to directly aim at ladder
			Vector to = m_pathLadder->GetPosAtHeight(myOrigin.z) - myOrigin;

			QAngle idealAngle;
			VectorAngles( to, idealAngle );

			const float angleTolerance = 5.0f;
			if (AnglesAreEqual( EyeAngles().y, idealAngle.y, angleTolerance ))
			{
				// move toward ladder until we become "on" it
				m_pathLadderState = MOUNT_DESCENDING_LADDER;
				ResetStuckMonitor();
				PrintIfWatched( "MOUNT_DESCENDING_LADDER\n" );
			}
			break;
		}

		case MOUNT_ASCENDING_LADDER:
			if (IsOnLadder())
			{
				m_pathLadderState = ASCEND_LADDER;
				PrintIfWatched( "ASCEND_LADDER\n" );

				// find actual top in case m_pathLadder penetrates the ceiling
				ComputeLadderEndpoint( true );
			}

			// move toward ladder mount point
			if ( !IsOnLadder() && (m_pathLadder->m_bottom.z - GetAbsOrigin().z > JumpCrouchHeight ) )
			{
				Jump();
			}

			switch( m_pathLadderDismountDir )
			{
				case RIGHT:		StrafeLeft();	break;
				case LEFT:		StrafeRight();	break;
				default:		MoveForward();	break;
			}
			break;

		case MOUNT_DESCENDING_LADDER:
			// fall check
			if (GetFeetZ() <= m_pathLadder->m_bottom.z + HalfHumanHeight)
			{
				PrintIfWatched( "Fell from ladder.\n" );

				m_pathLadderState = MOVE_TO_DESTINATION;
				m_path[ m_pathIndex ].area->GetClosestPointOnArea( m_pathLadder->m_bottom, &m_goalPosition );
				m_goalPosition += m_pathLadder->GetNormal() * HalfHumanWidth;

				PrintIfWatched( "MOVE_TO_DESTINATION\n" );
			}
			else
			{
				if (IsOnLadder())
				{
					m_pathLadderState = DESCEND_LADDER;
					PrintIfWatched( "DESCEND_LADDER\n" );

					// find actual bottom in case m_pathLadder penetrates the floor
					ComputeLadderEndpoint( false );
				}

				// move toward ladder mount point
				MoveForward();
			}
			break;

		case ASCEND_LADDER:
			// run, so we can make our dismount jump to the side, if necessary
			Run();

			// if our destination area requires us to crouch, do it
			if (m_path[ m_pathIndex ].area->GetAttributes() & NAV_MESH_CROUCH)
				Crouch();

			// did we reach the top?
			if (GetFeetZ() >= m_pathLadderEnd)
			{
				// we reached the top - dismount
				m_pathLadderState = DISMOUNT_ASCENDING_LADDER;
				PrintIfWatched( "DISMOUNT_ASCENDING_LADDER\n" );

				if (m_path[ m_pathIndex ].area == m_pathLadder->m_topForwardArea)
					m_pathLadderDismountDir = FORWARD;
				else if (m_path[ m_pathIndex ].area == m_pathLadder->m_topLeftArea)
					m_pathLadderDismountDir = LEFT;
				else if (m_path[ m_pathIndex ].area == m_pathLadder->m_topRightArea)
					m_pathLadderDismountDir = RIGHT;

				m_pathLadderDismountTimestamp = gpGlobals->curtime;
			}
			else if (!IsOnLadder())
			{
				// we fall off the ladder, repath
				DestroyPath();
				return false;
			}

			// move up ladder
			switch( m_pathLadderDismountDir )
			{
				case RIGHT:		StrafeLeft();	break;
				case LEFT:		StrafeRight();	break;
				default:		MoveForward();	break;
			}
			break;

		case DESCEND_LADDER:
		{
			Run();
			float destHeight = m_pathLadderEnd;
			if ( (m_path[ m_pathIndex ].area->GetAttributes() & NAV_MESH_NO_JUMP) == 0 )
			{
				destHeight += HalfHumanHeight;
			}
			if ( !IsOnLadder() || GetFeetZ() <= destHeight )
			{
				// we reached the bottom, or we fell off - dismount
				m_pathLadderState = MOVE_TO_DESTINATION;
				m_path[ m_pathIndex ].area->GetClosestPointOnArea( m_pathLadder->m_bottom, &m_goalPosition );
				m_goalPosition += m_pathLadder->GetNormal() * HalfHumanWidth;

				PrintIfWatched( "MOVE_TO_DESTINATION\n" );
			}

			// Move down ladder
			MoveForward();

			break;
		}

		case DISMOUNT_ASCENDING_LADDER:
		{
			if (gpGlobals->curtime - m_pathLadderDismountTimestamp >= 0.4f)
			{
				m_pathLadderState = MOVE_TO_DESTINATION;
				m_path[ m_pathIndex ].area->GetClosestPointOnArea( myOrigin, &m_goalPosition );
				PrintIfWatched( "MOVE_TO_DESTINATION\n" );
			}

			// We should already be facing the dismount point
			MoveForward();
			break;
		}

		case MOVE_TO_DESTINATION:
			if (m_path[ m_pathIndex ].area->Contains( myOrigin ))
			{
				// successfully traversed ladder and reached destination area
				// exit ladder state machine
				PrintIfWatched( "Ladder traversed.\n" );
				m_pathLadder = nullptr;

				// incrememnt path index to next step beyond this ladder
				SetPathIndex( m_pathIndex+1 );

				ClearLookAt();

				return false;
			}

			MoveTowardsPosition( m_goalPosition );
			break;
	}

	if (cv_bot_traceview.GetInt() == 1 && IsLocalPlayerWatchingMe() || cv_bot_traceview.GetInt() == 10)
	{
		DrawPath();
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute closest point on path to given point
 * NOTE: This does not do line-of-sight tests, so closest point may be thru the floor, etc
 */
bool CCSBot::FindClosestPointOnPath( const Vector &worldPos, int startIndex, int endIndex, Vector *close ) const
{
	if (!HasPath() || close == nullptr)
		return false;

	Vector along, toWorldPos;
	Vector pos;
	const Vector *from, *to;
	float length;
	float closeLength;
	float closeDistSq = 9999999999.9;
	float distSq;

	for( int i=startIndex; i<=endIndex; ++i )
	{
		from = &m_path[i-1].pos;
		to = &m_path[i].pos;

		// compute ray along this path segment
		along = *to - *from;

		// make it a unit vector along the path
		length = along.NormalizeInPlace();

		// compute vector from start of segment to our point
		toWorldPos = worldPos - *from;

		// find distance of closest point on ray
		closeLength = DotProduct( toWorldPos, along );

		// constrain point to be on path segment
		if (closeLength <= 0.0f)
			pos = *from;
		else if (closeLength >= length)
			pos = *to;
		else
			pos = *from + closeLength * along;

		distSq = (pos - worldPos).LengthSqr();

		// keep the closest point so far
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			*close = pos;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest point to our current position on our current path
 * If "local" is true, only check the portion of the path surrounding m_pathIndex.
 */
int CCSBot::FindOurPositionOnPath( Vector *close, bool local ) const
{
	if (!HasPath())
		return -1;

	Vector along, toFeet;
	Vector feet = GetAbsOrigin();
	Vector eyes = feet + Vector( 0, 0, HalfHumanHeight );	// in case we're crouching
	Vector pos;
	const Vector *from, *to;
	float length;
	float closeLength;
	float closeDistSq = 9999999999.9;
	int closeIndex = -1;
	float distSq;

	int start, end;

	if (local)
	{
		start = m_pathIndex - 3;
		if (start < 1)
			start = 1;

		end = m_pathIndex + 3;
		if (end > m_pathLength)
			end = m_pathLength;
	}
	else
	{
		start = 1;
		end = m_pathLength;
	}

	for( int i=start; i<end; ++i )
	{
		from = &m_path[i-1].pos;
		to = &m_path[i].pos;

		// compute ray along this path segment
		along = *to - *from;

		// make it a unit vector along the path
		length = along.NormalizeInPlace();

		// compute vector from start of segment to our point
		toFeet = feet - *from;

		// find distance of closest point on ray
		closeLength = DotProduct( toFeet, along );

		// constrain point to be on path segment
		if (closeLength <= 0.0f)
			pos = *from;
		else if (closeLength >= length)
			pos = *to;
		else
			pos = *from + closeLength * along;

		distSq = (pos - feet).LengthSqr();

		// keep the closest point so far
		if (distSq < closeDistSq)
		{
			// don't use points we cant see
			Vector probe = pos + Vector( 0, 0, HalfHumanHeight );
			if (!IsWalkableTraceLineClear( eyes, probe, WALK_THRU_DOORS | WALK_THRU_BREAKABLES ))
				continue;

			// don't use points we cant reach
			if (!IsStraightLinePathWalkable( pos ))
				continue;

			closeDistSq = distSq;
			if (close)
				*close = pos;
			closeIndex = i-1;
		}
	}

	return closeIndex;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Test for un-jumpable height change, or unrecoverable fall
 */
bool CCSBot::IsStraightLinePathWalkable( const Vector &goal ) const
{
// this is causing hang-up problems when crawling thru ducts/windows that drop off into rooms (they fail the "falling" check)
return true;

	const float inc = GenerationStepSize;

	Vector feet = GetAbsOrigin();
	Vector dir = goal - feet;
	float length = dir.NormalizeInPlace();

	float lastGround;
	//if (!GetSimpleGroundHeight( &pev->origin, &lastGround ))
	//	return false;
	lastGround = feet.z;

	float along=0.0f;
	Vector pos;
	float ground;
	bool done = false;
	while( !done )
	{
		along += inc;
		if (along > length)
		{
			along = length;
			done = true;
		}

		// compute step along path
		pos = feet + along * dir;

		pos.z += HalfHumanHeight;

		if (!TheNavMesh->GetSimpleGroundHeight( pos, &ground ))
			return false;

		// check for falling
		if (ground - lastGround < -StepHeight)
			return false;

		// check for unreachable jump
		// use slightly shorter jump limit, to allow for some fudge room
		if (ground - lastGround > JumpHeight)
			return false;

		lastGround = ground;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute a point a fixed distance ahead along our path.
 * Returns path index just after point.
 */
int CCSBot::FindPathPoint( float aheadRange, Vector *point, int *prevIndex )
{
	Vector myOrigin = GetCentroid( this );

	// find path index just past aheadRange
	int afterIndex;

	// finds the closest point on local area of path, and returns the path index just prior to it
	Vector close;
	int startIndex = FindOurPositionOnPath( &close, true );

	if (prevIndex)
		*prevIndex = startIndex;

	if (startIndex <= 0)
	{
		// went off the end of the path
		// or next point in path is unwalkable (ie: jump-down)
		// keep same point
		return m_pathIndex;
	}

	// if we are crouching, just follow the path exactly
	if (IsCrouching())
	{
		// we want to move to the immediately next point along the path from where we are now
		int index = startIndex+1;
		if (index >= m_pathLength)
			index = m_pathLength-1;

		*point = m_path[ index ].pos;

		// if we are very close to the next point in the path, skip ahead to the next one to avoid wiggling
		// we must do a 2D check here, in case the goal point is floating in space due to jump down, etc
		const float closeEpsilon = 20.0f;	// 10
		while ((*point - close).AsVector2D().IsLengthLessThan( closeEpsilon ))
		{
			++index;

			if (index >= m_pathLength)
			{
				index = m_pathLength-1;
				break;
			}

			*point = m_path[ index ].pos;
		}

		return index;
	}

	// make sure we use a node a minimum distance ahead of us, to avoid wiggling 
	while (startIndex < m_pathLength-1)
	{
		Vector pos = m_path[ startIndex+1 ].pos;

		// we must do a 2D check here, in case the goal point is floating in space due to jump down, etc
		const float closeEpsilon = 20.0f;
		if ((pos - close).AsVector2D().IsLengthLessThan( closeEpsilon ))
		{
			++startIndex;
		}
		else
		{
			break;
		}
	}

	// if we hit a ladder, stop, stair, or jump area, must stop (dont use ladder behind us)
	if (startIndex > m_pathIndex && startIndex < m_pathLength && 
		(m_path[ startIndex ].ladder || m_path[ startIndex ].area->GetAttributes() & (NAV_MESH_JUMP | NAV_MESH_STOP | NAV_MESH_STAIRS)))
	{
		*point = m_path[ startIndex ].pos;
		return startIndex;
	}

	// we need the point just *ahead* of us
	++startIndex;
	if (startIndex >= m_pathLength)
		startIndex = m_pathLength-1;

	// if we hit a ladder, stop, or jump area, must stop
	if (startIndex < m_pathLength && 
		(m_path[ startIndex ].ladder || m_path[ startIndex ].area->GetAttributes() & (NAV_MESH_JUMP | NAV_MESH_STOP | NAV_MESH_STAIRS)))
	{
		*point = m_path[ startIndex ].pos;
		return startIndex;
	}

	// note direction of path segment we are standing on
	Vector initDir = m_path[ startIndex ].pos - m_path[ startIndex-1 ].pos;
	initDir.NormalizeInPlace();

	Vector feet = GetAbsOrigin();
	Vector eyes = feet + Vector( 0, 0, HalfHumanHeight );
	float rangeSoFar = 0;

	// this flag is true if our ahead point is visible
	bool visible = true;

	Vector prevDir = initDir;

	// step along the path until we pass aheadRange
	bool isCorner = false;
	int i;
	for( i=startIndex; i<m_pathLength; ++i )
	{
		Vector pos = m_path[i].pos;
		Vector to = pos - m_path[i-1].pos;
		Vector dir = to;
		dir.NormalizeInPlace();

		// if path crosses damaging areas (ie: fire), stop and wait for it to go away
		if ( GetTimeSinceBurnedByFlames() > 1.0f && m_path[i].area->IsDamaging() && rangeSoFar < 100.0f )
		{
			Wait( RandomFloat( 0.5f, 1.5f ) );
			--i;
			break;
		}

		// don't allow path to double-back from our starting direction (going upstairs, down curved passages, etc)
		if (DotProduct( dir, initDir ) < 0.0f) // -0.25f
		{
			--i;
			break;
		}

		// if the path turns a corner, we want to move towards the corner, not into the wall/stairs/etc
		if (DotProduct( dir, prevDir ) < 0.5f)
		{
			isCorner = true;
			--i;
			break;
		}
		prevDir = dir;

		// don't use points we cant see
		Vector probe = pos + Vector( 0, 0, HalfHumanHeight );
		if (!IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ))
		{
			// presumably, the previous point is visible, so we will interpolate
			visible = false;
			break;
		}

		// if we encounter a ladder, stairs, or jump area, we must stop
		if (i < m_pathLength && 
				(m_path[ i ].ladder || m_path[ i ].area->GetAttributes() & (NAV_MESH_JUMP | NAV_MESH_STOP | NAV_MESH_STAIRS)))
			break;

		// Check straight-line path from our current position to this position
		// Test for un-jumpable height change, or unrecoverable fall
		if (!IsStraightLinePathWalkable( pos ))
		{
			--i;
			break;
		}

		Vector along = (i == startIndex) ? (pos - feet) : (pos - m_path[i-1].pos);
		rangeSoFar += along.Length2D();

		// stop if we have gone farther than aheadRange
		if (rangeSoFar >= aheadRange)
			break;
	}

	if (i < startIndex)
		afterIndex = startIndex;
	else if (i < m_pathLength)
		afterIndex = i;
	else
		afterIndex = m_pathLength-1;

	// compute point on the path at aheadRange
	if (afterIndex == 0)
	{
		*point = m_path[0].pos;
	}
	else
	{
		// interpolate point along path segment
		const Vector *afterPoint = &m_path[ afterIndex ].pos;
		const Vector *beforePoint = &m_path[ afterIndex-1 ].pos;

		Vector to = *afterPoint - *beforePoint;
		float length = to.Length2D();

		float t = 1.0f - ((rangeSoFar - aheadRange) / length);

		if (t < 0.0f)
			t = 0.0f;
		else if (t > 1.0f)
			t = 1.0f;

		*point = *beforePoint + t * to;

		// if afterPoint wasn't visible, slide point backwards towards beforePoint until it is
		if (!visible)
		{
			const float sightStepSize = 25.0f;
			float dt = sightStepSize / length;

			Vector probe = *point + Vector( 0, 0, HalfHumanHeight );
			while( t > 0.0f && !IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ) )
			{
				t -= dt;
				*point = *beforePoint + t * to;
			}

			if (t <= 0.0f)
				*point = *beforePoint;
		}
	}

	// if position found is too close to us, or behind us, force it farther down the path so we don't stop and wiggle
	if (!isCorner)
	{
		const float epsilon = 50.0f;
		Vector2D toPoint;
		toPoint.x = point->x - myOrigin.x;
		toPoint.y = point->y - myOrigin.y;
		if (DotProduct2D( toPoint, initDir.AsVector2D() ) < 0.0f || toPoint.IsLengthLessThan( epsilon ))
		{
			int i;
			for( i=startIndex; i<m_pathLength; ++i )
			{
				toPoint.x = m_path[i].pos.x - myOrigin.x;
				toPoint.y = m_path[i].pos.y - myOrigin.y;
				if (m_path[i].ladder || m_path[i].area->GetAttributes() & (NAV_MESH_JUMP | NAV_MESH_STOP | NAV_MESH_STAIRS) || toPoint.IsLengthGreaterThan( epsilon ))
				{
					*point = m_path[i].pos;
					startIndex = i;
					break;
				}
			}

			if (i == m_pathLength)
			{
				*point = GetPathEndpoint();
				startIndex = m_pathLength-1;
			}
		}
	}

	// m_pathIndex should always be the next point on the path, even if we're not moving directly towards it
	return startIndex;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the current index along the path
 */
void CCSBot::SetPathIndex( int newIndex )
{
	m_pathIndex = std::min( newIndex, m_pathLength-1 );
	m_areaEnteredTimestamp = gpGlobals->curtime;

	if (m_path[ m_pathIndex ].ladder)
	{
		SetupLadderMovement();
	}
	else
	{
		// get our "encounter spots" for this leg of the path
		if (m_pathIndex < m_pathLength && m_pathIndex >= 2)
			m_spotEncounter = m_path[ m_pathIndex-1 ].area->GetSpotEncounter( m_path[ m_pathIndex-2 ].area, m_path[ m_pathIndex ].area );
		else
			m_spotEncounter = nullptr;

		m_pathLadder = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if nearing a jump in the path
 */
bool CCSBot::IsNearJump( void ) const
{
	if (m_pathIndex == 0 || m_pathIndex >= m_pathLength)
		return false;

	for( int i=m_pathIndex-1; i<m_pathIndex; ++i )
	{
		if (m_path[ i ].area->GetAttributes() & NAV_MESH_JUMP)
		{
			float dz = m_path[ i+1 ].pos.z - m_path[ i ].pos.z;

			if (dz > 0.0f)
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return approximately how much damage will will take from the given fall height
 */
float CCSBot::GetApproximateFallDamage( float height ) const
{
	// empirically discovered height values
	const float slope = 0.2178f;
	const float intercept = 26.0f;

	float damage = slope * height - intercept;

	if (damage < 0.0f)
		return 0.0f;

	return damage;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if a friend is between us and the given position
 */
bool CCSBot::IsFriendInTheWay( const Vector &goalPos )
{
	if ( !CSGameRules()->IsTeammateSolid() )
	{
		// we can pass right thru teammates in the mode - no waiting
		return false;
	}

	// do this check less often to ease CPU burden
	if (!m_avoidFriendTimer.IsElapsed())
	{
		return m_isFriendInTheWay;
	}

	const float avoidFriendInterval = 0.5f;
	m_avoidFriendTimer.Start( avoidFriendInterval );

	// compute ray along intended path
	Vector myOrigin = GetCentroid( this );
	Vector moveDir = goalPos - myOrigin;

	// make it a unit vector 
	float length = moveDir.NormalizeInPlace();

	m_isFriendInTheWay = false;

	// check if any friends are overlapping this linear path
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer *player = static_cast<CCSPlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsAlive())
			continue;

		if ( IsOtherEnemy( player ) )
			continue;

		if (player->entindex() == entindex())
			continue;

		// compute vector from us to our friend
		Vector toFriend = player->GetAbsOrigin() - GetAbsOrigin();

		// check if friend is in our "personal space"
		const float personalSpace = 100.0f;
		if (toFriend.IsLengthGreaterThan( personalSpace ))
			continue;

		// find distance of friend along our movement path
		float friendDistAlong = DotProduct( toFriend, moveDir );

		// if friend is behind us, ignore him
		if (friendDistAlong <= 0.0f)
			continue;

		// constrain point to be on path segment
		Vector pos;
		if (friendDistAlong >= length)
			pos = goalPos;
		else
			pos = myOrigin + friendDistAlong * moveDir;

		// check if friend overlaps our intended line of movement
		const float friendRadius = 30.0f;
		if ((pos - GetCentroid( player )).IsLengthLessThan( friendRadius ))
		{
			// friend is in our personal space and overlaps our intended line of movement
			m_isFriendInTheWay = true;
			break;
		}
	}

	return m_isFriendInTheWay;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do reflex avoidance movements if our "feelers" are touched
 */
void CCSBot::FeelerReflexAdjustment( Vector *goalPosition )
{
	// if we are in a "precise" area, do not do feeler adjustments
	if (m_lastKnownArea && m_lastKnownArea->GetAttributes() & NAV_MESH_PRECISE)
		return;

	Vector dir( BotCOS( m_forwardAngle ), BotSIN( m_forwardAngle ), 0.0f );
	Vector lat( -dir.y, dir.x, 0.0f );

	const float feelerOffset = (IsCrouching()) ? 5.0f : 10.0f;
	const float feelerLengthRun = 25.0f;	// 50
	const float feelerLengthWalk = 15.0f;
	const float feelerHeight = StepHeight + 0.1f;	// if obstacle is lower than StepHeight, we'll walk right over it

	float feelerLength = (IsRunning()) ? feelerLengthRun : feelerLengthWalk;

	feelerLength = (IsCrouching()) ? 20.0f : feelerLength;

	//
	// Feelers must follow floor slope
	//
	float ground;
	Vector normal;
	Vector eye = EyePosition();
	if (GetSimpleGroundHeightWithFloor( eye, &ground, &normal ) == false)
		return;

	// get forward vector along floor
	dir = CrossProduct( lat, normal );

	// correct the sideways vector
	lat = CrossProduct( dir, normal );

	Vector feet = GetAbsOrigin();
	feet.z += feelerHeight;

	Vector from = feet + feelerOffset * lat;
	Vector to = from + feelerLength * dir;

	const float hullSize = 10.0f;
	Vector mins( -hullSize, -hullSize, 0.0f );
	Vector maxs( hullSize, hullSize, HalfHumanHeight - feelerHeight );

	bool leftClear = IsWalkableTraceHullClear( from, to, mins, maxs, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );

	// avoid ledges, too
	// use 'from' so it doesn't interfere with legitimate gap jumping (its at our feet)
	/// @todo Rethink this - it causes lots of wiggling when bots jump down from vents, etc
/*
	float ground;
	if (GetSimpleGroundHeightWithFloor( &from, &ground ))
	{
		if (GetFeetZ() - ground > JumpHeight)
			leftClear = false;
	}
*/

	if (cv_bot_traceview.GetInt() == 1 && IsLocalPlayerWatchingMe() || cv_bot_traceview.GetInt() == 10)
	{
		if (leftClear)
			NDebugOverlay::SweptBox( from, to, mins, maxs, vec3_angle, 0, 255, 0, 255, 0.1f );
		else
			NDebugOverlay::SweptBox( from, to, mins, maxs, vec3_angle, 255, 0, 0, 255, 0.1f );
	}

	from = feet - feelerOffset * lat;
	to = from + feelerLength * dir;

	bool rightClear = IsWalkableTraceHullClear( from, to, mins, maxs, WALK_THRU_DOORS | WALK_THRU_BREAKABLES );

/*
	// avoid ledges, too
	if (GetSimpleGroundHeightWithFloor( &from, &ground ))
	{
		if (GetFeetZ() - ground > JumpHeight)
			rightClear = false;
	}
*/

	if (cv_bot_traceview.GetInt() == 1 && IsLocalPlayerWatchingMe() || cv_bot_traceview.GetInt() == 10)
	{
		if (rightClear)
			NDebugOverlay::SweptBox( from, to, mins, maxs, vec3_angle, 0, 255, 0, 255, 0.1f );
		else
			NDebugOverlay::SweptBox( from, to, mins, maxs, vec3_angle, 255, 0, 0, 255, 0.1f );

	}

	const float avoidRange = (IsCrouching()) ? 150.0f : 300.0f;		// 50, 300

	if (!rightClear)
	{
		if (leftClear)
		{
			// right hit, left clear - veer left
			*goalPosition = *goalPosition + avoidRange * lat;
		}
	}
	else if (!leftClear)
	{
		// right clear, left hit - veer right
		*goalPosition = *goalPosition - avoidRange * lat;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Allows the current nav area to make us run/walk without messing with our state
 */
bool CCSBot::IsRunning( void ) const
{
	// if we've forced running, go with it
	if ( !m_mustRunTimer.IsElapsed() )
	{
		return BaseClass::IsRunning();
	}

	if ( m_lastKnownArea && m_lastKnownArea->GetAttributes() & NAV_MESH_RUN )
	{
		return true;
	}

	if ( m_lastKnownArea && m_lastKnownArea->GetAttributes() & NAV_MESH_WALK )
	{
		return false;
	}

	return BaseClass::IsRunning();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move along the path. Return false if end of path reached.
 */
CCSBot::PathResult CCSBot::UpdatePathMovement( bool allowSpeedChange )
{
	CVProfScope _( "CCSBot::UpdatePathMovement", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	if (m_pathLength == 0)
		return PATH_FAILURE;

	if (cv_bot_walk.GetBool())
		Walk();

	//
	// If we are navigating a ladder, it overrides all other path movement until complete
	//
	if (UpdateLadderMovement())
		return PROGRESSING;

	// ladder failure can destroy the path
	if (m_pathLength == 0)
		return PATH_FAILURE;

	// we are not supposed to be on a ladder - if we are, jump off
	if (IsOnLadder())
		Jump( true );

	contract_assert( m_pathIndex < m_pathLength );

	//
	// Stop path attribute
	//
	if (!IsUsingLadder())
	{
		// if the m_isStopping flag is set, clear our movement
		// if the m_isStopping flag is set and movement is stopped, clear m_isStopping
		if ( m_lastKnownArea && m_isStopping )
		{
			ResetStuckMonitor();
			ClearMovement();

			if ( GetAbsVelocity().LengthSqr() < 0.1f )
			{
				m_isStopping = false;
			}
			else
			{
				return PROGRESSING;
			}
		}
	}	// end stop logic

	//
	// Check if reached the end of the path
	//
	bool nearEndOfPath = false;
	if (m_pathIndex >= m_pathLength-1)
	{
		Vector toEnd = GetPathEndpoint() - GetAbsOrigin();
		Vector d = toEnd;	// can't use 2D because path end may be below us (jump down)

		const float walkRange = 200.0f;

		// walk as we get close to the goal position to ensure we hit it
		if (d.IsLengthLessThan( walkRange ))
		{
			// don't walk if crouching - too slow
			if (allowSpeedChange && !IsCrouching())
				Walk();

			// note if we are near the end of the path
			const float nearEndRange = 50.0f;
			if (d.IsLengthLessThan( nearEndRange ))
				nearEndOfPath = true;

			const float closeEpsilon = 20.0f;
			if (d.IsLengthLessThan( closeEpsilon ))
			{
				// reached goal position - path complete
				DestroyPath();

				/// @todo We should push and pop walk state here, in case we want to continue walking after reaching goal
				if (allowSpeedChange)
					Run();

				return END_OF_PATH;
			}
		}
	}

	//
	// To keep us moving smoothly, we will move towards
	// a point farther ahead of us down our path.
	//
	int prevIndex = 0;	// closest index on path just prior to where we are now
	const float aheadRange = 300.0f;
	int newIndex = FindPathPoint( aheadRange, &m_goalPosition, &prevIndex );

	// BOTPORT: Why is prevIndex sometimes -1?
	if (prevIndex < 0)
		prevIndex = 0;

	// if goal position is near to us, we must be about to go around a corner - so look ahead!
	Vector myOrigin = GetCentroid( this );
	const float nearCornerRange = 100.0f;
	if (m_pathIndex < m_pathLength-1 && (m_goalPosition - myOrigin).IsLengthLessThan( nearCornerRange ))
	{
		if (!IsLookingAtSpot( PRIORITY_HIGH ))
		{
			ClearLookAt();
			InhibitLookAround( 0.5f );
		}
	}

	// if we moved to a new node on the path, setup movement
	if (newIndex > m_pathIndex)
	{
		SetPathIndex( newIndex );
	}

	//
	// Crouching
	//
	if (!IsUsingLadder())
	{
		// if we are approaching a crouch area, crouch
		// if there are no crouch areas coming up, stand
		const float crouchRange = 50.0f;
		bool didCrouch = false;
		for( int i=prevIndex; i<m_pathLength; ++i )
		{
			const CNavArea *to = m_path[i].area;

			// if there is a jump area on the way to the crouch area, don't crouch as it messes up the jump
			// unless we are already higher than the jump area - we must've jumped already but not moved into next area
			if (to->GetAttributes() & NAV_MESH_JUMP && to->GetCenter().z > GetFeetZ())
				break;

			Vector close;
			to->GetClosestPointOnArea( myOrigin, &close );

			if ((close - myOrigin).AsVector2D().IsLengthGreaterThan( crouchRange ))
				break;

			if (to->GetAttributes() & NAV_MESH_CROUCH)
			{
				Crouch();
				didCrouch = true;
				ResetStuckMonitor();
				break;
			}
		}

		if (!didCrouch && !IsJumping())
		{
			// no crouch areas coming up
			StandUp();
		}

	}	// end crouching logic

	// compute our forward facing angle
	m_forwardAngle = UTIL_VecToYaw( m_goalPosition - myOrigin );

	//
	// Look farther down the path to "lead" our view around corners
	//
	Vector toGoal;
	bool isWaitingForLadder = false;

	// if we are crouching, look towards where we are moving to negotiate tight corners
	if (IsCrouching())
	{
		m_lookAheadAngle = m_forwardAngle;
	}
	else
	{
		if (m_pathIndex == 0)
		{
			toGoal = m_path[1].pos;
		}
		else if (m_pathIndex < m_pathLength)
		{
			toGoal = m_path[ m_pathIndex ].pos - myOrigin;

			// actually aim our view farther down the path
			const float lookAheadRange = 500.0f;
			if (!m_path[ m_pathIndex ].ladder &&
				!IsNearJump() &&
				toGoal.AsVector2D().IsLengthLessThan( lookAheadRange ))
			{
				float along = toGoal.Length2D();
				int i;
				for( i=m_pathIndex+1; i<m_pathLength; ++i )
				{
					Vector delta = m_path[i].pos - m_path[i-1].pos;
					float segmentLength = delta.Length2D();

					if (along + segmentLength >= lookAheadRange)
					{
						// interpolate between points to keep look ahead point at fixed distance
						float t = (lookAheadRange - along) / (segmentLength + along);
						Vector target;

						if (t <= 0.0f)
							target = m_path[i-1].pos;
						else if (t >= 1.0f)
							target = m_path[i].pos;
						else
							target = m_path[i-1].pos + t * delta;

						toGoal = target - myOrigin;
						break;
					}

					// if we are coming up to a ladder or a jump, look at it
					if (m_path[i].ladder ||
						(m_path[i].area->GetAttributes() & NAV_MESH_JUMP) ||
						(m_path[i].area->GetAttributes() & NAV_MESH_PRECISE) ||
						(m_path[i].area->GetAttributes() & NAV_MESH_STOP))
					{
						toGoal = m_path[i].pos - myOrigin;

						// if anyone is on the ladder, wait
						if (m_path[i].ladder && m_path[i].ladder->IsInUse( this ))
						{
							isWaitingForLadder = true;
							ResetStuckMonitor();

							// if we are too close to the ladder, back off a bit
							const float tooCloseRange = 100.0f;
							Vector2D delta( m_path[i].ladder->m_top.x - myOrigin.x, 
											m_path[i].ladder->m_top.y - myOrigin.y );
							if (delta.IsLengthLessThan( tooCloseRange ))
							{
								MoveAwayFromPosition( m_path[i].ladder->m_top );
							}
						}

						break;
					}

					along += segmentLength;
				}

				if (i == m_pathLength)
					toGoal = GetPathEndpoint() - myOrigin;
			}
		}
		else
		{
			toGoal = GetPathEndpoint() - myOrigin;
		}

		m_lookAheadAngle = UTIL_VecToYaw( toGoal );
	}

	// initialize "adjusted" goal to current goal
	Vector adjustedGoal = m_goalPosition;

	//
	// Use short "feelers" to veer away from close-range obstacles
	// Feelers come from our ankles, just above StepHeight, so we avoid short walls, too
	// Don't use feelers if very near the end of the path, or about to jump
	//
	/// @todo Consider having feelers at several heights to deal with overhangs, etc.
	if (!nearEndOfPath && !IsNearJump() && !IsJumping())
	{
		FeelerReflexAdjustment( &adjustedGoal );
	}

	// draw debug visualization
	if (cv_bot_traceview.GetInt() == 1 && IsLocalPlayerWatchingMe() || cv_bot_traceview.GetInt() == 10)
	{
		DrawPath();

		const Vector *pos = &m_path[ m_pathIndex ].pos;
		UTIL_DrawBeamPoints( *pos, *pos + Vector( 0, 0, 50 ), 1, 255, 255, 0 );

		UTIL_DrawBeamPoints( adjustedGoal, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
		UTIL_DrawBeamPoints( myOrigin, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
	}

	// dont use adjustedGoal, as it can vary wildly from the feeler adjustment
	if (!IsAttacking() && IsFriendInTheWay( m_goalPosition ))
	{
		if (!m_isWaitingBehindFriend)
		{
			m_isWaitingBehindFriend = true;

			const float politeDuration = 5.0f - 3.0f * GetProfile()->GetAggression();
			m_politeTimer.Start( politeDuration );
		}
		else if (m_politeTimer.IsElapsed())
		{
			// we have run out of patience
			m_isWaitingBehindFriend = false;
			ResetStuckMonitor();

			// repath to avoid clump of friends in the way
			DestroyPath();
		}
	}
	else if (m_isWaitingBehindFriend)
	{
		// we're done waiting for our friend to move
		m_isWaitingBehindFriend = false;
		ResetStuckMonitor();
	}

	//
	// Move along our path if there are no friends blocking our way,
	// or we have run out of patience
	//
	if (!isWaitingForLadder && (!m_isWaitingBehindFriend || m_politeTimer.IsElapsed()))
	{
		//
		// Move along path
		//
		MoveTowardsPosition( adjustedGoal );

		//
		// Stuck check
		//
		if (m_isStuck && !IsJumping())
		{
			Wiggle();
		}
	}

	// if our goal is high above us, we must have fallen
	bool didFall = false;
	if (m_goalPosition.z - GetFeetZ() > JumpCrouchHeight)
	{
		const float closeRange = 75.0f;
		Vector2D to( myOrigin.x - m_goalPosition.x, myOrigin.y - m_goalPosition.y );
		if (to.IsLengthLessThan( closeRange ))
		{
			// we can't reach the goal position
			// check if we can reach the next node, in case this was a "jump down" situation
			if (m_pathIndex < m_pathLength-1)
			{
				if (m_path[ m_pathIndex+1 ].pos.z - GetFeetZ() > JumpCrouchHeight)
				{
					// the next node is too high, too - we really did fall of the path
					didFall = true;

					for ( int i=m_pathIndex; i<=m_pathIndex+1; ++i )
					{
						if ( m_path[i].how == GO_LADDER_UP )
						{
							// if we're going up a ladder, and we're within reach of the ladder bottom, we haven't fallen
							if ( m_path[i].pos.z - GetFeetZ() <= JumpCrouchHeight )
							{
								didFall = false;
								break;
							}
						}
					}
				}
			}
			else
			{
				// fell trying to get to the last node in the path
				didFall = true;
			}
		}
	}

	//
	// This timeout check is needed if the bot somehow slips way off 
	// of its path and cannot progress, but also moves around
	// enough that it never becomes "stuck"
	//
	const float giveUpDuration = 4.0f;
	if (didFall || gpGlobals->curtime - m_areaEnteredTimestamp > giveUpDuration)
	{
		if (didFall)
		{
			PrintIfWatched( "I fell off!\n" );
			if (IsLocalPlayerWatchingMe() && cv_bot_debug.GetBool() && UTIL_GetListenServerHost())
			{
				CBasePlayer *localPlayer = UTIL_GetListenServerHost();
				CSingleUserRecipientFilter filter( localPlayer );
				EmitSound( filter, localPlayer->entindex(), "Bot.FellOff" );
			}
		}

		// if we havent made any progress in a long time, give up
		if (m_pathIndex < m_pathLength-1)
		{
			PrintIfWatched( "Giving up trying to get to area #%d\n", m_path[ m_pathIndex ].area->GetID() );
		}
		else
		{
			PrintIfWatched( "Giving up trying to get to end of path\n" );
		}

		Run();
		StandUp();
		DestroyPath();
		ClearLookAt();

		// See if we should be on a different nav area
		CNavArea *area = TheNavMesh->GetNearestNavArea( GetAbsOrigin(), false, 500.0f, true );
		if (area && area != m_lastNavArea)
		{
			if (m_lastNavArea)
			{
				m_lastNavArea->DecrementPlayerCount( GetTeamNumber(), entindex() );
			}

			area->IncrementPlayerCount( GetTeamNumber(), entindex() );

			m_lastNavArea = area;
			if ( area->GetPlace() != UNDEFINED_PLACE )
			{
				const char *placeName = TheNavMesh->PlaceToName( area->GetPlace() );
				if ( placeName && *placeName )
				{
					V_strncpy( m_szLastPlaceName.GetForModify(), placeName, MAX_PLACE_NAME_LENGTH );
				}
			}

			// generate event
			//KeyValues *event = new KeyValues( "player_entered_area" );
			//event->SetInt( "userid", GetUserID() );
			//event->SetInt( "areaid", area->GetID() );
			//gameeventmanager->FireEvent( event );
		}

		return PATH_FAILURE;
	}

	return PROGRESSING;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Build trivial path to goal, assuming we are already in the same area
 */
void CCSBot::BuildTrivialPath( const Vector &goal )
{
	Vector myOrigin = GetCentroid( this );

	m_pathIndex = 1;
	m_pathLength = 2;

	m_path[0].area = m_lastKnownArea;
	m_path[0].pos = myOrigin;
	m_path[0].pos.z = m_lastKnownArea->GetZ( myOrigin );
	m_path[0].ladder = nullptr;
	m_path[0].how = NUM_TRAVERSE_TYPES;

	m_path[1].area = m_lastKnownArea;
	m_path[1].pos = goal;
	m_path[1].pos.z = m_lastKnownArea->GetZ( goal );
	m_path[1].ladder = nullptr;
	m_path[1].how = NUM_TRAVERSE_TYPES;

	m_areaEnteredTimestamp = gpGlobals->curtime;
	m_spotEncounter = nullptr;
	m_pathLadder = nullptr;

	m_goalPosition = goal;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute shortest path to goal position via A* algorithm
 * If 'goalArea' is NULL, path will get as close as it can.
 */
bool CCSBot::ComputePath( const Vector &goal, RouteType route )
{
	CVProfScope _( "CCSBot::ComputePath", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	//
	// Throttle re-pathing
	//
	if (!m_repathTimer.IsElapsed())
		return false;

	// randomize to distribute CPU load
	m_repathTimer.Start( RandomFloat( 0.4f, 0.6f ) );

	DestroyPath();

	m_pathLadder = nullptr;

	CNavArea *goalArea = TheNavMesh->GetNearestNavArea( goal );

	CNavArea *startArea = m_lastKnownArea;
	if (startArea == nullptr)
		return false;

	// if we fell off a ledge onto an area off the mesh, we will path from the
	// ledge above our heads, resulting in a path we can't follow.
	Vector close;
	startArea->GetClosestPointOnArea( EyePosition(), &close );
	if (close.z - GetAbsOrigin().z > JumpCrouchHeight)
	{
		// we can't reach our last known area - find nearest area to us
		PrintIfWatched( "Last known area is above my head - resetting to nearest area.\n" );
		m_lastKnownArea = (CNavArea*)TheNavMesh->GetNearestNavArea( GetAbsOrigin(), false, 500.0f, true );
		if (m_lastKnownArea == nullptr)
		{
			return false;
		}

		startArea = m_lastKnownArea;
	}

	// note final specific position
	Vector pathEndPosition = goal;

	// make sure path end position is on the ground
	if (goalArea)
		pathEndPosition.z = goalArea->GetZ( pathEndPosition );
	else
		TheNavMesh->GetGroundHeight( pathEndPosition, &pathEndPosition.z );

	// if we are already in the goal area, build trivial path
	if (startArea == goalArea)
	{
		BuildTrivialPath( pathEndPosition );
		return true;
	}

	TheCSBots()->OnExpensiveBotOperation();

	//
	// Compute shortest path to goal
	//
	CNavArea *closestArea = nullptr;
	PathCost cost( this, route );
	bool pathToGoalExists = NavAreaBuildPath( startArea, goalArea, &goal, cost, &closestArea );

	CNavArea *effectiveGoalArea = (pathToGoalExists) ? goalArea : closestArea;

	//
	// Build path by following parent links
	//

	// get count
	int count = 0;
	CNavArea *area;
	for( area = effectiveGoalArea; area; area = area->GetParent() )
		++count;

	// save room for endpoint
	if (count > MAX_PATH_LENGTH-1)
		count = MAX_PATH_LENGTH-1;

	if (count == 0)
		return false;

	if (count == 1)
	{
		BuildTrivialPath( pathEndPosition );
		return true;
	}

	// build path
	m_pathLength = count;
	for( area = effectiveGoalArea; count && area; area = area->GetParent() )
	{
		--count;
		m_path[ count ].area = area;
		m_path[ count ].how = area->GetParentHow();
	}

	// compute path positions
	if (ComputePathPositions() == false)
	{
		PrintIfWatched( "Error building path\n" );
		DestroyPath();
		return false;
	}

	// append path end position
	m_path[ m_pathLength ].area = effectiveGoalArea;
	m_path[ m_pathLength ].pos = pathEndPosition;
	m_path[ m_pathLength ].ladder = nullptr;
	m_path[ m_pathLength ].how = NUM_TRAVERSE_TYPES;
	++m_pathLength;

	// do movement setup
	m_pathIndex = 1;
	m_areaEnteredTimestamp = gpGlobals->curtime;
	m_spotEncounter = nullptr;
	m_goalPosition = m_path[1].pos;

	if (m_path[1].ladder)
		SetupLadderMovement();
	else
		m_pathLadder = nullptr;

	// find initial encounter area along this path, if we are in the early part of the round
	if (IsSafe())
	{
		int myTeam = GetTeamNumber();
		int enemyTeam = OtherTeam( myTeam );
		int i;

		for( i=0; i<m_pathLength; ++i )
		{
			if (m_path[i].area->GetEarliestOccupyTime( myTeam ) > m_path[i].area->GetEarliestOccupyTime( enemyTeam ))
			{
				break;
			}
		}

		if (i < m_pathLength)
		{
			SetInitialEncounterArea( m_path[i].area );
		}
		else
		{
			SetInitialEncounterArea( nullptr );
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return estimated distance left to travel along path
 */
float CCSBot::GetPathDistanceRemaining( void ) const
{
	if (!HasPath())
		return -1.0f;

	int idx = (m_pathIndex < m_pathLength) ? m_pathIndex : m_pathLength-1;

	float dist = 0.0f;
	Vector prevCenter = m_path[m_pathIndex].area->GetCenter();

	for( int i=idx+1; i<m_pathLength; ++i )
	{
		dist += (m_path[i].area->GetCenter() - prevCenter).Length();
		prevCenter = m_path[i].area->GetCenter();
	}

	return dist;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw a portion of our current path for debugging.
 */
void CCSBot::DrawPath( void )
{
	if (!HasPath())
		return;

	for( int i=1; i<m_pathLength; ++i )
	{
		UTIL_DrawBeamPoints( m_path[i-1].pos, m_path[i].pos, 2, 255, 75, 0 );
	}

	Vector close;
	if (FindOurPositionOnPath( &close, true ) >= 0)
	{
		UTIL_DrawBeamPoints( close + Vector( 0, 0, 25 ), close, 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 25, 0, 0 ), close + Vector( -25, 0, 0 ), 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 0, 25, 0 ), close + Vector( 0, -25, 0 ), 1, 0, 255, 0 );
	}
}

class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CRecipientFilter;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

extern int gmsgBotVoice;

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if the radio message is an order to do something
 * NOTE: "Report in" is not considered a "command" because it doesnt ask the bot to go somewhere, or change its mind
 */
bool CCSBot::IsRadioCommand( RadioType event ) const
{
	if (event == RADIO_AFFIRMATIVE ||
		event == RADIO_NEGATIVE ||
		event == RADIO_ENEMY_SPOTTED ||
		event == RADIO_SECTOR_CLEAR ||
		event == RADIO_REPORTING_IN ||
		event == RADIO_REPORT_IN_TEAM ||
		event == RADIO_ENEMY_DOWN ||
		event == RADIO_INVALID )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Respond to radio commands from HUMAN players
 */
void CCSBot::RespondToRadioCommands( void )
{
	// bots use the chatter system to respond to each other
	if (m_radioSubject != nullptr && m_radioSubject->IsPlayer())
	{
		CCSPlayer *player = m_radioSubject;
		if (player->IsBot())
		{
			m_lastRadioCommand = RADIO_INVALID;
			return;
		}
	}

	if (m_lastRadioCommand == RADIO_INVALID)
		return;

	// a human player has issued a radio command
	GetChatter()->ResetRadioSilenceDuration();

	// if we are doing something important, ignore the radio
	// unless it is a "report in" request - we can do that while we continue to do other things
	/// @todo Create "uninterruptable" flag
	if (m_lastRadioCommand != RADIO_REPORT_IN_TEAM)
	{
		if (IsBusy())
		{
			// consume command
			m_lastRadioCommand = RADIO_INVALID;
			return;
		}
	}

	// wait for reaction time before responding
	// delay needs to be long enough for the radio message we're responding to to finish
	float respondTime = 1.0f + 2.0f * GetProfile()->GetReactionTime();
	if (IsRogue())
		respondTime += 2.0f;

	if (gpGlobals->curtime - m_lastRadioRecievedTimestamp < respondTime)
		return;

	// rogues won't follow commands, unless already following the player
	if (!IsFollowing() && IsRogue())
	{
		if (IsRadioCommand( m_lastRadioCommand ))
		{
			GetChatter()->Negative();
		}

		// consume command
		m_lastRadioCommand = RADIO_INVALID;
		return;
	}

	CCSPlayer *player = m_radioSubject;
	if (player == nullptr)
		return;

	// respond to command
	bool canDo = false;
	const float inhibitAutoFollowDuration = 60.0f;
	switch( m_lastRadioCommand )
	{
		case RADIO_REPORT_IN_TEAM:
		{
			GetChatter()->ReportingIn();
			break;
		}

		case RADIO_FOLLOW_ME:
		case RADIO_COVER_ME:
		case RADIO_STICK_TOGETHER_TEAM:
		case RADIO_REGROUP_TEAM:
		{
			if (!IsFollowing())
			{
				Follow( player );
				player->AllowAutoFollow();
				canDo = true;
			}
			break;
		}

		case RADIO_ENEMY_SPOTTED:
		case RADIO_NEED_BACKUP:
		case RADIO_TAKING_FIRE:
			if (!IsFollowing())
			{
				Follow( player );
				GetChatter()->Say( "OnMyWay" );
				player->AllowAutoFollow();
				canDo = false;
			}
			break;

		case RADIO_TEAM_FALL_BACK:
		{
			if (TryToRetreat())
				canDo = true;
			break;
		}

		case RADIO_HOLD_THIS_POSITION:
		{
			// find the leader's area
			SetTask( HOLD_POSITION );
			StopFollowing();
			player->InhibitAutoFollow( inhibitAutoFollowDuration );
			Hide( TheNavMesh->GetNearestNavArea( m_radioPosition ) );
			canDo = true;
			break;
		}

		case RADIO_GO_GO_GO:
		case RADIO_STORM_THE_FRONT:
			StopFollowing();
			Hunt();
			canDo = true;
			player->InhibitAutoFollow( inhibitAutoFollowDuration );
			break;

		case RADIO_GET_OUT_OF_THERE:
			if (TheCSBots()->IsBombPlanted())
			{
				EscapeFromBomb();
				player->InhibitAutoFollow( inhibitAutoFollowDuration );
				canDo = true;
			}
			break;

		case RADIO_SECTOR_CLEAR:
		{
			// if this is a defusal scenario, and the bomb is planted,
			// and a human player cleared a bombsite, check it off our list too
			if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
			{
				if (GetTeamNumber() == TEAM_CT && TheCSBots()->IsBombPlanted())
				{
					const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( player );

					if (zone)
					{
						GetGameState()->ClearBombsite( zone->m_index );

						// if we are huting for the planted bomb, re-select bombsite
						if (GetTask() == FIND_TICKING_BOMB)
							Idle();

						canDo = true;
					}
				}
			}
			break;
		}

		default:
			// ignore all other radio commands for now
			return;
	}

	if (canDo)
	{
		// affirmative
		GetChatter()->Affirmative();

		// if we agreed to follow a new command, put away our grenade
		if (IsRadioCommand( m_lastRadioCommand ) && IsUsingGrenade())
		{
			EquipBestWeapon();
		}
	}

	// consume command
	m_lastRadioCommand = RADIO_INVALID;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decide if we should move to help the player, return true if we will
 */
bool CCSBot::RespondToHelpRequest( CCSPlayer *them, Place place, float maxRange )
{
	if (IsRogue())
		return false;

	// if we're busy, ignore
	if (IsBusy())
		return false;

	Vector themOrigin = GetCentroid( them );

	// if we are too far away, ignore
	if (maxRange > 0.0f)
	{
		// compute actual travel distance
		PathCost cost(this);
		float travelDistance = NavAreaTravelDistance( m_lastKnownArea, TheNavMesh->GetNearestNavArea( themOrigin ), cost );
		if (travelDistance < 0.0f)
			return false;

		if (travelDistance > maxRange)
			return false;
	}

	if (place == UNDEFINED_PLACE)
	{
		// if we have no "place" identifier, go directly to them

		// if we are already there, ignore
		float rangeSq = (them->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		const float close = 750.0f * 750.0f;
		if (rangeSq < close)
			return true;

		MoveTo( themOrigin, FASTEST_ROUTE );
	}
	else
	{
		// if we are already there, ignore
		if (GetPlace() == place)
			return true;

		// go to where help is needed
		Vector pos;
		if ( GetRandomSpotAtPlace( place, &pos ) )
		{
			MoveTo( pos, FASTEST_ROUTE );
		}
		else
		{
			MoveTo( themOrigin, FASTEST_ROUTE );
		}
	}

	// acknowledge
	GetChatter()->Say( "OnMyWay" );

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Send a radio message
 */
void CCSBot::SendRadioMessage( RadioType event )
{
	// make sure this is a radio event
	if (event <= RADIO_START_1 || event >= RADIO_END)
		return;

	PrintIfWatched( "%3.1f: SendRadioMessage( %s )\n", gpGlobals->curtime, RadioEventName[ event ] );

	// note the time the message was sent
	TheCSBots()->SetRadioMessageTimestamp( event, GetTeamNumber() );

	m_lastRadioSentTimestamp = gpGlobals->curtime;

	char slot[2];
	slot[1] = '\000';

	if (event > RADIO_START_1 && event < RADIO_START_2)
	{
		HandleMenu_Radio1( event );
	}
	else if (event > RADIO_START_2 && event < RADIO_START_3)
	{
		HandleMenu_Radio2( event );
	}
	else
	{
		HandleMenu_Radio3( event );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Send voice chatter.  Also sends the entindex and duration for voice feedback.
 */
void CCSBot::SpeakAudio( const char *voiceFilename, float duration, int pitch )
{
	if( !IsAlive() )
		return;

	if ( IsObserver() )
		return;

	CRecipientFilter filter;
	ConstructRadioFilter( filter );

	ks::net::CCSUsrMsg_RawAudio msg;

	msg.pitch = pitch;
	msg.entidx = entindex();
	msg.duration = duration;
	msg.voice_filename = voiceFilename;

	SendUserMessage ( filter, ks::net::CS_UM_RawAudio, msg );

	GetChatter()->ResetRadioSilenceDuration();

	m_voiceEndTimestamp = gpGlobals->curtime + duration;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Send voice chatter through the response rules system.
 */
bool CCSBot::SpeakAudioResponseRules( const char *pConcept, AI_CriteriaSet *criteria, float duration )
{
	if( !IsAlive() )
		return false;

	if ( IsObserver() )
		return false;

	CRecipientFilter filter;
	ConstructRadioFilter( filter );

	AI_CriteriaSet local;
	if ( !criteria )
		criteria = &local;

	AIConcept_t conc( pConcept );
	if ( Speak( conc, criteria, nullptr, 0, &filter ) )
	{
		GetChatter()->ResetRadioSilenceDuration();
		m_voiceEndTimestamp = gpGlobals->curtime + duration;
		return true;
	}

	return false;
}

class CBaseEntity;
class CCSBot;
class CCSPlayer;
class CNavArea;
class HidingSpot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
// BotState methods: forward to the concrete state according to the Kind
//
void BotState::OnEnter( CCSBot *bot )
{
	switch ( m_kind )
	{
	case Kind::Idle:	static_cast< IdleState * >( this )->OnEnter( bot );	break;
	case Kind::Hunt:	static_cast< HuntState * >( this )->OnEnter( bot );	break;
	case Kind::Attack:	static_cast< AttackState * >( this )->OnEnter( bot );	break;
	case Kind::InvestigateNoise:	static_cast< InvestigateNoiseState * >( this )->OnEnter( bot );	break;
	case Kind::Buy:	static_cast< BuyState * >( this )->OnEnter( bot );	break;
	case Kind::MoveTo:	static_cast< MoveToState * >( this )->OnEnter( bot );	break;
	case Kind::FetchBomb:	static_cast< FetchBombState * >( this )->OnEnter( bot );	break;
	case Kind::PlantBomb:	static_cast< PlantBombState * >( this )->OnEnter( bot );	break;
	case Kind::DefuseBomb:	static_cast< DefuseBombState * >( this )->OnEnter( bot );	break;
	case Kind::PickupHostage:	static_cast< PickupHostageState * >( this )->OnEnter( bot );	break;
	case Kind::Hide:	static_cast< HideState * >( this )->OnEnter( bot );	break;
	case Kind::EscapeFromBomb:	static_cast< EscapeFromBombState * >( this )->OnEnter( bot );	break;
	case Kind::Follow:	static_cast< FollowState * >( this )->OnEnter( bot );	break;
	case Kind::UseEntity:	static_cast< UseEntityState * >( this )->OnEnter( bot );	break;
	case Kind::OpenDoor:	static_cast< OpenDoorState * >( this )->OnEnter( bot );	break;
	case Kind::EscapeFromFlames:	static_cast< EscapeFromFlamesState * >( this )->OnEnter( bot );	break;
	}
}

void BotState::OnUpdate( CCSBot *bot )
{
	switch ( m_kind )
	{
	case Kind::Idle:	static_cast< IdleState * >( this )->OnUpdate( bot );	break;
	case Kind::Hunt:	static_cast< HuntState * >( this )->OnUpdate( bot );	break;
	case Kind::Attack:	static_cast< AttackState * >( this )->OnUpdate( bot );	break;
	case Kind::InvestigateNoise:	static_cast< InvestigateNoiseState * >( this )->OnUpdate( bot );	break;
	case Kind::Buy:	static_cast< BuyState * >( this )->OnUpdate( bot );	break;
	case Kind::MoveTo:	static_cast< MoveToState * >( this )->OnUpdate( bot );	break;
	case Kind::FetchBomb:	static_cast< FetchBombState * >( this )->OnUpdate( bot );	break;
	case Kind::PlantBomb:	static_cast< PlantBombState * >( this )->OnUpdate( bot );	break;
	case Kind::DefuseBomb:	static_cast< DefuseBombState * >( this )->OnUpdate( bot );	break;
	case Kind::PickupHostage:	static_cast< PickupHostageState * >( this )->OnUpdate( bot );	break;
	case Kind::Hide:	static_cast< HideState * >( this )->OnUpdate( bot );	break;
	case Kind::EscapeFromBomb:	static_cast< EscapeFromBombState * >( this )->OnUpdate( bot );	break;
	case Kind::Follow:	static_cast< FollowState * >( this )->OnUpdate( bot );	break;
	case Kind::UseEntity:	static_cast< UseEntityState * >( this )->OnUpdate( bot );	break;
	case Kind::OpenDoor:	static_cast< OpenDoorState * >( this )->OnUpdate( bot );	break;
	case Kind::EscapeFromFlames:	static_cast< EscapeFromFlamesState * >( this )->OnUpdate( bot );	break;
	}
}

void BotState::OnExit( CCSBot *bot )
{
	switch ( m_kind )
	{
	case Kind::Idle:	break;	// no exit action
	case Kind::Hunt:	static_cast< HuntState * >( this )->OnExit( bot );	break;
	case Kind::Attack:	static_cast< AttackState * >( this )->OnExit( bot );	break;
	case Kind::InvestigateNoise:	static_cast< InvestigateNoiseState * >( this )->OnExit( bot );	break;
	case Kind::Buy:	static_cast< BuyState * >( this )->OnExit( bot );	break;
	case Kind::MoveTo:	static_cast< MoveToState * >( this )->OnExit( bot );	break;
	case Kind::FetchBomb:	break;	// no exit action
	case Kind::PlantBomb:	static_cast< PlantBombState * >( this )->OnExit( bot );	break;
	case Kind::DefuseBomb:	static_cast< DefuseBombState * >( this )->OnExit( bot );	break;
	case Kind::PickupHostage:	static_cast< PickupHostageState * >( this )->OnExit( bot );	break;
	case Kind::Hide:	static_cast< HideState * >( this )->OnExit( bot );	break;
	case Kind::EscapeFromBomb:	static_cast< EscapeFromBombState * >( this )->OnExit( bot );	break;
	case Kind::Follow:	static_cast< FollowState * >( this )->OnExit( bot );	break;
	case Kind::UseEntity:	static_cast< UseEntityState * >( this )->OnExit( bot );	break;
	case Kind::OpenDoor:	static_cast< OpenDoorState * >( this )->OnExit( bot );	break;
	case Kind::EscapeFromFlames:	static_cast< EscapeFromFlamesState * >( this )->OnExit( bot );	break;
	}
}

const char *BotState::GetName( void ) const
{
	switch ( m_kind )
	{
	case Kind::Idle:	return static_cast< const IdleState * >( this )->GetName();
	case Kind::Hunt:	return static_cast< const HuntState * >( this )->GetName();
	case Kind::Attack:	return static_cast< const AttackState * >( this )->GetName();
	case Kind::InvestigateNoise:	return static_cast< const InvestigateNoiseState * >( this )->GetName();
	case Kind::Buy:	return static_cast< const BuyState * >( this )->GetName();
	case Kind::MoveTo:	return static_cast< const MoveToState * >( this )->GetName();
	case Kind::FetchBomb:	return static_cast< const FetchBombState * >( this )->GetName();
	case Kind::PlantBomb:	return static_cast< const PlantBombState * >( this )->GetName();
	case Kind::DefuseBomb:	return static_cast< const DefuseBombState * >( this )->GetName();
	case Kind::PickupHostage:	return static_cast< const PickupHostageState * >( this )->GetName();
	case Kind::Hide:	return static_cast< const HideState * >( this )->GetName();
	case Kind::EscapeFromBomb:	return static_cast< const EscapeFromBombState * >( this )->GetName();
	case Kind::Follow:	return static_cast< const FollowState * >( this )->GetName();
	case Kind::UseEntity:	return static_cast< const UseEntityState * >( this )->GetName();
	case Kind::OpenDoor:	return static_cast< const OpenDoorState * >( this )->GetName();
	case Kind::EscapeFromFlames:	return static_cast< const EscapeFromFlamesState * >( this )->GetName();
	}

	return "";
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This method is the ONLY legal way to change a bot's current state
 */
void CCSBot::SetState( BotState *state )
{
	PrintIfWatched( "%s: SetState: %s -> %s\n", GetPlayerName(), (m_state) ? m_state->GetName() : "NULL", state->GetName() );

	/*
	if ( IsDefusingBomb() )
	{
		const Vector *bombPos = GetGameState()->GetBombPosition();
		if ( bombPos != NULL )
		{
			if ( TheCSBots()->GetBombDefuser() == this )
			{
				if ( TheCSBots()->IsBombPlanted() )
				{
					Msg( "Bot %s is switching from defusing the bomb to %s\n",
						GetPlayerName(), state->GetName() );
				}
			}
		}
	}
	*/

	// if we changed state from within the special Attack state, we are no longer attacking
	if (m_isAttacking)
		StopAttacking();

	if (m_state)
		m_state->OnExit( this );

	state->OnEnter( this );

	m_state = state;
	m_stateTimestamp = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Idle( void )
{
	SetTask( SEEK_AND_DESTROY );
	SetState( &m_idleState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::EscapeFromBomb( void )
{
	SetTask( ESCAPE_FROM_BOMB );
	SetState( &m_escapeFromBombState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::EscapeFromFlames( void )
{
	SetTask( ESCAPE_FROM_FLAMES );
	SetState( &m_escapeFromFlamesState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Follow( CCSPlayer *player )
{
	if (player == nullptr)
		return;

	// note when we began following
	if (!m_isFollowing || m_leader != player)
		m_followTimestamp = gpGlobals->curtime;

	m_isFollowing = true;
	m_leader = player;

	SetTask( FOLLOW );
	m_followState.SetLeader( player );
	SetState( &m_followState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Continue following our leader after finishing what we were doing
 */
void CCSBot::ContinueFollowing( void )
{
	SetTask( FOLLOW );

	m_followState.SetLeader( m_leader );

	SetState( &m_followState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Stop following
 */
void CCSBot::StopFollowing( void )
{
	m_isFollowing = false;
	m_leader = nullptr;
	m_allowAutoFollowTime = gpGlobals->curtime + 10.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin process of rescuing hostages
 */
void CCSBot::RescueHostages( void )
{
	SetTask( RESCUE_HOSTAGES );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Use the entity
 */
void CCSBot::UseEntity( CBaseEntity *entity )
{
	m_useEntityState.SetEntity( entity );
	SetState( &m_useEntityState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Open the door.
 * This assumes the bot is directly in front of the door with no obstructions.
 * NOTE: This state is special, like Attack, in that it suspends the current behavior and returns to it when done.
 */
void CCSBot::OpenDoor( CBaseEntity *door )
{
	m_openDoorState.SetDoor( door );
	m_isOpeningDoor = true;
	m_openDoorState.OnEnter( this );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use TryToHide() instead.
 * Move to a hiding place.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
void CCSBot::Hide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition )
{
	if ( !TheCSBots()->AllowedToDoExpensiveBotOperationThisFrame() )
		return;

	DestroyPath();

	CNavArea *source;
	Vector sourcePos;
	if (searchFromArea)
	{
		source = searchFromArea;
		sourcePos = searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = GetCentroid( this );
	}

	if (source == nullptr)
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		Idle();
		return;
	}

	m_hideState.SetSearchArea( source );
	m_hideState.SetSearchRange( hideRange );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );

	// search around source area for a good hiding spot
	Vector useSpot;

	const Vector *pos = FindNearbyHidingSpot( this, sourcePos, hideRange, IsSniper() );
	if (pos == nullptr)
	{
		PrintIfWatched( "No available hiding spots.\n" );
		// hide at our current position
		useSpot = GetCentroid( this );
	}
	else
	{
		useSpot = *pos;
	}

	m_hideState.SetHidingSpot( useSpot );

	// build a path to our new hiding spot
	if (ComputePath( useSpot, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the given hiding place
 */
void CCSBot::Hide( const Vector &hidingSpot, float duration, bool holdPosition )
{
	CNavArea *hideArea = TheNavMesh->GetNearestNavArea( hidingSpot );
	if (hideArea == nullptr)
	{
		PrintIfWatched( "Hiding spot off nav mesh\n" );
		Idle();
		return;
	}

	DestroyPath();

	m_hideState.SetSearchArea( hideArea );
	m_hideState.SetSearchRange( 750.0f );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );
	m_hideState.SetHidingSpot( hidingSpot );

	// build a path to our new hiding spot
	if (ComputePath( hidingSpot, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Try to hide nearby.  Return true if hiding, false if can't hide here.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
bool CCSBot::TryToHide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition, bool useNearest, const Vector *pStartPosOverride /*= NULL*/ )
{
	CNavArea *source;
	Vector sourcePos;
	if (searchFromArea)
	{
		source = searchFromArea;
		sourcePos = searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = GetCentroid( this );
	}
	
	// Optionally force the starting position instead of using the center of an area... 
	if ( pStartPosOverride )
		sourcePos = *pStartPosOverride;

	if (source == nullptr)
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		return false;
	}

	m_hideState.SetSearchArea( source );
	m_hideState.SetSearchRange( hideRange );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );

	// search around source area for a good hiding spot
	const Vector *pos = FindNearbyHidingSpot( this, sourcePos, hideRange, IsSniper(), useNearest );
	if (pos == nullptr)
	{
		PrintIfWatched( "No available hiding spots.\n" );
		return false;
	}

	m_hideState.SetHidingSpot( *pos );

	// build a path to our new hiding spot
	if (ComputePath( *pos, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		return false;
	}

	SetState( &m_hideState );
	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Retreat to a nearby hiding spot, away from enemies
 */
bool CCSBot::TryToRetreat( float maxRange, float duration )
{
	// We don't want heavies to retreat
	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() && HasHeavyArmor() )
		return false;

	const Vector *spot = FindNearbyRetreatSpot( this, maxRange );
	if (spot)
	{
		// ignore enemies for a second to give us time to hide
		// reaching our hiding spot clears our disposition
		IgnoreEnemies( 10.0f );

		if (duration < 0.0f)
		{
			duration = RandomFloat( 3.0f, 15.0f );
		}

		StandUp();
		Run();
		Hide( *spot, duration );

		PrintIfWatched( "Retreating to a safe spot!\n" );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Hunt( void )
{
	SetState( &m_huntState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Attack our the given victim
 * NOTE: Attacking does not change our task.
 */
void CCSBot::Attack( CCSPlayer *victim )
{
	if (victim == nullptr)
		return;

	// zombies never attack
	if (cv_bot_zombie.GetBool())
		return;

	// cannot attack if we are reloading
	if (IsReloading())
		return;

	// change enemy
	SetBotEnemy( victim );

	//
	// Do not "re-enter" the attack state if we are already attacking
	//
	if (IsAttacking())
		return;

	if (IsUsingGrenade())
	{
		// throw towards their feet
		ThrowGrenade( victim->GetAbsOrigin() );
		return;
	}

	// if we are currently hiding, increase our chances of crouching and holding position
	if (IsAtHidingSpot())
		m_attackState.SetCrouchAndHold( (RandomFloat( 0.0f, 100.0f ) < 60.0f) ? true : false );
	else
		m_attackState.SetCrouchAndHold( false );

	//SetState( &m_attackState );
	//PrintIfWatched( "ATTACK BEGIN (reaction time = %g (+ update time), surprise time = %g, attack delay = %g)\n", 
	//				GetProfile()->GetReactionTime(), m_surpriseDelay, GetProfile()->GetAttackDelay() );
	m_isAttacking = true;
	m_attackState.OnEnter( this );

	Vector victimOrigin = GetCentroid( victim );

	// cheat a bit and give the bot the initial location of its victim
	m_lastEnemyPosition = victimOrigin;
	m_lastSawEnemyTimestamp = gpGlobals->curtime;

	m_aimFocus = GetProfile()->GetAimFocusInitial();

	PickNewAimSpot();

	// forget any look at targets we have
	ClearLookAt();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Exit the Attack state
 */
void CCSBot::StopAttacking( void )
{
	PrintIfWatched( "ATTACK END\n" );
	m_attackState.OnExit( this );
	m_isAttacking = false;

	// if we are following someone, go to the Idle state after the attack to decide whether we still want to follow
	if (IsFollowing())
	{
		Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsAttacking( void ) const
{
	return m_isAttacking;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are escaping from the bomb
 */
bool CCSBot::IsEscapingFromBomb( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_escapeFromBombState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are escaping from a field of flames
 */
bool CCSBot::IsEscapingFromFlames( void ) const
{
	if ( m_state == static_cast< const BotState * >( &m_escapeFromFlamesState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are defusing the bomb
 */
bool CCSBot::IsDefusingBomb( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_defuseBombState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are defusing the bomb
 */
bool CCSBot::IsPickingupHostage( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_pickupHostageState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding
 */
bool CCSBot::IsHiding( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_hideState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding and at our hiding spot
 */
bool CCSBot::IsAtHidingSpot( void ) const
{
	if (!IsHiding())
		return false;

	return m_hideState.IsAtSpot();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of seconds we have been at our current hiding spot
 */
float CCSBot::GetHidingTime( void ) const
{
	if (IsHiding())
	{
		return m_hideState.GetHideTime();
	}

	return 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are huting
 */
bool CCSBot::IsHunting( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_huntState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are in the MoveTo state
 */
bool CCSBot::IsMovingTo( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_moveToState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are buying
 */
bool CCSBot::IsBuying( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_buyState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsInvestigatingNoise( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_investigateNoiseState ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsIdling( void ) const
{
	if ( m_state == static_cast< const BotState * >( &m_idleState ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to potentially distant position
 */
void CCSBot::MoveTo( const Vector &pos, RouteType route )
{
	m_moveToState.SetGoalPosition( pos );
	m_moveToState.SetRouteType( route );
	SetState( &m_moveToState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::PlantBomb( void )
{
	SetState( &m_plantBombState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Bomb has been dropped - go get it
 */
void CCSBot::FetchBomb( void )
{
	SetState( &m_fetchBombState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::DefuseBomb( void )
{
	SetState( &m_defuseBombState );
}

void CCSBot::PickupHostage( CBaseEntity *entity )
{
	m_pickupHostageState.SetEntity( entity );
	SetState( &m_pickupHostageState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Investigate recent enemy noise
 */
void CCSBot::InvestigateNoise( void )
{
	SetState( &m_investigateNoiseState );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::Buy( void )
{
	SetState( &m_buyState );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a hiding spot and wait for initial encounter with enemy team.
 * Return false if can't do this behavior (ie: no hiding spots available).
 */
bool CCSBot::MoveToInitialEncounter( void )
{
	int myTeam = GetTeamNumber();
	int enemyTeam = OtherTeam( myTeam );

	// build a path to an enemy spawn point
	CBaseEntity *enemySpawn = TheCSBots()->GetRandomSpawn( enemyTeam );

	if (enemySpawn == nullptr)
	{
		PrintIfWatched( "MoveToInitialEncounter: No enemy spawn points?\n" );
		return false;
	}

	if ( !TheCSBots()->AllowedToDoExpensiveBotOperationThisFrame() )
		return false;

	TheCSBots()->OnExpensiveBotOperation();

	// build a path from us to the enemy spawn
	CCSNavPath path;
	PathCost cost( this, FASTEST_ROUTE );
	path.Compute( WorldSpaceCenter(), enemySpawn->GetAbsOrigin(), cost );

	if (!path.IsValid())
	{
		PrintIfWatched( "MoveToInitialEncounter: Pathfind failed.\n" );
		return false;
	}

	// find battlefront area where teams will first meet along this path
	int i;
	for( i=0; i<path.GetSegmentCount(); ++i )
	{
		if (path[i]->area->GetEarliestOccupyTime( myTeam ) > path[i]->area->GetEarliestOccupyTime( enemyTeam ))
		{
			break;
		}
	}

	if (i == path.GetSegmentCount())
	{
		PrintIfWatched( "MoveToInitialEncounter: Can't find battlefront!\n" );
		return false;
	}

	/// @todo Remove this evil side-effect
	SetInitialEncounterArea( path[i]->area );

	// find a hiding spot on our side of the battlefront that has LOS to it
	const float maxRange = 1500.0f;
	const HidingSpot *spot = FindInitialEncounterSpot( this, path[i]->area->GetCenter(), path[i]->area->GetEarliestOccupyTime( enemyTeam ), maxRange, IsSniper() );

	if (spot == nullptr)
	{
		PrintIfWatched( "MoveToInitialEncounter: Can't find a hiding spot\n" );
		return false;
	}

	float timeToWait = path[i]->area->GetEarliestOccupyTime( enemyTeam ) - spot->GetArea()->GetEarliestOccupyTime( myTeam );
	float minWaitTime = 4.0f * GetProfile()->GetAggression() + 3.0f;
	if (timeToWait < minWaitTime)
	{
		timeToWait = minWaitTime;
	}

	Hide( spot->GetPosition(), timeToWait );

	return true;
}
