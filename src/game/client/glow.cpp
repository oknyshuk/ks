// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <igameevents.h>
#include <stdlib.h>
#include <wchar.h>
#include "edict.h"
#include "iclientrenderable.h"
#include "materialsystem/imaterialvar.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "tier2/renderutils.h"
#include "view_shared.h"
#include "materialsystem/imesh.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "worldsize.h"
#include "declare_class.h"
#include "engine/ivmodelinfo.h"
#include "simple_physics.h"
#include "tier2/beamsegdraw.h"
#include "cdll_int.h"
#include "game/client/iviewport.h"
#include "game/client/viewport_panel_names.h"
#include "igameevents.h"
#include "localize/ilocalize.h"
#include "matchmaking/imatchframework.h"
#include "hud_element_helper.h"
#include "shareddefs.h"
#include "netmessages.h"
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
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :baseentity;
import :props;
import :iinput;
import :baseanimatedtextureproxy;
import :sharedvar;
import :util;
import :cstrike15.cs_player;
import :precache;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import :particlemgr;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import :noopanimcontroller;
import ks.core.color;
import ks.tier1.bitbuf;
import ks.tier1.intrusive_list;
import ks.tier1.intrusive_ptr;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.tracked_ref;
import ks.core.vprof;
import ks.game.sun_shared;
import ks.math.vector2d;
import ks.core.random;
import ks.math.ssemath;
import ks.math.vmatrix;
import ks.core.time;
import ks.tier1.icvar;
import ks.core.dbg;
import ks.game.in_buttons;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.fmtstr;
import ks.tier1.playerid;
import ks.tier1.strtools;
import :cstrike15.cs_shareddefs;
import :game_controls.baseviewport;
import :triggers;
import :econ.econ_item_schema_minimal;
import ks.core.memalloc;
import ks.core.threadtools;
import ks.math.pfns;

class CBaseEntity;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: Functionality to render a glowing outline around client renderable objects.

inline constexpr const char *FULL_FRAME_TEXTURE = "_rt_FullFrameFB";

static constexpr auto GLOWBOX_PASS_COLOR = 0;
static constexpr auto GLOWBOX_PASS_STENCIL = 1;
static constexpr auto GLOW_PULSE_DURATION = 0.2f;

ConVar glow_outline_effect_enable( side_registry(), "glow_outline_effect_enable", "1", FCVAR_CHEAT, "Enable entity outline glow effects." );
ConVar glow_outline_effect_width( side_registry(), "glow_outline_width", "6.0f", FCVAR_CHEAT, "Width of glow outline effect in screen space." );

ConVar glow_muzzle_debug( side_registry(), "glow_muzzle_debug", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show muzzle glow shapes outside of the glow pass." );

CGlowObjectManager &GlowObjectManager()
{
	static CGlowObjectManager s_GlowObjectManager;
	return s_GlowObjectManager;
}

void CGlowObjectManager::RenderGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot )
{
	if ( glow_outline_effect_enable.GetBool() )
	{
		CMatRenderContextPtr pRenderContext( materials );

		int nX, nY, nWidth, nHeight;
		pRenderContext->GetViewport( nX, nY, nWidth, nHeight );

		PIXEvent _pixEvent( pRenderContext, "EntityGlowEffects" );
		ApplyEntityGlowEffects( pSetup, nSplitScreenSlot, pRenderContext, glow_outline_effect_width.GetFloat(), nX, nY, nWidth, nHeight );
	}
}

static void SetRenderTargetAndViewPort( ITexture *rt, int w, int h )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget(rt);
	pRenderContext->Viewport(0,0,w,h);
}

void CGlowObjectManager::RenderGlowBoxes( int iPass, CMatRenderContextPtr &pRenderContext )
{
	for ( int n = std::ssize( m_GlowBoxDefinitions ) - 1; n >= 0 ; n-- )
	{
		if ( m_GlowBoxDefinitions[n].m_flTerminationTimeIndex < gpGlobals->curtime )
		{
			ks::swap_remove( m_GlowBoxDefinitions, n );
		}
		else
		{
			float flLifeLeft = (m_GlowBoxDefinitions[n].m_flTerminationTimeIndex - gpGlobals->curtime) / (m_GlowBoxDefinitions[n].m_flTerminationTimeIndex - m_GlowBoxDefinitions[n].m_flBirthTimeIndex);
			if ( flLifeLeft > 0.95 )
				flLifeLeft = (0.05f - ( flLifeLeft - 0.95f )) / 0.05f; //fade in the first 5% of lifetime
			else
				flLifeLeft = std::min( flLifeLeft * 4.0f, 1.0f ); //fade out the last 25% of lifetime

			m_GlowBoxDefinitions[n].m_colColor[3] = flLifeLeft * 255;
			
			if ( iPass == GLOWBOX_PASS_COLOR )
			{
				Vector vecForward;
				AngleVectors( m_GlowBoxDefinitions[n].m_angOrientation, &vecForward );
				Vector vecLineEnd = m_GlowBoxDefinitions[n].m_vPosition + ( vecForward * m_GlowBoxDefinitions[n].m_vMaxs.x );

				RenderLine( m_GlowBoxDefinitions[n].m_vPosition, vecLineEnd, m_GlowBoxDefinitions[n].m_colColor, false );
			}
			else if ( iPass == GLOWBOX_PASS_STENCIL )
			{

				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				RenderBox( m_GlowBoxDefinitions[n].m_vPosition, m_GlowBoxDefinitions[n].m_angOrientation, m_GlowBoxDefinitions[n].m_vMins, m_GlowBoxDefinitions[n].m_vMaxs, m_GlowBoxDefinitions[n].m_colColor, false );
			}			
		}
	}
}

// *** Keep in sync with matsys_interface.cpp, where the texture is declared ***
// Resolution for glow target chosen to be the largest that we can fit in EDRAM after 720p color/depth textures.		
#define GLOW_360_RT_WIDTH ( std::min( 1120, pSetup->width ) )
#define GLOW_360_RT_HEIGHT ( std::min( 624, pSetup->height ) )

void CGlowObjectManager::RenderGlowModels( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, std::vector< GlowObjectDefinition_t > &vecGlowObjects )
{
	//==========================================================================================//
	// This renders solid pixels with the correct coloring for each object that needs the glow.	//
	// After this function returns, this image will then be blurred and added into the frame	//
	// buffer with the objects stenciled out.													//
	//==========================================================================================//
	pRenderContext->PushRenderTargetAndViewport();

	// Save modulation color and blend
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );
	float flOrigBlend = render->GetBlend();

	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );

	{
		SetRenderTargetAndViewPort( pRtFullFrame, pSetup->width, pSetup->height );
	}

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	// Set override material for glow color
	IMaterial *pMatGlowColor = nullptr;

	pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );

	//==================//
	// Draw the objects //
	//==================//
	for ( int i = 0; i < std::ssize( vecGlowObjects ); ++ i )
	{
		if ( vecGlowObjects[i].IsUnused() || !vecGlowObjects[i].ShouldDraw( nSplitScreenSlot ) || vecGlowObjects[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

		if ( vecGlowObjects[i].m_bFullBloomRender )
		{

			// Disabled because stencil test on off-screen buffers doesn't work with MSAA on.
			// Also, the normal model render does not seem to work on the off-screen buffer

			//g_pStudioRender->ForcedMaterialOverride( NULL );

			// 			ShaderStencilState_t stencilState;
			// 			stencilState.m_bEnable = true;
			// 			stencilState.m_nReferenceValue = vecGlowObjects[i].m_nFullBloomStencilTestValue;
			// 			stencilState.m_nTestMask = 0xFF;
			// 			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			// 			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			// 
			// 			pRenderContext->SetStencilState( stencilState );
		}
		else
		{

			// Disabled because stencil test on off-screen buffers doesn't work with MSAA on
			// Most features still work, but some (e.g. partial occlusion) don't
			// 			ShaderStencilState_t stencilState;
			// 			stencilState.m_bEnable = true;
			// 			stencilState.m_nReferenceValue = 1;
			// 			stencilState.m_nTestMask = 0x1;
			// 			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			// 			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			// 			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			// 
			// 			pRenderContext->SetStencilState( stencilState );
		}

		render->SetBlend( vecGlowObjects[i].m_flGlowAlpha );
		Vector vGlowColor = vecGlowObjects[i].m_vGlowColor * vecGlowObjects[i].m_flGlowAlpha;

		// if pulse overdrive is non-zero, add its contribution to render alpha
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive > 0 )
		{
			render->SetBlend( vecGlowObjects[i].m_flGlowAlpha + vecGlowObjects[i].m_flGlowPulseOverdrive );
		}

		// if set, cap glow alpha according to alpha of the non-glowing entity
		float flRenderAlpha = (float)vecGlowObjects[i].m_pEntity->GetRenderAlpha() * 0.00392;
		if ( vecGlowObjects[i].m_bGlowAlphaCappedByRenderAlpha && vecGlowObjects[i].m_flGlowAlpha > flRenderAlpha )
		{
			render->SetBlend( flRenderAlpha );
			vGlowColor *= flRenderAlpha;
		}

		// if set, alpha is multiplied by the ratio of entity velocity over the given maximum (e.g. faster = more opaque)
		if ( vecGlowObjects[i].m_flGlowAlphaFunctionOfMaxVelocity > 0.0f )
		{
			float flVelocityToAlpha = (vecGlowObjects[i].m_pEntity->GetAbsVelocity().Length() / vecGlowObjects[i].m_flGlowAlphaFunctionOfMaxVelocity);
			render->SetBlend( flVelocityToAlpha );
			vGlowColor *= flVelocityToAlpha;
		}

		// if set, cap cumulative glow alpha to a maximum value
		if ( render->GetBlend() > 0 && vecGlowObjects[i].m_flGlowAlphaMax < 1.0f )
		{
			float flCappedAlpha = std::min( render->GetBlend(), vecGlowObjects[i].m_flGlowAlphaMax );
			render->SetBlend( flCappedAlpha );
			vGlowColor *= flCappedAlpha;
		}

		render->SetColorModulation( &vGlowColor[0] ); // This only sets rgb, not alpha

		vecGlowObjects[i].DrawModel();

		// align and render the glow-only muzzle flash model for glowing weapon fire
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive >= 0.25f )
		{
			CCSPlayer* localPlayer = GetLocalOrInEyeCSPlayer();
			CCSPlayer* tempPlayer = ToCSPlayer( vecGlowObjects[i].m_pEntity );
			if ( tempPlayer && localPlayer && (localPlayer->GetAbsOrigin() - tempPlayer->GetAbsOrigin()).Length() > 20 )
			{
				CWeaponCSBase* tempWeapon = tempPlayer->GetActiveCSWeapon();
				if ( tempWeapon )
				{
					//move muzzle flash shape to muzzle location
					if ( tempPlayer->m_hMuzzleFlashShape && !(tempWeapon->HasSilencer() && tempWeapon->IsSilenced()) )
					{
						tempPlayer->m_hMuzzleFlashShape->SetAbsOrigin( tempPlayer->m_vecLastMuzzleFlashPos );
						tempPlayer->m_hMuzzleFlashShape->SetAbsAngles( tempPlayer->m_angLastMuzzleFlashAngle );

						//pick a random flash shape
						//tempPlayer->m_hMuzzleFlashShape->SetBodygroup(0, RandomInt( 0, tempPlayer->m_hMuzzleFlashShape->GetNumBodyGroups()-1 ));

						//unhide and render the muzzle flash shape
						tempPlayer->m_hMuzzleFlashShape->RemoveEffects( EF_NODRAW );
						RenderableInstance_t instance;
						instance.m_nAlpha = (uint8)( vecGlowObjects[i].m_flGlowAlpha * 255.0f );
						tempPlayer->m_hMuzzleFlashShape->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
						if ( glow_muzzle_debug.GetInt() == 0 )
							tempPlayer->m_hMuzzleFlashShape->AddEffects( EF_NODRAW );
					}					
				}
			}
		}

		// dampen overdrive here. Do this at the end, otherwise our framerate may be low enough that we don't see the effect for even one frame
		if ( vecGlowObjects[i].m_flGlowPulseOverdrive > 0 )
		{
			vecGlowObjects[i].m_flGlowPulseOverdrive -= std::max<std::common_type_t<int, float>>(0, ( vecGlowObjects[i].m_flGlowPulseOverdrive * ( gpGlobals->frametime / GLOW_PULSE_DURATION ) ) ); //return to default over 1/5th a second
		}

	}

	RenderGlowBoxes(GLOWBOX_PASS_COLOR, pRenderContext);

	g_pStudioRender->ForcedMaterialOverride( nullptr );
	render->SetColorModulation( vOrigColor.Base() );
	render->SetBlend( flOrigBlend );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	pRenderContext->SetStencilState( stencilStateDisable );

	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::DownSampleAndBlurRT( const CViewSetup *pSetup, CMatRenderContextPtr &pRenderContext, float flBloomScale, ITexture *pRtFullFrame, ITexture *pRtQuarterSize0, ITexture *pRtQuarterSize1 )
{
	static bool s_bFirstPass = true;

	//===================================
	// Setup state for downsample/bloom
	//===================================

	pRenderContext->PushRenderTargetAndViewport();

	// Get viewport
	int nSrcWidth = pSetup->width;
	int nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	// Get material and texture pointers
	IMaterial *pMatDownsample = materials->FindMaterial( "dev/glow_downsample", TEXTURE_GROUP_OTHER, true);
	IMaterial *pMatBlurX = materials->FindMaterial( "dev/glow_blur_x", TEXTURE_GROUP_OTHER, true );
	IMaterial *pMatBlurY = materials->FindMaterial( "dev/glow_blur_y", TEXTURE_GROUP_OTHER, true );

	//============================================
	// Downsample _rt_FullFrameFB to _rt_SmallFB0
	//============================================

	// First clear the full target to black if we're not going to touch every pixel
	if ( ( pRtQuarterSize0->GetActualWidth() != ( pSetup->width / 4 ) ) || ( pRtQuarterSize0->GetActualHeight() != ( pSetup->height / 4 ) ) )
	{
		SetRenderTargetAndViewPort( pRtQuarterSize0, pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight() );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
	}

	// Set the viewport
	SetRenderTargetAndViewPort( pRtQuarterSize0, pSetup->width / 4, pSetup->height / 4 );

	IMaterialVar *pbloomexpvar = pMatDownsample->FindVar( "$bloomexp", 0 );
	if ( pbloomexpvar != nullptr )
	{
		pbloomexpvar->SetFloatValue( 2.5f );
	}

	IMaterialVar *pbloomsaturationvar = pMatDownsample->FindVar( "$bloomsaturation", 0 );
	if ( pbloomsaturationvar != nullptr )
	{
		pbloomsaturationvar->SetFloatValue( 1.0f );
	}

	// note the -2's below. Thats because we are downsampling on each axis and the shader
	// accesses pixels on both sides of the source coord
	int nFullFbWidth = nSrcWidth;
	int nFullFbHeight = nSrcHeight;
	pRenderContext->DrawScreenSpaceRectangle( pMatDownsample, 0, 0, nSrcWidth/4, nSrcHeight/4,
		0, 0, nFullFbWidth - 4, nFullFbHeight - 4,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight() );

	//============================//
	// Guassian blur x rt0 to rt1 //
	//============================//

	// First clear the full target to black if we're not going to touch every pixel
	if ( s_bFirstPass || ( pRtQuarterSize1->GetActualWidth() != ( pSetup->width / 4 ) ) || ( pRtQuarterSize1->GetActualHeight() != ( pSetup->height / 4 ) ) )
	{
		// On the first render, this viewport may require clearing
		s_bFirstPass = false;
		SetRenderTargetAndViewPort( pRtQuarterSize1, pRtQuarterSize1->GetActualWidth(), pRtQuarterSize1->GetActualHeight() );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
	}

	// Set the viewport
	SetRenderTargetAndViewPort( pRtQuarterSize1, pSetup->width / 4, pSetup->height / 4  );

	pRenderContext->DrawScreenSpaceRectangle( pMatBlurX, 0, 0, nSrcWidth/4, nSrcHeight/4,
		0, 0, nSrcWidth/4-1, nSrcHeight/4-1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight() );

	//============================//
	// Gaussian blur y rt1 to rt0 //
	//============================//
	SetRenderTargetAndViewPort( pRtQuarterSize0, pSetup->width / 4, pSetup->height / 4 );
	IMaterialVar *pBloomAmountVar = pMatBlurY->FindVar( "$bloomamount", nullptr );
	pBloomAmountVar->SetFloatValue( flBloomScale );
	pRenderContext->DrawScreenSpaceRectangle( pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize1->GetActualWidth(), pRtQuarterSize1->GetActualHeight() );

	// Pop RT
	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::ApplyEntityGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, float flBloomScale, int x, int y, int w, int h )
{
	
	// gather up special glow styles
	
	std::vector< GlowObjectDefinition_t > vecGlowObjectsRimGlow3DStyle;
	vecGlowObjectsRimGlow3DStyle.clear();
	
	std::vector< GlowObjectDefinition_t > vecGlowObjectsEdgeHighlightStyle;
	vecGlowObjectsEdgeHighlightStyle.clear();
	
	for ( auto &glowObject : m_GlowObjectDefinitions | std::views::reverse )
	{
		if ( !glowObject.IsUnused() && glowObject.ShouldDraw( nSplitScreenSlot ) )
		{
			if ( glowObject.m_nRenderStyle == GLOWRENDERSTYLE_RIMGLOW3D )
			{
				vecGlowObjectsRimGlow3DStyle.push_back( glowObject );
			}
			else if ( glowObject.m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT || glowObject.m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT_PULSE )
			{
				vecGlowObjectsEdgeHighlightStyle.push_back( glowObject );
			}
		}
	}

	if ( !vecGlowObjectsRimGlow3DStyle.empty() )
	{
		//todo: expose pulse width and frequency parameters
		float flPulse = 0.5f + 0.5f * std::sin( gpGlobals->curtime * 12.0f );

		IMaterial *pMatRim = materials->FindMaterial( "dev/glow_rim3d", TEXTURE_GROUP_OTHER, true );
		g_pStudioRender->ForcedMaterialOverride( pMatRim );

		for ( int i = 0; i < std::ssize( vecGlowObjectsRimGlow3DStyle ); ++ i )
		{
			if ( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha <= 0 )
				continue;

			IMaterialVar *pMatVar = pMatRim->FindVar( "$envmaptint", 0 );
			if ( pMatVar != nullptr )
			{
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.x, 0, 1 ), 0 );
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.y, 0, 1 ), 1 );
				pMatVar->SetVecComponentValue( clamp( vecGlowObjectsRimGlow3DStyle[i].m_flGlowAlpha * vecGlowObjectsRimGlow3DStyle[i].m_vGlowColor.z, 0, 1 ), 2 );
			}

			pMatVar = pMatRim->FindVar( "$envmapfresnelminmaxexp", 0 );
			if ( pMatVar != nullptr )
			{
				pMatVar->SetVecComponentValue( 0, 0 );
				pMatVar->SetVecComponentValue( 1.5f, 1 );
				pMatVar->SetVecComponentValue( 3.0f + flPulse * 2.0f, 2 );
			}

			vecGlowObjectsRimGlow3DStyle[i].DrawModel();
		}

		g_pStudioRender->ForcedMaterialOverride( nullptr );
	}
	
	if ( !vecGlowObjectsEdgeHighlightStyle.empty() )
	{
		// render players into the fullscreen rt all white
		// render in-world 3d imposter players into scene using screenspace UV lookup from fullscreen RT, use glow object alpha and colorize here.
		// in the above step, multisample the rt to edge detect.

		// push the rt
		pRenderContext->PushRenderTargetAndViewport();

		// remember the color modulation so we can put it back again at the end
		Vector vOrigColor;
		render->GetColorModulation( vOrigColor.Base() );

		ITexture *pRtOutput = materials->FindTexture( "_rt_FullScreen", TEXTURE_GROUP_RENDER_TARGET );
		SetRenderTargetAndViewPort( pRtOutput, pSetup->width, pSetup->height );

		pRenderContext->ClearColor3ub( 0, 0, 0 );	
		pRenderContext->ClearBuffers( true, true, true );

		// Set override material for drawing the shapes
		IMaterial *pMatTeamIdShape = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
		g_pStudioRender->ForcedMaterialOverride( pMatTeamIdShape );
	
		// we want to render fully opaque, so bash blend to 1
		render->SetBlend( 1 );

		// set override color
		Vector vecOverrideColor = Vector( 1, 1, 1 );
		render->SetColorModulation( vecOverrideColor.Base() );

		// draw players
		for ( auto &glowObject : vecGlowObjectsEdgeHighlightStyle )
		{		
			glowObject.DrawModel();
		}

		g_pStudioRender->ForcedMaterialOverride( nullptr );

		pRenderContext->PopRenderTargetAndViewport();

		IMaterial *pMatTemp = materials->FindMaterial( "dev/glow_edge_highlight", TEXTURE_GROUP_OTHER, false );
		g_pStudioRender->ForcedMaterialOverride( pMatTemp );

		// draw players
		for ( auto &glowObject : vecGlowObjectsEdgeHighlightStyle )
		{
			Vector vecTempColor = glowObject.m_vGlowColor * clamp( glowObject.m_flGlowAlpha, 0.0f, 1.0f ) * 1.4f; // boost a bit

			if ( glowObject.m_nRenderStyle == GLOWRENDERSTYLE_EDGE_HIGHLIGHT_PULSE )
			{
				float flPulse = 1.5f + 0.5f * std::sin( gpGlobals->curtime * 16.0f );
				vecTempColor *= (flPulse * 0.5f);
			}

			render->SetColorModulation( vecTempColor.Base() );
			glowObject.DrawModel();
		}

		// restore the modulation from before
		render->SetColorModulation( vOrigColor.Base() );
		
	}

	//=======================================================//
	// Render objects into stencil buffer					 //
	//=======================================================//

	// Set override shader to the same simple shader we use to render the glow models
	IMaterial *pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	float flSavedBlend = render->GetBlend();

	// Set alpha to 0 so we don't touch any color pixels
	render->SetBlend( 0.0f );
	pRenderContext->OverrideDepthEnable( true, false );

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;

	int iNumGlowObjects = 0;

	for ( int i = 0; i < std::ssize( m_GlowObjectDefinitions ); ++ i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		// Full bloom rendered objects should not be stenciled out here
		if ( m_GlowObjectDefinitions[i].m_bFullBloomRender )
		{
			++ iNumGlowObjects;
			continue;
		}

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded || m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 1;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
			else if ( m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
			{
				ShaderStencilState_t stencilState;
				stencilState.m_bEnable = true;
				stencilState.m_nReferenceValue = 2;
				stencilState.m_nTestMask = 0x1;
				stencilState.m_nWriteMask = 0x3;
				stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
				stencilState.m_PassOp = SHADER_STENCILOP_INCREMENT_CLAMP;
				stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
				stencilState.m_ZFailOp = SHADER_STENCILOP_SET_TO_REFERENCE;

				pRenderContext->SetStencilState( stencilState );

				m_GlowObjectDefinitions[i].DrawModel();
			}
		}

		iNumGlowObjects++;
	}

	int iTempHealthBarRenderMaskIndex = 4;

	// Need to do a 2nd pass to warm stencil for objects which are rendered only when occluded
	for ( int i = 0; i < std::ssize( m_GlowObjectDefinitions ); ++ i )
	{
		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		// Full bloom rendered objects should not be stenciled out here
		if ( m_GlowObjectDefinitions[i].m_bFullBloomRender )
			continue;

		if ( m_GlowObjectDefinitions[i].m_bRenderWhenOccluded && !m_GlowObjectDefinitions[i].m_bRenderWhenUnoccluded )
		{
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nReferenceValue = 2;

			CCSPlayer* pPlayer = ToCSPlayer( m_GlowObjectDefinitions[i].m_pEntity );
			if ( pPlayer )
			{
				pPlayer->m_iHealthBarRenderMaskIndex = iTempHealthBarRenderMaskIndex;
				stencilState.m_nReferenceValue = iTempHealthBarRenderMaskIndex;
				iTempHealthBarRenderMaskIndex ++;
			}

			stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
			stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			pRenderContext->SetStencilState( stencilState );

			m_GlowObjectDefinitions[i].DrawModel();
		}
	}

	RenderGlowBoxes(GLOWBOX_PASS_STENCIL, pRenderContext);
	iNumGlowObjects += std::ssize( m_GlowBoxDefinitions );

	g_pStudioRender->ForcedMaterialOverride( nullptr );
	render->SetBlend( 0.0f );

	pRenderContext->OverrideDepthEnable( true, false, false ); // health bars render over everything

	IMaterial *pMatGlowHealthColor = nullptr;
	pMatGlowHealthColor = materials->FindMaterial( "dev/glow_health_color", TEXTURE_GROUP_OTHER, true );

	for ( int i = 0; i < std::ssize( m_GlowObjectDefinitions ); ++ i )
	{

		if ( m_GlowObjectDefinitions[i].IsUnused() || !m_GlowObjectDefinitions[i].ShouldDraw( nSplitScreenSlot ) || m_GlowObjectDefinitions[i].m_nRenderStyle != GLOWRENDERSTYLE_DEFAULT )
			continue;

		CCSPlayer* pPlayer = ToCSPlayer( m_GlowObjectDefinitions[i].m_pEntity );
		if ( pPlayer && pPlayer != GetLocalOrInEyeCSPlayer() && pPlayer->IsAlive() )
		{
			
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nWriteMask = 0x0;
			stencilState.m_nTestMask = 0xFFFFFF;
			stencilState.m_nReferenceValue = pPlayer->m_iHealthBarRenderMaskIndex;
			stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
			stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
			stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
			stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
			pRenderContext->SetStencilState( stencilState );

			if ( pPlayer->m_flHealthFadeAlpha > 0 )
				pPlayer->m_flHealthFadeAlpha -= gpGlobals->frametime * 0.4f;

			if ( pPlayer->m_flHealthFadeValue != pPlayer->GetHealth() )
				pPlayer->m_flHealthFadeAlpha = 1.0f;

			pPlayer->m_flHealthFadeValue = (float)pPlayer->GetHealth();

			if ( pPlayer->m_flHealthFadeAlpha > 0 ) //only need to update the effect if we can see it
			{
				float flGlowPulseSpeed = Lerp( pPlayer->m_flHealthFadeValue/100.0f, 30.0f, 10.0f );
				pMatGlowHealthColor->AlphaModulate( pPlayer->m_flHealthFadeAlpha * (0.4*(std::sin(flGlowPulseSpeed*gpGlobals->curtime)+1.4)) );

				Vector vecPlayerScreenSpaceOrigin;
				if ( ScreenTransform( pPlayer->GetAbsOrigin(), vecPlayerScreenSpaceOrigin ) == 0 )
				{

					ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceOrigin);

				float flHealthLeft = (100.0f-pPlayer->m_flHealthFadeValue)/100.0f;
					float flHealthHeightOffset = 72.0f;

				if ( pPlayer->GetFlags() & FL_DUCKING )
						flHealthHeightOffset = 55.0f;

					Vector vecPlayerScreenSpaceHealthPos;
					if ( ScreenTransform( pPlayer->GetAbsOrigin() + Vector (0,0,flHealthLeft*flHealthHeightOffset), vecPlayerScreenSpaceHealthPos ) == 0 )
				{

						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceHealthPos);

						Vector vecPlayerScreenSpaceSizeA;
						ScreenTransform( pPlayer->GetAbsOrigin() + Vector(0,0,100), vecPlayerScreenSpaceSizeA );
						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceSizeA);

						Vector vecPlayerScreenSpaceSizeB;
						ScreenTransform( pPlayer->GetAbsOrigin() + Vector(100,0,0), vecPlayerScreenSpaceSizeB );
						ConvertNormalizedScreenSpaceToPixelScreenSpace(vecPlayerScreenSpaceSizeB);

						Vector vecPlayerScreenSpaceSizeAPixels;
						VectorSubtract( vecPlayerScreenSpaceOrigin, vecPlayerScreenSpaceSizeA, vecPlayerScreenSpaceSizeAPixels );
						Vector vecPlayerScreenSpaceSizeBPixels;
						VectorSubtract( vecPlayerScreenSpaceOrigin, vecPlayerScreenSpaceSizeB, vecPlayerScreenSpaceSizeBPixels );

						float flPlayerScreenSpaceCoverage = Max( vecPlayerScreenSpaceSizeAPixels.Length(), vecPlayerScreenSpaceSizeBPixels.Length() );

						if ( flPlayerScreenSpaceCoverage < ScreenHeight() * 2 )
						{
							float flHealthWidth = flPlayerScreenSpaceCoverage;
							float flHealthHeight = flPlayerScreenSpaceCoverage;

							float flHealthPosX = vecPlayerScreenSpaceHealthPos.x - (flPlayerScreenSpaceCoverage * 0.5);
							float flHealthPosY = vecPlayerScreenSpaceHealthPos.y;
							
							pRenderContext->DrawScreenSpaceRectangle( pMatGlowHealthColor, 
								flHealthPosX, flHealthPosY,
								flHealthWidth, flHealthHeight,
								0, 0, 0, 0, 1, 1 );

						}

					}

				}

			}

		}

	}

	//clear out custom render settings
	pRenderContext->OverrideDepthEnable( false, false );
	render->SetBlend( flSavedBlend );
	pRenderContext->SetStencilState( stencilStateDisable );

	// If there aren't any objects to glow, don't do all this other stuff
	// this fixes a bug where if there are glow objects in the list, but none of them are glowing,
	// the whole screen blooms.
	if ( iNumGlowObjects <= 0 )
		return;

	//=============================================
	// Render the glow colors to _rt_FullFrameFB 
	//=============================================
	{
		PIXEvent pixEvent( pRenderContext, "RenderGlowModels" );
		RenderGlowModels( pSetup, nSplitScreenSlot, pRenderContext, m_GlowObjectDefinitions );
	}

	ITexture *pRtFullFrame = materials->FindTexture( FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtQuarterSize0 = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtQuarterSize1 = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
	
	DownSampleAndBlurRT( pSetup, pRenderContext, flBloomScale, pRtFullFrame, pRtQuarterSize0, pRtQuarterSize1 );

	{
		//=======================================================================================================//
		// At this point, pRtQuarterSize0 is filled with the fully colored glow around everything as solid glowy //
		// blobs. Now we need to stencil out the original objects by only writing pixels that have no            //
		// stencil bits set in the range we care about.                                                          //
		//=======================================================================================================//
		IMaterial *pMatHaloAddToScreen = materials->FindMaterial( "dev/halo_add_to_screen", TEXTURE_GROUP_OTHER, true );

		// Do not fade the glows out at all (weight = 1.0)
		IMaterialVar *pDimVar = pMatHaloAddToScreen->FindVar( "$C0_X", nullptr );
		pDimVar->SetFloatValue( 1.0f );

		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = true;
		stencilState.m_nWriteMask = 0x0; // We're not changing stencil
		//stencilState.m_nTestMask = 0x3;
		stencilState.m_nReferenceValue = 0x0;
		stencilState.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		stencilState.m_PassOp = SHADER_STENCILOP_KEEP;
		stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;
		pRenderContext->SetStencilState( stencilState );

		// Get viewport
		int nSrcWidth = pSetup->width;
		int nSrcHeight = pSetup->height;
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

		// Draw quad
		pRenderContext->DrawScreenSpaceRectangle( pMatHaloAddToScreen, 0, 0, nViewportWidth, nViewportHeight,
			0.0f, -0.5f, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
			pRtQuarterSize1->GetActualWidth(),
			pRtQuarterSize1->GetActualHeight() );

		// Disable stencil
		pRenderContext->SetStencilState( stencilStateDisable );
	}

}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	RenderableInstance_t instance;
	instance.m_nAlpha = (uint8)( m_flGlowAlpha * 255.0f );

	m_pEntity->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
	CBaseEntity *pAttachment = m_pEntity->FirstMoveChild();

	while ( pAttachment != nullptr )
	{
		if ( pAttachment->ShouldDraw() )
		{
			pAttachment->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
		}
		pAttachment = pAttachment->NextMovePeer();
	}
}

