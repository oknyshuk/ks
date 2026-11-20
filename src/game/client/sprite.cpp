// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "const.h"
#include "icliententity.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "model_types.h"
#include "utils/common/scriplib.h"
#include "client_class.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "baseentity.h"
#include "predictable_entity.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "reflect.h"
#include "tier2/beamsegdraw.h"
#include "reflect_datamap.h"
#include "filesystem.h"
#include "engine/IEngineSound.h"
#include "iclientrenderable.h"
#include "tier2/resourceprecacher.h"
#include "vphysics/constraints.h"
#include "te_effect_dispatch.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "cdll_client_int.h"
#include "vphysics_interface.h"
#include "datamap.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :iinput;
import :particlemgr;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import :util;
import ks.game.mapentities_shared;
import :precache;
import :achievements_and_stats_interface;
import :cstrike15.cs_player;
import ks.game.gamemovement;
import ks.game.shareddefs;
import :noopanimcontroller;
import ks.tier1.vector_ops;
import ks.core.platform;
import ks.core.numeric;
import ks.math.vector4d;
import ks.math.vmatrix;

class CEngineSprite;
class CBaseEntity;

using namespace ::ks::reflect;

ConVar	r_drawsprites( side_registry(), "r_drawsprites", "1", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Generic sprite model renderer
// Input  : *baseentity - 
//			*psprite - 
//			fscale - 
//			frame - 
//			rendermode - 
//			r - 
//			g - 
//			b - 
//			a - 
//			forward - 
//			right - 
//			up - 
//-----------------------------------------------------------------------------
static unsigned int s_nHDRColorScaleCache = 0;
void DrawSpriteModel( IClientEntity *baseentity, CEngineSprite *psprite, const Vector &origin, float fscale, float frame, 
	int rendermode, int r, int g, int b, int a, const Vector& forward, const Vector& right, const Vector& up, float flHDRColorScale )
{
	float		scale;
	IMaterial	*material;
	
	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	if ( fscale > 0 )
		scale = fscale;
	else
		scale = 1.0f;
	
	if ( rendermode == kRenderNormal )
	{
		render->SetBlend( 1.0f );
	}
	
	material = psprite->GetMaterial( (RenderMode_t)rendermode, frame );
	if ( !material )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	
	if ( ShouldDrawInWireFrameMode() || r_drawsprites.GetInt() == 2 )
	{
		IMaterial *pMaterial = materials->FindMaterial( "debug/debugspritewireframe", TEXTURE_GROUP_OTHER );
		pRenderContext->Bind( pMaterial, nullptr );
	}
	else
	{
		pRenderContext->Bind( material, (IClientRenderable*)baseentity );
	}

	unsigned char color[4];
	color[0] = r;
	color[1] = g;
	color[2] = b;
	color[3] = a;

	IMaterialVar *pHDRColorScaleVar = material->FindVarFast( "$HDRCOLORSCALE", &s_nHDRColorScaleCache );
	if( pHDRColorScaleVar )
	{
		pHDRColorScaleVar->SetVecValue( flHDRColorScale, flHDRColorScale, flHDRColorScale );
	}

	Vector point;
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	Vector vec_a;
	Vector vec_b;
	Vector vec_c;
	Vector vec_d;

	// isolate common terms
	VectorMA( origin, psprite->GetDown() * scale, up, vec_a );
	VectorScale( right, psprite->GetLeft() * scale, vec_b );
	VectorMA( origin, psprite->GetUp() * scale, up, vec_c );
	VectorScale( right, psprite->GetRight() * scale, vec_d );

	float flMinU, flMinV, flMaxU, flMaxV;
	psprite->GetTexCoordRange( &flMinU, &flMinV, &flMaxU, &flMaxV );

	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, flMinU, flMaxV );
	VectorAdd( vec_a, vec_b, point );
	meshBuilder.Position3fv( point.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, flMinU, flMinV );
	VectorAdd( vec_c, vec_b, point );
	meshBuilder.Position3fv( point.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, flMaxU, flMinV );
	VectorAdd( vec_c, vec_d, point );
	meshBuilder.Position3fv( point.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, flMaxU, flMaxV );
	VectorAdd( vec_a, vec_d, point );
	meshBuilder.Position3fv( point.Base() );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Determine glow brightness/scale based on distance to render origin and trace results
// Input  : entorigin - 
//			rendermode - 
//			renderfx - 
//			alpha - 
//			pscale - Pointer to the value for scale, will be changed based on distance and rendermode.
//-----------------------------------------------------------------------------
float StandardGlowBlend( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle, int rendermode, int renderfx, int alpha, float *pscale )
{
	float dist;
	float brightness;

	brightness = PixelVisibility_FractionVisible( params, queryHandle );
	if ( brightness <= 0.0f )
	{
		return 0.0f;
	}
	dist = GlowSightDistance( params.position, false );
	if ( dist <= 0.0f )
	{
		return 0.0f;
	}

	if ( renderfx == kRenderFxNoDissipation )
	{
		return (float)alpha * (1.0f/255.0f) * brightness;
	}

	// UNDONE: Tweak these magic numbers (1200 - distance at full brightness)
	float fadeOut = 1.0f;

	if (rendermode == kRenderWorldGlow)
	{
		fadeOut = ( 2400.0f*2400.0f ) / ( dist*dist );
		fadeOut = clamp( fadeOut, 0.0f, 1.0f );
	}
	else
	{
		fadeOut = ( 1200.0f*1200.0f ) / ( dist*dist );
		fadeOut = clamp( fadeOut, 0.0f, 1.0f );

		// Make the glow fixed size in screen space, taking into consideration the scale setting.
		if ( *pscale == 0.0f )
		{
			*pscale = 1.0f;
		}

		*pscale *= dist * (1.0f/200.0f);
	}

	return fadeOut * brightness;
}

static float SpriteAspect( CEngineSprite *pSprite )
{
	if ( pSprite )
	{
		float x = std::fabsf(pSprite->GetRight() - pSprite->GetLeft());
		float y = fabsf(pSprite->GetDown() - pSprite->GetUp());
		if ( y != 0 && x != 0 )
		{
			return x / y;
		}
	}

	return 1.0f;
}

float CSpriteRenderer::GlowBlend( CEngineSprite *psprite, const Vector& entorigin, int rendermode, int renderfx, int alpha, float *pscale )
{
	pixelvis_queryparams_t params;
	float aspect = SpriteAspect(psprite);
	params.Init( entorigin, PIXELVIS_DEFAULT_PROXY_SIZE, aspect );
	return StandardGlowBlend( params, &m_queryHandle, rendermode, renderfx, alpha, pscale );
}

// since sprites can network down a glow proxy size, handle that here
float CSprite::GlowBlend( CEngineSprite *psprite, const Vector& entorigin, int rendermode, int renderfx, int alpha, float *pscale )
{
	pixelvis_queryparams_t params;
	float aspect = SpriteAspect(psprite);
	params.Init( entorigin, m_flGlowProxySize, aspect );
	return StandardGlowBlend( params, &m_queryHandle, rendermode, renderfx, alpha, pscale );
}

//-----------------------------------------------------------------------------
// Purpose: Determine sprite orientation axes
// Input  : type - 
//			forward - 
//			right - 
//			up - 
//-----------------------------------------------------------------------------
void CSpriteRenderer::GetSpriteAxes( SPRITETYPE type, 
	const Vector& origin,
	const QAngle& angles,
	Vector& forward, 
	Vector& right, 
	Vector& up )
{
	int				i;
	float			dot, angle, sr, cr;
	Vector			tvec;

	// Automatically roll parallel sprites if requested
	if ( angles[2] != 0 && type == SPR_VP_PARALLEL )
	{
		type = SPR_VP_PARALLEL_ORIENTED;
	}

	switch( type )
	{
	case SPR_FACING_UPRIGHT:
		{
			// generate the sprite's axes, with vup straight up in worldspace, and
			// r_spritedesc.vright perpendicular to modelorg.
			// This will not work if the view direction is very close to straight up or
			// down, because the cross product will be between two nearly parallel
			// vectors and starts to approach an undefined state, so we don't draw if
			// the two vectors are less than 1 degree apart
			tvec[0] = -origin[0];
			tvec[1] = -origin[1];
			tvec[2] = -origin[2];
			VectorNormalize (tvec);
			dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
			//  r_spritedesc.vup is 0, 0, 1
			if ((dot > 0.999848f) || (dot < -0.999848f))	// cos(1 degree) = 0.999848
				return;
			up[0] = 0;
			up[1] = 0;
			up[2] = 1;
			right[0] = tvec[1];
			// CrossProduct(r_spritedesc.vup, -modelorg,
			right[1] = -tvec[0];
			//              r_spritedesc.vright)
			right[2] = 0;
			VectorNormalize (right);
			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
			// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
			//  r_spritedesc.vpn)
		}
		break;

	case SPR_VP_PARALLEL:
		{
			// generate the sprite's axes, completely parallel to the viewplane. There
			// are no problem situations, because the sprite is always in the same
			// position relative to the viewer
			for (i=0 ; i<3 ; i++)
			{
				up[i]		= CurrentViewUp()[i];
				right[i]	= CurrentViewRight()[i];
				forward[i]	= CurrentViewForward()[i];
			}
		}
		break;

	case SPR_VP_PARALLEL_UPRIGHT:
		{
			// generate the sprite's axes, with g_vecVUp straight up in worldspace, and
			// r_spritedesc.vright parallel to the viewplane.
			// This will not work if the view direction is very close to straight up or
			// down, because the cross product will be between two nearly parallel
			// vectors and starts to approach an undefined state, so we don't draw if
			// the two vectors are less than 1 degree apart
			dot = CurrentViewForward()[2];	// same as DotProduct (vpn, r_spritedesc.g_vecVUp) because
			//  r_spritedesc.vup is 0, 0, 1
			if ((dot > 0.999848f) || (dot < -0.999848f))	// cos(1 degree) = 0.999848
				return;
			up[0] = 0;
			up[1] = 0;
			up[2] = 1;
			right[0] = CurrentViewForward()[1];
			// CrossProduct (r_spritedesc.vup, vpn,
			right[1] = -CurrentViewForward()[0];	//  r_spritedesc.vright)
			right[2] = 0;
			VectorNormalize (right);
			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
			// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
			//  r_spritedesc.vpn)
		}
		break;

	case SPR_ORIENTED:
		{
			// generate the sprite's axes, according to the sprite's world orientation
			AngleVectors( angles, &forward, &right, &up );
		}
		break;
		
	case SPR_VP_PARALLEL_ORIENTED:
		{
			// generate the sprite's axes, parallel to the viewplane, but rotated in
			// that plane around the center according to the sprite entity's roll
			// angle. So vpn stays the same, but vright and vup rotate
			angle = angles[ROLL] * (std::numbers::pi*2.0f/360.0f);
			SinCos( angle, &sr, &cr );
			
			for (i=0 ; i<3 ; i++)
			{
				forward[i] = CurrentViewForward()[i];
				right[i] = CurrentViewRight()[i] * cr + CurrentViewUp()[i] * sr;
				up[i] = CurrentViewRight()[i] * -sr + CurrentViewUp()[i] * cr;
			}
		}
		break;

	default:
		Warning( "GetSpriteAxes: Bad sprite type %d\n", type );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CSpriteRenderer::DrawSprite( 
	IClientEntity *entity,
	const model_t *model, 
	const Vector& origin, 
	const QAngle& angles,
	float frame,
	IClientEntity *attachedto,
	int attachmentindex,
	int rendermode,
	int renderfx,
	int alpha,
	int r, 
	int g, 
	int b,
	float scale,
	float flHDRColorScale
	)
{
	CVProfScope _( "CSpriteRenderer::DrawSprite", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	if ( !r_drawsprites.GetBool() || !model || modelinfo->GetModelType( model ) != mod_sprite )
	{
		return 0;
	}

	// Get extra data
	CEngineSprite *psprite = (CEngineSprite *)modelinfo->GetModelExtraData( model );
	if ( !psprite )
	{
		return 0;
	}

	Vector effect_origin;
	VectorCopy( origin, effect_origin );

	// Use attachment point
	if ( attachedto )
	{
		CBaseEntity *ent = EntityCast<CBaseEntity>( attachedto->GetBaseEntity() );
		if ( ent )
		{
			// don't draw viewmodel effects in reflections
			if ( CurrentViewID() == VIEW_REFLECTION )
			{
				if ( g_pClientLeafSystem->IsRenderingWithViewModels( ent->RenderHandle() ) )
					return 0;
			}
			QAngle temp;
			ent->GetAttachment( attachmentindex, effect_origin, temp );
		}
	}

	if ( rendermode != kRenderNormal )
	{
		float blend = render->GetBlend();

		// kRenderGlow and kRenderWorldGlow have a special blending function
		if (( rendermode == kRenderGlow ) || ( rendermode == kRenderWorldGlow ))
		{
			blend *= GlowBlend( psprite, effect_origin, rendermode, renderfx, alpha, &scale );

			// Fade out the sprite depending on distance from the view origin.
			r *= blend;
			g *= blend;
			b *= blend;
		}

		render->SetBlend( blend );
		if ( blend <= 0.0f )
		{
			return 0;
		}
	}
	
	// Get orthonormal basis
	Vector forward, right, up;
	GetSpriteAxes( (SPRITETYPE)psprite->GetOrientation(), origin, angles, forward, right, up );

	// Draw
	DrawSpriteModel( 
		entity,
		psprite, 
		effect_origin,
		scale,
		frame, 
		rendermode, 
		r, 
		g, 
		b,
		alpha, 
		forward, right, up, flHDRColorScale );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	CVProfScope _( "CSprite::GetToolRecordingState", 0, VPROF_BUDGETGROUP_TOOLS, false, BUDGETFLAG_OTHER );

	BaseClass::GetToolRecordingState( msg );

	// Use attachment point
	if ( m_hAttachedToEntity )
	{
		CBaseEntity *ent = m_hAttachedToEntity->GetBaseEntity();
		if ( ent )
		{
			BaseEntityRecordingState_t *pState = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );

			// override position if we're driven by an attachment
			QAngle temp;
			pState->m_vecRenderOrigin = GetAbsOrigin();
			ent->GetAttachment( m_nAttachment, pState->m_vecRenderOrigin, temp );

			// override viewmodel if we're driven by an attachment
			bool bViewModel = ToBaseViewModel( ent ) != nullptr;
			msg->SetInt( "viewmodel", bViewModel );
		}
	}

	float renderscale = GetRenderScale();
	if ( m_bWorldSpaceScale )
	{
		CEngineSprite *psprite = ( CEngineSprite * )modelinfo->GetModelExtraData( GetModel() );
		float flMinSize = std::min( psprite->GetWidth(), psprite->GetHeight() );
		renderscale /= flMinSize;
	}

	color24 c = GetRenderColor();

	// sprite params
	static SpriteRecordingState_t state;
	state.m_flRenderScale = renderscale;
	state.m_flFrame = m_flFrame;
	state.m_flProxyRadius = m_flGlowProxySize;
	state.m_nRenderMode = GetRenderMode();
	state.m_nRenderFX = GetRenderFX() ? true : false;
	state.m_Color.SetColor( c.r, c.g, c.b, GetRenderBrightness() );

	msg->SetPtr( "sprite", &state );
}

std::vector< CSprite * > g_ClientsideSprites;

// ===================== For clientside spawning of sprites =====================
void CSprite::RecreateAllClientside()
{
	DestroyAllClientside();	
	ParseAllClientsideEntities( engine->GetMapEntitiesString() );
}

void CSprite::DestroyAllClientside()
{
	// This only gets called during LevelInitPostEntity and LevelShutdown so we're going to use 
	// Release() instead of Remove() or UTIL_Remove
	while ( !g_ClientsideSprites.empty() )
	{
		CSprite *p = g_ClientsideSprites[0];

		// This will call into CSprite::~CSprite in sprite.cpp, which will FindAndRemove this sprite from the array
		p->Release();
	}
}

bool CSprite::InitializeClientside()
{	
	if ( InitializeAsClientEntity( STRING( GetModelName() ), false ) == false )
	{
		return false;
	}

	m_bClientOnly = true;
	g_ClientsideSprites.push_back( this );
	
	Spawn();

	const model_t *mod = GetModel();
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetCollisionBounds( mins, maxs );		
	}

	SetBlocksLOS( false ); // this should be a small object
	SetNextClientThink( CLIENT_THINK_NEVER );

	return true;
}

const char *CSprite::ParseClientsideEntity( const char *pEntData )
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];

	MDLCACHE_CRITICAL_SECTION();

	if ( !entData.ExtractValue( "classname", className ) )
	{
		Error( "classname missing from entity!\n" );
	}

	if ( !V_strcmp( className, "env_sprite_clientside" ))
	{
		// always force clientside entities placed in maps
		CSprite *pEntity = new CSprite(); 

		if ( pEntity )
		{	
			// Set up keyvalues.
			pEntity->ParseMapData(&entData);

			if ( !pEntity->InitializeClientside() )
				pEntity->Release();

			return entData.CurrentBufferPosition();
		}
	}

	// Just skip past all the keys.
	char keyName[MAPKEY_MAXLENGTH];
	char value[MAPKEY_MAXLENGTH];
	if ( entData.GetFirstKey(keyName, value) )
	{
		do 
		{
		} 
		while ( entData.GetNextKey(keyName, value) );
	}

	//
	// Return the current parser position in the data block
	//
	return entData.CurrentBufferPosition();
}

