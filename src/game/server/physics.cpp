// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "coordsize.h"
#include "eiface.h"
#include "networkvar.h"
#include "shake.h"
#include "vcollide_parse.h"
#include "vphysics/collision_set.h"
#include "vphysics/constraints.h"
#include "vphysics/friction.h"
#include "vphysics/object_hash.h"
#include "vphysics/performance.h"
#include "vphysics/vehicles.h"
#include "vphysics2_interface_flags.h"
#include "shareddefs.h"
#include "iservernetworkable.h"
#include "string_t.h"
#include "bone_setup.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "collisionutils.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "bspflags.h"
#include "edict.h"
#include "vphysics/player_controller.h"
#include "engine/IEngineTrace.h"
#include "engine/thinktracecounter.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.fmtstr;
import ks.tier1.functors;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import ks.vstdlib.callqueue;
import ks.vstdlib.jobthread;
import :player;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import :entity_dispatch;
import :entity_kind;
import ks.math.vmatrix;
import ks.math.pfns;
import ks.game.vphysicsupdateai;
import :cstrike15.cs_player;

class CBaseEntity;
class CBasePlayer;
class CCollisionEvent;
class CEntityList;
class CTakeDamageInfo;
class IGameSystem;
struct gamevcollisionevent_t;

using namespace ::ks::reflect;

// Purpose: Interface layer for ipion IVP physics.

void PrecachePhysicsSounds( void );

ConVar phys_speeds( side_registry(), "phys_speeds", "0" );
ConVar phys_show_active( side_registry(), "phys_show_active", "0", FCVAR_CHEAT );

ConVar phys_debug_check_contacts(side_registry(), "phys_debug_check_contacts", "0", FCVAR_CHEAT|FCVAR_REPLICATED);
// defined in phys_constraint
extern IPhysicsConstraintEvent *g_pConstraintEvents;

CEntityList *g_pShadowEntities = nullptr;

// local variables
static float g_PhysAverageSimTime;
CCallQueue g_PostSimulationQueue;

// local routines
static IPhysicsObject *PhysCreateWorld( CBaseEntity *pWorld );
static void PhysFrame( float deltaTime );
static bool IsDebris( int collisionGroup );

void TimescaleChanged( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( physenv )
	{
		physenv->ResetSimulationClock();
	}
}

ConVar phys_timescale( side_registry(), "phys_timescale", "1", 0, "Scale time for physics", TimescaleChanged );

	CCollisionEvent g_Collisions;

IPhysicsCollisionSolver * const g_pCollisionSolver = &g_Collisions;
IPhysicsCollisionEvent * const g_pCollisionEventHandler = &g_Collisions;
IPhysicsObjectEvent * const g_pObjectEventHandler = &g_Collisions;


class CPhysicsHook : public CBaseGameSystemPerFrame
{
public:
	virtual const char *Name() { return "CPhysicsHook"; }

	virtual bool Init();
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
	virtual void LevelShutdownPostEntity();
	virtual void FrameUpdatePostEntityThink();
	virtual void PreClientUpdate();


	bool ShouldSimulate()
	{
		return (physenv && !m_bPaused) ? true : false;
	}

	physicssound::soundlist_t m_impactSounds;
	std::vector< physicssound::breaksound_t > m_breakSounds;

	std::vector< masscenteroverride_t >	m_massCenterOverrides;

	float		m_impactSoundTime;
	bool		m_bPaused;
	bool		m_isFinalTick;
};

CPhysicsHook	g_PhysicsHook;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* PhysicsGameSystem()
{
	return &g_PhysicsHook;
}

//-----------------------------------------------------------------------------
// Purpose: The physics hook callback implementations
//-----------------------------------------------------------------------------
bool CPhysicsHook::Init( void )
{
	factorylist_t factories;
	
	// Get the list of interface factories to extract the physics DLL's factory
	FactoryList_Retrieve( factories );

	if ( !factories.physicsFactory )
		return false;

	if ((physics = (IPhysics *)factories.physicsFactory( VPHYSICS_INTERFACE_VERSION, nullptr )) == nullptr ||
		(physcollision = (IPhysicsCollision *)factories.physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, nullptr )) == nullptr ||
		(physprops = (IPhysicsSurfaceProps *)factories.physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, nullptr )) == nullptr
		)
		return false;

	PhysParseSurfaceData( physprops, filesystem );

	m_isFinalTick = true;
	m_impactSoundTime = 0;
	return true;
}

// a little debug wrapper to help fix bugs when entity pointers get trashed

ConVar sv_phys_props_block_movers( side_registry(), "sv_phys_props_block_movers", "0" );

void CPhysicsHook::LevelInitPreEntity() 
{
	physenv = physics->CreateEnvironment();
	physics_performanceparams_t params;
	params.Defaults();
	params.maxCollisionsPerObjectPerTimestep = 10;
	physenv->SetPerformanceSettings( &params );

	{
	g_EntityCollisionHash = physics->CreateObjectPairHash();
	}
	factorylist_t factories;
	FactoryList_Retrieve( factories );
	physenv->SetDebugOverlay( factories.engineFactory );
	physenv->EnableDeleteQueue( true );

	physenv->SetCollisionSolver( &g_Collisions );
	physenv->SetCollisionEventHandler( &g_Collisions );
	physenv->SetConstraintEventHandler( g_pConstraintEvents );
	physenv->EnableConstraintNotify( true ); // callback when an object gets deleted that is attached to a constraint

	physenv->SetObjectEventHandler( &g_Collisions );
	
	// always run 64 tick physics
	// we haven't done enough testing here and with really small timestpes (1/128, 1/256) some things can become unstable
	// this results in behaviors like guns bouncing for example
	// Also we would normally try to sync the game & physics sim rates for player vphysics (which csgo doesn't use) and 
	// blocking moving objects (e.g. elevators) with physics objects
	float fTickInterval = 1.0f / 64.0f;
	physenv->SetSimulationTimestep( fTickInterval );

	// HL Game gravity, not real-world gravity
	physenv->SetGravity( Vector( 0, 0, -sv_gravity.GetFloat() ) );
	g_PhysAverageSimTime = 0;

	g_PhysWorldObject = PhysCreateWorld( GetWorldEntity() );

	g_pShadowEntities = new CEntityList;

	PrecachePhysicsSounds();

	m_bPaused = true;
}

void CPhysicsHook::LevelInitPostEntity() 
{
	m_bPaused = false;
}

void CPhysicsHook::LevelShutdownPreEntity() 
{
	if ( !physenv )
		return;
	physenv->SetQuickDelete( true );
}

void CPhysicsHook::LevelShutdownPostEntity() 
{
	if ( !physenv )
		return;

	g_Collisions.LevelShutdown();

	physics->DestroyEnvironment( physenv );
	physenv = nullptr;

	physics->DestroyObjectPairHash( g_EntityCollisionHash );
	g_EntityCollisionHash = nullptr;

	physics->DestroyAllCollisionSets();

	g_PhysWorldObject = nullptr;

	delete g_pShadowEntities;
	g_pShadowEntities = nullptr;
	m_impactSounds.RemoveAll();
	m_breakSounds.clear();
	m_massCenterOverrides.clear(); m_massCenterOverrides.shrink_to_fit();
}



// called after entities think
void CPhysicsHook::FrameUpdatePostEntityThink( ) 
{
	CVProfScope _( "CPhysicsHook::FrameUpdatePostEntityThink", 0, VPROF_BUDGETGROUP_PHYSICS, false, BUDGETFLAG_OTHER );

	// Tracker 24846:  If game is paused, don't simulate vphysics
	float interval = ( gpGlobals->frametime > 0.0f ) ? TICK_INTERVAL : 0.0f;

	// update the physics simulation, not we don't use gpGlobals->frametime, since that can be 30 msec or 15 msec
	// depending on whether IsSimulatingOnAlternateTicks is true or not
	if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
	{
		m_isFinalTick = false;

		PhysFrame( interval );

	}
	m_isFinalTick = true;

	PhysFrame( interval );
}

void CPhysicsHook::PreClientUpdate()
{
	m_impactSoundTime += gpGlobals->frametime;
	if ( m_impactSoundTime > 0.05f )
	{
		physicssound::PlayImpactSounds( m_impactSounds );
		m_impactSoundTime = 0.0f;
		physicssound::PlayBreakSounds( m_breakSounds );
	}
}

bool PhysIsFinalTick()
{
	return g_PhysicsHook.m_isFinalTick;
}

IPhysicsObject *PhysCreateWorld( CBaseEntity *pWorld )
{
	staticpropmgr->CreateVPhysicsRepresentations( physenv, &g_SolidSetup, pWorld );
	return PhysCreateWorld_Shared( pWorld, modelinfo->GetVCollide(1), g_PhysDefaultObjectParams );
}

// vehicle wheels can only collide with things that can't get stuck in them during game physics
// because they aren't in the game physics world at present
static bool WheelCollidesWith( IPhysicsObject *pObj, CBaseEntity *pEntity )
{
	// Cull against interactive debris
	if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS )
		return false;

	// Hit physics ents
	if ( pEntity->GetMoveType() == MOVETYPE_PUSH || pEntity->GetMoveType() == MOVETYPE_VPHYSICS || pObj->IsStatic() )
		return true;

	return false;
}

CCollisionEvent::CCollisionEvent()
{
	m_inCallback = 0;
	m_bBufferTouchEvents = false;
	m_lastTickFrictionError = 0;
}

int CCollisionEvent::ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 )
{
	CallbackContext check(this);

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

	// objects that are both constrained to the world don't collide with each other
	if ( (gameFlags0 & gameFlags1) & FVPHYSICS_CONSTRAINT_STATIC )
	{
		return 0;
	}

	// Special collision rules for vehicle wheels
	// Their entity collides with stuff using the normal rules, but they
	// have different rules than the vehicle body for various reasons.
	// sort of a hack because we don't have spheres to represent them in the game
	// world for speculative collisions.
	if ( pObj0->GetCallbackFlags() & CALLBACK_IS_VEHICLE_WHEEL )
	{
		if ( !WheelCollidesWith( pObj1, pEntity1 ) )
			return false;
	}
	if ( pObj1->GetCallbackFlags() & CALLBACK_IS_VEHICLE_WHEEL )
	{
		if ( !WheelCollidesWith( pObj0, pEntity0 ) )
			return false;
	}

	if ( pEntity0->ForceVPhysicsCollide( pEntity1 ) || pEntity1->ForceVPhysicsCollide( pEntity0 ) )
		return 1;

	if ( pEntity0->edict() && pEntity1->edict() )
	{
		// don't collide with your owner
		if ( pEntity0->GetOwnerEntity() == pEntity1 || pEntity1->GetOwnerEntity() == pEntity0 )
			return 0;
	}

	if ( pEntity0->GetMoveParent() || pEntity1->GetMoveParent() )
	{
		CBaseEntity *pParent0 = pEntity0->GetRootMoveParent();
		CBaseEntity *pParent1 = pEntity1->GetRootMoveParent();
		
		// NOTE: Don't let siblings/parents collide.  If you want this behavior, do it
		// with constraints, not hierarchy!
		if ( pParent0 == pParent1 )
			return 0;

		if ( g_EntityCollisionHash->IsObjectPairInHash( pParent0, pParent1 ) )
			return 0;

		IPhysicsObject *p0 = pParent0->VPhysicsGetObject();
		IPhysicsObject *p1 = pParent1->VPhysicsGetObject();
		if ( p0 && p1 )
		{
			if ( g_EntityCollisionHash->IsObjectPairInHash( p0, p1 ) )
				return 0;
		}
	}

	int solid0 = pEntity0->GetSolid();
	int solid1 = pEntity1->GetSolid();
	int nSolidFlags0 = pEntity0->GetSolidFlags();
	int nSolidFlags1 = pEntity1->GetSolidFlags();

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

	// BRJ 1/24/03
	// You can remove the assert if it's problematic; I *believe* this condition
	// should be met, but I'm not sure.
	//Assert ( (solid0 != SOLID_NONE) && (solid1 != SOLID_NONE) );
	if ( (solid0 == SOLID_NONE) || (solid1 == SOLID_NONE) )
		return 0;

	// not solid doesn't collide with anything
	if ( (nSolidFlags0|nSolidFlags1) & FSOLID_NOT_SOLID )
	{
		// might be a vphysics trigger, collide with everything but "not solid"
		if ( pObj0->IsTrigger() && !(nSolidFlags1 & FSOLID_NOT_SOLID) )
			return 1;
		if ( pObj1->IsTrigger() && !(nSolidFlags0 & FSOLID_NOT_SOLID) )
			return 1;

		return 0;
	}
	
	if ( (nSolidFlags0 & FSOLID_TRIGGER) && 
		!(solid1 == SOLID_VPHYSICS || solid1 == SOLID_BSP || movetype1 == MOVETYPE_VPHYSICS) )
		return 0;

	if ( (nSolidFlags1 & FSOLID_TRIGGER) && 
		!(solid0 == SOLID_VPHYSICS || solid0 == SOLID_BSP || movetype0 == MOVETYPE_VPHYSICS) )
		return 0;

	if ( !g_pGameRules->ShouldCollide( pEntity0->GetCollisionGroup(), pEntity1->GetCollisionGroup() ) )
		return 0;

	// check contents
	if ( !(pObj0->GetContents() & pEntity1->PhysicsSolidMaskForEntity()) || !(pObj1->GetContents() & pEntity0->PhysicsSolidMaskForEntity()) )
		return 0;

	if ( g_EntityCollisionHash->IsObjectPairInHash( pGameData0, pGameData1 ) )
		return 0;

	if ( g_EntityCollisionHash->IsObjectPairInHash( pObj0, pObj1 ) )
		return 0;

	return 1;
}

bool FindMaxContact( IPhysicsObject *pObject, float minForce, IPhysicsObject **pOtherObject, Vector *contactPos, Vector *pForce )
{
	float mass = pObject->GetMass();
	float maxForce = minForce;
	*pOtherObject = nullptr;
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		if ( pOther->IsMoveable() && pOther->GetMass() > mass )
		{
			float force = pSnapshot->GetNormalForce();
			if ( force > maxForce )
			{
				*pOtherObject = pOther;
				pSnapshot->GetContactPoint( *contactPos );
				pSnapshot->GetSurfaceNormal( *pForce );
				*pForce *= force;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	if ( *pOtherObject )
		return true;

	return false;
}

extern bool PropIsGib(CBaseEntity *pEntity);
bool CCollisionEvent::ShouldFreezeObject( IPhysicsObject *pObject )
{
	// for now, don't apply a per-object limit to ai MOVETYPE_PUSH objects
	// NOTE: If this becomes a problem (too many collision checks this tick) we should add a path
	// to inform the logic in VPhysicsUpdatePusher() about the limit being applied so 
	// that it doesn't falsely block the object when it's simply been temporarily frozen
	// for performance reasons
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( pEntity )
	{
		int nMoveType = pEntity->GetMoveType();
		if ( nMoveType == MOVETYPE_PUSH || nMoveType == MOVETYPE_NONE )
			return false;
		
		// don't limit vehicle collisions either, limit can make breaking through a pile of breakable
		// props very hitchy
		
	}

	// if we're freezing a debris object, then it's probably due to some kind of solver issue
	// usually this is a large object resting on the debris object in question which is not
	// very stable.
	// After doing the experiment of constraining the dynamic range of mass while solving friction
	// contacts, I like the results of this tradeoff better.  So damage or remove the debris object
	// wherever possible once we hit this case:
	if ( pEntity && IsDebris( pEntity->GetCollisionGroup()) && !nullptr )
	{
		IPhysicsObject *pOtherObject = nullptr;
		Vector contactPos;
		Vector force;
		// find the contact with the moveable object applying the most contact force
		if ( FindMaxContact( pObject, pObject->GetMass() * 10, &pOtherObject, &contactPos, &force ) )
		{
			CBaseEntity *pOther = static_cast<CBaseEntity *>(pOtherObject->GetGameData());
			// this object can take damage, crush it
			if ( pEntity->m_takedamage > DAMAGE_EVENTS_ONLY )
			{
				CTakeDamageInfo dmgInfo( pOther, pOther, force, contactPos, force.Length() * 0.1f, DMG_CRUSH );
				PhysCallbackDamage( pEntity, dmgInfo );
			}
			else
			{
				// can't be damaged, so do something else:
				if ( PropIsGib(pEntity) )
				{
					// it's always safe to delete gibs, so kill this one to avoid simulation problems
					PhysCallbackRemove( pEntity->NetworkProp() );
				}
				else
				{
					// not a gib, create a solver:
					// UNDONE: Add a property to override this in gameplay critical scenarios?
					g_PostSimulationQueue.QueueCall( EntityPhysics_CreateSolver, pOther, pEntity, true, 1.0f );
				}
			}
		}
	}
	return true;
}

bool CCollisionEvent::ShouldFreezeContacts( IPhysicsObject **pObjectList, int objectCount )
{
	if ( m_lastTickFrictionError > gpGlobals->tickcount || m_lastTickFrictionError < (gpGlobals->tickcount-1) )
	{
		DevWarning("Performance Warning: large friction system (%d objects)!!!\n", objectCount );
	}
	m_lastTickFrictionError = gpGlobals->tickcount;
	return false;
}

// NOTE: these are fully edge triggered events 
// called when an object wakes up (starts simulating)
void CCollisionEvent::ObjectWake( IPhysicsObject *pObject )
{
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( pEntity && pEntity->HasDataObjectType( VPHYSICSWATCHER ) )
	{
		ReportVPhysicsStateChanged( pObject, pEntity, true );
	}
}
// called when an object goes to sleep (no longer simulating)
void CCollisionEvent::ObjectSleep( IPhysicsObject *pObject )
{
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( pEntity && pEntity->HasDataObjectType( VPHYSICSWATCHER ) )
	{
		ReportVPhysicsStateChanged( pObject, pEntity, false );
	}
}

bool PhysShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1 )
{
	void *pGameData0 = pObj0->GetGameData();
	void *pGameData1 = pObj1->GetGameData();
	if ( !pGameData0 || !pGameData1 )
		return false;
	return g_Collisions.ShouldCollide( pObj0, pObj1, pGameData0, pGameData1 ) ? true : false;
}

bool PhysIsInCallback()
{
	if ( (physenv && physenv->IsInSimulation()) || g_Collisions.IsInCallback() )
		return true;

	return false;
}

static void ReportPenetration( CBaseEntity *pEntity, float duration )
{
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		if ( g_pDeveloper->GetInt() > 1 )
		{
			pEntity->m_debugOverlays |= OVERLAY_ABSBOX_BIT;
		}

		pEntity->AddTimedOverlay( UTIL_VarArgs("VPhysics Penetration Error (%s)!", pEntity->GetDebugName()), duration );
	}
}

static bool IsDebris( int collisionGroup )
{
	switch ( collisionGroup )
	{
	case COLLISION_GROUP_DEBRIS:
	case COLLISION_GROUP_INTERACTIVE_DEBRIS:
	case COLLISION_GROUP_DEBRIS_TRIGGER:
		return true;
	default:
		break;
	}
	return false;
}

static void UpdateEntityPenetrationFlag( CBaseEntity *pEntity, bool isPenetrating )
{
	if ( !pEntity )
		return;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
	for ( int i = 0; i < count; i++ )
	{
		if ( !pList[i]->IsStatic() )
		{
			if ( isPenetrating )
			{
				PhysSetGameFlags( pList[i], FVPHYSICS_PENETRATING );
			}
			else
			{
				PhysClearGameFlags( pList[i], FVPHYSICS_PENETRATING );
			}
		}
	}
}

void CCollisionEvent::GetListOfPenetratingEntities( CBaseEntity *pSearch, std::vector< CBaseEntity * > &list )
{
	for ( int i = std::ssize( m_penetrateEvents )-1; i >= 0; --i )
	{
		if ( m_penetrateEvents[i].hEntity0 == pSearch && m_penetrateEvents[i].hEntity1.Get() != nullptr )
		{
			list.push_back( m_penetrateEvents[i].hEntity1 );
		}
		else if ( m_penetrateEvents[i].hEntity1 == pSearch && m_penetrateEvents[i].hEntity0.Get() != nullptr )
		{
			list.push_back( m_penetrateEvents[i].hEntity0 );
		}
	}
}

void CCollisionEvent::UpdatePenetrateEvents( void )
{
	for ( int i = std::ssize( m_penetrateEvents )-1; i >= 0; --i )
	{
		CBaseEntity *pEntity0 = m_penetrateEvents[i].hEntity0;
		CBaseEntity *pEntity1 = m_penetrateEvents[i].hEntity1;

		if ( m_penetrateEvents[i].collisionState == COLLSTATE_TRYDISABLE )
		{
			if ( pEntity0 && pEntity1 )
			{
				IPhysicsObject *pObj0 = pEntity0->VPhysicsGetObject();
				if ( pObj0 )
				{
					PhysForceEntityToSleep( pEntity0, pObj0 );
				}
				IPhysicsObject *pObj1 = pEntity1->VPhysicsGetObject();
				if ( pObj1 )
				{
					PhysForceEntityToSleep( pEntity1, pObj1 );
				}
				m_penetrateEvents[i].collisionState = COLLSTATE_DISABLED;
				continue;
			}
			// missing entity or object, clear event
		}
		else if ( m_penetrateEvents[i].collisionState == COLLSTATE_TRYENTITYSOLVER )
		{
			if ( pEntity0 && pEntity1 )
			{
				if ( !IsDebris(pEntity1->GetCollisionGroup()) || pEntity1->GetMoveType() != MOVETYPE_VPHYSICS )
				{
					CBaseEntity *pTmp = pEntity0;
					pEntity0 = pEntity1;
					pEntity1 = pTmp;
				}
				EntityPhysics_CreateSolver( pEntity0, pEntity1, true, 1.0f );
			}
			// transferred to solver, clear event
		}
		else if ( gpGlobals->curtime - m_penetrateEvents[i].timeStamp > 1.0 )
		{
			if ( m_penetrateEvents[i].collisionState == COLLSTATE_DISABLED )
			{
				if ( pEntity0 && pEntity1 )
				{
					IPhysicsObject *pObj0 = pEntity0->VPhysicsGetObject();
					IPhysicsObject *pObj1 = pEntity1->VPhysicsGetObject();
					if ( pObj0 && pObj1 )
					{
						m_penetrateEvents[i].collisionState = COLLSTATE_ENABLED;
						continue;
					}
				}
			}
			// haven't penetrated for 1 second, so remove
		}
		else
		{
			// recent timestamp, don't remove the event yet
			continue;
		}
		// done, clear event
		ks::swap_remove( m_penetrateEvents, i );
		UpdateEntityPenetrationFlag( pEntity0, false );
		UpdateEntityPenetrationFlag( pEntity1, false );
	}
}

penetrateevent_t &CCollisionEvent::FindOrAddPenetrateEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	int index = -1;
	for ( int i = std::ssize( m_penetrateEvents )-1; i >= 0; --i )
	{
		if ( m_penetrateEvents[i].hEntity0.Get() == pEntity0 && m_penetrateEvents[i].hEntity1.Get() == pEntity1 )
		{
			index = i;
			break;
		}
	}
	if ( index < 0 )
	{
		index = std::ssize( m_penetrateEvents ); m_penetrateEvents.emplace_back();
		penetrateevent_t &event = m_penetrateEvents[index];
		event.hEntity0 = pEntity0;
		event.hEntity1 = pEntity1;
		event.startTime = gpGlobals->curtime;
		event.collisionState = COLLSTATE_ENABLED;
		UpdateEntityPenetrationFlag( pEntity0, true );
		UpdateEntityPenetrationFlag( pEntity1, true );
	}
	penetrateevent_t &event = m_penetrateEvents[index];
	event.timeStamp = gpGlobals->curtime;
	return event;
}

void CCollisionEvent::RemovePenetrationEvents( CBaseEntity *pEntity )
{
	for ( int i = std::ssize( m_penetrateEvents )-1; i >= 0; --i )
	{
		penetrateevent_t &event = m_penetrateEvents[i];
		CBaseEntity *pEntity0 = event.hEntity0;
		CBaseEntity *pEntity1 = event.hEntity1;
		if ( (pEntity0 == pEntity) || (pEntity1 == pEntity) )
		{
			if ( event.collisionState == COLLSTATE_DISABLED )
			{
				IPhysicsObject *pObj0 = pEntity0->VPhysicsGetObject();
				if ( pObj0 )
				{
					pObj0->Wake();
				}

				IPhysicsObject *pObj1 = pEntity1->VPhysicsGetObject();
				if ( pObj1 )
				{
					pObj1->Wake();
				}                                              
			}

			ks::swap_remove( m_penetrateEvents, i );
			UpdateEntityPenetrationFlag( pEntity0, false );
			UpdateEntityPenetrationFlag( pEntity1, false );
		}
	}
}

static ConVar phys_penetration_error_time( side_registry(), "phys_penetration_error_time", "10", 0, "Controls the duration of vphysics penetration error boxes." );

static bool CanResolvePenetrationWithNPC( CBaseEntity *pEntity, IPhysicsObject *pObject )
{
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		// hinged objects won't be able to be pushed out anyway, so don't try the npc solver
		if ( !pObject->IsHinged() && !pObject->IsAttachedToConstraint(true) )
		{
			if ( pObject->IsMoveable() || nullptr )
				return true;
		}
	}
	return false;
}

