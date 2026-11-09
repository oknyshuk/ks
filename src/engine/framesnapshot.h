#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import ks.tier1;
import std;
#include "const.h"
#include "packed_entity.h"


class PackedEntity;
class HLTVEntityData;
class ReplayEntityData;
class ServerClass;
class CEventInfo;

inline constexpr auto INVALID_PACKED_ENTITY_HANDLE = (0);
typedef intp PackedEntityHandle_t;

#ifdef _DEBUG
// You can also enable snapshot references debugging in release mode
#endif

//-----------------------------------------------------------------------------
// Purpose: Individual entity data, did the entity exist and what was it's serial number
//-----------------------------------------------------------------------------
class CFrameSnapshotEntry
{
public:
	ServerClass*			m_pClass;
	int						m_nSerialNumber;
	// Keeps track of the fullpack info for this frame for all entities in any pvs:
	PackedEntityHandle_t	m_pPackedData;

	std::size_t GetMemSize() const;
};

// HLTV needs some more data per entity 
class CHLTVEntityData
{
public:
	vec_t			origin[3];	// entity position
	unsigned int	m_nNodeCluster;  // if (1<<31) is set it's a node, otherwise a cluster
};

// Replay needs some more data per entity 
class CReplayEntityData
{
public:
	vec_t			origin[3];	// entity position
	unsigned int	m_nNodeCluster;  // if (1<<31) is set it's a node, otherwise a cluster
};

typedef struct
{
	PackedEntity	*pEntity;	// original packed entity
	int				counter;	// increaseing counter to find LRU entries
	int				bits;		// uncompressed data length in bits
	char		data[MAX_PACKEDENTITY_DATA] [[gnu::aligned(4)]]; // uncompressed data cache
} UnpackedDataCache_t;



//-----------------------------------------------------------------------------
// Purpose: For all entities, stores whether the entity existed and what frame the
//  snapshot is for.  Also tracks whether the snapshot is still referenced.  When no
//  longer referenced, it's freed
//-----------------------------------------------------------------------------
class CFrameSnapshot : public ks::pooled< CFrameSnapshot >
{
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CFrameSnapshot >;

public:
	// Reference-counting.
	void					AddReference();
	void					ReleaseReference();
	void					AddRef() { AddReference(); }
	void					Release() { ReleaseReference(); }

	std::size_t					GetMemSize()const;
public:
	// Associated frame. 
	int						m_nTickCount; // = sv.tickcount
	
	// State information
	CFrameSnapshotEntry		*m_pEntities;	
	int						m_nNumEntities; // = sv.num_edicts

	// This list holds the entities that are in use and that also aren't entities for inactive clients.
	unsigned short			*m_pValidEntities; 
	int						m_nValidEntities;

	// Additional HLTV info
	CHLTVEntityData			*m_pHLTVEntityData; // is NULL if not in HLTV mode or array of m_pValidEntities entries
	CReplayEntityData		*m_pReplayEntityData; // is NULL if not in replay mode or array of m_pValidEntities entries

	CEventInfo				**m_pTempEntities; // temp entities
	int						m_nTempEntities;

	std::vector< int >			m_iExplicitDeleteSlots;

private:

	//don't allow for creation/destruction outside of frame snapshot manager and reference counting
	friend class CFrameSnapshotManager;
	CFrameSnapshot();
	~CFrameSnapshot();

	// Position in CFrameSnapshotManager::m_FrameSnapshots.
	std::list< CFrameSnapshot * >::iterator m_ListIter;

	//the set that this snapshot belongs to
	uint32					m_nSnapshotSet;

	// Snapshots auto-delete themselves when their refcount goes to zero.
	std::atomic<int>			m_nReferences;

	char					m_chDebugSnapshotName[128];
};

class CReferencedSnapshotList
{
public:
	~CReferencedSnapshotList()
	{
		for ( int i = 0; i < std::ssize( m_vecSnapshots ); ++i )
		{
			m_vecSnapshots[ i ]->ReleaseReference();
		}
		m_vecSnapshots.clear();
	}

	std::vector< CFrameSnapshot * > m_vecSnapshots;
};

//-----------------------------------------------------------------------------
// Purpose: snapshot manager class
//-----------------------------------------------------------------------------
class CFrameSnapshotManager
{
	friend class CFrameSnapshot;

public:
	CFrameSnapshotManager( void );
	~CFrameSnapshotManager( void );

	// IFrameSnapshot implementation.
public:

