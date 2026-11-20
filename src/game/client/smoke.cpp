// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "materialsystem/imesh.h"
#include "tier2/resourceprecacher.h"
#include "client_class.h"
#include "declare_class.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "icliententity.h"
#include "reflect_annotations.h"
#include "smoke_trail.h"
#include "te_effect_dispatch.h"
#include "cdll_client_int.h"
#include "reflect_recvtable.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "irecipientfilter.h"
#include "soundchars.h"
#include "utils/common/scriplib.h"
#include "baseentity.h"
#include "ihandleentity.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "filesystem.h"
#include "tier2/interval.h"
#include "dlight.h"
module ks.client;

import std;
import ks.core.numeric;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :precache;
import :achievements_and_stats_interface;
import :baseentity;
import ks.game.gamemovement;
import :particlemgr;
import ks.core.basetypes;
import ks.game.entity_cast;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vmatrix;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.intrusive_ptr;
import ks.tier1.registry;
import :noopanimcontroller;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.math.vector2d;
import ks.tier1.exprevaluator;
import ks.tier1.strtools;
import ks.core.color;
import ks.core.dbg;
import ks.core.logging;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.vprof;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.core.tslist;
import ks.tier1.envelope;
import ks.tier1.fixed_pool;
import ks.tier1.vector_ops;
import ks.core.cmdline;
import ks.math.vector4d;

using namespace ::ks::reflect;

static IMaterial *g_pSmokeFogMaterial = nullptr;

float		g_SmokeFogOverlayAlpha;
Vector		g_SmokeFogOverlayColor;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheSmokeFogOverlay )
PRECACHE( MATERIAL, "particle/screenspace_fog" )
PRECACHE_REGISTER_END()

void InitSmokeFogOverlay()
{
	TermSmokeFogOverlay();
	
	g_SmokeFogOverlayAlpha = 0;
	g_SmokeFogOverlayColor.Init( 0.0f, 0.0f, 0.0f );

	if(materials)
	{
		g_pSmokeFogMaterial = materials->FindMaterial( "particle/screenspace_fog", TEXTURE_GROUP_CLIENT_EFFECTS );
		if(g_pSmokeFogMaterial)
			g_pSmokeFogMaterial->IncrementReferenceCount();
	}
}

void TermSmokeFogOverlay()
{
	if(g_pSmokeFogMaterial)
	{
		g_pSmokeFogMaterial->DecrementReferenceCount();
		g_pSmokeFogMaterial = nullptr;
	}
}

void DrawSmokeFogOverlay()
{
	if(g_SmokeFogOverlayAlpha == 0 || !g_pSmokeFogMaterial || !materials)
		return;
	
	CMatRenderContextPtr pRenderContext( materials );
	( pRenderContext, "DrawSmokeFogOverlay()" );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, 0, 1, 1, -99999, 99999 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	IMesh* pMesh = pRenderContext->GetDynamicMesh( false, nullptr, nullptr, g_pSmokeFogMaterial );
	CMeshBuilder meshBuilder;

	static float dist = 10;

	// g_SmokeFogOverlayColor is the sum total of all the smoke colors weighted by their alpha contribution and added.
	// Dividing by the g_SmokeFogOverlayAlpha gives us the weighted average color of all the smoke plumes that are influencing
	// the fog overlay. 
	Vector vColor = g_SmokeFogOverlayColor/* / g_SmokeFogOverlayAlpha*/;

	vColor.x = std::min<std::common_type_t<float, int>>(std::max<std::common_type_t<vec_t, int>>(vColor.x, 0), 1);
	vColor.y = std::min<std::common_type_t<float, int>>(std::max<std::common_type_t<vec_t, int>>(vColor.y, 0), 1);
	vColor.z = std::min<std::common_type_t<float, int>>(std::max<std::common_type_t<vec_t, int>>(vColor.z, 0), 1);
	float alpha = std::min<std::common_type_t<float, int>>(std::max<std::common_type_t<float, int>>(g_SmokeFogOverlayAlpha, 0), 1);

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( 0, 0, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0, 1, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, 1, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, 0, dist );
	meshBuilder.Color4f( vColor.x, vColor.y, vColor.z, alpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

class CEffectData;
class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
class SimpleParticle;

using namespace ::ks::reflect;

//
// CRocketTrailParticle
//

class CRocketTrailParticle : public CSimpleEmitter
{
public:
	
	CRocketTrailParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CRocketTrailParticle *Create( const char *pDebugName )
	{
		return new CRocketTrailParticle( pDebugName );
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -8.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
		}

		return pParticle->m_flRoll;
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
	}

private:
	CRocketTrailParticle( const CRocketTrailParticle & );
};

//
// CSmokeParticle
//

class CSmokeParticle : public CSimpleEmitter
{
public:
	
	CSmokeParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CSmokeParticle *Create( const char *pDebugName )
	{
		return new CSmokeParticle( pDebugName );
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
	}

	//Color
	virtual Vector UpdateColor( const SimpleParticle *pParticle )
	{
		Vector	color;

		float	tLifetime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float	ramp = 1.0f - tLifetime;

		color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
		color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
		color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

		return color;
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -8.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
		}

		return pParticle->m_flRoll;
	}

private:
	CSmokeParticle( const CSmokeParticle & );
};

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_REFLECT_CLIENTCLASS( CSmokeTrail, DT_SmokeTrail, SmokeTrail )

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
CSmokeTrail::CSmokeTrail()
{
	m_MaterialHandle[0] = nullptr;
	m_MaterialHandle[1] = nullptr;

	m_SpawnRate = 10;
	m_ParticleSpawn.Init(10);
	m_StartColor.Init(0.5, 0.5, 0.5);
	m_EndColor.Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0;	// No end time
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_VelocityOffset.Init();
	m_Opacity = 0.5f;

	m_bEmit = true;

	m_nAttachment	= -1;

	m_pSmokeEmitter = nullptr;
	m_pParticleMgr	= nullptr;
}

