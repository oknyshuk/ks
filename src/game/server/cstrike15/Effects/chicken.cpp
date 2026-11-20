// SPDX-License-Identifier: None

module;
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "../../ai_criteria.h"
#include "../../baseentity.h"
#include "../../entity_dispatch.h"
#include "../../entity_dispatch_list.h"
#include "../../entity_kinds_dump.h"
#include "../../reflect_sendtable.h"
#include "../../util.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
module ks.server;

import std;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.fmtstr;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import ks.game.ai_activity;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.Effects.chicken;
import :cstrike15.bot.bot;
import :cstrike15.cs_player;
import :cstrike15.hostage.cs_simple_hostage;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :cstrike15.cs_playeranimstate;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.numeric;
import ks.core.vprof;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import :econ.econ_item_schema_minimal;

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CImprovLocomotor;
class CNavArea;
class CNavLadder;
class CTakeDamageInfo;
class HostagePathCost;

using namespace ::ks::reflect;

// chicken.cpp
// An interactive, shootable chicken

IMPLEMENT_REFLECT_SERVERCLASS( CChicken, DT_CChicken )

LINK_ENTITY_TO_CLASS( chicken, CChicken );
PRECACHE_REGISTER( chicken );


extern ConVar hostage_debug;

static constexpr auto CHICKEN_ZOMBIE_SPAWN_DURATION = 3.5f;

static constexpr auto CHICKEN_DISTANCE_RUN = 200.0f;
static constexpr auto CHICKEN_DISTANCE_WALK = 100.0f;

//-----------------------------------------------------------------------
CChicken::CChicken() : CImprovLocomotor( CImprovLocomotor::Kind::Chicken )
{
}

//-----------------------------------------------------------------------
CChicken::~CChicken()
{
}

//-----------------------------------------------------------------------
void CChicken::Precache_Impl( void )
{
	SetModelName( MAKE_STRING( "models/chicken/chicken.mdl" ) );	// prop precache wants this

	BaseClass::Precache_Impl();

	PrecacheModel( "models/chicken/chicken.mdl" );
	PrecacheModel( "models/chicken/chicken_zombie.mdl" );
	PrecacheModel( "models/chicken/chicken_gone.mdl" );
	PrecacheModel( "models/antlers/antlers.mdl" );

	PrecacheScriptSound( "Chicken.Idle" );
	PrecacheScriptSound( "Chicken.Panic" );
	PrecacheScriptSound( "Chicken.Fly" );
	PrecacheScriptSound( "Chicken.FlapWings" );
	PrecacheScriptSound( "Chicken.Death" );
	PrecacheScriptSound( "Chicken.ZombieRez" );

	PrecacheParticleSystem( "weapon_confetti_omni" );
	PrecacheParticleSystem( "chicken_rez" );
	PrecacheParticleSystem( "chicken_gone_crumble_halloween" );
	PrecacheParticleSystem( "chicken_gone_zombie" );

	PrecacheParticleSystem( "impact_helmet_headshot" );
}

//-----------------------------------------------------------------------
void CChicken::Spawn_Impl( void )
{

	SetModel( "models/chicken/chicken.mdl" );

	BaseClass::Spawn_Impl();

	SetNextThink( gpGlobals->curtime );
	SetThink( &CChicken::ChickenThink );

	SetTouch( &CChicken::ChickenTouch );

	SetUse( &CChicken::ChickenUse );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );

	const model_t *pModel = modelinfo->GetModel( GetModelIndex() );
	if ( pModel )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( pModel, mins, maxs );
		mins.z = 0.0f;
		SetCollisionBounds( mins, maxs );
	}

	SetGravity( 1.0 );

	SetHealth( 1 );
	SetMaxHealth( 1 );
	m_takedamage = DAMAGE_YES;

	Idle();
	m_fleeFrom = nullptr;
	m_updateTimer.Invalidate();
	m_reuseTimer.Invalidate( );
	m_moveRateThrottleTimer.Invalidate( );

	m_stuckAnchor = GetAbsOrigin();
	m_stuckTimer.Start( 1.0f );

	m_isOnGround = false;

	m_startleTimer.Invalidate();
	ListenForGameEvent( "weapon_fire" );
	//ListenForGameEvent( "bullet_impact" );

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		SetBodygroup( 1, 1 ); // birthday hat
	}

	m_leader = INVALID_EHANDLE;
	m_reuseTimer.Invalidate( );
	m_hasBeenUsed = false;

	m_jumpedThisFrame = false;

	m_path.Invalidate( );
	m_repathTimer.Invalidate( );

	m_pathFollower.Reset( );
	m_pathFollower.SetPath( &m_path );
	m_pathFollower.SetImprov( this );

	m_lastKnownArea = nullptr;

	// Need to make sure the hostages are on the ground when they spawn
//	Vector GroundPos = DropToGround( this, GetAbsOrigin( ), HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
//	SetAbsOrigin( GroundPos );

	m_bInJump = false;
	m_flLastJumpTime = 0;
	m_inhibitObstacleAvoidanceTimer.Invalidate( );

	m_isWaitingForLeader = false;

	m_lastLeaderID = 0;

	m_flActiveFollowStartTime = 0;

}

//-----------------------------------------------------------------------
void CChicken::ChickenTouch( CBaseEntity *pOther )
{
	if ( !CSGameRules() || CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		Flee( pOther, RandomFloat( 2.0f, 3.0f ) );
	}
}

