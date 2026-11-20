// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <string.h>
#include "eiface.h"
#include "networkvar.h"
#include "cmodel.h"
#include "vphysics_interface.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "irecipientfilter.h"
#include "particles/particles.h"
#include "string_t.h"
#include "declare_class.h"
#include "vphysics/constraints.h"
#include "shareddefs.h"
module ks.server;

import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import std;
import ks.core.basetypes;
import ks.math.mathlib;
import ks.tier1.iconvar;
import :entity_dispatch;
import :entity_kind;
import ks.core.dbg;
import ks.core.platform;
import ks.tier1.strtools;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.core.vprof;
import ks.tier1.fmtstr;

class CBaseEntity;
struct OverlayLine_t;

using namespace ::ks::reflect;

// Purpose: Namespace for functions dealing with Debug Overlays

static constexpr auto NUM_DEBUG_OVERLAY_LINES = 20;

int				m_nDebugOverlayIndex	= -1;
OverlayLine_t*	m_debugOverlayLine[NUM_DEBUG_OVERLAY_LINES];

//-----------------------------------------------------------------------------
// Purpose: Return the old overlay line from the overlay pool
//-----------------------------------------------------------------------------
OverlayLine_t* GetDebugOverlayLine(void)
{
	// Make casing pool if not initialized
	if (m_nDebugOverlayIndex == -1)
	{
		for (int i=0;i<NUM_DEBUG_OVERLAY_LINES;i++)
		{
			m_debugOverlayLine[i]				= new OverlayLine_t;
			m_debugOverlayLine[i]->noDepthTest  = true;
			m_debugOverlayLine[i]->draw			= false;
		}
		m_nDebugOverlayIndex = 0;
	}

	int id;

	m_nDebugOverlayIndex++;
	if (m_nDebugOverlayIndex == NUM_DEBUG_OVERLAY_LINES)
	{
		m_nDebugOverlayIndex = 0;
		id			  = NUM_DEBUG_OVERLAY_LINES-1;

	}
	else
	{
		id = m_nDebugOverlayIndex-1;
	}
	return m_debugOverlayLine[id];
}

//-----------------------------------------------------------------------------
// Purpose: Adds a debug line to be drawn on the screen
// Input  : If testLOS is true, color is based on line of sight test
// Output : 
//-----------------------------------------------------------------------------
void UTIL_AddDebugLine(const Vector &startPos, const Vector &endPos, bool noDepthTest, bool testLOS) 
{
	OverlayLine_t* debugLine = GetDebugOverlayLine();

	debugLine->origin		= startPos;
	debugLine->dest			= endPos;
	debugLine->noDepthTest	= noDepthTest;
	debugLine->draw = true;

	if (testLOS)
	{
		trace_t tr;
		UTIL_TraceLine ( debugLine->origin, debugLine->dest, MASK_BLOCKLOS, nullptr, COLLISION_GROUP_NONE, &tr );
		if (tr.startsolid || tr.fraction < 1.0)
		{
			debugLine->r = 255;
			debugLine->g = 0;
			debugLine->b = 0;
			return;
		}
	}

	debugLine->r = 255;
	debugLine->g = 255;
	debugLine->b = 255;
}

//-----------------------------------------------------------------------------
// Purpose: Returns z value of floor below given point (up to 384 inches below)
//-----------------------------------------------------------------------------
float GetLongFloorZ(const Vector &origin) 
{
	// trace to the ground, then pop up 8 units and place node there to make it
	// easier for them to connect (think stairs, chairs, and bumps in the floor).
	// After the routing is done, push them back down.
	//
	trace_t	tr;
	UTIL_TraceLine ( origin,
					 origin - Vector ( 0, 0, 2048 ),
					 MASK_NPCSOLID_BRUSHONLY,
					 nullptr,
					 COLLISION_GROUP_NONE,
					 &tr );

	// This trace is ONLY used if we hit an entity flagged with FL_WORLDBRUSH
	trace_t	trEnt;
	UTIL_TraceLine ( origin,
					 origin - Vector ( 0, 0, 2048 ),
					 MASK_NPCSOLID,
					 nullptr,
					 COLLISION_GROUP_NONE,
					 &trEnt );

	// Did we hit something closer than the floor?
	if ( trEnt.fraction < tr.fraction )
	{
		// If it was a world brush entity, copy the node location
		if ( trEnt.Ent<CBaseEntity>() )
		{
			CBaseEntity *e = trEnt.Ent<CBaseEntity>();
			if ( e && (e->GetFlags() & FL_WORLDBRUSH) )
			{
				tr.endpos = trEnt.endpos;
			}
		}
	}

	return tr.endpos.z;
}



