// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <string.h>
#include "eiface.h"
#include "networkvar.h"
#include "vphysics_interface.h"
#include "shareddefs.h"
#include "bone_accessor.h"
#include "bone_setup.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "vphysics/constraints.h"
#include "ai_criteria.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "studio.h"
#include "model_types.h"
#include "tier2/resourceprecacher.h"
#include "tier2/tier2_logging.h"
#include "vcollide_parse.h"
#include "vphysics/friction.h"
#include "vphysics/object_hash.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.game.vphysicsupdateai;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.fixed_pool;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.vstdlib.callqueue;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.cs_playeranimstate;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.core.platform;
import ks.core.threadtools;
import ks.math.aabb;
import ks.math.transform;
import ks.math.vmatrix;
import ks.tier1.fmtstr;
import ks.tier1.keyvalues;
import ks.game.ai_activity;
import :entity_dispatch;
import :entity_kind;
import ks.core.scratcharena;

class CBaseEntity;
class CBasePlayer;
struct physicspushlist_t;
struct touchlink_t;

using namespace ::ks::reflect;

// memory pool for storing links between entities
static ks::fixed_pool g_EdictTouchLinks( sizeof(touchlink_t), MAX_EDICTS, ks::fixed_pool::grow::none, "g_EdictTouchLinks", alignof(touchlink_t) );
static ks::fixed_pool g_EntityGroundLinks( sizeof( groundlink_t ), MAX_EDICTS, ks::fixed_pool::grow::none, "g_EntityGroundLinks", alignof(groundlink_t) );

struct watcher_t
{
	EHANDLE				hWatcher;
	IWatcherCallback	*pWatcherCallback;
};

class CWatcherList
{
public:
	void NotifyPositionChanged( CBaseEntity *pEntity );
	void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake );

	void AddToList( CBaseEntity *pWatcher );
	void RemoveWatcher( CBaseEntity *pWatcher );

private:
	int GetCallbackObjects( IWatcherCallback **pList, int listMax );

	std::vector<watcher_t>::iterator Find( CBaseEntity *pEntity );
	std::vector<watcher_t> m_list;
};

int linksallocated = 0;
int groundlinksallocated = 0;

// Prints warnings if any entity think functions take longer than this many milliseconds
#define DEF_THINK_LIMIT "10"

ConVar think_limit( side_registry(), "think_limit", DEF_THINK_LIMIT, FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum think time in milliseconds, warning is printed if this is exceeded." );

ConVar debug_touchlinks( side_registry(), "debug_touchlinks", "0", 0, "Spew touch link activity" );
#define DebugTouchlinks() debug_touchlinks.GetBool()

ConVar sv_grenade_trajectory(side_registry(), "sv_grenade_trajectory", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_CHEAT, "Shows grenade trajectory visualization in-game." );
ConVar sv_grenade_trajectory_time(side_registry(), "sv_grenade_trajectory_time", "20", FCVAR_REPLICATED | FCVAR_RELEASE, "Length of time grenade trajectory remains visible.", true, 0.1f, true, 20.0f );
ConVar sv_grenade_trajectory_time_spectator(side_registry(), "sv_grenade_trajectory_time_spectator", "4", FCVAR_REPLICATED | FCVAR_RELEASE, "Length of time grenade trajectory remains visible as a spectator.", true, 0.0f, true, 8.0f );
ConVar sv_grenade_trajectory_thickness(side_registry(), "sv_grenade_trajectory_thickness", "0.2", FCVAR_REPLICATED | FCVAR_RELEASE, "Visible thickness of grenade trajectory arc", true, 0.1f, true, 1.0f );
ConVar sv_grenade_trajectory_dash(side_registry(), "sv_grenade_trajectory_dash", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Dot-dash style grenade trajectory arc" );

//-----------------------------------------------------------------------------
// Portal-specific hack designed to eliminate re-entrancy in touch functions
//-----------------------------------------------------------------------------
class CPortalTouchScope
{
public:
	CPortalTouchScope();
	~CPortalTouchScope();

public:
	static int m_nDepth;
	static CCallQueue m_CallQueue;	
};

int CPortalTouchScope::m_nDepth = 0;
CCallQueue CPortalTouchScope::m_CallQueue;	

CCallQueue *GetPortalCallQueue()
{
	return ( CPortalTouchScope::m_nDepth > 0 ) ? &CPortalTouchScope::m_CallQueue : nullptr;
}

CPortalTouchScope::CPortalTouchScope()
{
	++m_nDepth;
}

CPortalTouchScope::~CPortalTouchScope()
{
	contract_assert( m_nDepth >= 1 );
	if ( --m_nDepth == 0 )
	{
		m_CallQueue.CallQueued();
	}
}

//-----------------------------------------------------------------------------
// Purpose: System for hanging objects off of CBaseEntity, etc.
//  Externalized data objects ( see sharreddefs.h for enum )
//-----------------------------------------------------------------------------
class CDataObjectAccessSystem : public CAutoGameSystem
{
public:

	enum
	{
		MAX_ACCESSORS = 32,
	};

	CDataObjectAccessSystem()
	{
    	static_assert( (int)NUM_DATAOBJECT_TYPES <= (int)MAX_ACCESSORS );

		V_memset( m_Accessors, 0, sizeof( m_Accessors ) );
	}

	virtual bool Init()
	{
		AddDataAccessor( TOUCHLINK, new CEntityDataInstantiator< touchlink_t > );
		AddDataAccessor( GROUNDLINK, new CEntityDataInstantiator< groundlink_t > );
		AddDataAccessor( STEPSIMULATION, new CEntityDataInstantiator< StepSimulationData > );
		AddDataAccessor( MODELSCALE, new CEntityDataInstantiator< ModelScale > );
		AddDataAccessor( POSITIONWATCHER, new CEntityDataInstantiator< CWatcherList > );
		AddDataAccessor( PHYSICSPUSHLIST, new CEntityDataInstantiator< physicspushlist_t > );
		AddDataAccessor( VPHYSICSUPDATEAI, new CEntityDataInstantiator< vphysicsupdateai_t > );
		AddDataAccessor( VPHYSICSWATCHER, new CEntityDataInstantiator< CWatcherList > );
		
		return true;
	}

	virtual void Shutdown()
	{
		for ( int i = 0; i < MAX_ACCESSORS; i++ )
		{
			delete m_Accessors[ i ];
			m_Accessors[ i ]  = 0;
		}
	}

	void *GetDataObject( int type, const CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return nullptr;
		}
		return m_Accessors[ type ]->GetDataObject( instance );
	}

	void *CreateDataObject( int type, CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return nullptr;
		}

		return m_Accessors[ type ]->CreateDataObject( instance );
	}

	void DestroyDataObject( int type, CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return;
		}

		m_Accessors[ type ]->DestroyDataObject( instance );
	}

private:

	bool IsValidType( int type ) const
	{
		if ( type < 0 || type >= MAX_ACCESSORS )
			return false;

		if ( m_Accessors[ type ] == nullptr )
			return false;
		return true;
	}

	void AddDataAccessor( int type, IEntityDataInstantiator *instantiator )
	{
		if ( type < 0 || type >= MAX_ACCESSORS )
		{
			contract_assert( !"AddDataAccessor with out of range type!!!\n" );
			return;
		}

		contract_assert( instantiator );

		if ( m_Accessors[ type ] != nullptr )
		{
			contract_assert( !"AddDataAccessor, duplicate adds!!!\n" );
			return;
		}

		m_Accessors[ type ] = instantiator;
	}

	IEntityDataInstantiator *m_Accessors[ MAX_ACCESSORS ];
};

static CDataObjectAccessSystem g_DataObjectAccessSystem;

bool CBaseEntity::HasDataObjectType( int type ) const
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	return ( m_fDataObjectTypes	& (1<<type) ) ? true : false;
}

void CBaseEntity::AddDataObjectType( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	m_fDataObjectTypes |= (1<<type);
}

void CBaseEntity::RemoveDataObjectType( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	m_fDataObjectTypes &= ~(1<<type);
}

void *CBaseEntity::GetDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	if ( !HasDataObjectType( type ) )
		return nullptr;
	return g_DataObjectAccessSystem.GetDataObject( type, this );
}

void *CBaseEntity::CreateDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	AddDataObjectType( type );
	return g_DataObjectAccessSystem.CreateDataObject( type, this );
}

void CBaseEntity::DestroyDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	if ( !HasDataObjectType( type ) )
		return;
	g_DataObjectAccessSystem.DestroyDataObject( type, this );
	RemoveDataObjectType( type );
}

int CWatcherList::GetCallbackObjects( IWatcherCallback **pList, int listMax )
{
	int index = 0;
	for ( auto it = m_list.begin(); it != m_list.end(); )
	{
		if ( it->hWatcher.Get() )
		{
			pList[index] = it->pWatcherCallback;
			index++;
			if ( index >= listMax )
			{
				contract_assert( 0 );
				return index;
			}
			++it;
		}
		else
		{
			it = m_list.erase( it );
		}
	}
	return index;
}

void CWatcherList::NotifyPositionChanged( CBaseEntity *pEntity )
{
	IWatcherCallback *pCallbacks[1024]; // HACKHACK: Assumes this list is big enough
	int count = GetCallbackObjects( pCallbacks, std::size( pCallbacks ) );
	for ( int i = 0; i < count; i++ )
	{
		IPositionWatcher *pWatcher = static_cast<IPositionWatcher *>(pCallbacks[i]);
		if ( pWatcher )
		{
			pWatcher->NotifyPositionChanged(pEntity);
		}
	}
}

void CWatcherList::NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
{
	IWatcherCallback *pCallbacks[1024];	// HACKHACK: Assumes this list is big enough!
	int count = GetCallbackObjects( pCallbacks, std::size( pCallbacks ) );
	for ( int i = 0; i < count; i++ )
	{
		IVPhysicsWatcher *pWatcher = static_cast<IVPhysicsWatcher *>(pCallbacks[i]);
		if ( pWatcher )
		{
			pWatcher->NotifyVPhysicsStateChanged(pPhysics, pEntity, bAwake);
		}
	}
}

std::vector<watcher_t>::iterator CWatcherList::Find( CBaseEntity *pEntity )
{
	return std::ranges::find_if( m_list, [pEntity]( const watcher_t &w ) { return w.hWatcher.Get() == pEntity; } );
}

void CWatcherList::RemoveWatcher( CBaseEntity *pEntity )
{
	auto it = Find( pEntity );
	if ( it != m_list.end() )
	{
		m_list.erase( it );
	}
}

void CWatcherList::AddToList( CBaseEntity *pWatcher )
{
	if ( Find( pWatcher ) == m_list.end() )
	{
		watcher_t watcher;
		watcher.hWatcher = pWatcher;
			// save this separately so we can use the EHANDLE to test for deletion
		watcher.pWatcherCallback = dynamic_cast<IWatcherCallback *> (pWatcher);

		if ( watcher.pWatcherCallback )
		{
			m_list.push_back( watcher );
		}
	}
}

static void AddWatcherToEntity( CBaseEntity *pWatcher, CBaseEntity *pEntity, int watcherType )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(watcherType);
	if ( !pList )
	{
		pList = ( CWatcherList * )pEntity->CreateDataObject( watcherType );
	}

	pList->AddToList( pWatcher );
}

static void RemoveWatcherFromEntity( CBaseEntity *pWatcher, CBaseEntity *pEntity, int watcherType )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(watcherType);
	if ( pList )
	{
		pList->RemoveWatcher( pWatcher );
	}
}

