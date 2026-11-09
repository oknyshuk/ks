#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// A set of collision rules
// NOTE: Defaults to all indices disabled
class IPhysicsCollisionSet
{
public:
	~IPhysicsCollisionSet() {}

	void EnableCollisions( int index0, int index1 );
	void DisableCollisions( int index0, int index1 );

	bool ShouldCollide( int index0, int index1 );
};
