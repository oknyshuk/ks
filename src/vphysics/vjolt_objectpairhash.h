
#pragma once

#include <unordered_map>
#include <unordered_set>

// An undirected graph over opaque object pointers, stored as an adjacency map. VPhysics
// calls this a "hash" because IVP hand-rolled its buckets; std::unordered_map already is
// one, so a neighbour set per object answers every query in the interface directly.
class JoltPhysicsObjectPairHash final : public IPhysicsObjectPairHash
{
public:
	void AddObjectPair( void *pObject0, void *pObject1 ) override;
	void RemoveObjectPair( void *pObject0, void *pObject1 ) override;
	bool IsObjectPairInHash( void *pObject0, void *pObject1 ) override;
	void RemoveAllPairsForObject( void *pObject0 ) override;
	bool IsObjectInHash( void *pObject0 ) override;

	int GetPairCountForObject( void *pObject0 ) override;
	int GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList ) override;

private:
	using Neighbours = std::unordered_set< void * >;

	// nullptr when the object has no pairs, so the lookups below share one code path.
	const Neighbours *Find( void *pObject ) const;

	std::unordered_map< void *, Neighbours > m_Pairs;
};
