import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsCollisionData class to JoltPhysicsContactListener::JoltPhysicsCollisionData. Included by the source that defines JoltPhysicsContactListener::JoltPhysicsCollisionData.

void IPhysicsCollisionData::GetSurfaceNormal( Vector &out )
{ static_cast<JoltPhysicsContactListener::JoltPhysicsCollisionData*>(this)->GetSurfaceNormal( out ); }
void IPhysicsCollisionData::GetContactPoint( Vector &out )
{ static_cast<JoltPhysicsContactListener::JoltPhysicsCollisionData*>(this)->GetContactPoint( out ); }
void IPhysicsCollisionData::GetContactSpeed( Vector &out )
{ static_cast<JoltPhysicsContactListener::JoltPhysicsCollisionData*>(this)->GetContactSpeed( out ); }
