// SPDX-License-Identifier: None
// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

module;
#include "edict.h"
#include "igameevents.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "coordsize.h"
module ks.server:cstrike15.Effects.chicken;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :vscript;
import ks.game.ai_activity;
import :cstrike15.bot.bot;
import :cstrike15.cs_player;
import :baseentity;
import :fish;
import :nav_4;
import ks.game.shareddefs;
import :sharedvar;

class CBaseEntity;
class CCSPlayer;
class CImprovLocomotor;
class CNavArea;
class CNavLadder;
class CTakeDamageInfo;

using namespace ::ks::reflect;


// chicken.h
// An interactive, shootable chicken


class CChicken : public CDynamicProp, public CGameEventListener, public CImprovLocomotor
{
	[[= ks::reflect::NetTable{ .name = "DT_CChicken" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CChicken, CDynamicProp );
	DECLARE_SERVERCLASS();

	CChicken();
	virtual ~CChicken();

	void Precache_Impl( void );
	void Spawn_Impl( void );
	virtual void ChickenTouch( CBaseEntity *pOther );

	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );

	void FireGameEvent( IGameEvent *event );

	virtual bool IsAlive( void ) { return true; }

	void ChickenThink( void );

	bool IsZombie( void ) { return m_flWhenZombified > 0; }
	void Zombify( void ) { m_flWhenZombified = gpGlobals->curtime; }

	bool IsFollowingSomeone( void );
	bool IsFollowing( const CBaseEntity *entity );
	bool IsOnGround( void ) const;

	void Follow( CCSPlayer *leader );						// begin following "leader"
	CCSPlayer *GetLeader( void ) const;						// return our leader, or NULL

	void FaceTowards( const Vector &target, float deltaT );	// rotate body to face towards "target"

	int	ObjectCaps_Impl( void )	{ return ( BaseClass::ObjectCaps_Impl( ) | FCAP_IMPULSE_USE ); }

public:
	// begin CImprovLocomotor -----------------------------------------------------------------------------------------------------------------
	const Vector &GetCentroid( void ) const;
	const Vector &GetFeet( void ) const;			// return position of "feet" - point below centroid of improv at feet level
	const Vector &GetEyes( void ) const;
	float GetMoveAngle( void ) const;				// return direction of movement

	CNavArea *GetLastKnownArea( void ) const;
	bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = nullptr );	// find "simple" ground height, treating current nav area as part of the floor

	void Crouch( void );
	void StandUp( void );							// "un-crouch"
	bool IsCrouching( void ) const;

	void Jump( void );								// initiate a jump
	bool IsJumping( void ) const;

	bool		CanJump( void ) const;
	void		Jump( float flVelocity );

	void Run( void );								// set movement speed to running
 	void Walk( void );								// set movement speed to walking
	bool IsRunning( void ) const;

	void StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos );	// invoked when a ladder is encountered while following a path
	bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT );	// traverse given ladder
	bool IsUsingLadder( void ) const;

	void TrackPath( const Vector &pathGoal, float deltaT );		// move along path by following "pathGoal"
	void OnMoveToSuccess( const Vector &goal );					// invoked when an improv reaches its MoveTo goal
	void OnMoveToFailure( const Vector &goal, MoveToFailureType reason );	// invoked when an improv fails to reach a MoveTo goal
	// end CImprovLocomotor -------------------------------------------------------------------------------------------------------------------

protected:
	virtual void ChickenUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SetChickenStartFollowingPlayer( CCSPlayer *pPlayer );

private:
	void Idle();
