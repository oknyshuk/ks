// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <stddef.h>
#include <string.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "bone_accessor.h"
#include "bone_setup.h"
#include "vcollide_parse.h"
#include "vphysics/collision_set.h"
#include "vphysics/constraints.h"
#include "edict.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "const.h"
#include "irecipientfilter.h"
#include "rope_shared.h"
module ks.server;

import std;
import ks.core.dbg;
import ks.core.platform;
import ks.core.tslist;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.intrusive_list;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.vstdlib.jobthread;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import ks.game.shareddefs;
import ks.game.mapentities_shared;
import :nav_4;
import :entity_2;
import ks.tier1.strtools;
import :entity_dispatch;
import :entity_kind;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.core.basetypes;
import ks.core.numeric;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.mathlib;
import ks.math.vmatrix;
import ks.tier1.bit_ops;
import ks.tier1.small_vector;
import ks.tier1.vector_ops;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import ks.game.rope_helpers;

class CBaseEntity;

using namespace ::ks::reflect;

static constexpr auto QUERYCACHE_SIZE = 1024;

static QueryCacheEntry_t s_QCache[QUERYCACHE_SIZE];

static constexpr auto QUERYCACHE_HASH_SIZE = ( QUERYCACHE_SIZE  * 2 );

// elements available for cache reuse
static ks::intrusive_dlist<QueryCacheEntry_t> s_VictimList;

static ks::intrusive_dlist<QueryCacheEntry_t> s_HashChains[QUERYCACHE_HASH_SIZE];

static int s_nReplaceCtr = QUERYCACHE_SIZE - 1;
static int s_nTimeStampCounter = 0 ;
static int s_nNumCacheQueries = 0;
static int s_nNumCacheMisses = 0;
static int s_SuccessfulSpeculatives = 0;
static int s_WastedSpeculativeUpdates = 0;

void QueryCacheKey_t::ComputeHashIndex( void )
{
	unsigned int ret = ( unsigned int ) m_Type;
	for( int i = 0 ; i < m_nNumValidPoints; i++ )
	{
		ret += ( unsigned int ) m_pEntities[i].ToInt();
		ret += size_cast< unsigned int >( (uintp)m_nOffsetMode );
	}
	ret += *( ( uint32 *) &m_flMinimumUpdateInterval );
	ret += m_nTraceMask;
	m_nHashIdx = ret % QUERYCACHE_HASH_SIZE;
}

ConVar	sv_disable_querycache(side_registry(), "sv_disable_querycache", "0", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "debug - disable trace query cache" );

static QueryCacheEntry_t *FindOrAllocateCacheEntry( QueryCacheKey_t const &entry )
{
	QueryCacheEntry_t *pFound = nullptr;
	// see if we find it
	for( QueryCacheEntry_t *pNode = s_HashChains[entry.m_nHashIdx].head(); pNode; pNode = pNode->m_pNext )
	{
		if ( pNode->m_QueryParams.Matches( &entry ) )
		{
			pFound = pNode;
			break;
		}
	}
	if (! pFound )
	{
		pFound = s_VictimList.pop_front();
		if ( ! pFound )
		{
			// randomly replace one
			pFound = s_QCache + s_nReplaceCtr;
			s_nReplaceCtr--;
			if ( s_nReplaceCtr < 0 )
				s_nReplaceCtr = QUERYCACHE_SIZE - 1;
			if ( pFound->m_QueryParams.m_Type != EQUERY_INVALID )
			{
				s_HashChains[pFound->m_QueryParams.m_nHashIdx].erase( pFound );
			}
		}
		pFound->m_QueryParams = entry;
		s_HashChains[pFound->m_QueryParams.m_nHashIdx].push_front( pFound );
		pFound->m_bSpeculativelyDone = false;
		pFound->IssueQuery();
	}
	else
	{
		if ( sv_disable_querycache.GetInt() || 
			 ( gpGlobals->curtime - pFound->m_flLastUpdateTime >= 
			   pFound->m_QueryParams.m_flMinimumUpdateInterval ) )
		{
			pFound->m_bSpeculativelyDone = false;
			pFound->IssueQuery();
		}
		else
		{
			if ( pFound->m_bSpeculativelyDone )
				s_SuccessfulSpeculatives++;
		}
		
	}
	return pFound;
}

static QueryCacheEntry_t *FindOrAllocateCacheEntry( EQueryType_t nType,
													CBaseEntity *pEntity1, CBaseEntity *pEntity2,
													EEntityOffsetMode_t nMode1, EEntityOffsetMode_t nMode2,
													unsigned int nTraceMask )
{
	QueryCacheKey_t entry;
	entry.m_Type = nType;
	entry.m_pEntities[0] = pEntity1;
	entry.m_pEntities[1] = pEntity2;
	entry.m_nOffsetMode[0] = nMode1;
	entry.m_nOffsetMode[1] = nMode2;
	entry.m_nTraceMask = nTraceMask;
	entry.m_nNumValidPoints = 2;
	entry.ComputeHashIndex();
	return FindOrAllocateCacheEntry( entry );
}

bool QueryCacheKey_t::Matches( QueryCacheKey_t const *pNode ) const
{
	if (
		( pNode->m_Type != m_Type ) ||
		( pNode->m_nTraceMask != m_nTraceMask ) ||
		( pNode->m_pTraceFilterFunction != m_pTraceFilterFunction ) ||
		( pNode->m_nNumValidPoints != m_nNumValidPoints ) || 
		( pNode->m_flMinimumUpdateInterval != m_flMinimumUpdateInterval )
		)
		return false;
	for( int i = 0; i < m_nNumValidPoints; i++ )
	{
		if (
			( pNode->m_pEntities[i] != m_pEntities[i] ) ||
			( pNode->m_nOffsetMode[i] != m_nOffsetMode[i] )
			)
			return false;
	}
	return true;
}

static void CalculateOffsettedPosition( CBaseEntity *pEntity, EEntityOffsetMode_t nMode, Vector *pVecOut  )
{
	switch( nMode )
	{
		case EOFFSET_MODE_WORLDSPACE_CENTER:
			*pVecOut = pEntity->WorldSpaceCenter();
			break;

		case EOFFSET_MODE_EYEPOSITION:
			*pVecOut = pEntity->EyePosition();
			break;

		case EOFFSET_MODE_NONE:
			pVecOut->Init();
			break;
	}
}

struct QueryCacheUpdateRecord_t
{
	int m_nStartHashChain;
	int m_nNumHashChainsToUpdate;
	ks::intrusive_dlist_with_tail<QueryCacheEntry_t> m_KilledList;
};

void ProcessQueryCacheUpdate( QueryCacheUpdateRecord_t &workItem )
{
	float flCurTime = gpGlobals->curtime;
	// run through all of the cache.
	for( int i = 0; i < workItem.m_nNumHashChainsToUpdate; i++ )
	{
		QueryCacheEntry_t *pNext;
		for( QueryCacheEntry_t *pEntry = s_HashChains[i + workItem.m_nStartHashChain].head() ; pEntry; pEntry = pNext )
		{
			pNext = pEntry->m_pNext;
			if ( pEntry->m_bUsedSinceUpdated )
			{
				if ( flCurTime - pEntry->m_flLastUpdateTime >= 
					 pEntry->m_QueryParams.m_flMinimumUpdateInterval )
				{
					// don't bother updating if we have recently
					pEntry->IssueQuery();
					pEntry->m_bUsedSinceUpdated = false;
					pEntry->m_bSpeculativelyDone = true;
				}
			}
			else
			{
				if ( flCurTime - pEntry->m_flLastUpdateTime > pEntry->m_QueryParams.m_flMinimumUpdateInterval )
				{
					if ( pEntry->m_bSpeculativelyDone  && ( !pEntry->m_bUsedSinceUpdated ) )
					{
						s_WastedSpeculativeUpdates++;
					}
					pEntry->m_QueryParams.m_Type = EQUERY_INVALID;
					s_HashChains[pEntry->m_QueryParams.m_nHashIdx].erase( pEntry );
					workItem.m_KilledList.push_front( pEntry );
				}
			}
		}
	}
}

static constexpr auto N_WAYS_TO_SPLIT_CACHE_UPDATE = 8;

static void PreUpdateQueryCache()
{
	mdlcache->BeginCoarseLock();
	mdlcache->BeginLock();
}

static void PostUpdateQueryCache()
{
	mdlcache->EndLock();
	mdlcache->EndCoarseLock();
}

void UpdateQueryCache( void )
{
	// parallel process all hash chains
	QueryCacheUpdateRecord_t workList[N_WAYS_TO_SPLIT_CACHE_UPDATE];
	int nCurEntry = 0;
	for( int i =0 ; i < N_WAYS_TO_SPLIT_CACHE_UPDATE; i++ )
	{
		workList[i].m_nStartHashChain = nCurEntry;
		if ( i != N_WAYS_TO_SPLIT_CACHE_UPDATE -1 )
			workList[i].m_nNumHashChainsToUpdate = std::size( s_HashChains ) / N_WAYS_TO_SPLIT_CACHE_UPDATE;
		else
			workList[i].m_nNumHashChainsToUpdate = std::size( s_HashChains ) - nCurEntry;
		nCurEntry += std::size( s_HashChains ) / N_WAYS_TO_SPLIT_CACHE_UPDATE;
	}
	ParallelProcess( workList, N_WAYS_TO_SPLIT_CACHE_UPDATE, ProcessQueryCacheUpdate, PreUpdateQueryCache, PostUpdateQueryCache, ( sv_disable_querycache.GetBool() ) ? 0 : std::numeric_limits<int>::max() );
	// now, we need to take all of the obsolete cache entries each thread generated and add them to
	// the victim cache
	for( int i = 0 ; i < N_WAYS_TO_SPLIT_CACHE_UPDATE; i++ )
	{
		s_VictimList.splice_front( workList[i].m_KilledList );
	}
}

void InvalidateQueryCache( void )
{
	s_VictimList.clear();
	for( int i = 0; i < std::size( s_HashChains ); i++ )
		s_HashChains[i].clear();
	// now, invalidate all cache entries and add them to the victims
	for( int i = 0; i < std::size( s_QCache ); i++ )
	{
		s_QCache[i].m_QueryParams.m_Type = EQUERY_INVALID;
		s_VictimList.push_front( s_QCache + i );
	}
}

void QueryCacheEntry_t::IssueQuery( void )
{
	for( int i = 0 ; i < m_QueryParams.m_nNumValidPoints; i++ )
	{
		CBaseEntity *pEntity = m_QueryParams.m_pEntities[i];
		if (! pEntity )
		{
			m_QueryParams.m_Type = EQUERY_INVALID;
			s_HashChains[m_QueryParams.m_nHashIdx].erase( this );
			s_VictimList.push_front( this );
			return;
		}
		CalculateOffsettedPosition( pEntity, m_QueryParams.m_nOffsetMode[i],
									&( m_QueryParams.m_Points[i] ) );
	}
	CTraceFilterSimple filter( m_QueryParams.m_pEntities[2],
							   m_QueryParams.m_nCollisionGroup,
							   m_QueryParams.m_pTraceFilterFunction );
	trace_t result;
	s_nNumCacheMisses++;
	UTIL_TraceLine( m_QueryParams.m_Points[0], m_QueryParams.m_Points[1],
					m_QueryParams.m_nTraceMask, &filter, &result );
	m_bResult = ! ( result.DidHit() );
	m_flLastUpdateTime = gpGlobals->curtime;
}