bool CChicken::IsFollowingSomeone( void )
{
	return ( m_leader.m_Value != nullptr );
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsFollowing( const CBaseEntity *entity )
{
	return ( m_leader.m_Value == entity );
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsOnGround( void ) const
{
	return ( GetFlags( ) & FL_ONGROUND );
}

//-----------------------------------------------------------------------------------------------------
/**
* Begin following "leader"
*/
void CChicken::Follow( CCSPlayer *leader )
{
	m_lastLeaderID = 0;

	if ( leader )
	{
		leader->IncrementNumFollowers( );
		m_lastLeaderID = leader->GetUserID( );
		m_leader = leader;
	}
	else
	{
		m_leader = INVALID_EHANDLE;
	}

	m_leader = leader;
	m_isWaitingForLeader = false;

	m_flActiveFollowStartTime = gpGlobals->curtime;

	m_moveRateThrottleTimer.Start( 1.0f);
}

//-----------------------------------------------------------------------------------------------------
/**
* Return our leader, or NULL
*/
CCSPlayer *CChicken::GetLeader( void ) const
{
	return ToCSPlayer( m_leader.m_Value );
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when a chicken is "used" by a player
*/
void CChicken::ChickenUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{

	CCSPlayer *pPlayer = ToCSPlayer( pActivator );
	if ( !pPlayer )
		return;

	// limit use range
	float useRange = 1000.0f;
	Vector to = pActivator->GetAbsOrigin( ) - GetAbsOrigin( );
	if ( to.IsLengthGreaterThan( useRange ) )
	{
		return;
	}

	SetChickenStartFollowingPlayer( pPlayer );
	
}

void CChicken::SetChickenStartFollowingPlayer( CCSPlayer *pPlayer )
{

	// throttle how often leader can change
	if ( !m_reuseTimer.IsElapsed( ) )
	{
		return;
	}

	// if we are already following the player who used us, stop following
	if ( IsFollowing( pPlayer ) )
	{
		Follow( nullptr );
		Idle( );
		EmitSound( "Chicken.Idle" );
	}
	else
	{
		// if we're already following a CT, ignore new uses
		if ( IsFollowingSomeone( ) )
		{
			return;
		}

		// start following
		Follow( pPlayer );
		EmitSound( "Chicken.FlapWings" );
		Jump( 50.0f );
	}

	m_reuseTimer.Start( 1.0f );
}

//--------------------------------------------------------------------------------------------------------------
/**
* Rotate body to face towards "target"
*/
void CChicken::FaceTowards( const Vector &target, float deltaT )
{
	Vector to = target - GetFeet( );
	to.z = 0.0f;

	QAngle desiredAngles;
	VectorAngles( to, desiredAngles );

	QAngle angles = GetAbsAngles( );

	// The animstate system for hostages will smooth out the transition to this direction, so no need to double smooth it here.
	angles.y = desiredAngles.y;

	SetAbsAngles( angles );
}

int CChicken::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	/*
	if ( EconHolidays_IsHolidayActive( kHoliday_Halloween ) )
	{
		// if we recently turned into a zombie, ignore damage for CHICKEN_ZOMBIE_SPAWN_DURATION seconds.
		if ( IsZombie() )
		{
			if ( (gpGlobals->curtime - m_flWhenZombified) > CHICKEN_ZOMBIE_SPAWN_DURATION )
			{
				return BaseClass::OnTakeDamage( info );
			}
			else
			{
				return 0;
			}
		}
		else
		{
			if ( m_activity != ACT_GLIDE && m_isOnGround )
			{
				//when there's no more room in chicken-hell... turn into a chicken zombie!
				Zombify();

				SetModel( "models/chicken/chicken_zombie.mdl" );
				m_activity = ACT_CLIMB_UP;
				ResetSequence( LookupSequence( "spawn" ) );
				ResetSequenceInfo();
				ForceCycle( 0 );

				DispatchParticleEffect( "chicken_gone_crumble_halloween", GetAbsOrigin() + Vector( 0, 0, 8 ), QAngle( 0, 0, 0 ) );

				CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
				trace_t tr;
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -16 ), MASK_PLAYERSOLID, &traceFilter, &tr );

				Vector	vecPos, vecVel, vecNormal;
				QAngle angNormal;

				vecNormal = tr.DidHit() ? tr.plane.normal : Vector( 0, 0, 0 );
				//vecNormal.y *= -1;
				//vecNormal.z *= -1;
				VectorAngles( vecNormal, angNormal );
				angNormal.x += 90;

				DispatchParticleEffect( "chicken_rez", GetAbsOrigin(), angNormal );

				CSoundParameters params;
				if ( GetParametersForSound( "Chicken.ZombieRez", params, NULL ) )
				{
					EmitSound_t ep( params );
					ep.m_pOrigin = &GetAbsOrigin();

					CBroadcastRecipientFilter filter;
					EmitSound( filter, SOUND_FROM_WORLD, ep );
				}

				SetSolid( SOLID_NONE );

				return 0;
			}
			else
			{
				return BaseClass::OnTakeDamage( info );
			}
		}
	}
	else
	*/
	{
		return BaseClass::OnTakeDamage_Impl( info );
	}
}

//-----------------------------------------------------------------------
void CChicken::Event_Killed_Impl( const CTakeDamageInfo &info )
{
	EmitSound( "Chicken.Death" );

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
		DispatchParticleEffect( "weapon_confetti_omni", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

	if ( IsZombie() )
		DispatchParticleEffect( "chicken_gone_zombie", GetAbsOrigin() + Vector( 0, 0, 8 ), QAngle( 0, 0, 0 ) );

	if ( IsFollowingSomeone( ) )
	{
		CSingleUserRecipientFilter leaderfilter( ToCSPlayer( m_leader.m_Value ) );
		leaderfilter.MakeReliable( );

		float flFollowDuration = gpGlobals->curtime - m_flActiveFollowStartTime;

		UTIL_ClientPrintFilter( leaderfilter, HUD_PRINTTALK, "#Pet_Killed", CFmtStr( "%.0f", flFollowDuration ).Get() );
	}

	BaseClass::Event_Killed_Impl( info );
}

//-----------------------------------------------------------------------
void CChicken::FireGameEvent( IGameEvent *event )
{
	// all the events we care about scare us

	if ( m_startleTimer.HasStarted() )
	{
		// we're already scared
		return;
	}

	if ( event->GetBool( "silenced" ) )
	{
		// Silenced weapons don't scare chickens
		return;
	}

	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player )
	{
		const float fleeGunfireRange = 1000.0f;

		Vector toPlayer = player->GetAbsOrigin() - GetAbsOrigin();

		if ( toPlayer.IsLengthLessThan( fleeGunfireRange ) )
		{
			m_startleTimer.Start( RandomFloat( 0.1f, 0.5f ) );
			m_fleeFrom = player;
		}
	}
}

//-----------------------------------------------------------------------
void CChicken::Idle()
{
//	m_leader = NULL;

	m_activity = ACT_IDLE;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();

	m_pathFollower.ResetStuck( );
}

//-----------------------------------------------------------------------
void CChicken::Walk()
{

	m_activity = ACT_WALK;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );
	m_turnRate = RandomFloat( -45.0f, 45.0f );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}

//-----------------------------------------------------------------------
void CChicken::Flee( CBaseEntity *fleeFrom, float duration )
{
	if ( m_activity != ACT_RUN || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Panic" );
	}

	m_activity = ACT_RUN;
	m_activityTimer.Start( duration );
	m_turnRate = 0.0f;
	m_fleeFrom = fleeFrom;

	SetSequence( SelectWeightedSequence( m_activity ) );
	
	ResetSequenceInfo();
}

//-----------------------------------------------------------------------
void CChicken::Fly()
{
	if ( m_activity != ACT_GLIDE || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Fly" );
	}

	m_activity = ACT_GLIDE;
	m_turnRate = 0.0f;

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}

//-----------------------------------------------------------------------
void CChicken::Land()
{
	if ( m_activity != ACT_LAND || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Idle" );
	}

	m_activity = ACT_LAND;
	m_turnRate = 0.0f;

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}

//-----------------------------------------------------------------------
void CChicken::Update( void )
{
	if ( GetLeader( ) )
		return;

	if ( !m_updateTimer.IsElapsed() )
		return;

	m_updateTimer.Start( RandomFloat( 0.5f, 1.0f ) );

	// find closest visible player
	std::vector< CBasePlayer * > playerVector;
	CollectPlayers( &playerVector, TEAM_ANY, COLLECT_ONLY_LIVING_PLAYERS );

	float closeRangeSq = std::numeric_limits<float>::max();
	CBasePlayer *close = nullptr;

	for( int i=0; i<std::ssize( playerVector ); ++i )
	{
		Vector toPlayer = playerVector[i]->GetAbsOrigin() - GetAbsOrigin();
		float rangeSq = toPlayer.LengthSqr();

		// Only scare chickens if we're close
		if ( rangeSq < closeRangeSq )
		{
			// Only scare chickens if we're moving fast enough to make noise
			Vector vPlayerVelocity;
			playerVector[i]->GetVelocity( &vPlayerVelocity, nullptr );

			if ( vPlayerVelocity.Length() > 126.0f )
			{
				if ( playerVector[i]->IsLineOfSightClear( this ) )
				{
					closeRangeSq = rangeSq;
					close = playerVector[i];
				}
			}
		}
	}

	const float tooClose = 200.0f;

	if ( close && closeRangeSq < tooClose * tooClose )
	{
		Flee( close, RandomFloat( 0.5, 1.0f ) );
	}
}

//-----------------------------------------------------------------------
float CChicken::AvoidObstacles( void )
{
	const float feelerRange = 15.0f;

	const Vector &forward = Forward();

	Vector left( -forward.y, forward.x, 0.0f );
	Vector right( forward.y, -forward.x, 0.0f );

	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	trace_t resultLeft;
	UTIL_TraceLine( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + left ), MASK_PLAYERSOLID, &traceFilter, &resultLeft );
	//NDebugOverlay::Line( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + left ), 0, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );

	trace_t resultRight;
	UTIL_TraceLine( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + right ), MASK_PLAYERSOLID, &traceFilter, &resultRight );
	//NDebugOverlay::Line( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + right ), 255, 0, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );

	const float maxTurnRate = 360.0f;
	float turnRate = 0.0f;

	if ( resultLeft.DidHit() )
	{
		if ( resultRight.DidHit() )
		{
			// both sides hit
			if ( resultLeft.fraction < resultRight.fraction )
			{
				// left hit closer - turn right
				turnRate = -maxTurnRate;
			}
			else
			{
				// right hit closer - turn left
				turnRate = maxTurnRate;
			}
		}
		else
		{
			// left hit - turn right
			turnRate = -maxTurnRate;
		}
	}
	else if ( resultRight.DidHit() )
	{
		// right hit - turn left
		turnRate = maxTurnRate;
	}

	Vector toAnchor = m_stuckAnchor - GetAbsOrigin();

	if ( toAnchor.IsLengthGreaterThan( 50.0f ) )
	{
		m_stuckAnchor = GetAbsOrigin();
		m_stuckTimer.Reset();
	}

	return turnRate;
}

//-----------------------------------------------------------------------
void CChicken::ResolveCollisions( const Vector &desiredPosition, float deltaT )
{
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	trace_t result;

	const float stepHeight = WorldAlignSize().z / 2.0f;

	// try to do full move
	Vector testPosition = desiredPosition;

	for( int slideCount=0; slideCount<3; ++slideCount )
	{
		UTIL_TraceHull( GetAbsOrigin(), testPosition, WorldAlignMins() + Vector( 0, 0, stepHeight ), WorldAlignMaxs(), MASK_PLAYERSOLID, &traceFilter, &result );

		if ( !result.DidHit() )
		{
			break;
		}

		Vector fullMove = testPosition - GetAbsOrigin();
		float blocked = DotProduct( fullMove, result.plane.normal );

		testPosition = GetAbsOrigin() + fullMove - blocked * result.plane.normal;
	}

	Vector resolvedPosition = result.endpos;

	// snap to ground (or fall)
	UTIL_TraceHull( resolvedPosition + Vector( 0, 0, stepHeight ), resolvedPosition + Vector( 0, 0, -stepHeight ), WorldAlignMins(), WorldAlignMaxs(), MASK_PLAYERSOLID, &traceFilter, &result );

	if ( result.DidHit( ) && !IsJumping( ) )
	{
		// limit slope that can be walked up
		const float slopeLimit = 0.7071f;
		if ( !result.plane.normal.IsZero() && result.plane.normal.z > slopeLimit )
		{
			SetAbsOrigin( result.endpos );
		}
		else
		{
			SetAbsOrigin( resolvedPosition );
		}

		if ( !m_isOnGround )
		{
			Land();

			m_isOnGround = true;
			m_bInJump = false;

		}
	}
	else
	{
		SetAbsOrigin( resolvedPosition );

		// fall
		SetBaseVelocity( GetBaseVelocity() + Vector( 0, 0, GetGravity() * deltaT ) );

		m_isOnGround = false;

		Fly();
	}
}