bool CSprite::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "scale" ) )
	{
		m_flSpriteScale = atof(szValue);		
	}
	else if ( FStrEq( szKeyName, "framerate" ) )
	{
		m_flSpriteFramerate = atof(szValue);		
	}
	else if ( FStrEq( szKeyName, "GlowProxySize" ) )
	{
		m_flGlowProxySize = atof(szValue);		
	}
	else if ( FStrEq( szKeyName, "frame" ) )
	{
		m_flFrame = atof(szValue);		
	}
	else if ( FStrEq( szKeyName, "HDRColorScale" ) )
	{
		m_flHDRColorScale = atof(szValue);		
	}
	else if ( FStrEq( szKeyName, "rendermode" ) )
	{
		SetRenderMode( (RenderMode_t) atoi( szValue ) );
	}
	else if ( FStrEq( szKeyName, "model" ) )
	{
		SetModelName( AllocPooledString( szValue ) );
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void CSprite::ParseAllClientsideEntities(const char *pMapData)
{
	int nEntities = 0;

	char szTokenBuffer[MAPKEY_MAXLENGTH];

	//
	//  Loop through all entities in the map data, creating each.
	//
	for ( ; true; pMapData = MapEntity_SkipToNextEntity( pMapData, szTokenBuffer ) )
	{
		//
		// Parse the opening brace.
		//
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );

		//
		// Check to see if we've finished or not.
		//
		if ( !pMapData )
			break;

		if ( token[0] != '{' )
		{
			Error( "CSprite::ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		//
		// Parse the entity and add it to the spawn list.
		//

		pMapData = ParseClientsideEntity( pMapData );

		nEntities++;
	}
}

class CEngineSprite;
class CBaseEntity;
class CBaseViewModel;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

IMPLEMENT_REFLECT_PREDMAP( CSprite );

IMPLEMENT_NETWORKCLASS_ALIASED( Sprite, DT_Sprite );

 void RecvProxy_SpriteScale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CSprite*)pStruct)->SetSpriteScale( pData->m_Value.m_Float );
}

IMPLEMENT_REFLECT_TABLE( CSprite, DT_Sprite );

LINK_ENTITY_TO_CLASS_ALIASED( env_sprite, Sprite );

extern std::vector< CSprite * > g_ClientsideSprites;

CSprite::CSprite()
{
	m_bClientOnly = false;
	m_flGlowProxySize = 2.0f;
	m_flHDRColorScale = 1.0f;
}

CSprite::~CSprite()
{
	if ( m_bClientOnly )
	{
		ks::swap_erase_first( g_ClientsideSprites, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	m_flFrame = 0;

	Precache();
	SetModel( STRING( GetModelName() ) );
	CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );

	m_flMaxFrame = (float)modelinfo->GetModelFrameCount( GetModel() ) - 1;
	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );

	{
		TurnOn();
	}
	
	// Worldcraft only sets y rotation, copy to Z
	if ( GetLocalAngles().y != 0 && GetLocalAngles().z == 0 )
	{
		QAngle angles = GetLocalAngles();

		angles.z = angles.y;
		angles.y = 0;

		SetLocalAngles( angles );
	}

	// Clamp our scale if necessary
	float scale = m_flSpriteScale;
	
	if ( scale < 0 || scale > MAX_SPRITE_SCALE )
	{
		scale = clamp( m_flSpriteScale.Get(), 0, MAX_SPRITE_SCALE );
	}

	//Set our state
	SetBrightness( GetRenderAlpha() );
	SetScale( scale );

	m_flStartScale = m_flDestScale = m_flSpriteScale;
	m_nStartBrightness = m_nDestBrightness = m_nBrightness;

}

