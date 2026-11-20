// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "igameevents.h"
#include "networkvar.h"
#include "reflect.h"
#include "server_class.h"
#include "../predictable_entity.h"
#include "../reflect_datamap.h"
#include "../reflect_sendtable.h"
#include "edict.h"
#include "tier2/resourceprecacher.h"
#include "usermessages_schema.h"
#include "../baseentity.h"
#include "gametypes.h"
#include "netmessages.h"
#include "vphysics_interface.h"
#include "../shareddefs.h"
#include "engine/IEngineTrace.h"
module ks.server;

import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.Effects.chicken;
import :cstrike15.cs_player;
import :cstrike15.bot.bot;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import std;
import ks.core.vprof;
import ks.tier1.strtools;
import :entity_kind;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.core.dbg;
import ks.core.types;
import ks.tier1.cvar;
import ks.tier1.fmtstr;
import ks.tier1.icvar;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import ks.game.rumble_shared;
import ks.game.in_buttons;
import ks.game.iplayeranimstate;
import ks.math.vector2d;

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CEffectData;
class CHostage;
class CTakeDamageInfo;

using namespace ::ks::reflect;

extern ConVar sv_gravity;

	IMPLEMENT_REFLECT_DATAMAP( CBaseCSGrenadeProjectile )

	#define GRENADE_FAILSAFE_MAX_BOUNCES 20

IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )

IMPLEMENT_REFLECT_TABLE( CBaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile );

	void CBaseCSGrenadeProjectile::PostConstructor_Impl( const char *className )
	{
		BaseClass::PostConstructor_Impl( className );
		TheBots->AddGrenade( this );
	}

	CBaseCSGrenadeProjectile::~CBaseCSGrenadeProjectile()
	{	
		TheBots->RemoveGrenade( this );
	}

	void CBaseCSGrenadeProjectile::Precache_Impl()
	{
		BaseClass::Precache_Impl();
		PrecacheEffect( "gunshotsplash" );
	}

	void CBaseCSGrenadeProjectile::Spawn_Impl( void )
	{
		Precache();
		BaseClass::Spawn_Impl();

		SetSolidFlags( FSOLID_NOT_STANDABLE );
		SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
		SetSolid( SOLID_BBOX );	// So it will collide with physics props!
		AddFlag( FL_GRENADE );

		m_lastHitPlayer = nullptr;

		// smaller, cube bounding box so we rest on the ground
		Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
		Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );

		SetSize( min, max );
 		if ( CollisionProp( ) )
 			CollisionProp( )->SetCollisionBounds( min, max );

		m_nBounces = 0;
	}

	int	CBaseCSGrenadeProjectile::UpdateTransmitState_Impl()
	{
		// always call ShouldTransmit() for grenades
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CBaseCSGrenadeProjectile::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
	{
		CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
		if ( pRecipientEntity->IsPlayer() )
		{
			CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );

			// always transmit to the thrower of the grenade
			if ( pRecipientPlayer && ( (GetThrower() && pRecipientPlayer == GetThrower()) ||
				pRecipientPlayer->GetTeamNumber() == TEAM_SPECTATOR) )
			{
				return FL_EDICT_ALWAYS;
			}
		}

		return FL_EDICT_PVSCHECK;
	}

	void CBaseCSGrenadeProjectile::DangerSoundThink( void )
	{
		if (!IsInWorld())
		{
			Remove( );
			return;
		}

		if( gpGlobals->curtime > m_flDetonateTime )
		{
			Detonate();
			return;
		}

		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, GetAbsVelocity().Length( ), 0.2 );

		SetNextThink( gpGlobals->curtime + 0.2 );

		if (GetWaterLevel() != WL_NotInWater)
		{
			SetAbsVelocity( GetAbsVelocity() * 0.5 );
		}
	}

	//Sets the time at which the grenade will explode
	void CBaseCSGrenadeProjectile::SetDetonateTimerLength( float timer )
	{
		m_flDetonateTime = gpGlobals->curtime + timer;
	}

	unsigned int CBaseCSGrenadeProjectile::PhysicsSolidMaskForEntity_Impl( void ) const
	{
		if ( GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
		{
			return ((CONTENTS_GRENADECLIP | MASK_SOLID) & ~CONTENTS_MONSTER);
		}
		else
		{
			return (CONTENTS_GRENADECLIP|MASK_SOLID|MASK_VISIBLE_AND_NPCS|CONTENTS_HITBOX) & ~(CONTENTS_DEBRIS);
		}
	}

	void CBaseCSGrenadeProjectile::ResolveFlyCollisionCustom_Impl( trace_t &trace, Vector &vecMove )
	{
		const float kSleepVelocity = 20.0f;
		const float kSleepVelocitySquared = kSleepVelocity * kSleepVelocity; 

		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.Ent<CBaseEntity>();
		contract_assert( pEntity );
		
		if ( pEntity )
		{
			CChicken *pChicken = dynamic_cast< CChicken* >( pEntity );
			if (pChicken)
			{
				// hurt the chicken
				CTakeDamageInfo info( this, this, 10, DMG_CLUB );
				pChicken->DispatchTraceAttack( info, GetAbsVelocity().Normalized(), &trace );
				ApplyMultiDamage();

				return;
			}
		}

		// if its breakable glass and we kill it, don't bounce.
		// give some damage to the glass, and if it breaks, pass 
		// through it.
		bool breakthrough = false;

		if( pEntity && FClassnameIs( pEntity, "func_breakable" ) )
		{
			breakthrough = true;
		}

		if( pEntity && FClassnameIs( pEntity, "func_breakable_surf" ) )
		{
			breakthrough = true;
		}

		if( pEntity && FClassnameIs( pEntity, "prop_physics_multiplayer" ) && pEntity->GetMaxHealth() > 0 && pEntity->m_takedamage == DAMAGE_YES )
		{
			breakthrough = true;
		}

		// this one is tricky because BounceTouch hits breakable propers before we hit this function and the damage is already applied there (CBaseGrenade::BounceTouch( CBaseEntity *pOther ))
		// by the time we hit this, the prop hasn't been removed yet, but it broke, is set to not take anymore damage and is marked for deletion - we have to cover this case here
		if( pEntity && FClassnameIs( pEntity, "prop_dynamic" ) && pEntity->GetMaxHealth() > 0 && (pEntity->m_takedamage == DAMAGE_YES || (pEntity->m_takedamage == DAMAGE_NO && pEntity->IsEFlagSet( EFL_KILLME ))) )
		{
			breakthrough = true;
		}

		if ( breakthrough )
		{
			CTakeDamageInfo info( this, this, 10, DMG_CLUB );
			pEntity->DispatchTraceAttack( info, GetAbsVelocity().Normalized(), &trace );

			ApplyMultiDamage();

			if( pEntity->m_iHealth <= 0 )
			{
				// slow our flight a little bit
				Vector vel = GetAbsVelocity();

				vel *= 0.4;

				SetAbsVelocity( vel );
				return;
			}
		}

		//Assume all surfaces have the same elasticity
		float flSurfaceElasticity = 1.0;

		//Don't bounce off of players with perfect elasticity
		if ( pEntity && pEntity->IsPlayer() )
		{
			flSurfaceElasticity = 0.3f;

			// and do slight damage to players on the opposite team
			if ( GetTeamNumber() != pEntity->GetTeamNumber() )
			{
				CTakeDamageInfo info( this, GetThrower(), 2, DMG_GENERIC );

				pEntity->TakeDamage( info );
			}
		}

		//Don't bounce twice on a selection of problematic entities
		bool bIsProjectile = dynamic_cast< CBaseCSGrenadeProjectile* >( pEntity ) != nullptr;
		if ( pEntity && !pEntity->IsWorld() && m_lastHitPlayer.Get() == pEntity )
		{
			bool bIsHostage = dynamic_cast< CHostage* >( pEntity ) != nullptr;
			if (  pEntity->IsPlayer() || bIsHostage || bIsProjectile )
			{
				//DevMsg( "Setting %s to DEBRIS, it is in group %i, it hit %s in group %i\n", this->GetClassname(), this->GetCollisionGroup(), pEntity->GetClassname(), pEntity->GetCollisionGroup() );
				SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				if ( bIsProjectile )
				{
					//DevMsg( "Setting %s to DEBRIS, it is in group %i.\n", pEntity->GetClassname(), pEntity->GetCollisionGroup() );
					pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				}
				return;
			}
		}
		if ( pEntity )
		{
			m_lastHitPlayer = pEntity;
		}

		float flTotalElasticity = GetElasticity() * flSurfaceElasticity;
		flTotalElasticity = clamp( flTotalElasticity, 0.0f, 0.9f );

		// NOTE: A backoff of 2.0f is a reflection
		Vector vecAbsVelocity;
		PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, 2.0f );
		vecAbsVelocity *= flTotalElasticity;

		// Get the total velocity (player + conveyors, etc.)
		VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecMove );
		float flSpeedSqr = DotProduct( vecMove, vecMove );

		bool bIsWeapon = dynamic_cast< CBaseCombatWeapon* >( pEntity ) != nullptr;
		
		// Stop if on ground or if we bounce and our velocity is really low (keeps it from bouncing infinitely)
		if ( pEntity &&
			( ( trace.plane.normal.z > 0.7f ) || (trace.plane.normal.z > 0.1f && flSpeedSqr < kSleepVelocitySquared) ) &&
			( pEntity->IsStandable() || bIsProjectile || bIsWeapon || pEntity->IsWorld() ) 
			)
		{
			// clip it again to emulate old behavior and keep it from bouncing up like crazy when you throw it at the ground on the first toss
			if ( flSpeedSqr > 96000 )
			{
				float alongDist = DotProduct( vecAbsVelocity.Normalized(), trace.plane.normal );
				if ( alongDist > 0.5f )
				{
					float flBouncePadding = (1.0f - alongDist) + 0.5f;
					vecAbsVelocity *= flBouncePadding;
				}
			}

			SetAbsVelocity( vecAbsVelocity );

			if ( flSpeedSqr < kSleepVelocitySquared )
			{
				SetGroundEntity( pEntity );

				// Reset velocities.
				SetAbsVelocity( vec3_origin );
				SetLocalAngularVelocity( vec3_angle );

				//align to the ground so we're not standing on end
				QAngle angle;
				VectorAngles( trace.plane.normal, angle );

				// rotate randomly in yaw
				angle[1] = random_valve->RandomFloat( 0, 360 );

				// TODO: rotate around trace.plane.normal
				
				SetAbsAngles( angle );			
			}
			else
			{
				Vector vecBaseDir = GetBaseVelocity();
				if ( !vecBaseDir.IsZero() )
				{
					VectorNormalize( vecBaseDir );
					Vector vecDelta = GetBaseVelocity() - vecAbsVelocity;	
					float flScale = vecDelta.Dot( vecBaseDir );
					vecAbsVelocity += GetBaseVelocity() * flScale;
				}
					
				VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecMove ); 
				
				PhysicsPushEntity( vecMove, &trace );
			}
		}
		else
		{
			SetAbsVelocity( vecAbsVelocity );
			VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecMove ); 
			PhysicsPushEntity( vecMove, &trace );
		}
		
		BounceSound();

		// tell the bots a grenade has bounced
		CCSPlayer *player = ToCSPlayer(GetThrower());
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "grenade_bounce" );
			if ( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				event->SetFloat( "x", GetAbsOrigin().x );
				event->SetFloat( "y", GetAbsOrigin().y );
				event->SetFloat( "z", GetAbsOrigin().z );
				gameeventmanager->FireEvent( event );
			}
		}

		OnBounced();

		if (m_nBounces > GRENADE_FAILSAFE_MAX_BOUNCES )
		{
			//failsafe detonate after 20 bounces
			SetAbsVelocity( vec3_origin );
			DetonateOnNextThink();
			SetNextThink( gpGlobals->curtime );
			SetMoveType( MOVETYPE_NONE );
		}
		else
		{
			m_nBounces++;
		}
		
	}

	void CBaseCSGrenadeProjectile::SetupInitialTransmittedGrenadeVelocity( const Vector &velocity )
	{
		m_vInitialVelocity = velocity;
	}

	constexpr auto MAX_WATER_SURFACE_DISTANCE = 512;

	void CBaseCSGrenadeProjectile::Splash_Impl()
	{
		Vector centerPoint = GetAbsOrigin();
		Vector normal( 0, 0, 1 );

		// Find our water surface by tracing up till we're out of the water
		trace_t tr;
		Vector vecTrace( 0, 0, MAX_WATER_SURFACE_DISTANCE );
		UTIL_TraceLine( centerPoint, centerPoint + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );

		// If we didn't start in water, we're above it
		if ( tr.startsolid == false )
		{
			// Look downward to find the surface
			vecTrace.Init( 0, 0, -MAX_WATER_SURFACE_DISTANCE );
			UTIL_TraceLine( centerPoint, centerPoint + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );

			// If we hit it, setup the explosion
			if ( tr.fraction < 1.0f )
			{
				centerPoint = tr.endpos;
			}
			else
			{
				//NOTENOTE: We somehow got into a splash without being near water?
				contract_assert( 0 );
			}
		}
		else if ( tr.fractionleftsolid )
		{
			// Otherwise we came out of the water at this point
			centerPoint = centerPoint + (vecTrace * tr.fractionleftsolid);
		}
		else
		{
			// Use default values, we're really deep
		}

		CEffectData	data;
 		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		data.m_flScale = random_valve->RandomFloat( 1.0f, 2.0f );

		if ( GetWaterType() & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "gunshotsplash", data );
	}

#undef GRENADE_FAILSAFE_MAX_BOUNCES

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
void CCSAmmoDef::AddAmmoCost( char const* name, int cost, int buySize )
{
	int index = Index( name );
	if ( index < 1 || index >= m_nAmmoIndex )
		return;

	m_csAmmo[index].buySize = buySize;
	m_csAmmo[index].cost = cost;
}

//-----------------------------------------------------------------------------
int CCSAmmoDef::GetBuySize( int index ) const
{
	if ( index < 1 || index >= m_nAmmoIndex )
		return 0;

	return m_csAmmo[index].buySize;
}

//-----------------------------------------------------------------------------
int CCSAmmoDef::GetCost( int index ) const
{
	if ( index < 1 || index >= m_nAmmoIndex )
		return 0;

	return m_csAmmo[index].cost;
}

//-----------------------------------------------------------------------------
CCSAmmoDef::CCSAmmoDef(void)
{
	std::memset( m_csAmmo, 0, sizeof( m_csAmmo ) );
}

//-----------------------------------------------------------------------------
CCSAmmoDef::~CCSAmmoDef( void )
{
}

//-----------------------------------------------------------------------------

using namespace ::ks::reflect;

// Purpose: Data for Autobuy and Rebuy

