// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ICollisionQuery class to JoltCollisionQuery. Included by the source that defines JoltCollisionQuery.

int ICollisionQuery::ConvexCount()
{ return static_cast<JoltCollisionQuery*>(this)->ConvexCount(); }
int ICollisionQuery::TriangleCount( int convexIndex )
{ return static_cast<JoltCollisionQuery*>(this)->TriangleCount( convexIndex ); }
unsigned int ICollisionQuery::GetGameData( int convexIndex )
{ return static_cast<JoltCollisionQuery*>(this)->GetGameData( convexIndex ); }
void ICollisionQuery::GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts )
{ static_cast<JoltCollisionQuery*>(this)->GetTriangleVerts( convexIndex, triangleIndex, verts ); }
void ICollisionQuery::SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts )
{ static_cast<JoltCollisionQuery*>(this)->SetTriangleVerts( convexIndex, triangleIndex, verts ); }
int ICollisionQuery::GetTriangleMaterialIndex( int convexIndex, int triangleIndex )
{ return static_cast<JoltCollisionQuery*>(this)->GetTriangleMaterialIndex( convexIndex, triangleIndex ); }
void ICollisionQuery::SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits )
{ static_cast<JoltCollisionQuery*>(this)->SetTriangleMaterialIndex( convexIndex, triangleIndex, index7bits ); }
