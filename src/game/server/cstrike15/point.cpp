// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "declare_class.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../baseentity.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "../reflect_datamap.h"
#include "../util.h"
#include "igameevents.h"
#include "server_class.h"
#include "tier2/resourceprecacher.h"
#include "../ai_criteria.h"
#include "../reflect_sendtable.h"
#include "irecipientfilter.h"
#include "reflect_annotations.h"
module ks.server;

import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import ks.core.random;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.pfns;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.cs_player_2;
import ks.core.basetypes;
import ks.core.wchartypes;
import :cstrike15.Effects.chicken;
import :econ.econ_item_schema_minimal;
import ks.core.types;

struct inputdata_t;

using namespace ::ks::reflect;

class CSurroundTest : public CPointEntity
{
public:
	DECLARE_CLASS( CSurroundTest, CPointEntity );

	[[= ks::reflect::Input{ .name = "FireCorrectOutput", .type = FIELD_VOID } ]] void	FireCorrectOutput( inputdata_t &inputdata );
	void	Spawn_Impl( void );

private:
	
	[[= ks::reflect::Key{ .name = "On2Speakers" } ]] COutputEvent m_On2Speakers;
	[[= ks::reflect::Key{ .name = "On4Speakers" } ]] COutputEvent m_On4Speakers;
	[[= ks::reflect::Key{ .name = "On51Speakers" } ]] COutputEvent m_On51Speakers;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_surroundtest, CSurroundTest );

IMPLEMENT_REFLECT_DATAMAP( CSurroundTest )

enum
{
	SND_SURROUND_HEADPHONES = 0,
	SND_SURROUND_2SPEAKERS = 2,
	SND_SURROUND_4SPEAKERS = 4,
	SND_SURROUND_51SPEAKERS, 
};

void CSurroundTest::FireCorrectOutput( inputdata_t &inputdata )
{
	ConVar const *pSurroundCVar = cvar->FindVar( "snd_surround_speakers" );

	if ( pSurroundCVar )
	{
		int iSetting = pSurroundCVar->GetInt();
		
		if ( iSetting == SND_SURROUND_HEADPHONES || iSetting == SND_SURROUND_2SPEAKERS )
		{
			m_On2Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_4SPEAKERS )
		{
			m_On4Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_51SPEAKERS )
		{
			m_On51Speakers.FireOutput( this, this );
		}
	}
}

void CSurroundTest::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
}

class CBaseCombatCharacter;
class CBaseEntity;
class CCSPlayer;
class CCSWeaponInfo;
class CTakeDamageInfo;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_sensorgrenade_thrown.mdl"

LINK_ENTITY_TO_CLASS( tagrenade_projectile, CSensorGrenadeProjectile );
PRECACHE_REGISTER( tagrenade_projectile );

IMPLEMENT_REFLECT_SERVERCLASS( CSensorGrenadeProjectile, DT_SensorGrenadeProjectile )

// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CSensorGrenadeProjectile* CSensorGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile *pGrenade = ( CSensorGrenadeProjectile* )CBaseEntity::Create( "tagrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 2.0f );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );

	pGrenade->m_flDamage = 1.0f; // 25 = 1/4 of HEGrenade Damage
	pGrenade->m_DmgRadius = pGrenade->m_flDamage * 3.5f; // Changing damage will change the radius
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->SetAbsAngles( pOwner->EyeAngles() + QAngle( -80, 40, 0 ) );
	QAngle angRotationVel = QAngle( RandomFloat(100,200), RandomFloat(-100,200), RandomFloat(-100,200) );
	pGrenade->SetLocalAngularVelocity( angRotationVel );
	pGrenade->SetTouch( &CSensorGrenadeProjectile::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->m_pWeaponInfo = &weaponInfo;

	ASSERT(pOwner != nullptr);

	//pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CSensorGrenadeProjectile::SetTimer( float timer )
{
	SetThink( &CSensorGrenadeProjectile::Think_Arm );
	SetNextThink( gpGlobals->curtime + timer );

	m_fNextDetectPlayerSound = gpGlobals->curtime;

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CSensorGrenadeProjectile::Think_Arm( void )
{
	if ( GetAbsVelocity().Length() > 0.2f )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2f );
		return;
	}

	m_fExpireTime = gpGlobals->curtime + 2.0f; // TODO: Make this Data Driven

	SetThink( &CSensorGrenadeProjectile::SensorThink );
	//TheBots->SetGrenadeRadius( this, SensorGrenadeGrenadeRadius );
	SensorThink(); // This will handling the 'Detonate'
}

