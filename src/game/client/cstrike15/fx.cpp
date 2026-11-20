// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "tier2/resourceprecacher.h"
#include "../te_effect_dispatch.h"
#include "edict.h"
#include "iclientrenderable.h"
#include "vphysics_interface.h"
#include "const.h"
#include "irecipientfilter.h"
#include "soundflags.h"
#include "client_class.h"
#include "../predictable_entity.h"
#include "../reflect_predmap.h"
#include "../reflect_recvtable.h"
#include "engine/IEngineSound.h"
#include "rope_shared.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "igameevents.h"
#include "iclientnetworkable.h"
#include "materialsystem/imesh.h"
#include "declare_class.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.intrusive_ptr;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :particlemgr;
import :precache;
import :baseentity;
import ks.game.shareddefs;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import :noopanimcontroller;
import ks.core.color;
import ks.core.random;
import :achievements_and_stats_interface;
import ks.game.mapentities_shared;
import ks.core.dbg;
import ks.core.platform;
import ks.game.iplayeranimstate;
import ks.game.rumble_shared;
import ks.tier1.strtools;
import :cstrike15.cs_shareddefs;
import :econ.econ_item_schema_minimal;
import :cstrike15.Effects.clientinferno;
import ks.game.ai_activity;
import ks.tier1.vector_ops;
import ks.math.ssemath;

class CEffectData;
class SimpleParticle;

using namespace ::ks::reflect;

// Purpose: A blood spray effect to expose successful hits.

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectCSBloodSpray )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_basic" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_heavy" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_medium" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_light" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_light_headshot" )
PRECACHE_REGISTER_END()

static ConVar damage_impact_heavy( side_registry(), "damage_impact_heavy", "40", FCVAR_NONE, "Damage ABOVE this value is considered heavy damage" );
static ConVar damage_impact_medium( side_registry(), "damage_impact_medium", "20", FCVAR_NONE, "Damage BELOW this value is considered light damage" );

class CHitEffectRamp
{
public:
	float m_flDamageAmount;
	
	float m_flMinAlpha;
	float m_flMaxAlpha;

	float m_flMinSize;
	float m_flMaxSize;

	float m_flMinVelocity;
	float m_flMaxVelocity;
};

void InterpolateRamp( 
	const CHitEffectRamp &a,
	const CHitEffectRamp &b,
	CHitEffectRamp &out,
	int iDamage )
{
	float t = RemapVal( iDamage, a.m_flDamageAmount, b.m_flDamageAmount, 0, 1 );

	out.m_flMinAlpha = FLerp( a.m_flMinAlpha, b.m_flMinAlpha, t );
	out.m_flMaxAlpha = FLerp( a.m_flMaxAlpha, b.m_flMaxAlpha, t );
	out.m_flMinAlpha = clamp( out.m_flMinAlpha, 0, 255 );
	out.m_flMaxAlpha = clamp( out.m_flMaxAlpha, 0, 255 );

	out.m_flMinSize = FLerp( a.m_flMinSize, b.m_flMinSize, t );
	out.m_flMaxSize = FLerp( a.m_flMaxSize, b.m_flMaxSize, t );

	out.m_flMinVelocity = FLerp( a.m_flMinVelocity, b.m_flMinVelocity, t );
	out.m_flMaxVelocity = FLerp( a.m_flMaxVelocity, b.m_flMaxVelocity, t );
}

void FX_HitEffectSmoke(
	ks::intrusive_ptr<CBloodSprayEmitter> pEmitter, 
	int iDamage,
	const Vector &vEntryPoint,
	const Vector &vDirection,
	float flScale)
{
	SimpleParticle newParticle;
	PMaterialHandle hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_gore" );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			30,		// min/max alpha
			70, 
			0.5,	// min/max size
			1,
			0,		// min/max velocity (not used here)
			0
		},
		
		{ 
			50, 
			30,		// min/max alpha 
			70,
			1,		// min/max size
			2,
			0,		// min/max velocity (not used here)
			0
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );

	for ( int i=0; i < 2; i++ )
	{
		SimpleParticle &newParticle = *pEmitter->AddSimpleParticle( hMaterial, vEntryPoint, 0, 0 );

		newParticle.m_flLifetime = 0.0f;
		newParticle.m_flDieTime	= 3.0f;

		newParticle.m_uchStartSize	= random_valve->RandomInt( 
			interpolatedRamp.m_flMinSize, 
			interpolatedRamp.m_flMaxSize ) * flScale;
		newParticle.m_uchEndSize	= newParticle.m_uchStartSize * 4;
		
		newParticle.m_vecVelocity = Vector( 0, 0, 5 ) + RandomVector( -2, 2 );
		newParticle.m_uchStartAlpha	= random_valve->RandomInt( 
			interpolatedRamp.m_flMinSize,
			interpolatedRamp.m_flMaxSize );
		newParticle.m_uchEndAlpha	= 0;

		newParticle.m_flRoll			= random_valve->RandomFloat( 0, 360 );
		newParticle.m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		newParticle.m_iFlags = SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY;

		float colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

		newParticle.m_uchColor[0] = std::min( 1.0f, colorRamp ) * 255.0f;
		newParticle.m_uchColor[1] = std::min( 1.0f, colorRamp ) * 255.0f;
		newParticle.m_uchColor[2] = std::min( 1.0f, colorRamp ) * 255.0f;
	}	
}

void FX_HitEffectBloodSpray(
	ks::intrusive_ptr<CBloodSprayEmitter> pEmitter, 
	int iDamage,
	const Vector &vEntryPoint, 
	const Vector &vSprayNormal,
	const char *pMaterialName,
	float flLODDistance,
	float flDistanceScale,
	float flScale,
	float flSpeed )
{
	PMaterialHandle hMaterial = ParticleMgr()->GetPMaterial( pMaterialName );
	SimpleParticle *pParticle;

	float color[3] = { 1.0, 0, 0 };

	Vector up( 0, 0, 1 );
	Vector right = up.Cross( vSprayNormal );
	VectorNormalize( right );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			80,		// min/max alpha
			128, 
			flScale/2,// min/max size
			flScale,
			10,		// min/max velocity
			20
		},
		
		{ 
			50, 
			80,		// min/max alpha 
			128,
			flScale/2,// min/max size
			flScale,
			30,		// min/max velocity
			60
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );

	for ( int i = 0; i < 6; i++ )
	{
		// Originate from within a circle '2 * scale' inches in diameter.
		Vector offset = vEntryPoint + ( flScale * vSprayNormal * 0.5 );
		offset += right * random_valve->RandomFloat( -1, 1 ) * flScale;
		offset += up * random_valve->RandomFloat( -1, 1 ) * flScale;

		pParticle = pEmitter->AddSimpleParticle( hMaterial, offset, 0, 0 );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.7f, 1.3f);

			// All the particles are between red and white. The whiter the particle is, the slower it goes.
			float whiteness = random_valve->RandomFloat( 0.1, 0.7 );
			float speedFactor = 1 - whiteness;

			float spread = 0.5f;
			pParticle->m_vecVelocity.Random( -spread, spread );
			pParticle->m_vecVelocity += vSprayNormal * random_valve->RandomInt( interpolatedRamp.m_flMinVelocity, interpolatedRamp.m_flMaxVelocity ) * flSpeed * speedFactor;

			float colorRamp = random_valve->RandomFloat( 0.5f, 0.75f ) + flLODDistance;
			
			pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[1]	= std::min( 1.0f, whiteness * colorRamp ) * 255.0f;
			pParticle->m_uchColor[2]	= std::min( 1.0f, whiteness * colorRamp ) * 255.0f;
			
			pParticle->m_uchStartSize	= random_valve->RandomFloat( interpolatedRamp.m_flMinSize, interpolatedRamp.m_flMaxSize ) * flDistanceScale;
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4 * flDistanceScale;
			
			pParticle->m_uchStartAlpha	= random_valve->RandomInt( interpolatedRamp.m_flMinAlpha, interpolatedRamp.m_flMaxAlpha );
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -4.0f, 4.0f );
		}
	}
}