void CChicken::UpdateFollowing( float deltaT )
{
	if ( !IsFollowingSomeone( ) && m_lastLeaderID != 0 )
	{
		m_lastLeaderID = 0;
	}

	// if we have a leader, follow him
	CCSPlayer *leader = GetLeader( );
	if ( leader )
	{
		// if leader is dead, stop following him
		if ( !leader->IsAlive( ) )
		{
			Follow( nullptr );
			Idle( );

			return;
		}

//		m_nHostageState = k_EHostageStates_FollowingPlayer;

		// if leader has moved, repath
		if ( m_path.IsValid( ) )
		{
			Vector pathError = leader->GetAbsOrigin( ) - m_path.GetEndpoint( );

			const float repathRange = 100.0f;
			if ( pathError.IsLengthGreaterThan( repathRange ) )
			{
				m_path.Invalidate( );
			}

//			m_activity = ACT_WALK;
		}

		// build a path to our leader
		if ( !m_path.IsValid( ) && m_repathTimer.IsElapsed( ) )
		{
			const float repathInterval = 0.5f;
			m_repathTimer.Start( repathInterval );

			Vector from = GetAbsOrigin( );
			Vector to = leader->GetAbsOrigin( );
			HostagePathCost pathCost;

			m_path.Compute( from, to, pathCost );
			m_pathFollower.Reset( );
		}

		// if our rescuer is too far away, give up
		const float giveUpRange = 2000.0f;
		const float maxPathLength = 4000.0f;
		Vector toLeader = leader->GetAbsOrigin( ) - GetAbsOrigin( );
		if ( toLeader.IsLengthGreaterThan( giveUpRange ) || ( m_path.IsValid( ) && m_path.GetLength( ) > maxPathLength ) )
		{
			if ( hostage_debug.GetInt( ) < 2 )
			{
				Idle( );
			}
			return;
		}

		// don't crowd the leader
		if ( m_isWaitingForLeader )
		{
			// we are close to our leader and waiting for him to move
			const float waitRange = 200.0f;
			if ( toLeader.IsLengthGreaterThan( waitRange ) )
			{
				// leader has moved away - follow him
				m_isWaitingForLeader = false;
			}
		}

		if ( !m_isWaitingForLeader )
		{
			// move along path towards the leader
			m_pathFollower.Update( deltaT, m_inhibitObstacleAvoidanceTimer.IsElapsed( ) );

			if ( hostage_debug.GetBool( ) )
			{
				m_pathFollower.Debug( true );
			}

			float flDist = GetFeet( ).DistTo( ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) );
			const float minStuckJumpTime = 1.0f;
			if ( ( m_pathFollower.GetStuckDuration( ) > minStuckJumpTime ) && ( flDist > CHICKEN_DISTANCE_RUN * 2 ) )
			{
				Jump( );
			}

			if ( hostage_debug.GetBool( ) )
			{
				m_path.Draw( );
			}
		}
	}
}

//-----------------------------------------------------------------------
void CChicken::ChickenThink( void )
{

	float deltaT = gpGlobals->frametime;

	SetNextThink( gpGlobals->curtime + deltaT );

	// if we've been a zombie for less than CHICKEN_ZOMBIE_SPAWN_DURATION seconds
	float flZombieDuration = (gpGlobals->curtime - m_flWhenZombified);
	if ( IsZombie() && flZombieDuration < CHICKEN_ZOMBIE_SPAWN_DURATION )
	{

		m_activity = ACT_CLIMB_UP;
		SetSequence( LookupSequence( "spawn" ) );
		SetCycle( flZombieDuration / CHICKEN_ZOMBIE_SPAWN_DURATION );
		m_activityTimer.Start( 0 );
		SetSolid( SOLID_NONE );
	}
	else if ( IsZombie() && (m_flWhenZombified + 1.0f) > (gpGlobals->curtime - CHICKEN_ZOMBIE_SPAWN_DURATION) )
	{
		SetSolid( SOLID_BBOX );
		m_activity = ACT_WALK;
	}

	// do leader-following behavior, if necessary
	UpdateFollowing( deltaT );

	Update();

	if ( m_startleTimer.HasStarted() && m_startleTimer.IsElapsed() )
	{
		// we were startled by something and have just noticed it
		m_startleTimer.Invalidate();

		Flee( m_fleeFrom, RandomFloat( 2.0f, 3.0f ) );
	}

	if ( IsActivityFinished() && !IsFollowingSomeone() )
	{

		switch ( ( int ) m_activity )
		{
		case ACT_IDLE:
			if ( RandomInt( 1, 100 ) < 30 )
			{
				Walk( );
			}
			break;

		case ACT_WALK:
			if ( m_activityTimer.IsElapsed( ) )
			{
				Idle( );
			}
			break;

		case ACT_RUN:
			if ( m_activityTimer.IsElapsed( ) )
			{
				Walk( );
			}
			break;

		case ACT_GLIDE:
			Fly( );
			break;

		case ACT_LAND:
			Walk( );
			break;
		}
	}
	else// ongoing activity
	{
		if ( IsFollowingSomeone( ) )
		{
			if ( m_moveRateThrottleTimer.IsElapsed( ) )
			{
				float flDist = GetFeet( ).DistTo( ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) );

				if ( flDist < ( CHICKEN_DISTANCE_WALK * 0.9f ) )
				{
					if ( m_activity != ACT_IDLE )
						Idle( );
				}
				else if ( flDist > ( CHICKEN_DISTANCE_WALK * 1.1f ) && flDist < ( CHICKEN_DISTANCE_RUN * 0.9f ) )
				{
					if ( m_activity != ACT_WALK )
						Walk( );
				}
				if ( flDist > ( CHICKEN_DISTANCE_RUN * 1.1f ) )
				{
					if ( m_activity != ACT_RUN )
						Run( );
				}

				m_moveRateThrottleTimer.Start( RandomFloat( 0.0f, 0.5f ) );
			}

		}

		if ( m_activity == ACT_IDLE || m_activity == ACT_WALK )
		{
			// sound like a calm chicken
			if ( m_vocalizeTimer.IsElapsed() )
			{
				m_vocalizeTimer.Start( RandomFloat( 1.5f, 4.5f ) );

				EmitSound( "Chicken.Idle" );
			}
		}

		// don't walk/run in a straight line - turn a bit
		switch ( ( int ) m_activity )
		{
		case ACT_WALK:
		case ACT_RUN:
			QAngle angle = GetAbsAngles( );

			if ( m_fleeFrom != nullptr )
			{
				Vector fleeVector = GetAbsOrigin( ) - m_fleeFrom->GetAbsOrigin( );
				fleeVector.z = 0.0f;

				if ( IsZombie( ) )
					fleeVector = -fleeVector; //zombie chickens flee TOWARDS players

				float fleeRange = fleeVector.NormalizeInPlace( );

				const float safeRange = 150.0f;
				if ( fleeRange > safeRange )
				{
					m_fleeFrom = nullptr;
				}
				else
				{
					m_activityTimer.Reset( );

					if ( DotProduct( fleeVector, Forward( ) ) < 0.7071f )
					{
						// turn away
						m_turnRate = 360.0f;

						if ( CrossProduct( fleeVector, Forward( ) ).z > 0.0f )
						{
							m_turnRate = -m_turnRate;
						}
					}
					else
					{
						m_turnRate = 0.0f;
					}
				}

				float obstacleTurnRate = AvoidObstacles( );

				float actualTurnRate = ( obstacleTurnRate == 0.0f ) ? m_turnRate : obstacleTurnRate;

				angle.y += actualTurnRate * deltaT;

				SetAbsAngles( angle );

			}
			else if ( IsFollowingSomeone( ) )
			{

				Vector followVector = {0,0,0};

				m_activityTimer.Reset( );

				CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
				trace_t tr;
				UTIL_TraceLine( GetEyes( ), ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ), MASK_PLAYERSOLID, &traceFilter, &tr );

				if ( tr.fraction == 1.0 )	// chicken can see player so target player
				{
					followVector = ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) - GetAbsOrigin( );
				}
				else// chicken cannot see player so target next track node
				{
					followVector = m_vecPathGoal - GetAbsOrigin( );
				}

				float followRange = followVector.NormalizeInPlace( );

				// we use an softer angle threshold at greater distances. This makes the chicken seem more organic.
				float flAngleTheshold = RemapValClamped( followRange, 300, 2000, 0.96, 0.707 );

				if ( DotProduct( followVector, Forward( ) ) < flAngleTheshold )
				{
					m_turnRate = 360.0f;

					if ( CrossProduct( followVector, Forward( ) ).z > 0.0f )
					{
						m_turnRate = -m_turnRate;
					}
				}
				else
				{
					m_turnRate = 0.0f;
				}

				// If the path goal is above us then don't try to avoid obstacles even if one is present.
				// Instead brute force into a wiggle aka jump;
				//

				float actualTurnRate = m_turnRate;
				float obstacleTurnRate = AvoidObstacles( );

				if ( DotProduct( followVector, Up( ) ) >= .9f ) // the path goal is overhead
				{
					Jump( );
				}
				
				if ( obstacleTurnRate != 0.0f ) 
				{
					actualTurnRate = obstacleTurnRate;
				}

				angle.y += actualTurnRate * deltaT;

				SetAbsAngles( angle );;
			}

			break;
		}
	}

	Vector desiredPosition;

	Vector velocityFromAnimation = GetGroundSpeedVelocity();
	desiredPosition = GetAbsOrigin() + velocityFromAnimation * deltaT;

	ResolveCollisions( desiredPosition, deltaT );

	SetPlaybackRate( 1.0f );
	StudioFrameAdvance();
}

