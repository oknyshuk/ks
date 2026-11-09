// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)

//=================================================================================================
//
// Constraints
//
//=================================================================================================

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include "vjolt_internal_listeners.h"
#include "vphysics_interface.h"
#include "vphysics/constraints.h"

enum constraintType_t
{
	CONSTRAINT_UNKNOWN = 0,
	CONSTRAINT_RAGDOLL,
	CONSTRAINT_HINGE,
	CONSTRAINT_FIXED,
	CONSTRAINT_BALLSOCKET,
	CONSTRAINT_SLIDING,
	CONSTRAINT_PULLEY,
	CONSTRAINT_LENGTH,
};

class JoltPhysicsConstraint;
class JoltPhysicsEnvironment;

class JoltPhysicsConstraintGroup final : public IPhysicsConstraintGroup
{
public:
	JoltPhysicsConstraintGroup();
	~JoltPhysicsConstraintGroup();

	void Activate();
	bool IsInErrorState();
	void ClearErrorState();
	void GetErrorParams( constraint_groupparams_t *pParams );
	void SetErrorParams( const constraint_groupparams_t &params );
	void SolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1 );

	void AddConstraint( JoltPhysicsConstraint *pConstraint );
	void RemoveConstraint( JoltPhysicsConstraint *pConstraint );

	// [CS_PATCH]
	void DetachAllConstraints();

private:
	std::vector< JoltPhysicsConstraint * >	m_pConstraints;
	constraint_groupparams_t				m_ErrorParams = {};
};

class JoltPhysicsConstraint final : public IPhysicsConstraint
{
public:
	JoltPhysicsConstraint( JoltPhysicsEnvironment *pPhysicsEnvironment, IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, constraintType_t Type = CONSTRAINT_UNKNOWN, JPH::Constraint* pConstraint = nullptr, void *pGameData = nullptr );
	~JoltPhysicsConstraint();

	void			Activate();
	void			Deactivate();

	void			SetGameData( void *gameData );
	void *			GetGameData() const;

	IPhysicsObject *GetReferenceObject() const;
	IPhysicsObject *GetAttachedObject() const;

	void			SetLinearMotor( float speed, float maxLinearImpulse );
	void			SetAngularMotor( float rotSpeed, float maxAngularImpulse );

	void			UpdateRagdollTransforms( const matrix3x4_t &constraintToReference, const matrix3x4_t &constraintToAttached );
	bool			GetConstraintTransform( matrix3x4_t *pConstraintToReference, matrix3x4_t *pConstraintToAttached ) const;
	bool			GetConstraintParams( constraint_breakableparams_t *pParams ) const;

	void			OutputDebugInfo();

	// IJoltObjectListener
	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );
	void OnJoltPhysicsObjectTeleported( JoltPhysicsObject *pObject );

public:
	bool InitialiseHingeFromRagdoll( IPhysicsConstraintGroup* pGroup, const constraint_ragdollparams_t& ragdoll );
	void InitialiseRagdoll( IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );
	void InitialiseHinge( IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge );
	void InitialiseSliding( IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding );
	void InitialiseBallsocket( IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
	void InitialiseFixed( IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
	void InitialiseLength( IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

private:

	friend class JoltPhysicsConstraintGroup; // [CS_PATCH]

	void SetGroup( IPhysicsConstraintGroup *pGroup );

	void DestroyConstraint();

	JoltPhysicsObject			*m_pObjReference = nullptr;
	JoltPhysicsObject			*m_pObjAttached = nullptr;
	JPH::Ref< JPH::Constraint > m_pConstraint;
	constraintType_t			m_ConstraintType = CONSTRAINT_UNKNOWN;

	JoltPhysicsConstraintGroup	*m_pGroup = nullptr;

	void						*m_pGameData = nullptr;
	JoltPhysicsEnvironment		*m_pPhysicsEnvironment = nullptr;
	JPH::PhysicsSystem			*m_pPhysicsSystem = nullptr;
};

namespace JPH { class DistanceConstraint; }

// A spring between two objects.
class JoltPhysicsSpring final : public IPhysicsSpring
{
public:
	JoltPhysicsSpring( JPH::PhysicsSystem *pPhysicsSystem, JoltPhysicsObject *pObjectStart, JoltPhysicsObject *pObjectEnd, springparams_t *pParams );
	~JoltPhysicsSpring();

	void GetEndpoints( Vector *worldPositionStart, Vector *worldPositionEnd );
	void SetSpringConstant( float flSpringConstant );
	void SetSpringDamping( float flSpringDamping );
	void SetSpringLength( float flSpringLength );

	IPhysicsObject *GetStartObject();
	IPhysicsObject *GetEndObject();

	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );

private:
	JPH::PhysicsSystem *m_pPhysicsSystem = nullptr;

	JoltPhysicsObject *m_pObjectStart = nullptr;
	JoltPhysicsObject *m_pObjectEnd = nullptr;

	JPH::DistanceConstraint *m_pConstraint = nullptr;
	bool m_OnlyStretch = false;
};