CSmokeTrail::~CSmokeTrail()
{

	if ( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSmokeTrail::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pAttachedTo->GetBaseEntity() );
	if (pEnt && (m_nAttachment > 0))
	{
		pEnt->GetAttachment( m_nAttachment, *pAbsOrigin, *pAbsAngles );
	}
	else
	{
		BaseClass::GetAimEntOrigin( pAttachedTo, pAbsOrigin, pAbsAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEmit - 
//-----------------------------------------------------------------------------
void CSmokeTrail::SetEmit(bool bEmit)
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rate - 
//-----------------------------------------------------------------------------
void CSmokeTrail::SetSpawnRate(float rate)
{
	m_SpawnRate = rate;
	m_ParticleSpawn.Init(rate);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CSmokeTrail::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CSmokeTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;

	m_pParticleMgr	= pParticleMgr;
	m_pSmokeEmitter = CSmokeParticle::Create("smokeTrail");
	
	if ( !m_pSmokeEmitter )
	{
		contract_assert( false );
		return;
	}

	m_pSmokeEmitter->SetSortOrigin( GetAbsOrigin() );
	m_pSmokeEmitter->SetNearClip( 64.0f, 128.0f );

	m_MaterialHandle[0] = g_Mat_DustPuff[0];
	m_MaterialHandle[1] = g_Mat_DustPuff[1];
	
	m_ParticleSpawn.Init( m_SpawnRate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CSmokeTrail::Update( float fTimeDelta )
{
	if ( !m_pSmokeEmitter )
		return;

	Vector	offsetColor;

	// Add new particles
	if ( !m_bEmit )
		return;

	if ( ( m_StopEmitTime != 0 ) && ( m_StopEmitTime <= gpGlobals->curtime ) )
		return;

	float tempDelta = fTimeDelta;

	SimpleParticle	*pParticle;
	Vector			offset;

	Vector vecOrigin;
	VectorMA( GetAbsOrigin(), -fTimeDelta, GetAbsVelocity(), vecOrigin );

	Vector vecForward;
	GetVectors( &vecForward, nullptr, nullptr );

	while( m_ParticleSpawn.NextEvent( tempDelta ) )
	{
		float fldt = fTimeDelta - tempDelta;

		offset.Random( -m_SpawnRadius, m_SpawnRadius );
		offset += vecOrigin;
		VectorMA( offset, fldt, GetAbsVelocity(), offset );

		pParticle = (SimpleParticle *) m_pSmokeEmitter->AddParticle( sizeof( SimpleParticle ), m_MaterialHandle[random_valve->RandomInt(0,1)], offset );

		if ( pParticle == nullptr )
			continue;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= m_ParticleLifetime;

		pParticle->m_vecVelocity.Random( -1.0f, 1.0f );
		pParticle->m_vecVelocity *= random_valve->RandomFloat( m_MinSpeed, m_MaxSpeed );

		pParticle->m_vecVelocity = pParticle->m_vecVelocity + GetAbsVelocity();
		
		float flDirectedVel = random_valve->RandomFloat( m_MinDirectedSpeed, m_MaxDirectedSpeed );
		VectorMA( pParticle->m_vecVelocity, flDirectedVel, vecForward, pParticle->m_vecVelocity );

		offsetColor = m_StartColor;
		float flMaxVal = std::max( m_StartColor[0], m_StartColor[1] );
		if ( flMaxVal < m_StartColor[2] )
		{
			flMaxVal = m_StartColor[2];
		}
		offsetColor /= flMaxVal;

		offsetColor *= random_valve->RandomFloat( -0.2f, 0.2f );
		offsetColor += m_StartColor;

		offsetColor[0] = clamp( offsetColor[0], 0.0f, 1.0f );
		offsetColor[1] = clamp( offsetColor[1], 0.0f, 1.0f );
		offsetColor[2] = clamp( offsetColor[2], 0.0f, 1.0f );

		pParticle->m_uchColor[0]	= offsetColor[0]*255.0f;
		pParticle->m_uchColor[1]	= offsetColor[1]*255.0f;
		pParticle->m_uchColor[2]	= offsetColor[2]*255.0f;
		
		pParticle->m_uchStartSize	= m_StartSize;
		pParticle->m_uchEndSize		= m_EndSize;
		
		float alpha = random_valve->RandomFloat( m_Opacity*0.75f, m_Opacity*1.25f );
		alpha = clamp( alpha, 0.0f, 1.0f );

		pParticle->m_uchStartAlpha	= alpha * 255; 
		pParticle->m_uchEndAlpha	= 0;
			
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1.0f, 1.0f );
    }
}

void CSmokeTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
}

void CSmokeTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
}

//==================================================
// RocketTrail
//==================================================

// Expose to the particle app.

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_REFLECT_CLIENTCLASS( CRocketTrail, DT_RocketTrail, RocketTrail )

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
CRocketTrail::CRocketTrail()
{
	m_MaterialHandle[0] = nullptr;
	m_MaterialHandle[1] = nullptr;
	
	m_SpawnRate = 10;
	m_ParticleSpawn.Init(10);
	m_StartColor.Init(0.5, 0.5, 0.5);
	m_EndColor.Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0;	// No end time
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_VelocityOffset.Init();
	m_Opacity = 0.5f;

	m_bEmit		= true;
	m_bDamaged	= false;

	m_nAttachment	= -1;

	m_pRocketEmitter = nullptr;
	m_pParticleMgr	= nullptr;
}

CRocketTrail::~CRocketTrail()
{
	if ( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRocketTrail::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pAttachedTo->GetBaseEntity() );
	if (pEnt && (m_nAttachment > 0))
	{
		pEnt->GetAttachment( m_nAttachment, *pAbsOrigin, *pAbsAngles );
	}
	else
	{
		BaseClass::GetAimEntOrigin( pAttachedTo, pAbsOrigin, pAbsAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEmit - 
//-----------------------------------------------------------------------------
void CRocketTrail::SetEmit(bool bEmit)
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rate - 
//-----------------------------------------------------------------------------
void CRocketTrail::SetSpawnRate(float rate)
{
	m_SpawnRate = rate;
	m_ParticleSpawn.Init(rate);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CRocketTrail::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CRocketTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;

	m_pParticleMgr	= pParticleMgr;
	m_pRocketEmitter = CRocketTrailParticle::Create("smokeTrail");
	if ( !m_pRocketEmitter )
	{
		contract_assert( false );
		return;
	}

	m_pRocketEmitter->SetSortOrigin( GetAbsOrigin() );
	m_pRocketEmitter->SetNearClip( 64.0f, 128.0f );

	m_MaterialHandle[0] = g_Mat_DustPuff[0];
	m_MaterialHandle[1] = g_Mat_DustPuff[1];
	
	m_ParticleSpawn.Init( m_SpawnRate );

	m_vecLastPosition = GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CRocketTrail::Update( float fTimeDelta )
{
	if ( !m_pRocketEmitter )
		return;

	if ( gpGlobals->frametime == 0.0f )
		return;

	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash" );
	pSimple->SetSortOrigin( GetAbsOrigin() );
	
	SimpleParticle *pParticle;
	Vector			forward, offset;

	AngleVectors( GetAbsAngles(), &forward );
	
	forward.Negate();

	float flScale = random_valve->RandomFloat( m_flFlareScale-0.5f, m_flFlareScale+0.5f );

	//
	// Flash
	//

	int i;

	for ( i = 1; i < 9; i++ )
	{
		offset = GetAbsOrigin() + (forward * (i*2.0f*m_flFlareScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( VarArgs( "effects/muzzleflash%d", random_valve->RandomInt(1,4) ) ), offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.01f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 128;

		pParticle->m_uchStartSize	= (random_valve->RandomFloat( 5.0f, 6.0f ) * (12-(i))/9) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	// Add new particles (undamaged version)
	if ( m_bEmit )
	{
		Vector	moveDiff	= GetAbsOrigin() - m_vecLastPosition;
		float	moveLength	= VectorNormalize( moveDiff );

		int	numPuffs = moveLength / ( m_StartSize / 2.0f );

		//debugoverlay->AddLineOverlay( m_vecLastPosition, GetAbsOrigin(), 255, 0, 0, true, 2.0f ); 
		
		//FIXME: More rational cap here, perhaps
		if ( numPuffs > 50 )
			numPuffs = 50;

		Vector			offsetColor;
		float			step = moveLength / numPuffs;

		//Fill in the gaps
		for ( i = 1; i < numPuffs+1; i++ )
		{
			offset = m_vecLastPosition + ( moveDiff * step * i );

			//debugoverlay->AddBoxOverlay( offset, -Vector(2,2,2), Vector(2,2,2), vec3_angle, i*4, i*4, i*4, true, 4.0f );
			
			pParticle = (SimpleParticle *) m_pRocketEmitter->AddParticle( sizeof( SimpleParticle ), m_MaterialHandle[random_valve->RandomInt(0,1)], offset );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime		= 0.0f;
				pParticle->m_flDieTime		= m_ParticleLifetime + random_valve->RandomFloat(m_ParticleLifetime*0.9f,m_ParticleLifetime*1.1f);

				pParticle->m_vecVelocity.Random( -1.0f, 1.0f );
				pParticle->m_vecVelocity *= random_valve->RandomFloat( m_MinSpeed, m_MaxSpeed );
				
				offsetColor = m_StartColor * random_valve->RandomFloat( 0.75f, 1.25f );

				offsetColor[0] = clamp( offsetColor[0], 0.0f, 1.0f );
				offsetColor[1] = clamp( offsetColor[1], 0.0f, 1.0f );
				offsetColor[2] = clamp( offsetColor[2], 0.0f, 1.0f );

				pParticle->m_uchColor[0]	= offsetColor[0]*255.0f;
				pParticle->m_uchColor[1]	= offsetColor[1]*255.0f;
				pParticle->m_uchColor[2]	= offsetColor[2]*255.0f;
				
				pParticle->m_uchStartSize	= m_StartSize * random_valve->RandomFloat( 0.75f, 1.25f );
				pParticle->m_uchEndSize		= m_EndSize * random_valve->RandomFloat( 1.0f, 1.25f );
				
				float alpha = random_valve->RandomFloat( m_Opacity*0.75f, m_Opacity*1.25f );

				if ( alpha > 1.0f )
					alpha = 1.0f;
				if ( alpha < 0.0f )
					alpha = 0.0f;

				pParticle->m_uchStartAlpha	= alpha * 255; 
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
			}
		}
	}
	
	if ( m_bDamaged )
	{
		SimpleParticle	*pParticle;
		Vector			offset;
		Vector			offsetColor;

		ks::intrusive_ptr<CEmberEffect>	pEmitter = CEmberEffect::Create("CRocketTrail::damaged");

		pEmitter->SetSortOrigin( GetAbsOrigin() );

		PMaterialHandle flameMaterial = m_pRocketEmitter->GetPMaterial( VarArgs( "sprites/flamelet%d", random_valve->RandomInt( 1, 4 ) ) );
		
		// Flames from the rocket
		for ( i = 0; i < 8; i++ )
		{
			offset = RandomVector( -8, 8 ) + GetAbsOrigin();

			pParticle = (SimpleParticle *) pEmitter->AddParticle( sizeof( SimpleParticle ), flameMaterial, offset );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime		= 0.0f;
				pParticle->m_flDieTime		= 0.25f;

				pParticle->m_vecVelocity.Random( -1.0f, 1.0f );
				pParticle->m_vecVelocity *= random_valve->RandomFloat( 32, 128 );
				
				offsetColor = m_StartColor * random_valve->RandomFloat( 0.75f, 1.25f );

				offsetColor[0] = clamp( offsetColor[0], 0.0f, 1.0f );
				offsetColor[1] = clamp( offsetColor[1], 0.0f, 1.0f );
				offsetColor[2] = clamp( offsetColor[2], 0.0f, 1.0f );

				pParticle->m_uchColor[0]	= offsetColor[0]*255.0f;
				pParticle->m_uchColor[1]	= offsetColor[1]*255.0f;
				pParticle->m_uchColor[2]	= offsetColor[2]*255.0f;
				
				pParticle->m_uchStartSize	= 8.0f;
				pParticle->m_uchEndSize		= 32.0f;
				
				pParticle->m_uchStartAlpha	= 255;
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
			}
		}
	}

	m_vecLastPosition = GetAbsOrigin();
}

void CRocketTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
}

void CRocketTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
}

SporeEffect::SporeEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}

SporeEffect* SporeEffect::Create( const char *pDebugName )
{
	return new SporeEffect( pDebugName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------
void SporeEffect::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	float	speed = VectorNormalize( pParticle->m_vecVelocity );
	Vector	offset;

	speed -= ( 64.0f * timeDelta );

	offset.Random( -0.5f, 0.5f );

	pParticle->m_vecVelocity += offset;
	VectorNormalize( pParticle->m_vecVelocity );

	pParticle->m_vecVelocity *= speed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector SporeEffect::UpdateColor( const SimpleParticle *pParticle )
{
	Vector	color;
	float	ramp = ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) );//1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime );

	color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
	color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
	color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

	return color;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float SporeEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

//==================================================
// CSporeExplosion
//==================================================

IMPLEMENT_REFLECT_CLIENTCLASS( CSporeExplosion, DT_SporeExplosion, SporeExplosion )

CSporeExplosion::CSporeExplosion( void )
{
	m_pParticleMgr			= nullptr;

	m_flSpawnRate			= 32;
	m_flParticleLifetime	= 5;
	m_flStartSize			= 32;
	m_flEndSize				= 64;
	m_flSpawnRadius			= 32;
	m_pSporeEffect			= nullptr;

	m_teParticleSpawn.Init( 32 );

	m_bEmit = true;
	m_bDontRemove = false;
}

CSporeExplosion::~CSporeExplosion()
{
	if ( m_pParticleMgr != nullptr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CSporeExplosion::OnDataChanged( DataUpdateType_t updateType )
{
	CBaseEntity::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flPreviousSpawnRate = m_flSpawnRate;
		m_teParticleSpawn.Init( m_flSpawnRate );
		Start( ParticleMgr(), nullptr );
	}
	else if( m_bEmit )
	{
		// Just been turned on by the server.
		m_flPreviousSpawnRate = m_flSpawnRate;
		m_teParticleSpawn.Init( m_flSpawnRate );
	}

	m_pSporeEffect->SetDontRemove( m_bDontRemove );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CSporeExplosion::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	//Add us into the effect manager
	if( pParticleMgr->AddEffect( &m_ParticleEffect, this ) == false )
		return;

	//Create our main effect
	m_pSporeEffect = SporeEffect::Create( "CSporeExplosion" );
	
	if ( m_pSporeEffect == nullptr )
		return;

	m_hMaterial	= m_pSporeEffect->GetPMaterial( "particle/fire" );

	m_pSporeEffect->SetSortOrigin( GetAbsOrigin() );
	m_pSporeEffect->SetNearClip( 64, 128 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSporeExplosion::AddParticles( void )
{
	//Spores
	Vector	offset;
	Vector	dir;

	//Get our direction
	AngleVectors( GetAbsAngles(), &dir );

	SimpleParticle	*sParticle;

	for ( int i = 0; i < 4; i++ )
	{
		//Add small particle to the effect's origin
		offset.Random( -m_flSpawnRadius, m_flSpawnRadius );
		sParticle = (SimpleParticle *) m_pSporeEffect->AddParticle( sizeof(SimpleParticle), m_hMaterial, GetAbsOrigin()+offset );

		if ( sParticle == nullptr )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 2.0f;

		sParticle->m_flRoll			= 0;
		sParticle->m_flRollDelta	= 0;

		sParticle->m_uchColor[0]	= 225;
		sParticle->m_uchColor[1]	= 140;
		sParticle->m_uchColor[2]	= 64;
		sParticle->m_uchStartAlpha	= Helper_RandomInt( 128, 255 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= Helper_RandomInt( 1, 2 );
		sParticle->m_uchEndSize		= 1;

		sParticle->m_vecVelocity	= dir * Helper_RandomFloat( 128.0f, 256.0f );
	}

	//Add smokey bits
	offset.Random( -(m_flSpawnRadius * 0.5), (m_flSpawnRadius * 0.5) );
	sParticle = (SimpleParticle *) m_pSporeEffect->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], GetAbsOrigin()+offset );

	if ( sParticle == nullptr )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 1.0f;

	sParticle->m_flRoll			= Helper_RandomFloat( 0, 360 );
	sParticle->m_flRollDelta	= Helper_RandomFloat( -2.0f, 2.0f );

	sParticle->m_uchColor[0]	= 225;
	sParticle->m_uchColor[1]	= 140;
	sParticle->m_uchColor[2]	= 64;
	sParticle->m_uchStartAlpha	= Helper_RandomInt( 32, 64 );
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= m_flStartSize;
	sParticle->m_uchEndSize		= m_flEndSize;

	sParticle->m_vecVelocity	= dir * Helper_RandomFloat( 64.0f, 128.0f );
}

ConVar cl_sporeclipdistance( side_registry(), "cl_sporeclipdistance", "512", FCVAR_CHEAT | FCVAR_CLIENTDLL );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CSporeExplosion::Update( float fTimeDelta )
{
	if( m_bEmit )
	{
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		float tempDelta = fTimeDelta;

		float flDist = (MainViewOrigin(nSlot) - GetAbsOrigin()).Length();

		//Lower the spawnrate by half if we're far away from it.
		if ( cl_sporeclipdistance.GetFloat() <= flDist )
		{
			if ( m_flSpawnRate == m_flPreviousSpawnRate )
			{
				m_flPreviousSpawnRate = m_flSpawnRate * 0.5f;
				m_teParticleSpawn.ResetRate( m_flPreviousSpawnRate );
			}
		}
		else
		{
			if ( m_flSpawnRate != m_flPreviousSpawnRate )
			{
				m_flPreviousSpawnRate = m_flSpawnRate;
				m_teParticleSpawn.ResetRate( m_flPreviousSpawnRate );
			}
		}

		while ( m_teParticleSpawn.NextEvent( tempDelta ) )
		{
			AddParticles();
		}
	}
}

void CSporeExplosion::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	StandardParticle_t *pParticle = (StandardParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		pParticle->m_Lifetime += pIterator->GetTimeDelta();
		
		if( pParticle->m_Lifetime > m_flParticleLifetime )
		{
			pIterator->RemoveParticle( pParticle );
		}

		pParticle = (StandardParticle_t*)pIterator->GetNext();
	}
}

void CSporeExplosion::RenderParticles( CParticleRenderIterator *pIterator )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RPGShotDownCallback( const CEffectData &data )
{
	CLocalPlayerFilter filter;
	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Missile.ShotDown", &data.m_vOrigin );

	if ( CExplosionOverlay *pOverlay = new CExplosionOverlay )
	{
		pOverlay->m_flLifetime	= 0;
		pOverlay->m_vPos		= data.m_vOrigin;
		pOverlay->m_nSprites	= 1;
		
		pOverlay->m_vBaseColors[0].Init( 1.0f, 0.9f, 0.7f );

		pOverlay->m_Sprites[0].m_flHorzSize = 0.01f;
		pOverlay->m_Sprites[0].m_flVertSize = pOverlay->m_Sprites[0].m_flHorzSize*0.5f;

		pOverlay->Activate();
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( RPGShotDown, RPGShotDownCallback )
	PRECACHE( GAMESOUND, "Missile.ShotDown" )
DECLARE_CLIENT_EFFECT_END()

//==================================================
// CSporeTrail
//==================================================

class CSporeTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SporeTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSporeTrail, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
	CSporeTrail( void );
	virtual	~CSporeTrail( void );

public:
	void	SetEmit( bool bEmit );

// CBaseEntity
public:
	virtual	void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

// IPrototypeAppEffect
public:
	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );

// IParticleEffect
public:
	virtual void	Update( float fTimeDelta );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	StartRender( VMatrix &effectMatrix );

public:
	[[= ks::reflect::Net{} ]] Vector	m_vecEndColor;

	[[= ks::reflect::Net{} ]] float	m_flSpawnRate;
	[[= ks::reflect::Net{} ]] float	m_flParticleLifetime;
	[[= ks::reflect::Net{} ]] float	m_flStartSize;
	[[= ks::reflect::Net{} ]] float	m_flEndSize;
	[[= ks::reflect::Net{} ]] float	m_flSpawnRadius;

	Vector	m_vecVelocityOffset;

	[[= ks::reflect::Net{} ]] bool	m_bEmit;

private:
	CSporeTrail( const CSporeTrail & );

	void			AddParticles( void );

	PMaterialHandle		m_hMaterial;
	TimedEvent			m_teParticleSpawn;
	//ks::intrusive_ptr<SporeSmokeEffect> m_pSmokeEffect;

	Vector			m_vecPos;
	Vector			m_vecLastPos;	// This is stored so we can spawn particles in between the previous and new position
									// to eliminate holes in the trail.

	VMatrix			m_mAttachmentMatrix;
	CParticleMgr		*m_pParticleMgr;
};

//==================================================
// CSporeTrail
//==================================================

IMPLEMENT_REFLECT_CLIENTCLASS( CSporeTrail, DT_SporeTrail, SporeTrail )

CSporeTrail::CSporeTrail( void )
{
	m_pParticleMgr			= nullptr;
	//m_pSmokeEffect			= SporeSmokeEffect::Create( "CSporeTrail" );

	m_flSpawnRate			= 10;
	m_flParticleLifetime	= 5;
	m_flStartSize			= 35;
	m_flEndSize				= 55;
	m_flSpawnRadius			= 2;

	m_teParticleSpawn.Init( 5 );
	m_vecEndColor.Init();
	m_vecPos.Init();
	m_vecLastPos.Init();
	m_vecVelocityOffset.Init();

	m_bEmit = true;
}

CSporeTrail::~CSporeTrail()
{
	if( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEmit - 
//-----------------------------------------------------------------------------
void CSporeTrail::SetEmit( bool bEmit )
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CSporeTrail::OnDataChanged( DataUpdateType_t updateType )
{
	CBaseEntity::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CSporeTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if( pParticleMgr->AddEffect( &m_ParticleEffect, this ) == false )
		return;

	m_hMaterial	= g_Mat_DustPuff[1];
	m_pParticleMgr = pParticleMgr;
	m_teParticleSpawn.Init( 64 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSporeTrail::AddParticles( void )
{
	Vector	offset = RandomVector( -4.0f, 4.0f );

	//Make a new particle
	SimpleParticle *sParticle = (SimpleParticle *) m_ParticleEffect.AddParticle( sizeof(SimpleParticle), m_hMaterial );//m_pSmokeEffect->AddParticle( sizeof(SimpleParticle), m_hMaterial, GetAbsOrigin()+offset );

	if ( sParticle == nullptr )
		return;

	sParticle->m_Pos			= offset;
	sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= Helper_RandomFloat( -2.0f, 2.0f );

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 0.5f;

	sParticle->m_uchColor[0]	= 225;
	sParticle->m_uchColor[1]	= 140;
	sParticle->m_uchColor[2]	= 64;
	sParticle->m_uchStartAlpha	= Helper_RandomInt( 64, 128 );
	sParticle->m_uchEndAlpha	= 0;

	sParticle->m_uchStartSize	= 1.0f;
	sParticle->m_uchEndSize		= 1.0f;
	
	sParticle->m_vecVelocity	= RandomVector( -8.0f, 8.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CSporeTrail::Update( float fTimeDelta )
{
	if ( m_pParticleMgr == nullptr )
		return;

	//Add new particles
	if ( m_bEmit )
	{
		float tempDelta = fTimeDelta;
		
		while ( m_teParticleSpawn.NextEvent( tempDelta ) )
		{
			AddParticles();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &effectMatrix - 
//-----------------------------------------------------------------------------
void CSporeTrail::StartRender( VMatrix &effectMatrix )
{
	effectMatrix = effectMatrix * m_mAttachmentMatrix;
}

void CSporeTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
	if ( m_bEmit == false )
		return;

	const SimpleParticle *pParticle = (const SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Render
		Vector	tPos;
		TransformParticle( m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = tPos.z;

		Vector	color = Vector( 1.0f, 1.0f, 1.0f );

		//Render it
		RenderParticle_ColorSize(
			pIterator->GetParticleDraw(),
			tPos,
			color,
			1.0f,
			4 );

		pParticle = (const SimpleParticle*)pIterator->GetNext( sortKey );
	}
}

void CSporeTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	if ( m_bEmit == false )
		return;

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * pIterator->GetTimeDelta();

		//Should this particle die?
		pParticle->m_flLifetime += pIterator->GetTimeDelta();

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
		{
			pIterator->RemoveParticle( pParticle );
		}

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSporeTrail::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pAttachedTo->GetBaseEntity() );
	
	pEnt->GetAttachment( 1, *pAbsOrigin, *pAbsAngles );

	matrix3x4_t	matrix;

	AngleMatrix( *pAbsAngles, *pAbsOrigin, matrix );

	m_mAttachmentMatrix.Init( matrix );
}

//==================================================
// FireTrailhou
//==================================================

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_REFLECT_CLIENTCLASS( CFireTrail, DT_FireTrail, CFireTrail )

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
CFireTrail::CFireTrail()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFireTrail::~CFireTrail( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CFireTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	BaseClass::Start( pParticleMgr, pArgs );

	m_pTrailEmitter = CSimpleEmitter::Create( "FireTrail" );
	
	if ( !m_pTrailEmitter )
	{
		contract_assert( false );
		return;
	}

	m_pTrailEmitter->SetSortOrigin( GetAbsOrigin() );

	// Setup our materials
	m_hMaterial[FTRAIL_SMOKE1] = g_Mat_DustPuff[0];
	m_hMaterial[FTRAIL_SMOKE2] = g_Mat_DustPuff[1];
	
	m_hMaterial[FTRAIL_FLAME1] = m_pTrailEmitter->GetPMaterial( "sprites/flamelet1" );
	m_hMaterial[FTRAIL_FLAME2] = m_pTrailEmitter->GetPMaterial( "sprites/flamelet2" );
	m_hMaterial[FTRAIL_FLAME3] = m_pTrailEmitter->GetPMaterial( "sprites/flamelet3" );
	m_hMaterial[FTRAIL_FLAME4] = m_pTrailEmitter->GetPMaterial( "sprites/flamelet4" );
	m_hMaterial[FTRAIL_FLAME5] = m_pTrailEmitter->GetPMaterial( "sprites/flamelet5" );

	// Setup our smoke emitter
	m_pSmokeEmitter = CSmokeParticle::Create( "FireTrail_Smoke" );
	
	m_pSmokeEmitter->SetSortOrigin( GetAbsOrigin() );
	m_pSmokeEmitter->SetNearClip( 64.0f, 128.0f );

	if ( !m_pSmokeEmitter )
	{
		contract_assert( false );
		return;
	}

	// Seed our first position as the last known one
	m_vecLastPosition = GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CFireTrail::Update( float fTimeDelta )
{
	if ( !m_pTrailEmitter )
		return;

	if ( ( m_flLifetime != 0 ) && ( m_flLifetime <= gpGlobals->curtime ) )
		return;

	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FireTrail" );
	pSimple->SetSortOrigin( GetAbsOrigin() );
	
	Vector			offset;

static constexpr auto STARTSIZE = 8;
static constexpr auto ENDSIZE = 16;
static constexpr auto PARTICLE_LIFETIME = 0.075f;
static constexpr auto MIN_SPEED = 32;
static constexpr auto MAX_SPEED = 64;

	// Add new particles
	//if ( ShouldEmit() )
	{
		Vector	moveDiff	= GetAbsOrigin() - m_vecLastPosition;
		float	moveLength	= VectorNormalize( moveDiff );

		int	numPuffs = moveLength / ( STARTSIZE / 2.0f );

		//FIXME: More rational cap here, perhaps
		numPuffs = clamp( numPuffs, 1, 32 );

		SimpleParticle	*pParticle;
		Vector			offset;
		Vector			offsetColor;
		float			step = moveLength / numPuffs;

		//Fill in the gaps
		for ( int i = 1; i < numPuffs+1; i++ )
		{
			offset = m_vecLastPosition + ( moveDiff * step * i ) + RandomVector( -4.0f, 4.0f );

			//debugoverlay->AddBoxOverlay( offset, -Vector(2,2,2), Vector(2,2,2), vec3_angle, i*4, i*4, i*4, true, 1.0f );
			
			pParticle = (SimpleParticle *) m_pSmokeEmitter->AddParticle( sizeof( SimpleParticle ), m_hMaterial[random_valve->RandomInt( FTRAIL_FLAME1,FTRAIL_FLAME5 )], offset );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime		= 0.0f;
				pParticle->m_flDieTime		= /*PARTICLE_LIFETIME*/ 0.5f;// + random->RandomFloat(PARTICLE_LIFETIME*0.75f, PARTICLE_LIFETIME*1.25f);

				pParticle->m_vecVelocity.Random( 0.0f, 1.0f );
				pParticle->m_vecVelocity *= random_valve->RandomFloat( MIN_SPEED, MAX_SPEED );
				pParticle->m_vecVelocity[2] += 50;//random->RandomFloat( 32, 64 );
				
				pParticle->m_uchColor[0]	= 255.0f;
				pParticle->m_uchColor[1]	= 255.0f;
				pParticle->m_uchColor[2]	= 255.0f;
				
				pParticle->m_uchStartSize	= STARTSIZE * 2.0f;
				pParticle->m_uchEndSize		= STARTSIZE * 0.5f;
				
				pParticle->m_uchStartAlpha	= 255; 
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= 0.0f;//random->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -16.0f, 16.0f );
			}
		}

		//
		// Smoke
		//

		offset = RandomVector( -STARTSIZE*0.5f, STARTSIZE*0.5f ) + GetAbsOrigin();

		pParticle = (SimpleParticle *) m_pSmokeEmitter->AddParticle( sizeof( SimpleParticle ), m_hMaterial[random_valve->RandomInt( FTRAIL_SMOKE1, FTRAIL_SMOKE2 )], offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= ( PARTICLE_LIFETIME * 10.0f ) + random_valve->RandomFloat(PARTICLE_LIFETIME*0.75f, PARTICLE_LIFETIME*1.25f);

			pParticle->m_vecVelocity.Random( 0.0f, 1.0f );
			pParticle->m_vecVelocity *= random_valve->RandomFloat( MIN_SPEED, MAX_SPEED );
			pParticle->m_vecVelocity[2] += random_valve->RandomFloat( 50, 100 );
			
			pParticle->m_uchColor[0]	= 255.0f * 0.5f;
			pParticle->m_uchColor[1]	= 245.0f * 0.5f;
			pParticle->m_uchColor[2]	= 205.0f * 0.5f;
			
			pParticle->m_uchStartSize	= 16 * random_valve->RandomFloat( 0.75f, 1.25f );
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2.5f;
			
			pParticle->m_uchStartAlpha	= 64; 
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -16.0f, 16.0f );
		}
	}

	// Save off this position
	m_vecLastPosition = GetAbsOrigin();
}

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_REFLECT_CLIENTCLASS( CDustTrail, DT_DustTrail, DustTrail )

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
CDustTrail::CDustTrail()
{
	for (int i = 0; i < DUSTTRAIL_MATERIALS; i++)
	{
        m_MaterialHandle[i] = nullptr;
	}

	m_SpawnRate = 10;
	m_ParticleSpawn.Init(10);
	m_Color.Init(0.5, 0.5, 0.5);
	m_ParticleLifetime = 5;
	m_StartEmitTime = gpGlobals->curtime;
	m_StopEmitTime = 0;	// No end time
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_VelocityOffset.Init();
	m_Opacity = 0.5f;

	m_bEmit = true;

	m_pDustEmitter = nullptr;
	m_pParticleMgr	= nullptr;
}

CDustTrail::~CDustTrail()
{

	if ( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEmit - 
//-----------------------------------------------------------------------------
void CDustTrail::SetEmit(bool bEmit)
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rate - 
//-----------------------------------------------------------------------------
void CDustTrail::SetSpawnRate(float rate)
{
	m_SpawnRate = rate;
	m_ParticleSpawn.Init(rate);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CDustTrail::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), nullptr );
	}
}

// FIXME: These all have to be moved out of this old system and into the new to leverage art assets!
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectDusttrail )
PRECACHE( MATERIAL, "particle/smokesprites_0001" )
/*
PRECACHE( MATERIAL, "particle/smokesprites_0002" )
PRECACHE( MATERIAL, "particle/smokesprites_0003" )
PRECACHE( MATERIAL, "particle/smokesprites_0004" )
PRECACHE( MATERIAL, "particle/smokesprites_0005" )
PRECACHE( MATERIAL, "particle/smokesprites_0006" )
PRECACHE( MATERIAL, "particle/smokesprites_0007" )
PRECACHE( MATERIAL, "particle/smokesprites_0008" )
PRECACHE( MATERIAL, "particle/smokesprites_0009" )
PRECACHE( MATERIAL, "particle/smokesprites_0010" )
PRECACHE( MATERIAL, "particle/smokesprites_0011" )
PRECACHE( MATERIAL, "particle/smokesprites_0012" )
PRECACHE( MATERIAL, "particle/smokesprites_0013" )
PRECACHE( MATERIAL, "particle/smokesprites_0014" )
PRECACHE( MATERIAL, "particle/smokesprites_0015" )
PRECACHE( MATERIAL, "particle/smokesprites_0016" )
*/
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CDustTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;

	m_pParticleMgr	= pParticleMgr;
	m_pDustEmitter = CDustFollower::Create("DustTrail");
	
	if ( !m_pDustEmitter )
	{
		contract_assert( false );
		return;
	}

	m_pDustEmitter->SetSortOrigin( GetAbsOrigin() );
	m_pDustEmitter->SetNearClip( 64.0f, 128.0f );

	for (int i = 0; i < DUSTTRAIL_MATERIALS; i++)
	{
		//char name[256];
		//V_snprintf( name, sizeof( name ), "particle/smokesprites_%04d", i + 1 );
		m_MaterialHandle[i] = m_pDustEmitter->GetPMaterial( "particle/smokesprites_0001" );
	}
	
	m_ParticleSpawn.Init( m_SpawnRate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CDustTrail::Update( float fTimeDelta )
{
	if ( !m_pDustEmitter )
		return;

	Vector	offsetColor;

	// Add new particles
	if ( !m_bEmit )
		return;

	if ( ( m_StopEmitTime != 0 ) && ( m_StopEmitTime <= gpGlobals->curtime ) )
		return;

	float tempDelta = fTimeDelta;

	SimpleParticle	*pParticle;
	Vector			offset;

	Vector vecOrigin;
	VectorMA( GetAbsOrigin(), -fTimeDelta, GetAbsVelocity(), vecOrigin );

	Vector vecForward;
	GetVectors( &vecForward, nullptr, nullptr );

	while( m_ParticleSpawn.NextEvent( tempDelta ) )
	{
		float fldt = fTimeDelta - tempDelta;

		offset.Random( -m_SpawnRadius, m_SpawnRadius );
		offset += vecOrigin;
		VectorMA( offset, fldt, GetAbsVelocity(), offset );

		//if ( random->RandomFloat( 0.f, 5.0f ) > GetAbsVelocity().Length())
		//	continue;

		pParticle = (SimpleParticle *) m_pDustEmitter->AddParticle( sizeof( SimpleParticle ), m_MaterialHandle[random_valve->RandomInt(0,0)], offset ); // FIXME: the other sprites look bad

		if ( pParticle == nullptr )
			continue;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= m_ParticleLifetime;

		pParticle->m_vecVelocity.Random( -1.0f, 1.0f );
		pParticle->m_vecVelocity *= random_valve->RandomFloat( m_MinSpeed, m_MaxSpeed );

		pParticle->m_vecVelocity = pParticle->m_vecVelocity + GetAbsVelocity();
		
		float flDirectedVel = random_valve->RandomFloat( m_MinDirectedSpeed, m_MaxDirectedSpeed );
		VectorMA( pParticle->m_vecVelocity, flDirectedVel, vecForward, pParticle->m_vecVelocity );

		offsetColor = m_Color;
		float flMaxVal = std::max( m_Color[0], m_Color[1] );
		if ( flMaxVal < m_Color[2] )
		{
			flMaxVal = m_Color[2];
		}
		offsetColor /= flMaxVal;

		offsetColor *= random_valve->RandomFloat( -0.2f, 0.2f );
		offsetColor += m_Color;

		offsetColor[0] = clamp( offsetColor[0], 0.0f, 1.0f );
		offsetColor[1] = clamp( offsetColor[1], 0.0f, 1.0f );
		offsetColor[2] = clamp( offsetColor[2], 0.0f, 1.0f );

		pParticle->m_uchColor[0]	= offsetColor[0]*255.0f;
		pParticle->m_uchColor[1]	= offsetColor[1]*255.0f;
		pParticle->m_uchColor[2]	= offsetColor[2]*255.0f;
		
		pParticle->m_uchStartSize	= m_StartSize;
		pParticle->m_uchEndSize		= m_EndSize;
		
		float alpha = random_valve->RandomFloat( m_Opacity*0.75f, m_Opacity*1.25f );
		alpha = clamp( alpha, 0.0f, 1.0f );

		if ( m_StopEmitTime != 0 && m_StopEmitTime > m_StartEmitTime )
		{
			alpha *= std::sqrt( (m_StopEmitTime - gpGlobals->curtime) /(m_StopEmitTime - m_StartEmitTime) );
		}

		pParticle->m_uchStartAlpha	= alpha * 255; 
		pParticle->m_uchEndAlpha	= 0;
			
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1.0f, 1.0f );
    }
}

void CDustTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
}

void CDustTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
}

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
struct Particle;

using namespace ::ks::reflect;

// Purpose: Implements a particle system steam jet.

	constexpr auto SMOKESTACK_MAX_MATERIALS = 1;

//==================================================
// CSmokeStack
//==================================================

class CSmokeStack : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeStack" } ]]
	[[= ks::reflect::From<"m_AmbientLight.m_vPos", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_AmbientLight.m_vColor", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_AmbientLight.m_flIntensity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_DirLight.m_vPos", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_DirLight.m_vColor", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_DirLight.m_flIntensity", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CSmokeStack, CBaseParticleEntity );

	CSmokeStack();
	~CSmokeStack();

	class SmokeStackParticle : public Particle
	{
	public:
		Vector		m_Velocity;
		Vector		m_vAccel;
		float		m_Lifetime;
		float		m_flAngle;
		float		m_flRollDelta;
		float		m_flSortPos;
	};

//CBaseEntity
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink();

//IPrototypeAppEffect
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);
	virtual bool	GetPropEditInfo(RecvTable **ppTable, void **ppObj);

//IParticleEffect
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	StartRender( VMatrix &effectMatrix );

private:

	void QueueLightParametersInRenderer();

//Stuff from the datatable
public:

	CParticleSphereRenderer	m_Renderer;

	[[= ks::reflect::Net{} ]] float			m_SpreadSpeed;
	[[= ks::reflect::Net{} ]] float			m_Speed;
	[[= ks::reflect::Net{} ]] float			m_StartSize;
	[[= ks::reflect::Net{} ]] float			m_EndSize;
	[[= ks::reflect::Net{} ]] float			m_Rate;
	[[= ks::reflect::Net{} ]] float			m_JetLength;	// Length of the jet. Lifetime is derived from this.

	[[= ks::reflect::Net{} ]] int				m_bEmit;		// Emit particles?
	[[= ks::reflect::Net{} ]] float			m_flBaseSpread;

	class CLightInfo
	{
	public:
		Vector		m_vPos;
		Vector		m_vColor;
		float		m_flIntensity;
	};

	// Note: there are two ways the directional light can be specified. The default is to use 
	// DirLightColor and a default dirlight source (from above or below).
	//		In this case, m_DirLight.m_vPos and m_DirLight.m_flIntensity are ignored.
	//
	// The other is to attach a directional env_particlelight to us.
	//		In this case, m_DirLightSource is ignored and all the m_DirLight parameters are used.
	CParticleLightInfo	m_AmbientLight;
	CParticleLightInfo	m_DirLight;

	Vector			m_vBaseColor;
	
	[[= ks::reflect::Net{} ]] Vector			m_vWind;
	[[= ks::reflect::Net{} ]] float			m_flTwist;
	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_IntSubOne, ks::reflect::WireSide::Recv>{} ]] int				m_iMaterialModel;

private:
	CSmokeStack( const CSmokeStack & );

	float			m_TwistMat[2][2];
	int				m_bTwist;

	float			m_flAlphaScale;
	float			m_InvLifetime;		// Calculated from m_JetLength / m_Speed;

	CParticleMgr		*m_pParticleMgr;
	PMaterialHandle	m_MaterialHandle[SMOKESTACK_MAX_MATERIALS];
	TimedEvent		m_ParticleSpawn;
	int				m_iMaxFrames;
	bool			m_bInView;
	[[= ks::reflect::Net{} ]] float			m_flRollSpeed;
};

// ------------------------------------------------------------------------- //
// Tables.
// ------------------------------------------------------------------------- //

// Expose to the particle app.

IMPLEMENT_REFLECT_CLIENTCLASS( CSmokeStack, DT_SmokeStack, CSmokeStack )

// ------------------------------------------------------------------------- //
// CSmokeStack implementation.
// ------------------------------------------------------------------------- //
CSmokeStack::CSmokeStack()
{
	m_pParticleMgr = nullptr;
	m_MaterialHandle[0] = INVALID_MATERIAL_HANDLE;
	m_iMaterialModel = -1;
	
	m_SpreadSpeed = 15;
	m_Speed = 30;
	m_StartSize = 10;
	m_EndSize = 15;
	m_Rate = 80;
	m_JetLength = 180;
	m_bEmit = true;

	m_flBaseSpread = 20;
	m_bInView = false;

	// Lighting is (base color) + (ambient / dist^2) + bump(directional / dist^2)
	// By default, we use bottom-up lighting for the directional.
	SetRenderColor( 0, 0, 0 );
	SetRenderAlpha( 255 );

	m_AmbientLight.m_vPos.Init(0,0,-100);
	m_AmbientLight.m_vColor.Init( 40, 40, 40 );
	m_AmbientLight.m_flIntensity = 8000;

	m_DirLight.m_vColor.Init( 255, 128, 0 );
	
	m_vWind.Init();

	m_flTwist = 0;
}

CSmokeStack::~CSmokeStack()
{
	if(m_pParticleMgr)
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
}

//-----------------------------------------------------------------------------
// Purpose: Called after a data update has occured
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CSmokeStack::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		Start(ParticleMgr(), nullptr);
	}

	// Recalulate lifetime in case length or speed changed.
	m_InvLifetime = m_Speed / m_JetLength;
}

//-----------------------------------------------------------------------------
// Purpose: Starts the effect
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CSmokeStack::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	pParticleMgr->AddEffect( &m_ParticleEffect, this );
	
	// Figure out the material name.
	char str[512] = "unset_material";
	const model_t *pModel = modelinfo->GetModel( m_iMaterialModel );
	if ( pModel )
	{
		V_strncpy( str, modelinfo->GetModelName( pModel ), sizeof( str ) );

		// Get rid of the extension because the material system doesn't want it.
		char *pExt = V_stristr( str, ".vmt" );
		if ( pExt )
			pExt[0] = 0;
	}

	m_MaterialHandle[0] = m_ParticleEffect.FindOrAddMaterial( str );

	m_ParticleSpawn.Init(m_Rate);

	m_InvLifetime = m_Speed / m_JetLength;

	m_pParticleMgr = pParticleMgr;

	// Figure out how we need to draw.
	IMaterial *pMaterial = pParticleMgr->PMaterialToIMaterial( m_MaterialHandle[0] );
	if( pMaterial )
	{
		m_Renderer.Init( pParticleMgr, pMaterial );
	}
	
	QueueLightParametersInRenderer();

	// For the first N seconds, always simulate so it can build up the smokestack.
	// Afterwards, we set it to freeze when it's not being rendered.
	m_ParticleEffect.SetAlwaysSimulate( true );
	SetNextClientThink( gpGlobals->curtime + 5 );
}

void CSmokeStack::ClientThink()
{
	m_ParticleEffect.SetAlwaysSimulate( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppTable - 
//			**ppObj - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSmokeStack::GetPropEditInfo( RecvTable **ppTable, void **ppObj )
{
	*ppTable = &REFERENCE_RECV_TABLE(DT_SmokeStack);
	*ppObj = this;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void CSmokeStack::Update(float fTimeDelta)
{
	if( !m_pParticleMgr )
	{
		contract_assert( false );
		return;
	}

	// Don't spawn particles unless we're visible.
	if( m_bEmit && (m_ParticleEffect.WasDrawnPrevFrame() || m_ParticleEffect.GetAlwaysSimulate()) )
	{
		// Add new particles.																	
		Vector forward, right, up;
		AngleVectors(GetAbsAngles(), &forward, &right, &up);			

		float tempDelta = fTimeDelta;
		while(m_ParticleSpawn.NextEvent(tempDelta))
		{
			int iRandomFrame = random_valve->RandomInt( 0, m_iMaxFrames );

			iRandomFrame = 0;
	
			// Make a new particle.
			if(SmokeStackParticle *pParticle = (SmokeStackParticle*)m_ParticleEffect.AddParticle(sizeof(SmokeStackParticle), m_MaterialHandle[iRandomFrame]))
			{
				float angle = FRand( 0, 2.0f*M_PI_F );
				
				pParticle->m_Pos = GetAbsOrigin() +
					right * (std::cos( angle ) * m_flBaseSpread) +
					forward * (std::sin( angle ) * m_flBaseSpread);

				pParticle->m_Velocity = 
					FRand(-m_SpreadSpeed,m_SpreadSpeed) * right +
					FRand(-m_SpreadSpeed,m_SpreadSpeed) * forward +
					m_Speed * up;

				pParticle->m_vAccel = m_vWind;
				pParticle->m_Lifetime = 0;
				pParticle->m_flAngle = 0.0f;

				pParticle->m_flRollDelta = random_valve->RandomFloat( -m_flRollSpeed, m_flRollSpeed );
				pParticle->m_flSortPos = pParticle->m_Pos.z;
			}
		}
	}

	// Setup the twist matrix.
	float flTwist = (m_flTwist * (M_PI_F * 2.f) / 360.0f) * Helper_GetFrameTime();
	if( m_bTwist = !!flTwist )
	{
		m_TwistMat[0][0] =  std::cos(flTwist);
		m_TwistMat[0][1] =  std::sin(flTwist);
		m_TwistMat[1][0] = -sin(flTwist);
		m_TwistMat[1][1] =  cos(flTwist);
	}

	QueueLightParametersInRenderer();
}

void CSmokeStack::StartRender( VMatrix &effectMatrix )
{
	m_Renderer.StartRender( effectMatrix );
}

void CSmokeStack::QueueLightParametersInRenderer()
{
	m_Renderer.SetBaseColor( Vector( GetRenderColorR() / 255.0f, GetRenderColorG() / 255.0f, GetRenderColorB() / 255.0f ) );
	m_Renderer.SetAmbientLight( m_AmbientLight );
	m_Renderer.SetDirectionalLight( m_DirLight );
	m_flAlphaScale = (float)GetRenderAlpha();
} 

void CSmokeStack::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SmokeStackParticle *pParticle = (const SmokeStackParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Transform.						   
		Vector tPos;
		TransformParticle( m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos );

		// Figure out its alpha. Squaring it after it gets halfway through its lifetime
		// makes it get translucent and fade out for a longer time.
		//float alpha = cosf( -M_PI_F + tLifetime * M_PI_F * 2.f ) * 0.5f + 0.5f;
		float tLifetime = pParticle->m_Lifetime * m_InvLifetime;
		float alpha = TableCos( -M_PI_F + tLifetime * M_PI_F * 2.f ) * 0.5f + 0.5f;
		if( tLifetime > 0.5f )
			alpha *= alpha;

		m_Renderer.RenderParticle(
			pIterator->GetParticleDraw(),
			pParticle->m_Pos,
			tPos,
			alpha * m_flAlphaScale,
			FLerp(m_StartSize, m_EndSize, tLifetime),
			DEG2RAD( pParticle->m_flAngle )
		);
		
		pParticle = (const SmokeStackParticle*)pIterator->GetNext( pParticle->m_flSortPos );
	}
}

void CSmokeStack::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	bool bSortNow = true; // Change this to false if we see sorting issues.
	bool bQuickTest = false;

	bool bDrawn = m_ParticleEffect.WasDrawnPrevFrame() ? true : false;

	if ( bDrawn == true && m_bInView == false )
	{
		bSortNow = true;
	}

	if ( bDrawn == false && m_bInView == true )
	{
		bQuickTest = true;
	}

	bQuickTest = false;
	bSortNow = true;

	if( bQuickTest == false && m_bEmit && (!m_ParticleEffect.WasDrawnPrevFrame() && !m_ParticleEffect.GetAlwaysSimulate()) )
		return;

	SmokeStackParticle *pParticle = (SmokeStackParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Should this particle die?
		pParticle->m_Lifetime += pIterator->GetTimeDelta();

		float tLifetime = pParticle->m_Lifetime * m_InvLifetime;
		if( tLifetime >= 1 )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			// Transform.						   
			Vector tPos;
			if( m_bTwist )
			{
				Vector vTwist(
					pParticle->m_Pos.x - GetAbsOrigin().x,
					pParticle->m_Pos.y - GetAbsOrigin().y,
					0);

				pParticle->m_Pos.x = vTwist.x * m_TwistMat[0][0] + vTwist.y * m_TwistMat[0][1] + GetAbsOrigin().x;
				pParticle->m_Pos.y = vTwist.x * m_TwistMat[1][0] + vTwist.y * m_TwistMat[1][1] + GetAbsOrigin().y;
			}

			pParticle->m_Pos = pParticle->m_Pos + 
				pParticle->m_Velocity * pIterator->GetTimeDelta() + 
				pParticle->m_vAccel * (0.5f * pIterator->GetTimeDelta() * pIterator->GetTimeDelta());

			pParticle->m_Velocity += pParticle->m_vAccel * pIterator->GetTimeDelta();

			pParticle->m_flAngle += pParticle->m_flRollDelta * pIterator->GetTimeDelta();

			if ( bSortNow == true )
			{
				Vector tPos;
				TransformParticle( m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos );
				pParticle->m_flSortPos = tPos.z;
			}
		}

		pParticle = (SmokeStackParticle*)pIterator->GetNext();
	}

	m_bInView = bDrawn;
}

class CBaseEntity;
class CBasePlayer;
class CRecipientFilter;
class IGameSystem;

using namespace ::ks::reflect;

static void Register_LOG_SND_EMITTERSYSTEM_Tags();
LoggingChannelID_t LOG_SND_EMITTERSYSTEM = LoggingSystem_RegisterLoggingChannel( "SndEmitterSystem", Register_LOG_SND_EMITTERSYSTEM_Tags, LCF_CONSOLE_ONLY, LS_MESSAGE );
void Register_LOG_SND_EMITTERSYSTEM_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "SndEmitterSystem" );

};