int CCollisionEvent::ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt )
{
	CallbackContext check(this);
	
	// Pointers to the entity for each physics object
	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pGameData0);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pGameData1);

	// this can get called as entities are being constructed on the other side of a game load or level transition
	// Some entities may not be fully constructed, so don't call into their code until the level is running
	if ( g_PhysicsHook.m_bPaused )
		return true;

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
	if ( pEntity0 == pEntity1 )
	{
		if ( pObj0->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL )
		{
			DevMsg(2, "Solving ragdoll self penetration! %s (%s) (%d v %d)\n", pObj0->GetName(), pEntity0->GetDebugName(), pObj0->GetGameIndex(), pObj1->GetGameIndex() );
			ragdoll_t *pRagdoll = Ragdoll_GetRagdoll( pEntity0 );
			pRagdoll->pGroup->SolvePenetration( pObj0, pObj1 );
			return false;
		}
	}
	penetrateevent_t &event = FindOrAddPenetrateEvent( pEntity0, pEntity1 );
	float eventTime = gpGlobals->curtime - event.startTime;
	 
	// NPC vs. physics object.  Create a game DLL solver and remove this event
	
  
	if ( (IsDebris( pEntity0->GetCollisionGroup() ) && !pObj1->IsStatic()) || (IsDebris( pEntity1->GetCollisionGroup() ) && !pObj0->IsStatic()) )
	{
		if ( eventTime > 0.5f )
		{
			//Msg("Debris stuck in non-static!\n");
			event.collisionState = COLLSTATE_TRYENTITYSOLVER;
		}
	}

	if ( eventTime > 3 )
	{
		// don't report penetrations on ragdolls with themselves, or outside of developer mode
		if ( g_pDeveloper->GetInt() && pEntity0 != pEntity1 )
		{
			ReportPenetration( pEntity0, phys_penetration_error_time.GetFloat() );
			ReportPenetration( pEntity1, phys_penetration_error_time.GetFloat() );
		}
		event.startTime = gpGlobals->curtime;
		// don't put players or game physics controlled objects to sleep
		if ( !pEntity0->IsPlayer() && !pEntity1->IsPlayer() && !pObj0->GetShadowController() && !pObj1->GetShadowController() )
		{
			// two objects have been stuck for more than 3 seconds, try disabling simulation
			event.collisionState = COLLSTATE_TRYDISABLE;
			return false;
		}
	}

	return true;
}

void CCollisionEvent::FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid ) 
{
	CallbackContext check(this);
	if ( ( pObject == nullptr ) || ( pFluid == nullptr ) )
		return;

	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( !pEntity )
		return;

	pEntity->AddEFlags( EFL_TOUCHING_FLUID );
	pEntity->OnEntityEvent( ENTITY_EVENT_WATER_TOUCH, (void*)pFluid->GetContents() );

	float timeSinceLastCollision = DeltaTimeSinceLastFluid( pEntity );
	if ( timeSinceLastCollision < 0.5f )
		return;

	// UNDONE: Use this for splash logic instead?
	// UNDONE: Use angular term too - push splashes in rotAxs cross normal direction?
	Vector normal;
	float dist;
	pFluid->GetSurfacePlane( &normal, &dist );
	Vector vel;
	AngularImpulse angVel;
	pObject->GetVelocity( &vel, &angVel );
	Vector unitVel = vel;
	VectorNormalize( unitVel );
	
	// normal points out of the surface, we want the direction that points in
	float dragScale = pFluid->GetDensity() * physenv->GetSimulationTimestep();
	normal = -normal;
	float linearScale = 0.5f * DotProduct( unitVel, normal ) * pObject->CalculateLinearDrag( normal ) * dragScale;
	linearScale = clamp( linearScale, 0.0f, 1.0f );
	vel *= -linearScale;

	// UNDONE: Figure out how much of the surface area has crossed the water surface and scale angScale by that
	// For now assume 25%
	Vector rotAxis = angVel;
	VectorNormalize(rotAxis);
	float angScale = 0.25f * pObject->CalculateAngularDrag( angVel ) * dragScale;
	angScale = clamp( angScale, 0.0f, 1.0f );
	angVel *= -angScale;
	
	// compute the splash before we modify the velocity
	PhysicsSplash( pFluid, pObject, pEntity );

	// now damp out some motion toward the surface
	pObject->AddVelocity( &vel, &angVel );
}

void CCollisionEvent::FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid ) 
{
	CallbackContext check(this);
	if ( ( pObject == nullptr ) || ( pFluid == nullptr ) )
		return;

	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( !pEntity )
		return;

	float timeSinceLastCollision = DeltaTimeSinceLastFluid( pEntity );
	if ( timeSinceLastCollision >= 0.5f )
	{
		PhysicsSplash( pFluid, pObject, pEntity );
	}

	pEntity->RemoveEFlags( EFL_TOUCHING_FLUID );
	pEntity->OnEntityEvent( ENTITY_EVENT_WATER_UNTOUCH, (void*)pFluid->GetContents() );
}

class CSkipKeys : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) {}
	virtual void SetDefaults( void *pData ) {}
};

void PhysSolidOverride( solid_t &solid, string_t overrideScript )
{
	if ( overrideScript != NULL_STRING)
	{
		// parser destroys this data
		bool collisions = solid.params.enableCollisions;

		char pTmpString[4096];

		// write a header for a solid_t
		V_strncpy( pTmpString, "solid { ", sizeof(pTmpString) );

		// suck out the comma delimited tokens and turn them into quoted key/values
		char szToken[256];
		const char *pStr = nexttoken(szToken, STRING(overrideScript), ',');
		while ( szToken[0] != 0 )
		{
			V_strncat( pTmpString, "\"", sizeof(pTmpString), COPY_ALL_CHARACTERS );
			V_strncat( pTmpString, szToken, sizeof(pTmpString), COPY_ALL_CHARACTERS );
			V_strncat( pTmpString, "\" ", sizeof(pTmpString), COPY_ALL_CHARACTERS );
			pStr = nexttoken(szToken, pStr, ',');
		}
		// terminate the script
		V_strncat( pTmpString, "}", sizeof(pTmpString), COPY_ALL_CHARACTERS );

		// parse that sucker
		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pTmpString );
		CSkipKeys tmp;
		pParse->ParseSolid( &solid, &tmp );
		physcollision->VPhysicsKeyParserDestroy( pParse );

		// parser destroys this data
		solid.params.enableCollisions = collisions;
	}
}

void PhysSetMassCenterOverride( masscenteroverride_t &override )
{
	if ( override.entityName != NULL_STRING )
	{
		g_PhysicsHook.m_massCenterOverrides.push_back( override );
	}
}

// NOTE: This will remove the entry from the list as well
int PhysGetMassCenterOverrideIndex( string_t name )
{
	if ( name != NULL_STRING && std::ssize( g_PhysicsHook.m_massCenterOverrides ) )
	{
		for ( int i = 0; i < std::ssize( g_PhysicsHook.m_massCenterOverrides ); i++ )
		{
			if ( g_PhysicsHook.m_massCenterOverrides[i].entityName == name )
			{
				return i;
			}
		}
	}
	return -1;
}

void PhysGetMassCenterOverride( CBaseEntity *pEntity, vcollide_t *pCollide, solid_t &solidOut )
{
	int index = PhysGetMassCenterOverrideIndex( pEntity->GetEntityName() );

	if ( index >= 0 )
	{
		masscenteroverride_t &override = g_PhysicsHook.m_massCenterOverrides[index];
		Vector massCenterWS = override.center;
		switch ( override.alignType )
		{
		case masscenteroverride_t::ALIGN_POINT:
			VectorITransform( massCenterWS, pEntity->EntityToWorldTransform(), solidOut.massCenterOverride );
			break;
		case masscenteroverride_t::ALIGN_AXIS:
			{
				Vector massCenterLocal, defaultMassCenterWS;
				physcollision->CollideGetMassCenter( pCollide->solids[solidOut.index], &massCenterLocal );
				VectorTransform( massCenterLocal, pEntity->EntityToWorldTransform(), defaultMassCenterWS );
				massCenterWS += override.axis * 
					( DotProduct(defaultMassCenterWS,override.axis) - DotProduct( override.axis, override.center ) );
				VectorITransform( massCenterWS, pEntity->EntityToWorldTransform(), solidOut.massCenterOverride );
			}
			break;
		}
		ks::swap_remove( g_PhysicsHook.m_massCenterOverrides, index );

		if ( solidOut.massCenterOverride.Length() > DIST_EPSILON )
		{
			solidOut.params.massCenterOverride = &solidOut.massCenterOverride;
		}
	}
}

typedef void (*EntityCallbackFunction) ( CBaseEntity *pEntity );

void IterateActivePhysicsEntities( EntityCallbackFunction func )
{
	ks::ScratchArena<> scratch;
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
				func( pEntity );
			}
		}
	}
}

static void CallbackHighlight( CBaseEntity *pEntity )
{
	pEntity->m_debugOverlays |= OVERLAY_ABSBOX_BIT | OVERLAY_PIVOT_BIT;
}

static void CallbackReport( CBaseEntity *pEntity )
{
	const char *pName = STRING(pEntity->GetEntityName());
	if ( !V_strlen(pName) )
	{
		pName = STRING(pEntity->GetModelName());
	}
	Msg( "%s - %s\n", pEntity->GetClassname(), pName );
}

static void physics_highlight_active( const CCommand &args );
static ConCommand physics_highlight_active_command( side_registry(), "physics_highlight_active", physics_highlight_active, "Turns on the absbox for all active physics objects" );
static void physics_highlight_active( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	IterateActivePhysicsEntities( CallbackHighlight );
}

static void physics_report_active( const CCommand &args );
static ConCommand physics_report_active_command( side_registry(), "physics_report_active", physics_report_active, "Lists all active physics objects" );
static void physics_report_active( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	IterateActivePhysicsEntities( CallbackReport );
}

static void surfaceprop( const CCommand &args );
static ConCommand surfaceprop_command( side_registry(), "surfaceprop", surfaceprop, "Reports the surface properties at the cursor", FCVAR_CHEAT );
static void surfaceprop( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBasePlayer *pPlayer = UTIL_GetCommandClient();

	trace_t tr;
	Vector forward;
	pPlayer->EyeVectors( &forward );
	UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,
		MASK_SHOT_HULL|CONTENTS_GRATE|CONTENTS_DEBRIS, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHit() )
	{
		const model_t *pModel = modelinfo->GetModel( tr.Ent<CBaseEntity>()->GetModelIndex() );
		const char *pModelName = STRING(tr.Ent<CBaseEntity>()->GetModelName());
		if ( TraceDidHitWorld( tr ) && tr.hitbox > 0 )
		{
			ICollideable *pCollide = staticpropmgr->GetStaticPropByIndex( tr.hitbox-1 );
			pModel = pCollide->GetCollisionModel();
			pModelName = modelinfo->GetModelName( pModel );
		}
		CFmtStr modelStuff;
		if ( pModel )
		{
			modelStuff.sprintf("%s.%s ", modelinfo->IsTranslucent( pModel ) ? "Translucent" : "Opaque", 
				modelinfo->IsTranslucentTwoPass( pModel ) ? "  Two-pass." : "" );
		}
		
		// Calculate distance to surface that was hit
		Vector vecVelocity = tr.startpos - tr.endpos;
		int length = vecVelocity.Length();

		Msg("Hit surface \"%s\" (entity %s, model \"%s\" %s), texture \"%s\"\n", physprops->GetPropName( tr.surface.surfaceProps ), tr.Ent<CBaseEntity>()->GetClassname(), pModelName, modelStuff.Access(), tr.surface.name);
		Msg("Distance to surface: %d\n", length );
	}
}

static void OutputVPhysicsDebugInfo( CBaseEntity *pEntity )
{
	if ( pEntity )
	{
		Msg("Entity %s (%s) %s Collision Group %d\n", pEntity->GetClassname(), pEntity->GetDebugName(), pEntity->IsNavIgnored() ? "NAV IGNORE" : "", pEntity->GetCollisionGroup() );
		std::vector< CBaseEntity * > list;
		g_Collisions.GetListOfPenetratingEntities( pEntity, list );
		for ( int i = 0; i < std::ssize( list ); i++ )
		{
			Msg("  penetration with entity %s (%s)\n", list[i]->GetDebugName(), STRING(list[i]->GetModelName()) );
		}

		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int physCount = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
		if ( physCount )
		{
			if ( physCount > 1 )
			{
				for ( int i = 0; i < physCount; i++ )
				{
					Msg("Object %d (of %d) =========================\n", i+1, physCount );
					pList[i]->OutputDebugInfo();
				}
			}
			else
			{
				pList[0]->OutputDebugInfo();
			}
		}
	}
}

class CConstraintFloodEntry
{
public:
	CConstraintFloodEntry() : isMarked(false), isConstraint(false) {}

	std::vector< CBaseEntity * > linkList;
	bool isMarked;
	bool isConstraint;
};

class CConstraintFloodList
{
public:
	CConstraintFloodList()
	{
		m_entryList.reserve( 64 );
	}

	bool IsWorldEntity( CBaseEntity *pEnt )
	{
		if ( pEnt->edict() )
			return pEnt->IsWorld();
		return false;
	}

	void AddLink( CBaseEntity *pEntity, CBaseEntity *pLink, bool bIsConstraint )
	{
		if ( !pEntity || !pLink || IsWorldEntity(pEntity) || IsWorldEntity(pLink) )
			return;
		auto it = m_list.find(pEntity);
		if ( it == m_list.end() )
		{
			int entryIndex = std::ssize( m_entryList ); m_entryList.emplace_back();
			m_entryList[entryIndex].isConstraint = bIsConstraint;
			it = m_list.emplace( pEntity, entryIndex ).first;
		}
		int entryIndex = it->second;
		CConstraintFloodEntry &entry = m_entryList[ entryIndex ];
		contract_assert( entry.isConstraint == bIsConstraint );
		if ( ks::find_index( entry.linkList, pLink ) < 0 )
		{
			entry.linkList.push_back( pLink );
		}
	}

	void BuildGraphFromEntity( CBaseEntity *pEntity, std::vector< CBaseEntity * > &constraintList )
	{
		auto it = m_list.find(pEntity);
		if ( it != m_list.end() )
		{
			int entryIndex = it->second;
			CConstraintFloodEntry &entry = m_entryList[ entryIndex ];
			if ( !entry.isMarked )
			{
				if ( entry.isConstraint )
				{
					contract_assert( ks::find_index( constraintList, pEntity ) < 0 );
					constraintList.push_back( pEntity );
				}
				entry.isMarked = true;
				for ( int i = 0; i < std::ssize( entry.linkList ); i++ )
				{
					// now recursively traverse the graph from here
					BuildGraphFromEntity( entry.linkList[i], constraintList );
				}
			}
		}
	}
	std::map<CBaseEntity *, int>	m_list;
	std::vector< CConstraintFloodEntry > m_entryList;
};

// traverses the graph of attachments (currently supports springs & constraints) starting at an entity
// Then turns on debug info for each link in the graph (springs/constraints are links)
extern bool GetSpringAttachments( CBaseEntity *pEntity, CBaseEntity *pAttach[2], IPhysicsObject *pAttachVPhysics[2] );
extern bool GetConstraintAttachments( CBaseEntity *pEntity, CBaseEntity *pAttach[2], IPhysicsObject *pAttachVPhysics[2] );
extern void DebugConstraint(CBaseEntity *pEntity);
static void DebugConstraints( CBaseEntity *pEntity )
{

	if ( !pEntity )
		return;

	CBaseEntity *pAttach[2];
	IPhysicsObject *pAttachVPhysics[2];
	CConstraintFloodList list;

	for ( CBaseEntity *pList = gEntList.FirstEnt(); pList != nullptr; pList = gEntList.NextEnt(pList) )
	{
		if ( GetConstraintAttachments(pList, pAttach, pAttachVPhysics) || GetSpringAttachments(pList, pAttach, pAttachVPhysics) )
		{
			list.AddLink( pList, pAttach[0], true );
			list.AddLink( pList, pAttach[1], true );
			list.AddLink( pAttach[0], pList, false );
			list.AddLink( pAttach[1], pList, false );
		}
	}

	std::vector< CBaseEntity * > constraints;
	list.BuildGraphFromEntity( pEntity, constraints );
	for ( int i = 0; i < std::ssize( constraints ); i++ )
	{
		if ( !GetConstraintAttachments(constraints[i], pAttach, pAttachVPhysics) )
		{
			GetSpringAttachments(constraints[i], pAttach, pAttachVPhysics);
		}
		const char *pName0 = "world";
		const char *pName1 = "world";
		const char *pModel0 = "";
		const char *pModel1 = "";
		int index0 = 0;
		int index1 = 0;
		if ( pAttach[0] )
		{
			pName0 = pAttach[0]->GetClassname();
			pModel0 = STRING(pAttach[0]->GetModelName());
			index0 = pAttachVPhysics[0]->GetGameIndex();
		}
		if ( pAttach[1] )
		{
			pName1 = pAttach[1]->GetClassname();
			pModel1 = STRING(pAttach[1]->GetModelName());
			index1 = pAttachVPhysics[1]->GetGameIndex();
		}
		Msg("**********************\n%s connects %s(%s:%d) to %s(%s:%d)\n", constraints[i]->GetClassname(), pName0, pModel0, index0, pName1, pModel1, index1 );
		DebugConstraint(constraints[i]);
		constraints[i]->m_debugOverlays |= OVERLAY_BBOX_BIT | OVERLAY_TEXT_BIT;
	}
}

static void MarkVPhysicsDebug( CBaseEntity *pEntity )
{
	if ( pEntity )
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics )
		{
			unsigned short callbacks = pPhysics->GetCallbackFlags();
			callbacks ^= CALLBACK_MARKED_FOR_TEST;
			pPhysics->SetCallbackFlags( callbacks );
		}
	}
}

void PhysicsCommand( const CCommand &args, void (*func)( CBaseEntity *pEntity ) )
{
	if ( args.ArgC() < 2 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();

		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,
			MASK_SHOT_HULL|CONTENTS_GRATE|CONTENTS_DEBRIS, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() )
		{
			func( tr.Ent<CBaseEntity>() );
		}
	}
	else
	{
		CBaseEntity *pEnt = nullptr;
		while ( ( pEnt = gEntList.FindEntityGeneric( pEnt, args[1] ) ) != nullptr )
		{
			func( pEnt );
		}
	}
}

static void physics_constraints( const CCommand &args );
static ConCommand physics_constraints_command( side_registry(), "physics_constraints", physics_constraints, "Highlights constraint system graph for an entity" );
static void physics_constraints( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	PhysicsCommand( args, DebugConstraints );
}

static void physics_debug_entity( const CCommand &args );
static ConCommand physics_debug_entity_command( side_registry(), "physics_debug_entity", physics_debug_entity, "Dumps debug info for an entity" );
static void physics_debug_entity( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	PhysicsCommand( args, OutputVPhysicsDebugInfo );
}

static void physics_select( const CCommand &args );
static ConCommand physics_select_command( side_registry(), "physics_select", physics_select, "Dumps debug info for an entity" );
static void physics_select( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	PhysicsCommand( args, MarkVPhysicsDebug );
}

static void physics_budget( const CCommand &args );
static ConCommand physics_budget_command( side_registry(), "physics_budget", physics_budget, "Times the cost of each active object" );
static void physics_budget( const CCommand &args )
{
	ks::ScratchArena<> scratch;
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int activeCount = physenv->GetActiveObjectCount();

	IPhysicsObject **pActiveList = nullptr;
	std::vector< CBaseEntity * > ents;
	if ( activeCount )
	{
		int i;

		pActiveList = scratch.Alloc<IPhysicsObject *>( activeCount );
		physenv->GetActiveObjects( pActiveList );
		for ( i = 0; i < activeCount; i++ )
		{
			CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
			if ( pEntity )
			{
				int index = -1;
				for ( int j = 0; j < std::ssize( ents ); j++ )
				{
					if ( pEntity == ents[j] )
					{
						index = j;
						break;
					}
				}
				if ( index >= 0 )
					continue;

				ents.push_back( pEntity );
			}
		}

		if ( ents.empty() )
			return;

		std::vector< double > times;
		double totalTime = 0.f;
		g_Collisions.BufferTouchEvents( true );
		double full = Plat_FloatTime();
		{
			physenv->Simulate( DEFAULT_TICK_INTERVAL );
		}
		full = Plat_FloatTime() - full;
		double lastTime = full;

		times.assign( std::ssize( ents ), {} );

		// NOTE: This is just a heuristic.  Attempt to estimate cost by putting each object to sleep in turn.
		//	note that simulation may wake the objects again and some costs scale with sets of objects/constraints/etc
		//	so these are only generally useful for broad questions, not real metrics!
		for ( i = 0; i < std::ssize( ents ); i++ )
		{
			for ( int j = 0; j < i; j++ )
			{
				PhysForceEntityToSleep( ents[j], ents[j]->VPhysicsGetObject() );
			}
			float start = Plat_FloatTime();
			{
				physenv->Simulate( DEFAULT_TICK_INTERVAL );
			}
			double end = Plat_FloatTime();

			double elapsed = end - start;
			double avgTime = lastTime - elapsed;
			times[i] = clamp( avgTime, 0.00001, 1.0 );
			totalTime += times[i];
			lastTime = elapsed;
 		}

		totalTime = std::max( totalTime, 0.001 );
		for ( i = 0; i < std::ssize( ents ); i++ )
		{
			float fraction = times[i] / totalTime;
			Msg( "%s (%s): %.3fms (%.3f%%) @ %s\n", ents[i]->GetClassname(), ents[i]->GetDebugName(), fraction * totalTime * 1000.0f, fraction * 100.0f, VecToString(ents[i]->GetAbsOrigin()).c_str() );
		}
		g_Collisions.BufferTouchEvents( false );
	}

}

