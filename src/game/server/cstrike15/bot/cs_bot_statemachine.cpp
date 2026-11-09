// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cs_bot.h"
#include "cs_nav_path.h"

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