void FX_HitEffectBloodSplatter(
	ks::intrusive_ptr<CBloodSprayEmitter> pTrailEmitter, 
	int iDamage,
	const Vector &vExitPoint, 
	const Vector &vSplatterNormal,
	float flLODDistance )
{
	float flScale = 4;

	pTrailEmitter->SetSortOrigin( vExitPoint );
	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_drop" );

	Vector up( 0, 0, 1 );
	Vector right = up.Cross( vSplatterNormal );
	VectorNormalize( right );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			0,		// min/max alpha
			75, 
			1.5f,  // min/max size
			2.0f,
			25.0f * flScale,		// min/max velocity
			35.0f * flScale
		},
		
		{ 
			50, 
			0,		// min/max alpha 
			140,
			1.5f,// min/max size
			2.0f,
			65.0f * flScale,		// min/max velocity
			75.0f * flScale
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );

	for ( int i = 0; i < 20; i++ )
	{
		// Originate from within a circle 'scale' inches in diameter.
		Vector offset = vExitPoint;
		offset += right * random_valve->RandomFloat( -0.15f, 0.15f ) * flScale;
		offset += up * random_valve->RandomFloat( -0.15f, 0.15f ) * flScale;

		SimpleParticle *tParticle = (SimpleParticle*)pTrailEmitter->AddSimpleParticle( 
			hMaterial, 
			vExitPoint,
			random_valve->RandomFloat( 0.225f, 0.35f ),
			random_valve->RandomFloat( interpolatedRamp.m_flMinSize, interpolatedRamp.m_flMaxSize )
			);

		if ( tParticle == nullptr )
			break;

		Vector offDir = vSplatterNormal + RandomVector( -0.05f, 0.05f );

		tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( interpolatedRamp.m_flMinVelocity, interpolatedRamp.m_flMaxVelocity );

		tParticle->m_iFlags = SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY;
		tParticle->m_uchColor[0]	= 150;
		tParticle->m_uchColor[1]	= 0;
		tParticle->m_uchColor[2]	= 0;
		tParticle->m_uchStartAlpha = interpolatedRamp.m_flMaxAlpha / 2;
		tParticle->m_uchEndAlpha = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_CS_BloodSpray( const Vector &origin, const Vector &normal, float flDamage )
{
	if ( UTIL_IsLowViolence() )
		return;

	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	// Use the new particle system
	Vector dir = normal;/* * RandomVector( -0.05f, 0.05f )*/;
	Vector offset = origin + ( normal );

	QAngle vecAngles;
	VectorAngles( dir, vecAngles );

	const char *pEffectName;
	if ( flDamage > damage_impact_heavy.GetInt() )
	{
	      pEffectName = "blood_impact_heavy";
	}
	else if ( flDamage >= damage_impact_medium.GetInt()  )
	{
		pEffectName = "blood_impact_medium";
	}
	else if ( flDamage > 1 )
	{
		pEffectName = "blood_impact_light";
	}
	else
	{
		pEffectName = "blood_impact_light_headshot";
	}

	DispatchParticleEffect( pEffectName, offset, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bloodtype - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void GetBloodColorForTeam( int iTeam, unsigned char &r, unsigned char &g, unsigned char &b )
{

}

//-----------------------------------------------------------------------------
// Purpose: Intercepts the blood spray message.
//-----------------------------------------------------------------------------
void CSBloodSprayCallback( const CEffectData &data )
{
	FX_CS_BloodSpray( data.m_vOrigin, data.m_vNormal, data.m_flMagnitude );
}

DECLARE_CLIENT_EFFECT( csblood, CSBloodSprayCallback );

class CEffectData;
class CCSPlayer;
class CLocalTempEntity;

using namespace ::ks::reflect;

// Purpose: Game-specific impact effect hooks

extern int		g_sModelXmasBulb;	// holds the sprite index for splattered blood

// Used for cycling so that they're sequencially ordered on each strand
int g_nHolidayLightColor = 0;

// 4 light colors
Color g_rgbaHolidayRed( 255, 0, 0, 255 );
Color g_rgbaHolidayYellow( 2, 110, 197, 255 );
Color g_rgbaHolidayGreen( 117, 193, 8, 255 );
Color g_rgbaHolidayBlue( 255, 151, 29, 255 );

Color g_rgbaHalloweenOrange( 239, 136, 15, 255 );
Color g_rgbaHalloweenPurple( 128, 46, 204, 255 );

Color *(rgbaHolidayLightColors[]) = { &g_rgbaHolidayRed, 
									  &g_rgbaHolidayYellow, 
									  &g_rgbaHolidayGreen, 
									  &g_rgbaHolidayBlue };

Color *( rgbaHalloweenLightColors[] ) = { &g_rgbaHalloweenOrange,
										&g_rgbaHalloweenPurple };

struct HolidayLightData_t
{
	Vector vOrigin;
	int nID;
	int nSubID;
	float fScale;
};

void CreateHolidayLight( const HolidayLightData_t &holidayLight );

class CHolidayLightManager : public CAutoGameSystemPerFrame
{
public:
	explicit CHolidayLightManager( char const *name );

	// Methods of IGameSystem
	virtual void Update( float frametime );

	virtual void LevelInitPostEntity( void );
	virtual void LevelShutdownPreEntity();

	void AddHolidayLight( const CEffectData &data );

private:

	std::vector< HolidayLightData_t > m_PendingLightData;
};

CHolidayLightManager g_CHolidayLightManager( "CHolidayLightManager" );

CHolidayLightManager::CHolidayLightManager( char const *name ) : CAutoGameSystemPerFrame( name )
{
}

// Methods of IGameSystem
void CHolidayLightManager::Update( float frametime )
{
	for ( int i = 0; i < std::ssize( m_PendingLightData ); ++i )
	{
		CreateHolidayLight( m_PendingLightData[ i ] );
	}

	m_PendingLightData.clear();
}

void CHolidayLightManager::LevelInitPostEntity( void )
{
	m_PendingLightData.clear();
}

void CHolidayLightManager::LevelShutdownPreEntity()
{
	m_PendingLightData.clear();
}

void CHolidayLightManager::AddHolidayLight( const CEffectData &data )
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	Vector vecPlayerPos = pPlayer->GetNetworkOrigin();
	// Too far away?
	if ( vecPlayerPos.DistTo( data.m_vOrigin ) > 2000.0f )
		return;

	// In the skybox?
	sky3dparams_t *pSky = &(pPlayer->m_Local.m_skybox3d);
	if ( pSky->origin->DistTo( data.m_vOrigin ) < 2000.0f )
		return;

	HolidayLightData_t newData;

	newData.vOrigin = data.m_vOrigin;

	// HACK: Use these ints to ID the light later
	newData.nID = data.m_nMaterial;
	newData.nSubID = data.m_nHitBox;

	// Skybox lights pass in a smaller scale
	newData.fScale = data.m_flScale;

	if ( std::ssize( m_PendingLightData ) < CTempEnts::MAX_TEMP_ENTITIES / 2 )
	{
		m_PendingLightData.push_back( newData );
	}
}

void CS_HolidayLightCallback( const CEffectData &data )
{
	g_CHolidayLightManager.AddHolidayLight( data );
}

inline constexpr const char *XMAS_LIGHT_MODEL = "models/props/holiday_light/holiday_light.mdl";
void CreateHolidayLight( const HolidayLightData_t &holidayLight )
{
	int nHolidayLightStyle = RopeManager()->GetHolidayLightStyle();

	model_t *pModel = ( nHolidayLightStyle == 0 ? (model_t *)engine->LoadModel( "effects/christmas_bulb.vmt" ) : (model_t *)engine->LoadModel( XMAS_LIGHT_MODEL ) );
	if ( !pModel )
		return;

	contract_assert( pModel );

	CLocalTempEntity *pTemp = tempents->FindTempEntByID( holidayLight.nID, holidayLight.nSubID );

	if ( !pTemp )
	{
		// Didn't find one with that ID, so make a new one!
		// Randomize the angle
		QAngle angOrientation = ( nHolidayLightStyle == 0 ? QAngle( 0.0f, 0.0f, RandomFloat( -180.0f, 180.0f ) ) : vec3_angle );
		//pTemp = tempents->SpawnTempModel( pModel, holidayLight.vOrigin, angOrientation, vec3_origin, 2.0f, FTENT_NEVERDIE );
		pTemp = tempents->SpawnTempModel( pModel, holidayLight.vOrigin, angOrientation, vec3_origin, 2.0f, FTENT_NONE );

		if ( !pTemp )
		{
			return;
		}

		pTemp->clientIndex = 0;

		// HACK: Use these ints to ID the light later
		pTemp->m_nSkin = holidayLight.nID;
		pTemp->hitSound = holidayLight.nSubID;

		pTemp->SetDistanceFade( 1024, 1200 );

		// Skybox lights pass in a smaller scale
		pTemp->m_flSpriteScale = holidayLight.fScale;

		// Smuggle the color index here
		if ( nHolidayLightStyle == 0 )
		{
			pTemp->m_nHitboxSet = g_nHolidayLightColor;

			//Set the color
			{	
				pTemp->SetRenderColor( rgbaHolidayLightColors[ g_nHolidayLightColor ]->r(), 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->g(), 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->b()/*, 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->a() */);
			}
		}
		
		pTemp->SetRenderMode( kRenderTransColor );

		pTemp->SetProxyRandomValue( RandomFloat() );

		// Next color in the pattern
		g_nHolidayLightColor = ( g_nHolidayLightColor + 1 ) % std::size( rgbaHolidayLightColors );
	}
	else
	{
		// Update the position
		pTemp->SetAbsOrigin( holidayLight.vOrigin );

		// Every 10 light strands have a blink cycle
		if ( nHolidayLightStyle == 0 && pTemp->m_nSkin % 5 == 0 )
		{
			// Magic! Basically this makes the on/off cycle of each color different and offsets it by the segment index.
			// That way it looks like a timed pattern but is also chaotic.
			int nCycle = ( pTemp->hitSound + static_cast< int >( gpGlobals->curtime * 2.0f ) ) % ( pTemp->m_nHitboxSet + std::size( rgbaHolidayLightColors ) + 1 );
			pTemp->SetRenderAlpha( nCycle < std::size( rgbaHolidayLightColors ) ? 255 : 64 );
		}

		// Update the scale
		pTemp->m_flSpriteScale = holidayLight.fScale;

		Vector vecMin, vecMax;
		pTemp->GetRenderBounds( vecMin, vecMax );

		// Extend it's life
		pTemp->die = gpGlobals->curtime + 2.0f;
	}
}

DECLARE_CLIENT_EFFECT( CS_HolidayLight, CS_HolidayLightCallback );

void RopesHolidayLightColor( const CCommand &args )
{
	if ( args.ArgC() < 5 )
		return;

	int nLight = atoi( args[ 1 ] );

	{
		if ( nLight < 0 || nLight >= std::size( rgbaHolidayLightColors ) )
			return;

		rgbaHolidayLightColors[nLight]->SetColor( atoi( args[2] ), atoi( args[3] ), atoi( args[4] ) );
	}
}
ConCommand r_ropes_holiday_light_color( side_registry(), "r_ropes_holiday_light_color", RopesHolidayLightColor, "Set each light's color: [light0-3] [r0-255] [g0-255] [b0-255]", FCVAR_NONE );

class CEffectData;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Game-specific impact effect hooks

//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;

	CBaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity || pEntity->IsDormant())
		return;

	int flags = 0;
	if ( (iDamageType & DMG_SHOCK) != 0 ) // no decals for shock damage
	{
		flags = IMPACT_NODECAL;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, flags ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( Impact, ImpactCallback );

class CEffectData;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Game-specific impact effect hooks

void KnifeSlash( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;

	CBaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if( pEntity == nullptr )
		return;

	int decalNumber = decalsystem->GetDecalIndexForName( GetImpactDecal( pEntity, iMaterial, iDamageType ) );
	if ( decalNumber == -1 )
		return;

	// vector perpendicular to the slash direction
	// so we can align the slash decal to that
	Vector vecPerp;
	AngleVectors( data.m_vAngles, nullptr, &vecPerp, nullptr );

	const ConVar *decals =cvar->FindVar( "r_decals" );

	if ( decals && decals->GetInt() )
	{
		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			// Setup our shot information
			Vector shotDir = vecOrigin - vecStart;
			float flLength = VectorNormalize( shotDir );
			Vector traceExt;
			VectorMA( vecStart, flLength + 8.0f, shotDir, traceExt );

			// Special case for world entity with hitbox (that's a static prop):
			// In this case, we've hit a static prop. Decal it!
			staticpropmgr->AddDecalToStaticProp( vecStart, traceExt, iHitbox - 1, decalNumber, true, tr );
		}
		else
		{
			effects->DecalShoot( decalNumber,
						pEntity->entindex(),
						pEntity->GetModel(),
						pEntity->GetAbsOrigin(),
						pEntity->GetAbsAngles(),
						vecOrigin,
						&vecPerp,
						0 );
		}
		
	}

	if( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, data.m_fFlags ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );	
	}
}

