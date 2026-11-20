// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <string.h>
#include "dlight.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "iclientrenderable.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "te_effect_dispatch.h"
#include "cdll_client_int.h"
#include "effect_color_tables.h"
#include "materialsystem/imesh.h"
#include "cmodel.h"
#include "datacache/imdlcache.h"
#include "engine/ICollideable.h"
#include "materialsystem/imaterial.h"
#include "bspflags.h"
#include "const.h"
#include "icliententity.h"
module ks.client;

import std;
import ks.core.numeric;
import ks.core.random;
import ks.core.types;
import ks.core.vprof;
import ks.game.beam_flags;
import ks.game.sun_shared;
import ks.game.tempentity;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector4d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.intrusive_ptr;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :iinput;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.tier1.exprevaluator;
import ks.core.basetypes;
import ks.math.ssemath;
import ks.tier1.interfaces;
import ks.math.vector2d;
import ks.tier1.strtools;
import ks.core.dbg;
import ks.core.wchartypes;
import ks.math.vmatrix;
import :cstrike15.cs_player;
import ks.game.entity_cast;

class CEffectData;
class CParticleSimulateIterator;
class CBaseAnimating;
class CBaseEntity;
struct Particle;
class SimpleParticle;

using namespace ::ks::reflect;

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheMuzzleFlash )
PRECACHE( MATERIAL, "effects/muzzleflash1" )
PRECACHE( MATERIAL, "effects/muzzleflash2" )
PRECACHE( MATERIAL, "effects/muzzleflash3" )
PRECACHE( MATERIAL, "effects/muzzleflash4" )
PRECACHE_REGISTER_END()

//Whether or not we should emit a dynamic light
ConVar muzzleflash_light( side_registry(), "muzzleflash_light", "1", FCVAR_ARCHIVE );

extern void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType );

//===================================================================
//===================================================================
class CImpactOverlay : public CWarpOverlay
{
public:
	
	virtual bool Update( void )
	{
		m_flLifetime += gpGlobals->frametime;
		
		const float flTotalLifetime = 0.1f;

		if ( m_flLifetime < flTotalLifetime )
		{
			float flColorScale = 1.0f - ( m_flLifetime / flTotalLifetime );

			for( int i=0; i < m_nSprites; i++ )
			{
				m_Sprites[i].m_vColor = m_vBaseColors[i] * flColorScale;
				
				m_Sprites[i].m_flHorzSize += 1.0f * gpGlobals->frametime;
				m_Sprites[i].m_flVertSize += 1.0f * gpGlobals->frametime;
			}
	
			return true;
		}
	
		return false;
	}

public:

	float	m_flLifetime;
	Vector	m_vBaseColors[MAX_SUN_LAYERS];

};

