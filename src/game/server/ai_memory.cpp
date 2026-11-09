// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An NPC's memory of potential enemies

#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "ai_debug.h"
#include "ai_memory.h"
#include "ai_basenpc.h"

constexpr auto EMEMORY_POOL_SIZE = 64;
constexpr auto AI_FREE_KNOWLEDGE_DURATION = 1.75;

//-----------------------------------------------------------------------------
// AI_EnemyInfo_t
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

AI_EnemyInfo_t::AI_EnemyInfo_t(void) 
{
	hEnemy				= nullptr;
	vLastKnownLocation	= vec3_origin;
	vLastSeenLocation	= vec3_origin;
	timeLastSeen = 0;
	timeFirstSeen = 0;
	timeLastReacquired = 0;
	timeValidEnemy = 0;
	timeLastReceivedDamageFrom = 0;
	timeAtFirstHand = AI_INVALID_TIME;
	nFaction		= FACTION_NONE;
	bDangerMemory = 0;
	bEludedMe = 0;
	bUnforgettable = 0;
	bMobbedMe = 0;
}

 
//-----------------------------------------------------------------------------
// CAI_EnemiesListSaveRestoreOps
//
// Purpose: Handles save and load for enemy memories
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// CAI_Enemies
//
// Purpose: Stores a set of AI_EnemyInfo_t's
//
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CAI_Enemies )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( AI_EnemyInfo_t )

//-----------------------------------------------------------------------------

CAI_Enemies::CAI_Enemies(void)
{
	m_flFreeKnowledgeDuration = AI_FREE_KNOWLEDGE_DURATION;
	m_flEnemyDiscardTime = AI_DEF_ENEMY_DISCARD_TIME;
	m_vecDefaultLKP = vec3_invalid;
	m_vecDefaultLSP = vec3_invalid;
	m_serial = 0;
}


//-----------------------------------------------------------------------------

CAI_Enemies::~CAI_Enemies()
{
}

//-----------------------------------------------------------------------------
// Purpose:	Purges any dead enemies from memory
//-----------------------------------------------------------------------------

AI_EnemyInfo_t *CAI_Enemies::GetFirst( AIEnemiesIter_t *pIter )
{
	*pIter = m_Map.begin();

	if ( *pIter == m_Map.end() )
		return nullptr;

	if ( (*pIter)->second->hEnemy == nullptr )
		return GetNext( pIter );

	return (*pIter)->second.get();
}

//-----------------------------------------------------------------------------

AI_EnemyInfo_t *CAI_Enemies::GetNext( AIEnemiesIter_t *pIter )
{
	if ( *pIter == m_Map.end() )
		return nullptr;

	++*pIter;
	if ( *pIter == m_Map.end() )
		return nullptr;

	if ( (*pIter)->second->hEnemy == nullptr )
		return GetNext( pIter );

	return (*pIter)->second.get();
}
	
//-----------------------------------------------------------------------------

AI_EnemyInfo_t *CAI_Enemies::Find( CBaseEntity *pEntity, bool bTryDangerMemory )
{
	if ( pEntity == AI_UNKNOWN_ENEMY )
		pEntity = nullptr;

	auto it = m_Map.find( pEntity );
	if ( it == m_Map.end() )
	{
		if ( !bTryDangerMemory || ( it = m_Map.find( nullptr ) ) == m_Map.end() )
			return nullptr;
		contract_assert( it->second->bDangerMemory == true );
	}
	return it->second.get();
}


//-----------------------------------------------------------------------------

AI_EnemyInfo_t *CAI_Enemies::GetDangerMemory()
{
	auto it = m_Map.find( nullptr );
	if ( it == m_Map.end() )
		return nullptr;
	contract_assert( it->second->bDangerMemory == true );
	return it->second.get();
}

//-----------------------------------------------------------------------------