int CGlowObjectManager::RegisterGlowObject( CBaseEntity *pEntity, const Vector &vGlowColor, float flGlowAlpha, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded, int nSplitScreenSlot)
{
 int nIndex;
 if ( m_nFirstFreeSlot == GlowObjectDefinition_t::END_OF_FREE_LIST )
 {
  nIndex = std::ssize( m_GlowObjectDefinitions ); m_GlowObjectDefinitions.emplace_back();
 }
 else
 {
  nIndex = m_nFirstFreeSlot;
  m_nFirstFreeSlot = m_GlowObjectDefinitions[nIndex].m_nNextFreeSlot;
 }

 m_GlowObjectDefinitions[nIndex].m_flGlowPulseOverdrive = 0.0f;
 m_GlowObjectDefinitions[nIndex].m_bGlowAlphaCappedByRenderAlpha = false;
 m_GlowObjectDefinitions[nIndex].m_flGlowAlphaFunctionOfMaxVelocity = 0.0f;
 m_GlowObjectDefinitions[nIndex].m_flGlowAlphaMax = 1.0f;
 m_GlowObjectDefinitions[nIndex].m_pEntity = pEntity;
 m_GlowObjectDefinitions[nIndex].m_vGlowColor = vGlowColor;
 m_GlowObjectDefinitions[nIndex].m_flGlowAlpha = flGlowAlpha;
 m_GlowObjectDefinitions[nIndex].m_bRenderWhenOccluded = bRenderWhenOccluded;
 m_GlowObjectDefinitions[nIndex].m_bRenderWhenUnoccluded = bRenderWhenUnoccluded;
 m_GlowObjectDefinitions[nIndex].m_bFullBloomRender = false;
 m_GlowObjectDefinitions[nIndex].m_nFullBloomStencilTestValue = 0;
 m_GlowObjectDefinitions[nIndex].m_nSplitScreenSlot = nSplitScreenSlot;
 m_GlowObjectDefinitions[nIndex].m_nNextFreeSlot = GlowObjectDefinition_t::ENTRY_IN_USE;
 m_GlowObjectDefinitions[nIndex].m_nRenderStyle = GLOWRENDERSTYLE_DEFAULT;

 return nIndex;
}

int CGlowObjectManager::AddGlowBox( Vector vecOrigin, QAngle angOrientation, Vector mins, Vector maxs, Color colColor, float flLifetime)
{
 int nIndex = std::ssize( m_GlowBoxDefinitions ); m_GlowBoxDefinitions.emplace_back();
 m_GlowBoxDefinitions[nIndex].m_vPosition = vecOrigin;
 m_GlowBoxDefinitions[nIndex].m_angOrientation = angOrientation;
 m_GlowBoxDefinitions[nIndex].m_vMins = mins;
 m_GlowBoxDefinitions[nIndex].m_vMaxs = maxs;
 m_GlowBoxDefinitions[nIndex].m_colColor = colColor;
 m_GlowBoxDefinitions[nIndex].m_flBirthTimeIndex = gpGlobals->curtime;
 m_GlowBoxDefinitions[nIndex].m_flTerminationTimeIndex = gpGlobals->curtime + flLifetime;
 return nIndex;
}

void CGlowObjectManager::UnregisterGlowObject( int nGlowObjectHandle)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );

 m_GlowObjectDefinitions[nGlowObjectHandle].m_nNextFreeSlot = m_nFirstFreeSlot;
 m_GlowObjectDefinitions[nGlowObjectHandle].m_pEntity = nullptr;
 m_nFirstFreeSlot = nGlowObjectHandle;
}

void CGlowObjectManager::UnregisterGlowObject( CBaseEntity *pEntity)
{
 for ( int i=0; i<std::ssize( m_GlowObjectDefinitions ); i++ )
 {
  if ( m_GlowObjectDefinitions[i].m_pEntity == pEntity )
  {
	  UnregisterGlowObject(i);
	  return;
  }
 }
}

void CGlowObjectManager::SetGlowAlphaPulseOverdrive( int nGlowObjectHandle, float flAdditionalAlpha)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_flGlowPulseOverdrive = flAdditionalAlpha;
}

void CGlowObjectManager::SetGlowAlphaCappedByRenderAlpha( int nGlowObjectHandle, bool bGlowAlphaCappedByRenderAlpha)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_bGlowAlphaCappedByRenderAlpha = bGlowAlphaCappedByRenderAlpha;
}

void CGlowObjectManager::SetGlowAlphaFunctionOfMaxVelocity( int nGlowObjectHandle, float flMaxVelocity)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_flGlowAlphaFunctionOfMaxVelocity = flMaxVelocity;
}

void CGlowObjectManager::SetGlowAlphaMax( int nGlowObjectHandle, float flGlowAlphaMax)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_flGlowAlphaMax = flGlowAlphaMax;
}

void CGlowObjectManager::SetEntity( int nGlowObjectHandle, CBaseEntity *pEntity)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_pEntity = pEntity;
}

void CGlowObjectManager::SetAlpha( int nGlowObjectHandle, float flAlpha)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_flGlowAlpha = flAlpha;
}

void CGlowObjectManager::SetRenderFlags( int nGlowObjectHandle, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_bRenderWhenOccluded = bRenderWhenOccluded;
 m_GlowObjectDefinitions[nGlowObjectHandle].m_bRenderWhenUnoccluded = bRenderWhenUnoccluded;
}

void CGlowObjectManager::SetRenderStyle( int nGlowObjectHandle, GlowRenderStyle_t nNewRenderStyle)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 contract_assert( nNewRenderStyle >= GLOWRENDERSTYLE_DEFAULT && nNewRenderStyle < GLOWRENDERSTYLE_COUNT );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_nRenderStyle = nNewRenderStyle;
}

void CGlowObjectManager::SetFullBloomRender( int nGlowObjectHandle, bool bFullBloomRender, int nStencilTestValue)
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 m_GlowObjectDefinitions[nGlowObjectHandle].m_bFullBloomRender = bFullBloomRender;
 m_GlowObjectDefinitions[nGlowObjectHandle].m_nFullBloomStencilTestValue = nStencilTestValue;
}

bool CGlowObjectManager::IsRenderingWhenOccluded( int nGlowObjectHandle) const
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 return m_GlowObjectDefinitions[nGlowObjectHandle].m_bRenderWhenOccluded;
}

bool CGlowObjectManager::IsRenderingWhenUnoccluded( int nGlowObjectHandle) const
{
 contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
 return m_GlowObjectDefinitions[nGlowObjectHandle].m_bRenderWhenUnoccluded;
}

void CGlowObject::SetGlowAlphaPulseOverdrive( float flAdditionalAlpha)
{
	//this value is added to existing glow alpha, and fades away in a few frames
	GlowObjectManager().SetGlowAlphaPulseOverdrive( m_nGlowObjectHandle, flAdditionalAlpha );
}

#undef GLOW_360_RT_WIDTH
#undef GLOW_360_RT_HEIGHT

class CBaseEntity;

using namespace ::ks::reflect;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectGlow )
PRECACHE( MATERIAL, "sun/overlay" )
PRECACHE( MATERIAL, "sprites/light_glow02_add_noz" )
PRECACHE_REGISTER_END()

class CGlowOverlaySystem : public CAutoGameSystem
{
public:
	CGlowOverlaySystem() : CAutoGameSystem( "CGlowOverlaySystem" )
	{
	}
	// Level init, shutdown
	virtual void LevelInitPreEntity() {}

	virtual void LevelShutdownPostEntity()
	{
		// each overlay's destructor unlinks itself
		while ( !m_GlowOverlays.empty() )
		{
			delete m_GlowOverlays.front();
		}
	}

	std::list<CGlowOverlay*>::iterator AddToOverlayList( CGlowOverlay *pGlow )
	{
		return m_GlowOverlays.insert( m_GlowOverlays.end(), pGlow );
	}
	void RemoveFromOverlayList( std::list<CGlowOverlay*>::iterator it )
	{
		m_GlowOverlays.erase( it );
	}
	std::list<CGlowOverlay*> m_GlowOverlays;
};
CGlowOverlaySystem g_GlowOverlaySystem;

ConVar cl_ShowSunVectors( side_registry(), "cl_ShowSunVectors", "0", 0 );

ConVar cl_sun_decay_rate( side_registry(), "cl_sun_decay_rate", "0.05", FCVAR_CHEAT );

// Dot product space the overlays are drawn in.
// Here it's setup to allow you to see it if you're looking within 40 degrees of the source.
float g_flOverlayRange = std::cos( DEG2RAD( 40 ) );

// ----------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------- //

