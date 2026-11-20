// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

module;
#include <math.h>
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "imaterialproxydict.h"
#include "client_class.h"
#include "declare_class.h"
#include "dlight.h"
#include "edict.h"
#include "reflect_annotations.h"
#include "vphysics_interface.h"
#include "reflect_recvtable.h"
#include "basehandle.h"
#include "dt_recv.h"
#include "iclientrenderable.h"
#include "collisionutils.h"
#include "engine/ivmodelinfo.h"
#include "materialsystem/imesh.h"
#include "raytrace.h"
#include "tier2/resourceprecacher.h"
#include "worldsize.h"
#include "cdll_client_int.h"
#include "shareddefs.h"
#include "materialsystem/imaterialvar.h"
#include "cmodel.h"
#include "string_t.h"
module ks.client;

import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import :iinput;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :sharedvar;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import std;
import ks.core.random;
import ks.core.time;
import ks.core.vprof;
import ks.game.precipitation_shared;
import ks.math.fltx4;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.intrusive_ptr;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :cstrike15.cs_player;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import ks.core.types;
import ks.game.tempentity;
import :entitydatainstantiator;
import ks.math.aabb;

class CDummyMaterialProxy : public IMaterialProxy
{
public:
	CDummyMaterialProxy();
	virtual ~CDummyMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial() { return nullptr; }
};

CDummyMaterialProxy::CDummyMaterialProxy()
{
	DevMsg( 1, "CDummyMaterialProxy::CDummyMaterialProxy()\n" );
}

CDummyMaterialProxy::~CDummyMaterialProxy()
{
	DevMsg( 1, "CDummyMaterialProxy::~CDummyMaterialProxy()\n" );
}

bool CDummyMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	DevMsg( 1, "CDummyMaterialProxy::Init( material = \"%s\" )\n", pMaterial->GetName() );
	return true;
}

void CDummyMaterialProxy::OnBind( void *pC_BaseEntity )
{
	DevMsg( 1, "CDummyMaterialProxy::OnBind( %p )\n", pC_BaseEntity );
}

EXPOSE_MATERIAL_PROXY( CDummyMaterialProxy, Dummy );

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Dynamic light

//-----------------------------------------------------------------------------
// A dynamic light, with the goofy hack needed for spotlights
//-----------------------------------------------------------------------------
class CDynamicLight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_DynamicLight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDynamicLight, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CDynamicLight();

public:
	void	OnDataChanged(DataUpdateType_t updateType);
	bool	ShouldDraw();
	void	ClientThink( void );
	void	Release( void );

	[[= ks::reflect::Net{} ]] unsigned char	m_Flags;
	[[= ks::reflect::Net{} ]] unsigned char	m_LightStyle;

	[[= ks::reflect::Net{} ]] float	m_Radius;
	[[= ks::reflect::Net{} ]] int		m_Exponent;
	[[= ks::reflect::Net{} ]] float	m_InnerAngle;
	[[= ks::reflect::Net{} ]] float	m_OuterAngle;
	[[= ks::reflect::Net{} ]] float	m_SpotRadius;

private:
	dlight_t*	m_pDynamicLight;
	dlight_t*	m_pSpotlightEnd;

	inline bool ShouldBeElight() { return (m_Flags & DLIGHT_NO_WORLD_ILLUMINATION); }
};

IMPLEMENT_REFLECT_CLIENTCLASS( CDynamicLight, DT_DynamicLight, CDynamicLight )

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
CDynamicLight::CDynamicLight(void) : m_pSpotlightEnd(0), m_pDynamicLight(0)
{
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CDynamicLight::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime + 0.05);
	}

	BaseClass::OnDataChanged( updateType );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
bool CDynamicLight::ShouldDraw()
{
	return false;
}

//------------------------------------------------------------------------------
// Purpose : Disable drawing of this light when entity perishes
//------------------------------------------------------------------------------
void CDynamicLight::Release()
{
	if (m_pDynamicLight)
	{
		m_pDynamicLight->die = gpGlobals->curtime;
		m_pDynamicLight = 0;
	}
	
	if (m_pSpotlightEnd)
	{
		m_pSpotlightEnd->die = gpGlobals->curtime;
		m_pSpotlightEnd = 0;
	}

	BaseClass::Release();
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CDynamicLight::ClientThink(void)
{
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );

	if ( (m_Flags & DLIGHT_NO_MODEL_ILLUMINATION) == 0 )
	{
		// Deal with the model light
 		if ( !m_pDynamicLight || (m_pDynamicLight->key != index) )
		{
			m_pDynamicLight = effects->CL_AllocDlight( index );
			contract_assert( m_pDynamicLight );
			m_pDynamicLight->minlight = 0;
		}

		m_pDynamicLight->style = m_LightStyle;
		m_pDynamicLight->radius = m_Radius;
		m_pDynamicLight->flags = m_Flags;
		if ( m_OuterAngle > 0 )
			m_pDynamicLight->flags |= DLIGHT_NO_WORLD_ILLUMINATION;
		color24 c = GetRenderColor();
		m_pDynamicLight->color.r = c.r;
		m_pDynamicLight->color.g = c.g;
		m_pDynamicLight->color.b = c.b;
		m_pDynamicLight->color.exponent	= m_Exponent;	// this makes it match the world
		m_pDynamicLight->origin		= GetAbsOrigin();
		m_pDynamicLight->m_InnerAngle = m_InnerAngle;
		m_pDynamicLight->m_OuterAngle = m_OuterAngle;
		m_pDynamicLight->die = gpGlobals->curtime + 1e6;
		m_pDynamicLight->m_Direction = forward;
	}
	else
	{
		// In this case, the m_Flags could have changed; which is how we turn the light off
		if (m_pDynamicLight)
		{
			m_pDynamicLight->die = gpGlobals->curtime;
			m_pDynamicLight = 0;
		}
	}
	
	if (( m_OuterAngle > 0 ) && ((m_Flags & DLIGHT_NO_WORLD_ILLUMINATION) == 0))
	{
		// Raycast to where the endpoint goes
		// Deal with the environment light
		if ( !m_pSpotlightEnd || (m_pSpotlightEnd->key != -index) )
		{
			m_pSpotlightEnd = effects->CL_AllocDlight( -index );
			contract_assert( m_pSpotlightEnd );
		}
				  
		// Trace a line outward, don't use hitboxes (too slow)
		Vector end;
		VectorMA( GetAbsOrigin(), m_Radius, forward, end );

		trace_t		pm;
		CBaseEntity::PushEnableAbsRecomputations( false );	 // HACK don't recompute positions while doing RayTrace
		UTIL_TraceLine( GetAbsOrigin(), end, MASK_NPCWORLDSTATIC, nullptr, COLLISION_GROUP_NONE, &pm );
		CBaseEntity::PopEnableAbsRecomputations();
		VectorCopy( pm.endpos, m_pSpotlightEnd->origin );
		
		if (pm.fraction == 1.0f)
		{
			m_pSpotlightEnd->die = gpGlobals->curtime;
			m_pSpotlightEnd = 0;
		}
		else
		{
			float falloff = 1.0 - pm.fraction;
			falloff *= falloff;

			m_pSpotlightEnd->style = m_LightStyle;
			m_pSpotlightEnd->flags = DLIGHT_NO_MODEL_ILLUMINATION | (m_Flags & DLIGHT_DISPLACEMENT_MASK);
			m_pSpotlightEnd->radius		= m_SpotRadius; // * falloff;
			m_pSpotlightEnd->die		= gpGlobals->curtime + 1e6;
			color24 c = GetRenderColor();
			m_pSpotlightEnd->color.r	= c.r * falloff;
			m_pSpotlightEnd->color.g	= c.g * falloff;
			m_pSpotlightEnd->color.b	= c.b * falloff;
			m_pSpotlightEnd->color.exponent	= m_Exponent;

			// For bumped lighting
			m_pSpotlightEnd->m_Direction = forward;

			// Update list of surfaces we influence
			render->TouchLight( m_pSpotlightEnd );
		}
	}
	else
	{
		// In this case, the m_Flags could have changed; which is how we turn the light off
		if (m_pSpotlightEnd)
		{
			m_pSpotlightEnd->die = gpGlobals->curtime;
			m_pSpotlightEnd = 0;
		}
	}

	SetNextClientThink(gpGlobals->curtime + 0.001);
}

class CEffectData;
class CBaseEntity;

using namespace ::ks::reflect;

static constexpr auto SUBINCH_PRECISION = 3;

// MAX_MODEL_INDEX_BITS moved to effect_dispatch_data.h: an annotation on CEffectData has to be a
// constant expression where the class is declared.

	// Not static: declared in effect_dispatch_data.h so the class's annotation can name it.
	void RecvProxy_EntIndex( const CRecvProxyData *pData, void *pStruct, void *pOut )
	{
		int nEntIndex = pData->m_Value.m_Int;
		((CEffectData*)pStruct)->m_hEntity = (nEntIndex < 0) ? INVALID_EHANDLE : ClientEntityList().EntIndexToHandle( nEntIndex );
	}

	IMPLEMENT_REFLECT_TABLE( CEffectData, DT_EffectData );

IClientRenderable *CEffectData::GetRenderable() const
{
	return ClientEntityList().GetClientRenderableFromHandle( m_hEntity );
}

CBaseEntity *CEffectData::GetEntity() const
{
	return ClientEntityList().GetBaseEntityFromHandle( m_hEntity );
}

int CEffectData::entindex() const
{
	CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( m_hEntity );
	return pEnt ? pEnt->entindex() : -1;
}

bool g_bSuppressParticleEffects = false;

bool SuppressingParticleEffects()
{
	return g_bSuppressParticleEffects;
}

void SuppressParticleEffects( bool bSuppress )
{
	g_bSuppressParticleEffects = bSuppress;
}

CEffectData::CEffectData()
{
	m_vOrigin.Init();
	m_vStart.Init();
	m_vNormal.Init();
	m_vAngles.Init();

	m_fFlags = 0;
	m_hEntity = INVALID_EHANDLE;
	m_flScale = 1.f;
	m_nAttachmentIndex = 0;
	m_nSurfaceProp = 0;

	m_flMagnitude = 0.0f;
	m_flRadius = 0.0f;

	m_nMaterial = 0;
	m_nDamageType = 0;
	m_nHitBox = 0;

	m_nColor = 0;

	m_nOtherEntIndex = 0;

	m_bPositionsAreRelativeToEntity = false;
}

class CClient_Precipitation;
class CParticleSimulateIterator;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class SimpleParticle;

using namespace ::ks::reflect;

ConVar	cl_winddir			( side_registry(), "cl_winddir", "0", FCVAR_CHEAT, "Weather effects wind direction angle" );
ConVar	cl_windspeed		( side_registry(), "cl_windspeed", "0", FCVAR_CHEAT, "Weather effects wind speed scalar" );

Vector g_vSplashColor( 0.5, 0.5, 0.5 );
float g_flSplashScale = 0.15;
float g_flSplashLifetime = 0.5f;
float g_flSplashAlpha = 0.3f;
ConVar r_RainSplashPercentage( side_registry(), "r_RainSplashPercentage", "20", FCVAR_CHEAT ); // N% chance of a rain particle making a splash.
ConVar r_RainParticleDensity( side_registry(), "r_RainParticleDensity", "1", FCVAR_NONE, "Density of Particle Rain 0-1" );
ConVar r_RainAllowInSplitScreen( side_registry(), "r_RainAllowInSplitScreen", "0", FCVAR_NONE, "Allows rain in splitscreen" );

float GUST_INTERVAL_MIN = 1;
float GUST_INTERVAL_MAX = 2;

float GUST_LIFETIME_MIN = 1;
float GUST_LIFETIME_MAX = 3;

float MIN_SCREENSPACE_RAIN_WIDTH = 1;

ConVar r_RainHack( side_registry(), "r_RainHack", "0", FCVAR_CHEAT );
ConVar r_RainRadius( side_registry(), "r_RainRadius", "1500", FCVAR_CHEAT );
ConVar r_RainSideVel( side_registry(), "r_RainSideVel", "130", FCVAR_CHEAT, "How much sideways velocity rain gets." );

// Performance optimization by Certain Affinity
// calling IsInAir() for 800 particles was taking 4 ms
ConVar	r_RainCheck( side_registry(), "r_RainCheck", "0", FCVAR_CHEAT, "Enable/disable IsInAir() check for rain drops?" );

ConVar r_RainSimulate( side_registry(), "r_RainSimulate", "1", FCVAR_CHEAT, "Enable/disable rain simulation." );
ConVar r_DrawRain( side_registry(), "r_DrawRain", "1", FCVAR_CHEAT, "Enable/disable rain rendering." );
ConVar r_RainProfile( side_registry(), "r_RainProfile", "0", FCVAR_CHEAT, "Enable/disable rain profiling." );
ConVar r_RainDebugDuration( side_registry(), "r_RainDebugDuration", "0", FCVAR_CHEAT, "Shows rain tracelines for this many seconds (0 disables)" );

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePrecipitation )
	PRECACHE( MATERIAL, "particle/rain" )
	PRECACHE( MATERIAL, "particle/snow" )
	PRECACHE( PARTICLE_SYSTEM, "rain_storm" )
	PRECACHE( PARTICLE_SYSTEM, "rain_storm_screen" )
	PRECACHE( PARTICLE_SYSTEM, "rain_storm_outer" )
	PRECACHE( PARTICLE_SYSTEM, "rain" )
	PRECACHE( PARTICLE_SYSTEM, "rain_outer" )
	PRECACHE( PARTICLE_SYSTEM, "ash" )
	PRECACHE( PARTICLE_SYSTEM, "ash_outer" )
		PRECACHE( PARTICLE_SYSTEM, "snow" )
		PRECACHE( PARTICLE_SYSTEM, "snow_outer" )
PRECACHE_REGISTER_END()

std::vector< std::unique_ptr< RayTracingEnvironment > > g_RayTraceEnvironments;

//-----------------------------------------------------------------------------
// Precipitation blocker entity
//-----------------------------------------------------------------------------

// Just receive the normal data table stuff
IMPLEMENT_REFLECT_CLIENTCLASS( CPrecipitationBlocker, DT_PrecipitationBlocker, CPrecipitationBlocker )

static std::vector< CPrecipitationBlocker * > g_PrecipitationBlockers;

CPrecipitationBlocker::CPrecipitationBlocker()
{
	g_PrecipitationBlockers.push_back( this );
}

CPrecipitationBlocker::~CPrecipitationBlocker()
{
	ks::erase_first( g_PrecipitationBlockers, this );
}

bool ParticleIsBlocked( const Vector &end, const Vector &start )
{
	for ( int i=0; i<std::ssize( g_PrecipitationBlockers ); ++i )
	{
		CPrecipitationBlocker *blocker = g_PrecipitationBlockers[i];
		if ( blocker->CollisionProp()->IsPointInBounds( end ) )
		{
			return true;
		}
	}

	return false;
}

// Just receive the normal data table stuff
IMPLEMENT_REFLECT_CLIENTCLASS( CClient_Precipitation, DT_Precipitation, CPrecipitation )