const Vector &CChicken::GetCentroid( void ) const
{
	static Vector centroid;

	centroid = GetFeet( );
	centroid.z += HalfHumanHeight;

	return centroid;
}

//-----------------------------------------------------------------------------------------------------
/**
* Return position of "feet" - point below centroid of improv at feet level
*/
const Vector &CChicken::GetFeet( void ) const
{
	static Vector feet;

	feet = GetAbsOrigin( );

	return feet;
}

//-----------------------------------------------------------------------------------------------------
const Vector &CChicken::GetEyes( void ) const
{
	static Vector eyes;

	eyes = EyePosition( );

	return eyes;
}

//-----------------------------------------------------------------------------------------------------
/**
* Return direction of movement
*/
float CChicken::GetMoveAngle( void ) const
{
	return GetAbsAngles( ).y;
}

//-----------------------------------------------------------------------------------------------------
CNavArea *CChicken::GetLastKnownArea( void ) const
{
	return m_lastKnownArea;
}

//-----------------------------------------------------------------------------------------------------
/**
* Find "simple" ground height, treating current nav area as part of the floo
*/
bool CChicken::GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal )
{
	if ( TheNavMesh->GetSimpleGroundHeight( pos, height, normal ) )
	{
		// our current nav area also serves as a ground polygon
		if ( m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ) )
		{
			*height = std::max( ( *height ), m_lastKnownArea->GetZ( pos ) );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
void CChicken::Crouch( void )
{
// 	m_isCrouching = true;
}

//-----------------------------------------------------------------------------------------------------
/**
* un-crouch
*/
void CChicken::StandUp( void )
{
// 	m_isCrouching = false;
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsCrouching( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
* Initiate a jump
*/
void CChicken::Jump( void )
{
	float flJumpPower = 250.0f; /*RandomFloat( 10.0f, 200.0f );*/ /*RemapValClamped( flTimeSinceLastJump, 3.0, 20.0, 200.0f, 30.0f );*/

	Jump( flJumpPower );
}

void CChicken::Jump( float flVelocity )
{
	// don't jump if the nav disallows it
	CNavArea *myArea = GetLastKnownArea( );
	if ( myArea && myArea->HasAttributes( NAV_MESH_NO_JUMP ) )
		return;

	if ( CanJump( ) && IsOnGround( ) )
	{
		const float minJumpInterval = 0.1f;
		m_jumpTimer.Start( minJumpInterval );

		m_bInJump = true;

		// 
		// 		Vector dir;
		// 		AngleVectors( GetAbsAngles( ), &dir, NULL, NULL );
		// 
		// 		vel += dir * 10;

		Vector vel = GetAbsVelocity( );
		vel.z += flVelocity;
		SetAbsVelocity( vel );

		SetSequence( SelectWeightedSequence( ACT_RUN ) );
		ResetSequenceInfo( );

		m_jumpedThisFrame = true;

		m_flLastJumpTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsJumping( void ) const
{
	//return m_bInJump;

	return !m_jumpTimer.IsElapsed( );
}

bool CChicken::CanJump( void ) const
{
	return !IsJumping( ) && ( gpGlobals->curtime - m_flLastJumpTime > 3.0f );
}

//-----------------------------------------------------------------------------------------------------
/**
* Set movement speed to running
*/
void CChicken::Run( void )
{
//	m_isRunning = true;

	m_activity = ACT_RUN;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );
	m_turnRate = RandomFloat( -45.0f, 45.0f );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo( );

}

// -----------------------------------------------------------------------------------------------------
// /**
// * Set movement speed to walking
// */
// void CChicken::Walk( void )
// {
// 	m_isRunning = false;
// }

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsRunning( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when a ladder is encountered while following a path
*/
void CChicken::StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos )
{
}

//-----------------------------------------------------------------------------------------------------
/**
* Traverse given ladder
*/
bool CChicken::TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT )
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsUsingLadder( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
void CChicken::TrackPath( const Vector &pathGoal, float deltaT )
{
	m_vecPathGoal = pathGoal;
	
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when an improv reaches its MoveTo goal
*/
void CChicken::OnMoveToSuccess( const Vector &goal )
{
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when an improv fails to reach a MoveTo goal
*/
void CChicken::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
}

class CBaseEntity;
class CNavArea;
class CTakeDamageInfo;
struct Extent;

using namespace ::ks::reflect;

/**
 * Inferno.cpp
 * An Inferno
 * Author: Michael S. Booth, February 2005
 * Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved
 */

IMPLEMENT_REFLECT_SERVERCLASS( CInferno, DT_Inferno )

LINK_ENTITY_TO_CLASS( inferno, CInferno );
PRECACHE_REGISTER( inferno );

IMPLEMENT_REFLECT_SERVERCLASS( CFireCrackerBlast, DT_FireCrackerBlast )

LINK_ENTITY_TO_CLASS( fire_cracker_blast, CFireCrackerBlast );
PRECACHE_REGISTER( fire_cracker_blast );

ConVar InfernoPerFlameSpawnDuration( side_registry(), "inferno_per_flame_spawn_duration", "3", FCVAR_CHEAT, "Duration each new flame will attempt to spawn new flames" );
ConVar InfernoInitialSpawnInterval( side_registry(), "inferno_initial_spawn_interval", "0.02", FCVAR_CHEAT, "Time between spawning flames for first fire" );
ConVar InfernoChildSpawnIntervalMultiplier( side_registry(), "inferno_child_spawn_interval_multiplier", "0.1", FCVAR_CHEAT, "Amount spawn interval increases for each child" );
ConVar InfernoMaxChildSpawnInterval( side_registry(), "inferno_max_child_spawn_interval", "0.5", FCVAR_CHEAT, "Largest time interval for child flame spawning" );
ConVar InfernoSpawnAngle( side_registry(), "inferno_spawn_angle", "45", FCVAR_CHEAT, "Angular change from parent" );
ConVar InfernoMaxFlames( side_registry(), "inferno_max_flames", "16", FCVAR_CHEAT, "Maximum number of flames that can be created" );
ConVar InfernoFlameSpacing( side_registry(), "inferno_flame_spacing", "42", FCVAR_CHEAT, "Minimum distance between separate flame spawns" );
ConVar InfernoFlameLifetime( side_registry(), "inferno_flame_lifetime", "7", FCVAR_CHEAT, "Average lifetime of each flame in seconds" );
ConVar InfernoFriendlyFireDuration( side_registry(), "inferno_friendly_fire_duration", "6", FCVAR_CHEAT, "For this long, FF is credited back to the thrower." );
ConVar InfernoDebug( side_registry(), "inferno_debug", "0", FCVAR_CHEAT );
ConVar InfernoDamage( side_registry(), "inferno_damage", "40", FCVAR_CHEAT, "Damage per second" );
ConVar InfernoMaxRange( side_registry(), "inferno_max_range", "150", FCVAR_CHEAT, "Maximum distance flames can spread from their initial ignition point" );
ConVar InfernoVelocityFactor( side_registry(), "inferno_velocity_factor", "0.003", FCVAR_CHEAT );
ConVar InfernoVelocityDecayFactor( side_registry(), "inferno_velocity_decay_factor", "0.2", FCVAR_CHEAT );
ConVar InfernoVelocityNormalFactor( side_registry(), "inferno_velocity_normal_factor", "0", FCVAR_CHEAT );
ConVar InfernoSurfaceOffset( side_registry(), "inferno_surface_offset", "20", FCVAR_CHEAT );
ConVar InfernoChildSpawnMaxDepth( side_registry(), "inferno_child_spawn_max_depth", "4", FCVAR_CHEAT );
ConVar inferno_scorch_decals( side_registry(), "inferno_scorch_decals", "1", FCVAR_CHEAT );
ConVar inferno_max_trace_per_tick(side_registry(), "inferno_max_trace_per_tick", "16");
ConVar inferno_forward_reduction_factor( side_registry(), "inferno_forward_reduction_factor", "0.9", FCVAR_CHEAT );

// Inferno trace masks can allow to do different traces for spreading fire
inline constexpr auto INFERNO_MASK_TO_GROUND = ((MASK_SOLID_BRUSHONLY) & (~CONTENTS_GRATE));
inline constexpr auto INFERNO_MASK_LOS_CHECK = ( INFERNO_MASK_TO_GROUND | CONTENTS_MONSTER );

// Smoke grenade radius constant is actually tuned for the bots
// and not for gameplay. Visualizing smoke will show that it goes
// up from the emitter by 128 units (fuzzy top), nothing goes down,
// and it makes a wide XY-donut with a radius of *128* units (fuzzy edges).
static_assert( CONSTANT_UNITS_SMOKEGRENADERADIUS == 166 );
// When interacting with fire we don't want any vertical interactions unless
// contact points are definitely in smoke vertically.
static const float SmokeGrenadeRadius_InfernoAffectingZ = 120.0f;
// When interacting with fire on the same plane we don't want alpha depth-fighting
// in the most common case, so leave a grace margin between the smoke particles
// and the fire particles.
static const float SmokeGrenadeRadius_InfernoAffectingXY_topedge = 100.0f;
static const float SmokeGrenadeRadius_InfernoAffectingXY_equator = 150.0f;
static const float SmokeGrenadeRadius_InfernoAffectingXY_bottomedge = 128.0f;

// Fire burning things and smoke constants
static const float InfernoFire_HalfWidth = 30.0f;
static const float InfernoFire_FullHeight = 80.0f;

static bool BCheckFirePointInSmokeCloud( const Vector &vecFirePoint, const Vector &vecSmokeOrigin )
{
	const float flFireUpToSmokeCheckHeight = ( 2 * InfernoFire_HalfWidth + 4.0f );
	vec_t flFireAboveSmokeZ = ( vecFirePoint.z - vecSmokeOrigin.z );
	if ( flFireAboveSmokeZ < -flFireUpToSmokeCheckHeight )
		return false;	// fire not tall enough to burn up to smoke
	if ( flFireAboveSmokeZ > SmokeGrenadeRadius_InfernoAffectingZ )
		return false;	// smoke cloud not tall enough to reach to the fire
	
	// Now we know that fire is in XY-slice containing the smoke cloud
	// Figure out if we are in the equator XY-plane or in the shrinking edge XY-plane
	float flRadiusSquaredTest = SmokeGrenadeRadius_InfernoAffectingXY_equator*SmokeGrenadeRadius_InfernoAffectingXY_equator;
	if ( flFireAboveSmokeZ > SmokeGrenadeRadius_InfernoAffectingZ * 0.6f )
	{
		float flPctFromEquatorToEdge = RemapValClamped( flFireAboveSmokeZ, SmokeGrenadeRadius_InfernoAffectingZ * 0.6f, SmokeGrenadeRadius_InfernoAffectingZ, 0.0f, 1.0f );
		flPctFromEquatorToEdge *= flPctFromEquatorToEdge; // 0.0 still equator; 1.0 edge (squaring makes things feel quadratically closer to equator)
		flRadiusSquaredTest = RemapValClamped( flPctFromEquatorToEdge, 0.0f, 1.0f, flRadiusSquaredTest, SmokeGrenadeRadius_InfernoAffectingXY_topedge*SmokeGrenadeRadius_InfernoAffectingXY_topedge );
	}
	else if ( flFireAboveSmokeZ < SmokeGrenadeRadius_InfernoAffectingZ * 0.15f )
	{
		float flPctFromEquatorToEdge = RemapValClamped( flFireAboveSmokeZ, SmokeGrenadeRadius_InfernoAffectingZ * 0.1f, -flFireUpToSmokeCheckHeight, 0.0f, 1.0f );
		flPctFromEquatorToEdge *= flPctFromEquatorToEdge; // 0.0 still equator; 1.0 edge (squaring makes things feel quadratically closer to equator)
		flRadiusSquaredTest = RemapValClamped( flPctFromEquatorToEdge, 0.0f, 1.0f, flRadiusSquaredTest, SmokeGrenadeRadius_InfernoAffectingXY_bottomedge*SmokeGrenadeRadius_InfernoAffectingXY_bottomedge );
	}

	// Check if it is within XY-plane radius now
	vec_t lenXYsqr = ( vecFirePoint - vecSmokeOrigin ).Length2DSqr();
	return lenXYsqr <= flRadiusSquaredTest;
}

//------------------------------------------------------------------------------------------
CInferno::CInferno() :
	m_pWeaponInfo( nullptr )
{
	// Set max flames to default in case the user doesn't ask for
	// more or less max flames for this inferno.
	SetMaxFlames( InfernoMaxFlames.GetInt() );
	ListenForGameEvent( "hegrenade_detonate" );
	ListenForGameEvent( "smokegrenade_detonate" );
	m_bWasCreatedInSmoke = false;
}

//------------------------------------------------------------------------------------------
CInferno::~CInferno()
{
	for ( int i = 0; i < m_fireCount; i++ )
	{
		delete m_fire[i];
		m_fire[i] = nullptr;
	}

	switch( GetInfernoType() )
	{
	case INFERNO_TYPE_FIRE:
	case INFERNO_TYPE_INCGREN_FIRE:
		EmitSound( "Inferno.FadeOut" );
		StopSound( "Inferno.Loop" );
		break;
	case INFERNO_TYPE_FIREWORKS:
		EmitSound( "FireworksCrate.Stop" );
		StopSound( "FireworksCrate.Start" );
		break;
	}		
}

//------------------------------------------------------------------------------------------
void CInferno::Precache_Impl( void )
{
	// extend
	BaseClass::Precache_Impl();

	PrecacheScriptSound( "Inferno.Start" );
	PrecacheScriptSound( "Inferno.Start_IncGrenade" );
	PrecacheScriptSound( "Inferno.StartSweeten" );
	PrecacheScriptSound( "Inferno.Loop" );
	PrecacheScriptSound( "Inferno.Fire.Ignite" );
	PrecacheScriptSound( "Inferno.FadeOut" );

	PrecacheParticleSystem( "extinguish_fire" );
	PrecacheParticleSystem( "extinsguish_fire_blastout_01" );
	
	PrecacheScriptSound( "Molotov.Throw" );

	PrecacheScriptSound( "FireworksCrate.Start" );
	PrecacheScriptSound( "FireworksCrate.Stop" );

	if( GetParticleEffectName() != nullptr )
	{
		PrecacheParticleSystem( GetParticleEffectName() );
	}

	if( GetImpactParticleEffectName() != nullptr )
	{
		PrecacheParticleSystem( GetImpactParticleEffectName() );
	}
}

//------------------------------------------------------------------------------------------
void CInferno::Spawn_Impl( void )
{
	m_fireCount = 0;

	const float damageRampUpTime = 2.0f;
	m_damageRampTimer.Start( damageRampUpTime );

	SetThink( &CInferno::InfernoThink );
	SetNextThink( gpGlobals->curtime );

	m_NextSpreadTimer.Start( GetFlameSpreadDelay() );

	AddFlag( FL_ONFIRE );

	SetInfernoType( INFERNO_TYPE_FIRE );
}

//------------------------------------------------------------------------------------------
float CInferno::GetDamagePerSecond()
{
	return InfernoDamage.GetFloat();
}

//------------------------------------------------------------------------------------------
float CInferno::GetFlameLifetime() const
{
	return InfernoFlameLifetime.GetFloat();
}

void CInferno::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

// 	if ( V_strcmp( "hegrenade_detonate", eventname ) == 0 )
// 	{
// 		Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), ( float )event->GetInt( "z" ) );
// 		float flGrenadeRadius = HEGrenadeRadius*1.5;
// 
// 		if ((vecGrenade - m_startPos).IsLengthLessThan( flGrenadeRadius*4 ))
// 		{
// 			ExtinguishFlamesInSphere( vecGrenade, flGrenadeRadius );
// 		}
// 	}

	if ( V_strcmp( "smokegrenade_detonate", eventname ) == 0 )
	{
		Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), ( float )event->GetInt( "z" ) );
		if ((vecGrenade - m_startPos).IsLengthLessThan( SmokeGrenadeRadius*4 ))
		{
			ExtinguishFlamesAroundSmokeGrenade( vecGrenade );
		}
	}
}

//------------------------------------------------------------------------------------------
/**
 * Start the Inferno burning
 */
void CInferno::StartBurning( const Vector &pos, const Vector &normal, const Vector &velocity, int initialDepth )
{
	m_startPos.x = pos.x + InfernoSurfaceOffset.GetFloat() * normal.x;
	m_startPos.y = pos.y + InfernoSurfaceOffset.GetFloat() * normal.y;
	m_startPos.z = pos.z;

	// reflect velocity off of surface
	float splash = DotProduct( velocity, normal );
	Vector remainder = velocity - normal * splash;

	m_splashVelocity = remainder - InfernoVelocityNormalFactor.GetFloat() * normal * splash;

	QAngle splashangle;
	VectorAngles( velocity, splashangle );

	if( GetImpactParticleEffectName() != nullptr )
	{
		DispatchParticleEffect( GetImpactParticleEffectName(), pos, splashangle );
	}

	if( InfernoDebug.GetBool() )
	{
		NDebugOverlay::Sphere( pos, 0.5f * InfernoFire_HalfWidth, 0, 255, 0, true, 10.0f);
		NDebugOverlay::Sphere( m_startPos, 0.5f * InfernoFire_HalfWidth, 255, 255, 0, true, 10.0f);
	}
	
	// create the initial bonfire that begins to spread
	if ( k_ECreateFireResult_OK == CreateFire( m_startPos, normal, nullptr, initialDepth ) )
	{
		switch( GetInfernoType() )
		{
		case INFERNO_TYPE_FIRE:
			EmitSound( "Inferno.Start" );
			EmitSound( "Inferno.StartSweeten" );
			EmitSound( "Inferno.Loop" );
			break;
		case INFERNO_TYPE_INCGREN_FIRE:
			EmitSound( "Inferno.Start_IncGrenade" );
			EmitSound( "Inferno.StartSweeten_IncGrenade" );
			EmitSound( "Inferno.Loop" );
			break;
		case INFERNO_TYPE_FIREWORKS:
			EmitSound( "FireworksCrate.Start" );
			break;
		}

		m_startPos = m_fire[0]->m_pos;
		SetAbsOrigin( m_startPos );

		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_startburn" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}

		m_activeTimer.Start();
	}
	else
	{
		EmitSound( "Molotov.Extinguish" );
		DispatchParticleEffect( "extinguish_fire", m_startPos, splashangle );
		UTIL_Remove( this );
	}
}

class CInfernoLOSTraceFilter : public CTraceFilter
{
	// Find which objects can block molotov spreads
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

		// Players do not block spread
		if ( pEntity->IsPlayer() )
			return false;
		
		// Chickens, hostages, and other 'navigating' objects don't block spread
		if( FClassnameIs( pEntity, "chicken" ) || FClassnameIs( pEntity, "hostage_entity" ) || FClassnameIs( pEntity, "info_hostage_spawn" ) )	// the CImprovLocomotor entities
			return false;

		// Other objects (doors, terrain, etc.) block spread
		return true;
	}
};

//------------------------------------------------------------------------------------------
/**
 * Spread the flames
 */
void CInferno::Spread( const Vector &spreadVelocity )
{
	if( m_NextSpreadTimer.HasStarted() && !m_NextSpreadTimer.IsElapsed() )
	{
		return;
	}

	m_NextSpreadTimer.Start( GetFlameSpreadDelay() );

	for ( int i = 0; i < m_fireCount; i++ )
	{
		// attempt to spawn child-flames
		FireInfo *fire = m_fire[ i ];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be spreading from here

		if ( !fire->m_spawnLifetime.IsElapsed() && fire->m_spawnTimer.IsElapsed() )
		{
			fire->m_spawnTimer.Reset();
			fire->m_spawnCount++;
		}
	}

	int traceCount = inferno_max_trace_per_tick.GetInt();
	int nextFireOffset = m_fireSpawnOffset + 1;
	for ( int i = 0; i < m_fireCount && traceCount > 0; i++ )
	{
		if ( m_fireCount >= std::min( ( int ) MAX_INFERNO_FIRES, m_nMaxFlames ) )
			break;

		int fireIndex = (i + m_fireSpawnOffset) % m_fireCount;
		FireInfo *fire = m_fire[fireIndex];
		nextFireOffset = fireIndex;
		if ( !fire->m_spawnCount )
			continue;
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be spreading from here

		int depth = fire->m_treeDepth + 1;
		if ( depth >= InfernoChildSpawnMaxDepth.GetInt() )
			continue;

		fire->m_spawnCount--;

		trace_t tr;
		const int maxRetry = 4;
		for( int t=0; t<maxRetry; ++t )
		{
			Vector out;

			if (fire->m_parent == nullptr)
			{
				// initial fire spreads outward in a circle
				float angle = random_valve->RandomFloat( -3.14159f, 3.14159f );
				out = Vector( std::cos(angle), std::sin(angle), 0.0f );				
			}
			else
			{
				// child flames tend to spread away from their parent
				Vector to = fire->m_pos - fire->m_parent->m_pos;
				to.NormalizeInPlace();

				QAngle angles;
				VectorAngles( to, angles );

				angles.y += random_valve->RandomFloat( -InfernoSpawnAngle.GetFloat(), InfernoSpawnAngle.GetFloat() );

				AngleVectors( angles, &out );
			}

			// If we're going into a wall, don't keep trying to spread into a wall the entire lifetime - back off to
			// a circular spread at the end.
			float velocityDecay = std::pow( InfernoVelocityDecayFactor.GetFloat(), float(fire->m_treeDepth) );
			Vector timeAdjustedSpreadVelocity = spreadVelocity * fire->m_lifetime.GetRemainingRatio() * velocityDecay;
			out += InfernoVelocityFactor.GetFloat() * timeAdjustedSpreadVelocity;

			// put fire on plane of ground
			Vector side = CrossProduct( fire->m_normal, out );
			out = CrossProduct( side, fire->m_normal );

			float range = random_valve->RandomFloat( 50.0f, 75.0f );

			Vector pos = fire->m_pos + range * out;

			// limit maximum range of spread
			Vector fireDir = pos - m_startPos;
			if ( fireDir.IsLengthGreaterThan( InfernoMaxRange.GetFloat() ) )
			{
				VectorNormalize(fireDir);
				fireDir *= InfernoMaxRange.GetFloat();
				pos = m_startPos + fireDir;
			}

			// dont let flames fall too far
			const float maxDrop = 200.0f;
			Vector endPos = pos;
			endPos.z = fire->m_pos.z - maxDrop;

			// put fire on the ground
			UTIL_TraceLine( pos + Vector( 0, 0, 50.0f ), endPos, INFERNO_MASK_TO_GROUND, nullptr, COLLISION_GROUP_NONE, &tr );
			traceCount--;
			if (!tr.DidHit())
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( pos + Vector( 0, 0, 50.0f ), endPos, 255, 255, 0, true, 1.0f );
					NDebugOverlay::Cross3D( pos, 5, 255, 0, 0, true, 1.0f );
				}
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();
				continue;
			}
			pos.z = tr.endpos.z;
			Vector normal = tr.plane.normal;

			// make sure we dont go through walls
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );
			CInfernoLOSTraceFilter losTraceFilter;
			UTIL_TraceLine( fire->m_pos + fireHeight, pos + fireHeight, INFERNO_MASK_LOS_CHECK, &losTraceFilter, &tr );
			traceCount--;
			if (tr.fraction < 1.0f)
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 0, 0, true, 1.0f );
				}
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();
				continue;
			}

			ECreateFireResult_t eCreateFireResult = CreateFire( pos, normal, fire, depth );
			if ( ( eCreateFireResult == k_ECreateFireResult_OK )
				|| ( eCreateFireResult == k_ECreateFireResult_LimitExceeded ) )
				break;
			else if ( eCreateFireResult != k_ECreateFireResult_AlreadyOnFire )
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();

			if ( InfernoDebug.GetBool() )
			{
				if ( eCreateFireResult == k_ECreateFireResult_InSmoke )
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 255, 0, true, 10.0f );
				else if ( eCreateFireResult == k_ECreateFireResult_AlreadyOnFire )
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 100, 100, true, 2.0f );
				else
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 100, 0, true, 10.0f );
			}
		}
	}
	m_fireSpawnOffset = nextFireOffset + 1;
}