void Do2DRotation( Vector vIn, Vector &vOut, float flDegrees, int i1, int i2, int i3 )
{
	float c, s;
	SinCos( DEG2RAD( flDegrees ), &s, &c );

	vOut[i1] = vIn[i1]*c - vIn[i2]*s;
	vOut[i2] = vIn[i1]*s + vIn[i2]*c;
	vOut[i3] = vIn[i3];
}

// ----------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------- //

CGlowOverlay::CGlowOverlay()
{
	m_nSprites = 0;
	m_flGlowObstructionScale = 0.0f;
	m_bDirectional = false;
	m_bInSky = false;
	m_skyObstructionScale = 1.0f;
	m_bActivated = false;

	m_flProxyRadius = 2.0f;

	m_queryHandle = 0;

	m_flHDRColorScale = 1.0f;

	//Init our sprites
	for ( int i = 0; i < MAX_SUN_LAYERS; i++ )
	{
		m_Sprites[i].m_vColor.Init();
		m_Sprites[i].m_flHorzSize	= 1.0f;
		m_Sprites[i].m_flVertSize	= 1.0f;
		m_Sprites[i].m_pMaterial	= nullptr;
	}

}

CGlowOverlay::~CGlowOverlay()
{
	if ( m_ListIndex )
	{
		g_GlowOverlaySystem.RemoveFromOverlayList( *m_ListIndex );
	}
}

bool CGlowOverlay::Update()
{
	return true;
}

void BuildingCubemapsChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	CBaseEntity::UpdateVisibilityAllEntities();
}

ConVar building_cubemaps( side_registry(), "building_cubemaps", "0", FCVAR_CHEAT, "Indicates we're building cubemaps", BuildingCubemapsChanged );

float CGlowOverlay::CalcGlowAspect()
{
	if ( m_nSprites )
	{
		if ( m_Sprites[0].m_flHorzSize != 0 && m_Sprites[0].m_flVertSize != 0 )
			return m_Sprites[0].m_flHorzSize / m_Sprites[0].m_flVertSize;
	}
	return 1.0f;
}

void CGlowOverlay::UpdateSkyGlowObstruction( float zFar, bool bCacheFullSceneState )
{
	contract_assert( m_bInSky );

	// If we already cached the sky obstruction and are still using that, early-out
	if ( bCacheFullSceneState && m_bCacheSkyObstruction )
		return;

	// Turning on sky obstruction caching mode
	if ( bCacheFullSceneState && !m_bCacheSkyObstruction )	
	{
		m_bCacheSkyObstruction = true;
	}

	// Turning off sky obstruction caching mode
	if ( !bCacheFullSceneState && m_bCacheSkyObstruction )
	{
		m_bCacheSkyObstruction = false;
	}

	if ( PixelVisibility_IsAvailable() )
	{
		// Trace a ray at the object. 
		Vector pos = CurrentViewOrigin() + m_vDirection * zFar * 0.99f;

		// UNDONE: Can probably do only the pixelvis query in this case if you can figure out where
		// to put it - or save the position of this trace
		pixelvis_queryparams_t params;
		params.Init( pos, m_flProxyRadius );
		params.bSizeInScreenspace = true;
		m_skyObstructionScale = PixelVisibility_FractionVisible( params, &m_queryHandle );
		return;
	}
	// Trace a ray at the object.
	trace_t trace;
	UTIL_TraceLine( CurrentViewOrigin(), CurrentViewOrigin() + (m_vDirection*MAX_TRACE_LENGTH), 
		CONTENTS_SOLID, nullptr, COLLISION_GROUP_NONE, &trace );
	
	// back the trace with a pixel query to occlude with models
	if ( trace.surface.flags & SURF_SKY )
	{
		m_skyObstructionScale = 1.0f;
	}
	else
	{
		m_skyObstructionScale = 0.0f;
	}
}

void CGlowOverlay::UpdateGlowObstruction( const Vector &vToGlow, bool bCacheFullSceneState )
{
	// If we already cached the glow obstruction and are still using that, early-out
	if ( bCacheFullSceneState && m_bCacheGlowObstruction )
		return;
	
	if ( bCacheFullSceneState && !m_bCacheGlowObstruction )	// If turning on sky obstruction caching mode
	{
		m_bCacheGlowObstruction = true;
	}

	if ( !bCacheFullSceneState && m_bCacheGlowObstruction )
	{
		m_bCacheGlowObstruction = false;
	}

	if ( PixelVisibility_IsAvailable() )
	{
		if ( m_bInSky )
		{
			const CViewSetup *pViewSetup = view->GetViewSetup();
			Vector pos = CurrentViewOrigin() + m_vDirection * (pViewSetup->zFar * 0.999f);
			pixelvis_queryparams_t params;
			params.Init( pos, m_flProxyRadius, CalcGlowAspect() );
			params.bSizeInScreenspace = true;
			// use a pixel query to occlude with models
			m_flGlowObstructionScale = PixelVisibility_FractionVisible( params, &m_queryHandle ) * m_skyObstructionScale;
		}
		else
		{
			// If it's not in the sky, then we need a valid position or else we don't
			// know what's in front of it.
			contract_assert( !m_bDirectional );

			pixelvis_queryparams_t params;
			params.Init( m_vPos, m_flProxyRadius, CalcGlowAspect() );

			m_flGlowObstructionScale = PixelVisibility_FractionVisible( params, &m_queryHandle );
		}
		return;
	}

	bool bFade = false;
	if ( m_bInSky )
	{
		// Trace a ray at the object.
		trace_t trace;
		UTIL_TraceLine( CurrentViewOrigin(), CurrentViewOrigin() + (vToGlow*MAX_TRACE_LENGTH), 
			CONTENTS_SOLID, nullptr, COLLISION_GROUP_NONE, &trace );
		
		bFade = (trace.fraction < 1 && !(trace.surface.flags & SURF_SKY));
	}
	else
	{
		// If it's not in the sky, then we need a valid position or else we don't
		// know what's in front of it.
		contract_assert( !m_bDirectional );

		pixelvis_queryparams_t params;
		params.Init( m_vPos, m_flProxyRadius );

		bFade = PixelVisibility_FractionVisible( params, &m_queryHandle ) < 1.0f ? true : false;

	}

	if ( bFade )
	{
		if ( building_cubemaps.GetBool() )
		{
			m_flGlowObstructionScale = 0.0f;
		}
		else
		{
			m_flGlowObstructionScale -= gpGlobals->frametime / cl_sun_decay_rate.GetFloat();
			m_flGlowObstructionScale = std::max( m_flGlowObstructionScale, 0.0f );
		}
	}
	else
	{
		if ( building_cubemaps.GetBool() )
		{
			m_flGlowObstructionScale = 1.0f;
		}
		else
		{
			m_flGlowObstructionScale += gpGlobals->frametime / cl_sun_decay_rate.GetFloat();
			m_flGlowObstructionScale = std::min( m_flGlowObstructionScale, 1.0f );
		}
	}
}

void CGlowOverlay::CalcSpriteColorAndSize( 
	float flDot,
	CGlowSprite *pSprite, 
	float *flHorzSize, 
	float *flVertSize, 
	Vector *vColor )
{
	// The overlay is largest and completely translucent at g_flOverlayRange.
	// When the dot product is 1, then it's smaller and more opaque.
	const float flSizeAtOverlayRangeMul = 150;
	const float flSizeAtOneMul = 70;
	
	const float flOpacityAtOverlayRange = 0;
	const float flOpacityAtOne = 1;

	// Figure out how big and how opaque it will be.
	*flHorzSize = RemapValClamped( 
		flDot, 
		g_flOverlayRange, 
		1, 
		flSizeAtOverlayRangeMul * pSprite->m_flHorzSize, 
		flSizeAtOneMul * pSprite->m_flHorzSize );		

	*flVertSize = RemapValClamped( 
		flDot, 
		g_flOverlayRange, 
		1, 
		flSizeAtOverlayRangeMul * pSprite->m_flVertSize, 
		flSizeAtOneMul * pSprite->m_flVertSize );		
	
	float flOpacity = RemapValClamped( 
		flDot, 
		g_flOverlayRange, 
		1, 
		flOpacityAtOverlayRange, 
		flOpacityAtOne );		

	flOpacity = flOpacity * m_flGlowObstructionScale;
	*vColor = pSprite->m_vColor * flOpacity;
}

void CGlowOverlay::CalcBasis( 
	const Vector &vToGlow,
	float flHorzSize,
	float flVertSize,
	Vector &vBasePt,
	Vector &vUp,
	Vector &vRight )
{
	const float flOverlayDist = 100;	
	vBasePt = CurrentViewOrigin() + vToGlow * flOverlayDist;
	
	vUp.Init( 0, 0, 1 );
	
	vRight = vToGlow.Cross( vUp );
	VectorNormalize( vRight );

	vUp = vRight.Cross( vToGlow );
	VectorNormalize( vUp );

	vRight *= flHorzSize;
	vUp *= flVertSize;
}

extern ConVar	r_drawsprites;
void CGlowOverlay::Draw( bool bCacheFullSceneState )
{
	if( !r_drawsprites.GetBool() )
		return;
	
	// Get the vector to the sun.
	Vector vToGlow;
	
	if( m_bDirectional )
		vToGlow = m_vDirection;
	else
		vToGlow = m_vPos - CurrentViewOrigin();

	VectorNormalize( vToGlow );

	float flDot = vToGlow.Dot( CurrentViewForward() );

	UpdateGlowObstruction( vToGlow, bCacheFullSceneState );
	if( m_flGlowObstructionScale == 0 )
		return;
	
	bool bWireframe = ShouldDrawInWireFrameMode() || (r_drawsprites.GetInt() == 2);
	
	CMatRenderContextPtr pRenderContext( materials );

	for( int iSprite=0; iSprite < m_nSprites; iSprite++ )
	{
		CGlowSprite *pSprite = &m_Sprites[iSprite];
 
		// Figure out the color and size to draw it.
		float flHorzSize, flVertSize;
		Vector vColor;
		CalcSpriteColorAndSize( flDot, pSprite, &flHorzSize, &flVertSize, &vColor );
	
		// If we're alpha'd out, then don't bother
		if ( vColor.LengthSqr() < 0.00001f )
			continue;
		
		// Setup the basis to draw the sprite.
		Vector vBasePt, vUp, vRight;
		CalcBasis( vToGlow, flHorzSize, flVertSize, vBasePt, vUp, vRight );

		//Get our diagonal radius
		float radius = (vRight+vUp).Length();
		if ( R_CullSphere( view->GetFrustum(), 5, &vBasePt, radius ) )
			continue;

		// Get our material (deferred default load)
		if ( m_Sprites[iSprite].m_pMaterial == nullptr )
		{
			m_Sprites[iSprite].m_pMaterial = materials->FindMaterial( "sprites/light_glow02_add_noz", TEXTURE_GROUP_CLIENT_EFFECTS );
		}

		contract_assert( m_Sprites[iSprite].m_pMaterial );
		static unsigned int		nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = m_Sprites[iSprite].m_pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( m_flHDRColorScale );
		}

		// Draw the sprite.
		IMesh *pMesh = pRenderContext->GetDynamicMesh( false, 0, 0, m_Sprites[iSprite].m_pMaterial );

		CMeshBuilder builder;
		builder.Begin( pMesh, MATERIAL_QUADS, 1 );
		
		Vector vPt;
		
		vPt = vBasePt - vRight + vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 0, 1 );
		builder.AdvanceVertex();
		
		vPt = vBasePt + vRight + vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 1, 1 );
		builder.AdvanceVertex();
		
		vPt = vBasePt + vRight - vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 1, 0 );
		builder.AdvanceVertex();
		
		vPt = vBasePt - vRight - vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 0, 0 );
		builder.AdvanceVertex();
		
		builder.End( false, true );

		if( bWireframe )
		{
			IMaterial *pWireframeMaterial = materials->FindMaterial( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );
			pRenderContext->Bind( pWireframeMaterial );
			
			// Draw the sprite.
			IMesh *pMesh = pRenderContext->GetDynamicMesh( false, 0, 0, pWireframeMaterial );
			
			CMeshBuilder builder;
			builder.Begin( pMesh, MATERIAL_QUADS, 1 );
			
			Vector vPt;
			
			vPt = vBasePt - vRight + vUp;
			builder.Position3fv( vPt.Base() );
			builder.Color3f( 1.0f, 0.0f, 0.0f );
			builder.AdvanceVertex();
			
			vPt = vBasePt + vRight + vUp;
			builder.Position3fv( vPt.Base() );
			builder.Color3f( 1.0f, 0.0f, 0.0f );
			builder.AdvanceVertex();
			
			vPt = vBasePt + vRight - vUp;
			builder.Position3fv( vPt.Base() );
			builder.Color3f( 1.0f, 0.0f, 0.0f );
			builder.AdvanceVertex();
			
			vPt = vBasePt - vRight - vUp;
			builder.Position3fv( vPt.Base() );
			builder.Color3f( 1.0f, 0.0f, 0.0f );
			builder.AdvanceVertex();
			
			builder.End( false, true );
		}
	}
}

void CGlowOverlay::Activate()
{
	m_bActivated = true;
	if( !m_ListIndex )
	{
		m_ListIndex = g_GlowOverlaySystem.AddToOverlayList( this );
	}
}

void CGlowOverlay::Deactivate()
{
	m_bActivated = false;
}

void CGlowOverlay::DrawOverlays( bool bCacheFullSceneState )
{
	CVProfScope _( "CGlowOverlay::DrawOverlays()", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CMatRenderContextPtr pRenderContext( materials );

	bool bClippingEnabled = pRenderContext->EnableClipping( true );

	auto &overlays = g_GlowOverlaySystem.m_GlowOverlays;
	for ( auto it = overlays.begin(); it != overlays.end(); )
	{
		CGlowOverlay *pOverlay = *it++;
		
		if( !pOverlay->m_bActivated )
			continue;

		if( pOverlay->Update() )
		{
			pRenderContext->EnableClipping( ((pOverlay->m_bInSky) ? (false):(bClippingEnabled)) ); //disable clipping in skybox, restore clipping to pre-existing state when not in skybox (it may be off as well)
			pOverlay->Draw( bCacheFullSceneState );
		}
		else
		{
			delete pOverlay;
		}
	}

	pRenderContext->EnableClipping( bClippingEnabled ); //restore clipping to original state
}

void CGlowOverlay::UpdateSkyOverlays( float zFar, bool bCacheFullSceneState )
{
	for ( CGlowOverlay *pOverlay : g_GlowOverlaySystem.m_GlowOverlays )
	{
		if( !pOverlay->m_bActivated || !pOverlay->m_bDirectional || !pOverlay->m_bInSky )
			continue;

		pOverlay->UpdateSkyGlowObstruction( zFar, bCacheFullSceneState );
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

class CHairball : public CBaseEntity
{
	DECLARE_CLASS( CHairball, CBaseEntity );
private:

	class CHairballDelegate : public CSimplePhysics::IHelper
	{
	public:
		virtual void	GetNodeForces( CSimplePhysics::CNode *pNodes, int iNode, Vector *pAccel );
		virtual void	ApplyConstraints( CSimplePhysics::CNode *pNodes, int nNodes );
	
		CHairball		*m_pParent;
	};

public:
	
						CHairball();

	void				Init();

// IClientThinkable.
public:
	
	virtual void		ClientThink();

// IClientRenderable.
public:

	virtual int			DrawModel( int flags, const RenderableInstance_t &instance );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType();

public:

	float								m_flSphereRadius;

	int									m_nHairs;
	int									m_nNodesPerHair;
	float								m_flSpringDist;				// = hair length / (m_nNodesPerHair-1)

	std::vector< CSimplePhysics::CNode >	m_Nodes;					// This is m_nHairs * m_nNodesPerHair large.
	std::vector< Vector >					m_HairPositions;			// Untransformed base hair positions, distributed on the sphere.
	std::vector< Vector >					m_TransformedHairPositions;	// Transformed base hair positions, distributed on the sphere.

	CHairballDelegate					m_Delegate;
	CSimplePhysics						m_Physics;

	IMaterial							*m_pMaterial;

	// Super sophisticated AI.
	float m_flSitStillTime;
	Vector m_vMoveDir;

	float	m_flSpinDuration;
	float	m_flCurSpinTime;
	float	m_flSpinRateX, m_flSpinRateY;
	
	bool	m_bFirstThink;
};

void CHairball::CHairballDelegate::GetNodeForces( CSimplePhysics::CNode *pNodes, int iNode, Vector *pAccel )
{
	pAccel->Init( 0, 0, -1500 );
}

void CHairball::CHairballDelegate::ApplyConstraints( CSimplePhysics::CNode *pNodes, int nNodes )
{
	int nSegments = m_pParent->m_nNodesPerHair - 1;
	float flSpringDistSqr = m_pParent->m_flSpringDist * m_pParent->m_flSpringDist;

	static int nIterations = 1;
	for( int iIteration=0; iIteration < nIterations; iIteration++ )
	{
		for ( int iHair=0; iHair < m_pParent->m_nHairs; iHair++ )
		{
			CSimplePhysics::CNode *pBase = &pNodes[iHair * m_pParent->m_nNodesPerHair];

			for( int i=0; i < nSegments; i++ )
			{
				Vector &vNode1 = pBase[i].m_vPos;
				Vector &vNode2 = pBase[i+1].m_vPos;
				Vector vTo = vNode1 - vNode2;

				float flDistSqr = vTo.LengthSqr();
				if( flDistSqr > flSpringDistSqr )
				{
					float flDist = (float)std::sqrt( flDistSqr );
					vTo *= 1 - (m_pParent->m_flSpringDist / flDist);

					vNode1 -= vTo * 0.5f;
					vNode2 += vTo * 0.5f;
				}
			}

			// Lock the base of each hair to the right spot.
			pBase->m_vPos = m_pParent->m_TransformedHairPositions[iHair];
		}
	}
}

CHairball::CHairball()
{
	m_nHairs = 100;
	m_nNodesPerHair = 3;
	
	float flHairLength = 20;
	m_flSpringDist = flHairLength / (m_nNodesPerHair - 1);
	
	m_Nodes.assign( m_nHairs * m_nNodesPerHair, {} );
	m_HairPositions.assign( m_nHairs, {} );
	m_TransformedHairPositions.assign( m_nHairs, {} );

	m_flSphereRadius = 20;
	m_vMoveDir.Init();
	
	m_flSpinDuration = 1;
	m_flCurSpinTime = 0;
	m_flSpinRateX = m_flSpinRateY = 0;

	// Distribute on the sphere (need a better random distribution for the sphere).
	for ( int i=0; i < std::ssize( m_HairPositions ); i++ )
	{
		float theta = RandomFloat( -std::numbers::pi, std::numbers::pi );
		float phi   = RandomFloat( -std::numbers::pi/2, std::numbers::pi/2 );
		
		float cosPhi = std::cos( phi );

		m_HairPositions[i].Init( 
			cos(theta) * cosPhi * m_flSphereRadius, 
			std::sin(theta) * cosPhi * m_flSphereRadius, 
			sin(phi) * m_flSphereRadius );
	}

	m_Delegate.m_pParent = this;

	m_Physics.Init( 1.0 / 20 ); // NOTE: PLAY WITH THIS FOR EFFICIENCY
	m_pMaterial = nullptr;

	m_bFirstThink = true;
}

void CHairball::Init()
{
	ClientEntityList().AddNonNetworkableEntity( this );
	ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	
	AddToLeafSystem( false );

	m_pMaterial = materials->FindMaterial( "cable/cable", TEXTURE_GROUP_OTHER );
	m_flSitStillTime = 5;
}

void CHairball::ClientThink()
{
	// Do some AI-type stuff.. move the entity around.
	//CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	//m_vecAngles = SetAbsAngles( pPlayer->GetAbsAngles() ); // copy player angles.

	contract_assert( !GetMoveParent() );

	// Sophisticated AI.
	m_flCurSpinTime += gpGlobals->frametime;
	if ( m_flCurSpinTime < m_flSpinDuration )
	{
		float div = m_flCurSpinTime / m_flSpinDuration;

		QAngle angles = GetLocalAngles();

		angles.x += m_flSpinRateX * SmoothCurve( div );
		angles.y += m_flSpinRateY * SmoothCurve( div );

		SetLocalAngles( angles );
	}
	else
	{
		// Flip between stopped and starting.
		if ( fabs( m_flSpinRateX ) > 0.01f )
		{
			m_flSpinRateX = m_flSpinRateY = 0;

			m_flSpinDuration = RandomFloat( 1, 2 );
		}
		else
		{
			static float flXSpeed = 3;
			static float flYSpeed = flXSpeed * 0.1f;
			m_flSpinRateX = RandomFloat( -std::numbers::pi*flXSpeed, std::numbers::pi*flXSpeed );
			m_flSpinRateY = RandomFloat( -std::numbers::pi*flYSpeed, std::numbers::pi*flYSpeed );

			m_flSpinDuration = RandomFloat( 1, 4 );
		}

		m_flCurSpinTime = 0;
	}

	if ( m_flSitStillTime > 0 )
	{
		m_flSitStillTime -= gpGlobals->frametime;

		if ( m_flSitStillTime <= 0 )
		{
			// Shoot out some random lines and find the longest one.
			m_vMoveDir.Init( 1, 0, 0 );
			float flLongestFraction = 0;
			for ( int i=0; i < 15; i++ )
			{
				Vector vDir( RandomFloat( -1, 1 ), RandomFloat( -1, 1 ), RandomFloat( -1, 1 ) );
				VectorNormalize( vDir );

				trace_t trace;
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vDir * 10000, MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &trace );

				if ( trace.fraction != 1.0 )
				{
					if ( trace.fraction > flLongestFraction )
					{
						flLongestFraction = trace.fraction;
						m_vMoveDir = vDir;
					}
				}
			}

			m_vMoveDir *= 650; // set speed.
			m_flSitStillTime = -1; // Move in this direction..
		}
	}
	else
	{
		// Move in the specified direction.
		Vector vEnd = GetAbsOrigin() + m_vMoveDir * gpGlobals->frametime;

		trace_t trace;
		UTIL_TraceLine( GetAbsOrigin(), vEnd, MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &trace );

		if ( trace.fraction < 1 )
		{
			// Ok, stop moving.
			m_flSitStillTime = RandomFloat( 1, 3 );
		}
		else
		{
			SetLocalOrigin( GetLocalOrigin() + m_vMoveDir * gpGlobals->frametime );
		}
	}

	// Transform the base hair positions so we can lock them down.
	VMatrix mTransform;
	mTransform.SetupMatrixOrgAngles( GetLocalOrigin(), GetLocalAngles() );

	for ( int i=0; i < std::ssize( m_HairPositions ); i++ )
	{
		Vector3DMultiplyPosition( mTransform, m_HairPositions[i], m_TransformedHairPositions[i] );
	}

	if ( m_bFirstThink )
	{
		m_bFirstThink = false;
		for ( int i=0; i < std::ssize( m_HairPositions ); i++ )
		{
			for ( int j=0; j < m_nNodesPerHair; j++ )
			{
				m_Nodes[i*m_nNodesPerHair+j].Init( m_TransformedHairPositions[i] );
			}
		}
	}

	// Simulate the physics and apply constraints.
	m_Physics.Simulate( m_Nodes.data(), std::ssize( m_Nodes ), &m_Delegate, gpGlobals->frametime, 0.98 );
}

RenderableTranslucencyType_t CHairball::ComputeTranslucencyType()
{
	return ( m_pMaterial && m_pMaterial->IsTranslucent() ) ? RENDERABLE_IS_TRANSLUCENT : RENDERABLE_IS_OPAQUE;
}

int CHairball::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_pMaterial )
		return 0;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	for ( int iHair=0; iHair < m_nHairs; iHair++ )
	{
		CSimplePhysics::CNode *pBase = &m_Nodes[iHair * m_nNodesPerHair];
		
		CBeamSegDraw beamDraw;
		beamDraw.Start( pRenderContext, m_nNodesPerHair-1, m_pMaterial );

		for ( int i=0; i < m_nNodesPerHair; i++ )
		{
			BeamSeg_t seg;
			seg.m_vPos = pBase[i].m_vPredicted;
			seg.m_color.r = seg.m_color.g = seg.m_color.b = seg.m_color.a = 0;
			seg.m_flTexCoord = 0;
			static float flHairWidth = 1;
			seg.m_flWidth = flHairWidth;

			beamDraw.NextSeg( &seg );
		}
		
		beamDraw.End();
	}

	return 1;
}