// Weapon class information for each weapon including the class name and the buy command alias.
AutoBuyInfoStruct g_autoBuyInfo[] = 
{
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "galil", "weapon_galil" }, // galil
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "galilar", "weapon_galilar" }, // galilar
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "ak47", "weapon_ak47" },  // ak47
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_INVALID, "scout", "weapon_scout" }, // scout
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_INVALID, "sg552", "weapon_sg552" }, // sg552
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE4, "awp", "weapon_awp" }, // awp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE5, "g3sg1", "weapon_g3sg1" }, // g3sg1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "famas", "weapon_famas" }, // famas
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "m4a1", "weapon_m4a1" }, // m4a1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "m4a1_silencer", "weapon_m4a1_silencer" }, // m4a1_silencer
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE3, "aug", "weapon_aug" }, // aug
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_INVALID, "scar17", "weapon_scar17" }, // scar17
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE3, "sg556", "weapon_sg556" }, // sg556
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_INVALID, "sg550", "weapon_sg550" }, // sg550
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE5, "scar20", "weapon_scar20" }, // scar20
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE2, "ssg08", "weapon_ssg08" }, // ssg08
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "glock", "weapon_glock" }, // glock
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "usp", "weapon_usp" }, // usp
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "p228", "weapon_p228" }, // p228
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY4, "deagle", "weapon_deagle" }, // deagle
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY1, "elite", "weapon_elite" },	// elites
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY3, "fn57", "weapon_fiveseven" },	// fn57
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "hkp2000", "weapon_hkp2000" },	// hkp2000
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "usp_silencer", "weapon_usp_silencer" },	// usp_silencer
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY2, "p250", "weapon_p250" },	// p250
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY3, "tec9", "weapon_tec9" },	// p250
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "taser", "weapon_taser" },	// taser
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY0, "nova", "weapon_nova" }, // nova
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY1, "xm1014", "weapon_xm1014" },	// xm1014
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY2, "mag7", "weapon_mag7" },	// mag7
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_INVALID, "stryker", "weapon_stryker" },	// stryker
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG0, "mac10", "weapon_mac10" },	// mac10
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_INVALID, "tmp", "weapon_tmp" },	// tmp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_INVALID, "mp5navy", "weapon_mp5navy" },	// mp5navy
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG2, "ump45", "weapon_ump45" },	// ump45
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG3, "p90", "weapon_p90" },	// p90
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG4, "bizon", "weapon_bizon" },	// bizon
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG1, "mp7", "weapon_mp7" },	// mp7
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG0, "mp9", "weapon_mp9" },	// mp9
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_MACHINEGUN),		LOADOUT_POSITION_HEAVY3, "m249", "weapon_m249" },	// m249
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_MACHINEGUN),		LOADOUT_POSITION_HEAVY4, "negev", "weapon_negev" },	// negev
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_AMMO),				LOADOUT_POSITION_INVALID, "primammo", "primammo" },	// primammo
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_AMMO),			LOADOUT_POSITION_INVALID, "secammo", "secammo" }, // secmmo
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR),									LOADOUT_POSITION_INVALID, "vest", "item_kevlar" }, // vest
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR),									LOADOUT_POSITION_INVALID, "vesthelm", "item_assaultsuit" }, // vesthelm
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE2, "flashbang", "weapon_flashbang" }, // flash
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE3, "hegrenade", "weapon_hegrenade" }, // hegren
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE4, "smokegrenade", "weapon_smokegrenade" }, // sgren
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE0, "molotov", "weapon_molotov" }, // molotov
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE0, "incgrenade", "weapon_incgrenade" }, // incgrenade
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE1, "decoy", "weapon_decoy" }, // molotov
	{ (AutoBuyClassType)(AUTOBUYCLASS_NIGHTVISION),								LOADOUT_POSITION_INVALID, "nvgs", "nvgs" }, // nvgs
	{ (AutoBuyClassType)(AUTOBUYCLASS_DEFUSER),									LOADOUT_POSITION_INVALID, "defuser", "defuser" }, // defuser
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHIELD),			LOADOUT_POSITION_INVALID, "shield", "shield" }, // shield

	{ (AutoBuyClassType)0, LOADOUT_POSITION_INVALID, "", "" } // last one, must be at end.
};

void RebuyStruct::SetPrimary( int nPrimaryPos)
{
	m_nPrimaryPos = nPrimaryPos;
	m_isNotEmpty = true;
}

void RebuyStruct::SetSecondary( int nSecondaryPos)
{
	m_nSecondaryPos = nSecondaryPos;
	m_isNotEmpty = true;
}

void RebuyStruct::SetTertiary( CSWeaponID tertiary)
{
	m_tertiaryId = tertiary;
	m_isNotEmpty = true;
}

void RebuyStruct::SetGrenade( int index, CSWeaponID grenade)
{
	m_grenades[ index ] = grenade;
	m_isNotEmpty = true;
}

void RebuyStruct::SetArmor( int armor)
{
	m_armor = armor;
	m_isNotEmpty = true;
}

void RebuyStruct::SetDefuser( bool defuser)
{
	m_defuser = defuser;
	m_isNotEmpty = true;
}

void RebuyStruct::SetNightVision( bool nv)
{
	m_nightVision = nv;
	m_isNotEmpty = true;
}

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

extern bool			g_fGameOver;

extern ConVar mp_maxrounds;

void FinishClientPutInServer( CCSPlayer *pPlayer )
{
	pPlayer->InitialSpawn();
	pPlayer->Spawn();

	if (!pPlayer->IsBot())
	{
		// When the player first joins the server, they
		pPlayer->m_iNumSpawns = 0;
		pPlayer->m_takedamage = DAMAGE_NO;
		pPlayer->pl.deadflag = true;
		pPlayer->m_lifeState = LIFE_DEAD;
		pPlayer->AddEffects( EF_NODRAW );
		pPlayer->ChangeTeam( TEAM_UNASSIGNED );
		// TICK_NEVER_TICK We don't want to force a Team select until after MOTD closes
		pPlayer->SetContextThink( &CBasePlayer::PlayerForceTeamThink, TICK_NEVER_THINK, CS_FORCE_TEAM_THINK_CONTEXT );
		pPlayer->InitializeAccount();

		// Move them to the first intro camera.
		pPlayer->MoveToNextIntroCamera();
		pPlayer->SetMoveType( MOVETYPE_NONE );
	}

	char sName[128];
	V_strncpy( sName, pPlayer->GetPlayerName(), sizeof( sName ) );
	
	// First parse the name and remove any %'s
	for ( char *pApersand = sName; pApersand != nullptr && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '%' )
				*pApersand = ' ';
	}

	if ( !pPlayer->IsBot() )
	{
		// notify other clients of player joining the game
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", sName[ 0 ] != 0 ? sName : "<unconnected>" );
	}
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CCSPlayer *pPlayer = CCSPlayer::CreatePlayer( "player", pEdict );

	pPlayer->SetPlayerName( playername );
}

void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	contract_assert( !bLoadGame );

	CCSPlayer *pPlayer = ToCSPlayer( CBaseEntity::Instance( pEdict ) );
	FinishClientPutInServer( pPlayer );

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	// send the 4 end of match conditions.  long frag limit, long max rounds, long rounds needed won, and long time
	ks::net::CCSUsrMsg_MatchEndConditions msg;
	msg.fraglimit = fraglimit.GetInt();
	msg.mp_maxrounds = mp_maxrounds.GetInt();
	msg.mp_winlimit = mp_winlimit.GetInt();
	msg.mp_timelimit = mp_timelimit.GetInt();
	SendUserMessage( user, ks::net::CS_UM_MatchEndConditions, msg );

}

/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Counter-Strike: Global Offensive";
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, ClientGamePrecache )
	// Materials used by the client effects
	PRECACHE( MODEL, "sprites/white.vmt" );
	PRECACHE( MODEL, "sprites/physbeam.vmt" );

	// Legacy temp ents sounds
	PRECACHE( GAMESOUND, "Bounce.PistolShell" );
	PRECACHE( GAMESOUND, "Bounce.RifleShell" );
	PRECACHE( GAMESOUND, "Bounce.ShotgunShell" );
PRECACHE_REGISTER_END()

void ClientGamePrecache( void )
{
	// Flashbang-related files
	engine->ForceExactFile( "sprites/white.vmt" );
	engine->ForceExactFile( "sprites/white.vtf" );
	engine->ForceExactFile( "vgui/white.vmt" );
	engine->ForceExactFile( "vgui/white.vtf" );
	engine->ForceExactFile( "effects/flashbang.vmt" );
	engine->ForceExactFile( "effects/flashbang_white.vmt" );

	// Smoke grenade-related files
	engine->ForceExactFile( "particle/particle_smokegrenade1.vmt" );
	engine->ForceExactFile( "particle/particle_smokegrenade.vtf" );

	// Sniper scope
	engine->ForceExactFile( "sprites/scope_arc.vmt" );
	engine->ForceExactFile( "sprites/scope_arc.vtf" );

	// DSP presets - don't want people avoiding the deafening + ear ring
	engine->ForceExactFile( "scripts/dsp_presets.txt" );
}

// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			entity_cast<CBasePlayer>( pEdict )->CreateCorpse();
		}

		// respawn player
		pEdict->Spawn();
	}
	else
	{       // restart the entire server
		engine->ServerCommand("reload\n");
	}
}

void GameStartFrame( void )
{
	CVProfScope _( "GameStartFrame", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.GetBool();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject( "CCSGameRules" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void ClientFullyConnect( edict_t *pEntity )
{

}

class CBaseEntity;
class CBaseGrenade;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: A game system for tracking and updating entity spot state

static constexpr auto ENTITY_SPOT_FREQUENCY = 0.5f;

static CCSEntitySpotting s_EntitySpotting("CCSEntitySpotting");
CCSEntitySpotting * g_EntitySpotting = &s_EntitySpotting;

ConVar radarvis( side_registry(), "radarvismethod", "1", FCVAR_CHEAT, "0 for traditional method, 1 for more realistic method", true, 0, true, 1 );
ConVar radarpow( side_registry(), "radarvispow", ".4", FCVAR_CHEAT, "the degree to which you can point away from a target, and still see them on radar." );
ConVar radardist( side_registry(), "radarvisdistance", "1000.0f", FCVAR_CHEAT, "at this distance and beyond you need to be point right at someone to see them", true, 10, false, 0 );
ConVar radarmaxdot( side_registry(), "radarvismaxdot", ".996", FCVAR_CHEAT, "how closely you have to point at someone to see them beyond max distance", true, 0, true, 1.0f );

//--------------------------------------------------------------------------------------------------------
// Functors
//--------------------------------------------------------------------------------------------------------

template < typename SpotFunctor >
bool ForEachEntitySpotter( SpotFunctor &func )
{
	CVProfScope _( "ForEachEntitySpotter", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast< CBasePlayer * >( UTIL_PlayerByIndex( i ) );

		if ( player == nullptr )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( func( player ) == false )
			return false;
	}

	//ActiveGrenadeList activeGrenadeList = TheBots->m_activeGrenadeList;
	for ( const auto &pGrenade : TheCSBots()->m_activeGrenadeList )
	{
		ActiveGrenade *ag = pGrenade.get();
		if ( ag->IsSensor() == false )
			continue;

		CBaseGrenade *grenade = ag->GetEntity();
		if ( grenade == nullptr )
			continue;
		
		if ( FNullEnt( grenade->edict() ) )
			continue;

		if ( func( grenade ) == false )
			return false;
	}

	/*
	CBaseEntity * pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( pEntity == NULL || FNullEnt( pEntity->edict() ) )
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		if ( FNullEnt( pEntity->edict() ) )
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		if ( pEntity->IsPlayer() )
		{
			CBasePlayer *player = static_cast< CBasePlayer * >( pEntity );

			if ( !player->IsConnected() )
			{
				pEntity = gEntList.NextEnt( pEntity );
				continue;
			}
		}

		if ( func( pEntity ) == false )
			return false;

		// 		else if ( dynamic_cast<CSensorGrenadeProjectile*>( pEntity ) )
		// 		{
		// 			if ( pThrower->IsOtherEnemy( pPlayer ) )
		// 			{
		// 				Vector vDelta = pPlayer->EyePosition() - GetAbsOrigin();
		// 				float flDistance = vDelta.Length();
		// 
		// 				float flMaxDrawDist = 1024;
		// 				if ( flDistance <= flMaxDrawDist )
		// 				{
		// 					trace_t tr;
		// 					//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
		// 					{
		// 						if ( TheCSBots()->IsLineBlockedBySmoke( pPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) )
		// 						{
		// 							continue;
		// 						}
		// 
		// 						UTIL_TraceLine( pPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr );
		// 						if ( tr.fraction != 1 )
		// 						{
		// 							trace_t tr2;
		// 							UTIL_TraceLine( pPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
		// 							if ( tr2.fraction != 1 )
		// 							{
		// 								continue;
		// 							}
		// 						}
		// 
		// 						pPlayer->SetIsSpotted( true );
		// 						pPlayer->SetIsSpottedBy( nThrowerIndex );
		// 					}
		// 				}
		// 			}
		// 		}

		pEntity = gEntList.NextEnt( pEntity );
	}*/

	return true;
}
//==================================================
// - CanPlayerSeeTargetEntityFunctor -
//
// Determine if a player has spotted a target entity.
// Query IsSpotted() for result
//==================================================

class CanPlayerSeeTargetEntityFunctor
{
public:
	CanPlayerSeeTargetEntityFunctor( CBaseEntity *entity, int spottingTeam )
	{
		m_targetEntity = entity;
		m_target = entity->EyePosition();
		m_team = spottingTeam;
		m_spotted = false;
	}

	bool operator()( CBaseEntity *spotter )
	{
		CCSPlayer *csPlayer = ToCSPlayer( spotter );
		if ( (csPlayer && csPlayer->IsAlive() == false ) || spotter->GetTeamNumber() != m_team )
			return true;

		CCSPlayer *csPlayerSpotter = nullptr;

		bool doTrace = false;
		Vector eye, forward;
		if ( csPlayer )
		{
			csPlayerSpotter = csPlayer;

			if ( csPlayer->IsBlind() )
				return true;
	
			csPlayer->EyePositionAndVectors( &eye, &forward, nullptr, nullptr );
			Vector path( m_target - eye );
			float distance = path.Length();
			path.NormalizeInPlace();
			float dot = DotProduct( forward, path );

			if ( dot < 0 )
				return true;

			int rvm = radarvis.GetInt();

			switch ( rvm )
			{
				case 0:// original method
					doTrace = ( ( dot > 0.995f )
								|| ( dot > 0.98f && distance < 900 )
								|| ( dot > 0.8f && distance < 250 )
								);
					break;

				case 1: // new method method
				{
							int fov = csPlayer->GetFOVForNetworking() / 2;
							float cosfov = std::cosf( ( float )fov*3.1415f / 180.0f );

							float d = distance / radardist.GetFloat();
							d = clamp( std::powf( d, radarpow.GetFloat() ), cosfov, radarmaxdot.GetFloat() );

							doTrace = ( dot > d );
				}
					break;

			}
		}
		else if ( dynamic_cast<CSensorGrenadeProjectile*>( spotter ) )
		{
			//CCSPlayer *csTargetPlayer = ToCSPlayer( m_targetEntity );
			//if ( spotter->IsOtherEnemy( m_targetEntity ) )
			{
				eye = spotter->GetAbsOrigin();
				csPlayerSpotter = ToCSPlayer( dynamic_cast<CSensorGrenadeProjectile*>( spotter )->GetThrower() );
				doTrace = true;

				/*
				Vector vDelta = csTargetPlayer->EyePosition() - spotter->GetAbsOrigin();
				float flDistance = vDelta.Length();

				float flMaxDrawDist = 1024;
				if ( flDistance <= flMaxDrawDist )
				{
					trace_t tr;
					//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
					{
						if ( TheCSBots()->IsLineBlockedBySmoke( csTargetPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) == false )
						{
							UTIL_TraceLine( csTargetPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, csTargetPlayer, COLLISION_GROUP_DEBRIS, &tr );
							trace_t tr2;
							UTIL_TraceLine( csTargetPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, csTargetPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
							if ( tr1.fraction == 1 || tr2.fraction == 1 )
								{
									continue;
								}
							}
						}

						pPlayer->SetIsSpotted( true );
						pPlayer->SetIsSpottedBy( nThrowerIndex );
					}
				}
				*/
			}
		}

		if (doTrace && csPlayerSpotter)
		{
			trace_t tr;
			CTraceFilterSkipTwoEntities filter( spotter, m_targetEntity, COLLISION_GROUP_DEBRIS );
			UTIL_TraceLine( eye, m_target,
				(CONTENTS_OPAQUE|CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_DEBRIS|MASK_OPAQUE_AND_NPCS), &filter, &tr );

			if ( tr.fraction == 1.0f && TheCSBots()->IsLineBlockedBySmoke( eye, m_target, 1.0f ) == false )
			{
				if ( !csPlayer || (csPlayer && csPlayer->GetFogObscuredRatio( m_targetEntity ) < 0.9) )
				{
					m_spotted = true;
					m_spottedBy.push_back( csPlayerSpotter->entindex() );
					//return false; // spotted already, so no reason to check for other players spotting the same thing.
				}
			}
		}

		return true;
	}

	bool IsSpotted( void ) const
	{
		return m_spotted;
	}

	int GetIsSpottedBy( int nUtlIndex ) const
	{
		if ( nUtlIndex < std::ssize( m_spottedBy ) )
			return m_spottedBy[nUtlIndex];

		return 0;
	}

	int GetSpottedByCount( void ) const
	{
		return std::ssize( m_spottedBy );
	}

private:
	CBaseEntity *m_targetEntity;
	Vector m_target;
	int m_team;
	bool m_spotted;
	std::vector< int > m_spottedBy;
};

#define BIT_SET( a, b ) ((a)[(b)>>3] & (1<<((b)&7)))

extern bool WasPlayerOccluded( int fromplayer, int toplayer );

//===========================================================
// - GatherNonPVSSpottedEntitiesFunctor -
//
// Given a player, generate a list of spotted entities that
// exist outside of that player's PVS.
// Query GetSpotted for result
//===========================================================

GatherNonPVSSpottedEntitiesFunctor::GatherNonPVSSpottedEntitiesFunctor( CCSPlayer * pPlayer ) : m_pPlayer( pPlayer )
{
	if ( pPlayer )
	{
		m_nSourceTeam = pPlayer->GetAssociatedTeamNumber();

		engine->GetPVSForCluster( engine->GetClusterForOrigin( pPlayer->EyePosition() ), sizeof( m_pSourcePVS ), m_pSourcePVS );

		// spectators and OBS_ALLOW_ALL observers receive updates on all spottable entities 
		if ( m_nSourceTeam == TEAM_SPECTATOR )
		{
			m_bForceSpot = true;
		}
		else if ( pPlayer->GetObserverMode() != OBS_MODE_NONE )
		{
			ConVarRef mp_forcecamera( "mp_forcecamera" );
			m_bForceSpot = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;
		}
		else
		{
			m_bForceSpot = false;
		}
	}
}

bool GatherNonPVSSpottedEntitiesFunctor::operator()( CBaseEntity * pEntity )
{
	if ( !pEntity->edict() || !pEntity->CanBeSpotted() )
	{
		return true;
	}

	CBaseEntity *pParent = pEntity->GetRootMoveParent();

	int iBitNumber = engine->GetClusterForOrigin( pParent->EyePosition() );

	// We only care about entities who are not within this player's PVS
	// We include being occluded as being outside of PVS.
	if ( !BIT_SET( m_pSourcePVS, iBitNumber ) || ( m_pPlayer && pParent->entindex() <= MAX_PLAYERS && WasPlayerOccluded( pParent->entindex(), m_pPlayer->entindex() ) ) )
	{
		// target outside of PVS
		int nSpotRules = pEntity->GetSpotRules();
		bool bForceSpotted = false;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_FRIEND ) &&
			 ( pEntity->GetTeamNumber() == m_nSourceTeam ) )
			bForceSpotted = true;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_CT ) &&
			( TEAM_CT == m_nSourceTeam ) )
			bForceSpotted = true;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T ) &&
			( TEAM_TERRORIST == m_nSourceTeam  ) )
			bForceSpotted = true;

		CBasePlayer * pPlayer = UTIL_PlayerByIndex( pEntity->entindex() );
		if ( pPlayer )
		{
			// do not include dead players, observers, etc
			if ( !pPlayer->IsAlive() || pPlayer->IsObserver() || !pPlayer->IsConnected() )
			{
				return true;
			}
		}

		m_EntitySpotted.set( pEntity->entindex(), ( m_bForceSpot || pEntity->IsSpotted() || bForceSpotted ) );
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------
// Entity Spotting Game System
//--------------------------------------------------------------------------------------------------------

