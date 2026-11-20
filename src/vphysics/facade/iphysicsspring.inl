import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsSpring class to JoltPhysicsSpring. Included by the source that defines JoltPhysicsSpring.

void IPhysicsSpring::GetEndpoints( Vector *worldPositionStart, Vector *worldPositionEnd )
{ static_cast<JoltPhysicsSpring*>(this)->GetEndpoints( worldPositionStart, worldPositionEnd ); }
void IPhysicsSpring::SetSpringConstant( float flSpringContant )
{ static_cast<JoltPhysicsSpring*>(this)->SetSpringConstant( flSpringContant ); }
void IPhysicsSpring::SetSpringDamping( float flSpringDamping )
{ static_cast<JoltPhysicsSpring*>(this)->SetSpringDamping( flSpringDamping ); }
void IPhysicsSpring::SetSpringLength( float flSpringLenght )
{ static_cast<JoltPhysicsSpring*>(this)->SetSpringLength( flSpringLenght ); }
IPhysicsObject * IPhysicsSpring::GetStartObject()
{ return static_cast<JoltPhysicsSpring*>(this)->GetStartObject(); }
IPhysicsObject * IPhysicsSpring::GetEndObject()
{ return static_cast<JoltPhysicsSpring*>(this)->GetEndObject(); }
