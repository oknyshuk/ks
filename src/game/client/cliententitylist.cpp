// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Purpose: a global list of all the entities in the game.  All iteration through
//			entities is done through this object.
//-----------------------------------------------------------------------------
#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "const.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// Create interface
static CClientEntityList s_EntityList;
CBaseEntityList *g_pEntityList = &s_EntityList;

// Expose list to engine
static InterfaceReg s_Reg_CClientEntityList_IClientEntityList( []() -> void * { return static_cast<IClientEntityList *>( &s_EntityList ); }, VCLIENTENTITYLIST_INTERFACE_VERSION );;

// Store local pointer to interface for rest of client .dll only 
//  (CClientEntityList instead of IClientEntityList )
CClientEntityList *cl_entitylist = &s_EntityList; 


								 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientEntityList::CClientEntityList( void )
{
	m_iMaxUsedServerIndex = -1;
	m_iMaxServerEnts = 0;
	Release();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientEntityList::~CClientEntityList( void )
{
	Release();
}

//-----------------------------------------------------------------------------
// Purpose: Clears all entity lists and releases entities
//-----------------------------------------------------------------------------
void CClientEntityList::Release( void )
{
	// Free all the entities.
	ClientEntityHandle_t iter = FirstHandle();
	while( iter != InvalidHandle() )
	{
		// Try to call release on anything we can.
		IClientNetworkable *pNet = GetClientNetworkableFromHandle( iter );
		if ( pNet )
		{
			pNet->Release();
		}
		else
		{
			// Try to call release on anything we can.
			IClientThinkable *pThinkable = GetClientThinkableFromHandle( iter );
			if ( pThinkable )
			{
				pThinkable->Release();
			}
		}
		RemoveEntity( iter );

		iter = FirstHandle();
	}

	m_iNumServerEnts = 0;
	m_iMaxServerEnts = 0;
	m_iNumClientNonNetworkable = 0;
	m_iMaxUsedServerIndex = -1;
}

IClientNetworkable* CClientEntityList::GetClientNetworkable( int entnum )
{
	contract_assert( entnum >= 0 );
	contract_assert( entnum < MAX_EDICTS );
	return m_EntityCacheInfo[entnum].m_pNetworkable;
}


EntityCacheInfo_t *CClientEntityList::GetClientNetworkableArray()
{
	return m_EntityCacheInfo;
}

void CClientEntityList::SetDormant( int entityIndex, bool bDormant )
{
	contract_assert( entityIndex >= 0 );
	contract_assert( entityIndex < MAX_EDICTS );
	m_EntityCacheInfo[entityIndex].m_bDormant = bDormant;
}


IClientEntity* CClientEntityList::GetClientEntity( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? pEnt->GetIClientEntity() : 0;
}


int CClientEntityList::NumberOfEntities( bool bIncludeNonNetworkable )
{
	if ( bIncludeNonNetworkable == true )
		 return m_iNumServerEnts + m_iNumClientNonNetworkable;

	return m_iNumServerEnts;
}


void CClientEntityList::SetMaxEntities( int maxents )
{
	m_iMaxServerEnts = maxents;
}


int CClientEntityList::GetMaxEntities( void )
{
	return m_iMaxServerEnts;
}


//-----------------------------------------------------------------------------
// Convenience methods to convert between entindex + ClientEntityHandle_t
//-----------------------------------------------------------------------------
int CClientEntityList::HandleToEntIndex( ClientEntityHandle_t handle )
{
	if ( handle == INVALID_EHANDLE )
		return -1;
	C_BaseEntity *pEnt = GetBaseEntityFromHandle( handle );
	return pEnt ? pEnt->entindex() : -1; 
}


//-----------------------------------------------------------------------------
// Purpose: Because m_iNumServerEnts != last index
// Output : int
//-----------------------------------------------------------------------------
int CClientEntityList::GetHighestEntityIndex( void )
{
	return m_iMaxUsedServerIndex;
}

void CClientEntityList::RecomputeHighestEntityUsed( void )
{
	m_iMaxUsedServerIndex = -1;

	// Walk backward looking for first valid index
	int i;
	for ( i = MAX_EDICTS - 1; i >= 0; i-- )
	{
		if ( GetListedEntity( i ) != nullptr )
		{
			m_iMaxUsedServerIndex = i;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add a raw C_BaseEntity to the entity list.
// Input  : index - 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------

C_BaseEntity* CClientEntityList::GetBaseEntity( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? pEnt->GetBaseEntity() : 0;
}


ICollideable* CClientEntityList::GetCollideable( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? pEnt->GetCollideable() : 0;
}


IClientNetworkable* CClientEntityList::GetClientNetworkableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientNetworkable() : 0;
}


IClientEntity* CClientEntityList::GetClientEntityFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetIClientEntity() : 0;
}


IClientRenderable* CClientEntityList::GetClientRenderableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientRenderable() : 0;
}


C_BaseEntity* CClientEntityList::GetBaseEntityFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetBaseEntity() : 0;
}


