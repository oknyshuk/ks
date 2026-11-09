// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003


#include "bot.h"
#include "bot_manager.h"
#include "nav_area.h"
#include "bot_util.h"
#include "basegrenade_shared.h"

#include "cs_bot.h"


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
