// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Squad classes

#include "string_t.h"
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
#include "ai_squad.h"
#include "ai_squadslot.h"
#include "ai_basenpc.h"

//-----------------------------------------------------------------------------

CAI_SquadManager g_AI_SquadManager;

//-----------------------------------------------------------------------------
// CAI_SquadManager
//
// Purpose: Manages all the squads in the system
//
//-----------------------------------------------------------------------------

CAI_Squad *CAI_SquadManager::FindSquad( string_t squadName )
{
	CAI_Squad* pSquad = m_pSquads;

	while (pSquad)
	{
		if (FStrEq(STRING(squadName),pSquad->GetName()))
		{
			return pSquad;
		}
		pSquad = pSquad->m_pNextSquad;
	}
	return nullptr;
}

//-------------------------------------

CAI_Squad *CAI_SquadManager::CreateSquad(string_t squadName)
{
	CAI_Squad *pResult = new CAI_Squad(squadName);

	// ---------------------------------
	// Only named squads get added to the squad list
	if ( squadName != NULL_STRING )
	{
		pResult->m_pNextSquad = m_pSquads;
		m_pSquads = pResult;
	}
	else
		pResult->m_pNextSquad = nullptr;
	return pResult;
}

//-------------------------------------

int CAI_SquadManager::NumSquads()
{
	int nSquads = 0;
	CAI_Squad* pSquad = m_pSquads;

	while (pSquad)
	{
		nSquads++;
		pSquad = pSquad->GetNext();
	}
	return nSquads;
}

//-------------------------------------

void CAI_SquadManager::DeleteSquad( CAI_Squad *pSquad )
{
	CAI_Squad *pCurSquad = m_pSquads;
	if (pCurSquad == pSquad)
	{
		g_AI_SquadManager.m_pSquads = pCurSquad->m_pNextSquad;
	}
	else
	{
		while (pCurSquad)
		{
			if (pCurSquad->m_pNextSquad == pSquad)
			{
				pCurSquad->m_pNextSquad = pCurSquad->m_pNextSquad->m_pNextSquad;
				break;
			}
			pCurSquad= pCurSquad->m_pNextSquad;
		}
	}
	delete pSquad;
}

//-------------------------------------
// Purpose: Delete all the squads (called between levels / loads)
//-------------------------------------

void CAI_SquadManager::DeleteAllSquads(void) 
{
	CAI_Squad *squad = CAI_SquadManager::m_pSquads;

	while (squad) 
	{
		CAI_Squad *temp = squad->m_pNextSquad;
		delete squad;
		squad = temp;
	}
	CAI_SquadManager::m_pSquads = nullptr;
}

//-----------------------------------------------------------------------------
// CAI_Squad
//
// Purpose: Tracks enemies, squad slots, squad members
//
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( AISquadEnemyInfo_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CAI_Squad )

//-------------------------------------

CAI_Squad::CAI_Squad(string_t newName) 
{
	Init( newName );
}

//-------------------------------------

CAI_Squad::CAI_Squad() 
{
	Init( NULL_STRING );
}

//-------------------------------------

void CAI_Squad::Init(string_t newName) 
{
	m_Name = AllocPooledString( STRING(newName) );
	m_pNextSquad = nullptr;
	m_flSquadSoundWaitTime = 0;
	m_SquadMembers.clear();

	m_flSquadSoundWaitTime	= 0;

	SetSquadInflictor( nullptr );

	m_flEnemyInfoCleanupTime = 0;
	m_pLastFoundEnemyInfo = nullptr;

}

//-------------------------------------

CAI_Squad::~CAI_Squad(void)
{
}

//-------------------------------------

bool CAI_Squad::IsSilentMember( const CAI_BaseNPC *pNPC )
{
	if ( !pNPC || ( pNPC->GetMoveType() == MOVETYPE_NONE && pNPC->GetSolid() == SOLID_NONE ) ) // a.k.a., enemy finder
		return true;
	return pNPC->IsSilentSquadMember();
}

//-------------------------------------
// Purpose: Removes an NPC from a squad
//-------------------------------------

