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
//
// Jolt on the engine's thread pool.
//
// JobSystemThreadPool starts and sizes its own workers, so physics and the engine's global pool
// each sized themselves against the whole machine and had to be reconciled by hand: vphysics
// subtracted GetGlobalThreadPoolWidth() from hardware_concurrency(), arithmetic that had to be
// kept in step with DefaultGlobalWorkerCount() in vstdlib.
//
// JobSystemWithBarrier is Jolt's supported hook for a host scheduler. It implements the barrier
// half of the interface, leaving only job storage and queueing to us, so physics can ride
// g_pThreadPool and the process has exactly one worker count.
//
class CEngineJobSystem final : public JPH::JobSystemWithBarrier
{
public:
	explicit CEngineJobSystem( uint inMaxJobs )
	{
		Init( JPH::cMaxPhysicsBarriers );
		m_Jobs.Init( inMaxJobs, inMaxJobs );
	}

	JobHandle CreateJob( const char *inName, JPH::ColorArg inColor, const JobFunction &inJobFunction, JPH::uint32 inNumDependencies = 0 ) override
	{
		JPH::uint32 iJob;
		while ( ( iJob = m_Jobs.ConstructObject( inName, inColor, this, inJobFunction, inNumDependencies ) ) == AvailableJobs::cInvalidObjectIndex )
		{
			// cMaxPhysicsJobs is a hard ceiling: running out means jobs leaked, not that the
			// caller should cope.
			VJoltAssertMsg( false, "Jolt: no jobs available" );
			ThreadSleep( 0 );
		}

		Job *pJob = &m_Jobs.Get( iJob );

		// Takes a reference of its own; the job may complete as soon as it is queued.
		JobHandle handle( pJob );
		if ( inNumDependencies == 0 )
		{
			QueueJob( pJob );
		}

		return handle;
	}

	int GetMaxConcurrency() const override
	{
		// The barrier's waiting thread runs jobs too. With no pool running this is 1 and the
		// barrier executes every job itself, so physics still progresses, just unthreaded.
		return ( g_pThreadPool != nullptr ? Max( 1, g_pThreadPool->NumThreads() ) : 1 ) + 1;
	}

protected:
	void QueueJob( Job *inJob ) override
	{
		// Nowhere to queue it; the barrier will run it on the thread waiting for it.
		if ( g_pThreadPool == nullptr || g_pThreadPool->NumThreads() == 0 )
		{
			return;
		}

		inJob->AddRef();		// the queue holds this until the bridge below dies
		CJobBridge *pBridge = new CJobBridge( inJob );
		g_pThreadPool->AddJob( pBridge );

		// AddJob does not consume the caller's reference: queued leaves the queue's own, and an
		// inline run (no idle workers) is freed by this. See CThreadPool::AddFunctorInternal.
		pBridge->Release();
	}

	void QueueJobs( Job **inJobs, JPH::uint inNumJobs ) override
	{
		for ( JPH::uint i = 0; i < inNumJobs; ++i )
		{
			QueueJob( inJobs[i] );
		}
	}

	void FreeJob( Job *inJob ) override { m_Jobs.DestructObject( inJob ); }

private:
	// A physics job wrapped for the engine pool. Job::Execute is idempotent -- it CASes the
	// dependency counter to an executing state -- so a job the waiting thread already ran through
	// BarrierImpl::Wait() is a no-op if a worker reaches it first. That is what makes queueing to
	// a second scheduler safe rather than a double execution.
	class CJobBridge final : public CJob
	{
	public:
		explicit CJobBridge( Job *inJob ) : m_pJob( inJob ) {}

		// Releases on destruction rather than in DoExecute: the pool can also discard a queued job
		// without servicing it (AbortAll, queue flush at Stop), and the reference has to come back
		// exactly once on every path.
		~CJobBridge() override { m_pJob->Release(); }

		JobStatus_t DoExecute() override { m_pJob->Execute(); return JOB_OK; }

	private:
		Job *m_pJob;
	};

	using AvailableJobs = JPH::FixedSizeFreeList<Job>;
	AvailableJobs m_Jobs;
};

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

	// Physics jobs run on the engine's global worker pool rather than on a pool of their own, so
	// the two cannot each size themselves against the whole machine. See CEngineJobSystem above.
	m_pJobSystem = new CEngineJobSystem( JPH::cMaxPhysicsJobs );

	Log_Msg( LOG_VJolt, "Physics job system: engine global pool (%d workers, %d concurrent jobs)\n",
		g_pThreadPool != nullptr ? g_pThreadPool->NumThreads() : 0, m_pJobSystem->GetMaxConcurrency() );

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
