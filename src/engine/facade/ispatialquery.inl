// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISpatialQuery class to CEngineBSPTree. Included by gl_rsurf.cpp.

int ISpatialQuery::LeafCount() const
{ return static_cast<const CEngineBSPTree*>(this)->LeafCount(); }
bool ISpatialQuery::EnumerateLeavesAtPoint( const Vector& pt, ISpatialLeafEnumerator* pEnum, intp context )
{ return static_cast<CEngineBSPTree*>(this)->EnumerateLeavesAtPoint( pt, pEnum, context ); }
bool ISpatialQuery::EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, ISpatialLeafEnumerator* pEnum, intp context )
{ return static_cast<CEngineBSPTree*>(this)->EnumerateLeavesInBox( mins, maxs, pEnum, context ); }
bool ISpatialQuery::EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context )
{ return static_cast<CEngineBSPTree*>(this)->EnumerateLeavesInSphere( center, radius, pEnum, context ); }
bool ISpatialQuery::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context )
{ return static_cast<CEngineBSPTree*>(this)->EnumerateLeavesAlongRay( ray, pEnum, context ); }
bool ISpatialQuery::EnumerateLeavesInSphereWithFlagSet( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context, int nFlagsCheck )
{ return static_cast<CEngineBSPTree*>(this)->EnumerateLeavesInSphereWithFlagSet( center, radius, pEnum, context, nFlagsCheck ); }
int ISpatialQuery::ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax )
{ return static_cast<CEngineBSPTree*>(this)->ListLeavesInBox( mins, maxs, pList, listMax ); }
int ISpatialQuery::ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride, int nFlagsCheck )
{ return static_cast<CEngineBSPTree*>(this)->ListLeavesInSphereWithFlagSet( pLeafsInSphere, vecCenter, flRadius, nLeafCount, pLeafs, nLeafStride, nFlagsCheck ); }

