// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.core.platform;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.small_vector;
import ks.tier1.vector_ops;
#include "feagglomerator.h"
import std;

static constexpr CFeAgglomerator::LinkLessFunc_t s_LinkLess{};
static constexpr CFeAgglomerator::ClusterLessFunc_t s_ClusterLess{};
static constexpr CFeAgglomerator::ClusterSetIndexFunc_t s_ClusterSetIndex{};

CFeAgglomerator::CFeAgglomerator( uint nReserveNodes )
{
	m_Clusters.reserve( nReserveNodes * 2 );
	m_Clusters.resize( nReserveNodes );	// client needs to set all the nodes
/*
	for ( uint i = 0; i < nReserveNodes; ++i )
	{
		m_Clusters[ i ] = new CCluster;
	}
*/
}


CFeAgglomerator::~CFeAgglomerator() = default;



CFeAgglomerator::CCluster::CCluster( int nIndex, int nChildLeafs ) :m_nParent( -1 ), m_nIndex( nIndex ), m_nChildLeafs( nChildLeafs )
{
	m_nChild[ 0 ] = -1;
	m_nChild[ 1 ] = -1;
}


bool CFeAgglomerator::CCluster::HasLinks()const
{
	return !m_Links.empty();
}

float CFeAgglomerator::CCluster::GetBestCost()const
{
	if ( HasLinks() )
	{
		return m_Links.front().m_flCost;
	}
	else
	{
		return std::numeric_limits<float>::max();
	}
}

void CFeAgglomerator::CCluster::RemoveLink( CCluster *pOtherCluster )
{
	for ( int i = 0; i < std::ssize( m_Links ); ++i )
	{
		if ( m_Links[ i ].m_pOtherCluster == pOtherCluster )
		{
			ks::heap_erase( m_Links, i, s_LinkLess );
			return;
		}
	}
	contract_assert( !"Not found" );
}

const CFeAgglomerator::CLink *CFeAgglomerator::CCluster::FindLink( CCluster *pOtherCluster )
{
	for ( const CLink &link : m_Links )
	{
		if ( link.m_pOtherCluster == pOtherCluster )
		{
			return &link;
		}
	}
	return nullptr;
}

//
// The cost of a node of the tree is the probability of its collision with another bounding box, times number of points to test.
// The probably of collision is the probability of their minkowski sum containing the origin, which is proportional to the volume of the minkowski sum.
// It boils down to guessing the size of the other bounding box. Having no better heuristc, we can just say it'll probably be a box of average size 12x12x12 or something
// 
float CFeAgglomerator::CCluster::ComputeCost( const Vector &vSize, int nChildLeafs )
{
	return ( vSize.x + 12 ) * ( vSize.y + 12 ) * ( vSize.z + 12 ) * nChildLeafs;
}

void CFeAgglomerator::CCluster::AddLink( CCluster *pOtherCluster )
{

	CLink link;
	link.m_pOtherCluster = pOtherCluster;
	//
	// Note: GetSurfaceArea() is not a valid heuristic for cost here. E.g. 2 horizontal points will have 0 cost, which is clearly wrong
	// (m_Aabb + pOtherCluster->m_Aabb ).GetSurfaceArea()
	//
	link.m_flCost = ComputeCost( ( m_Aabb + pOtherCluster->m_Aabb ).GetSize(), m_nChildLeafs + pOtherCluster->m_nChildLeafs );
	ks::heap_push( m_Links, link, s_LinkLess );
}



void CFeAgglomerator::AddLink( CCluster* pCluster0, CCluster *pCluster1, ClustersPriorityQueue_t &queue )
{
	float flBestDist0 = pCluster0->GetBestCost();
	float flBestDist1 = pCluster1->GetBestCost();

	pCluster0->AddLink( pCluster1 );
	pCluster1->AddLink( pCluster0 );

	float flNewBestDist0 = pCluster0->GetBestCost();
	float flNewBestDist1 = pCluster1->GetBestCost();
	
	if ( flNewBestDist0 != flBestDist0 )
	{
		ks::heap_update( queue, pCluster0->m_nPriorityIndex, s_ClusterLess, s_ClusterSetIndex );
	}

	if ( flNewBestDist1 != flBestDist1 )
	{
		ks::heap_update( queue, pCluster1->m_nPriorityIndex, s_ClusterLess, s_ClusterSetIndex );
	}
}

