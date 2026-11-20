import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsObject class to JoltPhysicsObject. Included by the source that defines JoltPhysicsObject.

bool IPhysicsObject::IsStatic() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsStatic(); }
bool IPhysicsObject::IsAsleep() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsAsleep(); }
bool IPhysicsObject::IsTrigger() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsTrigger(); }
bool IPhysicsObject::IsFluid() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsFluid(); }
bool IPhysicsObject::IsHinged() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsHinged(); }
bool IPhysicsObject::IsCollisionEnabled() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsCollisionEnabled(); }
bool IPhysicsObject::IsGravityEnabled() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsGravityEnabled(); }
bool IPhysicsObject::IsDragEnabled() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsDragEnabled(); }
bool IPhysicsObject::IsMotionEnabled() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsMotionEnabled(); }
bool IPhysicsObject::IsMoveable() const
{ return static_cast<const JoltPhysicsObject*>(this)->IsMoveable(); }
bool IPhysicsObject::IsAttachedToConstraint( bool bExternalOnly ) const
{ return static_cast<const JoltPhysicsObject*>(this)->IsAttachedToConstraint( bExternalOnly ); }
void IPhysicsObject::EnableCollisions( bool enable )
{ static_cast<JoltPhysicsObject*>(this)->EnableCollisions( enable ); }
void IPhysicsObject::EnableGravity( bool enable )
{ static_cast<JoltPhysicsObject*>(this)->EnableGravity( enable ); }
void IPhysicsObject::EnableDrag( bool enable )
{ static_cast<JoltPhysicsObject*>(this)->EnableDrag( enable ); }
void IPhysicsObject::EnableMotion( bool enable )
{ static_cast<JoltPhysicsObject*>(this)->EnableMotion( enable ); }
void IPhysicsObject::SetGameData( void *pGameData )
{ static_cast<JoltPhysicsObject*>(this)->SetGameData( pGameData ); }
void * IPhysicsObject::GetGameData() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetGameData(); }
void IPhysicsObject::SetGameFlags( unsigned short userFlags )
{ static_cast<JoltPhysicsObject*>(this)->SetGameFlags( userFlags ); }
unsigned short IPhysicsObject::GetGameFlags() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetGameFlags(); }
void IPhysicsObject::SetGameIndex( unsigned short gameIndex )
{ static_cast<JoltPhysicsObject*>(this)->SetGameIndex( gameIndex ); }
unsigned short IPhysicsObject::GetGameIndex() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetGameIndex(); }
void IPhysicsObject::SetCallbackFlags( unsigned short callbackflags )
{ static_cast<JoltPhysicsObject*>(this)->SetCallbackFlags( callbackflags ); }
unsigned short IPhysicsObject::GetCallbackFlags() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetCallbackFlags(); }
void IPhysicsObject::Wake()
{ static_cast<JoltPhysicsObject*>(this)->Wake(); }
void IPhysicsObject::Sleep()
{ static_cast<JoltPhysicsObject*>(this)->Sleep(); }
void IPhysicsObject::RecheckCollisionFilter()
{ static_cast<JoltPhysicsObject*>(this)->RecheckCollisionFilter(); }
void IPhysicsObject::RecheckContactPoints( bool bSearchForNewContacts )
{ static_cast<JoltPhysicsObject*>(this)->RecheckContactPoints( bSearchForNewContacts ); }
void IPhysicsObject::SetMass( float mass )
{ static_cast<JoltPhysicsObject*>(this)->SetMass( mass ); }
float IPhysicsObject::GetMass() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetMass(); }
float IPhysicsObject::GetInvMass() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetInvMass(); }
Vector IPhysicsObject::GetInertia() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetInertia(); }
Vector IPhysicsObject::GetInvInertia() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetInvInertia(); }
void IPhysicsObject::SetInertia( const Vector &inertia )
{ static_cast<JoltPhysicsObject*>(this)->SetInertia( inertia ); }
void IPhysicsObject::SetDamping( const float *speed, const float *rot )
{ static_cast<JoltPhysicsObject*>(this)->SetDamping( speed, rot ); }
void IPhysicsObject::GetDamping( float *speed, float *rot ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetDamping( speed, rot ); }
void IPhysicsObject::SetDragCoefficient( float *pDrag, float *pAngularDrag )
{ static_cast<JoltPhysicsObject*>(this)->SetDragCoefficient( pDrag, pAngularDrag ); }
void IPhysicsObject::SetBuoyancyRatio( float ratio )
{ static_cast<JoltPhysicsObject*>(this)->SetBuoyancyRatio( ratio ); }
int IPhysicsObject::GetMaterialIndex() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetMaterialIndex(); }
void IPhysicsObject::SetMaterialIndex( int materialIndex )
{ static_cast<JoltPhysicsObject*>(this)->SetMaterialIndex( materialIndex ); }
unsigned int IPhysicsObject::GetContents() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetContents(); }
void IPhysicsObject::SetContents( unsigned int contents )
{ static_cast<JoltPhysicsObject*>(this)->SetContents( contents ); }
float IPhysicsObject::GetSphereRadius() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetSphereRadius(); }
void IPhysicsObject::SetSphereRadius( float radius )
{ static_cast<JoltPhysicsObject*>(this)->SetSphereRadius( radius ); }
float IPhysicsObject::GetEnergy() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetEnergy(); }
Vector IPhysicsObject::GetMassCenterLocalSpace() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetMassCenterLocalSpace(); }
void IPhysicsObject::SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport )
{ static_cast<JoltPhysicsObject*>(this)->SetPosition( worldPosition, angles, isTeleport ); }
void IPhysicsObject::SetPositionMatrix( const matrix3x4_t&matrix, bool isTeleport )
{ static_cast<JoltPhysicsObject*>(this)->SetPositionMatrix( matrix, isTeleport ); }
void IPhysicsObject::GetPosition( Vector *worldPosition, QAngle *angles ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetPosition( worldPosition, angles ); }
void IPhysicsObject::GetPositionMatrix( matrix3x4_t *positionMatrix ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetPositionMatrix( positionMatrix ); }
void IPhysicsObject::SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{ static_cast<JoltPhysicsObject*>(this)->SetVelocity( velocity, angularVelocity ); }
void IPhysicsObject::SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity )
{ static_cast<JoltPhysicsObject*>(this)->SetVelocityInstantaneous( velocity, angularVelocity ); }
void IPhysicsObject::GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetVelocity( velocity, angularVelocity ); }
void IPhysicsObject::AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{ static_cast<JoltPhysicsObject*>(this)->AddVelocity( velocity, angularVelocity ); }
void IPhysicsObject::GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetVelocityAtPoint( worldPosition, pVelocity ); }
void IPhysicsObject::GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{ static_cast<const JoltPhysicsObject*>(this)->GetImplicitVelocity( velocity, angularVelocity ); }
void IPhysicsObject::LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const
{ static_cast<const JoltPhysicsObject*>(this)->LocalToWorld( worldPosition, localPosition ); }
void IPhysicsObject::WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const
{ static_cast<const JoltPhysicsObject*>(this)->WorldToLocal( localPosition, worldPosition ); }
void IPhysicsObject::LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const
{ static_cast<const JoltPhysicsObject*>(this)->LocalToWorldVector( worldVector, localVector ); }
void IPhysicsObject::WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const
{ static_cast<const JoltPhysicsObject*>(this)->WorldToLocalVector( localVector, worldVector ); }
void IPhysicsObject::ApplyForceCenter( const Vector &forceVector )
{ static_cast<JoltPhysicsObject*>(this)->ApplyForceCenter( forceVector ); }
void IPhysicsObject::ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition )
{ static_cast<JoltPhysicsObject*>(this)->ApplyForceOffset( forceVector, worldPosition ); }
void IPhysicsObject::ApplyTorqueCenter( const AngularImpulse &torque )
{ static_cast<JoltPhysicsObject*>(this)->ApplyTorqueCenter( torque ); }
void IPhysicsObject::CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const
{ static_cast<const JoltPhysicsObject*>(this)->CalculateForceOffset( forceVector, worldPosition, centerForce, centerTorque ); }
void IPhysicsObject::CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const
{ static_cast<const JoltPhysicsObject*>(this)->CalculateVelocityOffset( forceVector, worldPosition, centerVelocity, centerAngularVelocity ); }
float IPhysicsObject::CalculateLinearDrag( const Vector &unitDirection ) const
{ return static_cast<const JoltPhysicsObject*>(this)->CalculateLinearDrag( unitDirection ); }
float IPhysicsObject::CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const
{ return static_cast<const JoltPhysicsObject*>(this)->CalculateAngularDrag( objectSpaceRotationAxis ); }
bool IPhysicsObject::GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const
{ return static_cast<const JoltPhysicsObject*>(this)->GetContactPoint( contactPoint, contactObject ); }
void IPhysicsObject::SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation )
{ static_cast<JoltPhysicsObject*>(this)->SetShadow( maxSpeed, maxAngularSpeed, allowPhysicsMovement, allowPhysicsRotation ); }
void IPhysicsObject::UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset )
{ static_cast<JoltPhysicsObject*>(this)->UpdateShadow( targetPosition, targetAngles, tempDisableGravity, timeOffset ); }
int IPhysicsObject::GetShadowPosition( Vector *position, QAngle *angles ) const
{ return static_cast<const JoltPhysicsObject*>(this)->GetShadowPosition( position, angles ); }
IPhysicsShadowController * IPhysicsObject::GetShadowController() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetShadowController(); }
void IPhysicsObject::RemoveShadowController()
{ static_cast<JoltPhysicsObject*>(this)->RemoveShadowController(); }
float IPhysicsObject::ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt )
{ return static_cast<JoltPhysicsObject*>(this)->ComputeShadowControl( params, secondsToArrival, dt ); }
const CPhysCollide * IPhysicsObject::GetCollide() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetCollide(); }
const char * IPhysicsObject::GetName() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetName(); }
void IPhysicsObject::BecomeTrigger()
{ static_cast<JoltPhysicsObject*>(this)->BecomeTrigger(); }
void IPhysicsObject::RemoveTrigger()
{ static_cast<JoltPhysicsObject*>(this)->RemoveTrigger(); }
void IPhysicsObject::BecomeHinged( int localAxis )
{ static_cast<JoltPhysicsObject*>(this)->BecomeHinged( localAxis ); }
void IPhysicsObject::RemoveHinged()
{ static_cast<JoltPhysicsObject*>(this)->RemoveHinged(); }
IPhysicsFrictionSnapshot * IPhysicsObject::CreateFrictionSnapshot()
{ return static_cast<JoltPhysicsObject*>(this)->CreateFrictionSnapshot(); }
void IPhysicsObject::DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot )
{ static_cast<JoltPhysicsObject*>(this)->DestroyFrictionSnapshot( pSnapshot ); }
void IPhysicsObject::OutputDebugInfo() const
{ static_cast<const JoltPhysicsObject*>(this)->OutputDebugInfo(); }
void IPhysicsObject::SetUseAlternateGravity( bool bSet )
{ static_cast<JoltPhysicsObject*>(this)->SetUseAlternateGravity( bSet ); }
void IPhysicsObject::SetCollisionHints( uint32 collisionHints )
{ static_cast<JoltPhysicsObject*>(this)->SetCollisionHints( collisionHints ); }
uint32 IPhysicsObject::GetCollisionHints() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetCollisionHints(); }
IPredictedPhysicsObject * IPhysicsObject::GetPredictedInterface() const
{ return static_cast<const JoltPhysicsObject*>(this)->GetPredictedInterface(); }
void IPhysicsObject::SyncWith( IPhysicsObject *pOther )
{ static_cast<JoltPhysicsObject*>(this)->SyncWith( pOther ); }