bool IsLineOfSightBetweenTwoEntitiesClear( CBaseEntity *pSrcEntity,
										   EEntityOffsetMode_t nSrcOffsetMode,
										   CBaseEntity *pDestEntity,
										   EEntityOffsetMode_t nDestOffsetMode,
										   CBaseEntity *pSkipEntity,
										   int nCollisionGroup,
										   unsigned int nTraceMask,
										   ShouldHitFunc_t pTraceFilterCallback,
										   float flMinimumUpdateInterval )
{
	QueryCacheKey_t entry;
	entry.m_Type = EQUERY_ENTITY_LOS_CHECK;
	entry.m_pEntities[0] = pSrcEntity;
	entry.m_pEntities[1] = pDestEntity;
	entry.m_pEntities[2] = pSkipEntity;
	entry.m_nOffsetMode[0] = nSrcOffsetMode;
	entry.m_nOffsetMode[1] = nDestOffsetMode;
	entry.m_nOffsetMode[2] = EOFFSET_MODE_NONE;
	entry.m_nTraceMask = nTraceMask;
	entry.m_nNumValidPoints = 3;
	entry.m_nCollisionGroup = nCollisionGroup;
	entry.m_pTraceFilterFunction = pTraceFilterCallback;
	entry.m_flMinimumUpdateInterval = flMinimumUpdateInterval;
	entry.ComputeHashIndex();

	s_nNumCacheQueries++;
	QueryCacheEntry_t *pNode = FindOrAllocateCacheEntry( entry );
	pNode->m_bUsedSinceUpdated = true;
	return pNode->m_bResult;
}

static void sv_querycache_stats( const CCommand &args );
static ConCommand sv_querycache_stats_command( side_registry(), "sv_querycache_stats", sv_querycache_stats, "Display status of the query cache (client only)" );
static void sv_querycache_stats( const CCommand &args )
{
	Warning( "%d queries, %d misses (%d free) suc spec = %d wasted spec=%d\n",
			 s_nNumCacheQueries, s_nNumCacheMisses, (int)s_VictimList.size(),
			 s_SuccessfulSpeculatives, s_WastedSpeculativeUpdates );
}

class CAI_BaseNPC;
class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CRagdollManager : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_RagdollManager", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CRagdollManager, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CRagdollManager();

	void	Activate_Impl();
	int		UpdateTransmitState_Impl();

	[[= ks::reflect::Input{ .name = "SetMaxRagdollCount", .type = FIELD_INTEGER } ]] void InputSetMaxRagdollCount(inputdata_t &data);

	int DrawDebugTextOverlays_Impl(void);

public:

	void UpdateCurrentMaxRagDollCount();

	CNetworkVar( int,  m_iCurrentMaxRagdollCount, [[= ks::reflect::Net{ .bits = 6 } ]] );

	[[= ks::reflect::Key{ .name = "MaxRagdollCount" } ]] int m_iMaxRagdollCount;

	[[= ks::reflect::Key{ .name = "SaveImportant" } ]] bool m_bSaveImportant;
};

IMPLEMENT_REFLECT_SERVERCLASS( CRagdollManager, DT_RagdollManager )

LINK_ENTITY_TO_CLASS( game_ragdoll_manager, CRagdollManager );

IMPLEMENT_REFLECT_DATAMAP( CRagdollManager )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CRagdollManager::CRagdollManager( void )
{
	m_iMaxRagdollCount = -1;
	m_iCurrentMaxRagdollCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pInfo - 
// Output : int
//-----------------------------------------------------------------------------
int CRagdollManager::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollManager::Activate_Impl()
{
	BaseClass::Activate_Impl();
	UpdateCurrentMaxRagDollCount();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRagdollManager::UpdateCurrentMaxRagDollCount()
{
	m_iCurrentMaxRagdollCount = m_iMaxRagdollCount;
	s_RagdollLRU.SetMaxRagdollCount( m_iCurrentMaxRagdollCount );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRagdollManager::InputSetMaxRagdollCount(inputdata_t &inputdata)
{
	m_iMaxRagdollCount = inputdata.value.Int();
	UpdateCurrentMaxRagDollCount();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool RagdollManager_SaveImportant( CAI_BaseNPC *pNPC )
{

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CRagdollManager::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print max ragdoll count
		V_snprintf(tempstr,sizeof(tempstr),"max ragdoll count: %d", m_iCurrentMaxRagdollCount.Get());
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

//CLIENT
//SERVER

CRagdollLowViolenceManager g_RagdollLVManager;

void CRagdollLowViolenceManager::SetLowViolence( const char *pMapName )
{
	// set the value using the engine's low violence settings
	m_bLowViolence = UTIL_IsLowViolence();

	// the server doesn't worry about low violence during multiplayer games
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		m_bLowViolence = false;
	}

	// Turn the low violence ragdoll stuff off if we're in the HL2 Citadel maps because
	// the player has the super gravity gun and fading ragdolls will break things.
	if( hl2_episodic.GetBool() )
	{
		if ( V_stricmp( pMapName, "ep1_citadel_02" ) == 0 ||
			V_stricmp( pMapName, "ep1_citadel_02b" ) == 0 ||
			V_stricmp( pMapName, "ep1_citadel_03" ) == 0 )
		{
			m_bLowViolence = false;
		}
	}
	else
	{
		if ( V_stricmp( pMapName, "d3_citadel_03" ) == 0 ||
			V_stricmp( pMapName, "d3_citadel_04" ) == 0 ||
			V_stricmp( pMapName, "d3_citadel_05" ) == 0 ||
			V_stricmp( pMapName, "d3_breen_01" ) == 0 )
		{
			m_bLowViolence = false;
		}
	}
}

// A simple cache to store the ragdoll's data after it has been parsed.  Avoid re-parsing on every create
struct cache_ragdollsolid_t
{
	objectparams_t params;
	int surfacePropIndex;
	short boneIndex;
	short collideIndex;
};

struct cache_ragdollconstraint_t
{
	constraint_axislimit_t axes[3];
	matrix3x4_t constraintToAttached;
	short parentIndex;
	short childIndex;
};

// store the ragdoll as a single allocation header, then solids array, then constraints array linearly in memory
struct cache_ragdoll_t
{
	IPhysicsCollisionSet *pCollisionSet;
	ragdollanimatedfriction_t animfriction;
	short solidCount;
	short constraintCount;

	const cache_ragdollsolid_t *GetSolids() { return (cache_ragdollsolid_t *)(this+1); }
	const cache_ragdollconstraint_t *GetConstraints() { return (cache_ragdollconstraint_t *)(GetSolids()+solidCount); }
};

cache_ragdoll_t *CreateRagdollCache( vcollide_t *pOutput, cache_ragdollsolid_t *pSolids, cache_ragdollconstraint_t *pConstraints, cache_ragdoll_t *pRagdoll )
{
	std::size_t memSize = sizeof(cache_ragdoll_t);
	size_t solidSize = sizeof(cache_ragdollsolid_t) * pRagdoll->solidCount;
	size_t constraintSize = sizeof(cache_ragdollconstraint_t) * pRagdoll->constraintCount;

	cache_ragdoll_t *pMem = (cache_ragdoll_t *)physcollision->VCollideAllocUserData( pOutput, memSize + solidSize + constraintSize );
	V_memcpy( pMem, pRagdoll, sizeof(*pMem) );
	V_memcpy( (void *)pMem->GetSolids(), pSolids, solidSize );
	V_memcpy( (void *)pMem->GetConstraints(), pConstraints, constraintSize );
	return pMem;
}

// OPTIMIZE: Slow, hopefully this is only called by save/load
void RagdollSetupAnimatedFriction( IPhysicsEnvironment *pPhysEnv, ragdoll_t *ragdoll, int iModelIndex )
{
	vcollide_t* pCollide = modelinfo->GetVCollide( iModelIndex );

	if ( pCollide )
	{
		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );

		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();

			if ( !strcmpi( pBlock, "animatedfriction") ) 
			{
				pParse->ParseRagdollAnimatedFriction( &ragdoll->animfriction, nullptr );
			}
			else
			{
				pParse->SkipBlock();
			}
		}

		physcollision->VPhysicsKeyParserDestroy( pParse );
	}
}

static void RagdollAddSolids( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params, cache_ragdollsolid_t *pSolids, int solidCount, const cache_ragdollconstraint_t *pConstraints, int constraintCount )
{
	const char *pszName = params.pStudioHdr->pszName();
	Vector position;
	matrix3x4_t xform;
	// init parent index
	for ( int i = 0; i < solidCount; i++ )
	{
		ragdoll.list[i].parentIndex = -1;
	}
	// now set from constraints
	for ( int i = 0; i < constraintCount; i++ )
	{
		// save parent index
		ragdoll.list[pConstraints[i].childIndex].parentIndex = pConstraints[i].parentIndex;
		MatrixGetColumn( pConstraints[i].constraintToAttached, 3, ragdoll.list[pConstraints[i].childIndex].originParentSpace );
	}

	// now setup the solids, using parent indices
	for ( int i = 0; i < solidCount; i++ )
	{
		if ( params.fixedConstraints )
		{
			pSolids[i].params.mass = 1000.f;
		}

		ragdoll.boneIndex[i] = pSolids[i].boneIndex;
		pSolids[i].params.pName = pszName;
		pSolids[i].params.pGameData = params.pGameData;
		ragdoll.list[i].pObject = pPhysEnv->CreatePolyObject( params.pCollide->solids[pSolids[i].collideIndex], pSolids[i].surfacePropIndex, vec3_origin, vec3_angle, &pSolids[i].params );
		ragdoll.list[i].pObject->SetGameIndex( i );
		int parentIndex = ragdoll.list[i].parentIndex;
		MatrixCopy( params.pCurrentBones[ragdoll.boneIndex[i]], xform );
		if ( parentIndex >= 0 )
		{
			contract_assert( parentIndex<i );
			ragdoll.list[parentIndex].pObject->LocalToWorld( &position, ragdoll.list[i].originParentSpace );
			MatrixSetColumn( position, 3, xform );
		}
		ragdoll.list[i].pObject->SetPositionMatrix( xform, true );

		PhysSetGameFlags( ragdoll.list[i].pObject, FVPHYSICS_PART_OF_RAGDOLL );

	}
	ragdoll.listCount = solidCount;
}

static void RagdollAddConstraints( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params, const cache_ragdollconstraint_t *pConstraints, int constraintCount )
{
	constraint_ragdollparams_t constraint;
	for ( int i = 0; i < constraintCount; i++ )
	{
		constraint.Defaults();
		V_memcpy( constraint.axes, pConstraints[i].axes, sizeof(constraint.axes) );
		if ( params.jointFrictionScale > 0 )
		{
			for ( int k = 0; k < 3; k++ )
			{
				constraint.axes[k].torque *= params.jointFrictionScale;
			}
		}
		int parentIndex = pConstraints[i].parentIndex;
		int childIndex = pConstraints[i].childIndex;
		constraint.childIndex = childIndex;
		constraint.parentIndex = parentIndex;
		constraint.useClockwiseRotations = true;
		constraint.constraintToAttached = pConstraints[i].constraintToAttached;
		// UNDONE: We could transform the constraint limit axes relative to the bone space
		// using this data.  Do we need that feature?
		SetIdentityMatrix( constraint.constraintToReference );

		if ( params.fixedConstraints )
		{
			// Makes the ragdoll a statue...
			constraint_fixedparams_t fixed;
			fixed.Defaults();
			fixed.InitWithCurrentObjectState( ragdoll.list[childIndex].pObject, ragdoll.list[constraint.parentIndex].pObject );
			fixed.constraint.Defaults();
			ragdoll.list[childIndex].pConstraint = pPhysEnv->CreateFixedConstraint( ragdoll.list[childIndex].pObject, ragdoll.list[parentIndex].pObject, ragdoll.pGroup, fixed );
		}
		else
		{
			ragdoll.list[childIndex].pConstraint = pPhysEnv->CreateRagdollConstraint( ragdoll.list[childIndex].pObject, ragdoll.list[parentIndex].pObject, ragdoll.pGroup, constraint );
		}
	}
}

static cache_ragdoll_t *ParseRagdollIntoCache( CStudioHdr *pStudioHdr, vcollide_t *pCollide, int modelIndex )
{
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	cache_ragdollsolid_t solidList[RAGDOLL_MAX_ELEMENTS];
	cache_ragdollconstraint_t constraintList[RAGDOLL_MAX_ELEMENTS];
	solid_t solid;
	int constraintCount = 0;
	int solidCount = 0;
	cache_ragdoll_t cache;
	V_memset( &cache, 0, sizeof(cache) );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			pParse->ParseSolid( &solid, &g_SolidSetup );
			cache_ragdollsolid_t *pSolid = &solidList[solidCount];
			pSolid->boneIndex = Studio_BoneIndexByName( pStudioHdr, solid.name );
			if ( pSolid->boneIndex >= 0 )
			{
				pSolid->collideIndex = solid.index;
				pSolid->surfacePropIndex = physprops->GetSurfaceIndex( solid.surfaceprop );
				if ( pSolid->surfacePropIndex < 0 )
				{
					pSolid->surfacePropIndex = physprops->GetSurfaceIndex( "default" );
				}
				pSolid->params = solid.params;
				pSolid->params.enableCollisions = false;
				solidCount++;
			}
			else
			{
				Msg( "ParseRagdollIntoCache:  Couldn't Lookup Bone %s\n", solid.name );
			}
		}
		else if ( !strcmpi( pBlock, "ragdollconstraint" ) )
		{
			constraint_ragdollparams_t constraint;
			pParse->ParseRagdollConstraint( &constraint, nullptr );
			if( constraint.childIndex != constraint.parentIndex && constraint.childIndex >= 0 && constraint.parentIndex >= 0)
			{
				cache_ragdollconstraint_t *pOut = &constraintList[constraintCount];
				constraintCount++;
				V_memcpy( pOut->axes, constraint.axes, sizeof(constraint.axes) );
				pOut->parentIndex = constraint.parentIndex;
				pOut->childIndex = constraint.childIndex;
				Studio_CalcBoneToBoneTransform( pStudioHdr, solidList[constraint.childIndex].boneIndex, solidList[constraint.parentIndex].boneIndex, pOut->constraintToAttached );
			}
		}
		else if ( !strcmpi( pBlock, "collisionrules" ) )
		{
			ragdollcollisionrules_t rules;
			IPhysicsCollisionSet *pSet = physics->FindOrCreateCollisionSet( modelIndex, pCollide->solidCount );
			rules.Defaults(physics, pSet);
			pParse->ParseCollisionRules( &rules, nullptr );
			cache.pCollisionSet = rules.pCollisionSet;
		}
		else if ( !strcmpi( pBlock, "animatedfriction") ) 
		{
			pParse->ParseRagdollAnimatedFriction( &cache.animfriction, nullptr );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );
	cache.solidCount = solidCount;
	cache.constraintCount = constraintCount;
	return CreateRagdollCache( pCollide, solidList, constraintList, &cache );
}

static void RagdollCreateObjects( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params )
{
	ragdoll.listCount = 0;
	ragdoll.pGroup = nullptr;
	ragdoll.allowStretch = params.allowStretch;
	std::memset( ragdoll.list, 0, sizeof(ragdoll.list) );
	memset( &ragdoll.animfriction, 0, sizeof(ragdoll.animfriction) );
	
	if ( !params.pCollide )
	{
		Warning( "Ragdoll has no pCollide!" );
		contract_assert( false );
		return;
	}

	if ( params.pCollide->solidCount > RAGDOLL_MAX_ELEMENTS )
	{
		Warning( "Ragdoll solid count %d exceeds maximum limit of %d - Ragdoll not created", params.pCollide->solidCount, RAGDOLL_MAX_ELEMENTS );
		contract_assert( false );
		return;
	}

	cache_ragdoll_t *pCache = (cache_ragdoll_t *)params.pCollide->pUserData;
	if ( !pCache )
	{
		pCache = ParseRagdollIntoCache(params.pStudioHdr, params.pCollide, params.modelIndex);
	}

	constraint_groupparams_t group;
	group.Defaults();
	ragdoll.pGroup = pPhysEnv->CreateConstraintGroup( group );
 
	RagdollAddSolids( pPhysEnv, ragdoll, params, const_cast<cache_ragdollsolid_t *>(pCache->GetSolids()), pCache->solidCount, pCache->GetConstraints(), pCache->constraintCount );
	RagdollAddConstraints( pPhysEnv, ragdoll, params, pCache->GetConstraints(), pCache->constraintCount );
}

void RagdollSetupCollisions( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex )
{
	contract_assert( pCollide );
	if (!pCollide)
		return;

	IPhysicsCollisionSet *pSet = physics->FindCollisionSet( modelIndex );
	if ( !pSet )
	{
		pSet = physics->FindOrCreateCollisionSet( modelIndex, pCollide->solidCount );
		if ( !pSet )
			return;

		bool bFoundRules = false;

		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();
			if ( !strcmpi( pBlock, "collisionrules" ) )
			{
				ragdollcollisionrules_t rules;
				rules.Defaults(physics, pSet);
				pParse->ParseCollisionRules( &rules, nullptr );
				contract_assert( rules.pCollisionSet == pSet );
				bFoundRules = true;
			}
			else
			{
				pParse->SkipBlock();
			}
		}
		physcollision->VPhysicsKeyParserDestroy( pParse );

		if ( !bFoundRules )
		{
			// these are the default rules - each piece collides with everything
			// except immediate parent/constrained object.
			int i;
			for ( i = 0; i < ragdoll.listCount; i++ )
			{
				for ( int j = i+1; j < ragdoll.listCount; j++ )
				{
					pSet->EnableCollisions( i, j );
				}
			}
			for ( i = 0; i < ragdoll.listCount; i++ )
			{
  				int parent = ragdoll.list[i].parentIndex;
				if ( parent >= 0 )
				{
  					contract_assert( ragdoll.list[i].pObject );
  					contract_assert( ragdoll.list[i].pConstraint );
					pSet->DisableCollisions( i, parent );
				}
 			}
		}
	}
}