void CreateHairballCallback()
{
	for ( int i=0; i < 20; i++ )
	{
		CHairball *pHairball = new CHairball;
		pHairball->Init();
		
		// Put it a short distance in front of the player.
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		
		if ( !pPlayer )
			return;

		Vector vForward;
		AngleVectors( pPlayer->GetAbsAngles(), &vForward );
		pHairball->SetLocalOrigin( pPlayer->GetAbsOrigin() + vForward * 300 + RandomVector( 0, 100 ) );
	}
}

ConCommand cc_CreateHairball( side_registry(), "CreateHairball", CreateHairballCallback, 0, FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

class CHintMessageQueue;
class CHintMessageTimers;
class CHintSystem;
class CBasePlayer;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------
/**
* Simple utility function to allocate memory and duplicate a string
*/
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ std::strlen(str)+1 ];
	std::strcpy( cloneStr, str );
	return cloneStr;
}

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

class CHintMessageQueue;
class CHintMessageTimers;
class CHintSystem;
class CBasePlayer;

using namespace ::ks::reflect;

ConVar cl_showhelp( side_registry(), "cl_showhelp", "1", FCVAR_ARCHIVE, "Set to 0 to not show on-screen help" );

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

	// On the client, we just send it straight to the hint hud element
	if ( cl_showhelp.GetBool() )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_hintmessage" );
		if ( event )
		{
			event->SetString( "hintmessage", pMessage );
			gameeventmanager->FireEventClientSide( event );
		}
	}
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
	return cl_showhelp.GetBool();
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
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->Hints() )
	{
		pPlayer->Hints()->ClearHintHistory();
	}
}
ConCommand cl_clearhinthistory( side_registry(), "cl_clearhinthistory", HintClear, "Clear memory of client side hints displayed to the player." );

class CBaseHudChat;
class CHudChat;
class CUserCmd;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CTeam;

using namespace ::ks::reflect;

static void Spec_Autodirector_Cameraman_Callback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( HLTVCamera() && HLTVCamera()->AutoDirectorState() != CHLTVCamera::AUTODIRECTOR_OFF )
	{
		HLTVCamera()->SetAutoDirector(CHLTVCamera::AUTODIRECTOR_ON);
	}
}

ConVar spec_autodirector( side_registry(), "spec_autodirector", "1", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director chooses best view modes while spectating" );
ConVar spec_autodirector_pausetime( side_registry(), "spec_autodirector_pausetime", "10", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director will pause for this long if a player is selected." );
ConVar spec_autodirector_cameraman( side_registry(), "spec_autodirector_cameraman", "-1", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Cameraman account ID. If a cameraman is active then use them when spectating and autodirector is active, 0 = no caster", Spec_Autodirector_Cameraman_Callback );
ConVar spec_overwatch_skip_idle_ticks( side_registry(), "spec_overwatch_skip_idle_ticks", "10", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director in overwatch mode will be skipping ticks when no subject observations are played." );

extern ConVar view_recoil_tracking;

#define CHASE_CAM_DISTANCE		76.0f
#define WALL_OFFSET				6.0f

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);
static const ConVar	*tv_transmitall = nullptr;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static CHLTVCamera s_HLTVCamera;

CHLTVCamera *HLTVCamera() { return &s_HLTVCamera; }

CHLTVCamera::CHLTVCamera()
{
	Reset();

	m_nNumSpectators = 0;
	m_szTitleText[0] = 0;
}

CHLTVCamera::~CHLTVCamera()
{

}

void CHLTVCamera::Init()
{
	ListenForGameEvent( "game_newmap" );
	ListenForGameEvent( "hltv_fixed" );
	ListenForGameEvent( "hltv_chase" );
	ListenForGameEvent( "hltv_message" );
	ListenForGameEvent( "hltv_title" );
	ListenForGameEvent( "hltv_status" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_connect_full" );
	ListenForGameEvent( "player_team" );
	
	Reset();

	m_nNumSpectators = 0;
	m_szTitleText[0] = 0;

	// get a handle to the engine convar
	tv_transmitall = cvar->FindVar( "tv_transmitall" );
}

void CHLTVCamera::Reset()
{
	m_nCameraMode = OBS_MODE_FIXED;
	m_iCameraMan  = 0;
	m_iTarget1 = m_iTarget2 = m_iLastTarget1 = 0;
	m_flFOV = 90;
	m_flDistance = m_flLastDistance = CHASE_CAM_DISTANCE;
	m_flInertia = 3.0f;
	m_flPhi = 0;
	m_flTheta = 0;
	m_flOffset = 0;
	m_bEntityPacketReceived = false;

	m_vCamOrigin.Zero();
	m_aCamAngle.Init();

	m_LastCmd.Reset();
	m_vecVelocity.Init();

	m_vIdealOverviewPos.Zero();
	m_vOldOverviewPos.Zero();
	m_vLastGrenadeVelocity.Zero();
	m_flLastGrenadeVelocityUpdate = 0;
	m_flLastCamZPos = 0;

	m_flAutodirectorPausedTime = -1.0f;
	m_flIdealOverviewScale = 1.0f;
	m_flNextIdealOverviewPosUpdate = 0;

	m_bIsSpecLerping = false;
	m_vecSpecLerpIdealPos = Vector( 0, 0, 0 );
	m_angSpecLerpIdealAng = QAngle( 0, 0, 0 );
	m_vecSpecLerpOldPos = Vector( 0, 0, 0 );
	m_angSpecLerpOldAng = QAngle( 0, 0, 0 );
	m_flSpecLerpEndTime = 0.0f;
	m_flSpecLerpTime = 1.0f;
	m_bIsFollowingGrenade = false;
}

void CHLTVCamera::SetWatchingGrenade( CBaseEntity *pGrenade, bool bWatching )
{
	if ( bWatching )
	{
		if ( pGrenade && m_bIsFollowingGrenade == false )
		{
			m_bIsFollowingGrenade = true;
			SetPrimaryTarget( pGrenade->entindex() );
		}
	}
	else
	{
		// only change state if we get a false
		//if ( pGrenade->entindex() == m_iTarget1 )
		{
			if ( m_bIsFollowingGrenade == true )
			{
				CBaseEntity *pLastTarget = ClientEntityList().GetBaseEntity( m_iLastTarget1 );

				if ( !pLastTarget || pLastTarget->IsDormant() || !pLastTarget->IsAlive() )
				{
					// find a player that is alive to go to
					for ( int i = 1; i <= MAX_PLAYERS; i++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
						if ( pPlayer && pPlayer->IsAlive() && (pPlayer->GetTeamNumber() == TEAM_TERRORIST || pPlayer->GetTeamNumber() == TEAM_CT) )
						{
							m_iLastTarget1 = pPlayer->entindex();
							break;
						}
					}
				}

				m_iTarget1 = m_iLastTarget1;
			}

			m_bIsFollowingGrenade = false;
		}
	}
}

void CHLTVCamera::CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
 	Vector targetOrigin1, targetOrigin2, cameraOrigin, forward;

 	if ( m_iTarget1 == 0 )
		return;

	// get primary target, also translates to ragdoll
	CBaseEntity *target1 = GetPrimaryTarget();

	if ( !target1 ) 
	{
		bool bFoundTarget = false;
		if ( m_bIsFollowingGrenade )
		{
			// if we we3re following a grenade and lost our target, revert back to our previous target
			CBaseEntity* oldTarget = ClientEntityList().GetEnt( m_iLastTarget1 );
			if ( oldTarget )
			{
				target1 = oldTarget;
				bFoundTarget = true;
			}
		}

		if ( !bFoundTarget )
			return;
	}
	
/*#ifdef CSTRIKE_DLL
	// weapon gun-cam go-pro chase camera
	CCSPlayer *pPlayer = ToCSPlayer( target1 );
	if ( pPlayer && pPlayer->ShouldDraw() )
	{
		Vector vecSrc = target1->GetObserverCamOrigin();
		vecSrc += (target1->GetFlags() & FL_DUCKING) ? VEC_DUCK_VIEW : VEC_VIEW;

		Vector vecObsForward, vecObsRight, vecObsUp;
		AngleVectors( m_aCamAngle, &vecObsForward, &vecObsRight, &vecObsUp );

		trace_t playerEyeTrace;
		UTIL_TraceLine( vecSrc, vecSrc - vecObsForward * 75.0f, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &playerEyeTrace );
		
		float flDistMax = playerEyeTrace.startpos.DistTo( playerEyeTrace.endpos + vecObsForward * 4 );
		m_flObserverChaseApproach = (m_flObserverChaseApproach >= flDistMax) ? flDistMax : Approach( 75.0f, m_flObserverChaseApproach, gpGlobals->frametime * 20 );

		Vector vecIdealCamEyePos = vecSrc - vecObsForward * m_flObserverChaseApproach;
		Vector vecIdealCamTargetPos = vecSrc + vecObsRight * RemapValClamped( m_flObserverChaseApproach, 20, 75, 6, 16 ) * abs(DotProduct(vecObsUp,Vector(0,0,1)));
		VectorAngles( (vecIdealCamTargetPos - vecIdealCamEyePos ).Normalized(), eyeAngles );

		eyeOrigin = vecIdealCamEyePos;
		return;
	}
#endif*/

	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target1 );

	if ( pGrenade )
	{
		m_iTarget2 = m_iTarget1;
		m_bIsFollowingGrenade = true;
	}

	bool bManual = !spec_autodirector.GetBool();
	if ( pGrenade && pGrenade->m_nBounces <= 0 )	// chase camera controlled manually
		bManual = false;

	if ( target1->IsPlayer() && target1->IsAlive() && target1->IsDormant() )
		return;

	targetOrigin1 = target1->GetRenderOrigin();

	if ( target1->IsPlayer() && !target1->IsAlive() )
	{
		targetOrigin1 += VEC_DEAD_VIEWHEIGHT;
	}
	else if ( target1->IsPlayer() && target1->GetFlags() & FL_DUCKING )
	{
		targetOrigin1 += VEC_DUCK_VIEW;
	}
	else if ( pGrenade )
	{
		targetOrigin1 += 2;//(VEC_DUCK_VIEW/2);
	}
	else
	{
		targetOrigin1 += VEC_VIEW;
	}

	// get secondary target if set
	CBaseEntity *target2 = nullptr;

	if ( m_iTarget2 > 0 && (m_iTarget2 != m_iTarget1) && !bManual )
	{
		target2 = ClientEntityList().GetBaseEntity( m_iTarget2 );

		// if target is out PVS and not dead, it's not valid
		if ( target2 && target2->IsDormant() && target2->IsAlive() )
			target2 = nullptr;

		if ( target2 )
		{
			targetOrigin2 = target2->GetRenderOrigin();

			if ( !target2->IsAlive() )
			{
				targetOrigin2 += VEC_DEAD_VIEWHEIGHT;
			}
			else if ( target2->GetFlags() & FL_DUCKING )
			{
				targetOrigin2 += VEC_DUCK_VIEW;
			}
			else
			{
				targetOrigin2 += VEC_VIEW;
			}
		}
	}

		// apply angle offset & smoothing
	QAngle angleOffset(  m_flPhi, m_flTheta, 0 );
	QAngle cameraAngles = m_aCamAngle;

	if ( bManual )
	{
		// let spectator choose the view angles
 		engine->GetViewAngles( cameraAngles );
	}
	else if ( target2 )
	{
		// look into direction of second target
 		forward = targetOrigin2 - targetOrigin1;
        VectorAngles( forward, cameraAngles );
        cameraAngles.z = 0; // no ROLL
	}
	else if ( pGrenade || m_iTarget2 == 0 || m_iTarget2 == m_iTarget1 )
	{
		if ( pGrenade )
		{
			//QAngle angFacing = pGrenade->GetLocalVelocity();
			Vector vecVel = pGrenade->GetLocalVelocity();//pGrenade->GetLocalAngularVelocity();//targetOrigin1 - m_vLastTarget1Origin;
		
			//cameraAngles = angFacing;
//			engine->Con_NPrintf(30, "vel = (%f, %f, %f)", XYZ(vecVel) );
			//cameraAngles = angFacing;

			angleOffset.Init();

			//float flTransTime = 0.1f;
			float flInterp = clamp( ( gpGlobals->curtime - m_flLastGrenadeVelocityUpdate ) * 10, 0.0f, 1.0f );
			//float flActualinterp = Gain( flInterp, 0.6 );
			Vector vecActualVel = Lerp( flInterp, m_vLastGrenadeVelocity, vecVel );
			//cameraAngles.x = 0; // no PITCH

			VectorAngles( vecActualVel, cameraAngles );
			if ( m_vLastGrenadeVelocity != vecVel )
			{
				m_flLastGrenadeVelocityUpdate = gpGlobals->curtime;
				m_vLastGrenadeVelocity = vecActualVel;
			}

//			Msg( "vel = (%f, %f, %f) - flInterp = %f\n", XYZ(vecActualVel), flInterp );

			// set the max distance to 64 with grenades
			m_flDistance = 64;
		}
		else
		{
			// look into direction where primary target is looking
			cameraAngles = target1->EyeAngles();
			cameraAngles.x = 0; // no PITCH
		}

		cameraAngles.z = 0; // no ROLL
	}
	else
	{
		// target2 is missing, just keep angelsm, reset offset
		angleOffset.Init();
	}

	if ( !bManual && target1->IsPlayer() )
	{
		if ( !target1->IsAlive() )
		{
			angleOffset.x = 15;
		}

		cameraAngles += angleOffset;
	}

	AngleVectors( cameraAngles, &forward );

	VectorNormalize( forward );

	// calc optimal camera position
	VectorMA(targetOrigin1, -m_flDistance, forward, cameraOrigin );

 	targetOrigin1.z += m_flOffset; // add offset

	// clip against walls
  	trace_t trace;
	CBaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( targetOrigin1, cameraOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, target1, COLLISION_GROUP_NONE, &trace );
	CBaseEntity::PopEnableAbsRecomputations();

  	float dist = VectorLength( trace.endpos -  targetOrigin1 );

	// grow distance by 32 unit a second
  	m_flLastDistance += gpGlobals->frametime * 32.0f; 

  	if ( dist > m_flLastDistance )
	{
		VectorMA(targetOrigin1, -m_flLastDistance, forward, cameraOrigin );
	}
 	else
	{
		cameraOrigin = trace.endpos;
		m_flLastDistance = dist;
	}
	
  	if ( target2 /*|| pGrenade*/ )
	{
		if ( pGrenade )
		{
			// if we have 2 targets look at point between them
  			forward = targetOrigin1 - cameraOrigin;
  			QAngle angle;
  			VectorAngles( forward, angle );
  			cameraAngles = angle;

			NormalizeAngles( cameraAngles );
			//cameraAngles.x = clamp( cameraAngles.x, -30, 30 );
		}
		else
		{
			// if we have 2 targets look at point between them
			forward = ( targetOrigin1 + targetOrigin2 ) / 2 - cameraOrigin;
			QAngle angle;
			VectorAngles( forward, angle );
			cameraAngles.y = angle.y;

			NormalizeAngles( cameraAngles );
			cameraAngles.x = clamp( cameraAngles.x, -60, 60 );
		}

		SmoothCameraAngle( cameraAngles );
	}
	else
	{
		SetCameraAngle( cameraAngles );
	}

//	engine->Con_NPrintf(20, "pos = (%f, %f, %f)", XYZ(m_vCamOrigin) );
// 	engine->Con_NPrintf(22, "ang = (%f, %f, %f)", XYZ(m_aCamAngle) );

	VectorCopy( cameraOrigin, m_vCamOrigin );
	VectorCopy( m_aCamAngle, eyeAngles );
	VectorCopy( m_vCamOrigin, eyeOrigin );
}

Vector CHLTVCamera::CalcIdealOverviewPosition( Vector vecStartPos, Vector vOldOverviewPos )
{
	Vector vecMinDist = vecStartPos + Vector( -1024.0f, -1024.0f, -256.0f );
	Vector vecMaxDist = vecStartPos + Vector( 1024.0f, 1024.0f, 256.0f );

	CBaseEntity	*pEntList[128];
	int count = UTIL_EntitiesInBox( pEntList, std::size( pEntList ), vecMinDist, vecMaxDist, 0 );
	//CBaseEntity *pFarthestEnt = NULL;
	float flFarthestEntDist = 0;

	CBaseEntity *pFocusEnts[32];
	int focusCount = 0;

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pOther = pEntList[i];
		if ( dynamic_cast<CCSPlayer*>(pOther) || dynamic_cast<CPlantedC4*>(pOther) || dynamic_cast<CBaseCSGrenadeProjectile*>(pOther) )
		{
			pFocusEnts[focusCount] = pOther;
			focusCount++;

			float dist = VectorLength( vecStartPos - pOther->GetAbsOrigin() );
			if ( dist > flFarthestEntDist )
				flFarthestEntDist = dist;
		}
	}

	Vector vecAvPos = Vector( 0, 0, 0 );
	for ( int i = 0; i < focusCount; i++ )
	{
		vecAvPos += pFocusEnts[i]->GetAbsOrigin();
	}

	Vector vecNewPos = vecStartPos;
	if ( focusCount > 0 )
	{
		vecNewPos = (vecAvPos/focusCount);
	}

	float flDist = (vOldOverviewPos-vecNewPos).Length();
	float flDistScaler = clamp( 1+((flDist - 200) / 500), 1, 2 );

	m_flIdealOverviewScale = std::max<std::common_type_t<double, float>>( 0.1, (std::min<std::common_type_t<float, int>>( flFarthestEntDist, 1024 ) / 1024) ) * (1.5 * flDistScaler);

	return vecNewPos;
}

int CHLTVCamera::GetMode()
{
	// hacky....
	if ( dynamic_cast< CBaseCSGrenadeProjectile* >( GetPrimaryTarget() ) )
	{
		m_bIsFollowingGrenade = true;
		return OBS_MODE_CHASE;
	}

	if ( m_iCameraMan > 0 )
	{
		CBasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );

		if ( pCameraMan )
			return pCameraMan->GetObserverMode();
	}

	// to get here, our target is not a grenade, but we think we're still folowing one
 	if ( m_bIsFollowingGrenade == true )
 	{
 		if ( CCSPlayer::GetLocalCSPlayer() )	
 		{
 			// if we're the cameraman and we're holding the shift key after a grenade has exired, 
 			// keep the camera where it is
 			bool bHoldingGrenadeKey = CCSPlayer::GetLocalCSPlayer()->IsHoldingSpecGrenadeKey();
 			if ( bHoldingGrenadeKey )
 				return OBS_MODE_ROAMING;
 
 			// otherwise, we're not following a grenade anymore
 			m_bIsFollowingGrenade = false;
 		}
 	}

	return m_nCameraMode;	
}

