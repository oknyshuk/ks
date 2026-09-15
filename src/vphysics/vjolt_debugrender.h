
#pragma once

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererSimple.h>

// The engine's physics debug overlay draws lines and text, nothing else. DebugRendererSimple
// reduces everything Jolt asks for -- shapes, arrows, markers, coordinate frames -- to those
// two primitives, so those are the only calls we have to implement.
class JoltPhysicsDebugRenderer final : public JPH::DebugRendererSimple
{
public:
	void DrawLine( JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor ) override;
	void DrawText3D( JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor, float inHeight ) override;

	void RenderPhysicsSystem( JPH::PhysicsSystem &physicsSystem );

	static JoltPhysicsDebugRenderer &GetInstance();
	static IVPhysicsDebugOverlay *GetDebugOverlay();
};

#endif
