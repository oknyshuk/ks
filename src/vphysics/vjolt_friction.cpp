// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
import std;
import ks.core;
#include "vphysics_interface.h"
#include "vphysics/friction.h"

#include "vjolt_friction.h"

// Josh:
// Friction snapshots are used for the following:
//  - Making objects on NPCs phase through them after some time
//  - Something something physics damage, but that seems to work anyway... :think:
//  - Game code callback for freezing objects if too many objects (IVP performance backdoor) -- not relevant for us
//  - Disabling last portal stuff for objects after teleporting, but our trigger callbacks are robust so this game code hack doesn't seem to matter *touch wood*
//  - Something to do with changing paint powers
//  - Train physics blockers
// All in all, it doesn't seem to be *that* important, most things still work
// without it implemented.
// Right now, we do not have an efficient way to implement this with Jolt.
// Hence it is just stubby.

//-------------------------------------------------------------------------------------------------

bool JoltPhysicsFrictionSnapshot::IsValid()
{
	return false;
}

//-------------------------------------------------------------------------------------------------

IPhysicsObject *JoltPhysicsFrictionSnapshot::GetObject( int index )
{
	return nullptr;
}

int JoltPhysicsFrictionSnapshot::GetMaterial( int index )
{
	return 0;
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsFrictionSnapshot::GetContactPoint( Vector &out )
{
	out.Zero();
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsFrictionSnapshot::GetSurfaceNormal( Vector &out )
{
	out.Zero();
}

float JoltPhysicsFrictionSnapshot::GetNormalForce()
{
	return 0.0f;
}

float JoltPhysicsFrictionSnapshot::GetEnergyAbsorbed()
{
	return 0.0f;
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsFrictionSnapshot::RecomputeFriction()
{
	
}

void JoltPhysicsFrictionSnapshot::ClearFrictionForce()
{

}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsFrictionSnapshot::MarkContactForDelete()
{

}

void JoltPhysicsFrictionSnapshot::DeleteAllMarkedContacts( bool wakeObjects )
{

}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsFrictionSnapshot::NextFrictionData()
{

}

float JoltPhysicsFrictionSnapshot::GetFrictionCoefficient()
{
	return 0.0f;
}