CBaseEntity* CHLTVCamera::GetPrimaryTarget()
{
	if ( m_iCameraMan > 0 )
	{
		CBasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		
		if ( pCameraMan )
		{
			return pCameraMan->GetObserverTarget();
		}
	}

	if ( m_iTarget1 <= 0 )
	{
		return nullptr;
	}

	CBaseEntity* target = ClientEntityList().GetEnt( m_iTarget1 );

	if ( !target || (m_bIsFollowingGrenade && dynamic_cast< CBaseCSGrenadeProjectile* >( target ) == nullptr) )
	{
		CBaseEntity* oldTarget = ClientEntityList().GetEnt( m_iLastTarget1 );
		if ( oldTarget )
		{
			target = oldTarget;
			m_iTarget1 = m_iLastTarget1;
		}
	}

	return target;
}

CBasePlayer *CHLTVCamera::GetCameraMan()
{
	return m_iCameraMan ? UTIL_PlayerByIndex( m_iCameraMan ): nullptr;
}

void CHLTVCamera::CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	CBasePlayer *pPlayer = dynamic_cast<CCSPlayer*>(GetPrimaryTarget());

	if ( !pPlayer )
		return;

	if ( !pPlayer->IsAlive() )
	{
		// if dead, show from 3rd person
		CCSPlayer *pCSPlayer =	static_cast<CCSPlayer*>( pPlayer );
		if ( pCSPlayer && pCSPlayer->GetLastKillerIndex() )
			m_iTarget2 = pCSPlayer->GetLastKillerIndex();

		CalcChaseCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	m_aCamAngle	= pPlayer->EyeAngles();
	m_vCamOrigin = pPlayer->GetAbsOrigin();
	m_flFOV = pPlayer->GetFOV();

	// Apply punch angle
	VectorAdd( m_aCamAngle, pPlayer->GetViewPunchAngle(), m_aCamAngle );

	// Apply aim punch angle
	VectorAdd( m_aCamAngle, pPlayer->GetAimPunchAngle() * view_recoil_tracking.GetFloat(), m_aCamAngle );

	// Shake it up baby!
	GetViewEffects()->CalcShake();
	GetViewEffects()->ApplyShake( m_vCamOrigin, m_aCamAngle, 1.0 );

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		m_vCamOrigin += VEC_DUCK_VIEW;
	}
	else
	{
		m_vCamOrigin += VEC_VIEW;
	}

	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	fov = m_flFOV;

	pPlayer->CalcViewModelView( eyeOrigin, eyeAngles);

	// Update view model visibility
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel( i );
		if ( !vm )
			continue;
		vm->UpdateVisibility();
	}
}

void CHLTVCamera::CalcChaseOverview( CViewSetup &pSetup )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_iTarget1 );

	if ( !pPlayer )
		return;

	m_aCamAngle	= QAngle( 90, 90, 0 );//pPlayer->EyeAngles();
	float flTransTime = 0.5f;

	if ( m_flNextIdealOverviewPosUpdate < gpGlobals->curtime )
	{
		m_vOldOverviewPos = m_vCamOrigin;
		Vector vNewIdealPos = CalcIdealOverviewPosition( pPlayer->GetAbsOrigin(), m_vOldOverviewPos ) + Vector( 0, 0, 128 );

		float flDist = (m_vOldOverviewPos-vNewIdealPos).Length();
		if ( flDist > 200.0f )
		{
			m_vIdealOverviewPos.x = vNewIdealPos.x;
			m_vIdealOverviewPos.y = vNewIdealPos.y;

			float flCamZDist = abs(m_flLastCamZPos - m_vIdealOverviewPos.z);
			if ( m_flLastCamZPos == 0 || flCamZDist > 64 )
			{
				trace_t	tr;
				UTIL_TraceLine( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin() + Vector( 0, 0, 512 ), MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
				m_vIdealOverviewPos.z = std::max( tr.endpos.z - 32.0f, pPlayer->GetAbsOrigin().z + 80.0f );
			}
		}

		m_flNextIdealOverviewPosUpdate = gpGlobals->curtime + flTransTime;
	}

	float flInterp = clamp( ( gpGlobals->curtime - (m_flNextIdealOverviewPosUpdate - flTransTime) ) / flTransTime, 0.0f, 1.0f );
	float flActualinterp = Gain( flInterp, 0.6 );
	m_vCamOrigin = m_vOldOverviewPos + ((m_vIdealOverviewPos-m_vOldOverviewPos) * flActualinterp);

	m_flFOV = 180.0f;

	pSetup.origin = m_vCamOrigin;
	pSetup.angles = m_aCamAngle;
	pSetup.fov = m_flFOV;
}

void CHLTVCamera::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	float addspeed, accelspeed, currentspeed;

	// See if we are changing direction a bit
	currentspeed =m_vecVelocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of acceleration.
	accelspeed = accel * gpGlobals->frametime * wishspeed;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (int i=0 ; i<3 ; i++)
	{
		m_vecVelocity[i] += accelspeed * wishdir[i];	
	}
}

static ConVar fov_cs_debug( side_registry(), "fov_cs_debug", "0", FCVAR_CHEAT, "Debug FOV override for spectator camera" ); 
// movement code is a copy of CGameMovement::FullNoClipMove()
void CHLTVCamera::CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	if ( !CSGameRules() )
		return;

	if ( m_bIsSpecLerping )
	{
		eyeOrigin = m_vCamOrigin;
		eyeAngles = m_aCamAngle;
		//fov = m_flFOV;
		fov = fov_cs_debug.GetInt() > 0 ? m_flFOV : CSGameRules()->DefaultFOV();

		if ( (m_vecSpecLerpIdealPos == m_vCamOrigin && m_angSpecLerpIdealAng == m_aCamAngle) || m_flSpecLerpEndTime <= gpGlobals->curtime )
		{
			m_bIsSpecLerping = false;
			SpecCameraGotoPos( m_vecSpecLerpIdealPos, m_angSpecLerpIdealAng );
		}
		else
		{
			float flTransTime = m_flSpecLerpTime;
			float flInterp = clamp( ( gpGlobals->curtime - (m_flSpecLerpEndTime - flTransTime) ) / flTransTime, 0.0f, 1.0f );
			float flActualinterp = Gain( flInterp, 0.6 );
			Vector vCamOrigin = m_vecSpecLerpOldPos + ((m_vecSpecLerpIdealPos-m_vecSpecLerpOldPos) * flActualinterp);
			QAngle aCamAngles = Lerp( flActualinterp, m_angSpecLerpOldAng, m_angSpecLerpIdealAng );//m_angSpecLerpOldAng + ((m_angSpecLerpIdealAng-m_angSpecLerpOldAng) * flActualinterp);
			SpecCameraGotoPos( vCamOrigin, aCamAngles );
		}
		return;
	}

	// only if PVS isn't locked by auto-director
	if ( !IsPVSLocked() )
	{
		Vector wishvel;
		Vector forward, right, up;
		Vector wishdir;
		float wishspeed;
		float factor = sv_specspeed.GetFloat();
		float maxspeed = sv_maxspeed.GetFloat() * factor;

		AngleVectors ( m_LastCmd.viewangles, &forward, &right, &up);  // Determine movement angles

		if ( m_LastCmd.buttons & IN_SPEED )
		{
			factor /= 2.0f;
		}

		// Copy movement amounts
		float fmove = m_LastCmd.forwardmove * factor;
		float smove = m_LastCmd.sidemove * factor;

		VectorNormalize (forward);  // Normalize remainder of vectors
		VectorNormalize (right);    // 

		for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
			wishvel[i] = forward[i]*fmove + right[i]*smove;
		wishvel[2] += m_LastCmd.upmove * factor;

		VectorCopy (wishvel, wishdir);   // Determine magnitude of speed of move
		wishspeed = VectorNormalize(wishdir);

		//
		// Clamp to server defined max speed
		//
		if (wishspeed > maxspeed )
		{
			VectorScale (wishvel, maxspeed/wishspeed, wishvel);
			wishspeed = maxspeed;
		}

		if ( sv_specaccelerate.GetFloat() > 0.0 )
		{
			// Set move velocity
			Accelerate ( wishdir, wishspeed, sv_specaccelerate.GetFloat() );

			float spd = VectorLength( m_vecVelocity );
			if (spd < 1.0f)
			{
				m_vecVelocity.Init();
			}
			else
			{
				// Bleed off some speed, but if we have less than the bleed
				//  threshold, bleed the threshold amount.
				float control = (spd < maxspeed/4.0) ? maxspeed/4.0 : spd;

				float friction = sv_friction.GetFloat();

				// Add the amount to the drop amount.
				float drop = control * friction * gpGlobals->frametime;

				// scale the velocity
				float newspeed = spd - drop;
				if (newspeed < 0)
					newspeed = 0;

				// Determine proportion of old speed we are using.
				newspeed /= spd;
				VectorScale( m_vecVelocity, newspeed, m_vecVelocity );
			}
		}
		else
		{
			VectorCopy( wishvel, m_vecVelocity );
		}

		// Just move ( don't clip or anything )
		VectorMA( m_vCamOrigin, gpGlobals->frametime, m_vecVelocity, m_vCamOrigin );
		
		// get camera angle directly from engine
		 engine->GetViewAngles( m_aCamAngle );

		// Zero out velocity if in noaccel mode
		if ( sv_specaccelerate.GetFloat() < 0.0f )
		{
			m_vecVelocity.Init();
		}
	}

	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	//fov = m_flFOV;
	fov = fov_cs_debug.GetInt() > 0 ? m_flFOV : CSGameRules()->DefaultFOV();
}

void CHLTVCamera::CalcFixedView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	fov = m_flFOV;

	if ( m_bIsSpecLerping )
	{
		if ( (m_vecSpecLerpIdealPos == m_vCamOrigin && m_angSpecLerpIdealAng == m_aCamAngle) || m_flSpecLerpEndTime <= gpGlobals->curtime )
		{
			m_bIsSpecLerping = false;
			SpecCameraGotoPos( m_vecSpecLerpIdealPos, m_angSpecLerpIdealAng );
		}
		else
		{
			float flTransTime = m_flSpecLerpTime;
			float flInterp = clamp( ( gpGlobals->curtime - (m_flSpecLerpEndTime - flTransTime) ) / flTransTime, 0.0f, 1.0f );
			float flActualinterp = Gain( flInterp, 0.6 );
			Vector vCamOrigin = m_vecSpecLerpOldPos + ((m_vecSpecLerpIdealPos-m_vecSpecLerpOldPos) * flActualinterp);
			QAngle aCamAngles = Lerp( flActualinterp, m_angSpecLerpOldAng, m_angSpecLerpIdealAng );//m_angSpecLerpOldAng + ((m_angSpecLerpIdealAng-m_angSpecLerpOldAng) * flActualinterp);
			SpecCameraGotoPos( vCamOrigin, aCamAngles );
		}
	}
	else
	{
		int nTarget = m_iTarget1;
		if ( m_iTarget1 == 0 && m_iLastTarget1 == 0 )
			return;

		int nButtonBits = input->GetButtonBits( false );

		if (m_iTarget1 == 0 && !(nButtonBits & IN_FORWARD) )
			return;

		if ( (nButtonBits & IN_FORWARD) )
			nTarget = m_iLastTarget1;

		CBaseEntity * target = ClientEntityList().GetBaseEntity( nTarget );

		if ( target && target->IsAlive() )
		{
			// if we're chasing a target, change viewangles
			QAngle angle;
			VectorAngles( (target->GetAbsOrigin()+VEC_VIEW) - m_vCamOrigin, angle );
			SmoothCameraAngle( angle );
		}
	}
}

void CHLTVCamera::PostEntityPacketReceived()
{
	m_bEntityPacketReceived = true;
}

void CHLTVCamera::FixupMovmentParents()
{
	// Find resource zone
	
	for (	ClientEntityHandle_t e = ClientEntityList().FirstHandle();
			e != ClientEntityList().InvalidHandle(); e = ClientEntityList().NextHandle( e ) )
	{
		CBaseEntity *ent = CBaseEntity::Instance( e );

		if ( !ent )
			continue;

		ent->HierarchyUpdateMoveParent();
	}
}

void CHLTVCamera::CalcView(CViewSetup *pSetup)
{
	//CViewSetup *pSetup

	if ( m_bEntityPacketReceived )
	{
		// try to fixup movment pareents
		FixupMovmentParents();
		m_bEntityPacketReceived = false;
	}

	if ( m_iCameraMan > 0 )
	{
		CBasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		if ( pCameraMan )
		{
			//float zNear,zFar;
			pCameraMan->CalcView( pSetup->origin, pSetup->angles, pSetup->zNear, pSetup->zFar, pSetup->fov );
			pCameraMan->CalcViewModelView( pSetup->origin, pSetup->angles );
			return;
		}
	}

	if ( input->CAM_IsThirdPersonOverview() )
	{
		CalcChaseOverview( *pSetup );
	}
	else
	{
		switch ( GetMode() )
		{
		case OBS_MODE_ROAMING	:	CalcRoamingView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_FIXED		:	CalcFixedView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_IN_EYE	:	CalcInEyeCamView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_CHASE		:	CalcChaseCamView( pSetup->origin, pSetup->angles, pSetup->fov  );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
// void CHLTVCamera::CalcOverview( CViewSetup *pSetup )
// {
// 	QAngle camAngles;
// 
// 	// Let the player override the view.
// 	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
// 	if(!pPlayer)
// 		return;
// 
// 	pPlayer->OverrideView( pSetup );
// 
// 	if( false/*::input->CAM_IsThirdPerson()*/ )
// 	{
// 		Vector cam_ofs;
// 
// 		::input->CAM_GetCameraOffset( cam_ofs );
// 
// 		camAngles[ PITCH ] = cam_ofs[ PITCH ];
// 		camAngles[ YAW ] = cam_ofs[ YAW ];
// 		camAngles[ ROLL ] = 0;
// 
// 		Vector camForward, camRight, camUp;
// 		AngleVectors( camAngles, &camForward, &camRight, &camUp );
// 
// 		pSetup->origin = pPlayer->GetThirdPersonViewPosition();
// 
// 		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );
// 
// 		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
// 		if ( c_thirdpersonshoulder.GetBool() )
// 		{
// 			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
// 			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
// 			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );
// 
// 			// add the shoulder offset to the origin in the cameras right vector
// 			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );
// 
// 			// add the shoulder height to the origin in the cameras up vector
// 			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );
// 
// 			// adjust the yaw to the aim-point
// 			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
// 
// 			// adjust the pitch to the aim-point
// 			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
// 		}
// 
// 		// Override angles from third person camera
// 		VectorCopy( camAngles, pSetup->angles );
// 	}
// 	else if ( true /*::input->CAM_IsOrthographic()*/)
// 	{
// 		pSetup->m_bOrtho = true;
// 		float w, h;
// 		::input->CAM_OrthographicSize( w, h );
// 		w *= 0.5f;
// 		h *= 0.5f;
// 		pSetup->m_OrthoLeft   = -w;
// 		pSetup->m_OrthoTop    = -h;
// 		pSetup->m_OrthoRight  = w;
// 		pSetup->m_OrthoBottom = h;
// 	}
// }

void CHLTVCamera::SetMode(int iMode)
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			iMode = OBS_MODE_IN_EYE;
		}
	}

	if ( m_nCameraMode == iMode )
		return;

    contract_assert( iMode > OBS_MODE_NONE && iMode <= LAST_PLAYER_OBSERVERMODE );

	int iOldMode = m_nCameraMode;
	m_nCameraMode = iMode;

	if ( m_nCameraMode == OBS_MODE_IN_EYE || m_nCameraMode == OBS_MODE_CHASE )
		m_bIsSpecLerping = false;

	IGameEvent *event = gameeventmanager->CreateEvent( "hltv_changed_mode" );
	if ( event )
	{
		event->SetInt( "oldmode", iOldMode );
		event->SetInt( "newmode", m_nCameraMode );
		event->SetInt( "obs_target", m_iTarget1 );
		gameeventmanager->FireEventClientSide( event );
	}

	// tell the target player to update the visibility of their view and world models
	CBaseEntity * target = UTIL_PlayerByIndex( m_iTarget1 );
	if ( target && target->IsPlayer() )
	{
		CBasePlayer * player = ToBasePlayer( target );
		if ( player )
			player->OnObserverModeChange( true );
	}

}

void CHLTVCamera::SetPrimaryTarget( int nEntity ) 
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Make sure that the requested target is the player account
			// that PVS must be locked to
			bool bFoundValidPlayer = false;
			for ( int idxEntity = 1; idxEntity <= gpGlobals->maxClients; ++ idxEntity )
			{
				CBaseEntity * target = UTIL_PlayerByIndex( idxEntity );

				if ( target == nullptr )
					continue;

				if ( !target->IsPlayer() )
					continue;

				CBasePlayer * player = ToBasePlayer( target );
				if ( !player )
					continue;

				CPlayerId playerUID;
				if ( player->GetPlayerID( &playerUID ) && playerUID.IsValid() &&
					( playerUID.GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
				{
					// when playback wants to lock to a specific account force PVS lock
					// regardless of player states to see exactly what that player was seeing
					bFoundValidPlayer = true;
					nEntity = idxEntity; // force the only entity allowed
					break;
				}
			}
			if ( !bFoundValidPlayer )
				return;
		}
	}

	if ( m_iTarget1 == nEntity )
		return;

	m_iLastTarget1 = m_iTarget1;

	m_iTarget1 = nEntity;

	// BUG: This uses the values (mode, target, etc) of the local player, not
	// the hltv camera... These happen to match so it works, but could be the source
	// of bugs... Could turn the observer lerp code into it's own class and have hltv/replay/csplayer 
	// have an instance. 
	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->ShouldInterpolateObserverChanges() )
	{
		pLocalPlayer->StartObserverInterpolation( m_aCamAngle );
	}

	if ( GetMode() == OBS_MODE_ROAMING )
	{
		Vector vOrigin;
		QAngle aAngles;
		float flFov;

		CalcChaseCamView( vOrigin,  aAngles, flFov );
	}
	else if ( GetMode() == OBS_MODE_CHASE )
	{
		CBaseEntity* target = ClientEntityList().GetEnt( m_iTarget1 );
		if ( target && target->IsPlayer() )
		{
			QAngle eyeAngle = target->EyeAngles();
			prediction->SetViewAngles( eyeAngle );
		}
	}

	CBasePlayer *pOldPlayer = ToBasePlayer( ClientEntityList().GetEnt( m_iLastTarget1 ) );
	if ( pOldPlayer )
	{
		// Update view model visibility
		for ( int i = 0; i < MAX_VIEWMODELS; i++ )
		{
			CBaseViewModel *vm = pOldPlayer->GetViewModel( i );
			if ( !vm )
				continue;
			vm->UpdateVisibility();
		}
	}

	m_flLastDistance = m_flDistance;
	m_flLastAngleUpdateTime = -1;

	IGameEvent *event = gameeventmanager->CreateEvent( "hltv_changed_target" );
	if ( event )
	{
		event->SetInt("userid", pLocalPlayer->GetUserID() );
		event->SetInt( "mode", m_nCameraMode );
		event->SetInt( "old_target", m_iLastTarget1 );
		event->SetInt( "obs_target", m_iTarget1 );
		gameeventmanager->FireEventClientSide( event );
	}
}

