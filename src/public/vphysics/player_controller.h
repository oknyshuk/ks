#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
class IPhysicsObject;
class IPhysicsPlayerControllerEvent
{
public:
	virtual int	ShouldMoveTo( IPhysicsObject *pObject, const Vector &position ) = 0;
};

enum PlayerContactState_t
{
	PLAYER_CONTACT_PHYSICS = 1,
	PLAYER_CONTACT_GAMEOBJECT = 2,
};

class IPhysicsPlayerController
{
public:

	void Update( const Vector &position, const Vector &velocity, float secondsToArrival, bool onground, IPhysicsObject *ground );
	void SetEventHandler( IPhysicsPlayerControllerEvent *handler );
	bool IsInContact( void );
	void MaxSpeed( const Vector &maxVelocity );

	// allows game code to change collision models
	void SetObject( IPhysicsObject *pObject );
	// UNDONE: Refactor this and shadow controllers into a single class/interface through IPhysicsObject
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
	// returns bitfield e.g. 0 (no contacts), 1 (has physics contact), 2 (contact matching nGameFlags), 3 (both 1 & 2)
	uint32 GetContactState( uint16 nGameFlags );
};
