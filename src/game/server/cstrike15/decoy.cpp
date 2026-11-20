// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2011 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "../baseentity.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "../reflect_sendtable.h"
#include "../util.h"
#include "reflect.h"
#include "../reflect_datamap.h"
#include "declare_class.h"
#include "edict.h"
#include "soundflags.h"
#include "../predictable_entity.h"
#include "worldsize.h"
module ks.server;

import std;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :econ.econ_item_schema_minimal;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.types;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import :cstrike15.hostage.cs_simple_hostage;
import ks.core.dbg;
import ks.core.vprof;
import ks.tier1.fmtstr;
import :cstrike15.cs_team;
import ks.core.basetypes;
import ks.game.iplayeranimstate;
import ks.tier1.strtools;
import :cstrike15.Effects.chicken;
import ks.game.ai_activity;
import ks.game.tempentity;
import ks.math.vector2d;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseGrenade;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CWeaponCSBase;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_decoy_dropped.mdl"

LINK_ENTITY_TO_CLASS( decoy_projectile, CDecoyProjectile );
PRECACHE_REGISTER( decoy_projectile );

IMPLEMENT_REFLECT_SERVERCLASS( CDecoyProjectile, DT_DecoyProjectile )

struct DecoyWeaponProfile
{
	CSWeaponType	weaponType;
	int				minShots;
	int				maxShots;
	float			extraDelay;
	float			pauseMin;
	float			pauseMax;
};

DecoyWeaponProfile gDecoyWeaponProfiles[] = 
{
	//	CSWeaponType		minShots, maxShots, extraDelay, pauseMin, pauseMax
	{	WEAPONTYPE_PISTOL,			1,	3,		0.3f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SUBMACHINEGUN,	1,	5,		0.0f,		0.5f,	4.0f	},
	{	WEAPONTYPE_RIFLE,			1,	3,		0.5f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SHOTGUN,			1,	3,		0.0f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SNIPER_RIFLE,	1,	3,		0.5f,		0.5f,	4.0f	},
	{	WEAPONTYPE_MACHINEGUN,		6,	20,		0.0f,		0.5f,	4.0f	},
};

// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CDecoyProjectile* CDecoyProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CDecoyProjectile *pGrenade = ( CDecoyProjectile* )CBaseEntity::Create( "decoy_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 2.0f );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );

	pGrenade->m_flDamage = 25.0f; // 25 = 1/4 of HEGrenade Damage
	pGrenade->m_DmgRadius = pGrenade->m_flDamage * 3.5f; // Changing damage will change the radius
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	pGrenade->SetTouch( &CBaseGrenade::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->m_pWeaponInfo = &weaponInfo;

	ASSERT(pOwner != nullptr);

	// pick a weapon based on what the player is carrying, falling back to default starting pistols
	CBaseCombatWeapon* pPrimaryWeapon = pOwner->Weapon_GetSlot(WEAPON_SLOT_RIFLE);
	CBaseCombatWeapon* pSecondaryWeapon = pOwner->Weapon_GetSlot(WEAPON_SLOT_PISTOL);

	pGrenade->m_decoyWeaponDefIndex = INVALID_ITEM_DEF_INDEX;
	pGrenade->m_decoyWeaponSoundType = SINGLE;

	if ( pPrimaryWeapon != nullptr )
	{
		CEconItemView* pItem = pPrimaryWeapon->GetEconItemView();
		if ( pItem && pItem->IsValid() )
		{
			pGrenade->m_decoyWeaponDefIndex = pItem->GetItemDefinition()->GetDefinitionIndex();
			pGrenade->m_decoyWeaponId = WeaponIdFromString( pItem->GetItemDefinition()->GetItemClass() );
			CWeaponCSBase *pWeapon = static_cast<CWeaponCSBase *>( pPrimaryWeapon );
			if ( pWeapon )
			{
				pGrenade->m_decoyWeaponSoundType = ( pWeapon->HasSilencer() && pWeapon->IsSilenced() ) ? SPECIAL1 : SINGLE;
			}
		}
		else
		{
			pGrenade->m_decoyWeaponId = (CSWeaponID)pPrimaryWeapon->GetWeaponID();
		}
	}
	else if ( pSecondaryWeapon != nullptr )
	{
		CEconItemView* pItem = pSecondaryWeapon->GetEconItemView();
		if ( pItem && pItem->IsValid() )
		{
			pGrenade->m_decoyWeaponDefIndex = pItem->GetItemDefinition()->GetDefinitionIndex();
			pGrenade->m_decoyWeaponId = WeaponIdFromString( pItem->GetItemDefinition()->GetItemClass() );
			CWeaponCSBase *pWeapon = static_cast<CWeaponCSBase *>( pSecondaryWeapon );
			if ( pWeapon )
			{
				pGrenade->m_decoyWeaponSoundType = ( pWeapon->HasSilencer() && pWeapon->IsSilenced() ) ? SPECIAL1 : SINGLE;
			}
		}
		else
		{
			pGrenade->m_decoyWeaponId = (CSWeaponID)pSecondaryWeapon->GetWeaponID();
		}
	}
	else
	{
		if ( pOwner->GetTeamNumber() == TEAM_CT )
		{
			pGrenade->m_decoyWeaponId = WEAPON_HKP2000;
		}
		else
		{
			pGrenade->m_decoyWeaponId = WEAPON_GLOCK;
		}
	}
	const CCSWeaponInfo* pDecoyWeaponInfo = GetWeaponInfo(pGrenade->m_decoyWeaponId);

	// find the corresponding decoy firing profile
	pGrenade->m_pProfile = &gDecoyWeaponProfiles[0];
	for ( int i = 0; i < std::size( gDecoyWeaponProfiles ); ++i )
	{
		if ( gDecoyWeaponProfiles[i].weaponType == pDecoyWeaponInfo->GetWeaponType() ) 
		{
			pGrenade->m_pProfile = &gDecoyWeaponProfiles[i];
			break;
		}
	}
	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CDecoyProjectile::SetTimer( float timer )
{
	SetThink( &CDecoyProjectile::Think_Detonate );
	SetNextThink( gpGlobals->curtime + timer );

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CDecoyProjectile::Think_Detonate( void )
{
	if ( GetAbsVelocity().Length() > 0.2f )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2f );
		return;
	}

	CCSPlayer *player = ToCSPlayer( GetThrower() );
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "decoy_started" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	m_shotsRemaining = 0;
	m_fExpireTime = gpGlobals->curtime + 14.0f; // TODO: Make this Data Driven

	SetThink( &CDecoyProjectile::GunfireThink );
	TheBots->SetGrenadeRadius( this, DecoyGrenadeRadius );
	GunfireThink(); // This will handling the 'Detonate'
}

void CDecoyProjectile::Detonate( void )
{
	// [mlowrance] The Decoy is handling it's own detonate.
	contract_assert( !"Decoy grenade handles its own detonation" );
}

void CDecoyProjectile::GunfireThink( void )
{
	ASSERT(m_pProfile != nullptr);
	if ( !m_pProfile )
		return;

	if ( m_shotsRemaining <= 0 )
	{
		// pick a new burst activity
		m_shotsRemaining = RandomInt( m_pProfile->minShots, m_pProfile->maxShots );
	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo(m_decoyWeaponId);

	CBroadcastRecipientFilter filter;

	const char *shootsound = pWeaponInfo->aShootSounds[ m_decoyWeaponSoundType ];

	if ( m_decoyWeaponDefIndex != INVALID_ITEM_DEF_INDEX )
	{
		// Get the item definition
		const CEconItemDefinition *pDef = ( m_decoyWeaponDefIndex > 0 ) ? GetItemSchema()->GetItemDefinition( m_decoyWeaponDefIndex ) : nullptr;
		if ( pDef )
		{
			const char *pszTempSound = pDef->GetWeaponReplacementSound( m_decoyWeaponSoundType );
			if ( pszTempSound )
			{
				shootsound = pszTempSound;
			}
		}
	}

	CSoundParameters params;
	if ( GetParametersForSound( shootsound, params, nullptr ) )
	{
		CPASAttenuationFilter filter( this, params.soundlevel );
		EmitSound( filter, entindex(), shootsound, &GetLocalOrigin(), 0.0f ); 
		DispatchParticleEffect( "weapon_decoy_ground_effect_shot", GetAbsOrigin(), GetAbsAngles() );
	}

	// tell the bots about the gunfire
	CCSPlayer *pPlayer = ToCSPlayer( GetThrower() );
	if ( pPlayer )
	{
		// allow the bots to react to the "gunfire"

		// we need a custom event here, because the "weapon_fire" event assumes
		// the gunfire is coming from the player!
		IGameEvent * event = gameeventmanager->CreateEvent( "decoy_firing" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( --m_shotsRemaining > 0 )
	{
		SetNextThink( gpGlobals->curtime + pWeaponInfo->GetCycleTime()  + RandomFloat( 0.0f, m_pProfile->extraDelay) );
		return;
	}

	if ( gpGlobals->curtime < m_fExpireTime )
	{
		SetNextThink( gpGlobals->curtime + pWeaponInfo->GetCycleTime() + RandomFloat( m_pProfile->pauseMin, m_pProfile->pauseMax ) );
	}
	else
	{
		// [mlowrance] Do the damage on Despawn and post event
		CCSPlayer *player = ToCSPlayer( GetThrower() );
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "decoy_detonate" );
			if ( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				event->SetInt( "entityid", this->entindex() );
				event->SetFloat( "x", GetAbsOrigin().x );
				event->SetFloat( "y", GetAbsOrigin().y );
				event->SetFloat( "z", GetAbsOrigin().z );
				gameeventmanager->FireEvent( event );
			}
		}

		BaseClass::Detonate();
		UTIL_Remove( this );
	}
}

void CDecoyProjectile::Spawn_Impl( void )
{
	SetModel( GRENADE_MODEL );
 	BaseClass::Spawn_Impl();
	
	SetBodygroupPreset( "thrown" );
}

void CDecoyProjectile::Precache_Impl( void )
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Flashbang.Explode" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	PrecacheParticleSystem( "weapon_decoy_ground_effect_shot" );

	BaseClass::Precache_Impl();
}

//TODO: Let physics handle the sound!
void CDecoyProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

#undef GRENADE_MODEL

class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseEntity;
class CBaseGrenade;
class CCSPlayer;
class CCSWeaponInfo;
class CWeaponCSBase;
struct inputdata_t;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_flashbang_dropped.mdl"