void RagdollActivate( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex, bool bForceWake )
{
	RagdollSetupCollisions( ragdoll, pCollide, modelIndex );

	for ( int i = 0; i < ragdoll.listCount; i++ )
	{

		PhysSetGameFlags( ragdoll.list[i].pObject, FVPHYSICS_MULTIOBJECT_ENTITY );
		// now that the relationships are set, activate the collision system
		ragdoll.list[i].pObject->EnableCollisions( true );

		if ( bForceWake == true )
		{
			ragdoll.list[i].pObject->Wake();
		}
	}
	if ( ragdoll.pGroup )
	{
		// NOTE: This also wakes the objects
		ragdoll.pGroup->Activate();
		// so if we didn't want that, we'll need to put them back to sleep here
		if ( !bForceWake )
		{
			for ( int i = 0; i < ragdoll.listCount; i++ )
			{
				ragdoll.list[i].pObject->Sleep();
			}

		}
	}
}

bool RagdollCreate( ragdoll_t &ragdoll, const ragdollparams_t &params, IPhysicsEnvironment *pPhysEnv )
{
	RagdollCreateObjects( pPhysEnv, ragdoll, params );

	if ( !ragdoll.listCount )
		return false;

	int forceBone = params.forceBoneIndex;
	
	int i;
	float totalMass = 0;
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		totalMass += ragdoll.list[i].pObject->GetMass();
	}
	totalMass = std::max<std::common_type_t<float, int>>(totalMass,1);

	// apply force to the model
	Vector nudgeForce = params.forceVector;
	Vector forcePosition = params.forcePosition;
	// UNDONE: Test scaling the force by total mass on all bones
	
	// UNDONE: forcebone can be out of range when a body part breaks off - it uses the shared force bone from the original model
	// UNDONE: Remap this?
	if ( forceBone >= 0 && forceBone < ragdoll.listCount )
	{
		ragdoll.list[forceBone].pObject->ApplyForceCenter( nudgeForce );
		//nudgeForce *= 0.5;
		ragdoll.list[forceBone].pObject->GetPosition( &forcePosition, nullptr );
	}
	if ( forcePosition != vec3_origin )
	{
		for ( i = 0; i < ragdoll.listCount; i++ )
		{
			if ( forceBone != i )
			{
				float scale = ragdoll.list[i].pObject->GetMass() / totalMass;
				ragdoll.list[i].pObject->ApplyForceOffset( scale * nudgeForce, forcePosition );
			}
		}
	}
	return true;
}

void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pPrevBones, const matrix3x4_t *pCurrentBones, float dt )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector velocity;
		AngularImpulse angVel;
		int boneIndex = ragdoll.boneIndex[i];
		CalcBoneDerivatives( velocity, angVel, pPrevBones[boneIndex], pCurrentBones[boneIndex], dt );
		
		AngularImpulse localAngVelocity;

		// Angular velocity is always applied in local space in vphysics
		ragdoll.list[i].pObject->WorldToLocalVector( &localAngVelocity, angVel );
		ragdoll.list[i].pObject->AddVelocity( &velocity, &localAngVelocity );
	}
}

void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pBoneToWorld )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		matrix3x4_t inverse;
		MatrixInvert( pBoneToWorld[i], inverse );
		Quaternion q;
		Vector pos;
		MatrixAngles( inverse, q, pos );

		Vector velocity;
		AngularImpulse angVel;
		float flSpin;

		Vector localVelocity;
		AngularImpulse localAngVelocity;

		QuaternionAxisAngle( q, localAngVelocity, flSpin );
		localAngVelocity *= flSpin;
		localVelocity = pos;

		// move those bone-local coords back to world space using the ragdoll transform
		ragdoll.list[i].pObject->LocalToWorldVector( &velocity, localVelocity );

		ragdoll.list[i].pObject->AddVelocity( &velocity, &localAngVelocity );
	}
}