static ConVar r_SnowEnable( side_registry(), "r_SnowEnable", "1", FCVAR_CHEAT, "Snow Enable" );
static ConVar r_SnowParticles( side_registry(), "r_SnowParticles", "500", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowInsideRadius( side_registry(), "r_SnowInsideRadius", "256", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowOutsideRadius( side_registry(), "r_SnowOutsideRadius", "1024", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowSpeedScale( side_registry(), "r_SnowSpeedScale", "1", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowPosScale( side_registry(), "r_SnowPosScale", "1", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowFallSpeed( side_registry(), "r_SnowFallSpeed", "1.5", FCVAR_CHEAT, "Snow fall speed scale." );
static ConVar r_SnowWindScale( side_registry(), "r_SnowWindScale", "0.0035", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowDebugBox( side_registry(), "r_SnowDebugBox", "0", FCVAR_CHEAT, "Snow Debug Boxes." );
static ConVar r_SnowZoomOffset( side_registry(), "r_SnowZoomOffset", "384.0f", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowZoomRadius( side_registry(), "r_SnowZoomRadius", "512.0f", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowStartAlpha( side_registry(), "r_SnowStartAlpha", "25", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowEndAlpha( side_registry(), "r_SnowEndAlpha", "255", FCVAR_CHEAT, "Snow." );
//static ConVar r_SnowColorRed( "r_SnowColorRed", "150", FCVAR_CHEAT, "Snow." );
//static ConVar r_SnowColorGreen( "r_SnowColorGreen", "175", FCVAR_CHEAT, "Snow." );
//static ConVar r_SnowColorBlue( "r_SnowColorBlue", "200", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowStartSize( side_registry(), "r_SnowStartSize", "1", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowEndSize( side_registry(), "r_SnowEndSize", "0", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowRayLength( side_registry(), "r_SnowRayLength", "8192.0f", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowRayRadius( side_registry(), "r_SnowRayRadius", "256", FCVAR_CHEAT, "Snow." );
static ConVar r_SnowRayEnable( side_registry(), "r_SnowRayEnable", "1", FCVAR_CHEAT, "Snow." );

void DrawPrecipitation()
{
	for ( int i=0; i < std::ssize( g_Precipitations ); i++ )
	{
		g_Precipitations[i]->Render();
	}
}

//-----------------------------------------------------------------------------
// determines if a weather particle has hit something other than air
//-----------------------------------------------------------------------------
static bool IsInAir( const Vector& position )
{
	int contents = enginetrace->GetPointContents( position ); 	
	return (contents & CONTENTS_SOLID) == 0;
}

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

ConVar CClient_Precipitation::s_raindensity( side_registry(), "r_raindensity","0.001", FCVAR_CHEAT);
ConVar CClient_Precipitation::s_rainwidth( side_registry(), "r_rainwidth", "0.5", FCVAR_CHEAT );
ConVar CClient_Precipitation::s_rainlength( side_registry(), "r_rainlength", "0.1f", FCVAR_CHEAT );
ConVar CClient_Precipitation::s_rainspeed( side_registry(), "r_rainspeed", "600.0f", FCVAR_CHEAT );
ConVar r_rainalpha( side_registry(), "r_rainalpha", "0.4", FCVAR_CHEAT );
ConVar r_rainalphapow( side_registry(), "r_rainalphapow", "0.8", FCVAR_CHEAT );

Vector CClient_Precipitation::s_WindVector;		// Stores the wind speed vector

void CClient_Precipitation::OnDataChanged( DataUpdateType_t updateType )
{
	// Simulate every frame.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		if ( m_nPrecipType == PRECIPITATION_TYPE_SNOWFALL )
		{
			SnowFallManagerCreate( this );
		}
	}

	m_flDensity = RemapVal( GetRenderAlpha(), 0, 100, 0, 1 );

	BaseClass::OnDataChanged( updateType );
}

void CClient_Precipitation::ClientThink()
{
	Simulate( gpGlobals->frametime );
}

//-----------------------------------------------------------------------------
//
// Utility methods for the various simulation functions
//
//-----------------------------------------------------------------------------
bool CClient_Precipitation::SimulateRain( CPrecipitationParticle* pParticle, float dt )
{
	if (GetRemainingLifetime( pParticle ) < 0.0f)
		return false;

	Vector vOldPos = pParticle->m_Pos;

	// Update position
	VectorMA( pParticle->m_Pos, dt, pParticle->m_Velocity, 
				pParticle->m_Pos );

	if (cl_windspeed.GetFloat() > 0) // determines if s_WindVector is zeroes
	{
		// wind blows rain around
		float drift = 5 / pParticle->m_Mass;
		for ( int i = 0 ; i < 2 ; i++ ) // X and Y components
		{
			float vel = pParticle->m_Velocity[i];
			float wind = s_WindVector[i];
			if ( vel < wind )
			{
				vel = std::min( vel+drift, wind);
			}
			else if ( vel > wind )
			{
				vel = std::max( vel-drift, wind);
			}
			pParticle->m_Velocity[i] = vel;
		}
	}

	// Left4Dead does not use rain splashes on water surfaces
	// This change could allow rain into some solids, but the code
	// already performed a ray-test to calculate the particle lifetime,
	// so it should still be blocked by normal bsp surfaces.
	if (r_RainCheck.GetInt() != 0)
	{
		// No longer in the air? punt.
		if ( !IsInAir( pParticle->m_Pos ) )
		{
			// Possibly make a splash if we hit a water surface and it's in front of the view.
			if ( std::ssize( m_Splashes ) < 20 )
			{
				if ( RandomInt( 0, 100 ) < r_RainSplashPercentage.GetInt() )
				{
					trace_t trace;
					UTIL_TraceLine(vOldPos, pParticle->m_Pos, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &trace);
					if( trace.fraction < 1 )
					{
						m_Splashes.push_back( trace.endpos );
					}
				}
			}

			// Tell the framework it's time to remove the particle from the list
			return false;
		}
	}

	// We still want this particle
	return true;
}

bool CClient_Precipitation::SimulateSnow( CPrecipitationParticle* pParticle, float dt )
{
	if ( IsInAir( pParticle->m_Pos ) )
	{
		// Update position
		VectorMA( pParticle->m_Pos, dt, pParticle->m_Velocity, 
					pParticle->m_Pos );

		// wind blows rain around
		for ( int i = 0 ; i < 2 ; i++ )
		{
			if ( pParticle->m_Velocity[i] < s_WindVector[i] )
			{
				pParticle->m_Velocity[i] += ( 5.0f / pParticle->m_Mass );

				// accelerating flakes get a trail
				pParticle->m_Ramp = 0.5f;

				// clamp
				if ( pParticle->m_Velocity[i] > s_WindVector[i] )
					pParticle->m_Velocity[i] = s_WindVector[i];
			}
			else if (pParticle->m_Velocity[i] > s_WindVector[i] )
			{
				pParticle->m_Velocity[i] -= ( 5.0f / pParticle->m_Mass );

				// accelerating flakes get a trail
				pParticle->m_Ramp = 0.5f;

				// clamp.
				if ( pParticle->m_Velocity[i] < s_WindVector[i] )
					pParticle->m_Velocity[i] = s_WindVector[i];
			}
		}

		return true;
	}

	// Kill the particle immediately!
	return false;
}

void CClient_Precipitation::Simulate( float dt )
{
	if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAIN ||
		 m_nPrecipType == PRECIPITATION_TYPE_PARTICLEASH ||
		 m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAINSTORM ||
		 m_nPrecipType == PRECIPITATION_TYPE_PARTICLESNOW )
	{
		CreateParticlePrecip();
		return;
	}

	// NOTE: When client-side prechaching works, we need to remove this
	Precache();

	m_flHalfScreenWidth = (float)ScreenWidth() / 2;

	// Our sim methods needs dt	and wind vector
	if ( dt )
	{
		ComputeWindVector( );
	}

	if ( m_nPrecipType == PRECIPITATION_TYPE_ASH )
	{
		CreateAshParticle();
		return;
	}

	// The snow fall manager handles the simulation.
	if ( m_nPrecipType == PRECIPITATION_TYPE_SNOWFALL )
		return;

	// calculate the max amount of time it will take this flake to fall.
	// This works if we assume the wind doesn't have a z component
	if ( r_RainHack.GetInt() )
		m_Lifetime = (GetClientWorldEntity()->m_WorldMaxs[2] - GetClientWorldEntity()->m_WorldMins[2]) / m_Speed;
	else
		m_Lifetime = (WorldAlignMaxs()[2] - WorldAlignMins()[2]) / m_Speed;

	if ( !r_RainSimulate.GetInt() )
		return;

	CFastTimer timer;
	timer.Start();

	// Emit new particles
	EmitParticles( dt );

	// Simulate all the particles.
	if ( m_nPrecipType == PRECIPITATION_TYPE_RAIN )
	{
		std::erase_if( m_Particles, [this, dt]( CPrecipitationParticle &p ) { return !SimulateRain( &p, dt ); } );
	}
	else if ( m_nPrecipType == PRECIPITATION_TYPE_SNOW )
	{
		std::erase_if( m_Particles, [this, dt]( CPrecipitationParticle &p ) { return !SimulateSnow( &p, dt ); } );
	}

	if ( r_RainProfile.GetInt() )
	{
		timer.End();
		engine->Con_NPrintf( 15, "Rain simulation: %du (%d tracers)", timer.GetDuration().GetMicroseconds(), (int)m_Particles.size() );
	}
}

//-----------------------------------------------------------------------------
// tracer rendering
//-----------------------------------------------------------------------------

void CClient_Precipitation::RenderParticle( CPrecipitationParticle* pParticle, CMeshBuilder &mb )
{
	float scale;
	Vector start, delta;

	if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAIN || m_nPrecipType == PRECIPITATION_TYPE_PARTICLEASH
		|| m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAINSTORM || m_nPrecipType == PRECIPITATION_TYPE_PARTICLESNOW )
		return;

	if ( m_nPrecipType == PRECIPITATION_TYPE_ASH )
		 return;

	if ( m_nPrecipType == PRECIPITATION_TYPE_SNOWFALL )
		 return;

	// make streaks 0.1 seconds long, but prevent from going past end
	float lifetimeRemaining = GetRemainingLifetime( pParticle );
	if (lifetimeRemaining >= GetLength())
		scale = GetLength() * pParticle->m_Ramp;
	else
		scale = lifetimeRemaining * pParticle->m_Ramp;
	
	// NOTE: We need to do everything in screen space
	Vector3DMultiplyPosition( CurrentWorldToViewMatrix(), pParticle->m_Pos, start );
	if ( start.z > -1 )
		return;

	Vector3DMultiply( CurrentWorldToViewMatrix(), pParticle->m_Velocity, delta );

	// give a spiraling pattern to snow particles
	if ( m_nPrecipType == PRECIPITATION_TYPE_SNOW )
	{
		Vector spiral, camSpiral;
		float s, c;

		if ( pParticle->m_Mass > 1.0f )
		{
			SinCos( gpGlobals->curtime * std::numbers::pi * (1+pParticle->m_Mass * 0.1f) + 
					pParticle->m_Mass * 5.0f, &s , &c );

			// only spiral particles with a mass > 1, so some fall straight down
			spiral[0] = 28 * c;
			spiral[1] = 28 * s;
			spiral[2] = 0.0f;

			Vector3DMultiply( CurrentWorldToViewMatrix(), spiral, camSpiral );

			// X and Y are measured in world space; need to convert to camera space
			VectorAdd( start, camSpiral, start );
			VectorAdd( delta, camSpiral, delta );
		}

		// shrink the trails on spiraling flakes.
		pParticle->m_Ramp = 0.3f;
	}

	delta[0] *= scale;
	delta[1] *= scale;
	delta[2] *= scale;

	// See c_tracer.* for this method
	float flAlpha = r_rainalpha.GetFloat();
	float flWidth = GetWidth();

	float flScreenSpaceWidth = flWidth * m_flHalfScreenWidth / -start.z;
	if ( flScreenSpaceWidth < MIN_SCREENSPACE_RAIN_WIDTH )
	{
		// Make the rain tracer at least the min size, but fade its alpha the smaller it gets.
		flAlpha *= flScreenSpaceWidth / MIN_SCREENSPACE_RAIN_WIDTH;
		flWidth = MIN_SCREENSPACE_RAIN_WIDTH * -start.z / m_flHalfScreenWidth;
	}
	flAlpha = std::pow( flAlpha, r_rainalphapow.GetFloat() );

	float flColor[4] = { 1, 1, 1, flAlpha };
	Tracer_Draw( &mb, start, delta, flWidth, flColor, 1 );
}

void CClient_Precipitation::CreateWaterSplashes()
{
	for ( int i=0; i < std::ssize( m_Splashes ); i++ )
	{
		Vector vSplash = m_Splashes[i];
		
		if ( CurrentViewForward().Dot( vSplash - CurrentViewOrigin() ) > 1 )
		{
			FX_WaterRipple( vSplash, g_flSplashScale, &g_vSplashColor, g_flSplashLifetime, g_flSplashAlpha );
		}
	}
	m_Splashes.clear(); m_Splashes.shrink_to_fit();
}

void CClient_Precipitation::Render()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( !r_DrawRain.GetInt() )
		return;

	if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAIN || m_nPrecipType == PRECIPITATION_TYPE_PARTICLEASH 
		|| m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAINSTORM || m_nPrecipType == PRECIPITATION_TYPE_PARTICLESNOW )
		return;

	// Don't render in monitors or in reflections or refractions.
	if ( CurrentViewID() == VIEW_MONITOR )
		return;

	if ( view->GetDrawFlags() & (DF_RENDER_REFLECTION | DF_RENDER_REFRACTION) )
		return;

	if ( m_nPrecipType == PRECIPITATION_TYPE_ASH )
		return;

	if ( m_nPrecipType == PRECIPITATION_TYPE_SNOWFALL )
		return;

	// Create any queued up water splashes.
	CreateWaterSplashes();

	CFastTimer timer;
	timer.Start();

	CMatRenderContextPtr pRenderContext( materials );
	
	// We want to do our calculations in view space.
	VMatrix	tempView;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &tempView );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	// Force the user clip planes to use the old view matrix
	pRenderContext->EnableUserClipTransformOverride( true );
	pRenderContext->UserClipTransform( tempView );

	// Draw all the rain tracers.
	pRenderContext->Bind( m_MatHandle );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	if ( pMesh )
	{
		CMeshBuilder mb;
		mb.Begin( pMesh, MATERIAL_QUADS, (int)m_Particles.size() );

		for ( CPrecipitationParticle &particle : m_Particles )
		{
			CPrecipitationParticle *p = &particle;
			if ( p->m_nSplitScreenPlayerSlot != nSlot )
				continue;
			RenderParticle( p, mb );
		}

		mb.End( false, true );
	}

	pRenderContext->EnableUserClipTransformOverride( false );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( tempView );

	if ( r_RainProfile.GetInt() )
	{
		timer.End();
		engine->Con_NPrintf( 16, "Rain render    : %du", timer.GetDuration().GetMicroseconds() );
	}
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

CClient_Precipitation::CClient_Precipitation() : m_Remainder(0.0f)
{
	m_nPrecipType = PRECIPITATION_TYPE_RAIN;
	m_MatHandle = INVALID_MATERIAL_HANDLE;
	m_flHalfScreenWidth = 1;
	
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_pParticlePrecipInnerNear[ i ] = nullptr;
		m_pParticlePrecipInnerFar[ i ] = nullptr;
		m_pParticlePrecipOuter[ i ] = nullptr;
		m_bActiveParticlePrecipEmitter[ i ] = false;
	}

	g_Precipitations.push_back( this );
}

CClient_Precipitation::~CClient_Precipitation()
{
	ks::erase_first( g_Precipitations, this );
	SnowFallManagerDestroy();
}

//-----------------------------------------------------------------------------
// Precache data
//-----------------------------------------------------------------------------

static constexpr auto SNOW_SPEED = 80.0f;
static constexpr auto RAIN_SPEED = 425.0f;

static constexpr auto RAIN_TRACER_WIDTH = 0.35f;
static constexpr auto SNOW_TRACER_WIDTH = 0.7f;

void CClient_Precipitation::Precache( )
{
	if ( !m_MatHandle )
	{
		// Compute precipitation emission speed
		switch( m_nPrecipType )
		{
		case PRECIPITATION_TYPE_SNOW:
			m_Speed	= SNOW_SPEED;
			m_MatHandle = materials->FindMaterial( "particle/snow", TEXTURE_GROUP_CLIENT_EFFECTS );
			m_InitialRamp = 0.6f;
			m_Width = SNOW_TRACER_WIDTH;
			break;

		case PRECIPITATION_TYPE_RAIN:
			contract_assert( m_nPrecipType == PRECIPITATION_TYPE_RAIN );
			m_Speed	= RAIN_SPEED;
			m_MatHandle = materials->FindMaterial( "particle/rain", TEXTURE_GROUP_CLIENT_EFFECTS );
			m_InitialRamp = 1.0f;
			m_Color[3] = 1.0f;	// make translucent
			m_Width = RAIN_TRACER_WIDTH;
			break;
		default:
			m_InitialRamp = 1.0f;
			m_Color[3] = 1.0f;	// make translucent
			break;
		}

		// Store off the color
		m_Color[0] = 1.0f;
		m_Color[1] = 1.0f;
		m_Color[2] = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Gets the tracer width and speed
//-----------------------------------------------------------------------------

float CClient_Precipitation::GetWidth() const
{
//	return m_Width;
	return s_rainwidth.GetFloat();
}

float CClient_Precipitation::GetLength() const
{
//	return m_Length;
	return s_rainlength.GetFloat();
}

float CClient_Precipitation::GetSpeed() const
{
//	return m_Speed;
	return s_rainspeed.GetFloat();
}

//-----------------------------------------------------------------------------
// Gets the remaining lifetime of the particle
//-----------------------------------------------------------------------------

float CClient_Precipitation::GetRemainingLifetime( CPrecipitationParticle* pParticle ) const
{
	float timeSinceSpawn = gpGlobals->curtime - pParticle->m_SpawnTime;
	return pParticle->m_flMaxLifetime - timeSinceSpawn;	// TERROR: use per-particle lifetime not dependent on func_precipitation lower bound
}

//-----------------------------------------------------------------------------
// Creates a particle
//-----------------------------------------------------------------------------

CPrecipitationParticle* CClient_Precipitation::CreateParticle()
{
	CPrecipitationParticle* pParticle = &m_Particles.emplace_back();

	pParticle->m_SpawnTime = gpGlobals->curtime;
	pParticle->m_Ramp = m_InitialRamp;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	pParticle->m_nSplitScreenPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	return pParticle;
}

//-----------------------------------------------------------------------------
// Compute the emission area
//-----------------------------------------------------------------------------

bool CClient_Precipitation::ComputeEmissionArea( Vector& origin, Vector2D& size, CBaseCombatCharacter *pCharacter )
{
	// calculate a volume around the player to snow in. Intersect this big magic
	// box around the player with the volume of the current environmental ent.

	if ( !pCharacter )
		return false;

	// FIXME: Compute the precipitation area based on computational power
	float emissionSize = r_RainRadius.GetFloat();	// size of box to emit particles in

	Vector vMins = WorldAlignMins();
	Vector vMaxs = WorldAlignMaxs();
	if ( r_RainHack.GetInt() )
	{
		vMins = GetClientWorldEntity()->m_WorldMins;
		vMaxs = GetClientWorldEntity()->m_WorldMaxs;
	}

	// Determine how much time it'll take a falling particle to hit the player
	float emissionHeight = std::min( vMaxs[2], pCharacter->GetAbsOrigin()[2] + 512 );
	float distToFall = emissionHeight - pCharacter->GetAbsOrigin()[2];
	float fallTime = distToFall / GetSpeed();
	
	// Based on the windspeed, figure out the center point of the emission
	Vector2D center;
	center[0] = pCharacter->GetAbsOrigin()[0] - fallTime * s_WindVector[0];
	center[1] = pCharacter->GetAbsOrigin()[1] - fallTime * s_WindVector[1];

	Vector2D lobound, hibound;
	lobound[0] = center[0] - emissionSize * 0.5f;
	lobound[1] = center[1] - emissionSize * 0.5f;
	hibound[0] = lobound[0] + emissionSize;
	hibound[1] = lobound[1] + emissionSize;

	// Cull non-intersecting.
	if ( ( vMaxs[0] < lobound[0] ) || ( vMaxs[1] < lobound[1] ) ||
		 ( vMins[0] > hibound[0] ) || ( vMins[1] > hibound[1] ) )
		return false;

	origin[0] = std::max( vMins[0], lobound[0] );
	origin[1] = std::max( vMins[1], lobound[1] );
	origin[2] = emissionHeight;

	hibound[0] = std::min( vMaxs[0], hibound[0] );
	hibound[1] = std::min( vMaxs[1], hibound[1] );

	size[0] = hibound[0] - origin[0];
	size[1] = hibound[1] - origin[1];

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDebugName - 
// Output : AshDebrisEffect*
//-----------------------------------------------------------------------------
AshDebrisEffect* AshDebrisEffect::Create( const char *pDebugName )
{
	return new AshDebrisEffect( pDebugName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float AshDebrisEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

static constexpr auto ASH_PARTICLE_NOISE = 0x4;

float AshDebrisEffect::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	float flRoll = CSimpleEmitter::UpdateRoll(pParticle, timeDelta );

	if ( pParticle->m_iFlags & ASH_PARTICLE_NOISE )
	{
		Vector vTempEntVel = pParticle->m_vecVelocity;
		float fastFreq = gpGlobals->curtime * 1.5;

		float s, c;
		SinCos( fastFreq, &s, &c );

		pParticle->m_Pos = ( pParticle->m_Pos + Vector(
			vTempEntVel[0] * timeDelta * s,
			vTempEntVel[1] * timeDelta * s, 0 ) );
	}

	return flRoll;
}

void CClient_Precipitation::CreateAshParticle( void )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		// Make sure the emitter is setup
		if ( m_Ash[ hh ].m_pAshEmitter == nullptr )
		{
			if ( ( m_Ash[ hh ].m_pAshEmitter = AshDebrisEffect::Create( "ashtray" ) ) == nullptr )
				continue;

			m_Ash[ hh ].m_tAshParticleTimer.Init( 192 );
			m_Ash[ hh ].m_tAshParticleTraceTimer.Init( 15 );
			m_Ash[ hh ].m_bActiveAshEmitter = false;
			m_Ash[ hh ].m_iAshCount = 0;
			m_Ash[ hh ].m_pAshEmitter->SetShouldDrawForSplitScreenUser( hh );
		}

		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		if ( pPlayer == nullptr )
			continue;

		Vector vForward;
		pPlayer->GetVectors( &vForward, nullptr, nullptr );
		vForward.z = 0.0f;

		float curTime = gpGlobals->frametime;

		Vector vPushOrigin;

		Vector absmins = WorldAlignMins();
		Vector absmaxs = WorldAlignMaxs();

		//15 Traces a second.
		while ( m_Ash[ hh ].m_tAshParticleTraceTimer.NextEvent( curTime ) )
		{
			trace_t tr;

			Vector vTraceStart = pPlayer->EyePosition();
			Vector vTraceEnd = pPlayer->EyePosition() + vForward * MAX_TRACE_LENGTH;

			UTIL_TraceLine( vTraceStart, vTraceEnd, MASK_SHOT_HULL & (~CONTENTS_GRATE), pPlayer, COLLISION_GROUP_NONE, &tr );
			//UTIL_TraceModel( vTraceStart, vTraceEnd, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), this, COLLISION_GROUP_NONE, &tr );
			//debugoverlay->AddLineOverlay( vTraceStart, tr.endpos, 255, 0, 0, 0, 0.2 );

			if ( tr.fraction != 1.0f )
			{
				trace_t tr2;

				UTIL_TraceModel( vTraceStart, tr.endpos, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), this, COLLISION_GROUP_NONE, &tr2 );

				if ( tr2.Ent<CBaseEntity>() == this )
				{
					m_Ash[ hh ].m_bActiveAshEmitter = true;

					if ( tr2.startsolid == false )
					{
						m_Ash[ hh ].m_vAshSpawnOrigin = tr2.endpos + vForward * 256;
					}
					else
					{
						m_Ash[ hh ].m_vAshSpawnOrigin = vTraceStart;
					}
				}
				else
				{
					m_Ash[ hh ].m_bActiveAshEmitter = false;
				}
			}
		}

		if ( m_Ash[ hh ].m_bActiveAshEmitter == false )
			 continue;

		Vector vecVelocity = pPlayer->GetAbsVelocity();

		float flVelocity = VectorNormalize( vecVelocity );
		Vector offset = m_Ash[ hh ].m_vAshSpawnOrigin;

		m_Ash[ hh ].m_pAshEmitter->SetSortOrigin( offset );

		PMaterialHandle	hMaterial[4];
		hMaterial[0] = ParticleMgr()->GetPMaterial( "effects/fleck_ash1" );
		hMaterial[1] = ParticleMgr()->GetPMaterial( "effects/fleck_ash2" );
		hMaterial[2] = ParticleMgr()->GetPMaterial( "effects/fleck_ash3" );
		hMaterial[3] = ParticleMgr()->GetPMaterial( "effects/ember_swirling001" );

		SimpleParticle	*pParticle;

		Vector vSpawnOrigin = vec3_origin;

		if ( flVelocity > 0 )
		{
			vSpawnOrigin = ( vForward * 256 ) + ( vecVelocity * ( flVelocity * 2 ) );
		}

		// Add as many particles as we need
		while ( m_Ash[ hh ].m_tAshParticleTimer.NextEvent( curTime ) )
		{
			int iRandomAltitude = RandomInt( 0, 128 );

			offset = m_Ash[ hh ].m_vAshSpawnOrigin + vSpawnOrigin + RandomVector( -256, 256 );
			offset.z = m_Ash[ hh ].m_vAshSpawnOrigin.z + iRandomAltitude;

			if  (  offset[0] > absmaxs[0]
			|| offset[1] > absmaxs[1]
			|| offset[2] > absmaxs[2]
			|| offset[0] < absmins[0]
			|| offset[1] < absmins[1]
			|| offset[2] < absmins[2] )
				continue;

			m_Ash[ hh ].m_iAshCount++;

			bool bEmberTime = false;
			
			if ( m_Ash[ hh ].m_iAshCount >= 250 )
			{
				bEmberTime = true;
				m_Ash[ hh ].m_iAshCount = 0;
			}

			int iRandom = random_valve->RandomInt(0,2);

			if ( bEmberTime == true )
			{
				offset = m_Ash[ hh ].m_vAshSpawnOrigin + (vForward * 256) + RandomVector( -128, 128 );
				offset.z = pPlayer->EyePosition().z + RandomFloat( -16, 64 );

				iRandom = 3;
			}

			pParticle = (SimpleParticle *)m_Ash[ hh ].m_pAshEmitter->AddParticle( sizeof(SimpleParticle), hMaterial[iRandom], offset );

			if (pParticle == nullptr)
				continue; 

			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime	= RemapVal( iRandomAltitude, 0, 128, 4, 8 );

			if ( bEmberTime == true )
			{
				Vector vGoal = pPlayer->EyePosition() + RandomVector( -64, 64 );
				Vector vDir = vGoal - offset;
				VectorNormalize( vDir );

				pParticle->m_vecVelocity = vDir * 75;
				pParticle->m_flDieTime = 2.5f;
			}
			else
			{
				pParticle->m_vecVelocity = Vector( RandomFloat( -20.0f, 20.0f ), RandomFloat( -20.0f, 20.0f ), RandomFloat( -10, -15 ) );
			}

			float color = random_valve->RandomInt( 125, 225 );
			pParticle->m_uchColor[0] = color;
			pParticle->m_uchColor[1] = color;
			pParticle->m_uchColor[2] = color;

			pParticle->m_uchStartSize	= 1;
			pParticle->m_uchEndSize		= 1.5;

			pParticle->m_uchStartAlpha	= 255;

			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -0.15f, 0.15f );

			pParticle->m_iFlags			= SIMPLE_PARTICLE_FLAG_WINDBLOWN;

			if ( random_valve->RandomInt( 0, 10 ) <= 1 )
			{
				pParticle->m_iFlags |= ASH_PARTICLE_NOISE;
			}
		}
	}
}

void CClient_Precipitation::CreateParticlePrecip( void )
{
	if ( !r_RainAllowInSplitScreen.GetBool() && engine->IsSplitScreenActive() )
	{	
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			if ( m_pParticlePrecipOuter[hh] != nullptr )
			{
				DestroyInnerParticlePrecip( hh );
				DestroyOuterParticlePrecip( hh );
			}
		}
		return;
	}

	if ( !m_bParticlePrecipInitialized )
	{
		InitializeParticlePrecip();
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		CCSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();

		if ( pPlayer == nullptr )
			continue;

		// Make sure the emitter is setup
		if ( !m_bActiveParticlePrecipEmitter[hh] )
		{
			//Update 8 times per second.
			m_tParticlePrecipTraceTimer[hh].Init( 8 );
			DestroyInnerParticlePrecip( hh );
			DestroyOuterParticlePrecip( hh );
			m_bActiveParticlePrecipEmitter[hh] = true;
		}

		UpdateParticlePrecip( pPlayer, hh );
	}
}

void CClient_Precipitation::UpdateParticlePrecip( CBasePlayer *pPlayer, int nSlot )
{
	if ( !pPlayer )
		return;

	Vector vForward;
	Vector vRight;

	pPlayer->GetVectors( &vForward, &vRight, nullptr );
	vForward.z = 0.0f;
	vForward.NormalizeInPlace();
	Vector vForward45Right = vForward + vRight;
	Vector vForward45Left = vForward - vRight;
	vForward45Right.NormalizeInPlace();
	vForward45Left.NormalizeInPlace();
	fltx4 TMax = ReplicateX4( 320.0f );
	SubFloat( TMax, 3 ) = std::numeric_limits<float>::max();
	float curTime = gpGlobals->frametime;

	while ( m_tParticlePrecipTraceTimer[nSlot].NextEvent( curTime ) )
	{
		Vector vPlayerPos = pPlayer->EyePosition();
		Vector vOffsetPos = vPlayerPos + Vector ( 0, 0, 180 );
		Vector vOffsetPosNear = vPlayerPos + Vector ( 0, 0, 180 ) + ( vForward * 32 );
		Vector vOffsetPosFar = vPlayerPos + Vector ( 0, 0, 180 ) + ( vForward * 100 );	

		Vector vDensity = Vector( r_RainParticleDensity.GetFloat(), 0, 0 ) * m_flDensity;

		// Get the rain volume Ray Tracing Environment.  Currently hard coded to 0, should have this lookup 
		RayTracingEnvironment *RtEnv = g_RayTraceEnvironments[ 0 ].get();

		// Our 4 Rays are forward, off to the left and right, and directly up.
		// Use the first three to determine if there's generally visible rain where we're looking.
		// The forth, straight up, tells us if we're standing inside a rain volume 
		// (based on the normal that we hit or if we miss entirely)
		FourRays frRays;
		FourVectors fvDirection;
		fvDirection = FourVectors( vForward, vForward45Left, vForward45Right, Vector( 0, 0, 1 ) );
		frRays.direction = fvDirection; 
		frRays.origin.DuplicateVector( vPlayerPos );
		RayTracingResult Result;

		RtEnv->Trace4Rays( frRays, Four_Zeros, TMax, &Result );

		i32x4 in4HitIds = LoadAlignedIntSIMD( Result.HitIds );
		fltx4 fl4HitIds = SignedIntConvertToFltSIMD ( in4HitIds );

		fltx4 fl4Tolerance = ReplicateX4( 300.0f );
		// ignore upwards test for tolerance, as we may be below an area which is raining, but with it not visible in front of us
		//SubFloat( fl4Tolerance, 3 ) = 0.0f;

		bool bInside = ( Result.HitIds[3] != -1 && Result.surface_normal.Vec( 3 ).z < 0.0f );
		bool bNearby = (  IsAnyTrue( CmpGeSIMD ( fl4HitIds, Four_Zeros ) ) && IsAnyTrue( CmpGeSIMD( fl4Tolerance, Result.HitDistance ) ) );

		if ( bInside || bNearby )
		{
			//We can see a rain volume, but it's farther than 180 units away, only use far effect.
			if ( !bInside && SubFloat( FindLowestSIMD3( Result.HitDistance ), 0 ) >= m_flParticleInnerDist )
			{
				// Kill the inner rain if it's previously been in use
				if ( m_pParticlePrecipInnerNear[nSlot] != nullptr )
				{
					DestroyInnerParticlePrecip( nSlot );
				}
				// Update if we've already got systems, otherwise, create them.
				if ( m_pParticlePrecipOuter[nSlot] != nullptr )
				{
					m_pParticlePrecipOuter[nSlot]->SetControlPoint( 1,  vOffsetPos );
					m_pParticlePrecipOuter[nSlot]->SetControlPoint( 3, vDensity );
				}
				else
				{
					DispatchOuterParticlePrecip( nSlot, pPlayer, vForward );
				}
			}
			else   //We're close enough to use the near effect.
			{
				// Update if we've already got systems, otherwise, create them.
				if ( m_pParticlePrecipInnerNear[nSlot] != nullptr  && m_pParticlePrecipInnerFar[nSlot] != nullptr  &&  m_pParticlePrecipOuter[nSlot] != nullptr )
				{
					m_pParticlePrecipOuter[nSlot]->SetControlPoint( 1, vOffsetPos );
					m_pParticlePrecipInnerNear[nSlot]->SetControlPoint( 1, vOffsetPosNear );
					m_pParticlePrecipInnerFar[nSlot]->SetControlPoint( 1, vOffsetPosFar );
					m_pParticlePrecipInnerNear[nSlot]->SetControlPoint( 3, vDensity );
					m_pParticlePrecipInnerFar[nSlot]->SetControlPoint( 3, vDensity );
					m_pParticlePrecipOuter[nSlot]->SetControlPoint( 3, vDensity );
				}
				else
				{
					DispatchInnerParticlePrecip( nSlot, pPlayer, vForward );
				}
			}
		}
		else  // No rain in the area, kill any leftover systems.
		{
			DestroyInnerParticlePrecip( nSlot );
			DestroyOuterParticlePrecip( nSlot );
		}
	}
}

void CClient_Precipitation::InitializeParticlePrecip( void )
{
	//Set up which type of precipitation particle we'll use
	if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLEASH )
	{
		m_pParticleInnerNearDef = "ash";
		m_pParticleInnerFarDef = "ash";
		m_pParticleOuterDef = "ash_outer";
		m_flParticleInnerDist = 280.0;
	}
	else if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLESNOW )
	{
		m_pParticleInnerNearDef = "snow";
		m_pParticleInnerFarDef = "snow";
		m_pParticleOuterDef = "snow_outer";
		m_flParticleInnerDist = 280.0;
	}
	else if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAINSTORM )
	{
		m_pParticleInnerNearDef = "rain_storm";
		m_pParticleInnerFarDef = "rain_storm_screen";
		m_pParticleOuterDef = "rain_storm_outer";
		m_flParticleInnerDist = 0.0;
	}
	else  //default to rain
	{
		m_pParticleInnerNearDef = "rain";
		m_pParticleInnerFarDef = "rain";
		m_pParticleOuterDef = "rain_outer";
		m_flParticleInnerDist = 180.0;
	}

	contract_assert( m_pParticleInnerFarDef != nullptr );

	//We'll want to change this if/when we add more raytrace environments.
	g_RayTraceEnvironments.clear();

	// Sets up ray tracing environments for all func_precipitations and func_precipitation_blockers
	RayTracingEnvironment *rtEnvRainEmission = g_RayTraceEnvironments.emplace_back( std::make_unique< RayTracingEnvironment >() ).get();
	RayTracingEnvironment *rtEnvRainBlocker = g_RayTraceEnvironments.emplace_back( std::make_unique< RayTracingEnvironment >() ).get();

	rtEnvRainEmission->Flags |= RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS;	// save some ram
	rtEnvRainBlocker->Flags |= RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS;		// save some ram

	int nTriCount = 1;
	for ( int i=0; i<std::ssize( g_Precipitations ); ++i )
	{
		CClient_Precipitation *volume = g_Precipitations[i];

		vcollide_t *pCollide = modelinfo->GetVCollide( volume->GetModelIndex() );

		if ( !pCollide || pCollide->solidCount <= 0 )
			continue;

		Vector *outVerts;
		int vertCount = g_pPhysicsCollision->CreateDebugMesh( pCollide->solids[0], &outVerts );

		if ( vertCount )
		{
			for ( int j = 0; j < vertCount; j += 3 )
			{
				rtEnvRainEmission->AddTriangle( nTriCount++, outVerts[j], outVerts[j + 1], outVerts[j + 2], Vector( 1, 1, 1 ) );
			}
		}
		physcollision->DestroyDebugMesh( vertCount, outVerts );
	}
	rtEnvRainEmission->SetupAccelerationStructure();

	nTriCount = 1;

	for ( int i=0; i<std::ssize( g_PrecipitationBlockers ); ++i )
	{
		CPrecipitationBlocker *blocker = g_PrecipitationBlockers[i];

		vcollide_t *pCollide = modelinfo->GetVCollide( blocker->GetModelIndex() );

		if ( !pCollide || pCollide->solidCount <= 0 )
			continue;

		Vector *outVerts;
		int vertCount = g_pPhysicsCollision->CreateDebugMesh( pCollide->solids[0], &outVerts );

		if ( vertCount )
		{
			for ( int j = 0; j < vertCount; j += 3 )
			{
				rtEnvRainBlocker->AddTriangle( nTriCount++, outVerts[j], outVerts[j + 1], outVerts[j + 2], Vector( 1, 1, 1 ) );
			}
		}
		physcollision->DestroyDebugMesh( vertCount, outVerts );
	}

	rtEnvRainBlocker->SetupAccelerationStructure();

	m_bParticlePrecipInitialized = true;
}