void WatchPositionChanges( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
{
	AddWatcherToEntity( pWatcher, pMovingEntity, POSITIONWATCHER );
}

void RemovePositionWatcher( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
{
	RemoveWatcherFromEntity( pWatcher, pMovingEntity, POSITIONWATCHER );
}

void ReportPositionChanged( CBaseEntity *pMovedEntity )
{
	CWatcherList *pList = (CWatcherList *)pMovedEntity->GetDataObject(POSITIONWATCHER);
	if ( pList )
	{
		pList->NotifyPositionChanged( pMovedEntity );
	}
}

void WatchVPhysicsStateChanges( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
{
	AddWatcherToEntity( pWatcher, pPhysicsEntity, VPHYSICSWATCHER );
}

void RemoveVPhysicsStateWatcher( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
{
	AddWatcherToEntity( pWatcher, pPhysicsEntity, VPHYSICSWATCHER );
}

void ReportVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(VPHYSICSWATCHER);
	if ( pList )
	{
		pList->NotifyVPhysicsStateChanged( pPhysics, pEntity, bAwake );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::DestroyAllDataObjects( void )
{
	int i;
	for ( i = 0; i < NUM_DATAOBJECT_TYPES; i++ )
	{
		if ( HasDataObjectType( i ) )
		{
			DestroyDataObject( i );
		}
	}
}

//-----------------------------------------------------------------------------
// For debugging
//-----------------------------------------------------------------------------

void SpewLinks()
{
	int nCount = 0;
	for ( CBaseEntity *pClass = gEntList.FirstEnt(); pClass != nullptr; pClass = gEntList.NextEnt(pClass) )
	{
		if ( pClass /*&& !pClass->IsDormant()*/ )
		{
			touchlink_t *root = ( touchlink_t * )pClass->GetDataObject( TOUCHLINK );
			if ( root )
			{

				// check if the edict is already in the list
				for ( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
				{
					++nCount;
					Msg("[%d] (%d) Link %d (%s) -> %d (%s)\n", nCount, pClass->IsDormant(),
						pClass->entindex(), pClass->GetClassname(),
						link->entityTouched->entindex(), link->entityTouched->GetClassname() );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Returns the actual gravity
//-----------------------------------------------------------------------------
static inline float GetActualGravity( CBaseEntity *pEnt )
{
	float ent_gravity = pEnt->GetGravity();
	if ( ent_gravity == 0.0f )
	{
		ent_gravity = 1.0f;
	}

	return ent_gravity * sv_gravity.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline touchlink_t
//-----------------------------------------------------------------------------
inline touchlink_t *AllocTouchLink( void )
{
	touchlink_t *link = (touchlink_t*)g_EdictTouchLinks.alloc( sizeof(touchlink_t) );
	if ( link )
	{
		++linksallocated;
	}
	else
	{
		DevWarning( "AllocTouchLink: failed to allocate touchlink_t.\n" );
	}

	return link;
}

static touchlink_t *g_pNextLink = nullptr;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *link - 
// Output : inline void
//-----------------------------------------------------------------------------
inline void FreeTouchLink( touchlink_t *link )
{
	if ( link )
	{
		if ( link == g_pNextLink )
		{
			g_pNextLink = link->nextLink;
		}
		--linksallocated;
		link->prevLink = link->nextLink = nullptr;
	}

	// Necessary to catch crashes
	g_EdictTouchLinks.free( link );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline groundlink_t
//-----------------------------------------------------------------------------
inline groundlink_t *AllocGroundLink( void )
{
	groundlink_t *link = (groundlink_t*)g_EntityGroundLinks.alloc( sizeof(groundlink_t) );
	if ( link )
	{
		++groundlinksallocated;
	}
	else
	{
		DevMsg( "AllocGroundLink: failed to allocate groundlink_t.!!!\n" );
	}

	return link;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *link - 
// Output : inline void
//-----------------------------------------------------------------------------
inline void FreeGroundLink( groundlink_t *link )
{
	if ( link )
	{
		--groundlinksallocated;
	}

	g_EntityGroundLinks.free( link );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsCurrentlyTouching( void ) const
{
	if ( HasDataObjectType( TOUCHLINK ) )
	{
		return true;
	}

	return false;
}

static bool g_bCleanupDatObject = true;

//-----------------------------------------------------------------------------
// Purpose: Checks to see if any entities that have been touching this one
//			have stopped touching it, and notify the entity if so.
//			Called at the end of a frame, after all the entities have run
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckForEntityUntouch( void )
{
	contract_assert( g_pNextLink == nullptr );

	touchlink_t *link;

	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		bool saveCleanup = g_bCleanupDatObject;
		g_bCleanupDatObject = false;

		link = root->nextLink;
		while ( link != root )
		{
			g_pNextLink = link->nextLink;

			// these touchlinks are not polled.  The ents are touching due to an outside
			// system that will add/delete them as necessary (vphysics in this case)
			if ( link->touchStamp == TOUCHSTAMP_EVENT_DRIVEN )
			{
				// refresh the touch call
				PhysicsTouch( link->entityTouched );
			}
			else
			{    
				// check to see if the touch stamp is up to date
				if ( link->touchStamp != touchStamp )
				{
					// stamp is out of data, so entities are no longer touching
					// remove self from other entities touch list
					PhysicsNotifyOtherOfUntouch( this, link->entityTouched );

					// remove other entity from this list
					PhysicsRemoveToucher( this, link );
				}
			}

			link = g_pNextLink;
		}

		g_bCleanupDatObject = saveCleanup;

		// Nothing left in list, destroy root
		if ( root->nextLink == root &&
			 root->prevLink == root )
		{
			DestroyDataObject( TOUCHLINK );
		}
	}

	g_pNextLink = nullptr;

	SetCheckUntouch( false );
}

//-----------------------------------------------------------------------------
// Purpose: notifies an entity than another touching entity has moved out of contact.
// Input  : *other - the entity to be acted upon
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNotifyOtherOfUntouch( CBaseEntity *ent, CBaseEntity *other )
{
	if ( !other )
		return;

	// loop through ed's touch list, looking for the notifier
	// remove and call untouch if found
	touchlink_t *root = ( touchlink_t * )other->GetDataObject( TOUCHLINK );
	if ( root )
	{
		touchlink_t *link = root->nextLink;
		while ( link != root )
		{
			if ( link->entityTouched == ent )
			{
				PhysicsRemoveToucher( other, link );

				// Check for complete removal
				if ( g_bCleanupDatObject &&
					 root->nextLink == root && 
					 root->prevLink == root )
				{
					other->DestroyDataObject( TOUCHLINK );
				}
				return;
			}

			link = link->nextLink;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: removes a toucher from the list
// Input  : *link - the link to remove
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveToucher( CBaseEntity *otherEntity, touchlink_t *link )
{
	// Every start Touch gets a corresponding end touch
	if ( (link->flags & FTOUCHLINK_START_TOUCH) && 
		link->entityTouched != nullptr &&
		otherEntity != nullptr )
	{
		otherEntity->EndTouch( link->entityTouched );
	}

	link->nextLink->prevLink = link->prevLink;
	link->prevLink->nextLink = link->nextLink;

	if ( DebugTouchlinks() )
		Msg( "remove 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, link->entityTouched->GetDebugName(), otherEntity->GetDebugName(), link->entityTouched->entindex(), otherEntity->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
	FreeTouchLink( link );
}

//-----------------------------------------------------------------------------
// Purpose: Clears all touches from the list
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveTouchedList( CBaseEntity *ent )
{

	touchlink_t *link, *nextLink;

	touchlink_t *root = ( touchlink_t * )ent->GetDataObject( TOUCHLINK );
	if ( root )
	{
		link = root->nextLink;
		bool saveCleanup = g_bCleanupDatObject;
		g_bCleanupDatObject = false;
		while ( link && link != root )
		{
			nextLink = link->nextLink;

			// notify the other entity that this ent has gone away
			PhysicsNotifyOtherOfUntouch( ent, link->entityTouched );

			// kill it
			if ( DebugTouchlinks() )
				Msg( "remove 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, ent->GetDebugName(), link->entityTouched->GetDebugName(), ent->entindex(), link->entityTouched->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
			FreeTouchLink( link );
			link = nextLink;
		}

		g_bCleanupDatObject = saveCleanup;
		ent->DestroyDataObject( TOUCHLINK );
	}

	ent->touchStamp = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *other - 
// Output : groundlink_t
//-----------------------------------------------------------------------------
groundlink_t *CBaseEntity::AddEntityToGroundList( CBaseEntity *other )
{
	groundlink_t *link;

	if ( this == other )
		return nullptr;

	if ( other->IsMarkedForDeletion() )
	{
		return nullptr;
	}

	// check if the edict is already in the list
	groundlink_t *root = ( groundlink_t * )GetDataObject( GROUNDLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			if ( link->entity == other )
			{
				// no more to do
				return link;
			}
		}
	}
	else
	{
		root = ( groundlink_t * )CreateDataObject( GROUNDLINK );
		root->prevLink = root->nextLink = root;
	}

	// entity is not in list, so it's a new touch
	// add it to the touched list and then call the touch function

	// build new link
	link = AllocGroundLink();
	if ( !link )
		return nullptr;

	link->entity = other;
	// add it to the list
	link->nextLink = root->nextLink;
	link->prevLink = root;
	link->prevLink->nextLink = link;
	link->nextLink->prevLink = link;

	PhysicsStartGroundContact( other );

	return link;
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever two entities come in contact
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStartGroundContact( CBaseEntity *pentOther )
{
	if ( !pentOther )
		return;

	if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
	{
		pentOther->StartGroundContact( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: notifies an entity than another touching entity has moved out of contact.
// Input  : *other - the entity to be acted upon
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNotifyOtherOfGroundRemoval( CBaseEntity *ent, CBaseEntity *other )
{
	if ( !other )
		return;

	// loop through ed's touch list, looking for the notifier
	// remove and call untouch if found
	groundlink_t *root = ( groundlink_t * )other->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			if ( link->entity == ent )
			{
				PhysicsRemoveGround( other, link );

				if ( root->nextLink == root && 
					 root->prevLink == root )
				{
					other->DestroyDataObject( GROUNDLINK );
				}
				return;
			}

			link = link->nextLink;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: removes a toucher from the list
// Input  : *link - the link to remove
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveGround( CBaseEntity *other, groundlink_t *link )
{
	// Every start Touch gets a corresponding end touch
	if ( link->entity != nullptr )
	{
		CBaseEntity *linkEntity = link->entity;
		CBaseEntity *otherEntity = other;
		if ( linkEntity && otherEntity )
		{
			linkEntity->EndGroundContact( otherEntity );
		}
	}

	link->nextLink->prevLink = link->prevLink;
	link->prevLink->nextLink = link->nextLink;
	FreeGroundLink( link );
}

//-----------------------------------------------------------------------------
// Purpose: static method to remove ground list for an entity
// Input  : *ent - 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveGroundList( CBaseEntity *ent )
{
	groundlink_t *link, *nextLink;

	groundlink_t *root = ( groundlink_t * )ent->GetDataObject( GROUNDLINK );
	if ( root )
	{
		link = root->nextLink;
		while ( link && link != root )
		{
			nextLink = link->nextLink;

			// notify the other entity that this ent has gone away
			PhysicsNotifyOtherOfGroundRemoval( ent, link->entity );

			// kill it
			FreeGroundLink( link );

			link = nextLink;
		}

		ent->DestroyDataObject( GROUNDLINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame that two entities are touching
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsTouch( CBaseEntity *pentOther )
{
	if ( pentOther )
	{
		if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
		{
			Touch( pentOther );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever two entities come in contact
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStartTouch( CBaseEntity *pentOther )
{
	if ( pentOther )
	{
		if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
		{
			StartTouch( pentOther );
			Touch( pentOther );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Marks in an entity that it is touching another entity, and calls
//			it's Touch() function if it is a new touch.
//			Stamps the touch link with the new time so that when we check for
//			untouch we know things haven't changed.
// Input  : *other - entity that it is in contact with
//-----------------------------------------------------------------------------
touchlink_t *CBaseEntity::PhysicsMarkEntityAsTouched( CBaseEntity *other )
{
	touchlink_t *link;

	if ( this == other )
		return nullptr;

	// Entities in hierarchy should not interact
	if ( (this->GetMoveParent() == other) || (this == other->GetMoveParent()) )
		return nullptr;

	// check if either entity doesn't generate touch functions
	if ( (GetFlags() | other->GetFlags()) & FL_DONTTOUCH )
		return nullptr;

	// Pure triggers should not touch each other
	if ( IsSolidFlagSet( FSOLID_TRIGGER ) && other->IsSolidFlagSet( FSOLID_TRIGGER ) )
	{
		if (!IsSolid() && !other->IsSolid())
			return nullptr;
	}

	// Don't do touching if marked for deletion
	if ( other->IsMarkedForDeletion() )
	{
		return nullptr;
	}

	if ( IsMarkedForDeletion() )
	{
		return nullptr;
	}

	// check if the edict is already in the list
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			if ( link->entityTouched == other )
			{
				// update stamp
				link->touchStamp = touchStamp;
				
				if ( !CBaseEntity::sm_bDisableTouchFuncs )
				{
					PhysicsTouch( other );
				}

				// no more to do
				return link;
			}
		}
	}
	else
	{
		// Allocate the root object
		root = ( touchlink_t * )CreateDataObject( TOUCHLINK );
		root->nextLink = root->prevLink = root;
	}

	// entity is not in list, so it's a new touch
	// add it to the touched list and then call the touch function

	// build new link
	link = AllocTouchLink();
	if ( DebugTouchlinks() )
		Msg( "add 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, GetDebugName(), other->GetDebugName(), entindex(), other->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
	if ( !link )
		return nullptr;

	link->touchStamp = touchStamp;
	link->entityTouched = other;
	link->flags = 0;
	// add it to the list
	link->nextLink = root->nextLink;
	link->prevLink = root;
	link->prevLink->nextLink = link;
	link->nextLink->prevLink = link;

	// non-solid entities don't get touched
	bool bShouldTouch = (IsSolid() && !IsSolidFlagSet(FSOLID_VOLUME_CONTENTS)) || IsSolidFlagSet(FSOLID_TRIGGER);
	if ( bShouldTouch && !other->IsSolidFlagSet(FSOLID_TRIGGER) )
	{
		link->flags |= FTOUCHLINK_START_TOUCH;
		if ( !CBaseEntity::sm_bDisableTouchFuncs )
		{
			PhysicsStartTouch( other );
		}
	}

	return link;
}

static trace_t g_TouchTrace;
const trace_t &CBaseEntity::GetTouchTrace( void )
{
	return g_TouchTrace;
}

//-----------------------------------------------------------------------------
// Purpose: Marks the fact that two edicts are in contact
// Input  : *other - other entity
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsMarkEntitiesAsTouching( CBaseEntity *other, trace_t &trace )
{
	g_TouchTrace = trace;
	touchlink_t *pLink0 = PhysicsMarkEntityAsTouched( other );
	touchlink_t *pLInk1 = other->PhysicsMarkEntityAsTouched( this );

	if ( pLink0 && !pLInk1 )
	{
		PhysicsNotifyOtherOfUntouch( other, this );
	}
	if ( pLInk1 && !pLink0 )
	{
		PhysicsNotifyOtherOfUntouch( this, other );
	}
	UTIL_ClearTrace( g_TouchTrace );
}

void CBaseEntity::PhysicsMarkEntitiesAsTouchingEventDriven( CBaseEntity *other, trace_t &trace )
{
	g_TouchTrace = trace;
	g_TouchTrace.m_pEnt = other;

	touchlink_t *link;
	link = this->PhysicsMarkEntityAsTouched( other );
	if ( link )
	{
		// mark these links as event driven so they aren't untouched the next frame
		// when the physics doesn't refresh them
		link->touchStamp = TOUCHSTAMP_EVENT_DRIVEN;
	}
	g_TouchTrace.m_pEnt = this;
	link = other->PhysicsMarkEntityAsTouched( this );
	if ( link )
	{
		link->touchStamp = TOUCHSTAMP_EVENT_DRIVEN;
	}
	UTIL_ClearTrace( g_TouchTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Two entities have touched, so run their touch functions
// Input  : *other - 
//			*ptrace - 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsImpact( CBaseEntity *other, trace_t &trace )
{
	if ( !other )
	{
		return;
	}

	// If either of the entities is flagged to be deleted, 
	//  don't call the touch functions
	if ( ( GetFlags() | other->GetFlags() ) & FL_KILLME )
	{
		return;
	}

	PhysicsMarkEntitiesAsTouching( other, trace );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the mask of what is solid for the given entity
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseEntity::PhysicsSolidMaskForEntity_Impl( void ) const
{
	return MASK_SOLID;
}

static inline int GetWaterContents( const Vector &point )
{
	// left 4 dead doesn't support moveable water brushes, only world water
	return enginetrace->GetPointContents_WorldOnly(point, MASK_WATER);
}
//-----------------------------------------------------------------------------
// Computes the water level + type
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateWaterState()
{
	// FIXME: This computation is nonsensical for rigid child attachments
	// Should we just grab the type + level of the parent?
	// Probably for rigid children anyways...

	// Compute the point to check for water state
	Vector	point;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &point );

	SetWaterLevel( 0 );
	SetWaterType( CONTENTS_EMPTY );
	int cont = GetWaterContents(point);

	if (( cont & MASK_WATER ) == 0)
		return;

	SetWaterType( cont );
	SetWaterLevel( 1 );

	// point sized entities are always fully submerged
	if ( IsPointSized() )
	{
		SetWaterLevel( 3 );
	}
	else
	{
		// Check the exact center of the box
		point[2] = WorldSpaceCenter().z;

		int midcont = GetWaterContents(point);
		if ( midcont & MASK_WATER )
		{
			// Now check where the eyes are...
			SetWaterLevel( 2 );
			point[2] = EyePosition().z;

			int eyecont = GetWaterContents(point);
			if ( eyecont & MASK_WATER )
			{
				SetWaterLevel( 3 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if entity is in the water and applies any current to velocity
// and sets appropriate water flags
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsCheckWater( void )
{
	return GetWaterLevel() > WL_Feet;
}

//-----------------------------------------------------------------------------
// Purpose: Bounds velocity
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckVelocity( void )
{
	Vector origin = GetAbsOrigin();
	Vector vecAbsVelocity = GetAbsVelocity();

	bool bReset = false;
	for ( int i=0 ; i<3 ; i++ )
	{
		if ( IS_NAN(vecAbsVelocity[i]) )
		{
			Msg( "Got a NaN velocity on %s\n", GetClassname() );
			vecAbsVelocity[i] = 0;
			bReset = true;
		}
		if ( IS_NAN(origin[i]) )
		{
			Msg( "Got a NaN origin on %s\n", GetClassname() );
			origin[i] = 0;
			bReset = true;
		}

		if ( vecAbsVelocity[i] > sv_maxvelocity.GetFloat() ) 
		{
			vecAbsVelocity[i] = sv_maxvelocity.GetFloat();
			bReset = true;
		}
		else if ( vecAbsVelocity[i] < -sv_maxvelocity.GetFloat() )
		{
			vecAbsVelocity[i] = -sv_maxvelocity.GetFloat();
			bReset = true;
		}
	}

	if (bReset)
	{
		SetAbsOrigin( origin );
		SetAbsVelocity( vecAbsVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies gravity to falling objects
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsAddGravityMove( Vector &move )
{
	Vector vecAbsVelocity = GetAbsVelocity();

	move.x = (vecAbsVelocity.x + GetBaseVelocity().x ) * gpGlobals->frametime;
	move.y = (vecAbsVelocity.y + GetBaseVelocity().y ) * gpGlobals->frametime;

	if ( GetFlags() & FL_ONGROUND )
	{
		move.z = GetBaseVelocity().z * gpGlobals->frametime;
		return;
	}

	// linear acceleration due to gravity
	float newZVelocity = vecAbsVelocity.z - GetActualGravity( this ) * gpGlobals->frametime;

	move.z = ((vecAbsVelocity.z + newZVelocity) / 2.0 + GetBaseVelocity().z ) * gpGlobals->frametime;

	Vector vecBaseVelocity = GetBaseVelocity();
	vecBaseVelocity.z = 0.0f;
	SetBaseVelocity( vecBaseVelocity );
	
	vecAbsVelocity.z = newZVelocity;
	SetAbsVelocity( vecAbsVelocity );

	// Bound velocity
	PhysicsCheckVelocity();
}

static constexpr auto STOP_EPSILON = 0.1;
//-----------------------------------------------------------------------------
// Purpose: Slide off of the impacting object.  Returns the blocked flags (1 = floor, 2 = step / wall)
// Input  : in - 
//			normal - 
//			out - 
//			overbounce - 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::PhysicsClipVelocity( const Vector& in, const Vector& normal, Vector& out, float overbounce )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	blocked = 0;

	angle = normal[ 2 ];

	if ( angle > 0 )
	{
		blocked |= 1;		// floor
	}
	if ( !angle )
	{
		blocked |= 2;		// step
	}
	
	backoff = DotProduct (in, normal) * overbounce;

	for ( i=0 ; i<3 ; i++ )
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
		{
			out[i] = 0;
		}
	}
	
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionBounce( trace_t &trace, Vector &vecVelocity, float flMinTotalElasticity )
{
	// Get the impact surface's elasticity.
	float flSurfaceElasticity;
	physprops->GetPhysicsProperties( trace.surface.surfaceProps, nullptr, nullptr, nullptr, &flSurfaceElasticity );
	
	float flTotalElasticity = GetElasticity() * flSurfaceElasticity;
	if ( flMinTotalElasticity > 0.9f )
	{
		flMinTotalElasticity = 0.9f;
	}
	flTotalElasticity = clamp( flTotalElasticity, flMinTotalElasticity, 0.9f );

	// NOTE: A backoff of 2.0f is a reflection
	Vector vecAbsVelocity;
	PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, 2.0f );
	vecAbsVelocity *= flTotalElasticity;

	// Get the total velocity (player + conveyors, etc.)
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	// Stop if on ground.
	if ( trace.plane.normal.z > 0.7f )			// Floor
	{
		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
		contract_assert( pEntity );

		// Are we on the ground?
		if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
		{
			vecAbsVelocity.z = 0.0f;

			// Recompute speedsqr based on the new absvel
			VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
			flSpeedSqr = DotProduct( vecVelocity, vecVelocity );
		}

		SetAbsVelocity( vecAbsVelocity );

		if ( flSpeedSqr < ( 30 * 30 ) )
		{
			if ( pEntity->IsStandable() )
			{
				SetGroundEntity( pEntity );
			}

			// Reset velocities.
			SetAbsVelocity( vec3_origin );
			SetLocalAngularVelocity( vec3_angle );
		}
		else
		{
			Vector vecDelta = GetBaseVelocity() - vecAbsVelocity;	
			Vector vecBaseDir = GetBaseVelocity();
			VectorNormalize( vecBaseDir );
			float flScale = vecDelta.Dot( vecBaseDir );

			VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecVelocity ); 
			VectorMA( vecVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, GetBaseVelocity() * flScale, vecVelocity );
			PhysicsPushEntity( vecVelocity, &trace );
		}
	}
	else
	{
		// If we get *too* slow, we'll stick without ever coming to rest because
		// we'll get pushed down by gravity faster than we can escape from the wall.
		if ( flSpeedSqr < ( 30 * 30 ) )
		{
			// Reset velocities.
			SetAbsVelocity( vec3_origin );
			SetLocalAngularVelocity( vec3_angle );
		}
		else
		{
			SetAbsVelocity( vecAbsVelocity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionSlide( trace_t &trace, Vector &vecVelocity )
{
	// Get the impact surface's friction.
	float flSurfaceFriction;
	physprops->GetPhysicsProperties( trace.surface.surfaceProps, nullptr, nullptr, &flSurfaceFriction, nullptr );

	// A backoff of 1.0 is a slide.
	float flBackOff = 1.0f;	
	Vector vecAbsVelocity;
	PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, flBackOff );

	if ( trace.plane.normal.z <= 0.7 )			// Floor
	{
		SetAbsVelocity( vecAbsVelocity );
		return;
	}

	// Stop if on ground.
	// Get the total velocity (player + conveyors, etc.)
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	// Verify that we have an entity.
	CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
	contract_assert( pEntity );

	// Are we on the ground?
	if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
	{
		vecAbsVelocity.z = 0.0f;

		// Recompute speedsqr based on the new absvel
		VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
		flSpeedSqr = DotProduct( vecVelocity, vecVelocity );
	}
	SetAbsVelocity( vecAbsVelocity );

	if ( flSpeedSqr < ( 30 * 30 ) )
	{
		if ( pEntity->IsStandable() )
		{
			SetGroundEntity( pEntity );
		}

		// Reset velocities.
		SetAbsVelocity( vec3_origin );
		SetLocalAngularVelocity( vec3_angle );
	}
	else
	{
		vecAbsVelocity += GetBaseVelocity();
		vecAbsVelocity *= ( 1.0f - trace.fraction ) * gpGlobals->frametime * flSurfaceFriction;
		PhysicsPushEntity( vecAbsVelocity, &trace );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionCustom_Impl( trace_t &trace, Vector &vecVelocity )
{
	// Stop if on ground.
	if ( trace.plane.normal.z > 0.7 )			// Floor
	{
		// Get the total velocity (player + conveyors, etc.)
		VectorAdd( GetAbsVelocity(), GetBaseVelocity(), vecVelocity );

		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
		contract_assert( pEntity );

		// Are we on the ground?
		if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
		{
			Vector vecAbsVelocity = GetAbsVelocity();
			vecAbsVelocity.z = 0.0f;
			SetAbsVelocity( vecAbsVelocity );
		}

		if ( pEntity->IsStandable() )
		{
			SetGroundEntity( pEntity );
		}
	}
}

//-----------------------------------------------------------------------------
// Performs the collision resolution for fliers.
//-----------------------------------------------------------------------------
void CBaseEntity::PerformFlyCollisionResolution( trace_t &trace, Vector &move )
{
	switch( GetMoveCollide() )
	{
	case MOVECOLLIDE_FLY_CUSTOM:
		{
			ResolveFlyCollisionCustom( trace, move );
			break;
		}

	case MOVECOLLIDE_FLY_BOUNCE:
		{
			ResolveFlyCollisionBounce( trace, move );
			break;
		}

	case MOVECOLLIDE_FLY_SLIDE:
	case MOVECOLLIDE_DEFAULT:
	// NOTE: The default fly collision state is the same as a slide (for backward capatability).
		{
			ResolveFlyCollisionSlide( trace, move );
			break;
		}

	default:
		{
			// Invalid MOVECOLLIDE_<type>
			contract_assert( 0 );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks if an object has passed into or out of water and sets water info, alters velocity, plays splash sounds, etc.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckWaterTransition( void )
{
	int oldcont = GetWaterType();
	UpdateWaterState();
	int cont = GetWaterType();

	// We can exit right out if we're a child... don't bother with this...
	if (GetMoveParent())
		return;

	if ( cont & MASK_WATER )
	{
		if (oldcont == CONTENTS_EMPTY)
		{
			Splash();

			// just crossed into water
			EmitSound( "BaseEntity.EnterWater" );

			if ( !IsEFlagSet( EFL_NO_WATER_VELOCITY_CHANGE ) )
			{
				Vector vecAbsVelocity = GetAbsVelocity();
				vecAbsVelocity[2] *= 0.5;
				SetAbsVelocity( vecAbsVelocity );
			}
		}
	}
	else
	{
		if ( oldcont != CONTENTS_EMPTY )
		{	
			// just crossed out of water
			EmitSound( "BaseEntity.ExitWater" );
		}		
	}
}

//-----------------------------------------------------------------------------
// Computes new angles based on the angular velocity
//-----------------------------------------------------------------------------
void CBaseEntity::SimulateAngles( float flFrameTime )
{
	// move angles
	QAngle angles;
	VectorMA ( GetLocalAngles(), flFrameTime, GetLocalAngularVelocity(), angles );
	SetLocalAngles( angles );
}

//-----------------------------------------------------------------------------
// Purpose: Toss, bounce, and fly movement.  When onground, do nothing.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsToss( void )
{
	trace_t	trace;
	Vector	move;

	PhysicsCheckWater();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	// Moving upward, off the ground, or  resting on a client/monster, remove FL_ONGROUND
	if ( GetAbsVelocity()[2] > 0 || !GetGroundEntity() || !GetGroundEntity()->IsStandable() )
	{
		SetGroundEntity( nullptr );
	}

	// Check to see if entity is on the ground at rest
	if ( GetFlags() & FL_ONGROUND )
	{
		if ( VectorCompare( GetAbsVelocity(), vec3_origin ) )
		{
			// Clear rotation if not moving (even if on a conveyor)
			SetLocalAngularVelocity( vec3_angle );
			if ( VectorCompare( GetBaseVelocity(), vec3_origin ) )
				return;
		}
	}

	PhysicsCheckVelocity();

	// add gravity
	if ( GetMoveType() == MOVETYPE_FLYGRAVITY && !(GetFlags() & FL_FLY) )
	{
		PhysicsAddGravityMove( move );
	}
	else
	{
		// Base velocity is not properly accounted for since this entity will move again after the bounce without
		// taking it into account
		Vector vecAbsVelocity = GetAbsVelocity();
		vecAbsVelocity += GetBaseVelocity();
		VectorScale( vecAbsVelocity, gpGlobals->frametime, move );
		PhysicsCheckVelocity();
	}

	// move angles
	SimulateAngles( gpGlobals->frametime );

	// move origin
	PhysicsPushEntity( move, &trace );

	if ( VPhysicsGetObject() )
	{
		VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), vec3_angle, true, gpGlobals->frametime );
	}

	PhysicsCheckVelocity();

/*
	NOTE[pmf]: removed this because grenades can start out inside a player volume (particularly in casual mode).
	Behavior for this is handled properly in the grenade's custom FlyCollisionResolution function

	if (trace.allsolid )
	{	
		// entity is trapped in another solid
		SetAbsVelocity(vec3_origin);
		SetLocalAngularVelocity(vec3_angle);
		return;
	}
*/
	
	if (IsEdictFree())
		return;
	
	if ( debugoverlay && sv_grenade_trajectory.GetInt() && (GetFlags() & FL_GRENADE) )
	{
		QAngle angGrTrajAngles;
		Vector vec3tempOrientation = (trace.endpos - trace.startpos);
		VectorAngles( vec3tempOrientation, angGrTrajAngles );

		float flGrTraThickness = sv_grenade_trajectory_thickness.GetFloat();
		Vector vec3_GrTrajMin = Vector( 0, -flGrTraThickness, -flGrTraThickness );
		Vector vec3_GrTrajMax = Vector( vec3tempOrientation.Length(), flGrTraThickness, flGrTraThickness );
		bool bDotted = ( sv_grenade_trajectory_dash.GetInt() && (std::fmod( gpGlobals->curtime, 0.1f ) < 0.05f) );
		
		//extruded "line" is really a box for more visible thickness
		debugoverlay->AddBoxOverlay( trace.startpos, vec3_GrTrajMin, vec3_GrTrajMax, angGrTrajAngles, 0, (bDotted ? 20 : 200), 0, 255, sv_grenade_trajectory_time.GetFloat() );

		//per-bounce box
		if (trace.fraction != 1.0f)
			debugoverlay->AddBoxOverlay( trace.endpos, Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE ), Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE ), QAngle( 0, 0, 0 ), 220, 0, 0, 190, sv_grenade_trajectory_time.GetFloat( ) );
	}	

	if (trace.fraction != 1.0f)
	{
		PerformFlyCollisionResolution( trace, move );
	}
	
	// check for in water
	PhysicsCheckWaterTransition();
}

//-----------------------------------------------------------------------------
// Simulation in local space of rigid children
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRigidChild( void )
{
	CVProfScope _( "CBaseEntity::PhysicsRigidChild", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// NOTE: rigidly attached children do simulation in local space
	// Collision impulses will be handled either not at all, or by
	// forwarding the information to the highest move parent

	Vector vecPrevOrigin = GetAbsOrigin();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	{ CVProfScope _( "CBaseEntity::PhysicsRigidChild-2", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Cause touch functions to be called
	PhysicsTouchTriggers( &vecPrevOrigin );

	// We have to do this regardless owing to hierarchy
	if ( VPhysicsGetObject() )
	{
		int solidType = GetSolid();
		bool bAxisAligned = ( solidType == SOLID_BBOX || solidType == SOLID_NONE ) ? true : false;
		VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), bAxisAligned ? vec3_angle : GetAbsAngles(), true, gpGlobals->frametime );
	}

	};
}

//-----------------------------------------------------------------------------
// Computes the base velocity
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateBaseVelocity( void )
{
	if ( GetFlags() & FL_ONGROUND )
	{
		CBaseEntity	*groundentity = GetGroundEntity();
		if ( groundentity )
		{
			// On conveyor belt that's moving?
			if ( groundentity->GetFlags() & FL_CONVEYOR )
			{
				Vector vecNewBaseVelocity;
				groundentity->GetGroundVelocityToApply( vecNewBaseVelocity );
				if ( GetFlags() & FL_BASEVELOCITY )
				{
					vecNewBaseVelocity += GetBaseVelocity();
				}
				AddFlag( FL_BASEVELOCITY );
				SetBaseVelocity( vecNewBaseVelocity );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs a frame of physics for a specific edict (and all it's children)
// Input  : *ent - the thinking edict
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsSimulate_Impl( void )
{
	CVProfScope _( "CBaseEntity::PhysicsSimulate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// NOTE:  Players override PhysicsSimulate and drive through their CUserCmds at that point instead of
	//  processng through this function call!!!  They shouldn't chain to here ever.
	// Make sure not to simulate this guy twice per frame
	if ( !IsPlayerSimulated() && m_nSimulationTick == gpGlobals->tickcount )
	{
		return;
	}

	m_nSimulationTick = gpGlobals->tickcount;

	contract_assert( !IsPlayer() );

	// If we've got a moveparent, we must simulate that first.
	CBaseEntity *pMoveParent = GetMoveParent();

	if ( (GetMoveType() == MOVETYPE_NONE && !pMoveParent) || (GetMoveType() == MOVETYPE_VPHYSICS ) )
	{
		PhysicsNone();
		return;
	}

	// If ground entity goes away, make sure FL_ONGROUND is valid
	if ( !GetGroundEntity() )
	{
		RemoveFlag( FL_ONGROUND );
	}

	if (pMoveParent)
	{
		CVProfScope _( "CBaseEntity::PhysicsSimulate-MoveParent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		pMoveParent->PhysicsSimulate();
	}
	else
	{
		CVProfScope _( "CBaseEntity::PhysicsSimulate-BaseVelocity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		UpdateBaseVelocity();

		if ( ((GetFlags() & FL_BASEVELOCITY) == 0) && (GetBaseVelocity() != vec3_origin) )
		{
			// Apply momentum (add in half of the previous frame of velocity first)
			// BUGBUG: This will break with PhysicsStep() because of the timestep difference
			Vector vecAbsVelocity;
			VectorMA( GetAbsVelocity(), 1.0 + (gpGlobals->frametime*0.5), GetBaseVelocity(), vecAbsVelocity );
			SetAbsVelocity( vecAbsVelocity );
			SetBaseVelocity( vec3_origin );
		}
		RemoveFlag( FL_BASEVELOCITY );
	}

	switch( GetMoveType() )
	{
	case MOVETYPE_PUSH:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_PUSH", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsPusher();
		}
		break;

	case MOVETYPE_VPHYSICS:
		{
		}
		break;

	case MOVETYPE_NONE:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_NONE", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			contract_assert( pMoveParent );
			PhysicsRigidChild();
		}
		break;

	case MOVETYPE_NOCLIP:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_NOCLIP", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsNoclip();
		}
		break;

	case MOVETYPE_STEP:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_STEP", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsStep();
		}
		break;

	case MOVETYPE_FLY:
	case MOVETYPE_FLYGRAVITY:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_FLY", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsToss();
		}
		break;

	case MOVETYPE_CUSTOM:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_CUSTOM", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsCustom();
		}
		break;

	default:
		Warning( "PhysicsSimulate: %s bad movetype %d", GetClassname(), GetMoveType() );
		contract_assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs thinking code if time.  There is some play in the exact time the think
//  function will be called, because it is called before any movement is done
//  in a frame.  Not used for pushmove objects, because they must be exact.
//  Returns false if the entity removed itself.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsRunThink( thinkmethods_t thinkMethod )
{
	if ( IsEFlagSet( EFL_NO_THINK_FUNCTION ) )
		return true;
	
	bool bAlive = true;

	// Don't fire the base if we're avoiding it
	if ( thinkMethod != THINK_FIRE_ALL_BUT_BASE )
	{
		bAlive = PhysicsRunSpecificThink( -1, &CBaseEntity::Think );
		if ( !bAlive )
			return false;
	}

	// Are we just firing the base think?
	if ( thinkMethod == THINK_FIRE_BASE_ONLY )
		return bAlive;

	// Fire the rest of 'em
	for ( int i = 0; i < std::ssize( m_aThinkFunctions ); i++ )
	{

		bAlive = PhysicsRunSpecificThink( i, m_aThinkFunctions[i].m_pfnThink );

		if ( !bAlive )
			return false;
	}
	
	return bAlive;
}

//-----------------------------------------------------------------------------
// Purpose: For testing if all thinks are occuring at the same time
//-----------------------------------------------------------------------------
struct ThinkSync
{
	float					thinktime;
	int						thinktick;
	std::vector< EHANDLE >	entities;

	ThinkSync()
	{
		thinktime = 0;
	}

	ThinkSync( const ThinkSync& src )
	{
		thinktime = src.thinktime;
		thinktick = src.thinktick;
		int c = std::ssize( src.entities );
		for ( int i = 0; i < c; i++ )
		{
			entities.push_back( src.entities[ i ] );
		}
	}
};

static ConVar sv_thinktimecheck( side_registry(), "sv_thinktimecheck", "0", 0, "Check for thinktimes all on same timestamp." );

//-----------------------------------------------------------------------------
// Purpose: For testing if all thinks are occuring at the same time
//-----------------------------------------------------------------------------
class CThinkSyncTester
{
public:
	CThinkSyncTester()
	{
		  m_nLastFrameCount = -1;
		  m_bShouldCheck = false;
	}

	void EntityThinking( int framecount, CBaseEntity *ent, float thinktime, int thinktick )
	{
		if ( m_nLastFrameCount != framecount )
		{
			if ( m_bShouldCheck )
			{
				// Report
				Report();
				m_Thinkers.clear();
				m_nLastFrameCount = framecount;
			}

			m_bShouldCheck = sv_thinktimecheck.GetBool();
		}

		if ( !m_bShouldCheck )
			return;

		ThinkSync *p = FindOrAddItem( ent, thinktime );
		if ( !p )
		{
			contract_assert( 0 );
		}

		p->thinktime = thinktime;
		p->thinktick = thinktick;
		EHANDLE h;
		h = ent;
		p->entities.push_back( h );
	}

private:

	ThinkSync	*FindOrAddItem( CBaseEntity *ent, float thinktime )
	{
		ThinkSync &item = m_Thinkers[ thinktime ];
		item.thinktime = thinktime;
		return &item;
	}

	void Report()
	{
		if ( m_Thinkers.empty() )
			return;

		Msg( "-----------------\nThink report frame %i\n", gpGlobals->tickcount );

		for ( auto &[flThinkTime, item] : m_Thinkers )
		{
			ThinkSync *p = &item;
			contract_assert( p );
			if ( !p )
				continue;

			int ecount = std::ssize( p->entities );
			if ( !ecount )
			{
				continue;
			}

			Msg( "thinktime %f, %i entities\n", p->thinktime, ecount );
			for ( int j =0; j < ecount; j++ )
			{
				EHANDLE h = p->entities[ j ];
				int lastthinktick = 0;
				int nextthinktick = 0;
				CBaseEntity *e = h.Get();
				if ( e )
				{
					lastthinktick = e->m_nLastThinkTick;
					nextthinktick = e->m_nNextThinkTick;
				}

				Msg( "  %p : %30s (last %5i/next %5i)\n", h.Get(), h.Get() ? h->GetClassname() : "NULL",
					lastthinktick, nextthinktick );
			}
		}
	}

	std::map< float, ThinkSync >	m_Thinkers;	// keyed by thinktime
	int			m_nLastFrameCount;
	bool		m_bShouldCheck;
};

static CThinkSyncTester g_ThinkChecker;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsRunSpecificThink( int nContextIndex, BASEPTR thinkFunc )
{
	int thinktick = GetNextThinkTick( nContextIndex );

	if ( thinktick <= 0 || thinktick > gpGlobals->tickcount )
		return true;
	
	float thinktime = thinktick * TICK_INTERVAL;

	// Don't let things stay in the past.
	//  it is possible to start that way
	//  by a trigger with a local time.
	if ( thinktime < gpGlobals->curtime )
	{
		thinktime = gpGlobals->curtime;	
	}
	
	// Only do this on the game server
	g_ThinkChecker.EntityThinking( gpGlobals->tickcount, this, thinktime, m_nNextThinkTick );

	SetNextThink( nContextIndex, TICK_NEVER_THINK );

	PhysicsDispatchThink( thinkFunc );

	SetLastThink( nContextIndex, gpGlobals->curtime );

	// Return whether entity is still valid
	return ( !IsMarkedForDeletion() );
}

void CBaseEntity::SetGroundEntity( CBaseEntity *ground )
{
	if ( GetGroundEntity() == ground )
		return;

	// this can happen in-between updates to the held object controller (physcannon, +USE)
	// so trap it here and release held objects when they become player ground
	if ( ground && IsPlayer() && ground->GetMoveType()== MOVETYPE_VPHYSICS )
	{
		CBasePlayer *pPlayer = ToBasePlayer(this);
		IPhysicsObject *pPhysGround = ground->VPhysicsGetObject();
		if ( pPhysGround && pPlayer )
		{
			if ( pPhysGround->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
			{
				pPlayer->ForceDropOfCarriedPhysObjects( ground );
			}
		}
	}

	CBaseEntity *oldGround = GetGroundEntity();
	m_hGroundEntity = ground ? ground->GetRefEHandle() : CBaseHandle();

	// Just starting to touch
	if ( !oldGround && ground )
	{
		ground->AddEntityToGroundList( this );
	}
	// Just stopping touching
	else if ( oldGround && !ground )
	{
		PhysicsNotifyOtherOfGroundRemoval( this, oldGround );
	}
	// Changing out to new ground entity
	else
	{
		PhysicsNotifyOtherOfGroundRemoval( this, oldGround );
		ground->AddEntityToGroundList( this );
	}

	// HACK/PARANOID:  This is redundant with the code above, but in case we get out of sync groundlist entries ever, 
	//  this will force the appropriate flags
	if ( ground )
	{
		AddFlag( FL_ONGROUND );
	}
	else
	{
		RemoveFlag( FL_ONGROUND );
	}
}

CBaseEntity *CBaseEntity::GetGroundEntity( void )
{
	return static_cast<CBaseEntity *>( EntityFromHandle( m_hGroundEntity.Get() ) );
}

void CBaseEntity::StartGroundContact( CBaseEntity *ground )
{
	AddFlag( FL_ONGROUND );
//	Msg( "+++ %s starting contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void CBaseEntity::EndGroundContact( CBaseEntity *ground )
{
	RemoveFlag( FL_ONGROUND );
//	Msg( "--- %s ending contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void CBaseEntity::SetGroundChangeTime( float flTime )
{
	m_flGroundChangeTime = flTime;
}

float CBaseEntity::GetGroundChangeTime( void )
{
	return m_flGroundChangeTime;
}

// Remove this as ground entity for all object resting on this object
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::WakeRestingObjects()
{
	// Unset this as ground entity for everything resting on this object
	//  This calls endgroundcontact for everything on the list
	PhysicsRemoveGroundList( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ent - 
//-----------------------------------------------------------------------------
bool CBaseEntity::HasNPCsOnIt( void )
{
	groundlink_t *link;
	groundlink_t *root = ( groundlink_t * )GetDataObject( GROUNDLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			
		}
	}

	return false;
}

#undef DEF_THINK_LIMIT
#undef DebugTouchlinks

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
const char *GetMassEquivalent(float flMass);

#define RAGDOLL_VISUALIZE 0

//-----------------------------------------------------------------------------
// ThinkContext
//-----------------------------------------------------------------------------
const char *s_pFadeOutContext = "RagdollFadeOutContext";
const char *s_pDebrisContext = "DebrisContext";

static const float ATTACHED_DAMPING_SCALE = 50.0f;

//-----------------------------------------------------------------------------
// Spawnflags
//-----------------------------------------------------------------------------
static constexpr auto SF_RAGDOLLPROP_DEBRIS = 0x0004;
static constexpr auto SF_RAGDOLLPROP_USE_LRU_RETIREMENT = 0x1000;
static constexpr auto SF_RAGDOLLPROP_ALLOW_DISSOLVE = 0x2000; // Allow this prop to be dissolved
static constexpr auto SF_RAGDOLLPROP_MOTIONDISABLED = 0x4000;
static constexpr auto SF_RAGDOLLPROP_ALLOW_STRETCH = 0x8000;
static constexpr auto SF_RAGDOLLPROP_STARTASLEEP = 0x10000;

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( physics_prop_ragdoll, CRagdollProp );
LINK_ENTITY_TO_CLASS_ALIAS( prop_ragdoll, CRagdollProp );
EXTERN_SEND_TABLE(DT_Ragdoll)

IMPLEMENT_REFLECT_SERVERCLASS( CRagdollProp, DT_Ragdoll )

IMPLEMENT_REFLECT_DATAMAP( CRagdollProp )

//-----------------------------------------------------------------------------
// Disable auto fading under dx7 or when level fades are specified
//-----------------------------------------------------------------------------
void CRagdollProp::DisableAutoFade()
{
	SetGlobalFadeScale( 0.0f );
	m_flDefaultFadeScale = 0;
}

void CRagdollProp::Spawn_Impl( void )
{
	// Starts out as the default fade scale value
	m_flDefaultFadeScale = GetGlobalFadeScale();

	contract_assert( RAGDOLL_MAX_ELEMENTS == 24 );
	Precache();
	SetModel( STRING( GetModelName() ) );

	CStudioHdr *pStudioHdr = GetModelPtr( );
	if ( pStudioHdr->flags() & STUDIOHDR_FLAGS_NO_FORCED_FADE )
	{
		DisableAutoFade();
	}
	else
	{
		SetGlobalFadeScale( m_flDefaultFadeScale );
	}

	matrix3x4a_t pBoneToWorld[MAXSTUDIOBONES];
	BaseClass::SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING ); // FIXME: shouldn't this be a subset of the bones
	// this is useless info after the initial conditions are set
	SetAbsAngles( vec3_angle );
	int collisionGroup = (m_spawnflags & SF_RAGDOLLPROP_DEBRIS) ? COLLISION_GROUP_DEBRIS : COLLISION_GROUP_NONE;
	bool bWake = (m_spawnflags & SF_RAGDOLLPROP_STARTASLEEP) ? false : true;
	InitRagdoll( vec3_origin, 0, vec3_origin, pBoneToWorld, pBoneToWorld, 0, collisionGroup, true, bWake );
	m_lastUpdateTickCount = 0;
	m_flBlendWeight = 0.0f;
	m_nOverlaySequence = -1;

	// Unless specified, do not allow this to be dissolved
	if ( HasSpawnFlags( SF_RAGDOLLPROP_ALLOW_DISSOLVE ) == false )
	{
		AddEFlags( EFL_NO_DISSOLVE );
	}

	if ( HasSpawnFlags(SF_RAGDOLLPROP_MOTIONDISABLED) )
	{
		DisableMotion();
	}

	if( m_bStartDisabled )
	{
		AddEffects( EF_NODRAW );
	}
}

void CRagdollProp::SetSourceClassName( const char *pClassname )
{
	m_strSourceClassName = MAKE_STRING( pClassname );
}

void CRagdollProp::OnRestore_Impl()
{
	// rebuild element 0 since it isn't saved
	// NOTE: This breaks the rules - the pointer needs to get fixed in Restore()
	m_ragdoll.list[0].pObject = VPhysicsGetObject();
	m_ragdoll.list[0].parentIndex = -1;
	m_ragdoll.list[0].originParentSpace.Init();

	BaseClass::OnRestore_Impl();
	if ( !m_ragdoll.listCount )
		return;

	// JAY: Reset collision relationships
	RagdollSetupCollisions( m_ragdoll, modelinfo->GetVCollide( GetModelIndex() ), GetModelIndex() );
	VPhysicsUpdate( VPhysicsGetObject() );
}

void CRagdollProp::CalcRagdollSize( void )
{
	CollisionProp()->SetSurroundingBoundsType( USE_HITBOXES );
	CollisionProp()->RemoveSolidFlags( FSOLID_FORCE_WORLD_ALIGNED );
}

void CRagdollProp::UpdateOnRemove_Impl( void )
{
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		if ( m_ragdoll.list[i].pObject )
		{
		}
	}

	// Set to null so that the destructor's call to DestroyObject won't destroy
	//  m_pObjects[ 0 ] twice since that's the physics object for the prop
	VPhysicsSetObject( nullptr );

	RagdollDestroy( m_ragdoll );
	// Chain to base after doing our own cleanup to mimic
	//  destructor unwind order
	BaseClass::UpdateOnRemove_Impl();
}

CRagdollProp::CRagdollProp( void )
{
	m_strSourceClassName = NULL_STRING;
	m_anglesOverrideString = NULL_STRING;
	m_ragdoll.listCount = 0;
	contract_assert( (1<<RAGDOLL_INDEX_BITS) >=RAGDOLL_MAX_ELEMENTS );
	m_allAsleep = false;
	SetGlobalFadeScale( 1.0f );
	m_flDefaultFadeScale = 1.0f;
}

CRagdollProp::~CRagdollProp( void )
{
}

void CRagdollProp::Precache_Impl( void )
{
	PrecacheModel( STRING( GetModelName() ) );
	BaseClass::Precache_Impl();
}

int CRagdollProp::ObjectCaps_Impl()
{
	return BaseClass::ObjectCaps_Impl() | FCAP_WCEDIT_POSITION;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::InitRagdollAnimation()
{
	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );
	
	// put into ACT_DIERAGDOLL if it exists, otherwise use sequence 0
	int nSequence = SelectWeightedSequence( ACT_DIERAGDOLL );
	if ( nSequence < 0 )
	{
		ResetSequence( 0 );
	}
	else
	{
		ResetSequence( nSequence );
	}
}

//-----------------------------------------------------------------------------
// Response system stuff
//-----------------------------------------------------------------------------
extern IResponseSystem *g_pResponseSystem;
IResponseSystem *CRagdollProp::GetResponseSystem_Impl()
{

	// Just use the general NPC response system; we often come from NPCs after all
	return g_pResponseSystem;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria_Impl( set );

	if ( m_strSourceClassName != NULL_STRING )
	{
		set.RemoveCriteria( "classname" );
		set.AppendCriteria( "classname", STRING(m_strSourceClassName) );
		set.AppendCriteria( "ragdoll", "1" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	// Clear out the classname if we've been physgunned before
	// so that the screams, etc. don't happen. Simulate that the first
	// major punt or throw has been enough to kill him.
	if ( m_bHasBeenPhysgunned )
	{
		m_strSourceClassName = NULL_STRING;
	}
	m_bHasBeenPhysgunned = true;

	if( HasPhysgunInteraction( "onpickup", "boogie" ) )
	{
		if ( reason == PUNTED_BY_CANNON )
		{
			CRagdollBoogie::Create( this, 150, gpGlobals->curtime, 3.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL );
		}
		else
		{
			CRagdollBoogie::Create( this, 150, gpGlobals->curtime, 2.0f, 0.0f );
		}
	}

	if ( HasSpawnFlags( SF_RAGDOLLPROP_USE_LRU_RETIREMENT ) )
	{
		s_RagdollLRU.MoveToTopOfLRU( this );
	}

	if ( !HasSpawnFlags( SF_PHYSPROP_ENABLE_ON_PHYSCANNON ) )
		return;

	ragdoll_t *pRagdollPhys = GetRagdoll( );
	for ( int j = 0; j < pRagdollPhys->listCount; ++j )
	{
		pRagdollPhys->list[j].pObject->Wake();
		pRagdollPhys->list[j].pObject->EnableMotion( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason )
{
	CDefaultPlayerPickupVPhysics::OnPhysGunDrop( pPhysGunUser, Reason );
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	if( HasPhysgunInteraction( "onpickup", "boogie" ) )
	{
		CRagdollBoogie::Create( this, 150, gpGlobals->curtime, 3.0f, SF_RAGDOLL_BOOGIE_ELECTRICAL );
	}

	if ( HasSpawnFlags( SF_RAGDOLLPROP_USE_LRU_RETIREMENT ) )
	{
		s_RagdollLRU.MoveToTopOfLRU( this );
	}

	// Make sure it's interactive debris for at most 5 seconds
	if ( GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS )
	{
		SetContextThink( &CRagdollProp::SetDebrisThink, gpGlobals->curtime + 5, s_pDebrisContext );
	}

	if ( Reason != LAUNCHED_BY_CANNON )
		return;

	if( HasPhysgunInteraction( "onlaunch", "spin_zaxis" ) )
	{
		Vector vecAverageCenter( 0, 0, 0 );

		// Get the average position, apply forces to produce a spin
		int j;
		ragdoll_t *pRagdollPhys = GetRagdoll( );
		for ( j = 0; j < pRagdollPhys->listCount; ++j )
		{
			Vector vecCenter;
			pRagdollPhys->list[j].pObject->GetPosition( &vecCenter, nullptr );
			vecAverageCenter += vecCenter;
		}

		vecAverageCenter /= pRagdollPhys->listCount;

		Vector vecZAxis( 0, 0, 1 );
		for ( j = 0; j < pRagdollPhys->listCount; ++j )
		{
			Vector vecDelta;
			pRagdollPhys->list[j].pObject->GetPosition( &vecDelta, nullptr );
			vecDelta -= vecAverageCenter;

			Vector vecDir;
			CrossProduct( vecZAxis, vecDelta, vecDir );
			vecDir *= 100;
			pRagdollPhys->list[j].pObject->AddVelocity( &vecDir, nullptr );
		}
	}

	PhysSetGameFlags( VPhysicsGetObject(), FVPHYSICS_WAS_THROWN );
	m_bFirstCollisionAfterLaunch = true;
}

//-----------------------------------------------------------------------------
// Physics attacker
//-----------------------------------------------------------------------------
CBasePlayer *CRagdollProp::HasPhysicsAttacker_Impl( float dt )
{
	if (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime)
	{
		return m_hPhysicsAttacker;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision_Impl( index, pEvent );

	CBaseEntity *pHitEntity = pEvent->pEntities[!index];
	if ( pHitEntity == this )
		return;

	// Don't take physics damage from whoever's holding him with the physcannon.
	if ( VPhysicsGetObject() && (VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
	{
		if ( pHitEntity && (pHitEntity == HasPhysicsAttacker( std::numeric_limits<float>::max() )) )
			return;
	}

	// Don't bother taking damage from the physics attacker
	if ( pHitEntity && HasPhysicsAttacker( 0.5f ) == pHitEntity )
		return;

	if( m_bFirstCollisionAfterLaunch )
	{
		HandleFirstCollisionInteractions( index, pEvent );
	}
	
	if ( m_takedamage != DAMAGE_NO )
	{
		int damageType = 0;
		float damage = CalculateDefaultPhysicsDamage( index, pEvent, 1.0f, true, damageType );
		if ( damage > 0 )
		{
			// Take extra damage after we're punted by the physcannon
			if ( m_bFirstCollisionAfterLaunch )
			{
				damage *= 10;
			}

			CBaseEntity *pHitEntity = pEvent->pEntities[!index];
			if ( !pHitEntity )
			{
				// hit world
				pHitEntity = GetContainingEntity( INDEXENT(0) );
			}
			Vector damagePos;
			pEvent->pInternalData->GetContactPoint( damagePos );
			Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
			if ( damageForce == vec3_origin )
			{
				// This can happen if this entity is motion disabled, and can't move.
				// Use the velocity of the entity that hit us instead.
				damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
			}

			// FIXME: this doesn't pass in who is responsible if some other entity "caused" this collision
			PhysCallbackDamage( this, CTakeDamageInfo( pHitEntity, pHitEntity, damageForce, damagePos, damage, damageType ), *pEvent, index );
		}
	}

	if ( m_bFirstCollisionAfterLaunch )
	{
		// Setup the think function to remove the flags
		SetThink( &CRagdollProp::ClearFlagsThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CRagdollProp::HasPhysgunInteraction( const char *pszKeyName, const char *pszValue )
{
	KeyValues *modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		KeyValues *pkvPropData = modelKeyValues->FindKey("physgun_interactions");
		if ( pkvPropData )
		{
			char const *pszBase = pkvPropData->GetString( pszKeyName );

			if ( pszBase && pszBase[0] && !stricmp( pszBase, pszValue ) )
			{
				modelKeyValues->deleteThis();
				return true;
			}
		}
	}

	modelKeyValues->deleteThis();
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRagdollProp::HandleFirstCollisionInteractions( int index, gamevcollisionevent_t *pEvent )
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( !pObj)
		return;

	if( HasPhysgunInteraction( "onfirstimpact", "break" ) )
	{
		// Looks like it's best to break by having the object damage itself. 
		CTakeDamageInfo info;

		info.SetDamage( m_iHealth );
		info.SetAttacker( this );
		info.SetInflictor( this );
		info.SetDamageType( DMG_GENERIC );

		Vector vecPosition;
		Vector vecVelocity;

		VPhysicsGetObject()->GetVelocity( &vecVelocity, nullptr );
		VPhysicsGetObject()->GetPosition( &vecPosition, nullptr );

		info.SetDamageForce( vecVelocity );
		info.SetDamagePosition( vecPosition );

		TakeDamage( info );
		return;
	}

	if( HasPhysgunInteraction( "onfirstimpact", "paintsplat" ) )
	{
		IPhysicsObject *pObj = VPhysicsGetObject();
 
		Vector vecPos;
		pObj->GetPosition( &vecPos, nullptr );
 
		trace_t tr;
		UTIL_TraceLine( vecPos, vecPos + pEvent->preVelocity[0] * 1.5, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		switch( random_valve->RandomInt( 1, 3 ) )
		{
		case 1:
			UTIL_DecalTrace( &tr, "PaintSplatBlue" );
			break;

		case 2:
			UTIL_DecalTrace( &tr, "PaintSplatGreen" );
			break;

		case 3:
			UTIL_DecalTrace( &tr, "PaintSplatPink" );
			break;
		}
	}

	bool bAlienBloodSplat = HasPhysgunInteraction( "onfirstimpact", "alienbloodsplat" );
	if( bAlienBloodSplat || HasPhysgunInteraction( "onfirstimpact", "bloodsplat" ) )
	{
		IPhysicsObject *pObj = VPhysicsGetObject();
 
		Vector vecPos;
		pObj->GetPosition( &vecPos, nullptr );
 
		trace_t tr;
		UTIL_TraceLine( vecPos, vecPos + pEvent->preVelocity[0] * 1.5, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		UTIL_BloodDecalTrace( &tr, bAlienBloodSplat ? BLOOD_COLOR_GREEN : BLOOD_COLOR_RED );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollProp::ClearFlagsThink( void )
{
	PhysClearGameFlags( VPhysicsGetObject(), FVPHYSICS_WAS_THROWN );
	m_bFirstCollisionAfterLaunch = false;
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AngularImpulse CRagdollProp::PhysGunLaunchAngularImpulse()
{
	if( HasPhysgunInteraction( "onlaunch", "spin_zaxis" ) )
	{
		// Don't add in random angular impulse if this object is supposed to spin in a specific way.
		AngularImpulse ang( 0, 0, 0 );
		return ang;
	}

	return CDefaultPlayerPickupVPhysics::PhysGunLaunchAngularImpulse();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
//-----------------------------------------------------------------------------
void CRagdollProp::SetOverlaySequence( Activity activity )
{
	int seq = SelectWeightedSequence( activity );
	if ( seq < 0 )
	{
		m_nOverlaySequence = -1;
	}
	else
	{
		m_nOverlaySequence = seq;
	}
}

void CRagdollProp::InitRagdoll( const Vector &forceVector, int forceBone, const Vector &forcePos, matrix3x4_t *pPrevBones, matrix3x4_t *pBoneToWorld, float dt, int collisionGroup, bool activateRagdoll, bool bWakeRagdoll )
{
	SetCollisionGroup( collisionGroup );

	// Make sure it's interactive debris for at most 5 seconds
	if ( collisionGroup == COLLISION_GROUP_INTERACTIVE_DEBRIS )
	{
		SetContextThink( &CRagdollProp::SetDebrisThink, gpGlobals->curtime + 5, s_pDebrisContext );
	}

	SetMoveType( MOVETYPE_VPHYSICS );
	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );
	m_takedamage = DAMAGE_EVENTS_ONLY;

	ragdollparams_t params;
	params.pGameData = static_cast<void *>( static_cast<CBaseEntity *>(this) );
	params.modelIndex = GetModelIndex();
	params.pCollide = modelinfo->GetVCollide( params.modelIndex );
	params.pStudioHdr = GetModelPtr();
	params.forceVector = forceVector;
	params.forceBoneIndex = forceBone;
	params.forcePosition = forcePos;
	params.pCurrentBones = pBoneToWorld;
	params.jointFrictionScale = 1.0;
	params.allowStretch = HasSpawnFlags(SF_RAGDOLLPROP_ALLOW_STRETCH);
	params.fixedConstraints = false;
	RagdollCreate( m_ragdoll, params, physenv );
	RagdollApplyAnimationAsVelocity( m_ragdoll, pPrevBones, pBoneToWorld, dt );
	if ( m_anglesOverrideString != NULL_STRING && V_strlen(m_anglesOverrideString.ToCStr()) > 0 )
	{
		char szToken[2048];
		const char *pStr = nexttoken(szToken, STRING(m_anglesOverrideString), ',');
		// anglesOverride is index,angles,index,angles (e.g. "1, 22.5 123.0 0.0, 2, 0 0 0, 3, 0 0 180.0")
		while ( szToken[0] != 0 )
		{
			int objectIndex = std::atoi(szToken);
			// sanity check to make sure this token is an integer
			contract_assert( std::atof(szToken) == ((float)objectIndex) );
			pStr = nexttoken(szToken, pStr, ',');
			contract_assert( szToken[0] );
			if ( objectIndex >= m_ragdoll.listCount )
			{
				Warning("Bad ragdoll pose in entity %s, model (%s) at %s, model changed?\n", GetDebugName(), GetModelName().ToCStr(), VecToString(GetAbsOrigin()).c_str() );
			}
			else if ( szToken[0] != 0 )
			{
				QAngle angles;
				contract_assert( objectIndex >= 0 && objectIndex < RAGDOLL_MAX_ELEMENTS );
				UTIL_StringToVector( angles.Base(), szToken );
				int boneIndex = m_ragdoll.boneIndex[objectIndex];
				AngleMatrix( angles, pBoneToWorld[boneIndex] );
				const ragdollelement_t &element = m_ragdoll.list[objectIndex];
				Vector out;
				if ( element.parentIndex >= 0 )
				{
					int parentBoneIndex = m_ragdoll.boneIndex[element.parentIndex];
					VectorTransform( element.originParentSpace, pBoneToWorld[parentBoneIndex], out );
				}
				else
				{
					out = GetAbsOrigin();
				}
				MatrixSetColumn( out, 3, pBoneToWorld[boneIndex] );
				element.pObject->SetPositionMatrix( pBoneToWorld[boneIndex], true );
			}
			pStr = nexttoken(szToken, pStr, ',');
		}
	}

	if ( activateRagdoll )
	{
		RagdollActivate( m_ragdoll, params.pCollide, GetModelIndex(), bWakeRagdoll );
	}

	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		UpdateNetworkDataFromVPhysics( m_ragdoll.list[i].pObject, i );
		physcollision->CollideGetAABB( &m_ragdollMins[i], &m_ragdollMaxs[i], m_ragdoll.list[i].pObject->GetCollide(), vec3_origin, vec3_angle );
	}
	VPhysicsSetObject( m_ragdoll.list[0].pObject );

	CalcRagdollSize();
}

void CRagdollProp::SetDebrisThink()
{
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	RecheckCollisionFilter();
}

void CRagdollProp::SetDamageEntity( CBaseEntity *pEntity )
{
	// Damage passing
	m_hDamageEntity = pEntity;

	// Set our takedamage to match it
	if ( pEntity )
	{
		m_takedamage = pEntity->m_takedamage;
	}
	else
	{
		m_takedamage = DAMAGE_EVENTS_ONLY;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CRagdollProp::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	// If we have a damage entity, we want to pass damage to it. Add the
	// Never Ragdoll flag, on the assumption that if the entity dies, we'll
	// actually be taking the role of its ragdoll.
	if ( m_hDamageEntity.Get() )
	{
		CTakeDamageInfo subInfo = info;
		subInfo.AddDamageType( DMG_REMOVENORAGDOLL );
		return m_hDamageEntity->OnTakeDamage( subInfo );
	}

	return BaseClass::OnTakeDamage_Impl( info );
}

//-----------------------------------------------------------------------------
// Purpose: Force all the ragdoll's bone's physics objects to recheck their collision filters
//-----------------------------------------------------------------------------
void CRagdollProp::RecheckCollisionFilter( void )
{
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		m_ragdoll.list[i].pObject->RecheckCollisionFilter();
	}
}

void CRagdollProp::TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &dir, trace_t *ptr )
{
	if ( ptr->physicsbone >= 0 && ptr->physicsbone < m_ragdoll.listCount )
	{
		VPhysicsSwapObject( m_ragdoll.list[ptr->physicsbone].pObject );
	}
	BaseClass::TraceAttack_Impl( info, dir, ptr );
}

void CRagdollProp::SetupBones( matrix3x4a_t *pBoneToWorld, int boneMask )
{
	// no ragdoll, fall through to base class 
	if ( !m_ragdoll.listCount )
	{
		BaseClass::SetupBones( pBoneToWorld, boneMask );
		return;
	}

	// Not really ideal, but it'll work for now
	UpdateModelScale();

	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *pStudioHdr = GetModelPtr( );
	bool sim[MAXSTUDIOBONES];
	std::memset( sim, 0, pStudioHdr->numbones() );

	int i;

	CBoneAccessor boneaccessor( pBoneToWorld );
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		// during restore this may be NULL
		if ( !m_ragdoll.list[i].pObject )
			continue;

		if ( RagdollGetBoneMatrix( m_ragdoll, boneaccessor, i ) )
		{
			sim[m_ragdoll.boneIndex[i]] = true;
		}
	}

	const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	for ( i = 0; i < pStudioHdr->numbones(); i++ )
	{
		if ( sim[i] )
			continue;
		
		if ( !(pStudioHdr->boneFlags(i) & boneMask) )
			continue;

		matrix3x4_t matBoneLocal;
		AngleMatrix( pbones[i].rot, pbones[i].pos, matBoneLocal );
		ConcatTransforms( pBoneToWorld[pbones[i].parent], matBoneLocal, pBoneToWorld[i]);
	}
}

bool CRagdollProp::TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace )
{

	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *pStudioHdr = GetModelPtr( );
	if (!pStudioHdr)
		return false;

	// Just iterate all of the elements and trace the box against each one.
	// NOTE: This is pretty expensive for small/dense characters
	trace_t tr;
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		Vector position;
		QAngle angles;

		if( m_ragdoll.list[i].pObject )
		{
			m_ragdoll.list[i].pObject->GetPosition( &position, &angles );
			physcollision->TraceBox( ray, m_ragdoll.list[i].pObject->GetCollide(), position, angles, &tr );

			if ( tr.fraction < trace.fraction )
			{
				tr.physicsbone = i;
				tr.surface.surfaceProps = m_ragdoll.list[i].pObject->GetMaterialIndex();
				trace = tr;
			}
		}
		else
		{
			DevWarning("Bogus object in Ragdoll Prop's ragdoll list!\n");
		}
	}

	if ( trace.fraction >= 1 )
	{
		return false;
	}

	return true;
}

void CRagdollProp::Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts )
{
	// newAngles is a relative transform for the entity
	// But a ragdoll entity has identity orientation by design
	// so we compute a relative transform here based on the previous transform
	matrix3x4_t startMatrixInv;
	MatrixInvert( EntityToWorldTransform(), startMatrixInv );
	matrix3x4_t endMatrix;
	MatrixCopy( EntityToWorldTransform(), endMatrix );
	if ( newAngles )
	{
		AngleMatrix( *newAngles, endMatrix );
	}
	if ( newPosition )
	{
		PositionMatrix( *newPosition, endMatrix );
	}
	// now endMatrix is the refernce matrix for the entity at the target position
	matrix3x4_t xform;
	ConcatTransforms( endMatrix, startMatrixInv, xform );
	// now xform is the relative transform the entity must undergo

	// we need to call the base class and it will teleport our vphysics object, 
	// so set object 0 up and compute the origin/angles for its new position (base implementation has side effects)
	VPhysicsSwapObject( m_ragdoll.list[0].pObject );
	matrix3x4_t obj0source, obj0Target;
	m_ragdoll.list[0].pObject->GetPositionMatrix( &obj0source );
	ConcatTransforms( xform, obj0source, obj0Target );
	Vector obj0Pos;
	QAngle obj0Angles;
	MatrixAngles( obj0Target, obj0Angles, obj0Pos );
	BaseClass::Teleport_Impl( &obj0Pos, &obj0Angles, newVelocity, bUseSlowHighAccuracyContacts );
	
	for ( int i = 1; i < m_ragdoll.listCount; i++ )
	{
		matrix3x4_t matrix, newMatrix;
		m_ragdoll.list[i].pObject->GetPositionMatrix( &matrix );
		ConcatTransforms( xform, matrix, newMatrix );
		m_ragdoll.list[i].pObject->SetPositionMatrix( newMatrix, true );
		UpdateNetworkDataFromVPhysics( m_ragdoll.list[i].pObject, i );
	}
	// fixup/relink object 0
	UpdateNetworkDataFromVPhysics( m_ragdoll.list[0].pObject, 0 );
}

void CRagdollProp::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	if ( m_lastUpdateTickCount == (unsigned int)gpGlobals->tickcount )
		return;

	m_lastUpdateTickCount = gpGlobals->tickcount;
	//NetworkStateChanged();

	matrix3x4a_t boneToWorld[MAXSTUDIOBONES];
	QAngle angles;
	Vector surroundingMins, surroundingMaxs;

	int i;
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		CBoneAccessor boneaccessor( boneToWorld );
		if ( RagdollGetBoneMatrix( m_ragdoll, boneaccessor, i ) )
		{
			Vector vNewPos;
			MatrixAngles( boneToWorld[m_ragdoll.boneIndex[i]], angles, vNewPos );
			m_ragPos.Set( i, vNewPos );
			m_ragAngles.Set( i, angles );
		}
		else
		{
			m_ragPos.GetForModify(i).Init();
			m_ragAngles.GetForModify(i).Init();
		}
	}

	// BUGBUG: Use the ragdollmins/maxs to do this instead of the collides
	m_allAsleep = RagdollIsAsleep( m_ragdoll );

	// Don't scream after you've come to rest
	if ( m_allAsleep )
	{
		m_strSourceClassName = NULL_STRING;
	}
	else
	{
		if ( m_ragdoll.pGroup->IsInErrorState() )
		{
			RagdollSolveSeparation( m_ragdoll, this );
		}
	}
	
	// Interactive debris converts back to debris when it comes to rest
	if ( m_allAsleep && GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS )
	{
		SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		RecheckCollisionFilter();
		SetContextThink( nullptr, gpGlobals->curtime, s_pDebrisContext );
	}

	Vector vecFullMins, vecFullMaxs;
	vecFullMins = m_ragPos[0];
	vecFullMaxs = m_ragPos[0];
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		Vector mins, maxs;
		matrix3x4_t update;
		if ( !m_ragdoll.list[i].pObject )
		{
			m_ragdollMins[i].Init();
			m_ragdollMaxs[i].Init();
			continue;
		}
		m_ragdoll.list[i].pObject->GetPositionMatrix( &update );
		TransformAABB( update, m_ragdollMins[i], m_ragdollMaxs[i], mins, maxs );
		for ( int j = 0; j < 3; j++ )
		{
			if ( mins[j] < vecFullMins[j] )
			{
				vecFullMins[j] = mins[j];
			}
			if ( maxs[j] > vecFullMaxs[j] )
			{
				vecFullMaxs[j] = maxs[j];
			}
		}
	}

	SetAbsOrigin( m_ragPos[0] );
	SetAbsAngles( vec3_angle );
	const Vector &vecOrigin = CollisionProp()->GetCollisionOrigin();
	CollisionProp()->AddSolidFlags( FSOLID_FORCE_WORLD_ALIGNED );
	CollisionProp()->SetSurroundingBoundsType( USE_COLLISION_BOUNDS_NEVER_VPHYSICS );
	SetCollisionBounds( vecFullMins - vecOrigin, vecFullMaxs - vecOrigin );
	CollisionProp()->MarkSurroundingBoundsDirty();

	PhysicsTouchTriggers();
}

int CRagdollProp::VPhysicsGetObjectList_Impl( IPhysicsObject **pList, int listMax )
{
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		if ( i < listMax )
		{
			pList[i] = m_ragdoll.list[i].pObject;
		}
	}

	return m_ragdoll.listCount;
}

void CRagdollProp::UpdateNetworkDataFromVPhysics( IPhysicsObject *pPhysics, int index )
{
	contract_assert( index < m_ragdoll.listCount );

	QAngle angles;
	Vector vPos;
	m_ragdoll.list[index].pObject->GetPosition( &vPos, &angles );
	m_ragPos.Set( index, vPos );
	m_ragAngles.Set( index, angles );

	// move/relink if root moved
	if ( index == 0 )
	{
		SetAbsOrigin( m_ragPos[0] );
		PhysicsTouchTriggers();
	}
}

//-----------------------------------------------------------------------------
// Fade out due to the LRU telling it do
//-----------------------------------------------------------------------------
static constexpr auto FADE_OUT_LENGTH = 0.5f;

void CRagdollProp::FadeOut( float flDelay, float fadeTime ) 
{
	if ( IsFading() )
		return;

	m_flFadeTime = ( fadeTime == -1 ) ? FADE_OUT_LENGTH : fadeTime;

	m_flFadeOutStartTime = gpGlobals->curtime + flDelay;
	SetGlobalFadeScale( 0 );
	SetContextThink( &CRagdollProp::FadeOutThink, gpGlobals->curtime + flDelay + 0.01f, s_pFadeOutContext );
}

bool CRagdollProp::IsFading()
{
	return ( GetNextThink( s_pFadeOutContext ) >= gpGlobals->curtime );
}

void CRagdollProp::FadeOutThink(void) 
{
	float dt = gpGlobals->curtime - m_flFadeOutStartTime;
	if ( dt < 0 )
	{
		SetContextThink( &CRagdollProp::FadeOutThink, gpGlobals->curtime + 0.1, s_pFadeOutContext );
	}
	else if ( dt < m_flFadeTime )
	{
		float alpha = 1.0f - dt / m_flFadeTime;
		int nFade = (int)(alpha * 255.0f);
		m_nRenderMode = kRenderTransTexture;
		SetRenderAlpha( nFade );
		NetworkStateChanged();
		SetContextThink( &CRagdollProp::FadeOutThink, gpGlobals->curtime + TICK_INTERVAL, s_pFadeOutContext );
	}
	else
	{
		// Necessary to cause it to do the appropriate death cleanup
		// Yeah, the player may have nothing to do with it, but
		// passing NULL to TakeDamage causes bad things to happen
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		CTakeDamageInfo info( pPlayer, pPlayer, 10000.0, DMG_GENERIC );
		TakeDamage( info );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CRagdollProp::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if (m_ragdoll.listCount)
		{
			float mass = 0;
			for ( int i = 0; i < m_ragdoll.listCount; i++ )
			{
				if ( m_ragdoll.list[i].pObject != nullptr )
				{
					mass += m_ragdoll.list[i].pObject->GetMass();
				}
			}

			char tempstr[512];
			V_snprintf(tempstr, sizeof(tempstr),"Mass: %.2f kg / %.2f lb (%s)", mass, kg2lbs(mass), GetMassEquivalent(mass) );
			EntityText( text_offset, tempstr, 0);
			text_offset++;
		}
	}

	return text_offset;
}

void CRagdollProp::DrawDebugGeometryOverlays_Impl() 
{
	if (m_debugOverlays & OVERLAY_BBOX_BIT) 
	{
		DrawServerHitboxes();
	}
	if (m_debugOverlays & OVERLAY_PIVOT_BIT)
	{
		for ( int i = 0; i < m_ragdoll.listCount; i++ )
		{
			if ( m_ragdoll.list[i].pObject )
			{
				float mass = m_ragdoll.list[i].pObject->GetMass();
				Vector pos;
				m_ragdoll.list[i].pObject->GetPosition( &pos, nullptr );
				CFmtStr str("mass %.1f", mass );
				NDebugOverlay::EntityTextAtPosition( pos, 0, str.Access(), 0, 0, 255, 0, 255 );
			}
		}
	} 
	BaseClass::DrawDebugGeometryOverlays_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CRagdollProp::SetUnragdoll( CBaseAnimating *pOther )
{
	m_hUnragdoll = pOther;
}

//===============================================================================================================
// RagdollPropAttached
//===============================================================================================================
class CRagdollPropAttached : public CRagdollProp
{
	[[= ks::reflect::NetTable{ .name = "DT_Ragdoll_Attached" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CRagdollPropAttached, CRagdollProp );
public:

	CRagdollPropAttached()
	{
		m_bShouldDetach = false;
	}

	~CRagdollPropAttached()
	{
		physenv->DestroyConstraint( m_pAttachConstraint );
		m_pAttachConstraint = nullptr;
	}

	void InitRagdollAttached( IPhysicsObject *pAttached, const Vector &forceVector, int forceBone, matrix3x4_t *pPrevBones, matrix3x4_t *pBoneToWorld, float dt, int collisionGroup, CBaseAnimating *pFollow, int boneIndexRoot, const Vector &boneLocalOrigin, int parentBoneAttach, const Vector &worldAttachOrigin );
	void DetachOnNextUpdate();
	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );

	DECLARE_SERVERCLASS();

private:
	void Detach();
	CNetworkVar( int, m_boneIndexAttached, [[= ks::reflect::Net{ .bits = MAXSTUDIOBONEBITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_ragdollAttachedObjectIndex, [[= ks::reflect::Net{ .bits = RAGDOLL_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_attachmentPointBoneSpace, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_attachmentPointRagdollSpace, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	bool		m_bShouldDetach;
	IPhysicsConstraint	*m_pAttachConstraint;
};

LINK_ENTITY_TO_CLASS( prop_ragdoll_attached, CRagdollPropAttached );
EXTERN_SEND_TABLE(DT_Ragdoll_Attached)

IMPLEMENT_REFLECT_SERVERCLASS( CRagdollPropAttached, DT_Ragdoll_Attached )

static void SyncAnimatingWithPhysics( CBaseAnimating *pAnimating )
{
	IPhysicsObject *pPhysics = pAnimating->VPhysicsGetObject();
	if ( pPhysics )
	{
		Vector pos;
		pPhysics->GetShadowPosition( &pos, nullptr );
		pAnimating->SetAbsOrigin( pos );
	}
}

CBaseAnimating *CreateServerRagdollSubmodel( CBaseAnimating *pOwner, const char *pModelName, const Vector &position, const QAngle &angles, int collisionGroup )
{
	CRagdollProp *pRagdoll = (CRagdollProp *)CBaseEntity::CreateNoSpawn( "prop_ragdoll", position, angles, pOwner );
	pRagdoll->SetModelName( AllocPooledString( pModelName ) );
	pRagdoll->SetModel( STRING(pRagdoll->GetModelName()) );
	matrix3x4a_t pBoneToWorld[MAXSTUDIOBONES];
	matrix3x4a_t pBoneToWorldNext[MAXSTUDIOBONES];
	pRagdoll->ResetSequence( 0 );

	// let bone merging do the work of copying everything over for us
	pRagdoll->SetParent( pOwner );
	pRagdoll->SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );
	// HACKHACK: don't want this parent anymore
	pRagdoll->SetParent( nullptr );

	std::memcpy( pBoneToWorldNext, pBoneToWorld, sizeof(pBoneToWorld) );

	pRagdoll->InitRagdoll( vec3_origin, -1, vec3_origin, pBoneToWorld, pBoneToWorldNext, 0.1, collisionGroup, true );
	return pRagdoll;
}

CBaseEntity *CreateServerRagdoll( CBaseAnimating *pAnimating, int forceBone, const CTakeDamageInfo &info, int collisionGroup, bool bUseLRURetirement )
{
	if ( info.GetDamageType() & (DMG_VEHICLE|DMG_CRUSH) )
	{
		// if the entity was killed by physics or a vehicle, move to the vphysics shadow position before creating the ragdoll.
		SyncAnimatingWithPhysics( pAnimating );
	}
	CRagdollProp *pRagdoll = (CRagdollProp *)CBaseEntity::CreateNoSpawn( "prop_ragdoll", pAnimating->GetAbsOrigin(), vec3_angle, nullptr );
	pRagdoll->CopyAnimationDataFrom( pAnimating );
	pRagdoll->SetOwnerEntity( pAnimating );

	pRagdoll->InitRagdollAnimation();
	matrix3x4a_t pBoneToWorld[MAXSTUDIOBONES];
	matrix3x4a_t pBoneToWorldNext[MAXSTUDIOBONES];
	
	float dt = 0.1f;

	// Copy over dissolve state...
	if ( pAnimating->IsEFlagSet( EFL_NO_DISSOLVE ) )
	{
		pRagdoll->AddEFlags( EFL_NO_DISSOLVE );
	}

	// NOTE: This currently is only necessary to prevent manhacks from
	// colliding with server ragdolls they kill
	pRagdoll->SetKiller( info.GetInflictor() );
	pRagdoll->SetSourceClassName( pAnimating->GetClassname() );

	// NPC_STATE_DEAD npc's will have their COND_IN_PVS cleared, so this needs to force SetupBones to happen
	unsigned short fPrevFlags = pAnimating->GetBoneCacheFlags();
	pAnimating->SetBoneCacheFlags( BCF_NO_ANIMATION_SKIP );

	// UNDONE: Extract velocity from bones via animation (like we do on the client)
	// UNDONE: For now, just move each bone by the total entity velocity if set.
	// Get Bones positions before
	// Store current cycle
	float fSequenceDuration = pAnimating->SequenceDuration( pAnimating->GetSequence() );
	float fSequenceTime = pAnimating->GetCycle() * fSequenceDuration;		

	if( fSequenceTime <= dt && fSequenceTime > 0.0f )
	{
		// Avoid having negative cycle
		dt = fSequenceTime;
	}

	float fPreviousCycle = clamp(pAnimating->GetCycle()-( dt * ( 1 / fSequenceDuration ) ),0.f,1.f);
	float fCurCycle = pAnimating->GetCycle();
	// Get current bones positions
	pAnimating->SetupBones( pBoneToWorldNext, BONE_USED_BY_ANYTHING );
	// Get previous bones positions
	pAnimating->SetCycle( fPreviousCycle );
	pAnimating->SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );		
	// Restore current cycle
	pAnimating->SetCycle( fCurCycle );

	// Reset previous bone flags
	pAnimating->ClearBoneCacheFlags( BCF_NO_ANIMATION_SKIP );
	pAnimating->SetBoneCacheFlags( fPrevFlags );

	Vector vel = pAnimating->GetAbsVelocity();
	if( ( vel.Length() == 0 ) && ( dt > 0 ) )
	{
		// Compute animation velocity
		CStudioHdr *pstudiohdr = pAnimating->GetModelPtr();
		if ( pstudiohdr )
		{
			Vector deltaPos;
			QAngle deltaAngles;
			if (Studio_SeqMovement( pstudiohdr, 
				pAnimating->GetSequence(), 
				fPreviousCycle, 
				pAnimating->GetCycle(), 
				pAnimating->GetPoseParameterArray(), 
				deltaPos, 
				deltaAngles ))
			{
				VectorRotate( deltaPos, pAnimating->EntityToWorldTransform(), vel );
				vel /= dt;
			}
		}
	}

	if ( vel.LengthSqr() > 0 )
	{
		int numbones = pAnimating->GetModelPtr()->numbones();
		vel *= dt;
		for ( int i = 0; i < numbones; i++ )
		{
			Vector pos;
			MatrixGetColumn( pBoneToWorld[i], 3, pos );
			pos -= vel;
			MatrixSetColumn( pos, 3, pBoneToWorld[i] );
		}
	}

#if RAGDOLL_VISUALIZE
	pAnimating->DrawRawSkeleton( pBoneToWorld, BONE_USED_BY_ANYTHING, true, 20, false );
	pAnimating->DrawRawSkeleton( pBoneToWorldNext, BONE_USED_BY_ANYTHING, true, 20, true );
#endif
	// Is this a vehicle / NPC collision?
	{
		pRagdoll->InitRagdoll( info.GetDamageForce(), forceBone, info.GetDamagePosition(), pBoneToWorld, pBoneToWorldNext, dt, collisionGroup, true );
	}

	// Are we dissolving?
	if ( pAnimating->IsDissolving() )
	{
		pRagdoll->TransferDissolveFrom( pAnimating );
	}
	else if ( bUseLRURetirement )
	{
		pRagdoll->AddSpawnFlags( SF_RAGDOLLPROP_USE_LRU_RETIREMENT );
		s_RagdollLRU.MoveToTopOfLRU( pRagdoll );
	}

	// Tracker 22598:  If we don't set the OBB mins/maxs to something valid here, then the client will have a zero sized hull
	//  for the ragdoll for one frame until Vphysics updates the real obb bounds after the first simulation frame.  Having
	//  a zero sized hull makes the ragdoll think it should be faded/alpha'd to zero for a frame, so you get a blink where
	//  the ragdoll doesn't draw initially.
	Vector mins, maxs;
	mins = pAnimating->CollisionProp()->OBBMins();
	maxs = pAnimating->CollisionProp()->OBBMaxs();
	pRagdoll->CollisionProp()->SetCollisionBounds( mins, maxs );

	return pRagdoll;
}

void CRagdollPropAttached::DetachOnNextUpdate()
{
	m_bShouldDetach = true;
}

void CRagdollPropAttached::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	if ( m_bShouldDetach )
	{
		Detach();
		m_bShouldDetach = false;
	}
	BaseClass::VPhysicsUpdate_Impl( pPhysics );
}

void CRagdollPropAttached::Detach()
{
	SetParent(nullptr);
	SetOwnerEntity( nullptr );
	SetAbsAngles( vec3_angle );
	SetMoveType( MOVETYPE_VPHYSICS );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	physenv->DestroyConstraint( m_pAttachConstraint );
	m_pAttachConstraint = nullptr;
	const float dampingScale = 1.0f / ATTACHED_DAMPING_SCALE;
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		float damping, rotdamping;
		m_ragdoll.list[i].pObject->GetDamping( &damping, &rotdamping );
		damping *= dampingScale;
		rotdamping *= dampingScale;
		m_ragdoll.list[i].pObject->SetDamping( &damping, &damping );
	}

	// Go non-solid
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	RecheckCollisionFilter();
}

void CRagdollPropAttached::InitRagdollAttached( 
	IPhysicsObject *pAttached, 
	const Vector &forceVector, 
	int forceBone, 
	matrix3x4_t *pPrevBones, 
	matrix3x4_t *pBoneToWorld, 
	float dt, 
	int collisionGroup, 
	CBaseAnimating *pFollow, 
	int boneIndexRoot, 
	const Vector &boneLocalOrigin, 
	int parentBoneAttach, 
	const Vector &worldAttachOrigin )
{
	int ragdollAttachedIndex = 0;
	if ( parentBoneAttach > 0 )
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		const mstudiobone_t *pBone = pStudioHdr->pBone( parentBoneAttach );
		ragdollAttachedIndex = pBone->physicsbone;
	}

	InitRagdoll( forceVector, forceBone, vec3_origin, pPrevBones, pBoneToWorld, dt, collisionGroup, false );
	
	IPhysicsObject *pRefObject = m_ragdoll.list[ragdollAttachedIndex].pObject;

	Vector attachmentPointRagdollSpace;
	pRefObject->WorldToLocal( &attachmentPointRagdollSpace, worldAttachOrigin );

	constraint_ragdollparams_t constraint;
	constraint.Defaults();
	matrix3x4_t tmp, worldToAttached, worldToReference, constraintToWorld;

	Vector offsetWS;
	pAttached->LocalToWorld( &offsetWS, boneLocalOrigin );

	QAngle followAng = QAngle(0, pFollow->GetAbsAngles().y, 0 );
	AngleMatrix( followAng, offsetWS, constraintToWorld );

	constraint.axes[0].SetAxisFriction( -2, 2, 20 );
	constraint.axes[1].SetAxisFriction( 0, 0, 0 );
	constraint.axes[2].SetAxisFriction( -15, 15, 20 );

	// Exaggerate the bone's ability to pull the mass of the ragdoll around
	constraint.constraint.bodyMassScale[1] = 50.0f;

	pAttached->GetPositionMatrix( &tmp );
	MatrixInvert( tmp, worldToAttached );

	pRefObject->GetPositionMatrix( &tmp );
	MatrixInvert( tmp, worldToReference );

	ConcatTransforms( worldToReference, constraintToWorld, constraint.constraintToReference );
	ConcatTransforms( worldToAttached, constraintToWorld, constraint.constraintToAttached );

	// for now, just slam this to be the passed in value
	MatrixSetColumn( attachmentPointRagdollSpace, 3, constraint.constraintToReference );

	PhysDisableEntityCollisions( pAttached, m_ragdoll.list[0].pObject );
	m_pAttachConstraint = physenv->CreateRagdollConstraint( pRefObject, pAttached, m_ragdoll.pGroup, constraint );

	SetParent( pFollow );
	SetOwnerEntity( pFollow );

	RagdollActivate( m_ragdoll, modelinfo->GetVCollide( GetModelIndex() ), GetModelIndex() );

	// add a bunch of dampening to the ragdoll
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		float damping, rotdamping;
		m_ragdoll.list[i].pObject->GetDamping( &damping, &rotdamping );
		damping *= ATTACHED_DAMPING_SCALE;
		rotdamping *= ATTACHED_DAMPING_SCALE;
		m_ragdoll.list[i].pObject->SetDamping( &damping, &rotdamping );
	}

	m_boneIndexAttached = boneIndexRoot;
	m_ragdollAttachedObjectIndex = ragdollAttachedIndex;
	m_attachmentPointBoneSpace = boneLocalOrigin;
	
	Vector vTemp;
	MatrixGetColumn( constraint.constraintToReference, 3, vTemp );
	m_attachmentPointRagdollSpace = vTemp;
}

CRagdollProp *CreateServerRagdollAttached( CBaseAnimating *pAnimating, const Vector &vecForce, int forceBone, int collisionGroup, IPhysicsObject *pAttached, CBaseAnimating *pParentEntity, int boneAttach, const Vector &originAttached, int parentBoneAttach, const Vector &boneOrigin )
{
	// Return immediately if the model doesn't have a vcollide
	if ( modelinfo->GetVCollide( pAnimating->GetModelIndex() ) == nullptr )
		return nullptr;

	CRagdollPropAttached *pRagdoll = (CRagdollPropAttached *)CBaseEntity::CreateNoSpawn( "prop_ragdoll_attached", pAnimating->GetAbsOrigin(), vec3_angle, nullptr );
	pRagdoll->CopyAnimationDataFrom( pAnimating );

	pRagdoll->InitRagdollAnimation();
	matrix3x4a_t pBoneToWorld[MAXSTUDIOBONES];
	pAnimating->SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );
	pRagdoll->InitRagdollAttached( pAttached, vecForce, forceBone, pBoneToWorld, pBoneToWorld, 0.1, collisionGroup, pParentEntity, boneAttach, boneOrigin, parentBoneAttach, originAttached );
	
	return pRagdoll;
}

void DetachAttachedRagdoll( CBaseEntity *pRagdollIn )
{
	CRagdollPropAttached *pRagdoll = dynamic_cast<CRagdollPropAttached *>(pRagdollIn);

	if ( pRagdoll )
	{
		pRagdoll->DetachOnNextUpdate();
	}
}

void DetachAttachedRagdollsForEntity( CBaseEntity *pRagdollParent )
{
	std::vector< CBaseEntity * > list;
	GetAllChildren( pRagdollParent, list );
	for ( int i = std::ssize( list )-1; i >= 0; --i )
	{
		DetachAttachedRagdoll( list[i] );
	}
}

bool Ragdoll_IsPropRagdoll( CBaseEntity *pEntity )
{
	if ( dynamic_cast<CRagdollProp *>(pEntity) != nullptr )
		return true;
	return false;
}

ragdoll_t *Ragdoll_GetRagdoll( CBaseEntity *pEntity )
{
	CRagdollProp *pProp = dynamic_cast<CRagdollProp *>(pEntity);
	if ( pProp )
		return pProp->GetRagdoll();
	return nullptr;
}

void CRagdollProp::GetAngleOverrideFromCurrentState( char *pOut, int size )
{
	pOut[0] = 0;
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		if ( i != 0 )
		{
			V_strncat( pOut, ",", size, COPY_ALL_CHARACTERS );

		}
		CFmtStr str("%d,%.2f %.2f %.2f", i, m_ragAngles[i].x, m_ragAngles[i].y, m_ragAngles[i].z );
		V_strncat( pOut, str, size, COPY_ALL_CHARACTERS );
	}
}

void CRagdollProp::DisableMotion( void )
{
	for ( int iRagdoll = 0; iRagdoll < m_ragdoll.listCount; ++iRagdoll )
	{
		IPhysicsObject *pPhysicsObject = m_ragdoll.list[ iRagdoll ].pObject;
		if ( pPhysicsObject != nullptr )
		{
			pPhysicsObject->EnableMotion( false );
		}
	}
}

void CRagdollProp::InputStartRadgollBoogie( inputdata_t &inputdata )
{
	float duration = inputdata.value.Float();

	if( duration <= 0.0f )
	{
		duration = 5.0f;
	}

	CRagdollBoogie::Create( this, 100, gpGlobals->curtime, duration, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Enable physics motion and collision response (on by default)
//-----------------------------------------------------------------------------
void CRagdollProp::InputEnableMotion( inputdata_t &inputdata )
{
	for ( int iRagdoll = 0; iRagdoll < m_ragdoll.listCount; ++iRagdoll )
	{
		IPhysicsObject *pPhysicsObject = m_ragdoll.list[ iRagdoll ].pObject;
		if ( pPhysicsObject != nullptr )
		{
			pPhysicsObject->EnableMotion( true );
			pPhysicsObject->Wake();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Disable any physics motion or collision response
//-----------------------------------------------------------------------------
void CRagdollProp::InputDisableMotion( inputdata_t &inputdata )
{
	DisableMotion();
}

void CRagdollProp::InputTurnOn( inputdata_t &inputdata )
{
	RemoveEffects( EF_NODRAW );
}

void CRagdollProp::InputTurnOff( inputdata_t &inputdata )
{
	AddEffects( EF_NODRAW );
}

void CRagdollProp::InputFadeAndRemove( inputdata_t &inputdata )
{
	float flFadeDuration = inputdata.value.Float();
	
	if( flFadeDuration == 0.0f )
		flFadeDuration = 1.0f;

	FadeOut( 0.0f, flFadeDuration );
}

void Ragdoll_GetAngleOverrideString( char *pOut, int size, CBaseEntity *pEntity )
{
	CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp *>(pEntity);
	if ( pRagdoll )
	{
		pRagdoll->GetAngleOverrideFromCurrentState( pOut, size );
	}
}

#undef RAGDOLL_VISUALIZE

class CBaseAnimating;
class CBaseEntity;
class CTakeDamageInfo;

using namespace ::ks::reflect;

//#include "particle_parse.h"

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( physics_prop_statue, CStatueProp );

IMPLEMENT_REFLECT_SERVERCLASS( CStatueProp, DT_StatueProp )

ConVarRef *s_vcollide_wireframe = nullptr;

CStatueProp::CStatueProp( void )
{
	static ConVarRef vcollide_wireframe( "vcollide_wireframe" );
	s_vcollide_wireframe = &vcollide_wireframe;
	m_pInitOBBs = nullptr;
}

void CStatueProp::Spawn_Impl( void )
{
	// Make it breakable
	SetBreakableModel( MAKE_STRING( "ConcreteChunks" ) );
	SetBreakableCount( 6 );
	SetHealth( 5 );

	BaseClass::Spawn_Impl();

	m_flFrozen = 1.0f;
}

void CStatueProp::Precache_Impl( void )
{
}

bool CStatueProp::CreateVPhysics_Impl( void )
{
	if ( m_pInitOBBs )
	{
		return CreateVPhysicsFromOBBs( m_hInitBaseAnimating );
	}
	else
	{
		if ( !CreateVPhysicsFromHitBoxes( m_hInitBaseAnimating ) )
		{
			// Init model didn't work out, so just use our own
			return CreateVPhysicsFromHitBoxes( this );
		}

		return true;
	}
}

void CStatueProp::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate_Impl( pPhysics );

	if ( s_vcollide_wireframe->GetBool() )
	{
		const CPhysCollide *pCollide = pPhysics->GetCollide();

		Vector vecOrigin;
		QAngle angAngles;

		pPhysics->GetPosition( &vecOrigin, &angAngles );

		if ( pCollide )
		{
			Vector *outVerts;
			int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
			int triCount = vertCount / 3;
			int vert = 0;

			VMatrix tmp = SetupMatrixOrgAngles( vecOrigin, angAngles );
			int i;
			for ( i = 0; i < vertCount; i++ )
			{
				outVerts[i] = tmp.VMul4x3( outVerts[i] );
			}

			for ( i = 0; i < triCount; i++ )
			{
				NDebugOverlay::Line( outVerts[ vert + 0 ], outVerts[ vert + 1 ], 0, 255, 255, false, 0.0f );
				NDebugOverlay::Line( outVerts[ vert + 1 ], outVerts[ vert + 2 ], 0, 255, 255, false, 0.0f );
				NDebugOverlay::Line( outVerts[ vert + 2 ], outVerts[ vert + 0 ], 0, 255, 255, false, 0.0f );
				vert += 3;
			}

			physcollision->DestroyDebugMesh( vertCount, outVerts );
		}
	}
}

void CStatueProp::ComputeWorldSpaceSurroundingBox_Impl( Vector *pMins, Vector *pMaxs )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;

	if ( pBaseAnimating )
	{
		pBaseAnimating->CollisionProp()->WorldSpaceSurroundingBounds( pMins, pMaxs );
		return;
	}

	CollisionProp()->WorldSpaceSurroundingBounds( pMins, pMaxs );
}

bool CStatueProp::TestCollision_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	IPhysicsObject *pPhysObject = VPhysicsGetObject();

	if ( pPhysObject )
	{
		Vector vecPosition;
		QAngle vecAngles;
		pPhysObject->GetPosition( &vecPosition, &vecAngles );
		const CPhysCollide *pScaledCollide = pPhysObject->GetCollide();
		physcollision->TraceBox( ray, pScaledCollide, vecPosition, vecAngles, &tr );

		return tr.DidHit();
	}

	return false;
}

int	CStatueProp::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage_Impl( info );
}

void CStatueProp::Event_Killed_Impl( const CTakeDamageInfo &info )
{
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	if ( pPhysics && !pPhysics->IsMoveable() )
	{
		pPhysics->EnableMotion( true );
		VPhysicsTakeDamage( info );
	}
	
	m_nShatterFlags = 0; // If you have some flags to network for the shatter effect, put them here!
	m_vShatterPosition = info.GetDamagePosition();
	m_vShatterForce = info.GetDamageForce();
	m_bShatter = true;

	// Skip over breaking code!
	//Break( info.GetInflictor(), info );
	//BaseClass::Event_Killed( info );

	// FIXME: Short delay before we actually remove so that the client statue gets a network update before we need it
	// This isn't a reliable way to do this and needs to be rethought.
	AddSolidFlags( FSOLID_NOT_SOLID );

	SetNextThink( gpGlobals->curtime + 0.2f );
	SetThink( &CBaseEntity::SUB_Remove );
}

void CStatueProp::Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay )
{
	// Can't freeze a statue
	TakeDamage( CTakeDamageInfo( pFreezer, pFreezer, 1, DMG_GENERIC ) );
}

void CStatueProp::CollisionPartnerThink( void )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;
	if ( !pBaseAnimating )
	{
		// Our partner died, I have no reason to live!
		UTIL_Remove( this );
	}

	if ( GetHealth() <= 0 )
	{
		// Reset health here in case it was tweaked by the model parse
		SetHealth( 5 );
		m_takedamage = DAMAGE_YES;
	}

	SetNextThink( gpGlobals->curtime + 1.0f );
}

bool CStatueProp::CreateVPhysicsFromHitBoxes( CBaseAnimating *pInitBaseAnimating )
{
	if ( !pInitBaseAnimating )
		return false;

	// Use the current animation sequence and cycle
	CopyAnimationDataFrom( pInitBaseAnimating );

	// Copy over any render color
	color24 colorRender = pInitBaseAnimating->GetRenderColor();
	SetRenderColor( colorRender.r, colorRender.g, colorRender.b );
	SetRenderAlpha( pInitBaseAnimating->GetRenderAlpha() );

	// Get hitbox data
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr )
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set )
		return false;

	Vector position;
	QAngle angles;

	// Make enough pointers to convexes for each hitbox
	CPhysConvex **ppConvex = new CPhysConvex*[ set->numhitboxes ];

	float flTotalVolume = 0.0f;
	float flTotalSurfaceArea = 0.0f;

	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		// Get the hitbox info
		mstudiobbox_t *pbox = set->pHitbox( i );
		GetBonePosition( pbox->bone, position, angles );

		// Accumulate volume and area
		Vector flDimentions = pbox->bbmax - pbox->bbmin;
		flTotalVolume += flDimentions.x * flDimentions.y * flDimentions.z;
		flTotalSurfaceArea += 2.0f * ( flDimentions.x * flDimentions.y + flDimentions.x * flDimentions.z + flDimentions.y * flDimentions.z );

		// Get angled min and max extents
		Vector vecMins, vecMaxs;
		VectorRotate( pbox->bbmin, angles, vecMins );
		VectorRotate( pbox->bbmax, angles, vecMaxs );

		// Get the corners in world space
		Vector vecMinCorner = position + vecMins;
		Vector vecMaxCorner = position + vecMaxs;

		// Get the normals of the hitbox in world space
		Vector vecForward, vecRight, vecUp;
		AngleVectors( angles, &vecForward, &vecRight, &vecUp );
		vecRight = -vecRight;

		// Convert corners and normals to local space
		Vector vecCornerLocal[ 2 ];
		Vector vecNormalLocal[ 3 ];

		matrix3x4_t matToWorld = EntityToWorldTransform();
		VectorITransform( vecMaxCorner, matToWorld, vecCornerLocal[ 0 ] );
		VectorITransform( vecMinCorner, matToWorld, vecCornerLocal[ 1 ] );
		VectorIRotate( vecForward, matToWorld, vecNormalLocal[ 0 ] );
		VectorIRotate( vecRight, matToWorld, vecNormalLocal[ 1 ] );
		VectorIRotate( vecUp, matToWorld, vecNormalLocal[ 2 ] );

		// Create 6 planes from the local oriented hit box data
		float pPlanes[ 4 * 6 ];

		for ( int iPlane = 0; iPlane < 6; ++iPlane )
		{
			int iPlaneMod2 = iPlane % 2;
			int iPlaneDiv2 = iPlane / 2;
			bool bOdd = ( iPlaneMod2 == 1 );

			// Plane Normal
			pPlanes[ iPlane * 4 + 0 ] = vecNormalLocal[ iPlaneDiv2 ].x * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 1 ] = vecNormalLocal[ iPlaneDiv2 ].y * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 2 ] = vecNormalLocal[ iPlaneDiv2 ].z * ( bOdd ? -1.0f : 1.0f );

			// Plane D
			pPlanes[ iPlane * 4 + 3 ] = ( vecCornerLocal[ iPlaneMod2 ].x * vecNormalLocal[ iPlaneDiv2 ].x + 
				vecCornerLocal[ iPlaneMod2 ].y * vecNormalLocal[ iPlaneDiv2 ].y + 
				vecCornerLocal[ iPlaneMod2 ].z * vecNormalLocal[ iPlaneDiv2 ].z ) * ( bOdd ? -1.0f : 1.0f );
		}

		// Create convex from the intersection of these planes
		ppConvex[ i ] = physcollision->ConvexFromPlanes( pPlanes, 6, 0.0f );
	}

	// Make a single collide out of the group of convex boxes
	CPhysCollide *pPhysCollide = physcollision->ConvertConvexToCollide( ppConvex, set->numhitboxes );

	delete[] ppConvex;

	// Create the physics object
	objectparams_t params = g_PhysDefaultObjectParams;
	params.pGameData = static_cast<void *>( this );

	int nMaterialIndex = physprops->GetSurfaceIndex( "ice" );	// use ice material

	IPhysicsObject* p = physenv->CreatePolyObject( pPhysCollide, nMaterialIndex, GetAbsOrigin(), GetAbsAngles(), &params );
	contract_assert( p != nullptr );

	// Set velocity
	Vector vecInitialVelocity = pInitBaseAnimating->GetAbsVelocity();
	p->SetVelocity( &vecInitialVelocity, nullptr );

	// Compute mass
	float flMass;
	float flDensity, flThickness;
	physprops->GetPhysicsProperties( nMaterialIndex, &flDensity, &flThickness, nullptr, nullptr );

	// Make it more hollow
	flThickness = std::min( 1.0f, flThickness + 0.5f );

	if ( flThickness > 0.0f )
	{
		flMass = flTotalSurfaceArea * flThickness * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}
	else
	{
		// density is in kg/m^3, volume is in in^3
		flMass = flTotalVolume * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}

	// Mass is somewhere between the original and if it was all ice
	p->SetMass( flMass );

	// Yes, gravity
	p->EnableGravity( true );

	// Use this as our vphysics
	VPhysicsSetObject( p );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	SetMoveType( MOVETYPE_VPHYSICS );

	if ( pInitBaseAnimating != this )
	{
		// Transfer children from the init base animating
		TransferChildren( pInitBaseAnimating, this );

		CBaseEntity *pChild = FirstMoveChild();

		while ( pChild )
		{
			CEntityFreezing *pFreezing = dynamic_cast<CEntityFreezing*>( pChild );
			if ( pFreezing )
			{
				pFreezing->FinishFreezing();
			}

			pChild = pChild->NextMovePeer();
		}
	}

	return true;
}

bool CStatueProp::CreateVPhysicsFromOBBs( CBaseAnimating *pInitBaseAnimating )
{
	// Make enough pointers to convexes for each hitbox
	CPhysConvex **ppConvex = new CPhysConvex*[ std::ssize( *m_pInitOBBs ) ];

	float flTotalVolume = 0.0f;
	float flTotalSurfaceArea = 0.0f;

	for ( int i = 0; i < std::ssize( *m_pInitOBBs ); i++ )
	{
		const outer_collision_obb_t *pOBB = &((*m_pInitOBBs)[ i ]);

		// Accumulate volume and area
		Vector flDimentions = pOBB->vecMaxs - pOBB->vecMins;
		flTotalVolume += flDimentions.x * flDimentions.y * flDimentions.z;
		flTotalSurfaceArea += 2.0f * ( flDimentions.x * flDimentions.y + flDimentions.x * flDimentions.z + flDimentions.y * flDimentions.z );

		// Get angled min and max extents
		Vector vecMins, vecMaxs;
		VectorRotate( pOBB->vecMins, pOBB->angAngles, vecMins );
		VectorRotate( pOBB->vecMaxs, pOBB->angAngles, vecMaxs );

		// Get the corners in world space
		Vector vecMinCorner = pOBB->vecPos + vecMins;
		Vector vecMaxCorner = pOBB->vecPos + vecMaxs;

		// Get the normals of the hitbox in world space
		Vector vecForward, vecRight, vecUp;
		AngleVectors( pOBB->angAngles, &vecForward, &vecRight, &vecUp );
		vecRight = -vecRight;

		// Convert corners and normals to local space
		Vector vecCornerLocal[ 2 ];
		Vector vecNormalLocal[ 3 ];

		matrix3x4_t matToWorld = EntityToWorldTransform();
		VectorITransform( vecMaxCorner, matToWorld, vecCornerLocal[ 0 ] );
		VectorITransform( vecMinCorner, matToWorld, vecCornerLocal[ 1 ] );
		VectorIRotate( vecForward, matToWorld, vecNormalLocal[ 0 ] );
		VectorIRotate( vecRight, matToWorld, vecNormalLocal[ 1 ] );
		VectorIRotate( vecUp, matToWorld, vecNormalLocal[ 2 ] );

		// Create 6 planes from the local oriented hit box data
		float pPlanes[ 4 * 6 ];

		for ( int iPlane = 0; iPlane < 6; ++iPlane )
		{
			int iPlaneMod2 = iPlane % 2;
			int iPlaneDiv2 = iPlane / 2;
			bool bOdd = ( iPlaneMod2 == 1 );

			// Plane Normal
			pPlanes[ iPlane * 4 + 0 ] = vecNormalLocal[ iPlaneDiv2 ].x * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 1 ] = vecNormalLocal[ iPlaneDiv2 ].y * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 2 ] = vecNormalLocal[ iPlaneDiv2 ].z * ( bOdd ? -1.0f : 1.0f );

			// Plane D
			pPlanes[ iPlane * 4 + 3 ] = ( vecCornerLocal[ iPlaneMod2 ].x * vecNormalLocal[ iPlaneDiv2 ].x + 
				vecCornerLocal[ iPlaneMod2 ].y * vecNormalLocal[ iPlaneDiv2 ].y + 
				vecCornerLocal[ iPlaneMod2 ].z * vecNormalLocal[ iPlaneDiv2 ].z ) * ( bOdd ? -1.0f : 1.0f );
		}

		// Create convex from the intersection of these planes
		ppConvex[ i ] = physcollision->ConvexFromPlanes( pPlanes, 6, 0.0f );
	}

	// Make a single collide out of the group of convex boxes
	CPhysCollide *pPhysCollide = physcollision->ConvertConvexToCollide( ppConvex, std::ssize( *m_pInitOBBs ) );

	delete[] ppConvex;

	// Create the physics object
	objectparams_t params = g_PhysDefaultObjectParams;
	params.pGameData = static_cast<void *>( this );

	int nMaterialIndex = physprops->GetSurfaceIndex( "ice" );	// use ice material

	IPhysicsObject* p = physenv->CreatePolyObject( pPhysCollide, nMaterialIndex, GetAbsOrigin(), GetAbsAngles(), &params );
	contract_assert( p != nullptr );

	// Set velocity
	Vector vecInitialVelocity = pInitBaseAnimating->GetAbsVelocity();
	p->SetVelocity( &vecInitialVelocity, nullptr );

	// Compute mass
	float flMass;
	float flDensity, flThickness;
	physprops->GetPhysicsProperties( nMaterialIndex, &flDensity, &flThickness, nullptr, nullptr );

	// Make it more hollow
	flThickness = std::min( 1.0f, flThickness + 0.5f );

	if ( flThickness > 0.0f )
	{
		flMass = flTotalSurfaceArea * flThickness * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}
	else
	{
		// density is in kg/m^3, volume is in in^3
		flMass = flTotalVolume * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}

	// Mass is somewhere between the original and if it was all ice
	p->SetMass( flMass );

	// Yes, gravity
	p->EnableGravity( true );

	// Use this as our vphysics
	VPhysicsSetObject( p );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	SetMoveType( MOVETYPE_VPHYSICS );

	m_pInitOBBs = nullptr;

	return true;
}

CBaseEntity *CreateServerStatue( CBaseAnimating *pAnimating, int collisionGroup )
{
	CStatueProp *pStatue = static_cast<CStatueProp *>( CreateEntityByName( "physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();
	}

	return pStatue;
}

CBaseEntity *CreateServerStatueFromOBBs( const std::vector< outer_collision_obb_t > &vecSphereOrigins, CBaseAnimating *pAnimating )
{
	contract_assert( std::ssize( vecSphereOrigins ) > 0 );

	if ( std::ssize( vecSphereOrigins ) <= 0 )
		return nullptr;

	CStatueProp *pStatue = static_cast<CStatueProp *>( CreateEntityByName( "physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_pInitOBBs = &vecSphereOrigins;

		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();

		pStatue->AddEffects( EF_NODRAW );
		pStatue->CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );
		pStatue->AddSolidFlags( ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMBOXTEST ) | ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMRAYTEST ) );

		pAnimating->SetParent( pStatue );

		// You'll need to keep track of the child for collision rules
		pStatue->SetThink( &CStatueProp::CollisionPartnerThink );
		pStatue->SetNextThink( gpGlobals->curtime + 1.0f );
	}

	return pStatue;
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Game & Client shared functions moved from physics.cpp

//
IPhysics			*physics = nullptr;
IPhysicsObject		*g_PhysWorldObject = nullptr;
IPhysicsCollision	*physcollision = nullptr;
IPhysicsEnvironment	*physenv = nullptr;
IPhysicsSurfaceProps *physprops = nullptr;
// UNDONE: This hash holds both entity & IPhysicsObject pointer pairs
// UNDONE: Split into separate hashes?
IPhysicsObjectPairHash *g_EntityCollisionHash = nullptr;

const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";

const objectparams_t g_PhysDefaultObjectParams =
{
	nullptr,
	1.0, //mass
	1.0, // inertia
	0.1f, // damping
	0.1f, // rotdamping
	0.05f, // rotIntertiaLimit
	"DEFAULT",
	nullptr,// game data
	0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f, // drag coefficient
	true,// enable collisions?
};

PRECACHE_REGISTER_BEGIN( GLOBAL, PhysFrictionEffect )
	PRECACHE( PARTICLE_SYSTEM, "impact_physics_dust" )
	PRECACHE( PARTICLE_SYSTEM, "impact_physics_sparks" )
PRECACHE_REGISTER_END()

void CSolidSetDefaults::SetDefaults( void *pData )
{
	solid_t *pSolid = (solid_t *)pData;
	pSolid->params = g_PhysDefaultObjectParams;
}

CSolidSetDefaults g_SolidSetup;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &mins - 
//			&maxs - 
// Output : CPhysCollide
//-----------------------------------------------------------------------------
CPhysCollide *PhysCreateBbox( const Vector &minsIn, const Vector &maxsIn )
{
	// UNDONE: Track down why this causes errors for the player controller and adjust/enable
	//float radius = 0.5 - DIST_EPSILON;
	Vector mins = minsIn;// + Vector(radius, radius, radius);
	Vector maxs = maxsIn;// - Vector(radius, radius, radius);

	// VPHYSICS caches/cleans up these
	CPhysCollide *pResult = physcollision->BBoxToCollide( mins, maxs );

	return pResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateBox( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic )
{
	int modelIndex = pEntity->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = modelinfo->GetModel( modelIndex );
		if ( model )
		{
			CStudioHdr studioHdr( modelinfo->GetStudiomodel( model ), mdlcache );
			if ( studioHdr.IsValid() )
			{
				pSurfaceProps = Studio_GetDefaultSurfaceProps( &studioHdr );
			}
		}
	}
	V_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox( mins, maxs );
	if ( !pCollide )
		return nullptr;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, vec3_angle, STRING(pEntity->GetModelName()), isStatic, &solid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateOBB( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic )
{
	int modelIndex = pEntity->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = modelinfo->GetModel( modelIndex );
		if ( model )
		{
			CStudioHdr studioHdr( modelinfo->GetStudiomodel( model ), mdlcache );
			if (studioHdr.IsValid()) 
			{
				pSurfaceProps = Studio_GetDefaultSurfaceProps( &studioHdr );
			}
		}
	}
	V_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox( mins, maxs );
	if ( !pCollide )
		return nullptr;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, angle, STRING(pEntity->GetModelName()), isStatic, &solid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, int modelIndex, int solidIndex )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide )
		return false;

	bool parsed = false;

	std::memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				contract_assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolid( solid_t &solid, CBaseEntity *pEntity, int modelIndex )
{
	return PhysModelParseSolidByIndex( solid, pEntity, modelIndex, -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			*pCollide - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, vcollide_t *pCollide, int solidIndex )
{
	bool parsed = false;

	std::memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				contract_assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreate( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid )
{
	if ( !physenv )
		return nullptr;

	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return nullptr;
	
	solid_t tmpSolid;
	if ( !pSolid )
	{
		pSolid = &tmpSolid;
		if ( !PhysModelParseSolidByIndex( tmpSolid, pEntity, pCollide, -1 ) )
			return nullptr;
	}

	int surfaceProp = -1;
	if ( pSolid->surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( pSolid->surfaceprop );
	}
	IPhysicsObject *pObject = physenv->CreatePolyObject( pCollide->solids[pSolid->index], surfaceProp, origin, angles, &pSolid->params );
	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );

	if ( pObject )
	{
		if ( modelinfo->GetModelType(modelinfo->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = modelinfo->GetModelContents( modelIndex );
			contract_assert( contents!=0 );
			// HACKHACK: contents is used to filter collisions
			// HACKHACK: So keep solid on for water brushes since they should pass collision rules (as triggers)
			if ( contents & MASK_WATER )
			{
				contents |= CONTENTS_SOLID;
			}
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}

	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateUnmoveable( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles )
{
	if ( !physenv )
		return nullptr;

	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return nullptr;

	solid_t solid;

	if ( !PhysModelParseSolidByIndex( solid, pEntity, pCollide, -1 ) )
		return nullptr;

	// collisions are off by default
	solid.params.enableCollisions = true;
	//solid.params.mass = 1.0;
	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( solid.surfaceprop );
	}
	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pCollide->solids[0], surfaceProp, origin, angles, &solid.params );

	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );
	if ( pObject )
	{
		if ( modelinfo->GetModelType(modelinfo->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = modelinfo->GetModelContents( modelIndex );
			contract_assert( contents!=0 );
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}
	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: Create a vphysics object based on an existing collision model
// Input  : *pEntity - 
//			*pModel - 
//			&origin - 
//			&angles - 
//			*pName - 
//			isStatic - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateCustom( CBaseEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid )
{
	if ( !physenv )
		return nullptr;

	solid_t tmpSolid;
	if ( !pSolid )
	{
		PhysGetDefaultAABBSolid( tmpSolid );
		pSolid = &tmpSolid;
	}
	int surfaceProp = physprops->GetSurfaceIndex( pSolid->surfaceprop );
	pSolid->params.pGameData = static_cast<void *>(pEntity);
	pSolid->params.pName = pName;
	IPhysicsObject *pObject = nullptr;
	if ( isStatic )
	{
		pObject = physenv->CreatePolyObjectStatic( pModel, surfaceProp, origin, angles, &pSolid->params );
	}
	else
	{
		pObject = physenv->CreatePolyObject( pModel, surfaceProp, origin, angles, &pSolid->params );
	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			radius - 
//			&origin - 
//			&solid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysSphereCreate( CBaseEntity *pEntity, float radius, const Vector &origin, solid_t &solid )
{
	if ( !physenv )
		return nullptr;

	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( solid.surfaceprop );
	}

	solid.params.pGameData = static_cast<void *>(pEntity);
	IPhysicsObject *pObject = physenv->CreateSphereObject( radius, surfaceProp, origin, vec3_angle, &solid.params, false );

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhysGetDefaultAABBSolid( solid_t &solid )
{
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	V_strncpy( solid.surfaceprop, "default", sizeof( solid.surfaceprop ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destroy a physics object
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void PhysDestroyObject( IPhysicsObject *pObject, CBaseEntity *pEntity )
{

	if ( pObject )
		pObject->SetGameData( nullptr );

	g_EntityCollisionHash->RemoveAllPairsForObject( pObject );
	if ( pEntity && pEntity->IsMarkedForDeletion() )
	{
		g_EntityCollisionHash->RemoveAllPairsForObject( pEntity );
	}

	if ( physenv )
	{
		physenv->DestroyObject( pObject );
	}
}

void AddSurfacepropFile( const char *pFileName, IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	ks::ScratchArena<> scratch;
	// Load file into memory
	FileHandle_t file = pFileSystem->Open( pFileName, "rb", "GAME" );

	if ( file )
	{
		int len = pFileSystem->Size( file );

		// read the file
		int nBufSize = len+1;
		char *buffer = (char *)scratch.AllocBytes( nBufSize );
		pFileSystem->ReadEx( buffer, nBufSize, len, file );
		pFileSystem->Close( file );
		buffer[len] = 0;
		pProps->ParseSurfaceData( pFileName, buffer );
		// buffer is on the stack, no need to free
	}
	else
	{
		Error( "Unable to load surface prop file '%s' (referenced by manifest file '%s')\n", pFileName, SURFACEPROP_MANIFEST_FILE );
	}
}

void PhysParseSurfaceData( IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSurfacepropFile( sub->GetString(), pProps, pFileSystem );
				continue;
			}

			Warning( "surfaceprops::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SURFACEPROP_MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SURFACEPROP_MANIFEST_FILE );
	}

	manifest->deleteThis();
}

void PhysCreateVirtualTerrain( CBaseEntity *pWorld, const objectparams_t &defaultParams )
{
	if ( !physenv )
		return;

	char nameBuf[1024];
	for ( int i = 0; i < MAX_MAP_DISPINFO; i++ )
	{
		CPhysCollide *pCollide = modelinfo->GetCollideForVirtualTerrain( i );
		if ( pCollide )
		{
			solid_t solid;
			solid.params = defaultParams;
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			V_snprintf(nameBuf, sizeof(nameBuf), "vdisp_%04d", i );
			solid.params.pName = nameBuf;
			int surfaceData = physprops->GetSurfaceIndex( "default" );
			// create this as part of the world
			IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pCollide, surfaceData, vec3_origin, vec3_angle, &solid.params );
			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			pObject->SetCollisionHints( COLLISION_HINT_STATICSOLID );
		}
	}
}

IPhysicsObject *PhysCreateWorld_Shared( CBaseEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams )
{
	solid_t solid;
	fluid_t fluid;

	if ( !physenv )
		return nullptr;

	int surfaceData = physprops->GetSurfaceIndex( "default" );

	objectparams_t params = defaultParams;
	params.pGameData = static_cast<void *>(pWorld);
	params.pName = "world";

	IPhysicsObject *pWorldPhysics = physenv->CreatePolyObjectStatic( 
		pWorldCollide->solids[0], surfaceData, vec3_origin, vec3_angle, &params );

	// hint - saves vphysics some work
	pWorldPhysics->SetCallbackFlags( pWorldPhysics->GetCallbackFlags() | CALLBACK_NEVER_DELETED );

	//PhysCheckAdd( world, "World" );
	// walk the world keys in case there are some fluid volumes to create
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pWorldCollide );

	bool bCreateVirtualTerrain = false;
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();

		if ( !strcmpi( pBlock, "solid" ) || !strcmpi( pBlock, "staticsolid" ) )
		{
			solid.params = defaultParams;
			pParse->ParseSolid( &solid, &g_SolidSetup );
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			solid.params.pName = "world";
			int surfaceData = physprops->GetSurfaceIndex( "default" );

			// already created world above
			if ( solid.index == 0 )
				continue;

			if ( !pWorldCollide->solids[solid.index] )
			{
				// this implies that the collision model is a mopp and the physics DLL doesn't support that.
				bCreateVirtualTerrain = true;
				continue;
			}
			// create this as part of the world
			IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pWorldCollide->solids[solid.index], 
				surfaceData, vec3_origin, vec3_angle, &solid.params );

			// invalid collision model or can't create, ignore
			if (!pObject)
				continue;

			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			contract_assert( solid.contents != 0 );
			pObject->SetContents( solid.contents );
			if ( solid.contents & CONTENTS_SOLID )
			{
				pObject->SetCollisionHints(COLLISION_HINT_STATICSOLID);
			}

			if ( !pWorldPhysics )
			{
				pWorldPhysics = pObject;
			}
		}
		else if ( !strcmpi( pBlock, "fluid" ) )
		{
			pParse->ParseFluid( &fluid, nullptr );

			// create a fluid for floating
			if ( fluid.index > 0 )
			{
				solid.params = defaultParams;	// copy world's params
				solid.params.enableCollisions = true;
				solid.params.pName = "fluid";
				solid.params.pGameData = static_cast<void *>(pWorld);
				fluid.params.pGameData = static_cast<void *>(pWorld);
				int surfaceData = physprops->GetSurfaceIndex( fluid.surfaceprop );
				// create this as part of the world
				IPhysicsObject *pWater = physenv->CreatePolyObjectStatic( pWorldCollide->solids[fluid.index], 
					surfaceData, vec3_origin, vec3_angle, &solid.params );

				pWater->SetCallbackFlags( pWater->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
				physenv->CreateFluidController( pWater, &fluid.params );
			}
		}
		else if ( !strcmpi( pBlock, "materialtable" ) )
		{
			int surfaceTable[128];
			std::memset( surfaceTable, 0, sizeof(surfaceTable) );

			pParse->ParseSurfaceTable( surfaceTable, nullptr );
			physprops->SetWorldMaterialIndexTable( surfaceTable, 128 );
		}
		else if ( !strcmpi(pBlock, "virtualterrain" ) )
		{
			bCreateVirtualTerrain = true;
			pParse->SkipBlock();
		}
		else
		{
			// unknown chunk???
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	if ( bCreateVirtualTerrain && physcollision->SupportsVirtualMesh() )
	{
		PhysCreateVirtualTerrain( pWorld, defaultParams );
	}
	return pWorldPhysics;
}

//=============================================================================
//
// Physics Game Trace
//
class CPhysicsGameTrace : public IPhysicsGameTrace
{
public:

	void VehicleTraceRay( const Ray_t &ray, void *pVehicle, trace_t *pTrace );
	void VehicleTraceRayWithWater( const Ray_t &ray, void *pVehicle, trace_t *pTrace );
	bool VehiclePointInWater( const Vector &vecPoint );
};

CPhysicsGameTrace g_PhysGameTrace;
IPhysicsGameTrace *physgametrace = &g_PhysGameTrace;

//-----------------------------------------------------------------------------
// Purpose: Game ray-traces in vphysics.
//-----------------------------------------------------------------------------
void CPhysicsGameTrace::VehicleTraceRay( const Ray_t &ray, void *pVehicle, trace_t *pTrace )
{
	CBaseEntity *pBaseEntity = static_cast<CBaseEntity*>( pVehicle );
	UTIL_TraceRay( ray, MASK_SOLID, pBaseEntity, COLLISION_GROUP_NONE, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Game ray-traces in vphysics.
//-----------------------------------------------------------------------------
void CPhysicsGameTrace::VehicleTraceRayWithWater( const Ray_t &ray, void *pVehicle, trace_t *pTrace )
{
	CBaseEntity *pBaseEntity = static_cast<CBaseEntity*>( pVehicle );
	UTIL_TraceRay( ray, MASK_SOLID|MASK_WATER, pBaseEntity, COLLISION_GROUP_NONE, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Test to see if a vehicle point is in water.
//-----------------------------------------------------------------------------
bool CPhysicsGameTrace::VehiclePointInWater( const Vector &vecPoint )
{
	return ( ( UTIL_PointContents( vecPoint, MASK_WATER ) & MASK_WATER ) != 0 );
}

void PhysRecheckObjectPair( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0->IsStatic() )
	{
		pObject0->RecheckCollisionFilter();
	}
	if ( !pObject1->IsStatic() )
	{
		pObject1->RecheckCollisionFilter();
	}
}

void PhysEnableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pObject0->GetGameData());
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	g_EntityCollisionHash->RemoveObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pObject0->GetGameData());
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	g_EntityCollisionHash->AddObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

void PhysDisableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	if ( !pEntity0 || !pEntity1 )
		return;

	g_EntityCollisionHash->AddObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

void PhysEnableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	if ( !pEntity0 || !pEntity1 )
		return;

	g_EntityCollisionHash->RemoveObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

bool PhysEntityCollisionsAreDisabled( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	return g_EntityCollisionHash->IsObjectPairInHash( pEntity0, pEntity1 );
}

void PhysEnableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	g_EntityCollisionHash->RemoveObjectPair( pObject0, pObject1 );
	PhysRecheckObjectPair( pObject0, pObject1 );
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	g_EntityCollisionHash->AddObjectPair( pObject0, pObject1 );
	PhysRecheckObjectPair( pObject0, pObject1 );
}

void PhysComputeSlideDirection( IPhysicsObject *pPhysics, const Vector &inputVelocity, const AngularImpulse &inputAngularVelocity, 
							   Vector *pOutputVelocity, Vector *pOutputAngularVelocity, float minMass )
{
	Vector velocity = inputVelocity;
	AngularImpulse angVel = inputAngularVelocity;
	Vector pos;

	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( !pOther->IsMoveable() || pOther->GetMass() > minMass )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );

			// BUGBUG: Figure out the correct rotation clipping equation
			if ( pOutputAngularVelocity )
			{
				angVel = normal * DotProduct( angVel, normal );
			}

			// Determine how far along plane to slide based on incoming direction.
			// NOTE: Normal points away from this object
			float proj = DotProduct( velocity, normal );
			if ( proj > 0.0f )
			{
				velocity -= normal * proj;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	//NDebugOverlay::Line( pos, pos + unitVel * 20, 0, 0, 255, true, 0.1 );
	
	if ( pOutputVelocity )
	{
		*pOutputVelocity = velocity;
	}
	if ( pOutputAngularVelocity )
	{
		*pOutputAngularVelocity = angVel;
	}
}

bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir )
{
	bool hit = false;
	void *pGameData = pPhysics->GetGameData();
	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( pOther->GetGameData() != pGameData )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );
			if ( DotProduct(normal,dir) > 0 )
			{
				hit = true;
				break;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	return hit;
}

void PhysForceClearVelocity( IPhysicsObject *pPhys )
{
	IPhysicsFrictionSnapshot *pSnapshot = pPhys->CreateFrictionSnapshot();
	// clear the velocity of the rigid body
	Vector vel;
	AngularImpulse angVel;
	vel.Init();
	angVel.Init();
	pPhys->SetVelocity( &vel, &angVel );
	// now clear the "strain" stored in the contact points
	while ( pSnapshot->IsValid() )
	{
		pSnapshot->ClearFrictionForce();
		pSnapshot->RecomputeFriction();
		pSnapshot->NextFrictionData();
	}
	pPhys->DestroyFrictionSnapshot( pSnapshot );
}

void PhysFrictionEffect( Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit )
{
	QAngle angDirection;
	VectorAngles( vecVel, angDirection );

	surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );
	surfacedata_t *phit = physprops->GetSurfaceData( surfacePropsHit );
	switch ( phit->game.material )
	{
	case CHAR_TEX_DIRT:
		
		if ( energy < MASS10_SPEED2ENERGY(15) )
			break;
		
		DispatchParticleEffect( "impact_physics_dust", vecPos, angDirection );
		break;

	case CHAR_TEX_CONCRETE:
		
		if ( energy < MASS10_SPEED2ENERGY(28) )
			break;

		DispatchParticleEffect( "impact_physics_dust", vecPos, angDirection );
		break;
	}
	
	//Metal sparks
	if ( energy > MASS10_SPEED2ENERGY(50) )
	{
		// make sparks for metal/concrete scrapes with enough energy
		if ( psurf->game.material == CHAR_TEX_METAL || psurf->game.material == CHAR_TEX_GRATE )
		{	
			switch ( phit->game.material )
			{
			case CHAR_TEX_CONCRETE:
			case CHAR_TEX_METAL:

				DispatchParticleEffect( "impact_physics_sparks", vecPos, angDirection );
				break;									
			}
		}
	}
}

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit )
{
	if ( !pEntity || energy < 75.0f || surfaceProps < 0 )
		return;
	
	// don't make noise for hidden/invisible/sky materials
	surfacedata_t *phit = physprops->GetSurfaceData( surfacePropsHit );
	surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );

	if ( phit->game.material == 'X' || psurf->game.material == 'X' )
		return;

	// rescale the incoming energy
	energy *= ENERGY_VOLUME_SCALE;

	// volume of scrape is proportional to square of energy (steeper rolloff at low energies)
	float volume = energy * energy;
		
	unsigned short soundName = psurf->sounds.scrapeRough;
	HSOUNDSCRIPTHASH *soundHandle = &psurf->soundhandles.scrapeRough;

	if ( psurf->sounds.scrapeSmooth && phit->audio.roughnessFactor < psurf->audio.roughThreshold )
	{
		soundName = psurf->sounds.scrapeSmooth;
		soundHandle = &psurf->soundhandles.scrapeRough;
	}

	const char *pSoundName = physprops->GetString( soundName );

	PhysFrictionSound( pEntity, pObject, pSoundName, *soundHandle, volume );
}

//-----------------------------------------------------------------------------
// Purpose: Precaches a surfaceproperties string name if it's set.
// Input  : idx - 
// Output : static void
//-----------------------------------------------------------------------------
static HSOUNDSCRIPTHASH PrecachePhysicsSoundByStringIndex( int idx )
{
	// Only precache if a value was set in the script file...
	if ( idx != 0 )
	{
		return CBaseEntity::PrecacheScriptSound( physprops->GetString( idx ) );
	}

	return SOUNDEMITTER_INVALID_HASH;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates all surfacedata sounds and precaches them
// Output : static void
//-----------------------------------------------------------------------------
void PrecachePhysicsSounds()
{
	// precache the surface prop sounds
	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		contract_assert( pprop );

		pprop->soundhandles.walkStepLeft = PrecachePhysicsSoundByStringIndex( pprop->sounds.walkStepLeft );
		pprop->soundhandles.walkStepRight = PrecachePhysicsSoundByStringIndex( pprop->sounds.walkStepRight );
		pprop->soundhandles.runStepLeft = PrecachePhysicsSoundByStringIndex( pprop->sounds.runStepLeft );
		pprop->soundhandles.runStepRight = PrecachePhysicsSoundByStringIndex( pprop->sounds.runStepRight );
		pprop->soundhandles.impactSoft = PrecachePhysicsSoundByStringIndex( pprop->sounds.impactSoft );
		pprop->soundhandles.impactHard = PrecachePhysicsSoundByStringIndex( pprop->sounds.impactHard );
		pprop->soundhandles.scrapeSmooth = PrecachePhysicsSoundByStringIndex( pprop->sounds.scrapeSmooth );
		pprop->soundhandles.scrapeRough = PrecachePhysicsSoundByStringIndex( pprop->sounds.scrapeRough );
		pprop->soundhandles.bulletImpact = PrecachePhysicsSoundByStringIndex( pprop->sounds.bulletImpact );
		pprop->soundhandles.rolling = PrecachePhysicsSoundByStringIndex( pprop->sounds.rolling );
		pprop->soundhandles.breakSound = PrecachePhysicsSoundByStringIndex( pprop->sounds.breakSound );
		pprop->soundhandles.strainSound = PrecachePhysicsSoundByStringIndex( pprop->sounds.strainSound );
	}
}

float PhysGetEntityMass( CBaseEntity *pEntity )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int physCount = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
	float otherMass = 0;
	for ( int i = 0; i < physCount; i++ )
	{
		otherMass += pList[i]->GetMass();
	}

	return otherMass;
}