//------------------------------------------------------------------------------------------
/**
 * Checks whether destination fire point is within a detonated smoke grenade cloud
 * This is useful to deny molly detonation in smoke, and to deny fire flames spreading into smoke
 */
bool CInferno::IsFirePosInSmokeCloud( const Vector &pos ) const
{
	const int INFERNO_SEARCH_ENTS = 32;
	CBaseEntity *pEntities[ INFERNO_SEARCH_ENTS ];
	int iNumEntities = UTIL_EntitiesInSphere( pEntities, INFERNO_SEARCH_ENTS, pos, SmokeGrenadeRadius, FL_GRENADE );
	for ( int i = 0; i < iNumEntities; i++ )
	{
		CSmokeGrenadeProjectile *pGrenade = dynamic_cast< CSmokeGrenadeProjectile * >( pEntities[ i ] );
		if ( pGrenade && pGrenade->m_bDidSmokeEffect
			&& BCheckFirePointInSmokeCloud( pos, pGrenade->GetAbsOrigin() ) )
		{
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------------------
/**
 * Create an actual fire entity at the given position
 */
CInferno::ECreateFireResult_t CInferno::CreateFire( const Vector &pos, const Vector &normal, FireInfo *parent, int depth )
{
	if ( m_fireCount >= std::min( ( int ) MAX_INFERNO_FIRES, m_nMaxFlames ) )
	{
		return k_ECreateFireResult_LimitExceeded;
	}

	if ( IsTouching( pos, pos, nullptr ) )
	{
		// we already created a fire here
		return k_ECreateFireResult_AlreadyOnFire;
	}

	// if we throw down a molly in the middle of a smoke grenade, DENY!
	if( IsFirePosInSmokeCloud( pos ) )
	{
		m_bWasCreatedInSmoke = true;
		return k_ECreateFireResult_InSmoke;
	}

	if (InfernoDebug.GetBool())
	{
		if (parent)
		{
			NDebugOverlay::Line( parent->m_pos, pos, 0, 255, 255, true, 10.0f );
		}
	}

	Vector firePos( pos );
	bool overWater = false;

	trace_t tr;
	int contents = enginetrace->GetPointContents( pos, MASK_WATER );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME ) )
	{
		Vector fireHeight( 0, 0, 30.0f );

		int mask = MASK_SOLID_BRUSHONLY | CONTENTS_SLIME | CONTENTS_WATER;
		UTIL_TraceLine( pos + fireHeight, pos, mask, nullptr, COLLISION_GROUP_NONE, &tr );
		if ( tr.allsolid )
		{
			return k_ECreateFireResult_AllSolid;
		}
		else
		{
			firePos = tr.endpos;
			overWater = true;
		}
	}

	FireInfo *fire = new FireInfo;

	fire->m_pos = firePos;
	fire->m_center = firePos + Vector( 0, 0, 0.5f * InfernoFire_FullHeight );
	fire->m_normal = normal;
	fire->m_parent = parent;
	fire->m_treeDepth = depth;
	fire->m_spawnCount = 0;
	fire->m_flWaterHeight = firePos.z - pos.z;
	fire->m_burning = true;

	// all control points on the client die down at the same time, so the server needs to match this
	if ( m_activeTimer.HasStarted() )
	{
		fire->m_lifetime.Start( GetFlameLifetime() - m_activeTimer.GetElapsedTime() );
	}
	else
	{
		fire->m_lifetime.Start( GetFlameLifetime() );
	}

	if (parent)
	{
		fire->m_spawnLifetime.Start( parent->m_spawnLifetime.GetCountdownDuration() );

		float duration = InfernoChildSpawnIntervalMultiplier.GetFloat() * parent->m_spawnTimer.GetCountdownDuration();
		if (duration > InfernoMaxChildSpawnInterval.GetFloat())
			duration = InfernoMaxChildSpawnInterval.GetFloat();

		fire->m_spawnTimer.Start( duration );
	}
	else
	{
		fire->m_spawnLifetime.Start( InfernoPerFlameSpawnDuration.GetFloat() );
		fire->m_spawnTimer.Start( InfernoInitialSpawnInterval.GetFloat() );
	}

	// keep a simple array of all active fires
	m_fire[ m_fireCount ] = fire;

	// propogate across the network

	// Compute this fire's position relative to the Inferno entity.
	Vector vecDelta = fire->m_pos - GetAbsOrigin(); 

	m_fireXDelta.Set( m_fireCount, (int)vecDelta.x );
	m_fireYDelta.Set( m_fireCount, (int)vecDelta.y );
	m_fireZDelta.Set( m_fireCount, (int)vecDelta.z );
	m_bFireIsBurning.Set( m_fireCount, true );
	m_BurnNormal.Set( m_fireCount, normal );

	++m_fireCount;

	RecomputeExtent();
	
	// emit a small flame burst sound
	if( GetInfernoType() == INFERNO_TYPE_FIRE || GetInfernoType() == INFERNO_TYPE_INCGREN_FIRE )
	{
		CSoundParameters params;
		if ( GetParametersForSound( "Inferno.Fire.Ignite", params, nullptr ) )
		{
			EmitSound_t ep( params );
			ep.m_pOrigin = &fire->m_pos;

			CBroadcastRecipientFilter filter;
			EmitSound( filter, SOUND_FROM_WORLD, ep );
		}

		if ( inferno_scorch_decals.GetBool() && !overWater )
		{
			trace_t trace;
			const float dist = 100.0f;
			Vector dir( 0, 0, -1 );
			UTIL_TraceLine( fire->m_pos, fire->m_pos + dir * dist, MASK_OPAQUE, nullptr, COLLISION_GROUP_NONE, &trace );
			UTIL_DecalTrace( &trace, "MolotovScorch" );
		}
	}

	return k_ECreateFireResult_OK;
}

void CInferno::ExtinguishIndividualFlameBySmokeGrenade( int iFire, Vector vecStart )
{
	m_fire[ iFire ]->m_lifetime.Invalidate();

	Vector vecAngleAway = m_fire[ iFire ]->m_pos - vecStart;
	vecAngleAway.NormalizeInPlace();

	QAngle angParticle;
	VectorAngles( vecAngleAway, angParticle );

	DispatchParticleEffect( "extinguish_fire", m_fire[ iFire ]->m_pos, angParticle );
}

int CInferno::ExtinguishFlamesAroundSmokeGrenade( Vector vecStart )
{
	bool bExtinguished = false; 
	bool bCheckDistanceForFlames = true;
	int nNumExtinguished = 0;
	
	// if the radius overlaps the center, extinguish the whole flame
	if ( BCheckFirePointInSmokeCloud( m_startPos, vecStart ) )
		bCheckDistanceForFlames = false;

	for( int i=0; i<m_fireCount; ++i )
	{
		// if this fire just died, propagate over the network
		if ( m_fire[i]->m_burning && (
			!bCheckDistanceForFlames || BCheckFirePointInSmokeCloud( m_fire[i]->m_pos, vecStart )
			) )
		{
			ExtinguishIndividualFlameBySmokeGrenade( i, vecStart );
			bExtinguished = true;
			nNumExtinguished++;
		}
	}

	// if we extinguished third or more of our fire, just put out the rest
	if ( !bCheckDistanceForFlames && nNumExtinguished >= (m_fireCount/3) )
	{
		for( int i=0; i<m_fireCount; ++i )
		{
			// if this fire just died, propagate over the network
			if ( !m_fire[i]->m_lifetime.IsElapsed() )
			{
				ExtinguishIndividualFlameBySmokeGrenade( i, vecStart );
				nNumExtinguished++;
			}
		}
	}

	if ( bExtinguished )
	{
		EmitSound( "Molotov.Extinguish" );

		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_extinguish" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}
	}

	return nNumExtinguished; 
}

bool CInferno::CheckExpired()
{
	CVProfScope _( "CInferno::CheckExpired (check lifetimes)", 0, "Fire", false, BUDGETFLAG_OTHER );

	bool bIsAttachedToMovingObject = GetParent() != nullptr;
	Vector vecInfernoOrigin = GetAbsOrigin();

	// check lifetime of flames
	bool isDone = true;
	for ( int i = 0; i < m_fireCount; ++i )
	{
		// Already dead.
		if ( !m_fire[i]->m_burning )
			continue;

		// if this fire just died, propagate over the network
		if ( m_fire[i]->m_lifetime.IsElapsed() )
		{
			m_fire[i]->m_pos = Vector( 0, 0, 0 );
			m_fire[i]->m_burning = false;
			m_bFireIsBurning.Set( i, false );
			continue;
		}

		// still at least one fire alive
		isDone = false;

		m_fire[i]->m_pos = vecInfernoOrigin;
		m_fire[i]->m_pos.x += m_fireXDelta[i];
		m_fire[i]->m_pos.y += m_fireYDelta[i];
		m_fire[i]->m_pos.z += m_fireZDelta[i];

		if ( bIsAttachedToMovingObject )
		{
			RecomputeExtent();
		}

		if ( InfernoDebug.GetBool() )
		{
			NDebugOverlay::Sphere( m_fire[i]->m_pos, 2.0f * InfernoFire_HalfWidth, 255, 100, 0, true, 0.1f );
		}
	}

	if ( isDone )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_expire" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}

		// if all fires have burned out, we're done
		UTIL_Remove( this );

		// Expired!
		return true;
	}

	// Not expired
	return false;
}