DECLARE_CLIENT_EFFECT( KnifeSlash,		KnifeSlash );

class CEffectData;
class CBaseEntity;
class SimpleParticle;

using namespace ::ks::reflect;

// Precache our effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffect_CS_MuzzleFlash )
	PRECACHE( MATERIAL, "effects/muzzleflashX" )	//.vmt
	PRECACHE( MATERIAL, "sprites/muzzleflash4" )	//.vmt
PRECACHE_REGISTER_END()

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

void CS_MuzzleFlashCallback( const CEffectData &data )
{
	ks::intrusive_ptr<CLocalSpaceEmitter> pEmitter = 
		CLocalSpaceEmitter::Create( "CS_MuzzleFlash", data.m_hEntity, data.m_nAttachmentIndex, 0 );

	if ( !pEmitter )
		return;

	// SetBBox() manually on the particle system so it doesn't have to be recalculated more than once.
	Vector vCenter( 0.0f, 0.0f, 0.0f );
	CBaseEntity *pEnt = data.GetEntity();
	if ( pEnt )
	{
		vCenter = pEnt->WorldSpaceCenter();
	}
	else
	{
		IClientRenderable *pRenderable = data.GetRenderable( );
		if ( pRenderable )
		{
			Vector vecMins, vecMaxs;
			pRenderable->GetRenderBoundsWorldspace( vecMins, vecMaxs );
			VectorAdd( vecMins, vecMaxs, vCenter );
			vCenter *= 0.5f;
		}
	}

	contract_assert( pEmitter );
	pEmitter->GetBinding().SetBBox( vCenter - Vector( 3, 3, 3 ), vCenter + Vector( 3, 3, 3 ) );

	// haxors - make the clip much shorter so the alpha is not 
	// changed based on large clip distances
	pEmitter->SetNearClip( 0, 5 );

	PMaterialHandle hFlashMaterial = pEmitter->GetPMaterial( "sprites/muzzleflash4" );

	for( int i=0;i<3;i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *)pEmitter->AddParticle( sizeof( SimpleParticle ),
															hFlashMaterial,
															vec3_origin );
		if( pParticle )
		{
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= 0.08f;

			pParticle->m_vecVelocity = vec3_origin;

			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;

			pParticle->m_uchStartAlpha	= 80;
			pParticle->m_uchEndAlpha	= 30;

			pParticle->m_uchStartSize	= ( 3.0 + 3.0*i ) * data.m_flScale;

			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 0.8;
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 3 );

			pParticle->m_flRollDelta	= 0.0f;
		}
	}

	// dynamic light temporary entity for the muzzle flash
	CPVSFilter filter(pEmitter->GetSortOrigin());
	TE_DynamicLight( filter, 0.0, &(pEmitter->GetSortOrigin()), 255, 192, 64, 5, 70, 0.05, 768 );
}

DECLARE_CLIENT_EFFECT( CS_MuzzleFlash, CS_MuzzleFlashCallback );