ICollideable* CClientEntityList::GetCollideableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetCollideable() : 0;
}


IClientThinkable* CClientEntityList::GetClientThinkableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientThinkable() : 0;
}


void CClientEntityList::AddPVSNotifier( IClientUnknown *pUnknown )
{
	IClientRenderable *pRen = pUnknown->GetClientRenderable();
	if ( pRen )
	{
		IPVSNotify *pNotify = pRen->GetPVSNotifyInterface();
		if ( pNotify )
		{
			auto index = m_PVSNotifyInfos.insert( m_PVSNotifyInfos.end(), CPVSNotifyInfo{} );
			CPVSNotifyInfo *pInfo = &*index;
			pInfo->m_pNotify = pNotify;
			pInfo->m_pRenderable = pRen;
			pInfo->m_InPVSStatus = 0;

			[[maybe_unused]] bool bInserted = m_PVSNotifierMap.emplace( pUnknown, index ).second;
			contract_assert( bInserted );
		}
	}
}


void CClientEntityList::RemovePVSNotifier( IClientUnknown *pUnknown )
{
	IClientRenderable *pRenderable = pUnknown->GetClientRenderable();
	if ( pRenderable )
	{
		IPVSNotify *pNotify = pRenderable->GetPVSNotifyInterface();
		if ( pNotify )
		{
			auto index = m_PVSNotifierMap.find( pUnknown );
			if ( index == m_PVSNotifierMap.end() )
			{
				Warning( "PVS notifier not in m_PVSNotifierMap\n" );
				contract_assert( false );
				return;
			}

			auto indexIntoPVSNotifyInfos = index->second;
			
			contract_assert( indexIntoPVSNotifyInfos->m_pNotify == pNotify );
			contract_assert( indexIntoPVSNotifyInfos->m_pRenderable == pRenderable );
			
			m_PVSNotifyInfos.erase( indexIntoPVSNotifyInfos );
			m_PVSNotifierMap.erase( index );
			return;
		}
	}

	// If it didn't report itself as a notifier, let's hope it's not in the notifier list now
	// (which would mean that it reported itself as a notifier earlier, but not now).
#ifdef _DEBUG
	contract_assert( !m_PVSNotifierMap.contains( pUnknown ) );
#endif
}

void CClientEntityList::AddListenerEntity( IClientEntityListener *pListener )
{
	if ( ks::find_index( m_entityListeners, pListener ) >= 0 )
	{
		contract_assert( false && "Can't add listeners multiple times\n" );
		return;
	}
	m_entityListeners.push_back( pListener );
}

void CClientEntityList::RemoveListenerEntity( IClientEntityListener *pListener )
{
	ks::erase_first( m_entityListeners, pListener );
}

void CClientEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int entnum = handle.GetEntryIndex();
	EntityCacheInfo_t *pCache = &m_EntityCacheInfo[entnum];

	if ( entnum >= 0 && entnum < MAX_EDICTS )
	{
		// Update our counters.
		m_iNumServerEnts++;
		if ( entnum > m_iMaxUsedServerIndex )
		{
			m_iMaxUsedServerIndex = entnum;
		}


		// Cache its networkable pointer.
		contract_assert( dynamic_cast< IClientUnknown* >( pEnt ) );
		contract_assert( ((IClientUnknown*)pEnt)->GetClientNetworkable() ); // Server entities should all be networkable.
		pCache->m_pNetworkable = ((IClientUnknown*)pEnt)->GetClientNetworkable();
		pCache->m_bDormant = true;
	}

	IClientUnknown *pUnknown = (IClientUnknown*)pEnt;

	// If this thing wants PVS notifications, hook it up.
	AddPVSNotifier( pUnknown );

	// Store it in a special list for fast iteration if it's a C_BaseEntity.
	C_BaseEntity *pBaseEntity = pUnknown->GetBaseEntity();
	if ( pBaseEntity )
	{
		pCache->m_BaseEntitiesIndex = m_BaseEntities.emplace( pBaseEntity );

		if ( pBaseEntity->ObjectCaps() & FCAP_SAVE_NON_NETWORKABLE  )
		{
			 m_iNumClientNonNetworkable++;
		}

		//DevMsg(2,"Created %s\n", pBaseEnt->GetClassname() );
		for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
		{
			m_entityListeners[i]->OnEntityCreated( pBaseEntity );
		}
	}
	else
	{
		pCache->m_BaseEntitiesIndex = m_BaseEntities.invalid;
	}


}


void CClientEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int entnum = handle.GetEntryIndex();
	EntityCacheInfo_t *pCache = &m_EntityCacheInfo[entnum];

	if ( entnum >= 0 && entnum < MAX_EDICTS )
	{
		// This is a networkable ent. Clear out our cache info for it.
		pCache->m_pNetworkable = nullptr;
		m_iNumServerEnts--;

		if ( entnum >= m_iMaxUsedServerIndex )
		{
			RecomputeHighestEntityUsed();
		}
	}


	IClientUnknown *pUnknown = (IClientUnknown*)pEnt;

	// If this is a PVS notifier, remove it.
	RemovePVSNotifier( pUnknown );

	C_BaseEntity *pBaseEntity = pUnknown->GetBaseEntity();

	if ( pBaseEntity )
	{
		if ( pBaseEntity->ObjectCaps() & FCAP_SAVE_NON_NETWORKABLE )
		{
			 m_iNumClientNonNetworkable--;
		}

		//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
		for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
		{
			m_entityListeners[i]->OnEntityDeleted( pBaseEntity );
		}
	}

	if ( pCache->m_BaseEntitiesIndex != m_BaseEntities.invalid )
		m_BaseEntities.erase( pCache->m_BaseEntitiesIndex );

	pCache->m_BaseEntitiesIndex = m_BaseEntities.invalid;
}


// Use this to iterate over all the C_BaseEntities.
C_BaseEntity* CClientEntityList::FirstBaseEntity() const
{
	const CEntInfo *pList = FirstEntInfo();
	while ( pList )
	{
		if ( pList->m_pEntity )
		{
			IClientUnknown *pUnk = static_cast<IClientUnknown*>( pList->m_pEntity );
			C_BaseEntity *pRet = pUnk->GetBaseEntity();
			if ( pRet )
				return pRet;
		}
		pList = pList->m_pNext;
	}

	return nullptr;

}

C_BaseEntity* CClientEntityList::NextBaseEntity( C_BaseEntity *pEnt ) const
{
	if ( pEnt == nullptr )
		return FirstBaseEntity();

	// Run through the list until we get a C_BaseEntity.
	const CEntInfo *pList = GetEntInfoPtr( pEnt->GetRefEHandle() );
	if ( pList )
	{
		pList = NextEntInfo(pList);
	}

	while ( pList )
	{
		if ( pList->m_pEntity )
		{
			IClientUnknown *pUnk = static_cast<IClientUnknown*>( pList->m_pEntity );
			C_BaseEntity *pRet = pUnk->GetBaseEntity();
			if ( pRet )
				return pRet;
		}
		pList = pList->m_pNext;
	}
	
	return nullptr; 
}



// -------------------------------------------------------------------------------------------------- //
// C_AllBaseEntityIterator
// -------------------------------------------------------------------------------------------------- //
C_AllBaseEntityIterator::C_AllBaseEntityIterator()
{
	Restart();
}


void C_AllBaseEntityIterator::Restart()
{
	m_CurBaseEntity = 0;
}

	
C_BaseEntity* C_AllBaseEntityIterator::Next()
{
	// Walks slots, so entities removed mid-iteration are simply skipped
	const auto &list = ClientEntityList().m_BaseEntities;
	while ( m_CurBaseEntity < list.slot_count() )
	{
		unsigned short h = m_CurBaseEntity++;
		if ( list.contains( h ) )
			return list[h];
	}
	return nullptr;
}


// -------------------------------------------------------------------------------------------------- //
// C_BaseEntityIterator
// -------------------------------------------------------------------------------------------------- //
C_BaseEntityIterator::C_BaseEntityIterator()
{
	Restart();
}

void C_BaseEntityIterator::Restart()
{
	m_CurBaseEntity = 0;
}

C_BaseEntity* C_BaseEntityIterator::Next()
{
	// Skip dormant entities
	const auto &list = ClientEntityList().m_BaseEntities;
	while ( m_CurBaseEntity < list.slot_count() )
	{
		unsigned short h = m_CurBaseEntity++;
		if ( !list.contains( h ) )
			continue;

		C_BaseEntity *pRet = list[h];
		if (!pRet->IsDormant())
			return pRet;
	}

	return nullptr;
}
