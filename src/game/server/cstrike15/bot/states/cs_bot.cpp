// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include "eiface.h"
#include "edict.h"
#include "networkvar.h"
#include "worldsize.h"
module ks.server;

import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :fish;
import :nav_4;
import :player;
import :sharedvar;
import :baseentity;
import :vscript;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import :entityutil;
import ks.game.gamemovement;
import std;
import ks.core.dbg;
import ks.core.platform;
import ks.tier1.convar;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.game.shareddefs;
import :econ.econ_item_schema_minimal;
import :entity_2;
import ks.game.mapentities_shared;
import ks.core.vprof;
import ks.math.vector2d;
import :cstrike15.cs_team;

class CBasePlayer;
class CCSBot;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//
// Various tweakable parameters which control overall bot behavior.
// Note[pmf]: I moved them all to the top of the file together so it's easier to get a sense of how they interact
//
static const float crouchFarRange = 750.0f;	// 50% (increased) chance to crouch when enemy is farther than this
static const float hysterisRange = 125.0f;		// (+/-) m_combatRange, used to control dodging
static const float dodgeRange = 2000.0f;		// maximum range from enemy to consider dodging
static const float jumpChance = 33.3f;			// chance of low-skill to jump when first engaging the enemy (if they are moving)
static const float lookAheadRange = 30.0f;		// how far L/R to consider whether we can fall while strafing
static const float sniperMinRange = 160.0f;	// what distance to switch to pistol if enemy is too close. Must be larger than NO_ZOOM range in AdjustZoom()
static const float shotgunMaxRange = 600.0f;	// what distance to switch to pistol if enemy is too far
static const float slashRange = 70.0f;			// when using knife, repath to enemy if they are farther than this
static const float repathInterval = 0.5f;
static const float repathRange = 100.0f;		// repath when enemy has moved this far from our current path endpoint

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin attacking
 */
