
#include "cbase.h"

#include "vjolt_objectpairhash.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------------------------------------------------------------

const JoltPhysicsObjectPairHash::Neighbours *JoltPhysicsObjectPairHash::Find( void *pObject ) const
{
	const auto it = m_Pairs.find( pObject );
	return it != m_Pairs.end() ? &it->second : nullptr;
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsObjectPairHash::AddObjectPair( void *pObject0, void *pObject1 )
{
	m_Pairs[ pObject0 ].insert( pObject1 );
	m_Pairs[ pObject1 ].insert( pObject0 );
}

void JoltPhysicsObjectPairHash::RemoveObjectPair( void *pObject0, void *pObject1 )
{
	// Drop objects that lose their last pair, so IsObjectInHash stays a plain lookup.
	for ( auto [ pFrom, pTo ] : { std::pair{ pObject0, pObject1 }, std::pair{ pObject1, pObject0 } } )
	{
		const auto it = m_Pairs.find( pFrom );
		if ( it != m_Pairs.end() && it->second.erase( pTo ) && it->second.empty() )
			m_Pairs.erase( it );
	}
}

bool JoltPhysicsObjectPairHash::IsObjectPairInHash( void *pObject0, void *pObject1 )
{
	const Neighbours *pNeighbours = Find( pObject0 );
	return pNeighbours && pNeighbours->contains( pObject1 );
}

void JoltPhysicsObjectPairHash::RemoveAllPairsForObject( void *pObject0 )
{
	const auto it = m_Pairs.find( pObject0 );
	if ( it == m_Pairs.end() )
		return;

	// Take the neighbours out first: unlinking the back references mutates the map, and
	// would invalidate this entry underneath us if the object was ever paired with itself.
	const Neighbours neighbours = std::move( it->second );
	m_Pairs.erase( it );

	for ( void *pOther : neighbours )
		RemoveObjectPair( pObject0, pOther );
}

bool JoltPhysicsObjectPairHash::IsObjectInHash( void *pObject0 )
{
	return Find( pObject0 ) != nullptr;
}

//-------------------------------------------------------------------------------------------------

int JoltPhysicsObjectPairHash::GetPairCountForObject( void *pObject0 )
{
	const Neighbours *pNeighbours = Find( pObject0 );
	return pNeighbours ? int( pNeighbours->size() ) : 0;
}

int JoltPhysicsObjectPairHash::GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList )
{
	const Neighbours *pNeighbours = Find( pObject0 );
	if ( !pNeighbours )
		return 0;

	const int nCount = Min( nMaxCount, int( pNeighbours->size() ) );
	std::copy_n( pNeighbours->begin(), nCount, ppObjectList );
	return nCount;
}