ConVar sv_soundemitter_version( side_registry(), "sv_soundemitter_version", "2", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "specfies what version of soundemitter system to use\n" );

ConVar snd_prevent_ss_duplicates( side_registry(), "snd_prevent_ss_duplicates", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "switch to en/disable the prevention of splitscreen audio file duplicates\n" );

ConVar snd_sos_show_client_xmit( side_registry(), "snd_sos_show_client_xmit", "0", FCVAR_CHEAT );

ConVar sv_soundemitter_trace( side_registry(), "sv_soundemitter_trace", "-1", FCVAR_REPLICATED, "Show all EmitSound calls including their symbolic name and the actual wave file they resolved to. (-1 = for nobody, 0 = for everybody, n = for one entity)\n" );
ConVar cc_showmissing( side_registry(), "cc_showmissing", "0", FCVAR_REPLICATED, "Show missing closecaption entries." );

extern ISoundEmitterSystemBase *soundemitterbase;
static ConVar *g_pClosecaption = nullptr;

static bool g_bPermitDirectSoundPrecache = false;

void WaveTrace( char const *wavname, char const *funcname )
{

	static ks::intern_pool s_WaveTrace;

	// Make sure we only show the message once
	if ( !s_WaveTrace.contains( wavname ) )
	{
		DevMsg( "%s directly referenced wave %s (should use game_sounds.txt system instead)\n", 
			funcname, wavname );
		s_WaveTrace.intern( wavname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &src - 
//-----------------------------------------------------------------------------
EmitSound_t::EmitSound_t( const CSoundParameters &src )
{
	m_nChannel = src.channel;
	m_pSoundName = src.soundname;
	m_flVolume = src.volume;
	m_SoundLevel = src.soundlevel;
	m_nFlags = 0;
	m_nPitch = src.pitch;
	m_pOrigin = 0;
	m_flSoundTime = ( src.delay_msec == 0 ) ? 0.0f : gpGlobals->curtime + ( (float)src.delay_msec / 1000.0f );
	m_pflSoundDuration = 0;
	m_bEmitCloseCaption = true;
	m_bWarnOnMissingCloseCaption = false;
	m_bWarnOnDirectWaveReference = false;
	m_nSpeakerEntity = -1;
	// if sound is tagged as version 2 or higher this will be treated as a soundentry!
	m_hSoundScriptHash = src.m_hSoundScriptHash;
	m_nSoundEntryVersion = src.m_nSoundEntryVersion;
}

void Hack_FixEscapeChars( char *str )
{
	ks::ScratchArena<> scratch;
	int len = V_strlen( str ) + 1;
	char *i = str;
	char *o = (char *)scratch.AllocBytes( len );
	char *osave = o;
	while ( *i )
	{
		if ( *i == '\\' )
		{
			switch (  *( i + 1 ) )
			{
			case 'n':
				*o = '\n';
				++i;
				break;
			default:
				*o = *i;
				break;
			}
		}
		else
		{
			*o = *i;
		}

		++i;
		++o;
	}
	*o = 0;
	V_strncpy( str, osave, len );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSoundEmitterSystem : public CBaseGameSystem
{
public:
	virtual char const *Name() { return "CSoundEmitterSystem"; }

	CSoundEmitterSystem( char const *name )
	{
	}

	// IServerSystem stuff
	virtual bool Init()
	{
		contract_assert( soundemitterbase );
		g_pClosecaption = cvar->FindVar("closecaption");
		contract_assert( g_pClosecaption );

		return true;
	}

	virtual void Shutdown()
	{
		contract_assert( soundemitterbase );
	}

	void Flush()
	{
		Shutdown();
		soundemitterbase->Flush();
		Init();
	}

	virtual void TraceEmitSound( int originEnt, char const *fmt, ... )
	{
		if ( sv_soundemitter_trace.GetInt() == -1 )
			return;

		if ( sv_soundemitter_trace.GetInt() != 0 && sv_soundemitter_trace.GetInt() != originEnt )
			return;

		std::va_list	argptr;
		char string[256];
		__builtin_va_start (argptr, fmt);
		V_vsnprintf( string, sizeof( string ), fmt, argptr );
		__builtin_va_end (argptr);

		// Spew to console
		Msg( "%s %s", CBaseEntity::IsServer() ? "(sv)" : "(cl)", string );
	}

	// Precache all wave files referenced in wave or rndwave keys
	virtual void LevelInitPreEntity()
	{

	}

	void PreloadSounds( void )
	{
		for ( int i=soundemitterbase->First(); i != soundemitterbase->InvalidIndex(); i=soundemitterbase->Next( i ) )
		{
			CSoundParametersInternal *pParams = soundemitterbase->InternalGetParametersForSound( i );
			if ( pParams->ShouldPreload() )
			{
				InternalPrecacheWaves( i );
			}
			if ( pParams->ShouldAutoCache() )
			{
				PrecacheScriptSound( soundemitterbase->GetSoundName( i ) );
			}
		}

	}

	virtual void LevelInitPostEntity()
	{
	}

	virtual void LevelShutdownPostEntity()
	{

	}
		
	void InternalPrecacheWaves( int soundIndex )
	{
		CSoundParametersInternal *internal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( !internal )
			return;

		int waveCount = internal->NumSoundNames();
		if ( !waveCount )
		{
			DevMsg( "CSoundEmitterSystem:  sounds.txt entry '%s' has no waves listed under 'wave' or 'rndwave' key!!!\n",
				soundemitterbase->GetSoundName( soundIndex ) );
		}
		else
		{
			g_bPermitDirectSoundPrecache = true;

			for( int wave = 0; wave < waveCount; wave++ )
			{
				CBaseEntity::PrecacheSound( soundemitterbase->GetWaveName( internal->GetSoundNames()[ wave ].symbol ) );
			}

			g_bPermitDirectSoundPrecache = false;
		}
	}

	void InternalPrefetchWaves( int soundIndex )
	{
		CSoundParametersInternal *internal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( !internal )
			return;

		int waveCount = internal->NumSoundNames();
		if ( !waveCount )
		{
			DevMsg( "CSoundEmitterSystem:  sounds.txt entry '%s' has no waves listed under 'wave' or 'rndwave' key!!!\n",
				soundemitterbase->GetSoundName( soundIndex ) );
		}
		else
		{
			for( int wave = 0; wave < waveCount; wave++ )
			{
				CBaseEntity::PrefetchSound( soundemitterbase->GetWaveName( internal->GetSoundNames()[ wave ].symbol ) );
			}
		}
	}

	void PrecacheSOSScriptSounds( KeyValues *pRootKV )
	{
		if ( !pRootKV )
			return;

		// iterate through all values
		for ( KeyValues *pValue = pRootKV->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
		{
			const char *pName = pValue->GetName();
			if ( pName && !V_stricmp( pName, "entry_name" ) )
			{
				const char *pScriptName = pValue->GetString();
				if ( pScriptName && pScriptName[0] )
				{
					PrecacheScriptSound( pScriptName );
				}
			}
		}

		// iterate and recurse into each true subkey
		for ( KeyValues *pSubKey = pRootKV->GetFirstTrueSubKey(); pSubKey; pSubKey = pSubKey->GetNextTrueSubKey() )
		{
			PrecacheSOSScriptSounds( pSubKey );
		}
	}

	HSOUNDSCRIPTHASH PrecacheScriptSound( const char *soundname )
	{
		HSOUNDSCRIPTHASH hash = soundemitterbase->HashSoundName( soundname );
		int soundIndex = soundemitterbase->GetSoundIndexForHash( hash );
		if ( !soundemitterbase->IsValidIndex( soundIndex ) )
		{
			if ( V_stristr( soundname, ".wav" ) || V_strstr( soundname, ".mp3" ) )
			{
				g_bPermitDirectSoundPrecache = true;
	
				CBaseEntity::PrecacheSound( soundname );
	
				g_bPermitDirectSoundPrecache = false;
				return SOUNDEMITTER_INVALID_HASH;
			}

			return SOUNDEMITTER_INVALID_HASH;
		}

		// recursively descend into possible operator stacks to precache all their script sounds
		CSoundParametersInternal *pInternal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( pInternal && !pInternal->HasCached() )
		{
			pInternal->SetCached( true );
			PrecacheSOSScriptSounds( pInternal->GetOperatorsKV() );
		}

		InternalPrecacheWaves( soundIndex );
		return hash;	
	}

	void PrefetchScriptSound( const char *soundname )
	{
		int soundIndex = soundemitterbase->GetSoundIndex( soundname );
		if ( !soundemitterbase->IsValidIndex( soundIndex ) )
		{
			if ( V_stristr( soundname, ".wav" ) || V_strstr( soundname, ".mp3" ) )
			{
				CBaseEntity::PrefetchSound( soundname );
			}
			return;
		}

		InternalPrefetchWaves( soundIndex );
	}
public:

	// utility for cracking parameters
	bool GetSoundEntryParameters( int entindex,  const EmitSound_t & ep, CSoundParameters & params, HSOUNDSCRIPTHASH& handle )
	{
		// Try to deduce the actor's gender
		gender_t gender = GENDER_NONE;
		CBaseEntity *ent = CBaseEntity::Instance( entindex );
		if ( ent )
		{
			char const *actorModel = STRING( ent->GetModelName() );
			gender = soundemitterbase->GetActorGender( actorModel );
		}

		if ( !soundemitterbase->GetParametersForSoundEx( ep.m_pSoundName, handle, params, gender, true ) )
		{
			return false;
		}

		if ( !params.soundname[0] )
			return false;

		if ( !V_strncasecmp( params.soundname, "vo", 2 ) &&
			!( params.channel == CHAN_STREAM ||
			params.channel == CHAN_VOICE ) &&
			params.m_nSoundEntryVersion < 2 )
		{
			DevMsg( "EmitSound:  Voice wave file %s doesn't specify CHAN_VOICE or CHAN_STREAM for sound %s\n",
				params.soundname, ep.m_pSoundName );
		}

		// handle SND_CHANGEPITCH/SND_CHANGEVOL and other sound flags.etc.
		if( ( ep.m_nFlags & SND_CHANGE_PITCH ) || ( ep.m_nFlags & SND_OVERRIDE_PITCH ) )
		{
			params.pitch = ep.m_nPitch;
		}

		if( ep.m_nFlags & SND_CHANGE_VOL )
		{
			params.volume = ep.m_flVolume;
		}

		return true;
	}

	// spew utility
	void TraceEmitSoundEntry( int handle, const char *pSoundEntryName, CSoundParameters &pSoundParams, int nSeed )
	{

		if( !snd_sos_show_client_xmit.GetInt() )
		{
			return;
		}

		Log_Msg( LOG_SND_EMITTERSYSTEM, Color( 180, 256, 180, 255 ), "Client: Emitting SoundEntry: %i : %s : %s : operators: %s : seed: %i\n", handle, pSoundEntryName, pSoundParams.soundname, pSoundParams.m_pOperatorsKV ? "true" : "false", nSeed );

	}
	// spew utility
	void TraceEmitSoundEntry( HSOUNDSCRIPTHASH handle, const char *pSoundEntryName, const char *pSoundFileName )
	{

		if( !snd_sos_show_client_xmit.GetInt() )
		{
			return;
		}
		Log_Msg( LOG_SND_EMITTERSYSTEM, Color( 180, 256, 180, 255 ), "Client: Emitting SoundEntry: %i : %s : %s\n", handle, pSoundEntryName, pSoundFileName );

	}

	//
	// emitting via a "SoundEntry" as opposed to the actual "SoundFile"
	// ep.m_pSoundName = a "SoundEntry" string
	//
	int EmitSoundByHandle( IRecipientFilter& filter, int entindex, const EmitSound_t & ep, HSOUNDSCRIPTHASH& handle )
	{
		// Whether the important params have come from code, defaults or the
		// script, we emit using them to stay backwards compatible.
		// It is possible, however, to override these values via the script
		// entries.

		CSoundParameters params;
		if( !GetSoundEntryParameters( entindex, ep, params, handle ) )
		{
			return 0;
		}

		// NOTE: This is probably 

		// sound precaching? 
		// NOTE: do something about this, should be irrelevant here
		// because we'll select our actual sound on the other side

		// calculating start time from param.delay_msec
		// does this get moved or just replicated on client?
		float st = ep.m_flSoundTime;
		if ( !st && 
			params.delay_msec != 0 )
		{
			st = gpGlobals->curtime + (float)params.delay_msec / 1000.f;
		}

		// TERROR:
		double startTime = Plat_FloatTime();

		// are we actually treating as a "SoundEntry"?
		int nFlags = ep.m_nFlags;
		if(sv_soundemitter_version.GetInt() > 1 && params.m_nSoundEntryVersion > 1)
		{
			 nFlags |= SND_IS_SCRIPTHANDLE;
			
			 TraceEmitSoundEntry( handle, ep.m_pSoundName, params, params.m_nRandomSeed );

		}

		// Emit via server or client engine call

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );
		
		// use the cracked script params
		int guid = enginesound->EmitSound( 
			filterCopy, 
			entindex, 
			params.channel,
			ep.m_pSoundName,    // gamesound
			handle,				// gamesound handle
			params.soundname,   // soundfile
			params.volume,
			(soundlevel_t)params.soundlevel,
			params.m_nRandomSeed,
			nFlags,
			params.pitch,
			ep.m_pOrigin,
			nullptr,
			&ep.m_UtlVecSoundOrigin,
			true,
			st,
			ep.m_nSpeakerEntity );

		// handle duration query
		// NOTE: This needs to be addressed for soundentry emission
		//
		if ( ep.m_pflSoundDuration )
		{
			*ep.m_pflSoundDuration = enginesound->GetSoundDuration( params.soundname );
		}
		
		//// --------------------------------------------------------
		// MattC?
		// TERROR:
		float timeSpent = ( Plat_FloatTime() - startTime ) * 1000.0f;
		const float thinkLimit = 50.0f;
		if ( timeSpent > thinkLimit )
		{
			DevMsg( "EmitSoundByHandle(%s) took %f milliseconds (client)\n",
				ep.m_pSoundName, timeSpent );
		}

		// Debug spew
		TraceEmitSound( entindex, "EmitSound:  '%s' emitted as '%s' (ent %i)\n",
			ep.m_pSoundName, params.soundname, entindex );

		// Don't caption modulations to the sound
		if ( !( ep.m_nFlags & ( SND_CHANGE_PITCH | SND_CHANGE_VOL ) ) )
		{
			EmitCloseCaption( filter, entindex, params, ep );
		}
		return guid;
	}

	//---------------------------------------------------------------------
	// Emits sound via a direct sound file reference, 
	//---------------------------------------------------------------------
	int EmitSoundBySoundFile( IRecipientFilter& filter, int entindex, const EmitSound_t & ep )
	{

		// Emission by soundfile is typically because the calling code has 
		// already cracked the soundscript, loaded it's parameters and altered some.
		// However, we want to retain BOTH the calling code's parameters
		// AND the soundscript handle so that we have ALL the data for processing.
		//
		// if this has been updated to include soundscript handle, we can tell
		// by the soundentry version and a valid soundscript handle. We flag it
		// and add the data to transmission.
		//
		int nFlags = ep.m_nFlags;
		const char *pSoundEntryName = ep.m_pSoundName;
		if( ep.m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH &&
			ep.m_nSoundEntryVersion > 1 &&
			sv_soundemitter_version.GetInt() > 1 )
		{
			// reget original soundentry name
			pSoundEntryName = soundemitterbase->GetSoundName( ep.m_hSoundScriptHash );
			nFlags |= SND_IS_SCRIPTHANDLE;
			TraceEmitSoundEntry( ep.m_hSoundScriptHash, pSoundEntryName, ep.m_pSoundName );

		}

		// TERROR:
		double startTime = Plat_FloatTime();
		if ( ep.m_bWarnOnDirectWaveReference && 
			V_stristr( ep.m_pSoundName, ".wav" ) )
		{
			WaveTrace( ep.m_pSoundName, "Emitsound" );
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
		// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
		// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
		
		// Emit sound via direct soundfile reference, unless tagged as a soundentry
		int nGuid = enginesound->EmitSound( 
			filterCopy, 
			entindex, 
			ep.m_nChannel,
			pSoundEntryName,
			ep.m_hSoundScriptHash,
			ep.m_pSoundName, 
			ep.m_flVolume, 
			ep.m_SoundLevel, 
			0 ,
			nFlags, 
			ep.m_nPitch, 
			ep.m_pOrigin,
			nullptr, 
			&ep.m_UtlVecSoundOrigin,
			true, 
			ep.m_flSoundTime,
			ep.m_nSpeakerEntity );

		//// -------------------------------------------------------------------
		if ( ep.m_pflSoundDuration )
		{
			// TERROR:
			CVProfScope _( "CSoundEmitterSystem::EmitSound GetSoundDuration (calls engine)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			*ep.m_pflSoundDuration = enginesound->GetSoundDuration( ep.m_pSoundName );
		}

		TraceEmitSound( entindex, "%f EmitSound:  Raw wave emitted '%s' (ent %i) (vol %f)\n",
			gpGlobals->curtime, ep.m_pSoundName, entindex, ep.m_flVolume );

		// TERROR:
		float timeSpent = ( Plat_FloatTime() - startTime ) * 1000.0f;
		const float thinkLimit = 50.0f;
		if ( timeSpent > thinkLimit )
		{
			DevMsg( "CSoundEmitterSystem::EmitSound(%s) took %f milliseconds (client)\n",
				ep.m_pSoundName, timeSpent );
		}
		return nGuid;
	}

	//
	// Checks for direct soundfile reference and splits to either gamesound handle
	// based emission or direct soundfile emission
	//
	int EmitSound( IRecipientFilter& filter, int entindex, const EmitSound_t & ep )
	{
		CVProfScope _( "CSoundEmitterSystem::EmitSound (calls engine)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		// Is this a direct soundfile reference or pre-parameterized call?
		if ( ep.m_pSoundName && 
			( V_stristr( ep.m_pSoundName, ".wav" ) || 
			  V_stristr( ep.m_pSoundName, ".mp3" ) || 
			  ep.m_pSoundName[0] == '!' ))
		{
			return EmitSoundBySoundFile(filter, entindex, ep);
		}
		
		// handle as a script sound entry
		if ( ep.m_hSoundScriptHash == SOUNDEMITTER_INVALID_HASH )
		{
			ep.m_hSoundScriptHash = soundemitterbase->HashSoundName( ep.m_pSoundName );
		}

		return EmitSoundByHandle( filter, entindex, ep, ep.m_hSoundScriptHash );
	}

	void EmitCloseCaption( IRecipientFilter& filter, int entindex, bool fromplayer, char const *token, std::vector< Vector >& originlist, float duration, bool warnifmissing /*= false*/, bool bForceSubtitle = false )
	{
		// Don't use dedicated closecaption ConVar since it will prevent remote clients from getting captions.
		// Okay to use it in SP, since it's the same ConVar, not the FCVAR_USERINFO one
		if ( gpGlobals->maxClients == 1 &&
			!g_pClosecaption->GetBool())
		{
			return;
		}

		// A negative duration means fill it in from the wav file if possible
		if ( duration < 0.0f )
		{
			char const *wav = soundemitterbase->GetWavFileForSound( token, GENDER_NONE );
			if ( wav )
			{
				duration = enginesound->GetSoundDuration( wav );
			}
			else
			{
				duration = 2.0f;
			}
		}

		char lowercase[ 256 ];
		V_strncpy( lowercase, token, sizeof( lowercase ) );
		V_strlower( lowercase );
		if ( V_strstr( lowercase, "\\" ) )
		{
			Hack_FixEscapeChars( lowercase );
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		// Captions only route to host player (there is only one closecaptioning HUD)
		filterCopy.RemoveSplitScreenPlayers();

		if ( !bForceSubtitle )
		{
			// Remove any players who don't want close captions
			CBaseEntity::RemoveRecipientsIfNotCloseCaptioning( (CRecipientFilter &)filterCopy );
		}

		// Anyone left?
		if ( filterCopy.GetRecipientCount() > 0 )
		{

			// Close caption HUD removed (VGUI teardown)
		}
	}

	void EmitCloseCaption( IRecipientFilter& filter, int entindex, const CSoundParameters & params, const EmitSound_t & ep )
	{
		// Don't use dedicated closecaption ConVar since it will prevent remote clients from getting captions.
		// Okay to use it in SP, since it's the same ConVar, not the FCVAR_USERINFO one
		if ( gpGlobals->maxClients == 1 &&
			!g_pClosecaption->GetBool())
		{
			return;
		}

		bool bForceSubtitle = false;

		if ( TestSoundChar( params.soundname, CHAR_SUBTITLED ) )
		{
			bForceSubtitle = true;
		}

		if ( !bForceSubtitle && !ep.m_bEmitCloseCaption )
		{
			return;
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		if ( !bForceSubtitle )
		{
			// Remove any players who don't want close captions
			CBaseEntity::RemoveRecipientsIfNotCloseCaptioning( (CRecipientFilter &)filterCopy );
		}

		// Anyone left?
		if ( filterCopy.GetRecipientCount() <= 0 )
		{
			return;
		}

		float duration = 0.0f;
		if ( ep.m_pflSoundDuration )
		{
			duration = *ep.m_pflSoundDuration;
		}
		else
		{
			duration = enginesound->GetSoundDuration( params.soundname );
		}

		bool fromplayer = false;
		CBaseEntity *ent = CBaseEntity::Instance( entindex );
		if ( ent )
		{
			while ( ent )
			{
				if ( ent->IsPlayer() )
				{
					fromplayer = true;
					break;
				}

				ent = ent->GetOwnerEntity();
			}
		}
		EmitCloseCaption( filter, entindex, fromplayer, ep.m_pSoundName, ep.m_UtlVecSoundOrigin, duration, ep.m_bWarnOnMissingCloseCaption, bForceSubtitle );
	}

	void EmitAmbientSoundAsEntry(CSoundParameters &params, int entindex, const Vector& origin, const char *soundname, float flVolume, int iFlags, int iPitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{
		EmitSound_t ep ;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = soundname;
		ep.m_flVolume = flVolume;
		ep.m_SoundLevel = params.soundlevel;
		ep.m_nFlags = iFlags;
		ep.m_nPitch = iPitch;
		ep.m_pOrigin = &origin;
		ep.m_flSoundTime = soundtime;
		ep.m_pflSoundDuration = duration;
//		ep.	m_bEmitCloseCaption = true;
//			m_bWarnOnMissingCloseCaption = false;
//			m_bWarnOnDirectWaveReference = false;
//			m_nSpeakerEntity = -1;
			// if sound is tagged as version 2 or higher this will be treated as a soundentry!
		ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

		// send sound to all active players
		CReliableBroadcastRecipientFilter filter;

		EmitSoundByHandle( filter, entindex, ep, params.m_hSoundScriptHash );
	}

	void EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, float flVolume, int iFlags, int iPitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{
		// Pull data from parameters
		CSoundParameters params;

		if ( !soundemitterbase->GetParametersForSound( soundname, params, GENDER_NONE ) )
		{
			return;
		}

		// hijack if it's a new style sound
		if( params.m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH && params.m_nSoundEntryVersion > 1 )
		{
			EmitAmbientSoundAsEntry( params, entindex, origin, soundname, flVolume, iFlags, iPitch, soundtime, duration );
			return;
		}

		if( iFlags & SND_CHANGE_PITCH )
		{
			params.pitch = iPitch;
		}

		if( iFlags & SND_CHANGE_VOL )
		{
			params.volume = flVolume;
		}

		enginesound->EmitAmbientSound( params.soundname, params.volume, params.pitch, iFlags, soundtime );

		bool needsCC = !( iFlags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) );

		float soundduration = 0.0f;
		
		if ( duration
			|| needsCC
			)
		{
			soundduration = enginesound->GetSoundDuration( params.soundname );
			if ( duration )
			{
				*duration = soundduration;
			}
		}

		TraceEmitSound( entindex, "EmitAmbientSound:  '%s' emitted as '%s' (ent %i)\n",
			soundname, params.soundname, entindex );

		// We only want to trigger the CC on the start of the sound, not on any changes or halting of the sound
		if ( needsCC )
		{
			CRecipientFilter filter;
			filter.AddAllPlayers();
			filter.MakeReliable();

			std::vector< Vector > dummy;
			EmitCloseCaption( filter, entindex, false, soundname, dummy, soundduration, false );
		}

	}

	void StopSoundByHandle( int entindex, const char *soundname, HSOUNDSCRIPTHASH& handle, bool bIsStoppingSpeakerSound = false )
	{
		if ( handle == SOUNDEMITTER_INVALID_HASH )
		{
			handle = soundemitterbase->HashSoundName( soundname );
		}

		int index = soundemitterbase->GetSoundIndexForHash( handle );

		CSoundParametersInternal *params = soundemitterbase->InternalGetParametersForSound( index );
		if ( !params )
		{
			return;
		}

		const char *pSoundEntryName = nullptr;
		if( params->GetSoundEntryVersion() > 1 &&
			sv_soundemitter_version.GetInt() > 1 )
		{
			// reget original soundentry name
			pSoundEntryName = soundemitterbase->GetSoundName( index );
			enginesound->StopSound( 
				entindex, 
				params->GetChannel(), 
				pSoundEntryName,
				handle );
			TraceEmitSoundEntry( handle, pSoundEntryName, soundname );
		}

		// HACK:  we have to stop all sounds if there are > 1 in the rndwave section...
		int c = params->NumSoundNames();
		for ( int i = 0; i < c; ++i )
		{
			char const *wavename = soundemitterbase->GetWaveName( params->GetSoundNames()[ i ].symbol );
			contract_assert( wavename );

			enginesound->StopSound( 
				entindex, 
				params->GetChannel(), 
				wavename );

			TraceEmitSound( entindex, "StopSound:  '%s' stopped as '%s' (ent %i)\n",
				soundname, wavename, entindex );

		}

	}

	void StopSound( int entindex, const char *soundname )
	{
		HSOUNDSCRIPTHASH hash = SOUNDEMITTER_INVALID_HASH;
		StopSoundByHandle( entindex, soundname, hash );
	}

	void StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound = false )
	{
		if ( pSample && ( V_stristr( pSample, ".wav" ) || V_stristr( pSample, ".mp3" ) || pSample[0] == '!' ) )
		{
			enginesound->StopSound( iEntIndex, iChannel, pSample );

			TraceEmitSound( iEntIndex, "StopSound:  Raw wave stopped '%s' (ent %i)\n",
				pSample, iEntIndex );
		}
		else
		{
			// Look it up in sounds.txt and ignore other parameters
			StopSound( iEntIndex, pSample );
		}
	}

	void EmitAmbientSound( int entindex, const Vector &origin, const char *pSample, float volume, soundlevel_t soundlevel, int flags, int pitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{

		if ( pSample && ( V_stristr( pSample, ".wav" ) || V_stristr( pSample, ".mp3" )) )
		{
			enginesound->EmitAmbientSound( pSample, volume, pitch, flags, soundtime );

			if ( duration )
			{
				*duration = enginesound->GetSoundDuration( pSample );
			}

			TraceEmitSound( entindex, "EmitAmbientSound:  Raw wave emitted '%s' (ent %i)\n",
				pSample, entindex );
		}
		else
		{
			EmitAmbientSound( entindex, origin, pSample, volume, flags, pitch, soundtime, duration );
		}
	}

};

static CSoundEmitterSystem g_SoundEmitterSystem( "CSoundEmitterSystem" );

IGameSystem *SoundEmitterSystem()
{
	return &g_SoundEmitterSystem;
}

void SoundSystemPreloadSounds( void )
{
	g_SoundEmitterSystem.PreloadSounds();
}

//!!!HACK- Zoid 8/9/2009
//This hack is for L4D DLC2.  We need to reload the soundemitter, but its reference counted by the
//client and the server, so we have to Shutdown() and Init() twice.  
static void cl_soundemitter_reload( const CCommand &args );
static ConCommand cl_soundemitter_reload_command( side_registry(), "cl_soundemitter_reload", cl_soundemitter_reload, "Flushes the sounds.txt system" );
static void cl_soundemitter_reload( const CCommand &args )
{
	// kill the system
	g_SoundEmitterSystem.Shutdown();

	// restart the system
	g_SoundEmitterSystem.Init();
}

static void cl_soundemitter_flush( const CCommand &args );
static ConCommand cl_soundemitter_flush_command( side_registry(), "cl_soundemitter_flush", cl_soundemitter_flush, "Flushes the sounds.txt system (server only)" );
static void cl_soundemitter_flush( const CCommand &args )
{
	// save the current soundscape
	// kill the system
	g_SoundEmitterSystem.Flush();

	// Redo precache all wave files... (this should work now that we have dynamic string tables)
	g_SoundEmitterSystem.LevelInitPreEntity();

	// These store raw sound indices for faster precaching, blow them away.
//	ClearModelSoundsCache();
	// TODO:  when we go to a handle system, we'll need to invalidate handles somehow
}

void Playgamesound_f( const CCommand &args )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		if ( args.ArgC() > 2 )
		{
			EmitSound_t params;
			if ( !V_strcmp( args[2], "stop" ) )
			{
				pPlayer->StopSound( args[1] );
				return;
			}

			ABS_QUERY_GUARD( true );
			CBroadcastRecipientFilter filter;

			Vector position = pPlayer->EyePosition();
			Vector forward;
			pPlayer->GetVectors( &forward, nullptr, nullptr );
			position += atof( args[2] ) * forward;
			params.m_pOrigin = &position;
			params.m_pSoundName = args[1];
			params.m_flVolume = 0.0f;
			params.m_nPitch = 0;
	
			g_SoundEmitterSystem.EmitSound( filter, 0, params );

		}
		else
		{
			pPlayer->EmitSound( args[1] );
		}
	}
	else
	{
		Msg("Can't play until a game is started.\n");
		// UNDONE: Make something like this work?
		//CBroadcastRecipientFilter filter;
		//g_SoundEmitterSystem.EmitSound( filter, 1, args[1], 0.0, 0, 0, &vec3_origin, 0, NULL );
	}
}

static int GamesoundCompletion( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	int current = 0;

	const char *cmdname = "playgamesound";
	char *substring = nullptr;
	int substringLen = 0;
	if ( V_strstr( partial, cmdname ) && strlen(partial) > strlen(cmdname) + 1 )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
		substringLen = strlen(substring);
	}
	
	for ( int i = soundemitterbase->GetSoundCount()-1; i >= 0 && current < COMMAND_COMPLETION_MAXITEMS; i-- )
	{
		const char *pSoundName = soundemitterbase->GetSoundName( i );
		if ( pSoundName )
		{
			if ( !substring || !V_strncasecmp( pSoundName, substring, substringLen ) )
			{
				V_snprintf( commands[ current ], sizeof( commands[ current ] ), "%s %s", cmdname, pSoundName );
				current++;
			}
		}
	}

	return current;
}

static ConCommand Command_Playgamesound( side_registry(), "playgamesound", Playgamesound_f, "Play a sound from the game sounds txt file", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE, GamesoundCompletion );

// --------------------------------------------------------------------
// snd_playsounds
//
// This a utility for testing sound values
// --------------------------------------------------------------------

static int GamesoundCompletion2( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	int current = 0;

	const char *cmdname = "snd_playsounds";
	char *substring = nullptr;
	int substringLen = 0;
	if ( V_strstr( partial, cmdname ) && strlen(partial) > strlen(cmdname) + 1 )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
		substringLen = strlen(substring);
	}
	
	for ( int i = soundemitterbase->GetSoundCount()-1; i >= 0 && current < COMMAND_COMPLETION_MAXITEMS; i-- )
	{
		const char *pSoundName = soundemitterbase->GetSoundName( i );
		if ( pSoundName )
		{
			if ( !substring || !V_strncasecmp( pSoundName, substring, substringLen ) )
			{
				V_snprintf( commands[ current ], sizeof( commands[ current ] ), "%s %s", cmdname, pSoundName );
				current++;
			}
		}
	}

	return current;
}

void S_PlaySounds( const CCommand &args )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		if ( args.ArgC() > 4 )
		{
//			Vector position = pPlayer->EyePosition();
			Vector position;
			//	Vector forward;
		//	pPlayer->GetVectors( &forward, NULL, NULL );
		//	position += atof( args[2] ) * forward;
			position[0] = atof( args[2] );
			position[1] = atof( args[3] );
			position[2] = atof( args[4] );

			ABS_QUERY_GUARD( true );
			CBroadcastRecipientFilter filter;
			EmitSound_t params;
			params.m_pSoundName = args[1];
			params.m_pOrigin = &position;
			params.m_flVolume = 0.0f;
			params.m_nPitch = 0;
			g_SoundEmitterSystem.EmitSound( filter, 0, params );
		}
		else
		{
			pPlayer->EmitSound( args[1] );
		}
	}
	else
	{
		Msg("Can't play until a game is started.\n");
		// UNDONE: Make something like this work?
		//CBroadcastRecipientFilter filter;
		//g_SoundEmitterSystem.EmitSound( filter, 1, args[1], 0.0, 0, 0, &vec3_origin, 0, NULL );
	}
}

static ConCommand SND_PlaySounds( side_registry(), "snd_playsounds", S_PlaySounds, "Play sounds from the game sounds txt file at a given location", FCVAR_CHEAT | FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE, GamesoundCompletion2 );

static int GamesoundCompletion3( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	int current = 0;

	const char *cmdname = "snd_setsoundparam";
	char *substring = nullptr;
	int substringLen = 0;
	if ( V_strstr( partial, cmdname ) && strlen(partial) > strlen(cmdname) + 1 )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
		substringLen = strlen(substring);
	}
	
	for ( int i = soundemitterbase->GetSoundCount()-1; i >= 0 && current < COMMAND_COMPLETION_MAXITEMS; i-- )
	{
		const char *pSoundName = soundemitterbase->GetSoundName( i );
		if ( pSoundName )
		{
			if ( !substring || !V_strncasecmp( pSoundName, substring, substringLen ) )
			{
				V_snprintf( commands[ current ], sizeof( commands[ current ] ), "%s %s", cmdname, pSoundName );
				current++;
			}
		}
	}

	return current;
}

static void S_SetSoundParam( const CCommand &args )
{
	if ( args.ArgC() != 4 )
	{
		DevMsg("Parameters: mix group name, [vol, mute, solo], value");
		return;
	}

	const char *szSoundName = args[1];
	const char *szparam = args[2];
	const char *szValue = args[3];

	// get the sound we're working on
	int soundindex = soundemitterbase->GetSoundIndex( szSoundName);
	if ( !soundemitterbase->IsValidIndex(soundindex) )
		return;

	// Look up the sound level from the soundemitter system
	CSoundParametersInternal *soundparams = soundemitterbase->InternalGetParametersForSound( soundindex );
	if ( !soundparams )
	{
		return;
	}

	// // See if it's writable, if not then bail
	// char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
	// if ( !scriptfile || 
		 // !filesystem->FileExists( scriptfile ) ||
		 // !filesystem->IsFileWritable( scriptfile ) )
	// {
		// return;
	// }

	// Copy the parameters
	CSoundParametersInternal newparams;
	newparams.CopyFrom( *soundparams );
				
	if(!V_stricmp("volume", szparam))
		newparams.VolumeFromString( szValue);
	else if(!V_stricmp("level", szparam))
		newparams.SoundLevelFromString( szValue );

	// No change
	if ( newparams == *soundparams )
	{
		return;
	}

	soundemitterbase->UpdateSoundParameters( szSoundName , newparams );

}

static ConCommand SND_SetSoundParam( side_registry(), "snd_setsoundparam", S_SetSoundParam, "Set a sound paramater", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE, GamesoundCompletion3 );

//-----------------------------------------------------------------------------
// Purpose:  Non-static override for doing the general case of CBroadcastRecipientFilter, and EmitSound( filter, entindex(), etc. );
// Input  : *soundname - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( const char *soundname, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	//VPROF( "CBaseEntity::EmitSound" );
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	ABS_QUERY_GUARD( true );
	CBroadcastRecipientFilter filter;
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, entindex(), params );
}

