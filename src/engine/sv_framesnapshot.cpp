// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for server files

import ks.tier1;

#include "decal.h"
#include "cmodel_engine.h"
#include "sound.h"
#include "keys.h"
#include "vengineserver_impl.h"
#include "server.h"
#include "eiface.h"
#include "edict.h"
#include "server_class.h"
import std;

#include "hltvserver.h"
#include "framesnapshot.h"
#include "sys_dll.h"

ks::fixed_pool CFrameSnapshot::s_Allocator( sizeof(CFrameSnapshot), 64, ks::fixed_pool::grow::fast, "CFrameSnapshot pool", alignof(CFrameSnapshot) );


static ConVar sv_creationtickcheck( "sv_creationtickcheck", "1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Do extended check for encoding of timestamps against tickcount" );
extern	CGlobalVars g_ServerGlobalVariables;

// Expose interface
static CFrameSnapshotManager g_FrameSnapshotManager;
CFrameSnapshotManager *framesnapshotmanager = &g_FrameSnapshotManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFrameSnapshotManager::CFrameSnapshotManager( void ) : m_PackedEntitiesPool( MAX_EDICTS / 16, ks::fixed_pool::grow::slow )
{
	static_assert( INVALID_PACKED_ENTITY_HANDLE == 0 );
	V_memset( m_pLastPackedData, 0x00, MAX_EDICTS * sizeof(PackedEntityHandle_t) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFrameSnapshotManager::~CFrameSnapshotManager( void )
{
#ifdef _DEBUG
	if ( !IsInErrorExit() )
	{
		contract_assert( m_FrameSnapshots.empty() );
		contract_assert( m_PackedEntitiesPool.count() == 0 );
	}
#endif
}

//-----------------------------------------------------------------------------
// Called when a level change happens
//-----------------------------------------------------------------------------

void CFrameSnapshotManager::LevelChanged()
{
	// Clear all lists...
	contract_assert( m_FrameSnapshots.empty() );

	// Release the most recent snapshot...
	m_PackedEntitiesPool.clear();
	m_PackedEntityCache.clear();
	static_assert( INVALID_PACKED_ENTITY_HANDLE == 0 );
	V_memset( m_pLastPackedData, 0x00, MAX_EDICTS * sizeof(PackedEntityHandle_t) );
}

CFrameSnapshot *CFrameSnapshotManager::NextSnapshot( CFrameSnapshot *pSnapshot )
{
	if ( !pSnapshot )
		return nullptr;

	auto next = std::next( pSnapshot->m_ListIter );
	if ( next == m_FrameSnapshots.end() )
		return nullptr;

	// return next element in list
	return *next;
}

CFrameSnapshot*	CFrameSnapshotManager::CreateEmptySnapshot(
	int tickcount, int maxEntities, uint32 nSnapshotSet )
{
	CFrameSnapshot *snap = nullptr;
	{
		std::scoped_lock lock( m_FrameSnapshotsWriteMutex );
		snap = new CFrameSnapshot;
		snap->AddReference();
	}
	snap->m_nSnapshotSet = nSnapshotSet;
	snap->m_nTickCount = tickcount;
	snap->m_nNumEntities = maxEntities;
	snap->m_nValidEntities = 0;
	snap->m_pValidEntities = nullptr;
	snap->m_pHLTVEntityData = nullptr;
	snap->m_pEntities = new CFrameSnapshotEntry[maxEntities];

	CFrameSnapshotEntry *entry = snap->m_pEntities;
	
	// clear entries
	for ( int i=0; i < maxEntities; i++)
	{
		entry->m_pClass = nullptr;
		entry->m_nSerialNumber = -1;
		entry->m_pPackedData = INVALID_PACKED_ENTITY_HANDLE;
		entry++;
	}

	{
		std::scoped_lock lock( m_FrameSnapshotsWriteMutex );
		snap->m_ListIter = m_FrameSnapshots.insert( m_FrameSnapshots.end(), snap );
	}

	return snap;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : framenumber - 
//-----------------------------------------------------------------------------
CFrameSnapshot* CFrameSnapshotManager::TakeTickSnapshot(
	int tickcount, uint32 nSnapshotSet )
{
	unsigned short nValidEntities[MAX_EDICTS];


	CFrameSnapshot *snap = CreateEmptySnapshot(
		tickcount, sv.num_edicts, nSnapshotSet );

	int maxclients = sv.GetClientCount();

	CFrameSnapshotEntry *entry = snap->m_pEntities - 1;
	edict_t *edict= sv.edicts - 1;
	
	// Build the snapshot.
	for ( int i = 0; i < sv.num_edicts; i++ )
	{
		edict++;
		entry++;

		IServerUnknown *pUnk = edict->GetUnknown();

		if ( !pUnk )
			continue;
																		  
		if ( edict->IsFree() )
			continue;
		
		// We don't want entities from inactive clients in the fullpack,
		if ( i > 0 && i <= maxclients )
		{
			// this edict is a client
			if ( !sv.GetClient(i-1)->IsActive() )
				continue;
		}
		
		// entity exists and is not marked as 'free'
		contract_assert( edict->m_NetworkSerialNumber != -1 );
		contract_assert( edict->GetNetworkable() );
		contract_assert( edict->GetNetworkable()->GetServerClass() );

		entry->m_nSerialNumber	= edict->m_NetworkSerialNumber;
		entry->m_pClass			= edict->GetNetworkable()->GetServerClass();
		nValidEntities[snap->m_nValidEntities++] = i;
	}

	// create dynamic valid entities array and copy indices
	snap->m_pValidEntities = new unsigned short[snap->m_nValidEntities];
	V_memcpy( snap->m_pValidEntities, nValidEntities, snap->m_nValidEntities * sizeof(unsigned short) );

	if ( IsHltvActive() )
	{
		snap->m_pHLTVEntityData = new CHLTVEntityData[snap->m_nValidEntities];
		V_memset( snap->m_pHLTVEntityData, 0, snap->m_nValidEntities * sizeof(CHLTVEntityData) );
	}

	snap->m_iExplicitDeleteSlots.assign( m_iExplicitDeleteSlots.begin(), m_iExplicitDeleteSlots.end() );
	m_iExplicitDeleteSlots.clear(); m_iExplicitDeleteSlots.shrink_to_fit();

	return snap;
}

//-----------------------------------------------------------------------------
// Cleans up packed entity data
//-----------------------------------------------------------------------------

void CFrameSnapshotManager::DeleteFrameSnapshot( CFrameSnapshot* pSnapshot )
{
	// Decrement reference counts of all packed entities
	for (int i = 0; i < pSnapshot->m_nNumEntities; ++i)
	{
		if ( pSnapshot->m_pEntities[i].m_pPackedData != INVALID_PACKED_ENTITY_HANDLE )
		{
			RemoveEntityReference( pSnapshot->m_pEntities[i].m_pPackedData );
		}
	}

	
	m_FrameSnapshots.erase( pSnapshot->m_ListIter );
	delete pSnapshot;
}

void CFrameSnapshotManager::RemoveEntityReference( PackedEntityHandle_t handle )
{
	contract_assert( handle != INVALID_PACKED_ENTITY_HANDLE );

	PackedEntity *packedEntity = PackedEntityFromHandle( handle );

	if ( --packedEntity->m_ReferenceCount <= 0)
	{
		std::scoped_lock lock( m_WriteMutex );

		m_PackedEntitiesPool.free( packedEntity );

		// if we have a uncompression cache, remove reference too
		for ( UnpackedDataCache_t &pdc : m_PackedEntityCache )
		{
			if ( pdc.pEntity == packedEntity )
			{
				pdc.pEntity = nullptr;
				pdc.counter = 0;
				break;
			}
		}
	}
}

void CFrameSnapshotManager::AddEntityReference( PackedEntityHandle_t handle )
{
	contract_assert( handle != INVALID_PACKED_ENTITY_HANDLE );

	PackedEntityFromHandle( handle )->m_ReferenceCount++;
}

void CFrameSnapshotManager::AddExplicitDelete( int iSlot )
{
	std::scoped_lock lock( m_WriteMutex );

	if ( ks::find_index( m_iExplicitDeleteSlots, iSlot ) == -1 )
	{
		m_iExplicitDeleteSlots.push_back( iSlot );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the "basis" for encoding m_flAnimTime, m_flSimulationTime has changed 
//  since the time this entity was packed to the time we're trying to re-use the packing.
//-----------------------------------------------------------------------------
bool CFrameSnapshotManager::ShouldForceRepack( CFrameSnapshot* pSnapshot, int entity, PackedEntityHandle_t handle )
{
	if ( sv_creationtickcheck.GetBool() )
	{
		PackedEntity *pe = PackedEntityFromHandle( handle );
		contract_assert( pe );
		if ( pe && pe->ShouldCheckCreationTick() )
		{
			int nCurrentNetworkBase			= g_ServerGlobalVariables.GetNetworkBase( pSnapshot->m_nTickCount, entity );
			int nPackedEntityNetworkBase	= g_ServerGlobalVariables.GetNetworkBase( pe->GetSnapshotCreationTick(), entity );
			if ( nCurrentNetworkBase != nPackedEntityNetworkBase )
			{
				return true;
			}
		}
	}

	return false;
}

bool CFrameSnapshotManager::UsePreviouslySentPacket( CFrameSnapshot* pSnapshot, 
											int entity, int entSerialNumber )
{
	PackedEntityHandle_t handle = m_pLastPackedData[entity]; 
	if ( handle != INVALID_PACKED_ENTITY_HANDLE )
	{
		// NOTE: We can't use the previously sent packet if there was a 
		// serial number change....
		if ( m_pSerialNumber[entity] == entSerialNumber )
		{
			// Check if we need to re-pack entity due to encoding against gpGlobals->tickcount
			if ( framesnapshotmanager->ShouldForceRepack( pSnapshot, entity, handle ) )
			{
				return false;
			}

			contract_assert( entity < pSnapshot->m_nNumEntities );
			pSnapshot->m_pEntities[entity].m_pPackedData = handle;
			PackedEntityFromHandle( handle )->m_ReferenceCount++;
			return true;
		}
		else
		{
			return false;
		}
	}
	
	return false;
}


PackedEntity* CFrameSnapshotManager::GetPreviouslySentPacket( int iEntity, int iSerialNumber )
{
	PackedEntityHandle_t handle = m_pLastPackedData[iEntity]; 
	if ( handle != INVALID_PACKED_ENTITY_HANDLE )
	{
		// NOTE: We can't use the previously sent packet if there was a 
		// serial number change....
		if ( m_pSerialNumber[iEntity] == iSerialNumber )
		{
			return PackedEntityFromHandle( handle );
		}
		else
		{
			return nullptr;
		}
	}
	
	return nullptr;
}

//-----------------------------------------------------------------------------
// Returns the pack data for a particular entity for a particular snapshot
//-----------------------------------------------------------------------------

PackedEntity* CFrameSnapshotManager::CreatePackedEntity( CFrameSnapshot* pSnapshot, int entity )
{
	PackedEntity* pNewEntity = CreateLocalPackedEntity( pSnapshot, entity );

	// Add a reference into the global list of last entity packets seen...
	// and remove the reference to the last entity packet we saw
	if (m_pLastPackedData[entity] != INVALID_PACKED_ENTITY_HANDLE )
	{
		RemoveEntityReference( m_pLastPackedData[entity] );
	}

	m_pLastPackedData[entity]	= pSnapshot->m_pEntities[entity].m_pPackedData;
	m_pSerialNumber[entity]		= pSnapshot->m_pEntities[entity].m_nSerialNumber;
	pNewEntity->m_ReferenceCount++;
	pNewEntity->SetSnapshotCreationTick( pSnapshot->m_nTickCount );

	return pNewEntity;
}

PackedEntity* CFrameSnapshotManager::CreateLocalPackedEntity( CFrameSnapshot* pSnapshot, int entity )
{

	m_WriteMutex.lock();
	PackedEntity *packedEntity = m_PackedEntitiesPool.alloc();
	PackedEntityHandle_t handle = reinterpret_cast< PackedEntityHandle_t >( packedEntity );
	m_WriteMutex.unlock();

	contract_assert( entity < pSnapshot->m_nNumEntities );
	contract_assert( entity >= 0 && entity < std::size( m_pLastPackedData ) );

	packedEntity->m_ReferenceCount = 1;
	packedEntity->m_nEntityIndex = entity;
	pSnapshot->m_pEntities[entity].m_pPackedData = handle;

	return packedEntity;
}




// ------------------------------------------------------------------------------------------------ //
// purpose: lookup cache if we have an uncompressed version of this packed entity
// ------------------------------------------------------------------------------------------------ //
UnpackedDataCache_t *CFrameSnapshotManager::GetCachedUncompressedEntity( PackedEntity *packedEntity )
{
	if ( m_PackedEntityCache.empty() )
	{
		// ops, we have no cache yet, create one and reset counter
		m_nPackedEntityCacheCounter = 0;
		m_PackedEntityCache.assign( 128, {} );

		for ( UnpackedDataCache_t &cacheEntry : m_PackedEntityCache )
		{
			cacheEntry.pEntity = nullptr;
			cacheEntry.counter = 0;
		}
	}

	m_nPackedEntityCacheCounter++;

	// remember oldest cache entry
	UnpackedDataCache_t *pdcOldest = nullptr;
	int oldestValue = m_nPackedEntityCacheCounter;


	for ( UnpackedDataCache_t &cacheEntry : m_PackedEntityCache )
	{
		UnpackedDataCache_t *pdc = &cacheEntry;

		if ( pdc->pEntity == packedEntity )
		{
			// hit, found it, update counter
			pdc->counter = m_nPackedEntityCacheCounter;
			return pdc;
		}

		if( pdc->counter < oldestValue )
		{
			oldestValue = pdc->counter;
			pdcOldest = pdc;
		}
	}

	contract_assert( pdcOldest );

	// hmm, not in cache, clear & return oldest one
	pdcOldest->counter = m_nPackedEntityCacheCounter;
	pdcOldest->bits = -1;	// important, this is the signal for the caller to fill this structure
	pdcOldest->pEntity = packedEntity;
	return pdcOldest;
}

void CFrameSnapshotManager::BuildSnapshotList( CFrameSnapshot *pCurrentSnapshot, CFrameSnapshot *pLastSnapshot, uint32 nSnapshotSet, CReferencedSnapshotList &list )
{
	// Keep list building thread-safe
	std::scoped_lock lock( m_FrameSnapshotsWriteMutex );

	int nInsanity = 0;
	CFrameSnapshot *pSnapshot;
	if ( pLastSnapshot )
	{
		pSnapshot = NextSnapshot( pLastSnapshot );
	} 
	else
	{
		pSnapshot = pCurrentSnapshot;
	}

	while ( pSnapshot )
	{
		//only add snapshots that match the desired set to the list
		if( pSnapshot->m_nSnapshotSet == nSnapshotSet )
		{
			pSnapshot->AddReference();
			list.m_vecSnapshots.push_back( pSnapshot );
		}

		++nInsanity;
		if ( nInsanity > 100000 )
		{
			Error( "CFrameSnapshotManager::BuildSnapshotList:  infinite loop building list!!!" );
		}

		if ( pSnapshot == pCurrentSnapshot )
			break; 

		// got to next snapshot
		pSnapshot = NextSnapshot( pSnapshot );
	}
}

// ------------------------------------------------------------------------------------------------ //
// CFrameSnapshot
// ------------------------------------------------------------------------------------------------ //

#if defined( _DEBUG )
	int g_nAllocatedSnapshots = 0;
#endif


CFrameSnapshot::CFrameSnapshot()
{
	m_nTempEntities = 0;
	m_pTempEntities = nullptr;
	m_pValidEntities = nullptr;
	m_nReferences = 0;
#if defined( _DEBUG )
	++g_nAllocatedSnapshots;
	contract_assert( g_nAllocatedSnapshots < 80000 ); // this probably would indicate a memory leak.
#endif
}


CFrameSnapshot::~CFrameSnapshot()
{
	delete [] m_pValidEntities;
	delete [] m_pEntities;

	if ( m_pTempEntities )
	{
		contract_assert( m_nTempEntities>0 );
		for (int i = 0; i < m_nTempEntities; i++ )
		{
			delete m_pTempEntities[i];
		}

		delete [] m_pTempEntities;
	}

	if ( m_pHLTVEntityData )
	{
		delete [] m_pHLTVEntityData;
	}

	contract_assert( m_nReferences == 0 );

#if defined( _DEBUG )
	--g_nAllocatedSnapshots;
	contract_assert( g_nAllocatedSnapshots >= 0 );
#endif
}


void CFrameSnapshot::AddReference()
{
	contract_assert( m_nReferences < 0xFFFF );
	++m_nReferences;
}

void CFrameSnapshot::ReleaseReference()
{
	// Keep list building thread-safe
	// This lock was moved to to fix bug https://bugbait.valvesoftware.com/show_bug.cgi?id=53403
	// Crash in CFrameSnapshotManager::GetPackedEntity where a CBaseClient's m_pBaseline snapshot could be removed the CReferencedSnapshotList destructor 
	// for another client that is in WriteTempEntities
	//
	std::scoped_lock lock( framesnapshotmanager->m_FrameSnapshotsWriteMutex );

	contract_assert( m_nReferences > 0 );

	--m_nReferences;
	if ( m_nReferences == 0 )
	{
		g_FrameSnapshotManager.DeleteFrameSnapshot( this );
	}
}