void CClient_Precipitation::DestroyInnerParticlePrecip( int nSlot )
{
	if ( m_pParticlePrecipInnerFar[nSlot] != nullptr )
	{
		m_pParticlePrecipInnerFar[nSlot]->StopEmission();
		m_pParticlePrecipInnerFar[nSlot] = nullptr;
	}
	if ( m_pParticlePrecipInnerNear[nSlot] != nullptr )
	{
		m_pParticlePrecipInnerNear[nSlot]->StopEmission();
		m_pParticlePrecipInnerNear[nSlot] = nullptr;
	}
}

void CClient_Precipitation::DestroyOuterParticlePrecip( int nSlot )
{
	if ( m_pParticlePrecipOuter[nSlot] != nullptr )
	{
		m_pParticlePrecipOuter[nSlot]->StopEmission();
		m_pParticlePrecipOuter[nSlot] = nullptr;
	}
}

void CClient_Precipitation::DispatchOuterParticlePrecip( int nSlot, CBasePlayer *pPlayer, Vector vForward )
{
	DestroyOuterParticlePrecip( nSlot );

	Vector vDensity = Vector( r_RainParticleDensity.GetFloat(), 0, 0 ) * m_flDensity;
	Vector vPlayerPos = pPlayer->EyePosition();

	m_pParticlePrecipOuter[nSlot] = ParticleProp()->Create( m_pParticleOuterDef, PATTACH_ABSORIGIN_FOLLOW );
	m_pParticlePrecipOuter[nSlot]->SetControlPointEntity( 2, pPlayer );
	m_pParticlePrecipOuter[nSlot]->SetControlPoint( 1,  vPlayerPos + Vector (0, 0, 180 ) );
	m_pParticlePrecipOuter[nSlot]->SetControlPoint( 3, vDensity );
	m_pParticlePrecipOuter[nSlot]->SetDrawOnlyForSplitScreenUser( nSlot );
}

void CClient_Precipitation::DispatchInnerParticlePrecip( int nSlot, CBasePlayer *pPlayer, Vector vForward )
{
	DestroyInnerParticlePrecip( nSlot );
	DestroyOuterParticlePrecip( nSlot );
	Vector vPlayerPos = pPlayer->EyePosition();
	Vector vOffsetPos = vPlayerPos + Vector ( 0, 0, 180 );
	Vector vOffsetPosNear = vPlayerPos + Vector ( 0, 0, 180 ) + ( vForward * 32 );
	Vector vOffsetPosFar = vPlayerPos + Vector ( 0, 0, 180 ) + ( vForward * m_flParticleInnerDist );  // 100.0
	Vector vDensity = Vector( r_RainParticleDensity.GetFloat(), 0, 0 ) * m_flDensity;
	
	m_pParticlePrecipOuter[nSlot] = ParticleProp()->Create( m_pParticleOuterDef, PATTACH_ABSORIGIN_FOLLOW );
	m_pParticlePrecipInnerNear[nSlot] = ParticleProp()->Create( m_pParticleInnerNearDef, PATTACH_ABSORIGIN_FOLLOW );
	m_pParticlePrecipInnerFar[nSlot] = ParticleProp()->Create( m_pParticleInnerFarDef, PATTACH_ABSORIGIN_FOLLOW );
	m_pParticlePrecipOuter[nSlot]->SetControlPointEntity( 2, pPlayer );
	m_pParticlePrecipInnerNear[nSlot]->SetControlPointEntity( 2, pPlayer );
	m_pParticlePrecipInnerFar[nSlot]->SetControlPointEntity( 2, pPlayer );
	m_pParticlePrecipOuter[nSlot]->SetControlPoint( 1,  vOffsetPos );
	m_pParticlePrecipInnerNear[nSlot]->SetControlPoint( 1,  vOffsetPosNear );
	m_pParticlePrecipInnerFar[nSlot]->SetControlPoint( 1,  vOffsetPosFar );
	m_pParticlePrecipInnerNear[nSlot]->SetControlPoint( 3, vDensity );
	m_pParticlePrecipInnerFar[nSlot]->SetControlPoint( 3, vDensity );
	m_pParticlePrecipOuter[nSlot]->SetControlPoint( 3, vDensity );
	m_pParticlePrecipOuter[nSlot]->SetDrawOnlyForSplitScreenUser( nSlot );
	m_pParticlePrecipInnerNear[nSlot]->SetDrawOnlyForSplitScreenUser( nSlot );
	m_pParticlePrecipInnerFar[nSlot]->SetDrawOnlyForSplitScreenUser( nSlot );
}

// TERROR: adding end pos for lifetime calcs
void CClient_Precipitation::CreateRainOrSnowParticle( const Vector &vSpawnPosition, const Vector &vEndPosition, const Vector &vVelocity )
{
	// Create the particle
	CPrecipitationParticle* p = CreateParticle();
	if (!p) 
		return;

	VectorCopy( vVelocity, p->m_Velocity );
	p->m_Pos = vSpawnPosition;

	/* TERROR: moving random velocity out so it can be included in endpos calcs
	p->m_Velocity[ 0 ] += random->RandomFloat(-r_RainSideVel.GetInt(), r_RainSideVel.GetInt());
	p->m_Velocity[ 1 ] += random->RandomFloat(-r_RainSideVel.GetInt(), r_RainSideVel.GetInt());
	*/

	p->m_Mass = random_valve->RandomFloat( 0.5, 1.5 );

	p->m_flMaxLifetime = fabs((vSpawnPosition.z - vEndPosition.z) / vVelocity.z);
}

//-----------------------------------------------------------------------------
// emit the precipitation particles
//-----------------------------------------------------------------------------

