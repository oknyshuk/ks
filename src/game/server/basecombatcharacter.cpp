// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <time.h>
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
#include "reflect.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "ai_activity.h"
#include "basecombatweapon_shared.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "engine/IEngineTrace.h"
#include "iservernetworkable.h"
#include "reflect_annotations.h"
#include "string_t.h"
#include "predictable_entity.h"
#include "entity_dispatch.h"
module ks.server;

import :entity_dispatch;
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.threadtools;
import ks.core.vprof;
import ks.game.rumble_shared;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :vscript;
import :sharedvar;
import :player;
import ks.game.ai_activity;
import :fish;
import :baseentity;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.types;
import ks.core.wchartypes;
import :cstrike15.cs_playeranimstate;
import ks.core.random;
import :econ.econ_item_schema_minimal;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CItem;
class CNavArea;
class CTakeDamageInfo;
struct animevent_t;
struct fogparams_t;
struct gamevcollisionevent_t;
struct impactdamagetable_t;
struct inputdata_t;
struct vphysics_objectstress_t;

using namespace ::ks::reflect;

// Purpose: Base combat character with no AI

extern ConVar weapon_showproficiency;

ConVar ai_show_hull_attacks( side_registry(), "ai_show_hull_attacks", "0" );
ConVar ai_force_serverside_ragdoll( side_registry(), "ai_force_serverside_ragdoll", "0" );

#ifndef _RETAIL
ConVar ai_use_visibility_cache( side_registry(), "ai_use_visibility_cache", "1" );
#define ShouldUseVisibilityCache() ai_use_visibility_cache.GetBool()
#else
#define ShouldUseVisibilityCache() true
#endif

IMPLEMENT_REFLECT_DATAMAP( CBaseCombatCharacter )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( Relationship_t )

//-----------------------------------------------------------------------------
// Init static variables
//-----------------------------------------------------------------------------
int					CBaseCombatCharacter::m_lastInteraction   = 0;
Relationship_t**	CBaseCombatCharacter::m_DefaultRelationship	= nullptr;
Relationship_t**	CBaseCombatCharacter::m_FactionRelationship	= nullptr;
std::vector< std::vector< EHANDLE > > CBaseCombatCharacter::m_aFactions;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCleanupDefaultRelationShips : public CAutoGameSystem
{
public:
	CCleanupDefaultRelationShips( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual void Shutdown()
	{
		if ( CBaseCombatCharacter::m_DefaultRelationship != nullptr )
		{
			int iNumClasses = GameRules() ? GameRules()->NumEntityClasses() : LAST_SHARED_ENTITY_CLASS;
			for ( int i=0; i<iNumClasses; ++i )
			{
				delete[] CBaseCombatCharacter::m_DefaultRelationship[ i ];
			}

			delete[] CBaseCombatCharacter::m_DefaultRelationship;
			CBaseCombatCharacter::m_DefaultRelationship = nullptr;
		}

		if ( CBaseCombatCharacter::m_FactionRelationship != nullptr )
		{
			for( int i = 0; i < std::ssize( CBaseCombatCharacter::m_aFactions ); i++ )
			{
				delete[] CBaseCombatCharacter::m_FactionRelationship[ i ];
				CBaseCombatCharacter::m_aFactions[ i ].clear(); CBaseCombatCharacter::m_aFactions[ i ].shrink_to_fit();
			}
			CBaseCombatCharacter::m_aFactions.clear(); CBaseCombatCharacter::m_aFactions.shrink_to_fit();
			delete[] CBaseCombatCharacter::m_FactionRelationship;
			CBaseCombatCharacter::m_FactionRelationship = nullptr;
		}
	}
};

static CCleanupDefaultRelationShips g_CleanupDefaultRelationships( "CCleanupDefaultRelationShips" );

void *SendProxy_SendBaseCombatCharacterLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Only send to local player if this is a player
	pRecipients->ClearAllRecipients();
	
	CBaseCombatCharacter *pBCC = ( CBaseCombatCharacter * )pStruct;
	if ( pBCC != nullptr)
	{
		if ( pBCC->IsPlayer() )
		{
			SendProxyRecipients_SetOnly( pRecipients,  pBCC->entindex() - 1 );
		}
		else
		{
			// If it's a vehicle, send to "driver" (e.g., operator of tf2 manned guns)
			
		}
	}
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendBaseCombatCharacterLocalDataTable );

void *SendProxy_SendBaseCombatCharacterNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Send to all players except itself
	CBaseCombatCharacter *pBCC = ( CBaseCombatCharacter * )pStruct;
	if ( pBCC != nullptr)
	{
		if ( pBCC->IsPlayer() )
		{
			SendProxyRecipients_ExcludeOnly( pRecipients,  pBCC->entindex() - 1 );
		}
	}
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendBaseCombatCharacterNonLocalDataTable );

// Only send active weapon index to local player
IMPLEMENT_REFLECT_TABLE_IN( CBaseCombatCharacter, DT_BCCLocalPlayerExclusive );

IMPLEMENT_REFLECT_TABLE_IN( CBaseCombatCharacter, DT_BCCNonLocalPlayerExclusive );

//-----------------------------------------------------------------------------
// This table encodes the CBaseCombatCharacter
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CBaseCombatCharacter, DT_BaseCombatCharacter )

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InitInteractionSystem()
{
	// interaction ids continue to go up with every map load, otherwise you get
	// collisions if a future map has a different set of NPCs from a current map
}

//-----------------------------------------------------------------------------
// Purpose: Return an interaction ID (so we have no collisions)
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetInteractionID(void)
{
	m_lastInteraction++;
	return (m_lastInteraction);
}

// ============================================================================
bool CBaseCombatCharacter::HasHumanGibs( void )
{

	return false;
}

bool CBaseCombatCharacter::HasAlienGibs( void )
{

	return false;
}

void CBaseCombatCharacter::CorpseFade( void )
{
	StopAnimation();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );
	SetLocalAngularVelocity( vec3_angle );
	m_flAnimTime = gpGlobals->curtime;
	AddEffects( EF_NOINTERP );
	SUB_StartFadeOut();
}

//-----------------------------------------------------------------------------
// Visibility caching
//-----------------------------------------------------------------------------

struct VisibilityCacheEntry_t
{
	EHANDLE		pBlocker;
	float		time;
};

// Keyed by the entity pair, lower pointer first.
static std::map<std::pair<CBaseEntity *, CBaseEntity *>, VisibilityCacheEntry_t> g_VisibilityCache;
// Legacy tree used unsigned short indices; keep its capacity limit.
const std::size_t VIS_CACHE_MAX_ENTRIES = 0xFFFF;
static const float VIS_CACHE_ENTRY_LIFE = .090;