LINK_ENTITY_TO_CLASS( flashbang_projectile, CFlashbangProjectile );
PRECACHE_REGISTER( flashbang_projectile );

IMPLEMENT_REFLECT_DATAMAP( CFlashbangProjectile )

// hack to allow de_nuke vents to occlude flashbangs when closed
class CTraceFilterNoPlayersAndFlashbangPassableAnims : public CTraceFilterNoPlayers
{
public:
	CTraceFilterNoPlayersAndFlashbangPassableAnims( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterNoPlayers( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{

		CBaseEntity *pEnt = EntityFromEntityHandle(pHandleEntity);
		if ( pEnt )
		{
			CBaseAnimating* pAnimating = dynamic_cast< CBaseAnimating* >( pEnt );
			if ( pAnimating )
			{
				// look for the flashbang passable animtag
				float flFlashbangPassable = pAnimating->GetAnySequenceAnimTag( pAnimating->GetSequence(), ANIMTAG_FLASHBANG_PASSABLE, -1 );

				if ( flFlashbangPassable != -1 )
					return false; // model animation is tagged to allow flashbangs through
			}

			// Weapons don't block flashbangs
			CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pEnt );
			CBaseGrenade* pGrenade = dynamic_cast< CBaseGrenade* > ( pEnt );
			if ( pWeapon || pGrenade )
				return false;
		}

		return CTraceFilterNoPlayers::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

float PercentageOfFlashForPlayer(CBaseEntity *player, Vector flashPos, CBaseEntity *pevInflictor)
{
	if (!(player->IsPlayer()))
	{
		// if this entity isn't a player, it's a hostage or some other entity, then don't bother with the expensive checks
		// that come below.
		return 0.0f;
	}

	const float FLASH_FRACTION = 0.167f;
	const float SIDE_OFFSET = 75.0f;

	Vector pos = player->EyePosition();
	Vector vecRight, vecUp;

	QAngle tempAngle;
	VectorAngles(player->EyePosition() - flashPos, tempAngle);
	AngleVectors(tempAngle, nullptr, &vecRight, &vecUp);

	vecRight.NormalizeInPlace();
	vecUp.NormalizeInPlace();

	// Set up all the ray stuff.
	// We don't want to let other players block the flash bang so we use this custom filter.
	Ray_t ray;
	trace_t tr;
	CTraceFilterNoPlayersAndFlashbangPassableAnims traceFilter( pevInflictor, COLLISION_GROUP_NONE );
	unsigned int FLASH_MASK = MASK_OPAQUE_AND_NPCS | CONTENTS_DEBRIS;

	// According to comment in IsNoDrawBrush in cmodel.cpp, CONTENTS_OPAQUE is ONLY used for block light surfaces,
	// and we want flashbang traces to pass through those, since the block light surface is only used for blocking
	// lightmap light rays during map compilation.
	FLASH_MASK &= ~CONTENTS_OPAQUE;

	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.Ent<CBaseEntity>() == player))
	{
		return 1.0f;
	}

	float retval = 0.0f;

	// check the point straight up.
	pos = flashPos + vecUp*50.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.Ent<CBaseEntity>() == player))
	{
		retval += FLASH_FRACTION;
	}

	// check the point up and right.
	pos = flashPos + vecRight*SIDE_OFFSET + vecUp*10.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.Ent<CBaseEntity>() == player))
	{
		retval += FLASH_FRACTION;
	}

	// Check the point up and left.
	pos = flashPos - vecRight*SIDE_OFFSET + vecUp*10.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.Ent<CBaseEntity>() == player))
	{
		retval += FLASH_FRACTION;
	}

	return retval;
}

// --------------------------------------------------------------------------------------------------- //
//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
// 
// only damage ents that can clearly be seen by the explosion!
// --------------------------------------------------------------------------------------------------- //

void RadiusFlash( 
	Vector vecSrc, 
	CBaseEntity *pevInflictor, 
	CBaseEntity *pevAttacker, 
	float flDamage, 
	int iClassIgnore, 
	int bitsDamageType, 
	uint8 *pOutNumOpponentsEffected = nullptr, 
	uint8 *pOutNumTeammatesEffected = nullptr )
{
	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	if ( pOutNumOpponentsEffected )
		*pOutNumOpponentsEffected = 0;

	if ( pOutNumTeammatesEffected )
		*pOutNumTeammatesEffected = 0;
	
	trace_t		tr;
	float		flAdjustedDamage;
	variant_t	var;
	Vector		vecEyePos;
	float		fadeTime, fadeHold;
	Vector		vForward;
	Vector		vecLOS;
	float		flDot;
	
	CBaseEntity		*pEntity = nullptr;
	static float	flRadius = 3000;
	float			falloff = flDamage / flRadius;

	//bool bInWater = (UTIL_PointContents( vecSrc, MASK_WATER ) == CONTENTS_WATER);

	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere( pEntity, vecSrc, flRadius )) != nullptr)
	{	
		bool bPlayer = pEntity->IsPlayer();
		
		if( !bPlayer )
			continue;

		vecEyePos = pEntity->EyePosition();

		//// blasts used to not travel into or out of water, users assumed it was a bug. Fix is not to run this check -wills
		//if ( bInWater && pEntity->GetWaterLevel() == WL_NotInWater)
		//	continue;
		//if (!bInWater && pEntity->GetWaterLevel() == WL_Eyes)
		//	continue;

		float percentageOfFlash = PercentageOfFlashForPlayer(pEntity, vecSrc, pevInflictor);

		if ( percentageOfFlash > 0.0 )
		{
			if ( pOutNumOpponentsEffected && pEntity->GetTeamNumber() != pevAttacker->GetTeamNumber() )
				(*pOutNumOpponentsEffected)++;
			if ( pOutNumTeammatesEffected && pEntity->GetTeamNumber() == pevAttacker->GetTeamNumber() )
				(*pOutNumTeammatesEffected)++;

			// decrease damage for an ent that's farther from the grenade
			flAdjustedDamage = flDamage - ( vecSrc - pEntity->EyePosition() ).Length() * falloff;

			if ( flAdjustedDamage > 0 )
			{
				// See if we were facing the flash
				AngleVectors( pEntity->EyeAngles(), &vForward );

				vecLOS = ( vecSrc - vecEyePos );

				float flDistance = vecLOS.Length();

				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vecLOS), 0, 255, 0, true, 10.0 );
				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vForward), 0, 0, 255, true, 10.0 );

				// Normalize both vectors so the dotproduct is in the range -1.0 <= x <= 1.0 
				vecLOS.NormalizeInPlace();

				flDot = DotProduct (vecLOS, vForward);

				float startingAlpha = 255;
	
				// if target is facing the bomb, the effect lasts longer
				if( flDot >= 0.6 )
				{
					// looking at the flashbang
					fadeTime = flAdjustedDamage * 2.5f;
					fadeHold = flAdjustedDamage * 1.25f;
				}
				else if( flDot >= 0.3 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 1.75f;
					fadeHold = flAdjustedDamage * 0.8f;
				}
				else if( flDot >= -0.2 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 1.00f;
					fadeHold = flAdjustedDamage * 0.5f;
				}
				else
				{
					// facing away
					fadeTime = flAdjustedDamage * 0.5f;
					fadeHold = flAdjustedDamage * 0.25f;
				//	startingAlpha = 200;
				}

				fadeTime *= percentageOfFlash;
				fadeHold *= percentageOfFlash;

				if ( bPlayer )
				{
					// blind players and bots
					CCSPlayer *player = static_cast< CCSPlayer * >( pEntity );

					// [tj] Store who was responsible for the most recent flashbang blinding.
					CCSPlayer *attacker = ToCSPlayer (pevAttacker);
					if ( attacker && player && player->IsAlive() )
					{
						player->SetLastFlashbangAttacker(attacker);

						// score points/penalties for blinding players
						if ( flDot >= 0.0f )
						{
							if ( attacker->GetTeamNumber() == player->GetTeamNumber() )
								CSGameRules()->ScoreBlindFriendly( attacker );
							else
								CSGameRules()->ScoreBlindEnemy( attacker );
						}
					}

					player->Blind( fadeHold, fadeTime, startingAlpha );
					
					// fire an event when a player has been sufficiently blinded as to not
					// be able to perform the training map flashbang range test
					if ( CSGameRules()->IsPlayingTraining() && fadeHold > 1.9f )
					{
						IGameEvent * event = gameeventmanager->CreateEvent( "tr_player_flashbanged" );
						if ( event )
						{
							event->SetInt( "userid", player->GetUserID() );
							gameeventmanager->FireEvent( event );
						}
					}
					
					IGameEvent * event = gameeventmanager->CreateEvent( "player_blind" );
					if ( event )
					{
						event->SetInt( "userid", player->GetUserID() );
						event->SetInt( "attacker", attacker ? attacker->GetUserID() : 0 );
						event->SetInt( "entityid", pevInflictor ? pevInflictor->entindex() : 0 );
						event->SetFloat( "blind_duration", player->m_flFlashDuration ); 
						gameeventmanager->FireEvent( event );
					}

					// deafen players and bots
					player->Deafen( flDistance );
				}
			}	
		}
	}

	CPVSFilter filter(vecSrc);
	te->DynamicLight( filter, 0.0, &vecSrc, 255, 255, 255, 2, 400, 0.1, 768 );
}

// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CFlashbangProjectile* CFlashbangProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CFlashbangProjectile *pGrenade = (CFlashbangProjectile*)CBaseEntity::Create( "flashbang_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );
	pGrenade->m_pWeaponInfo = &weaponInfo;

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

CFlashbangProjectile::CFlashbangProjectile()
{
	m_flDamage = 100;
	// default timer value for when a player throws it
	// can be overridden when spawned from an entity maker
	m_flTimeToDetonate = 1.5;
	m_numOpponentsHit = m_numTeammatesHit = 0;
}

void CFlashbangProjectile::Spawn_Impl()
{
	SetModel( GRENADE_MODEL );

	SetDetonateTimerLength( m_flTimeToDetonate );

	SetTouch( &CBaseGrenade::BounceTouch );

	SetThink( &CBaseCSGrenadeProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	SetGravity( BaseClass::GetGrenadeGravity() );
	SetFriction( BaseClass::GetGrenadeFriction() );
	SetElasticity( BaseClass::GetGrenadeElasticity() );

	m_pWeaponInfo = GetWeaponInfo( WEAPON_FLASHBANG );

	BaseClass::Spawn_Impl();

	SetBodygroupPreset( "thrown" );
}

void CFlashbangProjectile::Precache_Impl()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Flashbang.Explode" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	BaseClass::Precache_Impl();
}