void CAI_Squad::RemoveFromSquad( CAI_BaseNPC *pNPC, bool bDeath )
{
	if ( !pNPC )
		return;

	// Find the index of this squad member
	int member;
	int myIndex = ks::find_index( m_SquadMembers, pNPC );
	if (myIndex == -1)
	{
		DevMsg("ERROR: Attempting to remove non-existing squad membmer!\n");
		return;
	}
	m_SquadMembers.erase( m_SquadMembers.begin() + myIndex );

	// Notify squad members of death 
	if ( bDeath )
	{
		for (member = 0; member < std::ssize( m_SquadMembers ); member++)
		{
			CAI_BaseNPC* pSquadMem = m_SquadMembers[member];
			if (pSquadMem)
			{
				pSquadMem->NotifyDeadFriend(pNPC);
			}
		}
	}

	pNPC->SetSquad(nullptr);
	pNPC->SetSquadName( NULL_STRING );
}

//-------------------------------------
// Purpose: Addes the given NPC to the squad
//-------------------------------------
void CAI_Squad::AddToSquad(CAI_BaseNPC *pNPC)
{
	if ( !pNPC || !pNPC->IsAlive() )
	{
		contract_assert( 0 );
		return;
	}

	if ( pNPC->GetSquad() == this )
		return;

	if ( pNPC->GetSquad() )
	{
		pNPC->GetSquad()->RemoveFromSquad(pNPC);
	}

	if (std::ssize( m_SquadMembers ) == MAX_SQUAD_MEMBERS)
	{
		DevMsg("Error!! Squad %s is too big!!! Replacing last member\n", STRING(this->m_Name) );
		m_SquadMembers.erase( m_SquadMembers.begin() + ( std::ssize( m_SquadMembers )-1 ) );
	}

	m_SquadMembers.push_back( pNPC );
	pNPC->SetSquad( this );
	pNPC->SetSquadName( m_Name );

	if ( std::ssize( m_SquadMembers ) > 1 )
	{
		CAI_BaseNPC *pCopyFrom = m_SquadMembers[0];
		contract_assert( pCopyFrom != nullptr );
		CAI_Enemies *pEnemies = pCopyFrom->GetEnemies();
		AIEnemiesIter_t iter;
		AI_EnemyInfo_t *pInfo = pEnemies->GetFirst( &iter );
		while ( pInfo )
		{
			pNPC->UpdateEnemyMemory( pInfo->hEnemy, pInfo->vLastKnownLocation, pCopyFrom );
			pInfo = pEnemies->GetNext( &iter );
		}
	}

}

//-------------------------------------

CAI_BaseNPC *CAI_Squad::SquadMemberInRange( const Vector &vecLocation, float flDist )
{
	for (int i = 0; i < std::ssize( m_SquadMembers ); i++)
	{
		if (m_SquadMembers[i] != nullptr && (vecLocation - m_SquadMembers[i]->GetAbsOrigin() ).Length2D() <= flDist)
			return m_SquadMembers[i];
	}
	return nullptr;
}

//-------------------------------------
// Purpose: Returns the nearest squad member to the given squad member
//-------------------------------------

CAI_BaseNPC *CAI_Squad::NearestSquadMember( CAI_BaseNPC *pMember )
{
	float			fBestDist	= MAX_COORD_RANGE;
	CAI_BaseNPC		*fNearestEnt = nullptr;
	Vector			fStartLoc = pMember->GetAbsOrigin();
	for (int i = 0; i < std::ssize( m_SquadMembers ); i++)
	{
		if (m_SquadMembers[i] != nullptr)
		{
			float fDist = (fStartLoc - m_SquadMembers[i]->GetAbsOrigin()).Length();
			if (m_SquadMembers[i]	!=	pMember	&&
				fDist				<	fBestDist	)
			{
				fBestDist	= fDist;
				fNearestEnt	= m_SquadMembers[i];
			}
		}
	}
	return fNearestEnt;
}

//-------------------------------------
// Purpose: Return the number of squad members visible to the specified member
//-------------------------------------
int	CAI_Squad::GetVisibleSquadMembers( CAI_BaseNPC *pMember )
{
	int iCount = 0;

	for (int i = 0; i < std::ssize( m_SquadMembers ); i++)
	{
		// Make sure it's not the specified member
		if ( m_SquadMembers[i] != nullptr && pMember != m_SquadMembers[i] )
		{
			if ( pMember->FVisible( m_SquadMembers[i] ) )
			{
				iCount++;
			}
		}
	}

	return iCount;
}

