// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsFrictionSnapshot class to JoltPhysicsFrictionSnapshot. Included by the source that defines JoltPhysicsFrictionSnapshot.

bool IPhysicsFrictionSnapshot::IsValid()
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->IsValid(); }
IPhysicsObject * IPhysicsFrictionSnapshot::GetObject( int index )
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetObject( index ); }
int IPhysicsFrictionSnapshot::GetMaterial( int index )
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetMaterial( index ); }
void IPhysicsFrictionSnapshot::GetContactPoint( Vector &out )
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetContactPoint( out ); }
void IPhysicsFrictionSnapshot::GetSurfaceNormal( Vector &out )
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetSurfaceNormal( out ); }
float IPhysicsFrictionSnapshot::GetNormalForce()
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetNormalForce(); }
float IPhysicsFrictionSnapshot::GetEnergyAbsorbed()
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetEnergyAbsorbed(); }
void IPhysicsFrictionSnapshot::RecomputeFriction()
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->RecomputeFriction(); }
void IPhysicsFrictionSnapshot::ClearFrictionForce()
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->ClearFrictionForce(); }
void IPhysicsFrictionSnapshot::MarkContactForDelete()
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->MarkContactForDelete(); }
void IPhysicsFrictionSnapshot::DeleteAllMarkedContacts( bool wakeObjects )
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->DeleteAllMarkedContacts( wakeObjects ); }
void IPhysicsFrictionSnapshot::NextFrictionData()
{ static_cast<JoltPhysicsFrictionSnapshot*>(this)->NextFrictionData(); }
float IPhysicsFrictionSnapshot::GetFrictionCoefficient()
{ return static_cast<JoltPhysicsFrictionSnapshot*>(this)->GetFrictionCoefficient(); }
