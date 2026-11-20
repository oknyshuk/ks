// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "vjolt_internal_listeners.h"
#include "vphysics_interface.h"
import std;

class JoltPhysicsFluidController final : public IPhysicsFluidController
{
public:
	JoltPhysicsFluidController( JPH::PhysicsSystem *pPhysicsSystem, JoltPhysicsObject *pFluidObject, const fluidparams_t *pParams );
	~JoltPhysicsFluidController();

	void	SetGameData( void *pGameData );
	void *	GetGameData() const;

	void	GetSurfacePlane( Vector *pNormal, float *pDist ) const;
	float	GetDensity() const;
	void	WakeAllSleepingObjects();
	int		GetContents() const;

public:

	// IJoltObjectListener
	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );
	// IJoltPhysicsController
	void OnPreSimulate( float flDeltaTime );

private:

	cplane_t GetSurfacePlane() const;
	void ClearCachedObjectsInShape();

	JPH::PhysicsSystem *				m_pPhysicsSystem;
	JoltPhysicsObject *					m_pFluidObject;
	std::vector<JoltPhysicsObject *>	m_ObjectsInShape;

	fluidparams_t						m_Params;
	cplane_t							m_LocalPlane;
};