//-----------------------------------------------------------------------------
// Purpose:  Non-static override for doing the general case of CBroadcastRecipientFilter, and EmitSound( filter, entindex(), etc. );
// Input  : *soundname - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( const char *soundname, HSOUNDSCRIPTHASH& handle, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	// VPROF( "CBaseEntity::EmitSound" );
	ABS_QUERY_GUARD( true );
	CBroadcastRecipientFilter filter;

	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, entindex(), params, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			*soundname - 
//			*pOrigin - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, const Vector *pOrigin /*= NULL*/, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	// VPROF( "CBaseEntity::EmitSound" );
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pOrigin = pOrigin;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, iEntIndex, params );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			*soundname - 
//			*pOrigin - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, HSOUNDSCRIPTHASH& handle, const Vector *pOrigin /*= NULL*/, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	//VPROF( "CBaseEntity::EmitSound" );
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pOrigin = pOrigin;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, iEntIndex, params, handle );
}

static void Helper_UpdateLastMadeNoiseTime( const IRecipientFilter &filter, int iEntIndex, const EmitSound_t &params )
{
	CBaseEntity * pEnt = nullptr;

	pEnt = ClientEntityList().GetEnt( iEntIndex );

	if ( pEnt )
		pEnt->UpdateLastMadeNoiseTime( params.m_pSoundName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			params - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	Helper_UpdateLastMadeNoiseTime( filter, iEntIndex, params );
	
	// VPROF( "CBaseEntity::EmitSound" );
	// Call into the sound emitter system...
	return g_SoundEmitterSystem.EmitSound( filter, iEntIndex, params );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			params - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params, HSOUNDSCRIPTHASH& handle )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	Helper_UpdateLastMadeNoiseTime( filter, iEntIndex, params );

	// VPROF( "CBaseEntity::EmitSound" );
	// Call into the sound emitter system...
	return g_SoundEmitterSystem.EmitSoundByHandle( filter, iEntIndex, params, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( const char *soundname )
{
	if ( entindex() == -1 )
	{
		// If we're a clientside entity, we need to use the soundsourceindex instead of the entindex
		StopSound( GetSoundSourceIndex(), soundname );
		return;
	}

	StopSound( entindex(), soundname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( const char *soundname, HSOUNDSCRIPTHASH& handle )
{
	if ( entindex() == -1 )
	{
		// If we're a clientside entity, we need to use the soundsourceindex instead of the entindex
		StopSound( GetSoundSourceIndex(), soundname );
		return;
	}

	g_SoundEmitterSystem.StopSoundByHandle( entindex(), soundname, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iEntIndex - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( int iEntIndex, const char *soundname )
{
	g_SoundEmitterSystem.StopSound( iEntIndex, soundname );
}

void CBaseEntity::StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound )
{
	g_SoundEmitterSystem.StopSound( iEntIndex, iChannel, pSample, bIsStoppingSpeakerSound );
}

soundlevel_t CBaseEntity::LookupSoundLevel( const char *soundname )
{
	return soundemitterbase->LookupSoundLevel( soundname );
}

soundlevel_t CBaseEntity::LookupSoundLevel( const char *soundname, HSOUNDSCRIPTHASH& handle )
{
	return soundemitterbase->LookupSoundLevelByHandle( soundname, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *entity - 
//			origin - 
//			flags - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, int flags, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	g_SoundEmitterSystem.EmitAmbientSound( entindex, origin, soundname, 0.0, flags, 0, soundtime, duration );
}

// HACK HACK:  Do we need to pull the entire SENTENCEG_* wrapper over to the client .dll?
int SENTENCEG_Lookup(const char *sample)
{
	return engine->SentenceIndexFromName( sample + 1 );
}

void UTIL_EmitAmbientSound( int entindex, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	if (samp && *samp == '!')
	{
		int sentenceIndex = SENTENCEG_Lookup(samp);
		if (sentenceIndex >= 0)
		{
			char name[32];
			V_snprintf( name, sizeof(name), "!%d", sentenceIndex );
			enginesound->EmitAmbientSound( name, vol, pitch, fFlags, soundtime );
			if ( duration )
			{
				*duration = enginesound->GetSoundDuration( name );
			}

			g_SoundEmitterSystem.TraceEmitSound( entindex, "UTIL_EmitAmbientSound:  Sentence emitted '%s' (ent %i)\n",
				name, entindex );
		}
	}
	else
	{
		g_SoundEmitterSystem.EmitAmbientSound( entindex, vecOrigin, samp, vol, soundlevel, fFlags, pitch, soundtime, duration );
	}
}

static const char *UTIL_TranslateSoundName( const char *soundname, const char *actormodel )
{
	contract_assert( soundname );

	if ( V_stristr( soundname, ".wav" ) || V_stristr( soundname, ".mp3" ) )
	{
		if ( V_stristr( soundname, ".wav" ) )
		{
			WaveTrace( soundname, "UTIL_TranslateSoundName" );
		}
		return soundname;
	}

	return soundemitterbase->GetWavFileForSound( soundname, actormodel );
}

void CBaseEntity::GenderExpandString( char const *in, char *out, int maxlen )
{
	soundemitterbase->GenderExpandString( STRING( GetModelName() ), in, out, maxlen );
}

bool CBaseEntity::GetParametersForSound( const char *soundname, CSoundParameters &params, const char *actormodel )
{
	gender_t gender = soundemitterbase->GetActorGender( actormodel );
	
	return soundemitterbase->GetParametersForSound( soundname, params, gender );
}

bool CBaseEntity::GetParametersForSound( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters &params, const char *actormodel )
{
	gender_t gender = soundemitterbase->GetActorGender( actormodel );
	
	return soundemitterbase->GetParametersForSoundEx( soundname, handle, params, gender );
}

HSOUNDSCRIPTHASH CBaseEntity::PrecacheScriptSound( const char *soundname )
{
	HSOUNDSCRIPTHASH hash = soundemitterbase->HashSoundName( soundname );
	int soundIndex = soundemitterbase->GetSoundIndexForHash( hash );
	if ( soundemitterbase->IsValidIndex( soundIndex ) )
		return hash;
	return SOUNDEMITTER_INVALID_HASH;
}

void CBaseEntity::PrefetchScriptSound( const char *soundname )
{
	g_SoundEmitterSystem.PrefetchScriptSound( soundname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : float
//-----------------------------------------------------------------------------
float CBaseEntity::GetSoundDuration( const char *soundname, char const *actormodel )
{
	return enginesound->GetSoundDuration( PSkipSoundChars( UTIL_TranslateSoundName( soundname, actormodel ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			*token - 
//			duration - 
//			warnifmissing - 
//-----------------------------------------------------------------------------
void CBaseEntity::EmitCloseCaption( IRecipientFilter& filter, int entindex, char const *token, std::vector< Vector >& soundorigin, float duration, bool warnifmissing /*= false*/ )
{
	bool fromplayer = false;
	CBaseEntity *ent = CBaseEntity::Instance( entindex );
	while ( ent )
	{
		if ( ent->IsPlayer() )
		{
			fromplayer = true;
			break;
		}
		ent = ent->GetOwnerEntity();
	}

	g_SoundEmitterSystem.EmitCloseCaption( filter, entindex, fromplayer, token, soundorigin, duration, warnifmissing );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PrecacheSound( const char *name )
{
	if ( !g_bPermitDirectSoundPrecache )
	{
		Warning( "Direct precache of %s\n", name );
	}

	// If this is out of order, warn
	if ( !CBaseEntity::IsPrecacheAllowed() )
	{
		if ( !enginesound->IsSoundPrecached( name ) )
		{
			contract_assert( !"CBaseEntity::PrecacheSound:  too late" );

			Warning( "Late precache of %s\n", name );
		}
	}

	bool bret = enginesound->PrecacheSound( name, true );
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseEntity::PrefetchSound( const char *name )
{
	 enginesound->PrefetchSound( name );
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

static ConVar soundpatch_captionlength( side_registry(), "soundpatch_captionlength", "2.0", FCVAR_REPLICATED, "How long looping soundpatch captions should display for." );

// Envelope
// This is a class that controls a ramp for a sound (pitch / volume / etc)
class CSoundEnvelope
{
public:
	DECLARE_SIMPLE_DATADESC();

	CSoundEnvelope()
	{
		m_current = 0.0f;
		m_target = 0.0f;
		m_rate = 0.0f;
		m_forceupdate = false;
	}

	void SetTarget( float target, float deltaTime );
	void SetValue( float value );
	bool ShouldUpdate( void );
	void Update( float time );
	inline float	Value( void ) { return m_current; }

private:
	float	m_current;
	float	m_target;
	float	m_rate;
	bool	m_forceupdate;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundEnvelope )

//-----------------------------------------------------------------------------
// Purpose: Set the new target value for this ramp.  Reach this target in deltaTime
//			seconds from now
// Input  : target - new target value
//			deltaTime - time to reach target
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetTarget( float target, float deltaTime )
{
	float deltaValue = target - m_current;

	if ( deltaValue && deltaTime > 0 )
	{
		m_target = target;
		m_rate = std::max<std::common_type_t<double, float>>( 0.1, fabs(deltaValue / deltaTime) );
	}
	else
	{
		if ( target != m_current )
		{
			m_forceupdate = true;
		}

		SetValue( target );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Instantaneously set the value of this ramp
// Input  : value - new value
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetValue( float value )
{
	if ( m_target != value )
	{
		m_forceupdate = true;
	}

	m_current = m_target = value;
	m_rate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if I need to update this envelope
// Output : Returns true if this envelope is changing
//-----------------------------------------------------------------------------
bool CSoundEnvelope::ShouldUpdate( void )
{
	if ( m_forceupdate )
	{
		m_forceupdate = false;
		return true;
	}

	if ( m_current != m_target )
	{
		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Update the envelope for the current frame time
// Input  : time - amount of time that has passed
//-----------------------------------------------------------------------------
void CSoundEnvelope::Update( float deltaTime )
{
	m_current = Approach( m_target, m_current, m_rate * deltaTime );
}

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	DECLARE_SIMPLE_DATADESC();

	CCopyRecipientFilter() : m_Flags(0) {}

	void Init( IRecipientFilter *pSrc )
	{
		m_Flags = FLAG_ACTIVE;
		if ( pSrc->IsReliable() )
		{
			m_Flags |= FLAG_RELIABLE;
		}

		if ( pSrc->IsInitMessage() )
		{
			m_Flags |= FLAG_INIT_MESSAGE;
		}

		for ( int i = 0; i < pSrc->GetRecipientCount(); i++ )
		{
			int index = pSrc->GetRecipientIndex( i );
			
			if ( index >= 0 )
				m_Recipients.push_back( index );
		}
	}

	bool IsActive() const 
	{
		return (m_Flags & FLAG_ACTIVE) != 0;
	}

	virtual bool IsReliable( void ) const
	{
		return (m_Flags & FLAG_RELIABLE) != 0;
	}

	virtual int	GetRecipientCount( void ) const
	{
		return std::ssize( m_Recipients );
	}

	virtual int	GetRecipientIndex( int slot ) const
	{
		return m_Recipients[ slot ];
	}

	virtual bool IsInitMessage( void ) const
	{
		return (m_Flags & FLAG_INIT_MESSAGE) != 0;
	}

	virtual bool AddRecipient( CBasePlayer *player )
	{
		contract_assert( player );

		int index = player->entindex();

		if ( index < 0 )
			return false;

		// Already in list
		if ( ks::find_index( m_Recipients, index ) != -1 )
			return false;

		m_Recipients.push_back( index );
		return true;
	}

private:
	enum
	{
		FLAG_ACTIVE = 0x1,
		FLAG_RELIABLE = 0x2,
		FLAG_INIT_MESSAGE = 0x4,
	};

	int m_Flags;
	std::vector< int > m_Recipients;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CCopyRecipientFilter )

// This is the a basic sound controller, a "patch"
// It has envelopes for pitch and volume and can manage state changes to those
class CSoundPatch : public ks::pooled< CSoundPatch >
{
public:
	DECLARE_SIMPLE_DATADESC();

	static int g_SoundPatchCount;
	CSoundPatch()
	{
		g_SoundPatchCount++;
		m_iszSoundName = NULL_STRING;
		m_iszSoundScriptName = NULL_STRING;
		m_flCloseCaptionDuration = soundpatch_captionlength.GetFloat();
		m_soundOrigin.Init();
		m_soundEntityIndex = -1;
		m_guid = -1;

		m_hSoundScriptHash = SOUNDEMITTER_INVALID_HASH;
		m_nSoundEntryVersion = 1;
	}
	~CSoundPatch()
	{
		g_SoundPatchCount--;
	}

	void	Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
				soundlevel_t iSoundLevel, const Vector *pSoundOrigin, float scriptVolume = 1.0f );
	void	ChangePitch( float pitchTarget, float deltaTime );
	void	ChangeVolume( float volumeTarget, float deltaTime );
	void	FadeOut( float deltaTime, bool destroyOnFadeout );
	float	GetPitch( void );
	float	GetVolume( void );
	string_t GetName() { return m_iszSoundName; };
	int		GetGuid() { return m_guid; };
	float	GetElapsedTime( void );
	bool	IsStillPlaying( void );

	string_t GetScriptName() { return m_iszSoundScriptName; }
	// UNDONE: Don't call this, use the controller to shut down
	void	Shutdown( void );
	bool	Update( float time, float deltaTime );
	void	Reset( void );
	void	StartSound( float flStartTime = 0 );
	void	ResumeSound( void );
	int		IsPlaying( void ) { return m_isPlaying; }
	float	GetShutdownTime( void ) const { return m_shutdownTime; } // TERROR: debugging
	void	AddPlayerPost( CBasePlayer *pPlayer );
	void	SetCloseCaptionDuration( float flDuration ) { m_flCloseCaptionDuration = flDuration; }

	void	SetBaseFlags( int iFlags ) { m_baseFlags = iFlags; }
	
	// Returns the ent index
	int		EntIndex() const;

private:
	// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
	// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
	float	GetVolumeForEngine( void );

private:
	CSoundEnvelope	m_pitch;
	CSoundEnvelope	m_volume;

	int				m_guid;
	soundlevel_t	m_soundlevel;
	float			m_shutdownTime;
	string_t		m_iszSoundName;
	string_t		m_iszSoundScriptName;
	HSOUNDSCRIPTHASH m_hSoundScriptHash;
	int             m_nSoundEntryVersion;
	EHANDLE			m_hEnt;
	int				m_entityChannel;
	int				m_soundEntityIndex;
	Vector			m_soundOrigin;
	int				m_flags;
	int				m_baseFlags;
	int				m_isPlaying;
	float			m_flScriptVolume;	// Volume for this sound in sounds.txt
	CCopyRecipientFilter m_Filter;

	float			m_flCloseCaptionDuration;

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CSoundPatch >;
};

int CSoundPatch::g_SoundPatchCount = 0;

static void cl_report_soundpatch( const CCommand &args );
static ConCommand cl_report_soundpatch_command( side_registry(), "cl_report_soundpatch", cl_report_soundpatch, "reports client-side sound patch count" );
static void cl_report_soundpatch( const CCommand &args )
{

	Msg("Current sound patches: %d\n", CSoundPatch::g_SoundPatchCount );
}
ks::fixed_pool CSoundPatch::s_Allocator( sizeof(CSoundPatch), 64, ks::fixed_pool::grow::fast, "CSoundPatch pool", alignof(CSoundPatch) );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundPatch )

//-----------------------------------------------------------------------------
// Purpose: Setup the patch
// Input  : nEntIndex - index of the edict that owns the sound channel
//			channel - This is a sound channel (CHAN_ITEM, CHAN_STATIC)
//			*pSoundName - sound script string name
//			attenuation - attenuation of this sound (not animated)
//-----------------------------------------------------------------------------
void CSoundPatch::Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
			soundlevel_t soundlevel, const Vector *pSoundOrigin , float scriptVolume)
{
	m_hEnt = pEnt;
	if ( pEnt )
	{
		m_soundEntityIndex = pEnt->entindex();
	}
	m_entityChannel = channel;

	// if not a direct wave reference, crack the script
	CSoundParameters params;
	if ( !V_stristr( pSoundName, ".wav" ) && !V_stristr( pSoundName, ".mp3" ) &&
		CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
	{
		m_flScriptVolume = params.volume;
		// This has to be the actual .wav because rndwave would cause a bunch of new .wavs to play... bad...
		//  e.g., when you pitch shift it would start a different wav instead.

		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_hSoundScriptHash = params.m_hSoundScriptHash;
		m_nSoundEntryVersion = params.m_nSoundEntryVersion;

		pSoundName = params.soundname;
		m_soundlevel = params.soundlevel;

		// TERROR: if we say we want CHAN_USER_BASE + N, we mean it!
		if ( m_entityChannel < CHAN_USER_BASE )
		{
			m_entityChannel = params.channel;
		}
	}
	else
	{
		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_flScriptVolume = scriptVolume;
		m_soundlevel = soundlevel;
	}

	m_iszSoundName = AllocPooledString( pSoundName );
	m_volume.SetValue( 0 );
	m_pitch.SetValue( 0 );
	m_isPlaying = false;
	m_shutdownTime = 0;
	m_Filter.Init( pFilter );
	m_baseFlags = 0;
	if( pSoundOrigin )
	{
		m_soundOrigin.x = pSoundOrigin->x;
		m_soundOrigin.y = pSoundOrigin->y;
		m_soundOrigin.z = pSoundOrigin->z;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Ramps the pitch to a new value
// Input  : pitchTarget - new value
//			deltaTime - seconds to reach the value
//-----------------------------------------------------------------------------
void CSoundPatch::ChangePitch( float pitchTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_PITCH;
	m_pitch.SetTarget( pitchTarget, deltaTime );
}

//-----------------------------------------------------------------------------
// Purpose: Ramps the volume to a new value
// Input  : volumeTarget - new volume
//			deltaTime - seconds to reach the new volume 
//-----------------------------------------------------------------------------
void CSoundPatch::ChangeVolume( float volumeTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_VOL;
	if ( volumeTarget > 1.0 )
		volumeTarget = 1.0;
	m_volume.SetTarget( volumeTarget, deltaTime );
}

//-----------------------------------------------------------------------------
// Purpose: Fade volume to zero AND SHUT DOWN THIS SOUND
// Input  : deltaTime - seconds before done/shutdown
//-----------------------------------------------------------------------------
void CSoundPatch::FadeOut( float deltaTime, bool destroyOnFadeout )
{
	ChangeVolume( 0, deltaTime );
	if ( !destroyOnFadeout )
	{
		m_shutdownTime = g_pEffects->Time() + deltaTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current pitch
//-----------------------------------------------------------------------------
float CSoundPatch::GetPitch( void )
{
	return m_pitch.Value();
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current volume
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolume( void )
{
	return m_volume.Value();
}

//-----------------------------------------------------------------------------
// Purpose: Get the playing status of the sound
// Returns: Sounds playing status from the engine
//-----------------------------------------------------------------------------
bool CSoundPatch::IsStillPlaying( void )
{
	return enginesound->IsSoundStillPlaying(m_guid);
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current elapsed time
// Returns: Time in seconds
//-----------------------------------------------------------------------------
float CSoundPatch::GetElapsedTime( void )
{
	// convert to seconds
	return enginesound->GetElapsedTimeByGuid(m_guid) * 0.01;
}
//-----------------------------------------------------------------------------
// Returns the ent index
//-----------------------------------------------------------------------------
int CSoundPatch::EntIndex() const
{
	contract_assert( !m_hEnt.IsValid() || m_hEnt.Get() );
	return m_hEnt.Get() ? m_hEnt->entindex() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
// Output : float
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolumeForEngine( void )
{
	return ( m_flScriptVolume * m_volume.Value() );
}

//-----------------------------------------------------------------------------
// Purpose: Stop the sound
//-----------------------------------------------------------------------------
void CSoundPatch::Shutdown( void )
{
//	Msg( "Removing sound %s\n", m_pszSoundName );
	if ( m_isPlaying )
	{
		int entIndex = -1;
		if ( m_hEnt.Get() )
		{
			entIndex = EntIndex();
		}
		else
		{
			// may have deleted the entity after starting the sound, but before stopping the sound, try the saved index
			// this will handle that case so a sound patch doesn't get stuck on
			entIndex = m_soundEntityIndex;
		}
		contract_assert( entIndex >= 0 );
		// BUGBUG: Don't crash in release mode
		if ( entIndex >= 0 )
		{
			if( m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
			{
				CBaseEntity::StopSound( entIndex, STRING( m_iszSoundScriptName ) );
			}
			else
			{
				CBaseEntity::StopSound( entIndex, m_entityChannel, STRING( m_iszSoundName ) );
			}
		}
		m_isPlaying = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update all envelopes and send appropriate data to the client
// Input  : time - new global clock
//			deltaTime - amount of time that has passed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundPatch::Update( float time, float deltaTime )
{
	CVProfScope _( "CSoundPatch::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_shutdownTime && time > m_shutdownTime )
	{
		Shutdown();
		return false;
	}

	if ( EntIndex() < 0 )
	{
		// FIXME:  The pointer to this soundpatch is probably leaked since no entity is around to clean it up (ywb)
		DevWarning( "CSoundPatch::Update:  Removing CSoundPatch (%s) with NULL EHandle\n", STRING(m_iszSoundName) );
		return false;
	}

	if ( m_pitch.ShouldUpdate() )
	{
		m_pitch.Update( deltaTime );
		m_flags |= SND_CHANGE_PITCH;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_PITCH;
	}

	if ( m_volume.ShouldUpdate() )
	{
		m_volume.Update( deltaTime );
		m_flags |= SND_CHANGE_VOL;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_VOL;
	}

	// if ( m_flags && m_Filter.IsActive() )
	if ( m_flags )
	{
		// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
		// Because of this, we need to always set the SND_CHANGE_VOL flag when we emit sound, or it'll use the scriptfile's instead.
		m_flags |= SND_CHANGE_VOL;

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = m_flags;
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );

		m_flags = 0;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sound is going to start playing again, clear any shutdown time
//-----------------------------------------------------------------------------
void CSoundPatch::Reset( void )
{
	m_shutdownTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Start playing the sound - send updates to the client
//-----------------------------------------------------------------------------
void CSoundPatch::StartSound( float flStartTime )
{
//	Msg( "Start sound %s\n", m_pszSoundName );
	m_flags = 0;
	if ( m_Filter.IsActive() )
	{
		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		if ( V_stristr( STRING(m_iszSoundName), "music" ) )
		{
			ep.m_nFlags = m_baseFlags;
		}
		else
		{
			ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		}
		ep.m_nFlags |= SND_GENERATE_GUID;				// We need GUID in all cases, even for threaded sounds
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_bEmitCloseCaption = false;

		if ( flStartTime )
		{
			ep.m_flSoundTime = flStartTime;
		}

//#ifdef CLIENT_DLL
		{
			m_guid = CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
		}
		CBaseEntity::EmitCloseCaption( m_Filter, EntIndex(), STRING( m_iszSoundScriptName ), ep.m_UtlVecSoundOrigin, m_flCloseCaptionDuration, true );
	}
	m_isPlaying = true;
}

//-----------------------------------------------------------------------------
// Purpose: resumes playing the sound on restore
//-----------------------------------------------------------------------------
void CSoundPatch::ResumeSound( void )
{
	if ( IsPlaying() && m_Filter.IsActive() )
	{
		if ( EntIndex() >= 0 )
		{
			EmitSound_t ep;
			ep.m_nChannel = m_entityChannel;
			ep.m_pSoundName = STRING(m_iszSoundName);
			ep.m_flVolume = GetVolumeForEngine();
			ep.m_SoundLevel = m_soundlevel;
			ep.m_nFlags = (SND_CHANGE_VOL | SND_CHANGE_PITCH | m_baseFlags);
			ep.m_nPitch = (int)m_pitch.Value();

			ep.m_hSoundScriptHash = m_hSoundScriptHash;
			ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

			// only pass the position if it's coming from the world
			if( EntIndex() == 0 )
				ep.m_pOrigin = &m_soundOrigin;

			CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
		}
		else
		{
			// FIXME: Lost the entity on restore. It might have been suppressed by the save/restore system.
			// This will probably leak the sound patch since there's no one to delete it, but the next
			// call to CSoundPatch::Update should at least remove it from the list of sound patches.
			DevWarning( "CSoundPatch::ResumeSound: Lost EHAndle on restore - destroy the sound patch in your entity's StopLoopingSounds! (%s)\n", STRING( m_iszSoundName ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: A new player's entered the game. See if we need to restart our sound.
//-----------------------------------------------------------------------------
void CSoundPatch::AddPlayerPost( CBasePlayer *pPlayer )
{
	if ( m_Filter.IsActive() && m_Filter.AddRecipient(pPlayer) )
	{
		// Alrighty, he's new. We need to restart our sound just to him.
		// Create a new filter just to him.
		CSingleUserRecipientFilter filter( pPlayer );

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		ep.m_nPitch = (int)m_pitch.Value();

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( filter, EntIndex(), ep );
	}
}

// This is an entry in the command queue.  It's used to queue up various pitch and volume changes
// so you can define an envelope without writing timing code in an entity.  Existing queued commands
// can be deleted later if the envelope changes dynamically.
struct SoundCommand_t : public ks::pooled< SoundCommand_t >
{
	SoundCommand_t( void ) { std::memset( this, 0, sizeof(*this) ); }
	SoundCommand_t( CSoundPatch *pSound, float executeTime, soundcommands_t command, float deltaTime, float value ) : m_pPatch(pSound), m_time(executeTime), m_deltaTime(deltaTime), m_command(command), m_value(value) {}

	CSoundPatch		*m_pPatch;
	float			m_time;
	float			m_deltaTime;
	soundcommands_t	m_command;
	float			m_value;
	
	SoundCommand_t	*m_pNext;

	DECLARE_SIMPLE_DATADESC();
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< SoundCommand_t >;
};

ks::fixed_pool SoundCommand_t::s_Allocator( sizeof(SoundCommand_t), 32, ks::fixed_pool::grow::fast, "SoundCommand_t pool", alignof(SoundCommand_t) );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( SoundCommand_t )

typedef std::unique_ptr< SoundCommand_t > SOUNDCOMMANDPTR;

bool SoundCommandLessFunc( const SOUNDCOMMANDPTR &lhs, const SOUNDCOMMANDPTR &rhs )	
{ 
	// NOTE: A greater time means "less" priority
	return ( lhs->m_time > rhs->m_time );	
}

// This implements the sound controller
class CSoundControllerImp : public CSoundEnvelopeController, public CAutoGameSystemPerFrame
{
	//-----------------------------------------------------------------------------
	// internal functions, private to this file
	//-----------------------------------------------------------------------------
public:
	CSoundControllerImp( void ) : CAutoGameSystemPerFrame( "CSoundControllerImp" )
	{
	}

	void ProcessCommand( SoundCommand_t *pCmd );
	void RemoveFromList( CSoundPatch *pSound );

	virtual void	OnRestore();

	//-----------------------------------------------------------------------------
	// external interface functions (from CSoundEnvelopeController)
	//-----------------------------------------------------------------------------
public:

	// Start this sound playing, or reset if already playing with new volume/pitch
	void			Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime = 0 );
	void			CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue );
	
	void			SystemReset( void );
	void			SystemUpdate( void );
	void			CommandClear( CSoundPatch *pSound );
	void			Shutdown( CSoundPatch *pSound );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, const Vector *pSoundOrigin, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						soundlevel_t soundlevel );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es );

	void			SoundDestroy( CSoundPatch *pSound );
	void			SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime );
	void			SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime );
	void			SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout );
	float			SoundGetPitch( CSoundPatch *pSound );
	float			SoundGetVolume( CSoundPatch *pSound );
	float			SoundGetElapsedTime( CSoundPatch *pSound );
	bool			SoundIsStillPlaying( CSoundPatch *pSound );
	int				SoundGetGuid( CSoundPatch *pSound );

	string_t		SoundGetName( CSoundPatch *pSound ) { return pSound->GetName(); }
	string_t		SoundGetScriptName( CSoundPatch *pSound ) { return pSound->GetScriptName(); }
	void			SoundSetCloseCaptionDuration( CSoundPatch *pSound, float flDuration ) { pSound->SetCloseCaptionDuration(flDuration); }

	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints );
	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope );

	void			CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer );

	// Inserts the command into the list, sorted by time
	void			CommandInsert( SOUNDCOMMANDPTR pCommand );

	// CAutoClientSystem
	virtual void Update( float frametime ) 
	{
		SystemUpdate();
	}

	virtual void LevelShutdownPreEntity() 
	{
		SystemReset();
	}
	
private:
	std::vector< CSoundPatch * >			m_soundList;
	std::vector< SOUNDCOMMANDPTR >		m_commandList;	// heap by SoundCommandLessFunc (ks::heap_*)
	float				m_flLastTime;
};

// Execute a command from the list
// currently only 3 commands 
// UNDONE: Add start command?
void CSoundControllerImp::ProcessCommand( SoundCommand_t *pCmd )
{
	switch( pCmd->m_command )
	{
	case SOUNDCTRL_CHANGE_VOLUME:
		pCmd->m_pPatch->ChangeVolume( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_CHANGE_PITCH:
		pCmd->m_pPatch->ChangePitch( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_STOP:
		pCmd->m_pPatch->Shutdown();
		break;

	case SOUNDCTRL_DESTROY:
		RemoveFromList( pCmd->m_pPatch );
		delete pCmd->m_pPatch;
		pCmd->m_pPatch = nullptr;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove this sound from the sound list & shutdown (not in external interface)
// Input  : *pSound - patch to remove
//-----------------------------------------------------------------------------
void CSoundControllerImp::RemoveFromList( CSoundPatch *pSound )
{
	ks::erase_first( m_soundList, pSound );
	pSound->Shutdown();
}

//-----------------------------------------------------------------------------
// Start this sound playing, or reset if already playing with new volume/pitch
//-----------------------------------------------------------------------------
void CSoundControllerImp::Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime )
{
	// reset the vars
	pSound->Reset();

	pSound->ChangeVolume( volume, 0 );
	pSound->ChangePitch( pitch, 0 );

	if ( pSound->IsPlaying() )
	{
		// remove any previous commands in the queue
		CommandClear( pSound );
	}
	else
	{
		m_soundList.push_back( pSound );
		pSound->StartSound( flStartTime );
	}
}

//-----------------------------------------------------------------------------
// Inserts the command into the list, sorted by time
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandInsert( SOUNDCOMMANDPTR pCommand )
{
	ks::heap_push( m_commandList, std::move( pCommand ), SoundCommandLessFunc );
}

//-----------------------------------------------------------------------------
// Purpose: puts a command into the queue
// Input  : *pSound - patch this command affects
//			executeDeltaTime - relative time to execute this command
//			command - command to execute (SOUNDCTRL_*)
//			commandTime - commands have 2 parameters, a time and a value
//			value - 
// Output : 	void
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue )
{
	CommandInsert( std::make_unique< SoundCommand_t >( pSound, g_pEffects->Time() + executeDeltaTime, command, commandTime, commandValue ) );
}

// Reset the whole system (level change, etc.)
void CSoundControllerImp::SystemReset( void )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
	
		// shutdown all active sounds
		pNode->Shutdown();
	}

	// clear the list
	m_soundList.clear(); m_soundList.shrink_to_fit();

	// clear the command queue
	m_commandList.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Update the active sounds, dequeue any events and move the ramps
//-----------------------------------------------------------------------------
void CSoundControllerImp::SystemUpdate( void )
{
	CVProfScope _( "CSoundControllerImp::SystemUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	float time = g_pEffects->Time();
	float deltaTime = time - m_flLastTime;
	
	// handle clock resets
	if ( deltaTime < 0 )
		deltaTime = 0;

	m_flLastTime = time;

	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:processcommandlist", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		while ( !m_commandList.empty() )
		{
			// Commands are sorted by time.
			// process any that should occur by the current time
			if ( time >= m_commandList.front()->m_time )
			{
				SOUNDCOMMANDPTR pCmd = std::move( m_commandList.front() );
				ks::heap_pop( m_commandList, SoundCommandLessFunc );
				ProcessCommand( pCmd.get() );
			}
			else
			{
				break;
			}
		}
	}

	// NOTE: Because this loop goes from the end to the beginning
	// we can fast remove inside it without breaking the indexing
	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:removesounds", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
		{
			CSoundPatch *pNode = m_soundList[i];
			if ( !pNode->Update( time, deltaTime ) )
			{
				pNode->Reset();
				ks::swap_remove( m_soundList, i );
			}
		}
	}
}

// Remove any envelope commands from the list (dynamically changing envelope)
void CSoundControllerImp::CommandClear( CSoundPatch *pSound )
{
	for ( int i = std::ssize( m_commandList )-1; i >= 0; i-- )
	{
		if ( m_commandList[ i ]->m_pPatch == pSound )
		{
			ks::heap_erase( m_commandList, i, SoundCommandLessFunc );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: immediately stop playing this sound 
// Input  : *pSound - Patch to shut down
//-----------------------------------------------------------------------------
void CSoundControllerImp::Shutdown( CSoundPatch *pSound )
{
	if ( !pSound )
		return;

	pSound->Shutdown();
	CommandClear( pSound );
	RemoveFromList( pSound );
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), CHAN_AUTO, pSoundName, SNDLVL_NORM, nullptr );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), nullptr, scriptVolume );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, const Vector *pSoundOrigin, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), pSoundOrigin, scriptVolume );

	return pSound;
}
CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, soundlevel_t soundlevel )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, soundlevel, nullptr );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), es.m_nChannel, es.m_pSoundName, es.m_SoundLevel, es.m_pOrigin );
	pSound->ChangeVolume( es.m_flVolume, 0 );
	pSound->ChangePitch( es.m_nPitch, 0 );

	if ( es.m_nFlags & SND_SHOULDPAUSE )
	{
		pSound->SetBaseFlags( SND_SHOULDPAUSE );
	}

	return pSound;
}

void CSoundControllerImp::SoundDestroy( CSoundPatch	*pSound )
{
	if ( !pSound )
		return;

	Shutdown( pSound );
	delete pSound;
}

void CSoundControllerImp::SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime )
{
	pSound->ChangePitch( pitchTarget, deltaTime );
}

void CSoundControllerImp::SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime )
{
	pSound->ChangeVolume( volumeTarget, deltaTime );
}

int CSoundControllerImp::SoundGetGuid( CSoundPatch *pSound )
{
	return pSound->GetGuid();
}
float CSoundControllerImp::SoundGetElapsedTime( CSoundPatch *pSound )
{
	return pSound->GetElapsedTime();
}
bool CSoundControllerImp::SoundIsStillPlaying( CSoundPatch *pSound )
{
	return pSound->IsStillPlaying();
}

float CSoundControllerImp::SoundGetPitch( CSoundPatch *pSound )
{
	return pSound->GetPitch();
}

float CSoundControllerImp::SoundGetVolume( CSoundPatch *pSound )
{
	return pSound->GetVolume();
}

void CSoundControllerImp::SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout )
{
	if ( destroyOnFadeout && (deltaTime == 0.0f) )
	{
		SoundDestroy( pSound );
		return;
	}

	pSound->FadeOut( deltaTime, destroyOnFadeout );
	if ( destroyOnFadeout )
	{
		CommandAdd( pSound, deltaTime, SOUNDCTRL_DESTROY, 0.0f, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*points - List of enevelope points
//			numPoints - Number of points provided
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints )
{
	float	amplitude	= 0.0f;
	float	duration	= 0.0f;
	float	totalDuration	= 0.0f;

	contract_assert( points );

	// Clear out all previously acting commands
	CommandClear( pSound );

	// Evaluate and queue all points
	for ( int i = 0; i < numPoints; i++ )
	{
		// See if we're keeping our last amplitude for this new point
		if ( ( points[i].amplitudeMin != -1.0f ) || ( points[i].amplitudeMax != -1.0f ) )
		{
			amplitude = random_valve->RandomFloat( points[i].amplitudeMin, points[i].amplitudeMax );
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting amplitude value in envelope!  (Cannot be -1)\n" );
		}

		// See if we're keeping our last duration for this new point
		if ( ( points[i].durationMin != -1.0f ) || ( points[i].durationMax != -1.0f ) )
		{
			duration = random_valve->RandomFloat( points[i].durationMin, points[i].durationMax );	
			//duration = points[i].durationMin;
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting duration value in envelope! (Cannot be -1)\n" );
		}

		// Queue the command
		CommandAdd( pSound, totalDuration, soundCommand, duration, amplitude );

		// Tack this command's duration onto the running duration
		totalDuration += duration;
	}

	return totalDuration;
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*envelope - The envelope description to be queued
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope )
{
	return SoundPlayEnvelope( pSound, soundCommand, envelope->pPoints, envelope->nNumPoints );
}

//-----------------------------------------------------------------------------
// Purpose: Looping sounds are often started in entity spawn/activate functions. 
//			In singleplayer, the player's not ready to receive sounds then, so restart 
//			and SoundPatches that are active and have no receivers.
//-----------------------------------------------------------------------------
void CSoundControllerImp::CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		pNode->AddPlayerPost( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resumes saved soundpatches
//-----------------------------------------------------------------------------
void CSoundControllerImp::OnRestore()
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		if ( pNode && pNode->IsPlaying() )
		{
			pNode->ResumeSound();
		}
	}
}

//-----------------------------------------------------------------------------
// Singleton accessors
//-----------------------------------------------------------------------------
static CSoundControllerImp g_Controller;
CSoundEnvelopeController &CSoundEnvelopeController::GetController( void )
{
	return g_Controller;
}

class CBaseEntity;
class CBasePlayer;
class IGameSystem;
struct audioparams_t;

using namespace ::ks::reflect;

// Purpose: Soundscapes.txt resource file processor

// Only allow recursive references to be 8 levels deep.
// This test will flag any circular references and bail.
static constexpr auto MAX_SOUNDSCAPE_RECURSION = 8;

static const float DEFAULT_SOUND_RADIUS = 36.0f;
// Keep an array of all looping sounds so they can be faded in/out
// OPTIMIZE: Get a handle/pointer to the engine's sound channel instead 
//			of searching each frame!
enum soundfadestyle_t
{
	FADE_VOLUME_LINEAR = 0,
	FADE_VOLUME_SINE = 1,
};

// contains a set of data to implement a simple envelope to fade in/out sounds
struct soundfader_t
{
	float m_flCurrent;
	float m_flTarget;
	float m_flRate;
	float m_flStart;
	float m_flFadeT;
	int	m_nType;

	bool IsFading()
	{
		return ( m_flCurrent != m_flTarget ) ? true : false;
	}

	void FadeToValue( float flTarget, float flRate, soundfadestyle_t fadeType )
	{
		m_flStart = m_flCurrent;
		m_flFadeT = 0;
		m_flTarget = flTarget;
		m_flRate = flRate;
		m_nType = fadeType;
	}

	void ForceToTargetValue( float flTarget )
	{
		m_flFadeT = 1.0f;
		m_flCurrent = m_flTarget = flTarget;
		m_flRate = 0;
	}

	void UpdateFade( float flDt )
	{
		m_flFadeT += flDt * m_flRate;
		if ( m_flFadeT >= 1.0f )
		{
			ForceToTargetValue( m_flTarget );
			return;
		}
		float flFactor = m_flFadeT;
		float flDelta = m_flTarget - m_flStart;
		switch ( m_nType )
		{
		case FADE_VOLUME_LINEAR:
			break;
		case FADE_VOLUME_SINE:
			if ( flDelta >= 0 )
			{
				flFactor = std::sin( m_flFadeT * std::numbers::pi * 0.5f );
			}
			else
			{
				flFactor = 1.0f - std::cos( m_flFadeT * std::numbers::pi * 0.5f );
			}
			break;
		}
		m_flCurrent = m_flStart + flDelta * flFactor;
	}
};

struct loopingsound_t
{
	Vector		position;		// position (if !isAmbient)
	const char *pWaveName;		// name of the wave file
	soundfader_t m_volume;
	soundlevel_t soundlevel;	// sound level (if !isAmbient)
	int			pitch;			// pitch shift
	int			id;				// Used to fade out sounds that don't belong to the most current setting
	int			engineGuid;
	float		radius;			// if set, sound plays at full volume inside the radius and fallsoff as you move out of the radius.  Sound will lose directionality as you move inside the radius
	bool		isAmbient;		// Ambient sounds have no spatialization - they play from everywhere
};

ConVar soundscape_fadetime( side_registry(), "soundscape_fadetime", "3.0", FCVAR_CHEAT, "Time to crossfade sound effects between soundscapes" );
ConVar soundscape_message(side_registry(), "soundscape_message","0");
ConVar soundscape_radius_debug( side_registry(), "soundscape_radius_debug", "0", FCVAR_CHEAT, "Prints current volume of radius sounds" );

float GetSoundscapeFadeRate()
{
	float flFadeTime = soundscape_fadetime.GetFloat();
	float flFadeRate = 1.0f / (flFadeTime > 0 ? flFadeTime : 3.0f);

	return flFadeRate;
}

struct randomsound_t
{
	Vector		position;
	float		nextPlayTime;	// time to play a sound from the set
	interval_t	time;
	interval_t	volume;
	interval_t	pitch;
	interval_t	soundlevel;
	float		masterVolume;
	int			waveCount;
	bool		isAmbient;
	bool		isRandom;
	KeyValues	*pWaves;

	void Init()
	{
		memset( this, 0, sizeof(*this) );
	}
};

struct subsoundscapeparams_t
{
	Vector  vForcedTextOriginAmbient;
	int		recurseLevel;		// test for infinite loops in the script / circular refs
	float	masterVolume;
	float	flFadeRate;
	int		startingPosition;
	int		positionOverride;	// forces all sounds to this position
	int		ambientPositionOverride;	// forces all ambient sounds to this position
	bool	allowDSP;
	bool	wroteSoundMixer;
	bool	wroteDSPVolume;
	bool	bForceTextOriginAmbient;
};

Vector getVectorFromString(const char *pString)
{
	char tempString[128];
	V_strncpy( tempString, pString, sizeof(tempString) );

	Vector result;
	int i = 0;
	char *token = strtok( tempString, "," );
	while( token )
	{
		result[i] = atof( token );
		token = strtok( nullptr, "," );
		i++;
	}
	return result;
}
class CSoundscapeSystem : public CBaseGameSystemPerFrame
{
public:
	virtual char const *Name() { return "C_SoundScapeSystem"; }

	CSoundscapeSystem()
	{
		m_nRestoreFrame = -1;
	}

	~CSoundscapeSystem() {}

	void OnStopAllSounds()
	{
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetPerUser(hh).m_params.entIndex = 0;
			GetPerUser(hh).m_params.soundscapeIndex = -1;
			GetPerUser(hh).m_loopingSounds.clear();
			GetPerUser(hh).m_randomSounds.clear();
		}
	}

	// IClientSystem hooks, not needed
	virtual void LevelInitPreEntity()
	{
		Shutdown();
		Init();

		TouchSoundFiles();
	}

	virtual void LevelInitPostEntity() 
	{
		if ( !m_pSoundMixerVar )
		{
			m_pSoundMixerVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
		}
		if ( !m_pDSPVolumeVar )
		{
			m_pDSPVolumeVar = (ConVar *)cvar->FindVar( "dsp_volume" );
		}
	}

	// The level is shutdown in two parts
	virtual void LevelShutdownPreEntity() {}
	// Entities are deleted / released here...
	virtual void LevelShutdownPostEntity()
	{
		OnStopAllSounds();
	}

	virtual void OnSave() {}
	virtual void OnRestore()
	{
		m_nRestoreFrame = gpGlobals->framecount;
	}
	virtual void SafeRemoveIfDesired() {}

	// Called before rendering
	virtual void PreRender() { }

	// Called after rendering
	virtual void PostRender() { }

	// IClientSystem hooks used
	virtual bool Init();
	virtual void Shutdown();
	// Gets called each frame
	virtual void Update( float frametime );

	void PrintDebugInfo()
	{
		Msg( "\n------- CLIENT SOUNDSCAPES -------\n" );
		for ( int i=0; i < std::ssize( m_soundscapes ); i++ )
		{
			Msg( "- %d: %s\n", i, m_soundscapes[i]->GetName() );
		}

		Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

		if ( slot.m_forcedSoundscapeIndex )
		{
			Msg( "- PLAYING DEBUG SOUNDSCAPE: %d [%s]\n", slot.m_forcedSoundscapeIndex, SoundscapeNameByIndex(slot.m_forcedSoundscapeIndex) );
		}
		Msg( "- CURRENT SOUNDSCAPE: %d [%s]\n", slot.m_params.soundscapeIndex.Get(), SoundscapeNameByIndex(slot.m_params.soundscapeIndex) );
		Msg( "----------------------------------\n\n" );
	}

	// local functions
	void UpdateAudioParams( audioparams_t &audio );
	void GetAudioParams( audioparams_t &out ) const { out = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT()).m_params; }
	int GetCurrentSoundscape() 
	{ 
		Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

		if ( slot.m_forcedSoundscapeIndex >= 0 )
			return slot.m_forcedSoundscapeIndex;
		return slot.m_params.soundscapeIndex; 
	}
	void DevReportSoundscapeName( int index );
	void UpdateLoopingSounds( float frametime );
	int AddLoopingAmbient( const char *pSoundName, float volume, int pitch, float radius, float flFadeRate );
	void UpdateLoopingSound( loopingsound_t &loopSound );
	void StopLoopingSound( loopingsound_t &loopSound );
	int AddLoopingSound( const char *pSoundName, bool isAmbient, float volume, 
		soundlevel_t soundLevel, int pitch, const Vector &position, float radius, float flFadeRate );
	int AddRandomSound( const randomsound_t &sound );
	void PlayRandomSound( randomsound_t &sound );
	void UpdateRandomSounds( float gameClock );
	Vector GenerateRandomSoundPosition();

	void ForceSoundscape( const char *pSoundscapeName, float radius );

	int FindSoundscapeByName( const char *pSoundscapeName );
	const char *SoundscapeNameByIndex( int index );
	KeyValues *SoundscapeByIndex( int index );

	// main-level soundscape processing, called on new soundscape
	void StartNewSoundscape( KeyValues *pSoundscape );
	void StartSubSoundscape( KeyValues *pSoundscape, subsoundscapeparams_t &params );

	// root level soundscape keys
	// add a process for each new command here
	// "dsp"
	void ProcessDSP( KeyValues *pDSP );
	// "dsp_player"
	void ProcessDSPPlayer( KeyValues *pDSPPlayer );
	// "fadetime"
	void ProcessSoundscapeFadetime( KeyValues *pKey, subsoundscapeparams_t &params );
	// "playlooping"
	void ProcessPlayLooping( KeyValues *pPlayLooping, const subsoundscapeparams_t &params );	
	// "playrandom"
	void ProcessPlayRandom( KeyValues *pPlayRandom, const subsoundscapeparams_t &params );
	// "playsoundscape"
	void ProcessPlaySoundscape( KeyValues *pPlaySoundscape, subsoundscapeparams_t &params );
	// "soundmixer"
	void ProcessSoundMixer( KeyValues *pSoundMixer, subsoundscapeparams_t &params );
	// "dsp_volume"
	void ProcessDSPVolume( KeyValues *pKey, subsoundscapeparams_t &params );

private:

	bool	IsBeingRestored() const
	{
		return gpGlobals->framecount == m_nRestoreFrame ? true : false;
	}

	void	AddSoundScapeFile( const char *filename );

	void		TouchPlayLooping( KeyValues *pAmbient );
	void		TouchPlayRandom( KeyValues *pPlayRandom );
	void		TouchWaveFiles( KeyValues *pSoundScape );
	void		TouchSoundFile( char const *wavefile );

	void		TouchSoundFiles();

	int							m_nRestoreFrame;

	std::vector< KeyValues * >	m_SoundscapeScripts;	// The whole script file in memory
	std::vector< KeyValues * >		m_soundscapes;			// Lookup by index of each root section
	struct Split_t
	{
		audioparams_t				m_params;				// current player audio params
		std::vector< loopingsound_t >	m_loopingSounds;		// list of currently playing sounds
		std::vector< randomsound_t >	m_randomSounds;			// list of random sound commands
		float						m_nextRandomTime;		// next time to play a random sound
		int							m_loopingSoundId;		// marks when the sound was issued
		int							m_forcedSoundscapeIndex;// >= 0 if this a "forced" soundscape? i.e. debug mode?
		float						m_forcedSoundscapeRadius;// distance to spatialized sounds
	};

	Split_t						m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];

	Split_t						&GetPerUser( int nSlot )
	{
		return m_PerUser[ nSlot ];
	}

	const Split_t						&GetPerUser( int nSlot ) const
	{
		return m_PerUser[ nSlot ];
	}

	static ConVar *m_pDSPVolumeVar;
	static ConVar *m_pSoundMixerVar;

};

// singleton system
CSoundscapeSystem g_SoundscapeSystem;
ConVar *CSoundscapeSystem::m_pDSPVolumeVar = nullptr;
ConVar *CSoundscapeSystem::m_pSoundMixerVar = nullptr;

IGameSystem *ClientSoundscapeSystem()
{
	return &g_SoundscapeSystem;
}

CSoundscapeSystem *GetClientSoundscapeSystem()
{
	return &g_SoundscapeSystem;
}

void Soundscape_OnStopAllSounds()
{
	GetClientSoundscapeSystem()->OnStopAllSounds();
}

// player got a network update
void Soundscape_Update( audioparams_t &audio )
{
	GetClientSoundscapeSystem()->UpdateAudioParams( audio );
}

#define SOUNDSCAPE_MANIFEST_FILE				"scripts/soundscapes_manifest.txt"

void CSoundscapeSystem::AddSoundScapeFile( const char *filename )
{
	KeyValues *script = new KeyValues( filename );
	if ( filesystem->LoadKeyValues( *script, IFileSystem::TYPE_SOUNDSCAPE, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = script;
		while ( pKeys )
		{
			// save pointers to all sections in the root
			// each one is a soundscape
			if ( pKeys->GetFirstSubKey() )
			{
				m_soundscapes.push_back( pKeys );
			}
			pKeys = pKeys->GetNextKey();
		}

		// Keep pointer around so we can delete it at exit
		m_SoundscapeScripts.push_back( script );
	}
	else
	{
		script->deleteThis();
	}
}

// parse the script file, setup index table
bool CSoundscapeSystem::Init()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetPerUser(hh).m_loopingSoundId = 0;
	}

	const char *mapname = MapName();
	const char *mapSoundscapeFilename = nullptr;
	if ( mapname && *mapname )
	{
		mapSoundscapeFilename = VarArgs( "scripts/soundscapes_%s.txt", mapname );
	}

	KeyValues *manifest = new KeyValues( SOUNDSCAPE_MANIFEST_FILE );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDSCAPE, SOUNDSCAPE_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSoundScapeFile( sub->GetString() );
				if ( mapSoundscapeFilename && FStrEq( sub->GetString(), mapSoundscapeFilename ) )
				{
					mapSoundscapeFilename = nullptr; // we've already loaded the map's soundscape
				}
				continue;
			}

			Warning( "CSoundscapeSystem::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SOUNDSCAPE_MANIFEST_FILE, sub->GetName() );
		}

		if ( mapSoundscapeFilename && filesystem->FileExists( mapSoundscapeFilename ) )
		{
			AddSoundScapeFile( mapSoundscapeFilename );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SOUNDSCAPE_MANIFEST_FILE );
	}

	manifest->deleteThis();

	return true;
}

int CSoundscapeSystem::FindSoundscapeByName( const char *pSoundscapeName )
{
	// UNDONE: Bad perf, linear search!
	for ( int i = std::ssize( m_soundscapes )-1; i >= 0; --i )
	{
		if ( !V_stricmp( m_soundscapes[i]->GetName(), pSoundscapeName ) )
			return i;
	}

	return -1;
}

KeyValues *CSoundscapeSystem::SoundscapeByIndex( int index )
{
	if ( ks::is_valid_index( m_soundscapes, index ) )
		return m_soundscapes[index];
	return nullptr;
}

const char *CSoundscapeSystem::SoundscapeNameByIndex( int index )
{
	if ( index < std::ssize( m_soundscapes ) )
	{
		return m_soundscapes[index]->GetName();
	}

	return nullptr;
}

void CSoundscapeSystem::Shutdown()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		for ( int i = std::ssize( GetPerUser(hh).m_loopingSounds ) - 1; i >= 0; --i )
		{
			loopingsound_t &sound = GetPerUser(hh).m_loopingSounds[i];

			// sound is done, remove from list.
			StopLoopingSound( sound );
		}

		// These are only necessary so we can use shutdown/init calls
		// to flush soundscape data
		GetPerUser(hh).m_loopingSounds.clear();
		GetPerUser(hh).m_randomSounds.clear();
		GetPerUser(hh).m_params.entIndex = 0;
		GetPerUser(hh).m_params.soundscapeIndex = -1;
	}

	m_soundscapes.clear();

	while ( !m_SoundscapeScripts.empty() )
	{
		KeyValues *kv = m_SoundscapeScripts[ 0 ];
		m_SoundscapeScripts.erase( m_SoundscapeScripts.begin() + 0 );
		kv->deleteThis();
	}
}

// NOTE: This will not flush the server side so you cannot add or remove
// soundscapes from the list, only change their parameters!!!!
static void cl_soundscape_flush( const CCommand &args );
static ConCommand cl_soundscape_flush_command( side_registry(), "cl_soundscape_flush", cl_soundscape_flush, "Flushes the client side soundscapes", FCVAR_SERVER_CAN_EXECUTE|FCVAR_CHEAT );
static void cl_soundscape_flush( const CCommand &args )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		// save the current soundscape
		audioparams_t tmp;
		GetClientSoundscapeSystem()->GetAudioParams( tmp );

		// kill the system
		GetClientSoundscapeSystem()->Shutdown();

		// restart the system
		GetClientSoundscapeSystem()->Init();

		// reload the soundscape params from the temp copy
		Soundscape_Update( tmp );
	}
}

static int SoundscapeCompletion( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	// Autocomplete can just look at the base system
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	int current = 0;

	const char *cmdname = "playsoundscape";
	char *substring = nullptr;
	int substringLen = 0;
	if ( V_strstr( partial, cmdname ) && strlen(partial) > strlen(cmdname) + 1 )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
		substringLen = strlen(substring);
	}

	int i = 0;
	const char *pSoundscapeName = GetClientSoundscapeSystem()->SoundscapeNameByIndex( i );
	while ( pSoundscapeName && current < COMMAND_COMPLETION_MAXITEMS )
	{
		if ( !substring || !V_strncasecmp( pSoundscapeName, substring, substringLen ) )
		{
			V_snprintf( commands[ current ], sizeof( commands[ current ] ), "%s %s", cmdname, pSoundscapeName );
			current++;
		}
		i++;
		pSoundscapeName = GetClientSoundscapeSystem()->SoundscapeNameByIndex( i );
	}

	return current;
}

static void playsoundscape( const CCommand &args );
static ConCommand playsoundscape_command( side_registry(), "playsoundscape", playsoundscape, "Forces a soundscape to play", FCVAR_CHEAT, SoundscapeCompletion );
static void playsoundscape( const CCommand &args )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		if ( args.ArgC() < 2 )
		{
			GetClientSoundscapeSystem()->DevReportSoundscapeName( GetClientSoundscapeSystem()->GetCurrentSoundscape() );
			continue;
		}
		const char *pSoundscapeName = args[1];
		float radius = args.ArgC() > 2 ? atof( args[2] ) : DEFAULT_SOUND_RADIUS;
		GetClientSoundscapeSystem()->ForceSoundscape( pSoundscapeName, radius );
	}
}

static void stopsoundscape( const CCommand &args );
static ConCommand stopsoundscape_command( side_registry(), "stopsoundscape", stopsoundscape, "Stops all soundscape processing and fades current looping sounds", FCVAR_CHEAT );
static void stopsoundscape( const CCommand &args )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetClientSoundscapeSystem()->StartNewSoundscape( nullptr );
	}
}