void AttackState::OnEnter( CCSBot *me )
{
	CBasePlayer *enemy = me->GetBotEnemy();

	// store our posture when the attack began
	me->PushPostureContext();

	me->DestroyPath();

	// if we are using a knife, try to sneak up on the enemy
	if (enemy && me->IsUsingKnife() && !me->IsPlayerFacingMe( enemy ))
		me->Walk();
	else
		me->Run();

	me->GetOffLadder();
	me->ResetStuckMonitor();

	m_repathTimer.Invalidate();
	m_haveSeenEnemy = me->IsEnemyVisible();
	m_nextDodgeStateTimestamp = 0.0f;
	m_firstDodge = true;
	m_isEnemyHidden = false;
	m_reacquireTimestamp = 0.0f;

	m_pinnedDownTimestamp = gpGlobals->curtime + RandomFloat( 7.0f, 10.0f );

	m_shieldToggleTimestamp = gpGlobals->curtime + RandomFloat( 2.0f, 10.0f );
	m_shieldForceOpen = false;

	// if we encountered someone while escaping, grab our weapon and fight!
	if (me->IsEscapingFromBomb())
		me->EquipBestWeapon();

	if (me->IsUsingKnife())
	{
		// can't crouch and hold with a knife
		m_crouchAndHold = false;
		me->StandUp();
	}
	else if (me->CanSeeSniper() && !me->IsSniper())
	{
		// don't sit still if we see a sniper!
		m_crouchAndHold = false;
		me->StandUp();
	}
	else
	{
		// decide whether to crouch where we are, or run and gun (if we havent already - see CCSBot::Attack())
		if (!m_crouchAndHold)
		{
			if (enemy)
			{
				float crouchChance;
				
				// more likely to crouch if using sniper rifle or if enemy is far away
				if (me->IsUsingSniperRifle())
					crouchChance = 50.0f;
				else if ((GetCentroid( me ) - GetCentroid( enemy )).IsLengthGreaterThan( crouchFarRange ))
					crouchChance = 50.0f;
				else
					crouchChance = 20.0f * (1.0f - me->GetProfile()->GetAggression());

				if (RandomFloat( 0.0f, 100.0f ) < crouchChance)
				{
					// make sure we can still see if we crouch
					trace_t result;

					Vector origin = GetCentroid( me );
					if (!me->IsCrouching())
					{
						// we are standing, adjust for lower crouch origin
						origin.z -= 20.0f;
					}

					UTIL_TraceLine( origin, enemy->EyePosition(), MASK_PLAYERSOLID, me, COLLISION_GROUP_NONE, &result );

					if (result.fraction == 1.0f)
					{
						m_crouchAndHold = true;
					}
				}
			}
		}

		if (m_crouchAndHold)
		{
			me->Crouch();
			me->PrintIfWatched( "Crouch and hold attack!\n" ); 
		}
	}

	m_scopeTimestamp = 0;
	m_didAmbushCheck = false;

	float skill = me->GetProfile()->GetSkill();

	// tendency to dodge is proportional to skill
	float dodgeChance = 80.0f * skill;

	// high skill bots always dodge if outnumbered, or they see a sniper
	if (skill > 0.5f && (me->IsOutnumbered() || me->CanSeeSniper()))
	{
		dodgeChance = 100.0f;
	}

	m_shouldDodge = (RandomFloat( 0, 100 ) <= dodgeChance);

	// decide whether we might bail out of this fight
	m_isCoward = (RandomFloat( 0, 100 ) > 100.0f * me->GetProfile()->GetAggression());

	// HEAVY BOTS: If we have heavy armor, attack differently
	if ( me->HasHeavyArmor() )
	{
		m_isCoward = m_shouldDodge = false;
		me->Walk();
		m_pinnedDownTimestamp = gpGlobals->curtime + 60*60;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * When we are done attacking, this is invoked
 */
void AttackState::StopAttacking( CCSBot *me )
{
	if (me->GetTask() == CCSBot::SNIPING)
	{
		// stay in our hiding spot
		me->Hide( me->GetLastKnownArea(), -1.0f, 50.0f );
	}
	else
	{
		me->StopAttacking();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do dodge behavior
 */
void AttackState::Dodge( CCSBot *me )
{
	//
	// Dodge.
	// If sniping or crouching, stand still.
	//
	if (m_shouldDodge && !me->IsUsingSniperRifle() && !m_crouchAndHold)
	{
		CBasePlayer *enemy = me->GetBotEnemy();
		if (enemy == nullptr)
		{
			return;
		}

		Vector toEnemy = enemy->GetAbsOrigin() - me->GetAbsOrigin();
		float range = toEnemy.Length();

		float minRange = me->GetCombatRange() - hysterisRange;
		float maxRange = me->GetCombatRange() + hysterisRange;

		if (me->IsUsingKnife())
		{
			// dodge when far away if armed only with a knife
			maxRange = 999999.9f;
		}

		// move towards (or away from) enemy if we are using a knife, behind a corner, or we aren't very skilled
		if (me->GetProfile()->GetSkill() < 0.66f || !me->IsEnemyVisible())
		{
			if (range > maxRange)
				me->MoveForward();
			else if (range < minRange)
				me->MoveBackward();
		}

		// don't dodge if enemy is facing away
		if (!me->CanSeeSniper() && (range > dodgeRange || !me->IsPlayerFacingMe( enemy )))
		{
			m_dodgeState = STEADY_ON;
			m_nextDodgeStateTimestamp = 0.0f;
		}
		else if (gpGlobals->curtime >= m_nextDodgeStateTimestamp)
		{
			int next;

			// high-skill bots keep moving and don't jump if they see a sniper
			if (me->GetProfile()->GetSkill() > 0.5f && me->CanSeeSniper())
			{
				// juke back and forth
				if (m_firstDodge)
				{
					next = (RandomInt( 0, 100 ) < 50) ? SLIDE_RIGHT : SLIDE_LEFT;
				}
				else
				{
					next = (m_dodgeState == SLIDE_LEFT) ? SLIDE_RIGHT : SLIDE_LEFT;
				}
			}
			else
			{
				// select next dodge state that is different that our current one
				do
				{
					// low-skill bots may jump when first engaging the enemy (if they are moving)
					if (m_firstDodge && me->GetProfile()->GetSkill() < 0.5f && RandomFloat( 0, 100 ) < jumpChance && !me->IsNotMoving())
						next = RandomInt( 0, NUM_ATTACK_STATES-1 );
					else
						next = RandomInt( 0, NUM_ATTACK_STATES-2 );
				}
				while( !m_firstDodge && next == m_dodgeState );
			}

			m_dodgeState = (DodgeStateType)next;
			m_nextDodgeStateTimestamp = gpGlobals->curtime + RandomFloat( 0.3f, 1.0f );
			m_firstDodge = false;
		}

		Vector forward, right;
		me->EyeVectors( &forward, &right );

		float ground;

		switch( m_dodgeState )
		{
			case STEADY_ON:
			{
				break;
			}

			case SLIDE_LEFT:
			{
				// don't move left if we will fall
				Vector pos = me->GetAbsOrigin() - (lookAheadRange * right);

				if (me->GetSimpleGroundHeightWithFloor( pos, &ground ))
				{
					if (me->GetAbsOrigin().z - ground < StepHeight)
					{
						me->StrafeLeft();
					}
				}
				break;
			}

			case SLIDE_RIGHT:
			{
				// don't move left if we will fall
				Vector pos = me->GetAbsOrigin() + (lookAheadRange * right);

				if (me->GetSimpleGroundHeightWithFloor( pos, &ground ))
				{
					if (me->GetAbsOrigin().z - ground < StepHeight)
					{
						me->StrafeRight();
					}
				}
				break;
			}

			case JUMP:
			{
				if (me->m_isEnemyVisible)
				{
					me->Jump();
				}
				break;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Perform attack behavior
 */
void AttackState::OnUpdate( CCSBot *me )
{
	// can't be stuck while attacking
	me->ResetStuckMonitor();

	// if we somehow ended up with the C4 or a grenade in our hands, grab our weapon!
	CWeaponCSBase *weapon = me->GetActiveCSWeapon();
	if (weapon && !CSGameRules()->IsPlayingCoopMission())
	{
		if (weapon->GetCSWeaponID() == WEAPON_C4 || 
			weapon->GetCSWeaponID() == WEAPON_HEGRENADE || 
			weapon->GetCSWeaponID() == WEAPON_FLASHBANG ||
			weapon->GetCSWeaponID() == WEAPON_SMOKEGRENADE ||
			weapon->GetCSWeaponID() == WEAPON_MOLOTOV ||
			weapon->GetCSWeaponID() == WEAPON_INCGRENADE ||
			weapon->GetCSWeaponID() == WEAPON_DECOY ||
			weapon->GetCSWeaponID() == WEAPON_TAGRENADE )
		{
			me->EquipBestWeapon();
		}
	}

	CBasePlayer *enemy = me->GetBotEnemy();
	if (enemy == nullptr)
	{
		StopAttacking( me );
		return;
	}

	Vector myOrigin = GetCentroid( me );
	Vector enemyOrigin = GetCentroid( enemy );

	// keep track of whether we have seen our enemy at least once yet
	if (!m_haveSeenEnemy)
		m_haveSeenEnemy = me->IsEnemyVisible();

	//
	// Retreat check
	// Do not retreat if the enemy is too close
	//
	if (m_retreatTimer.IsElapsed())
	{
		// If we've been fighting this battle for awhile, we're "pinned down" and
		// need to do something else.
		// If we are outnumbered, retreat.
		// If we see a sniper and we aren't a sniper, retreat.

		bool isPinnedDown = (gpGlobals->curtime > m_pinnedDownTimestamp);

		if (isPinnedDown || 
			(me->CanSeeSniper() && !me->IsSniper()) ||
			(me->IsOutnumbered() && m_isCoward) || 
			(me->OutnumberedCount() >= 2 && me->GetProfile()->GetAggression() < 1.0f))
		{
			// only retreat if at least one of them is aiming at me
			if (me->IsAnyVisibleEnemyLookingAtMe( true ))
			{
				// tell our teammates our plight
				if (isPinnedDown)
					me->GetChatter()->PinnedDown();
				else if (!me->CanSeeSniper())
					me->GetChatter()->Scared();

				m_retreatTimer.Start( RandomFloat( 3.0f, 15.0f ) );

				// try to retreat
				if (me->TryToRetreat())
				{
					// if we are a sniper, equip our pistol so we can fire while retreating
					/*
					if (me->IsUsingSniperRifle())
					{
						// wait a moment to allow one last shot
						me->Wait( 0.5f );
						//me->EquipPistol();
					}
					*/

					// request backup if outnumbered
					if (me->IsOutnumbered())
					{
						me->GetChatter()->NeedBackup();
					}
				}
				else	
				{
					me->PrintIfWatched( "I want to retreat, but no safe spots nearby!\n" );
				}
			}
		}
	}

	//
	// Knife fighting
	// We need to pathfind right to the enemy to cut him
	//
	if (me->IsUsingKnife())
	{
		// can't crouch and hold with a knife
		m_crouchAndHold = false;
		me->StandUp();

		// if we are using a knife and our prey is looking towards us, run at him
		if (me->IsPlayerFacingMe( enemy ))
		{
			me->ForceRun( 5.0f );
			me->Hurry( 10.0f );
		}

		// slash our victim
		me->FireWeaponAtEnemy();

		// if toe to toe with our enemy, don't dodge, just slash
		if ((enemy->GetAbsOrigin() - me->GetAbsOrigin()).IsLengthGreaterThan( slashRange ))
		{
			// if our victim has moved, repath
			bool repath = false;
			if (me->HasPath())
			{
				if ((me->GetPathEndpoint() - enemy->GetAbsOrigin()).IsLengthGreaterThan( repathRange ))
				{
					repath = true;
				}
			}
			else
			{
				repath = true;
			}

			if (repath && m_repathTimer.IsElapsed())
			{
				Vector enemyPos = enemy->GetAbsOrigin() + Vector( 0, 0, HalfHumanHeight );
				me->ComputePath( enemyPos, FASTEST_ROUTE );
				m_repathTimer.Start( repathInterval );
			}

			// move towards victim
			if (me->UpdatePathMovement( false ) != CCSBot::PROGRESSING)
			{
				me->DestroyPath();
			}
		}

		return;
	}

	//
	// Simple shield usage
	//
	if (me->HasShield())
	{
		if (me->IsEnemyVisible() && !m_shieldForceOpen)
		{
			if (!me->IsRecognizedEnemyReloading() && !me->IsReloading() && me->IsPlayerLookingAtMe( enemy ))
			{
				// close up - enemy is pointing his gun at us
				if (!me->IsProtectedByShield())
					me->SecondaryAttack();
			}
			else
			{
				// enemy looking away or reloading his weapon - open up and shoot him
				if (me->IsProtectedByShield())
					me->SecondaryAttack();
			}
		}
		else
		{
			// can't see enemy, open up
			if (me->IsProtectedByShield())
				me->SecondaryAttack();
		}

		if (gpGlobals->curtime > m_shieldToggleTimestamp)
		{
			m_shieldToggleTimestamp = gpGlobals->curtime + RandomFloat( 0.5, 2.0f );

			// toggle shield force open
			m_shieldForceOpen = !m_shieldForceOpen;
		}
	}

	// check if our weapon range is bad and we should switch to pistol
	if (me->IsUsingSniperRifle())
	{
		// if we have a sniper rifle and our enemy is too close, switch to pistol
		if ((enemyOrigin - myOrigin).IsLengthLessThan( sniperMinRange ))
			me->EquipPistol();
	}
	else if (me->IsUsingShotgun())
	{
		// if we have a shotgun equipped and enemy is too far away, switch to pistol
		if ((enemyOrigin - myOrigin).IsLengthGreaterThan( shotgunMaxRange ))
			me->EquipPistol();
	}

	// if we're sniping, look through the scope - need to do this here in case a reload resets our scope
	if (me->IsUsingSniperRifle())
	{
		// for Scouts and AWPs, we need to wait for zoom to resume
		if (me->m_bResumeZoom)
		{
			m_scopeTimestamp = gpGlobals->curtime;
			return;
		}

		Vector toAimSpot3D = me->m_targetSpot - myOrigin;
		float targetRange = toAimSpot3D.Length();

		// dont adjust zoom level if we're already zoomed in - just fire
		if (me->GetZoomLevel() == CCSBot::NO_ZOOM && me->AdjustZoom( targetRange ))
			m_scopeTimestamp = gpGlobals->curtime;
	
		const float waitScopeTime = 0.3f + me->GetProfile()->GetReactionTime();
		if (gpGlobals->curtime - m_scopeTimestamp < waitScopeTime)
		{
			// force us to wait until zoomed in before firing
			return;
		}
	}

	// see if we "notice" that our prey is dead
	if (me->IsAwareOfEnemyDeath())
	{
		// let team know if we killed the last enemy
		if (me->GetLastVictimID() == enemy->entindex() && me->GetNearbyEnemyCount() <= 1)
		{
			me->GetChatter()->KilledMyEnemy( enemy->entindex() );

			// if there are other enemies left, wait a moment - they usually come in groups
			if (me->GetEnemiesRemaining())
			{
				me->Wait( RandomFloat( 1.0f, 3.0f ) );
			}
		}

		StopAttacking( me );
		return;
	}

	float notSeenEnemyTime = gpGlobals->curtime - me->GetLastSawEnemyTimestamp();

	// if we haven't seen our enemy for a moment, continue on if we dont want to fight, or decide to ambush if we do
	if (!me->IsEnemyVisible())
	{
		// attend to nearby enemy gunfire
		if (notSeenEnemyTime > 0.5f && me->CanHearNearbyEnemyGunfire())
		{
			// give up the attack, since we didn't want it in the first place
			StopAttacking( me );

			const Vector *pos = me->GetNoisePosition();
			if (pos)
			{
				me->SetLookAt( "Nearby enemy gunfire", *pos, PRIORITY_HIGH, 0.0f );
				me->PrintIfWatched( "Checking nearby threatening enemy gunfire!\n" );
				return;
			}
		}

		// check if we have lost track of our enemy during combat
		if (notSeenEnemyTime > 0.25f)
		{
			m_isEnemyHidden = true;
		}
					
		if (notSeenEnemyTime > 0.1f)
		{
			if (me->GetDisposition() == CCSBot::ENGAGE_AND_INVESTIGATE)
			{
				// decide whether we should hide and "ambush" our enemy
				if (m_haveSeenEnemy && !m_didAmbushCheck)
				{
					float hideChance = 33.3f;

					if (RandomFloat( 0.0, 100.0f ) < hideChance)
					{
						float ambushTime = RandomFloat( 3.0f, 15.0f );

						// hide in ambush nearby
						/// @todo look towards where we know enemy is
						const Vector *spot = FindNearbyRetreatSpot( me, 200.0f );
						if (spot)
						{
							me->IgnoreEnemies( 1.0f );

							me->Run();
							me->StandUp();
							me->Hide( *spot, ambushTime, true );
							return;
						}
					}

					// don't check again
					m_didAmbushCheck = true;
				}
			}
			else
			{
				// give up the attack, since we didn't want it in the first place
				StopAttacking( me );
				return;
			}
		}
	}
	else
	{
		// we can see the enemy again - reset our ambush check
		m_didAmbushCheck = false;

		// if the enemy is coming out of hiding, we need time to react
		if (m_isEnemyHidden)
		{
			m_reacquireTimestamp = gpGlobals->curtime + me->GetProfile()->GetReactionTime();
			m_isEnemyHidden = false;
		}
	}

	// if we haven't seen our enemy for a long time, chase after them
	float chaseTime = 2.0f + 2.0f * (1.0f - me->GetProfile()->GetAggression());

	// if we are sniping, be very patient
	if (me->IsUsingSniperRifle())
		chaseTime += 3.0f;
	else if (me->IsCrouching())	// if we are crouching, be a little patient
		chaseTime += 1.0f;

	// if we can't see the enemy, and have either seen him but currently lost sight of him, 
	// or haven't yet seen him, chase after him (unless we are a sniper)
	if (!me->IsEnemyVisible() && (notSeenEnemyTime > chaseTime || !m_haveSeenEnemy))
	{
		// snipers don't chase their prey - they wait for their prey to come to them
		if ( CSGameRules()->IsPlayingCoopGuardian() == false && me->GetTask() == CCSBot::SNIPING  )
		{
			StopAttacking( me );
			return;
		}
		else
		{
			// move to last known position of enemy
			me->SetTask( CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION, enemy );
			me->MoveTo( me->GetLastKnownEnemyPosition() );
			return;
		}
	}

	// if we can't see our enemy at the moment, and were shot by
	// a different visible enemy, engage them instead
	const float hurtRecentlyTime = 3.0f;
	if (!me->IsEnemyVisible() &&
		me->GetTimeSinceAttacked() < hurtRecentlyTime &&
		me->GetAttacker() &&
		me->GetAttacker() != me->GetBotEnemy())
	{
		// if we can see them, attack, otherwise panic
		if (me->IsVisible( me->GetAttacker(), true ))
		{
			me->Attack( me->GetAttacker() );
			me->PrintIfWatched( "Switching targets to retaliate against new attacker!\n" );
		}
		/*
		 * Rethink this
		else
		{
			me->Panic( me->GetAttacker() );
			me->PrintIfWatched( "Panicking from crossfire while attacking!\n" );
		}
		*/

		return;
	}

		me->FireWeaponAtEnemy();

	// do dodge behavior
	Dodge( me );
	
	if ( me->HasHeavyArmor() && me->GetBotEnemy() && me->IsEnemyVisible() )
	{
		me->MoveForward();
		me->GetChatter()->DoPhoenixHeavyWakeTaunt();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Finish attack
 */
void AttackState::OnExit( CCSBot *me )
{
	me->PrintIfWatched( "AttackState:OnExit()\n" );

	m_crouchAndHold = false;

	// clear any noises we heard during battle
	me->ForgetNoise();
	me->ResetStuckMonitor();

	// resume our original posture
	me->PopPostureContext();

	// put shield away
	if (me->IsProtectedByShield())
		me->SecondaryAttack();

	//me->StopAiming();
}

class CBaseCombatWeapon;
class CCSBot;
class CCSWeaponInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
ConVar bot_loadout( side_registry(), "bot_loadout", "", FCVAR_CHEAT, "bots are given these items at round start" );
ConVar bot_randombuy( side_registry(), "bot_randombuy", "0", FCVAR_CHEAT, "should bots ignore their prefered weapons and just buy weapons at random?" );
ConVar bot_gungameselect_weapons_t( side_registry(), "bot_gungameselect_weapons_t", "deagle awp p90 ak47 sg556", 0, "the list of weapons that T bots start with in gun game select" );
ConVar bot_gungameselect_weapons_ct( side_registry(), "bot_gungameselect_weapons_ct", "deagle awp p90 aug m4a1", 0, "the list of weapons that CT bots start with in gun game select" );
ConVar sv_bot_buy_grenade_chance( side_registry(), "sv_bot_buy_grenade_chance", "33", FCVAR_RELEASE | FCVAR_GAMEDLL, "Chance bots will buy a grenade with leftover money (after prim, sec and armor). Input as percent (0-100.0)", true, 0.0f, true, 100.0f );

ConVar sv_bot_buy_smoke_weight		( side_registry(), "sv_bot_buy_smoke_weight", "1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Given a bot will buy a grenade, controls the odds of the grenade type. Proportional to all other sv_bot_buy_*_weight convars.", true, 0.0f, false, 0.0f );
ConVar sv_bot_buy_flash_weight		( side_registry(), "sv_bot_buy_flash_weight", "1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Given a bot will buy a grenade, controls the odds of the grenade type. Proportional to all other sv_bot_buy_*_weight convars.", true, 0.0f, false, 0.0f );
ConVar sv_bot_buy_decoy_weight		( side_registry(), "sv_bot_buy_decoy_weight", "1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Given a bot will buy a grenade, controls the odds of the grenade type. Proportional to all other sv_bot_buy_*_weight convars.", true, 0.0f, false, 0.0f );
ConVar sv_bot_buy_molotov_weight	( side_registry(), "sv_bot_buy_molotov_weight", "1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Given a bot will buy a grenade, controls the odds of the grenade type. Proportional to all other sv_bot_buy_*_weight convars.", true, 0.0f, false, 0.0f );
ConVar sv_bot_buy_hegrenade_weight	( side_registry(), "sv_bot_buy_hegrenade_weight", "6", FCVAR_RELEASE | FCVAR_GAMEDLL, "Given a bot will buy a grenade, controls the odds of the grenade type. Proportional to all other sv_bot_buy_*_weight convars.", true, 0.0f, false, 0.0f );

struct { const ConVar *cv; const char* szName; } g_GrenadeWeights[] =
{
	{ &sv_bot_buy_smoke_weight, "smokegrenade" },
	{ &sv_bot_buy_flash_weight, "flashbang" },
	{ &sv_bot_buy_decoy_weight, "decoy" },
	{ &sv_bot_buy_molotov_weight, "molotov" },
	{ &sv_bot_buy_hegrenade_weight, "hegrenade" },
};

// Pick a grenade for bots to buy based on set weights
const char* Helper_PickBotGrenade()
{
	int iNumGrenades = std::size( g_GrenadeWeights );
	float flGrenadeWeight = 0.0f;
	for ( int i = 0; i < iNumGrenades; ++i )
		flGrenadeWeight += g_GrenadeWeights[ i ].cv->GetFloat();

	float flRand = RandomFloat( 0.0f + std::numeric_limits<float>::epsilon() , flGrenadeWeight );
	float flAccumulator = 0.0f;
	for ( int i = 0; i < iNumGrenades; ++i )
	{
		flAccumulator += g_GrenadeWeights[ i ].cv->GetFloat();
		if ( flRand <= flAccumulator )
		{
			return  g_GrenadeWeights[ i ].szName;
		}
	}
	
	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Debug command to give a named weapon
 */
void CCSBot::GiveWeapon( const char *weaponAlias )
{
	const char *translatedAlias = GetTranslatedWeaponAlias( weaponAlias );

	char wpnName[128];
	V_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", translatedAlias );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return;
	}

	CCSWeaponInfo *pWeaponInfo = static_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	if ( !pWeaponInfo )
	{
		return;
	}

	if ( !Weapon_OwnsThisType( wpnName ) )
	{
		CBaseCombatWeapon *pWeapon = Weapon_GetSlot( pWeaponInfo->iSlot );
		if ( pWeapon )
		{
			if ( pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL )
			{
				DropWeaponSlot( WEAPON_SLOT_PISTOL );
			}
			else if ( pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE )
			{
				DropWeaponSlot( WEAPON_SLOT_RIFLE );
			}
		}
	}

	GiveNamedItem( wpnName );
}

//--------------------------------------------------------------------------------------------------------------
static bool HasDefaultPistol( CCSBot *me )
{
	CWeaponCSBase *pistol = (CWeaponCSBase *)me->Weapon_GetSlot( WEAPON_SLOT_PISTOL );

	if (pistol == nullptr)
		return false;

	if (me->GetTeamNumber() == TEAM_TERRORIST && pistol->IsA( WEAPON_GLOCK ))
		return true;

	if (me->GetTeamNumber() == TEAM_CT && pistol->IsA( WEAPON_HKP2000 ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Buy weapons, armor, etc.
 */
void BuyState::OnEnter( CCSBot *me )
{
	m_retries = 0;
	m_prefRetries = 0;
	m_prefIndex = 0;

	const char *cheatWeaponString = bot_loadout.GetString();
	if ( cheatWeaponString && *cheatWeaponString )
	{
		m_doneBuying = false; // we're going to be given weapons - ignore the eco limit
	}
	else
	{
		// check if we are saving money for the next round
		if (me->GetAccountBalance() < cv_bot_eco_limit.GetFloat())
		{
			me->PrintIfWatched( "Saving money for next round.\n" );
			m_doneBuying = true;
		}
		else
		{
			m_doneBuying = false;
		}
	}

	m_isInitialDelay = true;

	// this will force us to stop holding live grenade
	me->EquipBestWeapon( true );

	m_buyShield = false;

	if (me->GetTeamNumber() == TEAM_CT)
	{
		// determine if we want a tactical shield
		if (!me->HasPrimaryWeapon() && TheCSBots()->AllowTacticalShield())
		{
			if (me->GetAccountBalance() > 2500)
			{
				if (me->GetAccountBalance() < 4000)
					m_buyShield = (RandomFloat( 0, 100.0f ) < 33.3f) ? true : false;
				else
					m_buyShield = (RandomFloat( 0, 100.0f ) < 10.0f) ? true : false;
			}
		}
	}

	if (TheCSBots()->AllowGrenades())
	{
		m_buyGrenade = (RandomFloat( 0.0f, 100.0f ) < sv_bot_buy_grenade_chance.GetFloat() ) ? true : false;
	}
	else
	{
		m_buyGrenade = false;
	}

	m_buyPistol = false;
	if (TheCSBots()->AllowPistols())
	{
		// check if we have a pistol
		if (me->Weapon_GetSlot( WEAPON_SLOT_PISTOL ))
		{
			// if we have our default pistol, think about buying a different one
			if (HasDefaultPistol( me ))
			{
				// if everything other than pistols is disallowed, buy a pistol
				if (TheCSBots()->AllowShotguns() == false &&
					TheCSBots()->AllowSubMachineGuns() == false &&
					TheCSBots()->AllowRifles() == false &&
					TheCSBots()->AllowMachineGuns() == false &&
					TheCSBots()->AllowTacticalShield() == false &&
					TheCSBots()->AllowSnipers() == false)
				{
					m_buyPistol = (RandomFloat( 0, 100 ) < 75.0f);
				}
				else if (me->GetAccountBalance() < 1000)
				{
					// if we're low on cash, buy a pistol
					m_buyPistol = (RandomFloat( 0, 100 ) < 75.0f);
				}
				else
				{
					m_buyPistol = (RandomFloat( 0, 100 ) < 33.3f);
				}
			}			
		}
		else
		{
			// we dont have a pistol - buy one
			m_buyPistol = true;
		}
	}
}

enum WeaponType
{
	PISTOL,
	SHOTGUN,
	SUB_MACHINE_GUN,
	RIFLE,
	MACHINE_GUN,
	SNIPER_RIFLE,
	GRENADE,

	NUM_WEAPON_TYPES,
	INVALID_WEAPON_TYPE = NUM_WEAPON_TYPES
};

struct BuyInfo
{
	WeaponType type;
	bool preferred;			///< more challenging bots prefer these weapons
	const char *buyAlias;   ///< the buy alias for this equipment
};

static constexpr auto PRIMARY_WEAPON_BUY_COUNT = 16;
static constexpr auto SECONDARY_WEAPON_BUY_COUNT = 3;

/**
 * These tables MUST be kept in sync with the CT and T buy aliases
 */

static BuyInfo primaryWeaponBuyInfoCT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "nova" },		
	{ SHOTGUN,			false, "xm1014" },
	{ SHOTGUN,			false, "mag7" },
	{ SUB_MACHINE_GUN,	false, "mp9" },	
	{ SUB_MACHINE_GUN,	false, "bizon" },	
	{ SUB_MACHINE_GUN,	false, "mp7" },			
	{ SUB_MACHINE_GUN,	false, "ump45" },		
	{ SUB_MACHINE_GUN,	false, "p90" },			
	{ RIFLE,			true,  "famas" },		
	{ SNIPER_RIFLE,		false, "ssg08" },		
	{ RIFLE,			true,  "m4a1" },		
	{ RIFLE,			true, "aug" },			
	{ SNIPER_RIFLE,		true,  "scar20" },		
	{ SNIPER_RIFLE,		true,  "awp" },			
	{ MACHINE_GUN,		false, "m249" },
	{ MACHINE_GUN,		false, "negev" }	
};

static BuyInfo secondaryWeaponBuyInfoCT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p250" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"fiveseven" }
};

static BuyInfo primaryWeaponBuyInfoT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "nova" },		
	{ SHOTGUN,			false, "xm1014" },
	{ SHOTGUN,			false, "sawedoff" },
	{ SUB_MACHINE_GUN,	false, "mac10" },			
	{ SUB_MACHINE_GUN,	false, "ump45" },		
	{ SUB_MACHINE_GUN,	false, "p90" },			
	{ RIFLE,			true,  "galilar" },		
	{ RIFLE,			true,  "ak47" },		
	{ SNIPER_RIFLE,		false, "ssg08" },		
	{ RIFLE,			true,  "sg556" },		
	{ SNIPER_RIFLE,		true,  "awp" },			
	{ SNIPER_RIFLE,		true,  "g3sg1" },		
	{ MACHINE_GUN,		false, "m249" },
	{ MACHINE_GUN,		false, "negev" },
	{ INVALID_WEAPON_TYPE,	false, "" },	
	{ INVALID_WEAPON_TYPE,	false, "" }
};

static BuyInfo secondaryWeaponBuyInfoT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p250" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"elites" }
};

/**
 * Given a weapon alias, return the kind of weapon it is
 */
inline WeaponType GetWeaponType( const char *alias )
{
	int i;
	
	for( i=0; i<PRIMARY_WEAPON_BUY_COUNT; ++i )
	{
		if (!stricmp( alias, primaryWeaponBuyInfoCT[i].buyAlias ))
			return primaryWeaponBuyInfoCT[i].type;

		if (!stricmp( alias, primaryWeaponBuyInfoT[i].buyAlias ))
			return primaryWeaponBuyInfoT[i].type;
	}

	for( i=0; i<SECONDARY_WEAPON_BUY_COUNT; ++i )
	{
		if (!stricmp( alias, secondaryWeaponBuyInfoCT[i].buyAlias ))
			return secondaryWeaponBuyInfoCT[i].type;

		if (!stricmp( alias, secondaryWeaponBuyInfoT[i].buyAlias ))
			return secondaryWeaponBuyInfoT[i].type;
	}

	return INVALID_WEAPON_TYPE;
}

//--------------------------------------------------------------------------------------------------------------
void BuyState::OnUpdate( CCSBot *me )
{
	char cmdBuffer[256];

	// wait for a Navigation Mesh
	if (!TheNavMesh->IsLoaded())
		return;

	// apparently we cant buy things in the first few seconds, so wait a bit
	if (m_isInitialDelay)
	{
		const float waitToBuyTime = 0.25f;
		if (gpGlobals->curtime - me->GetStateTimestamp() < waitToBuyTime)
			return;

		m_isInitialDelay = false;
	}

	// if we're done buying and still in the freeze period, wait
	if (m_doneBuying)
	{
		if (CSGameRules()->IsMultiplayer() && CSGameRules()->IsFreezePeriod())
		{
			// make sure we're locked and loaded
			me->EquipBestWeapon( true );
			me->Reload();
			me->ResetStuckMonitor();
			return;
		}

		me->Idle();
		return;
	}

	// If we're supposed to buy a specific weapon for debugging, do so and then bail
	const char *cheatWeaponString = bot_loadout.GetString();
	if ( cheatWeaponString && *cheatWeaponString )
	{
		std::vector< std::string > loadout;
		V_SplitString( cheatWeaponString, " ", loadout );
		for ( int i=0; i<std::ssize( loadout ); ++i )
		{
			const char *item = loadout[i].c_str();
			if ( FStrEq( item, "vest" ) )
			{
				me->GiveNamedItem( "item_kevlar" );
			}
			else if ( FStrEq( item, "vesthelm" ) )
			{
				me->GiveNamedItem( "item_assaultsuit" );
			}
			else if ( FStrEq( item, "defuser" ) )
			{
				if ( me->GetTeamNumber() == TEAM_CT )
				{
					me->GiveDefuser();
				}
			}
			else if ( FStrEq( item, "nvgs" ) )
			{
				me->m_bHasNightVision = true;
			}
			else if ( FStrEq( item, "primammo" ) )
			{
				me->AttemptToBuyAmmo( 0 );
			}
			else if ( FStrEq( item, "secammo" ) )
			{
				me->AttemptToBuyAmmo( 1 );
			}
			else
			{
				me->GiveWeapon( item );
			}
		}
		m_doneBuying = true;
		return;
	}

	if (!me->IsInBuyZone())
	{
		m_doneBuying = true;
        Msg( "%s bot spawned outside of a buy zone (%d, %d, %d)\n",
						(me->GetTeamNumber() == TEAM_CT) ? "CT" : "Terrorist",
						(int)me->GetAbsOrigin().x,
						(int)me->GetAbsOrigin().y,
						(int)me->GetAbsOrigin().z );
		return;
	}

	// try to buy some weapons
	const float buyInterval = 0.02f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > buyInterval)
	{
		me->m_stateTimestamp = gpGlobals->curtime;

		bool isPreferredAllDisallowed = true;

		// try to buy our preferred weapons first
		if (m_prefIndex < me->GetProfile()->GetWeaponPreferenceCount() && bot_randombuy.GetBool() == false )
		{
			// need to retry because sometimes first buy fails??
			const int maxPrefRetries = 2;
			if (m_prefRetries >= maxPrefRetries)
			{
				// try to buy next preferred weapon
				++m_prefIndex;
				m_prefRetries = 0;
				return;
			}

			int weaponPreference = me->GetProfile()->GetWeaponPreference( m_prefIndex );

			// don't buy it again if we still have one from last round
			char weaponPreferenceName[32];
			V_snprintf( weaponPreferenceName, sizeof(weaponPreferenceName), "weapon_%s", me->GetProfile()->GetWeaponPreferenceAsString( m_prefIndex ) );
			if( me->Weapon_OwnsThisType(weaponPreferenceName) )//Prefs and buyalias use the short version, this uses the long
			{
				// done with buying preferred weapon
				m_prefIndex = 9999;
				return;
			}

			const char *buyAlias = nullptr;

			buyAlias = WeaponIDToAlias( weaponPreference );
			WeaponType type = GetWeaponType( buyAlias );
			switch( type )
			{
			case PISTOL:
				if (!TheCSBots()->AllowPistols())
					buyAlias = nullptr;
				break;

			case SHOTGUN:
				if (!TheCSBots()->AllowShotguns())
					buyAlias = nullptr;
				break;

			case SUB_MACHINE_GUN:
				if (!TheCSBots()->AllowSubMachineGuns())
					buyAlias = nullptr;
				break;

			case RIFLE:
				if (!TheCSBots()->AllowRifles())
					buyAlias = nullptr;
				break;

			case MACHINE_GUN:
				if (!TheCSBots()->AllowMachineGuns())
					buyAlias = nullptr;
				break;

			case SNIPER_RIFLE:
				if (!TheCSBots()->AllowSnipers())
					buyAlias = nullptr;
				break;
			}

			if (buyAlias)
			{
				V_snprintf( cmdBuffer, 256, "buy %s\n", buyAlias );

				CCommand args;
				args.Tokenize( cmdBuffer );
				me->ClientCommand( args );

				me->PrintIfWatched( "Tried to buy preferred weapon %s.\n", buyAlias );
				isPreferredAllDisallowed = false;
			}

			++m_prefRetries;

		}

		// if we have no preferred primary weapon (or everything we want is disallowed), buy at random
		if (!me->HasPrimaryWeapon() && (isPreferredAllDisallowed || !me->GetProfile()->HasPrimaryPreference()))
		{
			if (m_buyShield)
			{
				// buy a shield
				CCommand args;
				args.Tokenize( "buy shield" );
				me->ClientCommand( args );

				me->PrintIfWatched( "Tried to buy a shield.\n" );
			}
			else 
			{
				// build list of allowable weapons to buy
				BuyInfo *masterPrimary = (me->GetTeamNumber() == TEAM_TERRORIST) ? primaryWeaponBuyInfoT : primaryWeaponBuyInfoCT;
				BuyInfo *stockPrimary[ PRIMARY_WEAPON_BUY_COUNT ];
				int stockPrimaryCount = 0;

				// dont choose sniper rifles as often
				const float sniperRifleChance = 50.0f;
				bool wantSniper = (RandomFloat( 0, 100 ) < sniperRifleChance) ? true : false;

				if ( bot_randombuy.GetBool() )
				{
					wantSniper = true;
				}

				for( int i=0; i<PRIMARY_WEAPON_BUY_COUNT; ++i )
				{
					if ((masterPrimary[i].type == SHOTGUN && TheCSBots()->AllowShotguns()) ||
						(masterPrimary[i].type == SUB_MACHINE_GUN && TheCSBots()->AllowSubMachineGuns()) ||
						(masterPrimary[i].type == RIFLE && TheCSBots()->AllowRifles()) ||
						(masterPrimary[i].type == SNIPER_RIFLE && TheCSBots()->AllowSnipers() && wantSniper) ||
						(masterPrimary[i].type == MACHINE_GUN && TheCSBots()->AllowMachineGuns()))
					{
						stockPrimary[ stockPrimaryCount++ ] = &masterPrimary[i];
					}
				}
 
				if (stockPrimaryCount)
				{
					// buy primary weapon if we don't have one
					int which;

					// on hard difficulty levels, bots try to buy preferred weapons on the first pass
					if (m_retries == 0 && TheCSBots()->GetDifficultyLevel() >= BOT_HARD && bot_randombuy.GetBool() == false )
					{
						// count up available preferred weapons
						int prefCount = 0;
						for( which=0; which<stockPrimaryCount; ++which )
							if (stockPrimary[which]->preferred)
								++prefCount;

						if (prefCount)
						{
							int whichPref = RandomInt( 0, prefCount-1 );
							for( which=0; which<stockPrimaryCount; ++which )
								if (stockPrimary[which]->preferred && whichPref-- == 0)
									break;
						}
						else
						{
							// no preferred weapons available, just pick randomly
							which = RandomInt( 0, stockPrimaryCount-1 );
						}
					}
					else
					{
						which = RandomInt( 0, stockPrimaryCount-1 );
					}

					V_snprintf( cmdBuffer, 256, "buy %s\n", stockPrimary[ which ]->buyAlias );

					CCommand args;
					args.Tokenize( cmdBuffer );
					me->ClientCommand( args );

					me->PrintIfWatched( "Tried to buy %s.\n", stockPrimary[ which ]->buyAlias );
				}
			}
		}

		//
		// If we now have a weapon, or have tried for too long, we're done
		//
		if (me->HasPrimaryWeapon() || m_retries++ > 5)
		{
			// primary ammo
			CCommand args;
			if (me->HasPrimaryWeapon())
			{
				args.Tokenize( "buy primammo" );
				me->ClientCommand( args );
			}

			// buy armor last, to make sure we bought a weapon first
			args.Tokenize( "buy vesthelm" );
			me->ClientCommand( args );
			args.Tokenize( "buy vest" );
			me->ClientCommand( args );

			// pistols - if we have no preferred pistol, buy at random
			if (TheCSBots()->AllowPistols() && !me->GetProfile()->HasPistolPreference())
			{
				if (m_buyPistol)
				{
					int which = RandomInt( 0, SECONDARY_WEAPON_BUY_COUNT-1 );
					
					const char *what = nullptr;

					if (me->GetTeamNumber() == TEAM_TERRORIST)
						what = secondaryWeaponBuyInfoT[ which ].buyAlias;
					else
						what = secondaryWeaponBuyInfoCT[ which ].buyAlias;

					V_snprintf( cmdBuffer, 256, "buy %s\n", what );
					args.Tokenize( cmdBuffer );
					me->ClientCommand( args );

					// only buy one pistol
					m_buyPistol = false;
				}

				// make sure we have enough pistol ammo
				args.Tokenize( "buy secammo" );
				me->ClientCommand( args );
			}

			// buy a grenade if we wish, and we don't already have one
			if (m_buyGrenade && !me->HasGrenade())
			{
				const char *szGrenade = Helper_PickBotGrenade();
				if ( szGrenade )
					args.Tokenize( CFmtStr( "buy %s", szGrenade ).Access() );

				/*
				if (rnd < 10)//10% chance
				{
					args.Tokenize( "buy smokegrenade" );
				}
				else if (rnd < 20)//10% chance
				{
					// only allow Flashbangs if everyone on the team is a bot (dont want to blind our friendly humans)
					if (UTIL_IsTeamAllBots( me->GetTeamNumber() ))
					{
						args.Tokenize( "buy flashbang" );
					}
					else
					{
						args.Tokenize( "buy hegrenade" );
					}
				}
				else if (rnd < 30)//10% chance
				{
					args.Tokenize( "buy decoy" );
				}
				else if (rnd < 40)//10% chance
				{
					args.Tokenize( "buy molotov" );
				}
				else//60%-70% chance (because of the flashbang case for full team of bots).
				{
					args.Tokenize( "buy hegrenade" );
				}
				*/
				me->ClientCommand( args );
			}

			m_doneBuying = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BuyState::OnExit( CCSBot *me )
{
	me->ResetStuckMonitor();
	me->EquipBestWeapon();
}

class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin defusing the bomb
 */
void DefuseBombState::OnEnter( CCSBot *me )
{
	me->SetDisposition( CCSBot::SELF_DEFENSE );
	me->GetChatter()->Say( "DefusingBomb" );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Defuse the bomb
 */
void DefuseBombState::OnUpdate( CCSBot *me )
{
	const Vector *bombPos = me->GetGameState()->GetBombPosition();

	// stay in SELF_DEFENSE so we get to the bomb in time!
	me->SetDisposition( CCSBot::SELF_DEFENSE );

	if (bombPos == nullptr)
	{
		me->PrintIfWatched( "In Defuse state, but don't know where the bomb is!\n" );
		me->Idle();
		return;
	}

	// look at the bomb
	me->SetLookAt( "Defuse bomb", *bombPos, PRIORITY_HIGH );

	// defuse...
	me->UseEnvironment();

	if (gpGlobals->curtime - me->GetStateTimestamp() > 1.0f)
	{
		// if we missed starting the defuse, give up
		if (TheCSBots()->GetBombDefuser() == nullptr)
		{
			me->PrintIfWatched( "Failed to start defuse, giving up\n" );
			me->Idle();
			return;
		}
		else if (TheCSBots()->GetBombDefuser() != me)
		{
			// if someone else got the defuse, give up
			me->PrintIfWatched( "Someone else started defusing, giving up\n" );
			me->Idle();
			return;
		}
	}

	// if bomb has been defused, give up
	if (!TheCSBots()->IsBombPlanted())
	{
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void DefuseBombState::OnExit( CCSBot *me )
{
	me->ResetStuckMonitor();
	me->SetTask( CCSBot::SEEK_AND_DESTROY );
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->ClearLookAt();
}

class CCSBot;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->DestroyPath();
	me->EquipKnife();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnUpdate( CCSBot *me )
{
	const Vector *bombPos = me->GetGameState()->GetBombPosition();

	// if we don't know where the bomb is, we shouldn't be in this state
	if (bombPos == nullptr)
	{
		me->Idle();
		return;
	}

	// grab our knife to move quickly
	me->EquipKnife();

	// look around
	me->UpdateLookAround();

	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		// we have no path, or reached the end of one - create a new path far away from the bomb
		FarAwayFromPositionFunctor func( *bombPos );
		CNavArea *goalArea = FindMinimumCostArea( me->GetLastKnownArea(), func );

		// if this fails, we'll try again next time
		me->ComputePath( goalArea->GetCenter(), FASTEST_ROUTE );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnExit( CCSBot *me )
{
	me->EquipBestWeapon();
}

class CCSBot;
class CNavArea;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from flames we're standing in!
 */
void EscapeFromFlamesState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->StopWaiting();
	me->DestroyPath();
	me->EquipKnife();

	m_safeArea = nullptr;
	m_searchTimer.Invalidate();
}

//--------------------------------------------------------------------------------------------------------------
class CNonDamagingScan : public ISearchSurroundingAreasFunctor
{
public:
	CNonDamagingScan( void )
	{
		m_safeArea = nullptr;
		m_safeAreaTravelRange = std::numeric_limits<float>::max();
	}

	bool operator() ( CNavArea *area, CNavArea *priorArea, float travelDistanceSoFar )
	{
		const float maxSearchRange = 2000.0f;
		if ( travelDistanceSoFar < maxSearchRange )
		{
			if ( !area->IsDamaging() && travelDistanceSoFar < m_safeAreaTravelRange )
			{
				m_safeArea = area;
			}
		}

		return true;
	}

	CNavArea *m_safeArea;
	float m_safeAreaTravelRange;
};

//--------------------------------------------------------------------------------------------------------------
CNavArea *EscapeFromFlamesState::FindNearestNonDamagingArea( CCSBot *me ) const
{
	CNavArea *myArea = me->GetLastKnownArea();

	if ( !myArea )
	{
		return nullptr;
	}

	CNonDamagingScan scan;
	SearchSurroundingAreas( myArea, scan );

	return scan.m_safeArea;
}

//--------------------------------------------------------------------------------------------------------------
void EscapeFromFlamesState::OnUpdate( CCSBot *me )
{
	// default behavior if we're safe
	if ( me->GetTimeSinceBurnedByFlames() > 1.5f )
	{
		me->Idle();
		return;
	}

	if ( m_searchTimer.IsElapsed() )
	{
		// because flames grow and change, periodically re-search
		m_searchTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		m_safeArea = FindNearestNonDamagingArea( me );
	}

	// look around
	me->UpdateLookAround();

	me->EquipBestWeapon();
	me->FireWeaponAtEnemy();

	if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
	{
		if ( m_safeArea )
		{
			me->ComputePath( m_safeArea->GetCenter(), FASTEST_ROUTE );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void EscapeFromFlamesState::OnExit( CCSBot *me )
{
	me->EquipBestWeapon();
}

class CBaseEntity;
class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnEnter( CCSBot *me )
{
	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnUpdate( CCSBot *me )
{
	if (me->HasC4())
	{
		me->PrintIfWatched( "I picked up the bomb\n" );
		me->Idle();
		return;
	}

	CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
	if (bomb)
	{
		if (!me->HasPath())
		{
			// build a path to the bomb
			if (me->ComputePath( bomb->GetAbsOrigin() ) == false)
			{
				me->PrintIfWatched( "Fetch bomb pathfind failed\n" );

				// go Hunt instead of Idle to prevent continuous re-pathing to inaccessible bomb
				me->Hunt();
				return;
			}
		}
	}
	else
	{
		// someone picked up the bomb
		me->PrintIfWatched( "Someone else picked up the bomb.\n" );
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround();

	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
		me->Idle();
}

class CBasePlayer;
class CCSBot;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 */
void FollowState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->DestroyPath();

	m_isStopped = false;
	m_stoppedTimestamp = 0.0f;

	// to force immediate repath
	m_lastLeaderPos.x = -99999999.9f;
	m_lastLeaderPos.y = -99999999.9f;
	m_lastLeaderPos.z = -99999999.9f;

	m_lastSawLeaderTime = 0;

	// set re-pathing frequency
	m_repathInterval.Invalidate();

	m_isSneaking = false;

	m_walkTime.Invalidate();
	m_isAtWalkSpeed = false;

	m_leaderMotionState = INVALID;
	m_idleTimer.Start( RandomFloat( 2.0f, 5.0f ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine the leader's motion state by tracking his speed
 */
void FollowState::ComputeLeaderMotionState( float leaderSpeed )
{
	// walk = 130, run = 250
	const float runWalkThreshold = 140.0f;
	const float walkStopThreshold = 10.0f; // 120.0f;
	LeaderMotionStateType prevState = m_leaderMotionState;
	if (leaderSpeed > runWalkThreshold)
	{
		m_leaderMotionState = RUNNING;
		m_isAtWalkSpeed = false;
	}
	else if (leaderSpeed > walkStopThreshold)
	{
		// track when began to walk
		if (!m_isAtWalkSpeed)
		{
			m_walkTime.Start();
			m_isAtWalkSpeed = true;
		}

		const float minWalkTime = 0.25f;
		if (m_walkTime.GetElapsedTime() > minWalkTime)
		{
			m_leaderMotionState = WALKING;
		}
	}
	else
	{
		m_leaderMotionState = STOPPED;
		m_isAtWalkSpeed = false;
	}

	// track time spent in this motion state
	if (prevState != m_leaderMotionState)
	{
		m_leaderMotionStateTime.Start();
		m_waitTime = RandomFloat( 1.0f, 3.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor to collect all areas in the forward direction of the given player within a radius
 */
class FollowTargetCollector
{
public:
	FollowTargetCollector( CBasePlayer *player )
	{
		m_player = player;

		Vector playerVel = player->GetAbsVelocity();
		m_forward.x = playerVel.x;
		m_forward.y = playerVel.y;
		float speed = m_forward.NormalizeInPlace();

		Vector playerOrigin = GetCentroid( player );

		const float walkSpeed = 100.0f;
		if (speed < walkSpeed)
		{
			m_cutoff.x = playerOrigin.x;
			m_cutoff.y = playerOrigin.y;
			m_forward.x = 0.0f;
			m_forward.y = 0.0f;
		}
		else
		{
			const float k = 1.5f; // 2.0f;
			float trimSpeed = std::min( speed, 200.0f );
			m_cutoff.x = playerOrigin.x + k * trimSpeed * m_forward.x;
			m_cutoff.y = playerOrigin.y + k * trimSpeed * m_forward.y;
		}

		m_targetAreaCount = 0;
	}

	enum { MAX_TARGET_AREAS = 128 };

	bool operator() ( CNavArea *area )
	{
		if (m_targetAreaCount >= MAX_TARGET_AREAS)
			return false;
		
		// only use two-way connections
		if (!area->GetParent() || area->IsConnected( area->GetParent(), NUM_DIRECTIONS ))
		{
			if (m_forward.IsZero())
			{
				m_targetArea[ m_targetAreaCount++ ] = area;
			}
			else
			{
				// collect areas in the direction of the player's forward motion
				Vector2D to( area->GetCenter().x - m_cutoff.x, area->GetCenter().y - m_cutoff.y );
				to.NormalizeInPlace();

				//if (DotProduct( to, m_forward ) > 0.7071f)
				if ((to.x * m_forward.x + to.y * m_forward.y) > 0.7071f)
					m_targetArea[ m_targetAreaCount++ ] = area;
			}
		}

		return (m_targetAreaCount < MAX_TARGET_AREAS);
	}

	CBasePlayer *m_player;
	Vector2D m_forward;
	Vector2D m_cutoff;

	CNavArea *m_targetArea[ MAX_TARGET_AREAS ];
	int m_targetAreaCount;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 * @todo Clean up this nasty mess
 */
void FollowState::OnUpdate( CCSBot *me )
{
	// if we lost our leader, give up
	if (m_leader == nullptr || !m_leader->IsAlive())
	{
		me->Idle();
		return;
	}

	// if we are carrying the bomb and at a bombsite, plant
	if (me->HasC4() && me->IsAtBombsite())
	{
		// plant it
		me->SetTask( CCSBot::PLANT_BOMB );
		me->PlantBomb();

		// radio to the team
		me->GetChatter()->PlantingTheBomb( me->GetPlace() );

		return;
	}

	// look around
	me->UpdateLookAround();

	// if we are moving, we are not idle
	if (me->IsNotMoving() == false)
		m_idleTimer.Start( RandomFloat( 2.0f, 5.0f ) );

	// compute the leader's speed
	Vector leaderVel = m_leader->GetAbsVelocity();
	float leaderSpeed = Vector2D( leaderVel.x, leaderVel.y ).Length();

	// determine our leader's movement state
	ComputeLeaderMotionState( leaderSpeed );

	// track whether we can see the leader
	bool isLeaderVisible;
	Vector leaderOrigin = GetCentroid( m_leader );
	if (me->IsVisible( leaderOrigin ))
	{
		m_lastSawLeaderTime = gpGlobals->curtime;
		isLeaderVisible = true;
	}
	else
	{
		isLeaderVisible = false;
	}

	// determine whether we should sneak or not
	const float farAwayRange = 750.0f;
	Vector myOrigin = GetCentroid( me );
	if ((leaderOrigin - myOrigin).IsLengthGreaterThan( farAwayRange ))
	{
		// far away from leader - run to catch up
		m_isSneaking = false;
	}
	else if (isLeaderVisible)
	{
		// if we see leader walking and we are nearby, walk
		if (m_leaderMotionState == WALKING)
			m_isSneaking = true;

		// if we are sneaking and our leader starts running, stop sneaking
		if (m_isSneaking && m_leaderMotionState == RUNNING)
			m_isSneaking = false;
	}

	// if we haven't seen the leader for a long time, run
	const float longTime = 20.0f;
	if (gpGlobals->curtime - m_lastSawLeaderTime > longTime)
		m_isSneaking = false;

	if (m_isSneaking)
		me->Walk();
	else
		me->Run();

	bool repath = false;

	// if the leader has stopped, hide nearby
	const float nearLeaderRange = 250.0f;
	if (!me->HasPath() && m_leaderMotionState == STOPPED && m_leaderMotionStateTime.GetElapsedTime() > m_waitTime)
	{
		// throttle how often this check occurs
		m_waitTime += RandomFloat( 1.0f, 3.0f );

		// the leader has stopped - if we are close to him, take up a hiding spot
		if ((leaderOrigin - myOrigin).IsLengthLessThan( nearLeaderRange ))
		{
			const float hideRange = 250.0f;
			if (me->TryToHide( nullptr, -1.0f, hideRange, false, true ))
			{
				me->ResetStuckMonitor();
				return;
			}
		}
	}

	// if we have been idle for awhile, move
	if (m_idleTimer.IsElapsed())
	{
		repath = true;

		// always walk when we move such a short distance
		m_isSneaking = true;
	}

	// if our leader has moved, repath (don't repath if leading is stopping)
	if (leaderSpeed > 100.0f && m_leaderMotionState != STOPPED)
	{
		repath = true;
	}

	// move along our path
	if (me->UpdatePathMovement( false ) != CCSBot::PROGRESSING)
	{
		me->DestroyPath();
	}

	// recompute our path if necessary	
	if (repath && m_repathInterval.IsElapsed() && !me->IsOnLadder())
	{
		// recompute our path to keep us near our leader
		m_lastLeaderPos = leaderOrigin;

		me->ResetStuckMonitor();

		const float runSpeed = 200.0f;

		const float collectRange = (leaderSpeed > runSpeed) ? 600.0f : 400.0f;		// 400, 200
		FollowTargetCollector collector( m_leader );
		SearchSurroundingAreas( TheNavMesh->GetNearestNavArea( m_lastLeaderPos ), m_lastLeaderPos, collector, collectRange );

		if (cv_bot_debug.GetBool())
		{
			for( int i=0; i<collector.m_targetAreaCount; ++i )
				collector.m_targetArea[i]->Draw( /*255, 0, 0, 2*/ );		
		}

		// move to one of the collected areas
		if (collector.m_targetAreaCount)
		{
			CNavArea *target = nullptr;
			Vector targetPos;

			// if we are idle, pick a random area
			if (m_idleTimer.IsElapsed())
			{
				target = collector.m_targetArea[ RandomInt( 0, collector.m_targetAreaCount-1 ) ];				
				targetPos = target->GetCenter();
				me->PrintIfWatched( "%4.1f: Bored. Repathing to a new nearby area\n", gpGlobals->curtime );
			}
			else
			{
				me->PrintIfWatched( "%4.1f: Repathing to stay with leader.\n", gpGlobals->curtime );

				// find closest area to where we are
				CNavArea *area;
				float closeRangeSq = 9999999999.9f;
				Vector close;

				for( int a=0; a<collector.m_targetAreaCount; ++a )
				{
					area = collector.m_targetArea[a];

					area->GetClosestPointOnArea( myOrigin, &close );

					float rangeSq = (myOrigin - close).LengthSqr();
					if (rangeSq < closeRangeSq)
					{
						target = area;
						targetPos = close;
						closeRangeSq = rangeSq;
					}
				}
			}
						
			if (target == nullptr || me->ComputePath( target->GetCenter(), FASTEST_ROUTE ) == false)
				me->PrintIfWatched( "Pathfind to leader failed.\n" );

			// throttle how often we repath
			m_repathInterval.Start( 0.5f );

			m_idleTimer.Reset();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void FollowState::OnExit( CCSBot *me )
{
}

class CBaseEntity;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CHostage;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin moving to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnEnter( CCSBot *me )
{
	m_isAtSpot = false;
	m_isLookingOutward = false;

	// if duration is "infinite", set it to a reasonably long time to prevent infinite camping
	if (m_duration < 0.0f)
	{
		m_duration = RandomFloat( 30.0f, 60.0f );
	}

	// decide whether to "ambush" or not - never set to false so as not to override external setting
	if (RandomFloat( 0.0f, 100.0f ) < 50.0f)
	{
		m_isHoldingPosition = true;
	}

	// if we are holding position, decide for how long
	if (m_isHoldingPosition)
	{
		m_holdPositionTime = RandomFloat( 3.0f, 10.0f );
	}
	else
	{
		m_holdPositionTime = 0.0f;
	}

	m_heardEnemy = false;
	m_firstHeardEnemyTime = 0.0f;
	m_retry = 0;

	if ( me->IsFollowing() && me->GetFollowLeader() )
	{
		m_leaderAnchorPos = GetCentroid( me->GetFollowLeader() );
	}

	// if we are a sniper, we need to periodically pause while we retreat to squeeze off a shot or two
	if (me->IsSniper())
	{
		// start off paused to allow a final shot before retreating
		m_isPaused = false;
		m_pauseTimer.Invalidate();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnUpdate( CCSBot *me )
{
	Vector myOrigin = GetCentroid( me );

	// wait until finished reloading to leave hide state
	if (!me->IsReloading())
	{
		// if we are momentarily hiding while following someone, check to see if he has moved on
		if ( me->IsFollowing() && me->GetFollowLeader() )
		{
			CCSPlayer *leader = static_cast<CCSPlayer *>( static_cast<CBaseEntity *>( me->GetFollowLeader() ) );
			Vector leaderOrigin = GetCentroid( leader );

			// BOTPORT: Determine walk/run velocity thresholds
			float runThreshold = 200.0f;
			if (leader->GetAbsVelocity().IsLengthGreaterThan( runThreshold ))
			{
				// leader is running, stay with him
				me->Follow( leader );
				return;			
			}

			// if leader has moved, stay with him
			const float followRange = 250.0f;
			if ((m_leaderAnchorPos - leaderOrigin).IsLengthGreaterThan( followRange ))
			{
				me->Follow( leader );
				return;
			}
		}

		// if we see a nearby buddy in combat, join him
		/// @todo - Perhaps tie in to TakeDamage(), so it works for human players, too

		//
		// Scenario logic
		//
		switch( TheCSBots()->GetScenario() )
		{
			case CCSBotManager::SCENARIO_DEFUSE_BOMB:
			{
				if (me->GetTeamNumber() == TEAM_CT)
				{
					// if we are just holding position (due to a radio order) and the bomb has just planted, go defuse it
					if (me->GetTask() == CCSBot::HOLD_POSITION && 
						TheCSBots()->IsBombPlanted() &&
						TheCSBots()->GetBombPlantTimestamp() > me->GetStateTimestamp())
					{
						me->Idle();
						return;
					}

					// if we are guarding the defuser and he dies/gives up, stop hiding (to choose another defuser)
					if (me->GetTask() == CCSBot::GUARD_BOMB_DEFUSER && TheCSBots()->GetBombDefuser() == nullptr)
					{
						me->Idle();
						return;
					}

					// if we are guarding the loose bomb and it is picked up, stop hiding
					if (me->GetTask() == CCSBot::GUARD_LOOSE_BOMB && TheCSBots()->GetLooseBomb() == nullptr)
					{
						me->GetChatter()->TheyPickedUpTheBomb();
						me->Idle();
						return;
					}

					// if we are guarding a bombsite and the bomb is dropped and we hear about it, stop guarding
					if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE && me->GetGameState()->IsLooseBombLocationKnown())
					{
						me->Idle();
						return;
					}

					// if we are guarding (bombsite, initial encounter, etc) and the bomb is planted, go defuse it
					if (me->IsDoingScenario() && me->GetTask() != CCSBot::GUARD_BOMB_DEFUSER && TheCSBots()->IsBombPlanted())
					{
						me->Idle();
						return;
					}

				}
				else	// TERRORIST
				{
					// if we are near the ticking bomb and someone starts defusing it, attack!
					if (TheCSBots()->GetBombDefuser())
					{
						Vector defuserOrigin = GetCentroid( TheCSBots()->GetBombDefuser() );
						Vector toDefuser = defuserOrigin - myOrigin;

						const float hearDefuseRange = 2000.0f;
						if (toDefuser.IsLengthLessThan( hearDefuseRange ))
						{
							// if we are nearby, attack, otherwise move to the bomb (which will cause us to attack when we see defuser)
							if (me->CanSeePlantedBomb())
							{
								me->Attack( TheCSBots()->GetBombDefuser() );
							}
							else
							{
								me->MoveTo( defuserOrigin, FASTEST_ROUTE );
								me->InhibitLookAround( 10.0f );
							}

							return;
						}
					}
				}
				break;
			}

			//--------------------------------------------------------------------------------------------------
			case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
			{
				// if we're guarding the hostages and they all die or are taken, do something else
				if (me->GetTask() == CCSBot::GUARD_HOSTAGES)
				{
					if (me->GetGameState()->AreAllHostagesBeingRescued() || me->GetGameState()->AreAllHostagesGone())
					{
						me->Idle();
						return;
					}
				}
				else if (me->GetTask() == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
				{
					// if we stumble across a hostage, guard it
					CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
					if (hostage)
					{
						// we see a free hostage, guard it
						Vector hostageOrigin = GetCentroid( hostage );
						CNavArea *area = TheNavMesh->GetNearestNavArea( hostageOrigin );
						if (area)
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area );
							me->PrintIfWatched( "I'm guarding hostages I found\n" );
							// don't chatter here - he'll tell us when he's in his hiding spot
							return;
						}
					}
				}
			}
		}

		bool isSettledInSniper = (me->IsSniper() && m_isAtSpot) ? true : false;

		// only investigate noises if we are initiating attacks, and we aren't a "settled in" sniper
		// dont investigate noises if we are reloading
		if (!me->IsReloading() && 
			!isSettledInSniper && 
			me->GetDisposition() == CCSBot::ENGAGE_AND_INVESTIGATE)
		{
			// if we are holding position, and have heard the enemy nearby, investigate after our hold time is up
			if (m_isHoldingPosition && m_heardEnemy && (gpGlobals->curtime - m_firstHeardEnemyTime > m_holdPositionTime))
			{
				/// @todo We might need to remember specific location of last enemy noise here
				me->InvestigateNoise();
				return;
			}

			// investigate nearby enemy noises
			if (me->HeardInterestingNoise())
			{
				// if we are holding position, check if enough time has elapsed since we first heard the enemy
				if (m_isAtSpot && m_isHoldingPosition)
				{
					if (!m_heardEnemy)
					{
						// first time we heard the enemy
						m_heardEnemy = true;
						m_firstHeardEnemyTime = gpGlobals->curtime;
						me->PrintIfWatched( "Heard enemy, holding position for %f2.1 seconds...\n", m_holdPositionTime );
					}
				}
				else
				{
					// not holding position - investigate enemy noise
					me->InvestigateNoise();
					return;
				}
			}
		}
	}	// end reloading check

	// look around
	me->UpdateLookAround();

	// if we are at our hiding spot, crouch and wait
	if (m_isAtSpot)
	{
		me->ResetStuckMonitor();

		CNavArea *area = TheNavMesh->GetNavArea( m_hidingSpot );
		if ( !area || !( area->GetAttributes() & NAV_MESH_STAND ) )
		{
			me->Crouch();
		}

		// check if duration has expired
		if (m_hideTimer.IsElapsed()) 
		{
			if (me->GetTask() == CCSBot::GUARD_LOOSE_BOMB)
			{
				// if we're guarding the loose bomb, continue to guard it but pick a new spot
				me->Hide( TheCSBots()->GetLooseBombArea() );
				return;
			}
			else if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE)
			{
				// if we're guarding a bombsite, continue to guard it but pick a new spot
				const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( myOrigin );
				if (zone)
				{
					CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
					if (area)
					{
						me->Hide( area );
						return;
					}
				}
			}
			else if (me->GetTask() == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
			{
				// if we're guarding a rescue zone, continue to guard this or another rescue zone
				if (me->GuardRandomZone())
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Continuing to guard hostage rescue zones\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}

			me->Idle();
			return;
		}

/*
		// if we are watching for an approaching noisy enemy, anticipate and fire before they round the corner
		/// @todo Need to check if we are looking at an ENEMY_NOISE here
		const float veryCloseNoise = 250.0f;
		if (me->IsLookingAtSpot() && me->GetNoiseRange() < veryCloseNoise)
		{
			// fire!
			me->PrimaryAttack();
			me->PrintIfWatched( "Firing at anticipated enemy coming around the corner!\n" );
		}
*/

		// if we have a shield, hide behind it
		if (me->HasShield() && !me->IsProtectedByShield())
			me->SecondaryAttack();

		// while sitting at our hiding spot, if we are being attacked but can't see our attacker, move somewhere else
		const float hurtRecentlyTime = 1.0f;
		if (!me->IsEnemyVisible() && me->GetTimeSinceAttacked() < hurtRecentlyTime)
		{
			me->Idle();
			return;
		}

		// encourage the human player
		if (!me->IsDoingScenario())
		{
			if (me->GetTeamNumber() == TEAM_CT)
			{
				if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE && 
					me->IsAtHidingSpot() && 
					TheCSBots()->IsBombPlanted())
				{
					if (me->GetNearbyEnemyCount() == 0)
					{
						const float someTime = 30.0f;
						const float littleTime = 11.0;

						if (TheCSBots()->GetBombTimeLeft() > someTime)
							me->GetChatter()->Encourage( "BombsiteSecure", RandomFloat( 10.0f, 15.0f ) );
						else if (TheCSBots()->GetBombTimeLeft() > littleTime)
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBomb", RandomFloat( 5.0f, 8.0f ) );
						else
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBombPanic", RandomFloat( 3.0f, 4.0f ) );
					}
				}

				if (me->GetTask() == CCSBot::GUARD_HOSTAGES && me->IsAtHidingSpot())
				{
					if (me->GetNearbyEnemyCount() == 0)
					{
						CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
						if (hostage)
						{
							me->GetChatter()->Encourage( "WaitingForHumanToRescueHostages", RandomFloat( 10.0f, 15.0f ) );
						}
					}
				}
			}
		}
	}
	else
	{
		// we are moving to our hiding spot

		// snipers periodically pause and fire while retreating
		if (me->IsSniper() && me->IsEnemyVisible())
		{
			if (m_isPaused)
			{
				if (m_pauseTimer.IsElapsed())
				{
					// get moving
					m_isPaused = false;
					m_pauseTimer.Start( RandomFloat( 1.0f, 3.0f ) );
				}
				else
				{
					me->Wait( 0.2f );
				}
			}
			else
			{
				if (m_pauseTimer.IsElapsed())
				{
					// pause for a moment
					m_isPaused = true;
					m_pauseTimer.Start( RandomFloat( 0.5f, 1.5f ) );
				}
			}
		}

		// if a Player is using this hiding spot, give up
		float range;
		CCSPlayer *camper = static_cast<CCSPlayer *>( UTIL_GetClosestPlayer( m_hidingSpot, &range ) );

		const float closeRange = 75.0f;
		if (camper && camper != me && range < closeRange && me->IsVisible( camper, true ))
		{
			// player is in our hiding spot
			me->PrintIfWatched( "Someone's in my hiding spot - picking another...\n" );

			const int maxRetries = 3;
			if (m_retry++ >= maxRetries)
			{
				me->PrintIfWatched( "Can't find a free hiding spot, giving up.\n" );
				me->Idle();
				return;
			}

			// pick another hiding spot near where we were planning on hiding
			me->Hide( TheNavMesh->GetNavArea( m_hidingSpot ) );

			return;
		}

		Vector toSpot;
		toSpot.x = m_hidingSpot.x - myOrigin.x;
		toSpot.y = m_hidingSpot.y - myOrigin.y;
		toSpot.z = m_hidingSpot.z - me->GetFeetZ(); // use feet location
		range = toSpot.Length();

		// look outwards as we get close to our hiding spot
		if (!me->IsEnemyVisible() && !m_isLookingOutward)
		{
			const float lookOutwardRange = 200.0f;
			const float nearSpotRange = 10.0f;
			if (range < lookOutwardRange && range > nearSpotRange)
			{
				m_isLookingOutward = true;

				toSpot.x /= range;
				toSpot.y /= range;
				toSpot.z /= range;

				me->SetLookAt( "Face outward", me->EyePosition() - 1000.0f * toSpot, PRIORITY_HIGH, 3.0f );
			}
		}

		const float atDist = 20.0f;
		if (range < atDist)
		{
			//-------------------------------------
			// Just reached our hiding spot
			//
			m_isAtSpot = true;
			m_hideTimer.Start( m_duration );

			// make sure our approach points are valid, since we'll be watching them
			me->ComputeApproachPoints();
			me->ClearLookAt();

			// ready our weapon and prepare to attack
			me->EquipBestWeapon( me->IsUsingGrenade() );
			me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

			// if we are a sniper, update our task
			if (me->GetTask() == CCSBot::MOVE_TO_SNIPER_SPOT)
			{
				me->SetTask( CCSBot::SNIPING );
			}
			else if (me->GetTask() == CCSBot::GUARD_INITIAL_ENCOUNTER)
			{
				const float campChatterChance = 20.0f;
				if (RandomFloat( 0, 100 ) < campChatterChance)
				{
					me->GetChatter()->Say( "WaitingHere" );
				}
			}

			// determine which way to look
			trace_t result;
			float outAngle = 0.0f;
			float outAngleRange = 0.0f;
			for( float angle = 0.0f; angle < 360.0f; angle += 45.0f )
			{
				UTIL_TraceLine( me->EyePosition(), me->EyePosition() + 1000.0f * Vector( BotCOS(angle), BotSIN(angle), 0.0f ), MASK_PLAYERSOLID, me, COLLISION_GROUP_NONE, &result );

				if (result.fraction > outAngleRange)
				{
					outAngle = angle;
					outAngleRange = result.fraction;
				}
			}

			me->SetLookAheadAngle( outAngle );

		}

		// move to hiding spot
		if (me->UpdatePathMovement() != CCSBot::PROGRESSING && !m_isAtSpot)
		{
			// we couldn't get to our hiding spot - pick another
			me->PrintIfWatched( "Can't get to my hiding spot - finding another...\n" );

			// search from hiding spot, since we know it was valid
			const Vector *pos = FindNearbyHidingSpot( me, m_hidingSpot, m_range, me->IsSniper() );
			if (pos == nullptr)
			{
				// no available hiding spots
				me->PrintIfWatched( "No available hiding spots - hiding where I'm at.\n" );

				// hide where we are
				m_hidingSpot.x = myOrigin.x;
				m_hidingSpot.x = myOrigin.y;
				m_hidingSpot.z = me->GetFeetZ();
			}
			else
			{
				m_hidingSpot = *pos;
			}

			// build a path to our new hiding spot
			if (me->ComputePath( m_hidingSpot, FASTEST_ROUTE ) == false)
			{
				me->PrintIfWatched( "Can't pathfind to hiding spot\n" );
				me->Idle();
				return;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void HideState::OnExit( CCSBot *me )
{
	m_isHoldingPosition = false;

	me->StandUp();
	me->ResetStuckMonitor();
	//me->ClearLookAt();
	me->ClearApproachPoints();

	// if we have a shield, put it away
	if (me->HasShield() && me->IsProtectedByShield())
		me->SecondaryAttack();
}

class CBaseEntity;
class CCSBot;
class CCSBotManager;
class CHostage;
class CNavArea;
struct Extent;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the hunt
 */
void HuntState::OnEnter( CCSBot *me )
{
	// lurking death
	if (me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying())
		me->Walk();
	else
		me->Run();

	me->StandUp();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->SetTask( CCSBot::SEEK_AND_DESTROY );

	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Hunt down our enemies
 */
void HuntState::OnUpdate( CCSBot *me )
{
	// if we've been hunting for a long time, drop into Idle for a moment to
	// select something else to do
	const float huntingTooLongTime = 30.0f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > huntingTooLongTime)
	{
		// stop being a rogue and do the scenario, since there must not be many enemies left to hunt
		me->PrintIfWatched( "Giving up hunting.\n" );
		me->SetRogue( false );
		me->Idle();
		return;
	}

	// scenario logic
	if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
	{
		if (me->GetTeamNumber() == TEAM_TERRORIST)
		{
			// if we have the bomb and it's time to plant, or we happen to be in a bombsite and it seems safe, do it
			if (me->HasC4())
			{
				const float safeTime = 3.0f;

				if (TheCSBots()->IsTimeToPlantBomb() || 
						(me->IsAtBombsite() && gpGlobals->curtime - me->GetLastSawEnemyTimestamp() > safeTime))
				{
					me->Idle();
					return;
				}
			}

			// if we notice the bomb lying on the ground, go get it
			if (me->NoticeLooseBomb())
			{
				me->FetchBomb();
				return;
			}

			// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and watch it
			const Vector *bombPos = me->GetGameState()->GetBombPosition();
			if (!me->IsRogue() && me->GetGameState()->IsBombPlanted() && bombPos)
			{
				me->SetTask( CCSBot::GUARD_TICKING_BOMB );
				me->Hide( TheNavMesh->GetNavArea( *bombPos ) );
				return;
			}
		}
		else		// CT
		{
			if (!me->IsRogue() && me->CanSeeLooseBomb())
			{
				// if we are near the loose bomb and can see it, hide nearby and guard it
				me->SetTask( CCSBot::GUARD_LOOSE_BOMB );
				me->Hide( TheCSBots()->GetLooseBombArea() );
				me->GetChatter()->GuardingLooseBomb( TheCSBots()->GetLooseBomb() );
				return;
			}
			else if (TheCSBots()->IsBombPlanted())
			{
				// rogues will defuse a bomb, but not guard the defuser
				if (!me->IsRogue() || !TheCSBots()->GetBombDefuser())
				{
					// search for the planted bomb to defuse
					me->Idle();
					return;
				}
			}
		}
	}
	else if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES)
	{
		if (me->GetTeamNumber() == TEAM_TERRORIST)
		{
			if (me->GetGameState()->AreAllHostagesBeingRescued())
			{
				// all hostages are being rescued, head them off at the escape zones
				if (me->GuardRandomZone())
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Trying to beat them to an escape zone!\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}

			// if safe time is up, and we stumble across a hostage, guard it
			if (!me->IsRogue() && !me->IsSafe())
			{
				CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
				if (hostage)
				{
					CNavArea *area = TheNavMesh->GetNearestNavArea( GetCentroid( hostage ) );
					if (area)
					{
						// we see a free hostage, guard it
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->Hide( area );
						me->PrintIfWatched( "I'm guarding hostages\n" );
						me->GetChatter()->GuardingHostages( area->GetPlace(), true );
						return;
					}
				}
			}
		}
	}

	// listen for enemy noises
	if (me->HeardInterestingNoise())
	{
		me->InvestigateNoise();
		return;
	}
		
	// look around
	me->UpdateLookAround();

	if ( !TheCSBots()->AllowedToDoExpensiveBotOperationThisFrame() )
		return;

	// if we have reached our destination area, pick a new one
	// if our path fails, pick a new one
	if (me->GetLastKnownArea() == m_huntArea || me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		// pick a new hunt area
		const float earlyGameTime = 45.0f;
		if (TheCSBots()->GetElapsedRoundTime() < earlyGameTime && !me->HasVisitedEnemySpawn())
		{
			// in the early game, rush the enemy spawn
			CBaseEntity *enemySpawn = TheCSBots()->GetRandomSpawn( OtherTeam( me->GetTeamNumber() ) );

			//ADRIAN: REVISIT
			if ( enemySpawn )
			{
				m_huntArea = TheNavMesh->GetNavArea( enemySpawn->WorldSpaceCenter() );
			}
		}
		else
		{
			m_huntArea = nullptr;
			float oldest = 0.0f;

			int areaCount = 0;
			const float minSize = 150.0f;

			for ( CNavArea *area : TheNavAreas )
			{
				++areaCount;

				// skip the small areas
				Extent extent;
				area->GetExtent(&extent);
				if (extent.hi.x - extent.lo.x < minSize || extent.hi.y - extent.lo.y < minSize)
					continue;

				// keep track of the least recently cleared area
				float age = gpGlobals->curtime - area->GetClearedTimestamp( me->GetTeamNumber()-1 );
				if (age > oldest)
				{
					oldest = age;
					m_huntArea = area;
				}
			}

			// if all the areas were too small, pick one at random
			int which = RandomInt( 0, areaCount-1 );

			areaCount = 0;
			for ( CNavArea *pArea : TheNavAreas )
			{
				m_huntArea = pArea;

				if (which == areaCount)
					break;

				--which;
			}
		}

		if (m_huntArea)
		{
			// create a new path to a far away area of the map
			me->ComputePath( m_huntArea->GetCenter() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Done hunting
 */
void HuntState::OnExit( CCSBot *me )
{
}

class CBaseEntity;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CHostage;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

// range for snipers to select a hiding spot
static const float sniperHideRange = 2000.0f;

extern ConVar mp_guardian_target_site;
//--------------------------------------------------------------------------------------------------------------
/**
 * The Idle state.
 * We never stay in the Idle state - it is a "home base" for the state machine that
 * does various checks to determine what we should do next.
 */
void IdleState::OnEnter( CCSBot *me )
{
	me->DestroyPath();
	me->SetBotEnemy( nullptr );

	// lurking death
	if (me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying())
		me->Walk();

	//
	// Since Idle assigns tasks, we assume that coming back to Idle means our task is complete
	//
	me->SetTask( CCSBot::SEEK_AND_DESTROY );
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine what we should do next
 */
void IdleState::OnUpdate( CCSBot *me )
{
	// all other states assume GetLastKnownArea() is valid, ensure that it is
	if (me->GetLastKnownArea() == nullptr && me->StayOnNavMesh() == false)
		return;

	// zombies never leave the Idle state
	if (cv_bot_zombie.GetBool())
	{
		me->ResetStuckMonitor();
		return;
	}

	// if we are in the early "safe" time, grab a knife or grenade
	if (me->IsSafe())
	{
		// if we have a grenade, use it
		if (!me->EquipGrenade())
		{
			// high-skill bots run with the knife
			if (me->GetProfile()->GetSkill() > 0.33f)
			{
				me->EquipKnife();
			}
		}
	}

	// if round is over, hunt
	if (me->GetGameState()->IsRoundOver())
	{
		// if we are escorting hostages, try to get to the rescue zone
		if (me->GetHostageEscortCount())
		{
			const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( me->GetLastKnownArea(), PathCost( me, FASTEST_ROUTE ) );
			const Vector *zonePos = TheCSBots()->GetRandomPositionInZone( zone );

			if (zonePos)
			{
				me->SetTask( CCSBot::RESCUE_HOSTAGES );
				me->Run();
				me->SetDisposition( CCSBot::SELF_DEFENSE );
				me->MoveTo( *zonePos, FASTEST_ROUTE );
				me->PrintIfWatched( "Trying to rescue hostages at the end of the round\n" );
				return;
			}
		}

		me->Hunt();
		return;
	}

	const float defenseSniperCampChance = 75.0f;
	const float offenseSniperCampChance = 10.0f;

	// if we were following someone, continue following them
	if (me->IsFollowing())
	{
		me->ContinueFollowing();
		return;
	}

	if ( CSGameRules()->IsPlayingCooperativeGametype() )
	{
		UpdateCoop( me );
		return;
	}
		
	//
	// Scenario logic
	//
	switch (TheCSBots()->GetScenario())
	{
		//======================================================================================================
		case CCSBotManager::SCENARIO_DEFUSE_BOMB:
		{
			// if this is a bomb game and we have the bomb, go plant it
			if (me->GetTeamNumber() == TEAM_TERRORIST)
			{
				if (me->GetGameState()->IsBombPlanted())
				{
					if (me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN)
					{
						// T's always know where the bomb is - go defend it
						const CCSBotManager::Zone *zone = TheCSBots()->GetZone( me->GetGameState()->GetPlantedBombsite() );
						if (zone)
						{
							me->SetTask( CCSBot::GUARD_TICKING_BOMB );

							Place place = TheNavMesh->GetPlace( zone->m_center );
							if (place != UNDEFINED_PLACE)
							{
								// pick a random hiding spot in this place
								const Vector *spot = FindRandomHidingSpot( me, place, me->IsSniper() );
								if (spot)
								{
									me->Hide( *spot );
									return;
								}
							}

							// hide nearby
							me->Hide( TheNavMesh->GetNearestNavArea( zone->m_center ) );
							return;
						}
					}
					else
					{
						// ask our teammates where the bomb is
						me->GetChatter()->RequestBombLocation();

						// we dont know where the bomb is - we must search the bombsites
						int zoneIndex = me->GetGameState()->GetNextBombsiteToSearch();

						// move to bombsite - if we reach it, we'll update its cleared status, causing us to select another
						const Vector *pos = TheCSBots()->GetRandomPositionInZone( TheCSBots()->GetZone( zoneIndex ) );
						if (pos)
						{
							me->SetTask( CCSBot::FIND_TICKING_BOMB );
							me->MoveTo( *pos );
							return;
						}
					}
				}
				else if (me->HasC4())
				{
					// always pick a random spot to plant in case the spot we'd picked is inaccessible
					if (TheCSBots()->IsTimeToPlantBomb())
					{
						// move to the closest bomb site
						const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( me->GetLastKnownArea(), PathCost( me ) );
						if (zone)
						{
							// pick a random spot within the bomb zone
							const Vector *pos = TheCSBots()->GetRandomPositionInZone( zone );
							if (pos)
							{
								// move to bombsite
								me->SetTask( CCSBot::PLANT_BOMB );
								me->Run();
								me->MoveTo( *pos );

								return;
							}
						}
					}
				}
				else
				{
					// at the start of the round, we may decide to defend "initial encounter" areas
					// where we will first meet the enemy rush
					if (me->IsSafe())
					{
						float defendRushChance = -17.0f * (me->GetMorale() - 2);

						if (me->IsSniper() || RandomFloat( 0.0f, 100.0f ) < defendRushChance)
						{
							if (me->MoveToInitialEncounter())
							{
								me->PrintIfWatched( "I'm guarding an initial encounter area\n" );
								me->SetTask( CCSBot::GUARD_INITIAL_ENCOUNTER );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}

					// small chance of sniper camping on offense, if we aren't carrying the bomb
					if (me->GetFriendsRemaining() && me->IsSniper() && RandomFloat( 0, 100.0f ) < offenseSniperCampChance)
					{
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
						me->Hide( me->GetLastKnownArea(), RandomFloat( 10.0f, 30.0f ), sniperHideRange );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						me->PrintIfWatched( "Sniping!\n" );
						return;
					}

					// if the bomb is loose (on the ground), go get it
					if (me->NoticeLooseBomb())
					{
						me->FetchBomb();
						return;
					}

					// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and guard it
					if (!me->IsRogue() && me->GetGameState()->IsBombPlanted() && me->GetGameState()->GetBombPosition())
					{
						const Vector *bombPos = me->GetGameState()->GetBombPosition();

						if (bombPos)
						{
							me->SetTask( CCSBot::GUARD_TICKING_BOMB );
							me->Hide( TheNavMesh->GetNavArea( *bombPos ) );
							return;
						}
					}
				}
			}
			else	// CT ------------------------------------------------------------------------------------------
			{
				if (me->GetGameState()->IsBombPlanted())
				{
					// if the bomb has been planted, attempt to defuse it
					const Vector *bombPos = me->GetGameState()->GetBombPosition();
					if (bombPos)
					{
						// if someone is defusing the bomb, guard them
						if (TheCSBots()->GetBombDefuser())
						{
							if (!me->IsRogue())
							{
								me->SetTask( CCSBot::GUARD_BOMB_DEFUSER );
								me->Hide( TheNavMesh->GetNavArea( *bombPos ) );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
						else if (me->IsDoingScenario())
						{
							// move to the bomb and defuse it
							me->SetTask( CCSBot::DEFUSE_BOMB );
							me->SetDisposition( CCSBot::SELF_DEFENSE );
							me->MoveTo( *bombPos );
							return;
						}
						else
						{
							// we're not allowed to defuse, guard the bomb zone
							me->SetTask( CCSBot::GUARD_BOMB_ZONE );
							me->Hide( TheNavMesh->GetNavArea( *bombPos ) );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
					else if (me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN)
					{
						// we know which bombsite, but not exactly where the bomb is, go there
						const CCSBotManager::Zone *zone = TheCSBots()->GetZone( me->GetGameState()->GetPlantedBombsite() );
						if (zone)
						{
							if (me->IsDoingScenario())
							{
								me->SetTask( CCSBot::DEFUSE_BOMB );
								me->MoveTo( zone->m_center );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
							else
							{
								// we're not allowed to defuse, guard the bomb zone
								me->SetTask( CCSBot::GUARD_BOMB_ZONE );
								me->Hide( TheNavMesh->GetNavArea( zone->m_center ) );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
					else
					{
						// we dont know where the bomb is - we must search the bombsites

						// find closest un-cleared bombsite
						const CCSBotManager::Zone *zone = nullptr;
						float travelDistance = 9999999.9f;

						for( int z=0; z<TheCSBots()->GetZoneCount(); ++z )
						{
							if (TheCSBots()->GetZone(z)->m_areaCount == 0)
								continue;

							// don't check bombsites that have been cleared
							if (me->GetGameState()->IsBombsiteClear( z ))
								continue;

							// just use the first overlapping nav area as a reasonable approximation
							ShortestPathCost cost = ShortestPathCost();
							float dist = NavAreaTravelDistance( me->GetLastKnownArea(), 
																TheNavMesh->GetNearestNavArea( TheCSBots()->GetZone(z)->m_center ), 
																cost );

							if (dist >= 0.0f && dist < travelDistance)
							{
								zone = TheCSBots()->GetZone(z);
								travelDistance = dist;
							}
						}			
						
						if (zone)
						{
							const float farAwayRange = 2000.0f;
							if (travelDistance > farAwayRange)
							{
								zone = nullptr;
							}
						}

						// if closest bombsite is "far away", pick one at random
						if (zone == nullptr)
						{
							int zoneIndex = me->GetGameState()->GetNextBombsiteToSearch();
							zone = TheCSBots()->GetZone( zoneIndex );
						}

						// move to bombsite - if we reach it, we'll update its cleared status, causing us to select another
						if (zone)
						{
							const Vector *pos = TheCSBots()->GetRandomPositionInZone( zone );
							if (pos)
							{
								me->SetTask( CCSBot::FIND_TICKING_BOMB );
								me->MoveTo( *pos );
								return;
							}
						}
					}
					contract_assert( false && "A CT bot doesn't know what to do while the bomb is planted!\n" );
				}

				// if we have a sniper rifle, we like to camp, whether rogue or not
				if (me->IsSniper() && !me->IsSafe())
				{
					if (RandomFloat( 0, 100 ) <= defenseSniperCampChance)
					{
						CNavArea *snipingArea = nullptr;

						// if the bomb is loose, snipe near it
						const Vector *bombPos = me->GetGameState()->GetBombPosition();
						if (me->GetGameState()->IsLooseBombLocationKnown() && bombPos)
						{
							snipingArea = TheNavMesh->GetNearestNavArea( *bombPos );
							me->PrintIfWatched( "Sniping near loose bomb\n" );
						}
						else
						{
							// snipe bomb zone(s)
							const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
							if (zone)
							{
								snipingArea = TheCSBots()->GetRandomAreaInZone( zone );
								me->PrintIfWatched( "Sniping near bombsite\n" );
							}
						}

						if (snipingArea)
						{
							me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
							me->Hide( snipingArea, -1.0, sniperHideRange );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				// if we know the bomb is dropped, hunt for enemies and the loose bomb
				if (me->IsRogue() || TheCSBots()->IsDefenseRushing() || me->GetGameState()->IsLooseBombLocationKnown())
				{
					me->Hunt();
					return;
				}

				// the lower our morale gets, the more we want to camp the bomb zone(s)
				// only decide to camp at the start of the round, or if we haven't seen anything for a long time
				if (me->IsSafe() || me->HasNotSeenEnemyForLongTime())
				{
					float guardBombsiteChance = -34.0f * me->GetMorale();

					if (RandomFloat( 0.0f, 100.0f ) < guardBombsiteChance)
					{
						float guardRange = 500.0f + 100.0f * (me->GetMorale() + 3);

						// guard bomb zone(s)
						const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
						if (zone)
						{
							CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
							if (area)
							{
								me->PrintIfWatched( "I'm guarding a bombsite\n" );
								me->GetChatter()->GuardingBombsite( area->GetPlace() );
								me->SetTask( CCSBot::GUARD_BOMB_ZONE );
								me->Hide( area, -1.0, guardRange );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}

					// at the start of the round, we may decide to defend "initial encounter" areas
					// where we will first meet the enemy rush
					if (me->IsSafe())
					{
						float defendRushChance = -17.0f * (me->GetMorale() - 2);

						if (me->IsSniper() || RandomFloat( 0.0f, 100.0f ) < defendRushChance)
						{
							if (me->MoveToInitialEncounter())
							{
								me->PrintIfWatched( "I'm guarding an initial encounter area\n" );
								me->SetTask( CCSBot::GUARD_INITIAL_ENCOUNTER );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
				}
			}

			break;
		}

		//======================================================================================================
		case CCSBotManager::SCENARIO_ESCORT_VIP:
		{
			if (me->GetTeamNumber() == TEAM_TERRORIST)
			{
				// if we have a sniper rifle, we like to camp, whether rogue or not
				if (me->IsSniper())
				{
					if (RandomFloat( 0, 100 ) <= defenseSniperCampChance)
					{
						// snipe escape zone(s)
						const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
						if (zone)
						{
							CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
							if (area)
							{
								me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
								me->Hide( area, -1.0, sniperHideRange );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								me->PrintIfWatched( "Sniping near escape zone\n" );
								return;
							}
						}
					}
				}

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				if (me->IsRogue() || TheCSBots()->IsDefenseRushing())
					break;

				// the lower our morale gets, the more we want to camp the escape zone(s)
				float guardEscapeZoneChance = -34.0f * me->GetMorale();

				if (RandomFloat( 0.0f, 100.0f ) < guardEscapeZoneChance)
				{
					// guard escape zone(s)
					const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
					if (zone)
					{
						CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
						if (area)
						{
							// guard the escape zone - stay closer if our morale is low
							me->SetTask( CCSBot::GUARD_VIP_ESCAPE_ZONE );
							me->PrintIfWatched( "I'm guarding an escape zone\n" );

							float escapeGuardRange = 750.0f + 250.0f * (me->GetMorale() + 3);
							me->Hide( area, -1.0, escapeGuardRange );

							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}
			}
			else	// CT
			{
				if (me->m_bIsVIP)
				{
					// if early in round, pick a random zone, otherwise pick closest zone
					const float earlyTime = 20.0f;
					const CCSBotManager::Zone *zone = nullptr;

					if (TheCSBots()->GetElapsedRoundTime() < earlyTime)
					{
						// pick random zone
						zone = TheCSBots()->GetRandomZone();
					}
					else
					{
						// pick closest zone
						zone = TheCSBots()->GetClosestZone( me->GetLastKnownArea(), PathCost( me ) );
					}

					if (zone)
					{
						// pick a random spot within the escape zone
						const Vector *pos = TheCSBots()->GetRandomPositionInZone( zone );
						if (pos)
						{
							// move to escape zone
							me->SetTask( CCSBot::VIP_ESCAPE );
							me->Run();
							me->MoveTo( *pos );

							// tell team to follow
							const float repeatTime = 30.0f;
							if (me->GetFriendsRemaining() && 
									TheCSBots()->GetRadioMessageInterval( RADIO_FOLLOW_ME, me->GetTeamNumber() ) > repeatTime)
								me->SendRadioMessage( RADIO_FOLLOW_ME );
							return;
						}
					}
				}
				else
				{
					// small chance of sniper camping on offense, if we aren't VIP
					if (me->GetFriendsRemaining() && me->IsSniper() && RandomFloat( 0, 100.0f ) < offenseSniperCampChance)
					{
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
						me->Hide( me->GetLastKnownArea(), RandomFloat( 10.0f, 30.0f ), sniperHideRange );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						me->PrintIfWatched( "Sniping!\n" );
						return;
					}
				}
			}
			break;
		}

		//======================================================================================================
		case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
		{
			if (me->GetTeamNumber() == TEAM_TERRORIST)
			{
				bool campHostages;

				// if we are in early game, camp the hostages
				if (me->IsSafe())
				{
					campHostages = true;
				}
				else if (me->GetGameState()->HaveSomeHostagesBeenTaken() || me->GetGameState()->AreAllHostagesBeingRescued())
				{
					campHostages = false;
				}
				else
				{
					// later in the game, camp either hostages or escape zone
					const float campZoneChance = 100.0f * (TheCSBots()->GetElapsedRoundTime() - me->GetSafeTime())/120.0f;

					campHostages = (RandomFloat( 0, 100 ) > campZoneChance) ? true : false; 
				}

				// if we have a sniper rifle, we like to camp, whether rogue or not
				if (me->IsSniper())
				{
					// the at start of the round, snipe the initial rush
					if (me->IsSafe())
					{
						if (me->MoveToInitialEncounter() && CSGameRules()->IsPlayingCooperativeGametype() == false )
						{
							me->PrintIfWatched( "I'm sniping an initial encounter area\n" );
							me->SetTask( CCSBot::GUARD_INITIAL_ENCOUNTER );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}

					if (RandomFloat( 0, 100 ) <= defenseSniperCampChance)
					{
						const Vector *hostagePos = me->GetGameState()->GetRandomFreeHostagePosition();
						if (hostagePos && campHostages)
						{
							me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
							me->PrintIfWatched( "Sniping near hostages\n" );
							me->Hide( TheNavMesh->GetNearestNavArea( *hostagePos ), -1.0, sniperHideRange );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
						else
						{
							// camp the escape zone(s)
							if (me->GuardRandomZone( sniperHideRange ))
							{
								me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
								me->PrintIfWatched( "Sniping near a rescue zone\n" );
								me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
								return;
							}
						}
					}
				}

				// if safe time is up, and we stumble across a hostage, guard it
				if (!me->IsSafe() && !me->IsRogue())
				{
					CBaseEntity *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
					if (hostage)
					{
						// we see a free hostage, guard it
						CNavArea *area = TheNavMesh->GetNearestNavArea( GetCentroid( hostage ) );
						if (area)
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area );
							me->PrintIfWatched( "I'm guarding hostages I found\n" );
							// don't chatter here - he'll tell us when he's in his hiding spot
							return;
						}
					}
				}

				// decide if we want to hunt, or guard
				const float huntChance = 70.0f + 25.0f * me->GetMorale();

				// rogues just hunt, unless they want to snipe
				// if the whole team has decided to rush, hunt
				if (me->GetFriendsRemaining())
				{
					if (me->IsRogue() || TheCSBots()->IsDefenseRushing() || RandomFloat( 0, 100 ) < huntChance)
					{
						me->Hunt();
						return;
					}
				}

				// at the start of the round, we may decide to defend "initial encounter" areas
				// where we will first meet the enemy rush
				if (me->IsSafe())
				{
					float defendRushChance = -17.0f * (me->GetMorale() - 2);

					if (me->IsSniper() || RandomFloat( 0.0f, 100.0f ) < defendRushChance)
					{
						if (me->MoveToInitialEncounter())
						{
							me->PrintIfWatched( "I'm guarding an initial encounter area\n" );
							me->SetTask( CCSBot::GUARD_INITIAL_ENCOUNTER );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}

				// decide whether to camp the hostages or the escape zones
				const Vector *hostagePos = me->GetGameState()->GetRandomFreeHostagePosition();
				if (hostagePos && campHostages)
				{
					CNavArea *area = TheNavMesh->GetNearestNavArea( *hostagePos );
					if (area)
					{
						// guard the hostages - stay closer to hostages if our morale is low
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->PrintIfWatched( "I'm guarding hostages\n" );

						float hostageGuardRange = 750.0f + 250.0f * (me->GetMorale() + 3);			// 2000
						me->Hide( area, -1.0, hostageGuardRange );
						me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

						if (RandomFloat( 0, 100 ) < 50)
							me->GetChatter()->GuardingHostages( area->GetPlace(), true );

						return;
					}
				}

				// guard rescue zone(s)
				if (me->GuardRandomZone())
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "I'm guarding a rescue zone\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( true );
					return;
				}
			}
			else	// CT ---------------------------------------------------------------------------------
			{
				// only decide to do something else if we aren't already rescuing hostages
				if (!me->GetHostageEscortCount())
				{
					// small chance of sniper camping on offense
					if (me->GetFriendsRemaining() && me->IsSniper() && RandomFloat( 0, 100.0f ) < offenseSniperCampChance)
					{
						if ( CSGameRules()->IsPlayingCooperativeGametype() == false )
						{
							me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
							me->Hide( me->GetLastKnownArea(), RandomFloat( 10.0f, 30.0f ), sniperHideRange );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							me->PrintIfWatched( "Sniping!\n" );
							return;
						}
					}

					if (me->GetFriendsRemaining() && !me->GetHostageEscortCount())
					{
						// rogues just hunt, unless all friends are dead
						// if we have friends left, we might go hunting instead of hostage rescuing
						const float huntChance = 33.3f;
						if (me->IsRogue() || RandomFloat( 0.0f, 100.0f ) < huntChance)
						{
							me->Hunt();
							return;
						}
					}
				}

				// at the start of the round, we may decide to defend "initial encounter" areas
				// where we will first meet the enemy rush
				if (me->IsSafe())
				{
					float defendRushChance = -17.0f * (me->GetMorale() - 2);

					if (CSGameRules()->IsPlayingCooperativeGametype() == false && 
						 (me->IsSniper() || RandomFloat( 0.0f, 100.0f ) < defendRushChance) )
					{
						if (me->MoveToInitialEncounter())
						{
							me->PrintIfWatched( "I'm guarding an initial encounter area\n" );
							me->SetTask( CCSBot::GUARD_INITIAL_ENCOUNTER );
							me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
							return;
						}
					}
				}

				// look for free hostages - CT's have radar so they know where hostages are at all times
				CHostage *hostage = me->GetGameState()->GetNearestFreeHostage();

				// if we are not allowed to do the scenario, guard the hostages to clear the area for the human(s)
				if (!me->IsDoingScenario())
				{
					if (hostage)
					{		
						CNavArea *area = TheNavMesh->GetNearestNavArea( GetCentroid( hostage ) );
						if (area)
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area );
							me->PrintIfWatched( "I'm securing the hostages for a human to rescue\n" );
							return;
						}
					}

					me->Hunt();
					return;
				}

				bool fetchHostages = false;
				bool rescueHostages = false;
				const CCSBotManager::Zone *zone = nullptr;
				me->SetGoalEntity( nullptr );

				// if we are escorting hostages, determine where to take them
				if (me->GetHostageEscortCount())
					zone = TheCSBots()->GetClosestZone( me->GetLastKnownArea(), PathCost( me, FASTEST_ROUTE ) );

				// if we are escorting hostages and there are more hostages to rescue, 
				// determine whether it's faster to rescue the ones we have, or go get the remaining ones
				if ( zone && !mp_hostages_moveable.GetBool() ) // We can only carry one hostage at a time so go ahead and rescue the one we have
				{
					rescueHostages = true;
				}
				else if (hostage)
				{
					Vector hostageOrigin = GetCentroid( hostage );

					if (zone)
					{
						PathCost cost( me, FASTEST_ROUTE );
						float toZone = NavAreaTravelDistance( me->GetLastKnownArea(), zone->m_area[0], cost  );
						float toHostage = NavAreaTravelDistance( me->GetLastKnownArea(), TheNavMesh->GetNearestNavArea( GetCentroid( hostage ) ), cost );

						if (toHostage < 0.0f)
						{
							rescueHostages = true;
						}
						else
						{
							if (toZone < toHostage)
								rescueHostages = true;
							else
								fetchHostages = true;
						}
					}
					else
					{
						fetchHostages = true;
					}
				}
				else if (zone)
				{
					rescueHostages = true;
				}

				if (fetchHostages)
				{
					// go get hostages
					me->SetTask( CCSBot::COLLECT_HOSTAGES );
					me->Run();
					me->SetGoalEntity( hostage );
					me->ResetWaitForHostagePatience();

					// if we already have some hostages, move to the others by the quickest route
					RouteType route = (me->GetHostageEscortCount()) ? FASTEST_ROUTE : SAFEST_ROUTE;
					me->MoveTo( GetCentroid( hostage ), route );

					me->PrintIfWatched( "I'm collecting hostages\n" );
					return;
				}

				const Vector *zonePos = TheCSBots()->GetRandomPositionInZone( zone );
				if (rescueHostages && zonePos)
				{
					me->SetTask( CCSBot::RESCUE_HOSTAGES );
					me->Run();
					me->SetDisposition( CCSBot::SELF_DEFENSE );
					me->MoveTo( *zonePos, FASTEST_ROUTE );
					me->PrintIfWatched( "I'm rescuing hostages\n" );
					me->GetChatter()->EscortingHostages();
					return;
				}
			}
			break;
		}

		default:	// deathmatch
		{
			// if we just spawned, cheat and make us aware of other players so players can't spawncamp us effectively
			if ( me->m_spawnedTime - gpGlobals->curtime < 1.0f )
			{
				std::vector< CCSPlayer * > playerVector;
				CollectPlayers( &playerVector, TEAM_ANY, COLLECT_ONLY_LIVING_PLAYERS );

				for( int i=0; i<std::ssize( playerVector ); ++i )
				{
					if ( me->entindex() == playerVector[i]->entindex() )
					{
						continue;
					}

					me->OnAudibleEvent( nullptr, playerVector[i], 9999999.9f, PRIORITY_HIGH, true );
				}
			}

			// sniping check
			if (me->GetFriendsRemaining() && me->IsSniper() && RandomFloat( 0, 100.0f ) < offenseSniperCampChance)
			{
				me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
				me->Hide( me->GetLastKnownArea(), RandomFloat( 10.0f, 30.0f ), sniperHideRange );
				me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
				me->PrintIfWatched( "Sniping!\n" );
				return;
			}
			break;
		}
	}

	// if we have nothing special to do, go hunting for enemies
	me->Hunt();
}

class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CNavArea;

using namespace ::ks::reflect;

// Purpose: Idle update function for coop mode. Split into different file for clarity

ConVar bot_coop_force_throw_grenade_chance( side_registry(), "bot_coop_force_throw_grenade_chance", "0.3", FCVAR_CHEAT );

bool ReactToBombState( CCSBot *me )
{
	const Vector* pVecPos = me->GetGameState()->GetBombPosition();
	if ( !pVecPos )
		return false;

	if ( me->GetTeamNumber() == TEAM_TERRORIST )
	{
		if ( TheCSBots()->IsBombPlanted() )
			me->SetTask( CCSBot::GUARD_TICKING_BOMB );
		else if ( me->GetGameState()->IsBombLoose() )
		{
			me->FetchBomb();
			return true;
		}
		else
			return false;
	}
	else if ( me->GetTeamNumber() == TEAM_CT )
	{
		if ( TheCSBots()->IsBombPlanted() )
			me->SetTask( CCSBot::DEFUSE_BOMB );
		else if ( me->GetGameState()->IsBombLoose() )
			me->SetTask( CCSBot::GUARD_LOOSE_BOMB );
		/*
		else if ( me->GetGameState()->GetBombState() == CSGameState::MOVING )
		{
			float flDistToSite0 = ( *pVecPos - TheCSBots()->GetZone( 0 )->m_center ).Length2D();
			float flDistToSite1 = ( *pVecPos - TheCSBots()->GetZone( 1 )->m_center ).Length2D();
		} */
		else
			return false;
	}

	me->MoveTo( *pVecPos );
	me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

	return true;
}

// Terrorists rush the chosen site
void RunStrat_Rush( CCSBot * me, int iBombSite )
{
	if ( !ReactToBombState( me ) )
	{
		const CCSBotManager::Zone *zone = TheCSBots()->GetZone( iBombSite );
		const Vector *pos = TheCSBots()->GetRandomPositionInZone( zone );
		if ( !pos)
		{
			Warning( "ERROR: Map either has < 2 bomb sites, or one of the sites has no areas. Coop bots will be broken." );
			return;
		}

		if ( me->IsAtBombsite() )
		{
			bool bIsSafe = gpGlobals->curtime - me->GetLastSawEnemyTimestamp() > 2.0f; // TODO: Might be better to use enemy death/remaining count for this
			if ( me->HasC4() && bIsSafe )
			{
				me->SetTask( CCSBot::PLANT_BOMB );
			}
			else
			{
				Place place = TheNavMesh->GetPlace( zone->m_center );
				if ( const Vector* pVecHidingSpot = FindRandomHidingSpot( me, place, me->IsSniper() ) )
				{
					pos = pVecHidingSpot;
				}
				me->SetTask( CCSBot::GUARD_BOMB_ZONE );
			}
		}
		else
		{
			me->SetTask( CCSBot::SEEK_AND_DESTROY );
		}

		me->MoveTo( *pos );
		me->Run();
		me->PrintIfWatched( "I'm rushing the bomb site\n" );
		me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
	}
}

void RunStrat_GuardSpot( CCSBot *me )
{
	if ( !ReactToBombState( me ) )
	{
		const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
		if ( zone )
		{
			CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
			if ( area )
			{
				me->PrintIfWatched( "I'm guarding a bombsite\n" );
				me->GetChatter()->GuardingBombsite( area->GetPlace() );
				me->SetTask( CCSBot::GUARD_BOMB_ZONE );
				me->Hide( area, -1.0, 1000.0f );
				me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
			}
		}
	}
}

void MakeAwareOfCTs( CCSBot * me )
{
	std::vector< CCSPlayer * > vecCTs;
	CollectPlayers( &vecCTs, TEAM_CT, COLLECT_ONLY_LIVING_PLAYERS );

	if ( vecCTs.empty() )
		return;

	CCSPlayer *pTargetPlayer = nullptr;
	float flClosePlayer = 1e6;
	for ( CCSPlayer *pCTPlayer : vecCTs )
	{
		if ( me->IsVisible( pCTPlayer ) )
		{
			float dist = me->GetTravelDistanceToPlayer( pCTPlayer );
			if ( dist < flClosePlayer )
			{
				flClosePlayer = dist;
				pTargetPlayer = pCTPlayer;
			}
		}

		me->OnAudibleEvent( nullptr, pCTPlayer, MAX_COORD_FLOAT, PRIORITY_HIGH, true );
	}

	if ( !pTargetPlayer )
		pTargetPlayer = vecCTs[ RandomInt( 0, std::ssize( vecCTs ) - 1 ) ];

	me->SetBotEnemy( pTargetPlayer );
}

extern ConVar mp_guardian_target_site;
void IdleState::UpdateCoop( CCSBot *me )
{
	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		contract_assert( mp_guardian_target_site.GetInt() >= 0 );
		// if this is a bomb game and we have the bomb, go plant it
		if ( me->GetTeamNumber() == TEAM_TERRORIST )
		{
			switch ( TheCSBots()->GetTStrat() )
			{
				case CCSBotManager::k_ETStrat_Rush:
				{
					RunStrat_Rush( me, TheCSBots()->GetTerroristTargetSite() );
					break;
				}
			}
		}
		else if ( me->GetTeamNumber() == TEAM_CT )
		{
			switch ( TheCSBots()->GetCTStrat() )
			{
				case CCSBotManager::k_ECTStrat_StackSite:
				{
					RunStrat_GuardSpot( me );
					break;
				}
			}
		}
	}
	else if ( CSGameRules()->IsPlayingCoopMission() )
	{
		if ( me->GetTeamNumber() == TEAM_TERRORIST )
		{
			SpawnPointCoopEnemy* pSpawn = me->GetLastCoopSpawnPoint();
			if ( !pSpawn )
				return;

			// if we're holding a grenade, throw it at the victim
			if ( me->HasGrenade() && CSGameRules()->IsPlayingCooperativeGametype() && RandomFloat() < bot_coop_force_throw_grenade_chance.GetFloat() )
				me->EquipGrenade();

			// HACK: We'd like last guy alive to stop hiding and hunt down the players
			// but don't want the first spawned guy to go straight to charging... kick this clause into the future a few seconds
			// to make sure our wave is in before making this check. 
			SpawnPointCoopEnemy::BotDefaultBehavior_t behavior = pSpawn->GetDefaultBehavior();
			if ( me->GetFriendsRemaining() == 0 && gpGlobals->curtime - me->m_spawnedTime > 5.0f )
				behavior = SpawnPointCoopEnemy::CHARGE_ENEMY;

			switch ( behavior )
			{
				case SpawnPointCoopEnemy::HUNT:
				{
					me->Hunt();
					break;
				}
				case SpawnPointCoopEnemy::DEFEND_AREA:
				case SpawnPointCoopEnemy::DEFEND_INVESTIGATE:
				{
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					if ( me->IsSniper() )
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
					else
						me->SetTask( CCSBot::HOLD_POSITION );

					if ( pSpawn->HideRadius() > 0 )
					{
						if ( !me->TryToHide( pSpawn->FindNearestArea(), -1.0, pSpawn->HideRadius(), true, false, &me->GetAbsOrigin() ) )
						{
							// This spawn isn't hideable, stop trying
							pSpawn->HideRadius( 0 );
						}
					}
					else
					{
						// look around
						me->UpdateLookAround();

						if ( pSpawn->GetDefaultBehavior() == SpawnPointCoopEnemy::DEFEND_INVESTIGATE )
						{
							me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
							// listen for enemy noises
							if ( me->HeardInterestingNoise() )
							{
								me->InvestigateNoise();
								pSpawn->SetDefaultBehavior( SpawnPointCoopEnemy::HUNT );
							}
						}
					}
					break;
				}
				case SpawnPointCoopEnemy::CHARGE_ENEMY:
				{
					MakeAwareOfCTs( me );

					me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
					
					if ( me->GetBotEnemy() )
					{
						me->SetTask( CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION, me->GetBotEnemy() );
						me->MoveTo( me->GetBotEnemy()->GetAbsOrigin() );
					}
					
					break;
				}
			}
		}
	}
}

class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Move towards currently heard noise
 */
void InvestigateNoiseState::AttendCurrentNoise( CCSBot *me )
{
	if (!me->IsNoiseHeard() && me->GetNoisePosition())
		return;

	// remember where the noise we heard was
	m_checkNoisePosition = *me->GetNoisePosition();

	// tell our teammates (unless the noise is obvious, like gunfire)
	if (me->IsWellPastSafe() && me->HasNotSeenEnemyForLongTime() && me->GetNoisePriority() != PRIORITY_HIGH)
		me->GetChatter()->HeardNoise( *me->GetNoisePosition() );

	// figure out how to get to the noise		
	me->PrintIfWatched( "Attending to noise...\n" );
	me->ComputePath( m_checkNoisePosition, FASTEST_ROUTE );

	const float minAttendTime = 3.0f;
	const float maxAttendTime = 10.0f;
	m_minTimer.Start( RandomFloat( minAttendTime, maxAttendTime ) );

	// consume the noise
	me->ForgetNoise();
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnEnter( CCSBot *me )
{
	AttendCurrentNoise( me );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * @todo Use TravelDistance instead of distance...
 */
void InvestigateNoiseState::OnUpdate( CCSBot *me )
{
	Vector myOrigin = GetCentroid( me );

	// keep an ear out for closer noises...
	if (m_minTimer.IsElapsed())
	{
		const float nearbyRange = 500.0f;
		if (me->HeardInterestingNoise() && me->GetNoiseRange() < nearbyRange)
		{
			// new sound is closer
			AttendCurrentNoise( me );
		}
	}

	// if the pathfind fails, give up
	if (!me->HasPath())
	{
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround();

	// get distance remaining on our path until we reach the source of the noise
	float range = me->GetPathDistanceRemaining();

	if (me->IsUsingKnife())
	{
		if (me->IsHurrying())
			me->Run();
		else
			me->Walk();
	}
	else
	{
		const float closeToNoiseRange = 1500.0f;
		if (range < closeToNoiseRange)
		{
			// if we dont have many friends left, or we are alone, and we are near noise source, sneak quietly
			if ((me->GetNearbyFriendCount() == 0 || me->GetFriendsRemaining() <= 2) && !me->IsHurrying())
			{
				me->Walk();
			}
			else
			{
				me->Run();
			}
		}
		else
		{
			me->Run();
		}
	}

	// if we can see the noise position and we're close enough to it and looking at it, 
	// we don't need to actually move there (it's checked enough)
	const float closeRange = 500.0f;
	if (range < closeRange)
	{
		if (me->IsVisible( m_checkNoisePosition, true ))
		{
			// can see noise position
			me->PrintIfWatched( "Noise location is clear.\n" );
			me->ForgetNoise();
			me->Idle();
			return;
		}
	}

	// move towards noise
	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		me->Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnExit( CCSBot *me )
{
	// reset to run mode in case we were sneaking about
	me->Run();
}

class CBasePlayer;
class CCSBot;
class CCSBotManager;
class CHostage;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

extern ConVar mp_hostages_moveable;

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a potentially far away position.
 */
void MoveToState::OnEnter( CCSBot *me )
{
	if ( ( me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying() ) || 
		( me->HasHeavyArmor() && me->GetBotEnemy() ) )
	{
		me->Walk();
	}
	else
	{
		me->Run();
	}

	// if we need to find the bomb, get there as quick as we can
	RouteType route;
	switch (me->GetTask())
	{
		case CCSBot::FIND_TICKING_BOMB:
		case CCSBot::DEFUSE_BOMB:
		case CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION:
			route = FASTEST_ROUTE;
			break;

		default:
			route = SAFEST_ROUTE;
			break;
	}
		
	// build path to, or nearly to, goal position
	me->ComputePath( m_goalPosition, route );

	m_radioedPlan = false;
	m_askedForCover = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a potentially far away position.
 */
void MoveToState::OnUpdate( CCSBot *me )
{
	Vector myOrigin = GetCentroid( me );

	// assume that we are paying attention and close enough to know our enemy died
	if (me->GetTask() == CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION)
	{
		/// @todo Account for reaction time so we take some time to realized the enemy is dead
		CBasePlayer *victim = static_cast<CBasePlayer *>( me->GetTaskEntity() );
		if (victim == nullptr || !victim->IsAlive())
		{
			me->PrintIfWatched( "The enemy I was chasing was killed - giving up.\n" );
			me->Idle();
			return;
		}
	}

	// look around
	me->UpdateLookAround();

	//
	// Scenario logic
	//
	switch (TheCSBots()->GetScenario())
	{
		case CCSBotManager::SCENARIO_DEFUSE_BOMB:
		{
			// if the bomb has been planted, find it
			// NOTE: This task is used by both CT and T's to find the bomb
			if (me->GetTask() == CCSBot::FIND_TICKING_BOMB)
			{
				if (!me->GetGameState()->IsBombPlanted())
				{
					// the bomb is not planted - give up this task
					me->Idle();
					return;
				}

				if (me->GetGameState()->GetPlantedBombsite() != CSGameState::UNKNOWN)
				{
					// we know where the bomb is planted, stop searching
					me->Idle();
					return;
				}

				// check off bombsites that we explore or happen to stumble into
				for( int z=0; z<TheCSBots()->GetZoneCount(); ++z )
				{
					// don't re-check zones
					if (me->GetGameState()->IsBombsiteClear( z ))
						continue;

					if (TheCSBots()->GetZone(z)->m_extent.Contains( myOrigin ))
					{
						// note this bombsite is clear
						me->GetGameState()->ClearBombsite( z );

						if (me->GetTeamNumber() == TEAM_CT)
						{
							// tell teammates this bombsite is clear
							me->GetChatter()->BombsiteClear( z );
						}

						// find another zone to check
						me->Idle();

						return;
					}
				}

				// move to a bombsite
				break;
			}

			if (me->GetTeamNumber() == TEAM_CT)
			{
				if (me->GetGameState()->IsBombPlanted())
				{
					switch( me->GetTask() )
					{
						case CCSBot::DEFUSE_BOMB:
						{	
							// if we are near the bombsite and there is time left, sneak in (unless all enemies are dead)
							if (me->GetEnemiesRemaining())
							{
								const float plentyOfTime = 15.0f;
								if (TheCSBots()->GetBombTimeLeft() > plentyOfTime)
								{
									// get distance remaining on our path until we reach the bombsite
									float range = me->GetPathDistanceRemaining();

									const float closeRange = 1500.0f;
									if (range < closeRange)
									{
										me->Walk();
									}
									else
									{
										me->Run();
									}
								}
							}
							else
							{
								// everyone is dead - run!
								me->Run();
							}

							// if we are trying to defuse the bomb, and someone has started defusing, guard them instead
							if (me->CanSeePlantedBomb() && TheCSBots()->GetBombDefuser())
							{
								me->GetChatter()->Say( "CoveringFriend" );
								me->Idle();
								return;
							}

							// if we are near the bomb, defuse it (if we are reloading, don't try to defuse until we finish)
							const Vector *bombPos = me->GetGameState()->GetBombPosition();
							if (bombPos && !me->IsReloading())
							{
								if ((*bombPos - me->EyePosition()).IsLengthLessThan( 72 ) && ( me->EyePosition().AsVector2D().DistTo( bombPos->AsVector2D() ) < 48 ))
								{
									// make sure we can see the bomb
									if (me->IsVisible( *bombPos ))
									{
										me->DefuseBomb();
										return;
									}
								}
							}

							break;
						}

						default:
						{
							// we need to find the bomb
							me->Idle();
							return;
						}
					}
				}
			}
			else		// TERRORIST
			{
				if (me->GetTask() == CCSBot::PLANT_BOMB )
				{
					if (  me->GetFriendsRemaining() )
					{
						// if we are about to plant, radio for cover
						if (!m_askedForCover)
						{
							const float nearPlantSite = 50.0f;
							if (me->IsAtBombsite() && me->GetPathDistanceRemaining() < nearPlantSite)
							{
								// radio to the team
								me->GetChatter()->PlantingTheBomb( me->GetPlace() );
								m_askedForCover = true;
							}

							// after we have started to move to the bombsite, tell team we're going to plant, and where
							// don't do this if we have already radioed that we are starting to plant
							if (!m_radioedPlan)
							{
								const float radioTime = 2.0f;
								if (gpGlobals->curtime - me->GetStateTimestamp() > radioTime)
								{
									// radio to the team if we're more than 10 seconds (2400 units) out
									const float nearPlantSite = 2400.0f;
									if ( me->GetPathDistanceRemaining() >= nearPlantSite )
									{
										me->GetChatter()->GoingToPlantTheBomb( TheNavMesh->GetPlace( m_goalPosition ) );
									}
									m_radioedPlan = true;
								}
							}
						}
					}
				}
			}
			break;
		}

		//--------------------------------------------------------------------------------------------------
		case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
		{
			if (me->GetTask() == CCSBot::COLLECT_HOSTAGES)
			{
				//
				// Since CT's have a radar, they can directly look at the actual hostage state
				//

				// check if someone else collected our hostage, or the hostage died or was rescued
				CHostage *hostage = static_cast<CHostage *>( me->GetGoalEntity() );
				if (hostage == nullptr || !hostage->IsValid() || hostage->IsFollowingSomeone() )
				{
					me->Idle();
					return;
				}

				Vector hostageOrigin = GetCentroid( hostage );

				// if our hostage has moved, repath
				const float repathToleranceSq = 75.0f * 75.0f;
				float error = (hostageOrigin - m_goalPosition).LengthSqr();
				if (error > repathToleranceSq)
				{
					m_goalPosition = hostageOrigin;
					me->ComputePath( m_goalPosition, SAFEST_ROUTE );
				}

				/// @todo Generalize ladder priorities over other tasks
				if (!me->IsUsingLadder())
				{
					Vector pos = hostage->EyePosition();
					Vector to = pos - me->EyePosition(); // "Use" checks from eye position, so we should too

					// look at the hostage as we approach
					const float watchHostageRange = 100.0f;
					if (to.IsLengthLessThan( watchHostageRange ))
					{
						me->SetLookAt( "Hostage", pos, PRIORITY_LOW, 0.5f );

						// randomly move just a bit to avoid infinite use loops from bad hostage placement
						NavRelativeDirType dir = (NavRelativeDirType)RandomInt( 0, 3 );
						switch( dir )
						{
							case LEFT:		me->StrafeLeft(); break;
							case RIGHT:		me->StrafeRight(); break;
							case FORWARD:	me->MoveForward(); break;
							case BACKWARD:	me->MoveBackward(); break;
						}

						// check if we are close enough to the hostage to talk to him
						const float useRange = PLAYER_USE_RADIUS - 10.0f; // shave off a fudge factor to make sure we're within range
						if (to.IsLengthLessThan( useRange ))
						{
							if ( !mp_hostages_moveable.GetBool() )
							{
								//me->PickupHostage( me->GetGoalEntity() );

								bool bBeingRescued = false;

								CHostage *hostage = static_cast<CHostage*>( me->GetGoalEntity() );
								if ( hostage && hostage->GetHostageState() != k_EHostageStates_GettingPickedUp &&
									 hostage->IsFollowingSomeone() == false && me->GetNearbyFriendCount() > 0 )
								{
									// see if one of my friends if picking up this hostage
									for ( int i = 1; i <= gpGlobals->maxClients; ++i )
									{
										CCSBot *player = ToCSBot( UTIL_PlayerByIndex( i ) );

										if ( player == nullptr || !player->IsAlive() ||
											 me->IsOtherEnemy( player ) || player->entindex() == me->entindex() )
											 continue;

										if ( player->IsPickingupHostage() )
										{
											bBeingRescued = true;
											break;
										}
									}
								}

								// if not, pick it up
								if ( bBeingRescued == false )
									me->PickupHostage( me->GetGoalEntity() );
								else
								{
									if ( hostage && me->IsVisible( hostage->GetAbsOrigin(), false, nullptr ) )
									{
										// if we can see the hostage, guard it
										me->GetChatter()->Say( "CoveringFriend" );
										me->Idle();
									}
								}
							}
							else
								me->UseEntity( me->GetGoalEntity() );					
							return;
						}
					}
				}
			}
			else if (me->GetTask() == CCSBot::RESCUE_HOSTAGES)
			{
				// periodically check if we lost all our hostages
				if (me->GetHostageEscortCount() == 0)
				{
					// lost our hostages - go get 'em
					me->Idle();
					return;
				}
			}

			break;
		}
	}

	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		// reached destination
		switch( me->GetTask() )
		{
			case CCSBot::PLANT_BOMB:
				// if we are at bombsite with the bomb, plant it
				if (me->IsAtBombsite() && me->HasC4())
				{
					me->PlantBomb();
					return;
				}
				break;
		
			case CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION:
			{
				CBasePlayer *victim = static_cast<CBasePlayer *>( me->GetTaskEntity() );
				if (victim && victim->IsAlive())
				{
					// if we got here and haven't re-acquired the enemy, we lost him
					BotStatement *say = new BotStatement( me->GetChatter(), REPORT_ENEMY_LOST, 8.0f );

					say->AppendPhrase( TheBotPhrases->GetPhrase( "LostEnemy" ) );
					say->SetStartTime( gpGlobals->curtime + RandomFloat( 3.0f, 5.0f ) );

					me->GetChatter()->AddStatement( say );
				}
				break;
			}
		}

		// default behavior when destination is reached
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void MoveToState::OnExit( CCSBot *me )
{
	// reset to run in case we were walking near our goal position
	me->Run();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	//me->StopAiming();
}

class CBaseDoor;
class CBaseEntity;
class CBasePropDoor;
class CCSBot;
class CTakeDamageInfo;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), April 2005

//-------------------------------------------------------------------------------------------------
/**
 * Face the door and open it.
 * NOTE: This state assumes we are standing in range of the door to be opened, with no obstructions.
 */
void OpenDoorState::OnEnter( CCSBot *me )
{
	m_isDone = false;
	m_timeout.Start( 0.5f );
}

//-------------------------------------------------------------------------------------------------
void OpenDoorState::SetDoor( CBaseEntity *door )
{
	if ( door == nullptr )
		return;

	if ( FClassnameIs( door, "func_door*" ) || FClassnameIs( door, "func_water" ) )
	{
		m_funcDoor = static_cast< CBaseDoor * >( door );
		return;
	}

	if ( FClassnameIs( door, "prop_door*" ) )
	{
		m_propDoor = static_cast< CBasePropDoor * >( door );
		return;
	}
}

//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnUpdate( CCSBot *me )
{
	me->ResetStuckMonitor();

	// look at the door
	Vector pos;
	bool isDoorMoving = false;
	CBaseEntity *door = nullptr;

	if ( m_funcDoor.Get() )
	{
		door = m_funcDoor;
		isDoorMoving = m_funcDoor->m_toggle_state == TS_GOING_UP || m_funcDoor->m_toggle_state == TS_GOING_DOWN;
	}
	else if ( m_propDoor.Get() )
	{
		door = m_propDoor;
		isDoorMoving = m_propDoor->IsDoorOpening() || m_propDoor->IsDoorClosing();

		// "prop_door_rotating" is CPropDoorRotatingBreakable
		CPropDoorRotatingBreakable *pPropDoor = FClassnameIs( door, "prop_door_rotating" ) ? static_cast< CPropDoorRotatingBreakable* >( door ) : nullptr;
		if ( pPropDoor && pPropDoor->IsDoorLocked() && pPropDoor->IsBreakable() == false )
		{
			m_isDone = true;
			return;
		}
	}

	// wait for door to swing open before leaving state
	if ( isDoorMoving || !door )
	{
		m_isDone = true;
		return;
	}

	me->SetLookAt( "Open door", door->WorldSpaceCenter(), PRIORITY_UNINTERRUPTABLE );

	// if we are looking at the door, "use" it and exit
	if ( me->IsLookingAtPosition( door->WorldSpaceCenter() ) )
	{
		if ( m_timeout.IsElapsed() )
		{
			// possibly stuck - blow the damn door away!
			me->PrimaryAttack();

			if ( door )
			{
				contract_assert( ( door->GetHealth() > 2 ) && "Bot is stuck on a door and is going to destroy it to get free!\n" );

				CTakeDamageInfo damageInfo( me, me, 2.0f, DMG_GENERIC );
				door->TakeDamage( damageInfo );
			}

		}

		// we are looking at it - use it directly to avoid complications
		door->Use( me, me, USE_ON, 0.0f );
	}
}

//-------------------------------------------------------------------------------------------------
void OpenDoorState::OnExit( CCSBot *me )
{
	me->ClearLookAt();
	me->ResetStuckMonitor();
}

class CCSBot;
class CHostage;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PickupHostageState::OnEnter( CCSBot *me )
{
	me->Crouch();
	me->SetDisposition( CCSBot::SELF_DEFENSE );

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PickupHostageState::OnUpdate( CCSBot *me )
{
	const float timeout = 7.0f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > timeout)
	{
		// find a new spot
		me->Idle();
		return;
	}

	// look at the entity
	Vector pos = m_entity->EyePosition();
	me->SetLookAt( "Use entity", pos, PRIORITY_HIGH );

	// if we are looking at the entity, "use" it and exit
	if (me->IsLookingAtPosition( pos ))
	{
		me->UseEnvironment();
	}

	CHostage *hostage = static_cast<CHostage *>( m_entity.Get() );

	if ( hostage && hostage->IsFollowingSomeone() )
	{
		me->Idle();
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void PickupHostageState::OnExit( CCSBot *me )
{
	// equip our rifle (in case we were interrupted while holding C4)
	me->EquipBestWeapon();
	me->StandUp();
	me->ResetStuckMonitor();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->ClearLookAt();

	CHostage *hostage = static_cast<CHostage *>( m_entity.Get() );

	if ( hostage && hostage->IsFollowing( me ) )
		me->IncreaseHostageEscortCount();
}

class CBaseCombatWeapon;
class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PlantBombState::OnEnter( CCSBot *me )
{
	me->Crouch();
	me->SetDisposition( CCSBot::SELF_DEFENSE );

	// look at the floor
//	Vector down( myOrigin.x, myOrigin.y, -1000.0f );

	float yaw = me->EyeAngles().y;
	Vector2D dir( BotCOS(yaw), BotSIN(yaw) );
	Vector myOrigin = GetCentroid( me );

	Vector down( myOrigin.x + 10.0f * dir.x, myOrigin.y + 10.0f * dir.y, me->GetFeetZ() );
	me->SetLookAt( "Plant bomb on floor", down, PRIORITY_HIGH );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Plant the bomb.
 */
void PlantBombState::OnUpdate( CCSBot *me )
{
	// can't be stuck while planting
	me->ResetStuckMonitor();

	CBaseCombatWeapon *gun = me->GetActiveWeapon();
	bool holdingC4 = false;
	if (gun)
	{
		if (FStrEq( gun->GetClassname(), "weapon_c4" ))
			holdingC4 = true;
	}

	// if we aren't holding the C4, grab it, otherwise plant it
	if ( holdingC4 )
	{
		me->PrimaryAttack();

		CC4 *pC4 = static_cast< CC4 * >( gun );	// holdingC4 implies gun is the "weapon_c4" entity
		if ( pC4 && !pC4->m_bStartedArming && gpGlobals->curtime - me->GetStateTimestamp() > 2.0f )
		{
			// can't plant here for some reason - try another spot
			me->Idle();
			return;
		}
	}
	else
	{
		me->SelectItem( "weapon_c4" );
	}

	// if we no longer have the C4, we've successfully planted
	if (!me->HasC4())
	{
		// move to a hiding spot and watch the bomb
		me->SetTask( CCSBot::GUARD_TICKING_BOMB );
		me->Hide();
	}

	// if we time out, it's because we slipped into a non-plantable area
	const float timeout = 5.0f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > timeout)
	{
		// find a new spot
		me->Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
void PlantBombState::OnExit( CCSBot *me )
{
	// equip our rifle (in case we were interrupted while holding C4)
	me->EquipBestWeapon();
	me->StandUp();
	me->ResetStuckMonitor();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->ClearLookAt();
}

class CCSBot;
class CCSBotManager;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

/**
 * Face the entity and "use" it
 * NOTE: This state assumes we are standing in range of the entity to be used, with no obstructions.
 */
void UseEntityState::OnEnter( CCSBot *me )
{
}

void UseEntityState::OnUpdate( CCSBot *me )
{
	// in the case when an entity we were supposed to +use is deleted we shouldn't try to
	// dereference a NULL pointer and should just go to a higher level objective
	if ( !m_entity.Get() )
	{
		me->Idle();
		return;
	}

	// in the very rare situation where two or more bots "used" a hostage at the same time,
	// one bot will fail and needs to time out of this state
	const float useTimeout = 5.0f;
	if (me->GetStateTimestamp() - gpGlobals->curtime > useTimeout)
	{
		me->Idle();
		return;
	}

	// look at the entity
	Vector pos = m_entity->EyePosition();
	me->SetLookAt( "Use entity", pos, PRIORITY_HIGH );

	// if we are looking at the entity, "use" it and exit
	if (me->IsLookingAtPosition( pos ))
	{
		if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES && 
			me->GetTeamNumber() == TEAM_CT &&
			me->GetTask() == CCSBot::COLLECT_HOSTAGES)
		{
			// we are collecting a hostage, assume we were successful - the update check will correct us if we weren't
			me->IncreaseHostageEscortCount();
		}

		me->UseEnvironment();
		me->Idle();
	}
}

void UseEntityState::OnExit( CCSBot *me )
{
	me->ClearLookAt();
	me->ResetStuckMonitor();
}
