// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsCollisionSet class to JoltPhysicsCollisionSet. Included by the source that defines JoltPhysicsCollisionSet.

void IPhysicsCollisionSet::EnableCollisions( int index0, int index1 )
{ static_cast<JoltPhysicsCollisionSet*>(this)->EnableCollisions( index0, index1 ); }
void IPhysicsCollisionSet::DisableCollisions( int index0, int index1 )
{ static_cast<JoltPhysicsCollisionSet*>(this)->DisableCollisions( index0, index1 ); }
bool IPhysicsCollisionSet::ShouldCollide( int index0, int index1 )
{ return static_cast<JoltPhysicsCollisionSet*>(this)->ShouldCollide( index0, index1 ); }
