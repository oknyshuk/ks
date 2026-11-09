#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IPhysicsObjectPairHash
{
public:
	void AddObjectPair( void *pObject0, void *pObject1 );
	void RemoveObjectPair( void *pObject0, void *pObject1 );
	bool IsObjectPairInHash( void *pObject0, void *pObject1 );
	void RemoveAllPairsForObject( void *pObject0 );
	bool IsObjectInHash( void *pObject0 );

	// Used to iterate over all pairs an object is part of
	int GetPairCountForObject( void *pObject0 );
	int GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList );
};