void CHLTVCamera::SpecNextPlayer( bool bReverse )
{

	// Copy of GetNextObserverSearchStartPoint
	int iDir = bReverse ? -1 : 1; 

	int StartIndex = 1;

	if ( m_iTarget1 > 0 && m_iTarget1 <= gpGlobals->maxClients )
		StartIndex = m_iTarget1;
	// end copy

	int Index = StartIndex;
	bool bFoundValidPlayer = false;

	// copy of FindNextObserverTarget
	do
	{
		Index += iDir;

		// Loop through the clients
		if (Index > gpGlobals->maxClients)
			Index = 1;
		else if (Index < 1)
			Index = gpGlobals->maxClients;

		CBaseEntity * target = UTIL_PlayerByIndex( Index );

		if ( target == nullptr )
			continue;

		if ( !target->IsPlayer() )
			continue;

		CBasePlayer * player = ToBasePlayer( target );

		if ( player->IsEffectActive( EF_NODRAW ) ) // don't watch invisible players
			continue;

		if ( player->m_lifeState == LIFE_RESPAWNABLE ) // target is dead, waiting for respawn
			continue;

		if ( player->m_lifeState == LIFE_DEAD )
			continue;

		// for HLTV replay, them local player is both IsAlive(), and also is an observer because it's HLTV. Checking for IsObserver here seems to be redundant because the dead or specating players are already filtered out
		//if ( player->IsObserver() )
		//	continue;

		if ( player->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		bFoundValidPlayer = true;
		break;	// found next valid player

	} while ( Index != StartIndex );
	// end copy

	if ( bFoundValidPlayer )
	{
		SetPrimaryTarget( Index );

		if ( AutoDirectorState() != AUTODIRECTOR_OFF )
		   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );
	}
}

void CHLTVCamera::SpecPlayerByAccountID( const char *pszPlayerID )
{
	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		CPlayerId playerUID;
		pPlayer->GetPlayerID( &playerUID );

		if ( CPlayerId( pszPlayerID ).GetAccountID() != playerUID.GetAccountID( ) )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			continue;

		SetPrimaryTarget( index );

		if ( AutoDirectorState() != AUTODIRECTOR_OFF )
		   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

		return;
	}
}

void CHLTVCamera::SpecNamedPlayer( const char *szPlayerName )
{
	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		if ( !FStrEq( szPlayerName, pPlayer->GetPlayerName( ) ) )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver( ) && pPlayer->GetTeamNumber( ) != TEAM_SPECTATOR )
			continue;

		SetPrimaryTarget( index );

		if ( AutoDirectorState( ) != AUTODIRECTOR_OFF )
			SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

		return;
	}
}

void CHLTVCamera::SpecPlayerByIndex( int iIndex )
{
	CBasePlayer *pPlayer =	UTIL_PlayerByIndex( iIndex );

	if ( !pPlayer )
		return;

	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		// In HLTV Replay case, just follow the living players explicitly
		if ( !pPlayer->IsAlive() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			return;
	}
	else
	{
		// only follow living players or dedicated spectators. TODO: Fold in the HLTV Replay case; just follow the living players explicitly
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			return;
	}

	if ( GetMode() == OBS_MODE_ROAMING || GetMode() == OBS_MODE_FIXED )
		SetMode( OBS_MODE_IN_EYE );

	SetPrimaryTarget( iIndex );

	if ( AutoDirectorState() != AUTODIRECTOR_OFF )
	   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

	return;
}

void CHLTVCamera::Update()
{
	bool bFastForwardNotification = false;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		int numFastForwardTicks = 0;

		// Get the team score
		if ( CSGameRules() )
		{
			CTeam *tTeam = GetGlobalTeam( TEAM_TERRORIST );
			CTeam *ctTeam = GetGlobalTeam( TEAM_CT );

			int numRoundsCompleted = ( tTeam ? tTeam->Get_Score() : 0 ) + ( ctTeam ? ctTeam->Get_Score() : 0 );
			if ( ( pParameters->m_numRoundStop != ~0 ) && ( numRoundsCompleted >= ( int ) pParameters->m_numRoundStop ) )
			{
				// End of fraction is now reached, just end the playback
				if ( g_pMatchFramework )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
					g_pMatchFramework->CloseSession();
				}
				return;
			}
		}
		
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Skip portions of playback that aren't locking view to the requested account ID
			bool bLockedToRequestedAccount = false;
			for ( int iLockAttempt = 0; iLockAttempt < 2; ++ iLockAttempt )
			{
				if ( m_iTarget1 >= 1 && m_iTarget1 < gpGlobals->maxClients )
				{
					CBaseEntity * target = UTIL_PlayerByIndex( m_iTarget1 );
					if ( target && target->IsPlayer() )
					{
						CBasePlayer * player = ToBasePlayer( target );
						CPlayerId playerUID;
						if ( player && player->GetPlayerID( &playerUID ) && playerUID.IsValid() &&
							( playerUID.GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
						{
							// Check if the player that we are locked to is doing non-interesting stuff
							if ( player->IsEffectActive( EF_NODRAW ) || // don't watch invisible players
								( player->m_lifeState == LIFE_RESPAWNABLE ) || // target is dead, waiting for respawn
								( player->m_lifeState == LIFE_DEAD ) ||
								( player->GetFlags() & FL_FROZEN ) || // skip freezetime
								( player->IsObserver() ) ||
								( player->GetTeamNumber() == TEAM_SPECTATOR ) ||
								( CSGameRules() && CSGameRules()->IsFreezePeriod() ) )
								break; // break to skip some ticks

							bLockedToRequestedAccount = true;
						}
					}
				}

				// If we aren't locked then try locking now and check again
				if ( bLockedToRequestedAccount )
					break;
				// Try locking and loop one more time
				if ( !iLockAttempt )
					SetPrimaryTarget( 0 );
			}

			if ( !bLockedToRequestedAccount && ( spec_overwatch_skip_idle_ticks.GetInt() > numFastForwardTicks ) && pParameters->m_bAnonymousPlayerIdentity )
			{
				numFastForwardTicks = spec_overwatch_skip_idle_ticks.GetInt();
			}
		}

		if ( ( numFastForwardTicks > 1 ) && ( engine->GetDemoPlaybackTimeScale() >= 0.999f ) )
		{
			// Skip some ticks if demo playback is not being slowed down by the user
			engine->ClientCmd_Unrestricted( CFmtStr( "demo_gototick %u R;\n", numFastForwardTicks ) );
			bFastForwardNotification = true;
		}
	}

	if ( m_flAutodirectorPausedTime >= 0.0)
	{
		const float endTime = spec_autodirector_pausetime.GetFloat() + m_flAutodirectorPausedTime;
		if ( gpGlobals->curtime >= endTime)
		{
			HLTVCamera()->SetAutoDirector( AUTODIRECTOR_ON );
			m_flAutodirectorPausedTime = -1.0f;
		}
	}
}

void CHLTVCamera::SpecCameraGotoPos( Vector vecPos, QAngle angAngle, int nPlayerIndex )
{
	m_iCameraMan  = 0;
	m_iLastTarget1 = (nPlayerIndex > 0) ? nPlayerIndex : (GetPrimaryTarget() ? GetPrimaryTarget()->entindex() : m_iLastTarget1);
	SetPrimaryTarget( 0 );

	QAngle oldang = m_aCamAngle;
	QAngle newang;
	newang.x = angAngle[0];
	newang.y = angAngle[1];
	newang.z = oldang.z;

	m_vCamOrigin = vecPos;

	SetMode( OBS_MODE_ROAMING );
	SetCameraAngle( newang );
	engine->SetViewAngles( newang );
	m_flFOV = 90.0f;

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
	if ( pPlayer && event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		gameeventmanager->FireEventClientSide( event );
	}
}

void CHLTVCamera::SpecCameraLerptoPos( const Vector &origin, const QAngle &angles, int nPlayerIndex, float flTime )
{
	m_iCameraMan  = 0;
	m_iLastTarget1 = (nPlayerIndex > 0) ? nPlayerIndex : (GetPrimaryTarget() ? GetPrimaryTarget()->entindex() : m_iLastTarget1);
	SetPrimaryTarget( 0 );
	SetMode( OBS_MODE_ROAMING );
	m_flFOV = 90.0f;

	QAngle oldang = m_aCamAngle;
	QAngle newang;
	newang.x = angles[0];
	newang.y = angles[1];
	newang.z = oldang.z;

	m_bIsSpecLerping = true;

	m_vecSpecLerpOldPos = m_vCamOrigin;
	m_angSpecLerpOldAng = m_aCamAngle;

	m_vecSpecLerpIdealPos = origin;
	m_angSpecLerpIdealAng = angles;

	m_flSpecLerpTime = flTime;
	m_flSpecLerpEndTime = gpGlobals->curtime + flTime;

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
	if ( pPlayer && event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		gameeventmanager->FireEventClientSide( event );
	}
}

void CHLTVCamera::FireGameEvent( IGameEvent * event)
{
	const char *type = event->GetName();

	if ( !engine->IsHLTV() )
	{
		return;	// not in HLTV mode
	}

	// in HLTV mode
	if ( V_strcmp( "game_newmap", type ) == 0 )
	{
		Reset();	// reset all camera settings

		// show spectator UI
		if ( !GetViewPortInterface() )
			return;

		if ( !engine->IsPlayingDemo() )
        {
			// during live broadcast only show black bars
			GetViewPortInterface()->ShowPanel( PANEL_SPECGUI, true );
		}

		SetMode( OBS_MODE_ROAMING );
		SetAutoDirector( CHLTVCamera::AUTODIRECTOR_ON );

		return;
	}

	if ( V_strcmp( "hltv_message", type ) == 0 )
	{
		wchar_t outputBuf[1024];
		const char *pszText = event->GetString( "text", "" );
		
		const char *tmpStr = hudtextmessage->LookupString( pszText );
		const wchar_t *pBuf = g_pLocalize->Find( tmpStr );
		if ( pBuf )
		{
			// Copy pBuf into szBuf[i].
			int nMaxChars = sizeof( outputBuf ) / sizeof( wchar_t );
			wcsncpy( outputBuf, pBuf, nMaxChars );
			outputBuf[nMaxChars-1] = 0;
		}
		else
		{
			g_pLocalize->ConvertANSIToUnicode( tmpStr, outputBuf, sizeof(outputBuf) );
		}

		GetCenterPrint()->Print( ConvertCRtoNL( outputBuf ) );
		return ;
	}

	if ( V_strcmp( "hltv_title", type ) == 0 )
	{
		V_strncpy( m_szTitleText, event->GetString( "text", "" ), sizeof(m_szTitleText) );
		return;
	}

	if ( V_strcmp( "hltv_status", type ) == 0 )
	{
		int nNumProxies = event->GetInt( "proxies" );
		m_nNumSpectators = event->GetInt( "clients" ) - nNumProxies;
		return;
	}

	// after this only auto-director commands follow
	// don't execute them if the autodirector is off and PVS is unlocked
	//( hltv player is controlling the camera )
	if ( !spec_autodirector.GetBool() && !IsPVSLocked() )
		return;
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Whenever a player connects we will force lookup of the required player
			if ( !V_strcmp( "player_connect", type ) ||
				!V_strcmp( "player_connect_full", type ) ||
				!V_strcmp( "player_team", type ) )
			{
				SetPrimaryTarget( 0 );
				return;
			}

			return;	// don't execute further commands if locked to a specific account forced PVS
		}
	}

	if ( m_iCameraMan > 0 )
	{
		return;
	}

	if ( V_strcmp( "hltv_fixed", type ) == 0 )
	{
		m_iCameraMan  = 0;
		
		m_vCamOrigin.x = event->GetInt( "posx" );
		m_vCamOrigin.y = event->GetInt( "posy" );
		m_vCamOrigin.z = event->GetInt( "posz" );

		QAngle angle;
 		angle.x = event->GetInt( "theta" );
		angle.y = event->GetInt( "phi" );
		angle.z = 0; // no roll yet

		if ( m_nCameraMode != OBS_MODE_FIXED )
		{
			SetMode( OBS_MODE_FIXED );
			SetCameraAngle( angle );
			m_flFOV = event->GetFloat( "fov", 90 );
		}

		SetPrimaryTarget( event->GetInt( "target" ) );

		if ( m_iTarget1 == 0 )
		{
			SetCameraAngle( angle );
		}
						
		return;
	}

	if ( V_strcmp( "hltv_chase", type ) == 0 )
	{
		bool bInEye	= event->GetBool( "ineye" );

		// check if we are already in a player chase mode
		bool bIsInChaseMode = (GetMode()==OBS_MODE_IN_EYE)|| (GetMode()==OBS_MODE_CHASE);

		// if we are in auto director or not in a valid chase mode, set new mode now
		if ( spec_autodirector.GetBool() || !bIsInChaseMode )
		{
			SetMode( bInEye?OBS_MODE_IN_EYE:OBS_MODE_CHASE );
		}

		m_iCameraMan  = 0;
				
		m_iTarget2		= event->GetInt( "target2" );
		m_flDistance	= event->GetFloat( "distance", m_flDistance );
		m_flOffset		= event->GetFloat( "offset", m_flOffset );
		m_flTheta		= event->GetFloat( "theta", m_flTheta );
		m_flPhi			= event->GetFloat( "phi", m_flPhi );
		m_flFOV			= event->GetFloat( "fov", 90 );
 		m_flInertia		= event->GetFloat( "inertia", 30.f ) / 10.f;

		// if inertia is not set use standard value
		if ( m_flInertia <= 0 )
			m_flInertia = 3.0f;

		SetPrimaryTarget( event->GetInt( "target1" ) );
							
		return;
	}

}

// this is a cheap version of FullNoClipMove():
void CHLTVCamera::CreateMove( CUserCmd *cmd)
{
	if ( cmd )
	{
		m_LastCmd = *cmd;
	}
}

void CHLTVCamera::SetCameraAngle( QAngle& targetAngle )
{
	m_aCamAngle	= targetAngle;
 	NormalizeAngles( m_aCamAngle );
	m_flLastAngleUpdateTime = gpGlobals->realtime;
}

void CHLTVCamera::SmoothCameraAngle( QAngle& targetAngle )
{
	if ( m_flLastAngleUpdateTime > 0 )
	{
		float deltaTime = gpGlobals->realtime - m_flLastAngleUpdateTime;

		deltaTime = clamp( deltaTime*m_flInertia, 0.01, 1);

		InterpolateAngles( m_aCamAngle, targetAngle, m_aCamAngle, deltaTime );
	}
	else
	{
		m_aCamAngle = targetAngle;
	}

	m_flLastAngleUpdateTime = gpGlobals->realtime;
}

void CHLTVCamera::ToggleChaseAsFirstPerson()
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			if ( GetMode() != OBS_MODE_IN_EYE )
				SetMode( OBS_MODE_IN_EYE );
			SetPrimaryTarget( 0 ); // will force correct computation of the target
			return;	// when playback wants to lock to a specific account force PVS lock to IN_EYE
		}
	}

	if ( GetMode() == OBS_MODE_CHASE )
	{
		SetMode( OBS_MODE_IN_EYE );
	}
	else if ( GetMode() == OBS_MODE_IN_EYE )
	{
		SetMode( OBS_MODE_CHASE );
	}

	if ( AutoDirectorState() != AUTODIRECTOR_OFF )
	   SetAutoDirector( AUTODIRECTOR_OFF ); //SetAutoDirector( AUTODIRECTOR_PAUSED );
}

bool CHLTVCamera::IsPVSLocked()
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
			return true;	// when playback wants to lock to a specific account force PVS lock
	}

	if ( tv_transmitall != nullptr )
	{
		return !tv_transmitall->GetBool();
	}
	else
	{
		//old style, assume locked unless we playback a demo
		return !engine->IsPlayingDemo();
	}
}

void CHLTVCamera::SetAutoDirector( AutodirectorState_t eState )
{
	//m_iCameraMan = 0;
	if ( eState != AUTODIRECTOR_OFF && spec_autodirector_cameraman.GetInt() > 0 )
	{
		// find a cameraman and set m_iCameraMan
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			CPlayerId comparePlayerID;
			if ( pPlayer && pPlayer->GetPlayerID( &comparePlayerID ) )
			{
				// is this played the selected cameraman
				if ( ( uint32 )( spec_autodirector_cameraman.GetInt() ) == comparePlayerID.GetAccountID() )
				{
					// validate that they are a tournament caster
					for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; j++ )
					{
						if ( comparePlayerID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
						{
							if ( pPlayer->IsActiveCameraMan() )
							{
								m_iCameraMan = i;

								CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
								CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
								if ( hudChat && pLocalPlayer && !IsAutoDirectorOn() )
								{
									hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_On" ) );
								
									pLocalPlayer->EmitSound("Vote.Passed");		
								}
								break;
							}
						}
					}
				}
				if ( m_iCameraMan )
					break;
			}
		}
	}

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			eState = AUTODIRECTOR_ON; // force auto-director first person mode
			m_iCameraMan = 0; // force no cameraman
		}
	}

	if ( eState == AUTODIRECTOR_ON)
	{
		spec_autodirector.SetValue( 1 );
	}
	else // OFF or PAUSED
	{
		CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
		CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
		if ( hudChat && pLocalPlayer && m_iCameraMan != 0 && IsAutoDirectorOn() )
		{
			hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_Off" ) );
			
			pLocalPlayer->EmitSound("UI.ButtonRolloverLarge");		
		}

		spec_autodirector.SetValue( 0 );
		m_iCameraMan = 0;
	}

	if  ( eState == AUTODIRECTOR_PAUSED )
	{
		m_flAutodirectorPausedTime = gpGlobals->curtime;
	}
	else
	{
		m_flAutodirectorPausedTime = -1.0f;
	}
		
}

CHLTVCamera::AutodirectorState_t CHLTVCamera::AutoDirectorState() const
{
	if ( IsAutoDirectorOn() )
		return AUTODIRECTOR_ON;

	if ( m_flAutodirectorPausedTime < 0.0f )
		return AUTODIRECTOR_OFF;
	else
		return AUTODIRECTOR_PAUSED;
}

bool CHLTVCamera::IsAutoDirectorOn() const
{
	return spec_autodirector.GetBool();
}

static void list_active_casters( const CCommand &args );
static ConCommand list_active_casters_command( side_registry(), "list_active_casters", list_active_casters, "List currently active casters.", FCVAR_CLIENTDLL | FCVAR_RELEASE |FCVAR_HIDDEN );
static void list_active_casters( const CCommand &args )
{
	if ( !CSGameRules() )
	{
		Msg( "You need to be watching a game!\n" );
		return;
	}

	Msg( "Active Casters:\n" );

	CPlayerId cameraManPlayerID;
	if ( CBasePlayer *pPlayer = HLTVCamera()->GetCameraMan() )
	{
		pPlayer->GetPlayerID( &cameraManPlayerID );
	}

	int nActiveCasters = 0;
	for (int i = 0; i < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; i++ )
	{
		if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ] )
		{
			nActiveCasters++;
			Msg( "%d, ID: %d  %s\n", i, CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ],
				 ( cameraManPlayerID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ] ) ? "*Camera Man*" : "" );
		}
	}
	if ( nActiveCasters == 0 )
	{
		Msg( "None.\n" );
	}
}

