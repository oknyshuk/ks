// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "igameevents.h"
#include "irecipientfilter.h"
#include "networkvar.h"
#include "server_class.h"
#include "predictable_entity.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "networkstringtabledefs.h"
#include "string_t.h"
#include "gamestringpool.h"
#include "gametrace.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "edict.h"
#include "filesystem.h"
#include "tier2/tier2_logging.h"
#include "basehandle.h"
#include "usermessages_schema.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :vscript;
import :player;
import :fish;
import :sharedvar;
import :baseentity;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.tier1.keyvalues;
import ks.math.polyhedron;
import ks.tier1.map_ops;
import ks.core.random;
import ks.tier1.vector_ops;
import :entity_dispatch;
import :entity_kind;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.tier1.intern_pool;
import ks.tier1.string_ops;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.core.tslist;
import ks.core.time;
import ks.tier1.icvar;

class CAmmoDef;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CGameRules;
class CTakeDamageInfo;

using namespace ::ks::reflect;

ConVar g_Language( side_registry(), "g_Language", "0", FCVAR_REPLICATED );
ConVar sk_autoaim_mode( side_registry(), "sk_autoaim_mode", "1", FCVAR_ARCHIVE | FCVAR_REPLICATED );

static CViewVectors g_DefaultViewVectors(
	Vector( 0, 0, 64 ),			//VEC_VIEW (m_vView)
								
	Vector(-16, -16, 0 ),		//VEC_HULL_MIN (m_vHullMin)
	Vector( 16,  16,  72 ),		//VEC_HULL_MAX (m_vHullMax)
													
	Vector(-16, -16, 0 ),		//VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector( 16,  16,  36 ),		//VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector( 0, 0, 28 ),			//VEC_DUCK_VIEW		(m_vDuckView)
													
	Vector(-10, -10, -10 ),		//VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector( 10,  10,  10 ),		//VEC_OBS_HULL_MAX	(m_vObsHullMax)
													
	Vector( 0, 0, 14 )			//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)
);													
	
// ------------------------------------------------------------------------------------ //
// CGameRulesProxy implementation.
// ------------------------------------------------------------------------------------ //

CGameRulesProxy *CGameRulesProxy::s_pGameRulesProxy = nullptr;

IMPLEMENT_NETWORKCLASS_ALIASED( GameRulesProxy, DT_GameRulesProxy )

// Don't send any of the CBaseEntity stuff..
IMPLEMENT_REFLECT_TABLE( CGameRulesProxy, DT_GameRulesProxy );

CGameRulesProxy::CGameRulesProxy()
{
	// allow map placed proxy entities to overwrite the static one
	if ( s_pGameRulesProxy )
	{
		UTIL_Remove( s_pGameRulesProxy );
		s_pGameRulesProxy = nullptr;
	}
	s_pGameRulesProxy = this;
}

CGameRulesProxy::~CGameRulesProxy()
{
	if ( s_pGameRulesProxy == this )
	{
		s_pGameRulesProxy = nullptr;
	}
}

int CGameRulesProxy::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );

}

void CGameRulesProxy::NotifyNetworkStateChanged()
{
	if ( s_pGameRulesProxy )
		s_pGameRulesProxy->NetworkStateChanged();
}

ConVar	old_radius_damage( side_registry(), "old_radiusdamage", "0.0", FCVAR_REPLICATED );

// In tf_gamerules.cpp or hl_gamerules.cpp.
extern IVoiceGameMgrHelper *g_pVoiceGameMgrHelper;

CGameRules*	g_pGameRules = nullptr;
extern bool	g_fGameOver;

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CGameRules::CGameRules() : CAutoGameSystemPerFrame( "CGameRules" )
{
	contract_assert( !g_pGameRules );
	g_pGameRules = this;

	GetVoiceGameMgr()->Init( g_pVoiceGameMgrHelper, gpGlobals->maxClients );
	ClearMultiDamage();
}

//-----------------------------------------------------------------------------
// Precache game-specific resources
//-----------------------------------------------------------------------------
void CGameRules::Precache( void ) 
{
	// Used by particle property
	PrecacheEffect( "ParticleEffect" );
	PrecacheEffect( "ParticleEffectStop" );

	// Used by default impact system
	PrecacheEffect( "GlassImpact" );
	PrecacheEffect( "Impact" );
	PrecacheEffect( "RagdollImpact" );
	PrecacheEffect( "gunshotsplash"	);
	PrecacheEffect( "TracerSound" );
	PrecacheEffect( "Tracer" );

	// Used by physics impacts
	PrecacheEffect( "watersplash" );
	PrecacheEffect( "waterripple" );

	// Used by UTIL_BloodImpact, which is used in many places
	PrecacheEffect( "bloodimpact" );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player can carry any more of the ammo type
//-----------------------------------------------------------------------------
bool CGameRules::CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex )
{
	if ( iAmmoIndex > -1 )
	{
		// Get the max carrying capacity for this ammo
		int iMaxCarry = GetAmmoDef()->MaxCarry( iAmmoIndex, pPlayer );

		// Does the player have room for more of this type of ammo?
		if ( pPlayer->GetAmmoCount( iAmmoIndex ) < iMaxCarry )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player can carry any more of the ammo type
//-----------------------------------------------------------------------------
bool CGameRules::CanHaveAmmo( CBaseCombatCharacter *pPlayer, const char *szName )
{
	return CanHaveAmmo( pPlayer, GetAmmoDef()->Index(szName) );
}

//=========================================================
//=========================================================
CBaseEntity *CGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();
	contract_assert( pSpawnSpot );
	if ( pSpawnSpot == nullptr )
		return nullptr;

	pPlayer->SetLocalOrigin( pSpawnSpot->GetAbsOrigin() + Vector(0,0,1) );
	pPlayer->SetAbsVelocity( vec3_origin );
	pPlayer->SetLocalAngles( pSpawnSpot->GetLocalAngles() );
	pPlayer->m_Local.m_viewPunchAngle = vec3_angle;
	pPlayer->m_Local.m_aimPunchAngle = vec3_angle;
	pPlayer->m_Local.m_aimPunchAngleVel = vec3_angle;
	pPlayer->SnapEyeAngles( pSpawnSpot->GetLocalAngles() );

	return pSpawnSpot;
}

// checks if the spot is clear of players
bool CGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer  )
{
	CBaseEntity *ent = nullptr;

	if ( !pSpot->IsTriggered( pPlayer ) )
	{
		return false;
	}

	for ( CEntitySphereQuery sphere( pSpot->GetAbsOrigin(), 128 ); (ent = sphere.GetCurrentEntity()) != nullptr; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		if ( ent->IsPlayer() && ent != pPlayer )
			return false;
	}

	return true;
}

//=========================================================
//=========================================================
bool CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
/*
	if ( pWeapon->m_pszAmmo1 )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->m_iPrimaryAmmoType ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->Weapon_OwnsThisType( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->Weapon_OwnsThisType( pWeapon ) )
		{
			return FALSE;
		}
	}
*/
	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return true;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( bool forceUpdate )
{
	if ( !forceUpdate )
	{
		if ( GlobalEntity_IsInTable( "skill.cfg" ) )
			return;
	}
	GlobalEntity_Add( "skill.cfg", STRING(gpGlobals->mapname), GLOBAL_ON );
	char	szExec[256];

	ConVarRef skill( "skill" );

	SetSkillLevel( skill.IsValid() ? skill.GetInt() : 1 );

	V_snprintf( szExec,sizeof(szExec), "exec skill%d.cfg\n", GetSkillLevel() );

	engine->ServerCommand( szExec );
	engine->ServerExecute();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool IsExplosionTraceBlocked( trace_t *ptr )
{
	if( TraceDidHitWorld( *ptr ) )
		return true;

	if( ptr->Ent<CBaseEntity>() == nullptr )
		return false;

	if( ptr->Ent<CBaseEntity>()->GetMoveType() == MOVETYPE_PUSH )
	{
		// All doors are push, but not all things that push are doors. This 
		// narrows the search before we start to do classname compares.
		if( FClassnameIs(ptr->Ent<CBaseEntity>(), "prop_door_rotating") ||
        FClassnameIs(ptr->Ent<CBaseEntity>(), "func_door") ||
        FClassnameIs(ptr->Ent<CBaseEntity>(), "func_door_rotating") )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Default implementation of radius damage
//-----------------------------------------------------------------------------
static constexpr auto ROBUST_RADIUS_PROBE_DIST = 16.0f; // If a solid surface blocks the explosion, this is how far to creep along the surface looking for another way to the target
void CGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	const int MASK_RADIUS_DAMAGE = MASK_SHOT&(~CONTENTS_HITBOX);
	CBaseEntity *pEntity = nullptr;
	trace_t		tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	Vector vecSrc = vecSrcIn;

	if ( flRadius )
		falloff = info.GetDamage() / flRadius;
	else
		falloff = 1.0;

	int bInWater = (UTIL_PointContents ( vecSrc, MASK_WATER ) & MASK_WATER) ? true : false;

	vecSrc.z += 1;// in case grenade is lying on the ground

	float flHalfRadiusSqr = Square( flRadius / 2.0f );

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); (pEntity = sphere.GetCurrentEntity()) != nullptr; sphere.NextEntity() )
	{
		// This value is used to scale damage when the explosion is blocked by some other object.
		float flBlockedDamagePercent = 0.0f;

		if ( pEntity == pEntityIgnore )
			continue;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			continue;

		// UNDONE: this should check a damage mask, not an ignore
		if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
		{// houndeyes don't hurt other houndeyes with their attack
			continue;
		}

		// blast's don't tavel into or out of water
		if (bInWater && pEntity->GetWaterLevel() == WL_NotInWater)
			continue;

		if (!bInWater && pEntity->GetWaterLevel() == WL_Eyes)
			continue;

		// Check that the explosion can 'see' this entity.
		vecSpot = pEntity->BodyTarget( vecSrc, false );
		UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

		if( old_radius_damage.GetBool() )
		{
			if ( tr.fraction != 1.0 && tr.Ent<CBaseEntity>() != pEntity )
			continue;
		}
		else
		{
			if ( tr.fraction != 1.0 )
			{
				if ( IsExplosionTraceBlocked(&tr) )
				{
					if( ShouldUseRobustRadiusDamage( pEntity ) )
					{
						if( vecSpot.DistToSqr( vecSrc ) > flHalfRadiusSqr )
						{
							// Only use robust model on a target within one-half of the explosion's radius.
							continue;
						}

						Vector vecToTarget = vecSpot - tr.endpos;
						VectorNormalize( vecToTarget );

						// We're going to deflect the blast along the surface that 
						// interrupted a trace from explosion to this target.
						Vector vecUp, vecDeflect;
						CrossProduct( vecToTarget, tr.plane.normal, vecUp );
						CrossProduct( tr.plane.normal, vecUp, vecDeflect );
						VectorNormalize( vecDeflect );

						// Trace along the surface that intercepted the blast...
						UTIL_TraceLine( tr.endpos, tr.endpos + vecDeflect * ROBUST_RADIUS_PROBE_DIST, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );
						//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 255, 0, false, 10 );

						// ...to see if there's a nearby edge that the explosion would 'spill over' if the blast were fully simulated.
						UTIL_TraceLine( tr.endpos, vecSpot, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );
						//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );

						if( tr.fraction != 1.0 && TraceDidHitWorld( tr ) )
						{
							// Still can't reach the target.
							continue;
						}
						// else fall through
					}
					else
					{
						continue;
					}
				}

				// UNDONE: Probably shouldn't let children block parents either?  Or maybe those guys should set their owner if they want this behavior?
				// HL2 - Dissolve damage is not reduced by interposing non-world objects
				if( tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>() != pEntity && tr.Ent<CBaseEntity>()->GetOwnerEntity() != pEntity )
				{
					// Some entity was hit by the trace, meaning the explosion does not have clear
					// line of sight to the entity that it's trying to hurt. If the world is also
					// blocking, we do no damage.
					CBaseEntity *pBlockingEntity = tr.Ent<CBaseEntity>();
					//Msg( "%s may be blocked by %s...", pEntity->GetClassname(), pBlockingEntity->GetClassname() );

					UTIL_TraceLine( vecSrc, vecSpot, CONTENTS_SOLID, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

					if( tr.fraction != 1.0 )
					{
						continue;
					}
					
					// Now, if the interposing object is physics, block some explosion force based on its mass.
					if( pBlockingEntity->VPhysicsGetObject() )
					{
						const float MASS_ABSORB_ALL_DAMAGE = 350.0f;
						float flMass = pBlockingEntity->VPhysicsGetObject()->GetMass();
						float scale = flMass / MASS_ABSORB_ALL_DAMAGE;

						// Absorbed all the damage.
						if( scale >= 1.0f )
						{
							continue;
						}

						ASSERT( scale > 0.0f );
						flBlockedDamagePercent = scale;
						//Msg("  Object (%s) weighing %fkg blocked %f percent of explosion damage\n", pBlockingEntity->GetClassname(), flMass, scale * 100.0f);
					}
					else
					{
						// Some object that's not the world and not physics. Generically block 25% damage
						flBlockedDamagePercent = 0.25f;
					}
				}
			}
		}
		// decrease damage for an ent that's farther from the bomb.
		float flDistanceToEnt = ( vecSrc - tr.endpos ).Length();
		flAdjustedDamage = flDistanceToEnt * falloff;
		flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

		if ( flAdjustedDamage <= 0 )
		{
			continue;
		}

		// the explosion can 'see' this entity, so hurt them!
		if (tr.startsolid)
		{
			// if we're stuck inside them, fixup the position and distance
			tr.endpos = vecSrc;
			tr.fraction = 0.0;
		}
		
		CTakeDamageInfo adjustedInfo = info;
		//Msg("%s: Blocked damage: %f percent (in:%f  out:%f)\n", pEntity->GetClassname(), flBlockedDamagePercent * 100, flAdjustedDamage, flAdjustedDamage - (flAdjustedDamage * flBlockedDamagePercent) );
		adjustedInfo.SetRadius( flRadius );
		adjustedInfo.SetDamage( flAdjustedDamage - (flAdjustedDamage * flBlockedDamagePercent) );

		// Now make a consideration for skill level!
		

		Vector dir = vecSpot - vecSrc;
		VectorNormalize( dir );

		// If we don't have a damage force, manufacture one
		if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
		{
			if ( !( adjustedInfo.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE ) )
			{
				CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc );
			}
		}
		else
		{
			// Assume the force passed in is the maximum force. Decay it based on falloff.
			float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
			adjustedInfo.SetDamageForce( dir * flForce );
			adjustedInfo.SetDamagePosition( vecSrc );
		}

		if ( tr.fraction != 1.0 && pEntity == tr.Ent<CBaseEntity>() )
		{
			ClearMultiDamage( );
			pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
			ApplyMultiDamage();
		}
		else
		{
			pEntity->TakeDamage( adjustedInfo );
		}

		// Now hit all triggers along the way that respond to damage... 
		pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, tr.endpos, dir );

		if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() && ToBaseCombatCharacter( tr.Ent<CBaseEntity>() ) )
		{

			// This is a total hack!!!
			bool bIsPrimary = true;
			CBasePlayer *player = ToBasePlayer( info.GetAttacker() );
			CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
			if ( pWeapon && FClassnameIs( pWeapon, "weapon_smg1" ) )
			{
				bIsPrimary = false;
			}

			gamestats->Event_WeaponHit( player, bIsPrimary, (pWeapon != nullptr) ? player->GetActiveWeapon()->GetClassname() : "NULL", info );
		}
	}
}

