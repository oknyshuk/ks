// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
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
#include "entitylist.h"
#include "igamesystem.h"
#include "collisionutils.h"
#include "mapentities.h"
#include "client.h"
#include "ai_initutils.h"
#include "globalstate.h"
#include "datacache/imdlcache.h"

void SceneManager_ClientActive( CBasePlayer *player );

static std::vector< IServerNetworkable* > g_DeleteList;

CGlobalEntityList gEntList;
CBaseEntityList *g_pEntityList = &gEntList;

class CAimTargetManager : public IEntityListener
{
public:
	// Called by CEntityListSystem
	void LevelInitPreEntity() 
	{ 
		gEntList.AddListenerEntity( this );
		Clear(); 
	}
	void LevelShutdownPostEntity()
	{
		gEntList.RemoveListenerEntity( this );
		Clear();
	}

	void Clear() 
	{ 
		m_targetList.clear(); m_targetList.shrink_to_fit(); 
	}

	void ForceRepopulateList()
	{
		Clear();

		CBaseEntity *pEnt = gEntList.FirstEnt();

		while( pEnt )
		{
			if( ShouldAddEntity(pEnt) )
				AddEntity(pEnt);

			pEnt = gEntList.NextEnt( pEnt );
		}
	}
	
	bool ShouldAddEntity( CBaseEntity *pEntity )
	{
		return ((pEntity->GetFlags() & FL_AIMTARGET) != 0);
	}

	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntitySpawned( CBaseEntity *pEntity )
	{
		if ( ShouldAddEntity( pEntity ) )
		{
			RemoveEntity( pEntity );
			AddEntity( pEntity );
		}
	}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
		if ( !(pEntity->GetFlags() & FL_AIMTARGET) )
			return;
		RemoveEntity(pEntity);
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		if ( pEntity->IsMarkedForDeletion() )
			return;
		m_targetList.push_back( pEntity );
	}
	void RemoveEntity( CBaseEntity *pEntity )
	{
		int index = ks::find_index( m_targetList, pEntity );
		if ( ks::is_valid_index( m_targetList, index ) )
		{
			ks::swap_remove( m_targetList, index );
		}
	}

	int ListCount() { return std::ssize( m_targetList ); }
	int ListCopy( CBaseEntity *pList[], int listMax )
	{
		int count = std::min(listMax, ListCount() );
		std::memcpy( pList, m_targetList.data(), sizeof(CBaseEntity *) * count );
		return count;
	}
	CBaseEntity *ListElement( int iIndex )
	{
		return m_targetList[iIndex];
	}

private:
	std::vector< CBaseEntity * >	m_targetList;
};

static CAimTargetManager g_AimManager;

int AimTarget_ListCount()
{
	return g_AimManager.ListCount();
}
int AimTarget_ListCopy( CBaseEntity *pList[], int listMax )
{
	return g_AimManager.ListCopy( pList, listMax );
}
CBaseEntity *AimTarget_ListElement( int iIndex )
{
	return g_AimManager.ListElement( iIndex );
}
void AimTarget_ForceRepopulateList()
{
	g_AimManager.ForceRepopulateList();
}


// Manages a list of all entities currently doing game simulation or thinking
// NOTE: This is usually a small subset of the global entity list, so it's
// an optimization to maintain this list incrementally rather than polling each
// frame.
struct simthinkentry_t
{
	unsigned short	entEntry;
	unsigned short	unused0;
	int				nextThinkTick;
};
class CSimThinkManager : public IEntityListener
{
public:
	CSimThinkManager()
	{
		Clear();
	}
	void Clear()
	{
		m_simThinkList.clear(); m_simThinkList.shrink_to_fit();
		for ( int i = 0; i < std::size( m_entinfoIndex ); i++ )
		{
			m_entinfoIndex[i] = 0xFFFF;
		}
	}
	void LevelInitPreEntity()
	{
		gEntList.AddListenerEntity( this );
	}

	void LevelShutdownPostEntity()
	{
		gEntList.RemoveListenerEntity( this );
		Clear();
	}

	void OnEntityCreated( CBaseEntity *pEntity )
	{
	}
	void OnEntityDeleted( CBaseEntity *pEntity )
	{
		RemoveEntinfoIndex( pEntity->GetRefEHandle().GetEntryIndex() );
	}

	void RemoveEntinfoIndex( int index )
	{
		int listHandle = m_entinfoIndex[index];
		// If this guy is in the active list, remove him
		if ( listHandle != 0xFFFF )
		{
			contract_assert( m_simThinkList[listHandle].entEntry == index );
			ks::swap_remove( m_simThinkList, listHandle );
			m_entinfoIndex[index] = 0xFFFF;
			
			// fast remove shifted someone, update that someone
			if ( listHandle < std::ssize( m_simThinkList ) )
			{
				m_entinfoIndex[m_simThinkList[listHandle].entEntry] = listHandle;
			}
		}
	}
	int ListCount()
	{
		return std::ssize( m_simThinkList );
	}

	int ListCopy( CBaseEntity *pList[], int listMax )
	{
		int count = std::min(listMax, ListCount());
		int out = 0;
		for ( int i = 0; i < count; i++ )
		{
			// only copy out entities that will simulate or think this frame
			if ( m_simThinkList[i].nextThinkTick <= gpGlobals->tickcount )
			{
				contract_assert( m_simThinkList[i].nextThinkTick>=0 );
				int entinfoIndex = m_simThinkList[i].entEntry;
				const CEntInfo *pInfo = gEntList.GetEntInfoPtrByIndex( entinfoIndex );
				if ( !pInfo->m_pEntity )
					continue;
				pList[out] = (CBaseEntity *)pInfo->m_pEntity;
				contract_assert( m_simThinkList[i].nextThinkTick==0 || pList[out]->GetFirstThinkTick()==m_simThinkList[i].nextThinkTick );
				out++;
			}
		}

		return out;
	}

