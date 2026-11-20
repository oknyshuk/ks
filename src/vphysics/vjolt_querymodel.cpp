// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "cmodel.h"
#include "vphysics_interface.h"
#include "vjolt_util.h"

#include "vjolt_querymodel.h"

//-------------------------------------------------------------------------------------------------

JoltCollisionQuery::JoltCollisionQuery( JPH::Shape *pShape )
	: m_pShape ( pShape )
{
}

//-------------------------------------------------------------------------------------------------

int JoltCollisionQuery::ConvexCount()
{
	const JPH::StaticCompoundShape *pCompoundShape = GetCompoundShape( m_pShape );
	if ( pCompoundShape )
		return int( pCompoundShape->GetNumSubShapes() );

	// If we aren't a compound shape, then we have one.
	return 1;
}

int JoltCollisionQuery::TriangleCount( int convexIndex )
{
	return ActOnSubShape<int, JPH::Shape>( m_pShape, convexIndex, []( const JPH::Shape* pShape ) -> int
	{
		JPH::Shape::Stats stats = pShape->GetStats();
		return stats.mNumTriangles;
	} );
}

//-------------------------------------------------------------------------------------------------

unsigned int JoltCollisionQuery::GetGameData( int convexIndex )
{
	return static_cast< unsigned int >( m_pShape->GetUserData() );
}

//-------------------------------------------------------------------------------------------------

void JoltCollisionQuery::GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts )
{
	ActOnSubShape<int, JPH::Shape>( m_pShape, convexIndex, [&]( const JPH::Shape* pShape ) -> int
	{
		static constexpr int kRequestedTriangles = 2048;

		JPH::Shape::GetTrianglesContext ctx;
		pShape->GetTrianglesStart( ctx, JPH::AABox::sBiggest(), JPH::Vec3::sZero(), JPH::Quat::sIdentity(), JPH::Vec3( 1.0f, 1.0f, 1.0f ) );
		int i = 0;
		for ( ;; )
		{
			JPH::Float3 vertices[ kRequestedTriangles * 3 ];
			int count = pShape->GetTrianglesNext( ctx, kRequestedTriangles, vertices, nullptr /* materials */ );
			if ( count == 0 )
				break;

			if ( triangleIndex >= i && triangleIndex < i + count )
			{
				verts[ 0 ] = JoltToSource::Distance( vertices[ ( triangleIndex % kRequestedTriangles ) * 3 + 0 ] );
				verts[ 1 ] = JoltToSource::Distance( vertices[ ( triangleIndex % kRequestedTriangles ) * 3 + 1 ] );
				verts[ 2 ] = JoltToSource::Distance( vertices[ ( triangleIndex % kRequestedTriangles ) * 3 + 2 ] );
				return 0;
			}

			i += kRequestedTriangles;
		}

		return 1;
	} );

}

void JoltCollisionQuery::SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts )
{
}

//-------------------------------------------------------------------------------------------------

int JoltCollisionQuery::GetTriangleMaterialIndex( int convexIndex, int triangleIndex )
{
	return 0;
}

void JoltCollisionQuery::SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits )
{
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
#include "facade/icollisionquery.inl"
