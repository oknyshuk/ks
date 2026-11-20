// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <stdlib.h>
#include <string.h>
#include "eiface.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "datacache/imdlcache.h"
#include "edict.h"
#include "networkvar.h"
#include "stringregistry.h"
#include "declare_class.h"
#include "igameevents.h"
#include "vscript/ivscript.h"
#include "ai_criteria.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "shake.h"
#include "keyframe/keyframe.h"
module ks.server;

import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import std;
import ks.core.scratcharena;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.tier1.convar;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import :cstrike15.cs_player;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_playeranimstate;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import ks.core.basetypes;
import ks.tier1.icvar;
import :econ.econ_item_schema_minimal;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.vector_ops;
import ks.math.mathlib;
import ks.core.threadtools;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.core.platform;
import ks.tier1.iconvar;

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: A message forwarder. Fires an OnTrigger output when triggered, and can be
// disabled to prevent forwarding outputs.
// Useful as an intermediary between one entity and another for turning on or
// off an I/O connection, or as a container for holding a set of outputs that
// can be triggered from multiple places.

static const int SF_REMOVE_ON_FIRE				= 0x001;	// Relay will remove itself after being triggered.
static const int SF_ALLOW_FAST_RETRIGGER		= 0x002;	// Unless set, relay will disable itself until the last output is sent.

LINK_ENTITY_TO_CLASS(logic_relay, CLogicRelay);

IMPLEMENT_REFLECT_DATAMAP( CLogicRelay )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicRelay::CLogicRelay(void)
{
}

//------------------------------------------------------------------------------
// Kickstarts a think if we have OnSpawn connections.
//------------------------------------------------------------------------------
void CLogicRelay::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	if ( m_OnSpawn.NumberOfElements() > 0)
	{
		SetNextThink( gpGlobals->curtime + 0.01 );
	}
}

//-----------------------------------------------------------------------------
// If we have OnSpawn connections, this is called shortly after spawning to
// fire the OnSpawn output.
//-----------------------------------------------------------------------------
void CLogicRelay::Think_Impl()
{
	// Fire an output when we spawn. This is used for self-starting an entity
	// template -- since the logic_relay is inside the template, it gets all the
	// name and I/O connection fixup, so can target other entities in the template.
	m_OnSpawn.FireOutput( this, this );

	// We only get here if we had OnSpawn connections, so this is safe.
	if ( m_spawnflags & SF_REMOVE_ON_FIRE )
	{
		UTIL_Remove(this);
	}
}

//------------------------------------------------------------------------------
// Purpose: Turns on the relay, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicRelay::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Enables us to fire again. This input is only posted from our Trigger
//			function to prevent rapid refire.
//------------------------------------------------------------------------------
void CLogicRelay::InputEnableRefire( inputdata_t &inputdata )
{ 
	m_bWaitForRefire = false;
}

//------------------------------------------------------------------------------
// Purpose: Cancels any I/O events in the queue that were fired by us.
//------------------------------------------------------------------------------
void CLogicRelay::InputCancelPending( inputdata_t &inputdata )
{ 
	g_EventQueue.CancelEvents( this );

	// Stop waiting; allow another Trigger.
	m_bWaitForRefire = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns off the relay, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicRelay::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}

//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the relay.
//------------------------------------------------------------------------------
void CLogicRelay::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the relay.
//-----------------------------------------------------------------------------
void CLogicRelay::InputTrigger( inputdata_t &inputdata )
{
	if (!m_bDisabled)
	{
		if (!m_bWaitForRefire)
		{
			m_OnTrigger.FireOutput( inputdata.pActivator, this );
			
			if (m_spawnflags & SF_REMOVE_ON_FIRE)
			{
				UTIL_Remove(this);
			}
			else if (!(m_spawnflags & SF_ALLOW_FAST_RETRIGGER))
			{
				//
				// Disable the relay so that it cannot be refired until after the last output
				// has been fired and post an input to re-enable ourselves.
				//
				m_bWaitForRefire = true;
				g_EventQueue.AddEvent(this, "EnableRefire", m_OnTrigger.GetMaxDelay() + 0.001, this, this);
			}
		}
		else
		{
			Warning( "*************************************************************************************************\n" );
			Warning( "*** ERROR: logic_relay %s has been triggered but is awaiting refire. OUTPUTS WILL NOT BE FIRED!!!\n", GetDebugName() );
			Warning( "*************************************************************************************************\n\n" );
		}
	}
}

class CBaseAnimating;
class CBaseEntity;
class CPointTemplate;
class CWorld;

using namespace ::ks::reflect;

// Purpose: Controls the loading, parsing and creation of the entities from the BSP.

static CStringRegistry *g_pClassnameSpawnPriority = nullptr;
extern edict_t *g_pForceAttachEdict;

extern ConVar mp_weapons_allow_map_placed;

// creates an entity by string name, but does not spawn it
CBaseEntity *CreateEntityByName( const char *className, int iForceEdictIndex, bool bNotify )
{
	if ( iForceEdictIndex != -1 )
	{
		g_pForceAttachEdict = engine->CreateEdict( iForceEdictIndex );
		if ( !g_pForceAttachEdict )
			Error( "CreateEntityByName( %s, %d ) - CreateEdict failed.", className, iForceEdictIndex );
	}

	IServerNetworkable *pNetwork = EntityFactoryDictionary()->Create( className );
	g_pForceAttachEdict = nullptr;

	if ( !pNetwork )
		return nullptr;

	CBaseEntity *pEntity = EntityCast<CBaseEntity>( pNetwork->GetBaseEntity() );
	contract_assert( pEntity );
	if ( bNotify )
	{
		gEntList.NotifyCreateEntity( pEntity );
	}
	return pEntity;
}

CBaseNetworkable *CreateNetworkableByName( const char *className )
{
	IServerNetworkable *pNetwork = EntityFactoryDictionary()->Create( className );
	if ( !pNetwork )
		return nullptr;

	CBaseNetworkable *pNetworkable = pNetwork->GetBaseNetworkable();
	contract_assert( pNetworkable );
	return pNetworkable;
}

void FreeContainingEntity( edict_t *ed )
{
	if ( ed )
	{
		CBaseEntity *ent = GetContainingEntity( ed );
		if ( ent )
		{
			ed->SetEdict( nullptr, false );
			CBaseEntity::PhysicsRemoveTouchedList( ent );
			CBaseEntity::PhysicsRemoveGroundList( ent );
			UTIL_RemoveImmediate( ent );
		}
	}
}

// parent name may have a , in it to include an attachment point
string_t ExtractParentName(string_t parentName)
{
	if ( !std::strchr(STRING(parentName), ',') )
		return parentName;

	char szToken[256];
	nexttoken(szToken, STRING(parentName), ',');
	return AllocPooledString(szToken);
}

//-----------------------------------------------------------------------------
// Purpose: Callback function for qsort, used to sort entities by their depth
//			in the movement hierarchy.
// Input  : pEnt1 - 
//			pEnt2 - 
// Output : Returns -1, 0, or 1 per qsort spec.
//-----------------------------------------------------------------------------
static int CompareSpawnOrder(HierarchicalSpawn_t *pEnt1, HierarchicalSpawn_t *pEnt2)
{
	if (pEnt1->m_nDepth == pEnt2->m_nDepth)
	{
		if ( g_pClassnameSpawnPriority )
		{
			int o1 = pEnt1->m_pEntity ? g_pClassnameSpawnPriority->GetStringID( pEnt1->m_pEntity->GetClassname() ) : -1;
			int o2 = pEnt2->m_pEntity ? g_pClassnameSpawnPriority->GetStringID( pEnt2->m_pEntity->GetClassname() ) : -1;
			if ( o1 < o2 )
				return 1;
			if ( o2 < o1 )
				return -1;
		}
		return 0;
	}

	if (pEnt1->m_nDepth > pEnt2->m_nDepth)
		return 1;

	return -1;
}

//-----------------------------------------------------------------------------
// Computes the hierarchical depth of the entities to spawn..
//-----------------------------------------------------------------------------
static int ComputeSpawnHierarchyDepth_r( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return 1;

	if (pEntity->m_iParent == NULL_STRING)
		return 1;

	CBaseEntity *pParent = gEntList.FindEntityByName( nullptr, ExtractParentName(pEntity->m_iParent) );
	if (!pParent)
		return 1;
	
	if (pParent == pEntity)
	{
		Warning( "LEVEL DESIGN ERROR: Entity %s is parented to itself!\n", pEntity->GetDebugName() );
		return 1;
	}

	return 1 + ComputeSpawnHierarchyDepth_r( pParent );
}

static void ComputeSpawnHierarchyDepth( int nEntities, HierarchicalSpawn_t *pSpawnList )
{
	// NOTE: This isn't particularly efficient, but so what? It's at the beginning of time
	// I did it this way because it simplified the parent setting in hierarchy (basically
	// eliminated questions about whether you should transform origin from global to local or not)
	int nEntity;
	for (nEntity = 0; nEntity < nEntities; nEntity++)
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;
		if (pEntity && !pEntity->IsDormant())
		{
			pSpawnList[nEntity].m_nDepth = ComputeSpawnHierarchyDepth_r( pEntity );
		}
		else
		{
			pSpawnList[nEntity].m_nDepth = 1;
		}
	}
}

static void SortSpawnListByHierarchy( int nEntities, HierarchicalSpawn_t *pSpawnList )
{
	g_pClassnameSpawnPriority = new CStringRegistry;
	// this will cause the entities to be spawned in the indicated order
	// Highest string ID spawns first.  String ID is spawn priority.
	// by default, anything not in this list has priority -1
	g_pClassnameSpawnPriority->AddString( "func_wall", 10 );
	g_pClassnameSpawnPriority->AddString( "scripted_sequence", 9 );
	g_pClassnameSpawnPriority->AddString( "phys_hinge", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_ballsocket", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_slideconstraint", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_constraint", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_pulleyconstraint", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_lengthconstraint", 8 );
	g_pClassnameSpawnPriority->AddString( "phys_ragdollconstraint", 8 );
	g_pClassnameSpawnPriority->AddString( "info_mass_center", 8 ); // spawn these before physbox/prop_physics
	g_pClassnameSpawnPriority->AddString( "trigger_vphysics_motion", 8 ); // spawn these before physbox/prop_physics

	g_pClassnameSpawnPriority->AddString( "prop_physics", 7 );
	g_pClassnameSpawnPriority->AddString( "prop_ragdoll", 7 );
	// Sort the entities (other than the world) by hierarchy depth, in order to spawn them in
	// that order. This insures that each entity's parent spawns before it does so that
	// it can properly set up anything that relies on hierarchy.
	std::qsort(&pSpawnList[0], nEntities, sizeof(pSpawnList[0]), (int (*)(const void *, const void *))CompareSpawnOrder);
	delete g_pClassnameSpawnPriority;
	g_pClassnameSpawnPriority = nullptr;
}

void SetupParentsForSpawnList( int nEntities, HierarchicalSpawn_t *pSpawnList )
{
	int nEntity;
	for (nEntity = nEntities - 1; nEntity >= 0; nEntity--)
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;
		if ( pEntity )
		{
			if ( std::strchr(STRING(pEntity->m_iParent), ',') )
			{
				char szToken[256];
				const char *pAttachmentName = nexttoken(szToken, STRING(pEntity->m_iParent), ',');
				pEntity->m_iParent = AllocPooledString(szToken);
				CBaseEntity *pParent = gEntList.FindEntityByName( nullptr, pEntity->m_iParent );

				// setparent in the spawn pass instead - so the model will have been set & loaded
				pSpawnList[nEntity].m_pDeferredParent = pParent;
				pSpawnList[nEntity].m_pDeferredParentAttachment = pAttachmentName;
			}
			else
			{
				CBaseEntity *pParent = gEntList.FindEntityByName( nullptr, pEntity->m_iParent );

				if ((pParent != nullptr) && (pParent->edict() != nullptr))
				{
					pEntity->SetParent( pParent ); 
				}
			}
		}
	}
}

// this is a hook for edit mode
void RememberInitialEntityPositions( int nEntities, HierarchicalSpawn_t *pSpawnList )
{
	for (int nEntity = 0; nEntity < nEntities; nEntity++)
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		if ( pEntity )
		{
			NWCEdit::RememberEntityPosition( pEntity );
		}
	}
}