	void EntityChanged( CBaseEntity *pEntity )
	{
		// might change after deletion, don't put back into the list
		if ( pEntity->IsMarkedForDeletion() )
			return;
		
		const CBaseHandle &eh = pEntity->GetRefEHandle();
		if ( !eh.IsValid() )
			return;

		int index = eh.GetEntryIndex();
		if ( pEntity->IsEFlagSet( EFL_NO_THINK_FUNCTION ) && pEntity->IsEFlagSet( EFL_NO_GAME_PHYSICS_SIMULATION ) )
		{
			RemoveEntinfoIndex( index );
		}
		else
		{
			// already in the list? (had think or sim last time, now has both - or had both last time, now just one)
			if ( m_entinfoIndex[index] == 0xFFFF )
			{
				m_entinfoIndex[index] = std::ssize( m_simThinkList ); m_simThinkList.emplace_back();
				m_simThinkList[m_entinfoIndex[index]].entEntry = (unsigned short)index;
				m_simThinkList[m_entinfoIndex[index]].nextThinkTick = 0;
				if ( pEntity->IsEFlagSet(EFL_NO_GAME_PHYSICS_SIMULATION) )
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = pEntity->GetFirstThinkTick();
					contract_assert( m_simThinkList[m_entinfoIndex[index]].nextThinkTick>=0 );
				}
			}
			else
			{
				contract_assert( m_simThinkList[m_entinfoIndex[index]].entEntry == index );
				// updating existing entry - if no sim, reset think time
				if ( pEntity->IsEFlagSet(EFL_NO_GAME_PHYSICS_SIMULATION) )
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = pEntity->GetFirstThinkTick();
					contract_assert( m_simThinkList[m_entinfoIndex[index]].nextThinkTick>=0 );
				}
				else
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = 0;
				}
			}
		}
	}

private:
	unsigned short m_entinfoIndex[NUM_ENT_ENTRIES];
	std::vector< simthinkentry_t >	m_simThinkList;
};

CSimThinkManager g_SimThinkManager;

int SimThink_ListCount()
{
	return g_SimThinkManager.ListCount();
}

int SimThink_ListCopy( CBaseEntity *pList[], int listMax )
{
	return g_SimThinkManager.ListCopy( pList, listMax );
}

void SimThink_EntityChanged( CBaseEntity *pEntity )
{
	g_SimThinkManager.EntityChanged( pEntity );
}

// This manages a list of entities queued up to receive PostClientMessages callbacks
class CPostClientMessageManager
{
public:
	void LevelShutdownPostEntity()
	{
		m_list.clear(); m_list.shrink_to_fit();
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		m_list.push_back( (unsigned short)pEntity->GetRefEHandle().GetEntryIndex() );
	}
	void PostClientMessagesSent()
	{
		for ( int i = std::ssize( m_list )-1; i >= 0; --i )
		{
			CBaseEntity *pEntity = (CBaseEntity *)gEntList.GetEntInfoPtrByIndex( m_list[i] )->m_pEntity;

			if ( pEntity )
			{
				pEntity->PostClientMessagesSent();
			}
		}
		m_list.clear();
	}
private:
	// UNDONE: Need to use ehandles instead?
	std::vector< unsigned short >	m_list;
};

static CPostClientMessageManager g_PostClientManager;

static CBaseEntityClassList *s_pClassLists = nullptr;
CBaseEntityClassList::CBaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
CBaseEntityClassList::~CBaseEntityClassList()
{
}

//-----------------------------------------------------------------------------
// Entity hash tables
//-----------------------------------------------------------------------------

struct CEntsByStringHashFuncs
{
	std::size_t operator()( string_t iszStr ) const
	{
		static_assert( sizeof( char * ) == sizeof( intp ) );
		return HashIntp( ( intp )iszStr.ToCStr() );
	}
};

// Pooled classname -> head of that class's entity list; entities keep a pointer to their head slot
static std::unordered_map< string_t, CBaseEntity *, CEntsByStringHashFuncs > g_EntsByClassname( 512 );

//-----------------------------------------------------------------------------
CGlobalEntityList::CGlobalEntityList()
{
	m_iHighestEnt = m_iNumEnts = m_iNumEdicts = 0;
	m_bClearingEntities = false;
}


// removes the entity from the global list
// only called from with the CBaseEntity destructor
static bool g_fInCleanupDelete;

void CGlobalEntityList::UpdateName( IHandleEntity *pHandleEnt, CBaseHandle hEnt )
{
	CEntInfo *pEntInfo = const_cast<CEntInfo *>( GetEntInfoPtr( hEnt ) );
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
	pEntInfo->m_iClassName = MAKE_STRING( pEnt->GetClassname() );
	pEntInfo->m_iName = pEnt->GetEntityName();
}

void CGlobalEntityList::UpdateName( IHandleEntity *pEnt )
{
	UpdateName( pEnt, pEnt->GetRefEHandle() );
}


// mark an entity as deleted
void CGlobalEntityList::AddToDeleteList( IServerNetworkable *ent )
{
	if ( ent && ent->GetEntityHandle()->GetRefEHandle() != CBaseHandle( INVALID_EHANDLE ) )
	{
		g_DeleteList.push_back( ent );
	}
}