//-------------------------------------
//
//-------------------------------------
CAI_BaseNPC *CAI_Squad::GetSquadMemberNearestTo( const Vector &vecLocation )
{
	CAI_BaseNPC *pNearest = nullptr;
	float		flNearest = std::numeric_limits<float>::max();

	for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
	{
		float flDist;
		flDist = m_SquadMembers[i]->GetAbsOrigin().DistToSqr( vecLocation );

		if( flDist < flNearest )
		{
			flNearest = flDist;
			pNearest = m_SquadMembers[i];
		}
	}

	contract_assert( pNearest != nullptr );
	return pNearest;
}

//-------------------------------------
// Purpose: Returns true if given entity is in the squad
//-------------------------------------
bool CAI_Squad::SquadIsMember( CBaseEntity *pMember )
{
	CAI_BaseNPC *pNPC = pMember->MyNPCPointer();
	if ( pNPC && pNPC->GetSquad() == this )
		return true;

	return false;
}

//-------------------------------------

bool CAI_Squad::IsLeader( CAI_BaseNPC *pNPC )
{
	if ( IsSilentMember( pNPC ) )
		return false;

	if ( !pNPC )
		return false;

	if ( GetLeader() == pNPC )
		return true;

	return false;
}

//-------------------------------------

CAI_BaseNPC *CAI_Squad::GetLeader( void )
{
	CAI_BaseNPC *pLeader = nullptr;
	int nSilentMembers = 0;
	for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
	{
		if ( !IsSilentMember( m_SquadMembers[i] ) )
		{
			if ( !pLeader )
				pLeader = m_SquadMembers[i];
		}
		else
		{
			nSilentMembers++;
		}
	}
	return ( std::ssize( m_SquadMembers ) - nSilentMembers > 1) ? pLeader : nullptr;
}

//-----------------------------------------------------------------------------
CAI_BaseNPC *CAI_Squad::GetFirstMember( AISquadIter_t *pIter, bool bIgnoreSilentMembers )
{
	int i = 0;
	if ( bIgnoreSilentMembers )
	{
		for ( ; i < std::ssize( m_SquadMembers ); i++ )
		{
			if ( !IsSilentMember( m_SquadMembers[i] ) )
				break;
		}
	}

	if ( pIter )
		*pIter = (AISquadIter_t)i;
	if ( i >= std::ssize( m_SquadMembers ) )
		return nullptr;

	return m_SquadMembers[i];
}

//-------------------------------------

CAI_BaseNPC *CAI_Squad::GetNextMember( AISquadIter_t *pIter, bool bIgnoreSilentMembers )
{
	int &i = (int &)*pIter;
	i++;
	if ( bIgnoreSilentMembers )
	{
		for ( ; i < std::ssize( m_SquadMembers ); i++ )
		{
			if ( !IsSilentMember( m_SquadMembers[i] ) )
				break;
		}
	}

	if ( i >= std::ssize( m_SquadMembers ) )
		return nullptr;

	return m_SquadMembers[i];
}

//-------------------------------------
// Purpose: Returns the average of the
// positions of all squad members. 
// Optionally, you may exclude one
// member.
//-------------------------------------
Vector CAI_Squad::ComputeSquadCentroid( bool bIncludeSilentMembers, CBaseCombatCharacter *pExcludeMember )
{
	int count = 0;
	Vector vecSumOfOrigins = Vector( 0, 0, 0 );

	for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
	{
		if ( (m_SquadMembers[i] != pExcludeMember) && (bIncludeSilentMembers||!IsSilentMember(m_SquadMembers[i]) ) )
		{
			count++;
			vecSumOfOrigins+= m_SquadMembers[i]->GetAbsOrigin();
		}
	}

	return vecSumOfOrigins / count;
}

//-------------------------------------
// Purpose: Alert everyone in the squad to the presence of a new enmey
//-------------------------------------

int	CAI_Squad::NumMembers( bool bIgnoreSilentMembers )
{
	int nSilentMembers = 0;
	if ( bIgnoreSilentMembers )
	{
		for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
		{
			if ( IsSilentMember( m_SquadMembers[i] ) )
				nSilentMembers++;
		}
	}
	return ( std::ssize( m_SquadMembers ) - nSilentMembers );
}

//-------------------------------------
// Purpose: Alert everyone in the squad to the presence of a new enmey
//-------------------------------------

