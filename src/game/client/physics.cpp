// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "cdll_int.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "vphysics/collision_set.h"
#include "vphysics/constraints.h"
#include "vphysics/friction.h"
#include "vphysics/object_hash.h"
#include "bspflags.h"
#include "const.h"
#include "vphysics_interface.h"
#include "shareddefs.h"
#include "client_class.h"
#include "reflect_recvtable.h"
#include "bone_setup.h"
#include "bspfile.h"
#include "iclientrenderable.h"
#include "model_types.h"
#include "tier2/resourceprecacher.h"
#include "vcollide_parse.h"
#include "mathlib/softbodyenvironment.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.convar_serverbounded;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.dbg;
import ks.core.platform;
import ks.game.vphysicsupdateai;
import ks.tier1.fixed_pool;
import ks.tier1.strtools;
import ks.vstdlib.callqueue;
import :cstrike15.cs_shareddefs;
import :entitydatainstantiator;
import :econ.econ_item_schema_minimal;
import ks.tier1.keyvalues;

class CBaseAnimating;
class CBaseEntity;
class IGameSystem;

using namespace ::ks::reflect;

// file system interface
extern IFileSystem *filesystem;

static ConVar	cl_phys_timescale( side_registry(), "cl_phys_timescale", "1.0", FCVAR_CHEAT, "Sets the scale of time for client-side physics (ragdolls)" );
static ConVar	cl_phys_maxticks( side_registry(), "cl_phys_maxticks", "0", FCVAR_NONE, "Sets the max number of physics ticks allowed for client-side physics (ragdolls)" );
ConVar	cl_ragdoll_gravity( side_registry(), "cl_ragdoll_gravity", "600", FCVAR_CHEAT, "Sets the gravity client-side ragdolls" );
ConVar phys_debug_check_contacts(side_registry(), "phys_debug_check_contacts", "0", FCVAR_CHEAT|FCVAR_REPLICATED);

// blocked entity detecting
static ConVar cl_phys_block_fraction(side_registry(), "cl_phys_block_fraction", "0.1");
static ConVar cl_phys_block_dist(side_registry(), "cl_phys_block_dist","1.0");

void PrecachePhysicsSounds( void );

//FIXME: Replicated from server end, consolidate?

std::vector<CBaseEntity *> g_ShadowEntities;

void PhysAddShadow( CBaseEntity *pEntity )
{
	if( !ks::contains( g_ShadowEntities, pEntity ) )
	{
		g_ShadowEntities.push_back( pEntity );
	}
}

void PhysRemoveShadow( CBaseEntity *pEntity )
{
	ks::erase_first( g_ShadowEntities, pEntity );
}

extern IVEngineClient *engine;

struct penetrateevent_t
{
	CBaseEntity *pEntity0;
	CBaseEntity *pEntity1;
	float		startTime;
	float		timeStamp;
};

class CCollisionEvent : public IPhysicsCollisionEvent, public IPhysicsCollisionSolver, public IPhysicsObjectEvent
{
public:
	CCollisionEvent( void );

	void	ObjectSound( int index, vcollisionevent_t *pEvent );
	void	PreCollision( vcollisionevent_t *pEvent ) {}
	void	PostCollision( vcollisionevent_t *pEvent );
	void	Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData );

	void	BufferTouchEvents( bool enable ) { m_bBufferTouchEvents = enable; }

	void	StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );
	void	EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );

	void	FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void	FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void	PostSimulationFrame() {}

	virtual void ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}
	virtual void ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}

	float	DeltaTimeSinceLastFluid( CBaseEntity *pEntity );
	void	FrameUpdate( void );

	void	UpdateFluidEvents( void );
	void	UpdateTouchEvents( void );
	void	UpdatePenetrateEvents();

	void	LevelShutdown();
	// IPhysicsCollisionSolver
	int		ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
	// debugging collision problem in TF2
	int		ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt );
	bool	ShouldFreezeObject( IPhysicsObject *pObject )
	{
		// shadow controlled objects are probably server side.
		// UNDONE: Explicitly flag server side objects?
		if ( pObject->GetShadowController() )
			return false;
		return true;
	}
	int		AdditionalCollisionChecksThisTick( int currentChecksDone ) { return 0; }
	bool ShouldFreezeContacts( IPhysicsObject **pObjectList, int objectCount )  { return true; }

	// IPhysicsObjectEvent
	virtual void ObjectWake( IPhysicsObject *pObject )
	{
		CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
		if (pEntity && pEntity->HasDataObjectType(VPHYSICSWATCHER))
		{
			ReportVPhysicsStateChanged( pObject, pEntity, true );
		}
	}

	virtual void ObjectSleep( IPhysicsObject *pObject )
	{
		CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
		if ( pEntity && pEntity->HasDataObjectType( VPHYSICSWATCHER ) )
		{
			ReportVPhysicsStateChanged( pObject, pEntity, false );
		}
	}

	friction_t *FindFriction( CBaseEntity *pObject );
	void ShutdownFriction( friction_t &friction );
	void UpdateFrictionSounds();
	bool IsInCallback() { return m_inCallback > 0 ? true : false; }

private:
	class CallbackContext
	{
	public:
		explicit CallbackContext(CCollisionEvent *pOuter)
		{
			m_pOuter = pOuter;
			m_pOuter->m_inCallback++;
		}
		~CallbackContext()
		{
			m_pOuter->m_inCallback--;
		}
	private:
		CCollisionEvent *m_pOuter;
	};
	friend class CallbackContext;
	
	void	AddTouchEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1, int touchType, const Vector &point, const Vector &normal );
	void	DispatchStartTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1, const Vector &point, const Vector &normal );
	void	DispatchEndTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
	void	FindOrAddPenetrateEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );

	friction_t					m_current[8];
	std::vector< fluidevent_t >	m_fluidEvents;
	std::vector< touchevent_t >	m_touchEvents;
	std::vector< penetrateevent_t > m_penetrateEvents;
	int							m_inCallback;
	bool						m_bBufferTouchEvents;

	float						m_flLastSplashTime;
};

CCollisionEvent g_Collisions;

bool PhysIsInCallback()
{
	if ( (physenv && physenv->IsInSimulation()) || g_Collisions.IsInCallback() )
		return true;

	return false;
}

bool PhysicsDLLInit( CreateInterfaceFn physicsFactory )
{
	if ((physics = (IPhysics *)physicsFactory( VPHYSICS_INTERFACE_VERSION, nullptr )) == nullptr ||
		(physprops = (IPhysicsSurfaceProps *)physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, nullptr )) == nullptr ||
		(physcollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, nullptr )) == nullptr )
	{
		return false;
	}

	PhysParseSurfaceData( physprops, filesystem );
	return true;
}

extern ConVar_ServerBounded *cl_predict;
ConVar cl_predictphysics( side_registry(), "cl_predictphysics", "0", 0, "Use a prediction-friendly physics interface on the client" );

void PhysicsLevelInit( void )
{
	physenv = physics->CreateEnvironment();
	contract_assert( physenv );
	if( gpGlobals->IsRemoteClient() && g_pGameRules->IsMultiplayer() && cl_predictphysics.GetBool() )
	{
		physenv->SetPredicted( true );
	}

	{
	g_EntityCollisionHash = physics->CreateObjectPairHash();
	}

	// TODO: need to get the right factory function here
	//physenv->SetDebugOverlay( appSystemFactory );
	physenv->SetGravity( Vector(0, 0, -sv_gravity.GetFloat() ) );
	physenv->SetAlternateGravity( Vector(0, 0, -cl_ragdoll_gravity.GetFloat() ) );
	
	// NOTE: Always run client physics at a rate >= 45Hz - helps keep ragdolls stable
	const float defaultPhysicsTick = 1.0f / 60.0f; // 60Hz to stay in sync with x360 framerate of 30Hz
	physenv->SetSimulationTimestep( defaultPhysicsTick );

	physenv->SetCollisionEventHandler( &g_Collisions );
	physenv->SetCollisionSolver( &g_Collisions );

	CWorld *pWorld = GetClientWorldEntity();
	g_PhysWorldObject = PhysCreateWorld_Shared( pWorld, modelinfo->GetVCollide(1), g_PhysDefaultObjectParams );

	staticpropmgr->CreateVPhysicsRepresentations( physenv, &g_SolidSetup, pWorld );
}

void PhysicsReset()
{
	if ( !physenv )
		return;

	physenv->ResetSimulationClock();
}

static CBaseEntity *FindPhysicsBlocker( IPhysicsObject *pPhysics )
{
	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	CBaseEntity *pBlocker = nullptr;
	float maxVel = 10.0f;
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		if ( pOther->IsMoveable() )
		{
			CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
			// dot with this if you have a direction
			//Vector normal;
			//pSnapshot->GetSurfaceNormal(normal);
			float force = pSnapshot->GetNormalForce();
			float vel = force * pOther->GetInvMass();
			if ( vel > maxVel )
			{
				pBlocker = pOtherEntity;
				maxVel = vel;
			}

		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	return pBlocker;
}

ConVar cl_ragdoll_collide( side_registry(), "cl_ragdoll_collide", "0" );

int CCollisionEvent::ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 )
{
	CallbackContext callback(this);

	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pGameData0);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pGameData1);

	if ( !pEntity0 || !pEntity1 )
		return 1;

	unsigned short gameFlags0 = pObj0->GetGameFlags();
	unsigned short gameFlags1 = pObj1->GetGameFlags();

	if ( pEntity0 == pEntity1 )
	{
		// allow all-or-nothing per-entity disable
		if ( (gameFlags0 | gameFlags1) & FVPHYSICS_NO_SELF_COLLISIONS )
			return 0;

		IPhysicsCollisionSet *pSet = physics->FindCollisionSet( pEntity0->GetModelIndex() );
		if ( pSet )
			return pSet->ShouldCollide( pObj0->GetGameIndex(), pObj1->GetGameIndex() );

		return 1;
	}
	if ( (pObj0->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL) && (pObj1->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL) )
	{
		return cl_ragdoll_collide.GetBool();
	}

	// Obey collision group rules
	contract_assert( GameRules() );
	if ( GameRules() )
	{
		if (!GameRules()->ShouldCollide( pEntity0->GetCollisionGroup(), pEntity1->GetCollisionGroup() ))
			return 0;
	}

	// check contents
	if ( !(pObj0->GetContents() & pEntity1->PhysicsSolidMaskForEntity()) || !(pObj1->GetContents() & pEntity0->PhysicsSolidMaskForEntity()) )
		return 0;

	if ( g_EntityCollisionHash->IsObjectPairInHash( pGameData0, pGameData1 ) )
		return 0;

	if ( g_EntityCollisionHash->IsObjectPairInHash( pObj0, pObj1 ) )
		return 0;

	int movetype0 = pEntity0->GetMoveType();
	int movetype1 = pEntity1->GetMoveType();

	// entities with non-physical move parents or entities with MOVETYPE_PUSH
	// are considered as "AI movers".  They are unchanged by collision; they exert
	// physics forces on the rest of the system.
	bool aiMove0 = (movetype0 == MOVETYPE_PUSH || movetype0 == MOVETYPE_NONE) ? true : false;
	bool aiMove1 = (movetype1 == MOVETYPE_PUSH || movetype1 == MOVETYPE_NONE) ? true : false;
	
	// Anything with custom movement and a shadow controller is assumed to do its own world/AI collisions
	if ( movetype0 == MOVETYPE_CUSTOM && pObj0->GetShadowController() )
	{
		aiMove0 = true;
	}
	if ( movetype1 == MOVETYPE_CUSTOM && pObj1->GetShadowController() )
	{
		aiMove1 = true;
	}

	if ( pEntity0->GetMoveParent() )
	{
		// if the object & its parent are both MOVETYPE_VPHYSICS, then this must be a special case
		// like a prop_ragdoll_attached
		if ( !(movetype0 == MOVETYPE_VPHYSICS && pEntity0->GetRootMoveParent()->GetMoveType() == MOVETYPE_VPHYSICS) )
		{
			aiMove0 = true;
		}
	}
	if ( pEntity1->GetMoveParent() )
	{
		// if the object & its parent are both MOVETYPE_VPHYSICS, then this must be a special case.
		if ( !(movetype1 == MOVETYPE_VPHYSICS && pEntity1->GetRootMoveParent()->GetMoveType() == MOVETYPE_VPHYSICS) )
		{
			aiMove1 = true;
		}
	}

	// AI movers don't collide with the world/static/pinned objects or other AI movers
	if ( (aiMove0 && !pObj1->IsMoveable()) ||
		(aiMove1 && !pObj0->IsMoveable()) ||
		(aiMove0 && aiMove1) )
		return 0;

	// two objects under shadow control should not collide.  The AI will figure it out
	if ( pObj0->GetShadowController() && pObj1->GetShadowController() )
		return 0;
	return 1;
}

int CCollisionEvent::ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt )
{
	CallbackContext callback(this);
	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pGameData0);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pGameData1);

	// solve it yourself here and return 0, or have the default implementation do it
	if ( pEntity0 > pEntity1 )
	{
		// swap sort
		CBaseEntity *pTmp = pEntity0;
		pEntity0 = pEntity1;
		pEntity1 = pTmp;
		IPhysicsObject *pTmpObj = pObj0;
		pObj0 = pObj1;
		pObj1 = pTmpObj;
	}

	if ( !pEntity0 || !pEntity1 )
		return 1;

	unsigned short gameFlags0 = pObj0->GetGameFlags();
	unsigned short gameFlags1 = pObj1->GetGameFlags();

	// solve it yourself here and return 0, or have the default implementation do it
	if ( pGameData0 == pGameData1 )
	{
		if ( gameFlags0 & FVPHYSICS_PART_OF_RAGDOLL )
		{
			// this is a ragdoll, self penetrating
			CBaseEntity *pEnt = reinterpret_cast<CBaseEntity *>(pGameData0);
			CBaseAnimating *pAnim = pEnt->GetBaseAnimating();

			if ( pAnim && pAnim->m_pRagdoll )
			{
				IPhysicsConstraintGroup *pGroup = pAnim->m_pRagdoll->GetConstraintGroup();
				if ( pGroup )
				{
					pGroup->SolvePenetration( pObj0, pObj1 );
					return false;
				}
			}
		}
	}
	else if ( (gameFlags0|gameFlags1) & FVPHYSICS_PART_OF_RAGDOLL )
	{
		// ragdoll penetrating shadow object, just give up for now
		if ( pObj0->GetShadowController() || pObj1->GetShadowController() )
		{
			FindOrAddPenetrateEvent( pEntity0, pEntity1 );
			return true;
		}

	}

	return true;
}

// A class that implements an IClientSystem for physics
class CPhysicsSystem : public CAutoGameSystemPerFrame
{
public:
	explicit CPhysicsSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	// HACKHACK: PhysicsDLLInit() is called explicitly because it requires a parameter
	virtual bool Init();
	virtual void Shutdown();

	// Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();

	// The level is shutdown in two parts
	virtual void LevelShutdownPreEntity();
	
	virtual void LevelShutdownPostEntity();

	void AddImpactSound( void *pGameData, IPhysicsObject *pObject, int surfaceProps, int surfacePropsHit, float volume, float speed );

	virtual void Update( float frametime );

	void PhysicsSimulate();

private:
	physicssound::soundlist_t m_impactSounds;
};

static CPhysicsSystem g_PhysicsSystem( "CPhysicsSystem" );
// singleton to hook into the client system
IGameSystem *PhysicsGameSystem( void )
{
	return &g_PhysicsSystem;
}

// HACKHACK: PhysicsDLLInit() is called explicitly because it requires a parameter
bool CPhysicsSystem::Init()
{
	return true;
}

void CPhysicsSystem::Shutdown()
{
}

// Level init, shutdown
void CPhysicsSystem::LevelInitPreEntity( void )
{
	m_impactSounds.RemoveAll();
	PrecachePhysicsSounds();
}

void CPhysicsSystem::LevelInitPostEntity( void )
{
	PhysicsLevelInit();
}