//-----------------------------------------------------------------------------
// Purpose: Play random ricochet sound
// Input  : *pos - 
//-----------------------------------------------------------------------------
void FX_RicochetSound( const Vector& pos )
{
	Vector org = pos;
	CLocalPlayerFilter filter;
 	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "FX_RicochetSound.Ricochet", &org );

	if (RandomFloat() < 0.01) // 1% chance of playing legacy cs 1.6 ric sound.
	{
		CBaseEntity::EmitSound(filter, SOUND_FROM_WORLD, "FX_RicochetSound.Ricochet_Legacy", &org);
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entityIndex - 
//			attachmentIndex - 
//			*origin - 
//			*angles - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FX_GetAttachmentTransform( ClientEntityHandle_t hEntity, int attachmentIndex, Vector *origin, QAngle *angles )
{
	// Validate our input
	if ( ( hEntity == INVALID_EHANDLE ) || ( attachmentIndex < 1 ) )
	{
		if ( origin != nullptr )
		{
			*origin = vec3_origin;
		}
		
		if ( angles != nullptr )
		{
			*angles = QAngle(0,0,0);
		}

		return false;
	}

	// Get the actual entity
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( hEntity );
	if ( pRenderable )
	{
		Vector attachOrigin;
		QAngle attachAngles;

		// Find the attachment's matrix
		pRenderable->GetAttachment( attachmentIndex, attachOrigin, attachAngles );
	
		if ( origin != nullptr )
		{
			*origin = attachOrigin;
		}
		
		if ( angles != nullptr )
		{
			*angles = attachAngles;
		}
		
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entityIndex - 
//			attachmentIndex - 
//			&transform - 
//-----------------------------------------------------------------------------
bool FX_GetAttachmentTransform( ClientEntityHandle_t hEntity, int attachmentIndex, matrix3x4_t &transform )
{
	Vector	origin;
	QAngle	angles;

	if ( FX_GetAttachmentTransform( hEntity, attachmentIndex, &origin, &angles ) )
	{
		AngleMatrix( angles, origin, transform );
		return true;
	}

	// Entity doesn't exist
	SetIdentityMatrix( transform );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FX_MuzzleEffect( 
	const Vector &origin, 
	const QAngle &angles, 
	float scale, 
	ClientEntityHandle_t hEntity, 
	unsigned char *pFlashColor,
	bool bOneFrame )
{
	CVProfScope _( "FX_MuzzleEffect", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash" );
	pSimple->SetSortOrigin( origin );
	
	SimpleParticle *pParticle;
	Vector			forward, offset;

	AngleVectors( angles, &forward );
	float flScale = random_valve->RandomFloat( scale-0.25f, scale+0.25f );

	if ( flScale < 0.5f )
	{
		flScale = 0.5f;
	}
	else if ( flScale > 8.0f )
	{
		flScale = 8.0f;
	}

	//
	// Flash
	//

	int i;
	for ( i = 1; i < 9; i++ )
	{
		offset = origin + (forward * (i*2.0f*scale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( VarArgs( "effects/muzzleflash%d", random_valve->RandomInt(1,4) ) ), offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= /*bOneFrame ? 0.0001f : */0.1f;

		pParticle->m_vecVelocity.Init();

		if ( !pFlashColor )
		{
			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;
		}
		else
		{
			pParticle->m_uchColor[0]	= pFlashColor[0];
			pParticle->m_uchColor[1]	= pFlashColor[1];
			pParticle->m_uchColor[2]	= pFlashColor[2];
		}

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 128;

		pParticle->m_uchStartSize	= (random_valve->RandomFloat( 6.0f, 9.0f ) * (12-(i))/9) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	//
	// Smoke
	//

	/*
	for ( i = 0; i < 4; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "particle/particle_smokegrenade" ), origin );
			
		if ( pParticle == NULL )
			return;

		alpha = random->RandomInt( 32, 84 );
		color = random->RandomInt( 64, 164 );

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random->RandomFloat( 0.5f, 1.0f );

		pParticle->m_vecVelocity.Random( -0.5f, 0.5f );
		pParticle->m_vecVelocity += forward;
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity	*= random->RandomFloat( 16.0f, 32.0f );
		pParticle->m_vecVelocity[2] += random->RandomFloat( 4.0f, 16.0f );

		pParticle->m_uchColor[0]	= color;
		pParticle->m_uchColor[1]	= color;
		pParticle->m_uchColor[2]	= color;
		pParticle->m_uchStartAlpha	= alpha;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_uchStartSize	= random->RandomInt( 4, 8 ) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize*2;
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scale - 
//			attachmentIndex - 
//			bOneFrame - 
//-----------------------------------------------------------------------------
void FX_MuzzleEffectAttached( 
	float scale, 
	ClientEntityHandle_t hEntity, 
	int attachmentIndex, 
	unsigned char *pFlashColor,
	bool bOneFrame )
{
	CVProfScope _( "FX_MuzzleEffect", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
	ks::intrusive_ptr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash", hEntity, attachmentIndex );
	contract_assert( pSimple );
	if ( pSimple == nullptr )
		return;
	
	// Lock our bounding box
	pSimple->GetBinding().SetBBox( -( Vector( 16, 16, 16 ) * scale ), ( Vector( 16, 16, 16 ) * scale ) );
	
	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset;

	float flScale = random_valve->RandomFloat( scale-0.25f, scale+0.25f );

	if ( flScale < 0.5f )
	{
		flScale = 0.5f;
	}
	else if ( flScale > 8.0f )
	{
		flScale = 8.0f;
	}

	//
	// Flash
	//

	int i;
	for ( i = 1; i < 9; i++ )
	{
		offset = (forward * (i*2.0f*scale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_SMG_Muzzleflash[random_valve->RandomInt(0,3)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= bOneFrame ? 0.0001f : 0.1f;

		pParticle->m_vecVelocity.Init();

		if ( !pFlashColor )
		{
			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;
		}
		else
		{
			pParticle->m_uchColor[0]	= pFlashColor[0];
			pParticle->m_uchColor[1]	= pFlashColor[1];
			pParticle->m_uchColor[2]	= pFlashColor[2];
		}

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 128;

		pParticle->m_uchStartSize	= (random_valve->RandomFloat( 6.0f, 9.0f ) * (12-(i))/9) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

}

//-----------------------------------------------------------------------------
// Old-style muzzle flashes
//-----------------------------------------------------------------------------
void MuzzleFlashCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;
	if ( data.entindex() > 0 )
	{
		IClientRenderable *pRenderable = data.GetRenderable();
		if ( !pRenderable )
			return;

		if ( data.m_nAttachmentIndex )
		{
			//FIXME: We also need to allocate these particles into an attachment space setup
			pRenderable->GetAttachment( data.m_nAttachmentIndex, vecOrigin, vecAngles );
		}
		else
		{
			vecOrigin = pRenderable->GetRenderOrigin();
			vecAngles = pRenderable->GetRenderAngles();
		}
	}

	tempents->MuzzleFlash( vecOrigin, vecAngles, data.m_fFlags & (~MUZZLEFLASH_FIRSTPERSON), data.m_hEntity, (data.m_fFlags & MUZZLEFLASH_FIRSTPERSON) != 0 );	
}

DECLARE_CLIENT_EFFECT( MuzzleFlash, MuzzleFlashCallback );
 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&velocity - 
//			scale - 
//			numParticles - 
//			*pColor - 
//			iAlpha - 
//			*pMaterial - 
//			flRoll - 
//			flRollDelta - 
//-----------------------------------------------------------------------------
ks::intrusive_ptr<CSimpleEmitter> FX_Smoke( const Vector &origin, const Vector &velocity, float scale, int numParticles, float flDietime, unsigned char *pColor, int iAlpha, const char *pMaterial, float flRoll, float flRollDelta )
{
	CVProfScope _( "FX_Smoke", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_Smoke" );
	pSimple->SetSortOrigin( origin );

	SimpleParticle *pParticle;

	// Smoke
	for ( int i = 0; i < numParticles; i++ )
	{
		PMaterialHandle hMaterial = pSimple->GetPMaterial( pMaterial );
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );			
		if ( pParticle == nullptr )
			return nullptr;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime = flDietime;
		pParticle->m_vecVelocity = velocity;
		for( int i = 0; i < 3; ++i )
		{
			pParticle->m_uchColor[i] = pColor[i];
		}
		pParticle->m_uchStartAlpha	= iAlpha;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_uchStartSize	= scale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize*2;
		pParticle->m_flRoll	= flRoll;
		pParticle->m_flRollDelta = flRollDelta;
	}

	return pSimple;
}

//-----------------------------------------------------------------------------
// Purpose: Smoke puffs
//-----------------------------------------------------------------------------
void FX_Smoke( const Vector &origin, const QAngle &angles, float scale, int numParticles, unsigned char *pColor, int iAlpha )
{
	CVProfScope _( "FX_Smoke", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector vecVelocity;
	Vector vecForward;

	// Smoke
	for ( int i = 0; i < numParticles; i++ )
	{
		// Velocity
		AngleVectors( angles, &vecForward );
		vecVelocity.Random( -0.5f, 0.5f );
		vecVelocity += vecForward;
		VectorNormalize( vecVelocity );
		vecVelocity	*= random_valve->RandomFloat( 16.0f, 32.0f );
		vecVelocity[2] += random_valve->RandomFloat( 4.0f, 16.0f );

		// Color
		unsigned char particlecolor[3];
		if ( !pColor )
		{
			int color = random_valve->RandomInt( 64, 164 );
			particlecolor[0] = color;
			particlecolor[1] = color;
			particlecolor[2] = color;
		}
		else
		{
			particlecolor[0] = pColor[0];
			particlecolor[1] = pColor[1];
			particlecolor[2] = pColor[2];
		}

		// Alpha
		int alpha = iAlpha;
		if ( alpha == -1 )
		{
			alpha = random_valve->RandomInt( 10, 25 );
		}

		// Scale
		int iSize = random_valve->RandomInt( 4, 8 ) * scale;

		// Roll
		float flRoll = random_valve->RandomInt( 0, 360 );
		float flRollDelta = random_valve->RandomFloat( -4.0f, 4.0f );

		//pParticle->m_uchEndSize		= pParticle->m_uchStartSize*2;

		FX_Smoke( origin, vecVelocity, iSize, 1, random_valve->RandomFloat( 0.5f, 1.0f ), particlecolor, alpha, "particle/particle_smokegrenade", flRoll, flRollDelta );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Smoke Emitter
//			This is an emitter that keeps spitting out particles for its lifetime
//			It won't create particles if it's not in view, so it's best when short lived
//-----------------------------------------------------------------------------
class CSmokeEmitter : public CSimpleEmitter
{
	typedef CSimpleEmitter BaseClass;
public:
	CSmokeEmitter( ClientEntityHandle_t hEntity, int nAttachment, const char *pDebugName ) : CSimpleEmitter( pDebugName ) 
	{
		m_hEntity = hEntity;
		m_nAttachmentIndex = nAttachment;
		m_flDeathTime = 0;
		m_flLastParticleSpawnTime = 0;
	}
	
	// Create
	static CSmokeEmitter *Create( ClientEntityHandle_t hEntity, int nAttachment, const char *pDebugName="smoke" )
	{
		return new CSmokeEmitter( hEntity, nAttachment, pDebugName );
	}

	void SetLifeTime( float flTime )
	{
		m_flDeathTime = gpGlobals->curtime + flTime;
	}

	void SetSpurtAngle( QAngle &vecAngles )
	{
		AngleVectors( vecAngles, &m_vecSpurtForward );
	}

	void SetSpurtColor( const Vector4D &pColor )
	{
		for ( int i = 0; i <= 3; i++ )
		{
			m_SpurtColor[i] = pColor[i];
		}
	}

	void SetSpawnRate( float flRate )
	{
		m_flSpawnRate = flRate;
	}

	void CreateSpurtParticles( void )
	{
		SimpleParticle *pParticle;
		// PMaterialHandle hMaterial = GetPMaterial( "particle/particle_smokegrenade" );

		Vector vecOrigin = m_vSortOrigin;
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle(m_hEntity);
		if ( pRenderable && m_nAttachmentIndex )
		{
			QAngle tmp;
			pRenderable->GetAttachment( m_nAttachmentIndex, vecOrigin, tmp );
			SetSortOrigin( vecOrigin );
		}

		// Smoke
		int numParticles = RandomInt( 1,2 );
		for ( int i = 0; i < numParticles; i++ )
		{
			pParticle = (SimpleParticle *) AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[0], vecOrigin );			
			if ( pParticle == nullptr )
				break;

			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime = RandomFloat( 0.5, 1.0 );

			// Random velocity around the angles forward
			Vector vecVelocity;
			vecVelocity.Random( -0.1f, 0.1f );
			vecVelocity += m_vecSpurtForward;
			VectorNormalize( vecVelocity );
			vecVelocity	*= RandomFloat( 160.0f, 640.0f );
			pParticle->m_vecVelocity = vecVelocity;

			// Randomize the color a little
			int color[3][2];
			for( int i = 0; i < 3; ++i )
			{
				color[i][0] = std::max<std::common_type_t<int, vec_t>>( 0, m_SpurtColor[i] - 64 );
				color[i][1] = std::min<std::common_type_t<int, vec_t>>( 255, m_SpurtColor[i] + 64 );
			}
			pParticle->m_uchColor[0] = random_valve->RandomInt( color[0][0], color[0][1] );
			pParticle->m_uchColor[1] = random_valve->RandomInt( color[1][0], color[1][1] );
			pParticle->m_uchColor[2] = random_valve->RandomInt( color[2][0], color[2][1] );

			pParticle->m_uchStartAlpha = m_SpurtColor[3];
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 50, 60 );
			pParticle->m_uchEndSize = pParticle->m_uchStartSize*3;
			pParticle->m_flRoll	= RandomFloat( 0, 360 );
			pParticle->m_flRollDelta = RandomFloat( -4.0f, 4.0f );
		}			

		m_flLastParticleSpawnTime = gpGlobals->curtime + m_flSpawnRate;
	}

	virtual void SimulateParticles( CParticleSimulateIterator *pIterator )
	{
		Particle *pParticle = pIterator->GetFirst();
		while ( pParticle )
		{
			// If our lifetime isn't up, create more particles
			if ( m_flDeathTime > gpGlobals->curtime )
			{
				if ( m_flLastParticleSpawnTime <= gpGlobals->curtime )
				{
					CreateSpurtParticles();
				}
			}
			
			pParticle = pIterator->GetNext();
		}

		BaseClass::SimulateParticles( pIterator );
	}

private:
	float		m_flDeathTime;
	float		m_flLastParticleSpawnTime;
	float		m_flSpawnRate;
	Vector		m_vecSpurtForward;
	Vector4D	m_SpurtColor;
	ClientEntityHandle_t m_hEntity;
	int			m_nAttachmentIndex;

	CSmokeEmitter( const CSmokeEmitter & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Purpose: Small hose gas spurt
//-----------------------------------------------------------------------------
void FX_BuildSmoke( Vector &vecOrigin, QAngle &vecAngles, ClientEntityHandle_t hEntity, int nAttachment, float flLifeTime, const Vector4D &pColor )
{
	ks::intrusive_ptr<CSmokeEmitter> pSimple = CSmokeEmitter::Create( hEntity, nAttachment, "FX_Smoke" );
	pSimple->SetSortOrigin( vecOrigin );
	pSimple->SetLifeTime( flLifeTime );
	pSimple->SetSpurtAngle( vecAngles );
	pSimple->SetSpurtColor( pColor );
	pSimple->SetSpawnRate( 0.03 );
	pSimple->CreateSpurtParticles();
}

//-----------------------------------------------------------------------------
// Purpose: Green hose gas spurt
//-----------------------------------------------------------------------------
void SmokeCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	Vector4D color( 50,50,50,255 );
	FX_BuildSmoke( vecOrigin, vecAngles, data.m_hEntity, data.m_nAttachmentIndex, 100.0, color ); 
}

DECLARE_CLIENT_EFFECT( Smoke, SmokeCallback );

//-----------------------------------------------------------------------------
// Purpose: Shockwave for gunship bullet impacts!
// Input  : &pos - 
//
// NOTES:	-Don't draw this effect when the viewer is very far away.
//-----------------------------------------------------------------------------
void FX_GunshipImpact( const Vector &pos, const Vector &normal, float r, float g, float b )
{
	CVProfScope _( "FX_GunshipImpact", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	if ( CImpactOverlay *pOverlay = new CImpactOverlay )
	{
		pOverlay->m_flLifetime	= 0;
		VectorMA( pos, 1.0f, normal, pOverlay->m_vPos ); // Doesn't show up on terrain if you don't do this(sjb)
		pOverlay->m_nSprites	= 1;
		
		pOverlay->m_vBaseColors[0].Init( r, g, b );

		pOverlay->m_Sprites[0].m_flHorzSize = 0.01f;
		pOverlay->m_Sprites[0].m_flVertSize = 0.01f;

		pOverlay->Activate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : data - 
//-----------------------------------------------------------------------------
void GunshipImpactCallback( const CEffectData & data )
{
	Vector vecPosition;

	vecPosition = data.m_vOrigin;

	FX_GunshipImpact( vecPosition, Vector( 0, 0, 1 ), 100, 0, 200 );
}
DECLARE_CLIENT_EFFECT( GunshipImpact, GunshipImpactCallback );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CommandPointerCallback( const CEffectData & data )
{
	int size = COLOR_TABLE_SIZE( commandercolors );

	for( int i = 0 ; i < size ; i++ )
	{
		if( commandercolors[ i ].index == data.m_nColor )
		{
			FX_GunshipImpact( data.m_vOrigin, Vector( 0, 0, 1 ), commandercolors[ i ].r, commandercolors[ i ].g, commandercolors[ i ].b );
			return;
		}
	}
}

DECLARE_CLIENT_EFFECT( CommandPointer, CommandPointerCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FX_GunshipMuzzleEffect( const Vector &origin, const QAngle &angles, float scale, ClientEntityHandle_t hEntity, unsigned char *pFlashColor )
{
	CVProfScope _( "FX_GunshipMuzzleEffect", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash" );
	pSimple->SetSortOrigin( origin );
	
	SimpleParticle *pParticle;
	Vector			forward, offset;

	AngleVectors( angles, &forward );

	//
	// Flash
	//
	offset = origin;

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/gunshipmuzzle" ), offset );
		
	if ( pParticle == nullptr )
		return;

	pParticle->m_flLifetime		= 0.0f;
	pParticle->m_flDieTime		= 0.15f;

	pParticle->m_vecVelocity.Init();

	pParticle->m_uchStartSize	= random_valve->RandomFloat( 40.0, 50.0 );
	pParticle->m_uchEndSize		= pParticle->m_uchStartSize;

	pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= 0.15f;

	pParticle->m_uchColor[0]	= 255;
	pParticle->m_uchColor[1]	= 255;
	pParticle->m_uchColor[2]	= 255;

	pParticle->m_uchStartAlpha	= 255;
	pParticle->m_uchEndAlpha	= 255;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//			velocity - 
//			makeWhiz - 
//-----------------------------------------------------------------------------
void FX_GunshipTracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	CVProfScope _( "FX_GunshipTracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	vNear, dStart, dEnd, shotDir;
	float	totalDist;

	//Get out shot direction and length
	VectorSubtract( end, start, shotDir );
	totalDist = VectorNormalize( shotDir );

	//Don't make small tracers
	if ( totalDist <= 256 )
		return;

	float length = random_valve->RandomFloat( 128.0f, 256.0f );
	float life = ( totalDist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
	
	//Add it
	FX_AddDiscreetLine( start, shotDir, velocity, length, totalDist, 5.0f, life, "effects/gunshiptracer" );

	if( makeWhiz )
	{
		FX_TracerSound( start, end, TRACER_TYPE_GUNSHIP );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void FX_StriderMuzzleEffect( const Vector &origin, const QAngle &angles, float scale, ClientEntityHandle_t hEntity, unsigned char *pFlashColor )
{
	Vector vecDir;
	AngleVectors( angles, &vecDir );

	float life = 0.3f;
	float speed = 100.0f;

	for( int i = 0 ; i < 5 ; i++ )
	{
		FX_AddDiscreetLine( origin, vecDir, speed, 32, speed * life, 5.0f, life, "effects/bluespark" );
		speed *= 1.5f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//			velocity - 
//			makeWhiz - 
//-----------------------------------------------------------------------------
void FX_StriderTracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	CVProfScope _( "FX_StriderTracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	vNear, dStart, dEnd, shotDir;
	float	totalDist;

	//Get out shot direction and length
	VectorSubtract( end, start, shotDir );
	totalDist = VectorNormalize( shotDir );

	//Don't make small tracers
	if ( totalDist <= 256 )
		return;

	float length = random_valve->RandomFloat( 64.0f, 128.0f );
	float life = ( totalDist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
	
	//Add it
	FX_AddDiscreetLine( start, shotDir, velocity, length, totalDist, 2.5f, life, "effects/gunshiptracer" );

	if( makeWhiz )
	{
		FX_TracerSound( start, end, TRACER_TYPE_STRIDER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//			velocity - 
//			makeWhiz - 
//-----------------------------------------------------------------------------
void FX_HunterTracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	CVProfScope _( "FX_HunterTracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	vNear, dStart, dEnd, shotDir;
	float	totalDist;

	// Get out shot direction and length
	VectorSubtract( end, start, shotDir );
	totalDist = VectorNormalize( shotDir );

	// Make short tracers in close quarters
	// float flMinLength = std::min( totalDist, 128.0f );
	// float flMaxLength = std::min( totalDist, 128.0f );

	float length = 128.0f;//random->RandomFloat( flMinLength, flMaxLength );
	float life = ( totalDist + length ) / velocity;	// NOTENOTE: We want the tail to finish its run as well
	
	// Add it
	FX_AddDiscreetLine( start, shotDir, velocity*0.5f, length, totalDist, 2.0f, life, "effects/huntertracer" );

	if( makeWhiz ) 
	{
		FX_TracerSound( start, end, TRACER_TYPE_STRIDER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//			velocity - 
//			makeWhiz - 
//-----------------------------------------------------------------------------
void FX_GaussTracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	CVProfScope _( "FX_GaussTracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	vNear, dStart, dEnd, shotDir;
	float	totalDist;

	//Get out shot direction and length
	VectorSubtract( end, start, shotDir );
	totalDist = VectorNormalize( shotDir );

	//Don't make small tracers
	if ( totalDist <= 256 )
		return;

	float length = random_valve->RandomFloat( 250.0f, 500.0f );
	float life = ( totalDist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
	
	//Add it
	FX_AddDiscreetLine( start, shotDir, velocity, length, totalDist, random_valve->RandomFloat( 5.0f, 8.0f ), life, "effects/spark" );
}

//-----------------------------------------------------------------------------
// Purpose: Create a tesla effect between two points
//-----------------------------------------------------------------------------
void FX_BuildTesla( 
	CBaseEntity *pEntity, 
	const Vector &vecOrigin, 
	const Vector &vecEnd,
	const char *pModelName,
	float flBeamWidth,
	const Vector &vColor,
	int nFlags,
	float flTimeVisible )
{
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vecOrigin;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_pszModelName = pModelName;
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = flTimeVisible;
	beamInfo.m_flWidth = flBeamWidth;
	beamInfo.m_flEndWidth = 1;
	beamInfo.m_flFadeLength = 0.3;
	beamInfo.m_flAmplitude = 16;
	beamInfo.m_flBrightness = 200.0;
	beamInfo.m_flSpeed = 0.0;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 1.0;
	beamInfo.m_flRed = vColor.x * 255.0;
	beamInfo.m_flGreen = vColor.y * 255.0;
	beamInfo.m_flBlue = vColor.z * 255.0;
	beamInfo.m_nSegments = 20;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = nFlags;
	
	beams->CreateBeamPoints( beamInfo );
}

void FX_Tesla( const CTeslaInfo &teslaInfo )
{
	CBaseEntity *pEntity = ClientEntityList().GetEnt( teslaInfo.m_nEntIndex );

	// Send out beams around us
	int iNumBeamsAround = (teslaInfo.m_nBeams * 2) / 3; // (2/3 of the beams are placed around in a circle)
	int iNumRandomBeams = teslaInfo.m_nBeams - iNumBeamsAround;
	int iTotalBeams = iNumBeamsAround + iNumRandomBeams;
	float flYawOffset = RandomFloat(0,360);
	for ( int i = 0; i < iTotalBeams; i++ )
	{
		// Make a couple of tries at it
		int iTries = -1;
		Vector vecForward;
		trace_t tr;
		do
		{
			iTries++;

			// Some beams are deliberatly aimed around the point, the rest are random.
			if ( i < iNumBeamsAround )
			{
				QAngle vecTemp = teslaInfo.m_vAngles;
				vecTemp[YAW] += anglemod( flYawOffset + ((360 / iTotalBeams) * i) );
				AngleVectors( vecTemp, &vecForward );

				// Randomly angle it up or down
				vecForward.z = RandomFloat( -1, 1 );
			}
			else
			{
				vecForward = RandomVector( -1, 1 );
			}
			VectorNormalize( vecForward );

			UTIL_TraceLine( teslaInfo.m_vPos, teslaInfo.m_vPos + (vecForward * teslaInfo.m_flRadius), MASK_SHOT, pEntity, COLLISION_GROUP_NONE, &tr );
		} while ( tr.fraction >= 1.0 && iTries < 3 );

		Vector vecEnd = tr.endpos - (vecForward * 8);

		// Only spark & glow if we hit something
		if ( tr.fraction < 1.0 )
		{
			if ( !EffectOccluded( tr.endpos, 0 ) )
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
		FX_BuildTesla( pEntity, teslaInfo.m_vPos, tr.endpos, teslaInfo.m_pszSpriteName, teslaInfo.m_flBeamWidth, teslaInfo.m_vColor, FBEAM_ONLYNOISEONCE, teslaInfo.m_flTimeVisible );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void BuildTeslaCallback( const CEffectData &data )
{
	if ( data.entindex() < 0 )
		return;

	CTeslaInfo teslaInfo;

	teslaInfo.m_vPos = data.m_vOrigin;
	teslaInfo.m_vAngles = data.m_vAngles;
	teslaInfo.m_nEntIndex = data.entindex();
	teslaInfo.m_flBeamWidth = 5;
	teslaInfo.m_vColor.Init( 1, 1, 1 );
	teslaInfo.m_flTimeVisible = 0.3;
	teslaInfo.m_flRadius = 192;
	teslaInfo.m_nBeams = 6;
	teslaInfo.m_pszSpriteName = "sprites/physbeam.vmt";

	FX_Tesla( teslaInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Tesla hitbox
//-----------------------------------------------------------------------------
void FX_BuildTeslaHitbox( 
	CBaseEntity *pEntity,
	int nStartAttachment,
	int nEndAttachment,
	float flBeamWidth,
	const Vector &vColor,
	float flTimeVisible )
{
	// One along the body
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vec3_origin;
	beamInfo.m_pszModelName = "sprites/lgtning.vmt";
	beamInfo.m_flHaloScale = 8.0f;
	beamInfo.m_flLife = 0.01f;
	beamInfo.m_flWidth = random_valve->RandomFloat( 3.0f, 6.0f );
	beamInfo.m_flEndWidth = 0.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = random_valve->RandomInt( 16, 32 );
	beamInfo.m_flBrightness = 255.0f;
	beamInfo.m_flSpeed = 32.0;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 30.0;
	beamInfo.m_flRed = vColor.x * 255.0;
	beamInfo.m_flGreen = vColor.y * 255.0;
	beamInfo.m_flBlue = vColor.z * 255.0;
	beamInfo.m_nSegments = 32;
	beamInfo.m_bRenderable = true;
	beamInfo.m_pStartEnt = beamInfo.m_pEndEnt = nullptr;

	beamInfo.m_nFlags = (FBEAM_USE_HITBOXES);

	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_nStartAttachment = nStartAttachment;
	beamInfo.m_pEndEnt = pEntity;
	beamInfo.m_nEndAttachment = nEndAttachment;

	beams->CreateBeamEntPoint( beamInfo );

	// One out to the world
	trace_t	tr;
	Vector	randomDir;

	randomDir = RandomVector( -1.0f, 1.0f );
	VectorNormalize( randomDir );

	UTIL_TraceLine( pEntity->WorldSpaceCenter(), pEntity->WorldSpaceCenter() + ( randomDir * 100 ), MASK_SOLID_BRUSHONLY, pEntity, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0f )
	{
		beamInfo.m_nType = TE_BEAMTESLA;
		beamInfo.m_vecStart = vec3_origin;
		beamInfo.m_vecEnd = tr.endpos;
		beamInfo.m_pszModelName = "sprites/lgtning.vmt";
		beamInfo.m_flHaloScale = 8.0f;
		beamInfo.m_flLife = 0.05f;
		beamInfo.m_flWidth = random_valve->RandomFloat( 2.0f, 6.0f );
		beamInfo.m_flEndWidth = 0.0f;
		beamInfo.m_flFadeLength = 0.0f;
		beamInfo.m_flAmplitude = random_valve->RandomInt( 16, 32 );
		beamInfo.m_flBrightness = 255.0f;
		beamInfo.m_flSpeed = 32.0;
		beamInfo.m_nStartFrame = 0.0;
		beamInfo.m_flFrameRate = 30.0;
		beamInfo.m_flRed = vColor.x * 255.0;
		beamInfo.m_flGreen = vColor.y * 255.0;
		beamInfo.m_flBlue = vColor.z * 255.0;
		beamInfo.m_nSegments = 32;
		beamInfo.m_bRenderable = true;
		beamInfo.m_pStartEnt = beamInfo.m_pEndEnt = nullptr;

		beamInfo.m_pStartEnt = pEntity;
		beamInfo.m_nStartAttachment = nStartAttachment;

		beams->CreateBeamEntPoint( beamInfo );
	}

	// Create an elight to illuminate the target
	if ( pEntity != nullptr )
	{
		dlight_t *el = effects->CL_AllocElight( LIGHT_INDEX_TE_DYNAMIC + pEntity->entindex() );

		// Randomly place it
		el->origin	= pEntity->WorldSpaceCenter() + RandomVector( -32, 32 );

		el->color.r = 235;
		el->color.g = 235;
		el->color.b = 255;
		el->color.exponent = 4;

		el->radius	= random_valve->RandomInt( 32, 128 );
		el->decay	= el->radius / 0.1f;
		el->die		= gpGlobals->curtime + 0.1f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void FX_BuildTeslaHitbox( const CEffectData &data )
{
	Vector vColor( 1, 1, 1 );

	CBaseEntity *pEntity = ClientEntityList().GetEnt( data.entindex() );
	CBaseAnimating *pAnimating = pEntity ? pEntity->GetBaseAnimating() : nullptr;
	if (!pAnimating)
		return;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if (!pStudioHdr)
		return;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
	if ( !set )
		return;

	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
	if ( !pAnimating->HitboxToWorldTransforms( hitboxbones ) )
		return;

	int nBeamCount = (int)(data.m_flMagnitude + 0.5f);
	for ( int i = 0; i < nBeamCount; ++i )
	{
		int nStartHitBox = random_valve->RandomInt( 1, set->numhitboxes );
		int nEndHitBox = random_valve->RandomInt( 1, set->numhitboxes );
		FX_BuildTeslaHitbox( pEntity, nStartHitBox, nEndHitBox, data.m_flScale, vColor, random_valve->RandomFloat( 0.05f, 0.2f ) );
	}
}

DECLARE_CLIENT_EFFECT( TeslaHitboxes, FX_BuildTeslaHitbox );

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void FX_BuildTeslaZap( const CEffectData &data )
{
	// Build the tesla, only works on entities
	CBaseEntity *pEntity = data.GetEntity();
	if ( !pEntity )
		return;

	Vector vColor( 1, 1, 1 );

	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_nStartAttachment = data.m_nAttachmentIndex;
	beamInfo.m_pEndEnt = nullptr;
	beamInfo.m_vecEnd = data.m_vOrigin;
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = 0.3f;
	beamInfo.m_flWidth = data.m_flScale;
	beamInfo.m_flEndWidth = 1;
	beamInfo.m_flFadeLength = 0.3;
	beamInfo.m_flAmplitude = 16;
	beamInfo.m_flBrightness = 200.0;
	beamInfo.m_flSpeed = 0.0;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 1.0;
	beamInfo.m_flRed = vColor.x * 255.0;
	beamInfo.m_flGreen = vColor.y * 255.0;
	beamInfo.m_flBlue = vColor.z * 255.0;
	beamInfo.m_nSegments = 20;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = 0;
	
	beams->CreateBeamEntPoint( beamInfo );
}

DECLARE_CLIENT_EFFECT( TeslaZap, FX_BuildTeslaZap );

class CEffectData;
class SimpleParticle;

using namespace ::ks::reflect;

// Purpose: A blood spray effect, like a big exit wound, used when people are
//			violently impaled, skewered, eviscerated, etc.

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectBloodSpray )
PRECACHE( MATERIAL, "effects/blood_core" )
PRECACHE( MATERIAL, "effects/blood_gore" )
PRECACHE( MATERIAL, "effects/blood_drop" )
PRECACHE( MATERIAL, "effects/blood_puff" )
PRECACHE_REGISTER_END()

// Cached material handles
PMaterialHandle g_Blood_Core = nullptr;
PMaterialHandle g_Blood_Gore = nullptr;
PMaterialHandle g_Blood_Drops = nullptr;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bloodtype - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void GetBloodColor( int bloodtype, colorentry_t &color )
{
	int i;

	for( i = 0 ; i < COLOR_TABLE_SIZE( bloodcolors ) ; i++ )
	{
		if( bloodcolors[i].index == bloodtype )
		{
			color = bloodcolors[ i ];
			return;
		}
	}

	// build a ridiculous default color
	color.r = 255;
	color.g = 0;
	color.b = 255;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//			r - 
//			g - 
//			b - 
//			flags - 
//-----------------------------------------------------------------------------
void FX_BloodSpray( const Vector &origin, const Vector &normal, float scale, unsigned char r, unsigned char g, unsigned char b, int flags )
{
	if ( UTIL_IsLowViolence() )
		return;

	//debugoverlay->AddLineOverlay( origin, origin + normal * 72, 255, 255, 255, true, 10 ); 

	Vector offset;
	float spread	= 0.2f;
	
	//Find area ambient light color and use it to tint smoke
	Vector worldLight = WorldGetLightForPoint( origin, true );
	Vector color = Vector( (float)(worldLight[0] * r) / 255.0f, (float)(worldLight[1] * g) / 255.0f, (float)(worldLight[2] * b) / 255.0f );
	float colorRamp;

	int i;

	Vector	offDir;

	Vector right;
	Vector up;

	if (normal != Vector(0, 0, 1) )
	{
		right = normal.Cross( Vector(0, 0, 1) );
		up = right.Cross( normal );
	}
	else
	{
		right = Vector(0, 0, 1);
		up = right.Cross( normal );
	}

	//
	// Dump out drops
	//
	if (flags & FX_BLOODSPRAY_DROPS)
	{
		TrailParticle *tParticle;

		ks::intrusive_ptr<CTrailParticles> pTrailEmitter = CTrailParticles::Create( "blooddrops" );
		if ( !pTrailEmitter )
			return;

		pTrailEmitter->SetSortOrigin( origin );

		// Partial gravity on blood drops.
		pTrailEmitter->SetGravity( 600.0 ); 

		pTrailEmitter->GetBinding().SetBBox( origin - Vector( 32, 32, 32 ), origin + Vector( 32, 32, 32 ) );
		pTrailEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
		pTrailEmitter->SetVelocityDampen( 0.2f );

		PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_drop" );

		//
		// Long stringy drops of blood.
		//
		for ( i = 0; i < 14; i++ )
		{
			// Originate from within a circle 'scale' inches in diameter.
			offset = origin;
			offset += right * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;
			offset += up * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;

			tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( tParticle == nullptr )
				break;

			tParticle->m_flLifetime	= 0.0f;

			offDir = normal + RandomVector( -0.3f, 0.3f );

			tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( 4.0f * scale, 40.0f * scale );
			tParticle->m_vecVelocity[2] += random_valve->RandomFloat( 4.0f, 16.0f ) * scale;

			tParticle->m_flWidth		= random_valve->RandomFloat( 0.125f, 0.275f ) * scale;
			tParticle->m_flLength		= random_valve->RandomFloat( 0.02f, 0.03f ) * scale;
			tParticle->m_flDieTime		= random_valve->RandomFloat( 0.5f, 1.0f );

			FloatToColor32( tParticle->m_color, color[0], color[1], color[2], 1.0f );
		}

		//
		// Shorter droplets.
		//
		for ( i = 0; i < 24; i++ )
		{
			// Originate from within a circle 'scale' inches in diameter.
			offset = origin;
			offset += right * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;
			offset += up * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;

			tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( tParticle == nullptr )
				break;

			tParticle->m_flLifetime	= 0.0f;

			offDir = normal + RandomVector( -1.0f, 1.0f );
			offDir[2] += random_valve->RandomFloat(0, 1.0f);

			tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( 2.0f * scale, 25.0f * scale );
			tParticle->m_vecVelocity[2] += random_valve->RandomFloat( 4.0f, 16.0f ) * scale;

			tParticle->m_flWidth		= random_valve->RandomFloat( 0.25f, 0.375f ) * scale;
			tParticle->m_flLength		= random_valve->RandomFloat( 0.0025f, 0.005f ) * scale;
			tParticle->m_flDieTime		= random_valve->RandomFloat( 0.5f, 1.0f );

			FloatToColor32( tParticle->m_color, color[0], color[1], color[2], 1.0f );
		}
	}

	if ((flags & FX_BLOODSPRAY_GORE) || (flags & FX_BLOODSPRAY_CLOUD))
	{
		ks::intrusive_ptr<CBloodSprayEmitter> pSimple = CBloodSprayEmitter::Create( "bloodgore" );
		if ( !pSimple )
			return;

		pSimple->SetSortOrigin( origin );
		pSimple->SetGravity( 0 );

		PMaterialHandle	hMaterial;

		//
		// Tight blossom of blood at the center.
		//
		if (flags & FX_BLOODSPRAY_GORE)
		{
			hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_gore" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle 'scale' inches in diameter.
				offset = origin + ( 0.5 * scale * normal );
				offset += right * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;
				offset += up * random_valve->RandomFloat( -0.5f, 0.5f ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != nullptr )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= 0.3f;

					spread = 0.2f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += normal * random_valve->RandomInt( 10, 100 );
					//VectorNormalize( pParticle->m_vecVelocity );

					colorRamp = random_valve->RandomFloat( 0.75f, 1.25f );

					pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= std::min( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= std::min( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random_valve->RandomFloat( scale * 0.25, scale );
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2;
					
					pParticle->m_uchStartAlpha	= random_valve->RandomInt( 200, 255 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= 0.0f;
				}
			}
		}

		//
		// Diffuse cloud just in front of the exit wound.
		//
		if (flags & FX_BLOODSPRAY_CLOUD)
		{
			hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_puff" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle '2 * scale' inches in diameter.
				offset = origin + ( scale * normal );
				offset += right * random_valve->RandomFloat( -1, 1 ) * scale;
				offset += up * random_valve->RandomFloat( -1, 1 ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != nullptr )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 0.8f);

					spread = 0.5f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += normal * random_valve->RandomInt( 100, 200 );

					colorRamp = random_valve->RandomFloat( 0.75f, 1.25f );

					pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= std::min( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= std::min( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random_valve->RandomFloat( scale * 1.5f, scale * 2.0f );
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4;
					
					pParticle->m_uchStartAlpha	= random_valve->RandomInt( 80, 128 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= 0.0f;
				}
			}
		}
	}

	// TODO: Play a sound?
	//CLocalPlayerFilter filter;
	//CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, CHAN_VOICE, "Physics.WaterSplash", 1.0, ATTN_NORM, 0, 100, &origin );
}

//-----------------------------------------------------------------------------
// Purpose: Used for bullets hitting bleeding surfaces
// Input  : origin - 
//			normal - 
//			scale - This parameter is not currently used
//-----------------------------------------------------------------------------
void FX_BloodBulletImpact( const Vector &origin, const Vector &normal, float scale /*NOTE: Unused!*/, unsigned char r, unsigned char g, unsigned char b )
{
	if ( UTIL_IsLowViolence() )
		return;

	Vector offset;
	
	//Find area ambient light color and use it to tint smoke
	Vector worldLight = WorldGetLightForPoint( origin, true );
	
	if ( gpGlobals->maxClients > 1 )
	{
		worldLight = Vector( 1.0, 1.0, 1.0 );
		r = 96;
		g = 0;
		b = 10;
	}

	Vector color = Vector( (float)(worldLight[0] * r) / 255.0f, (float)(worldLight[1] * g) / 255.0f, (float)(worldLight[2] * b) / 255.0f );
	float colorRamp;

	Vector	offDir;

	ks::intrusive_ptr<CBloodSprayEmitter> pSimple = CBloodSprayEmitter::Create( "bloodgore" );
	if ( !pSimple )
		return;

	pSimple->SetSortOrigin( origin );
	pSimple->SetGravity( 200 );
	
	// Setup a bounding box to contain the particles without (stops auto-updating)
	pSimple->GetBinding().SetBBox( origin - Vector( 16, 16, 16 ), origin + Vector( 16, 16, 16 ) );

	// Cache the material if we haven't already
	if ( g_Blood_Core == nullptr )
	{
		g_Blood_Core = ParticleMgr()->GetPMaterial( "effects/blood_core" );
	}

	SimpleParticle *pParticle;

	Vector	dir = normal * RandomVector( -0.5f, 0.5f );

	offset = origin + ( 2.0f * normal );

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Blood_Core, offset );

	if ( pParticle != nullptr )
	{
		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= random_valve->RandomFloat( 0.25f, 0.5f);

		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 16.0f, 32.0f );
		pParticle->m_vecVelocity[2] -= random_valve->RandomFloat( 8.0f, 16.0f );

		colorRamp = random_valve->RandomFloat( 0.75f, 2.0f );

		pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= std::min( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= std::min( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= random_valve->RandomInt( 2, 4 );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 8;
	
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	// Cache the material if we haven't already
	if ( g_Blood_Gore == nullptr )
	{
		g_Blood_Gore = ParticleMgr()->GetPMaterial( "effects/blood_gore" );
	}

	for ( int i = 0; i < 4; i++ )
	{
		offset = origin + ( 2.0f * normal );

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Blood_Gore, offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 0.75f);

			pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 16.0f, 32.0f )*(i+1);
			pParticle->m_vecVelocity[2] -= random_valve->RandomFloat( 32.0f, 64.0f )*(i+1);

			colorRamp = random_valve->RandomFloat( 0.75f, 2.0f );

			pParticle->m_uchColor[0]	= std::min( 1.0f, color[0] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[1]	= std::min( 1.0f, color[1] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[2]	= std::min( 1.0f, color[2] * colorRamp ) * 255.0f;
			
			pParticle->m_uchStartSize	= random_valve->RandomInt( 2, 4 );
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4;
		
			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= 0.0f;
		}
	}

	//
	// Dump out drops
	//
	TrailParticle *tParticle;

	ks::intrusive_ptr<CTrailParticles> pTrailEmitter = CTrailParticles::Create( "blooddrops" );
	if ( !pTrailEmitter )
		return;

	pTrailEmitter->SetSortOrigin( origin );

	// Partial gravity on blood drops
	pTrailEmitter->SetGravity( 400.0 ); 
	
	// Enable simple collisions with nearby surfaces
	pTrailEmitter->Setup(origin, &normal, 1, 10, 100, 400, 0.2, 0 );

	if ( g_Blood_Drops == nullptr )
	{
		g_Blood_Drops = ParticleMgr()->GetPMaterial( "effects/blood_drop" );
	}

	//
	// Shorter droplets
	//
	for ( int i = 0; i < 8; i++ )
	{
		// Originate from within a circle 'scale' inches in diameter
		offset = origin;

		tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), g_Blood_Drops, offset );

		if ( tParticle == nullptr )
			break;

		tParticle->m_flLifetime	= 0.0f;

		offDir = RandomVector( -1.0f, 1.0f );

		tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( 64.0f, 128.0f );

		tParticle->m_flWidth		= random_valve->RandomFloat( 0.5f, 2.0f );
		tParticle->m_flLength		= random_valve->RandomFloat( 0.05f, 0.15f );
		tParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 0.5f );

		FloatToColor32( tParticle->m_color, color[0], color[1], color[2], 1.0f );
	}

	// TODO: Play a sound?
	//CLocalPlayerFilter filter;
	//CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, CHAN_VOICE, "Physics.WaterSplash", 1.0, ATTN_NORM, 0, 100, &origin );
}

// FIXME: This will be simplified when the initializer can take color parameters as an input
//	      For now, we use different systems

struct ParticleForBlood_t
{
	int nColor;
	const char *lpszParticleSystemName;
};

ParticleForBlood_t	bloodCallbacks[] =
{
	{ BLOOD_COLOR_RED,		"blood_impact_red_01" },
	{ BLOOD_COLOR_GREEN,	"blood_impact_green_01" },
	{ BLOOD_COLOR_YELLOW,	"blood_impact_yellow_01" },

};

//-----------------------------------------------------------------------------
// Purpose: Intercepts the blood spray message.
//-----------------------------------------------------------------------------
void BloodSprayCallback( const CEffectData &data )
{
	colorentry_t color;

	GetBloodColor( data.m_nColor, color );
	FX_BloodSpray( data.m_vOrigin, data.m_vNormal, data.m_flScale, color.r, color.g, color.b, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( bloodspray, BloodSprayCallback );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void BloodImpactCallback( const CEffectData & data )
{
	bool bFoundBlood = false;

	// Find which sort of blood we are
	for ( int i = 0; i < std::size( bloodCallbacks ); i++ )
	{
		if ( bloodCallbacks[i].nColor == data.m_nColor )
		{
			QAngle	vecAngles;
			VectorAngles( -data.m_vNormal, vecAngles );
			DispatchParticleEffect( bloodCallbacks[i].lpszParticleSystemName, data.m_vOrigin, vecAngles );
			bFoundBlood = true;
			break;
		}
	}

	if ( bFoundBlood == false )
	{
		Vector vecPosition;
		vecPosition = data.m_vOrigin;
		
		// Fetch the blood color.
		colorentry_t color;
		GetBloodColor( data.m_nColor, color );

		FX_BloodBulletImpact( vecPosition, data.m_vNormal, data.m_flScale, color.r, color.g, color.b );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( BloodImpact, BloodImpactCallback )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_red_01" )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_green_01" )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_yellow_01" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void HunterDamageCallback( const CEffectData &data )
{
	ks::intrusive_ptr<CSimple3DEmitter> pGlassEmitter = CSimple3DEmitter::Create( "HunterDamage" );
	if ( pGlassEmitter == nullptr )
		return;

	pGlassEmitter->SetSortOrigin( data.m_vOrigin );

	// Handle increased scale
	const float flMaxSpeed = 400.0f;
	const float flMinSpeed = 50.0f;
	float flAngularSpray = 1.0f;

	// Setup our collision information
	pGlassEmitter->m_ParticleCollision.Setup( data.m_vOrigin, &data.m_vNormal, flAngularSpray, flMinSpeed, flMaxSpeed, 600.0f, 0.2f );

	Vector	dir, end;

	int	numFlecks = 32;

	Particle3D *pFleckParticle;
	Vector spawnOffset;

	//Dump out flecks
	for ( int i = 0; i < numFlecks; i++ )
	{
		spawnOffset = data.m_vOrigin + RandomVector( -32.0f, 32.0f );
		pFleckParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), g_Mat_Fleck_Antlion[random_valve->RandomInt(0,1)], spawnOffset );

		if ( pFleckParticle == nullptr )
			break;

		pFleckParticle->m_flLifeRemaining	= random_valve->RandomFloat( 2.0f, 3.0f );

		dir[0] = data.m_vNormal[0] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[1] = data.m_vNormal[1] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[2] = data.m_vNormal[2] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );

		pFleckParticle->m_uchSize		= random_valve->RandomInt( 3, 8 );

		pFleckParticle->m_vecVelocity	= dir * random_valve->RandomFloat( flMinSpeed, flMaxSpeed);

		pFleckParticle->m_vAngles		= RandomAngle( 0, 360 );
		pFleckParticle->m_flAngSpeed	= random_valve->RandomFloat( -800, 800 );

		unsigned char color = 255;
		pFleckParticle->m_uchFrontColor[0]	= color;
		pFleckParticle->m_uchFrontColor[1]	= color;
		pFleckParticle->m_uchFrontColor[2]	= color;
		pFleckParticle->m_uchBackColor[0]	= color * 0.25f;
		pFleckParticle->m_uchBackColor[1]	= color * 0.25f;
		pFleckParticle->m_uchBackColor[2]	= color * 0.25f;
	}
}

DECLARE_CLIENT_EFFECT( HunterDamage, HunterDamageCallback );

float CBloodSprayEmitter::UpdateRoll( SimpleParticle *pParticle, float timeDelta)
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
	
	pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -4.0f );

	//Cap the minimum roll
	/*
	if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
	{
		pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
	}
	*/

	return pParticle->m_flRoll;
}

void CBloodSprayEmitter::UpdateVelocity( SimpleParticle *pParticle, float timeDelta)
{
	if ( !( pParticle->m_iFlags & SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY ) )
	{
		//Decelerate
		static float dtime;
		static float decay;

		if ( dtime != timeDelta )
		{
			decay = ExponentialDecay( 0.1, 0.4f, dtime );
			dtime = timeDelta;
		}

		pParticle->m_vecVelocity *= decay;
		pParticle->m_vecVelocity[2] -= ( m_flGravity * timeDelta );
	}
}

using namespace ::ks::reflect;

/*
==================================================
CFXLine
==================================================
*/
CFXDiscreetLine::CFXDiscreetLine( const char *name, const Vector& start, const Vector& direction, 
	float velocity, float length, float clipLength, float scale, float life, const char *shader )
: CClientSideEffect( name )
{
	contract_assert( materials );
	if ( materials == nullptr )
		return;

	// Create a material...
	m_pMaterial = materials->FindMaterial( shader, TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pMaterial->IncrementReferenceCount();

	m_vecOrigin			= start;
	m_vecDirection		= direction;
	m_fVelocity			= velocity;
	m_fClipLength		= clipLength;
	m_fScale			= scale;
	m_fLife				= life;
	m_fStartTime		= 0.0f;
	m_fLength			= length;
}

CFXDiscreetLine::~CFXDiscreetLine( void )
{
	Destroy();
}

// Does extra calculations to make them more visible over distance
ConVar	tracer_extra( side_registry(), "tracer_extra", "1" );

/*
==================================================
Draw
==================================================
*/

void CFXDiscreetLine::Draw( double frametime )
{
	Vector			lineDir, viewDir, cross;

	Vector			vecEnd, vecStart;
	Vector tmp;

	// Update the effect
	Update( frametime );

	// Calculate our distance along our path
	float	sDistance = m_fVelocity * m_fStartTime;
	float	eDistance = sDistance - m_fLength;
	
	//Clip to start
	sDistance = std::max( 0.0f, sDistance );
	eDistance = std::max( 0.0f, eDistance );

	if ( ( sDistance == 0.0f ) && ( eDistance == 0.0f ) )
		return;

	// Clip it
	if ( m_fClipLength != 0.0f )
	{
		sDistance = std::min( sDistance, m_fClipLength );
		eDistance = std::min( eDistance, m_fClipLength );
	}

	// Get our delta to calculate the tc offset
	float	dDistance	= fabs( sDistance - eDistance );
	float	dTotal		= ( m_fLength != 0.0f ) ? m_fLength : 0.01f;
	float	fOffset		= ( dDistance / dTotal );

	// Find our points along our path
	VectorMA( m_vecOrigin, sDistance, m_vecDirection, vecEnd );
	VectorMA( m_vecOrigin, eDistance, m_vecDirection, vecStart );

	//Setup our info for drawing the line
	VectorSubtract( vecEnd, vecStart, lineDir );
	VectorSubtract( vecEnd, CurrentViewOrigin(), viewDir );
	
	cross = lineDir.Cross( viewDir );
	VectorNormalize( cross );

	CMeshBuilder meshBuilder;
	IMesh *pMesh;

	CMatRenderContextPtr pRenderContext( materials );
		
	// Better, more visible tracers
	if ( tracer_extra.GetBool() )
	{
		float flScreenWidth = ScreenWidth();
		float flHalfScreenWidth = flScreenWidth * 0.5f;
		
		float zCoord = CurrentViewForward().Dot( vecStart - CurrentViewOrigin() );
		float flScreenSpaceWidth = m_fScale * flHalfScreenWidth / zCoord;

		float flAlpha;
		float flScale;

		if ( flScreenSpaceWidth < 0.5f )
		{
			flAlpha = RemapVal( flScreenSpaceWidth, 0.25f, 2.0f, 0.3f, 1.0f );
			flAlpha = clamp( flAlpha, 0.25f, 1.0f );
			flScale = 0.5f * zCoord / flHalfScreenWidth;
		}
		else
		{
			flAlpha = 1.0f;
			flScale = m_fScale;
		}

		//Bind the material
		pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_pMaterial );

		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 2 );

		float color = (int) 255.0f * flAlpha;

		//FIXME: for now no coloration
		VectorMA( vecStart, -flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecStart,  flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, fOffset );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, -flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, fOffset );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		flScale = flScale * 2.0f;
		color = (int) 64.0f * flAlpha;

		// Soft outline
		VectorMA( vecStart, -flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecStart,  flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, fOffset );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, -flScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, fOffset );
		meshBuilder.Color4ub( color, color, color, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();
	}
	else
	{
		//Bind the material
		pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_pMaterial );

		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		//FIXME: for now no coloration
		VectorMA( vecStart, -m_fScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecStart,  m_fScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, m_fScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 0.0f, fOffset );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();

		VectorMA( vecEnd, -m_fScale, cross, tmp );
		meshBuilder.Position3fv( tmp.Base() );
		meshBuilder.TexCoord2f( 0, 1.0f, fOffset );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.Normal3fv( cross.Base() );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

/*
==================================================
IsActive
==================================================
*/

bool CFXDiscreetLine::IsActive( void )
{
	return ( m_fLife > 0.0 ) ? true : false;
}

/*
==================================================
Destroy
==================================================
*/

void CFXDiscreetLine::Destroy( void )
{
	//Release the material
	if ( m_pMaterial != nullptr )
	{
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial = nullptr;
	}
}

/*
==================================================
Update
==================================================
*/

void CFXDiscreetLine::Update( double frametime )
{
	m_fStartTime += frametime;
	m_fLife -= frametime;

	//Move our end points
	//VectorMA( m_vecStart, frametime, m_vecStartVelocity, m_vecStart );
	//VectorMA( m_vecEnd, frametime, m_vecStartVelocity, m_vecEnd );
}

class CEffectData;
class CParticleSubTexture;
class CBaseEntity;
struct FXLineData_t;
class SimpleParticle;

using namespace ::ks::reflect;

// Purpose: Base explosion effect

#define	__EXPLOSION_DEBUG	0

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectExplosion )
	PRECACHE( MATERIAL, "effects/fire_cloud1" )
	PRECACHE( MATERIAL, "effects/fire_cloud2" )
	PRECACHE( MATERIAL, "effects/fire_embers1" )
	PRECACHE( MATERIAL, "effects/fire_embers2" )
	PRECACHE( MATERIAL, "effects/fire_embers3" )
	PRECACHE( MATERIAL, "effects/splash3" )
	PRECACHE( MATERIAL, "effects/splashwake1" )
	PRECACHE( MATERIAL, "particle/particle_smokegrenade" )
	PRECACHE( MATERIAL, "particle/particle_smokegrenade1" )
PRECACHE_REGISTER_END()

//
// CExplosionParticle
//

class CExplosionParticle : public CSimpleEmitter
{
public:
	
	CExplosionParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CExplosionParticle *Create( const char *pDebugName )
	{
		return new CExplosionParticle( pDebugName );
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

	//Velocity
	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		Vector	saveVelocity = pParticle->m_vecVelocity;

		//Decellerate
		//pParticle->m_vecVelocity += pParticle->m_vecVelocity * ( timeDelta * -20.0f );
		static float dtime;
		static float decay;

		if ( dtime != timeDelta )
		{
			dtime = timeDelta;
			float expected = 0.5;
			decay = std::exp( std::log( 0.0001f ) * dtime / expected );
		}

		pParticle->m_vecVelocity = pParticle->m_vecVelocity * decay;

		//Cap the minimum speed
		if ( pParticle->m_vecVelocity.LengthSqr() < (32.0f*32.0f) )
		{
			VectorNormalize( saveVelocity );
			pParticle->m_vecVelocity = saveVelocity * 32.0f;
		}
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		float	tLifetime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float	ramp = 1.0f - tLifetime;

		return Bias( ramp, 0.25f );
	}

	//Color
	virtual Vector UpdateColor( const SimpleParticle *pParticle )
	{
		Vector	color;

		float	tLifetime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float	ramp = Bias( 1.0f - tLifetime, 0.25f );

		color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
		color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
		color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

		return color;
	}

private:
	CExplosionParticle( const CExplosionParticle & );
};

//Singleton static member definition
CBaseExplosionEffect	CBaseExplosionEffect::m_instance;

CBaseExplosionEffect::CBaseExplosionEffect( void ) : m_Material_Smoke( nullptr ), m_Material_FireCloud( nullptr )
{
	m_Material_Embers[0] = nullptr;
	m_Material_Embers[1] = nullptr;
}

//Singleton accessor
CBaseExplosionEffect &BaseExplosionEffect( void )
{ 
	return CBaseExplosionEffect::Instance(); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &deviant - 
//			&source - 
// Output : float
//-----------------------------------------------------------------------------
float CBaseExplosionEffect::ScaleForceByDeviation( Vector &deviant, Vector &source, float spread, float *force )
{
	if ( ( deviant == vec3_origin ) || ( source == vec3_origin ) )
		return 1.0f;

	float	dot = source.Dot( deviant );
	
	dot = spread * fabs( dot );	

	if ( force != nullptr )
	{
		(*force) *= dot;
	}

	return dot;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : position - 
//			force - 
// Output : virtual void
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::Create( const Vector &position, float force, float scale, int flags )
{
	m_vecOrigin = position;
	m_fFlags	= flags;

	//if explosion is an ice explosion skip all the other stuff and draw a particle system
	if ( m_fFlags & TE_EXPLFLAG_ICE )
	{
		QAngle	vecAngles;
		//DispatchParticleEffect( "freeze_explosion", m_vecOrigin , vecAngles );
		PlaySound();
		return;
	}

	if ( m_fFlags & TE_EXPLFLAG_SCALEPARTICLES )
	{
		m_flScale = scale;
	}
	else
	{
		m_flScale = 1.0f;
	}

	//Find the force of the explosion
	GetForceDirection( m_vecOrigin, force, &m_vecDirection, &m_flForce );

#if __EXPLOSION_DEBUG
	debugoverlay->AddBoxOverlay( m_vecOrigin, -Vector(32,32,32), Vector(32,32,32), vec3_angle, 255, 0, 0, 64, 5.0f );
	debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin+(m_vecDirection*force*m_flForce), 0, 0, 255, false, 3 );
#endif

	PlaySound();

	if ( scale != 0 )
	{
		// UNDONE: Make core size parametric to scale or remove scale?
		CreateCore();
	}

	CreateDebris();
	CreateDynamicLight();
	CreateMisc();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::CreateCore( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOFIREBALL )
		return;

	if ( m_fFlags & TE_EXPLFLAG_ICE )
		return;
	
	Vector	offset;
	int		i;

	//Spread constricts as force rises
	float force = m_flForce;

	//Cap our force
	if ( force < EXPLOSION_FORCE_MIN )
		force = EXPLOSION_FORCE_MIN;
	
	if ( force > EXPLOSION_FORCE_MAX )
		force = EXPLOSION_FORCE_MAX;

	float spread = 1.0f - (0.15f*force);

	SimpleParticle	*pParticle;

	ks::intrusive_ptr<CExplosionParticle> pSimple = CExplosionParticle::Create( "exp_smoke" );
	pSimple->SetSortOrigin( m_vecOrigin );
	pSimple->SetNearClip( 64, 128 );

	pSimple->GetBinding().SetBBox( m_vecOrigin - Vector( 128, 128, 128 ), m_vecOrigin + Vector( 128, 128, 128 ) );
	
	if ( m_Material_Smoke == nullptr )
	{
		m_Material_Smoke = g_Mat_DustPuff[1];
	}

	//FIXME: Better sampling area
	offset = m_vecOrigin + ( m_vecDirection * 32.0f );

	//Find area ambient light color and use it to tint smoke
	Vector worldLight = WorldGetLightForPoint( offset, true );
	
	Vector	tint;
	float	luminosity;
	if ( worldLight == vec3_origin )
	{
		tint = vec3_origin;
		luminosity = 0.0f;
	}
	else
	{
		UTIL_GetNormalizedColorTintAndLuminosity( worldLight, &tint, &luminosity );
	}

	// We only take a portion of the tint
	tint = (tint * 0.25f)+(Vector(0.75f,0.75f,0.75f));
	
	// Rescale to a character range
	luminosity *= 255;

	if ( (m_fFlags & TE_EXPLFLAG_NOFIREBALLSMOKE) == 0 )
	{
		//
		// Smoke - basic internal filler
		//

		for ( i = 0; i < 4; i++ )
		{
			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Smoke, m_vecOrigin );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime = 0.0f;

				pParticle->m_flDieTime	= random_valve->RandomFloat( 2.0f, 3.0f );

				pParticle->m_vecVelocity.Random( -spread, spread );
				pParticle->m_vecVelocity += ( m_vecDirection * random_valve->RandomFloat( 1.0f, 6.0f ) );
				
				VectorNormalize( pParticle->m_vecVelocity );

				float	fForce = random_valve->RandomFloat( 1, 750 ) * force;

				//Scale the force down as we fall away from our main direction
				ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread, &fForce );

				pParticle->m_vecVelocity *= fForce;
				pParticle->m_vecVelocity *= m_flScale;
				
				#if __EXPLOSION_DEBUG
				debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
				#endif

				int nColor = random_valve->RandomInt( luminosity*0.5f, luminosity );
				pParticle->m_uchColor[0] = ( worldLight[0] * nColor );
				pParticle->m_uchColor[1] = ( worldLight[1] * nColor );
				pParticle->m_uchColor[2] = ( worldLight[2] * nColor );
				
				pParticle->m_uchStartSize	= 72 * m_flScale;
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2;
				
				pParticle->m_uchStartAlpha	= 255;
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );
			}
		}

		//
		// Inner core
		//

		for ( i = 0; i < 8; i++ )
		{
			offset.Random( -16.0f, 16.0f );
			offset *= m_flScale;
			offset += m_vecOrigin;

			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Smoke, offset );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime = 0.0f;

				pParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 1.0f );

				pParticle->m_vecVelocity.Random( -spread, spread );
				pParticle->m_vecVelocity += ( m_vecDirection * random_valve->RandomFloat( 1.0f, 6.0f ) );
				
				VectorNormalize( pParticle->m_vecVelocity );

				float	fForce = random_valve->RandomFloat( 1, 2000 ) * force;

				//Scale the force down as we fall away from our main direction
				ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread, &fForce );

				pParticle->m_vecVelocity *= fForce;
				pParticle->m_vecVelocity *= m_flScale;
				
				#if __EXPLOSION_DEBUG
				debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
				#endif

				int nColor = random_valve->RandomInt( luminosity*0.5f, luminosity );
				pParticle->m_uchColor[0] = ( worldLight[0] * nColor );
				pParticle->m_uchColor[1] = ( worldLight[1] * nColor );
				pParticle->m_uchColor[2] = ( worldLight[2] * nColor );
						
				pParticle->m_uchStartSize	= random_valve->RandomInt( 32, 64 ) * m_flScale;
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2;

				pParticle->m_uchStartAlpha	= random_valve->RandomFloat( 128, 255 );
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
			}
		}

		//
		// Ground ring
		//

		Vector	vRight, vUp;
		VectorVectors( m_vecDirection, vRight, vUp );

		Vector	forward;

		int	numRingSprites = 32;

		float flIncr = (2*std::numbers::pi) / (float) numRingSprites; // Radians
		float flYaw = 0.0f;

		for ( i = 0; i < numRingSprites; i++ )
		{
			flYaw += flIncr;
			SinCos( flYaw, &forward.y, &forward.x );
			forward.z = 0.0f;

			offset = ( ( RandomVector( -4.0f, 4.0f ) * m_flScale ) + m_vecOrigin ) + ( m_flScale * forward * random_valve->RandomFloat( 8.0f, 16.0f ) );

			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Smoke, offset );

			if ( pParticle != nullptr )
			{
				pParticle->m_flLifetime = 0.0f;
				pParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 1.5f );

				pParticle->m_vecVelocity = forward;
			
				float	fForce = random_valve->RandomFloat( 500, 2000 ) * force;

				//Scale the force down as we fall away from our main direction
				ScaleForceByDeviation( pParticle->m_vecVelocity, pParticle->m_vecVelocity, spread, &fForce );

				pParticle->m_vecVelocity *= fForce;
				pParticle->m_vecVelocity *= m_flScale;
				
				#if __EXPLOSION_DEBUG
				debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
				#endif

				int nColor = random_valve->RandomInt( luminosity*0.5f, luminosity );
				pParticle->m_uchColor[0] = ( worldLight[0] * nColor );
				pParticle->m_uchColor[1] = ( worldLight[1] * nColor );
				pParticle->m_uchColor[2] = ( worldLight[2] * nColor );

				pParticle->m_uchStartSize	= random_valve->RandomInt( 16, 32 ) * m_flScale;
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4;

				pParticle->m_uchStartAlpha	= random_valve->RandomFloat( 16, 32 );
				pParticle->m_uchEndAlpha	= 0;
				
				pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
			}
		}
	}

	//
	// Embers
	//

	if ( m_Material_Embers[0] == nullptr )
	{
		m_Material_Embers[0] = pSimple->GetPMaterial( "effects/fire_embers1" );
	}

	if ( m_Material_Embers[1] == nullptr )
	{
		m_Material_Embers[1] = pSimple->GetPMaterial( "effects/fire_embers2" );
	}

	for ( i = 0; i < 16; i++ )
	{
		offset.Random( -32.0f, 32.0f );
		offset *= m_flScale;
		offset += m_vecOrigin;

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Embers[random_valve->RandomInt(0,1)], offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random_valve->RandomFloat( 2.0f, 3.0f );

			pParticle->m_vecVelocity.Random( -spread*2, spread*2 );
			pParticle->m_vecVelocity += m_vecDirection;
			
			VectorNormalize( pParticle->m_vecVelocity );

			float	fForce = random_valve->RandomFloat( 1.0f, 400.0f );

			//Scale the force down as we fall away from our main direction
			float	vDev = ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread );

			pParticle->m_vecVelocity *= fForce * ( 16.0f * (vDev*vDev*0.5f) );
			pParticle->m_vecVelocity *= m_flScale;
			
			#if __EXPLOSION_DEBUG
			debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
			#endif

			int nColor = random_valve->RandomInt( 192, 255 );
			pParticle->m_uchColor[0]	= pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = nColor;
			
			pParticle->m_uchStartSize	= random_valve->RandomInt( 8, 16 ) * vDev;

			pParticle->m_uchStartSize	= clamp( pParticle->m_uchStartSize, 4, 32 ) * m_flScale;

			pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
			
			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
		}
	}

	//
	// Fireballs
	//

	if ( m_Material_FireCloud == nullptr )
	{
		m_Material_FireCloud = pSimple->GetPMaterial( "effects/fire_cloud2" );
	}

	int numFireballs = 32;

	for ( i = 0; i < numFireballs; i++ )
	{
		offset.Random( -48.0f, 48.0f );
		offset *= m_flScale;
		offset += m_vecOrigin;

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_FireCloud, offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.2f, 0.4f );

			pParticle->m_vecVelocity.Random( -spread*0.75f, spread*0.75f );
			pParticle->m_vecVelocity += m_vecDirection;
			
			VectorNormalize( pParticle->m_vecVelocity );

			float	fForce = random_valve->RandomFloat( 400.0f, 800.0f );

			//Scale the force down as we fall away from our main direction
			float	vDev = ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread );

			pParticle->m_vecVelocity *= fForce * ( 16.0f * (vDev*vDev*0.5f) );
			pParticle->m_vecVelocity *= m_flScale;

			#if __EXPLOSION_DEBUG
			debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
			#endif

			int nColor = random_valve->RandomInt( 128, 255 );
			pParticle->m_uchColor[0]	= pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = nColor;
			
			pParticle->m_uchStartSize	= random_valve->RandomInt( 32, 85 ) * vDev;

			pParticle->m_uchStartSize	= clamp( pParticle->m_uchStartSize, 32, 85 ) * m_flScale;

			pParticle->m_uchEndSize		= (int)((float)pParticle->m_uchStartSize * 1.5f);
			
			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -16.0f, 16.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::CreateDebris( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOPARTICLES )
		return;

	//
	// Sparks
	//

	ks::intrusive_ptr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "CreateDebris 1" );
	if ( pSparkEmitter == nullptr )
	{
		contract_assert( 0 );
		return;
	}

	if ( m_Material_FireCloud == nullptr )
	{
		m_Material_FireCloud = pSparkEmitter->GetPMaterial( "effects/fire_cloud2" );
	}

	pSparkEmitter->SetSortOrigin( m_vecOrigin );
	
	pSparkEmitter->m_ParticleCollision.SetGravity( 200.0f );
	pSparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	pSparkEmitter->SetVelocityDampen( 8.0f );
	
	// Set our bbox, don't auto-calculate it!
	pSparkEmitter->GetBinding().SetBBox( m_vecOrigin - Vector( 128, 128, 128 ), m_vecOrigin + Vector( 128, 128, 128 ) );

	int		numSparks = random_valve->RandomInt( 8, 16 );

	Vector	dir;
	float	spread = 1.0f;
	TrailParticle	*tParticle;

	// Dump out sparks
	int i;
	for ( i = 0; i < numSparks; i++ )
	{
		tParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), m_Material_FireCloud, m_vecOrigin );

		if ( tParticle == nullptr )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random_valve->RandomFloat( 0.1f, 0.15f );

		dir.Random( -spread, spread );
		dir += m_vecDirection;
		VectorNormalize( dir );
		
		tParticle->m_flWidth		= random_valve->RandomFloat( 2.0f, 16.0f ) * m_flScale;
		tParticle->m_flLength		= random_valve->RandomFloat( 0.05f, 0.1f ) * m_flScale;
		
		tParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 1500, 2500 );
		tParticle->m_vecVelocity *= m_flScale;

		Color32Init( tParticle->m_color, 255, 255, 255, 255 );
	}

	//
	// Chunks
	//

	Vector	offset;
	ks::intrusive_ptr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "CreateDebris 2", m_vecOrigin, Vector(128,128,128) * m_flScale );
	if ( !fleckEmitter )
		return;

	// Setup our collision information
	fleckEmitter->m_ParticleCollision.Setup( m_vecOrigin, &m_vecDirection, 0.9f, 512, 1024, 800, 0.5f );
	
	int	numFlecks = random_valve->RandomInt( 16, 32 );

	// Dump out flecks
	for ( i = 0; i < numFlecks; i++ )
	{
		offset = ( m_vecDirection * 16.0f );

		offset[0] += random_valve->RandomFloat( -8.0f, 8.0f );
		offset[1] += random_valve->RandomFloat( -8.0f, 8.0f );
		offset[2] += random_valve->RandomFloat( -8.0f, 8.0f );
		offset *= m_flScale;
		offset += m_vecOrigin;

		FleckParticle *pParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), g_Mat_Fleck_Cement[random_valve->RandomInt(0,1)], offset );

		if ( pParticle == nullptr )
			break;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 3.0f;
		
		dir[0] = m_vecDirection[0] + random_valve->RandomFloat( -1.0f, 1.0f );
		dir[1] = m_vecDirection[1] + random_valve->RandomFloat( -1.0f, 1.0f );
		dir[2] = m_vecDirection[2] + random_valve->RandomFloat( -1.0f, 1.0f );

		pParticle->m_uchSize		= random_valve->RandomInt( 1, 3 ) * m_flScale;

		VectorNormalize( dir );

		float	fForce = ( random_valve->RandomFloat( 64, 256 ) * ( 4 - pParticle->m_uchSize ) );

		float	fDev = ScaleForceByDeviation( dir, m_vecDirection, 0.8f );

		pParticle->m_vecVelocity = dir * ( fForce * ( 16.0f * (fDev*fDev*0.5f) ) );
		pParticle->m_vecVelocity *= m_flScale;

		pParticle->m_flRoll			= random_valve->RandomFloat( 0, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( 0, 360 );

		float colorRamp = random_valve->RandomFloat( 0.5f, 1.5f );
		pParticle->m_uchColor[0] = std::min( 1.0f, 0.25f*colorRamp )*255.0f;
		pParticle->m_uchColor[1] = std::min( 1.0f, 0.25f*colorRamp )*255.0f;
		pParticle->m_uchColor[2] = std::min( 1.0f, 0.25f*colorRamp )*255.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::CreateMisc( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::CreateDynamicLight( void )
{
	if ( m_fFlags & TE_EXPLFLAG_DLIGHT )
	{
		dlight_t *dl = effects->CL_AllocDlight( 0 );
		
		VectorCopy (m_vecOrigin, dl->origin);
		
		dl->decay	= 200;
		dl->radius	= 255 * m_flScale;
		dl->color.r = 255;
		dl->color.g = 220;
		dl->color.b = 128;
		dl->die		= gpGlobals->curtime + 0.1f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::PlaySound( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOSOUND )
		return;

	CLocalPlayerFilter filter;

	if ( m_fFlags & TE_EXPLFLAG_ICE )
	{
		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "explode_3", &m_vecOrigin );
		return;
	}
	
	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "BaseExplosionEffect.Sound", &m_vecOrigin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			&m_vecDirection - 
//			strength - 
// Output : float
//-----------------------------------------------------------------------------
float CBaseExplosionEffect::Probe( const Vector &origin, Vector *vecDirection, float strength )
{
	//Press out
	Vector endpos = origin + ( (*vecDirection) * strength );

	//Trace into the world
	trace_t	tr;
	UTIL_TraceLine( origin, endpos, CONTENTS_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );

	//Push back a proportional amount to the probe
	(*vecDirection) = -(*vecDirection) * (1.0f-tr.fraction);

#if __EXPLOSION_DEBUG
	debugoverlay->AddLineOverlay( m_vecOrigin, endpos, (255*(1.0f-tr.fraction)), (255*tr.fraction), 0, false, 3 );
#endif

	contract_assert( ( 1.0f - tr.fraction ) >= 0.0f );

	//Return the impacted proportion of the probe
	return (1.0f-tr.fraction);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			&m_vecDirection - 
//			&m_flForce - 
//-----------------------------------------------------------------------------
void CBaseExplosionEffect::GetForceDirection( const Vector &origin, float magnitude, Vector *resultDirection, float *resultForce )
{
	Vector	d[6];

	//All cardinal directions
	d[0] = Vector(  1,  0,  0 );
	d[1] = Vector( -1,  0,  0 );
	d[2] = Vector(  0,  1,  0 );
	d[3] = Vector(  0, -1,  0 );
	d[4] = Vector(  0,  0,  1 );
	d[5] = Vector(  0,  0, -1 );

	//Init the results
	(*resultDirection).Init();
	(*resultForce) = 1.0f;
	
	//Get the aggregate force vector
	for ( int i = 0; i < 6; i++ )
	{
		(*resultForce) += Probe( origin, &d[i], magnitude );
		(*resultDirection) += d[i];
	}

	//If we've hit nothing, then point up
	if ( (*resultDirection) == vec3_origin )
	{
		(*resultDirection) = Vector( 0, 0, 1 );
		(*resultForce) = EXPLOSION_FORCE_MIN;
	}

	//Just return the direction
	VectorNormalize( (*resultDirection) );
}

//-----------------------------------------------------------------------------
// Purpose: Intercepts the water explosion dispatch effect
//-----------------------------------------------------------------------------
void ExplosionCallback( const CEffectData &data )
{
	BaseExplosionEffect().Create( data.m_vOrigin, data.m_flMagnitude, data.m_flScale, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( Explosion, ExplosionCallback );

//===============================================================================================================
// Water Explosion
//===============================================================================================================
//
// CExplosionParticle
//

class CWaterExplosionParticle : public CSimpleEmitter
{
public:
	
	CWaterExplosionParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CWaterExplosionParticle *Create( const char *pDebugName )
	{
		return new CWaterExplosionParticle( pDebugName );
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -8.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.25f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.25f : -0.25f;
		}

		return pParticle->m_flRoll;
	}

	//Velocity
	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		Vector	saveVelocity = pParticle->m_vecVelocity;

		//Decellerate
		//pParticle->m_vecVelocity += pParticle->m_vecVelocity * ( timeDelta * -20.0f );
		static float dtime;
		static float decay;

		if ( dtime != timeDelta )
		{
			dtime = timeDelta;
			float expected = 0.5;
			decay = std::exp( std::log( 0.0001f ) * dtime / expected );
		}

		pParticle->m_vecVelocity = pParticle->m_vecVelocity * decay;

		//Cap the minimum speed
		if ( pParticle->m_vecVelocity.LengthSqr() < (8.0f*8.0f) )
		{
			VectorNormalize( saveVelocity );
			pParticle->m_vecVelocity = saveVelocity * 8.0f;
		}
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		float	tLifetime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float	ramp = 1.0f - tLifetime;

		//Non-linear fade
		if ( ramp < 0.75f )
			ramp *= ramp;

		return ramp;
	}

private:
	CWaterExplosionParticle( const CWaterExplosionParticle & );
};

//Singleton static member definition
CWaterExplosionEffect	CWaterExplosionEffect::m_waterinstance;

//Singleton accessor
CWaterExplosionEffect &WaterExplosionEffect( void )
{ 
	return CWaterExplosionEffect::Instance(); 
}

static constexpr auto MAX_WATER_SURFACE_DISTANCE = 512;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterExplosionEffect::Create( const Vector &position, float force, float scale, int flags )
{
	m_vecOrigin = position;

	// Find our water surface by tracing up till we're out of the water
	trace_t tr;
	Vector vecTrace( 0, 0, MAX_WATER_SURFACE_DISTANCE );
	UTIL_TraceLine( m_vecOrigin, m_vecOrigin + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );
	
	// If we didn't start in water, we're above it
	if ( tr.startsolid == false )
	{
		// Look downward to find the surface
		vecTrace.Init( 0, 0, -MAX_WATER_SURFACE_DISTANCE );
		UTIL_TraceLine( m_vecOrigin, m_vecOrigin + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );

		// If we hit it, setup the explosion
		if ( tr.fraction < 1.0f )
		{
			m_vecWaterSurface = tr.endpos;
			m_flDepth = 0.0f;
		}
		else
		{
			//NOTENOTE: We somehow got into a water explosion without being near water?
			contract_assert( 0 );
			m_vecWaterSurface = m_vecOrigin;
			m_flDepth = 0.0f;
		}
	}
	else if ( tr.fractionleftsolid )
	{
		// Otherwise we came out of the water at this point
		m_vecWaterSurface = m_vecOrigin + (vecTrace * tr.fractionleftsolid);
		m_flDepth = MAX_WATER_SURFACE_DISTANCE * tr.fractionleftsolid;
	}
	else
	{
		// Use default values, we're really deep
		m_vecWaterSurface = m_vecOrigin;
		m_flDepth = MAX_WATER_SURFACE_DISTANCE;
	}

	// Get our lighting information
	FX_GetSplashLighting( m_vecOrigin + Vector( 0, 0, 32 ), &m_vecColor, &m_flLuminosity );

	BaseClass::Create( position, force, scale, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterExplosionEffect::CreateCore( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOFIREBALL )
		return;

	// Get our lighting information for the water surface
	Vector	color;
	float	luminosity;
	FX_GetSplashLighting( m_vecWaterSurface + Vector( 0, 0, 8 ), &color, &luminosity );

	float lifetime = random_valve->RandomFloat( 0.8f, 1.0f );

	// Ground splash
	FX_AddQuad( m_vecWaterSurface + Vector(0,0,2), 
				Vector(0,0,1), 
				64, 
				64 * 4.0f,
				0.85f, 
				luminosity,
				0.0f,
				0.25f,
				random_valve->RandomInt( 0, 360 ), 
				random_valve->RandomFloat( -4, 4 ), 
				color,
				2.0f,
				"effects/splashwake1",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	Vector	vRight, vUp;
	VectorVectors( Vector(0,0,1) , vRight, vUp );

	Vector	start, end;
	
	float radius = 50.0f;

	unsigned int flags = 0;

	// Base vertical shaft
	FXLineData_t lineData;

	start = m_vecWaterSurface;
	end = start + ( Vector( 0, 0, 1 ) * random_valve->RandomFloat( radius, radius*1.5f ) );

	if ( random_valve->RandomInt( 0, 1 ) )
	{
		flags |= FXSTATICLINE_FLIP_HORIZONTAL;
	}
	else
	{
		flags = 0;
	}

	lineData.m_flDieTime = lifetime * 0.5f;
	
	lineData.m_flStartAlpha= luminosity;
	lineData.m_flEndAlpha = 0.0f;
	
	lineData.m_flStartScale = radius*0.5f;
	lineData.m_flEndScale = radius*2; 

	lineData.m_pMaterial = materials->FindMaterial( "effects/splash3", 0, 0 );

	lineData.m_vecStart = start;
	lineData.m_vecStartVelocity = vec3_origin;

	lineData.m_vecEnd = end;
	lineData.m_vecEndVelocity = Vector(0,0,random_valve->RandomFloat( 650, 750 ));

	FX_AddLine( lineData );

	// Inner filler shaft
	start = m_vecWaterSurface;
	end = start + ( Vector(0,0,1) * random_valve->RandomFloat( 32, 64 ) );

	if ( random_valve->RandomInt( 0, 1 ) )
	{
		flags |= FXSTATICLINE_FLIP_HORIZONTAL;
	}
	else
	{
		flags = 0;
	}

	lineData.m_flDieTime = lifetime * 0.5f;
	
	lineData.m_flStartAlpha= luminosity;
	lineData.m_flEndAlpha = 0.0f;
	
	lineData.m_flStartScale = radius;
	lineData.m_flEndScale = radius*2; 

	lineData.m_pMaterial = materials->FindMaterial( "effects/splash3", 0, 0 );

	lineData.m_vecStart = start;
	lineData.m_vecStartVelocity = vec3_origin;

	lineData.m_vecEnd = end;
	lineData.m_vecEndVelocity = Vector(0,0,1) * random_valve->RandomFloat( 64, 128 );

	FX_AddLine( lineData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterExplosionEffect::CreateDebris( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOPARTICLES )
		return;

	// Must be in deep enough water
	if ( m_flDepth <= 128 )
		return;

	Vector	offset;
	int		i;

	//Spread constricts as force rises
	float force = m_flForce;

	//Cap our force
	if ( force < EXPLOSION_FORCE_MIN )
		force = EXPLOSION_FORCE_MIN;
	
	if ( force > EXPLOSION_FORCE_MAX )
		force = EXPLOSION_FORCE_MAX;

	float spread = 1.0f - (0.15f*force);

	SimpleParticle	*pParticle;

	ks::intrusive_ptr<CWaterExplosionParticle> pSimple = CWaterExplosionParticle::Create( "waterexp_bubbles" );
	pSimple->SetSortOrigin( m_vecOrigin );
	pSimple->SetNearClip( 64, 128 );

	//FIXME: Better sampling area
	offset = m_vecOrigin + ( m_vecDirection * 64.0f );

	//Find area ambient light color and use it to tint bubbles
	Vector	worldLight;
	FX_GetSplashLighting( offset, &worldLight, nullptr );

	//
	// Smoke
	//

	CParticleSubTexture *pMaterial[2];

	pMaterial[0] = pSimple->GetPMaterial( "effects/splash1" );
	pMaterial[1] = pSimple->GetPMaterial( "effects/splash2" );

	for ( i = 0; i < 16; i++ )
	{
		offset.Random( -32.0f, 32.0f );
		offset += m_vecOrigin;

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pMaterial[random_valve->RandomInt(0,1)], offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;

			pParticle->m_flDieTime	= random_valve->RandomFloat( 2.0f, 3.0f );

			pParticle->m_vecVelocity.Random( -spread, spread );
			pParticle->m_vecVelocity += ( m_vecDirection * random_valve->RandomFloat( 1.0f, 6.0f ) );
			
			VectorNormalize( pParticle->m_vecVelocity );

			float	fForce = 1500 * force;

			//Scale the force down as we fall away from our main direction
			ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread, &fForce );

			pParticle->m_vecVelocity *= fForce;
			
			#if __EXPLOSION_DEBUG
			debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
			#endif

			pParticle->m_uchColor[0] = m_vecColor.x * 255;
			pParticle->m_uchColor[1] = m_vecColor.y * 255;
			pParticle->m_uchColor[2] = m_vecColor.z * 255;
			
			pParticle->m_uchStartSize	= random_valve->RandomInt( 32, 64 );
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2;
			
			pParticle->m_uchStartAlpha	= m_flLuminosity;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -8.0f, 8.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterExplosionEffect::CreateMisc( void )
{
	Vector	offset;
	float	colorRamp;
	
	int i;
	float	flScale = 2.0f;

	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/splash2" );

	int		numDrops = 32;
	float	length = 0.1f;
	Vector	vForward, vRight, vUp;
	Vector	offDir;

	TrailParticle	*tParticle;

	ks::intrusive_ptr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "splash" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( m_vecWaterSurface );
	sparkEmitter->m_ParticleCollision.SetGravity( 800.0f );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 2.0f );

	//Dump out drops
	for ( i = 0; i < numDrops; i++ )
	{
		offset = m_vecWaterSurface;
		offset[0] += random_valve->RandomFloat( -16.0f, 16.0f ) * flScale;
		offset[1] += random_valve->RandomFloat( -16.0f, 16.0f ) * flScale;

		tParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == nullptr )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 1.0f );

		offDir = Vector(0,0,1) + RandomVector( -1.0f, 1.0f );

		tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( 50.0f * flScale * 2.0f, 100.0f * flScale * 2.0f );
		tParticle->m_vecVelocity[2] += random_valve->RandomFloat( 32.0f, 128.0f ) * flScale;

		tParticle->m_flWidth		= clamp( random_valve->RandomFloat( 1.0f, 3.0f ) * flScale, 0.1f, 4.0f );
		tParticle->m_flLength		= random_valve->RandomFloat( length*0.25f, length )/* * flScale*/;

		colorRamp = random_valve->RandomFloat( 1.5f, 2.0f );

		FloatToColor32( tParticle->m_color, std::min( 1.0f, m_vecColor[0] * colorRamp ), std::min( 1.0f, m_vecColor[1] * colorRamp ), std::min( 1.0f, m_vecColor[2] * colorRamp ), m_flLuminosity );
	}

	//Dump out drops
	for ( i = 0; i < 4; i++ )
	{
		offset = m_vecWaterSurface;
		offset[0] += random_valve->RandomFloat( -16.0f, 16.0f ) * flScale;
		offset[1] += random_valve->RandomFloat( -16.0f, 16.0f ) * flScale;

		tParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == nullptr )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random_valve->RandomFloat( 0.5f, 1.0f );

		offDir = Vector(0,0,1) + RandomVector( -0.2f, 0.2f );

		tParticle->m_vecVelocity = offDir * random_valve->RandomFloat( 50 * flScale * 3.0f, 100 * flScale * 3.0f );
		tParticle->m_vecVelocity[2] += random_valve->RandomFloat( 32.0f, 128.0f ) * flScale;

		tParticle->m_flWidth		= clamp( random_valve->RandomFloat( 2.0f, 3.0f ) * flScale, 0.1f, 4.0f );
		tParticle->m_flLength		= random_valve->RandomFloat( length*0.25f, length )/* * flScale*/;

		colorRamp = random_valve->RandomFloat( 1.5f, 2.0f );

		FloatToColor32( tParticle->m_color, std::min( 1.0f, m_vecColor[0] * colorRamp ), std::min( 1.0f, m_vecColor[1] * colorRamp ), std::min( 1.0f, m_vecColor[2] * colorRamp ), m_flLuminosity );
	}

	ks::intrusive_ptr<CSplashParticle> pSimple = CSplashParticle::Create( "splish" );
	pSimple->SetSortOrigin( m_vecWaterSurface );
	pSimple->SetClipHeight( m_vecWaterSurface.z );
	pSimple->GetBinding().SetBBox( m_vecWaterSurface-(Vector(32.0f, 32.0f, 32.0f)*flScale), m_vecWaterSurface+(Vector(32.0f, 32.0f, 32.0f)*flScale) );

	SimpleParticle	*pParticle;

	for ( i = 0; i < 16; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, m_vecWaterSurface );

		if ( pParticle == nullptr )
			break;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 2.0f;	//NOTENOTE: We use a clip plane to realistically control our lifespan

		pParticle->m_vecVelocity.Random( -0.2f, 0.2f );
		pParticle->m_vecVelocity += ( Vector( 0, 0, random_valve->RandomFloat( 4.0f, 6.0f ) ) );
		
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity *= 50 * flScale * (8-i);
		
		colorRamp = random_valve->RandomFloat( 0.75f, 1.25f );

		pParticle->m_uchColor[0]	= std::min( 1.0f, m_vecColor[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= std::min( 1.0f, m_vecColor[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= std::min( 1.0f, m_vecColor[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= 24 * flScale * RemapValClamped( i, 7, 0, 1, 0.5f );
		pParticle->m_uchEndSize		= std::min( 255, pParticle->m_uchStartSize * 2 );
		
		pParticle->m_uchStartAlpha	= RemapValClamped( i, 7, 0, 255, 32 ) * m_flLuminosity;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -4.0f, 4.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterExplosionEffect::PlaySound( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOSOUND )
		return;

	CLocalPlayerFilter filter;											   
	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Physics.WaterSplash", &m_vecWaterSurface );

	if ( m_flDepth > 128 )
	{
		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "WaterExplosionEffect.Sound", &m_vecOrigin );
	}
	else
	{
		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "BaseExplosionEffect.Sound", &m_vecOrigin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Intercepts the water explosion dispatch effect
//-----------------------------------------------------------------------------
void WaterSurfaceExplosionCallback( const CEffectData &data )
{
	WaterExplosionEffect().Create( data.m_vOrigin, data.m_flMagnitude, data.m_flScale, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( WaterSurfaceExplosion, WaterSurfaceExplosionCallback );

//Singleton static member definition
CMegaBombExplosionEffect	CMegaBombExplosionEffect::m_megainstance;

//Singleton accessor
CMegaBombExplosionEffect &MegaBombExplosionEffect( void )
{ 
	return CMegaBombExplosionEffect::Instance(); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMegaBombExplosionEffect::CreateCore( void )
{
	if ( m_fFlags & TE_EXPLFLAG_NOFIREBALL )
		return;

	Vector	offset;
	int		i;

	//Spread constricts as force rises
	float force = m_flForce;

	//Cap our force
	if ( force < EXPLOSION_FORCE_MIN )
		force = EXPLOSION_FORCE_MIN;
	
	if ( force > EXPLOSION_FORCE_MAX )
		force = EXPLOSION_FORCE_MAX;

	float spread = 1.0f - (0.15f*force);

	ks::intrusive_ptr<CExplosionParticle> pSimple = CExplosionParticle::Create( "exp_smoke" );
	pSimple->SetSortOrigin( m_vecOrigin );
	pSimple->SetNearClip( 32, 64 );

	SimpleParticle	*pParticle;

	if ( m_Material_FireCloud == nullptr )
	{
		m_Material_FireCloud = pSimple->GetPMaterial( "effects/fire_cloud2" );
	}

	//
	// Fireballs
	//

	for ( i = 0; i < 32; i++ )
	{
		offset.Random( -48.0f, 48.0f );
		offset += m_vecOrigin;

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_FireCloud, offset );

		if ( pParticle != nullptr )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.2f, 0.4f );

			pParticle->m_vecVelocity.Random( -spread*0.75f, spread*0.75f );
			pParticle->m_vecVelocity += m_vecDirection;
			
			VectorNormalize( pParticle->m_vecVelocity );

			float	fForce = random_valve->RandomFloat( 400.0f, 800.0f );

			//Scale the force down as we fall away from our main direction
			float	vDev = ScaleForceByDeviation( pParticle->m_vecVelocity, m_vecDirection, spread );

			pParticle->m_vecVelocity *= fForce * ( 16.0f * (vDev*vDev*0.5f) );

			#if __EXPLOSION_DEBUG
			debugoverlay->AddLineOverlay( m_vecOrigin, m_vecOrigin + pParticle->m_vecVelocity, 255, 0, 0, false, 3 );
			#endif

			int nColor = random_valve->RandomInt( 128, 255 );
			pParticle->m_uchColor[0]	= pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = nColor;
			
			pParticle->m_uchStartSize	= random_valve->RandomInt( 32, 85 ) * vDev;

			pParticle->m_uchStartSize	= clamp( pParticle->m_uchStartSize, 32, 85 );

			pParticle->m_uchEndSize		= (int)((float)pParticle->m_uchStartSize * 1.5f);
			
			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( -16.0f, 16.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void HelicopterMegaBombCallback( const CEffectData &data )
{
	CMegaBombExplosionEffect().Create( data.m_vOrigin, 1.0f, 1.0f, 0 );
}

DECLARE_CLIENT_EFFECT_BEGIN( HelicopterMegaBomb, HelicopterMegaBombCallback )
	//PRECACHE( PARTICLE_SYSTEM, "freeze_explosion" )
DECLARE_CLIENT_EFFECT_END()

#undef __EXPLOSION_DEBUG

class CParticleRenderIterator;
class CParticleSimulateIterator;

using namespace ::ks::reflect;

// enable this to have the fleck_merge cvar as well as the current system count displayed as it changes (for profiling)

//
// class PARTICLE_MERGE
//{
//public:
//	bool MergeParticleSystems( CFleckParticles *pSystem, const char *pEffectName, const Vector &center, const Vector &extents )
//	{ merge; return true; }
//};

// a singly linked list through all particle effects of a specific type
// with a specific rule for sharing them.
// Needs a hook to the particle effect's constructor/destructor and factory method
// The factory needs to support optionally merging the new particles into a previously built particle effect
// this cuts down on lots of scene management overhead as well as rendering/batch overhead
template< class PARTICLE_EFFECT, class PARTICLE_MERGE >
class CParticleMergeList
{
public:
	CParticleMergeList() : m_pHead(nullptr) {}
	void AddParticleSystem( PARTICLE_EFFECT *pSystem );
	void RemoveParticleSystem( PARTICLE_EFFECT *pRemove );
	PARTICLE_EFFECT *FindAndMergeParticleSystem( const char *pEffectName, const Vector &center, const Vector &extents );
	bool MergeParticleSystems( PARTICLE_EFFECT *pSystem, const char *pEffectName, const Vector &center, const Vector &extents );
private:
	PARTICLE_EFFECT *m_pHead;
	PARTICLE_MERGE m_merge;
};

template< class PARTICLE_EFFECT, class PARTICLE_MERGE >
void CParticleMergeList<PARTICLE_EFFECT,PARTICLE_MERGE>::AddParticleSystem( PARTICLE_EFFECT *pSystem )
{
	pSystem->m_pNextParticleSystem = m_pHead;
	m_pHead = pSystem;
}

template< class PARTICLE_EFFECT, class PARTICLE_MERGE >
void CParticleMergeList<PARTICLE_EFFECT,PARTICLE_MERGE>::RemoveParticleSystem( PARTICLE_EFFECT *pRemove )
{
	PARTICLE_EFFECT **pPrev = &m_pHead;
	PARTICLE_EFFECT *pCur = *pPrev;
	while ( pCur )
	{
		if ( pCur == pRemove )
		{
			*pPrev = pCur->m_pNextParticleSystem;
			return;
		}
		pPrev = &pCur->m_pNextParticleSystem;
		pCur = *pPrev;
	}
}

template< class PARTICLE_EFFECT, class PARTICLE_MERGE >
PARTICLE_EFFECT *CParticleMergeList<PARTICLE_EFFECT,PARTICLE_MERGE>::FindAndMergeParticleSystem( const char *pEffectName, const Vector &center, const Vector &extents )
{

	for ( PARTICLE_EFFECT *pMerge = m_pHead; pMerge != nullptr; pMerge = pMerge->m_pNextParticleSystem )
	{
		if ( m_merge.MergeParticleSystems( pMerge, pEffectName, center, extents ) )
			return pMerge;
	}
	return nullptr;
}

// merge anything within 10 feet
static const float MAX_RADIUS_BBOX_MERGE = 120.0f;

template< class PARTICLE_EFFECT >
class CMergeSameNameBbox
{
public:
	bool MergeParticleSystems( PARTICLE_EFFECT *pSystem, const char *pEffectName, const Vector &center, const Vector &extents )
	{
		// by default, match names
		if ( !V_stricmp(pSystem->GetEffectName(), pEffectName) )
		{
			Vector mins, maxs;
			pSystem->GetBinding().GetWorldspaceBounds( &mins, &maxs );
			AddPointToBounds( center - extents, mins, maxs );
			AddPointToBounds( center + extents, mins, maxs );
			Vector size = maxs - mins;
			float radius = size.Length();
			if ( radius < MAX_RADIUS_BBOX_MERGE )
			{
				pSystem->GetBinding().SetBBox( mins, maxs );
				// put sort origin at center of the new box
				Vector sortOrigin = 0.5f * (mins+maxs);
				pSystem->SetSortOrigin(sortOrigin);
				return true;
			}
		}
		return false;
	}
};

CParticleMergeList< CFleckParticles, CMergeSameNameBbox<CFleckParticles> > g_FleckMergeList;

//
// CFleckParticles
//
ks::intrusive_ptr<CFleckParticles> CFleckParticles::Create( const char *pDebugName, const Vector &vCenter, const Vector &extents )
{
	CFleckParticles *pMerge = g_FleckMergeList.FindAndMergeParticleSystem( pDebugName, vCenter, extents );
	if ( pMerge )
		return pMerge;

	CFleckParticles *pRet = new CFleckParticles( pDebugName );
	if ( pRet )
	{
		pRet->GetBinding().SetBBox( vCenter - extents, vCenter + extents );
		pRet->SetSortOrigin(vCenter);
	}
	return pRet;
}

CFleckParticles::CFleckParticles( const char *pDebugName ) : CSimpleEmitter( pDebugName ), m_pNextParticleSystem(nullptr)
{
	g_FleckMergeList.AddParticleSystem(this);
}

CFleckParticles::~CFleckParticles()
{
	g_FleckMergeList.RemoveParticleSystem(this);
}

//-----------------------------------------------------------------------------
// Purpose: Test for surrounding collision surfaces for quick collision testing for the particle system
// Input  : &origin - starting position
//			*dir - direction of movement (if NULL, will do a point emission test in four directions)
//			angularSpread - looseness of the spread
//			minSpeed - minimum speed
//			maxSpeed - maximum speed
//			gravity - particle gravity for the sytem
//			dampen - dampening amount on collisions
//			flags - extra information
//-----------------------------------------------------------------------------
void CFleckParticles::Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags )
{
	//See if we've specified a direction
	m_ParticleCollision.Setup( origin, direction, angularSpread, minSpeed, maxSpeed, gravity, dampen );
}

void CFleckParticles::RenderParticles( CParticleRenderIterator *pIterator )
{
	const FleckParticle *pParticle = (const FleckParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		Vector	tPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;
		
		Vector	color;
		color[0] = pParticle->m_uchColor[0] / 255.0f;
		color[1] = pParticle->m_uchColor[1] / 255.0f;
		color[2] = pParticle->m_uchColor[2] / 255.0f;
		//Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tPos,
			color,
			1.0f - (pParticle->m_flLifetime / pParticle->m_flDieTime),
			pParticle->m_uchSize,
			pParticle->m_flRoll );

		pParticle = (const FleckParticle*)pIterator->GetNext( sortKey );
	}
}

void CFleckParticles::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	FleckParticle *pParticle = (FleckParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		const float	timeDelta = pIterator->GetTimeDelta();

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;

			//Simulate the movement with collision
			trace_t trace;
			m_ParticleCollision.MoveParticle( pParticle->m_Pos, pParticle->m_vecVelocity, &pParticle->m_flRollDelta, timeDelta, &trace );

			// If we're in solid, then stop moving
			if ( trace.allsolid )
			{
				pParticle->m_vecVelocity = vec3_origin;
				pParticle->m_flRollDelta = 0.0f;
			}
		}

		pParticle = (FleckParticle*)pIterator->GetNext();
	}
}

class CEffectData;
class CNewParticleEffect;
class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

static ConVar  r_drawflecks( side_registry(), "r_drawflecks", "1" );
static ConVar  r_impacts_alt_orientation ( side_registry(), "r_impacts_alt_orientation", "1" );
extern ConVar r_drawmodeldecals;

ImpactSoundRouteFn g_pImpactSoundRouteFn = nullptr;

//==========================================================================================================================
// RAGDOLL ENUMERATOR
//==========================================================================================================================
CRagdollEnumerator::CRagdollEnumerator( Ray_t& shot, int iDamageType )
{
	m_rayShot = shot;
	m_iDamageType = iDamageType;
	m_bHit = false;
}

IterationRetval_t CRagdollEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
	if ( pEnt == nullptr )
		return ITERATION_CONTINUE;

	CBaseAnimating *pModel = static_cast< CBaseAnimating * >( pEnt );

	// If the ragdoll was created on this tick, then the forces were already applied on the server
	if ( pModel == nullptr || WasRagdollCreatedOnCurrentTick( pEnt ) )
		return ITERATION_CONTINUE;

	IPhysicsObject *pPhysicsObject = pModel->VPhysicsGetObject();
	if ( pPhysicsObject == nullptr )
		return ITERATION_CONTINUE;

	trace_t tr;
	enginetrace->ClipRayToEntity( m_rayShot, MASK_SHOT, pModel, &tr );

	if ( tr.fraction < 1.0 )
	{
		pModel->ImpactTrace( &tr, m_iDamageType, nullptr );
		m_bHit = true;

		//FIXME: Yes?  No?
		return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool FX_AffectRagdolls( Vector vecOrigin, Vector vecStart, int iDamageType )
{
	// don't do this when lots of ragdolls are simulating
	if ( s_RagdollLRU.CountRagdolls(true) > 1 )
		return false;
	Ray_t shotRay;
	shotRay.Init( vecStart, vecOrigin );

	CRagdollEnumerator ragdollEnum( shotRay, iDamageType );
	::partition->EnumerateElementsAlongRay( PARTITION_CLIENT_RESPONSIVE_EDICTS, shotRay, false, &ragdollEnum );

	return ragdollEnum.Hit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RagdollImpactCallback( const CEffectData &data )
{
	FX_AffectRagdolls( data.m_vOrigin, data.m_vStart, data.m_nDamageType );
}

DECLARE_CLIENT_EFFECT( RagdollImpact, RagdollImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Impact( Vector &vecOrigin, Vector &vecStart, int iMaterial, int iDamageType, int iHitbox, CBaseEntity *pEntity, trace_t &tr, int nFlags, int maxLODToDecal )
{
	CVProfScope _( "Impact", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	contract_assert( pEntity );

	MDLCACHE_CRITICAL_SECTION();

	// Clear out the trace
	memset( &tr, 0, sizeof(trace_t));
	tr.fraction = 1.0f;

	// Setup our shot information
	Vector shotDir = vecOrigin - vecStart;
	float flLength = VectorNormalize( shotDir );
	Vector traceExt;
	VectorMA( vecStart, flLength + 8.0f, shotDir, traceExt );

	// Attempt to hit ragdolls
	
	bool bHitRagdoll = false;
	
	if ( !pEntity->IsClientCreated() )
	{
		bHitRagdoll = FX_AffectRagdolls( vecOrigin, vecStart, iDamageType );
	}

	if ( (nFlags & IMPACT_NODECAL) == 0 )
	{
		int decalNumber = decalsystem->GetDecalIndexForName( GetImpactDecal( pEntity, iMaterial, iDamageType ) );
		if ( decalNumber == -1 )
			return false;

		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			staticpropmgr->AddDecalToStaticProp( vecStart, traceExt, iHitbox - 1, decalNumber, true, tr );
		}
		else if ( pEntity )
		{
			// Here we deal with decals on entities.
			pEntity->AddDecal( vecStart, traceExt, vecOrigin, iHitbox, decalNumber, true, tr, maxLODToDecal );
		}
	}
	else
	{
		// Perform the trace ourselves
		Ray_t ray;
		ray.Init( vecStart, traceExt );

		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			// Special case for world entity with hitbox (that's a static prop)
			ICollideable *pCollideable = staticpropmgr->GetStaticPropByIndex( iHitbox - 1 ); 
			enginetrace->ClipRayToCollideable( ray, MASK_SHOT, pCollideable, &tr );
		}
		else
		{
			if ( !pEntity )
				return false;

			enginetrace->ClipRayToEntity( ray, MASK_SHOT, pEntity, &tr );
		}
	}

	// If we found the surface, emit debris flecks
	bool bReportRagdollImpacts = (nFlags & IMPACT_REPORT_RAGDOLL_IMPACTS) != 0;
	if ( ( tr.fraction == 1.0f ) || ( bHitRagdoll && !bReportRagdollImpacts ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *GetImpactDecal( CBaseEntity *pEntity, int iMaterial, int iDamageType )
{
	char const *decalName;
	if ( !pEntity )
	{
		decalName = "Impact.Concrete";
	}
	else
	{
		decalName = pEntity->DamageDecal( iDamageType, iMaterial );
	}

	// See if we need to offset the decal for material type
	return decalsystem->TranslateDecalForGameMaterial( decalName, iMaterial );
}

//-----------------------------------------------------------------------------
// Purpose: Perform custom effects based on the Decal index
//-----------------------------------------------------------------------------

struct ImpactEffect_t
{
	const char *m_pName;
	const char *m_pNameNoFlecks;
};

static ImpactEffect_t s_pImpactEffect[26] = 
{
	{ nullptr,					nullptr },							// CHAR_TEX_ANTLION
	{ nullptr,					nullptr },							// CHAR_TEX_BLOODYFLESH	
	{ "impact_concrete",	"impact_concrete" },		// CHAR_TEX_CONCRETE		
	{ "impact_dirt",		"impact_dirt" },			// CHAR_TEX_DIRT			
	{ nullptr,					nullptr },							// CHAR_TEX_EGGSHELL		
	{ nullptr,					nullptr },							// CHAR_TEX_FLESH			
	{ "impact_metal",		"impact_metal" },			// CHAR_TEX_GRATE			
	{ nullptr,					nullptr },							// CHAR_TEX_ALIENFLESH		
	{ nullptr,					nullptr },							// CHAR_TEX_CLIP			
	{ "impact_grass",		"impact_grass" },			// CHAR_TEX_GRASS		
	{ "impact_snow",		"impact_snow" },			// CHAR_TEX_SNOW
	{ "impact_plastic",		"impact_plastic" },		// CHAR_TEX_PLASTIC		
	{ "impact_metal",		"impact_metal" },			// CHAR_TEX_METAL			
	{ "impact_sand",		"impact_sand" },			// CHAR_TEX_SAND			
	{ "impact_leaves",		"impact_leaves" },		// CHAR_TEX_FOLIAGE		
	{ "impact_computer",	"impact_computer" },		// CHAR_TEX_COMPUTER		
	{ "impact_asphalt",		"impact_asphalt" },		// CHAR_TEX_ASPHALT		
	{ "impact_brick",		"impact_brick" },			// CHAR_TEX_BRICK		
	{ "impact_wet",			"impact_wet" },			// CHAR_TEX_SLOSH			
	{ "impact_tile",		"impact_tile" },			// CHAR_TEX_TILE			
	{ "impact_cardboard",	"impact_cardboard" },		// CHAR_TEX_CARDBOARD		
	{ "impact_metal",		"impact_metal" },			// CHAR_TEX_VENT			
	{ "impact_wood",		"impact_wood" },			// CHAR_TEX_WOOD			
	{ nullptr,					nullptr },							// CHAR_TEX_FAKE		
	{ "impact_glass",		"impact_glass" },			// CHAR_TEX_GLASS			
	{ nullptr,					nullptr },							// CHAR_TEX_WARPSHIELD	
};

static ImpactEffect_t s_pImpactEffect2[12] = 
{
	{ "impact_clay",		"impact_clay" },			// CHAR_TEX_CLAY
	{ "impact_plaster",		"impact_plaster" },		// CHAR_TEX_PLASTER	
	{ "impact_rock",		"impact_rock" },			// CHAR_TEX_ROCK		
	{ "impact_rubber",		"impact_rubber" },		// CHAR_TEX_RUBBER			
	{ "impact_sheetrock",	"impact_sheetrock" },		// CHAR_TEX_SHEETROCK		
	{ "impact_cloth",		"impact_cloth" },			// CHAR_TEX_CLOTH			
	{ "impact_carpet",		"impact_carpet" },		// CHAR_TEX_CARPET			
	{ "impact_paper",		"impact_paper" },			// CHAR_TEX_PAPER		
	{ "impact_upholstery",	"impact_upholstery" },	// CHAR_TEX_UPHOLSTERY				
	{ "impact_puddle",		"impact_puddle" },		// CHAR_TEX_PUDDLE
	{ "impact_mud",			"impact_mud" },			// CHAR_TEX_MUD	
	{ "impact_sandbarrel",	"impact_sandbarrel" },	// CHAR_TEX_SANDBARREL	
};

static int s_pImpactEffectIndex[ std::size( s_pImpactEffect ) ][2];
static int s_pImpactEffect2Index[ std::size( s_pImpactEffect2 ) ][2];

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheImpacts )
	for ( int i = 0; i < std::size( s_pImpactEffect ); ++i )
	{
		if ( s_pImpactEffect[i].m_pName )
		{
			PRECACHE_INDEX( PARTICLE_SYSTEM, s_pImpactEffect[i].m_pName, s_pImpactEffectIndex[i][0] );
		}
		if ( s_pImpactEffect[i].m_pNameNoFlecks )
		{
			PRECACHE_INDEX( PARTICLE_SYSTEM, s_pImpactEffect[i].m_pNameNoFlecks, s_pImpactEffectIndex[i][1] );
		}
	}

	for ( int i = 0; i < std::size( s_pImpactEffect2 ); ++i )
	{
		if ( s_pImpactEffect2[i].m_pName )
		{
			PRECACHE_INDEX( PARTICLE_SYSTEM, s_pImpactEffect2[i].m_pName, s_pImpactEffect2Index[i][0] );
		}
		if ( s_pImpactEffect2[i].m_pNameNoFlecks )
		{
			PRECACHE_INDEX( PARTICLE_SYSTEM, s_pImpactEffect2[i].m_pNameNoFlecks, s_pImpactEffect2Index[i][1] );
		}
	}
PRECACHE_REGISTER_END()

static void SetImpactControlPoint( CNewParticleEffect *pEffect, int nPoint, const Vector &vecImpactPoint, const Vector &vecForward, CBaseEntity *pEntity )
{
	Vector vecImpactY, vecImpactZ;
	VectorVectors( vecForward, vecImpactY, vecImpactZ ); 
	vecImpactY *= -1.0f;

	pEffect->SetControlPoint( nPoint, vecImpactPoint );

	if ( r_impacts_alt_orientation.GetBool() )
		pEffect->SetControlPointOrientation( nPoint, vecImpactZ, vecImpactY, vecForward );
	else
		pEffect->SetControlPointOrientation( nPoint, vecForward, vecImpactY, vecImpactZ );
	pEffect->SetControlPointEntity( nPoint, pEntity );
}

static void PerformNewCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	bool bNoFlecks = !r_drawflecks.GetBool();
	if ( !bNoFlecks )
	{
		bNoFlecks = ( ( nFlags & FLAGS_CUSTIOM_EFFECTS_NOFLECKS ) != 0  );
	}

	// Compute the impact effect name
	ImpactEffect_t *pEffectList;
	int *pEffectIndex;
	int nOffset;
	if ( iMaterial >= FIRST_L4D_CHAR_TEX && iMaterial <= LAST_L4D_CHAR_TEX )
	{
		pEffectList = s_pImpactEffect2;
		nOffset = 1;
		pEffectIndex = s_pImpactEffect2Index[iMaterial - nOffset];
	}
	else if ( ( iMaterial >= FIRST_CHAR_TEX ) && ( iMaterial <= LAST_CHAR_TEX ) )
	{
		pEffectList = s_pImpactEffect;
		nOffset = FIRST_CHAR_TEX;
		pEffectIndex = s_pImpactEffectIndex[iMaterial - nOffset];
	}
	else
	{
		DevMsg( "Invalid surface property.  Double-check surfaceproperties_manifest.txt\n" );
		return;
	}

	const ImpactEffect_t &effect = pEffectList[ iMaterial - nOffset ];

	const char *pImpactName = effect.m_pName;
	int nEffectIndex = pEffectIndex[0];
	if ( bNoFlecks && effect.m_pNameNoFlecks )
	{
		pImpactName = effect.m_pNameNoFlecks;
		nEffectIndex = pEffectIndex[1];
	}
	if ( !pImpactName )
		return;

	Vector	vecReflect;
	float	flDot = DotProduct( shotDir, tr.plane.normal );
	VectorMA( shotDir, -2.0f * flDot, tr.plane.normal, vecReflect );
		
	Vector vecShotBackward;
	VectorMultiply( shotDir, -1.0f, vecShotBackward );
		
	Vector vecImpactPoint = ( tr.fraction != 1.0f ) ? tr.endpos : vecOrigin;

	ks::intrusive_ptr<CNewParticleEffect> pEffect = CNewParticleEffect::CreateOrAggregatePrecached( nullptr, nEffectIndex, vecImpactPoint );
	if ( !pEffect->IsValid() )
		return;

	SetImpactControlPoint( pEffect.get(), 0, vecImpactPoint, tr.plane.normal, tr.Ent<CBaseEntity>() ); 
	SetImpactControlPoint( pEffect.get(), 1, vecImpactPoint, vecReflect,		tr.Ent<CBaseEntity>() ); 
	SetImpactControlPoint( pEffect.get(), 2, vecImpactPoint, vecShotBackward,	tr.Ent<CBaseEntity>() ); 
	pEffect->SetControlPoint( 3, Vector( iScale, iScale, iScale ) );

	if ( pEffect->m_pDef->ReadsControlPoint( 4 ) )
	{
		Vector vecColor;
		GetColorForSurface( &tr, &vecColor );
		pEffect->SetControlPoint( 4, vecColor );
	}

}

void PerformCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	// Throw out the effect if any of these are true
	const int noEffectsFlags = (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP);
	if ( tr.surface.flags & noEffectsFlags )
		return;

	PerformNewCustomEffects( vecOrigin, tr, shotDir, iMaterial, iScale, nFlags );
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound for an impact. If tr contains a valid hit, use that. 
//			If not, use the passed in origin & surface.
//-----------------------------------------------------------------------------
void PlayImpactSound( CBaseEntity *pEntity, trace_t &tr, Vector &vecServerOrigin, int nServerSurfaceProp )
{
	CVProfScope _( "PlayImpactSound", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	surfacedata_t *pdata;
	Vector vecOrigin;

	if (pEntity->IsDormant())
		return;

	// If the client-side trace hit a different entity than the server, or
	// the server didn't specify a surfaceprop, then use the client-side trace 
	// material if it's valid.
	if ( tr.DidHit() && (pEntity != tr.Ent<CBaseEntity>() || nServerSurfaceProp == 0) )
	{
		nServerSurfaceProp = tr.surface.surfaceProps;
	}
	pdata = physprops->GetSurfaceData( nServerSurfaceProp );
	if ( tr.fraction < 1.0 )
	{
		vecOrigin = tr.endpos;
	}
	else
	{
		vecOrigin = vecServerOrigin;
	}

	// Now play the esound
	if ( pdata->sounds.bulletImpact )
	{
		const char *pbulletImpactSoundName = physprops->GetString( pdata->sounds.bulletImpact );
		
		if ( g_pImpactSoundRouteFn )
		{
			g_pImpactSoundRouteFn( pbulletImpactSoundName, vecOrigin );
		}
		else
		{
			CLocalPlayerFilter filter;
			CBaseEntity::EmitSound( filter, 0, pbulletImpactSoundName, pdata->soundhandles.bulletImpact, &vecOrigin );
		}

		// play a ricochet based on the material
		float flRicoChance = 0.0f;
		switch( pdata->game.material )
		{
			case CHAR_TEX_METAL:
			case CHAR_TEX_CONCRETE:
			case CHAR_TEX_COMPUTER:
			case CHAR_TEX_BRICK:
			case CHAR_TEX_TILE:
			case CHAR_TEX_ROCK:
			case CHAR_TEX_ASPHALT:
				flRicoChance = 5.0;
				break;

			case CHAR_TEX_GRATE:
			case CHAR_TEX_VENT:
			case CHAR_TEX_WOOD:
			case CHAR_TEX_RUBBER:
			case CHAR_TEX_SHEETROCK:
			case CHAR_TEX_CARPET:
				flRicoChance = 3.0;
				break;
				
			case CHAR_TEX_DIRT:
			case CHAR_TEX_PLASTIC:
			case CHAR_TEX_CLAY:
			case CHAR_TEX_PLASTER:
				flRicoChance = 1.0;
				break;
		}

		if ( RandomFloat(0, 10) <= flRicoChance )
		{
			FX_RicochetSound( vecOrigin );
		}
		return;
	}

}

void SetImpactSoundRoute( ImpactSoundRouteFn fn )
{
	g_pImpactSoundRouteFn = fn;
}

//-----------------------------------------------------------------------------
// Purpose: Pull the impact data out
// Input  : &data - 
//			*vecOrigin - 
//			*vecAngles - 
//			*iMaterial - 
//			*iDamageType - 
//			*iHitbox - 
//			*iEntIndex - 
//-----------------------------------------------------------------------------
CBaseEntity *ParseImpactData( const CEffectData &data, Vector *vecOrigin, Vector *vecStart, 
	Vector *vecShotDir, short &nSurfaceProp, int &iMaterial, int &iDamageType, int &iHitbox )
{
	CBaseEntity *pEntity = data.GetEntity( );

	if ( data.m_bPositionsAreRelativeToEntity && pEntity )
	{
		*vecOrigin = data.m_vOrigin + pEntity->GetAbsOrigin();
		*vecStart = data.m_vStart + pEntity->GetAbsOrigin();
	}
	else
	{
		*vecOrigin = data.m_vOrigin;
		*vecStart = data.m_vStart;
	}

	nSurfaceProp = data.m_nSurfaceProp;
	iDamageType = data.m_nDamageType;
	iHitbox = data.m_nHitBox;

	*vecShotDir = (*vecOrigin - *vecStart);
	VectorNormalize( *vecShotDir );

	// Get the material from the surfaceprop
	surfacedata_t *psurfaceData = physprops->GetSurfaceData( data.m_nSurfaceProp );
	iMaterial = psurfaceData->game.material;

	return pEntity;
}

CInterpolatedValue::CInterpolatedValue( void ) :  m_flStartTime( 0.0f ), m_flEndTime( 0.0f ), m_flStartValue( 0.0f ), m_flEndValue( 0.0f ), m_nInterpType( INTERP_LINEAR ) 
{
}

CInterpolatedValue::CInterpolatedValue( float startTime, float endTime, float startValue, float endValue, InterpType_t type ) : 
	  m_flStartTime( startTime ), m_flEndTime( endTime ), m_flStartValue( startValue ), m_flEndValue( endValue ), m_nInterpType( type ) 
{
}

void CInterpolatedValue::SetTime( float start, float end ) 
{ 
	m_flStartTime = start; m_flEndTime = end; 
}

void CInterpolatedValue::SetRange( float start, float end ) 
{ 
	m_flStartValue = start; m_flEndValue = end; 
}

void CInterpolatedValue::SetType( InterpType_t type )
{ 
	m_nInterpType = type; 
}
	
// Set the value with no range
void CInterpolatedValue::SetAbsolute( float value )
{
	m_flStartValue = m_flEndValue = value;
	m_flStartTime = m_flEndTime = gpGlobals->curtime;
	m_nInterpType = INTERP_LINEAR;
}

// Set the value with range and time supplied
void CInterpolatedValue::Init( float startValue, float endValue, float dt, InterpType_t type /*= INTERP_LINEAR*/ )
{
	if ( dt <= 0.0f )
	{
		SetAbsolute( endValue );
		return;
	}

	SetTime( gpGlobals->curtime, gpGlobals->curtime + dt );
	SetRange( startValue, endValue );
	SetType( type );
}

// Start from the current value and move towards the end value
void CInterpolatedValue::InitFromCurrent( float endValue, float dt, InterpType_t type /*= INTERP_LINEAR*/ )
{
	Init( Interp( gpGlobals->curtime ), endValue, dt, type );
}

// Find our interpolated value at the given point in time
float CInterpolatedValue::Interp( float curTime )
{
	switch( m_nInterpType )
	{
	case INTERP_LINEAR:
		{
			if ( curTime >= m_flEndTime )
				return m_flEndValue;

			if ( curTime <= m_flStartTime )
				return m_flStartValue;

			return RemapVal( curTime, m_flStartTime, m_flEndTime, m_flStartValue, m_flEndValue );
		}

	case INTERP_SPLINE:
		{
			if ( curTime >= m_flEndTime )
				return m_flEndValue;

			if ( curTime <= m_flStartTime )
				return m_flStartValue;

			return SimpleSplineRemapVal( curTime, m_flStartTime, m_flEndTime, m_flStartValue, m_flEndValue );
		}
	}

	// NOTENOTE: You managed to pass in a bogus interpolation type!
	contract_assert( 0 );
	return -1.0f;
}

struct FXLineData_t;

using namespace ::ks::reflect;

/*
==================================================
CFXLine
==================================================
*/
CFXLine::CFXLine( const char *name, const FXLineData_t &data )
: CClientSideEffect( name )
{
	m_FXData = data;
	
	m_FXData.m_flLifeTime = 0.0f;
}

CFXLine::~CFXLine( void )
{
	Destroy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//-----------------------------------------------------------------------------
void CFXLine::Draw( double frametime )
{
	// Update the effect
	Update( frametime );

	Vector lineDir, viewDir;
	
	//Get the proper orientation for the line
	VectorSubtract( m_FXData.m_vecStart, m_FXData.m_vecEnd, lineDir );
	VectorSubtract( m_FXData.m_vecEnd, CurrentViewOrigin(), viewDir );
	
	Vector cross = lineDir.Cross( viewDir );

	VectorNormalize( cross );

	CMatRenderContextPtr pRenderContext( materials );
	
	//Bind the material
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_FXData.m_pMaterial );
	
	CMeshBuilder meshBuilder;

	Vector tmp;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float scaleTimePerc = ( m_FXData.m_flLifeTime / m_FXData.m_flDieTime );
	float scale = m_FXData.m_flStartScale + ( ( m_FXData.m_flEndScale - m_FXData.m_flStartScale ) * scaleTimePerc );

	color32 color = {255,255,255,255};

	float alpha = m_FXData.m_flStartAlpha + ( ( m_FXData.m_flEndAlpha - m_FXData.m_flStartAlpha ) * scaleTimePerc );
	alpha = clamp( alpha, 0.0f, 1.0f );

	color.a *= alpha;

	// Start
	VectorMA( m_FXData.m_vecStart, -scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	VectorMA( m_FXData.m_vecStart, scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	// End
	VectorMA( m_FXData.m_vecEnd, scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	VectorMA( m_FXData.m_vecEnd, -scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFXLine::IsActive( void )
{
	return ( m_FXData.m_flLifeTime < m_FXData.m_flDieTime ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFXLine::Destroy( void )
{
	//Release the material
	if ( m_FXData.m_pMaterial != nullptr )
	{
		m_FXData.m_pMaterial->DecrementReferenceCount();
		m_FXData.m_pMaterial = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//-----------------------------------------------------------------------------
void CFXLine::Update( double frametime )
{
	m_FXData.m_flLifeTime += frametime;

	//Move our end points
	VectorMA( m_FXData.m_vecStart, frametime, m_FXData.m_vecStartVelocity, m_FXData.m_vecStart );
	VectorMA( m_FXData.m_vecEnd, frametime, m_FXData.m_vecEndVelocity, m_FXData.m_vecEnd );
}

void FX_DrawLine( const Vector &start, const Vector &end, float scale, IMaterial *pMaterial, const color32 &color )
{
	Vector			lineDir, viewDir;
	//Get the proper orientation for the line
	VectorSubtract( end, start, lineDir );
	VectorSubtract( end, CurrentViewOrigin(), viewDir );
	
	Vector cross = lineDir.Cross( viewDir );

	VectorNormalize( cross );

	CMatRenderContextPtr pRenderContext( materials );
	
	//Bind the material
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, pMaterial );
	CMeshBuilder meshBuilder;

	Vector			tmp;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	VectorMA( start, -scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	VectorMA( start, scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	VectorMA( end, scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	VectorMA( end, -scale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

void FX_DrawLineFade( const Vector &start, const Vector &end, float scale, IMaterial *pMaterial, const color32 &color, float fadeDist )
{
	Vector			lineDir, viewDir;
	//Get the proper orientation for the line
	VectorSubtract( end, start, lineDir );
	VectorSubtract( end, CurrentViewOrigin(), viewDir );
	
	float lineLength = lineDir.Length();
	float t0 = 0.25f;
	float t1 = 0.75f;
	if ( lineLength > 0 )
	{
		t0 = fadeDist / lineLength;
		t0 = clamp( t0, 0.0f, 0.25f );
		t1 = 1.0f - t0;
	}

	Vector cross = lineDir.Cross( viewDir );

	VectorNormalize( cross );

	CMatRenderContextPtr pRenderContext( materials );

	//Bind the material
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, pMaterial );
	CMeshBuilder meshBuilder;

	Vector			tmp;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 8, 24 );

	//		2			5
	// 0	1			4	 7
	//		3			6

	// 0,2,1 - 0,1,3 - 7,4,5 - 7,6,4 - 1,4,6, 1,6,3 - 1,5,4 - 1,2,5

	// v0
	meshBuilder.Position3fv( start.Base() );
	meshBuilder.TexCoord2f( 0, 0.5f, 0.0f );
	meshBuilder.Color4ub( 0, 0, 0, 0 );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v1
	Vector v1 = start + t0 * lineDir;
	meshBuilder.Position3fv( v1.Base() );
	meshBuilder.TexCoord2f( 0, 0.5f, t0 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v2
	tmp = v1 - scale*cross;
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, t0 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v3
	tmp = v1 + scale*cross;
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, t0 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v4
	Vector v4 = start + t1 * lineDir;
	meshBuilder.Position3fv( v4.Base() );
	meshBuilder.TexCoord2f( 0, 0.5f, t1 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v5
	tmp = v4 - scale*cross;
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, t1 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v6
	tmp = v4 + scale*cross;
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, t1 );
	meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();
	// v7
	meshBuilder.Position3fv( end.Base() );
	meshBuilder.TexCoord2f( 0, 0.5f, 1.0f );
	meshBuilder.Color4ub( 0, 0, 0, 0 );
	meshBuilder.Normal3fv( cross.Base() );
	meshBuilder.AdvanceVertex();

	// triangles - 0,2,1 - 0,1,3 - 7,4,5 - 7,6,4 - 1,4,6, 1,6,3 - 1,5,4 - 1,2,5
	meshBuilder.FastIndex( 0 ); meshBuilder.FastIndex( 2 ); meshBuilder.FastIndex( 1 );
	meshBuilder.FastIndex( 0 ); meshBuilder.FastIndex( 1 ); meshBuilder.FastIndex( 3 );

	meshBuilder.FastIndex( 7 ); meshBuilder.FastIndex( 4 ); meshBuilder.FastIndex( 5 );
	meshBuilder.FastIndex( 7 ); meshBuilder.FastIndex( 6 ); meshBuilder.FastIndex( 4 );

	meshBuilder.FastIndex( 1 ); meshBuilder.FastIndex( 4 ); meshBuilder.FastIndex( 6 );
	meshBuilder.FastIndex( 1 ); meshBuilder.FastIndex( 6 ); meshBuilder.FastIndex( 3 );
	meshBuilder.FastIndex( 1 ); meshBuilder.FastIndex( 5 ); meshBuilder.FastIndex( 4 );
	meshBuilder.FastIndex( 1 ); meshBuilder.FastIndex( 2 ); meshBuilder.FastIndex( 5 );

	meshBuilder.End();
	pMesh->Draw();
}

struct FXQuadData_t;

using namespace ::ks::reflect;

CFXQuad::CFXQuad( const FXQuadData_t &data )

: CClientSideEffect( "Quad" )
{
	m_FXData = data;
}

CFXQuad::~CFXQuad( void )
{
	Destroy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//-----------------------------------------------------------------------------
void CFXQuad::Draw( double frametime )
{
	CVProfScope _( "FX_Quad::Draw", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	// Update the effect
	Update( frametime );

	float	scaleTimePerc, alphaTimePerc;

	//Determine the scale
	if ( m_FXData.m_flDieTime == 0.0f )
	{
		scaleTimePerc = 1.0f;	// don't divide by 0...
	}
	else
	{
		if ( m_FXData.m_uiFlags & FXQUAD_BIAS_SCALE )
		{
			scaleTimePerc = Bias( ( m_FXData.m_flLifeTime / m_FXData.m_flDieTime ), m_FXData.m_flScaleBias );
		}
		else
		{
			scaleTimePerc = ( m_FXData.m_flLifeTime / m_FXData.m_flDieTime );
		}
	}

	float scale = m_FXData.m_flStartScale + ( ( m_FXData.m_flEndScale - m_FXData.m_flStartScale ) * scaleTimePerc );

	//Determine the alpha
	if ( m_FXData.m_flDieTime == 0.0f )
	{
		alphaTimePerc = 1.0f;	// don't divide by 0...
	}
	else
	{
		if ( m_FXData.m_uiFlags & FXQUAD_BIAS_ALPHA )
		{
			alphaTimePerc = Bias( ( m_FXData.m_flLifeTime / m_FXData.m_flDieTime ), m_FXData.m_flAlphaBias );
		}
		else
		{
			alphaTimePerc = ( m_FXData.m_flLifeTime / m_FXData.m_flDieTime );
		}
	}

	float alpha = m_FXData.m_flStartAlpha + ( ( m_FXData.m_flEndAlpha - m_FXData.m_flStartAlpha ) * alphaTimePerc );
	alpha = clamp( alpha, 0.0f, 1.0f );
	
	CMatRenderContextPtr pRenderContext( materials );

	//Bind the material
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_FXData.m_pMaterial );
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	//Update our roll
	m_FXData.m_flYaw = anglemod( m_FXData.m_flYaw + ( m_FXData.m_flDeltaYaw * frametime ) );

	Vector	pos;
	Vector	vRight, vUp;

	float color[4];

	color[0] = m_FXData.m_Color[0];
	color[1] = m_FXData.m_Color[1];
	color[2] = m_FXData.m_Color[2];

	if ( m_FXData.m_uiFlags & FXQUAD_COLOR_FADE )
	{
		color[0] *= alpha;
		color[1] *= alpha;
		color[2] *= alpha;
	}

	color[3] = alpha;

	VectorVectors( m_FXData.m_vecNormal, vRight, vUp );

	Vector	rRight, rUp;

	rRight	= ( vRight * std::cos( DEG2RAD( m_FXData.m_flYaw ) ) ) - ( vUp * std::sin( DEG2RAD( m_FXData.m_flYaw ) ) );
	rUp		= ( vRight * cos( DEG2RAD( m_FXData.m_flYaw+90.0f ) ) ) - ( vUp * sin( DEG2RAD( m_FXData.m_flYaw+90.0f ) ) );

	vRight	= rRight * ( scale * 0.5f );
	vUp		= rUp * ( scale * 0.5f );

	pos = m_FXData.m_vecOrigin + vRight - vUp;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Normal3fv( m_FXData.m_vecNormal.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	pos = m_FXData.m_vecOrigin - vRight - vUp;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Normal3fv( m_FXData.m_vecNormal.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	pos = m_FXData.m_vecOrigin - vRight + vUp;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Normal3fv( m_FXData.m_vecNormal.Base() );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	pos = m_FXData.m_vecOrigin + vRight + vUp;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Normal3fv( m_FXData.m_vecNormal.Base() );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFXQuad::IsActive( void )
{
	return ( m_FXData.m_flLifeTime < m_FXData.m_flDieTime ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFXQuad::Destroy( void )
{
	//Release the material
	if ( m_FXData.m_pMaterial != nullptr )
	{
		m_FXData.m_pMaterial->DecrementReferenceCount();
		m_FXData.m_pMaterial = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//-----------------------------------------------------------------------------
void CFXQuad::Update( double frametime )
{
	m_FXData.m_flLifeTime += frametime;
}

FXQuadData_t::FXQuadData_t( void)
{
	m_flLifeTime	= 0.0f;
	m_flDieTime		= 0.0f;
	m_uiFlags		= 0;
}

void FXQuadData_t::SetMaterial( const char *shader)
{
	m_pMaterial = materials->FindMaterial( shader, TEXTURE_GROUP_CLIENT_EFFECTS );

	if ( m_pMaterial != nullptr )
	{
		m_pMaterial->IncrementReferenceCount();
	}
}

class CEffectData;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled ShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( ShellEject, ShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RifleShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled RifleShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( RifleShellEject, RifleShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShotgunShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		DevWarning( "Unhandled ShotgunShellEject effect\n" );
	}
}

DECLARE_CLIENT_EFFECT( ShotgunShellEject, ShotgunShellEjectCallback );

class CEffectData;
class CParticleRenderIterator;
class CParticleSimulateIterator;
struct FXQuadData_t;
class SimpleParticle;

using namespace ::ks::reflect;

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectSparks )
	PRECACHE( MATERIAL, "effects/spark" )
	PRECACHE( MATERIAL, "effects/energysplash" )
	PRECACHE( MATERIAL, "effects/energyball" )
	PRECACHE( MATERIAL, "sprites/rico1" )
	PRECACHE( MATERIAL, "sprites/rico1_noz" )
	PRECACHE( MATERIAL, "sprites/blueflare1" )
	PRECACHE( MATERIAL, "effects/yellowflare" )
	PRECACHE( MATERIAL, "effects/combinemuzzle1_nocull" )
	PRECACHE( MATERIAL, "effects/combinemuzzle2_nocull" )
	PRECACHE( MATERIAL, "effects/yellowflare_noz" )
PRECACHE_REGISTER_END()

PMaterialHandle g_Material_Spark = nullptr;

static ConVar fx_drawmetalspark( side_registry(), "fx_drawmetalspark", "1", FCVAR_DEVELOPMENTONLY, "Draw metal spark effects." );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool EffectOccluded( const Vector &pos, pixelvis_handle_t *queryHandle )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( !queryHandle )
	{
		// NOTE: This is called by networking code before the current view is set up.
		// so use the main view instead
		trace_t	tr;
		UTIL_TraceLine( pos, MainViewOrigin(nSlot), MASK_OPAQUE, nullptr, COLLISION_GROUP_NONE, &tr );
		
		return ( tr.fraction < 1.0f ) ? true : false;
	}
	pixelvis_queryparams_t params;
	params.Init(pos);
	
	return PixelVisibility_FractionVisible( params, queryHandle ) > 0.0f ? false : true;
}

CSimpleGlowEmitter::CSimpleGlowEmitter( const char *pDebugName, const Vector &sortOrigin, float flDeathTime )
	: CSimpleEmitter( pDebugName )
{
	SetSortOrigin( sortOrigin );
	m_queryHandle = 0;
	m_wasTested = 0;
	m_isVisible = 0;
	m_startTime = gpGlobals->curtime;
	m_flDeathTime = flDeathTime;
}
	
CSimpleGlowEmitter *CSimpleGlowEmitter::Create( const char *pDebugName, const Vector &sortOrigin, float flDeathTime )
{
	return new CSimpleGlowEmitter( pDebugName, sortOrigin, flDeathTime );
}

void CSimpleGlowEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	if ( gpGlobals->curtime > m_flDeathTime )
	{
		pIterator->RemoveAllParticles();
		return;
	}

	if ( !WasTestedInView(1<<0) )
		return;

	BaseClass::SimulateParticles( pIterator );
}

bool CSimpleGlowEmitter::WasTestedInView( unsigned char viewMask )
{
	return (m_wasTested & viewMask) ? true : false;
}

bool CSimpleGlowEmitter::IsVisibleInView( unsigned char viewMask )
{
	return (m_isVisible & viewMask) ? true : false;
}

void CSimpleGlowEmitter::SetTestedInView( unsigned char viewMask, bool bTested )
{
	m_wasTested &= ~viewMask;
	if ( bTested )
	{
		m_wasTested |= viewMask;
	}
}

void CSimpleGlowEmitter::SetVisibleInView( unsigned char viewMask, bool bVisible )
{
	m_isVisible &= ~viewMask;
	if ( bVisible )
	{
		m_isVisible |= viewMask;
	}
}

unsigned char CSimpleGlowEmitter::CurrentViewMask() const
{
	int viewId = (int)CurrentViewID();
	viewId = clamp(viewId, 0, 7);
	return 1<<viewId;
}

void CSimpleGlowEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	unsigned char viewMask = CurrentViewMask();
	if ( !WasTestedInView(CurrentViewMask()) )
	{
		pixelvis_queryparams_t params;
		params.Init(GetSortOrigin());
		
		float visible = PixelVisibility_FractionVisible( params, &m_queryHandle );
		if ( visible == 0.0f )
		{
			if ( (gpGlobals->curtime - m_startTime) <= 0.1f )
				return;
			SetVisibleInView(viewMask, false);
		}
		else
		{
			SetVisibleInView(viewMask, true);
		}

		SetTestedInView(viewMask, true);
	}
	if ( !IsVisibleInView(viewMask) )
		return;

	BaseClass::RenderParticles( pIterator );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CTrailParticles::CTrailParticles( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
	m_fFlags			= 0;
	m_flVelocityDampen	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Test for surrounding collision surfaces for quick collision testing for the particle system
// Input  : &origin - starting position
//			*dir - direction of movement (if NULL, will do a point emission test in four directions)
//			angularSpread - looseness of the spread
//			minSpeed - minimum speed
//			maxSpeed - maximum speed
//			gravity - particle gravity for the sytem
//			dampen - dampening amount on collisions
//			flags - extra information
//-----------------------------------------------------------------------------
void CTrailParticles::Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags, bool bNotCollideable )
{
	//Take the flags
	if ( !bNotCollideable )
	{
		SetFlag( (flags|bitsPARTICLE_TRAIL_COLLIDE) ); //Force this if they've called this function
	}
	else
	{
		SetFlag( flags );
	}

	//See if we've specified a direction
	m_ParticleCollision.Setup( origin, direction, angularSpread, minSpeed, maxSpeed, gravity, dampen );
}

void CTrailParticles::RenderParticles( CParticleRenderIterator *pIterator )
{
	const TrailParticle *pParticle = (const TrailParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Get our remaining time
		float lifePerc = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime  );
		float scale = (pParticle->m_flLength*lifePerc);

		if ( scale < 0.01f )
			scale = 0.01f;

		Vector	start, delta;

		//NOTE: We need to do everything in screen space
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, start );
		float sortKey = start.z;

		Vector3DMultiply( ParticleMgr()->GetModelView(), pParticle->m_vecVelocity, delta );
		
		float	color[4];
		float	ramp = 1.0;

		// Fade in for the first few frames
		if ( pParticle->m_flLifetime <= 0.3 && m_fFlags & bitsPARTICLE_TRAIL_FADE_IN )
		{
			ramp = pParticle->m_flLifetime;
		}
		else if ( m_fFlags & bitsPARTICLE_TRAIL_FADE )
		{
			ramp = ( 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime  ) );
		}

		color[0] = pParticle->m_color.r * ramp * (1.0f / 255.0f);
		color[1] = pParticle->m_color.g * ramp * (1.0f / 255.0f);
		color[2] = pParticle->m_color.b * ramp * (1.0f / 255.0f);
		color[3] = pParticle->m_color.a * ramp * (1.0f / 255.0f);

		float	flLength = (pParticle->m_vecVelocity * scale).Length();//( delta - pos ).Length();
		float	flWidth	 = ( flLength < pParticle->m_flWidth ) ? flLength : pParticle->m_flWidth;

		//See if we should fade
		Tracer_Draw( pIterator->GetParticleDraw(), start, (delta*scale), flWidth, color );
		
		pParticle = (const TrailParticle*)pIterator->GetNext( sortKey );
	}
}

void CTrailParticles::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	//Turn off collision if we're not told to do it
	if (( m_fFlags & bitsPARTICLE_TRAIL_COLLIDE )==false)
	{
		m_ParticleCollision.ClearActivePlanes();
	}

	TrailParticle *pParticle = (TrailParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		const float	timeDelta = pIterator->GetTimeDelta();

		//Simulate the movement with collision
		trace_t trace;
		m_ParticleCollision.MoveParticle( pParticle->m_Pos, pParticle->m_vecVelocity, nullptr, timeDelta, &trace );

		//Laterally dampen if asked to do so
		if ( m_fFlags & bitsPARTICLE_TRAIL_VELOCITY_DAMPEN )
		{
			float attenuation = 1.0f - (timeDelta * m_flVelocityDampen);

			if ( attenuation < 0.0f )
				attenuation = 0.0f;

			//Laterally dampen
			pParticle->m_vecVelocity[0] *= attenuation;
			pParticle->m_vecVelocity[1] *= attenuation;
			pParticle->m_vecVelocity[2] *= attenuation;
		}

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (TrailParticle*)pIterator->GetNext();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Electric spark
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
static constexpr auto SPARK_ELECTRIC_SPREAD = 0.0f;
static constexpr auto SPARK_ELECTRIC_MINSPEED = 64.0f;
static constexpr auto SPARK_ELECTRIC_MAXSPEED = 300.0f;
static constexpr auto SPARK_ELECTRIC_GRAVITY = 800.0f;
static constexpr auto SPARK_ELECTRIC_DAMPEN = 0.3f;

void FX_ElectricSpark( const Vector &pos, int nMagnitude, int nTrailLength, const Vector *vecDir )
{
	CVProfScope _( "FX_ElectricSpark", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_ElectricSpark 1" );

	if ( !pSparkEmitter )
	{
		contract_assert( 0 );
		return;
	}

	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = pSparkEmitter->GetPMaterial( "effects/spark" );
	}

	//Setup our collision information
	pSparkEmitter->Setup( (Vector &) pos, 
							nullptr, 
							SPARK_ELECTRIC_SPREAD, 
							SPARK_ELECTRIC_MINSPEED, 
							SPARK_ELECTRIC_MAXSPEED, 
							SPARK_ELECTRIC_GRAVITY, 
							SPARK_ELECTRIC_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( pos );

	//
	// Big sparks.
	//
	Vector	dir;
	int		numSparks = nMagnitude * nMagnitude * random_valve->RandomFloat( 2, 4 );

	int i;
	TrailParticle	*pParticle;
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, pos );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= nMagnitude * random_valve->RandomFloat( 1.0f, 2.0f );

		dir.Random( -1.0f, 1.0f );
		dir[2] = random_valve->RandomFloat( 0.5f, 1.0f );
	
		if ( vecDir )
		{
			dir += 2 * (*vecDir);
			VectorNormalize( dir );
		}
			
		pParticle->m_flWidth		= random_valve->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= nTrailLength * random_valve->RandomFloat( 0.02, 0.05f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( SPARK_ELECTRIC_MINSPEED, SPARK_ELECTRIC_MAXSPEED );

		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	//
	// Little sparks
	//
	
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter2	= CTrailParticles::Create( "FX_ElectricSpark 2" );

	if ( !pSparkEmitter2 )
	{
		contract_assert( 0 );
		return;
	}

	pSparkEmitter2->SetSortOrigin( pos );
	
	pSparkEmitter2->m_ParticleCollision.SetGravity( 400.0f );
	pSparkEmitter2->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	numSparks = nMagnitude * random_valve->RandomInt( 16, 32 );

	// Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter2->AddParticle( sizeof(TrailParticle), g_Material_Spark, pos );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;

		dir.Random( -1.0f, 1.0f );
		if ( vecDir )
		{
			dir += *vecDir;
			VectorNormalize( dir );
		}
		
		pParticle->m_flWidth		= random_valve->RandomFloat( 2.0f, 4.0f );
		pParticle->m_flLength		= nTrailLength * random_valve->RandomFloat( 0.02f, 0.03f );
		pParticle->m_flDieTime		= nMagnitude * random_valve->RandomFloat( 0.1f, 0.2f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 128, 256 );

		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	//
	// Caps
	//
	ks::intrusive_ptr<CSimpleGlowEmitter> pSimple = CSimpleGlowEmitter::Create( "FX_ElectricSpark 3", pos, gpGlobals->curtime + 0.2 );

	// NOTE: None of these will render unless the effect is visible!
	//
	// Inner glow
	//
	SimpleParticle *sParticle;

	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/yellowflare_noz" ), pos );
		
	if ( sParticle == nullptr )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 0.2f;
	
	sParticle->m_vecVelocity.Init();

	sParticle->m_uchColor[0]	= 255;
	sParticle->m_uchColor[1]	= 255;
	sParticle->m_uchColor[2]	= 255;
	sParticle->m_uchStartAlpha	= 255;
	sParticle->m_uchEndAlpha	= 255;
	sParticle->m_uchStartSize	= nMagnitude * random_valve->RandomInt( 4, 8 );
	sParticle->m_uchEndSize		= 0;
	sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= 0.0f;
	
	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/yellowflare_noz" ), pos );
		
	if ( sParticle == nullptr )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 0.2f;
	
	sParticle->m_vecVelocity.Init();

	float	fColor = random_valve->RandomInt( 32, 64 );
	sParticle->m_uchColor[0]	= fColor;
	sParticle->m_uchColor[1]	= fColor;
	sParticle->m_uchColor[2]	= fColor;
	sParticle->m_uchStartAlpha	= fColor;
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= nMagnitude * random_valve->RandomInt( 32, 64 );
	sParticle->m_uchEndSize		= 0;
	sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= random_valve->RandomFloat( -1.0f, 1.0f );

	//
	// Smoke
	//
	Vector	sOffs;

	sOffs[0] = pos[0] + random_valve->RandomFloat( -4.0f, 4.0f );
	sOffs[1] = pos[1] + random_valve->RandomFloat( -4.0f, 4.0f );
	sOffs[2] = pos[2];

	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[1], sOffs );
		
	if ( sParticle == nullptr )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 1.0f;
	
	sParticle->m_vecVelocity.Init();
	
	sParticle->m_vecVelocity[2] = 16.0f;
	
	sParticle->m_vecVelocity[0] = random_valve->RandomFloat( -16.0f, 16.0f );
	sParticle->m_vecVelocity[1] = random_valve->RandomFloat( -16.0f, 16.0f );

	sParticle->m_uchColor[0]	= 255;
	sParticle->m_uchColor[1]	= 255;
	sParticle->m_uchColor[2]	= 200;
	sParticle->m_uchStartAlpha	= random_valve->RandomInt( 16, 32 );
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= random_valve->RandomInt( 4, 8 );
	sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4.0f;
	sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );

	//
	// Dlight
	//

	/*
	dlight_t *dl= effects->CL_AllocDlight ( 0 );

	dl->origin	= pos;
	dl->color.r = dl->color.g = dl->color.b = 250;
	dl->radius	= random->RandomFloat(16,32);
	dl->die		= gpGlobals->curtime + 0.001;
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Sparks created by scraping metal
// Input  : &position - start
//			&normal - direction of spark travel
//-----------------------------------------------------------------------------

static constexpr auto METAL_SCRAPE_MINSPEED = 128.0f;
static constexpr auto METAL_SCRAPE_MAXSPEED = 512.0f;
static constexpr auto METAL_SCRAPE_SPREAD = 0.3f;
static constexpr auto METAL_SCRAPE_GRAVITY = 800.0f;
static constexpr auto METAL_SCRAPE_DAMPEN = 0.4f;

void FX_MetalScrape( Vector &position, Vector &normal )
{
	CVProfScope _( "FX_MetalScrape", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	offset = position + ( normal * 1.0f );

	ks::intrusive_ptr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalScrape 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						METAL_SCRAPE_SPREAD, 
						METAL_SCRAPE_MINSPEED, 
						METAL_SCRAPE_MAXSPEED, 
						METAL_SCRAPE_GRAVITY, 
						METAL_SCRAPE_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	int	numSparks = random_valve->RandomInt( 4, 8 );
	
	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = sparkEmitter->GetPMaterial( "effects/spark" );
	}

	Vector	dir;
	TrailParticle *pParticle;
	float	length	= 0.06f;

	//Dump out sparks
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, offset );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;

		float	spreadOfs = random_valve->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[1] = normal[1] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[2] = normal[2] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
	
		pParticle->m_flWidth		= random_valve->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= random_valve->RandomFloat( length*0.25f, length );
		pParticle->m_flDieTime		= random_valve->RandomFloat( 2.0f, 2.0f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( (METAL_SCRAPE_MINSPEED*(2.0f-spreadOfs)), (METAL_SCRAPE_MAXSPEED*(2.0f-spreadOfs)) );
		
		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ricochet spark on metal
// Input  : &position - origin of effect
//			&normal - normal of the surface struck
//-----------------------------------------------------------------------------
static constexpr auto METAL_SPARK_SPREAD = 0.5f;
static constexpr auto METAL_SPARK_MINSPEED = 128.0f;
static constexpr auto METAL_SPARK_MAXSPEED = 512.0f;
static constexpr auto METAL_SPARK_GRAVITY = 400.0f;
static constexpr auto METAL_SPARK_DAMPEN = 0.25f;

void FX_MetalSpark( const Vector &position, const Vector &direction, const Vector &surfaceNormal, int iScale )
{
	CVProfScope _( "FX_MetalSpark", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	if ( !fx_drawmetalspark.GetBool() )
		return;

	//
	// Emitted particles
	//

	Vector offset = position + ( surfaceNormal * 1.0f );

	ks::intrusive_ptr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalSpark 1" );

	if ( sparkEmitter == nullptr )
		return;

	//Setup our information
	sparkEmitter->SetSortOrigin( offset );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 8.0f );
	sparkEmitter->SetGravity( METAL_SPARK_GRAVITY );
	sparkEmitter->SetCollisionDamped( METAL_SPARK_DAMPEN );
	sparkEmitter->GetBinding().SetBBox( offset - Vector( 32, 32, 32 ), offset + Vector( 32, 32, 32 ) );

	int	numSparks = random_valve->RandomInt( 4, 8 ) * ( iScale * 2 );
	
	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = sparkEmitter->GetPMaterial( "effects/spark" );
	}

	TrailParticle	*pParticle;
	Vector	dir;
	float	length	= 0.1f;

	//Dump out sparks
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, offset );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;
		
		if( iScale > 1 && i%3 == 0 )
		{
			// Every third spark goes flying far if we're having a big batch of sparks.
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.15f, 0.25f );
		}
		else
		{
			pParticle->m_flDieTime	= random_valve->RandomFloat( 0.05f, 0.1f );
		}

		float	spreadOfs = random_valve->RandomFloat( 0.0f, 2.0f );

		dir[0] = direction[0] + random_valve->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
		dir[1] = direction[1] + random_valve->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
		dir[2] = direction[2] + random_valve->RandomFloat( -(METAL_SPARK_SPREAD*spreadOfs), (METAL_SPARK_SPREAD*spreadOfs) );
	
		VectorNormalize( dir );

		pParticle->m_flWidth		= random_valve->RandomFloat( 1.0f, 4.0f );
		pParticle->m_flLength		= random_valve->RandomFloat( length*0.25f, length );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( (METAL_SPARK_MINSPEED*(2.0f-spreadOfs)), (METAL_SPARK_MAXSPEED*(2.0f-spreadOfs)) );
		
		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	//
	// Impact point glow
	//

	FXQuadData_t data;

	data.SetMaterial( "effects/yellowflare" );
	data.SetColor( 1.0f, 1.0f, 1.0f );
	data.SetOrigin( offset );
	data.SetNormal( surfaceNormal );
	data.SetAlpha( 1.0f, 0.0f );
	data.SetLifeTime( 0.1f );
	data.SetYaw( random_valve->RandomInt( 0, 360 ) );
	
	int scale = random_valve->RandomInt( 24, 28 );
	data.SetScale( scale, 0 );

	FX_AddQuad( data );
}

//-----------------------------------------------------------------------------
// Purpose: Spark effect. Nothing but sparks.
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
static constexpr auto SPARK_SPREAD = 3.0f;
static constexpr auto SPARK_GRAVITY = 800.0f;
static constexpr auto SPARK_DAMPEN = 0.3f;

void FX_Sparks( const Vector &pos, int nMagnitude, int nTrailLength, const Vector &vecDir, float flWidth, float flMinSpeed, float flMaxSpeed, char *pSparkMaterial )
{
	CVProfScope _( "FX_Sparks", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_Sparks 1" );

	if ( !pSparkEmitter )
	{
		contract_assert( 0 );
		return;
	}

	PMaterialHandle hMaterial;
	if ( pSparkMaterial )
	{
		hMaterial = pSparkEmitter->GetPMaterial( pSparkMaterial );
	}
	else
	{
		if ( g_Material_Spark == nullptr )
		{
			g_Material_Spark = pSparkEmitter->GetPMaterial( "effects/spark" );
		}

		hMaterial = g_Material_Spark;
	}

	//Setup our collision information
	pSparkEmitter->Setup( (Vector &) pos, 
							nullptr, 
							SPARK_SPREAD, 
							flMinSpeed, 
							flMaxSpeed, 
							SPARK_GRAVITY, 
							SPARK_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( pos );

	//
	// Big sparks.
	//
	Vector	dir;
	int		numSparks = nMagnitude * nMagnitude * random_valve->RandomFloat( 2, 4 );

	int i;
	TrailParticle	*pParticle;
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= nMagnitude * random_valve->RandomFloat( 1.0f, 2.0f );

		float	spreadOfs = random_valve->RandomFloat( 0.0f, 2.0f );
		dir[0] = vecDir[0] + random_valve->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		dir[1] = vecDir[1] + random_valve->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		dir[2] = vecDir[2] + random_valve->RandomFloat( -(SPARK_SPREAD*spreadOfs), (SPARK_SPREAD*spreadOfs) );
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( (flMinSpeed*(2.0f-spreadOfs)), (flMaxSpeed*(2.0f-spreadOfs)) );
			
		pParticle->m_flWidth		= flWidth + random_valve->RandomFloat( 0.0f, 0.5f );
		pParticle->m_flLength		= nTrailLength * random_valve->RandomFloat( 0.02, 0.05f );
		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	//
	// Little sparks
	//
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter2	= CTrailParticles::Create( "FX_ElectricSpark 2" );

	if ( !pSparkEmitter2 )
	{
		contract_assert( 0 );
		return;
	}

	if ( pSparkMaterial )
	{
		hMaterial = pSparkEmitter->GetPMaterial( pSparkMaterial );
	}
	else
	{
		if ( g_Material_Spark == nullptr )
		{
			g_Material_Spark = pSparkEmitter2->GetPMaterial( "effects/spark" );
		}
		
		hMaterial = g_Material_Spark;
	}

	pSparkEmitter2->SetSortOrigin( pos );
	
	pSparkEmitter2->m_ParticleCollision.SetGravity( 400.0f );
	pSparkEmitter2->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	numSparks = nMagnitude * random_valve->RandomInt( 4, 8 );

	// Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter2->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;

		dir.Random( -1.0f, 1.0f );
		dir += vecDir;
		VectorNormalize( dir );
		
		pParticle->m_flWidth		= (flWidth * 0.25) + random_valve->RandomFloat( 0.0f, 0.5f );
		pParticle->m_flLength		= nTrailLength * random_valve->RandomFloat( 0.02f, 0.03f );
		pParticle->m_flDieTime		= nMagnitude * random_valve->RandomFloat( 0.3f, 0.5f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( flMinSpeed, flMaxSpeed );
		
		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Energy splash for plasma/beam weapon impacts
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
static constexpr auto ENERGY_SPLASH_SPREAD = 0.7f;
static constexpr auto ENERGY_SPLASH_MINSPEED = 128.0f;
static constexpr auto ENERGY_SPLASH_MAXSPEED = 160.0f;
static constexpr auto ENERGY_SPLASH_GRAVITY = 800.0f;
static constexpr auto ENERGY_SPLASH_DAMPEN = 0.3f;

void FX_EnergySplash( const Vector &pos, const Vector &normal, int nFlags )
{
	CVProfScope _( "FX_EnergySplash", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	offset = pos + ( normal * 2.0f );

	// Quick flash
	FX_AddQuad( pos,
				normal,
				64.0f,
				0,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random_valve->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.25f, 
				"effects/combinemuzzle1_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	// Lingering burn
	FX_AddQuad( pos,
				normal, 
				16,
				32,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random_valve->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.5f, 
				"effects/combinemuzzle2_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	SimpleParticle *sParticle;

	ks::intrusive_ptr<CSimpleEmitter> pEmitter;

	pEmitter = CSimpleEmitter::Create( "CEntityDissolve" );
	pEmitter->SetSortOrigin( pos );

	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = pEmitter->GetPMaterial( "effects/spark" );
	}

	// Anime ground effects
	for ( int j = 0; j < 8; j++ )
	{
		offset.x = random_valve->RandomFloat( -8.0f, 8.0f );
		offset.y = random_valve->RandomFloat( -8.0f, 8.0f );
		offset.z = random_valve->RandomFloat( 0.0f, 4.0f );

		offset += pos;

		sParticle = (SimpleParticle *) pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_Spark, offset );
		
		if ( sParticle == nullptr )
			return;

		sParticle->m_vecVelocity = Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( 16.0f, 64.0f ) );
		
		sParticle->m_uchStartSize	= random_valve->RandomFloat( 2, 4 );

		sParticle->m_flDieTime = random_valve->RandomFloat( 0.4f, 0.6f );
		
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );

		float alpha = 255;

		sParticle->m_flRollDelta	= Helper_RandomFloat( -4.0f, 4.0f );
		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchEndSize		= 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Micro-Explosion effect
// Input  : &position - origin of effect
//			&normal - normal of the surface struck
//-----------------------------------------------------------------------------

static constexpr auto MICRO_EXPLOSION_MINSPEED = 100.0f;
static constexpr auto MICRO_EXPLOSION_MAXSPEED = 150.0f;
static constexpr auto MICRO_EXPLOSION_SPREAD = 1.0f;
static constexpr auto MICRO_EXPLOSION_GRAVITY = 0.0f;
static constexpr auto MICRO_EXPLOSION_DAMPEN = 0.4f;

void FX_MicroExplosion( Vector &position, Vector &normal )
{
	CVProfScope _( "FX_MicroExplosion", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	offset = position + ( normal * 2.0f );

	ks::intrusive_ptr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MicroExplosion 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						MICRO_EXPLOSION_SPREAD, 
						MICRO_EXPLOSION_MINSPEED, 
						MICRO_EXPLOSION_MAXSPEED, 
						MICRO_EXPLOSION_GRAVITY, 
						MICRO_EXPLOSION_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	int	numSparks = random_valve->RandomInt( 8, 16 );
	
	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = sparkEmitter->GetPMaterial( "effects/spark" );
	}

	TrailParticle	*pParticle;
	Vector	dir, vOfs;
	float	length = 0.2f;

	//Fast lines
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, offset );

		if ( pParticle )
		{
			pParticle->m_flLifetime	= 0.0f;

			float	ramp = ( (float) i / (float)numSparks );

			dir[0] = normal[0] + random_valve->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
			dir[1] = normal[1] + random_valve->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
			dir[2] = normal[2] + random_valve->RandomFloat( -MICRO_EXPLOSION_SPREAD*ramp, MICRO_EXPLOSION_SPREAD*ramp );
		
			pParticle->m_flWidth		= random_valve->RandomFloat( 5.0f, 10.0f );
			pParticle->m_flLength		= (length*((1.0f-ramp)*(1.0f-ramp)*0.5f));
			pParticle->m_flDieTime		= 0.2f;
			pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( MICRO_EXPLOSION_MINSPEED*(1.5f-ramp), MICRO_EXPLOSION_MAXSPEED*(1.5f-ramp) );

			Color32Init( pParticle->m_color, 255, 255, 255, 255 );
		}
	}

	//
	// Filler
	//

	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_MicroExplosion 2" );
	pSimple->SetSortOrigin( offset );
	
	SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "sprites/rico1" ), offset );
		
	if ( sParticle )
	{
		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.3f;
		
		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 255;
		sParticle->m_uchColor[2]	= 255;
		sParticle->m_uchStartAlpha	= random_valve->RandomInt( 128, 255 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random_valve->RandomInt( 12, 16 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ugly prototype explosion effect
//-----------------------------------------------------------------------------
static constexpr auto EXPLOSION_MINSPEED = 300.0f;
static constexpr auto EXPLOSION_MAXSPEED = 300.0f;
static constexpr auto EXPLOSION_SPREAD = 0.8f;
static constexpr auto EXPLOSION_GRAVITY = 800.0f;
static constexpr auto EXPLOSION_DAMPEN = 0.4f;

static constexpr auto EXPLOSION_FLECK_MIN_SPEED = 150.0f;
static constexpr auto EXPLOSION_FLECK_MAX_SPEED = 350.0f;
static constexpr auto EXPLOSION_FLECK_GRAVITY = 800.0f;
static constexpr auto EXPLOSION_FLECK_DAMPEN = 0.3f;
static constexpr auto EXPLOSION_FLECK_ANGULAR_SPRAY = 0.8f;

void FX_Explosion( Vector& origin, Vector& normal, char materialType )
{
	CVProfScope _( "FX_Explosion", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	offset = origin + ( normal * 2.0f );

	ks::intrusive_ptr<CTrailParticles> pSparkEmitter = CTrailParticles::Create( "FX_Explosion 1" );
	if ( !pSparkEmitter )
		return;

	// Get color data from our hit point
	IMaterial	*pTraceMaterial;
	Vector		diffuseColor, baseColor;
	pTraceMaterial = engine->TraceLineMaterialAndLighting( origin, normal * -16.0f, diffuseColor, baseColor );
	// Get final light value
	float r = std::pow( diffuseColor[0], 1.0f/2.2f ) * baseColor[0];
	float g = pow( diffuseColor[1], 1.0f/2.2f ) * baseColor[1];
	float b = pow( diffuseColor[2], 1.0f/2.2f ) * baseColor[2];

	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = pSparkEmitter->GetPMaterial( "effects/spark" );
	}

	// Setup our collision information
	pSparkEmitter->Setup(	offset, 
							&normal, 
							EXPLOSION_SPREAD, 
							EXPLOSION_MINSPEED, 
							EXPLOSION_MAXSPEED, 
							EXPLOSION_GRAVITY, 
							EXPLOSION_DAMPEN, 
							bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	pSparkEmitter->SetSortOrigin( offset );

	Vector	dir;
	int		numSparks = random_valve->RandomInt( 8,16 );

	// Dump out sparks
	int i;
	for ( i = 0; i < numSparks; i++ )
	{
		TrailParticle *pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, offset );

		if ( pParticle == nullptr )
			break;

		pParticle->m_flLifetime	= 0.0f;

		pParticle->m_flWidth		= random_valve->RandomFloat( 5.0f, 10.0f );
		pParticle->m_flLength		= random_valve->RandomFloat( 0.05, 0.1f );
		pParticle->m_flDieTime		= random_valve->RandomFloat( 1.0f, 2.0f );
		
		dir[0] = normal[0] + random_valve->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		dir[1] = normal[1] + random_valve->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		dir[2] = normal[2] + random_valve->RandomFloat( -EXPLOSION_SPREAD, EXPLOSION_SPREAD );
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( EXPLOSION_MINSPEED, EXPLOSION_MAXSPEED );

		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	// Chunks o'dirt
	// Only create dirt chunks on concrete/world
	if ( materialType == 'C' || materialType == 'W' )
	{
		ks::intrusive_ptr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "FX_Explosion 10", offset, Vector(5,5,5) );
		if ( !fleckEmitter )
			return;

		// Setup our collision information
		fleckEmitter->m_ParticleCollision.Setup( offset, &normal, EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_MIN_SPEED, EXPLOSION_FLECK_MAX_SPEED, EXPLOSION_FLECK_GRAVITY, EXPLOSION_FLECK_DAMPEN );

		PMaterialHandle	*hMaterialArray;
		
		switch ( materialType )
		{
		case 'C':
		case 'c':
		default:
			hMaterialArray = g_Mat_Fleck_Cement;
			break;
		}

		int	numFlecks = random_valve->RandomInt( 48, 64 );
		// Dump out flecks
		for ( i = 0; i < numFlecks; i++ )
		{
			FleckParticle *pParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), hMaterialArray[random_valve->RandomInt(0,1)], offset );

			if ( pParticle == nullptr )
				break;

			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime		= 3.0f;
			dir[0] = normal[0] + random_valve->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			dir[1] = normal[1] + random_valve->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			dir[2] = normal[2] + random_valve->RandomFloat( -EXPLOSION_FLECK_ANGULAR_SPRAY, EXPLOSION_FLECK_ANGULAR_SPRAY );
			pParticle->m_uchSize		= random_valve->RandomInt( 2, 6 );
			pParticle->m_vecVelocity	= dir * ( random_valve->RandomFloat( EXPLOSION_FLECK_MIN_SPEED, EXPLOSION_FLECK_MAX_SPEED ) * ( 7 - pParticle->m_uchSize ) );
			pParticle->m_flRoll		= random_valve->RandomFloat( 0, 360 );
			pParticle->m_flRollDelta	= random_valve->RandomFloat( 0, 360 );

			float colorRamp = random_valve->RandomFloat( 0.75f, 1.5f );
			pParticle->m_uchColor[0] = std::min( 1.0f, r*colorRamp )*255.0f;
			pParticle->m_uchColor[1] = std::min( 1.0f, g*colorRamp )*255.0f;
			pParticle->m_uchColor[2] = std::min( 1.0f, b*colorRamp )*255.0f;
		}
	}

	// Large sphere bursts
	ks::intrusive_ptr<CSimpleEmitter> pSimpleEmitter = CSimpleEmitter::Create( "FX_Explosion 1" );
	PMaterialHandle	hSphereMaterial = g_Mat_DustPuff[1];
	Vector vecBurstOrigin = offset + normal * 8.0;
	pSimpleEmitter->SetSortOrigin( vecBurstOrigin );
	SimpleParticle *pSphereParticle = (SimpleParticle *) pSimpleEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecBurstOrigin );
	if ( pSphereParticle )
	{
		pSphereParticle->m_flLifetime		= 0.0f;
		pSphereParticle->m_flDieTime		= 0.3f;
		pSphereParticle->m_uchStartAlpha	= 150.0;
		pSphereParticle->m_uchEndAlpha		= 64.0;
		pSphereParticle->m_uchStartSize		= 0.0;
		pSphereParticle->m_uchEndSize		= 255.0;
		pSphereParticle->m_vecVelocity		= Vector(0,0,0);

		float colorRamp = random_valve->RandomFloat( 0.75f, 1.5f );
		pSphereParticle->m_uchColor[0] = std::min( 1.0f, r*colorRamp )*255.0f;
		pSphereParticle->m_uchColor[1] = std::min( 1.0f, g*colorRamp )*255.0f;
		pSphereParticle->m_uchColor[2] = std::min( 1.0f, b*colorRamp )*255.0f;
	}

	// Throw some smoke balls out around the normal
	int numBalls = 12;
	Vector vecRight, vecForward, vecUp;
	QAngle vecAngles;
	VectorAngles( normal, vecAngles );
	AngleVectors( vecAngles, nullptr, &vecRight, &vecUp );
	for ( i = 0; i < numBalls; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSimpleEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecBurstOrigin );
		if ( pParticle )
		{
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= 0.25f;
			pParticle->m_uchStartAlpha	= 128.0;
			pParticle->m_uchEndAlpha	= 64.0;
			pParticle->m_uchStartSize	= 16.0;
			pParticle->m_uchEndSize		= 64.0;

			float flAngle = ((float)i * std::numbers::pi * 2) / numBalls;
			float x = std::cos( flAngle );
			float y = std::sin( flAngle );
			pParticle->m_vecVelocity = (vecRight*x + vecUp*y) * 1024.0;

			float colorRamp = random_valve->RandomFloat( 0.75f, 1.5f );
			pParticle->m_uchColor[0] = std::min( 1.0f, r*colorRamp )*255.0f;
			pParticle->m_uchColor[1] = std::min( 1.0f, g*colorRamp )*255.0f;
			pParticle->m_uchColor[2] = std::min( 1.0f, b*colorRamp )*255.0f;
		}
	}

	// Create a couple of big, floating smoke clouds
	ks::intrusive_ptr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "FX_Explosion 2" );
	pSmokeEmitter->SetSortOrigin( offset );
	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], offset );
		if ( pParticle == nullptr )
			break;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random_valve->RandomFloat( 2.0f, 3.0f );
		pParticle->m_uchStartSize	= 64;
		pParticle->m_uchEndSize		= 255;
		dir[0] = normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );
		pParticle->m_vecVelocity = dir * random_valve->RandomFloat( 2.0f, 24.0f )*(i+1);
		pParticle->m_uchStartAlpha	= 160;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random_valve->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		float colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );
		pParticle->m_uchColor[0] = std::min( 1.0f, r*colorRamp )*255.0f;
		pParticle->m_uchColor[1] = std::min( 1.0f, g*colorRamp )*255.0f;
		pParticle->m_uchColor[2] = std::min( 1.0f, b*colorRamp )*255.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//-----------------------------------------------------------------------------
void FX_ConcussiveExplosion( Vector &origin, Vector &normal )
{
	CVProfScope _( "FX_ConcussiveExplosion", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	offset = origin + ( normal * 2.0f );
	Vector	dir;
	int		i;

	// 
	// Smoke
	//

	ks::intrusive_ptr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "FX_ConcussiveExplosion 1" );
	pSmokeEmitter->SetSortOrigin( offset );

	//Quick moving sprites
	 for ( i = 0; i < 16; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], offset );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random_valve->RandomFloat( 0.2f, 0.4f );
		pParticle->m_uchStartSize	= random_valve->RandomInt( 4, 8 );
		pParticle->m_uchEndSize		= random_valve->RandomInt( 32, 64 );
		
		dir[0] = random_valve->RandomFloat( -1.0f, 1.0f );
		dir[1] = random_valve->RandomFloat( -1.0f, 1.0f );
		dir[2] = random_valve->RandomFloat( -1.0f, 1.0f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 64.0f, 128.0f );
		pParticle->m_uchStartAlpha	= random_valve->RandomInt( 64, 128 );
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random_valve->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -4, 4 );

		int colorRamp = random_valve->RandomFloat( 235, 255 );
		pParticle->m_uchColor[0] = colorRamp;
		pParticle->m_uchColor[1] = colorRamp;
		pParticle->m_uchColor[2] = colorRamp;
	}

	//Slow lingering sprites
	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], offset );
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random_valve->RandomFloat( 1.0f, 2.0f );
		pParticle->m_uchStartSize	= random_valve->RandomInt( 32, 64 );
		pParticle->m_uchEndSize		= random_valve->RandomInt( 100, 128 );

		dir[0] = normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

		pParticle->m_vecVelocity = dir * random_valve->RandomFloat( 16.0f, 32.0f );

		pParticle->m_uchStartAlpha	= random_valve->RandomInt( 32, 64 );
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random_valve->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		int colorRamp = random_valve->RandomFloat( 235, 255 );
		pParticle->m_uchColor[0] = colorRamp;
		pParticle->m_uchColor[1] = colorRamp;
		pParticle->m_uchColor[2] = colorRamp;
	}
	
	//	
	// Quick sphere
	//

	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_ConcussiveExplosion 2" );

	pSimple->SetSortOrigin( offset );
	
	SimpleParticle *pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), pSimple->GetPMaterial( "effects/blueflare1" ), offset );
	
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 0.1f;
		pParticle->m_vecVelocity.Init();
		pParticle->m_flRoll			= random_valve->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0] = pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = 128;
		
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= 16;
		pParticle->m_uchEndSize		= 64;
	}
	
	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), pSimple->GetPMaterial( "effects/blueflare1" ), offset );
	
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 0.2f;
		pParticle->m_vecVelocity.Init();
		pParticle->m_flRoll		= random_valve->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -1, 1 );
		pParticle->m_uchColor[0] = pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = 32;
		
		pParticle->m_uchStartAlpha	= 64;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= 64;
		pParticle->m_uchEndSize		= 128;
	}

	//
	// Dlight
	//

	dlight_t *dl= effects->CL_AllocDlight ( 0 );

	dl->origin	= offset;
	dl->color.r = dl->color.g = dl->color.b = 64;
	dl->radius	= random_valve->RandomFloat(128,256);
	dl->die		= gpGlobals->curtime + 0.1;

	//
	// Moving lines
	//

	TrailParticle	*pTrailParticle;
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter	= CTrailParticles::Create( "FX_ConcussiveExplosion 3" );
	PMaterialHandle hMaterial;
	int				numSparks;

	if ( pSparkEmitter )
	{
		hMaterial= pSparkEmitter->GetPMaterial( "effects/blueflare1" );

		pSparkEmitter->SetSortOrigin( offset );
		pSparkEmitter->m_ParticleCollision.SetGravity( 0.0f );

		numSparks = random_valve->RandomInt( 16, 32 );

		//Dump out sparks
		for ( i = 0; i < numSparks; i++ )
		{
			pTrailParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( pTrailParticle == nullptr )
				return;

			pTrailParticle->m_flLifetime	= 0.0f;

			dir.Random( -1.0f, 1.0f );

			pTrailParticle->m_flWidth		= random_valve->RandomFloat( 1.0f, 2.0f );
			pTrailParticle->m_flLength		= random_valve->RandomFloat( 0.01f, 0.1f );
			pTrailParticle->m_flDieTime		= random_valve->RandomFloat( 0.1f, 0.2f );
			
			pTrailParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 800, 1000 );

			float colorRamp = random_valve->RandomFloat( 0.75f, 1.0f );
			FloatToColor32( pTrailParticle->m_color, colorRamp, colorRamp, 1.0f, 1.0f );
		}
	}

	//Moving particles
	ks::intrusive_ptr<CTrailParticles> pCollisionEmitter	= CTrailParticles::Create( "FX_ConcussiveExplosion 4" );

	if ( pCollisionEmitter )
	{
		//Setup our collision information
		pCollisionEmitter->Setup( (Vector &) offset,
								nullptr, 
								SPARK_ELECTRIC_SPREAD, 
								SPARK_ELECTRIC_MINSPEED*6, 
								SPARK_ELECTRIC_MAXSPEED*6, 
								-400, 
								SPARK_ELECTRIC_DAMPEN, 
								bitsPARTICLE_TRAIL_FADE );

		pCollisionEmitter->SetSortOrigin( offset );

		numSparks = random_valve->RandomInt( 8, 16 );
		hMaterial = pCollisionEmitter->GetPMaterial( "effects/blueflare1" );

		//Dump out sparks
		for ( i = 0; i < numSparks; i++ )
		{
			pTrailParticle = (TrailParticle *) pCollisionEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( pTrailParticle == nullptr )
				return;

			pTrailParticle->m_flLifetime	= 0.0f;

			dir.Random( -1.0f, 1.0f );
			dir[2] = random_valve->RandomFloat( 0.0f, 0.75f );

			pTrailParticle->m_flWidth		= random_valve->RandomFloat( 1.0f, 2.0f );
			pTrailParticle->m_flLength		= random_valve->RandomFloat( 0.01f, 0.1f );
			pTrailParticle->m_flDieTime		= random_valve->RandomFloat( 0.2f, 1.0f );
			
			pTrailParticle->m_vecVelocity	= dir * random_valve->RandomFloat( 128, 512 );

			float colorRamp = random_valve->RandomFloat( 0.75f, 1.0f );
			FloatToColor32( pTrailParticle->m_color, colorRamp, colorRamp, 1.0f, 1.0f );
		}
	}
}

void FX_SparkFan( Vector &position, Vector &normal )
{
	Vector	offset = position + ( normal * 1.0f );

	ks::intrusive_ptr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_MetalScrape 1" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( offset );

	//Setup our collision information
	sparkEmitter->Setup( offset, 
						&normal, 
						METAL_SCRAPE_SPREAD, 
						METAL_SCRAPE_MINSPEED, 
						METAL_SCRAPE_MAXSPEED, 
						METAL_SCRAPE_GRAVITY, 
						METAL_SCRAPE_DAMPEN, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );

	if ( g_Material_Spark == nullptr )
	{
		g_Material_Spark = sparkEmitter->GetPMaterial( "effects/spark" );
	}

	TrailParticle	*pParticle;
	Vector			dir;

	float	length	= 0.06f;

	//Dump out sparks
	for ( int i = 0; i < 35; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), g_Material_Spark, offset );
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;

		float	spreadOfs = random_valve->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[1] = normal[1] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
		dir[2] = normal[2] + random_valve->RandomFloat( -(METAL_SCRAPE_SPREAD*spreadOfs), (METAL_SCRAPE_SPREAD*spreadOfs) );
	
		pParticle->m_flWidth		= random_valve->RandomFloat( 2.0f, 5.0f );
		pParticle->m_flLength		= random_valve->RandomFloat( length*0.25f, length );
		pParticle->m_flDieTime		= random_valve->RandomFloat( 2.0f, 2.0f );
		
		pParticle->m_vecVelocity	= dir * random_valve->RandomFloat( (METAL_SCRAPE_MINSPEED*(2.0f-spreadOfs)), (METAL_SCRAPE_MAXSPEED*(2.0f-spreadOfs)) );
		
		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}
}

void ManhackSparkCallback( const CEffectData & data )
{
	Vector vecNormal;
	Vector vecPosition;
	QAngle angles;

	vecPosition = data.m_vOrigin;
	vecNormal = data.m_vNormal;
	angles = data.m_vAngles;

	FX_SparkFan( vecPosition, vecNormal );
}

DECLARE_CLIENT_EFFECT( ManhackSparks, ManhackSparkCallback );

using namespace ::ks::reflect;

/*
==================================================
CFXStaticLine
==================================================
*/
CFXStaticLine::CFXStaticLine( const char *name, const Vector& start, const Vector& end, float scale, float life, const char *shader, unsigned int flags )
: CClientSideEffect( name )
{
	contract_assert( materials );
	if ( materials == nullptr )
		return;

	// Create a material...
	m_pMaterial = materials->FindMaterial( shader, TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pMaterial->IncrementReferenceCount();

	//Fill in the rest of the fields
	m_vecStart	= start;
	m_vecEnd	= end;
	m_uiFlags	= flags;
	m_fLife		= life;
	m_fScale	= scale * 0.5f;
}

CFXStaticLine::~CFXStaticLine( void )
{
	Destroy();
}

//==================================================
// Purpose:	Draw the primitive
// Input:	frametime - the time, this frame
//==================================================

void CFXStaticLine::Draw( double frametime )
{
	Vector lineDir, viewDir, cross;
	Vector tmp;

	// Update the effect
	Update( frametime );

	// Get the proper orientation for the line
	VectorSubtract( m_vecEnd, m_vecStart, lineDir );
	VectorSubtract( m_vecEnd, CurrentViewOrigin(), viewDir );
	
	cross = lineDir.Cross( viewDir );

	VectorNormalize( cross );

	CMatRenderContextPtr pRenderContext( materials );

	//Bind the material
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_pMaterial );
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	bool flipVertical = (m_uiFlags & FXSTATICLINE_FLIP_VERTICAL) != 0;
	bool flipHorizontal = (m_uiFlags & FXSTATICLINE_FLIP_HORIZONTAL ) != 0;

	//Setup our points
	VectorMA( m_vecStart, -m_fScale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.Normal3fv( cross.Base() );
	if (flipHorizontal)
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	else if (flipVertical)
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	else 
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	VectorMA( m_vecStart, m_fScale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.Normal3fv( cross.Base() );
	if (flipHorizontal)
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	else if (flipVertical)
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	else 
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	VectorMA( m_vecEnd, m_fScale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.Normal3fv( cross.Base() );
	if (flipHorizontal)
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	else if (flipVertical)
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	else 
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	VectorMA( m_vecEnd, -m_fScale, cross, tmp );
	meshBuilder.Position3fv( tmp.Base() );
	meshBuilder.Normal3fv( cross.Base() );
	if (flipHorizontal)
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	else if (flipVertical)
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	else 
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//==================================================
// Purpose:	Returns whether or not the effect is still active
// Output:	true if active
//==================================================

bool CFXStaticLine::IsActive( void )
{
	return ( m_fLife > 0.0 ) ? true : false;
}

//==================================================
// Purpose:	Destroy the effect and its local resources
//==================================================

void CFXStaticLine::Destroy( void )
{
	//Release the material
	if ( m_pMaterial != nullptr )
	{
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial = nullptr;
	}
}

//==================================================
// Purpose: Perform any necessary updates
// Input:	frametime - the time, this frame
//==================================================

void CFXStaticLine::Update( double frametime )
{
	m_fLife -= frametime;
}

class CEffectData;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

ConVar r_drawtracers( side_registry(), "r_drawtracers", "1", FCVAR_CHEAT );
ConVar r_drawtracers_firstperson( side_registry(), "r_drawtracers_firstperson", "1", FCVAR_ARCHIVE | FCVAR_RELEASE, "Toggle visibility of first person weapon tracers" );
ConVar r_drawtracers_movetonotintersect( side_registry(), "r_drawtracers_movetonotintersect", "1", FCVAR_CHEAT, "" );
extern ConVar cl_righthand;
void FormatViewModelAttachment( CBasePlayer *pPlayer, Vector &vOrigin, bool bInverse );

static constexpr auto TRACER_SPEED = 5000;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector GetTracerOrigin( const CEffectData &data )
{
	Vector vecStart = data.m_vStart;
	QAngle vecAngles;

	int iAttachment = data.m_nAttachmentIndex;;

	// Attachment?
	if ( data.m_fFlags & TRACER_FLAG_USEATTACHMENT )
	{
		CBaseViewModel *pViewModel = nullptr;

		// If the entity specified is a weapon being carried by this player, use the viewmodel instead
		IClientRenderable *pRenderable = data.GetRenderable();
		if ( !pRenderable )
			return vecStart;

		CBaseEntity *pEnt = data.GetEntity();

// This check should probably be for all multiplayer games, investigate later

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );		

			CBaseCombatWeapon *pWpn = ToBaseCombatWeapon( pEnt );
			if ( pWpn && pWpn->IsCarriedByLocalPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( pWpn->GetOwner() );

				pViewModel = player ? player->GetViewModel( 0 ) : nullptr;
				if ( pViewModel )
				{
					// Get the viewmodel and use it instead
					pRenderable = pViewModel;
					break;
				}
			}
		}

		// Get the attachment origin
		if ( !pRenderable->GetAttachment( iAttachment, vecStart, vecAngles ) )
		{
			DevMsg( "GetTracerOrigin: Couldn't find attachment %d on model %s\n", iAttachment, 
				modelinfo->GetModelName( pRenderable->GetModel() ) );
		}
	}

	return vecStart;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerCallback( const CEffectData &data )
{
	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	if ( !r_drawtracers_firstperson.GetBool() )
	{
		CBaseViewModel *pViewModel = ToBaseViewModel(data.GetEntity());
		if ( pViewModel )
			return;
	}

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	float flVelocity = data.m_flScale;
	bool bWhiz = (data.m_fFlags & TRACER_FLAG_WHIZ);
	int iEntIndex = data.entindex();

	if ( IsPlayerIndex( iEntIndex ) && CBasePlayer::IsLocalPlayer( CBaseEntity::Instance( iEntIndex ) ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( CBaseEntity::Instance( iEntIndex ) );
		Vector	foo = data.m_vStart;
		QAngle	vangles;
		Vector	vforward, vright, vup;

		engine->GetViewAngles( vangles );
		AngleVectors( vangles, &vforward, &vright, &vup );

		VectorMA( data.m_vStart, 4, vright, foo );
		foo[2] -= 0.5f;

		FX_PlayerTracer( foo, (Vector&)data.m_vOrigin );
		return;
	}
	
	// Use default velocity if none specified
	if ( !flVelocity )
	{
		flVelocity = TRACER_SPEED;
	}

	// Do tracer effect
	FX_Tracer( (Vector&)vecStart, (Vector&)data.m_vOrigin, flVelocity, bWhiz );
}

DECLARE_CLIENT_EFFECT_BEGIN( Tracer, TracerCallback )
	PRECACHE( MATERIAL, "effects/spark" )
	PRECACHE( GAMESOUND, "Bullets.DefaultNearmiss" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static int s_nWeaponTracerIndex;

void ParticleTracerCallback( const CEffectData &data )
{
	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	CBaseEntity *pEntity = data.GetEntity();
	CBasePlayer *pPlayer = nullptr;
	CBaseViewModel *pViewModel = ToBaseViewModel(pEntity);
	if ( pEntity )
	{
		pPlayer = dynamic_cast<CBasePlayer*>( pEntity );
		if ( pPlayer && !pViewModel )
			pViewModel = pPlayer->GetViewModel( 0 );

		if ( !r_drawtracers_firstperson.GetBool() && pPlayer )
		{	
			if ( pViewModel )
				return;
		}
	}

	int nParticleIndex = (data.m_nHitBox > 1) ? data.m_nHitBox : s_nWeaponTracerIndex;
	Vector vecEnd;
	// Grab the data
	
	CBaseCombatWeapon *pWpn = (pEntity) ? pEntity->MyCombatWeaponPointer() : nullptr;
	if ( (!pWpn && !pViewModel) || !(data.m_fFlags & TRACER_FLAG_USEATTACHMENT) )
	{
		Vector vecStart = GetTracerOrigin( data );
		vecEnd = data.m_vOrigin;

		// if the tracer visually hits anything that it should not, we move the tracer to almost match the bullet trace itself
		if ( r_drawtracers_movetonotintersect.GetBool() )
		{
			trace_t tr;
			UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, pEntity, COLLISION_GROUP_PROJECTILE, &tr );
			if ( tr.fraction != 1.0f && pPlayer )
			{
				vecStart = pPlayer->EyePosition();

				ACTIVE_SPLITSCREEN_PLAYER_GUARD( pPlayer );
				QAngle	vangles;
				Vector	vforward, vright, vup;

				engine->GetViewAngles( vangles );
				AngleVectors( vangles, &vforward, &vright, &vup );

				VectorMA( vecStart, cl_righthand.GetBool() ? 2.5 : -2.5, vright, vecStart );
				VectorMA( vecStart, 10, vforward, vecStart );
				vecStart[2] -= 2.5f;
			}
		}

		// Create the particle effect
		QAngle vecAngles;
		Vector vecToEnd = vecEnd - vecStart;
		VectorNormalize(vecToEnd);
		VectorAngles( vecToEnd, vecAngles );
		DispatchParticleEffect( nParticleIndex, vecStart, vecEnd, vecAngles );
	}
	else
	{
		Vector vecStart = GetTracerOrigin( data );  
		vecEnd = data.m_vOrigin;
		QAngle dummy;
		
		if ( pViewModel )
		{
			CBasePlayer *pPlayer = ToBasePlayer( ((CBaseViewModel *)pViewModel)->GetOwner() );
			FormatViewModelAttachment( pPlayer, vecStart, true );
		}

		// if the tracer visually hits anything that it should not, just don't do the tracer
		trace_t tr;
		UTIL_TraceLine( vecStart, vecEnd, MASK_VISIBLE, pEntity, COLLISION_GROUP_DEBRIS, &tr );
		if ( tr.fraction == 1.0f )
			DispatchParticleEffect( nParticleIndex, vecStart, vecEnd, dummy );
		
	}

	if ( data.m_fFlags & TRACER_FLAG_WHIZ )
	{
		Vector vecStart = data.m_vStart;
		FX_TracerSound( vecStart, vecEnd, TRACER_TYPE_DEFAULT );	
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( ParticleTracer, ParticleTracerCallback );
	PRECACHE_INDEX( PARTICLE_SYSTEM, "weapon_tracers", s_nWeaponTracerIndex )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerSoundCallback( const CEffectData &data )
{
	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	
	// Do tracer effect
	FX_TracerSound( vecStart, (Vector&)data.m_vOrigin, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( TracerSound, TracerSoundCallback );

class CParticleMgr;
class CBaseEntity;

using namespace ::ks::reflect;

CParticleTrail::CParticleTrail( void )
{
}

CParticleTrail::~CParticleTrail( void )
{
	if ( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the attachment point to spawn at
//-----------------------------------------------------------------------------
void CParticleTrail::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pAttachedTo->GetBaseEntity() );

	if ( pEnt && (m_nAttachment > 0) )
	{
		pEnt->GetAttachment( m_nAttachment, *pAbsOrigin, *pAbsAngles );
		return;
	}

	BaseClass::GetAimEntOrigin( pAttachedTo, pAbsOrigin, pAbsAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Turn on the emission of particles 
//-----------------------------------------------------------------------------
void CParticleTrail::SetEmit( bool bEmit )
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: Set the spawn rate of the effect
//-----------------------------------------------------------------------------
void CParticleTrail::SetSpawnRate( float rate )
{
	m_SpawnRate = rate;
	m_ParticleSpawn.Init( rate );
}

//-----------------------------------------------------------------------------
// Purpose: First sent down from the server
//-----------------------------------------------------------------------------
void CParticleTrail::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if( pParticleMgr->AddEffect( &m_ParticleEffect, this ) == false )
		return;

	m_pParticleMgr = pParticleMgr;
}

class CEffectData;
class CParticleSimulateIterator;
class CBaseEntity;
class SimpleParticle;

using namespace ::ks::reflect;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectSplash )
PRECACHE( MATERIAL, "effects/splash1" )
PRECACHE( MATERIAL, "effects/splash2" )
PRECACHE( MATERIAL, "effects/splash4" )
PRECACHE( MATERIAL, "effects/slime1" )
PRECACHE_REGISTER_END()

static constexpr auto SPLASH_MIN_SPEED = 50.0f;
static constexpr auto SPLASH_MAX_SPEED = 100.0f;

ConVar	cl_show_splashes( side_registry(), "cl_show_splashes", "1" );

static Vector s_vecSlimeColor( 46.0f/255.0f, 90.0f/255.0f, 36.0f/255.0f );

// Each channel does not contribute to the luminosity equally, as represented here
static constexpr auto RED_CHANNEL_CONTRIBUTION = 0.30f;
static constexpr auto GREEN_CHANNEL_CONTRIBUTION = 0.59f;
static constexpr auto BLUE_CHANNEL_CONTRIBUTION = 0.11f;

//-----------------------------------------------------------------------------
// Purpose: Returns a normalized tint and luminosity for a specified color
// Input  : &color - normalized input color to extract information from
//			*tint - normalized tint of that color
//			*luminosity - normalized luminosity of that color
//-----------------------------------------------------------------------------
void UTIL_GetNormalizedColorTintAndLuminosity( const Vector &color, Vector *tint, float *luminosity )
{
	// Give luminosity if requested
	if ( luminosity != nullptr )
	{
		// Each channel contributes differently than the others
		*luminosity =	( color.x * RED_CHANNEL_CONTRIBUTION ) +
						( color.y * GREEN_CHANNEL_CONTRIBUTION ) +
						( color.z * BLUE_CHANNEL_CONTRIBUTION );
	}

	// Give tint if requested
	if ( tint != nullptr )
	{
		if ( color == vec3_origin )
		{
			*tint = vec3_origin;
		}
		else
		{
			float maxComponent = std::max( color.x, std::max( color.y, color.z ) );
			*tint = color / maxComponent;
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_WaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime, float flAlpha )
{

	CVProfScope _( "FX_WaterRipple", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	trace_t	tr;

	Vector	color = pColor ? *pColor : Vector( 0.8f, 0.8f, 0.75f );

	Vector startPos = origin + Vector(0,0,8);
	Vector endPos = origin + Vector(0,0,-64);

	UTIL_TraceLine( startPos, endPos, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.fraction < 1.0f )
	{
		QAngle vecAngles;
		// we flip the z and the x to match the orientation of how the impact particles are authored
		// all impact particles are authored with the effect going "up" (0, 0, 1)
		VectorAngles( Vector( tr.plane.normal.z, tr.plane.normal.y, tr.plane.normal.x ), vecAngles );
		DispatchParticleEffect( "water_splash_02_surface2", tr.endpos, vecAngles, nullptr );
	}
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_WaterRipple )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_02_surface2" )
	//PRECACHE( MATERIAL, "effects/splashwake1" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//-----------------------------------------------------------------------------
void FX_GunshotSplashVisuals( const Vector &origin, const Vector &normal, float scale )
{
	CVProfScope _( "FX_GunshotSplash", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
  	if ( cl_show_splashes.GetBool() == false )
		return;

	QAngle vecAngles;
	// we flip the z and the x to match the orientation of how the impact particles are authored
	// all impact particles are authored with the effect going "up" (0, 0, 1)
	VectorAngles( Vector( normal.z, normal.y, normal.x ), vecAngles );
	if ( scale < 4.0f )
	{
		DispatchParticleEffect( "water_splash_01", origin, vecAngles );
	}
	else if ( scale < 8.0f )
	{
		DispatchParticleEffect( "water_splash_02", origin, vecAngles );
	}
	else
	{
		DispatchParticleEffect( "water_splash_03", origin, vecAngles );
	}
}

void FX_GunshotSplashSound( const Vector &origin, const Vector &normal, float scale )
{
	//Play a sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;

	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_GunshotSplash )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_01" )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_02" )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_03" )
	//PRECACHE( MATERIAL, "effects/splash2" )
	PRECACHE( GAMESOUND, "Physics.WaterSplash" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//-----------------------------------------------------------------------------
void FX_GunshotSplash( const Vector &origin, const Vector &normal, float scale )
{
	CVProfScope _( "FX_GunshotSplash", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	if ( cl_show_splashes.GetBool() == false )
		return;

	FX_GunshotSplashVisuals( origin, normal, scale );
	FX_GunshotSplashSound( origin, normal, scale );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//			*pColor - 
//-----------------------------------------------------------------------------
void FX_GunshotSlimeSplash( const Vector &origin, const Vector &normal, float scale )
{
	if ( cl_show_splashes.GetBool() == false )
		return;

	CVProfScope _( "FX_GunshotSlimeSplash", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
	QAngle vecAngles;
	// we flip the z and the x to match the orientation of how the impact particles are authored
	// all impact particles are authored with the effect going "up" (0, 0, 1)
	VectorAngles( Vector( normal.z, normal.y, normal.x ), vecAngles );
	if ( scale < 2.0f )
	{
		DispatchParticleEffect( "slime_splash_01", origin, vecAngles );
	}
	else if ( scale < 4.0f )
	{
		DispatchParticleEffect( "slime_splash_02", origin, vecAngles );
	}
	else
	{
		DispatchParticleEffect( "slime_splash_03", origin, vecAngles );
	}

	//Play a sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;

	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_GunshotSlimeSplash )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_01" )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_02" )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_03" )
	PRECACHE( GAMESOUND, "Physics.WaterSplash" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SplashCallback( const CEffectData &data )
{
	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( watersplash, SplashCallback )
	PRECACHE( SHARED, "FX_GunshotSlimeSplash" )
	PRECACHE( SHARED, "FX_GunshotSplash" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SplashQuietCallback( const CEffectData &data )
{
	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplashVisuals( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( watersplashquiet, SplashQuietCallback )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void GunshotSplashCallback( const CEffectData &data )
{
	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( gunshotsplash, GunshotSplashCallback )
	PRECACHE( SHARED, "FX_GunshotSlimeSplash" )
	PRECACHE( SHARED, "FX_GunshotSplash" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RippleCallback( const CEffectData &data )
{
	float	flScale = data.m_flScale / 8.0f;

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( data.m_vOrigin + ( Vector(0,0,1) * 4.0f ), &color, &luminosity );

	FX_WaterRipple( data.m_vOrigin, flScale, &color, 1.5f, luminosity );
}

DECLARE_CLIENT_EFFECT_BEGIN( waterripple, RippleCallback )
	PRECACHE( SHARED, "FX_WaterRipple" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
	
	pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -4.0f );

	//Cap the minimum roll
	if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
	{
		pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
	}

	return pParticle->m_flRoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
void CSplashParticle::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	//Decellerate
	static float dtime;
	static float decay;

	if ( dtime != timeDelta )
	{
		dtime = timeDelta;
		float expected = 3.0f;
		decay = std::exp( std::log( 0.0001f ) * dtime / expected );
	}

	pParticle->m_vecVelocity *= decay;
	pParticle->m_vecVelocity[2] -= ( 800.0f * timeDelta );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateAlpha( const SimpleParticle *pParticle )
{
	if ( m_bUseClipHeight )
	{
		float flAlpha = pParticle->m_uchStartAlpha / 255.0f;

		return  flAlpha * RemapValClamped(pParticle->m_Pos.z,
								m_flClipHeight,
								m_flClipHeight - ( UpdateScale( pParticle ) * 0.5f ),
								1.0f,
								0.0f );
	}

	return (pParticle->m_uchStartAlpha/255.0f) + ( (float)(pParticle->m_uchEndAlpha/255.0f) - (float)(pParticle->m_uchStartAlpha/255.0f) ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &clipPlane - 
//-----------------------------------------------------------------------------
void CSplashParticle::SetClipHeight( float flClipHeight )
{
	m_bUseClipHeight = true;
	m_flClipHeight = flClipHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pIterator - 
//-----------------------------------------------------------------------------
void CSplashParticle::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		// Clip by height if requested
		if ( m_bUseClipHeight )
		{
			// See if we're below, and therefore need to clip
			if ( pParticle->m_Pos.z + UpdateScale( pParticle ) < m_flClipHeight )
			{
				pIterator->RemoveParticle( pParticle );
				pParticle = (SimpleParticle*)pIterator->GetNext();
				continue;
			}
		}

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}