void CSensorGrenadeProjectile::Think_Remove( void )
{
	UTIL_Remove( this );
}

void CSensorGrenadeProjectile::Detonate( void )
{
	// [mlowrance] The SensorGrenade is handling it's own detonate.
	contract_assert( !"SensorGrenade grenade handles its own detonation" );
}

void CSensorGrenadeProjectile::SensorThink( void )
{
	// tell the bots about the gunfire
 	CCSPlayer *pThrower = ToCSPlayer( GetThrower() );
	if ( !pThrower )
		return;

	if ( gpGlobals->curtime > m_fNextDetectPlayerSound )
	{
		EmitSound( "Sensor.WarmupBeep" );
		m_fNextDetectPlayerSound = gpGlobals->curtime + 1.0f; // TODO: Make this Data Driven
	}

	if ( gpGlobals->curtime < m_fExpireTime )
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		// [mlowrance] Do the damage on Despawn and post event
		CCSPlayer *player = ToCSPlayer( GetThrower() );
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "tagrenade_detonate" );
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

		TheBots->RemoveGrenade( this );

		DispatchParticleEffect( "weapon_sensorgren_detonate", PATTACH_POINT, this, "Wick" );
		EmitSound( "Sensor.Detonate" );	

		DoDetectWave();

		//BaseClass::Detonate();
	}
}

void CSensorGrenadeProjectile::DoDetectWave( void )
{
	// tell the bots about the gunfire
	CCSPlayer *pThrower = ToCSPlayer( GetThrower() );
	if ( !pThrower )
		return;

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( !pPlayer || !pPlayer->IsAlive() || !pThrower->IsOtherEnemy( pPlayer ) )
			continue;

		Vector vDelta = pPlayer->EyePosition() - GetAbsOrigin();
		float flDistance = vDelta.Length();

		float flMaxTraceDist = 1600;
		if ( flDistance <= flMaxTraceDist )
		{
			trace_t tr;
			//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
			{
				if ( TheCSBots()->IsLineBlockedBySmoke( pPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) )
				{
					// if we are outside half the max dist and don't trace, dont show
					if ( flDistance > (flMaxTraceDist/2) )
						continue;
				}

				UTIL_TraceLine( pPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr );
				if ( tr.fraction != 1 )
				{
					trace_t tr2;
					UTIL_TraceLine( pPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
					if ( tr2.fraction != 1 )
					{
						// if we are outside half the max dist and don't trace, dont show
						if ( flDistance > (flMaxTraceDist/2) )
							continue;
					}
				}

				int nThrowerIndex = 0;
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( pPlayer == pThrower )
					{
						nThrowerIndex = i;
						break;
					}
				}

				DebugDrawLine( WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), 90, 0, 0, true, 1.5f );
				pPlayer->SetIsSpotted( true );
				pPlayer->SetIsSpottedBy( nThrowerIndex );
				pPlayer->m_flDetectedByEnemySensorTime = gpGlobals->curtime;
				pPlayer->Blind( 0.02f, 1.0f, 128 );
				pPlayer->EmitSound( "Sensor.WarmupBeep" );
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.25f );
	SetThink( &CSensorGrenadeProjectile::Think_Remove );
}

void CSensorGrenadeProjectile::Spawn_Impl( void )
{
	SetModel( GRENADE_MODEL );
 	BaseClass::Spawn_Impl();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
}

