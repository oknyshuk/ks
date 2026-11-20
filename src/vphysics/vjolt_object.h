// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)

//=================================================================================================
//
// A physics object
//
//=================================================================================================

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorder.h>
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "vphysics_interface.h"
#include "vjolt_internal_listeners.h"
import std;
#include "bspflags.h"

class IPredictedPhysicsObject;

class JoltPhysicsShadowController;
class JoltPhysicsFluidController;
class JoltPhysicsEnvironment;
class JoltPhysicsObject;

#if defined( GAME_CSGO_OR_NEWER )
using IPhysicsObjectInterface = IPredictedPhysicsObject;
#else
using IPhysicsObjectInterface = IPhysicsObject;
#endif

class JoltPhysicsObject final : public IPhysicsObjectInterface
{
public:
	JoltPhysicsObject( JPH::Body *pBody, JoltPhysicsEnvironment *pEnvironment, bool bStatic, int nMaterialIndex, const objectparams_t *pParams );
	JoltPhysicsObject( JPH::Body *pBody, JoltPhysicsEnvironment *pEnvironment, void *pGameData, JPH::StateRecorder &recorder );
	~JoltPhysicsObject();

	bool			IsStatic() const;
	bool			IsAsleep() const;
	bool			IsTrigger() const;
	bool			IsFluid() const;
	bool			IsHinged() const;
	bool			IsCollisionEnabled() const;
	bool			IsGravityEnabled() const;
	bool			IsDragEnabled() const;
	bool			IsMotionEnabled() const;
	bool			IsMoveable() const; // legacy: IsMotionEnabled() && !IsStatic()
	bool			IsAttachedToConstraint( bool bExternalOnly ) const;

	void			EnableCollisions( bool enable );
	void			EnableGravity( bool enable );
	void			EnableDrag( bool enable );
	void			EnableMotion( bool enable );

	void			SetGameData( void *pGameData );
	void *			GetGameData() const;
	void			SetGameFlags( unsigned short userFlags );
	unsigned short	GetGameFlags() const;
	void			SetGameIndex( unsigned short gameIndex );
	unsigned short	GetGameIndex() const;

	void			SetCallbackFlags( unsigned short callbackflags );
	unsigned short	GetCallbackFlags() const;

	void			Wake();
	void			Sleep();
	void			RecheckCollisionFilter();
	void			RecheckContactPoints( bool bSearchForNewContacts ) ;
	void			RecheckContactPoints()  { RecheckContactPoints( false ); }

	void			SetMass( float mass );
	float			GetMass() const;
	float			GetInvMass() const;
	Vector			GetInertia() const;
	Vector			GetInvInertia() const;
	void			SetInertia( const Vector &inertia );

	void			SetDamping( const float *speed, const float *rot );
	void			GetDamping( float *speed, float *rot ) const;

	void			SetDragCoefficient( float *pDrag, float *pAngularDrag );
	void			SetBuoyancyRatio( float ratio );

	int				GetMaterialIndex() const;
	void			SetMaterialIndex( int materialIndex );

	unsigned int	GetContents() const;
	void			SetContents( unsigned int contents );

	float			GetSphereRadius() const;
	void			SetSphereRadius( float radius ) ;
	float			GetEnergy() const;
	Vector			GetMassCenterLocalSpace() const;

	void			SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport );
	void			SetPositionMatrix( const matrix3x4_t &matrix, bool isTeleport );

	void			GetPosition( Vector *worldPosition, QAngle *angles ) const;
	void			GetPositionMatrix( matrix3x4_t *positionMatrix ) const;
	void			SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );

	void			SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity );

	void			GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;

	void			AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );
	void			GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const;
	void			GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;
	void			LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const;
	void			WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const;

	void			LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const;
	void			WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const;

	void			ApplyForceCenter( const Vector &forceVector );
	void			ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition );
	void			ApplyTorqueCenter( const AngularImpulse &torque );

	void			CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const;
	void			CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const;
	float			CalculateLinearDrag( const Vector &unitDirection ) const;
	float			CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const;

	bool			GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const;

	void			SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation );
	void			UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset );

	int							GetShadowPosition( Vector *position, QAngle *angles ) const;
	IPhysicsShadowController *	GetShadowController() const;
	void						RemoveShadowController();
	float						ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt );


	const CPhysCollide *	GetCollide() const;
	const char *			GetName() const;

	void			BecomeTrigger();
	void			RemoveTrigger();

	void			BecomeHinged( int localAxis );
	void			RemoveHinged();

	IPhysicsFrictionSnapshot *CreateFrictionSnapshot();
	void DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot );

	void			OutputDebugInfo() const;

	void			SetUseAlternateGravity( bool bSet ) ;
	void			SetCollisionHints( uint32 collisionHints ) ;
	uint32			GetCollisionHints() const ;

	IPredictedPhysicsObject *	GetPredictedInterface() const ;
	void						SyncWith( IPhysicsObject *pOther ) ;

	void SetErrorDelta_Position( const Vector& vPosition )  {}
	void SetErrorDelta_Velocity( const Vector& vVelocity )  {}