CCSEntitySpotting::CCSEntitySpotting( const char * szName ) : CAutoGameSystemPerFrame( szName ),
	m_fLastUpdate( 0.0f )
{

}

void CCSEntitySpotting::FrameUpdatePostEntityThink( void )
{
	if ( gpGlobals->curtime > ( m_fLastUpdate + ENTITY_SPOT_FREQUENCY ) )
	{
		UpdateSpottedEntities();
	}
}

void CCSEntitySpotting::UpdateSpottedEntities( void )
{
	m_fLastUpdate = gpGlobals->curtime;

	CBaseEntity * pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( pEntity->CanBeSpotted() )
		{
			int nTeamID = 0;

			if ( pEntity->GetSpotRules() & SPOT_RULE_ENEMY )
			{
				nTeamID = (pEntity->GetTeamNumber( ) == TEAM_CT) ? TEAM_TERRORIST : TEAM_CT;
			}
			else if ( pEntity->GetSpotRules() & SPOT_RULE_CT )
			{
				nTeamID = TEAM_CT;
			}
			else if ( pEntity->GetSpotRules() & SPOT_RULE_T )
			{
				nTeamID = TEAM_TERRORIST;
			}

			CanPlayerSeeTargetEntityFunctor canPlayerSeeTargetEntity( pEntity, nTeamID  );
			ForEachEntitySpotter( canPlayerSeeTargetEntity );
			
			pEntity->SetIsSpotted( canPlayerSeeTargetEntity.IsSpotted( ) );
			pEntity->ClearSpottedBy();
			if ( canPlayerSeeTargetEntity.IsSpotted() )
			{
				for ( int i = 0; i < canPlayerSeeTargetEntity.GetSpottedByCount(); i++ )
				{
					pEntity->SetIsSpottedBy( canPlayerSeeTargetEntity.GetIsSpottedBy( i ) );
				}
			}
		}

		pEntity = gEntList.NextEnt( pEntity );
	}
}

bool CCSEntitySpotting::Init( void )
{
	return true;
}

#undef BIT_SET

class CBaseEntity;
class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;

static constexpr auto LOG_DETAIL_ENEMY_ATTACKS = 0x01;
static constexpr auto LOG_DETAIL_TEAMMATE_ATTACKS = 0x02;

ConVar mp_logdetail( side_registry(), "mp_logdetail", "0", FCVAR_RELEASE, "Logs attacks.  Values are: 0=off, 1=enemy, 2=teammate, 3=both)", true, 0.0f, true, 3.0f );

class CCSEventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	bool PrintEvent( IGameEvent *event )	// override virtual function
	{
		if ( !PrintCStrikeEvent( event ) ) // allow CS to override logging
		{
			return BaseClass::PrintEvent( event );
		}
		else
		{
			return true;
		}
	}

	bool Init()
	{
		BaseClass::Init();

		// listen to CS events
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "bomb_pickup" );
		ListenForGameEvent( "bomb_begindefuse" );
		ListenForGameEvent( "bomb_dropped" );
		ListenForGameEvent( "bomb_defused" );
		ListenForGameEvent( "bomb_planted" );
		ListenForGameEvent( "bomb_beginplant" );
		ListenForGameEvent( "hostage_rescued" );
		ListenForGameEvent( "hostage_killed" );
		ListenForGameEvent( "hostage_follows" );
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "other_death" );
		ListenForGameEvent( "hegrenade_detonate" );
		ListenForGameEvent( "flashbang_detonate" );
		ListenForGameEvent( "player_blind" );
		ListenForGameEvent( "smokegrenade_detonate" );
		ListenForGameEvent( "molotov_detonate" );
		ListenForGameEvent( "decoy_detonate" );
		ListenForGameEvent( "item_purchase" );

		return true;
	}

protected:

	bool PrintCStrikeEvent( IGameEvent *event )	// print Mod specific logs
	{
		const char *eventName = event->GetName();
	
		// messages that don't have a user associated to them
		if ( StringHasPrefixCaseSensitive( eventName, "round_end" ) )
		{
			const int winner = event->GetInt( "winner" );
			const int reason = event->GetInt( "reason" );
			const char *msg = event->GetString( "message" );
			msg++; // remove the '#' char

			switch( reason )
			{
			case Game_Commencing:
				UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );
				return true;
				break;
			}

			CTeam *ct = GetGlobalTeam( TEAM_CT );
			CTeam *ter = GetGlobalTeam( TEAM_TERRORIST );
			contract_assert( ct && ter );

			switch ( winner )
			{
			case WINNER_CT:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ct->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_TER:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ter->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_DRAW:
			default:
				UTIL_LogPrintf( "World triggered \"%s\" (CT \"%i\") (T \"%i\")\n", msg, ct->GetScore(), ter->GetScore() );
				break;
			}	

			UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", ct->GetScore(), ct->GetNumPlayers() );
			UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", ter->GetScore(), ter->GetNumPlayers() );
			
			UTIL_LogPrintf("World triggered \"Round_End\"\n");
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "other_death" ) )
		{
			const int attackerid = event->GetInt( "attacker" );
			const char *weapon = event->GetString( "weapon" );
			const bool headShot = ( event->GetInt( "headshot" ) == 1 );
			const bool penetrated = ( event->GetInt( "penetrated" ) > 0 );
			const bool domination = false;
			const bool revenge = false;
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			
			const int otherid = event->GetInt( "otherid" );
			CBaseEntity *pOther = UTIL_EntityByIndex( otherid );

			if ( pAttacker && otherid && pOther )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed other \"%s<%i>\" [%.0f %.0f %.0f] with \"%s\"%s%s%s%s%s%s%s%s%s\n",
					//attacker
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					//target
					event->GetString( "othertype" ),
					event->GetInt( "otherid" ),
					pOther->GetAbsOrigin().x,
					pOther->GetAbsOrigin().y,
					pOther->GetAbsOrigin().z,
					//weapon
					weapon,
					( domination || revenge || headShot || penetrated ) ? " (" : "",
					domination ? "domination" : "",
					( revenge && ( domination ) ) ? " " : "",
					revenge ? "revenge" : "",
					( headShot && ( domination || revenge ) ) ? " " : "",
					headShot ? "headshot" : "",
					( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
					penetrated ? "penetrated" : "",
					( domination || revenge || headShot || penetrated ) ? ")" : ""
					);
			}
			else if ( pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed other \"%s\" with \"%s\"%s%s%s%s%s%s%s%s%s\n",
					//attacker
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					//target
					event->GetString( "othertype" ),
					//weapon
					weapon,
					( domination || revenge || headShot || penetrated ) ? " (" : "",
					domination ? "domination" : "",
					( revenge && ( domination ) ) ? " " : "",
					revenge ? "revenge" : "",
					( headShot && ( domination || revenge ) ) ? " " : "",
					headShot ? "headshot" : "",
					( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
					penetrated ? "penetrated" : "",
					( domination || revenge || headShot || penetrated ) ? ")" : ""
					);
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "server_" ) )
		{
			return false; // ignore server_ messages
		}
		
		const int userid = event->GetInt( "userid" );
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
		if ( !pPlayer )
		{
			return false;
		}

		if ( FStrEq( eventName, "player_hurt" ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			if ( !pAttacker )
			{
				return false;
			}

			bool isTeamAttack = ( (pPlayer->GetTeamNumber() == pAttacker->GetTeamNumber() ) && (pPlayer != pAttacker) );
			int detail = mp_logdetail.GetInt();
			if ( ( isTeamAttack && ( detail & LOG_DETAIL_TEAMMATE_ATTACKS ) ) ||
				( !isTeamAttack && ( detail & LOG_DETAIL_ENEMY_ATTACKS ) ) )
			{
				int hitgroup = event->GetInt( "hitgroup" );
				const char *hitgroupStr = "GENERIC";
				switch ( hitgroup )
				{
				case HITGROUP_GENERIC:
					hitgroupStr = "generic";
					break;
				case HITGROUP_HEAD:
					hitgroupStr = "head";
					break;
				case HITGROUP_CHEST:
					hitgroupStr = "chest";
					break;
				case HITGROUP_STOMACH:
					hitgroupStr = "stomach";
					break;
				case HITGROUP_LEFTARM:
					hitgroupStr = "left arm";
					break;
				case HITGROUP_RIGHTARM:
					hitgroupStr = "right arm";
					break;
				case HITGROUP_LEFTLEG:
					hitgroupStr = "left leg";
					break;
				case HITGROUP_RIGHTLEG:
					hitgroupStr = "right leg";
					break;
				}

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] attacked \"%s<%i><%s><%s>\" [%.0f %.0f %.0f] with \"%s\" (damage \"%d\") (damage_armor \"%d\") (health \"%d\") (armor \"%d\") (hitgroup \"%s\")\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pAttacker->GetAbsOrigin().x,
					pAttacker->GetAbsOrigin().y,
					pAttacker->GetAbsOrigin().z,
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName(),
					pPlayer->GetAbsOrigin().x,
					pPlayer->GetAbsOrigin().y,
					pPlayer->GetAbsOrigin().z,
					weapon,
					event->GetInt( "dmg_health" ),
					event->GetInt( "dmg_armor" ),
					event->GetInt( "health" ),
					event->GetInt( "armor" ),
					hitgroupStr );
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_death" ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			const bool headShot = (event->GetInt( "headshot" ) == 1);
			const bool penetrated = (event->GetInt( "penetrated" ) > 0);
			const bool domination = (event->GetInt( "dominated" ) > 0);
			const bool revenge = (event->GetInt( "revenge" ) > 0);
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );

			const int assisterid = event->GetInt( "assister" );
			CBasePlayer *pAssister = UTIL_PlayerByUserId( assisterid );

			if ( pPlayer == pAttacker )  
			{  
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] committed suicide with \"%s\"\n",  
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z,
								weapon
								);
			}
			else if ( pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] killed \"%s<%i><%s><%s>\" [%.0f %.0f %.0f] with \"%s\"%s%s%s%s%s%s%s%s%s\n",  
								//attacker
								pAttacker->GetPlayerName(),
								attackerid,
								pAttacker->GetNetworkIDString(),
								pAttacker->GetTeam()->GetName(),
								pAttacker->GetAbsOrigin().x,
								pAttacker->GetAbsOrigin().y,
								pAttacker->GetAbsOrigin().z,
								//target
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z,
								weapon,
								( domination || revenge || headShot || penetrated ) ? " (" : "",
								domination ? "domination":"",
								( revenge && ( domination ) ) ? " " : "",
								revenge ? "revenge" : "",
								( headShot && ( domination || revenge ) ) ? " " : "",
								headShot ? "headshot":"",
								( penetrated && ( domination || revenge || headShot ) ) ? " " : "",
								penetrated ? "penetrated" : "",
								( domination || revenge || headShot || penetrated ) ? ")" : ""

								);
				if ( pAssister )
				{
					UTIL_LogPrintf( "\"%s<%i><%s><%s>\" assisted killing \"%s<%i><%s><%s>\"\n", 
						//attacker
						pAssister->GetPlayerName(),
						assisterid,
						pAssister->GetNetworkIDString(),
						pAssister->GetTeam()->GetName(),
						// target
						pPlayer->GetPlayerName(),
						userid,
						pPlayer->GetNetworkIDString(),
						pPlayer->GetTeam()->GetName()
						);
				}
			}
			else
			{  
				// killed by the world
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] committed suicide with \"world\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								pPlayer->GetAbsOrigin().x,
								pPlayer->GetAbsOrigin().y,
								pPlayer->GetAbsOrigin().z
								);
			}
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hegrenade_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw hegrenade [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "flashbang_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw flashbang [%.0f %.0f %.0f] flashbang entindex %d)\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" ),
				event->GetInt( "entityid")
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_blind" ) )
		{
			const int attackerid = event->GetInt( "attacker" );
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			float flDuration = event->GetFloat( "blind_duration" );
			int entnum = event->GetInt( "entityid" );
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" blinded for %.2f by \"%s<%i><%s><%s>\" from flashbang entindex %d \n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				flDuration,
				pAttacker ? pAttacker->GetPlayerName() : "unknown",
				attackerid,
				pAttacker ? pAttacker->GetNetworkIDString() : "unknown",
				pAttacker ? pAttacker->GetTeam()->GetName() : "unknown",
				entnum
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "smokegrenade_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw smokegrenade [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "molotov_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw molotov [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefix( eventName, "decoy_detonate" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" threw decoy [%.0f %.0f %.0f]\n",
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				event->GetFloat( "x" ),
				event->GetFloat( "y" ),
				event->GetFloat( "z" )
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "round_start" ) )
		{
			UTIL_LogPrintf("World triggered \"Round_Start\"\n");
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "player_team" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s>\" switched from team <%s> to <%s>\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								GetGlobalTeam( event->GetInt("oldteam") )->GetName(),
								GetGlobalTeam( event->GetInt("team") )->GetName()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_follows" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Touched_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_killed" ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Killed_A_Hostage\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "hostage_rescued" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Rescued_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_planted" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Planted_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefix( eventName, "bomb_planted" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Bomb_Begin_Plant\"\n", 
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString()
				);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_defused" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Defused_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}

		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_dropped" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Dropped_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_begindefuse" ) )
		{
			const bool haskit = (event->GetInt( "haskit" ) == 1);
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"%s\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								haskit ? "Begin_Bomb_Defuse_With_Kit" : "Begin_Bomb_Defuse_Without_Kit"
								);
			return true;
		}
		else if ( StringHasPrefixCaseSensitive( eventName, "bomb_pickup" ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Got_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}	
		else if ( StringHasPrefixCaseSensitive( eventName, "item_purchase" ) )
		{
			const char *weapon = event->GetString( "weapon" );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" purchased \"%s\"\n",  
				pPlayer->GetPlayerName(),
				userid,
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				weapon
				);

			return true;
		}	