ConVar sv_flashbang_strength( side_registry(), "sv_flashbang_strength", "3.55", FCVAR_REPLICATED, "Flashbang strength", true, 2.0, true, 8.0 );

void CFlashbangProjectile::Detonate()
{
	// tell the bots a flashbang grenade has exploded (and record log events)
	CCSPlayer *player = ToCSPlayer( GetThrower() );
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "flashbang_detonate" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	RadiusFlash ( GetAbsOrigin(), this, GetThrower(), sv_flashbang_strength.GetInt(), CLASS_NONE, DMG_BLAST, &m_numOpponentsHit, &m_numTeammatesHit );
	EmitSound( "Flashbang.Explode" );	

	trace_t		tr;
	Vector		vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 2 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -64 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	UTIL_DecalTrace( &tr, "Scorch" );

	UTIL_Remove( this );
}

//TODO: Let physics handle the sound!
void CFlashbangProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

void CFlashbangProjectile::InputSetTimer( inputdata_t &inputdata )
{
	m_flTimeToDetonate = inputdata.value.Float();
	SetDetonateTimerLength( m_flTimeToDetonate );
}

#undef GRENADE_MODEL

class CBaseEntity;
class CCSPlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Bomb target area

LINK_ENTITY_TO_CLASS( func_bomb_target, CBombTarget );

IMPLEMENT_REFLECT_DATAMAP( CBombTarget )

CBombTarget::CBombTarget( void )
{
	m_bIsHeistBombTarget = false;
	m_bBombPlantedHere = false;
	m_szMountTarget = NULL_STRING;
	m_hInstructorHint = nullptr;
}

void CBombTarget::Spawn_Impl()
{
	InitTrigger();
	SetTouch( &CBombTarget::BombTargetTouch );
	SetUse( &CBombTarget::BombTargetUse );

	//VisibilityMonitor_AddEntity( this, 1200.0f, NULL, NULL );
}

void CBombTarget::ReInitOnRoundStart( void )
{
	if ( m_hInstructorHint.Get() )
	{
		CPointEntity *pEnt = static_cast< CPointEntity* >( m_hInstructorHint.Get() );
		UTIL_Remove( pEnt );
		m_hInstructorHint = nullptr;
	}

	m_bBombPlantedHere = false;

	//CCSPlayerResource *pCSPR = CSPlayerResource();
	Vector bombA = Vector( 0, 0, 0 );
	Vector bombB = Vector( 0, 0, 0 );
	Vector vecTrigger = CollisionProp()->WorldSpaceCenter();
	if ( CSPlayerResource() )
	{
		bombA = CSPlayerResource()->GetBombsiteAPosition();
		bombB = CSPlayerResource()->GetBombsiteBPosition();

		CPointEntity *pEnt;
		if ( vecTrigger.DistTo( bombA ) > vecTrigger.DistTo( bombB ) )
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_bomb_target_hint_B" ) );
		}
		else
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_bomb_target_hint_A" ) );
		}

		if ( pEnt )
		{
			pEnt->Spawn();
			trace_t tr;

			UTIL_TraceLine( vecTrigger, vecTrigger + Vector ( 0, 0, -128 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			Vector vecHint = tr.endpos +  Vector ( 0, 0, 16 );

			pEnt->SetAbsOrigin( vecHint );
			pEnt->SetOwnerEntity( this );
			pEnt->SetParent( this );

			m_hInstructorHint = pEnt;
		}
	}
}

void CBombTarget::BombTargetTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		p->m_bInBombZoneTrigger = true;

		if ( p->HasC4() && m_bBombPlantedHere == false && (CSGameRules()->m_bBombPlanted == false || CSGameRules()->IsPlayingCoopMission()) )
		{
			p->m_bInBombZone = true;
			p->m_iBombSiteIndex = entindex();

			//bool bC4Active = p->GetActiveCSWeapon() && p->GetActiveCSWeapon()->GetCSWeaponID() == WEAPON_C4;
			if ( !(p->m_iDisplayHistoryBits & DHF_IN_TARGET_ZONE) )
			{
				p->m_iDisplayHistoryBits |= DHF_IN_TARGET_ZONE;
			}
		}
	}
}

void CBombTarget::BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//SUB_UseTargets( NULL, USE_TOGGLE, 0 );
	DevMsg( 2, "BombTargetUse does nothing\n" );
}

// Relay to our outputs
void CBombTarget::OnBombExplode( inputdata_t &inputdata )
{
	m_OnBombExplode.FireOutput(this, this);
}

// Relay to our outputs
void CBombTarget::OnBombPlanted( inputdata_t &inputdata )
{
	m_OnBombPlanted.FireOutput(this, this);

	m_bBombPlantedHere = true;
}
// Relay to our outputs
void CBombTarget::OnBombDefused( inputdata_t &inputdata )
{
	m_OnBombDefused.FireOutput(this, this);
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
void CInfoInstructorHintBombTargetA::Spawn_Impl( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, nullptr, nullptr );
}

LINK_ENTITY_TO_CLASS( info_bomb_target_hint_A, CInfoInstructorHintBombTargetA );

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
void CInfoInstructorHintBombTargetB::Spawn_Impl( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, nullptr, nullptr );
}

LINK_ENTITY_TO_CLASS( info_bomb_target_hint_B, CInfoInstructorHintBombTargetB );

class CBaseCombatWeapon;
class CBaseEntity;
class CBaseTrigger;
class CCSPlayer;
struct inputdata_t;

using namespace ::ks::reflect;

//======================================
// Buy zone
//
//

extern ConVar mp_logdetail;

class CBuyZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBuyZone, CBaseTrigger );
	DECLARE_DATADESC();

	CBuyZone();
	void Spawn_Impl();
	void BuyZoneTouch( CBaseEntity* pOther );

	[[= ks::reflect::Input{ .name = "SetTeam_TerroristOnly", .type = FIELD_VOID } ]] void InputSetTeam_TerroristOnly( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "SetTeam_CTOnly", .type = FIELD_VOID } ]] void InputSetTeam_CTOnly( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "SetTeam_AllTeams", .type = FIELD_VOID } ]] void InputSetTeam_AllTeams( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "SetTeam_None", .type = FIELD_VOID } ]] void InputSetTeam_None( inputdata_t& inputdata );

	// CBaseTrigger override
	void EndTouch_Impl( CBaseEntity *pOther ) ;

public:
	[[= ks::reflect::Key{ .name = "team", .input = true } ]] int m_LegacyTeamNum;
};

LINK_ENTITY_TO_CLASS( func_buyzone, CBuyZone );

IMPLEMENT_REFLECT_DATAMAP( CBuyZone )

CBuyZone::CBuyZone()
{
	m_LegacyTeamNum = -1;
}

void CBuyZone::Spawn_Impl()
{
	InitTrigger();
	SetTouch( &CBuyZone::BuyZoneTouch );

	// Support for legacy-style teamnums.
	if ( m_LegacyTeamNum == 1 )
	{
		ChangeTeam( TEAM_TERRORIST );
	}
	else if ( m_LegacyTeamNum == 2 )
	{
		ChangeTeam( TEAM_CT );
	}
}

void CBuyZone::BuyZoneTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		int nZoneTeam = GetTeamNumber();
		if ( nZoneTeam == -1 )
		{
			// -1 mean no team can use it
			return;
		}
		// compare player team with buy zone team number
		else if ( nZoneTeam == 0 || p->GetTeamNumber() == GetTeamNumber() )
		{
			p->m_bInBuyZone = true;
			p->AutoBuyAmmo();
		}
	}
}

void CBuyZone::InputSetTeam_TerroristOnly( inputdata_t& inputdata )
{
	ChangeTeam( TEAM_TERRORIST );
}

void CBuyZone::InputSetTeam_CTOnly( inputdata_t& inputdata )
{
	ChangeTeam( TEAM_CT );
}

void CBuyZone::InputSetTeam_AllTeams( inputdata_t& inputdata )
{
	// team 0 in this case means that everyone can use it
	ChangeTeam( 0 );
}

void CBuyZone::InputSetTeam_None( inputdata_t& inputdata )
{
	// team -1 means that no one can use it
	ChangeTeam( -1 );
}

