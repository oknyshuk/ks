// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "igameevents.h"
module ks.server;

import std;
import ks.core.dbg;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :fish;
import :nav_4;
import :player;
import :baseentity;
import :sharedvar;
import :vscript;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.core.vprof;
import ks.tier1.strtools;

class CBasePlayer;
class CCSBot;
class CCSBotManager;
class CHostage;
class CNavArea;

using namespace ::ks::reflect;

// Purpose: Encapsulation of the current scenario/game state. Allows each bot imperfect knowledge.

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

//--------------------------------------------------------------------------------------------------------------
CSGameState::CSGameState( CCSBot *owner )
{
	m_owner = owner;
	m_isRoundOver = false;

	m_bombState = MOVING;
	m_lastSawBomber.Invalidate();
	m_lastSawLooseBomb.Invalidate();
	m_isPlantedBombPosKnown = false;
	m_plantedBombsite = UNKNOWN;

	m_bombsiteCount = 0;
	m_bombsiteSearchIndex = 0;

	for( int i=0; i<MAX_HOSTAGES; ++i )
	{
		m_hostage[i].hostage = nullptr;
		m_hostage[i].isValid = false;
		m_hostage[i].isAlive = false;
		m_hostage[i].isFree = true;
		m_hostage[i].knownPos = Vector( 0, 0, 0 );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset at round start
 */
void CSGameState::Reset( void )
{
	m_isRoundOver = false;

	// bomb -----------------------------------------------------------------------
	if ( !CSGameRules()->IsPlayingCoopGuardian() )
	{
		m_lastSawLooseBomb.Invalidate();
		m_bombState = MOVING;
	}
	m_lastSawBomber.Invalidate();
	m_isPlantedBombPosKnown = false;
	m_plantedBombsite = UNKNOWN;

	m_bombsiteCount = TheCSBots()->GetZoneCount();

	int i;
	for( i=0; i<m_bombsiteCount; ++i )
	{
		m_isBombsiteClear[i] = false;
		m_bombsiteSearchOrder[i] = i;
	}

	// shuffle the bombsite search order
	// allows T's to plant at random site, and TEAM_CT's to search in a random order
	// NOTE: VS6 std::random_shuffle() doesn't work well with an array of two elements (most maps)
	for( i=0; i < m_bombsiteCount; ++i )
	{
		int swap = m_bombsiteSearchOrder[i];
		int rnd = RandomInt( i, m_bombsiteCount-1 );
		m_bombsiteSearchOrder[i] = m_bombsiteSearchOrder[ rnd ];
		m_bombsiteSearchOrder[ rnd ] = swap;
	}

	m_bombsiteSearchIndex = 0;

	// hostage ---------------------------------------------------------------------
	InitializeHostageInfo();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnHostageRescuedAll( IGameEvent *event )
{
	m_allHostagesRescued = true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnRoundEnd( IGameEvent *event )
{
	m_isRoundOver = true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnRoundStart( IGameEvent *event )
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnBombPlanted( IGameEvent *event )
{
	// change state - the event is announced to everyone
	SetBombState( PLANTED );

	CBasePlayer *plantingPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );

	// Terrorists always know where the bomb is
	if (m_owner->GetTeamNumber() == TEAM_TERRORIST && plantingPlayer)
	{
		UpdatePlantedBomb( plantingPlayer->GetAbsOrigin() );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnBombDefused( IGameEvent *event )
{
	// change state - the event is announced to everyone
	SetBombState( DEFUSED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update game state based on events we have received
 */
void CSGameState::OnBombExploded( IGameEvent *event )
{
	// change state - the event is announced to everyone
	SetBombState( EXPLODED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * True if round has been won or lost (but not yet reset)
 */
bool CSGameState::IsRoundOver( void ) const
{
	return m_isRoundOver;
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::SetBombState( BombState state )
{
	// if state changed, reset "last seen" timestamps
	if (m_bombState != state)
	{
		m_bombState = state;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::UpdateLooseBomb( const Vector &pos )
{
	m_looseBombPos = pos;
	m_lastSawLooseBomb.Reset();

	// we saw the loose bomb, update our state
	SetBombState( LOOSE );
}

//--------------------------------------------------------------------------------------------------------------
float CSGameState::TimeSinceLastSawLooseBomb( void ) const
{
	return m_lastSawLooseBomb.GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsLooseBombLocationKnown( void ) const
{
	if ( CSGameRules()->IsPlayingCoopGuardian() )
		return true;

	if (m_bombState != LOOSE)
		return false;

	return (m_lastSawLooseBomb.HasStarted()) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
void CSGameState::UpdateBomber( const Vector &pos )
{
	m_bomberPos = pos;
	m_lastSawBomber.Reset();

	// we saw the bomber, update our state
	SetBombState( MOVING );
}

//--------------------------------------------------------------------------------------------------------------
float CSGameState::TimeSinceLastSawBomber( void ) const
{
	return m_lastSawBomber.GetElapsedTime();
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsPlantedBombLocationKnown( void ) const
{
	if (m_bombState != PLANTED)
		return false;

	return m_isPlantedBombPosKnown;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the zone index of the planted bombsite, or UNKNOWN
 */
int CSGameState::GetPlantedBombsite( void ) const
{
	if (m_bombState != PLANTED)
		return UNKNOWN;

	return m_plantedBombsite;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are currently in the bombsite where the bomb is planted
 */
bool CSGameState::IsAtPlantedBombsite( void ) const
{
	if (m_bombState != PLANTED)
		return false;

	Vector myOrigin = GetCentroid( m_owner );
	const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( myOrigin );

	if (zone)
	{
		return (m_plantedBombsite == zone->m_index);
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the zone index of the next bombsite to search
 */
int CSGameState::GetNextBombsiteToSearch( void )
{
	if (m_bombsiteCount <= 0)
		return 0;

	int i;

	// return next non-cleared bombsite index
	for( i=m_bombsiteSearchIndex; i<m_bombsiteCount; ++i )
	{
		int z = m_bombsiteSearchOrder[i];
		if (!m_isBombsiteClear[z])
		{
			m_bombsiteSearchIndex = i;
			return z;
		}
	}

	// all the bombsites are clear, someone must have been mistaken - start search over
	for( i=0; i<m_bombsiteCount; ++i )
		m_isBombsiteClear[i] = false;
	m_bombsiteSearchIndex = 0;

	return GetNextBombsiteToSearch();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns position of bomb in its various states (moving, loose, planted),
 * or NULL if we don't know where the bomb is
 */
const Vector *CSGameState::GetBombPosition( void ) const
{
	switch( m_bombState )
	{
		case MOVING:
		{
			if (!m_lastSawBomber.HasStarted())
				return nullptr;

			return &m_bomberPos;
		}

		case LOOSE:
		{
			if (IsLooseBombLocationKnown())
				return &m_looseBombPos;

			return nullptr;
		}

		case PLANTED:
		{
			if (IsPlantedBombLocationKnown())
				return &m_plantedBombPos;

			return nullptr;
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * We see the planted bomb at 'pos'
 */
void CSGameState::UpdatePlantedBomb( const Vector &pos )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( pos );

	if (zone == nullptr)
	{
        Msg( "ERROR: Bomb planted outside of a zone!\n" );
		m_plantedBombsite = UNKNOWN;
	}
	else
	{
		m_plantedBombsite = zone->m_index;
	}

	m_plantedBombPos = pos;

	// add an epsilon to handle bomb origin slightly embedded in a model/world
	m_plantedBombPos.z += 1.0f;

	m_isPlantedBombPosKnown = true;
	SetBombState( PLANTED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us where the bomb is planted
 */
void CSGameState::MarkBombsiteAsPlanted( int zoneIndex )
{
	m_plantedBombsite = zoneIndex;
	SetBombState( PLANTED );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us a bombsite is clear
 */
void CSGameState::ClearBombsite( int zoneIndex )
{
	if (zoneIndex >= 0 && zoneIndex < m_bombsiteCount)
		m_isBombsiteClear[ zoneIndex ] = true;	
}

//--------------------------------------------------------------------------------------------------------------
bool CSGameState::IsBombsiteClear( int zoneIndex ) const
{
	if (zoneIndex >= 0 && zoneIndex < m_bombsiteCount)
		return m_isBombsiteClear[ zoneIndex ];

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Initialize our knowledge of the number and location of hostages
 */
void CSGameState::InitializeHostageInfo( void )
{
	m_hostageCount = 0;
	m_allHostagesRescued = false;
	m_haveSomeHostagesBeenTaken = false;

	for( int i=0; i<std::ssize( g_Hostages ); ++i )
	{
		m_hostage[ m_hostageCount ].hostage = g_Hostages[i];
		m_hostage[ m_hostageCount ].knownPos = g_Hostages[i]->GetAbsOrigin();
		m_hostage[ m_hostageCount ].isValid = true;
		m_hostage[ m_hostageCount ].isAlive = true;
		m_hostage[ m_hostageCount ].isFree = true;
		++m_hostageCount;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest free and live hostage
 * If we are a CT this information is perfect. 
 * Otherwise, this is based on our individual memory of the game state.
 * If NULL is returned, we don't think there are any hostages left, or we dont know where they are.
 * NOTE: a T can remember a hostage who has died.  knowPos will be filled in, but NULL will be
 *       returned, since CHostages get deleted when they die.
 */
CHostage *CSGameState::GetNearestFreeHostage( Vector *knowPos ) const
{
	if (m_owner == nullptr)
		return nullptr;

	CNavArea *startArea = m_owner->GetLastKnownArea();
	if (startArea == nullptr)
		return nullptr;

	CHostage *close = nullptr;
	Vector closePos( 0, 0, 0 );
	float closeDistance = 9999999999.9f;

	for( int i=0; i<m_hostageCount; ++i )
	{
		CHostage *hostage = m_hostage[i].hostage;
		Vector hostagePos;

		if (m_owner->GetTeamNumber() == TEAM_CT)
		{
			// we know exactly where the hostages are, and if they are alive
			if (!m_hostage[i].hostage || !m_hostage[i].hostage->IsValid())
				continue;

			if (m_hostage[i].hostage->IsFollowingSomeone())
				continue;

			hostagePos = m_hostage[i].hostage->GetAbsOrigin();
		}
		else
		{
			// use our memory of where we think the hostages are
			if (m_hostage[i].isValid == false)
				continue;

			hostagePos = m_hostage[i].knownPos;
		}

		CNavArea *hostageArea = TheNavMesh->GetNearestNavArea( hostagePos );
		if (hostageArea)
		{
			ShortestPathCost cost;
			float travelDistance = NavAreaTravelDistance( startArea, hostageArea, cost );

			if (travelDistance >= 0.0f && travelDistance < closeDistance)
			{
				closeDistance = travelDistance;
				closePos = hostagePos; 
				close = hostage;
			}
		}
	}

	// return where we think the hostage is
	if (knowPos && close)
		*knowPos = closePos;

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the location of a "free" hostage, or NULL if we dont know of any
 */
const Vector *CSGameState::GetRandomFreeHostagePosition( void ) const
{
	if (m_owner == nullptr)
		return nullptr;

	static Vector freePos[ MAX_HOSTAGES ];
	int freeCount = 0;
	
	for( int i=0; i<m_hostageCount; ++i )
	{
		const HostageInfo *info = &m_hostage[i];

		if (m_owner->GetTeamNumber() == TEAM_CT)
		{
			// we know exactly where the hostages are, and if they are alive
			if (!info->hostage || !info->hostage->IsAlive())
				continue;

			// escorted hostages are not "free"
			if (info->hostage->IsFollowingSomeone())
				continue;

			freePos[ freeCount++ ] = info->hostage->GetAbsOrigin();
		}
		else
		{
			// use our memory of where we think the hostages are
			if (info->isValid == false)
				continue;

			freePos[ freeCount++ ] = info->knownPos;
		}
	}

	if (freeCount)
	{
		return &freePos[ RandomInt( 0, freeCount-1 ) ];
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * If we can see any of the positions where we think a hostage is, validate it
 * Return status of any changes (a hostage died or was moved)
 */
unsigned char CSGameState::ValidateHostagePositions( void )
{
	// limit how often we validate
	if (!m_validateInterval.IsElapsed())
		return NO_CHANGE;

	const float validateInterval = 0.5f;
	m_validateInterval.Start( validateInterval );

	// check the status of hostages
	unsigned char status = NO_CHANGE;

	int i;
	int startValidCount = 0;
	for( i=0; i<m_hostageCount; ++i )
		if (m_hostage[i].isValid)
			++startValidCount;

	for( i=0; i<m_hostageCount; ++i )
	{
		HostageInfo *info = &m_hostage[i];

		if  (!info->hostage )
			continue;

		// if we can see a hostage, update our knowledge of it
		Vector pos = info->hostage->GetAbsOrigin() + Vector( 0, 0, HalfHumanHeight );
		if (m_owner->IsVisible( pos, true ))
		{
			if (info->hostage->IsAlive())
			{
				// live hostage

				// if hostage is being escorted by a CT, we don't "see" it, we see the CT
				if (info->hostage->IsFollowingSomeone())
				{
					info->isValid = false;
				}
				else
				{
					info->knownPos = info->hostage->GetAbsOrigin();
					info->isValid = true;
				}
			}
			else
			{
				// dead hostage

				// if we thought it was alive, this is news to us
				if (info->isAlive)
					status |= HOSTAGE_DIED;

				info->isAlive = false;
				info->isValid = false;
			}

			continue;
		}

		// if we dont know where this hostage is, nothing to validate
		if (!info->isValid)
			continue;

		// can't directly see this hostage
		// check line of sight to where we think this hostage is, to see if we noticed that is has moved
		pos = info->knownPos + Vector( 0, 0, HalfHumanHeight );
		if (m_owner->IsVisible( pos, true ))
		{
			// we can see where we thought the hostage was - verify it is still there and alive

			if (!info->hostage->IsValid())
			{
				// since we have line of sight to an invalid hostage, it must be dead
				// discovered that hostage has been killed
				status |= HOSTAGE_DIED;
				info->isAlive = false;
				info->isValid = false;
				continue;
			}

			if (info->hostage->IsFollowingSomeone())
			{
				// discovered the hostage has been taken
				status |= HOSTAGE_GONE;
				info->isValid = false;
				continue;
			}

			const float tolerance = 50.0f;
			if ((info->hostage->GetAbsOrigin() - info->knownPos).IsLengthGreaterThan( tolerance ))
			{
				// discovered that hostage has been moved
				status |= HOSTAGE_GONE;
				info->isValid = false;
				continue;
			}
		}
	}

	int endValidCount = 0;
	for( i=0; i<m_hostageCount; ++i )
		if (m_hostage[i].isValid)
			++endValidCount;

	if (endValidCount == 0 && startValidCount > 0)
	{
		// we discovered all the hostages are gone
		status &= ~HOSTAGE_GONE;
		status |= HOSTAGES_ALL_GONE;
	}

	return status;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the nearest visible free hostage
 * Since we can actually see any hostage we return, we know its actual position
 */
CHostage *CSGameState::GetNearestVisibleFreeHostage( void ) const
{
	CHostage *close = nullptr;
	float closeRangeSq = 999999999.9f;
	float rangeSq;

	Vector pos;
	Vector myOrigin = GetCentroid( m_owner );

	for( int i=0; i<m_hostageCount; ++i )
	{
		const HostageInfo *info = &m_hostage[i];

		if ( !info->hostage )
			continue;

		// if the hostage is dead or rescued, its not free
		if (!info->hostage->IsAlive())
			continue;

		// if this hostage is following someone, its not free
		if (info->hostage->IsFollowingSomeone())
			continue;

		/// @todo Use travel distance here
		pos = info->hostage->GetAbsOrigin();
		rangeSq = (pos - myOrigin).LengthSqr();

		if (rangeSq < closeRangeSq)
		{
			if (!m_owner->IsVisible( pos ))
				continue;

			close = info->hostage;
			closeRangeSq = rangeSq;
		}
	}

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if there are no free hostages
 */
bool CSGameState::AreAllHostagesBeingRescued( void ) const
{
	// if the hostages have all been rescued, they are not being rescued any longer
	if (m_allHostagesRescued)
		return false;

	bool isAllDead = true;

	for( int i=0; i<m_hostageCount; ++i )
	{
		const HostageInfo *info = &m_hostage[i];

		if (m_owner->GetTeamNumber() == TEAM_CT)
		{
			// CT's have perfect knowledge via their radar
			if (info->hostage && info->hostage->IsValid())
			{
				if (!info->hostage->IsFollowingSomeone())
					return false;

				isAllDead = false;
			}
		}
		else
		{
			if (info->isValid && info->isAlive)
				return false;

			if (info->isAlive)
				isAllDead = false;
		}
	}	

	// if all of the remaining hostages are dead, they arent being rescued
	if (isAllDead)
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * All hostages have been rescued or are dead
 */
bool CSGameState::AreAllHostagesGone( void ) const
{
	if (m_allHostagesRescued)
		return true;

	// do we know that all the hostages are dead
	for( int i=0; i<m_hostageCount; ++i )
	{
		const HostageInfo *info = &m_hostage[i];

		if (m_owner->GetTeamNumber() == TEAM_CT)
		{
			// CT's have perfect knowledge via their radar
			if (info->hostage && info->hostage->IsAlive())
				return false;
		}
		else
		{
			if (info->isValid && info->isAlive)
				return false;
		}
	}	

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Someone told us all the hostages are gone
 */
void CSGameState::AllHostagesGone( void )
{
	for( int i=0; i<m_hostageCount; ++i )
		m_hostage[i].isValid = false;
}

// Purpose: dll-agnostic routines (no dll dependencies here)

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

static char s_shared_token[ 1500 ];
static char s_shared_quote = '\"';

//--------------------------------------------------------------------------------------------------------------
char * SharedVarArgs(const char *format, ...)
{
	std::va_list argptr;
	const int BufLen = 1024;
	const int NumBuffers = 4;
	static char string[NumBuffers][BufLen];
	static int curstring = 0;
	
	curstring = ( curstring + 1 ) % NumBuffers;

	__builtin_va_start (argptr, format);
	std::vsnprintf( string[curstring], BufLen, format, argptr );
	__builtin_va_end (argptr);

	return string[curstring];  
}

//--------------------------------------------------------------------------------------------------------------
char * BufPrintf(char *buf, int& len, const char *fmt, ...)
{
	if (len <= 0)
		return nullptr;

	std::va_list argptr;

	__builtin_va_start(argptr, fmt);
	std::vsnprintf(buf, len, fmt, argptr);
	__builtin_va_end(argptr);
	// Make sure the buffer is null-terminated.
	buf[ len - 1 ] = 0;

	len -= std::strlen(buf);
	return buf + std::strlen(buf);
}

//--------------------------------------------------------------------------------------------------------------
// dgoodenough - PS3 needs this guy as well.
// PS3_BUILDFIX
//--------------------------------------------------------------------------------------------------------------
const char * NumAsString( int val )
{
	const int BufLen = 16;
	static char buf[BufLen];
	int len = BufLen;
	BufPrintf( buf, len, "%d", val );
	return buf;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char *SharedGetToken( void )
{
	return s_shared_token;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
void SharedSetQuoteChar( char c )
{
	s_shared_quote = c;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char *SharedParse( const char *data )
{
	int             c;
	int             len;
	
	len = 0;
	s_shared_token[0] = 0;
	
	if (!data)
		return nullptr;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return nullptr;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	
// handle quoted strings specially
	if (c == s_shared_quote)
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c==s_shared_quote || !c)
			{
				s_shared_token[len] = 0;
				return data;
			}
			s_shared_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		s_shared_token[len] = c;
		len++;
		s_shared_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		s_shared_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	s_shared_token[len] = 0;
	return data;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char *buffer )
{
	const char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !V_isspace( *p ) || V_isalnum( *p ) )
			return true;

		p++;
	}

	return false;
}
