// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsVehicleController class to JoltPhysicsVehicleController. Included by the source that defines JoltPhysicsVehicleController.

void IPhysicsVehicleController::Update( float dt, vehicle_controlparams_t &controls )
{ static_cast<JoltPhysicsVehicleController*>(this)->Update( dt, controls ); }
const vehicle_operatingparams_t & IPhysicsVehicleController::GetOperatingParams()
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetOperatingParams(); }
const vehicleparams_t & IPhysicsVehicleController::GetVehicleParams()
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetVehicleParams(); }
vehicleparams_t & IPhysicsVehicleController::GetVehicleParamsForChange()
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetVehicleParamsForChange(); }
float IPhysicsVehicleController::UpdateBooster( float dt )
{ return static_cast<JoltPhysicsVehicleController*>(this)->UpdateBooster( dt ); }
int IPhysicsVehicleController::GetWheelCount()
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetWheelCount(); }
IPhysicsObject * IPhysicsVehicleController::GetWheel( int index )
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetWheel( index ); }
bool IPhysicsVehicleController::GetWheelContactPoint( int index, Vector *pContactPoint, int *pSurfaceProps )
{ return static_cast<JoltPhysicsVehicleController*>(this)->GetWheelContactPoint( index, pContactPoint, pSurfaceProps ); }
void IPhysicsVehicleController::SetSpringLength( int wheelIndex, float length )
{ static_cast<JoltPhysicsVehicleController*>(this)->SetSpringLength( wheelIndex, length ); }
void IPhysicsVehicleController::SetWheelFriction( int wheelIndex, float friction )
{ static_cast<JoltPhysicsVehicleController*>(this)->SetWheelFriction( wheelIndex, friction ); }
void IPhysicsVehicleController::OnVehicleEnter()
{ static_cast<JoltPhysicsVehicleController*>(this)->OnVehicleEnter(); }
void IPhysicsVehicleController::OnVehicleExit()
{ static_cast<JoltPhysicsVehicleController*>(this)->OnVehicleExit(); }
void IPhysicsVehicleController::SetEngineDisabled( bool bDisable )
{ static_cast<JoltPhysicsVehicleController*>(this)->SetEngineDisabled( bDisable ); }
bool IPhysicsVehicleController::IsEngineDisabled()
{ return static_cast<JoltPhysicsVehicleController*>(this)->IsEngineDisabled(); }
void IPhysicsVehicleController::GetCarSystemDebugData( vehicle_debugcarsystem_t &debugCarSystem )
{ static_cast<JoltPhysicsVehicleController*>(this)->GetCarSystemDebugData( debugCarSystem ); }
void IPhysicsVehicleController::VehicleDataReload()
{ static_cast<JoltPhysicsVehicleController*>(this)->VehicleDataReload(); }
