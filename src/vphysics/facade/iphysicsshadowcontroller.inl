import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsShadowController class to JoltPhysicsShadowController. Included by the source that defines JoltPhysicsShadowController.

void IPhysicsShadowController::Update( const Vector &position, const QAngle &angles, float timeOffset )
{ static_cast<JoltPhysicsShadowController*>(this)->Update( position, angles, timeOffset ); }
void IPhysicsShadowController::MaxSpeed( float maxSpeed, float maxAngularSpeed )
{ static_cast<JoltPhysicsShadowController*>(this)->MaxSpeed( maxSpeed, maxAngularSpeed ); }
void IPhysicsShadowController::StepUp( float height )
{ static_cast<JoltPhysicsShadowController*>(this)->StepUp( height ); }
void IPhysicsShadowController::SetTeleportDistance( float teleportDistance )
{ static_cast<JoltPhysicsShadowController*>(this)->SetTeleportDistance( teleportDistance ); }
bool IPhysicsShadowController::AllowsTranslation()
{ return static_cast<JoltPhysicsShadowController*>(this)->AllowsTranslation(); }
bool IPhysicsShadowController::AllowsRotation()
{ return static_cast<JoltPhysicsShadowController*>(this)->AllowsRotation(); }
void IPhysicsShadowController::SetPhysicallyControlled( bool isPhysicallyControlled )
{ static_cast<JoltPhysicsShadowController*>(this)->SetPhysicallyControlled( isPhysicallyControlled ); }
bool IPhysicsShadowController::IsPhysicallyControlled()
{ return static_cast<JoltPhysicsShadowController*>(this)->IsPhysicallyControlled(); }
void IPhysicsShadowController::GetLastImpulse( Vector *pOut )
{ static_cast<JoltPhysicsShadowController*>(this)->GetLastImpulse( pOut ); }
void IPhysicsShadowController::UseShadowMaterial( bool bUseShadowMaterial )
{ static_cast<JoltPhysicsShadowController*>(this)->UseShadowMaterial( bUseShadowMaterial ); }
void IPhysicsShadowController::ObjectMaterialChanged( int materialIndex )
{ static_cast<JoltPhysicsShadowController*>(this)->ObjectMaterialChanged( materialIndex ); }
float IPhysicsShadowController::GetTargetPosition( Vector *pPositionOut, QAngle *pAnglesOut )
{ return static_cast<JoltPhysicsShadowController*>(this)->GetTargetPosition( pPositionOut, pAnglesOut ); }
float IPhysicsShadowController::GetTeleportDistance()
{ return static_cast<JoltPhysicsShadowController*>(this)->GetTeleportDistance(); }
void IPhysicsShadowController::GetMaxSpeed( float *pMaxSpeedOut, float *pMaxAngularSpeedOut )
{ static_cast<JoltPhysicsShadowController*>(this)->GetMaxSpeed( pMaxSpeedOut, pMaxAngularSpeedOut ); }