// Advance physics by time (in seconds)
void PhysFrame( float deltaTime )
{
	ks::ScratchArena<> scratch;
	static int lastObjectCount = 0;
	entitem_t *pItem;

	if ( !g_PhysicsHook.ShouldSimulate() )
		return;

	// Trap interrupts and clock changes
	if ( deltaTime > 1.0f || deltaTime < 0.0f )
	{
		deltaTime = 0;
		Msg( "Reset physics clock\n" );
	}
	else if ( deltaTime > 0.1f )	// limit incoming time to 100ms
	{
		deltaTime = 0.1f;
	}
	float simRealTime = 0;

	deltaTime *= phys_timescale.GetFloat();
	// !!!HACKHACK -- hard limit scaled time to avoid spending too much time in here
	// Limit to 100 ms
	if ( deltaTime > 0.100f )
		deltaTime = 0.100f;

	bool bProfile = phys_speeds.GetBool();

	if ( bProfile )
	{
		simRealTime = Plat_FloatTime();
	}

	if ( phys_debug_check_contacts.GetBool() )
	{
		physenv->DebugCheckContacts();
	}

	g_Collisions.BufferTouchEvents( true );

	{
		CVProfScope _( "physenv->Simulate()", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		physenv->Simulate( deltaTime );
	}

	int activeCount = physenv->GetActiveObjectCount();
	IPhysicsObject **pActiveList = nullptr;
	if ( activeCount )
	{
		CVProfScope _( "physenv->GetActiveObjects->VPhysicsUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

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
		if ( phys_show_active.GetBool() )
		{
			for ( int i = 0; i < activeCount; i++ )
			{
				CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
				if ( pEntity )
				{
					NDebugOverlay::Cross3D( pEntity->GetAbsOrigin(), 12, 255, 0, 0, false, 0 );
					NDebugOverlay::BoxAngles( pEntity->GetAbsOrigin(), pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->GetAbsAngles(), 255, 255, 0, 8, 0 );
				}
			}
		}
	}

	{
	CVProfScope _( "PhysFrame VPhysicsShadowUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	for ( pItem = g_pShadowEntities->m_pItemList; pItem; pItem = pItem->pNext )
	{
		CBaseEntity *pEntity = pItem->hEnt.Get();
		if ( !pEntity )
		{
			Msg( "Dangling pointer to physics entity!!!\n" );
			continue;
		}

		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		// apply updates
		if ( pPhysics && !pPhysics->IsAsleep() )
		{
			pEntity->VPhysicsShadowUpdate( pPhysics );
		}
	}
	}

	if ( bProfile )
	{
		simRealTime = Plat_FloatTime() - simRealTime;

		if ( simRealTime < 0 )
			simRealTime = 0;
		g_PhysAverageSimTime *= 0.8;
		g_PhysAverageSimTime += (simRealTime * 0.2);
		if ( lastObjectCount != 0 || activeCount != 0 )
		{
			Msg( "Physics: %3d objects, %4.1fms / AVG: %4.1fms\n", activeCount, simRealTime * 1000, g_PhysAverageSimTime * 1000 );
		}

		lastObjectCount = activeCount;
	}

	g_Collisions.BufferTouchEvents( false );
	g_Collisions.FrameUpdate();
}

void PhysAddShadow( CBaseEntity *pEntity )
{
	g_pShadowEntities->AddEntity( pEntity );
}

void PhysRemoveShadow( CBaseEntity *pEntity )
{
	g_pShadowEntities->DeleteEntity( pEntity );
}

bool PhysHasShadow( CBaseEntity *pEntity )
{
	EHANDLE hTestEnt = pEntity;
	entitem_t *pCurrent = g_pShadowEntities->m_pItemList;
	while( pCurrent )
	{
		if( pCurrent->hEnt == hTestEnt )
		{
			return true;
		}
		pCurrent = pCurrent->pNext;
	}
	return false;
}

void PhysEnableFloating( IPhysicsObject *pObject, bool bEnable )
{
	if ( pObject != nullptr )
	{
		unsigned short flags = pObject->GetCallbackFlags();
		if ( bEnable )
		{
			flags |= CALLBACK_DO_FLUID_SIMULATION;
		}
		else
		{
			flags &= ~CALLBACK_DO_FLUID_SIMULATION;
		}
		pObject->SetCallbackFlags( flags );
	}
}

//-----------------------------------------------------------------------------
// CollisionEvent system 
//-----------------------------------------------------------------------------
// NOTE: PreCollision/PostCollision ALWAYS come in matched pairs!!!
void CCollisionEvent::PreCollision( vcollisionevent_t *pEvent )
{
	CallbackContext check(this);
	m_gameEvent.Init( pEvent );

	// gather the pre-collision data that the game needs to track
	for ( int i = 0; i < 2; i++ )
	{
		IPhysicsObject *pObject = pEvent->pObjects[i];
		if ( pObject )
		{
			if ( pObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
			{
				CBaseEntity *pOtherEntity = reinterpret_cast<CBaseEntity *>(pEvent->pObjects[!i]->GetGameData());
				if ( pOtherEntity && !pOtherEntity->IsPlayer() )
				{
					Vector velocity;
					AngularImpulse angVel;
					// HACKHACK: If we totally clear this out, then Havok will think the objects
					// are penetrating and generate forces to separate them
					// so make it fairly small and have a tiny collision instead.
					pObject->GetVelocity( &velocity, &angVel );
					float len = VectorNormalize(velocity);
					len = std::max<std::common_type_t<float, int>>( len, 10 );
					velocity *= len;
					len = VectorNormalize(angVel);
					len = std::max<std::common_type_t<float, int>>( len, 1 );
					angVel *= len;
					pObject->SetVelocity( &velocity, &angVel );
				}
			}
			pObject->GetVelocity( &m_gameEvent.preVelocity[i], &m_gameEvent.preAngularVelocity[i] );
		}
	}
}

void CCollisionEvent::PostCollision( vcollisionevent_t *pEvent )
{
	CallbackContext check(this);
	bool isShadow[2] = {false,false};
	int i;

	for ( i = 0; i < 2; i++ )
	{
		IPhysicsObject *pObject = pEvent->pObjects[i];
		if ( pObject )
		{
			CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pObject->GetGameData());
			if ( !pEntity )
				return;

			// UNDONE: This is here to trap crashes due to NULLing out the game data on delete
			m_gameEvent.pEntities[i] = pEntity;
			unsigned int flags = pObject->GetCallbackFlags();
			pObject->GetVelocity( &m_gameEvent.postVelocity[i], nullptr );
			if ( flags & CALLBACK_SHADOW_COLLISION )
			{
				isShadow[i] = true;
			}

			// Shouldn't get impacts with triggers
			contract_assert( !pObject->IsTrigger() );
		}
	}

	// copy off the post-collision variable data
	m_gameEvent.collisionSpeed = pEvent->collisionSpeed;
	m_gameEvent.pInternalData = pEvent->pInternalData;

	// special case for hitting self, only make one non-shadow call
	if ( m_gameEvent.pEntities[0] == m_gameEvent.pEntities[1] )
	{
		if ( pEvent->isCollision && m_gameEvent.pEntities[0] )
		{
			m_gameEvent.pEntities[0]->VPhysicsCollision( 0, &m_gameEvent );
		}
		return;
	}

	if ( isShadow[0] && isShadow[1] )
	{
		pEvent->isCollision = false;
	}

	for ( i = 0; i < 2; i++ )
	{
		if ( pEvent->isCollision )
		{
			m_gameEvent.pEntities[i]->VPhysicsCollision( i, &m_gameEvent );
		}
		if ( pEvent->isShadowCollision && isShadow[i] )
		{
			m_gameEvent.pEntities[i]->VPhysicsShadowCollision( i, &m_gameEvent );
		}
	}
}

void PhysForceEntityToSleep( CBaseEntity *pEntity, IPhysicsObject *pObject )
{
	// UNDONE: Check to see if the object is touching the player first?
	// Might get the player stuck?
	if ( !pObject || !pObject->IsMoveable() )
		return;

	DevMsg(2, "Putting entity to sleep: %s\n", pEntity->GetClassname() );
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int physCount = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
	for ( int i = 0; i < physCount; i++ )
	{
		PhysForceClearVelocity( pList[i] );
		pList[i]->Sleep();
	}
}

void CCollisionEvent::Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData )
{
	CallbackContext check(this);
	//Get our friction information
	Vector vecPos, vecVel;
	pData->GetContactPoint( vecPos );
	pObject->GetVelocityAtPoint( vecPos, &vecVel );

	CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pObject->GetGameData());
		
	if ( pEntity  )
	{
		friction_t *pFriction = g_Collisions.FindFriction( pEntity );

		if ( pFriction && pFriction->pObject) 
		{
			// in MP mode play sound and effects once every 500 msecs,
			// no ongoing updates, takes too much bandwidth
			if ( (pFriction->flLastEffectTime + 0.5f) > gpGlobals->curtime)
			{
				pFriction->flLastUpdateTime = gpGlobals->curtime;
				return; 			
			}
		}

		pEntity->VPhysicsFriction( pObject, energy, surfaceProps, surfacePropsHit );
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

void CCollisionEvent::UpdateRemoveObjects()
{
	contract_assert( !PhysIsInCallback() );
	for ( int i = 0 ; i < std::ssize( m_removeObjects ); i++ )
	{
		UTIL_Remove(m_removeObjects[i]);
	}
	m_removeObjects.clear();
}

void CCollisionEvent::PostSimulationFrame()
{
	UpdateDamageEvents();
	g_PostSimulationQueue.CallQueued();
	UpdateRemoveObjects();
}

void CCollisionEvent::FlushQueuedOperations()
{
	int loopCount = 0;
	while ( loopCount < 20 )
	{
		int count = std::ssize( m_triggerEvents ) + std::ssize( m_touchEvents ) + std::ssize( m_damageEvents ) + std::ssize( m_removeObjects ) + g_PostSimulationQueue.Count();
		if ( !count )
			break;
		// testing, if this assert fires it proves we've fixed the crash
		// after that the assert + warning can safely be removed
		contract_assert( 0 );
		Warning("Physics queue not empty, error!\n");
		loopCount++;
		UpdateTouchEvents();
		UpdateDamageEvents();
		g_PostSimulationQueue.CallQueued();
		UpdateRemoveObjects();
	}
}

void CCollisionEvent::FrameUpdate( void )
{
	UpdateFrictionSounds();
	UpdateTouchEvents();
	UpdatePenetrateEvents();
	UpdateFluidEvents();
	UpdateDamageEvents(); // if there was no PSI in physics, we'll still need to do some of these because collisions are solved in between PSIs
	g_PostSimulationQueue.CallQueued();
	UpdateRemoveObjects();

	// There are some queued operations that must complete each frame, iterate until these are done
	FlushQueuedOperations();
}

// the delete list is getting flushed, clean up ours
void PhysOnCleanupDeleteList()
{
	g_Collisions.FlushQueuedOperations();
	if ( physenv )
	{
		physenv->CleanupDeleteList();
	}
}

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

void CCollisionEvent::DispatchStartTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1, const Vector &point, const Vector &normal )
{
	trace_t trace;
	std::memset( &trace, 0, sizeof(trace) );
	trace.endpos = point;
	trace.plane.dist = DotProduct( point, normal );
	trace.plane.normal = normal;

	// NOTE: This sets up the touch list for both entities, no call to pEntity1 is needed
	pEntity0->PhysicsMarkEntitiesAsTouchingEventDriven( pEntity1, trace );
}

void CCollisionEvent::DispatchEndTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1 )
{
	// frees the event-driven touchlinks
	pEntity0->PhysicsNotifyOtherOfUntouch( pEntity0, pEntity1 );
	pEntity1->PhysicsNotifyOtherOfUntouch( pEntity1, pEntity0 );
}

void CCollisionEvent::UpdateTouchEvents( void )
{
	int i;
	// Turn on buffering in case new touch events occur during processing
	bool bOldTouchEvents = m_bBufferTouchEvents;
	m_bBufferTouchEvents = true;
	for ( i = 0; i < std::ssize( m_touchEvents ); i++ )
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

	for ( i = 0; i < std::ssize( m_triggerEvents ); i++ )
	{
		m_currentTriggerEvent = m_triggerEvents[i];
		if ( m_currentTriggerEvent.bStart )
		{
			m_currentTriggerEvent.pTriggerEntity->StartTouch( m_currentTriggerEvent.pEntity );
		}
		else
		{
			m_currentTriggerEvent.pTriggerEntity->EndTouch( m_currentTriggerEvent.pEntity );
		}
	}
	m_triggerEvents.clear();
	m_currentTriggerEvent.Clear();
	m_bBufferTouchEvents = bOldTouchEvents;
}

void CCollisionEvent::UpdateDamageEvents( void )
{
	for ( int i = 0; i < std::ssize( m_damageEvents ); i++ )
	{
		damageevent_t &event = m_damageEvents[i];

		// Track changes in the entity's life state
		int iEntBits = event.pEntity->IsAlive() ? 0x0001 : 0;
		iEntBits |= event.pEntity->IsMarkedForDeletion() ? 0x0002 : 0;
		iEntBits |= (event.pEntity->GetSolidFlags() & FSOLID_NOT_SOLID) ? 0x0004 : 0;

		event.pEntity->TakeDamage( event.info );
		int iEntBits2 = event.pEntity->IsAlive() ? 0x0001 : 0;
		iEntBits2 |= event.pEntity->IsMarkedForDeletion() ? 0x0002 : 0;
		iEntBits2 |= (event.pEntity->GetSolidFlags() & FSOLID_NOT_SOLID) ? 0x0004 : 0;

		if ( event.bRestoreVelocity && iEntBits != iEntBits2 )
		{
			// UNDONE: Use ratio of masses to blend in a little of the collision response?
			// UNDONE: Damage for future events is already computed - it would be nice to
			//			go back and recompute it now that the values have
			//			been adjusted
			RestoreDamageInflictorState( event.pInflictorPhysics );
		}
	}
	m_damageEvents.clear();
	m_damageInflictors.clear();
}

void CCollisionEvent::RestoreDamageInflictorState( int inflictorStateIndex, float velocityBlend )
{
	inflictorstate_t &state = m_damageInflictors[inflictorStateIndex];
	if ( state.restored )
		return;

	// so we only restore this guy once
	state.restored = true;

	if ( velocityBlend > 0 )
	{
		Vector velocity;
		AngularImpulse angVel;
		state.pInflictorPhysics->GetVelocity( &velocity, &angVel );
		state.savedVelocity = state.savedVelocity*velocityBlend + velocity*(1-velocityBlend);
		state.savedAngularVelocity = state.savedAngularVelocity*velocityBlend + angVel*(1-velocityBlend);
		state.pInflictorPhysics->SetVelocity( &state.savedVelocity, &state.savedAngularVelocity );
	}

	if ( state.nextIndex >= 0 )
	{
		RestoreDamageInflictorState( state.nextIndex, velocityBlend );
	}
}

void CCollisionEvent::RestoreDamageInflictorState( IPhysicsObject *pInflictor )
{
	if ( !pInflictor )
		return;

	int index = FindDamageInflictor( pInflictor );
	if ( index >= 0 )
	{
		inflictorstate_t &state = m_damageInflictors[index];
		if ( !state.restored )
		{
			float velocityBlend = 1.0;
			float inflictorMass = state.pInflictorPhysics->GetMass();
			if ( inflictorMass < VPHYSICS_LARGE_OBJECT_MASS && !(state.pInflictorPhysics->GetGameFlags() & FVPHYSICS_DMG_SLICE) )
			{
				float otherMass = state.otherMassMax > 0 ? state.otherMassMax : 1;
				float massRatio = inflictorMass / otherMass;
				massRatio = clamp( massRatio, 0.1f, 10.0f );
				if ( massRatio < 1 )
				{
					velocityBlend = RemapVal( massRatio, 0.1, 1, 0, 0.5 );
				}
				else
				{
					velocityBlend = RemapVal( massRatio, 1.0, 10, 0.5, 1 );
				}
			}
			RestoreDamageInflictorState( index, velocityBlend );
		}
	}
}

bool CCollisionEvent::GetInflictorVelocity( IPhysicsObject *pInflictor, Vector &velocity, AngularImpulse &angVelocity )
{
	int index = FindDamageInflictor( pInflictor );
	if ( index >= 0 )
	{
		inflictorstate_t &state = m_damageInflictors[index];
		velocity = state.savedVelocity;
		angVelocity = state.savedAngularVelocity;
		return true;
	}

	return false;
}

bool PhysGetDamageInflictorVelocityStartOfFrame( IPhysicsObject *pInflictor, Vector &velocity, AngularImpulse &angVelocity )
{
	return g_Collisions.GetInflictorVelocity( pInflictor, velocity, angVelocity );
}

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

void CCollisionEvent::AddDamageEvent( CBaseEntity *pEntity, const CTakeDamageInfo &info, IPhysicsObject *pInflictorPhysics, bool bRestoreVelocity, const Vector &savedVel, const AngularImpulse &savedAngVel )
{
	if ( pEntity->IsMarkedForDeletion() )
		return;

	int iTimeBasedDamage = g_pGameRules->Damage_GetTimeBased();
	if ( !( info.GetDamageType() & (DMG_BURN | DMG_DROWN | iTimeBasedDamage | DMG_PREVENT_PHYSICS_FORCE) ) )
	{
		contract_assert( info.GetDamageForce() != vec3_origin && info.GetDamagePosition() != vec3_origin );
	}

	int index = std::ssize( m_damageEvents ); m_damageEvents.emplace_back();
	damageevent_t &event = m_damageEvents[index];
	event.pEntity = pEntity;
	event.info = info;
	event.pInflictorPhysics = pInflictorPhysics;
	event.bRestoreVelocity = bRestoreVelocity;
	if ( !pInflictorPhysics || !pInflictorPhysics->IsMoveable() )
	{
		event.bRestoreVelocity = false;
	}

	if ( event.bRestoreVelocity )
	{
		float otherMass = pEntity->VPhysicsGetObject()->GetMass();
		int inflictorIndex = FindDamageInflictor(pInflictorPhysics);
		if ( inflictorIndex >= 0 )
		{
			// if this is a bigger mass, save that info
			inflictorstate_t &state = m_damageInflictors[inflictorIndex];
			if ( otherMass > state.otherMassMax )
			{
				state.otherMassMax = otherMass;
			}

		}
		else
		{
			AddDamageInflictor( pInflictorPhysics, otherMass, savedVel, savedAngVel, true );
		}
	}

}

//-----------------------------------------------------------------------------
// Impulse events
//-----------------------------------------------------------------------------
static void PostSimulation_ImpulseEvent( IPhysicsObject *pObject, const Vector &centerForce, const AngularImpulse &centerTorque )
{
	pObject->ApplyForceCenter( centerForce );
	pObject->ApplyTorqueCenter( centerTorque );
}

void PostSimulation_SetVelocityEvent( IPhysicsObject *pPhysicsObject, const Vector &vecVelocity )
{
	pPhysicsObject->SetVelocity( &vecVelocity, nullptr );
}

void CCollisionEvent::AddRemoveObject(IServerNetworkable *pRemove)
{
	if ( pRemove && ks::find_index( m_removeObjects, pRemove ) == -1 )
	{
		m_removeObjects.push_back( pRemove );
	}
}
int CCollisionEvent::FindDamageInflictor( IPhysicsObject *pInflictorPhysics )
{
	// UNDONE: Linear search?  Probably ok with a low count here
	for ( int i = std::ssize( m_damageInflictors )-1; i >= 0; --i )
	{
		const inflictorstate_t &state = m_damageInflictors[i];
		if ( state.pInflictorPhysics == pInflictorPhysics )
			return i;
	}

	return -1;
}

int CCollisionEvent::AddDamageInflictor( IPhysicsObject *pInflictorPhysics, float otherMass, const Vector &savedVel, const AngularImpulse &savedAngVel, bool addList )
{
	// NOTE: Save off the state of the object before collision
	// restore if the impact is a kill
	// UNDONE: Should we absorb some energy here?
	// NOTE: we can't save a delta because there could be subsequent post-fatal collisions

	int addIndex = std::ssize( m_damageInflictors ); m_damageInflictors.emplace_back();
	{
		inflictorstate_t &state = m_damageInflictors[addIndex];
		state.pInflictorPhysics = pInflictorPhysics;
		state.savedVelocity = savedVel;
		state.savedAngularVelocity = savedAngVel;
		state.otherMassMax = otherMass;
		state.restored = false;
		state.nextIndex = -1;
	}

	if ( addList )
	{
		CBaseEntity *pEntity = static_cast<CBaseEntity *>(pInflictorPhysics->GetGameData());
		if ( pEntity )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int physCount = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
			if ( physCount > 1 )
			{
				int currentIndex = addIndex;
				for ( int i = 0; i < physCount; i++ )
				{
					if ( pList[i] != pInflictorPhysics )
					{
						Vector vel;
						AngularImpulse angVel;
						pList[i]->GetVelocity( &vel, &angVel );
						int next = AddDamageInflictor( pList[i], otherMass, vel, angVel, false );
						m_damageInflictors[currentIndex].nextIndex = next;
						currentIndex = next;
					}
				}
			}
		}
	}
	return addIndex;
}

void CCollisionEvent::LevelShutdown( void )
{
	for ( int i = 0; i < std::size( m_current ); i++ )
	{
		if ( m_current[i].patch )
		{
			ShutdownFriction( m_current[i] );
		}
	}
}

void CCollisionEvent::StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )
{
	CallbackContext check(this);
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

static int CountPhysicsObjectEntityContacts( IPhysicsObject *pObject, CBaseEntity *pEntity )
{
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	int count = 0;
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
		if ( pOtherEntity == pEntity )
			count++;
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	return count;
}

void CCollisionEvent::EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )
{
	CallbackContext check(this);
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *>(pObject1->GetGameData());
	CBaseEntity *pEntity2 = static_cast<CBaseEntity *>(pObject2->GetGameData());

	if ( !pEntity1 || !pEntity2 )
		return;

	// contact point deleted, but entities are still touching?
	IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity1->VPhysicsGetObjectList( list, std::size( list ) );

	int contactCount = 0;
	for ( int i = 0; i < count; i++ )
	{
		contactCount += CountPhysicsObjectEntityContacts( list[i], pEntity2 );
		
		// still touching
		if ( contactCount > 1 )
			return;
	}

	// should have exactly one contact point (the one getting deleted here)
	//Assert( contactCount == 1 );

	Vector endPoint, normal;
	pTouchData->GetContactPoint( endPoint );
	pTouchData->GetSurfaceNormal( normal );

	if ( !m_bBufferTouchEvents )
	{
		DispatchEndTouch( pEntity1, pEntity2 );
	}
	else
	{
		AddTouchEvent( pEntity1, pEntity2, TOUCH_END, vec3_origin, vec3_origin );
	}
}

// UNDONE: This is functional, but minimally.
void CCollisionEvent::ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject )
{
	CBaseEntity *pTriggerEntity = static_cast<CBaseEntity *>(pTrigger->GetGameData());
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( pTriggerEntity && pEntity )
	{
		// UNDONE: Don't buffer these until we can solve generating touches at object creation time
		if ( 0 && m_bBufferTouchEvents )
		{
			int index = std::ssize( m_triggerEvents ); m_triggerEvents.emplace_back();
			m_triggerEvents[index].Init( pTriggerEntity, pTrigger, pEntity, pObject, true );
		}
		else
		{
			CallbackContext check(this);
			m_currentTriggerEvent.Init( pTriggerEntity, pTrigger, pEntity, pObject, true ); 
			pTriggerEntity->StartTouch( pEntity );
			m_currentTriggerEvent.Clear();
		}
	}
}

void CCollisionEvent::ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject )
{
	CBaseEntity *pTriggerEntity = static_cast<CBaseEntity *>(pTrigger->GetGameData());
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());
	if ( pTriggerEntity && pEntity )
	{
		// UNDONE: Don't buffer these until we can solve generating touches at object creation time
		if ( 0 && m_bBufferTouchEvents )
		{
			int index = std::ssize( m_triggerEvents ); m_triggerEvents.emplace_back();
			m_triggerEvents[index].Init( pTriggerEntity, pTrigger, pEntity, pObject, false );
		}
		else
		{
			CallbackContext check(this);
			m_currentTriggerEvent.Init( pTriggerEntity, pTrigger, pEntity, pObject, false ); 
			pTriggerEntity->EndTouch( pEntity );
			m_currentTriggerEvent.Clear();
		}
	}
}

bool CCollisionEvent::GetTriggerEvent( triggerevent_t *pEvent, CBaseEntity *pTriggerEntity )
{
	if ( pEvent && pTriggerEntity == m_currentTriggerEvent.pTriggerEntity )
	{
		*pEvent = m_currentTriggerEvent;
		return true;
	}

	return false;
}

void PhysGetListOfPenetratingEntities( CBaseEntity *pSearch, std::vector< CBaseEntity * > &list )
{
	g_Collisions.GetListOfPenetratingEntities( pSearch, list );
}

bool PhysGetTriggerEvent( triggerevent_t *pEvent, CBaseEntity *pTriggerEntity )
{
	return g_Collisions.GetTriggerEvent( pEvent, pTriggerEntity );
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// External interface to collision sounds
//-----------------------------------------------------------------------------

void PhysicsImpactSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, int channel, int surfaceProps, int surfacePropsHit, float volume, float impactSpeed )
{
	physicssound::AddImpactSound( g_PhysicsHook.m_impactSounds, pEntity, pEntity->entindex(), channel, pPhysObject, surfaceProps, surfacePropsHit, volume, impactSpeed );
}

void PhysCollisionSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, int channel, int surfaceProps, int surfacePropsHit, float deltaTime, float speed )
{
	if ( deltaTime < 0.05f || speed < 70.0f )
		return;

	float volume = speed * speed * (1.0f/(320.0f*320.0f));	// max volume at 320 in/s
	if ( volume > 1.0f )
		volume = 1.0f;

	PhysicsImpactSound( pEntity, pPhysObject, channel, surfaceProps, surfacePropsHit, volume, speed );
}

void PhysBreakSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, Vector vecOrigin )
{
	if ( !pPhysObject)
		return;

	physicssound::AddBreakSound( g_PhysicsHook.m_breakSounds, vecOrigin, pPhysObject->GetMaterialIndex() );
}

ConVar collision_shake_amp(side_registry(), "collision_shake_amp", "0.2");
ConVar collision_shake_freq(side_registry(), "collision_shake_freq", "0.5");
ConVar collision_shake_time(side_registry(), "collision_shake_time", "0.5");
				 
void PhysCollisionScreenShake( gamevcollisionevent_t *pEvent, int index )
{
	int otherIndex = !index;
	float mass = pEvent->pObjects[index]->GetMass();
	if ( mass >= VPHYSICS_LARGE_OBJECT_MASS && pEvent->pObjects[otherIndex]->IsStatic() && 
		!(pEvent->pObjects[index]->GetGameFlags() & FVPHYSICS_PENETRATING) )
	{
		mass = clamp(mass, VPHYSICS_LARGE_OBJECT_MASS, 2000);
		if ( pEvent->collisionSpeed > 30 && pEvent->deltaCollisionTime > 0.25f )
		{
			Vector vecPos;
			pEvent->pInternalData->GetContactPoint( vecPos );
			float impulse = pEvent->collisionSpeed * mass;
			float amplitude = impulse * (collision_shake_amp.GetFloat() / (30.0f * VPHYSICS_LARGE_OBJECT_MASS));
			UTIL_ScreenShake( vecPos, amplitude, collision_shake_freq.GetFloat(), collision_shake_time.GetFloat(), amplitude * 60, SHAKE_START );
		}
	}
}