void CSoundscapeSystem::ForceSoundscape( const char *pSoundscapeName, float radius )
{
	int index = FindSoundscapeByName( pSoundscapeName );
	if ( index >= 0 )
	{
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		GetPerUser(nSlot).m_forcedSoundscapeIndex = index;
		GetPerUser(nSlot).m_forcedSoundscapeRadius = radius;
		StartNewSoundscape( SoundscapeByIndex(index) );
	}
	else
	{
		DevWarning("Can't find soundscape %s\n", pSoundscapeName );
	}
}

void CSoundscapeSystem::DevReportSoundscapeName( int index )
{
	const char *pName = "none";
	if ( index >= 0 && index < std::ssize( m_soundscapes ) )
	{
		pName = m_soundscapes[index]->GetName();
	}

	if ( soundscape_message.GetBool() )
	{
		Msg( "Soundscape[%d]: %s\n", GET_ACTIVE_SPLITSCREEN_SLOT(), pName  );
	}
}

// This makes all currently playing loops fade toward their target volume
void CSoundscapeSystem::UpdateLoopingSounds( float frametime )
{
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());
	int fadeCount = std::ssize( slot.m_loopingSounds );
	while ( fadeCount > 0 )
	{
		fadeCount--;
		loopingsound_t &sound = slot.m_loopingSounds[fadeCount];

		bool bUpdateSound = sound.m_volume.IsFading();

		// for radius looping sounds, volume is manually set based on listener's distance
		if ( sound.radius > 0 )
		{
			CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
			if ( pPlayer )
			{
				CBaseEntity *pEnt = pPlayer->GetSoundscapeListener();
				if ( pEnt )
				{
					float distance = pEnt->GetAbsOrigin().DistTo( sound.position );

					if ( distance > sound.radius * 100.0f )
					{
						// long way away, let sound fade to silence
						sound.m_volume.FadeToValue( 0.01f, 1.0f, FADE_VOLUME_LINEAR );	// HACK: Don't set sound to zero volume else it'll be removed and never started again!
					}
					else
					{
						float flTarget = 1.0f;
						// inside the radius, full volume, outside fade out
						if ( distance >= sound.radius )
						{
							flTarget = 1.0f / ( 1 + 0.5f * ( distance - sound.radius ) / sound.radius );
						}
						sound.m_volume.ForceToTargetValue( flTarget );
					}

					if ( soundscape_radius_debug.GetBool() )
					{
						DevMsg( 1, "Updated looping radius sound %d to vol=%f\n", fadeCount, sound.m_volume.m_flTarget );
					}

					bUpdateSound = true;
				}
			}
		}

		if ( bUpdateSound )
		{
			sound.m_volume.UpdateFade( frametime );
			if ( sound.m_volume.m_flTarget == 0 && sound.m_volume.m_flCurrent == 0 )
			{
				// sound is done, remove from list.
				StopLoopingSound( sound );
				ks::swap_remove( slot.m_loopingSounds, fadeCount );
			}
			else
			{
				// tell the engine about the new volume
				UpdateLoopingSound( sound );
			}
		}
	}
}