extern bool g_bDisableEhandleAccess;
// call this before and after each frame to delete all of the marked entities.
void CGlobalEntityList::CleanupDeleteList( void )
{
	CVProfScope _( "CGlobalEntityList::CleanupDeleteList", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	g_fInCleanupDelete = true;
	// clean up the vphysics delete list as well
	PhysOnCleanupDeleteList();

	g_bDisableEhandleAccess = true;
	for ( int i = 0; i < std::ssize( g_DeleteList ); i++ )
	{
		g_DeleteList[i]->Release();
	}
	g_bDisableEhandleAccess = false;
	g_DeleteList.clear();

	g_fInCleanupDelete = false;
}

int CGlobalEntityList::ResetDeleteList( void )
{
	int result = std::ssize( g_DeleteList );
	g_DeleteList.clear();
	return result;
}


	// add a class that gets notified of entity events
void CGlobalEntityList::AddListenerEntity( IEntityListener *pListener )
{
	if ( ks::find_index( m_entityListeners, pListener ) >= 0 )
	{
		contract_assert( false && "Can't add listeners multiple times\n" );
		return;
	}
	m_entityListeners.push_back( pListener );
}

void CGlobalEntityList::RemoveListenerEntity( IEntityListener *pListener )
{
	ks::erase_first( m_entityListeners, pListener );
}

void CGlobalEntityList::Clear( void )
{
	m_bClearingEntities = true;

	// Add all remaining entities in the game to the delete list and call appropriate UpdateOnRemove
	CBaseHandle hCur = FirstHandle();
	while ( hCur != InvalidHandle() )
	{
		IServerNetworkable *ent = GetServerNetworkable( hCur );
		if ( ent )
		{
			MDLCACHE_CRITICAL_SECTION();
			// Force UpdateOnRemove to be called
			UTIL_Remove( ent );
		}
		hCur = NextHandle( hCur );
	}
		
	CleanupDeleteList();
	// free the memory
	g_DeleteList.clear(); g_DeleteList.shrink_to_fit();

#ifdef _DEBUG
	for ( const auto &[iszClassname, pHead] : g_EntsByClassname )
	{
		contract_assert( pHead == nullptr );
	}
#endif

	g_EntsByClassname.clear();

	CBaseEntity::m_nDebugPlayer = -1;
	CBaseEntity::m_bInDebugSelect = false; 
	m_iHighestEnt = 0;
	m_iNumEnts = 0;

	m_bClearingEntities = false;
}


int CGlobalEntityList::NumberOfEntities( void )
{
	return m_iNumEnts;
}

int CGlobalEntityList::NumberOfEdicts( void )
{
	return m_iNumEdicts;
}

CBaseEntity *CGlobalEntityList::NextEnt( CBaseEntity *pCurrentEnt ) 
{ 
	if ( !pCurrentEnt )
	{
		const CEntInfo *pInfo = FirstEntInfo();
		if ( !pInfo )
			return nullptr;

		return (CBaseEntity *)pInfo->m_pEntity;
	}

	// Run through the list until we get a CBaseEntity.
	const CEntInfo *pList = GetEntInfoPtr( pCurrentEnt->GetRefEHandle() );
	if ( pList )
		pList = NextEntInfo(pList);

	while ( pList )
	{
		return (CBaseEntity *)pList->m_pEntity;
		pList = pList->m_pNext;
	}
	
	return nullptr; 

}


void CGlobalEntityList::ReportEntityFlagsChanged( CBaseEntity *pEntity, unsigned int flagsOld, unsigned int flagsNow )
{
	if ( pEntity->IsMarkedForDeletion() )
		return;
	// UNDONE: Move this into IEntityListener instead?
	unsigned int flagsChanged = flagsOld ^ flagsNow;
	if ( flagsChanged & FL_AIMTARGET )
	{
		unsigned int flagsAdded = flagsNow & flagsChanged;
		unsigned int flagsRemoved = flagsOld & flagsChanged;

		if ( flagsAdded & FL_AIMTARGET )
		{
			g_AimManager.AddEntity( pEntity );
		}
		if ( flagsRemoved & FL_AIMTARGET )
		{
			g_AimManager.RemoveEntity( pEntity );
		}
	}
}

void CGlobalEntityList::AddPostClientMessageEntity( CBaseEntity *pEntity )
{
	g_PostClientManager.AddEntity( pEntity );
}
void CGlobalEntityList::PostClientMessagesSent()
{
	g_PostClientManager.PostClientMessagesSent();
}

//-----------------------------------------------------------------------------
// Purpose: Used to confirm a pointer is a pointer to an entity, useful for
//			asserts.
//-----------------------------------------------------------------------------
bool CGlobalEntityList::IsEntityPtr( void *pTest )
{
	if ( pTest )
	{
		const CEntInfo *pInfo = FirstEntInfo();
		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			if ( pTest == (void *)pInfo->m_pEntity )
				return true;
		}
	}

	return false; 
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given classname.
// Input  : pStartEntity - Last entity found, NULL to start a new iteration.
//			szName - Classname to search for.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *pEntity = (CBaseEntity *)pInfo->m_pEntity;
		if ( !pEntity )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( EntityNamesMatch( szName, pInfo->m_iClassName ) )
			return pEntity;
	}

	return nullptr;
}

