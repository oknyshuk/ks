// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsConstraintGroup class to JoltPhysicsConstraintGroup. Included by the source that defines JoltPhysicsConstraintGroup.

void IPhysicsConstraintGroup::Activate()
{ static_cast<JoltPhysicsConstraintGroup*>(this)->Activate(); }
bool IPhysicsConstraintGroup::IsInErrorState()
{ return static_cast<JoltPhysicsConstraintGroup*>(this)->IsInErrorState(); }
void IPhysicsConstraintGroup::ClearErrorState()
{ static_cast<JoltPhysicsConstraintGroup*>(this)->ClearErrorState(); }
void IPhysicsConstraintGroup::GetErrorParams( constraint_groupparams_t *pParams )
{ static_cast<JoltPhysicsConstraintGroup*>(this)->GetErrorParams( pParams ); }
void IPhysicsConstraintGroup::SetErrorParams( const constraint_groupparams_t &params )
{ static_cast<JoltPhysicsConstraintGroup*>(this)->SetErrorParams( params ); }
void IPhysicsConstraintGroup::SolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1 )
{ static_cast<JoltPhysicsConstraintGroup*>(this)->SolvePenetration( pObj0, pObj1 ); }