void CSoundscapeSystem::Update( float frametime ) 
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		if ( GetPerUser(hh).m_forcedSoundscapeIndex >= 0 )
		{
			// generate fake positional sources
			CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
			if ( pPlayer )
			{
				Vector origin, forward, right;
				pPlayer->EyePositionAndVectors( &origin, &forward, &right, nullptr );

				// put the sound origins at the corners of a box around the player
				GetPerUser(hh).m_params.localSound.Set( 0, origin + GetPerUser(hh).m_forcedSoundscapeRadius * (forward-right) );
				GetPerUser(hh).m_params.localSound.Set( 1, origin + GetPerUser(hh).m_forcedSoundscapeRadius * (forward+right) );
				GetPerUser(hh).m_params.localSound.Set( 2, origin + GetPerUser(hh).m_forcedSoundscapeRadius * (-forward-right) );
				GetPerUser(hh).m_params.localSound.Set( 3, origin + GetPerUser(hh).m_forcedSoundscapeRadius * (-forward+right) );
				GetPerUser(hh).m_params.localBits = 0x0007;
			}
		}
		// fade out the old sounds over soundscape_fadetime seconds
		UpdateLoopingSounds( frametime );
		UpdateRandomSounds( gpGlobals->curtime );
	}
}

void CSoundscapeSystem::UpdateAudioParams( audioparams_t &audio )
{
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());
	if ( slot.m_params.soundscapeIndex == audio.soundscapeIndex && slot.m_params.entIndex == audio.entIndex )
		return;

	slot.m_params = audio;
	slot.m_forcedSoundscapeIndex = -1;
	if ( audio.entIndex > 0 && audio.soundscapeIndex >= 0 && audio.soundscapeIndex < std::ssize( m_soundscapes ) )
	{
		DevReportSoundscapeName( audio.soundscapeIndex );
		StartNewSoundscape( m_soundscapes[audio.soundscapeIndex] );
	}
	else
	{
		// bad index (and the soundscape file actually existed...)
		if ( audio.entIndex > 0 &&
			audio.soundscapeIndex != -1 )
		{
			DevMsg(1, "Error: Bad soundscape!\n");
		}
	}
}