// 'X' shaped muzzleflash used by certain weapons
void CS_MuzzleFlashXCallback( const CEffectData &data )
{
	ks::intrusive_ptr<CLocalSpaceEmitter> pEmitter = 
		CLocalSpaceEmitter::Create( "CS_MuzzleFlashX", data.m_hEntity, data.m_nAttachmentIndex, 0 );

	// SetBBox() manually on the particle system so it doesn't have to be recalculated more than once.
	CBaseEntity *pEnt = data.GetEntity();
	if ( pEnt )
	{
		Vector vCenter = pEnt->WorldSpaceCenter();
		pEmitter->GetBinding().SetBBox( vCenter - Vector( 3, 3, 3 ), vCenter + Vector( 3, 3, 3 ) );
	}	

	contract_assert( pEmitter );

	// haxors - make the clip much shorter so the alpha is not 
	// changed based on large clip distances
	pEmitter->SetNearClip( 0, 5 );

	PMaterialHandle hFlashMaterial = pEmitter->GetPMaterial( "effects/muzzleflashX" );

	SimpleParticle *pParticle = (SimpleParticle *)pEmitter->AddParticle( sizeof( SimpleParticle ),
														hFlashMaterial,
														vec3_origin );
	if( pParticle )
	{
		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.08f;

		pParticle->m_vecVelocity = vec3_origin;

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 130;
		pParticle->m_uchEndAlpha	= 80;

		pParticle->m_uchStartSize	= 6.0f * data.m_flScale * random_valve->RandomFloat( 0.9, 1.1 );

		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 0.8;

		pParticle->m_flRoll			= random_valve->RandomFloat( -0.25, 0.25 );

		pParticle->m_flRollDelta	= 0.0f;
	}

	// dynamic light temporary entity for the muzzle flash
	CPVSFilter filter(pEmitter->GetSortOrigin());
	TE_DynamicLight( filter, 0.0, &(pEmitter->GetSortOrigin()), 255, 192, 64, 5, 70, 0.05, 768 );
}

DECLARE_CLIENT_EFFECT( CS_MuzzleFlash_X, CS_MuzzleFlashXCallback );

class CCSWeaponInfo;
class CEconItemDefinition;
class CBaseEntity;
class CCSPlayer;
class CEconItemView;
class CWeaponCSBase;

using namespace ::ks::reflect;

ConVar weapon_accuracy_logging( side_registry(), "weapon_accuracy_logging", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE );
ConVar weapon_near_empty_sound( side_registry(), "weapon_near_empty_sound", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar weapon_debug_max_inaccuracy( side_registry(), "weapon_debug_max_inaccuracy", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force all shots to have maximum inaccuracy" );
ConVar weapon_debug_inaccuracy_only_up( side_registry(), "weapon_debug_inaccuracy_only_up", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force weapon inaccuracy to be in exactly the up direction" );
// Client-only sound tuning cvar; keep it out of server.dll to avoid an
// unlinkable duplicate registration.
ConVar snd_max_pitch_shift_inaccuracy(side_registry(), "snd_max_pitch_shift_inaccuracy", "0.08", 0);

	// this is a cheap ripoff from CBaseCombatWeapon::WeaponSound():
	void FX_WeaponSound(
		int iPlayerIndex,
		uint16 nItemDefIndex,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		const CCSWeaponInfo *pWeaponInfo,
		float flSoundTime,
		int nPitch )
	{
		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = pWeaponInfo->aShootSounds[ sound_type ];

		// Get the item definition
		const CEconItemDefinition *pDef = ( nItemDefIndex > 0 ) ? GetItemSchema()->GetItemDefinition( nItemDefIndex ) : nullptr;
		if ( pDef )
		{
			const char *pszTempSound = pDef->GetWeaponReplacementSound( sound_type );
			if ( pszTempSound )
			{
				shootsound = pszTempSound;
			}
		}

		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;

		EmitSound_t params;
		params.m_pSoundName = shootsound;
		params.m_flSoundTime = flSoundTime;
		params.m_pOrigin = &vOrigin;
		params.m_pflSoundDuration = nullptr;
		params.m_bWarnOnDirectWaveReference = true;
		params.m_nPitch = nPitch;

		if (nPitch != PITCH_NORM)
		{
			params.m_nFlags = params.m_nFlags | SND_OVERRIDE_PITCH;
		}
				
		CBaseEntity::EmitSound( filter, iPlayerIndex, params ); 
	}

	class CGroupedSound
	{
	public:
		string_t m_SoundName;
		Vector m_vPos;
	};

	std::vector< CGroupedSound > g_GroupedSounds;

	// Called by the ImpactSound function.
	void ShotgunImpactSoundGroup( const char *pSoundName, const Vector &vEndPos )
	{
		int i;
		// Don't play the sound if it's too close to another impact sound.
		for ( i=0; i < std::ssize( g_GroupedSounds ); i++ )
		{
			CGroupedSound *pSound = &g_GroupedSounds[i];

			if ( vEndPos.DistToSqr( pSound->m_vPos ) < 300*300 )
			{
				if ( V_stricmp( pSound->m_SoundName, pSoundName ) == 0 )
					return;
			}
		}

		// Ok, play the sound and add it to the list.
		CLocalPlayerFilter filter;
		CBaseEntity::EmitSound( filter, 0, pSoundName, &vEndPos );

		i = std::ssize( g_GroupedSounds ); g_GroupedSounds.emplace_back();
		g_GroupedSounds[i].m_SoundName = pSoundName;
		g_GroupedSounds[i].m_vPos = vEndPos;
	}

	void StartGroupingSounds()
	{
		contract_assert( std::ssize( g_GroupedSounds ) == 0 );
		SetImpactSoundRoute( ShotgunImpactSoundGroup );
	}

	void EndGroupingSounds()
	{
		g_GroupedSounds.clear(); g_GroupedSounds.shrink_to_fit();
		SetImpactSoundRoute( nullptr );
	}

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

	CCSPlayer *pPlayer = ToCSPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );

	if ( !pPlayer || iPlayerIndex < 0 )
	{
		// probably an env_gunfire
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( iWeaponID );
		FX_WeaponSound( iPlayerIndex, nItemDefIndex, sound_type, vOrigin, pWeaponInfo, flSoundTime, PITCH_NORM );
		return;
	}

	CWeaponCSBase* pClientWeapon = pPlayer ? pPlayer->GetActiveCSWeapon() : nullptr;
	if ( pClientWeapon )
	{
		if ( gpGlobals->curtime - pClientWeapon->m_flLastClientFireBulletTime > 0.02f ) // this should be enough even for the negev, at ~1000 rof
		{
			pClientWeapon->m_flLastClientFireBulletTime = gpGlobals->curtime;
		}
		else
		{
			return; // we already traced this shot on the client!
		}
	}

	QAngle adjustedAngles = vAngles;
	adjustedAngles.y += fAccuracyFishtail;

	if ( pPlayer && debug_aim_angle.GetBool() )
	{
		QAngle old = pPlayer->EyeAngles() + pPlayer->GetAimPunchAngle();
		DevMsg("Client ");
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

	if ( pPlayer && pPlayer->m_bUseNewAnimstate )
	{
		pPlayer->ProcessMuzzleFlashEvent();
	}

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

	// [sbodenbender] rumble when shooting
	// since we are handling bullet fx in CS differently than other titles, call 
	// rumble effect directly instead of Player::RumbleEffect
	//=============================================================================

	if (pPlayer && pPlayer->IsLocalPlayer() && pWeaponInfo && pWeaponInfo->GetBullets() > 0)
	{
		int rumbleEffect = pWeaponInfo->iRumbleEffect;

		if( rumbleEffect != RUMBLE_INVALID )
		{
			RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), rumbleEffect, 0, RUMBLE_FLAG_RESTART );
		}
	}

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

	EndGroupingSounds();
}

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
void FX_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
	CCSPlayer *pPlayer = ToCSPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );

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

}

class CEffectData;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Game-specific impact effect hooks

//-----------------------------------------------------------------------------
// Purpose: Handle weapon effect callbacks
//-----------------------------------------------------------------------------
void CStrike_EjectBrass( int shell, const CEffectData &data )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();

	if( !pPlayer )
		return;

	DevWarning( "Unhandled CStrike EjectBrass effect\n" );
}