void CBuyZone::EndTouch_Impl( CBaseEntity *pOther )
{
	BaseClass::EndTouch_Impl( pOther );

	// Feature mostly for tournament organizers to aid match stat log parsing. 
	if ( mp_logdetail.GetInt() >= 3 && pOther->IsPlayer() )
	{
		if ( CCSPlayer *pCSPlayer = ToCSPlayer( pOther ) )
		{
			std::string strEquipment( "[ ");

			for ( int i = 0; i < MAX_WEAPONS; ++i )
			{
				if ( CBaseCombatWeapon* pWeapon = pCSPlayer->GetWeapon( i ) )
					strEquipment.append( CFmtStr( "%s ", pWeapon->GetEconItemView() ? pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionName() : pWeapon->GetName() ).Get() );
			}

			if ( pCSPlayer->HasDefuser() )
				strEquipment.append( "defuser " );

			if ( pCSPlayer->ArmorValue() > 0 )
				strEquipment.append( CFmtStr( "kevlar(%d) ", pCSPlayer->ArmorValue() ).Get() );

			if ( pCSPlayer->m_bHasHelmet )
				strEquipment.append( "helmet " );

			if ( pCSPlayer->HasC4() )
				strEquipment.append( "C4 " );

			strEquipment.append( "]" );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" left buyzone with %s\n",
							pCSPlayer->GetPlayerName(),
							pCSPlayer->GetUserID(),
							pCSPlayer->GetNetworkIDString(),
							pCSPlayer->GetTeam()->GetName(),
							strEquipment.data() );
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

//============================================================================
LINK_ENTITY_TO_CLASS( func_hostage_rescue, CHostageRescueZone );

void CHostageRescueZone::Spawn_Impl()
{
	m_hInstructorHint = nullptr;

	InitTrigger();
	SetTouch( &CHostageRescueZone::HostageRescueTouch );

	VisibilityMonitor_AddEntity( this, 1600.0f, nullptr, nullptr );
}

void CHostageRescueZone::ReInitOnRoundStart( void )
{
	if ( m_hInstructorHint.Get() )
	{
		CPointEntity *pEnt = static_cast< CPointEntity* >( m_hInstructorHint.Get() );
		UTIL_Remove( pEnt );
		m_hInstructorHint = nullptr;
	}

	//CCSPlayerResource *pCSPR = CSPlayerResource();
	Vector vecPosCur = Vector( 0, 0, 0 );
	Vector vecPosNew = Vector( 0, 0, 0 );
	Vector vecTrigger = CollisionProp()->WorldSpaceCenter();
	if ( CSPlayerResource() )
	{
		vecPosCur = CSPlayerResource()->GetHostageRescuePosition( 0 );

		for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			vecPosNew = CSPlayerResource()->GetHostageRescuePosition( std::min( i+1, MAX_HOSTAGE_RESCUES-1 ) );
			if ( vecPosNew != vec3_origin )
			{
				if ( vecTrigger.DistTo( vecPosCur ) > vecTrigger.DistTo( vecPosNew ) )
				{
					vecPosCur = vecPosNew;
				}
			}
		}

		CPointEntity *pEnt = nullptr;
		if ( vecPosCur != vec3_origin )
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_hostage_rescue_zone_hint" ) );
		}

		if ( pEnt )
		{
			pEnt->Spawn();
			trace_t tr;

			UTIL_TraceLine( vecPosCur, vecPosCur + Vector ( 0, 0, -128 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			Vector vecHint = tr.endpos +  Vector ( 0, 0, 16 );

			pEnt->SetAbsOrigin( vecHint );
			pEnt->SetOwnerEntity( this );
			pEnt->SetParent( this );

			m_hInstructorHint = pEnt;
		}
	}
}

void CHostageRescueZone::HostageRescueTouch( CBaseEntity *pOther )
{
	if ( m_bDisabled == false )
	{
		variant_t emptyVariant;
		pOther->AcceptInput( "OnRescueZoneTouch", nullptr, nullptr, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_hostage_rescue_zone_hint, CInfoInstructorHintHostageRescueZone );

void CInfoInstructorHintHostageRescueZone::Spawn_Impl( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, nullptr, nullptr );
}

class CBaseEntity;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: No defuse area

LINK_ENTITY_TO_CLASS( func_no_defuse, CNoDefuseArea );

void CNoDefuseArea::Spawn_Impl()
{
	InitTrigger();
	SetTouch( &CNoDefuseArea::NoDefuseAreaTouch );
}

void CNoDefuseArea::NoDefuseAreaTouch( CBaseEntity* pOther )
{
	CCSPlayer *player = dynamic_cast< CCSPlayer* >( pOther );
	// CT players are not allowed to defuse a bomb when they are in a no defuse zone.
	// the is to help prevent bugs with CTs being able to defuse bombs through walls and floors etc.
	if ( player )
	{
		player->m_bInNoDefuseArea = true;
	}
}

class CCSPlayer;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEconItemView;
class CTakeDamageInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

ConVar weapon_accuracy_logging( side_registry(), "weapon_accuracy_logging", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE );
ConVar weapon_near_empty_sound( side_registry(), "weapon_near_empty_sound", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar weapon_debug_max_inaccuracy( side_registry(), "weapon_debug_max_inaccuracy", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force all shots to have maximum inaccuracy" );
ConVar weapon_debug_inaccuracy_only_up( side_registry(), "weapon_debug_inaccuracy_only_up", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force weapon inaccuracy to be in exactly the up direction" );

	// Server doesn't play sounds anyway.
	void StartGroupingSounds() {}
	void EndGroupingSounds() {}
	void FX_WeaponSound ( int iPlayerIndex,
		uint16 nItemDefIndex,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		const CCSWeaponInfo *pWeaponInfo, float flSoundTime, int nPitch ) {};

ConVar debug_aim_angle(side_registry(), "debug_aim_angle", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);
	
// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayerIndex,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	CSWeaponID iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	float flSoundTime,
	WeaponSound_t sound_type,
	float flRecoilIndex
	)
{
	bool bDoEffects = true;

	if ( fInaccuracy > 1.0f )
		fInaccuracy = 1.0f;

	CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex) );

	if ( !pPlayer || iPlayerIndex < 0 )
	{
		// probably an env_gunfire
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( iWeaponID );
		FX_WeaponSound( iPlayerIndex, nItemDefIndex, sound_type, vOrigin, pWeaponInfo, flSoundTime, PITCH_NORM );
		// if this is server code, send the effect over to client as temp entity
		// Dispatch one message for all the bullet impacts and sounds.
		TE_FireBullets( 
			-1,
			nItemDefIndex,
			vOrigin, 
			vAngles, 
			iWeaponID,
			iMode,
			iSeed,
			fInaccuracy,
			fSpread,
			fAccuracyFishtail,
			sound_type,
			flRecoilIndex
			);
		return;
	}

	QAngle adjustedAngles = vAngles;
	adjustedAngles.y += fAccuracyFishtail;

	if ( pPlayer && debug_aim_angle.GetBool() )
	{
		QAngle old = pPlayer->EyeAngles() + pPlayer->GetAimPunchAngle();
		DevMsg("Server ");
		DevMsg("old: %f %f new: %f %f\n",
			old[YAW], old[PITCH],
			vAngles[YAW], vAngles[PITCH]
			);
		if ( debug_aim_angle.GetInt() == 2 )
		{
			adjustedAngles = old;
		}
	}

	const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinition( nItemDefIndex );
	if ( !pItemDef )
	{
		DevMsg( "FX_FireBullets: GetItemDefinition failed for defindex %d\n", nItemDefIndex );
		return;
	}

	if ( weapon_accuracy_logging.GetBool() )
	{
		char szFlags[256];

		V_strcpy(szFlags, " ");

// #if defined(CLIENT_DLL)
// 		V_strcat(szFlags, "CLIENT ", sizeof(szFlags));
// #else
// 		V_strcat(szFlags, "SERVER ", sizeof(szFlags));
// #endif
// 
		if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
			V_strcat(szFlags, "LADDER ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			V_strcat(szFlags, "GROUND ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING) )
			V_strcat(szFlags, "DUCKING ", sizeof(szFlags));

		float fVelocity = pPlayer->GetAbsVelocity().Length2D();

		Msg("FireBullets @ %10f [ %s ]: inaccuracy=%f  spread=%f  max dispersion=%f  mode=%2i  vel=%10f  seed=%3i  %s\n", 
			gpGlobals->curtime, pItemDef->GetItemBaseName(), fInaccuracy, fSpread, fInaccuracy + fSpread, iMode, fVelocity, iSeed, szFlags);
	}

	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( pItemDef->GetItemClass() );

	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		DevMsg("FX_FireBullets: LookupWeaponInfoSlot failed for weapon %s\n", pItemDef->GetItemBaseName() );
		return;
	}

	CCSWeaponInfo *pWeaponInfo = static_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	if ( !pWeaponInfo )
	{
		DevMsg( "FX_FireBullets: GetFileWeaponInfoFromHandle failed for weapon %s\n", pItemDef->GetItemBaseName() );
		return;
	}

	// Do the firing animation event.
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		if ( iMode == Primary_Mode )
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
		else
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_SECONDARY );
	}

	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_FireBullets( 
		iPlayerIndex,
		nItemDefIndex,
		vOrigin, 
		vAngles, 
		iWeaponID,
		iMode,
		iSeed,
		fInaccuracy,
		fSpread,
		fAccuracyFishtail,
		sound_type,
		flRecoilIndex
		);

	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	if ( pPlayer )
		pPlayer->NoteWeaponFired();

	bDoEffects = false; // no effects on server

	iSeed++;

	CWeaponCSBase* pWeapon = pPlayer ? pPlayer->GetActiveCSWeapon() : nullptr;
	CEconItemView* pItem = pWeapon ? pWeapon->GetEconItemView() : nullptr;

	int		iDamage = pWeaponInfo->GetDamage( pItem );
	float	flRange = pWeaponInfo->GetRange( pItem );
	float	flPenetration = pWeaponInfo->GetPenetration( pItem );
	float	flRangeModifier = pWeaponInfo->GetRangeModifier( pItem );
	int		iAmmoType = pWeaponInfo->GetPrimaryAmmoType( pItem );

	if ( bDoEffects)
	{
		static const float MaxPitchShiftInaccuracy = 0.05f;
		float flPitchShift = pWeaponInfo->GetInaccuracyPitchShift() * (fInaccuracy < MaxPitchShiftInaccuracy ? fInaccuracy : MaxPitchShiftInaccuracy);

		if ( sound_type == SINGLE && pWeaponInfo->GetInaccuracyAltSoundThreshhold() > 0.0f && fInaccuracy < pWeaponInfo->GetInaccuracyAltSoundThreshhold() )
		{
			sound_type = SINGLE_ACCURATE;
			flPitchShift = 0.0f;
		}

		FX_WeaponSound( iPlayerIndex, nItemDefIndex, sound_type, vOrigin, pWeaponInfo, flSoundTime, PITCH_NORM + int(flPitchShift) );

		// If the gun's nearly empty, also play a subtle "nearly-empty" sound, since the weapon 
		// is lighter and acoustically different when weighed down by fewer bullets.
		// But really it's so you get a fun low ammo warning from an audio cue.
		if ( weapon_near_empty_sound.GetBool() &&
			 pWeapon && pWeapon->GetMaxClip1() > 1 && // not a single-shot weapon
			 (((float)pWeapon->m_iClip1) / ((float)pWeapon->GetMaxClip1()) <= 0.2) ) // 20% or fewer bullets remaining
		{
			FX_WeaponSound( iPlayerIndex, nItemDefIndex, NEARLYEMPTY, vOrigin, pWeaponInfo, flSoundTime, PITCH_NORM );
		}
	}

	// Fire bullets, calculate impacts & effects

	if ( !pPlayer )
		return;
	
	StartGroupingSounds();

	pPlayer->StartNewBulletGroup();

	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, vOrigin, vAngles, flRange );

	// [sbodenbender] rumble when shooting
	// since we are handling bullet fx in CS differently than other titles, call 
	// rumble effect directly instead of Player::RumbleEffect
	//=============================================================================

	bool bForceMaxInaccuracy = weapon_debug_max_inaccuracy.GetBool();
	bool bForceInaccuracyDirection = weapon_debug_inaccuracy_only_up.GetBool();

	RandomSeed( iSeed );	// init random system with this seed

	// Accuracy curve density adjustment FOR R8 REVOLVER SECONDARY FIRE, NEGEV WILD BEAST
	float flRadiusCurveDensity = RandomFloat();
	if ( nItemDefIndex == 64 && iMode == Secondary_Mode ) /*R8 REVOLVER SECONDARY FIRE*/
	{
		flRadiusCurveDensity = 1.0f - flRadiusCurveDensity*flRadiusCurveDensity;
	}
	if ( nItemDefIndex == 28 && flRecoilIndex < 3 ) /*NEGEV WILD BEAST*/
	{
		for ( int j = 3; j > flRecoilIndex; -- j )
		{
			flRadiusCurveDensity *= flRadiusCurveDensity;
		}
		flRadiusCurveDensity = 1.0f - flRadiusCurveDensity;
	}

	if ( bForceMaxInaccuracy )
		flRadiusCurveDensity = 1.0f;

	// Get accuracy displacement
	float fTheta0 = RandomFloat(0.0f, 2.0f * std::numbers::pi);
	
	if ( bForceInaccuracyDirection )
		fTheta0 = std::numbers::pi * 0.5f;

	float fRadius0 = flRadiusCurveDensity * fInaccuracy;
	float x0 = fRadius0 * std::cosf(fTheta0);
	float y0 = fRadius0 * std::sinf(fTheta0);

	const int kMaxBullets = 16;
	float x1[kMaxBullets], y1[kMaxBullets];
	contract_assert( pWeaponInfo->GetBullets() <= kMaxBullets );

	// the RNG can be desynchronized by FireBullet(), so pre-generate all spread offsets
	for ( int iBullet=0; iBullet < pWeaponInfo->GetBullets(); iBullet++ )
	{
		// Spread curve density adjustment for R8 REVOLVER SECONDARY FIRE, NEGEV WILD BEAST
		float flSpreadCurveDensity = RandomFloat();
		if ( nItemDefIndex == 64 && iMode == Secondary_Mode )
		{
			flSpreadCurveDensity = 1.0f - flSpreadCurveDensity*flSpreadCurveDensity;
		}
		if ( nItemDefIndex == 28 && flRecoilIndex < 3 ) /*NEGEV WILD BEAST*/
		{
			for ( int j = 3; j > flRecoilIndex; --j )
			{
				flSpreadCurveDensity *= flSpreadCurveDensity;
			}
			flSpreadCurveDensity = 1.0f - flSpreadCurveDensity;
		}

		if ( bForceMaxInaccuracy )
			flSpreadCurveDensity = 1.0f;

		float fTheta1 = RandomFloat(0.0f, 2.0f * std::numbers::pi);
		if ( bForceInaccuracyDirection )
			fTheta1 = std::numbers::pi * 0.5f;

		float fRadius1 = flSpreadCurveDensity * fSpread;
		x1[iBullet] = fRadius1 * cosf(fTheta1);
		y1[iBullet] = fRadius1 * sinf(fTheta1);
	}

	{	/// Make sure take damage listener stays in scope only for the duration of FireBullet loop below!
	class CFireBulletTakeDamageListener : public CCSPlayer::ITakeDamageListener
	{
	public:
		CFireBulletTakeDamageListener( CCSPlayer *pPlayerShooting ) :
			m_pPlayerShooting(pPlayerShooting),
			m_bEnemyHit( false ),
			m_bShotFiredAndOnTargetRecorded( false )
		{}
		virtual void OnTakeDamageListenerCallback( CCSPlayer *pVictim, CTakeDamageInfo &infoTweakable ) override
		{
			if ( m_pPlayerShooting && pVictim->IsOtherEnemy( m_pPlayerShooting ) )
			{
				m_bEnemyHit = true;

				if ( infoTweakable.GetDamageType() & DMG_HEADSHOT )
				{
					m_rbHsPlayers.insert( pVictim );	// remember that at least one pellet hit a headshot
				}
				else if ( m_rbHsPlayers.contains( pVictim ) )
				{
					infoTweakable.SetDamageType( infoTweakable.GetDamageType() | DMG_HEADSHOT );	// since previous pellets hit a headshot we visualize it as a headshot
				}

				// Since we know that bullet was fired and that we hit the target
				// we should record the accuracy stats right now, otherwise we may TerminateRound
				// based on a kill from this bullet and not have this data recorded
				RecordShotFiredAndOnTargetData();
			}
		}
		void BulletBurstCompleted()
		{
			RecordShotFiredAndOnTargetData();
		}
	private:
		void RecordShotFiredAndOnTargetData()
		{
			if ( m_bShotFiredAndOnTargetRecorded )
				return;
			m_bShotFiredAndOnTargetRecorded = true;

			if ( m_pPlayerShooting && CSGameRules() && !CSGameRules()->IsWarmupPeriod() && !m_pPlayerShooting->IsBot() )
			{
				// Track in QMM total number of shots that connected with an opponent
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_pPlayerShooting->GetHumanPlayerAccountID() ) )
				{
					++pQMM->m_numShotsFiredTotal;
					if ( m_bEnemyHit )
						++pQMM->m_numShotsOnTargetTotal;
				}
			}
		}
	private:
		CCSPlayer *m_pPlayerShooting;
		bool m_bEnemyHit;
		bool m_bShotFiredAndOnTargetRecorded;
		std::set< CCSPlayer * > m_rbHsPlayers;	// players who were dinked in the head as part of this bullet batch
	} fbtdl( pPlayer );

	for ( int iBullet=0; iBullet < pWeaponInfo->GetBullets(); iBullet++ )
	{
		if ( !pPlayer )
			break;

		int nPenetrationCount = 4;

		pPlayer->FireBullet(
			vOrigin,
			adjustedAngles,
			flRange,
			flPenetration,
			nPenetrationCount,
			iAmmoType,
			iDamage,
			flRangeModifier,
			pPlayer,
			bDoEffects,
			x0 + x1[iBullet], y0 + y1[iBullet]
			);
	}

	fbtdl.BulletBurstCompleted();
	} /// Closes the lifetime scope of take damage listener in scope only for the duration of FireBullet loop above.

	lagcompensation->FinishLagCompensation( pPlayer );

	EndGroupingSounds();
}

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
void FX_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex) );

	// Do the firing animation event.
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		switch ( option )
		{
		case PLANTBOMB_PLANT:
			{
				pPlayer->DoAnimStateEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
			}
			break;

		case PLANTBOMB_ABORT:
			{
				pPlayer->DoAnimStateEvent( PLAYERANIMEVENT_CLEAR_FIRING );
			}
			break;
		}
	}

	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_PlantBomb( iPlayerIndex, vOrigin, option );
}