void PhysCollisionDust( gamevcollisionevent_t *pEvent, surfacedata_t *phit )
{

	switch ( phit->game.material )
	{
	case CHAR_TEX_SAND:
	case CHAR_TEX_DIRT:

		if ( pEvent->collisionSpeed < 200.0f )
			return;
		
		break;

	case CHAR_TEX_CONCRETE:

		if ( pEvent->collisionSpeed < 340.0f )
			return;

		break;

	default:
		return;
	}

	//Kick up dust
	Vector	vecPos, vecVel, vecNormal;
	QAngle angNormal;

	pEvent->pInternalData->GetContactPoint( vecPos );
	pEvent->pInternalData->GetSurfaceNormal( vecNormal );
	VectorAngles( vecNormal, angNormal );
	vecVel = Vector ( pEvent->collisionSpeed, pEvent->collisionSpeed, pEvent->collisionSpeed );

	DispatchParticleEffect( "impact_physics_dust", vecPos, vecVel, angNormal );

}

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, const char *pSoundName, HSOUNDSCRIPTHASH& handle, float flVolume )
{
	if ( !pEntity )
		return;

	// cut out the quiet sounds
	// UNDONE: Separate threshold for starting a sound vs. continuing?
	flVolume = clamp( flVolume, 0.0f, 1.0f );
	if ( flVolume > (1.0f/128.0f) ) // 0.0078125
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
			CBroadcastRecipientFilter filter;
			pFriction->patch = CSoundEnvelopeController::GetController().SoundCreate( 
				filter, pEntity->entindex(), CHAN_BODY, pSoundName, params.soundlevel );
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

//-----------------------------------------------------------------------------
// Applies force impulses at a later time
//-----------------------------------------------------------------------------
void PhysCallbackImpulse( IPhysicsObject *pPhysicsObject, const Vector &vecCenterForce, const AngularImpulse &vecCenterTorque )
{
	contract_assert( physenv->IsInSimulation() );
	g_PostSimulationQueue.QueueCall( PostSimulation_ImpulseEvent, pPhysicsObject, RefToVal(vecCenterForce), RefToVal(vecCenterTorque) );
}

void PhysCallbackSetVelocity( IPhysicsObject *pPhysicsObject, const Vector &vecVelocity )
{
	contract_assert( physenv->IsInSimulation() );
	g_PostSimulationQueue.QueueCall( PostSimulation_SetVelocityEvent, pPhysicsObject, RefToVal(vecVelocity) );
}

void PhysCallbackDamage( CBaseEntity *pEntity, const CTakeDamageInfo &info, gamevcollisionevent_t &event, int hurtIndex )
{
	contract_assert( physenv->IsInSimulation() );
	int otherIndex = !hurtIndex;
	g_Collisions.AddDamageEvent( pEntity, info, event.pObjects[otherIndex], true, event.preVelocity[otherIndex], event.preAngularVelocity[otherIndex] );
}

void PhysCallbackDamage( CBaseEntity *pEntity, const CTakeDamageInfo &info )
{
	if ( PhysIsInCallback() )
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		IPhysicsObject *pInflictorPhysics = (pInflictor) ? pInflictor->VPhysicsGetObject() : nullptr;
		g_Collisions.AddDamageEvent( pEntity, info, pInflictorPhysics, false, vec3_origin, vec3_origin );
		if ( pEntity && info.GetInflictor() )
		{
			DevMsg( 2, "Warning: Physics damage event with no recovery info!\nObjects: %s, %s\n", pEntity->GetClassname(), info.GetInflictor()->GetClassname() );
		}
	}
	else
	{
		pEntity->TakeDamage( info );
	}
}

void PhysCallbackRemove(IServerNetworkable *pRemove)
{
	if ( PhysIsInCallback() )
	{
		g_Collisions.AddRemoveObject(pRemove);
	}
	else
	{
		UTIL_Remove(pRemove);
	}
}

void PhysSetEntityGameFlags( CBaseEntity *pEntity, unsigned short flags )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity->VPhysicsGetObjectList( pList, std::size( pList ) );
	for ( int i = 0; i < count; i++ )
	{
		PhysSetGameFlags( pList[i], flags );
	}
}




IPhysicsObject *FindPhysicsObjectByName( const char *pName, CBaseEntity *pErrorEntity )
{
	if ( !pName || !std::strlen(pName) )
		return nullptr;

	CBaseEntity *pEntity = nullptr;
	IPhysicsObject *pBestObject = nullptr;
	while (1)
	{
		pEntity = gEntList.FindEntityByName( pEntity, pName );
		if ( !pEntity )
			break;
		if ( pEntity->VPhysicsGetObject() )
		{
			if ( pBestObject )
			{
				const char *pErrorName = pErrorEntity ? pErrorEntity->GetClassname() : "Unknown";
				Vector origin = pErrorEntity ? pErrorEntity->GetAbsOrigin() : vec3_origin;
				DevWarning("entity %s at %s has physics attachment to more than one entity with the name %s!!!\n", pErrorName, VecToString(origin).c_str(), pName );
				while ( ( pEntity = gEntList.FindEntityByName( pEntity, pName ) ) != nullptr )
				{
					DevWarning("Found %s\n", pEntity->GetClassname() );
				}
				break;

			}
			pBestObject = pEntity->VPhysicsGetObject();
		}
	}
	return pBestObject;
}

void CC_AirDensity( const CCommand &args )
{
	if ( !physenv )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg( "air_density <value>\nCurrent air density is %.2f\n", physenv->GetAirDensity() );
	}
	else
	{
		float density = std::atof( args[1] );
		physenv->SetAirDensity( density );
	}
}
static ConCommand air_density(side_registry(), "air_density", CC_AirDensity, "Changes the density of air for drag computations.", FCVAR_CHEAT);

void DebugDrawContactPoints(IPhysicsObject *pPhysics)
{
	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();

	while ( pSnapshot->IsValid() )
	{
		Vector pt, normal;
		pSnapshot->GetContactPoint( pt );
		pSnapshot->GetSurfaceNormal( normal );
		NDebugOverlay::Box( pt, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 32, 0 );
		NDebugOverlay::Line( pt, pt - normal * 20, 0, 255, 0, false, 0 );
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pOther->GetGameData());
		CFmtStr str("%s (%s): %s [%0.2f]", pEntity0->GetClassname(), STRING(pEntity0->GetModelName()), pEntity0->GetDebugName(), pSnapshot->GetFrictionCoefficient() );
		NDebugOverlay::Text( pt, str.Access(), false, 0 );
		pSnapshot->NextFrictionData();
	}
	pSnapshot->DeleteAllMarkedContacts( true );
	pPhysics->DestroyFrictionSnapshot( pSnapshot );
}

void masscenteroverride_t::SnapToPoint( string_t name, const Vector &pointWS)
{
	entityName = name;
	center = pointWS;
	axis.Init();
	alignType = ALIGN_POINT;
}

void masscenteroverride_t::SnapToAxis( string_t name, const Vector &axisStartWS, const Vector &unitAxisDirWS)
{
	entityName = name;
	center = axisStartWS;
	axis = unitAxisDirWS;
	alignType = ALIGN_AXIS;
}

class CBaseAnimating;
class CBaseEntity;
class CBoneFollower;
class CTakeDamageInfo;
struct gamevcollisionevent_t;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( physfollower_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CBoneFollowerManager )

//================================================================================================================
// BONE FOLLOWER MANAGER
//================================================================================================================
CBoneFollowerManager::CBoneFollowerManager()
{
	m_iNumBones = 0;
}

CBoneFollowerManager::~CBoneFollowerManager()
{
	// if this fires then someone isn't destroying their bonefollowers in UpdateOnRemove
	contract_assert( m_iNumBones==0 );
	DestroyBoneFollowers();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			iNumBones - 
//			**pFollowerBoneNames - 
//-----------------------------------------------------------------------------
void CBoneFollowerManager::InitBoneFollowers( CBaseAnimating *pParentEntity, int iNumBones, const char **pFollowerBoneNames )
{
	m_iNumBones = iNumBones;
	ks::ensure_size( m_physBones, iNumBones );

	// Now init all the bones
	for ( int i = 0; i < iNumBones; i++ )
	{
		CreatePhysicsFollower( pParentEntity, m_physBones[i], pFollowerBoneNames[i], nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneFollowerManager::AddBoneFollower( CBaseAnimating *pParentEntity, const char *pFollowerBoneName, solid_t *pSolid )
{
	m_iNumBones++;

	int iIndex = std::ssize( m_physBones ); m_physBones.emplace_back();
	CreatePhysicsFollower( pParentEntity, m_physBones[iIndex], pFollowerBoneName, pSolid );
}

// walk the hitboxes and find the first one that is attached to the physics bone in question
// return the hitgroup of that box
static int HitGroupFromPhysicsBone( CBaseAnimating *pAnim, int physicsBone )
{
	CStudioHdr *pStudioHdr = pAnim->GetModelPtr( );
	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnim->m_nHitboxSet );
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		if ( pStudioHdr->pBone( set->pHitbox(i)->bone )->physicsbone == physicsBone )
		{
			return set->pHitbox(i)->group;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &follow - 
//			*pBoneName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBoneFollowerManager::CreatePhysicsFollower( CBaseAnimating *pParentEntity, physfollower_t &follow, const char *pBoneName, solid_t *pSolid )
{
	CStudioHdr *pStudioHdr = pParentEntity->GetModelPtr();
	matrix3x4_t boneToWorld;
	solid_t solidTmp;

	Vector bonePosition;
	QAngle boneAngles;

	int boneIndex = Studio_BoneIndexByName( pStudioHdr, pBoneName );

	if ( boneIndex >= 0 )
	{
		const mstudiobone_t *pBone = pStudioHdr->pBone( boneIndex );

		int physicsBone = pBone->physicsbone;
		if ( !pSolid )
		{
			if ( !PhysModelParseSolidByIndex( solidTmp, pParentEntity, pParentEntity->GetModelIndex(), physicsBone ) )
				return false;
			pSolid = &solidTmp;
		}

		// fixup in case ragdoll is assigned to a parent of the requested follower bone
		follow.boneIndex = Studio_BoneIndexByName( pStudioHdr, pSolid->name );
		if ( follow.boneIndex < 0 )
		{
			follow.boneIndex = boneIndex;
		}

		pParentEntity->GetBoneTransform( follow.boneIndex, boneToWorld );
		MatrixAngles( boneToWorld, boneAngles, bonePosition );

		follow.hFollower = CBoneFollower::Create( pParentEntity, STRING(pParentEntity->GetModelName()), *pSolid, bonePosition, boneAngles );
		follow.hFollower->SetTraceData( physicsBone, HitGroupFromPhysicsBone( pParentEntity, physicsBone ) );
		follow.hFollower->SetBlocksLOS( pParentEntity->BlocksLOS() );
		return true;
	}
	else
	{
		Warning( "ERROR: Tried to create bone follower on invalid bone %s\n", pBoneName );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneFollowerManager::UpdateBoneFollowers( CBaseAnimating *pParentEntity )
{
	if ( m_iNumBones )
	{
		matrix3x4_t boneToWorld;
		Vector bonePosition;
		QAngle boneAngles;
		for ( int i = 0; i < m_iNumBones; i++ )
		{
			if ( !m_physBones[i].hFollower )
				continue;

			pParentEntity->GetBoneTransform( m_physBones[i].boneIndex, boneToWorld );
			MatrixAngles( boneToWorld, boneAngles, bonePosition );
			m_physBones[i].hFollower->UpdateFollower( bonePosition, boneAngles, 0.1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneFollowerManager::DestroyBoneFollowers( void )
{
	for ( int i = 0; i < m_iNumBones; i++ )
	{
		if ( !m_physBones[i].hFollower )
			continue;

		UTIL_Remove( m_physBones[i].hFollower );
		m_physBones[i].hFollower = nullptr;
	}

	m_physBones.clear(); m_physBones.shrink_to_fit();
	m_iNumBones = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
physfollower_t *CBoneFollowerManager::GetBoneFollower( int iFollowerIndex )
{
	contract_assert( iFollowerIndex >= 0 && iFollowerIndex < m_iNumBones );
	if ( iFollowerIndex >= 0 && iFollowerIndex < m_iNumBones )
		return &m_physBones[iFollowerIndex];
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve the index for a supplied bone follower
// Input  : *pFollower - Bone follower to look up
// Output : -1 if not found, otherwise the index of the bone follower
//-----------------------------------------------------------------------------
int CBoneFollowerManager::GetBoneFollowerIndex( CBoneFollower *pFollower )
{
	if ( pFollower == nullptr )
		return -1;

	for ( int i = 0; i < m_iNumBones; i++ )
	{
		if ( !m_physBones[i].hFollower )
			continue;

		if ( m_physBones[i].hFollower == pFollower )
			return i;
	}
	
	return -1;
}

//================================================================================================================
// BONE FOLLOWER
//================================================================================================================

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

IMPLEMENT_REFLECT_SERVERCLASS( CBoneFollower, DT_BoneFollower )

bool CBoneFollower::Init( CBaseEntity *pOwner, const char *pModelName, solid_t &solid, const Vector &position, const QAngle &orientation )
{
	SetOwnerEntity( pOwner );
	UTIL_SetModel( this, pModelName );

	AddEffects( EF_NODRAW ); // invisible

	m_modelIndex = modelinfo->GetModelIndex( pModelName );
	m_solidIndex = solid.index;
	SetAbsOrigin( position );
	SetAbsAngles( orientation );
	SetMoveType( MOVETYPE_PUSH );
	SetSolid( SOLID_VPHYSICS );
	SetCollisionGroup( pOwner->GetCollisionGroup() );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );
	solid.params.pGameData = (void *)this;
	IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false, &solid );
	if ( !pPhysics )
		return false;

	PhysSetGameFlags( pPhysics, FVPHYSICS_PUSH_PLAYER );
	// we can't use the default model bounds because each entity is only one bone of the model
	// so compute the OBB of the physics model and use that.
	Vector mins, maxs;
	physcollision->CollideGetAABB( &mins, &maxs, pPhysics->GetCollide(), vec3_origin, vec3_angle );
	SetCollisionBounds( mins, maxs );

	pPhysics->SetCallbackFlags( pPhysics->GetCallbackFlags() | CALLBACK_GLOBAL_TOUCH );
	pPhysics->EnableGravity( false );
	// This is not a normal shadow controller that is trying to go to a space occupied by an entity in the game physics
	// This entity is not running PhysicsPusher(), so Vphysics is supposed to move it
	// This line of code informs vphysics of that fact
	

	return true;
}

int CBoneFollower::UpdateTransmitState_Impl()
{
	// Send to the client for client-side collisions and visualization
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

void CBoneFollower::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	Vector origin;
	QAngle angles;

	pPhysics->GetPosition( &origin, &angles );

	SetAbsOrigin( origin );
	SetAbsAngles( angles );
}

// a little helper class to temporarily change the physics object
// for an entity - and change it back when it goes out of scope.
class CPhysicsSwapTemp
{
public:
	CPhysicsSwapTemp( CBaseEntity *pEntity, IPhysicsObject *pTmpPhysics )
	{
		contract_assert( pEntity );
		contract_assert( pTmpPhysics );
		m_pEntity = pEntity;
		m_pPhysics = m_pEntity->VPhysicsGetObject();
		if ( m_pPhysics )
		{
			m_pEntity->VPhysicsSwapObject( pTmpPhysics );
		}
		else
		{
			m_pEntity->VPhysicsSetObject( pTmpPhysics );
		}
	}
	~CPhysicsSwapTemp()
	{
		m_pEntity->VPhysicsSwapObject( m_pPhysics );
	}

private:
	CBaseEntity *m_pEntity;
	IPhysicsObject *m_pPhysics;
};

void CBoneFollower::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		CPhysicsSwapTemp tmp(pOwner, pEvent->pObjects[index] );
		pOwner->VPhysicsCollision( index, pEvent );
	}
}

void CBoneFollower::VPhysicsShadowCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		CPhysicsSwapTemp tmp(pOwner, pEvent->pObjects[index] );
		pOwner->VPhysicsShadowCollision( index, pEvent );
	}
}

void CBoneFollower::VPhysicsFriction_Impl( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		CPhysicsSwapTemp tmp(pOwner, pObject );
		pOwner->VPhysicsFriction( pObject, energy, surfaceProps, surfacePropsHit );
	}
}

bool CBoneFollower::TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( GetModelIndex() );
	contract_assert( pCollide && pCollide->solidCount > m_solidIndex );

	UTIL_ClearTrace( trace );

	physcollision->TraceBox( ray, pCollide->solids[m_solidIndex], GetAbsOrigin(), GetAbsAngles(), &trace );

	if ( trace.fraction >= 1 )
		return false;

	// return owner as trace hit
	trace.m_pEnt = GetOwnerEntity();
	trace.hitgroup = m_hitGroup;
	trace.physicsbone = m_physicsBone;
	return true;
}

void CBoneFollower::UpdateFollower( const Vector &position, const QAngle &orientation, float flInterval )
{
	// UNDONE: Shadow update needs timing info?
	VPhysicsGetObject()->UpdateShadow( position, orientation, false, flInterval );
}

void CBoneFollower::SetTraceData( int physicsBone, int hitGroup )
{
	m_hitGroup = hitGroup;
	m_physicsBone = physicsBone;
}

CBoneFollower *CBoneFollower::Create( CBaseEntity *pOwner, const char *pModelName, solid_t &solid, const Vector &position, const QAngle &orientation )
{
	CBoneFollower *pFollower = (CBoneFollower *)CreateEntityByName( "phys_bone_follower" );
	if ( pFollower )
	{
		pFollower->Init( pOwner, pModelName, solid, position, orientation );
	}
	return pFollower;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBoneFollower::ObjectCaps_Impl() 
{ 
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		if( pOwner->m_iGlobalname != NULL_STRING )
		{
			int caps = BaseClass::ObjectCaps_Impl() | pOwner->ObjectCaps();
			caps &= ~FCAP_ACROSS_TRANSITION;
			return caps;
		}
	}

	return BaseClass::ObjectCaps_Impl();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneFollower::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		pOwner->Use( pActivator, pCaller, useType, value );
		return;
	}

	BaseClass::Use_Impl( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: Pass on Touch calls to the entity we're following
//-----------------------------------------------------------------------------
void CBoneFollower::Touch_Impl( CBaseEntity *pOther )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		//TODO: fill in the touch trace with the hitbox number associated with this bone
		pOwner->Touch( pOther );
		return;
	}

	BaseClass::Touch_Impl( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Pass on trace attack calls to the entity we're following
//-----------------------------------------------------------------------------
void CBoneFollower::TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		pOwner->DispatchTraceAttack( info, vecDir, ptr );
		return;
	}

	BaseClass::TraceAttack_Impl( info, vecDir, ptr );
}

LINK_ENTITY_TO_CLASS( phys_bone_follower, CBoneFollower );

// create a manager and a list of followers directly from a ragdoll
void CreateBoneFollowersFromRagdoll( CBaseAnimating *pEntity, CBoneFollowerManager *pManager, vcollide_t *pCollide )
{
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t solid;

			pParse->ParseSolid( &solid, nullptr );
			// collisions are off by default, turn them on
			solid.params.enableCollisions = true;
			solid.params.pName = STRING(pEntity->GetModelName());

			pManager->AddBoneFollower( pEntity, solid.name, &solid );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );
}

class CBaseEntity;
class CBasePlayer;
class CSteamJet;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto SF_CANNISTER_ASLEEP = 0x0001;
static constexpr auto SF_CANNISTER_EXPLODE = 0x0002;

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CThrustController )

LINK_ENTITY_TO_CLASS( physics_cannister, CPhysicsCannister );

IMPLEMENT_REFLECT_DATAMAP( CPhysicsCannister )

void CPhysicsCannister::Spawn_Impl( void )
{
	Precache();
	SetModel( STRING(GetModelName()) );
	SetBloodColor( DONT_BLEED );

	AddSolidFlags( FSOLID_CUSTOMRAYTEST );
	m_takedamage = DAMAGE_YES;
	SetNextThink( TICK_NEVER_THINK );

	if ( m_iHealth <= 0 )
		m_iHealth = 25;

	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );
	m_bFired = false;

	// not thrusting
	m_active = false;

	CreateVPhysics();
	if ( !VPhysicsGetObject() )
	{
		// must have a physics object or code will crash later
		UTIL_Remove(this);
	}
}

void CPhysicsCannister::OnRestore_Impl()
{
	BaseClass::OnRestore_Impl();
	if ( m_pController )
	{
		m_pController->SetEventHandler( &m_thruster );
	}
}

bool CPhysicsCannister::CreateVPhysics_Impl()
{
	bool asleep = HasSpawnFlags(SF_CANNISTER_ASLEEP);

	VPhysicsInitNormal( SOLID_VPHYSICS, 0, asleep );
	return true;
}

bool CPhysicsCannister::TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	Vector vecAbsMins, vecAbsMaxs;
	CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );

	if ( !IsBoxIntersectingRay( vecAbsMins, vecAbsMaxs, ray.m_Start, ray.m_Delta ) )
		return false;
	
	return BaseClass::TestCollision_Impl( ray, mask, trace );
}

Vector CPhysicsCannister::CalcLocalThrust( const Vector &offset )
{
	matrix3x4_t nozzleMatrix;
	Vector thrustDirection;

	GetAttachment( LookupAttachment("nozzle"), nozzleMatrix );
	MatrixGetColumn( nozzleMatrix, 2, thrustDirection );
	MatrixGetColumn( nozzleMatrix, 3, m_thrustOrigin );
	thrustDirection = -5*thrustDirection + offset;
	VectorNormalize( thrustDirection );
	return thrustDirection;
}

CPhysicsCannister::~CPhysicsCannister( void )
{
}

void CPhysicsCannister::Precache_Impl( void )
{
	PropBreakablePrecacheAll( GetModelName() );
	if ( m_gasSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_gasSound) );
	}
	BaseClass::Precache_Impl();
}

int CPhysicsCannister::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	// HACKHACK: Shouldn't g_vecAttackDir be a parameter to this function?
	if ( !m_takedamage )
		return 0;

	if ( !m_active )
	{
		m_iHealth -= info.GetDamage();
		if ( m_iHealth < 0 )
		{
			Explode( info.GetAttacker() );
		}
		else
		{
			// explosions that don't destroy will activate
			// 50% of the time blunt damage will activate as well
			if ( (info.GetDamageType() & DMG_BLAST) ||
				( (info.GetDamageType() & (DMG_CLUB|DMG_SLASH|DMG_CRUSH) ) && random_valve->RandomInt(1,100) < 50 ) )
			{
				CannisterActivate( info.GetAttacker(), g_vecAttackDir );
			}
		}
		return 1;
	}

	if ( (gpGlobals->curtime - m_activateTime) <= 0.1 )
		return 0;

	if ( info.GetDamageType() & (DMG_BULLET|DMG_BUCKSHOT|DMG_BURN|DMG_BLAST) )
	{
		Explode( info.GetAttacker() );
	}

	return 0;
}

