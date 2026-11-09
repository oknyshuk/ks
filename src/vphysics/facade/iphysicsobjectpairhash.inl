// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsObjectPairHash class to JoltPhysicsObjectPairHash. Included by the source that defines JoltPhysicsObjectPairHash.

void IPhysicsObjectPairHash::AddObjectPair( void *pObject0, void *pObject1 )
{ static_cast<JoltPhysicsObjectPairHash*>(this)->AddObjectPair( pObject0, pObject1 ); }
void IPhysicsObjectPairHash::RemoveObjectPair( void *pObject0, void *pObject1 )
{ static_cast<JoltPhysicsObjectPairHash*>(this)->RemoveObjectPair( pObject0, pObject1 ); }
bool IPhysicsObjectPairHash::IsObjectPairInHash( void *pObject0, void *pObject1 )
{ return static_cast<JoltPhysicsObjectPairHash*>(this)->IsObjectPairInHash( pObject0, pObject1 ); }
void IPhysicsObjectPairHash::RemoveAllPairsForObject( void *pObject0 )
{ static_cast<JoltPhysicsObjectPairHash*>(this)->RemoveAllPairsForObject( pObject0 ); }
bool IPhysicsObjectPairHash::IsObjectInHash( void *pObject0 )
{ return static_cast<JoltPhysicsObjectPairHash*>(this)->IsObjectInHash( pObject0 ); }
int IPhysicsObjectPairHash::GetPairCountForObject( void *pObject0 )
{ return static_cast<JoltPhysicsObjectPairHash*>(this)->GetPairCountForObject( pObject0 ); }
int IPhysicsObjectPairHash::GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList )
{ return static_cast<JoltPhysicsObjectPairHash*>(this)->GetPairListForObject( pObject0, nMaxCount, ppObjectList ); }