class CBaseCombatCharacter;
class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
struct inputdata_t;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_fraggrenade_dropped.mdl"

LINK_ENTITY_TO_CLASS( hegrenade_projectile, CHEGrenadeProjectile );
PRECACHE_REGISTER( hegrenade_projectile );

IMPLEMENT_REFLECT_DATAMAP( CHEGrenadeProjectile )

CHEGrenadeProjectile* CHEGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner, 
	const CCSWeaponInfo& weaponInfo,
	float timer )
{
	CHEGrenadeProjectile *pGrenade = (CHEGrenadeProjectile*)CBaseEntity::Create( "hegrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.

	pGrenade->SetDetonateTimerLength( 1.5 );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner ); 

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );	

	// make NPCs afaid of it while in the air
	pGrenade->SetThink( &CHEGrenadeProjectile::DangerSoundThink );
	pGrenade->SetNextThink( gpGlobals->curtime );

	pGrenade->m_pWeaponInfo = &weaponInfo;

	pGrenade->m_flDamage = (float)weaponInfo.GetDamage();		//	100;
	pGrenade->m_DmgRadius = weaponInfo.GetRange();			// pGrenade->m_flDamage * 3.5f;
	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CHEGrenadeProjectile::Spawn_Impl()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn_Impl();

	SetBodygroupPreset( "thrown" );
}

void CHEGrenadeProjectile::Precache_Impl()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "HEGrenade.Bounce" );

	BaseClass::Precache_Impl();
}

void CHEGrenadeProjectile::BounceSound( void )
{
	EmitSound( "HEGrenade.Bounce" );
}

void CHEGrenadeProjectile::Detonate()
{
	// tell the bots an HE grenade has exploded (and record the event in the log)
	if ( CCSPlayer *player = ToCSPlayer( GetThrower() ) )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "hegrenade_detonate" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	BaseClass::Detonate();
}

const char *CHEGrenadeProjectile::GetParticleSystemName( int pointContents, surfacedata_t *pdata )
{
	if ( pointContents & MASK_WATER )
		return "explosion_basic_water";
	
	// [msmith] If the grenade goes off near smoke, then we need to make sure that it doesn't
	// spawn any of it's own smoke (the explosion_hegrenade_brief effect has no smoke).
	// This fixes an exploit that allowed players to "see through" the smokegrenade smoke.
	const Vector *detonatePosition = &GetAbsOrigin();
	if ( TheBots->IsInsideSmokeCloud( detonatePosition, HEGrenadeRadius ) )
		return "explosion_hegrenade_brief";

	if ( pdata )
	{

		switch( pdata->game.material )
		{
		case CHAR_TEX_DIRT:
		case CHAR_TEX_SAND:
		case CHAR_TEX_GRASS:
		case CHAR_TEX_MUD:
		case CHAR_TEX_FOLIAGE:
			return "explosion_hegrenade_dirt";

		case CHAR_TEX_SNOW:
			return "explosion_hegrenade_snow";
		}
	}

	return "explosion_basic";
}

void CHEGrenadeProjectile::InitializeSpawnFromWorld( inputdata_t &inputdata )
{
	SetDetonateTimerLength( 1.5 );

	SetGravity( GetGrenadeGravity() );
	SetFriction( GetGrenadeFriction() );
	SetElasticity( GetGrenadeElasticity() );

	//pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	ApplyLocalAngularVelocityImpulse( AngularImpulse(600,random_valve->RandomInt(-1200,1200),0) );

	// make NPCs afaid of it while in the air
	SetThink( &CHEGrenadeProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	m_pWeaponInfo = GetWeaponInfo( WEAPON_HEGRENADE );

	//m_flDamage = (float)m_pWeaponInfo.GetDamage();		//	100;
	//m_DmgRadius = m_pWeaponInfo.GetRange();			// pGrenade->m_flDamage * 3.5f;
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
}

#undef GRENADE_MODEL

using namespace ::ks::reflect;

//---------------------------------------------------------
const char *CInferno::GetParticleEffectName()
{
	return "molotov_groundfire";
}
const char *CInferno::GetImpactParticleEffectName()
{
	return "molotov_explosion";
}

//---------------------------------------------------------
const char *CFireCrackerBlast::GetParticleEffectName()
{
	return "firework_crate_ground_effect";
}
const char *CFireCrackerBlast::GetImpactParticleEffectName()
{
	return "firework_crate_explosion_01";
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_DATAMAP( CInfoViewParameters )

LINK_ENTITY_TO_CLASS( info_view_parameters, CInfoViewParameters );

class CBasePlayer;
class CCSPlayer;
class CItem;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
class CItemAmmo : public CItem
{
public:
	DECLARE_CLASS( CItemAmmo, CItem );

	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}

		int ammoIndex = GetCSAmmoDef()->Index( GetAmmoName() );
		if ( ammoIndex < 0 )
		{
			contract_assert( false );
			return false;
		}

		pPlayer->GiveAmmo( GetCSAmmoDef()->GetBuySize( ammoIndex ), ammoIndex );

		return true;
	}

	virtual const char * GetAmmoName( void ) const { return nullptr; }
};