void CSensorGrenadeProjectile::Precache_Impl( void )
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Sensor.Detonate" );
	PrecacheScriptSound( "Sensor.WarmupBeep" );
	PrecacheScriptSound( "Sensor.Activate" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	//PrecacheParticleSystem( "weapon_sen_active" );
	PrecacheParticleSystem( "weapon_sensorgren_detlight" );
	PrecacheParticleSystem( "weapon_sensorgren_detonate" );

	BaseClass::Precache_Impl();
}

void CSensorGrenadeProjectile::BounceTouch( CBaseEntity *other )
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
		SetAbsVelocity( Vector( 0, 0, 0) );
		SetMoveType(MOVETYPE_NONE);
		SetNextThink( gpGlobals->curtime + 1.0f );
		SetThink( &CSensorGrenadeProjectile::Think_Arm );

		EmitSound( "Sensor.Activate" );

		m_fExpireTime = gpGlobals->curtime + 15.0f; // TODO: Make this Data Driven

		// stick the grenade onto the target surface using the closest rotational alignment to match the in-flight orientation,
		// ( like breach charges )

		Vector vecSurfNormal = hitTrace.plane.normal.Normalized();
		Vector vecProjectileZ = EntityToWorldTransform().GetColumn(Z_AXIS);

		// sensor grenades can stick on either of two sides, unlike the breach charges. So they don't need to flip when they land on their 'backs'.
		if ( DotProduct( vecSurfNormal, vecProjectileZ ) < 0 )
			vecSurfNormal = -vecSurfNormal;

		QAngle angSurface;
		MatrixAngles( ConcatTransforms( QuaternionMatrix( RotateBetween( vecProjectileZ, vecSurfNormal ) ), EntityToWorldTransform() ), angSurface );
		SetAbsAngles( angSurface );

		//if ( fabs(hitTrace.plane.normal.Dot(Vector(0,0,1))) > 0.65f )
		{
			//get the player forward vector
// 			Vector vecFlatForward;
// 			VectorCopy( pPlayer->Forward(), vecFlatForward );
// 			vecFlatForward.z = 0;
// 
// 			//derive c4 forward and right
// 			Vector vecC4Right = CrossProduct( vecFlatForward.Normalized(), hitTrace.plane.normal );
// 			Vector vecC4Forward = CrossProduct( vecC4Right, trPlant.plane.normal );

			//QAngle angle;
			//VectorAngles( hitTrace.plane.normal, angle );
			//SetAbsAngles( angle );

			//m_hDisplayGrenade = CreatePhysicsProp( GRENADE_MODEL, GetAbsOrigin(), GetAbsOrigin(), NULL, false, "prop_physics_multiplayer" );

			/*
			m_hDisplayGrenade = ( CBreakableProp * )CBaseEntity::CreateNoSpawn( "prop_physics", GetAbsOrigin(), angle );
			CBreakableProp *pDisplay = dynamic_cast< CBreakableProp* >( m_hDisplayGrenade.Get() );
			if ( pDisplay )
			{
				pDisplay->KeyValue( "fademindist", "-1" );
				pDisplay->KeyValue( "fademaxdist", "0" );
				pDisplay->KeyValue( "fadescale", "1" );
				pDisplay->KeyValue( "inertiaScale", "1.0" );
				pDisplay->KeyValue( "physdamagescale", "0.1" );
				//pDisplay->SetPhysicsMode( PHYSICS_MULTIPLAYER_SOLID );
				pDisplay->SetModel( GRENADE_MODEL );
				pDisplay->SetSolid( SOLID_BBOX );
				pDisplay->AddSolidFlags( FSOLID_NOT_STANDABLE );
				pDisplay->AddSpawnFlags( SF_PHYSPROP_MOTIONDISABLED );
				pDisplay->Precache();
				DispatchSpawn( pDisplay );
				pDisplay->Activate();

				// disable the parent model
				SetModelName( NULL_STRING );//invisible
				SetSolid( SOLID_NONE );
			}
			*/
		}

		/*
		// only detonate on surfaces less steep than this
		const float kMinCos = cosf( DEG2RAD( weapon_molotov_maxdetonateslope.GetFloat() ) );
		if ( hitTrace.plane.normal.z >= kMinCos )
		{
			//Stick();

		}
		*/
	}
}