#undef CHASE_CAM_DISTANCE
#undef WALL_OFFSET

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

ConVar snd_deathcam_replay_mix( side_registry(), "snd_deathcam_replay_mix", "0", 0, "When set to non-0, client switches to DeathCam_Replay_Mix mixgroup during deathcam replay" );
ConVar spec_replay_review_sound( side_registry(), "spec_replay_review_sound", "1", FCVAR_CLIENTDLL, "When set to non-0, a sound effect is played during Killer Replay" );
ConVar spec_replay_rate_slowdown( side_registry(), "spec_replay_rate_slowdown", "1", FCVAR_CLIENTDLL, "The part of Killer Replay right before death is played at this rate" );
ConVar spec_replay_rate_slowdown_length( side_registry(), "spec_replay_rate_slowdown_length", "0.5", FCVAR_CLIENTDLL, "The part of Killer Replay right before death is played at this rate" );
ConVar spec_replay_fadein( side_registry(), "spec_replay_fadein", "0.75", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to visually fade into replay, or into real-time after replay" ); // Original tuning: 1.0; MattWood 12/3/15: 0.75
ConVar spec_replay_fadeout( side_registry(), "spec_replay_fadeout", "0.5", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to visually fade out of replay, or out of real-time before replay" );
ConVar spec_replay_sound_fadein( side_registry(), "spec_replay_sound_fadein", "1", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to fade in the audio before or after replay" );
ConVar spec_replay_sound_fadeout( side_registry(), "spec_replay_sound_fadeout", "1.5", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to fade out the audio before or after replay" );
ConVar spec_replay_cache_ragdolls( side_registry(), "spec_replay_cache_ragdolls", "1", FCVAR_CLIENTDLL, "when set to 0, ragdolls will settle dynamically before and after Killer Replay" );
ConVar spec_replay_others_experimental( side_registry(), "spec_replay_others_experimental", "0", FCVAR_CLIENTDLL, "Replay the last death of the round, if possible. Disabled on official servers by default. Experimental." );
ConVar spec_replay_autostart( side_registry(), "spec_replay_autostart", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-start Killer Replay when available" ); // in game/csgo/scripts/game_options.txt
ConVar spec_replay_autostart_delay( side_registry(), "spec_replay_autostart_delay", "1.5", FCVAR_CLIENTDLL, "Time in freeze panel before switching to Killer Replay automatically" ); // original tuning: 2.4; MattWood 12/3/15: 1.5
ConVar spec_replay_victim_pov( side_registry(), "spec_replay_victim_pov", "0", FCVAR_CLIENTDLL, "Killer Replay - replay from victim's point of view (1); the default is killer's (0). Experimental." );
CHltvReplaySystem g_HltvReplaySystem;
extern void CS_FreezePanel_OnHltvReplayButtonStateChanged();
void CS_FreezePanel_OnHltvReplayButtonStateChanged()
{
    /*
    SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
    if ( pPanel )
    {
        pPanel->OnHltvReplayButtonStateChanged();
    }*/
}
CHltvReplaySystem::CHltvReplaySystem()
{
	m_nHltvReplayDelay = 0;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_flHltvLastRequestRealTime = 0;
	m_bWaitingForHltvReplayTick = false;
	m_flStartedWaitingForHltvReplayTickRealTime = 0;
	m_bDemoPlayback = false;
	m_bListeningForGameEvents = false;
	m_nDemoPlaybackStartAt = 0;
	m_nDemoPlaybackStopAt = 0;
	m_nCurrentPlaybackTick = 0;
	m_flCurrentPlaybackTime = 0.0f;
	m_nDemoPlaybackXuid = 0;
	m_flReplayVideoFadeAmount = 0.0f;
	m_flReplaySoundFadeAmount = 0.0f;
	m_nExperimentalEvents = 0;
	m_flFadeinStartRealTime = -1000;
	m_flFadeoutEndTime = -1000;
	m_DelayedReplay.Reset();
	m_nHltvReplayBeginTick = -1;
	m_nHltvReplayPrimaryTarget = 0;
	m_bHltvReplayButtonTimedOut = false;
}

CHltvReplaySystem::~CHltvReplaySystem()
{

}

int CL_GetHltvReplayDelay() { return g_HltvReplaySystem.GetHltvReplayDelay(); }

void CHltvReplaySystem::EmitTimeJump()
{
	GetHud().OnTimeJump();
	CBasePlayer::OnTimeJumpAllPlayers();
	//GetHud().UpdateHud( true ); // may call SFUniqueAlerts::ShowHltvReplayAlertPanel
	//GetHud().ProcessInput( true );
	//GetHud().OnTimeJump();
}

void CHltvReplaySystem::StopHltvReplay()
{
	if ( m_nHltvReplayDelay )
	{
		ks::net::CSVCMsg_HltvReplay msg;
		OnHltvReplay( msg );
	}
}

extern bool g_bForceCLPredictOff;
void CHltvReplaySystem::OnHltvReplay( const ks::net::CSVCMsg_HltvReplay  &msg )
{
	int nNewReplayDelay = msg.delay;
	DevMsg( "%.2f OnHltvReplay %s\n", gpGlobals->curtime, nNewReplayDelay ? "START" : "END" );

	g_bForceCLPredictOff = ( nNewReplayDelay != 0 );

	static int s_nReplayLayerIndex = engine->GetMixLayerIndex( "ReplayLayer" );

	if ( m_nHltvReplayDelay )
	{
		if ( !nNewReplayDelay )
		{
			// we're about to go out of HLTV replay. Current time is delayed replay time.
			if ( CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer() )
			{
				CBaseEntity::StopSound( -1, "Deathcam.Review_Start" );
				if ( spec_replay_review_sound.GetBool() )
					pPlayer->EmitSound( "Deathcam.Review_End" );
			}
			engine->SetMixLayerLevel( s_nReplayLayerIndex, 0.0f );
			IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}
			if ( snd_deathcam_replay_mix.GetBool() )
			{
				ConVar *pSoundmixer = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
				pSoundmixer->Revert();
			}
		}
		StopFadeout();// Msg( "%.2f Fadeout not stopped (%d,%d,%.2f)\n", gpGlobals->curtime, m_nHltvReplayDelay, nNewReplayDelay, m_flFadeoutEndTime );
	}
	else
	{
		if ( nNewReplayDelay )
		{
			m_DelayedReplay.Invalidate(); // we don't do delayed replay if we are about to do the replay
			CacheRagdollBones();
			// we're about to go into HLTV replay. Current time is real time.
			HLTVCamera()->SetPrimaryTarget( m_nHltvReplayPrimaryTarget = msg.primary_target );
			HLTVCamera()->SetMode( OBS_MODE_IN_EYE );
			GetHud().ResetHUD();
			/*IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}*/
			if ( spec_replay_review_sound.GetBool() )
			{
				if ( CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer() )
				{
					EmitSound_t params;
					params.m_pSoundName = "Deathcam.Review_Start";
					params.m_bWarnOnDirectWaveReference = true;
					params.m_hSoundScriptHash = soundemitterbase->HashSoundName( params.m_pSoundName );
					CLocalPlayerFilter filter;
					CBaseEntity::EmitSound( filter, -1, params );
				}
			}
			if ( snd_deathcam_replay_mix.GetBool() )
			{
				ConVar *pSoundmixer = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
				pSoundmixer->SetValue( "DeathCam_Replay_Mix" );
			}
			engine->SetMixLayerLevel( s_nReplayLayerIndex, 1.0f );
			if ( int nSlowdownRatio = msg.replay_slowdown_rate )
			{
				m_flHltvReplaySlowdownRate = 1.0f / nSlowdownRatio;
				m_nHltvReplaySlowdownBegin = msg.replay_slowdown_begin;
				m_nHltvReplaySlowdownEnd = msg.replay_slowdown_end;
			}
			else
			{
				m_nHltvReplaySlowdownBegin = 0;
				m_nHltvReplaySlowdownEnd = 0;
				m_flHltvReplaySlowdownRate = 1.0f;
			}

			m_nExperimentalEvents |= EE_REPLAY_STARTED;
			// currently, Hltv Replay Active (m_nHltvReplayDelay!=0)
			StartFadeout( msg.replay_stop_at * gpGlobals->interval_per_tick, spec_replay_fadeout.GetFloat() );
		}
		else
		{
			// Hltv replay is NOT active. Requested NO Hltv replay . There's nothing to do.
			StopFadeout(); //Msg( "%.2f Fadeout not stopped-2 (%d,%d,%.2f)\n", gpGlobals->curtime, m_nHltvReplayDelay, nNewReplayDelay, m_flFadeoutEndTime );
		}
	}
	StartFadein( gpGlobals->realtime );
	m_flHltvLastRequestRealTime = gpGlobals->realtime;
	m_bWaitingForHltvReplayTick = true;
	m_flStartedWaitingForHltvReplayTickRealTime = gpGlobals->realtime;
	m_nHltvReplayBeginTick = gpGlobals->tickcount; 
	debugoverlay->ClearAllOverlays();

	m_nHltvReplayDelay = nNewReplayDelay;
	//enginesound->SetReplayMusicGain( nNewReplayDelay ? 0.0f : 1.0f ); 

	m_nHltvReplayStopAt = msg.replay_stop_at; // we'll stop at this current tick, more or less; we'll jump back in time and stop here
	m_nHltvReplayStartAt = msg.replay_start_at - nNewReplayDelay;
	// I'd like to call ParticleMgr()->RemoveAllEffects(); but there are still some entities referencing the particles in question
	view->FreezeFrame( 0.0f );
	Update();

	EmitTimeJump();
}

void CHltvReplaySystem::OnHltvReplayTick()
{
	contract_assert( m_bWaitingForHltvReplayTick );
	m_bWaitingForHltvReplayTick = false;
	m_nHltvReplayBeginTick = gpGlobals->tickcount;
	Update();
	CS_FreezePanel_OnHltvReplayButtonStateChanged();
}

float ReplayFade( float flCurTime, int nStartAt, int nStopAt, float flFadeIn, float flFadeOut )
{
	if ( flFadeIn > 0.001f )
	{
		float flTimeIn = flCurTime - nStartAt * gpGlobals->interval_per_tick;
		if ( flTimeIn < flFadeIn )
		{
			return 1.0f - Max( 0.0f, flTimeIn / flFadeIn );
		}
	}

	if ( flFadeOut > 0.001f )
	{
		float flTimeOut = nStopAt * gpGlobals->interval_per_tick - flCurTime;
		if ( flTimeOut < flFadeOut )
		{
			return 1.0f - Max( 0.0f, flTimeOut / flFadeOut );
		}
	}
	return 0.0f;
}

void CHltvReplaySystem::Update()
{
	float flCurTime = gpGlobals->curtime;
	float flPrevReplaySoundFadeAmount = m_flReplaySoundFadeAmount;
	//Assert( enginesound->GetReplaySoundFade() == m_flReplaySoundFadeAmount );

	m_flReplayVideoFadeAmount = m_flReplaySoundFadeAmount = 0.0f; // there are no replay post effects going on, by default

	if ( !m_nHltvReplayDelay && gpGlobals->tickcount > m_nHltvReplayBeginTick + 5 )
	{
		PurgeRagdollBoneCache();
	}

	if ( m_bWaitingForHltvReplayTick )
	{
		// we're in this state when server let us know that Hltv replay is either about to stop or start, but we haven't received the frame update yet and the new timeline hasn't started yet
		// whether we're going in or out of replay, let's darken the screen. It's possible that we'll miss that message, though. If we disconnect or reconnect, we'll want to reset this flag
		m_flReplayVideoFadeAmount = m_flReplaySoundFadeAmount = 1.0f; // fade everything out completely		

		if ( gpGlobals->realtime - m_flStartedWaitingForHltvReplayTickRealTime > 0.75f )
		{
			Msg( "Killer replay stuck waiting for tick message for %.2fs, requesting update\n", gpGlobals->realtime - m_flHltvLastRequestRealTime );
			m_nExperimentalEvents |= EE_REPLAY_STUCK;

			m_flStartedWaitingForHltvReplayTickRealTime = gpGlobals->realtime;
			CCLCMsg_HltvReplay_t msgReplay;
			msgReplay.request = ks::net::REPLAY_EVENT_STUCK_NEED_FULL_UPDATE; // request full update
			engine->SendMessageToServer( &msgReplay );
		}
	}
	else if ( m_bDemoPlayback )
	{
		if ( m_nDemoPlaybackStartAt < m_nDemoPlaybackStopAt )
		{
			m_flReplayVideoFadeAmount = ReplayFade( m_nCurrentPlaybackTick * gpGlobals->interval_per_tick + flCurTime - m_flCurrentPlaybackTime, m_nDemoPlaybackStartAt, m_nDemoPlaybackStopAt - 1, spec_replay_fadein.GetFloat(), spec_replay_fadeout.GetFloat() );
			m_flReplaySoundFadeAmount = ReplayFade( m_nCurrentPlaybackTick * gpGlobals->interval_per_tick + flCurTime - m_flCurrentPlaybackTime, m_nDemoPlaybackStartAt, m_nDemoPlaybackStopAt - 1, spec_replay_sound_fadein.GetFloat(), spec_replay_sound_fadeout.GetFloat() );
		}
	}
	else
	{
		if ( m_flFadeinStartRealTime > 0.0f )
		{
			float flTimeIn = gpGlobals->realtime - m_flFadeinStartRealTime;
			bool bFadeInStop = true;

			if ( flTimeIn < m_flFadeinDuration )
			{
				m_flReplayVideoFadeAmount = Max( m_flReplayVideoFadeAmount, 1.0f - Max( 0.0f, flTimeIn / Max( m_flFadeinDuration, 0.01f ) ) );
				bFadeInStop = false;
			}
			
			if ( flTimeIn < spec_replay_sound_fadein.GetFloat() )
			{
				m_flReplaySoundFadeAmount = Max( m_flReplaySoundFadeAmount, 1.0f - Max( 0.0f, flTimeIn / Max( spec_replay_sound_fadein.GetFloat(), 0.01f ) ) );
				bFadeInStop = false;
			}

			if ( bFadeInStop )
				m_flFadeinStartRealTime = -1000;
		}

		if ( m_flFadeoutEndTime > 0.0f )
		{
			float flTimeLeft = m_flFadeoutEndTime - gpGlobals->curtime;
			if ( flTimeLeft < m_flFadeoutDuration )
			{
				m_flReplayVideoFadeAmount = Max( m_flReplayVideoFadeAmount, 1 - Max( 0.0f, flTimeLeft / Max( m_flFadeoutDuration, 0.01f ) ) );
				// Msg( "%.2f +%.2f fade %.2f\n", gpGlobals->curtime, flTimeLeft, m_flReplayVideoFadeAmount ); // replayfade
			}
			if ( flTimeLeft < spec_replay_sound_fadeout.GetFloat() )
			{
				m_flReplaySoundFadeAmount = Max( m_flReplaySoundFadeAmount, 1 - Max( 0.0f, flTimeLeft / Max( spec_replay_sound_fadeout.GetFloat(), 0.01f ) ) );
			}

			if ( flTimeLeft < -1.0f )
			{
				// we're in full fade-out for over 1 second. All fadeouts must be very short to avoid disorienting the player. This may only happen when something went wrong: disconnect, dropped connection, server failed to reply to a request... Just stop it
				DevMsg( "%.2f Aborting fadeout\n", gpGlobals->curtime );
				m_flFadeoutEndTime = -1000;
				m_flFadeinStartRealTime = gpGlobals->realtime;
				m_flFadeinDuration = .3f; // fade in quickly
			}
		}
	}

	if ( m_DelayedReplay.IsValid() )
	{
		if ( gpGlobals->curtime >= m_DelayedReplay.flTime )
		{
			if ( IsHltvReplayFeatureEnabled() ) // we checked the button enabled when we set up this delayed replay
				RequestHltvReplay( m_DelayedReplay );
			m_DelayedReplay.Invalidate();
		}
	}

	bool bLocalPlayerChanged = m_LocalPlayer.Update();
	bool bHltvReplayButtonTimeOutChanged = UpdateHltvReplayButtonTimeOutState();
	if ( bLocalPlayerChanged || bHltvReplayButtonTimeOutChanged )
	{
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
		if ( m_LocalPlayer.m_bLastSeenAlive && !m_nHltvReplayDelay ) // || m_bHltvReplayButtonTimedOut would cancel pending replay start, but we already don't allow replays right before time-out events
			CancelDelayedHltvReplay(); // we can't have a replay if we see the player alive in real-time timeline
	}
	if ( m_nHltvReplayDelay )
	{
		// set the primary target
		int nPrevTarget = HLTVCamera()->GetCurrentTargetEntindex(), nNewTarget = m_nHltvReplayPrimaryTarget;
		if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( nNewTarget ) )
		{
			if ( !pPlayer->IsAlive() )
			{
				if ( CBaseEntity *pKillerSpectating = pPlayer->GetObserverTarget() )
				{
					nNewTarget = pKillerSpectating->entindex();
				}
			}
			if ( nNewTarget != nPrevTarget )
			{
				HLTVCamera()->SetPrimaryTarget( nNewTarget );
			}
		}

		if ( !m_bWaitingForHltvReplayTick && gpGlobals->tickcount > m_nHltvReplayStopAt + 32 )
		{
			// time to ask server to stop replay, again
			RequestCancelHltvReplay( true );
			// .. and ask the server again in 32 ticks, if this packet is lost, too
			m_nHltvReplayStopAt = gpGlobals->tickcount;
		}
	}

	if ( flPrevReplaySoundFadeAmount != m_flReplaySoundFadeAmount )
	{
		SetReplaySoundMixLayer( m_flReplaySoundFadeAmount );
	}
}

void CHltvReplaySystem::PurgeRagdollBoneCache()
{
	if ( int nCount = ( int )m_mapCachedRagdollBones.size() )
	{
		// destroy all unclaimed bones
		m_mapCachedRagdollBones.clear();
		DevMsg( "%d ragdoll states purged\n", nCount );
	}
}

bool CHltvReplaySystem::IsFadeoutActive()
{
	return m_flFadeoutEndTime > 0.0f && gpGlobals->curtime > m_flFadeoutEndTime - m_flFadeoutDuration; // fadeout is requested and has already started
}

bool CHltvReplaySystem::IsFadeoutFinished()
{
	return gpGlobals->curtime >= m_flFadeoutEndTime;
}

void CHltvReplaySystem::OnLevelShutdown()
{
	m_bWaitingForHltvReplayTick = false;
	StopFades();
	StopHltvReplay();
	Update();
}

void CHltvReplaySystem::StopFadeout()
{
	if ( m_flFadeoutEndTime > 0 )
		DevMsg( "%.2f Stopping fadeout\n", gpGlobals->curtime );

	SetReplaySoundMixLayer( 0.0f );
	m_flFadeoutEndTime = -1000;
}

//ConVar spec_replay_sound_fade( "spec_replay_sound_fade", "2", /*FCVAR_CLIENTDLL | FCVAR_RELEASE*/ 0, "0 : fade everything out at replay transition; 1 : use ReplayBlinkLayer for the replay sound transition; 2: use both ReplayLayer and ReplayBlinkLayer for replay sound transition" );

void CHltvReplaySystem::SetReplaySoundMixLayer( float flFade )
{
	static int s_nReplayBlinkLayerIndex = engine->GetMixLayerIndex( "ReplayBlinkLayer" );
	engine->SetMixLayerLevel( s_nReplayBlinkLayerIndex, flFade );
	//enginesound->SetReplaySoundFade( m_flReplaySoundFadeAmount );
}

void CHltvReplaySystem::StartFadeout( float flTimeEnd, float flDuration )
{
	DevMsg( "%.2f Starting fadeout %.2f->%.2f\n", gpGlobals->curtime, flTimeEnd, flDuration );
	m_flFadeoutEndTime = flTimeEnd;
	m_flFadeoutDuration = flDuration;
}

void CHltvReplaySystem::StartFadein( float flStartRealTime )
{
	m_flFadeinStartRealTime = flStartRealTime;
	m_flFadeinDuration = spec_replay_fadein.GetFloat();
}

void CHltvReplaySystem::StopFades()
{
	StopFadeout();
	m_flFadeinStartRealTime = -1000;
}

void CHltvReplaySystem::OnLevelInit()
{
	m_DelayedReplay.Reset();
	m_bWaitingForHltvReplayTick = false;
	m_flHltvLastRequestRealTime = 0;
	m_flCurrentPlaybackTime = 0;
	StopFades();
	contract_assert( !m_nHltvReplayDelay ); // we MAY be in demo playback
	if ( !m_bListeningForGameEvents )
	{
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "bot_takeover" );
		ListenForGameEvent( "player_death" );
		m_bListeningForGameEvents = true; // now we're listening
	}
}

void CHltvReplaySystem::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();
	if ( !V_strcmp( pEventName, "round_start" ) )
	{
		m_nExperimentalEvents = 0;
		CancelDelayedHltvReplay();
	}
	else if ( m_DelayedReplay.IsValid() && !V_strcmp( pEventName, "bot_takeover" ) )
	{
		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
		int nEventUserID = event->GetInt( "userid", -1 );

		if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE && pLocalPlayer->GetUserID() == nEventUserID )
		{
			int nBotId = event->GetInt( "botid" );
			CBasePlayer *pBot = UTIL_PlayerByUserId( nBotId );
			DevMsg( "%.2f Aborting hltv replay scheduled @%.2f because %s (%d) took over bot %s (%d)\n", gpGlobals->curtime, m_DelayedReplay.flTime, pLocalPlayer->GetPlayerName(), nEventUserID, pBot ? pBot->GetPlayerName() : "(null)", nBotId );
			CancelDelayedHltvReplay();
		}
	}
	else if ( !V_strcmp( pEventName, "player_death" ) )
	{
		OnPlayerDeath( event );
	}
}