void RagdollDestroy( ragdoll_t &ragdoll )
{
	if ( !ragdoll.listCount )
		return;

	int i;
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		physenv->DestroyConstraint( ragdoll.list[i].pConstraint );
		ragdoll.list[i].pConstraint = nullptr;
	}
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		// during level transitions these can get temporarily loaded without physics objects
		// purely for the purpose of testing for PVS of transition.  If they fail they get
		// deleted before the physics objects are loaded.  The list count will be nonzero
		// since that is saved separately.
		if ( ragdoll.list[i].pObject )
		{
			physenv->DestroyObject( ragdoll.list[i].pObject );
		}
		ragdoll.list[i].pObject = nullptr;
	}
	physenv->DestroyConstraintGroup( ragdoll.pGroup );
	ragdoll.pGroup = nullptr;
	ragdoll.listCount = 0;
}

// Parse the ragdoll and obtain the mapping from each physics element index to a bone index
// returns num phys elements
int RagdollExtractBoneIndices( int *boneIndexOut, CStudioHdr *pStudioHdr, vcollide_t *pCollide )
{
	int elementCount = 0;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t solid;
			pParse->ParseSolid( &solid, nullptr );
			if ( elementCount < RAGDOLL_MAX_ELEMENTS )
			{
				boneIndexOut[elementCount] = Studio_BoneIndexByName( pStudioHdr, solid.name );
				elementCount++;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	return elementCount;
}

bool RagdollGetBoneMatrix( const ragdoll_t &ragdoll, CBoneAccessor &pBoneToWorld, int objectIndex )
{
	int boneIndex = ragdoll.boneIndex[objectIndex];
	if ( boneIndex < 0 )
		return false;

	const ragdollelement_t &element = ragdoll.list[objectIndex];

	// during restore if a model has changed since the file was saved, this could be NULL
	if ( !element.pObject )
		return false;
	element.pObject->GetPositionMatrix( &pBoneToWorld.GetBoneForWrite( boneIndex ) );
	if ( element.parentIndex >= 0 && !ragdoll.allowStretch )
	{
		// overwrite the position from physics to force rigid attachment
		// NOTE: On the client we actually override this with the proper parent bone in each LOD
		int parentBoneIndex = ragdoll.boneIndex[element.parentIndex];
		Vector out;
		VectorTransform( element.originParentSpace, pBoneToWorld.GetBone( parentBoneIndex ), out );
		MatrixSetColumn( out, 3, pBoneToWorld.GetBoneForWrite( boneIndex ) );
	}
	return true;
}

void RagdollComputeExactBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs )
{
	outMins = origin;
	outMaxs = origin;

	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector mins, maxs;
		IPhysicsObject *pObject = ragdoll.list[i].pObject;
		Vector objectOrg;
		QAngle objectAng;
		pObject->GetPosition( &objectOrg, &objectAng );
		physcollision->CollideGetAABB( &mins, &maxs, pObject->GetCollide(), objectOrg, objectAng );
		for ( int j = 0; j < 3; j++ )
		{
			if ( mins[j] < outMins[j] )
			{
				outMins[j] = mins[j];
			}
			if ( maxs[j] > outMaxs[j] )
			{
				outMaxs[j] = maxs[j];
			}
		}
	}
}

void RagdollComputeApproximateBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs )
{
	Vector mins, maxs;
	ClearBounds(mins,maxs);
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector objectOrg;
		ragdoll.list[i].pObject->GetPosition( &objectOrg, nullptr );
		float radius = physcollision->CollideGetRadius( ragdoll.list[i].pObject->GetCollide() );
		for ( int k = 0; k < 3; k++ )
		{
			float ext = objectOrg[k] + radius;
			maxs[k] = fpmax( maxs[k], ext );
			ext = objectOrg[k] - radius;
			mins[k] = fpmin( mins[k], ext );
		}
	}
	outMins = mins;
	outMaxs = maxs;
}

bool RagdollIsAsleep( const ragdoll_t &ragdoll )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		if ( ragdoll.list[i].pObject && !ragdoll.list[i].pObject->IsAsleep() )
			return false;
	}

	return true;
}

void RagdollSolveSeparation( ragdoll_t &ragdoll, CBaseEntity *pEntity )
{
	byte needsFix[256];
	int fixCount = 0;
	contract_assert( ragdoll.listCount<=std::size( needsFix ) );
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		needsFix[i] = 0;
		const ragdollelement_t &element = ragdoll.list[i];
		if ( element.pConstraint && element.parentIndex >= 0 )
		{
			Vector start, target;
			element.pObject->GetPosition( &start, nullptr );
			ragdoll.list[element.parentIndex].pObject->LocalToWorld( &target, element.originParentSpace );
			if ( needsFix[element.parentIndex] )
			{
				needsFix[i] = 1;
				++fixCount;
				continue;
			}
			Vector dir = target-start;
			if ( dir.LengthSqr() > 1.0f )
			{
				// this fixes a bug in ep2 with antlion grubs, but causes problems in TF2 - revisit, but disable for TF now
				// heuristic: guess that anything separated and small mass ratio is in some state that's 
				// keeping the solver from fixing it
				float mass = element.pObject->GetMass();
				float massParent = ragdoll.list[element.parentIndex].pObject->GetMass();

				if ( mass*2.0f < massParent )
				{
					// if this is <0.5 mass of parent and still separated it's attached to something heavy or 
					// in a bad state
					needsFix[i] = 1;
					++fixCount;
					continue;
				}

				if ( PhysHasContactWithOtherInDirection(element.pObject, dir) )
				{
					Ray_t ray;
					trace_t tr;
					ray.Init( target, start );
					UTIL_TraceRay( ray, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tr );
					if ( tr.DidHit() )
					{
						needsFix[i] = 1;
						++fixCount;
					}
				}
			}
		}
	}

	if ( fixCount )
	{
		for ( int i = 0; i < ragdoll.listCount; i++ )
		{
			if ( !needsFix[i] )
				continue;

			const ragdollelement_t &element = ragdoll.list[i];
			Vector target, velocity;
			ragdoll.list[element.parentIndex].pObject->LocalToWorld( &target, element.originParentSpace );
			ragdoll.list[element.parentIndex].pObject->GetVelocityAtPoint( target, &velocity );
			matrix3x4_t xform;
			element.pObject->GetPositionMatrix( &xform );
			MatrixSetColumn( target, 3, xform );
			element.pObject->SetPositionMatrix( xform, true );
			element.pObject->SetVelocity( &velocity, &vec3_origin );
		}
		DevMsg(2, "TICK:%5d:Ragdoll separation count: %d\n", gpGlobals->tickcount, fixCount );
	}
	else
	{
		ragdoll.pGroup->ClearErrorState();
	}
}

//-----------------------------------------------------------------------------
// LRU
//-----------------------------------------------------------------------------
ConVar g_ragdoll_maxcount(side_registry(), "g_ragdoll_maxcount", "8", FCVAR_REPLICATED );
ConVar g_debug_ragdoll_removal(side_registry(), "g_debug_ragdoll_removal", "0", FCVAR_REPLICATED |FCVAR_CHEAT );

CRagdollLRURetirement s_RagdollLRU( "CRagdollLRURetirement" );

void CRagdollLRURetirement::LevelInitPreEntity( void )
{
	m_iMaxRagdolls = -1;
	m_LRUImportantRagdolls.clear();
	m_LRU.clear();
}

bool ShouldRemoveThisRagdoll( CBaseAnimating *pRagdoll )
{
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		return true;
	}

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if( !UTIL_FindClientInPVS( pRagdoll->edict() ) )
	{
		if ( g_debug_ragdoll_removal.GetBool() )
			 NDebugOverlay::Line( pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsOrigin() + Vector( 0, 0, 64 ), 0, 255, 0, true, 5 );

		return true;
	}
	else if( !pPlayer->FInViewCone( pRagdoll ) )
	{
		if ( g_debug_ragdoll_removal.GetBool() )
			 NDebugOverlay::Line( pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsOrigin() + Vector( 0, 0, 64 ), 0, 0, 255, true, 5 );
		
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Cull stale ragdolls. There is an ifdef here: one version for episodic, 
// one for everything else.
//-----------------------------------------------------------------------------

void CRagdollLRURetirement::Update( float frametime ) // Non-episodic version
{
	CVProfScope _( "CRagdollLRURetirement::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	int iMaxRagdollCount = m_iMaxRagdolls;

	if ( iMaxRagdollCount == -1 )
	{
		iMaxRagdollCount = g_ragdoll_maxcount.GetInt();
	}

	// fade them all for the low violence version
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		iMaxRagdollCount = 0;
	}
	m_iRagdollCount = 0;
	m_iSimulatedRagdollCount = 0;

	// remove ragdolls with a forced retire time
	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		CBaseAnimating *pRagdoll = it->Get();

		//Just ignore it until we're done burning/dissolving.
		if ( pRagdoll && pRagdoll->GetEffectEntity() )
		{
			++it;
			continue;
		}

		// ignore if it's not time to force retire this ragdoll
		if ( it->GetForcedRetireTime() == 0.0f || gpGlobals->curtime < it->GetForcedRetireTime() )
		{
			++it;
			continue;
		}

		//Msg(" Removing ragdoll %s due to forced retire time of %f (now = %f)\n", pRagdoll->GetModelName(), it->GetForcedRetireTime(), gpGlobals->curtime );

		pRagdoll->SUB_StartFadeOut( 0 );
		it = m_LRU.erase( it );
	}

	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		CBaseAnimating *pRagdoll = it->Get();
		if ( pRagdoll )
		{
			m_iRagdollCount++;
			IPhysicsObject *pObject = pRagdoll->VPhysicsGetObject();
			if (pObject && !pObject->IsAsleep())
			{
				m_iSimulatedRagdollCount++;
			}
			if ( (int)m_LRU.size() > iMaxRagdollCount )
			{
				//Found one, we're done.
				if ( ShouldRemoveThisRagdoll( pRagdoll ) == true )
				{
					pRagdoll->SUB_StartFadeOut( 0 );

					m_LRU.erase( it );
					return;
				}
			}
			++it;
		}
		else 
		{
			it = m_LRU.erase( it );
		}
	}

	//////////////////////////////
	///   ORIGINAL ALGORITHM   ///
	//////////////////////////////
	// not episodic -- this is the original mechanism

	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		if ( (int)m_LRU.size() <=  iMaxRagdollCount )
			break;

		CBaseAnimating *pRagdoll = it->Get();

		//Just ignore it until we're done burning/dissolving.
		if ( pRagdoll && pRagdoll->GetEffectEntity() )
		{
			++it;
			continue;
		}

		pRagdoll->SUB_StartFadeOut( 0 );
		it = m_LRU.erase( it );
	}
}

//This is pretty hacky, it's only called on the server so it just calls the update method.
void CRagdollLRURetirement::FrameUpdatePostEntityThink( void )
{
	Update( 0 );
}