	//the default identifier to use for snapshots that are created
	static const uint32 knDefaultSnapshotSet = 0;

	// Called when a level change happens
	void			LevelChanged();

	// Called once per frame after simulation to store off all entities.
	// Note: the returned snapshot has a recount of 1 so you MUST call ReleaseReference on it. Snapshots are created into different sets,
	// so when enumerating snapshots, you only collect those in the desired set
	CFrameSnapshot*	CreateEmptySnapshot(
		int ticknumber, int maxEntities, uint32 nSnapshotSet = knDefaultSnapshotSet );
	CFrameSnapshot*	TakeTickSnapshot(
		int ticknumber, uint32 nSnapshotSet = knDefaultSnapshotSet );

	// Creates pack data for a particular entity for a particular snapshot
	PackedEntity*	CreatePackedEntity( CFrameSnapshot* pSnapshot, int entity );
	//this is similar to the above, but the packed entity is created locally so that it doesn't interfere with the global last sent packet information
	PackedEntity*	CreateLocalPackedEntity( CFrameSnapshot* pSnapshot, int entity );

	// Returns the pack data for a particular entity for a particular snapshot
	inline PackedEntity*	GetPackedEntity( CFrameSnapshot& Snapshot, int entity );
	PackedEntity*			GetPackedEntity( PackedEntityHandle_t handle  )					{ return PackedEntityFromHandle( handle ); }

	// if we are copying a Packed Entity, we have to increase the reference counter 
	void			AddEntityReference( PackedEntityHandle_t handle );

	// if we are removeing a Packed Entity, we have to decrease the reference counter
	void			RemoveEntityReference( PackedEntityHandle_t handle );

	// Uses a previously sent packet
	bool			UsePreviouslySentPacket( CFrameSnapshot* pSnapshot, int entity, int entSerialNumber );

	bool			ShouldForceRepack( CFrameSnapshot* pSnapshot, int entity, PackedEntityHandle_t handle );

	PackedEntity*	GetPreviouslySentPacket( int iEntity, int iSerialNumber );

	// Return the entity sitting in iEntity's slot if iSerialNumber matches its number.
	UnpackedDataCache_t *GetCachedUncompressedEntity( PackedEntity *pPackedEntity );

	// List of entities to explicitly delete
	void			AddExplicitDelete( int iSlot );

	void			BuildSnapshotList( CFrameSnapshot *pCurrentSnapshot, CFrameSnapshot *pLastSnapshot, uint32 nSnapshotSet, CReferencedSnapshotList &list );

public:
	void					DeleteFrameSnapshot( CFrameSnapshot* pSnapshot );
	// Non-threadsafe call, used with BuildSnapshotList which acquires the mutex
	CFrameSnapshot			*NextSnapshot( CFrameSnapshot *pSnapshot );

	// Mutex for m_FrameSnapshots array
	std::recursive_mutex									m_FrameSnapshotsWriteMutex;
	std::list< CFrameSnapshot * >						m_FrameSnapshots;

	// A PackedEntityHandle_t is the address of its entity in this pool; 0 is invalid.
	ks::object_pool< PackedEntity >					m_PackedEntitiesPool;	// addresses are the packed-entity handles
	static PackedEntity *PackedEntityFromHandle( PackedEntityHandle_t handle ) { return reinterpret_cast< PackedEntity * >( handle ); }

	int								m_nPackedEntityCacheCounter;  // increase with every cache access
	std::vector< UnpackedDataCache_t >	m_PackedEntityCache;	// cache for uncompressed packed entities

	// The most recently sent packets for each entity
	PackedEntityHandle_t	m_pLastPackedData[ MAX_EDICTS ];
	int						m_pSerialNumber[ MAX_EDICTS ];

	std::recursive_mutex		m_WriteMutex;

	std::vector< int >			m_iExplicitDeleteSlots;
};

PackedEntity* CFrameSnapshotManager::GetPackedEntity( CFrameSnapshot& Snapshot, int entity )
{
	contract_assert( entity < Snapshot.m_nNumEntities );
	PackedEntityHandle_t index = Snapshot.m_pEntities[entity].m_pPackedData;
	if ( index == INVALID_PACKED_ENTITY_HANDLE )
		return nullptr;

	contract_assert( PackedEntityFromHandle( index )->m_nEntityIndex == entity );
	return PackedEntityFromHandle( index );
}

extern CFrameSnapshotManager *framesnapshotmanager;