//-----------------------------------------------------------------------------
class CItemAmmo50AE : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo50AE, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_50AE; }
};

LINK_ENTITY_TO_CLASS( ammo_50ae, CItemAmmo50AE );

//-----------------------------------------------------------------------------
class CItemAmmo762MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo762MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_762MM; }
};

LINK_ENTITY_TO_CLASS( ammo_762mm, CItemAmmo762MM );

//-----------------------------------------------------------------------------
class CItemAmmo556MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo556MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_556MM; }
};

LINK_ENTITY_TO_CLASS( ammo_556mm, CItemAmmo556MM );

class CItemAmmo556MM_SMALL : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo556MM_SMALL, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_556MM_SMALL; }
};

LINK_ENTITY_TO_CLASS( ammo_556mm_small, CItemAmmo556MM_SMALL );

//-----------------------------------------------------------------------------
class CItemAmmo556MM_BOX : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo556MM_BOX, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_556MM_BOX; }
};

LINK_ENTITY_TO_CLASS( ammo_556mm_box, CItemAmmo556MM_BOX );

//-----------------------------------------------------------------------------
class CItemAmmo338MAG : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo338MAG, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_338MAG; }
};

LINK_ENTITY_TO_CLASS( ammo_338mag, CItemAmmo338MAG );

//-----------------------------------------------------------------------------
class CItemAmmo9MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo9MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_9MM; }
};

LINK_ENTITY_TO_CLASS( ammo_9mm, CItemAmmo9MM );

//-----------------------------------------------------------------------------
class CItemAmmoBuckshot : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmoBuckshot, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_BUCKSHOT; }
};

LINK_ENTITY_TO_CLASS( ammo_buckshot, CItemAmmoBuckshot );

//-----------------------------------------------------------------------------
class CItemAmmo45ACP : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo45ACP, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_45ACP; }
};

LINK_ENTITY_TO_CLASS( ammo_45acp, CItemAmmo45ACP );

//-----------------------------------------------------------------------------
class CItemAmmo357SIG : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo357SIG, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_357SIG; }
};

LINK_ENTITY_TO_CLASS( ammo_357sig, CItemAmmo357SIG );

//-----------------------------------------------------------------------------
class CItemAmmo357SIG_P250 : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo357SIG_P250, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_357SIG_P250; }
};

LINK_ENTITY_TO_CLASS( ammo_357sig_p250, CItemAmmo357SIG_P250 );

//-----------------------------------------------------------------------------
class CItemAmmo357SIG_SMALL : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo357SIG_SMALL, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_357SIG_SMALL; }
};

LINK_ENTITY_TO_CLASS( ammo_357sig_small, CItemAmmo357SIG_SMALL );

//-----------------------------------------------------------------------------
class CItemAmmo357SIG_MIN : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo357SIG_MIN, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_357SIG_MIN; }
};

LINK_ENTITY_TO_CLASS( ammo_357sig_np_reserve, CItemAmmo357SIG_MIN );

//-----------------------------------------------------------------------------
class CItemAmmo57MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo57MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_57MM; }
};

LINK_ENTITY_TO_CLASS( ammo_57mm, CItemAmmo57MM );

//-----------------------------------------------------------------------------

class CBasePlayer;
class CCSPlayer;
class CItem;

using namespace ::ks::reflect;

class CItemAssaultSuit : public CItem
{
	public: void Spawn_Impl( void )
	{ 
		Precache( );
		CItem::Spawn_Impl( );
	}
	private:
	
	public: void Precache_Impl( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	private:
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 100 );
		pPlayer->RecalculateCurrentEquipmentValue();

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;		
	}
};

LINK_ENTITY_TO_CLASS( item_assaultsuit, CItemAssaultSuit );

class CItemHeavyAssaultSuit : public CItemAssaultSuit
{
// 	void Spawn( void )
// 	{
// 		Precache();
// 		CItem::Spawn();
// 	}

// 	void Precache( void )
// 	{
// 		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
// 	}

	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 200 );
		pPlayer->RecalculateCurrentEquipmentValue();
		pPlayer->m_bHasHeavyArmor = true;

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				pPlayer->EmitSound("Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				pPlayer->EmitSound("Player.EquipArmor_T");
			}
			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_heavyassaultsuit, CItemHeavyAssaultSuit );

class CBaseEntity;
class CCSPlayer;
class CItem;

using namespace ::ks::reflect;

// Purpose: Defuser kit that drops from counter-strike CTS

class CItemDefuser : public CItem
{
public:
	DECLARE_CLASS( CItemDefuser, CItem );
	
	CItemDefuser();
	~CItemDefuser();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	DefuserTouch( CBaseEntity *pOther );
	void	ActivateThink( void );
	
};

LINK_ENTITY_TO_CLASS( item_defuser, CItemDefuser );
LINK_ENTITY_TO_CLASS_ALIAS( item_cutters, CItemDefuser );
PRECACHE_REGISTER(item_defuser);

Vector g_vecDefuserPosition = vec3_origin;
CBaseEntity* g_pDefuserEntity = nullptr;

CItemDefuser::CItemDefuser()
{
	g_pDefuserEntity = this;
	SetSpotRules( CCSEntitySpotting::SPOT_RULE_T | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_CT );
}

CItemDefuser::~CItemDefuser()
{
	if ( g_pDefuserEntity == this )
	{
		g_pDefuserEntity = nullptr;
		g_vecDefuserPosition = vec3_origin;
	}
}

void CItemDefuser::Spawn_Impl( void )
{ 
	Precache( );
	SetModel( "models/weapons/w_defuser.mdl" );
	BaseClass::Spawn_Impl();
	
	if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "defuser_dropped" );
		if ( event )
		{
			event->SetInt( "entityid", entindex() );
			event->SetInt( "priority", 0 ); //defuser_dropped

			gameeventmanager->FireEvent( event );
		}
	}

	g_vecDefuserPosition = GetAbsOrigin();

	SetNextThink( gpGlobals->curtime + 0.5f );
	SetThink( &CItemDefuser::ActivateThink );

	SetTouch( nullptr );
}
	
void CItemDefuser::Precache_Impl( void )
{
	PrecacheModel( "models/weapons/w_defuser.mdl" );

	PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
}

void CItemDefuser::ActivateThink( void )
{
	//since we can't stop the item from being touched while its in the air,
	//activate 1 second after being dropped

	SetTouch( &CItemDefuser::DefuserTouch );
	SetThink( nullptr );
}
	
void CItemDefuser::DefuserTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	//if( GetFlags() & FL_ONGROUND )
	{
		CCSPlayer *pPlayer = (CCSPlayer *)pOther;

		if ( !pPlayer )
		{
			contract_assert( false );
			return;
		}

		if( pPlayer->GetTeamNumber() == TEAM_CT && !pPlayer->HasDefuser() )
		{
			// [dwenger] Added for fun-fact support
			pPlayer->GiveDefuser( true );

			if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "defuser_pickup" );
				if ( event )
				{
					event->SetInt( "entityid", entindex() );
					event->SetInt( "userid", pPlayer->GetUserID() );
					event->SetInt( "priority", 0 ); //defuser_pickup
					gameeventmanager->FireEvent( event );
				}
			}

// 			if ( pPlayer->IsDead() == false )
// 			{
//				CBroadcastRecipientFilter filter;
// 				EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
// 			}

			UTIL_Remove( this );
			return;
		}	
	}
}

class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Healthshot Item, belt item

static constexpr auto HEALTHSHOT_INJECT_TIME = 1.65f;

IMPLEMENT_NETWORKCLASS_ALIASED( Item_Healthshot, DT_Item_Healthshot )

IMPLEMENT_REFLECT_TABLE( CItem_Healthshot, DT_Item_Healthshot );

LINK_ENTITY_TO_CLASS_ALIASED( weapon_healthshot, Item_Healthshot );
PRECACHE_WEAPON_REGISTER( weapon_healthshot );

// #ifndef CLIENT_DLL
// BEGIN_DATADESC( CItem_Healthshot )
// END_DATADESC()
// #endif

ConVar healthshot_health( side_registry(), "healthshot_health", "50", FCVAR_CHEAT | FCVAR_REPLICATED );

void CItem_Healthshot::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;
	
	SendWeaponAnim( ACT_VM_IDLE );

	SetWeaponIdleTime( gpGlobals->curtime + 20 );
}

void CItem_Healthshot::Precache_Impl( void )
{
	//PrecacheScriptSound( "Healthshot.Shot" );

	BaseClass::Precache_Impl();
}

bool CItem_Healthshot::CanPrimaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( pPlayer == nullptr )
		return false;

	return CanUseOnSelf( pPlayer );
}

bool CItem_Healthshot::CanUseOnSelf( CCSPlayer *pPlayer )	
{ 
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetHealth() >= pPlayer->GetMaxHealth() )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Healthshot_AlreadyAtMax" );
		return false;
	}

	return true;
}

void CItem_Healthshot::DropHealthshot( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	int iAmount = pPlayer->GetAmmoCount( GetPrimaryAmmoType() );
	if ( iAmount <= 1 )
	{
		pPlayer->CSWeaponDrop( this, false, true );
		return;
	}
	else	
	{
		pPlayer->RemoveAmmo( 1, AMMO_TYPE_HEALTHSHOT );

		CItem_Healthshot *pHealth = static_cast< CItem_Healthshot * >( CreateEntityByName( "weapon_healthshot" ) );

		if ( pHealth )
		{
			Vector vecWeaponThrowFromPos = pPlayer->EyePosition();
			QAngle angWeaponThrowFromAngle = pPlayer->EyeAngles();

 			Vector vForward;
 			AngleVectors(angWeaponThrowFromAngle, &vForward, nullptr, nullptr);
 			vecWeaponThrowFromPos = vecWeaponThrowFromPos + (vForward * 100);
			//NDebugOverlay::Box( vecWeaponThrowFromPos, Vector( 10, 10, 10 ), Vector( -10, -10, -10 ), 255, 0, 0, 200, 3 );

			DispatchSpawn( pHealth );

			// set it non-solid because it hits itself during the trace when trying to throw it
			pHealth->SetSolidFlags( FSOLID_NOT_SOLID );
			pHealth->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

			pPlayer->Weapon_Drop( pHealth, &vecWeaponThrowFromPos, nullptr );			

			pHealth->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
			pHealth->SetPreviousOwner( pPlayer );
			pHealth->m_flDroppedAtTime = gpGlobals->curtime;
			pHealth->AddPriorOwner( pPlayer );
		}
	}

}

