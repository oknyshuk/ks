// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "edict.h"
#include "networkvar.h"
#include "eiface.h"
#include "studio.h"
#include "shareddefs.h"
module ks.server;

import ks.core.random;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.fmtstr;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import std;
import ks.core.types;
import ks.math.ssemath;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import :entity_2;
import ks.core.basetypes;

class CBaseEntity;
class CBasePlayer;
class CBotManager;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//-----------------------------------------------------------------------------------------------------------
float CCSBot::GetMoveSpeed( void )
{
	return 250.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Lightweight maintenance, invoked frequently
 */
void CCSBot::Upkeep( void )
{
	CVProfScope _( "CCSBot::Upkeep", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	if (TheNavMesh->IsGenerating() || !IsAlive())
		return;
	
	// If bot_flipout is on, then generate some random commands.
	if ( cv_bot_flipout.GetBool() )
	{
		int val = RandomInt( 0, 2 );
		if ( val == 0 )
			MoveForward();
		else if ( val == 1 )
			MoveBackward();
		
		val = RandomInt( 0, 2 );
		if ( val == 0 )
			StrafeLeft();
		else if ( val == 1 )
			StrafeRight();

		if ( RandomInt( 0, 5 ) == 0 )
			Jump( true );
		
		val = RandomInt( 0, 2 );
		if ( val == 0 )
			Crouch();
		else ( val == 1 );
			StandUp();
	
		return;
	}
	
	// BOTPORT: Remove this nasty hack
	m_eyePosition = EyePosition();

	Vector myOrigin = GetCentroid( this );

	if ( m_bIsSleeping )
		return;

	// aiming must be smooth - update often
	if ( IsAimingAtEnemy() )
	{
		if (gpGlobals->curtime >= m_aimFocusNextUpdate)
		{
			PickNewAimSpot();
		}

		UpdateAimPrediction();

		SetLookAngles( m_aimGoal[YAW], m_aimGoal[PITCH] );
	}
	else
	{
		if (m_lookAtSpotClearIfClose)
		{
			// don't look at spots just in front of our face - it causes erratic view rotation
			const float tooCloseRange = 100.0f;
			if ((m_lookAtSpot - myOrigin).IsLengthLessThan( tooCloseRange ))
				m_lookAtSpotState = NOT_LOOKING_AT_SPOT;
		}

		switch( m_lookAtSpotState )
		{
			case NOT_LOOKING_AT_SPOT:
			{
				// look ahead
				SetLookAngles( m_lookAheadAngle, 0.0f );
				break;
			}

			case LOOK_TOWARDS_SPOT:
			{
				UpdateLookAt();
				if (IsLookingAtPosition( m_lookAtSpot, m_lookAtSpotAngleTolerance ))
				{
					m_lookAtSpotState = LOOK_AT_SPOT;
					m_lookAtSpotTimestamp = gpGlobals->curtime;
				}
				break;
			}

			case LOOK_AT_SPOT:
			{
				UpdateLookAt();

				if (m_lookAtSpotDuration >= 0.0f && gpGlobals->curtime - m_lookAtSpotTimestamp > m_lookAtSpotDuration)
				{
					m_lookAtSpotState = NOT_LOOKING_AT_SPOT;
					m_lookAtSpotDuration = 0.0f;
				}
				break;
			}
		}

		// have view "drift" very slowly, so view looks "alive"
		if (!IsUsingSniperRifle())
		{
			float driftAmplitude = 2.0f;
			if (IsBlind())
			{
				driftAmplitude = 5.0f;
			}

			m_lookYaw += driftAmplitude * BotCOS( 33.0f * gpGlobals->curtime );
			m_lookPitch += driftAmplitude * BotSIN( 13.0f * gpGlobals->curtime );
		}
	}

	// view angles can change quickly
	UpdateLookAngles();
}

void CCSBot::CoopUpdateChecks()
{
		// check to see if the bot hasn't seen anyone in a long time and if so, kill them and recycle their spot
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopGuardian() && 
		 IsAlive() && 
		 !CSGameRules()->IsFreezePeriod() &&
		 !CSGameRules()->IsWarmupPeriod() && 
		 !CSGameRules()->IsRoundOver() )
	{
		float flStart = CSGameRules()->GetRoundStartTime();
		if ( m_spawnedTime > flStart )
			flStart = m_spawnedTime;

		// this waits 70 seconds from the last time they spawned or the round start
		// and sees if they've seen an enemy in the last 15 seconds
		// if not, they get removed
		// TODO: put the 70 and 15 on convars
		if ( m_nextCleanupCheckTimestamp < gpGlobals->curtime &&  flStart > 0 
			 && IsEnemyVisible() == false && GetTimeSinceLastSawEnemy() > 20 )
		{
			if ( CSGameRules()->IsHostageRescueMap() && (gpGlobals->curtime - flStart) > 50 
				 && GetTask() != CCSBot::COLLECT_HOSTAGES )
			{
				CBaseEntity *hostage = static_cast<CBaseEntity*>( GetGameState()->GetNearestFreeHostage() );
				if ( hostage )
				{
					// here we want to force them to run for the hostages, no questions asked
					// go get hostages
					SetTask( CCSBot::COLLECT_HOSTAGES );
					ForceRun( 3.0 );
					SetGoalEntity( hostage );
					ResetWaitForHostagePatience();
					SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
					MoveTo( GetCentroid( hostage ), FASTEST_ROUTE );
					PrintIfWatched( "I'm collecting hostages\n" );
				}
			}
			else if ( (gpGlobals->curtime - flStart) > 140 )
			{
				bool bSeen = false;

				int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
				// last check is to make sure a player can't see them
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == nTeam )
					{
						if ( IsVisible( pPlayer->GetAbsOrigin() + Vector( 0, 0, 64 ), false, pPlayer ) )
						{
							bSeen = true;
							break;
						}
					}
				}

				const Vector *vecGoal = GetGameState()->GetRandomFreeHostagePosition();
				if ( CSGameRules()->IsHostageRescueMap() == false )
				{
					int zoneIndex = GetGameState()->GetNextBombsiteToSearch();
					// move to bombsite - if we reach it, we'll update its cleared status, causing us to select another
					vecGoal = TheCSBots()->GetRandomPositionInZone( TheCSBots()->GetZone( zoneIndex ) );
				}

				if ( vecGoal && IsVisible( *vecGoal, false, nullptr ) )
				{
					bSeen = true;
				}

				if ( bSeen == false )
					CommitSuicide();
			}

			m_nextCleanupCheckTimestamp = gpGlobals->curtime + 0.5f;
		}

		// Make sure bots know the bomb state in guardian. Don't require them to actually see it.
		if ( CBaseEntity* pBomb = TheCSBots()->GetLooseBomb() )
		{
			GetGameState()->UpdateLooseBomb( pBomb->GetAbsOrigin() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Heavyweight processing, invoked less often
 */
void CCSBot::Update( void )
{
	CVProfScope _( "CCSBot::Update", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	// If bot_flipout is on, then we only do stuff in Upkeep().
	if ( cv_bot_flipout.GetBool() )
		return;

	Vector myOrigin = GetCentroid( this );

	// if we're sleeping and essentially dormant, check to see if there is a CT near us to wake us up
	if ( m_bIsSleeping && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
			{
				Vector vecPlayer = pPlayer->GetAbsOrigin() + Vector( 0, 0, 64 );
				if ( !IsBeyondBotMaxVisionDistance( vecPlayer ) && IsVisible( vecPlayer, false, pPlayer ) )
				{
					// get us going ASAP
					m_bIsSleeping = false;
					m_lookAtSpotState = NOT_LOOKING_AT_SPOT;

					// if we are a heavy, shout a taunt!
					if ( HasHeavyArmor() )
					{
						GetChatter()->DoPhoenixHeavyWakeTaunt();
					}

					break;
				}
			}
		}

		if ( m_bIsSleeping )
			return;
	}

	// if we are spectating, get into the game
	if (GetTeamNumber() == 0)
	{
		HandleCommand_JoinTeam( m_desiredTeam );
		HandleCommand_JoinClass();
		return;
	}

	// update our radio chatter
	// need to allow bots to finish their chatter even if they are dead
	GetChatter()->Update();
	
	// check if we are dead
	if (!IsAlive())
	{
		// remember that we died
		m_diedLastRound = true;

		BotDeathThink();
		return;
	}

	// the bot is alive and in the game at this point
	m_hasJoined = true;

	//
	// Debug beam rendering
	//

	if (cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
	{
		DebugDisplay();
	}

	if (cv_bot_stop.GetBool())
		return;

	// check if we are stuck
	StuckCheck();

	// Check for physics props and other breakables in our way that we can break
	BreakablesCheck();

	// Check for useable doors in our way that we need to open
	DoorCheck();

	CoopUpdateChecks();

	// update travel distance to all players (this is an optimization)
	// EDIT: actually this is really slow and only used to detect audible events; we're using straight line dist instead
	//UpdateTravelDistanceToAllPlayers();

	// if our current 'noise' was heard a long time ago, forget it
	const float rememberNoiseDuration = 20.0f;
	if (m_noiseTimestamp > 0.0f && gpGlobals->curtime - m_noiseTimestamp > rememberNoiseDuration)
	{
		ForgetNoise();
	}

	// where are we
	if (!m_currentArea || !m_currentArea->Contains( myOrigin ))
	{
		m_currentArea = (CNavArea *)TheNavMesh->GetNavArea( myOrigin );
	}

	// track the last known area we were in
	if (m_currentArea && m_currentArea != m_lastKnownArea)
	{
		m_lastKnownArea = m_currentArea;

		OnEnteredNavArea( m_currentArea );
	}

	// keep track of how long we have been motionless
	const float stillSpeed = 10.0f;
	if (GetAbsVelocity().IsLengthLessThan( stillSpeed ))
	{
		m_stillTimer.Start();
	}
	else
	{
		m_stillTimer.Invalidate();
	}

	// if we're blind, retreat!
	if (IsBlind())
	{
		if (m_blindFire)
		{
			PrimaryAttack();
		}
	}

	UpdatePanicLookAround();

	//
	// Enemy acquisition and attack initiation
	//

	// take a snapshot and update our reaction time queue
	UpdateReactionQueue();

	// "threat" may be the same as our current enemy
	CCSPlayer *threat = GetRecognizedEnemy();
	if (threat)
	{

		Vector threatOrigin = GetCentroid( threat );

		// adjust our personal "safe" time
		AdjustSafeTime();

		BecomeAlert();

		const float selfDefenseRange = 500.0f; // 750.0f;
		const float farAwayRange = 2000.0f;

		//
		// Decide if we should attack
		//
		bool doAttack = false;
		switch( GetDisposition() )
		{
			case IGNORE_ENEMIES:
			{
				// never attack
				doAttack = false;
				break;
			}

			case SELF_DEFENSE:
			{
				// attack if fired on
				doAttack = (IsPlayerLookingAtMe( threat, 0.99f ) && DidPlayerJustFireWeapon( threat ));

				// attack if enemy very close
				if (!doAttack)
				{
					doAttack = (myOrigin - threatOrigin).IsLengthLessThan( selfDefenseRange );
				}

				if ( CSGameRules()->IsPlayingCoopMission() && IsSniper() )
				{
					// snipers love far away targets
					doAttack = true;
				}

				break;
			}

			case ENGAGE_AND_INVESTIGATE:
			case OPPORTUNITY_FIRE:
			{
				if ((myOrigin - threatOrigin).IsLengthGreaterThan( farAwayRange ))
				{
					// enemy is very far away - wait to take our shot until he is closer
					// unless we are a sniper or he is shooting at us
					if (IsSniper())
					{
						// snipers love far away targets
						doAttack = true;
					}
					else
					{
						// attack if fired on
						doAttack = (IsPlayerLookingAtMe( threat, 0.99f ) && DidPlayerJustFireWeapon( threat ));					
					}
				}
				else
				{
					// normal combat range
					doAttack = true;
				}

				break;
			}
		}

		// if we aren't attacking but we are being attacked, retaliate
		if (!doAttack && !IsAttacking() && GetDisposition() != IGNORE_ENEMIES)
		{
			const float recentAttackDuration = 1.0f;
			if (GetTimeSinceAttacked() < recentAttackDuration)
			{
				// we may not be attacking our attacker, but at least we're not just taking it
				// (since m_attacker isn't reaction-time delayed, we can't directly use it)
				doAttack = true;
				PrintIfWatched( "Ouch! Retaliating!\n" );
			}
		}

		if (doAttack)
		{
			if (!IsAttacking() || threat != GetBotEnemy())
			{
				if (IsUsingKnife() && IsHiding())
				{
					// if hiding with a knife, wait until threat is close
					const float knifeAttackRange = 250.0f;
					if ((GetAbsOrigin() - threat->GetAbsOrigin()).IsLengthLessThan( knifeAttackRange ))
					{
						Attack( threat );
					}
				}
				else
				{
					Attack( threat );
				}
			}
		}
		else
		{
			// dont attack, but keep track of nearby enemies
			SetBotEnemy( threat );
			m_isEnemyVisible = true;
		}

		TheCSBots()->SetLastSeenEnemyTimestamp();
	}

	//
	// Validate existing enemy, if any
	//
	if (m_enemy != nullptr)
	{

		if (IsAwareOfEnemyDeath())
		{
			// we have noticed that our enemy has died
			m_enemy = nullptr;
			m_isEnemyVisible = false;
		}
		else
		{
			// check LOS to current enemy (chest & head), in case he's dead (GetNearestEnemy() only returns live players)
			// note we're not checking FOV - once we've acquired an enemy (which does check FOV), assume we know roughly where he is
			if (IsVisible( m_enemy, false, &m_visibleEnemyParts ))
			{
				m_isEnemyVisible = true;
				m_lastSawEnemyTimestamp = gpGlobals->curtime;
				m_lastEnemyPosition = GetCentroid( m_enemy );
			}
			else
			{
				m_isEnemyVisible = false;

				if ( (gpGlobals->curtime - m_lastSawEnemyTimestamp ) > 1.0f && EquipGrenade() && CSGameRules()->IsPlayingCooperativeGametype() )
				{
					ThrowGrenade( m_lastEnemyPosition );
				}
			}
				
			// check if enemy died
			if (m_enemy->IsAlive())
			{
				m_enemyDeathTimestamp = 0.0f;
				m_isLastEnemyDead = false;
			}
			else if (m_enemyDeathTimestamp == 0.0f)
			{
				// note time of death (to allow bots to overshoot for a time)
				m_enemyDeathTimestamp = gpGlobals->curtime;
				m_isLastEnemyDead = true;
			}
		}
	}
	else
	{
		m_isEnemyVisible = false;
	}

	// if we have seen an enemy recently, keep an eye on him if we can

	{

	if (!IsBlind() && !IsLookingAtSpot(PRIORITY_UNINTERRUPTABLE) )
	{
		const float seenRecentTime = 3.0f;
		if (m_enemy != nullptr && GetTimeSinceLastSawEnemy() < seenRecentTime)
		{
			AimAtEnemy();
		}
		else
		{
			StopAiming();
		}
	}
	else if( IsAimingAtEnemy() )
	{
		StopAiming();
	}

	}

	//
	// Hack to fire while retreating
	/// @todo Encapsulate aiming and firing on enemies separately from current task
	//
	if (GetDisposition() == IGNORE_ENEMIES)
	{

		FireWeaponAtEnemy();
	}

	{

	// toss grenades
	LookForGrenadeTargets();

	// process grenade throw state machine
	UpdateGrenadeThrow();

	// avoid enemy grenades
	AvoidEnemyGrenades();

	}

	// check if our weapon is totally out of ammo
	// or if we no longer feel "safe", equip our weapon
	if (!IsSafe() && !IsUsingGrenade() && IsActiveWeaponOutOfAmmo())
	{
		EquipBestWeapon();
	}

	/// @todo This doesn't work if we are restricted to just knives and sniper rifles because we cant use the rifle at close range
	if (!IsSafe() && !IsUsingGrenade() && IsUsingKnife() && !IsEscapingFromBomb())
	{
		EquipBestWeapon();
	}

	// if we haven't seen an enemy in awhile, and we switched to our pistol during combat, 
	// switch back to our primary weapon (if it still has ammo left)
	const float safeRearmTime = 5.0f;
	if (!IsReloading() && IsUsingPistol() && !IsPrimaryWeaponEmpty() && GetTimeSinceLastSawEnemy() > safeRearmTime)
	{
		EquipBestWeapon();
	}

	{

	// reload our weapon if we must
	ReloadCheck();

	// equip silencer
	SilencerCheck();

	// listen to the radio
	RespondToRadioCommands();

	}

	// make way
	const float avoidTime = 0.33f;
	if (gpGlobals->curtime - m_avoidTimestamp < avoidTime && m_avoid != nullptr)
	{
		StrafeAwayFromPosition( GetCentroid( m_avoid ) );
	}
	else
	{
		m_avoid = nullptr;
	}

	// if we're using a sniper rifle and are no longer attacking, stop looking thru scope
	if (!IsAtHidingSpot() && !IsAttacking() && IsUsingSniperRifle() && IsUsingScope())
	{
		SecondaryAttack();
	}

	if (!IsBlind())
	{

		// check encounter spots
		UpdatePeripheralVision();

		// watch for snipers
		if (CanSeeSniper() && !HasSeenSniperRecently())
		{

			GetChatter()->SpottedSniper();

			const float sniperRecentInterval = 20.0f;
			m_sawEnemySniperTimer.Start( sniperRecentInterval );
		}

		//
		// Update gamestate
		//
		if (m_bomber != nullptr)
			GetChatter()->SpottedBomber( GetBomber() );

		if (CanSeeLooseBomb())
			GetChatter()->SpottedLooseBomb( TheCSBots()->GetLooseBomb() );
	}

	// if we're burning, escape from the flames
	if ( GetTimeSinceBurnedByFlames() < 1.0f && !IsEscapingFromFlames() )
	{
		EscapeFromFlames();
		return;
	}

	//
	// Scenario interrupts
	//

	{

	switch (TheCSBots()->GetScenario())
	{
		case CCSBotManager::SCENARIO_DEFUSE_BOMB:
		{
			// flee if the bomb is ready to blow and we aren't defusing it or attacking and we know where the bomb is
			// (aggressive players wait until its almost too late)
			float gonnaBlowTime = 8.0f - (2.0f * GetProfile()->GetAggression());

			// if we have a defuse kit, can wait longer
			if (m_bHasDefuser)
				gonnaBlowTime *= 0.66f;

			if (!IsEscapingFromBomb() &&								// we aren't already escaping the bomb
				TheCSBots()->IsBombPlanted() &&							// is the bomb planted
				GetGameState()->IsPlantedBombLocationKnown() &&			// we know where the bomb is
				TheCSBots()->GetBombTimeLeft() < gonnaBlowTime &&		// is the bomb about to explode
				!IsDefusingBomb() &&									// we aren't defusing the bomb
				!IsAttacking())											// we aren't in the midst of a firefight
			{
				EscapeFromBomb();
				break;
			}

			break;
		}

		case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
		{
			if (GetTeamNumber() == TEAM_CT)
			{
				UpdateHostageEscortCount();
			}
			else
			{
				// Terrorists have imperfect information on status of hostages
				unsigned char status = GetGameState()->ValidateHostagePositions();

				if (status & CSGameState::HOSTAGES_ALL_GONE)
				{
					GetChatter()->HostagesTaken();
					Idle();
				}
				else if (status & CSGameState::HOSTAGE_GONE)
				{
					GetGameState()->HostageWasTaken();
					Idle();
				}
			}
			break;
		}
	}

	}

	//
	// Follow nearby humans if our co-op is high and we have nothing else to do
	// If we were just following someone, don't auto-follow again for a short while to 
	// give us a chance to do something else.
	//
	const float earliestAutoFollowTime = 5.0f;
	const float minAutoFollowTeamwork = 0.4f;
	if (cv_bot_auto_follow.GetBool() &&
		TheCSBots()->GetElapsedRoundTime() > earliestAutoFollowTime &&
		GetProfile()->GetTeamwork() > minAutoFollowTeamwork && 
		CanAutoFollow() &&
		!IsBusy() && 
		!IsFollowing() && 
		!IsBlind() && 
		!GetGameState()->IsAtPlantedBombsite())
	{

		// chance of following is proportional to teamwork attribute
		if (GetProfile()->GetTeamwork() > RandomFloat( 0.0f, 1.0f ))
		{
			CCSPlayer *leader = GetClosestVisibleHumanFriend();
			if (leader && leader->IsAutoFollowAllowed())
			{
				// count how many bots are already following this player
				const float maxFollowCount = 2;
				if (GetBotFollowCount( leader ) < maxFollowCount)
				{
					const float autoFollowRange = 300.0f;
					Vector leaderOrigin = GetCentroid( leader );
					if ((leaderOrigin - myOrigin).IsLengthLessThan( autoFollowRange ))
					{
						CNavArea *leaderArea = TheNavMesh->GetNavArea( leaderOrigin );
						if (leaderArea)
						{
							PathCost cost( this, FASTEST_ROUTE );
							float travelRange = NavAreaTravelDistance( GetLastKnownArea(), leaderArea, cost );
							if (travelRange >= 0.0f && travelRange < autoFollowRange)
							{
								// follow this human
								Follow( leader );
								PrintIfWatched( "Auto-Following %s\n", leader->GetPlayerName() );

								if (CSGameRules()->IsCareer())
								{
									GetChatter()->Say( "FollowingCommander", 10.0f );
								}
								else
								{
									GetChatter()->Say( "FollowingSir", 10.0f );
								}
							}
						}
					}
				}
			}
		}
		else
		{	

			// we decided not to follow, don't re-check for a duration
			m_allowAutoFollowTime = gpGlobals->curtime + 15.0f + (1.0f - GetProfile()->GetTeamwork()) * 30.0f;
		}
	}

	if (IsFollowing())
	{
		// if we are following someone, make sure they are still alive
		CBaseEntity *leader = m_leader;
		if (leader == nullptr || !leader->IsAlive())
		{
			StopFollowing();
		}

		// decide whether to continue following them
		const float highTeamwork = 0.85f;
		if (GetProfile()->GetTeamwork() < highTeamwork)
		{
			float minFollowDuration = 15.0f;
			if (GetFollowDuration() > minFollowDuration + 40.0f * GetProfile()->GetTeamwork())
			{
				// we are bored of following our leader
				StopFollowing();
				PrintIfWatched( "Stopping following - bored\n" );
			}
		}
	}

	//
	// Execute state machine
	//

	{

	if (m_isOpeningDoor)
	{

		// opening doors takes precedence over attacking because DoorCheck() will stop opening doors if
		// we don't have a knife out.
		m_openDoorState.OnUpdate( this );

		if (m_openDoorState.IsDone())
		{
			// open door behavior is finished, return to previous behavior context
			m_openDoorState.OnExit( this );
			m_isOpeningDoor = false;
		}
	}
	else if (m_isAttacking)
	{

		m_attackState.OnUpdate( this );
	}
	else
	{
		//SNPROF(m_state->GetName());

		m_state->OnUpdate( this );
	}

	// do wait behavior
	if (!IsAttacking() && IsWaiting())
	{

		ResetStuckMonitor();
		ClearMovement();
	}

	// don't move while reloading unless we see an enemy
	if (IsReloading() && !m_isEnemyVisible)
	{
		ResetStuckMonitor();
		ClearMovement();
	}

	}

	// if we get too far ahead of the hostages we are escorting, wait for them
	if (!IsAttacking() && m_inhibitWaitingForHostageTimer.IsElapsed())
	{

		const float waitForHostageRange = 500.0f;
		if ((GetTask() == COLLECT_HOSTAGES || GetTask() == RESCUE_HOSTAGES) && GetRangeToFarthestEscortedHostage() > waitForHostageRange)
		{
			if (!m_isWaitingForHostage)
			{
				// just started waiting
				m_isWaitingForHostage = true;
				m_waitForHostageTimer.Start( 10.0f );
			}
			else
			{
				// we've been waiting
				if (m_waitForHostageTimer.IsElapsed())
				{
					// give up waiting for awhile
					m_isWaitingForHostage = false;
					m_inhibitWaitingForHostageTimer.Start( 3.0f );
				}
				else
				{
					// keep waiting
					ResetStuckMonitor();
					ClearMovement();
				}
			}
		}
	}

	// remember our prior safe time status
	m_wasSafe = IsSafe();
}

//--------------------------------------------------------------------------------------------------------------
class DrawTravelTime 
{
public:
	DrawTravelTime( const CCSBot *me )
	{
		m_me = me;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player->IsAlive() && !m_me->InSameTeam( player ))
		{
			CFmtStr msg;
			player->EntityText(	0,
								msg.sprintf( "%3.0f", m_me->GetTravelDistanceToPlayer( (CCSPlayer *)player ) ),
								0.1f );

			if (m_me->DidPlayerJustFireWeapon( ToCSPlayer( player ) ))
			{
				player->EntityText( 1, "BANG!", 0.1f );
			}
		}

		return true;
	}

	const CCSBot *m_me;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Render bot debug info
 */
void CCSBot::DebugDisplay( void ) const
{
	const float duration = 0.15f;
	CFmtStr msg;
	
	NDebugOverlay::ScreenText( 0.5f, 0.34f, msg.sprintf( "Skill: %d%%", (int)(100.0f * GetProfile()->GetSkill()) ), 255, 255, 255, 150, duration );

	if ( m_pathLadder )
	{
		NDebugOverlay::ScreenText( 0.5f, 0.36f, msg.sprintf( "Ladder: %d", m_pathLadder->GetID() ), 255, 255, 255, 150, duration );
	}

	// show safe time
	float safeTime = GetSafeTimeRemaining();
	if (safeTime > 0.0f)
	{
		NDebugOverlay::ScreenText( 0.5f, 0.38f, msg.sprintf( "SafeTime: %3.2f", safeTime ), 255, 255, 255, 150, duration );
	}

	// show if blind
	if (IsBlind())
	{
		NDebugOverlay::ScreenText( 0.5f, 0.38f, msg.sprintf( "<<< BLIND >>>" ), 255, 255, 255, 255, duration );
	}

	// show if alert
	if (IsAlert())
	{
		NDebugOverlay::ScreenText( 0.5f, 0.38f, msg.sprintf( "ALERT" ), 255, 0, 0, 255, duration );
	}

	// show if panicked
	if (IsPanicking())
	{
		NDebugOverlay::ScreenText( 0.5f, 0.36f, msg.sprintf( "PANIC" ), 255, 255, 0, 255, duration );
	}

	// show behavior variables
	if (m_isAttacking)
	{
		NDebugOverlay::ScreenText( 0.5f, 0.4f, msg.sprintf( "ATTACKING: %s", GetBotEnemy()->GetPlayerName() ), 255, 0, 0, 255, duration );
	}
	else
	{
		NDebugOverlay::ScreenText( 0.5f, 0.4f, msg.sprintf( "State: %s", m_state->GetName() ), 255, 255, 0, 255, duration );
		NDebugOverlay::ScreenText( 0.5f, 0.42f, msg.sprintf( "Task: %s", GetTaskName() ), 0, 255, 0, 255, duration );
		NDebugOverlay::ScreenText( 0.5f, 0.44f, msg.sprintf( "Disposition: %s", GetDispositionName() ), 100, 100, 255, 255, duration );
		NDebugOverlay::ScreenText( 0.5f, 0.46f, msg.sprintf( "Morale: %s", GetMoraleName() ), 0, 200, 200, 255, duration );
	}

	// show look at status
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT)
	{
		const char *string = msg.sprintf( "LookAt: %s (%s)", m_lookAtDesc, (m_lookAtSpotState == LOOK_TOWARDS_SPOT) ? "LOOK_TOWARDS_SPOT" : "LOOK_AT_SPOT" );

		NDebugOverlay::ScreenText( 0.5f, 0.60f, string, 255, 255, 0, 150, duration );
	}

	NDebugOverlay::ScreenText( 0.5f, 0.62f, msg.sprintf( "Steady view = %s", HasViewBeenSteady( 0.2f ) ? "YES" : "NO" ), 255, 255, 0, 150, duration );

	// show friend/foes I know of
	NDebugOverlay::ScreenText( 0.5f, 0.64f, msg.sprintf( "Nearby friends = %d", m_nearbyFriendCount ), 100, 255, 100, 150, duration );
	NDebugOverlay::ScreenText( 0.5f, 0.66f, msg.sprintf( "Nearby enemies = %d", m_nearbyEnemyCount ), 255, 100, 100, 150, duration );

	if ( m_lastNavArea )
	{
		NDebugOverlay::ScreenText( 0.5f, 0.68f, msg.sprintf( "Nav Area: %d (%s)", m_lastNavArea->GetID(), m_szLastPlaceName.Get() ), 255, 255, 255, 150, duration );
		/*
		if ( cv_bot_traceview.GetBool() )
		{
			if ( m_currentArea )
			{
				NDebugOverlay::Line( GetAbsOrigin(), m_currentArea->GetCenter(), 0, 255, 0, true, 0.1f );
			}
			else if ( m_lastKnownArea )
			{
				NDebugOverlay::Line( GetAbsOrigin(), m_lastKnownArea->GetCenter(), 255, 0, 0, true, 0.1f );
			}
			else if ( m_lastNavArea )
			{
				NDebugOverlay::Line( GetAbsOrigin(), m_lastNavArea->GetCenter(), 0, 0, 255, true, 0.1f );
			}
		}
		*/
	}

	// show debug message history
	float y = 0.8f;
	const float lineHeight = 0.02f;
	const float fadeAge = 7.0f;
	const float maxAge = 10.0f;
	for( int i=0; i<TheBots->GetDebugMessageCount(); ++i )
	{
		const CBotManager::DebugMessage *msg = TheBots->GetDebugMessage( i );

		if (msg->m_age.GetElapsedTime() < maxAge)
		{
			int alpha = 255;

			if (msg->m_age.GetElapsedTime() > fadeAge)
			{
				alpha *= (1.0f - (msg->m_age.GetElapsedTime() - fadeAge) / (maxAge - fadeAge));
			}

			NDebugOverlay::ScreenText( 0.5f, y, msg->m_string, 255, 255, 255, alpha, duration );
			y += lineHeight;
		}
	}

	// show noises
	const Vector *noisePos = GetNoisePosition();
	if (noisePos)
	{
		const float size = 25.0f;
		NDebugOverlay::VertArrow( *noisePos + Vector( 0, 0, size ), *noisePos, size/4.0f, 255, 255, 0, 0, true, duration );
	}

	// show aim spot
	if (IsAimingAtEnemy())
	{
		// since this is executed on the server, we don't have a way of rendering something view relative, 
		// so project out the aim vector to a distance corresponding to the target distance
		Vector toCurrent = m_targetSpot - EyePositionConst();
		float fDistance = toCurrent.Length();
		Vector aimVector;
		AngleVectors( m_aimGoal, &aimVector );
		Vector aimTarget = EyePositionConst() +  aimVector * fDistance;

		NDebugOverlay::Cross3D( aimTarget, 8.0f, 255, 255, 0, true, duration );

	}

	if (IsHiding())
	{
		// show approach points
		DrawApproachPoints();
	}
	else
	{
		// show encounter spot data
	}

	// show aim targets

	/*
	const QAngle &angles = const_cast< CCSBot *>( this )->GetPunchAngle();
	NDebugOverlay::ScreenText( 0.3f, 0.66f, msg.sprintf( "Punch angle pitch = %3.2f", angles.x ), 255, 255, 0, 150, duration );
	*/

	DrawTravelTime drawTravelTime( this );
	ForEachPlayer( drawTravelTime );

/*
	// show line of fire
	if ((cv_bot_traceview.GetInt() == 100 && IsLocalPlayerWatchingMe()) || cv_bot_traceview.GetInt() == 101)
	{
		if (!IsFriendInLineOfFire())
		{
			Vector vecAiming = GetViewVector();
			Vector vecSrc	 = EyePositionConst();

			if (GetTeamNumber() == TEAM_TERRORIST)
				UTIL_DrawBeamPoints( vecSrc, vecSrc + 2000.0f * vecAiming, 1, 255, 0, 0 );
			else
				UTIL_DrawBeamPoints( vecSrc, vecSrc + 2000.0f * vecAiming, 1, 0, 50, 255 );
		}
	}

	// show path navigation data
	if (cv_bot_traceview.GetInt() == 1 && IsLocalPlayerWatchingMe())
	{
		Vector from = EyePositionConst();

		const float size = 50.0f;
		//Vector arrow( size * cos( m_forwardAngle * M_PI/180.0f ), size * sin( m_forwardAngle * M_PI/180.0f ), 0.0f );
		Vector arrow( size * (float)cos( m_lookAheadAngle * M_PI/180.0f ), 
					  size * (float)sin( m_lookAheadAngle * M_PI/180.0f ), 
					  0.0f );
		
		UTIL_DrawBeamPoints( from, from + arrow, 1, 0, 255, 255 );
	}

	if (cv_bot_show_nav.GetBool() && m_lastKnownArea)
	{
		m_lastKnownArea->DrawConnectedAreas();
	}
	*/

	if (IsAttacking())
	{
		const float crossSize = 2.0f;
		CCSPlayer *enemy = GetBotEnemy();
		if (enemy)
		{
			NDebugOverlay::Cross3D( GetPartPosition( enemy, GUT ), crossSize, 0, 255, 0, true, 0.1f );
			NDebugOverlay::Cross3D( GetPartPosition( enemy, HEAD ), crossSize, 0, 255, 0, true, 0.1f );
			NDebugOverlay::Cross3D( GetPartPosition( enemy, FEET ), crossSize, 0, 255, 0, true, 0.1f );
			NDebugOverlay::Cross3D( GetPartPosition( enemy, LEFT_SIDE ), crossSize, 0, 255, 0, true, 0.1f );
			NDebugOverlay::Cross3D( GetPartPosition( enemy, RIGHT_SIDE ), crossSize, 0, 255, 0, true, 0.1f );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Periodically compute shortest path distance to each player.
 * NOTE: Travel distance is NOT symmetric between players A and B.  Each much be computed separately.
 */
void CCSBot::UpdateTravelDistanceToAllPlayers( void )
{

	const unsigned char numPhases = 3;

	if (m_updateTravelDistanceTimer.IsElapsed())
	{
		ShortestPathCost pathCost;

		for( int i=1; i<=gpGlobals->maxClients; ++i )
		{
			CCSPlayer *player = static_cast< CCSPlayer * >( UTIL_PlayerByIndex( i ) );

			if (player == nullptr)
				continue;

			if (FNullEnt( player->edict() ))
				continue;

			if (!player->IsPlayer())
				continue;
			
			if (!player->IsAlive())
				continue;

			// skip friends for efficiency
			if ( !IsOtherEnemy( player ) )
				continue;

			int which = player->entindex() % MAX_PLAYERS;

			// if player is very far away, update every third time (on phase 0)
			const float veryFarAway = 4000.0f;
			if (m_playerTravelDistance[ which ] < 0.0f || m_playerTravelDistance[ which ] > veryFarAway)
			{
				if (m_travelDistancePhase != 0)
					continue;
			}
			else
			{
				// if player is far away, update two out of three times (on phases 1 and 2)
				const float farAway = 2000.0f;
				if (m_playerTravelDistance[ which ] > farAway && m_travelDistancePhase == 0)
					continue;
			}

			// if player is fairly close, update often
			m_playerTravelDistance[ which ] = NavAreaTravelDistance( EyePosition(), player->EyePosition(), pathCost );
		}

		// throttle the computation frequency
		const float checkInterval = 1.0f;
		m_updateTravelDistanceTimer.Start( checkInterval );

		// round-robin the phases
		++m_travelDistancePhase;
		if (m_travelDistancePhase >= numPhases)
		{
			m_travelDistancePhase = 0;
		}
	}
}

class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSPlayer;
class CNavArea;
class CNavLadder;
class HidingSpot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

ConVar bot_max_vision_distance_override( side_registry(), "bot_max_vision_distance_override", "-1", FCVAR_CHEAT | FCVAR_REPLICATED, "Max distance bots can see targets.", true, -1, false, 0 );
ConVar bot_ignore_players( side_registry(), "bot_ignore_players", "0", FCVAR_CHEAT, "Bots will not see non-bot players." );
ConVar bot_coop_idle_max_vision_distance( side_registry(), "bot_coop_idle_max_vision_distance", "1400", FCVAR_CHEAT | FCVAR_REPLICATED, "Max distance bots can see targets (in coop) when they are idle, dormant, hiding or asleep.", true, -1, false, 0 );

//--------------------------------------------------------------------------------------------------------------
/**
 * Used to update view angles to stay on a ladder
 */
inline float StayOnLadderLine( CCSBot *me, const CNavLadder *ladder )
{
	// determine our facing
	NavDirType faceDir = AngleToDirection( me->EyeAngles().y );

	const float stiffness = 1.0f;

	// move toward ladder mount point
	switch( faceDir )
	{
		case NORTH:
			return stiffness * (ladder->m_top.x - me->GetAbsOrigin().x);

		case SOUTH:
			return -stiffness * (ladder->m_top.x - me->GetAbsOrigin().x);

		case WEST:
			return -stiffness * (ladder->m_top.y - me->GetAbsOrigin().y);

		case EAST:
			return stiffness * (ladder->m_top.y - me->GetAbsOrigin().y);
	}

	return 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::ComputeLadderAngles( float *yaw, float *pitch )
{
	if ( !yaw || !pitch )
		return;

	Vector myOrigin = GetCentroid( this );

	// set yaw to aim at ladder
	Vector to = m_pathLadder->GetPosAtHeight(myOrigin.z) - myOrigin;
	float idealYaw = UTIL_VecToYaw( to );

	Vector faceDir = (m_pathLadderFaceIn) ? -m_pathLadder->GetNormal() : m_pathLadder->GetNormal();
	QAngle faceAngles;
	VectorAngles( faceDir, faceAngles );

	const float lookAlongLadderRange = 50.0f;
	const float ladderPitchUpApproach = -30.0f;
	const float ladderPitchUpTraverse = -60.0f;		// -80
	const float ladderPitchDownApproach = 0.0f;
	const float ladderPitchDownTraverse = 80.0f;

	// adjust pitch to look up/down ladder as we ascend/descend
	switch( m_pathLadderState )
	{
		case APPROACH_ASCENDING_LADDER:
		{
			Vector to = m_goalPosition - myOrigin;
			*yaw = idealYaw;

			if (to.IsLengthLessThan( lookAlongLadderRange ))
				*pitch = ladderPitchUpApproach;
			break;
		}

		case APPROACH_DESCENDING_LADDER:
		{
			Vector to = m_goalPosition - myOrigin;
			*yaw = idealYaw;

			if (to.IsLengthLessThan( lookAlongLadderRange ))
				*pitch = ladderPitchDownApproach;
			break;
		}

		case FACE_ASCENDING_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( idealYaw + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( idealYaw - 90.0f );
			}
			else
			{
				*yaw = idealYaw;
			}
			*pitch = ladderPitchUpApproach;
			break;

		case FACE_DESCENDING_LADDER:
			*yaw = idealYaw;
			*pitch = ladderPitchDownApproach;
			break;

		case MOUNT_ASCENDING_LADDER:
		case ASCEND_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( idealYaw + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( idealYaw - 90.0f );
			}
			else
			{
				*yaw = faceAngles[ YAW ] + StayOnLadderLine( this, m_pathLadder );
			}
			*pitch = ( m_pathLadderState == ASCEND_LADDER ) ? ladderPitchUpTraverse : ladderPitchUpApproach;
			break;

		case MOUNT_DESCENDING_LADDER:
		case DESCEND_LADDER:
			*yaw = faceAngles[ YAW ] + StayOnLadderLine( this, m_pathLadder );
			*pitch = ( m_pathLadderState == DESCEND_LADDER ) ? ladderPitchDownTraverse : ladderPitchDownApproach;
			break;

		case DISMOUNT_ASCENDING_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( faceAngles[ YAW ] + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( faceAngles[ YAW ] - 90.0f );
			}
			else
			{
				*yaw = faceAngles[ YAW ];
			}
			break;

		case DISMOUNT_DESCENDING_LADDER:
			*yaw = faceAngles[ YAW ];
			break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move actual view angles towards desired ones.
 * This is the only place v_angle is altered.
 * @todo Make stiffness and turn rate constants timestep invariant.
 */
void CCSBot::UpdateLookAngles( void )
{
	CVProfScope _( "CCSBot::UpdateLookAngles", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	const float deltaT = g_BotUpkeepInterval;
	float maxAccel;
	float stiffness;
	float damping;

	// If mimicing the player, don't modify the view angles.
	if ( bot_mimic.GetInt() )
		return;

	// springs are stiffer when attacking, so we can track and move between targets better
	if (IsAttacking())
	{
		stiffness = GetProfile()->GetLookAngleStiffnessAttacking();
		damping = GetProfile()->GetLookAngleDampingAttacking();
		maxAccel = GetProfile()->GetLookAngleMaxAccelerationAttacking();
	}
	else
	{
		stiffness = GetProfile()->GetLookAngleStiffnessNormal();
		damping = GetProfile()->GetLookAngleDampingNormal();
		maxAccel = GetProfile()->GetLookAngleMaxAccelerationNormal();
	}

	// these may be overridden by ladder logic
	float useYaw = m_lookYaw;
	float usePitch = m_lookPitch;

	//
	// Ladders require precise movement, therefore we need to look at the 
	// ladder as we approach and ascend/descend it.
	// If we are on a ladder, we need to look up or down to traverse it - override pitch in this case.
	//
	// If we're trying to break something, though, we actually need to look at it before we can
	// look at the ladder
	//
	if ( IsUsingLadder() && !(IsLookingAtSpot( PRIORITY_HIGH ) && m_lookAtSpotAttack) )
	{
		ComputeLadderAngles( &useYaw, &usePitch );
	}

	// get current view angles
	QAngle viewAngles = EyeAngles();

	//
	// Yaw
	//
	float angleDiff = AngleNormalize( useYaw - viewAngles.y );

	/*
	 * m_forwardAngle is unreliable. Need to simulate mouse sliding & centering
	if (!IsAttacking())
	{
		// do not allow rotation through our reverse facing angle - go the "long" way instead
		float toCurrent = AngleNormalize( pev->v_angle.y - m_forwardAngle );
		float toDesired = AngleNormalize( useYaw - m_forwardAngle );

		// if angle differences are different signs, they cross the forward facing
		if (toCurrent * toDesired < 0.0f)
		{
			// if the sum of the angles is greater than 180, turn the "long" way around
			if (abs( toCurrent - toDesired ) >= 180.0f)
			{
				if (angleDiff > 0.0f)
					angleDiff -= 360.0f;
				else
					angleDiff += 360.0f;
			}
		}
	}
	*/

	// if almost at target angle, snap to it
	const float onTargetTolerance = 0; // 1.0f;		// 3
	if ( std::fabsf(angleDiff) < onTargetTolerance )
	{
		m_lookYawVel = 0.0f;
		viewAngles.y = useYaw;
	}
	else
	{
		// simple angular spring/damper
		float accel = stiffness * angleDiff - damping * m_lookYawVel;

		// limit rate
		if (accel > maxAccel)
			accel = maxAccel;
		else if (accel < -maxAccel)
			accel = -maxAccel;

		m_lookYawVel += deltaT * accel;
		viewAngles.y += deltaT * m_lookYawVel;

		// keep track of how long our view remains steady
		const float steadyYaw = 1000.0f;
		if (fabs( accel ) > steadyYaw)
		{
			m_viewSteadyTimer.Start();
		}
	}

	//
	// Pitch
	// Actually, this is negative pitch.
	//
	angleDiff = usePitch - viewAngles.x;

	angleDiff = AngleNormalize( angleDiff );

	// simple angular spring/damper
	// double the stiffness since pitch is only +/- 90 and yaw is +/- 180
	float accel = 2.0f * stiffness * angleDiff - damping * m_lookPitchVel;

	// limit rate
	if (accel > maxAccel)
		accel = maxAccel;
	else if (accel < -maxAccel)
		accel = -maxAccel;

	m_lookPitchVel += deltaT * accel;
	viewAngles.x += deltaT * m_lookPitchVel;

	// keep track of how long our view remains steady
	const float steadyPitch = 1000.0f;
	if (fabs( accel ) > steadyPitch)
	{
		m_viewSteadyTimer.Start();
	}

	//PrintIfWatched( "yawVel = %g, pitchVel = %g\n", m_lookYawVel, m_lookPitchVel );

	// limit range - avoid gimbal lock
	if (viewAngles.x < -89.0f)
		viewAngles.x = -89.0f;
	else if (viewAngles.x > 89.0f)
		viewAngles.x = 89.0f;

	// update view angles
	SnapEyeAngles( viewAngles );

	// if our weapon is zooming, our view is not steady
	if (IsWaitingForZoom())
	{
		m_viewSteadyTimer.Start();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see the point
 */
bool CCSBot::IsVisible( const Vector &pos, bool testFOV, const CBaseEntity *ignore ) const
{
	CVProfScope _( "CCSBot::IsVisible( pos )", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	// we can't see anything if we're blind
	if (IsBlind())
		return false;

	// is it in my general viewcone?
	if (testFOV && !(const_cast<CCSBot *>(this)->FInViewCone( pos )))
		return false;

	// check line of sight against smoke
	if (TheCSBots()->IsLineBlockedBySmoke( EyePositionConst(), pos ))
		return false;

	// check line of sight
	// Must include CONTENTS_MONSTER to pick up all non-brush objects like barrels
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( ignore, COLLISION_GROUP_NONE );
	UTIL_TraceLine( EyePositionConst(), pos, MASK_VISIBLE_AND_NPCS|CONTENTS_BLOCKLOS, &traceFilter, &result );
	if (result.fraction != 1.0f)
		return false;

	return true;
}

bool CCSBot::IsBeyondBotMaxVisionDistance(const Vector &vecTargetPosition) const
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
	{
		bool bIsIdling = m_bIsSleeping || IsHiding() || IsIdling();

		if ( GetTask() == CCSBot::SNIPING )
			bIsIdling = false; // Snipers need longer vision distance

		if ( bIsIdling || ( g_pMapInfo && g_pMapInfo->m_flBotMaxVisionDistance >= 0 ) )
		{
			float flBotMaxVisionDistance = 3600;
			float flMapMaxDist = g_pMapInfo ? g_pMapInfo->m_flBotMaxVisionDistance : 0;
			float flIdleMaxDist = bot_coop_idle_max_vision_distance.GetFloat();
			if ( bIsIdling )
			{
				// use the lower one
				if ( flMapMaxDist > 0 )
					flBotMaxVisionDistance = (flMapMaxDist < flIdleMaxDist) ? flMapMaxDist : flIdleMaxDist;
				else
					flBotMaxVisionDistance = flIdleMaxDist;
			}
			else if ( g_pMapInfo && g_pMapInfo->m_flBotMaxVisionDistance >= 0 )
			{
				flBotMaxVisionDistance = g_pMapInfo->m_flBotMaxVisionDistance;
			}

			float flEnemyDistance = ( EyePositionConst() - vecTargetPosition ).Length();

			return ( flEnemyDistance > flBotMaxVisionDistance );
		}

		return false;
	}
	else
	{
		if ( !g_pMapInfo || g_pMapInfo->m_flBotMaxVisionDistance < 0 )
			return false;

		const float flBotMaxVisionDistance = ( bot_max_vision_distance_override.GetFloat() > 0 ) ? bot_max_vision_distance_override.GetFloat() : g_pMapInfo->m_flBotMaxVisionDistance;

		float flEnemyDistance = ( EyePositionConst() - vecTargetPosition ).Length();

		return ( flEnemyDistance > flBotMaxVisionDistance );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see any part of the player
 * Check parts in order of importance. Return the first part seen in "visPart" if it is non-NULL.
 */

// We only send back new data every 5 upsates (ie every 1/3 second)

bool CCSBot::IsVisible( CCSPlayer *player, bool testFOV, unsigned char *visParts ) const
{
	CVProfScope _( "CCSBot::IsVisible( player )", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	if ( bot_ignore_players.GetBool() && !player->IsBot() )
		return false;

	// optimization - assume if center is not in FOV, nothing is
	// we're using WorldSpaceCenter instead of GUT so we can skip GetPartPosition below - that's
	// the most expensive part of this, and if we can skip it, so much the better.
	if (testFOV && !(const_cast<CCSBot *>(this)->FInViewCone( player->WorldSpaceCenter() )))
	{
		return false;
	}

	//don't let bots see farther than the map-defined bot max vision distance
	if ( IsBeyondBotMaxVisionDistance( player->WorldSpaceCenter() ) )
	{
		return false;
	}

	unsigned char testVisParts = NONE;

	// check gut
	Vector partPos = GetPartPosition( player, GUT );

	// finish gut check
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == nullptr)
			return true;

		testVisParts |= GUT;
	}

	// check top of head
	partPos = GetPartPosition( player, HEAD );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == nullptr)
			return true;

		testVisParts |= HEAD;
	}

	// check feet
	partPos = GetPartPosition( player, FEET );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == nullptr)
			return true;

		testVisParts |= FEET;
	}

	// check "edges"
	partPos = GetPartPosition( player, LEFT_SIDE );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == nullptr)
			return true;

		testVisParts |= LEFT_SIDE;
	}

	partPos = GetPartPosition( player, RIGHT_SIDE );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == nullptr)
			return true;

		testVisParts |= RIGHT_SIDE;
	}

	if (visParts)
		*visParts = testVisParts;

	if (testVisParts)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Interesting part positions
 */
CCSBot::PartInfo CCSBot::m_partInfo[ MAX_PLAYERS ];

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute part positions from bone location.
 */
void CCSBot::ComputePartPositions( CCSPlayer *player )
{
	// was found and fixed by PiMoNFeeD
	const int headBox = m_bUseNewAnimstate ? 0 : 11;
	const int gutBox = m_bUseNewAnimstate ? 4 : 7;
	const int leftElbowBox = m_bUseNewAnimstate ? 16 : 13;
	const int rightElbowBox = m_bUseNewAnimstate ? 18 : 16;

	const int maxBoxIndex = rightElbowBox;

	CVProfScope _( "CCSBot::ComputePartPositions", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	// which PartInfo corresponds to the given player
	PartInfo *info = &m_partInfo[ player->entindex() % MAX_PLAYERS ];

	// always compute feet, since it doesn't rely on bones
	info->m_feetPos = player->GetAbsOrigin();
	info->m_feetPos.z += 5.0f;

	// get bone positions for interesting points on the player
	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *studioHdr = player->GetModelPtr();
	if (studioHdr)
	{
		mstudiohitboxset_t *set = studioHdr->pHitboxSet( player->GetHitboxSet() );
		if (set && maxBoxIndex < set->numhitboxes)
		{
			QAngle angles;
			mstudiobbox_t *box;

			// gut
			box = set->pHitbox( gutBox );
			player->GetBonePosition( box->bone, info->m_gutPos, angles );	

			// head
			box = set->pHitbox( headBox );
			player->GetBonePosition( box->bone, info->m_headPos, angles );

			Vector forward, right;
			AngleVectors( angles, &forward, &right, nullptr );

			// in local bone space
			const float headForwardOffset = 4.0f;
			const float headRightOffset = 2.0f;
			info->m_headPos += headForwardOffset * forward + headRightOffset * right;

			/// @todo Fix this hack - lower the head target because it's a bit too high for the current T model
			info->m_headPos.z -= 2.0f;

			// left side
			box = set->pHitbox( leftElbowBox );
			player->GetBonePosition( box->bone, info->m_leftSidePos, angles );	

			// right side
			box = set->pHitbox( rightElbowBox );
			player->GetBonePosition( box->bone, info->m_rightSidePos, angles );	

			return;
		}
	}

	// default values if bones are not available
	info->m_headPos = GetCentroid( player );
	info->m_gutPos = info->m_headPos;
	info->m_leftSidePos = info->m_headPos;
	info->m_rightSidePos = info->m_headPos;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return world space position of given part on player.
 * Uses hitboxes to get accurate positions.
 * @todo Optimize by computing once for each player and storing.
 */
const Vector &CCSBot::GetPartPosition( CCSPlayer *player, VisiblePartType part ) const
{
	CVProfScope _( "CCSBot::GetPartPosition", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	// which PartInfo corresponds to the given player
	PartInfo *info = &m_partInfo[ player->entindex() % MAX_PLAYERS ];

	if (gpGlobals->framecount > info->m_validFrame)
	{
		// update part positions
		const_cast< CCSBot * >( this )->ComputePartPositions( player );
		info->m_validFrame = gpGlobals->framecount;
	}

	// return requested part position
	switch( part )
	{
		default:
		{
			contract_assert( false && "GetPartPosition: Invalid part" );
			// fall thru to GUT
		}

		case GUT:
			return info->m_gutPos;

		case HEAD:
			return info->m_headPos;
			
		case FEET:
			return info->m_feetPos;

		case LEFT_SIDE:
			return info->m_leftSidePos;
			
		case RIGHT_SIDE:
			return info->m_rightSidePos;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update desired view angles to point towards m_lookAtSpot
 */
void CCSBot::UpdateLookAt( void )
{
	Vector to = m_lookAtSpot - EyePositionConst();

	QAngle idealAngle;
	VectorAngles( to, idealAngle );

	//Vector idealAngle = UTIL_VecToAngles( to );
	//idealAngle.x = 360.0f - idealAngle.x;

	SetLookAngles( idealAngle.y, idealAngle.x );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Look at the given point in space for the given duration (-1 means forever)
 */
void CCSBot::SetLookAt( const char *desc, const Vector &pos, PriorityType pri, float duration, bool clearIfClose, float angleTolerance, bool attack )
{
	if (IsBlind())
		return;

	// if currently looking at a point in space with higher priority, ignore this request
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && m_lookAtSpotPriority > pri)
		return;

	// if already looking at this spot, just extend the time
	const float tolerance = 10.0f;
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && VectorsAreEqual( pos, m_lookAtSpot, tolerance ))
	{
		m_lookAtSpotDuration = duration;

		if (m_lookAtSpotPriority < pri)
			m_lookAtSpotPriority = pri;
	}
	else
	{
		// look at new spot
		m_lookAtSpot = pos; 
		m_lookAtSpotState = LOOK_TOWARDS_SPOT;
		m_lookAtSpotDuration = duration;
		m_lookAtSpotPriority = pri;
	}

	m_lookAtSpotAngleTolerance = angleTolerance;
	m_lookAtSpotClearIfClose = clearIfClose;
	m_lookAtDesc = desc;
	m_lookAtSpotAttack = attack;

	PrintIfWatched( "%3.1f SetLookAt( %s ), duration = %f\n", gpGlobals->curtime, desc, duration );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Block all "look at" and "look around" behavior for given duration - just look ahead
 */
void CCSBot::InhibitLookAround( float duration )
{
	m_inhibitLookAroundTimestamp = gpGlobals->curtime + duration;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update enounter spot timestamps, etc
 */
void CCSBot::UpdatePeripheralVision()
{
	CVProfScope _( "CCSBot::UpdatePeripheralVision", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	const float peripheralUpdateInterval = 0.29f;		// if we update at 10Hz, this ensures we test once every three
	if (gpGlobals->curtime - m_peripheralTimestamp < peripheralUpdateInterval)
		return;

	m_peripheralTimestamp = gpGlobals->curtime;

	if (m_spotEncounter)
	{
		// check LOS to all spots in case we see them with our "peripheral vision"
		const SpotOrder *spotOrder;
		Vector pos;

		for ( const SpotOrder &order : m_spotEncounter->spots )
		{
			spotOrder = &order;

			const Vector &spotPos = spotOrder->spot->GetPosition();

			pos.x = spotPos.x;
			pos.y = spotPos.y;
			pos.z = spotPos.z + HalfHumanHeight;

			if (!IsVisible( pos, true ))
				continue;

			// can see hiding spot, remember when we saw it last
			SetHidingSpotCheckTimestamp( spotOrder->spot );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update the "looking around" behavior.
 */
void CCSBot::UpdateLookAround( bool updateNow )
{
	CVProfScope _( "CCSBot::UpdateLookAround", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	//
	// If we recently saw an enemy, look towards where we last saw them
	// Unless we can hear them moving, in which case look towards the noise
	//
	const float closeRange = 500.0f;
	if (!IsNoiseHeard() || GetNoiseRange() > closeRange)
	{
		const float recentThreatTime = 1.0f; // 0.25f;
		if (!IsLookingAtSpot( PRIORITY_MEDIUM ) && gpGlobals->curtime - m_lastSawEnemyTimestamp < recentThreatTime)
		{
			ClearLookAt();

			Vector spot = m_lastEnemyPosition;

			// find enemy position on the ground
			if (TheNavMesh->GetSimpleGroundHeight( m_lastEnemyPosition, &spot.z ))
			{
				spot.z += HalfHumanHeight;
				SetLookAt( "Last Enemy Position", spot, PRIORITY_MEDIUM, RandomFloat( 2.0f, 3.0f ), true );
				return;
			}
		}
	}

	//
	// Look at nearby enemy noises
	//
	if (UpdateLookAtNoise())
		return;

	// check if looking around has been inhibited
	// Moved inhibit to allow high priority enemy lookats to still occur
	if (gpGlobals->curtime < m_inhibitLookAroundTimestamp)
		return;

	//
	// If we are hiding (or otherwise standing still), watch all approach points leading into this region
	//
	const float minStillTime = 2.0f;
	if (IsAtHidingSpot() || IsNotMoving( minStillTime ))
	{
		// update approach points
		const float recomputeApproachPointTolerance = 50.0f;
		if ((m_approachPointViewPosition - GetAbsOrigin()).IsLengthGreaterThan( recomputeApproachPointTolerance ))
		{
			ComputeApproachPoints();
			m_approachPointViewPosition = GetAbsOrigin();
		}

		// if we're sniping, zoom in to watch our approach points
		if (IsUsingSniperRifle())
		{
			// low skill bots don't pre-zoom
			if (GetProfile()->GetSkill() > 0.4f)
			{
				if (!IsViewMoving())
				{
					float range = ComputeWeaponSightRange();
					AdjustZoom( range );
				}
				else
				{
					// zoom out
					if (GetZoomLevel() != NO_ZOOM)
						SecondaryAttack();
				}
			}
		}

		if (m_lastKnownArea == nullptr)
			return;

		if (gpGlobals->curtime < m_lookAroundStateTimestamp)
			return;

		// if we're sniping, switch look-at spots less often
		if (IsUsingSniperRifle())
			m_lookAroundStateTimestamp = gpGlobals->curtime + RandomFloat( 5.0f, 10.0f );
		else
			m_lookAroundStateTimestamp = gpGlobals->curtime + RandomFloat( 1.0f, 2.0f );	// 0.5, 1.0

		constexpr auto MAX_APPROACHES = 16;
		Vector validSpot[ MAX_APPROACHES ];
		int validSpotCount = 0;

		Vector *earlySpot = nullptr;
		float earliest = 999999.9f;

		for ( int i = 0; i < m_approachPointCount; ++i )
		{
			float spotTime = m_approachPoint[ i ].m_area->GetEarliestOccupyTime( OtherTeam( GetTeamNumber() ) );

			// ignore approach areas the enemy could not have possibly reached yet
			if ( TheCSBots()->GetElapsedRoundTime() >= spotTime )
			{
				validSpot[ validSpotCount++ ] = m_approachPoint[ i ].m_pos;
			}
			else
			{
				// keep track of earliest spot we can see in case we get there very early
				if ( spotTime < earliest )
				{
					earlySpot = &m_approachPoint[ i ].m_pos;
					earliest = spotTime;
				}
			}
		}

		Vector spot;

		if (validSpotCount)
		{
			int which = RandomInt( 0, validSpotCount-1 );
			spot = validSpot[ which ];
		}
		else if (earlySpot)
		{
			// all of the spots we can see can't be reached yet by the enemy - look at the earliest spot
			spot = *earlySpot;
		}
		else
		{
			return;
		}

		// don't look at the floor, look roughly at chest level
		/// @todo If this approach point is very near, this will cause us to aim up in the air if were crouching
		spot.z += HalfHumanHeight;

		SetLookAt( "Approach Point (Hiding)", spot, PRIORITY_LOW );

		return;
	}

	//
	// Glance at "encouter spots" as we move past them
	//
	if (m_spotEncounter)
	{
		//
		// Check encounter spots
		//
		if (!IsSafe() && !IsLookingAtSpot( PRIORITY_LOW ))
		{
			// allow a short time to look where we're going
			if (gpGlobals->curtime < m_spotCheckTimestamp)
				return;

			/// @todo Use skill parameter instead of accuracy

			// lower skills have exponentially longer delays
			float asleep = (1.0f - GetProfile()->GetSkill());
			asleep *= asleep;
			asleep *= asleep;

			m_spotCheckTimestamp = gpGlobals->curtime + asleep * RandomFloat( 10.0f, 30.0f );

			// figure out how far along the path segment we are
			Vector delta = m_spotEncounter->path.to - m_spotEncounter->path.from;
			float length = delta.Length();
			float adx = (float)fabs(delta.x);
			float ady = (float)fabs(delta.y);
			float t;
			Vector myOrigin = GetCentroid( this );

			if (adx > ady)
				t = (myOrigin.x - m_spotEncounter->path.from.x) / delta.x;
			else
				t = (myOrigin.y - m_spotEncounter->path.from.y) / delta.y;

			// advance parameter a bit so we "lead" our checks
			const float leadCheckRange = 50.0f;
			t += leadCheckRange / length;

			if (t < 0.0f)
				t = 0.0f;
			else if (t > 1.0f)
				t = 1.0f;

			// collect the unchecked spots so far
			constexpr auto MAX_DANGER_SPOTS = 16;
			HidingSpot *dangerSpot[MAX_DANGER_SPOTS];
			int dangerSpotCount = 0;
			int dangerIndex = 0;

			const float checkTime = 10.0f;
			const SpotOrder *spotOrder;
			for ( const SpotOrder &order : m_spotEncounter->spots )
			{
				spotOrder = &order;

				// if we have seen this spot recently, we don't need to look at it
				if (gpGlobals->curtime - GetHidingSpotCheckTimestamp( spotOrder->spot ) <= checkTime)
					continue;

				if (spotOrder->t > t)
					break;

				// ignore spots the enemy could not have possibly reached yet
				if (spotOrder->spot->GetArea())
				{
					if (TheCSBots()->GetElapsedRoundTime() < spotOrder->spot->GetArea()->GetEarliestOccupyTime( OtherTeam( GetTeamNumber() ) ))
					{
						continue;
					}
				}

				dangerSpot[ dangerIndex++ ] = spotOrder->spot;
				if (dangerIndex >= MAX_DANGER_SPOTS)
					dangerIndex = 0;
				if (dangerSpotCount < MAX_DANGER_SPOTS)
					++dangerSpotCount;
			}

			if (dangerSpotCount)
			{
				// pick one of the spots at random
				int which = RandomInt( 0, dangerSpotCount-1 );

				// glance at the spot for minimum time 
				SetLookAt( "Encounter Spot", dangerSpot[which]->GetPosition() + Vector( 0, 0, HalfHumanHeight ), PRIORITY_LOW, 0.2f, true, 10.0f );

				// immediately mark it as "checked", so we don't check it again
				// if we get distracted before we check it - that's the way it goes
				SetHidingSpotCheckTimestamp( dangerSpot[which] );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * "Bend" our line of sight around corners until we can "see" the point. 
 */
bool CCSBot::BendLineOfSight( const Vector &eye, const Vector &target, Vector *bend, float angleLimit ) const
{
	CVProfScope _( "CCSBot::BendLineOfSight", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	bool doDebug = false;
	const float debugDuration = 0.04f;
	if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
		NDebugOverlay::Line( eye, target, 255, 255, 255, true, debugDuration );

	// if we can directly see the point, use it
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceLine( eye, target, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );
	if (result.fraction == 1.0f && !result.startsolid)
	{
		// can directly see point, no bending needed
		*bend = target;
		return true;
	}

	// "bend" our line of sight until we can see the approach point
	Vector to = target - eye;
	float startAngle = UTIL_VecToYaw( to );
	float length = to.Length2D();
	to.NormalizeInPlace();

	struct Color3
	{
		int r, g, b;
	};
	const int colorCount = 6;
	Color3 colorSet[ colorCount ] =
	{
		{ 255,   0,   0 },
		{   0, 255,   0 },
		{   0,   0, 255 },
		{ 255, 255,   0 },
		{   0, 255, 255 },
		{ 255,   0, 255 },
	};

	int color = 0;

	// optiming assumption - previous rays cast "shadow" on subsequent rays since they already
	// enumerated visible space along their length.
	// We should do a dot product and compute the exact length, but since the angular changes
	// are incremental, using the direct length should be close enough.
	float priorVisibleLength[2] = { 0.0f, 0.0f };

	float angleInc = 5.0f; 
	for( float angle = angleInc; angle <= angleLimit; angle += angleInc )
	{
		// check both sides at this angle offset
		for( int side=0; side<2; ++side )
		{
			float actualAngle = (side) ? (startAngle + angle) : (startAngle - angle);

			float dx = std::cos( 3.141592f * actualAngle / 180.0f );
			float dy = std::sin( 3.141592f * actualAngle / 180.0f );

			// compute rotated point ray endpoint
			Vector rotPoint( eye.x + length * dx, eye.y + length * dy, target.z );

			// check LOS to find length to test along ray
			UTIL_TraceLine( eye, rotPoint, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );

			// if this ray started in an obstacle, skip it
			if (result.startsolid)
			{
				continue;
			}

			Vector ray = rotPoint - eye;
			float rayLength = ray.NormalizeInPlace();
			float visibleLength = rayLength * result.fraction;

			if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
			{
				NDebugOverlay::Line( eye, eye + visibleLength * ray, colorSet[color].r, colorSet[color].g, colorSet[color].b, true, debugDuration );
			}

			// step along ray, checking if point is visible from ray point
			const float bendStepSize = 50.0f; 

			// start from point that prior rays couldn't see
			float startLength = priorVisibleLength[ side ];

			for( float bendLength=startLength; bendLength <= visibleLength; bendLength += bendStepSize )
			{
				// compute point along ray
				Vector bendPoint = eye + bendLength * ray;

				// check if we can see approach point from this bend point
				UTIL_TraceLine( bendPoint, target, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );

				if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
				{
					NDebugOverlay::Line( bendPoint, result.endpos, colorSet[color].r/2, colorSet[color].g/2, colorSet[color].b/2, true, debugDuration );
				}

				if (result.fraction == 1.0f && !result.startsolid)
				{
					// target is visible from this bend point on the ray - use this point on the ray as our point

					// keep "bent" point at correct height along line of sight
					bendPoint.z = eye.z + bendLength * to.z;

					*bend = bendPoint;

					return true;
				}	
			}

			priorVisibleLength[ side ] = visibleLength;

			++color;
			if (color >= colorCount)
			{
				color = 0;
			}
		} // side
	}

	// bending rays didn't help - still can't see the point
	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we "notice" given player
 * @todo Increase chance if player is rotating
 * @todo Decrease chance as nears edge of FOV
 */
bool CCSBot::IsNoticable( const CCSPlayer *player, unsigned char visParts ) const
{
	// if this player has just fired his weapon, we notice him
	if (DidPlayerJustFireWeapon( player ))
	{
		return true;
	}

	//don't let bots see farther than the map-defined bot max vision distance
	if ( IsBeyondBotMaxVisionDistance( player->WorldSpaceCenter() ) )
	{
		return false;
	}

	float deltaT = m_attentionInterval.GetElapsedTime();

	// all chances are specified in terms of a standard "quantum" of time
	// in which a normal person would notice something
	const float noticeQuantum = 0.25f;

	// determine percentage of player that is visible
	float coverRatio = 0.0f;

	if (visParts & GUT)
	{
		const float chance = 40.0f;
		coverRatio += chance;
	}

	if (visParts & HEAD)
	{
		const float chance = 10.0f;
		coverRatio += chance;
	}

	if (visParts & LEFT_SIDE)
	{
		const float chance = 20.0f;
		coverRatio += chance;
	}

	if (visParts & RIGHT_SIDE)
	{
		const float chance = 20.0f;
		coverRatio += chance;
	}

	if (visParts & FEET)
	{
		const float chance = 10.0f;
		coverRatio += chance;
	}

	// compute range modifier - farther away players are harder to notice, depeding on what they are doing
	float range = (player->GetAbsOrigin() - GetAbsOrigin()).Length();
	const float closeRange = 300.0f;
	const float farRange = 1000.0f;

	float rangeModifier;
	if (range < closeRange)
	{
		rangeModifier = 0.0f;
	}
	else if (range > farRange)
	{
		rangeModifier = 1.0f;
	}
	else
	{
		rangeModifier = (range - closeRange)/(farRange - closeRange);
	}

	// harder to notice when crouched
	bool isCrouching = (player->GetFlags() & FL_DUCKING) != 0;

	// moving players are easier to spot
	float playerSpeedSq = player->GetAbsVelocity().LengthSqr();
	const float runSpeed = 200.0f;
	const float walkSpeed = 30.0f;
	float farChance, closeChance;
	if (playerSpeedSq > runSpeed * runSpeed)
	{
		// running players are always easy to spot (must be standing to run)
		return true;
	}
	else if (playerSpeedSq > walkSpeed * walkSpeed)
	{
		// walking players are less noticable far away
		if (isCrouching)
		{
			closeChance = 90.0f;
			farChance = 60.0f;
		}
		else // standing
		{
			closeChance = 100.0f;
			farChance = 75.0f;
		}
	}
	else
	{
		// motionless players are hard to notice
		if (isCrouching)
		{
			// crouching and motionless - very tough to notice
			closeChance = 80.0f;
			farChance = 5.0f;		// takes about three seconds to notice (50% chance)
		}
		else // standing
		{
			closeChance = 100.0f;
			farChance = 10.0f;
		}
	}

	// combine posture, speed, and range chances
	float dispositionChance = closeChance + (farChance - closeChance) * rangeModifier;

	// determine actual chance of noticing player
	float noticeChance = dispositionChance * coverRatio/100.0f;

	// scale by skill level
	noticeChance *= (0.5f + 0.5f * GetProfile()->GetSkill());

	// if we are alert, our chance of noticing is much higher
	if (IsAlert())
	{
		const float alertBonus = 50.0f;
		noticeChance += alertBonus;
	}

	// scale by time quantum
	noticeChance *= deltaT / noticeQuantum;

	// there must always be a chance of detecting the enemy
	const float minChance = 0.1f;
	if (noticeChance < minChance)
	{
		noticeChance = minChance;
	}

	//PrintIfWatched( "Notice chance = %3.2f\n", noticeChance );

	return (RandomFloat( 0.0f, 100.0f ) < noticeChance);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return most dangerous threat in my field of view (feeds into reaction time queue).
 * @todo Account for lighting levels, cover, and distance to see if we notice enemy
 */
CCSPlayer *CCSBot::FindMostDangerousThreat( void )
{
	CVProfScope _( "CCSBot::FindMostDangerousThreat", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	if (IsBlind())
	{
		return nullptr;
	}

	enum { MAX_THREATS = 16 };		// maximum number of simulataneously attendable threats	
	struct CloseInfo
	{
		CCSPlayer *enemy;
		float range;
	}
	threat[ MAX_THREATS ];
	int threatCount = 0;

	int prevIndex = m_enemyQueueIndex - 1;
	if ( prevIndex < 0 )
		prevIndex = MAX_ENEMY_QUEUE - 1;
	CCSPlayer *currentThreat = m_enemyQueue[ prevIndex ].player;

	m_bomber = nullptr;
	m_isEnemySniperVisible = false;

	m_closestVisibleFriend = nullptr;
	float closeFriendRange = 99999999999.9f;

	m_closestVisibleHumanFriend = nullptr;
	float closeHumanFriendRange = 99999999999.9f;

	CCSPlayer *sniperThreat = nullptr;
	float sniperThreatRange = 99999999999.9f;
	bool sniperThreatIsFacingMe = false;

	const float lookingAtMeTolerance = 0.7071f;

	int i;

	{
		CVProfScope _( "CCSBot::Collect Threats", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

		for( i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *entity = UTIL_PlayerByIndex( i );

			if (entity == nullptr)
				continue;

			// is it a player?
			if (!entity->IsPlayer())
				continue;

			CCSPlayer *player = static_cast<CCSPlayer *>( entity );

			// ignore self
			if (player->entindex() == entindex())
				continue;

			// is it alive?
			if (!player->IsAlive())
				continue;

			// is it an enemy?
			if ( !IsOtherEnemy( player ) )
			{

				// keep track of nearby friends - use less exact visibility check
				if (IsVisible( entity->WorldSpaceCenter(), false, this ))
				{
					// update watch timestamp
					int idx = player->entindex();
					m_watchInfo[idx].timestamp = gpGlobals->curtime;
					m_watchInfo[idx].isEnemy = false;

					// keep track of our closest friend 
					Vector to = GetAbsOrigin() - player->GetAbsOrigin();
					float rangeSq = to.LengthSqr();
					if (rangeSq < closeFriendRange)
					{
						m_closestVisibleFriend = player;
						closeFriendRange = rangeSq;
					}

					// keep track of our closest human friend 
					if (!player->IsBot() && rangeSq < closeHumanFriendRange)
					{
						m_closestVisibleHumanFriend = player;
						closeHumanFriendRange = rangeSq;
					}
				}

				continue;
			}

			// check if this enemy is fully or partially visible

			unsigned char visParts;
			if (!IsVisible( player, true, &visParts ))
				continue;

			// do we notice this enemy? (always notice current enemy)
			if (player != currentThreat)
			{
				if (!IsNoticable( player, visParts ))
				{
					continue;
				}
			}

			// update watch timestamp
			int idx = player->entindex();
			m_watchInfo[idx].timestamp = gpGlobals->curtime;
			m_watchInfo[idx].isEnemy = true;

			// note if we see the bomber
			if (player->HasC4())
			{
				m_bomber = player;
				
			}

			// keep track of all visible threats
			Vector d = GetAbsOrigin() - player->GetAbsOrigin();
			float distSq = d.LengthSqr();

			// track enemy sniper threats
			if (IsSniperRifle( player->GetActiveCSWeapon() ))
			{
				m_isEnemySniperVisible = true;

				// keep track of the most dangerous sniper we see
				if (sniperThreat)
				{
					if (IsPlayerLookingAtMe( player, lookingAtMeTolerance ))
					{
						if (sniperThreatIsFacingMe)
						{
							// several snipers are facing us - keep closest
							if (distSq < sniperThreatRange)
							{
								sniperThreat = player;
								sniperThreatRange = distSq;
								sniperThreatIsFacingMe = true;
							}
						}
						else
						{
							// even if this sniper is farther away, keep it because he's aiming at us
							sniperThreat = player;
							sniperThreatRange = distSq;
							sniperThreatIsFacingMe = true;
						}
					}
					else
					{
						// this sniper is not looking at us, only consider it if we dont have a sniper facing us
						if (!sniperThreatIsFacingMe && distSq < sniperThreatRange)
						{
							sniperThreat = player;
							sniperThreatRange = distSq;
						}
					}
				}
				else
				{
					// first sniper we see
					sniperThreat = player;
					sniperThreatRange = distSq;
					sniperThreatIsFacingMe = IsPlayerLookingAtMe( player, lookingAtMeTolerance );
				}
			}

			{
				CVProfScope _( "CCSBot::Sort Threats", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

				// maintain set of visible threats, sorted by increasing distance
				if (threatCount == 0)
				{
					threat[0].enemy = player;
					threat[0].range = distSq;
					threatCount = 1;
				}
				else
				{
					// find insertion point
					int j;
					for( j=0; j<threatCount; ++j )
					{
						if (distSq < threat[j].range)
							break;
					}

					// shift lower half down a notch
					for( int k=threatCount-1; k>=j; --k )
						threat[k+1] = threat[k];

					// insert threat into sorted list
					threat[j].enemy = player;
					threat[j].range = distSq;

					if (threatCount < MAX_THREATS)
						++threatCount;
				}
			}
		}
	}

	{
		CVProfScope _( "CCSBot::Count nearby Friends & Enemies", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

		// track the maximum enemy and friend counts we've seen recently
		int prevEnemies = m_nearbyEnemyCount;
		m_nearbyEnemyCount = 0;
		m_nearbyFriendCount = 0;
		for( i=0; i<MAX_PLAYERS; ++i )
		{
			if (m_watchInfo[i].timestamp <= 0.0f)
				continue;

			const float recentTime = 3.0f;
			if (gpGlobals->curtime - m_watchInfo[i].timestamp < recentTime)
			{
				if (m_watchInfo[i].isEnemy)
					++m_nearbyEnemyCount;
				else
					++m_nearbyFriendCount;
			}
		}

		// note when we saw this batch of enemies
		if (prevEnemies == 0 && m_nearbyEnemyCount > 0)
		{
			m_firstSawEnemyTimestamp = gpGlobals->curtime;
		}
	}

	{
		CVProfScope _( "CCSBot::Track enemy Place", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

		//
		// Track the place where we saw most of our enemies
		//
		struct PlaceRank
		{
			unsigned int place;
			int count;
		};
		static PlaceRank placeRank[ MAX_PLACES_PER_MAP ];
		int locCount = 0;

		PlaceRank common;
		common.place = 0;
		common.count = 0;

		for( i=0; i<threatCount; ++i )
		{
			// find the area the player/bot is standing on
			CNavArea *area;
			CCSBot *bot = ToCSBot( threat[i].enemy );
			if (bot && bot->IsBot())
			{
				area = bot->GetLastKnownArea();
			}
			else
			{
				Vector enemyOrigin = GetCentroid( threat[i].enemy );
				area = TheNavMesh->GetNearestNavArea( enemyOrigin );
			}

			if (area == nullptr)
				continue;

			unsigned int threatLoc = area->GetPlace();
			if (!threatLoc)
				continue;

			// if place is already in set, increment count
			int j;
			for( j=0; j<locCount; ++j )
				if (placeRank[j].place == threatLoc)
					break;

			if (j == locCount)
			{
				// new place
				if (locCount < MAX_PLACES_PER_MAP)
				{
					placeRank[ locCount ].place = threatLoc;
					placeRank[ locCount ].count = 1;

					if (common.count == 0)
						common = placeRank[locCount];

					++locCount;
				}
			}
			else
			{
				// others are in that place, increment
				++placeRank[j].count;

				// keep track of the most common place
				if (placeRank[j].count > common.count)
					common = placeRank[j];
			}
		}

		// remember most common place
		m_enemyPlace = common.place;
	}

	{
		CVProfScope _( "CCSBot::Select Threat", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

		if (threatCount == 0)
			return nullptr;

		// if we can still see our current threat, keep it
		// unless a new one is much closer
		bool sawCloserThreat = false;
		bool sawCurrentThreat = false;
		int t;
		for( t=0; t<threatCount; ++t )
		{
			if ( threat[t].enemy == currentThreat )
			{
				sawCurrentThreat = true;
			}
			else if ( threat[t].enemy != currentThreat &&
				IsSignificantlyCloser( threat[t].enemy, currentThreat ) )
			{
				sawCloserThreat = true;
			}
		}

		if ( sawCurrentThreat && !sawCloserThreat )
		{
			return currentThreat;
		}

		// if we are a sniper and we see a sniper threat, attack it unless 
		// there are other close enemies facing me
		if (IsSniper() && sniperThreat)
		{
			const float closeCombatRange = 500.0f;

			for( t=0; t<threatCount; ++t )
			{
				if (threat[t].range < closeCombatRange && IsPlayerLookingAtMe( threat[t].enemy, lookingAtMeTolerance ))
				{
					return threat[t].enemy;
				}
			}

			return sniperThreat;
		}
		
		// otherwise, find the closest threat that is looking at me
		for( t=0; t<threatCount; ++t )
		{
			if (IsPlayerLookingAtMe( threat[t].enemy, lookingAtMeTolerance ))
			{
				return threat[t].enemy;
			}
		}
	}

	// return closest threat
	return threat[0].enemy;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update our reaction time queue
 */
void CCSBot::UpdateReactionQueue( void )
{
	CVProfScope _( "CCSBot::UpdateReactionQueue", 0, VPROF_BUDGETGROUP_NPCS, false, BUDGETFLAG_OTHER );

	// zombies dont see any threats
	if (cv_bot_zombie.GetBool())
		return;

	// find biggest threat at this instant
	CCSPlayer *threat = FindMostDangerousThreat();

	// reset timer
	m_attentionInterval.Start();

	int now = m_enemyQueueIndex;

	// store a snapshot of its state at the end of the reaction time queue
	if (threat)
	{
		m_enemyQueue[ now ].player = threat;
		m_enemyQueue[ now ].isReloading = threat->IsReloading();
		m_enemyQueue[ now ].isProtectedByShield = threat->IsProtectedByShield();
	}
	else
	{
		m_enemyQueue[ now ].player = nullptr;
		m_enemyQueue[ now ].isReloading = false;
		m_enemyQueue[ now ].isProtectedByShield = false;
	}

	// queue is round-robin
	++m_enemyQueueIndex;
	if (m_enemyQueueIndex >= MAX_ENEMY_QUEUE)
		m_enemyQueueIndex = 0;
	
	if (m_enemyQueueCount < MAX_ENEMY_QUEUE)
		++m_enemyQueueCount;

	// clamp reaction time to enemy queue size
	float reactionTime = GetProfile()->GetReactionTime() - g_BotUpdateInterval;
	float maxReactionTime = (MAX_ENEMY_QUEUE * g_BotUpdateInterval) - 0.01f;
	if (reactionTime > maxReactionTime)
		reactionTime = maxReactionTime;

	// "rewind" time back to our reaction time
	int reactionTimeSteps = (int)((reactionTime / g_BotUpdateInterval) + 0.5f);

	int i = now - reactionTimeSteps;
	if (i < 0)
		i += MAX_ENEMY_QUEUE;

	m_enemyQueueAttendIndex = (unsigned char)i;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the most dangerous threat we are "conscious" of
 */
CCSPlayer *CCSBot::GetRecognizedEnemy( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount || IsBlind())
	{
		return nullptr;
	}

	return m_enemyQueue[ m_enemyQueueAttendIndex ].player;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is reloading
 */
bool CCSBot::IsRecognizedEnemyReloading( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount)
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isReloading;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is hiding behind a shield
 */
bool CCSBot::IsRecognizedEnemyProtectedByShield( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount)
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isProtectedByShield;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return distance to closest enemy we are "conscious" of
 */
float CCSBot::GetRangeToNearestRecognizedEnemy( void )
{
	const CCSPlayer *enemy = GetRecognizedEnemy();

	if (enemy)
		return (GetAbsOrigin() - enemy->GetAbsOrigin()).Length();

	return 99999999.9f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Blind the bot for the given duration
 */
void CCSBot::Blind( float holdTime, float fadeTime, float startingAlpha )
{
	PrintIfWatched( "Blinded: holdTime = %3.2f, fadeTime = %3.2f, alpha = %3.2f\n", holdTime, fadeTime, startingAlpha );

	// if we were only blinded a little bit, shake it off
	const float mildBlindTime = 3.0f;
	if (holdTime < mildBlindTime)
	{
		Wait( 0.75f * holdTime );
		BecomeAlert();
		BaseClass::Blind( holdTime, fadeTime, startingAlpha );
		return;
	}

	// if blinded while in combat - then spray and pray!
	if ( CSGameRules()->IsPlayingOffline() && CSGameRules()->GetCustomBotDifficulty() == CUSTOM_BOT_DIFFICULTY_DUMB )
	{
		// For Offline games: Bots in dumb mode should not fire their weapons when flash-banged
		m_blindFire = false;
	}
	else
	{
		m_blindFire = IsAttacking();
	}

	// retreat
	// do this first, so spot selection happens before IsBlind() is set
	const float hideRange = 400.0f;
	TryToRetreat( hideRange );

	PrintIfWatched( "I'm blind!\n" );

	if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
	{
		GetChatter()->Say( "Blinded", 1.0f );
	}

	// no longer safe
	AdjustSafeTime();

	// decide which way to move while blind
	m_blindMoveDir = static_cast<NavRelativeDirType>( RandomInt( 1, NUM_RELATIVE_DIRECTIONS-1 ) );

	// if we're defusing, don't give up
	if (IsDefusingBomb())
	{
		return;
	}

	// can't see to aim at enemy
	StopAiming();

	// dont override "facing away" behavior unless we are going to spray and pray
	if (m_blindFire)
	{
		ClearLookAt();

		// just look straight ahead while blind
		Vector forward;
		EyeVectors( &forward );
		SetLookAt( "Blind", EyePosition() + 10000.0f * forward, PRIORITY_UNINTERRUPTABLE, holdTime + 0.5f * fadeTime );
	}
	
	StopWaiting();
	BecomeAlert();

	BaseClass::Blind( holdTime, fadeTime, startingAlpha );
}

//--------------------------------------------------------------------------------------------------------------
class CheckLookAt
{
public:
	CheckLookAt( const CCSBot *me, bool testFOV )
	{
		m_me = me;
		m_testFOV = testFOV;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (!m_me->IsEnemy( player ))
			return true;

		if (m_testFOV && !(const_cast< CCSBot * >(m_me)->FInViewCone( player->WorldSpaceCenter() )))
			return true;

		if (!m_me->IsPlayerLookingAtMe( player ))
			return true;
			
		if (m_me->IsVisible( (CCSPlayer *)player ))
			return false;

		return true;
	}

	const CCSBot *m_me;
	bool m_testFOV;
};

/**
 * Return true if any enemy I have LOS to is looking directly at me
 * @todo Use reaction time pipeline
 */
bool CCSBot::IsAnyVisibleEnemyLookingAtMe( bool testFOV ) const
{
	CheckLookAt checkLookAt( this, testFOV );
	return (ForEachPlayer( checkLookAt ) == false) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do panic behavior
 */
void CCSBot::UpdatePanicLookAround( void )
{
	if (m_panicTimer.IsElapsed())
	{
		return;
	}

	if (IsEnemyVisible())
	{
		StopPanicking();
		return;
	}

	if (HasLookAtTarget())
	{
		// wait until we finish our current look at
		return;
	}

	// select a spot somewhere behind us to look at as we search for our attacker
	const QAngle &eyeAngles = EyeAngles();

	QAngle newAngles;
	newAngles.x = RandomFloat( -30.0f, 30.0f );

	// Look directly behind at a random offset in a 90 window.
	newAngles.y = eyeAngles.y + 180.0f + RandomFloat(-45.0f, +45.0f );

	newAngles.z = 0.0f;

	Vector forward;
	AngleVectors( newAngles, &forward );

	Vector spot;
	spot = EyePosition() + 1000.0f * forward;

	SetLookAt( "Panic", spot, PRIORITY_HIGH, 0.0f );
	PrintIfWatched( "Panic yaw angle = %3.2f\n", newAngles.y );
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CNavArea;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Fire our active weapon towards our current enemy
 * NOTE: Aiming our weapon is handled in RunBotUpkeep()
 */
void CCSBot::FireWeaponAtEnemy( void )
{
	if (cv_bot_dont_shoot.GetBool())
	{
		return;
	}

	CBasePlayer *enemy = GetBotEnemy();
	if (enemy == nullptr)
	{
		return;
	}

	Vector myOrigin = GetCentroid( this );

	if (IsUsingSniperRifle())
	{
		// if we're using a sniper rifle, don't fire until we are standing still, are zoomed in, and not rapidly moving our view
		if (!IsNotMoving() || IsWaitingForZoom() || !HasViewBeenSteady( GetProfile()->GetReactionTime() ) )
		{
			return;
		}
	}

	if (gpGlobals->curtime > m_fireWeaponTimestamp &&
		GetTimeSinceAcquiredCurrentEnemy() >= GetProfile()->GetAttackDelay() &&
		!IsSurprised())
	{
		if (!(IsRecognizedEnemyProtectedByShield() && IsPlayerFacingMe( enemy )) &&	// don't shoot at enemies behind shields
			!IsReloading() && 
			!IsActiveWeaponClipEmpty() && 
			//gpGlobals->curtime > m_reacquireTimestamp &&
			IsEnemyVisible())
		{
			// we have a clear shot - pull trigger if we are aiming at enemy
			Vector toAimSpot = m_targetSpot - EyePosition();
			float rangeToEnemy = toAimSpot.NormalizeInPlace();

			if ( IsUsingSniperRifle() )
			{
				// check our accuracy versus our target distance
				float fProjectedSpread = rangeToEnemy * GetActiveCSWeapon()->GetInaccuracy();
				float fRequiredSpread = IsUsing( WEAPON_AWP ) ? 50.0f : 25.0f;	// AWP will kill with any hit
				if ( fProjectedSpread > fRequiredSpread )
					return;
			}

			// get actual view direction vector
			Vector aimDir = GetViewVector();

			float onTarget = DotProduct( toAimSpot, aimDir );

			// aim more precisely with a sniper rifle
			// because rifles' bullets spray, don't have to be very precise
			const float halfSize = (IsUsingSniperRifle()) ? HalfHumanWidth : 2.0f * HalfHumanWidth;

			// aiming tolerance depends on how close the target is - closer targets subtend larger angles
			float aimTolerance = (float)std::cos( std::atan( halfSize / rangeToEnemy ) );

			if (onTarget > aimTolerance)
			{
				bool doAttack;

				// if friendly fire is on, don't fire if a teammate is blocking our line of fire
				if (TheCSBots()->AllowFriendlyFireDamage())
				{
					if (IsFriendInLineOfFire())
						doAttack = false;
					else
						doAttack = true;
				}
				else
				{
					// fire freely
					doAttack = true;
				}

				if (doAttack)
				{
					// if we are using a knife, only swing it if we're close
					if (IsUsingKnife())
					{
						const float knifeRange = 75.0f;		// 50
						if (rangeToEnemy < knifeRange)
						{
							// since we've given ourselves away - run!
							ForceRun( 5.0f );

							// if our prey is facing away, backstab him!
							if (!IsPlayerFacingMe( enemy ))
							{
								SecondaryAttack();
							}
							else
							{
								// randomly choose primary and secondary attacks with knife
								const float knifeStabChance = 33.3f;
								if (RandomFloat( 0, 100 ) < knifeStabChance)
									SecondaryAttack();
								else
									PrimaryAttack();
							}
						}
					}
					else
					{
						PrimaryAttack();
					}
				}

				if (IsUsingPistol())
				{
					// high-skill bots fire their pistols quickly at close range
					const float closePistolRange = 360.0f;
					if (GetProfile()->GetSkill() > 0.75f && rangeToEnemy < closePistolRange)
					{
						// fire as fast as possible
						m_fireWeaponTimestamp = 0.0f; 
					}
					else
					{
						// fire somewhat quickly
						m_fireWeaponTimestamp = RandomFloat( 0.15f, 0.4f );
					}
				}
				else	// not using a pistol
				{
					const float sprayRange = 400.0f;
					if (GetProfile()->GetSkill() < 0.5f || rangeToEnemy < sprayRange || IsUsingMachinegun())
					{
						// spray 'n pray if enemy is close, or we're not that good, or we're using the big machinegun
						m_fireWeaponTimestamp = 0.0f;
					}
					else
					{
						const float distantTargetRange = 800.0f;
						if (!IsUsingSniperRifle() && rangeToEnemy > distantTargetRange)
						{
							// if very far away, fire slowly for better accuracy
							m_fireWeaponTimestamp = RandomFloat( 0.3f, 0.7f );
						}
						else
						{
							// fire short bursts for accuracy
							m_fireWeaponTimestamp = RandomFloat( 0.15f, 0.25f );		// 0.15, 0.5
						}
					}
				}

				// subtract system latency
				m_fireWeaponTimestamp -= g_BotUpdateInterval;

				m_fireWeaponTimestamp += gpGlobals->curtime;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the current aim offset
 */
void CCSBot::PickNewAimSpot()
{
	// aim at enemy, if he's still alive
	if (m_enemy != nullptr && m_enemy->IsAlive())
	{
		Vector enemyOrigin = GetCentroid( m_enemy );

		if ( IsEnemyVisible() )
		{
			//
			// Enemy is visible - determine which part of him to shoot at
			//
			const float sharpshooter = 0.8f;
			VisiblePartType aimAtPart;

			if (IsUsingMachinegun())
			{
				// spray the big machinegun at the enemy's gut
				aimAtPart = GUT;
			}
			else if (IsUsing( WEAPON_AWP ) || IsUsingShotgun())
			{
				// these weapons are best aimed at the chest
				aimAtPart = GUT;
			}
			else if (GetProfile()->GetSkill() > 0.5f && IsActiveWeaponRecoilHigh() )
			{
				// sprayin' and prayin' - aim at the gut since we're not going to be accurate
				aimAtPart = GUT;
			}
			else if (GetProfile()->GetSkill() < sharpshooter)
			{
				// low skill bots don't go for headshots
				aimAtPart = GUT;
			}
			else
			{
				// high skill - aim for the head
				aimAtPart = HEAD;
			}

			if (IsEnemyPartVisible( aimAtPart ))
			{
				m_targetSpot = GetPartPosition( GetBotEnemy(), aimAtPart );
			}
			else
			{
				// desired part is blocked - aim at whatever part is visible 
				if (IsEnemyPartVisible( GUT ))
				{
					m_targetSpot = GetPartPosition( GetBotEnemy(), GUT );
				}
				else if (IsEnemyPartVisible( HEAD ))
				{
					m_targetSpot = GetPartPosition( GetBotEnemy(), HEAD );
				}
				else if (IsEnemyPartVisible( LEFT_SIDE ))
				{
					m_targetSpot = GetPartPosition( GetBotEnemy(), LEFT_SIDE );
				}
				else if (IsEnemyPartVisible( RIGHT_SIDE ))
				{
					m_targetSpot = GetPartPosition( GetBotEnemy(), RIGHT_SIDE );
				}
				else // FEET
				{
					m_targetSpot = GetPartPosition( GetBotEnemy(), FEET );
				}
			}

			// temp test
			m_targetSpot = GetPartPosition( GetBotEnemy(), GUT );

			m_targetSpotVelocity = m_enemy->GetAbsVelocity();
			m_targetSpotTime = gpGlobals->curtime;
		}
		else
		{
			// aim where we last saw enemy - but bend the ray so we don't point directly into walls
			// if we put this back, make sure you only bend the ray ONCE and keep the bent spot - don't continually recompute
			//BendLineOfSight( m_eyePosition, m_lastEnemyPosition, &m_aimSpot );
			m_targetSpot = m_lastEnemyPosition;
 			m_targetSpotVelocity.Zero();
		}

		// decay old aim focus angle
		m_aimFocus *= std::expf(std::logf(GetProfile()->GetAimFocusDecay()) * m_aimFocusInterval);

		// calculate current aim focus maxima
		Vector toTarget = m_targetSpot - EyePositionConst();
		QAngle aimGoal;
		VectorAngles( toTarget, aimGoal );
		QAngle viewAngles = EyeAngles();
		float deltaYaw = AngleDistance(viewAngles.y, aimGoal.y);
		float deltaPitch = AngleDistance(viewAngles.x, aimGoal.x);
		float fAngleOffset = std::sqrtf(deltaYaw * deltaYaw + deltaPitch * deltaPitch);
		float fNewMaxFocus = std::min(60.0f, fAngleOffset) * GetProfile()->GetAimFocusOffsetScale();
		m_aimFocus = std::max(m_aimFocus, fNewMaxFocus);

		float fTheta = RandomFloat(0.0f, 2.0f * std::numbers::pi);
		float fRadius = RandomFloat(0.0f, m_aimFocus);

		m_aimError[YAW] = fRadius * std::cosf(fTheta);
		m_aimError[PITCH] = fRadius * std::sinf(fTheta);
	}

	// define time when aim offset will automatically be updated
	m_aimFocusInterval = GetProfile()->GetAimFocusInterval();
	m_aimFocusInterval *= RandomFloat(0.8f, 1.2f);	// add some randomness
	m_aimFocusNextUpdate = gpGlobals->curtime + m_aimFocusInterval;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Wiggle aim error based on GetProfile()->GetSkill()
 */
void CCSBot::UpdateAimPrediction( void )
{
	float fTimeSinceAimSpot = gpGlobals->curtime - m_targetSpotTime;
	m_targetSpotPredicted = m_targetSpot + fTimeSinceAimSpot * m_targetSpotVelocity;

	Vector toTarget = m_targetSpotPredicted - EyePositionConst();

	VectorAngles( toTarget, m_aimGoal );
	if ( m_aimGoal[PITCH] > 180.0f )
		m_aimGoal[PITCH] -= 360.f;

	m_aimGoal += m_aimError;

	// adjust aim angle for recoil
	float fSkill = GetProfile()->GetSkill();	// 0.0 .. 1.0
	float fPunchAngleCorrectionFactor = 1.0f + (1.f - fSkill) * .8f * SlowNoise( 6.f );
	const QAngle &punchAngles = GetAimPunchAngle();
	m_aimGoal -= punchAngles * fPunchAngleCorrectionFactor;
	m_aimGoal[PITCH] = clamp(m_aimGoal[PITCH], -89.0f, +89.0f);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Change our zoom level to be appropriate for the given range.
 * Return true if the zoom level changed.
 */
bool CCSBot::AdjustZoom( float range )
{
	bool adjustZoom = false;

	if (IsUsingSniperRifle())
	{
		const float sniperZoomRange = 150.0f;	// NOTE: This must be less than sniperMinRange in AttackState
		const float sniperFarZoomRange = 1500.0f;

		// if range is too close, don't zoom
		if (range <= sniperZoomRange)
		{
			// zoom out
			if (GetZoomLevel() != NO_ZOOM)
			{
				adjustZoom = true;
			}
		}
		else if (range < sniperFarZoomRange)
		{
			// maintain low zoom
			if (GetZoomLevel() != LOW_ZOOM)
			{
				adjustZoom = true;
			}
		}
		else
		{
			// maintain high zoom
			if (GetZoomLevel() != HIGH_ZOOM)
			{
				adjustZoom = true;
			}
		}
	}
	else
	{
		// zoom out
		if (GetZoomLevel() != NO_ZOOM)
		{
			adjustZoom = true;
		}
	}

	if (adjustZoom)
	{
		SecondaryAttack();

		// pause after zoom to allow "eyes" to refocus
// 		m_zoomTimer.Start( 0.25f + (1.0f - GetProfile()->GetSkill()) );
		m_zoomTimer.Start( 0.25f );
	}

	return adjustZoom;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if using the specific weapon
 */
bool CCSBot::IsUsing( CSWeaponID weaponID ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == nullptr)
		return false;

	if (weapon->IsA( weaponID ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we are using a weapon with a removable silencer
 */
bool CCSBot::DoesActiveWeaponHaveRemoveableSilencer( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == nullptr)
		return false;
	
	if ( weapon->IsA( WEAPON_USP ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a sniper rifle
 */
bool CCSBot::IsUsingSniperRifle( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && IsSniperRifle( weapon ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a sniper rifle in our inventory
 */
bool CCSBot::IsSniper( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

	if (weapon && IsSniperRifle( weapon ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are actively sniping (moving to sniper spot or settled in)
 */
bool CCSBot::IsSniping( void ) const
{
	if (GetTask() == MOVE_TO_SNIPER_SPOT || GetTask() == SNIPING)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a shotgun
 */
bool CCSBot::IsUsingShotgun( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == nullptr)
		return false;

	return weapon->IsKindOf(WEAPONTYPE_SHOTGUN);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if using the big 'ol machinegun
 */
bool CCSBot::IsUsingMachinegun( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && weapon->IsA( WEAPON_M249 ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if primary weapon doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPrimaryWeaponEmpty( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

	if (weapon == nullptr)
		return true;

	// check if gun has any ammo left
	if (weapon->HasAnyAmmo())
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if pistol doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPistolEmpty( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );

	if (weapon == nullptr)
		return true;

	// check if gun has any ammo left
	if (weapon->HasAnyAmmo())
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the given item
 */
bool CCSBot::DoEquip( CWeaponCSBase *weapon )
{
	if (weapon == nullptr)
		return false;

	// check if weapon has any ammo left
	if (!weapon->HasAnyAmmo())
		return false;

	// equip it
	SelectItem( weapon->GetClassname() );
	m_equipTimer.Start();

	return true;
}

// throttle how often equipping is allowed
static const float minEquipInterval = 5.0f;

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the best weapon we are carrying that has ammo
 */
void CCSBot::EquipBestWeapon( bool mustEquip )
{
	// throttle how often equipping is allowed
	if (!mustEquip && m_equipTimer.GetElapsedTime() < minEquipInterval)
		return;

	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheBots );

	CWeaponCSBase *primary = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );
	if (primary)
	{
		CSWeaponType weaponClass = primary->GetWeaponType();

		if ((ctrl->AllowShotguns() && weaponClass == WEAPONTYPE_SHOTGUN) ||
			(ctrl->AllowMachineGuns() && weaponClass == WEAPONTYPE_MACHINEGUN) || 
			(ctrl->AllowRifles() && weaponClass == WEAPONTYPE_RIFLE) || 
			(ctrl->AllowShotguns() && weaponClass == WEAPONTYPE_SHOTGUN) ||
			(ctrl->AllowSnipers() && weaponClass == WEAPONTYPE_SNIPER_RIFLE) || 
			(ctrl->AllowSubMachineGuns() && weaponClass == WEAPONTYPE_SUBMACHINEGUN))
		{
			if (DoEquip( primary ))
				return;
		}
	}

	if (ctrl->AllowPistols())
	{
		if (DoEquip( static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) ) ))
			return;
	}

	// always have a knife
	EquipKnife();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip our pistol
 */
void CCSBot::EquipPistol( void )
{
	// throttle how often equipping is allowed
	if (m_equipTimer.GetElapsedTime() < minEquipInterval)
		return;

	if (TheCSBots()->AllowPistols() && !IsUsingPistol())
	{
		CWeaponCSBase *pistol = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );
		DoEquip( pistol );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the knife
 */
void CCSBot::EquipKnife( void )
{
	if (!IsUsingKnife())
	{
		SelectItem( "weapon_knife" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a grenade in our inventory
 */
bool CCSBot::HasGrenade( void ) const
{
	CWeaponCSBase *grenade = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_GRENADES ) );
	return (grenade) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip a grenade, return false if we cant
 */
bool CCSBot::EquipGrenade( bool noSmoke )
{
	// snipers don't use grenades
	if (IsSniper())
		return false;

	if ( CSGameRules()->IsPlayingOffline() && CSGameRules()->GetCustomBotDifficulty() == CUSTOM_BOT_DIFFICULTY_DUMB )
	{
		// For Offline games: Bots in dumb mode should not toss grenades
		return false;
	}

	if (IsUsingGrenade())
		return true;

	if (HasGrenade())
	{
		CWeaponCSBase *grenade = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_GRENADES ) );

		if (noSmoke && grenade->IsA( WEAPON_SMOKEGRENADE ))
			return false;

		SelectItem( grenade->GetClassname() );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have knife equipped
 */
bool CCSBot::IsUsingKnife( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && (weapon->IsA( WEAPON_KNIFE ) || weapon->IsA( WEAPON_KNIFE_GG )))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have pistol equipped
 */
bool CCSBot::IsUsingPistol( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && weapon->IsPistol())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have a grenade equipped
 */
bool CCSBot::IsUsingGrenade( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (!weapon)
		return false;

	if (weapon->IsA( WEAPON_FLASHBANG ) ||
		weapon->IsA( WEAPON_SMOKEGRENADE ) ||
		weapon->IsA( WEAPON_HEGRENADE ) ||
		weapon->IsA( WEAPON_MOLOTOV ) ||
		weapon->IsA( WEAPON_INCGRENADE ) ||
		weapon->IsA( WEAPON_DECOY ) ||
		weapon->IsA( WEAPON_TAGRENADE ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the process of throwing the grenade
 */
void CCSBot::ThrowGrenade( const Vector &target )
{
	if (IsUsingGrenade() && m_grenadeTossState == NOT_THROWING && !IsOnLadder())
	{
		m_grenadeTossState = START_THROW;
		m_tossGrenadeTimer.Start( 2.0f ); // wait up to two seconds for teammates to clear out of the way

		Vector bend = target;
		BendLineOfSight( target, GetLastKnownEnemyPosition(), &bend );

		const float angleTolerance = 3.0f;
		SetLookAt( "GrenadeThrowBend", bend, PRIORITY_UNINTERRUPTABLE, 4.0f, false, angleTolerance );

		if ( !CSGameRules()->IsPlayingCooperativeGametype() )
			Wait( RandomFloat( 2.0f, 4.0f ) );

		if (cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
		{
			NDebugOverlay::Cross3D( bend, 25.0f, 125, 255, 125, true, 300.0f );
		}

		PrintIfWatched( "%3.2f: Grenade: START_THROW\n", gpGlobals->curtime );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if our weapon can attack
 */
bool CCSBot::CanActiveWeaponFire( void ) const
{
	return ( GetActiveWeapon() && GetActiveWeapon()->m_flNextPrimaryAttack <= gpGlobals->curtime );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find spot to throw grenade ahead of us and "around the corner" along our path
 */
bool CCSBot::FindGrenadeTossPathTarget( Vector *pos )
{
	if (!HasPath())
		return false;

	// find farthest point we can see on the path
	int i;
	for( i=m_pathIndex; i<m_pathLength; ++i )
	{
		if (!FVisible( m_path[i].pos + Vector( 0, 0, HalfHumanHeight ) ))
			break;
	}

	if (i == m_pathIndex)
		return false;

	// find exact spot where we lose sight
	Vector dir = m_path[i].pos - m_path[i-1].pos;
	float length = dir.NormalizeInPlace();

	const float inc = 25.0f;
	Vector p;
	Vector visibleSpot = m_path[i-1].pos;
	for( float t = 0.0f; t<length; t += inc )
	{
		p = m_path[i-1].pos + t * dir;
		p.z += HalfHumanHeight;
		if (!FVisible( p ))
			break;

		visibleSpot = p;
	}

	// massage the location a bit
	visibleSpot.z += 10.0f;

	const float bufferRange = 50.0f;

	trace_t result;
	Vector check;

	// check +X
	check = visibleSpot + Vector( 999.9f, 0, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = result.endpos.x - visibleSpot.x;
		if (range < bufferRange)
		{
			visibleSpot.x = result.endpos.x - bufferRange;
		}
	}

	// check -X
	check = visibleSpot + Vector( -999.9f, 0, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = visibleSpot.x - result.endpos.x;
		if (range < bufferRange)
		{
			visibleSpot.x = result.endpos.x + bufferRange;
		}
	}

	// check +Y
	check = visibleSpot + Vector( 0, 999.9f, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = result.endpos.y - visibleSpot.y;
		if (range < bufferRange)
		{
			visibleSpot.y = result.endpos.y - bufferRange;
		}
	}

	// check -Y
	check = visibleSpot + Vector( 0, -999.9f, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = visibleSpot.y - result.endpos.y;
		if (range < bufferRange)
		{
			visibleSpot.y = result.endpos.y + bufferRange;
		}
	}

	*pos = visibleSpot;
	return true;	
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Look for grenade throw targets and throw the grenade
 */
void CCSBot::LookForGrenadeTargets( void )
{
	if (!IsUsingGrenade() || IsThrowingGrenade())
	{
		return;
	}

	const CNavArea *tossArea = GetInitialEncounterArea();
	if (tossArea == nullptr)
	{
		return;
	}

	int enemyTeam = OtherTeam( GetTeamNumber() );

	// check if we should put our grenade away
	if (tossArea->GetEarliestOccupyTime( enemyTeam ) > gpGlobals->curtime)
	{
		EquipBestWeapon( true );
		return;
	}

	// throw grenades at initial encounter area
	Vector tossTarget = Vector( 0, 0, 0 );
	if (!tossArea->IsVisible( EyePosition(), &tossTarget ))
	{
		return;
	}

	CWeaponCSBase *weapon = GetActiveCSWeapon();
	if (weapon && weapon->IsA( WEAPON_SMOKEGRENADE ))
	{
		// don't worry so much about smokes
		ThrowGrenade( tossTarget );
		PrintIfWatched( "Throwing smoke grenade!" );
		SetInitialEncounterArea( nullptr );
		return;
	}
	else	// explosive and flashbang grenades
	{
		// initial encounter area is visible, wait to throw until timing is right

		const float leadTime = 1.5f;
		float enemyTime = tossArea->GetEarliestOccupyTime( enemyTeam );
		if (enemyTime - TheCSBots()->GetElapsedRoundTime() > leadTime)
		{
			// don't throw yet
			return;
		}

		Vector to = tossTarget - EyePosition();
		float range = to.Length();

		const float slope = 0.2f; // 0.25f;
		float tossHeight = slope * range;

		trace_t result;
		CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );

		const float heightInc = tossHeight / 10.0f;
		Vector target;
		float safeSpace = tossHeight / 2.0f;

		// Build a box to sweep along the ray when looking for obstacles
		const Vector& eyePosition = EyePosition();
		Vector mins = VEC_HULL_MIN;
		Vector maxs = VEC_HULL_MAX;
		mins.z = 0;
		maxs.z = heightInc;

		// find low and high bounds of toss window
		float low = 0.0f;
		float high = tossHeight + safeSpace;
		bool gotLow = false;
		float lastH = 0.0f;
		for( float h = 0.0f; h < 3.0f * tossHeight; h += heightInc )
		{
			target = tossTarget + Vector( 0, 0, h );

			// make sure toss line is clear

			QAngle angles( 0, 0, 0 );
			Ray_t ray;
			ray.Init( eyePosition, target, mins, maxs );
			enginetrace->TraceRay( ray, MASK_VISIBLE_AND_NPCS | CONTENTS_GRATE, &traceFilter, &result );
			if (result.fraction == 1.0f)
			{
				//NDebugOverlay::SweptBox( eyePosition, target, mins, maxs, angles, 0, 0, 255, 40, 10.0f );

				// line is clear
				if (!gotLow)
				{
					low = h;
					gotLow = true;
				}
			}
			else
			{
				//NDebugOverlay::SweptBox( eyePosition, target, mins, maxs, angles, 255, 0, 0, 5, 10.0f );

				// line is blocked
				if (gotLow)
				{
					high = lastH;
					break;
				}
			}

			lastH = h;
		}

		if (gotLow)
		{
			// throw grenade into toss window
			if (tossHeight < low)
			{
				if (low + safeSpace > high)
				{
					// narrow window
					tossHeight = (high + low)/2.0f;
				}
				else
				{
					tossHeight = low + safeSpace;
				}
			}
			else if (tossHeight > high - safeSpace)
			{
				if (high - safeSpace < low)
				{
					// narrow window
					tossHeight = (high + low)/2.0f;
				}
				else
				{
					tossHeight = high - safeSpace;
				}
			}

			ThrowGrenade( tossTarget + Vector( 0, 0, tossHeight ) );
			SetInitialEncounterArea( nullptr );
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
class FOVClearOfFriends
{
public:
	FOVClearOfFriends( CCSBot *me )
	{
		m_me = me;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player == m_me || !player->IsAlive())
			return true;

		if (m_me->InSameTeam( player ))
		{
			Vector to = player->EyePosition() - m_me->EyePosition();
			to.NormalizeInPlace();

			Vector forward;
			m_me->EyeVectors( &forward );

			if (DotProduct( to, forward ) > 0.95f)
			{
				if (m_me->IsVisible( (CCSPlayer *)player ))
				{
					// we see a friend in our FOV
					return false;
				}
			}
		}

		return true;
	}

	CCSBot *m_me;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Process the grenade throw state machine
 */
void CCSBot::UpdateGrenadeThrow( void )
{
	switch( m_grenadeTossState )
	{
		case START_THROW:
		{
			if (m_tossGrenadeTimer.IsElapsed())
			{
				// something prevented the throw - give up
				EquipBestWeapon( true );
				ClearLookAt();
				m_grenadeTossState = NOT_THROWING;
				PrintIfWatched( "%3.2f: Grenade: THROW FAILED\n", gpGlobals->curtime );
				return;
			}

			if (m_lookAtSpotState == LOOK_AT_SPOT)
			{
				// don't throw if there are friends ahead of us
				FOVClearOfFriends fovClear( this );
				if (ForEachPlayer( fovClear ))
				{
					m_grenadeTossState = FINISH_THROW;
					float flGrenadeTossDelay = CSGameRules()->IsPlayingCooperativeGametype() ? 0.01f : 1.0f;
					m_tossGrenadeTimer.Start( flGrenadeTossDelay );
					PrintIfWatched( "%3.2f: Grenade: FINISH_THROW\n", gpGlobals->curtime );
				}
				else
				{
					PrintIfWatched( "%3.2f: Grenade: Friend is in the way...\n", gpGlobals->curtime );
				}
			}

			// hold in the trigger and be ready to throw
			PrimaryAttack();

			break;
		}

		case FINISH_THROW:
		{
			// throw the grenade and hold our aiming line for a moment
			if (m_tossGrenadeTimer.IsElapsed())
			{
				ClearLookAt();

				m_grenadeTossState = NOT_THROWING;
				PrintIfWatched( "%3.2f: Grenade: THROW COMPLETE\n", gpGlobals->curtime );
			}
			break;
		}

		default:
		{
			if (IsUsingGrenade())
			{
				// pull the pin
				PrimaryAttack();
			}
			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
class GrenadeResponse
{
public:
	GrenadeResponse( CCSBot *me )
	{
		m_me = me;
	}

	bool operator() ( ActiveGrenade *ag ) const
	{
		const float retreatRange = 300.0f;
		const float hideTime = 1.0f;

		// do we see this grenade
		if (m_me->IsVisible( ag->GetPosition(), true, (CBaseEntity *)ag->GetEntity() ))
		{
			// we see it
			if (ag->IsSmoke())
			{
				// ignore smokes
				return true;
			}

			Vector velDir = ag->GetEntity()->GetAbsVelocity();
			float grenadeSpeed = velDir.NormalizeInPlace();
			const float atRestSpeed = 50.0f;

			const float aboutToBlow = 0.5f;
			if (ag->IsFlashbang() && ag->GetEntity()->m_flDetonateTime - gpGlobals->curtime < aboutToBlow)
			{
				// turn away from flashbangs about to explode
				QAngle eyeAngles = m_me->EyeAngles();

				float yaw = RandomFloat( 100.0f, 135.0f );
				eyeAngles.y += (RandomFloat( -1.0f, 1.0f ) < 0.0f) ? (-yaw) : yaw;

				Vector forward;
				AngleVectors( eyeAngles, &forward );

				Vector away = m_me->EyePosition() - 1000.0f * forward;
				const float duration = 2.0f;

				m_me->ClearLookAt();
				m_me->SetLookAt( "Avoid Flashbang", away, PRIORITY_UNINTERRUPTABLE, duration );

				m_me->StopAiming();

				return false;
			}

			// flee from grenades if close by or thrown towards us
			const float throwDangerRange = 750.0f;
			const float nearDangerRange = 300.0f;
			Vector to = ag->GetPosition() - m_me->GetAbsOrigin();
			float range = to.NormalizeInPlace();
			if (range > throwDangerRange)
			{
				return true;
			}

			if (grenadeSpeed > atRestSpeed)
			{
				// grenade is moving
				if (DotProduct( to, velDir ) >= -0.5f)
				{
					// going away from us
					return true;
				}

				m_me->PrintIfWatched( "Retreating from a grenade thrown towards me!\n" );
			}
			else if (range < nearDangerRange)
			{
				// grenade has come to rest near us
				m_me->PrintIfWatched( "Retreating from a grenade that landed near me!\n" );
			}

			// retreat!
			m_me->TryToRetreat( retreatRange, hideTime );

			return false;
		}

		return true;
	}

	CCSBot *m_me;
};

/**
 * React to enemy grenades we see
 */
void CCSBot::AvoidEnemyGrenades( void )
{
	// low skill bots dont avoid grenades
	if (GetProfile()->GetSkill() < 0.5)
	{
		return;
	}

	if (IsAvoidingGrenade())
	{
		// already avoiding one
		return;
	}

	// low skill bots don't avoid grenades
	if (GetProfile()->GetSkill() < 0.6f)
	{
		return;
	}

	GrenadeResponse respond( this );	
	if (TheBots->ForEachGrenade( respond ) == false)
	{
		const float avoidTime = 4.0f;
		m_isAvoidingGrenade.Start( avoidTime );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reload our weapon if we must
 */
void CCSBot::ReloadCheck( void )
{
	const float safeReloadWaitTime = 3.0f;
	const float reloadAmmoRatio = 0.6f;

	// don't bother to reload if there are no enemies left
	if (GetEnemiesRemaining() == 0)
		return;

	if (IsDefusingBomb() || IsReloading())
		return;

	if (IsActiveWeaponClipEmpty())
	{
		// high-skill players switch to pistol instead of reloading during combat
		if (GetProfile()->GetSkill() > 0.5f && IsAttacking())
		{
			if (!GetActiveCSWeapon()->IsPistol() && !IsPistolEmpty())
			{
				// switch to pistol instead of reloading
				EquipPistol();
				return;
			}
		}
	}
	else if (GetTimeSinceLastSawEnemy() > safeReloadWaitTime && GetActiveWeaponAmmoRatio() <= reloadAmmoRatio)
	{
		// high-skill players use all their ammo and switch to pistol instead of reloading during combat
		if (GetProfile()->GetSkill() > 0.5f && IsAttacking())
			return;
	}
	else
	{
		// do not need to reload
		return;
	}

	// don't reload the AWP until it is totally out of ammo
	if (IsUsing( WEAPON_AWP ) && !IsActiveWeaponClipEmpty())
		return;

	Reload();

	// move to cover to reload if there are enemies nearby
	if (GetNearbyEnemyCount())
	{
		// avoid enemies while reloading (above 0.75 skill always hide to reload)
		const float hideChance = 25.0f + 100.0f * GetProfile()->GetSkill();

		if (!IsHiding() && RandomFloat( 0, 100 ) < hideChance)
		{
			const float safeTime = 5.0f;
			if (GetTimeSinceLastSawEnemy() < safeTime)
			{
				PrintIfWatched( "Retreating to a safe spot to reload!\n" );
				const Vector *spot = FindNearbyRetreatSpot( this, 1000.0f );
				if (spot)
				{
					// ignore enemies for a second to give us time to hide
					// reaching our hiding spot clears our disposition
					IgnoreEnemies( 10.0f );

					Run();
					StandUp();
					Hide( *spot, 0.0f );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Silence/unsilence our weapon if we must
 */
void CCSBot::SilencerCheck( void )
{
	const float safeSilencerWaitTime = 3.5f; // longer than reload check because reloading should take precedence

	if (IsDefusingBomb() || IsReloading() || IsAttacking())
		return;

	if (!DoesActiveWeaponHaveRemoveableSilencer())
		return;

	if (GetTimeSinceLastSawEnemy() < safeSilencerWaitTime)
		return;

	// don't touch the silencer if there are enemies nearby
	if (GetNearbyEnemyCount() == 0)
	{
		CWeaponCSBase *weapon = GetActiveCSWeapon();
		if (weapon == nullptr)
			return;

		bool isSilencerOn = weapon->IsSilenced();

		if ( weapon->m_flNextSecondaryAttack >= gpGlobals->curtime )
			return;

		// equip silencer if we want to and we don't have a shield.
		if ( isSilencerOn != (GetProfile()->PrefersSilencer() || GetProfile()->GetSkill() > 0.7f) && !HasShield() )
		{
			PrintIfWatched( "%s silencer!\n", (isSilencerOn) ? "Unequipping" : "Equipping" );
			weapon->SecondaryAttack();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when in contact with a CBaseCombatWeapon
 */
bool CCSBot::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	CWeaponCSBase *droppedGun = static_cast< CWeaponCSBase* >( pWeapon );

	// right now we only care about primary weapons on the ground
	if ( droppedGun && droppedGun->GetSlot() == WEAPON_SLOT_RIFLE )
	{
		CWeaponCSBase *myGun = static_cast< CWeaponCSBase* >( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

		// if the gun on the ground is the same one we have, dont bother
		if ( myGun && droppedGun->GetCSWeaponID() != myGun->GetCSWeaponID() )
		{
			// if we don't have a weapon preference, give up
			if ( GetProfile()->HasPrimaryPreference() )
			{
				// don't change weapons if we've seen enemies recently
				const float safeTime = 2.5f;
				if ( GetTimeSinceLastSawEnemy() >= safeTime )
				{
					// we have a primary weapon - drop it if the one on the ground is better
					for( int i = 0; i < GetProfile()->GetWeaponPreferenceCount(); ++i )
					{
						CSWeaponID prefID = GetProfile()->GetWeaponPreference( i );

						if (!IsPrimaryWeapon( prefID ))
							continue;

						// if the gun we are using is more desirable, give up
						if ( prefID == myGun->GetCSWeaponID() )
							break;

						if ( prefID == droppedGun->GetCSWeaponID() )
						{
							// the gun on the ground is better than the one we have - drop our gun
							DropWeaponSlot( WEAPON_SLOT_RIFLE );
							break;
						}
					}
				}
			}
		}
	}

	return BaseClass::BumpWeapon( droppedGun );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if a friend is in our weapon's way
 * @todo Check more rays for safety.
 */
bool CCSBot::IsFriendInLineOfFire( void )
{
	// compute the unit vector along our view
	Vector aimDir = GetViewVector();

	// trace the bullet's path
	trace_t result;
	UTIL_TraceLine( EyePosition(), EyePosition() + 10000.0f * aimDir, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (TraceDidHitNonWorldEntity( result ))
	{
		CBaseEntity *victim = result.Ent<CBaseEntity>();

		if (victim && victim->IsPlayer() && victim->IsAlive())
		{
			CBasePlayer *player = static_cast<CBasePlayer *>( victim );

			if ( !IsOtherEnemy( player->entindex() ) )
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return line-of-sight distance to obstacle along weapon fire ray
 * @todo Re-use this computation with IsFriendInLineOfFire()
 */
float CCSBot::ComputeWeaponSightRange( void )
{
	// compute the unit vector along our view
	Vector aimDir = GetViewVector();

	// trace the bullet's path
	trace_t result;
	UTIL_TraceLine( EyePosition(), EyePosition() + 10000.0f * aimDir, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	return (EyePosition() - result.endpos).Length();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given player just fired their weapon
 */
bool CCSBot::DidPlayerJustFireWeapon( const CCSPlayer *player ) const
{
	// if this player has just fired his weapon, we notice him
	CWeaponCSBase *weapon = player->GetActiveCSWeapon();
	return (weapon && !weapon->IsSilenced() && weapon->m_flNextPrimaryAttack > gpGlobals->curtime);
}

using namespace ::ks::reflect;

// Author: Michael Booth (mike@turtlerockstudios.com), 2003
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------
//
// Temporary solution until we have time to build something more elegant
// Very nasty - need to keep in sync with the buy aliases
// NOTE: Array must be NULL terminated
//