// unused events:
//hostage_hurt
//bomb_exploded

		return false;
	}

};

CCSEventLog g_CSEventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
CEventLog* GameLogSystem()
{
	return &g_CSEventLog;
}

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

//////////////////////////////////////////////////////////////////////////
//
// Convars
//
static ConVar tv_allow_camera_man_playerid( side_registry(), "tv_allow_camera_man_steamid", "", FCVAR_RELEASE, "Allows tournament production cameraman to run csgo.exe -interactivecaster on SteamID 7650123456XXX and be the camera man." );

// #define SVGC_RESERVATION_DEBUG 1

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = 1;  // allow single player for the test maps (but we default to multi)
	maxplayers = MAX_PLAYERS;
	
	defaultMaxPlayers = MAX_PLAYERS;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

extern IResponseSystem *g_pResponseSystem;
void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	if ( V_strcmp( STRING(gpGlobals->mapname), "cs_" ) )
	{
		// don't precache AI responses (hostages) if it's not a hostage rescure map
		g_pResponseSystem->PrecacheResponses( false );	
	}
}

// Marks the queue matchmaking game as starting
void CServerGameDLL::ReportGCQueuedMatchStart( int32 iReservationStage, uint32 *puiConfirmedAccounts, int numConfirmedAccounts )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: A user has had their network id setup and validated 
//-----------------------------------------------------------------------------
void CServerGameClients::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CPlayerId playerUID )
{
	/** Removed for partner depot **/
}

//
// Matchmaking game data buffer
//
void CServerGameDLL::GetMatchmakingGameData( char *buf, std::size_t bufSize )
{
	char * const bufBase = buf;
	int len = 0;

	// Put the game key
	V_snprintf( buf, bufSize, "g:csgo,gt:%u,gm:%u,", game_type.GetInt(), game_mode.GetInt() );
	len = std::strlen( buf );
	buf += len;
	bufSize -= len;

	// Trim the last comma if anything was written
	if ( buf > bufBase )
		buf[ -1 ] = 0;
}

// this returns true if they were already in the list or were successfully added
// returns false if they were not added (not allowed to be a caster)
bool AddAccountToActiveCasters( const CPlayerId &playerUID )
{
	// first check if they are already in the list
	bool bAlreadyAdded = false;
	for ( int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
	{
		if ( playerUID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
		{
			// this caster is already in the list so skip adding them, but allow them
			bAlreadyAdded = true;
			break;
		}
		if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
		{
			// already have an active caster, so don't allow another
			return false;
		}
	}

	if ( !bAlreadyAdded )
	{
		// not already added, so find an empty slot and put them in it
		for (int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
		{
			if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] == 0 )
			{
				CSGameRules()->m_arrTournamentActiveCasterAccounts.Set( j, playerUID.GetAccountID() );
				ConMsg( "Adding ID:%d to active caster list!\n", playerUID.GetAccountID() );
				break;
			}
		}
	}

	return true;
}

// validate if player is a caster and is not playing in the current game, then add them to the active caster list
// returns false if they are not allow to be a caster
bool CServerGameDLL::ValidateAndAddActiveCaster( const CPlayerId &playerUID )
{
	// check if they are a player in the current game. Note: players can be casters sometimes (and might be in the casters list below), but we don't want their voice data "public" when they are playing
	for ( int i = 0; i < CCSGameRules::sm_QueuedServerReservation.account_ids.size(); i++ )
	{
		if ( playerUID.GetAccountID() == CCSGameRules::sm_QueuedServerReservation.account_ids[ i ] )
		{
			// this is a player
			return false;
		}
	}
	// they weren't in the player list, so now check the caster list
	for ( int i = 0; i < CCSGameRules::sm_QueuedServerReservation.tournament_casters_account_ids.size(); i++ )
	{
		if ( playerUID.GetAccountID() == CCSGameRules::sm_QueuedServerReservation.tournament_casters_account_ids[ i ] )
		{
			// this is a caster
			return AddAccountToActiveCasters( playerUID );
		}
	}
	if ( tv_allow_camera_man_playerid.GetString()[0] && engine->IsDedicatedServer() )
	{
		CPlayerId playeridCameraMan( V_atoui64( tv_allow_camera_man_playerid.GetString() ) );
		if ( playeridCameraMan.IsValid() && playeridCameraMan.BIndividualAccount() && playeridCameraMan.GetAccountID() &&
			( playeridCameraMan.GetAccountID() == playerUID.GetAccountID() ) )
		{
			return AddAccountToActiveCasters( playerUID );
		}
	}
	return false;
}

// Returns which encryption key to use for messages to be encrypted for TV
EncryptedMessageKeyType_t CServerGameDLL::GetMessageEncryptionKey( INetMessage *pMessage )
{
	switch ( pMessage->GetType() )
	{
	case ks::net::svc_VoiceData:
		{
			// check the voice data packets for being from an active caster and add the caster flag and use the public key
			CSVCMsg_VoiceData_t *pVoiceData = ( CSVCMsg_VoiceData_t * ) pMessage;
			CPlayerId playerUID( static_cast<uint64>( pVoiceData->xuid ) );
			if ( playerUID.GetAccountID() )
			{
				for ( int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
				{
					if ( playerUID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
					{
						pVoiceData->caster = true;
						return kEncryptedMessageKeyType_Public;
					}
				}
			}
		}
		return kEncryptedMessageKeyType_Private;

	case ks::net::svc_UserMessage:
		{
			CSVCMsg_UserMessage_t *pUsrMessageHeader = ( CSVCMsg_UserMessage_t * ) pMessage;
			switch ( pUsrMessageHeader->msg_type )
			{
			case ks::net::CS_UM_SayText:
				{
					ks::net::CCSUsrMsg_SayText usrMsg;
					if ( ks::proto::read_bytes( usrMsg, &pUsrMessageHeader->msg_data->at( 0 ), pUsrMessageHeader->msg_data->size( ) ) )
					{
						if ( usrMsg.textallchat )
							return kEncryptedMessageKeyType_Public;
					}
				}
				return kEncryptedMessageKeyType_Private;
			
			case ks::net::CS_UM_SayText2:
				{
					ks::net::CCSUsrMsg_SayText2 usrMsg;
					if ( ks::proto::read_bytes( usrMsg, &pUsrMessageHeader->msg_data->at( 0 ), pUsrMessageHeader->msg_data->size( ) ) )
					{
						if ( usrMsg.textallchat )
							return kEncryptedMessageKeyType_Public;
					}
				}
				return kEncryptedMessageKeyType_Private;

			case ks::net::CS_UM_TextMsg:
			case ks::net::CS_UM_RadioText:
			case ks::net::CS_UM_RawAudio:
			case ks::net::CS_UM_SendAudio:
				return kEncryptedMessageKeyType_Private;

			default:
				return kEncryptedMessageKeyType_None;
			}
		}
		return kEncryptedMessageKeyType_None;

	case ks::net::svc_EncryptedData:
	default:
		return kEncryptedMessageKeyType_None;
	}
}

// If server game dll needs more time before server process quits then
// it should return true to hold game server reservation from this interface method.
// If this method returns false then the server process will clear the reservation
// and might shutdown to meet uptime or memory limit requirements.
bool CServerGameDLL::ShouldHoldGameServerReservation( float flTimeElapsedWithoutClients )
{
	/** Removed for partner depot **/
	return false; // let the server get unreserved
}

// Pure server validation failed for the given client, client supplied
// data is included in the payload
void CServerGameDLL::OnPureServerFileValidationFailure( edict_t *edictClient, const char *path, const char *fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID )
{
	/** Removed for partner depot **/
}

// Last chance validation on connect packet for the client, non-NULL return value
// causes the client connect to be aborted with the provided error
char const * CServerGameDLL::ClientConnectionValidatePreNetChan( bool bGameServer, char const *adr, int nAuthProtocol, uint64 ullPlayerID )
{
	/** Removed for partner depot **/
	return nullptr;	// allow connections by default
}

// Network channel notification from engine to game server code
void CServerGameDLL::OnEngineClientNetworkEvent( edict_t *edictClient, uint64 ullPlayerID, int nEventType, void *pvParam )
{
	/** Removed for partner depot **/
}

// Game server notifying GC with its sync packet
void CServerGameDLL::EngineGotvSyncPacket( const ks::net::CEngineGotvSyncPacket *pPkt )
{
	/** Removed for partner depot **/
}

// GOTV client attempt redirect over SDR
bool CServerGameDLL::OnEngineClientProxiedRedirect( uint64 ullClient, const char *adrProxiedRedirect, const char *adrRegular )
{
	/** Removed for partner depot **/
	return false;
}
	
//-----------------------------------------------------------------------------
// Purpose: Called to apply lobby settings to a dedicated server
//-----------------------------------------------------------------------------
void CServerGameDLL::ApplyGameSettings( KeyValues *pKV )
{
	if ( !pKV )
	{
		return;
	}

	if ( engine )
	{
		DevMsg( "CServerGameDLL::ApplyGameSettings game settings payload received:\n" );
		KeyValuesDumpAsDevMsg( pKV, 1 );

		const char* pMapName = nullptr;
		const char* pMapNameFromKV = pKV->GetString( "game/map" );
		const char* pGameType = pKV->GetString( "game/type" );
		const char* pGameMode = pKV->GetString( "game/mode" );
		const char* pMapGroupName = pKV->GetString( "game/mapgroupname", nullptr );
		const char* pMapGroupNameToValidate = nullptr;		// pMapGroupName is ok to be NULL; this variable lets us easily use a non null pMapGroupName or gpGlobals->mapGroupName

		if ( pMapGroupName && (pMapGroupName[0] != '\0') && !pMapNameFromKV )
		{
			// if we have a mapgroup name, then we don't care about any map name from the pKV and we just want the first map from the mapgroup
			pMapName = g_pGameTypes->GetRandomMap( pMapGroupName );
			pMapGroupNameToValidate = pMapGroupName;
		}
		else
		{
			pMapGroupNameToValidate = ( pMapGroupName && (pMapGroupName[0] != '\0') ) ? pMapGroupName : STRING( gpGlobals->mapGroupName );
		}

		// make sure we are not using a bogus mapgroup name
		if ( pMapGroupNameToValidate && !StringIsEmpty( pMapGroupNameToValidate ) && !g_pGameTypes->IsValidMapGroupName( pMapGroupNameToValidate ) )
		{
			Warning( "ApplyGameSettings: Invalid mapgroup name %s\n", pMapGroupNameToValidate );
			return;
		}

		// only use the map name from the pKV if there was no mapgroup name in the pKV
		if ( !pMapName )
		{
			pMapName = pMapNameFromKV;
		}

		// For team games we add the prefix "team" to the game type. This is to
		// eliminate team game lobbies from searches for QuickMatch and Custom Match
		const char *teamStr = "team";
		const char *pTeamPrefix = V_strstr( pGameType, teamStr);
		if ( pTeamPrefix == pGameType )
		{
			pGameType += V_strlen( teamStr );
		}

		if ( pMapName && pMapName[0] != '\0' )
		{
			// validate map exists in the mapgroup
			if ( !g_pGameTypes->IsValidMapInMapGroup( pMapGroupNameToValidate, pMapName ) )
			{
				Warning( "ApplyGameSettings: Map %s not part of Mapgroup %s\n", pMapName, pMapGroupNameToValidate );
			}

			int extraSpectators = 2;

			if ( ( pGameType && pGameType[0] != '\0' ) &&
				 ( pGameMode && pGameMode[0] != '\0' ) )
			{
				// make sure the mapgroup is in this game type & mode
				if ( !g_pGameTypes->IsValidMapGroupForTypeAndMode( pMapGroupNameToValidate, pGameType, pGameMode ) )
				{
					Warning( "ApplyGameSettings: MapGroup %s not part of type %s mode %s\n", pMapGroupNameToValidate, pGameType, pGameMode );
				}

				// Get the bot difficulty setting before it gets reverted.
				ConVarRef cvCustomBotDiff( "custom_bot_difficulty" );
				int customBotDiff = cvCustomBotDiff.GetInt();

/*
				// FIXME[pmf]: We don't want to reset all replicated convars unless we also re-exec game.cfg on the server,
				// otherwise we'll overwrite all the game configuration convars specified in game.cfg

				// Reset server enforced convars
				g_pCVar->RevertFlaggedConVars( FCVAR_REPLICATED );
*/

				// Cheats were disabled; revert all cheat cvars to their default values.
				// This must be done heading into multiplayer games because people can play
				// demos etc and set cheat cvars with sv_cheats 0.
				g_pCVar->RevertFlaggedConVars( FCVAR_CHEAT );

				// we know that the loading screen data is correct, so let the loading screen know
				//g_pGameTypes->SetLoadingScreenDataIsCorrect( true );
				g_pGameTypes->SetRunMapWithDefaultGametype( false );
				// Set game_type and game_mode convars.
				g_pGameTypes->SetGameTypeAndMode( pGameType, pGameMode );

				if ( char const *szOnline = pKV->GetString( "system/network", nullptr ) )
				{
					game_online.SetValue( ( !V_stricmp( szOnline, "LIVE" ) ) ? 1 : 0 );
				}
				
				if ( char const *szAccess = pKV->GetString( "system/access", nullptr ) )
				{
					game_public.SetValue( ( !V_stricmp( szAccess, "public" ) ) ? 1 : 0 );
				}

				if ( engine->IsDedicatedServer() )
					game_online.SetValue( 1 );
				// Special case: set the custom bot difficulty for offline games
				if ( !game_online.GetBool() )
				{
					g_pGameTypes->SetCustomBotDifficulty( customBotDiff );
				}

				// Make sure that correct number of slots is set for the engine
				{
					int iType, iMode;
					if ( g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameType, pGameMode, iType, iMode ) )
					{
						int iMaxPlayersForTypeMode = g_pGameTypes->GetMaxPlayersForTypeAndMode( iType, iMode );
						pKV->SetInt( "members/numSlots", iMaxPlayersForTypeMode );
					}
				}

				// Make sure the settings keys have extra spectator info
				pKV->SetInt( "members/numExtraSpectatorSlots", extraSpectators );
			}

			CFmtStr command;

			if ( pMapGroupName )
			{
				command.AppendFormat( "mapgroup %s\n", pMapGroupName );
			}
			command.AppendFormat( "nextlevel %s\n", pMapName ); // gamerules will clean it up when they construct for the next map
			command.AppendFormat( "map %s reserved\n", pMapName );
			
			Warning( "Executing server command:\n%s\n---\n", command.Access() );
			engine->ServerCommand( command );
			if ( engine->IsDedicatedServer() )
				engine->ServerExecute();
		}
	}
}

extern CCSPlayer *ToCSPlayer( CBaseEntity *pEntity );
const char * CServerGameClients::ClientNameHandler( uint64 xuid, const char *pchName )
{
	CPlayerId playerUID( xuid );

	// In tournament mode force names for the players according to the reservation
	if ( playerUID.IsValid() && playerUID.BIndividualAccount() &&
		CCSGameRules::sm_QueuedServerReservation.tournament_event.has_value() )
	{
		for ( int32 iTeam = 0; iTeam < CCSGameRules::sm_QueuedServerReservation.tournament_teams.size(); ++iTeam )
		{
			ks::net::TournamentTeam const &ttTeam = CCSGameRules::sm_QueuedServerReservation.tournament_teams[ iTeam ];
			for ( int32 iTeamPlayer = 0; iTeamPlayer < ttTeam.players.size(); ++iTeamPlayer )
			{
				ks::net::TournamentPlayer const &ttPlayer = ttTeam.players[ iTeamPlayer ];
				if ( ttPlayer.account_id && ( ttPlayer.account_id == playerUID.GetAccountID() ) )
				{
					return ( ttPlayer.player_nick->c_str() );
				}
			}
		}
	}

	// Throttle name changes from clients (hacked clients can set the name convar at any rate)
	if ( CCSPlayer* pPlayer = ToCSPlayer( CBasePlayer::GetPlayerByPlayerID( playerUID ) ) )
	{
		if ( !pPlayer->CanChangeName() )
			return pPlayer->GetPlayerName();
	}

	// Account not resolved, use whatever name they provided
	return pchName;
}

void CServerGameClients::ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pEntity ) );
	if ( !pPlayer )
		return;

	switch ( nType )
	{
	case ks::net::CS_UM_PlayerDecalDigitalSignature:
		{
			ks::net::CCSUsrMsg_PlayerDecalDigitalSignature msg;
			if ( ks::proto::read_bytes( msg, pvBuffer, cbSize ) )
				pPlayer->SprayPaint( msg );
		}
		return;
	}
}

