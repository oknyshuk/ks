// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVirtualMeshEvent class to CVirtualTerrain. Included by cmodel_disp.cpp.

void IVirtualMeshEvent::GetVirtualMesh( void *userData, virtualmeshlist_t *pList )
{ static_cast<CVirtualTerrain*>(this)->GetVirtualMesh( userData, pList ); }
void IVirtualMeshEvent::GetWorldspaceBounds( void *userData, Vector *pMins, Vector *pMaxs )
{ static_cast<CVirtualTerrain*>(this)->GetWorldspaceBounds( userData, pMins, pMaxs ); }
void IVirtualMeshEvent::GetTrianglesInSphere( void *userData, const Vector &center, float radius, virtualmeshtrianglelist_t *pList )
{ static_cast<CVirtualTerrain*>(this)->GetTrianglesInSphere( userData, center, radius, pList ); }

