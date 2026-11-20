import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISpatialPartition class to CSpatialPartition. Included by spatialpartition.cpp.

SpatialPartitionHandle_t ISpatialPartition::CreateHandle( IHandleEntity *pHandleEntity )
{ return static_cast<CSpatialPartition*>(this)->CreateHandle( pHandleEntity ); }
SpatialPartitionHandle_t ISpatialPartition::CreateHandle( IHandleEntity *pHandleEntity, SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs )
{ return static_cast<CSpatialPartition*>(this)->CreateHandle( pHandleEntity, listMask, mins, maxs ); }
void ISpatialPartition::DestroyHandle( SpatialPartitionHandle_t handle )
{ static_cast<CSpatialPartition*>(this)->DestroyHandle( handle ); }
void ISpatialPartition::Insert( SpatialPartitionListMask_t listMask, SpatialPartitionHandle_t handle )
{ static_cast<CSpatialPartition*>(this)->Insert( listMask, handle ); }
void ISpatialPartition::Remove( SpatialPartitionListMask_t listMask, SpatialPartitionHandle_t handle )
{ static_cast<CSpatialPartition*>(this)->Remove( listMask, handle ); }
void ISpatialPartition::RemoveAndInsert( SpatialPartitionListMask_t removeMask, SpatialPartitionListMask_t insertMask, SpatialPartitionHandle_t handle )
{ static_cast<CSpatialPartition*>(this)->RemoveAndInsert( removeMask, insertMask, handle ); }
void ISpatialPartition::Remove( SpatialPartitionHandle_t handle )
{ static_cast<CSpatialPartition*>(this)->Remove( handle ); }
void ISpatialPartition::ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs )
{ static_cast<CSpatialPartition*>(this)->ElementMoved( handle, mins, maxs ); }
SpatialTempHandle_t ISpatialPartition::HideElement( SpatialPartitionHandle_t handle )
{ return static_cast<CSpatialPartition*>(this)->HideElement( handle ); }
void ISpatialPartition::UnhideElement( SpatialPartitionHandle_t handle, SpatialTempHandle_t tempHandle )
{ static_cast<CSpatialPartition*>(this)->UnhideElement( handle, tempHandle ); }
void ISpatialPartition::InstallQueryCallback( IPartitionQueryCallback *pCallback )
{ static_cast<CSpatialPartition*>(this)->InstallQueryCallback( pCallback ); }
void ISpatialPartition::RemoveQueryCallback( IPartitionQueryCallback *pCallback )
{ static_cast<CSpatialPartition*>(this)->RemoveQueryCallback( pCallback ); }
void ISpatialPartition::EnumerateElementsInBox( SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs, bool coarseTest, IPartitionEnumerator* pIterator )
{ static_cast<CSpatialPartition*>(this)->EnumerateElementsInBox( listMask, mins, maxs, coarseTest, pIterator ); }
void ISpatialPartition::EnumerateElementsInSphere( SpatialPartitionListMask_t listMask, const Vector& origin, float radius, bool coarseTest, IPartitionEnumerator* pIterator )
{ static_cast<CSpatialPartition*>(this)->EnumerateElementsInSphere( listMask, origin, radius, coarseTest, pIterator ); }
void ISpatialPartition::EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, const Ray_t& ray, bool coarseTest, IPartitionEnumerator* pIterator )
{ static_cast<CSpatialPartition*>(this)->EnumerateElementsAlongRay( listMask, ray, coarseTest, pIterator ); }
void ISpatialPartition::EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, const Vector& pt, bool coarseTest, IPartitionEnumerator* pIterator )
{ static_cast<CSpatialPartition*>(this)->EnumerateElementsAtPoint( listMask, pt, coarseTest, pIterator ); }
void ISpatialPartition::SuppressLists( SpatialPartitionListMask_t nListMask, bool bSuppress )
{ static_cast<CSpatialPartition*>(this)->SuppressLists( nListMask, bSuppress ); }
SpatialPartitionListMask_t ISpatialPartition::GetSuppressedLists()
{ return static_cast<CSpatialPartition*>(this)->GetSuppressedLists(); }
void ISpatialPartition::RenderAllObjectsInTree( float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderAllObjectsInTree( flTime ); }
void ISpatialPartition::RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderObjectsInPlayerLeafs( vecPlayerMin, vecPlayerMax, flTime ); }
void ISpatialPartition::RenderLeafsForRayTraceStart( float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForRayTraceStart( flTime ); }
void ISpatialPartition::RenderLeafsForRayTraceEnd( void )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForRayTraceEnd(); }
void ISpatialPartition::RenderLeafsForHullTraceStart( float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForHullTraceStart( flTime ); }
void ISpatialPartition::RenderLeafsForHullTraceEnd( void )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForHullTraceEnd(); }
void ISpatialPartition::RenderLeafsForBoxStart( float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForBoxStart( flTime ); }
void ISpatialPartition::RenderLeafsForBoxEnd( void )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForBoxEnd(); }
void ISpatialPartition::RenderLeafsForSphereStart( float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForSphereStart( flTime ); }
void ISpatialPartition::RenderLeafsForSphereEnd( void )
{ static_cast<CSpatialPartition*>(this)->RenderLeafsForSphereEnd(); }
void ISpatialPartition::RenderObjectsInBox( const Vector &vecMin, const Vector &vecMax, float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderObjectsInBox( vecMin, vecMax, flTime ); }
void ISpatialPartition::RenderObjectsInSphere( const Vector &vecCenter, float flRadius, float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderObjectsInSphere( vecCenter, flRadius, flTime ); }
void ISpatialPartition::RenderObjectsAlongRay( const Ray_t& ray, float flTime )
{ static_cast<CSpatialPartition*>(this)->RenderObjectsAlongRay( ray, flTime ); }
void ISpatialPartition::ReportStats( const char *pFileName )
{ static_cast<CSpatialPartition*>(this)->ReportStats( pFileName ); }

void ISpatialPartitionInternal::Init( const Vector& worldmin, const Vector& worldmax )
{ static_cast<CSpatialPartition*>(this)->Init( worldmin, worldmax ); }
void ISpatialPartitionInternal::DrawDebugOverlays()
{ static_cast<CSpatialPartition*>(this)->DrawDebugOverlays(); }