CBaseEntity *CGlobalEntityList::FindEntityByClassnameFast( CBaseEntity *pStartEntity, string_t iszClassname )
{
	if ( pStartEntity )
	{
		return pStartEntity->m_pNextByClass;
	}

	auto it = g_EntsByClassname.find( iszClassname );
	if ( it != g_EntsByClassname.end() )
	{
		return it->second;
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Finds an entity given a procedural name.
// Input  : szName - The procedural name to search for, should start with '!'.
//			pSearchingEntity - 
//			pActivator - The activator entity if this was called from an input
//				or Use handler.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityProcedural( const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	//
	// Check for the name escape character.
	//
	if ( szName[0] == '!' )
	{
		const char *pName = szName + 1;

		//
		// It is a procedural name, look for the ones we understand.
		//
		if ( FStrEq( pName, "player" ) )
		{
			return (CBaseEntity *)UTIL_PlayerByIndex( 1 );
		}
		else if ( FStrEq( pName, "pvsplayer" ) )
		{
			if ( pSearchingEntity )
			{
				return CBaseEntity::Instance( UTIL_FindClientInPVS( pSearchingEntity->edict() ) );
			}
			else if ( pActivator )
			{
				// FIXME: error condition?
				return CBaseEntity::Instance( UTIL_FindClientInPVS( pActivator->edict() ) );
			}
			else
			{
				// FIXME: error condition?
				return (CBaseEntity *)UTIL_PlayerByIndex( 1 );
			}

		}
		else if ( FStrEq( pName, "activator" ) )
		{
			return pActivator;
		}
		else if ( FStrEq( pName, "caller" ) )
		{
			return pCaller;
		}
		else if ( FStrEq( pName, "picker" ) )
		{
			return UTIL_PlayerByIndex(1) ? UTIL_PlayerByIndex(1)->FindPickerEntity() : nullptr;
		}
		else if ( FStrEq( pName, "self" ) )
		{
			return pSearchingEntity;
		}
		else 
		{
			Warning( "Invalid entity search name %s\n", szName );
			contract_assert( 0 );
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given name.
// Input  : pStartEntity - Last entity found, NULL to start a new iteration.
//			szName - Name to search for.
//			pActivator - Activator entity if this was called from an input
//				handler or Use handler.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByName( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller, IEntityFindFilter *pFilter )
{
	if ( !szName || szName[0] == 0 )
		return nullptr;

	if ( szName[0] == '!' )
	{
		//
		// Avoid an infinite loop, only find one match per procedural search!
		//
		if (pStartEntity == nullptr)
			return FindEntityProcedural( szName, pSearchingEntity, pActivator, pCaller );

		return nullptr;
	}
	
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( pInfo->m_iName == NULL_STRING )
			continue;

		if ( EntityNamesMatch( szName, pInfo->m_iName ) )
		{
			if ( pFilter && !pFilter->ShouldFindEntity(ent) )
				continue;

			return ent;
		}
	}

	return nullptr;
}

CBaseEntity *CGlobalEntityList::FindEntityByNameFast( CBaseEntity *pStartEntity, string_t iszName )
{
	if ( iszName == NULL_STRING || STRING(iszName)[0] == 0 )
		return nullptr;

	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( pInfo->m_iName == NULL_STRING )
			continue;

		if ( pInfo->m_iName == iszName )
		{
			return ent;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pStartEntity - 
//			szModelName - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByModel( CBaseEntity *pStartEntity, const char *szModelName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->edict() || !ent->GetModelName() )
			continue;

		if ( FStrEq( STRING(ent->GetModelName()), szModelName ) )
			return ent;
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given target.
// Input  : pStartEntity - 
//			szName - 
//-----------------------------------------------------------------------------
// FIXME: obsolete, remove
CBaseEntity	*CGlobalEntityList::FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->m_target )
			continue;

		if ( FStrEq( STRING(ent->m_target), szName ) )
			return ent;
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given target.
// Input  : pStartEntity - 
//			szName - 
//-----------------------------------------------------------------------------
// FIXME: obsolete, remove
CBaseEntity	*CGlobalEntityList::FindEntityByOutputTarget( CBaseEntity *pStartEntity, string_t iTarget )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		datamap_t *dmap = ent->GetDataDescMap();
		while ( dmap )
		{
			int fields = dmap->dataNumFields;
			for ( int i = 0; i < fields; i++ )
			{
				typedescription_t *dataDesc = &dmap->dataDesc[i];
				if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((intp)ent + (intp)dataDesc->fieldOffset);
					if ( pOutput->GetActionForTarget( iTarget ) )
						return ent;
				}
			}

			dmap = dmap->baseMap;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Used to iterate all the entities within a sphere.
// Input  : pStartEntity - 
//			vecCenter - 
//			flRadius - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->edict() )
			continue;

		Vector vecRelativeCenter;
		ent->CollisionProp()->WorldToCollisionSpace( vecCenter, &vecRelativeCenter );
		if ( !IsBoxIntersectingSphere( ent->CollisionProp()->OBBMins(),	ent->CollisionProp()->OBBMaxs(), vecRelativeCenter, flRadius ) )
			continue;

		return ent;
	}

	// nothing found
	return nullptr; 
}


//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by name within a radius
// Input  : szName - Entity name to search for.
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByNameNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByName( pSearch, szName, pSearchingEntity, pActivator, pCaller )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}



//-----------------------------------------------------------------------------
// Purpose: Finds the first entity by name within a radius
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for.
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByNameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		return gEntList.FindEntityByName( pEntity, szName, pSearchingEntity, pActivator, pCaller );
	}

	while ((pEntity = gEntList.FindEntityByName( pEntity, szName, pSearchingEntity, pActivator, pCaller )) != nullptr)
	{
		if ( !pEntity->edict() )
			continue;

		float flDist2 = (pEntity->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			return pEntity;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by class name withing given search radius.
// Input  : szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearest( const char *szName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassname( pSearch, szName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}


CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearestFast( string_t iszName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassnameFast( pSearch, iszName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by class name withing given search radius.
// Input  : szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearest2D( const char *szName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassname( pSearch, szName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin().AsVector2D() - vecSrc.AsVector2D()).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}



//-----------------------------------------------------------------------------
// Purpose: Finds the first entity within radius distance by class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		return gEntList.FindEntityByClassname( pEntity, szName );
	}

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szName )) != nullptr)
	{
		if ( !pEntity->edict() && !pEntity->IsEFlagSet( EFL_SERVER_ONLY ) )
			continue;

		// Instead of checking absorigin vs sphere, check if the obb intersects the sphere.
		Vector vecRelativeCenter;
		pEntity->CollisionProp()->WorldToCollisionSpace( vecSrc, &vecRelativeCenter );
		if ( IsBoxIntersectingSphere( pEntity->CollisionProp()->OBBMins(),	pEntity->CollisionProp()->OBBMaxs(), vecRelativeCenter, flRadius ) )
		{
			return pEntity;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the first entity within an extent by class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity class name, ie "info_target".
//			vecMins - Search mins.
//			vecMaxs - Search maxs.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecMins, const Vector &vecMaxs )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szName )) != nullptr)
	{
		if ( !pEntity->edict() && !pEntity->IsEFlagSet( EFL_SERVER_ONLY ) )
			continue;

		// check if the aabb intersects the search aabb.
		Vector entMins, entMaxs;
		pEntity->CollisionProp()->WorldSpaceAABB( &entMins, &entMaxs );
		if ( IsBoxIntersectingBox( vecMins, vecMaxs, entMins, entMaxs ) )
		{
			return pEntity;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Finds an entity by target name or class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGeneric( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByName( pStartEntity, szName, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassname( pStartEntity, szName );
	}

	return pEntity;
} 


//-----------------------------------------------------------------------------
// Purpose: Finds the first entity by target name or class name within a radius
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGenericWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByNameWithin( pStartEntity, szName, vecSrc, flRadius, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassnameWithin( pStartEntity, szName, vecSrc, flRadius );
	}

	return pEntity;
} 

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by target name or class name within a radius.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGenericNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByNameNearest( szName, vecSrc, flRadius, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassnameNearest( szName, vecSrc, flRadius );
	}

	return pEntity;
} 


//-----------------------------------------------------------------------------
// Purpose: Find the nearest entity along the facing direction from the given origin
//			within the angular threshold (ignores worldspawn) with the
//			given classname.
// Input  : origin - 
//			facing - 
//			threshold - 
//			classname - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityClassNearestFacing( const Vector &origin, const Vector &facing, float threshold, char *classname)
{
	float bestDot = threshold;
	CBaseEntity *best_ent = nullptr;

	const CEntInfo *pInfo = FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		// FIXME: why is this skipping pointsize entities?
		if (ent->IsPointSized() )
			continue;

		// Make vector to entity
		Vector	to_ent = (ent->GetAbsOrigin() - origin);

		VectorNormalize( to_ent );
		float dot = DotProduct (facing , to_ent );
		if (dot > bestDot) 
		{
			if (FClassnameIs(ent,classname))
			{
				// Ignore if worldspawn
				if (!FClassnameIs( ent, "worldspawn" )  && !FClassnameIs( ent, "soundent")) 
				{
					bestDot	= dot;
					best_ent = ent;
				}
			}
		}
	}
	return best_ent;
}


//-----------------------------------------------------------------------------
// Purpose: Find the nearest entity along the facing direction from the given origin
//			within the angular threshold (ignores worldspawn)
// Input  : origin - 
//			facing - 
//			threshold - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityNearestFacing( const Vector &origin, const Vector &facing, float threshold)
{
	float bestDot = threshold;
	CBaseEntity *best_ent = nullptr;

	const CEntInfo *pInfo = FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		// Ignore logical entities
		if (!ent->edict())
			continue;

		// Make vector to entity
		Vector	to_ent = ent->WorldSpaceCenter() - origin;
		VectorNormalize(to_ent);

		float dot = DotProduct( facing, to_ent );
		if (dot <= bestDot) 
			continue;

		// Ignore if worldspawn
		if (!FStrEq( STRING(ent->m_iClassname), "worldspawn")  && !FStrEq( STRING(ent->m_iClassname), "soundent")) 
		{
			bestDot	= dot;
			best_ent = ent;
		}
	}
	return best_ent;
}


void CGlobalEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int i = handle.GetEntryIndex();

	// record current list details
	m_iNumEnts++;
	if ( i > m_iHighestEnt )
		m_iHighestEnt = i;

	// If it's a CBaseEntity, notify the listeners.
	CBaseEntity *pBaseEnt = EntityCast<CBaseEntity>( static_cast<IServerUnknown*>(pEnt)->GetBaseEntity() );
	if ( pBaseEnt->edict() )
		m_iNumEdicts++;
	
	// NOTE: Must be a CBaseEntity on server
	contract_assert( pBaseEnt );
	//DevMsg(2,"Created %s\n", pBaseEnt->GetClassname() );
	for ( i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityCreated( pBaseEnt );
	}
}


void CGlobalEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
#ifdef DEBUG
	if ( !g_fInCleanupDelete )
	{
		int i;
		for ( i = 0; i < std::ssize( g_DeleteList ); i++ )
		{
			if ( g_DeleteList[i]->GetEntityHandle() == pEnt )
			{
				ks::swap_remove( g_DeleteList, i );
				Msg( "ERROR: Entity being destroyed but previously threaded on g_DeleteList\n" );
				break;
			}
		}
	}
#endif

	CBaseEntity *pBaseEnt = EntityCast<CBaseEntity>( static_cast<IServerUnknown*>(pEnt)->GetBaseEntity() );
	if ( pBaseEnt->edict() )
		m_iNumEdicts--;

	m_iNumEnts--;
}

