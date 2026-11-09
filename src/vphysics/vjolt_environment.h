// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)

//=================================================================================================
//
// Interface to a physics scene
//
//=================================================================================================

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/DecoratedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Core/Color.h>
import std;
import ks.tier1;
#include "vjolt_interface.h"
#include "vjolt_object.h"
#include "vjolt_constraints.h"
#include "vjolt_listener_contact.h"
#include "vphysics/performance.h"

// StateRecorder implementation that saves to or restores from a fixed buffer
class VJoltStateRecorder final : public JPH::StateRecorder
{
public:
	struct read_only_t {};
	static constexpr read_only_t read_only{};

	VJoltStateRecorder( void* pBuffer, size_t bufferSize ) : m_Writer( pBuffer, bufferSize ) {}
	VJoltStateRecorder( const void* pBuffer, size_t bufferSize, read_only_t ) : m_Reader( pBuffer, bufferSize ) {}

	// StreamIn
	void ReadBytes( void* outData, size_t inNumBytes ) override
	{
		if ( m_Reader.ok() )
			m_Reader.read_bytes( outData, inNumBytes );
	}

	bool IsEOF() const override { return false; }

	// StreamOut
	void WriteBytes( const void* inData, size_t inNumBytes ) override
	{
		if ( m_Writer.ok() )
			m_Writer.write_bytes( inData, inNumBytes );
	}

	// Both
	bool IsFailed() const override { return false; }

private:
	ks::byte_writer m_Writer;
	ks::byte_reader m_Reader;
};

class JoltPhysicsEnvironment final : public IPhysicsEnvironment
{
public:
	JoltPhysicsEnvironment();
	~JoltPhysicsEnvironment();

	void SetDebugOverlay( CreateInterfaceFn debugOverlayFactory );
	IVPhysicsDebugOverlay* GetDebugOverlay( void );

	void SetGravity( const Vector& gravityVector );
	void GetGravity( Vector* pGravityVector ) const;

	void SetAirDensity( float density );
	float GetAirDensity() const;

	IPhysicsObject* CreatePolyObject( const CPhysCollide* pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams );
	IPhysicsObject* CreatePolyObjectStatic( const CPhysCollide* pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams );
	IPhysicsObject* CreateSphereObject( float radius, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams, bool isStatic );
	void DestroyObject( IPhysicsObject* );

	IPhysicsFluidController* CreateFluidController( IPhysicsObject* pFluidObject, fluidparams_t* pParams );
	void DestroyFluidController( IPhysicsFluidController* );

	IPhysicsSpring* CreateSpring( IPhysicsObject* pObjectStart, IPhysicsObject* pObjectEnd, springparams_t* pParams );
	void DestroySpring( IPhysicsSpring* );