public:
	JoltPhysicsEnvironment *GetEnvironment() { return m_pEnvironment; }

	JPH::BodyID GetBodyID() { return m_pBody->GetID(); }
	JPH::Body *GetBody() { return m_pBody; }

	void UpdateEnvironment( JoltPhysicsEnvironment *pEnvironment );

	void AddListener( JoltObjectListener listener );
	void RemoveListener( JoltObjectListener listener );

	// Tells every listener the object moved discontinuously.
	void NotifyTeleported();

	// Grabs the position, adds addPos and teleports the object
	void AddToPosition( JPH::Vec3Arg addPos );

	// Only sets the position, and nothing else.
	void SetPosition( const Vector &worldPosition );

	// Adds to the velocity (Source space)
	void AddVelocity( const Vector &worldPosition );

	Vector GetVelocity();

	void CalculateBuoyancy();

	float GetMaterialDensity() const;
	float GetBuoyancyRatio() const;
	float GetVolume() const { return m_flVolume; }

	bool IsControlledByGame() const;

	void AddCallbackFlags( uint16 flags ) { m_callbackFlags |= flags; }
	void RemoveCallbackFlags( uint16 flags ) { m_callbackFlags &= ~flags; }

	void SaveObjectState( JPH::StateRecorder &recorder );
	void RestoreObjectState( JPH::StateRecorder &recorder );

	unsigned short GetGameMaterial() const
	{
		return m_GameMaterial;
	}

	bool GetGameMaterialAllowsSounds() const
	{
		return m_GameMaterial != 'X';
	}

	JoltPhysicsFluidController *GetFluidController()
	{
		return m_pFluidController;
	}

	void SetFluidController( JoltPhysicsFluidController *pFluidController )
	{
		m_pFluidController = pFluidController;
	}

	// Fakes a linear velocity so we can have correct before/after velocity
	// when going between PreCollision and PostCollision callbacks.
	JPH::Vec3 FakeJoltLinearVelocity( JPH::Vec3Arg fakeVelocity )
	{
		if ( m_pBody->IsStatic() )
			return JPH::Vec3::sZero();

		JPH::Vec3 oldVel = m_pBody->GetLinearVelocity();
		m_pBody->SetLinearVelocity( fakeVelocity );
		return oldVel;
	}

	void RestoreJoltLinearVelocity( JPH::Vec3Arg realVelocity )
	{
		if ( m_pBody->IsStatic() )
			return;

		m_pBody->SetLinearVelocity( realVelocity );
	}

private:
	void UpdateMaterialProperties();
	void UpdateLayer();

	// Josh:
	// Always put m_pGameData first. Some games that will
	// remain un-named offset by the vtable to get to this
	// instead of calling GetGameData().
	void *m_pGameData = nullptr;
	const char *m_pName = "NoName";

	uint16 m_gameFlags = 0;
	uint16 m_gameIndex = 0;
	uint16 m_callbackFlags = CALLBACK_GLOBAL_COLLISION|CALLBACK_GLOBAL_FRICTION|CALLBACK_FLUID_TOUCH|CALLBACK_GLOBAL_TOUCH|CALLBACK_GLOBAL_COLLIDE_STATIC|CALLBACK_DO_FLUID_SIMULATION;
	uint32 m_collisionHints = 0;

	bool m_bStatic = false;
	bool m_bPinned = false;

	int m_materialIndex = 0;
	uint m_contents = CONTENTS_SOLID;

	// Need this as Jolt gets very unhappy about reading motion
	// properties of static objects.
	float m_flCachedMass = 0.0f;
	float m_flCachedInvMass = 0.0f;
	bool m_bCachedCollisionEnabled = true;

	float m_flMaterialDensity = 1.0f; // Material density in Jolt space.
	float m_flBuoyancyRatio = 0.0f;
	float m_flVolume = 0.0f;

	unsigned short m_GameMaterial = 0;


	std::vector< JoltObjectListener > m_Listeners;

	// Shadow variables
	JoltPhysicsShadowController *m_pShadowController = nullptr;
	JoltPhysicsFluidController *m_pFluidController = nullptr;
	bool m_bShadowTemporarilyDisableGravity = false;

	JPH::Body *m_pBody = nullptr;						// Underlying Jolt body
	JoltPhysicsEnvironment *m_pEnvironment = nullptr;	// Physics environment this body belongs to
	JPH::PhysicsSystem *m_pPhysicsSystem = nullptr;		// Physics system this body belongs to
};

// Josh: This doesn't handle mass change and is kind of a hack and sliightly wrong.
// Would be nice to just specify spring constant directly in Jolt.
inline float GetInvEffectiveMass( JoltPhysicsObject *pObject0, JoltPhysicsObject *pObject1 )
{
	return ( pObject0->IsStatic() ? 0.0f : pObject0->GetInvMass() ) + ( pObject1->IsStatic() ? 0.0f : pObject1->GetInvMass() );
}