void SpawnAllEntities( int nEntities, HierarchicalSpawn_t *pSpawnList, bool bActivateEntities )
{
	int nEntity;
	for (nEntity = 0; nEntity < nEntities; nEntity++)
	{
		CVProfScope _( "MapEntity_ParseAllEntities_Spawn", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		if ( pSpawnList[nEntity].m_pDeferredParent )
		{
			// UNDONE: Promote this up to the root of this function?
			MDLCACHE_CRITICAL_SECTION();
			CBaseEntity *pParent = pSpawnList[nEntity].m_pDeferredParent;
			int iAttachment = -1;
			CBaseAnimating *pAnim = pParent->GetBaseAnimating();
			if ( pAnim )
			{
				iAttachment = pAnim->LookupAttachment(pSpawnList[nEntity].m_pDeferredParentAttachment);
			}
			pEntity->SetParent( pParent, iAttachment );
		}
		if ( pEntity )
		{
			if (DispatchSpawn(pEntity) < 0)
			{
				// Walk through all entities in this list in case spawning an entity
				// resulted in another one being UTIL_Remove'd
				for ( int i = 0; i < nEntities; i++ )
				{
					// this is a child object that will be deleted now
					if ( pSpawnList[i].m_pEntity && pSpawnList[i].m_pEntity->IsMarkedForDeletion() )
					{
						pSpawnList[i].m_pEntity = nullptr;
					}
				}
				// Spawn failed.
				gEntList.CleanupDeleteList();
				// Remove the entity from the spawn list
				pSpawnList[nEntity].m_pEntity = nullptr;
			}
		}
	}

	if ( bActivateEntities )
	{
		CVProfScope _( "MapEntity_ParseAllEntities_Activate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
		for (nEntity = 0; nEntity < nEntities; nEntity++)
		{
			CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

			if ( pEntity )
			{
				MDLCACHE_CRITICAL_SECTION();
				pEntity->Activate();
			}
		}
		mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
	}
}

// --------------------------------------------------------------------------------------------------- //
// CMapEntitySpawner implementation.
// --------------------------------------------------------------------------------------------------- //

CMapEntitySpawner::CMapEntitySpawner()
{
	m_nEntities = 0;
	m_pSpawnMapData = new HierarchicalSpawnMapData_t[NUM_ENT_ENTRIES];
	m_pSpawnList = new HierarchicalSpawn_t[NUM_ENT_ENTRIES];
}

CMapEntitySpawner::~CMapEntitySpawner()
{
	delete [] m_pSpawnMapData;
	delete [] m_pSpawnList;
}

void CMapEntitySpawner::AddEntity( CBaseEntity *pEntity, const char *pCurMapData, int iMapDataLength )
{
	if (pEntity->IsTemplate())
	{
		// It's a template entity. Squirrel away its keyvalue text so that we can
		// recreate the entity later via a spawner. pMapData points at the '}'
		// so we must add one to include it in the string.
		Templates_Add( pEntity, pCurMapData, iMapDataLength, pEntity->GetHammerID() );

		// Remove the template entity so that it does not show up in FindEntityXXX searches.
		UTIL_Remove(pEntity);
		PurgeRemovedEntities();
		return;
	}

	// To 
	if ( dynamic_cast<CWorld*>( pEntity ) )
	{
		CVProfScope _( "MapEntity_ParseAllEntities_SpawnWorld", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		pEntity->m_iParent = NULL_STRING;	// don't allow a parent on the first entity (worldspawn)

		DispatchSpawn(pEntity);
		return;
	}
			
	if ( dynamic_cast<CLight*>(pEntity) )
	{
		CVProfScope _( "MapEntity_ParseAllEntities_SpawnTransients", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		// We overflow the max edicts on large maps that have lots of entities.
		// Nodes & Lights remove themselves immediately on Spawn(), so dispatch their
		// spawn now, to free up the slot inside this loop.
		// NOTE: This solution prevents nodes & lights from being used inside point_templates.
		if (DispatchSpawn(pEntity) < 0)
		{
			PurgeRemovedEntities();
		}
		return;
	}

	// Build a list of all point_template's so we can spawn them before everything else
	CPointTemplate *pTemplate = dynamic_cast< CPointTemplate* >(pEntity);
	if ( pTemplate )
	{
		m_PointTemplates.push_back( pTemplate );
	}
	else
	{
		// Queue up this entity for spawning
		m_pSpawnList[m_nEntities].m_pEntity = pEntity;
		m_pSpawnList[m_nEntities].m_nDepth = 0;
		m_pSpawnList[m_nEntities].m_pDeferredParentAttachment = nullptr;
		m_pSpawnList[m_nEntities].m_pDeferredParent = nullptr;

		m_pSpawnMapData[m_nEntities].m_pMapData = pCurMapData;
		m_pSpawnMapData[m_nEntities].m_iMapDataLength = iMapDataLength;
		m_nEntities++;
	}
}

void MapEntity_ParseAllEntites_SpawnTemplates( CPointTemplate **pTemplates, int iTemplateCount, CBaseEntity **pSpawnedEntities, HierarchicalSpawnMapData_t *pSpawnMapData, int iSpawnedEntityCount )
{
	// Now loop through all our point_template entities and tell them to make templates of everything they're pointing to
	for ( int i = 0; i < iTemplateCount; i++ )
	{
		CVProfScope _( "MapEntity_ParseAllEntities_SpawnTemplates", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		CPointTemplate *pPointTemplate = pTemplates[i];

		// First, tell the Point template to Spawn
		if ( DispatchSpawn(pPointTemplate) < 0 )
		{
			UTIL_Remove(pPointTemplate);
			gEntList.CleanupDeleteList();
			continue;
		}

		pPointTemplate->StartBuildingTemplates();

		// Now go through all it's templates and turn the entities into templates
		int iNumTemplates = pPointTemplate->GetNumTemplateEntities();
		for ( int iTemplateNum = 0; iTemplateNum < iNumTemplates; iTemplateNum++ )
		{
			// Find it in the spawn list
			CBaseEntity *pEntity = pPointTemplate->GetTemplateEntity( iTemplateNum );
			for ( int iEntNum = 0; iEntNum < iSpawnedEntityCount; iEntNum++ )
			{
				if ( pSpawnedEntities[iEntNum] == pEntity )
				{
					// Give the point_template the mapdata
					pPointTemplate->AddTemplate( pEntity, pSpawnMapData[iEntNum].m_pMapData, pSpawnMapData[iEntNum].m_iMapDataLength );

					if ( pPointTemplate->ShouldRemoveTemplateEntities() )
					{
						// Remove the template entity so that it does not show up in FindEntityXXX searches.
						UTIL_Remove(pEntity);
						gEntList.CleanupDeleteList();

						// Remove the entity from the spawn list
						pSpawnedEntities[iEntNum] = nullptr;
					}
					break;
				}
			}
		}

		pPointTemplate->FinishBuildingTemplates();
	}
}

void CMapEntitySpawner::HandleTemplates()
{
	ks::ScratchArena<> scratch;
	if( m_PointTemplates.empty() )
		return;

	CBaseEntity **pSpawnedEntities = scratch.Alloc<CBaseEntity *>( m_nEntities );
	for( int i = 0; i != m_nEntities; ++i )
	{
		pSpawnedEntities[i] = m_pSpawnList[i].m_pEntity;
	}

	PurgeRemovedEntities();
	MapEntity_ParseAllEntites_SpawnTemplates( m_PointTemplates.data(), std::ssize( m_PointTemplates ), pSpawnedEntities, m_pSpawnMapData, m_nEntities );

	//copy the entity list back since some entities may have been removed and nulled out
	for( int i = 0; i != m_nEntities; ++i )
	{
		m_pSpawnList[i].m_pEntity = pSpawnedEntities[i]; 
	}
}

void CMapEntitySpawner::SpawnAndActivate( bool bActivateEntities )
{
	SpawnHierarchicalList( m_nEntities, m_pSpawnList, bActivateEntities );
}

void CMapEntitySpawner::PurgeRemovedEntities()
{
	// Walk through spawn list and NULL out any soon-to-be-stale pointers
	for ( int i = 0; i < m_nEntities; ++ i )
	{
		if ( m_pSpawnList[i].m_pEntity && m_pSpawnList[i].m_pEntity->IsMarkedForDeletion() )
		{
			m_pSpawnList[i].m_pEntity = nullptr;
		}
	}

	gEntList.CleanupDeleteList();
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void MapEntity_ParseAllEntities(const char *pMapData, IMapEntityFilter *pFilter, bool bActivateEntities)
{
	CVProfScope _( "MapEntity_ParseAllEntities", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CMapEntitySpawner spawner;

	char szTokenBuffer[MAPKEY_MAXLENGTH];

	//  Loop through all entities in the map data, creating each.
	for ( ; true; pMapData = MapEntity_SkipToNextEntity(pMapData, szTokenBuffer) )
	{
		//
		// Parse the opening brace.
		//
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );

		//
		// Check to see if we've finished or not.
		//
		if (!pMapData)
			break;

		if (token[0] != '{')
		{
			Error( "MapEntity_ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		//
		// Parse the entity and add it to the spawn list.
		//
		CBaseEntity *pEntity;
		const char *pCurMapData = pMapData;
		pMapData = MapEntity_ParseEntity(pEntity, pMapData, pFilter);
		if (pEntity == nullptr)
			continue;

		spawner.AddEntity( pEntity, pCurMapData, pMapData - pCurMapData + 2 );
	}

	spawner.HandleTemplates();
	spawner.SpawnAndActivate( bActivateEntities );
}

void SpawnHierarchicalList( int nEntities, HierarchicalSpawn_t *pSpawnList, bool bActivateEntities )
{
	// Compute the hierarchical depth of all entities hierarchically attached
	ComputeSpawnHierarchyDepth( nEntities, pSpawnList );

	// Sort the entities (other than the world) by hierarchy depth, in order to spawn them in
	// that order. This insures that each entity's parent spawns before it does so that
	// it can properly set up anything that relies on hierarchy.
	SortSpawnListByHierarchy( nEntities, pSpawnList );

	// save off entity positions if in edit mode
	if ( engine->IsInEditMode() )
	{
		RememberInitialEntityPositions( nEntities, pSpawnList );
	}
	// Set up entity movement hierarchy in reverse hierarchy depth order. This allows each entity
	// to use its parent's world spawn origin to calculate its local origin.
	SetupParentsForSpawnList( nEntities, pSpawnList );

	// Spawn all the entities in hierarchy depth order so that parents spawn before their children.
	SpawnAllEntities( nEntities, pSpawnList, bActivateEntities );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntData - 
//-----------------------------------------------------------------------------
void MapEntity_PrecacheEntity( const char *pEntData, int &nStringSize )
{
	CEntityMapData entData( (char*)pEntData, nStringSize );
	char className[MAPKEY_MAXLENGTH];
	
	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}

	// Construct via the LINK_ENTITY_TO_CLASS factory.
	CBaseEntity *pEntity = CreateEntityByName(className);

	//
	// Set up keyvalues, which can set the model name, which is why we don't just do UTIL_PrecacheOther here...
	//
	if ( pEntity != nullptr )
	{
		pEntity->ParseMapData(&entData);
		pEntity->Precache();
		UTIL_RemoveImmediate( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Takes a block of character data as the input
// Input  : pEntity - Receives the newly constructed entity, NULL on failure.
//			pEntData - Data block to parse to extract entity keys.
// Output : Returns the current position in the entity data block.
//-----------------------------------------------------------------------------
const char *MapEntity_ParseEntity(CBaseEntity *&pEntity, const char *pEntData, IMapEntityFilter *pFilter)
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];
	
	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}
	
	bool skipForGameType = false;

	pEntity = nullptr;
	if ( !pFilter || ( pFilter->ShouldCreateEntity( className ) && !skipForGameType ) )
	{
		//
		// Construct via the LINK_ENTITY_TO_CLASS factory.
		//
		if ( pFilter )
		{
			pEntity = pFilter->CreateNextEntity( className );
		}
		else
		{
			pEntity = CreateEntityByName(className);
		}

		//
		// Set up keyvalues.
		//
		if (pEntity != nullptr)
		{
			pEntity->ParseMapData(&entData);
		}
		else
		{
			Warning("Can't init %s\n", className);
		}
	}
	else
	{
		// Just skip past all the keys.
		char keyName[MAPKEY_MAXLENGTH];
		char value[MAPKEY_MAXLENGTH];
		if ( entData.GetFirstKey(keyName, value) )
		{
			do 
			{
			} 
			while ( entData.GetNextKey(keyName, value) );
		}
	}

	//
	// Return the current parser position in the data block
	//
	return entData.CurrentBufferPosition();
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CCSWeaponInfo;
class CGameCoopMissionManager;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Contains entities for implementing/changing game rules dynamically within each BSP.

IMPLEMENT_REFLECT_DATAMAP( CRuleEntity )

void CRuleEntity::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
}

bool CRuleEntity::CanFireForActivator( CBaseEntity *pActivator )
{
	if ( m_iszMaster != NULL_STRING )
	{
		if ( UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
			return true;
		else
			return false;
	}
	
	return true;
}

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CRulePointEntity )

void CRulePointEntity::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
	SetModelName( NULL_STRING );
	m_Score = 0;
}

// 
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
public:
	DECLARE_CLASS( CRuleBrushEntity, CRuleEntity );

	void		Spawn_Impl( void );

private:
};

void CRuleBrushEntity::Spawn_Impl( void )
{
	SetModel( STRING( GetModelName() ) );
	BaseClass::Spawn_Impl();
}

// CGameScore / game_score	-- award points to player / team 
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

static constexpr auto SF_SCORE_NEGATIVE = 0x0001;
static constexpr auto SF_SCORE_TEAM = 0x0002;

class CGameScore : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameScore, CRulePointEntity );
	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	inline	int		Points( void ) { return m_Score; }
	inline	bool	AllowNegativeScore( void ) { return m_spawnflags & SF_SCORE_NEGATIVE; }
	inline	int		AwardToTeam( void ) { return (m_spawnflags & SF_SCORE_TEAM); }

	inline	void	SetPoints( int points ) { m_Score = points; }

	[[= ks::reflect::Input{ .name = "ApplyScore", .type = FIELD_VOID } ]] void InputApplyScore( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddScoreTerrorist", .type = FIELD_VOID } ]] void InputAddScoreTerrorist( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddScoreCT", .type = FIELD_VOID } ]] void InputAddScoreCT( inputdata_t &inputdata );

private:
};

LINK_ENTITY_TO_CLASS( game_score, CGameScore );

IMPLEMENT_REFLECT_DATAMAP( CGameScore )

void CGameScore::Spawn_Impl( void )
{
	int iScore = Points();
	BaseClass::Spawn_Impl();
	SetPoints( iScore );
}

bool CGameScore::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "points"))
	{
		SetPoints( std::atoi(szValue) );
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

void CGameScore::InputApplyScore( inputdata_t &inputdata )
{
	CBaseEntity *pActivator = inputdata.pActivator;

	if ( pActivator == nullptr )
		 return;

	if ( CanFireForActivator( pActivator ) == false )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}

void CGameScore::InputAddScoreTerrorist( inputdata_t &inputdata )
{
	CCSMatch* match = CSGameRules()->GetMatch();
	if ( match )
	{
		match->AddTerroristScore( Points() );
	}
}

void CGameScore::InputAddScoreCT( inputdata_t &inputdata )
{
	CCSMatch* match = CSGameRules()->GetMatch();
	if ( match )
	{
		match->AddCTScore( Points() );
	}
}

void CGameScore::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}