class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

static constexpr auto STAMINA_RANGE = 100.0;
ConVar sv_staminajumpcost( side_registry(), "sv_staminajumpcost", ".080", FCVAR_RELEASE | FCVAR_REPLICATED, "Stamina penalty for jumping", true, 0.0, false, 0.0 );
ConVar sv_staminalandcost( side_registry(), "sv_staminalandcost", ".050", FCVAR_RELEASE | FCVAR_REPLICATED, "Stamina penalty for landing", true, 0.0, false, 0.0 );
ConVar sv_staminarecoveryrate( side_registry(), "sv_staminarecoveryrate", "60", FCVAR_RELEASE | FCVAR_REPLICATED, "Rate at which stamina recovers (units/sec)", true, 0.0, false, 0.0 );
ConVar sv_staminamax( side_registry(), "sv_staminamax", "80", FCVAR_RELEASE | FCVAR_REPLICATED, "Maximum stamina penalty", true, 0.0, true, 100.0 );

static constexpr auto get_sv_crouch_spam_penalty = 2.0f;

// Prevent super-fast duck-spam while in air
ConVar sv_timebetweenducks( side_registry(), "sv_timebetweenducks", "0.4", FCVAR_REPLICATED | FCVAR_RELEASE, "Minimum time before recognizing consecutive duck key", true, 0.0, true, 2.0 );

extern ConVar sv_accelerate_use_weapon_speed;
extern ConVar sv_accelerate_debug_speed;

static constexpr auto SV_ACCELERATE_EXPONENT_TIME = 0;
static constexpr auto SV_ACCELERATE_EXPONENT = 1;

