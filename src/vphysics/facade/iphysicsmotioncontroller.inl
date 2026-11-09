// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsMotionController class to JoltPhysicsMotionController. Included by the source that defines JoltPhysicsMotionController.

void IPhysicsMotionController::SetEventHandler( IMotionEvent *handler )
{ static_cast<JoltPhysicsMotionController*>(this)->SetEventHandler( handler ); }
void IPhysicsMotionController::AttachObject( IPhysicsObject *pObject, bool checkIfAlreadyAttached )
{ static_cast<JoltPhysicsMotionController*>(this)->AttachObject( pObject, checkIfAlreadyAttached ); }
void IPhysicsMotionController::DetachObject( IPhysicsObject *pObject )
{ static_cast<JoltPhysicsMotionController*>(this)->DetachObject( pObject ); }
int IPhysicsMotionController::CountObjects()
{ return static_cast<JoltPhysicsMotionController*>(this)->CountObjects(); }
void IPhysicsMotionController::GetObjects( IPhysicsObject **pObjectList )
{ static_cast<JoltPhysicsMotionController*>(this)->GetObjects( pObjectList ); }
void IPhysicsMotionController::ClearObjects()
{ static_cast<JoltPhysicsMotionController*>(this)->ClearObjects(); }
void IPhysicsMotionController::WakeObjects()
{ static_cast<JoltPhysicsMotionController*>(this)->WakeObjects(); }
void IPhysicsMotionController::SetPriority( priority_t priority )
{ static_cast<JoltPhysicsMotionController*>(this)->SetPriority( priority ); }