BEGIN_ENT_SCRIPTDESC( CGameCoopMissionManager, CBaseEntity, "game_coopmission_manager" )
DEFINE_SCRIPTFUNC_NAMED( GetWaveNumber, "GetWaveNumber", "Get the number of waves the players have completed" )
END_SCRIPTDESC()

LINK_ENTITY_TO_CLASS( game_coopmission_manager, CGameCoopMissionManager );

IMPLEMENT_REFLECT_DATAMAP( CGameCoopMissionManager )

void CGameCoopMissionManager::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
}

bool CGameCoopMissionManager::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

int	CGameCoopMissionManager::GetWaveNumber( void )
{
	if ( !CSGameRules() )
		return 0;

	return CSGameRules()->GetCoopWaveNumber();
}

void CGameCoopMissionManager::SetWaveCompleted( void )
{
	// send output
	m_OnWaveCompleted.FireOutput( nullptr, nullptr );
}

void CGameCoopMissionManager::SetRoundReset( void )
{
	// send output
	m_OnRoundReset.FireOutput( nullptr, nullptr );
}

void CGameCoopMissionManager::SetSpawnsReset( void )
{
	// send output
	m_OnSpawnsReset.FireOutput( nullptr, nullptr );
}

void CGameCoopMissionManager::SetRoundLostKilled( void )
{
	// send output
	m_OnRoundLostKilled.FireOutput( nullptr, nullptr );
}

void CGameCoopMissionManager::SetRoundLostTime( void )
{
	// send output
	m_OnRoundLostTime.FireOutput( nullptr, nullptr );
}

void CGameCoopMissionManager::SetMissionCompleted( void )
{
	// send output
	m_OnMissionCompleted.FireOutput( nullptr, nullptr );
}

// void CGameCoopMissionManager::InputApplyScore( inputdata_t &inputdata )
// {
// 	CBaseEntity *pActivator = inputdata.pActivator;
// 
// 	if ( pActivator == NULL )
// 		return;
// 
// 	if ( CanFireForActivator( pActivator ) == false )
// 		return;
// 
// 	// Only players can use this
// 	if ( pActivator->IsPlayer() )
// 	{
// 		if ( AwardToTeam() )
// 		{
// 			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
// 		}
// 		else
// 		{
// 			pActivator->AddPoints( Points(), AllowNegativeScore() );
// 		}
// 	}
// }

// CGameMoney / game_money	-- award money to player / team 

class CGameMoney : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameMoney, CRulePointEntity );
	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	inline	int		Money( void ) { return m_nMoney; }

	[[= ks::reflect::Input{ .name = "SetMoneyAmount", .type = FIELD_INTEGER } ]] void InputSetMoneyAmount( inputdata_t &inputdata );

	void InputSetTeamMoneyTerrorist( inputdata_t &inputdata );
	void InputSetTeamMoneyCT( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "AddTeamMoneyTerrorist", .type = FIELD_VOID } ]] void InputAddTeamMoneyTerrorist( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddTeamMoneyCT", .type = FIELD_VOID } ]] void InputAddTeamMoneyCT( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "AddMoneyPlayer", .type = FIELD_VOID } ]] void InputAddMoneyPlayer( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "Money" } ]] int m_nMoney;
	[[= ks::reflect::Key{ .name = "AwardText" } ]] string_t		m_strAwardText;
};

LINK_ENTITY_TO_CLASS( game_money, CGameMoney );

IMPLEMENT_REFLECT_DATAMAP( CGameMoney )

void CGameMoney::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
}

void CGameMoney::InputSetMoneyAmount( inputdata_t &inputdata )
{
	int nMoney = inputdata.value.Int();

	m_nMoney = nMoney;
}

void CGameMoney::InputAddMoneyPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pActivator = inputdata.pActivator;

	if ( pActivator == nullptr )
		return;

// 	if ( CanFireForActivator( pActivator ) == false )
// 		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );
		CCSPlayer *pPlayer = dynamic_cast<CCSPlayer*>( inputdata.pActivator );	
		pPlayer->AddAccount( nMoney, true, false );
	}
}

void CGameMoney::InputAddTeamMoneyTerrorist( inputdata_t &inputdata )
{
	int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );

	CSGameRules()->AddTeamAccount( TEAM_TERRORIST, TeamCashAward::CUSTOM_AWARD, nMoney, STRING(m_strAwardText) );
}

void CGameMoney::InputAddTeamMoneyCT( inputdata_t &inputdata )
{
	int nMoney = clamp( m_nMoney, 0, CSGameRules()->GetMaxMoney() );

	CSGameRules()->AddTeamAccount( TEAM_CT, TeamCashAward::CUSTOM_AWARD, nMoney, STRING(m_strAwardText) );
}

// CGameEnd / game_end	-- Ends the game in MP

class CGameEnd : public CRulePointEntity
{
	DECLARE_CLASS( CGameEnd, CRulePointEntity );

public:
	DECLARE_DATADESC();

	[[= ks::reflect::Input{ .name = "EndGame", .type = FIELD_VOID } ]] void	InputGameEnd( inputdata_t &inputdata );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

IMPLEMENT_REFLECT_DATAMAP( CGameEnd )

LINK_ENTITY_TO_CLASS( game_end, CGameEnd );

void CGameEnd::InputGameEnd( inputdata_t &inputdata )
{
	g_pGameRules->EndMultiplayerGame();
}

void CGameEnd::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	g_pGameRules->EndMultiplayerGame();
}

// CGameEnd / game_round_end	-- Ends the round in MP

class CGameRoundEnd : public CRulePointEntity , public CGameEventListener
{
	DECLARE_CLASS( CGameRoundEnd, CRulePointEntity );

public:
	DECLARE_DATADESC();

	CGameRoundEnd();
	void FireGameEvent( IGameEvent *event );

	[[= ks::reflect::Input{ .name = "EndRound_Draw", .type = FIELD_FLOAT } ]] void	InputEndRound_Draw( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EndRound_TerroristsWin", .type = FIELD_FLOAT } ]] void	InputEndRound_TerroristsWin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EndRound_CounterTerroristsWin", .type = FIELD_FLOAT } ]] void	InputEndRound_CounterTerroristsWin( inputdata_t &inputdata );
private:
	[[= ks::reflect::Key{ .name = "OnRoundEnded" } ]] COutputEvent	m_OnRoundEnded;
	//m_OnForcedInteractionFinished.FireOutput( this, this );
};

IMPLEMENT_REFLECT_DATAMAP( CGameRoundEnd )

LINK_ENTITY_TO_CLASS( game_round_end, CGameRoundEnd );

CGameRoundEnd::CGameRoundEnd()
{
	ListenForGameEvent( "round_end" );
}

void CGameRoundEnd::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();

	if ( V_strcmp( name, "round_end" ) == 0 )
	{
		m_OnRoundEnded.FireOutput( nullptr, nullptr );
	}
}

void CGameRoundEnd::InputEndRound_Draw( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, Round_Draw );
}

void CGameRoundEnd::InputEndRound_TerroristsWin( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, Terrorists_Win );
}

void CGameRoundEnd::InputEndRound_CounterTerroristsWin( inputdata_t &inputdata )
{
	float flDelay = inputdata.value.Float();
	CSGameRules()->IncrementAndTerminateRound( flDelay, CTs_Win );
}

//
// CGameText / game_text	-- NON-Localized HUD Message (use env_message to display a titles.txt message)
//	Flag: All players					SF_ENVTEXT_ALLPLAYERS
//
static constexpr auto SF_ENVTEXT_ALLPLAYERS = 0x0001;