bool CAI_Enemies::ShouldDiscardMemory( AI_EnemyInfo_t *pMemory )
{
	CBaseEntity *pEnemy = pMemory->hEnemy;

	if ( pEnemy )
	{
		CAI_BaseNPC *pEnemyNPC = pEnemy->MyNPCPointer();
		if ( pEnemyNPC )
		{
			if ( pEnemyNPC->GetState() == NPC_STATE_DEAD )
				return true;

			// forget about the enemy if he changes faction
			if ( pEnemyNPC->GetFaction() != pMemory->nFaction )
				return true;
		}
	}
	else
	{
		if ( !pMemory->bDangerMemory )
			return true;
	}

	if ( !pMemory->bUnforgettable &&
		 gpGlobals->curtime > pMemory->timeLastSeen + m_flEnemyDiscardTime )
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------

void CAI_Enemies::RefreshMemories(void)
{

	if ( m_flFreeKnowledgeDuration >= m_flEnemyDiscardTime )
	{
		m_flFreeKnowledgeDuration = m_flEnemyDiscardTime - .1;
	}

	// -------------------
	// Check each record
	// -------------------
	
	auto it = m_Map.begin();
	while ( it != m_Map.end() )
	{	
		AI_EnemyInfo_t *pMemory = it->second.get();
		
		if ( ShouldDiscardMemory( pMemory ) )
		{
			it = m_Map.erase( it );
			continue;
		}
		else if ( pMemory->hEnemy )
		{
			if ( gpGlobals->curtime <= pMemory->timeLastSeen + m_flFreeKnowledgeDuration )
			{
				// Free knowledge is ignored if the target has notarget on
				if ( !(pMemory->hEnemy->GetFlags() & FL_NOTARGET) )
				{
					pMemory->vLastKnownLocation = pMemory->hEnemy->GetAbsOrigin();
				}
			}

			if ( gpGlobals->curtime <= pMemory->timeLastSeen )
			{
				pMemory->vLastSeenLocation = pMemory->hEnemy->GetAbsOrigin();
			}
		}
		++it;
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Updates information about our enemies
// Output : Returns true if new enemy, false if already know of enemy
//-----------------------------------------------------------------------------

bool CAI_Enemies::UpdateMemory(CAI_Network* pAINet, CBaseEntity *pEnemy, const Vector &vPosition, float reactionDelay, bool firstHand )
{
	if ( pEnemy == AI_UNKNOWN_ENEMY )
		pEnemy = nullptr;

	const float DIST_TRIGGER_REACQUIRE_SQ			= Square(20.0 * 12.0);
	const float TIME_TRIGGER_REACQUIRE				= 4.0;
	const float MIN_DIST_TIME_TRIGGER_REACQUIRE_SQ 	= Square(4.0 * 12.0);

	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	// -------------------------------------------
	//  Otherwise just update my own
	// -------------------------------------------
	// Update enemy information
	if ( pMemory )
	{
		contract_assert( pEnemy || pMemory->bDangerMemory == true );

		if ( firstHand )
			pMemory->timeLastSeen = gpGlobals->curtime;
		pMemory->bEludedMe = false;
		
		float deltaDist = (pMemory->vLastKnownLocation - vPosition).LengthSqr();
		
		if (deltaDist>DIST_TRIGGER_REACQUIRE_SQ || ( deltaDist>MIN_DIST_TIME_TRIGGER_REACQUIRE_SQ && ( gpGlobals->curtime - pMemory->timeLastSeen ) > TIME_TRIGGER_REACQUIRE ) )
		{
			pMemory->timeLastReacquired = gpGlobals->curtime;
		}

		// Only update if the enemy has moved
		if (deltaDist>Square(12.0))
		{
			pMemory->vLastKnownLocation = vPosition;

		}

		// Update the time at which we first saw him firsthand
		if ( firstHand && pMemory->timeAtFirstHand == AI_INVALID_TIME )
		{
			pMemory->timeAtFirstHand = gpGlobals->curtime;
		}

		return false;
	}

	// If not on my list of enemies add it
	auto pNewMemory = std::make_unique< AI_EnemyInfo_t >();
	AI_EnemyInfo_t *pAddMemory = pNewMemory.get();
	pAddMemory->vLastKnownLocation = vPosition;

	if ( firstHand )
	{
		pAddMemory->timeLastReacquired = pAddMemory->timeFirstSeen = pAddMemory->timeLastSeen = pAddMemory->timeAtFirstHand = gpGlobals->curtime;
	}
	else
	{
		// Block free knowledge
		pAddMemory->timeLastReacquired = pAddMemory->timeFirstSeen = pAddMemory->timeLastSeen = ( gpGlobals->curtime - (m_flFreeKnowledgeDuration + 0.01) );
		pAddMemory->timeAtFirstHand = AI_INVALID_TIME;
	}

	if ( reactionDelay > 0.0 )
		pAddMemory->timeValidEnemy = gpGlobals->curtime + reactionDelay;

	pAddMemory->bEludedMe = false;

	// I'm either remembering a position of an enemy or just a danger position
	pAddMemory->hEnemy = pEnemy;
	pAddMemory->bDangerMemory = ( pEnemy == nullptr );

	if ( pEnemy )
	{
		CAI_BaseNPC *pEnemyNPC = pEnemy->MyNPCPointer();
		if ( pEnemyNPC )
			pAddMemory->nFaction = pEnemyNPC->GetFaction();
	}

	// add to the list
	m_Map.emplace( pEnemy, std::move( pNewMemory ) );
	m_serial++;

	return true;
}

//------------------------------------------------------------------------------
// Purpose : Returns true if this enemy is part of my memory
//------------------------------------------------------------------------------
void CAI_Enemies::OnTookDamageFrom( CBaseEntity *pEnemy )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
		pMemory->timeLastReceivedDamageFrom = gpGlobals->curtime;
}

//------------------------------------------------------------------------------
// Purpose : Returns true if this enemy is part of my memory
//------------------------------------------------------------------------------
bool CAI_Enemies::HasMemory( CBaseEntity *pEnemy )
{
	return ( Find( pEnemy ) != nullptr );
}

//-----------------------------------------------------------------------------
// Purpose:	Clear information about our enemy
//-----------------------------------------------------------------------------
void CAI_Enemies::ClearMemory(CBaseEntity *pEnemy)
{
	m_Map.erase( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: Notes that the given enemy has eluded me
//-----------------------------------------------------------------------------
void CAI_Enemies::MarkAsEluded( CBaseEntity *pEnemy )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	if ( pMemory )
	{
		pMemory->bEludedMe = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns last known posiiton of given enemy
//-----------------------------------------------------------------------------
const Vector &CAI_Enemies::LastKnownPosition( CBaseEntity *pEnemy )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
	{
		m_vecDefaultLKP = pMemory->vLastKnownLocation;
	}
	else
	{
		DevWarning( 2,"Asking LastKnownPosition for enemy that's not in my memory!!\n");
	}
	return m_vecDefaultLKP;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the last position the enemy was SEEN at. This will always be
// different than LastKnownPosition() when the enemy is out of sight, because
// the last KNOWN position will be updated for a number of seconds after the 
// player disappears. 
//-----------------------------------------------------------------------------
const Vector &CAI_Enemies::LastSeenPosition( CBaseEntity *pEnemy )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
	{
		m_vecDefaultLSP = pMemory->vLastSeenLocation;
	}
	else
	{
		DevWarning( 2,"Asking LastSeenPosition for enemy that's not in my memory!!\n");
	}
	return m_vecDefaultLSP;
}

float CAI_Enemies::TimeLastReacquired( CBaseEntity *pEnemy )
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
		return pMemory->timeLastReacquired;

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking TimeLastReacquired for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: Sets position to the last known position of an enemy.  If enemy
//			was not found returns last memory of danger position if it exists
// Output : Returns false is no position is known
//-----------------------------------------------------------------------------
float CAI_Enemies::LastTimeSeen( CBaseEntity *pEnemy, bool bCheckDangerMemory /*= true*/ )
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, bCheckDangerMemory );
	if ( pMemory )
		return pMemory->timeLastSeen;

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking LastTimeSeen for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: Get the time at which the enemy was first seen.
// Output : Returns false is no position is known
//-----------------------------------------------------------------------------
float CAI_Enemies::FirstTimeSeen( CBaseEntity *pEnemy)
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
		return pMemory->timeFirstSeen;

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking FirstTimeSeen for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_Enemies::HasFreeKnowledgeOf( CBaseEntity *pEnemy )
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
	{
		float flFreeKnowledgeTime = pMemory->timeLastSeen + m_flFreeKnowledgeDuration;
		return ( gpGlobals->curtime < flFreeKnowledgeTime );
	}

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking HasFreeKnowledgeOf for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
float CAI_Enemies::LastTimeTookDamageFrom( CBaseEntity *pEnemy)
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
		return pMemory->timeLastReceivedDamageFrom;

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking LastTimeTookDamageFrom for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the time at which the enemy was first seen firsthand
// Input  : *pEnemy - 
// Output : float
//-----------------------------------------------------------------------------
float CAI_Enemies::TimeAtFirstHand( CBaseEntity *pEnemy )
{
	// I've never seen something that doesn't exist
	if (!pEnemy)
		return 0;

	AI_EnemyInfo_t *pMemory = Find( pEnemy, true );
	if ( pMemory )
		return pMemory->timeAtFirstHand;

	if ( pEnemy != AI_UNKNOWN_ENEMY )
		DevWarning( 2,"Asking TimeAtFirstHand for enemy that's not in my memory!!\n");
	return AI_INVALID_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: Sets position to the last known position of an enemy.  If enemy
//			was not found returns last memory of danger position if it exists
// Output : Returns false is no position is known
//-----------------------------------------------------------------------------
bool CAI_Enemies::HasEludedMe( CBaseEntity *pEnemy )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	if ( pMemory )
		return pMemory->bEludedMe;
	return false;
}

void CAI_Enemies::SetTimeValidEnemy( CBaseEntity *pEnemy, float flTime )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	if ( pMemory )
		pMemory->timeValidEnemy = flTime;
}

//-----------------------------------------------------------------------------
void CAI_Enemies::SetUnforgettable( CBaseEntity *pEnemy, bool bUnforgettable )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	if ( pMemory )
		pMemory->bUnforgettable = bUnforgettable;
}

//-----------------------------------------------------------------------------
void CAI_Enemies::SetMobbedMe( CBaseEntity *pEnemy, bool bMobbedMe )
{
	AI_EnemyInfo_t *pMemory = Find( pEnemy );
	if ( pMemory )
		pMemory->bMobbedMe = bMobbedMe;
}

//-----------------------------------------------------------------------------

void CAI_Enemies::SetFreeKnowledgeDuration( float flDuration )
{ 
	m_flFreeKnowledgeDuration = flDuration;	

	if ( m_flFreeKnowledgeDuration >= m_flEnemyDiscardTime )
	{
		// If your free knowledge time is greater than your discard time,
		// you'll forget about secondhand enemies passed to you by squadmates
		// as soon as you're given them.
		contract_assert( m_flFreeKnowledgeDuration < m_flEnemyDiscardTime );

		m_flFreeKnowledgeDuration = m_flEnemyDiscardTime - .1;
	}
}

//-----------------------------------------------------------------------------

void CAI_Enemies::SetEnemyDiscardTime( float flTime )
{ 
	m_flEnemyDiscardTime = flTime;			

	if ( m_flFreeKnowledgeDuration >= m_flEnemyDiscardTime )
	{
		// If your free knowledge time is greater than your discard time,
		// you'll forget about secondhand enemies passed to you by squadmates
		// as soon as you're given them.
		contract_assert( m_flFreeKnowledgeDuration < m_flEnemyDiscardTime );

		m_flFreeKnowledgeDuration = m_flEnemyDiscardTime - .1;
	}
}
