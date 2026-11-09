// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsConstraint class to JoltPhysicsConstraint. Included by the source that defines JoltPhysicsConstraint.

void IPhysicsConstraint::Activate()
{ static_cast<JoltPhysicsConstraint*>(this)->Activate(); }
void IPhysicsConstraint::Deactivate()
{ static_cast<JoltPhysicsConstraint*>(this)->Deactivate(); }
void IPhysicsConstraint::SetGameData( void *gameData )
{ static_cast<JoltPhysicsConstraint*>(this)->SetGameData( gameData ); }
void * IPhysicsConstraint::GetGameData() const
{ return static_cast<const JoltPhysicsConstraint*>(this)->GetGameData(); }
IPhysicsObject * IPhysicsConstraint::GetReferenceObject() const
{ return static_cast<const JoltPhysicsConstraint*>(this)->GetReferenceObject(); }
IPhysicsObject * IPhysicsConstraint::GetAttachedObject() const
{ return static_cast<const JoltPhysicsConstraint*>(this)->GetAttachedObject(); }
void IPhysicsConstraint::SetLinearMotor( float speed, float maxLinearImpulse )
{ static_cast<JoltPhysicsConstraint*>(this)->SetLinearMotor( speed, maxLinearImpulse ); }
void IPhysicsConstraint::SetAngularMotor( float rotSpeed, float maxAngularImpulse )
{ static_cast<JoltPhysicsConstraint*>(this)->SetAngularMotor( rotSpeed, maxAngularImpulse ); }
void IPhysicsConstraint::UpdateRagdollTransforms( const matrix3x4_t &constraintToReference, const matrix3x4_t &constraintToAttached )
{ static_cast<JoltPhysicsConstraint*>(this)->UpdateRagdollTransforms( constraintToReference, constraintToAttached ); }
bool IPhysicsConstraint::GetConstraintTransform( matrix3x4_t *pConstraintToReference, matrix3x4_t *pConstraintToAttached ) const
{ return static_cast<const JoltPhysicsConstraint*>(this)->GetConstraintTransform( pConstraintToReference, pConstraintToAttached ); }
bool IPhysicsConstraint::GetConstraintParams( constraint_breakableparams_t *pParams ) const
{ return static_cast<const JoltPhysicsConstraint*>(this)->GetConstraintParams( pParams ); }
void IPhysicsConstraint::OutputDebugInfo()
{ static_cast<JoltPhysicsConstraint*>(this)->OutputDebugInfo(); }