void CInferno::MarkCoveredAreaAsDamaging()
{
	// mark overlapping nav areas as "damaging"
	NavAreaCollector overlap;

	// bloat extents enough to ensure any non-damaging area is actually safe
	// bloat in Z as well to catch nav areas that may be slightly above/below ground
	Extent extent = m_extent;
	float DangerBloat = 32.0f;

	Vector dangerBloat( DangerBloat, DangerBloat, DangerBloat );
	extent.lo -= dangerBloat;
	extent.hi += dangerBloat;

	//NDebugOverlay::Box( vec3_origin, extent.lo, extent.hi, 0, 255, 0, 10, 0.1f );

	TheNavMesh->ForAllAreasOverlappingExtent( overlap, extent );

	for ( CNavArea *area : overlap.m_area )
	{
		if ( IsTouching( area ) )
		{
			area->MarkAsDamaging( 1.0f );
		}
	}
}

//------------------------------------------------------------------------------------------
/**
 * Spread the flames
 */
void CInferno::InfernoThink( void )
{
	CVProfScope _( "CInferno::InfernoThink", 0, "Fire", false, BUDGETFLAG_OTHER );

	bool bExpiryCheckPerformed = false;

	// Run bookkeeping every 0.1s
	if ( m_BookkeepingTimer.Interval(0.1f) )
	{
		bExpiryCheckPerformed = true;
		if ( CheckExpired() )
			return;

		// the fire grows...
		if ( m_fireCount > 0 && m_fireCount < std::min( m_nMaxFlames, ( int )MAX_INFERNO_FIRES ) )
		{
			CVProfScope _( "CInferno::InfernoThink (spread)", 0, "Fire", false, BUDGETFLAG_OTHER );
			Spread( m_splashVelocity );
		}

		// Mark area as damaging for bot avoidance
		MarkCoveredAreaAsDamaging();
	}

	// Deal damage every 0.2s
	const float kDamageTimerSeconds = 0.2f;
	while ( m_damageTimer.RunEvery( kDamageTimerSeconds ) )
	{
		// Note that we run a lot of code in this RunEvery(), but we expect the loop to run 0 or 1 times
		// in almost every case, unless this think function somehow got super delayed by the server

		if(!bExpiryCheckPerformed)
		{
			bExpiryCheckPerformed = true;
			if ( CheckExpired() )
				return;
		}

		CVProfScope _( "CInferno::InfernoThink (damage)", 0, "Fire", false, BUDGETFLAG_OTHER );

		const int maxVictims = 256;
		CBaseEntity *damageList[ maxVictims ];
		CBaseEntity *owner = GetOwnerEntity();
		int damageCount = 0;
		const float flameRadius = 2.0f * InfernoFire_HalfWidth;

		CBaseEntity *list[ maxVictims ];
		int count = UTIL_EntitiesInBox( list, maxVictims, m_extent.lo, m_extent.hi, 0 );

		for( int i=0; i<count; ++i )
		{
			if (list[i] == nullptr || !list[i]->IsAlive() || list[i] == this)
				continue;

			if (IsTouching( list[i], flameRadius, list[i]->IsPlayer() ))
			{
				damageList[damageCount] = list[i];
				damageCount++;
			}
		}

		int damageType = GetDamageType();

		// Note that we expect molotov this value to be an integer (currently it is 40 * 0.2 == 8 damage per tick)
		// If molotov DPS changes, we may need to also adjust how often damage is applied.
		//
		// We could also change this to tick at the exact rate required to deal 1 damage (tick rate = 1 / GetDamagePerSecond())
		// at which point we might want to consider optimizing this loop to run a single time and multiply its damage
		// by the damage dealt, so that (for example) if molotovs deal 80 dps on a 64 tick server, we only find the targets
		// once during the ticks when the molotov deals 2 damage.
		float baseDamage = GetDamagePerSecond() * kDamageTimerSeconds; // dmg / sec * sec / tick = dmg / tick

		if ( !m_damageRampTimer.IsElapsed() )
		{
			baseDamage *= m_damageRampTimer.GetElapsedRatio();
		}

		for ( int i = 0; i < damageCount; i++ )
		{
			// damage the victim
			CBaseEntity *pEnt = damageList[i];
			
			float damage = baseDamage;

			if( CanHarm( pEnt ) )
			{
				CTakeDamageInfo info( this, owner, damage, damageType );

				pEnt->TakeDamage( info );

			}
		}
	}

	// Figure out when our next event is and make sure we get a think tick close to it.
	float nextThink = m_BookkeepingTimer.GetTargetTime();
	nextThink = std::min( nextThink, m_damageTimer.GetTargetTime() );
	SetNextThink( nextThink );
}

