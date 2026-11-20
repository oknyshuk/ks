import ks.math.aabb;
import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IEngineTrace class: kind-based dispatch to CEngineTraceServer / CEngineTraceClient.
// Included by enginetrace.cpp.

int IEngineTrace::GetPointContents( const Vector &vecAbsPosition, int contentsMask, IHandleEntity** ppEntity )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetPointContents( vecAbsPosition, contentsMask, ppEntity );
	default: return static_cast<CEngineTraceClient*>(this)->GetPointContents( vecAbsPosition, contentsMask, ppEntity );
	}
}
int IEngineTrace::GetPointContents_WorldOnly( const Vector &vecAbsPosition, int contentsMask )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetPointContents_WorldOnly( vecAbsPosition, contentsMask );
	default: return static_cast<CEngineTraceClient*>(this)->GetPointContents_WorldOnly( vecAbsPosition, contentsMask );
	}
}
int IEngineTrace::GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetPointContents_Collideable( pCollide, vecAbsPosition );
	default: return static_cast<CEngineTraceClient*>(this)->GetPointContents_Collideable( pCollide, vecAbsPosition );
	}
}
void IEngineTrace::ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEnt, trace_t *pTrace )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->ClipRayToEntity( ray, fMask, pEnt, pTrace ); break;
	default: static_cast<CEngineTraceClient*>(this)->ClipRayToEntity( ray, fMask, pEnt, pTrace ); break;
	}
}
void IEngineTrace::ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pCollide, trace_t *pTrace )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->ClipRayToCollideable( ray, fMask, pCollide, pTrace ); break;
	default: static_cast<CEngineTraceClient*>(this)->ClipRayToCollideable( ray, fMask, pCollide, pTrace ); break;
	}
}
void IEngineTrace::TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->TraceRay( ray, fMask, pTraceFilter, pTrace ); break;
	default: static_cast<CEngineTraceClient*>(this)->TraceRay( ray, fMask, pTraceFilter, pTrace ); break;
	}
}
void IEngineTrace::SetupLeafAndEntityListRay( const Ray_t &ray, ITraceListData *pTraceData )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->SetupLeafAndEntityListRay( ray, pTraceData ); break;
	default: static_cast<CEngineTraceClient*>(this)->SetupLeafAndEntityListRay( ray, pTraceData ); break;
	}
}
void IEngineTrace::SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, ITraceListData *pTraceData )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->SetupLeafAndEntityListBox( vecBoxMin, vecBoxMax, pTraceData ); break;
	default: static_cast<CEngineTraceClient*>(this)->SetupLeafAndEntityListBox( vecBoxMin, vecBoxMax, pTraceData ); break;
	}
}
void IEngineTrace::TraceRayAgainstLeafAndEntityList( const Ray_t &ray, ITraceListData *pTraceData, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->TraceRayAgainstLeafAndEntityList( ray, pTraceData, fMask, pTraceFilter, pTrace ); break;
	default: static_cast<CEngineTraceClient*>(this)->TraceRayAgainstLeafAndEntityList( ray, pTraceData, fMask, pTraceFilter, pTrace ); break;
	}
}
void IEngineTrace::SweepCollideable( ICollideable *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, const QAngle &vecAngles, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->SweepCollideable( pCollide, vecAbsStart, vecAbsEnd, vecAngles, fMask, pTraceFilter, pTrace ); break;
	default: static_cast<CEngineTraceClient*>(this)->SweepCollideable( pCollide, vecAbsStart, vecAbsEnd, vecAngles, fMask, pTraceFilter, pTrace ); break;
	}
}
void IEngineTrace::EnumerateEntities( const Ray_t &ray, bool triggers, IEntityEnumerator *pEnumerator )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->EnumerateEntities( ray, triggers, pEnumerator ); break;
	default: static_cast<CEngineTraceClient*>(this)->EnumerateEntities( ray, triggers, pEnumerator ); break;
	}
}
void IEngineTrace::EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->EnumerateEntities( vecAbsMins, vecAbsMaxs, pEnumerator ); break;
	default: static_cast<CEngineTraceClient*>(this)->EnumerateEntities( vecAbsMins, vecAbsMaxs, pEnumerator ); break;
	}
}
ICollideable * IEngineTrace::GetCollideable( IHandleEntity *pEntity )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetCollideable( pEntity );
	default: return static_cast<CEngineTraceClient*>(this)->GetCollideable( pEntity );
	}
}
int IEngineTrace::GetStatByIndex( int index, bool bClear )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetStatByIndex( index, bClear );
	default: return static_cast<CEngineTraceClient*>(this)->GetStatByIndex( index, bClear );
	}
}
void IEngineTrace::GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CBrushQuery &BrushQuery, int iContentsMask, int cmodelIndex )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->GetBrushesInAABB( vMins, vMaxs, BrushQuery, iContentsMask, cmodelIndex ); break;
	default: static_cast<CEngineTraceClient*>(this)->GetBrushesInAABB( vMins, vMaxs, BrushQuery, iContentsMask, cmodelIndex ); break;
	}
}
CPhysCollide* IEngineTrace::GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetCollidableFromDisplacementsInAABB( vMins, vMaxs );
	default: return static_cast<CEngineTraceClient*>(this)->GetCollidableFromDisplacementsInAABB( vMins, vMaxs );
	}
}
int IEngineTrace::GetNumDisplacements()
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetNumDisplacements();
	default: return static_cast<CEngineTraceClient*>(this)->GetNumDisplacements();
	}
}
void IEngineTrace::GetDisplacementMesh( int nIndex, virtualmeshlist_t *pMeshTriList )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->GetDisplacementMesh( nIndex, pMeshTriList ); break;
	default: static_cast<CEngineTraceClient*>(this)->GetDisplacementMesh( nIndex, pMeshTriList ); break;
	}
}
int IEngineTrace::GetBrushInfo( int iBrush, int &ContentsOut, BrushSideInfo_t *pBrushSideInfoOut, int iBrushSideInfoArraySize )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetBrushInfo( iBrush, ContentsOut, pBrushSideInfoOut, iBrushSideInfoArraySize );
	default: return static_cast<CEngineTraceClient*>(this)->GetBrushInfo( iBrush, ContentsOut, pBrushSideInfoOut, iBrushSideInfoArraySize );
	}
}
bool IEngineTrace::PointOutsideWorld( const Vector &ptTest )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->PointOutsideWorld( ptTest );
	default: return static_cast<CEngineTraceClient*>(this)->PointOutsideWorld( ptTest );
	}
}
int IEngineTrace::GetLeafContainingPoint( const Vector &ptTest )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetLeafContainingPoint( ptTest );
	default: return static_cast<CEngineTraceClient*>(this)->GetLeafContainingPoint( ptTest );
	}
}
ITraceListData * IEngineTrace::AllocTraceListData()
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->AllocTraceListData();
	default: return static_cast<CEngineTraceClient*>(this)->AllocTraceListData();
	}
}
void IEngineTrace::FreeTraceListData( ITraceListData * a0 )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->FreeTraceListData( a0 ); break;
	default: static_cast<CEngineTraceClient*>(this)->FreeTraceListData( a0 ); break;
	}
}
int IEngineTrace::GetSetDebugTraceCounter( int value, DebugTraceCounterBehavior_t behavior )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetSetDebugTraceCounter( value, behavior );
	default: return static_cast<CEngineTraceClient*>(this)->GetSetDebugTraceCounter( value, behavior );
	}
}
int IEngineTrace::GetMeshesFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs, virtualmeshlist_t *pOutputMeshes, int iMaxOutputMeshes )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->GetMeshesFromDisplacementsInAABB( vMins, vMaxs, pOutputMeshes, iMaxOutputMeshes );
	default: return static_cast<CEngineTraceClient*>(this)->GetMeshesFromDisplacementsInAABB( vMins, vMaxs, pOutputMeshes, iMaxOutputMeshes );
	}
}
void IEngineTrace::GetBrushesInCollideable( ICollideable *pCollideable, CBrushQuery &BrushQuery )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->GetBrushesInCollideable( pCollideable, BrushQuery ); break;
	default: static_cast<CEngineTraceClient*>(this)->GetBrushesInCollideable( pCollideable, BrushQuery ); break;
	}
}
bool IEngineTrace::IsFullyOccluded( int nOcclusionKey, const AABB_t &aabb1, const AABB_t &aabb2, const Vector &vShadow )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CEngineTraceServer*>(this)->IsFullyOccluded( nOcclusionKey, aabb1, aabb2, vShadow );
	default: return static_cast<CEngineTraceClient*>(this)->IsFullyOccluded( nOcclusionKey, aabb1, aabb2, vShadow );
	}
}
void IEngineTrace::SuspendOcclusionTests()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->SuspendOcclusionTests(); break;
	default: static_cast<CEngineTraceClient*>(this)->SuspendOcclusionTests(); break;
	}
}
void IEngineTrace::ResumeOcclusionTests()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->ResumeOcclusionTests(); break;
	default: static_cast<CEngineTraceClient*>(this)->ResumeOcclusionTests(); break;
	}
}
void IEngineTrace::FlushOcclusionQueries()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CEngineTraceServer*>(this)->FlushOcclusionQueries(); break;
	default: static_cast<CEngineTraceClient*>(this)->FlushOcclusionQueries(); break;
	}
}

