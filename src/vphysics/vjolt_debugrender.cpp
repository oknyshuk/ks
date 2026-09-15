//=================================================================================================
//
// Jolt Debug Renderer Implementation
//
//=================================================================================================

#include "cbase.h"

#ifdef JPH_DEBUG_RENDERER

#include "vjolt_debugrender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Slart: Not sure if this is still relevant,
// it's the amount of time a debugoverlay element should stay on-screen
static constexpr float kOneSingleFrame = 0.0f;

static ConVar vjolt_debugrender( "vjolt_debugrender", "0", FCVAR_CHEAT );

//-------------------------------------------------------------------------------------------------

void JoltPhysicsDebugRenderer::DrawLine( JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor )
{
	GetDebugOverlay()->AddLineOverlay(
		JoltToSource::Distance( inFrom ), JoltToSource::Distance( inTo ),
		inColor.r, inColor.g, inColor.b, true, kOneSingleFrame );
}

void JoltPhysicsDebugRenderer::DrawText3D( JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor, float inHeight )
{
	// AddTextOverlayRGB takes a C string and a string_view is not necessarily terminated.
	// 1024 is as much as a debug overlay will hold anyway.
	char text[ 1024 ];
	V_strncpy( text, inString.data(), Min( sizeof( text ), inString.size() + 1 ) );

	GetDebugOverlay()->AddTextOverlayRGB( JoltToSource::Distance( inPosition ), 0, 0.5f,
		inColor.r, inColor.g, inColor.b, inColor.a, "%s", text );
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsDebugRenderer::RenderPhysicsSystem( JPH::PhysicsSystem &physicsSystem )
{
	if ( !GetDebugOverlay() || !vjolt_debugrender.GetBool() )
		return;

	// Shapes stay off: the overlay draws a line at a time, so a solid scene would swamp it
	// (and the materialsystem mempool with it). Bounds and velocities are what this is for.
	physicsSystem.DrawBodies( {
		.mDrawShape			= false,
		.mDrawBoundingBox	= true,
		.mDrawVelocity		= true,
	}, this );
}

//-------------------------------------------------------------------------------------------------

JoltPhysicsDebugRenderer &JoltPhysicsDebugRenderer::GetInstance()
{
	static JoltPhysicsDebugRenderer s_DebugRenderer;
	return s_DebugRenderer;
}

IVPhysicsDebugOverlay *JoltPhysicsDebugRenderer::GetDebugOverlay()
{
	return JoltPhysicsInterface::GetInstance().GetDebugOverlay();
}

//-------------------------------------------------------------------------------------------------

#endif // JPH_DEBUG_RENDERER
