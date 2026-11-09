// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

import ks.math;
#include "vphysics_interface.h"
#include "vphysics/friction.h"
import std;

class JoltPhysicsFrictionSnapshot final : public IPhysicsFrictionSnapshot
{
public:
	bool IsValid();

	IPhysicsObject *GetObject( int index );
	int GetMaterial( int index );

	void GetContactPoint( Vector &out );

	void GetSurfaceNormal( Vector &out );
	float GetNormalForce();
	float GetEnergyAbsorbed();

	void RecomputeFriction();
	void ClearFrictionForce();

	void MarkContactForDelete();
	void DeleteAllMarkedContacts( bool wakeObjects );

	void NextFrictionData();
	float GetFrictionCoefficient();
};