void CPhysicsCannister::TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &dir, trace_t *ptr )
{
	if ( !m_active && ptr->hitgroup != 0 )
	{
		Vector direction = -dir;
		direction.z -= 5;
		VectorNormalize( direction );
		CannisterActivate( info.GetAttacker(), direction );
	}
	BaseClass::TraceAttack_Impl( info, dir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::CannisterActivate( CBaseEntity *pActivator, const Vector &thrustOffset )
{
	// already active or spent
	if ( m_active || !m_thrustTime )
	{
		return;
	}

	m_hLauncher = pActivator;

	Vector thrustDirection = CalcLocalThrust( thrustOffset );
	m_onActivate.FireOutput( pActivator, this, 0 );
	m_thruster.CalcThrust( m_thrustOrigin, thrustDirection, VPhysicsGetObject() );
	m_pController = physenv->CreateMotionController( &m_thruster );
	IPhysicsObject *pPhys = VPhysicsGetObject();
	m_pController->AttachObject( pPhys, true );
	// Make sure the object is simulated
	pPhys->Wake();

	m_active = true;
	m_activateTime = gpGlobals->curtime;
	SetNextThink( gpGlobals->curtime + m_thrustTime );
	SetThink( &CPhysicsCannister::BeginShutdownThink );

	QAngle angles;
	VectorAngles( -thrustDirection, angles );
	m_pJet = dynamic_cast<CSteamJet *>( CBaseEntity::Create( "env_steam", m_thrustOrigin, angles, this ) );
	m_pJet->SetParent( this );

	float extra = m_thruster.m_thrust * (1/5000.f);
	extra = clamp( extra, 0, 1 );

	m_pJet->m_SpreadSpeed = 15 * m_thruster.m_thrust * 0.001;
	m_pJet->m_Speed = 128 + 100 * extra;
	m_pJet->m_StartSize = 10;
	m_pJet->m_EndSize = 25;

	m_pJet->m_Rate = 52 + (int)extra*20;
	m_pJet->m_JetLength = 64;
	m_pJet->m_clrRender = m_clrRender;

	m_pJet->Use( this, this, USE_ON, 1 );
	if ( m_gasSound != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_ITEM;
		ep.m_pSoundName =  STRING(m_gasSound);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The cannister's been fired by a weapon, so it should stay pretty accurate
//-----------------------------------------------------------------------------
void CPhysicsCannister::CannisterFire( CBaseEntity *pActivator )
{
	m_bFired = true;

	// Increase thrust
	m_thruster.m_thrust *= 4;

	// Only last a short time
	m_thrustTime = 10.0;

	// Explode on contact
	SetTouch( &CPhysicsCannister::ExplodeTouch );

	CannisterActivate( pActivator, vec3_origin );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for activating the cannister.
//-----------------------------------------------------------------------------
void CPhysicsCannister::InputActivate( inputdata_t &data )
{
	CannisterActivate( data.pActivator, Vector(0,0.1,-0.25) );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for deactivating the cannister.
//-----------------------------------------------------------------------------
void CPhysicsCannister::InputDeactivate(inputdata_t &data)
{
	Deactivate();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for making the cannister go boom.
//-----------------------------------------------------------------------------
void CPhysicsCannister::InputExplode(inputdata_t &data)
{
	Explode( data.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for waking up the cannister if it is sleeping.
//-----------------------------------------------------------------------------
void CPhysicsCannister::InputWake( inputdata_t &data )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys != nullptr )
	{
		pPhys->Wake();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::Deactivate(void)
{
	if ( !m_pController )
		return;

	m_pController->DetachObject( VPhysicsGetObject() );
	physenv->DestroyMotionController( m_pController );
	m_pController = nullptr;
	SetNextThink( TICK_NEVER_THINK );
	m_thrustTime = 0;
	m_active = false;
	if ( m_pJet )
	{
		ShutdownJet();
	}
	if ( m_gasSound != NULL_STRING )
	{
		StopSound( entindex(), CHAN_ITEM, STRING(m_gasSound) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::Explode( CBaseEntity *pAttacker )
{
	// don't recurse
	m_takedamage = 0;
	Deactivate();

	Vector velocity;
	AngularImpulse angVelocity;
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	pPhysics->GetVelocity( &velocity, &angVelocity );
	PropBreakableCreateAll( GetModelIndex(), pPhysics, GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity, 1.0, 20, COLLISION_GROUP_DEBRIS );
	ExplosionCreate( GetAbsOrigin(), GetAbsAngles(), pAttacker, m_damage, 0, true );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Explode when I next hit a damageable entity
//-----------------------------------------------------------------------------
void CPhysicsCannister::ExplodeTouch( CBaseEntity *pOther )
{
	if ( !pOther->m_takedamage )
		return;

	Explode( m_hLauncher );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	if ( m_bFired && m_active )
	{
		int otherIndex = !index;
		CBaseEntity *pHitEntity = pEvent->pEntities[otherIndex];
		if ( pEvent->deltaCollisionTime < 0.5 && (pHitEntity == this) )
			return;

		// If we hit hard enough. explode
		if ( pEvent->collisionSpeed > 1000 )
		{
			Explode( m_hLauncher );
			return;
		}
	}

	BaseClass::VPhysicsCollision_Impl( index, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::ShutdownJet( void )
{
	g_EventQueue.AddEvent( m_pJet, "kill", 5, nullptr, nullptr );

	m_pJet->m_bEmit = false;
	m_pJet->m_Rate = 0;
	m_pJet = nullptr;
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: The think just shuts the cannister down
//-----------------------------------------------------------------------------
void CPhysicsCannister::BeginShutdownThink( void )
{
	Deactivate();
}

//-----------------------------------------------------------------------------
// Physics Attacker
//-----------------------------------------------------------------------------
void CPhysicsCannister::SetPhysicsAttacker( CBasePlayer *pEntity, float flTime )
{
	m_hPhysicsAttacker = pEntity;
	m_flLastPhysicsInfluenceTime = flTime;
}

//-----------------------------------------------------------------------------
// Purpose: Keep track of physgun influence
//-----------------------------------------------------------------------------
void CPhysicsCannister::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	SetPhysicsAttacker( pPhysGunUser, gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysicsCannister::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason )
{
	SetPhysicsAttacker( pPhysGunUser, gpGlobals->curtime );
	if ( Reason == LAUNCHED_BY_CANNON )
	{
		CannisterActivate( pPhysGunUser, vec3_origin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlayer *CPhysicsCannister::HasPhysicsAttacker_Impl( float dt )
{
	if (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime)
	{
		return m_hPhysicsAttacker;
	}
	return nullptr;
}
//-----------------------------------------------------------------------------
// Purpose: Update the visible representation of the physic system's representation of this object
//-----------------------------------------------------------------------------
void CPhysicsCannister::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate_Impl( pPhysics );

	// if this is the first time we have moved, fire our target
	if ( HasSpawnFlags( SF_CANNISTER_ASLEEP ) )
	{
		if ( !pPhysics->IsAsleep() )
		{
			m_OnAwakened.FireOutput(this, this);
			RemoveSpawnFlags( SF_CANNISTER_ASLEEP );
		}
	}
}

IMotionEvent::simresult_e CThrustController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular)
{
	angular = m_torqueVector;
	linear = m_thrustVector;
	return SIM_LOCAL_ACCELERATION;
}

void CThrustController::CalcThrust( const Vector &position, const Vector &direction, IPhysicsObject *pPhys)
{
	Vector force = direction * m_thrust * pPhys->GetMass();
	
	// Adjust for the position of the thruster -- apply proper torque)
	pPhys->CalculateVelocityOffset( force, position, &m_thrustVector, &m_torqueVector );
	pPhys->WorldToLocalVector( &m_thrustVector, m_thrustVector );
}

void CPhysicsCannister::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		pPlayer->PickupObject( this );
	}
}

bool CPhysicsCannister::ShouldPuntUseLaunchForces( PhysGunForce_t reason)
{
	if ( reason == PHYSGUN_FORCE_LAUNCHED ) 
		return (m_thrustTime!=0);
		
	return false; 
}

class CBaseEntity;

using namespace ::ks::reflect;

class CPhysicsEntitySolver : public CLogicalEntity//, public IMotionEvent
{
	DECLARE_CLASS( CPhysicsEntitySolver, CLogicalEntity );
public:
	void Init( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, float separationTime );
	static CPhysicsEntitySolver *Create( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, float separationTime );

	// CBaseEntity
	void Spawn_Impl();
	void UpdateOnRemove_Impl();
	void Think_Impl();

	// IMotionEvent
	//virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

private:
	// locals
	void ResetCancelTime();
	void BecomePenetrationSolver();
	//bool IsIntersecting();
	//bool IsTouching();

	EHANDLE						m_hMovingEntity;
	EHANDLE						m_hPhysicsBlocker;
	//IPhysicsMotionController	*m_pController;
	float						m_separationDuration;
	float						m_cancelTime;
	int							m_savedCollisionGroup;
};

LINK_ENTITY_TO_CLASS( physics_entity_solver, CPhysicsEntitySolver );

CPhysicsEntitySolver *CPhysicsEntitySolver::Create( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, float separationTime )
{
	CPhysicsEntitySolver *pSolver = (CPhysicsEntitySolver *)CBaseEntity::CreateNoSpawn( "physics_entity_solver", vec3_origin, vec3_angle, nullptr );
	pSolver->Init( pMovingEntity, pPhysicsBlocker, separationTime );
	pSolver->Spawn();
	//NDebugOverlay::EntityBounds(pNPC, 255, 255, 0, 64, 0.5f );
	return pSolver;
}

void CPhysicsEntitySolver::Init( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, float separationTime )
{
	m_hMovingEntity = pMovingEntity;
	m_hPhysicsBlocker = pPhysicsBlocker;
	//m_pController = NULL;
	m_separationDuration = separationTime;
}

void CPhysicsEntitySolver::Spawn_Impl()
{
	SetNextThink( gpGlobals->curtime + m_separationDuration );
	PhysDisableEntityCollisions( m_hMovingEntity, m_hPhysicsBlocker );
	m_savedCollisionGroup = m_hPhysicsBlocker->GetCollisionGroup();
	m_hPhysicsBlocker->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	if ( m_hPhysicsBlocker->VPhysicsGetObject() )
	{
		m_hPhysicsBlocker->VPhysicsGetObject()->RecheckContactPoints();
	}
}

void CPhysicsEntitySolver::Think_Impl()
{
	UTIL_Remove(this);
}

void CPhysicsEntitySolver::UpdateOnRemove_Impl()
{
	//physenv->DestroyMotionController( m_pController );
	//m_pController = NULL;
	CBaseEntity *pEntity = m_hMovingEntity.Get();
	CBaseEntity *pPhysics = m_hPhysicsBlocker.Get();
	if ( pEntity && pPhysics )
	{
		PhysEnableEntityCollisions( pEntity, pPhysics );
	}
	if ( pPhysics )
	{
		pPhysics->SetCollisionGroup( m_savedCollisionGroup );
	}
	BaseClass::UpdateOnRemove_Impl();
}

CBaseEntity *EntityPhysics_CreateSolver( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsObject, bool disableCollisions, float separationDuration )
{
	if ( PhysEntityCollisionsAreDisabled( pMovingEntity, pPhysicsObject ) )
		return nullptr;

	return CPhysicsEntitySolver::Create( pMovingEntity, pPhysicsObject, separationDuration );
}

class CBaseEntity;
class CEffectData;

using namespace ::ks::reflect;

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

void PhysicsSplash( IPhysicsFluidController *pFluid, IPhysicsObject *pObject, CBaseEntity *pEntity )
{
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

	CEffectData	data;

	if ( pObject->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL )
	{
		/*
		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		VectorAngles( normal, data.m_vAngles );
		data.m_flScale = random->RandomFloat( 8, 10 );

		DispatchEffect( "watersplash", data );
		
		int		splashes = 4;
		Vector	point;

		for ( int i = 0; i < splashes; i++ )
		{
			point = RandomVector( -32.0f, 32.0f );
			point[2] = 0.0f;

			point += corner[i];

			data.m_vOrigin = point;
			data.m_vNormal = normal;
			VectorAngles( normal, data.m_vAngles );
			data.m_flScale = random->RandomFloat( 4, 6 );

			DispatchEffect( "watersplash", data );
		}
		*/

		//FIXME: This code will not work correctly given how the ragdoll/fluid collision is acting currently
		return;
	}

	Vector vel;
	pObject->GetVelocity( &vel, nullptr );
	float rawSpeed = -DotProduct( normal, vel );

	// proportional to cross-sectional area times velocity squared (fluid pressure)
	float speed = rawSpeed * rawSpeed * extents[0] * extents[1] * (1.0f / 2500000.0f) * pObject->GetMass() * (0.01f);

	speed = clamp( speed, 0, 50 );

	bool bRippleOnly = false;

	// allow the entity to perform a custom splash effect
	if ( pEntity->PhysicsSplash( centerPoint, normal, rawSpeed, speed ) )
		return;

	//Deny really weak hits
	//FIXME: We still need to ripple the surface in this case
	if ( speed <= 0.35f )
	{
		if ( speed <= 0.1f )
			return;

		bRippleOnly = true;
	}
	float size = RemapVal( speed, 0.35, 50, 8, 18 );

	//Find the surface area
	float	radius = extents[0] * extents[1];
	//int	splashes = clamp ( radius / 128.0f, 1, 2 );	//One splash for every three square feet of area

	//Msg( "Speed: %.2f, Size: %.2f\n, Radius: %.2f, Splashes: %d", speed, size, radius, splashes );

	Vector point;

	data.m_fFlags = 0;
	data.m_vOrigin = centerPoint;
	data.m_vNormal = normal;
	VectorAngles( normal, data.m_vAngles );
	data.m_flScale = size + random_valve->RandomFloat( 0, 2 );
	if ( pEntity->GetWaterType() & CONTENTS_SLIME )
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}

	if ( bRippleOnly )
	{
		DispatchEffect( "waterripple", data );
	}
	else
	{
		DispatchEffect( "watersplash", data );
	}

	if ( radius > 500.0f )
	{
		int splashes = random_valve->RandomInt( 1, 4 );

		for ( int i = 0; i < splashes; i++ )
		{
			point = RandomVector( -4.0f, 4.0f );
			point[2] = 0.0f;

			point += corner[i];

			data.m_fFlags = 0;
			data.m_vOrigin = point;
			data.m_vNormal = normal;
			VectorAngles( normal, data.m_vAngles );
			data.m_flScale = size + random_valve->RandomFloat( -3, 1 );
 			if ( pEntity->GetWaterType() & CONTENTS_SLIME )
			{
				data.m_fFlags |= FX_WATER_IN_SLIME;
			}

			if ( bRippleOnly )
			{
				DispatchEffect( "waterripple", data );
			}
			else
			{
				DispatchEffect( "watersplash", data );
			}
		}
	}

	/*
	for ( i = 0; i < splashes; i++ )
	{
		point = RandomVector( -8.0f, 8.0f );
		point[2] = 0.0f;

		point += centerPoint + axes[0] * random->RandomFloat( -1, 1 ) + axes[1] * random->RandomFloat( -1, 1 );

		data.m_vOrigin = point;
		data.m_vNormal = normal;
		VectorAngles( normal, data.m_vAngles );
		data.m_flScale = size + random->RandomFloat( -2, 4 );

		DispatchEffect( "watersplash", data );
	}
	*/
}

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
struct gamevcollisionevent_t;
struct impactdamagetable_t;
struct vphysics_objectstress_t;

using namespace ::ks::reflect;

//==============================================================================================
// PLAYER PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t playerLinearTable[] =
{
	{ 150*150, 5 },
	{ 250*250, 10 },
	{ 450*450, 20 },
	{ 550*550, 50 },
	{ 700*700, 100 },
	{ 1000*1000, 500 },
};

static impactentry_t playerAngularTable[] =
{
	{ 100*100, 10 },
	{ 150*150, 20 },
	{ 200*200, 50 },
	{ 300*300, 500 },
};

impactdamagetable_t gDefaultPlayerImpactDamageTable =
{
	playerLinearTable,
	playerAngularTable,

	std::size( playerLinearTable ),
	std::size( playerAngularTable ),

	24*24.0f,	// minimum linear speed
	360*360.0f,	// minimum angular speed
	2.0f,		// can't take damage from anything under 2kg

	5.0f,		// anything less than 5kg is "small"
	5.0f,		// never take more than 5 pts of damage from anything under 5kg
	36*36.0f,	// <5kg objects must go faster than 36 in/s to do damage

	0.0f,		// large mass in kg (no large mass effects)
	1.0f,		// large mass scale
	2.0f,		// large mass falling scale
	320.0f,		// min velocity for player speed to cause damage

};

//==============================================================================================
// PLAYER-IN-VEHICLE PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t playerVehicleLinearTable[] =
{
	{ 450*450, 5 },
	{ 600*600, 10 },
	{ 700*700, 25 },
	{ 1000*1000, 50 },
	{ 1500*1500, 100 },
	{ 2000*2000, 500 },
};

static impactentry_t playerVehicleAngularTable[] =
{
	{ 100*100, 10 },
	{ 150*150, 20 },
	{ 200*200, 50 },
	{ 300*300, 500 },
};

impactdamagetable_t gDefaultPlayerVehicleImpactDamageTable =
{
	playerVehicleLinearTable,
	playerVehicleAngularTable,

	std::size( playerVehicleLinearTable ),
	std::size( playerVehicleAngularTable ),

	24*24,		// minimum linear speed
	360*360,	// minimum angular speed
	80,			// can't take damage from anything under 80 kg

	150,		// anything less than 150kg is "small"
	5,			// never take more than 5 pts of damage from anything under 150kg
	36*36,		// <150kg objects must go faster than 36 in/s to do damage

	0,			// large mass in kg (no large mass effects)
	1.0f,		// large mass scale
	1.0f,		// large mass falling scale
	0.0f,		// min vel
};

//==============================================================================================
// NPC PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t npcLinearTable[] =
{
	{ 150*150, 5 },
	{ 250*250, 10 },
	{ 350*350, 50 },
	{ 500*500, 100 },
	{ 1000*1000, 500 },
};

static impactentry_t npcAngularTable[] =
{
	{ 100*100, 10 },
	{ 150*150, 25 },
	{ 200*200, 50 },
	{ 250*250, 500 },
};

impactdamagetable_t gDefaultNPCImpactDamageTable =
{
	npcLinearTable,
	npcAngularTable,
	
	std::size( npcLinearTable ),
	std::size( npcAngularTable ),

	24*24,		// minimum linear speed squared
	360*360,	// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	2,			// can't take damage from anything under 2kg

	5,			// anything less than 5kg is "small"
	5,			// never take more than 5 pts of damage from anything under 5kg
	36*36,		// <5kg objects must go faster than 36 in/s to do damage

	VPHYSICS_LARGE_OBJECT_MASS,		// large mass in kg 
	4,			// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	5,			// large mass falling scale (emphasize falling/crushing damage over sideways impacts since the stress will kill you anyway)
	0.0f,		// min vel
};

//==============================================================================================
// GLASS DAMAGE TABLE
//==============================================================================================
static impactentry_t glassLinearTable[] =
{
	{ 25*25, 10 },
	{ 50*50, 20 },
	{ 100*100, 50 },
	{ 200*200, 75 },
	{ 500*500, 100 },
	{ 250*250, 500 },
};

static impactentry_t glassAngularTable[] =
{
	{ 50*50, 25 },
	{ 100*100, 50 },
	{ 200*200, 100 },
	{ 250*250, 500 },
};

impactdamagetable_t gGlassImpactDamageTable =
{
	glassLinearTable,
	glassAngularTable,
	
	std::size( glassLinearTable ),
	std::size( glassAngularTable ),

	8*8,		// minimum linear speed squared
	360*360,	// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	2,			// can't take damage from anything under 2kg

	1,			// anything less than 1kg is "small"
	10,			// never take more than 10 pts of damage from anything under 1kg
	8*8,		// <1kg objects must go faster than 8 in/s to do damage

	50,			// large mass in kg 
	4,			// large mass scale (anything over 50kg does 4X as much energy to read from damage table)
	0.0f,		// min vel
};

//==============================================================================================
// PHYSICS TABLE NAMES
//==============================================================================================
struct damagetable_t
{
	const char			*pszTableName;
	impactdamagetable_t	*pTable;
};

static damagetable_t gDamageTableRegistry[] =
{
	{
		"player",
		&gDefaultPlayerImpactDamageTable,
	},
	{
		"player_vehicle",
		&gDefaultPlayerVehicleImpactDamageTable,
	},
	{
		"npc",
		&gDefaultNPCImpactDamageTable,
	},
	{
		"glass",
		&gGlassImpactDamageTable,
	},
};

//==============================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float ReadDamageTable( impactentry_t *pTable, int tableCount, float impulse, bool bDebug )
{
	if ( pTable )
	{
		int i;
		for ( i = 0; i < tableCount; i++ )
		{
			if ( impulse < pTable[i].impulse )
				break;
		}
		if ( i > 0 )
		{
			i--;
			if ( bDebug )
			{
				Msg("Damage %.0f, energy %.0f\n", pTable[i].damage, FastSqrt(impulse) );
			}
			return pTable[i].damage;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CalculatePhysicsImpactDamage( int index, gamevcollisionevent_t *pEvent, const impactdamagetable_t &table, float energyScale, bool allowStaticDamage, int &damageType, bool bDamageFromHeldObjects )
{
	damageType = DMG_CRUSH;
	int otherIndex = !index;

	// UNDONE: Expose a flag for self-inflicted damage?  Can't think of a valid case so far.
	if ( pEvent->pEntities[0] == pEvent->pEntities[1] )
		return 0;

	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_NO_NPC_IMPACT_DMG )
	{
		if( pEvent->pEntities[index]->IsPlayer() )
		{
			return 0;
		}
	}

	// use implicit velocities on ragdolls since they may have high constraint velocities that aren't actually executed, just pushed through contacts
	if (( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_PART_OF_RAGDOLL) && pEvent->pEntities[index]->IsPlayer() )
	{
		pEvent->pObjects[otherIndex]->GetImplicitVelocity( &pEvent->preVelocity[otherIndex], &pEvent->preAngularVelocity[otherIndex] );
	}

	// Dissolving impact damage results in death always.
	if ( ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_DMG_DISSOLVE ) && 
			!pEvent->pEntities[index]->IsEFlagSet(EFL_NO_DISSOLVE) )
	{
		damageType |= DMG_DISSOLVE;
		return 1000;
	}

	if ( energyScale <= 0.0f )
		return 0;

	const int gameFlagsNoDamage = FVPHYSICS_CONSTRAINT_STATIC | FVPHYSICS_NO_IMPACT_DMG;

	// NOTE: Crushing damage is handled by stress calcs in vphysics update functions, this is ONLY impact damage
	// this is a non-moving object due to a constraint - no damage
	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & gameFlagsNoDamage )
		return 0;

	// If it doesn't take damage from held objects and the object is being held - no damage
	if ( !bDamageFromHeldObjects && ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
	{
		// If it doesn't take damage from held objects - no damage
		if ( !bDamageFromHeldObjects )
			return 0;
	}

	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_MULTIOBJECT_ENTITY )
	{
		// UNDONE: Add up mass here for car wheels and prop_ragdoll pieces?
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int count = pEvent->pEntities[otherIndex]->VPhysicsGetObjectList( pList, std::size( pList ) );
		for ( int i = 0; i < count; i++ )
		{
			if ( pList[i]->GetGameFlags() & gameFlagsNoDamage )
				return 0;
		}
	}

	if ( pEvent->pObjects[index]->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		// players can't damage held objects
		if ( pEvent->pEntities[otherIndex]->IsPlayer() )
			return 0;

		allowStaticDamage = false;
	}

	float otherSpeedSqr = pEvent->preVelocity[otherIndex].LengthSqr();
	float otherAngSqr = 0;
	
	// factor in angular for sharp objects
	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_DMG_SLICE )
	{
		otherAngSqr = pEvent->preAngularVelocity[otherIndex].LengthSqr();
	}

	float otherMass = pEvent->pObjects[otherIndex]->GetMass();

	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		if ( gpGlobals->maxClients == 1 )
		{
			// if the player is holding the object, use it's real mass (player holding reduced the mass)
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				otherMass = pPlayer->GetHeldObjectMass( pEvent->pObjects[otherIndex] );
			}
		}
	}

	// NOTE: sum the mass of each object in this system for the purpose of damage
	if ( pEvent->pEntities[otherIndex] && (pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_MULTIOBJECT_ENTITY) )
	{
		otherMass = PhysGetEntityMass( pEvent->pEntities[otherIndex] );
	}
	
	if ( pEvent->pObjects[otherIndex]->GetGameFlags() & FVPHYSICS_HEAVY_OBJECT )
	{
		otherMass = table.largeMassMin;
		if ( energyScale < 2.0f )
		{
			energyScale = 2.0f;
		}
	}

	// UNDONE: allowStaticDamage is a hack - work out some method for 
	// breakable props to impact the world and break!!
	if ( !allowStaticDamage )
	{
		if ( otherMass < table.minMass )
			return 0;
		// check to see if the object is small
		if ( otherMass < table.smallMassMax && otherSpeedSqr < table.smallMassMinSpeedSqr )
			return 0;
		
		if ( otherSpeedSqr < table.minSpeedSqr && otherAngSqr < table.minRotSpeedSqr )
			return 0;
	}

	// Add extra oomph for floating objects
	if ( pEvent->pEntities[index]->IsFloating() && !pEvent->pEntities[otherIndex]->IsWorld() )
	{
		if ( energyScale < 3.0f )
		{
			energyScale = 3.0f;
		}
	}

	float damage = 0;
	bool bDebug = false;//(&table == &gDefaultPlayerImpactDamageTable);

	// don't ever take spin damage from slowly spinning objects
	if ( otherAngSqr > table.minRotSpeedSqr )
	{
		Vector otherInertia = pEvent->pObjects[otherIndex]->GetInertia();
		float angularMom = DotProductAbs( otherInertia, pEvent->preAngularVelocity[otherIndex] );
		damage = ReadDamageTable( table.angularTable, table.angularCount, angularMom * energyScale, bDebug );
		if ( damage > 0 )
		{
//			Msg("Spin : %.1f, Damage %.0f\n", FastSqrt(angularMom), damage );
			damageType |= DMG_SLASH;
		}
	}
	
	float deltaV = pEvent->preVelocity[index].Length() - pEvent->postVelocity[index].Length();
	float mass = pEvent->pObjects[index]->GetMass();

	// If I lost speed, and I lost less than min velocity, then filter out this energy
	if ( deltaV > 0 && deltaV < table.myMinVelocity )
	{
		deltaV = 0;
	}
	float eliminatedEnergy = deltaV * deltaV * mass;

	deltaV = pEvent->preVelocity[otherIndex].Length() - pEvent->postVelocity[otherIndex].Length();
	float otherEliminatedEnergy = deltaV * deltaV * otherMass;
	
	// exaggerate the effects of really large objects
	if ( otherMass >= table.largeMassMin )
	{
		otherEliminatedEnergy *= table.largeMassScale;
		float dz = pEvent->preVelocity[otherIndex].z - pEvent->postVelocity[otherIndex].z;

		if ( deltaV > 0 && dz < 0 && pEvent->preVelocity[otherIndex].z < 0 )
		{
			float factor = fabs(dz / deltaV);
			otherEliminatedEnergy *= (1 + factor * (table.largeMassFallingScale - 1.0f));
		}
	}

	eliminatedEnergy += otherEliminatedEnergy;

	// now in units of this character's speed squared
	float invMass = pEvent->pObjects[index]->GetInvMass();
	if ( !pEvent->pObjects[index]->IsMoveable() )
	{
		// inv mass is zero, but impact damage is enabled on this
		// prop, so recompute:
		invMass = 1.0f / pEvent->pObjects[index]->GetMass();
	}
	else if ( pEvent->pObjects[index]->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		if ( gpGlobals->maxClients == 1 )
		{
			// if the player is holding the object, use it's real mass (player holding reduced the mass)
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				float mass = pPlayer->GetHeldObjectMass( pEvent->pObjects[index] );
				if ( mass > 0 )
				{
					invMass = 1.0f / mass;
				}
			}
		}
	}

	eliminatedEnergy *= invMass * energyScale;
	
	damage += ReadDamageTable( table.linearTable, table.linearCount, eliminatedEnergy, bDebug );

	if ( !pEvent->pObjects[otherIndex]->IsStatic() && otherMass < table.smallMassMax && table.smallMassCap > 0 )
	{
		damage = clamp( damage, 0, table.smallMassCap );
	}

	return damage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CalculateDefaultPhysicsDamage( int index, gamevcollisionevent_t *pEvent, float energyScale, bool allowStaticDamage, int &damageType, string_t iszDamageTableName, bool bDamageFromHeldObjects )
{
	// If we have a specified damage table, find it and use it instead
	if ( iszDamageTableName != NULL_STRING )
	{
		for ( int i = 0; i < std::size( gDamageTableRegistry ); i++ )
		{
			if ( !V_strcmp( gDamageTableRegistry[i].pszTableName, STRING(iszDamageTableName) ) )
				return CalculatePhysicsImpactDamage( index, pEvent, *(gDamageTableRegistry[i].pTable), energyScale, allowStaticDamage, damageType, bDamageFromHeldObjects );
		}

		Warning("Failed to find custom physics damage table name: %s\n", STRING(iszDamageTableName) );
	}

	return CalculatePhysicsImpactDamage( index, pEvent, gDefaultNPCImpactDamageTable, energyScale, allowStaticDamage, damageType, bDamageFromHeldObjects );
}

static bool IsPhysicallyControlled( CBaseEntity *pEntity, IPhysicsObject *pPhysics )
{
	bool isPhysical = false;
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		isPhysical = true;
	}
	else
	{
		if ( pPhysics->GetShadowController() )
		{
			isPhysical = pPhysics->GetShadowController()->IsPhysicallyControlled();
		}
	}
	return isPhysical;
}
float CalculateObjectStress( IPhysicsObject *pObject, CBaseEntity *pInputOwnerEntity, vphysics_objectstress_t *pOutput )
{
	std::vector< CBaseEntity * > pObjectList;
	std::vector< Vector >		objectForce;
	bool hasLargeObject = false;

	// add a slot for static objects
	pObjectList.push_back( nullptr );
	objectForce.push_back( vec3_origin );
	// add a slot for friendly objects
	pObjectList.push_back( nullptr );
	objectForce.push_back( vec3_origin );

	CBaseCombatCharacter *pBCC = pInputOwnerEntity->MyCombatCharacterPointer();

	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	float objMass = pObject->GetMass();
	while ( pSnapshot->IsValid() )
	{
		float force = pSnapshot->GetNormalForce();
		if ( force > 0.0f )
		{
			IPhysicsObject *pOther = pSnapshot->GetObject(1);
			CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
			if ( !pOtherEntity )
			{
				// object was just deleted, but we still have a contact point this frame...
				// just assume it came from the world.
				pOtherEntity = GetWorldEntity();
			}
			CBaseEntity *pOtherOwner = pOtherEntity;
			if ( pOtherEntity->GetOwnerEntity() )
			{
				pOtherOwner = pOtherEntity->GetOwnerEntity();
			}

			int outIndex = 0;
			if ( !pOther->IsMoveable() )
			{
				outIndex = 0;
			}
			// NavIgnored objects are often being pushed by a friendly
			else if ( pBCC && (pBCC->IRelationType( pOtherOwner ) == D_LI || pOtherEntity->IsNavIgnored()) )
			{
				outIndex = 1;
			}
			// player held objects do no stress
			else if ( pOther->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
			{
				outIndex = 1;
			}
			else
			{
				if ( pOther->GetMass() >= VPHYSICS_LARGE_OBJECT_MASS )
				{
					if ( pInputOwnerEntity->GetGroundEntity() != pOtherEntity)
					{
						hasLargeObject = true;
					}
				}
				// moveable, non-friendly
				
				// aggregate contacts over each object to avoid greater stress in multiple contact cases
				// NOTE: Contacts should be in order, so this shouldn't ever search, but just in case
				outIndex = std::ssize( pObjectList );
				for ( int i = std::ssize( pObjectList )-1; i >= 2; --i )
				{
					if ( pObjectList[i] == pOtherOwner )
					{
						outIndex = i;
						break;
					}
				}
				if ( outIndex == std::ssize( pObjectList ) )
				{
					pObjectList.push_back( pOtherOwner );
					objectForce.push_back( vec3_origin );
				}
			}

			if ( outIndex != 0 && pInputOwnerEntity->GetMoveType() != MOVETYPE_VPHYSICS && !IsPhysicallyControlled(pOtherEntity, pOther) )
			{
				// UNDONE: Test this!  This is to remove any shadow/shadow stress.  The game should handle this with blocked/damage
				force = 0.0f;
			}

			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );
			objectForce[outIndex] += normal * force;
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	pSnapshot = nullptr;

	// clear out all friendly force
	objectForce[1].Init();

	float sum = 0;
	Vector negativeForce = vec3_origin;
	Vector positiveForce = vec3_origin;

	contract_assert( std::ssize( pObjectList ) == std::ssize( objectForce ) );
	for ( int objectIndex = std::ssize( pObjectList )-1; objectIndex >= 0; --objectIndex )
	{
		sum += objectForce[objectIndex].Length();
		for ( int i = 0; i < 3; i++ )
		{
			if ( objectForce[objectIndex][i] < 0 )
			{
				negativeForce[i] -= objectForce[objectIndex][i];
			}
			else
			{
				positiveForce[i] += objectForce[objectIndex][i];
			}
		}
	}

	// "external" stress is two way (something pushes on the object and something else pushes back)
	// so the set of minimum values per component are the projections of the two-way force
	// "internal" stress is one way (the object is pushing against something OR something pushing back)
	// the momentum must have come from inside the object (gravity, controller, etc)
	Vector internalForce = vec3_origin;
	Vector externalForce = vec3_origin;

	for ( int i = 0; i < 3; i++ )
	{
		if ( negativeForce[i] < positiveForce[i] )
		{
			internalForce[i] = positiveForce[i] - negativeForce[i];
			externalForce[i] = negativeForce[i];
		}
		else
		{
			internalForce[i] = negativeForce[i] - positiveForce[i];
			externalForce[i] = positiveForce[i];
		}
	}

	// sum is kg in / s
	Vector gravVector;
	physenv->GetGravity( &gravVector );
	float gravity = gravVector.Length();
	if ( pInputOwnerEntity->GetMoveType() != MOVETYPE_VPHYSICS && pObject->IsMoveable() )
	{
		Vector lastVel;
		lastVel.Init();
		if ( pObject->GetShadowController() )
		{
			pObject->GetShadowController()->GetLastImpulse( &lastVel );
		}
		else 
		{
			if ( ( pObject->GetCallbackFlags() & CALLBACK_IS_PLAYER_CONTROLLER ) )
			{
				CBasePlayer *pPlayer = ToBasePlayer( pInputOwnerEntity );
				IPhysicsPlayerController *pController = pPlayer ? pPlayer->GetPhysicsController() : nullptr;
				if ( pController )
				{
					pController->GetLastImpulse( &lastVel );
				}
			}
		}
		
		// Work in progress...

		// Peek into the controller for this object.  Look at the input velocity and make sure it's all
		// accounted for in the computed stress.  If not, redistribute external to internal as it's 
		// probably being reflected in a way we can't measure here.
		float inputLen = lastVel.Length() * (1.0f / physenv->GetSimulationTimestep()) * objMass;
		if ( inputLen > 0.0f )
		{
			float internalLen = internalForce.Length();
			if ( internalLen < inputLen )
			{
				float ratio = internalLen / inputLen;
				Vector delta = internalForce * (1.0f - ratio);
				internalForce += delta;
				float deltaLen = delta.Length();
				sum -= deltaLen;
				float extLen = VectorNormalize(externalForce) - deltaLen;
				if ( extLen < 0 )
				{
					extLen = 0;
				}
				externalForce *= extLen;
			}
		}
	}

	float invGravity = gravity;
	if ( invGravity <= 0 )
	{
		invGravity = 1.0f;
	}
	else
	{
		invGravity = 1.0f / invGravity;
	}
	sum *= invGravity;
	internalForce *= invGravity;
	externalForce *= invGravity;
	if ( !pObject->IsMoveable() )
	{
		// the above algorithm will see almost all force as internal if the object is not moveable 
		// (it doesn't push on anything else, so nothing is reciprocated)
		// exceptions for friction of a single other object with multiple contact points on this object
		
		// But the game wants to see it all as external because obviously the object can't move, so it can't have
		// internal stress
		externalForce = internalForce;
		internalForce.Init();

		if ( !pObject->IsStatic() )
		{
			sum += objMass;
		}
	}
	else
	{
		// assume object is at rest
		if ( sum > objMass )
		{
			sum = objMass + (sum-objMass) * 0.5;
		}
	}

	if ( pOutput )
	{
		pOutput->exertedStress = internalForce.Length();
		pOutput->receivedStress = externalForce.Length();
		pOutput->hasNonStaticStress = std::ssize( pObjectList ) > 2 ? true : false;
		pOutput->hasLargeObjectContact = hasLargeObject;
	}

	// sum is now kg 
	return sum;
}

class CBaseEntity;
class CBasePlayer;
struct physicspushlist_t;

using namespace ::ks::reflect;

// Purpose: Physics simulation for non-havok/ipion objects

extern IGameMovement *g_pGameMovement;

extern ConVar think_limit;
ConVar vprof_think_limit( side_registry(), "vprof_think_limit", "0" );

ConVar vprof_scope_entity_thinks( side_registry(), "vprof_scope_entity_thinks", "0" );
ConVar vprof_scope_entity_gamephys( side_registry(), "vprof_scope_entity_gamephys", "0" );

ConVar	npc_vphysics	( side_registry(), "npc_vphysics","0");
//-----------------------------------------------------------------------------
// helper method for trace hull as used by physics...
//-----------------------------------------------------------------------------
static void Physics_TraceEntity( CBaseEntity* pBaseEntity, const Vector &vecAbsStart,
	const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr )
{
	// FIXME: I really am not sure the best way of doing this
	// The TraceHull code below for shots will make sure the object passes
	// through shields which do not block that damage type. It will also 
	// send messages to the shields that they've been hit.
	if (pBaseEntity->GetDamageType() != DMG_GENERIC)
	{
		GameRules()->WeaponTraceEntity( pBaseEntity, vecAbsStart, vecAbsEnd, mask, ptr );
	}
	else
	{
		UTIL_TraceEntity( pBaseEntity, vecAbsStart, vecAbsEnd, mask, ptr );

		// perform an additional trace if this is a grenade projectile hitting a player
		CBaseCSGrenadeProjectile* pGrenadeProjectile = dynamic_cast<CBaseCSGrenadeProjectile*>( pBaseEntity );

		if ( pGrenadeProjectile && ptr->startsolid && ptr->contents & CONTENTS_GRENADECLIP )
		{
			// HACK HACK: players don't collide with CONTENTS_GRENADECLIP, so it's possible (but very inadvisable) for maps to contain
			// CONTENTS_GRENADECLIP brushes that are big enough for the player to throw a grenade from INSIDE one. To account for this
			// in the simplest and most straightforward way, I'm just running the trace again to let grenades fly OUT of CONTENTS_GRENADECLIP
			// volumes, just not INTO them.
			UTIL_ClearTrace( *ptr );
			UTIL_TraceEntity( pBaseEntity, vecAbsStart, vecAbsEnd, mask & ~CONTENTS_GRENADECLIP, ptr );
		}

		if ( pGrenadeProjectile && ptr->DidHit() && ptr->Ent<CBaseEntity>() && ptr->Ent<CBaseEntity>()->IsPlayer() )
		{
			UTIL_ClearTrace( *ptr );
			//why does traceline respect hitmoxes in the mask param but traceentity and tracehull do not?
			UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, pBaseEntity, pBaseEntity->GetCollisionGroup(), ptr );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Does not change the entities velocity at all
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
static void PhysicsCheckSweep( CBaseEntity *pEntity, const Vector& vecAbsStart, const Vector &vecAbsDelta, trace_t *pTrace )
{
	unsigned int mask = pEntity->PhysicsSolidMaskForEntity();

	Vector vecAbsEnd;
	VectorAdd( vecAbsStart, vecAbsDelta, vecAbsEnd );
	
	// Set collision type
	if ( !pEntity->IsSolid() || pEntity->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS) )
	{
		if ( pEntity->GetMoveParent() )
		{
			UTIL_ClearTrace( *pTrace );
			return;
		}

		// don't collide with monsters
		mask &= ~CONTENTS_MONSTER;
	}

	Physics_TraceEntity( pEntity, vecAbsStart, vecAbsEnd, mask, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysicsPushedEntities::CPhysicsPushedEntities( void )
{
	m_flMoveTime = -1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Store off entity and copy original origin to temporary array
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::AddEntity( CBaseEntity *ent )
{
	int i = std::ssize( m_rgMoved ); m_rgMoved.emplace_back();
	m_rgMoved[i].m_pEntity = ent;
	m_rgMoved[i].m_vecStartAbsOrigin = ent->GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Unlink + relink the pusher list so we can actually do the push
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::UnlinkPusherList( int *pPusherHandles )
{
	for ( int i = std::ssize( m_rgPusher ); --i >= 0; )
	{
		pPusherHandles[i] = ::partition->HideElement( m_rgPusher[i].m_pEntity->CollisionProp()->GetPartitionHandle() );
	}
}

void CPhysicsPushedEntities::RelinkPusherList( int *pPusherHandles )
{
	for ( int i = std::ssize( m_rgPusher ); --i >= 0; )
	{
		::partition->UnhideElement( m_rgPusher[i].m_pEntity->CollisionProp()->GetPartitionHandle(), pPusherHandles[i] );
	}
}

//-----------------------------------------------------------------------------
// Compute the direction to move the rotation blocker
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::ComputeRotationalPushDirection( CBaseEntity *pBlocker, const RotatingPushMove_t &rotPushMove, Vector *pMove, CBaseEntity *pRoot )
{
	// calculate destination position
	// "start" is relative to the *root* pusher, world orientation
	Vector start = pBlocker->CollisionProp()->GetCollisionOrigin();
	if ( pRoot->GetSolid() == SOLID_VPHYSICS )
	{
		// HACKHACK: Use move dir to guess which corner of the box determines contact and rotate the box so
		// that corner remains in the same local position.
		// BUGBUG: This will break, but not as badly as the previous solution!!!
		Vector vecAbsMins, vecAbsMaxs;
		pBlocker->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
		start.x = (pMove->x < 0) ? vecAbsMaxs.x : vecAbsMins.x;
		start.y = (pMove->y < 0) ? vecAbsMaxs.y : vecAbsMins.y;
		start.z = (pMove->z < 0) ? vecAbsMaxs.z : vecAbsMins.z;
		
		CBasePlayer *pPlayer = ToBasePlayer(pBlocker);
		if ( pPlayer )
		{
			// notify the player physics code so it can use vphysics to keep players from getting stuck
			pPlayer->SetPhysicsFlag( PFLAG_GAMEPHYSICS_ROTPUSH, true );
		}
	}

	// org is pusher local coordinate of start
	Vector local;
	// transform starting point into local space
	VectorITransform( start, rotPushMove.startLocalToWorld, local );
	// rotate local org into world space at end of rotation
	Vector end;
	VectorTransform( local, rotPushMove.endLocalToWorld, end );

	// move is the difference (in world space) that the move will push this object
	VectorSubtract( end, start, *pMove );
}

class CTraceFilterPushFinal : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterPushFinal, CTraceFilterSimple );

	CBaseEntity *m_pOrigEntity;
	bool m_bIgnoreTeammates;

public:
	CTraceFilterPushFinal( CBaseEntity *pEntity, int nCollisionGroup, bool bIgnoreTeammates ) 
		: CTraceFilterSimple( pEntity, nCollisionGroup )
		, m_pOrigEntity( pEntity )
		, m_bIgnoreTeammates( bIgnoreTeammates )
	{
		contract_assert( m_pOrigEntity != nullptr );
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pTestEntity = static_cast<CBaseEntity*>(pHandleEntity);

		if ( m_bIgnoreTeammates )
		{
			contract_assert( pTestEntity && m_pOrigEntity );
			if ( pTestEntity->IsPlayer() && pTestEntity->GetTeamNumber() == m_pOrigEntity->GetTeamNumber() ) 
				return false;
		}

		// UNDONE: This should really filter to just the pushing entities
		if ( pTestEntity->GetMoveType() == MOVETYPE_VPHYSICS && 
			pTestEntity->VPhysicsGetObject() && pTestEntity->VPhysicsGetObject()->IsMoveable() )
			return false;

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

};

bool CPhysicsPushedEntities::IsPushedPositionValid( CBaseEntity *pBlocker, bool bIgnoreTeammates )
{
	CTraceFilterPushFinal pushFilter( pBlocker, pBlocker->GetCollisionGroup(), bIgnoreTeammates );

	trace_t trace;
	UTIL_TraceEntity( pBlocker, pBlocker->GetAbsOrigin(), pBlocker->GetAbsOrigin(), pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &trace );

	return !trace.startsolid;
}

void CPhysicsPushedEntities::TraceBlockerEntity( CBaseEntity *pBlocker, const Vector& absStart, const Vector& absEnd, bool bIgnoreTeammates, trace_t *pOutTrace )
{
	contract_assert( pOutTrace != nullptr && pBlocker != nullptr );

	CTraceFilterPushFinal pushFilter( pBlocker, pBlocker->GetCollisionGroup(), bIgnoreTeammates );
	UTIL_TraceEntity( pBlocker, absStart, absEnd, pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, pOutTrace );
}

//-----------------------------------------------------------------------------
// Try and find a location directly upwards that makes sense for the specified blocker
// This is a slightly faster specialization of the code that works along a vector.
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::FindValidLocationUpwards( float *pOutLengthUp, CBaseEntity *pBlocker, float maxDist, float slop )
{
	contract_assert( pOutLengthUp && pBlocker );

	const Vector & vBlockerOrigin = pBlocker->GetAbsOrigin();
	Vector vTraceEnd = vBlockerOrigin + Vector( 0, 0, maxDist );

	CTraceFilterPushFinal pushFilter( pBlocker, pBlocker->GetCollisionGroup(), true );

	trace_t trace;
	UTIL_TraceEntity( pBlocker, vTraceEnd, vBlockerOrigin, pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &trace );

	if ( trace.allsolid )
		return false;

	if ( trace.fraction == 1.0 )
	{
		contract_assert( !"This isn't a failure, but it means that our tracing code is disagreeing with itself." );
		( *pOutLengthUp ) = 0.0f;
		return true;
	}

	( *pOutLengthUp ) = ( trace.endpos.z - vBlockerOrigin.z ) * slop;
	return true;
}

//-----------------------------------------------------------------------------
// Find a location from the player's AbsOrigin to the specified vEndPoint that would be
// safe to snap the player to.
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::FindValidLocationAlongVector( Vector *pOutDelta, CBaseEntity *pBlocker, const Vector &vEndPoint, float slop )
{
	contract_assert( pOutDelta && pBlocker );

	const Vector & vBlockerOrigin = pBlocker->GetAbsOrigin();

	CTraceFilterPushFinal pushFilter( pBlocker, pBlocker->GetCollisionGroup(), true );

	trace_t trace;
	UTIL_TraceEntity( pBlocker, vEndPoint, vBlockerOrigin, pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &trace );

	if ( trace.allsolid )
		return false;

	if ( trace.fraction == 1.0 )
	{
		contract_assert( !"This isn't a failure, but it means that our tracing code is disagreeing with itself." );
		( *pOutDelta ) = vBlockerOrigin;
		return true;
	}

	Vector outLoc = ( trace.startpos - trace.endpos ) * ( 1 - trace.fraction );
	outLoc *= slop;
	( *pOutDelta ) = outLoc;
	return true;		
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckPush( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush, CBaseEntity *pRoot, bool bIgnoreTeammates )
{
	ks::ScratchArena<> scratch;
	CBaseEntity *pBlocker = info.m_pEntity;

	// See if it's possible to move the entity, but disable all pushers in the hierarchy first
	int *pPusherHandles = scratch.Alloc<int>( std::ssize( m_rgPusher ) );
	UnlinkPusherList( pPusherHandles );
	CTraceFilterPushMove pushFilter(pBlocker, pBlocker->GetCollisionGroup() );

	Vector pushDestPosition = pBlocker->GetAbsOrigin() + vecAbsPush;
	// TODO: Should we be passing in bIgnoreTeammates here? 
	UTIL_TraceEntity( pBlocker, pBlocker->GetAbsOrigin(), pushDestPosition, 
		pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &info.m_Trace );

	RelinkPusherList(pPusherHandles);
	info.m_bPusherIsGround = false;
	if ( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootMoveParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	Vector blockerOrigin = pBlocker->GetAbsOrigin();

	bool bIsUnblockable = (m_bIsUnblockableByPlayer && (pBlocker->IsPlayer() || nullptr)) ? true : false;
	if ( bIsUnblockable )
	{
		pBlocker->SetAbsOrigin( pushDestPosition );
	}
	else
	{
		if( pRoot && !pRoot->CanPushEntity(pBlocker) )
		{
			// Without this bit, this function assumes all pushes are valid, and it is only checking
			// to see if the pushed guy would be okay in the new location.  The first check should
			// be if we are allowed to push them at all.  (But this can't be first because other code
			// is also assuming that the only failure could be from a second entity, so we need
			// to have gone through the trace or info.trace.pEnt will crash for them.
			info.m_bBlocked = true;
			return false;
		}

		// Move the blocker into its new position
		if ( info.m_Trace.fraction )
		{
			pBlocker->SetAbsOrigin( info.m_Trace.endpos );
		}

		// We're not blocked if the blocker is point-sized or non-solid
		if ( pBlocker->IsPointSized() || !pBlocker->IsSolid() || 
			pBlocker->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		{
			return true;
		}

		if ( (!bRotationalPush) && (info.m_Trace.fraction == 1.0) )
		{
			//Assert( pBlocker->PhysicsTestEntityPosition() == false );
			if ( !IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
			{
				Warning("Interpenetrating entities! (%s and %s)\n",
					pBlocker->GetClassname(), m_rgPusher[0].m_pEntity->GetClassname() );
			}

			return true;
		}
	}

	// Check to see if we're still blocked by the pushers
	// FIXME: If the trace fraction == 0 can we early out also?
	info.m_bBlocked = !IsPushedPositionValid( pBlocker, bIgnoreTeammates );

	if ( !info.m_bBlocked )
		return true;

	// if the player is blocking the train try nudging him around to fix accumulated error
	if ( bIsUnblockable )
	{
		Vector org = pBlocker->GetAbsOrigin();
		for ( int checkCount = 0; checkCount < 4; checkCount++ )
		{
			Vector move;
			MatrixGetColumn( m_rgPusher[0].m_pEntity->EntityToWorldTransform(), checkCount>>1, move );
			
			// alternate movements 1/2" in each direction
			float factor = ( checkCount & 1 ) ? -0.5f : 0.5f;
			pBlocker->SetAbsOrigin( org + move * factor );
			info.m_bBlocked = !IsPushedPositionValid( pBlocker, bIgnoreTeammates );
			if ( !info.m_bBlocked )
			{
				DevMsg(1, "Fixing player blocking train!\n");
				return true;
			}
		}
		pBlocker->SetAbsOrigin( pushDestPosition );
		DevMsg(1, "Ignoring player blocking train!\n");
		return true;
	}
	else
	{
		// If a player is blocking us, try nudging him around to fix accumulated errors
		Vector org = pBlocker->GetAbsOrigin();
		CBaseEntity *ground = pBlocker->GetGroundEntity();
		if ( ground && !ground->IsWorld() )
		{
			Vector toCenter = ground->GetAbsOrigin() - org;
			toCenter.z = 0;
			if ( !toCenter.IsZero() )
			{
				toCenter.NormalizeInPlace();
				pBlocker->SetAbsOrigin( org + toCenter * 16.0f );
				info.m_bBlocked = !IsPushedPositionValid( pBlocker, bIgnoreTeammates );
				if ( !info.m_bBlocked )
				{
					DevMsg(1, "Fixing player blocking train by moving to center!\n");
					return true;
				}
			}
		}

		if ( pBlocker->IsPlayer() )
		{
			pBlocker->SetAbsOrigin( blockerOrigin );
			g_pGameMovement->UnblockPusher( ToBasePlayer( pBlocker ), m_rgPusher[0].m_pEntity );	// this checks validity
			info.m_bBlocked = ( pBlocker->GetAbsOrigin() == blockerOrigin ) || !IsPushedPositionValid( pBlocker, bIgnoreTeammates );
			if ( !info.m_bBlocked )
			{
				DevMsg(1, "Fixing player blocking train via gamemovement!\n");
				return true;
			}
			else
			{
				DevMsg(2, "Blocked by player on train!\n" );
				return false;
			}
		}
		else
		{
			for ( int checkCount = 0; checkCount < 4; checkCount++ )
			{
				Vector move;
				MatrixGetColumn( m_rgPusher[0].m_pEntity->EntityToWorldTransform(), checkCount>>1, move );

				// alternate movements 1/2" in each direction
				float factor = ( checkCount & 1 ) ? -0.5f : 0.5f;
				pBlocker->SetAbsOrigin( org + move * factor );
				info.m_bBlocked = !IsPushedPositionValid( pBlocker, bIgnoreTeammates );
				if ( !info.m_bBlocked )
				{
					DevMsg(1, "Fixing player blocking train!\n");
					return true;
				}
			}
		}

		pBlocker->SetAbsOrigin( org ); // restore origin...

		DevMsg(2, "Blocked by player on train!\n");
		return false;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot )
{
	// This function is overridden by CCSPhysicsPushEntities::SpeculativelyCheckRotPush, make sure to keep 
	// them in sync.
	Vector vecAbsPush;
	m_nBlocker = -1;
	for (int i = std::ssize( m_rgMoved ); --i >= 0; )
	{
		ComputeRotationalPushDirection( m_rgMoved[i].m_pEntity, rotPushMove, &vecAbsPush, pRoot );
		if (!SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, true, pRoot ))
		{
			m_nBlocker = i;
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckLinearPush( const Vector &vecAbsPush )
{
	// This function is overridden by CCSPhysicsPushEntities::SpeculativelyCheckLinearPush, make sure 
	// to keep them in sync.
	m_nBlocker = -1;
	for (int i = std::ssize( m_rgMoved ); --i >= 0; )
	{
		if (!SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, false, nullptr ))
		{
			m_nBlocker = i;
			return false;
		}
	}

	return true;
}

void CPhysicsPushedEntities::UpdatePusherPhysicsEndOfTick()
{
	int nCount = std::ssize( m_rgUpdatedPushers );
	for ( int i = 0; i < nCount; i++ )
	{
		PhysicsPusherInfo_t &info = m_rgUpdatedPushers[i];
		// Cause touch functions to be called
		// FIXME: Need to make moved entities not touch triggers until we know we're ok
		// FIXME: it'd be better for the engine to just have a touch method
		info.m_pEntity->PhysicsTouchTriggers( &info.m_vecStartAbsOrigin );
	}
	for ( int i = 0; i < std::ssize( m_rgUpdatedChildren ); i++ )
	{
		m_rgUpdatedChildren[i]->PhysicsTouchTriggers();
	}
	for ( int i = 0; i < nCount; i++ )
	{
		m_rgUpdatedPushers[i].m_pEntity->UpdatePhysicsShadowToCurrentPosition( gpGlobals->frametime );
	}
	for ( int i = 0; i < std::ssize( m_rgUpdatedChildren ); i++ )
	{
		m_rgUpdatedChildren[i]->UpdatePhysicsShadowToCurrentPosition( gpGlobals->frametime );
	}
	m_rgUpdatedPushers.clear();
	m_rgUpdatedChildren.clear();
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPushers()
{
	// We succeeded! Now that we know the final location of all entities,
	// touch triggers + update physics objects + do other fixup.
	// schedule this to happen all at once at the end of the tick
	m_rgUpdatedPushers.append_range( m_rgPusher );
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove )
{
	// Impart angular velocity of push onto pushed objects
	if ( pPushedEntity->IsPlayer() )
	{
		QAngle angVel = pPushedEntity->GetLocalAngularVelocity();
		angVel[1] = rotPushMove.amove[1];
		pPushedEntity->SetLocalAngularVelocity(angVel);

		// Look up associated client
		CBasePlayer *player = ( CBasePlayer * )pPushedEntity;
		if ( player->IsNetClient() )	// don't fixup angles on bots - they don't ever reset anglechange
		{
			player->pl.fixangle = FIXANGLE_RELATIVE;
			// Because we can run multiple ticks per server frame, accumulate a total offset here instead of straight
			//  setting it.  The engine will reset anglechange to 0 when the message is actually sent to the client
			player->pl.anglechange += rotPushMove.amove;
		}
	}
	else
	{
		QAngle angles = pPushedEntity->GetAbsAngles();
		
		// only rotate YAW with pushing.  Freely rotateable entities should either use VPHYSICS
		// or be set up as children
		angles.y += rotPushMove.amove.y;
		pPushedEntity->SetAbsAngles( angles );
	}
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPush( bool bIsRotPush, const RotatingPushMove_t *pRotPushMove )
{
	FinishPushers();

	for ( int i = std::ssize( m_rgMoved ); --i >= 0; )
	{
		PhysicsPushedInfo_t &info = m_rgMoved[i];
		CBaseEntity *pPushedEntity = info.m_pEntity;

		// Cause touch functions to be called
		// FIXME: it'd be better for the engine to just have a touch method
		info.m_pEntity->PhysicsTouchTriggers( &info.m_vecStartAbsOrigin );
		info.m_pEntity->UpdatePhysicsShadowToCurrentPosition( gpGlobals->frametime );
		

		// Register physics impacts...
		if (info.m_Trace.Ent<CBaseEntity>())
		{
			pPushedEntity->PhysicsImpact( info.m_Trace.Ent<CBaseEntity>(), info.m_Trace );
		}

		if (bIsRotPush)
		{
			FinishRotPushedEntity( pPushedEntity, *pRotPushMove );
		}
	}
}

// save initial state when beginning a push sequence
void CPhysicsPushedEntities::BeginPush( CBaseEntity *pRoot )
{
	m_rgMoved.clear();
	m_rgPusher.clear();

	m_rootPusherStartLocalOrigin = pRoot->GetLocalOrigin();
	m_rootPusherStartLocalAngles = pRoot->GetLocalAngles();
	m_rootPusherStartLocaltime = pRoot->GetLocalTime();
}

// store off a list of what has changed - so vphysicsUpdate can undo this if the object gets blocked
void CPhysicsPushedEntities::StoreMovedEntities( physicspushlist_t &list )
{
	list.localMoveTime = m_rootPusherStartLocaltime;
	list.localOrigin = m_rootPusherStartLocalOrigin;
	list.localAngles = m_rootPusherStartLocalAngles;
	list.pushedCount = CountMovedEntities();
	contract_assert( list.pushedCount < std::size( list.pushedEnts ) );
	if ( list.pushedCount > std::size( list.pushedEnts ) )
	{
		list.pushedCount = std::size( list.pushedEnts );
	}
	for ( int i = 0; i < list.pushedCount; i++ )
	{
		list.pushedEnts[i] = m_rgMoved[i].m_pEntity;
		list.pushVec[i] = m_rgMoved[i].m_pEntity->GetAbsOrigin() - m_rgMoved[i].m_vecStartAbsOrigin;
	}
}

//-----------------------------------------------------------------------------
// Registers a blockage
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::RegisterBlockage()
{
	contract_assert( m_nBlocker >= 0 );

	// Generate a PhysicsImpact against the blocker...
	PhysicsPushedInfo_t &info = m_rgMoved[m_nBlocker];
	if ( info.m_Trace.Ent<CBaseEntity>() )
	{
		info.m_pEntity->PhysicsImpact( info.m_Trace.Ent<CBaseEntity>(), info.m_Trace );
	}

	// This is the dude 
	return info.m_pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: Restore entities that might have been moved
// Input  : fromrotation - if the move is from a rotation, then angular move must also be reverted
//			*amove - 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RestoreEntities( )
{
	// Reset all of the pushed entities to get them back into place also
	for ( int i = std::ssize( m_rgMoved ); --i >= 0; )
	{
		m_rgMoved[ i ].m_pEntity->SetAbsOrigin( m_rgMoved[ i ].m_vecStartAbsOrigin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: This is a trace filter that only hits an exclusive list of entities
//-----------------------------------------------------------------------------
class CTraceFilterAgainstEntityList : public ITraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
	{
		for ( int i = std::ssize( m_entityList )-1; i >= 0; --i )
		{
			if ( m_entityList[i] == pEntity )
				return true;
		}

		return false;
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}

	void AddEntityToHit( IHandleEntity *pEntity )
	{
		m_entityList.push_back( pEntity );
	}

	std::vector< IHandleEntity * >	m_entityList;
};

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
class CPushBlockerEnum : public IPartitionEnumerator
{
public:
	CPushBlockerEnum( CPhysicsPushedEntities *pPushedEntities ) : m_pPushedEntities(pPushedEntities)
	{
		// All elements are part of the same hierarchy, so they all have
		// the same root, so it doesn't matter which one we grab
		m_pRootHighestParent = m_pPushedEntities->m_rgPusher[0].m_pEntity->GetRootMoveParent();
		++s_nEnumCount;

		m_collisionGroupCount = 0;
		for ( int i = std::ssize( m_pPushedEntities->m_rgPusher ); --i >= 0; )
		{
			if ( !m_pPushedEntities->m_rgPusher[i].m_pEntity->IsSolid() )
				continue;

			m_pushersOnly.AddEntityToHit( m_pPushedEntities->m_rgPusher[i].m_pEntity );
			int collisionGroup = m_pPushedEntities->m_rgPusher[i].m_pEntity->GetCollisionGroup();
			AddCollisionGroup(collisionGroup);
		}

	}

	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pCheck = GetPushableEntity( pHandleEntity );
		if ( !pCheck )
			return ITERATION_CONTINUE;

		// Mark it as seen
		m_pPushedEntities->AddEntity( pCheck );

		return ITERATION_CONTINUE;
	}

private:

	inline void AddCollisionGroup(int collisionGroup)
	{
		for ( int i = 0; i < m_collisionGroupCount; i++ )
		{
			if ( m_collisionGroups[i] == collisionGroup )
				return;
		}
		if ( m_collisionGroupCount < std::size( m_collisionGroups ) )
		{
			m_collisionGroups[m_collisionGroupCount] = collisionGroup;
			m_collisionGroupCount++;
		}
	}

	bool IsStandingOnPusher( CBaseEntity *pCheck )
	{
		CBaseEntity *pGroundEnt = pCheck->GetGroundEntity();
		if ( pCheck->GetFlags() & FL_ONGROUND || pGroundEnt )
		{
			for ( int i = std::ssize( m_pPushedEntities->m_rgPusher ); --i >= 0; )
			{
				if (m_pPushedEntities->m_rgPusher[i].m_pEntity == pGroundEnt)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IntersectsPushers( CBaseEntity *pTest )
	{
		trace_t tr;

		ICollideable *pCollision = pTest->GetCollideable();
		enginetrace->SweepCollideable( pCollision, pTest->GetAbsOrigin(), pTest->GetAbsOrigin(), pCollision->GetCollisionAngles(), 
			pTest->PhysicsSolidMaskForEntity(), &m_pushersOnly, &tr );

		return tr.startsolid;
	}

	CBaseEntity *GetPushableEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pCheck = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
		if ( !pCheck )
			return nullptr;

		// Don't bother if we've already seen this one...
		if (pCheck->m_nPushEnumCount == s_nEnumCount)
			return nullptr;

		if ( !pCheck->IsSolid() )
			return nullptr;

		int nMovetype = pCheck->GetMoveType();
		if ( !IsPushableMoveType( nMovetype ) )
			return nullptr;

		pCheck->m_nPushEnumCount = s_nEnumCount;
		bool bCollide = false;
		for ( int i = 0; i < m_collisionGroupCount; i++ )
		{
			if ( g_pGameRules->ShouldCollide( pCheck->GetCollisionGroup(), m_collisionGroups[i] ) )
			{
				bCollide = true;
				break;
			}
		}
		if ( !bCollide )
			return nullptr;
		// We're not pushing stuff we're hierarchically attached to
		CBaseEntity *pCheckHighestParent = pCheck->GetRootMoveParent();
		if (pCheckHighestParent == m_pRootHighestParent)
			return nullptr;

		// If we're standing on the pusher or any rigidly attached child
		// of the pusher, we don't need to bother checking for interpenetration
		if ( !IsStandingOnPusher(pCheck) )
		{
			// Our surrounding boxes are touching. But we may well not be colliding....
			// see if the ent's bbox is inside the pusher's final position
			if ( !IntersectsPushers( pCheck ) )
				return nullptr;
		}

		// NOTE: This is pretty tricky here. If a rigidly attached child comes into
		// contact with a pusher, we *cannot* push the child. Instead, we must push
		// the highest parent of that child.
		return pCheckHighestParent;
	}

private:
	static int s_nEnumCount;
	CPhysicsPushedEntities *m_pPushedEntities;
	CBaseEntity *m_pRootHighestParent;
	CTraceFilterAgainstEntityList	m_pushersOnly;
	int m_collisionGroups[8];
	int m_collisionGroupCount;
};

int CPushBlockerEnum::s_nEnumCount = 0;

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityList()
{
	CVProfScope _( "CPhysicsPushedEntities::GenerateBlockingEntityList", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	m_rgMoved.clear();
	CPushBlockerEnum blockerEnum( this );

	for ( int i = std::ssize( m_rgPusher ); --i >= 0;  )
	{
		CBaseEntity *pPusher = m_rgPusher[i].m_pEntity;

		// Don't bother if the pusher isn't solid
		if ( !pPusher->IsSolid() || pPusher->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		{
			continue;
		}

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
		::partition->EnumerateElementsInBox( PARTITION_ENGINE_PUSHABLE, vecAbsMins, vecAbsMaxs, false, &blockerEnum );

		//Go back throught the generated list.
	}
}

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityListAddBox( const Vector &vecMoved )
{
	CVProfScope _( "CPhysicsPushedEntities::GenerateBlockingEntityListAddBox", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	m_rgMoved.clear();
	CPushBlockerEnum blockerEnum( this );

	for ( int i = std::ssize( m_rgPusher ); --i >= 0;  )
	{
		CBaseEntity *pPusher = m_rgPusher[i].m_pEntity;

		// Don't bother if the pusher isn't solid
		if ( !pPusher->IsSolid() || pPusher->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		{
			continue;
		}

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			if ( vecMoved[iAxis] >= 0.0f )
			{
				vecAbsMins[iAxis] -= vecMoved[iAxis];
			}
			else
			{
				vecAbsMaxs[iAxis] -= vecMoved[iAxis];
			}
		}

		::partition->EnumerateElementsInBox( PARTITION_ENGINE_PUSHABLE, vecAbsMins, vecAbsMaxs, false, &blockerEnum );

		//Go back throught the generated list.
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a list of all entities hierarchically attached to the root 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::SetupAllInHierarchy( CBaseEntity *pRoot )
{
	if (!pRoot)
		return;

	CVProfScope _( "CPhysicsPushedEntities::SetupAllInHierarchy", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Make sure to snack the position +before+ relink because applying the
	// rotation (which occurs in relink) will put it at the final location
	// NOTE: The root object at this point is actually at its final position.
	// We'll fix that up later
	int i = std::ssize( m_rgPusher ); m_rgPusher.emplace_back();
	m_rgPusher[i].m_pEntity = pRoot;
	int nRecurseIndex = 0;
	CBaseEntity *pEntity = pRoot;

	do
	{
		for ( CBaseEntity *pChild = pEntity->FirstMoveChild(); pChild != nullptr; pChild = pChild->NextMovePeer() )
		{
			i = std::ssize( m_rgPusher ); m_rgPusher.emplace_back();
			m_rgPusher[i].m_pEntity = pChild;
		}
		nRecurseIndex++;
		if ( nRecurseIndex >= std::ssize( m_rgPusher ) )
			break;
		pEntity = m_rgPusher[nRecurseIndex].m_pEntity;
	} while ( true );

	for ( i = 0; i < std::ssize( m_rgPusher ); i++ )
	{
		m_rgPusher[i].m_vecStartAbsOrigin = m_rgPusher[i].m_pEntity->GetAbsOrigin();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rotates the root entity, fills in the pushmove structure
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RotateRootEntity( CBaseEntity *pRoot, float movetime, RotatingPushMove_t &rotation )
{
	CVProfScope _( "CPhysicsPushedEntities::RotateRootEntity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	rotation.amove = pRoot->GetLocalAngularVelocity() * movetime;
	rotation.origin = pRoot->GetAbsOrigin();

	// Knowing the initial + ending basis is needed for determining
	// which corner we're pushing 
	MatrixCopy( pRoot->EntityToWorldTransform(), rotation.startLocalToWorld );

	// rotate the pusher to it's final position
	QAngle angles = pRoot->GetLocalAngles();
	angles += pRoot->GetLocalAngularVelocity() * movetime;
	pRoot->SetLocalAngles( angles );
	
	// Compute the change in absangles
	MatrixCopy( pRoot->EntityToWorldTransform(), rotation.endLocalToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::PerformRotatePush( CBaseEntity *pRoot, float movetime )
{
	CVProfScope _( "CPhysicsPushedEntities::PerformRotatePush", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	m_bIsUnblockableByPlayer = (pRoot->GetFlags() & FL_UNBLOCKABLE_BY_PLAYER) ? true : false;
	// Build a list of this entity + all its children because we're going to try to move them all
	// This will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.clear();
	SetupAllInHierarchy( pRoot );

	// save where we rotated from, in case we're blocked
	QAngle angPrevAngles = pRoot->GetLocalAngles();

	// Apply the rotation
	RotatingPushMove_t	rotPushMove;
	RotateRootEntity( pRoot, movetime, rotPushMove );

	// Next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityList( );

	// Now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// If we fail, undo it all
	if (!SpeculativelyCheckRotPush( rotPushMove, pRoot ))
	{
		CBaseEntity *pBlocker = RegisterBlockage();
		pRoot->SetLocalAngles( angPrevAngles );
		RestoreEntities( );
		return pBlocker;
	}

	FinishPush( true, &rotPushMove );
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Linearly moves the root entity
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::LinearlyMoveRootEntity( CBaseEntity *pRoot, float movetime, Vector *pAbsPushVector )
{
	CVProfScope _( "CPhysicsPushedEntities::LinearlyMoveRootEntity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// move the pusher to it's final position
	Vector move = pRoot->GetLocalVelocity() * movetime;
	Vector origin = pRoot->GetLocalOrigin();
	origin += move;		
	pRoot->SetLocalOrigin( origin );

	// Store off the abs push vector
	*pAbsPushVector = pRoot->GetAbsVelocity() * movetime;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::PerformLinearPush( CBaseEntity *pRoot, float movetime )
{
	CVProfScope _( "CPhysicsPushedEntities::PerformLinearPush", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	m_flMoveTime = movetime;

	m_bIsUnblockableByPlayer = (pRoot->GetFlags() & FL_UNBLOCKABLE_BY_PLAYER) ? true : false;
	// Build a list of this entity + all its children because we're going to try to move them all
	// This will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.clear();
	SetupAllInHierarchy( pRoot );

	// save where we started from, in case we're blocked
	Vector vecPrevOrigin = pRoot->GetLocalOrigin();

	// Move the root (and all children) into its new position
	Vector vecAbsPush;
	LinearlyMoveRootEntity( pRoot, movetime, &vecAbsPush );

	// Next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityListAddBox( vecAbsPush );

	// Now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// If we fail, undo it all
	if (!SpeculativelyCheckLinearPush( vecAbsPush ))
	{
		CBaseEntity *pBlocker = RegisterBlockage();
		pRoot->SetLocalOrigin( vecPrevOrigin );
		RestoreEntities();
		return pBlocker;
	}

	FinishPush( );
	return nullptr;
}

//-----------------------------------------------------------------------------
//
// CBaseEntity methods
//
//-----------------------------------------------------------------------------

// create a macro that is true if we are allowed to debug traces during thinks, and compiles out to nothing otherwise.
#define THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_SERVER

//-----------------------------------------------------------------------------
// Purpose: Called when it's time for a physically moved objects (plats, doors, etc)
//			to run it's game code.
//			All other entity thinking is done during worldspawn's think
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsDispatchThink( BASEPTR thinkFunc )
{
	VPROF_ENTER_SCOPE( ( !vprof_scope_entity_thinks.GetBool() ) ? 
						"CBaseEntity::PhysicsDispatchThink" : 
						EntityFactoryDictionary()->GetCannonicalName( GetClassname() ) );

	float thinkLimit = think_limit.GetFloat();
	
	double startTime = 0.0;

/*
	// TODO:  Disable physics for dormant entites and test
	if ( IsDormant() )
	{
		Warning( "Dormant entity %s (%s) is thinking!!\n", GetClassname(), GetDebugName() );
		Assert(0);
	}
*/

	if ( thinkLimit )
	{
		startTime = Plat_FloatTime();
	}
	
	if ( thinkFunc )
	{
		static ConVarRef think_trace_limit( "think_trace_limit" );
		const int tracelimit = abs(think_trace_limit.GetInt());
		const bool bThinkTraceAllowed = DEBUG_THINK_TRACE_COUNTER_ALLOWED();
		if ( bThinkTraceAllowed )
		{
			enginetrace->GetSetDebugTraceCounter( tracelimit, kTRACE_COUNTER_SET );
		}

		(this->*thinkFunc)();

		if ( bThinkTraceAllowed )
		{
			enginetrace->GetSetDebugTraceCounter( 0, kTRACE_COUNTER_SET );
		}
	}

	if ( thinkLimit )
	{
		// calculate running time of the AI in milliseconds
		float time = ( Plat_FloatTime() - startTime ) * 1000.0f;
		if ( time > thinkLimit )
		{
			if ( vprof_think_limit.GetBool() )
			{
				g_VProfSignalSpike = true;
			}
			// If its an NPC print out the shedule/task that took so long
			{
				Msg( "%s(%s) thinking for %.02f ms!!!\n", GetClassname(), typeid(this).name(), time );
			}
		}
	}

	VPROF_EXIT_SCOPE();
}

//-----------------------------------------------------------------------------
// Purpose: Does not change the entities velocity at all
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCheckSweep( const Vector& vecAbsStart, const Vector &vecAbsDelta, trace_t *pTrace )
{
	::PhysicsCheckSweep( this, vecAbsStart, vecAbsDelta, pTrace );
}

static constexpr auto MAX_CLIP_PLANES = 5;
//-----------------------------------------------------------------------------
// Purpose: The basic solid body movement attempt/clip that slides along multiple planes
// Input  : time - Amount of time to try moving for
//			*steptrace - if not NULL, the trace results of any vertical wall hit will be stored
// Output : int - the clipflags if the velocity was modified (hit something solid)
//   1 = floor
//   2 = wall / step
//   4 = dead stop
//-----------------------------------------------------------------------------
int CBaseEntity::PhysicsTryMove( float flTime, trace_t *steptrace )
{
	CVProfScope _( "CBaseEntity::PhysicsTryMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity, new_velocity;
	int			i, j;
	trace_t		trace;
	Vector		end;
	float		time_left;
	int			blocked;
	
	unsigned int mask = PhysicsSolidMaskForEntity();

	new_velocity.Init();

	numbumps = 4;

	Vector vecAbsVelocity = GetAbsVelocity();

	blocked = 0;
	VectorCopy (vecAbsVelocity, original_velocity);
	VectorCopy (vecAbsVelocity, primal_velocity);
	numplanes = 0;
	
	time_left = flTime;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		if (vecAbsVelocity == vec3_origin)
			break;

		VectorMA( GetAbsOrigin(), time_left, vecAbsVelocity, end );

		Physics_TraceEntity( this, GetAbsOrigin(), end, mask, &trace );

		if (trace.startsolid)
		{	// entity is trapped in another solid
			SetAbsVelocity(vec3_origin);
			return 4;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			SetAbsOrigin( trace.endpos );
			VectorCopy (vecAbsVelocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		if (!trace.Ent<CBaseEntity>())
		{
			SetAbsVelocity( vecAbsVelocity );
			Warning( "PhysicsTryMove: !trace.u.ent" );
			contract_assert( 0 );
			return 4;
		}

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
			if (CanStandOn( trace.Ent<CBaseEntity>() ))
			{
				// keep track of time when changing ground entity
				if (GetGroundEntity() != trace.Ent<CBaseEntity>())
				{
					SetGroundChangeTime( gpGlobals->curtime + (flTime - (1 - trace.fraction) * time_left) );
				}

				SetGroundEntity( trace.Ent<CBaseEntity>() );
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
			if (steptrace)
				*steptrace = trace;	// save for player extrafriction
		}

		// run the impact function
		Vector vecCurrentVelocity = GetAbsVelocity();
		PhysicsImpact( trace.Ent<CBaseEntity>(), trace );
		// Removed by the impact function
		if ( IsMarkedForDeletion() || IsEdictFree() )
			break;
	
		time_left -= time_left * trace.fraction;
		
		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			SetAbsVelocity(vec3_origin);
			return blocked;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// if physics impact or entity's touch function changed our velocity, then update to use that
		if ( GetAbsVelocity() != vecCurrentVelocity )
		{
			vecAbsVelocity = GetAbsVelocity();
			original_velocity = GetAbsVelocity();
		}		
		else if ( GetMoveType() == MOVETYPE_WALK && (!(GetFlags() & FL_ONGROUND) || GetFriction()!=1) )	// reflect player velocity
		{
			// modify original_velocity so it parallels all of the clip planes
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{// floor or slope
					PhysicsClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					PhysicsClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + sv_bounce.GetFloat() * (1-GetFriction()) );
				}
			}

			VectorCopy( new_velocity, vecAbsVelocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i<numplanes ; i++)
			{
				PhysicsClipVelocity (original_velocity, planes[i], new_velocity, 1);
				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						if (DotProduct (new_velocity, planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)
					break;
			}
			
			if (i != numplanes)
			{	
				// go along this plane
				VectorCopy (new_velocity, vecAbsVelocity);
			}
			else
			{	
				// go along the crease
				if (numplanes != 2)
				{
	//				Msg( "clip velocity, numplanes == %i\n",numplanes);
					SetAbsVelocity( vecAbsVelocity );
					return blocked;
				}
				CrossProduct (planes[0], planes[1], dir);
				d = DotProduct (dir, vecAbsVelocity);
				VectorScale (dir, d, vecAbsVelocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny oscillations in sloping corners
			//
			if (DotProduct (vecAbsVelocity, primal_velocity) <= 0)
			{
				SetAbsVelocity(vec3_origin);
				return blocked;
			}
		}
	}

	SetAbsVelocity( vecAbsVelocity );
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: Applies 1/2 gravity to falling movetype step objects
//			Simulation should be done assuming average velocity over the time 
//			interval.  Since that would effect a lot of code, and since most of 
//			that code is going away, it's easier to just add in the average effect 
//			of gravity on the velocity over the interval at the beginning of similation, 
//			then add it in again at the end of simulation so that the final velocity is
//			correct for the entire interval.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsAddHalfGravity( float timestep )
{
	CVProfScope _( "CBaseEntity::PhysicsAddHalfGravity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	float	ent_gravity;

	if ( GetGravity() )
	{
		ent_gravity = GetGravity();
	}
	else
	{
		ent_gravity = 1.0;
	}

	// Add 1/2 of the total gravitational effects over this timestep
	Vector vecAbsVelocity = GetAbsVelocity();
	vecAbsVelocity[2] -= ( 0.5 * ent_gravity * sv_gravity.GetFloat() * timestep );
	vecAbsVelocity[2] += GetBaseVelocity()[2] * gpGlobals->frametime;
	SetAbsVelocity( vecAbsVelocity );

	Vector vecNewBaseVelocity = GetBaseVelocity();
	vecNewBaseVelocity[2] = 0;
	SetBaseVelocity( vecNewBaseVelocity );
	
	// Bound velocity
	PhysicsCheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: Does not change the entities velocity at all
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsPushEntity( const Vector& push, trace_t *pTrace )
{
	CVProfScope _( "CBaseEntity::PhysicsPushEntity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( GetMoveParent() )
	{
		Warning( "pushing entity (%s) that has parent (%s)!\n", GetDebugName(), GetMoveParent()->GetDebugName() );
		contract_assert( 0 );
	}

	// NOTE: absorigin and origin must be equal because there is no moveparent
	Vector prevOrigin;
	VectorCopy( GetAbsOrigin(), prevOrigin );

	::PhysicsCheckSweep( this, prevOrigin, push, pTrace );

	// if the sweep check starts inside a solid surface, try once more from the last origin
	if ( pTrace->startsolid )
	{

		CBaseCSGrenadeProjectile* pGrenadeProjectile = dynamic_cast<CBaseCSGrenadeProjectile*>( this );
		if ( pGrenadeProjectile )
		{
			pGrenadeProjectile->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
			UTIL_TraceLine( prevOrigin - push, prevOrigin + push, (CONTENTS_GRENADECLIP|CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_GRATE), this, COLLISION_GROUP_INTERACTIVE_DEBRIS, pTrace );
		}
		else
		{
			::PhysicsCheckSweep( this, prevOrigin - push, push, pTrace );
		}
	}

	if ( pTrace->fraction )
	{
		SetAbsOrigin( pTrace->endpos );

		// FIXME(ywb):  Should we try to enable this here
		// WakeRestingObjects();
	}

	// Passing in the previous abs origin here will cause the relinker
	// to test the swept ray from previous to current location for trigger intersections
	PhysicsTouchTriggers( &prevOrigin );

	if ( pTrace->Ent<CBaseEntity>() )
	{
		PhysicsImpact( pTrace->Ent<CBaseEntity>(), *pTrace );
	}
}

//-----------------------------------------------------------------------------
// Purpose:  See if entity is inside another entity, if so, returns true if so, fills in *ppEntity if ppEntity is not NULL
// Input  : **ppEntity - optional return pointer to entity we are inside of
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PhysicsTestEntityPosition( CBaseEntity **ppEntity /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::PhysicsTestEntityPosition", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	trace_t	trace;
	
	unsigned int mask = PhysicsSolidMaskForEntity();

	Physics_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), mask, &trace );
	
	if ( trace.startsolid )
	{
		if ( ppEntity )
		{
			*ppEntity = trace.Ent<CBaseEntity>();
		}
		return true;
	}
		
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::PhysicsPushMove( float movetime )
{
	CVProfScope _( "CBaseEntity::PhysicsPushMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// If this entity isn't moving, just update the time.
	IncrementLocalTime( movetime );

	if ( GetLocalVelocity() == vec3_origin )
	{
		return nullptr;
	}

	// Now check that the entire hierarchy can rotate into the new location
	CBaseEntity *pBlocker = g_pPushedEntities->PerformLinearPush( this, movetime );
	if ( pBlocker )
	{
		IncrementLocalTime( -movetime );
	}
	return pBlocker;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to rotate, returns success or failure
// Input  : movetime - 
// Output : bool
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::PhysicsPushRotate( float movetime )
{
	CVProfScope _( "CBaseEntity::PhysicsPushRotate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	IncrementLocalTime( movetime );

	// Not rotating
	if ( GetLocalAngularVelocity() == vec3_angle )
	{
		return nullptr;
	}

	// Now check that the entire hierarchy can rotate into the new location
	CBaseEntity *pBlocker = g_pPushedEntities->PerformRotatePush( this, movetime );
	if ( pBlocker )
	{
		IncrementLocalTime( -movetime );
	}

	return pBlocker;
}

//-----------------------------------------------------------------------------
// Block of icky shared code from PhysicsParent + PhysicsPusher
//-----------------------------------------------------------------------------
void CBaseEntity::PerformPush( float movetime )
{
	CVProfScope _( "CBaseEntity::PerformPush", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// NOTE: Use handle index because the previous blocker could have been deleted
	int hPrevBlocker = m_pBlocker.ToInt();
	CBaseEntity *pBlocker;
	g_pPushedEntities->BeginPush( this );
	if (movetime > 0)
	{
		if ( GetLocalAngularVelocity() != vec3_angle )
		{
			if ( GetLocalVelocity() != vec3_origin )
			{
				// NOTE: Both PhysicsPushRotate + PhysicsPushMove
				// will attempt to advance local time. Choose the one that's
				// the greater of the two from push + move

				// FIXME: Should we really be doing them both simultaneously??
				// FIXME: Choose the *greater* of the two?!? That's strange...
				float flInitialLocalTime = m_flLocalTime;

				// moving and rotating, so rotate first, then move
				pBlocker = PhysicsPushRotate( movetime );
				if ( !pBlocker )
				{
					float flRotateLocalTime = m_flLocalTime;

					// Reset the local time to what it was before we rotated
					m_flLocalTime = flInitialLocalTime;
					pBlocker = PhysicsPushMove( movetime );
					if ( m_flLocalTime < flRotateLocalTime )
					{
						m_flLocalTime = flRotateLocalTime;
					}
				}
			}
			else
			{
				// only rotating
				pBlocker = PhysicsPushRotate( movetime );
			}
		}
		else
		{
			// only moving
			pBlocker = PhysicsPushMove( movetime );
		}

		m_pBlocker = pBlocker;
		if (m_pBlocker.ToInt() != hPrevBlocker)
		{
			if (hPrevBlocker != INVALID_EHANDLE_INDEX)
			{
				EndBlocked();
			}
			if (m_pBlocker)
			{
				StartBlocked( pBlocker );
			}
		}
		if (m_pBlocker)
		{
			Blocked( m_pBlocker );
		}

		// NOTE NOTE: This is here for brutal reasons.
		// For MOVETYPE_PUSH objects with VPhysics shadow objects, the move done time
		// is handled by CBaseEntity::VPhyicsUpdatePusher, which only gets called if
		// the physics system thinks the entity is awake. That will happen if the
		// shadow gets updated, but the push code above doesn't update unless the
		// move is successful or non-zero. So we must make sure it's awake
		if ( VPhysicsGetObject() )
		{
			VPhysicsGetObject()->Wake();
		}
	}

	// move done is handled by physics if it has any
	if ( VPhysicsGetObject() && sv_phys_props_block_movers.GetBool() )
	{
		// store the list of moved entities for later
		// if you actually did an unblocked push that moved entities, and you're using physics (which may block later)
		if ( movetime > 0 && !m_pBlocker && GetSolid() == SOLID_VPHYSICS && g_pPushedEntities->CountMovedEntities() > 0 )
		{
			// UNDONE: Any reason to want to call this twice before physics runs?
			// If so, maybe just append to the list?
			physicspushlist_t *pList = (physicspushlist_t *)CreateDataObject( PHYSICSPUSHLIST );
			if ( pList )
			{
				g_pPushedEntities->StoreMovedEntities( *pList );
			}
		}
	}
	else
	{
		if ( m_flMoveDoneTime <= m_flLocalTime && m_flMoveDoneTime > 0 )
		{
			SetMoveDoneTime( -1 );
			MoveDone();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: UNDONE: This is only different from PhysicsParent because of the callback to PhysicsVelocity()
// Can we support that callback in push objects as well?
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsPusher( void )
{
	CVProfScope _( "CBaseEntity::PhysicsPusher", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	m_flVPhysicsUpdateLocalTime = m_flLocalTime;

	float movetime = GetMoveDoneTime();
	if (movetime > gpGlobals->frametime)
	{
		movetime = gpGlobals->frametime;
	}

	PerformPush( movetime );
}

//-----------------------------------------------------------------------------
// Purpose: Non moving objects can only think
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNone( void )
{
	CVProfScope _( "CBaseEntity::PhysicsNone", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// regular thinking
	PhysicsRunThink();
}

//-----------------------------------------------------------------------------
// Purpose: A moving object that doesn't obey physics
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsNoclip( void )
{
	CVProfScope _( "CBaseEntity::PhysicsNoclip", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// regular thinking
	if ( !PhysicsRunThink() )
	{
		return;
	}
	
	// Apply angular velocity
	SimulateAngles( gpGlobals->frametime );

	Vector origin;
	VectorMA( GetLocalOrigin(), gpGlobals->frametime, GetLocalVelocity(), origin );
	SetLocalOrigin( origin );
}

void CBaseEntity::PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	// If you're going to use custom physics, you need to implement this!
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Allows entities to describe their own physics
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsCustom()
{
	CVProfScope _( "CBaseEntity::PhysicsCustom", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PhysicsCheckWater();

	// regular thinking
	if ( !PhysicsRunThink() )
		return;

	// Moving upward, off the ground, or  resting on a client/monster, remove FL_ONGROUND
	if ( m_vecVelocity[2] > 0 || !GetGroundEntity() || !GetGroundEntity()->IsStandable() )
	{
		SetGroundEntity( nullptr );
	}

	// NOTE: The entity must set the position, angles, velocity in its custom movement
	Vector vecNewPosition = GetAbsOrigin();
	Vector vecNewVelocity = GetAbsVelocity();
	QAngle angNewAngles = GetAbsAngles();
	QAngle angNewAngVelocity = GetLocalAngularVelocity();

	PerformCustomPhysics( &vecNewPosition, &vecNewVelocity, &angNewAngles, &angNewAngVelocity );

	// Store off all of the new state information...
	SetAbsVelocity( vecNewVelocity );
	SetAbsAngles( angNewAngles );
	SetLocalAngularVelocity( angNewAngVelocity );

	Vector move;
	VectorSubtract( vecNewPosition, GetAbsOrigin(), move );

	if ( move.LengthSqr() > 1e-6f )
	{
		// move origin
		trace_t trace;
		PhysicsPushEntity( move, &trace );

		PhysicsCheckVelocity();

		if (trace.allsolid)
		{	
			// entity is trapped in another solid
			// UNDONE: does this entity needs to be removed?
			SetAbsVelocity(vec3_origin);
			SetLocalAngularVelocity(vec3_angle);
			return;
		}
	}
	
	if (IsEdictFree())
		return;

	// check for in water
	PhysicsCheckWaterTransition();
}

bool g_bTestMoveTypeStepSimulation = true;
ConVar sv_teststepsimulation( side_registry(), "sv_teststepsimulation", "1", 0 );

//-----------------------------------------------------------------------------
// Purpose: Until we remove the above cvar, we need to have the entities able
//  to dynamically deal with changing their simulation stuff here.
//-----------------------------------------------------------------------------
void CBaseEntity::CheckStepSimulationChanged()
{
	if ( g_bTestMoveTypeStepSimulation != IsSimulatedEveryTick() )
	{
		SetSimulatedEveryTick( g_bTestMoveTypeStepSimulation );
	}

	bool hadobject = HasDataObjectType( STEPSIMULATION );

	if ( g_bTestMoveTypeStepSimulation )
	{
		if ( !hadobject )
		{
			CreateDataObject( STEPSIMULATION );
		}
	}
	else
	{
		if ( hadobject )
		{
			DestroyDataObject( STEPSIMULATION );
		}
	}
}

static constexpr auto STEP_TELPORTATION_VEL_SQ = ( 4096.0f * 4096.0f );
//-----------------------------------------------------------------------------
// Purpose: Run regular think and latch off angle/origin changes so we can interpolate them on the server to fake simulation
// Input  : *step - 
//-----------------------------------------------------------------------------
void CBaseEntity::StepSimulationThink( float dt )
{
	// See if we need to allocate, deallocate step simulation object
	CheckStepSimulationChanged();

	StepSimulationData *step = ( StepSimulationData * )GetDataObject( STEPSIMULATION );
	if ( !step )
	{
		PhysicsStepRunTimestep( dt );

		// Just call the think function directly
		PhysicsRunThink( THINK_FIRE_BASE_ONLY );
	}
	else
	{
		// Assume that it's in use
		step->m_bOriginActive = true;
		step->m_bAnglesActive = true;

		// Reset networked versions of origin and angles
		step->m_nLastProcessTickCount = -1;
		step->m_vecNetworkOrigin.Init();
		step->m_angNetworkAngles.Init();

		// Remember old old values
		step->m_Previous2 = step->m_Previous;

		// Remember old values
		step->m_Previous.nTickCount = gpGlobals->tickcount;
		step->m_Previous.vecOrigin = GetStepOrigin();
		QAngle stepAngles = GetStepAngles();
		AngleQuaternion( stepAngles, step->m_Previous.qRotation );

		// Run simulation
		PhysicsStepRunTimestep( dt );

		// Call the actual think function...
		PhysicsRunThink( THINK_FIRE_BASE_ONLY );

		// do any local processing that's needed
		if (GetBaseAnimating() != nullptr)
		{
			GetBaseAnimating()->UpdateStepOrigin();
		}

		// Latch new values to see if external code modifies our position/orientation
		step->m_Next.vecOrigin = GetStepOrigin();
		stepAngles = GetStepAngles();
		AngleQuaternion( stepAngles, step->m_Next.qRotation );
		// Also store of non-Quaternion version for simple comparisons
		step->m_angNextRotation = GetStepAngles();
		step->m_Next.nTickCount = GetNextThinkTick();

		// Hack:  Add a tick if we are simulating every other tick
		if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
		{
			++step->m_Next.nTickCount;
		}

		// Check for teleportation/snapping of the origin
		if ( dt > 0.0f )
		{
			Vector deltaorigin = step->m_Next.vecOrigin - step->m_Previous.vecOrigin;
			float velSq = deltaorigin.LengthSqr() / ( dt * dt );
			if ( velSq >= STEP_TELPORTATION_VEL_SQ )
			{
				// Deactivate it due to large origin change
				step->m_bOriginActive = false;
				step->m_bAnglesActive = false;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Monsters freefall when they don't have a ground entity, otherwise
//   all movement is done with discrete steps.
// This is also used for objects that have become still on the ground, but
//  will fall if the floor is pulled out from under them.
// JAY: Extended this to synchronize movement and thinking wherever possible.
// This allows the client-side interpolation to interpolate animation and simulation
// data at the same time.
// UNDONE:	Remove all other cases from this loop - only use MOVETYPE_STEP to simulate
//			entities that are currently animating/thinking.
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStep()
{
	// EVIL HACK: Force these to appear as if they've changed!!!
	// The underlying values don't actually change, but we need the network sendproxy on origin/angles
	//  to get triggered, and that only happens if NetworkStateChanged() appears to have occured.
	// Getting them for modify marks them as changed automagically.
	m_vecOrigin.GetForModify();
	m_angRotation.GetForModify();
	
	// HACK:  Make sure that the client latches the networked origin/orientation changes with the current server tick count
	//  so that we don't get jittery interpolation.  All of this is necessary to mimic actual continuous simulation of the underlying
	//  variables.
	SetSimulationTime( gpGlobals->curtime );
	
	// Run all but the base think function
	PhysicsRunThink( THINK_FIRE_ALL_BUT_BASE );

	int thinktick = GetNextThinkTick();
	float thinktime = thinktick * TICK_INTERVAL;

	// Is the next think too far out, or non-existent?
	// BUGBUG: Interpolation is going to look bad in here.  But it should only
	// be for dead things - and those should be ragdolls (client-side sim) anyway.
	// UNDONE: Remove this and assert?  Force MOVETYPE_STEP objs to become MOVETYPE_TOSS when
	// they aren't thinking?
	// UNDONE: this happens as the first frame for a bunch of things like dynamically created ents.
	// can't remove until initial conditions are resolved
	float deltaThink = thinktime - gpGlobals->curtime;
	if ( thinktime <= 0 || deltaThink > 0.5 )
	{
		PhysicsStepRunTimestep( gpGlobals->frametime );
		PhysicsCheckWaterTransition();
		SetLastThink( -1, gpGlobals->curtime );
		UpdatePhysicsShadowToCurrentPosition(gpGlobals->frametime);
		PhysicsRelinkChildren(gpGlobals->frametime);
		return;
	}

	Vector oldOrigin = GetAbsOrigin();

	// Feed the position delta back from vphysics if enabled
	bool updateFromVPhysics = npc_vphysics.GetBool();
	if ( HasDataObjectType(VPHYSICSUPDATEAI) )
	{
		vphysicsupdateai_t *pUpdate = static_cast<vphysicsupdateai_t *>(GetDataObject( VPHYSICSUPDATEAI ));
		if ( pUpdate->stopUpdateTime > gpGlobals->curtime )
		{
			updateFromVPhysics = true;
		}
		else
		{
			float maxAngular;
			VPhysicsGetObject()->GetShadowController()->GetMaxSpeed( nullptr, &maxAngular );
			VPhysicsGetObject()->GetShadowController()->MaxSpeed( pUpdate->savedShadowControllerMaxSpeed, maxAngular );
			DestroyDataObject(VPHYSICSUPDATEAI);
		}
	}

	if ( updateFromVPhysics && VPhysicsGetObject() && !GetParent() )
	{
		Vector position;
		VPhysicsGetObject()->GetShadowPosition( &position, nullptr );
		float delta = (GetAbsOrigin() - position).LengthSqr();
		// for now, use a tolerance of 1 inch for these tests
		if ( delta < 1 )
		{
			// physics is really close, check to see if my current position is valid.
			// If so, ignore the physics result.
			trace_t tr;
			Physics_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), PhysicsSolidMaskForEntity(), &tr );
			updateFromVPhysics = tr.startsolid;
		}
		if ( updateFromVPhysics )
		{
			SetAbsOrigin( position );
			PhysicsTouchTriggers();
		}
		//NDebugOverlay::Box( position, WorldAlignMins(), WorldAlignMaxs(), 255, 255, 0, 0, 0.0 );
	}

	// not going to think, don't run game physics either
	if ( thinktick > gpGlobals->tickcount )
		return;
	
	// Don't let things stay in the past.
	//  it is possible to start that way
	//  by a trigger with a local time.
	if ( thinktime < gpGlobals->curtime )
	{
		thinktime = gpGlobals->curtime;	
	}

	// simulate over the timestep
	float dt = thinktime - GetLastThink();

	// Now run step simulator
	StepSimulationThink( dt );

	PhysicsCheckWaterTransition();

	if ( VPhysicsGetObject() )
	{
		if ( !VectorCompare( oldOrigin, GetAbsOrigin() ) )
		{
			VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), vec3_angle, (GetFlags() & FL_FLY) ? true : false, dt );
		}
	}
	PhysicsRelinkChildren(dt);
}

void UTIL_TraceLineFilterEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					   unsigned int mask, const int nCollisionGroup, trace_t *ptr );

// Check to see what (if anything) this MOVETYPE_STEP entity is standing on
void CBaseEntity::PhysicsStepRecheckGround()
{
	unsigned int mask = PhysicsSolidMaskForEntity();
	// determine if it's on solid ground at all
	Vector	mins, maxs, point;
	int		x, y;
	trace_t trace;
	
	VectorAdd (GetAbsOrigin(), WorldAlignMins(), mins);
	VectorAdd (GetAbsOrigin(), WorldAlignMaxs(), maxs);
	point[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
	{
		for	(y=0 ; y<=1 ; y++)
		{
			point[0] = x ? maxs[0] : mins[0];
			point[1] = y ? maxs[1] : mins[1];

			ICollideable *pCollision = GetCollideable();

			{
				UTIL_TraceLine( point, point, mask, this, COLLISION_GROUP_NONE, &trace );
			}

			if ( trace.startsolid )
			{
				SetGroundEntity( trace.Ent<CBaseEntity>() );
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timestep - 
//-----------------------------------------------------------------------------
void CBaseEntity::PhysicsStepRunTimestep( float timestep )
{
	bool	wasonground;
	bool	inwater;
	bool	hitsound = false;
	float	speed, newspeed, control;
	float	friction;

	PhysicsCheckVelocity();

	wasonground = ( GetFlags() & FL_ONGROUND ) ? true : false;

	// add gravity except:
	//   flying monsters
	//   swimming monsters who are in the water
	inwater = PhysicsCheckWater();

	bool isfalling = false;
	float fFallingSpeed = GetAbsVelocity()[2];

	if ( !wasonground )
	{
		if ( !( GetFlags() & FL_FLY ) )
		{
			if ( !( ( GetFlags() & FL_SWIM ) && ( GetWaterLevel() > WL_NotInWater ) ) )
			{
				if ( GetAbsVelocity()[2] < ( sv_gravity.GetFloat() * -0.1 ) )
				{
					hitsound = true;
				}

				if ( !inwater )
				{
					PhysicsAddHalfGravity( timestep );
					isfalling = true;
				}
			}
		}
	}

	if ( !(GetFlags() & FL_STEPMOVEMENT) &&
		(!VectorCompare(GetAbsVelocity(), vec3_origin) || 
		 !VectorCompare(GetBaseVelocity(), vec3_origin)))
	{
		Vector vecAbsVelocity = GetAbsVelocity();

		SetGroundEntity( nullptr );
		// apply friction
		// let dead monsters who aren't completely onground slide
		if ( wasonground )
		{
			speed = VectorLength( vecAbsVelocity );
			if (speed)
			{
				friction = sv_friction.GetFloat() * GetFriction();

				control = speed < sv_stopspeed.GetFloat() ? sv_stopspeed.GetFloat() : speed;
				newspeed = speed - timestep*control*friction;

				if (newspeed < 0)
					newspeed = 0;
				newspeed /= speed;

				vecAbsVelocity[0] *= newspeed;
				vecAbsVelocity[1] *= newspeed;
			}
		}

		vecAbsVelocity += GetBaseVelocity();
		SetAbsVelocity( vecAbsVelocity );

		// Apply angular velocity
		SimulateAngles( timestep );

		PhysicsCheckVelocity();

		PhysicsTryMove( timestep, nullptr );

		PhysicsCheckVelocity();

		vecAbsVelocity = GetAbsVelocity();
		vecAbsVelocity -= GetBaseVelocity();
		SetAbsVelocity( vecAbsVelocity );

		PhysicsCheckVelocity();

		if ( !(GetFlags() & FL_ONGROUND) )
		{
			PhysicsStepRecheckGround();
		}

		PhysicsTouchTriggers();
	}

	if (!( GetFlags() & FL_ONGROUND ) && isfalling)
	{
		PhysicsAddHalfGravity( timestep );
	}

	// if we were falling before and now we're on the ground..
	if ( isfalling && GetFlags() & FL_ONGROUND )	
	{
		PhysicsLandedOnGround( fFallingSpeed );
	}
}

// After this long, if a player isn't updating, then return it's projectiles to server control
static constexpr auto PLAYER_PACKETS_STOPPED_SO_RETURN_TO_PHYSICS_TIME = 1.0f;

void Physics_SimulateEntity( CBaseEntity *pEntity )
{
	CVProfScope _( ( !vprof_scope_entity_gamephys.GetBool() ) ? 
			"Physics_SimulateEntity" : 
			EntityFactoryDictionary()->GetCannonicalName( pEntity->GetClassname() ), 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( pEntity->edict() )
	{
		// Player drives simulation of this entity
		if ( pEntity->IsPlayerSimulated() )
		{
			// If the player is gone, dropped, crashed, then return
			//  control to the game code.
			CBasePlayer *simulatingPlayer = pEntity->GetSimulatingPlayer();
			if ( simulatingPlayer &&
				( simulatingPlayer->GetTimeBase() > gpGlobals->curtime - PLAYER_PACKETS_STOPPED_SO_RETURN_TO_PHYSICS_TIME ) )
			{
				// Okay, the guy is still around
				return;
			}

			pEntity->UnsetPlayerSimulated();
		}

		{
			// Run entity physics
			pEntity->PhysicsSimulate();
		}
	}
	else
	{
		pEntity->PhysicsRunThink();
	}
}
//-----------------------------------------------------------------------------
// Purpose: Runs the main physics simulation loop against all entities ( except players )
//-----------------------------------------------------------------------------
void Physics_RunThinkFunctions( bool simulating )
{
	ks::ScratchArena<> scratch;
	MDLCACHE_CRITICAL_SECTION();
	CVProfScope _( "Physics_RunThinkFunctions", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_bTestMoveTypeStepSimulation = sv_teststepsimulation.GetBool();

	float starttime = gpGlobals->curtime;
	// clear all entites freed outside of this loop
	gEntList.CleanupDeleteList();

	if ( !simulating )
	{
		// only simulate players
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				// Always reset clock to real sv.time
				gpGlobals->curtime = starttime;
				// Force usercmd processing even though gpGlobals->tickcount isn't incrementing
				pPlayer->ForceSimulation();
				Physics_SimulateEntity( pPlayer );
			}
		}
	}
	else
	{
		UTIL_DisableRemoveImmediate();
		int listMax = SimThink_ListCount();
		listMax = std::max(listMax,1);
		CBaseEntity **list = scratch.Alloc<CBaseEntity *>( listMax );
		// iterate through all entities and have them think or simulate
		
		// UNDONE: This has problems with UTIL_RemoveImmediate() (now disabled during this loop).  
		// Do we really need UTIL_RemoveImmediate()?
		int count = SimThink_ListCopy( list, listMax );

		//DevMsg(1, "Count: %d\n", count );
		for ( int i = 0; i < count; i++ )
		{
			// Always reset clock to real sv.time
			gpGlobals->curtime = starttime;
			Physics_SimulateEntity( list[i] );
		}

		// The pusher system queued up a bunch of physics updates.  Make them happen now.
		g_pPushedEntities->UpdatePusherPhysicsEndOfTick();

		UTIL_EnableRemoveImmediate();
		physenv->CleanupDeleteList();
	}

	gpGlobals->curtime = starttime;
}

#undef THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_SERVER
