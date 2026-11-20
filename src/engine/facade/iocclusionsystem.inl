import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vmatrix;
import ks.math.vplane;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IOcclusionSystem class to COcclusionSystem. Included by OcclusionSystem.cpp.

void IOcclusionSystem::ActivateOccluder( int nOccluderIndex, bool bActive )
{ static_cast<COcclusionSystem*>(this)->ActivateOccluder( nOccluderIndex, bActive ); }
void IOcclusionSystem::SetView( const Vector &vecCameraPos, float flFOV, const VMatrix &worldToCamera, const VMatrix &cameraToProjection, const VPlane &nearClipPlane )
{ static_cast<COcclusionSystem*>(this)->SetView( vecCameraPos, flFOV, worldToCamera, cameraToProjection, nearClipPlane ); }
int IOcclusionSystem::GetViewId() const
{ return static_cast<const COcclusionSystem*>(this)->GetViewId(); }
bool IOcclusionSystem::IsOccluded( int occlusionViewId, const Vector &vecAbsMins, const Vector &vecAbsMaxs )
{ return static_cast<COcclusionSystem*>(this)->IsOccluded( occlusionViewId, vecAbsMins, vecAbsMaxs ); }
void IOcclusionSystem::SetOcclusionParameters( float flMaxOccludeeArea, float flMinOccluderArea )
{ static_cast<COcclusionSystem*>(this)->SetOcclusionParameters( flMaxOccludeeArea, flMinOccluderArea ); }
float IOcclusionSystem::MinOccluderArea() const
{ return static_cast<const COcclusionSystem*>(this)->MinOccluderArea(); }
void IOcclusionSystem::DrawDebugOverlays()
{ static_cast<COcclusionSystem*>(this)->DrawDebugOverlays(); }