ConVar g_ragdoll_important_maxcount( side_registry(), "g_ragdoll_important_maxcount", "2", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Move it to the top of the LRU
//-----------------------------------------------------------------------------
void CRagdollLRURetirement::MoveToTopOfLRU( CBaseAnimating *pRagdoll, bool bImportant, float flForcedRetireTime )
{
	if ( bImportant )
	{
		m_LRUImportantRagdolls.emplace_back( pRagdoll, flForcedRetireTime );

		if ( (int)m_LRUImportantRagdolls.size() > g_ragdoll_important_maxcount.GetInt() )
		{
			CBaseAnimating *pRagdoll = m_LRUImportantRagdolls.front().Get();

			if ( pRagdoll )
			{
				pRagdoll->SUB_StartFadeOut( 0 );
				m_LRUImportantRagdolls.erase( m_LRUImportantRagdolls.begin() );
			}

		}
		return;
	}
	for ( auto it = m_LRU.begin(); it != m_LRU.end(); ++it )
	{
		if ( it->Get() == pRagdoll )
		{
			m_LRU.erase( it );
			break;
		}
	}

	m_LRU.emplace_back( pRagdoll, flForcedRetireTime );
}

//EFFECT/ENTITY TRANSFERS

//CLIENT

//SERVER

//-----------------------------------------------------------------------------
// Transfer dissolve
//-----------------------------------------------------------------------------
void CBaseAnimating::TransferDissolveFrom( CBaseAnimating *pAnim )
{
	if ( !pAnim || !pAnim->IsDissolving() )
		return;

	CEntityDissolve *pDissolve = CEntityDissolve::Create( this, pAnim );
	if (pDissolve)
	{
		AddFlag( FL_DISSOLVING );
		m_flDissolveStartTime = pAnim->m_flDissolveStartTime;

		CEntityDissolve *pDissolveFrom = dynamic_cast < CEntityDissolve * > (pAnim->GetEffectEntity());

		if ( pDissolveFrom )
		{
			pDissolve->SetDissolverOrigin( pDissolveFrom->GetDissolverOrigin() );
			pDissolve->SetDissolveType( pDissolveFrom->GetDissolveType() );

			if ( pDissolveFrom->GetDissolveType() == ENTITY_DISSOLVE_CORE )
			{
				pDissolve->SetMagnitude( pDissolveFrom->GetMagnitude() );
				pDissolve->m_flFadeOutStart = CORE_DISSOLVE_FADE_START;
				pDissolve->m_flFadeOutModelStart = CORE_DISSOLVE_MODEL_FADE_START;
				pDissolve->m_flFadeOutModelLength = CORE_DISSOLVE_MODEL_FADE_LENGTH;
				pDissolve->m_flFadeInLength = CORE_DISSOLVE_FADEIN_LENGTH;
			}
		}
	}
}

class CBaseAnimating;
class CBaseEntity;
class CEffectData;

using namespace ::ks::reflect;

// Purpose: Dissolve entity to be attached to target entity. Serves two purposes:
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//			2) An entity that can be created at runtime to ignite a target entity.

//-----------------------------------------------------------------------------
// Make electriciy every so often
//-----------------------------------------------------------------------------
static const char *s_pZapContext = "ZapContext";

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( env_ragdoll_boogie, CRagdollBoogie );

//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CRagdollBoogie *CRagdollBoogie::Create( CBaseEntity *pTarget, float flMagnitude, 
	float flStartTime, float flLengthTime, int nSpawnFlags )
{
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pTarget );
	if ( !pRagdoll )
		return nullptr;

	CRagdollBoogie *pBoogie = (CRagdollBoogie *)CreateEntityByName( "env_ragdoll_boogie" );
	if ( pBoogie == nullptr )
		return nullptr;

	pBoogie->AddSpawnFlags( nSpawnFlags );
	pBoogie->AttachToEntity( pTarget );
	pBoogie->SetBoogieTime( flStartTime, flLengthTime );
	pBoogie->SetMagnitude( flMagnitude );
	pBoogie->Spawn();
	return pBoogie;
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CRagdollBoogie::Precache_Impl()
{
	BaseClass::Precache_Impl();

	PrecacheEffect( "TeslaHitboxes" );

}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CRagdollBoogie::Spawn_Impl()
{
	Precache();

	BaseClass::Spawn_Impl();

	SetThink( &CRagdollBoogie::BoogieThink );
	SetNextThink( gpGlobals->curtime + 0.01f );

	if ( HasSpawnFlags( SF_RAGDOLL_BOOGIE_ELECTRICAL ) )
	{
		SetContextThink( &CRagdollBoogie::ZapThink, gpGlobals->curtime + random_valve->RandomFloat( 0.1f, 0.3f ), s_pZapContext ); 
	}
}

//-----------------------------------------------------------------------------
// Zap!
//-----------------------------------------------------------------------------
void CRagdollBoogie::ZapThink()
{
	if ( !GetMoveParent() )
		return;

	CBaseAnimating *pRagdoll = GetMoveParent()->GetBaseAnimating();
	if ( !pRagdoll )
		return;

	// Make electricity on the client
	CStudioHdr *pStudioHdr = pRagdoll->GetModelPtr( );
	if (!pStudioHdr)
		return;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pRagdoll->GetHitboxSet() );

	if ( set->numhitboxes == 0 )
		return;

	if ( m_nSuppressionCount == 0 )
	{
		CEffectData	data;
		
		data.m_nEntIndex = GetMoveParent()->entindex();
		data.m_flMagnitude = 4;
		data.m_flScale = HasSpawnFlags(SF_RAGDOLL_BOOGIE_ELECTRICAL_NARROW_BEAM) ? 1.0f : 2.0f;

		DispatchEffect( "TeslaHitboxes", data );	
	}

	SetContextThink( &CRagdollBoogie::ZapThink, gpGlobals->curtime + random_valve->RandomFloat( 0.1f, 0.3f ), s_pZapContext ); 
}