void CStrike_FX_EjectBrass_9mm_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_9MM, data );
}

void CStrike_FX_EjectBrass_57_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_57, data );
}

void CStrike_FX_EjectBrass_12Gauge_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_12GAUGE, data );
}

void CStrike_FX_EjectBrass_556_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_556, data );
}

void CStrike_FX_EjectBrass_762Nato_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_762NATO, data );
}

void CStrike_FX_EjectBrass_338Mag_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_338MAG, data );
}

DECLARE_CLIENT_EFFECT( EjectBrass_9mm,		CStrike_FX_EjectBrass_9mm_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_12Gauge,	CStrike_FX_EjectBrass_12Gauge_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_57,		CStrike_FX_EjectBrass_57_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_556,		CStrike_FX_EjectBrass_556_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_762Nato,	CStrike_FX_EjectBrass_762Nato_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_338Mag,	CStrike_FX_EjectBrass_338Mag_Callback );

using namespace ::ks::reflect;

//---------------------------------------------------------
const char *CInferno::GetParticleEffectName()
{
	return "molotov_groundfire";
}

//---------------------------------------------------------
const char *CFireCrackerBlast::GetParticleEffectName()
{
	return "firework_crate_ground_effect";
}

class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Healthshot Item, belt item

static constexpr auto HEALTHSHOT_INJECT_TIME = 1.65f;

IMPLEMENT_NETWORKCLASS_ALIASED( Item_Healthshot, DT_Item_Healthshot )

IMPLEMENT_REFLECT_TABLE( CItem_Healthshot, DT_Item_Healthshot );

IMPLEMENT_REFLECT_PREDMAP( CItem_Healthshot );

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

void CItem_Healthshot::Precache( void )
{
	//PrecacheScriptSound( "Healthshot.Shot" );

	BaseClass::Precache();
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
		return false;
	}

	return true;
}

void CItem_Healthshot::DropHealthshot( void )
{
}

//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::OnStartUse( CCSPlayer *pPlayer )
{
	SetWeaponIdleTime( gpGlobals->curtime + 20 ); // don't fidget for a bit
	BaseClass::OnStartUse( pPlayer );
}

float CItem_Healthshot::GetUseTimerDuration( void )
{
	return HEALTHSHOT_INJECT_TIME;
}

class CNewParticleEffect;

using namespace ::ks::reflect;

ConVar molotov_throw_detonate_time( side_registry(), "molotov_throw_detonate_time", "2.0", FCVAR_CHEAT | FCVAR_REPLICATED );

IMPLEMENT_REFLECT_CLIENTCLASS( CMolotovProjectile, DT_MolotovProjectile, CMolotovProjectile )

//--------------------------------------------------------------------------------------------------------
void CMolotovProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_molotov_thrown" ) || FStrEq( pszParticleName, "incgrenade_thrown_trail" ) )
	{
		m_molotovParticleEffect = pNewParticleEffect;
	}
}