void CClient_Precipitation::EmitParticles( float fTimeDelta )
{
	Vector2D size;
	Vector vel, org;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
	
		CCSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();
		if ( !pPlayer )
			continue;
	
		Vector vPlayerCenter = pPlayer->WorldSpaceCenter();

		// Compute where to emit
		if (!ComputeEmissionArea( org, size, pPlayer ))
			continue;

		// clamp this to prevent creating a bunch of rain or snow at one time.
		if( fTimeDelta > 0.075f )
			fTimeDelta = 0.075f;

		// FIXME: Compute the precipitation density based on computational power
		float density = m_flDensity * 0.001;

		if (density > 0.01f) 
			density = 0.01f;

		// Compute number of particles to emit based on precip density and emission area and dt
		float fParticles = size[0] * size[1] * density * fTimeDelta + m_Remainder; 
		int cParticles = (int)fParticles;
		m_Remainder = fParticles - cParticles;

		// calculate the max amount of time it will take this flake to fall.
		// This works if we assume the wind doesn't have a z component
		VectorCopy( s_WindVector, vel );
		vel[2] -= GetSpeed();

		// Emit all the particles
		for ( int i = 0 ; i < cParticles ; i++ )
		{
			// TERROR: moving random velocity out so it can be included in endpos calcs
			Vector vParticleVel = vel;
			vParticleVel[ 0 ] += random_valve->RandomFloat(-r_RainSideVel.GetInt(), r_RainSideVel.GetInt());
			vParticleVel[ 1 ] += random_valve->RandomFloat(-r_RainSideVel.GetInt(), r_RainSideVel.GetInt());

			Vector vParticlePos = org;
			vParticlePos[ 0 ] += size[ 0 ] * random_valve->RandomFloat(0, 1);
			vParticlePos[ 1 ] += size[ 1 ] * random_valve->RandomFloat(0, 1);

			// Figure out where the particle should lie in Z by tracing a line from the player's height up to the 
			// desired height and making sure it doesn't hit a wall.
			Vector vPlayerHeight = vParticlePos;
			vPlayerHeight.z = vPlayerCenter.z;

			if ( ParticleIsBlocked( vPlayerHeight, vParticlePos ) )
			{
				if ( r_RainDebugDuration.GetBool() )
				{
					debugoverlay->AddLineOverlay( vPlayerHeight, vParticlePos, 255, 0, 0, false, r_RainDebugDuration.GetFloat() );
				}
				continue;
			}

			Vector vUnitParticleVel = vParticleVel;
			float fallHeight = vParticlePos.z - vPlayerHeight.z;
			vUnitParticleVel /= fallHeight;
			vPlayerHeight.x += vUnitParticleVel.x * fallHeight;
			vPlayerHeight.y += vUnitParticleVel.y * fallHeight;

			trace_t trace;
			UTIL_TraceLine( vPlayerHeight, vParticlePos, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &trace );
			if ( trace.fraction < 1 )
			{
				// If we hit a brush, then don't spawn the particle.
				if ( trace.surface.flags & SURF_SKY )
				{
					vParticlePos = trace.endpos;
					if ( r_RainDebugDuration.GetBool() )
					{
						debugoverlay->AddLineOverlay( vPlayerHeight, trace.endpos, 0, 0, 255, false, r_RainDebugDuration.GetFloat() );
					}
				}
				else
				{
					if ( r_RainDebugDuration.GetBool() )
					{
						debugoverlay->AddLineOverlay( vPlayerHeight, trace.endpos, 255, 0, 0, false, r_RainDebugDuration.GetFloat() );
					}
					continue;
				}
			}

			// TERROR: Find an endpos
			Vector vParticleEndPos( vPlayerHeight );
			//vParticleEndPos.z -= 256.0f;
			//UTIL_TraceLine( vPlayerHeight, vParticleEndPos, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace );
			//vParticleEndPos = trace.endpos;

			if ( r_RainDebugDuration.GetBool() )
			{
				debugoverlay->AddLineOverlay( vParticlePos, vParticleEndPos, 0, 255, 0, true, r_RainDebugDuration.GetFloat() );
			}

			CreateRainOrSnowParticle( vParticlePos, vParticleEndPos, vParticleVel );
		}
	}
}

//-----------------------------------------------------------------------------
// Computes the wind vector
//-----------------------------------------------------------------------------

void CClient_Precipitation::ComputeWindVector( )
{
	// Compute the wind direction
	QAngle windangle( 0, cl_winddir.GetFloat(), 0 );	// used to turn wind yaw direction into a vector

	// Randomize the wind angle and speed slightly to get us a little variation
	windangle[1] = windangle[1] + random_valve->RandomFloat( -10, 10 );
	float windspeed = cl_windspeed.GetFloat() * (1.0 + random_valve->RandomFloat( -0.2, 0.2 ));

	AngleVectors( windangle, &s_WindVector );
	VectorScale( s_WindVector, windspeed, s_WindVector );
}

CHandle<CClient_Precipitation> g_pPrecipHackEnt;

class CPrecipHack : public CAutoGameSystemPerFrame
{
public:
	explicit CPrecipHack( char const *name ) : CAutoGameSystemPerFrame( name )
	{
		m_bLevelInitted = false;
	}

	virtual void LevelInitPostEntity()
	{
		if ( r_RainHack.GetInt() )
		{
			CClient_Precipitation *pPrecipHackEnt = new CClient_Precipitation;
			pPrecipHackEnt->InitializeAsClientEntity( nullptr, false );
			g_pPrecipHackEnt = pPrecipHackEnt;
		}
		m_bLevelInitted = true;
	}
	
	virtual void LevelShutdownPreEntity()
	{
		if ( r_RainHack.GetInt() && g_pPrecipHackEnt )
		{
			UTIL_Remove( g_pPrecipHackEnt );
		}
		m_bLevelInitted = false;
	}

	virtual void Update( float frametime )
	{
		// Handle changes to the cvar at runtime.
		if ( m_bLevelInitted )
		{
			if ( r_RainHack.GetInt() && !g_pPrecipHackEnt )
				LevelInitPostEntity();
			else if ( !r_RainHack.GetInt() && g_pPrecipHackEnt )
				LevelShutdownPreEntity();
		}
	}

	bool m_bLevelInitted;
};
CPrecipHack g_PrecipHack( "CPrecipHack" );

//-----------------------------------------------------------------------------
// EnvWind - global wind info
//-----------------------------------------------------------------------------
class CEnvWind : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvWind" } ]]
	static constexpr bool ks_class = true;

public:
	CEnvWind();

	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEnvWind, CBaseEntity );

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	ShouldDraw( void ) { return false; }

	virtual void	ClientThink( );

private:
	CEnvWind( const CEnvWind & );

	[[= ks::reflect::Net{} ]] CEnvWindShared m_EnvWindShared;
};

// Receive datatables
IMPLEMENT_REFLECT_TABLE( CEnvWindShared, DT_EnvWindShared );

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvWind, DT_EnvWind, CEnvWind )

CEnvWind::CEnvWind()
{
}

//-----------------------------------------------------------------------------
// Post data update!
//-----------------------------------------------------------------------------
void CEnvWind::OnDataChanged( DataUpdateType_t updateType )
{
	// Whenever we get an update, reset the entire state.
	// Note that the fields have already been stored by the datatables,
	// but there's still work to be done in the init block
	m_EnvWindShared.Init( entindex(), m_EnvWindShared.m_iWindSeed, 
		m_EnvWindShared.m_flStartTime, m_EnvWindShared.m_iInitialWindDir,
		m_EnvWindShared.m_flInitialWindSpeed );

	SetNextClientThink(0.0f);

	BaseClass::OnDataChanged( updateType );
}

void CEnvWind::ClientThink( )
{
	// Update the wind speed
	float flNextThink = m_EnvWindShared.WindThink( gpGlobals->curtime );
	SetNextClientThink(flNextThink);
}

//==================================================
// EmberParticle
//==================================================

class CEmberEmitter : public CSimpleEmitter
{
public:
	explicit 				CEmberEmitter( const char *pDebugName );
	static ks::intrusive_ptr<CEmberEmitter>	Create( const char *pDebugName );
	virtual void			UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector			UpdateColor( const SimpleParticle *pParticle );

private:
							CEmberEmitter( const CEmberEmitter & );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------
CEmberEmitter::CEmberEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}

ks::intrusive_ptr<CEmberEmitter> CEmberEmitter::Create( const char *pDebugName )
{
	return new CEmberEmitter( pDebugName );
}

void CEmberEmitter::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	float	speed = VectorNormalize( pParticle->m_vecVelocity );
	Vector	offset;

	speed -= ( 1.0f * timeDelta );

	offset.Random( -0.025f, 0.025f );
	offset[2] = 0.0f;

	pParticle->m_vecVelocity += offset;
	VectorNormalize( pParticle->m_vecVelocity );

	pParticle->m_vecVelocity *= speed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector CEmberEmitter::UpdateColor( const SimpleParticle *pParticle )
{
	Vector	color;
	float	ramp = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime );

	color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
	color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
	color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

	return color;
}

//==================================================
// CEmbers
//==================================================

class CEmbers : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Embers" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEmbers, CBaseEntity );

					CEmbers();
					~CEmbers();

	void	Start( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	ShouldDraw( void );
	virtual bool	Simulate( void );

	//Server-side
	[[= ks::reflect::Net{} ]] int		m_nDensity;
	[[= ks::reflect::Net{} ]] int		m_nLifetime;
	[[= ks::reflect::Net{} ]] int		m_nSpeed;
	[[= ks::reflect::Net{} ]] bool	m_bEmit;

protected:

	void	SpawnEmber( void );

	PMaterialHandle		m_hMaterial;
	TimedEvent			m_tParticleSpawn;
	ks::intrusive_ptr<CEmberEmitter> m_pEmitter;

};

//Receive datatable
IMPLEMENT_REFLECT_CLIENTCLASS( CEmbers, DT_Embers, CEmbers )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
CEmbers::CEmbers()
{
	m_pEmitter = CEmberEmitter::Create( "CEmbers" );
	AddToEntityList(ENTITY_LIST_SIMULATE);
}

CEmbers::~CEmbers()
{
}