//TODO: Let physics handle the sound!
void CSensorGrenadeProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

#undef GRENADE_MODEL

class CBaseCombatCharacter;
class CBaseEntity;
class CBaseGrenade;
class CCSPlayer;
class CCSWeaponInfo;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_smokegrenade_thrown.mdl"

LINK_ENTITY_TO_CLASS( smokegrenade_projectile, CSmokeGrenadeProjectile );
PRECACHE_REGISTER( smokegrenade_projectile );

IMPLEMENT_REFLECT_SERVERCLASS( CSmokeGrenadeProjectile, DT_SmokeGrenadeProjectile )

CSmokeGrenadeProjectile* CSmokeGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CSmokeGrenadeProjectile *pGrenade = (CSmokeGrenadeProjectile*)CBaseEntity::Create( "smokegrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 1.5 );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );
	pGrenade->SetGravity( 0.55 );
	pGrenade->SetFriction( 0.7 );
	pGrenade->m_flDamage = 100;
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );	
	pGrenade->SetTouch( &CBaseGrenade::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );
	pGrenade->m_bDidSmokeEffect = false;
	pGrenade->m_nSmokeEffectTickBegin = 0;
	pGrenade->m_flLastBounce = 0;
	pGrenade->m_vSmokeColor = weaponInfo.GetSmokeColor();
	pGrenade->m_pWeaponInfo = &weaponInfo;

	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CSmokeGrenadeProjectile::SetTimer( float timer )
{
	SetThink( &CSmokeGrenadeProjectile::Think_Detonate );
	SetNextThink( gpGlobals->curtime + timer );

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CSmokeGrenadeProjectile::Think_Detonate()
{
	if ( GetAbsVelocity().Length() > 0.1 )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2 );
		return;
	}

	SmokeDetonate();
}

void CSmokeGrenadeProjectile::SmokeDetonate( void )
{
	TheBots->SetGrenadeRadius( this, SmokeGrenadeRadius );

	// Ok, we've stopped rolling or whatever. Now detonate.

	// Make sure all players get the message about this smoke effect.
	// This fixes an exploit where a player could enter a room where others were seeing smoke and he wasn't
	// because he wasn't in the PVS when the smoke effect started.
	m_nSmokeEffectTickBegin = gpGlobals->tickcount; // client will star the explosion_smokegrenade particle effect at AbsOrigin

	//tell the hostages about the smoke!
	CBaseEntity *pEntity = nullptr;
	variant_t var;	//send the location of the smoke?
	var.SetVector3D( GetAbsOrigin() );
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "hostage_entity" ) ) != nullptr)
	{
		//send to hostages that have a resonable chance of being in it while its still smoking
		if( (GetAbsOrigin() - pEntity->GetAbsOrigin()).Length() < 1000 )
			pEntity->AcceptInput( "smokegrenade", this, this, var, 0 );
	}

	// tell the bots a smoke grenade has exploded
	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "smokegrenade_detonate" );
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

	m_bDidSmokeEffect = true; //<- the old way to signal the start of smoke effect; the new way is to set the particle start tick, so that we can replay and fix the bug when we lose the smoke effect when we connect right after smoke grenade went off

	EmitSound( "BaseSmokeEffect.Sound" );

	m_nRenderMode = kRenderTransColor;

	SetMoveType(MOVETYPE_NONE);
	SetNextThink( gpGlobals->curtime + 12.5f );
	SetThink( &CSmokeGrenadeProjectile::Think_Fade );
	
	SetSolid( SOLID_NONE );
}

void CSmokeGrenadeProjectile::RemoveGrenadeFromLists( void )
{
	TheBots->RemoveGrenade( this );
	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );

	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "smokegrenade_expired" );
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

}