// register a link between the nodes. 
// Call this to register all links between all nodes before building agglomerated clusters
void CFeAgglomerator::LinkNodes( int nNode0, int nNode1 )
{
	if ( nNode0 == nNode1 )
		return;
	CCluster* pCluster0 = m_Clusters[ nNode0 ].get();
	CCluster *pCluster1 = m_Clusters[ nNode1 ].get();
	if ( pCluster0->FindLink( pCluster1 ) )
	{
		contract_assert( pCluster1->FindLink( pCluster0 ) );
	}
	else
	{
		// link not duplicated, create new link
		pCluster0->AddLink( pCluster1 );
		pCluster1->AddLink( pCluster0 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
// Build agglomerated clusters; cannot call this function twice, because it will try to agglomerate clusters at all levels (nodes to root) the 2nd time
// 
void CFeAgglomerator::Build( bool bSingleRoot )
{
	ClustersPriorityQueue_t queue;
	for ( int i = 0; i < std::ssize( m_Clusters ); ++i )
	{
		ks::heap_push( queue, m_Clusters[ i ].get(), s_ClusterLess, s_ClusterSetIndex );
	}
	Process( queue );
	// create links of all-with-all
	// this will be ultra-slow if we have degenerate case (like 1000 small disconnected pieces of cloth)
	if ( bSingleRoot && std::ssize( queue ) > 1 )
	{
		for ( int i = std::ssize( queue ); i-- > 1; )
		{
			for ( int j = i; j-- > 0; )
			{
				queue[ i ]->AddLink( queue[ j ] );
				queue[ j ]->AddLink( queue[ i ] );
			}			
		}
		// the old queue order is destroyed; create a new queue (we could re-heapify the old queue
		ClustersPriorityQueue_t queue2;
		for ( int i = std::ssize( queue ); i-- > 0; )
		{
			ks::heap_push( queue2, queue[ i ], s_ClusterLess, s_ClusterSetIndex );
		}

		Process( queue2 );
		contract_assert( queue2.size() == 1 );
	}
}


void CFeAgglomerator::Validate( ClustersPriorityQueue_t *pQueue )
{
}


void CFeAgglomerator::Process( ClustersPriorityQueue_t &queue )
{
	while ( !queue.empty() && queue.front()->HasLinks() )
	{
		Validate( &queue );

		// remove the clusters we're merging from priority queue
		CCluster *pChild[ 2 ];
		pChild[ 0 ] = queue.front();
		pChild[ 1 ] = queue.front()->m_Links.front().m_pOtherCluster;

		// remove the children from the queue
		contract_assert( pChild[ 0 ]->m_nPriorityIndex == 0 );
		ks::heap_pop( queue, s_ClusterLess, s_ClusterSetIndex ); // removing pChild[0]
		ks::heap_erase( queue, pChild[ 1 ]->m_nPriorityIndex, s_ClusterLess, s_ClusterSetIndex );

		pChild[ 0 ]->m_nPriorityIndex = -1;
		pChild[ 1 ]->m_nPriorityIndex = -1;

		// make the new cluster, link and compute its distances to nearest clusters
		int nParentIndex = std::ssize( m_Clusters );
		CCluster *pParent = m_Clusters.emplace_back( std::make_unique< CCluster >( nParentIndex, pChild[ 0 ]->m_nChildLeafs + pChild[ 1 ]->m_nChildLeafs ) ).get();
		pParent->m_Aabb = pChild[ 0 ]->m_Aabb + pChild[ 1 ]->m_Aabb;
		pParent->m_nChild[ 0 ] = pChild[ 0 ]->m_nIndex;
		pParent->m_nChild[ 1 ] = pChild[ 1 ]->m_nIndex;


		pChild[ 0 ]->m_nParent = nParentIndex;
		pChild[ 1 ]->m_nParent = nParentIndex;


		ks::small_vector< CCluster*, 8 >	reAdd;
		std::vector< bool > skipAddLink( std::ssize( m_Clusters ) );
		// remove all links to the children, replace them with links to the parent
		{
			ClusterLinkQueue_t &links = pChild[ 0 ]->m_Links;
			for ( const CLink &link : links )
			{
				CCluster *pOther = link.m_pOtherCluster;
				contract_assert( pOther != pChild[ 0 ] );
				if ( pOther == pChild[ 1 ] )
					continue; // just skip the connection to the other child
				contract_assert( pOther->m_nPriorityIndex >= 0 );
				// we see this cluster for the first time
				float flOldDistance = pOther->GetBestCost();
				pOther->RemoveLink( pChild[ 0 ] );
				pOther->AddLink( pParent );
				pParent->AddLink( pOther );
				skipAddLink[ pOther->m_nIndex ] = true;
				float flNewDistance = pOther->GetBestCost();
				if ( flOldDistance != flNewDistance )
				{
					reAdd.push_back( pOther );
					ks::heap_erase( queue, pOther->m_nPriorityIndex, s_ClusterLess, s_ClusterSetIndex );
					pOther->m_nPriorityIndex = -1;
				}
			}
			ClusterLinkQueue_t().swap( links ); // the links don't matter any more, we can free the memory
		}
		{
			ClusterLinkQueue_t &links = pChild[ 1 ]->m_Links;
			for ( const CLink &link : links )
			{
				CCluster *pOther = link.m_pOtherCluster;
				contract_assert( pOther != pChild[ 1 ] );
				if ( pOther == pChild[ 0 ] )
					continue; // just skip the connection to the other child
				if ( pOther->m_nPriorityIndex >= 0 )
				{
					// we see this cluster for the first time
					float flOldDistance = pOther->GetBestCost();
					pOther->RemoveLink( pChild[ 1 ] );
					// if we saw this pOther already, and didn't remove it from the queue, then we marked it as added
					if ( !skipAddLink[ pOther->m_nIndex ] )
					{
						pOther->AddLink( pParent );
						pParent->AddLink( pOther );
					}
					float flNewDistance = pOther->GetBestCost();
					if ( flOldDistance != flNewDistance )
					{
						ks::heap_update( queue, pOther->m_nPriorityIndex, s_ClusterLess, s_ClusterSetIndex ); // no need to remove, this is the first and last edit of this other element
					}
				}
				else
				{
					// we've seen this cluster before within this loop, just remove the link; we already added the new link to the new parent
					pOther->RemoveLink( pChild[ 1 ] );
				}
			}
			ClusterLinkQueue_t().swap( links ); // the links don't matter any more, we can free the memory
		}

		for ( int nCluster = 0; nCluster < std::ssize( reAdd ); ++nCluster )
		{
			ks::heap_push( queue, reAdd[nCluster], s_ClusterLess, s_ClusterSetIndex );
		}
		ks::heap_push( queue, pParent, s_ClusterLess, s_ClusterSetIndex );
		
		Validate( &queue );
	}

	for ( CCluster *pCluster : queue )
	{
		contract_assert( !pCluster->HasLinks() );
	}
}
