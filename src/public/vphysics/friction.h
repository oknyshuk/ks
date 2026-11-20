#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// NOTE: This is an iterator for the contact points on an object
// NOTE: This should only be used temporarily.  Holding one of these
// NOTE: across collision callbacks or calls into simulation will cause errors!
// NOTE: VPHYSICS may choose to make the data contained within this object invalid 
// NOTE: any time simulation is run.
import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
class IPhysicsObject;
class IPhysicsFrictionSnapshot
{
public:

	bool IsValid();

	// Object 0 is this object, Object 1 is the other object
	IPhysicsObject *GetObject( int index );
	int GetMaterial( int index );

	void GetContactPoint( Vector &out );
	
	// points away from source object
	void GetSurfaceNormal( Vector &out );
	float GetNormalForce();
	float GetEnergyAbsorbed();

	// recompute friction (useful if dynamically altering materials/mass)
	void RecomputeFriction();
	// clear all friction force at this contact point
	void ClearFrictionForce();

	void MarkContactForDelete();
	void DeleteAllMarkedContacts( bool wakeObjects );

	// Move to the next friction data for this object
	void NextFrictionData();
	float GetFrictionCoefficient();
};
