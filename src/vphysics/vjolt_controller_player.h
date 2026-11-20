// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include "vjolt_object.h"
#include "vjolt_environment.h"
#include "vphysics/player_controller.h"
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;

class JoltPhysicsPlayerController : public IPhysicsPlayerController
{
public:
	JoltPhysicsPlayerController( JoltPhysicsObject *pObject );
	~JoltPhysicsPlayerController();

	void Update( const Vector &position, const Vector &velocity, float secondsToArrival, bool onground, IPhysicsObject *ground );
	void SetEventHandler( IPhysicsPlayerControllerEvent *handler );
	bool IsInContact( void );
	void MaxSpeed( const Vector &maxVelocity );

	void SetObject( IPhysicsObject *pObject );

	int GetShadowPosition( Vector *position, QAngle *angles );
	void StepUp( float height );
	void Jump();
	void GetShadowVelocity( Vector *velocity );
	IPhysicsObject *GetObject();
	void GetLastImpulse( Vector *pOut );

	void SetPushMassLimit( float maxPushMass );
	void SetPushSpeedLimit( float maxPushSpeed );

	float GetPushMassLimit();
	float GetPushSpeedLimit();

	bool WasFrozen();

	uint32 GetContactState( uint16 nGameFlags ) ;

	// IJoltObjectListener
	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );
	// IJoltPhysicsController
	void OnPreSimulate( float flDeltaTime );

private:
	void SetObjectInternal( JoltPhysicsObject *pObject );
	void SetGround( JoltPhysicsObject *pObject );

private:
	JoltPhysicsObject *m_pObject = nullptr;
	IPhysicsPlayerControllerEvent *m_pHandler = nullptr;

	JoltPhysicsObject *m_pGround = nullptr;
	JPH::Vec3 m_groundPos = JPH::Vec3::sZero();

	JPH::Vec3 m_targetPosition = JPH::Vec3::sZero();			// Where we want to be
	JPH::Vec3 m_targetVelocity = JPH::Vec3::sZero();			// How we want to be
	float m_secondsToArrival = std::numeric_limits<float>::epsilon();						// When we want to be

	float m_maxSpeed = 0.0f;
	float m_maxDampSpeed = 0.0f;
	float m_maxAngular = 0.0f;
	float m_maxDampAngular = 0.0f;
	float m_teleportDistance = 0.0f;
	bool m_isPhysicallyControlled = false;		// If true we're a bone follower on an NPC or something...
	bool m_allowTranslation = false;			// Should we translate?
	bool m_allowRotation = false;				// Should we rotate?

	float m_flPushableMassLimit = 1e4f;
	float m_flPushableSpeedLimit = 1e4f;

	uint16 m_savedMaterialIndex = 0;
};