void CEmbers::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_pEmitter->SetSortOrigin( GetAbsOrigin() );

		Start();
	}
	if ( m_bEmit )
	{
		AddToEntityList(ENTITY_LIST_SIMULATE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEmbers::ShouldDraw()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEmbers::Start( void )
{
	//Various setup info
	m_tParticleSpawn.Init( m_nDensity );
	
	m_hMaterial	= m_pEmitter->GetPMaterial( "particle/fire" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEmbers::Simulate( void ) 
{
	if ( m_bEmit == false )
		return false;

	float tempDelta = gpGlobals->frametime;

	while( m_tParticleSpawn.NextEvent( tempDelta ) )
	{
		SpawnEmber();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEmbers::SpawnEmber( void )
{
	Vector	offset, mins, maxs;
	
	modelinfo->GetModelBounds( GetModel(), mins, maxs );

	//Setup our spawn position
	offset[0] = random_valve->RandomFloat( mins[0], maxs[0] );
	offset[1] = random_valve->RandomFloat( mins[1], maxs[1] );
	offset[2] = random_valve->RandomFloat( mins[2], maxs[2] );

	//Spawn the particle
	SimpleParticle	*sParticle = (SimpleParticle *) m_pEmitter->AddParticle( sizeof( SimpleParticle ), m_hMaterial, offset );

	if (sParticle == nullptr)
		return;

	float	cScale = random_valve->RandomFloat( 0.75f, 1.0f );

	//Set it up
	sParticle->m_flLifetime = 0.0f;
	sParticle->m_flDieTime	= m_nLifetime;

	sParticle->m_uchColor[0]	= GetRenderColorR() * cScale;
	sParticle->m_uchColor[1]	= GetRenderColorG() * cScale;
	sParticle->m_uchColor[2]	= GetRenderColorB() * cScale;
	sParticle->m_uchStartAlpha	= 255;
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= 1;
	sParticle->m_uchEndSize		= 0;
	sParticle->m_flRollDelta	= 0;
	sParticle->m_flRoll			= 0;

	//Set the velocity
	Vector	velocity;

	AngleVectors( GetAbsAngles(), &velocity );

	sParticle->m_vecVelocity = velocity * m_nSpeed;

	sParticle->m_vecVelocity[0]	+= random_valve->RandomFloat( -(m_nSpeed/8), (m_nSpeed/8) );
	sParticle->m_vecVelocity[1]	+= random_valve->RandomFloat( -(m_nSpeed/8), (m_nSpeed/8) );
	sParticle->m_vecVelocity[2]	+= random_valve->RandomFloat( -(m_nSpeed/8), (m_nSpeed/8) );

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Quadratic spline beam effect 
//-----------------------------------------------------------------------------

class CQuadraticBeam : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_QuadraticBeam" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CQuadraticBeam, CBaseEntity );

	//virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	ShouldDraw( void ) { return true; }
	virtual int		DrawModel( int, const RenderableInstance_t &instance );

	virtual void	GetRenderBounds( Vector& mins, Vector& maxs )
	{
		ClearBounds( mins, maxs );
		AddPointToBounds( vec3_origin, mins, maxs );
		AddPointToBounds( m_targetPosition, mins, maxs );
		AddPointToBounds( m_controlPosition, mins, maxs );
		mins -= GetRenderOrigin();
		maxs -= GetRenderOrigin();
	}

protected:

	[[= ks::reflect::Net{} ]] Vector		m_targetPosition;
	[[= ks::reflect::Net{} ]] Vector		m_controlPosition;
	[[= ks::reflect::Net{} ]] float		m_scrollRate;
	[[= ks::reflect::Net{} ]] float		m_flWidth;
};

//Receive datatable
IMPLEMENT_REFLECT_CLIENTCLASS( CQuadraticBeam, DT_QuadraticBeam, CEnvQuadraticBeam )

Vector Color24ToVector( const color24 &color )
{
	return Vector( color.r * (1.0/255.0f), color.g * (1.0/255.0f), color.b * (1.0/255.0f) );
}

int	CQuadraticBeam::DrawModel( int, const RenderableInstance_t &instance )
{
	Draw_SetSpriteTexture( GetModel(), 0, GetRenderMode() );
	Vector color = Color24ToVector( GetRenderColor() );
	DrawBeamQuadratic( GetRenderOrigin(), m_controlPosition, m_targetPosition, m_flWidth, color, gpGlobals->curtime*m_scrollRate );
	return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class SnowFallEffect : public CSimpleEmitter
{
public:

	explicit SnowFallEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	static SnowFallEffect* Create( const char *pDebugName )
	{
		return new SnowFallEffect( pDebugName );
	}

	void UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		float flSpeed = VectorNormalize( pParticle->m_vecVelocity );
		flSpeed -= timeDelta;

		int randomVelocityValue = RandomInt( 0, 65536 );
		float velX = ((128 - (randomVelocityValue / 256)) / 128.0f) * 0.025f;
		float velY = ((128 - (randomVelocityValue % 256)) / 128.0f) * 0.025f;
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity.x += velX;
		pParticle->m_vecVelocity.y += velY;

		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity *= flSpeed;

		Vector vecWindVelocity;
		GetWindspeedAtTime( gpGlobals->curtime, vecWindVelocity );
		pParticle->m_vecVelocity += ( vecWindVelocity * r_SnowWindScale.GetFloat() );
	}

	void SimulateParticles( CParticleSimulateIterator *pIterator )
	{
		float timeDelta = pIterator->GetTimeDelta();
	
		SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
		while ( pParticle )
		{
			//Update velocity
			UpdateVelocity( pParticle, timeDelta );
			pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

			//Should this particle die?
			pParticle->m_flLifetime += timeDelta;
			UpdateRoll( pParticle, timeDelta );

			if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			{
				pIterator->RemoveParticle( pParticle );
			}
			else if ( !IsInAir( pParticle->m_Pos ) )
			{
				pIterator->RemoveParticle( pParticle );
			}

			pParticle = (SimpleParticle*)pIterator->GetNext();
		}
	}

	int	GetParticleCount( void )
	{	
		return GetBinding().GetNumActiveParticles(); 
	}

	void SetBounds( const Vector &vecMin, const Vector &vecMax )
	{
		GetBinding().SetBBox( vecMin, vecMax, true );
	}

	RenderableTranslucencyType_t ComputeTranslucencyType( void ) { return RENDERABLE_IS_OPAQUE; }

private:
	SnowFallEffect( const SnowFallEffect & );
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CSnowFallManager : public CBaseEntity
{
public:

	CSnowFallManager();
	~CSnowFallManager();

	bool CreateEmitter( void );

	void SpawnClientEntity( void );
	void ClientThink();

	void AddSnowFallEntity( CClient_Precipitation *pSnowEntity );

	// Snow Effect
	enum
	{
		SNOWFALL_NONE = 0,
		SNOWFALL_AROUND_PLAYER,
		SNOWFALL_IN_ENTITY,
	};

	RenderableTranslucencyType_t ComputeTranslucencyType( void ) { return RENDERABLE_IS_OPAQUE; }
	void	SetSplitScreenPlayerSlot( int nSlot );

private:
	bool CreateSnowFallEmitter( void );
	void CreateSnowFall( void );
	void CreateSnowFallParticles( float flCurrentTime, float flRadius, const Vector &vecEyePos, const Vector &vecForward, float flZoomScale, CBasePlayer *pLocalPlayer );
	void CreateOutsideVolumeSnowParticles( float flCurrentTime, float flRadius, float flZoomScale, CBasePlayer *pLocalPlayer );
	void CreateInsideVolumeSnowParticles( float flCurrentTime, float flRadius, const Vector &vecEyePos, const Vector &vecForward, float flZoomScale, CBasePlayer *pLocalPlayer );
	void CreateSnowParticlesSphere( float flRadius, CBasePlayer *pLocalPlayer );
	void CreateSnowParticlesRay( float flRadius, const Vector &vecEyePos, const Vector &vecForward, CBasePlayer *pLocalPlayer );
	void CreateSnowFallParticle( const Vector &vecParticleSpawn, int iBBox, CBasePlayer *pLocalPlayer );

	int StandingInSnowVolume( Vector &vecPoint );
	void FindSnowVolumes( Vector const &vecCenter, float flRadius, Vector const &vecEyePos, Vector const &vecForward );

	void UpdateBounds( const Vector &vecSnowMin, const Vector &vecSnowMax );

private:

	enum { MAX_SNOW_PARTICLES = 500 };
	enum { MAX_SNOW_LIST = 32 };

	TimedEvent						m_tSnowFallParticleTimer;
	TimedEvent						m_tSnowFallParticleTraceTimer;

	int								m_iSnowFallArea;
	ks::intrusive_ptr<SnowFallEffect>		m_pSnowFallEmitter;
	Vector							m_vecSnowFallEmitOrigin;
	float							m_flSnowRadius;

	Vector							m_vecMin;
	Vector							m_vecMax;

	int								m_nActiveSnowCount;
	int								m_aActiveSnow[MAX_SNOW_LIST];

	bool							m_bRayParticles;

	struct SnowFall_t
	{
		PMaterialHandle				m_hMaterial;
		CClient_Precipitation		*m_pEntity;
		ks::intrusive_ptr<SnowFallEffect>	m_pEffect;
		Vector						m_vecMin;
		Vector						m_vecMax;
	};

	std::vector< SnowFall_t >		m_aSnow;
	int								m_nSplitScreenPlayerSlot;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSnowFallManager::CSnowFallManager( void )
{
	m_iSnowFallArea = SNOWFALL_NONE;
	m_pSnowFallEmitter = nullptr;
	m_vecSnowFallEmitOrigin.Init();
	m_flSnowRadius = 0.0f;
	m_vecMin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	m_vecMax.Init( std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min() );
	m_nActiveSnowCount = 0;
	m_aSnow.clear(); m_aSnow.shrink_to_fit();
	m_nSplitScreenPlayerSlot = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSnowFallManager::~CSnowFallManager( void )
{
	m_aSnow.clear(); m_aSnow.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSnowFallManager::CreateEmitter( void )
{
	return CreateSnowFallEmitter();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSnowFallManager::SpawnClientEntity( void )
{
	m_tSnowFallParticleTimer.Init( 500 );
	m_tSnowFallParticleTraceTimer.Init( 6 );
	m_iSnowFallArea = SNOWFALL_NONE;

	// Have the Snow Fall Manager think for all the snow fall entities.
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSnowFallManager::CreateSnowFallEmitter( void )
{
	if ( ( m_pSnowFallEmitter = SnowFallEffect::Create( "snowfall" ) ) == nullptr )
		return false;

	m_pSnowFallEmitter->SetShouldDrawForSplitScreenUser( m_nSplitScreenPlayerSlot );
	return true;
}

void CSnowFallManager::SetSplitScreenPlayerSlot( int nSlot )
{
	m_nSplitScreenPlayerSlot = nSlot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSnowFallManager::ClientThink( void )
{
	if ( !IsValidSplitScreenSlot( m_nSplitScreenPlayerSlot ) )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenPlayerSlot );

	if ( !r_SnowEnable.GetBool() )
		return;

	// Make sure we have a snow fall emitter.
	if ( !m_pSnowFallEmitter )
	{
		if ( !CreateSnowFallEmitter() )
			return;
	}

	CreateSnowFall();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSnowEntity - 
//-----------------------------------------------------------------------------
void CSnowFallManager::AddSnowFallEntity( CClient_Precipitation *pSnowEntity )
{
	if ( !pSnowEntity )
		return;

	int nSnowCount = std::ssize( m_aSnow );
	int iSnow = 0;
	for ( iSnow = 0; iSnow < nSnowCount; ++iSnow )
	{
		if ( m_aSnow[iSnow].m_pEntity == pSnowEntity )
			break;
	}

	if ( iSnow != nSnowCount )
		return;

	iSnow = std::ssize( m_aSnow ); m_aSnow.emplace_back();
	m_aSnow[iSnow].m_pEntity = pSnowEntity;
	m_aSnow[iSnow].m_pEffect = SnowFallEffect::Create( "snowfall" );
	m_aSnow[iSnow].m_hMaterial = ParticleMgr()->GetPMaterial( "particle/snow" );

	VectorCopy( pSnowEntity->WorldAlignMins(), m_aSnow[iSnow].m_vecMin );
	VectorCopy( pSnowEntity->WorldAlignMaxs(), m_aSnow[iSnow].m_vecMax );

	UpdateBounds( m_aSnow[iSnow].m_vecMin, m_aSnow[iSnow].m_vecMax );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSnowFallManager::UpdateBounds( const Vector &vecSnowMin, const Vector &vecSnowMax )
{
	int iAxis = 0;
	for ( iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( vecSnowMin[iAxis] < m_vecMin[iAxis] )
		{
			m_vecMin[iAxis] = vecSnowMin[iAxis];
		}

		if ( vecSnowMax[iAxis] > m_vecMax[iAxis] )
		{
			m_vecMax[iAxis] = vecSnowMax[iAxis];
		}
	}

	contract_assert( m_pSnowFallEmitter );
	m_pSnowFallEmitter->SetBounds( m_vecMin, m_vecMax );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecPoint - 
// Output : int
//-----------------------------------------------------------------------------
int CSnowFallManager::StandingInSnowVolume( Vector &vecPoint )
{
	trace_t traceSnow;

	int nSnowCount = std::ssize( m_aSnow );
	int iSnow = 0;
	for ( iSnow = 0; iSnow < nSnowCount; ++iSnow )
	{
		UTIL_TraceModel( vecPoint, vecPoint, vec3_origin, vec3_origin, static_cast<CBaseEntity*>( m_aSnow[iSnow].m_pEntity ), COLLISION_GROUP_NONE, &traceSnow );
		if ( traceSnow.startsolid )
			return iSnow;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecCenter - 
//			flRadius - 
//-----------------------------------------------------------------------------
void CSnowFallManager::FindSnowVolumes( Vector const &vecCenter, float flRadius, Vector const &vecEyePos, Vector const &vecForward )
{
	// Reset.
	m_nActiveSnowCount = 0;
	m_bRayParticles = false;

	int nSnowCount = std::ssize( m_aSnow );
	int iSnow = 0;
	for ( iSnow = 0; iSnow < nSnowCount; ++iSnow )
	{
		// Check to see if the volume is in the PVS.
		bool bInPVS = g_pClientLeafSystem->IsRenderableInPVS( m_aSnow[iSnow].m_pEntity->GetClientRenderable() );
		if ( !bInPVS )
			continue;

		// Check to see if a snow volume is inside the given radius.
		if ( IsBoxIntersectingSphere( m_aSnow[iSnow].m_vecMin, m_aSnow[iSnow].m_vecMax, vecCenter, flRadius ) )
		{
			m_aActiveSnow[m_nActiveSnowCount] = iSnow;
			++m_nActiveSnowCount;
			if ( m_nActiveSnowCount >= MAX_SNOW_LIST )
			{
				DevWarning( 1, "Max Active Snow Volume Count!\n" );
				break;
			}
		}
		// Check to see if a snow volume is outside of the sphere radius, but is along line-of-sight.
		else		
		{
			CBaseTrace trace;
			Vector vecNewForward;
			vecNewForward = vecForward * r_SnowRayLength.GetFloat();
			vecNewForward.z = 0.0f;
			IntersectRayWithBox( vecEyePos, vecNewForward, m_aSnow[iSnow].m_vecMin, m_aSnow[iSnow].m_vecMax, 0.325f, &trace );
			if ( trace.fraction < 1.0f )
			{
				m_aActiveSnow[m_nActiveSnowCount] = iSnow;
				++m_nActiveSnowCount;
				if ( m_nActiveSnowCount >= MAX_SNOW_LIST )
				{
					DevWarning( 1, "Max Active Snow Volume Count!\n" );
					break;
				}

				m_bRayParticles = true;
			}
		}
	}

	// Debugging code!
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateSnowFall( void )
{
	CVProfScope _( "SnowFall", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	// Check to see if we have a local player before starting the snow around a local player.
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer == nullptr )
		return;

	// Get the current frame time.
	float flCurrentTime = gpGlobals->frametime;

	// Get the players data to determine where the snow emitter should reside.
	VectorCopy( pPlayer->EyePosition(), m_vecSnowFallEmitOrigin );
	Vector vecForward;
	pPlayer->GetVectors( &vecForward, nullptr, nullptr );
	vecForward.z = 0.0f;
	Vector vecVelocity = pPlayer->GetAbsVelocity();
	float flSpeed = VectorNormalize( vecVelocity );
	m_vecSnowFallEmitOrigin += ( vecForward * ( 64.0f + ( flSpeed * 0.4f * r_SnowPosScale.GetFloat() ) ) );
	m_vecSnowFallEmitOrigin += ( vecVelocity * ( flSpeed * 1.25f * r_SnowSpeedScale.GetFloat() ) );

	// Check to see if the player is zoomed.
	bool bZoomed = ( pPlayer->GetFOV() != pPlayer->GetDefaultFOV() );
	float flZoomScale = 1.0f;
	if ( bZoomed )
	{
		flZoomScale = pPlayer->GetDefaultFOV() / pPlayer->GetFOV();
		flZoomScale *= 0.5f;
	}

	// Time to test for a snow volume yet?  (Only do this 6 times a second!)
	if ( m_tSnowFallParticleTraceTimer.NextEvent( flCurrentTime ) )
	{
		// Reset the active snow emitter.
		m_iSnowFallArea = SNOWFALL_NONE;

		// Set the trace start and the emit origin.
		Vector vecTraceStart;
		VectorCopy( pPlayer->EyePosition(), vecTraceStart );

		int iSnowVolume = StandingInSnowVolume( vecTraceStart );
		if ( iSnowVolume != -1 )
		{
			m_flSnowRadius = r_SnowInsideRadius.GetFloat() + ( flSpeed * 0.5f );
			m_iSnowFallArea = SNOWFALL_AROUND_PLAYER;
		}
		else
		{
			m_flSnowRadius = r_SnowOutsideRadius.GetFloat();
		}

		float flRadius = m_flSnowRadius;
		if ( bZoomed )
		{
			if ( m_iSnowFallArea == SNOWFALL_AROUND_PLAYER )
			{
				flRadius = r_SnowOutsideRadius.GetFloat() * flZoomScale;
			}
			else
			{
				flRadius *= flZoomScale;
			}
		}

		FindSnowVolumes( m_vecSnowFallEmitOrigin, flRadius, pPlayer->EyePosition(), vecForward );
		if ( m_nActiveSnowCount != 0 && m_iSnowFallArea != SNOWFALL_AROUND_PLAYER )
		{
			// We found an active snow emitter.
			m_iSnowFallArea = SNOWFALL_IN_ENTITY;

		}
	}

	if ( m_iSnowFallArea == SNOWFALL_NONE )
		 return;

	// Set the origin in the snow emitter.
	m_pSnowFallEmitter->SetSortOrigin( m_vecSnowFallEmitOrigin );

	// Create snow fall particles.
	CreateSnowFallParticles( flCurrentTime, m_flSnowRadius, pPlayer->EyePosition(), vecForward, flZoomScale, pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCurrentTime - 
//			flRadius - 
//			&vecEyePos - 
//			&vecForward - 
//			flZoomScale - 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateSnowFallParticles( float flCurrentTime, float flRadius, const Vector &vecEyePos, const Vector &vecForward, float flZoomScale, CBasePlayer *pLocalPlayer )
{
	const float SnowfallRate = 500.0f;
	if ( m_nActiveSnowCount > 0 )
	{
		CBaseEntity *pEntity = m_aSnow[ m_aActiveSnow[0] ].m_pEntity;
		int density = pEntity->GetRenderAlpha();
		density = clamp( density, 0, 100 );
		if ( pEntity && density > 0 )
		{
			m_tSnowFallParticleTimer.ResetRate( SnowfallRate * density * 0.01f );
		}
		else
		{
			m_tSnowFallParticleTimer.ResetRate( SnowfallRate );
		}
	}
	else
	{
		m_tSnowFallParticleTimer.ResetRate( SnowfallRate );
	}

	// Outside of a snow volume.
	if ( m_iSnowFallArea == SNOWFALL_IN_ENTITY )
	{
		CreateOutsideVolumeSnowParticles( flCurrentTime, flRadius, flZoomScale, pLocalPlayer );
	}
	// Inside of a snow volume.
	else
	{
		CreateInsideVolumeSnowParticles( flCurrentTime, flRadius, vecEyePos, vecForward, flZoomScale, pLocalPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCurrentTime - 
//			flRadius - 
//			flZoomScale - 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateOutsideVolumeSnowParticles( float flCurrentTime, float flRadius, float flZoomScale, CBasePlayer *pLocalPlayer )
{
	Vector vecParticleSpawn;

	// Outside of a snow volume.
	int iSnow = 0;
	float flRadiusScaled = flRadius * flZoomScale;
	float flRadius2 = flRadiusScaled * flRadiusScaled;

	// Add as many particles as we need
	while ( m_tSnowFallParticleTimer.NextEvent( flCurrentTime ) )
	{
		// Check for a max particle count.
		if ( m_pSnowFallEmitter->GetParticleCount() >= r_SnowParticles.GetInt() )
			continue;

		vecParticleSpawn.x = RandomFloat( m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.x, m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.x );
		vecParticleSpawn.y = RandomFloat( m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.y, m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.y );
		vecParticleSpawn.z = RandomFloat( m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.z, m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.z );

		float flDistance2 = ( m_vecSnowFallEmitOrigin - vecParticleSpawn ).LengthSqr();
		if ( flDistance2 < flRadius2 )
		{
			CreateSnowFallParticle( vecParticleSpawn, m_aActiveSnow[iSnow], pLocalPlayer );
		}

		iSnow = ( iSnow + 1 ) % m_nActiveSnowCount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCurrentTime - 
//			flRadius - 
//			&vecEyePos - 
//			&vecForward - 
//			flZoomScale - 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateInsideVolumeSnowParticles( float flCurrentTime, float flRadius, const Vector &vecEyePos, const Vector &vecForward, float flZoomScale, CBasePlayer *pLocalPlayer )
{
	Vector vecParticleSpawn;

	// Check/Setup for zoom.
	bool bZoomed = ( flZoomScale > 1.0f );
	float flZoomRadius = 0.0f;
	Vector vecZoomEmitOrigin;
	if ( bZoomed )
	{
		vecZoomEmitOrigin = m_vecSnowFallEmitOrigin + ( vecForward * ( r_SnowZoomOffset.GetFloat() * flZoomScale ) );
		flZoomRadius = flRadius * flZoomScale;
	}

	int iIndex = 0;

	// Add as many particles as we need
	while ( m_tSnowFallParticleTimer.NextEvent( flCurrentTime ) )
	{
		// Check for a max particle count.
		if ( m_pSnowFallEmitter->GetParticleCount() >= r_SnowParticles.GetInt() )
			continue;

		// Create particle inside of sphere.
		if ( iIndex > 0 )
		{
			CreateSnowParticlesSphere( flZoomRadius, pLocalPlayer );
			CreateSnowParticlesRay( flZoomRadius, vecEyePos, vecForward, pLocalPlayer );
		}
		else
		{
			CreateSnowParticlesSphere( flRadius, pLocalPlayer );
			CreateSnowParticlesRay( flRadius, vecEyePos, vecForward, pLocalPlayer );
		}

		// Increment if zoomed.
		if ( bZoomed )
		{
			iIndex = ( iIndex + 1 ) % 3;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flRadius - 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateSnowParticlesSphere( float flRadius, CBasePlayer *pLocalPlayer )
{
	Vector vecParticleSpawn;

	vecParticleSpawn.x = m_vecSnowFallEmitOrigin.x + RandomFloat( -flRadius, flRadius );
	vecParticleSpawn.y = m_vecSnowFallEmitOrigin.y + RandomFloat( -flRadius, flRadius );
	vecParticleSpawn.z = m_vecSnowFallEmitOrigin.z + RandomFloat( -flRadius, flRadius );

	int iSnow = 0;
	for ( iSnow = 0; iSnow < m_nActiveSnowCount; ++iSnow )
	{
		if ( ( vecParticleSpawn.x < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.x ) || ( vecParticleSpawn.x > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.x ) )
			continue;
		if ( ( vecParticleSpawn.y < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.y ) || ( vecParticleSpawn.y > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.y ) )
			continue;
		if ( ( vecParticleSpawn.z < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.z ) || ( vecParticleSpawn.z > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.z ) )
			continue;

		break;
	}

	if ( iSnow == m_nActiveSnowCount )
		return;

	CreateSnowFallParticle( vecParticleSpawn, m_aActiveSnow[iSnow], pLocalPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecEyePos - 
//			&vecForward - 
//-----------------------------------------------------------------------------
void CSnowFallManager::CreateSnowParticlesRay( float flRadius, const Vector &vecEyePos, const Vector &vecForward, CBasePlayer *pLocalPlayer )
{
	// Check to see if we should create particles along line-of-sight.
	if ( !m_bRayParticles && r_SnowRayEnable.GetBool() )
		return;

	Vector vecParticleSpawn;

	// Create a particle down the player's view beyond the radius.
	float flRayRadius = r_SnowRayRadius.GetFloat();

	Vector vecNewForward;
	vecNewForward = vecForward * RandomFloat( flRadius, r_SnowRayLength.GetFloat() );

	vecParticleSpawn.x = vecEyePos.x + vecNewForward.x;
	vecParticleSpawn.y = vecEyePos.y + vecNewForward.y;
	vecParticleSpawn.z = vecEyePos.z + RandomFloat( 72, flRayRadius );
	vecParticleSpawn.x += RandomFloat( -flRayRadius, flRayRadius );
	vecParticleSpawn.y += RandomFloat( -flRayRadius, flRayRadius );

	int iSnow = 0;
	for ( iSnow = 0; iSnow < m_nActiveSnowCount; ++iSnow )
	{
		if ( ( vecParticleSpawn.x < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.x ) || ( vecParticleSpawn.x > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.x ) )
			continue;
		if ( ( vecParticleSpawn.y < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.y ) || ( vecParticleSpawn.y > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.y ) )
			continue;
		if ( ( vecParticleSpawn.z < m_aSnow[m_aActiveSnow[iSnow]].m_vecMin.z ) || ( vecParticleSpawn.z > m_aSnow[m_aActiveSnow[iSnow]].m_vecMax.z ) )
			continue;

		break;
	}

	if ( iSnow == m_nActiveSnowCount )
		return;

	CreateSnowFallParticle( vecParticleSpawn, m_aActiveSnow[iSnow], pLocalPlayer );
}

void CSnowFallManager::CreateSnowFallParticle( const Vector &vecParticleSpawn, int iSnow, CBasePlayer *pLocalPlayer )
{	
	SimpleParticle *pParticle = ( SimpleParticle* )m_pSnowFallEmitter->AddParticle( sizeof( SimpleParticle ), m_aSnow[iSnow].m_hMaterial, vecParticleSpawn );
	if ( pParticle == nullptr )
		return; 

	pParticle->m_flLifetime	= 0.0f;
	pParticle->m_vecVelocity = Vector( RandomFloat( -5.0f, 5.0f ), RandomFloat( -5.0f, 5.0f ), ( RandomFloat( -25, -35 ) * r_SnowFallSpeed.GetFloat() ) );
	pParticle->m_flDieTime	= fabs( ( vecParticleSpawn.z - m_aSnow[iSnow].m_vecMin.z ) / ( pParticle->m_vecVelocity.z - 0.1 ) );
	
	// We finally now use the color of the func_precipitation entity for snow color.
	pParticle->m_uchColor[0] = m_aSnow[iSnow].m_pEntity->GetRenderColorR();
	pParticle->m_uchColor[1] = m_aSnow[iSnow].m_pEntity->GetRenderColorG();
	pParticle->m_uchColor[2] = m_aSnow[iSnow].m_pEntity->GetRenderColorB();
	//pParticle->m_uchColor[0] = r_SnowColorRed.GetInt();
	//pParticle->m_uchColor[1] = r_SnowColorGreen.GetInt();
	//pParticle->m_uchColor[2] = r_SnowColorBlue.GetInt();

	pParticle->m_uchStartSize = r_SnowStartSize.GetInt();
	pParticle->m_uchEndSize = r_SnowEndSize.GetInt();

//	pParticle->m_uchStartAlpha	= 255;
	pParticle->m_uchStartAlpha	= r_SnowStartAlpha.GetInt();
	pParticle->m_uchEndAlpha	= r_SnowEndAlpha.GetInt();

	pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= random_valve->RandomFloat( -0.15f, 0.15f );

	pParticle->m_iFlags			= SIMPLE_PARTICLE_FLAG_WINDBLOWN;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool SnowFallManagerCreate( CClient_Precipitation *pSnowEntity )
{
	bool bret = true;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( !s_pSnowFallMgr[ i ] )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );

			s_pSnowFallMgr[ i ] = new CSnowFallManager();
			if ( !s_pSnowFallMgr[ i ] )
			{
				bret = false;
				break;
			}
			s_pSnowFallMgr[ i ]->SetSplitScreenPlayerSlot( i );
			s_pSnowFallMgr[ i ]->CreateEmitter();
			s_pSnowFallMgr[ i ]->InitializeAsClientEntity( nullptr, false );
			g_pClientLeafSystem->EnableRendering( s_pSnowFallMgr[ i ]->RenderHandle(), false );
		}

		s_pSnowFallMgr[ i ]->AddSnowFallEntity( pSnowEntity );
	}
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SnowFallManagerDestroy( void )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( s_pSnowFallMgr[ i ] )
		{
			delete s_pSnowFallMgr[ i ];
			s_pSnowFallMgr[ i ] = nullptr;
		}
	}
}

class CRecipientFilter;

using namespace ::ks::reflect;

// Purpose: Utility code.

//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
class CEffectsClient : public IEffects
{
public:
	CEffectsClient();
	virtual ~CEffectsClient();

	// Members of the IEffect interface
	virtual void Beam( const Vector &Start, const Vector &End, int nModelIndex, 
		int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
		float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
		unsigned char noise, unsigned char red, unsigned char green,
		unsigned char blue, unsigned char brightness, unsigned char speed);
	virtual void Smoke( const Vector &origin, int modelIndex, float scale, float framerate );
	virtual void Sparks( const Vector &position, int nMagnitude = 1, int nTrailLength = 1, const Vector *pvecDir = nullptr );
	virtual void Dust( const Vector &pos, const Vector &dir, float size, float speed );
	virtual void MuzzleFlash( const Vector &origin, const QAngle &angles, float fScale, int type );
	virtual void MetalSparks( const Vector &position, const Vector &direction ); 
	virtual void EnergySplash( const Vector &position, const Vector &direction, bool bExplosive = false );
	virtual void Ricochet( const Vector &position, const Vector &direction );

	// FIXME: Should these methods remain in this interface? Or go in some 
	// other client-server neutral interface?
	virtual float Time();
	virtual bool IsServer();
	virtual void SuppressEffectsSounds( bool bSuppress );

private:
	//-----------------------------------------------------------------------------
	// Purpose: Returning true means don't even call TE func
	// Input  : filter - 
	//			*suppress_host - 
	// Output : static bool
	//-----------------------------------------------------------------------------
	bool SuppressTE( CRecipientFilter& filter )
	{
		if ( !CanPredict() )
			return true;

		if ( !filter.GetRecipientCount() )
		{
			// Suppress it
			return true;
		}

		// There's at least one recipient
		return false;
	}

	bool m_bSuppressSound;
};

//-----------------------------------------------------------------------------
// Client-server neutral effects interface accessor
//-----------------------------------------------------------------------------
static CEffectsClient s_EffectClient;
static InterfaceReg s_Reg_CEffectsClient_IEffects( side_registry(), []() -> void * { return static_cast<IEffects *>( &s_EffectClient ); }, IEFFECTS_INTERFACE_VERSION );;
IEffects *g_pEffects = &s_EffectClient;

ConVar r_decals( side_registry(), "r_decals", "2048" );

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEffectsClient::CEffectsClient()
{
	m_bSuppressSound = false;
}

CEffectsClient::~CEffectsClient()
{
}

//-----------------------------------------------------------------------------
// Suppress sound on effects
//-----------------------------------------------------------------------------
void CEffectsClient::SuppressEffectsSounds( bool bSuppress )
{
	m_bSuppressSound = bSuppress;
}

//-----------------------------------------------------------------------------
// Generates a beam
//-----------------------------------------------------------------------------
void CEffectsClient::Beam( const Vector &vecStartPoint, const Vector &vecEndPoint, 
	int nModelIndex, int nHaloIndex, unsigned char frameStart, unsigned char nFrameRate,
	float flLife, unsigned char nWidth, unsigned char nEndWidth, unsigned char nFadeLength, 
	unsigned char noise, unsigned char r, unsigned char g,
	unsigned char b, unsigned char brightness, unsigned char nSpeed)
{
	contract_assert( 0 );
//	CBroadcastRecipientFilter filter;
//	if ( !SuppressTE( filter ) )
//	{
//	beams->CreateBeamPoints( vecStartPoint, vecEndPoint, nModelIndex, nHaloIndex, 
//		m_fHaloScale, 
//		flLife, 0.1 * nWidth,  0.1 * nEndWidth, nFadeLength, 0.01 * nAmplitude, a, 0.1 * nSpeed, 
//		m_nStartFrame, 0.1 * nFrameRate, r, g, b );
//	}
}

//-----------------------------------------------------------------------------
// Generates various tempent effects
//-----------------------------------------------------------------------------
void CEffectsClient::Smoke( const Vector &vecOrigin, int modelIndex, float scale, float framerate )
{
	CPVSFilter filter( vecOrigin );
	if ( !SuppressTE( filter ) )
	{
		int iColor = random_valve->RandomInt(20,35);
		color32 color;
		color.r = iColor;
		color.g = iColor;
		color.b = iColor;
		color.a = iColor;
		QAngle angles;
		VectorAngles( Vector(0,0,1), angles );
		FX_Smoke( vecOrigin, angles, scale * 0.1f, 4, (unsigned char *)&color, 255 );
	}
}

void CEffectsClient::Sparks( const Vector &position, int nMagnitude, int nTrailLength, const Vector *pVecDir )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		FX_ElectricSpark( position, nMagnitude, nTrailLength, pVecDir );
	}
}

void CEffectsClient::Dust( const Vector &pos, const Vector &dir, float size, float speed )
{
	CPVSFilter filter( pos );
	if ( !SuppressTE( filter ) )
	{
		FX_Dust( pos, dir, size, speed );
	}
}

void CEffectsClient::MuzzleFlash( const Vector &vecOrigin, const QAngle &vecAngles, float flScale, int iType )
{
	CPVSFilter filter( vecOrigin );
	if ( !SuppressTE( filter ) )
	{
		switch( iType )
		{
		case MUZZLEFLASH_TYPE_DEFAULT:
			FX_MuzzleEffect( vecOrigin, vecAngles, flScale, INVALID_EHANDLE );
			break;

		case MUZZLEFLASH_TYPE_GUNSHIP:
			FX_GunshipMuzzleEffect( vecOrigin, vecAngles, flScale, INVALID_EHANDLE );
			break;

		case MUZZLEFLASH_TYPE_STRIDER:
			FX_StriderMuzzleEffect( vecOrigin, vecAngles, flScale, INVALID_EHANDLE );
			break;
		
		default:
			Msg("No case for Muzzleflash type: %d\n", iType );
			break;
		}
	}
}

void CEffectsClient::MetalSparks( const Vector &position, const Vector &direction )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		FX_MetalSpark( position, direction, direction );
	}
}

void CEffectsClient::EnergySplash( const Vector &position, const Vector &direction, bool bExplosive )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		FX_EnergySplash( position, direction, bExplosive );
	}
}

void CEffectsClient::Ricochet( const Vector &position, const Vector &direction )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		FX_MetalSpark( position, direction, direction );

		if ( !m_bSuppressSound )
		{
			FX_RicochetSound( position );
		}
	}
}

// FIXME: Should these methods remain in this interface? Or go in some 
// other client-server neutral interface?
float CEffectsClient::Time()
{
	return gpGlobals->curtime;
}

bool CEffectsClient::IsServer()
{
	return false;
}

// Purpose: This module implements functions to support ehandles.

class CBaseAnimating;
class CBaseEntity;
class CClientRagdoll;
class SimpleParticle;

using namespace ::ks::reflect;

ConVar cl_portal_use_new_dissolve( side_registry(), "cl_portal_use_new_dissolve", "1", FCVAR_CHEAT, "Use new dissolve effect" );

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectBuild )
PRECACHE( MATERIAL,"effects/tesla_glow_noz" )
PRECACHE( MATERIAL,"effects/spark" )
PRECACHE( MATERIAL,"effects/combinemuzzle2" )
PRECACHE( PARTICLE_SYSTEM, "dissolve" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CEntityDissolve, DT_EntityDissolve, CEntityDissolve )

extern PMaterialHandle g_Material_Spark;
PMaterialHandle g_Material_AR2Glow = nullptr;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityDissolve::CEntityDissolve( void )
{
	m_bLinkedToServerEnt = true;
	m_pController = nullptr;
	m_bCoreExplode = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityDissolve::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( GetMoveParent() )
	{
		GetMoveParent()->GetRenderBounds( theMins, theMaxs );
	}
	else
	{
		theMins = GetAbsOrigin();
		theMaxs = theMaxs;
	}
}

//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void CEntityDissolve::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flNextSparkTime = m_flStartTime;
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		if ( cl_portal_use_new_dissolve.GetBool() )
			DispatchParticleEffect( "dissolve", PATTACH_ABSORIGIN_FOLLOW, GetMoveParent() );
	}
}

//-----------------------------------------------------------------------------
// Cleanup
//-----------------------------------------------------------------------------
void CEntityDissolve::UpdateOnRemove( void )
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = nullptr;
	}

	BaseClass::UpdateOnRemove();
}

//------------------------------------------------------------------------------
// Apply the forces to the entity
//------------------------------------------------------------------------------
IMotionEvent::simresult_e CEntityDissolve::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear.Init();
	angular.Init();

	// Make it zero g
	linear.z -= -1.02 * sv_gravity.GetFloat();

	Vector vel;
	AngularImpulse angVel;
	pObject->GetVelocity( &vel, &angVel );
	vel += linear * deltaTime; // account for gravity scale

	Vector unitVel = vel;
	Vector unitAngVel = angVel;

	float speed = VectorNormalize( unitVel );
//	float angSpeed = VectorNormalize( unitAngVel );

//	float speedScale = 0.0;
//	float angSpeedScale = 0.0;

	float flLinearLimit = 50;
	float flLinearLimitDelta = 40;
	if ( speed > flLinearLimit )
	{
		float flDeltaVel = (flLinearLimit - speed) / deltaTime;
		if ( flLinearLimitDelta != 0.0f )
		{
			float flMaxDeltaVel = -flLinearLimitDelta / deltaTime;
			if ( flDeltaVel < flMaxDeltaVel )
			{
				flDeltaVel = flMaxDeltaVel;
			}
		}
		VectorMA( linear, flDeltaVel, unitVel, linear );
	}

	return SIM_GLOBAL_ACCELERATION;
}

//-----------------------------------------------------------------------------
// Tesla effect
//-----------------------------------------------------------------------------
static void FX_BuildTesla( CBaseEntity *pEntity, Vector &vecOrigin, Vector &vecEnd )
{
	BeamInfo_t beamInfo;
	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_nStartAttachment = 0;
	beamInfo.m_pEndEnt = nullptr;
	beamInfo.m_nEndAttachment = 0;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vecOrigin;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_pszModelName = "sprites/lgtning.vmt";
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = random_valve->RandomFloat( 0.25f, 1.0f );
	beamInfo.m_flWidth = random_valve->RandomFloat( 8.0f, 14.0f );
	beamInfo.m_flEndWidth = 1.0f;
	beamInfo.m_flFadeLength = 0.5f;
	beamInfo.m_flAmplitude = 24;
	beamInfo.m_flBrightness = 255.0;
	beamInfo.m_flSpeed = 150.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 30.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 18;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = 0; //FBEAM_ONLYNOISEONCE;
	
	beams->CreateBeamEntPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void CEntityDissolve::BuildTeslaEffect( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset )
{
	Vector vecOrigin;
	QAngle vecAngles;
	MatrixGetColumn( hitboxToWorld, 3, vecOrigin );
	MatrixAngles( hitboxToWorld, vecAngles.Base() );
	CBaseEntity *pEntity = GetMoveParent();

	// Make a couple of tries at it
	int iTries = -1;
	Vector vecForward;
	trace_t tr;
	do
	{
		iTries++;

		// Some beams are deliberatly aimed around the point, the rest are random.
		if ( !bRandom )
		{
			QAngle vecTemp = vecAngles;
			vecTemp[YAW] += flYawOffset;
			AngleVectors( vecTemp, &vecForward );

			// Randomly angle it up or down
			vecForward.z = RandomFloat( -1, 1 );
		}
		else
		{
			vecForward = RandomVector( -1, 1 );
		}

		UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * 192), MASK_SHOT, pEntity, COLLISION_GROUP_NONE, &tr );
	} while ( tr.fraction >= 1.0 && iTries < 3 );

	Vector vecEnd = tr.endpos - (vecForward * 8);

	// Only spark & glow if we hit something
	if ( tr.fraction < 1.0 )
	{
		if ( !EffectOccluded( tr.endpos ) )
		{
			int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

			// Move it towards the camera
			Vector vecFlash = tr.endpos;
			Vector vecForward;
			AngleVectors( MainViewAngles(nSlot), &vecForward );
			vecFlash -= (vecForward * 8);

			g_pEffects->EnergySplash( vecFlash, -vecForward, false );

			// End glow
			ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
			pSimple->SetSortOrigin( vecFlash );
			SimpleParticle *pParticle;
			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/tesla_glow_noz" ), vecFlash );
			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime = 0.0f;
				pParticle->m_flDieTime	= RandomFloat( 0.5, 1 );
				pParticle->m_vecVelocity = vec3_origin;
				Vector color( 1,1,1 );
				float  colorRamp = RandomFloat( 0.75f, 1.25f );
				pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
				pParticle->m_uchColor[1]	= std::min( 1.0f, color[1] * colorRamp ) * 255.0f;
				pParticle->m_uchColor[2]	= std::min( 1.0f, color[2] * colorRamp ) * 255.0f;
				pParticle->m_uchStartSize	= RandomFloat( 6,13 );
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize - 2;
				pParticle->m_uchStartAlpha	= 255;
				pParticle->m_uchEndAlpha	= 10;
				pParticle->m_flRoll			= RandomFloat( 0,360 );
				pParticle->m_flRollDelta	= 0;
			}
		}
	}

	// Build the tesla
	FX_BuildTesla( pEntity, vecOrigin, tr.endpos );
}

//-----------------------------------------------------------------------------
// Sorts the components of a vector
//-----------------------------------------------------------------------------
static inline void SortAbsVectorComponents( const Vector& src, int* pVecIdx )
{
	Vector absVec( fabs(src[0]), fabs(src[1]), fabs(src[2]) );

	int maxIdx = (absVec[0] > absVec[1]) ? 0 : 1;
	if (absVec[2] > absVec[maxIdx])
	{
		maxIdx = 2;
	}

	// always choose something right-handed....
	switch(	maxIdx )
	{
	case 0:
		pVecIdx[0] = 1;
		pVecIdx[1] = 2;
		pVecIdx[2] = 0;
		break;
	case 1:
		pVecIdx[0] = 2;
		pVecIdx[1] = 0;
		pVecIdx[2] = 1;
		break;
	case 2:
		pVecIdx[0] = 0;
		pVecIdx[1] = 1;
		pVecIdx[2] = 2;
		break;
	}
}

//-----------------------------------------------------------------------------
// Compute the bounding box's center, size, and basis
//-----------------------------------------------------------------------------
void CEntityDissolve::ComputeRenderInfo( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, 
										 Vector *pVecAbsOrigin, Vector *pXVec, Vector *pYVec )
{
	// Compute the center of the hitbox in worldspace
	Vector vecHitboxCenter;
	VectorAdd( pHitBox->bbmin, pHitBox->bbmax, vecHitboxCenter );
	vecHitboxCenter *= 0.5f;
	VectorTransform( vecHitboxCenter, hitboxToWorld, *pVecAbsOrigin );

	// Get the object's basis
	Vector vec[3];
	MatrixGetColumn( hitboxToWorld, 0, vec[0] );
	MatrixGetColumn( hitboxToWorld, 1, vec[1] );
	MatrixGetColumn( hitboxToWorld, 2, vec[2] );
//	vec[1] *= -1.0f;

	Vector vecViewDir;
	VectorSubtract( CurrentViewOrigin(), *pVecAbsOrigin, vecViewDir );
	VectorNormalize( vecViewDir );

	// Project the shadow casting direction into the space of the hitbox
	Vector localViewDir;
	localViewDir[0] = DotProduct( vec[0], vecViewDir );
	localViewDir[1] = DotProduct( vec[1], vecViewDir );
	localViewDir[2] = DotProduct( vec[2], vecViewDir );

	// Figure out which vector has the largest component perpendicular
	// to the view direction...
	// Sort by how perpendicular it is
	int vecIdx[3];
	SortAbsVectorComponents( localViewDir, vecIdx );

	// Here's our hitbox basis vectors; namely the ones that are
	// most perpendicular to the view direction
	*pXVec = vec[vecIdx[0]];
	*pYVec = vec[vecIdx[1]];

	// Project them into a plane perpendicular to the view direction
	*pXVec -= vecViewDir * DotProduct( vecViewDir, *pXVec );
	*pYVec -= vecViewDir * DotProduct( vecViewDir, *pYVec );
	VectorNormalize( *pXVec );
	VectorNormalize( *pYVec );

	// Compute the hitbox size
	Vector boxSize;
	VectorSubtract( pHitBox->bbmax, pHitBox->bbmin, boxSize );

	// We project the two longest sides into the vectors perpendicular
	// to the projection direction, then add in the projection of the perp direction
	Vector2D size( boxSize[vecIdx[0]], boxSize[vecIdx[1]] );
	size.x *= fabs( DotProduct( vec[vecIdx[0]], *pXVec ) );
	size.y *= fabs( DotProduct( vec[vecIdx[1]], *pYVec ) );

	// Add the third component into x and y
	size.x += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pXVec ) );
	size.y += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pYVec ) );

	// Bloat a bit, since the shadow wants to extend outside the model a bit
	size *= 2.0f;

	// Clamp the minimum size
	Vector2DMax( size, Vector2D(10.0f, 10.0f), size );

	// Factor the size into the xvec + yvec
	(*pXVec) *= size.x * 0.5f;
	(*pYVec) *= size.y * 0.5f;
}

//-----------------------------------------------------------------------------
// Sparks!
//-----------------------------------------------------------------------------
void CEntityDissolve::DoSparks( mstudiohitboxset_t *set, matrix3x4_t *hitboxbones[MAXSTUDIOBONES] )
{
	if ( m_flNextSparkTime > gpGlobals->curtime )
		return;

	float dt = m_flStartTime + m_flFadeOutStart - gpGlobals->curtime;
	dt = clamp( dt, 0.0f, m_flFadeOutStart );
	
	float flNextTime;
	if (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL)
	{
		flNextTime = SimpleSplineRemapVal( dt, 0.0f, m_flFadeOutStart, 2.0f * TICK_INTERVAL, 0.4f );
	}
	else
	{
		// m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT);
		flNextTime = SimpleSplineRemapVal( dt, 0.0f, m_flFadeOutStart, 0.3f, 1.0f );
	}

	m_flNextSparkTime = gpGlobals->curtime + flNextTime;

	// Send out beams around us
	int iNumBeamsAround = 2;
	int iNumRandomBeams = 1;
	int iTotalBeams = iNumBeamsAround + iNumRandomBeams;
	float flYawOffset = RandomFloat(0,360);
	for ( int i = 0; i < iTotalBeams; i++ )
	{
		int nHitbox = random_valve->RandomInt( 0, set->numhitboxes - 1 );
		mstudiobbox_t *pBox = set->pHitbox(nHitbox);

		float flActualYawOffset = 0;
		bool bRandom = ( i >= iNumBeamsAround );
		if ( !bRandom )
		{
			flActualYawOffset = anglemod( flYawOffset + ((360 / iTotalBeams) * i) );
		}

		BuildTeslaEffect( pBox, *hitboxbones[pBox->bone], bRandom, flActualYawOffset );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityDissolve::SetupEmitter( void )
{
	if ( !m_pEmitter )
	{
		m_pEmitter = CSimpleEmitter::Create( "CEntityDissolve" );
		m_pEmitter->SetSortOrigin( GetAbsOrigin() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CEntityDissolve::GetFadeInPercentage( void )
{
	float dt = gpGlobals->curtime - m_flStartTime;
	
	if ( dt > m_flFadeOutStart )
		return 1.0f;

	if ( dt < m_flFadeInStart )
		return 0.0f;

	if ( (dt > m_flFadeInStart) && (dt < m_flFadeInStart + m_flFadeInLength) )
	{
		dt -= m_flFadeInStart;
		
		return ( dt / m_flFadeInLength );
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CEntityDissolve::GetFadeOutPercentage( void )
{
	float dt = gpGlobals->curtime - m_flStartTime;
	
	if ( dt < m_flFadeInStart )
		return 1.0f;

	if ( dt > m_flFadeOutStart )
	{
		dt -= m_flFadeOutStart;

		if ( dt > m_flFadeOutLength )
			return 0.0f;
		
		return 1.0f - ( dt / m_flFadeOutLength );
	}
	
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CEntityDissolve::GetModelFadeOutPercentage( void )
{
	float dt = gpGlobals->curtime - m_flStartTime;
	
	if ( dt < m_flFadeOutModelStart )
		return 1.0f;

	if ( dt > m_flFadeOutModelStart )
	{
		dt -= m_flFadeOutModelStart;
		
		if ( dt > m_flFadeOutModelLength )
			return 0.0f;
		
		return 1.0f - ( dt / m_flFadeOutModelLength );
	}
	
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityDissolve::ClientThink( void )
{
	CBaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : nullptr;
	if (!pAnimating)
		return;

	// NOTE: IsRagdoll means *client-side* ragdoll. We shouldn't be trying to fight
	// the server ragdoll (or any server physics) on the client
	if (( !m_pController ) && ( m_nDissolveType == ENTITY_DISSOLVE_NORMAL ) && pAnimating->IsRagdoll())
	{
		IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int nCount = pAnimating->VPhysicsGetObjectList( ppList, std::size( ppList ) );
		if ( nCount > 0 )
		{
			m_pController = physenv->CreateMotionController( this );
			for ( int i = 0; i < nCount; ++i )
			{
				m_pController->AttachObject( ppList[i], true );
			}
		}
	}

	color32 color;

	color.r = color.g = color.b = ( 1.0f - GetFadeInPercentage() ) * 255.0f;
	color.a = GetModelFadeOutPercentage() * 255.0f;

	// Setup the entity fade
	pAnimating->SetRenderMode( kRenderTransColor );
	pAnimating->SetRenderColor( color.r, color.g, color.b );
	pAnimating->SetRenderAlpha( color.a );

	if ( GetModelFadeOutPercentage() <= 0.2f )
	{
		m_bCoreExplode = true;
	}

	// If we're dead, fade out
	if ( GetFadeOutPercentage() <= 0.0f )
	{
		// Do NOT remove from the client entity list. It'll confuse the local network backdoor, and the entity will never get destroyed
		// because when the server says to destroy it, the client won't be able to find it.
		// ClientEntityList().RemoveEntity( GetClientHandle() );

		::partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );

		RemoveFromLeafSystem();

		//FIXME: Ick!
		//Adrian: I'll assume we don't need the ragdoll either so I'll remove that too.
		if ( m_bLinkedToServerEnt == false )
		{
			Release();

			CClientRagdoll *pRagdoll = dynamic_cast <CClientRagdoll *> ( pAnimating );

			if ( pRagdoll )
			{
				pRagdoll->ReleaseRagdoll();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int CEntityDissolve::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !cl_portal_use_new_dissolve.GetBool() )
	{
		// See if we should draw
		if ( gpGlobals->frametime == 0 || m_bReadyToDraw == false )
			return 0;

		CBaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : nullptr;
		if ( pAnimating == nullptr )
			return 0;

		matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
		if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) == false )
			return 0;

		studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
		if ( pStudioHdr == nullptr )
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
		if ( set == nullptr )
			return false;

		// Make sure the emitter is setup properly
		SetupEmitter();

		// Get fade percentages for the effect
		float fadeInPerc = GetFadeInPercentage();
		float fadeOutPerc = GetFadeOutPercentage();

		float fadePerc = ( fadeInPerc >= 1.0f ) ? fadeOutPerc : fadeInPerc;

		Vector vecSkew = vec3_origin;

		// Do extra effects under certain circumstances
		if ( ( fadePerc < 0.99f ) && ( (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) ) )
		{
			DoSparks( set, hitboxbones );
		}

		// Skew the particles in front or in back of their targets
		vecSkew = CurrentViewForward() * ( 8.0f - ( ( 1.0f - fadePerc ) * 32.0f ) );

		float spriteScale = ( ( gpGlobals->curtime - m_flStartTime ) / m_flFadeOutLength );
		spriteScale = clamp( spriteScale, 0.75f, 1.0f );

		// Cache off this material reference
		if ( g_Material_Spark == nullptr )
		{
			g_Material_Spark = ParticleMgr()->GetPMaterial( "effects/spark" );
		}

		if ( g_Material_AR2Glow == nullptr )
		{
			g_Material_AR2Glow = ParticleMgr()->GetPMaterial( "effects/combinemuzzle2" );
		}

		SimpleParticle *sParticle;

		for ( int i = 0; i < set->numhitboxes; ++i )
		{
			Vector vecAbsOrigin, xvec, yvec;
			mstudiobbox_t *pBox = set->pHitbox(i);
			ComputeRenderInfo( pBox, *hitboxbones[pBox->bone], &vecAbsOrigin, &xvec, &yvec );

			Vector offset;
			Vector	xDir, yDir;

			xDir = xvec;
			float xScale = VectorNormalize( xDir ) * 0.75f;

			yDir = yvec;
			float yScale = VectorNormalize( yDir ) * 0.75f;

			int numParticles = clamp( 3.0f * fadePerc, 0, 3 );

			int iTempParts = 2;

			if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
			{
				if ( m_bCoreExplode == true )
				{
					numParticles = 15;
					iTempParts = 20;
				}
			}

			for ( int j = 0; j < iTempParts; j++ )
			{
				// Skew the origin
				offset = xDir * Helper_RandomFloat( -xScale*0.5f, xScale*0.5f ) + yDir * Helper_RandomFloat( -yScale*0.5f, yScale*0.5f );
				offset += vecSkew;

				if ( random_valve->RandomInt( 0, 2 ) != 0 )
					continue;

				sParticle = (SimpleParticle *) m_pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_Spark, vecAbsOrigin + offset );

				if ( sParticle == nullptr )
					return 1;

				sParticle->m_vecVelocity	= Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( 16.0f, 64.0f ) );

				if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
				{
					if ( m_bCoreExplode == true )
					{
						Vector vDirection = (vecAbsOrigin + offset) - m_vDissolverOrigin;
						VectorNormalize( vDirection );
						sParticle->m_vecVelocity = vDirection * m_nMagnitude;
					}
				}

				if ( sParticle->m_vecVelocity.z > 0 )
				{
					sParticle->m_uchStartSize	= random_valve->RandomFloat( 4, 6 ) * spriteScale;
				}
				else
				{
					sParticle->m_uchStartSize	= 2 * spriteScale;
				}

				sParticle->m_flDieTime = random_valve->RandomFloat( 0.4f, 0.5f );

				// If we're the last particles, last longer
				if ( numParticles == 0 )
				{
					sParticle->m_flDieTime *= 2.0f;
					sParticle->m_uchStartSize = 2 * spriteScale;
					sParticle->m_flRollDelta	= Helper_RandomFloat( -4.0f, 4.0f );

					if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
					{
						if ( m_bCoreExplode == true )
						{
							sParticle->m_flDieTime *= 2.0f;
							sParticle->m_flRollDelta	= Helper_RandomFloat( -1.0f, 1.0f );
						}
					}
				}
				else
				{
					sParticle->m_flRollDelta	= Helper_RandomFloat( -8.0f, 8.0f );
				}

				sParticle->m_flLifetime		= 0.0f;

				sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );

				float alpha = 255;

				sParticle->m_uchColor[0]	= alpha;
				sParticle->m_uchColor[1]	= alpha;
				sParticle->m_uchColor[2]	= alpha;
				sParticle->m_uchStartAlpha	= alpha;
				sParticle->m_uchEndAlpha	= 0;
				sParticle->m_uchEndSize		= 0;
			}

			for ( int j = 0; j < numParticles; j++ )
			{
				offset = xDir * Helper_RandomFloat( -xScale*0.5f, xScale*0.5f ) + yDir * Helper_RandomFloat( -yScale*0.5f, yScale*0.5f );
				offset += vecSkew;

				sParticle = (SimpleParticle *) m_pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_AR2Glow, vecAbsOrigin + offset );

				if ( sParticle == nullptr )
					return 1;

				sParticle->m_vecVelocity	= Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -64.0f, 128.0f ) );
				sParticle->m_uchStartSize	= random_valve->RandomFloat( 8, 12 ) * spriteScale;
				sParticle->m_flDieTime		= 0.1f;
				sParticle->m_flLifetime		= 0.0f;

				sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
				sParticle->m_flRollDelta	= Helper_RandomFloat( -2.0f, 2.0f );

				float alpha = 255;

				sParticle->m_uchColor[0]	= alpha;
				sParticle->m_uchColor[1]	= alpha;
				sParticle->m_uchColor[2]	= alpha;
				sParticle->m_uchStartAlpha	= alpha;
				sParticle->m_uchEndAlpha	= 0;
				sParticle->m_uchEndSize		= 0;

				if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
				{
					if ( m_bCoreExplode == true )
					{
						Vector vDirection = (vecAbsOrigin + offset) - m_vDissolverOrigin;

						VectorNormalize( vDirection );

						sParticle->m_vecVelocity = vDirection * m_nMagnitude;

						sParticle->m_flDieTime		= 0.5f;
					}
				}
			}
		}
	}

	return 1;
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Datadesc
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CEntityFlame, DT_EntityFlame, CEntityFlame )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityFlame::CEntityFlame( void ) : m_hEffect( nullptr )
{
	m_hOldAttached = nullptr;
	AddToEntityList( ENTITY_LIST_SIMULATE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityFlame::~CEntityFlame( void )
{
	StopEffect();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFlame::StopEffect( void )
{
	if ( m_hEffect )
	{
		ParticleProp()->StopEmission( m_hEffect, true );
		m_hEffect = nullptr;
	}

	if ( m_hEntAttached )
	{
		m_hEntAttached->RemoveFlag( FL_ONFIRE );
		m_hEntAttached->SetEffectEntity( nullptr );
		m_hEntAttached->StopSound( "General.BurningFlesh" );
		m_hEntAttached->StopSound( "General.BurningObject" );
		m_hEntAttached = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFlame::UpdateOnRemove( void )
{
	StopEffect();
	BaseClass::UpdateOnRemove();
}

void CEntityFlame::CreateEffect( void )
{
	if ( m_hEffect )
	{
		m_hOldAttached = m_hEntAttached;
		ParticleProp()->StopEmission( m_hEffect, true );
		m_hEffect = nullptr;
	}

	CBaseEntity *pEntity = m_hEntAttached;
	if ( pEntity && !pEntity->IsAbleToHaveFireEffect() )
		return;

	m_hEffect = ParticleProp()->Create( m_bCheapEffect ? "burning_gib_01" : "burning_character", PATTACH_ABSORIGIN_FOLLOW );
	if ( m_hEffect )
	{
		m_hOldAttached = m_hEntAttached;

		ParticleProp()->AddControlPoint( m_hEffect, 1, pEntity, PATTACH_ABSORIGIN_FOLLOW );
		m_hEffect->SetControlPoint( 0, GetAbsOrigin() );
		m_hEffect->SetControlPoint( 1, GetAbsOrigin() );
		m_hEffect->SetControlPointEntity( 0, pEntity );
		m_hEffect->SetControlPointEntity( 1, pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFlame::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateEffect();
	}

	// FIXME: This is a bit of a shady path
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		// If our owner changed, then recreate the effect
		if ( m_hEntAttached != m_hOldAttached )
		{
			CreateEffect();
		}
	}

	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEntityFlame::Simulate( void )
{
	if ( gpGlobals->frametime <= 0.0f )
		return true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFlame::ClientThink( void )
{
	StopEffect();
	Release();
}

class CBaseAnimating;

using namespace ::ks::reflect;

ConVar cl_blobulator_freezing_max_metaball_radius( side_registry(), "cl_blobulator_freezing_max_metaball_radius", 
												  "12.0", 
												  FCVAR_NONE, "Setting this can create more complex surfaces on large hitboxes at the cost of performance.", true, 12.0f, true, 100.0f );

//PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectFreezing )
//	PRECACHE( MATERIAL,"effects/tesla_glow_noz" )
//	PRECACHE( MATERIAL,"effects/spark" )
//	PRECACHE( MATERIAL,"effects/combinemuzzle2" )
//PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CEntityFreezing, DT_EntityFreezing, CEntityFreezing )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFreezing::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( GetMoveParent() )
	{
		GetMoveParent()->GetRenderBounds( theMins, theMaxs );
	}
	else
	{
		theMins = GetAbsOrigin();
		theMaxs = theMaxs;
	}
}

//-----------------------------------------------------------------------------
// Yes we bloody are
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t CEntityFreezing::ComputeTranslucencyType( )
{
	return RENDERABLE_IS_TRANSLUCENT;
}

//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void CEntityFreezing::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityFreezing::ClientThink( void )
{
	//CBaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	//if (!pAnimating)
	//	return;

	//color32 color = pAnimating->GetRenderColor();

	//color.r = color.g = ( 1.0f - m_flFrozen ) * 255.0f;

	//// Setup the entity fade
	//pAnimating->SetRenderMode( kRenderTransColor );
	//pAnimating->SetRenderColor( color.r, color.g, color.b, color.a );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int CEntityFreezing::DrawModel( int flags, const RenderableInstance_t &instance )
{
#ifdef USE_BLOBULATOR

	// See if we should draw
	if ( m_bReadyToDraw == false )
		return 0;

	// The parent needs to be a base animating
	CBaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : nullptr;
	if ( pAnimating == nullptr )
		return 0;

	// Make sure we have hitboxes
	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
	if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) == false )
		return 0;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if ( pStudioHdr == nullptr )
		return 0;

	int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), "effects" );
	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
	if ( !set )
		return 0;

	// FIXME: No idea how many particles we'll need, so let's start with 1000
	g_SurfaceRenderParticles.SetCount( 1000 );

	int iNumParticles = 0;

	for ( int i = 0; i < set->numhitboxes; ++i )
	{
		mstudiobbox_t *pBox = set->pHitbox(i);
		matrix3x4_t matBone = *hitboxbones[ pBox->bone ];

		Vector vecHorizontal = pBox->bbmax - pBox->bbmin;

		// Get the particle radius
		float flShortestAxis = std::min( std::min( vecHorizontal.x, vecHorizontal.y ), vecHorizontal.z );
		float flDiameter = clamp( flShortestAxis, 1.0f, cl_blobulator_freezing_max_metaball_radius.GetFloat() );
		float flRadius = flDiameter * 0.5f;
		float flVarience = flRadius * 0.25f;
		float flRadiusIsoSurface = flRadius / 12.0f;

		// Get the hitbox data
		EntityFreezingHitboxBlobData_t *pHitboxBlobData = nullptr;

		if ( m_HitboxBlobData.Count() <= i )
		{
			// We don't have data for this hitbox yet, so build need relative point positions that are along its faces
			int nNewHitboxBlobData = m_HitboxBlobData.AddToTail();
			pHitboxBlobData = &(m_HitboxBlobData[ nNewHitboxBlobData ]);

			// Start in the min corner
			Vector vecStartPoint = pBox->bbmin + ReplicateToVector( flRadius * 0.5f );
			Vector vecEndPoint = pBox->bbmax - ReplicateToVector( flRadius * 0.5f );
			Vector vecPoint;

			bool bEdgeX = true;
			vecPoint.x = vecStartPoint.x;

			// Loop across each axis
			while ( vecPoint.x <= vecEndPoint.x )
			{
				bool bEdgeY = true;
				vecPoint.y = vecStartPoint.y;

				while ( vecPoint.y <= vecEndPoint.y )
				{
					bool bEdgeZ = true;
					vecPoint.z = vecStartPoint.z;

					while ( vecPoint.z <= vecEndPoint.z )
					{
						// Only add particles not in the middle of the box
						if ( bEdgeX || bEdgeY || bEdgeZ )
						{
							int nNewPoint = pHitboxBlobData->m_vPoints.AddToTail();
							pHitboxBlobData->m_vPoints[ nNewPoint ] = vecPoint + RandomVector( -flVarience, flVarience );
						}

						// Make sure the final particles don't stick out past the edge
						bEdgeZ = ( vecPoint.z < vecEndPoint.z && ( ( !bEdgeX && !bEdgeY ) || vecPoint.z + flRadius >= vecEndPoint.z ) );
						if ( bEdgeZ )
						{
							vecPoint.z = vecEndPoint.z;
						}
						else
						{
							vecPoint.z += flRadius;
						}
					}

					// Make sure the final particles don't stick out past the edge
					bEdgeY = ( vecPoint.y < vecEndPoint.y && vecPoint.y + flRadius >= vecEndPoint.y );
					if ( bEdgeY )
					{
						vecPoint.y = vecEndPoint.y;
					}
					else
					{
						vecPoint.y += flRadius;
					}
				}

				// Make sure the final particles don't stick out past the edge
				bEdgeX = ( vecPoint.x < vecEndPoint.x && vecPoint.x + flRadius >= vecEndPoint.x );
				if ( bEdgeX )
				{
					vecPoint.x = vecEndPoint.x;
				}
				else
				{
					vecPoint.x += flRadius;
				}
			}
		}
		else
		{
			pHitboxBlobData = &(m_HitboxBlobData[ i ]);
		}

		// Decide which of the hitbox points to draw based on how frozen the hitbox is and transform them into worldspace
		for ( int nPoint = 0; nPoint < pHitboxBlobData->m_vPoints.Count() && iNumParticles < 1000; ++nPoint )
		{
			// Fast out if the min Z surrounding this hitbox is above the cut off height
			Vector vecBoxAbsMins, vecBoxAbsMaxs;
			TransformAABB( matBone, pBox->bbmin, pBox->bbmax, vecBoxAbsMins, vecBoxAbsMaxs );

			if ( m_bFinishFreezing )
			{
				m_flFrozenPerHitbox[ i ] = std::min( 1.0f, m_flFrozenPerHitbox[ i ] + gpGlobals->frametime * 0.75f );
			}

			if ( m_flFrozenPerHitbox[ i ] <= 0.0f )
			{
				// No particles will be below the freezing line, skip this hitbox
				continue;
			}

			float fCutOffHeight = vecBoxAbsMins.z + ( vecBoxAbsMaxs.z - vecBoxAbsMins.z ) * m_flFrozenPerHitbox[ i ];

			// Get the point in worldspace
			Vector vecTransformedPoint;
			VectorTransform( pHitboxBlobData->m_vPoints[ nPoint ], matBone, vecTransformedPoint );

			// Only add particles below this height and if it's not in the middle of the box
			if ( vecTransformedPoint.z < fCutOffHeight )
			{
				ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[ iNumParticles ]);
				imp_particle->center = vecTransformedPoint;
				imp_particle->setFieldScale( flRadiusIsoSurface );
				imp_particle->interpolants1.set( 1.0f, 1.0f, 1.0f );
				imp_particle->interpolants1[ 3 ] = 0.0f;
				++iNumParticles;
			}
		}
	}

	g_SurfaceRenderParticles.SetCountNonDestructively( iNumParticles );

	// Set up lighting
	modelrender->SetupLighting( GetRenderOrigin() );

	Surface_Draw( GetClientRenderable(), GetRenderOrigin(), materials->FindMaterial( "models/weapons/w_icegun/ice_surface", TEXTURE_GROUP_OTHER, true ), 6.5f );
#endif

	return 1;
}

enum
{
	SERIAL_MASK = 0x7fff // the max value of a serial number, rolls back to 0 when it hits this limit
};

void CEntInfo::ClearLinks()
{
	m_pPrev = m_pNext = this;
}

CBaseEntityList::CEntInfoList::CEntInfoList()
{
	m_pHead = nullptr;
	m_pTail = nullptr;
}

// NOTE: Cut from UtlFixedLinkedList<>, UNDONE: Find a way to share this code
void CBaseEntityList::CEntInfoList::LinkBefore( CEntInfo *pBefore, CEntInfo *pElement )
{
	contract_assert( pElement );
	
	// Unlink it if it's in the list at the moment
	Unlink(pElement);
	
	// The element *after* our newly linked one is the one we linked before.
	pElement->m_pNext = pBefore;
	
	if (pBefore == nullptr)
	{
		// In this case, we're linking to the end of the list, so reset the tail
		pElement->m_pPrev = m_pTail;
		m_pTail = pElement;
	}
	else
	{
		// Here, we're not linking to the end. Set the prev pointer to point to
		// the element we're linking.
		pElement->m_pPrev = pBefore->m_pPrev;
		pBefore->m_pPrev = pElement;
	}
	
	// Reset the head if we linked to the head of the list
	if (pElement->m_pPrev == nullptr)
	{
		m_pHead = pElement;
	}
	else
	{
		pElement->m_pPrev->m_pNext = pElement;
	}
}

void CBaseEntityList::CEntInfoList::LinkAfter( CEntInfo *pAfter, CEntInfo *pElement )
{
	contract_assert( pElement );
	
	// Unlink it if it's in the list at the moment
	if ( IsInList(pElement) )
		Unlink(pElement);
	
	// The element *before* our newly linked one is the one we linked after
	pElement->m_pPrev = pAfter;
	if (pAfter == nullptr)
	{
		// In this case, we're linking to the head of the list, reset the head
		pElement->m_pNext = m_pHead;
		m_pHead = pElement;
	}
	else
	{
		// Here, we're not linking to the end. Set the next pointer to point to
		// the element we're linking.
		pElement->m_pNext = pAfter->m_pNext;
		pAfter->m_pNext = pElement;
	}
	
	// Reset the tail if we linked to the tail of the list
	if (pElement->m_pNext == nullptr )
	{
		m_pTail = pElement;
	}
	else
	{
		pElement->m_pNext->m_pPrev = pElement;
	}
}

void CBaseEntityList::CEntInfoList::Unlink( CEntInfo *pElement )
{
	if (IsInList(pElement))
	{
		// If we're the first guy, reset the head
		// otherwise, make our previous node's next pointer = our next
		if ( pElement->m_pPrev )
		{
			pElement->m_pPrev->m_pNext = pElement->m_pNext;
		}
		else
		{
			m_pHead = pElement->m_pNext;
		}
		
		// If we're the last guy, reset the tail
		// otherwise, make our next node's prev pointer = our prev
		if ( pElement->m_pNext )
		{
			pElement->m_pNext->m_pPrev = pElement->m_pPrev;
		}
		else
		{
			m_pTail = pElement->m_pPrev;
		}
		
		// This marks this node as not in the list, 
		// but not in the free list either
		pElement->ClearLinks();
	}
}

bool CBaseEntityList::CEntInfoList::IsInList( CEntInfo *pElement )
{
	return pElement->m_pPrev != pElement;
}

CBaseEntityList::CBaseEntityList()
{
	// These are not in any list (yet)
	int i;
	for ( i = 0; i < NUM_ENT_ENTRIES; i++ )
	{
		m_EntPtrArray[i].ClearLinks();
		m_EntPtrArray[i].m_SerialNumber = (std::rand()& SERIAL_MASK); // generate random starting serial number
		m_EntPtrArray[i].m_pEntity = nullptr;
	}

	// make a free list of the non-networkable entities
	// Initially, all the slots are free.
	for ( i=MAX_EDICTS+1; i < NUM_ENT_ENTRIES; i++ )
	{
		CEntInfo *pList = &m_EntPtrArray[i];
		m_freeNonNetworkableList.AddToTail( pList );
	}
}

CBaseEntityList::~CBaseEntityList()
{
	CEntInfo *pList = m_activeList.Head();

	while ( pList )
	{
		CEntInfo *pNext = pList->m_pNext;
		RemoveEntityAtSlot( GetEntInfoIndex( pList ) );
		pList = pNext;
	}
}

CBaseHandle CBaseEntityList::AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum )
{
	contract_assert( index >= 0 && index < MAX_EDICTS );
	return AddEntityAtSlot( pEnt, index, iForcedSerialNum );
}

CBaseHandle CBaseEntityList::AddNonNetworkableEntity( IHandleEntity *pEnt )
{
	// Find a slot for it.
	CEntInfo *pSlot = m_freeNonNetworkableList.Head();
	if ( !pSlot )
	{
		Warning( "CBaseEntityList::AddNonNetworkableEntity: no free slots!\n" );
		contract_assert( 0 );
		return CBaseHandle();
	}

	// Move from the free list into the allocated list.
	m_freeNonNetworkableList.Unlink( pSlot );
	int iSlot = GetEntInfoIndex( pSlot );
	
	return AddEntityAtSlot( pEnt, iSlot, -1 );
}

void CBaseEntityList::RemoveEntity( CBaseHandle handle )
{
	RemoveEntityAtSlot( handle.GetEntryIndex() );
}

CBaseHandle CBaseEntityList::AddEntityAtSlot( IHandleEntity *pEnt, int iSlot, int iForcedSerialNum )
{
	// Init the CSerialEntity.
	CEntInfo *pSlot = &m_EntPtrArray[iSlot];
	contract_assert( pSlot->m_pEntity == nullptr );
	pSlot->m_pEntity = pEnt;

	// Force the serial number (client-only)?
	if ( iForcedSerialNum != -1 )
	{
		pSlot->m_SerialNumber = iForcedSerialNum;
		
	}
	
	// Update our list of active entities.
	m_activeList.AddToTail( pSlot );
	CBaseHandle retVal( iSlot, pSlot->m_SerialNumber );

	// Tell the entity to store its handle.
	pEnt->SetRefEHandle( retVal );

	// Notify any derived class.
	OnAddEntity( pEnt, retVal );
	return retVal;
}

void CBaseEntityList::RemoveEntityAtSlot( int iSlot )
{
	contract_assert( iSlot >= 0 && iSlot < NUM_ENT_ENTRIES );

	CEntInfo *pInfo = &m_EntPtrArray[iSlot];

	if ( pInfo->m_pEntity )
	{
		pInfo->m_pEntity->SetRefEHandle( INVALID_EHANDLE );

		// Notify the derived class that we're about to remove this entity.
		OnRemoveEntity( pInfo->m_pEntity, CBaseHandle( iSlot, pInfo->m_SerialNumber ) );

		// Increment the serial # so ehandles go invalid.
		pInfo->m_pEntity = nullptr;
		pInfo->m_SerialNumber = ( pInfo->m_SerialNumber+1)& SERIAL_MASK;

		m_activeList.Unlink( pInfo );

		// Add the slot back to the free list if it's a non-networkable entity.
		if ( iSlot >= MAX_EDICTS )
		{
			m_freeNonNetworkableList.AddToTail( pInfo );
		}
	}
}

void CBaseEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
}

void CBaseEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
}

int CBaseEntityList::GetEntInfoIndex( const CEntInfo *pEntInfo ) const
{
	contract_assert( pEntInfo );
	int index = (int)(pEntInfo - m_EntPtrArray);
	contract_assert( index >= 0 && index < NUM_ENT_ENTRIES );
	return index;
}

CBaseHandle CBaseEntityList::GetNetworkableHandle( int iEntity ) const
{
	contract_assert( iEntity >= 0 && iEntity < MAX_EDICTS );
	if ( m_EntPtrArray[iEntity].m_pEntity )
		return CBaseHandle( iEntity, m_EntPtrArray[iEntity].m_SerialNumber );
	else
		return CBaseHandle();
}

IHandleEntity* CBaseEntityList::LookupEntity( const CBaseHandle &handle ) const
{
	if ( !handle.IsValid() )
		return nullptr;

	// You can use this to determine when something is trying to resolve
	// handles to static props as if they were handles to ordinary props,
	// but in practice this is done a great deal and seems benign.
	/*
	// 0x40000000 = STATICPROP_EHANDLE_MASK
	AssertMsg( ( handle.GetSerialNumber() != (0x40000000 >> NUM_SERIAL_NUM_SHIFT_BITS) ) , 
		"Tried to look up a static prop as if it was a regular entity, a bad pointer and doom are the result.\n" ); 
	*/

	const CEntInfo *pInfo = &m_EntPtrArray[ handle.GetEntryIndex() ];
	if ( pInfo->m_SerialNumber == handle.GetSerialNumber() )
		return pInfo->m_pEntity;
	else
		return nullptr;
}

IHandleEntity* CBaseEntityList::LookupEntityByNetworkIndex( int edictIndex ) const
{
	// (Legacy support).
	if ( edictIndex < 0 )
		return nullptr;

	contract_assert( edictIndex < NUM_ENT_ENTRIES );
	return m_EntPtrArray[edictIndex].m_pEntity;
}

CBaseHandle CBaseEntityList::FirstHandle() const
{
	if ( !m_activeList.Head() )
		return INVALID_EHANDLE;

	int index = GetEntInfoIndex( m_activeList.Head() );
	return CBaseHandle( index, m_EntPtrArray[index].m_SerialNumber );
}

CBaseHandle CBaseEntityList::NextHandle( CBaseHandle hEnt ) const
{
	int iSlot = hEnt.GetEntryIndex();
	CEntInfo *pNext = m_EntPtrArray[iSlot].m_pNext;
	if ( !pNext )
		return INVALID_EHANDLE;

	int index = GetEntInfoIndex( pNext );

	return CBaseHandle( index, m_EntPtrArray[index].m_SerialNumber );
}

CBaseHandle CBaseEntityList::InvalidHandle()
{
	return INVALID_EHANDLE;
}

const CEntInfo *CBaseEntityList::FirstEntInfo() const
{
	return m_activeList.Head();
}

const CEntInfo *CBaseEntityList::NextEntInfo( const CEntInfo *pInfo ) const
{
	return pInfo->m_pNext;
}

const CEntInfo *CBaseEntityList::GetEntInfoPtr( const CBaseHandle &hEnt ) const
{
	int iSlot = hEnt.GetEntryIndex();
	return &m_EntPtrArray[iSlot];
}

const CEntInfo *CBaseEntityList::GetEntInfoPtrByIndex( int index ) const
{
	return &m_EntPtrArray[index];
}

void CBaseEntityList::ForceEntSerialNumber( int iEntIndex, int iSerialNumber )
{
	m_EntPtrArray[iEntIndex].m_SerialNumber = iSerialNumber;
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: A base class for all material proxies in the client dll

// identifier was truncated to '255' characters in the debug information

class CEntityOriginMaterialProxy : public CEntityMaterialProxy
{
public:
	CEntityOriginMaterialProxy()
	{
		m_pMaterial = nullptr;
		m_pOriginVar = nullptr;
	}
	virtual ~CEntityOriginMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$entityorigin", &found );
		if( !found )
		{
			m_pOriginVar = nullptr;
			return false;
		}
		return true;
	}
	virtual void OnBind( CBaseEntity *pC_BaseEntity )
	{
		const Vector &origin = pC_BaseEntity->GetAbsOrigin();
		m_pOriginVar->SetVecValue( origin.x, origin.y, origin.z );
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEntityOriginMaterialProxy, EntityOrigin );

/*
extern std::vector< EHANDLE > g_SmokeGrenadeHandles;

static constexpr auto SMOKE_PROXY_RADIUS_SQUARED = 30000; //approx smoke radius ^2
ConVar cl_debug_smoke_proxy( side_registry(), "cl_debug_smoke_proxy", "0", FCVAR_CHEAT );
class CSmokeOriginMaterialProxy : public CEntityMaterialProxy
{
public:
	CSmokeOriginMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pOriginVar = NULL;

	}
	virtual ~CSmokeOriginMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$BlendWithSmokeGrenadePosEntity", &found );
		if( !found )
		{
			m_pOriginVar = NULL;
			return false;
		}
		m_pSmokeOriginVar = m_pMaterial->FindVar( "$BlendWithSmokeGrenadePosSmoke", &found );
		if( !found )
		{
			m_pSmokeOriginVar = NULL;
			return false;
		}
		return true;
	}
	virtual void OnBind( CBaseEntity *pC_BaseEntity )
	{
		if ( !g_SmokeGrenadeHandles.Count() )
		{
			m_pSmokeOriginVar->SetVecValue( 0, 0, 0 );
			return;
		}

		CBaseAnimating *pBaseAnimating = pC_BaseEntity ? pC_BaseEntity->GetBaseAnimating() : NULL;
		if ( pBaseAnimating )
		{
			CStudioHdr *pHdr = pBaseAnimating->GetModelPtr();
			if ( pHdr )
			{
				matrix3x4_t matTemp;
				AngleMatrix( pC_BaseEntity->GetAbsAngles(), matTemp );

				Vector vecWorldRelativeIllumPos;
				VectorRotate( pHdr->illumposition(), matTemp, vecWorldRelativeIllumPos );

				Vector origin = pC_BaseEntity->GetAbsOrigin() + vecWorldRelativeIllumPos;
				m_pOriginVar->SetVecValue( origin.x, origin.y, origin.z );

				Vector vecClosestGrenadePos;
				vecClosestGrenadePos.Init();

				//find closest smoke grenade
				float flClosestDist = SMOKE_PROXY_RADIUS_SQUARED;
				for ( int i = 0; i < g_SmokeGrenadeHandles.Count(); i++ )
				{
					CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[i].Get() );

					if ( !pGrenade )
						continue;

					float flTemp = pGrenade->GetAbsOrigin().DistToSqr( origin - Vector(0,0,69) );
					if ( flTemp < flClosestDist )
					{
						flClosestDist = flTemp;
						vecClosestGrenadePos = pGrenade->GetAbsOrigin() + Vector(0,0,69);
					}
				}

				m_pSmokeOriginVar->SetVecValue( vecClosestGrenadePos.x, vecClosestGrenadePos.y, vecClosestGrenadePos.z );

				if ( cl_debug_smoke_proxy.GetBool() )
				{
					debugoverlay->AddBoxOverlay( origin, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255,0,0,0, 0 );
					if ( flClosestDist < SMOKE_PROXY_RADIUS_SQUARED )
					{
						debugoverlay->AddLineOverlay( origin, vecClosestGrenadePos, 255,0,0, true, 0 );
					}
				}
			}
		}
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
	IMaterialVar *m_pSmokeOriginVar;
};

EXPOSE_MATERIAL_PROXY( CSmokeOriginMaterialProxy, SmokeOrigin );
*/

//=================================================================================================================
// This is a last-minute hack to ship Orange Box on the 360!
//=================================================================================================================
class CEntityOriginAlyxMaterialProxy : public CEntityMaterialProxy
{
public:
	CEntityOriginAlyxMaterialProxy()
	{
		m_pMaterial = nullptr;
		m_pOriginVar = nullptr;
	}
	virtual ~CEntityOriginAlyxMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$entityorigin", &found );
		if( !found )
		{
			m_pOriginVar = nullptr;
			return false;
		}
		return true;
	}
	virtual void OnBind( CBaseEntity *pC_BaseEntity )
	{
		const Vector &origin = pC_BaseEntity->GetAbsOrigin();
		m_pOriginVar->SetVecValue( origin.x - 15.0f, origin.y, origin.z );
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEntityOriginAlyxMaterialProxy, EntityOriginAlyx );

//=================================================================================================================
// This is a last-minute hack to ship Orange Box on the 360!
//=================================================================================================================
class CEp1IntroVortRefractMaterialProxy : public CEntityMaterialProxy
{
public:
	CEp1IntroVortRefractMaterialProxy()
	{
		m_pMaterial = nullptr;
		m_pOriginVar = nullptr;
	}
	virtual ~CEp1IntroVortRefractMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$refractamount", &found );
		if( !found )
		{
			m_pOriginVar = nullptr;
			return false;
		}
		return true;
	}
	virtual void OnBind( CBaseEntity *pC_BaseEntity )
	{
		if ( m_pOriginVar != nullptr)
		{
			float flTmp = ( 1.0f - m_pOriginVar->GetFloatValue() );
			flTmp *= flTmp;
			flTmp *= flTmp;
			flTmp = ( 1.0f - flTmp ) * 0.25f;
			m_pOriginVar->SetFloatValue( flTmp );
		}
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEp1IntroVortRefractMaterialProxy, Ep1IntroVortRefract );

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseAnimating;
class CBaseEntity;
class CBaseParticleEntity;
class SimpleParticle;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Entity particle trail, client-side implementation
//-----------------------------------------------------------------------------
class CEntityParticleTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CEntityParticleTrail, CBaseParticleEntity );

	CEntityParticleTrail( );
	~CEntityParticleTrail( );

// CBaseEntity
	virtual void OnDataChanged( DataUpdateType_t updateType );

// IParticleEffect
	void Update( float fTimeDelta );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

private:

	CEntityParticleTrail( const CEntityParticleTrail & ); // not defined, not accessible

	void Start( );
	void AddParticle( float flInitialDeltaTime, const Vector &vecMins, const Vector &vecMaxs, const matrix3x4_t &boxToWorld );

	[[= ks::reflect::Net{} ]] int		m_iMaterialName;
	[[= ks::reflect::Net{} ]] EntityParticleTrailInfo_t	m_Info;
	[[= ks::reflect::Net{} ]] EHANDLE m_hConstraintEntity;

	PMaterialHandle		m_hMaterial;
	TimedEvent			m_teParticleSpawn;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CEntityParticleTrail, DT_EntityParticleTrail, CEntityParticleTrail )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityParticleTrail::CEntityParticleTrail( void )
{
}

CEntityParticleTrail::~CEntityParticleTrail()
{
	ParticleMgr()->RemoveEffect( &m_ParticleEffect );
}

//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void CEntityParticleTrail::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CEntityParticleTrail::Start( )
{
	if( ParticleMgr()->AddEffect( &m_ParticleEffect, this ) == false )
		return;

	const char *pMaterialName = GetMaterialNameFromIndex( m_iMaterialName );
	if ( !pMaterialName )
		return;

	m_hMaterial	= ParticleMgr()->GetPMaterial( pMaterialName );	
	m_teParticleSpawn.Init( 150 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityParticleTrail::AddParticle( float flInitialDeltaTime, const Vector &vecMins, const Vector &vecMaxs, const matrix3x4_t &boxToWorld )
{
	// Select a random point somewhere in the hitboxes of the entity.
	Vector vecLocalPosition, vecWorldPosition;
	vecLocalPosition.x			= Lerp( random_valve->RandomFloat( 0.0f, 1.0f ), vecMins.x, vecMaxs.x );
	vecLocalPosition.y			= Lerp( random_valve->RandomFloat( 0.0f, 1.0f ), vecMins.y, vecMaxs.y );
	vecLocalPosition.z			= Lerp( random_valve->RandomFloat( 0.0f, 1.0f ), vecMins.z, vecMaxs.z );
	VectorTransform( vecLocalPosition, boxToWorld, vecWorldPosition );

	// Don't emit the particle unless it's inside the model
	if ( m_hConstraintEntity.Get() )
	{
		Ray_t ray;
		trace_t tr;
		ray.Init( vecWorldPosition, vecWorldPosition );
		enginetrace->ClipRayToEntity( ray, MASK_ALL, m_hConstraintEntity, &tr );
		
		if ( !tr.startsolid )
			return;
	}

	// Make a new particle
	SimpleParticle *pParticle = (SimpleParticle *)m_ParticleEffect.AddParticle( sizeof(SimpleParticle), m_hMaterial );
	if ( pParticle == nullptr )
		return;

	pParticle->m_Pos			= vecWorldPosition;
	pParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= Helper_RandomFloat( -2.0f, 2.0f );

	pParticle->m_flLifetime		= flInitialDeltaTime;
	pParticle->m_flDieTime		= m_Info.m_flLifetime;

	pParticle->m_uchColor[0]	= 64;
	pParticle->m_uchColor[1]	= 140;
	pParticle->m_uchColor[2]	= 225;
	pParticle->m_uchStartAlpha	= Helper_RandomInt( 64, 64 );
	pParticle->m_uchEndAlpha	= 0;

	pParticle->m_uchStartSize	= m_Info.m_flStartSize;
	pParticle->m_uchEndSize		= m_Info.m_flEndSize;

	pParticle->m_vecVelocity	= vec3_origin;
	VectorMA( pParticle->m_Pos, flInitialDeltaTime, pParticle->m_vecVelocity, pParticle->m_Pos );  
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CEntityParticleTrail::Update( float fTimeDelta )
{
	float tempDelta = fTimeDelta;
	studiohdr_t *pStudioHdr;
	mstudiohitboxset_t *set;
	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
		return;

	CBaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
	if (!pAnimating)
		goto trailNoHitboxes;

	if ( !pAnimating->HitboxToWorldTransforms( hitboxbones ) )
		goto trailNoHitboxes;

	pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if (!pStudioHdr)
		goto trailNoHitboxes;

	set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
	if ( !set )
		goto trailNoHitboxes;

	//Add new particles
	while ( m_teParticleSpawn.NextEvent( tempDelta ) )
	{
		int nHitbox = random_valve->RandomInt( 0, set->numhitboxes - 1 );
		mstudiobbox_t *pBox = set->pHitbox(nHitbox);

		AddParticle( tempDelta, pBox->bbmin, pBox->bbmax, *hitboxbones[pBox->bone] );
	}
	return;

trailNoHitboxes:
	while ( m_teParticleSpawn.NextEvent( tempDelta ) )
	{
		AddParticle( tempDelta, pMoveParent->CollisionProp()->OBBMins(), pMoveParent->CollisionProp()->OBBMaxs(), pMoveParent->EntityToWorldTransform() );
	}
}

void CEntityParticleTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SimpleParticle *pParticle = (const SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		float t = pParticle->m_flLifetime / pParticle->m_flDieTime;

		// Render
		Vector	tPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = tPos.z;

		Vector	color = Vector( pParticle->m_uchColor[0] / 255.0f, pParticle->m_uchColor[1] / 255.0f, pParticle->m_uchColor[2] / 255.0f );
		float alpha = Lerp( t, pParticle->m_uchStartAlpha / 255.0f, pParticle->m_uchEndAlpha / 255.0f );
		float flSize = Lerp( t, pParticle->m_uchStartSize, pParticle->m_uchEndSize );

		// Render it
		RenderParticle_ColorSize( pIterator->GetParticleDraw(), tPos, color, alpha, flSize );
		
		pParticle = (const SimpleParticle*)pIterator->GetNext( sortKey );
	}
}

void CEntityParticleTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Update position
		float flTimeDelta = pIterator->GetTimeDelta();
		pParticle->m_Pos += pParticle->m_vecVelocity * flTimeDelta;

		// NOTE: I'm overloading "die time" to be the actual start time.
		pParticle->m_flLifetime += flTimeDelta;

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}

using namespace ::ks::reflect;

// Purpose: Drops particles where the entity was.

//-----------------------------------------------------------------------------
// Default values
//-----------------------------------------------------------------------------
EntityParticleTrailInfo_t::EntityParticleTrailInfo_t()
{
	m_strMaterialName = NULL_STRING;
	m_flLifetime = 4.0f;
	m_flStartSize = 2.0f;
	m_flEndSize = 3.0f;
}

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( EntityParticleTrailInfo_t, DT_EntityParticleTrailInfo );
