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
import std;
import ks.core.logging;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "bspfile.h"
#include "cmodel.h"
#include "vphysics/collision_set.h"
#include "vphysics/constraints.h"
#include "vphysics/performance.h"
#include "vphysics/vehicles.h"
#include "vjolt_interface.h"
#include "vjolt_util.h"

#include "vjolt_controller_motion.h"

//-------------------------------------------------------------------------------------------------

JoltPhysicsMotionController::JoltPhysicsMotionController( IMotionEvent *pHandler )
	: m_pMotionEvent( pHandler )
{
}

JoltPhysicsMotionController::~JoltPhysicsMotionController()
{
	for ( JoltPhysicsObject *pObject : m_pObjects )
		pObject->RemoveListener( this );
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsMotionController::SetEventHandler( IMotionEvent *pHandler )
{
	m_pMotionEvent = pHandler;
}

void JoltPhysicsMotionController::AttachObject( IPhysicsObject *pObject, bool bCheckIfAlreadyAttached )
{
	if ( !pObject || pObject->IsStatic() )
		return;

	JoltPhysicsObject *pPhysicsObject = static_cast< JoltPhysicsObject * >( pObject );
	if ( bCheckIfAlreadyAttached && std::ranges::contains( m_pObjects, pPhysicsObject ) )
		return;

	pPhysicsObject->AddListener( this );
	m_pObjects.push_back( pPhysicsObject );
}

void JoltPhysicsMotionController::DetachObject( IPhysicsObject *pObject )
{
	if ( !pObject )
		return;

	JoltPhysicsObject *pPhysicsObject = static_cast< JoltPhysicsObject * >( pObject );
	Erase( m_pObjects, pPhysicsObject );
	pPhysicsObject->RemoveListener( this );
}

//-------------------------------------------------------------------------------------------------

int JoltPhysicsMotionController::CountObjects( void )
{
	return int( m_pObjects.size() );
}

void JoltPhysicsMotionController::GetObjects( IPhysicsObject **pObjectList )
{
	for ( size_t i = 0; i < m_pObjects.size(); i++ )
		pObjectList[ i ] = m_pObjects[ i ];
}

void JoltPhysicsMotionController::ClearObjects( void )
{
	m_pObjects.clear();
}

void JoltPhysicsMotionController::WakeObjects( void )
{
	for ( JoltPhysicsObject *pObject : m_pObjects )
		pObject->Wake();
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsMotionController::SetPriority( priority_t priority )
{
	// Not relevant to us.
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsMotionController::OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject )
{
	JoltPhysicsObject *pPhysicsObject = static_cast< JoltPhysicsObject * >( pObject );
	Erase( m_pObjects, pPhysicsObject );
}

void JoltPhysicsMotionController::OnPreSimulate( float flDeltaTime )
{
	if ( !m_pMotionEvent )
		return;

	for ( JoltPhysicsObject *pObject : m_pObjects )
	{
		if ( !pObject->IsMoveable() )
			return;

		Vector vecLocalVelocity = vec3_origin;
		Vector vecAngularVelocity = vec3_origin;
		IMotionEvent::simresult_e simResult = m_pMotionEvent->Simulate( this, pObject, flDeltaTime, vecLocalVelocity, vecAngularVelocity );

		vecLocalVelocity *= flDeltaTime;
		vecAngularVelocity *= flDeltaTime;

		// Convert linear velocity to world space
		Vector vecWorldLinearVelocity = vec3_origin;
		pObject->LocalToWorldVector( &vecWorldLinearVelocity, vecLocalVelocity );

		switch ( simResult )
		{
			case IMotionEvent::SIM_NOTHING:
			{
				break;
			}

			case IMotionEvent::SIM_LOCAL_ACCELERATION:
			{
				pObject->AddVelocity( &vecWorldLinearVelocity, &vecAngularVelocity );
				break;
			}

			case IMotionEvent::SIM_LOCAL_FORCE:
			{
				pObject->ApplyForceCenter( vecWorldLinearVelocity );
				pObject->ApplyTorqueCenter( vecAngularVelocity );
				break;
			}

			case IMotionEvent::SIM_GLOBAL_ACCELERATION:
			{
				pObject->AddVelocity( &vecLocalVelocity, &vecAngularVelocity );
				break;
			}

			case IMotionEvent::SIM_GLOBAL_FORCE:
			{
				pObject->ApplyForceCenter( vecLocalVelocity );
				pObject->ApplyTorqueCenter( vecAngularVelocity );
				break;
			}

			default:
			{
				Log_Warning( LOG_VJolt, "Invalid motion event\n" );
				break;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
#include "facade/iphysicsmotioncontroller.inl"