//------------------------------------------------------------------------------
// Purpose : Draw all overlay lines in the list
//------------------------------------------------------------------------------
void UTIL_DrawOverlayLines(void) 
{
	if (m_nDebugOverlayIndex != -1)
	{
		for (int i=0;i<NUM_DEBUG_OVERLAY_LINES;i++)
		{
			if (m_debugOverlayLine[i]->draw)
			{
				NDebugOverlay::Line(m_debugOverlayLine[i]->origin,
									 m_debugOverlayLine[i]->dest,
									 m_debugOverlayLine[i]->r,
									 m_debugOverlayLine[i]->g,
									 m_debugOverlayLine[i]->b,
									 m_debugOverlayLine[i]->noDepthTest,0);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add an overlay line with padding on the start and end
//-----------------------------------------------------------------------------
void DebugDrawLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration )
{
	NDebugOverlay::Line( vecAbsStart + Vector( 0,0,0.1), vecAbsEnd + Vector( 0,0,0.1), r,g,b, test, duration );
}

//-----------------------------------------------------------------------------
// Purpose: Allow all debug overlays to be cleared at once
//-----------------------------------------------------------------------------
static void clear_debug_overlays( const CCommand &args );
static ConCommand clear_debug_overlays_command( side_registry(), "clear_debug_overlays", clear_debug_overlays, "clears debug overlays" );
static void clear_debug_overlays( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBaseEntity *pEntity = gEntList.FirstEnt();
	
	// Clear all entities of their debug overlays
	while ( pEntity )
	{
		pEntity->m_debugOverlays = 0;
		// UNDONE: Clear out / expire timed overlays?
		pEntity = gEntList.NextEnt( pEntity );
	}
	
	// Clear all engine overlays
	if ( debugoverlay )
	{
		debugoverlay->ClearAllOverlays();
	}
}

class CBaseCombatCharacter;
class CBaseDoor;
class CBaseEntity;
class CBasePropDoor;
class CPushAwayEnumerator;
class CUserCmd;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------------------------------
ConVar sv_pushaway_force( side_registry(), "sv_pushaway_force", "30000", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How hard physics objects are pushed away from the players on the server." );
ConVar sv_pushaway_min_player_speed( side_registry(), "sv_pushaway_min_player_speed", "75", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "If a player is moving slower than this, don't push away physics objects (enables ducking behind things)." );
ConVar sv_pushaway_max_force( side_registry(), "sv_pushaway_max_force", "1000", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Maximum amount of force applied to physics objects by players." );
ConVar sv_pushaway_clientside( side_registry(), "sv_pushaway_clientside", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Clientside physics push away (0=off, 1=only localplayer, 1=all players)" );

ConVar sv_pushaway_player_force( side_registry(), "sv_pushaway_player_force", "200000", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "How hard the player is pushed away from physics objects (falls off with inverse square of distance)." );
ConVar sv_pushaway_max_player_force( side_registry(), "sv_pushaway_max_player_force", "10000", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Maximum of how hard the player is pushed away from physics objects." );

extern ConVar sv_turbophysics;

//-----------------------------------------------------------------------------------------------------
bool IsPushAwayEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	if ( pEnt->GetCollisionGroup() != COLLISION_GROUP_PUSHAWAY )
	{
		// Try backing away from doors that are currently rotating, to prevent blocking them
		if ( FClassnameIs( pEnt, "func_door_rotating" ) )
		{
			CBaseDoor *door = dynamic_cast<CBaseDoor *>(pEnt);
			if ( !door )
			{
				return false;
			}

			if ( door->m_toggle_state != TS_GOING_UP && door->m_toggle_state != TS_GOING_DOWN )
			{
				return false;
			}
		}
		else if ( FClassnameIs( pEnt, "prop_door_rotating" ) )
		{
			CBasePropDoor *door = dynamic_cast<CBasePropDoor *>(pEnt);
			if ( !door )
			{
				return false;
			}

			if ( !door->IsDoorOpening() && !door->IsDoorClosing() )
			{
				return false;
			}
		}

		else
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------------------------------
bool IsPushableEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	if ( sv_turbophysics.GetBool() )
	{
		if ( pEnt->GetCollisionGroup() == COLLISION_GROUP_NONE )
		{
			if ( FClassnameIs( pEnt, "prop_physics_multiplayer" ) )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
bool IsBreakableEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	// first check to see if it's already broken
	if ( pEnt->m_iHealth < 0 && pEnt->GetMaxHealth() > 0 )
		return true;

	// If we won't be able to break it, don't try
	if ( pEnt->m_takedamage != DAMAGE_YES )
		return false;

	if ( pEnt->GetCollisionGroup() != COLLISION_GROUP_PUSHAWAY && pEnt->GetCollisionGroup() != COLLISION_GROUP_BREAKABLE_GLASS && pEnt->GetCollisionGroup() != COLLISION_GROUP_NONE )
		return false;

	if ( pEnt->m_iHealth > 200 )
		return false;

	IMultiplayerPhysics *pPhysicsInterface = dynamic_cast< IMultiplayerPhysics * >( pEnt );
	if ( pPhysicsInterface )
	{
		if ( pPhysicsInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			return false;
	}
	else
	{
		if ((FClassnameIs( pEnt, "func_breakable" ) || FClassnameIs( pEnt, "func_breakable_surf" )))
		{
			if (FClassnameIs( pEnt, "func_breakable_surf" ))
			{
				// don't try to break it if it has already been broken
				CBreakableSurface *surf = static_cast< CBreakableSurface * >( pEnt );

				if ( surf->m_bIsBroken )
					return false;
			}
		}
		else if ( pEnt->PhysicsSolidMaskForEntity() & CONTENTS_PLAYERCLIP )
		{
			// hostages and players use CONTENTS_PLAYERCLIP, so we can use it to ignore them
			return false;
		}
	}

	IBreakableWithPropData *pBreakableInterface = dynamic_cast< IBreakableWithPropData * >( pEnt );
	if ( pBreakableInterface )
	{
		// Bullets don't damage it - ignore
		if ( pBreakableInterface->GetDmgModBullet() <= 0.0f )
		{
			return false;
		}
	}

// what the shit is this
/*
	CBreakableProp *pProp = dynamic_cast< CBreakableProp * >( pEnt );
	if ( pProp )
	{
		// It takes a large amount of damage to even scratch it - ignore
		if ( pProp->m_iMinHealthDmg >= 50 )
		{
			return false;
		}
	}
*/

	return true;
}

//-----------------------------------------------------------------------------------------------------
int GetPushawayEnts( CBaseCombatCharacter *pPushingEntity, CBaseEntity **ents, int nMaxEnts, float flPlayerExpand, int PartitionMask, CPushAwayEnumerator *enumerator )
{
	
	Vector vExpand( flPlayerExpand, flPlayerExpand, flPlayerExpand );

	Ray_t ray;
	ray.Init( pPushingEntity->GetAbsOrigin(), pPushingEntity->GetAbsOrigin(), pPushingEntity->GetCollideable()->OBBMins() - vExpand, pPushingEntity->GetCollideable()->OBBMaxs() + vExpand );

	CPushAwayEnumerator *physPropEnum = nullptr;
	if  ( !enumerator )
	{
		physPropEnum = new CPushAwayEnumerator( ents, nMaxEnts );
		enumerator = physPropEnum;
	}

	::partition->EnumerateElementsAlongRay( PartitionMask, ray, false, enumerator );

	int numHit = enumerator->m_nAlreadyHit;

	if ( physPropEnum )
		delete physPropEnum;

	return numHit;
}

void AvoidPushawayProps( CBaseCombatCharacter *pPlayer, CUserCmd *pCmd )
{
	// Figure out what direction we're moving and the extents of the box we're going to sweep 
	// against physics objects.
	Vector currentdir;
	Vector rightdir;
	if (g_pGameRules->IsTopDown())
	{
		AngleVectors( g_pGameRules->GetTopDownMovementAxis(), &currentdir, &rightdir, nullptr );
	}
	else
	{
		AngleVectors( pCmd->viewangles, &currentdir, &rightdir, nullptr );
	}

	CBaseEntity *props[512];
	int nEnts = GetPushawayEnts( pPlayer, props, std::size( props ), 0.0f, PARTITION_ENGINE_SOLID_EDICTS, nullptr );

	const Vector & ourCenter = pPlayer->WorldSpaceCenter();
	Vector nearestPropPoint;
	Vector nearestPlayerPoint;

	for ( int i=0; i < nEnts; i++ )
	{
		// Don't respond to this entity on the client unless it has PHYSICS_MULTIPLAYER_FULL set.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );
		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			continue;

		const float minMass = 10.0f; // minimum mass that can push a player back
		const float maxMass = 30.0f; // cap at a decently large value
		float mass = maxMass;
		if ( pInterface )
		{
			mass = pInterface->GetMass();
		}
		mass = clamp( mass, minMass, maxMass );
		
		mass = std::max<std::common_type_t<float, int>>( mass, 0 );
		mass /= maxMass; // bring into a 0..1 range

		// Push away from the collision point. The closer our center is to the collision point,
		// the harder we push away.
		props[i]->CollisionProp()->CalcNearestPoint( ourCenter, &nearestPropPoint );
		pPlayer->CollisionProp()->CalcNearestPoint( nearestPropPoint, &nearestPlayerPoint );
		Vector vPushAway = (nearestPlayerPoint - nearestPropPoint);
		float flDist = VectorNormalize( vPushAway );

		const float MaxPushawayDistance = 5.0f;
		if ( flDist > MaxPushawayDistance && !pPlayer->CollisionProp()->IsPointInBounds( nearestPropPoint ) )
		{
			continue;
		}

		// If we're not pushing, try from our center to the nearest edge of the prop
		if ( vPushAway.IsZero() )
		{
			vPushAway = (ourCenter - nearestPropPoint);
			flDist = VectorNormalize( vPushAway );
		}

		// If we're still not pushing, try from our center to the center of the prop
		if ( vPushAway.IsZero() )
		{
			vPushAway = (ourCenter - props[i]->WorldSpaceCenter());
			flDist = VectorNormalize( vPushAway );
		}

		flDist = std::max<std::common_type_t<float, int>>( flDist, 1 );

		float flForce = sv_pushaway_player_force.GetFloat() / flDist * mass;
		flForce = std::min( flForce, sv_pushaway_max_player_force.GetFloat() );

		pPlayer->PushawayTouch( props[i] );

		// We can get right up next to rotating doors before they start to move, so scale back our force so we don't go flying
		if ( FClassnameIs( props[i], "func_door_rotating" ) || FClassnameIs( props[i], "prop_door_rotating" ) )
		{
			flForce *= 0.25f;
		}

		vPushAway *= flForce;

		pCmd->forwardmove += vPushAway.Dot( currentdir );
		pCmd->sidemove    += vPushAway.Dot( rightdir );
	}
}

//-----------------------------------------------------------------------------------------------------
void PerformObstaclePushaway( CBaseCombatCharacter *pPushingEntity )
{
	if (  pPushingEntity->m_lifeState != LIFE_ALIVE )
		return;

	// Give a push to any barrels that we're touching.
	// The client handles adjusting our usercmd to push us away.
	CBaseEntity *props[256];

	int nEnts = GetPushawayEnts( pPushingEntity, props, std::size( props ), 3.0f, PARTITION_ENGINE_SOLID_EDICTS, nullptr );
	
	for ( int i=0; i < nEnts; i++ )
	{
		// If this entity uas PHYSICS_MULTIPLAYER_FULL set (ie: it's not just debris), and we're moving too slow, don't push it away.
		// Instead, let the client bounce off it. This allows players to get close to and duck behind things without knocking them over.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );

		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() == PHYSICS_MULTIPLAYER_SOLID )
		{
			if ( pPushingEntity->GetAbsVelocity().Length2D() < sv_pushaway_min_player_speed.GetFloat() )
				continue;
		}

		IPhysicsObject *pObj = props[i]->VPhysicsGetObject();

		if ( pObj )
		{		
			Vector vPushAway = (props[i]->WorldSpaceCenter() - pPushingEntity->WorldSpaceCenter());
			vPushAway.z = 0;
			
			float flDist = VectorNormalize( vPushAway );
			flDist = std::max<std::common_type_t<float, int>>( flDist, 1 );
			
			float flForce = sv_pushaway_force.GetFloat() / flDist;
			flForce = std::min( flForce, sv_pushaway_max_force.GetFloat() );

			pObj->ApplyForceOffset( vPushAway * flForce, pPushingEntity->WorldSpaceCenter() );
		}
	}
}

CPushAwayEnumerator::CPushAwayEnumerator(CBaseEntity **ents, int nMaxEnts)
{
	m_nAlreadyHit = 0;
	m_AlreadyHit = ents;
	m_nMaxHits = nMaxEnts;
}

IterationRetval_t CBotBreakableEnumerator::EnumElement( IHandleEntity *pHandleEntity)
{
	CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

	if ( !IsBreakableEntity( pEnt ) )
		return ITERATION_CONTINUE;

	// ignore breakables parented to doors
	if ( pEnt->GetParent() &&
		( FClassnameIs( pEnt->GetParent(), "func_door*" ) ||
		FClassnameIs( pEnt, "prop_door*" ) ) )
		return ITERATION_CONTINUE;

	if ( m_nAlreadyHit < m_nMaxHits )
	{
		m_AlreadyHit[m_nAlreadyHit] = pEnt;
		m_nAlreadyHit++;
	}

	return ITERATION_CONTINUE;
}

IterationRetval_t CBotDoorEnumerator::EnumElement( IHandleEntity *pHandleEntity)
{
	CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

	if ( pEnt == nullptr )
		return ITERATION_CONTINUE;

	if ( ( pEnt->ObjectCaps() & FCAP_IMPULSE_USE ) == 0 )
	{
		return ITERATION_CONTINUE;
	}

	if ( FClassnameIs( pEnt, "func_door*" ) )
	{
		CBaseDoor *door = dynamic_cast<CBaseDoor *>(pEnt);
		if ( !door )
		{
			return ITERATION_CONTINUE;
		}

		if ( door->m_toggle_state == TS_GOING_UP || door->m_toggle_state == TS_GOING_DOWN )
		{
			return ITERATION_CONTINUE;
		}
	}
	else if ( FClassnameIs( pEnt, "prop_door*" ) )
	{
		CBasePropDoor *door = dynamic_cast<CBasePropDoor *>(pEnt);
		if ( !door )
		{
			return ITERATION_CONTINUE;
		}

		if ( door->IsDoorOpening() || door->IsDoorClosing() )
		{
			return ITERATION_CONTINUE;
		}
	}
	else
	{
		return ITERATION_CONTINUE;
	}

	if ( m_nAlreadyHit < m_nMaxHits )
	{
		m_AlreadyHit[m_nAlreadyHit] = pEnt;
		m_nAlreadyHit++;
	}

	return ITERATION_CONTINUE;
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_SERVERCLASS( CParticleFire, DT_ParticleFire )

LINK_ENTITY_TO_CLASS( env_particlefire, CParticleFire );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

CParticleFire::CParticleFire()
{
}

using namespace ::ks::reflect;

LINK_ENTITY_TO_CLASS( env_particlelight, CParticleLight );

//Save/restore
IMPLEMENT_REFLECT_DATAMAP( CParticleLight )

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
CParticleLight::CParticleLight()
{
	m_flIntensity = 5000;
	m_vColor.Init( 1, 0, 0 );
	m_PSName = NULL_STRING;
	m_bDirectional = false;
}

class CBaseEntity;
class CBasePlayer;
class CEffectData;

using namespace ::ks::reflect;

extern void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot = -1 );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GetAttachTypeFromString( const char *pszString )
{
	if ( !pszString || !pszString[0] )
		return -1;

	// If you add new attach types, you need to add them to this list
	static const char *pAttachmentNames[MAX_PATTACH_TYPES] =
	{
		"start_at_origin",		// PATTACH_ABSORIGIN = 0,
		"follow_origin",		// PATTACH_ABSORIGIN_FOLLOW,
		"start_at_customorigin",// PATTACH_CUSTOMORIGIN,
		"follow_customorigin",  // PATTACH_CUSTOMORIGIN_FOLLOW,
		"start_at_attachment",	// PATTACH_POINT,
		"follow_attachment",	// PATTACH_POINT_FOLLOW,
		"follow_eyes",			// PATTACH_EYES_FOLLOW,
		"follow_overhead",		// PATTACH_OVERHEAD_FOLLOW
		"world_origin",			// PATTACH_WORLDORIGIN
		"follow_rootbone"		// PATTACH_ROOTBONE_FOLLOW
	};

	for ( int i = 0; i < MAX_PATTACH_TYPES; i++ )
	{
		if ( FStrEq( pAttachmentNames[i], pszString ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParseParticleEffects( bool bLoadSheets )
{

	g_pParticleSystemMgr->ShouldLoadSheets( bLoadSheets );

	std::vector< std::string > files;
	GetParticleManifest( files );

	int nCount = std::ssize( files );
	for ( int i = 0; i < nCount; ++i )
	{
		g_pParticleSystemMgr->ReadParticleConfigFile( files[i].c_str(), false, false );
	}

	g_pParticleSystemMgr->DecommitTempMemory();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PrecacheStandardParticleSystems( )
{
	// Now add each particle system name to the network string pool, so we can send string_t's 
	// down to the client instead of full particle system names.
	for ( int i = 0; i < g_pParticleSystemMgr->GetParticleSystemCount(); i++ )
	{
		const char *pParticleSystemName = g_pParticleSystemMgr->GetParticleSystemNameFromIndex(i);
		CParticleSystemDefinition *pParticleSystem = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
		if ( pParticleSystem->ShouldAlwaysPrecache() )
		{
			PrecacheParticleSystem( pParticleSystemName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iAttachment = -1;
	if ( pEntity && pEntity->GetBaseAnimating() )
	{
		// Find the attachment point index
		iAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
		if ( iAttachment == -1 )
		{
			Warning("Model '%s' doesn't have attachment '%s' to attach particle system '%s' to.\n", STRING(pEntity->GetBaseAnimating()->GetModelName()), pszAttachmentName, pszParticleName );
			return;
		}
	}

	DispatchParticleEffect( pszParticleName, iAttachType, pEntity, iAttachment, bResetAllParticlesOnEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/, bool bAllowDormantSpawn )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}
	if ( bAllowDormantSpawn )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_ALLOW_DORMANT;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
	if ( filter )
		DispatchEffect( *filter, 0.0f, "ParticleEffect", data );
	else
		DispatchEffect( "ParticleEffect", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, CBaseEntity *pOtherEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity && pOtherEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
		data.m_nOtherEntIndex = pOtherEntity->entindex();

		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
	DispatchEffect( "ParticleEffect", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int nEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	data.m_nHitBox = nEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = iAttachType;
	}
	else
	{
		data.m_nEntIndex = 0;
	}

	DispatchEffect( "ParticleEffect", data );
}

void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	int nEffectIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( nEffectIndex, vecOrigin, vecAngles, iAttachType, pEntity, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	CEffectData	data;

	data.m_nHitBox = iEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vStart = vecStart;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = PATTACH_CUSTOMORIGIN;
	}
	else
	{
		data.m_nEntIndex = 0;
	}

	if ( filter )
	{
		DispatchEffect( *filter, 0.0f, "ParticleEffect", data );
	}
	else
	{
		DispatchEffect( "ParticleEffect", data );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecOrigin, vecAngles, pEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: Yet another overload, lets us supply vecStart
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecStart, vecAngles, pEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffects( CBaseEntity *pEntity )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
	}

	DispatchEffect( "ParticleEffectStop", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_nEntIndex = pEntity->entindex();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );

	DispatchEffect( "ParticleEffectStop", data );
}

	extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );

	ConVar particle_test_file( side_registry(), "particle_test_file", "", FCVAR_CHEAT, "Name of the particle system to dynamically spawn" );
	ConVar particle_test_attach_mode( side_registry(), "particle_test_attach_mode", "follow_attachment", FCVAR_CHEAT, "Possible Values: 'start_at_attachment', 'follow_attachment', 'start_at_origin', 'follow_origin'" );
	ConVar particle_test_attach_attachment( side_registry(), "particle_test_attach_attachment", "0", FCVAR_CHEAT, "Attachment index for attachment mode" );

	void Particle_Test_Start( CBasePlayer* pPlayer, const char *name, bool bStart )
	{
		if ( !pPlayer )
			return;

		int iAttachType = GetAttachTypeFromString( particle_test_attach_mode.GetString() );

		if ( iAttachType < 0 )
		{
			Warning( "Invalid attach type specified for particle_test in cvar 'particle_test_attach_mode.\n" );
			return;
		}

		int iAttachmentIndex = particle_test_attach_attachment.GetInt();

		const char *pszParticleFile = particle_test_file.GetString();

		CBaseEntity *pEntity = nullptr;
		while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != nullptr )
		{
			/* 
			Fire the test particle system on this entity
			*/

			DispatchParticleEffect( 
				pszParticleFile,
				(ParticleAttachment_t)iAttachType,
				pEntity,
				iAttachmentIndex,
				true );				// stops existing particle systems
		}
	}

	void CC_Particle_Test_Start( const CCommand& args )
	{
		Particle_Test_Start( UTIL_GetCommandClient(), args[1], true );
	}
	static ConCommand particle_test_start(side_registry(), "particle_test_start", CC_Particle_Test_Start, "Dispatches the test particle system with the parameters specified in particle_test_file,\n particle_test_attach_mode and particle_test_attach_param on the entity the player is looking at.\n\tArguments:   	{entity_name} / {class_name} / no argument picks what player is looking at ", FCVAR_CHEAT);

	void Particle_Test_Stop( CBasePlayer* pPlayer, const char *name, bool bStart )
	{
		if ( !pPlayer )
			return;

		CBaseEntity *pEntity = nullptr;
		while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != nullptr )
		{
			//Stop all particle systems on the selected entity
			DispatchParticleEffect( "", PATTACH_ABSORIGIN, pEntity, 0, true );
		}
	}

	void CC_Particle_Test_Stop( const CCommand& args )
	{
		Particle_Test_Stop( UTIL_GetCommandClient(), args[1], false );
	}
	static ConCommand particle_test_stop(side_registry(), "particle_test_stop", CC_Particle_Test_Stop, "Stops all particle systems on the selected entities.\n\tArguments:   	{entity_name} / {class_name} / no argument picks what player is looking at ", FCVAR_CHEAT);

class CBasePlayer;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_SERVERCLASS( ParticleSmokeGrenade, DT_ParticleSmokeGrenade )

LINK_ENTITY_TO_CLASS( env_particlesmokegrenade, ParticleSmokeGrenade );

ParticleSmokeGrenade::ParticleSmokeGrenade()
{
	m_CurrentStage = 0;
	m_FadeStartTime = 17;
	m_FadeEndTime = 22;
	
	m_MinColor = Vector(MIN_SMOKE_TINT, MIN_SMOKE_TINT, MIN_SMOKE_TINT);
	m_MaxColor = Vector(MAX_SMOKE_TINT, MAX_SMOKE_TINT, MAX_SMOKE_TINT);

	m_flSpawnTime = gpGlobals->curtime;
}

ParticleSmokeGrenade::~ParticleSmokeGrenade()
{
}

// Smoke grenade particles should always transmitted to clients.  If not, a client who
// enters the PVS late will see the smoke start billowing from then, allowing better vision.
int ParticleSmokeGrenade::UpdateTransmitState_Impl( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return SetTransmitState( FL_EDICT_DONTSEND );

	return SetTransmitState( FL_EDICT_ALWAYS );
}

void ParticleSmokeGrenade::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	SetNextThink( gpGlobals->curtime );
	m_creatorPlayer = nullptr;
}

void ParticleSmokeGrenade::SetCreator(CBasePlayer *creator)
{
	m_creatorPlayer = creator;
}

CBasePlayer* ParticleSmokeGrenade::GetCreator()
{
	return (CBasePlayer*)m_creatorPlayer.Get();
}

void ParticleSmokeGrenade::FillVolume()
{
	m_CurrentStage = 1;
	CollisionProp()->SetCollisionBounds( Vector( -50, -50, -50 ), Vector( 50, 50, 50 ) );
}

void ParticleSmokeGrenade::SetFadeTime(float startTime, float endTime)
{
	m_FadeStartTime = startTime;
	m_FadeEndTime = endTime;
}

// Fade start and end are relative to current time
void ParticleSmokeGrenade::SetRelativeFadeTime(float startTime, float endTime)
{
	float flCurrentTime = gpGlobals->curtime - m_flSpawnTime;

	m_FadeStartTime = flCurrentTime + startTime;
	m_FadeEndTime = flCurrentTime + endTime;
}

// Fade start and end are relative to current time
void ParticleSmokeGrenade::SetSmokeColor( Vector color )
{
	m_MinColor = color * MIN_SMOKE_TINT;
	m_MaxColor = color * MAX_SMOKE_TINT;
}

void ParticleSmokeGrenade::Think_Impl()
{
	// Override, don't extend.  (Baseclass's Think just deletes.)

	float now = gpGlobals->curtime;
	if( now >= (m_flSpawnTime + m_FadeEndTime) )
	{
		// We are done
		UTIL_Remove(this);
		return;
	}

	const float PSG_THINK_DELAY = 1.0f;
	SetNextThink(now + PSG_THINK_DELAY);
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: An entity that spawns and controls a particle system

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// Stripped down CBaseEntity send table
IMPLEMENT_REFLECT_SERVERCLASS( CParticleSystem, DT_ParticleSystem )

IMPLEMENT_REFLECT_DATAMAP( CParticleSystem )

LINK_ENTITY_TO_CLASS( info_particle_system, CParticleSystem );

CParticleSystem::CParticleSystem( void ) : m_bNoSave( false )
{
	for( int i = 0; i != kSERVERCONTROLLEDPOINTS; ++i )
	{
		m_iServerControlPointAssignments.GetForModify(i) = 255;
	}
}

//-----------------------------------------------------------------------------
// Precache 
//-----------------------------------------------------------------------------
void CParticleSystem::Precache_Impl( void )
{
	const char *pParticleSystemName = STRING( m_iszEffectName );
	if ( pParticleSystemName == nullptr || pParticleSystemName[0] == 0 )
	{
		Warning( "info_particle_system (%s) has no particle system name specified!\n", GetEntityName().ToCStr() );
	}

	PrecacheParticleSystem( pParticleSystemName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	Precache();
	m_iEffectIndex = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// Find our particle effect index
	m_iEffectIndex = GetParticleSystemIndex( STRING(m_iszEffectName) );

	if ( m_bStartActive )
	{
		m_bStartActive = false;
		StartParticleSystem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CParticleSystem::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "snapshot_file" ) )
	{
		V_strncpy( m_szSnapshotFileName.GetForModify(), szValue, MAX_PATH );
		return true;
	}
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CParticleSystem::GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "snapshot_file" ) )
	{
		V_snprintf( szValue, iMaxLen, "%s", m_szSnapshotFileName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue_Impl( szKeyName, szValue, iMaxLen );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StartParticleSystemThink( void )
{
	StartParticleSystem();
}

//-----------------------------------------------------------------------------
// Purpose: Always transmitted to clients
//-----------------------------------------------------------------------------
int CParticleSystem::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StartParticleSystem( void )
{
	if ( m_bActive == false )
	{
		m_flStartTime = gpGlobals->curtime;
		m_bActive = true;
		
		// Setup our control points at this time (in case our targets weren't around at spawn time)
		ReadControlPointEnts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StopParticleSystem( int nStopType )
{
	m_bActive = false;
	m_nStopType = nStopType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::InputStart( inputdata_t &inputdata )
{
	StartParticleSystem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::InputStop( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_NORMAL );
}

void CParticleSystem::InputDestroy( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_DESTROY_IMMEDIATELY );
}

void CParticleSystem::InputStopEndCap( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_PLAY_ENDCAP );
}

//-----------------------------------------------------------------------------
// Purpose: Find each entity referred to by m_iszControlPointNames and 
// resolve it into the corresponding slot in m_hControlPointEnts
//-----------------------------------------------------------------------------
void CParticleSystem::ReadControlPointEnts( void )
{
	for ( int i = 0 ; i < kMAXCONTROLPOINTS; ++i )
	{
		if ( m_iszControlPointNames[i] == NULL_STRING )
			continue;

		CBaseEntity *pPointEnt = gEntList.FindEntityGeneric( nullptr, STRING( m_iszControlPointNames[i] ), this );
		contract_assert( pPointEnt != nullptr );
		if ( pPointEnt == nullptr )
		{
			Warning("Particle system %s could not find control point entity (%s)\n", GetEntityName().ToCStr(), m_iszControlPointNames[i].ToCStr() );
			continue;
		}

		m_hControlPointEnts.Set( i, pPointEnt );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Try to allocate one of the server controlled control points to 
// hold the value. Designed to let the server funnel some variables to
// particle systems (size, color, swirliness, ...)
//-----------------------------------------------------------------------------
bool CParticleSystem::SetControlPointValue( int iControlPoint, const Vector &vValue )
{
	for( int i = 0; i != kSERVERCONTROLLEDPOINTS; ++i )
	{
		if( m_iServerControlPointAssignments[i] == iControlPoint )
		{
			m_vServerControlPoints.GetForModify(i) = vValue;
			return true;
		}
		if( m_iServerControlPointAssignments[i] == 255 )
		{
			m_iServerControlPointAssignments.GetForModify(i) = iControlPoint;
			m_vServerControlPoints.GetForModify(i) = vValue;
			return true;
		}
	}

	Warning( "No free server controlled control points.\n" );
	return false; //already using up all of our server control points
}

//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
int CParticleSystem::ObjectCaps_Impl( void )
{ 
	int flags = 0;
	if ( m_bNoSave )
		flags = FCAP_DONT_SAVE;
	
	return BaseClass::ObjectCaps_Impl() | flags; 
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Used to create a path that can be followed by NPCs and trains.

class CPathCorner : public CPointEntity
{
	DECLARE_CLASS( CPathCorner, CPointEntity );
public:

	void	Spawn_Impl( );
	float	GetDelay_Impl( void ) { return m_flWait; }
	int		DrawDebugTextOverlays_Impl(void);
	void	DrawDebugGeometryOverlays_Impl(void);

	// Input handlers	
	[[= ks::reflect::Input{ .name = "SetNextPathCorner", .type = FIELD_STRING } ]] void InputSetNextPathCorner( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "InPass", .type = FIELD_VOID } ]] void InputInPass( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "wait" } ]] float			m_flWait;
	[[= ks::reflect::Key{ .name = "OnPass" } ]] COutputEvent	m_OnPass;
};

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

class CPathCornerCrash : public CPathCorner
{
	DECLARE_CLASS( CPathCornerCrash, CPathCorner );
};

LINK_ENTITY_TO_CLASS( path_corner_crash, CPathCornerCrash );

IMPLEMENT_REFLECT_DATAMAP( CPathCorner )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathCorner::Spawn_Impl( void )
{
	ASSERTSZ(GetEntityName() != NULL_STRING, "path_corner without a targetname");
}

//------------------------------------------------------------------------------
// Purpose: Sets the next path corner by name.
// Input  : String ID name of next path corner.
//-----------------------------------------------------------------------------
void CPathCorner::InputSetNextPathCorner( inputdata_t &inputdata )
{
	m_target = inputdata.value.StringID();
}

//-----------------------------------------------------------------------------
// Purpose: Fired by path followers as they pass the path corner.
//-----------------------------------------------------------------------------
void CPathCorner::InputInPass( inputdata_t &inputdata )
{
	m_OnPass.FireOutput( inputdata.pActivator, inputdata.pCaller, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CPathCorner::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// --------------
		// Print Target
		// --------------
		char tempstr[255];
		if (m_target!=NULL_STRING) 
		{
			V_snprintf(tempstr,sizeof(tempstr),"Target: %s",STRING(m_target));
		}
		else
		{
			V_strncpy(tempstr,"Target:   -  ",sizeof(tempstr));
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Override base class to add display of paths
//-----------------------------------------------------------------------------
void CPathCorner::DrawDebugGeometryOverlays_Impl(void) 
{
	// ----------------------------------------------
	// Draw line to next target is bbox is selected
	// ----------------------------------------------
	if (m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_ABSBOX_BIT))
	{
		NDebugOverlay::Box(GetAbsOrigin(), Vector(-10,-10,-10), Vector(10,10,10), 255, 100, 100, 0 ,0);

		if (m_target != NULL_STRING)
		{
			CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target );
			if (pTarget)
			{
				NDebugOverlay::Line(GetAbsOrigin(),pTarget->GetAbsOrigin(),255,100,100,true,0.0);
			}
		}
	}
	BaseClass::DrawDebugGeometryOverlays_Impl();
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Used to create a path that can be followed by NPCs and trains.

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CPathTrack )

LINK_ENTITY_TO_CLASS( path_track, CPathTrack );

//-----------------------------------------------------------------------------
// Finds circular paths
//-----------------------------------------------------------------------------
int CPathTrack::s_nCurrIterVal = 0;
bool CPathTrack::s_bIsIterating = false;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPathTrack::CPathTrack()
{
	m_nIterVal = -1;
	m_eOrientationType = TrackOrientation_FacePath;
}

//-----------------------------------------------------------------------------
// Spawn!
//-----------------------------------------------------------------------------
void CPathTrack::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	UTIL_SetSize(this, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_pnext = nullptr;
	m_pprevious = nullptr;
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CPathTrack::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	if ( GetEntityName() != NULL_STRING )		// Link to next, and back-link
	{
		Link();
	}
}

//-----------------------------------------------------------------------------
// Connects up the previons + next pointers 
//-----------------------------------------------------------------------------
void CPathTrack::Link( void  )
{
	CBaseEntity *pTarget;

	if ( m_target != NULL_STRING )
	{
		pTarget = gEntList.FindEntityByName( nullptr, m_target );

		if ( pTarget == this)
		{
			Warning("ERROR: path_track (%s) refers to itself as a target!\n", GetDebugName());
			
			//FIXME: Why were we removing this?  If it was already connected to, we weren't updating the other linked
			//		 end, causing problems with walking through bogus memory links!  -- jdw

			//UTIL_Remove(this);
			//return;
		}
		else if ( pTarget )
		{
			m_pnext = dynamic_cast<CPathTrack*>( pTarget );

			if ( m_pnext )		// If no next pointer, this is the end of a path
			{
				m_pnext->SetPrevious( this );
			}
		}
		else
		{
			Warning("Dead end link: %s\n", STRING( m_target ) );
		}
	}

	// Find "alternate" path
	if ( m_altName != NULL_STRING )
	{
		pTarget = gEntList.FindEntityByName( nullptr, m_altName );
		if ( pTarget )
		{
			m_paltpath = dynamic_cast<CPathTrack*>( pTarget );
			m_paltpath->SetPrevious( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Circular path checking
//-----------------------------------------------------------------------------
void CPathTrack::BeginIteration()
{
	contract_assert( !s_bIsIterating );
	++s_nCurrIterVal;
	s_bIsIterating = true;
}

void CPathTrack::EndIteration()
{
	contract_assert( s_bIsIterating );
	s_bIsIterating = false;
}

void CPathTrack::Visit()
{
	m_nIterVal = s_nCurrIterVal;
}

bool CPathTrack::HasBeenVisited() const
{
	return ( m_nIterVal == s_nCurrIterVal );
}

//-----------------------------------------------------------------------------
// Do we have an alternate path?
//-----------------------------------------------------------------------------
bool CPathTrack::HasAlternathPath() const
{
	return ( m_paltpath != nullptr ); 
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the track to or from its alternate path
//-----------------------------------------------------------------------------
void CPathTrack::ToggleAlternatePath( void )
{
	// Use toggles between two paths
	if ( m_paltpath != nullptr )
	{
		if ( FBitSet( m_spawnflags, SF_PATH_ALTERNATE ) == false )
		{
			EnableAlternatePath();
		}
		else
		{
			DisableAlternatePath();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathTrack::EnableAlternatePath( void )
{
	if ( m_paltpath != nullptr )
	{
		SETBITS( m_spawnflags, SF_PATH_ALTERNATE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathTrack::DisableAlternatePath( void )
{
	if ( m_paltpath != nullptr )
	{
		CLEARBITS( m_spawnflags, SF_PATH_ALTERNATE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputEnableAlternatePath( inputdata_t &inputdata )
{
	EnableAlternatePath();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputDisableAlternatePath( inputdata_t &inputdata )
{
	DisableAlternatePath();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputToggleAlternatePath( inputdata_t &inputdata )
{
	ToggleAlternatePath();
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the track to or from its alternate path
//-----------------------------------------------------------------------------
void CPathTrack::TogglePath( void )
{
	// Use toggles between two paths
	if ( FBitSet( m_spawnflags, SF_PATH_DISABLED ) )
	{
		EnablePath();
	}
	else
	{
		DisablePath();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathTrack::EnablePath( void )
{
	CLEARBITS( m_spawnflags, SF_PATH_DISABLED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathTrack::DisablePath( void )
{
	SETBITS( m_spawnflags, SF_PATH_DISABLED );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputEnablePath( inputdata_t &inputdata )
{
	EnablePath();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputDisablePath( inputdata_t &inputdata )
{
	DisablePath();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPathTrack::InputTogglePath( inputdata_t &inputdata )
{
	TogglePath();
}

void CPathTrack::DrawDebugGeometryOverlays_Impl() 
{
	// ----------------------------------------------
	// Draw line to next target is bbox is selected
	// ----------------------------------------------
	if (m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_ABSBOX_BIT))
	{
		if (m_pnext)
		{
			NDebugOverlay::Line(GetAbsOrigin(),m_pnext->GetAbsOrigin(),255,100,100,true,0.0);
		}
	}
	BaseClass::DrawDebugGeometryOverlays_Impl();
}

CPathTrack	*CPathTrack::ValidPath( CPathTrack	*ppath, int testFlag )
{
	if ( !ppath )
		return nullptr;

	if ( testFlag && FBitSet( ppath->m_spawnflags, SF_PATH_DISABLED ) )
		return nullptr;

	return ppath;
}

void CPathTrack::Project( CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist )
{
	if ( pstart && pend )
	{
		Vector dir = (pend->GetLocalOrigin() - pstart->GetLocalOrigin());
		VectorNormalize( dir );
		origin = pend->GetLocalOrigin() + dir * dist;
	}
}

CPathTrack *CPathTrack::GetNext( void )
{
	if ( m_paltpath && FBitSet( m_spawnflags, SF_PATH_ALTERNATE ) && !FBitSet( m_spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pnext;
}

CPathTrack *CPathTrack::GetPrevious( void )
{
	if ( m_paltpath && FBitSet( m_spawnflags, SF_PATH_ALTERNATE ) && FBitSet( m_spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pprevious;
}

void CPathTrack::SetPrevious( CPathTrack *pprev )
{
	// Only set previous if this isn't my alternate path
	if ( pprev && !FStrEq( STRING(pprev->GetEntityName()), STRING(m_altName) ) )
		m_pprevious = pprev;
}

CPathTrack *CPathTrack::GetNextInDir( bool bForward )
{
	if ( bForward )
		return GetNext();
	
	return GetPrevious();
}

//-----------------------------------------------------------------------------
// Purpose: Assumes this is ALWAYS enabled
// Input  : origin - position along path to look ahead from
//			dist - distance to look ahead, negative values look backward
//			move - 
// Output : Returns the track that we will be PAST in 'dist' units.
//-----------------------------------------------------------------------------
CPathTrack *CPathTrack::LookAhead( Vector &origin, float dist, int move, CPathTrack **pNextNext )
{
	CPathTrack *pcurrent = this;
	float originalDist = dist;
	Vector currentPos = origin;

	bool bForward = true;
	if ( dist < 0 )
	{
		// Travelling backwards along the path.
		dist = -dist;
		bForward = false;
	}

	// Move along the path, until we've gone 'dist' units or run out of path.
	while ( dist > 0 )
	{
		// If there is no next path track, or it's disabled, we're done.
		if ( !ValidPath( pcurrent->GetNextInDir( bForward ), move ) )
		{
			if ( !move )
			{
				Project( pcurrent->GetNextInDir( !bForward ), pcurrent, origin, dist );
			}

			return nullptr;
		}

		// The next path track is valid. How far are we from it?
		Vector dir = pcurrent->GetNextInDir( bForward )->GetLocalOrigin() - currentPos;
		float length = dir.Length();

		// If we are at the next node and there isn't one beyond it, return the next node.
		if ( !length  && !ValidPath( pcurrent->GetNextInDir( bForward )->GetNextInDir( bForward ), move ) )
		{
			if ( pNextNext )
			{
				*pNextNext = nullptr;
			}

			if ( dist == originalDist )
			{
				// Didn't move at all, must be in a dead end.
				return nullptr;
			}

			return pcurrent->GetNextInDir( bForward );
		}

		// If we don't hit the next path track within the distance remaining, we're done.
		if ( length > dist )
		{
			origin = currentPos + ( dir * ( dist / length ) );
			if ( pNextNext )
			{
				*pNextNext = pcurrent->GetNextInDir( bForward );
			}

			return pcurrent;
		}

		// We hit the next path track, advance to it.
		dist -= length;
		currentPos = pcurrent->GetNextInDir( bForward )->GetLocalOrigin();
		pcurrent = pcurrent->GetNextInDir( bForward );
		origin = currentPos;
	}

	// We consumed all of the distance, and exactly landed on a path track.
	if ( pNextNext )
	{
		*pNextNext = pcurrent->GetNextInDir( bForward );
	}

	return pcurrent;
}

// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack::Nearest( const Vector &origin )
{
	int			deadCount;
	float		minDist, dist;
	Vector		delta;
	CPathTrack	*ppath, *pnearest;

	delta = origin - GetLocalOrigin();
	delta.z = 0;
	minDist = delta.Length();
	pnearest = this;
	ppath = GetNext();

	// Hey, I could use the old 2 racing pointers solution to this, but I'm lazy :)
	deadCount = 0;
	while ( ppath && ppath != this )
	{
		deadCount++;
		if ( deadCount > 9999 )
		{
			Warning( "Bad sequence of path_tracks from %s\n", GetDebugName() );
			contract_assert( 0 );
			return nullptr;
		}
		delta = origin - ppath->GetLocalOrigin();
		delta.z = 0;
		dist = delta.Length();
		if ( dist < minDist )
		{
			minDist = dist;
			pnearest = ppath;
		}
		ppath = ppath->GetNext();
	}
	return pnearest;
}

//-----------------------------------------------------------------------------
// Purpose: Returns how the path follower should orient itself when moving
//			through this path track.
//-----------------------------------------------------------------------------
TrackOrientationType_t CPathTrack::GetOrientationType()
{
	return m_eOrientationType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CPathTrack::GetOrientation( bool bForwardDir )
{
	TrackOrientationType_t eOrient = GetOrientationType();
	if ( eOrient == TrackOrientation_FacePathAngles )
	{
		return GetLocalAngles();
	}

	CPathTrack *pPrev = this;
	CPathTrack *pNext = GetNextInDir( bForwardDir );

	if ( !pNext )
	{	pPrev = GetNextInDir( !bForwardDir );
		pNext = this;
	}

	Vector vecDir = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();

	QAngle angDir;
	VectorAngles( vecDir, angDir );
	return angDir;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pent - 
// Output : CPathTrack
//-----------------------------------------------------------------------------
CPathTrack *CPathTrack::Instance( edict_t *pent )
{
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if ( FClassnameIs( pEntity, "path_track" ) )
		return (CPathTrack *)pEntity;
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPathTrack::InputPass( inputdata_t &inputdata )
{
	m_OnPass.FireOutput( inputdata.pActivator, this );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CPhysThruster;

//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CConstantForceController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	void Init( IMotionEvent::simresult_e controlType ) 
	{ 
		m_controlType = controlType;
	}

	void SetConstantForce( const Vector &linear, const AngularImpulse &angular );
	void ScaleConstantForce( float scale );
	
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	IMotionEvent::simresult_e	m_controlType;
	Vector			m_linear;
	AngularImpulse	m_angular;
	Vector			m_linearSave;
	AngularImpulse	m_angularSave;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CConstantForceController )

void CConstantForceController::SetConstantForce( const Vector &linear, const AngularImpulse &angular )
{
	m_linear = linear;
	m_angular = angular;
	// cache these for scaling later
	m_linearSave = linear;
	m_angularSave = angular;
}

void CConstantForceController::ScaleConstantForce( float scale )
{
	m_linear = m_linearSave * scale;
	m_angular = m_angularSave * scale;
}

IMotionEvent::simresult_e CConstantForceController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = m_linear;
	angular = m_angular;
	
	return m_controlType;
}

// UNDONE: Make these logical entities
//-----------------------------------------------------------------------------
// Purpose: This is a general entity that has a force/motion controller that 
//			simply integrates a constant linear/angular acceleration
//-----------------------------------------------------------------------------
class CPhysForce : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysForce, CPointEntity );

	CPhysForce();
	~CPhysForce();

	DECLARE_DATADESC();

	void OnRestore_Impl( );
	void Spawn_Impl( void );
	void Activate_Impl( void );

	void ForceOn( void );
	void ForceOff( void );
	void ActivateForce( void );
	void SetAttachedObject( EHANDLE hObject ) { m_attachedObject = hObject; }

	// Input handlers
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputActivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "scale", .type = FIELD_FLOAT } ]] void InputForceScale( inputdata_t &inputdata );

	void SaveForce( void );
	void ScaleForce( float scale );

	// MUST IMPLEMENT THIS IN DERIVED CLASS
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular ) = 0;

	// optional
	virtual void OnActivate( void ) {}

protected:	
	IPhysicsMotionController	*m_pController;

	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t		m_nameAttach;
	[[= ks::reflect::Key{ .name = "force" } ]] float			m_force;
	[[= ks::reflect::Key{ .name = "forcetime" } ]] float			m_forceTime;
	EHANDLE			m_attachedObject;
	bool			m_wasRestored;

	CConstantForceController m_integrator;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysForce )

CPhysForce::CPhysForce( void )
{
	m_pController = nullptr;
	m_wasRestored = false;
}

CPhysForce::~CPhysForce()
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
	}
}

void CPhysForce::Spawn_Impl( void )
{
	if ( m_spawnflags & SF_THRUST_LOCAL_ORIENTATION )
	{
		m_integrator.Init( IMotionEvent::SIM_LOCAL_ACCELERATION );
	}
	else
	{
		m_integrator.Init( IMotionEvent::SIM_GLOBAL_ACCELERATION );
	}
}

void CPhysForce::OnRestore_Impl( )
{
	BaseClass::OnRestore_Impl();

	if ( m_pController )
	{
		m_pController->SetEventHandler( &m_integrator );
	}
	m_wasRestored = true;
}

void CPhysForce::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	if ( m_pController )
	{
		m_pController->WakeObjects();
	}
	if ( m_wasRestored )
		return;

	if ( m_attachedObject == nullptr )
	{
		m_attachedObject = gEntList.FindEntityByName( nullptr, m_nameAttach );
	}
	
	// Let the derived class set up before we throw the switch
	OnActivate();

	if ( m_spawnflags & SF_THRUST_STARTACTIVE )
	{
		ForceOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputActivate( inputdata_t &inputdata )
{
	ForceOn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputDeactivate( inputdata_t &inputdata )
{
	ForceOff();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputForceScale( inputdata_t &inputdata )
{
	ScaleForce( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::ForceOn( void )
{
	if ( m_pController )
		return;

	ActivateForce();
	if ( m_forceTime )
	{
		SetNextThink( gpGlobals->curtime + m_forceTime );
		SetThink( &CPhysForce::ForceOff );
	}
}

void CPhysForce::ActivateForce( void )
{
	IPhysicsObject *pPhys = nullptr;
	if ( m_attachedObject )
	{
		pPhys = m_attachedObject->VPhysicsGetObject();
	}
	
	if ( !pPhys )
		return;

	Vector linear;
	AngularImpulse angular;

	SetupForces( pPhys, linear, angular );

	m_integrator.SetConstantForce( linear, angular );
	m_pController = physenv->CreateMotionController( &m_integrator );
	m_pController->AttachObject( pPhys, true );
	// Make sure the object is simulated
	pPhys->Wake();
}

void CPhysForce::ForceOff( void )
{
	if ( !m_pController )
		return;

	physenv->DestroyMotionController( m_pController );
	m_pController = nullptr;
	SetThink( nullptr );
	SetNextThink( TICK_NEVER_THINK );
	IPhysicsObject *pPhys = nullptr;
	if ( m_attachedObject )
	{
		pPhys = m_attachedObject->VPhysicsGetObject();
		if ( pPhys )
		{
			pPhys->Wake();
		}
	}
}

void CPhysForce::ScaleForce( float scale )
{
	if ( !m_pController )
		ForceOn();

	m_integrator.ScaleConstantForce( scale );
	m_pController->WakeObjects();
}

//-----------------------------------------------------------------------------
// Purpose: A rocket-engine/thruster based on the force controller above
//			Calculate the force (and optional torque) that the engine would create
//-----------------------------------------------------------------------------
class CPhysThruster : public CPhysForce
{
	DECLARE_CLASS( CPhysThruster, CPhysForce );
public:

	virtual void OnActivate( void );
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular );

private:
	friend CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags );
	
	Vector			m_localOrigin;
};

LINK_ENTITY_TO_CLASS( phys_thruster, CPhysThruster );

//-----------------------------------------------------------------------------
// Purpose: Use this to spawn a keepupright controller via code instead of map-placed
//-----------------------------------------------------------------------------
CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags )
{
	CPhysThruster *pThruster = ( CPhysThruster* )CBaseEntity::Create( "phys_thruster", vecOrigin, vecAngles, pOwner );
	if ( pThruster )
	{
		pThruster->AddSpawnFlags( nFlags );
		pThruster->m_attachedObject = pOwner;
		pThruster->m_force = flForce;
		pThruster->m_forceTime = flForcetime;
		pThruster->Spawn();
		pThruster->Activate();
		if ( bActive )
			pThruster->ForceOn();
	}

	return pThruster;
}

void CPhysThruster::OnActivate( void )
{
	if ( m_attachedObject != nullptr )
	{
		matrix3x4_t worldToAttached, thrusterToAttached;
		MatrixInvert( m_attachedObject->EntityToWorldTransform(), worldToAttached );
		ConcatTransforms( worldToAttached, EntityToWorldTransform(), thrusterToAttached );
		MatrixGetColumn( thrusterToAttached, 3, m_localOrigin );

		if ( HasSpawnFlags( SF_THRUST_LOCAL_ORIENTATION ) )
		{
			QAngle angles;
			MatrixAngles( thrusterToAttached, angles );
			SetLocalAngles( angles );
		}
		// maintain the local relationship with this entity
		// it may move before the thruster is activated
		if ( HasSpawnFlags( SF_THRUST_IGNORE_POS ) )
		{
			m_localOrigin.Init();
		}
	}
}

// utility function to duplicate this call in local space
void CalculateVelocityOffsetLocal( IPhysicsObject *pPhys, const Vector &forceLocal, const Vector &positionLocal, Vector &outVelLocal, AngularImpulse &outAngular )
{
	Vector posWorld, forceWorld;
	pPhys->LocalToWorld( &posWorld, positionLocal );
	pPhys->LocalToWorldVector( &forceWorld, forceLocal );
	Vector velWorld;
	pPhys->CalculateVelocityOffset( forceWorld, posWorld, &velWorld, &outAngular );
	pPhys->WorldToLocalVector( &outVelLocal, velWorld );
}

void CPhysThruster::SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular )
{
	Vector thrustVector;
	AngleVectors( GetLocalAngles(), &thrustVector );
	thrustVector *= m_force;

	// multiply the force by mass (it's actually just an acceleration)
	if ( m_spawnflags & SF_THRUST_MASS_INDEPENDENT )
	{
		thrustVector *= pPhys->GetMass();
	}
	if ( m_spawnflags & SF_THRUST_LOCAL_ORIENTATION )
	{
		CalculateVelocityOffsetLocal( pPhys, thrustVector, m_localOrigin, linear, angular );
	}
	else
	{
		Vector position;
		VectorTransform( m_localOrigin, m_attachedObject->EntityToWorldTransform(), position );
		pPhys->CalculateVelocityOffset( thrustVector, position, &linear, &angular );
	}

	if ( !(m_spawnflags & SF_THRUST_FORCE) )
	{
		// clear out force
		linear.Init();
	}

	if ( !(m_spawnflags & SF_THRUST_TORQUE) )
	{
		// clear out torque
		angular.Init();
	}
}

//-----------------------------------------------------------------------------
// Purpose: A controllable motor - exerts torque
//-----------------------------------------------------------------------------
class CPhysTorque : public CPhysForce
{
	DECLARE_CLASS( CPhysTorque, CPhysForce );
public:
	DECLARE_DATADESC();
	void Spawn_Impl( void );
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular );
private:	
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector m_axis;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysTorque )

LINK_ENTITY_TO_CLASS( phys_torque, CPhysTorque );

void CPhysTorque::Spawn_Impl( void )
{
	// force spawnflags to agree with implementation of this class
	m_spawnflags |= SF_THRUST_TORQUE | SF_THRUST_MASS_INDEPENDENT;
	m_spawnflags &= ~SF_THRUST_FORCE;

	m_axis -= GetAbsOrigin();
	VectorNormalize(m_axis);
	UTIL_SnapDirectionToAxis( m_axis );
	BaseClass::Spawn_Impl();
}

void CPhysTorque::SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular )
{
	// clear out force
	linear.Init();

	matrix3x4_t matrix;
	pPhys->GetPositionMatrix( &matrix );

	// transform motor axis to local space
	Vector axis_ls;
	VectorIRotate( m_axis, matrix, axis_ls );

	// Set torque to be around selected axis
	angular = axis_ls * m_force;
}

//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CMotorController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	float		m_speed;
	float		m_maxTorque;
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector		m_axis;
	[[= ks::reflect::Key{ .name = "inertiafactor" } ]] float		m_inertiaFactor;

	float		m_lastSpeed;
	float		m_lastAcceleration;
	float		m_lastForce;
	float		m_restistanceDamping;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CMotorController )

IMotionEvent::simresult_e CMotorController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = vec3_origin;
	angular = vec3_origin;

	if ( m_speed == 0 )
		return SIM_NOTHING;

	matrix3x4_t matrix;
	pObject->GetPositionMatrix( &matrix );
	AngularImpulse currentRotAxis;
	
	// currentRotAxis is in local space
	pObject->GetVelocity( nullptr, &currentRotAxis );
	// transform motor axis to local space
	Vector motorAxis_ls;
	VectorIRotate( m_axis, matrix, motorAxis_ls );
	float currentSpeed = DotProduct( currentRotAxis, motorAxis_ls );
	
	float inertia = DotProductAbs( pObject->GetInertia(), motorAxis_ls );

	// compute absolute acceleration, don't integrate over the timestep
	float accel = m_speed - currentSpeed;
	float rotForce = accel * inertia * m_inertiaFactor;

	// BUGBUG: This heuristic is a little flaky
	// UNDONE: Make a better heuristic for speed control
	if ( std::fabsf(m_lastAcceleration) > 0 )
	{
		float deltaSpeed = currentSpeed - m_lastSpeed;
		// make sure they are going the same way
		if ( deltaSpeed * accel > 0 )
		{
			float factor = deltaSpeed / m_lastAcceleration;
			factor = 1 - clamp( factor, 0, 1 );
			rotForce += m_lastForce * factor * m_restistanceDamping;
		}
		else 
		{
			if ( currentSpeed != 0 )
			{
				// have we reached a steady state that isn't our target?
				float increase = deltaSpeed / m_lastAcceleration;
				if ( fabsf(increase) < 0.05 )
				{
					rotForce += m_lastForce * m_restistanceDamping;
				}
			}
		}
	}
	// -------------------------------------------------------

	if ( m_maxTorque != 0 )
	{
		if ( rotForce > m_maxTorque )
		{
			rotForce = m_maxTorque;
		}
		else if ( rotForce < -m_maxTorque )
		{
			rotForce = -m_maxTorque;
		}
	}

	m_lastForce = rotForce;
	m_lastAcceleration = (rotForce / inertia);
	m_lastSpeed = currentSpeed;

	// this is in local space
	angular = motorAxis_ls * rotForce;
	
	return SIM_LOCAL_FORCE;
}

static constexpr auto SF_MOTOR_START_ON = 0x0001; // starts on by default
static constexpr auto SF_MOTOR_NOCOLLIDE = 0x0002; // don't collide with world geometry
static constexpr auto SF_MOTOR_HINGE = 0x0004; // motor also acts as a hinge constraining the object to this axis
// NOTE: THIS DOESN'T WORK YET
static constexpr auto SF_MOTOR_LOCAL = 0x0008; // Maintain local relationship with the attached object

class CPhysMotor : public CLogicalEntity
{
	DECLARE_CLASS( CPhysMotor, CLogicalEntity );
public:
	~CPhysMotor();
	DECLARE_DATADESC();
	void Spawn_Impl( void );
	void Activate_Impl( void );
	void Think_Impl( void );

	void TurnOn( void );
	void TargetSpeedChanged( void );
	void OnRestore_Impl();

	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetTargetSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	void CalculateAcceleration();

	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t	m_nameAttach;
	EHANDLE		m_attachedObject;
	[[= ks::reflect::Key{ .name = "spinup" } ]] float		m_spinUp;
	[[= ks::reflect::Key{ .name = "addangaccel" } ]] float		m_additionalAcceleration;
	float		m_angularAcceleration;
	float		m_lastTime;
	// FIXME: can we remove m_flSpeed from CBaseEntity?
	//float m_flSpeed;

	IPhysicsConstraint	*m_pHinge;
	IPhysicsMotionController *m_pController;
	CMotorController m_motor;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysMotor )

LINK_ENTITY_TO_CLASS( phys_motor, CPhysMotor );

void CPhysMotor::CalculateAcceleration()
{
	if ( m_spinUp )
	{
		m_angularAcceleration = std::fabsf(m_flSpeed / m_spinUp);
	}
	else
	{
		m_angularAcceleration = std::fabsf(m_flSpeed);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that sets a speed to spin up or down to.
//-----------------------------------------------------------------------------
void CPhysMotor::InputSetTargetSpeed( inputdata_t &inputdata )
{
	if ( m_flSpeed == inputdata.value.Float() )
		return;

	m_flSpeed = inputdata.value.Float();
	TargetSpeedChanged();
	CalculateAcceleration();
}

void CPhysMotor::TargetSpeedChanged( void )
{
	SetNextThink( gpGlobals->curtime );
	m_lastTime = gpGlobals->curtime;
	m_pController->WakeObjects();
}

//------------------------------------------------------------------------------
// Purpose: Input handler that turns the motor on.
//------------------------------------------------------------------------------
void CPhysMotor::InputTurnOn( inputdata_t &inputdata )
{
	TurnOn();
}

//------------------------------------------------------------------------------
// Purpose: Input handler that turns the motor off.
//------------------------------------------------------------------------------
void CPhysMotor::InputTurnOff( inputdata_t &inputdata )
{
	m_motor.m_speed = 0;
	SetNextThink( TICK_NEVER_THINK );
}

CPhysMotor::~CPhysMotor()
{
	if ( m_attachedObject && m_pHinge )
	{
		IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();
		if ( pPhys )
		{
			PhysClearGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
		}
	}

	physenv->DestroyConstraint( m_pHinge );
	physenv->DestroyMotionController( m_pController );
}

void CPhysMotor::Spawn_Impl( void )
{
	m_motor.m_axis -= GetLocalOrigin();
	float axisLength = VectorNormalize(m_motor.m_axis);
	// double check that the axis is at least a unit long. If not, warn and self-destruct.
	if ( axisLength > 1.0f )
	{
		UTIL_SnapDirectionToAxis( m_motor.m_axis );
	}
	else
	{
		Warning("phys_motor %s does not have a valid axis helper, and self-destructed!\n", GetDebugName());

		m_motor.m_speed = 0;
		SetNextThink( TICK_NEVER_THINK );

		UTIL_Remove(this);
	}
}

void CPhysMotor::TurnOn( void )
{
	CBaseEntity *pAttached = m_attachedObject;
	if ( !pAttached )
		return;

	IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
	if ( pPhys )
	{
		m_pController->WakeObjects();
		// If the current speed is zero, the objects can run a tick without getting torque'd and go back to sleep
		// so force a think now and have some acceleration happen before the controller gets called.
		m_lastTime = gpGlobals->curtime - TICK_INTERVAL;
		Think();
	}
}

void CPhysMotor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	
	// This gets called after all objects spawn and after all objects restore
	if ( m_attachedObject == nullptr )
	{
		CBaseEntity *pAttach = gEntList.FindEntityByName( nullptr, m_nameAttach );
		if ( pAttach && pAttach->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			m_attachedObject = pAttach;
			IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();
			CalculateAcceleration();
			matrix3x4_t matrix;
			pPhys->GetPositionMatrix( &matrix );
			Vector motorAxis_ls;
			VectorIRotate( m_motor.m_axis, matrix, motorAxis_ls );
			float inertia = DotProductAbs( pPhys->GetInertia(), motorAxis_ls );
			m_motor.m_maxTorque = inertia * m_motor.m_inertiaFactor * (m_angularAcceleration + m_additionalAcceleration);
			m_motor.m_restistanceDamping = 1.0f;
		}
	}

	if ( m_attachedObject )
	{
		IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();

		// create a hinge constraint for this object?
		if ( m_spawnflags & SF_MOTOR_HINGE )
		{
			// UNDONE: Don't do this on restore?
			if ( !m_pHinge )
			{
				constraint_hingeparams_t hingeParams;
				hingeParams.Defaults();
				hingeParams.worldAxisDirection = m_motor.m_axis;
				hingeParams.worldPosition = GetLocalOrigin();

				m_pHinge = physenv->CreateHingeConstraint( g_PhysWorldObject, pPhys, nullptr, hingeParams );
				m_pHinge->SetGameData( (void *)this );
				// can't grab this object
				PhysSetGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
			}

			if ( m_spawnflags & SF_MOTOR_NOCOLLIDE )
			{
				PhysDisableEntityCollisions( g_PhysWorldObject, pPhys );
			}
		}
		else
		{
			m_pHinge = nullptr;
		}

		// NOTE: On restore, this path isn't run because m_pController will not be NULL
		if ( !m_pController )
		{
			m_pController = physenv->CreateMotionController( &m_motor );
			m_pController->AttachObject( m_attachedObject->VPhysicsGetObject(), false );

			if ( m_spawnflags & SF_MOTOR_START_ON )
			{
				TurnOn();
			}
		}
	}
}

void CPhysMotor::OnRestore_Impl()
{
	BaseClass::OnRestore_Impl();
	// Need to do this on restore since there's no good way to save this
	if ( m_pController )
	{
		m_pController->SetEventHandler( &m_motor );
	}
}

void CPhysMotor::Think_Impl( void )
{
	// angular acceleration is always positive - it should be treated as a magnitude - the controller 
	// will apply it in the proper direction
	contract_assert( m_angularAcceleration>=0 );

	m_motor.m_speed = UTIL_Approach( m_flSpeed, m_motor.m_speed, m_angularAcceleration*(gpGlobals->curtime-m_lastTime) );
	m_lastTime = gpGlobals->curtime;
	if ( m_motor.m_speed != m_flSpeed )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//======================================================================================
// KEEPUPRIGHT CONTROLLER
//======================================================================================

class CKeepUpright : public CPointEntity, public IMotionEvent
{
	DECLARE_CLASS( CKeepUpright, CPointEntity );
public:
	DECLARE_DATADESC();

	CKeepUpright();
	~CKeepUpright();
	void Spawn_Impl();
	void Activate_Impl();

	// IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata )
	{
		m_bActive = true;
	}
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata )
	{
		m_bActive = false;
	}

	[[= ks::reflect::Input{ .name = "SetAngularLimit", .type = FIELD_FLOAT } ]] void InputSetAngularLimit( inputdata_t &inputdata )
	{
		m_angularLimit = inputdata.value.Float();
	}

private:	
	friend CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive );

	Vector						m_worldGoalAxis;
	Vector						m_localTestAxis;
	IPhysicsMotionController	*m_pController;
	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t					m_nameAttach;
	EHANDLE						m_attachedObject;
	[[= ks::reflect::Key{ .name = "angularlimit" } ]] float						m_angularLimit;
	bool						m_bActive;
	bool						m_bDampAllRotation;
};

static constexpr auto SF_KEEPUPRIGHT_START_INACTIVE = 0x0001;

LINK_ENTITY_TO_CLASS( phys_keepupright, CKeepUpright );

IMPLEMENT_REFLECT_DATAMAP( CKeepUpright )

CKeepUpright::CKeepUpright()
{
	// by default, recover from up to 15 degrees / sec angular velocity
	m_angularLimit = 15;
	m_attachedObject = nullptr;
	m_bDampAllRotation = false;
}

CKeepUpright::~CKeepUpright()
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = nullptr;
	}
}

void CKeepUpright::Spawn_Impl()
{
	// align the object's local Z axis
	m_localTestAxis.Init( 0, 0, 1 );
	// Use our Up axis so mapmakers can orient us arbitrarily
	GetVectors( nullptr, nullptr, &m_worldGoalAxis );

	SetMoveType( MOVETYPE_NONE );

	if ( m_spawnflags & SF_KEEPUPRIGHT_START_INACTIVE )
	{
		m_bActive = false;
	}
	else
	{
		m_bActive = true;
	}
}

void CKeepUpright::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	if ( !m_pController )
	{
		// This case occurs when spawning
		IPhysicsObject *pPhys;
		if ( m_attachedObject )
		{
			pPhys = m_attachedObject->VPhysicsGetObject();
		}
		else
		{
			pPhys = FindPhysicsObjectByName( STRING(m_nameAttach), this );
		}

		if ( !pPhys )
		{
			UTIL_Remove(this);
			return;
		}
		// HACKHACK: Due to changes in the vehicle simulator the keepupright controller used in coast_01 is unstable
		// force it to have perfect damping to compensate.
		// detect it using the hack of angular limit == 150, attached to a vehicle
		// Fixing it in the code is the simplest course of action presently

		m_pController = physenv->CreateMotionController( (IMotionEvent *)this );
		m_pController->AttachObject( pPhys, false );
	}
	else
	{
		// This case occurs when restoring
		m_pController->SetEventHandler( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use this to spawn a keepupright controller via code instead of map-placed
//-----------------------------------------------------------------------------
CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive )
{
	CKeepUpright *pKeepUpright = (CKeepUpright*)CBaseEntity::Create( "phys_keepupright", vecOrigin, vecAngles, pOwner );
	if ( pKeepUpright )
	{
		pKeepUpright->m_attachedObject = pOwner;
		pKeepUpright->m_angularLimit = flAngularLimit;
		if ( !bActive )
		{
			pKeepUpright->AddSpawnFlags( SF_KEEPUPRIGHT_START_INACTIVE );
		}
		pKeepUpright->Spawn();
		pKeepUpright->Activate();
	}

	return pKeepUpright;
}

IMotionEvent::simresult_e CKeepUpright::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	if ( !m_bActive )
		return SIM_NOTHING;

	linear.Init();

	AngularImpulse angVel;
	pObject->GetVelocity( nullptr, &angVel );

	matrix3x4_t matrix;
	// get the object's local to world transform
	pObject->GetPositionMatrix( &matrix );

	// Get the alignment axis in object space
	Vector currentLocalTargetAxis;
	VectorIRotate( m_worldGoalAxis, matrix, currentLocalTargetAxis );

	float invDeltaTime = (1/deltaTime);

	if ( m_bDampAllRotation )
	{
		angular = ComputeRotSpeedToAlignAxes( m_localTestAxis, currentLocalTargetAxis, angVel, 0, invDeltaTime, m_angularLimit );
		angular -= angVel;
		angular *= invDeltaTime;
		return SIM_LOCAL_ACCELERATION;
	}

	angular = ComputeRotSpeedToAlignAxes( m_localTestAxis, currentLocalTargetAxis, angVel, 1.0, invDeltaTime, m_angularLimit );
	angular *= invDeltaTime;

	return SIM_LOCAL_ACCELERATION;
}

// computes the torque necessary to align testAxis with alignAxis
AngularImpulse ComputeRotSpeedToAlignAxes( const Vector &testAxis, const Vector &alignAxis, const AngularImpulse &currentSpeed, float damping, float scale, float maxSpeed )
{
	Vector rotationAxis = CrossProduct( testAxis, alignAxis );

	// atan2() is well defined, so do a Dot & Cross instead of asin(Cross)
	float cosine = DotProduct( testAxis, alignAxis );
	float sine = VectorNormalize( rotationAxis );
	float angle = std::atan2( sine, cosine );

	angle = RAD2DEG(angle);
	AngularImpulse angular = rotationAxis * scale * angle;
	angular -= rotationAxis * damping * DotProduct( currentSpeed, rotationAxis );

	float len = VectorNormalize( angular );

	if ( len > maxSpeed )
	{
		len = maxSpeed;
	}

	return angular * len;
}

KS_ENTITY_ABSTRACT_KIND( CPhysForce )

class CBaseAnimating;
class CBaseEntity;
struct inputdata_t;
struct notify_system_event_params_t;

using namespace ::ks::reflect;

// Purpose: Physics constraint entities

#define HINGE_NOTIFY HL2_EPISODIC
#if HINGE_NOTIFY
#endif

static constexpr auto SF_CONSTRAINT_DISABLE_COLLISION = 0x0001;
static constexpr auto SF_SLIDE_LIMIT_ENDS = 0x0002;
static constexpr auto SF_PULLEY_RIGID = 0x0002;
static constexpr auto SF_LENGTH_RIGID = 0x0002;
static constexpr auto SF_RAGDOLL_FREEMOVEMENT = 0x0002;
static constexpr auto SF_CONSTRAINT_START_INACTIVE = 0x0004;
static constexpr auto SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY = 0x0008;
static constexpr auto SF_CONSTRAINT_NO_CONNECT_UNTIL_ACTIVATED = 0x0010; // Will only check the two attached entities at activation

ConVar    g_debug_constraint_sounds	  ( side_registry(), "g_debug_constraint_sounds", "0", FCVAR_CHEAT, "Enable debug printing about constraint sounds.");

struct hl_constraint_info_t
{
	hl_constraint_info_t() 
	{ 
		pObjects[0] = pObjects[1] = nullptr;
		pGroup = nullptr;
		anchorPosition[0].Init();
		anchorPosition[1].Init();
		swapped = false; 
		massScale[0] = massScale[1] = 1.0f;
	}
	Vector			anchorPosition[2];
	IPhysicsObject	*pObjects[2];
	IPhysicsConstraintGroup *pGroup;
	float			massScale[2];
	bool			swapped;
};

struct constraint_anchor_t
{
	Vector		localOrigin;
	EHANDLE		hEntity;
	int			parentAttachment;
	string_t	name;
	float		massScale;
};

class CAnchorList : public CAutoGameSystem
{
public:
	CAnchorList( char const *name ) : CAutoGameSystem( name )
	{
	}
	void LevelShutdownPostEntity() 
	{
		m_list.clear(); m_list.shrink_to_fit();
	}

	void AddToList( CBaseEntity *pEntity, float massScale )
	{
		int index = std::ssize( m_list ); m_list.emplace_back();
		constraint_anchor_t *pAnchor = &m_list[index];

		pAnchor->hEntity = pEntity->GetParent();
		pAnchor->parentAttachment = pEntity->GetParentAttachment();
		pAnchor->name = pEntity->GetEntityName();
		pAnchor->localOrigin = pEntity->GetLocalOrigin();
		pAnchor->massScale = massScale;
	}

	constraint_anchor_t *Find( string_t name )
	{
		for ( int i = std::ssize( m_list )-1; i >=0; i-- )
		{
			if ( FStrEq( STRING(m_list[i].name), STRING(name) ) )
			{
				return &m_list[i];
			}
		}
		return nullptr;
	}

private:
	std::vector< constraint_anchor_t >	m_list;
};

static CAnchorList g_AnchorList( "CAnchorList" );

class CConstraintAnchor : public CPointEntity
{
	DECLARE_CLASS( CConstraintAnchor, CPointEntity );
public:
	CConstraintAnchor()
	{
		m_massScale = 1.0f;
	}
	void Spawn_Impl( void )
	{
		if ( GetParent() )
		{
			g_AnchorList.AddToList( this, m_massScale );
			UTIL_Remove( this );
		}
	}
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "massScale" } ]] float m_massScale;
};

IMPLEMENT_REFLECT_DATAMAP( CConstraintAnchor )

LINK_ENTITY_TO_CLASS( info_constraint_anchor, CConstraintAnchor );

class CPhysConstraintSystem : public CLogicalEntity
{
	DECLARE_CLASS( CPhysConstraintSystem, CLogicalEntity );
public:

	void Spawn_Impl();
	~CPhysConstraintSystem();
	IPhysicsConstraintGroup *GetVPhysicsGroup() { return m_pMachine; }

	DECLARE_DATADESC();
private:
	IPhysicsConstraintGroup *m_pMachine;
	[[= ks::reflect::Key{ .name = "additionaliterations" } ]] int						m_additionalIterations;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysConstraintSystem )

void CPhysConstraintSystem::Spawn_Impl()
{
	constraint_groupparams_t group;
	group.Defaults();
	group.additionalIterations = m_additionalIterations;
	m_pMachine = physenv->CreateConstraintGroup( group );
}

CPhysConstraintSystem::~CPhysConstraintSystem()
{
	physenv->DestroyConstraintGroup( m_pMachine );
}

LINK_ENTITY_TO_CLASS( phys_constraintsystem, CPhysConstraintSystem );

void PhysTeleportConstrainedEntity( CBaseEntity *pTeleportSource, IPhysicsObject *pObject0, IPhysicsObject *pObject1, const Vector &prevPosition, const QAngle &prevAngles, bool physicsRotate )
{
	// teleport the other object
	CBaseEntity *pEntity0 = static_cast<CBaseEntity *> (pObject0->GetGameData());
	CBaseEntity *pEntity1 = static_cast<CBaseEntity *> (pObject1->GetGameData());
	if ( !pEntity0 || !pEntity1 )
		return;

	// figure out which entity needs to be fixed up (the one that isn't pTeleportSource)
	CBaseEntity *pFixup = pEntity1;
	// teleport the other object
	if ( pTeleportSource != pEntity0 )
	{
		if ( pTeleportSource != pEntity1 )
		{
			Msg("Bogus teleport notification!!\n");
			return;
		}
		pFixup = pEntity0;
	}

	// constraint doesn't move this entity
	if ( pFixup->GetMoveType() != MOVETYPE_VPHYSICS )
		return;

	if ( !pFixup->VPhysicsGetObject() || !pFixup->VPhysicsGetObject()->IsMoveable() )
		return;

	QAngle oldAngles = prevAngles;

	if ( !physicsRotate )
	{
		oldAngles = pTeleportSource->GetAbsAngles();
	}

	matrix3x4_t startCoord, startInv, endCoord, xform;
	AngleMatrix( oldAngles, prevPosition, startCoord );
	MatrixInvert( startCoord, startInv );
	ConcatTransforms( pTeleportSource->EntityToWorldTransform(), startInv, xform );
	QAngle fixupAngles;
	Vector fixupPos;

	ConcatTransforms( xform, pFixup->EntityToWorldTransform(), endCoord );
	MatrixAngles( endCoord, fixupAngles, fixupPos );
	pFixup->Teleport( &fixupPos, &fixupAngles, nullptr );
}

static void DrawPhysicsBounds( IPhysicsObject *pObject, int r, int g, int b, int a )
{
	const CPhysCollide *pCollide = pObject->GetCollide();
	Vector pos;
	QAngle angles;
	pObject->GetPosition( &pos, &angles );
	Vector mins, maxs;
	physcollision->CollideGetAABB( &mins, &maxs, pCollide, vec3_origin, vec3_angle );
	// don't fight the z-buffer
	mins -= Vector(1,1,1);
	maxs += Vector(1,1,1);
	NDebugOverlay::BoxAngles( pos, mins, maxs, angles, r, g, b, a, 0 );
}

static void DrawConstraintObjectsAxes(CBaseEntity *pConstraintEntity, IPhysicsConstraint *pConstraint)
{
	if ( !pConstraint || !pConstraintEntity )
		return;
	matrix3x4_t xformRef, xformAtt;
	bool bXform = pConstraint->GetConstraintTransform( &xformRef, &xformAtt );
	IPhysicsObject *pRef = pConstraint->GetReferenceObject();

	if ( pRef && !pRef->IsStatic() )
	{
		if ( bXform )
		{
			Vector pos, posWorld;
			QAngle angles;
			MatrixAngles( xformRef, angles, pos );
			pRef->LocalToWorld( &posWorld, pos );
			NDebugOverlay::Axis( posWorld, vec3_angle, 12, false, 0 );
		}
		DrawPhysicsBounds( pRef, 0, 255, 0, 12 );
	}
	IPhysicsObject *pAttach = pConstraint->GetAttachedObject();
	if ( pAttach && !pAttach->IsStatic() )
	{
		if ( bXform )
		{
			Vector pos, posWorld;
			QAngle angles;
			MatrixAngles( xformAtt, angles, pos );
			pAttach->LocalToWorld( &posWorld, pos );
			NDebugOverlay::Axis( posWorld, vec3_angle, 12, false, 0 );
		}
		DrawPhysicsBounds( pAttach, 255, 0, 0, 12 );
	}
}

class CPhysConstraint : public CLogicalEntity
{
	DECLARE_CLASS( CPhysConstraint, CLogicalEntity );
public:

	CPhysConstraint();
	~CPhysConstraint();

	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void Activate_Impl( void );

	void ClearStaticFlag( IPhysicsObject *pObj )
	{
		if ( !pObj )
			return;
		PhysClearGameFlags( pObj, FVPHYSICS_CONSTRAINT_STATIC );
	}

	virtual void Deactivate()
	{
		if ( !m_pConstraint )
			return;
		m_pConstraint->Deactivate();
		ClearStaticFlag( m_pConstraint->GetReferenceObject() );
		ClearStaticFlag( m_pConstraint->GetAttachedObject() );
		if ( m_spawnflags & SF_CONSTRAINT_DISABLE_COLLISION )
		{
			// constraint may be getting deactivated because an object got deleted, so check them here.
			IPhysicsObject *pRef = m_pConstraint->GetReferenceObject();
			IPhysicsObject *pAtt = m_pConstraint->GetAttachedObject();
			if ( pRef && pAtt )
			{
				PhysEnableEntityCollisions( pRef, pAtt );
			}
		}
	}

	void OnBreak( void )
	{
		Deactivate();
		if ( m_breakSound != NULL_STRING )
		{
			CBroadcastRecipientFilter filter;

			Vector origin = GetAbsOrigin();
			Vector refPos = origin, attachPos = origin;

			IPhysicsObject *pRef = m_pConstraint->GetReferenceObject();
			if ( pRef && (pRef != g_PhysWorldObject) )
			{
				pRef->GetPosition( &refPos, nullptr );
				attachPos = refPos;
			}
			IPhysicsObject *pAttach = m_pConstraint->GetAttachedObject();
			if ( pAttach && (pAttach != g_PhysWorldObject) )
			{
				pAttach->GetPosition( &attachPos, nullptr );
				if ( !pRef || (pRef == g_PhysWorldObject) )
				{
					refPos = attachPos;
				}
			}
			
			VectorAdd( refPos, attachPos, origin );
			origin *= 0.5f;

			EmitSound_t ep;
			ep.m_nChannel = CHAN_STATIC;
			ep.m_pSoundName = STRING(m_breakSound);
			ep.m_flVolume = VOL_NORM;
			ep.m_SoundLevel = ATTN_TO_SNDLVL( ATTN_STATIC );
			ep.m_pOrigin = &origin;

			EmitSound( filter, entindex(), ep );
		}
		m_OnBreak.FireOutput( this, this );
		// queue this up to be deleted at the end of physics 
		// The Deactivate() call should make sure we don't get more of these callbacks.
		PhysCallbackRemove( this->NetworkProp() );
	}

	[[= ks::reflect::Input{ .name = "Break", .type = FIELD_VOID } ]]
	void InputBreak( inputdata_t &inputdata )
	{
		if ( m_pConstraint ) 
			m_pConstraint->Deactivate();
		
		OnBreak();
	}

	[[= ks::reflect::Input{ .name = "ConstraintBroken", .type = FIELD_VOID } ]]
	void InputOnBreak( inputdata_t &inputdata )
	{
		OnBreak();
	}

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]]
	void InputTurnOn( inputdata_t &inputdata )
	{
		if ( HasSpawnFlags( SF_CONSTRAINT_NO_CONNECT_UNTIL_ACTIVATED ) )
		{
			ActivateConstraint();
		}

		if ( !m_pConstraint || !m_pConstraint->GetReferenceObject() || !m_pConstraint->GetAttachedObject() )
			return;

		m_pConstraint->Activate();
		m_pConstraint->GetReferenceObject()->Wake();
		m_pConstraint->GetAttachedObject()->Wake();
	}

	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]]
	void InputTurnOff( inputdata_t &inputdata )
	{
		Deactivate();
	}

	int DrawDebugTextOverlays_Impl()
	{
		int pos = BaseClass::DrawDebugTextOverlays_Impl();
		if ( m_pConstraint && (m_debugOverlays & OVERLAY_TEXT_BIT) )
		{
			constraint_breakableparams_t params;
			V_memset(&params,0,sizeof(params));
			m_pConstraint->GetConstraintParams( &params );
			
			if ( (params.bodyMassScale[0] != 1.0f && params.bodyMassScale[0] != 0.0f) || (params.bodyMassScale[1] != 1.0f && params.bodyMassScale[1] != 0.0f) )
			{
				CFmtStr str("mass ratio %.4f:%.4f\n", params.bodyMassScale[0], params.bodyMassScale[1] );
				NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), pos, str.Access(), 0, 255, 255, 0, 255 );
			}
			pos++;
		}
		return pos;
	}

	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			DrawConstraintObjectsAxes(this, m_pConstraint);
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	void GetBreakParams( constraint_breakableparams_t &params, const hl_constraint_info_t &info )
	{
		params.Defaults();
		params.forceLimit = lbs2kg(m_forceLimit);
		params.torqueLimit = lbs2kg(m_torqueLimit);
		params.isActive = HasSpawnFlags( SF_CONSTRAINT_START_INACTIVE ) ? false : true;
		params.bodyMassScale[0] = info.massScale[0];
		params.bodyMassScale[1] = info.massScale[1];
	}

	// the notify system calls this on the constrained entities - used to detect & follow teleports
	void NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );
	
	// gets called at setup time on first init and restore
	virtual void OnConstraintSetup( hl_constraint_info_t &info ); 

	// return the internal constraint object (used by sound gadgets)
	inline IPhysicsConstraint *GetPhysConstraint() { return m_pConstraint; }

protected:	
	void GetConstraintObjects( hl_constraint_info_t &info );
	void SetupTeleportationHandling( hl_constraint_info_t &info );
	bool ActivateConstraint( void );
	virtual IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info ) = 0;

	IPhysicsConstraint	*m_pConstraint;

	// These are "template" values used to construct the hinge
	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t		m_nameAttach1;
	[[= ks::reflect::Key{ .name = "attach2" } ]] string_t		m_nameAttach2;
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "breaksound" } ]] string_t		m_breakSound;
	[[= ks::reflect::Key{ .name = "constraintsystem" } ]] string_t		m_nameSystem;
	[[= ks::reflect::Key{ .name = "forcelimit" } ]] float			m_forceLimit;
	[[= ks::reflect::Key{ .name = "torquelimit" } ]] float			m_torqueLimit;
	unsigned int	m_teleportTick;
	[[= ks::reflect::Key{ .name = "teleportfollowdistance" } ]] float			m_minTeleportDistance;

	[[= ks::reflect::Key{ .name = "OnBreak" } ]] COutputEvent	m_OnBreak;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysConstraint )

CPhysConstraint::CPhysConstraint( void )
{
	m_pConstraint = nullptr;
	m_nameAttach1 = NULL_STRING;
	m_nameAttach2 = NULL_STRING;
	m_forceLimit = 0;
	m_torqueLimit = 0;
	m_teleportTick = 0xFFFFFFFF;
	m_minTeleportDistance = 0.0f;
}

CPhysConstraint::~CPhysConstraint()
{
	Deactivate();
	physenv->DestroyConstraint( m_pConstraint );
}

void CPhysConstraint::Precache_Impl( void )
{
	if ( m_breakSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_breakSound) );
	}
}

void CPhysConstraint::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	Precache();
}

// debug function - slow, uses dynamic_cast<> - use this to query the attached objects
// physics_debug_entity toggles the constraint system for an object using this
bool GetConstraintAttachments( CBaseEntity *pEntity, CBaseEntity *pAttachOut[2], IPhysicsObject *pAttachVPhysics[2] )
{
	CPhysConstraint *pConstraintEntity = dynamic_cast<CPhysConstraint *>(pEntity);
	if ( pConstraintEntity )
	{
		IPhysicsConstraint *pConstraint = pConstraintEntity->GetPhysConstraint();
		if ( pConstraint )
		{
			IPhysicsObject *pRef = pConstraint->GetReferenceObject();
			pAttachVPhysics[0] = pRef;
			pAttachOut[0] = pRef ? static_cast<CBaseEntity *>(pRef->GetGameData()) : nullptr;
			IPhysicsObject *pAttach = pConstraint->GetAttachedObject();
			pAttachVPhysics[1] = pAttach;
			pAttachOut[1] = pAttach ? static_cast<CBaseEntity *>(pAttach->GetGameData()) : nullptr;
			return true;
		}
	}
	return false;
}

void DebugConstraint(CBaseEntity *pEntity)
{
	CPhysConstraint *pConstraintEntity = dynamic_cast<CPhysConstraint *>(pEntity);
	if ( pConstraintEntity )
	{
		IPhysicsConstraint *pConstraint = pConstraintEntity->GetPhysConstraint();
		if ( pConstraint )
		{
			pConstraint->OutputDebugInfo();
		}
	}
}

void FindPhysicsAnchor( string_t name, hl_constraint_info_t &info, int index, CBaseEntity *pErrorEntity )
{
	constraint_anchor_t *pAnchor = g_AnchorList.Find( name );
	if ( pAnchor )
	{
		CBaseEntity *pEntity = pAnchor->hEntity;
		if ( pEntity )
		{
			info.massScale[index] = pAnchor->massScale;
			bool bWroteAttachment = false;
			if ( pAnchor->parentAttachment > 0 )
			{
				CBaseAnimating *pAnim = pAnchor->hEntity->GetBaseAnimating();
				if ( pAnim )
				{
					IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
					int listCount = pAnchor->hEntity->VPhysicsGetObjectList( list, std::size( list ) );
					int iPhysicsBone = pAnim->GetPhysicsBone( pAnim->GetAttachmentBone( pAnchor->parentAttachment ) );
					if ( iPhysicsBone < listCount )
					{
						Vector pos;
						info.pObjects[index] = list[iPhysicsBone];
						pAnim->GetAttachment( pAnchor->parentAttachment, pos );
						list[iPhysicsBone]->WorldToLocal( &info.anchorPosition[index], pos );
						bWroteAttachment = true;
					}
				}
			}
			if ( !bWroteAttachment )
			{
				info.anchorPosition[index] = pAnchor->localOrigin;
				info.pObjects[index] = pAnchor->hEntity->VPhysicsGetObject();
			}
		}
		else
		{
			pAnchor = nullptr;
		}
	}
	if ( !pAnchor )
	{
		info.anchorPosition[index] = vec3_origin;
		info.pObjects[index] = FindPhysicsObjectByName( STRING(name), pErrorEntity );
		info.massScale[index] = 1.0f;
	}
}

void CPhysConstraint::OnConstraintSetup( hl_constraint_info_t &info )
{
	if ( info.pObjects[0] && info.pObjects[1] )
	{
		SetupTeleportationHandling( info );
	}
	if ( m_spawnflags & SF_CONSTRAINT_DISABLE_COLLISION )
	{
		PhysDisableEntityCollisions( info.pObjects[0], info.pObjects[1] );
	}
}

void CPhysConstraint::SetupTeleportationHandling( hl_constraint_info_t &info )
{
	CBaseEntity *pEntity0 = (CBaseEntity *)info.pObjects[0]->GetGameData();
	if ( pEntity0 )
	{
		g_pNotify->AddEntity( this, pEntity0 );
	}

	CBaseEntity *pEntity1 = (CBaseEntity *)info.pObjects[1]->GetGameData();
	if ( pEntity1 )
	{
		g_pNotify->AddEntity( this, pEntity1 );
	}
}

static IPhysicsConstraintGroup *GetRagdollConstraintGroup( IPhysicsObject *pObj )
{
	if ( pObj )
	{
		CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObj->GetGameData());
		ragdoll_t *pRagdoll = Ragdoll_GetRagdoll(pEntity);
		if ( pRagdoll )
			return pRagdoll->pGroup;
	}
	return nullptr;
}

void CPhysConstraint::GetConstraintObjects( hl_constraint_info_t &info )
{
	FindPhysicsAnchor( m_nameAttach1, info, 0, this );
	FindPhysicsAnchor( m_nameAttach2, info, 1, this );

	// Missing one object, assume the world instead
	if ( info.pObjects[0] == nullptr && info.pObjects[1] )
	{
		if ( V_strlen(STRING(m_nameAttach1)) )
		{
			Warning("Bogus constraint %s (attaches ENTITY NOT FOUND:%s to %s)\n", GetDebugName(), STRING(m_nameAttach1), STRING(m_nameAttach2));
		}
		info.pObjects[0] = g_PhysWorldObject;
		info.massScale[0] = info.massScale[1] = 1.0f; // no mass scale on world constraint
	}
	else if ( info.pObjects[0] && !info.pObjects[1] )
	{
		if ( V_strlen(STRING(m_nameAttach2)) )
		{
			Warning("Bogus constraint %s (attaches %s to ENTITY NOT FOUND:%s)\n", GetDebugName(), STRING(m_nameAttach1), STRING(m_nameAttach2));
		}
		info.pObjects[1] = info.pObjects[0];
		info.pObjects[0] = g_PhysWorldObject;		// Try to make the world object consistently object0 for ease of implementation
		info.massScale[0] = info.massScale[1] = 1.0f; // no mass scale on world constraint
		info.swapped = true;
	}

	info.pGroup = GetRagdollConstraintGroup(info.pObjects[0]);
	if ( !info.pGroup )
	{
		info.pGroup = GetRagdollConstraintGroup(info.pObjects[1]);
	}
}

void CPhysConstraint::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	if ( HasSpawnFlags( SF_CONSTRAINT_NO_CONNECT_UNTIL_ACTIVATED ) == false )
	{
		if ( !ActivateConstraint() )
		{
			UTIL_Remove(this);
		}
	}
}

IPhysicsConstraintGroup *GetConstraintGroup( string_t systemName )
{
	CBaseEntity *pMachine = gEntList.FindEntityByName( nullptr, systemName );

	if ( pMachine )
	{
		CPhysConstraintSystem *pGroup = dynamic_cast<CPhysConstraintSystem *>(pMachine);
		if ( pGroup )
		{
			return pGroup->GetVPhysicsGroup();
		}
	}
	return nullptr;
}

bool CPhysConstraint::ActivateConstraint( void )
{
	// A constraint attaches two objects to each other.
	// The constraint is specified in the coordinate frame of the "reference" object
	// and constrains the "attached" object
	hl_constraint_info_t info;
	if ( m_pConstraint )
	{
		// already have a constraint, don't make a new one
		info.pObjects[0] = m_pConstraint->GetReferenceObject();
		info.pObjects[1] = m_pConstraint->GetAttachedObject();
		OnConstraintSetup(info);
		return true;
	}

	GetConstraintObjects( info );
	if ( !info.pObjects[0] && !info.pObjects[1] )
		return false;

	if ( info.pObjects[0]->IsStatic() && info.pObjects[1]->IsStatic() )
	{
		Warning("Constraint (%s) attached to two static objects (%s and %s)!!!\n", STRING(GetEntityName()), STRING(m_nameAttach1), m_nameAttach2 == NULL_STRING ? "world" : STRING(m_nameAttach2) );
		return false;
	}

	if ( info.pObjects[0]->GetShadowController() && info.pObjects[1]->GetShadowController() )
	{
		Warning("Constraint (%s) attached to two shadow objects (%s and %s)!!!\n", STRING(GetEntityName()), STRING(m_nameAttach1), m_nameAttach2 == NULL_STRING ? "world" : STRING(m_nameAttach2) );
		return false;
	}
	IPhysicsConstraintGroup *pGroup = GetConstraintGroup( m_nameSystem );
	if ( !pGroup )
	{
		pGroup = info.pGroup;
	}
	m_pConstraint = CreateConstraint( pGroup, info );
	if ( !m_pConstraint )
		return false;

	m_pConstraint->SetGameData( (void *)this );

	if ( pGroup )
	{
		pGroup->Activate();
	}

	OnConstraintSetup(info);

	return true;
}

void CPhysConstraint::NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	// don't recurse
	if ( eventType != NOTIFY_EVENT_TELEPORT || (unsigned int)gpGlobals->tickcount == m_teleportTick )
		return;

	float distance = (params.pTeleport->prevOrigin - pNotify->GetAbsOrigin()).Length();
	
	// no need to follow a small teleport
	if ( distance <= m_minTeleportDistance )
		return;

	m_teleportTick = gpGlobals->tickcount;

	PhysTeleportConstrainedEntity( pNotify, m_pConstraint->GetReferenceObject(), m_pConstraint->GetAttachedObject(), params.pTeleport->prevOrigin, params.pTeleport->prevAngles, params.pTeleport->physicsRotate );
}

class CPhysHinge : public CPhysConstraint, public IVPhysicsWatcher
{
	[[= ks::reflect::KeyFrom<"m_hinge.worldAxisDirection", ks::reflect::Key{ .name = "hingeaxis" } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPhysHinge, CPhysConstraint );

public:
	void Spawn_Impl( void );
	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
	{
		if ( m_hinge.worldAxisDirection == vec3_origin )
		{
			DevMsg("ERROR: Hinge with bad data!!!\n" );
			return nullptr;
		}
		GetBreakParams( m_hinge.constraint, info );
		m_hinge.constraint.strength = 1.0;
		// BUGBUG: These numbers are very hard to edit
		// Scale by 1000 to make things easier
		// CONSIDER: Unify the units of torque around something other 
		// than HL units (kg * in^2 / s ^2)
		m_hinge.hingeAxis.SetAxisFriction( 0, 0, m_hingeFriction * 1000 );

		int hingeAxis = 0;
		if ( IsWorldHinge( info, &hingeAxis ) )
		{
			info.pObjects[1]->BecomeHinged( hingeAxis );
		}
		else
		{
			RemoveSpawnFlags( SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY );
		}

		return physenv->CreateHingeConstraint( info.pObjects[0], info.pObjects[1], pGroup, m_hinge );
	}

	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			NDebugOverlay::Line(m_hinge.worldPosition, m_hinge.worldPosition + 48 * m_hinge.worldAxisDirection, 0, 255, 0, false, 0 );
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	[[= ks::reflect::Input{ .name = "SetAngularVelocity", .type = FIELD_FLOAT } ]]
	void InputSetVelocity( inputdata_t &inputdata )
	{
		if ( !m_pConstraint || !m_pConstraint->GetReferenceObject() || !m_pConstraint->GetAttachedObject() )
			return;
	
		float speed = inputdata.value.Float();
		float massLoad = 1;
		int numMasses = 0;
		if ( m_pConstraint->GetReferenceObject()->IsMoveable() )
		{
			massLoad = m_pConstraint->GetReferenceObject()->GetInertia().Length();
			numMasses++;
			m_pConstraint->GetReferenceObject()->Wake();
		}
		if ( m_pConstraint->GetAttachedObject()->IsMoveable() )
		{
			massLoad += m_pConstraint->GetAttachedObject()->GetInertia().Length();
			numMasses++;
			m_pConstraint->GetAttachedObject()->Wake();
		}
		if ( numMasses > 0 )
		{
			massLoad /= (float)numMasses;
		}
		
		float loadscale = m_systemLoadScale != 0 ? m_systemLoadScale : 1;
		m_pConstraint->SetAngularMotor( speed, speed * loadscale * massLoad * loadscale * (1.0/TICK_INTERVAL) );
	}

	[[= ks::reflect::Input{ .name = "SetHingeFriction", .type = FIELD_FLOAT } ]]
	void InputSetHingeFriction( inputdata_t &inputdata )
	{
		m_hingeFriction = inputdata.value.Float();
		Msg("Setting hinge friction to %f\n", m_hingeFriction );
		m_hinge.hingeAxis.SetAxisFriction( 0, 0, m_hingeFriction * 1000 );
	}

	virtual void Deactivate()
	{
		if ( HasSpawnFlags( SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY ) )
		{
			if ( m_pConstraint && m_pConstraint->GetAttachedObject() )
			{
				// NOTE: RemoveHinged() is always safe
				m_pConstraint->GetAttachedObject()->RemoveHinged();
			}
		}

		BaseClass::Deactivate();
	}
	
	void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
	{
#if HINGE_NOTIFY
		contract_assert( m_pConstraint );
		if (!m_pConstraint) 
			return;

		// if something woke up, start thinking. If everything is asleep, stop thinking.
		if ( bAwake )
		{
			// Did something wake up when I was not thinking?
			if ( GetNextThink() == TICK_NEVER_THINK )
			{
				m_soundInfo.StartThinking(this, 
					VelocitySampler::GetRelativeAngularVelocity(m_pConstraint->GetAttachedObject(), m_pConstraint->GetReferenceObject()) ,
					m_hinge.worldAxisDirection
					);

				SetThink(&CPhysHinge::SoundThink);
				SetNextThink(gpGlobals->curtime + m_soundInfo.getThinkRate());
			}
		}
		else
		{
			// Is everything asleep? If so, stop thinking.
			if ( GetNextThink() != TICK_NEVER_THINK				&&
				m_pConstraint->GetAttachedObject()->IsAsleep() &&
				m_pConstraint->GetReferenceObject()->IsAsleep() )
			{
				m_soundInfo.StopThinking(this);
				SetNextThink(TICK_NEVER_THINK);
			}
		}
#endif
	}

#if HINGE_NOTIFY
	virtual void OnConstraintSetup( hl_constraint_info_t &info )
	{
		CBaseEntity *pEntity0 = info.pObjects[0] ? static_cast<CBaseEntity *>(info.pObjects[0]->GetGameData()) : nullptr;
		if ( pEntity0 && !info.pObjects[0]->IsStatic()  )
		{
			WatchVPhysicsStateChanges( this, pEntity0 );
		}
		CBaseEntity *pEntity1 = info.pObjects[1] ? static_cast<CBaseEntity *>(info.pObjects[1]->GetGameData()) : nullptr;
		if ( pEntity1 && !info.pObjects[1]->IsStatic()  )
		{
			WatchVPhysicsStateChanges( this, pEntity1 );
		}
		BaseClass::OnConstraintSetup(info);
	}

	void SoundThink( void );
	// void Spawn( void );
	void Activate( void );
	void Precache( void );
#endif

	DECLARE_DATADESC();

#if HINGE_NOTIFY
protected:
	ConstraintSoundInfo m_soundInfo;
#endif

private:
	constraint_hingeparams_t m_hinge;
	[[= ks::reflect::Key{ .name = "hingefriction" } ]] float m_hingeFriction;
	[[= ks::reflect::Key{ .name = "systemloadscale" } ]] float	m_systemLoadScale;
	bool IsWorldHinge( const hl_constraint_info_t &info, int *pAxisOut );
};

IMPLEMENT_REFLECT_DATAMAP( CPhysHinge )

LINK_ENTITY_TO_CLASS( phys_hinge, CPhysHinge );

void CPhysHinge::Spawn_Impl( void )
{
	m_hinge.worldPosition = GetLocalOrigin();
	m_hinge.worldAxisDirection -= GetLocalOrigin();
	VectorNormalize(m_hinge.worldAxisDirection);
	UTIL_SnapDirectionToAxis( m_hinge.worldAxisDirection );

	m_hinge.hingeAxis.SetAxisFriction( 0, 0, 0 );

	if ( HasSpawnFlags( SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY ) )
	{
		masscenteroverride_t params;
		if ( m_nameAttach1 == NULL_STRING )
		{
			params.SnapToAxis( m_nameAttach2, m_hinge.worldPosition, m_hinge.worldAxisDirection );
			PhysSetMassCenterOverride( params );
		}
		else if ( m_nameAttach2 == NULL_STRING )
		{
			params.SnapToAxis( m_nameAttach1, m_hinge.worldPosition, m_hinge.worldAxisDirection );
			PhysSetMassCenterOverride( params );
		}
		else
		{
			RemoveSpawnFlags( SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY );
		}
	}

	Precache();
}

#if HINGE_NOTIFY
void CPhysHinge::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	m_soundInfo.OnActivate(this);
	if (m_pConstraint)
	{
		m_soundInfo.StartThinking(this, 
			VelocitySampler::GetRelativeAngularVelocity(m_pConstraint->GetAttachedObject(), m_pConstraint->GetReferenceObject()) ,
			m_hinge.worldAxisDirection
			);

		SetThink(&CPhysHinge::SoundThink);
		SetNextThink( gpGlobals->curtime + m_soundInfo.getThinkRate() );
	}
}

void CPhysHinge::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
	return m_soundInfo.OnPrecache(this);
}

#endif

static int GetUnitAxisIndex( const Vector &axis )
{
	bool valid = false;
	int index = -1;

	for ( int i = 0; i < 3; i++ )
	{
		if ( axis[i] != 0 )
		{
			if ( fabs(axis[i]) == 1 )
			{
				if ( index  < 0 )
				{
					index = i;
					valid = true;
					continue;
				}
			}
			valid = false;
		}
	}
	return valid ? index : -1;
}

bool CPhysHinge::IsWorldHinge( const hl_constraint_info_t &info, int *pAxisOut )
{
	if ( HasSpawnFlags( SF_CONSTRAINT_ASSUME_WORLD_GEOMETRY ) && info.pObjects[0] == g_PhysWorldObject )
	{
		Vector localHinge;
		info.pObjects[1]->WorldToLocalVector( &localHinge, m_hinge.worldAxisDirection );
		UTIL_SnapDirectionToAxis( localHinge );
		int hingeAxis = GetUnitAxisIndex( localHinge );
		if ( hingeAxis >= 0 )
		{
			*pAxisOut = hingeAxis;
			return true;
		}
	}
	return false;
}

#if HINGE_NOTIFY
void CPhysHinge::SoundThink( void )
{
	contract_assert( m_pConstraint );
	if (!m_pConstraint)
		return;

	IPhysicsObject * pAttached = m_pConstraint->GetAttachedObject(), *pReference = m_pConstraint->GetReferenceObject();
	contract_assert( pAttached && pReference );
	if (pAttached && pReference)
	{
		Vector relativeVel = VelocitySampler::GetRelativeAngularVelocity(pAttached,pReference);
		if (g_debug_constraint_sounds.GetBool())
		{
			NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (relativeVel), 255, 255, 0, true, 0.1f );
		}
		m_soundInfo.OnThink( this, relativeVel );

		SetNextThink(gpGlobals->curtime + m_soundInfo.getThinkRate());
	}
}
#endif

class CPhysBallSocket : public CPhysConstraint
{
public:
	DECLARE_CLASS( CPhysBallSocket, CPhysConstraint );

	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
	{
		constraint_ballsocketparams_t ballsocket;
	
		ballsocket.Defaults();
		
		for ( int i = 0; i < 2; i++ )
		{
			info.pObjects[i]->WorldToLocal( &ballsocket.constraintPosition[i], GetAbsOrigin() );
		}
		GetBreakParams( ballsocket.constraint, info );
		ballsocket.constraint.torqueLimit = 0;

		return physenv->CreateBallsocketConstraint( info.pObjects[0], info.pObjects[1], pGroup, ballsocket );
	}
};

LINK_ENTITY_TO_CLASS( phys_ballsocket, CPhysBallSocket );

class CPhysSlideConstraint : public CPhysConstraint, public IVPhysicsWatcher
{
public:
	DECLARE_CLASS( CPhysSlideConstraint, CPhysConstraint );

	DECLARE_DATADESC();
	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info );
	[[= ks::reflect::Input{ .name = "SetVelocity", .type = FIELD_FLOAT } ]]
	void InputSetVelocity( inputdata_t &inputdata )
	{
		if ( !m_pConstraint || !m_pConstraint->GetReferenceObject() || !m_pConstraint->GetAttachedObject() )
			return;

		float speed = inputdata.value.Float();
		float massLoad = 1;
		int numMasses = 0;
		if ( m_pConstraint->GetReferenceObject()->IsMoveable() )
		{
			massLoad = m_pConstraint->GetReferenceObject()->GetMass();
			numMasses++;
			m_pConstraint->GetReferenceObject()->Wake();
		}
		if ( m_pConstraint->GetAttachedObject()->IsMoveable() )
		{
			massLoad += m_pConstraint->GetAttachedObject()->GetMass();
			numMasses++;
			m_pConstraint->GetAttachedObject()->Wake();
		}
		if ( numMasses > 0 )
		{
			massLoad /= (float)numMasses;
		}
		float loadscale = m_systemLoadScale != 0 ? m_systemLoadScale : 1;
		m_pConstraint->SetLinearMotor( speed, speed * loadscale * massLoad * (1.0/TICK_INTERVAL) );
	}

	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			NDebugOverlay::Box( GetAbsOrigin(), -Vector(8,8,8), Vector(8,8,8), 0, 255, 0, 0, 0 );
			NDebugOverlay::Box( m_axisEnd, -Vector(4,4,4), Vector(4,4,4), 0, 0, 255, 0, 0 );
			NDebugOverlay::Line( GetAbsOrigin(), m_axisEnd, 255, 255, 0, false, 0 );
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
	{
#if HINGE_NOTIFY
		contract_assert( m_pConstraint );
		if (!m_pConstraint) 
			return;

		// if something woke up, start thinking. If everything is asleep, stop thinking.
		if ( bAwake )
		{
			// Did something wake up when I was not thinking?
			if ( GetNextThink() == TICK_NEVER_THINK )
			{
				Vector axisDirection = m_axisEnd - GetAbsOrigin();
				VectorNormalize( axisDirection );
				UTIL_SnapDirectionToAxis( axisDirection );

				m_soundInfo.StartThinking(this, 
					VelocitySampler::GetRelativeVelocity(m_pConstraint->GetAttachedObject(), m_pConstraint->GetReferenceObject()),
					axisDirection
					);
				SetThink(&CPhysSlideConstraint::SoundThink);
				SetNextThink(gpGlobals->curtime + m_soundInfo.getThinkRate());
			}
		}
		else
		{
			// Is everything asleep? If so, stop thinking.
			if ( GetNextThink() != TICK_NEVER_THINK				&&
				 m_pConstraint->GetAttachedObject()->IsAsleep() &&
				 m_pConstraint->GetReferenceObject()->IsAsleep() )
			{
				m_soundInfo.StopThinking(this);
				SetNextThink(TICK_NEVER_THINK);
			}
		}
#endif
	}

#if HINGE_NOTIFY
	virtual void OnConstraintSetup( hl_constraint_info_t &info )
	{
		CBaseEntity *pEntity0 = info.pObjects[0] ? static_cast<CBaseEntity *>(info.pObjects[0]->GetGameData()) : nullptr;
		if ( pEntity0 && !info.pObjects[0]->IsStatic()  )
		{
			WatchVPhysicsStateChanges( this, pEntity0 );
		}
		CBaseEntity *pEntity1 = info.pObjects[1] ? static_cast<CBaseEntity *>(info.pObjects[1]->GetGameData()) : nullptr;
		if ( pEntity1 && !info.pObjects[1]->IsStatic()  )
		{
			WatchVPhysicsStateChanges( this, pEntity1 );
		}
		BaseClass::OnConstraintSetup(info);
	}

	void SoundThink( void );
	// void Spawn( void );
	void Activate( void );
	void Precache( void );
#endif

	[[= ks::reflect::As{ FIELD_POSITION_VECTOR } ]] [[= ks::reflect::Key{ .name = "slideaxis" } ]] Vector	m_axisEnd;
	[[= ks::reflect::Key{ .name = "slidefriction" } ]] float	m_slideFriction;
	[[= ks::reflect::Key{ .name = "systemloadscale" } ]] float	m_systemLoadScale;

#if HINGE_NOTIFY
protected:
	ConstraintSoundInfo m_soundInfo;
#endif
};

LINK_ENTITY_TO_CLASS( phys_slideconstraint, CPhysSlideConstraint );

IMPLEMENT_REFLECT_DATAMAP( CPhysSlideConstraint )

IPhysicsConstraint *CPhysSlideConstraint::CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
{
	constraint_slidingparams_t sliding;
	sliding.Defaults();
	GetBreakParams( sliding.constraint, info );
	sliding.constraint.strength = 1.0;

	Vector axisDirection = m_axisEnd - GetAbsOrigin();
	VectorNormalize( axisDirection );
	UTIL_SnapDirectionToAxis( axisDirection );

	sliding.InitWithCurrentObjectState( info.pObjects[0], info.pObjects[1], axisDirection );
	sliding.friction = m_slideFriction;
	if ( m_spawnflags & SF_SLIDE_LIMIT_ENDS )
	{
		Vector position;
		info.pObjects[1]->GetPosition( &position, nullptr );

		sliding.limitMin = DotProduct( axisDirection, GetAbsOrigin() );
		sliding.limitMax = DotProduct( axisDirection, m_axisEnd );
		if ( sliding.limitMax < sliding.limitMin )
		{
			V_swap( sliding.limitMin, sliding.limitMax );
		}

		// expand limits to make initial position of the attached object valid
		float limit = DotProduct( position, axisDirection );
		if ( limit < sliding.limitMin )
		{
			sliding.limitMin = limit;
		}
		else if ( limit > sliding.limitMax )
		{
			sliding.limitMax = limit;
		}
		// offset so that the current position is 0
		sliding.limitMin -= limit;
		sliding.limitMax -= limit;
	}

	return physenv->CreateSlidingConstraint( info.pObjects[0], info.pObjects[1], pGroup, sliding );
}

#if HINGE_NOTIFY
void CPhysSlideConstraint::SoundThink( void )
{
	contract_assert( m_pConstraint );
	if (!m_pConstraint)
		return;

	IPhysicsObject * pAttached = m_pConstraint->GetAttachedObject(), *pReference = m_pConstraint->GetReferenceObject();
	contract_assert( pAttached && pReference );
	if (pAttached && pReference)
	{
		Vector relativeVel = VelocitySampler::GetRelativeVelocity(pAttached,pReference);
		// project velocity onto my primary axis.:

		Vector axisDirection = m_axisEnd - GetAbsOrigin();
		relativeVel = m_axisEnd * relativeVel.Dot(m_axisEnd)/m_axisEnd.Dot(m_axisEnd);

		m_soundInfo.OnThink( this, relativeVel );

		SetNextThink(gpGlobals->curtime + m_soundInfo.getThinkRate());
	}

}

void CPhysSlideConstraint::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	m_soundInfo.OnActivate(this);

	Vector axisDirection = m_axisEnd - GetAbsOrigin();
	VectorNormalize( axisDirection );
	UTIL_SnapDirectionToAxis( axisDirection );
	
	if ( m_pConstraint )
	{
		m_soundInfo.StartThinking(this, 
			VelocitySampler::GetRelativeVelocity(m_pConstraint->GetAttachedObject(), m_pConstraint->GetReferenceObject()),
			axisDirection
			);
	
		SetThink(&CPhysSlideConstraint::SoundThink);
		SetNextThink(gpGlobals->curtime + m_soundInfo.getThinkRate());
	}
}

void CPhysSlideConstraint::Precache_Impl()
{
	m_soundInfo.OnPrecache(this);
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Fixed breakable constraint
//-----------------------------------------------------------------------------
class CPhysFixed : public CPhysConstraint
{
	DECLARE_CLASS( CPhysFixed, CPhysConstraint );
public:
	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info );
	
	// just for debugging - move to the position of the reference entity
	void MoveToRefPosition()
	{
		if ( m_pConstraint )
		{
			matrix3x4_t xformRef;
			m_pConstraint->GetConstraintTransform( &xformRef, nullptr );
			IPhysicsObject *pObj = m_pConstraint->GetReferenceObject();
			if ( pObj && pObj->IsMoveable() )
			{
				Vector pos, posWorld;
				MatrixPosition( xformRef, pos );
				pObj->LocalToWorld(&posWorld, pos);
				SetAbsOrigin(posWorld);
			}
		}
	}
	int DrawDebugTextOverlays_Impl()
	{
		if ( m_debugOverlays & OVERLAY_TEXT_BIT )
		{
			MoveToRefPosition();
		}
		return BaseClass::DrawDebugTextOverlays_Impl();
	}
	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			MoveToRefPosition();
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}
};

LINK_ENTITY_TO_CLASS( phys_constraint, CPhysFixed );

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CPhysFixed::CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
{
	constraint_fixedparams_t fixed;
	fixed.Defaults();
	fixed.InitWithCurrentObjectState( info.pObjects[0], info.pObjects[1] );
	GetBreakParams( fixed.constraint, info );

	// constraining to the world means object 1 is fixed
	if ( info.pObjects[0] == g_PhysWorldObject )
	{
		PhysSetGameFlags( info.pObjects[1], FVPHYSICS_CONSTRAINT_STATIC );
	}

	return physenv->CreateFixedConstraint( info.pObjects[0], info.pObjects[1], pGroup, fixed );
}

//-----------------------------------------------------------------------------
// Purpose: Breakable pulley w/ropes constraint
//-----------------------------------------------------------------------------
class CPhysPulley : public CPhysConstraint
{
	DECLARE_CLASS( CPhysPulley, CPhysConstraint );
public:
	DECLARE_DATADESC();

	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			Vector origin = GetAbsOrigin();
			Vector refPos = origin, attachPos = origin;
			IPhysicsObject *pRef = m_pConstraint->GetReferenceObject();
			if ( pRef )
			{
				matrix3x4_t matrix;
				pRef->GetPositionMatrix( &matrix );
				VectorTransform( m_offset[0], matrix, refPos );
			}
			IPhysicsObject *pAttach = m_pConstraint->GetAttachedObject();
			if ( pAttach )
			{
				matrix3x4_t matrix;
				pAttach->GetPositionMatrix( &matrix );
				VectorTransform( m_offset[1], matrix, attachPos );
			}
			NDebugOverlay::Line( refPos, origin, 0, 255, 0, false, 0 );
			NDebugOverlay::Line( origin, m_position2, 128, 128, 128, false, 0 );
			NDebugOverlay::Line( m_position2, attachPos, 0, 255, 0, false, 0 );
			NDebugOverlay::Box( origin, -Vector(8,8,8), Vector(8,8,8), 128, 255, 128, 32, 0 );
			NDebugOverlay::Box( m_position2, -Vector(8,8,8), Vector(8,8,8), 255, 128, 128, 32, 0 );
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info );

private:
	[[= ks::reflect::As{ FIELD_POSITION_VECTOR } ]] [[= ks::reflect::Key{ .name = "position2" } ]] Vector		m_position2;
	Vector		m_offset[2];
	[[= ks::reflect::Key{ .name = "addlength" } ]] float		m_addLength;
	[[= ks::reflect::Key{ .name = "gearratio" } ]] float		m_gearRatio;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysPulley )

LINK_ENTITY_TO_CLASS( phys_pulleyconstraint, CPhysPulley );

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CPhysPulley::CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
{
	constraint_pulleyparams_t pulley;
	pulley.Defaults();
	pulley.pulleyPosition[0] = GetAbsOrigin();
	pulley.pulleyPosition[1] = m_position2;

	matrix3x4_t matrix;
	Vector world[2];

	info.pObjects[0]->GetPositionMatrix( &matrix );
	VectorTransform( info.anchorPosition[0], matrix, world[0] );
	info.pObjects[1]->GetPositionMatrix( &matrix );
	VectorTransform( info.anchorPosition[1], matrix, world[1] );

	for ( int i = 0; i < 2; i++ )
	{
		pulley.objectPosition[i] = info.anchorPosition[i];
		m_offset[i] = info.anchorPosition[i];
	}
	
	pulley.totalLength = m_addLength + 
		(world[0] - pulley.pulleyPosition[0]).Length() + 
		((world[1] - pulley.pulleyPosition[1]).Length() * m_gearRatio);

	if ( m_gearRatio != 0 )
	{
		pulley.gearRatio = m_gearRatio;
	}
	GetBreakParams( pulley.constraint, info );
	if ( m_spawnflags & SF_PULLEY_RIGID )
	{
		pulley.isRigid = true;
	}

	return physenv->CreatePulleyConstraint( info.pObjects[0], info.pObjects[1], pGroup, pulley );
}

//-----------------------------------------------------------------------------
// Purpose: Breakable rope/length constraint
//-----------------------------------------------------------------------------
class CPhysLength : public CPhysConstraint
{
	DECLARE_CLASS( CPhysLength, CPhysConstraint );
public:
	DECLARE_DATADESC();

	void DrawDebugGeometryOverlays_Impl()
	{
		if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
		{
			Vector origin = GetAbsOrigin();
			Vector refPos = origin, attachPos = origin;
			IPhysicsObject *pRef = m_pConstraint->GetReferenceObject();
			if ( pRef )
			{
				matrix3x4_t matrix;
				pRef->GetPositionMatrix( &matrix );
				VectorTransform( m_offset[0], matrix, refPos );
			}
			IPhysicsObject *pAttach = m_pConstraint->GetAttachedObject();
			if ( pAttach )
			{
				matrix3x4_t matrix;
				pAttach->GetPositionMatrix( &matrix );
				VectorTransform( m_offset[1], matrix, attachPos );
			}
			Vector dir = attachPos - refPos;

			float len = VectorNormalize(dir);
			if ( len > m_totalLength )
			{
				Vector mid = refPos + dir * m_totalLength;
				NDebugOverlay::Line( refPos, mid, 0, 255, 0, false, 0 );
				NDebugOverlay::Line( mid, attachPos, 255, 0, 0, false, 0 );
			}
			else
			{
				NDebugOverlay::Line( refPos, attachPos, 0, 255, 0, false, 0 );
			}
		}
		BaseClass::DrawDebugGeometryOverlays_Impl();
	}

	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info );

private:
	Vector		m_offset[2];
	[[= ks::reflect::As{ FIELD_POSITION_VECTOR } ]] [[= ks::reflect::Key{ .name = "attachpoint" } ]] Vector		m_vecAttach;
	[[= ks::reflect::Key{ .name = "addlength" } ]] float		m_addLength;
	[[= ks::reflect::Key{ .name = "minlength" } ]] float		m_minLength;
	float		m_totalLength;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysLength )

LINK_ENTITY_TO_CLASS( phys_lengthconstraint, CPhysLength );

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CPhysLength::CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
{
	constraint_lengthparams_t length;
	length.Defaults();
	Vector position[2];
	position[0] = GetAbsOrigin();
	position[1] = m_vecAttach;
	int index = info.swapped ? 1 : 0;
	length.InitWorldspace( info.pObjects[0], info.pObjects[1], position[index], position[!index] );
	length.totalLength += m_addLength;
	length.minLength = m_minLength;
	m_totalLength = length.totalLength;
	if ( HasSpawnFlags(SF_LENGTH_RIGID) )
	{
		length.minLength = length.totalLength;
	}

	for ( int i = 0; i < 2; i++ )
	{
		m_offset[i] = length.objectPosition[i];
	}
	GetBreakParams( length.constraint, info );

	return physenv->CreateLengthConstraint( info.pObjects[0], info.pObjects[1], pGroup, length );
}

//-----------------------------------------------------------------------------
// Purpose: Limited ballsocket constraint with toggle-able translation constraints
//-----------------------------------------------------------------------------
class CRagdollConstraint : public CPhysConstraint
{
	DECLARE_CLASS( CRagdollConstraint, CPhysConstraint );
public:
	DECLARE_DATADESC();

	IPhysicsConstraint *CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info );

private:
	[[= ks::reflect::Key{ .name = "xmin" } ]] float		m_xmin;	// constraint limits in degrees
	[[= ks::reflect::Key{ .name = "xmax" } ]] float		m_xmax;
	[[= ks::reflect::Key{ .name = "ymin" } ]] float		m_ymin;
	[[= ks::reflect::Key{ .name = "ymax" } ]] float		m_ymax;
	[[= ks::reflect::Key{ .name = "zmin" } ]] float		m_zmin;
	[[= ks::reflect::Key{ .name = "zmax" } ]] float		m_zmax;

	[[= ks::reflect::Key{ .name = "xfriction" } ]] float		m_xfriction;
	[[= ks::reflect::Key{ .name = "yfriction" } ]] float		m_yfriction;
	[[= ks::reflect::Key{ .name = "zfriction" } ]] float		m_zfriction;
};

IMPLEMENT_REFLECT_DATAMAP( CRagdollConstraint )

LINK_ENTITY_TO_CLASS( phys_ragdollconstraint, CRagdollConstraint );

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CRagdollConstraint::CreateConstraint( IPhysicsConstraintGroup *pGroup, const hl_constraint_info_t &info )
{
	constraint_ragdollparams_t ragdoll;
	ragdoll.Defaults();

	matrix3x4_t entityToWorld, worldToEntity;
	info.pObjects[0]->GetPositionMatrix( &entityToWorld );
	MatrixInvert( entityToWorld, worldToEntity );
	ConcatTransforms( worldToEntity, EntityToWorldTransform(), ragdoll.constraintToReference );

	info.pObjects[1]->GetPositionMatrix( &entityToWorld );
	MatrixInvert( entityToWorld, worldToEntity );
	ConcatTransforms( worldToEntity, EntityToWorldTransform(), ragdoll.constraintToAttached );

	ragdoll.onlyAngularLimits = HasSpawnFlags( SF_RAGDOLL_FREEMOVEMENT ) ? true : false;

	// FIXME: Why are these friction numbers in different units from what the hinge uses?
	ragdoll.axes[0].SetAxisFriction( m_xmin, m_xmax, m_xfriction );
	ragdoll.axes[1].SetAxisFriction( m_ymin, m_ymax, m_yfriction );
	ragdoll.axes[2].SetAxisFriction( m_zmin, m_zmax, m_zfriction );

	if ( HasSpawnFlags( SF_CONSTRAINT_START_INACTIVE ) )
	{
		ragdoll.isActive = false;
	}
	return physenv->CreateRagdollConstraint( info.pObjects[0], info.pObjects[1], pGroup, ragdoll );
}

class CPhysConstraintEvents : public IPhysicsConstraintEvent
{
	void ConstraintBroken( IPhysicsConstraint *pConstraint )
	{
		CBaseEntity *pEntity = (CBaseEntity *)pConstraint->GetGameData();
		if ( pEntity )
		{
			IPhysicsConstraintEvent *pConstraintEvent = dynamic_cast<IPhysicsConstraintEvent*>( pEntity );
			//Msg("Constraint broken %s\n", pEntity->GetDebugName() );
			if ( pConstraintEvent )
			{
				pConstraintEvent->ConstraintBroken( pConstraint );
			}
			else
			{
				variant_t emptyVariant;
				pEntity->AcceptInput( "ConstraintBroken", nullptr, nullptr, emptyVariant, 0 );
			}
		}
	}
};

static CPhysConstraintEvents constraintevents;
// registered in physics.cpp
IPhysicsConstraintEvent *g_pConstraintEvents = &constraintevents;

#if HINGE_NOTIFY
//-----------------------------------------------------------------------------
// Code for sampler
//-----------------------------------------------------------------------------

/// Call this in spawn(). (Not a constructor because those are difficult to use in entities.)
void VelocitySampler::Initialize(float samplerate)
{
	m_fIdealSampleRate = samplerate;
}

// This is an old style approach to reversal sounds, from when there was only one.

/// Looks at the force of reversal and compares it to a ladder of thresholds.
/// Returns the index of the highest threshold exceeded by the reversal velocity. 
int VelocitySampler::HasReversed(const Vector &relativeVelocity, const float thresholdAcceleration[], const unsigned short numThresholds)
{
	// first, make sure the velocity has reversed (is more than 90deg off) from last time, or is zero now.
	// float rVsq = relativeVelocity.LengthSqr();
	float vDot = relativeVelocity.Dot(m_prevSample);
	if (vDot <= 0) // there is a reversal in direction. compute the magnitude of acceleration.
	{
		// find the scalar projection of the relative acceleration this fame onto the previous frame's
		// velocity, and compare that to the threshold. 
		Vector accel = relativeVelocity - m_prevSample;

		float prevSampleLength = m_prevSample.Length();
		float projection = 0;
		// divide through by dt to get the accel per sec
		if (prevSampleLength)
		{
			// the scalar projection is negative because the acceleration is against vel
			projection = -(accel.Dot(m_prevSample) / prevSampleLength) / (gpGlobals->curtime - m_fPrevSampleTime);
		}
		else
		{
			projection = accel.Length() / (gpGlobals->curtime - m_fPrevSampleTime);
		}

		if (g_debug_constraint_sounds.GetBool())
		{
			Msg("Reversal accel is %f/%f\n", projection, thresholdAcceleration[0]);
		}

		// now find the threshold crossed.
		int retval;
		for (retval = numThresholds - 1; retval >= 0 ; --retval)
		{
			if (projection > thresholdAcceleration[retval])
				break;
		}

		return retval; 
	}
	else
	{
		return -1;
	}
}

/// small helper function used just below (technique copy-pasted  from sound.cpp)
inline static bool IsEmpty (const string_t &str)
{
	return (!str || strlen(str.ToCStr()) < 1 );
}

void ConstraintSoundInfo::OnActivate( CPhysConstraint *pOuter )
{
	m_pTravelSound = nullptr;
	m_vSampler.Initialize( getThinkRate() );

	ValidateInternals( pOuter );

	// make sure sound filenames are not empty 
	m_bPlayTravelSound   = !IsEmpty(m_iszTravelSoundFwd) || !IsEmpty(m_iszTravelSoundBack);
	m_bPlayReversalSound = false;
	for (int i = 0; i < SimpleConstraintSoundProfile::kREVERSAL_SOUND_ARRAY_SIZE ; ++i)
	{
		if ( !IsEmpty(m_iszReversalSounds[i]) )
		{
			// if there is at least one filled sound field, we should try
			// to play reversals
			m_bPlayReversalSound = true;
			break;
		}
	}

	/*
	SetThink(&CPhysSlideConstraint::SoundThink);
	SetNextThink(gpGlobals->curtime + m_vSampler.getSampleRate());
	*/
}

/// Maintain consistency of internal datastructures on start
void ConstraintSoundInfo::ValidateInternals( CPhysConstraint *pOuter )
{
	// Make sure the reversal sound thresholds are strictly increasing.
	for (int i = 1 ; i < SimpleConstraintSoundProfile::kREVERSAL_SOUND_ARRAY_SIZE ; ++i)
	{
		// if decreases from small to medium, promote small to medium and warn.
		if (m_soundProfile.m_reversalSoundThresholds[i] < m_soundProfile.m_reversalSoundThresholds[i-1])
		{
			Warning("Constraint reversal sounds for %s are out of order!", pOuter->GetDebugName() );
			m_soundProfile.m_reversalSoundThresholds[i] = m_soundProfile.m_reversalSoundThresholds[i-1];
			m_iszReversalSounds[i] = m_iszReversalSounds[i-1];
		}
	}
}

void ConstraintSoundInfo::OnPrecache( CPhysConstraint *pOuter )
{
	pOuter->PrecacheScriptSound( m_iszTravelSoundFwd.ToCStr() ); 
	pOuter->PrecacheScriptSound( m_iszTravelSoundBack.ToCStr() ); 
	for (int i = 0 ; i < SimpleConstraintSoundProfile::kREVERSAL_SOUND_ARRAY_SIZE; ++i )
	{
		pOuter->PrecacheScriptSound( m_iszReversalSounds[i].ToCStr() );
	}
}

void ConstraintSoundInfo::OnThink( CPhysConstraint *pOuter, const Vector &relativeVelocity )
{
	// have we had a hard reversal?
	int playReversal = m_vSampler.HasReversed( relativeVelocity, m_soundProfile.m_reversalSoundThresholds, SimpleConstraintSoundProfile::kREVERSAL_SOUND_ARRAY_SIZE );
	float relativeVelMag = relativeVelocity.Length(); //< magnitude of relative velocity

	CBaseEntity *pChildEntity = static_cast<CBaseEntity *>(pOuter->GetPhysConstraint()->GetAttachedObject()->GetGameData());

	// compute sound level
	float soundVol = this->m_soundProfile.GetVolume(relativeVelMag);

	if (g_debug_constraint_sounds.GetBool())
	{
		char tempstr[512];
		V_snprintf(tempstr,sizeof(tempstr),"Velocity: %.3f", relativeVelMag );
		pChildEntity->EntityText( 0, tempstr, m_vSampler.getSampleRate() );

		V_snprintf(tempstr,sizeof(tempstr),"Sound volume: %.3f", soundVol );
		pChildEntity->EntityText( 1, tempstr, m_vSampler.getSampleRate() );

		if (playReversal >= 0)
		{
			V_snprintf(tempstr,sizeof(tempstr),"Reversal [%d]", playReversal );
			pChildEntity->EntityText(2,tempstr,m_vSampler.getSampleRate());
		}
	}

	// if we loaded a travel sound
	if (m_bPlayTravelSound)
	{
		if (soundVol > 0)
		{
			// if we want to play a sound...
			if ( m_pTravelSound )
			{	// if a sound exists, modify it
				CSoundEnvelopeController::GetController().SoundChangeVolume( m_pTravelSound, soundVol, 0.1f );
			}
			else
			{	// if a sound does not exist, create it
				bool travellingForward = relativeVelocity.Dot(m_forwardAxis) > 0;

				CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
				CBroadcastRecipientFilter filter;
				m_pTravelSound = controller.SoundCreate( filter, pChildEntity->entindex(), 
					(travellingForward ? m_iszTravelSoundFwd : m_iszTravelSoundBack).ToCStr() );
				controller.Play( m_pTravelSound, soundVol, 100 );
			}
		}
		else
		{
			// if we want to not play sound
			if ( m_pTravelSound )
			{	// and it exists, kill it
				CSoundEnvelopeController::GetController().SoundDestroy( m_pTravelSound );
				m_pTravelSound = nullptr;
			}
		}
	}

	if (m_bPlayReversalSound && (playReversal >= 0))
	{
		pChildEntity->EmitSound(m_iszReversalSounds[playReversal].ToCStr());
	}

	m_vSampler.AddSample( relativeVelocity );
	
}

void ConstraintSoundInfo::StartThinking( CPhysConstraint *pOuter, const Vector &relativeVelocity, const Vector &forwardVector )
{
	m_forwardAxis = forwardVector;
	m_vSampler.BeginSampling( relativeVelocity );

	/*
	IPhysicsConstraint *pConstraint = pOuter->GetPhysConstraint();
	Assert(pConstraint);
	if (pConstraint)
	{
		IPhysicsObject * pAttached = pConstraint->GetAttachedObject(), *pReference = pConstraint->GetReferenceObject();
		m_vSampler.BeginSampling( VelocitySampler::GetRelativeVelocity(pAttached,pReference) );
	}
	*/
}

void ConstraintSoundInfo::StopThinking( CPhysConstraint *pOuter )
{
	DeleteAllSounds();
}

ConstraintSoundInfo::~ConstraintSoundInfo()
{
	DeleteAllSounds();
}

// Any sounds envelopes that are active, kill.
void ConstraintSoundInfo::DeleteAllSounds()
{
	if ( m_pTravelSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pTravelSound );
		m_pTravelSound = nullptr;
	}
}

#endif

KS_ENTITY_ABSTRACT_KIND( CPhysConstraint )

#undef HINGE_NOTIFY
