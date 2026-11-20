// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <time.h>
#include "edict.h"
#include "igameevents.h"
#include "networkvar.h"
#include "eiface.h"
#include "reflect.h"
#include "tier2/tier2_logging.h"
#include "cs_bot_manager.h"
module ks.server;

import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :baseentity;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.vprof;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import :entity_2;
import std;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.platform;
import ks.core.types;
import ks.math.ssemath;
import ks.tier1.cvar;
import ks.tier1.fmtstr;
import ks.tier1.icvar;
import ks.tier1.keyvalues;
import ks.tier1.vector_ops;

class CBasePlayer;
class CCSBot;
class CNavArea;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * Checks if the bot can hear the event
 */
void CCSBot::OnAudibleEvent( IGameEvent *event, CBasePlayer *player, float range, PriorityType priority, bool isHostile, bool isFootstep, const Vector *actualOrigin )
{
	/// @todo Listen to non-player sounds
	if (player == nullptr)
		return;

	// don't pay attention to noise that friends make (unless it is a decoy)
	if ( !IsEnemy( player ) )
	{
		if ( !event || !FStrEq( event->GetName(), "decoy_firing" ) )
			return;
	}

	Vector playerOrigin = GetCentroid( player );
	Vector myOrigin = GetCentroid( this );

	// If the event occurs far from the triggering player, it may override the origin
	if ( actualOrigin )
	{
		playerOrigin = *actualOrigin;
	}

	// check if noise is close enough for us to hear
	const Vector *newNoisePosition = &playerOrigin;
	float newNoiseDist = (myOrigin - *newNoisePosition).Length();
	if (newNoiseDist < range)
	{
		// we heard the sound
		if ((IsLocalPlayerWatchingMe() && cv_bot_debug.GetInt() == 3) || cv_bot_debug.GetInt() == 4)
		{
			PrintIfWatched( "Heard noise (%s from %s, pri %s, time %3.1f)\n", 
											(FStrEq( "weapon_fire", event ? event->GetName() : "<no event>" )) ? "Weapon fire " : "",
											(player) ? player->GetPlayerName() : "NULL",
											(priority == PRIORITY_HIGH) ? "HIGH" : ((priority == PRIORITY_MEDIUM) ? "MEDIUM" : "LOW"),
											gpGlobals->curtime );
		}

		// should we pay attention to it
		// if noise timestamp is zero, there is no prior noise
		if (m_noiseTimestamp > 0.0f)
		{
			// only overwrite recent sound if we are louder (closer), or more important - if old noise was long ago, its faded
			const float shortTermMemoryTime = 3.0f;
			if (gpGlobals->curtime - m_noiseTimestamp < shortTermMemoryTime)
			{
				// prior noise is more important - ignore new one
				if (priority < m_noisePriority)
					return;

				float oldNoiseDist = (myOrigin - m_noisePosition).Length();
				if (newNoiseDist >= oldNoiseDist)
					return;
			}
		}

		// find the area in which the noise occured
		/// @todo Better handle when noise occurs off the nav mesh
		/// @todo Make sure noise area is not through a wall or ceiling from source of noise
		/// @todo Change GetNavTravelTime to better deal with NULL destination areas
		CNavArea *noiseArea = TheNavMesh->GetNearestNavArea( *newNoisePosition );
		if (noiseArea == nullptr)
		{
			PrintIfWatched( "  *** Noise occurred off the nav mesh - ignoring!\n" );
			return;
		}

		m_noiseArea = noiseArea;

		// remember noise priority
		m_noisePriority = priority;

		// randomize noise position in the area a bit - hearing isn't very accurate
		// the closer the noise is, the more accurate our placement
		/// @todo Make sure not to pick a position on the opposite side of ourselves.
		const float maxErrorRadius = 400.0f;
		const float maxHearingRange = 2000.0f;
		float errorRadius = maxErrorRadius * newNoiseDist/maxHearingRange;

		m_noisePosition.x = newNoisePosition->x + RandomFloat( -errorRadius, errorRadius );
		m_noisePosition.y = newNoisePosition->y + RandomFloat( -errorRadius, errorRadius );

		// note the *travel distance* to the noise
		// EDIT: use straight line distance for now; the A* calc is really expensive
		m_noiseTravelDistance = EyePosition().DistTo( player->EyePosition() );

		// make sure noise position remains in the same area
		m_noiseArea->GetClosestPointOnArea( m_noisePosition, &m_noisePosition );

		// note when we heard the noise
		m_noiseTimestamp = gpGlobals->curtime;

		// if we hear a nearby enemy, become alert
		const float nearbyNoiseRange = 1000.0f;
		if (m_noiseTravelDistance < nearbyNoiseRange && m_noiseTravelDistance > 0.0f)
		{
			BecomeAlert();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHEGrenadeDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 99999.0f, PRIORITY_HIGH, true ); // hegrenade_detonate
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnFlashbangDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1000.0f, PRIORITY_LOW, true ); // flashbang_detonate
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnSmokeGrenadeDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1000.0f, PRIORITY_LOW, true ); // smokegrenade_detonate
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnMolotovDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 99999.0f, PRIORITY_HIGH, true ); // molotov_detonate
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnDecoyDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 99999.0f, PRIORITY_HIGH, true ); // decoy_detonate
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnDecoyFiring( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *thrower = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( thrower == this )
		return;

	Vector decoySpot( event->GetInt( "x" ), event->GetInt( "y" ), event->GetInt( "z" ) );

	OnAudibleEvent( event, thrower, 99999.0f, PRIORITY_HIGH, true, false, &decoySpot );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnGrenadeBounce( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 500.0f, PRIORITY_LOW, true ); // grenade_bounce
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBulletImpact( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// Construct an origin for the sound, since it can be far from the originating player
	Vector actualOrigin;
	actualOrigin.x = event->GetFloat( "x", 0.0f );
	actualOrigin.y = event->GetFloat( "y", 0.0f );
	actualOrigin.z = event->GetFloat( "z", 0.0f );

	/// @todo Ignoring bullet impact events for now - we dont want bots to look directly at them!
	//OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true, false, &actualOrigin ); // bullet_impact
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBreakProp( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true ); // break_prop
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBreakBreakable( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true ); // break_glass
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnDoorMoving( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, false ); // door_moving
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHostageFollows( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// player_follows needs a player
	if (player == nullptr)
		return;

	// don't pay attention to noise that friends make
	if (!IsEnemy( player ))
		return;

	Vector playerOrigin = GetCentroid( player );
	Vector myOrigin = GetCentroid( this );
	const float range = 1200.0f;

	// this is here so T's not only act on the noise, but look at it, too
	if (GetTeamNumber() == TEAM_TERRORIST)
	{
		// make sure we can hear the noise
		if ((playerOrigin - myOrigin).IsLengthGreaterThan( range ))
			return;

		// tell our teammates that the hostages are being taken
		GetChatter()->HostagesBeingTaken();

		// only move if we hear them being rescued and can't see any hostages
		if (GetGameState()->GetNearestVisibleFreeHostage() == nullptr)
		{			
			// since we are guarding the hostages, presumably we know where they are
			// if we're close enough to "hear" this event, either go to where the event occured,
			// or head for an escape zone to head them off
			if (GetTask() != CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
			{
				//const float headOffChance = 33.3f;
				// head them off at a rescue zone
				if (GuardRandomZone())
				{
					SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					PrintIfWatched( "Trying to beat them to an escape zone!\n" );
				}
			}
		}
	}
	else
	{
		// CT's don't care about this noise
		return;
	}

	OnAudibleEvent( event, player, range, PRIORITY_MEDIUM, false ); // hostage_follows
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnRoundEnd( IGameEvent *event )
{
	// Morale adjustments happen even for dead players
	int winner = event->GetInt( "winner" );
	switch ( winner )
	{
	case WINNER_TER:
		if (GetTeamNumber() == TEAM_CT)
		{
			DecreaseMorale();
		}
		else
		{
			IncreaseMorale();
		}
		break;

	case WINNER_CT:
		if (GetTeamNumber() == TEAM_CT)
		{
			IncreaseMorale();
		}
		else
		{
			DecreaseMorale();
		}
		break;

	default:
		break;
	}

	m_gameState.OnRoundEnd( event );

	if ( !IsAlive() )
		return;

	if ( event->GetInt( "winner" ) == WINNER_TER )
	{
		if (GetTeamNumber() == TEAM_TERRORIST)
			GetChatter()->CelebrateWin();
	}
	else if ( event->GetInt( "winner" ) == WINNER_CT )
	{
		if (GetTeamNumber() == TEAM_CT)
			GetChatter()->CelebrateWin();
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnRoundStart( IGameEvent *event )
{
	m_gameState.OnRoundStart( event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHostageRescuedAll( IGameEvent *event )
{
	m_gameState.OnHostageRescuedAll( event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnNavBlocked( IGameEvent *event )
{
	if ( event->GetBool( "blocked" ) )
	{
		unsigned int areaID = event->GetInt( "area" );
		if ( areaID )
		{
			// An area was blocked off.  Reset our path if it has this area on it.
			for( int i=0; i<m_pathLength; ++i )
			{
				const ConnectInfo *info = &m_path[ i ];
				if ( info->area && info->area->GetID() == areaID )
				{
					DestroyPath();
					return;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when bot enters a nav area
 */
void CCSBot::OnEnteredNavArea( CNavArea *newArea )
{

	// assume that we "clear" an area of enemies when we enter it
	newArea->SetClearedTimestamp( GetTeamNumber()-1 );

	// if we just entered a 'stop' area, set the flag
	if ( newArea->GetAttributes() & NAV_MESH_STOP )
	{
		m_isStopping = true;
	}

	/// @todo Flag these areas as spawn areas during load
	if (IsAtEnemySpawn())
	{
		m_hasVisitedEnemySpawn = true;
	}
}

class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSBotManager;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombPickedUp( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );

	// In guardian mode, terrorists always know who the bomber is
	if ( CSGameRules()->IsPlayingCoopGuardian() && GetTeamNumber() == TEAM_TERRORIST )
	{
		GetGameState()->UpdateBomber( player->GetAbsOrigin() );
	}

	// don't react to our own events
	if ( player == this )
		return;

	if (GetTeamNumber() == TEAM_CT && player)
	{
		// check if we're close enough to hear it
		const float bombPickupHearRangeSq = 1000.0f * 1000.0f;
		Vector myOrigin = GetCentroid( this );

		if ((myOrigin - player->GetAbsOrigin()).LengthSqr() < bombPickupHearRangeSq)
		{
			GetChatter()->TheyPickedUpTheBomb();
			GetGameState()->UpdateBomber( player->GetAbsOrigin() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombPlanted( IGameEvent *event )
{
	m_gameState.OnBombPlanted( event );

	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// if we're a TEAM_CT, forget what we're doing and go after the bomb
	if (GetTeamNumber() == TEAM_CT)
	{
		Idle();
	}

	// if we are following someone, stop following
	if (IsFollowing())
	{
		StopFollowing();
		Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombBeep( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	CBaseEntity *entity = UTIL_EntityByIndex( event->GetInt( "entindex" ) );
	Vector myOrigin = GetCentroid( this );

	// if we don't know where the bomb is, but heard it beep, we've discovered it
	if (GetGameState()->IsPlantedBombLocationKnown() == false && entity)
	{
		// check if we're close enough to hear it
		const float bombBeepHearRangeSq = 1500.0f * 1500.0f;
		if ((myOrigin - entity->GetAbsOrigin()).LengthSqr() < bombBeepHearRangeSq)
		{
			// radio the news to our team
			if (GetTeamNumber() == TEAM_CT && GetGameState()->GetPlantedBombsite() == CSGameState::UNKNOWN)
			{
				const CCSBotManager::Zone *zone = TheCSBots()->GetZone( entity->GetAbsOrigin() );
				if (zone)
					GetChatter()->FoundPlantedBomb( zone->m_index );
			}

			// remember where the bomb is
			GetGameState()->UpdatePlantedBomb( entity->GetAbsOrigin() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefuseBegin( IGameEvent *event )
{
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefused( IGameEvent *event )
{
	m_gameState.OnBombDefused( event );

	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	if (GetTeamNumber() == TEAM_CT)
	{
		if (TheCSBots()->GetBombTimeLeft() < 2.0f)
			GetChatter()->Say( "BarelyDefused" );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefuseAbort( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	PrintIfWatched( "BOMB DEFUSE ABORTED\n" );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombExploded( IGameEvent *event )
{
	m_gameState.OnBombExploded( event );
}

class CBasePlayer;
class CCSBot;
class CCSPlayer;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnPlayerDeath( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	Vector playerOrigin = (player) ? GetCentroid( player ) : Vector( 0, 0, 0 );

	CBasePlayer *other = UTIL_PlayerByUserId( event->GetInt( "attacker" ) );
	CBasePlayer *victim = player;

	CBasePlayer *killer = (other && other->IsPlayer()) ? static_cast<CBasePlayer *>( other ) : nullptr;

	// if the human player died in the single player game, tell the team
	if (CSGameRules()->IsCareer() && !victim->IsBot() && victim->GetTeamNumber() == GetTeamNumber())
	{
		GetChatter()->Say( "CommanderDown", 20.0f );
	}

	// keep track of the last player we killed
	if (killer == this)
	{
		m_lastVictimID = victim->entindex();
	}

	// react to teammate death
	if (victim->GetTeamNumber() == GetTeamNumber())
	{
		// note time of death
		m_friendDeathTimestamp = gpGlobals->curtime;

		// chastise friendly fire from humans
		if (killer && !killer->IsBot() && killer->GetTeamNumber() == GetTeamNumber() && killer != this)
		{
			GetChatter()->KilledFriend();
		}

		if ( CSGameRules()->IsPlayingCoopMission() )
		{
			if ( victim )
			{
				Vector vDelta = GetAbsOrigin() - victim->GetAbsOrigin();
				if ( vDelta.Length() < 320 )
					m_bIsSleeping = false;
			}
		}

		if (IsAttacking())
		{
			if ( !CSGameRules()->IsPlayingCoopMission() &&  GetTimeSinceLastSawEnemy() > 0.4f)
			{
				PrintIfWatched( "Rethinking my attack due to teammate death\n" );

				// allow us to sneak past windows, doors, etc
				IgnoreEnemies( 1.0f );

				// move to last known position of enemy - this could cause us to flank if 
				// the danger has changed due to our teammate's recent death
				SetTask( MOVE_TO_LAST_KNOWN_ENEMY_POSITION, GetBotEnemy() );
				MoveTo( GetLastKnownEnemyPosition() );
				return;
			}
		}
		else	// not attacking
		{
			//
			// If we just saw a nearby friend die, and we haven't yet acquired an enemy
			// automatically acquire our dead friend's killer
			//
			if (GetDisposition() == ENGAGE_AND_INVESTIGATE || GetDisposition() == OPPORTUNITY_FIRE)
			{
				CBasePlayer *other = UTIL_PlayerByUserId( event->GetInt( "attacker" ) );

				// check that attacker is an enemy (for friendly fire, etc)
				if (other && other->IsPlayer())
				{
					CCSPlayer *killer = static_cast<CCSPlayer *>( other );
					if (killer->GetTeamNumber() != GetTeamNumber())
					{
						// check if we saw our friend die - dont check FOV - assume we're aware of our surroundings in combat
						// snipers stay put
						if (!IsSniper() && IsVisible( playerOrigin ))
						{
							// people are dying - we should hurry
							Hurry( RandomFloat( 10.0f, 15.0f ) );

							// if we're hiding with only our knife, be a little more cautious
							const float knifeAmbushChance = 50.0f;
							if (!IsHiding() || !IsUsingKnife() || RandomFloat( 0, 100 ) < knifeAmbushChance)
							{
								PrintIfWatched( "Attacking our friend's killer!\n" );
								Attack( killer );
								return;
							}
						}

						// if friend was far away and we haven't seen an enemy in awhile, go to where our friend was killed
						const float longHidingTime = 20.0f;
						if (IsHunting() || IsInvestigatingNoise() || (IsHiding() && GetTask() != FOLLOW && GetHidingTime() > longHidingTime))
						{
							const float someTime = 10.0f;
							const float farAway = 750.0f;
							if (GetTimeSinceLastSawEnemy() > someTime && (playerOrigin - GetAbsOrigin()).IsLengthGreaterThan( farAway ))
							{
								PrintIfWatched( "Checking out where our friend was killed\n" );
								MoveTo( playerOrigin, FASTEST_ROUTE );
								return;
							}
						}
					}
				}
			}
		}
	}
	else  // an enemy was killed
	{
		// forget our current noise - it may have come from the now dead enemy
		ForgetNoise();

		if (killer && killer->GetTeamNumber() == GetTeamNumber())
		{
			// only chatter about enemy kills if we see them occur, and they were the last one we see
			if (GetNearbyEnemyCount() <= 1)
			{
				// report if number of enemies left is few and we killed the last one we saw locally
				GetChatter()->EnemiesRemaining();

				Vector victimOrigin = GetCentroid( victim );
				if (IsVisible( victimOrigin, true ))
				{						
					// congratulate teammates on their kills
					if (killer && killer != this)
					{
						float delay = RandomFloat( 2.0f, 3.0f );
						if (killer->IsBot())
						{
							if (RandomFloat( 0.0f, 100.0f ) < 40.0f)
								GetChatter()->Say( "NiceShot", 3.0f, delay );
						}
						else
						{
							// humans get the honorific
							if (CSGameRules()->IsCareer())
								GetChatter()->Say( "NiceShotCommander", 3.0f, delay );
							else
								GetChatter()->Say( "NiceShotSir", 3.0f, delay );
						}
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnPlayerRadio( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CCSPlayer *player = ToCSPlayer( UTIL_PlayerByUserId( event->GetInt( "userid" ) ) );
	if ( player == this )
		return;

	//
	// Process radio events from our team
	//
	if (player && player->GetTeamNumber() == GetTeamNumber() )
	{
		/// @todo Distinguish between radio commands and responses
		RadioType radioEvent = (RadioType)event->GetInt( "slot" );

		if (radioEvent != RADIO_INVALID && radioEvent != RADIO_AFFIRMATIVE && radioEvent != RADIO_NEGATIVE && radioEvent != RADIO_REPORTING_IN
			&& radioEvent != RADIO_CHEER && radioEvent != RADIO_THANKS && radioEvent != RADIO_COMPLIMENT)
		{
			m_lastRadioCommand = radioEvent;
			m_lastRadioRecievedTimestamp = gpGlobals->curtime;
			m_radioSubject = player;
			m_radioPosition = GetCentroid( player );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnPlayerFallDamage( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // player_falldamage
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnPlayerFootstep( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false, true ); // player_footstep
}

class CBasePlayer;
class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponFire( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	const float ShortRange = 1000.0f;
	const float NormalRange = 2000.0f;

	float range;

	/// @todo Check weapon type (knives are pretty quiet)
	/// @todo Use actual volume, account for silencers, etc.

	// [mlowrance] use the weapon as posted in the event message
	int iWeaponID = -1;
	const char *weaponName = event->GetString( "weapon" );
	if ( weaponName )
	{
		iWeaponID = AliasToWeaponID( weaponName );
	}
	
	if ( iWeaponID == -1 )
		return;

	switch( iWeaponID )
	{
		// silent "firing"
		case WEAPON_HEGRENADE:
		case WEAPON_SMOKEGRENADE:
		case WEAPON_FLASHBANG:
		case WEAPON_INCGRENADE:
		case WEAPON_MOLOTOV:
		case WEAPON_DECOY:
		case WEAPON_TAGRENADE:
		case WEAPON_C4:
			return;

		// quiet
		case WEAPON_KNIFE:
		case WEAPON_KNIFE_GG:
			range = ShortRange;
			break;

		// loud
		case WEAPON_AWP:
			range = 99999.0f;
			break;

		// normal
		default:
			if ( event->GetBool( "silenced" ) )
			{
				range = ShortRange;
			}
			else
			{
				range = NormalRange;
			}
			break;
	}

	OnAudibleEvent( event, player, range, PRIORITY_HIGH, true ); // weapon_fire
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponFireOnEmpty( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_fire_on_empty
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponReload( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// for knife fighting - if our victim is attacking or reloading, rush him
	/// @todo Propagate events into active state
	if (GetBotEnemy() == player && IsUsingKnife())
	{
		ForceRun( 5.0f );
	}

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_reload
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnWeaponZoom( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_LOW, false ); // weapon_zoom
}

class BotChatterInterface;
class BotProfile;
class CBasePlayer;
class CCSBot;
class CCSPlayer;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

ConVar mp_coopmission_bot_difficulty_offset(
	side_registry(), "mp_coopmission_bot_difficulty_offset",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The difficulty offset modifier for bots during coop missions." );

//--------------------------------------------------------------------------------------------------------------
static void PrefixChanged( ConVar *c, const char *oldPrefix, float flOldValue )
{
	if ( TheCSBots() && TheCSBots()->IsServerActive() )
	{
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

			if ( !player )
				continue;

			if ( !player->IsBot() || !IsEntityValid( player ) )
				continue;

			CCSBot *bot = ToCSBot( player );

			if ( !bot )
				continue;

			// set the bot's name
			char botName[MAX_PLAYER_NAME_LENGTH];
			UTIL_ConstructBotNetName( botName, MAX_PLAYER_NAME_LENGTH, bot->GetProfile() );

			engine->SetFakeClientConVarValue( bot->edict(), "name", botName );
		}
	}
}

ConVar cv_bot_traceview( side_registry(), "bot_traceview", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "For internal testing purposes." );
ConVar cv_bot_stop( side_registry(), "bot_stop", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "If nonzero, immediately stops all bot processing." );
ConVar cv_bot_show_nav( side_registry(), "bot_show_nav", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "For internal testing purposes." );
ConVar cv_bot_walk( side_registry(), "bot_walk", "0", FCVAR_REPLICATED, "If nonzero, bots can only walk, not run." );
ConVar cv_bot_difficulty( side_registry(), "bot_difficulty", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Defines the skill of bots joining the game.  Values are: 0=easy, 1=normal, 2=hard, 3=expert." );
ConVar cv_bot_debug( side_registry(), "bot_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "For internal testing purposes." );
ConVar cv_bot_debug_target( side_registry(), "bot_debug_target", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "For internal testing purposes." );
ConVar cv_bot_quota( side_registry(), "bot_quota", "10", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines the total number of bots in the game." );
ConVar cv_bot_quota_mode( side_registry(), "bot_quota_mode", "normal", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines the type of quota.\nAllowed values: 'normal', 'fill', and 'match'.\nIf 'fill', the server will adjust bots to keep N players in the game, where N is bot_quota.\nIf 'match', the server will maintain a 1:N ratio of humans to bots, where N is bot_quota." );
ConVar cv_bot_prefix( side_registry(), "bot_prefix", "", FCVAR_REPLICATED, "This string is prefixed to the name of all bots that join the game.\n<difficulty> will be replaced with the bot's difficulty.\n<weaponclass> will be replaced with the bot's desired weapon class.\n<skill> will be replaced with a 0-100 representation of the bot's skill.", PrefixChanged );
ConVar cv_bot_allow_rogues( side_registry(), "bot_allow_rogues", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may occasionally go 'rogue'. Rogue bots do not obey radio commands, nor pursue scenario goals." );
ConVar cv_bot_allow_pistols( side_registry(), "bot_allow_pistols", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use pistols." );
ConVar cv_bot_allow_shotguns( side_registry(), "bot_allow_shotguns", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use shotguns." );
ConVar cv_bot_allow_sub_machine_guns( side_registry(), "bot_allow_sub_machine_guns", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use sub-machine guns." );
ConVar cv_bot_allow_rifles( side_registry(), "bot_allow_rifles", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use rifles." );
ConVar cv_bot_allow_machine_guns( side_registry(), "bot_allow_machine_guns", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use the machine gun." );
ConVar cv_bot_allow_grenades( side_registry(), "bot_allow_grenades", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use grenades." );
ConVar cv_bot_allow_snipers( side_registry(), "bot_allow_snipers", "1", FCVAR_RELEASE|FCVAR_REPLICATED, "If nonzero, bots may use sniper rifles." );
ConVar cv_bot_join_team( side_registry(), "bot_join_team", "any", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines the team bots will join into. Allowed values: 'any', 'T', or 'CT'." );
ConVar cv_bot_join_after_player( side_registry(), "bot_join_after_player", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "If nonzero, bots wait until a player joins before entering the game." );
ConVar cv_bot_auto_vacate( side_registry(), "bot_auto_vacate", "1", FCVAR_REPLICATED, "If nonzero, bots will automatically leave to make room for human players." );
ConVar cv_bot_zombie( side_registry(), "bot_zombie", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "If nonzero, bots will stay in idle mode and not attack." );
ConVar cv_bot_defer_to_human_goals( side_registry(), "bot_defer_to_human_goals", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "If nonzero and there is a human on the team, the bots will not do the scenario tasks." );
ConVar cv_bot_defer_to_human_items( side_registry(), "bot_defer_to_human_items", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "If nonzero and there is a human on the team, the bots will not get scenario items." );
ConVar cv_bot_chatter( side_registry(), "bot_chatter", "normal", FCVAR_REPLICATED | FCVAR_RELEASE, "Control how bots talk. Allowed values: 'off', 'radio', 'minimal', or 'normal'." );
ConVar cv_bot_profile_db( side_registry(), "bot_profile_db", "BotProfile.db", FCVAR_REPLICATED, "The filename from which bot profiles will be read." );
ConVar cv_bot_dont_shoot( side_registry(), "bot_dont_shoot", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_CHEAT, "If nonzero, bots will not fire weapons (for debugging)." );
ConVar cv_bot_eco_limit( side_registry(), "bot_eco_limit", "2000", FCVAR_REPLICATED, "If nonzero, bots will not buy if their money falls below this amount." );
ConVar cv_bot_auto_follow( side_registry(), "bot_auto_follow", "0", FCVAR_REPLICATED, "If nonzero, bots with high co-op may automatically follow a nearby human player." );
ConVar cv_bot_flipout( side_registry(), "bot_flipout", "0", FCVAR_REPLICATED, "If nonzero, bots use no CPU for AI. Instead, they run around randomly." );
ConVar cv_bot_controllable( side_registry(), "bot_controllable", "1", FCVAR_REPLICATED, "Determines whether bots can be controlled by players" );

extern void FinishClientPutInServer( CCSPlayer *pPlayer );

//--------------------------------------------------------------------------------------------------------------
// Engine callback for custom server commands
void Bot_ServerCommand( void )
{
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor
 */
CCSBot::CCSBot( void ) :
m_gameState( this ),
m_hasJoined( false ),
m_pLocalProfile( nullptr )
{
	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		m_pChatter = new BotChatterCoop( this );
	}
	else
	{
		m_pChatter = new BotChatterInterface( this );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CCSBot::~CCSBot()
{
	if ( m_pLocalProfile )
	{
		delete m_pLocalProfile;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Prepare bot for action
 */
bool CCSBot::Initialize( const BotProfile *profile, int team )
{
	int preserved_voice_pitch = -1;
	char preserved_name[256];
	preserved_name[0] = 0;

	contract_assert( ( profile != nullptr ) && "You cannot pass in null for a bot profile." );
	if ( nullptr == profile ) return false;

	if ( m_pLocalProfile )
	{
		// Preserve the voice pitch and name from the existing profile
		preserved_voice_pitch = m_pLocalProfile->GetVoicePitch();
		if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
		{
			// change names everytime in coop
			V_snprintf( preserved_name, 256, "%s", profile->GetName() );
		}
		else
			V_snprintf( preserved_name, 256, "%s", m_pLocalProfile->GetName() );
		delete m_pLocalProfile;
	}

	m_pLocalProfile = new BotProfile;

	if ( m_pLocalProfile )
	{
		// Copy the profile into the local profile
		m_pLocalProfile->Clone( profile );

		// Restore the name
		if ( V_strlen( preserved_name ) > 0 )
		{
			m_pLocalProfile->SetName( preserved_name );
		}

		// Restore the voice pitch
		if ( preserved_voice_pitch > -1 )
		{
			m_pLocalProfile->SetVoicePitch( preserved_voice_pitch );
		}
	}

	// extend
	BaseClass::Initialize( m_pLocalProfile, team );

	// CS bot initialization
	m_diedLastRound = false;

	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// in demolition, start the CT's off with terrible morale, so they try to camp the bombsite
		if ( team == TEAM_CT )
		{
			m_morale = TERRIBLE;
		}
		else
		{
			m_morale = POSITIVE;			// starting a new round makes everyone a little happy
		}
	}
	else
	{
		m_morale = POSITIVE;			// starting a new round makes everyone a little happy
	}

	m_combatRange = RandomFloat( 325.0f, 425.0f );

	// set initial safe time guess for this map
	m_safeTime = 15.0f + 5.0f * GetProfile()->GetAggression( );

	m_name[0] = '\000';

	ResetValues();

	m_desiredTeam = team;

	if (GetTeamNumber() == 0)
	{
		HandleCommand_JoinTeam( m_desiredTeam );
		// join class is already called within JoinTeam
		//HandleCommand_JoinClass();
	}

	return true;
}

void CCSBot::CoopInitialize( void )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && GetTeamNumber() == TEAM_TERRORIST )
	{
		// bots start asleep when they spawn and wake up when players come close to them
		SpawnPointCoopEnemy *pEnemySpawnSpot = GetLastCoopSpawnPoint();
		if ( pEnemySpawnSpot )
		{
			SetAbsAngles( pEnemySpawnSpot->GetAbsAngles() );
			int nDifficulty = pEnemySpawnSpot->GetBotDifficulty();
			int nOffset = mp_coopmission_bot_difficulty_offset.GetInt();
			nDifficulty = nDifficulty + nOffset;

			BotDifficultyType botDifficultyType = BOT_EASY;
			if ( nDifficulty >= 6 )
				botDifficultyType = BOT_EXPERT;
			else if ( nDifficulty >= 3 )
				botDifficultyType = BOT_HARD;
			else if ( nDifficulty >= 1 )
				botDifficultyType = BOT_NORMAL;

			bool bIsAgressive = pEnemySpawnSpot->IsBotAgressive(); 

			char profileName[128];
			V_snprintf( profileName, sizeof( profileName ), "Level_%d%s", ( int )nDifficulty, bIsAgressive ? "_Agressive" : "" );

			const BotProfile* pNewProfileData = TheBotProfiles->GetProfileMatchingTemplate( profileName, TEAM_TERRORIST, botDifficultyType, CSGameRules()->GetMatchDevice(), true );

			if ( pNewProfileData )
			{
				char szHeavy[64] = {};
				int nArmor = pEnemySpawnSpot->GetArmorToSpawnWith();
				if ( nArmor == 1 )
					V_snprintf( szHeavy, sizeof( szHeavy ), "%s", "" );
				else if ( nArmor == 2 )
					V_snprintf( szHeavy, sizeof( szHeavy ), "%s", "Heavy " );

				char szName[128] = {};
				char szDifficulty[128] = {};

				if ( nDifficulty >= 6 )
					V_snprintf( szDifficulty, sizeof( szDifficulty ), "%s", "Elite " );
				else if ( nDifficulty >= 4 )
					V_snprintf( szDifficulty, sizeof( szDifficulty ), "%s", "Expert " );
				else
					V_snprintf( szDifficulty, sizeof( szDifficulty ), "%s", "" );

				V_snprintf( szName, sizeof( szName ), "%s%sPhoenix", szHeavy, szDifficulty );

				Initialize( pNewProfileData, GetTeamNumber() );
				SetPlayerName( szName );
				// have to inform the engine that the bot name has been updated
				engine->SetFakeClientConVarValue( edict(), "name", szName );
			}

			// sleeping needs to be set after the initialize because we reset values in the init
			m_bIsSleeping = pEnemySpawnSpot->ShouldStartAsleep();
		}	
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset internal data to initial state
 */
void CCSBot::ResetValues( void )
{
	m_pChatter->Reset();
	m_gameState.Reset();

	m_avoid = nullptr;
	m_avoidTimestamp = 0.0f;

	m_hurryTimer.Invalidate();
	m_alertTimer.Invalidate();
	m_sneakTimer.Invalidate();
	m_noiseBendTimer.Invalidate();
	m_bendNoisePositionValid = false;

	m_isStuck = false;
	m_stuckTimestamp = 0.0f;
	m_wiggleTimer.Invalidate();
	m_stuckJumpTimer.Invalidate();
	m_nextCleanupCheckTimestamp = 0.0f;

	m_pathLength = 0;
	m_pathIndex = 0;
	m_areaEnteredTimestamp = 0.0f;
	m_currentArea = nullptr;
	m_lastKnownArea = nullptr;
	m_isStopping = false;

	m_avoidFriendTimer.Invalidate();
	m_isFriendInTheWay = false;
	m_isWaitingBehindFriend = false;
	m_isAvoidingGrenade.Invalidate();

	StopPanicking();

	m_disposition = ENGAGE_AND_INVESTIGATE;

	m_enemy = nullptr;

	m_grenadeTossState = NOT_THROWING;
	m_initialEncounterArea = nullptr;

	m_wasSafe = true;

	m_nearbyEnemyCount = 0;
	m_enemyPlace = 0;
	m_nearbyFriendCount = 0;
	m_closestVisibleFriend = nullptr;
	m_closestVisibleHumanFriend = nullptr;

	for( int w=0; w<MAX_PLAYERS; ++w )
	{
		m_watchInfo[w].timestamp = 0.0f;
		m_watchInfo[w].isEnemy = false;

		m_playerTravelDistance[ w ] = -1.0f;
	}

	// randomly offset each bot's timer to spread computation out
	m_updateTravelDistanceTimer.Start( RandomFloat( 0.0f, 0.9f ) );
	m_travelDistancePhase = 0;

	m_isEnemyVisible = false;
	m_visibleEnemyParts = NONE;
	m_lastSawEnemyTimestamp = -999.9f;
	m_firstSawEnemyTimestamp = 0.0f;
	m_currentEnemyAcquireTimestamp = 0.0f;
	m_isLastEnemyDead = true;
	m_attacker = nullptr;
	m_attackedTimestamp = 0.0f;
	m_enemyDeathTimestamp = 0.0f;
	m_friendDeathTimestamp = 0.0f;
	m_lastVictimID = 0;
	m_isAimingAtEnemy = false;
	m_fireWeaponTimestamp = 0.0f;
	m_equipTimer.Invalidate();
	m_zoomTimer.Invalidate();

	m_isFollowing = false;
	m_leader = nullptr;
	m_followTimestamp = 0.0f;
	m_allowAutoFollowTime = 0.0f;

	m_enemyQueueIndex = 0;
	m_enemyQueueCount = 0;
	m_enemyQueueAttendIndex = 0;
	m_bomber = nullptr;

	m_bIsSleeping = false;

	m_isEnemySniperVisible = false;
	m_sawEnemySniperTimer.Invalidate();

	m_lookAroundStateTimestamp = 0.0f;
	m_inhibitLookAroundTimestamp = 0.0f;

	m_lookPitch = 0.0f;
	m_lookPitchVel = 0.0f;
	m_lookYaw = 0.0f;
	m_lookYawVel = 0.0f;

	m_lookAtSpotState = NOT_LOOKING_AT_SPOT;

	m_targetSpot.Zero();
	m_targetSpotVelocity.Zero();
	m_targetSpotPredicted.Zero();
	m_aimError.Init();
	m_aimGoal.Init();
	m_targetSpotTime = 0.0f;
	m_aimFocus = 0.0f;
	m_aimFocusInterval = 0.0f;
	m_aimFocusNextUpdate = 0.0f;

	for( int p=0; p<MAX_PLAYERS; ++p )
	{
		m_partInfo[p].m_validFrame = 0;
	}

	m_spotEncounter = nullptr;
	m_spotCheckTimestamp = 0.0f;
	m_peripheralTimestamp = 0.0f;

	m_avgVelIndex = 0;
	m_avgVelCount = 0;

	m_lastOrigin = GetCentroid( this );

	m_lastRadioCommand = RADIO_INVALID;
	m_lastRadioRecievedTimestamp = 0.0f;
	m_lastRadioSentTimestamp = 0.0f;
	m_radioSubject = nullptr;
	m_voiceEndTimestamp = 0.0f;

	m_hostageEscortCount = 0;
	m_hostageEscortCountTimestamp = 0.0f;

	m_noisePosition = Vector( 0, 0, 0 );
	m_noiseTimestamp = 0.0f;

	m_stateTimestamp = 0.0f;
	m_task = SEEK_AND_DESTROY;
	m_taskEntity = nullptr;

	m_approachPointCount = 0;
	m_approachPointViewPosition.x = 99999999999.9f;
	m_approachPointViewPosition.y = 0.0f;
	m_approachPointViewPosition.z = 0.0f;

	m_checkedHidingSpotCount = 0;

	StandUp();
	Run();
	m_mustRunTimer.Invalidate();
	m_waitTimer.Invalidate();
	m_pathLadder = nullptr;

	m_repathTimer.Invalidate();

	m_huntState.ClearHuntArea();
	m_hasVisitedEnemySpawn = false;
	m_stillTimer.Invalidate();

	// adjust morale - if we died, our morale decreased, 
	// but if we live, no adjustement (round win/loss also adjusts morale)
	if (m_diedLastRound)
		DecreaseMorale();

	m_diedLastRound = false;

	// IsRogue() randomly changes this
	m_isRogue = false;	

	m_surpriseTimer.Invalidate();

	// even though these are EHANDLEs, they need to be NULL-ed
	m_goalEntity = nullptr;
	m_avoid = nullptr;
	m_enemy = nullptr;

	for ( int i=0; i<MAX_ENEMY_QUEUE; ++i )
	{
		m_enemyQueue[i].player = nullptr;
		m_enemyQueue[i].isReloading = false;
		m_enemyQueue[i].isProtectedByShield = false;
	}

	m_burnedByFlamesTimer.Invalidate();

	// start in idle state
	m_isOpeningDoor = false;
	StopAttacking();
	Idle();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when bot is placed in map, and when bots are reset after a round ends.
 * NOTE: For some reason, this can be called twice when a bot is added.
 */
void CCSBot::Spawn_Impl( void )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && GetTeamNumber() == TEAM_TERRORIST )
		SetLastCoopSpawnPoint( nullptr );

	// do the normal player spawn process
	BaseClass::Spawn_Impl();

	ResetValues();

	Buy();

	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		if ( GetTeamNumber() == TEAM_CT )
		{
			SetDisposition( CCSBot::SELF_DEFENSE );

			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer && !pPlayer->IsBot() && pPlayer->GetTeamNumber() == TEAM_CT )
				{
					//m_isFollowing = true;
					//m_leader = pPlayer;

					//SetTask( CCSBot::FOLLOW );
					Follow( pPlayer );

					m_lastRadioCommand = RADIO_FOLLOW_ME;
					m_lastRadioRecievedTimestamp = gpGlobals->curtime;
					m_radioSubject = pPlayer;
					m_radioPosition = GetCentroid( pPlayer );		
					break;
				}
			}		
		}
		else if ( GetTeamNumber() == TEAM_TERRORIST )
		{
			CoopInitialize();
		}
	}

	// set the bot name here (after we've had a chance to initialize a new profile
	V_strcpy_safe( m_name, GetPlayerName() );
}

class CCSBot;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsNoiseHeard( void ) const
{
	if (m_noiseTimestamp <= 0.0f)
		return false;

	// primitive reaction time simulation - cannot "hear" noise until reaction time has elapsed
	if (gpGlobals->curtime - m_noiseTimestamp >= GetProfile()->GetReactionTime())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Listen for enemy noises, and determine if we should react to them.
 * Returns true if heard a noise and should move to investigate.
 */
bool CCSBot::HeardInterestingNoise( void )
{
	if (IsBlind())
		return false;

	// don't investigate noises during safe time
	if (!IsWellPastSafe())
		return false;

	// if our disposition is not to investigate, dont investigate
	if (GetDisposition() != ENGAGE_AND_INVESTIGATE)
		return false;

	// listen for enemy noises
	if (IsNoiseHeard())
	{
		// if we are hiding, only react to noises very nearby, depending on how aggressive we are
		if (IsAtHidingSpot() && GetNoiseRange() > 100.0f + 400.0f * GetProfile()->GetAggression())
			return false;
		
		float chance = GetNoiseInvestigateChance();

		if (RandomFloat( 0.0f, 100.0f ) <= chance)
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we hear nearby threatening enemy gunfire within given range
 * -1 == infinite range
 */
bool CCSBot::CanHearNearbyEnemyGunfire( float range ) const
{
	Vector myOrigin = GetCentroid( this );

	// only attend to noise if it just happened
	if (gpGlobals->curtime - m_noiseTimestamp > 0.5f)
		return false;

	// gunfire is high priority
	if (m_noisePriority < PRIORITY_HIGH)
		return false;

	// check noise range
	if (range > 0.0f && (myOrigin - m_noisePosition).IsLengthGreaterThan( range ))
		return false;

	// if we dont have line of sight, it's not threatening (cant get shot)
	if (!CanSeeNoisePosition())
		return false;

	if (IsAttacking() && m_enemy != nullptr && GetTimeSinceLastSawEnemy() < 1.0f)
	{
		// gunfire is only threatening if it is closer than our current enemy
		float gunfireDistSq = (m_noisePosition - myOrigin).LengthSqr();
		float enemyDistSq = (GetCentroid( m_enemy ) - myOrigin).LengthSqr();
		const float muchCloserSq = 100.0f * 100.0f;
		if (gunfireDistSq > enemyDistSq - muchCloserSq)
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we directly see where we think the noise came from
 * NOTE: Dont check FOV, since this is used to determine if we should turn our head to look at the noise
 * NOTE: Dont use IsVisible(), because smoke shouldnt cause us to not look toward noises
 */
bool CCSBot::CanSeeNoisePosition( void ) const
{
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceLine( EyePositionConst(), m_noisePosition + Vector( 0, 0, HalfHumanHeight ), MASK_VISIBLE_AND_NPCS, &traceFilter, &result );
	if (result.fraction == 1.0f)
	{
		// we can see the source of the noise
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we decided to look towards the most recent noise source
 * Assumes m_noisePosition is valid.
 */
bool CCSBot::UpdateLookAtNoise( void )
{
	// make sure a noise exists
	if (!IsNoiseHeard())
	{
		return false;
	}

	Vector spot;

	// if we have clear line of sight to noise position, look directly at it
	if (CanSeeNoisePosition())
	{
		/// @todo adjust noise Z to keep consistent with current height while fighting
		spot = m_noisePosition + Vector( 0, 0, HalfHumanHeight );

		// since we can see the noise spot, forget about it
		ForgetNoise();
	}
	else
	{
		// line of sight is blocked, bend it

		// the bending algorithm is very expensive, throttle how often it is done
		if (m_noiseBendTimer.IsElapsed())
		{
			const float noiseBendLOSInterval = RandomFloat( 0.2f, 0.3f );
			m_noiseBendTimer.Start( noiseBendLOSInterval );

			// line of sight is blocked, bend it
			if (BendLineOfSight( EyePosition(), m_noisePosition, &spot ) == false)
			{
				m_bendNoisePositionValid = false;
				return false;
			}

			m_bentNoisePosition = spot;
			m_bendNoisePositionValid = true;
		}
		else if (m_bendNoisePositionValid)
		{
			// use result of prior bend computation
			spot = m_bentNoisePosition;
		}
		else
		{
			// prior bend failed
			return false;
		}
	}

	// it's always important to look at enemy noises, because they come from ... enemies!
	PriorityType pri = PRIORITY_HIGH;

	// look longer if we're hiding
	if (IsAtHidingSpot())
	{
		// if there is only one enemy left, look for a long time
		if (GetEnemiesRemaining() == 1)
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 5.0f, 15.0f ), true );
		}
		else
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 3.0f, 5.0f ), true );
		}
	}
	else
	{
		const float closeRange = 500.0f;
		if (GetNoiseRange() < closeRange)
		{
			// look at nearby enemy noises for a longer time
			SetLookAt( "Noise", spot, pri, RandomFloat( 3.0f, 5.0f ), true );
		}
		else
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 1.0f, 2.0f ), true );
		}
	}

	return true;
}

class BotProfile;
class CBaseEntity;
class CBasePlayer;
class CBotManager;
class CCSBot;
class CCSBotManager;
class CCSPlayer;
class CNavArea;
class CWeaponCSBase;
struct Extent;

using namespace ::ks::reflect;

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

CBotManager *TheBots = nullptr;

bool CCSBotManager::m_isMapDataLoaded = false;

int g_nClientPutInServerOverrides = 0;

void DrawOccupyTime( void );
ConVar bot_show_occupy_time( side_registry(), "bot_show_occupy_time", "0", FCVAR_GAMEDLL | FCVAR_CHEAT, "Show when each nav area can first be reached by each team." );

void DrawBattlefront( void );
ConVar bot_show_battlefront( side_registry(), "bot_show_battlefront", "0", FCVAR_GAMEDLL | FCVAR_CHEAT, "Show areas where rushing players will initially meet." );

int UTIL_CSSBotsInGame( void );

ConVar bot_join_delay( side_registry(), "bot_join_delay", "0", FCVAR_GAMEDLL, "Prevents bots from joining the server for this many seconds after a map change." );
ConVar bot_join_in_warmup( side_registry(), "bot_join_in_warmup", "1", FCVAR_GAMEDLL, "Prevents bots from joining the server while warmup phase is active." );

ConVar throttle_expensive_ai( side_registry(), "throttle_expensive_ai", "0" );

extern ConVar mp_guardian_target_site;
/**
 * Determine whether bots can be used or not
 */
inline bool AreBotsAllowed()
{
	// If they pass in -nobots, don't allow bots.  This is for people who host servers, to
	// allow them to disallow bots to enforce CPU limits.
	const char *nobots = CommandLine()->CheckParm( "-nobots" );
	if ( nobots )
	{
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void InstallBotControl( void )
{
	if ( TheBots != nullptr )
		delete TheCSBots();

	TheBots = new CCSBotManager;
}

//--------------------------------------------------------------------------------------------------------------
void RemoveBotControl( void )
{
	if ( TheBots != nullptr )
		delete TheCSBots();

	TheBots = nullptr;
}

//--------------------------------------------------------------------------------------------------------------
CBasePlayer* ClientPutInServerOverride_Bot( edict_t *pEdict, const char *playername )
{
	CBasePlayer *pPlayer = TheCSBots()->AllocateAndBindBotEntity( pEdict );
	if ( pPlayer )
	{
		pPlayer->SetPlayerName( playername );
	}
	++g_nClientPutInServerOverrides;

	return pPlayer;
}

//--------------------------------------------------------------------------------------------------------------
// Constructor
CCSBotManager::CCSBotManager()
{
	m_zoneCount = 0;
	SetLooseBomb( nullptr );
	m_serverActive = false;

	m_isBombPlanted = false;
	m_bombDefuser = nullptr;
	m_roundStartTimestamp = 0.0f;

	m_eventListenersEnabled = true;
	m_commonEventListeners.push_back( &m_PlayerFootstepEvent );
	m_commonEventListeners.push_back( &m_PlayerRadioEvent );
	m_commonEventListeners.push_back( &m_PlayerFallDamageEvent );
	m_commonEventListeners.push_back( &m_BombBeepEvent );
	m_commonEventListeners.push_back( &m_DoorMovingEvent );
	m_commonEventListeners.push_back( &m_BreakPropEvent );
	m_commonEventListeners.push_back( &m_BreakBreakableEvent );
	m_commonEventListeners.push_back( &m_WeaponFireEvent );
	m_commonEventListeners.push_back( &m_WeaponFireOnEmptyEvent );
	m_commonEventListeners.push_back( &m_WeaponReloadEvent );
	m_commonEventListeners.push_back( &m_WeaponZoomEvent );
	m_commonEventListeners.push_back( &m_BulletImpactEvent );
	m_commonEventListeners.push_back( &m_GrenadeBounceEvent );
	m_commonEventListeners.push_back( &m_NavBlockedEvent );

	TheBotPhrases = new BotPhraseManager;
	TheBotProfiles = new BotProfileManager;

	m_nNumExpensiveOperationsThisFrame = 0;
}

const CCSBotManager::Zone* Helper_GetZoneForPlaceName( const char* szName )
{
	Place pl = TheNavMesh->NameToPlace( szName );
	for ( int i = 0; i < TheCSBots()->GetZoneCount(); ++i )
	{
		if ( !TheCSBots()->GetZone( i ) )
			continue;

		for ( int j = 0; j < CCSBotManager::MAX_ZONE_NAV_AREAS; ++j )
		{
			if ( !TheCSBots()->GetZone( i )->m_area[ j ] )
				continue;

			if ( TheCSBots()->GetZone( i )->m_area[ j ]->GetPlace() == pl )
			{
				return TheCSBots()->GetZone( i );
			}
		}
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when a new round begins
 */
void CCSBotManager::RestartRound( void )
{
	// extend
	CBotManager::RestartRoundBase();

	SetLooseBomb( nullptr );
	m_isBombPlanted = false;

	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// push to plant the bomb quickly in this game mode
		m_earliestBombPlantTimestamp = gpGlobals->curtime + RandomFloat( 0.0f, 10.0f );
	}
	else
	{
		m_earliestBombPlantTimestamp = gpGlobals->curtime + RandomFloat( 10.0f, 30.0f ); // 60
	}
	m_bombDefuser = nullptr;

	ResetRadioMessageTimestamps();

	m_lastSeenEnemyTimestamp = -9999.9f;

	m_roundStartTimestamp = gpGlobals->curtime + mp_freezetime.GetFloat();

	// randomly decide if defensive team wants to "rush" as a whole
	const float defenseRushChance = 33.3f;	// 25.0f;
	m_isDefenseRushing = (RandomFloat( 0.0f, 100.0f ) <= defenseRushChance) ? true : false;

	if ( CSGameRules()->IsPlayingCoopGuardian() && mp_guardian_target_site.GetInt() >= 0 )
	{
		m_isDefenseRushing = 0;
		m_earliestBombPlantTimestamp = 0;
		m_eTStrat = k_ETStrat_Rush;
		m_iTerroristTargetSite = mp_guardian_target_site.GetInt();

		/*
		// Choose random strat if there are no humans on team
		if ( UTIL_HumansOnTeam( TEAM_TERRORIST ) == 0 )
		{
			m_eTStrat = k_ETStrat_Rush;
			m_iTerroristTargetSite = RandomInt( 0, 1 );
		}
		if ( UTIL_HumansOnTeam( TEAM_CT ) == 0 )
		{
			m_eCTStrat = k_ECTStrat_StackSite;
			m_iCTPrioritySite = RandomInt( 0, 1 );
		}
		*/
	}

	TheBotPhrases->OnRoundRestart();

	m_isRoundOver = false;
}

//--------------------------------------------------------------------------------------------------------------

void UTIL_DrawBox( Extent *extent, int lifetime, int red, int green, int blue )
{
	int darkRed = red/2;
	int darkGreen = green/2;
	int darkBlue = blue/2;

	Vector v[8];
	v[0].x = extent->lo.x; v[0].y = extent->lo.y; v[0].z = extent->lo.z;
	v[1].x = extent->hi.x; v[1].y = extent->lo.y; v[1].z = extent->lo.z;
	v[2].x = extent->hi.x; v[2].y = extent->hi.y; v[2].z = extent->lo.z;
	v[3].x = extent->lo.x; v[3].y = extent->hi.y; v[3].z = extent->lo.z;
	v[4].x = extent->lo.x; v[4].y = extent->lo.y; v[4].z = extent->hi.z;
	v[5].x = extent->hi.x; v[5].y = extent->lo.y; v[5].z = extent->hi.z;
	v[6].x = extent->hi.x; v[6].y = extent->hi.y; v[6].z = extent->hi.z;
	v[7].x = extent->lo.x; v[7].y = extent->hi.y; v[7].z = extent->hi.z;

	static int edge[] = 
	{
		1, 2, 3, 4, -1,
		5, 6, 7, 8, -5,
		1, -5,
		2, -6,
		3, -7,
		4, -8,
		0
	};

	Vector from, to;
	bool restart = true;
	for( int i=0; edge[i] != 0; ++i )
	{
		if (restart)
		{
			to = v[ edge[i]-1 ];
			restart = false;
			continue;
		}
		
		from = to;

		int index = edge[i];
		if (index < 0)
		{
			restart = true;
			index = -index;
		}

		to = v[ index-1 ];

		NDebugOverlay::Line( from, to, darkRed, darkGreen, darkBlue, true, 0.1f );
		NDebugOverlay::Line( from, to, red, green, blue, false, 0.15f );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::EnableEventListeners( bool enable )
{
	if ( m_eventListenersEnabled == enable )
	{
		return;
	}

	m_eventListenersEnabled = enable;

	// enable/disable the most frequent event listeners, to improve performance when no bots are present.
	for ( int i=0; i<std::ssize( m_commonEventListeners ); ++i )
	{
		if ( enable )
		{
			gameeventmanager->AddListener( m_commonEventListeners[i], m_commonEventListeners[i]->GetCallback(), m_commonEventListeners[i]->GetEventName(), true );
		}
		else
		{
			gameeventmanager->RemoveListener( m_commonEventListeners[i] );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called each frame
 */
void CCSBotManager::StartFrame( void )
{
	m_nNumExpensiveOperationsThisFrame = 0;

	if ( engine->IsPaused() )
		return;

	if ( !AreBotsAllowed() )
	{
		EnableEventListeners( false );
		return;
	}

	// EXTEND
	CBotManager::StartFrameBase();

	if ( !CSGameRules()->IsSwitchingTeamsAtRoundReset() )
	{
		// Maintain the bot quota only if the game is not currently switching teams at halftime
		MaintainBotQuota();
	}
	EnableEventListeners( UTIL_CSSBotsInGame() > 0 );

	// debug zone extent visualization
	if (cv_bot_debug.GetInt() == 5)
	{
		for( int z=0; z<m_zoneCount; ++z )
		{
			Zone *zone = &m_zone[z];

			if ( zone->m_isBlocked )
			{
				UTIL_DrawBox( &zone->m_extent, 1, 255, 0, 200 );
			}
			else
			{
				UTIL_DrawBox( &zone->m_extent, 1, 255, 100, 0 );
			}
		}
	}

	if (bot_show_occupy_time.GetBool())
	{
		DrawOccupyTime();
	}

	if (bot_show_battlefront.GetBool())
	{
		DrawBattlefront();
	}

	if ( m_checkTransientAreasTimer.IsElapsed() && !nav_edit.GetBool() )
	{
		std::vector< CNavArea * >& transientAreas = TheNavMesh->GetTransientAreas();
		for ( int i=0; i<std::ssize( transientAreas ); ++i )
		{
			CNavArea *area = transientAreas[i];
			if ( area->GetAttributes() & NAV_MESH_TRANSIENT )
			{
				area->UpdateBlocked();
			}
		}

		m_checkTransientAreasTimer.Start( 2.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the bot can use this weapon
 */
bool CCSBotManager::IsWeaponUseable( const CWeaponCSBase *weapon ) const
{
	if (weapon == nullptr)
		return false;

	if (weapon->IsA( WEAPON_C4 ))
		return true;

	if ((!AllowShotguns() && weapon->IsKindOf( WEAPONTYPE_SHOTGUN )) ||
		(!AllowMachineGuns() && weapon->IsKindOf( WEAPONTYPE_MACHINEGUN )) || 
		(!AllowRifles() && weapon->IsKindOf( WEAPONTYPE_RIFLE )) || 
		(!AllowShotguns() && weapon->IsKindOf( WEAPONTYPE_SHOTGUN )) || 
		(!AllowSnipers() && weapon->IsKindOf( WEAPONTYPE_SNIPER_RIFLE )) || 
		(!AllowSubMachineGuns() && weapon->IsKindOf( WEAPONTYPE_SUBMACHINEGUN )) || 
		(!AllowPistols() && weapon->IsKindOf( WEAPONTYPE_PISTOL )) ||
		(!AllowGrenades() && weapon->IsKindOf( WEAPONTYPE_GRENADE )))
	{
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this player is on "defense"
 */
bool CCSBotManager::IsOnDefense( const CCSPlayer *player ) const
{
	switch (GetScenario())
	{
		case SCENARIO_DEFUSE_BOMB:
			return (player->GetTeamNumber() == TEAM_CT);

		case SCENARIO_RESCUE_HOSTAGES:
			return (player->GetTeamNumber() == TEAM_TERRORIST);

		case SCENARIO_ESCORT_VIP:
			return (player->GetTeamNumber() == TEAM_TERRORIST);
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this player is on "offense"
 */
bool CCSBotManager::IsOnOffense( const CCSPlayer *player ) const
{
	return !IsOnDefense( player );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when a map has just been loaded
 */
void CCSBotManager::ServerActivate( void )
{
	m_isMapDataLoaded = false;

	// load the database of bot radio chatter
	TheBotPhrases->Reset();
	TheBotPhrases->Initialize( "BotChatter.db", 0 );

	TheBotProfiles->Reset();
	TheBotProfiles->FindVoiceBankIndex( "BotChatter.db" ); // make sure default voice bank is first
	const char *filename = "BotPackList.db";

	// read in the list of bot profile DBs
	FileHandle_t file = filesystem->Open( filename, "r" );
	if ( !file )
	{
		if ( CSGameRules()->IsPlayingCoopMission() )
			TheBotProfiles->Init( "BotProfileCoop.db" );
		else
			TheBotProfiles->Init( "BotProfile.db" );
	}
	else
	{
		int dataLength = filesystem->Size( filename );
		if ( dataLength > 0 )
		{
			char *dataPointer = new char[ dataLength + 1 ];
			int nDataReadSize = filesystem->Read( dataPointer, dataLength, file );
			if ( nDataReadSize > 0 )
			{
				dataPointer[nDataReadSize] = '\0';

				const char *dataFile = SharedParse( dataPointer );
				const char *token;

				while ( dataFile )
				{
					token = SharedGetToken();
					char *clone = CloneString( token );
					TheBotProfiles->Init( clone );
					delete[] clone;
					dataFile = SharedParse( dataFile );
				}

				delete [] dataPointer;
			}
		}
		filesystem->Close( file );
	}

	// Now that we've parsed all the profiles, we have a list of the voice banks they're using.
	// Go back and parse the custom voice speakables.
	const BotProfileManager::VoiceBankList *voiceBanks = TheBotProfiles->GetVoiceBanks();
	for ( int i=1; i<std::ssize( *voiceBanks ); ++i )
	{
		TheBotPhrases->Initialize( (*voiceBanks)[i], i );
	}

	// tell the Navigation Mesh system what CS spawn points are named
	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		TheNavMesh->SetPlayerSpawnName( "info_enemy_terrorist_spawn" );
	}
	else
	{
		TheNavMesh->SetPlayerSpawnName( "info_player_terrorist" );
	}

	ExtractScenarioData();

	RestartRound();

	TheBotPhrases->OnMapChange();

	m_serverActive = true;
}

void CCSBotManager::ServerDeactivate( void )
{
	m_serverActive = false;
}

void CCSBotManager::ClientDisconnect( CBaseEntity *entity )
{
/*
	if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) )
	{
		FREE_PRIVATE( entity );
	}
*/

	/*
	// make sure voice feedback is turned off
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pEntity );
	if ( pPlayer && pPlayer->IsBot() )
	{
		CCSBot *pBot = static_cast<CCSBot *>(pPlayer);
		if (pBot)
		{
			pBot->EndVoiceFeedback( true );
		}
	}
	*/
}

//--------------------------------------------------------------------------------------------------------------
/**
* Parses out bot name/template/etc params from the current ConCommand
*/
void BotArgumentsFromArgv( const CCommand &args, const char **name, CSWeaponType *weaponType, BotDifficultyType *difficulty, int *team = nullptr, bool *all = nullptr )
{
	static char s_name[MAX_PLAYER_NAME_LENGTH];

	s_name[0] = 0;
	*name = s_name;
	*difficulty = NUM_DIFFICULTY_LEVELS;
	if ( team )
	{
		*team = TEAM_UNASSIGNED;
	}
	if ( all )
	{
		*all = false;
	}

	*weaponType = WEAPONTYPE_UNKNOWN;

	for ( int arg=1; arg<args.ArgC(); ++arg )
	{
		bool found = false;

		const char *token = args[arg];
		if ( all && FStrEq( token, "all" ) )
		{
			*all = true;
			found = true;
		}
		else if ( team && FStrEq( token, "t" ) )
		{
			*team = TEAM_TERRORIST;
			found = true;
		}
		else if ( team && FStrEq( token, "ct" ) )
		{
			*team = TEAM_CT;
			found = true;
		}

		for( int i=0; i<NUM_DIFFICULTY_LEVELS && !found; ++i )
		{
			if (!stricmp( BotDifficultyName[i], token ))
			{
				*difficulty = (BotDifficultyType)i;
				found = true;
			}
		}

		if ( !found )
		{
			*weaponType = WeaponClassFromString( token );
			if ( *weaponType != WEAPONTYPE_UNKNOWN )
			{
				found = true;
			}
		}

		if ( !found )
		{
			V_strncpy( s_name, token, sizeof( s_name ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
static void bot_place( const CCommand &args );
static ConCommand bot_place_command( side_registry(), "bot_place", bot_place, "bot_place - Places a bot from the map at where the local player is pointing.", FCVAR_GAMEDLL | FCVAR_CHEAT );
static void bot_place( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	uint nTeamMask = 0;

	for ( int i = 1; i < args.ArgC(); ++i )
	{
		if ( !V_strcmp( args.Arg( i ), "t" ) )
			nTeamMask |= 1 << TEAM_TERRORIST;
		else if ( !V_strcmp( args.Arg( i ), "ct" ) )
			nTeamMask |= 1 << TEAM_CT;
	}
	if ( !nTeamMask )
		nTeamMask = 0xFFFFFFFF;

	TheCSBots()->BotPlaceCommand(nTeamMask);
}

//--------------------------------------------------------------------------------------------------------------
static void bot_add( const CCommand &args );
static ConCommand bot_add_command( side_registry(), "bot_add", bot_add, "bot_add <t|ct> <type> <difficulty> <name> - Adds a bot matching the given criteria.", FCVAR_GAMEDLL );
static void bot_add( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *name;
	BotDifficultyType difficulty;
	CSWeaponType weaponType;
	int team;
	BotArgumentsFromArgv( args, &name, &weaponType, &difficulty, &team );
	TheCSBots()->BotAddCommand( team, true, name, weaponType, difficulty );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_add_t( const CCommand &args );
static ConCommand bot_add_t_command( side_registry(), "bot_add_t", bot_add_t, "bot_add_t <type> <difficulty> <name> - Adds a terrorist bot matching the given criteria.", FCVAR_GAMEDLL );
static void bot_add_t( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *name;
	BotDifficultyType difficulty;
	CSWeaponType weaponType;
	BotArgumentsFromArgv( args, &name, &weaponType, &difficulty );
	TheCSBots()->BotAddCommand( TEAM_TERRORIST, true, name, weaponType, difficulty );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_add_ct( const CCommand &args );
static ConCommand bot_add_ct_command( side_registry(), "bot_add_ct", bot_add_ct, "bot_add_ct <type> <difficulty> <name> - Adds a Counter-Terrorist bot matching the given criteria.", FCVAR_GAMEDLL );
static void bot_add_ct( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *name;
	BotDifficultyType difficulty;
	CSWeaponType weaponType;
	BotArgumentsFromArgv( args, &name, &weaponType, &difficulty );
	TheCSBots()->BotAddCommand( TEAM_CT, true, name, weaponType, difficulty );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Collects all bots matching the given criteria (player name, profile template name, difficulty, and team)
 */
class CollectBots
{
public:
	CollectBots( const char *name, CSWeaponType weaponType, BotDifficultyType difficulty, int team )
	{
		m_name = name;
		m_difficulty = difficulty;
		m_team = team;
		m_weaponType = weaponType;
	}

	bool operator() ( CBasePlayer *player )
	{
		if ( !player->IsBot() )
		{
			return true;
		}

		CCSBot *bot = ToCSBot( player );
		if ( !bot || !bot->GetProfile() )
		{
			return true;
		}

		if ( m_name && *m_name )
		{
			// accept based on name
			if ( FStrEq( m_name, bot->GetProfile()->GetName() ) )
			{
				m_bots.clear();
				m_bots.push_back( bot );
				return false;
			}

			// Reject based on profile template name
			if ( !bot->GetProfile()->InheritsFrom( m_name ) )
			{
				return true;
			}
		}

		// reject based on difficulty
		if ( m_difficulty != NUM_DIFFICULTY_LEVELS )
		{
			if ( !bot->GetProfile()->IsDifficulty( m_difficulty ) )
			{
				return true;
			}
		}

		// reject based on team
		if ( m_team == TEAM_CT || m_team == TEAM_TERRORIST )
		{
			if ( bot->GetTeamNumber() != m_team )
			{
				return true;
			}
		}

		// reject based on weapon preference
		if ( m_weaponType != WEAPONTYPE_UNKNOWN )
		{
			if ( !bot->GetProfile()->GetWeaponPreferenceCount() )
			{
				return true;
			}

			if ( m_weaponType != WeaponClassFromWeaponID( (CSWeaponID)bot->GetProfile()->GetWeaponPreference( 0 ) ) )
			{
				return true;
			}
		}

		// A match!
		m_bots.push_back( bot );

		return true;
	}

	std::vector< CCSBot * > m_bots;

private:
	const char *m_name;
	CSWeaponType m_weaponType;
	BotDifficultyType m_difficulty;
	int m_team;
};

//--------------------------------------------------------------------------------------------------------------
static void bot_kill( const CCommand &args );
static ConCommand bot_kill_command( side_registry(), "bot_kill", bot_kill, "bot_kill <all> <t|ct> <type> <difficulty> <name> - Kills a specific bot, or all bots, matching the given criteria.", FCVAR_GAMEDLL | FCVAR_CHEAT );
static void bot_kill( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *name;
	BotDifficultyType difficulty;
	CSWeaponType weaponType;
	int team;
	bool all;

	BotArgumentsFromArgv( args, &name, &weaponType, &difficulty, &team, &all );
	if ( (!name || !*name) && team == TEAM_UNASSIGNED && difficulty == NUM_DIFFICULTY_LEVELS )
	{
		all = true;
	}

	CollectBots collector( name, weaponType, difficulty, team );
	ForEachPlayer( collector );

	for ( int i=0; i<std::ssize( collector.m_bots ); ++i )
	{
		CCSBot *bot = collector.m_bots[i];
		if ( !bot->IsAlive() )
			continue;

		bot->CommitSuicide();
		
		if ( !all )
		{
			return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
static void bot_kick( const CCommand &args );
static ConCommand bot_kick_command( side_registry(), "bot_kick", bot_kick, "bot_kick <all> <t|ct> <type> <difficulty> <name> - Kicks a specific bot, or all bots, matching the given criteria.", FCVAR_GAMEDLL );
static void bot_kick( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *name;
	BotDifficultyType difficulty;
	CSWeaponType weaponType;
	int team;
	bool all;

	BotArgumentsFromArgv( args, &name, &weaponType, &difficulty, &team, &all );
	if ( (!name || !*name) && team == TEAM_UNASSIGNED && difficulty == NUM_DIFFICULTY_LEVELS )
	{
		all = true;
	}

	CollectBots collector( name, weaponType, difficulty, team );
	ForEachPlayer( collector );

	for ( int i=0; i<std::ssize( collector.m_bots ); ++i )
	{
		CCSBot *bot = collector.m_bots[i];
		engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( bot->edict() ) ) );
		if ( !all )
		{
			// adjust bot quota so kicked bot is not immediately added back in
			int newQuota = cv_bot_quota.GetInt() - 1;
			cv_bot_quota.SetValue( clamp( newQuota, 0, cv_bot_quota.GetInt() ) );
			return;
		}
	}

	// adjust bot quota so kicked bot is not immediately added back in
	if ( all && (!name || !*name) && team == TEAM_UNASSIGNED && difficulty == NUM_DIFFICULTY_LEVELS )
	{
		cv_bot_quota.SetValue( 0 );
	}
	else
	{
		int newQuota = cv_bot_quota.GetInt() - std::ssize( collector.m_bots );
		cv_bot_quota.SetValue( clamp( newQuota, 0, cv_bot_quota.GetInt() ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
static void bot_knives_only( const CCommand &args );
static ConCommand bot_knives_only_command( side_registry(), "bot_knives_only", bot_knives_only, "Restricts the bots to only using knives", FCVAR_GAMEDLL );
static void bot_knives_only( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	cv_bot_allow_pistols.SetValue( 0 );
	cv_bot_allow_shotguns.SetValue( 0 );
	cv_bot_allow_sub_machine_guns.SetValue( 0 );
	cv_bot_allow_rifles.SetValue( 0 );
	cv_bot_allow_machine_guns.SetValue( 0 );
	cv_bot_allow_grenades.SetValue( 0 );
	cv_bot_allow_snipers.SetValue( 0 );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_pistols_only( const CCommand &args );
static ConCommand bot_pistols_only_command( side_registry(), "bot_pistols_only", bot_pistols_only, "Restricts the bots to only using pistols", FCVAR_GAMEDLL );
static void bot_pistols_only( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	cv_bot_allow_pistols.SetValue( 1 );
	cv_bot_allow_shotguns.SetValue( 0 );
	cv_bot_allow_sub_machine_guns.SetValue( 0 );
	cv_bot_allow_rifles.SetValue( 0 );
	cv_bot_allow_machine_guns.SetValue( 0 );
	cv_bot_allow_grenades.SetValue( 0 );
	cv_bot_allow_snipers.SetValue( 0 );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_snipers_only( const CCommand &args );
static ConCommand bot_snipers_only_command( side_registry(), "bot_snipers_only", bot_snipers_only, "Restricts the bots to only using sniper rifles", FCVAR_GAMEDLL );
static void bot_snipers_only( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	cv_bot_allow_pistols.SetValue( 0 );
	cv_bot_allow_shotguns.SetValue( 0 );
	cv_bot_allow_sub_machine_guns.SetValue( 0 );
	cv_bot_allow_rifles.SetValue( 0 );
	cv_bot_allow_machine_guns.SetValue( 0 );
	cv_bot_allow_grenades.SetValue( 0 );
	cv_bot_allow_snipers.SetValue( 1 );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_all_weapons( const CCommand &args );
static ConCommand bot_all_weapons_command( side_registry(), "bot_all_weapons", bot_all_weapons, "Allows the bots to use all weapons", FCVAR_GAMEDLL );
static void bot_all_weapons( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	cv_bot_allow_pistols.SetValue( 1 );
	cv_bot_allow_shotguns.SetValue( 1 );
	cv_bot_allow_sub_machine_guns.SetValue( 1 );
	cv_bot_allow_rifles.SetValue( 1 );
	cv_bot_allow_machine_guns.SetValue( 1 );
	cv_bot_allow_grenades.SetValue( 1 );
	cv_bot_allow_snipers.SetValue( 1 );
}

//--------------------------------------------------------------------------------------------------------------
static void BotGotoArea( CNavArea *pArea )
{
	if ( !pArea )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (player->IsBot())
		{
			CCSBot *bot = ToCSBot( player );

			if ( bot )
			{
				bot->MoveTo( pArea->GetCenter(), FASTEST_ROUTE );
			}

			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
static void bot_goto_mark( const CCommand &args );
static ConCommand bot_goto_mark_command( side_registry(), "bot_goto_mark", bot_goto_mark, "Sends a bot to the marked nav area (useful for testing navigation meshes)", FCVAR_GAMEDLL | FCVAR_CHEAT );
static void bot_goto_mark( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	// tell the first bot we find to go to our marked area
	BotGotoArea( TheNavMesh->GetMarkedArea() );
}

//--------------------------------------------------------------------------------------------------------------
static void bot_goto_selected( const CCommand &args );
static ConCommand bot_goto_selected_command( side_registry(), "bot_goto_selected", bot_goto_selected, "Sends a bot to the selected nav area (useful for testing navigation meshes)", FCVAR_GAMEDLL | FCVAR_CHEAT );
static void bot_goto_selected( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	// tell the first bot we find to go to our selected area
	BotGotoArea( TheNavMesh->GetSelectedArea() );
}

//--------------------------------------------------------------------------------------------------------------

bool CCSBotManager::ServerCommand( const char *cmd )
{
	return false;
}

bool CCSBotManager::ClientCommand( CBasePlayer *player, const CCommand &args )
{
	return false;
}

/**
 * Process the "bot_add" console command
 */
extern ConVar mp_guardian_special_weapon_needed;
bool CCSBotManager::BotAddCommand( int team, bool isFromConsole, const char *profileName, CSWeaponType weaponType, BotDifficultyType difficulty )
{
	if ( !TheNavMesh->IsLoaded() )
	{
		// [msmith] We don't want to run nav mesh generation if we're on a console.

		// If there isn't a Navigation Mesh in memory, create one
		if ( !TheNavMesh->IsGenerating() )
		{
			if ( !m_isMapDataLoaded )
			{
				TheNavMesh->BeginGeneration();
				m_isMapDataLoaded = true;
			}
			return false;
		}
	}

	// dont allow bots to join if the Navigation Mesh is being generated
	if (TheNavMesh->IsGenerating())
		return false;

	const BotProfile *profile = nullptr;

	if ( !isFromConsole )
	{
		profileName = nullptr;
		difficulty = GetDifficultyLevel();
	}
	else
	{
		if ( difficulty == NUM_DIFFICULTY_LEVELS )
		{
			difficulty = GetDifficultyLevel();
		}

		// if team not specified, check bot_join_team cvar for preference
		if (team == TEAM_UNASSIGNED)
		{
			if (!stricmp( cv_bot_join_team.GetString(), "T" ))
				team = TEAM_TERRORIST;
			else if (!stricmp( cv_bot_join_team.GetString(), "CT" ))
				team = TEAM_CT;
			else
				team = CSGameRules()->SelectDefaultTeam();
		}
	}

	if ( profileName && *profileName )
	{
		// in career, ignore humans, since we want to add anyway
		bool ignoreHumans = CSGameRules()->IsCareer();
		if (UTIL_IsNameTaken( profileName, ignoreHumans ))
		{
			if ( isFromConsole )
			{
				Msg( "Error - %s is already in the game.\n", profileName );
			}
			return true;
		}

		// try to add a bot by name
		profile = TheBotProfiles->GetProfile( profileName, team );
		if ( !profile )
		{
			// try to add a bot by template
			profile = TheBotProfiles->GetProfileMatchingTemplate( profileName, team, difficulty, CSGameRules()->GetMatchDevice() );
			if ( !profile )
			{
				if ( isFromConsole )
				{
					Msg( "Error - no profile for '%s' exists.\n", profileName );
				}
				return true;
			}
		}
	}
	else
	{
		// if team not specified, check bot_join_team cvar for preference
		if (team == TEAM_UNASSIGNED)
		{
			if (!stricmp( cv_bot_join_team.GetString(), "T" ))
				team = TEAM_TERRORIST;
			else if (!stricmp( cv_bot_join_team.GetString(), "CT" ))
				team = TEAM_CT;
			else
			{
				if ( CSGameRules()->IsPlayingCooperativeGametype() )
				{
					team = UTIL_HumansOnTeam( TEAM_CT ) > 0 ? TEAM_TERRORIST : TEAM_CT;
				}
				else
				{
					team = CSGameRules()->SelectDefaultTeam();
				}
			}
		}

		if ( !isFromConsole && !profileName && ( weaponType == WEAPONTYPE_UNKNOWN ) && CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
		{
			//
			// Ensure that we have a sniper profile on the bot team for cooperative game
			//
			int numSniperBots = 0;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CCSBot *player = ToCSBot( UTIL_PlayerByIndex( i ) );
				if ( player == nullptr )
					continue;

				const BotProfile *pExistingBotProfile = player->GetProfile();
				if ( !pExistingBotProfile )
					continue;
				if ( !pExistingBotProfile->GetWeaponPreferenceCount() )
					continue;
				if ( WeaponClassFromWeaponID( pExistingBotProfile->GetWeaponPreference( 0 ) ) == WEAPONTYPE_SNIPER_RIFLE )
				{	// found a bot who will buy a sniper rifle guaranteed
					++ numSniperBots;
				}
			}

			int minNumSniperBots = 1;
			if ( CSGameRules()->IsPlayingCoopGuardian() )
			{
				char const *szWepShortName = mp_guardian_special_weapon_needed.GetString();
				if ( szWepShortName && *szWepShortName && V_strcmp( szWepShortName, "any" ) )
				{
					CSWeaponID csWeaponIdGuardianRequired = AliasToWeaponID( szWepShortName );
					if ( ( csWeaponIdGuardianRequired != WEAPON_NONE ) &&
						( WeaponClassFromWeaponID( csWeaponIdGuardianRequired ) == WEAPONTYPE_SNIPER_RIFLE ) )
					{
						++ minNumSniperBots;
					}
				}
			}
			if ( numSniperBots < minNumSniperBots )
				profile = TheBotProfiles->GetRandomProfile( difficulty, team, WEAPONTYPE_SNIPER_RIFLE );
		}
		if ( !profile )
			profile = TheBotProfiles->GetRandomProfile( difficulty, team, weaponType );

		if (profile == nullptr)
		{
			if ( isFromConsole )
			{
				Msg( "All bot profiles at this difficulty level are in use.\n" );
			}
			return true;
		}
	}

	if (team == TEAM_UNASSIGNED || team == TEAM_SPECTATOR)
	{
		if ( isFromConsole )
		{
			Msg( "Could not add bot to the game: The game is full\n" );
		}
		return false;
	}

	if (CSGameRules()->TeamFull( team ))
	{
		if ( isFromConsole )
		{
			Msg( "Could not add bot to the game: Team is full\n" );
		}
		return false;
	}

	if (CSGameRules()->TeamStacked( team, TEAM_UNASSIGNED ))
	{
		if ( isFromConsole )
		{
			Msg( "Could not add bot to the game: Team is stacked (to disable this check, set mp_autoteambalance to zero, increase mp_limitteams, and restart the round).\n" );
		}
		return false;
	}

	// create the actual bot
	CCSBot *bot = CreateBot<CCSBot>( profile, team );

	if (bot == nullptr)
	{
		if ( isFromConsole )
		{
			Msg( "Error: CreateBot() failed.\n" );
		}
		return false;
	}

	if (isFromConsole)
	{
		// increase the bot quota to account for manually added bot
		cv_bot_quota.SetValue( cv_bot_quota.GetInt() + 1 );
	}

	//
	// In Queued Matchmaking mode bots are always taking spots of humans
	// find a human that is not yet connected and use that human's stats
	// TODO: <vitaliy> this would allow bots to take over humans
	//
	if ( 0 && CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && !CSGameRules()->m_mapQueuedMatchmakingPlayersData.empty() )
	{
		std::vector< uint32 > arrConnectedHumans;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CCSPlayer *pHuman = static_cast<CCSPlayer *>(UTIL_PlayerByIndex( i ));
			if ( pHuman )
			{
				uint32 uiAccountId = pHuman->GetHumanPlayerAccountID();
				if ( uiAccountId )
					arrConnectedHumans.push_back( uiAccountId );
			}
		}

		for ( auto &[uiAccountID, pQMM] : CSGameRules()->m_mapQueuedMatchmakingPlayersData )
		{
			CCSGameRules::CQMMPlayerData_t &qmmPlayerData = *pQMM;
			if ( ks::find_index( arrConnectedHumans, qmmPlayerData.m_uiPlayerAccountId ) != -1 )
				continue;	// such human is already connected
			
			bool bThisPlayerIsFirstTeam = ( qmmPlayerData.m_iDraftIndex < 5 );
			bool bTeamsArePlayingSwitchedSides = CSGameRules() ? CSGameRules()->AreTeamsPlayingSwitchedSides() : false;

			// Force team according to queue reservation
			int nThisHumanTeam = ( bThisPlayerIsFirstTeam == !bTeamsArePlayingSwitchedSides ) ? TEAM_CT : TEAM_TERRORIST;
			if ( nThisHumanTeam != team )
				continue;

			// Otherwise let's take over this human!
			bot->SetHumanPlayerAccountID( qmmPlayerData.m_uiPlayerAccountId );
			engine->SetFakeClientConVarValue( bot->edict(), "name", CFmtStr( "BOT %u", qmmPlayerData.m_uiPlayerAccountId ).Access() );
		}
	}

	return true;
}

/**
 * Process the "bot_place" console command
 */
bool CCSBotManager::BotPlaceCommand( uint nTeamMask )
{
	static int lastBotPlaced = -1;

	int numBots = 0;	
	//Count the number of bots in the map.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSBot *bot = ToCSBot( UTIL_PlayerByIndex( i ) );
		if ( nullptr != bot )
		{
			numBots++;
		}
	}

	if ( numBots <= 0 )
	{
		Msg( "Error: bot_place needs at least one bot already in the map.\n" );
		return false;
	}

	// See which bot is the next one to be placed.
	int nextBotToPlace = (lastBotPlaced+1) % numBots;
	int botCount = 0;
	CCSPlayer *botToMove = nullptr;
	for ( int i = 1; i <= gpGlobals->maxClients && botToMove == nullptr; ++i )
	{
		CCSBot *bot = ToCSBot( UTIL_PlayerByIndex( i ) );
		if ( nullptr != bot )
		{
			if ( nextBotToPlace == botCount )
			{
				if ( nTeamMask & ( 1u << bot->GetTeamNumber() ) )
				{
					botToMove = bot;
				}
				else
				{
					nextBotToPlace = ( nextBotToPlace + 1 ) % numBots;
				}
			}
			botCount++;
		}
	}
	lastBotPlaced = nextBotToPlace;

	CBasePlayer* localPlayer = UTIL_GetCommandClient();

	if ( nullptr == localPlayer )
	{
		Msg( "Error: BotPlaceCommand() could not find a human player to move a bot to.\n" );
		return false;
	}
	if ( nullptr == botToMove )
	{
		Msg( "Error: BotPlaceCommand() could not find a bot to move to player's location.\n" );
		return false;
	}

	Vector forward;
	localPlayer->EyeVectors( &forward, nullptr, nullptr );
	trace_t tr;
	UTIL_ClearTrace( tr );
	// trace forward from the eye
	Vector vEye = localPlayer->GetAbsOrigin() + localPlayer->GetViewOffset(), vTargetEye = vEye + ( forward * PLAYER_USE_RADIUS );
	UTIL_TraceLine( vEye, vTargetEye, CONTENTS_SOLID, EntityFromHandle( localPlayer->GetRefEHandle() ), COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHit() )
		vTargetEye = tr.endpos;

	UTIL_ClearTrace( tr );
	Vector vTargetOrigin = vTargetEye - localPlayer->GetViewOffset() - Vector( 0, 0, ( 0.03125 ) );
	UTIL_TraceLine( vTargetEye, vTargetOrigin, CONTENTS_SOLID, EntityFromHandle( localPlayer->GetRefEHandle() ), COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHit() )
	{
		vTargetOrigin = tr.endpos;
		vTargetOrigin.z += ( 0.03125 );
	}

	botToMove->SetAbsOrigin( vTargetOrigin  );

	return true;
}

int UTIL_CSSBotsInGame()
{
	int count = 0;

	for (int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSBot *player = ToCSBot( UTIL_PlayerByIndex( i ) );

		if ( player == nullptr )
			continue;

		count++;
	}

	return count;
}

bool UTIL_CSSKickBotFromTeam( int kickTeam )
{
	int i;

	// try to kick a dead bot first
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSBot *player = ToCSBot( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (!player->IsAlive() && player->GetTeamNumber() == kickTeam)
		{
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( player->edict() ) ) );

			return true;
		}
	}

	// no dead bots, kick any bot on the given team
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSBot *player = ToCSBot( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (player->GetTeamNumber() == kickTeam)
		{
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( player->edict() ) ) );

			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Keep a minimum quota of bots in the game
 */
void CCSBotManager::MaintainBotQuota( void )
{
	if ( !AreBotsAllowed() )
		return;

	if (TheNavMesh->IsGenerating())
		return;

	int totalHumansInGame = UTIL_HumansInGame();
	int humanPlayersInGame = UTIL_HumansInGame( IGNORE_SPECTATORS, IGNORE_UNASSIGNED );
	int spectatorPlayersInGame = UTIL_SpectatorsInGame();

	// don't add bots until local player has been registered, to make sure he's player ID #1
	if (!engine->IsDedicatedServer() && totalHumansInGame == 0)
		return;

	// new players can't spawn immediately after the round has been going for some time
	if ( !CSGameRules() || !TheCSBots() )
	{
		return;
	}

	int desiredBotCount = cv_bot_quota.GetInt();
	int botsInGame = UTIL_CSSBotsInGame();

	/// isRoundInProgress is true if the round has progressed far enough that new players will join as dead.
	bool isRoundInProgress = CSGameRules()->m_bFirstConnected &&
							 !TheCSBots()->IsRoundOver() &&
							 ( CSGameRules()->GetRoundElapsedTime() >= 20.0f );

	if ( FStrEq( cv_bot_quota_mode.GetString(), "fill" ) )
	{
		// If bot_quota_mode is 'fill', we want the number of bots and humans together to equal bot_quota
		// unless the round is already in progress, in which case we play with what we've been dealt
		// don't do this check in coop mission mode because we change the number every wave
		if ( !isRoundInProgress || CSGameRules()->IsPlayingCoopMission() )
		{
			desiredBotCount = std::max( 0, desiredBotCount - humanPlayersInGame + spectatorPlayersInGame );
		}
		else
		{
			desiredBotCount = botsInGame;
		}
	}
	else if ( FStrEq( cv_bot_quota_mode.GetString(), "match" ) )
	{
		// If bot_quota_mode is 'match', we want the number of bots to be bot_quota * total humans
		// unless the round is already in progress, in which case we play with what we've been dealt
		if ( !isRoundInProgress )
		{
			desiredBotCount = (int)std::max<std::common_type_t<int, float>>( 0, cv_bot_quota.GetFloat() * humanPlayersInGame );
		}
		else
		{
			desiredBotCount = botsInGame;
		}
	}

	// wait for a player to join, if necessary
	if (cv_bot_join_after_player.GetBool())
	{
		if ( humanPlayersInGame == 0 && spectatorPlayersInGame == 0 )
			desiredBotCount = 0;
	}

	// wait until the map has been loaded for a bit, to allow players to transition across
	// the transition without missing the pistol round
	if ( bot_join_delay.GetInt() > CSGameRules()->GetMapElapsedTime() )
	{
		desiredBotCount = 0;
	}

	// If the match is in warmup phase and we don't want any bots in warmup then don't have
	// the bots joining the match during warmup
	if ( !bot_join_in_warmup.GetBool() && CSGameRules()->IsWarmupPeriod() )
	{
		desiredBotCount = 0;
	}

	// if bots will auto-vacate, we need to keep one slot open to allow players to join
	if ( cv_bot_auto_vacate.GetBool() && !CSGameRules()->IsPlayingCoopMission() )
		desiredBotCount = std::min( desiredBotCount, gpGlobals->maxClients - (humanPlayersInGame + 1) );
	else
		desiredBotCount = std::min( desiredBotCount, gpGlobals->maxClients - humanPlayersInGame + spectatorPlayersInGame );

	// Try to balance teams, if we are in the first 20 seconds of a round and bots can join either team.
	if ( botsInGame > 0 && desiredBotCount == botsInGame && CSGameRules()->m_bFirstConnected )
	{
		if ( CSGameRules()->GetRoundElapsedTime() < 20.0f ) // new bots can still spawn during this time
		{
			if ( mp_autoteambalance.GetBool() )
			{
				int numAliveTerrorist;
				int numAliveCT;
				int numDeadTerrorist;
				int numDeadCT;
				CSGameRules()->InitializePlayerCounts( numAliveTerrorist, numAliveCT, numDeadTerrorist, numDeadCT );

				if ( !FStrEq( cv_bot_join_team.GetString(), "T" ) &&
					 !FStrEq( cv_bot_join_team.GetString(), "CT" ) )
				{
					if ( numAliveTerrorist > CSGameRules()->m_iNumCT + 1 )
					{
						if ( UTIL_KickBotFromTeam( TEAM_TERRORIST ) )
							return;
					}
					else if ( numAliveCT > CSGameRules()->m_iNumTerrorist + 1 )
					{
						if ( UTIL_KickBotFromTeam( TEAM_CT ) )
							return;
					}
				}
			}
		}
	}

	// add bots if necessary
	if (desiredBotCount > botsInGame)
	{
		// don't try to add a bot if all teams are full
		if ( !CSGameRules()->TeamFull( TEAM_TERRORIST ) || !CSGameRules()->TeamFull( TEAM_CT ) )
			TheCSBots()->BotAddCommand( TEAM_UNASSIGNED );
	}
	else if (desiredBotCount < botsInGame)
	{
		// kick a bot to maintain quota
		
		// first remove any unassigned bots
		if (UTIL_CSSKickBotFromTeam( TEAM_UNASSIGNED ))
			return;

		int kickTeam;

		CCSMatch* match = CSGameRules()->GetMatch();

		// remove from the team that has more players
		if (CSGameRules()->m_iNumTerrorist > CSGameRules()->m_iNumCT)
		{
			kickTeam = TEAM_TERRORIST;
		}
		else if (CSGameRules()->m_iNumTerrorist < CSGameRules()->m_iNumCT)
		{
			kickTeam = TEAM_CT;
		}

		// remove from the team that's winning				
		else if ( match && match->GetWinningTeam() == TEAM_TERRORIST )
		{
			kickTeam = TEAM_TERRORIST;
		}
		else if ( match && match->GetWinningTeam() == TEAM_CT )
		{
			kickTeam = TEAM_CT;
		}
		else
		{
			// teams and scores are equal, pick a team at random
			kickTeam = (RandomInt( 0, 1 ) == 0) ? TEAM_CT : TEAM_TERRORIST;
		}

		// attempt to kick a bot from the given team
		if (UTIL_CSSKickBotFromTeam( kickTeam ))
			return;

		// if there were no bots on the team, kick a bot from the other team
		if (kickTeam == TEAM_TERRORIST)
			UTIL_CSSKickBotFromTeam( TEAM_CT );
		else
			UTIL_CSSKickBotFromTeam( TEAM_TERRORIST );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Collect all nav areas that overlap the given zone
 */
class CollectOverlappingAreas
{
public:
	CollectOverlappingAreas( CCSBotManager::Zone *zone )
	{
		m_zone = zone;

		zone->m_areaCount = 0;
	}

	bool operator() ( CNavArea *area )
	{
		Extent areaExtent;
		area->GetExtent(&areaExtent);

		if (areaExtent.hi.x >= m_zone->m_extent.lo.x && areaExtent.lo.x <= m_zone->m_extent.hi.x &&
			areaExtent.hi.y >= m_zone->m_extent.lo.y && areaExtent.lo.y <= m_zone->m_extent.hi.y &&
			areaExtent.hi.z >= m_zone->m_extent.lo.z && areaExtent.lo.z <= m_zone->m_extent.hi.z)
		{
			// area overlaps m_zone
			m_zone->m_area[ m_zone->m_areaCount++ ] = area;
			if (m_zone->m_areaCount == CCSBotManager::MAX_ZONE_NAV_AREAS)
			{
				return false;
			}
		}

		return true;
	}

private:
	CCSBotManager::Zone *m_zone;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Search the map entities to determine the game scenario and define important zones.
 */
void CCSBotManager::ExtractScenarioData( void )
{
	if (!TheNavMesh->IsLoaded())
		return;

	m_zoneCount = 0;
	m_gameScenario = SCENARIO_DEATHMATCH;

	//
	// Search all entities in the map and set the game type and
	// store all zones (bomb target, etc).
	//
	CBaseEntity *entity;
	int i;
	for( i=1; i<gpGlobals->maxEntities; ++i )
	{
		entity = CBaseEntity::Instance( INDEXENT( i ) );

		if (entity == nullptr)
			continue;

		bool found = false;
		bool isLegacy = false;

		if (FClassnameIs( entity, "func_bomb_target" ))
		{
			m_gameScenario = SCENARIO_DEFUSE_BOMB;
			found = true;
			isLegacy = false;
		}
		else if (FClassnameIs( entity, "info_bomb_target" ))
		{
			m_gameScenario = SCENARIO_DEFUSE_BOMB;
			found = true;
			isLegacy = true;
		}
		else if (FClassnameIs( entity, "func_hostage_rescue" ))
		{
			m_gameScenario = SCENARIO_RESCUE_HOSTAGES;
			found = true;
			isLegacy = false;
		}
		else if (FClassnameIs( entity, "info_hostage_rescue" ))
		{
			m_gameScenario = SCENARIO_RESCUE_HOSTAGES;
			found = true;
			isLegacy = true;
		}
		else if (FClassnameIs( entity, "hostage_entity" ))
		{
			// some very old maps (ie: cs_assault) use info_player_start
			// as rescue zones, so set the scenario if there are hostages
			// in the map
			m_gameScenario = SCENARIO_RESCUE_HOSTAGES;
		}
		else if (FClassnameIs( entity, "func_vip_safetyzone" ))
		{
			m_gameScenario = SCENARIO_ESCORT_VIP;
			found = true;
			isLegacy = false;
		}

		if (found)
		{
			if (m_zoneCount < MAX_ZONES)
			{
				Vector absmin, absmax;
				entity->CollisionProp()->WorldSpaceAABB( &absmin, &absmax );

				m_zone[ m_zoneCount ].m_isBlocked = false;
				m_zone[ m_zoneCount ].m_center = (isLegacy) ? entity->GetAbsOrigin() : (absmin + absmax)/2.0f;
				m_zone[ m_zoneCount ].m_isLegacy = isLegacy;
				m_zone[ m_zoneCount ].m_index = m_zoneCount;
				m_zone[ m_zoneCount++ ].m_entity = entity;
			}
			else
				Msg( "Warning: Too many zones, some will be ignored.\n" );
		}
	}

	//
	// If there are no zones and the scenario is hostage rescue,
	// use the info_player_start entities as rescue zones.
	//
	if (m_zoneCount == 0 && m_gameScenario == SCENARIO_RESCUE_HOSTAGES)
	{
		for( entity = gEntList.FindEntityByClassname( nullptr, "info_player_start" );
			 entity && !FNullEnt( entity->edict() );
			 entity = gEntList.FindEntityByClassname( entity, "info_player_start" ) )
		{
			if (m_zoneCount < MAX_ZONES)
			{
				m_zone[ m_zoneCount ].m_isBlocked = false;
				m_zone[ m_zoneCount ].m_center = entity->GetAbsOrigin();
				m_zone[ m_zoneCount ].m_isLegacy = true;
				m_zone[ m_zoneCount ].m_index = m_zoneCount;
				m_zone[ m_zoneCount++ ].m_entity = entity;
			}
			else
			{
				Msg( "Warning: Too many zones, some will be ignored.\n" );
			}
		}
	}

	//
	// Collect nav areas that overlap each zone
	//
	for( i=0; i<m_zoneCount; ++i )
	{
		Zone *zone = &m_zone[i];

		if (zone->m_isLegacy)
		{
			const float legacyRange = 256.0f;
			zone->m_extent.lo.x = zone->m_center.x - legacyRange;
			zone->m_extent.lo.y = zone->m_center.y - legacyRange;
			zone->m_extent.lo.z = zone->m_center.z - legacyRange;
			zone->m_extent.hi.x = zone->m_center.x + legacyRange;
			zone->m_extent.hi.y = zone->m_center.y + legacyRange;
			zone->m_extent.hi.z = zone->m_center.z + legacyRange;
		}
		else
		{
			Vector absmin, absmax;
			zone->m_entity->CollisionProp()->WorldSpaceAABB( &absmin, &absmax );

			zone->m_extent.lo = absmin;
			zone->m_extent.hi = absmax;
		}

		// ensure Z overlap
		const float zFudge = 50.0f;
		zone->m_extent.lo.z -= zFudge;
		zone->m_extent.hi.z += zFudge;

		// build a list of nav areas that overlap this zone
		CollectOverlappingAreas collector( zone );
		TheNavMesh->ForAllAreas( collector );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the zone that contains the given position
 */
const CCSBotManager::Zone *CCSBotManager::GetZone( const Vector &pos ) const
{
	for( int z=0; z<m_zoneCount; ++z )
	{
		if (m_zone[z].m_extent.Contains( pos ))
		{
			return &m_zone[z];
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest zone to the given position
 */
const CCSBotManager::Zone *CCSBotManager::GetClosestZone( const Vector &pos ) const
{
	const Zone *close = nullptr;
	float closeRangeSq = 999999999.9f;

	for( int z=0; z<m_zoneCount; ++z )
	{
		if ( m_zone[z].m_isBlocked )
			continue;

		float rangeSq = (m_zone[z].m_center - pos).LengthSqr();

		if (rangeSq < closeRangeSq)
		{
			closeRangeSq = rangeSq;
			close = &m_zone[z];
		}
	}

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return a random position inside the given zone
 */
const Vector *CCSBotManager::GetRandomPositionInZone( const Zone *zone ) const
{
	static Vector pos;

	if (zone == nullptr)
		return nullptr;

	if (zone->m_areaCount == 0)
		return nullptr;

	// pick a random overlapping area
	CNavArea *area = GetRandomAreaInZone(zone);

	// pick a location inside both the nav area and the zone
	/// @todo Randomize this

	if (zone->m_isLegacy)
	{
		/// @todo It is possible that the radius might not overlap this area at all...
		area->GetClosestPointOnArea( zone->m_center, &pos );
	}
	else
	{
		Extent areaExtent;
		area->GetExtent(&areaExtent);
		Extent overlap;
		overlap.lo.x = std::max( areaExtent.lo.x, zone->m_extent.lo.x );
		overlap.lo.y = std::max( areaExtent.lo.y, zone->m_extent.lo.y );
		overlap.hi.x = std::min( areaExtent.hi.x, zone->m_extent.hi.x );
		overlap.hi.y = std::min( areaExtent.hi.y, zone->m_extent.hi.y );

		pos.x = (overlap.lo.x + overlap.hi.x)/2.0f;
		pos.y = (overlap.lo.y + overlap.hi.y)/2.0f;
		pos.z = area->GetZ( pos );
	}

	return &pos;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return a random area inside the given zone
 */
CNavArea *CCSBotManager::GetRandomAreaInZone( const Zone *zone ) const
{
	int areaCount = zone->m_areaCount;
	if( areaCount == 0 )
	{
		contract_assert( false && "CCSBotManager::GetRandomAreaInZone: No areas for this zone" );
		return nullptr;
	}

	// Random, but weighted.  Jump areas score zero, since you aren't ever meant to stop on one of those.
	// Avoid areas score 1 to a normal area's 20 because pathfinding treats Avoid as a 20x penalty.
	int totalWeight = 0;
	for( int areaIndex = 0; areaIndex < areaCount; areaIndex++ )
	{
		CNavArea *currentArea = zone->m_area[areaIndex];
		if( currentArea->GetAttributes() & NAV_MESH_JUMP )
			totalWeight += 0;
		else if( currentArea->GetAttributes() & NAV_MESH_AVOID )
			totalWeight += 1;
		else
			totalWeight += 20;
	}

	if( totalWeight == 0 )
	{
		contract_assert( false && "CCSBotManager::GetRandomAreaInZone: No real areas for this zone" );
		return nullptr;
	}

	int randomPick = RandomInt( 1, totalWeight );

	for( int areaIndex = 0; areaIndex < areaCount; areaIndex++ )
	{
		CNavArea *currentArea = zone->m_area[areaIndex];
		if( currentArea->GetAttributes() & NAV_MESH_JUMP )
			randomPick -= 0;
		else if( currentArea->GetAttributes() & NAV_MESH_AVOID )
			randomPick -= 1;
		else
			randomPick -= 20;

		if( randomPick <= 0 )
			return currentArea;
	}

	// Won't ever get here, but the compiler will cry without it.
	return zone->m_area[0];
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnServerShutdown( IGameEvent *event )
{
	if ( !engine->IsDedicatedServer() )
	{
		// Since we're a listenserver, save some config info for the next time we start up
		static const char *botVars[] =
		{
			"bot_quota",
			"bot_difficulty",
			"bot_chatter",
			"bot_prefix",
			"bot_join_team",
			"bot_defer_to_human_items",
			"bot_defer_to_human_goals",
			"bot_join_after_player",
			"bot_allow_rogues",
			"bot_allow_pistols",
			"bot_allow_shotguns",
			"bot_allow_sub_machine_guns",
			"bot_allow_machine_guns",
			"bot_allow_rifles",
			"bot_allow_snipers",
			"bot_allow_grenades"
			"bot_controllable",
		};
		
		KeyValues *data = new KeyValues( "ServerConfig" );

		// load the config data
		if (data)
		{
			data->LoadFromFile( filesystem, "ServerConfig.vdf", "GAME" );
			for ( int i=0; i<sizeof(botVars)/sizeof(botVars[0]); ++i )
			{
				const char *varName = botVars[i];
				if ( varName )
				{
					ConVar *var = cvar->FindVar( varName );
					if ( var )
					{
						data->SetString( varName, var->GetString() );
					}
				}
			}
			data->SaveToFile( filesystem, "ServerConfig.vdf", "GAME" );
			data->deleteThis();
		}
		return;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnPlayerFootstep( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnPlayerFootstep, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnPlayerRadio( IGameEvent *event )
{
	// if it's an Enemy Spotted radio, update our enemy spotted timestamp
	if ( event->GetInt( "slot" ) == RADIO_ENEMY_SPOTTED )
	{
		// to have some idea of when a human Player has seen an enemy
		SetLastSeenEnemyTimestamp();
	}

	CCSBOTMANAGER_ITERATE_BOTS( OnPlayerRadio, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnPlayerDeath( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnPlayerDeath, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnPlayerFallDamage( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnPlayerFallDamage, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombPickedUp( IGameEvent *event )
{
	// bomb no longer loose
	SetLooseBomb( nullptr );

	CCSBOTMANAGER_ITERATE_BOTS( OnBombPickedUp, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombPlanted( IGameEvent *event )
{
	m_isBombPlanted = true;
	m_bombPlantTimestamp = gpGlobals->curtime;

	CCSBOTMANAGER_ITERATE_BOTS( OnBombPlanted, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombBeep( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnBombBeep, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombDefuseBegin( IGameEvent *event )
{
	m_bombDefuser = static_cast<CCSPlayer *>( UTIL_PlayerByUserId( event->GetInt( "userid" ) ) );

	CCSBOTMANAGER_ITERATE_BOTS( OnBombDefuseBegin, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombDefused( IGameEvent *event )
{
	m_isBombPlanted = false;
	m_bombDefuser = nullptr;

	CCSBOTMANAGER_ITERATE_BOTS( OnBombDefused, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombDefuseAbort( IGameEvent *event )
{
	m_bombDefuser = nullptr;

	CCSBOTMANAGER_ITERATE_BOTS( OnBombDefuseAbort, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBombExploded( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnBombExploded, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnRoundEnd( IGameEvent *event )
{
	m_isRoundOver = true;

	CCSBOTMANAGER_ITERATE_BOTS( OnRoundEnd, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnRoundStart( IGameEvent *event )
{
	RestartRound();

	CCSBOTMANAGER_ITERATE_BOTS( OnRoundStart, event );
}

//--------------------------------------------------------------------------------------------------------------
static CBaseEntity * SelectSpawnSpot( const char *pEntClassName )
{
	CBaseEntity* pSpot = nullptr;

	// Find the next spawn spot.
	pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );

	if ( pSpot == nullptr ) // skip over the null point
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );

	CBaseEntity *pFirstSpot = pSpot;
	do 
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
			{
				pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
				continue;
			}

			// if so, go to pSpot
			return pSpot;
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Pathfind from each zone to a spawn point to ensure it is valid.  Assumes that every spawn can pathfind to
 * every other spawn.
 */
void CCSBotManager::CheckForBlockedZones( void )
{
	CBaseEntity *pSpot = SelectSpawnSpot( "info_player_counterterrorist" );
	if ( !pSpot )
	{
		if ( CSGameRules()->IsPlayingCoopMission() )
		{
			pSpot = SelectSpawnSpot( "info_enemy_terrorist_spawn" );
		}
		else
		{
			pSpot = SelectSpawnSpot( "info_player_terrorist" );
		}
	}

	if ( !pSpot )
		return;

	Vector spawnPos = pSpot->GetAbsOrigin();
	CNavArea *spawnArea = TheNavMesh->GetNearestNavArea( spawnPos );
	if ( !spawnArea )
		return;

	ShortestPathCost costFunc;

	for( int i=0; i<m_zoneCount; ++i )
	{
		if (m_zone[i].m_areaCount == 0)
			continue;

		// just use the first overlapping nav area as a reasonable approximation
		float dist = NavAreaTravelDistance( spawnArea, m_zone[i].m_area[0], costFunc );
		m_zone[i].m_isBlocked = (dist < 0.0f );

		if ( cv_bot_debug.GetInt() == 5 )
		{
			if ( m_zone[i].m_isBlocked )
				DevMsg( "%.1f: Zone %d, area %d (%.0f %.0f %.0f) is blocked from spawn area %d (%.0f %.0f %.0f)\n",
					gpGlobals->curtime, i, m_zone[i].m_area[0]->GetID(),
					m_zone[i].m_area[0]->GetCenter().x, m_zone[i].m_area[0]->GetCenter().y, m_zone[i].m_area[0]->GetCenter().z,
					spawnArea->GetID(),
					spawnPos.x, spawnPos.y, spawnPos.z );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnRoundFreezeEnd( IGameEvent *event )
{
	bool reenableEvents = m_NavBlockedEvent.IsEnabled();

	m_NavBlockedEvent.Enable( false ); // don't listen to nav_blocked events - there could be several, and we don't have bots pathing
	std::vector< CNavArea * >& transientAreas = TheNavMesh->GetTransientAreas();
	for ( int i=0; i<std::ssize( transientAreas ); ++i )
	{
		CNavArea *area = transientAreas[i];
		if ( area->GetAttributes() & NAV_MESH_TRANSIENT )
		{
			area->UpdateBlocked();
		}
	}
	if ( reenableEvents )
	{
		m_NavBlockedEvent.Enable( true );
	}

	CheckForBlockedZones();
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnNavBlocked( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnNavBlocked, event );
	CheckForBlockedZones();
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnDoorMoving( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnDoorMoving, event );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Check all nav areas inside the breakable's extent to see if players would now fall through
 */
namespace
{
class CheckAreasOverlappingBreakable
{
public:
	CheckAreasOverlappingBreakable( CBaseEntity *breakable )
	{
		m_breakable = breakable;
		ICollideable *collideable = breakable->GetCollideable();
		collideable->WorldSpaceSurroundingBounds( &m_breakableExtent.lo, &m_breakableExtent.hi );

		const float expand = 10.0f;
		m_breakableExtent.lo += Vector( -expand, -expand, -expand );
		m_breakableExtent.hi += Vector(  expand,  expand,  expand );
	}

	bool operator() ( CNavArea *area )
	{
		Extent areaExtent;
		area->GetExtent(&areaExtent);

		if (areaExtent.hi.x >= m_breakableExtent.lo.x && areaExtent.lo.x <= m_breakableExtent.hi.x &&
			areaExtent.hi.y >= m_breakableExtent.lo.y && areaExtent.lo.y <= m_breakableExtent.hi.y &&
			areaExtent.hi.z >= m_breakableExtent.lo.z && areaExtent.lo.z <= m_breakableExtent.hi.z)
		{
			// area overlaps the breakable
			area->CheckFloor( m_breakable );
		}

		return true;
	}

private:
	Extent m_breakableExtent;
	CBaseEntity *m_breakable;
};
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBreakBreakable( IGameEvent *event )
{
	CBaseEntity *pEntity = UTIL_EntityByIndex( event->GetInt( "entindex" ) );

	contract_assert( pEntity && "Bad Entity Index received" );

	if ( pEntity )
	{

		CheckAreasOverlappingBreakable collector( pEntity );
		TheNavMesh->ForAllAreas( collector );

		CCSBOTMANAGER_ITERATE_BOTS( OnBreakBreakable, event );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBreakProp( IGameEvent *event )
{
	CBaseEntity *pEntity = UTIL_EntityByIndex( event->GetInt( "entindex" ) );

	contract_assert( pEntity && "Bad Entity Index received" );

	if ( pEntity )
	{

		CheckAreasOverlappingBreakable collector( pEntity );
		TheNavMesh->ForAllAreas( collector );

		CCSBOTMANAGER_ITERATE_BOTS( OnBreakProp, event );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnHostageFollows( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnHostageFollows, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnHostageRescuedAll( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnHostageRescuedAll, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnWeaponFire( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnWeaponFire, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnWeaponFireOnEmpty( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnWeaponFireOnEmpty, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnWeaponReload( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnWeaponReload, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnWeaponZoom( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnWeaponZoom, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnBulletImpact( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnBulletImpact, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnHEGrenadeDetonate( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnHEGrenadeDetonate, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnFlashbangDetonate( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnFlashbangDetonate, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnSmokeGrenadeDetonate( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnSmokeGrenadeDetonate, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnMolotovDetonate( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnMolotovDetonate, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnDecoyDetonate( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnDecoyDetonate, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnDecoyFiring( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnDecoyFiring, event );
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::OnGrenadeBounce( IGameEvent *event )
{
	CCSBOTMANAGER_ITERATE_BOTS( OnGrenadeBounce, event );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Get the time remaining before the planted bomb explodes
 */
float CCSBotManager::GetBombTimeLeft( void ) const
{ 
	return (mp_c4timer.GetFloat() - (gpGlobals->curtime - m_bombPlantTimestamp));
}

//--------------------------------------------------------------------------------------------------------------
void CCSBotManager::SetLooseBomb( CBaseEntity *bomb )
{
	m_looseBomb = bomb;

	if (bomb)
	{
		m_looseBombArea = TheNavMesh->GetNearestNavArea( bomb->GetAbsOrigin() );
	}
	else
	{
		m_looseBombArea = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if player is important to scenario (VIP, bomb carrier, etc)
 */
bool CCSBotManager::IsImportantPlayer( CCSPlayer *player ) const
{
	switch (GetScenario())
	{
		case SCENARIO_DEFUSE_BOMB:
		{
			if (player->GetTeamNumber() == TEAM_TERRORIST && player->HasC4())
				return true;

			/// @todo TEAM_CT's defusing the bomb are important

			return false;
		}

		case SCENARIO_ESCORT_VIP:
		{
			if (player->GetTeamNumber() == TEAM_CT && player->IsVIP())
				return true;

			return false;
		}

		case SCENARIO_RESCUE_HOSTAGES:
		{
			/// @todo TEAM_CT's escorting hostages are important
			return false;
		}
	}

	// everyone is equally important in a deathmatch
	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return priority of player (0 = max pri)
 */
unsigned int CCSBotManager::GetPlayerPriority( CBasePlayer *player ) const
{
	const unsigned int lowestPriority = 0xFFFFFFFF;

	if (!player->IsPlayer())
		return lowestPriority;

	// human players have highest priority
	if (!player->IsBot())
		return 0;

	CCSBot *bot = ToCSBot( player );

	if ( !bot )
		return 0;

	// bots doing something important for the current scenario have high priority
	switch (GetScenario())
	{
		case SCENARIO_DEFUSE_BOMB:
		{
			// the bomb carrier has high priority
			if (bot->GetTeamNumber() == TEAM_TERRORIST && bot->HasC4())
				return 1;

			break;
		}

		case SCENARIO_ESCORT_VIP:
		{
			// the VIP has high priority
			if (bot->GetTeamNumber() == TEAM_CT && bot->m_bIsVIP)
				return 1;

			break;
		}

		case SCENARIO_RESCUE_HOSTAGES:
		{
			// TEAM_CT's rescuing hostages have high priority
			if (bot->GetTeamNumber() == TEAM_CT && bot->GetHostageEscortCount())
				return 1;

			break;
		}
	}

	// everyone else is ranked by their unique ID (which cannot be zero)
	return 1 + bot->GetID();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a random spawn point for the given team (no arg means use both team spawnpoints)
 */
CBaseEntity *CCSBotManager::GetRandomSpawn( int team ) const
{
	std::vector< CBaseEntity * > spawnSet;
	CBaseEntity *spot;

	if (team == TEAM_TERRORIST || team == TEAM_MAXCOUNT)
	{
		const char* szTSpawnEntName = "info_player_terrorist";
		if ( CSGameRules()->IsPlayingCoopMission() )
			szTSpawnEntName = "info_enemy_terrorist_spawn";

		// collect T spawns
		for( spot = gEntList.FindEntityByClassname( nullptr, szTSpawnEntName );
			 spot;
			 spot = gEntList.FindEntityByClassname( spot, szTSpawnEntName ) )
		{
			spawnSet.push_back( spot );			
		}
	}

	if (team == TEAM_CT || team == TEAM_MAXCOUNT)
	{
		// collect CT spawns
		for( spot = gEntList.FindEntityByClassname( nullptr, "info_player_counterterrorist" );
			 spot;
			 spot = gEntList.FindEntityByClassname( spot, "info_player_counterterrorist" ) )
		{
			spawnSet.push_back( spot );			
		}
	}

	if (spawnSet.empty())
	{
		return nullptr;
	}

	// select one at random
	int which = RandomInt( 0, std::ssize( spawnSet )-1 );
	return spawnSet[ which ];
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the last time the given radio message was sent for given team
 * 'teamID' can be TEAM_CT or TEAM_TERRORIST
 */
float CCSBotManager::GetRadioMessageTimestamp( RadioType event, int teamID ) const
{
	int i = (teamID == TEAM_TERRORIST) ? 0 : 1;

	if (event > RADIO_START_1 && event < RADIO_END)
		return m_radioMsgTimestamp[ event - RADIO_START_1 ][ i ];

	return 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the interval since the last time this message was sent
 */
float CCSBotManager::GetRadioMessageInterval( RadioType event, int teamID ) const
{
	int i = (teamID == TEAM_TERRORIST) ? 0 : 1;

	if (event > RADIO_START_1 && event < RADIO_END)
		return gpGlobals->curtime - m_radioMsgTimestamp[ event - RADIO_START_1 ][ i ];

	return 99999999.9f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the given radio message timestamp.
 * 'teamID' can be TEAM_CT or TEAM_TERRORIST
 */
void CCSBotManager::SetRadioMessageTimestamp( RadioType event, int teamID )
{
	int i = (teamID == TEAM_TERRORIST) ? 0 : 1;

	if (event > RADIO_START_1 && event < RADIO_END)
		m_radioMsgTimestamp[ event - RADIO_START_1 ][ i ] = gpGlobals->curtime;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset all radio message timestamps
 */
void CCSBotManager::ResetRadioMessageTimestamps( void )
{
	for( int t=0; t<2; ++t )
	{
		for( int m=0; m<(RADIO_END - RADIO_START_1); ++m )
			m_radioMsgTimestamp[ m ][ t ] = 0.0f;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Display nav areas as they become reachable by each team
 */
void DrawOccupyTime( void )
{
	for ( CNavArea *area : TheNavAreas )
	{

		int r, g, b;
		
		if (TheCSBots()->GetElapsedRoundTime() > area->GetEarliestOccupyTime( TEAM_TERRORIST ))
		{
			if (TheCSBots()->GetElapsedRoundTime() > area->GetEarliestOccupyTime( TEAM_CT ))
			{
				r = 255; g = 0; b = 255;
			}
			else
			{
				r = 255; g = 0; b = 0;
			}
		}
		else if (TheCSBots()->GetElapsedRoundTime() > area->GetEarliestOccupyTime( TEAM_CT ))
		{
			r = 0; g = 0; b = 255;
		}
		else
		{
			continue;
		}

		const Vector &nw = area->GetCorner( NORTH_WEST );
		const Vector &ne = area->GetCorner( NORTH_EAST );
		const Vector &sw = area->GetCorner( SOUTH_WEST );
		const Vector &se = area->GetCorner( SOUTH_EAST );

		NDebugOverlay::Line( nw, ne, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( nw, sw, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( se, sw, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( se, ne, r, g, b, true, 0.1f );		
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Display areas where players will likely have initial battles
 */
void DrawBattlefront( void )
{
	const float epsilon = 1.0f;
	int r = 255, g = 50, b = 0;
	
	for ( CNavArea *area : TheNavAreas )
	{

		if ( fabs(area->GetEarliestOccupyTime( TEAM_TERRORIST ) - area->GetEarliestOccupyTime( TEAM_CT )) > epsilon )
		{
			continue;
		}

		const Vector &nw = area->GetCorner( NORTH_WEST );
		const Vector &ne = area->GetCorner( NORTH_EAST );
		const Vector &sw = area->GetCorner( SOUTH_WEST );
		const Vector &se = area->GetCorner( SOUTH_EAST );

		NDebugOverlay::Line( nw, ne, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( nw, sw, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( se, sw, r, g, b, true, 0.1f );		
		NDebugOverlay::Line( se, ne, r, g, b, true, 0.1f );		
	}
}

//--------------------------------------------------------------------------------------------------------------
static bool CheckAreaAgainstAllZoneAreas(CNavArea *queryArea)
{
	// A marked area means they just want to double check this one spot
	int goalZoneCount = TheCSBots()->GetZoneCount();

	for( int zoneIndex = 0; zoneIndex < goalZoneCount; zoneIndex++ )
	{
		const CCSBotManager::Zone *currentZone = TheCSBots()->GetZone(zoneIndex);

		int zoneAreaCount = currentZone->m_areaCount;
		for( int areaIndex = 0; areaIndex < zoneAreaCount; areaIndex++ )
		{
			CNavArea *zoneArea = currentZone->m_area[areaIndex];
			// We need to be connected to every area in the zone, since we don't know what other code might pick for an area
			ShortestPathCost cost;
			if( NavAreaTravelDistance(queryArea, zoneArea, cost) == -1.0f )
			{
				Msg( "Area #%d is disconnected from goal area #%d.\n", 
					queryArea->GetID(), 
					zoneArea->GetID()
					);
				return false;
			}
		}

	}
	return true;
}

static void nav_check_connectivity( const CCommand &args );
static ConCommand nav_check_connectivity_command( side_registry(), "nav_check_connectivity", nav_check_connectivity, "Checks to be sure every (or just the marked) nav area can get to every goal area for the map (hostages or bomb site).", FCVAR_CHEAT );
static void nav_check_connectivity( const CCommand &args )
{
	//Nav command in here since very CS specific.

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( TheNavMesh->GetMarkedArea() )
	{
		CNavArea *markedArea = TheNavMesh->GetMarkedArea();
		bool fine = CheckAreaAgainstAllZoneAreas( markedArea );
		if( fine )
		{
			Msg( "Area #%d is connected to all goal areas.\n", markedArea->GetID() );
		}
	}
	else
	{
		// Otherwise, loop through every area, and make sure they can all get to the goal.
		float start = Plat_FloatTime();
		for ( CNavArea *area : TheNavAreas )
		{
			CheckAreaAgainstAllZoneAreas(area);
		}

		float end = Plat_FloatTime();
		float time = (end - start) * 1000.0f;
		Msg( "nav_check_connectivity took %2.2f ms\n", time );
	}
}

CBasePlayer *CCSBotManager::AllocateAndBindBotEntity( edict_t *ed )
{
	SetPlayerEdictForNextBot( ed );
	return AllocateBotEntity();
}

CBasePlayer *CCSBotManager::AllocateBotEntity( void )
{
	return static_cast<CBasePlayer *>( CreateEntityByName( "cs_bot" ) );
}

bool CCSBotManager::IsTimeToPlantBomb( void ) const
{
	return (gpGlobals->curtime >= m_earliestBombPlantTimestamp);
}

const CCSBotManager::Zone *CCSBotManager::GetClosestZone( const CBaseEntity *entity ) const
{
	if (entity == nullptr)
		return nullptr;

	Vector centroid = entity->GetAbsOrigin();
	centroid.z += HalfHumanHeight;
	return GetClosestZone( centroid );
}