//	void Walk();
	void Flee( CBaseEntity *fleeFrom, float duration );
	void Fly();
	void Land();

	void Update( void );
	CountdownTimer m_updateTimer;

	float AvoidObstacles( void );
	Vector m_stuckAnchor;
	CountdownTimer m_stuckTimer;

	void ResolveCollisions( const Vector &desiredPosition, float deltaT );
	bool m_isOnGround;

	Activity m_activity;
	CountdownTimer m_activityTimer;
	float m_turnRate;
	CHandle< CBaseEntity > m_fleeFrom;
	CountdownTimer m_moveRateThrottleTimer;

	CountdownTimer m_startleTimer;

	CountdownTimer m_vocalizeTimer;

	float m_flWhenZombified;

	CNetworkVar( bool, m_jumpedThisFrame, [[= ks::reflect::Net{} ]] );
	CNetworkVar( EHANDLE, m_leader, [[= ks::reflect::Net{} ]] );						// the player we are following

	void UpdateFollowing( float deltaT );					// do following behavior
	int m_lastLeaderID;

	CountdownTimer m_reuseTimer;							// to throttle how often hostage can be used
	bool m_hasBeenUsed;

	CountdownTimer m_jumpTimer;								// if zero, we can jump
	float m_flLastJumpTime;
	bool m_bInJump;

	bool m_isWaitingForLeader;								// true if we are waiting for our rescuer to move

	CCSNavPath m_path;										// current path to follow
	CountdownTimer m_repathTimer;							// throttle pathfinder

	CountdownTimer m_inhibitDoorTimer;

	CNavPathFollower m_pathFollower;						// path tracking mechanism
	CountdownTimer m_inhibitObstacleAvoidanceTimer;			// when active, turn off path following feelers

	CNavArea *m_lastKnownArea;								// last area we were in
	Vector m_vecPathGoal;

	float m_flActiveFollowStartTime;						// when the current follow started
};

class CBaseEntity;
class CCSWeaponInfo;
class CNavArea;
struct Extent;

using namespace ::ks::reflect;


// Inferno.h
// An Inferno
// Author: Michael S. Booth, February 2005

// Forward declaration

/**
 * The server-side Inferno entity.
 * This entity manages the growth of the flames and damage-dealing.
 * Keep this in sync with C_Inferno on the client, which manages the fire rendering
 */

enum
{
	INFERNO_TYPE_FIRE = 0,
	INFERNO_TYPE_INCGREN_FIRE,  // incendiary grenade fire, used to play a different sound
	INFERNO_TYPE_FIREWORKS,
};