bool CGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	if( pEdict->IsPlayer() )
	{
		if( GetVoiceGameMgr()->ClientCommand( static_cast<CBasePlayer*>(pEdict), args ) )
			return true;
	}

	return false;
}

void CGameRules::FrameUpdatePostEntityThink()
{
	CVProfScope _( "CGameRules::FrameUpdatePostEntityThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	Think();
}

// Hook into the convar from the engine
ConVar skill( side_registry(), "skill", "1", FCVAR_ARCHIVE );

void CGameRules::Think()
{
	GetVoiceGameMgr()->Update( gpGlobals->frametime );

	SetSkillLevel( skill.GetInt() );
}

//-----------------------------------------------------------------------------
// Purpose: Called at the end of GameFrame (i.e. after all game logic has run this frame)
//-----------------------------------------------------------------------------
void CGameRules::EndGameFrame( void )
{
	// If you hit this assert, it means something called AddMultiDamage() and didn't ApplyMultiDamage().
	// The g_MultiDamage.m_hAttacker & g_MultiDamage.m_hInflictor should give help you figure out the culprit.
	if ( !g_MultiDamage.IsClear() )
	{
		Warning("Unapplied multidamage left in the system:\nTarget: %s\nInflictor: %s\nAttacker: %s\nDamage: %.2f\n", 
			g_MultiDamage.GetTarget()->GetDebugName(),
			g_MultiDamage.GetInflictor()->GetDebugName(),
			g_MultiDamage.GetAttacker()->GetDebugName(),
			g_MultiDamage.GetDamage() );
		ApplyMultiDamage();
	}
}

//-----------------------------------------------------------------------------
// trace line rules
//-----------------------------------------------------------------------------
float CGameRules::WeaponTraceEntity( CBaseEntity *pEntity, const Vector &vecStart, const Vector &vecEnd,
					 unsigned int mask, trace_t *ptr )
{
	UTIL_TraceEntity( pEntity, vecStart, vecEnd, mask, ptr );
	return 1.0f;
}

void CGameRules::CreateStandardEntities()
{
	g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "player_manager", vec3_origin, vec3_angle );
	g_pPlayerResource->AddEFlags( EFL_KEEP_ON_RECREATE_ENTITIES );
}

//-----------------------------------------------------------------------------
// Purpose: Inform client(s) they can mark the indicated achievement as completed (SERVER VERSION)
// Input  : filter - which client(s) to send this to
//			iAchievementID - The enumeration value of the achievement to mark (see TODO:Kerry, what file will have the mod's achievement enum?) 
//-----------------------------------------------------------------------------
void CGameRules::MarkAchievement( IRecipientFilter& filter, char const *pchAchievementName )
{
	gamestats->Event_IncrementCountedStatistic( vec3_origin, pchAchievementName, 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper allowing gamerules to change the way client-finding in PVS works
//-----------------------------------------------------------------------------
edict_t *CGameRules::DoFindClientInPVS( edict_t *pEdict, unsigned char *pvs, unsigned pvssize )
{
	return UTIL_FindClientInPVSGuts( pEdict, pvs, pvssize );
}

// ----------------------------------------------------------------------------- //
// Shared CGameRules implementation.
// ----------------------------------------------------------------------------- //

CGameRules::~CGameRules()
{
	contract_assert( g_pGameRules == this );
	g_pGameRules = nullptr;
}

bool CGameRules::Init()
{
	RefreshSkillData( true );

	return true;
}

bool CGameRules::SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	return false;
}

CBaseCombatWeapon *CGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	return nullptr;
}

bool CGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}

	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		return false;
	}

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		// let debris and multiplayer objects collide
		return true;
	}

	// Only let projectile blocking debris collide with projectiles
	if ( collisionGroup0 == COLLISION_GROUP_PROJECTILE && collisionGroup1 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE )
		return true;

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE || collisionGroup1 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE )
		return false;
	
	// --------------------------------------------------------------------------
	// NOTE: All of this code assumes the collision groups have been sorted!!!!
	// NOTE: Don't change their order without rewriting this code !!!
	// --------------------------------------------------------------------------

	// Don't bother if either is in a vehicle...
	if (( collisionGroup0 == COLLISION_GROUP_IN_VEHICLE ) || ( collisionGroup1 == COLLISION_GROUP_IN_VEHICLE ))
		return false;

	if ( ( collisionGroup1 == COLLISION_GROUP_DOOR_BLOCKER ) && ( collisionGroup0 != COLLISION_GROUP_NPC ) )
		return false;

	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER ) && ( collisionGroup1 == COLLISION_GROUP_PASSABLE_DOOR ) )
		return false;

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || collisionGroup0 == COLLISION_GROUP_DEBRIS_TRIGGER )
	{
		// put exceptions here, right now this will only collide with COLLISION_GROUP_NONE
		return false;
	}

	// Dissolving guys only collide with COLLISION_GROUP_NONE
	if ( (collisionGroup0 == COLLISION_GROUP_DISSOLVING) || (collisionGroup1 == COLLISION_GROUP_DISSOLVING) )
	{
		if ( collisionGroup0 != COLLISION_GROUP_NONE )
			return false;
	}

	// doesn't collide with other members of this group
	// or debris, but that's handled above
	if ( collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && collisionGroup1 == COLLISION_GROUP_INTERACTIVE_DEBRIS )
		return false;

	// This change was breaking HL2DM
	// Adrian: TEST! Interactive Debris doesn't collide with the player.
	if ( collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && ( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT || collisionGroup1 == COLLISION_GROUP_PLAYER ) )
		 return false;

	if ( collisionGroup0 == COLLISION_GROUP_BREAKABLE_GLASS && collisionGroup1 == COLLISION_GROUP_BREAKABLE_GLASS )
		return false;

	// interactive objects collide with everything except debris & interactive debris
	if ( collisionGroup1 == COLLISION_GROUP_INTERACTIVE && collisionGroup0 != COLLISION_GROUP_NONE )
		return false;

	// Projectiles hit everything but debris, weapons, + other projectiles
	if ( collisionGroup1 == COLLISION_GROUP_PROJECTILE )
	{
		if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE )
		{
			return false;
		}
	}

	// Don't let vehicles collide with weapons
	// Don't let players collide with weapons...
	// Don't let NPCs collide with weapons
	// Weapons are triggers, too, so they should still touch because of that
	if ( collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		if ( collisionGroup0 == COLLISION_GROUP_VEHICLE || 
			collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ) //don't let weapons collide with weapons, they can pile up and choke the server
		{
			return false;
		}
	}

	// collision with vehicle clip entity??
	if ( collisionGroup0 == COLLISION_GROUP_VEHICLE_CLIP || collisionGroup1 == COLLISION_GROUP_VEHICLE_CLIP )
	{
		// yes then if it's a vehicle, collide, otherwise no collision
		// vehicle sorts lower than vehicle clip, so must be in 0
		if ( collisionGroup0 == COLLISION_GROUP_VEHICLE )
			return true;
		// vehicle clip against non-vehicle, no collision
		return false;
	}

	return true;
}

const CViewVectors* CGameRules::GetViewVectors() const
{
	return &g_DefaultViewVectors;
}

//-----------------------------------------------------------------------------
// Purpose: Returns how much damage the given ammo type should do to the victim
//			when fired by the attacker.
// Input  : pAttacker - Dude what shot the gun.
//			pVictim - Dude what done got shot.
//			nAmmoType - What been shot out.
// Output : How much hurt to put on dude what done got shot (pVictim).
//-----------------------------------------------------------------------------
float CGameRules::GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType )
{
	float flDamage = 0;
	CAmmoDef *pAmmoDef = GetAmmoDef();

	if ( pAttacker->IsPlayer() )
	{
		flDamage = pAmmoDef->PlrDamage( nAmmoType );
	}
	else
	{
		flDamage = pAmmoDef->NPCDamage( nAmmoType );
	}

	return flDamage;
}

const char *CGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( pPlayer && pPlayer->IsAlive() == false )
	{
		if ( bTeamOnly )
			return "*DEAD*(TEAM)";
		else
			return "*DEAD*";
	}
	
	return "";
}

void CGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	const char *pszName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );

	const char *pszOldName = pPlayer->GetPlayerName();

	// msg everyone if someone changes their name, and it isn't the first time (changing no name to current name)
	// Note, this is case sensitive
	if ( ( V_strcmp( pszOldName, pszName ) != 0 ) &&
		CanClientCustomizeOwnIdentity() )
	{
		if ( pszOldName[0] != '\0' )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "player_changename" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetString( "oldname", pszOldName );
				event->SetString( "newname", pszName );
				gameeventmanager->FireEvent( event );
			}
		}

		pPlayer->SetPlayerName( pszName );
	}

	const char *pszFov = engine->GetClientConVarValue( pPlayer->entindex(), "fov_desired" );
	if ( pszFov )
	{
		int iFov = std::atoi(pszFov);
		iFov = clamp( iFov, 1, 90 );
		pPlayer->SetDefaultFOV( iFov );
	}
}

void CGameRules::SetSkillLevel( int iLevel)
{
	int oldLevel = g_iSkillLevel; 

	if ( iLevel < 1 )
	{
		iLevel = 1;
	}
	else if ( iLevel > 3 )
	{
		iLevel = 3; 
	}

	g_iSkillLevel = iLevel;

	if( g_iSkillLevel != oldLevel )
	{
		OnSkillLevelChanged( g_iSkillLevel );
	}
}

