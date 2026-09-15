
#pragma once

#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>

// The layer an object lives in decides what it may collide with. Each layer also gets its own
// broad phase tree, so the static world is not in a tree that has to be rebuilt every frame.
namespace Layers
{
	inline constexpr JPH::ObjectLayer NON_MOVING_WORLD	= 0;
	inline constexpr JPH::ObjectLayer NON_MOVING_OBJECT	= 1;
	inline constexpr JPH::ObjectLayer MOVING			= 2;
	inline constexpr JPH::ObjectLayer NO_COLLIDE		= 3;	// Collides with nothing, the world included.
	inline constexpr JPH::ObjectLayer DEBRIS			= 4;	// Collides with the non-moving layers only.

	inline constexpr uint NUM_LAYERS = 5;

	inline constexpr const char *Names[ NUM_LAYERS ] =
	{
		"NON_MOVING_WORLD", "NON_MOVING_OBJECT", "MOVING", "NO_COLLIDE", "DEBRIS",
	};

	// Object layers map onto broad phase layers one to one.
	inline constexpr JPH::BroadPhaseLayer BroadPhase( JPH::ObjectLayer layer )
	{
		return JPH::BroadPhaseLayer( JPH::BroadPhaseLayer::Type( layer ) );
	}
}

// The collision matrix. Jolt builds the object-vs-broad-phase filter out of the object pair
// filter, so the pairs enabled below are the only place the policy is written down.
struct JoltCollisionLayers
{
	// Built on first use: the tables allocate through Jolt's allocator, and that is installed
	// by JoltPhysicsInterface::Init, well after static initialisation has run.
	static const JoltCollisionLayers &Get()
	{
		static const JoltCollisionLayers s_Layers;
		return s_Layers;
	}

	JPH::ObjectLayerPairFilterTable			objectPairs{ Layers::NUM_LAYERS };
	JPH::BroadPhaseLayerInterfaceTable		broadPhase{ Layers::NUM_LAYERS, Layers::NUM_LAYERS };

	// Members initialise in declaration order, so Fill has run on both tables above by the time
	// this one reads them. The tables are non-copyable, hence filling them in place.
	JPH::ObjectVsBroadPhaseLayerFilterTable	objectVsBroadPhase
	{
		Fill( broadPhase ), Layers::NUM_LAYERS, Fill( objectPairs ), Layers::NUM_LAYERS
	};

private:
	static const JPH::ObjectLayerPairFilterTable &Fill( JPH::ObjectLayerPairFilterTable &filter )
	{
		filter.EnableCollision( Layers::MOVING, Layers::MOVING );
		filter.EnableCollision( Layers::MOVING, Layers::NON_MOVING_WORLD );
		filter.EnableCollision( Layers::MOVING, Layers::NON_MOVING_OBJECT );
		filter.EnableCollision( Layers::DEBRIS, Layers::NON_MOVING_WORLD );
		filter.EnableCollision( Layers::DEBRIS, Layers::NON_MOVING_OBJECT );
		return filter;
	}

	static const JPH::BroadPhaseLayerInterfaceTable &Fill( JPH::BroadPhaseLayerInterfaceTable &table )
	{
		for ( JPH::ObjectLayer layer = 0; layer < Layers::NUM_LAYERS; ++layer )
		{
			table.MapObjectToBroadPhaseLayer( layer, Layers::BroadPhase( layer ) );
		#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
			table.SetBroadPhaseLayerName( Layers::BroadPhase( layer ), Layers::Names[ layer ] );
		#endif
		}
		return table;
	}
};