//-----------------------------------------------------------------------------
// Suppression count
//-----------------------------------------------------------------------------
void CRagdollBoogie::IncrementSuppressionCount( CBaseEntity *pTarget )
{
	// Look for other boogies on the ragdoll + kill them
	for ( CBaseEntity *pChild = pTarget->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
	{
		CRagdollBoogie *pBoogie = dynamic_cast<CRagdollBoogie*>(pChild);
		if ( !pBoogie )
			continue;

		++pBoogie->m_nSuppressionCount;
	}
}

void CRagdollBoogie::DecrementSuppressionCount( CBaseEntity *pTarget )
{
	// Look for other boogies on the ragdoll + kill them
	CBaseEntity *pNext;
	for ( CBaseEntity *pChild = pTarget->FirstMoveChild(); pChild; pChild = pNext )
	{
		pNext = pChild->NextMovePeer();
		CRagdollBoogie *pBoogie = dynamic_cast<CRagdollBoogie*>(pChild);
		if ( !pBoogie )
			continue;

		if ( --pBoogie->m_nSuppressionCount <= 0 )
		{
			pBoogie->m_nSuppressionCount = 0;

			float dt = gpGlobals->curtime - pBoogie->m_flStartTime;
			if ( dt >= pBoogie->m_flBoogieLength )
			{
				PhysCallbackRemove( pBoogie->NetworkProp() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Attach to an entity
//-----------------------------------------------------------------------------
void CRagdollBoogie::AttachToEntity( CBaseEntity *pTarget )
{
	m_nSuppressionCount = 0;

	// Look for other boogies on the ragdoll + kill them
	CBaseEntity *pNext;
	for ( CBaseEntity *pChild = pTarget->FirstMoveChild(); pChild; pChild = pNext )
	{
		pNext = pChild->NextMovePeer();
		CRagdollBoogie *pBoogie = dynamic_cast<CRagdollBoogie*>(pChild);
		if ( !pBoogie )
			continue;

		m_nSuppressionCount = pBoogie->m_nSuppressionCount;
		UTIL_Remove( pChild );
	}

	FollowEntity( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lifetime - 
//-----------------------------------------------------------------------------
void CRagdollBoogie::SetBoogieTime( float flStartTime, float flLengthTime )
{
	m_flStartTime = flStartTime;
	m_flBoogieLength = flLengthTime;
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CRagdollBoogie::SetMagnitude( float flMagnitude )
{
	m_flMagnitude = flMagnitude;
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CRagdollBoogie::BoogieThink( void )
{
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( GetMoveParent() );
	if ( !pRagdoll )
	{
		UTIL_Remove( this );
		return;
	}

	float flMagnitude = m_flMagnitude;
	if ( m_flBoogieLength != 0 )
	{
		float dt = gpGlobals->curtime - m_flStartTime;
		if ( dt >= m_flBoogieLength )
		{
			// Don't remove while suppressed... this helps if we try to start another boogie
			if ( m_nSuppressionCount == 0 )
			{
				UTIL_Remove( this );
			}
			SetThink( nullptr );
			return;
		}

		if ( dt < 0 )
		{
			SetNextThink( gpGlobals->curtime + random_valve->RandomFloat( 0.1, 0.2f ) );
			return;
		}

		flMagnitude = SimpleSplineRemapVal( dt, 0.0f, m_flBoogieLength, m_flMagnitude, 0.0f ); 
	}

	if ( m_nSuppressionCount == 0 )
	{
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			float flMass = pRagdollPhys->list[j].pObject->GetMass();
			float flForce = m_flMagnitude * flMass;

			Vector vecForce;
			vecForce = RandomVector( -flForce, flForce );
			pRagdollPhys->list[j].pObject->ApplyForceCenter( vecForce ); 
		}
	}

	SetNextThink( gpGlobals->curtime + random_valve->RandomFloat( 0.1, 0.2f ) );
}

class CBaseEntity;
class CBasePlayer;
class CRecipientFilter;
class CTeam;

using namespace ::ks::reflect;

static IPredictionSystem g_RecipientFilterPredictionSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRecipientFilter::CRecipientFilter()
{
	Reset();
}

CRecipientFilter::~CRecipientFilter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
void CRecipientFilter::CopyFrom( const CRecipientFilter& src )
{
	m_bReliable = src.IsReliable();
	m_bInitMessage = src.IsInitMessage();

	m_bUsingPredictionRules = src.IsUsingPredictionRules();
	m_bIgnorePredictionCull = src.IgnorePredictionCull();

	int c = src.GetRecipientCount();
	for ( int i = 0; i < c; ++i )
	{
		m_Recipients.push_back( src.GetRecipientIndex( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_bInitMessage		= false;
	m_Recipients.clear();
	m_bUsingPredictionRules = false;
	m_bIgnorePredictionCull = false;
}

void CRecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}

bool CRecipientFilter::IsReliable( void ) const
{
	return m_bReliable;
}

int CRecipientFilter::GetRecipientCount( void ) const
{
	return std::ssize( m_Recipients );
}

int	CRecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void CRecipientFilter::AddAllPlayers( void )
{
	m_Recipients.clear();

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
		{
			continue;
		}

		AddRecipient( pPlayer );
	}
}

void CRecipientFilter::AddRecipient( CBasePlayer *player )
{
	contract_assert( player );

	if ( !player )
		return;

	int index = player->entindex();

	// If we're predicting and this is not the first time we've predicted this sound
	//  then don't send it to the local player again.
	if ( m_bUsingPredictionRules )
	{
		// Only add local player if this is the first time doing prediction
		if ( g_RecipientFilterPredictionSystem.GetSuppressHost() == player )
		{
			return;
		}
	}

	// Already in list
	if ( ks::find_index( m_Recipients, index ) != -1 )
		return;

	m_Recipients.push_back( index );
}

void CRecipientFilter::RemoveAllRecipients( void )
{
	m_Recipients.clear();
}

void CRecipientFilter::RemoveRecipient( CBasePlayer *player )
{
	contract_assert( player );
	if ( player )
	{
		int index = player->entindex();

		// Remove it if it's in the list
		ks::erase_first( m_Recipients, index );
	}
}

void CRecipientFilter::RemoveRecipientByPlayerIndex( int playerindex )
{
	contract_assert( playerindex >= 1 && playerindex <= MAX_PLAYERS );

	ks::erase_first( m_Recipients, playerindex );
}

void CRecipientFilter::AddRecipientsByTeam( CTeam *team )
{
	contract_assert( team );

	int i;
	int c = team->GetNumPlayers();
	for ( i = 0 ; i < c ; i++ )
	{
		CBasePlayer *player = team->GetPlayer( i );
		if ( !player )
			continue;

		AddRecipient( player );
	}
}

void CRecipientFilter::RemoveRecipientsByTeam( CTeam *team )
{
	contract_assert( team );

	int i;
	int c = team->GetNumPlayers();
	for ( i = 0 ; i < c ; i++ )
	{
		CBasePlayer *player = team->GetPlayer( i );
		if ( !player )
			continue;

		RemoveRecipient( player );
	}
}

void CRecipientFilter::RemoveRecipientsNotOnTeam( CTeam *team )
{
	contract_assert( team );

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );
		if ( !player )
			continue;

		if ( player->GetTeam() != team )
		{
			RemoveRecipient( player );
		}
	}
}

void CRecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	int index = ks::next_set_bit( playerbits, 0 );

	while ( index > -1 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index + 1 );
		if ( pPlayer )
		{
			AddRecipient( pPlayer );
		}

		index = ks::next_set_bit( playerbits, index + 1 );
	}
}

void CRecipientFilter::RemovePlayersFromBitMask( CPlayerBitVec& playerbits )
{
	int index = ks::next_set_bit( playerbits, 0 );

	while ( index > -1 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index + 1 );
		if ( pPlayer )
		{
			RemoveRecipient( pPlayer );
		}

		index = ks::next_set_bit( playerbits, index + 1 );
	}
}

void CRecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( false, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}

void CRecipientFilter::RemoveRecipientsByPVS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		m_Recipients.clear();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( false, origin, playerbits );
		RemovePlayersFromBitMask( playerbits );
	}
}

void CRecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( true, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}

bool CRecipientFilter::IsInitMessage( void ) const
{
	return m_bInitMessage;
}

void CRecipientFilter::MakeInitMessage( void )
{
	m_bInitMessage = true;
}

void CRecipientFilter::UsePredictionRules( void )
{
	if ( m_bUsingPredictionRules )
		return;

	m_bUsingPredictionRules = true;

	// Cull list now, if needed
	if ( GetRecipientCount() == 0 )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( (CBaseEntity*)g_RecipientFilterPredictionSystem.GetSuppressHost() );

	if ( pPlayer)
	{
		RemoveRecipient( pPlayer );

		if ( pPlayer->IsSplitScreenPlayer() )
		{
			RemoveRecipient( pPlayer->GetSplitScreenPlayerOwner() );
		}
		else
		{
			std::vector< CHandle< CBasePlayer > > &players = pPlayer->GetSplitScreenAndPictureInPicturePlayers();
			for ( int i = 0; i < std::ssize( players ); ++i )
			{
				CBasePlayer *pl = players[i];
				if ( !pl )
				{
					continue;
				}
				RemoveRecipient( pl );
			}
		}
	}
}

void CRecipientFilter::RemoveSplitScreenPlayers()
{
	for ( int i = GetRecipientCount() - 1; i >= 0; --i )
	{
		int idx = m_Recipients[ i ];
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( idx );
		if ( !pPlayer || !pPlayer->IsSplitScreenPlayer() )
			continue;

		m_Recipients.erase( m_Recipients.begin() + i );
	}
}

// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
void CRecipientFilter::ReplaceSplitScreenPlayersWithOwners()
{
	// coop
	if( gpGlobals->maxClients >= 2 ) 
	{
		int count = std::ssize( m_Recipients );
		ks::small_vector< int, 4 > playerOwners;
		for( int i = 0; i < count; ++i )
		{
			// If this is a split screen player
			CBasePlayer* pPlayer = UTIL_PlayerByIndex( m_Recipients[i] );
			if( pPlayer && pPlayer->IsSplitScreenPlayer() )
			{
				// Add its owner to the recipients. If it doesn't exist, abort.
				const CBasePlayer* pOwnerPlayer = pPlayer->GetSplitScreenPlayerOwner();
				if( pOwnerPlayer )
					playerOwners.push_back( pOwnerPlayer->entindex() );
				else
					return;
			}
		}

		if( std::ssize( playerOwners ) > 0 )
		{
			// Remove all split screen players
			RemoveSplitScreenPlayers();

			// Add all owner players
			count = std::ssize( m_Recipients );
			ks::ensure_size( m_Recipients, count + std::ssize( playerOwners ) );
			V_memcpy( m_Recipients.data() + count, playerOwners.data(), std::ssize( playerOwners ) * sizeof( int ) );

			// Remove duplicates
			RemoveDuplicateRecipients();
		}
	}
}

bool CRecipientFilter::IsUsingPredictionRules( void ) const
{
	return m_bUsingPredictionRules;
}

bool CRecipientFilter::	IgnorePredictionCull( void ) const
{
	return m_bIgnorePredictionCull;
}

void CRecipientFilter::SetIgnorePredictionCull( bool ignore )
{
	m_bIgnorePredictionCull = ignore;
}

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players on a given team 
//-----------------------------------------------------------------------------
CTeamRecipientFilter::CTeamRecipientFilter( int team, bool isReliable )
{
	if (isReliable)
		MakeReliable();

	RemoveAllRecipients();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
		{
			continue;
		}

		if ( team == TEAM_SPECTATOR )
		{
			if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR && !pPlayer->IsHLTV() )
				continue;
		}
		else if ( pPlayer->GetTeamNumber() != team )
		{
			//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
			if ( ( pPlayer->IsHLTV() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) && (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pPlayer->GetObserverMode() == OBS_MODE_CHASE) )
			{
				if ( pPlayer->GetObserverTarget() )
				{
					if ( pPlayer->GetObserverTarget()->GetTeamNumber() != team )
						continue;
				}
			}
			else
			{
				continue;
			}
		}

		AddRecipient( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			ATTN_NORM - 
//-----------------------------------------------------------------------------
void CPASAttenuationFilter::Filter( const Vector& origin, float attenuation /*= ATTN_NORM*/ )
{
	// Don't crop for attenuation in single player
	if ( gpGlobals->maxClients == 1 )
		return;

	// CPASFilter adds them by pure PVS in constructor
	if ( attenuation <= 0 )
	{
		AddAllPlayers();
		return;
	}

	// Now remove recipients that are outside sound radius
	float maxAudible = ( 2 * SOUND_NORMAL_CLIP_DIST ) / attenuation;

	int c = GetRecipientCount();
	
	for ( int i = c - 1; i >= 0; i-- )
	{
		int index = GetRecipientIndex( i );

		CBaseEntity *ent = CBaseEntity::Instance( index );
		if ( !ent || !ent->IsPlayer() )
		{
			contract_assert( 0 );
			continue;
		}

		CBasePlayer *player = ToBasePlayer( ent );
		if ( !player )
		{
			contract_assert( 0 );
			continue;
		}

		// never remove the HLTV or Replay bot
		if ( player->IsHLTV() || player->IsReplay() )
			continue;

		if ( player->EarPosition().DistTo(origin) <= maxAudible )
			continue;
		if ( std::ssize( player->GetSplitScreenAndPictureInPicturePlayers() ) )
		{
			std::vector< CHandle< CBasePlayer > > &list = player->GetSplitScreenAndPictureInPicturePlayers();
			bool bSend = false;
			for ( int k = 0; k < std::ssize( list ); k++ )
			{
				CBasePlayer *pl = list[k];
				if ( !pl )
				{
					continue;
				}
				if ( pl->EarPosition().DistTo(origin) <= maxAudible )
				{
					bSend = true;
					break;
				}
			}
			if ( bSend )
				continue;
		}
		RemoveRecipient( player );
	}
}

void CRecipientFilter::RemoveDuplicateRecipients()
{
	for( int i = 0; i < std::ssize( m_Recipients ); ++i )
	{
		int currentElem = m_Recipients[i];
		for( int j = std::ssize( m_Recipients ) - 1; j > i ; --j )
		{
			if( m_Recipients[j] == currentElem )
				ks::swap_remove( m_Recipients, j );
		}
	}
}

CSingleUserAndReplayRecipientFilter::CSingleUserAndReplayRecipientFilter( CBasePlayer *pAddPlayer )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
		{
			continue;
		}
		contract_assert( pPlayer->entindex() == i );

		if ( pPlayer == pAddPlayer || pPlayer->IsHLTV() || pPlayer->IsReplay() )
		{
			// If we're predicting and this is not the first time we've predicted this sound
			//  then don't send it to the local player again.
			contract_assert( !IsUsingPredictionRules () );

			m_Recipients.push_back( i );
		}
	}
}

CPASAttenuationFilter::CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound) :
		CPASFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()) )
{
	soundlevel_t level = CBaseEntity::LookupSoundLevel( lookupSound );
	float attenuation = SNDLVL_TO_ATTN( level );
	Filter( entity->GetSoundEmissionOrigin(), attenuation );
}

CPASAttenuationFilter::CPASAttenuationFilter( const Vector& origin, const char *lookupSound) :
		CPASFilter( origin )
{
	soundlevel_t level = CBaseEntity::LookupSoundLevel( lookupSound );
	float attenuation = SNDLVL_TO_ATTN( level );
	Filter( origin, attenuation );
}

CPASAttenuationFilter::CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound, HSOUNDSCRIPTHASH& handle) :
		CPASFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()) )
{
	soundlevel_t level = CBaseEntity::LookupSoundLevel( lookupSound, handle );
	float attenuation = SNDLVL_TO_ATTN( level );
	Filter( entity->GetSoundEmissionOrigin(), attenuation );
}

CPASAttenuationFilter::CPASAttenuationFilter( const Vector& origin, const char *lookupSound, HSOUNDSCRIPTHASH& handle) :
		CPASFilter( origin )
{
	soundlevel_t level = CBaseEntity::LookupSoundLevel( lookupSound, handle );
	float attenuation = SNDLVL_TO_ATTN( level );
	Filter( origin, attenuation );
}

class CBaseAnimating;
class CBaseEntity;
class CEffectData;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

//--------------------------------------------
// Rope Spawn Flags
//--------------------------------------------
static constexpr auto SF_ROPE_RESIZE = 1; // Automatically resize the rope

