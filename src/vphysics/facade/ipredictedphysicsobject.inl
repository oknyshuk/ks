// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// IPredictedPhysicsObject: JoltPhysicsObject does not implement it (GetPredictedInterface() returns
// nullptr), so these are never reached. Included by vjolt_object.cpp.

void IPredictedPhysicsObject::SetErrorDelta_Position( const Vector &vPosition )
{
}
void IPredictedPhysicsObject::SetErrorDelta_Velocity( const Vector &vVelocity )
{
}