//--------------------------------------------------------------------------------------------------------
void CMolotovProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_molotovParticleEffect == pParticleEffect )
	{
		m_molotovParticleEffect = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------
bool CMolotovProjectile::Simulate( void )
{
	if ( !m_molotovParticleEffect.IsValid() )
	{
		if ( m_bIsIncGrenade )
			// todo: make this come from an attachment
			DispatchParticleEffect( "incgrenade_thrown_trail", PATTACH_POINT_FOLLOW, this, "trail" );
		else
			DispatchParticleEffect( "weapon_molotov_thrown", PATTACH_POINT_FOLLOW, this, "Wick" );
	}
	else
	{
		m_molotovParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_molotovParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}

class CBaseAnimating;
class CCSPlayer;
class CRopeKeyframe;

using namespace ::ks::reflect;

static constexpr auto PLANTEDC4_MSG_JUSTBLEW = 1;
static constexpr auto TIME_TO_DETONATE_WARNING = 10;

extern ConVar spec_show_xray;

IMPLEMENT_REFLECT_CLIENTCLASS( CPlantedC4, DT_PlantedC4, CPlantedC4 )

// PRECACHE_REGISTER_BEGIN( GLOBAL, CPlantedC4 )
// PRECACHE( PARTICLE_SYSTEM, "bomb_explosion_huge" )
// PRECACHE_REGISTER_END()

std::vector< CPlantedC4* > g_PlantedC4s;

CPlantedC4::CPlantedC4() :
	m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
{
	g_PlantedC4s.push_back( this );

	m_flNextRadarFlashTime = gpGlobals->curtime;
	m_bRadarFlash = true;

	// Don't beep right away, leave time for the planting sound
	m_flNextGlow = gpGlobals->curtime + 1.0;
	m_flNextBeep = gpGlobals->curtime + 1.0;

	m_hLocalDefusingPlayerHandle = nullptr;
}

CPlantedC4::~CPlantedC4()
{
	ks::erase_first( g_PlantedC4s, this );
}

void CPlantedC4::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	
	// Remove us from the list of planted C4s.
	if ( bDormant )
	{
		ks::erase_first( g_PlantedC4s, this );
		m_bTenSecWarning = false;
	}
	else
	{
		if ( ks::find_index( g_PlantedC4s, this ) == -1 )
			g_PlantedC4s.push_back( this );
	}
}

void CPlantedC4::UpdateOnRemove( void )
{
	DestroyDefuserRopes();
	BaseClass::UpdateOnRemove();
}

void CPlantedC4::Spawn( void )
{
	BaseClass::Spawn();

	AddFlag( FL_OBJECT );

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_bTenSecWarning = false;
	m_bExplodeWarning = false;
	m_bTriggerWarning = false;

	DestroyDefuserRopes();
}

void CPlantedC4::DestroyDefuserRopes( void )
{
	for ( auto &hRope : m_hDefuserRopes | std::views::reverse )
	{
		if ( hRope )
			UTIL_Remove( hRope );
	}
	m_hDefuserRopes.clear();
	
	if ( m_hDefuserMultimeter )
		UTIL_Remove( m_hDefuserMultimeter );
}

bool CPlantedC4::CreateDefuserRopes( void )
{
	DestroyDefuserRopes();

	// make sure we are being defused
	CCSPlayer *pDefusingPlayer = m_hBombDefuser->Get();
	if ( !pDefusingPlayer )
	{
		DevWarning( "Cannot find defusing player to create bomb defuse wire.\n" );
		return false;
	}

	if ( pDefusingPlayer->IsDormant() )
		return false;

	if ( !m_hDefuserMultimeter )
	{
		CBaseAnimating *pMultimeter = new CBaseAnimating;
		if ( pMultimeter->InitializeAsClientEntity( "models/weapons/w_eq_multimeter.mdl", false ) )
		{
			pMultimeter->AddSolidFlags( FSOLID_NOT_SOLID );
			pMultimeter->AddEffects( EF_BONEMERGE );
			pMultimeter->SetParent( pDefusingPlayer );
			m_hDefuserMultimeter = pMultimeter;
		}
		else
		{
			m_hDefuserMultimeter = nullptr;
		}
	}

	if ( !m_hDefuserMultimeter )
	{
		DevWarning( "Could not create defuser tool model.\n" );
		return false;
	}

	// validate attachments
	int nDefuseToolAttachmentA = m_hDefuserMultimeter->LookupAttachment("weapon_defusewire_a");
	int nDefuseToolAttachmentB = m_hDefuserMultimeter->LookupAttachment("weapon_defusewire_b");

	int nBombClipAttachmentA = LookupAttachment("weapon_defusewire_a");
	int nBombClipAttachmentB = LookupAttachment("weapon_defusewire_b");

	if ( nDefuseToolAttachmentA < 0 ||
		 nDefuseToolAttachmentB < 0 ||
		 nBombClipAttachmentA < 0 ||
		 nBombClipAttachmentB < 0 )
	{
		DevWarning( "Could not locate attachment for bomb defuse wire.\n" );
		return false;
	}

	// create the wires
	CRopeKeyframe* pRopeA = CRopeKeyframe::Create( this, m_hDefuserMultimeter, nBombClipAttachmentA, nDefuseToolAttachmentA, 0.9, "cable/phonecable", 8, ROPE_SIMULATE, ROPE_TYPE_DEFUSECABLES );
	if ( pRopeA )
	{
		pRopeA->AddRopeFlags( ROPE_USE_WIND );
		pRopeA->SetSlack( 142 );
		pRopeA->AddToLeafSystem();
		m_hDefuserRopes.emplace_back() = pRopeA;
		//pRopeA->AddEffects( EF_NORECEIVESHADOW );
	}
	else
	{
		DevWarning( "Failed to create bomb defuse wire.\n" );
		return false;
	}

	CRopeKeyframe* pRopeB = CRopeKeyframe::Create( this, m_hDefuserMultimeter, nBombClipAttachmentB, nDefuseToolAttachmentB, 0.9, "cable/phonecable_red", 8, ROPE_SIMULATE, ROPE_TYPE_DEFUSECABLES );
	if ( pRopeB )
	{
		pRopeB->AddRopeFlags( ROPE_USE_WIND );
		pRopeB->SetSlack( 142 );
		pRopeB->AddToLeafSystem();
		m_hDefuserRopes.emplace_back() = pRopeB;
		//pRopeA->AddEffects( EF_NORECEIVESHADOW );
	}
	else
	{
		DevWarning( "Failed to create bomb defuse wire.\n" );
		return false;
	}
	
	m_hLocalDefusingPlayerHandle = m_hBombDefuser->Get(); // remember this player handle in case it changes

	return true;
}

void CPlantedC4::ClientThink( void )
{
	BaseClass::ClientThink();

	CCSPlayer *pDefusingPlayer = m_hBombDefuser->Get();

	if ( m_hBombDefuser->Get() != m_hLocalDefusingPlayerHandle )
		DestroyDefuserRopes(); // we're still being defused, but the defusing player changed

	CCSPlayer *pLocalPlayer = GetLocalOrInEyeCSPlayer();
	if ( pDefusingPlayer && ( pDefusingPlayer != pLocalPlayer || pLocalPlayer->ShouldDraw() ) )
	{
		if ( m_hDefuserRopes.empty() )
		{
			CreateDefuserRopes();
		}
	}
	else
	{
		DestroyDefuserRopes();
	}

	// If it's dormant, don't beep or anything..
	if ( IsDormant() )
		return;

	Vector glowColor;
	bool bRenderGlow = (CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt());

	if ( !m_bBombTicking )
	{
		// disable C4 thinking if not armed
		// undone.  we need the bome to resume thinking once it's been actiavted (for training) - mtw
		//SetNextClientThink( CLIENT_THINK_NEVER );

		if ( m_bBombDefused )
		{
			glowColor.x = (64.0f/255.0f);
			glowColor.y = (255.0f/255.0f);
			glowColor.z = (64.0f/255.0f);
			m_GlowObject.SetRenderFlags( bRenderGlow, false );
			m_GlowObject.SetColor( glowColor );
			m_GlowObject.SetAlpha( bRenderGlow ? 0.95f : 0.0f );
		}

		return;
	}

	if( ( m_flC4Blow - gpGlobals->curtime ) < 1.0f &&
		!m_bTriggerWarning )
	{
		EmitSound( "C4.ExplodeTriggerTrip" );

		int ledAttachmentIndex = LookupAttachment("led");
		DispatchParticleEffect( "c4_timer_light_trigger", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );

		m_bTriggerWarning = true;
	}

	if( ( m_flC4Blow - gpGlobals->curtime ) < 0.0f &&
		!m_bExplodeWarning )
	{
		EmitSound( "C4.ExplodeWarning" );

		m_bExplodeWarning = true;
	}

	if( gpGlobals->curtime > m_flNextBeep )
	{
		// as it gets closer to going off, increase the radius

		CLocalPlayerFilter filter;
		float attenuation;
		float freq;

		//the percent complete of the bomb timer
		float fComplete = ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );
		
		fComplete = clamp( fComplete, 0.0f, 1.0f );

		attenuation = std::min( 0.3 + 0.6 * fComplete, 1.0 );
		
		CSoundParameters params;

		if ( ( m_flC4Blow - gpGlobals->curtime ) > 1.0f && GetParametersForSound( "C4.PlantSound", params, nullptr ) )
		{
			EmitSound_t ep( params );
			ep.m_SoundLevel = ATTN_TO_SNDLVL( attenuation );
			ep.m_pOrigin = &GetAbsOrigin();

			EmitSound( filter, SOUND_FROM_WORLD, ep );
		}

		freq = std::max( 0.1 + 0.9 * fComplete, 0.15 );

		m_flNextBeep = gpGlobals->curtime + freq;
	}
	if( ( m_flC4Blow - gpGlobals->curtime ) <= TIME_TO_DETONATE_WARNING &&
		!m_bTenSecWarning )
	{
		m_bTenSecWarning = true;
		
		if ( !CSGameRules()->IsPlayingTraining() )
		{
			CLocalPlayerFilter filter;
			PlayMusicSelection( filter, CSMUSIC_BOMBTEN );
		}
	}

	glowColor.x = (170.0f/255.0f);
	glowColor.y = (0.0f/255.0f);
	glowColor.z = (0.0f/255.0f);

	if ( (gpGlobals->curtime + 0.25f) > m_flNextGlow )
		glowColor.x = (255.0f/255.0f);

	if ( gpGlobals->curtime > m_flNextGlow )
	{
		if ( gpGlobals->curtime > m_flNextGlow && ( m_flC4Blow - gpGlobals->curtime ) > 1.0f )
		{
			int ledAttachmentIndex = LookupAttachment("led");
			DispatchParticleEffect( "c4_timer_light", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );
		}

		float freq = 0.1 + 0.9 * ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );

		if( freq < 0.15 ) freq = 0.15;

		m_flNextGlow = gpGlobals->curtime + freq;
	}	

	// Start glowing
	m_GlowObject.SetRenderFlags( bRenderGlow, false );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( bRenderGlow ? 0.95f : 0.0f );
}

// Using a new particle system, networked down from the server (KWD)
void CPlantedC4::Explode( void )
{
	// don't hide it in training, we have a particle effect that covers it up and then eventally it all gets removed
	if ( !CSGameRules()->IsPlayingTraining() )
		AddEffects( EF_NODRAW );

	SetDormant( true );
}

float CPlantedC4::GetDetonationProgress( void )
{
	float fComplete = 0.0f;
	if ( m_flTimerLength > 0.0f )
	{
		fComplete = ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );
	}

	return fComplete;
}