//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::OnStartUse( CCSPlayer *pPlayer )
{
	SetWeaponIdleTime( gpGlobals->curtime + 20 ); // don't fidget for a bit
	BaseClass::OnStartUse( pPlayer );
}

//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::CompleteUse( CCSPlayer *pPlayer )
{
	pPlayer->OnHealthshotUsed();

	// Give half health buffer
	pPlayer->SetHealth( Min( pPlayer->GetHealth() + healthshot_health.GetInt(), pPlayer->GetMaxHealth() ) );

	// emit event	
// 	IGameEvent *event = gameeventmanager->CreateEvent( "healthshot_used" );
// 	if( event )
// 	{
// 		int userID = pPlayer->GetUserID();
// 		event->SetInt( "userid", userID );
// 		event->SetInt( "subject", userID );
// 		gameeventmanager->FireEvent( event );
// 	}

	BaseClass::CompleteUse( pPlayer );
}

float CItem_Healthshot::GetUseTimerDuration( void )
{
	return HEALTHSHOT_INJECT_TIME;
}

class CBasePlayer;
class CCSPlayer;
class CItem;

using namespace ::ks::reflect;

class CItemKevlar : public CItem
{
public:

	DECLARE_CLASS( CItemKevlar, CItem );

	void Spawn_Impl( void )
	{ 
		Precache( );
		BaseClass::Spawn_Impl( );
	}
	
	void Precache_Impl( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}
		
		pPlayer->SetArmorValue( 100 );
		pPlayer->RecalculateCurrentEquipmentValue();

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;
			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_kevlar, CItemKevlar );

class CBasePlayer;
class CCSPlayer;
class CItem;

using namespace ::ks::reflect;

class CItemNvgs : public CItem
{
public:

	DECLARE_CLASS( CItemNvgs, CItem );

	void Spawn_Impl( void )
	{ 
		Precache( );
		BaseClass::Spawn_Impl( );
	}
	
	void Precache_Impl( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}

		pPlayer->m_bHasNightVision = true;

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_nvgs, CItemNvgs );

struct inputdata_t;

using namespace ::ks::reflect;

LINK_ENTITY_TO_CLASS( info_map_parameters, CMapInfo );

IMPLEMENT_REFLECT_DATAMAP( CMapInfo )

CMapInfo *g_pMapInfo = nullptr;

CMapInfo::CMapInfo()
{
	m_flBombRadius = 500.0f;
	m_iBuyingStatus = 0;
	m_flBotMaxVisionDistance = -1.0f;

	if ( g_pMapInfo )
	{
		// Should only be one of these.
		Warning( "Warning: Multiple info_map_parameters entities in map!\n" );
	}
	else
	{
		g_pMapInfo = this;
	}
}

CMapInfo::~CMapInfo()
{
	if ( g_pMapInfo == this )
		g_pMapInfo = nullptr;
}
 
bool CMapInfo::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "buying"))
	{
		m_iBuyingStatus = std::atoi(szValue);
		return true;
	}
	else if (FStrEq(szKeyName, "bombradius"))
	{
		m_flBombRadius = (float)(std::atoi(szValue));
		if (m_flBombRadius > 2048)
			m_flBombRadius = 2048;
		
		return true;
	}
	
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

void CMapInfo::Spawn_Impl( void )
{ 
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
}

void CMapInfo::InputFireWinCondition(inputdata_t &inputdata )
{
	CSGameRules()->TerminateRound( 5, inputdata.value.Int() );
}

class CBaseCombatCharacter;
class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

ConVar molotov_throw_detonate_time( side_registry(), "molotov_throw_detonate_time", "2.0", FCVAR_CHEAT | FCVAR_REPLICATED );

ConVar weapon_molotov_maxdetonateslope( 
	side_registry(), "weapon_molotov_maxdetonateslope",
	"30.0",
	FCVAR_REPLICATED,
	"Maximum angle of slope on which the molotov will detonate",
	true, 0.0,
	true, 90.0 );

inline constexpr const char *MOLOTOV_MODEL = "models/Weapons/w_eq_molotov_dropped.mdl";
inline constexpr const char *INCGREN_MODEL = "models/Weapons/w_eq_incendiarygrenade_dropped.mdl";

LINK_ENTITY_TO_CLASS( molotov_projectile, CMolotovProjectile );
PRECACHE_REGISTER( molotov_projectile );

IMPLEMENT_REFLECT_DATAMAP( CMolotovProjectile )

IMPLEMENT_REFLECT_SERVERCLASS( CMolotovProjectile, DT_MolotovProjectile )

CMolotovProjectile *CMolotovProjectile::Create( const Vector &position, const QAngle &angles, 
												const Vector &velocity, const AngularImpulse &angVelocity, 
												CBaseCombatCharacter *owner, const CCSWeaponInfo& weaponInfo )
{
	CMolotovProjectile *molotov = (CMolotovProjectile *)CBaseEntity::Create( "molotov_projectile", position, angles, owner );
	UTIL_LogPrintf( "Molotov projectile spawned at %f %f %f, velocity %f %f %f\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z );

	molotov->SetDetonateTimerLength( molotov_throw_detonate_time.GetFloat() );
	molotov->SetAbsVelocity( velocity );
	molotov->SetupInitialTransmittedGrenadeVelocity( velocity );
	molotov->SetThrower( owner ); 
	molotov->m_pWeaponInfo = &weaponInfo;
	molotov->SetIsIncGrenade( weaponInfo.m_weaponId == WEAPON_INCGRENADE );

	if ( molotov->m_bIsIncGrenade )
		molotov->SetModel( INCGREN_MODEL );
	else
		molotov->SetModel( MOLOTOV_MODEL );

	molotov->SetGravity( BaseClass::GetGrenadeGravity() );
	molotov->SetFriction( BaseClass::GetGrenadeFriction() );
	molotov->SetElasticity( BaseClass::GetGrenadeElasticity() );

	molotov->SetTouch( &CMolotovProjectile::BounceTouch );
	molotov->SetThink( &CMolotovProjectile::DetonateThink );
	molotov->SetNextThink( gpGlobals->curtime + 2.0f );

	molotov->m_flDamage = 200.0f;
	molotov->m_DmgRadius = 300.0f;
	molotov->ChangeTeam( owner->GetTeamNumber() );
	molotov->ApplyLocalAngularVelocityImpulse( angVelocity );	

	// make NPCs afaid of it while in the air
	molotov->SetThink( &CMolotovProjectile::DangerSoundThink );
	molotov->SetNextThink( gpGlobals->curtime );

	molotov->EmitSound( "Molotov.Throw" );
	molotov->EmitSound( "Molotov.Loop" );
	molotov->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	// we have to reset these here because we set the model late and it resets the collision
	Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
	Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );
	molotov->SetSize( min, max );
	if ( molotov->CollisionProp() )
		molotov->CollisionProp()->SetCollisionBounds( min, max );

	return molotov;
}

void CMolotovProjectile::InitializeSpawnFromWorld( inputdata_t &inputdata )
{
	SetDetonateTimerLength( molotov_throw_detonate_time.GetFloat() );

	SetGravity( GetGrenadeGravity() );
	SetFriction( GetGrenadeFriction() );
	SetElasticity( GetGrenadeElasticity() );
	SetIsIncGrenade( false );

	SetTouch( &CMolotovProjectile::BounceTouch );
	SetThink( &CMolotovProjectile::DetonateThink );
	SetNextThink( gpGlobals->curtime + 2.0f );

	//pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	ApplyLocalAngularVelocityImpulse( AngularImpulse( 600, random_valve->RandomInt( -1200, 1200 ), 0 ) );

	// make NPCs afaid of it while in the air
	SetThink( &CMolotovProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	EmitSound( "Molotov.Throw" );
	EmitSound( "Molotov.Loop" );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	// we have to reset these here because we set the model late and it resets the collision
	Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
	Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );
	SetSize( min, max );
	if ( CollisionProp() )
		CollisionProp()->SetCollisionBounds( min, max );

	m_pWeaponInfo = GetWeaponInfo( WEAPON_MOLOTOV );
}

void CMolotovProjectile::Spawn_Impl( void )
{
	m_stillTimer.Invalidate();
	m_throwDetTimer.Invalidate();

	BaseClass::Spawn_Impl();

	if ( this->m_bIsIncGrenade )
	{
		SetModel( INCGREN_MODEL );
		SetBodygroupPreset( "thrown" );
	}
	else
	{
		SetModel( MOLOTOV_MODEL );
	}

}

void CMolotovProjectile::Precache_Impl( void )
{
	PrecacheModel( MOLOTOV_MODEL );
	PrecacheModel( INCGREN_MODEL );

	PrecacheScriptSound( "Molotov.Throw" );
	PrecacheScriptSound( "Molotov.Loop" );
	PrecacheParticleSystem( "weapon_molotov_thrown" );
	PrecacheParticleSystem( "weapon_molotov_held" );
	PrecacheParticleSystem( "incgrenade_thrown_trail" );

	BaseClass::Precache_Impl();
}

void CMolotovProjectile::BounceTouch( CBaseEntity *other )
{
	if ( other->IsSolidFlagSet( FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS ) )
		return;

	// don't hit the guy that launched this grenade
	if ( other == GetThrower() )
		return;

	if ( FClassnameIs( other, "func_breakable" ) )
	{
		return;
	}

	if ( FClassnameIs( other, "func_breakable_surf" ) )
	{
		return;
	}

	// don't detonate on ladders
	if ( FClassnameIs( other, "func_ladder" ) )
	{
		return;
	}

	// Deal car alarms direct damage to set them off - flames won't do so
	if ( FClassnameIs( other, "prop_car_alarm" ) || FClassnameIs( other, "prop_car_glass" ) )
	{
		CTakeDamageInfo info( this, GetThrower(), 10, DMG_GENERIC );
		other->OnTakeDamage( info );
	}

	const trace_t &hitTrace = GetTouchTrace();
	if ( hitTrace.Ent<CBaseEntity>() && hitTrace.Ent<CBaseEntity>()->MyCombatCharacterPointer() )
	{
		// don't break if we hit an actor - wait until we hit the environment
		return;
	}
	else
	{
		// only detonate on surfaces less steep than this
		const float kMinCos = std::cosf(DEG2RAD(weapon_molotov_maxdetonateslope.GetFloat()));
		if ( hitTrace.plane.normal.z >= kMinCos )
		{
			Detonate();
		}
	}
}

