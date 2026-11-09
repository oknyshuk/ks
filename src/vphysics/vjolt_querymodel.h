// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
import ks.math;
#include "vphysics_interface.h"
import std;

class JoltCollisionQuery final : public ICollisionQuery
{
public:
	JoltCollisionQuery( JPH::Shape *pShape );

	int				ConvexCount();
	int				TriangleCount( int convexIndex );

	unsigned int	GetGameData( int convexIndex );

	void			GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts );
	void			SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts );
	
	int				GetTriangleMaterialIndex( int convexIndex, int triangleIndex );
	void			SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits );

private:

	JPH::Ref<JPH::Shape> m_pShape;
};