using namespace ::ks::reflect;

// ------------------------------------------------------------------------------------------ //
// CGameRulesRegister implementation.
// ------------------------------------------------------------------------------------------ //

CGameRulesRegister* CGameRulesRegister::s_pHead = nullptr;

CGameRulesRegister::CGameRulesRegister( const char *pClassName, CreateGameRulesFn fn )
{
	m_pClassName = pClassName;
	m_pFn = fn;
	
	m_pNext = s_pHead;	// Add us to the global list.
	s_pHead = this;
}

void CGameRulesRegister::CreateGameRules()
{
	m_pFn();
}

CGameRulesRegister* CGameRulesRegister::FindByName( const char *pName )
{
	for ( CGameRulesRegister *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( V_stricmp( pName, pCur->m_pClassName ) == 0 )
			return pCur;
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------ //
// Functions to dispatch the messages to create the game rules object on the client.
// ------------------------------------------------------------------------------------------ //

inline constexpr const char *GAMERULES_STRINGTABLE_NAME = "GameRulesCreation";

	INetworkStringTable *g_StringTableGameRules = nullptr;

	void CreateNetworkStringTables_GameRules()
	{
		// Create the string table used by 
		g_StringTableGameRules = networkstringtable->CreateStringTable( GAMERULES_STRINGTABLE_NAME, 1 );
	}

	void CreateGameRulesObject( const char *pClassName )
	{
		// Delete the old game rules object.
		delete g_pGameRules;
		g_pGameRules = nullptr;

		// Create a new game rules object.
		CGameRulesRegister *pReg = CGameRulesRegister::FindByName( pClassName );
		if ( !pReg )
			Error( "InitGameRules: missing gamerules class '%s' on the server", pClassName );
	
		pReg->CreateGameRules();
		if ( !g_pGameRules )
		{
			Error( "InitGameRules: game rules entity (%s) not created", pClassName );
		}

		// Make sure the client gets notification to make a new game rules object.
		contract_assert( g_StringTableGameRules );
		g_StringTableGameRules->AddString( true, "classname", std::strlen( pClassName ) + 1, pClassName );

		if ( g_pGameRules )
		{
			g_pGameRules->CreateCustomNetworkStringTables();
		}
	}

class IGameSystem;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: The actual storage for pooled per-level strings
//-----------------------------------------------------------------------------
class CGameStringPool : public CBaseGameSystem
{
	virtual char const *Name() { return "CGameStringPool"; }

	virtual void LevelShutdownPostEntity() 
	{
		FreeAll();
		PurgeDeferredDeleteList();
		CGameString::IncrementSerialNumber();
	}

public:
	// Case-insensitive; keeps the first spelling. string_t holds the returned pointer, which a node set keeps stable.
	const char *Allocate( const char *pszValue )
	{
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it == m_Strings.end() )
			it = m_Strings.emplace( pszValue ).first;
		return it->c_str();
	}

	const char *Find( const char *pszValue )
	{
		if ( !pszValue )
			return nullptr;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		return it == m_Strings.end() ? nullptr : it->c_str();
	}

	void FreeAll()
	{
		m_Strings.clear();
	}

	void PurgeDeferredDeleteList()
	{
		m_DeferredDeleteList.clear(); m_DeferredDeleteList.shrink_to_fit();
	}

	void Dump( void )
	{
		std::vector< const char * > sorted;
		for ( const std::string &s : m_Strings )
			sorted.push_back( s.c_str() );
		std::sort( sorted.begin(), sorted.end(), ks::cstr_iless() );
		for ( int i = 0; i < std::ssize( sorted ); i++ )
		{
			DevMsg( "  %d (0x%p) : %s\n", i, sorted[i], sorted[i] );
		}
		DevMsg( "\n" );
		DevMsg( "Size:  %d items\n", (int)m_Strings.size() );
	}

	// Existing string_t's may still point at the string, so it stays allocated until level shutdown.
	void Remove( const char *pszValue )
	{
		if ( !pszValue )
			return;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it != m_Strings.end() )
		{
			m_DeferredDeleteList.push_back( m_Strings.extract( it ) );
		}
	}

private:
	using StringSet_t = std::unordered_set< std::string, ks::ihash, ks::iequal >;
	StringSet_t m_Strings;
	std::vector< StringSet_t::node_type > m_DeferredDeleteList;
};

static CGameStringPool g_GameStringPool;

//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem()
{
	return &g_GameStringPool;
}

//-----------------------------------------------------------------------------
// Purpose: The public accessor for the level-global pooled strings
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char * pszValue )
{
	if (pszValue && *pszValue)
		return MAKE_STRING( g_GameStringPool.Allocate( pszValue ) );
	return NULL_STRING;
}

string_t FindPooledString( const char *pszValue )
{
	return MAKE_STRING( g_GameStringPool.Find( pszValue ) );
}

void RemovePooledString( const char *pszValue )
{
	g_GameStringPool.Remove( pszValue );
}

int CGameString::gm_iSerialNumber = 1;

#if !defined(CLIENT_DLL) && !defined( GC )
//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CC_DumpGameStringTable( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_GameStringPool.Dump();
}
static ConCommand dumpgamestringtable(side_registry(), "dumpgamestringtable", CC_DumpGameStringTable, "Dump the contents of the game string table to the console.", FCVAR_CHEAT);
#endif

CGameString::~CGameString()
{
	if ( m_bCopy ) 
		std::free( (void *)m_pszString ); 
}

class CBaseEntity;

using namespace ::ks::reflect;

bool TraceDidHitWorld( const CGameTrace &tr )
{
	return tr.Ent<CBaseEntity>() == GetWorldEntity();
}

bool TraceDidHitNonWorldEntity( const CGameTrace &tr )
{
	return tr.m_pEnt != nullptr && !TraceDidHitWorld( tr );
}

int TraceGetEntityIndex( const CGameTrace &tr )
{
	if ( tr.m_pEnt )
		return tr.Ent<CBaseEntity>()->entindex();
	else
		return -1;
}

using namespace ::ks::reflect;

void MPForceCameraCallback( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( mp_forcecamera.GetInt() < OBS_ALLOW_ALL || mp_forcecamera.GetInt() >= OBS_ALLOW_NUM_MODES )
	{
		mp_forcecamera.SetValue( OBS_ALLOW_TEAM );
	}
}

// some shared cvars used by game rules
ConVar mp_forcecamera( 
	side_registry(), "mp_forcecamera", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Restricts spectator modes for dead players. 0 = Any team. 1 = Only own team. 2 = No one; fade to black on death (previously mp_fadetoblack)."
	, MPForceCameraCallback 
	);

ConVar mp_radar_showall(
	side_registry(), "mp_radar_showall",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines who should see all. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists.",
	true, 0, //min
	true, 3	 //max
	);
	
ConVar mp_allowspectators(
	side_registry(), "mp_allowspectators", 
	"1.0", 
	FCVAR_REPLICATED,
	"toggles whether the server allows spectator mode or not" );

ConVar mp_friendlyfire(
	side_registry(), "mp_friendlyfire",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"Allows team members to injure other members of their team"
	);

ConVar mp_teammates_are_enemies(
	side_registry(), "mp_teammates_are_enemies",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, your teammates act as enemies and all players are valid targets."
	);

ConVar mp_buy_anywhere(
	side_registry(), "mp_buy_anywhere",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy anywhere, not only in buyzones. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);

