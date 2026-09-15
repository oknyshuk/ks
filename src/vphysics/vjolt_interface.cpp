//=================================================================================================
//
// The base physics DLL interface
//
//=================================================================================================

#include "cbase.h"

#include "vjolt_environment.h"
#include "vjolt_collide.h"
#include "vjolt_surfaceprops.h"
#include "vjolt_objectpairhash.h"

#include "vjolt_interface.h"

#include "vstdlib/jobthread.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------------------------------------------------------------

// Slart:
// Pre-allocate 64 megabytes for physics allocations.
// I don't think we've tuned this value. It's just a big number that we probably won't ever hit.
static constexpr uint kTempAllocSize = 64 * 1024 * 1024;

// Josh:
// We cannot support more than 64 threads doing physics work because
// of the code I wrote in vjolt_listener_contact to dispatch events.
// It uses a single uint64_t bitmask that is iterated on for the thread-local
// event vectors.
// This isn't an issue, the benefits of more threads tends to trail off between
// 8-16 threads anyway.
static constexpr uint kMaxPhysicsThreads = 64;

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VJolt, "VJolt", 0, LS_MESSAGE, Color( 205, 142, 212, 255 ) );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_JoltInternal, "Jolt" );

JoltPhysicsInterface JoltPhysicsInterface::s_PhysicsInterface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( JoltPhysicsInterface, IPhysics, VPHYSICS_INTERFACE_VERSION, JoltPhysicsInterface::GetInstance() );

//-------------------------------------------------------------------------------------------------

// Route every Jolt allocation through tier0 instead of the CRT, so physics memory is
// visible to the engine's allocator (and to its leak tracking). `new`/`delete` already
// land there via the Valve overrides in memoverride.cpp.
//
// Reallocate goes straight to g_pMemAlloc because memalloc.h has no MemAlloc_Realloc
// wrapper; like C realloc it has to accept a null block, which tier0 handles.
static void InstallJoltAllocator()
{
	JPH::Allocate        = []( size_t size )                         { return MemAlloc_Alloc( size ); };
	JPH::Reallocate      = []( void *block, size_t, size_t newSize ) { return g_pMemAlloc->Realloc( block, newSize ); };
	JPH::Free            = []( void *block )                         { MemAlloc_Free( block ); };
	JPH::AlignedAllocate = []( size_t size, size_t alignment )       { return MemAlloc_AllocAligned( size, alignment ); };
	JPH::AlignedFree     = []( void *block )                         { MemAlloc_FreeAligned( block ); };
}

//-------------------------------------------------------------------------------------------------

InitReturnVal_t JoltPhysicsInterface::Init()
{
	const InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	MathLib_Init();

	// Install callbacks. The allocator has to be in place before anything else touches Jolt.
	InstallJoltAllocator();
	JPH::Trace = JoltPhysicsInterface::OnTrace;
	JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = JoltPhysicsInterface::OnAssert; )

	// Create a factory
	JPH::Factory::sInstance = new JPH::Factory();

	// Register all Jolt physics types
	JPH::RegisterTypes();

	// Create an allocator for temporary allocations during physics simulations
	m_pTempAllocator = new JPH::TempAllocatorImpl( kTempAllocSize );

	// Josh:
	// We may want to replace this with a better heuristic, or add a launch arg for this in future.
	// Right now, this does what -1 does in Jolt, but limits it to 64 threads, as we cannot support
	// more than this (see above).
	//
	// oknyshuk: hardware_concurrency() - 1 assumed physics was the only thing on the machine. The
	// engine's global pool is already running its own workers, so both pools sized themselves to
	// the whole box and together oversubscribed it (measured: 16 compute workers on 14 cores).
	// Leave room for the main thread and for the engine pool's workers instead.
	const uint32 nEngineWorkers = static_cast<uint32>( Max( 0, GetGlobalThreadPoolWidth() ) );
	const uint32 nAvailable = Max( 1u, std::thread::hardware_concurrency() ) - 1;
	const uint32 threadCount = Min( Max( 1u, nAvailable - Min( nAvailable - 1, nEngineWorkers ) ), kMaxPhysicsThreads );
	m_pJobSystem = new JPH::JobSystemThreadPool( JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threadCount );

	Log_Msg( LOG_VJolt, "Physics job system: %u threads (%u logical processors, %u engine pool workers)\n",
		threadCount, std::thread::hardware_concurrency(), nEngineWorkers );

	return INIT_OK;
}

void JoltPhysicsInterface::Shutdown()
{
	delete m_pJobSystem;
	delete m_pTempAllocator;
	delete JPH::Factory::sInstance;

	BaseClass::Shutdown();
}

void *JoltPhysicsInterface::QueryInterface( const char *pInterfaceName )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();
	return factory( pInterfaceName, nullptr );	
}

//-------------------------------------------------------------------------------------------------

IPhysicsEnvironment *JoltPhysicsInterface::CreateEnvironment()
{
	return new JoltPhysicsEnvironment();
}

void JoltPhysicsInterface::DestroyEnvironment( IPhysicsEnvironment *pEnvironment )
{
	delete static_cast<JoltPhysicsEnvironment *>( pEnvironment );
}

IPhysicsEnvironment *JoltPhysicsInterface::GetActiveEnvironmentByIndex( int index )
{
	// Josh: Nothing uses this... ever.
	Log_Stub( LOG_VJolt );
	return nullptr;
}

//-------------------------------------------------------------------------------------------------

IPhysicsObjectPairHash *JoltPhysicsInterface::CreateObjectPairHash()
{
	return new JoltPhysicsObjectPairHash;
}

void JoltPhysicsInterface::DestroyObjectPairHash( IPhysicsObjectPairHash *pHash )
{
	delete static_cast<JoltPhysicsObjectPairHash *>( pHash );
}

//-------------------------------------------------------------------------------------------------

IPhysicsCollisionSet *JoltPhysicsInterface::FindOrCreateCollisionSet( uintptr_t id, int maxElementCount )
{
	if ( maxElementCount > 32 )
		return nullptr;

	if ( IPhysicsCollisionSet *pSet = FindCollisionSet( id ) )
		return pSet;

	auto result = m_CollisionSets.emplace( id, JoltPhysicsCollisionSet{} );
	return &result.first->second;
}

IPhysicsCollisionSet *JoltPhysicsInterface::FindCollisionSet( uintptr_t id )
{
	auto iter = m_CollisionSets.find( id );
	if ( iter != m_CollisionSets.end() )
		return &iter->second;

	return nullptr;
}

void JoltPhysicsInterface::DestroyAllCollisionSets()
{
	m_CollisionSets.clear();
}

//-------------------------------------------------------------------------------------------------

void JoltPhysicsInterface::OnTrace( const char *fmt, ... )
{
	va_list args;
	char msg[MAX_LOGGING_MESSAGE_LENGTH];

	va_start( args, fmt );
	V_vsnprintf( msg, sizeof( msg ), fmt, args );
	va_end( args );

	Log_Msg( LOG_JoltInternal, "%s\n", msg );
}

bool JoltPhysicsInterface::OnAssert( const char *inExpression, const char *inMessage, const char *inFile, uint inLine )
{
	const char *message = inMessage ? inMessage : inExpression;
	(void) message;
	VJoltAssertMsg( false, message );
	return false;
}