bool CBaseCombatCharacter::FVisible_Impl( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	CVProfScope _( "CBaseCombatCharacter::FVisible", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( traceMask != MASK_BLOCKLOS || !ShouldUseVisibilityCache() || pEntity == this
		 )
	{
		return BaseClass::FVisible_Impl( pEntity, traceMask, ppBlocker );
	}

	std::pair<CBaseEntity *, CBaseEntity *> cacheKey;

	if ( this < pEntity )
	{
		cacheKey = { this, pEntity };
	}
	else
	{
		cacheKey = { pEntity, this };
	}

	auto iCache = g_VisibilityCache.find( cacheKey );

	if ( iCache != g_VisibilityCache.end() )
	{
		if ( gpGlobals->curtime - iCache->second.time < VIS_CACHE_ENTRY_LIFE )
		{
			bool bBlockerValid = iCache->second.pBlocker.IsValid();
			if ( bBlockerValid )
			{
				if ( ppBlocker )
				{
					*ppBlocker = iCache->second.pBlocker;
					if ( !*ppBlocker )
					{
						*ppBlocker = GetWorldEntity();
					}
				}
			}
			else
			{
				if ( ppBlocker )
				{
					*ppBlocker = nullptr;
				}
			}

			return !bBlockerValid;
		}
	}
	else
	{
		if ( g_VisibilityCache.size() < VIS_CACHE_MAX_ENTRIES )
		{
			iCache = g_VisibilityCache.emplace( cacheKey, VisibilityCacheEntry_t{} ).first;
		}
		else
		{
			return BaseClass::FVisible_Impl( pEntity, traceMask, ppBlocker );
		}
	}

	CBaseEntity *pBlocker = nullptr;
	if ( ppBlocker == nullptr )
	{
		ppBlocker = &pBlocker;
	}

	bool bResult = BaseClass::FVisible_Impl( pEntity, traceMask, ppBlocker );

	if ( !bResult )
	{
		iCache->second.pBlocker = *ppBlocker;
	}
	else
	{
		iCache->second.pBlocker = nullptr;
	}

	iCache->second.time = gpGlobals->curtime;

	return bResult;
}

void CBaseCombatCharacter::ResetVisibilityCache( CBaseCombatCharacter *pBCC )
{
	CVProfScope _( "CBaseCombatCharacter::ResetVisibilityCache", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( !pBCC )
	{
		g_VisibilityCache.clear();
		return;
	}

	std::erase_if( g_VisibilityCache, [pBCC]( const auto &entry ) { return entry.first.first == pBCC || entry.first.second == pBCC; } );
}

//-----------------------------------------------------------------------------

//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInViewCone( CBaseEntity *pEntity )
{
	return FInViewCone( pEntity->WorldSpaceCenter() );
}

//=========================================================
// FInViewCone - returns true is the passed Vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInViewCone( const Vector &vecSpot )
{
	Vector eyepos = EyePosition();
	// do this in 2D
	eyepos.z = vecSpot.z;

	return PointWithinViewAngle( eyepos, vecSpot, EyeDirection2D(), m_flFieldOfView );
}

//=========================================================
// FInAimCone - returns true is the passed ent is in
// the caller's forward aim cone. The dot product is performed
// in 2d, making the aim cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInAimCone( CBaseEntity *pEntity )
{
	return FInAimCone( pEntity->BodyTarget( EyePosition() ) );
}

//=========================================================
// FInAimCone - returns true is the passed Vector is in
// the caller's forward aim cone. The dot product is performed
// in 2d, making the view cone infinitely tall. By default, the
// callers aim cone is assumed to be very narrow
//=========================================================
bool CBaseCombatCharacter::FInAimCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - GetAbsOrigin() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = BodyDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if ( flDot > 0.994 )//!!!BUGBUG - magic number same as FacingIdeal(), what is this?
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  The type of interaction, extra info pointer, and who started it
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor : Initialize some fields
//-----------------------------------------------------------------------------
CBaseCombatCharacter::CBaseCombatCharacter( void )
{

	// Zero the damage accumulator.
	m_flDamageAccumulator = 0.0f;

	// Init weapon and Ammo data
	m_hActiveWeapon			= nullptr;
	m_uiLastDamageTypeFlags = 0;

	// Init faction
	m_nFaction = FACTION_NONE;

	// reset all ammo values to 0
	RemoveAllAmmo();
	
	// not alive yet
	m_aliveTimer.Invalidate();
	m_hasBeenInjured = 0;

	for( int t=0; t<MAX_DAMAGE_TEAMS; ++t )
	{
		m_damageHistory[t].team = TEAM_INVALID;
	}

	m_flTimeOfLastInjury = 0.0f;

	// not standing on a nav area yet
	m_lastNavArea = nullptr;
	m_registeredNavTeam = TEAM_INVALID;

	m_LastHitGroup = HITGROUP_GENERIC;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		m_hMyWeapons.Set( i, INVALID_EHANDLE );
	}

	V_memset( m_weaponIDToIndex, 0, sizeof(m_weaponIDToIndex) );
	// Default so that spawned entities have this set
	m_impactEnergyScale = 1.0f;

	m_bForceServerRagdoll = ai_force_serverside_ragdoll.GetBool();
}

//------------------------------------------------------------------------------
// Purpose : Destructor
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseCombatCharacter::~CBaseCombatCharacter( void )
{
	if ( ( m_nFaction != FACTION_NONE ) && ( !m_aFactions.empty() ) )
	{
		ks::swap_erase_first( m_aFactions[ m_nFaction ], this );
	}
	ResetVisibilityCache( this );
	ClearLastKnownArea();
}

//-----------------------------------------------------------------------------
// Purpose: Put the combat character into the environment
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
	
	SetBlocksLOS( false );
	m_aliveTimer.Start();
	m_hasBeenInjured = 0;

	for( int t=0; t<MAX_DAMAGE_TEAMS; ++t )
	{
		m_damageHistory[t].team = TEAM_INVALID;
	}

	m_flTimeOfLastInjury = 0.0f;
	m_LastHitGroup = HITGROUP_GENERIC;

	// not standing on a nav area yet
	ClearLastKnownArea();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Precache_Impl()
{
	CVProfScope _( "CBaseCombatCharacter::Precache", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	BaseClass::Precache_Impl();

	PrecacheScriptSound( "BaseCombatCharacter.CorpseGib" );
	PrecacheScriptSound( "BaseCombatCharacter.StopWeaponSounds" );
	PrecacheScriptSound( "BaseCombatCharacter.AmmoPickup" );

	for ( int i = std::ssize( m_Relationship ) - 1; i >= 0 ; i--) 
	{
		if ( !m_Relationship[i].entity && m_Relationship[i].classType == CLASS_NONE ) 
		{
			DevMsg( 2, "Removing relationship for lost entity\n" );
			ks::swap_remove( m_Relationship, i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::UpdateOnRemove_Impl( void )
{
	int i;
	// Make sure any weapons I didn't drop get removed.
	for (i=0;i<MAX_WEAPONS;i++) 
	{
		if (m_hMyWeapons[i]) 
		{
			UTIL_Remove( m_hMyWeapons[i] );
		}
	}

	V_memset( m_weaponIDToIndex, 0, sizeof(m_weaponIDToIndex) );
	// tell owner ( if any ) that we're dead.This is mostly for NPCMaker functionality.
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		pOwner->DeathNotice( this );
		SetOwnerEntity( nullptr );
	}

	RemoveAllWearables();

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove_Impl();
}

//=========================================================
// CorpseGib - create some gore and get rid of a character's
// model.
//=========================================================
bool CBaseCombatCharacter::CorpseGib( const CTakeDamageInfo &info )
{
	trace_t		tr;
	bool		gibbed = false;

	EmitSound( "BaseCombatCharacter.CorpseGib" );

	// only humans throw skulls !!!UNDONE - eventually NPCs will have their own sets of gibs
	if ( HasHumanGibs() )
	{
		CGib::SpawnHeadGib( this );
		CGib::SpawnRandomGibs( this, 4, GIB_HUMAN );	// throw some human gibs.
		gibbed = true;
	}
	else if ( HasAlienGibs() )
	{
		CGib::SpawnRandomGibs( this, 4, GIB_ALIEN );	// Throw alien gibs
		gibbed = true;
	}

	return gibbed;
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CBaseCombatCharacter::GetDeathActivity ( void )
{
	Activity	deathActivity;
	bool		fTriedDirection;
	float		flDot;
	trace_t		tr;
	Vector		vecSrc;

	if (IsPlayer())
	{
		// die in an interesting way
		switch( random_valve->RandomInt(0,7) )
		{
		case 0:	return ACT_DIESIMPLE;
		case 1: return ACT_DIEBACKWARD;
		case 2: return ACT_DIEFORWARD;
		case 3: return ACT_DIEVIOLENT;
		case 4: return ACT_DIE_HEADSHOT;
		case 5: return ACT_DIE_CHESTSHOT;
		case 6: return ACT_DIE_GUTSHOT;
		case 7: return ACT_DIE_BACKSHOT;
		}
	}

	vecSrc = WorldSpaceCenter();

	fTriedDirection = false;
	deathActivity = ACT_DIESIMPLE;// in case we can't find any special deaths to do.

	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );
	flDot = -DotProduct( forward, g_vecAttackDir );

	switch ( m_LastHitGroup )
	{
		// try to pick a region-specific death.
	case HITGROUP_HEAD:
		deathActivity = ACT_DIE_HEADSHOT;
		break;

	case HITGROUP_STOMACH:
		deathActivity = ACT_DIE_GUTSHOT;
		break;

	case HITGROUP_GENERIC:
		// try to pick a death based on attack direction
		fTriedDirection = true;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;

	default:
		// try to pick a death based on attack direction
		fTriedDirection = true;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;
	}

	// can we perform the prescribed death?
	if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// no! did we fail to perform a directional death? 
		if ( fTriedDirection )
		{
			// if yes, we're out of options. Go simple.
			deathActivity = ACT_DIESIMPLE;
		}
		else
		{
			// cannot perform the ideal region-specific death, so try a direction.
			if ( flDot > 0.3 )
			{
				deathActivity = ACT_DIEFORWARD;
			}
			else if ( flDot <= -0.3 )
			{
				deathActivity = ACT_DIEBACKWARD;
			}
		}
	}

	if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// if we're still invalid, simple is our only option.
		deathActivity = ACT_DIESIMPLE;

		if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
		{
			Msg( "ERROR! %s missing ACT_DIESIMPLE\n", STRING(GetModelName()) );
		}
	}

	if ( deathActivity == ACT_DIEFORWARD )
	{
			// make sure there's room to fall forward
			UTIL_TraceHull ( vecSrc, vecSrc + forward * 64, Vector(-16,-16,-18), 
				Vector(16,16,18), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( deathActivity == ACT_DIEBACKWARD )
	{
			// make sure there's room to fall backward
			UTIL_TraceHull ( vecSrc, vecSrc - forward * 64, Vector(-16,-16,-18), 
				Vector(16,16,18), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	return deathActivity;
}

// UNDONE: Should these operate on a list of weapon/items
Activity CBaseCombatCharacter::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	Activity translated = baseAct;

	if ( m_hActiveWeapon )
	{
		translated = m_hActiveWeapon->ActivityOverride( baseAct, pRequired );
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

//-----------------------------------------------------------------------------
// Purpose: NPCs should override this function to translate activities
//			such as ACT_WALK, etc.
// Input  :
// Output :
//-----------------------------------------------------------------------------
Activity CBaseCombatCharacter::NPC_TranslateActivity( Activity baseAct )
{
	return baseAct;
}

void CBaseCombatCharacter::Weapon_SetActivity( Activity newActivity, float duration )
{
	if ( m_hActiveWeapon )
	{
		m_hActiveWeapon->SetActivity( newActivity, duration );
	}
}

void CBaseCombatCharacter::Weapon_FrameUpdate( void )
{
	if ( m_hActiveWeapon )
	{
		m_hActiveWeapon->Operator_FrameUpdate( this );
	}
}

//------------------------------------------------------------------------------
// Purpose :	expects a length to trace, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );
	return CheckTraceHullAttack( vStart, vEnd, mins, maxs, flDamage, iDmgType, forceScale, bDamageAnyNPC );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHandleEntity - 
//			contentsMask - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTraceFilterMelee::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		return false;

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	
	if ( pEntity )
	{
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			return false;

		// FIXME: Do not translate this to the driver because the driver only accepts damage from the vehicle
		// Translate the vehicle into its driver for damage
		/*
		if ( nullptr != NULL )
		{
			CBaseEntity *pDriver = nullptr->GetPassenger();

			if ( pDriver != NULL )
			{
				pEntity = pDriver;
			}
		}
		*/

		Vector	attackDir = pEntity->WorldSpaceCenter() - m_dmgInfo->GetAttacker()->WorldSpaceCenter();
		VectorNormalize( attackDir );

		CTakeDamageInfo info = (*m_dmgInfo);				
		CalculateMeleeDamageForce( &info, attackDir, info.GetAttacker()->WorldSpaceCenter(), m_flForceScale );

		CBaseCombatCharacter *pBCC = info.GetAttacker()->MyCombatCharacterPointer();
		CBaseCombatCharacter *pVictimBCC = pEntity->MyCombatCharacterPointer();

		// Only do these comparisons between NPCs
		if ( pBCC && pVictimBCC )
		{
			// Can only damage other NPCs that we hate
			if ( m_bDamageAnyNPC || pBCC->IRelationType( pEntity ) == D_HT )
			{
				if ( info.GetDamage() )
				{
					pEntity->TakeDamage( info );
				}
				
				// Put a combat sound in
				CSoundEnt::InsertSound( SOUND_COMBAT, info.GetDamagePosition(), 200, 0.2f, info.GetAttacker() );

				m_pHit = pEntity;
				return true;
			}
		}
		else
		{
			m_pHit = pEntity;

			// Make sure if the player is holding this, he drops it
			Pickup_ForcePlayerToDropThisObject( pEntity );

			// Otherwise just damage passive objects in our way
			if ( info.GetDamage() )
			{
				pEntity->TakeDamage( info );
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Purpose :	start and end trace position, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		float length	 = (vEnd - vStart ).Length();
		Vector direction = (vEnd - vStart );
		VectorNormalize( direction );
		Vector hullMaxs = maxs;
		hullMaxs.x = length + hullMaxs.x;
		NDebugOverlay::BoxDirection(vStart, mins, hullMaxs, direction, 100,255,255,20,1.0);
		NDebugOverlay::BoxDirection(vStart, mins, maxs, direction, 255,0,0,20,1.0);
	}

	CTakeDamageInfo	dmgInfo( this, this, flDamage, iDmgType );
	
	// COLLISION_GROUP_PROJECTILE does some handy filtering that's very appropriate for this type of attack, as well. (sjb) 7/25/2007
	CTraceFilterMelee traceFilter( this, COLLISION_GROUP_PROJECTILE, &dmgInfo, flForceScale, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

	CBaseEntity *pEntity = traceFilter.m_pHit;
	
	if ( pEntity == nullptr )
	{
		// See if perhaps I'm trying to claw/bash someone who is standing on my head.
		Vector vecTopCenter;
		Vector vecEnd;
		Vector vecMins, vecMaxs;

		// Do a tracehull from the top center of my bounding box.
		vecTopCenter = GetAbsOrigin();
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecTopCenter.z = vecMaxs.z + 1.0f;
		vecEnd = vecTopCenter;
		vecEnd.z += 2.0f;
		
		ray.Init( vecTopCenter, vEnd, mins, maxs );
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

		pEntity = traceFilter.m_pHit;
	}

	if( pEntity && !pEntity->CanBeHitByMeleeAttack(this) )
	{
		// If we touched something, but it shouldn't be hit, return nothing.
		pEntity = nullptr;
	}

	return pEntity;

}

bool  CBaseCombatCharacter::Event_Gibbed( const CTakeDamageInfo &info )
{
	bool fade = false;

	if ( HasHumanGibs() )
	{
		ConVarRef violence_hgibs( "violence_hgibs" );
		if ( violence_hgibs.IsValid() && violence_hgibs.GetInt() == 0 )
		{
			fade = true;
		}
	}
	else if ( HasAlienGibs() )
	{
		ConVarRef violence_agibs( "violence_agibs" );
		if ( violence_agibs.IsValid() && violence_agibs.GetInt() == 0 )
		{
			fade = true;
		}
	}

	m_takedamage	= DAMAGE_NO;
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_lifeState		= LIFE_DEAD;

	if ( fade )
	{
		CorpseFade();
		return false;
	}
	else
	{
		AddEffects( EF_NODRAW ); // make the model invisible.
		return CorpseGib( info );
	}
}

Vector CBaseCombatCharacter::CalcDeathForceVector( const CTakeDamageInfo &info )
{
	// Already have a damage force in the data, use that.
	bool bNoPhysicsForceDamage = g_pGameRules->Damage_NoPhysicsForce( info.GetDamageType() );
	if ( info.GetDamageForce() != vec3_origin || bNoPhysicsForceDamage )
	{
		if( info.GetDamageType() & DMG_BLAST )
		{
			// Fudge blast forces a little bit, so that each
			// victim gets a slightly different trajectory. 
			// This simulates features that usually vary from
			// person-to-person variables such as bodyweight,
			// which are all indentical for characters using the same model.
			float scale = random_valve->RandomFloat( 0.85, 1.15 );
			Vector force = info.GetDamageForce();
			force.x *= scale;
			force.y *= scale;
			// Try to always exaggerate the upward force because we've got pretty harsh gravity
			force.z *= (force.z > 0) ? 1.15 : scale;
			return force;
		}

		return info.GetDamageForce();
	}

	CBaseEntity *pForce = info.GetInflictor();
	if ( !pForce )
	{
		pForce = info.GetAttacker();
	}

	if ( pForce )
	{
		// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
		float forceScale = info.GetDamage() * 75 * 4;

		Vector forceVector;
		// If the damage is a blast, point the force vector higher than usual, this gives 
		// the ragdolls a bodacious "really got blowed up" look.
		if( info.GetDamageType() & DMG_BLAST )
		{
			// exaggerate the force from explosions a little (37.5%)
			forceVector = (GetLocalOrigin() + Vector(0, 0, WorldAlignSize().z) ) - pForce->GetLocalOrigin();
			VectorNormalize(forceVector);
			forceVector *= 1.375f;
		}
		else
		{
			// taking damage from self?  Take a little random force, but still try to collapse on the spot.
			if ( this == pForce )
			{
				forceVector.x = random_valve->RandomFloat( -1.0f, 1.0f );
				forceVector.y = random_valve->RandomFloat( -1.0f, 1.0f );
				forceVector.z = 0.0;
				forceScale = random_valve->RandomFloat( 1000.0f, 2000.0f );
			}
			else
			{
				// UNDONE: Collision forces are baked in to CTakeDamageInfo now
				// UNDONE: Is this MOVETYPE_VPHYSICS code still necessary?
				if ( pForce->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					// killed by a physics object
					IPhysicsObject *pPhysics = VPhysicsGetObject();
					if ( !pPhysics )
					{
						pPhysics = pForce->VPhysicsGetObject();
					}
					pPhysics->GetVelocity( &forceVector, nullptr );
					forceScale = pPhysics->GetMass();
				}
				else
				{
					forceVector = GetLocalOrigin() - pForce->GetLocalOrigin();
					VectorNormalize(forceVector);
				}
			}
		}
		return forceVector * forceScale;
	}
	return vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::FixupBurningServerRagdoll( CBaseEntity *pRagdoll )
{
	if ( !IsOnFire() )
		return;

	// Move the fire effects entity to the ragdoll
	CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( GetEffectEntity() );
	if ( pFireChild )
	{
		SetEffectEntity( nullptr );
		pRagdoll->AddFlag( FL_ONFIRE );
		pFireChild->SetAbsOrigin( pRagdoll->GetAbsOrigin() );
		pFireChild->AttachToEntity( pRagdoll );
		pFireChild->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
 		pRagdoll->SetEffectEntity( pFireChild );

		color24 color = GetRenderColor();
		pRagdoll->SetRenderColor( color.r, color.g, color.b );
	}
}

bool CBaseCombatCharacter::BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags )
{

	CTakeDamageInfo info( pKiller, pKiller, 1.0f, DMG_GENERIC );

	info.SetDamageForce( forceVector );

	CBaseEntity *pRagdoll = CreateServerRagdoll( this, 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );

	pRagdoll->SetCollisionBounds( CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs() );

	CRagdollBoogie::Create( pRagdoll, 200, gpGlobals->curtime, duration, flags );

	CTakeDamageInfo ragdollInfo( pKiller, pKiller, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
	ragdollInfo.SetDamagePosition(WorldSpaceCenter());
	ragdollInfo.SetDamageForce( Vector( 0, 0, 1) );
	TakeDamage( ragdollInfo );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::BecomeRagdoll( const CTakeDamageInfo &info, const Vector &forceVector )
{
	if ( (info.GetDamageType() & DMG_VEHICLE) && !g_pGameRules->IsMultiplayer() )
	{
		CTakeDamageInfo info2 = info;
		info2.SetDamageForce( forceVector );
		Vector pos = info2.GetDamagePosition();
		float flAbsMinsZ = GetAbsOrigin().z + WorldAlignMins().z;
		if ( (pos.z - flAbsMinsZ) < 24 )
		{
			// HACKHACK: Make sure the vehicle impact is at least 2ft off the ground
			pos.z = flAbsMinsZ + 24;
			info2.SetDamagePosition( pos );
		}

		// in single player create ragdolls on the server when the player hits someone
		// with their vehicle - for more dramatic death/collisions
		CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, info2, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		FixupBurningServerRagdoll( pRagdoll );
		RemoveDeferred();
		return true;
	}

	//Fix up the force applied to server side ragdolls. This fixes magnets not affecting them.
	CTakeDamageInfo newinfo = info;
	newinfo.SetDamageForce( forceVector );

	return BecomeRagdollOnClient( forceVector );
}

/*
============
Killed
============
*/
extern ConVar npc_vphysics;
void CBaseCombatCharacter::Event_Killed_Impl( const CTakeDamageInfo &info )
{

	// Advance life state to dying
	m_lifeState = LIFE_DYING;

	// Calculate death force
	Vector forceVector = CalcDeathForceVector( info );

	// See if there's a ragdoll magnet that should influence our force.
	CRagdollMagnet *pMagnet = CRagdollMagnet::FindBestMagnet( this );
	if( pMagnet )
	{
		forceVector += pMagnet->GetForceVector( this );
	}

	CBaseCombatWeapon *pDroppedWeapon = nullptr;

	if ( ShouldDropActiveWeaponWhenKilled() )
	{
		pDroppedWeapon = m_hActiveWeapon.Get();

		// Drop any weapon that I own
		if ( VPhysicsGetObject() )
		{
			Vector weaponForce = forceVector * VPhysicsGetObject()->GetInvMass();
			Weapon_Drop( m_hActiveWeapon, nullptr, &weaponForce );
		}
		else
		{
			Weapon_Drop( m_hActiveWeapon );
		}
	}
	
	// clear the deceased's sound channels.(may have been firing or reloading when killed)
	EmitSound( "BaseCombatCharacter.StopWeaponSounds" );

	// Tell my killer that he got me!
	if( info.GetAttacker() )
	{
		info.GetAttacker()->Event_KilledOther(this, info);
		g_EventQueue.AddEvent( info.GetAttacker(), "KilledNPC", 0.3, this, this );
	}
	SendOnKilledGameEvent( info );

	// Ragdoll unless we've gibbed
	if ( ShouldGib( info ) == false )
	{
		bool bRagdollCreated = false;
		if ( (info.GetDamageType() & DMG_DISSOLVE) && CanBecomeRagdoll() )
		{
			int nDissolveType = ENTITY_DISSOLVE_NORMAL;
			if ( info.GetDamageType() & DMG_SHOCK )
			{
				nDissolveType = ENTITY_DISSOLVE_ELECTRICAL;
			}

			bRagdollCreated = Dissolve( nullptr, gpGlobals->curtime, false, nDissolveType );

			// Also dissolve any weapons we dropped
			if ( pDroppedWeapon )
			{
				pDroppedWeapon->Dissolve( nullptr, gpGlobals->curtime, false, nDissolveType );
			}
		}

		if ( !bRagdollCreated && ( info.GetDamageType() & DMG_REMOVENORAGDOLL ) == 0 )
		{
			BecomeRagdoll( info, forceVector );
		}
	}
	
	// no longer standing on a nav area
	ClearLastKnownArea();
}

void CBaseCombatCharacter::Event_Dying( void )
{
}

// ===========================================================================
//  > Weapons
// ===========================================================================
bool CBaseCombatCharacter::Weapon_Detach( CBaseCombatWeapon *pWeapon )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( pWeapon == m_hMyWeapons[i] )
		{
			m_hMyWeapons.Set( i, INVALID_EHANDLE );

			int slotID = (i+1);
			for (int j = 0; j < std::size( m_weaponIDToIndex ); j++ )
			{
				if ( m_weaponIDToIndex[j] == slotID )
				{
					m_weaponIDToIndex[j] = 0;
					break;
				}
			}
			pWeapon->SetOwner( nullptr );

			if ( pWeapon == m_hActiveWeapon )
				ClearActiveWeapon();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ThrowDirForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, Vector *pVecThrowDir )
{
	// HACK! Always throw the physcannon directly in front of the player
	// This is necessary for the physgun upgrade scene.
	if ( FClassnameIs( pWeapon, "weapon_physcannon" ) )
	{
		if( hl2_episodic.GetBool() )
		{
			// It has been discovered that it's possible to throw the physcannon out of the world this way.
			// So try to find a direction to throw the physcannon that's legal.
			Vector vecOrigin = EyePosition();
			Vector vecRight;

			CrossProduct( vecForward, Vector( 0, 0, 1), vecRight );

			Vector vecTest[ 4 ];
			vecTest[0] = vecForward;
			vecTest[1] = -vecForward;
			vecTest[2] = vecRight;
			vecTest[3] = -vecRight;

			trace_t tr;
			int i;
			for( i = 0 ; i < 4 ; i++ )
			{
				UTIL_TraceLine( vecOrigin, vecOrigin + vecTest[ i ] * 48.0f, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

				if ( !tr.startsolid && tr.fraction == 1.0f )
				{
					*pVecThrowDir = vecTest[ i ];
					return;
				}
			}
		}

		// Well, fall through to what we did before we tried to make this a bit more robust.
		*pVecThrowDir = vecForward;
	}
	else
	{
		// Nowhere in particular; just drop it.
		VMatrix zRot;
		MatrixBuildRotateZ( zRot, random_valve->RandomFloat( -60.0f, 60.0f ) );

		Vector vecThrow;
		Vector3DMultiply( zRot, vecForward, *pVecThrowDir );

		pVecThrowDir->z = random_valve->RandomFloat( -0.5f, 0.5f );
		VectorNormalize( *pVecThrowDir );
	}
}

//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::DropWeaponForWeaponStrip( CBaseCombatWeapon *pWeapon, 
	const Vector &vecForward, const QAngle &vecAngles, float flDiameter )
{
	Vector vecOrigin;
	CollisionProp()->RandomPointInBounds( Vector( 0.5f, 0.5f, 0.5f ), Vector( 0.5f, 0.5f, 1.0f ), &vecOrigin );

	// Nowhere in particular; just drop it.
	Vector vecThrow;
	ThrowDirForWeaponStrip( pWeapon, vecForward, &vecThrow );

	Vector vecOffsetOrigin;
	VectorMA( vecOrigin, flDiameter, vecThrow, vecOffsetOrigin );

	trace_t	tr;
	UTIL_TraceLine( vecOrigin, vecOffsetOrigin, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		
	if ( tr.startsolid || tr.allsolid || ( tr.fraction < 1.0f && tr.Ent<CBaseEntity>() != pWeapon ) )
	{
		//FIXME: Throw towards a known safe spot?
		vecThrow.Negate();
		VectorMA( vecOrigin, flDiameter, vecThrow, vecOffsetOrigin );
	}

	vecThrow *= random_valve->RandomFloat( 400.0f, 600.0f );

	pWeapon->SetAbsOrigin( vecOrigin );
	pWeapon->SetAbsAngles( vecAngles );
	pWeapon->Drop( vecThrow );
	pWeapon->SetRemoveable( false );
	Weapon_Detach( pWeapon );
}

//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_DropAll( bool bDisallowWeaponPickup )
{
	if ( GetFlags() & FL_NPC )
	{
		for (int i=0; i<MAX_WEAPONS; ++i) 
		{
			CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
			if (!pWeapon)
				continue;

 			Weapon_Drop( pWeapon );
		}
		return;
	}

	QAngle gunAngles;
	VectorAngles( BodyDirection2D(), gunAngles );

	Vector vecForward;
	AngleVectors( gunAngles, &vecForward, nullptr, nullptr );

	float flDiameter = std::sqrt( CollisionProp()->OBBSize().x * CollisionProp()->OBBSize().x +
		CollisionProp()->OBBSize().y * CollisionProp()->OBBSize().y );

	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	for (int i=0; i<MAX_WEAPONS; ++i) 
	{
		CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
		if (!pWeapon)
			continue;

		// Have to drop this after we've dropped everything else, so autoswitch doesn't happen
		if ( pWeapon == pActiveWeapon )
			continue;

		DropWeaponForWeaponStrip( pWeapon, vecForward, gunAngles, flDiameter );

		// HACK: This hack is required to allow weapons to be disintegrated
		// in the citadel weapon-strip scene
		// Make them not pick-uppable again. This also has the effect of allowing weapons
		// to collide with triggers. 
		if ( bDisallowWeaponPickup )
		{
			pWeapon->RemoveSolidFlags( FSOLID_TRIGGER );
			
			IPhysicsObject *pObj = pWeapon->VPhysicsGetObject();
			
			if ( pObj != nullptr )
			{	
				pObj->SetGameFlags( FVPHYSICS_NO_PLAYER_PICKUP );
			}
		}
	}

	// Drop the active weapon normally...
	if ( pActiveWeapon )
	{
		// Nowhere in particular; just drop it.
		Vector vecThrow;
		ThrowDirForWeaponStrip( pActiveWeapon, vecForward, &vecThrow );

		// Throw a little more vigorously; it starts closer to the player
		vecThrow *= random_valve->RandomFloat( 800.0f, 1000.0f );

		Weapon_Drop( pActiveWeapon, nullptr, &vecThrow );
		pActiveWeapon->SetRemoveable( false );

		// HACK: This hack is required to allow weapons to be disintegrated
		// in the citadel weapon-strip scene
		// Make them not pick-uppable again. This also has the effect of allowing weapons
		// to collide with triggers. 
		if ( bDisallowWeaponPickup )
		{
			pActiveWeapon->RemoveSolidFlags( FSOLID_TRIGGER );
		}
	}
}

static constexpr auto TOSS_HEIGHT_MAX = 300; // altitude of initial trace done to see how high something can be tossed

// The velocity that tosses something from vecSpot1 to land at vecSpot2, or vec3_origin.
static Vector VecCheckToss( CBaseEntity *pEntity, ITraceFilter *pFilter, Vector vecSpot1, Vector vecSpot2, float flHeightMaxRatio, float flGravityAdj, bool bRandomize, Vector *vecMins = nullptr, Vector *vecMaxs = nullptr )
{
	trace_t			tr;
	Vector			vecMidPoint;// halfway point between Spot1 and Spot2
	Vector			vecApex;// highest point 
	Vector			vecScale;
	Vector			vecTossVel;
	Vector			vecTemp;
	float			flGravity = sv_gravity.GetFloat() * flGravityAdj;

	if (vecSpot2.z - vecSpot1.z > 500)
	{
		// to high, fail
		return vec3_origin;
	}

	Vector forward, right;
	AngleVectors( pEntity->GetLocalAngles(), &forward, &right, nullptr );

	if (bRandomize)
	{
		// toss a little bit to the left or right, not right down on the enemy's bean (head). 
		vecSpot2 += right * ( random_valve->RandomFloat(-8,8) + random_valve->RandomFloat(-16,16) );
		vecSpot2 += forward * ( random_valve->RandomFloat(-8,8) + random_valve->RandomFloat(-16,16) );
	}

	// calculate the midpoint and apex of the 'triangle'
	// UNDONE: normalize any Z position differences between spot1 and spot2 so that triangle is always RIGHT
	// get a rough idea of how high it can be thrown
	vecMidPoint = vecSpot1 + (vecSpot2 - vecSpot1) * 0.5;
	UTIL_TraceLine(vecMidPoint, vecMidPoint + Vector(0,0,TOSS_HEIGHT_MAX), MASK_SOLID_BRUSHONLY, pFilter, &tr);
	vecMidPoint = tr.endpos;

	if( tr.fraction != 1.0 )
	{
		// (subtract 15 so the object doesn't hit the ceiling)
		vecMidPoint.z -= 15;
	}

	if (flHeightMaxRatio != -1)
	{
		// But don't throw so high that it looks silly. Maximize the height of the
		// throw above the highest of the two endpoints to a ratio of the throw length.
		float flHeightMax = flHeightMaxRatio * (vecSpot2 - vecSpot1).Length();
		float flHighestEndZ = std::max(vecSpot1.z, vecSpot2.z);
		if ((vecMidPoint.z - flHighestEndZ) > flHeightMax)
		{
			vecMidPoint.z = flHighestEndZ + flHeightMax;
		}
	}

	if (vecMidPoint.z < vecSpot1.z || vecMidPoint.z < vecSpot2.z)
	{
		// Not enough space, fail
		return vec3_origin;
	}

	// How high should the object travel to reach the apex
	float distance1 = (vecMidPoint.z - vecSpot1.z);
	float distance2 = (vecMidPoint.z - vecSpot2.z);

	// How long will it take for the object to travel this distance
	float time1 = std::sqrt( distance1 / (0.5 * flGravity) );
	float time2 = sqrt( distance2 / (0.5 * flGravity) );

	if (time1 < 0.1)
	{
		// too close
		return vec3_origin;
	}

	// how hard to throw sideways to get there in time.
	vecTossVel = (vecSpot2 - vecSpot1) / (time1 + time2);

	// how hard upwards to reach the apex at the right time.
	vecTossVel.z = flGravity * time1;

	// find the apex
	vecApex  = vecSpot1 + vecTossVel * time1;
	vecApex.z = vecMidPoint.z;

	// JAY: Repro behavior from HL1 -- toss check went through gratings
	UTIL_TraceLine(vecSpot1, vecApex, (MASK_SOLID&(~CONTENTS_GRATE)), pFilter, &tr);
	if (tr.fraction != 1.0)
	{
		// fail!
		return vec3_origin;
	}

	// UNDONE: either ignore NPCs or change it to not care if we hit our enemy
	UTIL_TraceLine(vecSpot2, vecApex, (MASK_SOLID_BRUSHONLY&(~CONTENTS_GRATE)), pFilter, &tr); 
	if (tr.fraction != 1.0)
	{
		// fail!
		return vec3_origin;
	}

	if ( vecMins && vecMaxs )
	{
		// Check to ensure the entity's hull can travel the first half of the grenade throw
		UTIL_TraceHull( vecSpot1, vecApex, *vecMins, *vecMaxs, (MASK_SOLID&(~CONTENTS_GRATE)), pFilter, &tr);		
		if ( tr.fraction < 1.0 )
			return vec3_origin;
	}

	return vecTossVel;
}

static Vector VecCheckToss( CBaseEntity *pEntity, Vector vecSpot1, Vector vecSpot2, float flHeightMaxRatio, float flGravityAdj, bool bRandomize, Vector *vecMins = nullptr, Vector *vecMaxs = nullptr )
{
	// construct a filter and call through to the other version of this function.
	CTraceFilterSimple traceFilter( pEntity, COLLISION_GROUP_NONE );
	return VecCheckToss( pEntity, &traceFilter, vecSpot1, vecSpot2, 
						 flHeightMaxRatio, flGravityAdj, bRandomize, 
						 vecMins, vecMaxs );
}

//-----------------------------------------------------------------------------
// Purpose: Drop the active weapon, optionally throwing it at the given target position.
// Input  : pWeapon - Weapon to drop/throw.
//			pvecTarget - Position to throw it at, NULL for none.
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	if ( !pWeapon )
		return;

	// If I'm an NPC, fill the weapon with ammo before I drop it.
	if ( GetFlags() & FL_NPC )
	{
		if ( pWeapon->UsesClipsForAmmo1() )
		{
			pWeapon->m_iClip1 = pWeapon->GetDefaultClip1();

			if( FClassnameIs( pWeapon, "weapon_smg1" ) )
			{
				if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 ) )
				{
					// Drop enough ammo to kill 2 of me.
					// Figure out how much damage one piece of this type of ammo does to this type of enemy.
					float flAmmoDamage = g_pGameRules->GetAmmoDamage( UTIL_PlayerByIndex(1), this, pWeapon->GetPrimaryAmmoType() );
					pWeapon->m_iClip1 = (GetMaxHealth() / flAmmoDamage) * 2;
				}
				else
				{
					pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
				}
			}
		}
		if ( pWeapon->UsesClipsForAmmo2() )
		{
			pWeapon->m_iClip2 = pWeapon->GetDefaultClip2();
		}

	}

	if ( IsPlayer() )
	{
		Vector vThrowPos = Weapon_ShootPosition() - Vector(0,0,12);

		if( UTIL_PointContents(vThrowPos, CONTENTS_SOLID) & CONTENTS_SOLID )
		{
			Msg("Weapon spawning in solid!\n");
		}

		pWeapon->SetAbsOrigin( vThrowPos );

		QAngle gunAngles;
		VectorAngles( BodyDirection2D(), gunAngles );
		pWeapon->SetAbsAngles( gunAngles );
	}
	else
	{
		int iBIndex = -1;
		int iWeaponBoneIndex = -1;

		CStudioHdr *hdr = pWeapon->GetModelPtr();
		// If I have a hand, set the weapon position to my hand bone position.
		if ( hdr && hdr->numbones() > 0 )
		{
			// Assume bone zero is the root
			for ( iWeaponBoneIndex = 0; iWeaponBoneIndex < hdr->numbones(); ++iWeaponBoneIndex )
			{
				iBIndex = LookupBone( hdr->pBone( iWeaponBoneIndex )->pszName() );
				// Found one!
				if ( iBIndex != -1 )
				{
					break;
				}
			}

			if ( iBIndex == -1 )
			{
				iBIndex = LookupBone( "ValveBiped.weapon_bone" );
			}
		}
		else
		{
			iBIndex = LookupBone( "ValveBiped.weapon_bone" );
		}

		if ( iBIndex != -1)  
		{
			Vector origin;
			QAngle angles;
			matrix3x4_t transform;

			// Get the transform for the weapon bonetoworldspace in the NPC
			GetBoneTransform( iBIndex, transform );

			// find offset of root bone from origin in local space
			// Make sure we're detached from hierarchy before doing this!!!
			pWeapon->StopFollowingEntity();
			pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
			pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
			pWeapon->InvalidateBoneCache();
			matrix3x4_t rootLocal;
			pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );

			// invert it
			matrix3x4_t rootInvLocal;
			MatrixInvert( rootLocal, rootInvLocal );

			matrix3x4_t weaponMatrix;
			ConcatTransforms( transform, rootInvLocal, weaponMatrix );
			MatrixAngles( weaponMatrix, angles, origin );
			
			pWeapon->Teleport( &origin, &angles, nullptr );
		}
		// Otherwise just set in front of me.
		else 
		{
			Vector vFacingDir = BodyDirection2D();
			vFacingDir = vFacingDir * 10.0; 
			pWeapon->SetAbsOrigin( Weapon_ShootPosition() + vFacingDir );
		}
	}

	Vector vecThrow;
	if (pvecTarget)
	{
		// I've been told to throw it somewhere specific.
		vecThrow = VecCheckToss( this, pWeapon->GetAbsOrigin(), *pvecTarget, 0.2, 1.0, false );
	}
	else
	{
		if ( pVelocity )
		{
			vecThrow = *pVelocity;
			float flLen = vecThrow.Length();
			if (flLen > 400)
			{
				VectorNormalize(vecThrow);
				vecThrow *= 400;
			}
		}
		else
		{
			// Nowhere in particular; just drop it.
			float throwForce = ( IsPlayer() ) ? 400.0f : random_valve->RandomInt( 64, 128 );
			vecThrow = BodyDirection3D() * throwForce;
		}
	}

	pWeapon->Drop( vecThrow );
	Weapon_Detach( pWeapon );

}

//-----------------------------------------------------------------------------
// Lighting origin
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetLightingOriginRelative( CBaseEntity *pLightingOrigin )
{
	BaseClass::SetLightingOriginRelative( pLightingOrigin );
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->SetLightingOriginRelative( pLightingOrigin );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Add new weapon to the character
// Input  : New weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	// Add the weapon to my weapon inventory
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if (!m_hMyWeapons[i]) 
		{
			m_hMyWeapons.Set( i, pWeapon );
			m_weaponIDToIndex[pWeapon->GetWeaponID()] = (i+1);
			break;
		}
	}

	// Weapon is now on my team
	pWeapon->ChangeTeam( GetTeamNumber() );

	// ----------------------
	//  Give Primary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if ( !pWeapon->UsesClipsForAmmo1() )
	{
		{
			// non-clip, exhaustible ammo ( such as grenades ) is still held on player.
			CBaseCombatCharacter * pOwner = nullptr;

			if ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE )
			{
				pOwner = this;
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, pWeapon->GetDefaultClip1(), ShouldPickupItemSilently( this ), pOwner );
			}
		}
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
// 	else if (pWeapon->GetDefaultClip1() >  pWeapon->GetMaxClip1() )
// 	{
// 		pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
// 		pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, (pWeapon->GetDefaultClip1() - pWeapon->GetMaxClip1())); 
// 	}

	// ----------------------
	//  Give Secondary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if ( !pWeapon->UsesClipsForAmmo2() )
	{
		// non-clip, exhaustible ammo ( such as grenades ) is still held on player.
		CBaseCombatCharacter * pOwner = nullptr;

		if ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE )
		{
			pOwner = this;
			pWeapon->SetReserveAmmoCount( AMMO_POSITION_SECONDARY, pWeapon->GetDefaultClip2(), ShouldPickupItemSilently( this ), pOwner );
		}
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
// 	else if ( pWeapon->GetDefaultClip2() > pWeapon->GetMaxClip2() )
// 	{
// 		pWeapon->m_iClip2 = pWeapon->GetMaxClip2();
// 		pWeapon->SetReserveAmmoCount( AMMO_POSITION_SECONDARY, ( pWeapon->GetDefaultClip2() - pWeapon->GetMaxClip2() ) );
// 	}

	pWeapon->Equip( this );

	// Players don't automatically holster their current weapon
	if ( IsPlayer() == false )
	{
		if ( m_hActiveWeapon )
		{
			m_hActiveWeapon->Holster();
			// FIXME: isn't this handeled by the weapon?
			m_hActiveWeapon->AddEffects( EF_NODRAW );
		}
		SetActiveWeapon( pWeapon );
		m_hActiveWeapon->RemoveEffects( EF_NODRAW );

	}
	

	WeaponProficiency_t proficiency;
	proficiency = CalcWeaponProficiency( pWeapon );
	
	if( weapon_showproficiency.GetBool() != 0 )
	{
		Msg("%s equipped with %s, proficiency is %s\n", GetClassname(), pWeapon->GetClassname(), GetWeaponProficiencyName( proficiency ) );
	}

	SetCurrentWeaponProficiency( proficiency );

	// Pass the lighting origin over to the weapon if we have one
	pWeapon->SetLightingOriginRelative( GetLightingOriginRelative() );
}

//-----------------------------------------------------------------------------
// Purpose:	Leaves weapon, giving only ammo to the character
// Input  : Weapon
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_EquipAmmoOnly( CBaseCombatWeapon *pWeapon )
{
	// Check for duplicates
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs(m_hMyWeapons[i], pWeapon->GetClassname()) )
		{
			// Just give the ammo from the clip
			int	primaryGiven	= (pWeapon->UsesClipsForAmmo1()) ? pWeapon->m_iClip1 : pWeapon->GetPrimaryAmmoCount();
			int secondaryGiven	= (pWeapon->UsesClipsForAmmo2()) ? pWeapon->m_iClip2 : pWeapon->GetSecondaryAmmoCount();

			bool bSuppressSound = false;
			bSuppressSound = ShouldPickupItemSilently( this );
			CBaseCombatCharacter * pOwner = nullptr;

			if ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE )
				pOwner = this;

			int takenPrimary   = pWeapon->GiveReserveAmmo( AMMO_POSITION_PRIMARY, primaryGiven, bSuppressSound, pOwner ); 
			int takenSecondary = pWeapon->GiveReserveAmmo( AMMO_POSITION_SECONDARY, secondaryGiven, bSuppressSound, pOwner ); 
			
			if( pWeapon->UsesClipsForAmmo1() )
			{
				pWeapon->m_iClip1 -= takenPrimary;
			}
			else
			{
				pWeapon->SetPrimaryAmmoCount( pWeapon->GetPrimaryAmmoCount() - takenPrimary );
			}

			if( pWeapon->UsesClipsForAmmo2() )
			{
				pWeapon->m_iClip2 -= takenSecondary;
			}
			else
			{
				pWeapon->SetSecondaryAmmoCount( pWeapon->GetSecondaryAmmoCount() - takenSecondary );
			}
			
			//Only succeed if we've taken ammo from the weapon
			if ( takenPrimary > 0 || takenSecondary > 0 )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "ammo_pickup" );
				if( event )
				{
					const char *weaponName = pWeapon->GetClassname();
					if ( IsWeaponClassname( weaponName ) )
					{
						weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
					}
					event->SetInt( "userid", engine->GetPlayerUserId( edict() ) );
					event->SetString( "item", weaponName );
					event->SetInt( "index", m_hMyWeapons[i].Get()->entindex() );
					gameeventmanager->FireEvent( event );
				}
				return true;
			}
			
			return false;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the weapon passed in would occupy a slot already occupied by the carrier
// Input  : *pWeapon - weapon to test for
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_SlotOccupied( CBaseCombatWeapon *pWeapon )
{
	if ( pWeapon == nullptr )
		return false;

	//Check to see if there's a resident weapon already in this slot
	if ( Weapon_GetSlot( pWeapon->GetSlot() ) == nullptr )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to a weapon this character has that uses the specified ammo
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetWpnForAmmo( int iAmmoIndex )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoIndex )
			return weapon;
		if ( weapon->GetSecondaryAmmoType() == iAmmoIndex )
			return weapon;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Can this character operate this weapon?
// Input  : A weapon
// Output :	true or false
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanUse( CBaseCombatWeapon *pWeapon )
{
	acttable_t *pTable		= pWeapon->ActivityList();
	int			actCount	= pWeapon->ActivityListCount();

	if( actCount < 1 )
	{
		// If the weapon has no activity table, it definitely cannot be used.
		return false;
	}

	for ( int i = 0; i < actCount; i++, pTable++ )
	{
		if ( pTable->required )
		{
			// The NPC might translate the weapon activity into another activity
			Activity translatedActivity = NPC_TranslateActivity( (Activity)(pTable->weaponAct) );

			if ( SelectWeightedSequence(translatedActivity) == ACTIVITY_NOT_AVAILABLE )
			{
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_Create( const char *pWeaponName )
{
	CBaseCombatWeapon *pWeapon = static_cast<CBaseCombatWeapon *>( Create( pWeaponName, GetLocalOrigin(), GetLocalAngles(), this ) );

	return pWeapon;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_HandleAnimEvent( animevent_t *pEvent )
{
	// UNDONE: Some check to make sure that pEvent->pSource is a weapon I'm holding?
	if ( m_hActiveWeapon )
	{
		// UNDONE: Pass to pEvent->pSource instead?
		m_hActiveWeapon->Operator_HandleAnimEvent( pEvent, this );
	}
}

void CBaseCombatCharacter::RemoveAllWeapons()
{
	ClearActiveWeapon();
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( m_hMyWeapons[i] )
		{
			m_hMyWeapons[i]->Delete( );
			m_hMyWeapons.Set( i, INVALID_EHANDLE );
		}
	}
	V_memset( m_weaponIDToIndex, 0, sizeof(m_weaponIDToIndex) );
}

void CBaseCombatCharacter::RemoveAllWearables( void )
{
}

void CBaseCombatCharacter::RemoveWeaponOnPlayer( CBaseCombatWeapon *pWeapon )
{
	ClearActiveWeapon();
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( m_hMyWeapons[i].Get() == pWeapon )
		{
			m_hMyWeapons[i]->Delete( );
			m_hMyWeapons.Set( i, INVALID_EHANDLE );
		}
	}
	V_memset( m_weaponIDToIndex, 0, sizeof(m_weaponIDToIndex) );
}

// take health
int CBaseCombatCharacter::TakeHealth_Impl (float flHealth, int bitsDamageType)
{
	if (!m_takedamage)
		return 0;
	
	return BaseClass::TakeHealth_Impl(flHealth, bitsDamageType);
}

/*
============
OnTakeDamage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
bitsDamageType indicates the type of damage sustained, ie: DMG_SHOCK

Time-based damage: only occurs while the NPC is within the trigger_hurt.
When a NPC is poisoned via an arrow etc it takes all the poison damage at once.

GLOBALS ASSUMED SET:  g_iSkillLevel
============
*/
int CBaseCombatCharacter::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	int retVal = 0;

	if (!m_takedamage)
		return 0;

	if( IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer *>(this);
		pPlayer->RumbleEffect( RUMBLE_DMG_LOW, 0, RUMBLE_FLAG_RESTART ); 
	}

	m_iDamageCount++;

	m_uiLastDamageTypeFlags = info.GetDamageType();

	if ( info.GetDamageType() & DMG_SHOCK )
	{
		g_pEffects->Sparks( info.GetDamagePosition(), 2, 2 );
		UTIL_Smoke( info.GetDamagePosition(), random_valve->RandomInt( 10, 15 ), 10 );
	}

	// track damage history
	if ( info.GetAttacker() )
	{
		int attackerTeam = info.GetAttacker()->GetTeamNumber();

		m_hasBeenInjured |= ( 1 << attackerTeam );

		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team == attackerTeam )
			{
				// restart the injury timer
				m_damageHistory[i].interval.Start();
				break;
			}

			if ( m_damageHistory[i].team == TEAM_INVALID )
			{
				// team not registered yet
				m_damageHistory[i].team = attackerTeam;
				m_damageHistory[i].interval.Start();
				break;
			}
		}

		m_flTimeOfLastInjury = gpGlobals->curtime;

		// Figure out what relative direction it hit from
		Vector vDamageDirection = info.GetDamageForce();
		VectorNormalize( vDamageDirection );

		Vector vForward, vRight;
		if( IsPlayer() )
		{
			AngleVectors( EyeAngles(), &vForward, &vRight, nullptr );
		}
		else
		{
			GetVectors( &vForward, &vRight, nullptr );
		}

		// Try front and back
		float flDamageDirectionDot = vForward.Dot( vDamageDirection );

		if ( flDamageDirectionDot <= -0.5f )
		{
			m_nRelativeDirectionOfLastInjury = DAMAGED_DIR_FRONT;
		}
		else if ( flDamageDirectionDot >= 0.5f )
		{
			m_nRelativeDirectionOfLastInjury = DAMAGED_DIR_BACK;
		}
		else
		{
			// Try left and right sides
			float flDamageDirectionDot = vRight.Dot( vDamageDirection );

			if ( flDamageDirectionDot <= -0.5f )
			{
				m_nRelativeDirectionOfLastInjury = DAMAGED_DIR_RIGHT;
			}
			else if ( flDamageDirectionDot >= 0.5f )
			{
				m_nRelativeDirectionOfLastInjury = DAMAGED_DIR_LEFT;
			}
			else
			{
				m_nRelativeDirectionOfLastInjury = DAMAGED_DIR_NONE;
			}
		}
	}

	switch( m_lifeState )
	{
	case LIFE_ALIVE:
		retVal = OnTakeDamage_Alive( info );
		if ( m_iHealth <= 0 )
		{
			IPhysicsObject *pPhysics = VPhysicsGetObject();
			if ( pPhysics )
			{
				pPhysics->EnableCollisions( false );
			}
			
			bool bGibbed = false;

			Event_Killed( info );

			// Only classes that specifically request it are gibbed
			if ( ShouldGib( info ) )
			{
				bGibbed = Event_Gibbed( info );
			}
			
			if ( bGibbed == false )
			{
				Event_Dying();
			}
		}
		return retVal;
		break;

	case LIFE_DYING:
		return OnTakeDamage_Dying( info );
	
	default:
	case LIFE_DEAD:
		retVal = OnTakeDamage_Dead( info );
		if ( m_iHealth <= 0 && g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && ShouldGib( info ) )
		{
			Event_Gibbed( info );
			retVal = 0;
		}
		return retVal;
	}
}

int CBaseCombatCharacter::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( GetFlags() & FL_GODMODE )
		return 0;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	Vector vecDir = vec3_origin;
	if (info.GetInflictor())
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize(vecDir);
	}
	g_vecAttackDir = vecDir;

	//!!!LATER - make armor consideration here!
	// do the damage
	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		// Separate the fractional amount of damage from the whole
		float flFractionalDamage = info.GetDamage() - std::floor( info.GetDamage() );
		float flIntegerDamage = info.GetDamage() - flFractionalDamage;

		// Add fractional damage to the accumulator
		m_flDamageAccumulator += flFractionalDamage;

		// If the accumulator is holding a full point of damage, move that point
		// of damage into the damage we're about to inflict.
		if( m_flDamageAccumulator >= 1.0 )
		{
			flIntegerDamage += 1.0;
			m_flDamageAccumulator -= 1.0;
		}

		if ( flIntegerDamage <= 0 )
			return 0;

		SetHealth( m_iHealth - flIntegerDamage );
	}

	return 1;
}

int CBaseCombatCharacter::OnTakeDamage_Dying( const CTakeDamageInfo &info )
{
	return 1;
}

int CBaseCombatCharacter::OnTakeDamage_Dead( const CTakeDamageInfo &info )
{
	// do the damage
	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		m_iHealth -= info.GetDamage();
	}

	if( IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer *>(this);
		pPlayer->RumbleEffect( RUMBLE_DMG_HIGH, 0, RUMBLE_FLAG_RESTART ); 
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Sets vBodyDir to the body direction (2D) of the combat character.  
//			Used as NPC's and players extract facing direction differently
// Input  :
// Output :
//-----------------------------------------------------------------------------
QAngle CBaseCombatCharacter::BodyAngles()
{
	return GetAbsAngles();
}

Vector CBaseCombatCharacter::BodyDirection2D( void )
{
	Vector vBodyDir = BodyDirection3D( );
	vBodyDir.z = 0;
	vBodyDir.AsVector2D().NormalizeInPlace();
	return vBodyDir;
}

Vector CBaseCombatCharacter::BodyDirection3D( void )
{
	QAngle angles = BodyAngles();

	// FIXME: cache this
	Vector vBodyDir;
	AngleVectors( angles, &vBodyDir );
	return vBodyDir;
}

void CBaseCombatCharacter::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Skip this work if we're already marked for transmission.
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );

	bool bLocalPlayer = ( pInfo->m_pClientEnt == edict() );
	if ( IsPlayer() && !bLocalPlayer )
	{
		if ( ToBasePlayer( this )->IsSplitScreenUserOnEdict( pInfo->m_pClientEnt ) )
			bLocalPlayer = true;
	}

	if ( bLocalPlayer )
	{
		for ( int i=0; i < MAX_WEAPONS; i++ )
		{
			CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
			if ( !pWeapon )
				continue;

			// The local player is sent all of his weapons.
			pWeapon->SetTransmit( pInfo, bAlways );
		}
	}
	else
	{
		// The check for EF_NODRAW is useless because the weapon will be networked anyway. In CBaseCombatWeapon::
		// UpdateTransmitState all weapons with owners will transmit to clients in the PVS.
		if ( m_hActiveWeapon && !m_hActiveWeapon->IsEffectActive( EF_NODRAW ) )
			m_hActiveWeapon->SetTransmit( pInfo, bAlways );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fetch the default team relationship
// Input  :
// Output :
//-----------------------------------------------------------------------------
Disposition_t CBaseCombatCharacter::GetFactionRelationshipDisposition( int nFaction )
{
	contract_assert( m_FactionRelationship != nullptr );

	return m_FactionRelationship[ GetFaction() ][ nFaction ].disposition;
}

//-----------------------------------------------------------------------------
// Purpose: Add or Change a class relationship for this entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddClassRelationship ( Class_T class_type, Disposition_t disposition, int priority )
{
	// First check to see if a relationship has already been declared for this class
	// If so, update it with the new relationship
	for (int i=std::ssize( m_Relationship )-1;i >= 0;i--) 
	{
		if (m_Relationship[i].classType == class_type) 
		{
			m_Relationship[i].disposition = disposition;
			if ( priority != DEF_RELATIONSHIP_PRIORITY )
				m_Relationship[i].priority	  = priority;
			return;
		}
	}

	int index = std::ssize( m_Relationship ); m_Relationship.emplace_back();
	// Add the new class relationship to our relationship table
	m_Relationship[index].classType		= class_type;
	m_Relationship[index].entity		= nullptr;
	m_Relationship[index].faction		= FACTION_NONE;
	m_Relationship[index].disposition	= disposition;
	m_Relationship[index].priority		= ( priority != DEF_RELATIONSHIP_PRIORITY ) ? priority : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Add or Change a entity relationship for this entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddEntityRelationship ( CBaseEntity* pEntity, Disposition_t disposition, int priority )
{
	// First check to see if a relationship has already been declared for this entity
	// If so, update it with the new relationship
	for (int i=std::ssize( m_Relationship )-1;i >= 0;i--) 
	{
		if (m_Relationship[i].entity == pEntity) 
		{
			m_Relationship[i].disposition	= disposition;
			if ( priority != DEF_RELATIONSHIP_PRIORITY )
				m_Relationship[i].priority	= priority;
			return;
		}
	}

	int index = std::ssize( m_Relationship ); m_Relationship.emplace_back();
	// Add the new class relationship to our relationship table
	m_Relationship[index].classType		= CLASS_NONE;
	m_Relationship[index].entity		= pEntity;
	m_Relationship[index].faction		= FACTION_NONE;
	m_Relationship[index].disposition	= disposition;
	m_Relationship[index].priority		= ( priority != DEF_RELATIONSHIP_PRIORITY ) ? priority : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Removes an entity relationship from our list
// Input  : *pEntity - Entity with whom the relationship should be ended
// Output : True is entity was removed, false if it was not found
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::RemoveEntityRelationship( CBaseEntity *pEntity )
{
	// Find the entity in our list, if it exists
	for ( int i = std::ssize( m_Relationship )-1; i >= 0; i-- ) 
	{
		if ( m_Relationship[i].entity == pEntity )
		{
			// Done, remove it
			m_Relationship.erase( m_Relationship.begin() + i );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddFactionRelationship(int nFaction, Disposition_t disposition, int priority )
{
	// First check to see if a relationship has already been declared for this faction
	// If so, update it with the new relationship
	for (int i=std::ssize( m_Relationship )-1;i >= 0;i--) 
	{
		if (m_Relationship[i].faction == nFaction) 
		{
			m_Relationship[i].disposition	= disposition;
			if ( priority != DEF_RELATIONSHIP_PRIORITY )
				m_Relationship[i].priority	= priority;
			return;
		}
	}

	int index = std::ssize( m_Relationship ); m_Relationship.emplace_back();
	// Add the new class relationship to our relationship table
	m_Relationship[index].classType		= CLASS_NONE;
	m_Relationship[index].entity		= nullptr;
	m_Relationship[index].faction		= nFaction;
	m_Relationship[index].disposition	= disposition;
	m_Relationship[index].priority		= ( priority != DEF_RELATIONSHIP_PRIORITY ) ? priority : 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ChangeFaction( int nNewFaction ) {
	int nOldFaction = m_nFaction;

	if ( ( m_nFaction != FACTION_NONE ) && ( !m_aFactions.empty() ) )
	{
		ks::swap_erase_first( m_aFactions[ m_nFaction ], this );
	}

	m_nFaction = nNewFaction;

	if ( m_nFaction != FACTION_NONE )
	{
		if ( m_aFactions.empty() )
		{
			AllocateDefaultFactionRelationships();
		}

		m_aFactions[ m_nFaction ].push_back( this );
	}

	// remove any relationship to entities where the relationship may change due to the faction change
	if ( ( m_FactionRelationship ) && ( m_nFaction != FACTION_NONE ) )
	{
		for(int i = 0; i < std::ssize( m_Relationship ); i++ )
		{
			
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetNumFactions( void ) {
	if ( m_aFactions.empty() )
	{
		AllocateDefaultFactionRelationships();
	}

	return std::ssize( m_aFactions );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
std::vector< EHANDLE > *CBaseCombatCharacter::GetEntitiesInFaction( int nFaction ) {
	if ( m_aFactions.empty() )
	{
		return nullptr;
	}

	return &m_aFactions[ nFaction ];
}

//-----------------------------------------------------------------------------
// Allocates default relationships
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AllocateDefaultRelationships( )
{
	if (!m_DefaultRelationship)
	{
		int iNumClasses = GameRules() ? GameRules()->NumEntityClasses() : LAST_SHARED_ENTITY_CLASS;
		m_DefaultRelationship = new Relationship_t*[iNumClasses];

		for (int i=0; i<iNumClasses; ++i)
		{
			// Be default all relationships are neutral of priority zero
			m_DefaultRelationship[i] = new Relationship_t[iNumClasses];
		}
	}
}

//-----------------------------------------------------------------------------
// Allocates default faction relationships
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AllocateDefaultFactionRelationships( )
{
	if (!m_FactionRelationship)
	{
		int nNumFactions = GameRules() ? GameRules()->NumFactions() : NUM_SHARED_FACTIONS;
		m_aFactions.assign( nNumFactions, {} );
		m_FactionRelationship = new Relationship_t*[nNumFactions];

		for (int i=0; i<nNumFactions; ++i)
		{
			// Be default all relationships are neutral of priority zero
			m_FactionRelationship[i] = new Relationship_t[nNumFactions];
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetDefaultRelationship(Class_T nClass, Class_T nClassTarget, Disposition_t nDisposition, int nPriority)
{
	if (m_DefaultRelationship)
	{
		m_DefaultRelationship[nClass][nClassTarget].disposition	= nDisposition;
		m_DefaultRelationship[nClass][nClassTarget].priority	= nPriority;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetDefaultFactionRelationship(int nFaction, int nFactionTarget, Disposition_t nDisposition, int nPriority)
{
	if (!m_FactionRelationship)
	{
		AllocateDefaultFactionRelationships();
	}

	m_FactionRelationship[nFaction][nFactionTarget].disposition	= nDisposition;
	m_FactionRelationship[nFaction][nFactionTarget].priority	= nPriority;
}

//-----------------------------------------------------------------------------
// Purpose: Fetch the default (ignore ai_relationship changes) relationship
// Input  :
// Output :
//-----------------------------------------------------------------------------
Disposition_t CBaseCombatCharacter::GetDefaultRelationshipDisposition( Class_T nClassTarget )
{
	contract_assert( m_DefaultRelationship != nullptr );

	return m_DefaultRelationship[Classify()][nClassTarget].disposition;
}

//-----------------------------------------------------------------------------
// Purpose: describes the relationship between two types of NPC.
// Input  :
// Output :
//-----------------------------------------------------------------------------
Relationship_t *CBaseCombatCharacter::FindEntityRelationship( CBaseEntity *pTarget )
{
	if ( !pTarget )
	{
		static Relationship_t dummy; 
		return &dummy;
	}

	// First check for specific relationship with this edict
	int i;
	for (i=0;i<std::ssize( m_Relationship );i++) 
	{
		if (pTarget == (CBaseEntity *)m_Relationship[i].entity) 
		{
			return &m_Relationship[i];
		}
	}

	if (pTarget->Classify() != CLASS_NONE)
	{
		// Then check for relationship with this edict's class
		for (i=0;i<std::ssize( m_Relationship );i++) 
		{
			if (pTarget->Classify() == m_Relationship[i].classType) 
			{
				return &m_Relationship[i];
			}
		}
	}
	
	CBaseCombatCharacter *pBaseCombatCharacter = ToBaseCombatCharacter( pTarget );
	if (pBaseCombatCharacter)
	{
		int nFaction = pBaseCombatCharacter->GetFaction();
		if ( nFaction != FACTION_NONE )
		{
			// Then check for relationship with this edict's faction
			for (i=0;i<std::ssize( m_Relationship );i++) 
			{
				if (nFaction == m_Relationship[i].faction) 
				{
					return &m_Relationship[i];
				}
			}

			if ( ( m_FactionRelationship ) && ( GetFaction() != FACTION_NONE ) )
			{
				return &m_FactionRelationship[ GetFaction() ][ nFaction ];
			}
		}
	}

	AllocateDefaultRelationships();
	// If none found return the default
	return &m_DefaultRelationship[ Classify() ][ pTarget->Classify() ];
}

Disposition_t CBaseCombatCharacter::IRelationType ( CBaseEntity *pTarget )
{
	if ( pTarget )
		return FindEntityRelationship( pTarget )->disposition;
	return D_NU;
}

//-----------------------------------------------------------------------------
// Purpose: describes the relationship between two types of NPC.
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::IRelationPriority( CBaseEntity *pTarget )
{
	if ( pTarget )
		return FindEntityRelationship( pTarget )->priority;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at current position/orientation
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CBaseCombatCharacter::Weapon_ShootPosition( )
{
	Vector forward, right, up;

	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	Vector vecSrc = GetAbsOrigin() 
					+ forward * m_HackedGunPos.y 
					+ right * m_HackedGunPos.x 
					+ up * m_HackedGunPos.z;

	return vecSrc;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::FindHealthItem( const Vector &vecPosition, const Vector &range )
{
	CBaseEntity *list[1024];
	int count = UTIL_EntitiesInBox( list, 1024, vecPosition - range, vecPosition + range, 0 );

	for ( int i = 0; i < count; i++ )
	{
		CItem *pItem = dynamic_cast<CItem *>(list[ i ]);

		if( pItem )
		{
			// Healthkits and healthvials
			if( pItem->ClassMatches( "item_health*" ) && FVisible( pItem ) )
			{
				return pItem;
			}
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Compares the weapon's center with this character's current origin, so it
// will not give reliable results for weapons that are visible to the NPC
// but are upstairs/downstairs, etc.
//
// A weapon is said to be on the ground if it is no more than 12 inches above
// or below the caller's feet.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_IsOnGround( CBaseCombatWeapon *pWeapon )
{
	if( pWeapon->IsConstrained() )
	{
		// Constrained to a rack.
		return false;
	}

	if( fabs(pWeapon->WorldSpaceCenter().z - GetAbsOrigin().z) >= 12.0f )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &range - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::Weapon_FindUsable( const Vector &range )
{
	bool bConservative = false;

	CBaseCombatWeapon *weaponList[64];
	CBaseCombatWeapon *pBestWeapon = nullptr;

	Vector mins = GetAbsOrigin() - range;
	Vector maxs = GetAbsOrigin() + range;
	int listCount = CBaseCombatWeapon::GetAvailableWeaponsInBox( weaponList, std::size( weaponList ), mins, maxs );

	float fBestDist = 1e6;

	for ( int i = 0; i < listCount; i++ )
	{
		// Make sure not moving (ie flying through the air)
		Vector velocity;

		CBaseCombatWeapon *pWeapon = weaponList[i];
		contract_assert( pWeapon );
		pWeapon->GetVelocity( &velocity, nullptr );

		if ( pWeapon->CanBePickedUpByNPCs() == false )
			continue;

		if ( velocity.LengthSqr() > 1 || !Weapon_CanUse(pWeapon) )
			continue;

		if ( pWeapon->IsLocked(this) )
			continue;

		if ( GetActiveWeapon() )
		{
			// Already armed. Would picking up this weapon improve my situation?
			if( GetActiveWeapon()->m_iClassname == pWeapon->m_iClassname )
			{
				// No, I'm already using this type of weapon.
				continue;
			}

			if( FClassnameIs( pWeapon, "weapon_pistol" ) )
			{
				// No, it's a pistol.
				continue;
			}
		}

		float fCurDist = (pWeapon->GetLocalOrigin() - GetLocalOrigin()).Length();

		// Give any reserved weapon a bonus
		if( pWeapon->HasSpawnFlags( SF_WEAPON_NO_PLAYER_PICKUP ) )
		{
			fCurDist *= 0.5f;
		}

		if ( pBestWeapon )
		{
			// UNDONE: Better heuristic needed here
			//			Need to pick by power of weapons
			//			Don't want to pick a weapon right next to a NPC!

			// Give the AR2 a bonus to be selected by making it seem closer.
			if( FClassnameIs( pWeapon, "weapon_ar2" ) )
			{
				fCurDist *= 0.5;
			}

			// choose the last range attack weapon you find or the first available other weapon
			if ( ! (pWeapon->CapabilitiesGet() & bits_CAP_RANGE_ATTACK_GROUP) )
			{
				continue;
			}
			else if (fCurDist > fBestDist ) 
			{
				continue;
			}
		}

		if( Weapon_IsOnGround(pWeapon) )
		{
			// Weapon appears to be lying on the ground. Make sure this weapon is reachable
			// by tracing out a human sized hull just above the weapon.  If not, reject
			trace_t tr;

			Vector	vAboveWeapon = pWeapon->GetAbsOrigin();
			UTIL_TraceEntity( this, vAboveWeapon, vAboveWeapon + Vector( 0, 0, 1 ), MASK_SOLID, pWeapon, COLLISION_GROUP_NONE, &tr );

			if ( tr.startsolid || (tr.fraction < 1.0) )
				continue;
		}
		else if( bConservative )
		{
			// Skip it.
			continue;
		}

		if( FVisible(pWeapon) )
		{
			fBestDist   = fCurDist;
			pBestWeapon = pWeapon;
		}
	}

	if( pBestWeapon )
	{
		// Lock this weapon for my exclusive use. Lock it for just a couple of seconds because my AI 
		// might not actually be able to go pick it up right now.
		pBestWeapon->Lock( 2.0, this );
	}

	return pBestWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
// Input  : iCount - Amount of ammo to give.
//			iAmmoIndex - Index of the ammo into the AmmoInfoArray
//			iMax - Max carrying capability of the player
// Output : Amount of ammo actually given
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound)
{
	if (iCount <= 0)
		return 0;

	if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	{
		// game rules say I can't have any more of this ammo type.
		return 0;
	}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return 0;

	int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex, this);
	int iAdd = std::min( iCount, iMax - m_iAmmo[iAmmoIndex] );
	if ( iAdd < 1 )
		return 0;

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	m_iAmmo.Set( iAmmoIndex, m_iAmmo[iAmmoIndex] + iAdd );

	return iAdd;
}

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GiveAmmo( int iCount, const char *szName, bool bSuppressSound )
{
	int iAmmoType = GetAmmoDef()->Index(szName);
	if (iAmmoType == -1)
	{
		Msg("ERROR: Attempting to give unknown ammo type (%s)\n",szName);
		return 0;
	}
	return GiveAmmo( iCount, iAmmoType, bSuppressSound );
}

ConVar	phys_stressbodyweights( side_registry(), "phys_stressbodyweights", "5.0" );
// disabled stress damage to save CPU, none of the NPCs really use it and the player has a different codepath
void CBaseCombatCharacter::VPhysicsUpdate_Impl( IPhysicsObject *pPhysics )
{
	ApplyStressDamage( pPhysics, false );
	BaseClass::VPhysicsUpdate_Impl( pPhysics );
}

float CBaseCombatCharacter::CalculatePhysicsStressDamage( vphysics_objectstress_t *pStressOut, IPhysicsObject *pPhysics )
{
	// stress damage hack.
	float mass = pPhysics->GetMass();
	CalculateObjectStress( pPhysics, this, pStressOut );
	float stress = (pStressOut->receivedStress * m_impactEnergyScale) / mass;

	// Make sure the stress isn't from being stuck inside some static object.
	// how many times your own weight can you hold up?
	if ( pStressOut->hasNonStaticStress && stress > phys_stressbodyweights.GetFloat() )
	{
		// if stuck, don't do this!
		if ( !(pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING) )
			return 200;
	}

	return 0;
}

void CBaseCombatCharacter::ApplyStressDamage( IPhysicsObject *pPhysics, bool bRequireLargeObject )
{

	vphysics_objectstress_t stressOut;
	float damage = CalculatePhysicsStressDamage( &stressOut, pPhysics );
	if ( damage > 0 )
	{
		if ( bRequireLargeObject && !stressOut.hasLargeObjectContact )
			return;

		//Msg("Stress! %.2f / %.2f\n", stressOut.exertedStress, stressOut.receivedStress );
		CTakeDamageInfo dmgInfo( GetWorldEntity(), GetWorldEntity(), vec3_origin, vec3_origin, damage, DMG_CRUSH );
		dmgInfo.SetDamageForce( Vector( 0, 0, -stressOut.receivedStress * sv_gravity.GetFloat() * gpGlobals->frametime ) );
		dmgInfo.SetDamagePosition( GetAbsOrigin() );
		TakeDamage( dmgInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const impactdamagetable_t
//-----------------------------------------------------------------------------
const impactdamagetable_t &CBaseCombatCharacter::GetPhysicsImpactDamageTable( void )
{
	return gDefaultNPCImpactDamageTable;
}

// how much to amplify impact forces
// This is to account for the ragdolls responding differently than
// the shadow objects.  Also this makes the impacts more dramatic.
ConVar	phys_impactforcescale( side_registry(), "phys_impactforcescale", "1.0" ); 
ConVar	phys_upimpactforcescale( side_registry(), "phys_upimpactforcescale", "0.375" ); 

void CBaseCombatCharacter::VPhysicsShadowCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];
	IPhysicsObject *pOtherPhysics = pEvent->pObjects[otherIndex];
	if ( !pOther )
		return;

	// Ragdolls are marked as dying.
	if ( pOther->m_lifeState == LIFE_DYING )
		return;

	if ( pOther->GetMoveType() != MOVETYPE_VPHYSICS )
		return;
	
	if ( !pOtherPhysics->IsMoveable() )
		return;
	
	if ( pOther == GetGroundEntity() )
		return;

	// Player can't damage himself if he's was physics attacker *on this frame*
	// which can occur owing to ordering issues it appears.
	float flOtherAttackerTime = 0.0f;

	if ( this == pOther->HasPhysicsAttacker( flOtherAttackerTime ) )
		return;

	int damageType = 0;
	float damage = 0;

	damage = CalculatePhysicsImpactDamage( index, pEvent, GetPhysicsImpactDamageTable(), m_impactEnergyScale, false, damageType );
	
	if ( damage <= 0 )
		return;
	
	// NOTE: We really need some rotational motion for some of these collisions.
	// REVISIT: Maybe resolve this collision on death with a different (not approximately infinite like AABB tensor)
	// inertia tensor to get torque?
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass() * phys_impactforcescale.GetFloat();
	
	

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	CTakeDamageInfo dmgInfo( pOther, pOther, damageForce, damagePos, damage, damageType );

	// FIXME: is there a better way for physics objects to keep track of what root entity responsible for them moving?
	CBasePlayer *pPlayer = pOther->HasPhysicsAttacker( 1.0 );
	if (pPlayer)
	{
		dmgInfo.SetAttacker( pPlayer );
	}

	// UNDONE: Find one near damagePos?
	m_nForceBone = 0;
	PhysCallbackDamage( this, dmgInfo, *pEvent, index );
}

//-----------------------------------------------------------------------------
// Purpose: this entity is exploding, or otherwise needs to inflict damage upon 
//			entities within a certain range.  only damage ents that can clearly 
//			be seen by the explosion!
// Input  :
// Output :
//-----------------------------------------------------------------------------	
void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	// NOTE: I did this this way so I wouldn't have to change a whole bunch of
	// code unnecessarily. We need TF2 specific rules for RadiusDamage, so I moved
	// the implementation of radius damage into gamerules. All existing code calls
	// this method, which calls the game rules method
	g_pGameRules->RadiusDamage( info, vecSrc, flRadius, iClassIgnore, pEntityIgnore );

	// Let the world know if this was an explosion.
	if( info.GetDamageType() & DMG_BLAST )
	{
		// Even the tiniest explosion gets attention. Don't let the radius
		// be less than 128 units.
		float soundRadius = std::max<std::common_type_t<float, double>>( 128.0f, flRadius * 1.5 );

		CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_EXPLOSION, vecSrc, soundRadius, 0.25, info.GetInflictor() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change active weapon and notify derived classes
//			
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetActiveWeapon( CBaseCombatWeapon *pNewWeapon )
{
	CBaseCombatWeapon *pOldWeapon = m_hActiveWeapon;
	if ( pNewWeapon != pOldWeapon )
	{
		m_hActiveWeapon = pNewWeapon;
		OnChangeActiveWeapon( pOldWeapon, pNewWeapon );
	}
}

//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CBaseCombatCharacter::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread(GetCurrentWeaponProficiency());
	return VECTOR_CONE_15DEGREES;
}

//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetSpreadBias( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetSpreadBias(GetCurrentWeaponProficiency());
	return 1.0;
}

//-----------------------------------------------------------------------------
// Assume everyone is average with every weapon. Override this to make exceptions.
//-----------------------------------------------------------------------------
WeaponProficiency_t CBaseCombatCharacter::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	return WEAPON_PROFICIENCY_AVERAGE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static constexpr auto MAX_MISS_CANDIDATES = 16;
CBaseEntity *CBaseCombatCharacter::FindMissTarget( void )
{
	CBaseEntity *pMissCandidates[ MAX_MISS_CANDIDATES ];
	int numMissCandidates = 0;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	CBaseEntity *pEnts[256];
	Vector		radius( 100, 100, 100);
	Vector		vecSource = GetAbsOrigin();

	int numEnts = UTIL_EntitiesInBox( pEnts, 256, vecSource-radius, vecSource+radius, 0 );

	for ( int i = 0; i < numEnts; i++ )
	{
		if ( pEnts[i] == nullptr )
			continue;

		// New rule for this system. Don't shoot what the player won't see.
		if ( pPlayer && !pPlayer->FInViewCone( pEnts[ i ] ) )
			continue;

		if ( numMissCandidates >= MAX_MISS_CANDIDATES )
			break;

		//See if it's a good target candidate
		if ( FClassnameIs( pEnts[i], "prop_dynamic" ) || 
			 FClassnameIs( pEnts[i], "prop_physics" ) || 
			 FClassnameIs( pEnts[i], "physics_prop" ) )
		{
			pMissCandidates[numMissCandidates++] = pEnts[i];
			continue;
		}
	}

	if( numMissCandidates == 0 )
		return nullptr;

	return pMissCandidates[ random_valve->RandomInt( 0, numMissCandidates - 1 ) ];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::ShouldShootMissTarget( CBaseCombatCharacter *pAttacker )
{
	// Don't shoot at NPC's right now.
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputKilledNPC( inputdata_t &inputdata )
{
	OnKilledNPC( inputdata.pActivator ? inputdata.pActivator->MyCombatCharacterPointer() : nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: Overload our muzzle flash and send it to any actively held weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::DoMuzzleFlash()
{
	// Our weapon takes our muzzle flash command
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

//-----------------------------------------------------------------------------
// Purpose: track the last trigger_fog touched by this character
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::OnFogTriggerStartTouch( CBaseEntity *fogTrigger )
{
	m_hTriggerFogList.insert( m_hTriggerFogList.begin(), fogTrigger );
}

//-----------------------------------------------------------------------------
// Purpose: track the last trigger_fog touched by this character
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::OnFogTriggerEndTouch( CBaseEntity *fogTrigger )
{
	ks::erase_first( m_hTriggerFogList, fogTrigger );
}

//-----------------------------------------------------------------------------
// Purpose: track the last trigger_fog touched by this character
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::GetFogTrigger( void )
{
	float bestDist = 999999.0f;
	CBaseEntity *bestTrigger = nullptr;

	for ( int i=0; i<std::ssize( m_hTriggerFogList ); ++i )
	{
		CBaseEntity *fogTrigger = m_hTriggerFogList[i];
		if ( fogTrigger != nullptr )
		{
			float dist = WorldSpaceCenter().DistTo( fogTrigger->WorldSpaceCenter() );
			if ( dist < bestDist )
			{
				bestDist = dist;
				bestTrigger = fogTrigger;
			}
		}
	}

	if ( bestTrigger )
	{
		m_hLastFogTrigger = bestTrigger;
	}

	return m_hLastFogTrigger;
}

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( const Vector &target ) const
{
	float range = EyePosition().DistTo( target );
	return IsHiddenByFog( range );
}

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( CBaseEntity *target ) const
{
	if ( !target )
		return false;

	float range = EyePosition().DistTo( target->WorldSpaceCenter() );
	return IsHiddenByFog( range );
}

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( float range ) const
{
	if ( GetFogObscuredRatio( range ) >= 1.0f )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( const Vector &target ) const
{
	float range = EyePosition().DistTo( target );
	return GetFogObscuredRatio( range );
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( CBaseEntity *target ) const
{
	if ( !target )
		return false;

	float range = EyePosition().DistTo( target->WorldSpaceCenter() );
	return GetFogObscuredRatio( range );
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( float range ) const
{
	fogparams_t fog;
	GetFogParams( &fog );

	if ( !fog.enable )
		return 0.0f;

	if ( range <= fog.start )
		return 0.0f;

	if ( range >= fog.end )
		return 1.0f;

	float ratio = (range - fog.start) / (fog.end - fog.start);
	ratio = Min( ratio, fog.maxdensity.Get() );
	return ratio;
}

//-----------------------------------------------------------------------------
// Purpose: return the current fog parameters
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::GetFogParams( fogparams_t *fog ) const
{
	if ( !fog )
		return false;

	return GetWorldFogParams( const_cast< CBaseCombatCharacter * >( this ), *fog );
}

//-----------------------------------------------------------------------------
// Purpose: Invoke this to update our last known nav area 
// (since there is no think method chained to CBaseCombatCharacter)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::UpdateLastKnownArea( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we can use (walk through) the given area 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsAreaTraversable( const CNavArea *area ) const
{
	return area ? !area->IsBlocked( GetTeamNumber() ) : false;
}

//-----------------------------------------------------------------------------
// Purpose: Leaving the nav mesh
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ClearLastKnownArea( void )
{
	OnNavAreaChanged( nullptr, m_lastNavArea );

	if ( m_lastNavArea )
	{
		if ( m_registeredNavTeam != TEAM_INVALID )
		{
			m_lastNavArea->DecrementPlayerCount( m_registeredNavTeam, entindex() );
			m_lastNavArea->OnExit( this, nullptr );
			m_registeredNavTeam = TEAM_INVALID;
		}
		m_lastNavArea = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handling editor removing the area we're standing upon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::OnNavAreaRemoved( CNavArea *removedArea )
{
	if ( m_lastNavArea == removedArea )
	{
		ClearLastKnownArea();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changing team, maintain associated data
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ChangeTeam_Impl( int iTeamNum )
{
	// old team member no longer in the nav mesh
	ClearLastKnownArea();

	BaseClass::ChangeTeam_Impl( iTeamNum );
}

//-----------------------------------------------------------------------------
// Return true if we have ever been injured by a member of the given team
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::HasEverBeenInjured( int team /*= TEAM_ANY */ ) const
{
	if ( team == TEAM_ANY )
	{
		return ( m_hasBeenInjured == 0 ) ? false : true;
	}

	int teamMask = 1 << team;

	if ( m_hasBeenInjured & teamMask )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Return time since we were hurt by a member of the given team
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetTimeSinceLastInjury( int team /*= TEAM_ANY */ ) const
{
	const float never = 999999999999.9f;

	if ( team == TEAM_ANY )
	{
		float time = never;

		// find most recent injury time
		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team != TEAM_INVALID )
			{
				if ( m_damageHistory[i].interval.GetElapsedTime() < time )
				{
					time = m_damageHistory[i].interval.GetElapsedTime();
				}
			}
		}

		return time;
	}
	else
	{
		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team == team )
			{
				return m_damageHistory[i].interval.GetElapsedTime();
			}
		}
	}

	return never;
}

float CBaseCombatCharacter::GetAliveDuration( void ) const
{
	return m_aliveTimer.GetElapsedTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::WeaponCount() const
{
	return MAX_WEAPONS;
}

#undef ShouldUseVisibilityCache

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CEconItemView;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Switches to the best weapon that is also better than the given weapon.
// Input  : pCurrent - The current weapon used by the player.
// Output : Returns true if the weapon was switched, false if there was no better
//			weapon to switch to.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent)
{
	CBaseCombatWeapon *pNewWeapon = g_pGameRules->GetNextBestWeapon(this, pCurrent);
	
	if ( ( pNewWeapon != nullptr ) && ( pNewWeapon != pCurrent ) )
	{
		return Weapon_Switch( pNewWeapon );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	if ( pWeapon == nullptr )
		return false;

	// Already have it out?
	if ( m_hActiveWeapon.Get() == pWeapon )
	{
		if ( !m_hActiveWeapon->IsWeaponVisible() || m_hActiveWeapon->IsHolstered() )
			return m_hActiveWeapon->Deploy( );
		return false;
	}

	if (!Weapon_CanSwitchTo(pWeapon))
	{
		return false;
	}

	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->IsAlwaysActive() )
		{
			if ( !m_hActiveWeapon->Holster( pWeapon ) )
			{
				return false;
			}
		}
	}

	m_hActiveWeapon = pWeapon;
	return pWeapon->Deploy( );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if (IsPlayer())
	{
		CBasePlayer *pPlayer = (CBasePlayer *)this;
		
	}

	if ( !pWeapon->HasAnyAmmo() && !GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->CanHolster() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetActiveWeapon() const
{
	return m_hActiveWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetWeapon( int i ) const
{
	contract_assert( (i >= 0) && (i < MAX_WEAPONS) );
	return m_hMyWeapons[i].Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iCount - 
//			iAmmoIndex - 
//-----------------------------------------------------------------------------
extern ConVar sv_infinite_ammo;
void CBaseCombatCharacter::RemoveAmmo( int iCount, int iAmmoIndex )
{
	if (iCount <= 0)
		return;

	if ( iAmmoIndex < 0 )
		return;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return;

	if ( sv_infinite_ammo.GetInt() == 2 ) // infinite total ammo but magazine reloads are still required.
		return;

	// Ammo pickup sound
	m_iAmmo.Set( iAmmoIndex, std::max( m_iAmmo[iAmmoIndex] - iCount, 0 ) );
}

void CBaseCombatCharacter::RemoveAmmo( int iCount, const char *szName )
{
	RemoveAmmo( iCount, GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAllAmmo( )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_iAmmo.Set( i, 0 );
	}
}

//-----------------------------------------------------------------------------
// FIXME: This is a sort of hack back-door only used by physgun!
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetAmmoCount( int iCount, int iAmmoIndex )
{
	// NOTE: No sound, no max check! Seems pretty bogus to me!
	m_iAmmo.Set( iAmmoIndex, iCount );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of a particular type owned
//			owned by the character
// Input  :	Ammo Index
// Output :	The amount of ammo
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetAmmoCount( int iAmmoIndex ) const
{

	// DEPRECATED - Ido says please use GetReserveAmmoCount

	if ( iAmmoIndex == -1 )
		return 0;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return 999;

	return m_iAmmo[ iAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CBaseCombatWeapon* CBaseCombatCharacter::Weapon_OwnsThisType( const char *pszWeapon, int iSubType ) const
{
	for ( int i = 0; i < MAX_WEAPONS; i++ ) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs( m_hMyWeapons[i], pszWeapon ) )
		{
			// Make sure it matches the subtype
			if ( m_hMyWeapons[i]->GetSubType() == iSubType )
			{
				return m_hMyWeapons[i];
			}
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested slot
// Input  : slot - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetSlot( int slot ) const
{
	int	targetSlot = slot;

	// Check for that slot being occupied already
	for ( int i=0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[i].Get() != nullptr )
		{
			// If the slots match, it's already occupied
			if ( m_hMyWeapons[i]->GetSlot() == targetSlot )
				return m_hMyWeapons[i];
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested loadout position
// Input  : position - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetPosition( int position ) const
{
	// Check for that slot being occupied already
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[ i ].Get() != nullptr )
		{
			CEconItemView * pItem = m_hMyWeapons[ i ]->GetEconItemView() ;
			if ( !pItem )
				continue;

			loadout_positions_t unItemPos = ( loadout_positions_t )pItem->GetItemDefinition()->GetLoadoutSlot( GetTeamNumber() );

			// If the slots match, it's already occupied
			if ( unItemPos == position )
				return m_hMyWeapons[ i ];
		}
	}

	return nullptr;
}

int CBaseCombatCharacter::BloodColor_Impl()
{
	return m_bloodColor;
}

//-----------------------------------------------------------------------------
// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetBloodColor( int nBloodColor )
{
	m_bloodColor = nBloodColor;
}

//-----------------------------------------------------------------------------
/**
The main visibility check.  Checks all the entity specific reasons that could 
make IsVisible fail.  Then checks points in space to get environmental reasons.
This is LOS, plus invisibility and fog and smoke and such.
*/

enum VisCacheResult_t
{
	VISCACHE_UNKNOWN = 0,
	VISCACHE_IS_VISIBLE,
	VISCACHE_IS_NOT_VISIBLE,
};


class CCombatCharVisCache : public CAutoGameSystemPerFrame
{
public:
	virtual void FrameUpdatePreEntityThink();
	virtual void LevelShutdownPreEntity();

private:
	struct VisCacheKey_t
	{
		CHandle< CBaseCombatCharacter >	m_hEntity1;
		CHandle< CBaseCombatCharacter >	m_hEntity2;
	};

	struct CVisCacheKeyLess
	{
		bool operator()( const VisCacheKey_t &lhs, const VisCacheKey_t &rhs ) const
		{
			return ( std::memcmp( &lhs, &rhs, sizeof( VisCacheKey_t ) ) < 0 );
		}
	};

	struct VisCacheEntry_t
	{
		float							m_flTime;
		bool							m_bEntity1CanSeeEntity2;
		bool							m_bEntity2CanSeeEntity1;
	};

	using VisCacheMap_t = std::map< VisCacheKey_t, VisCacheEntry_t, CVisCacheKeyLess >;

public:
	// An entry, and whether the caller's pair is stored the other way round.
	struct VisCacheRef_t
	{
		VisCacheMap_t::iterator	m_it;
		bool					m_bValid;
		bool					m_bReverse;
	};

	VisCacheRef_t LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 );
	VisCacheResult_t HasVisibility( const VisCacheRef_t &ref ) const;
	void RegisterVisibility( const VisCacheRef_t &ref, bool bChar1SeesChar2, bool bChar2SeesChar1 );

private:
	VisCacheMap_t m_VisCache;

	mutable int m_nTestCount;
	mutable int m_nHitCount;
};

void CCombatCharVisCache::FrameUpdatePreEntityThink()
{
	//	Msg( "test: %d/%d\n", m_nHitCount, m_nTestCount );

	// Lazy retirement of vis cache
	// NOTE: 256 was chosen heuristically based on a playthrough where 200
	// was the max # in the viscache where nothing could be retired.
	if ( m_VisCache.size() < 256 )
		return;

	for ( int i = 0; i < 8; ++i )
	{
		auto it = std::next( m_VisCache.begin(), RandomInt( 0, (int)m_VisCache.size() - 1 ) );
		const VisCacheKey_t &key = it->first;
		if ( !key.m_hEntity1.IsValid() || !key.m_hEntity2.IsValid() || ( gpGlobals->curtime - it->second.m_flTime > 10.0f ) )
		{
			m_VisCache.erase( it );
		}
	}
}

void CCombatCharVisCache::LevelShutdownPreEntity()
{
	m_VisCache.clear();
}

CCombatCharVisCache::VisCacheRef_t CCombatCharVisCache::LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 )
{
	VisCacheKey_t cacheKey;
	if ( pChar1 < pChar2 )
	{
		cacheKey.m_hEntity1 = pChar1;
		cacheKey.m_hEntity2 = pChar2;
	}
	else
	{
		cacheKey.m_hEntity1 = pChar2;
		cacheKey.m_hEntity2 = pChar1;
	}

	auto it = m_VisCache.find( cacheKey );
	if ( it == m_VisCache.end() )
	{
		if ( m_VisCache.size() >= VIS_CACHE_MAX_ENTRIES )
			return { m_VisCache.end(), false, false };

		it = m_VisCache.emplace( cacheKey, VisCacheEntry_t{} ).first;
		it->second.m_flTime = gpGlobals->curtime - 2.0f * VIS_CACHE_ENTRY_LIFE;
	}

	return { it, true, !( pChar1 < pChar2 ) };
}

VisCacheResult_t CCombatCharVisCache::HasVisibility( const VisCacheRef_t &ref ) const
{
	if ( !ref.m_bValid )
		return VISCACHE_UNKNOWN;

	m_nTestCount++;

	bool bReverse = ref.m_bReverse;
	const VisCacheEntry_t &entry = ref.m_it->second;
	if ( gpGlobals->curtime - entry.m_flTime > VIS_CACHE_ENTRY_LIFE )
		return VISCACHE_UNKNOWN;

	m_nHitCount++;

	bool bIsVisible = !bReverse ? entry.m_bEntity1CanSeeEntity2 : entry.m_bEntity2CanSeeEntity1;
	return bIsVisible ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
}

void CCombatCharVisCache::RegisterVisibility( const VisCacheRef_t &ref, bool bEntity1CanSeeEntity2, bool bEntity2CanSeeEntity1 )
{
	if ( !ref.m_bValid )
		return;

	bool bReverse = ref.m_bReverse;
	VisCacheEntry_t &entry = ref.m_it->second;
	entry.m_flTime = gpGlobals->curtime;
	if ( !bReverse )
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity1CanSeeEntity2;
		entry.m_bEntity2CanSeeEntity1 = bEntity2CanSeeEntity1;
	}
	else
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity2CanSeeEntity1;
		entry.m_bEntity2CanSeeEntity1 = bEntity1CanSeeEntity2;
	}
}

static CCombatCharVisCache s_CombatCharVisCache;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsAbleToSee( const CBaseEntity *pEntity, FieldOfViewCheckType checkFOV )
{
	CBaseCombatCharacter *pBCC = const_cast<CBaseEntity *>( pEntity )->MyCombatCharacterPointer();
	if ( pBCC )
		return IsAbleToSee( pBCC, checkFOV );

	// Test this every time; it's cheap.
	Vector vecEyePosition = EyePosition();
	Vector vecTargetPosition = pEntity->WorldSpaceCenter();

	Vector vecEyeToTarget;
	VectorSubtract( vecTargetPosition, vecEyePosition, vecEyeToTarget );
	float flDistToOther = VectorNormalize( vecEyeToTarget ); 

	// We can't see because they are too far in the fog
	if ( IsHiddenByFog( flDistToOther ) )
		return false;

	if ( !ComputeLOS( vecEyePosition, vecTargetPosition ) )
		return false;

	return ( checkFOV != USE_FOV || IsInFieldOfView( vecTargetPosition ) );
}

static void ComputeSeeTestPosition( Vector *pEyePosition, CBaseCombatCharacter *pBCC )
{
	{
		*pEyePosition = pBCC->EyePosition();
	}	
}

bool CBaseCombatCharacter::IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV )
{
	Vector vecEyePosition, vecOtherEyePosition;
	ComputeSeeTestPosition( &vecEyePosition, this );
	ComputeSeeTestPosition( &vecOtherEyePosition, pBCC );

	Vector vecEyeToTarget;
	VectorSubtract( vecOtherEyePosition, vecEyePosition, vecEyeToTarget );
	float flDistToOther = VectorNormalize( vecEyeToTarget ); 

	// Test this every time; it's cheap.
	// We can't see because they are too far in the fog
	if ( IsHiddenByFog( flDistToOther ) )
		return false;

	// Check if we have a cached-off visibility
	CCombatCharVisCache::VisCacheRef_t cacheRef = s_CombatCharVisCache.LookupVisibility( this, pBCC );
	VisCacheResult_t nResult = s_CombatCharVisCache.HasVisibility( cacheRef );

	// Compute symmetric visibility
	if ( nResult == VISCACHE_UNKNOWN )
	{
		bool bThisCanSeeOther = false, bOtherCanSeeThis = false;
		if ( ComputeLOS( vecEyePosition, vecOtherEyePosition ) )
		{
			bThisCanSeeOther = true, bOtherCanSeeThis = true;
		}

		s_CombatCharVisCache.RegisterVisibility( cacheRef, bThisCanSeeOther, bOtherCanSeeThis );
		nResult = bThisCanSeeOther ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
	}

	if ( nResult == VISCACHE_IS_VISIBLE )
		return ( checkFOV != USE_FOV || IsInFieldOfView( pBCC ) );

	return false;
}

class CTraceFilterNoCombatCharacters : public CTraceFilterSimple
{
public:
	CTraceFilterNoCombatCharacters( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
		{
			CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
			if ( !pEntity )
				return false;

			if ( pEntity->MyCombatCharacterPointer() || pEntity->MyCombatWeaponPointer() )
				return false;

			// Honor BlockLOS - this lets us see through partially-broken doors, etc
			if ( !pEntity->BlocksLOS() )
				return false;

			return true;
		}

		return false;
	}
};

bool CBaseCombatCharacter::ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const
{
	// We simply can't see because the world is in the way.
	trace_t result;
	CTraceFilterNoCombatCharacters traceFilter( nullptr, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecEyePosition, vecTarget, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &result );
	return ( result.fraction == 1.0f );
}

//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const CBaseEntity *target, float cosTolerance ) const
{
	return IsLookingTowards( target->WorldSpaceCenter(), cosTolerance ) || IsLookingTowards( target->EyePosition(), cosTolerance ) || IsLookingTowards( target->GetAbsOrigin(), cosTolerance );
}

//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const Vector &target, float cosTolerance ) const
{
	Vector toTarget = target - EyePosition();
	toTarget.NormalizeInPlace();

	Vector forward;
	AngleVectors( EyeAngles(), &forward );

	return ( DotProduct( forward, toTarget ) >= cosTolerance );
}

//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( CBaseEntity *entity ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );
	float flTolerance = pPlayer ? std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) : BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE;

	Vector vecForward;
	Vector vecEyePosition = EyePosition();
	AngleVectors( EyeAngles(), &vecForward );

	// FIXME: Use a faster check than this!

	// Check 3 spots, or else when standing right next to someone looking at their eyes, 
	// the angle will be too great to see their center.
	Vector vecToTarget = entity->GetAbsOrigin() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->WorldSpaceCenter() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->EyePosition() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	return ( DotProduct( vecForward, vecToTarget ) >= flTolerance );
}

//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( const Vector &pos ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );

	if ( pPlayer )
		return IsLookingTowards( pos, std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) );

	return IsLookingTowards( pos );
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/

bool CBaseCombatCharacter::IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType ) const
{
	// FIXME: Should we do the same check here as the client does?
	return IsLineOfSightClear( entity->WorldSpaceCenter(), checkType, entity ) || IsLineOfSightClear( entity->EyePosition(), checkType, entity ) || IsLineOfSightClear( entity->GetAbsOrigin(), checkType, entity );
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/
static bool TraceFilterNoCombatCharacters( IHandleEntity *pServerEntity, int contentsMask )
{
	// Honor BlockLOS also to allow seeing through partially-broken doors
	CBaseEntity *entity = EntityFromEntityHandle( pServerEntity );
	return ( entity->MyCombatCharacterPointer() == nullptr && !entity->MyCombatWeaponPointer() && entity->BlocksLOS() );
}

bool CBaseCombatCharacter::IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType, CBaseEntity *entityToIgnore ) const
{
	if( checkType == IGNORE_ACTORS )
	{

		// use the query cache unless it causes problems
		trace_t trace;
		CTraceFilterNoCombatCharacters traceFilter( entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
	else
	{
		trace_t trace;
		CTraceFilterSkipTwoEntities traceFilter( this, entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
}

KS_ENTITY_ABSTRACT_KIND( CBaseCombatCharacter )