void CAI_Squad::SquadNewEnemy( CBaseEntity *pEnemy )
{
	if ( !pEnemy )
	{
		DevMsg( "ERROR: SquadNewEnemy() - pEnemy is NULL!\n" );
		return;
	}

	for (int i = 0; i < std::ssize( m_SquadMembers ); i++)
	{
		CAI_BaseNPC *pMember = m_SquadMembers[i];
		if (pMember)
		{
			// reset members who aren't activly engaged in fighting (only do this if the NPC's using the squad memory, or it'll fail)
			if ( !pMember->GetEnemy() || 
				 ( pMember->GetEnemy() != pEnemy && 
				   !pMember->HasCondition( COND_SEE_ENEMY) &&
				   gpGlobals->curtime - pMember->GetEnemyLastTimeSeen() > 3.0 ) )
			{
				// give them a new enemy
				if( !hl2_episodic.GetBool() || pMember->IsValidEnemy(pEnemy) )
				{
					pMember->SetEnemy( pEnemy );
				}
				// pMember->SetLastAttackTime( 0 );
			}
		}
	}
}

//-------------------------------------
// Purpose: Broadcast a message to all squad members
// Input:	messageID - generic message handle
//			data - generic data handle
//			sender - who sent the message (NULL by default, if not, will not resend to the sender)
//-------------------------------------

