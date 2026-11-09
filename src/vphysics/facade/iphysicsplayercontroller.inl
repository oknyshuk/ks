// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsPlayerController class to JoltPhysicsPlayerController. Included by the source that defines JoltPhysicsPlayerController.

void IPhysicsPlayerController::Update( const Vector &position, const Vector &velocity, float secondsToArrival, bool onground, IPhysicsObject *ground )
{ static_cast<JoltPhysicsPlayerController*>(this)->Update( position, velocity, secondsToArrival, onground, ground ); }
void IPhysicsPlayerController::SetEventHandler( IPhysicsPlayerControllerEvent *handler )
{ static_cast<JoltPhysicsPlayerController*>(this)->SetEventHandler( handler ); }
bool IPhysicsPlayerController::IsInContact()
{ return static_cast<JoltPhysicsPlayerController*>(this)->IsInContact(); }
void IPhysicsPlayerController::MaxSpeed( const Vector &maxVelocity )
{ static_cast<JoltPhysicsPlayerController*>(this)->MaxSpeed( maxVelocity ); }
void IPhysicsPlayerController::SetObject( IPhysicsObject *pObject )
{ static_cast<JoltPhysicsPlayerController*>(this)->SetObject( pObject ); }
int IPhysicsPlayerController::GetShadowPosition( Vector *position, QAngle *angles )
{ return static_cast<JoltPhysicsPlayerController*>(this)->GetShadowPosition( position, angles ); }
void IPhysicsPlayerController::StepUp( float height )
{ static_cast<JoltPhysicsPlayerController*>(this)->StepUp( height ); }
void IPhysicsPlayerController::Jump()
{ static_cast<JoltPhysicsPlayerController*>(this)->Jump(); }
void IPhysicsPlayerController::GetShadowVelocity( Vector *velocity )
{ static_cast<JoltPhysicsPlayerController*>(this)->GetShadowVelocity( velocity ); }
IPhysicsObject * IPhysicsPlayerController::GetObject()
{ return static_cast<JoltPhysicsPlayerController*>(this)->GetObject(); }
void IPhysicsPlayerController::GetLastImpulse( Vector *pOut )
{ static_cast<JoltPhysicsPlayerController*>(this)->GetLastImpulse( pOut ); }
void IPhysicsPlayerController::SetPushMassLimit( float maxPushMass )
{ static_cast<JoltPhysicsPlayerController*>(this)->SetPushMassLimit( maxPushMass ); }
void IPhysicsPlayerController::SetPushSpeedLimit( float maxPushSpeed )
{ static_cast<JoltPhysicsPlayerController*>(this)->SetPushSpeedLimit( maxPushSpeed ); }
float IPhysicsPlayerController::GetPushMassLimit()
{ return static_cast<JoltPhysicsPlayerController*>(this)->GetPushMassLimit(); }
float IPhysicsPlayerController::GetPushSpeedLimit()
{ return static_cast<JoltPhysicsPlayerController*>(this)->GetPushSpeedLimit(); }
bool IPhysicsPlayerController::WasFrozen()
{ return static_cast<JoltPhysicsPlayerController*>(this)->WasFrozen(); }
uint32 IPhysicsPlayerController::GetContactState( uint16 nGameFlags )
{ return static_cast<JoltPhysicsPlayerController*>(this)->GetContactState( nGameFlags ); }