float CHltvReplaySystem::GetHltvReplayDistortAmount()const
{
	return m_nHltvReplayDelay ? 1.0f : 0.0f; // no distortion in highlights or lowlights, just the fade in/out
}

void CHltvReplaySystem::OnDemoPlayback( bool bPlaying )
{ 
	m_bDemoPlayback = bPlaying;
	if ( !bPlaying )
	{
		if ( !m_nHltvReplayDelay )
		{
			StopFades();
		}
		m_nDemoPlaybackXuid = 0;
		m_bDemoPlaybackLowLights = false;
	}
	// by default, we don't know if we need fade in demo playback
	m_nDemoPlaybackStartAt = 0;
	m_nDemoPlaybackStopAt = 0;
	m_nCurrentPlaybackTick = 0;
	m_flCurrentPlaybackTime = gpGlobals->curtime;
	EmitTimeJump();
}

void CHltvReplaySystem::SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights )
{
	m_nDemoPlaybackXuid = xuid;
	m_bDemoPlaybackLowLights = bLowlights;
}

bool CHltvReplaySystem::IsDemoPlaybackXuidOther()const
{
	return uint32( m_nDemoPlaybackXuid ) != 0;
}

CBasePlayer *CHltvReplaySystem::GetDemoPlaybackPlayer()
{
	if ( CBasePlayer *pPlayer = m_DemoPlaybackPlayer.Get() )
	{
		CPlayerId playerUid;
		if ( pPlayer->GetPlayerID(&playerUid) )
		{
			if ( playerUid.GetAccountID() == uint32( m_nDemoPlaybackXuid ) )
				return pPlayer;
		}
	}
	m_DemoPlaybackPlayer.Term();

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( i ) )
		{
			CPlayerId playerUid;
			if ( pPlayer->GetPlayerID( &playerUid ) )
			{
				if ( playerUid.GetAccountID() == uint32( m_nDemoPlaybackXuid ) )
				{
					m_DemoPlaybackPlayer.Set( pPlayer );
					return pPlayer;
				}
			}
		}
	}
	return nullptr;
}

void CHltvReplaySystem::SetDemoPlaybackFadeBrackets( int nCurrentPlaybackTick, int nStartAt, int nStopAt )
{
	contract_assert( m_bDemoPlayback );
	if ( nCurrentPlaybackTick + 320 < nStartAt && nStartAt != m_nDemoPlaybackStartAt )
	{
		// we're starting a jump forward in time. The jump is long (over 320 ticks), so it may take some time, and we don't want the sounds currently in the queue to randomly play
		enginesound->StopAllSounds( true );
	}

	m_nDemoPlaybackStartAt = nStartAt;
	m_nDemoPlaybackStopAt = nStopAt;
	m_nCurrentPlaybackTick = nCurrentPlaybackTick;
	m_flCurrentPlaybackTime = gpGlobals->curtime;
}

bool CHltvReplaySystem::IsHltvReplayFeatureEnabled()
{
	if ( g_bEngineIsHLTV )
		return false; // no replay during replay or GOTV

	static ConVarRef spec_replay_enable( "spec_replay_enable" );
	return spec_replay_enable.GetBool();
}

bool CHltvReplaySystem::IsHltvReplayButtonEnabled()
{
	if ( !IsHltvReplayFeatureEnabled() )
		return false;

	static ConVarRef spec_replay_rate_limit( "spec_replay_rate_limit" );

	if ( m_LocalPlayer.m_bLastSeenAlive ) // must be dead to allow replay
		return false;

	// Note: player death time may not be networked yet, so current pPlayer->GetDeathTime() may not be up to date

	if ( m_bHltvReplayButtonTimedOut )
		return false;

	if ( gpGlobals->curtime > m_DelayedReplay.flEventTime + GetReplayMessageTime() )
		return false;// did we die too long ago ? Then we can't replay

	if ( gpGlobals->realtime < m_flHltvLastRequestRealTime + spec_replay_rate_limit.GetFloat() ) // rate-limit mashing the Replay button
		return false;

	return true;
}

void CHltvReplaySystem::CancelDelayedHltvReplay()
{
	StopFadeout(); // fadeins are fine, leave them be
	// abort the pending death replay, stop all fades because the user took over a bot
	if ( m_DelayedReplay.IsValid() )
	{
		m_DelayedReplay.Stop();
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
	}
}

void CHltvReplaySystem::RequestHltvReplayDeath()
{
	ReplayParams_t rp;
	rp.nRequest = ks::net::REPLAY_EVENT_DEATH;
	RequestHltvReplay( rp );
}

void CHltvReplaySystem::RequestHltvReplay( const ReplayParams_t &replay )
{
	CCLCMsg_HltvReplay_t msgReplay;
	msgReplay.request = replay.nRequest;
	float flSlowdown = spec_replay_rate_slowdown.GetFloat();
	if ( flSlowdown != 1.0f )
	{
		float flSlowdownLength = spec_replay_rate_slowdown_length.GetFloat();
		if ( flSlowdownLength > 0.01f && flSlowdownLength < 5.0f )
		{
			msgReplay.slowdown_length = flSlowdownLength;
			msgReplay.slowdown_rate = flSlowdown;
		}
	}
	if ( replay.nRequest != ks::net::REPLAY_EVENT_DEATH )
	{
		msgReplay.primary_target_ent_index = replay.nPrimaryTargetEntIndex;
		msgReplay.event_time = replay.flEventTime;
	}

	// if we want to always see from victim's perspective, always set the primary target to victim
	if ( spec_replay_victim_pov.GetBool() && replay.nPrimaryVictimEntIndex > 0 )
	{
		msgReplay.primary_target_ent_index = replay.nPrimaryVictimEntIndex;
	}

	engine->SendMessageToServer( &msgReplay );

	StartFadein( gpGlobals->realtime + 1.0f ); // if we don't get hltv replay message in 1 second, it means we couldn't start replay normally, and it's time to fade out
	StopFadeout();

	m_flHltvLastRequestRealTime = gpGlobals->realtime;
	m_nExperimentalEvents |= EE_REPLAY_REQUESTED;
}

void CHltvReplaySystem::RequestCancelHltvReplay( bool bSuppressFadeout )
{
	if ( GetHltvReplayDelay() )
	{
		CCLCMsg_HltvReplay_t msgReplay;
		msgReplay.request = 0; // cancel replay
		engine->SendMessageToServer( &msgReplay );
		if ( bSuppressFadeout )
		{
			StopFadeout();
		}
		else
		{
			if ( !IsFadeoutActive() )
			{// if there's no other fadeout happening now, start fading out for cancel; pretend we're already .1 seconde into fadeout
				StartFadeout( gpGlobals->curtime + m_flFadeoutDuration, 0.3f );// approximately how much it'll take for server to come back with the cancellation
			}
		}
		m_nExperimentalEvents |= EE_REPLAY_CANCELLED;
		DevMsg( "%.2f Replay Cancel requested (%d,%d..%d)\n", gpGlobals->curtime, gpGlobals->tickcount, m_nHltvReplayStartAt, m_nHltvReplayStopAt );
	}
	else
	{
		DevMsg( "%.2f Replay Cancel request failed, not in replay now\n", gpGlobals->curtime );
	}
}

bool IsPlayerTeamDead( int nPlayerEntIndex )
{
	if ( CBasePlayer* pPrincipalPlayer = UTIL_PlayerByIndex( nPlayerEntIndex ) )
	{
		int nTeam = pPrincipalPlayer->GetTeamNumber();
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			if ( i != nPlayerEntIndex )
			{
				if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( i ) )
				{
					contract_assert( pPlayer != pPrincipalPlayer );
					if ( pPlayer->GetTeamNumber() == nTeam )
					{
						if ( pPlayer->IsAlive() )
							return false;
					}
				}
			}
		}
		return true;
	}
	else
		return false; // something's wrong - can't find a team of a non-existing player
}

void CHltvReplaySystem::OnPlayerDeath( IGameEvent *event )
{
	if ( m_DelayedReplay.IsValid() || GetHltvReplayDelay() )
	{
		// we're already replaying or preparing to replay. Let's not react to new player deaths
		return;
	}

	int iPlayerIndexVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
	int iPlayerIndexKiller = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
	{
		DevMsg( "OnPlayerDeath: Cannot replay without local player\n" );
		return;
	}
	bool isLocalVictim = pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex(), isReplayAvailable = iPlayerIndexKiller && iPlayerIndexKiller != iPlayerIndexVictim && !event->GetBool( "noreplay", false );

	UpdateHltvReplayButtonTimeOutState();

	if ( !isReplayAvailable || m_bHltvReplayButtonTimedOut )
		return; // no need to react to anything if replay isn't available for this death

	if ( isLocalVictim )
	{
		m_LocalPlayer.m_bLastSeenAlive = false; // we can assume the local player is dead and unlock replays. The next Update may turn it back on for a few frames, but then it'll go back to dead
		if ( iPlayerIndexVictim != iPlayerIndexKiller ) // we don't replay suicides
		{
			m_DelayedReplay.bFinalKillOfRound = false;
			if ( PrepareHltvReplayCountdown() )
			{
				m_DelayedReplay.nRequest = ks::net::REPLAY_EVENT_DEATH;
			}
		}
	}
	else if (
		iPlayerIndexKiller <= MAX_PLAYERS // the killer must be an actual player (not e.g. a bomb) for replay to make sense
		&& !m_LocalPlayer.m_bLastSeenAlive && spec_replay_autostart.GetBool()
		&& spec_replay_others_experimental.GetBool() && IsPlayerTeamDead( iPlayerIndexVictim )
		)
	{
		int nLocalPlayerTeam = pLocalPlayer->GetTeamNumber();
		if ( nLocalPlayerTeam == TEAM_CT || nLocalPlayerTeam == TEAM_TERRORIST )
		{
			if ( CBaseEntity *pTarget = pLocalPlayer->GetObserverTarget() )
			{
				int nObserverTarget = pTarget->entindex();
				if ( nObserverTarget != iPlayerIndexKiller && nObserverTarget != iPlayerIndexVictim ) // 
				{
					m_DelayedReplay.bFinalKillOfRound = true;
					// EXPERIMENT: showing death of other players to people
					if ( PrepareHltvReplayCountdown() )
					{
						// we're dead anyway... request a replay, it may be interesting
						m_DelayedReplay.nRequest = ks::net::REPLAY_EVENT_GENERIC;
						//extern void CS_FreezePanel_ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim );
						//CS_FreezePanel_ResetDamageText( iPlayerIndexKiller, iPlayerIndexVictim );
					}
				}
			}
		}
	}

	if ( isLocalVictim || m_DelayedReplay.nRequest >= 0 )
	{// Some interesting event happened, and we are not replaying anything else. Store some stats that may or may not be useful for later replay
		m_DelayedReplay.nPrimaryTargetEntIndex = iPlayerIndexKiller;
		m_DelayedReplay.flEventTime = gpGlobals->curtime;
		m_DelayedReplay.flTime = gpGlobals->curtime + spec_replay_autostart_delay.GetFloat();
		m_DelayedReplay.nPrimaryVictimEntIndex = iPlayerIndexVictim;
		m_DelayedReplay.bPrimaryVictimIsLocalPlayer = isLocalVictim;
	}
}

void CHltvReplaySystem::OnLocalPlayerRespawning()
{
}

bool CHltvReplaySystem::PrepareHltvReplayCountdown()
{
	bool bCountdownStarted = false;
	if ( !GetHltvReplayDelay() )
	{
		m_nExperimentalEvents |= EE_REPLAY_OFFERED;

		// if we have to show freezeframe before deathcam, this is the place to request it
		if ( IsHltvReplayFeatureEnabled() && spec_replay_autostart.GetBool() )
		{
			bCountdownStarted = true;
			m_nExperimentalEvents |= EE_REPLAY_AUTOMATIC;

			float flDelay = spec_replay_autostart_delay.GetFloat();
			m_DelayedReplay.flTime = gpGlobals->curtime + flDelay;
			if ( !IsFadeoutActive() )
			{
				// use fadeout 
				StartFadeout( gpGlobals->curtime + flDelay, Min( flDelay, spec_replay_fadeout.GetFloat() ) );
			}
			// DevMsg( "%.2f Replay: starting to fade out\n", gpGlobals->curtime ); // replayfade
		}
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
	}
	return bCountdownStarted;
}

ConVar r_replay_post_effect( side_registry(), "r_replay_post_effect", "-1", FCVAR_CHEAT );

bool CHltvReplaySystem::WantsReplayEffect() const
{
	if ( r_replay_post_effect.GetInt() == -1 )
	{
		return ( g_HltvReplaySystem.GetHltvReplayDelay() != 0 );
	}
	else
		return r_replay_post_effect.GetBool();
}

void CHltvReplaySystem::CacheRagdollBones()
{
	PurgeRagdollBoneCache();
	if ( !spec_replay_cache_ragdolls.GetBool() )
		return;
	CBaseEntityIterator iterator;
	CBaseEntity *pEnt;
	int nRagdollsCached = 0;
	while ( ( pEnt = iterator.Next() ) != nullptr )
	{
		if ( CCSRagdoll * pRagdoll = dynamic_cast< CCSRagdoll * >( pEnt ) )
		{
			if ( int nEntIndex = pRagdoll->entindex() )
			{
				if ( pRagdoll->m_pRagdoll )
				{
					nRagdollsCached++;
					int nBones = pRagdoll->GetModelPtr()->numbones();
					int nBodyParts = pRagdoll->m_pRagdoll->RagdollBoneCount();
					CachedRagdollBones_t *pCachedRagdoll = ( CachedRagdollBones_t * )MemAlloc_AllocAlignedUnattributed( sizeof( CachedRagdollBones_t ) + sizeof( matrix3x4a_t ) * ( nBones + nBodyParts + 1 ), 16 );
					pCachedRagdoll->bAllAsleep = true;
					pCachedRagdoll->nBones = nBones;
					pCachedRagdoll->nBodyParts = nBodyParts;
					matrix3x4a_t *pBones = pCachedRagdoll->GetBones();
					pBones[ nBones ].SetColumn( Vector( 11, 22, 33 ), X_AXIS );
					pRagdoll->SetupBones( pBones, nBones, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
					matrix3x4a_t *pBodyParts = pCachedRagdoll->GetBodyParts();
					for ( int i = 0; i < nBodyParts; i++ )
					{
						if ( IPhysicsObject* pObj = pRagdoll->m_pRagdoll->RagdollPhysicsObject( i ) )
						{
							pObj->GetPositionMatrix( &pBodyParts[ i ] );
							if ( !pObj->IsAsleep() )
								pCachedRagdoll->bAllAsleep = false;
						}
						else
						{
							pBodyParts[ i ].SetToIdentity();
						}
					}

					contract_assert( pBones[ nBones ].GetColumn( X_AXIS ) == Vector( 11, 22, 33 ) );
					[[maybe_unused]] bool bInserted = m_mapCachedRagdollBones.try_emplace( nEntIndex, pCachedRagdoll ).second;
					contract_assert( bInserted );
				}
			}
			else
			{
				Warning( "A ragdoll without entindex found @%p\n", pRagdoll );
			}
		}
	}
	if ( nRagdollsCached )
		DevMsg( "%d ragdolls cached\n", nRagdollsCached );
}

CachedRagdollBones_t *CHltvReplaySystem::GetCachedRagdollBones( int nEntIndex, bool bTake )
{
	if ( gpGlobals->tickcount > m_nHltvReplayBeginTick + 5 )
		return nullptr; // this cache isn't useful anymore, we need to create ragdoll dynamically - otherwise it'll just teleport to the final position instantaneously

	auto it = m_mapCachedRagdollBones.find( nEntIndex );
	CachedRagdollBones_t *pTakenBones = nullptr;

	if ( it != m_mapCachedRagdollBones.end() )
	{
		pTakenBones = it->second.get();
		if ( bTake )
		{
			// caller now owns the bones and frees them with FreeCachedRagdollBones
			it->second.release();
			m_mapCachedRagdollBones.erase( it );
		}
	}
	
	return pTakenBones;
}

void CHltvReplaySystem::FreeCachedRagdollBones( CachedRagdollBones_t*pBones )
{
	CachedRagdollBonesDeleter()( pBones );
}

void CHltvReplaySystem::CachedRagdollBonesDeleter::operator()( CachedRagdollBones_t *pBones ) const
{
	MemAlloc_FreeAligned( pBones );
}

CHltvReplaySystem::CLocalPlayerProps::CLocalPlayerProps()
{
	m_bLastSeenAlive = false;
	m_nLastTickUpdated = gpGlobals->tickcount;
}

bool CHltvReplaySystem::CLocalPlayerProps::Update()
{
	if ( m_nLastTickUpdated != gpGlobals->tickcount )
	{
		m_nLastTickUpdated = gpGlobals->tickcount;
		bool bLastLastSeenAlive = m_bLastSeenAlive;
		if ( CBasePlayer *pBasePlayer = CBasePlayer::GetLocalPlayer() )
		{
			m_bLastSeenAlive = pBasePlayer->IsAlive();
		}
		else
		{
			m_bLastSeenAlive = false;
		}

		if ( bLastLastSeenAlive != m_bLastSeenAlive )
			return true; // something changed
	}
	return false; // nothing changed
}

float CHltvReplaySystem::GetReplayMessageTime()
{
	static ConVarRef spec_replay_message_time( "spec_replay_message_time" );
	float flReplayMessageTime = spec_replay_message_time.GetFloat();
	if ( CCSGameRules* pRules = CSGameRules() )
	{
		if ( pRules->m_iRoundWinStatus != WINNER_NONE )
		{// the message time is cut short by the end of round
			flReplayMessageTime = spec_replay_autostart_delay.GetFloat();
		}
	}
	return flReplayMessageTime;
}

bool CHltvReplaySystem::UpdateHltvReplayButtonTimeOutState()
{
	bool bTimedOut = false;

	if ( CCSGameRules* pRules = CSGameRules() )
	{
		if ( pRules->IsWarmupPeriod() && !pRules->IsWarmupPeriodPaused() )
		{
			static ConVarRef spec_replay_leadup_time( "spec_replay_leadup_time" );
			static ConVarRef spec_replay_winddown_time( "spec_replay_winddown_time" );

			float flWarmupTimeLeft = pRules->GetWarmupPeriodEndTime() - gpGlobals->curtime;
			float flReplayRoundtripTime = spec_replay_autostart_delay.GetFloat() + spec_replay_leadup_time.GetFloat() + spec_replay_winddown_time.GetFloat();
			if ( flWarmupTimeLeft <= flReplayRoundtripTime )
			{
				bTimedOut = true;
			}
		}
	}

	bool bStateUpdated = ( m_bHltvReplayButtonTimedOut != bTimedOut );
	m_bHltvReplayButtonTimedOut = bTimedOut;
	return bStateUpdated;
}