ConVar mp_buy_during_immunity(
	side_registry(), "mp_buy_during_immunity",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy when immune, ignoring buytime. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

//=========================================================
//=========================================================
class CGameWeaponManager;
static std::vector< CGameWeaponManager * > g_Managers;

//=========================================================
//=========================================================
class CGameWeaponManager : public CBaseEntity
{
	DECLARE_CLASS( CGameWeaponManager, CBaseEntity );
	DECLARE_DATADESC();

public:
	void Spawn_Impl();
	CGameWeaponManager()
	{
		m_flAmmoMod = 1.0f;
		m_bExpectingWeapon = false;
		g_Managers.push_back( this );
	}

	~CGameWeaponManager()
	{
		ks::erase_first( g_Managers, this );
	}

	void Think_Impl();
	[[= ks::reflect::Input{ .name = "SetMaxPieces", .type = FIELD_INTEGER } ]] void InputSetMaxPieces( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAmmoModifier", .type = FIELD_FLOAT } ]] void InputSetAmmoModifier( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "weaponname" } ]] string_t	m_iszWeaponName;
	[[= ks::reflect::Key{ .name = "maxpieces" } ]] int			m_iMaxPieces;
	[[= ks::reflect::Key{ .name = "ammomod" } ]] float		m_flAmmoMod;
	bool		m_bExpectingWeapon;

	std::vector< EHANDLE > m_ManagedNonWeapons;

};

IMPLEMENT_REFLECT_DATAMAP( CGameWeaponManager )

LINK_ENTITY_TO_CLASS( game_weapon_manager, CGameWeaponManager );

void CreateWeaponManager( const char *pWeaponName, int iMaxPieces )
{
	CGameWeaponManager *pManager = (CGameWeaponManager *)CreateEntityByName( "game_weapon_manager");

	if( pManager )
	{
		pManager->m_iszWeaponName = MAKE_STRING( pWeaponName );
		pManager->m_iMaxPieces = iMaxPieces;
		DispatchSpawn( pManager );
	}
}

void WeaponManager_AmmoMod( CBaseCombatWeapon *pWeapon )
{
	for ( int i = 0; i < std::ssize( g_Managers ); i++ )
	{
		if ( g_Managers[i]->m_iszWeaponName == pWeapon->m_iClassname )
		{
			int iNewClip = (int)(pWeapon->m_iClip1 * g_Managers[i]->m_flAmmoMod);
			int iNewRandomClip = iNewClip + RandomInt( -2, 2 );

			if ( iNewRandomClip > pWeapon->GetMaxClip1() )
			{
				iNewRandomClip = pWeapon->GetMaxClip1();
			}
			else if ( iNewRandomClip <= 0 )
			{
				//Drop at least one bullet.
				iNewRandomClip = 1;
			}

			pWeapon->m_iClip1 = iNewRandomClip;
		}
	}
}

void WeaponManager_AddManaged( CBaseEntity *pWeapon )
{
	for ( int i = 0; i < std::ssize( g_Managers ); i++ )
	{
		if ( g_Managers[i]->m_iszWeaponName == pWeapon->m_iClassname )
		{
			contract_assert( ks::find_index( g_Managers[i]->m_ManagedNonWeapons, pWeapon ) == -1 );
			g_Managers[i]->m_ManagedNonWeapons.push_back( pWeapon );
			break;
		}
	}
}

void WeaponManager_RemoveManaged( CBaseEntity *pWeapon )
{
	for ( int i = 0; i < std::ssize( g_Managers ); i++ )
	{
		if ( g_Managers[i]->m_iszWeaponName == pWeapon->m_iClassname )
		{
			int j = ks::find_index( g_Managers[i]->m_ManagedNonWeapons, pWeapon );
			if ( j != -1 )
			{
				ks::swap_remove( g_Managers[i]->m_ManagedNonWeapons, j );
			}
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGameWeaponManager::Spawn_Impl()
{
	SetThink( &CGameWeaponManager::Think );
	SetNextThink( gpGlobals->curtime );
	CBaseEntity *pEntity = CreateEntityByName( STRING(m_iszWeaponName) );
	if ( !pEntity )
	{
		DevMsg("%s removed itself!\n", GetDebugName() );
		UTIL_Remove(this);
	}
	else
	{
		m_bExpectingWeapon = ( entity_cast<CBaseCombatWeapon>(pEntity) != nullptr );
		UTIL_Remove(pEntity);
	}
}

//---------------------------------------------------------
// Count of all the weapons in the world of my type and
// see if we have a surplus. If there is a surplus, try
// to find suitable candidates for removal.
//
// Right now we just remove the first weapons we find that
// are behind the player, or are out of the player's PVS. 
// Later, we may want to score the results so that we
// removed the farthest gun that's not in the player's 
// viewcone, etc.
//
// Some notes and thoughts:
//
// This code is designed NOT to remove weapons that are 
// hand-placed by level designers. It should only clean
// up weapons dropped by dead NPCs, which is useful in
// situations where enemies are spawned in for a sustained
// period of time.
//
// Right now we PREFER to remove weapons that are not in the
// player's PVS, but this could be opposite of what we 
// really want. We may only want to conduct the cleanup on
// weapons that are IN the player's PVS.
//---------------------------------------------------------
void CGameWeaponManager::Think_Impl()
{
	int i;

	// Don't have to think all that often. 
	SetNextThink( gpGlobals->curtime + 2.0 );

	const char *pszWeaponName = STRING( m_iszWeaponName );

	std::vector< CBaseEntity * > candidates; candidates.reserve( 64 );

	if ( m_bExpectingWeapon )
	{
		CBaseCombatWeapon *pWeapon = nullptr;
		// Firstly, count the total number of weapons of this type in the world.
		// Also count how many of those can potentially be removed.
		pWeapon = static_cast<CBaseCombatWeapon *>(gEntList.FindEntityByClassname( pWeapon, pszWeaponName ));

		while( pWeapon )
		{
			if( !pWeapon->IsEffectActive( EF_NODRAW ) && pWeapon->IsRemoveable() )
			{
				candidates.push_back( pWeapon );
			}

			pWeapon = static_cast<CBaseCombatWeapon *>(gEntList.FindEntityByClassname( pWeapon, pszWeaponName ));
		}
	}
	else
	{
		for ( i = 0; i < std::ssize( m_ManagedNonWeapons ); i++)
		{
			CBaseEntity *pEntity = m_ManagedNonWeapons[i];
			if ( pEntity )
			{
				contract_assert( pEntity->m_iClassname == m_iszWeaponName );
				if ( !pEntity->IsEffectActive( EF_NODRAW ) )
				{
					candidates.push_back( pEntity );
				}
			}
			else
			{
				ks::swap_remove( m_ManagedNonWeapons, i-- );
			}
		}
	}

	// Calculate the surplus.
	int surplus = std::ssize( candidates ) - m_iMaxPieces;

	// Based on what the player can see, try to clean up the world by removing weapons that
	// the player cannot see right at the moment.
	CBaseEntity *pCandidate;
	for ( i = 0; i < std::ssize( candidates ) && surplus > 0; i++ )
	{
		bool fRemovedOne = false;

		pCandidate = candidates[i];
		contract_assert( !pCandidate->IsEffectActive( EF_NODRAW ) );

		if ( gpGlobals->maxClients == 1 )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			// Nodraw serves as a flag that this weapon is already being removed since
			// all we're really doing inside this loop is marking them for removal by
			// the entity system. We don't want to count the same weapon as removed
			// more than once.
			if( !UTIL_FindClientInPVS( pCandidate->edict() ) )
			{
				fRemovedOne = true;
			}
			else if( !pPlayer->FInViewCone( pCandidate ) )
			{
				fRemovedOne = true;
			}
			else if ( UTIL_DistApprox( pPlayer->GetAbsOrigin(), pCandidate->GetAbsOrigin() ) > (30*12) )
			{
				fRemovedOne = true;
			}
		}
		else
		{
			fRemovedOne = true;
		}

		if( fRemovedOne )
		{
			pCandidate->AddEffects( EF_NODRAW );
			UTIL_Remove( pCandidate );

			DevMsg( 2, "Surplus %s removed\n", pszWeaponName);
			surplus--;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGameWeaponManager::InputSetMaxPieces( inputdata_t &inputdata )
{
	m_iMaxPieces = inputdata.value.Int();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CGameWeaponManager::InputSetAmmoModifier( inputdata_t &inputdata )
{
	m_flAmmoMod = inputdata.value.Float();
}

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CBreakable;
class CSprite;

using namespace ::ks::reflect;

// Purpose: A gib is a chunk of a body, or a piece of wood/metal/rocks/etc.

extern Vector			g_vecAttackDir;		// In globals.cpp

std::vector< EHANDLE > CGib::s_ExtantGibs;

// HACKHACK -- The gib velocity equations don't work
void CGib::LimitVelocity( void )
{
	Vector vecNewVelocity = GetAbsVelocity();
	float length = VectorNormalize( vecNewVelocity );

	// ceiling at 1500.  The gib velocity equation is not bounded properly.  Rather than tune it
	// in 3 separate places again, I'll just limit it here.
	if ( length > 1500.0 )
	{
		vecNewVelocity *= 1500;		// This should really be sv_maxvelocity * 0.75 or something
		SetAbsVelocity( vecNewVelocity );
	}
}

void CGib::SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs )
{
	int i;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN )
	{
		// no sticky gibs in germany right now!
		return; 
	}

	for ( i = 0 ; i < cGibs ; i++ )
	{
		CGib *pGib = (CGib *)CreateEntityByName( "gib" );

		pGib->Spawn( "models/stickygib.mdl" );
		pGib->m_nBody = random_valve->RandomInt(0,2);

		if ( pVictim )
		{
			pGib->SetLocalOrigin(
				Vector( vecOrigin.x + random_valve->RandomFloat( -3, 3 ),
						vecOrigin.y + random_valve->RandomFloat( -3, 3 ),
						vecOrigin.z + random_valve->RandomFloat( -3, 3 ) ) );

			// make the gib fly away from the attack vector
			Vector vecNewVelocity = g_vecAttackDir * -1;

			// mix in some noise
			vecNewVelocity.x += random_valve->RandomFloat ( -0.15, 0.15 );
			vecNewVelocity.y += random_valve->RandomFloat ( -0.15, 0.15 );
			vecNewVelocity.z += random_valve->RandomFloat ( -0.15, 0.15 );

			vecNewVelocity *= 900;

			QAngle vecAngVelocity( random_valve->RandomFloat ( 250, 400 ), random_valve->RandomFloat ( 250, 400 ), 0 );
			pGib->SetLocalAngularVelocity( vecAngVelocity );

			// copy owner's blood color
			pGib->SetBloodColor( pVictim->BloodColor() );
		
			pGib->AdjustVelocityBasedOnHealth( pVictim->m_iHealth, vecNewVelocity );
			pGib->SetAbsVelocity( vecNewVelocity );
			
			pGib->SetMoveType( MOVETYPE_FLYGRAVITY );
			pGib->RemoveSolidFlags( FSOLID_NOT_SOLID );
			pGib->SetCollisionBounds( vec3_origin, vec3_origin );
			pGib->SetTouch ( &CGib::StickyGibTouch );
			pGib->SetThink (nullptr);
		}
		pGib->LimitVelocity();
	}
}

void CGib::SpawnHeadGib( CBaseEntity *pVictim )
{
	CGib *pGib = CREATE_ENTITY( CGib, "gib" );

	if ( g_Language.GetInt() == LANGUAGE_GERMAN )
	{
		pGib->Spawn( "models/germangibs.mdl" );// throw one head
		pGib->m_nBody = 0;
	}
	else
	{
		pGib->Spawn( "models/gibs/hgibs.mdl" );// throw one head
		pGib->m_nBody = 0;
	}

	if ( pVictim )
	{
		Vector vecNewVelocity = pGib->GetAbsVelocity();

		pGib->SetLocalOrigin( pVictim->EyePosition() );
		
		edict_t *pentPlayer = UTIL_FindClientInPVS( pGib->edict() );
		
		if ( random_valve->RandomInt ( 0, 100 ) <= 5 && pentPlayer )
		{
			// 5% chance head will be thrown at player's face.
			CBasePlayer *player = (CBasePlayer *)CBaseEntity::Instance( pentPlayer );
			if ( player )
			{
				vecNewVelocity = ( player->EyePosition() ) - pGib->GetAbsOrigin();
				VectorNormalize(vecNewVelocity);
				vecNewVelocity *= 300;
				vecNewVelocity.z += 100;
			}
		}
		else
		{
			vecNewVelocity = Vector (random_valve->RandomFloat(-100,100), random_valve->RandomFloat(-100,100), random_valve->RandomFloat(200,300));
		}

		QAngle vecNewAngularVelocity = pGib->GetLocalAngularVelocity();
		vecNewAngularVelocity.x = random_valve->RandomFloat ( 100, 200 );
		vecNewAngularVelocity.y = random_valve->RandomFloat ( 100, 300 );
		pGib->SetLocalAngularVelocity( vecNewAngularVelocity );

		// copy owner's blood color
		pGib->SetBloodColor( pVictim->BloodColor() );
		pGib->AdjustVelocityBasedOnHealth( pVictim->m_iHealth, vecNewVelocity );
		pGib->SetAbsVelocity( vecNewVelocity );
	}
	pGib->LimitVelocity();
}

//-----------------------------------------------------------------------------
// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
//-----------------------------------------------------------------------------
void CGib::SetBloodColor( int nBloodColor )
{
	m_bloodColor = nBloodColor;
}

//------------------------------------------------------------------------------
// A little piece of duplicated code
//------------------------------------------------------------------------------
void CGib::AdjustVelocityBasedOnHealth( int nHealth, Vector &vecVelocity )
{
	if ( nHealth > -50)
	{
		vecVelocity *= 0.7;
	}
	else if ( nHealth > -200)
	{
		vecVelocity *= 2;
	}
	else
	{
		vecVelocity *= 4;
	}
}

//------------------------------------------------------------------------------
// Purpose : Initialize a gibs position and velocity
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGib::InitGib( CBaseEntity *pVictim, float fMinVelocity, float fMaxVelocity )
{
	// ------------------------------------------------------------------------
	// If have a pVictim spawn the gib somewhere in the pVictim's bounding volume
	// ------------------------------------------------------------------------
	if ( pVictim )
	{
		// Find a random position within the bounding box (add 1 to Z to get it out of the ground)
		Vector vecOrigin;
		pVictim->CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1, 1, 1 ), &vecOrigin );
		vecOrigin.z += 1.0f;
		SetAbsOrigin( vecOrigin );	

		// make the gib fly away from the attack vector
		Vector vecNewVelocity =	 g_vecAttackDir * -1;

		// mix in some noise
		vecNewVelocity.x += random_valve->RandomFloat ( -0.25, 0.25 );
		vecNewVelocity.y += random_valve->RandomFloat ( -0.25, 0.25 );
		vecNewVelocity.z += random_valve->RandomFloat ( -0.25, 0.25 );

		vecNewVelocity *= random_valve->RandomFloat ( fMaxVelocity, fMinVelocity );

		QAngle vecNewAngularVelocity = GetLocalAngularVelocity();
		vecNewAngularVelocity.x = random_valve->RandomFloat ( 100, 200 );
		vecNewAngularVelocity.y = random_valve->RandomFloat ( 100, 300 );
		SetLocalAngularVelocity( vecNewAngularVelocity );
		
		// copy owner's blood color
		SetBloodColor( pVictim->BloodColor() );
		
		AdjustVelocityBasedOnHealth( pVictim->m_iHealth, vecNewVelocity );

		// Attempt to be physical if we can
		if ( VPhysicsInitNormal( SOLID_BBOX, 0, false ) )
		{
			IPhysicsObject *pObj = VPhysicsGetObject();

			if ( pObj != nullptr )
			{
				AngularImpulse angImpulse = RandomAngularImpulse( -500, 500 );
				pObj->AddVelocity( &vecNewVelocity, &angImpulse );
			}
		}
		else
		{
			SetSolid( SOLID_BBOX );
			SetCollisionBounds( vec3_origin, vec3_origin );
			SetAbsVelocity( vecNewVelocity );
		}
	
		SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	}

	LimitVelocity();
}

//------------------------------------------------------------------------------
// Purpose : Given an .mdl file with gibs and the number of gibs in the file
//			 spawns them in pVictim's bounding box
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGib::SpawnSpecificGibs(	CBaseEntity*	pVictim, 
								int				nNumGibs, 
								float			vMinVelocity, 
								float			vMaxVelocity, 
								const char*		cModelName,
								float			flLifetime)
{
	for (int i=0;i<nNumGibs;i++)
	{
		CGib *pGib = CREATE_ENTITY( CGib, "gib" );
		pGib->Spawn( cModelName );
		pGib->m_nBody = i;
		pGib->InitGib( pVictim, vMinVelocity, vMaxVelocity );
		pGib->m_lifeTime = flLifetime;
		
		if ( pVictim != nullptr )
		{
			pGib->SetOwnerEntity( pVictim );
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Spawn random gibs of the given gib type
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGib::SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, GibType_e eGibType )
{
	int cSplat;

	for ( cSplat = 0 ; cSplat < cGibs ; cSplat++ )
	{
		CGib *pGib = CREATE_ENTITY( CGib, "gib" );

		if ( g_Language.GetInt() == LANGUAGE_GERMAN )
		{
			pGib->Spawn( "models/germangibs.mdl" );
			pGib->m_nBody = random_valve->RandomInt(0,GERMAN_GIB_COUNT-1);
		}
		else
		{
			switch (eGibType)
			{
			case GIB_HUMAN:
				// human pieces
				pGib->Spawn( "models/gibs/hgibs.mdl" );
				pGib->m_nBody = random_valve->RandomInt(1,HUMAN_GIB_COUNT-1);// start at one to avoid throwing random amounts of skulls (0th gib)
				break;
			case GIB_ALIEN:
				// alien pieces
				pGib->Spawn( "models/gibs/agibs.mdl" );
				pGib->m_nBody = random_valve->RandomInt(0,ALIEN_GIB_COUNT-1);
				break;
			}
		}
		pGib->InitGib( pVictim, 300, 400);
	}
}

//=========================================================
// WaitTillLand - in order to emit their meaty scent from
// the proper location, gibs should wait until they stop 
// bouncing to emit their scent. That's what this function
// does.
//=========================================================
void CGib::WaitTillLand ( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( GetAbsVelocity() == vec3_origin )
	{
		SetRenderAlpha( 255 );
		m_nRenderMode = kRenderTransTexture;
		if ( GetMoveType() != MOVETYPE_VPHYSICS )
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
		}
		SetLocalAngularVelocity( vec3_angle );

		SetNextThink( gpGlobals->curtime + m_lifeTime );
		SetThink ( &CGib::SUB_FadeOut );

		if ( GetSprite() )
		{
			CSprite *pSprite = dynamic_cast<CSprite*>( GetSprite() );

			if ( pSprite )
			{
				//Adrian - Why am I doing this? Check InitPointGib for the answer!
				if ( m_lifeTime == 0 )
					m_lifeTime = random_valve->RandomFloat( 1, 3 );

				pSprite->FadeAndDie( m_lifeTime );
			}
		}

		if ( GetFlame() )
		{
			CEntityFlame *pFlame = dynamic_cast< CEntityFlame*>( GetFlame() );

			if ( pFlame )
			{
				pFlame->SetLifetime( 1.0f );
			}
		}

		// If you bleed, you stink!
		if ( m_bloodColor != DONT_BLEED )
		{
			// ok, start stinkin!
			// FIXME: It's too easy to fill up the sound queue with all these meat sounds
			// CSoundEnt::InsertSound ( SOUND_MEAT, GetAbsOrigin(), 384, 25 );
		}
	}
	else
	{
		// wait and check again in another half second.
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
}

bool CGib::SUB_AllowedToFade_Impl( void )
{
	if( VPhysicsGetObject() )
	{
		if( VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD || GetEFlags() & EFL_IS_BEING_LIFTED_BY_BARNACLE )
			return false;
	}

	CBasePlayer *pPlayer = ( UTIL_IsSinglePlayer() ) ? UTIL_GetLocalPlayer() : nullptr;

	if ( pPlayer && pPlayer->FInViewCone( this ) && m_bForceRemove == false )
	{
		// We don't want Gibs to fade out while onscreen... but we also can't allow an infinite
		// number of gibs accumulate, so let the oldest ones fade out if we're over the limit.
		int nGibsToKill = std::ssize( s_ExtantGibs ) - MAX_CONCURRENT_GIBS;
		for ( int i = 0; i < nGibsToKill; i++ )
		{
			if ( this == s_ExtantGibs[ i ] )
				return true;
		}
		return false;
	}

	return true;
}

void CGib::DieThink ( void )
{
	if ( GetSprite() )
	{
		CSprite *pSprite = dynamic_cast<CSprite*>( GetSprite() );

		if ( pSprite )
		{
			pSprite->FadeAndDie( 0.0 );
		}
	}

	if ( GetFlame() )
	{
		CEntityFlame *pFlame = dynamic_cast< CEntityFlame*>( GetFlame() );

		if ( pFlame )
		{
			pFlame->SetLifetime( 1.0f );
		}
	}

	if ( g_pGameRules->IsMultiplayer() )
	{
		UTIL_Remove( this );
	}
	else
	{
		SetThink ( &CGib::SUB_FadeOut );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGib::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer )
	{
		pPlayer->PickupObject( this );
	}
}

//-----------------------------------------------------------------------------
// Physics Attacker
//-----------------------------------------------------------------------------
void CGib::SetPhysicsAttacker( CBasePlayer *pEntity, float flTime )
{
	m_hPhysicsAttacker = pEntity;
	m_flLastPhysicsInfluenceTime = flTime;
}

//-----------------------------------------------------------------------------
// Purpose: Keep track of physgun influence
//-----------------------------------------------------------------------------
void CGib::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	SetPhysicsAttacker( pPhysGunUser, gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGib::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason )
{
	SetPhysicsAttacker( pPhysGunUser, gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBasePlayer *CGib::HasPhysicsAttacker_Impl( float dt )
{
	if (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime)
	{
		return m_hPhysicsAttacker;
	}
	return nullptr;
}

//
// Gib bounces on the ground or wall, sponges some blood down, too!
//
void CGib::BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	trace_t	tr;
	
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	if ( pPhysics )
		 return;
	
	//if ( random->RandomInt(0,1) )
	//	return;// don't bleed everytime
	if (GetFlags() & FL_ONGROUND)
	{
		SetAbsVelocity( GetAbsVelocity() * 0.9 );
		QAngle angles = GetLocalAngles();
		angles.x = 0;
		angles.z = 0;
		SetLocalAngles( angles );

		QAngle angVel = GetLocalAngularVelocity();
		angVel.x = 0;
		angVel.z = 0;
		SetLocalAngularVelocity( vec3_angle );
	}
	else
	{
		if ( g_Language.GetInt() != LANGUAGE_GERMAN && m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

			UTIL_BloodDecalTrace( &tr, m_bloodColor );

			m_cBloodDecals--; 
		}

		if ( m_material != matNone && random_valve->RandomInt(0,2) == 0 )
		{
			float volume;
			float zvel = fabs(GetAbsVelocity().z);
		
			volume = 0.8f * std::min<std::common_type_t<double, float>>(1.0, ((float)zvel) / 450.0f);

			CBreakable::MaterialSoundRandom( entindex(), (Materials)m_material, volume );
		}
	}
}

//
// Sticky gib puts blood on the wall and stays put. 
//
void CGib::StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	trace_t tr;
	
	SetThink ( &CGib::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 10 );

	if ( !FClassnameIs( pOther, "worldspawn" ) )
	{
		SetNextThink( gpGlobals->curtime );
		return;
	}

	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * 32,  MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	Vector vecForward = tr.plane.normal * -1;
	QAngle angles;
	VectorAngles( vecForward, angles );
	SetLocalAngles( angles );
	SetAbsVelocity( vec3_origin ); 
	SetLocalAngularVelocity( vec3_angle );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// CGib destructor
//-----------------------------------------------------------------------------
CGib::~CGib( void )
{
	// NOTE: this should not be slow, because MAX_CONCURRENT_GIBS should not be large
	int nIndex = ks::find_index( s_ExtantGibs, this );
	if ( nIndex >= 0 )
		s_ExtantGibs.erase( s_ExtantGibs.begin() + nIndex );
	else
		ExecuteNTimes( 20, [&]{ Warning( "CGibs are being created which circumvent Spawn() - this can result in a leak.\n" ); } );
}

//-----------------------------------------------------------------------------
// Spawn a gib with a finite lifetime, after which it will fade out.
//-----------------------------------------------------------------------------
void CGib::Spawn( const char *szGibModel, float flLifetime )
{
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetFriction(0.55); // deading the bounce a bit
	
	// sometimes an entity inherits the edict from a former piece of glass,
	// and will spawn using the same render FX or m_nRenderMode! bad!
	SetRenderAlpha( 255 );
	m_nRenderMode = kRenderNormal;
	m_nRenderFX = kRenderFxNone;
	
	// hopefully this will fix the VELOCITY TOO LOW crap
	m_takedamage = DAMAGE_EVENTS_ONLY;
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	SetModel( szGibModel );

	SetNextThink( gpGlobals->curtime + 4 );
	m_lifeTime = flLifetime;
	SetTouch ( &CGib::BounceGibTouch );

    m_bForceRemove = false;

	m_material = matNone;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain). 

	// Make sure the gib dies within its alotted lifetime
	s_ExtantGibs.push_back( this );
	SetThink ( &CGib::SUB_FadeOut );
	SetNextThink( gpGlobals->curtime + m_lifeTime );
}

LINK_ENTITY_TO_CLASS( gib, CGib );

CBaseEntity *CreateRagGib( const char *szModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecForce, float flFadeTime, bool bShouldIgnite )
{
	CRagGib *pGib;

	pGib = (CRagGib*)CreateEntityByName( "raggib" );

	pGib->SetLocalAngles( vecAngles );

	if ( !pGib )
	{
		Msg( "**Can't create ragdoll gib!\n" );
		return nullptr;
	}

	if ( bShouldIgnite )
	{
		CBaseAnimating *pAnimating = pGib->GetBaseAnimating();
		if (pAnimating != nullptr )
		{
			pAnimating->Ignite( random_valve->RandomFloat( 8.0, 12.0 ), false );
		}
	}

	pGib->Spawn( szModel, vecOrigin, vecForce, flFadeTime );

	return pGib;
}

void CRagGib::Spawn( const char *szModel, const Vector &vecOrigin, const Vector &vecForce, float flFadeTime = 0.0 )
{
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetModel( szModel );
	UTIL_SetSize(this, vec3_origin, vec3_origin);
	UTIL_SetOrigin( this, vecOrigin );
	if ( !BecomeRagdollOnClient( vecForce ) )
	{
		AddSolidFlags( FSOLID_NOT_STANDABLE );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		if( flFadeTime > 0.0 )
		{
			SUB_StartFadeOut( flFadeTime );
		}
	}
}

LINK_ENTITY_TO_CLASS( raggib, CRagGib );

using namespace ::ks::reflect;

// Purpose: work in progress

CGlobalEventLog	GlobalEventLog;

static	ks::intern_pool	EventSymbols;
static	ConVar			global_event_log_enabled( side_registry(), "global_event_log_enabled", "0", FCVAR_CHEAT, "Enables the global event log system" );

class CGlobalEventLine
{
public:
	CGlobalEventLine( );
	~CGlobalEventLine( );

	void	Clear( );
	bool	SetStaticText( const char *pszValue );
	bool	SetVaryingText( const char *pszValue );

	bool	IsDirty( ) { return m_bDirty; }

	void	Write( ks::text_writer &buffer );
	void	ClearDirty( );		

private:
	std::string_view	m_ValueSymbol;	// view into EventSymbols
	char		*m_pszValue;
	bool		m_bDirty;
};

CGlobalEventLine::CGlobalEventLine( )
{
	m_pszValue = nullptr;
	m_bDirty = true;
}

CGlobalEventLine::~CGlobalEventLine( )
{
	Clear();
}

void CGlobalEventLine::Clear( )
{
	m_ValueSymbol = {};
	if ( m_pszValue != nullptr )
	{
		MemAlloc_Free( m_pszValue );
		m_pszValue = nullptr;
	}
	m_bDirty = true;
}

bool CGlobalEventLine::SetStaticText( const char *pszValue )
{
	if ( m_ValueSymbol.data() && m_ValueSymbol == pszValue )
	{
		return false;
	}

	Clear();

	m_ValueSymbol = EventSymbols.intern( pszValue );

	return true;
}

bool CGlobalEventLine::SetVaryingText( const char *pszValue )
{
	if ( m_pszValue && strcmpi( m_pszValue, pszValue ) == 0 )
	{	// no change
		return false;
	}

	Clear();

	m_pszValue = ( char * )MemAlloc_Alloc( std::strlen( pszValue ) + 1 );
	std::strcpy( m_pszValue, pszValue );

	return true;
}

void CGlobalEventLine::Write( ks::text_writer &buffer )
{
	const char *pszValue;

	if ( m_pszValue != nullptr )
	{
		pszValue = m_pszValue;
	}
	else
	{
		pszValue = ks::nz( m_ValueSymbol.data() );
	}

	if ( std::strchr( pszValue, ' ' ) != nullptr )
	{
		buffer.printf( "\"%s\"\n", pszValue );
	}
	else
	{
		buffer.printf( "%s\n", pszValue );
	}
}

void CGlobalEventLine::ClearDirty( )
{
	m_bDirty = false;
}

class CGlobalEvent
{
public:
	CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent = nullptr );

	bool			AddValue( bool bVarying, const char *pszKey, const char *pszValue );

	unsigned int	GetID( ) { return m_nID; }
	bool			IsDirty( ) { return m_bDirty; }

	void			Write( ks::text_writer &buffer );
	void			ClearDirty( );		

private:
	unsigned int								m_nID;
	std::string_view							m_Name;	// view into EventSymbols
	float										m_flTime;
	bool										m_bIsHighLevel;
	bool										m_bFullUpdate;
	bool										m_bDirty;
	CGlobalEvent								*m_pParent;
	// Keys view EventSymbols; lines are written in key order.
	std::map< std::string_view, std::unique_ptr< CGlobalEventLine >, std::less<> >	m_EventLines;
};

CGlobalEvent::CGlobalEvent( const char *pszName, unsigned int nID, bool bIsHighLevel, CGlobalEvent *pParent )
{
	m_Name = EventSymbols.intern( pszName );
	m_nID = nID;
	m_bIsHighLevel = bIsHighLevel;
	m_pParent = pParent;
	m_bFullUpdate = true;
	m_flTime = gpGlobals->curtime;
}

bool CGlobalEvent::AddValue( bool bVarying, const char *pszKey, const char *pszValue )
{
	std::string_view	KeyID;
	CGlobalEventLine	*pEvent;
	bool				bResult;

	KeyID = EventSymbols.intern( pszKey );
	std::unique_ptr< CGlobalEventLine > &pLine = m_EventLines[ KeyID ];
	if ( !pLine )
	{
		pLine = std::make_unique< CGlobalEventLine >();
	}
	pEvent = pLine.get();

	if ( bVarying )
	{
		bResult = pEvent->SetVaryingText( pszValue );
	}
	else
	{
		bResult = pEvent->SetStaticText( pszValue );
	}

	if ( bResult == true )
	{
		m_bDirty = true;
		m_flTime = gpGlobals->curtime;
	}

	return bResult;
}

void CGlobalEvent::Write( ks::text_writer &buffer )
{
	buffer.printf( "event %u\n", m_nID );
	buffer.printf( "{\n" );

	if ( m_bFullUpdate )
	{
		const char *pszName = m_Name.data();

		if ( std::strchr( pszName, ' ' ) != nullptr )
		{
			buffer.printf( "\tName\t\"%s\"\n", pszName );
		}
		else
		{
			buffer.printf( "\tName\t%s\n", pszName );
		}
		if ( m_pParent != nullptr )
		{
			buffer.printf( "\tParent_ID\t%u\n", m_pParent->GetID() );
		}
		if ( m_bIsHighLevel == true )
		{
			buffer.printf( "\tHighLevel\t1\n" );
		}
	}

	buffer.printf( "\tTime\t%g\n", m_flTime );

	for ( auto &[key, pLine] : m_EventLines )
	{
		if ( pLine->IsDirty() )
		{
			const char *pszKey = key.data();

			if ( std::strchr( pszKey, ' ' ) != nullptr )
			{
				buffer.printf( "\t\"%s\"\t", pszKey );
			}
			else
			{
				buffer.printf( "\t%s\t", pszKey );
			}
			pLine->Write( buffer );
		}
	}
	buffer.printf( "}\n" );
}

void CGlobalEvent::ClearDirty( )
{
	m_bFullUpdate = false;
	m_bDirty = false;
	for ( auto &[key, pLine] : m_EventLines )
	{
		pLine->ClearDirty();
	}
}

CGlobalEventLog::CGlobalEventLog( )
{
	m_nNextID = 1;
}

CGlobalEvent *CGlobalEventLog::GetGlobalEvent( EGlobalEvent GlobalEvent )
{
	return m_pGlobalEvents[ GlobalEvent ];
}

CGlobalEvent *CGlobalEventLog::CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, bIsHighLevel, pParent );

	m_Events.push_back( pEvent );
	m_DirtyEvents.push_back( pEvent );
	m_nNextID++;

	return pEvent;
}

CGlobalEvent *CGlobalEventLog::CreateTempEvent( const char *pszName, CGlobalEvent *pParent )
{
	CGlobalEvent	*pEvent = new CGlobalEvent( pszName, m_nNextID, false, pParent );

	m_TempEvents.push_back( pEvent );
	m_DirtyEvents.push_back( pEvent );
	m_nNextID++;

	return pEvent;
}

void CGlobalEventLog::RemoveEvent( CGlobalEvent *pEvent )
{
	if ( ks::erase_first( m_Events, pEvent ) == true && ks::find_index( m_TempEvents, pEvent ) == -1 )
	{
		m_TempEvents.push_back( pEvent );
	}
}

void CGlobalEventLog::AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... )
{
	ks::ScratchArena<> scratch;
	std::va_list		Args;
	int			nLen;
	char		*pszBuffer;
	bool		bResult;

	__builtin_va_start( Args, pszValueFormat );

	nLen = std::vsnprintf( nullptr, 0, pszValueFormat, Args ) + 1;
	pszBuffer = scratch.Alloc<char>( nLen );
	V_vsnprintf( pszBuffer, nLen, pszValueFormat, Args );
	
	bResult = pEvent->AddValue( bVarying, pszKey, pszBuffer );

	if ( bResult == true && ks::find_index( m_DirtyEvents, pEvent ) == -1 )
	{
		m_DirtyEvents.push_back( pEvent );
	}
}

void CGlobalEventLog::SendUpdate( )
{
	if ( m_DirtyEvents.empty() )
	{
		return;
	}

	if ( global_event_log_enabled.GetBool() == true )
	{
		FileHandle_t fh = g_pFullFileSystem->Open( "c:\\o.events", "a" );
		ks::text_writer buffer;

		for( int i = 0; i < std::ssize( m_DirtyEvents ); i++ )
		{
			m_DirtyEvents[ i ]->Write( buffer );

			g_pFullFileSystem->Write( buffer.c_str(), buffer.size(), fh );
			buffer.clear();
		}

		g_pFullFileSystem->Close( fh );
	}

	for( int i = 0; i < std::ssize( m_DirtyEvents ); i++ )
	{
		m_DirtyEvents[ i ]->ClearDirty();
	}
	m_DirtyEvents.clear(); m_DirtyEvents.shrink_to_fit();

	for( int i = 0; i < std::ssize( m_TempEvents ); i++ )
	{
		delete m_TempEvents[ i ];
	}
	m_TempEvents.clear(); m_TempEvents.shrink_to_fit();
}

void CGlobalEventLog::PostInit( )
{
	m_pGlobalEvents[ GLOBAL_EVENT_NPCS ] = CreateEvent( "NPCs", true );
}

void CGlobalEventLog::FrameUpdatePostEntityThink( )
{
	SendUpdate();
}

using namespace ::ks::reflect;

/*

===== globals.cpp ========================================================

  DLL-wide global variable definitions.
  They're all defined here, for convenient centralization.
  Source files that need them should "extern ..." declare each
  variable, to better document what globals they care about.

*/

Vector			g_vecAttackDir(0.0f, 0.0f, 0.0f);
int				g_iSkillLevel = 0;
bool			g_fGameOver = false;

using namespace ::ks::reflect;

struct globalentity_t
{
	DECLARE_SIMPLE_DATADESC();

	// Views into CGlobalState::m_nameList.
	std::string_view	name;
	std::string_view	levelName;
	GLOBALESTATE	state;
	int				counter;
};

class CGlobalState : public CAutoGameSystem
{
public:
	CGlobalState( char const *name ) : CAutoGameSystem( name ), m_disableStateUpdates(false)
	{
	}

	// IGameSystem
	virtual void LevelShutdownPreEntity() 
	{
		// don't allow state updates during shutdowns
		contract_assert( !m_disableStateUpdates );
		m_disableStateUpdates = true;
	}
	
	virtual void LevelShutdownPostEntity() 
	{
		contract_assert( m_disableStateUpdates );
		m_disableStateUpdates = false;
	}

	void EnableStateUpdates( bool bEnable )
	{
		m_disableStateUpdates = !bEnable;
	}

	void SetState( int globalIndex, GLOBALESTATE state )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].state = state;
	}

	GLOBALESTATE GetState( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return GLOBAL_OFF;
		return m_list[globalIndex].state;
	}

	void SetCounter( int globalIndex, int counter )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].counter = counter;
	}

	int AddToCounter( int globalIndex, int delta )
	{
		if ( m_disableStateUpdates || !ks::is_valid_index( m_list, globalIndex ) )
			return 0;
		return ( m_list[globalIndex].counter += delta );
	}

	int GetCounter( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return 0;
		return m_list[globalIndex].counter;
	}

	void SetMap( int globalIndex, string_t mapname )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return;
		m_list[globalIndex].levelName = m_nameList.intern( STRING(mapname) );
	}

	const char *GetMap( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return nullptr;
		return m_list[globalIndex].levelName.data();
	}

	const char *GetName( int globalIndex )
	{
		if ( !ks::is_valid_index( m_list, globalIndex ) )
			return nullptr;
		return m_list[globalIndex].name.data();
	}

	int GetIndex( const char *pGlobalname )
	{
		std::string_view symName = m_nameList.find( pGlobalname );

		if ( symName.data() )
		{
			for ( int i = std::ssize( m_list ) - 1; i >= 0; --i )
			{
				// Same pool, so identity is equality.
				if ( m_list[i].name.data() == symName.data() )
					return i;
			}
		}

		return -1;
	}

	int AddEntity( const char *pGlobalname, const char *pMapName, GLOBALESTATE state )
	{
		globalentity_t entity;
		entity.name = m_nameList.intern( pGlobalname );
		entity.levelName = m_nameList.intern( pMapName );
		entity.state = state;

		int index = GetIndex( entity.name.data() );
		if ( index >= 0 )
			return index;
		m_list.push_back( entity ); return (int)std::ssize( m_list ) - 1;
	}

	int GetNumGlobals( void )
	{
		return std::ssize( m_list );
	}

	void			Reset( void );
	DECLARE_SIMPLE_DATADESC();