// -------------------------------------------------------------------------------- //
// Fun With Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( move_rope, CRopeKeyframe );
LINK_ENTITY_TO_CLASS_ALIAS( keyframe_rope, CRopeKeyframe );

IMPLEMENT_REFLECT_SERVERCLASS( CRopeKeyframe, DT_RopeKeyframe )

IMPLEMENT_REFLECT_DATAMAP( CRopeKeyframe )

// -------------------------------------------------------------------------------- //
// CRopeKeyframe implementation.
// -------------------------------------------------------------------------------- //

CRopeKeyframe::CRopeKeyframe()
{
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	
	m_takedamage = DAMAGE_YES;

	m_iStartAttachment = m_iEndAttachment = 0;

	m_Slack = 0;
	m_Width = 2;
	m_TextureScale = 4;	// 4:1
	m_nSegments = 5;
	m_RopeLength = 20;
	m_fLockedPoints = (int) (ROPE_LOCK_START_POINT | ROPE_LOCK_END_POINT); // by default, both points are locked
	m_flScrollSpeed = 0;
	m_RopeFlags = ROPE_SIMULATE | ROPE_INITIAL_HANG;
	m_iRopeMaterialModelIndex = -1;
	m_Subdiv = 2;
	m_iDefaultRopeMaterialModelIndex = -1;

	m_bCreatedFromMapFile = true;
}

CRopeKeyframe::~CRopeKeyframe()
{
	// Release transmit state ownership.
	SetStartPoint( nullptr, 0 );
	SetEndPoint( nullptr, 0 );
	SetParent( nullptr, 0 );
}

void CRopeKeyframe::SetAttachmentPoint( CBaseHandle &hOutEnt, short &iOutAttachment, CBaseEntity *pEnt, int iAttachment )
{
	// Unforce our previously attached entity from transmitting.
	CBaseEntity *pCurEnt = gEntList.GetBaseEntity( hOutEnt );
	if ( pCurEnt && pCurEnt->edict() )
	{
		pCurEnt->DecrementTransmitStateOwnedCounter();
		pCurEnt->DispatchUpdateTransmitState();
	}
	
	hOutEnt = pEnt;
	iOutAttachment = iAttachment;

	// Force this entity to transmit.
	if ( pEnt )
	{
		pEnt->SetTransmitState( FL_EDICT_ALWAYS );
		pEnt->IncrementTransmitStateOwnedCounter();
	}

	EndpointsChanged();
}

void CRopeKeyframe::SetStartPoint( CBaseEntity *pStartPoint, int attachment )
{
	SetAttachmentPoint( m_hStartPoint.GetForModify(), m_iStartAttachment.GetForModify(), pStartPoint, attachment );
}

void CRopeKeyframe::SetEndPoint( CBaseEntity *pEndPoint, int attachment )
{
	SetAttachmentPoint( m_hEndPoint.GetForModify(), m_iEndAttachment.GetForModify(), pEndPoint, attachment );
}

void CRopeKeyframe::SetParent_Impl( CBaseEntity *pNewParent, int iAttachment )
{
	CBaseEntity *pCurParent = GetMoveParent();
	if ( pCurParent )
	{
		pCurParent->DecrementTransmitStateOwnedCounter();
		pCurParent->DispatchUpdateTransmitState();
	}

	// Make sure our move parent always transmits or we get asserts on the client.
	if ( pNewParent	)
	{
		pNewParent->IncrementTransmitStateOwnedCounter();
		pNewParent->SetTransmitState( FL_EDICT_ALWAYS );
	}

	BaseClass::SetParent_Impl( pNewParent, iAttachment );
}

void CRopeKeyframe::EnablePlayerWeaponAttach( bool bAttach )
{
	int newFlags = m_RopeFlags;
	if ( bAttach )
		newFlags |= ROPE_PLAYER_WPN_ATTACH;
	else
		newFlags &= ~ROPE_PLAYER_WPN_ATTACH;

	if ( newFlags != m_RopeFlags )
	{
		m_RopeFlags = newFlags;
	}
}

CRopeKeyframe* CRopeKeyframe::Create(
	CBaseEntity *pStartEnt,
	CBaseEntity *pEndEnt,
	int iStartAttachment,
	int iEndAttachment,
	int ropeWidth,
	const char *pMaterialName,
	int numSegments,
	const char *pClassName
	)
{
	CRopeKeyframe *pRet = (CRopeKeyframe*)CreateEntityByName( pClassName );
	if( !pRet )
		return nullptr;

	pRet->SetStartPoint( pStartEnt, iStartAttachment );
	pRet->SetEndPoint( pEndEnt, iEndAttachment );
	pRet->m_bCreatedFromMapFile = false;
	pRet->m_RopeFlags &= ~ROPE_INITIAL_HANG;

	pRet->Init();

	pRet->SetMaterial( pMaterialName );
	pRet->m_Width = ropeWidth;
	pRet->m_nSegments = clamp( numSegments, 2, ROPE_MAX_SEGMENTS );
	pRet->Spawn();

	return pRet;
}

CRopeKeyframe* CRopeKeyframe::CreateWithSecondPointDetached(
	CBaseEntity *pStartEnt,
	int iStartAttachment,
	int ropeLength,
	int ropeWidth,
	const char *pMaterialName,
	int numSegments,
	bool bInitialHang,
	const char *pClassName
	)
{
	CRopeKeyframe *pRet = (CRopeKeyframe*)CreateEntityByName( pClassName );
	if( !pRet )
		return nullptr;

	pRet->SetStartPoint( pStartEnt, iStartAttachment );
	pRet->SetEndPoint( nullptr, 0 );
	pRet->m_bCreatedFromMapFile = false;
	pRet->m_fLockedPoints.Set( ROPE_LOCK_START_POINT ); // Only attach the first point.

	if( !bInitialHang )
	{
		pRet->m_RopeFlags &= ~ROPE_INITIAL_HANG;
	}

	pRet->Init();

	pRet->SetMaterial( pMaterialName );
	pRet->m_RopeLength = ropeLength;
	pRet->m_Width = ropeWidth;
	pRet->m_nSegments = clamp( numSegments, 2, ROPE_MAX_SEGMENTS );

	return pRet;
}

void CRopeKeyframe::ActivateStartDirectionConstraints( bool bEnable )
{
	if (bEnable)
	{
		m_fLockedPoints.Set( m_fLockedPoints | ROPE_LOCK_START_DIRECTION ); 
	}
	else
	{
		m_fLockedPoints &= ~((int)ROPE_LOCK_START_DIRECTION); 
	}
}

void CRopeKeyframe::ActivateEndDirectionConstraints( bool bEnable )
{
	if (bEnable)
	{
		m_fLockedPoints.Set( m_fLockedPoints | ROPE_LOCK_END_DIRECTION ); 
	}
	else
	{
		m_fLockedPoints &= ~((int)ROPE_LOCK_END_DIRECTION); 
	}
}

void CRopeKeyframe::PrecacheShakeRopes()
{
	PrecacheEffect( "ShakeRopes" );
}

void CRopeKeyframe::ShakeRopes( const Vector &vCenter, float flRadius, float flMagnitude )
{
	CEffectData shakeData;
	shakeData.m_vOrigin = vCenter;
	shakeData.m_flRadius = flRadius;
	shakeData.m_flMagnitude = flMagnitude;
	DispatchEffect( "ShakeRopes", shakeData );
}

bool CRopeKeyframe::SetupHangDistance( float flHangDist )
{
	CBaseEntity *pEnt1 = m_hStartPoint.Get();
	CBaseEntity *pEnt2 = m_hEndPoint.Get();
	if ( !pEnt1 || !pEnt2 )
		return false;

	// Calculate starting conditions so we can force it to hang down N inches.
	Vector v1 = pEnt1->GetAbsOrigin();
	if ( pEnt1->GetBaseAnimating() )
		pEnt1->GetBaseAnimating()->GetAttachment( m_iStartAttachment, v1 );
		
	Vector v2 = pEnt2->GetAbsOrigin();
	if ( pEnt2->GetBaseAnimating() )
		pEnt2->GetBaseAnimating()->GetAttachment( m_iEndAttachment, v2 );

	float flSlack, flLen;
	CalcRopeStartingConditions( v1, v2, ROPE_MAX_SEGMENTS, flHangDist, &flLen, &flSlack );

	m_RopeLength = (int)flLen;
	m_Slack = (int)flSlack;
	return true;
}

void CRopeKeyframe::Init()
{
	SetLocalAngles( vec3_angle );
	RecalculateLength();

	m_nSegments = clamp( m_nSegments.Get(), 2, ROPE_MAX_SEGMENTS );

	UpdateBBox( true );

	m_bStartPointValid = (m_hStartPoint.Get() != nullptr);
	m_bEndPointValid = (m_hEndPoint.Get() != nullptr);

	// Sanity-check the rope texture scale before it goes over the wire
	if ( m_TextureScale < 0.1f )
	{
		Vector origin = GetAbsOrigin();
		GetEndPointPos( 0, origin );
		DevMsg( "move_rope has TextureScale less than 0.1 at (%2.2f, %2.2f, %2.2f)\n",
			origin.x, origin.y, origin.z );
		m_TextureScale = 0.1f;
	}
	else if ( m_TextureScale > 10.0f )
	{
		Vector origin = GetAbsOrigin();
		GetEndPointPos( 0, origin );
		DevMsg( "move_rope has TextureScale greater than 10 at (%2.2f, %2.2f, %2.2f)\n",
			origin.x, origin.y, origin.z );
		m_TextureScale = 10.0f;
	}
}

void CRopeKeyframe::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	if( !m_bCreatedFromMapFile )
		return;

	if ( m_iDefaultRopeMaterialModelIndex == -1 )
	{
		m_iDefaultRopeMaterialModelIndex = PrecacheModel( "cable/cable.vmt" );
	}

	// Legacy support..
	if ( m_iRopeMaterialModelIndex == -1 )
	{
		m_iRopeMaterialModelIndex = m_iDefaultRopeMaterialModelIndex;
	}

	// Find the next entity in our chain.
	CBaseEntity *pEnt = gEntList.FindEntityByName( nullptr, m_iNextLinkName );
	if( pEnt && pEnt->edict() )
	{
		SetEndPoint( pEnt );

		if( m_spawnflags & SF_ROPE_RESIZE )
			m_RopeFlags |= ROPE_RESIZE;
	}
	else
	{
		// If we're from the map file, and we don't have a target ent, and 
		// "Start Dangling" wasn't set, then this rope keyframe doesn't have
		// any rope coming out of it.
		if ( m_fLockedPoints & (int)ROPE_LOCK_END_POINT )
		{
			m_RopeFlags &= ~ROPE_SIMULATE;
		}
	}

	// By default, our start point is our own entity.
	SetStartPoint( this );

	// If we don't do this here, then when we save/load, we won't "own" the transmit 
	// state of our parent, so the client might get our entity without our parent entity.
	SetParent( GetParent(), GetParentAttachment() );

	EndpointsChanged();

	Init();
}