// The level is shutdown in two parts
void CPhysicsSystem::LevelShutdownPreEntity()
{
	if ( physenv )
	{
		// we may have deleted multiple objects including the world by now, so 
		// don't try to wake them up
		physenv->SetQuickDelete( true );
	}
}

void CPhysicsSystem::LevelShutdownPostEntity()
{
	g_Collisions.LevelShutdown();
	if ( physenv )
	{
		// environment destroys all objects
		// entities are gone, so this is safe now
		physics->DestroyEnvironment( physenv );
	}
	physics->DestroyObjectPairHash( g_EntityCollisionHash );
	g_EntityCollisionHash = nullptr;

	physics->DestroyAllCollisionSets();

	physenv = nullptr;
	g_PhysWorldObject = nullptr;
}

void CPhysicsSystem::AddImpactSound( void *pGameData, IPhysicsObject *pObject, int surfaceProps, int surfacePropsHit, float volume, float speed )
{
	physicssound::AddImpactSound( m_impactSounds, pGameData, SOUND_FROM_WORLD, CHAN_STATIC, pObject, surfaceProps, surfacePropsHit, volume, speed );
}

void CPhysicsSystem::Update( float frametime )
{
	// THIS WAS MOVED TO POST-ENTITY SIM
	//PhysicsSimulate();
}

//#ifdef _LINUX
//#else
//#endif

//ConVar cl_visualize_physics_shadows("cl_visualize_physics_shadows","0");

struct blocklist_t
{
	CBaseEntity *pEntity;
	int firstBlockFrame;
	int lastBlockFrame;
};
static blocklist_t g_BlockList[4];

bool IsBlockedShouldDisableCollisions( CBaseEntity *pEntity )
{
	int listCount = std::size( g_BlockList );
	int available = -1;
	for ( int i = 0; i < listCount; i++ )
	{
		if ( gpGlobals->framecount - g_BlockList[i].lastBlockFrame > 4 )
		{
			available = i;
			g_BlockList[i].pEntity = nullptr;
		}
		if ( g_BlockList[i].pEntity == pEntity )
		{
			available = i;
			break;
		}
	}
	if ( available )
	{
		if ( g_BlockList[available].pEntity != pEntity )
		{
			g_BlockList[available].pEntity = pEntity;
			g_BlockList[available].firstBlockFrame = gpGlobals->framecount;
		}
		g_BlockList[available].lastBlockFrame = gpGlobals->framecount;
		if ( g_BlockList[available].lastBlockFrame - g_BlockList[available].firstBlockFrame > 2 )
			return true;
	}
	return false;
}

ConVar cl_phys_show_active( side_registry(), "cl_phys_show_active", "0", FCVAR_CHEAT );