//#ifdef _DEBUG
	void			DumpGlobals( void );
//#endif

public:
	ks::intern_pool	m_nameList;
private:
	bool			m_disableStateUpdates;
	std::vector< globalentity_t > m_list;
};

static CGlobalState gGlobalState( "CGlobalState" );

void GlobalEntity_SetState( int globalIndex, GLOBALESTATE state )
{
	gGlobalState.SetState( globalIndex, state );
}

void GlobalEntity_SetCounter( int globalIndex, int counter )
{
	gGlobalState.SetCounter( globalIndex, counter );
}

int GlobalEntity_AddToCounter( int globalIndex, int delta )
{
	return gGlobalState.AddToCounter( globalIndex, delta );
}

void GlobalEntity_SetFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, flags );
}

void GlobalEntity_AddFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, gGlobalState.GetCounter( globalIndex ) | flags );
}

void GlobalEntity_RemoveFlags( int globalIndex, int flags )
{
	// Flags are stored in counter var
	gGlobalState.SetCounter( globalIndex, gGlobalState.GetCounter( globalIndex ) & ~flags );
}

void GlobalEntity_EnableStateUpdates( bool bEnable )
{
	gGlobalState.EnableStateUpdates( bEnable );
}

void GlobalEntity_SetMap( int globalIndex, string_t mapname )
{
	gGlobalState.SetMap( globalIndex, mapname );
}