// Not ready to ship yet... previous value was 2.0
ConVar sv_extreme_strafe_accuracy_fishtail( side_registry(), "sv_extreme_strafe_accuracy_fishtail", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of degrees of aim 'fishtail' when making an extreme strafe direction change", true, -5.0, true, 5.0 );

// sqrt(2 * 800 * 57) = sqrt(2 * gravity * height)
ConVar sv_jump_impulse( side_registry(), "sv_jump_impulse", "301.993377", FCVAR_REPLICATED | FCVAR_RELEASE, "Initial upward velocity for player jumps; sqrt(2*gravity*height).", true, 0.0f, false, 0.0f );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecSrc - 
//			flDist - 
//			flDelta - 
// Output : Vector
//-----------------------------------------------------------------------------
class CTraceFilterSkipTwoEntitiesAndCheckTeamMask : public CTraceFilterSkipTwoEntities
{
public:
	CTraceFilterSkipTwoEntitiesAndCheckTeamMask( const IHandleEntity *passentity = nullptr, const IHandleEntity *passentity2 = nullptr, int collisionGroup = COLLISION_GROUP_NONE ) :
		CTraceFilterSkipTwoEntities( passentity, passentity2, collisionGroup )
	{
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		const CBaseEntity *pSelf = EntityFromEntityHandle( GetPassEntity() );
		if ( !pSelf )
			return false;

		if ( GetCollisionGroup() == COLLISION_GROUP_PLAYER_MOVEMENT )
		{
			if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_PROJECTILE && pSelf->GetTeamNumber() == pEntity->GetTeamNumber() )
				return false;

			unsigned int myTeamMask = ( pSelf->PhysicsSolidMaskForEntity() & ( CONTENTS_TEAM1 | CONTENTS_TEAM2 ) );
			unsigned int otherTeamMask = ( pEntity->PhysicsSolidMaskForEntity() & ( CONTENTS_TEAM1 | CONTENTS_TEAM2 ) );
		
			// See if we have a team and we're on the same team.
			// If we are on the same team, then don't collide.
			if ( myTeamMask != 0x0 && myTeamMask == otherTeamMask  )
			{
				return false;
			}
		}

		return CTraceFilterSkipTwoEntities::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

// See shareddefs.h

// Movement nests its traces at most this deep.
static CTraceFilterSkipTwoEntitiesAndCheckTeamMask s_TraceFilter[8];
static int s_nTraceFilterCount = 0;

static ITraceFilter *LockTraceFilter( const CBaseHandle &hPlayer, int collisionGroup )
{
	// If this assertion triggers, you forgot to call UnlockTraceFilter
	contract_assert( s_nTraceFilterCount < (int)std::size( s_TraceFilter ) );
	if ( s_nTraceFilterCount >= (int)std::size( s_TraceFilter ) )
		return nullptr;

	CTraceFilterSkipTwoEntitiesAndCheckTeamMask *pFilter = &s_TraceFilter[s_nTraceFilterCount++];
	pFilter->SetPassEntity( EntityFromHandle( hPlayer ) );
	pFilter->SetCollisionGroup( collisionGroup );
	return pFilter;
}

static void UnlockTraceFilter( ITraceFilter *pFilter )
{
	contract_assert( s_nTraceFilterCount > 0 );
	--s_nTraceFilterCount;
	contract_assert( &s_TraceFilter[s_nTraceFilterCount] == pFilter );
}
ConVar sv_ladder_dampen ( side_registry(), "sv_ladder_dampen", "0.2", FCVAR_REPLICATED, "Amount to dampen perpendicular movement on a ladder", true, 0.0f, true, 1.0f );
ConVar sv_ladder_angle( side_registry(), "sv_ladder_angle", "-0.707", FCVAR_REPLICATED, "Cos of angle of incidence to ladder perpendicular for applying ladder_dampen", true, -1.0f, true, 1.0f );
ConVar sv_ladder_scale_speed( side_registry(), "sv_ladder_scale_speed", "0.78", FCVAR_REPLICATED | FCVAR_RELEASE, "Scale top speed on ladders", true, 0.0f, true, 1.0f );
ConVar sv_optimizedmovement( side_registry(), "sv_optimizedmovement", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

static void CountTraces( CPlayerMoveCore *player, int nTraces )
{
	if ( !player->IsBot() )
		VProfIncrementCounter<"PlayerMovementTraces">( nTraces );
}

static bool DeflectGrenade( CEntityCore *pCore )
{
	CBaseEntity *pEnt = static_cast<CBaseEntity *>( pCore );
	if ( !entity_cast<CBaseCSGrenadeProjectile>( pEnt ) )
		return false;
	pEnt->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	return true;
}

// If the game material underfoot changed, tell any player surface triggers that are watching.
static void OnGroundSurface( CBasePlayer *player, const trace_t &pm )
{
	surfacedata_t *pSurfaceProp = MoveHelper()->GetSurfaceProps()->GetSurfaceData( pm.surface.surfaceProps );
	char cCurrGameMaterial = player->GetGroundEntity() ? pSurfaceProp->game.material : 0;
	if ( player->m_chPreviousTextureType != cCurrGameMaterial )
		CEnvPlayerSurfaceTrigger::SetPlayerSurface( player, cCurrGameMaterial );
	player->m_chPreviousTextureType = cCurrGameMaterial;
}

extern ConVar view_punch_decay;

// What CGameMovement needs from this side.
static constexpr GameMovementServices s_MoveServices{
	[]() -> CGlobalVarsBase * { return gpGlobals; },
	[]() -> IEngineTrace * { return enginetrace; },
	[]() -> IMoveHelper * { return MoveHelper(); },
	[]() -> const CViewVectors * { return g_pGameRules->GetViewVectors(); },
	[]( const CGameTrace &tr ) -> CEntityCore * { return tr.Ent<CBaseEntity>(); },
	[]( const CGameTrace &tr ) { return TraceDidHitWorld( tr ); },
	[]( const CBaseHandle &h ) -> CPlayerMoveCore * { return static_cast<CBasePlayer *>( EntityFromHandle( h ) ); },
	LockTraceFilter,
	UnlockTraceFilter,
	// What movement does differently here.
	CountTraces,
	[]( CPlayerMoveCore *p ) { static_cast<CBasePlayer *>( p )->Splash(); },
	[] { return true; },
	DeflectGrenade,
	[]( CPlayerMoveCore *p, const CGameTrace &pm ) { OnGroundSurface( static_cast<CBasePlayer *>( p ), pm ); },
	[]( CPlayerMoveCore *p, float fvol ) { static_cast<CBasePlayer *>( p )->RumbleEffect( fvol > 0.85f ? RUMBLE_FALL_LONG : RUMBLE_FALL_SHORT, 0, RUMBLE_FLAGS_NONE ); },
	[]( CPlayerMoveCore * ) {},
	{ &sv_accelerate, &sv_airaccelerate, &sv_bounce, &sv_friction, &sv_gravity,
	  &sv_ladder_angle, &sv_ladder_dampen, &sv_ladder_scale_speed,
	  &sv_maxspeed, &sv_maxvelocity, &sv_noclipaccelerate, &sv_noclipspeed, &sv_optimizedmovement,
	  &sv_rollangle, &sv_rollspeed, &sv_specaccelerate, &sv_specnoclip, &sv_specspeed, &sv_stopspeed,
	  &view_punch_decay, &developer },
};

class CCSGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CCSGameMovement, CGameMovement );

	CCSGameMovement();

	CBasePlayer *BasePlayer() const { return static_cast<CBasePlayer *>( player ); }

	virtual void ProcessMovement( CPlayerMoveCore *pPlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton( void );
	virtual void PreventBunnyJumping( void );
	virtual void ReduceTimers( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual bool LadderMove( void );
	virtual void DecayAimPunchAngle( void );
	virtual void CheckParameters( void );

	// allow overridden versions to respond to jumping
	virtual void	OnJump( float fImpulse );
	virtual void	OnLand( float fVelocity );

	// Ducking
	virtual void Duck( void );
	virtual void FinishUnDuck( void );
	virtual void FinishDuck( void );
	virtual bool CanUnduck();

	virtual void Accelerate( Vector& wishdir, float wishspeed, float accel);

	virtual bool OnLadder( trace_t &trace );
	virtual float LadderDistance( void ) const
	{
		if ( BasePlayer()->GetMoveType() == MOVETYPE_LADDER )
			return 10.0f;
		return 2.0f;
	}

	virtual unsigned int LadderMask( void ) const
	{
		return MASK_PLAYERSOLID & ( ~CONTENTS_PLAYERCLIP );
	}

	virtual float ClimbSpeed( void ) const;
	virtual float LadderLateralMultiplier( void ) const;

protected:
	virtual void PlayerMove();

	virtual unsigned int PlayerSolidMask( bool brushOnly = false, CPlayerMoveCore *testPlayer = nullptr ) const;	///< returns the solid mask for the given player, so bots can have a more-restrictive set

public:
	CCSPlayer *m_pCSPlayer;

private:
	void HandleDuckingSpeedCrop( float duckFraction );
	inline float GetDuckSpeedModifier( float duckFraction ) const { return CS_PLAYER_SPEED_DUCK_MODIFIER * duckFraction + 1.0f - duckFraction; }
	bool DuckingEnabled();
};

// Expose our interface.
static CCSGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

static InterfaceReg s_Reg_CGameMovement_IGameMovement( side_registry(), []() -> void * { return static_cast<IGameMovement *>( &g_GameMovement ); }, INTERFACENAME_GAMEMOVEMENT );;

// ---------------------------------------------------------------------------------------- //
// CCSGameMovement.
// ---------------------------------------------------------------------------------------- //

CCSGameMovement::CCSGameMovement()
	: CGameMovement( s_MoveServices )
{
}

//-----------------------------------------------------------------------------
// Purpose: Allow bots etc to use slightly different solid masks
//-----------------------------------------------------------------------------
unsigned int CCSGameMovement::PlayerSolidMask( bool brushOnly, CPlayerMoveCore *testPlayer ) const
{
	bool isBot = !BasePlayer() || BasePlayer()->IsBot();
	unsigned int uMask = MASK_PLAYERSOLID_BRUSHONLY;

	if ( !brushOnly )
	{
		uMask = BasePlayer()->PhysicsSolidMaskForEntity();
	}

	if ( isBot )
	{
		uMask |= CONTENTS_MONSTERCLIP;
	}

	return uMask;
}

bool CCSGameMovement::DuckingEnabled()
{
	// Anti-duck-spam: If mashing duck key too much, treat as unpressed to avoid being
	// able to "camp" in half-ducked positions by pressing/releasing near a target height.
	if ( m_pCSPlayer->m_flDuckSpeed < 1.5f )
		return false;

	// After completing a duck/unduck, we can't re-duck for a minimum amount of time.
	// This is to minimize ugly camera shaking due to immediate duck/un-duck in the air.
	// before the above anti-spam code kicks in.
	if ( ( BasePlayer()->GetFlags() & FL_DUCKING ) == 0
		 && gpGlobals->curtime < BasePlayer()->m_Local.m_flLastDuckTime + sv_timebetweenducks.GetFloat() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameMovement::CheckParameters( void )
{
	// HACK: IN_BULLRUSH isn't used anywhere.  We use it to store the "raw" value of whether
	//       IN_DUCK was held, before it is modified by code.  This way when m_nButtons is
	//       copied to m_nOldButtons we can tell if duck was pressed or released on the next
	//       frame.

	QAngle	v_angle;

	// Save off state of IN_DUCK before we set/unset it from gameplay effects
	if ( ( mv->m_nButtons & IN_DUCK ) != 0 )
		mv->m_nButtons |= IN_BULLRUSH;

	// Add duck-spam speed penalty when we press or release the duck key.
	// (this is the only place that IN_RAWDUCK is checked)
	if ( ( mv->m_nButtons & IN_BULLRUSH ) != ( mv->m_nOldButtons & IN_BULLRUSH ) )
	{
		BasePlayer()->m_flDuckSpeed = std::max<std::common_type_t<int, float>>( 0, BasePlayer()->m_flDuckSpeed - get_sv_crouch_spam_penalty );
	}

	// Cancel auto-duck if player hits +duck, so cancel our auto duck
	// Note: only bots use m_duckUntilOnGround
	if ( m_pCSPlayer->m_duckUntilOnGround && ( mv->m_nButtons & IN_DUCK ) )
	{
		m_pCSPlayer->m_duckUntilOnGround = false;
	}

	// Anti-duck-spam: If mashing duck key too much, treat as unpressed to avoid being
	// able to "camp" in half-ducked positions by pressing/releasing near a target height.
	if ( !DuckingEnabled() )
	{
		mv->m_nButtons &= ~IN_DUCK;
	}

	// maintaining auto-duck during jumps
	// Note: only bots use m_duckUntilOnGround
	if ( m_pCSPlayer->m_duckUntilOnGround && !BasePlayer()->GetGroundEntity() && BasePlayer()->GetMoveType() != MOVETYPE_LADDER )
	{
		mv->m_nButtons |= IN_DUCK;
	}

	// Always duck during bomb-plant animation
	if ( m_pCSPlayer->m_bDuckOverride )
	{
		mv->m_nButtons |= IN_DUCK;
	}

	// it would be nice to put this into the player->GetPlayerMaxSpeed() method, but
	// this flag is only stored in the move!

	bool walkButtonIsDown = ( mv->m_nButtons & ( /*IN_WALK | */ IN_SPEED ) ) != 0;
	bool runButtonIsDown = ( mv->m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT | IN_RUN) ) != 0;
	bool moveForward = ( mv->m_nButtons & ( IN_FORWARD ) ) != 0;
	bool moveBackward = ( mv->m_nButtons & ( IN_BACK ) ) != 0;
	bool moveRight = ( mv->m_nButtons & ( IN_MOVERIGHT ) ) != 0;
	bool moveLeft = ( mv->m_nButtons & ( IN_MOVELEFT ) ) != 0;
	bool opposingForwardBack = ( moveForward && moveBackward );
	bool opposingRightLeft = ( moveRight && moveLeft );

	// NOTE[pmf] ignore the walk button when we're ducking; this test matches that in CCSGameMovement::HandleDuckingSpeedCrop()
	// we can't simply test mv->m_iSpeedCropped == SPEED_CROPPED_DUCK, because that is set AFTER this code executes
	if ( ( mv->m_nButtons & IN_DUCK ) || ( BasePlayer()->m_Local.m_bDucking ) || ( BasePlayer()->GetFlags() & FL_DUCKING ) )
	{
		walkButtonIsDown = false;
	}

	if ( walkButtonIsDown )
	{
		// we don't cap walk immediately, let the player decelerate and only cap when the speed is really close
		float currentspeed = m_pCSPlayer->GetLocalVelocity( ).Length( );
		if ( currentspeed < ( ( mv->m_flMaxSpeed * CS_PLAYER_SPEED_WALK_MODIFIER ) + 25 ) )
		{
			mv->m_flMaxSpeed *= CS_PLAYER_SPEED_WALK_MODIFIER;
			m_pCSPlayer->m_bIsWalking = true;
		}
	}
	else
	{
		m_pCSPlayer->m_bIsWalking = false;
	}

	float speed_squared = 0.0f;

	if ( BasePlayer()->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 BasePlayer()->GetMoveType() != MOVETYPE_NOCLIP &&
		 BasePlayer()->GetMoveType() != MOVETYPE_OBSERVER	)
	{
		speed_squared = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove ) +
			  ( mv->m_flUpMove * mv->m_flUpMove );

		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if (BasePlayer()->m_pSurfaceData)
		{
			flSpeedFactor = BasePlayer()->m_pSurfaceData->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

		// Take the player's velocity modifier into account
		if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
		{
			flSpeedFactor *= m_pCSPlayer->m_flVelocityModifier;
		}

		mv->m_flMaxSpeed *= flSpeedFactor;

		// stamina slowing factor
		if ( m_pCSPlayer->m_flStamina > 0 )
		{
			float fSpeedScale = clamp(1.0f - m_pCSPlayer->m_flStamina / STAMINA_RANGE, 0.f, 1.f);
			fSpeedScale *= fSpeedScale;	// square the scale factor so that it correlates more closely with the jump penalty (i.e. a 50% stamina jumps .25 * normal height)
			mv->m_flMaxSpeed *= fSpeedScale;
		}

		if ( g_bMovementOptimizations )
		{
			// Same thing but only do the sqrt if we have to.
			if ( ( speed_squared != 0.0 ) && ( speed_squared > mv->m_flMaxSpeed*mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / std::sqrt( speed_squared );
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
		else
		{
			float spd = std::sqrt( speed_squared );
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / spd;
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
	}
	
	if ( BasePlayer()->GetFlags() & FL_FROZEN ||
		 BasePlayer()->GetFlags() & FL_ONTRAIN || 
		 IsDead() )
	{
		if ( BasePlayer()->GetObserverMode() != OBS_MODE_ROAMING )
		{
			mv->m_flForwardMove = 0;
			mv->m_flSideMove    = 0;
			mv->m_flUpMove      = 0;
		}
	}

	DecayViewPunchAngle();
	DecayAimPunchAngle();

	// Take angles from command.
	if ( !IsDead() )
	{
		v_angle = mv->m_vecAngles;
		v_angle = v_angle + BasePlayer()->m_Local.m_viewPunchAngle;

		// Now adjust roll angle
		if ( BasePlayer()->GetMoveType() != MOVETYPE_ISOMETRIC  &&
			 BasePlayer()->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecAngles[ROLL]  = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0; // v_angle[ ROLL ];
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW]   = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	// Set dead player view_offset
	if ( IsDead() && 
		 BasePlayer()->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		BasePlayer()->SetViewOffset( VEC_DEAD_VIEWHEIGHT );
	}

	// Adjust client view angles to match values used on server.
	mv->m_vecAngles[YAW] = AngleNormalize( mv->m_vecAngles[YAW] );

	// If we're standing on a player, then force them off.
	if ( !BasePlayer()->IsObserver()  && ( BasePlayer()->GetMoveType() != MOVETYPE_LADDER ) )
	{
		int nLevels = 0;
		CBaseEntity *pCurGround = BasePlayer()->GetGroundEntity();
		while ( pCurGround && pCurGround->IsPlayer() && nLevels < 1000 )
		{
			pCurGround = pCurGround->GetGroundEntity();
			++nLevels;
		}
		if ( nLevels == 1000 )
			Warning( "BUG: CCSGameMovement::CheckParameters - too many stacking levels.\n" );

		// If they're stacked too many levels deep, slide them off.
		if ( nLevels > 1 )
		{
			mv->m_flForwardMove = mv->m_flMaxSpeed * 3;
			mv->m_flSideMove = 0;
			mv->m_nButtons = 0;
			mv->m_nImpulseCommand = 0;
		}
	}

	// Determine if the player is trying to run / move at full speed.
	m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // idle, not driving the character
	if ( runButtonIsDown )
	{
		if ( opposingForwardBack && opposingRightLeft )
		{
			m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // Idle, don't move if we are holding all 4 directions down.
		}
		else if ( opposingForwardBack || opposingRightLeft )
		{
			if ( ( opposingForwardBack && ( moveRight || moveLeft ) ) || ( opposingRightLeft && ( moveForward || moveBackward ) ) )
			{
				m_pCSPlayer->m_iMoveState = MOVESTATE_RUN; // Run, move if we are holding 3 buttons down, 2 are opposing directions.
			}
			else
			{
				m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // Idle, don't move if we are holding just 2 opposing directions down.
			}

		}
		else
		{
			m_pCSPlayer->m_iMoveState = MOVESTATE_RUN; // Run
		}
	}

	if ( ( m_pCSPlayer->m_iMoveState == MOVESTATE_RUN ) && walkButtonIsDown )
	{
		m_pCSPlayer->m_iMoveState = MOVESTATE_WALK; // Walk
	}

}

void CCSGameMovement::ProcessMovement( CPlayerMoveCore *pCore, CMoveData *pMove )
{
	CBasePlayer *pBasePlayer = static_cast<CBasePlayer *>( pCore );
	m_pCSPlayer = static_cast<CCSPlayer *>( pBasePlayer );
	
	if ( m_pCSPlayer->IsBot() && m_pCSPlayer->IsDormant() )
		return;

	BaseClass::ProcessMovement( pBasePlayer, pMove );
}

bool CCSGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	CSPlayerState curState = m_pCSPlayer->State_Get();
	if ( curState == STATE_ACTIVE )
	{
		return BasePlayer()->GetWaterJumpTime() == 0;
	}
	else if ( BasePlayer()->IsObserver() )
	{
		return true;
	}
	else
	{	
		return false;
	}
}

void CCSGameMovement::PlayerMove()
{
	if ( !m_pCSPlayer->CanMove() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove = 0;
		mv->m_flUpMove = 0;
		mv->m_nButtons &= ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
	}

	BaseClass::PlayerMove();

	if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
	{
		const float kVelocityRecoveryRate = 1.f / 2.5f;
		if ( m_pCSPlayer->m_flVelocityModifier < 1.0 )
		{
			m_pCSPlayer->m_flVelocityModifier = clamp(m_pCSPlayer->m_flVelocityModifier + gpGlobals->frametime * kVelocityRecoveryRate, 0.0f, 1.0f);
		}
	}

	if( BasePlayer() && 
		BasePlayer()->GetTeamNumber() == TEAM_SPECTATOR &&
		BasePlayer()->GetObserverMode() == OBS_MODE_FIXED )
	{
		// [dkorus] if we're using a fixed view mode when spectating, we need to make sure we bump the camera up off the ground
		BasePlayer()->SetViewOffset( VEC_VIEW );
	}

	if ( m_pCSPlayer->IsAlive() )
	{
		if ( ( BasePlayer()->GetFlags() & FL_DUCKING ) == 0 && !BasePlayer()->m_Local.m_bDucking && !BasePlayer()->m_Local.m_bDucked )
		{
			BasePlayer()->SetViewOffset( VEC_VIEW );
		}
		else if ( m_pCSPlayer->m_duckUntilOnGround )
		{
			// Duck Hull, but we're in the air.  Calculate where the view would be.
			Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
			Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;

			// We've got the duck hull, pulled up to the top of where the player should be
			Vector lowerClearance = hullSizeNormal - hullSizeCrouch;
			Vector duckEyeHeight = GetPlayerViewOffset( false ) - lowerClearance;
			BasePlayer()->SetViewOffset( duckEyeHeight );
		}
		else if( BasePlayer()->m_Local.m_bDucked && !BasePlayer()->m_Local.m_bDucking )
		{
			BasePlayer()->SetViewOffset( VEC_DUCK_VIEW );
		}
	}

	if ( !m_pCSPlayer->m_bHasMovedSinceSpawn && Vector2DLength( mv->m_vecVelocity.AsVector2D() ) != 0  )
		m_pCSPlayer->m_bHasMovedSinceSpawn = true;

}

void CCSGameMovement::WalkMove( void )
{
	BaseClass::WalkMove();
}

//-------------------------------------------------------------------------------------------------------------------------------
void CCSGameMovement::AirMove( void )
{
	BaseClass::AirMove();
}

//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::OnLadder( trace_t &trace )
{
	if ( trace.plane.normal.z == 1.0f )
		return false;

	return BaseClass::OnLadder( trace );
}

//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::LadderMove( void )
{
	bool isOnLadder = BaseClass::LadderMove();
	if ( isOnLadder && m_pCSPlayer )
	{
		m_pCSPlayer->SurpressLadderChecks( mv->GetAbsOrigin(), m_pCSPlayer->m_vecLadderNormal );
	}

	return isOnLadder;
}

//-------------------------------------------------------------------------------------------------------------------------------
/**
 * In CS, crouching or walking up ladders goes slowly and shouldn't make a sound.
 */
float CCSGameMovement::ClimbSpeed( void ) const
{
	if ( ( mv->m_nButtons & IN_DUCK ) || ( mv->m_nButtons & IN_SPEED ) )
	{
		return BaseClass::ClimbSpeed() * CS_PLAYER_SPEED_CLIMB_MODIFIER;
	}
	else
	{
		return BaseClass::ClimbSpeed();
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
/**
* In CS, strafing on ladders goes slowly.
*/
float CCSGameMovement::LadderLateralMultiplier( void ) const
{
	if ( mv->m_nButtons & IN_DUCK )
	{
		return 1.0f;
	}
	else
	{
		return 0.5f;
	}
}

void CCSGameMovement::ReduceTimers( void )
{
	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		m_pCSPlayer->m_flStamina -= gpGlobals->frametime * sv_staminarecoveryrate.GetFloat();

		if ( m_pCSPlayer->m_flStamina < 0 )
		{
			m_pCSPlayer->m_flStamina = 0;
		}
	}

	BaseClass::ReduceTimers();
}

ConVar sv_enablebunnyhopping( side_registry(), "sv_enablebunnyhopping", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Allow BasePlayer() speed to exceed maximum running speed" );
ConVar sv_autobunnyhopping( side_registry(), "sv_autobunnyhopping", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Players automatically re-jump while holding jump button" );

// Only allow bunny jumping up to 1.1x server / player maxspeed setting
static constexpr auto BUNNYJUMP_MAX_SPEED_FACTOR = 1.1f;

// taken from TF2 but changed BUNNYJUMP_MAX_SPEED_FACTOR from 1.1 to 1.0
void CCSGameMovement::PreventBunnyJumping()
{
	// Speed at which bunny jumping is limited
	float maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * BasePlayer()->m_flMaxspeed;
	if ( maxscaledspeed <= 0.0f )
		return;

	// Current player speed
	float spd = mv->m_vecVelocity.Length();

	if ( spd <= maxscaledspeed )
		return;

	// Apply this cropping fraction to velocity
	float fraction = ( maxscaledspeed / spd );

	mv->m_vecVelocity *= fraction;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameMovement::CheckJumpButton( void )
{
	if (m_pCSPlayer->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (m_pCSPlayer->m_flWaterJumpTime)
	{
		m_pCSPlayer->m_flWaterJumpTime -= gpGlobals->frametime;
		if (m_pCSPlayer->m_flWaterJumpTime < 0)
			m_pCSPlayer->m_flWaterJumpTime = 0;
		
		return false;
	}

	// Can't jump while in a thirdperson taunt
	if ( m_pCSPlayer->IsTaunting() && m_pCSPlayer->IsThirdPersonTaunt() )
		return false;

//#if !defined(CLIENT_DLL)
//	if ( m_pCSPlayer->IsTaunting() )
//	{
//		// They jumped so break out of the taunt!
//		m_pCSPlayer->StopTaunting();
//	}
//#endif

	// If we are in the water most of the way...
	if ( m_pCSPlayer->GetWaterLevel() >= WL_Waist )
	{	
		// swimming, not jumping
		SetGroundEntity( nullptr );

		if(m_pCSPlayer->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (m_pCSPlayer->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( m_pCSPlayer->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			m_pCSPlayer->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	// are we jumping on another player's head?
	bool bStandingOnOtherPlayer = false;
	bool bStandingOnFallingPlayer = false;
	CBaseEntity *groundEntity = m_pCSPlayer->GetGroundEntity();
	if ( groundEntity && !groundEntity->IsWorld() && groundEntity->IsPlayer() )
	{
		bStandingOnOtherPlayer = true;
		if ( groundEntity->GetGroundEntity() == nullptr )
		{
			bStandingOnFallingPlayer = true;
		}
	}

	// the player jumped so this bool will remain false until the player next walks
	BasePlayer()->m_bHasWalkMovedSinceLastJump = false;

	// No more effect
 	if (m_pCSPlayer->GetGroundEntity() == nullptr)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( ( mv->m_nOldButtons & IN_JUMP ) != 0 && !sv_autobunnyhopping.GetBool() )
		return false;		// don't pogo stick

	if ( !sv_enablebunnyhopping.GetBool() )
	{
		PreventBunnyJumping();
	}

	// In the air now.
	SetGroundEntity( nullptr );

	// if we're walking or standing still, play only a local sounds
	if ( mv->m_vecVelocity.Length() > 126 )
	{
		m_pCSPlayer->PlayStepSound( (Vector &)mv->GetAbsOrigin(), BasePlayer()->m_pSurfaceData, 1.0, true );
	}

	//MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
	m_pCSPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );

	float flGroundFactor = 1.0f;
	if (BasePlayer()->m_pSurfaceData)
	{
		flGroundFactor = BasePlayer()->m_pSurfaceData->game.jumpFactor; 
	}

	// if we weren't ducking, bots and hostages do a crouchjump programatically
	if ( (!BasePlayer() || BasePlayer()->IsBot()) && !(mv->m_nButtons & IN_DUCK) )
	{
		m_pCSPlayer->m_duckUntilOnGround = true;
		FinishDuck();
	}

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	if ( bStandingOnFallingPlayer )
	{
		mv->m_vecVelocity[2] = 0.0f;
	}
	else if ( m_pCSPlayer->m_duckUntilOnGround || (  m_pCSPlayer->m_Local.m_bDucking ) || (  m_pCSPlayer->GetFlags() & FL_DUCKING ) || bStandingOnOtherPlayer )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		
		mv->m_vecVelocity[2] = flGroundFactor * sv_jump_impulse.GetFloat();  // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * sv_jump_impulse.GetFloat();  // 2 * gravity * height
	}

	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		mv->m_vecVelocity[2] *= clamp(1.0f - m_pCSPlayer->m_flStamina / STAMINA_RANGE, 0.f, 1.f);
	}

	FinishGravity();

	mv->m_outWishVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.1f;

	OnJump(mv->m_outWishVel.z);

	// allow bots to react
	IGameEvent * event = gameeventmanager->CreateEvent( "player_jump" );
	if ( event )
	{
		event->SetInt( "userid", m_pCSPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}

void HybridDecay( QAngle& v, float fExp, float fLin, float dT )
{
	fExp *= dT;
	fLin *= dT;

	v *= std::expf(-fExp);

	float fMag = v.Length();
	if ( fMag > fLin )
	{
		v *= (1.0f - fLin / fMag);
	}
	else
	{
		v.Init(0.0f, 0.0f, 0.0f);
	}
}

void CCSGameMovement::DecayAimPunchAngle( void )
{
	QAngle punchAngle = m_pCSPlayer->m_Local.m_aimPunchAngle;
	QAngle punchAngleVel = m_pCSPlayer->m_Local.m_aimPunchAngleVel;

	// decay the punch angle
	HybridDecay(punchAngle, weapon_recoil_decay2_exp.GetFloat(), weapon_recoil_decay2_lin.GetFloat(), TICK_INTERVAL);

	// add in the velocity
	punchAngle += punchAngleVel * TICK_INTERVAL * 0.5f;

	// decay the punch angle velocity
	punchAngleVel *= std::expf(TICK_INTERVAL * -weapon_recoil_vel_decay.GetFloat());

	punchAngle += punchAngleVel * TICK_INTERVAL * 0.5f;

	// save off the new values
	m_pCSPlayer->m_Local.m_aimPunchAngle = punchAngle;
	m_pCSPlayer->m_Local.m_aimPunchAngleVel = punchAngleVel;
}

void CCSGameMovement::HandleDuckingSpeedCrop( float duckFraction )
{
	// [Forrest] Movement speed in free look camera mode is unaffected by ducking state.
	if ( BasePlayer()->GetObserverMode() == OBS_MODE_ROAMING )
		return;

	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) )
	{
		if ( ( mv->m_nButtons & IN_DUCK ) || ( BasePlayer()->m_Local.m_bDucking ) || ( BasePlayer()->GetFlags() & FL_DUCKING ) )
		{
			float duckSpeedModifier = GetDuckSpeedModifier(duckFraction);
			//DevMsg( "duckSpeedModifier = %f\n", duckSpeedModifier );
			mv->m_flForwardMove	*= duckSpeedModifier;
			mv->m_flSideMove	*= duckSpeedModifier;
			mv->m_flUpMove		*= duckSpeedModifier;
			mv->m_flMaxSpeed	*= duckSpeedModifier;
			m_iSpeedCropped		|= SPEED_CROPPED_DUCK;
		}
	}
}

bool CCSGameMovement::CanUnduck()
{
	// Can't unduck if we are planting the bomb.
	if ( m_pCSPlayer->m_bDuckOverride )
		return false;

	// Can always unduck if we are no-clipping
	if ( BasePlayer()->GetMoveType() == MOVETYPE_NOCLIP )
		return true;
	
	// Check to see if we would collide on anything if we unducked.
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( BasePlayer()->GetGroundEntity() != nullptr )
	{
		newOrigin += VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching
 		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		
		newOrigin += -0.5f * ( hullSizeNormal - hullSizeCrouch );
	}

	UTIL_TraceHull( mv->GetAbsOrigin(), newOrigin, VEC_HULL_MIN, VEC_HULL_MAX, PlayerSolidMask(), BasePlayer(), COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.startsolid || ( trace.fraction != 1.0f ) )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishUnDuck( void )
{
	Vector newOrigin = mv->GetAbsOrigin();
	if ( BasePlayer()->GetGroundEntity() != nullptr || BasePlayer()->GetMoveType() == MOVETYPE_LADDER )
	{
		Vector hullMinDelta = VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
		newOrigin += hullMinDelta;
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeCrouch );
		newOrigin += viewDelta;
	}

	mv->SetAbsOrigin( newOrigin );

	BasePlayer()->RemoveFlag( FL_DUCKING | FL_ANIMDUCKING );
	BasePlayer()->m_Local.m_bDucked = false;
	BasePlayer()->m_Local.m_bDucking = false;
	BasePlayer()->m_Local.m_nDuckTimeMsecs = 0; // legacy
	BasePlayer()->SetViewOffset( GetPlayerViewOffset( false ) );

	// Recategorize position since ducking can change origin
	CategorizePosition();

	BasePlayer()->m_flDuckAmount = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishDuck( void )
{
	contract_assert( !BasePlayer()->m_Local.m_bDucked );

	Vector newOrigin = mv->GetAbsOrigin();

	if ( BasePlayer()->GetGroundEntity() != nullptr || BasePlayer()->GetMoveType() == MOVETYPE_LADDER )
	{
		Vector hullMinDelta = VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
		newOrigin -= hullMinDelta;
	}
	else
	{
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeCrouch );
		newOrigin -= viewDelta;
	}
	mv->SetAbsOrigin( newOrigin );

	BasePlayer()->SetViewOffset( GetPlayerViewOffset( true ) );
	BasePlayer()->m_Local.m_bDucking = false;
	BasePlayer()->m_Local.m_bDucked = true;
	BasePlayer()->m_Local.m_flLastDuckTime = gpGlobals->curtime;
	BasePlayer()->AddFlag( FL_ANIMDUCKING | FL_DUCKING );

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();

	BasePlayer()->m_flDuckAmount = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CCSGameMovement::Duck( void )
{
	const bool playerTouchingGround = BasePlayer()->GetGroundEntity() != nullptr;

	// Check to see if we are in the air.
	const bool bInAir = !playerTouchingGround && BasePlayer()->GetMoveType() != MOVETYPE_LADDER;

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Dead players don't duck.
	if ( IsDead() && !BasePlayer()->IsObserver() )
	{
		// They also don't plant the bomb.
		m_pCSPlayer->m_bDuckOverride = false;

		if ( BasePlayer()->GetFlags() & FL_DUCKING )
		{
			FinishUnDuck();
		}

		return;
	}

	if ( m_pCSPlayer->m_duckUntilOnGround )
	{
		// This code handles the case where a bot is jumping; they
		// automatically crouch jump, and we want to decide if they are
		// ready to un-duck here.

		// TODO: Should we move this code into the bot movement logic
		//       instead?

		// $$$REI There still seems to be a way to end up in this state if the bot
		//        was crouch-jumping at the end of the round; I haven't found where
		//        his flags are getting reset.  Will fix later, and just reset the
		//        inconsistent state here.  Next tick the bot will behave normally.
		// Assert( player->GetFlags() & FL_DUCKING );
		if ( ( BasePlayer()->GetFlags() & FL_DUCKING ) == 0 )
		{
			m_pCSPlayer->m_duckUntilOnGround = false;
			return;
		}

		// If we have landed, we are done with 'duck until on ground'.
		if ( !bInAir )
		{
			m_pCSPlayer->m_duckUntilOnGround = false;

			// Stop crouching if possible
			if ( CanUnduck() )
			{
				FinishUnDuck();
			}

			return;
		}

		// Otherwise we are still in the air.
		// Try to un-duck just as we land, for better animation and movement.

		// If we're still going up, we aren't about to land.  Early-out.
		if ( mv->m_vecVelocity.z > 0.0f )
			return;

		// Check if we are close enough to the ground and that there is room to un-duck.
		trace_t trace;
		Vector newOrigin;
		Vector groundCheck;

		VectorCopy( mv->GetAbsOrigin(), newOrigin );
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		newOrigin -= ( hullSizeNormal - hullSizeCrouch );
		groundCheck = newOrigin;
		groundCheck.z -= BasePlayer()->GetStepSize();

		UTIL_TraceHull( newOrigin, groundCheck, VEC_HULL_MIN, VEC_HULL_MAX, PlayerSolidMask(), BasePlayer(), COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( trace.startsolid			// No room to unduck.
			|| trace.fraction == 1.0f	// We are still in the air
			)
			return;

		// Success!  We can un-duck.  Remove "un-duck when possible" flag.
		m_pCSPlayer->m_duckUntilOnGround = false;

		// Theoretically CanUnduck() should always succeed here since we just did a hull trace.
		// REI: But the hulltrace in CanUnduck() looks slightly different than this one; it uses
		//         newOrigin = mv->GetAbsOrigin() + -0.5f * ( hullSizeNormal - hullSizeCrouch )
		//      and traces from mv->GetAbsOrigin() to newOrigin instead of from newOrigin to a
		//      step away.
		if ( CanUnduck() )
		{
			FinishUnDuck();
		}

		return;
	}

	// Reduce duck-spam penalty over time
	BasePlayer()->m_flDuckSpeed = Approach( CS_PLAYER_DUCK_SPEED_IDEAL, BasePlayer()->m_flDuckSpeed, gpGlobals->frametime * 3.0f );

	// Use the last-known position of full crouch speed to restore crouch speed as a function of physical player position.
	// The goal is that moving a sufficient distance should reset crouch speed in an intuitive manner.
	if ( BasePlayer()->m_flDuckSpeed >= CS_PLAYER_DUCK_SPEED_IDEAL )
	{
		BasePlayer()->m_vecLastPositionAtFullCrouchSpeed = BasePlayer()->GetAbsOrigin().AsVector2D();
	}
	else if ( BasePlayer()->m_flDuckAmount <= 0 || BasePlayer()->m_flDuckAmount >= 1 )
	{
		//debugoverlay->AddLineOverlay( player->m_vecLastPositionAtFullCrouchSpeed, player->GetAbsOrigin(), 255,0,0, true, 0.1f );
		//debugoverlay->AddTextOverlay( player->GetAbsOrigin(), 0.1f, "%f", player->m_flDuckSpeed );

		float flDistToLastPositionAtFullCrouchSpeed = BasePlayer()->m_vecLastPositionAtFullCrouchSpeed.DistToSqr( BasePlayer()->GetAbsOrigin().AsVector2D() );

		// if we're sufficiently far from the last full crouch speed location, we can safely restore crouch speed faster.
		if ( flDistToLastPositionAtFullCrouchSpeed > (64*64) )
		{
			BasePlayer()->m_flDuckSpeed = Approach( CS_PLAYER_DUCK_SPEED_IDEAL, BasePlayer()->m_flDuckSpeed, gpGlobals->frametime * 6.0f );
		}
	}

	bool duckButtonHeld = ( mv->m_nButtons & IN_DUCK ) != 0;

	if ( !duckButtonHeld && BasePlayer()->m_flDuckAmount > 0 )
	{
		// Not sure if this is the appropriate use of this flag. It seems odd to have a dedicated variable that effectively means crouch-is-not-zero-or-one.

		// When the round restarts with the player in the ducked state, they can get stuck crouched.
		// To prevent this, I'm setting the "duck-in-progress" bool (m_bDucking) to true if
		// the player is ever in the state of NOT holding the duck button but is still ducked.
		BasePlayer()->m_Local.m_bDucking = true;
	}
	else if ( duckButtonHeld && BasePlayer()->m_flDuckAmount < 1 )
	{
		// or if the player IS holding the duck button but isn't yet fully ducked.
		BasePlayer()->m_Local.m_bDucking = true;
	}

	// Handle animating into the ducking pose.
	if ( duckButtonHeld && BasePlayer()->m_Local.m_bDucking )
	{
		contract_assert( !BasePlayer()->m_Local.m_bDucked );

		// ducking is always a little slower than unducking
		float duckSpeed = BasePlayer()->m_flDuckSpeed * 0.8f;

		// Reduce crouch/uncrouch speed significantly while defusing
		if ( m_pCSPlayer->m_bIsDefusing )
			duckSpeed *= 0.4f;

		BasePlayer()->m_flDuckAmount = Approach( 1.0f, BasePlayer()->m_flDuckAmount, gpGlobals->frametime * duckSpeed );

		// Finish ducking immediately if duck time is over or not on ground
		if ( BasePlayer()->m_flDuckAmount >= 1.0f || !playerTouchingGround )
		{
			FinishDuck();
		}
		else
		{
			SetDuckedEyeOffset( BasePlayer()->m_flDuckAmount );
		}

		// REI: For some reason we don't set this flag immediately, but wait until you have ducked a little bit.  Investigate?
		if ( BasePlayer()->m_flDuckAmount >= 0.1f && !( BasePlayer()->GetFlags() & FL_ANIMDUCKING ) )
		{
			BasePlayer()->AddFlag( FL_ANIMDUCKING );
		}
	}

	// Handle animating out of ducking pose.
	if ( !duckButtonHeld && BasePlayer()->m_Local.m_bDucking
		// Try to unduck unless automovement is not allowed
		// NOTE: When not onground, you can always unduck
		// REI: Cloned behavior from old code, not sure when m_bAllowAutomovement is used?
		&& ( BasePlayer()->m_Local.m_bAllowAutoMovement || !playerTouchingGround ) )
	{
		if ( CanUnduck() )
		{
			// Always unduck at at least 1.5 to prevent advantageous semi-ducked positions
			float duckSpeed = std::max<std::common_type_t<float, CNetworkVarBase<float, CBasePlayer::NetworkVar_m_flDuckSpeed>>>( 1.5f, BasePlayer()->m_flDuckSpeed );

			// Reduce crouch/uncrouch speed significantly while defusing
			if ( m_pCSPlayer->m_bIsDefusing )
				duckSpeed *= 0.4f;

			BasePlayer()->m_flDuckAmount = Approach( 0.0f, BasePlayer()->m_flDuckAmount, gpGlobals->frametime * duckSpeed );
			BasePlayer()->m_Local.m_bDucked = false;

			if ( BasePlayer()->m_flDuckAmount <= 0.0f || !playerTouchingGround )
			{
				FinishUnDuck();
			}
			else
			{
				SetDuckedEyeOffset( BasePlayer()->m_flDuckAmount );
			}

			// Remove the ducked flags if we're not fully ducked anymore.
			// REI: This is inconsistent with the documentation for these flags, but I'm not sure why the code
			//      is doing this.  It does mean you lose your ducking accuracy bonus very early in the un-duck,
			//      which is certainly important.
			if ( BasePlayer()->m_flDuckAmount <= 0.75f && BasePlayer()->GetFlags() & ( FL_ANIMDUCKING | FL_DUCKING ) )
			{
				BasePlayer()->RemoveFlag( FL_ANIMDUCKING | FL_DUCKING );
			}
		}
		else
		{
			// Reset to fully-ducked as we went under something we can't un-duck from.
			// We'll try again once the player has moved out of the obstructing obstacle.
			BasePlayer()->m_flDuckAmount = 1.0f;
			BasePlayer()->m_Local.m_bDucked = true;
			BasePlayer()->m_Local.m_bDucking = false;
			BasePlayer()->AddFlag( FL_ANIMDUCKING | FL_DUCKING );

			SetDuckedEyeOffset( BasePlayer()->m_flDuckAmount );
		}
	}

	// REI: I think I've fixed all cases of this happening.  Leaving it in for now.  $$$REI remove this after testing shows it never happening again.
	if ( BasePlayer()->m_flDuckAmount <= 0 && (BasePlayer()->GetFlags() & FL_ANIMDUCKING) )
	{
		contract_assert( false && "Clearing FL_ANIMDUCKING flag on BasePlayer() %s to prevent crab-walk.  Please let Ryan know if you hit this." );
		BasePlayer()->RemoveFlag( FL_ANIMDUCKING );
	}

	HandleDuckingSpeedCrop( BasePlayer()->m_flDuckAmount );
}

void CCSGameMovement::OnJump( float fImpulse )
{
	float flStamCost = sv_staminajumpcost.GetFloat( );

	m_pCSPlayer->m_flStamina = clamp( m_pCSPlayer->m_flStamina + flStamCost * fImpulse, 0.0f, sv_staminamax.GetFloat( ) );

	m_pCSPlayer->OnJump( fImpulse );
}	

void CCSGameMovement::OnLand( float fVelocity )
{
	m_pCSPlayer->m_flStamina = clamp( m_pCSPlayer->m_flStamina + sv_staminalandcost.GetFloat() * fVelocity, 0.0f, sv_staminamax.GetFloat());

	m_pCSPlayer->OnLand( fVelocity );
}

// override the default behavior in order to change acceleration based on movement modifiers
void CCSGameMovement::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	if ( !CanAccelerate() )
		return;

	float flStoredAccel = accel;
	// See if we are changing direction a bit
	float currentspeed = mv->m_vecVelocity.Dot(wishdir);
	float flUnmodifiedSpeed = currentspeed;

	// Reduce wishspeed by the amount of veer.
	float addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	if ( currentspeed < 0 )
		currentspeed = 0;

	bool bIsDucking = ( mv->m_nButtons & IN_DUCK ) || ( BasePlayer()->m_Local.m_bDucking ) || ( BasePlayer()->GetFlags() & FL_DUCKING );
	bool bIsWalking = ( mv->m_nButtons & ( /*IN_WALK | */ IN_SPEED ) ) != 0 && !bIsDucking;

	float flMaxSpeed = 250.0f;
	float fAccelerationScale = std::max(flMaxSpeed, wishspeed);
	float flGoalSpeed = fAccelerationScale;

	// if this convar is set, we ignore the passed in accel value (sv_acceleration) and accelerate based on the one below instead
	float flZeroToMaxSpeedTime = SV_ACCELERATE_EXPONENT_TIME;

	CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase * >( BasePlayer()->GetActiveWeapon() );
/*		flMaxScaleSpeed = mv->m_flMaxSpeed;*/
	
	bool bIsSlowSniperScoped = false;

	flGoalSpeed = fAccelerationScale;
	if ( sv_accelerate_use_weapon_speed.GetBool( ) && csWeapon )
	{
		bIsSlowSniperScoped = (csWeapon->GetCSZoomLevel() > 0 && csWeapon->GetZoomLevels() > 1 
								&& (csWeapon->GetMaxSpeed( ) * CS_PLAYER_SPEED_WALK_MODIFIER) < 110.0);

		flGoalSpeed *= std::min( 1.0f, ( csWeapon->GetMaxSpeed( ) / flMaxSpeed ) );
		
		if ( (!bIsDucking && !bIsWalking) || (( bIsWalking || bIsDucking) && bIsSlowSniperScoped) )
			fAccelerationScale *= std::min( 1.0f, (csWeapon->GetMaxSpeed() / flMaxSpeed));
	}

	// TODO: make this number not a magic number
	if ( bIsDucking )
	{
		if ( !bIsSlowSniperScoped )
			fAccelerationScale *= CS_PLAYER_SPEED_DUCK_MODIFIER;

		flGoalSpeed *= CS_PLAYER_SPEED_DUCK_MODIFIER;
	}

	if ( bIsWalking )
	{
		if ( !bIsSlowSniperScoped )
			fAccelerationScale *= CS_PLAYER_SPEED_WALK_MODIFIER;

		flGoalSpeed *= CS_PLAYER_SPEED_WALK_MODIFIER;
	}

	/*
	bool bShouldStutter = (wishspeed > 200.0f);
	if ( bShouldStutter == false && bIsDucking == false && bIsWalking == false )
	{
		flStoredAccel *= 0.75;//clamp( 1.0f - ( std::max( 0.0f, currentspeed - ( flGoalSpeed - 5 ) ) / std::max( 0.0f, flGoalSpeed - ( flGoalSpeed - 5 ) ) ), 0.0f, 1.0f );
	}
	// we no longer just clamp player's max speed when they hit the walk key
	else if (bIsWalking && currentspeed > (flGoalSpeed-5) )*/
	if (bIsWalking && currentspeed > (flGoalSpeed-5) )
	{	
		// we now only clamp it when it's within a certain range of walking, otherwise we stop adding speed to let them decelerate naturally
		flStoredAccel *= clamp( 1.0f - ( std::max( 0.0f, currentspeed - ( flGoalSpeed - 5 ) ) / std::max( 0.0f, flGoalSpeed - ( flGoalSpeed - 5 ) ) ), 0.0f, 1.0f );
	}
	
	// Determine amount of acceleration.
	float accelspeed = 0;

	float flCounterSpeed = 0;
	float flMaxAccelspeed = 0;

	if ( flZeroToMaxSpeedTime > 0 )
	{	
		currentspeed = std::max<std::common_type_t<int, float>>( 5, currentspeed );

		float flRawAccelExponent = std::max(1, SV_ACCELERATE_EXPONENT);
		float flAccelExponentTop = (flRawAccelExponent-1);
		flAccelExponentTop *= std::max( 0.0001, 1-(std::max<std::common_type_t<double, float>>( 0, (fAccelerationScale-220) )/(flGoalSpeed-220) ) );

		if ( bIsDucking || bIsWalking )
			flAccelExponentTop *= 0.05;

		float flAccelExponent = flAccelExponentTop + 1;

		float flMoveFracActual = std::pow( currentspeed/flGoalSpeed, 1/flAccelExponent );

		float flLastTimeDelta = gpGlobals->curtime - m_pCSPlayer->m_flGroundAccelLinearFracLastTime;
		if ( currentspeed <= 1 || flLastTimeDelta > 0.1 )
			flLastTimeDelta = 0.015;

		// get friction because we're going to counter it a bit on accelerate
		float flFriction = (sv_friction.GetFloat() * BasePlayer()->m_surfaceFriction);
		float flControl = (currentspeed < sv_stopspeed.GetFloat()) ? sv_stopspeed.GetFloat() : currentspeed;
		flCounterSpeed = flControl*flFriction*gpGlobals->frametime;

		float flTimeSinceStart = (flZeroToMaxSpeedTime * flMoveFracActual);
		float flNewTime = (flTimeSinceStart + flLastTimeDelta);
		float flNewFrac = std::min<std::common_type_t<int, float>>( 1, (flNewTime / flZeroToMaxSpeedTime) );
// 		if ( flNewFrac > 1 )
// 		{
// 			int x = 1;
// 			x++;
// 		}
		float flNewSpeed = pow( flNewFrac, flAccelExponent ) * flGoalSpeed;
		flStoredAccel = flNewSpeed-currentspeed;

		// we're stutter stepping, counter the current speed here
		if ( flUnmodifiedSpeed < -(flMaxSpeed*CS_PLAYER_SPEED_WALK_MODIFIER) )
			flCounterSpeed += -(flUnmodifiedSpeed/3);

		flMaxAccelspeed = accel * gpGlobals->frametime * fAccelerationScale * BasePlayer()->m_surfaceFriction;

		// apply the speed
		accelspeed = std::min( flMaxAccelspeed, (flStoredAccel * gpGlobals->frametime * fAccelerationScale)+flCounterSpeed );
	}
	else
	{
		// apply the speed
		accelspeed = flStoredAccel * gpGlobals->frametime * fAccelerationScale * BasePlayer()->m_surfaceFriction;
	}

// 	if ( accelspeed < 16 && accelspeed > 11 )
// 	{
// 		Msg( "BLAH!!!" );
// 	}

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	mv->m_vecVelocity += (accelspeed * wishdir);

	// store the last time we updated the speed
	m_pCSPlayer->m_flGroundAccelLinearFracLastTime = gpGlobals->curtime;

	if ( sv_accelerate_debug_speed.GetBool() )
		DevMsg( "------- accelspeed = %f, flGoalSpeed = %f, flStoredAccel = %f\n", accelspeed, flGoalSpeed, flStoredAccel );

	//DevMsg( "TRAILING MOVESPEED %f!\n", mv->m_vecTrailingVelocity.AsVector2D().Length() );

	if ( mv->m_vecTrailingVelocity.IsZero() || ( gpGlobals->curtime - mv->m_flTrailingVelocityTime ) > 0.35f )
	{
		// Do a full update
		mv->m_vecTrailingVelocity = mv->m_vecVelocity;
		mv->m_flTrailingVelocityTime = gpGlobals->curtime;
	}
	else
	{
		Vector2D vNormalizedCurrent = mv->m_vecVelocity.AsVector2D();
		Vector2DNormalize( vNormalizedCurrent );

		Vector2D vNormalizedPrev = mv->m_vecTrailingVelocity.AsVector2D();
		Vector2DNormalize( vNormalizedPrev );

		// Check if they're pointed roughly the same direction
		float flDot = vNormalizedCurrent.Dot( vNormalizedPrev );
		if ( flDot > 0.8f )
		{
			// Check if the current has a larger magnitude
			if ( mv->m_vecTrailingVelocity.AsVector2D().LengthSqr() < mv->m_vecVelocity.AsVector2D().LengthSqr() )
			{
				mv->m_vecTrailingVelocity = mv->m_vecVelocity;
				mv->m_flTrailingVelocityTime = gpGlobals->curtime;
			}
		}
		// Check if they're going the opposite direction
		else if ( flDot < -0.8f )
		{
			// Check if the velocity difference is extreme
			if ( mv->m_vecTrailingVelocity.AsVector2D().Length() < 225.0f && mv->m_vecTrailingVelocity.AsVector2D().Length() > 115.0f && mv->m_vecVelocity.AsVector2D().Length() > 115.0f )
			{
				// Check if the player is moving perpendicular
				Vector vEyeForward;
				m_pCSPlayer->EyeVectors( &vEyeForward );

				float flEyeDot = vEyeForward.AsVector2D().Dot( vNormalizedCurrent );
				if ( flEyeDot > -0.3f && flEyeDot < 0.3f )
				{
					CWeaponCSBase *pWeapon = m_pCSPlayer->GetActiveCSWeapon();
					if ( pWeapon )
					{
						mv->m_vecTrailingVelocity = mv->m_vecVelocity;
						mv->m_flTrailingVelocityTime = gpGlobals->curtime;
					}
				}
			}
		}
	}
}