void CPhysicsSystem::PhysicsSimulate()
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "CPhysicsSystem::PhysicsSimulate", 0, VPROF_BUDGETGROUP_PHYSICS, false, BUDGETFLAG_OTHER );
	float frametime = gpGlobals->frametime;

	if ( physenv )
	{
		if( physenv->IsPredicted() )
		{
			if( !prediction->InPrediction() )
				return;

			if( !prediction->IsFirstTimePredicted() )
			{
				//Don't actually simulate. Fake it while restoring results from the first time
				physenv->RestorePredictedSimulation();

				int activeCount = physenv->GetActiveObjectCount();
				if ( activeCount )
				{
					IPhysicsObject **pActiveList = nullptr;
					pActiveList = scratch.Alloc<IPhysicsObject *>( activeCount );
					physenv->GetActiveObjects( pActiveList );

					for ( int i = 0; i < activeCount; i++ )
					{
						CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
						if ( pEntity )
						{
							if ( pEntity->CollisionProp()->DoesVPhysicsInvalidateSurroundingBox() )
							{
								pEntity->CollisionProp()->MarkSurroundingBoundsDirty();
							}
							pEntity->VPhysicsUpdate( pActiveList[i] );
						}
					}
				}

				if( !g_ShadowEntities.empty() )
				{
					CVProfScope _( "PhysFrame VPhysicsShadowUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
					for ( size_t i = 0; i < g_ShadowEntities.size(); i++ )
					{
						CBaseEntity *pEntity = g_ShadowEntities[i];

						IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
						// apply updates
						if ( pPhysics && !pPhysics->IsAsleep() )
						{
							pEntity->VPhysicsShadowUpdate( pPhysics );
						}
					}
				}

				return;
			}
		}

		g_Collisions.BufferTouchEvents( true );
		if( phys_debug_check_contacts.GetBool() && physenv )
		{
			physenv->DebugCheckContacts();
		}
		frametime *= cl_phys_timescale.GetFloat();

		int maxTicks = cl_phys_maxticks.GetInt();
		if ( maxTicks )
		{
			float maxFrameTime = physenv->GetDeltaFrameTime( maxTicks ) - 1e-4f;
			frametime = clamp( frametime, 0, maxFrameTime );
		}

		physenv->Simulate( frametime );

		int activeCount = physenv->GetActiveObjectCount();
		IPhysicsObject **pActiveList = nullptr;
		if ( activeCount )
		{
			pActiveList = scratch.Alloc<IPhysicsObject *>( activeCount );
			physenv->GetActiveObjects( pActiveList );

			for ( int i = 0; i < activeCount; i++ )
			{
				CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
				if ( pEntity )
				{
					//const CCollisionProperty *collProp = pEntity->CollisionProp();
					//debugoverlay->AddBoxOverlay( collProp->GetCollisionOrigin(), collProp->OBBMins(), collProp->OBBMaxs(), collProp->GetCollisionAngles(), 190, 190, 0, 0, 0.01 );

					if ( pEntity->CollisionProp()->DoesVPhysicsInvalidateSurroundingBox() )
					{
						pEntity->CollisionProp()->MarkSurroundingBoundsDirty();
					}
					pEntity->VPhysicsUpdate( pActiveList[i] );
					IPhysicsShadowController *pShadow = pActiveList[i]->GetShadowController();
					if ( pShadow )
					{
						// active shadow object, check for error
						Vector pos, targetPos;
						QAngle rot, targetAngles;
						pShadow->GetTargetPosition( &targetPos, &targetAngles );
						pActiveList[i]->GetPosition( &pos, &rot );
						Vector delta = targetPos - pos;
						float dist = VectorNormalize(delta);
						bool bBlocked = false;
						if ( dist > cl_phys_block_dist.GetFloat() )
						{
							Vector vel;
							pActiveList[i]->GetImplicitVelocity( &vel, nullptr );
							float proj = DotProduct(vel, delta);
							if ( proj < dist * cl_phys_block_fraction.GetFloat() )
							{
								bBlocked = true;
								//Msg("%s was blocked %.3f (%.3f proj)!\n", pEntity->GetClassname(), dist, proj );
							}
						}
						Vector targetAxis;
						float deltaTargetAngle;
						RotationDeltaAxisAngle( rot, targetAngles, targetAxis, deltaTargetAngle );
						if ( std::fabsf(deltaTargetAngle) > 0.5f )
						{
							AngularImpulse angVel;
							pActiveList[i]->GetImplicitVelocity( nullptr, &angVel );
							float proj = DotProduct( angVel, targetAxis ) * Sign(deltaTargetAngle);
							if ( proj < (fabsf(deltaTargetAngle) * cl_phys_block_fraction.GetFloat()) )
							{
								bBlocked = true;
								//Msg("%s was rot blocked %.3f proj %.3f!\n", pEntity->GetClassname(), deltaTargetAngle, proj );
							}
						}
					
						if ( bBlocked )
						{
							CBaseEntity *pBlocker = FindPhysicsBlocker( pActiveList[i] );
							if ( pBlocker )
							{
								if ( IsBlockedShouldDisableCollisions( pEntity ) )
								{
									PhysDisableEntityCollisions( pEntity, pBlocker );
									pActiveList[i]->RecheckContactPoints();
									// GetClassname returns a pointer to the same buffer always!
									//Msg("%s blocked !", pEntity->GetClassname() ); Msg("by %s\n", pBlocker->GetClassname() );
								}
							}
						}
					}
				}
			}
			if ( cl_phys_show_active.GetBool() )
			{
				for ( int i = 0; i < activeCount; i++ )
				{
					CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
					if ( pEntity )
					{
						//debugoverlay->Cross3D( pEntity->GetAbsOrigin(), 12, 255, 0, 0, false, 0 );
						debugoverlay->AddBoxOverlay( pEntity->GetAbsOrigin(), pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->GetAbsAngles(), 255, 255, 0, 8, 0 );
					}
				}
			}
		}

		if( !g_ShadowEntities.empty() )
		{
			CVProfScope _( "PhysFrame VPhysicsShadowUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			for ( size_t i = 0; i < g_ShadowEntities.size(); i++ )
			{
				CBaseEntity *pEntity = g_ShadowEntities[i];
				
				IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
				// apply updates
				if ( pPhysics && !pPhysics->IsAsleep() )
				{
					pEntity->VPhysicsShadowUpdate( pPhysics );
				}
			}
		}

		g_Collisions.BufferTouchEvents( false );
		g_Collisions.FrameUpdate();
	}
	physicssound::PlayImpactSounds( m_impactSounds );
}

void PhysicsSimulate()
{
	g_PhysicsSystem.PhysicsSimulate();
}

void PhysSetPredictionCommandNum( int iCommandNum )
{
	physenv->SetPredictionCommandNum( iCommandNum );
}

CCollisionEvent::CCollisionEvent( void ) 
{ 
	m_flLastSplashTime = 0.0f;
}

void CCollisionEvent::ObjectSound( int index, vcollisionevent_t *pEvent )
{
	IPhysicsObject *pObject = pEvent->pObjects[index];
	if ( !pObject || pObject->IsStatic() )
		return;

	float speed = pEvent->collisionSpeed * pEvent->collisionSpeed;
	int surfaceProps = pEvent->surfaceProps[index];

	void *pGameData = pObject->GetGameData();
		
	if ( pGameData )
	{
		float volume = speed * (1.0f/(320.0f*320.0f));	// max volume at 320 in/s
		
		if ( volume > 1.0f )
			volume = 1.0f;

		if ( surfaceProps >= 0 )
		{
			g_PhysicsSystem.AddImpactSound( pGameData, pObject, surfaceProps, pEvent->surfaceProps[!index], volume, speed );
		}
	}
}

void CCollisionEvent::PostCollision( vcollisionevent_t *pEvent )
{
	CallbackContext callback(this);
	if ( pEvent->deltaCollisionTime > 0.1f && pEvent->collisionSpeed > 70 )
	{
		ObjectSound( 0, pEvent );
		ObjectSound( 1, pEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCollisionEvent::FrameUpdate( void )
{
	UpdateFrictionSounds();
	UpdateTouchEvents();
	UpdateFluidEvents();
	UpdatePenetrateEvents();
}

void CCollisionEvent::LevelShutdown()
{
	m_penetrateEvents.clear();
	m_flLastSplashTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCollisionEvent::UpdateTouchEvents( void )
{
	// Turn on buffering in case new touch events occur during processing
	bool bOldTouchEvents = m_bBufferTouchEvents;
	m_bBufferTouchEvents = true;
	for ( int i = 0; i < std::ssize( m_touchEvents ); i++ )
	{
		const touchevent_t &event = m_touchEvents[i];
		if ( event.touchType == TOUCH_START )
		{
			DispatchStartTouch( event.pEntity0, event.pEntity1, event.endPoint, event.normal );
		}
		else
		{
			// TOUCH_END
			DispatchEndTouch( event.pEntity0, event.pEntity1 );
		}
	}

	m_touchEvents.clear();
	m_bBufferTouchEvents = bOldTouchEvents;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity0 - 
//			*pEntity1 - 
//			touchType - 
//-----------------------------------------------------------------------------
void CCollisionEvent::AddTouchEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1, int touchType, const Vector &point, const Vector &normal )
{
	if ( !pEntity0 || !pEntity1 )
		return;

	int index = std::ssize( m_touchEvents ); m_touchEvents.emplace_back();
	touchevent_t &event = m_touchEvents[index];
	event.pEntity0 = pEntity0;
	event.pEntity1 = pEntity1;
	event.touchType = touchType;
	event.endPoint = point;
	event.normal = normal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject1 - 
//			*pObject2 - 
//			*pTouchData - 
//-----------------------------------------------------------------------------
void CCollisionEvent::StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )
{
	CallbackContext callback(this);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	CBaseEntity *pEntity2 = static_cast<CBaseEntity *>(pObject2->GetGameData());

	if ( !pEntity1 || !pEntity2 )
		return;

	Vector endPoint, normal;
	pTouchData->GetContactPoint( endPoint );
	pTouchData->GetSurfaceNormal( normal );
	if ( !m_bBufferTouchEvents )
	{
		DispatchStartTouch( pEntity1, pEntity2, endPoint, normal );
	}
	else
	{
		AddTouchEvent( pEntity1, pEntity2, TOUCH_START, endPoint, normal );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity0 - 
//			*pEntity1 - 
//-----------------------------------------------------------------------------
void CCollisionEvent::DispatchStartTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1, const Vector &point, const Vector &normal )
{
	trace_t trace;
	memset( &trace, 0, sizeof(trace) );
	trace.endpos = point;
	trace.plane.dist = DotProduct( point, normal );
	trace.plane.normal = normal;

	// NOTE: This sets up the touch list for both entities, no call to pEntity1 is needed
	pEntity0->PhysicsMarkEntitiesAsTouchingEventDriven( pEntity1, trace );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject1 - 
//			*pObject2 - 
//			*pTouchData - 
//-----------------------------------------------------------------------------
void CCollisionEvent::EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )
{
	CallbackContext callback(this);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	CBaseEntity *pEntity2 = static_cast<CBaseEntity *>(pObject2->GetGameData());

	if ( !pEntity1 || !pEntity2 )
		return;

	if ( !m_bBufferTouchEvents )
	{
		DispatchEndTouch( pEntity1, pEntity2 );
	}
	else
	{
		AddTouchEvent( pEntity1, pEntity2, TOUCH_END, vec3_origin, vec3_origin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity0 - 
//			*pEntity1 - 
//-----------------------------------------------------------------------------
void CCollisionEvent::DispatchEndTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	// frees the event-driven touchlinks
	pEntity0->PhysicsNotifyOtherOfUntouch( pEntity0, pEntity1 );
	pEntity1->PhysicsNotifyOtherOfUntouch( pEntity1, pEntity0 );
}

// NOTE: This assumes entity pointers are sorted to simplify search!
void CCollisionEvent::FindOrAddPenetrateEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	int count = std::ssize( m_penetrateEvents );
	for ( int i = 0; i < count; i++ )
	{
		if ( m_penetrateEvents[i].pEntity0 == pEntity0 && m_penetrateEvents[i].pEntity1 == pEntity1 )
		{
			m_penetrateEvents[i].timeStamp = gpGlobals->curtime;
			return;
		}
	}
	int index = std::ssize( m_penetrateEvents ); m_penetrateEvents.emplace_back();
	m_penetrateEvents[index].pEntity0 = pEntity0;
	m_penetrateEvents[index].pEntity1 = pEntity1;
	m_penetrateEvents[index].startTime = gpGlobals->curtime;
	m_penetrateEvents[index].timeStamp = gpGlobals->curtime;
}

// NOTE: This assumes entity pointers are sorted to simplify search!
void CCollisionEvent::UpdatePenetrateEvents()
{
	const float MAX_PENETRATION_TIME = 3.0f;

	for ( int i = std::ssize( m_penetrateEvents )-1; i >= 0; --i )
	{
		float timeSincePenetration = gpGlobals->curtime - m_penetrateEvents[i].timeStamp;
		if ( timeSincePenetration > 0.1f )
		{
			ks::swap_remove( m_penetrateEvents, i );
			continue;
		}
		float timeInPenetration = m_penetrateEvents[i].timeStamp - m_penetrateEvents[i].startTime;
		// it's been too long, just give up and disable collisions
		if ( timeInPenetration > MAX_PENETRATION_TIME )
		{
			PhysDisableEntityCollisions( m_penetrateEvents[i].pEntity0, m_penetrateEvents[i].pEntity1 );
			ks::swap_remove( m_penetrateEvents, i );
			continue;
		}
	}
}

void CCollisionEvent::Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData )
{
	CallbackContext callback(this);
	if ( energy < 0.05f || surfaceProps < 0 )
		return;

	//Get our friction information
	Vector vecPos, vecVel;
	pData->GetContactPoint( vecPos );
	pObject->GetVelocityAtPoint( vecPos, &vecVel );

	CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pObject->GetGameData());
		
	if ( pEntity  )
	{
		if ( pEntity->m_bClientSideRagdoll )
			return;

		friction_t *pFriction = g_Collisions.FindFriction( pEntity );

		if ( (gpGlobals->maxClients > 1) && pFriction && pFriction->pObject) 
		{
			// in MP mode play sound and effects once every 500 msecs,
			// no ongoing updates, takes too much bandwidth
			if ( (pFriction->flLastEffectTime + 0.5f) > gpGlobals->curtime)
			{
				pFriction->flLastUpdateTime = gpGlobals->curtime;
				return; 			
			}
		}

		PhysFrictionSound( pEntity, pObject, energy, surfaceProps, surfacePropsHit );
	}

	PhysFrictionEffect( vecPos, vecVel, energy, surfaceProps, surfacePropsHit );
}

friction_t *CCollisionEvent::FindFriction( CBaseEntity *pObject )
{
	friction_t *pFree = nullptr;

	for ( int i = 0; i < std::size( m_current ); i++ )
	{
		if ( !m_current[i].pObject && !pFree )
			pFree = &m_current[i];

		if ( m_current[i].pObject == pObject )
			return &m_current[i];
	}

	return pFree;
}

void CCollisionEvent::ShutdownFriction( friction_t &friction )
{
//	Msg( "Scrape Stop %s \n", STRING(friction.pObject->m_iClassname) );
	CSoundEnvelopeController::GetController().SoundDestroy( friction.patch );
	friction.patch = nullptr;
	friction.pObject = nullptr;
}

void CCollisionEvent::UpdateFrictionSounds( void )
{
	for ( int i = 0; i < std::size( m_current ); i++ )
	{
		if ( m_current[i].patch )
		{
			if ( m_current[i].flLastUpdateTime < (gpGlobals->curtime-0.1f) )
			{
				// friction wasn't updated the last 100msec, assume fiction finished
				ShutdownFriction( m_current[i] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &matrix - 
//			&normal - 
// Output : static int
//-----------------------------------------------------------------------------
static int BestAxisMatchingNormal( matrix3x4_t &matrix, const Vector &normal )
{
	float bestDot = -1;
	int best = 0;
	for ( int i = 0; i < 3; i++ )
	{
		Vector tmp;
		MatrixGetColumn( matrix, i, tmp );
		float dot = fabs(DotProduct( tmp, normal ));
		if ( dot > bestDot )
		{
			bestDot = dot;
			best = i;
		}
	}

	return best;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFluid - 
//			*pObject - 
//			*pEntity - 
//-----------------------------------------------------------------------------
void PhysicsSplash( IPhysicsFluidController *pFluid, IPhysicsObject *pObject, CBaseEntity *pEntity )
{
	//FIXME: For now just allow ragdolls for E3 - jdw
	if ( ( pObject->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL ) == false )
		return;

	Vector velocity;
	pObject->GetVelocity( &velocity, nullptr );
	
	float impactSpeed = velocity.Length();

	if ( impactSpeed < 25.0f )
		return;

	Vector normal;
	float dist;
	pFluid->GetSurfacePlane( &normal, &dist );

	matrix3x4_t &matrix = pEntity->EntityToWorldTransform();
	
	// Find the local axis that best matches the water surface normal
	int bestAxis = BestAxisMatchingNormal( matrix, normal );

	Vector tangent, binormal;
	MatrixGetColumn( matrix, (bestAxis+1)%3, tangent );
	binormal = CrossProduct( normal, tangent );
	VectorNormalize( binormal );
	tangent = CrossProduct( binormal, normal );
	VectorNormalize( tangent );

	// Now we have a basis tangent to the surface that matches the object's local orientation as well as possible
	// compute an OBB using this basis
	
	// Get object extents in basis
	Vector tanPts[2], binPts[2];
	tanPts[0] = physcollision->CollideGetExtent( pObject->GetCollide(), pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), -tangent );
	tanPts[1] = physcollision->CollideGetExtent( pObject->GetCollide(), pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), tangent );
	binPts[0] = physcollision->CollideGetExtent( pObject->GetCollide(), pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), -binormal );
	binPts[1] = physcollision->CollideGetExtent( pObject->GetCollide(), pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), binormal );

	// now compute the centered bbox
	float mins[2], maxs[2], center[2], extents[2];
	mins[0] = DotProduct( tanPts[0], tangent );
	maxs[0] = DotProduct( tanPts[1], tangent );

	mins[1] = DotProduct( binPts[0], binormal );
	maxs[1] = DotProduct( binPts[1], binormal );

	center[0] = 0.5 * (mins[0] + maxs[0]);
	center[1] = 0.5 * (mins[1] + maxs[1]);

	extents[0] = maxs[0] - center[0];
	extents[1] = maxs[1] - center[1];

	Vector centerPoint = center[0] * tangent + center[1] * binormal + dist * normal;

	Vector axes[2];
	axes[0] = (maxs[0] - center[0]) * tangent;
	axes[1] = (maxs[1] - center[1]) * binormal;

	// visualize OBB hit
	/*
	Vector corner1 = centerPoint - axes[0] - axes[1];
	Vector corner2 = centerPoint + axes[0] - axes[1];
	Vector corner3 = centerPoint + axes[0] + axes[1];
	Vector corner4 = centerPoint - axes[0] + axes[1];
	NDebugOverlay::Line( corner1, corner2, 0, 0, 255, false, 10 );
	NDebugOverlay::Line( corner2, corner3, 0, 0, 255, false, 10 );
	NDebugOverlay::Line( corner3, corner4, 0, 0, 255, false, 10 );
	NDebugOverlay::Line( corner4, corner1, 0, 0, 255, false, 10 );
	*/

	Vector	corner[4];

	corner[0] = centerPoint - axes[0] - axes[1];
	corner[1] = centerPoint + axes[0] - axes[1];
	corner[2] = centerPoint + axes[0] + axes[1];
	corner[3] = centerPoint - axes[0] + axes[1];

	int contents = enginetrace->GetPointContents( centerPoint-Vector(0,0,2), MASK_WATER );

	bool bInSlime = ( contents & CONTENTS_SLIME ) ? true : false;

	Vector	color = vec3_origin;
	float	luminosity = 1.0f;
	
	if ( !bInSlime )
	{
		// Get our lighting information
		FX_GetSplashLighting( centerPoint + ( normal * 8.0f ), &color, &luminosity );
	}

	if ( impactSpeed > 150 )
	{
		if ( bInSlime )
		{
			FX_GunshotSlimeSplash( centerPoint, normal, random_valve->RandomFloat( 8, 10 ) );
		}
		else
		{
			FX_GunshotSplash( centerPoint, normal, random_valve->RandomFloat( 8, 10 ) );
		}
	}
	else if ( !bInSlime )
	{
		FX_WaterRipple( centerPoint, 1.5f, &color, 1.5f, luminosity );
	}
	
	int		splashes = 4;
	Vector	point;

	for ( int i = 0; i < splashes; i++ )
	{
		point = RandomVector( -32.0f, 32.0f );
		point[2] = 0.0f;

		point += corner[i];

		if ( impactSpeed > 150 )
		{
			if ( bInSlime )
			{
				FX_GunshotSlimeSplash( centerPoint, normal, random_valve->RandomFloat( 4, 6 ) );
			}
			else
			{
				FX_GunshotSplash( centerPoint, normal, random_valve->RandomFloat( 4, 6 ) );
			}
		}
		else if ( !bInSlime )
		{
			FX_WaterRipple( point, random_valve->RandomFloat( 0.25f, 0.5f ), &color, luminosity, random_valve->RandomFloat( 0.5f, 1.0f ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCollisionEvent::UpdateFluidEvents( void )
{
	for ( int i = std::ssize( m_fluidEvents )-1; i >= 0; --i )
	{
		if ( (gpGlobals->curtime - m_fluidEvents[i].impactTime) > FLUID_TIME_MAX )
		{
			ks::swap_remove( m_fluidEvents, i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : float
//-----------------------------------------------------------------------------
float CCollisionEvent::DeltaTimeSinceLastFluid( CBaseEntity *pEntity )
{
	for ( int i = std::ssize( m_fluidEvents )-1; i >= 0; --i )
	{
		if ( m_fluidEvents[i].hEntity.Get() == pEntity )
		{
			return gpGlobals->curtime - m_fluidEvents[i].impactTime;
		}
	}

	int index = std::ssize( m_fluidEvents ); m_fluidEvents.emplace_back();
	m_fluidEvents[index].hEntity = pEntity;
	m_fluidEvents[index].impactTime = gpGlobals->curtime;
	return FLUID_TIME_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			*pFluid - 
//-----------------------------------------------------------------------------
void CCollisionEvent::FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid )
{
	CallbackContext callback(this);
	if ( ( pObject == nullptr ) || ( pFluid == nullptr ) )
		return;

	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	
	if ( pEntity )
	{
		float timeSinceLastCollision = DeltaTimeSinceLastFluid( pEntity );
		
		if ( timeSinceLastCollision < 0.5f )
			return;

		// We are generating too many splashes in CStrike15 as well, so enable this
		// prevent too many splashes spawning at once across different entities
		float flGlobalTimeSinceLastSplash = gpGlobals->curtime - m_flLastSplashTime;
		if ( flGlobalTimeSinceLastSplash < 0.1f )
			return;

		//Msg( "ent %d %s doing splash. delta = %f\n", pEntity->entindex(), pEntity->GetModelName(), timeSinceLastCollision );
		PhysicsSplash( pFluid, pObject, pEntity );

		m_flLastSplashTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			*pFluid - 
//-----------------------------------------------------------------------------
void CCollisionEvent::FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid )
{
	CallbackContext callback(this);
	//FIXME: Do nothing for now
}

IPhysicsObject *GetWorldPhysObject ( void )
{
	return g_PhysWorldObject;
}

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, const char *pSoundName, HSOUNDSCRIPTHASH& handle, float flVolume )
{
	if ( !pEntity )
		return;
	
	// cut out the quiet sounds
	// UNDONE: Separate threshold for starting a sound vs. continuing?
	flVolume = clamp( flVolume, 0.0f, 1.0f );
	if ( flVolume > (1.0f/128.0f) )
	{
		friction_t *pFriction = g_Collisions.FindFriction( pEntity );
		if ( !pFriction )
			return;

		CSoundParameters params;
		if ( !CBaseEntity::GetParametersForSound( pSoundName, handle, params, nullptr ) )
			return;

		if ( !pFriction->pObject )
		{
			// don't create really quiet scrapes
			if ( params.volume * flVolume <= 0.1f )
				return;

			pFriction->pObject = pEntity;
			CPASAttenuationFilter filter( pEntity, params.soundlevel );
			int entindex = pEntity->entindex();

			// clientside created entites doesn't have a valid entindex, let 'world' play the sound for them
			if ( entindex < 0 )
				entindex = 0;

			pFriction->patch = CSoundEnvelopeController::GetController().SoundCreate( 
				filter, entindex, CHAN_BODY, pSoundName, params.soundlevel );
			CSoundEnvelopeController::GetController().Play( pFriction->patch, params.volume * flVolume, params.pitch );
		}
		else
		{
			float pitch = (flVolume * (params.pitchhigh - params.pitchlow)) + params.pitchlow;
			CSoundEnvelopeController::GetController().SoundChangeVolume( pFriction->patch, params.volume * flVolume, 0.1f );
			CSoundEnvelopeController::GetController().SoundChangePitch( pFriction->patch, pitch, 0.1f );
		}

		pFriction->flLastUpdateTime = gpGlobals->curtime;
		pFriction->flLastEffectTime = gpGlobals->curtime;
	}
}

void PhysCleanupFrictionSounds( CBaseEntity *pEntity )
{
	friction_t *pFriction = g_Collisions.FindFriction( pEntity );
	if ( pFriction && pFriction->patch )
	{
		g_Collisions.ShutdownFriction( *pFriction );
	}
}

float PhysGetNextSimTime()
{
	return physenv->GetSimulationTime() + gpGlobals->frametime * cl_phys_timescale.GetFloat();
}

float PhysGetSyncCreateTime()
{
	float nextTime = physenv->GetNextFrameTime();
	float simTime = PhysGetNextSimTime();
	if ( nextTime < simTime )
	{
		// The next simulation frame begins before the end of this frame
		// so create physics objects at that time so that they will reach the current
		// position at curtime.  Otherwise the physics object will simulate forward from curtime
		// and pop into the future a bit at this point of transition
		return gpGlobals->curtime + nextTime - simTime;
	}
	return gpGlobals->curtime;
}

void VPhysicsShadowDataChanged( bool bCreate, CBaseEntity *pEntity )
{
	// client-side vphysics shadow management
	if ( bCreate && !pEntity->VPhysicsGetObject() && !(pEntity->GetSolidFlags() & FSOLID_NOT_SOLID) )
	{
		if ( pEntity->GetSolid() != SOLID_BSP )
		{
			pEntity->SetSolid(SOLID_VPHYSICS);
		}
		if ( pEntity->GetSolidFlags() & FSOLID_NOT_MOVEABLE )
		{
			pEntity->VPhysicsInitStatic();
		}
		else
		{
			pEntity->VPhysicsInitShadow( false, false );
		}
	}
	else if ( pEntity->VPhysicsGetObject() && !pEntity->VPhysicsGetObject()->IsStatic() )
	{
		float interpTime = pEntity->GetInterpolationAmount(LATCH_SIMULATION_VAR);
		// this is the client time the network origin will become the entity's render origin
		float schedTime = pEntity->m_flSimulationTime + interpTime;
		// how far is that from now
		float deltaTime = schedTime - gpGlobals->curtime;
		// Compute that time on the client vphysics clock
		float physTime = physenv->GetSimulationTime() + deltaTime + gpGlobals->frametime;
		// arrival time is relative to the next tick
		float arrivalTime = physTime - physenv->GetNextFrameTime();
		if ( arrivalTime < 0 )
			arrivalTime = 0;
		pEntity->VPhysicsGetObject()->UpdateShadow( pEntity->GetNetworkOrigin(), pEntity->GetNetworkAngles(), false, arrivalTime );
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// helper method for trace hull as used by physics...
//-----------------------------------------------------------------------------
static void Physics_TraceHull( CBaseEntity* pBaseEntity, const Vector &vecStart,
	const Vector &vecEnd, const Vector &hullMin, const Vector &hullMax,	
	unsigned int mask, trace_t *ptr )
{
	// FIXME: I really am not sure the best way of doing this
	// The TraceHull code below for shots will make sure the object passes
	// through shields which do not block that damage type. It will also 
	// send messages to the shields that they've been hit.
	{
		UTIL_TraceHull( vecStart, vecEnd, hullMin, hullMax, mask, 
			pBaseEntity, pBaseEntity->GetCollisionGroup(), ptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Does not change the entities velocity at all
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckSweep( const Vector& vecAbsStart, const Vector &vecAbsDelta, trace_t *pTrace )
{
	unsigned int mask = PhysicsSolidMaskForEntity();

	Vector vecAbsEnd;
	VectorAdd( vecAbsStart, vecAbsDelta, vecAbsEnd );

	// Set collision type
	if ( !IsSolid() || IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
	{
		// don't collide with monsters
		mask &= ~CONTENTS_MONSTER;
	}

	Physics_TraceHull( this, vecAbsStart, vecAbsEnd, WorldAlignMins(), WorldAlignMaxs(), mask, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsPushEntity( const Vector& push, trace_t *pTrace )
{
/*
	if ( m_pMoveParent )
	{
		Warning( "pushing entity (%s) that has m_pMoveParent!\n", STRING( pev->classname ) );
		Assert(0);
	}
*/

	// NOTE: absorigin and origin must be equal because there is no moveparent
	Vector prevOrigin;
	VectorCopy( GetAbsOrigin(), prevOrigin );

	trace_t		trace;
	PhysicsCheckSweep( prevOrigin, push, pTrace );

	if ( pTrace->fraction )
	{
		SetAbsOrigin( pTrace->endpos );
	}

	// CLIENT DLL HACKS
	m_vecNetworkOrigin = GetLocalOrigin();
	m_angNetworkAngles = GetLocalAngles();

//	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED );

	if ( pTrace->Ent<CBaseEntity>() )
	{
		PhysicsImpact( pTrace->Ent<CBaseEntity>(), *pTrace );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pNewPosition - 
//			*pNewVelocity - 
//			*pNewAngles - 
//			*pNewAngVelocity - 
//-----------------------------------------------------------------------------
void CBaseEntity::PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	// If you're going to use custom physics, you need to implement this!
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCustom()
{
	PhysicsCheckWater();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	// Moving upward, off the ground, or  resting on something that isn't ground
	if ( m_vecVelocity[2] > 0 || !GetGroundEntity() || !GetGroundEntity()->IsStandable() )
	{
		SetGroundEntity( nullptr );
	}

	// NOTE: The entity must set the position, angles, velocity in its custom movement
	Vector vecNewPosition = GetAbsOrigin();

	if ( vecNewPosition == vec3_origin )
	{
		// Shouldn't be at world origin
		contract_assert( 0 );
	}

	Vector vecNewVelocity = m_vecVelocity;
	QAngle angNewAngles = GetAbsAngles();
	QAngle angNewAngVelocity = m_vecAngVelocity;

	PerformCustomPhysics( &vecNewPosition, &vecNewVelocity, &angNewAngles, &angNewAngVelocity );

	// Store off all of the new state information...
	m_vecVelocity = vecNewVelocity;
	SetAbsAngles( angNewAngles );
	m_vecAngVelocity = angNewAngVelocity;

	Vector move;
	VectorSubtract( vecNewPosition, GetAbsOrigin(), move );

	// move origin
	trace_t trace;
	PhysicsPushEntity( move, &trace );

	PhysicsCheckVelocity();

	if (trace.allsolid)
	{	
		// entity is trapped in another solid
		// UNDONE: does this entity needs to be removed?
		m_vecVelocity = vec3_origin;
		VectorCopy (vec3_angle, m_vecAngVelocity);
		return;
	}
	
	// check for in water
	PhysicsCheckWaterTransition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStep()
{
	// Run all but the base think function
	PhysicsRunThink( THINK_FIRE_ALL_BUT_BASE );
PhysicsRunThink( THINK_FIRE_BASE_ONLY );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNoclip( void )
{
	PhysicsRunThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNone( void )
{
	PhysicsRunThink();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsPusher( void )
{
	PhysicsRunThink();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsParent( void )
{
	PhysicsRunThink();
}

//-----------------------------------------------------------------------------
// Purpose: Not yet supported on client .dll
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseEntity::StartTouch( CBaseEntity *pOther )
{
	// notify parent
//	if ( m_pParent != NULL )
//		m_pParent->StartTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Call touch function if one is set
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseEntity::Touch( CBaseEntity *pOther )
{ 
	if ( m_pfnTouch ) 
		(this->*m_pfnTouch)( pOther );

	// notify parent of touch
//	if ( m_pParent != NULL )
//		m_pParent->Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Call end touch
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseEntity::EndTouch( CBaseEntity *pOther )
{
	// notify parent
//	if ( m_pParent != NULL )
//	{
//		m_pParent->EndTouch( pOther );
//	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : check - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetCheckUntouch( bool check )
{
	// Invalidate touchstamp
	if ( check )
	{
		touchStamp++;
		if ( !IsEFlagSet( EFL_CHECK_UNTOUCH ) )
		{
			AddEFlags( EFL_CHECK_UNTOUCH );
		}
	}
	else
	{
		RemoveEFlags( EFL_CHECK_UNTOUCH );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::GetCheckUntouch() const
{
	return IsEFlagSet( EFL_CHECK_UNTOUCH );
}

extern ConVar think_limit;

//-----------------------------------------------------------------------------
// Purpose: Called when it's time for a physically moved objects (plats, doors, etc)
//			to run it's game code.
//			All other entity thinking is done during worldspawn's think
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsDispatchThink( BASEPTR thinkFunc )
{
	float thinkLimit = think_limit.GetFloat();
	double startTime = 0.0;

	/*
	// This doesn't apply on the client, really
	if ( IsDormant() )
	{
		Warning( "Dormant entity %s is thinking!!\n", GetClassname() );
		Assert(0);
	}
	*/

	if ( thinkLimit )
	{
		startTime = Plat_FloatTime();
	}
	
	if ( thinkFunc )
	{
		(this->*thinkFunc)();
	}

	if ( thinkLimit )
	{
		// calculate running time of the AI in milliseconds
		float time = ( Plat_FloatTime() - startTime ) * 1000.0f;
		if ( time > thinkLimit )
		{
			{
				Msg( "CLIENT:  %s(%s) thinking for %.02f ms!!!\n", GetClassname(), typeid(this).name(), time );				
			}
		}
	}
}

class CBaseEntity;
struct groundlink_t;
struct touchlink_t;

using namespace ::ks::reflect;

// memory pool for storing links between entities
static ks::fixed_pool g_EdictTouchLinks( sizeof(touchlink_t), MAX_EDICTS, ks::fixed_pool::grow::none, "g_EdictTouchLinks", alignof(touchlink_t) );
static ks::fixed_pool g_EntityGroundLinks( sizeof( groundlink_t ), MAX_EDICTS, ks::fixed_pool::grow::none, "g_EntityGroundLinks", alignof(groundlink_t) );

struct watcher_t
{
	EHANDLE				hWatcher;
	IWatcherCallback	*pWatcherCallback;
};

class CWatcherList
{
public:
	void NotifyPositionChanged( CBaseEntity *pEntity );
	void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake );

	void AddToList( CBaseEntity *pWatcher );
	void RemoveWatcher( CBaseEntity *pWatcher );

private:
	int GetCallbackObjects( IWatcherCallback **pList, int listMax );

	std::vector<watcher_t>::iterator Find( CBaseEntity *pEntity );
	std::vector<watcher_t> m_list;
};

int linksallocated = 0;
int groundlinksallocated = 0;

// Prints warnings if any entity think functions take longer than this many milliseconds
#define DEF_THINK_LIMIT "10"

ConVar think_limit( side_registry(), "think_limit", DEF_THINK_LIMIT, FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum think time in milliseconds, warning is printed if this is exceeded." );

#define DebugTouchlinks() false

ConVar sv_grenade_trajectory(side_registry(), "sv_grenade_trajectory", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_CHEAT, "Shows grenade trajectory visualization in-game." );
ConVar sv_grenade_trajectory_time(side_registry(), "sv_grenade_trajectory_time", "20", FCVAR_REPLICATED | FCVAR_RELEASE, "Length of time grenade trajectory remains visible.", true, 0.1f, true, 20.0f );
ConVar sv_grenade_trajectory_time_spectator(side_registry(), "sv_grenade_trajectory_time_spectator", "4", FCVAR_REPLICATED | FCVAR_RELEASE, "Length of time grenade trajectory remains visible as a spectator.", true, 0.0f, true, 8.0f );
ConVar sv_grenade_trajectory_thickness(side_registry(), "sv_grenade_trajectory_thickness", "0.2", FCVAR_REPLICATED | FCVAR_RELEASE, "Visible thickness of grenade trajectory arc", true, 0.1f, true, 1.0f );
ConVar sv_grenade_trajectory_dash(side_registry(), "sv_grenade_trajectory_dash", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Dot-dash style grenade trajectory arc" );

//-----------------------------------------------------------------------------
// Portal-specific hack designed to eliminate re-entrancy in touch functions
//-----------------------------------------------------------------------------
class CPortalTouchScope
{
public:
	CPortalTouchScope();
	~CPortalTouchScope();

public:
	static int m_nDepth;
	static CCallQueue m_CallQueue;	
};

int CPortalTouchScope::m_nDepth = 0;
CCallQueue CPortalTouchScope::m_CallQueue;	

CCallQueue *GetPortalCallQueue()
{
	return ( CPortalTouchScope::m_nDepth > 0 ) ? &CPortalTouchScope::m_CallQueue : nullptr;
}

CPortalTouchScope::CPortalTouchScope()
{
	++m_nDepth;
}

CPortalTouchScope::~CPortalTouchScope()
{
	contract_assert( m_nDepth >= 1 );
	if ( --m_nDepth == 0 )
	{
		m_CallQueue.CallQueued();
	}
}

//-----------------------------------------------------------------------------
// Purpose: System for hanging objects off of CBaseEntity, etc.
//  Externalized data objects ( see sharreddefs.h for enum )
//-----------------------------------------------------------------------------
class CDataObjectAccessSystem : public CAutoGameSystem
{
public:

	enum
	{
		MAX_ACCESSORS = 32,
	};

	CDataObjectAccessSystem()
	{
    	static_assert( (int)NUM_DATAOBJECT_TYPES <= (int)MAX_ACCESSORS );

		V_memset( m_Accessors, 0, sizeof( m_Accessors ) );
	}

	virtual bool Init()
	{
		AddDataAccessor( TOUCHLINK, new CEntityDataInstantiator< touchlink_t > );
		AddDataAccessor( GROUNDLINK, new CEntityDataInstantiator< groundlink_t > );
		AddDataAccessor( STEPSIMULATION, new CEntityDataInstantiator< StepSimulationData > );
		AddDataAccessor( MODELSCALE, new CEntityDataInstantiator< ModelScale > );
		AddDataAccessor( POSITIONWATCHER, new CEntityDataInstantiator< CWatcherList > );
		AddDataAccessor( PHYSICSPUSHLIST, new CEntityDataInstantiator< physicspushlist_t > );
		AddDataAccessor( VPHYSICSUPDATEAI, new CEntityDataInstantiator< vphysicsupdateai_t > );
		AddDataAccessor( VPHYSICSWATCHER, new CEntityDataInstantiator< CWatcherList > );
		
		return true;
	}

	virtual void Shutdown()
	{
		for ( int i = 0; i < MAX_ACCESSORS; i++ )
		{
			delete m_Accessors[ i ];
			m_Accessors[ i ]  = 0;
		}
	}

	void *GetDataObject( int type, const CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return nullptr;
		}
		return m_Accessors[ type ]->GetDataObject( instance );
	}

	void *CreateDataObject( int type, CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return nullptr;
		}

		return m_Accessors[ type ]->CreateDataObject( instance );
	}

	void DestroyDataObject( int type, CBaseEntity *instance )
	{
		if ( !IsValidType( type ) )
		{
			contract_assert( !"Bogus type" );
			return;
		}

		m_Accessors[ type ]->DestroyDataObject( instance );
	}

private:

	bool IsValidType( int type ) const
	{
		if ( type < 0 || type >= MAX_ACCESSORS )
			return false;

		if ( m_Accessors[ type ] == nullptr )
			return false;
		return true;
	}

	void AddDataAccessor( int type, IEntityDataInstantiator *instantiator )
	{
		if ( type < 0 || type >= MAX_ACCESSORS )
		{
			contract_assert( !"AddDataAccessor with out of range type!!!\n" );
			return;
		}

		contract_assert( instantiator );

		if ( m_Accessors[ type ] != nullptr )
		{
			contract_assert( !"AddDataAccessor, duplicate adds!!!\n" );
			return;
		}

		m_Accessors[ type ] = instantiator;
	}

	IEntityDataInstantiator *m_Accessors[ MAX_ACCESSORS ];
};

static CDataObjectAccessSystem g_DataObjectAccessSystem;

bool CBaseEntity::HasDataObjectType( int type ) const
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	return ( m_fDataObjectTypes	& (1<<type) ) ? true : false;
}

void CBaseEntity::AddDataObjectType( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	m_fDataObjectTypes |= (1<<type);
}

void CBaseEntity::RemoveDataObjectType( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	m_fDataObjectTypes &= ~(1<<type);
}

void *CBaseEntity::GetDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	if ( !HasDataObjectType( type ) )
		return nullptr;
	return g_DataObjectAccessSystem.GetDataObject( type, this );
}

void *CBaseEntity::CreateDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	AddDataObjectType( type );
	return g_DataObjectAccessSystem.CreateDataObject( type, this );
}

void CBaseEntity::DestroyDataObject( int type )
{
	contract_assert( type >= 0 && type < NUM_DATAOBJECT_TYPES );
	if ( !HasDataObjectType( type ) )
		return;
	g_DataObjectAccessSystem.DestroyDataObject( type, this );
	RemoveDataObjectType( type );
}

int CWatcherList::GetCallbackObjects( IWatcherCallback **pList, int listMax )
{
	int index = 0;
	for ( auto it = m_list.begin(); it != m_list.end(); )
	{
		if ( it->hWatcher.Get() )
		{
			pList[index] = it->pWatcherCallback;
			index++;
			if ( index >= listMax )
			{
				contract_assert( 0 );
				return index;
			}
			++it;
		}
		else
		{
			it = m_list.erase( it );
		}
	}
	return index;
}

void CWatcherList::NotifyPositionChanged( CBaseEntity *pEntity )
{
	IWatcherCallback *pCallbacks[1024]; // HACKHACK: Assumes this list is big enough
	int count = GetCallbackObjects( pCallbacks, std::size( pCallbacks ) );
	for ( int i = 0; i < count; i++ )
	{
		IPositionWatcher *pWatcher = static_cast<IPositionWatcher *>(pCallbacks[i]);
		if ( pWatcher )
		{
			pWatcher->NotifyPositionChanged(pEntity);
		}
	}
}

void CWatcherList::NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
{
	IWatcherCallback *pCallbacks[1024];	// HACKHACK: Assumes this list is big enough!
	int count = GetCallbackObjects( pCallbacks, std::size( pCallbacks ) );
	for ( int i = 0; i < count; i++ )
	{
		IVPhysicsWatcher *pWatcher = static_cast<IVPhysicsWatcher *>(pCallbacks[i]);
		if ( pWatcher )
		{
			pWatcher->NotifyVPhysicsStateChanged(pPhysics, pEntity, bAwake);
		}
	}
}

std::vector<watcher_t>::iterator CWatcherList::Find( CBaseEntity *pEntity )
{
	return std::ranges::find_if( m_list, [pEntity]( const watcher_t &w ) { return w.hWatcher.Get() == pEntity; } );
}

void CWatcherList::RemoveWatcher( CBaseEntity *pEntity )
{
	auto it = Find( pEntity );
	if ( it != m_list.end() )
	{
		m_list.erase( it );
	}
}

void CWatcherList::AddToList( CBaseEntity *pWatcher )
{
	if ( Find( pWatcher ) == m_list.end() )
	{
		watcher_t watcher;
		watcher.hWatcher = pWatcher;
			// save this separately so we can use the EHANDLE to test for deletion
		watcher.pWatcherCallback = dynamic_cast<IWatcherCallback *> (pWatcher);

		if ( watcher.pWatcherCallback )
		{
			m_list.push_back( watcher );
		}
	}
}

static void AddWatcherToEntity( CBaseEntity *pWatcher, CBaseEntity *pEntity, int watcherType )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(watcherType);
	if ( !pList )
	{
		pList = ( CWatcherList * )pEntity->CreateDataObject( watcherType );
	}

	pList->AddToList( pWatcher );
}

static void RemoveWatcherFromEntity( CBaseEntity *pWatcher, CBaseEntity *pEntity, int watcherType )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(watcherType);
	if ( pList )
	{
		pList->RemoveWatcher( pWatcher );
	}
}

void WatchPositionChanges( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
{
	AddWatcherToEntity( pWatcher, pMovingEntity, POSITIONWATCHER );
}

void RemovePositionWatcher( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
{
	RemoveWatcherFromEntity( pWatcher, pMovingEntity, POSITIONWATCHER );
}

void ReportPositionChanged( CBaseEntity *pMovedEntity )
{
	CWatcherList *pList = (CWatcherList *)pMovedEntity->GetDataObject(POSITIONWATCHER);
	if ( pList )
	{
		pList->NotifyPositionChanged( pMovedEntity );
	}
}

void WatchVPhysicsStateChanges( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
{
	AddWatcherToEntity( pWatcher, pPhysicsEntity, VPHYSICSWATCHER );
}

void RemoveVPhysicsStateWatcher( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
{
	AddWatcherToEntity( pWatcher, pPhysicsEntity, VPHYSICSWATCHER );
}

void ReportVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
{
	CWatcherList *pList = (CWatcherList *)pEntity->GetDataObject(VPHYSICSWATCHER);
	if ( pList )
	{
		pList->NotifyVPhysicsStateChanged( pPhysics, pEntity, bAwake );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::DestroyAllDataObjects( void )
{
	int i;
	for ( i = 0; i < NUM_DATAOBJECT_TYPES; i++ )
	{
		if ( HasDataObjectType( i ) )
		{
			DestroyDataObject( i );
		}
	}
}

//-----------------------------------------------------------------------------
// For debugging
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Returns the actual gravity
//-----------------------------------------------------------------------------
static inline float GetActualGravity( CBaseEntity *pEnt )
{
	float ent_gravity = pEnt->GetGravity();
	if ( ent_gravity == 0.0f )
	{
		ent_gravity = 1.0f;
	}

	return ent_gravity * sv_gravity.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline touchlink_t
//-----------------------------------------------------------------------------
inline touchlink_t *AllocTouchLink( void )
{
	touchlink_t *link = (touchlink_t*)g_EdictTouchLinks.alloc( sizeof(touchlink_t) );
	if ( link )
	{
		++linksallocated;
	}
	else
	{
		DevWarning( "AllocTouchLink: failed to allocate touchlink_t.\n" );
	}

	return link;
}

static touchlink_t *g_pNextLink = nullptr;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *link - 
// Output : inline void
//-----------------------------------------------------------------------------
inline void FreeTouchLink( touchlink_t *link )
{
	if ( link )
	{
		if ( link == g_pNextLink )
		{
			g_pNextLink = link->nextLink;
		}
		--linksallocated;
		link->prevLink = link->nextLink = nullptr;
	}

	// Necessary to catch crashes
	g_EdictTouchLinks.free( link );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline groundlink_t
//-----------------------------------------------------------------------------
inline groundlink_t *AllocGroundLink( void )
{
	groundlink_t *link = (groundlink_t*)g_EntityGroundLinks.alloc( sizeof(groundlink_t) );
	if ( link )
	{
		++groundlinksallocated;
	}
	else
	{
		DevMsg( "AllocGroundLink: failed to allocate groundlink_t.!!!\n" );
	}

	return link;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *link - 
// Output : inline void
//-----------------------------------------------------------------------------
inline void FreeGroundLink( groundlink_t *link )
{
	if ( link )
	{
		--groundlinksallocated;
	}

	g_EntityGroundLinks.free( link );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsCurrentlyTouching( void ) const
{
	if ( HasDataObjectType( TOUCHLINK ) )
	{
		return true;
	}

	return false;
}

static bool g_bCleanupDatObject = true;

//-----------------------------------------------------------------------------
// Purpose: Checks to see if any entities that have been touching this one
//			have stopped touching it, and notify the entity if so.
//			Called at the end of a frame, after all the entities have run
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckForEntityUntouch( void )
{
	contract_assert( g_pNextLink == nullptr );

	touchlink_t *link;

	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		bool saveCleanup = g_bCleanupDatObject;
		g_bCleanupDatObject = false;

		link = root->nextLink;
		while ( link != root )
		{
			g_pNextLink = link->nextLink;

			// these touchlinks are not polled.  The ents are touching due to an outside
			// system that will add/delete them as necessary (vphysics in this case)
			if ( link->touchStamp == TOUCHSTAMP_EVENT_DRIVEN )
			{
				// refresh the touch call
				PhysicsTouch( link->entityTouched );
			}
			else
			{    
				// check to see if the touch stamp is up to date
				if ( link->touchStamp != touchStamp )
				{
					// stamp is out of data, so entities are no longer touching
					// remove self from other entities touch list
					PhysicsNotifyOtherOfUntouch( this, link->entityTouched );

					// remove other entity from this list
					PhysicsRemoveToucher( this, link );
				}
			}

			link = g_pNextLink;
		}

		g_bCleanupDatObject = saveCleanup;

		// Nothing left in list, destroy root
		if ( root->nextLink == root &&
			 root->prevLink == root )
		{
			DestroyDataObject( TOUCHLINK );
		}
	}

	g_pNextLink = nullptr;

	SetCheckUntouch( false );
}

//-----------------------------------------------------------------------------
// Purpose: notifies an entity than another touching entity has moved out of contact.
// Input  : *other - the entity to be acted upon
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNotifyOtherOfUntouch( CBaseEntity *ent, CBaseEntity *other )
{
	if ( !other )
		return;

	// loop through ed's touch list, looking for the notifier
	// remove and call untouch if found
	touchlink_t *root = ( touchlink_t * )other->GetDataObject( TOUCHLINK );
	if ( root )
	{
		touchlink_t *link = root->nextLink;
		while ( link != root )
		{
			if ( link->entityTouched == ent )
			{
				PhysicsRemoveToucher( other, link );

				// Check for complete removal
				if ( g_bCleanupDatObject &&
					 root->nextLink == root && 
					 root->prevLink == root )
				{
					other->DestroyDataObject( TOUCHLINK );
				}
				return;
			}

			link = link->nextLink;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: removes a toucher from the list
// Input  : *link - the link to remove
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveToucher( CBaseEntity *otherEntity, touchlink_t *link )
{
	// Every start Touch gets a corresponding end touch
	if ( (link->flags & FTOUCHLINK_START_TOUCH) && 
		link->entityTouched != nullptr &&
		otherEntity != nullptr )
	{
		otherEntity->EndTouch( link->entityTouched );
	}

	link->nextLink->prevLink = link->prevLink;
	link->prevLink->nextLink = link->nextLink;

	if ( DebugTouchlinks() )
		Msg( "remove 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, link->entityTouched->GetDebugName(), otherEntity->GetDebugName(), link->entityTouched->entindex(), otherEntity->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
	FreeTouchLink( link );
}

//-----------------------------------------------------------------------------
// Purpose: Clears all touches from the list
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveTouchedList( CBaseEntity *ent )
{

	touchlink_t *link, *nextLink;

	touchlink_t *root = ( touchlink_t * )ent->GetDataObject( TOUCHLINK );
	if ( root )
	{
		link = root->nextLink;
		bool saveCleanup = g_bCleanupDatObject;
		g_bCleanupDatObject = false;
		while ( link && link != root )
		{
			nextLink = link->nextLink;

			// notify the other entity that this ent has gone away
			PhysicsNotifyOtherOfUntouch( ent, link->entityTouched );

			// kill it
			if ( DebugTouchlinks() )
				Msg( "remove 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, ent->GetDebugName(), link->entityTouched->GetDebugName(), ent->entindex(), link->entityTouched->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
			FreeTouchLink( link );
			link = nextLink;
		}

		g_bCleanupDatObject = saveCleanup;
		ent->DestroyDataObject( TOUCHLINK );
	}

	ent->touchStamp = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *other - 
// Output : groundlink_t
//-----------------------------------------------------------------------------
groundlink_t *CBaseEntity::AddEntityToGroundList( CBaseEntity *other )
{
	groundlink_t *link;

	if ( this == other )
		return nullptr;

	if ( other->IsMarkedForDeletion() )
	{
		return nullptr;
	}

	// check if the edict is already in the list
	groundlink_t *root = ( groundlink_t * )GetDataObject( GROUNDLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			if ( link->entity == other )
			{
				// no more to do
				return link;
			}
		}
	}
	else
	{
		root = ( groundlink_t * )CreateDataObject( GROUNDLINK );
		root->prevLink = root->nextLink = root;
	}

	// entity is not in list, so it's a new touch
	// add it to the touched list and then call the touch function

	// build new link
	link = AllocGroundLink();
	if ( !link )
		return nullptr;

	link->entity = other;
	// add it to the list
	link->nextLink = root->nextLink;
	link->prevLink = root;
	link->prevLink->nextLink = link;
	link->nextLink->prevLink = link;

	PhysicsStartGroundContact( other );

	return link;
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever two entities come in contact
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStartGroundContact( CBaseEntity *pentOther )
{
	if ( !pentOther )
		return;

	if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
	{
		pentOther->StartGroundContact( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: notifies an entity than another touching entity has moved out of contact.
// Input  : *other - the entity to be acted upon
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNotifyOtherOfGroundRemoval( CBaseEntity *ent, CBaseEntity *other )
{
	if ( !other )
		return;

	// loop through ed's touch list, looking for the notifier
	// remove and call untouch if found
	groundlink_t *root = ( groundlink_t * )other->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			if ( link->entity == ent )
			{
				PhysicsRemoveGround( other, link );

				if ( root->nextLink == root && 
					 root->prevLink == root )
				{
					other->DestroyDataObject( GROUNDLINK );
				}
				return;
			}

			link = link->nextLink;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: removes a toucher from the list
// Input  : *link - the link to remove
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveGround( CBaseEntity *other, groundlink_t *link )
{
	// Every start Touch gets a corresponding end touch
	if ( link->entity != nullptr )
	{
		CBaseEntity *linkEntity = link->entity;
		CBaseEntity *otherEntity = other;
		if ( linkEntity && otherEntity )
		{
			linkEntity->EndGroundContact( otherEntity );
		}
	}

	link->nextLink->prevLink = link->prevLink;
	link->prevLink->nextLink = link->nextLink;
	FreeGroundLink( link );
}

//-----------------------------------------------------------------------------
// Purpose: static method to remove ground list for an entity
// Input  : *ent - 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRemoveGroundList( CBaseEntity *ent )
{
	groundlink_t *link, *nextLink;

	groundlink_t *root = ( groundlink_t * )ent->GetDataObject( GROUNDLINK );
	if ( root )
	{
		link = root->nextLink;
		while ( link && link != root )
		{
			nextLink = link->nextLink;

			// notify the other entity that this ent has gone away
			PhysicsNotifyOtherOfGroundRemoval( ent, link->entity );

			// kill it
			FreeGroundLink( link );

			link = nextLink;
		}

		ent->DestroyDataObject( GROUNDLINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame that two entities are touching
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsTouch( CBaseEntity *pentOther )
{
	if ( pentOther )
	{
		if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
		{
			Touch( pentOther );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever two entities come in contact
// Input  : *pentOther - the entity who it has touched
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStartTouch( CBaseEntity *pentOther )
{
	if ( pentOther )
	{
		if ( !(IsMarkedForDeletion() || pentOther->IsMarkedForDeletion()) )
		{
			StartTouch( pentOther );
			Touch( pentOther );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Marks in an entity that it is touching another entity, and calls
//			it's Touch() function if it is a new touch.
//			Stamps the touch link with the new time so that when we check for
//			untouch we know things haven't changed.
// Input  : *other - entity that it is in contact with
//-----------------------------------------------------------------------------
touchlink_t *CBaseEntity::PhysicsMarkEntityAsTouched( CBaseEntity *other )
{
	touchlink_t *link;

	if ( this == other )
		return nullptr;

	// Entities in hierarchy should not interact
	if ( (this->GetMoveParent() == other) || (this == other->GetMoveParent()) )
		return nullptr;

	// check if either entity doesn't generate touch functions
	if ( (GetFlags() | other->GetFlags()) & FL_DONTTOUCH )
		return nullptr;

	// Pure triggers should not touch each other
	if ( IsSolidFlagSet( FSOLID_TRIGGER ) && other->IsSolidFlagSet( FSOLID_TRIGGER ) )
	{
		if (!IsSolid() && !other->IsSolid())
			return nullptr;
	}

	// Don't do touching if marked for deletion
	if ( other->IsMarkedForDeletion() )
	{
		return nullptr;
	}

	if ( IsMarkedForDeletion() )
	{
		return nullptr;
	}

	// check if the edict is already in the list
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			if ( link->entityTouched == other )
			{
				// update stamp
				link->touchStamp = touchStamp;
				
				if ( !CBaseEntity::sm_bDisableTouchFuncs )
				{
					PhysicsTouch( other );
				}

				// no more to do
				return link;
			}
		}
	}
	else
	{
		// Allocate the root object
		root = ( touchlink_t * )CreateDataObject( TOUCHLINK );
		root->nextLink = root->prevLink = root;
	}

	// entity is not in list, so it's a new touch
	// add it to the touched list and then call the touch function

	// build new link
	link = AllocTouchLink();
	if ( DebugTouchlinks() )
		Msg( "add 0x%p: %s-%s (%d-%d) [%d in play, %d max]\n", link, GetDebugName(), other->GetDebugName(), entindex(), other->entindex(), linksallocated, g_EdictTouchLinks.peak_count() );
	if ( !link )
		return nullptr;

	link->touchStamp = touchStamp;
	link->entityTouched = other;
	link->flags = 0;
	// add it to the list
	link->nextLink = root->nextLink;
	link->prevLink = root;
	link->prevLink->nextLink = link;
	link->nextLink->prevLink = link;

	// non-solid entities don't get touched
	bool bShouldTouch = (IsSolid() && !IsSolidFlagSet(FSOLID_VOLUME_CONTENTS)) || IsSolidFlagSet(FSOLID_TRIGGER);
	if ( bShouldTouch && !other->IsSolidFlagSet(FSOLID_TRIGGER) )
	{
		link->flags |= FTOUCHLINK_START_TOUCH;
		if ( !CBaseEntity::sm_bDisableTouchFuncs )
		{
			PhysicsStartTouch( other );
		}
	}

	return link;
}

static trace_t g_TouchTrace;
const trace_t &CBaseEntity::GetTouchTrace( void )
{
	return g_TouchTrace;
}

//-----------------------------------------------------------------------------
// Purpose: Marks the fact that two edicts are in contact
// Input  : *other - other entity
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsMarkEntitiesAsTouching( CBaseEntity *other, trace_t &trace )
{
	g_TouchTrace = trace;
	touchlink_t *pLink0 = PhysicsMarkEntityAsTouched( other );
	touchlink_t *pLInk1 = other->PhysicsMarkEntityAsTouched( this );

	if ( pLink0 && !pLInk1 )
	{
		PhysicsNotifyOtherOfUntouch( other, this );
	}
	if ( pLInk1 && !pLink0 )
	{
		PhysicsNotifyOtherOfUntouch( this, other );
	}
	UTIL_ClearTrace( g_TouchTrace );
}

void CBaseEntity::PhysicsMarkEntitiesAsTouchingEventDriven( CBaseEntity *other, trace_t &trace )
{
	g_TouchTrace = trace;
	g_TouchTrace.m_pEnt = other;

	touchlink_t *link;
	link = this->PhysicsMarkEntityAsTouched( other );
	if ( link )
	{
		// mark these links as event driven so they aren't untouched the next frame
		// when the physics doesn't refresh them
		link->touchStamp = TOUCHSTAMP_EVENT_DRIVEN;
	}
	g_TouchTrace.m_pEnt = this;
	link = other->PhysicsMarkEntityAsTouched( this );
	if ( link )
	{
		link->touchStamp = TOUCHSTAMP_EVENT_DRIVEN;
	}
	UTIL_ClearTrace( g_TouchTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Two entities have touched, so run their touch functions
// Input  : *other - 
//			*ptrace - 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsImpact( CBaseEntity *other, trace_t &trace )
{
	if ( !other )
	{
		return;
	}

	// If either of the entities is flagged to be deleted, 
	//  don't call the touch functions
	if ( ( GetFlags() | other->GetFlags() ) & FL_KILLME )
	{
		return;
	}

	PhysicsMarkEntitiesAsTouching( other, trace );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the mask of what is solid for the given entity
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseEntity::PhysicsSolidMaskForEntity( void ) const
{
	return MASK_SOLID;
}

static inline int GetWaterContents( const Vector &point )
{
	// left 4 dead doesn't support moveable water brushes, only world water
	return enginetrace->GetPointContents_WorldOnly(point, MASK_WATER);
}
//-----------------------------------------------------------------------------
// Computes the water level + type
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateWaterState()
{
	// FIXME: This computation is nonsensical for rigid child attachments
	// Should we just grab the type + level of the parent?
	// Probably for rigid children anyways...

	// Compute the point to check for water state
	Vector	point;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &point );

	SetWaterLevel( 0 );
	SetWaterType( CONTENTS_EMPTY );
	int cont = GetWaterContents(point);

	if (( cont & MASK_WATER ) == 0)
		return;

	SetWaterType( cont );
	SetWaterLevel( 1 );

	// point sized entities are always fully submerged
	if ( IsPointSized() )
	{
		SetWaterLevel( 3 );
	}
	else
	{
		// Check the exact center of the box
		point[2] = WorldSpaceCenter().z;

		int midcont = GetWaterContents(point);
		if ( midcont & MASK_WATER )
		{
			// Now check where the eyes are...
			SetWaterLevel( 2 );
			point[2] = EyePosition().z;

			int eyecont = GetWaterContents(point);
			if ( eyecont & MASK_WATER )
			{
				SetWaterLevel( 3 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if entity is in the water and applies any current to velocity
// and sets appropriate water flags
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsCheckWater( void )
{
	return GetWaterLevel() > WL_Feet;
}

//-----------------------------------------------------------------------------
// Purpose: Bounds velocity
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckVelocity( void )
{
	Vector origin = GetAbsOrigin();
	Vector vecAbsVelocity = GetAbsVelocity();

	bool bReset = false;
	for ( int i=0 ; i<3 ; i++ )
	{
		if ( IS_NAN(vecAbsVelocity[i]) )
		{
			Msg( "Got a NaN velocity on %s\n", GetClassname() );
			vecAbsVelocity[i] = 0;
			bReset = true;
		}
		if ( IS_NAN(origin[i]) )
		{
			Msg( "Got a NaN origin on %s\n", GetClassname() );
			origin[i] = 0;
			bReset = true;
		}

		if ( vecAbsVelocity[i] > sv_maxvelocity.GetFloat() ) 
		{
			vecAbsVelocity[i] = sv_maxvelocity.GetFloat();
			bReset = true;
		}
		else if ( vecAbsVelocity[i] < -sv_maxvelocity.GetFloat() )
		{
			vecAbsVelocity[i] = -sv_maxvelocity.GetFloat();
			bReset = true;
		}
	}

	if (bReset)
	{
		SetAbsOrigin( origin );
		SetAbsVelocity( vecAbsVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies gravity to falling objects
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsAddGravityMove( Vector &move )
{
	Vector vecAbsVelocity = GetAbsVelocity();

	move.x = (vecAbsVelocity.x + GetBaseVelocity().x ) * gpGlobals->frametime;
	move.y = (vecAbsVelocity.y + GetBaseVelocity().y ) * gpGlobals->frametime;

	if ( GetFlags() & FL_ONGROUND )
	{
		move.z = GetBaseVelocity().z * gpGlobals->frametime;
		return;
	}

	// linear acceleration due to gravity
	float newZVelocity = vecAbsVelocity.z - GetActualGravity( this ) * gpGlobals->frametime;

	move.z = ((vecAbsVelocity.z + newZVelocity) / 2.0 + GetBaseVelocity().z ) * gpGlobals->frametime;

	Vector vecBaseVelocity = GetBaseVelocity();
	vecBaseVelocity.z = 0.0f;
	SetBaseVelocity( vecBaseVelocity );
	
	vecAbsVelocity.z = newZVelocity;
	SetAbsVelocity( vecAbsVelocity );

	// Bound velocity
	PhysicsCheckVelocity();
}

static constexpr auto STOP_EPSILON = 0.1;
//-----------------------------------------------------------------------------
// Purpose: Slide off of the impacting object.  Returns the blocked flags (1 = floor, 2 = step / wall)
// Input  : in - 
//			normal - 
//			out - 
//			overbounce - 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::PhysicsClipVelocity( const Vector& in, const Vector& normal, Vector& out, float overbounce )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	blocked = 0;

	angle = normal[ 2 ];

	if ( angle > 0 )
	{
		blocked |= 1;		// floor
	}
	if ( !angle )
	{
		blocked |= 2;		// step
	}
	
	backoff = DotProduct (in, normal) * overbounce;

	for ( i=0 ; i<3 ; i++ )
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
		{
			out[i] = 0;
		}
	}
	
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionBounce( trace_t &trace, Vector &vecVelocity, float flMinTotalElasticity )
{
	// Get the impact surface's elasticity.
	float flSurfaceElasticity;
	physprops->GetPhysicsProperties( trace.surface.surfaceProps, nullptr, nullptr, nullptr, &flSurfaceElasticity );
	
	float flTotalElasticity = GetElasticity() * flSurfaceElasticity;
	if ( flMinTotalElasticity > 0.9f )
	{
		flMinTotalElasticity = 0.9f;
	}
	flTotalElasticity = clamp( flTotalElasticity, flMinTotalElasticity, 0.9f );

	// NOTE: A backoff of 2.0f is a reflection
	Vector vecAbsVelocity;
	PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, 2.0f );
	vecAbsVelocity *= flTotalElasticity;

	// Get the total velocity (player + conveyors, etc.)
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	// Stop if on ground.
	if ( trace.plane.normal.z > 0.7f )			// Floor
	{
		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
		contract_assert( pEntity );

		// Are we on the ground?
		if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
		{
			vecAbsVelocity.z = 0.0f;

			// Recompute speedsqr based on the new absvel
			VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
			flSpeedSqr = DotProduct( vecVelocity, vecVelocity );
		}

		SetAbsVelocity( vecAbsVelocity );

		if ( flSpeedSqr < ( 30 * 30 ) )
		{
			if ( pEntity->IsStandable() )
			{
				SetGroundEntity( pEntity );
			}

			// Reset velocities.
			SetAbsVelocity( vec3_origin );
			SetLocalAngularVelocity( vec3_angle );
		}
		else
		{
			Vector vecDelta = GetBaseVelocity() - vecAbsVelocity;	
			Vector vecBaseDir = GetBaseVelocity();
			VectorNormalize( vecBaseDir );
			float flScale = vecDelta.Dot( vecBaseDir );

			VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecVelocity ); 
			VectorMA( vecVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, GetBaseVelocity() * flScale, vecVelocity );
			PhysicsPushEntity( vecVelocity, &trace );
		}
	}
	else
	{
		// If we get *too* slow, we'll stick without ever coming to rest because
		// we'll get pushed down by gravity faster than we can escape from the wall.
		if ( flSpeedSqr < ( 30 * 30 ) )
		{
			// Reset velocities.
			SetAbsVelocity( vec3_origin );
			SetLocalAngularVelocity( vec3_angle );
		}
		else
		{
			SetAbsVelocity( vecAbsVelocity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionSlide( trace_t &trace, Vector &vecVelocity )
{
	// Get the impact surface's friction.
	float flSurfaceFriction;
	physprops->GetPhysicsProperties( trace.surface.surfaceProps, nullptr, nullptr, &flSurfaceFriction, nullptr );

	// A backoff of 1.0 is a slide.
	float flBackOff = 1.0f;	
	Vector vecAbsVelocity;
	PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, flBackOff );

	if ( trace.plane.normal.z <= 0.7 )			// Floor
	{
		SetAbsVelocity( vecAbsVelocity );
		return;
	}

	// Stop if on ground.
	// Get the total velocity (player + conveyors, etc.)
	VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
	float flSpeedSqr = DotProduct( vecVelocity, vecVelocity );

	// Verify that we have an entity.
	CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
	contract_assert( pEntity );

	// Are we on the ground?
	if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
	{
		vecAbsVelocity.z = 0.0f;

		// Recompute speedsqr based on the new absvel
		VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecVelocity );
		flSpeedSqr = DotProduct( vecVelocity, vecVelocity );
	}
	SetAbsVelocity( vecAbsVelocity );

	if ( flSpeedSqr < ( 30 * 30 ) )
	{
		if ( pEntity->IsStandable() )
		{
			SetGroundEntity( pEntity );
		}

		// Reset velocities.
		SetAbsVelocity( vec3_origin );
		SetLocalAngularVelocity( vec3_angle );
	}
	else
	{
		vecAbsVelocity += GetBaseVelocity();
		vecAbsVelocity *= ( 1.0f - trace.fraction ) * gpGlobals->frametime * flSurfaceFriction;
		PhysicsPushEntity( vecAbsVelocity, &trace );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity )
{
	// Stop if on ground.
	if ( trace.plane.normal.z > 0.7 )			// Floor
	{
		// Get the total velocity (player + conveyors, etc.)
		VectorAdd( GetAbsVelocity(), GetBaseVelocity(), vecVelocity );

		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
		contract_assert( pEntity );

		// Are we on the ground?
		if ( vecVelocity.z < ( GetActualGravity( this ) * gpGlobals->frametime ) )
		{
			Vector vecAbsVelocity = GetAbsVelocity();
			vecAbsVelocity.z = 0.0f;
			SetAbsVelocity( vecAbsVelocity );
		}

		if ( pEntity->IsStandable() )
		{
			SetGroundEntity( pEntity );
		}
	}
}

//-----------------------------------------------------------------------------
// Performs the collision resolution for fliers.
//-----------------------------------------------------------------------------
void CBaseEntity::PerformFlyCollisionResolution( trace_t &trace, Vector &move )
{
	switch( GetMoveCollide() )
	{
	case MOVECOLLIDE_FLY_CUSTOM:
		{
			ResolveFlyCollisionCustom( trace, move );
			break;
		}

	case MOVECOLLIDE_FLY_BOUNCE:
		{
			ResolveFlyCollisionBounce( trace, move );
			break;
		}

	case MOVECOLLIDE_FLY_SLIDE:
	case MOVECOLLIDE_DEFAULT:
	// NOTE: The default fly collision state is the same as a slide (for backward capatability).
		{
			ResolveFlyCollisionSlide( trace, move );
			break;
		}

	default:
		{
			// Invalid MOVECOLLIDE_<type>
			contract_assert( 0 );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks if an object has passed into or out of water and sets water info, alters velocity, plays splash sounds, etc.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckWaterTransition( void )
{
	int oldcont = GetWaterType();
	UpdateWaterState();
	int cont = GetWaterType();

	// We can exit right out if we're a child... don't bother with this...
	if (GetMoveParent())
		return;

	if ( cont & MASK_WATER )
	{
		if (oldcont == CONTENTS_EMPTY)
		{

			// just crossed into water
			EmitSound( "BaseEntity.EnterWater" );

			if ( !IsEFlagSet( EFL_NO_WATER_VELOCITY_CHANGE ) )
			{
				Vector vecAbsVelocity = GetAbsVelocity();
				vecAbsVelocity[2] *= 0.5;
				SetAbsVelocity( vecAbsVelocity );
			}
		}
	}
	else
	{
		if ( oldcont != CONTENTS_EMPTY )
		{	
			// just crossed out of water
			EmitSound( "BaseEntity.ExitWater" );
		}		
	}
}

//-----------------------------------------------------------------------------
// Computes new angles based on the angular velocity
//-----------------------------------------------------------------------------
void CBaseEntity::SimulateAngles( float flFrameTime )
{
	// move angles
	QAngle angles;
	VectorMA ( GetLocalAngles(), flFrameTime, GetLocalAngularVelocity(), angles );
	SetLocalAngles( angles );
}

//-----------------------------------------------------------------------------
// Purpose: Toss, bounce, and fly movement.  When onground, do nothing.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsToss( void )
{
	trace_t	trace;
	Vector	move;

	PhysicsCheckWater();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	// Moving upward, off the ground, or  resting on a client/monster, remove FL_ONGROUND
	if ( GetAbsVelocity()[2] > 0 || !GetGroundEntity() || !GetGroundEntity()->IsStandable() )
	{
		SetGroundEntity( nullptr );
	}

	// Check to see if entity is on the ground at rest
	if ( GetFlags() & FL_ONGROUND )
	{
		if ( VectorCompare( GetAbsVelocity(), vec3_origin ) )
		{
			// Clear rotation if not moving (even if on a conveyor)
			SetLocalAngularVelocity( vec3_angle );
			if ( VectorCompare( GetBaseVelocity(), vec3_origin ) )
				return;
		}
	}

	PhysicsCheckVelocity();

	// add gravity
	if ( GetMoveType() == MOVETYPE_FLYGRAVITY && !(GetFlags() & FL_FLY) )
	{
		PhysicsAddGravityMove( move );
	}
	else
	{
		// Base velocity is not properly accounted for since this entity will move again after the bounce without
		// taking it into account
		Vector vecAbsVelocity = GetAbsVelocity();
		vecAbsVelocity += GetBaseVelocity();
		VectorScale( vecAbsVelocity, gpGlobals->frametime, move );
		PhysicsCheckVelocity();
	}

	// move angles
	SimulateAngles( gpGlobals->frametime );

	// move origin
	PhysicsPushEntity( move, &trace );

	if ( VPhysicsGetObject() )
	{
		VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), vec3_angle, true, gpGlobals->frametime );
	}

	PhysicsCheckVelocity();

/*
	NOTE[pmf]: removed this because grenades can start out inside a player volume (particularly in casual mode).
	Behavior for this is handled properly in the grenade's custom FlyCollisionResolution function

	if (trace.allsolid )
	{	
		// entity is trapped in another solid
		SetAbsVelocity(vec3_origin);
		SetLocalAngularVelocity(vec3_angle);
		return;
	}
*/
	
	if ( debugoverlay && sv_grenade_trajectory.GetInt() && (GetFlags() & FL_GRENADE) )
	{
		QAngle angGrTrajAngles;
		Vector vec3tempOrientation = (trace.endpos - trace.startpos);
		VectorAngles( vec3tempOrientation, angGrTrajAngles );

		float flGrTraThickness = sv_grenade_trajectory_thickness.GetFloat();
		Vector vec3_GrTrajMin = Vector( 0, -flGrTraThickness, -flGrTraThickness );
		Vector vec3_GrTrajMax = Vector( vec3tempOrientation.Length(), flGrTraThickness, flGrTraThickness );
		bool bDotted = ( sv_grenade_trajectory_dash.GetInt() && (std::fmod( gpGlobals->curtime, 0.1f ) < 0.05f) );
		
		//extruded "line" is really a box for more visible thickness
		debugoverlay->AddBoxOverlay( trace.startpos, vec3_GrTrajMin, vec3_GrTrajMax, angGrTrajAngles, 0, (bDotted ? 20 : 200), 0, 255, sv_grenade_trajectory_time.GetFloat() );

		//per-bounce box
		if (trace.fraction != 1.0f)
			debugoverlay->AddBoxOverlay( trace.endpos, Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE ), Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE ), QAngle( 0, 0, 0 ), 220, 0, 0, 190, sv_grenade_trajectory_time.GetFloat( ) );
	}	

	if (trace.fraction != 1.0f)
	{
		PerformFlyCollisionResolution( trace, move );
	}
	
	// check for in water
	PhysicsCheckWaterTransition();
}

//-----------------------------------------------------------------------------
// Simulation in local space of rigid children
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsRigidChild( void )
{
	CVProfScope _( "CBaseEntity::PhysicsRigidChild", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// NOTE: rigidly attached children do simulation in local space
	// Collision impulses will be handled either not at all, or by
	// forwarding the information to the highest move parent

	Vector vecPrevOrigin = GetAbsOrigin();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	{ CVProfScope _( "CBaseEntity::PhysicsRigidChild-2", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	};
}

//-----------------------------------------------------------------------------
// Computes the base velocity
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateBaseVelocity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Runs a frame of physics for a specific edict (and all it's children)
// Input  : *ent - the thinking edict
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsSimulate( void )
{
	CVProfScope _( "CBaseEntity::PhysicsSimulate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// NOTE:  Players override PhysicsSimulate and drive through their CUserCmds at that point instead of
	//  processng through this function call!!!  They shouldn't chain to here ever.
	// Make sure not to simulate this guy twice per frame
	if ( !IsPlayerSimulated() && m_nSimulationTick == gpGlobals->tickcount )
	{
		return;
	}

	m_nSimulationTick = gpGlobals->tickcount;

	contract_assert( !IsPlayer() );

	// If we've got a moveparent, we must simulate that first.
	CBaseEntity *pMoveParent = GetMoveParent();

	if ( (GetMoveType() == MOVETYPE_NONE && !pMoveParent) || (GetMoveType() == MOVETYPE_VPHYSICS ) )
	{
		PhysicsNone();
		return;
	}

	// If ground entity goes away, make sure FL_ONGROUND is valid
	if ( !GetGroundEntity() )
	{
		RemoveFlag( FL_ONGROUND );
	}

	if (pMoveParent)
	{
		CVProfScope _( "CBaseEntity::PhysicsSimulate-MoveParent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		pMoveParent->PhysicsSimulate();
	}
	else
	{
		CVProfScope _( "CBaseEntity::PhysicsSimulate-BaseVelocity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		UpdateBaseVelocity();

		if ( ((GetFlags() & FL_BASEVELOCITY) == 0) && (GetBaseVelocity() != vec3_origin) )
		{
			// Apply momentum (add in half of the previous frame of velocity first)
			// BUGBUG: This will break with PhysicsStep() because of the timestep difference
			Vector vecAbsVelocity;
			VectorMA( GetAbsVelocity(), 1.0 + (gpGlobals->frametime*0.5), GetBaseVelocity(), vecAbsVelocity );
			SetAbsVelocity( vecAbsVelocity );
			SetBaseVelocity( vec3_origin );
		}
		RemoveFlag( FL_BASEVELOCITY );
	}

	switch( GetMoveType() )
	{
	case MOVETYPE_PUSH:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_PUSH", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsPusher();
		}
		break;

	case MOVETYPE_VPHYSICS:
		{
		}
		break;

	case MOVETYPE_NONE:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_NONE", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			contract_assert( pMoveParent );
			PhysicsRigidChild();
		}
		break;

	case MOVETYPE_NOCLIP:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_NOCLIP", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsNoclip();
		}
		break;

	case MOVETYPE_STEP:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_STEP", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsStep();
		}
		break;

	case MOVETYPE_FLY:
	case MOVETYPE_FLYGRAVITY:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_FLY", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsToss();
		}
		break;

	case MOVETYPE_CUSTOM:
		{
			CVProfScope _( "CBaseEntity::PhysicsSimulate-MOVETYPE_CUSTOM", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PhysicsCustom();
		}
		break;

	default:
		Warning( "PhysicsSimulate: %s bad movetype %d", GetClassname(), GetMoveType() );
		contract_assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs thinking code if time.  There is some play in the exact time the think
//  function will be called, because it is called before any movement is done
//  in a frame.  Not used for pushmove objects, because they must be exact.
//  Returns false if the entity removed itself.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsRunThink( thinkmethods_t thinkMethod )
{
	if ( IsEFlagSet( EFL_NO_THINK_FUNCTION ) )
		return true;
	
	bool bAlive = true;

	// Don't fire the base if we're avoiding it
	if ( thinkMethod != THINK_FIRE_ALL_BUT_BASE )
	{
		bAlive = PhysicsRunSpecificThink( -1, &CBaseEntity::Think );
		if ( !bAlive )
			return false;
	}

	// Are we just firing the base think?
	if ( thinkMethod == THINK_FIRE_BASE_ONLY )
		return bAlive;

	// Fire the rest of 'em
	for ( int i = 0; i < std::ssize( m_aThinkFunctions ); i++ )
	{

		bAlive = PhysicsRunSpecificThink( i, m_aThinkFunctions[i].m_pfnThink );

		if ( !bAlive )
			return false;
	}
	
	return bAlive;
}

//-----------------------------------------------------------------------------
// Purpose: For testing if all thinks are occuring at the same time
//-----------------------------------------------------------------------------
struct ThinkSync
{
	float					thinktime;
	int						thinktick;
	std::vector< EHANDLE >	entities;

	ThinkSync()
	{
		thinktime = 0;
	}

	ThinkSync( const ThinkSync& src )
	{
		thinktime = src.thinktime;
		thinktick = src.thinktick;
		int c = std::ssize( src.entities );
		for ( int i = 0; i < c; i++ )
		{
			entities.push_back( src.entities[ i ] );
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: For testing if all thinks are occuring at the same time
//-----------------------------------------------------------------------------
class CThinkSyncTester
{
public:
	CThinkSyncTester()
	{
		  m_nLastFrameCount = -1;
		  m_bShouldCheck = false;
	}

	void EntityThinking( int framecount, CBaseEntity *ent, float thinktime, int thinktick )
	{
	}

private:

	ThinkSync	*FindOrAddItem( CBaseEntity *ent, float thinktime )
	{
		ThinkSync &item = m_Thinkers[ thinktime ];
		item.thinktime = thinktime;
		return &item;
	}

	void Report()
	{
		if ( m_Thinkers.empty() )
			return;

		Msg( "-----------------\nThink report frame %i\n", gpGlobals->tickcount );

		for ( auto &[flThinkTime, item] : m_Thinkers )
		{
			ThinkSync *p = &item;
			contract_assert( p );
			if ( !p )
				continue;

			int ecount = std::ssize( p->entities );
			if ( !ecount )
			{
				continue;
			}

			Msg( "thinktime %f, %i entities\n", p->thinktime, ecount );
			for ( int j =0; j < ecount; j++ )
			{
				EHANDLE h = p->entities[ j ];
				int lastthinktick = 0;
				int nextthinktick = 0;
				CBaseEntity *e = h.Get();
				if ( e )
				{
					lastthinktick = e->m_nLastThinkTick;
					nextthinktick = e->m_nNextThinkTick;
				}

				Msg( "  %p : %30s (last %5i/next %5i)\n", h.Get(), h.Get() ? h->GetClassname() : "NULL",
					lastthinktick, nextthinktick );
			}
		}
	}

	std::map< float, ThinkSync >	m_Thinkers;	// keyed by thinktime
	int			m_nLastFrameCount;
	bool		m_bShouldCheck;
};

static CThinkSyncTester g_ThinkChecker;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsRunSpecificThink( int nContextIndex, BASEPTR thinkFunc )
{
	int thinktick = GetNextThinkTick( nContextIndex );

	if ( thinktick <= 0 || thinktick > gpGlobals->tickcount )
		return true;
	
	float thinktime = thinktick * TICK_INTERVAL;

	// Don't let things stay in the past.
	//  it is possible to start that way
	//  by a trigger with a local time.
	if ( thinktime < gpGlobals->curtime )
	{
		thinktime = gpGlobals->curtime;	
	}
	
	// Only do this on the game server

	SetNextThink( nContextIndex, TICK_NEVER_THINK );

	PhysicsDispatchThink( thinkFunc );

	SetLastThink( nContextIndex, gpGlobals->curtime );

	// Return whether entity is still valid
	return ( !IsMarkedForDeletion() );
}

void CBaseEntity::SetGroundEntity( CBaseEntity *ground )
{
	if ( GetGroundEntity() == ground )
		return;

	CBaseEntity *oldGround = GetGroundEntity();
	m_hGroundEntity = ground ? ground->GetRefEHandle() : CBaseHandle();

	// Just starting to touch
	if ( !oldGround && ground )
	{
		ground->AddEntityToGroundList( this );
	}
	// Just stopping touching
	else if ( oldGround && !ground )
	{
		PhysicsNotifyOtherOfGroundRemoval( this, oldGround );
	}
	// Changing out to new ground entity
	else
	{
		PhysicsNotifyOtherOfGroundRemoval( this, oldGround );
		ground->AddEntityToGroundList( this );
	}

	// HACK/PARANOID:  This is redundant with the code above, but in case we get out of sync groundlist entries ever, 
	//  this will force the appropriate flags
	if ( ground )
	{
		AddFlag( FL_ONGROUND );
	}
	else
	{
		RemoveFlag( FL_ONGROUND );
	}
}

CBaseEntity *CBaseEntity::GetGroundEntity( void )
{
	return static_cast<CBaseEntity *>( EntityFromHandle( m_hGroundEntity.Get() ) );
}

void CBaseEntity::StartGroundContact( CBaseEntity *ground )
{
	AddFlag( FL_ONGROUND );
//	Msg( "+++ %s starting contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void CBaseEntity::EndGroundContact( CBaseEntity *ground )
{
	RemoveFlag( FL_ONGROUND );
//	Msg( "--- %s ending contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void CBaseEntity::SetGroundChangeTime( float flTime )
{
	m_flGroundChangeTime = flTime;
}

float CBaseEntity::GetGroundChangeTime( void )
{
	return m_flGroundChangeTime;
}

// Remove this as ground entity for all object resting on this object
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::WakeRestingObjects()
{
	// Unset this as ground entity for everything resting on this object
	//  This calls endgroundcontact for everything on the list
	PhysicsRemoveGroundList( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ent - 
//-----------------------------------------------------------------------------
bool CBaseEntity::HasNPCsOnIt( void )
{
	groundlink_t *link;
	groundlink_t *root = ( groundlink_t * )GetDataObject( GROUNDLINK );
	if ( root )
	{
		for ( link = root->nextLink; link != root; link = link->nextLink )
		{
			
		}
	}

	return false;
}

#undef DEF_THINK_LIMIT
#undef DebugTouchlinks

class CBaseAnimating;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CStatueProp, DT_StatueProp, CStatueProp )

CStatueProp::CStatueProp()
{
}

CStatueProp::~CStatueProp()
{
}

void CStatueProp::Spawn( void )
{
	BaseClass::Spawn();

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
}

void CStatueProp::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;

	if ( pBaseAnimating )
	{
		pBaseAnimating->CollisionProp()->WorldSpaceSurroundingBounds( pVecWorldMins, pVecWorldMaxs );
	}
}

void CStatueProp::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( m_bShatter )
	{
		// Here's the networked data to use with my effect
		//m_nShatterFlags;
		//m_vShatterPosition;
		//m_vShatterForce;

		//FIXME: shatter effects should call a function so derived classes can make their own
		// Do shatter effects here
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Game & Client shared functions moved from physics.cpp

//
IPhysics			*physics = nullptr;
IPhysicsObject		*g_PhysWorldObject = nullptr;
IPhysicsCollision	*physcollision = nullptr;
IPhysicsEnvironment	*physenv = nullptr;
IPhysicsSurfaceProps *physprops = nullptr;
// UNDONE: This hash holds both entity & IPhysicsObject pointer pairs
// UNDONE: Split into separate hashes?
IPhysicsObjectPairHash *g_EntityCollisionHash = nullptr;

const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";

const objectparams_t g_PhysDefaultObjectParams =
{
	nullptr,
	1.0, //mass
	1.0, // inertia
	0.1f, // damping
	0.1f, // rotdamping
	0.05f, // rotIntertiaLimit
	"DEFAULT",
	nullptr,// game data
	0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f, // drag coefficient
	true,// enable collisions?
};

PRECACHE_REGISTER_BEGIN( GLOBAL, PhysFrictionEffect )
	PRECACHE( PARTICLE_SYSTEM, "impact_physics_dust" )
	PRECACHE( PARTICLE_SYSTEM, "impact_physics_sparks" )
PRECACHE_REGISTER_END()

void CSolidSetDefaults::SetDefaults( void *pData )
{
	solid_t *pSolid = (solid_t *)pData;
	pSolid->params = g_PhysDefaultObjectParams;
}

CSolidSetDefaults g_SolidSetup;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &mins - 
//			&maxs - 
// Output : CPhysCollide
//-----------------------------------------------------------------------------
CPhysCollide *PhysCreateBbox( const Vector &minsIn, const Vector &maxsIn )
{
	// UNDONE: Track down why this causes errors for the player controller and adjust/enable
	//float radius = 0.5 - DIST_EPSILON;
	Vector mins = minsIn;// + Vector(radius, radius, radius);
	Vector maxs = maxsIn;// - Vector(radius, radius, radius);

	// VPHYSICS caches/cleans up these
	CPhysCollide *pResult = physcollision->BBoxToCollide( mins, maxs );

	return pResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateBox( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic )
{
	int modelIndex = pEntity->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = modelinfo->GetModel( modelIndex );
		if ( model )
		{
			CStudioHdr studioHdr( modelinfo->GetStudiomodel( model ), mdlcache );
			if ( studioHdr.IsValid() )
			{
				pSurfaceProps = Studio_GetDefaultSurfaceProps( &studioHdr );
			}
		}
	}
	V_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox( mins, maxs );
	if ( !pCollide )
		return nullptr;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, vec3_angle, STRING(pEntity->GetModelName()), isStatic, &solid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateOBB( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic )
{
	int modelIndex = pEntity->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = modelinfo->GetModel( modelIndex );
		if ( model )
		{
			CStudioHdr studioHdr( modelinfo->GetStudiomodel( model ), mdlcache );
			if (studioHdr.IsValid()) 
			{
				pSurfaceProps = Studio_GetDefaultSurfaceProps( &studioHdr );
			}
		}
	}
	V_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox( mins, maxs );
	if ( !pCollide )
		return nullptr;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, angle, STRING(pEntity->GetModelName()), isStatic, &solid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, int modelIndex, int solidIndex )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide )
		return false;

	bool parsed = false;

	std::memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				contract_assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolid( solid_t &solid, CBaseEntity *pEntity, int modelIndex )
{
	return PhysModelParseSolidByIndex( solid, pEntity, modelIndex, -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			*pCollide - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, vcollide_t *pCollide, int solidIndex )
{
	bool parsed = false;

	std::memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				contract_assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreate( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid )
{
	if ( !physenv )
		return nullptr;

	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return nullptr;
	
	solid_t tmpSolid;
	if ( !pSolid )
	{
		pSolid = &tmpSolid;
		if ( !PhysModelParseSolidByIndex( tmpSolid, pEntity, pCollide, -1 ) )
			return nullptr;
	}

	int surfaceProp = -1;
	if ( pSolid->surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( pSolid->surfaceprop );
	}
	IPhysicsObject *pObject = physenv->CreatePolyObject( pCollide->solids[pSolid->index], surfaceProp, origin, angles, &pSolid->params );
	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );

	if ( pObject )
	{
		if ( modelinfo->GetModelType(modelinfo->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = modelinfo->GetModelContents( modelIndex );
			contract_assert( contents!=0 );
			// HACKHACK: contents is used to filter collisions
			// HACKHACK: So keep solid on for water brushes since they should pass collision rules (as triggers)
			if ( contents & MASK_WATER )
			{
				contents |= CONTENTS_SOLID;
			}
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}

	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateUnmoveable( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles )
{
	if ( !physenv )
		return nullptr;

	vcollide_t *pCollide = modelinfo->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return nullptr;

	solid_t solid;

	if ( !PhysModelParseSolidByIndex( solid, pEntity, pCollide, -1 ) )
		return nullptr;

	// collisions are off by default
	solid.params.enableCollisions = true;
	//solid.params.mass = 1.0;
	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( solid.surfaceprop );
	}
	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetModelName());
	IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pCollide->solids[0], surfaceProp, origin, angles, &solid.params );

	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );
	if ( pObject )
	{
		if ( modelinfo->GetModelType(modelinfo->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = modelinfo->GetModelContents( modelIndex );
			contract_assert( contents!=0 );
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}
	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: Create a vphysics object based on an existing collision model
// Input  : *pEntity - 
//			*pModel - 
//			&origin - 
//			&angles - 
//			*pName - 
//			isStatic - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateCustom( CBaseEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid )
{
	if ( !physenv )
		return nullptr;

	solid_t tmpSolid;
	if ( !pSolid )
	{
		PhysGetDefaultAABBSolid( tmpSolid );
		pSolid = &tmpSolid;
	}
	int surfaceProp = physprops->GetSurfaceIndex( pSolid->surfaceprop );
	pSolid->params.pGameData = static_cast<void *>(pEntity);
	pSolid->params.pName = pName;
	IPhysicsObject *pObject = nullptr;
	if ( isStatic )
	{
		pObject = physenv->CreatePolyObjectStatic( pModel, surfaceProp, origin, angles, &pSolid->params );
	}
	else
	{
		pObject = physenv->CreatePolyObject( pModel, surfaceProp, origin, angles, &pSolid->params );
	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			radius - 
//			&origin - 
//			&solid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysSphereCreate( CBaseEntity *pEntity, float radius, const Vector &origin, solid_t &solid )
{
	if ( !physenv )
		return nullptr;

	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( solid.surfaceprop );
	}

	solid.params.pGameData = static_cast<void *>(pEntity);
	IPhysicsObject *pObject = physenv->CreateSphereObject( radius, surfaceProp, origin, vec3_angle, &solid.params, false );

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhysGetDefaultAABBSolid( solid_t &solid )
{
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	V_strncpy( solid.surfaceprop, "default", sizeof( solid.surfaceprop ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destroy a physics object
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void PhysDestroyObject( IPhysicsObject *pObject, CBaseEntity *pEntity )
{

	if ( pObject )
		pObject->SetGameData( nullptr );

	g_EntityCollisionHash->RemoveAllPairsForObject( pObject );
	if ( pEntity && pEntity->IsMarkedForDeletion() )
	{
		g_EntityCollisionHash->RemoveAllPairsForObject( pEntity );
	}

	if ( physenv )
	{
		physenv->DestroyObject( pObject );
	}
}

void AddSurfacepropFile( const char *pFileName, IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	ks::ScratchArena<> scratch;
	// Load file into memory
	FileHandle_t file = pFileSystem->Open( pFileName, "rb", "GAME" );

	if ( file )
	{
		int len = pFileSystem->Size( file );

		// read the file
		int nBufSize = len+1;
		char *buffer = (char *)scratch.AllocBytes( nBufSize );
		pFileSystem->ReadEx( buffer, nBufSize, len, file );
		pFileSystem->Close( file );
		buffer[len] = 0;
		pProps->ParseSurfaceData( pFileName, buffer );
		// buffer is on the stack, no need to free
	}
	else
	{
		Error( "Unable to load surface prop file '%s' (referenced by manifest file '%s')\n", pFileName, SURFACEPROP_MANIFEST_FILE );
	}
}

void PhysParseSurfaceData( IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSurfacepropFile( sub->GetString(), pProps, pFileSystem );
				continue;
			}

			Warning( "surfaceprops::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SURFACEPROP_MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SURFACEPROP_MANIFEST_FILE );
	}

	manifest->deleteThis();
}

void PhysCreateVirtualTerrain( CBaseEntity *pWorld, const objectparams_t &defaultParams )
{
	if ( !physenv )
		return;

	char nameBuf[1024];
	for ( int i = 0; i < MAX_MAP_DISPINFO; i++ )
	{
		CPhysCollide *pCollide = modelinfo->GetCollideForVirtualTerrain( i );
		if ( pCollide )
		{
			solid_t solid;
			solid.params = defaultParams;
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			V_snprintf(nameBuf, sizeof(nameBuf), "vdisp_%04d", i );
			solid.params.pName = nameBuf;
			int surfaceData = physprops->GetSurfaceIndex( "default" );
			// create this as part of the world
			IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pCollide, surfaceData, vec3_origin, vec3_angle, &solid.params );
			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			pObject->SetCollisionHints( COLLISION_HINT_STATICSOLID );
		}
	}
}

IPhysicsObject *PhysCreateWorld_Shared( CBaseEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams )
{
	solid_t solid;
	fluid_t fluid;

	if ( !physenv )
		return nullptr;

	int surfaceData = physprops->GetSurfaceIndex( "default" );

	objectparams_t params = defaultParams;
	params.pGameData = static_cast<void *>(pWorld);
	params.pName = "world";

	IPhysicsObject *pWorldPhysics = physenv->CreatePolyObjectStatic( 
		pWorldCollide->solids[0], surfaceData, vec3_origin, vec3_angle, &params );

	// hint - saves vphysics some work
	pWorldPhysics->SetCallbackFlags( pWorldPhysics->GetCallbackFlags() | CALLBACK_NEVER_DELETED );

	//PhysCheckAdd( world, "World" );
	// walk the world keys in case there are some fluid volumes to create
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pWorldCollide );

	bool bCreateVirtualTerrain = false;
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();

		if ( !strcmpi( pBlock, "solid" ) || !strcmpi( pBlock, "staticsolid" ) )
		{
			solid.params = defaultParams;
			pParse->ParseSolid( &solid, &g_SolidSetup );
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			solid.params.pName = "world";
			int surfaceData = physprops->GetSurfaceIndex( "default" );

			// already created world above
			if ( solid.index == 0 )
				continue;

			if ( !pWorldCollide->solids[solid.index] )
			{
				// this implies that the collision model is a mopp and the physics DLL doesn't support that.
				bCreateVirtualTerrain = true;
				continue;
			}
			// create this as part of the world
			IPhysicsObject *pObject = physenv->CreatePolyObjectStatic( pWorldCollide->solids[solid.index], 
				surfaceData, vec3_origin, vec3_angle, &solid.params );

			// invalid collision model or can't create, ignore
			if (!pObject)
				continue;

			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			contract_assert( solid.contents != 0 );
			pObject->SetContents( solid.contents );
			if ( solid.contents & CONTENTS_SOLID )
			{
				pObject->SetCollisionHints(COLLISION_HINT_STATICSOLID);
			}

			if ( !pWorldPhysics )
			{
				pWorldPhysics = pObject;
			}
		}
		else if ( !strcmpi( pBlock, "fluid" ) )
		{
			pParse->ParseFluid( &fluid, nullptr );

			// create a fluid for floating
			if ( fluid.index > 0 )
			{
				solid.params = defaultParams;	// copy world's params
				solid.params.enableCollisions = true;
				solid.params.pName = "fluid";
				solid.params.pGameData = static_cast<void *>(pWorld);
				fluid.params.pGameData = static_cast<void *>(pWorld);
				int surfaceData = physprops->GetSurfaceIndex( fluid.surfaceprop );
				// create this as part of the world
				IPhysicsObject *pWater = physenv->CreatePolyObjectStatic( pWorldCollide->solids[fluid.index], 
					surfaceData, vec3_origin, vec3_angle, &solid.params );

				pWater->SetCallbackFlags( pWater->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
				physenv->CreateFluidController( pWater, &fluid.params );
			}
		}
		else if ( !strcmpi( pBlock, "materialtable" ) )
		{
			int surfaceTable[128];
			std::memset( surfaceTable, 0, sizeof(surfaceTable) );

			pParse->ParseSurfaceTable( surfaceTable, nullptr );
			physprops->SetWorldMaterialIndexTable( surfaceTable, 128 );
		}
		else if ( !strcmpi(pBlock, "virtualterrain" ) )
		{
			bCreateVirtualTerrain = true;
			pParse->SkipBlock();
		}
		else
		{
			// unknown chunk???
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	if ( bCreateVirtualTerrain && physcollision->SupportsVirtualMesh() )
	{
		PhysCreateVirtualTerrain( pWorld, defaultParams );
	}
	return pWorldPhysics;
}

void PhysRecheckObjectPair( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0->IsStatic() )
	{
		pObject0->RecheckCollisionFilter();
	}
	if ( !pObject1->IsStatic() )
	{
		pObject1->RecheckCollisionFilter();
	}
}

void PhysEnableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pObject0->GetGameData());
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	g_EntityCollisionHash->RemoveObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pObject0->GetGameData());
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	g_EntityCollisionHash->AddObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

void PhysDisableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	if ( !pEntity0 || !pEntity1 )
		return;

	g_EntityCollisionHash->AddObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

void PhysEnableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	if ( !pEntity0 || !pEntity1 )
		return;

	g_EntityCollisionHash->RemoveObjectPair( pEntity0, pEntity1 );
	pEntity0->CollisionRulesChanged();
	pEntity1->CollisionRulesChanged();
}

bool PhysEntityCollisionsAreDisabled( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	return g_EntityCollisionHash->IsObjectPairInHash( pEntity0, pEntity1 );
}

void PhysEnableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	g_EntityCollisionHash->RemoveObjectPair( pObject0, pObject1 );
	PhysRecheckObjectPair( pObject0, pObject1 );
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 )
{
	if ( !pObject0 || !pObject1 )
		return;

	g_EntityCollisionHash->AddObjectPair( pObject0, pObject1 );
	PhysRecheckObjectPair( pObject0, pObject1 );
}

void PhysComputeSlideDirection( IPhysicsObject *pPhysics, const Vector &inputVelocity, const AngularImpulse &inputAngularVelocity, 
							   Vector *pOutputVelocity, Vector *pOutputAngularVelocity, float minMass )
{
	Vector velocity = inputVelocity;
	AngularImpulse angVel = inputAngularVelocity;
	Vector pos;

	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( !pOther->IsMoveable() || pOther->GetMass() > minMass )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );

			// BUGBUG: Figure out the correct rotation clipping equation
			if ( pOutputAngularVelocity )
			{
				angVel = normal * DotProduct( angVel, normal );
			}

			// Determine how far along plane to slide based on incoming direction.
			// NOTE: Normal points away from this object
			float proj = DotProduct( velocity, normal );
			if ( proj > 0.0f )
			{
				velocity -= normal * proj;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	//NDebugOverlay::Line( pos, pos + unitVel * 20, 0, 0, 255, true, 0.1 );
	
	if ( pOutputVelocity )
	{
		*pOutputVelocity = velocity;
	}
	if ( pOutputAngularVelocity )
	{
		*pOutputAngularVelocity = angVel;
	}
}

bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir )
{
	bool hit = false;
	void *pGameData = pPhysics->GetGameData();
	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( pOther->GetGameData() != pGameData )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );
			if ( DotProduct(normal,dir) > 0 )
			{
				hit = true;
				break;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	return hit;
}

void PhysForceClearVelocity( IPhysicsObject *pPhys )
{
	IPhysicsFrictionSnapshot *pSnapshot = pPhys->CreateFrictionSnapshot();
	// clear the velocity of the rigid body
	Vector vel;
	AngularImpulse angVel;
	vel.Init();
	angVel.Init();
	pPhys->SetVelocity( &vel, &angVel );
	// now clear the "strain" stored in the contact points
	while ( pSnapshot->IsValid() )
	{
		pSnapshot->ClearFrictionForce();
		pSnapshot->RecomputeFriction();
		pSnapshot->NextFrictionData();
	}
	pPhys->DestroyFrictionSnapshot( pSnapshot );
}

void PhysFrictionEffect( Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit )
{
	QAngle angDirection;
	VectorAngles( vecVel, angDirection );

	surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );
	surfacedata_t *phit = physprops->GetSurfaceData( surfacePropsHit );
	switch ( phit->game.material )
	{
	case CHAR_TEX_DIRT:
		
		if ( energy < MASS10_SPEED2ENERGY(15) )
			break;
		
		DispatchParticleEffect( "impact_physics_dust", vecPos, angDirection );
		break;

	case CHAR_TEX_CONCRETE:
		
		if ( energy < MASS10_SPEED2ENERGY(28) )
			break;

		DispatchParticleEffect( "impact_physics_dust", vecPos, angDirection );
		break;
	}
	
	//Metal sparks
	if ( energy > MASS10_SPEED2ENERGY(50) )
	{
		// make sparks for metal/concrete scrapes with enough energy
		if ( psurf->game.material == CHAR_TEX_METAL || psurf->game.material == CHAR_TEX_GRATE )
		{	
			switch ( phit->game.material )
			{
			case CHAR_TEX_CONCRETE:
			case CHAR_TEX_METAL:

				DispatchParticleEffect( "impact_physics_sparks", vecPos, angDirection );
				break;									
			}
		}
	}
}

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit )
{
	if ( !pEntity || energy < 75.0f || surfaceProps < 0 )
		return;
	
	// don't make noise for hidden/invisible/sky materials
	surfacedata_t *phit = physprops->GetSurfaceData( surfacePropsHit );
	surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );

	if ( phit->game.material == 'X' || psurf->game.material == 'X' )
		return;

	// rescale the incoming energy
	energy *= ENERGY_VOLUME_SCALE;

	// volume of scrape is proportional to square of energy (steeper rolloff at low energies)
	float volume = energy * energy;
		
	unsigned short soundName = psurf->sounds.scrapeRough;
	HSOUNDSCRIPTHASH *soundHandle = &psurf->soundhandles.scrapeRough;

	if ( psurf->sounds.scrapeSmooth && phit->audio.roughnessFactor < psurf->audio.roughThreshold )
	{
		soundName = psurf->sounds.scrapeSmooth;
		soundHandle = &psurf->soundhandles.scrapeRough;
	}

	const char *pSoundName = physprops->GetString( soundName );

	PhysFrictionSound( pEntity, pObject, pSoundName, *soundHandle, volume );
}

//-----------------------------------------------------------------------------
// Purpose: Precaches a surfaceproperties string name if it's set.
// Input  : idx - 
// Output : static void
//-----------------------------------------------------------------------------
static HSOUNDSCRIPTHASH PrecachePhysicsSoundByStringIndex( int idx )
{
	// Only precache if a value was set in the script file...
	if ( idx != 0 )
	{
		return CBaseEntity::PrecacheScriptSound( physprops->GetString( idx ) );
	}

	return SOUNDEMITTER_INVALID_HASH;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates all surfacedata sounds and precaches them
// Output : static void
//-----------------------------------------------------------------------------
void PrecachePhysicsSounds()
{
	// precache the surface prop sounds
	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		contract_assert( pprop );

		pprop->soundhandles.walkStepLeft = PrecachePhysicsSoundByStringIndex( pprop->sounds.walkStepLeft );
		pprop->soundhandles.walkStepRight = PrecachePhysicsSoundByStringIndex( pprop->sounds.walkStepRight );
		pprop->soundhandles.runStepLeft = PrecachePhysicsSoundByStringIndex( pprop->sounds.runStepLeft );
		pprop->soundhandles.runStepRight = PrecachePhysicsSoundByStringIndex( pprop->sounds.runStepRight );
		pprop->soundhandles.impactSoft = PrecachePhysicsSoundByStringIndex( pprop->sounds.impactSoft );
		pprop->soundhandles.impactHard = PrecachePhysicsSoundByStringIndex( pprop->sounds.impactHard );
		pprop->soundhandles.scrapeSmooth = PrecachePhysicsSoundByStringIndex( pprop->sounds.scrapeSmooth );
		pprop->soundhandles.scrapeRough = PrecachePhysicsSoundByStringIndex( pprop->sounds.scrapeRough );
		pprop->soundhandles.bulletImpact = PrecachePhysicsSoundByStringIndex( pprop->sounds.bulletImpact );
		pprop->soundhandles.rolling = PrecachePhysicsSoundByStringIndex( pprop->sounds.rolling );
		pprop->soundhandles.breakSound = PrecachePhysicsSoundByStringIndex( pprop->sounds.breakSound );
		pprop->soundhandles.strainSound = PrecachePhysicsSoundByStringIndex( pprop->sounds.strainSound );
	}
}

float PhysGetEntityMass( CBaseEntity *pEntity )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int physCount = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
	float otherMass = 0;
	for ( int i = 0; i < physCount; i++ )
	{
		otherMass += pList[i]->GetMass();
	}

	return otherMass;
}

CSoftbodyEnvironment g_SoftbodyEnvironment;
static CSoftbodyProcess s_ClothSystemProcess;

void CSoftbodyProcess::OnRestore()
{
	// ToDo: implement softbody reset-all 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output :	
//-----------------------------------------------------------------------------
void CSoftbodyProcess::Update( float frametime )
{
	g_SoftbodyEnvironment.Step( frametime );
}

static RnDebugDrawOptions_t s_SoftbodyDebugDrawOptions( RN_SOFTBODY_DRAW_EDGES | RN_SOFTBODY_DRAW_POLYGONS );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output :	
//-----------------------------------------------------------------------------
void CSoftbodyProcess::PostRender( )
{
}