void CGlobalEntityList::NotifyCreateEntity( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return;

	// Make sure no one is trying to build an entity without game strings.
	contract_assert( MAKE_STRING( pEnt->GetClassname() ) == FindPooledString( pEnt->GetClassname() ) && 
		( pEnt->GetEntityName() == NULL_STRING || pEnt->GetEntityName() == FindPooledString( pEnt->GetEntityName().ToCStr() ) ) );

	contract_assert( pEnt->m_pPrevByClass == nullptr && pEnt->m_pNextByClass == nullptr && pEnt->m_ppListByClassHead == nullptr );

	CBaseEntity *&pHead = g_EntsByClassname.try_emplace( MAKE_STRING( pEnt->GetClassname() ), nullptr ).first->second;
	pEnt->m_ppListByClassHead = &pHead;
	if ( pHead )
	{
		pHead->m_pPrevByClass = pEnt;
		pEnt->m_pNextByClass = pHead;
		contract_assert( pEnt->m_pPrevByClass == nullptr );
	}
	pHead = pEnt;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityCreated( pEnt );
	}
}

void CGlobalEntityList::NotifySpawn( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntitySpawned( pEnt );
	}
}

// NOTE: This doesn't happen in OnRemoveEntity() specifically because 
// listeners may want to reference the object as it's being deleted
// OnRemoveEntity isn't called until the destructor and all data is invalid.
void CGlobalEntityList::NotifyRemoveEntity( CBaseEntity *pBaseEnt )
{
	if ( !pBaseEnt )
		return;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityDeleted( pBaseEnt );
	}

	if ( pBaseEnt->m_ppListByClassHead )
	{
		CBaseEntity *&pHead = *pBaseEnt->m_ppListByClassHead;
		if ( pHead == pBaseEnt )
		{
			pHead = pBaseEnt->m_pNextByClass;
			// Don't remove empty list, on the assumption that the number of classes that are not referenced again is small
			// Plus during map load we get a lot of precache others that hit this [8/8/2008 tom]
		}

		contract_assert( pHead != pBaseEnt );
		if ( pBaseEnt->m_pNextByClass )
		{
			pBaseEnt->m_pNextByClass->m_pPrevByClass = pBaseEnt->m_pPrevByClass;
		}

		if ( pBaseEnt->m_pPrevByClass )
		{
			pBaseEnt->m_pPrevByClass->m_pNextByClass = pBaseEnt->m_pNextByClass;
		}

		pBaseEnt->m_pPrevByClass = pBaseEnt->m_pNextByClass = nullptr;
		pBaseEnt->m_ppListByClassHead = nullptr;
	}
}


//-----------------------------------------------------------------------------
// NOTIFY LIST
// 
// Allows entities to get events fired when another entity changes
//-----------------------------------------------------------------------------
struct entitynotify_t
{
	CBaseEntity	*pNotify;
	CBaseEntity	*pWatched;
};
class CNotifyList : public INotify, public IEntityListener
{
public:
	// INotify
	void AddEntity( CBaseEntity *pNotify, CBaseEntity *pWatched );
	void RemoveEntity( CBaseEntity *pNotify, CBaseEntity *pWatched );
	void ReportNamedEvent( CBaseEntity *pEntity, const char *pEventName );
	void ClearEntity( CBaseEntity *pNotify );
	void ReportSystemEvent( CBaseEntity *pEntity, notify_system_event_t eventType, const notify_system_event_params_t &params );

	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity );
	virtual void OnEntityDeleted( CBaseEntity *pEntity );

	// Called from CEntityListSystem
	void LevelInitPreEntity();
	void LevelShutdownPreEntity();

private:
	std::vector< entitynotify_t >	m_notifyList;
};

void CNotifyList::AddEntity( CBaseEntity *pNotify, CBaseEntity *pWatched )
{
	// OPTIMIZE: Also flag pNotify for faster "RemoveAllNotify" ?
	pWatched->AddEFlags( EFL_NOTIFY );
	int index = std::ssize( m_notifyList ); m_notifyList.emplace_back();
	entitynotify_t &notify = m_notifyList[index];
	notify.pNotify = pNotify;
	notify.pWatched = pWatched;
}

// Remove noitfication for an entity
void CNotifyList::RemoveEntity( CBaseEntity *pNotify, CBaseEntity *pWatched )
{
	for ( int i = std::ssize( m_notifyList ); --i >= 0; )
	{
		if ( m_notifyList[i].pNotify == pNotify && m_notifyList[i].pWatched == pWatched)
		{
			ks::swap_remove( m_notifyList, i );
		}
	}
}