// Fade the projectile out over time before making it disappear
void CSmokeGrenadeProjectile::Think_Fade()
{
	SetNextThink( gpGlobals->curtime );

	byte a = GetRenderAlpha();
	a -= 1;
	SetRenderAlpha( a );

	if ( !a )
	{
		//RemoveGrenadeFromLists();
		SetNextThink( gpGlobals->curtime + 1.0 );
		SetThink( &CSmokeGrenadeProjectile::Think_Remove );	// Spit out smoke for 10 seconds.
	}
}

void CSmokeGrenadeProjectile::Think_Remove()
{
	RemoveGrenadeFromLists();
	SetMoveType( MOVETYPE_NONE );
	UTIL_Remove( this );
}

//Implement this so we never call the base class,
//but this should never be called either.
void CSmokeGrenadeProjectile::Detonate( void )
{
	contract_assert( !"Smoke grenade handles its own detonation" );
}

void CSmokeGrenadeProjectile::Spawn_Impl()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn_Impl();

	SetBodygroupPreset( "thrown" );
}

void CSmokeGrenadeProjectile::Precache_Impl()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "BaseSmokeEffect.Sound" );
	PrecacheScriptSound( "SmokeGrenade.Bounce" );
	BaseClass::Precache_Impl();
}

void CSmokeGrenadeProjectile::OnBounced( void )
{
	if ( m_flLastBounce >= ( gpGlobals->curtime - 3*gpGlobals->interval_per_tick ) )
		return;

	m_flLastBounce = gpGlobals->curtime;

	//
	// if the smoke grenade is above ground, trace down to the ground and see where it would end up?
	//
	Vector posDropSmoke = GetAbsOrigin();
	trace_t trSmokeTrace;
	UTIL_TraceLine( posDropSmoke, posDropSmoke - Vector( 0, 0, SmokeGrenadeRadius ), ( MASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ),
		this, COLLISION_GROUP_PROJECTILE, &trSmokeTrace );
	if ( !trSmokeTrace.startsolid )
	{
		if ( trSmokeTrace.fraction >= 1.0f )
			return;	// this smoke cannot drop enough to cause extinguish

		if ( trSmokeTrace.fraction > 0.001f )
			posDropSmoke = trSmokeTrace.endpos;
	}

	//
	// See if it touches any inferno?
	//
	const int maxEnts = 64;
	CBaseEntity *list[ maxEnts ];
	int count = UTIL_EntitiesInSphere( list, maxEnts, GetAbsOrigin(), 512, FL_ONFIRE );
	for( int i=0; i<count; ++i )
	{
		if (list[i] == nullptr || list[i] == this)
			continue;

		CInferno* pInferno = dynamic_cast<CInferno*>( list[i] );

		if ( pInferno && pInferno->BShouldExtinguishSmokeGrenadeBounce( this, posDropSmoke ) )
		{
			if ( posDropSmoke != GetAbsOrigin() )
			{
				const QAngle qAngOriginZero = vec3_angle;
				const Vector vVelocityZero = vec3_origin;
				Teleport( &posDropSmoke, &qAngOriginZero, &vVelocityZero );
			}

			SmokeDetonate();
			break;
		}
	}
}

void CSmokeGrenadeProjectile::BounceSound( void )
{
	if ( !m_bDidSmokeEffect )
	{
		EmitSound( "SmokeGrenade.Bounce" );
	}
}

#undef GRENADE_MODEL

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Dispatches blood stream tempentity
//-----------------------------------------------------------------------------
class CTERadioIcon : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TERadioIcon" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTERadioIcon, CBaseTempEntity );

					CTERadioIcon( const char *name );
	virtual			~CTERadioIcon( void );

	void Precache( void );
	
	DECLARE_SERVERCLASS();