void CMolotovProjectile::BounceSound( void )
{
	if ( m_bIsIncGrenade )
		EmitSound( "IncGrenade.Bounce" );
	else
		EmitSound( "GlassBottle.ImpactHard" );
}

void CMolotovProjectile::DetonateThink( void )
{
// 	if( gpGlobals->curtime > m_flDetonateTime )
// 	{
// 		Detonate();
// 		return;
// 	}

	if ( GetAbsVelocity().IsLengthGreaterThan( 5.0f ) )
	{
		m_stillTimer.Invalidate();
	}
	else if ( !m_stillTimer.HasStarted() )
	{
		m_stillTimer.Start();
	}

	const float StillDetonateTime = 0.5f;
	if ( m_stillTimer.HasStarted() && m_stillTimer.GetElapsedTime() > StillDetonateTime )
	{
		Detonate();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CMolotovProjectile::Detonate( void )
{
	//BaseClass::Detonate();

	const trace_t &hitTrace = GetTouchTrace();
	if ( hitTrace.surface.flags & SURF_SKY )
		return;

	// tell the bots an HE grenade has exploded
	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "molotov_detonate" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	Vector burnPos, splashNormal;

	if ( TraceDidHitWorld( hitTrace ) )
	{
		// hit the world, just explode at that position
		burnPos = hitTrace.endpos;
		splashNormal = hitTrace.plane.normal;
	}
	else
	{
		// exploded in the air, or hit an object or player.
		// find the world normal under them (if close enough) and explode there
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin() + Vector( 0, 0, 10 ), GetAbsOrigin() + Vector( 0, 0, -128.0f ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction == 1 )
		{
			// Too high, just play explosion effect and don't start a fire
			if ( m_bIsIncGrenade )
				EmitSound( "Inferno.Start_IncGrenade" );
			else
				EmitSound( "Inferno.Start" );

			TheBots->SetGrenadeRadius( this, MolotovGrenadeRadius );
			StopSound( "Molotov.Loop" );

			DispatchParticleEffect( "explosion_molotov_air", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

			Vector vecAbsOrigin = GetAbsOrigin();
			CPASFilter filter( vecAbsOrigin );
			te->Explosion( filter, -1.0, // don't apply cl_interp delay
				vecAbsOrigin,
				0,
				32,
				25,
				TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOFIREBALL | TE_EXPLFLAG_NOPARTICLES,
				152,
				50 );

			UTIL_Remove( this );
			return;
		}
		else if( tr.surface.flags & SURF_SKY )
		{
			// just bounce
			return;
		}

		// otherwise explode normally
		burnPos = tr.endpos;
		splashNormal = tr.plane.normal;
	}
	
	TheBots->SetGrenadeRadius( this, MolotovGrenadeRadius );

	CInferno *inferno = (CInferno *)CBaseEntity::Create( "inferno", burnPos, QAngle( 0, 0, 0 ), GetThrower() );
	Vector vBurnDir = m_vInitialVelocity;
	vBurnDir.NormalizeInPlace();
	vBurnDir *= GetAbsVelocity().Length();
	inferno->SetSourceWeaponInfo( m_pWeaponInfo );

	if ( m_bIsIncGrenade )
		inferno->SetInfernoType( INFERNO_TYPE_INCGREN_FIRE );
	else
		inferno->SetInfernoType( INFERNO_TYPE_FIRE );
	inferno->StartBurning( burnPos, splashNormal, vBurnDir );

	StopSound( "Molotov.Loop" );
	
	UTIL_Remove( this );
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// player pickup utility routine
void Pickup_ForcePlayerToDropThisObject( CBaseEntity *pTarget )
{
	Warning ( "Failing to force player to drop object.\n" );
	contract_assert( false && "This function is assumed to not be needed in portal 2, if this assert fires we need to fix it." );
}

void Pickup_OnPhysGunDrop( CBaseEntity *pDroppedObject, CBasePlayer *pPlayer, PhysGunDrop_t Reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pDroppedObject);
	if ( pPickup )
	{
		pPickup->OnPhysGunDrop( pPlayer, Reason );
	}
}

void Pickup_OnPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		pPickup->OnPhysGunPickup( pPlayer, reason );
	}

	// send phys gun pickup item event, but only in single player
	if ( !g_pGameRules->IsMultiplayer() )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "physgun_pickup" );
		if ( event )
		{
			event->SetInt( "entindex", pPickedUpObject->entindex() );
			gameeventmanager->FireEvent( event );
		}
	}
}

bool Pickup_OnAttemptPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		return pPickup->OnAttemptPhysGunPickup( pPlayer, reason );
	}
	return true;
}

CBaseEntity	*Pickup_OnFailedPhysGunPickup( CBaseEntity *pPickedUpObject, Vector vPhysgunPos )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pPickedUpObject);
	if ( pPickup )
	{
		return pPickup->OnFailedPhysGunPickup( vPhysgunPos );
	}

	return nullptr;
}

bool Pickup_GetPreferredCarryAngles( CBaseEntity *pObject, CBasePlayer *pPlayer, matrix3x4_t &localToWorld, QAngle &outputAnglesWorldSpace )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		if ( pPickup->HasPreferredCarryAnglesForPlayer( pPlayer ) )
		{
			outputAnglesWorldSpace = TransformAnglesToWorldSpace( pPickup->PreferredCarryAngles(), localToWorld );
			return true;
		}
	}
	return false;
}

bool Pickup_ForcePhysGunOpen( CBaseEntity *pObject, CBasePlayer *pPlayer )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		return pPickup->ForcePhysgunOpen( pPlayer );
	}
	return false;
}

AngularImpulse Pickup_PhysGunLaunchAngularImpulse( CBaseEntity *pObject, PhysGunForce_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup != nullptr && pPickup->ShouldPuntUseLaunchForces( reason ) )
	{
		return pPickup->PhysGunLaunchAngularImpulse();
	}
	return RandomAngularImpulse( -600, 600 );
}

Vector Pickup_DefaultPhysGunLaunchVelocity( const Vector &vecForward, float flMass )
{

	// Do the simple calculation
	return ( vecForward * flMass );
}

Vector Pickup_PhysGunLaunchVelocity( CBaseEntity *pObject, const Vector &vecForward, PhysGunForce_t reason )
{
	// The object must be valid
	if ( pObject == nullptr )
	{
		contract_assert( 0 );
		return vec3_origin;
	}

	// Shouldn't ever get here with a non-vphysics object.
	IPhysicsObject *pPhysicsObject = pObject->VPhysicsGetObject();
	if ( pPhysicsObject == nullptr )
	{
		contract_assert( 0 );
		return vec3_origin;
	}

	// Call the pickup entity's callback
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup != nullptr && pPickup->ShouldPuntUseLaunchForces( reason ) )
		return pPickup->PhysGunLaunchVelocity( vecForward, pPhysicsObject->GetMass() );

	// Do our default behavior
	return Pickup_DefaultPhysGunLaunchVelocity(	vecForward, pPhysicsObject->GetMass() );
}

bool Pickup_ShouldPuntUseLaunchForces( CBaseEntity *pObject, PhysGunForce_t reason )
{
	IPlayerPickupVPhysics *pPickup = dynamic_cast<IPlayerPickupVPhysics *>(pObject);
	if ( pPickup )
	{
		return pPickup->ShouldPuntUseLaunchForces( reason );
	}
	return false;
}

class CNavArea;
class HidingSpot;

using namespace ::ks::reflect;

const char* g_pszPostActivateSetupThink = "PostActivateSetupThink";


IMPLEMENT_REFLECT_DATAMAP( CPointHidingSpot )

LINK_ENTITY_TO_CLASS( point_hiding_spot, CPointHidingSpot );

CPointHidingSpot::CPointHidingSpot() : 
m_pNavArea( nullptr ),
m_pSpot( nullptr )
{
}

void CPointHidingSpot::Activate_Impl()
{
	BaseClass::Activate_Impl();
		
	// Do setup after all activates have run and we're in game simulation. Nav gets loaded/setup during server
	// activate so we can't guarantee this ent will be before or after nav load.
	SetContextThink( &CPointHidingSpot::PostActivateSetupThink, gpGlobals->curtime + 0.1f, g_pszPostActivateSetupThink );
}

void CPointHidingSpot::DetachFromHidingSpot()
{
	m_pSpot = nullptr;
}

void CPointHidingSpot::UpdateOnRemove_Impl()
{
	if ( m_pNavArea && m_pSpot )
	{
		m_pNavArea->RemoveHidingSpot( m_pSpot );
		bool bSuccess = DestroyHidingSpot( m_pSpot );
		contract_assert( bSuccess );
		(void)bSuccess;
	}
	BaseClass::UpdateOnRemove_Impl();
}

void CPointHidingSpot::PostActivateSetupThink()
{
	if ( !m_pNavArea )
	{
		m_pNavArea = TheNavMesh->GetNearestNavArea( this, 0, MAX_COORD_FLOAT );
		if ( !m_pNavArea )
		{
			Warning( "Warning: point_hiding_spot (%s) couldn't find a nearby nav. Removing.", GetDebugName() );
			UTIL_Remove( this );
			return;
		}
		Vector vecPointOnGround;
		m_pNavArea->GetClosestPointOnArea( GetAbsOrigin(), &vecPointOnGround );
		m_pSpot = TheNavMesh->CreateHidingSpot();
		m_pSpot->SetSaved( false );
		m_pSpot->SetPosition( vecPointOnGround );
		m_pSpot->SetFlags( HidingSpot::IN_COVER | HidingSpot::GOOD_SNIPER_SPOT | HidingSpot::IDEAL_SNIPER_SPOT );
		m_pSpot->SetArea( m_pNavArea );
		m_pNavArea->AddHidingSpot( m_pSpot );

		SetContextThink( nullptr, TICK_NEVER_THINK, g_pszPostActivateSetupThink );
	}
}