void CNotifyList::ReportNamedEvent( CBaseEntity *pEntity, const char *pInputName )
{
	variant_t emptyVariant;

	if ( !pEntity->IsEFlagSet(EFL_NOTIFY) )
		return;

	for ( int i = 0; i < std::ssize( m_notifyList ); i++ )
	{
		if ( m_notifyList[i].pWatched == pEntity )
		{
			m_notifyList[i].pNotify->AcceptInput( pInputName, pEntity, pEntity, emptyVariant, 0 );
		}
	}
}

void CNotifyList::LevelInitPreEntity()
{
	gEntList.AddListenerEntity( this );
}

void CNotifyList::LevelShutdownPreEntity( void )
{
	gEntList.RemoveListenerEntity( this );
	m_notifyList.clear(); m_notifyList.shrink_to_fit();
}

void CNotifyList::OnEntityCreated( CBaseEntity *pEntity )
{
}

void CNotifyList::OnEntityDeleted( CBaseEntity *pEntity )
{
	ReportDestroyEvent( pEntity );
	ClearEntity( pEntity );
}


// UNDONE: Slow linear search?
void CNotifyList::ClearEntity( CBaseEntity *pNotify )
{
	for ( int i = std::ssize( m_notifyList ); --i >= 0; )
	{
		if ( m_notifyList[i].pNotify == pNotify || m_notifyList[i].pWatched == pNotify)
		{
			ks::swap_remove( m_notifyList, i );
		}
	}
}

void CNotifyList::ReportSystemEvent( CBaseEntity *pEntity, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	if ( !pEntity->IsEFlagSet(EFL_NOTIFY) )
		return;

	for ( int i = 0; i < std::ssize( m_notifyList ); i++ )
	{
		if ( m_notifyList[i].pWatched == pEntity )
		{
			m_notifyList[i].pNotify->NotifySystemEvent( pEntity, eventType, params );
		}
	}
}

static CNotifyList g_NotifyList;
INotify *g_pNotify = &g_NotifyList;

class CEntityTouchManager : public IEntityListener
{
public:
	// called by CEntityListSystem
	void LevelInitPreEntity() 
	{ 
		gEntList.AddListenerEntity( this );
		Clear(); 
	}
	void LevelShutdownPostEntity() 
	{ 
		gEntList.RemoveListenerEntity( this );
		Clear(); 
	}
	void FrameUpdatePostEntityThink();

	void Clear()
	{
		m_updateList.clear(); m_updateList.shrink_to_fit();
	}
	
	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
		if ( !pEntity->GetCheckUntouch() )
			return;
		int index = ks::find_index( m_updateList, pEntity );
		if ( ks::is_valid_index( m_updateList, index ) )
		{
			ks::swap_remove( m_updateList, index );
		}
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		if ( pEntity->IsMarkedForDeletion() )
			return;

		contract_assert( !ks::is_valid_index( m_updateList, ks::find_index( m_updateList, pEntity ) ) );
		m_updateList.push_back( pEntity );
	}

	void RemoveEntity( CBaseEntity *pEntity )
	{
		ks::swap_erase_first( m_updateList, pEntity );
	}

private:
	std::vector< CBaseEntity * >	m_updateList;
};

static CEntityTouchManager g_TouchManager;

void EntityTouch_Add( CBaseEntity *pEntity )
{
	g_TouchManager.AddEntity( pEntity );
}

void EntityTouch_Remove( CBaseEntity *pEntity )
{
	g_TouchManager.RemoveEntity( pEntity );
}


void CEntityTouchManager::FrameUpdatePostEntityThink()
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "CEntityTouchManager::FrameUpdatePostEntityThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// Loop through all entities again, checking their untouch if flagged to do so
	
	int count = std::ssize( m_updateList );
	if ( count )
	{
		// copy off the list
		CBaseEntity **ents = scratch.Alloc<CBaseEntity *>( count );
		std::memcpy( ents, m_updateList.data(), sizeof(CBaseEntity *) * count );
		// clear it
		m_updateList.clear();
		
		// now update those ents
		for ( int i = 0; i < count; i++ )
		{
			//Assert( ents[i]->GetCheckUntouch() );
			if ( ents[i]->GetCheckUntouch() )
			{
				ents[i]->PhysicsCheckForEntityUntouch();
			}
		}
	}
}

class CRespawnEntitiesFilter : public IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname )
	{
		// Create everything but the world
		return V_stricmp( pClassname, "worldspawn" ) != 0;
	}

	virtual CBaseEntity* CreateNextEntity( const char *pClassname )
	{
		return CreateEntityByName( pClassname );
	}
};