float CPlantedC4::GetDefuseProgress( void)
{
	float flProgress = 1.0f;

	if( m_flDefuseLength > 0.0 )
	{
		flProgress = ( ( m_flDefuseCountDown - gpGlobals->curtime ) / m_flDefuseLength );
	}

	return flProgress;
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

class CCSPlayer;

using namespace ::ks::reflect;

// ---------------------------------------------------------------------- //
// The radio feedback manager for the client.
// ---------------------------------------------------------------------- //
static CRadioStatus s_RadioStatus;

//
//-----------------------------------------------------
//
ConVar radio_icons_use_particles( side_registry(), "radio_icons_use_particles", "1", FCVAR_NONE, "0 = classic style, 1 = particles" );

// Stuff for the Radio Menus
static void radio1_f( const CCommand &args );
static void radio2_f( const CCommand &args );
static void radio3_f( const CCommand &args );

static ConCommand radio1( side_registry(), "radio1", radio1_f, "Opens a radio menu" );
static ConCommand radio2( side_registry(), "radio2", radio2_f, "Opens a radio menu" );
static ConCommand radio3( side_registry(), "radio3", radio3_f, "Opens a radio menu" );
static int g_whichMenu = 0;

//
//--------------------------------------------------------------
//
// These methods will bring up the radio menus from the client side.
// They mimic the old server commands of the same name, which used
// to require a round-trip causing latency and unreliability in 
// menu responses.  Only 1 message is sent to the server now which
// includes both the menu name and the selected item.  The server
// is never informed that the menu has been displayed.
//
//--------------------------------------------------------------
//
void OpenRadioMenu( int index )
{
}

static void radio1_f( const CCommand &args )
{
	OpenRadioMenu( 1 );
}

static void radio2_f( const CCommand &args )
{
	OpenRadioMenu( 2 );
}

static void radio3_f( const CCommand &args )
{
	OpenRadioMenu( 3 );
}

static void menuselect( const CCommand &args );
static ConCommand menuselect_command( side_registry(), "menuselect", menuselect, "menuselect", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void menuselect( const CCommand &args )
{
	if ( args.ArgC() < 2 )
		return;

	if( g_whichMenu == 0 )
	{
		// if we didn't have a menu open, maybe a plugin did.  send it on to the server.
		const char *cmd = VarArgs( "menuselect %s", args[1] );
		engine->ServerCmd( cmd );
		return;
	}

	int whichEntry = atoi( args[ 1 ] );

	switch( g_whichMenu )
	{
		case 1: //RadioA
		{
			switch( whichEntry )
			{
			case 1: // coverme
				engine->ClientCmd( "coverme" );
				break;
			case 2: // takepoint
				engine->ClientCmd( "takepoint" );
				break;
			case 3: // holdpos
				engine->ClientCmd( "holdpos" );
				break;
			case 4: // regroup
				engine->ClientCmd( "regroup" );
				break;
			case 5: // followme
				engine->ClientCmd( "followme" );
				break;
			case 6: // takingfire
				engine->ClientCmd( "takingfire" );
				break;
			}
		}
		break;

		case 2: //RadioB
		{
			switch( whichEntry )
			{
			case 1: // go
				engine->ClientCmd( "go" );
				break;
			case 2: // fallback
				engine->ClientCmd( "fallback" );
				break;
			case 3: // sticktog
				engine->ClientCmd( "sticktog" );
				break;
			case 4: // getinpos
				engine->ClientCmd( "getinpos" );
				break;
			case 5: // stormfront
				engine->ClientCmd( "stormfront" );
				break;
			case 6: // report
				engine->ClientCmd( "report" );
				break;
			}
		}
		break;

		case 3: //RadioC
		{
			switch( whichEntry )
			{
			case 1: // roger
				engine->ClientCmd( "roger" );
				break;
			case 2: // enemyspot
				engine->ClientCmd( "enemyspot" );
				break;
			case 3: // needbackup
				engine->ClientCmd( "needbackup" );
				break;
			case 4: // sectorclear
				engine->ClientCmd( "sectorclear" );
				break;
			case 5: // inposition
				engine->ClientCmd( "inposition" );
				break;
			case 6: // reportingin
				engine->ClientCmd( "reportingin" );
				break;
			case 7: // getout
				engine->ClientCmd( "getout" );
				break;
			case 8: // negative
				engine->ClientCmd( "negative" );
				break;
			case 9: // enemydown
				engine->ClientCmd( "enemydown" );
				break;
			}
		}
		break;

		default:
			// if we didn't have a menu open, maybe a plugin did.  send it on to the server.
			const char *cmd = VarArgs( "menuselect %d", whichEntry );
			engine->ServerCmd( cmd );
			break;
	}

	// reset menu
	g_whichMenu = 0;
}

//
//-----------------------------------------------------
//

CRadioStatus* RadioManager()
{
	return &s_RadioStatus;
}

// ---------------------------------------------------------------------- //
// CRadioStatus.
// ---------------------------------------------------------------------- //

CRadioStatus::CRadioStatus()
{
	m_pHeadLabelMaterial = nullptr;
	V_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	V_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

bool CRadioStatus::Init()
{
	if ( !m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial = materials->FindMaterial( "sprites/radio", TEXTURE_GROUP_VGUI );
	}

	if ( IsErrorMaterial( m_pHeadLabelMaterial ) && !g_bTextMode )
		return false;

	m_pHeadLabelMaterial->IncrementReferenceCount();

	return true;
}

void CRadioStatus::Shutdown()
{
	if ( m_pHeadLabelMaterial )
		m_pHeadLabelMaterial->DecrementReferenceCount();

	m_pHeadLabelMaterial = nullptr;		
}

void CRadioStatus::LevelInitPostEntity()
{
	ExpireBotVoice( true );
	V_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	V_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

void CRadioStatus::LevelShutdownPreEntity()
{
	ExpireBotVoice( true );
	V_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	V_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

static float s_flHeadIconSize = 7;

void CRadioStatus::DrawHeadLabels()
{
	ExpireBotVoice();
	ConVarRef voice_head_icon_height( "voice_head_icon_height" );

	if( !m_pHeadLabelMaterial )
		return;

	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if ( m_radioUntil[i] < gpGlobals->curtime )
			continue;

		IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );
		
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pClient );
		if( !pPlayer )
			continue;

		// Don't show an icon if the player is not in our PVS.
		if ( !pClient || pClient->IsDormant() )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		if( !UTIL_PlayerByIndex( i+1 ) )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if( pPlayer->IsPlayerDead() )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		// Don't show an icon for players we can't hear
		if ( !GetClientVoiceMgr()->IsPlayerAudible( i+1 ) )
			continue;

		if ( radio_icons_use_particles.GetBool() )
		{
			pPlayer->UpdateRadioHeadIcon( true );
			return;
		}

		// Place it above his head.
		Vector vOrigin = pPlayer->EyePosition();
		vOrigin.z += GetClientVoiceMgr()->GetHeadLabelOffset();
		
		// Place this above the speaking bubble if he's currently speaking.
		if ( GetClientVoiceMgr()->IsPlayerSpeaking( i+1 ) )
		{
			vOrigin.z += GetClientVoiceMgr()->GetHeadLabelOffset();
		}
		
		// Align it so it never points up or down.
		Vector vUp( 0, 0, 1 );
		Vector vRight = CurrentViewRight();
		if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
			continue;

		vRight.z = 0;
		VectorNormalize( vRight );

		float flSize = s_flHeadIconSize;

		CMatRenderContextPtr pRenderContext( materials );

		pRenderContext->Bind( m_pHeadLabelMaterial );
		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}

void CRadioStatus::UpdateRadioStatus(int entindex, float duration)
{
	if(entindex > 0 && entindex <= VOICE_MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		m_radioUntil[iClient] = gpGlobals->curtime + duration;
	}
}

void CRadioStatus::UpdateVoiceStatus(int entindex, float duration)
{
	if(entindex > 0 && entindex <= VOICE_MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		m_voiceUntil[iClient] = gpGlobals->curtime + duration;
		GetClientVoiceMgr()->UpdateSpeakerStatus( entindex, -1, true );
	}
}

void CRadioStatus::ExpireBotVoice( bool force )
{
	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if ( m_voiceUntil[i] > 0.0f )
		{
			bool expire = force;

			CCSPlayer *player = static_cast<CCSPlayer*>( cl_entitylist->GetEnt(i+1) );
			if ( !player )
			{
				// player left the game
				expire = true;
			}
			else if ( m_voiceUntil[i] < gpGlobals->curtime )
			{
				// player is done speaking
				expire = true;
			}

			if ( expire )
			{
				m_voiceUntil[i] = 0.0f;
				GetClientVoiceMgr()->UpdateSpeakerStatus( i+1, -1, false );
			}
		}
	}
}

class CNewParticleEffect;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CSensorGrenadeProjectile, DT_SensorGrenadeProjectile, CSensorGrenadeProjectile )

//--------------------------------------------------------------------------------------------------------
void CSensorGrenadeProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_sensorgren_detlight" ) )
	{
		m_sensorgrenadeParticleEffect = pNewParticleEffect;
	}
}