//-----------------------------------------------------------------------------------------------
void CInferno::RecomputeExtent( void )
{
	m_extent.lo = Vector( 999999.9f, 999999.9f, 999999.9f );
	m_extent.hi = Vector( -999999.9f, -999999.9f, -999999.9f );

	for( int i=0; i<m_fireCount; ++i )
	{
		FireInfo *fire = m_fire[i];

		if ( fire->m_pos.x - InfernoFire_HalfWidth < m_extent.lo.x )
			m_extent.lo.x = fire->m_pos.x - InfernoFire_HalfWidth;

		if ( fire->m_pos.x + InfernoFire_HalfWidth > m_extent.hi.x )
			m_extent.hi.x = fire->m_pos.x + InfernoFire_HalfWidth;

		if ( fire->m_pos.y - InfernoFire_HalfWidth < m_extent.lo.y )
			m_extent.lo.y = fire->m_pos.y - InfernoFire_HalfWidth;

		if ( fire->m_pos.y + InfernoFire_HalfWidth > m_extent.hi.y )
			m_extent.hi.y = fire->m_pos.y + InfernoFire_HalfWidth;

		if ( fire->m_pos.z < m_extent.lo.z )
			m_extent.lo.z = fire->m_pos.z;

		if ( fire->m_pos.z + InfernoFire_FullHeight > m_extent.hi.z )
			m_extent.hi.z = fire->m_pos.z + InfernoFire_FullHeight;
	}
}