public:

	CNetworkVar( int, m_iAttachToClient, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTERadioIcon::CTERadioIcon( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTERadioIcon::~CTERadioIcon( void )
{
}

void CTERadioIcon::Precache( void )
{
	CBaseEntity::PrecacheModel("sprites/radio.vmt");
}

IMPLEMENT_REFLECT_SERVERCLASS( CTERadioIcon, DT_TERadioIcon )

// Singleton to fire StickyBolt objects
static CTERadioIcon g_TERadioIcon( "RadioIcon" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			pPlayer - 
//-----------------------------------------------------------------------------
void TE_RadioIcon( IRecipientFilter& filter, float delay, CBaseEntity *pPlayer )
{
	g_TERadioIcon.m_iAttachToClient = pPlayer->entindex();
	
	// Send it over the wire
	g_TERadioIcon.Create( filter, delay );
}

class CBaseTempEntity;

using namespace ::ks::reflect;

static constexpr auto NUM_BULLET_SEED_BITS = 8;

//-----------------------------------------------------------------------------
// Purpose: Display's a blood sprite
//-----------------------------------------------------------------------------
class CTEFireBullets : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFireBullets", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFireBullets, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEFireBullets( const char *name );
	virtual			~CTEFireBullets( void );

public:
	CNetworkVar( int, m_iPlayer, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint16, m_nItemDefIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkQAngle( m_vecAngles, [[= ks::reflect::Net{ .bits = 13, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 0 } ]] );
	CNetworkVar( int, m_iWeaponID, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iMode, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iSeed, [[= ks::reflect::Net{ .bits = NUM_BULLET_SEED_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fInaccuracy, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1 } ]] );
	CNetworkVar( float, m_flRecoilIndex, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1000 } ]] );
	CNetworkVar( float, m_fSpread, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 0.1f } ]] );
	CNetworkVar( int, m_iSoundType, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEFireBullets::CTEFireBullets( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFireBullets::~CTEFireBullets( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEFireBullets, DT_TEFireBullets )

// Singleton
static CTEFireBullets g_TEFireBullets( "Shotgun Shot" );

void TE_FireBullets( 
	int	iPlayerIndex,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	int iSoundType,
	float flRecoilIndex
	)
{
	// Just always send gunshots to clients.
	CBroadcastRecipientFilter filter;
	filter.UsePredictionRules();

	g_TEFireBullets.m_iPlayer = iPlayerIndex-1;
	g_TEFireBullets.m_nItemDefIndex = nItemDefIndex;
	g_TEFireBullets.m_vecOrigin = vOrigin;
	g_TEFireBullets.m_vecAngles = vAngles;
	g_TEFireBullets.m_iSeed = iSeed;
	g_TEFireBullets.m_fInaccuracy = fInaccuracy;
	g_TEFireBullets.m_flRecoilIndex = flRecoilIndex;
	g_TEFireBullets.m_fSpread = fSpread;
	g_TEFireBullets.m_iMode = iMode;
	g_TEFireBullets.m_iWeaponID = iWeaponID;
	g_TEFireBullets.m_iSoundType = iSoundType;

	contract_assert( iSeed < (1 << NUM_BULLET_SEED_BITS) );
	
	g_TEFireBullets.Create( filter, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Displays a bomb plant animation
//-----------------------------------------------------------------------------
class CTEPlantBomb : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPlantBomb", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPlantBomb, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEPlantBomb( const char *name );
	virtual			~CTEPlantBomb( void );

public:
	CNetworkVar( int, m_iPlayer, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( PlantBombOption_t, m_option, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPlantBomb::CTEPlantBomb( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlantBomb::~CTEPlantBomb( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEPlantBomb, DT_TEPlantBomb )

// Singleton
static CTEPlantBomb g_TEPlantBomb( "Bomb Plant" );

void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
	CPASFilter filter( vOrigin );
	filter.UsePredictionRules();

	g_TEPlantBomb.m_iPlayer = iPlayerIndex-1;
	g_TEPlantBomb.m_option = option;
	g_TEPlantBomb.Create( filter, 0 );
}