int	CAI_Squad::BroadcastInteraction( int interactionType, void *data, CBaseCombatCharacter *sender )
{
	//Must have a squad
	if ( std::ssize( m_SquadMembers ) == 0 )
		return false;

	//Broadcast to all members of the squad
	for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
	{
		CAI_BaseNPC *pMember = m_SquadMembers[i]->MyNPCPointer();
		
		//Validate and don't send again to the sender
		if ( ( pMember != nullptr) && ( pMember != sender ) )
		{
			//Send it
			pMember->DispatchInteraction( interactionType, data, sender );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: is it ok to make a sound of the given priority?  Check for conflicts
// Input  : soundPriority - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_Squad::FOkToMakeSound( int soundPriority )
{
	if (gpGlobals->curtime <= m_flSquadSoundWaitTime)
	{
		if ( soundPriority <= m_nSquadSoundPriority )
			return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: A squad member made an exclusive sound.  Keep track so other squad
//			members don't talk over it
// Input  : soundPriority - for sorting
//			time - 
//-----------------------------------------------------------------------------
void CAI_Squad::JustMadeSound( int soundPriority, float time )
{
	m_flSquadSoundWaitTime = time;
	m_nSquadSoundPriority = soundPriority;
}

//-----------------------------------------------------------------------------
// Purpose: Try to get one of a contiguous range of slots
// Input  : slotIDStart - start of slot range
//			slotIDEnd - end of slot range
//			hEnemy - enemy this slot is for
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_Squad::OccupyStrategySlotRange( CBaseEntity *pEnemy, int slotIDStart, int slotIDEnd, int *pSlot )
{

	// If I'm already occupying this slot
	if ( *pSlot >= slotIDStart && *pSlot <= slotIDEnd)
		return true;

	for ( int i = slotIDStart; i <= slotIDEnd; i++ )
	{
		// Check enemy to see if slot already occupied
		if (!IsSlotOccupied(pEnemy, i))
		{
			// Clear any previous spot;
			if (*pSlot != SQUAD_SLOT_NONE)
			{
				// As a debug measure check to see if slot was filled
				if (!IsSlotOccupied(pEnemy, *pSlot))
				{
					DevMsg( "ERROR! Vacating an empty slot!\n");
				}

				// Free the slot
				VacateSlot(pEnemy, *pSlot);
			}

			// Fill the slot
			OccupySlot(pEnemy, i);
			*pSlot = i;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------

bool CAI_Squad::IsStrategySlotRangeOccupied( CBaseEntity *pEnemy, int slotIDStart, int slotIDEnd )
{
	for ( int i = slotIDStart; i <= slotIDEnd; i++ )
	{
		if (!IsSlotOccupied(pEnemy, i))
			return false;
	}
	return true;
}

//------------------------------------------------------------------------------

void CAI_Squad::VacateStrategySlot( CBaseEntity *pEnemy, int slot)
{
	// If I wasn't taking up a squad slot I'm done
	if (slot == SQUAD_SLOT_NONE)
		return;

	// As a debug measure check to see if slot was filled
	if (!IsSlotOccupied(pEnemy, slot))
	{
		DevMsg( "ERROR! Vacating an empty slot!\n");
	}

	// Free the slot
	VacateSlot(pEnemy, slot);
}

//------------------------------------------------------------------------------

void CAI_Squad::UpdateEnemyMemory( CAI_BaseNPC *pUpdater, CBaseEntity *pEnemy, const Vector &position )
{
	//Broadcast to all members of the squad
	for ( int i = 0; i < std::ssize( m_SquadMembers ); i++ )
	{
		if ( m_SquadMembers[i] != pUpdater )
		{
			m_SquadMembers[i]->UpdateEnemyMemory( pEnemy, position, pUpdater );
		}
	}
}

//------------------------------------------------------------------------------

AISquadEnemyInfo_t *CAI_Squad::FindEnemyInfo( CBaseEntity *pEnemy )
{
	int i;
	if ( gpGlobals->curtime > m_flEnemyInfoCleanupTime )
	{
		if ( !m_EnemyInfos.empty() )
		{
			m_pLastFoundEnemyInfo = nullptr;
			std::set<CBaseEntity *> activeEnemies;

			// Gather up the set of active enemies
			for ( i = 0; i < std::ssize( m_SquadMembers ); i++ )
			{
				CBaseEntity *pMemberEnemy = m_SquadMembers[i]->GetEnemy();
				if ( pMemberEnemy )
				{
					activeEnemies.insert( pMemberEnemy );
				}
			}
			
			// Remove the records for deleted or unused enemies
			for ( i = std::ssize( m_EnemyInfos ) - 1; i >= 0; --i )
			{
				if ( m_EnemyInfos[i].hEnemy == nullptr || !activeEnemies.contains( m_EnemyInfos[i].hEnemy.Get() ) )
				{
					ks::swap_remove( m_EnemyInfos, i );
				}
			}
		}
		
		m_flEnemyInfoCleanupTime = gpGlobals->curtime + 30;
	}

	if ( m_pLastFoundEnemyInfo && m_pLastFoundEnemyInfo->hEnemy == pEnemy )
		return m_pLastFoundEnemyInfo;

	for ( i = 0; i < std::ssize( m_EnemyInfos ); i++ )
	{
		if ( m_EnemyInfos[i].hEnemy == pEnemy )
		{
			m_pLastFoundEnemyInfo = &m_EnemyInfos[i];
			return &m_EnemyInfos[i];
		}
	}

	m_pLastFoundEnemyInfo = nullptr;
	i = std::ssize( m_EnemyInfos ); m_EnemyInfos.emplace_back();
	m_EnemyInfos[i].hEnemy = pEnemy;

	m_pLastFoundEnemyInfo = &m_EnemyInfos[i];
	return &m_EnemyInfos[i];
}

//------------------------------------------------------------------------------

void CAI_Squad::OccupySlot( CBaseEntity *pEnemy, int i )			
{ 
	AISquadEnemyInfo_t *pInfo = FindEnemyInfo( pEnemy );
	pInfo->slots.set(i);
}

//------------------------------------------------------------------------------

void CAI_Squad::VacateSlot( CBaseEntity *pEnemy, int i )			
{ 
	AISquadEnemyInfo_t *pInfo = FindEnemyInfo( pEnemy );
	pInfo->slots.reset(i);
}

//------------------------------------------------------------------------------

bool CAI_Squad::IsSlotOccupied( CBaseEntity *pEnemy, int i ) const	
{ 
	const AISquadEnemyInfo_t *pInfo = FindEnemyInfo( pEnemy );
	return pInfo->slots[i];
}

void CAI_Squad::SquadRemember( int iMemory )
{
	for (int i = 0; i < std::ssize( m_SquadMembers ); i++)
	{
		if (m_SquadMembers[i] != nullptr )
		{
			m_SquadMembers[i]->Remember( iMemory );
		}
	}
}

//------------------------------------------------------------------------------
void CAI_Squad::SetSquadInflictor( CBaseEntity *pInflictor )
{
	m_hSquadInflictor.Set(pInflictor);
}

//------------------------------------------------------------------------------
bool CAI_Squad::IsSquadInflictor( CBaseEntity *pInflictor )
{
	return (m_hSquadInflictor.Get() == pInflictor);
}

//=============================================================================