// Called when a soundscape is activated (leading edge of becoming the active soundscape)
void CSoundscapeSystem::StartNewSoundscape( KeyValues *pSoundscape )
{
	int i;

	float flFadeRate = GetSoundscapeFadeRate();
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

	// Reset the system
	// fade out the current loops
	// save off the count of old looping sounds
	int nOldLoopingSoundMax = std::ssize( slot.m_loopingSounds )-1;
	for ( i = std::ssize( slot.m_loopingSounds )-1; i >= 0; --i )
	{
		slot.m_loopingSounds[i].m_volume.FadeToValue( 0, flFadeRate, FADE_VOLUME_SINE );
		if ( !pSoundscape )
		{
			// if we're cancelling the soundscape, stop the sound immediately
			slot.m_loopingSounds[i].m_volume.ForceToTargetValue( 0 );
		}
	}
	// update ID
	slot.m_loopingSoundId++;

	// clear all random sounds
	slot.m_randomSounds.clear();
	slot.m_nextRandomTime = gpGlobals->curtime;

	if ( pSoundscape )
	{
		subsoundscapeparams_t params;
		params.allowDSP = true;
		params.wroteSoundMixer = false;
		params.wroteDSPVolume = false;

		params.masterVolume = 1.0;
		params.startingPosition = 0;
		params.recurseLevel = 0;
		params.positionOverride = -1;
		params.ambientPositionOverride = -1;
		params.flFadeRate = flFadeRate;
		params.bForceTextOriginAmbient = false;
		params.vForcedTextOriginAmbient.Init();

		StartSubSoundscape( pSoundscape, params );

		if ( !params.wroteDSPVolume )
		{
			m_pDSPVolumeVar->Revert();
		}
		if ( !params.wroteSoundMixer )
		{
			m_pSoundMixerVar->Revert();
		}
		// if we processed a fade rate, update the fade
		// This is a little bit of a hack but since we don't pre-parse soundscapes
		// into structs we can't know if there is a rate change on this soundscape
		if ( params.flFadeRate != flFadeRate )
		{
			for ( i = nOldLoopingSoundMax; i >= 0; --i )
			{
				// if we're still fading out at the old rate, fade at the new rate
				if ( slot.m_loopingSounds[i].m_volume.m_flTarget == 0.0f && slot.m_loopingSounds[i].m_volume.m_flRate == flFadeRate )
				{
					slot.m_loopingSounds[i].m_volume.m_flRate = params.flFadeRate;
				}
			}
		}
	}
}

void CSoundscapeSystem::StartSubSoundscape( KeyValues *pSoundscape, subsoundscapeparams_t &params )
{
	// Parse/process all of the commands
	KeyValues *pKey = pSoundscape->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "dsp" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSP( pKey );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "dsp_player" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSPPlayer( pKey );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "fadetime" ) )
		{
			// don't allow setting these recursively since they are order dependent
			if ( params.recurseLevel < 1 )
			{
				ProcessSoundscapeFadetime( pKey, params );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "playlooping" ) )
		{
			ProcessPlayLooping( pKey, params );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "playrandom" ) )
		{
			ProcessPlayRandom( pKey, params );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "playsoundscape" ) )
		{
			ProcessPlaySoundscape( pKey, params );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "Soundmixer" ) )
		{
			if ( params.allowDSP )
			{
				ProcessSoundMixer( pKey, params );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "dsp_volume" ) )
		{
			if ( params.allowDSP )
			{
				ProcessDSPVolume( pKey, params );
			}
		}
		// add new commands here
		else
		{
			DevMsg( 1, "Soundscape %s:Unknown command %s\n", pSoundscape->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}
}

// add a process for each new command here

// change DSP effect
void CSoundscapeSystem::ProcessDSP( KeyValues *pDSP )
{
	int roomType = pDSP->GetInt();
	CLocalPlayerFilter filter;
	enginesound->SetRoomType( filter, roomType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDSPPlayer - 
//-----------------------------------------------------------------------------
void CSoundscapeSystem::ProcessDSPPlayer( KeyValues *pDSPPlayer )
{
	int dspType = pDSPPlayer->GetInt();
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, dspType, false );
}

void CSoundscapeSystem::ProcessSoundMixer( KeyValues *pSoundMixer, subsoundscapeparams_t &params )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer || pPlayer->CanSetSoundMixer() )
	{
		m_pSoundMixerVar->SetValue( pSoundMixer->GetString() );
		params.wroteSoundMixer = true;
	}
}

void CSoundscapeSystem::ProcessDSPVolume( KeyValues *pKey, subsoundscapeparams_t &params )
{
	m_pDSPVolumeVar->SetValue( pKey->GetFloat() );
	params.wroteDSPVolume = true;
}

void CSoundscapeSystem::ProcessSoundscapeFadetime( KeyValues *pKey, subsoundscapeparams_t &params )
{
	float flFadeTime = pKey->GetFloat();
	if ( flFadeTime > 0.0f )
	{
		params.flFadeRate = 1.0f / flFadeTime;
	}
}

// start a new looping sound
void CSoundscapeSystem::ProcessPlayLooping( KeyValues *pAmbient, const subsoundscapeparams_t &params )
{
	float volume = 0;
	soundlevel_t soundlevel = ATTN_TO_SNDLVL(ATTN_NORM);
	const char *pSoundName = nullptr;
	int pitch = PITCH_NORM;
	int positionIndex = -1;
	bool randomPosition = false;
	bool suppress = false;
	bool useTextOrigin = false;
	Vector textOrigin;
	float radius = 0;

	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

	KeyValues *pKey = pAmbient->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "volume" ) )
		{
			volume = params.masterVolume * RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			pitch = RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "wave" ) )
		{
			pSoundName = pKey->GetString();
		}
		else if ( !V_strcasecmp( pKey->GetName(), "origin" ) )
		{
			textOrigin = getVectorFromString(pKey->GetString());
			useTextOrigin = true;
		}
		else if ( !V_strcasecmp( pKey->GetName(), "position" ) )
		{
			if ( !V_strcasecmp( pKey->GetString(), "random" ) )
			{
				randomPosition = true;
			}
			else
			{
				positionIndex = params.startingPosition + pKey->GetInt();
			}
		//	positionIndex = params.startingPosition + pKey->GetInt();
		}
		else if ( !V_strcasecmp( pKey->GetName(), "attenuation" ) )
		{
			soundlevel = ATTN_TO_SNDLVL( RandomInterval( ReadInterval( pKey->GetString() ) ) );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "soundlevel" ) )
		{
			if ( !V_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				soundlevel = TextToSoundLevel( pKey->GetString() );
			}
			else
			{
				soundlevel = (soundlevel_t)((int)RandomInterval( ReadInterval( pKey->GetString() ) ));
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "suppress_on_restore" ) )
		{
			suppress = V_atoi( pKey->GetString() ) != 0 ? true : false;
		}
		else if ( !V_strcasecmp( pKey->GetName(), "radius" ) )
		{
			radius = (float) atof( pKey->GetString() );
		}
		else
		{
			DevMsg( 1, "Ambient %s:Unknown command %s\n", pAmbient->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}

	if ( positionIndex < 0 )
	{
		positionIndex = params.ambientPositionOverride;
	}
	else if ( params.positionOverride >= 0 )
	{
		positionIndex = params.positionOverride;
	}
	if ( params.bForceTextOriginAmbient && positionIndex < 0 )
	{
		useTextOrigin = true;
		textOrigin = params.vForcedTextOriginAmbient;
	}

	// Sound is mared as "suppress_on_restore" so don't restart it
	if ( IsBeingRestored() && suppress )
	{
		return;
	}

	if ( volume != 0 && pSoundName != nullptr )
	{
		if ( randomPosition )
		{
			AddLoopingSound( pSoundName, false, volume, soundlevel, pitch, GenerateRandomSoundPosition(), radius, params.flFadeRate );
		}
		else if ( useTextOrigin )
		{
			AddLoopingSound( pSoundName, false, volume, soundlevel, pitch, textOrigin, radius, params.flFadeRate );
		}
		else if ( positionIndex < 0 )
		{
			AddLoopingAmbient( pSoundName, volume, pitch, radius, params.flFadeRate );
		}
		else
		{
			if ( positionIndex > 31 || !(slot.m_params.localBits & (1<<positionIndex) ) )
			{
				// suppress sounds if the position isn't available
				//DevMsg( 1, "Bad position %d\n", positionIndex );
				return;
			}
			AddLoopingSound( pSoundName, false, volume, soundlevel, pitch, slot.m_params.localSound[positionIndex], radius, params.flFadeRate );
		}
	}
}

void CSoundscapeSystem::TouchSoundFile( char const *wavefile )
{
	filesystem->GetFileTime( VarArgs( "sound/%s", PSkipSoundChars( wavefile ) ), "GAME" );
}

// start a new looping sound
void CSoundscapeSystem::TouchPlayLooping( KeyValues *pAmbient )
{
	KeyValues *pKey = pAmbient->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "wave" ) )
		{
			char const *pSoundName = pKey->GetString();

			// Touch the file
			TouchSoundFile( pSoundName );
		}

		pKey = pKey->GetNextKey();
	}
}

Vector CSoundscapeSystem::GenerateRandomSoundPosition()
{
	float angle = random_valve->RandomFloat( -180, 180 );
	float sinAngle, cosAngle;
	SinCos( angle, &sinAngle, &cosAngle );
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		Vector origin, forward, right;
		pPlayer->EyePositionAndVectors( &origin, &forward, &right, nullptr );
		return origin + DEFAULT_SOUND_RADIUS * (cosAngle * right + sinAngle * forward);
	}
	else
	{
		return CurrentViewOrigin() + DEFAULT_SOUND_RADIUS * (cosAngle * CurrentViewRight() + sinAngle * CurrentViewForward());
	}
}

void CSoundscapeSystem::TouchSoundFiles()
{
	if ( !CommandLine()->FindParm( "-makereslists" ) )
		return;

	int c = std::ssize( m_soundscapes );
	for ( int i = 0; i < c ; ++i )
	{
		TouchWaveFiles( m_soundscapes[ i ] );
	}
}

void CSoundscapeSystem::TouchWaveFiles( KeyValues *pSoundScape )
{
	KeyValues *pKey = pSoundScape->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "playlooping" ) )
		{
			TouchPlayLooping( pKey );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "playrandom" ) )
		{
			TouchPlayRandom( pKey );
		}

		pKey = pKey->GetNextKey();
	}

}

// puts a recurring random sound event into the queue
void CSoundscapeSystem::TouchPlayRandom( KeyValues *pPlayRandom )
{
	KeyValues *pKey = pPlayRandom->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			while ( pWaves )
			{
				TouchSoundFile( pWaves->GetString() );

				pWaves = pWaves->GetNextKey();
			}
		}

		pKey = pKey->GetNextKey();
	}
}

// puts a recurring random sound event into the queue
void CSoundscapeSystem::ProcessPlayRandom( KeyValues *pPlayRandom, const subsoundscapeparams_t &params )
{
	randomsound_t sound;
	sound.Init();
	sound.masterVolume = params.masterVolume;
	int positionIndex = -1;
	bool suppress = false;
	bool randomPosition = false;
	bool useTextOrigin = false;
	Vector textOrigin;
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

	KeyValues *pKey = pPlayRandom->GetFirstSubKey();
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "volume" ) )
		{
			sound.volume = ReadInterval( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			sound.pitch = ReadInterval( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "attenuation" ) )
		{
			interval_t atten = ReadInterval( pKey->GetString() );
			sound.soundlevel.start = ATTN_TO_SNDLVL( atten.start );
			sound.soundlevel.range = ATTN_TO_SNDLVL( atten.start + atten.range ) - sound.soundlevel.start;
		}
		else if ( !V_strcasecmp( pKey->GetName(), "soundlevel" ) )
		{
			if ( !V_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				sound.soundlevel.start = TextToSoundLevel( pKey->GetString() );
				sound.soundlevel.range = 0;
			}
			else
			{
				sound.soundlevel = ReadInterval( pKey->GetString() );
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "time" ) )
		{
			sound.time = ReadInterval( pKey->GetString() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			sound.pWaves = pWaves;
			sound.waveCount = 0;
			while ( pWaves )
			{
				sound.waveCount++;
				pWaves = pWaves->GetNextKey();
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "position" ) )
		{
			if ( !V_strcasecmp( pKey->GetString(), "random" ) )
			{
				randomPosition = true;
			}
			else
			{
				positionIndex = params.startingPosition + pKey->GetInt();
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "origin" ) )
		{
			const char *originString = pKey->GetString();
			textOrigin = getVectorFromString(originString);	
			useTextOrigin = true;
		}
		else if ( !V_strcasecmp( pKey->GetName(), "suppress_on_restore" ) )
		{
			suppress = V_atoi( pKey->GetString() ) != 0 ? true : false;
		}
		else
		{
			DevMsg( 1, "Random Sound %s:Unknown command %s\n", pPlayRandom->GetName(), pKey->GetName() );
		}

		pKey = pKey->GetNextKey();
	}

	if ( positionIndex < 0 )
	{
		positionIndex = params.ambientPositionOverride;
	}
	else if ( params.positionOverride >= 0 )
	{
		positionIndex = params.positionOverride;
		randomPosition = false; // override trumps random position
	}
	if ( params.bForceTextOriginAmbient && positionIndex < 0 )
	{
		useTextOrigin = true;
		textOrigin = params.vForcedTextOriginAmbient;
		randomPosition = false;
	}

	// Sound is mared as "suppress_on_restore" so don't restart it
	if ( IsBeingRestored() && suppress )
	{
		return;
	}

	if ( sound.waveCount != 0 )
	{
		if ( positionIndex < 0 && !randomPosition && !useTextOrigin )
		{
			sound.isAmbient = true;
			AddRandomSound( sound );
		}
		else
		{
			sound.isAmbient = false;
			if ( randomPosition )
			{
				sound.isRandom = true;
			}
			else if ( useTextOrigin )
			{
				sound.position = textOrigin;
			}
			else
			{
				if ( positionIndex > 31 || !(slot.m_params.localBits & (1<<positionIndex) ) )
				{
					// suppress sounds if the position isn't available
					//DevMsg( 1, "Bad position %d\n", positionIndex );
					return;
				}
				sound.position = slot.m_params.localSound[positionIndex];
			}
			AddRandomSound( sound );
		}
	}
}

void CSoundscapeSystem::ProcessPlaySoundscape( KeyValues *pPlaySoundscape, subsoundscapeparams_t &paramsIn )
{
	subsoundscapeparams_t subParams = paramsIn;

	// sub-soundscapes NEVER set the DSP effects
	subParams.allowDSP = false;
	subParams.recurseLevel++;
	if ( subParams.recurseLevel > MAX_SOUNDSCAPE_RECURSION )
	{
		DevMsg( "Error!  Soundscape recursion overrun!\n" );
		return;
	}
	KeyValues *pKey = pPlaySoundscape->GetFirstSubKey();
	const char *pSoundscapeName = nullptr;
	while ( pKey )
	{
		if ( !V_strcasecmp( pKey->GetName(), "volume" ) )
		{
			subParams.masterVolume = paramsIn.masterVolume * RandomInterval( ReadInterval( pKey->GetString() ) );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "position" ) )
		{
			subParams.startingPosition = paramsIn.startingPosition + pKey->GetInt();
		}
		else if ( !V_strcasecmp( pKey->GetName(), "positionoverride" ) )
		{
			if ( paramsIn.positionOverride < 0 )
			{
				subParams.positionOverride = paramsIn.startingPosition + pKey->GetInt();
				// positionoverride is only ever used to make a whole soundscape come from a point in space
				// So go ahead and default ambients there too.
				subParams.ambientPositionOverride = paramsIn.startingPosition + pKey->GetInt();
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "ambientpositionoverride" ) )
		{
			if ( paramsIn.ambientPositionOverride < 0 )
			{
				subParams.ambientPositionOverride = paramsIn.startingPosition + pKey->GetInt();
			}
		}
		else if ( !V_strcasecmp( pKey->GetName(), "ambientoriginoverride" ) )
		{
			subParams.vForcedTextOriginAmbient = getVectorFromString(pKey->GetString());
			subParams.bForceTextOriginAmbient = true;
		}
		else if ( !V_strcasecmp( pKey->GetName(), "name" ) )
		{
			pSoundscapeName = pKey->GetString();
		}
		else if ( !V_strcasecmp(pKey->GetName(), "soundlevel") )
		{
			DevMsg(1,"soundlevel not supported on sub-soundscapes\n");
		}
		else
		{
			DevMsg( 1, "Playsoundscape %s:Unknown command %s\n", pSoundscapeName ? pSoundscapeName : pPlaySoundscape->GetName(), pKey->GetName() );
		}
		pKey = pKey->GetNextKey();
	}

	if ( pSoundscapeName )
	{
		KeyValues *pSoundscapeKeys = SoundscapeByIndex( FindSoundscapeByName( pSoundscapeName ) );
		if ( pSoundscapeKeys )
		{
			StartSubSoundscape( pSoundscapeKeys, subParams );
		}
		else
		{
			DevMsg( 1, "Trying to play unknown soundscape %s\n", pSoundscapeName );
		}
	}
}