bool CInferno::BShouldExtinguishSmokeGrenadeBounce( CBaseEntity *entity, Vector &posDropSmoke ) const
{
	const float radius = 2.0f * InfernoFire_HalfWidth;
	for ( int i = 0; i < m_fireCount; i++ )
	{
		FireInfo *fire = m_fire[ i ];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't cause damage

		if ( ( posDropSmoke - fire->m_center ).IsLengthLessThan( radius ) )
		{
			// doublecheck los if required
			trace_t tr;
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );
			UTIL_TraceLine( fire->m_center + fireHeight, posDropSmoke, INFERNO_MASK_LOS_CHECK, entity, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction < 1.0f )
				UTIL_TraceLine( fire->m_center, posDropSmoke, INFERNO_MASK_LOS_CHECK, entity, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction == 1.0f )
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_center, posDropSmoke, 255, 0, 255, true, 50.2f );
				}

				return true;
			}
			else
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_center, posDropSmoke, 255, 0, 0, true, 50.2f );
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------
/**
 * Return true if position is in contact with a fire within the Inferno
 */
bool CInferno::IsTouching( CBaseEntity *entity, float radius, bool checkLOS ) const
{
	if ( entity != nullptr )
	{
		for ( int i = 0; i < m_fireCount; i++ )
		{
			FireInfo *fire = m_fire[i];
			if ( !fire->m_burning ||
				fire->m_lifetime.IsElapsed() )
				continue;	// This flame has been extinguished or elapsed, shouldn't cause damage

			// Calculate the nearest point to our potential victim, from our center point
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );

			Vector pos;
			Vector fireCheck = fire->m_center;
			if( checkLOS )
				fireCheck += fireHeight;
			entity->CollisionProp()->CalcNearestPoint( fireCheck, &pos );

			if ( ( pos - fireCheck ).IsLengthLessThan( radius ) )
			{
				// touching at least one flame
				if( checkLOS )
				{
					// doublecheck los if required
					trace_t tr;
					UTIL_TraceLine( fireCheck, pos, INFERNO_MASK_LOS_CHECK, entity, COLLISION_GROUP_NONE, &tr );
					if ( tr.fraction < 1.0f )
					{
						fireCheck = fire->m_center;
						entity->CollisionProp()->CalcNearestPoint( fireCheck, &pos );
						if ( ( pos - fireCheck ).IsLengthLessThan( radius ) )
							UTIL_TraceLine( fireCheck, pos, INFERNO_MASK_LOS_CHECK, entity, COLLISION_GROUP_NONE, &tr );
					}
					if( tr.fraction == 1.0f )
					{
						if( InfernoDebug.GetBool() )
						{
							NDebugOverlay::Line( fire->m_center, pos, 255, 0, 255, true, 50.2f );
						}

						return true;
					}
					else
					{
						if( InfernoDebug.GetBool() )
						{
							NDebugOverlay::Line( fire->m_center, pos, 255, 0, 0, true, 50.2f );
						}
					}
				}
				else
				{
					// los not needed, it's touching
					if( InfernoDebug.GetBool() )
					{
						NDebugOverlay::Line( fire->m_center, pos, 255, 0, 255, true, 0.2f );
					}

					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------
/**
 * Return true if given ray intersects any fires
 * TODO: Check LOS if needed.
 */
bool CInferno::IsTouching( const Vector &from, const Vector &to, Vector *where ) const
{
	for ( int i = 0; i < m_fireCount; i++ )
	{
		FireInfo *fire = m_fire[i];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be considered touching

		Vector pointOnRay;
		ClosestPointOnRay( m_fire[i]->m_center, from, to, &pointOnRay );

		const float radius = 2.0f * InfernoFire_HalfWidth;
		if ( ( pointOnRay - m_fire[i]->m_center ).IsLengthLessThan( radius ) )
		{
			if ( where )
			{
				*where = pointOnRay;
			}
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------
/**
 * Return true if given area overlaps any fires
 */
bool CInferno::IsTouching( const CNavArea *area ) const	
{
	if ( area != nullptr )
	{
		float radius = 2.0f * InfernoFire_HalfWidth;

		for ( int i = 0; i < m_fireCount; i++ )
		{
			FireInfo *fire = m_fire[ i ];
			if ( !fire->m_burning ||
				fire->m_lifetime.IsElapsed() )
				continue;	// This flame has been extinguished or elapsed, shouldn't be considered touching

			Vector close;
			area->GetClosestPointOnArea( m_fire[i]->m_center, &close );

			close.z += m_fire[i]->m_flWaterHeight;	// If the inferno was raised above the water, check the nav as if it was in the original pos

			if ( ( close - m_fire[i]->m_center ).IsLengthLessThan( radius ) )
			{
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------------------
void CFireCrackerBlast::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
	SetInfernoType( INFERNO_TYPE_FIREWORKS );
}