class CInferno : public CBaseEntity, public CGameEventListener
{
	[[= ks::reflect::NetTable{ .name = "DT_Inferno" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CInferno, CBaseEntity );
	DECLARE_SERVERCLASS();

	CInferno();
	virtual ~CInferno();

	void Precache_Impl( void );

	void Spawn_Impl( void );

	void SetMaxFlames( int nMaxFlames )		{ m_nMaxFlames = nMaxFlames; }

	int UpdateTransmitState_Impl( void )
	{
		return FL_EDICT_ALWAYS;
	}

	void StartBurning( const Vector &pos, const Vector &normal, const Vector &velocity, int initialDepth = 0 );	// start the Inferno burning
	void InfernoThink( void );

	bool BShouldExtinguishSmokeGrenadeBounce( CBaseEntity *entity, Vector &posDropSmoke ) const;
	bool IsTouching( const Vector &from, const Vector &to, Vector *where = nullptr ) const;	// return true if given ray intersects any fires, point of intersection in "where" if non-NULL
	bool IsTouching( const CNavArea *area ) const;	// return true if given area overlaps any fires

	bool WasCreatedInSmoke( void ) const { return m_bWasCreatedInSmoke; }

	virtual int		GetDamageType()	{ return DMG_BURN; }
	virtual float	GetDamagePerSecond();

	// I've moved some functionality into these member functions in order to 
	// make the insect swarm easier to implement (sjb)
	virtual const char *GetParticleEffectName();
	virtual const char *GetImpactParticleEffectName();
	virtual float		GetFlameLifetime() const;
	virtual bool		CanHarm( CBaseEntity *pEnt ) const { return true; }
	virtual float		GetFlameSpreadDelay() { return 0.0f; }

	int GetInfernoType() const		{ return m_nInfernoType; }
	void SetInfernoType( int type )	{ m_nInfernoType = type; }

	void SetSourceWeaponInfo( const CCSWeaponInfo* pWeaponInfo ) { m_pWeaponInfo = pWeaponInfo; }
	const CCSWeaponInfo* GetSourceWeaponInfo() const { return m_pWeaponInfo; }

protected:
public:
	void FireGameEvent( IGameEvent *event );
protected:
	int ExtinguishFlamesAroundSmokeGrenade( Vector vecStart ); // returns count of fire patches extinguished
	void ExtinguishIndividualFlameBySmokeGrenade( int iFire, Vector vecStart );

	// Checks if all flames have expired and we should delete the inferno
	bool CheckExpired();

	// Marks covered areas as damaging so bots will avoid them
	void MarkCoveredAreaAsDamaging();

private:
	enum { MAX_INFERNO_FIRES = 64 };
	enum ECreateFireResult_t
	{
		k_ECreateFireResult_OK,
		k_ECreateFireResult_LimitExceeded,
		k_ECreateFireResult_AlreadyOnFire,
		k_ECreateFireResult_InSmoke,
		k_ECreateFireResult_AllSolid,
	};
	CNetworkArray( int, m_fireXDelta, MAX_INFERNO_FIRES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );
	CNetworkArray( int, m_fireYDelta, MAX_INFERNO_FIRES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );
	CNetworkArray( int, m_fireZDelta, MAX_INFERNO_FIRES, [[= ks::reflect::Net{ .bits = COORD_INTEGER_BITS+1 } ]] );
	CNetworkArray( bool, m_bFireIsBurning, MAX_INFERNO_FIRES, [[= ks::reflect::Net{} ]] );
	CNetworkArray( Vector, m_BurnNormal, MAX_INFERNO_FIRES );
	CNetworkVar( int, m_fireCount, [[= ks::reflect::Net{ .bits = 7, .flags = SPROP_UNSIGNED } ]] );				// total number of flames spawned
	CNetworkVar( int, m_nInfernoType );

	bool m_bWasCreatedInSmoke;

	struct FireInfo
	{
		Vector m_pos;								// location of this fire
		Vector m_center;							// center of fire
		Vector m_normal;							// surface normal at this fire
		bool m_burning;
		int m_treeDepth;
		int m_spawnCount;
		FireInfo *m_parent;							// the fire that spawned us
		CountdownTimer m_spawnLifetime;				// how long we attempt to spawn new fires
		CountdownTimer m_spawnTimer;				// when we try to spawn a new fire
		CountdownTimer m_lifetime;					// lifetime of this fire
		float m_flWaterHeight;						// how much we were raised above water
	};

	bool IsFirePosInSmokeCloud( const Vector &pos ) const;
	void Spread( const Vector &spreadVelocity );
	ECreateFireResult_t CreateFire( const Vector &pos, const Vector &normal, FireInfo *parent, int depth );		// create an actual fire entity at the given position

	FireInfo *m_fire[ MAX_INFERNO_FIRES ];			// set of all active fires

	Extent m_extent;
	void RecomputeExtent( void );

	CountdownTimer m_damageTimer;
	CountdownTimer m_damageRampTimer;				// damage starts at zero and ramps up
	bool IsTouching( CBaseEntity *entity, float radius, bool checkLOS ) const;	// return true if position is in contact with a fire within the Inferno
	
	Vector m_splashVelocity;						// the velocity of the flame-causing incendiary that hit
	Vector m_startPos;								// the ignition point of the entire inferno

	IntervalTimer m_activeTimer;					// How long this inferno has been active.
	int m_fireSpawnOffset;

	int m_nMaxFlames;

	CountdownTimer m_BookkeepingTimer; // Only check spread / force expiry checks every 0.1s;
	CountdownTimer m_NextSpreadTimer;

	// the weapon info for the grenade type (if any) that spawned this inferno
	const CCSWeaponInfo* m_pWeaponInfo;
};

class CFireCrackerBlast : public CInferno
{
	[[= ks::reflect::NetTable{ .name = "DT_FireCrackerBlast" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFireCrackerBlast, CInferno );
	DECLARE_SERVERCLASS();

	void	Spawn_Impl();

	virtual const char *GetParticleEffectName();
	virtual const char *GetImpactParticleEffectName();
};