int GlobalEntity_Add( const char *pGlobalname, const char *pMapName, GLOBALESTATE state )
{
	return gGlobalState.AddEntity( pGlobalname, pMapName, state );
}

int GlobalEntity_GetIndex( const char *pGlobalname )
{
	return gGlobalState.GetIndex( pGlobalname );
}

GLOBALESTATE GlobalEntity_GetState( int globalIndex )
{
	return gGlobalState.GetState( globalIndex );
}

int GlobalEntity_GetCounter( int globalIndex )
{
	return gGlobalState.GetCounter( globalIndex );
}

int GlobalEntity_GetFlags( int globalIndex )
{
	// Flags are stored in counter var
	return gGlobalState.GetCounter( globalIndex );
}

const char *GlobalEntity_GetMap( int globalIndex )
{
	return gGlobalState.GetMap( globalIndex );
}

const char *GlobalEntity_GetName( int globalIndex )
{
	return gGlobalState.GetName( globalIndex );
}

int GlobalEntity_GetNumGlobals( void )
{
	return gGlobalState.GetNumGlobals();
}

static void dump_globals( const CCommand &args );
static ConCommand dump_globals_command( side_registry(), "dump_globals", dump_globals, "Dump all global entities/states" );
static void dump_globals( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	gGlobalState.DumpGlobals();
}