//--------------------------------------------------------------------------------------------------------
void CSensorGrenadeProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_sensorgrenadeParticleEffect == pParticleEffect )
	{
		m_sensorgrenadeParticleEffect = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------
bool CSensorGrenadeProjectile::Simulate( void )
{
	// we are still moving
	if ( GetAbsVelocity().Length() > 0.1f )
	{
		return true;
	}

	if ( !m_sensorgrenadeParticleEffect.IsValid() )
	{
		DispatchParticleEffect( "weapon_sensorgren_detlight", PATTACH_POINT_FOLLOW, this, "Wick" );
	}
	else
	{
		m_sensorgrenadeParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_sensorgrenadeParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}

class CNewParticleEffect;

using namespace ::ks::reflect;

#define GRENADE_MODEL "models/Weapons/w_eq_smokegrenade_thrown.mdl"

IMPLEMENT_REFLECT_CLIENTCLASS( CSmokeGrenadeProjectile, DT_SmokeGrenadeProjectile, CSmokeGrenadeProjectile )

CSmokeGrenadeProjectile::~CSmokeGrenadeProjectile()
{
	RemoveSmokeGrenadeHandle( this );
}

void CSmokeGrenadeProjectile::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );	
}

void CSmokeGrenadeProjectile::OnDataChanged( DataUpdateType_t updateType ) 
{ 
	if ( ( m_nSmokeEffectTickBegin || m_bDidSmokeEffect ) && !m_bSmokeEffectSpawned )
	{
		SpawnSmokeEffect();
		// And the smoke grenade particle began! - every call but the first is extraneous here
		AddSmokeGrenadeHandle( this );
	}
}

void CSmokeGrenadeProjectile::SpawnSmokeEffect( )
{
	if ( !m_bSmokeEffectSpawned )
	{
		m_bSmokeEffectSpawned = true;
		CNewParticleEffect *pSmokeEffect = nullptr;

		// Used to be: 
		int nUseMethod = 2;
		if ( nUseMethod == 0 )
		{
			// this is the closest to the old method; it doesn't let us correct the lifetime of the particle system in case of full frame update
			DispatchParticleEffect( "explosion_smokegrenade", GetAbsOrigin(), QAngle( 0, 0, 0 ) );// note QAngle(0,0,0). But we need to simulate the particle effect forward sometimes, so we need to use different API now.
		}
		else
		{
			Vector vOrigin = GetNetworkOrigin();
			if ( nUseMethod == 1 )
			{
				// This method works, but isn't the closest to the old method. The old method used CNewParticleEffect::CreateOrAggregate() API in its guts, but aggregation is implicitly disabled by explosion_smokegrenade particle definition as of Dec 2015 in CSGO staging.
				pSmokeEffect = ParticleProp()->Create( "explosion_smokegrenade", PATTACH_CUSTOMORIGIN );
			}
			else
			{
				// The old method used CNewParticleEffect::CreateOrAggregate() API in its guts, so this is the closest method to create smoke to the old method, but it's not been tested in trunk
				pSmokeEffect = CNewParticleEffect::CreateOrAggregate( nullptr, "explosion_smokegrenade", vOrigin );
			}

			if ( pSmokeEffect )
			{
				pSmokeEffect->SetSortOrigin( vOrigin );
				pSmokeEffect->SetControlPoint( 0, vOrigin );
				pSmokeEffect->SetControlPoint( 1, vOrigin );
				pSmokeEffect->SetControlPointOrientation( 0, Vector( 1, 0, 0 ), Vector( 0, -1, 0 ), Vector( 0, 0, 1 ) );
			}
		}

		if ( m_nSmokeEffectTickBegin )
		{
			int nSkipFrames = gpGlobals->tickcount - m_nSmokeEffectTickBegin;
			if ( nSkipFrames > 4 && pSmokeEffect )
			{
				//Note: pSmokeEffect->Simulate( flSkipSeconds ); would be ideal, but it doesn't work well for long intervals. SkipToTime would be even better but it will extinguish the particle effect if it skips past 2 seconds due to some perf heuristic, and it's not clear if it skips correctly either.
				// this doesn't happen often, and when it does, it's on connection or on replay begin/end, so a little hitch shouldn't be a problem.
				for ( int i = 2; i < nSkipFrames; i += 2 )
					pSmokeEffect->Simulate( gpGlobals->interval_per_tick * 2 );
			}
		}
	}
}

#undef GRENADE_MODEL

class CCSPlayer;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Kills Player Attachments
//-----------------------------------------------------------------------------
class CTERadioIcon : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TERadioIcon" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTERadioIcon, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTERadioIcon( void );
	virtual			~CTERadioIcon( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int m_iAttachToClient;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTERadioIcon::CTERadioIcon( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTERadioIcon::~CTERadioIcon( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTERadioIcon::PostDataUpdate( DataUpdateType_t updateType )
{
	//Flash them on the radar
	//this could be in a better place.
	CCSPlayer *pPlayer = static_cast<CCSPlayer*>( cl_entitylist->GetEnt(m_iAttachToClient) );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Create the flashy above player's head
		RadioManager()->UpdateRadioStatus( m_iAttachToClient, 1.5f );
	}
	
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTERadioIcon, DT_TERadioIcon, CTERadioIcon )

using namespace ::ks::reflect;

class CTEFireBullets : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEFireBullets", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEFireBullets, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

	CTEFireBullets();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_iPlayer;
	[[= ks::reflect::Net{} ]] uint16			m_nItemDefIndex;
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] QAngle			m_vecAngles;
	[[= ks::reflect::Net{} ]] CSWeaponID		m_iWeaponID;
	[[= ks::reflect::Net{} ]] int				m_iMode;
	[[= ks::reflect::Net{} ]] int				m_iSeed;
	[[= ks::reflect::Net{} ]] float			m_fInaccuracy;
	[[= ks::reflect::Net{} ]] float			m_flRecoilIndex;
	[[= ks::reflect::Net{} ]] float			m_fSpread;
	[[= ks::reflect::Net{} ]] WeaponSound_t	m_iSoundType;
};

CTEFireBullets::CTEFireBullets()
{
	m_iPlayer = 0;
	m_nItemDefIndex = 0;
	m_iWeaponID = WEAPON_NONE;
	m_iMode = 0;
	m_iSeed = 0;
	m_fInaccuracy = 0;
	m_flRecoilIndex = 0;
	m_fSpread = 0;
	m_iSoundType = EMPTY;
}

void CTEFireBullets::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.

	//
	// Vitaliy: Aug-2013 economy update note
	// fields m_nItemDefIndex and m_iSoundType are new and are not
	// networked in the previous demo recordings, force the sound type
	// to the default SINGLE value if it didn't network correctly
	//
	contract_assert( ( m_iSoundType == EMPTY ) ||	// pre-economy update ver. 1.23.1.4
		( m_iSoundType == SINGLE ) ||	// default sound for firing
		( m_iSoundType == SPECIAL1 ) ||	// silenced weapons use this sound
		0 );
	WeaponSound_t wpnSoundToUse = SINGLE;
	switch ( m_iSoundType )
	{
	case SPECIAL1:
		wpnSoundToUse = SPECIAL1;
		break;
	}
	
	m_vecAngles.z = 0;
	
	FX_FireBullets( 
		m_iPlayer+1,
		m_nItemDefIndex,
		m_vecOrigin,
		m_vecAngles,
		m_iWeaponID,
		m_iMode,
		m_iSeed,
		m_fInaccuracy,
		m_fSpread,
		0.0f,
		0,
		wpnSoundToUse,
		m_flRecoilIndex
		);
}

IMPLEMENT_CLIENTCLASS_EVENT( CTEFireBullets, DT_TEFireBullets, CTEFireBullets );

IMPLEMENT_REFLECT_TABLE( CTEFireBullets, DT_TEFireBullets );

class CTEPlantBomb : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPlantBomb", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPlantBomb, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int		m_iPlayer;
	[[= ks::reflect::Net{} ]] Vector	m_vecOrigin;
	[[= ks::reflect::Net{} ]] PlantBombOption_t	m_option;
};

void CTEPlantBomb::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.
	FX_PlantBomb( m_iPlayer+1, m_vecOrigin, m_option );
}

IMPLEMENT_CLIENTCLASS_EVENT( CTEPlantBomb, DT_TEPlantBomb, CTEPlantBomb );

IMPLEMENT_REFLECT_TABLE( CTEPlantBomb, DT_TEPlantBomb );