// special kind of looping sound with no spatialization
int CSoundscapeSystem::AddLoopingAmbient( const char *pSoundName, float volume, int pitch, float radius, float flFadeRate )
{
	return AddLoopingSound( pSoundName, true, volume, SNDLVL_NORM, pitch, vec3_origin, radius, flFadeRate );
}

// add a looping sound to the list
// NOTE: will reuse existing entry (fade from current volume) if possible
//		this prevents pops
int CSoundscapeSystem::AddLoopingSound( const char *pSoundName, bool isAmbient, float volume, soundlevel_t soundlevel, int pitch, const Vector &position, float radius, float flFadeRate )
{
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());

	loopingsound_t *pSoundSlot = nullptr;
	int soundSlot = std::ssize( slot.m_loopingSounds ) - 1;
	bool bForceSoundUpdate = false;
	while ( soundSlot >= 0 )
	{
		loopingsound_t &sound = slot.m_loopingSounds[soundSlot];

		// NOTE: Will always restart/crossfade positional sounds
		if ( sound.id != slot.m_loopingSoundId && 
			sound.pitch == pitch && 
			!V_strcasecmp( pSoundName, sound.pWaveName ) )
		{
			// Ambient sounds can reuse the slots.
			if ( isAmbient == true && 
				sound.isAmbient == true )
			{
				// reuse this sound
				pSoundSlot = &sound;
				break;
			}
			// Positional sounds can reuse the slots if the positions are the same.
			else if ( isAmbient == sound.isAmbient )
			{
				if ( VectorsAreEqual( position, sound.position, 0.1f ) )
				{
					// reuse this sound
					pSoundSlot = &sound;
					break;
				}
			}
		}
		soundSlot--;
	}

	if ( soundSlot < 0 )
	{
		// can't find the sound in the list, make a new one
		soundSlot = std::ssize( slot.m_loopingSounds ); slot.m_loopingSounds.emplace_back();
		if ( isAmbient )
		{
			// start at 0 and fade in
			enginesound->EmitAmbientSound( pSoundName, 0, pitch );
			slot.m_loopingSounds[soundSlot].m_volume.m_flCurrent = 0.0;
		}
		else
		{
			// non-ambients at 0 volume are culled, so start at 0.05
			CLocalPlayerFilter filter;

			EmitSound_t ep;
			ep.m_nChannel = CHAN_STATIC;
			ep.m_pSoundName =  pSoundName;
			ep.m_flVolume = 0.05;
			ep.m_SoundLevel = soundlevel;
			ep.m_nPitch = pitch;
			ep.m_pOrigin = &position;

			CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
			slot.m_loopingSounds[soundSlot].m_volume.m_flCurrent = 0.05;
		}
		slot.m_loopingSounds[soundSlot].engineGuid = enginesound->GetGuidForLastSoundEmitted();
	}

	loopingsound_t &sound = slot.m_loopingSounds[soundSlot];
	// fill out the slot
	sound.pWaveName = pSoundName;
	sound.m_volume.FadeToValue( volume, flFadeRate, FADE_VOLUME_SINE );
	sound.pitch = pitch;
	sound.id = slot.m_loopingSoundId;
	sound.isAmbient = isAmbient;
	sound.position = position;
	sound.radius = radius;
	if ( radius > 0 )
	{
		sound.soundlevel = SNDLVL_NONE;	  // play without attenuation if sound has a radius (volume will be manually set based on distance of listener to the radius)
	}
	else
	{
		sound.soundlevel = soundlevel;
	}

	if (bForceSoundUpdate)
	{
		UpdateLoopingSound(sound);
	}

	return soundSlot;
}

// stop this loop forever
void CSoundscapeSystem::StopLoopingSound( loopingsound_t &loopSound )
{
	enginesound->StopSoundByGuid( loopSound.engineGuid );
}

// update with new volume
void CSoundscapeSystem::UpdateLoopingSound( loopingsound_t &loopSound )
{
	if ( enginesound->IsSoundStillPlaying(loopSound.engineGuid) )
	{
		enginesound->SetVolumeByGuid( loopSound.engineGuid, loopSound.m_volume.m_flCurrent );
		return;
	}

	if ( loopSound.isAmbient )
	{
		enginesound->EmitAmbientSound( loopSound.pWaveName, loopSound.m_volume.m_flCurrent, loopSound.pitch, SND_CHANGE_VOL );
	}
	else
	{
		CLocalPlayerFilter filter;

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName =  loopSound.pWaveName;
		ep.m_flVolume = loopSound.m_volume.m_flCurrent;
		ep.m_SoundLevel = loopSound.soundlevel;
		ep.m_nFlags = SND_CHANGE_VOL;
		ep.m_nPitch = loopSound.pitch;
		ep.m_pOrigin = &loopSound.position;

		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
	}
	loopSound.engineGuid = enginesound->GetGuidForLastSoundEmitted();
}

// add a recurring random sound event
int CSoundscapeSystem::AddRandomSound( const randomsound_t &sound )
{
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());
	int index = std::ssize( slot.m_randomSounds ); slot.m_randomSounds.push_back( sound );
	slot.m_randomSounds[index].nextPlayTime = gpGlobals->curtime + 0.5 * RandomInterval( sound.time );

	return index;
}

// play a random sound randomly from this parameterization table
void CSoundscapeSystem::PlayRandomSound( randomsound_t &sound )
{
	contract_assert( sound.waveCount > 0 );

	int waveId = random_valve->RandomInt( 0, sound.waveCount-1 );
	KeyValues *pWaves = sound.pWaves;
	while ( waveId > 0 && pWaves )
	{
		pWaves = pWaves->GetNextKey();
		waveId--;
	}
	if ( !pWaves )
		return;

	const char *pWaveName = pWaves->GetString();

	if ( !pWaveName )
		return;

	if ( sound.isAmbient )
	{
		enginesound->EmitAmbientSound( pWaveName, sound.masterVolume * RandomInterval( sound.volume ), (int)RandomInterval( sound.pitch ) );
	}
	else
	{
		CLocalPlayerFilter filter;

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName =  pWaveName;
		ep.m_flVolume = sound.masterVolume * RandomInterval( sound.volume );
		ep.m_SoundLevel = (soundlevel_t)(int)RandomInterval( sound.soundlevel );
		ep.m_nPitch = (int)RandomInterval( sound.pitch );
		if ( sound.isRandom )
		{
			sound.position = GenerateRandomSoundPosition();
		}
		ep.m_pOrigin = &sound.position;

		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
	}
}

// walk the list of random sound commands and update
void CSoundscapeSystem::UpdateRandomSounds( float gameTime )
{
	Split_t &slot = GetPerUser(GET_ACTIVE_SPLITSCREEN_SLOT());
	if ( gameTime < slot.m_nextRandomTime )
		return;

	slot.m_nextRandomTime = gameTime + 3600;	// add some big time to check again (an hour)

	for ( int i = std::ssize( slot.m_randomSounds )-1; i >= 0; i-- )
	{
		// time to play?
		if ( gameTime >= slot.m_randomSounds[i].nextPlayTime )
		{
			// UNDONE: add this in to fix range?
			// float dt = m_randomSounds[i].nextPlayTime - gameTime;
			PlayRandomSound( slot.m_randomSounds[i] );

			// now schedule the next occurrance
			// UNDONE: add support for "play once" sounds? FastRemove() here.
			slot.m_randomSounds[i].nextPlayTime = gameTime + RandomInterval( slot.m_randomSounds[i].time );
		}

		// update next time to check the queue
		if ( slot.m_randomSounds[i].nextPlayTime < slot.m_nextRandomTime )
		{
			slot.m_nextRandomTime = slot.m_randomSounds[i].nextPlayTime;
		}
	}
}

static void cl_soundscape_printdebuginfo( const CCommand &args );
static ConCommand cl_soundscape_printdebuginfo_command( side_registry(), "cl_soundscape_printdebuginfo", cl_soundscape_printdebuginfo, "print soundscapes" );
static void cl_soundscape_printdebuginfo( const CCommand &args )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetClientSoundscapeSystem()->PrintDebugInfo();
	}
}

static void cl_ss_origin( const CCommand &args );
static ConCommand cl_ss_origin_command( side_registry(), "cl_ss_origin", cl_ss_origin, "print origin in script format" );
static void cl_ss_origin( const CCommand &args )
{
	Vector org = MainViewOrigin( GET_ACTIVE_SPLITSCREEN_SLOT() );
	Warning("\"origin\"\t\"%.1f, %.1f, %.1f\"\n", org.x, org.y, org.z );
}

#undef SOUNDSCAPE_MANIFEST_FILE

class CBaseEntity;
class CBasePlayer;
class CSpatialEntity;

using namespace ::ks::reflect;

// Purpose: Spatial entity with simple radial falloff

IMPLEMENT_REFLECT_CLIENTCLASS( CSpatialEntity, DT_SpatialEntity, CSpatialEntity )

void CSpatialEntity::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitSpatialEntity();
	}
}

void CSpatialEntity::InitSpatialEntity()
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	AddToPersonalSpatialEntityMgr();
	m_flWeight = 0.0f;
	m_flInfluence = 0.0f;
}

void CSpatialEntity::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	RemoveFromPersonalSpatialEntityMgr();
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CSpatialEntity::ShouldDraw()
{
	return false;
}

void CSpatialEntity::ClientThink()
{
	if( !m_bEnabled && m_flCurWeight == 0.0f )
	{
		m_flWeight = 0.0f;
		return;
	}

	CBaseEntity *pPlayer = CBasePlayer::GetLocalPlayer( 0 );
	if( !pPlayer )
		return;

	Vector playerOrigin = pPlayer->GetAbsOrigin();

	m_flWeight = 0.0f;

	if ( ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff )
	{
		float dist = (playerOrigin - m_vecOrigin).Length();
		m_flWeight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		m_flWeight = fpmax( 0.0f, m_flWeight );
		m_flWeight = fpmin( 1.0f, m_flWeight );

		m_flInfluence = fpmax( 0.0f, m_minFalloff - dist );
	}

	m_flWeight = m_flCurWeight * ( 1.0f - m_flWeight );

	BaseClass::ClientThink();
}

class CSpatialEntity;

using namespace ::ks::reflect;

// Purpose: Singleton manager for color correction on the client

// Gets called each frame
void CSpatialEntityMgr::Update( float frametime )
{
	if ( std::ssize( m_SpatialEntities ) <= 0 )
		return;

	m_SpatialEntities[ 0 ]->ResetAccumulation();

	for ( int i = 0; i < std::ssize( m_SpatialEntities ); ++i )
	{
		m_SpatialEntities[ i ]->Accumulate();
	}

	m_SpatialEntities[ 0 ]->ApplyAccumulation();
}

//------------------------------------------------------------------------------
// Creates, destroys spatial entities
//------------------------------------------------------------------------------
void CSpatialEntityMgr::AddSpatialEntity( CSpatialEntity *pSpatialEntity )
{
	m_SpatialEntities.push_back( pSpatialEntity );
}

void CSpatialEntityMgr::RemoveSpatialEntity( CSpatialEntity *pSpatialEntity )
{
	ks::erase_first( m_SpatialEntities, pSpatialEntity );
}

// Purpose: A little helper class that computes a spline patch

//-----------------------------------------------------------------------------
// Catmull rom blend weights
//-----------------------------------------------------------------------------

static VMatrix s_CatmullRom( -0.5,  1.5, -1.5,  0.5,
							    1, -2.5,    2, -0.5,
							 -0.5,    0,  0.5,    0,
							    0,    1,    0,    0 );

//-----------------------------------------------------------------------------
// The last argument represents the number of float channels in addition to position
//-----------------------------------------------------------------------------

CSplinePatch::CSplinePatch( ) : m_ChannelCount(0),
	m_Width(0), m_Height(0), m_ppPositions(0), m_LinearFactor(1.0f)
{
}

CSplinePatch::~CSplinePatch()
{
}

//-----------------------------------------------------------------------------
// Initialize the spline patch
//-----------------------------------------------------------------------------

void CSplinePatch::Init( int w, int h, int extraChannels )
{
	contract_assert( extraChannels < MAX_CHANNELS );
	m_ChannelCount = extraChannels;
	m_Width = w;
	m_Height = h;
	m_LinearFactor = 1.0f;
}

//-----------------------------------------------------------------------------
// 0 = linear, 1 = spliney!
//-----------------------------------------------------------------------------

void CSplinePatch::SetLinearBlend( float factor )
{
	m_LinearFactor = factor;
}

//-----------------------------------------------------------------------------
// Hooks the patch up to externally controlled data...
//-----------------------------------------------------------------------------

void CSplinePatch::SetControlPositions( Vector const** pPositions )
{
	m_ppPositions = pPositions;
}

void CSplinePatch::SetChannelData( int channel, float* pChannel )
{
	m_pChannel[channel] = pChannel;
}

static inline void ComputeIndex( int i, int maxval, int* idx )
{
	if (i == 0)
	{
		idx[0] = 0; idx[1] = 0; idx[2] = 1; 
		idx[3] = (maxval > 2) ? 2 : 1;
	}
	else
	{
		idx[0] = i-1; idx[1] = i;
		if (i >= maxval - 1)
		{
			idx[2] = i; idx[3] = i;
		}
		else
		{
			idx[2] = i+1;
			if (i >= maxval - 2)
				idx[3] = i+1;
			else
				idx[3]=  i+2;
		}
	}
}

//-----------------------------------------------------------------------------
// Computes indices of the samples to read for this interpolation
//-----------------------------------------------------------------------------

void CSplinePatch::ComputeIndices( )
{
	int s[4];
	int t[4];

	ComputeIndex( m_is, m_Width, s );
	ComputeIndex( m_it, m_Height, t );

	int base = t[0] * m_Width;
	m_SampleIndices[0][0] = base + s[0];
	m_SampleIndices[1][0] = base + s[1];
	m_SampleIndices[2][0] = base + s[2];
	m_SampleIndices[3][0] = base + s[3];

	base = t[1] * m_Width;
	m_SampleIndices[0][1] = base + s[0];
	m_SampleIndices[1][1] = base + s[1];
	m_SampleIndices[2][1] = base + s[2];
	m_SampleIndices[3][1] = base + s[3];

	base = t[2] * m_Width;
	m_SampleIndices[0][2] = base + s[0];
	m_SampleIndices[1][2] = base + s[1];
	m_SampleIndices[2][2] = base + s[2];
	m_SampleIndices[3][2] = base + s[3];

	base = t[3] * m_Width;
	m_SampleIndices[0][3] = base + s[0];
	m_SampleIndices[1][3] = base + s[1];
	m_SampleIndices[2][3] = base + s[2];
	m_SampleIndices[3][3] = base + s[3];
}

//-----------------------------------------------------------------------------
// Call this before querying the patch for data at (s,t)
//-----------------------------------------------------------------------------

void CSplinePatch::SetupPatchQuery( float s, float t )
{
	m_is = (int)s;
	m_it = (int)t;

	if( m_is >= m_Width )
	{
		m_is = m_Width - 1;
		m_fs = 1.0f;
	}
	else
	{
		m_fs = s - m_is;
	}

	if( m_it >= m_Height )
	{
		m_it = m_Height - 1;
		m_ft = 1.0f;
	}
	else
	{
		m_ft = t - m_it;
	}

	ComputeIndices( );

	// The patch equation is:
	// px = S * M * Gx * M^T * T^T 
	// py = S * M * Gy * M^T * T^T 
	// pz = S * M * Gz * M^T * T^T 
	// where S = [s^3 s^2 s 1], T = [t^3 t^2 t 1]
	// M is the patch type matrix, in my case I'm using a catmull-rom
	// G is the array of control points. rows have constant t

	// We're gonna cache off S * M and M^T * T^T...
	Vector4D svec, tvec;
 	float fs2 = m_fs * m_fs;
	svec[0] = fs2 * m_fs; svec[1] = fs2; svec[2] = m_fs; svec[3] = 1.0f;
	float ft2 = m_ft * m_ft;
	tvec[0] = ft2 * m_ft; tvec[1] = ft2; tvec[2] = m_ft; tvec[3] = 1.0f;

	// This sets up the catmull rom matrix based on the blend factor!!
	// we can go from linear to curvy!
	s_CatmullRom.Init(  -0.5 * m_LinearFactor,  1.5 * m_LinearFactor, -1.5 * m_LinearFactor,  0.5 * m_LinearFactor,
						       m_LinearFactor, -2.5 * m_LinearFactor,    2 * m_LinearFactor, -0.5 * m_LinearFactor,
						-0.5 * m_LinearFactor,   -1 + m_LinearFactor,    1 - 0.5 * m_LinearFactor,    0,
											0,					   1,					  0,    0 );
	Vector4DMultiplyTranspose( s_CatmullRom, svec, m_SVec );
	Vector4DMultiplyTranspose( s_CatmullRom, tvec, m_TVec );
}

//-----------------------------------------------------------------------------
// Gets the point and normal at (i,j) specified above
//-----------------------------------------------------------------------------

void CSplinePatch::GetPointAndNormal( Vector& position, Vector& normal ) const
{
	// The patch equation is:
	// px = S * M * Gx * M^T * T^T 
	// py = S * M * Gy * M^T * T^T 
	// pz = S * M * Gz * M^T * T^T 
	// where S = [s^3 s^2 s 1], T = [t^3 t^2 t 1]
	// M is the patch type matrix, in my case I'm using a catmull-rom
	// G is the array of control points. rows have constant t

	VMatrix controlPointsX, controlPointsY, controlPointsZ;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			int idx = m_SampleIndices[i][j];
			controlPointsX[i][j] = m_ppPositions[ idx ]->x;
			controlPointsY[i][j] = m_ppPositions[ idx ]->y;
			controlPointsZ[i][j] = m_ppPositions[ idx ]->z;
		}
	}

	Vector4D tmp;

	Vector4DMultiply( controlPointsX, m_TVec, tmp );
	position[0] = DotProduct4D( tmp, m_SVec );
	Vector4DMultiply( controlPointsY, m_TVec, tmp );
	position[1] = DotProduct4D( tmp, m_SVec );
	Vector4DMultiply( controlPointsZ, m_TVec, tmp );
	position[2] = DotProduct4D( tmp, m_SVec );

	// Normal computation

	float fs2 = m_fs * m_fs;
	float ft2 = m_ft * m_ft;
	Vector4D dsvec( 3.0f * fs2, 2.0f * m_fs, 1.0f, 0.0f );
	Vector4D dtvec( 3.0f * ft2, 2.0f * m_ft, 1.0f, 0.0f );

	Vector4DMultiplyTranspose( s_CatmullRom, dsvec, dsvec );
	Vector4DMultiplyTranspose( s_CatmullRom, dtvec, dtvec );

	Vector ds, dt;

	Vector4DMultiply( controlPointsX, m_TVec, tmp );
	ds[0] = DotProduct4D( tmp, dsvec );
	Vector4DMultiply( controlPointsY, m_TVec, tmp );
	ds[1] = DotProduct4D( tmp, dsvec );
	Vector4DMultiply( controlPointsZ, m_TVec, tmp );
	ds[2] = DotProduct4D( tmp, dsvec );

	Vector4DMultiply( controlPointsX, dtvec, tmp );
	dt[0] = DotProduct4D( tmp, m_SVec );
	Vector4DMultiply( controlPointsY, dtvec, tmp );
	dt[1] = DotProduct4D( tmp, m_SVec );
	Vector4DMultiply( controlPointsZ, dtvec, tmp );
	dt[2] = DotProduct4D( tmp, m_SVec );

	CrossProduct( ds, dt, normal );
	VectorNormalize( normal );
}

//-----------------------------------------------------------------------------
// Gets at other channels
//-----------------------------------------------------------------------------

float CSplinePatch::GetChannel( int channel ) const
{
	// The patch equation is:
	// px = S * M * Gx * M^T * T^T 
	// py = S * M * Gy * M^T * T^T 
	// pz = S * M * Gz * M^T * T^T 
	// where S = [s^3 s^2 s 1], T = [t^3 t^2 t 1]
	// M is the patch type matrix, in my case I'm using a catmull-rom
	// G is the array of control points. rows have constant t

	contract_assert( m_pChannel[channel] );

	VMatrix controlPoints;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			controlPoints[i][j] = m_pChannel[channel][ m_SampleIndices[i][j] ];
		}
	}

	Vector4D tmp;
	Vector4DMultiply( controlPoints, m_TVec, tmp );
	return DotProduct4D( tmp, m_SVec );
}

class CBaseEntity;

using namespace ::ks::reflect;

//##################################################################
//
// PlasmaBeamNode - generates plasma embers
//
//##################################################################
class CSpotlightEnd : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SpotlightEnd" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSpotlightEnd, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CSpotlightEnd();

public:
	void	OnDataChanged(DataUpdateType_t updateType);
	bool	ShouldDraw();
	void	ClientThink( void );

	virtual bool ShouldInterpolate();

//	Vector	m_vSpotlightOrg;
//	Vector	m_vSpotlightDir;
	[[= ks::reflect::Net{} ]] float	m_flLightScale;
	[[= ks::reflect::Net{} ]] float	m_Radius;

private:
	dlight_t*	m_pDynamicLight;

	//dlight_t*	m_pModelLight;
};

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CSpotlightEnd::CSpotlightEnd(void) : /*m_pModelLight(0), */m_pDynamicLight(0)
{
	m_flLightScale	= 100;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CSpotlightEnd::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CSpotlightEnd::ShouldDraw()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: YWB:  This is a hack, BaseClass::Interpolate skips this entity because model == NULL
//   We could do something like model = (model_t *)0x00000001, but that's probably more evil.
// Input  : currentTime - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSpotlightEnd::ShouldInterpolate()
{
	return true;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CSpotlightEnd::ClientThink(void)
{
	// If light scale is zero, don't draw light
	if ( m_flLightScale <= 0 )
		return;

	color24 c = GetRenderColor();
	float a = GetRenderAlpha() / 255.0f;
	ColorRGBExp32 color;
	color.r	= c.r * a;
	color.g	= c.g * a;
	color.b	= c.b * a;
	color.exponent = 0;
	if ( color.r == 0 && color.g == 0 && color.b == 0 )
		return;

	// Deal with the environment light
	if ( !m_pDynamicLight || (m_pDynamicLight->key != index) )
	{
		m_pDynamicLight = effects->CL_AllocDlight( index );
		contract_assert( m_pDynamicLight );
	}

	//m_pDynamicLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
	m_pDynamicLight->radius		= m_flLightScale*3.0f;
	m_pDynamicLight->origin		= GetAbsOrigin() + Vector(0,0,5);
	m_pDynamicLight->die		= gpGlobals->curtime + 0.05f;
	m_pDynamicLight->color		= color;

	/*
	// For bumped lighting
	VectorCopy (m_vSpotlightDir,  m_pDynamicLight->m_Direction);

	// Deal with the model light
 	if ( !m_pModelLight || (m_pModelLight->key != -index) )
	{
		m_pModelLight = effects->CL_AllocDlight( -index );
		assert (m_pModelLight);
	}

	m_pModelLight->radius = m_Radius;
	m_pModelLight->flags = DLIGHT_NO_WORLD_ILLUMINATION;
	m_pModelLight->color.r = c.r * c.a;
	m_pModelLight->color.g = c.g * c.a;
	m_pModelLight->color.b = c.b * c.a;
	m_pModelLight->color.exponent	= 1;
	m_pModelLight->origin		= m_vSpotlightOrg;
	m_pModelLight->m_InnerAngle = 6;
	m_pModelLight->m_OuterAngle = 8;
	m_pModelLight->die = gpGlobals->curtime + 0.05;
	VectorCopy( m_vSpotlightDir, m_pModelLight->m_Direction );
	*/

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

IMPLEMENT_REFLECT_CLIENTCLASS( CSpotlightEnd, DT_SpotlightEnd, CSpotlightEnd )