// This is available all the time now on impulse 104, remove later
//#ifdef _DEBUG
void CGlobalState::DumpGlobals( void )
{
	static const char *estates[] = { "Off", "On", "Dead" };

	Msg( "-- Globals --\n" );
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		Msg( "%s: %s (%s) = %d\n", m_list[i].name.data(), m_list[i].levelName.data(), estates[m_list[i].state], m_list[i].counter );
	}
}
//#endif

// Global state Savedata 
IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CGlobalState )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( globalentity_t )

void CGlobalState::Reset( void )
{
	m_list.clear(); m_list.shrink_to_fit();
	m_nameList.clear();
}

//-----------------------------------------------------------------------------
// Purpose: This gets called when a level is shut down
//-----------------------------------------------------------------------------

void ResetGlobalState( void )
{
	gGlobalState.Reset();
}

void ShowServerGameTime()
{
	Msg( "Server game time: %f\n", gpGlobals->curtime );
}

static void server_game_time( const CCommand &args );
static ConCommand server_game_time_command( side_registry(), "server_game_time", server_game_time, "Gives the game time in seconds (server's curtime)" );
static void server_game_time( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	ShowServerGameTime();
}

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements a moving target that moves along a path of path_tracks
//			and can be shot and killed. When the target it killed it fires an
//			OnDeath output.
//			m_flSpeed is the travel speed
//			m_iHealth is current health
//			m_iMaxHealth is the amount to reset to each time it starts

static constexpr auto FGUNTARGET_START_ON = 0x0001;

class CGunTarget : public CBaseToggle
{
	DECLARE_CLASS( CGunTarget, CBaseToggle );

public:

	void Spawn_Impl( void );
	void Activate_Impl( void );
	bool CreateVPhysics_Impl( void );

	int BloodColor_Impl( void ) { return DONT_BLEED; }

	Class_T Classify_Impl( void ) { return CLASS_NONE; }
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	Vector BodyTarget_Impl( const Vector &posSrc, bool bNoisy = true ) { return GetAbsOrigin(); }

	// Input handlers
	[[= ks::reflect::Input{ .name = "Start", .type = FIELD_VOID } ]] void InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop", .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:

	void Next( void );
	void Start( void );
	void Wait( void );
	void Stop( void );

private:

	bool			m_on;
	EHANDLE			m_hTargetEnt;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnDeath" } ]] COutputEvent	m_OnDeath;
};

LINK_ENTITY_TO_CLASS( func_guntarget, CGunTarget );

IMPLEMENT_REFLECT_DATAMAP( CGunTarget )

void CGunTarget::Spawn_Impl( void )
{
	SetSolid( SOLID_BSP );
	SetMoveType( MOVETYPE_PUSH );

	SetModel( STRING( GetModelName() ) );

	if ( m_flSpeed == 0 )
		m_flSpeed = 100;

	// Don't take damage until "on"
	m_takedamage = DAMAGE_NO;
	AddFlag( FL_NPC );

	m_on = false;
	m_iMaxHealth = m_iHealth;

	if ( HasSpawnFlags(FGUNTARGET_START_ON) )
	{
		SetMoveDone( &CGunTarget::Start );
		SetMoveDoneTime( 0.3 );
	}
	CreateVPhysics();
}

bool CGunTarget::CreateVPhysics_Impl( void )
{
	VPhysicsInitShadow( false, false );
	return true;
}

void CGunTarget::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CBaseEntity	*pTarg;
	// now find our next target
	pTarg = GetNextTarget();
	if ( pTarg )
	{
		m_hTargetEnt = pTarg;
		Vector nextPos = pTarg->GetAbsOrigin();
		Teleport( &nextPos, nullptr, nullptr );
	}
}

void CGunTarget::Start( void )
{
	m_takedamage = DAMAGE_YES;
	AddFlag( FL_AIMTARGET );
	m_hTargetEnt = GetNextTarget();
	if ( m_hTargetEnt == nullptr )
		return;
	m_iHealth = m_iMaxHealth;
	Next();
}

void CGunTarget::Next( void )
{
	SetThink( nullptr );

	m_hTargetEnt = GetNextTarget();
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	SetMoveDone( &CGunTarget::Wait );
	LinearMove( pTarget->GetLocalOrigin(), m_flSpeed );
}

void CGunTarget::Wait( void )
{
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	variant_t emptyVariant;
	pTarget->AcceptInput( "InPass", this, this, emptyVariant, 0 );
		
	m_flWait = pTarget->GetDelay();

	m_target = pTarget->m_target;
	SetMoveDone( &CGunTarget::Next );
	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		SetMoveDoneTime( m_flWait );
	}
	else
	{
		Next();// do it RIGHT now!
	}
}

void CGunTarget::Stop( void )
{
	SetAbsVelocity( vec3_origin );
	SetMoveDoneTime( -1 );
	m_takedamage = DAMAGE_NO;
}

int	CGunTarget::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	if ( m_iHealth > 0 )
	{
		m_iHealth -= info.GetDamage();
		if ( m_iHealth <= 0 )
		{
			m_iHealth = 0;
			Stop();

			m_OnDeath.FireOutput( info.GetInflictor(), this );
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that starts the target moving.
//-----------------------------------------------------------------------------
void CGunTarget::InputStart( inputdata_t &inputdata )
{
	Start();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops the target from moving.
//-----------------------------------------------------------------------------
void CGunTarget::InputStop( inputdata_t &inputdata )
{
	Stop();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that toggles the start/stop state of the target.
//-----------------------------------------------------------------------------
void CGunTarget::InputToggle( inputdata_t &inputdata )
{
	if ( m_on )
	{
		Stop();
	}
	else
	{
		Start();
	}
}

/*

===== h_export.cpp ========================================================

  Entity classes exported by Halflife.

*/

#line 12

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Contains the set of functions for manipulating entity hierarchies.

// UNDONE: Reconcile this with SetParent()

//-----------------------------------------------------------------------------
// Purpose: Does the linked list work of removing a child object from the hierarchy.
// Input  : pParent - 
//			pChild - 
//-----------------------------------------------------------------------------
void UnlinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	CBaseEntity *pList;
	EHANDLE *pPrev;

	pList = pParent->m_hMoveChild;
	pPrev = &pParent->m_hMoveChild;
	while ( pList )
	{
		CBaseEntity *pNext = pList->m_hMovePeer;
		if ( pList == pChild )
		{
			// patch up the list
			pPrev->Set( pNext );

			// Clear hierarchy bits for this guy
			pList->m_hMoveParent.Set( nullptr );
			pList->m_hMovePeer.Set( nullptr );
			pList->NetworkProp()->SetNetworkParent( INVALID_EHANDLE );
			pList->DispatchUpdateTransmitState();	
			pList->OnEntityEvent( ENTITY_EVENT_PARENT_CHANGED, nullptr );
			
			pParent->RecalcHasPlayerChildBit();
			return;
		}
		else
		{
			pPrev = &pList->m_hMovePeer;
			pList = pNext;
		}
	}

	// This only happens if the child wasn't found in the parent's child list
	contract_assert( 0 );
}

void LinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	EHANDLE hParent;
	hParent.Set( pParent );
	pChild->m_hMovePeer.Set( pParent->FirstMoveChild() );
	pParent->m_hMoveChild.Set( pChild );
	pChild->m_hMoveParent = hParent;
	pChild->NetworkProp()->SetNetworkParent( hParent );
	pChild->DispatchUpdateTransmitState();
	pChild->OnEntityEvent( ENTITY_EVENT_PARENT_CHANGED, nullptr );
	pParent->RecalcHasPlayerChildBit();
}

void TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent )
{
	CBaseEntity *pChild = pOldParent->FirstMoveChild();
	while ( pChild )
	{
		// NOTE: Have to do this before the unlink to ensure local coords are valid
		Vector vecAbsOrigin = pChild->GetAbsOrigin();
		QAngle angAbsRotation = pChild->GetAbsAngles();
		Vector vecAbsVelocity = pChild->GetAbsVelocity();
//		QAngle vecAbsAngVelocity = pChild->GetAbsAngularVelocity();

		UnlinkChild( pOldParent, pChild );
		LinkChild( pNewParent, pChild );

		// FIXME: This is a hack to guarantee update of the local origin, angles, etc.
		pChild->m_vecAbsOrigin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		pChild->m_angAbsRotation.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		pChild->m_vecAbsVelocity.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );

		pChild->SetAbsOrigin(vecAbsOrigin);
		pChild->SetAbsAngles(angAbsRotation);
		pChild->SetAbsVelocity(vecAbsVelocity);
//		pChild->SetAbsAngularVelocity(vecAbsAngVelocity);

		pChild  = pOldParent->FirstMoveChild();
	}
}

