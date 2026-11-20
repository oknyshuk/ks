import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsFluidController class to JoltPhysicsFluidController. Included by the source that defines JoltPhysicsFluidController.

void IPhysicsFluidController::SetGameData( void *pGameData )
{ static_cast<JoltPhysicsFluidController*>(this)->SetGameData( pGameData ); }
void * IPhysicsFluidController::GetGameData() const
{ return static_cast<const JoltPhysicsFluidController*>(this)->GetGameData(); }
void IPhysicsFluidController::GetSurfacePlane( Vector *pNormal, float *pDist ) const
{ static_cast<const JoltPhysicsFluidController*>(this)->GetSurfacePlane( pNormal, pDist ); }
float IPhysicsFluidController::GetDensity() const
{ return static_cast<const JoltPhysicsFluidController*>(this)->GetDensity(); }
void IPhysicsFluidController::WakeAllSleepingObjects()
{ static_cast<JoltPhysicsFluidController*>(this)->WakeAllSleepingObjects(); }
int IPhysicsFluidController::GetContents() const
{ return static_cast<const JoltPhysicsFluidController*>(this)->GetContents(); }