// One hook to rule them all...
// Since most of the little list managers in here only need one or two of the game
// system callbacks, this hook is a game system that passes them the appropriate callbacks
class CEntityListSystem : public CAutoGameSystemPerFrame
{
public:
	CEntityListSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
		m_bRespawnAllEntities = false;
	}
	void LevelInitPreEntity()
	{
		g_NotifyList.LevelInitPreEntity();
		g_TouchManager.LevelInitPreEntity();
		g_AimManager.LevelInitPreEntity();
		g_SimThinkManager.LevelInitPreEntity();
	}
	void LevelShutdownPreEntity()
	{
		g_NotifyList.LevelShutdownPreEntity();
	}
	void LevelShutdownPostEntity()
	{
		g_TouchManager.LevelShutdownPostEntity();
		g_AimManager.LevelShutdownPostEntity();
		g_PostClientManager.LevelShutdownPostEntity();
		g_SimThinkManager.LevelShutdownPostEntity();
		CBaseEntityClassList *pClassList = s_pClassLists;
		while ( pClassList )
		{
			pClassList->LevelShutdownPostEntity();
			pClassList = pClassList->m_pNextClassList;
		}
	}

	void FrameUpdatePostEntityThink()
	{
		g_TouchManager.FrameUpdatePostEntityThink();

		if ( m_bRespawnAllEntities )
		{
			m_bRespawnAllEntities = false;

			// Don't change globalstate owing to deletion here
			GlobalEntity_EnableStateUpdates( false );

			// Remove all entities
			int nPlayerIndex = -1;
			CBaseEntity *pEnt = gEntList.FirstEnt();
			while ( pEnt )
			{
				CBaseEntity *pNextEnt = gEntList.NextEnt( pEnt );
				if ( pEnt->IsPlayer() )
				{
					nPlayerIndex = pEnt->entindex();
				}
				if ( !pEnt->IsEFlagSet( EFL_KEEP_ON_RECREATE_ENTITIES ) )
				{
					UTIL_Remove( pEnt );
				}
				pEnt = pNextEnt;
			}
			
			gEntList.CleanupDeleteList();

			GlobalEntity_EnableStateUpdates( true );

			// Allows us to immediately re-use the edict indices we just freed to avoid edict overflow
			engine->AllowImmediateEdictReuse();

			// Reset node counter used during load
			CNodeEnt::m_nNodeCount = 0;

			CRespawnEntitiesFilter filter;
			MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );

			// Allocate a CBasePlayer for pev, and call spawn
			if ( nPlayerIndex >= 0 )
			{
				edict_t *pEdict = INDEXENT( nPlayerIndex );
				ClientPutInServer( pEdict, "unnamed" );
				ClientActive( pEdict, false );

				CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
				SceneManager_ClientActive( pPlayer );
			}
		}
	}

	bool m_bRespawnAllEntities;
};

static CEntityListSystem g_EntityListSystem( "CEntityListSystem" );

//-----------------------------------------------------------------------------
// Respawns all entities in the level
//-----------------------------------------------------------------------------
void RespawnEntities()
{
	g_EntityListSystem.m_bRespawnAllEntities = true;
}

static ConCommand restart_entities( "respawn_entities", RespawnEntities, "Respawn all the entities in the map.", FCVAR_CHEAT | FCVAR_SPONLY );

class CSortedEntityList
{
public:
	CSortedEntityList() : m_sortedList(), m_emptyCount(0) {}

	struct CEntityReportLess
	{
		bool operator()( CBaseEntity *src1, CBaseEntity *src2 ) const
		{
			return stricmp( src1->GetClassname(), src2->GetClassname() ) < 0;
		}
	};

	void AddEntityToList( CBaseEntity *pEntity )
	{
		if ( !pEntity )
		{
			m_emptyCount++;
		}
		else
		{
			m_sortedList.insert( pEntity );
		}
	}
	void ReportEntityList()
	{
		const char *pLastClass = "";
		int count = 0;
		int edicts = 0;
		for ( CBaseEntity *pEntity : m_sortedList )
		{
			if ( !pEntity )
				continue;

			if ( pEntity->edict() )
				edicts++;

			const char *pClassname = pEntity->GetClassname();
			if ( !FStrEq( pClassname, pLastClass ) )
			{
				if ( count )
				{
					Msg("Class: %s (%d)\n", pLastClass, count );
				}

				pLastClass = pClassname;
				count = 1;
			}
			else
				count++;
		}
		if ( pLastClass[0] != 0 && count )
		{
			Msg("Class: %s (%d)\n", pLastClass, count );
		}
		if ( !m_sortedList.empty() )
		{
			Msg("Total %d entities (%d empty, %d edicts)\n", (int)std::ssize( m_sortedList ), m_emptyCount, edicts );
		}
	}
private:
	std::flat_multiset< CBaseEntity *, CEntityReportLess > m_sortedList;
	int		m_emptyCount;
};



static void report_entities( const CCommand &args );
static ConCommand report_entities_command( "report_entities", report_entities, "Lists all entities" );
static void report_entities( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSortedEntityList list;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		list.AddEntityToList( pEntity );
		pEntity = gEntList.NextEnt( pEntity );
	}
	list.ReportEntityList();
}


static void report_touchlinks( const CCommand &args );
static ConCommand report_touchlinks_command( "report_touchlinks", report_touchlinks, "Lists all touchlinks" );
static void report_touchlinks( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSortedEntityList list;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	const char *pClassname = nullptr;
	if ( args.ArgC() > 1 )
	{
		pClassname = args.Arg(1);
	}
	while ( pEntity )
	{
		if ( !pClassname || FClassnameIs(pEntity, pClassname) )
		{
			touchlink_t *root = ( touchlink_t * )pEntity->GetDataObject( TOUCHLINK );
			if ( root )
			{
				touchlink_t *link = root->nextLink;
				while ( link != root )
				{
					list.AddEntityToList( link->entityTouched );
					link = link->nextLink;
				}
			}
		}
		pEntity = gEntList.NextEnt( pEntity );
	}
	list.ReportEntityList();
}

static void report_simthinklist( const CCommand &args );
static ConCommand report_simthinklist_command( "report_simthinklist", report_simthinklist, "Lists all simulating/thinking entities" );
static void report_simthinklist( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBaseEntity *pTmp[NUM_ENT_ENTRIES];
	int count = SimThink_ListCopy( pTmp, std::size( pTmp ) );

	CSortedEntityList list;
	for ( int i = 0; i < count; i++ )
	{
		if ( !pTmp[i] )
			continue;

		list.AddEntityToList( pTmp[i] );
	}
	list.ReportEntityList();
}