void UnlinkFromParent( CBaseEntity *pRemove )
{
	if ( pRemove->GetMoveParent() )
	{
		// NOTE: Have to do this before the unlink to ensure local coords are valid
		Vector vecAbsOrigin = pRemove->GetAbsOrigin();
		QAngle angAbsRotation = pRemove->GetAbsAngles();
		Vector vecAbsVelocity = pRemove->GetAbsVelocity();
//		QAngle vecAbsAngVelocity = pRemove->GetAbsAngularVelocity();

		UnlinkChild( pRemove->GetMoveParent(), pRemove );

		pRemove->SetLocalOrigin(vecAbsOrigin);
		pRemove->SetLocalAngles(angAbsRotation);
		pRemove->SetLocalVelocity(vecAbsVelocity);

		// objects with physics need to know that they got teleported. Otherwise physics will tell us where to go later.
		IPhysicsObject *pObject = pRemove->VPhysicsGetObject();
		if( pObject )
		{
			pObject->SetPosition( vecAbsOrigin, angAbsRotation, true );
		}

//		pRemove->SetLocalAngularVelocity(vecAbsAngVelocity);
		if ( pRemove->GetMoveType() != MOVETYPE_NONE && pRemove->GetMoveType() != MOVETYPE_VPHYSICS )
		{
			pRemove->UpdateWaterState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clears the parent of all the children of the given object.
//-----------------------------------------------------------------------------
void UnlinkAllChildren( CBaseEntity *pParent )
{
	CBaseEntity *pChild = pParent->FirstMoveChild();
	while ( pChild )
	{
		CBaseEntity *pNext = pChild->NextMovePeer();
		UnlinkFromParent( pChild );
		pChild  = pNext;
	}
}

bool EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity )
{
	while ( pEntity->GetMoveParent() )
	{
		pEntity = pEntity->GetMoveParent();
		if ( pParent == pEntity )
			return true;
	}

	return false;
}

static void GetAllChildren_r( CBaseEntity *pEntity, std::vector< CBaseEntity * > &list )
{
	for ( ; pEntity != nullptr; pEntity = pEntity->NextMovePeer() )
	{
		list.push_back( pEntity );
		GetAllChildren_r( pEntity->FirstMoveChild(), list );
	}
}

int GetAllChildren( CBaseEntity *pParent, std::vector< CBaseEntity * > &list )
{
	if ( !pParent )
		return 0;

	GetAllChildren_r( pParent->FirstMoveChild(), list );
	return std::ssize( list );
}

int	GetAllInHierarchy( CBaseEntity *pParent, std::vector< CBaseEntity * > &list )
{
	if (!pParent)
		return 0;
	list.push_back( pParent );
	return GetAllChildren( pParent, list ) + 1;
}

class CBasePlayer;
class CHintMessageQueue;
class CHintMessageTimers;
class CHintSystem;

using namespace ::ks::reflect;

extern int gmsgHudText;

enum { HMQ_SIZE = 8 };	// Maximum number of messages queue can hold
						// If the limit is reached, no more can be
						// added.

//--------------------------------------------------------------------------------------------------------------
CHintMessage::CHintMessage( const char * hintString, std::vector< const char * > * args, float duration )
{
	m_hintString = hintString;
	m_duration = duration;

	if ( args )
	{
		for ( int i=0; i<std::ssize( *args ); ++i )
		{
			m_args.push_back( CloneString( (*args)[i] ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
CHintMessage::~CHintMessage()
{
	for ( int i=0; i<std::ssize( m_args ); ++i )
	{
		delete[] m_args[i];
	}
	m_args.clear();
}

//--------------------------------------------------------------------------------------------------------------
bool CHintMessage::IsEquivalent( const char *hintString, std::vector< const char * > * args ) const
{
	if ( FStrEq( hintString, m_hintString ) )
	{
		if ( !args && !std::ssize( m_args ) )
		{
			return true;
		}

		if ( !args )
			return false;

		if ( std::ssize( *args ) != std::ssize( m_args ) )
			return false;

		for ( int i=0; i<std::ssize( *args ); ++i )
		{
			if ( !FStrEq( (*args)[i], m_args[i] ) )
			{
				return false;
			}
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessage::Send( CBasePlayer * client )
{
	if ( !client )
		return;

	// Custom hint text sending to allow for arguments.  This is OK because the client has a custom
	// message parser for hint text that can read the arguments.
	CSingleUserRecipientFilter user( (CBasePlayer *)client );
	user.MakeReliable();

	// client can handle 1 string only
	ks::net::CCSUsrMsg_HintText msg;
	msg.text = m_hintString;

	SendUserMessage( user, ks::net::CS_UM_HintText, msg );
	
}

//--------------------------------------------------------------------------------------------------------------
CHintMessageQueue::CHintMessageQueue( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessageQueue::Reset()
{
	m_tmMessageEnd = 0;
	for ( int i=0; i<std::ssize( m_messages ); ++i )
	{
		delete m_messages[i];
	}
	m_messages.clear();
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessageQueue::Update()
{
	if ( !m_pPlayer )
		return;

	// test this - send the message as soon as it is ready, 
	// just stomp the old message
	//if ( gpGlobals->curtime > m_tmMessageEnd )
	{
		if ( !m_messages.empty() )
		{
			CHintMessage *msg = m_messages[0];
			m_tmMessageEnd = gpGlobals->curtime + msg->GetDuration();
			msg->Send( m_pPlayer );
			delete msg;
			m_messages.erase( m_messages.begin() + 0 );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CHintMessageQueue::AddMessage( const char* message, float duration, std::vector< const char * > * args )
{
	if ( !m_pPlayer )
		return false;

	for ( int i=0; i<std::ssize( m_messages ); ++i )
	{
		// weed out duplicates
		if ( m_messages[i]->IsEquivalent( message, args ) )
		{
			return true;
		}
	}

	// 'message' is not copied, so the pointer must remain valid forever
	CHintMessage *msg = new CHintMessage( message, args, duration );
	m_messages.push_back( msg );
	return true;
}

//--------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
CHintMessageTimers::CHintMessageTimers( CHintSystem *pSystem, CHintMessageQueue *pQueue )
{
	m_pHintSystem = pSystem;
	m_pQueue = pQueue;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all registered timers
//-----------------------------------------------------------------------------
void CHintMessageTimers::Reset()
{
	for ( int i=0; i<std::ssize( m_Timers ); ++i )
	{
		delete m_Timers[i];
	}
	m_Timers.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Check & fire any timers that should fire based on their duration.
//-----------------------------------------------------------------------------
void CHintMessageTimers::Update()
{
	if ( !m_pHintSystem )
		return;

	for ( int i = 0; i < std::ssize( m_Timers ); i++ )
	{
		if ( m_Timers[i]->timer.Expired() )
		{
			if ( m_pHintSystem->TimerShouldFire( m_Timers[i]->iHintID ) )
			{
				//Warning("TIMER FIRED: %s\n", m_pszHintMessages[m_Timers[i]->iHintID] );

				m_pHintSystem->HintMessage( m_Timers[i]->iHintID );

				// Remove and return. No reason to bring up multiple hints.
				RemoveTimer( m_Timers[i]->iHintID );
				return;
			}
			else
			{
				// Push the timer out again
				m_Timers[i]->timer.Start();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Register a new timer that the system should should keep track of.
// Input  : iHintID - The ID of the hint message
//			timer_duration - the total time the timer should run for until it fires the hint message
//			message_duration - the duration passed into the hint message system when the hint fires
//			args - the arguments passed into the hint message system when the hint fires
//-----------------------------------------------------------------------------
void CHintMessageTimers::AddTimer( int iHintID, float timer_duration, float message_duration, std::vector< const char * > * args )
{
	if ( GetTimerIndex(iHintID) != -1 )
		return;

	// 'message' is not copied, so the pointer must remain valid forever
	hintmessagetime_t *newTimer = new hintmessagetime_t( timer_duration );
	newTimer->iHintID = iHintID;
	newTimer->flMessageDuration = message_duration;
	if ( args )
	{
		for ( int i=0; i<std::ssize( *args ); ++i )
		{
			newTimer->args.push_back( CloneString( (*args)[i] ) );
		}
	}
	m_Timers.push_back( newTimer );

	//Warning("TIMER ADDED: %s\n", m_pszHintMessages[iHintID] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintMessageTimers::RemoveTimer( int iHintID )
{
	int iIndex = GetTimerIndex(iHintID);
	if ( iIndex != -1 )
	{
		//Warning("TIMER REMOVED: %s\n", m_pszHintMessages[iHintID] );
		m_Timers.erase( m_Timers.begin() + iIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintMessageTimers::StartTimer( int iHintID )
{
	int iIndex = GetTimerIndex(iHintID);
	if ( iIndex != -1 )
	{
		//Warning("TIMER STARTED: %s\n", m_pszHintMessages[iHintID] );
		m_Timers[iIndex]->timer.Start();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintMessageTimers::StopTimer( int iHintID )
{
	int iIndex = GetTimerIndex(iHintID);
	if ( iIndex != -1 )
	{
		//Warning("TIMER STOPPED: %s\n", m_pszHintMessages[iHintID] );
		m_Timers[iIndex]->timer.Stop();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the index of the hint message in the timer list, if any
//-----------------------------------------------------------------------------
int CHintMessageTimers::GetTimerIndex( int iHintID )
{
	for ( int i = 0; i < std::ssize( m_Timers ); i++ )
	{
		if ( m_Timers[i]->iHintID == iHintID )
			return i;
	}

	return -1;
}

class CBasePlayer;
class CHintMessageQueue;
class CHintMessageTimers;
class CHintSystem;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHintSystem::CHintSystem( void )
{
	Init( nullptr, 0, nullptr );
	m_pHintMessageQueue = nullptr;
	m_pHintMessageTimers = nullptr;
	m_flLastHintPlayedAt = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHintSystem::~CHintSystem( void )
{
	if ( m_pHintMessageTimers )
	{
		delete m_pHintMessageTimers;
		m_pHintMessageTimers = nullptr;
	}

	if ( m_pHintMessageQueue )
	{
		delete m_pHintMessageQueue;
		m_pHintMessageQueue = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::Init( CBasePlayer *pPlayer, int iMaxHintTypes, const char **pszHintStrings )
{
	m_pPlayer = pPlayer;	
	m_bShowHints = true;

	m_HintHistory.assign( iMaxHintTypes, false );

	m_pszHintMessages = pszHintStrings;

	if ( m_pPlayer )
	{
		m_pHintMessageQueue = new CHintMessageQueue( m_pPlayer );
		m_pHintMessageTimers = new CHintMessageTimers( this, m_pHintMessageQueue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::Update( void )
{
	if ( m_pHintMessageQueue )
	{
		m_pHintMessageQueue->Update();
	}
	if ( m_pHintMessageTimers )
	{
		m_pHintMessageTimers->Update();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Displays a hint message to the player
// Input  : hint - enum'd hint to show
//			bForce - always play this hint even if they have seen it before
//-----------------------------------------------------------------------------
bool CHintSystem::HintMessage( int hint, bool bForce /* = false */, bool bOnlyIfClear /* = false */ )
{
	contract_assert( m_pPlayer );
	contract_assert( hint < static_cast< int >( m_HintHistory.size() ) );

	// Not really an optimal solution, but saves us querying the hud element,
	// which wouldn't be easy with derived versions in different mods.
	if ( bOnlyIfClear && (gpGlobals->curtime - m_flLastHintPlayedAt < 11 ) )
		return false;

	if ( bForce || !HasPlayedHint(hint) )
	{
		PlayedAHint();
		HintMessage( m_pszHintMessages[hint] );
		m_HintHistory[hint] = true;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Displays a hint message to the player
// Input  : *pMessage - 
//-----------------------------------------------------------------------------
void CHintSystem::HintMessage( const char *pMessage )
{
	contract_assert( m_pPlayer );

	// On the server, we send it down to the queue who sends it to the client
	if ( !m_pPlayer->IsNetClient() || !m_pHintMessageQueue )
		return;

	if ( !m_bShowHints )
		return;

	m_pHintMessageQueue->AddMessage( pMessage );
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the existing timers, and register new ones for any
//			hints that haven't been displayed yet.
//-----------------------------------------------------------------------------
void CHintSystem::ResetHints( void )
{
	if ( !m_pHintMessageTimers )
		return;

	m_pHintMessageTimers->Reset();

	// Readd registered hints
	for (int i = 0; i < std::ssize( m_RegisteredResetHints ); i++ )
	{
		ReAddHintTimerIfNotDisplayed( m_RegisteredResetHints[i].iHintID, m_RegisteredResetHints[i].flTimer );
	}

	// Reset our queue
	if ( m_pHintMessageQueue )
	{
		m_pHintMessageQueue->Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Call this to add a hint message that should be re-added  
//			everytime we're reset, if it hasn't been displayed yet.
//-----------------------------------------------------------------------------
void CHintSystem::RegisterHintTimer( int iHintID, float flTimerDuration, bool bOnlyIfClear /* = false */, HintTimerCallback pfnCallback )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );

	onresethints_t newHint;
	newHint.iHintID = iHintID;
	newHint.flTimer = flTimerDuration;
	newHint.bOnlyIfClear = bOnlyIfClear;
	newHint.pfnCallback = pfnCallback;
	m_RegisteredResetHints.push_back( newHint );
}

//-----------------------------------------------------------------------------
// Purpose: If the hint hasn't been displayed, start a timer for it
//-----------------------------------------------------------------------------
void CHintSystem::ReAddHintTimerIfNotDisplayed( int iHintID, float flTimerDuration )
{
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	if ( m_HintHistory[iHintID] == 0 )
	{
		m_pHintMessageTimers->AddTimer( iHintID, flTimerDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::StartHintTimer( int iHintID )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	contract_assert( m_pHintMessageTimers );
	m_pHintMessageTimers->StartTimer( iHintID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::StopHintTimer( int iHintID )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	contract_assert( m_pHintMessageTimers );
	m_pHintMessageTimers->StopTimer( iHintID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::ResetHintTimers( void )
{
	contract_assert( m_pPlayer );
	contract_assert( m_pHintMessageTimers );
	m_pHintMessageTimers->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::RemoveHintTimer( int iHintID )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	contract_assert( m_pHintMessageTimers );
	m_pHintMessageTimers->RemoveTimer( iHintID );

	// Mark us as having heard this hint
	m_HintHistory[iHintID] = true;
}

//-----------------------------------------------------------------------------
// Purpose: See if there's a callback registered for the specified hint.
//			If so, see if it wants to allow the hint to fire.
//-----------------------------------------------------------------------------
bool CHintSystem::TimerShouldFire( int iHintID )
{
	for (int i = 0; i < std::ssize( m_RegisteredResetHints ); i++ )
	{
		if ( m_RegisteredResetHints[i].iHintID != iHintID )
			continue;
		
		if ( m_RegisteredResetHints[i].bOnlyIfClear && HintIsCurrentlyVisible() )
			return false;

		if ( m_RegisteredResetHints[i].pfnCallback )
			return m_RegisteredResetHints[i].pfnCallback( m_pPlayer );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHintSystem::ShouldShowHints( void ) 
{ 
	return m_bShowHints; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::PlayedAHint( void )
{
	m_flLastHintPlayedAt = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHintSystem::HasPlayedHint( int iHintID )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	return ( m_HintHistory[iHintID] > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintSystem::SetHintPlayed( int iHintID )
{
	contract_assert( m_pPlayer );
	contract_assert( iHintID < static_cast< int >( m_HintHistory.size() ) );
	m_HintHistory[iHintID] = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HintClear( void )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if ( pPlayer && pPlayer->Hints() )
	{
		pPlayer->Hints()->ClearHintHistory();
	}
}
ConCommand sv_clearhinthistory( side_registry(), "sv_clearhinthistory", HintClear, "Clear memory of server side hints displayed to the player." );

KS_ENTITY_ABSTRACT_KIND( CGameRulesProxy )
