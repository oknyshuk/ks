//=================================================================================================
//
// The base physics DLL interface
//
//=================================================================================================

#pragma once

#include <unordered_map>

//-------------------------------------------------------------------------------------------------

// Call this in stubbed functions to spew when they're hit
#define Log_Stub( Channel )

// So we can toggle assertions in this module at our discretion
#if DEVELOPMENT_ONLY
#define VJoltAssert	DevAssert
#define VJoltAssertMsg DevAssertMsg
#else
#define VJoltAssert Assert
#define VJoltAssertMsg AssertMsg
#endif

DECLARE_LOGGING_CHANNEL( LOG_VJolt );			// For our vphysics_jolt code
DECLARE_LOGGING_CHANNEL( LOG_JoltInternal );	// For Jolt's traces/assertions. Do NOT use for our code.

//-------------------------------------------------------------------------------------------------

class JoltPhysicsCollisionSet final : public IPhysicsCollisionSet
{
public:

	void EnableCollisions( int index0, int index1 ) override
	{
		m_Bits[ index0 ] |= 1u << index1;
		m_Bits[ index1 ] |= 1u << index0;
	}

	void DisableCollisions( int index0, int index1 ) override
	{
		m_Bits[ index0 ] &= ~( 1u << index1 );
		m_Bits[ index1 ] &= ~( 1u << index0 );
	}

	bool ShouldCollide( int index0, int index1 ) override
	{
		return !!( m_Bits[ index0 ] & ( 1u << index1 ) );
	}

private:

	std::array< uint32, 32 > m_Bits = {};

};

//-------------------------------------------------------------------------------------------------

class JoltPhysicsInterface final : public CTier1AppSystem<IPhysics>
{
private:
	using BaseClass = CTier1AppSystem<IPhysics>;

public:
	InitReturnVal_t Init() override;
	void Shutdown() override;
	void *QueryInterface( const char *pInterfaceName ) override;

	IPhysicsEnvironment *CreateEnvironment() override;
	void DestroyEnvironment( IPhysicsEnvironment *pEnvironment ) override;
	IPhysicsEnvironment *GetActiveEnvironmentByIndex( int index ) override;

	IPhysicsObjectPairHash *CreateObjectPairHash() override;
	void DestroyObjectPairHash( IPhysicsObjectPairHash *pHash ) override;

	IPhysicsCollisionSet *FindOrCreateCollisionSet( uintptr_t id, int maxElementCount ) override;
	IPhysicsCollisionSet *FindCollisionSet( uintptr_t id ) override;
	void DestroyAllCollisionSets() override;

public:
	static JoltPhysicsInterface &GetInstance() { return s_PhysicsInterface; }
	JPH::TempAllocator *GetTempAllocator() { return m_pTempAllocator; }
	JPH::JobSystem *GetJobSystem() { return m_pJobSystem; }

	void SetDebugOverlay( IVPhysicsDebugOverlay *pOverlay ) { if ( m_pDebugOverlay != pOverlay ) m_pDebugOverlay = pOverlay; }
	IVPhysicsDebugOverlay *GetDebugOverlay() { return m_pDebugOverlay; }

private:
	static void OnTrace( const char *fmt, ... );
	static bool OnAssert( const char *inExpression, const char *inMessage, const char *inFile, uint inLine );

	std::unordered_map< uintptr_t, JoltPhysicsCollisionSet > m_CollisionSets;

	// We need a temp allocator for temporary allocations during the physics update. We're
	// pre-allocating 10 MB to avoid having to do allocations during the physics update. 
	// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
	// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
	// malloc / free.
	JPH::TempAllocator *m_pTempAllocator;

	// Not Jolt's own thread pool: physics dispatches onto the engine's global pool, so the
	// two cannot each size themselves against the whole machine. See CEngineJobSystem.
	JPH::JobSystem *m_pJobSystem;

	// For debugging stuff in collide and such.
	IVPhysicsDebugOverlay *m_pDebugOverlay = nullptr;

	static JoltPhysicsInterface s_PhysicsInterface;
};