	IPhysicsConstraint* CreateRagdollConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_ragdollparams_t& ragdoll );
	IPhysicsConstraint* CreateHingeConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_hingeparams_t& hinge );
	IPhysicsConstraint* CreateFixedConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_fixedparams_t& fixed );
	IPhysicsConstraint* CreateSlidingConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_slidingparams_t& sliding );
	IPhysicsConstraint* CreateBallsocketConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_ballsocketparams_t& ballsocket );
	IPhysicsConstraint* CreatePulleyConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_pulleyparams_t& pulley );
	IPhysicsConstraint* CreateLengthConstraint( IPhysicsObject* pReferenceObject, IPhysicsObject* pAttachedObject, IPhysicsConstraintGroup* pGroup, const constraint_lengthparams_t& length );

	void DestroyConstraint( IPhysicsConstraint* );

	IPhysicsConstraintGroup* CreateConstraintGroup( const constraint_groupparams_t& groupParams );
	void DestroyConstraintGroup( IPhysicsConstraintGroup* pGroup );

	IPhysicsShadowController* CreateShadowController( IPhysicsObject* pObject, bool allowTranslation, bool allowRotation );
	void DestroyShadowController( IPhysicsShadowController* );

	IPhysicsPlayerController* CreatePlayerController( IPhysicsObject* pObject );
	void DestroyPlayerController( IPhysicsPlayerController* );

	IPhysicsMotionController* CreateMotionController( IMotionEvent* pHandler );
	void DestroyMotionController( IPhysicsMotionController* pController );

	IPhysicsVehicleController* CreateVehicleController( IPhysicsObject* pVehicleBodyObject, const vehicleparams_t& params, unsigned int nVehicleType, IPhysicsGameTrace* pGameTrace );
	void DestroyVehicleController( IPhysicsVehicleController* );

	void SetCollisionSolver( IPhysicsCollisionSolver* pSolver );

	void Simulate( float deltaTime );
	bool IsInSimulation() const;

	float GetSimulationTimestep() const;
	void SetSimulationTimestep( float timestep );

	float GetSimulationTime() const;
	void ResetSimulationClock();
	float GetNextFrameTime() const;

	void SetCollisionEventHandler( IPhysicsCollisionEvent* pCollisionEvents );
	void SetObjectEventHandler( IPhysicsObjectEvent* pObjectEvents );
	void SetConstraintEventHandler( IPhysicsConstraintEvent* pConstraintEvents );

	void SetQuickDelete( bool bQuick );

	int GetActiveObjectCount() const;
	void GetActiveObjects( IPhysicsObject** pOutputObjectList ) const;
	const IPhysicsObject** GetObjectList( int* pOutputObjectCount ) const;
	bool TransferObject( IPhysicsObject* pObject, IPhysicsEnvironment* pDestinationEnvironment );

	void CleanupDeleteList();
	void EnableDeleteQueue( bool enable );

	bool IsCollisionModelUsed( CPhysCollide* pCollide ) const;

	void TraceRay( const Ray_t& ray, unsigned int fMask, IPhysicsTraceFilter* pTraceFilter, trace_t* pTrace );
	void SweepCollideable( const CPhysCollide* pCollide, const Vector& vecAbsStart, const Vector& vecAbsEnd,
		const QAngle& vecAngles, unsigned int fMask, IPhysicsTraceFilter* pTraceFilter, trace_t* pTrace );

	void GetPerformanceSettings( physics_performanceparams_t* pOutput ) const;
	void SetPerformanceSettings( const physics_performanceparams_t* pSettings );

	void ReadStats( physics_stats_t* pOutput );
	void ClearStats();

	unsigned int GetObjectSerializeSize( IPhysicsObject* pObject ) const;
	void SerializeObjectToBuffer( IPhysicsObject* pObject, unsigned char* pBuffer, unsigned int bufferSize );
	IPhysicsObject* UnserializeObjectFromBuffer( void* pGameData, unsigned char* pBuffer, unsigned int bufferSize, bool enableCollisions );

	void EnableConstraintNotify( bool bEnable );
	void DebugCheckContacts();

	void SetAlternateGravity( const Vector& gravityVector ) ;
	void GetAlternateGravity( Vector* pGravityVector ) const ;

	float GetDeltaFrameTime( int maxTicks ) const ;
	void ForceObjectsToSleep( IPhysicsObject** pList, int listCount ) ;

	void SetPredicted( bool bPredicted ) ;
	bool IsPredicted() ;
	void SetPredictionCommandNum( int iCommandNum ) ;
	int GetPredictionCommandNum() ;
	void DoneReferencingPreviousCommands( int iCommandNum ) ;
	void RestorePredictedSimulation() ;

	void DestroyCollideOnDeadObjectFlush( CPhysCollide* ) ;

public:
	JPH::PhysicsSystem* GetPhysicsSystem() { return &m_PhysicsSystem; }

	void ObjectTransferHandOver( JoltPhysicsObject* pObject );

	JoltPhysicsContactListener* GetContactListener() { return &m_ContactListener; }

	IPhysicsConstraintEvent* GetConstraintEvents() { return m_pConstraintListener; }

	void NotifyConstraintDisabled( JoltPhysicsConstraint* pConstraint );

	void AddDirtyStaticBody( const JPH::BodyID &id );
	void RemoveDirtyStaticBody( const JPH::BodyID &id );

private:

	void RemoveBodyAndDeleteObject( JoltPhysicsObject* pObject );
	void DeleteDeadObjects();

	void HandleDebugDumpingEnvironment( void* pReturnAddress );

	bool m_bSimulating = false;
	bool m_bEnableDeleteQueue = false;
	bool m_bWakeObjectsOnConstraintDeletion = false;
	bool m_bOptimizedBroadPhase = false;
	bool m_bUseLinearCast = true;
	float m_flStepTime = 1.0f / 60.0f;
	float m_flAirDensity = 2.0f;

	// For GetObjectList
	mutable JPH::BodyIDVector m_CachedBodies;
	mutable std::vector< const IPhysicsObject * > m_CachedObjects;

	// For GetActiveObjectCount and GetActiveObjects
	mutable JPH::BodyIDVector m_CachedActiveBodies;

	JPH::PhysicsSystem m_PhysicsSystem;

	// A vector of objects that were awake, and changed their
	// motion type from Dynamic -> Static, so that they can be
	// retrieved in GetActiveObjects, and have their visuals updated.
	// If we don't do this, objects that get moved, woken, and their
	// movement type changed to static will not get their transforms
	// updated on the game side.
	mutable JPH::BodyIDVector m_DirtyStaticBodies;

	std::vector< JoltPhysicsObject * > m_pDeadObjects;
	std::vector< JoltPhysicsConstraint * > m_pDeadConstraints;
	std::vector< CPhysCollide * > m_pDeadObjectCollides;

	std::vector< JoltPhysicsController > m_pPhysicsControllers;

	// The physics system that simulates the world
	// The debug overlay to render with (if it was ever passed to us)
	IVPhysicsDebugOverlay *m_pDebugOverlay = nullptr;

	JoltPhysicsContactListener m_ContactListener;
	IPhysicsConstraintEvent *m_pConstraintListener = nullptr;

	bool m_EnableConstraintNotify = false;

	mutable bool m_bActiveObjectCountFirst = true;

	physics_performanceparams_t m_PerformanceParams;
};