class CGameText : public CRulePointEntity
{
	[[= ks::reflect::KeyFrom<"m_textParms.channel", ks::reflect::Key{ .name = "channel" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.x", ks::reflect::Key{ .name = "x" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.y", ks::reflect::Key{ .name = "y" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.effect", ks::reflect::Key{ .name = "effect" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.fadeinTime", ks::reflect::Key{ .name = "fadein" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.fadeoutTime", ks::reflect::Key{ .name = "fadeout" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.holdTime", ks::reflect::Key{ .name = "holdtime" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_textParms.fxTime", ks::reflect::Key{ .name = "fxtime" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CGameText, CRulePointEntity );

	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	DECLARE_DATADESC();

	inline	bool	MessageToAll( void ) { return (m_spawnflags & SF_ENVTEXT_ALLPLAYERS); }
	inline	void	MessageSet( const char *pMessage ) { m_iszMessage = AllocPooledString(pMessage); }
	inline	const char *MessageGet( void )	{ return STRING( m_iszMessage ); }

	[[= ks::reflect::Input{ .name = "Display", .type = FIELD_VOID } ]] void InputDisplay( inputdata_t &inputdata );
	void Display( CBaseEntity *pActivator );
	[[= ks::reflect::Input{ .name = "SetText", .type = FIELD_STRING } ]] void InputSetText ( inputdata_t &inputdata );
	void SetText( const char* pszStr );
	[[= ks::reflect::Input{ .name = "SetPosX", .type = FIELD_FLOAT } ]] void InputSetPosX( inputdata_t &inputdata );
	void SetPosX( float flPosX );
	[[= ks::reflect::Input{ .name = "SetPosY", .type = FIELD_FLOAT } ]] void InputSetPosY( inputdata_t &inputdata );
	void SetPosY( float flPosY );
	[[= ks::reflect::Input{ .name = "SetTextColor", .type = FIELD_COLOR32 } ]] void InputSetTextColor( inputdata_t &inputdata );
	void SetTextColor( color32 color );
	[[= ks::reflect::Input{ .name = "SetTextColor2", .type = FIELD_COLOR32 } ]] void InputSetTextColor2( inputdata_t &inputdata );
	void SetTextColor2( color32 color );

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		Display( pActivator );
	}

private:

	[[= ks::reflect::Key{ .name = "message" } ]] string_t m_iszMessage;
	hudtextparms_t	m_textParms;
};

LINK_ENTITY_TO_CLASS( game_text, CGameText );

// Save parms as a block.  Will break save/restore if the structure changes, but this entity didn't ship with Half-Life, so
// it can't impact saved Half-Life games.
IMPLEMENT_REFLECT_DATAMAP( CGameText )

bool CGameText::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "color"))
	{
		int color[4];
		V_StringToIntArray( color, 4, szValue );
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
	}
	else if (FStrEq(szKeyName, "color2"))
	{
		int color[4];
		V_StringToIntArray( color, 4, szValue );
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

void CGameText::InputDisplay( inputdata_t &inputdata )
{
	Display( inputdata.pActivator );
}

void CGameText::Display( CBaseEntity *pActivator )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( MessageToAll() )
	{
		UTIL_HudMessageAll( m_textParms, MessageGet() );
	}
	else
	{
		// If we're in singleplayer, show the message to the player.
		if ( gpGlobals->maxClients == 1 )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			UTIL_HudMessage( pPlayer, m_textParms, MessageGet() );
		}
		// Otherwise show the message to the player that triggered us.
		else if ( pActivator && pActivator->IsNetClient() )
		{
			UTIL_HudMessage( ToBasePlayer( pActivator ), m_textParms, MessageGet() );
		}
	}
}

void CGameText::InputSetText( inputdata_t &inputdata )
{
	SetText( inputdata.value.String() );
}

void CGameText::SetText( const char* pszStr )
{
	m_iszMessage = AllocPooledString( pszStr );
}

void CGameText::InputSetPosX( inputdata_t &inputdata )
{
	SetPosX( inputdata.value.Float() );
}

void CGameText::SetPosX( float flPosX )
{
	m_textParms.x = flPosX;
}

void CGameText::InputSetPosY( inputdata_t &inputdata )
{
	SetPosY( inputdata.value.Float() );
}

void CGameText::SetPosY( float flPosY )
{
	m_textParms.y = flPosY;
}

void CGameText::InputSetTextColor( inputdata_t &inputdata )
{
	SetTextColor( inputdata.value.Color32() );
}

void CGameText::SetTextColor( color32 color )
{
	m_textParms.r1 = color.r;
	m_textParms.g1 = color.g;
	m_textParms.b1 = color.b;
	m_textParms.a1 = color.a;
}

void CGameText::InputSetTextColor2( inputdata_t &inputdata )
{
	SetTextColor2( inputdata.value.Color32() );
}

void CGameText::SetTextColor2( color32 color )
{
	m_textParms.r2 = color.r;
	m_textParms.g2 = color.g;
	m_textParms.b2 = color.b;
	m_textParms.a2 = color.a;
}

/* TODO: Replace with an entity I/O version
//
// CGameTeamSet / game_team_set	-- Changes the team of the entity it targets to the activator's team
// Flag: Fire once
// Flag: Clear team				-- Sets the team to "NONE" instead of activator

static constexpr auto SF_TEAMSET_FIREONCE = 0x0001;
static constexpr auto SF_TEAMSET_CLEARTEAM = 0x0002;

class CGameTeamSet : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameTeamSet, CRulePointEntity );

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_TEAMSET_FIREONCE) ? true : false; }
	inline bool ShouldClearTeam( void ) { return (m_spawnflags & SF_TEAMSET_CLEARTEAM) ? true : false; }
	void InputTrigger( inputdata_t &inputdata );

private:
	COutputEvent m_OnTrigger;
};

LINK_ENTITY_TO_CLASS( game_team_set, CGameTeamSet );

void CGameTeamSet::InputTrigger( inputdata_t &inputdata )
{
	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	if ( ShouldClearTeam() )
	{
		// clear the team of our target
	}
	else
	{
		// set the team of our target to our activator's team
	}

	m_OnTrigger.FireOutput(pActivator, this);

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/

//
// CGamePlayerZone / game_player_zone -- players in the zone fire my target when I'm fired
//
// Needs master?
class CGamePlayerZone : public CRuleBrushEntity
{
public:
	DECLARE_CLASS( CGamePlayerZone, CRuleBrushEntity );
	[[= ks::reflect::Input{ .name = "CountPlayersInZone", .type = FIELD_VOID } ]] void InputCountPlayersInZone( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	[[= ks::reflect::Key{ .name = "OnPlayerInZone" } ]] COutputEvent m_OnPlayerInZone;
	[[= ks::reflect::Key{ .name = "OnPlayerOutZone" } ]] COutputEvent m_OnPlayerOutZone;

	[[= ks::reflect::Key{ .name = "PlayersInCount" } ]] COutputInt m_PlayersInCount;
	[[= ks::reflect::Key{ .name = "PlayersOutCount" } ]] COutputInt m_PlayersOutCount;
};

LINK_ENTITY_TO_CLASS( game_zone_player, CGamePlayerZone );
IMPLEMENT_REFLECT_DATAMAP( CGamePlayerZone )

//-----------------------------------------------------------------------------
// Purpose: Counts all the players in the zone. Fires one output per player
//			in the zone, one output per player out of the zone, and outputs
//			with the total counts of players in and out of the zone.
//-----------------------------------------------------------------------------
void CGamePlayerZone::InputCountPlayersInZone( inputdata_t &inputdata )
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	CBaseEntity *pPlayer = nullptr;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			trace_t		trace;
			UTIL_TraceModel( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), pPlayer->WorldAlignMins(),
				pPlayer->WorldAlignMaxs(), this, COLLISION_GROUP_NONE, &trace );

			if ( trace.startsolid )
			{
				playersInCount++;
				m_OnPlayerInZone.FireOutput(pPlayer, this);
			}
			else
			{
				playersOutCount++;
				m_OnPlayerOutZone.FireOutput(pPlayer, this);
			}
		}
	}

	m_PlayersInCount.Set(playersInCount, inputdata.pActivator, this);
	m_PlayersOutCount.Set(playersOutCount, inputdata.pActivator, this);
}

/*
// Disable.  Eventually will be replace by new activator filter entities.  (LHL)
//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once

static constexpr auto SF_PKILL_FIREONCE = 0x0001;
class CGamePlayerHurt : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerHurt, CRulePointEntity );

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PKILL_FIREONCE) ? true : false; }

	DECLARE_DATADESC();

private:
	
	[[= ks::reflect::Key{ .name = "dmg" } ]] float m_flDamage;		// Damage to inflict, negative values give health.

	COutputEvent m_OnUse;
};

LINK_ENTITY_TO_CLASS( game_player_hurt, CGamePlayerHurt );

IMPLEMENT_REFLECT_DATAMAP( CGamePlayerHurt )

void CGamePlayerHurt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( m_flDamage < 0 )
		{
			pActivator->TakeHealth( -m_flDamage, DMG_GENERIC );
		}
		else
		{
			pActivator->TakeDamage( this, this, m_flDamage, DMG_GENERIC );
		}
	}
	
	SUB_UseTargets( pActivator, useType, value );
	m_OnUse.FireOutput(pActivator, this); // dvsents2: handle useType and value here - they are passed through

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/

//
// CGamePlayerEquip / game_playerequip	-- Sets the default player equipment
// Flag: USE Only

LINK_ENTITY_TO_CLASS( game_player_equip, CGamePlayerEquip );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CGamePlayerEquip )

void CGamePlayerEquip::InputTriggerForAllPlayers( inputdata_t &inputdata )
{
	TriggerForAllPlayers();
}

void CGamePlayerEquip::InputTriggerForActivatedPlayer( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = entity_cast<CBasePlayer>( inputdata.pActivator );
	if ( pPlayer )
		TriggerForActivatedPlayer( pPlayer, inputdata.value.String() );
}

bool CGamePlayerEquip::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue_Impl( szKeyName, szValue ) )
	{
		for ( int i = 0; i < MAX_EQUIP; i++ )
		{
			if ( !m_weaponNames[i] )
			{
				char tmp[128];

				UTIL_StripToken( szKeyName, tmp );

				m_weaponNames[i] = AllocPooledString(tmp);
				m_weaponCount[i] = std::atoi(szValue);
				m_weaponCount[i] = std::max(0,m_weaponCount[i]);
				return true;
			}
		}
	}

	return false;
}

void CGamePlayerEquip::TriggerForAllPlayers( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			if ( CanFireForActivator( pToPlayer ) )
			{
				EquipPlayer( pToPlayer );
			}
		}
	}
}

void CGamePlayerEquip::TriggerForActivatedPlayer( CBasePlayer *pPlayer, const char *szWeapon )
{
	if ( pPlayer )
	{
		if ( CanFireForActivator( pPlayer ) )
		{
			EquipPlayer( pPlayer, szWeapon );
		}
	}
}

void CGamePlayerEquip::Touch_Impl( CBaseEntity *pOther )
{
	if ( !CanFireForActivator( pOther ) )
		return;

	if ( UseOnly() )
		return;

	EquipPlayer( pOther );
}

void CGamePlayerEquip::EquipPlayer( CBaseEntity *pEntity, const char *szWeapon )
{
	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = nullptr;

	if ( pEntity->IsPlayer() )
	{
		pPlayer = (CBasePlayer *)pEntity;
	}

	if ( !pPlayer )
		return;

	if ( StripFirst() )
	{
		// remove all our weapons and armor
		pPlayer->RemoveAllItems( true );
	}

	const char *weaponName = szWeapon;

	int nMaxLoop = MAX_EQUIP;
	if ( szWeapon != nullptr )
		nMaxLoop = 1;

	for ( int i = 0; i < MAX_EQUIP; i++ )
	{
		if ( szWeapon == nullptr && !m_weaponNames[i] )
			break;

		if ( szWeapon == nullptr )
			weaponName = STRING( m_weaponNames[i] );

		CSWeaponID weaponID = WeaponIdFromString( weaponName );

		CCSPlayer *pCSPlayer = static_cast<CCSPlayer*>( pPlayer );	

		// if it's a grenade and we don't have it, give it
		// if we do have it, don't do anything because you can only carry one of each grenade
		// TODO: if we change how many grenades you can carry, this code needs to cover that, this is poor code otherwise
		if ( pCSPlayer && IsGrenadeWeapon( weaponID ) )
		{
			if ( !pCSPlayer->Weapon_OwnsThisType( weaponName ) )
			{
				AcquireResult::Type acquireResult = pCSPlayer->CanAcquire( weaponID, AcquireMethod::PickUp, nullptr );
				if ( acquireResult == AcquireResult::Allowed )
					pCSPlayer->GiveNamedItem( weaponName );
			}
		}
		else
		{
			if ( OnlyStripSameWeaponType() )
			{
				if ( weaponID == WEAPON_NONE )
				{
					for ( int i = GGLIST_PISTOLS_START; i < ( GGLIST_SNIPERS_LAST ); i++ )
					{
						const char *item_name = weaponName;
						if ( IsWeaponClassname( item_name ) )
						{
							item_name += WEAPON_CLASSNAME_PREFIX_LENGTH;
						}

						if ( V_strcmp( ggWeaponAliasNameList[i].aliasName, item_name ) == 0 )
						{
							weaponID = ggWeaponAliasNameList[i].id;
							break;
						}		
					}
				}

				if ( IsPrimaryWeapon( weaponID ) )
				{
					CBaseCombatWeapon *pPrimary = pPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE );
					if ( pPrimary )
						pPlayer->RemoveWeaponOnPlayer( pPrimary );
				}
				else if ( IsSecondaryWeapon( weaponID ) )
				{
					CBaseCombatWeapon *pSecondary = pPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL );
					if ( pSecondary )
						pPlayer->RemoveWeaponOnPlayer( pSecondary );
				}
			}

			pPlayer->GiveNamedItem( weaponName );
			if ( const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponID ) )
			{
				int nType = pWeaponInfo->GetPrimaryAmmoType();
				pPlayer->GiveAmmo( m_weaponCount[ i ], nType );
			}
		}
	}
}

void CGamePlayerEquip::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EquipPlayer( pActivator ); // note: pActivator may sometimes be NULL
}

//
// CGamePlayerTeam / game_player_team	-- Changes the team of the player who fired it
// Flag: Fire once
// Flag: Kill Player
// Flag: Gib Player

static constexpr auto SF_PTEAM_FIREONCE = 0x0001;
static constexpr auto SF_PTEAM_KILL = 0x0002;
static constexpr auto SF_PTEAM_GIB = 0x0004;

class CGamePlayerTeam : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerTeam, CRulePointEntity );

	void		Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PTEAM_FIREONCE) ? true : false; }
	inline bool ShouldKillPlayer( void ) { return (m_spawnflags & SF_PTEAM_KILL) ? true : false; }
	inline bool ShouldGibPlayer( void ) { return (m_spawnflags & SF_PTEAM_GIB) ? true : false; }
	
	const char *TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator );
};

LINK_ENTITY_TO_CLASS( game_player_team, CGamePlayerTeam );

const char *CGamePlayerTeam::TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator )
{
	CBaseEntity *pTeamEntity = nullptr;

	while ((pTeamEntity = gEntList.FindEntityByName( pTeamEntity, pszTargetName, nullptr, pActivator )) != nullptr)
	{
		if ( FClassnameIs( pTeamEntity, "game_team_master" ) )
			return pTeamEntity->TeamID();
	}

	return nullptr;
}

void CGamePlayerTeam::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		const char *pszTargetTeam = TargetTeamName( STRING(m_target), pActivator );
		if ( pszTargetTeam )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
			g_pGameRules->ChangePlayerTeam( pPlayer, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer() );
		}
	}
	
	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Material modify control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight.
//------------------------------------------------------------------------------

static constexpr auto MATERIAL_MODIFY_STRING_SIZE = 255;
static constexpr auto MATERIAL_MODIFY_ANIMATION_UNSET = -1;

// Must match C_MaterialModifyControl.cpp
enum MaterialModifyMode_t
{
	MATERIAL_MODIFY_MODE_NONE = 0,
	MATERIAL_MODIFY_MODE_SETVAR = 1,
	MATERIAL_MODIFY_MODE_ANIM_SEQUENCE = 2,
	MATERIAL_MODIFY_MODE_FLOAT_LERP = 3,
};

ConVar debug_materialmodifycontrol( side_registry(), "debug_materialmodifycontrol", "0" );

class CMaterialModifyControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_MaterialModifyControl" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CMaterialModifyControl, CBaseEntity );

	CMaterialModifyControl();

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int UpdateTransmitState_Impl();
	int ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

	[[= ks::reflect::Input{ .name = "SetMaterialVar", .type = FIELD_STRING } ]] void SetMaterialVar( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaterialVarToCurrentTime", .type = FIELD_VOID } ]] void SetMaterialVarToCurrentTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartAnimSequence", .type = FIELD_STRING } ]] void InputStartAnimSequence( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartFloatLerp", .type = FIELD_STRING } ]] void InputStartFloatLerp( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkString( m_szMaterialName, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szMaterialVar, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szMaterialVarValue, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iFrameStart, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( int, m_iFrameEnd, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( bool, m_bWrap, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFramerate, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( bool, m_bNewAnimCommandsSemaphore, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFloatLerpStartValue, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFloatLerpEndValue, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFloatLerpTransitionTime, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nModifyMode, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
};

LINK_ENTITY_TO_CLASS(material_modify_control, CMaterialModifyControl);

IMPLEMENT_REFLECT_DATAMAP( CMaterialModifyControl )

IMPLEMENT_REFLECT_SERVERCLASS( CMaterialModifyControl, DT_MaterialModifyControl )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMaterialModifyControl::CMaterialModifyControl()
{
	m_iFrameStart = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_iFrameEnd = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_nModifyMode = MATERIAL_MODIFY_MODE_NONE;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CMaterialModifyControl::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
bool CMaterialModifyControl::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "materialName" ) )
	{
		V_strncpy( m_szMaterialName.GetForModify(), szValue, MATERIAL_MODIFY_STRING_SIZE );
		return true;
	}

	if ( FStrEq( szKeyName, "materialVar" ) )
	{
		V_strncpy( m_szMaterialVar.GetForModify(), szValue, MATERIAL_MODIFY_STRING_SIZE );
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model.
//------------------------------------------------------------------------------
int CMaterialModifyControl::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// Send if the parent is being sent:
//-----------------------------------------------------------------------------
int CMaterialModifyControl::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
{
	CBaseEntity *pEnt = GetMoveParent();
	if ( pEnt )
	{
		return pEnt->ShouldTransmit( pInfo );
	}
	
	return FL_EDICT_DONTSEND;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialModifyControl::SetMaterialVar( inputdata_t &inputdata )
{
	//if( debug_materialmodifycontrol.GetBool() && V_stristr( GetDebugName(), "alyx" ) )
	//{
		//DevMsg( 1, "CMaterialModifyControl::SetMaterialVar %s %s %s=\"%s\"\n", 
			//GetDebugName(), m_szMaterialName.Get(), m_szMaterialVar.Get(), inputdata.value.String() );
	//}
	V_strncpy( m_szMaterialVarValue.GetForModify(), inputdata.value.String(), MATERIAL_MODIFY_STRING_SIZE );
	m_nModifyMode = MATERIAL_MODIFY_MODE_SETVAR;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::SetMaterialVarToCurrentTime( inputdata_t &inputdata )
{
	char temp[32];
	V_snprintf( temp, 32, "%f", gpGlobals->curtime );
	V_strncpy( m_szMaterialVarValue.GetForModify(), temp, MATERIAL_MODIFY_STRING_SIZE );
	m_nModifyMode = MATERIAL_MODIFY_MODE_SETVAR;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::InputStartAnimSequence( inputdata_t &inputdata )
{
	char parseString[255];
	V_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get the start & end frames
	char *pszParam = std::strtok(parseString," ");
	if ( pszParam && pszParam[0] )
	{
		int iFrameStart = std::atoi(pszParam);

		pszParam = strtok(nullptr," ");
		if ( pszParam && pszParam[0] )
		{
			int iFrameEnd = atoi(pszParam);

			pszParam = strtok(nullptr," ");
			if ( pszParam && pszParam[0] )
			{
				float flFramerate = std::atof(pszParam);

				pszParam = strtok(nullptr," ");
				if ( pszParam && pszParam[0] )
				{
					bool bWrap = atoi(pszParam) != 0;

					// Got all the parameters. Save 'em and return;
					m_iFrameStart = iFrameStart;
					m_iFrameEnd = iFrameEnd;
					m_flFramerate = flFramerate;
					m_bWrap = bWrap;
					m_nModifyMode = MATERIAL_MODIFY_MODE_ANIM_SEQUENCE;
					m_bNewAnimCommandsSemaphore = !m_bNewAnimCommandsSemaphore;
					return;
				}
			}
		}
	}

	Warning("%s (%s) received StartAnimSequence input without correct parameters. Syntax: <Frame Start> <Frame End> <Frame Rate> <Loop>\nSetting <Frame End> to -1 uses the last frame of the texture. <Loop> should be 1 or 0.\n", GetClassname(), GetDebugName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::InputStartFloatLerp( inputdata_t &inputdata )
{
	char parseString[255];
	V_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

//	if( debug_materialmodifycontrol.GetBool() )//&& V_stristr( GetDebugName(), "alyx" ) )
//	{
//		DevMsg( 1, "CMaterialModifyControl::InputStartFloatLerp %s %s %s \"%s\"\n", 
//			GetDebugName(), m_szMaterialName.Get(), m_szMaterialVar.Get(), inputdata.value.String() );
//	}

	// Get the start & end values
	char *pszParam = std::strtok(parseString," ");
	if ( pszParam && pszParam[0] )
	{
		float flStartValue = std::atof(pszParam);

		pszParam = strtok(nullptr," ");
		if ( pszParam && pszParam[0] )
		{
			float flEndValue = atof(pszParam);

			pszParam = strtok(nullptr," ");
			if ( pszParam && pszParam[0] )
			{
				float flTransitionTime = atof(pszParam);

				pszParam = strtok(nullptr," ");
				if ( pszParam && pszParam[0] )
				{
					bool bWrap = std::atoi(pszParam) != 0;
					// We don't implement wrap currently.
					bWrap = bWrap;

					// Got all the parameters. Save 'em and return;
					m_flFloatLerpStartValue = flStartValue;
					m_flFloatLerpEndValue = flEndValue;
					m_flFloatLerpTransitionTime = flTransitionTime;
					m_nModifyMode = MATERIAL_MODIFY_MODE_FLOAT_LERP;
					m_bNewAnimCommandsSemaphore = !m_bNewAnimCommandsSemaphore;
					return;
				}
			}
		}
	}

	Warning("%s (%s) received StartFloatLerp input without correct parameters. Syntax: <Start Value> <End Value> <Transition Time> <Loop>\n<Loop> should be 1 or 0.\n", GetClassname(), GetDebugName() );
}

using namespace ::ks::reflect;

// Purpose: See memorylog.h

// Memory log auto game system instantiation
CMemoryLog g_MemoryLog;

const char *GetMapName( void )
{
	static char mapName[32];
	mapName[0] = 0;
	if ( gpGlobals->mapname.ToCStr() )
		V_strncpy( mapName, gpGlobals->mapname.ToCStr(), sizeof( mapName ) );
	if ( !mapName[ 0 ] )
		V_strncpy( mapName, "none", sizeof( mapName ) );
	return mapName;
}

void CMemoryLog::LevelInitPostEntity( void )
{
//#include "entitylist.h"
}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Spawnflags
static constexpr auto SF_MESSAGE_DISABLED = 1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMessageEntity : public CPointEntity
{
	DECLARE_CLASS( CMessageEntity, CPointEntity );

public:
	void	Spawn_Impl( void );
	void	Activate_Impl( void );
	void	Think_Impl( void );
	void	DrawOverlays(void);

	void UpdateOnRemove_Impl();

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:
	[[= ks::reflect::Key{ .name = "radius" } ]] int				m_radius;
	[[= ks::reflect::Key{ .name = "message" } ]] string_t		m_messageText;
	bool			m_drawText;
	[[= ks::reflect::Key{ .name = "developeronly" } ]] bool			m_bDeveloperOnly;
	bool			m_bEnabled;
};

LINK_ENTITY_TO_CLASS( point_message, CMessageEntity );

IMPLEMENT_REFLECT_DATAMAP( CMessageEntity )

static std::vector< CHandle< CMessageEntity > >	g_MessageEntities;

//-----------------------------------------
// Spawn
//-----------------------------------------
void CMessageEntity::Spawn_Impl( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_drawText = false;
	m_bDeveloperOnly = false;
	m_bEnabled = !HasSpawnFlags( SF_MESSAGE_DISABLED );
	//m_debugOverlays |= OVERLAY_TEXT_BIT;		// make sure we always show the text
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CHandle< CMessageEntity > h;
	h = this;
	g_MessageEntities.push_back( h );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::UpdateOnRemove_Impl()
{
	BaseClass::UpdateOnRemove_Impl();

	CHandle< CMessageEntity > h;
	h = this;
	ks::erase_first( g_MessageEntities, h );

	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------
// Think
//-----------------------------------------
void CMessageEntity::Think_Impl( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	// check for player distance
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( !pPlayer || ( pPlayer->GetFlags() & FL_NOTARGET ) )
		return;

	Vector worldTargetPosition = pPlayer->EyePosition();

	// bail if player is too far away
	if ( (worldTargetPosition - GetAbsOrigin()).Length() > m_radius )
	{
		m_drawText = false;
		return;
	}

	// turn on text
	m_drawText = true;
}
	
//-------------------------------------------
//-------------------------------------------
void CMessageEntity::DrawOverlays(void) 
{
	if ( !m_drawText )
		return;

	if ( m_bDeveloperOnly && !g_pDeveloper->GetInt() )
		return;

	if ( !m_bEnabled )
		return;

	// display text if they are within range
	char tempstr[512];
	V_snprintf( tempstr, sizeof(tempstr), "%s", STRING(m_messageText) );
	EntityText( 0, tempstr, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

// This is a hack to make point_message stuff appear in developer 0 release builds
//  for now
void DrawMessageEntities()
{
	int c = std::ssize( g_MessageEntities );
	for ( int i = c - 1; i >= 0; i-- )
	{
		CMessageEntity *me = g_MessageEntities[ i ];
		if ( !me )
		{
			g_MessageEntities.erase( g_MessageEntities.begin() + i );
			continue;
		}

		me->DrawOverlays();
	}
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

extern ConVar ent_debugkeys;
extern ConVar	showtriggers;

LINK_ENTITY_TO_CLASS( func_brush, CFuncBrush );

IMPLEMENT_REFLECT_DATAMAP( CFuncBrush )

IMPLEMENT_REFLECT_SERVERCLASS( CFuncBrush, DT_FuncBrush )

void CFuncBrush::Spawn_Impl( void )
{
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything

	SetSolid( SOLID_VPHYSICS );
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	if ( m_iSolidity == BRUSHSOLID_NEVER )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	SetModel( STRING( GetModelName() ) );

	if ( m_iDisabled )
		TurnOff();
	
	// If it can't move/go away, it's really part of the world
	if ( !GetEntityName() || !m_iParent )
		AddFlag( FL_WORLDBRUSH );

	CreateVPhysics();

	// Slam the object back to solid - if we really want it to be solid.
	if ( m_bSolidBsp )
	{
		SetSolid( SOLID_BSP );
	}
}

void CFuncBrush::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	IPhysicsObject *pPhysObject = VPhysicsGetObject();
	if( pPhysObject )
	{
		pPhysObject->Wake();
	}
}

//-----------------------------------------------------------------------------

bool CFuncBrush::CreateVPhysics_Impl( void )
{

	// NOTE: Don't init this static.  It's pretty common for these to be constrained
	// and dynamically parented.  Initing shadow avoids having to destroy the physics
	// object later and lose the constraints.
	IPhysicsObject *pPhys = VPhysicsInitShadow(false, false);
	if ( pPhys )
	{
		int contents = modelinfo->GetModelContents( GetModelIndex() );
		if ( m_iDisabled || !(contents & (MASK_SOLID|MASK_PLAYERSOLID|MASK_NPCSOLID)) )
		{
			// leave the physics shadow there in case it has crap constrained to it
			// but disable collisions with it
			pPhys->EnableCollisions( false );
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFuncBrush::DrawDebugTextOverlays_Impl( void )
{
	int nOffset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		V_snprintf( tempstr,sizeof(tempstr), "angles: %g %g %g", (double)GetLocalAngles()[PITCH], (double)GetLocalAngles()[YAW], (double)GetLocalAngles()[ROLL] );
		EntityText( nOffset, tempstr, 0 );
		nOffset++;
	}

	return nOffset;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the hidden/shown state of the brush.
//-----------------------------------------------------------------------------
void CFuncBrush::InputToggle( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
		return;
	}

	TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for hiding the brush.
//-----------------------------------------------------------------------------
void CFuncBrush::InputTurnOff( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the brush.
//-----------------------------------------------------------------------------
void CFuncBrush::InputTurnOn( inputdata_t &inputdata )
{
	TurnOn();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CFuncBrush::InputSetExcluded( inputdata_t &inputdata )
{
	m_iszExcludedClass = inputdata.value.StringID();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CFuncBrush::InputSetInvert( inputdata_t &inputdata )
{
	m_bInvertExclusion = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the brush.
//-----------------------------------------------------------------------------
void CFuncBrush::TurnOff( void )
{
	if ( !IsOn() )
		return;

	if ( m_iSolidity != BRUSHSOLID_ALWAYS )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	AddEffects( EF_NODRAW );

	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		pObject->Wake();
		pObject->EnableCollisions( false );
	}

	WakeRestingObjects();	

	m_iDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Shows the brush.
//-----------------------------------------------------------------------------
void CFuncBrush::TurnOn( void )
{
	if ( IsOn() )
		return;

	if ( m_iSolidity != BRUSHSOLID_NEVER )
	{
		RemoveSolidFlags( FSOLID_NOT_SOLID );
	}

	RemoveEffects( EF_NODRAW );

	IPhysicsObject *pObject = VPhysicsGetObject();
	if( pObject )
	{
		pObject->EnableCollisions( true );
		if( pObject->IsAsleep() )
		{
			pObject->Wake();
		}		
	}

	m_iDisabled = false;
}

bool CFuncBrush::IsOn( void )
{
	return !IsEffectActive( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: Invisible field that activates when touched
//			All inputs are passed up to the main entity, unless filtered out
//-----------------------------------------------------------------------------
// DVS TODO: obsolete, remove
class CTriggerBrush : public CBaseEntity
{
	//
	// Filters controlling what this trigger responds to.
	//
	enum TriggerFilters_e
	{
		TRIGGER_IGNOREPLAYERS	= 0x01,
		TRIGGER_IGNORENPCS		= 0x02,
		TRIGGER_IGNOREPUSHABLES	= 0x04,
		TRIGGER_IGNORETOUCH		= 0x08,
		TRIGGER_IGNOREUSE		= 0x10,
		TRIGGER_IGNOREALL		= 0x20,
	};

public:
	DECLARE_CLASS( CTriggerBrush, CBaseEntity );

	// engine inputs
	void Spawn_Impl( void );
	void StartTouch_Impl( CBaseEntity *pOther );
	void EndTouch_Impl( CBaseEntity *pOther );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// input filtering (use/touch/blocked)
	bool PassesInputFilter( CBaseEntity *pOther, int filter );

	// input functions
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata )
	{
		RemoveFlag( FL_DONTTOUCH );
	}

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata )
	{
		// this ensures that all the remaining EndTouch() calls still get passed through
		AddFlag( FL_DONTTOUCH );
	}

	// outputs
	[[= ks::reflect::Key{ .name = "OnStartTouch" } ]] COutputEvent m_OnStartTouch;
	[[= ks::reflect::Key{ .name = "OnEndTouch" } ]] COutputEvent m_OnEndTouch;
	[[= ks::reflect::Key{ .name = "OnUse" } ]] COutputEvent m_OnUse;

	// data
	[[= ks::reflect::Key{ .name = "InputFilter" } ]] int m_iInputFilter;
	[[= ks::reflect::Key{ .name = "DontMessageParent" } ]] int m_iDontMessageParent;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_brush, CTriggerBrush );

IMPLEMENT_REFLECT_DATAMAP( CTriggerBrush )

void CTriggerBrush::Spawn_Impl( void )
{
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );

	SetModel( STRING( GetModelName() ) );    // set size and link into world

	if ( !showtriggers.GetInt() )
	{
		AddEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when an entity starts touching us.
// Input  : pOther - the entity that is now touching us.
//-----------------------------------------------------------------------------
void CTriggerBrush::StartTouch_Impl( CBaseEntity *pOther )
{
	if ( PassesInputFilter(pOther, m_iInputFilter) && !(m_iInputFilter & TRIGGER_IGNORETOUCH) )
	{
		m_OnStartTouch.FireOutput( pOther, this );
		if ( !m_iDontMessageParent )
			BaseClass::StartTouch_Impl( pOther );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when an entity stops touching us.
// Input  : pOther - the entity that was touching us.
//-----------------------------------------------------------------------------
void CTriggerBrush::EndTouch_Impl( CBaseEntity *pOther )
{
	if ( PassesInputFilter(pOther, m_iInputFilter) && !(m_iInputFilter & TRIGGER_IGNORETOUCH) )
	{
		m_OnEndTouch.FireOutput( pOther, this );

		if ( !m_iDontMessageParent )
			BaseClass::EndTouch_Impl( pOther );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when we are triggered by another entity or used by the player.
// Input  : pActivator -
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CTriggerBrush::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( PassesInputFilter(pActivator, m_iInputFilter) && !(m_iInputFilter & TRIGGER_IGNOREUSE) )
	{
		m_OnUse.FireOutput( pActivator, this );
		if ( !m_iDontMessageParent )
		{
			BaseClass::Use_Impl( pActivator, pCaller, useType, value );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks an entity input, against a filter and the current entity
// Input  : *pOther - the entity that is part of the input (touch/untouch/block/use)
//			filter - a field of standard filters (TriggerFilters_e)
// Output : Returns true if the input passes, false if it should be ignored
//-----------------------------------------------------------------------------
bool CTriggerBrush::PassesInputFilter( CBaseEntity *pOther, int filter )
{
	if ( !filter )
		return true;

	// check for players
	if ( (filter & TRIGGER_IGNOREPLAYERS) && pOther->IsPlayer() )
		return false;

	// NPCs
	if ( (filter & TRIGGER_IGNORENPCS) && pOther->edict() && (pOther->GetFlags() & FL_NPC) )
		return false;

	// pushables
	if ( (filter & TRIGGER_IGNOREPUSHABLES) && FStrEq(STRING(pOther->m_iClassname), "func_pushable") )
		return false;

	return true;
}

using namespace ::ks::reflect;

extern ISoundEmitterSystemBase *soundemitterbase;

CStudioHdr *ModelSoundsCache_LoadModel( char const *filename );
void ModelSoundsCache_PrecacheScriptSound( const char *soundname );
void ModelSoundsCache_FinishModel( CStudioHdr *hdr );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hdr - 
// Output : static void
//-----------------------------------------------------------------------------

void VerifySequenceIndex( CStudioHdr *pstudiohdr );

// HACK:  This must match the #define in cl_animevent.h in the client .dll code!!!
#define CL_EVENT_SOUND				5004
#define CL_EVENT_FOOTSTEP_LEFT		6004
#define CL_EVENT_FOOTSTEP_RIGHT		6005
#define CL_EVENT_MFOOTSTEP_LEFT		6006
#define CL_EVENT_MFOOTSTEP_RIGHT	6007

extern ISoundEmitterSystemBase *soundemitterbase;

CModelSoundsCache::CModelSoundsCache()
{
}

CModelSoundsCache::CModelSoundsCache( const CModelSoundsCache& src )
{
	sounds = src.sounds;
}

char const *CModelSoundsCache::GetSoundName( int index )
{
	return soundemitterbase->GetSoundName( sounds.begin()[ index ] );
}

void CModelSoundsCache::Save( ks::byte_writer& buf )
{
	buf.write< short >( std::ssize( sounds ) );
	
	for ( int i = 0; i < std::ssize( sounds ); ++i )
	{
		buf.write_cstring( GetSoundName( i ) );
	}
}

void CModelSoundsCache::Restore( ks::byte_reader& buf )
{
	unsigned short c;

	c = (unsigned short)buf.read< short >();

	for ( int i = 0; i < c; ++i )
	{
		char soundname[ 512 ];

		buf.read_cstring( soundname, sizeof( soundname ) );

		int idx = soundemitterbase->GetSoundIndex( soundname );
		if ( soundemitterbase->IsValidIndex( idx ) )
		{
			sounds.insert( idx );
		}
	}
}

void CModelSoundsCache::Rebuild( char const *filename )
{
	sounds.clear();

	CStudioHdr *hdr = ModelSoundsCache_LoadModel( filename );

	if ( hdr )
	{
		// Precache all sounds referenced in animation events
		BuildAnimationEventSoundList( hdr, sounds );
		ModelSoundsCache_FinishModel( hdr );
	}
}

void CModelSoundsCache::PrecacheSoundList()
{
	for ( int i = 0; i < std::ssize( sounds ); ++i )
	{
		ModelSoundsCache_PrecacheScriptSound( GetSoundName( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : sounds - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CModelSoundsCache::FindOrAddScriptSound( std::flat_set< int >& sounds, char const *soundname )
{
	int soundindex = soundemitterbase->GetSoundIndex( soundname );
	if ( soundemitterbase->IsValidIndex( soundindex ) )
	{
		// Only add it once per model...
		sounds.insert( soundindex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : *hdr - 
//			sounds - 
//-----------------------------------------------------------------------------
void CModelSoundsCache::BuildAnimationEventSoundList( CStudioHdr *hdr, std::flat_set< int >& sounds )
{
	contract_assert( hdr );
	
	// force animation event resolution!!!
	VerifySequenceIndex( hdr );

	// Find all animation events which fire off sound script entries...
	for ( int iSeq=0; iSeq < hdr->GetNumSeq(); iSeq++ )
	{
		mstudioseqdesc_t *pSeq = &hdr->pSeqdesc( iSeq );
		
		// Now read out all the sound events with their timing
		for ( int iEvent=0; iEvent < (int)pSeq->numevents; iEvent++ )
		{
			mstudioevent_for_client_server_t *pEvent = (mstudioevent_for_client_server_t*)pSeq->pEvent( iEvent );
			
			int nEvent = pEvent->Event();
			
			switch ( nEvent )
			{
			default:
				{
					if ( pEvent->type & AE_TYPE_NEWEVENTSYSTEM )
					{
						if ( nEvent == AE_SV_PLAYSOUND )
						{
							FindOrAddScriptSound( sounds, pEvent->pszOptions() );
						}
					}
				}
				break;
			// Old-style client .dll animation event
			case CL_EVENT_SOUND:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;
			case CL_EVENT_FOOTSTEP_LEFT:
			case CL_EVENT_FOOTSTEP_RIGHT:
				{
					char soundname[256];
					char const *options = pEvent->pszOptions();
					if ( !options || !options[0] )
					{
						options = "NPC_CombineS";
					}

					V_snprintf( soundname, 256, "%s.RunFootstepLeft", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.RunFootstepRight", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.FootstepLeft", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.FootstepRight", options );
					FindOrAddScriptSound( sounds, soundname );
				}
				break;
			case AE_CL_PLAYSOUND:
				{
					if ( !( pEvent->type & AE_TYPE_CLIENT ) )
						break;

					if ( pEvent->pszOptions()[0] )
					{
						FindOrAddScriptSound( sounds, pEvent->pszOptions() );
					}
					else
					{
						Warning( "-- Error --:  empty soundname, .qc error on AE_CL_PLAYSOUND in model %s, sequence %s, animevent # %i\n", 
							hdr->pszName(), pSeq->pszLabel(), iEvent+1 );
					}
				}
				break;
			case SCRIPT_EVENT_SOUND:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;

			case SCRIPT_EVENT_SOUND_VOICE:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;
			}
		}
	}
}

#undef CL_EVENT_SOUND
#undef CL_EVENT_FOOTSTEP_LEFT
#undef CL_EVENT_FOOTSTEP_RIGHT
#undef CL_EVENT_MFOOTSTEP_LEFT
#undef CL_EVENT_MFOOTSTEP_RIGHT

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CRecipientFilter;
class CTakeDamageInfo;

using namespace ::ks::reflect;

extern IPhysicsCollision *physcollision;

//-----------------------------------------------------------------------------
// Implementation of the movehelper on the server
//-----------------------------------------------------------------------------

class CMoveHelperServer : public IMoveHelper
{
public:
	CMoveHelperServer( void );
	virtual ~CMoveHelperServer();

	// Methods associated with a particular entity
	virtual	char const*		GetName( EntityHandle_t handle ) const;

	// Touch list...
	virtual void	ResetTouchList( void );
	virtual bool	AddToTouched( const trace_t &tr, const Vector& impactvelocity );
	virtual void	SetGroundNormal( const Vector& groundNormal );
 	virtual void	ProcessImpacts( void );

	virtual bool	PlayerFallingDamage( void );
	virtual void	PlayerSetAnimation( PLAYER_ANIM eAnim );

	// Numbered line printf
	virtual void	Con_NPrintf( int idx, char const* fmt, ... );
	
	// These have separate server vs client impementations
	virtual void	StartSound( const Vector& origin, int channel, char const* sample, float volume, soundlevel_t soundlevel, int fFlags, int pitch );
	virtual void	StartSound( const Vector& origin, const char *soundname ); 

	virtual void	PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	virtual IPhysicsSurfaceProps *GetSurfaceProps( void );

	void			SetHost( CEntityCore *host );

	virtual bool IsWorldEntity( const CBaseHandle &handle );

private:
	CBaseEntity*	m_pHost;

	// results, tallied on client and server, but only used by server to run SV_Impact.
	// we store off our velocity in the trace_t structure so that we can determine results
	// of shoving boxes etc. around.
	struct touchlist_t
	{
		Vector	deltavelocity;
		trace_t trace;
	};

	std::vector< touchlist_t >	m_TouchList;

	Vector m_collisionNormal;
	Vector m_groundNormal;
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------


static CMoveHelperServer s_MoveHelperServer;

//-----------------------------------------------------------------------------
// Converts the entity handle into a edict_t
//-----------------------------------------------------------------------------

static inline edict_t* GetEdict( EntityHandle_t handle )
{
	return gEntList.GetEdict( handle );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

CMoveHelperServer::CMoveHelperServer( void )
{
	m_pHost = 0;
	g_pMoveHelper = this;
}

CMoveHelperServer::~CMoveHelperServer( void )
{
	g_pMoveHelper = nullptr;
}

//-----------------------------------------------------------------------------
// Indicates which player we're going to move
//-----------------------------------------------------------------------------

void CMoveHelperServer::SetHost( CEntityCore *host )
{
	m_pHost = static_cast<CBaseEntity *>( host );

	// In case any stuff is ever left over, sigh...
	ResetTouchList();
}

//-----------------------------------------------------------------------------
// Returns the name for debugging purposes
//-----------------------------------------------------------------------------
char const* CMoveHelperServer::GetName( EntityHandle_t handle ) const
{
	// This ain't pertickulerly fast, but it's for debugging anyways
	edict_t* pEdict = GetEdict(handle);
	CBaseEntity *ent = CBaseEntity::Instance( pEdict );
	
	// Is it the world?
	if (ENTINDEX(pEdict) == 0)
		return STRING(gpGlobals->mapname);

	// Is it a model?
	if ( ent && ent->GetModelName() != NULL_STRING )
		return STRING( ent->GetModelName() );

	if ( ent->GetClassname() != nullptr )
	{
		return ent->GetClassname();
	}

	return "?";
}	

//-----------------------------------------------------------------------------
// When we do a collision test, we report everything we hit.. 
//-----------------------------------------------------------------------------

void CMoveHelperServer::ResetTouchList( void )
{
	m_TouchList.clear();

	// Track collision normal
	m_collisionNormal.Init();
	m_groundNormal.Init();
}

//-----------------------------------------------------------------------------
// When a collision occurs, we add it to the touched list
//-----------------------------------------------------------------------------

bool CMoveHelperServer::AddToTouched( const trace_t &tr, const Vector& impactvelocity )
{
	contract_assert( m_pHost );

	// Trace missed
	if ( !tr.Ent<CBaseEntity>() )
		return false;

	if ( tr.Ent<CBaseEntity>() == m_pHost )
	{
		contract_assert( !"CMoveHelperServer::AddToTouched:  Tried to add self to touchlist!!!" );
		return false;
	}

	// Track collision normal
	m_collisionNormal += tr.plane.normal;

	// Check for duplicate entities
	for ( int j = std::ssize( m_TouchList ); --j >= 0; )
	{
		if ( m_TouchList[j].trace.Ent<CBaseEntity>() == tr.Ent<CBaseEntity>() )
		{
			return false;
		}
	}
	
	int i = std::ssize( m_TouchList ); m_TouchList.emplace_back();
	m_TouchList[i].trace = tr;
	VectorCopy( impactvelocity, m_TouchList[i].deltavelocity );

	return true;
}

//-----------------------------------------------------------------------------
// When the ground is hit, update the normal
//-----------------------------------------------------------------------------

void CMoveHelperServer::SetGroundNormal( const Vector& groundNormal )
{
	m_groundNormal = groundNormal;
}

//-----------------------------------------------------------------------------
// After we built the touch list, deal with all the impacts...
//-----------------------------------------------------------------------------
void CMoveHelperServer::ProcessImpacts( void )
{
	contract_assert( m_pHost );

	m_pHost->PhysicsTouchTriggers();

	// Don't bother if the player ain't solid
	if ( m_pHost->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		return;

	// Save off the velocity, cause we need to temporarily reset it
	Vector vel = m_pHost->GetAbsVelocity();

	// Touch other objects that were intersected during the movement.
	for (int i = 0 ; i < std::ssize( m_TouchList ); i++)
	{
		CBaseHandle entindex = m_TouchList[i].trace.Ent<CBaseEntity>()->GetRefEHandle();

		// We should have culled negative indices by now
		contract_assert( entindex.IsValid() );

		edict_t* ent = GetEdict( entindex );
		if (!ent)
			continue;

		// Run the impact function as if we had run it during movement.
		CBaseEntity *entity = GetContainingEntity( ent );
		if ( !entity )
			continue;

		contract_assert( entity != m_pHost );
		// Don't ever collide with self!!!!
		if ( entity == m_pHost )
			continue;

		// Reconstruct trace results.
		m_TouchList[i].trace.m_pEnt = CBaseEntity::Instance( ent );

		// Use the velocity we had when we collided, so boxes will move, etc.
		m_pHost->SetAbsVelocity( m_TouchList[i].deltavelocity );
		
		entity->PhysicsImpact( m_pHost, m_TouchList[i].trace );
	}

	// Restore the velocity
	m_pHost->SetAbsVelocity( vel );

	// Track collision normal
	if ( m_pHost && m_pHost->IsPlayer() )
	{
		CBasePlayer *pPlayerHost = static_cast< CBasePlayer * >( m_pHost );
		contract_assert( pPlayerHost );
		if ( !m_collisionNormal.IsZero() )
		{
			m_collisionNormal.NormalizeInPlace();
			pPlayerHost->m_movementCollisionNormal = m_collisionNormal;
		}		  

		if ( !m_groundNormal.IsZero() )
		{
			pPlayerHost->m_groundNormal = m_groundNormal;
		}
	}

	// So no stuff is ever left over, sigh...
	ResetTouchList();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CMoveHelperServer::StartSound( const Vector& origin, const char *soundname )
{
	//MDB - Changing this to send to PAS, as the overloaded function below has done.
	//Also removed the UsePredictionRules, client does not yet play the equivalent sound

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( origin );

	CBaseEntity::EmitSound( filter, m_pHost->entindex(), soundname );
}

//-----------------------------------------------------------------------------
// plays a sound
//-----------------------------------------------------------------------------
void CMoveHelperServer::StartSound( const Vector& origin, int channel, char const* sample, 
						float volume, soundlevel_t soundlevel, int fFlags, int pitch )
{

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( origin );
	// FIXME, these sounds should not go to the host entity ( SND_NOTHOST )
	if ( gpGlobals->maxClients == 1 )
	{
		// Always send sounds down in SP

		EmitSound_t ep;
		ep.m_nChannel = channel;
		ep.m_pSoundName = sample;
		ep.m_flVolume = volume;
		ep.m_SoundLevel = soundlevel;
		ep.m_nFlags = fFlags;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &origin;

		CBaseEntity::EmitSound( filter, m_pHost->entindex(), ep );
	}
	else
	{
		filter.UsePredictionRules();

		EmitSound_t ep;
		ep.m_nChannel = channel;
		ep.m_pSoundName = sample;
		ep.m_flVolume = volume;
		ep.m_SoundLevel = soundlevel;
		ep.m_nFlags = fFlags;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &origin;

		CBaseEntity::EmitSound( filter, m_pHost->entindex(), ep );
	}
}

//-----------------------------------------------------------------------------
// Umm...
//-----------------------------------------------------------------------------
void CMoveHelperServer::PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	// FIXME, Redo with new event system parameter stuff
}

extern IPhysicsSurfaceProps *physprops;
IPhysicsSurfaceProps *CMoveHelperServer::GetSurfaceProps( void )
{
	return physprops;
}

//-----------------------------------------------------------------------------
// Purpose: Note that this only works on a listen server (since it requires graphical output)
//			*pFormat - 
//			... - 
//-----------------------------------------------------------------------------
void CMoveHelperServer::Con_NPrintf( int idx, char const* pFormat, ...)
{
	std::va_list marker;
	char msg[8192];

	__builtin_va_start(marker, pFormat);
	V_vsnprintf(msg, sizeof( msg ), pFormat, marker);
	__builtin_va_end(marker);
	
	engine->Con_NPrintf( idx, msg );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player falls onto a surface fast enough to take
//			damage, according to the rules in CGameMovement::CheckFalling.
// Output : Returns true if the player survived the fall, false if they died.
//-----------------------------------------------------------------------------
bool CMoveHelperServer::PlayerFallingDamage( void )
{
	if ( m_pHost->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( m_pHost );

		float flFallDamage = g_pGameRules->FlPlayerFallDamage( pPlayer );	
		if ( flFallDamage > 0 )
		{
			pPlayer->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), flFallDamage, DMG_FALL ) ); 
			StartSound( pPlayer->GetAbsOrigin(), "Player.FallDamage" );

			// [dwenger] Needed for fun-fact implementation
			// Increment the stat for fall damage
			CCSPlayer *pPlayer = ToCSPlayer( m_pHost );
			if ( pPlayer )
			{
				CCS_GameStats.IncrementStat( pPlayer, CSSTAT_FALL_DAMAGE, (int)flFallDamage );
			}
		}

		if ( pPlayer->m_iHealth <= 0 )
		{
			if ( g_pGameRules->FlPlayerFallDeathDoesScreenFade( pPlayer ) )
			{
				color32 black = {0, 0, 0, 255};
				UTIL_ScreenFade( pPlayer, black, 0, 9999, FFADE_OUT | FFADE_STAYOUT );
			}
			return(false);
		}
	}

	return(true);
}

//-----------------------------------------------------------------------------
// Purpose: Sets an animation in the player.
// Input  : eAnim - Animation to set.
//-----------------------------------------------------------------------------
void CMoveHelperServer::PlayerSetAnimation( PLAYER_ANIM eAnim )
{
	if ( m_pHost && m_pHost->IsPlayer() )
	{
		static_cast< CBasePlayer * >( m_pHost )->SetAnimation( eAnim );
	}
}

bool CMoveHelperServer::IsWorldEntity( const CBaseHandle &handle )
{
	return handle == CBaseEntity::Instance( 0 );
}

// todo: remove this and find/replace all the MoveHelperServer() calls   (server no longer uses a specialized version of the IMoveHelper interface)
IMoveHelper* MoveHelperServer()
{
	return MoveHelper();
}

using namespace ::ks::reflect;

// Purpose: MOVEMENT ENTITIES TEST

// Hack, sort of. These interpolators don't get to hold state, but the ones
// that need state (like the rope simulator) should NOT be used as paths here.
IPositionInterpolator *g_pPositionInterpolators[8] = {0,0,0,0,0,0,0,0};

IPositionInterpolator* GetPositionInterpolator( int iInterp )
{
	if( !g_pPositionInterpolators[iInterp] )
		g_pPositionInterpolators[iInterp] = Motion_GetPositionInterpolator( iInterp );

	return g_pPositionInterpolators[iInterp];
}

static float Fix( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}

void FixupAngles( QAngle &v )
{
	v.x = Fix( v.x );
	v.y = Fix( v.y );
	v.z = Fix( v.z );
}

//-----------------------------------------------------------------------------
//
// Purpose: Contains a description of a keyframe
//			has no networked representation, so has to store origin, etc. itself
//
//-----------------------------------------------------------------------------
class CPathKeyFrame : public CLogicalEntity
{
public:
	DECLARE_CLASS( CPathKeyFrame, CLogicalEntity );

	void Spawn_Impl( void );
	void Activate_Impl( void );
	void Link( void );

	Vector m_Origin;
	QAngle m_Angles;	// euler angles PITCH YAW ROLL (Y Z X)
	Quaternion m_qAngle;	// quaternion angle (generated from m_Angles)

	[[= ks::reflect::Key{ .name = "NextKey" } ]] string_t m_iNextKey;
	float m_flNextTime;

	CPathKeyFrame *NextKey( int direction );
	CPathKeyFrame *PrevKey( int direction );

	float Speed( void ) { return m_flSpeed; }
	void SetKeyAngles( QAngle angles );

	CPathKeyFrame *InsertNewKey( Vector newPos, QAngle newAngles );
	void CalculateFrameDuration( void );

protected:
	CPathKeyFrame *m_pNextKey;
	CPathKeyFrame *m_pPrevKey;

	[[= ks::reflect::Key{ .name = "MoveSpeed" } ]] float m_flSpeed;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( keyframe_track, CPathKeyFrame );

IMPLEMENT_REFLECT_DATAMAP( CPathKeyFrame )

//-----------------------------------------------------------------------------
// Purpose: Converts inputed euler angles to internal angle format (quaternions)
//-----------------------------------------------------------------------------
void CPathKeyFrame::Spawn_Impl( void )
{
	m_Origin = GetLocalOrigin();
	m_Angles = GetLocalAngles();

	SetKeyAngles( m_Angles );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the keyframe into the path after all the other keys have spawned
//-----------------------------------------------------------------------------
void CPathKeyFrame::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	
	Link();

	CalculateFrameDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathKeyFrame::CalculateFrameDuration( void )
{
	// calculate time from speed
	if ( m_pNextKey && m_flSpeed > 0 )
	{
		m_flNextTime = (m_Origin - m_pNextKey->m_Origin).Length() / m_flSpeed;

		// couldn't get time from distance, get it from rotation instead
		if ( !m_flNextTime )
		{
			// speed is in degrees per second
			// find the largest rotation component and use that
			QAngle ang = m_Angles - m_pNextKey->m_Angles;
			FixupAngles( ang );
			float x = 0;
			for ( int i = 0; i < 3; i++ )
			{
				if ( abs(ang[i]) > x )
				{
					x = abs(ang[i]);
				}
			}

			m_flNextTime = x / m_flSpeed;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Links the key frame into the key frame list
//-----------------------------------------------------------------------------
void CPathKeyFrame::Link( void )
{
	m_pNextKey = dynamic_cast<CPathKeyFrame*>( gEntList.FindEntityByName(nullptr, m_iNextKey ) );

	if ( m_pNextKey )
	{
		m_pNextKey->m_pPrevKey = this;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : angles - 
//-----------------------------------------------------------------------------
void CPathKeyFrame::SetKeyAngles( QAngle angles )
{
	m_Angles = angles;
	AngleQuaternion( m_Angles, m_qAngle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : direction - 
// Output : CPathKeyFrame
//-----------------------------------------------------------------------------
CPathKeyFrame* CPathKeyFrame::NextKey( int direction )
{
	if ( direction == 1 )
	{
		return m_pNextKey;
	}
	else if ( direction == -1 )
	{
		return m_pPrevKey;
	}
	
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : direction - 
// Output : CPathKeyFrame
//-----------------------------------------------------------------------------
CPathKeyFrame *CPathKeyFrame::PrevKey( int direction )
{
	if ( direction == 1 )
	{
		return m_pPrevKey;
	}
	else if ( direction == -1 )
	{
		return m_pNextKey;
	}
	
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Creates and insterts a new keyframe into the sequence
// Input  : newPos - 
//			newAngles - 
// Output : CPathKeyFrame
//-----------------------------------------------------------------------------
CPathKeyFrame *CPathKeyFrame::InsertNewKey( Vector newPos, QAngle newAngles )
{
	CPathKeyFrame *newKey = CREATE_ENTITY( CPathKeyFrame, "keyframe_track" ); 

	// copy data across
	newKey->SetKeyAngles( newAngles );
	newKey->m_Origin = newPos;
	newKey->m_flSpeed = m_flSpeed;
	newKey->SetEFlags( GetEFlags() );
	if ( m_iParent != NULL_STRING )
	{
		newKey->SetParent( m_iParent, nullptr );
	}

	// link forward
	newKey->m_pNextKey = m_pNextKey;
	m_pNextKey->m_pPrevKey = newKey;

	// link back
	m_pNextKey = newKey;
	newKey->m_pPrevKey = this;

	// calculate new times
	CalculateFrameDuration();
	newKey->CalculateFrameDuration();

	return newKey;
}

//-----------------------------------------------------------------------------
//
// Purpose: Basic keyframed movement behavior
//
//-----------------------------------------------------------------------------
class CBaseMoveBehavior : public CPathKeyFrame
{
public:
	DECLARE_CLASS( CBaseMoveBehavior, CPathKeyFrame );

	void Spawn_Impl( void );
	void Activate_Impl( void );
	void MoveDone_Impl( void );
	float SetObjectPhysicsVelocity( float moveTime );

	// methods
	virtual bool StartMoving( int direction );
	virtual void StopMoving( void );
	bool IsMoving_Impl( void );

	// derived classes should override this to get notification of arriving at new keyframes
//	virtual void ArrivedAtKeyFrame( CPathKeyFrame * ) {}

	bool IsAtSequenceStart( void );
	bool IsAtSequenceEnd( void );

	// interpolation functions
//	int m_iTimeModifier;
	[[= ks::reflect::Key{ .name = "PositionInterpolator" } ]] int m_iPositionInterpolator;
	[[= ks::reflect::Key{ .name = "RotationInterpolator" } ]] int m_iRotationInterpolator;

	// animation vars
	float m_flAnimStartTime;
	float m_flAnimEndTime;
	float m_flAverageSpeedAcrossFrame; // for advancing time with speed (not the normal visa-versa)
	CPathKeyFrame *m_pCurrentKeyFrame;	// keyframe currently moving from
	CPathKeyFrame *m_pTargetKeyFrame;	// keyframe being moved to
	CPathKeyFrame *m_pPreKeyFrame, *m_pPostKeyFrame;	// pre- and post-keyframe's for spline interpolation
	float m_flTimeIntoFrame;

	int m_iDirection;		// 1 for forward, -1 for backward, and 0 for at rest

	float CalculateTimeAdvancementForSpeed( float moveTime, float speed );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( move_keyframed, CBaseMoveBehavior );

IMPLEMENT_REFLECT_DATAMAP( CBaseMoveBehavior )

void CBaseMoveBehavior::Spawn_Impl( void )
{
	m_pCurrentKeyFrame = this;
	m_flTimeIntoFrame = 0;
	SetMoveType( MOVETYPE_PUSH );

	// a move behavior is also it's first keyframe
	m_Origin = GetLocalOrigin();
	m_Angles = GetLocalAngles();

	BaseClass::Spawn_Impl();
}

void CBaseMoveBehavior::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	SetMoveDoneTime( 0.5 );	// start moving in 0.2 seconds time

	// if we are just the basic keyframed entity, cycle our animation
	if ( !stricmp(GetClassname(), "move_keyframed") )
	{
		StartMoving( 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the we're at the start of the keyframe sequence
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseMoveBehavior::IsAtSequenceStart( void )
{
	if ( !m_pCurrentKeyFrame )
		return true;

	if ( m_flAnimStartTime && m_flAnimStartTime >= GetLocalTime() )
	{
		if ( !m_pCurrentKeyFrame->PrevKey(1) && !m_pTargetKeyFrame )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if we're at the end of the keyframe sequence
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseMoveBehavior::IsAtSequenceEnd( void )
{
	if ( !m_pCurrentKeyFrame )
		return false;

	if ( !m_pCurrentKeyFrame->NextKey(1) && !m_pTargetKeyFrame )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseMoveBehavior::IsMoving_Impl( void )
{
	if ( m_iDirection != 0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Starts the object moving from it's current position, in the direction indicated
// Input  : direction - 1 is forward through the sequence, -1 is backwards, and 0 is stop
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseMoveBehavior::StartMoving( int direction )
{
	// 0 direction is to stop moving
	if ( direction == 0 )
	{
		StopMoving();
		return false;
	}
	
	// check to see if we should keep moving in the current direction
	if ( m_iDirection == direction )
	{
		// if we're at the end of the current anim key, move to the next one
		if ( GetLocalTime() >= m_flAnimEndTime )
		{
			m_pCurrentKeyFrame = m_pTargetKeyFrame;
			m_flTimeIntoFrame = 0;
			
			if ( !m_pTargetKeyFrame->NextKey(direction) )
			{
				// we've hit the end of the sequence
				m_flAnimEndTime = 0;
				m_flAnimStartTime = 0;
				StopMoving();
				return false;
			}

			// advance the target keyframe
			m_pTargetKeyFrame = m_pTargetKeyFrame->NextKey(direction);
		}
	}
	else
	{
		// we're changing direction

		// need to calculate current position in the frame
		// stop first, then start again
		if ( m_iDirection != 0 )
		{
			StopMoving();
		}

		m_iDirection = direction;

		// if we're going in reverse, swap the currentkey and targetkey (since we're going opposite dir)
		if ( direction == 1 )
		{
			m_pTargetKeyFrame = m_pCurrentKeyFrame->NextKey( direction );
		}
		else if ( direction == -1 )
		{
			if ( m_flTimeIntoFrame > 0 )
			{
				m_pTargetKeyFrame = m_pCurrentKeyFrame;
				m_pCurrentKeyFrame = m_pCurrentKeyFrame->NextKey( 1 );
			}
			else
			{
				m_pTargetKeyFrame = m_pCurrentKeyFrame->PrevKey( 1 );
			}
		}

		// recalculate our movement from the stored data
		if ( !m_pTargetKeyFrame )
		{
			StopMoving();
			return false;
		}

		// calculate the keyframes before and after the keyframes we're interpolating between
		m_pPostKeyFrame = m_pTargetKeyFrame->NextKey( direction );
		if ( !m_pPostKeyFrame )
		{
			m_pPostKeyFrame = m_pTargetKeyFrame;
		}
		m_pPreKeyFrame = m_pCurrentKeyFrame->PrevKey( direction );
		if ( !m_pPreKeyFrame )
		{
			m_pPreKeyFrame = m_pCurrentKeyFrame;
		}
	}

	// no target, can't move
	if ( !m_pTargetKeyFrame )
		return false;
	
	// calculate start/end time
	// ->m_flNextTime is the time to traverse to the NEXT key, so we need the opposite if travelling backwards
	if ( m_iDirection == 1 )
	{
		m_flAnimStartTime = GetLocalTime() - m_flTimeIntoFrame;
		m_flAnimEndTime = GetLocalTime() + m_pCurrentKeyFrame->m_flNextTime - m_flTimeIntoFrame;
	}
	else
	{
		// flip the timing, since we're in reverse
		if ( m_flTimeIntoFrame )
			m_flTimeIntoFrame = m_pTargetKeyFrame->m_flNextTime - m_flTimeIntoFrame;

		m_flAnimStartTime = GetLocalTime() - m_flTimeIntoFrame;
		m_flAnimEndTime = GetLocalTime() + m_pTargetKeyFrame->m_flNextTime - m_flTimeIntoFrame;
	}

	// calculate the average speed at which we cross 
	float animDuration = (m_flAnimEndTime - m_flAnimStartTime);
	float dist = (m_pCurrentKeyFrame->m_Origin - m_pTargetKeyFrame->m_Origin).Length();
	m_flAverageSpeedAcrossFrame = animDuration / dist;

	SetMoveDoneTime( m_flAnimEndTime - GetLocalTime() );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: stops the object from moving
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CBaseMoveBehavior::StopMoving( void )
{
	// remember exactly where we are in the frame
	m_flTimeIntoFrame = 0;

	if ( m_iDirection == 1 )
	{
		// record the time if we're not at the end of the frame
		if ( GetLocalTime() < m_flAnimEndTime )
		{
			m_flTimeIntoFrame = GetLocalTime() - m_flAnimStartTime;
		}
		else
		{
			// we're actually at the end
			if ( m_pTargetKeyFrame )
			{
				m_pCurrentKeyFrame = m_pTargetKeyFrame;
			}
		}
	}
	else if ( m_iDirection == -1 )
	{
		// store it only as a forward movement
		m_pCurrentKeyFrame = m_pTargetKeyFrame;

		if ( GetLocalTime() < m_flAnimEndTime )
		{
			m_flTimeIntoFrame = m_flAnimEndTime - GetLocalTime();
		}
	}

	// stop moving totally
	SetMoveDoneTime( -1 );
	m_iDirection = 0;
	m_flAnimStartTime = 0;
	m_flAnimEndTime = 0;
	m_pTargetKeyFrame = nullptr;
	SetAbsVelocity(vec3_origin);
	SetLocalAngularVelocity( vec3_angle );
}

//-----------------------------------------------------------------------------
// Purpose: We have just arrived at a key, move onto the next keyframe
//-----------------------------------------------------------------------------
void CBaseMoveBehavior::MoveDone_Impl( void )
{
	// if we're just a base then keep playing the anim
	if ( !stricmp(STRING(m_iClassname), "move_keyframed") )
	{
		int direction = m_iDirection;
		// start moving from the keyframe we've just reached
		if ( !StartMoving(direction) )
		{
			// try moving in the other direction
			StartMoving( -direction );
		}
	}

	BaseClass::MoveDone_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: Calculates a new moveTime based on the speed and the current point
//			in the animation.
//			used to advance keyframed objects that have dynamic speeds.
// Input  : moveTime - 
// Output : float - the new time in the keyframing sequence
//-----------------------------------------------------------------------------
float CBaseMoveBehavior::CalculateTimeAdvancementForSpeed( float moveTime, float speed )
{
	return (moveTime * speed * m_flAverageSpeedAcrossFrame);
}

//-----------------------------------------------------------------------------
// Purpose: 
//			GetLocalTime() is the objects local current time
// Input  : destTime - new time that is being moved to
//			moveTime - amount of time to be advanced this frame
// Output : float - the actual amount of time to move (usually moveTime)
//-----------------------------------------------------------------------------
float CBaseMoveBehavior::SetObjectPhysicsVelocity( float moveTime )
{
	// make sure we have a valid set up
	if ( !m_pCurrentKeyFrame || !m_pTargetKeyFrame )
		return moveTime;

	// if we're not moving, we're not moving
	if ( !IsMoving() )
		return moveTime;
	
	float destTime = moveTime + GetLocalTime();

	// work out where we want to be, using destTime
	m_flTimeIntoFrame = destTime - m_flAnimStartTime;
	float newTime = (destTime - m_flAnimStartTime) / (m_flAnimEndTime - m_flAnimStartTime);
	Vector newPos;
	QAngle newAngles;

	IPositionInterpolator *pInterp = GetPositionInterpolator( m_iPositionInterpolator );
	if( pInterp )
	{
		// setup key frames
		pInterp->SetKeyPosition( -1, m_pPreKeyFrame->m_Origin );
		Motion_SetKeyAngles( -1, m_pPreKeyFrame->m_qAngle );

		pInterp->SetKeyPosition( 0, m_pCurrentKeyFrame->m_Origin );
		Motion_SetKeyAngles( 0, m_pCurrentKeyFrame->m_qAngle );

		pInterp->SetKeyPosition( 1, m_pTargetKeyFrame->m_Origin );
		Motion_SetKeyAngles( 1, m_pTargetKeyFrame->m_qAngle );

		pInterp->SetKeyPosition( 2, m_pPostKeyFrame->m_Origin );
		Motion_SetKeyAngles( 2, m_pPostKeyFrame->m_qAngle );

		// find new interpolated position & rotation
		pInterp->InterpolatePosition( newTime, newPos );
	}
	else
	{
		newPos.Init();
	}

	Quaternion qRot;
	Motion_InterpolateRotation( newTime, m_iRotationInterpolator, qRot );
	QuaternionAngles( qRot, newAngles );

	// find our velocity vector (newPos - currentPos) and scale velocity vector according to the movetime
	float oneOnMoveTime = 1 / moveTime;
	SetAbsVelocity( (newPos - GetLocalOrigin()) * oneOnMoveTime );
	SetLocalAngularVelocity( (newAngles - GetLocalAngles()) * oneOnMoveTime );

	return moveTime;
}

// some cvars used by player movement system
inline constexpr const char *DEFAULT_GRAVITY_STRING = "800";

ConVar	sv_gravity		( side_registry(), "sv_gravity", DEFAULT_GRAVITY_STRING, FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "World gravity." );

ConVar	sv_stopspeed	( side_registry(), "sv_stopspeed","80", FCVAR_NOTIFY | FCVAR_REPLICATED |  FCVAR_RELEASE, "Minimum stopping speed when on ground." );
ConVar	sv_noclipaccelerate( side_registry(), "sv_noclipaccelerate", "5", FCVAR_NOTIFY | FCVAR_ARCHIVE | FCVAR_REPLICATED);
ConVar	sv_noclipspeed	( side_registry(), "sv_noclipspeed", "5", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_specaccelerate( side_registry(), "sv_specaccelerate", "5", FCVAR_NOTIFY | FCVAR_ARCHIVE | FCVAR_REPLICATED);
ConVar	sv_specspeed	( side_registry(), "sv_specspeed", "3", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_specnoclip	( side_registry(), "sv_specnoclip", "1", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_maxspeed		( side_registry(), "sv_maxspeed", "320", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE);

ConVar	sv_accelerate	( side_registry(), "sv_accelerate", "5.5", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "Linear acceleration amount (old value is 5.6)" );
ConVar	sv_accelerate_use_weapon_speed	( side_registry(), "sv_accelerate_use_weapon_speed", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar	sv_accelerate_debug_speed	( side_registry(), "sv_accelerate_debug_speed", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar	sv_airaccelerate(  side_registry(), "sv_airaccelerate", "12", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );    
ConVar	sv_wateraccelerate(  side_registry(), "sv_wateraccelerate", "10", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_RELEASE );     
ConVar	sv_waterfriction(  side_registry(), "sv_waterfriction", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_RELEASE );      
ConVar	sv_footsteps	( side_registry(), "sv_footsteps", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Play footstep sound for players" );
ConVar	sv_rollspeed	( side_registry(), "sv_rollspeed", "200", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);
ConVar	sv_rollangle	( side_registry(), "sv_rollangle", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Max view roll angle");

ConVar	sv_friction		( side_registry(), "sv_friction","5.2", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "World friction." );

ConVar	sv_bounce		( side_registry(), "sv_bounce","0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "Bounce multiplier for when physically simulated objects collide with other objects." );
ConVar	sv_maxvelocity	( side_registry(), "sv_maxvelocity","3500", FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum speed any ballistically moving object is allowed to attain per axis." );

ConVar	sv_stepsize		( side_registry(), "sv_stepsize","18", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

ConVar	sv_skyname		( side_registry(), "sv_skyname", "sky_urb01", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Current name of the skybox texture" );
ConVar	sv_backspeed	( side_registry(), "sv_backspeed", "0.6", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How much to slow down backwards motion" );
ConVar  sv_waterdist	( side_registry(), "sv_waterdist","12", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Vertical view fixup when eyes are near water plane." );

// Vehicle convars
ConVar r_VehicleViewDampen( side_registry(), "r_VehicleViewDampen", "1", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

// Jeep convars
ConVar r_JeepViewDampenFreq( side_registry(), "r_JeepViewDampenFreq", "7.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar r_JeepViewDampenDamp( side_registry(), "r_JeepViewDampenDamp", "1.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar r_JeepViewZHeight( side_registry(), "r_JeepViewZHeight", "10.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

// Airboat convars
ConVar r_AirboatViewDampenFreq( side_registry(), "r_AirboatViewDampenFreq", "7.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar r_AirboatViewDampenDamp( side_registry(), "r_AirboatViewDampenDamp", "1.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar r_AirboatViewZHeight( side_registry(), "r_AirboatViewZHeight", "0.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

KS_ENTITY_ABSTRACT_KIND( CRuleEntity )

KS_ENTITY_ABSTRACT_KIND( CRuleBrushEntity )

KS_ENTITY_ABSTRACT_KIND( CRulePointEntity )