void CRopeKeyframe::EndpointsChanged()
{
	CBaseEntity *pStartEnt = m_hStartPoint.Get();
	if ( pStartEnt )
	{
		if ( (pStartEnt != this) || GetMoveParent() )
		{
			WatchPositionChanges( this, pStartEnt );
		}
	}
	CBaseEntity *pEndEnt = m_hEndPoint.Get();
	if ( pEndEnt )
	{
		if ( (pEndEnt != this) || GetMoveParent() )
		{
			WatchPositionChanges( this, pEndEnt );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the length of the rope
//-----------------------------------------------------------------------------
void CRopeKeyframe::RecalculateLength( void )
{
	// Get my entities
	if( m_hEndPoint.Get() )
	{
		CBaseEntity *pStartEnt = m_hStartPoint.Get();
		CBaseEntity *pEndEnt = m_hEndPoint.Get();

		// Set the length
		m_RopeLength = (int)( pStartEnt->GetAbsOrigin() - pEndEnt->GetAbsOrigin() ).Length();
	}
	else
	{
		m_RopeLength = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: This should remove the rope next time it reaches a resting state.
//			Right now only the client knows when it reaches a resting state, so
//			for now it just removes itself after a short time.
//-----------------------------------------------------------------------------
void CRopeKeyframe::DieAtNextRest( void )
{
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

void CRopeKeyframe::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	if ( !(*pInfo->m_pTransmitEdict)[ entindex() ] )
	{	
		BaseClass::SetTransmit_Impl( pInfo, bAlways );
	
		// Make sure our target ents are sent too.
		CBaseEntity *pEnt = m_hStartPoint;
		if ( pEnt )
			pEnt->SetTransmit( pInfo, bAlways );

		pEnt = m_hEndPoint;
		if ( pEnt )
			pEnt->SetTransmit( pInfo, bAlways );
	}
}

bool CRopeKeyframe::GetEndPointPos2( CBaseEntity *pAttached, int iAttachment, Vector &vPos )
{
	if( !pAttached )
		return false;

	if ( iAttachment > 0 )
	{
		CBaseAnimating *pAnim = pAttached->GetBaseAnimating();
		if ( pAnim )
		{
			if( !pAnim->GetAttachment( iAttachment, vPos ) )
				return false;
		}
		else
		{
			return false;
		}
	}
	else
	{
		vPos = pAttached->GetAbsOrigin();
	}

	return true;
}

bool CRopeKeyframe::GetEndPointPos( int iPt, Vector &v )
{
	if ( iPt == 0 )
		return GetEndPointPos2( m_hStartPoint, m_iStartAttachment, v );
	else
		return GetEndPointPos2( m_hEndPoint, m_iEndAttachment, v );
}

void CRopeKeyframe::UpdateBBox( bool bForceRelink )
{
	Vector v1, v2;
	Vector vMin, vMax;
	if ( GetEndPointPos( 0, v1 ) )
	{
		if ( GetEndPointPos( 1, v2 ) )
		{
			VectorMin( v1, v2, vMin );
			VectorMax( v1, v2, vMax );

			// Set our bounds to enclose both endpoints and relink.
			vMin -= GetAbsOrigin();
			vMax -= GetAbsOrigin();
		}
		else
		{
			vMin = vMax = v1 - GetAbsOrigin();
		}
	}
	else
	{
		vMin = vMax = Vector( 0, 0, 0 );
	}

	if ( WorldAlignMins() != vMin || WorldAlignMaxs() != vMax )
	{
		UTIL_SetSize( this, vMin, vMax );
	}
}

//------------------------------------------------------------------------------
// Purpose : Propagate force to each link in the rope.  Check for loops
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CRopeKeyframe::PropagateForce(CBaseEntity *pActivator, CBaseEntity *pCaller, CBaseEntity *pFirstLink, float x, float y, float z)
{
	EntityMessageBegin( this, true );
		WRITE_FLOAT( x );
		WRITE_FLOAT( y );
		WRITE_FLOAT( z );
	MessageEnd();

	// UNDONE: Doesn't deal with intermediate loops
	// Propagate to next segment
	CRopeKeyframe *pNextLink = dynamic_cast<CRopeKeyframe*>((CBaseEntity *)m_hEndPoint);
	if (pNextLink && pNextLink != pFirstLink)
	{
		pNextLink->PropagateForce(pActivator, pCaller, pFirstLink, x, y, z);
	}
}

//------------------------------------------------------------------------------
// Purpose: Set an instaneous force on the rope.
// Input  : Force vector.
//------------------------------------------------------------------------------
void CRopeKeyframe::InputSetForce( inputdata_t &inputdata )
{
	Vector vecForce;
	inputdata.value.Vector3D(vecForce);
	PropagateForce( inputdata.pActivator, inputdata.pCaller, this, vecForce.x, vecForce.y, vecForce.z );
}

//-----------------------------------------------------------------------------
// Purpose: Breaks the rope if able
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CRopeKeyframe::InputBreak( inputdata_t &inputdata )
{
	//Route through the damage code
	Break();
}

//-----------------------------------------------------------------------------
// Purpose: Breaks the rope
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CRopeKeyframe::Break( void )
{
	DetachPoint( 0 );

	// Find whoever references us and detach us from them.
	// UNDONE: PERFORMANCE: This is very slow!!!
	CRopeKeyframe *pTest = nullptr;
	pTest = gEntList.NextEntByClass( pTest );
	while ( pTest )
	{
		if( stricmp( STRING(pTest->m_iNextLinkName), STRING(GetEntityName()) ) == 0 )
		{
			pTest->DetachPoint( 1 );
		}
	
		pTest = gEntList.NextEntByClass( pTest );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRopeKeyframe::NotifyPositionChanged( CBaseEntity *pEntity )
{
	++m_nChangeCount;

	// Update our bbox?
	UpdateBBox( false );

	CBaseEntity *ents[2] = { m_hStartPoint.Get(), m_hEndPoint.Get() };
	if ( (m_RopeFlags & ROPE_RESIZE) && ents[0] && ents[0]->edict() && ents[1] && ents[1]->edict() )
	{
		int len = (int)( ents[0]->GetAbsOrigin() - ents[1]->GetAbsOrigin() ).Length() + m_Slack;
		if ( len != m_RopeLength )
		{
			m_RopeLength = len;
		}
	}

	// Figure out if our attachment points have gone away and make sure to update the client if they have.
	bool *pValid[2] = { &m_bStartPointValid, &m_bEndPointValid };
	for ( int i=0; i < 2; i++ )
	{
		bool bCurrentlyValid = ( ents[i] != nullptr );
		if ( *pValid[i] != bCurrentlyValid )
		{
			*pValid[i] = bCurrentlyValid;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Take damage will break the rope
//-----------------------------------------------------------------------------
int CRopeKeyframe::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	// Only allow this if it's been marked
	if( !(m_RopeFlags & ROPE_BREAKABLE) )
		return false;

	Break();
	return 0;
}

void CRopeKeyframe::Precache_Impl()
{
	m_iRopeMaterialModelIndex = PrecacheModel( STRING( m_strRopeMaterialModel ) );
	PrecacheMaterial( "cable/rope_shadowdepth" );

	BaseClass::Precache_Impl();
}

void CRopeKeyframe::Spawn_Impl( void ) 
{
	BaseClass::Spawn_Impl();
	Precache();
}

void CRopeKeyframe::DetachPoint( int iPoint )
{
	contract_assert( iPoint == 0 || iPoint == 1 );
	
	m_fLockedPoints &= ~(1 << iPoint);
}

void CRopeKeyframe::EnableCollision()
{
	if( !( m_RopeFlags & ROPE_COLLIDE ) )
	{
		m_RopeFlags |= ROPE_COLLIDE;
	}
}

void CRopeKeyframe::EnableWind( bool bEnable )
{
	int flag = 0;
	if ( bEnable )
	{
		flag |= ROPE_USE_WIND;
	}

	if ( (m_RopeFlags & ROPE_USE_WIND) != flag )
	{
		m_RopeFlags |= flag;
	}
}

bool CRopeKeyframe::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if( stricmp( szKeyName, "Breakable" ) == 0 )
	{
		if( std::atoi( szValue ) == 1 )
		{
			m_RopeFlags |= ROPE_BREAKABLE;
		}
	}
	else if( stricmp( szKeyName, "Collide" ) == 0 )
	{
		if( std::atoi( szValue ) == 1 )
			m_RopeFlags |= ROPE_COLLIDE;
	}
	else if( stricmp( szKeyName, "Barbed" ) == 0 )
	{
		if( std::atoi( szValue ) == 1 )
			m_RopeFlags |= ROPE_BARBED;
	}
	else if( stricmp( szKeyName, "UseWind" ) == 0 )
	{
		if( std::atoi( szValue ) == 1 )
		{
			m_RopeFlags |= ROPE_USE_WIND;
		}
	}
	else if( stricmp( szKeyName, "Dangling" ) == 0 )
	{
		if( std::atoi( szValue ) == 1 )
		{
			m_fLockedPoints &= ~ROPE_LOCK_END_POINT; // detach our dest point
		}
		
		return true;
	}
	else if( stricmp( szKeyName, "Type" ) == 0 )
	{
		int iType = std::atoi( szValue );
		if( iType == 0 )
			m_nSegments = ROPE_MAX_SEGMENTS;
		else if( iType == 1 )
			m_nSegments = ROPE_TYPE1_NUMSEGMENTS;
		else
			m_nSegments = ROPE_TYPE2_NUMSEGMENTS;
	}
	else if ( stricmp( szKeyName, "RopeShader" ) == 0 )
	{
		// Legacy support for the RopeShader parameter.
		int iShader = std::atoi( szValue );
		if ( iShader == 0 )
		{
			m_strRopeMaterialModel = MAKE_STRING( "cable/cable.vmt" );
		}
		else if ( iShader == 1 )
		{
			m_strRopeMaterialModel = MAKE_STRING( "cable/rope.vmt" );
		}
		else
		{
			m_strRopeMaterialModel = MAKE_STRING( "cable/chain.vmt" );
		}
	}
	else if ( stricmp( szKeyName, "RopeMaterial" ) == 0 )
	{
		// Make sure we have a vmt extension.
		if ( V_stristr( szValue, ".vmt" ) )
		{
			SetMaterial( szValue );
		}
		else
		{
			char str[512];
			V_snprintf( str, sizeof( str ), "%s.vmt", szValue );
			SetMaterial( str );
		}
	}
	
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that sets the scroll speed.
//-----------------------------------------------------------------------------
void CRopeKeyframe::InputSetScrollSpeed( inputdata_t &inputdata )
{
	m_flScrollSpeed = inputdata.value.Float();
}

void CRopeKeyframe::SetMaterial( const char *pName )
{
	m_strRopeMaterialModel = AllocPooledString( pName );
}

int CRopeKeyframe::UpdateTransmitState_Impl()
{
	// Certain entities like sprites and ropes are strewn throughout the level and they rarely change.
	// For these entities, it's more efficient to transmit them once and then always leave them on
	// the client. Otherwise, the server will have to send big bursts of data with the entity states
	// as they come in and out of the PVS.
	return SetTransmitState( FL_EDICT_ALWAYS );
}
