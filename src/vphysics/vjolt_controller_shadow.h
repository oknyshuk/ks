// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include "vjolt_object.h"
#include "vjolt_environment.h"
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;

class JoltPhysicsShadowController final : public IPhysicsShadowController
{
public:
	JoltPhysicsShadowController( JoltPhysicsObject *pObject, bool allowTranslation, bool allowRotation );
	~JoltPhysicsShadowController();

	void Update( const Vector &position, const QAngle &angles, float timeOffset );
	void MaxSpeed( float maxSpeed, float maxAngularSpeed );
	void StepUp( float height );
	
	void SetTeleportDistance( float teleportDistance );
	bool AllowsTranslation();
	bool AllowsRotation();

	void SetPhysicallyControlled( bool isPhysicallyControlled );
	bool IsPhysicallyControlled();
	void GetLastImpulse( Vector *pOut );
	void UseShadowMaterial( bool bUseShadowMaterial );
	void ObjectMaterialChanged( int materialIndex );

	float GetTargetPosition( Vector *pPositionOut, QAngle *pAnglesOut );
	
	float GetTeleportDistance();
	void GetMaxSpeed( float *pMaxSpeedOut, float *pMaxAngularSpeedOut );
	
	// IJoltPhysicsController
	void OnPreSimulate( float flDeltaTime );

private:
	JoltPhysicsObject *m_pObject = nullptr;

	JPH::Vec3 m_targetPosition = JPH::Vec3::sZero();			// Where we want to be
	JPH::Quat m_targetRotation = JPH::Quat::sIdentity();		// How we want to be
	float m_secondsToArrival = 0;						// When we want to be

	float m_maxSpeed = 0.0f;
	float m_maxDampSpeed = 0.0f;
	float m_maxAngular = 0.0f;
	float m_maxDampAngular = 0.0f;
	float m_teleportDistance = 0.0f;
	bool m_isPhysicallyControlled = false;		// If true we're a bone follower on an NPC or something...
	bool m_allowTranslation = false;			// Should we translate?
	bool m_allowRotation = false;				// Should we rotate?

	bool m_enabled = false;

	uint16 m_savedMaterialIndex = 0;
	uint16 m_savedCallbackFlags = 0;
};
