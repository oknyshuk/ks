// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "querycache.h"
#include "datacache/imdlcache.h"

constexpr auto QUERYCACHE_SIZE = 1024;

static QueryCacheEntry_t s_QCache[QUERYCACHE_SIZE];

constexpr auto QUERYCACHE_HASH_SIZE = ( QUERYCACHE_SIZE  * 2 );

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


ConVar	sv_disable_querycache("sv_disable_querycache", "0", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "debug - disable trace query cache" );

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


constexpr auto N_WAYS_TO_SPLIT_CACHE_UPDATE = 8;

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


#if defined( CLIENT_DLL )
static void cl_querycache_stats( const CCommand &args );
static ConCommand cl_querycache_stats_command( "cl_querycache_stats", cl_querycache_stats, "Display status of the query cache (client only)", FCVAR_CHEAT );
static void cl_querycache_stats( const CCommand &args )
#else
static void sv_querycache_stats( const CCommand &args );
static ConCommand sv_querycache_stats_command( "sv_querycache_stats", sv_querycache_stats, "Display status of the query cache (client only)" );
static void sv_querycache_stats( const CCommand &args )
#endif
{
	Warning( "%d queries, %d misses (%d free) suc spec = %d wasted spec=%d\n",
			 s_nNumCacheQueries, s_nNumCacheMisses, (int)s_VictimList.size(),
			 s_SuccessfulSpeculatives, s_WastedSpeculativeUpdates );
}