//-----------------------------------------------------------------------------
// Purpose: Initialize absmin & absmax to the appropriate box
//-----------------------------------------------------------------------------
void CSprite::EnableWorldSpaceScale( bool bEnable )
{
	m_bWorldSpaceScale = bEnable;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize absmin & absmax to the appropriate box
//-----------------------------------------------------------------------------
void CSprite::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	float flScale = m_flSpriteScale * 0.5f;

	if ( m_bWorldSpaceScale == false )
	{
		// Find the height and width of the source of the sprite
		float width = modelinfo->GetModelSpriteWidth( GetModel() );
		float height = modelinfo->GetModelSpriteHeight( GetModel() );
		flScale *= std::max( width, height );
	}

	pVecWorldMins->Init( -flScale, -flScale, -flScale );
	pVecWorldMaxs->Init( flScale, flScale, flScale );
	*pVecWorldMins += GetAbsOrigin();
	*pVecWorldMaxs += GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szModelName - 
//-----------------------------------------------------------------------------
void CSprite::SetModel( const char *szModelName )
{
	int index = modelinfo->GetModelIndex( szModelName );
	const model_t *model = modelinfo->GetModel( index );
	if ( model && modelinfo->GetModelType( model ) != mod_sprite )
	{
		Msg( "Setting CSprite to non-sprite model %s\n", szModelName?szModelName:"NULL" );
	}

	BaseClass::SetModel( szModelName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::Precache( void )
{
	if ( GetModelName() != NULL_STRING )
	{
		PrecacheModel( STRING( GetModelName() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//-----------------------------------------------------------------------------
void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	SetModelName( MAKE_STRING(pSpriteName) );
	SetLocalOrigin( origin );
	Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSprite
//-----------------------------------------------------------------------------
CSprite *CSprite::SpriteCreatePredictable( const char *module, int line, const char *pSpriteName, const Vector &origin, bool animate )
{
	CSprite *pSprite = ( CSprite * )CBaseEntity::CreatePredictedEntityByName( "env_sprite", module, line );
	if ( pSprite )
	{
		pSprite->SpriteInit( pSpriteName, origin );
		pSprite->SetSolid( SOLID_NONE );
		pSprite->SetSize( vec3_origin, vec3_origin );
		pSprite->SetMoveType( MOVETYPE_NONE );
		if ( animate )
			pSprite->TurnOn();
	}

	return pSprite;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::AnimateThink( void )
{
	Animate( m_flSpriteFramerate * (gpGlobals->curtime - m_flLastTime) );

	SetNextThink( gpGlobals->curtime );
	m_flLastTime			= gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->curtime > m_flDieTime )
	{
		Remove( );
	}
	else
	{
		AnimateThink();
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scaleSpeed - 
//			fadeSpeed - 
//-----------------------------------------------------------------------------
void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	m_flSpeed = scaleSpeed;
	m_iHealth = fadeSpeed;
	SetThink( &CSprite::ExpandThink );

	SetNextThink( gpGlobals->curtime );
	m_flLastTime	= gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->curtime - m_flLastTime;
	SetSpriteScale( m_flSpriteScale + m_flSpeed * frametime );

	int sub = (int)(m_iHealth * frametime);
	if ( sub > GetRenderAlpha() )
	{
		SetRenderAlpha( 0 );
		Remove( );
	}
	else
	{
		SetRenderAlpha( GetRenderAlpha() - sub );
		SetNextThink( gpGlobals->curtime );
		m_flLastTime		= gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frames - 
//-----------------------------------------------------------------------------
void CSprite::Animate( float frames )
{ 
	m_flFrame += frames;
	if ( m_flFrame > m_flMaxFrame )
	{
		{
			if ( m_flMaxFrame > 0 )
				m_flFrame = std::fmod( m_flFrame, m_flMaxFrame );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSprite::SetBrightness( int brightness, float time )
{
	m_nBrightness			= brightness;	//Take our current position as our starting position
	m_flBrightnessTime		= time;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::SetSpriteScale( float scale )
{
	if ( scale != m_flSpriteScale )
	{
		m_flSpriteScale		= scale;	//Take our current position as our new starting position
		// The surrounding box is based on sprite scale... it changes, box is dirty
		CollisionProp()->MarkSurroundingBoundsDirty();
	}
}

void CSprite::SetScale( float scale, float time )
{
	m_flScaleTime		= time;
	SetSpriteScale( scale );
	// The surrounding box is based on sprite scale... it changes, box is dirty
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::TurnOff( void )
{
	AddEffects( EF_NODRAW );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::TurnOn( void )
{
	RemoveEffects( EF_NODRAW );
	if ( (m_flSpriteFramerate && m_flMaxFrame > 1.0)
		)
	{
		SetThink( &CSprite::AnimateThink );
		SetNextThink( gpGlobals->curtime );
		m_flLastTime = gpGlobals->curtime;
	}
	m_flFrame = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CSprite::GetRenderScale( void )
{
	//See if we're done scaling
	if ( ( m_flScaleTime == 0 ) || ( (m_flScaleTimeStart+m_flScaleTime) < gpGlobals->curtime ) )
		return m_flSpriteScale;

	//Get our percentage
	float timeDelta = ( gpGlobals->curtime - m_flScaleTimeStart ) / m_flScaleTime;

	//Return the result
	return ( m_flStartScale + ( ( m_flDestScale - m_flStartScale  ) * timeDelta ) );
}

float CSprite::GetMaxRenderScale( void )
{
	//See if we're done scaling
	if ( m_flScaleTime == 0 )
		return m_flSpriteScale;

	// return the max scale over the interval
	return std::max(m_flStartScale, m_flDestScale);
}

//-----------------------------------------------------------------------------
// Purpose: Get the rendered extents of the sprite
//-----------------------------------------------------------------------------
void CSprite::GetRenderBounds( Vector &vecMins, Vector &vecMaxs )
{
	// NOTE: Don't call GetRenderScale here because the bounds will get cached
	// if we don't blow away that cache every time the interpolant (curtime) changes
	// then it will be wrong as long as this is animating.
	// instead while animating just use the 
	float flScale = GetMaxRenderScale() * 0.5f;

	// If our scale is normalized we need to convert that to actual world units
	if ( m_bWorldSpaceScale == false )
	{
		CEngineSprite *psprite = (CEngineSprite *) modelinfo->GetModelExtraData( GetModel() );
		if ( psprite )
		{
			float flSize = std::max( psprite->GetWidth(), psprite->GetHeight() );
			flScale *= flSize;
		}
	}

	vecMins.Init( -flScale, -flScale, -flScale );
	vecMaxs.Init(  flScale,  flScale,  flScale );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CSprite::GetRenderBrightness( void )
{
	//See if we're done scaling
	if ( ( m_flBrightnessTime == 0 ) || ( (m_flBrightnessTimeStart+m_flBrightnessTime) < gpGlobals->curtime ) )
	{
		return m_nBrightness;
	}

	//Get our percentage
	float timeDelta = ( gpGlobals->curtime - m_flBrightnessTimeStart ) / m_flBrightnessTime;

	float brightness = ( (float) m_nStartBrightness + ( (float) ( m_nDestBrightness - m_nStartBrightness  ) * timeDelta ) );

	//Return the result
	return (int) brightness;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flStartScale = m_flDestScale = m_flSpriteScale;
		m_nStartBrightness = m_nDestBrightness = m_nBrightness;
	}

	UpdateVisibility();

	if ( m_flSpriteScale != m_flDestScale || m_nBrightness != m_nDestBrightness )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

void CSprite::ClientThink( void )
{
	bool bDisableThink = true;
	// Module render colors over time
	if ( m_flSpriteScale != m_flDestScale )
	{
		m_flStartScale		= m_flDestScale;
		m_flDestScale		= m_flSpriteScale;
		m_flScaleTimeStart	= gpGlobals->curtime;
		bDisableThink = false;
	}

	if ( m_nBrightness != m_nDestBrightness )
	{
		m_nStartBrightness		= m_nDestBrightness;
		m_nDestBrightness		= m_nBrightness;
		m_flBrightnessTimeStart = gpGlobals->curtime;
		bDisableThink = false;
	}

	// changed bounds
	InvalidatePhysicsRecursive(BOUNDS_CHANGED);
	if ( bDisableThink )
	{
		SetNextClientThink(CLIENT_THINK_NEVER);
	}
}

extern bool g_bRenderingScreenshot;
extern ConVar r_drawviewmodel;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int CSprite::DrawModel( int flags, const RenderableInstance_t &instance )
{
	CVProfScope _( "CSprite::DrawModel", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	//See if we should draw
	if ( !IsVisible() || ( m_bReadyToDraw == false ) )
		return 0;

	// Tracker 16432:  If rendering a savegame screenshot then don't draw sprites 
	//   who have viewmodels as their moveparent
	if ( g_bRenderingScreenshot || !r_drawviewmodel.GetBool() )
	{
		CBaseViewModel *vm = ToBaseViewModel( GetMoveParent() );
		if ( vm )
		{
			return 0;
		}
	}

	//Must be a sprite
	if ( modelinfo->GetModelType( GetModel() ) != mod_sprite )
	{
		const char *modelName = modelinfo->GetModelName( GetModel() );
		char msg[256];
		V_snprintf( msg, 256, "Sprite %d has non-mod_sprite model %s (type %d)\n",
			entindex(), modelName, modelinfo->GetModelType( GetModel() ) );
		contract_assert( 0 );
		return 0;
	}

	float renderscale = GetRenderScale();
	if ( m_bWorldSpaceScale )
	{
		CEngineSprite *psprite = ( CEngineSprite * )modelinfo->GetModelExtraData( GetModel() );
		float flMinSize = std::min( psprite->GetWidth(), psprite->GetHeight() );
		renderscale /= flMinSize;
	}

	//Draw it
	int drawn = DrawSprite( 
		this,
		GetModel(), 
		GetAbsOrigin(), 
		GetAbsAngles(), 
		m_flFrame,				// sprite frame to render
		m_hAttachedToEntity,	// attach to
		m_nAttachment,			// attachment point
		GetRenderMode(),		// rendermode
		GetRenderFX(),
		(float)( GetRenderBrightness() * instance.m_nAlpha ) * ( 1.0f / 255.0f ) + 0.5f,	// alpha
		GetRenderColorR(),
		GetRenderColorG(),
		GetRenderColorB(),
		renderscale,			// sprite scale
		GetHDRColorScale()		// HDR Color Scale
		);

	return drawn;
}

const Vector& CSprite::GetRenderOrigin()
{
	static Vector vOrigin;
	vOrigin = GetAbsOrigin();

	if ( m_hAttachedToEntity )
	{
		CBaseEntity *ent = m_hAttachedToEntity->GetBaseEntity();
		if ( ent )
		{
			QAngle dummyAngles;
			ent->GetAttachment( m_nAttachment, vOrigin, dummyAngles );
		}
	}

	return vOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: oriented sprites
//			CSprites swap the roll and yaw angle inputs, and rotate the yaw 180 degrees
//-----------------------------------------------------------------------------

#undef CSpriteOriented
IMPLEMENT_REFLECT_CLIENTCLASS( CSpriteOriented, DT_SpriteOriented, CSpriteOriented )
LINK_ENTITY_TO_CLASS_ALIASED( env_sprite_oriented, SpriteOriented );

RenderableTranslucencyType_t CSpriteOriented::ComputeTranslucencyType()
{
	return RENDERABLE_IS_TRANSLUCENT;
}

class CBaseEntity;

using namespace ::ks::reflect;

extern bool g_bMeasureParticlePerformance;
extern bool g_bDisplayParticlePerformance;

void ResetParticlePerformanceCounters( void );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CParticlePerformanceMonitor : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticlePerformanceMonitor" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CParticlePerformanceMonitor, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CParticlePerformanceMonitor();
	~CParticlePerformanceMonitor();
	virtual void	OnDataChanged( DataUpdateType_t updateType );

private:
	[[= ks::reflect::Net{} ]] bool m_bDisplayPerf;
	[[= ks::reflect::Net{} ]] bool m_bMeasurePerf;
private:
	CParticlePerformanceMonitor( const CParticlePerformanceMonitor & );
};

IMPLEMENT_REFLECT_CLIENTCLASS( CParticlePerformanceMonitor, DT_ParticlePerformanceMonitor, CParticlePerformanceMonitor )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticlePerformanceMonitor::CParticlePerformanceMonitor( void )
{
	m_bDisplayPerf = false;
	m_bMeasurePerf = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticlePerformanceMonitor::~CParticlePerformanceMonitor( void )
{
	g_bMeasureParticlePerformance = false;
	g_bDisplayParticlePerformance = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticlePerformanceMonitor::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	if ( m_bMeasurePerf && ( ! g_bMeasureParticlePerformance ) )
		ResetParticlePerformanceCounters();
	g_bMeasureParticlePerformance = m_bMeasurePerf;
	g_bDisplayParticlePerformance = m_bDisplayPerf;
}

class CEngineSprite;
class CBaseEntity;

using namespace ::ks::reflect;

extern CEngineSprite *Draw_SetSpriteTexture( const model_t *pSpriteModel, int frame, int rendermode );

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Save/Restore
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( TrailPoint_t )

IMPLEMENT_REFLECT_DATAMAP( CSpriteTrail )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( SpriteTrail, DT_SpriteTrail );

IMPLEMENT_REFLECT_TABLE( CSpriteTrail, DT_SpriteTrail );

LINK_ENTITY_TO_CLASS_ALIASED( env_spritetrail, SpriteTrail );

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_PREDMAP( CSpriteTrail );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSpriteTrail::CSpriteTrail( void )
{
	m_nFirstStep = 0;
	m_nStepCount = 0;

	m_flStartWidthVariance = 0;
	m_vecSkyboxOrigin.Init( 0, 0, 0 );
	m_flSkyboxScale = 1.0f;
	m_flEndWidth = -1.0f;
}

void CSpriteTrail::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::Precache( void ) 
{
	BaseClass::Precache();

	if ( m_iszSpriteName != NULL_STRING )
	{
		PrecacheModel( STRING(m_iszSpriteName) );
	}
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::SetLifeTime( float time ) 
{ 
	m_flLifeTime = time; 
}

void CSpriteTrail::SetStartWidth( float flStartWidth )
{ 
	m_flStartWidth = flStartWidth; 
	m_flStartWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetStartWidthVariance( float flStartWidthVariance )
{ 
	m_flStartWidthVariance = flStartWidthVariance; 
	m_flStartWidthVariance /= m_flSkyboxScale;
}

void CSpriteTrail::SetEndWidth( float flEndWidth )
{ 
	m_flEndWidth = flEndWidth; 
	m_flEndWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetTextureResolution( float flTexelsPerInch )
{ 
	m_flTextureRes = flTexelsPerInch; 
	m_flTextureRes *= m_flSkyboxScale;
}

void CSpriteTrail::SetMinFadeLength( float flMinFadeLength )
{
	m_flMinFadeLength = flMinFadeLength;
	m_flMinFadeLength /= m_flSkyboxScale;
}

void CSpriteTrail::SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale )
{
	m_flTextureRes /= m_flSkyboxScale;
	m_flMinFadeLength *= m_flSkyboxScale;
	m_flStartWidth *= m_flSkyboxScale;
	m_flEndWidth *= m_flSkyboxScale;
	m_flStartWidthVariance *= m_flSkyboxScale;

	m_vecSkyboxOrigin = vecSkyboxOrigin;
	m_flSkyboxScale = flSkyboxScale;

	m_flTextureRes *= m_flSkyboxScale;
	m_flMinFadeLength /= m_flSkyboxScale;
	m_flStartWidth /= m_flSkyboxScale;
	m_flEndWidth /= m_flSkyboxScale;
	m_flStartWidthVariance /= m_flSkyboxScale;

	if ( IsInSkybox() )
	{
		AddEFlags( EFL_IN_SKYBOX ); 
	}
	else
	{
		RemoveEFlags( EFL_IN_SKYBOX ); 
	}
}

//-----------------------------------------------------------------------------
// Is the trail in the skybox?
//-----------------------------------------------------------------------------
bool CSpriteTrail::IsInSkybox() const
{
	return (m_flSkyboxScale != 1.0f) || (m_vecSkyboxOrigin != vec3_origin);
}

//-----------------------------------------------------------------------------
// On data update
//-----------------------------------------------------------------------------
void CSpriteTrail::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );
	m_vecPrevSkyboxOrigin = m_vecSkyboxOrigin;
	m_flPrevSkyboxScale = m_flSkyboxScale;
}

void CSpriteTrail::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		if ((m_flPrevSkyboxScale != m_flSkyboxScale) || (m_vecPrevSkyboxOrigin != m_vecSkyboxOrigin))
		{
			ConvertSkybox();
		}
	}
}

//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::ClientThink()
{
	// Update the trail + bounding box
	UpdateTrail();
	UpdateBoundingBox();
}

//-----------------------------------------------------------------------------
// Render bounds
//-----------------------------------------------------------------------------
void CSpriteTrail::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_vecRenderMins;
	maxs = m_vecRenderMaxs;
}

//-----------------------------------------------------------------------------
// Converts the trail when it changes skyboxes
//-----------------------------------------------------------------------------
void CSpriteTrail::ConvertSkybox()
{
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = GetTrailPoint(i);

		VectorSubtract( pPoint->m_vecScreenPos, m_vecPrevSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_vecScreenPos *= m_flPrevSkyboxScale / m_flSkyboxScale;
		VectorSubtract( pPoint->m_vecScreenPos, m_vecSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_flWidthVariance *= m_flPrevSkyboxScale / m_flSkyboxScale;
	}
}

//-----------------------------------------------------------------------------
// Gets at the nth item in the list
//-----------------------------------------------------------------------------
TrailPoint_t *CSpriteTrail::GetTrailPoint( int n )
{
	contract_assert( n < MAX_SPRITE_TRAIL_POINTS );
	static_assert( (MAX_SPRITE_TRAIL_POINTS & (MAX_SPRITE_TRAIL_POINTS-1)) == 0 );
	int nIndex = (n + m_nFirstStep) & MAX_SPRITE_TRAIL_MASK; 
	return &m_vecSteps[nIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::ComputeScreenPosition( Vector *pScreenPos )
{
	*pScreenPos = GetRenderOrigin();
}

//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateBoundingBox( void )
{
	Vector vecRenderOrigin = GetRenderOrigin();
	m_vecRenderMins = vecRenderOrigin;
	m_vecRenderMaxs = vecRenderOrigin;

	float flMaxWidth = m_flStartWidth;
	if (( m_flEndWidth >= 0.0f ) && ( m_flEndWidth > m_flStartWidth ))
	{
		flMaxWidth = m_flEndWidth;
	}

	Vector mins, maxs;
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		TrailPoint_t *pPoint = GetTrailPoint(i);

		float flActualWidth = (flMaxWidth + pPoint->m_flWidthVariance) * 0.5f;
		Vector size( flActualWidth, flActualWidth, flActualWidth );
		VectorSubtract( pPoint->m_vecScreenPos, size, mins );
		VectorAdd( pPoint->m_vecScreenPos, size, maxs );
		
		VectorMin( m_vecRenderMins, mins, m_vecRenderMins ); 
		VectorMax( m_vecRenderMaxs, maxs, m_vecRenderMaxs ); 
	}
	
	m_vecRenderMins -= vecRenderOrigin; 
	m_vecRenderMaxs -= vecRenderOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateTrail( void )
{
	// Can't update too quickly
	if ( m_flUpdateTime > gpGlobals->curtime )
		return;

	Vector	screenPos;
	ComputeScreenPosition( &screenPos );
	TrailPoint_t *pLast = m_nStepCount ? GetTrailPoint( m_nStepCount-1 ) : nullptr;
	if ( ( pLast == nullptr ) || ( pLast->m_vecScreenPos.DistToSqr( screenPos ) > 4.0f ) )
	{
		// If we're over our limit, steal the last point and put it up front
		if ( m_nStepCount >= MAX_SPRITE_TRAIL_POINTS )
		{
			--m_nStepCount;
			++m_nFirstStep;
		}

		// Save off its screen position, not its world position
		TrailPoint_t *pNewPoint = GetTrailPoint( m_nStepCount );
		pNewPoint->m_vecScreenPos = screenPos;
		pNewPoint->m_flDieTime	= gpGlobals->curtime + m_flLifeTime;
		pNewPoint->m_flWidthVariance = random_valve->RandomFloat( -m_flStartWidthVariance, m_flStartWidthVariance );
		if (pLast)
		{
			pNewPoint->m_flTexCoord	= pLast->m_flTexCoord + pLast->m_vecScreenPos.DistTo( screenPos ) * m_flTextureRes;
		}
		else
		{
			pNewPoint->m_flTexCoord = 0.0f;
		}

		++m_nStepCount;
	}

	// Don't update again for a bit
	m_flUpdateTime = gpGlobals->curtime + ( m_flLifeTime / (float) MAX_SPRITE_TRAIL_POINTS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSpriteTrail::DrawModel( int flags, const RenderableInstance_t &instance )
{
	CVProfScope _( "CSpriteTrail::DrawModel", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
	// Must have at least one point
	if ( m_nStepCount < 1 )
		return 1;

	//See if we should draw
	if ( !IsVisible() || ( m_bReadyToDraw == false ) )
		return 0;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( GetModel(), m_flFrame, GetRenderMode() );
	if ( pSprite == nullptr )
		return 0;

	// Specify all the segments.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, m_nStepCount + 1, pSprite->GetMaterial( GetRenderMode() ) );
	
	// Setup the first point, always emanating from the attachment point
	TrailPoint_t *pLast = GetTrailPoint( m_nStepCount-1 );
	TrailPoint_t currentPoint;
	currentPoint.m_flDieTime = gpGlobals->curtime + m_flLifeTime;
	ComputeScreenPosition( &currentPoint.m_vecScreenPos );
	currentPoint.m_flTexCoord = pLast->m_flTexCoord + currentPoint.m_vecScreenPos.DistTo(pLast->m_vecScreenPos) * m_flTextureRes;
	currentPoint.m_flWidthVariance = 0.0f;

	TrailPoint_t *pPrevPoint = nullptr;
	float flTailAlphaDist = m_flMinFadeLength;
	for ( int i = 0; i <= m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = (i != m_nStepCount) ? GetTrailPoint(i) : &currentPoint;

		float flLifePerc = (pPoint->m_flDieTime - gpGlobals->curtime) / m_flLifeTime;
		flLifePerc = clamp( flLifePerc, 0.0f, 1.0f );

		color24 c = GetRenderColor();
		BeamSeg_t curSeg;
		curSeg.m_color.r = c.r; curSeg.m_color.g = c.g; curSeg.m_color.b = c.b;

		float flAlphaFade = flLifePerc;
		if ( flTailAlphaDist > 0.0f )
		{
			if ( pPrevPoint )
			{
				float flDist = pPoint->m_vecScreenPos.DistTo( pPrevPoint->m_vecScreenPos );
				flTailAlphaDist -= flDist;
			}

			if ( flTailAlphaDist > 0.0f )
			{
				float flTailFade = Lerp( (m_flMinFadeLength - flTailAlphaDist) / m_flMinFadeLength, 0.0f, 1.0f );
				if ( flTailFade < flAlphaFade )
				{
					flAlphaFade = flTailFade;
				}
			}
		}
		curSeg.m_color.a  = GetRenderBrightness() * flAlphaFade;

		curSeg.m_vPos = pPoint->m_vecScreenPos;

		if ( m_flEndWidth >= 0.0f )
		{
			curSeg.m_flWidth = Lerp( flLifePerc, m_flEndWidth.Get(), m_flStartWidth.Get() );
		}
		else
		{
			curSeg.m_flWidth = m_flStartWidth.Get();
		}
		curSeg.m_flWidth += pPoint->m_flWidthVariance;
		if ( curSeg.m_flWidth < 0.0f )
		{
			curSeg.m_flWidth = 0.0f;
		}

		curSeg.m_flTexCoord = pPoint->m_flTexCoord;

		segDraw.NextSeg( &curSeg );

		// See if we're done with this bad boy
		if ( pPoint->m_flDieTime <= gpGlobals->curtime )
		{
			// Push this back onto the top for use
			++m_nFirstStep;
			--i;
			--m_nStepCount;
		}

		pPrevPoint = pPoint;
	}

	segDraw.End();

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector const&
//-----------------------------------------------------------------------------
const Vector &CSpriteTrail::GetRenderOrigin( void )
{
	static Vector vOrigin;
	vOrigin = GetAbsOrigin();

	if ( m_hAttachedToEntity )
	{
		CBaseEntity *ent = m_hAttachedToEntity->GetBaseEntity();
		if ( ent )
		{
			QAngle dummyAngles;
			ent->GetAttachment( m_nAttachment, vOrigin, dummyAngles );
		}
	}

	return vOrigin;
}

const QAngle &CSpriteTrail::GetRenderAngles( void )
{
	return vec3_angle;
}

class CEngineSprite;

using namespace ::ks::reflect;

// Sprites are clipped to this rectangle (x,y,width,height) if ScissorTest is enabled
static int scissor_x = 0;
static int scissor_y = 0;
static int scissor_width = 0;
static int scissor_height = 0;
static bool giScissorTest = false;

//-----------------------------------------------------------------------------
// Purpose: 
// Set the scissor
//  the coordinate system for gl is upsidedown (inverted-y) as compared to software, so the
//  specified clipping rect must be flipped
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void EnableScissorTest( int x, int y, int width, int height )
{
	x = clamp( x, 0, ScreenWidth() );
	y = clamp( y, 0, ScreenHeight() );
	width = clamp( width, 0, ScreenWidth() - x );
	height = clamp( height, 0, ScreenHeight() - y );

	scissor_x = x;
	scissor_width = width;
	scissor_y = y;
	scissor_height = height;

	giScissorTest = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DisableScissorTest( void )
{
	scissor_x = 0;
	scissor_width = 0;
	scissor_y = 0;
	scissor_height = 0;
	
	giScissorTest = false;
}

//-----------------------------------------------------------------------------
// Purpose: Verify that this is a valid, properly ordered rectangle.
// Input  : *prc - 
// Output : int
//-----------------------------------------------------------------------------
static int ValidateWRect(const wrect_t *prc)
{

	if (!prc)
		return false;

	if ((prc->left >= prc->right) || (prc->top >= prc->bottom))
	{
		//!!!UNDONE Dev only warning msg
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: classic interview question
// Input  : *prc1 - 
//			*prc2 - 
//			*prc - 
// Output : int
//-----------------------------------------------------------------------------
static int IntersectWRect(const wrect_t *prc1, const wrect_t *prc2, wrect_t *prc)
{	
	wrect_t rc;

	if (!prc)
		prc = &rc;

	prc->left = std::max(prc1->left, prc2->left);
	prc->right = std::min(prc1->right, prc2->right);

	if (prc->left < prc->right)
	{
		prc->top = std::max(prc1->top, prc2->top);
		prc->bottom = std::min(prc1->bottom, prc2->bottom);

		if (prc->top < prc->bottom)
			return 1;

	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			width - 
//			height - 
//			u0 - 
//			v0 - 
//			u1 - 
//			v1 - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool Scissor( int& x, int& y, int& width, int& height, float& u0, float& v0, float& u1, float& v1 )
{
	// clip sub rect to sprite
	if ((width == 0) || (height == 0))
		return false;

	if ((x + width <= scissor_x) || (x >= scissor_x + scissor_width) ||
		(y + height <= scissor_y) || (y >= scissor_y + scissor_height))
		return false;

	float dudx = (u1-u0) / width;
	float dvdy = (v1-v0) / height;
	if (x < scissor_x)
	{
		u0 += (scissor_x - x) * dudx;
		width -= scissor_x - x;
		x = scissor_x;
	}

	if (x + width > scissor_x + scissor_width)
	{
		u1 -= (x + width - (scissor_x + scissor_width)) * dudx;
		width = scissor_x + scissor_width - x;
	}

	if (y < scissor_y)
	{
		v0 += (scissor_y - y) * dvdy;
		height -= scissor_y - y;
		y = scissor_y;
	}

	if (y + height > scissor_y + scissor_height)
	{
		v1 -= (y + height - (scissor_y + scissor_height)) * dvdy;
		height = scissor_y + scissor_height - y;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSprite - 
//			frame - 
//			*pfLeft - 
//			*pfRight - 
//			*pfTop - 
//			*pfBottom - 
//			*pw - 
//			*ph - 
//			*prcSubRect - 
// Output : static void
//-----------------------------------------------------------------------------
static void AdjustSubRect(CEngineSprite *pSprite, int frame, float *pfLeft, float *pfRight, float *pfTop, 
						  float *pfBottom, int *pw, int *ph, const wrect_t *prcSubRect)
{
	wrect_t rc;
	float f;

	if (!ValidateWRect(prcSubRect))
		return;

	// clip sub rect to sprite

	rc.top = rc.left = 0;
	rc.right = *pw;
	rc.bottom = *ph;

	if (!IntersectWRect(prcSubRect, &rc, &rc))
		return;

	*pw = rc.right - rc.left;
	*ph = rc.bottom - rc.top;

	f = 1.0 / (float)pSprite->GetWidth();;
	*pfLeft = ((float)rc.left + 0.5) * f;
	*pfRight = ((float)rc.right - 0.5) * f;

	f = 1.0 / (float)pSprite->GetHeight();
	*pfTop = ((float)rc.top + 0.5) * f;
	*pfBottom = ((float)rc.bottom - 0.5) * f;

	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static unsigned int spriteOriginCache = 0;
static unsigned int spriteOrientationCache = 0;
bool CEngineSprite::Init( const char *pName )
{
	m_width = m_height = m_numFrames = 1;

	{
		char pTemp[MAX_PATH];
		char pMaterialName[MAX_PATH];
		char pMaterialPath[MAX_PATH];
		V_StripExtension( pName, pTemp, sizeof(pTemp) );
		V_strlower( pTemp );
		V_FixSlashes( pTemp, '/' );

		// Check to see if this is a UNC-specified material name
		bool bIsUNC = pTemp[0] == '/' && pTemp[1] == '/' && pTemp[2] != '/';
		if ( !bIsUNC )
		{
			V_strncpy( pMaterialName, "materials/", sizeof(pMaterialName) );
			V_strncat( pMaterialName, pTemp, sizeof(pMaterialName), COPY_ALL_CHARACTERS );
		}
		else
		{
			V_strncpy( pMaterialName, pTemp, sizeof(pMaterialName) );
		}
		V_strncpy( pMaterialPath, pMaterialName, sizeof(pMaterialPath) );
		V_SetExtension( pMaterialPath, ".vmt", sizeof(pMaterialPath) );

		for ( int i = 0; i < kRenderModeCount; ++i )
		{	
			m_material[i] = nullptr;
		}

		KeyValues *kv = new KeyValues( "vmt" );
		if ( !kv->LoadFromFile( g_pFullFileSystem, pMaterialPath, "GAME" ) )
		{
			Warning( "Unable to load sprite material %s!\n", pMaterialPath );
			return false;
		}

		for ( int i = 0; i < kRenderModeCount; ++i )
		{	
			if ( i == kRenderNone || i == kRenderEnvironmental )
			{
				continue;
			}

			// strip possible materials/
			V_snprintf( pMaterialPath, sizeof(pMaterialPath), "%s_rendermode_%d", pMaterialName + ( bIsUNC ? 0 : 10 ), i );
			KeyValues *pMaterialKV = kv->MakeCopy();
			pMaterialKV->SetInt( "$spriteRenderMode", i );
			m_material[i] = g_pMaterialSystem->FindProceduralMaterial( pMaterialPath, TEXTURE_GROUP_CLIENT_EFFECTS, pMaterialKV );
			m_material[i]->IncrementReferenceCount();	
		}

		kv->deleteThis();

		m_width = m_material[0]->GetMappingWidth();
		m_height = m_material[0]->GetMappingHeight();
		m_numFrames = m_material[0]->GetNumAnimationFrames();
	}

	for ( int i = 0; i < kRenderModeCount; ++i )
	{
		if ( i == kRenderNone || i == kRenderEnvironmental )
			continue;

		if ( !m_material[i] )
			return false;
	}

	IMaterialVar *orientationVar = m_material[0]->FindVarFast( "$spriteorientation", &spriteOrientationCache );
	m_orientation = orientationVar ? orientationVar->GetIntValue() : CSpriteRenderer::SPR_VP_PARALLEL_UPRIGHT;

	IMaterialVar *originVar = m_material[0]->FindVarFast( "$spriteorigin", &spriteOriginCache );
	Vector origin, originVarValue;
	if( !originVar || ( originVar->GetType() != MATERIAL_VAR_TYPE_VECTOR ) )
	{
		origin[0] = -m_width * 0.5f;
		origin[1] = m_height * 0.5f;
	}
	else
	{
		originVar->GetVecValue( &originVarValue[0], 3 );
		origin[0] = -m_width * originVarValue[0];
		origin[1] = m_height * originVarValue[1];
	}

	up = origin[1];
	down = origin[1] - m_height;
	left = origin[0];
	right = m_width + origin[0];

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineSprite::Shutdown( void )
{
	UnloadMaterial();
}

//-----------------------------------------------------------------------------
// Returns the texture coordinate range	used to draw the sprite
//-----------------------------------------------------------------------------
void CEngineSprite::GetTexCoordRange( float *pMinU, float *pMinV, float *pMaxU, float *pMaxV )
{
	*pMaxU = 1.0f; 
	*pMaxV = 1.0f;
	float flOOWidth = ( m_width != 0 ) ? 1.0f / m_width : 1.0f;
	float flOOHeight = ( m_height!= 0 ) ? 1.0f / m_height : 1.0f;

	*pMinU = 0.5f * flOOWidth; 
	*pMinV = 0.5f * flOOHeight;
	*pMaxU = (*pMaxU) - (*pMinU);
	*pMaxV = (*pMaxV) - (*pMinV);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineSprite::SetColor( float r, float g, float b )
{
	contract_assert( (r >= 0.0) && (g >= 0.0) && (b >= 0.0) );
	contract_assert( (r <= 1.0) && (g <= 1.0) && (b <= 1.0) );
	m_hudSpriteColor[0] = r;
	m_hudSpriteColor[1] = g;
	m_hudSpriteColor[2] = b;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineSprite::GetHUDSpriteColor( float* color )
{
	VectorCopy( m_hudSpriteColor, color );
}

//-----------------------------------------------------------------------------
// Returns the material 
//-----------------------------------------------------------------------------
static unsigned int frameCache = 0;
IMaterial *CEngineSprite::GetMaterial( RenderMode_t nRenderMode, int nFrame ) 
{
	if ( nRenderMode == kRenderNone || nRenderMode == kRenderEnvironmental )
		return nullptr;

	IMaterial *pMaterial = m_material[nRenderMode];
	contract_assert( pMaterial );
	if ( pMaterial == nullptr )
		return nullptr;

	IMaterialVar* pFrameVar = pMaterial->FindVarFast( "$frame", &frameCache );
	if ( pFrameVar )
	{
		pFrameVar->SetIntValue( nFrame );
	}

	return pMaterial;
} 

void CEngineSprite::SetFrame( RenderMode_t nRenderMode, int nFrame )
{
	IMaterial *pMaterial = m_material[nRenderMode];
	if ( !pMaterial )
		return;

	IMaterialVar* pFrameVar = pMaterial->FindVarFast( "$frame", &frameCache );
	if ( pFrameVar )
	{
		pFrameVar->SetIntValue( nFrame );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CEngineSprite::GetOrientation( void )
{
	return m_orientation;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineSprite::UnloadMaterial( void )
{
	for ( int i = 0; i < kRenderModeCount; ++i )
	{
		if( m_material[i] )
		{
			m_material[i]->DecrementReferenceCount();
			m_material[i] = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineSprite::DrawFrame( RenderMode_t nRenderMode, int frame, int x, int y, const wrect_t *prcSubRect )
{
	DrawFrameOfSize( nRenderMode, frame, x, y, GetWidth(), GetHeight(), prcSubRect );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//			x - 
//			y - 
//			*prcSubRect - 
//-----------------------------------------------------------------------------
void CEngineSprite::DrawFrameOfSize( RenderMode_t nRenderMode, int frame, int x, int y, int iWidth, int iHeight, const wrect_t *prcSubRect )
{
	// FIXME: If we ever call this with AVIs, need to have it call GetTexCoordRange and make that work

	float fLeft = 0;
	float fRight = 1;
	float fTop = 0;
	float fBottom = 1;

	if ( prcSubRect )
	{
		AdjustSubRect( this, frame, &fLeft, &fRight, &fTop, &fBottom, &iWidth, &iHeight, prcSubRect );
	}

	if ( giScissorTest && !Scissor( x, y, iWidth, iHeight, fLeft, fTop, fRight, fBottom ) )
		return;

	SetFrame( nRenderMode, frame );

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, GetMaterial( nRenderMode ) );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float color[3];
	GetHUDSpriteColor( color );
	
	meshBuilder.Color3fv( color );
	meshBuilder.TexCoord2f( 0, fLeft, fTop );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv( color );
	meshBuilder.TexCoord2f( 0, fRight, fTop );
	meshBuilder.Position3f( x + iWidth, y, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv( color );
	meshBuilder.TexCoord2f( 0, fRight, fBottom );
	meshBuilder.Position3f( x + iWidth, y + iHeight, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv( color );
	meshBuilder.TexCoord2f( 0, fLeft, fBottom );
	meshBuilder.Position3f( x, y + iHeight, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
struct Particle;
class ParticleDraw;

using namespace ::ks::reflect;

// Purpose: Implements a particle system steam jet.

//NOTENOTE: Mirrored in dlls\steamjet.h
static constexpr auto STEAM_NORMAL = 0;
static constexpr auto STEAM_HEATWAVE = 1;

static constexpr auto STEAMJET_NUMRAMPS = 5;
static constexpr auto SF_EMISSIVE = 0x00000001;

//==================================================
// CSteamJet
//==================================================

class CSteamJet : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_SteamJet" } ]]
	[[= ks::reflect::From<"m_SpreadSpeed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Speed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_StartSize", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_EndSize", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Rate", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_JetLength", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_bEmit", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_bFaceLeft", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nType", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_flRollSpeed", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CSteamJet, CBaseParticleEntity );

	CSteamJet();
	~CSteamJet();

	class SteamJetParticle : public Particle
	{
	public:
		Vector			m_Velocity;
		float			m_flRoll;
		float			m_flRollDelta;
		float			m_Lifetime;
		float			m_DieTime;
		unsigned char	m_uchStartSize;
		unsigned char	m_uchEndSize;
	};

	int IsEmissive( void ) { return ( m_spawnflags & SF_EMISSIVE ); }

//CBaseEntity
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );

//IPrototypeAppEffect
public:
	virtual void		Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);
	virtual bool		GetPropEditInfo(RecvTable **ppTable, void **ppObj);

//IParticleEffect
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

//Stuff from the datatable
public:

	float			m_SpreadSpeed;
	float			m_Speed;
	float			m_StartSize;
	float			m_EndSize;
	float			m_Rate;
	float			m_JetLength;	// Length of the jet. Lifetime is derived from this.

	int				m_bEmit;		// Emit particles?
	int				m_nType;		// Type of particles to emit
	bool			m_bFaceLeft;	// For support of legacy env_steamjet entity, which faced left instead of forward.

	int				m_spawnflags;
	float			m_flRollSpeed;

private:

	void			UpdateLightingRamp();

private:

	// Stored the last time it updates the lighting ramp, so it can cache the values.
	Vector			m_vLastRampUpdatePos;
	QAngle			m_vLastRampUpdateAngles;

	float			m_Lifetime;		// Calculated from m_JetLength / m_Speed;

	// We sample the world to get these colors and ramp the particles.
	Vector			m_Ramps[STEAMJET_NUMRAMPS];

	CParticleMgr		*m_pParticleMgr;
	PMaterialHandle	m_MaterialHandle;
	TimedEvent		m_ParticleSpawn;

private:
					CSteamJet( const CSteamJet & );
};

// ------------------------------------------------------------------------- //
// Tables.
// ------------------------------------------------------------------------- //

// Expose to the particle app.

// Datatable..
IMPLEMENT_REFLECT_CLIENTCLASS( CSteamJet, DT_SteamJet, CSteamJet )

// ------------------------------------------------------------------------- //
// CSteamJet implementation.
// ------------------------------------------------------------------------- //
CSteamJet::CSteamJet()
{
	m_pParticleMgr = nullptr;
	m_MaterialHandle = INVALID_MATERIAL_HANDLE;
	
	m_SpreadSpeed = 15;
	m_Speed = 120;
	m_StartSize = 10;
	m_EndSize = 25;
	m_Rate = 26;
	m_JetLength = 80;
	m_bEmit = true;
	m_bFaceLeft = false;
	m_ParticleEffect.SetAlwaysSimulate( false ); // Don't simulate outside the PVS or frustum.

	m_vLastRampUpdatePos.Init( 1e24, 1e24, 1e24 );
	m_vLastRampUpdateAngles.Init( 1e24, 1e24, 1e24 );
}

CSteamJet::~CSteamJet()
{
	if(m_pParticleMgr)
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
}

//-----------------------------------------------------------------------------
// Purpose: Called after a data update has occured
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CSteamJet::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		Start(ParticleMgr(), nullptr);
	}

	// Recalulate lifetime in case length or speed changed.
	m_Lifetime = m_JetLength / m_Speed;
	m_ParticleEffect.SetParticleCullRadius( std::max(m_StartSize, m_EndSize) );
}

//-----------------------------------------------------------------------------
// Purpose: Starts the effect
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void CSteamJet::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	pParticleMgr->AddEffect( &m_ParticleEffect, this );
	
	switch(m_nType)
	{
	case STEAM_NORMAL:
	default:
		m_MaterialHandle = g_Mat_DustPuff[0];
		break;

	case STEAM_HEATWAVE:
		m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial("sprites/heatwave");
		break;
	}

	m_ParticleSpawn.Init(m_Rate);
	m_Lifetime = m_JetLength / m_Speed;
	m_pParticleMgr = pParticleMgr;

	UpdateLightingRamp();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppTable - 
//			**ppObj - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSteamJet::GetPropEditInfo( RecvTable **ppTable, void **ppObj )
{
	*ppTable = &REFERENCE_RECV_TABLE(DT_SteamJet);
	*ppObj = this;
	return true;
}

// This might be useful someday.
/*
void CalcFastApproximateRenderBoundsAABB( CBaseEntity *pEnt, float flBloatSize, Vector *pMin, Vector *pMax )
{
	CBaseEntity *pParent = pEnt->GetMoveParent();
	if ( pParent )
	{
		// Get the parent's abs space world bounds.
		CalcFastApproximateRenderBoundsAABB( pParent, 0, pMin, pMax );

		// Add the maximum of our local render bounds. This is making the assumption that we can be at any
		// point and at any angle within the parent's world space bounds.
		Vector vAddMins, vAddMaxs;
		pEnt->GetRenderBounds( vAddMins, vAddMaxs );

		flBloatSize += std::max( vAddMins.Length(), vAddMaxs.Length() );
	}
	else
	{
		// Start out with our own render bounds. Since we don't have a parent, this won't incur any nasty 
		pEnt->GetRenderBoundsWorldspace( *pMin, *pMax );
	}

	// Bloat the box.
	if ( flBloatSize )
	{
		*pMin -= Vector( flBloatSize, flBloatSize, flBloatSize );
		*pMax += Vector( flBloatSize, flBloatSize, flBloatSize );
	}
}
*/

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------

void CSteamJet::Update(float fTimeDelta)
{
	if(!m_pParticleMgr)
	{
		contract_assert( false );
		return;
	}

	if( m_bEmit )
	{
		// Add new particles.
		int nToEmit = 0;
		float tempDelta = fTimeDelta;
		while( m_ParticleSpawn.NextEvent(tempDelta) )
			++nToEmit;

		if ( nToEmit > 0 )
		{
			Vector forward, right, up;
			AngleVectors(GetAbsAngles(), &forward, &right, &up);			

			// Legacy env_steamjet entities faced left instead of forward.
			if (m_bFaceLeft)
			{
				Vector temp = forward;
				forward = -right;
				right = temp;
			}

			// EVIL: Ideally, we could tell the renderer our OBB, and let it build a big box that encloses
			// the entity with its parent so it doesn't have to setup its parent's bones here.
			Vector vEndPoint = GetAbsOrigin() + forward * m_Speed;
			Vector vMin, vMax;
			VectorMin( GetAbsOrigin(), vEndPoint, vMin );
			VectorMax( GetAbsOrigin(), vEndPoint, vMax );
			m_ParticleEffect.SetBBox( vMin, vMax );

			if ( m_ParticleEffect.WasDrawnPrevFrame() )
			{
				while ( nToEmit-- )
				{
					// Make a new particle.
					if( SteamJetParticle *pParticle = (SteamJetParticle*) m_ParticleEffect.AddParticle( sizeof(SteamJetParticle), m_MaterialHandle ) )
					{
						pParticle->m_Pos = GetAbsOrigin();
						
						pParticle->m_Velocity = 
							FRand(-m_SpreadSpeed,m_SpreadSpeed) * right +
							FRand(-m_SpreadSpeed,m_SpreadSpeed) * up +
							m_Speed * forward;
						
						pParticle->m_Lifetime	= 0;
						pParticle->m_DieTime	= m_Lifetime;

						pParticle->m_uchStartSize	= m_StartSize;
						pParticle->m_uchEndSize		= m_EndSize;

						pParticle->m_flRoll = random_valve->RandomFloat( 0, 360 );
						pParticle->m_flRollDelta = random_valve->RandomFloat( -m_flRollSpeed, m_flRollSpeed );
					}
				}
			}

			UpdateLightingRamp();
		}	
	}
}

// Render a quad on the screen where you pass in color and size.
// Normal is random and "flutters"
inline void RenderParticle_ColorSizePerturbNormal(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size
	)
{
	// Don't render totally transparent particles.
	if( alpha < 0.001f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x * 254.9f );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y * 254.9f );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z * 254.9f );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha * 254.9f );

	Vector vNorm;
	
	vNorm.Random( -1.0f, 1.0f );
	
	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x-size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3fv( vNorm.Base() );
	pBuilder->TexCoord2f( 0, 0, 1.0f );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x-size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3fv( vNorm.Base() );
	pBuilder->TexCoord2f( 0, 0, 0 );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3fv( vNorm.Base() );
	pBuilder->TexCoord2f( 0, 1.0f, 0 );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3fv( vNorm.Base() );
	pBuilder->TexCoord2f( 0, 1.0f, 1.0f );
 	pBuilder->AdvanceVertex();
}

void CSteamJet::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SteamJetParticle *pParticle = (const SteamJetParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Render.
		Vector tPos;
		TransformParticle(m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos);
		float sortKey = tPos.z;

		float lifetimeT = pParticle->m_Lifetime / (pParticle->m_DieTime + 0.001);
		float fRamp = lifetimeT * (STEAMJET_NUMRAMPS-1);
		int iRamp = (int)fRamp;
		float fraction = fRamp - iRamp;
		
		Vector vRampColor = m_Ramps[iRamp] + (m_Ramps[iRamp+1] - m_Ramps[iRamp]) * fraction;

		vRampColor[0] = std::min( 1.0f, vRampColor[0] );
		vRampColor[1] = std::min( 1.0f, vRampColor[1] );
		vRampColor[2] = std::min( 1.0f, vRampColor[2] );

		float sinLifetime = std::sin(pParticle->m_Lifetime * 3.14159f / pParticle->m_DieTime);

		if ( m_nType == STEAM_HEATWAVE )
		{
			RenderParticle_ColorSizePerturbNormal(
				pIterator->GetParticleDraw(),
				tPos,
				vRampColor,
				sinLifetime * (GetRenderAlpha()/255.0f),
				FLerp(m_StartSize, m_EndSize, pParticle->m_Lifetime));
		}
		else
		{
			RenderParticle_ColorSizeAngle(
				pIterator->GetParticleDraw(),
				tPos,
				vRampColor,
				sinLifetime * (GetRenderAlpha()/255.0f),
				FLerp(pParticle->m_uchStartSize, pParticle->m_uchEndSize, pParticle->m_Lifetime),
				pParticle->m_flRoll );
		}

		pParticle = (const SteamJetParticle*)pIterator->GetNext( sortKey );
	}
}

void CSteamJet::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	//Don't simulate if we're emiting particles...
	//This fixes the cases where looking away from a steam jet and then looking back would cause a break on the stream.
	if ( m_ParticleEffect.WasDrawnPrevFrame() == false && m_bEmit )
		return;

	SteamJetParticle *pParticle = (SteamJetParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Should this particle die?
		pParticle->m_Lifetime += pIterator->GetTimeDelta();
		
		if( pParticle->m_Lifetime > pParticle->m_DieTime )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			pParticle->m_flRoll += pParticle->m_flRollDelta * pIterator->GetTimeDelta();
			pParticle->m_Pos = pParticle->m_Pos + pParticle->m_Velocity * pIterator->GetTimeDelta();
		}

		pParticle = (SteamJetParticle*)pIterator->GetNext();
	}
}

void CSteamJet::UpdateLightingRamp()
{
	if( VectorsAreEqual( m_vLastRampUpdatePos, GetAbsOrigin(), 0.1 ) && 
		QAnglesAreEqual( m_vLastRampUpdateAngles, GetAbsAngles(), 0.1 ) )
	{
		return;
	}

	m_vLastRampUpdatePos = GetAbsOrigin();
	m_vLastRampUpdateAngles = GetAbsAngles();

	// Sample the world lighting where we think the particles will be.
	Vector forward, right, up;
	AngleVectors(GetAbsAngles(), &forward, &right, &up);

	// Legacy env_steamjet entities faced left instead of forward.
	if (m_bFaceLeft)
	{
		Vector temp = forward;
		forward = -right;
		right = temp;
	}

	Vector startPos = GetAbsOrigin();
	Vector endPos = GetAbsOrigin() + forward * (m_Speed * m_Lifetime);

	for(int iRamp=0; iRamp < STEAMJET_NUMRAMPS; iRamp++)
	{
		float t = (float)iRamp / (STEAMJET_NUMRAMPS-1);
		Vector vTestPos = startPos + (endPos - startPos) * t;
		
		Vector *pRamp = &m_Ramps[iRamp];
		*pRamp = WorldGetLightForPoint(vTestPos, false);
		
		if ( IsEmissive() )
		{
			pRamp->x += (GetRenderColorR()/255.0f);
			pRamp->y += (GetRenderColorG()/255.0f);
			pRamp->z += (GetRenderColorB()/255.0f);

			pRamp->x = clamp( pRamp->x, 0.0f, 1.0f );
			pRamp->y = clamp( pRamp->y, 0.0f, 1.0f );
			pRamp->z = clamp( pRamp->z, 0.0f, 1.0f );
		}
		else
		{
			pRamp->x *= (GetRenderColorR()/255.0f);
			pRamp->y *= (GetRenderColorG()/255.0f);
			pRamp->z *= (GetRenderColorB()/255.0f);
		}

		// Renormalize?
		float maxVal = std::max(pRamp->x, std::max(pRamp->y, pRamp->z));
		if(maxVal > 1)
		{
			*pRamp = *pRamp / maxVal;
		}
	}
}

class CEffectData;
class CRagdoll;
class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Implements the Sticky Bolt code. This constraints ragdolls to the world
//			after being hit by a crossbow bolt. If something here is acting funny
//			let me know - Adrian.

extern IPhysicsSurfaceProps *physprops;
IPhysicsObject *GetWorldPhysObject( void );

extern ITempEnts* tempents;

class CRagdollBoltEnumerator : public IPartitionEnumerator
{
public:
	//Forced constructor   
	CRagdollBoltEnumerator( Ray_t& shot, Vector vOrigin )
	{
		m_rayShot = shot;
		m_vWorld = vOrigin;
	}

	//Actual work code
	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
		if ( pEnt == nullptr )
			return ITERATION_CONTINUE;

		CBaseAnimating *pModel = static_cast< CBaseAnimating * >( pEnt );

		if ( pModel == nullptr )
			return ITERATION_CONTINUE;

		trace_t tr;
		enginetrace->ClipRayToEntity( m_rayShot, MASK_SHOT, pModel, &tr );

		IPhysicsObject	*pPhysicsObject = nullptr;
		
		//Find the real object we hit.
		if( tr.physicsbone >= 0 )
		{
			if ( pModel->m_pRagdoll )
			{
				CRagdoll *pCRagdoll = dynamic_cast < CRagdoll * > ( pModel->m_pRagdoll );

				if ( pCRagdoll )
				{
					ragdoll_t *pRagdollT = pCRagdoll->GetRagdoll();

					if ( tr.physicsbone < pRagdollT->listCount )
					{
						pPhysicsObject = pRagdollT->list[tr.physicsbone].pObject;
					}
				}
			}
		}

		if ( pPhysicsObject == nullptr )
			return ITERATION_CONTINUE;

		if ( tr.fraction < 1.0 )
		{
			IPhysicsObject *pReference = GetWorldPhysObject();

			if ( pReference == nullptr || pPhysicsObject == nullptr )
				 return ITERATION_CONTINUE;
			
			float flMass = pPhysicsObject->GetMass();
			pPhysicsObject->SetMass( flMass * 2 );

			constraint_ballsocketparams_t ballsocket;
			ballsocket.Defaults();
		
			pReference->WorldToLocal( &ballsocket.constraintPosition[0], m_vWorld );
			pPhysicsObject->WorldToLocal( &ballsocket.constraintPosition[1], tr.endpos );
	
			physenv->CreateBallsocketConstraint( pReference, pPhysicsObject, nullptr, ballsocket );

			//Play a sound
			CPASAttenuationFilter filter( pEnt );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_VOICE;
			ep.m_pSoundName =  "Weapon_Crossbow.BoltSkewer";
			ep.m_flVolume = 1.0f;
			ep.m_SoundLevel = SNDLVL_NORM;
			ep.m_pOrigin = &pEnt->GetAbsOrigin();

			CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
	
			return ITERATION_STOP;
		}

		return ITERATION_CONTINUE;
	}

private:
	Ray_t	m_rayShot;
	Vector  m_vWorld;
};

inline constexpr const char *CROSSBOW_BOLT_MODEL = "models/crossbow_bolt.mdl";
void CreateCrossbowBolt( const Vector &vecOrigin, const Vector &vecDirection )
{
	model_t *pModel = (model_t *)engine->LoadModel( CROSSBOW_BOLT_MODEL );

	QAngle vAngles;

	VectorAngles( vecDirection, vAngles );
	
	if ( gpGlobals->maxClients > 1 )
	{
		tempents->SpawnTempModel( pModel, vecOrigin - vecDirection * 8, vAngles, Vector(0, 0, 0 ), 30.0f, FTENT_NONE );
	}
	else
	{
		tempents->SpawnTempModel( pModel, vecOrigin - vecDirection * 8, vAngles, Vector(0, 0, 0 ), 1, FTENT_NEVERDIE );
	}
}

void StickRagdollNow( const Vector &vecOrigin, const Vector &vecDirection )
{
	Ray_t	shotRay;
	trace_t tr;
	
	UTIL_TraceLine( vecOrigin, vecOrigin + vecDirection * 16, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	if ( tr.surface.flags & SURF_SKY )
		return;

	Vector vecEnd = vecOrigin - vecDirection * 128;

	shotRay.Init( vecOrigin, vecEnd );

	CRagdollBoltEnumerator	ragdollEnum( shotRay, vecOrigin );
	::partition->EnumerateElementsAlongRay( PARTITION_CLIENT_RESPONSIVE_EDICTS, shotRay, false, &ragdollEnum );
	
	CreateCrossbowBolt( vecOrigin, vecDirection );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void StickyBoltCallback( const CEffectData &data )
{
	 StickRagdollNow( data.m_vOrigin, data.m_vNormal );
}

DECLARE_CLIENT_EFFECT_BEGIN( BoltImpact, StickyBoltCallback )
	PRECACHE( MODEL, CROSSBOW_BOLT_MODEL )
DECLARE_CLIENT_EFFECT_END()

using namespace ::ks::reflect;

void RecvProxy_SunHDRColorScale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CSun *pSun = ( CSun * )pStruct;

	pSun->m_Overlay.m_flHDRColorScale = pData->m_Value.m_Float;
	pSun->m_GlowOverlay.m_flHDRColorScale = pData->m_Value.m_Float;
}

IMPLEMENT_REFLECT_CLIENTCLASS( CSun, DT_Sun, CSun )

CSun::CSun()
{
	m_Overlay.m_bDirectional = true;
	m_Overlay.m_bInSky = true;

	m_GlowOverlay.m_bDirectional = true;
	m_GlowOverlay.m_bInSky = true;
}

CSun::~CSun()
{
}

void CSun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// We have to do special setup on our colors because we're tinting an additive material.
	// If we don't have at least one component at full strength, the luminosity of the material
	// will change and that will cause the material to become more translucent  This would be incorrect
	// for the sun, which should always be completely opaque at its core.  Here, we renormalize the
	// components to make sure only hue is altered.
	color24 c = GetRenderColor();
	float maxComponent = std::max( c.r, std::max( c.g, c.b ) );

	Vector vOverlayColor;
	Vector vMainColor;

	// Re-normalize the color ranges
	if ( maxComponent <= 0.0f )
	{
		// This is an error, set to pure white
		vMainColor.Init( 1.0f, 1.0f, 1.0f );
	}
	else
	{
		vMainColor.x = c.r / maxComponent;
		vMainColor.y = c.g / maxComponent;
		vMainColor.z = c.b / maxComponent;
	}
	
	// If we're non-zero, use the value (otherwise use the value we calculated above)
	if ( m_clrOverlay.r != 0 || m_clrOverlay.g != 0 || m_clrOverlay.b != 0 )
	{
		// Get our overlay color
		vOverlayColor.x = m_clrOverlay.r / 255.0f;
		vOverlayColor.y = m_clrOverlay.g / 255.0f;
		vOverlayColor.z = m_clrOverlay.b / 255.0f;
	}
	else
	{
		vOverlayColor = vMainColor;
	}

	// 
	// Setup the core overlay
	//

	m_Overlay.m_vDirection = m_vDirection;
	m_Overlay.m_nSprites = 1;

	m_Overlay.m_Sprites[0].m_vColor = vMainColor;
	m_Overlay.m_Sprites[0].m_flHorzSize = m_nSize;
	m_Overlay.m_Sprites[0].m_flVertSize = m_nSize;

	const model_t* pModel = (m_nMaterial != 0) ? modelinfo->GetModel( m_nMaterial ) : nullptr;
	const char *pModelName = pModel ? modelinfo->GetModelName( pModel ) : "";
	m_Overlay.m_Sprites[0].m_pMaterial = materials->FindMaterial( pModelName, TEXTURE_GROUP_OTHER );
	m_Overlay.m_flProxyRadius = 0.05f; // about 1/20th of the screen

	//
	// Setup the external glow overlay
	//

	m_GlowOverlay.m_vDirection = m_vDirection;
	m_GlowOverlay.m_nSprites = 1;

	m_GlowOverlay.m_Sprites[0].m_vColor = vOverlayColor;
	m_GlowOverlay.m_Sprites[0].m_flHorzSize = m_nOverlaySize;
	m_GlowOverlay.m_Sprites[0].m_flVertSize = m_nOverlaySize;

	pModel = (m_nOverlayMaterial != 0) ? modelinfo->GetModel( m_nOverlayMaterial ) : nullptr;
	pModelName = pModel ? modelinfo->GetModelName( pModel ) : "";
	m_GlowOverlay.m_Sprites[0].m_pMaterial = materials->FindMaterial( pModelName, TEXTURE_GROUP_OTHER );

	// This texture will fade away as the dot between camera and sun changes
	m_GlowOverlay.SetModulateByDot();
	m_GlowOverlay.m_flProxyRadius = 0.05f; // about 1/20th of the screen

	// Either activate or deactivate.
	if ( m_bOn )
	{
		m_Overlay.Activate();
		m_GlowOverlay.Activate();
	}
	else
	{
		m_Overlay.Deactivate();
		m_GlowOverlay.Deactivate();
	}
}

void CSunGlowOverlay::CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor)
{
	if ( m_bModulateByDot )
	{
		float alpha = RemapVal( flDot, 1.0f, 0.9f, 0.75f, 0.0f );
		alpha = clamp( alpha, 0.0f, 0.75f );

		*flHorzSize = pSprite->m_flHorzSize * 6.0f;
		*flVertSize = pSprite->m_flVertSize * 6.0f;
		*vColor = pSprite->m_vColor * alpha * m_flGlowObstructionScale;
	}
	else
	{
		*flHorzSize = pSprite->m_flHorzSize;
		*flVertSize = pSprite->m_flVertSize;
		*vColor = pSprite->m_vColor * m_flGlowObstructionScale;
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Sunlight shadow control entity.

ConVar cl_sunlight_ortho_size(side_registry(), "cl_sunlight_ortho_size", "0.0", FCVAR_CHEAT, "Set to values greater than 0 for ortho view render projections.");
ConVar cl_sunlight_depthbias( side_registry(), "cl_sunlight_depthbias", "0.02" );

//------------------------------------------------------------------------------
// Purpose : Sunlights shadow control entity
//------------------------------------------------------------------------------
class CSunlightShadowControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SunlightShadowControl" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSunlightShadowControl, CBaseEntity );

	DECLARE_CLIENTCLASS();

	virtual ~CSunlightShadowControl();

	void OnDataChanged( DataUpdateType_t updateType );
	void Spawn();
	bool ShouldDraw();

	void ClientThink();

private:
	[[= ks::reflect::Net{} ]] Vector m_shadowDirection;
	[[= ks::reflect::Net{} ]] bool m_bEnabled;
	[[= ks::reflect::Net{} ]] char m_TextureName[ MAX_PATH ];
	CTextureReference m_SpotlightTexture;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32	m_LightColor;
	Vector m_CurrentLinearFloatLightColor;
	float m_flCurrentLinearFloatLightAlpha;
	[[= ks::reflect::Net{} ]] float m_flColorTransitionTime;
	[[= ks::reflect::Net{} ]] float m_flSunDistance;
	[[= ks::reflect::Net{} ]] float m_flFOV;
	[[= ks::reflect::Net{} ]] float m_flNearZ;
	[[= ks::reflect::Net{} ]] float m_flNorthOffset;
	[[= ks::reflect::Net{} ]] bool m_bEnableShadows;
	bool m_bOldEnableShadows;

	static ClientShadowHandle_t m_LocalFlashlightHandle;
};

ClientShadowHandle_t CSunlightShadowControl::m_LocalFlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;

IMPLEMENT_REFLECT_CLIENTCLASS( CSunlightShadowControl, DT_SunlightShadowControl, CSunlightShadowControl )

CSunlightShadowControl::~CSunlightShadowControl()
{
	if ( m_LocalFlashlightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( m_LocalFlashlightHandle );
		m_LocalFlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}

void CSunlightShadowControl::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_SpotlightTexture.Init( m_TextureName, TEXTURE_GROUP_OTHER, true );
	}

	BaseClass::OnDataChanged( updateType );
}

void CSunlightShadowControl::Spawn()
{
	BaseClass::Spawn();

	m_bOldEnableShadows = m_bEnableShadows;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CSunlightShadowControl::ShouldDraw()
{
	return false;
}

extern ConVar r_flashlightdepthres;
void CSunlightShadowControl::ClientThink()
{
	CVProfScope _( "CSunlightShadowControl::ClientThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_bEnabled )
	{
		Vector vLinearFloatLightColor( m_LightColor.r, m_LightColor.g, m_LightColor.b );
		float flLinearFloatLightAlpha = m_LightColor.a;

		if ( m_CurrentLinearFloatLightColor != vLinearFloatLightColor || m_flCurrentLinearFloatLightAlpha != flLinearFloatLightAlpha )
		{
			float flColorTransitionSpeed = gpGlobals->frametime * m_flColorTransitionTime * 255.0f;

			m_CurrentLinearFloatLightColor.x = Approach( vLinearFloatLightColor.x, m_CurrentLinearFloatLightColor.x, flColorTransitionSpeed );
			m_CurrentLinearFloatLightColor.y = Approach( vLinearFloatLightColor.y, m_CurrentLinearFloatLightColor.y, flColorTransitionSpeed );
			m_CurrentLinearFloatLightColor.z = Approach( vLinearFloatLightColor.z, m_CurrentLinearFloatLightColor.z, flColorTransitionSpeed );
			m_flCurrentLinearFloatLightAlpha = Approach( flLinearFloatLightAlpha, m_flCurrentLinearFloatLightAlpha, flColorTransitionSpeed );
		}

		FlashlightState_t state;

		Vector vDirection = m_shadowDirection;
		VectorNormalize( vDirection );

		QAngle angView;
		engine->GetViewAngles( angView );

		//Vector vViewUp = Vector( 0.0f, 1.0f, 0.0f );
		Vector vSunDirection2D = vDirection;
		vSunDirection2D.z = 0.0f;

		HACK_GETLOCALPLAYER_GUARD( "CSunlightShadowControl::ClientThink" );

		if ( !CBasePlayer::GetLocalPlayer() )
			return;

		Vector vPos = ( CBasePlayer::GetLocalPlayer()->GetAbsOrigin() + vSunDirection2D * m_flNorthOffset ) - vDirection * m_flSunDistance;

		QAngle angAngles;
		VectorAngles( vDirection, angAngles );

		Vector vForward, vRight, vUp;
		AngleVectors( angAngles, &vForward, &vRight, &vUp );

		state.m_fHorizontalFOVDegrees = m_flFOV;
		state.m_fVerticalFOVDegrees = m_flFOV;

		state.m_vecLightOrigin = vPos;
		BasisToQuaternion( vForward, vRight, vUp, state.m_quatOrientation );

		state.m_fQuadraticAtten = 0.0f;
		state.m_fLinearAtten = m_flSunDistance / 2.0f;
		state.m_fConstantAtten = 0.0f;
		state.m_FarZAtten = m_flSunDistance + 300.0f;
		state.m_Color[0] = m_CurrentLinearFloatLightColor.x * ( 1.0f / 255.0f ) * m_flCurrentLinearFloatLightAlpha;
		state.m_Color[1] = m_CurrentLinearFloatLightColor.y * ( 1.0f / 255.0f ) * m_flCurrentLinearFloatLightAlpha;
		state.m_Color[2] = m_CurrentLinearFloatLightColor.z * ( 1.0f / 255.0f ) * m_flCurrentLinearFloatLightAlpha;
		state.m_Color[3] = 0.0f; // fixme: need to make ambient work m_flAmbient;
		state.m_NearZ = fpmax( 4.0f, m_flSunDistance - m_flNearZ );
		state.m_FarZ = m_flSunDistance + 300.0f;

		float flOrthoSize = cl_sunlight_ortho_size.GetFloat();

		if ( flOrthoSize > 0 )
		{
			state.m_bOrtho = true;
			state.m_fOrthoLeft = -flOrthoSize;
			state.m_fOrthoTop = -flOrthoSize;
			state.m_fOrthoRight = flOrthoSize;
			state.m_fOrthoBottom = flOrthoSize;
		}
		else
		{
			state.m_bOrtho = false;
		}

		state.m_flShadowSlopeScaleDepthBias = 2;
		state.m_flShadowDepthBias = cl_sunlight_depthbias.GetFloat();
		state.m_bEnableShadows = m_bEnableShadows;
		state.m_pSpotlightTexture = m_SpotlightTexture;
		state.m_pProjectedMaterial = nullptr;
		state.m_nSpotlightTextureFrame = 0;
		state.m_flShadowMapResolution = r_flashlightdepthres.GetFloat();

		state.m_nShadowQuality = 1; // Allow entity to affect shadow quality
		state.m_bShadowHighRes = true;

		if ( m_bOldEnableShadows != m_bEnableShadows )
		{
			// If they change the shadow enable/disable, we need to make a new handle
			if ( m_LocalFlashlightHandle != CLIENTSHADOW_INVALID_HANDLE )
			{
				g_pClientShadowMgr->DestroyFlashlight( m_LocalFlashlightHandle );
				m_LocalFlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;
			}

			m_bOldEnableShadows = m_bEnableShadows;
		}

		if( m_LocalFlashlightHandle == CLIENTSHADOW_INVALID_HANDLE )
		{
			m_LocalFlashlightHandle = g_pClientShadowMgr->CreateFlashlight( state );
		}
		else
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_LocalFlashlightHandle, state );
			g_pClientShadowMgr->UpdateProjectedTexture( m_LocalFlashlightHandle, true );
		}
	}
	else if ( m_LocalFlashlightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( m_LocalFlashlightHandle );
		m_LocalFlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}

	BaseClass::ClientThink();
}

using namespace ::ks::reflect;

#ifdef USE_BLOBULATOR

ConVar r_surface_draw_isosurface( side_registry(), "r_surface_draw_isosurface", "1", 0, "Draws the surface as an isosurface" );

ConVar r_surface_rotate( side_registry(), "r_surface_rotate", "1", FCVAR_NONE, "Whether to rotate for transparency" );
ConVar r_surface_rotate_by90( side_registry(), "r_surface_rotate_by90", "1", FCVAR_NONE, "Whether to only rotate in 90 degree increments" );

ConVar r_surface_blr_scale( side_registry(), "r_surface_blr_scale", "1.0", FCVAR_NONE, "Scale all surface rendering parameters." );
ConVar r_surface_blr_cubewidth( side_registry(), "r_surface_blr_cubewidth", "0.8", FCVAR_NONE, "Set cubewidth (coarseness of the mesh)" );
ConVar r_surface_blr_render_radius( side_registry(), "r_surface_blr_render_radius", "1.3", FCVAR_NONE, "Set render radius (how far from particle center surface will be)" );
ConVar r_surface_blr_cutoff_radius( side_registry(), "r_surface_blr_cutoff_radius", "3.3", FCVAR_NONE, "Set cutoff radius (how far field extends from each particle)" );

ConVar r_surface_calc_uv_and_tan( side_registry(), "r_surface_calc_uv_and_tan", "1", FCVAR_ARCHIVE, "Calculate UVs and Tangents" );
ConVar r_surface_calc_tan_only( side_registry(), "r_surface_calc_tan_only", "0", FCVAR_ARCHIVE, "Calculate Only Tangents" );
ConVar r_surface_calc_color( side_registry(), "r_surface_calc_color", "0", FCVAR_ARCHIVE, "Just interpolate colors" );
ConVar r_surface_calc_hifreq_color( side_registry(), "r_surface_calc_hifreq_color", "0", FCVAR_ARCHIVE, "Experimental hi-freq colors" );
ConVar r_surface_calc_tile_color( side_registry(), "r_surface_calc_tile_color", "0", FCVAR_ARCHIVE, "Shows color of the tile" );

ConVar r_surface_debug_use_tiler(side_registry(), "r_surface_debug_use_tiler", "1", FCVAR_NONE, "Use the tiler");
ConVar r_surface_debug_max_tiles( side_registry(), "r_surface_debug_max_tiles", "-1", FCVAR_NONE, "The maximum number of tiles to draw" );
ConVar r_surface_debug_max_slices( side_registry(), "r_surface_debug_max_slices", "-1", FCVAR_NONE, "The maximum number of slices to draw" );
ConVar r_surface_debug_tile(side_registry(), "r_surface_debug_tile", "1", FCVAR_NONE, "If tiler is enabled, whether we draw all tiles or just the central one.");
ConVar r_surface_debug_draw_margin(side_registry(), "r_surface_debug_draw_margin", "0", FCVAR_NONE, "If tiler is disabled, whether to draw the margin.");
ConVar r_surface_debug_draw_tile_boundaries(side_registry(), "r_surface_debug_draw_tile_boundaries", "0", FCVAR_NONE, "Whether to draw outlines of all tiles.");

ConVar	r_surface_wireframe( side_registry(), "r_surface_wireframe", "0", FCVAR_NONE, "Draw wireframe" );
ConVar	r_surface_material(side_registry(), "r_surface_material", "-1", FCVAR_NONE, "Choose a material from 0 to N");
ConVar	r_surface_shader(side_registry(), "r_surface_shader", "", FCVAR_NONE, "Choose a shader");

ks::aligned_vector< ImpParticleWithFourInterpolants, 16 > g_SurfaceRenderParticles;	// This must be aligned due to SSE use
const QAngle g_SurfaceRenderAnglesAngles( 0.0f, 0.0f, 0.0f );	// our local coordinate system is always aligned to world space

//-----------------------------------------------------------------------------
// Purpose: Draw a Sphere
//-----------------------------------------------------------------------------

float g_FastSpherePosData[51][8] = {
	{  0.0000,  0.0000,  1.0000,  0.0000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{ -0.0000,  0.5000,  0.8660,  0.2500,  0.1667, -0.0000,  0.5000,  0.8660 },
	{  0.5000,  0.0000,  0.8660,  0.0000,  0.1667,  0.5000,  0.0000,  0.8660 },
	{  0.0000,  0.0000,  1.0000,  0.5000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{ -0.5000, -0.0000,  0.8660,  0.5000,  0.1667, -0.5000, -0.0000,  0.8660 },
	{  0.0000,  0.0000, -1.0000,  0.0000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{  0.5000,  0.0000, -0.8660,  0.0000,  0.8333,  0.5000,  0.0000, -0.8660 },
	{ -0.0000,  0.5000, -0.8660,  0.2500,  0.8333, -0.0000,  0.5000, -0.8660 },
	{  0.0000,  0.0000, -1.0000,  0.5000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{ -0.5000, -0.0000, -0.8660,  0.5000,  0.8333, -0.5000, -0.0000, -0.8660 },
	{  0.0000, -0.5000,  0.8660,  0.7500,  0.1667,  0.0000, -0.5000,  0.8660 },
	{  0.0000,  0.0000,  1.0000,  1.0000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{  0.5000,  0.0000,  0.8660,  1.0000,  0.1667,  0.5000,  0.0000,  0.8660 },
	{  0.0000, -0.5000, -0.8660,  0.7500,  0.8333,  0.0000, -0.5000, -0.8660 },
	{  0.0000,  0.0000, -1.0000,  1.0000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{  0.5000,  0.0000, -0.8660,  1.0000,  0.8333,  0.5000,  0.0000, -0.8660 },
	{  0.6124,  0.6124,  0.5000,  0.1250,  0.3333,  0.6124,  0.6124,  0.5000 },
	{  0.8660,  0.0000,  0.5000,  0.0000,  0.3333,  0.8660,  0.0000,  0.5000 },
	{ -0.0000,  0.8660,  0.5000,  0.2500,  0.3333, -0.0000,  0.8660,  0.5000 },
	{  0.8660,  0.0000, -0.5000,  0.0000,  0.6667,  0.8660,  0.0000, -0.5000 },
	{  0.6124,  0.6124, -0.5000,  0.1250,  0.6667,  0.6124,  0.6124, -0.5000 },
	{ -0.0000,  0.8660, -0.5000,  0.2500,  0.6667, -0.0000,  0.8660, -0.5000 },
	{ -0.6124,  0.6124,  0.5000,  0.3750,  0.3333, -0.6124,  0.6124,  0.5000 },
	{ -0.8660, -0.0000,  0.5000,  0.5000,  0.3333, -0.8660, -0.0000,  0.5000 },
	{ -0.6124,  0.6124, -0.5000,  0.3750,  0.6667, -0.6124,  0.6124, -0.5000 },
	{ -0.8660, -0.0000, -0.5000,  0.5000,  0.6667, -0.8660, -0.0000, -0.5000 },
	{ -0.6124, -0.6124,  0.5000,  0.6250,  0.3333, -0.6124, -0.6124,  0.5000 },
	{  0.0000, -0.8660,  0.5000,  0.7500,  0.3333,  0.0000, -0.8660,  0.5000 },
	{ -0.6124, -0.6124, -0.5000,  0.6250,  0.6667, -0.6124, -0.6124, -0.5000 },
	{  0.0000, -0.8660, -0.5000,  0.7500,  0.6667,  0.0000, -0.8660, -0.5000 },
	{  0.6124, -0.6124,  0.5000,  0.8750,  0.3333,  0.6124, -0.6124,  0.5000 },
	{  0.8660,  0.0000,  0.5000,  1.0000,  0.3333,  0.8660,  0.0000,  0.5000 },
	{  0.6124, -0.6124, -0.5000,  0.8750,  0.6667,  0.6124, -0.6124, -0.5000 },
	{  0.8660,  0.0000, -0.5000,  1.0000,  0.6667,  0.8660,  0.0000, -0.5000 },
	{  0.9239,  0.3827,  0.0000,  0.0625,  0.5000,  0.9239,  0.3827,  0.0000 },
	{  1.0000,  0.0000,  0.0000,  0.0000,  0.5000,  1.0000,  0.0000,  0.0000 },
	{  0.7071,  0.7071,  0.0000,  0.1250,  0.5000,  0.7071,  0.7071,  0.0000 },
	{  0.3827,  0.9239,  0.0000,  0.1875,  0.5000,  0.3827,  0.9239,  0.0000 },
	{ -0.0000,  1.0000,  0.0000,  0.2500,  0.5000, -0.0000,  1.0000,  0.0000 },
	{ -0.3827,  0.9239,  0.0000,  0.3125,  0.5000, -0.3827,  0.9239,  0.0000 },
	{ -0.7071,  0.7071,  0.0000,  0.3750,  0.5000, -0.7071,  0.7071,  0.0000 },
	{ -0.9239,  0.3827,  0.0000,  0.4375,  0.5000, -0.9239,  0.3827,  0.0000 },
	{ -1.0000, -0.0000,  0.0000,  0.5000,  0.5000, -1.0000, -0.0000,  0.0000 },
	{ -0.9239, -0.3827,  0.0000,  0.5625,  0.5000, -0.9239, -0.3827,  0.0000 },
	{ -0.7071, -0.7071,  0.0000,  0.6250,  0.5000, -0.7071, -0.7071,  0.0000 },
	{ -0.3827, -0.9239,  0.0000,  0.6875,  0.5000, -0.3827, -0.9239,  0.0000 },
	{  0.0000, -1.0000,  0.0000,  0.7500,  0.5000,  0.0000, -1.0000,  0.0000 },
	{  0.3827, -0.9239,  0.0000,  0.8125,  0.5000,  0.3827, -0.9239,  0.0000 },
	{  0.7071, -0.7071,  0.0000,  0.8750,  0.5000,  0.7071, -0.7071,  0.0000 },
	{  0.9239, -0.3827,  0.0000,  0.9375,  0.5000,  0.9239, -0.3827,  0.0000 },
	{  1.0000,  0.0000,  0.0000,  1.0000,  0.5000,  1.0000,  0.0000,  0.0000 }
};

int g_FastSphereTriData[84][3] = {
	{ 0, 1, 2 },
	{ 0, 3, 1 },
	{ 3, 4, 1 },
	{ 5, 6, 7 },
	{ 5, 7, 8 },
	{ 8, 7, 9 },
	{ 3, 10, 4 },
	{ 3, 11, 10 },
	{ 11, 12, 10 },
	{ 8, 9, 13 },
	{ 8, 13, 14 },
	{ 14, 13, 15 },
	{ 2, 16, 17 },
	{ 2, 1, 16 },
	{ 1, 18, 16 },
	{ 6, 19, 20 },
	{ 6, 20, 7 },
	{ 7, 20, 21 },
	{ 1, 22, 18 },
	{ 1, 4, 22 },
	{ 4, 23, 22 },
	{ 7, 21, 24 },
	{ 7, 24, 9 },
	{ 9, 24, 25 },
	{ 4, 26, 23 },
	{ 4, 10, 26 },
	{ 10, 27, 26 },
	{ 9, 25, 28 },
	{ 9, 28, 13 },
	{ 13, 28, 29 },
	{ 10, 30, 27 },
	{ 10, 12, 30 },
	{ 12, 31, 30 },
	{ 13, 29, 32 },
	{ 13, 32, 15 },
	{ 15, 32, 33 },
	{ 17, 34, 35 },
	{ 17, 16, 34 },
	{ 16, 36, 34 },
	{ 19, 35, 34 },
	{ 19, 34, 20 },
	{ 20, 34, 36 },
	{ 16, 37, 36 },
	{ 16, 18, 37 },
	{ 18, 38, 37 },
	{ 20, 36, 37 },
	{ 20, 37, 21 },
	{ 21, 37, 38 },
	{ 18, 39, 38 },
	{ 18, 22, 39 },
	{ 22, 40, 39 },
	{ 21, 38, 39 },
	{ 21, 39, 24 },
	{ 24, 39, 40 },
	{ 22, 41, 40 },
	{ 22, 23, 41 },
	{ 23, 42, 41 },
	{ 24, 40, 41 },
	{ 24, 41, 25 },
	{ 25, 41, 42 },
	{ 23, 43, 42 },
	{ 23, 26, 43 },
	{ 26, 44, 43 },
	{ 25, 42, 43 },
	{ 25, 43, 28 },
	{ 28, 43, 44 },
	{ 26, 45, 44 },
	{ 26, 27, 45 },
	{ 27, 46, 45 },
	{ 28, 44, 45 },
	{ 28, 45, 29 },
	{ 29, 45, 46 },
	{ 27, 47, 46 },
	{ 27, 30, 47 },
	{ 30, 48, 47 },
	{ 29, 46, 47 },
	{ 29, 47, 32 },
	{ 32, 47, 48 },
	{ 30, 49, 48 },
	{ 30, 31, 49 },
	{ 31, 50, 49 },
	{ 32, 48, 49 },
	{ 32, 49, 33 },
	{ 33, 49, 50 }
};

void Surface_DrawFastSphere( CMeshBuilder &meshBuilder, const Vector &center, float radius, float r, float g, float b )
{
	int offset = meshBuilder.GetCurrentVertex();

	Vector pos;
	for (int i = 0; i < 51; i++)
	{
		pos.x = g_FastSpherePosData[i][0] + center.x + g_FastSpherePosData[i][5] * radius;
		pos.y = g_FastSpherePosData[i][1] + center.y + g_FastSpherePosData[i][6] * radius;
		pos.z = g_FastSpherePosData[i][2] + center.z + g_FastSpherePosData[i][7] * radius;

		meshBuilder.Position3fv( pos.Base() );
		meshBuilder.Normal3fv( &g_FastSpherePosData[i][5] );
		meshBuilder.TexCoord2fv( 0, &g_FastSpherePosData[i][3] );
		meshBuilder.Color3f( r, g, b );
		meshBuilder.AdvanceVertex();
	}

	for (int i = 0; i < 84; i++)
	{
		meshBuilder.FastIndex( g_FastSphereTriData[i][0] + offset );
		meshBuilder.FastIndex( g_FastSphereTriData[i][1] + offset );
		meshBuilder.FastIndex( g_FastSphereTriData[i][2] + offset );
	}
}

struct Surface_DrawSpheres_sortParticles_t
{
	int no;
	float dist;

	class C
	{
	public:
		static bool gt(Surface_DrawSpheres_sortParticles_t a, Surface_DrawSpheres_sortParticles_t b)
		{
			return a.dist < b.dist;
		}
	};
};

void Surface_DrawSpheres( IMaterial *pMaterial, float flRadius )
{
	Point3D eye = view->GetViewSetup()->origin;

	SmartArray<Surface_DrawSpheres_sortParticles_t> sort_particles;
	sort_particles.ensureCapacity(g_SurfaceRenderParticles.Count());
	sort_particles.size = g_SurfaceRenderParticles.Count();
	for(int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
	{
		sort_particles[i].no = i;
		sort_particles[i].dist = g_SurfaceRenderParticles[i].center.length(eye);
	}
	sort_particles.sort<Surface_DrawSpheres_sortParticles_t::C>();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->Bind( pMaterial );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	int vertMax = std::min( 24000 / 51, 32768 / (84 * 3) );

	int j = 0;
	// Msg( "point %.2f %.2f %.2f\n", m_vecSurfacePos[0].x, m_vecSurfacePos[0].y, m_vecSurfacePos[0].z );
	while (j < g_SurfaceRenderParticles.Count())
	{
		int total = std::min( g_SurfaceRenderParticles.Count() - j, vertMax );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, total * 51, total * 84 * 3 );

		int i = 0;
		while (i < vertMax && j < g_SurfaceRenderParticles.Count())
		{
			ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[sort_particles[j].no]);

			if (imp_particle->scale > 0.01)
			{
				Surface_DrawFastSphere( meshBuilder, imp_particle->center.AsVector(), flRadius * imp_particle->scale, imp_particle->interpolants1[0], imp_particle->interpolants1[1], imp_particle->interpolants1[2] );
				i++;
			}
			j++;
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}

void Surface_SafeLightCubeUpdate( const Vector &vecRenderOrigin, Vector4D *cachedCubeColours )
{
	// We sometimes get an invalid ambient lightcube for the isosurface sample point, this func smooths over that
	// FIXME: this is probably to do with sampling at invalid locations
	// FIXME: we need more complex lighting data for isosurfaces anyway
	//        (per-vertex light sample indices ala skinning, or UVWs into a light sample texture)

	Vector boxColors[6];
	engine->ComputeLightingCube( vecRenderOrigin, false, boxColors );

	// Are we trying to sample outside the world?
	bool invalidOrigin = enginetrace->PointOutsideWorld( vecRenderOrigin );

	for ( int i = 0; i < 6; i++ )
	{
		if( !IsFinite( boxColors[i].x ) || !IsFinite( boxColors[i].y ) || !IsFinite( boxColors[i].z ) )
		{
			DevWarning( "Isosurface lighting cube with infinite values!\n%s",
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
			return;
		}
	}

	for ( int i = 0; i < 6; i++ )
	{
		if( boxColors[i].x < -0.1f || boxColors[i].y < -0.1f || boxColors[i].z < -0.1f )
		{
			DevWarning( "Isosurface lighting cube negative: %f %f %f\n%s", boxColors[i].x, boxColors[i].y, boxColors[i].z,
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
		}

		cachedCubeColours[i].AsVector3D() = Lerp( clamp( 2.0f * gpGlobals->frametime, 0.0f, 1.0f ), cachedCubeColours[i].AsVector3D(), boxColors[i] );
	}

	bool bBadLightCube = false;
	for ( int i = 0; i < 6; i++ )
	{
		if ( !IsFinite( cachedCubeColours[i].x ) || !IsFinite( cachedCubeColours[i].y ) || !IsFinite( cachedCubeColours[i].z ) ||
			 ( cachedCubeColours[i].x < -0.01f ) || ( cachedCubeColours[i].y < -0.01f ) || ( cachedCubeColours[i].z < -0.01f )  )
		{
			DevWarning( "Isosurface lighting cube bad. Resetting.\n%s",
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
			bBadLightCube = true;
			break;
		}
	}

	if ( bBadLightCube )
	{
		memset( cachedCubeColours, 0, sizeof( cachedCubeColours ) );
	}
}

IMaterial* GetDrawMaterial( void )
{
	if ( r_surface_wireframe.GetBool() )
	{
		if( r_surface_material.GetInt() < 0 )
		{
			return materials->FindMaterial("shadertest/wireframe", TEXTURE_GROUP_OTHER);
		}
		else
		{
			return materials->FindMaterial("shadertest/wireframevertexcolornocull", TEXTURE_GROUP_OTHER);
		}
	}
	else if ( strlen( r_surface_shader.GetString() ) > 0 )
	{
		return materials->FindMaterial( r_surface_shader.GetString(), TEXTURE_GROUP_OTHER, true );
	}
	else
	{
		switch ( r_surface_material.GetInt() )
		{
			case 0:
				return materials->FindMaterial( "models/debug/debugwhite", TEXTURE_GROUP_OTHER, true );
			case 1:
				return materials->FindMaterial( "models/debug/debugwhite2", TEXTURE_GROUP_OTHER, true );
			case 2:
				return materials->FindMaterial( "models/debug/debugwhite3", TEXTURE_GROUP_OTHER, true );
			case 3:
				return materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true );
			case 4:
				return materials->FindMaterial( "debug/env_cubemap_model", TEXTURE_GROUP_OTHER, true );
			case 5:
				return materials->FindMaterial( "models/blob/env_cubemap_model_translucent_fountain", TEXTURE_GROUP_OTHER, true );
			case 6:
				return materials->FindMaterial( "models/debug/debugmesh", TEXTURE_GROUP_OTHER, true );
			case 7:
				return materials->FindMaterial( "models/debug/debugmesh_transparent", TEXTURE_GROUP_OTHER, true );
			case 8:
				return materials->FindMaterial( "models/ihvtest/tongue_bumped", TEXTURE_GROUP_OTHER, true );
			case 9:
				return materials->FindMaterial( "models/debug/debugbumps", TEXTURE_GROUP_OTHER, true );
			case 10:
				return materials->FindMaterial( "debug/env_cubemap_model_translucent_no_bumps", TEXTURE_GROUP_OTHER, true );
			case 11:
				return materials->FindMaterial( "models/shadertest/predator", TEXTURE_GROUP_OTHER, true );
			default:
				return nullptr;
		}
	}
}

void SetUpImpRendererUserDefinedFuncs( void )
{
	RENDERER_CLASS::setCalcSignFunc(calcSign);
	RENDERER_CLASS::setCalcSign2Func(calcSign2);

	if(r_surface_calc_uv_and_tan.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_UV_TAN_CI_SIZE, calcCornerNormalColorUVTan);
		//RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColorUVTan);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNTexCoord4); // HACKHACK: This is blob specific!
	}
	else if(r_surface_calc_tan_only.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_UV_TAN_CI_SIZE, calcCornerNormalColorTanNoUV);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColorTanNoUV);
	}
	else if	(r_surface_calc_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	}
	else if (r_surface_calc_hifreq_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalHiFreqColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	}
	else if (r_surface_calc_tile_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_CI_SIZE, calcCornerNormal);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalDebugColor);
	}
	else
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_CI_SIZE, calcCornerNormal);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormal);
	}
}

void TransformParticles( float angle, const Vector& center, Vector& transformedCenter, Vector& transformedEye, VMatrix& rotationMatrix, VMatrix& invRotationMatrix )
{
	for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
	{
		ImpParticleWithFourInterpolants* imp_particle = &(g_SurfaceRenderParticles[i]);

		Vector vParticle = imp_particle->center.AsVector();
		Vector transformedParticle = vParticle-center;

		transformedParticle = rotationMatrix.ApplyRotation(transformedParticle);

		Point3D pParticle = transformedParticle; 

		imp_particle->center = pParticle;

		// HACK ALERT!!!
		// TODO: This code should not be here as this is very specific to blobs.

		// interpolants1[0..2] is the color. interpolants1[3] is the v coordinate
		// imp_particle1->interpolants1[3] = std::min(std::max(1.4f - vec.length()/17.0f, 0.0f), 1.0f);
		// interpolants2[0..2] is the tangent vector.
		// interpolants3[0..2] and interpolants4[0..2] are the normal and
		// binormal which are used to generate a u coordinate
		Point3D pCenter = transformedCenter;
		Point3D vec = (pParticle - pCenter);

		imp_particle->interpolants2 = vec.unit();
		imp_particle->interpolants4.set(0.0f, 0.0f, -1.0f);
		imp_particle->interpolants3 = imp_particle->interpolants2.crossProduct(imp_particle->interpolants4);
		imp_particle->interpolants3.normalize();
		imp_particle->interpolants4 = imp_particle->interpolants2.crossProduct(imp_particle->interpolants3);
		imp_particle->interpolants4.normalize();
	}
}

void Surface_DrawIsoSurface( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth )
{
	IMaterial *pSpecialMaterial = GetDrawMaterial();

	if( pSpecialMaterial && !IsErrorMaterial( pSpecialMaterial ) )
	{
		pMaterial = pSpecialMaterial;
	}

	//////////////////////////////////////////////////
	// Set up the render context
	/////////////////////////////////////////////////
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->Bind( pMaterial, pClientRenderable );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	////////////////////////////////////////////////////
	// Set up rotations and translations and corresponding matrices.
	////////////////////////////////////////////////////
	Vector transformedCenter = vecRenderOrigin;

	pRenderContext->Translate(vecRenderOrigin.x, vecRenderOrigin.y, vecRenderOrigin.z);

	VMatrix rotationMatrix;
	VMatrix invRotationMatrix;
	Vector transformedEye;
	float angle = 0.0f;
	if( r_surface_rotate.GetBool() )
	{
		angle = view->GetViewSetup()->angles.y+180.0f;

		if( r_surface_rotate_by90.GetBool() )
		{
			// This is a test mode which locks the rotation to a multiple of 90.
			angle = float(int((angle + 45.0f) / 90.0f) * 90);
		}

		pRenderContext->Rotate(angle, 0.0f, 0.0f, 1.0f);

		rotationMatrix = SetupMatrixAxisRot(Vector(0.0f, 0.0f, 1.0f), -angle);
		invRotationMatrix = SetupMatrixAxisRot(Vector(0.0f, 0.0f, 1.0f), angle);
		Vector eye = view->GetViewSetup()->origin;
		transformedEye = eye-vecRenderOrigin;
		transformedEye = rotationMatrix.ApplyRotation(transformedEye);
	}
	else
	{
		rotationMatrix.Identity();
		invRotationMatrix.Identity();
		transformedEye.Init();
		angle = 0.0f;
	}

	///////////////////////////////////////////////////////////////
	// Begin Rendering by initializing the sweepRenderer or Tiler
	// (depending on whether we're using the tiler)
	///////////////////////////////////////////////////////////////

	// TODO: These probably shouldn't be static variables (not thread safe)
	RENDERER_CLASS::setCubeWidth( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_cubewidth.GetFloat());
	RENDERER_CLASS::setRenderR( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_render_radius.GetFloat());
	RENDERER_CLASS::setCutoffR( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_cutoff_radius.GetFloat());
	SetUpImpRendererUserDefinedFuncs();

	TransformParticles( angle, vecRenderOrigin, transformedCenter, transformedEye, rotationMatrix, invRotationMatrix );

	///////////////////////////////////////////////////////////////
	// Insert the particles into the tiler/renderer, and do a rendering
	// pass. If we are tiling, we can also visualize the tiles and slices.
	///////////////////////////////////////////////////////////////
	if( r_surface_debug_use_tiler.GetBool() )
	{
		ImpTiler* tiler = ImpTilerFactory::factory->getTiler();
		RENDERER_CLASS* sweepRenderer = tiler->getRenderer();

		tiler->setMaxNoTilesToDraw(r_surface_debug_max_tiles.GetInt());
		sweepRenderer->setMaxNoSlicesToDraw(r_surface_debug_max_slices.GetInt());

		// TODO: Do we always want to center the tiler on 0,0,0?
		// I guess if we are centering the render coordinates, then the answer
		// is a yes.
		tiler->beginFrame(Point3D(0.0f, 0.0f, 0.0f), (void*)&pRenderContext, !(r_surface_debug_draw_margin.GetBool()));

		for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
		{
			tiler->insertParticle(&g_SurfaceRenderParticles[i]);
		}

		if(r_surface_debug_tile.GetBool())
		{
			if(r_surface_rotate.GetBool())
			{
				tiler->drawSurfaceSorted(Point3D(transformedEye));
			}
			else
			{
				tiler->drawSurface();
			}

			// This is code used for visualizing the boundaries of every tile that's drawn
			// and maybe in the future, slices within the tiles.
			if( r_surface_debug_draw_tile_boundaries.GetBool() == true )
			{
				for( int i = 0; i < tiler->getNoTiles(); i++ )
				{
					Vector overlayCenter = invRotationMatrix.ApplyRotation(tiler->getTileOffset(i).AsVector());
					Vector mins = tiler->getRenderDim().AsVector() * -0.5f;
					Vector maxs = tiler->getRenderDim().AsVector() * 0.5f;
					debugoverlay->AddBoxOverlay(overlayCenter + vecRenderOrigin, mins, maxs, QAngle( 0, angle, 0 ), 0, 255, 0, 0, 0 );
				}
			}
		}
		else
		{
			tiler->drawTile(0,0,0);

			// This is code used for visualizing the boundaries of the last tile drawn
			// slices within that tile
			if( r_surface_debug_max_tiles.GetInt() > 0 || r_surface_debug_tile.GetBool() == false )
			{
				Vector overlayCenter = invRotationMatrix.ApplyRotation(tiler->getLastTilesOffset().AsVector());
				Vector mins = tiler->getRenderDim().AsVector() * -0.5f;
				Vector maxs = tiler->getRenderDim().AsVector() * 0.5f;
				debugoverlay->AddBoxOverlay(overlayCenter + vecRenderOrigin, mins, maxs, QAngle( 0, angle, 0 ), 0, 255, 0, 0, 0 );

				if( r_surface_debug_max_slices.GetInt() > 0 )
				{
					Vector sliceMins = mins;
					Vector sliceMaxs = maxs;
					sliceMins.x += (sweepRenderer->getLastSliceDrawn() - sweepRenderer->getMarginNCubes()) * sweepRenderer->getCubeWidth();
					sliceMaxs.x = sliceMins.x + sweepRenderer->getCubeWidth();
					debugoverlay->AddBoxOverlay( overlayCenter + vecRenderOrigin, sliceMins, sliceMaxs, QAngle( 0, angle, 0 ), 255, 0, 0, 0, 0 );
				}
			}
		}

		tiler->endFrame();

		ImpTilerFactory::factory->returnTiler(tiler);
	}
	else
	{
		RENDERER_CLASS* sweepRenderer = ImpRendererFactory::factory->getRenderer();

		// This is the no tiler version. We just draw the center tile.
		sweepRenderer->beginFrame(!(r_surface_debug_draw_margin.GetBool()), (void*)&pRenderContext);
		sweepRenderer->setOffset(Point3D(0.0f, 0.0f, 0.0f));
		sweepRenderer->beginTile(nullptr);

		for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
		{
			sweepRenderer->addParticle(&(g_SurfaceRenderParticles[i]));
		}

		sweepRenderer->endTile();
		sweepRenderer->endFrame();

		ImpRendererFactory::factory->returnRenderer(sweepRenderer);
	}

	//////////////////////////////////////
	// Clean up render context
	//////////////////////////////////////
	pRenderContext->PopMatrix();
}

void Surface_CullOutOfViewParticles( void )
{
	int iParticlesRemoved = 0;

	for ( int i = 0; i < g_SurfaceRenderParticles.Count() - iParticlesRemoved; ++i )
	{
		ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[ i ]);
		Vector vCenter = Vector( imp_particle->center[ 0 ], imp_particle->center[ 1 ], imp_particle->center[ 2 ] );

		if ( R_CullSphere( view->GetFrustum(), 5, &vCenter, imp_particle->scale * 24.0f ) )
		{
			ImpParticleWithOneInterpolant* last_particle = &(g_SurfaceRenderParticles[ g_SurfaceRenderParticles.Count() - iParticlesRemoved - 1 ]);
			*imp_particle = *last_particle;

			++iParticlesRemoved;
			--i;
		}
	}

	if ( iParticlesRemoved )
	{
		g_SurfaceRenderParticles.SetCountNonDestructively( g_SurfaceRenderParticles.Count() - iParticlesRemoved );
	}
}

void Surface_Draw( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth, bool bSurfaceNoParticleCull )
{
	if ( !bSurfaceNoParticleCull )
	{
		// Reduce the complexity by first removing particles that won't visibly affect the view
		Surface_CullOutOfViewParticles();
	}

	if ( !r_surface_draw_isosurface.GetBool() )
	{
		// This draws particles as spheres instead of an isosurface
		Surface_DrawSpheres( pMaterial, flCubeWidth );
	}
	else
	{
		Surface_DrawIsoSurface( pClientRenderable, vecRenderOrigin, pMaterial, flCubeWidth );
	}
}

#endif

class CTakeDamageInfo;
class CBaseEntity;

using namespace ::ks::reflect;

ConVar phys_pushscale( side_registry(), "phys_pushscale", "1", FCVAR_REPLICATED );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CTakeDamageInfo )

void CTakeDamageInfo::Init( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iCustomDamage, int iObjectsPenetrated )
{
	m_hInflictor = pInflictor;
	if ( pAttacker )
	{
		m_hAttacker = pAttacker;
	}
	else
	{
		m_hAttacker = pInflictor;
	}

	m_hWeapon = pWeapon;

	m_flDamage = flDamage;

	m_flBaseDamage = BASEDAMAGE_NOT_SPECIFIED;

	m_bitsDamageType = bitsDamageType;
	m_iDamageCustom = iCustomDamage;

	m_flMaxDamage = flDamage;
	m_vecDamageForce = damageForce;
	m_vecDamagePosition = damagePosition;
	m_vecReportedPosition = reportedPosition;
	m_iAmmoType = -1;
	m_flRadius = 0.0f;
	m_iDamagedOtherPlayers = 0;
	m_iObjectsPenetrated = iObjectsPenetrated;
	m_uiBulletID = 0;
}

CTakeDamageInfo::CTakeDamageInfo()
{
	Init( nullptr, nullptr, nullptr, vec3_origin, vec3_origin, vec3_origin, 0, 0, 0, 0 );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, pWeapon, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Init( pInflictor, pAttacker, nullptr, vec3_origin, vec3_origin, vec3_origin, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Init( pInflictor, pAttacker, pWeapon, vec3_origin, vec3_origin, vec3_origin, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, nullptr, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Vector vecReported = vec3_origin;
	if ( reportedPosition )
	{
		vecReported = *reportedPosition;
	}
	Init( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, vecReported, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

//-----------------------------------------------------------------------------
// Squirrel the damage value away as BaseDamage, which will later be used to 
// calculate damage force. 
//-----------------------------------------------------------------------------
void CTakeDamageInfo::AdjustPlayerDamageInflictedForSkillLevel()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTakeDamageInfo::AdjustPlayerDamageTakenForSkillLevel()
{
}

//-----------------------------------------------------------------------------
// Purpose: get the name of the ammo that caused damage
// Note: returns the ammo name, or the classname of the object, or the model name in the case of physgun ammo.
//-----------------------------------------------------------------------------
const char *CTakeDamageInfo::GetAmmoName() const
{
	const char *pszAmmoType;

	if ( m_iAmmoType >= 0 )
	{
		pszAmmoType = GetAmmoDef()->GetAmmoOfIndex( m_iAmmoType )->pName;
	}
	// no ammoType, so get the ammo name from the inflictor
	else if ( m_hInflictor != nullptr )
	{
		pszAmmoType = m_hInflictor->GetClassname();

		// check for physgun ammo.  unfortunate that this is in game_shared.
		if ( V_strcmp( pszAmmoType, "prop_physics" ) == 0 )
		{
			pszAmmoType = STRING( m_hInflictor->GetModelName() );
		}
	}
	else
	{
		pszAmmoType = "Unknown";
	}

	return pszAmmoType;
}

// -------------------------------------------------------------------------------------------------- //
// MultiDamage
// Collects multiple small damages into a single damage
// -------------------------------------------------------------------------------------------------- //
IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CMultiDamage, CTakeDamageInfo )

CMultiDamage g_MultiDamage;

CMultiDamage::CMultiDamage()
{
	m_hTarget = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiDamage::Init( CBaseEntity *pTarget, CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	m_hTarget = pTarget;
	BaseClass::Init( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, reportedPosition, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the global multi damage accumulator
//-----------------------------------------------------------------------------
void ClearMultiDamage( void )
{
	g_MultiDamage.Init( nullptr, nullptr, nullptr, nullptr, vec3_origin, vec3_origin, vec3_origin, 0, 0, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: inflicts contents of global multi damage register on gMultiDamage.pEntity
//-----------------------------------------------------------------------------
void ApplyMultiDamage( void )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	trace_t		tr;

	if ( !g_MultiDamage.GetTarget() )
		return;

	g_MultiDamage.GetTarget()->TakeDamage( g_MultiDamage );

	// Damage is done, clear it out
	ClearMultiDamage();
}

//-----------------------------------------------------------------------------
// Purpose: Add damage to the existing multidamage, and apply if it won't fit
//-----------------------------------------------------------------------------
void AddMultiDamage( const CTakeDamageInfo &info, CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	if ( pEntity != g_MultiDamage.GetTarget() )
	{
		ApplyMultiDamage();
		g_MultiDamage.Init( pEntity, info.GetInflictor(), info.GetAttacker(), info.GetWeapon(), vec3_origin, vec3_origin, vec3_origin, 0.0, info.GetDamageType(), info.GetDamageCustom(), info.GetObjectsPenetrated() );
		g_MultiDamage.SetDamagedOtherPlayers( info.GetDamagedOtherPlayers() );
	}

	g_MultiDamage.AddDamageType( info.GetDamageType() );
	g_MultiDamage.SetDamage( g_MultiDamage.GetDamage() + info.GetDamage() );
	g_MultiDamage.SetDamageForce( g_MultiDamage.GetDamageForce() + info.GetDamageForce() );
	g_MultiDamage.SetDamagePosition( info.GetDamagePosition() );
	g_MultiDamage.SetReportedPosition( info.GetReportedPosition() );
	g_MultiDamage.SetMaxDamage( std::max( g_MultiDamage.GetMaxDamage(), info.GetMaxDamage() ) );
	g_MultiDamage.SetAmmoType( info.GetAmmoType() );
	g_MultiDamage.SetBulletID( info.GetBulletID(), info.GetRecoilIndex() );

	bool bHasPhysicsForceDamage = !g_pGameRules->Damage_NoPhysicsForce( info.GetDamageType() );
	if ( bHasPhysicsForceDamage && g_MultiDamage.GetDamageType() != DMG_GENERIC )
	{
		// If you hit this assert, you've called TakeDamage with a damage type that requires a physics damage
		// force & position without specifying one or both of them. Decide whether your damage that's causing 
		// this is something you believe should impart physics force on the receiver. If it is, you need to 
		// setup the damage force & position inside the CTakeDamageInfo (Utility functions for this are in
		// takedamageinfo.cpp. If you think the damage shouldn't cause force (unlikely!) then you can set the 
		// damage type to DMG_GENERIC, or | DMG_CRUSH if you need to preserve the damage type for purposes of HUD display.
		if ( g_MultiDamage.GetDamageForce() == vec3_origin || g_MultiDamage.GetDamagePosition() == vec3_origin )
		{
			static int warningCount = 0;
			if ( ++warningCount < 10 )
			{
				if ( g_MultiDamage.GetDamageForce() == vec3_origin )
				{
					Warning( "AddMultiDamage:  g_MultiDamage.GetDamageForce() == vec3_origin\n" );
				}

				if ( g_MultiDamage.GetDamagePosition() == vec3_origin)
				{
					Warning( "AddMultiDamage:  g_MultiDamage.GetDamagePosition() == vec3_origin\n" );
				}
			}
		}
	}
}

//============================================================================================================
// Utility functions for physics damage force calculation 
//============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Returns an impulse scale required to push an object.
// Input  : flTargetMass - Mass of the target object, in kg
//			flDesiredSpeed - Desired speed of the target, in inches/sec.
//-----------------------------------------------------------------------------
float ImpulseScale( float flTargetMass, float flDesiredSpeed )
{
	return (flTargetMass * flDesiredSpeed);
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for an explosive
//-----------------------------------------------------------------------------
void CalculateExplosiveDamageForce( CTakeDamageInfo *info, const Vector &vecDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );

	float flClampForce = ImpulseScale( 75, 400 );

	// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
	float flForceScale = info->GetBaseDamage() * ImpulseScale( 75, 4 );

	if( flForceScale > flClampForce )
		flForceScale = flClampForce;

	// Fudge blast forces a little bit, so that each
	// victim gets a slightly different trajectory. 
	// This simulates features that usually vary from
	// person-to-person variables such as bodyweight,
	// which are all indentical for characters using the same model.
	flForceScale *= random_valve->RandomFloat( 0.85, 1.15 );

	// Calculate the vector and stuff it into the takedamageinfo
	Vector vecForce = vecDir;
	VectorNormalize( vecForce );
	vecForce *= flForceScale;
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for a bullet impact
//-----------------------------------------------------------------------------
void CalculateBulletDamageForce( CTakeDamageInfo *info, int iBulletType, const Vector &vecBulletDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );
	Vector vecForce = vecBulletDir;
	VectorNormalize( vecForce );
	vecForce *= GetAmmoDef()->DamageForce( iBulletType );
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
	contract_assert( vecForce!=vec3_origin );
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for a melee impact
//-----------------------------------------------------------------------------
void CalculateMeleeDamageForce( CTakeDamageInfo *info, const Vector &vecMeleeDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );

	// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
	float flForceScale = info->GetBaseDamage() * ImpulseScale( 75, 4 );
	Vector vecForce = vecMeleeDir;
	VectorNormalize( vecForce );
	vecForce *= flForceScale;
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
}

//-----------------------------------------------------------------------------
// Purpose: Try and guess the physics force to use.
//			This shouldn't be used for any damage where the damage force is unknown.
//			i.e. only use it for mapmaker specified damages.
//-----------------------------------------------------------------------------
void GuessDamageForce( CTakeDamageInfo *info, const Vector &vecForceDir, const Vector &vecForceOrigin, float flScale )
{
	if ( info->GetDamageType() & DMG_BULLET )
	{
		CalculateBulletDamageForce( info, GetAmmoDef()->Index("SMG1"), vecForceDir, vecForceOrigin, flScale );
	}
	else if ( info->GetDamageType() & DMG_BLAST )
	{
		CalculateExplosiveDamageForce( info, vecForceDir, vecForceOrigin, flScale );
	}
	else
	{
		CalculateMeleeDamageForce( info, vecForceDir, vecForceOrigin, flScale );
	}
}

// Debug functions for printing out damage types

// This table maps the DMG_* defines to their strings such that 
// for DMG_XXX = i << x  then table[i] = string for DMG_XXX

static const char * const s_DamageTypeToStrTable[] =
{
	"GENERIC",
	"CRUSH",
	"BULLET",
	"SLASH",
	"BURN",
	"VEHICLE",
	"FALL",
	"BLAST",
	"CLUB",
	"SHOCK",
	"SONIC",
	"ENERGYBEAM",
	"PREVENT_PHYSICS_FORCE",
	"NEVERGIB",
	"ALWAYSGIB",
	"DROWN",
	"PARALYZE",
	"NERVEGAS",
	"POISON",
	"RADIATION",
	"DROWNRECOVER",
	"ACID",
	"SLOWBURN",
	"REMOVENORAGDOLL",
	"PHYSGUN",
	"PLASMA",
	"AIRBOAT",
	"DISSOLVE",
	"BLAST_SURFACE",
	"DIRECT",
	"BUCKSHOT"
};
static constexpr auto DAMAGE_TYPE_STR_TABLE_ENTRIES = 31; // number of entries in table above

void CTakeDamageInfo::DebugGetDamageTypeString(unsigned int damageType, char *outbuf, int outbuflength )
{
	contract_assert( outbuflength > 0 );

	// we need to use snprintf to actually copy out the strings here because that's the only function that returns
	// how much text was output
	if ( damageType == 0 )
	{
		int charsWrit = V_snprintf(outbuf, outbuflength, "%s", s_DamageTypeToStrTable[0]);
		
		outbuflength -= charsWrit;
		outbuf += charsWrit; // advance the output pointer (now it sits on the null terminator)
	}

	// loop through the other entries in the table
	for (int i = 0;
		 outbuflength > 0 && i < (DAMAGE_TYPE_STR_TABLE_ENTRIES - 1);
		 ++i )
	{
		if ( damageType & (1 << i) )
		{
			// this bit was set. Print the corresponding entry from the table
			// (the index is +1 because entry 1 in the table corresponds to 1 << 0)
			int charsWrit = V_snprintf(outbuf, outbuflength, "%s ", s_DamageTypeToStrTable[i + 1]); 

			outbuflength -= charsWrit; // reduce the chars left
			outbuf += charsWrit; // advance the output pointer (now it sits on the null terminator)
		}
	}
}

/*
// instant damage

#define DMG_GENERIC			0			// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object. 
// NOTE: It's assumed crush damage is occurring as a result of physics collision, so no extra physics force is generated by crush damage.
// DON'T use DMG_CRUSH when damaging entities unless it's the result of a physics collision. You probably want DMG_CLUB instead.
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_VEHICLE			(1 << 4)	// hit by a vehicle
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam 
#define DMG_PREVENT_PHYSICS_FORCE		(1 << 11)	// Prevent a physics force 
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning

#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisoning - heals over time like drowning damage
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven

#define DMG_REMOVENORAGDOLL	(1<<22)		// with this bit OR'd in, no ragdoll will be created, and the target will be quietly removed.
// use this to kill an entity that you've already got a server-side ragdoll for

#define DMG_PHYSGUN			(1<<23)		// Hit by manipulator. Usually doesn't do any damage.
#define DMG_PLASMA			(1<<24)		// Shot by Cremator
#define DMG_AIRBOAT			(1<<25)		// Hit by the airboat's gun

#define DMG_DISSOLVE		(1<<26)		// Dissolving!
#define DMG_BLAST_SURFACE	(1<<27)		// A blast on the surface of water that cannot harm things underwater
#define DMG_DIRECT			(1<<28)
#define DMG_BUCKSHOT		(1<<29)		// not quite a bullet. Little, rounder, different.
*/

CBaseEntity* CTakeDamageInfo::GetInflictor() const
{
	return m_hInflictor;
}

void CTakeDamageInfo::SetInflictor( CBaseEntity *pInflictor )
{
	m_hInflictor = pInflictor;
}

CBaseEntity* CTakeDamageInfo::GetAttacker() const
{
	return m_hAttacker;
}

void CTakeDamageInfo::SetAttacker( CBaseEntity *pAttacker )
{
	m_hAttacker = pAttacker;
}

CBaseEntity* CTakeDamageInfo::GetWeapon() const
{
	return m_hWeapon;
}

void CTakeDamageInfo::SetWeapon( CBaseEntity *pWeapon )
{
	m_hWeapon = pWeapon;
}

float CTakeDamageInfo::GetDamage() const
{
	return m_flDamage;
}

void CTakeDamageInfo::SetDamage( float flDamage )
{
	m_flDamage = flDamage;
}

float CTakeDamageInfo::GetMaxDamage() const
{
	return m_flMaxDamage;
}

void CTakeDamageInfo::SetMaxDamage( float flMaxDamage )
{
	m_flMaxDamage = flMaxDamage;
}

void CTakeDamageInfo::ScaleDamage( float flScaleAmount )
{
	m_flDamage *= flScaleAmount;
}

void CTakeDamageInfo::AddDamage( float flAddAmount )
{
	m_flDamage += flAddAmount;
}

void CTakeDamageInfo::SubtractDamage( float flSubtractAmount )
{
	m_flDamage -= flSubtractAmount;
}

float CTakeDamageInfo::GetBaseDamage() const
{
	if( BaseDamageIsValid() )
		return m_flBaseDamage;

	// No one ever specified a base damage, so just return damage.
	return m_flDamage;
}

bool CTakeDamageInfo::BaseDamageIsValid() const
{
	return (m_flBaseDamage != BASEDAMAGE_NOT_SPECIFIED);
}

Vector CTakeDamageInfo::GetDamageForce() const
{
	return m_vecDamageForce;
}

void CTakeDamageInfo::SetDamageForce( const Vector &damageForce )
{
	m_vecDamageForce = damageForce;
}

void	CTakeDamageInfo::ScaleDamageForce( float flScaleAmount )
{
	m_vecDamageForce *= flScaleAmount;
}

Vector CTakeDamageInfo::GetDamagePosition() const
{
	return m_vecDamagePosition;
}

void CTakeDamageInfo::SetDamagePosition( const Vector &damagePosition )
{
	m_vecDamagePosition = damagePosition;
}

Vector CTakeDamageInfo::GetReportedPosition() const
{
	return m_vecReportedPosition;
}

void CTakeDamageInfo::SetReportedPosition( const Vector &reportedPosition )
{
	m_vecReportedPosition = reportedPosition;
}

int CTakeDamageInfo::GetDamageType() const
{
	return m_bitsDamageType;
}

void CTakeDamageInfo::SetDamageType( int bitsDamageType )
{
	m_bitsDamageType = bitsDamageType;
}

void	CTakeDamageInfo::AddDamageType( int bitsDamageType )
{
	m_bitsDamageType |= bitsDamageType;
}

int CTakeDamageInfo::GetDamageCustom() const
{
	return m_iDamageCustom;
}

void CTakeDamageInfo::SetDamageCustom( int iDamageCustom )
{
	m_iDamageCustom = iDamageCustom;
}

int CTakeDamageInfo::GetDamageStats() const
{
	return m_iDamageCustom;
}

void CTakeDamageInfo::SetDamageStats( int iDamageCustom )
{
	m_iDamageCustom = iDamageCustom;
}

int CTakeDamageInfo::GetAmmoType() const
{
	return m_iAmmoType;
}

void CTakeDamageInfo::SetAmmoType( int iAmmoType )
{
	m_iAmmoType = iAmmoType;
}

void CTakeDamageInfo::CopyDamageToBaseDamage()
{ 
	m_flBaseDamage = m_flDamage;
}

float CTakeDamageInfo::GetRadius() const
{
	return m_flRadius;
}

void CTakeDamageInfo::SetRadius( float flRadius )
{
	m_flRadius = flRadius;
}

int CTakeDamageInfo::GetObjectsPenetrated() const
{
	return m_iObjectsPenetrated;
}

void CTakeDamageInfo::SetObjectsPenetrated( int iObjectsPenetrated )
{
	m_iObjectsPenetrated = iObjectsPenetrated;
}

// -------------------------------------------------------------------------------------------------- //
// Inlines.
// -------------------------------------------------------------------------------------------------- //
CBaseEntity *CMultiDamage::GetTarget() const
{
	return m_hTarget;
}

void CMultiDamage::SetTarget( CBaseEntity *pTarget )
{
	m_hTarget = pTarget;
}

void CTakeDamageInfo::SetBulletID( uint32 bulletid, uint8 uiRecoilIndex)
{
	m_uiBulletID = bulletid; 
	m_uiRecoilIndex = uiRecoilIndex;
}

#undef DMG_GENERIC
#undef DMG_CRUSH
#undef DMG_BULLET
#undef DMG_SLASH
#undef DMG_BURN
#undef DMG_VEHICLE
#undef DMG_FALL
#undef DMG_BLAST
#undef DMG_CLUB
#undef DMG_SHOCK
#undef DMG_SONIC
#undef DMG_ENERGYBEAM
#undef DMG_PREVENT_PHYSICS_FORCE
#undef DMG_NEVERGIB
#undef DMG_ALWAYSGIB
#undef DMG_DROWN
#undef DMG_PARALYZE
#undef DMG_NERVEGAS
#undef DMG_POISON
#undef DMG_RADIATION
#undef DMG_DROWNRECOVER
#undef DMG_ACID
#undef DMG_SLOWBURN
#undef DMG_REMOVENORAGDOLL
#undef DMG_PHYSGUN
#undef DMG_PLASMA
#undef DMG_AIRBOAT
#undef DMG_DISSOLVE
#undef DMG_BLAST_SURFACE
#undef DMG_DIRECT
#undef DMG_BUCKSHOT
