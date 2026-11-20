// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include "client_class.h"
#include "reflect_recvtable.h"
#include "edict.h"
#include "engine/ivdebugoverlay.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/materialsystem_config.h"
#include "view_shared.h"
#include "predictable_entity.h"
#include "basehandle.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "particlemgr.h"
#include "engine/ivmodelinfo.h"
#include "videocfg/videocfg.h"
#include "cdll_client_int.h"
#include "materialsystem/imaterialsystem.h"
#include "renderparm.h"
#include "networkvar.h"
module ks.client;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :basetoggle;
import :baseentity;
import :util;
import :precache;
import :noopanimcontroller;
import :iinput;
import :sharedvar;
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.math.mathlib;
import ks.math.polyhedron;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vertexcolor;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.math.vplane;
import ks.tier1.interfaces;
import ks.tier1.strtools;
import :particlemgr;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.core.scratcharena;
import ks.core.wchartypes;
import ks.math.transform;
import ks.core.cmdline;
import ks.tier1.bitbuf;
import ks.tier1.keyvalues;
import ks.tier1.vector_ops;
import ks.core.random;

using namespace ::ks::reflect;

// Purpose: Ambient light controller entity with simple radial falloff

static ConVar cl_ambient_light_disableentities( side_registry(), "cl_ambient_light_disableentities", "0", FCVAR_NONE, "Disable map ambient light entities." );

static CSpatialEntityMgr s_EnvAmbientLightMgr;

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvAmbientLight, DT_EnvAmbientLight, CEnvAmbientLight )

void CEnvAmbientLight::ApplyAccumulation( void )
{
	Vector rgbVal = BlendedValue();

	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
	{
		mat_ambient_light_r.SetValue( rgbVal.x );
	}

	if ( mat_ambient_light_g.IsValid() )
	{
		mat_ambient_light_g.SetValue( rgbVal.y );
	}

	if ( mat_ambient_light_b.IsValid() )
	{
		mat_ambient_light_b.SetValue( rgbVal.z );
	}
}

void CEnvAmbientLight::AddToPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.AddSpatialEntity( this );
}

void CEnvAmbientLight::RemoveFromPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.RemoveSpatialEntity( this );
}

void CEnvAmbientLight::SetColor( const Vector &vecColor, float flLerpTime )
{
	if ( flLerpTime <= 0 )
	{
		m_Value = vecColor / 255.0f;
		m_colorTimer.Invalidate();
		return;
	}

	m_vecStartColor = m_Value;
	m_vecTargetColor = vecColor / 255.0f;
	m_colorTimer.Start( flLerpTime );
}

void CEnvAmbientLight::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( m_colorTimer.HasStarted() )
	{
		if ( m_colorTimer.IsElapsed() )
		{
			m_Value = m_vecTargetColor;
			m_colorTimer.Invalidate();
		}
		else
		{
			m_Value = m_vecTargetColor - ( m_vecTargetColor - m_vecStartColor ) * m_colorTimer.GetRemainingRatio();
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Directional lighting with cascaded shadow mapping entity.

//#define CsmDbgMsg Msg
#define CsmDbgMsg(x)

static constexpr auto MAX_CSM_CASCADES = 3;

// NOTE: cl_csm_enabled is driven by the video config system (it is in
// videocfg.cpp's s_pConVarsAllowedInSystemLevel, set from gpu_level/cpu_level).
// It must NOT be FCVAR_ARCHIVE/FCVAR_CHEAT or UpdateSystemLevel() refuses to
// apply the level setting and spews a warning. FCVAR_RELEASE keeps it visible
// and settable from the console without persisting a stale archived value.
ConVar cl_csm_enabled( side_registry(), "cl_csm_enabled", "1", FCVAR_RELEASE, "" );
ConVar cl_csm_max_shadow_dist(side_registry(), "cl_csm_max_shadow_dist", "-1", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_capture_state( side_registry(), "cl_csm_capture_state", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_clear_captured_state( side_registry(), "cl_csm_clear_captured_state", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_render_ztest( side_registry(), "cl_csm_debug_render_ztest", "1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_max_visible_dist(side_registry(), "cl_csm_max_visible_dist", "5000", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_vis_lo_range(side_registry(), "cl_csm_debug_vis_lo_range", ".35", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_vis_hi_range(side_registry(), "cl_csm_debug_vis_hi_range", "1.0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_use_forced_view_matrices(side_registry(), "cl_csm_use_forced_view_matrices", "1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_2d( side_registry(), "cl_csm_debug_2d", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_3d( side_registry(), "cl_csm_debug_3d", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_culling( side_registry(), "cl_csm_debug_culling", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_culling_cascade( side_registry(), "cl_csm_debug_culling_cascade", "-1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_print_culling_planes( side_registry(), "cl_csm_print_culling_planes", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viz_numplanes( side_registry(), "cl_csm_viz_numplanes", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_viz_polyhedron_quad_size( side_registry(), "cl_csm_viz_polyhedron_quad_size", "131072", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_use_env_light_direction( side_registry(), "cl_csm_use_env_light_direction", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_override( side_registry(), "cl_csm_rot_override", "0", FCVAR_DEVELOPMENTONLY );

// dust2's angles
ConVar cl_csm_rot_x( side_registry(), "cl_csm_rot_x", "50", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_y( side_registry(), "cl_csm_rot_y", "43", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_z( side_registry(), "cl_csm_rot_z", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_disable_culling( side_registry(), "cl_csm_disable_culling", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_shadows( side_registry(), "cl_csm_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_entity_shadows( side_registry(), "cl_csm_entity_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_static_prop_shadows( side_registry(), "cl_csm_static_prop_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_world_shadows( side_registry(), "cl_csm_world_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_world_shadows_in_viewmodelcascade( side_registry(), "cl_csm_world_shadows_in_viewmodelcascade", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_sprite_shadows( side_registry(), "cl_csm_sprite_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rope_shadows( side_registry(), "cl_csm_rope_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_translucent_shadows( side_registry(), "cl_csm_translucent_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_translucent_shadows_using_opaque_path( side_registry(), "cl_csm_translucent_shadows_using_opaque_path", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_ignore_disable_shadow_depth_rendering( side_registry(), "cl_csm_ignore_disable_shadow_depth_rendering", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_optimize_static_props( side_registry(), "cl_csm_optimize_static_props", "1", FCVAR_DEVELOPMENTONLY, "Enable/Disable optimal static prop rendering into CSM's (cull static props that make no visual contribution to shadows)" );

ConVar cl_csm_viewmodel_shadows( side_registry(), "cl_csm_viewmodel_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_max_shadow_dist( side_registry(), "cl_csm_viewmodel_max_shadow_dist", "21", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_farz( side_registry(), "cl_csm_viewmodel_farz", "30", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_max_visible_dist( side_registry(), "cl_csm_viewmodel_max_visible_dist", "1000", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_slopescaledepthbias_c0( side_registry(), "cl_csm_slopescaledepthbias_c0", "1.3", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c1( side_registry(), "cl_csm_slopescaledepthbias_c1", "2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c2( side_registry(), "cl_csm_slopescaledepthbias_c2", "2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c3( side_registry(), "cl_csm_slopescaledepthbias_c3", "2", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_depthbias_c0(	side_registry(), "cl_csm_depthbias_c0", ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c1(	side_registry(), "cl_csm_depthbias_c1", ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c2(	side_registry(), "cl_csm_depthbias_c2", ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c3(	side_registry(), "cl_csm_depthbias_c3", ".000025", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_viewmodel_slopescaledepthbias( side_registry(), "cl_csm_viewmodel_slopescaledepthbias", "1.5", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_depthbias( side_registry(), "cl_csm_viewmodel_depthbias", ".00005", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_hack_proj_matrices_for_cull_debugging( side_registry(), "cl_csm_hack_proj_matrices_for_cull_debugging", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_xlat_continuity( side_registry(), "cl_csm_xlat_continuity", "1", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_force_no_csm_in_reflections( side_registry(), "cl_csm_force_no_csm_in_reflections", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_cull_small_prop_threshold_volume( side_registry(), "cl_csm_cull_small_prop_threshold_volume", "2000.0f ", FCVAR_DEVELOPMENTONLY );

void CC_CSM_Status( const CCommand& args );
static ConCommand cl_csm_status(side_registry(), "cl_csm_status", CC_CSM_Status, "Usage:\n   cl_csm_status\n", 0);

CCascadeLightManager g_CascadeLightManager;

IMPLEMENT_REFLECT_CLIENTCLASS( CCascadeLight, DT_CascadeLight, CCascadeLight )

LINK_ENTITY_TO_CLASS(env_cascade_light, CCascadeLight);

CCascadeLight *CCascadeLight::m_pCascadeLight;

CCascadeLight::CCascadeLight() : 
	CBaseEntity(),
	m_shadowDirection( 0, 0, -1 ),
	m_envLightShadowDirection( 0, 0, -1 ),
	m_bEnabled( false ),
	m_bUseLightEnvAngles( true ),
	m_flMaxShadowDist( 400.0f )
{
	CsmDbgMsg( "CCascadeLight::CCascadeLight\n" );
	
	m_LightColor.r = 255;
	m_LightColor.g = 255;
	m_LightColor.b = 255;
	m_LightColor.a = 255;
	m_LightColorScale = 255;
}

CCascadeLight::~CCascadeLight()
{
	CsmDbgMsg( "CCascadeLight::~CCascadeLight\n" );
}

void CCascadeLight::Spawn()
{
	CsmDbgMsg( "CCascadeLight::Spawn\n" );

	BaseClass::Spawn();

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_pCascadeLight = this;
}

void CCascadeLight::Release()
{
	CsmDbgMsg( "CCascadeLight::Release\n" );

	m_pCascadeLight = nullptr;

	BaseClass::Release();
}

bool CCascadeLight::ShouldDraw()
{
	return false;
}

void CCascadeLight::ClientThink()
{
	CVProfScope _( "CCascadeLight::ClientThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	BaseClass::ClientThink();
}

// 4 cascades plus 1 for the scene frustum itself
static Vector g_vCascadeFrustumColors[ 4 + 1 ] =
{
	Vector( 0, 1, 0 ),
	Vector( 0, 0, 1 ),
	Vector( 0, 1, 1 ),
	Vector( 1, 0, 0 ),
	Vector( .85f, .85f, .2f )
};

void CCascadeLightManager::RotXPlusDown( const CCommand &args ) { g_CascadeLightManager.m_flRotX[0] = 1.0f; }
void CCascadeLightManager::RotXPlusUp( const CCommand &args ) { g_CascadeLightManager.m_flRotX[0] = 0.0f; }
void CCascadeLightManager::RotXNegDown( const CCommand &args ) { g_CascadeLightManager.m_flRotX[1] = -1.0f; }
void CCascadeLightManager::RotXNegUp( const CCommand &args ) { g_CascadeLightManager.m_flRotX[1] = 0.0f; }
void CCascadeLightManager::RotYPlusDown( const CCommand &args ) { g_CascadeLightManager.m_flRotY[0] = 1.0f; }
void CCascadeLightManager::RotYPlusUp( const CCommand &args ) { g_CascadeLightManager.m_flRotY[0] = 0.0f; }
void CCascadeLightManager::RotYNegDown( const CCommand &args ) { g_CascadeLightManager.m_flRotY[1] = -1.0f; }
void CCascadeLightManager::RotYNegUp( const CCommand &args ) { g_CascadeLightManager.m_flRotY[1] = 0.0f; }

static ConCommand start_csm_rot_x_plus( side_registry(), "+csm_rot_x_plus", CCascadeLightManager::RotXPlusDown);
static ConCommand end_csm_rot_x_plus( side_registry(), "-csm_rot_x_plus", CCascadeLightManager::RotXPlusUp);
static ConCommand start_csm_rot_x_neg( side_registry(), "+csm_rot_x_neg", CCascadeLightManager::RotXNegDown);
static ConCommand end_csm_rot_x_neg( side_registry(), "-csm_rot_x_neg", CCascadeLightManager::RotXNegUp);
static ConCommand start_csm_rot_y_plus( side_registry(), "+csm_rot_y_plus", CCascadeLightManager::RotYPlusDown);
static ConCommand end_csm_rot_y_plus( side_registry(), "-csm_rot_y_plus", CCascadeLightManager::RotYPlusUp);
static ConCommand start_csm_rot_y_neg( side_registry(), "+csm_rot_y_neg", CCascadeLightManager::RotYNegDown);
static ConCommand end_csm_rot_y_neg( side_registry(), "-csm_rot_y_neg", CCascadeLightManager::RotYNegUp);

static void DebugRenderWireframeFrustum3D( const VMatrix &xform, const Vector &color, bool bDepthTest = false )
{
	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_LINES; ++i )
	{
		Vector4D s, e;

		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 0 ] ], s );
		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 1 ] ], e );

		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );

		NDebugOverlay::Line( Vector( s.x, s.y, s.z ), Vector( e.x, e.y, e.z ), (int)(color.x * 255.0f), (int)(color.y * 255.0f), (int)(color.z * 255.0f), !bDepthTest, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	}
}

static void Add3DLineOverlay( const Vector &s, const Vector &e, const Vector &color, bool bDepthTest )
{
	NDebugOverlay::Line( s, e, (int)(color.x * 255.0f), (int)(color.y * 255.0f), (int)(color.z * 255.0f), !bDepthTest, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

static void DebugRenderConvexPolyhedron( const Vector4D *pPlanes, uint nNumPlanes, const Vector &color, bool zTest )
{

	uint nLastPlane = cl_csm_viz_numplanes.GetInt();
	if ( nLastPlane < 0 )
		nLastPlane = nNumPlanes;
	nLastPlane = std::min( nLastPlane, nNumPlanes );

	const float Q = cl_csm_viz_polyhedron_quad_size.GetFloat();

	for ( uint nPlaneIndex = 0; nPlaneIndex < nLastPlane; ++nPlaneIndex )
	{
		Vector4D plane( pPlanes[nPlaneIndex] );
				
		Vector vNormal( plane.x, plane.y, plane.z );
		float flDist = -plane.w;

		Vector vOrigin( vNormal * flDist );
		
		uint nMinorAxis = 0;
		float flMag = fabs( vNormal.x );
		if ( fabs( vNormal.y ) < flMag ) { flMag = fabs( vNormal.y ); nMinorAxis = 1; }
		if ( fabs( vNormal.z ) < flMag ) { flMag = fabs( vNormal.z ); nMinorAxis = 2; }

		Vector vU( 0.0f, 0.0f, 0.0f );
		vU.Base()[nMinorAxis] = 1.0f;

		float flDirAlong = vU.Dot( vNormal );
		
		vU -= flDirAlong * vNormal;
		vU.NormalizeInPlace();

		Vector vV( vNormal.Cross( vU ) );
		vV.NormalizeInPlace();
						
		double verts[2][64][3];
		
		verts[0][0][0] = vOrigin.x - vU.x * Q - vV.x * Q;
		verts[0][0][1] = vOrigin.y - vU.y * Q - vV.y * Q;
		verts[0][0][2] = vOrigin.z - vU.z * Q - vV.z * Q;

		verts[0][1][0] = vOrigin.x + vU.x * Q - vV.x * Q;
		verts[0][1][1] = vOrigin.y + vU.y * Q - vV.y * Q;
		verts[0][1][2] = vOrigin.z + vU.z * Q - vV.z * Q;

		verts[0][2][0] = vOrigin.x + vU.x * Q + vV.x * Q;
		verts[0][2][1] = vOrigin.y + vU.y * Q + vV.y * Q;
		verts[0][2][2] = vOrigin.z + vU.z * Q + vV.z * Q;

		verts[0][3][0] = vOrigin.x - vU.x * Q + vV.x * Q;
		verts[0][3][1] = vOrigin.y - vU.y * Q + vV.y * Q;
		verts[0][3][2] = vOrigin.z - vU.z * Q + vV.z * Q;
				
		uint nVerts = 4;
		double *pSrcVerts = &verts[0][0][0];
		double *pDstVerts = &verts[1][0][0];

		for ( uint nClipPlaneIndex = 0; nClipPlaneIndex < nLastPlane; ++nClipPlaneIndex )
		{
			if ( nPlaneIndex == nClipPlaneIndex )
				continue;

			Vector4D clipPlane( pPlanes[nClipPlaneIndex] );

			double vClipNormal[3] = { clipPlane.x, clipPlane.y, clipPlane.z };
			double flClipDist = -clipPlane.w;

			int nClipped = ClipPolyToPlane_Precise( pSrcVerts, nVerts, pDstVerts, vClipNormal, flClipDist, .000000125f );
			
			nVerts = nClipped;
			std::swap( pSrcVerts, pDstVerts );
			if ( nVerts < 3 )
				break;
		}

		if ( nVerts >= 3 )
		{
			Vector vAvg( 0.0f, 0.0f, 0.0f );
			for ( uint i = 0; i < nVerts; ++i )
			{
				const uint j = ( i + 1 ) % nVerts;
				Vector s( pSrcVerts[i*3+0], pSrcVerts[i*3+1], pSrcVerts[i*3+2] );
				Vector e( pSrcVerts[j*3+0], pSrcVerts[j*3+1], pSrcVerts[j*3+2] );
				
				Add3DLineOverlay( s, e, color, zTest );
				vAvg += s;
			}

			vAvg /= nVerts;

			Vector2D vLo( 1e+10f, 1e+10f );
			Vector2D vHi( -1e+10f, -1e+10f );
			for ( uint i = 0; i < nVerts; ++i )
			{
				Vector p( pSrcVerts[i*3+0], pSrcVerts[i*3+1], pSrcVerts[i*3+2] );

				p -= vAvg;
				Vector2D p2D( p.Dot( vU ),  p.Dot( vV ) );
				
				vLo = vLo.Min( p2D );
				vHi = vHi.Max( p2D );
			}

			float L = 75.0f;
			Add3DLineOverlay( vAvg, vAvg + vNormal * L, Vector( .1f, .1f, 1.0f ), true );
			Add3DLineOverlay( vAvg, vAvg + vU * L, Vector( 1.0f, .1f, .1f ), true );
			Add3DLineOverlay( vAvg, vAvg + vV * L, Vector( 0.1f, 1.0f, .1f ), true );

			float flXD = ( vHi.x - vLo.x ) / 24.0f;
			float flYD = ( vHi.y - vLo.y ) / 24.0f;

			for ( float flX = vLo.x; flX < vHi.x; flX += flXD )
			{
				for ( float flY = vLo.y; flY < vHi.y; flY += flYD )
				{
					Vector p( vAvg + vU * flX + vV * flY );

					uint i;
					for ( i = 0; i < nLastPlane; ++i )
					{
						if ( i == nPlaneIndex )
							continue;
						float flDist = pPlanes[i].x * p.x + pPlanes[i].y * p.y + pPlanes[i].z * p.z + pPlanes[i].w;
						if ( flDist < -.25f )
							break;
					}
					if ( i == nLastPlane )
					{
						Add3DLineOverlay( p, p + vNormal * 4.0f, Vector( .1f, .1f, 1.0f ), true );
					}
				}
			}
		}
	}

}

static void	ScreenText( int x, int y, int r, int g, int b, int a, const char *text, ...)
{
	char buf[256];
	va_list args;
	__builtin_va_start( args, text );
	V_vsnprintf( buf, sizeof( buf ), text, args );
	__builtin_va_end( args );

	const float flTextColWidth = 0.0044f; // 227
	const float flTextRowHeight = 0.0083f; // 120		
	NDebugOverlay::ScreenText( x * flTextColWidth, y * flTextRowHeight, buf, r, g, b, a, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

CDebugPrimRenderer2D::CDebugPrimRenderer2D()
{
	m_debugLines.reserve( 128 );
}

void CDebugPrimRenderer2D::Clear()
{
	m_debugLines.assign( 0, {} );
}

void CDebugPrimRenderer2D::AddNormalizedLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	int nIndex = std::ssize( m_debugLines ); m_debugLines.emplace_back();
	m_debugLines[nIndex].m_EndPoints[0].Init( sx, sy );
	m_debugLines[nIndex].m_EndPoints[1].Init( ex, ey );
	m_debugLines[nIndex].m_nColor[0] = r; 
	m_debugLines[nIndex].m_nColor[1] = g;	
	m_debugLines[nIndex].m_nColor[2] = b; 
	m_debugLines[nIndex].m_nColor[3] = 255;
}

void CDebugPrimRenderer2D::AddScreenspaceLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	int nIndex = std::ssize( m_debugLines ); m_debugLines.emplace_back();
	m_debugLines[nIndex].m_EndPoints[0].Init( sx * flInvBackBufWidth, sy * flInvBackBufHeight );
	m_debugLines[nIndex].m_EndPoints[1].Init( ex * flInvBackBufWidth, ey * flInvBackBufHeight );
	m_debugLines[nIndex].m_nColor[0] = r; 
	m_debugLines[nIndex].m_nColor[1] = g;	
	m_debugLines[nIndex].m_nColor[2] = b; 
	m_debugLines[nIndex].m_nColor[3] = 255;
}

void CDebugPrimRenderer2D::AddScreenspaceRect2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	AddScreenspaceLine2D( sx, sy, ex, sy, r, g, b );
	AddScreenspaceLine2D( ex, sy, ex, ey, r, g, b );
	AddScreenspaceLine2D( ex, ey, sx, ey, r, g, b );
	AddScreenspaceLine2D( sx, ey, sx, sy, r, g, b );
}

void CDebugPrimRenderer2D::AddScreenspaceLineList2D( uint nCount, const Vector2D *pVerts, const VertexColor_t &color )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	for ( uint i = 0; i < nCount; ++i )
	{
		int nIndex = std::ssize( m_debugLines ); m_debugLines.emplace_back();
		m_debugLines[nIndex].m_EndPoints[0].Init( pVerts[0].x * flInvBackBufWidth, pVerts[0].y * flInvBackBufHeight );
		m_debugLines[nIndex].m_EndPoints[1].Init( pVerts[1].x * flInvBackBufWidth, pVerts[1].y * flInvBackBufHeight );
		m_debugLines[nIndex].m_nColor[0] = color.r;
		m_debugLines[nIndex].m_nColor[1] = color.g;
		m_debugLines[nIndex].m_nColor[2] = color.b; 
		m_debugLines[nIndex].m_nColor[3] = 255;

		pVerts += 2;
	}
}

void CDebugPrimRenderer2D::RenderScreenspaceDepthTexture( float sx, float sy, float ex, float ey, float su, float sv, float eu, float ev, CTextureReference &depthTex, float zLo, float zHi )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	sx *= flInvBackBufWidth;
	sy *= flInvBackBufHeight;
	ex *= flInvBackBufWidth;
	ey *= flInvBackBufHeight;

	const float xl = -1.0f;
	const float yl = 1.0f; 
	const float xh = 1.0f; 
	const float yh = -1.0f;

	IMaterial *pMaterial = materials->FindMaterial( "debug/debugshadowbuffer", TEXTURE_GROUP_OTHER, true );
	if ( !pMaterial ) 
		return;
			
	float flInvZRange = 1.0f / ( zHi - zLo );

	IMaterialVar *c0_x = pMaterial->FindVar( "$c0_x", nullptr, false );
	c0_x->SetFloatValue( flInvZRange );	

	IMaterialVar *c0_y = pMaterial->FindVar( "$c0_y", nullptr, false );
	c0_y->SetFloatValue( -zLo * flInvZRange );

	IMaterialVar *c0_z = pMaterial->FindVar( "$c0_z", nullptr, false );
	c0_z->SetFloatValue( 0.0f );

	bool bFound = false;
	IMaterialVar *pMatVar = pMaterial->FindVar( "$basetexture", &bFound, false );
	if ( bFound && pMatVar )
	{
		if ( pMatVar->GetTextureValue() != depthTex )
		{
			pMatVar->SetTextureValue( depthTex );
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( Lerp( sx, xl, xh ), Lerp( sy, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, su, sv );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( ex, xl, xh ), Lerp( sy, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, eu, sv );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( ex, xl, xh ), Lerp( ey, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, eu, ev );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( sx, xl, xh ), Lerp( ey, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, su, ev );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

void CDebugPrimRenderer2D::Render2D( )
{
	if ( !m_debugLines.empty() )
	{
		RenderDebugLines2D( std::ssize( m_debugLines ), &m_debugLines[0] );
	}
}

void CDebugPrimRenderer2D::RenderDebugLines2D( uint nNumLines, const CDebugLine *pLines )
{
	if ( !nNumLines )
		return;

	const float xl = -1.0f;
	const float yl = 1.0f;
	const float xh = 1.0f;
	const float yh = -1.0f;
		
	IMaterial *pMaterial = materials->FindMaterial( "debug/debugscreenspacewireframe", TEXTURE_GROUP_OTHER, true );
	if ( !pMaterial ) 
		return;

	IMaterialVar *c0_x = pMaterial->FindVar( "$c0_x", nullptr, false );
	c0_x->SetFloatValue( 1.0f );	

	IMaterialVar *c0_y = pMaterial->FindVar( "$c0_y", nullptr, false );
	c0_y->SetFloatValue( 1.0f );

	IMaterialVar *c0_z = pMaterial->FindVar( "$c0_z", nullptr, false );
	c0_z->SetFloatValue( 1.0f );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nNumLines );

	for ( uint i = 0; i < nNumLines; ++i )
	{
		const CDebugLine &debugLine = pLines[i];

		meshBuilder.Position3f( Lerp( debugLine.m_EndPoints[0].x, xl, xh ), Lerp( debugLine.m_EndPoints[0].y, yl, yh ), 0.0f );
		meshBuilder.Color4ub( debugLine.m_nColor[0], debugLine.m_nColor[1], debugLine.m_nColor[2], 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( Lerp( debugLine.m_EndPoints[1].x, xl, xh ), Lerp( debugLine.m_EndPoints[1].y, yl, yh ), 0.0f );
		meshBuilder.Color4ub( debugLine.m_nColor[0], debugLine.m_nColor[1], debugLine.m_nColor[2], 255 );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

void CDebugPrimRenderer2D::AddScreenspaceWireframeFrustum2D( const VMatrix &xform, const VertexColor_t &color, bool bShowAxes )
{
	Vector2D points[ CCSMFrustumDefinition::NUM_FRUSTUM_LINES * 2 ];
	Vector2D *pDstPoint = points;

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_LINES; ++i )
	{
		Vector4D s, e;

		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 0 ] ], s );
		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 1 ] ], e );

		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );

		pDstPoint[0].Init( s.x, s.y ); 
		pDstPoint[1].Init( e.x, e.y );
		pDstPoint += 2;
	}

	AddScreenspaceLineList2D( CCSMFrustumDefinition::NUM_FRUSTUM_LINES, points, color );

	if ( bShowAxes )
	{
		Vector4D s, e;
		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.95f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 20, 20, 255, 255 ) );

		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 0.0f, 15.0f, 0.0f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 20, 255, 20, 255 ) );

		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 15.0f, 0.0f, 0.0f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 255, 20, 20, 255 ) );
	}
}

// TODO: Break CCascadeLightManager out to a separate file?

	// Important note: On PC, this depth texture resolution (or the inverse of it) is effectively hardcoded into the filter kernels sample offsets. Don't change it unless you fix this dependency.
	constexpr auto CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION = 1024*2;
	constexpr auto CSM_FALLBACK_DEPTH_TEXTURE_RESOLUTION = 768*2;
	constexpr auto CSM_FALLBACK2_DEPTH_TEXTURE_RESOLUTION = 640*2;

CCascadeLightManager::CCascadeLightManager() :
	m_bRenderTargetsAllocated( false ),
	m_nDepthTextureResolution( CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION ),
	m_bCSMIsActive( false ),
	m_bStateIsValid( false ),
	m_nCurRenderTargetQualityMode( CSMQUALITY_HIGH )
{
	V_memset( &m_flRotX, 0, sizeof( m_flRotX ) );
	V_memset( &m_flRotY, 0, sizeof( m_flRotY ) );
		
	m_curState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
	m_curViewModelState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
}

CCascadeLightManager::~CCascadeLightManager()
{
}

bool CCascadeLightManager::InitRenderTargets()
{
	CVProfScope _( "CCascadeLightManager::InitRenderTargets", 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );
	
	CsmDbgMsg( "CCascadeLight::InitRenderTargets\n" );

	if (
		!cl_csm_enabled.GetBool() ||
		!g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() ||
		!g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures()
		)
	{
		DeinitRenderTargets();

		cl_csm_enabled.SetValue( 0 );
		return false;
	}

	if ( m_bRenderTargetsAllocated )
		return true;
	
	m_bRenderTargetsAllocated = true;
			
	ImageFormat dstFormat  = g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat();	// Vendor-dependent depth texture format
	ImageFormat nullFormat = g_pMaterialSystemHardwareConfig->GetNullTextureFormat();			// Vendor-dependent null texture format (takes as little memory as possible)
		
	RenderTargetSizeMode_t sizeMode = RT_SIZE_OFFSCREEN;
	
	// Don't allow the shadow buffer render target's to get resized to always be <= the size of the backbuffer on the PC.
	// This allows us to use 1024x1024 or larger shadow depth buffers when 1024x768 backbuffers, for example.
	sizeMode = RT_SIZE_NO_CHANGE;

	m_nCurRenderTargetQualityMode = GetCSMQualityMode();

	m_nDepthTextureResolution = CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION;
	if ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW )
	{
		m_nDepthTextureResolution = CSM_FALLBACK2_DEPTH_TEXTURE_RESOLUTION;
	}
	else if ( GetCSMQualityMode() == CSMQUALITY_LOW )
	{
		m_nDepthTextureResolution = CSM_FALLBACK_DEPTH_TEXTURE_RESOLUTION;
	}
	
	m_curState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
	m_curViewModelState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	materials->BeginRenderTargetAllocation();
	
	m_DummyColorTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, sizeMode, nullFormat, 
		MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDummy" );
	m_ShadowDepthTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, sizeMode, dstFormat, 
		MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDepth" );

	materials->EndRenderTargetAllocation();

	if ( ( !m_DummyColorTexture.IsValid() ) && ( !m_ShadowDepthTexture.IsValid() ) )
	{
		DeinitRenderTargets();

		cl_csm_enabled.SetValue(0);
		
		return false;
	}

	V_memset( &m_flRotX, 0, sizeof( m_flRotX ) );
	V_memset( &m_flRotY, 0, sizeof( m_flRotY ) );

	return true;
}

void CCascadeLightManager::ShutdownRenderTargets()
{
	// This purposely does NOT actually destroy the render targets here, to clone the (strange, but well tested?) behavior of the clientshadowmgr.cpp.
	// Not destroying the shadow buffers when baseclientrendertargets.cpp calls this method is beneficial when loading HDR maps, which triggers a recreation of the well known 
	// render targets during level load.
	CsmDbgMsg( "CCascadeLight::ShutdownRenderTargets\n" );
}

void CCascadeLightManager::LevelInitPreEntity()
{
	CsmDbgMsg( "CCascadeLight::LevelInitPreEntity\n" );
}

void CCascadeLightManager::LevelInitPostEntity()
{
	CsmDbgMsg( "CCascadeLight::LevelInitPostEntity\n" );
}

void CCascadeLightManager::LevelShutdownPreEntity()
{
	CsmDbgMsg( "CCascadeLight::LevelShutdownPreEntity\n" );
}

void CCascadeLightManager::LevelShutdownPostEntity()
{
	CsmDbgMsg( "CCascadeLight::LevelShutdownPostEntity\n" );
}

void CCascadeLightManager::Shutdown()
{
	CsmDbgMsg( "CCascadeLight::Shutdown\n" );

	DeinitRenderTargets();
}

void CCascadeLightManager::DeinitRenderTargets()
{
	CsmDbgMsg( "CCascadeLight::DeinitRenderTargets\n" );

	UnlockAllShadowDepthTextures();

	if ( m_bRenderTargetsAllocated )
	{
		m_DummyColorTexture.Shutdown();
		m_ShadowDepthTexture.Shutdown();
		
		m_bRenderTargetsAllocated = false;
	}
}

bool CCascadeLightManager::IsEnabled() const
{
	if ( !CCascadeLight::Get() || !CCascadeLight::Get()->IsEnabled() || !cl_csm_enabled.GetBool() || !m_bRenderTargetsAllocated )
		return false;
	return true;
}

bool CCascadeLightManager::IsEnabledAndActive() const
{
	return IsEnabled() && ( CCascadeLight::Get() != nullptr );
}

void CCascadeLightManager::Draw3DDebugInfo()
{
	if ( !IsEnabled() )
		return;
			
	CFullCSMState &state = m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState;

	if ( !state.m_CSMParallelSplit.IsValid() )
		return;

	const bool zTest = cl_csm_debug_render_ztest.GetBool();

	const SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	const CFrustum *pCascadeFrustums = lightState.m_CascadeFrustums; pCascadeFrustums;
	const ShadowFrustaDebugInfo_t *pDebugInfo = lightState.m_DebugInfo; pDebugInfo;
	//const SunLightShaderParamsCB_t &shadowParams = lightState.m_SunLightShaderParams;
	const VMatrix *pCascadeProjToTexMatrices = lightState.m_CascadeProjToTexMatrices; pCascadeProjToTexMatrices;
	const CFrustum &sceneFrustum = state.m_sceneFrustum;

	const VMatrix sceneWorldToView( sceneFrustum.GetView() );
	const VMatrix &sceneViewToProj = sceneFrustum.GetProj();
	const VMatrix sceneWorldToProj( sceneViewToProj * sceneWorldToView );
	VMatrix sceneProjToWorld;
	sceneWorldToProj.InverseGeneral( sceneProjToWorld );

	const Vector &vEye = sceneFrustum.GetCameraPosition(); 
	const Vector &vForward = sceneFrustum.CameraForward(); 
	const Vector &vLeft = sceneFrustum.CameraLeft(); 
	const Vector &vUp = sceneFrustum.CameraUp(); 

	const Vector vDirToLight( -state.m_shadowDir );

	static_assert( 4 == MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	if ( cl_csm_debug_culling.GetBool() )
	{
		int s = 0, e = lightState.m_nShadowCascadeSize;
		if ( cl_csm_debug_culling_cascade.GetInt() >= 0 )
		{
			s = clamp<int, int, int>( cl_csm_debug_culling_cascade.GetInt(), 0, lightState.m_nShadowCascadeSize - 1 );
			e = s + 1;
		}
		for ( int i = s; i < e; ++i )
		{
			if ( lightState.m_CascadeVolumeCullers[i].HasBaseFrustum() )
			{
				VPlane basePlanes[CVolumeCuller::cNumBaseFrustumPlanes];
				lightState.m_CascadeVolumeCullers[i].GetBaseFrustumPlanes( basePlanes );
				int nPlaneCount = lightState.m_CascadeVolumeCullers[i].GetNumBaseFrustumPlanes();
				DebugRenderConvexPolyhedron( (Vector4D*)basePlanes, nPlaneCount, Vector( 1.0f, 1.0f, .2f ), true );
			}

			if ( lightState.m_CascadeVolumeCullers[i].HasInclusionVolume() )
				DebugRenderConvexPolyhedron( (Vector4D*)lightState.m_CascadeVolumeCullers[i].GetInclusionVolumePlanes(), lightState.m_CascadeVolumeCullers[i].GetNumInclusionVolumePlanes(), Vector( .3f, 1.0f, .3f ), true );

			if ( lightState.m_CascadeVolumeCullers[i].HasExclusionFrustum() )
				DebugRenderConvexPolyhedron( (Vector4D *)lightState.m_CascadeVolumeCullers[i].GetExclusionFrustumPlanes(), lightState.m_CascadeVolumeCullers[i].GetNumExclusionFrustumPlanes(), Vector( 1, .25f, .25f ), true );
		}

		return;
	}

	for ( uint nCascadeIndex = 0; nCascadeIndex < lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		const CFrustum &shadowFrustum = lightState.m_CascadeFrustums[ nCascadeIndex ];
		//const VMatrix &shadowWorldToView = shadowFrustum.GetView();
		//const VMatrix &shadowViewToProj = shadowFrustum.GetProj();

		VMatrix projToWorld( shadowFrustum.GetInvViewProj() );

		DebugRenderWireframeFrustum3D( projToWorld, g_vCascadeFrustumColors[nCascadeIndex], zTest );
	}

	Add3DLineOverlay( sceneFrustum.GetCameraPosition(), sceneFrustum.GetCameraPosition() + vDirToLight * 250.0f, Vector( 1, 1, 1 ), zTest );
		
	for ( uint nCascadeIndex = 0; nCascadeIndex <= lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		const VMatrix sceneWorldToView( sceneFrustum.GetView() );
		const VMatrix &sceneViewToProj = sceneFrustum.GetProj();

		VMatrix sceneProjToView;
		sceneViewToProj.InverseGeneral( sceneProjToView );

		VMatrix sceneViewToWorld;
		sceneWorldToView.InverseGeneral( sceneViewToWorld );

		Vector v[8];
		for ( uint z = 0; z < 2; ++z )
		{
			const float flZ = z ? 1.0f : 0.0f;

			for ( uint i = 0; i < 4; ++i )
			{
				Vector vViewspacePoint;
				sceneProjToView.V3Mul( Vector( CCSMFrustumDefinition::g_vProjFrustumVerts[i].x, CCSMFrustumDefinition::g_vProjFrustumVerts[i].y, flZ ), vViewspacePoint );

				// Scale the vector so its Z is -1.0f (negative viewspace Z is inside the frustum due to RH proj matrices)
								
				if ( ( z ) && ( nCascadeIndex < lightState.m_nShadowCascadeSize ) )
				{
					float flOneOverZ = 1.0f / vViewspacePoint.z;
					vViewspacePoint *= flOneOverZ;
					vViewspacePoint.z *= -1.0f;

					vViewspacePoint *= lightState.m_DebugInfo[nCascadeIndex].m_flSplitPlaneDistance;
				}
				else if ( ( !z ) && ( nCascadeIndex ) )
				{
					float flOneOverZ = 1.0f / vViewspacePoint.z;
					vViewspacePoint *= flOneOverZ;
					vViewspacePoint.z *= -1.0f;

					vViewspacePoint *= lightState.m_DebugInfo[nCascadeIndex - 1].m_flSplitPlaneDistance;
				}
				else
				{
					// flip xy to be consistent with the other cases
					vViewspacePoint.x *= -1.0f;
					vViewspacePoint.y *= -1.0f;
				}

				sceneViewToWorld.V3Mul( vViewspacePoint, v[i + z * 4] );
			}
		}

		if ( !nCascadeIndex )
		{
			Add3DLineOverlay( v[0], v[1], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[1], v[2], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[2], v[3], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[3], v[0], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		}

		Add3DLineOverlay( v[4], v[5], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[5], v[6], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[6], v[7], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[7], v[4], g_vCascadeFrustumColors[nCascadeIndex], zTest );

		Add3DLineOverlay( v[0], v[4], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[1], v[5], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[2], v[6], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[3], v[7], g_vCascadeFrustumColors[nCascadeIndex], zTest );
	}

	Vector vFadeStart( vEye + vForward * lightState.m_flZLerpStartDist );
	Vector vFadeEnd( vEye + vForward * lightState.m_flZLerpEndDist );
	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeStart + vLeft * -300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeStart + vLeft *  300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.0f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeStart + vUp * -300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeStart + vUp *  300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.0f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeEnd + vLeft * -300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeEnd + vLeft *  300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.5f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeEnd + vUp * -300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeEnd + vUp *  300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.5f, 1.0f ), true );
	}

	//DebugRenderWireframeFrustum3D( sceneFrustum.GetInvViewProj(), Vector( .4f, .4f, .8f ), zTest );
}

void CCascadeLightManager::Draw2DDebugInfo()
{
	if ( !IsEnabled() )
		return;

	CFullCSMState &state = m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState;

	if ( !state.m_CSMParallelSplit.IsValid() )
		return;

	const SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	const CFrustum *pCascadeFrustums = lightState.m_CascadeFrustums;
	const ShadowFrustaDebugInfo_t *pDebugInfo = lightState.m_DebugInfo;
	//const SunLightShaderParamsCB_t &shadowParams = lightState.m_SunLightShaderParams;
	const VMatrix *pCascadeProjToTexMatrices = lightState.m_CascadeProjToTexMatrices;
	const CFrustum &sceneFrustum = state.m_sceneFrustum;

	const VMatrix sceneWorldToView( sceneFrustum.GetView() );
	const VMatrix &sceneViewToProj = sceneFrustum.GetProj();
	const VMatrix sceneWorldToProj( sceneViewToProj * sceneWorldToView );
	VMatrix sceneProjToWorld;
	sceneWorldToProj.InverseGeneral( sceneProjToWorld );
	
	m_debugPrimRenderer.Clear();

	int nFirstCascadeIndex = 0;
	int nLastCascadeIndex = (int)lightState.m_nShadowCascadeSize - 1;
	
	const int nShadowBufferDebugVisWidth  = 512;
	const int nShadowBufferDebugVisHeight = 512;

	for ( int nCascadeIndex = nFirstCascadeIndex; nCascadeIndex <= nLastCascadeIndex; ++nCascadeIndex )
	{
		const Rect_t &cascadeViewport = lightState.m_CascadeViewports[nCascadeIndex];

		int nPlacementX = nCascadeIndex & 1;
		int nPlacementY = nCascadeIndex >> 1;
		
		Rect_t destRect;

		destRect.x = 16 + ( nShadowBufferDebugVisWidth + 75 ) * nPlacementX;
		destRect.y = 16 + ( nShadowBufferDebugVisHeight + 75 ) * nPlacementY;
		destRect.width = nShadowBufferDebugVisWidth;
		destRect.height = nShadowBufferDebugVisHeight;

		VertexColor_t color( g_vCascadeFrustumColors[nCascadeIndex].x * 255, g_vCascadeFrustumColors[nCascadeIndex].y * 255, g_vCascadeFrustumColors[nCascadeIndex].z * 255, 255 );

		m_debugPrimRenderer.AddScreenspaceRect2D( destRect.x, destRect.y, destRect.x + destRect.width, destRect.y + destRect.height, color.r, color.g, color.b );

		m_debugPrimRenderer.RenderScreenspaceDepthTexture( 
			destRect.x, destRect.y, destRect.x + destRect.width, destRect.y + destRect.height, 
			(float)cascadeViewport.x / m_nDepthTextureResolution,
			(float)cascadeViewport.y / m_nDepthTextureResolution,
			(float)( cascadeViewport.x + cascadeViewport.width ) / m_nDepthTextureResolution,
			(float)( cascadeViewport.y + cascadeViewport.height ) / m_nDepthTextureResolution,
			m_ShadowDepthTexture, cl_csm_debug_vis_lo_range.GetFloat(), cl_csm_debug_vis_hi_range.GetFloat() );
							
		const CFrustum &shadowFrustum = pCascadeFrustums[ nCascadeIndex ];
		const VMatrix shadowWorldToView( shadowFrustum.GetView() );
		const VMatrix &shadowViewToProj = shadowFrustum.GetProj();

		const VMatrix shadowWorldToProj( shadowViewToProj * shadowWorldToView );

		const VMatrix shadowProjToTex( pCascadeProjToTexMatrices[nCascadeIndex] );

		const VMatrix sceneProjToTex( shadowProjToTex * ( shadowWorldToProj * sceneProjToWorld ) );

		VMatrix shadowTexToRect( 
			destRect.width,     0.0f,					0.0f,   destRect.x,
			0.0f,				destRect.height,		0.0f,   destRect.y,
			0.0f,				0.0f,					0.0f,	.5f,
			0.0f,				0.0f,					0.0f,	1.0f );

		const VMatrix sceneProjToRect( shadowTexToRect * sceneProjToTex );

		m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( sceneProjToRect, color, true );	

		if ( m_capturedState.m_CSMParallelSplit.IsValid() )
		{
			CFullCSMState &actualState = m_curState;
			const CFrustum &actualSceneFrustum = actualState.m_sceneFrustum;
			
			const VMatrix actualSceneWorldToView( actualSceneFrustum.GetView() );
			const VMatrix &actualSceneViewToProj = actualSceneFrustum.GetProj();

			const VMatrix actualSceneWorldToProj( actualSceneViewToProj * actualSceneWorldToView );
			VMatrix actualSceneProjToWorld;
			actualSceneWorldToProj.InverseGeneral( actualSceneProjToWorld );

			const VMatrix actualSceneProjToTex( shadowProjToTex * ( shadowWorldToProj * actualSceneProjToWorld ) );
			const VMatrix actualSceneProjToRect( shadowTexToRect * actualSceneProjToTex );

			VertexColor_t actualFrustumColor( 0, 255, 0, 255 );
			m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( actualSceneProjToRect, actualFrustumColor, true );	
		}
		
		VMatrix shadowWorldToRect( shadowTexToRect * ( shadowProjToTex * shadowWorldToProj ) );

		for ( uint i = 0; i < pDebugInfo[nCascadeIndex].m_nNumWorldFocusVerts; ++i )
		{
			const Vector &v = pDebugInfo[nCascadeIndex].m_WorldFocusVerts[i];
			Vector4D worldVert( v.x, v.y, v.z, 1.0f );

			Vector4D rectVert;
			shadowWorldToRect.V4Mul( worldVert, rectVert );

			rectVert *= ( 1.0f / rectVert.w );

			Vector2D points[4];
			points[0].Init( rectVert.x - 5, rectVert.y );
			points[1].Init( rectVert.x + 5, rectVert.y );
			points[2].Init( rectVert.x, rectVert.y - 5 );
			points[3].Init( rectVert.x, rectVert.y + 5);

			m_debugPrimRenderer.AddScreenspaceLineList2D( 2, points, VertexColor_t( 20, 255, 20, 255 ) );
		}

		for ( int nPrevSplitIndex = 0; nPrevSplitIndex < nCascadeIndex; ++nPrevSplitIndex )
		{
			const CFrustum &prevShadowFrustum = pCascadeFrustums[ nPrevSplitIndex ];
			const VMatrix prevShadowWorldToView( prevShadowFrustum.GetView() );
			const VMatrix &prevShadowViewToProj = prevShadowFrustum.GetProj();

			const VMatrix prevShadowWorldToProj( prevShadowViewToProj * prevShadowWorldToView );
			VMatrix prevShadowProjToWorld;
			prevShadowWorldToProj.InverseGeneral( prevShadowProjToWorld	);
			
			const VMatrix prevShadowProjToShadowRect( shadowTexToRect * ( shadowProjToTex * ( shadowWorldToProj * prevShadowProjToWorld ) ) );

			VertexColor_t prevSplitColor( g_vCascadeFrustumColors[nPrevSplitIndex].x * 255, g_vCascadeFrustumColors[nPrevSplitIndex].y * 255, g_vCascadeFrustumColors[nPrevSplitIndex].z * 255, 255 );

			m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( prevShadowProjToShadowRect, prevSplitColor, false );
		}
	}

	m_debugPrimRenderer.Render2D();
}

void CCascadeLightManager::DrawTextDebugInfo()
{
	CFullCSMState &curState = GetActiveState();

	uint cur_y = 105;

	ScreenText( 5, cur_y, 255, 95, 95, 255, "Active State: %s Pos:(%3.3f, %3.3f, %3.3f), Forward:(%3.3f, %3.3f, %3.3f), Left:(%3.3f, %3.3f, %3.3f), Up:(%3.3f, %3.3f, %3.3f)", 
		m_capturedState.m_CSMParallelSplit.IsValid() ? "CAP" : "CUR", 
		curState.m_sceneFrustum.GetCameraPosition().x, curState.m_sceneFrustum.GetCameraPosition().y, curState.m_sceneFrustum.GetCameraPosition().z,
		curState.m_sceneFrustum.CameraForward().x, curState.m_sceneFrustum.CameraForward().y, curState.m_sceneFrustum.CameraForward().z,
		curState.m_sceneFrustum.CameraLeft().x, curState.m_sceneFrustum.CameraLeft().y, curState.m_sceneFrustum.CameraLeft().z,
		curState.m_sceneFrustum.CameraUp().x, curState.m_sceneFrustum.CameraUp().y, curState.m_sceneFrustum.CameraUp().z );
	cur_y += 2;

	if ( GetClientWorldEntity() )
	{
		const Vector &worldMins = GetClientWorldEntity()->m_WorldMins;
		const Vector &worldMaxs = GetClientWorldEntity()->m_WorldMaxs;
	
		ScreenText( 5, cur_y, 255, 95, 95, 255, "World Bounds: (%f,%f,%f) - (%f,%f,%f)", 
			worldMins.x, worldMins.y, worldMins.z,
			worldMaxs.x, worldMaxs.y, worldMaxs.z );
		cur_y += 2;
	}
	
	uint m_nTotalAABB = 0;
	uint m_nTotalAABBPassed = 0;
	uint m_nTotalCenterHalfDiagonal = 0;
	uint m_nTotalCenterHalfDiagonalPassed = 0;

	uint i;
	for ( i = 0; i < 4; i++ )
	{
		CVolumeCuller &cascadeVolumeCuller = (i == 3) ? m_curViewModelState.m_CSMParallelSplit.GetLightState().m_CascadeVolumeCullers[0] : curState.m_CSMParallelSplit.GetLightState().m_CascadeVolumeCullers[i];
		CVolumeCuller::CullCheckStats_t &cullStats = cascadeVolumeCuller.GetStats();
		
		ScreenText( 5, cur_y, 255, 95, 95, 255, "Cascade %u: Total AABB Cull Checks: %u, Passed: %u, Total World AABB Cull Checks: %u, Passed: %u", 
			i,
			cullStats.m_nTotalAABB, cullStats.m_nTotalAABBPassed,
			cullStats.m_nTotalCenterHalfDiagonal, cullStats.m_nTotalCenterHalfDiagonalPassed );
		cur_y += 2;

		m_nTotalAABB += cullStats.m_nTotalAABB;
		m_nTotalAABBPassed += cullStats.m_nTotalAABBPassed;
		m_nTotalCenterHalfDiagonal += cullStats.m_nTotalCenterHalfDiagonal;
		m_nTotalCenterHalfDiagonalPassed += cullStats.m_nTotalCenterHalfDiagonalPassed;
		
		cascadeVolumeCuller.ClearCullCheckStats();
	}

	ScreenText( 5, cur_y, 255, 95, 95, 255, "All Cascades: Total AABB Cull Checks: %u, Passed: %u, Total World AABB Cull Checks: %u, Passed: %u", 
		m_nTotalAABB, m_nTotalAABBPassed,
		m_nTotalCenterHalfDiagonal, m_nTotalCenterHalfDiagonalPassed );
	cur_y += 2;
}

void CCascadeLightManager::CFullCSMState::Update( const CViewSetup &viewSetup, const Vector &shadowDir, color32 lightColor, int lightColorScale, float flMaxShadowDist, float flMaxVisibleDist, uint nMaxCascadeSize, uint nAtlasFirstCascadeIndex, int nCSMQualityLevel, bool bSetAllCascadesToFirst )
{
	m_shadowDir = shadowDir;
	m_flMaxShadowDist = flMaxShadowDist;
	m_flMaxVisibleDist = flMaxVisibleDist;
	m_nMaxCascadeSize = nMaxCascadeSize;
	
	m_flSceneAspectRatio = viewSetup.ComputeViewMatrices( &m_sceneWorldToView, &m_sceneViewToProj, &m_sceneWorldToProj );

	m_sceneFrustum.BuildFrustumFromParameters( viewSetup.origin, viewSetup.angles, viewSetup.zNear, viewSetup.zFar, viewSetup.fov, m_flSceneAspectRatio, m_sceneWorldToView, m_sceneViewToProj );

	SunLightViewState_t sunLightViewState;
	sunLightViewState.m_Direction = shadowDir;
	sunLightViewState.m_LightColor = lightColor;
	sunLightViewState.m_LightColorScale = lightColorScale;
	sunLightViewState.m_flMaxShadowDist = flMaxShadowDist;
	sunLightViewState.m_flMaxVisibleDist = flMaxVisibleDist;
	sunLightViewState.m_frustum = m_sceneFrustum;
	sunLightViewState.m_nMaxCascadeSize = nMaxCascadeSize;
	sunLightViewState.m_nAtlasFirstCascadeIndex = nAtlasFirstCascadeIndex;
	sunLightViewState.m_nCSMQualityLevel = nCSMQualityLevel;
	sunLightViewState.m_bSetAllCascadesToFirst = bSetAllCascadesToFirst;
	m_CSMParallelSplit.Update( sunLightViewState );
	
	m_bValid = ( m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize != 0 );
}

void CCascadeLightManager::RenderViews( CCascadeLightManager::CFullCSMState &state, bool bIncludeViewModels )
{
	SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	
	uint nCascadeIndex = 0;
	{
		if ( ( !bIncludeViewModels ) && ( GetCSMQualityMode() <= CSMQUALITY_LOW ) )
			nCascadeIndex = 1;
	}

	CMatRenderContextPtr pRenderContext( materials );

	for ( ; nCascadeIndex < lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		CViewSetup shadowView;
		shadowView.m_flAspectRatio = 1.0f;
		shadowView.x = lightState.m_CascadeViewports[nCascadeIndex].x;
		shadowView.y = lightState.m_CascadeViewports[nCascadeIndex].y;
		shadowView.width = lightState.m_CascadeViewports[nCascadeIndex].width;
		shadowView.height = lightState.m_CascadeViewports[nCascadeIndex].height;
		shadowView.m_bRenderToSubrectOfLargerScreen = true;

		const CFrustum &cascadeFrustum = lightState.m_CascadeFrustums[nCascadeIndex];

		// Force custom matrices, to avoid FP precision issues causing shadow continuity problems. (We need precise control over the matrices used for rendering.)
		if ( cl_csm_use_forced_view_matrices.GetBool() )
		{
			shadowView.m_bCustomViewMatrix = true;
			// Argh - m_matCustomViewMatrix is actually a custom world->camera matrix, not world->view!
			// The view->camera matrix only has 0,-1,1 values so it shouldn't affect precision.
			shadowView.m_matCustomViewMatrix = ( g_matViewToCameraMatrix * VMatrix( cascadeFrustum.GetView() ) ).As3x4();

			shadowView.m_bCustomProjMatrix = true;
			shadowView.m_matCustomProjMatrix = cascadeFrustum.GetProj();
		}

		cascadeFrustum.GetClipSpaceBounds( shadowView.m_OrthoLeft, shadowView.m_OrthoBottom, shadowView.m_OrthoRight, shadowView.m_OrthoTop );
		shadowView.origin = cascadeFrustum.GetCameraPosition();
		shadowView.angles = cascadeFrustum.GetCameraAngles();
		shadowView.zNear = shadowView.zNearViewmodel = cascadeFrustum.GetCameraNearPlane();
		shadowView.zFar = shadowView.zFarViewmodel = cascadeFrustum.GetCameraFarPlane();

		if ( cl_csm_debug_2d.GetBool() && cl_csm_use_forced_view_matrices.GetBool() && cl_csm_hack_proj_matrices_for_cull_debugging.GetBool() ) 
		{
			MatrixBuildOrtho( 
				shadowView.m_matCustomProjMatrix, 
				Lerp( -2.0f, shadowView.m_OrthoLeft, shadowView.m_OrthoRight ),
				Lerp( -2.0f, shadowView.m_OrthoTop, shadowView.m_OrthoBottom ),
				Lerp( -2.0f, shadowView.m_OrthoRight, shadowView.m_OrthoLeft ),
				Lerp( -2.0f, shadowView.m_OrthoBottom, shadowView.m_OrthoTop ),
				shadowView.zNear, 
				shadowView.zFar );
		}

		shadowView.m_bDoBloomAndToneMapping = false;
		shadowView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;

		// Set depth bias factors specific to this cascade
		
		static ConVar *s_csm_slopescales[4] = { &cl_csm_slopescaledepthbias_c0, &cl_csm_slopescaledepthbias_c1, &cl_csm_slopescaledepthbias_c2, &cl_csm_slopescaledepthbias_c3 };
		static ConVar *s_csm_depthbias[4] = { &cl_csm_depthbias_c0, &cl_csm_depthbias_c1, &cl_csm_depthbias_c2, &cl_csm_depthbias_c3 };
				
		float flShadowSlopeScaleDepthBias = s_csm_slopescales[nCascadeIndex]->GetFloat();
		float flShadowDepthBias = s_csm_depthbias[nCascadeIndex]->GetFloat();

		if ( bIncludeViewModels )
		{
			flShadowSlopeScaleDepthBias = cl_csm_viewmodel_slopescaledepthbias.GetFloat();
			flShadowDepthBias = cl_csm_viewmodel_depthbias.GetFloat();
		}

		pRenderContext->PerpareForCascadeDraw( nCascadeIndex, flShadowSlopeScaleDepthBias, flShadowDepthBias );
		pRenderContext->SetShadowDepthBiasFactors( flShadowSlopeScaleDepthBias, flShadowDepthBias );
		pRenderContext->CullMode( MATERIAL_CULLMODE_NONE );

		shadowView.m_bOrtho = true;
		shadowView.m_bCSMView = true;
		
		CVolumeCuller &volumeCuller = lightState.m_CascadeVolumeCullers[nCascadeIndex];
		if ( ( nCascadeIndex == ( lightState.m_nShadowCascadeSize - 1 ) ) && ( GetCSMQualityMode() <= CSMQUALITY_LOW ) )
		{
			volumeCuller.SetCullSmallObjects( true, cl_csm_cull_small_prop_threshold_volume.GetFloat() );
		}
		else
		{
			volumeCuller.SetCullSmallObjects( false, 1e+10f );
		}

		shadowView.m_pCSMVolumeCuller = &volumeCuller;

		// Render to the shadow depth texture with appropriate view
		
		view->UpdateShadowDepthTexture( m_DummyColorTexture, m_ShadowDepthTexture, shadowView, true, bIncludeViewModels );
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
}

void CCascadeLightManager::PreRender()
{
}

Vector CCascadeLightManager::GetShadowDirection()
{
	Vector vShadowDir( CCascadeLight::Get()->GetShadowDirection() );
	if ( ( cl_csm_use_env_light_direction.GetBool() ) && ( CCascadeLight::Get()->UseLightEnvAngles() ) && ( CCascadeLight::Get()->GetEnvLightShadowDirection().Length() > .5f ) )
	{
		vShadowDir = CCascadeLight::Get()->GetEnvLightShadowDirection();
	}

	if ( cl_csm_rot_override.GetBool() ) 
	{
		const float flRotDegPerSec = 20.0f;

		float flRotX = cl_csm_rot_x.GetFloat(), flRotY = cl_csm_rot_y.GetFloat();
		if ( m_flRotX[0] || m_flRotX[1] )
		{
			flRotX += m_flRotX[0] * gpGlobals->frametime * flRotDegPerSec;
			flRotX += m_flRotX[1] * gpGlobals->frametime * flRotDegPerSec;
			if ( flRotX > 360.0f )
				flRotX -= 360.0f;
			else if ( flRotX < 0.0f )
				flRotX += 360.0f;
			cl_csm_rot_x.SetValue( flRotX );
		}

		if ( m_flRotY[0] || m_flRotY[1] ) 
		{
			flRotY += m_flRotY[0] * gpGlobals->frametime * flRotDegPerSec;
			flRotY += m_flRotY[1] * gpGlobals->frametime * flRotDegPerSec;
			if ( flRotY > 360.0f )
				flRotY -= 360.0f;
			else if ( flRotY < 0.0f )
				flRotY += 360.0f;
			cl_csm_rot_y.SetValue( flRotY );
		}

		QAngle angles;
		angles.Init( flRotX, flRotY, cl_csm_rot_z.GetFloat() );
		AngleVectors( angles, &vShadowDir );
	}
	
	return vShadowDir.Normalized();
}

// bSetup only used on PS3 right now - to support 2 pass Build/Draw rendering 
// bSetup indicates whether to execute once per frame setup code, do this in the buildlist (1st pass) pass only
// Needed since this will get called twice on PS3 if 2 pass drawing is on
// bSetup = true otherwise
void CCascadeLightManager::ComputeShadowDepthTextures( const CViewSetup &viewSetup, bool bSetup )
{
	m_bStateIsValid = false;
	m_bCSMIsActive = false;
	
	if (
		!cl_csm_enabled.GetBool() ||
		!g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() ||
		!g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures()
		)
	{
		cl_csm_enabled.SetValue( 0 );

		if ( m_bRenderTargetsAllocated )
		{
			// This code path will only execute if CSM initializes successfully at startup, but is then disabled either from the console or a config/device change.
			materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
			DeinitRenderTargets();
			materials->FinishRenderTargetAllocation();
		}
		
		return;
	}

	if ( !m_bRenderTargetsAllocated )
	{
		// If shadow depth texture is now needed but wasn't allocated, allocate it. 
		// (This is not a normal code path - it's only taken if the user turns on CSM from the console.)
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		InitRenderTargets();
		materials->FinishRenderTargetAllocation();
	}
	else if ( m_nCurRenderTargetQualityMode != GetCSMQualityMode() )
	{
		DeinitRenderTargets();
		
		// Quality level has changed - recreate render targets as they may change size.
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		InitRenderTargets();
		materials->FinishRenderTargetAllocation();
	}
	
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( !m_bRenderTargetsAllocated || !CCascadeLight::Get() )
	{
		m_curState.Reset();
		m_curViewModelState.Reset();
	}
	else
	{
		Vector vShadowDir( GetShadowDirection() );

		color32 lightColor = CCascadeLight::Get()->GetColor();
		int lightColorScale = CCascadeLight::Get()->GetColorScale();
		
		float flMaxShadowDist = cl_csm_max_shadow_dist.GetFloat();
		if ( flMaxShadowDist <= 0.0f )
		{
			flMaxShadowDist = CCascadeLight::Get()->GetMaxShadowDist();
			if (                  ( GetCSMQualityMode() == CSMQUALITY_HIGH ) )
			{
				// At the highest CSM quality level boost the max shadow distance.
				// This seems OK from a CS fairness perspective (it can be argued either way whether this gives a player an advantage, or disadvantage).
				flMaxShadowDist *= 1.4f;
			}
		}
		if ( flMaxShadowDist <= 0.0f )
			flMaxShadowDist = 400.0f;
				
		flMaxShadowDist = clamp<float>( flMaxShadowDist, 1.0f, 10000.0f );
		m_curState.Update( viewSetup, vShadowDir, lightColor, lightColorScale, flMaxShadowDist, cl_csm_max_visible_dist.GetFloat(), MAX_CSM_CASCADES, 0, GetCSMQualityMode(), false );
		
		CViewSetup viewModelViewSetup( viewSetup );
		viewModelViewSetup.zNear = .5f;
		viewModelViewSetup.zFar = cl_csm_viewmodel_farz.GetFloat();
		m_curViewModelState.Update( viewModelViewSetup, vShadowDir, lightColor, lightColorScale, cl_csm_viewmodel_max_shadow_dist.GetFloat(), cl_csm_viewmodel_max_visible_dist.GetFloat(), 1, 3, GetCSMQualityMode(), true );
							
		if ( cl_csm_capture_state.GetInt() )
		{
			cl_csm_capture_state.SetValue( 0 );
			m_capturedState = m_curState;
		}

		if ( cl_csm_clear_captured_state.GetInt() )
		{
			cl_csm_clear_captured_state.SetValue( 0 );
			m_capturedState.Clear();
		}

		// The CSM sampling shader can only handle MAX_CSM_CASCADES cascades to reduce the number of dynamic combos.
		if ( GetActiveState().IsValid() && ( GetActiveState().m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize == MAX_CSM_CASCADES ) )
		{
			pRenderContext->BeginGeneratingCSMs();

			RenderViews( GetActiveState(), false );
			
			bool bRenderViewModelCascade = cl_csm_viewmodel_shadows.GetBool();
			if ( 				 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
			{
				bRenderViewModelCascade = false;
			}
							
			if ( bRenderViewModelCascade )
			{
				RenderViews( m_curViewModelState, true );
			}

			pRenderContext->EndGeneratingCSMs();
			m_bCSMIsActive = true;
		}
	}
				
	pRenderContext->SetCascadedShadowMapping( m_bCSMIsActive );
	if ( m_bCSMIsActive )
	{
		pRenderContext->SetCascadedShadowMappingState( GetActiveState().m_CSMParallelSplit.GetLightState().m_SunLightShaderParams, m_ShadowDepthTexture );
				
		if ( cl_csm_debug_2d.GetBool() )
		{
			// Draw the text here so it doesn't lag 1 frame behind the other debug info
			DrawTextDebugInfo();
		}
	}

	m_bStateIsValid = m_bCSMIsActive;
}

void CCascadeLightManager::UnlockAllShadowDepthTextures()
{
	if ( m_bCSMIsActive )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->SetCascadedShadowMapping( false );

		m_bCSMIsActive = false;
	}
}

void CCascadeLightManager::BeginViewModelRendering()
{
	if ( !IsEnabledAndActive() || !m_bStateIsValid || !cl_csm_viewmodel_shadows.GetBool() )
		return;

	if ( 		 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
		return;
			
	m_bCSMIsActive = true;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( true );

	CascadedShadowMappingState_t &viewModelShaderParams = m_curViewModelState.m_CSMParallelSplit.GetLightState().m_SunLightShaderParams;
	viewModelShaderParams.m_bIsRenderingViewModels = true;

	pRenderContext->SetCascadedShadowMappingState( viewModelShaderParams, m_ShadowDepthTexture );
}

void CCascadeLightManager::EndViewModelRendering()
{
	if ( !IsEnabledAndActive() || !m_bStateIsValid || !cl_csm_viewmodel_shadows.GetBool() )
		return;

	if ( 		 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
		return;

	if ( m_bCSMIsActive )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->SetCascadedShadowMapping( false );

		m_bCSMIsActive = false;

		// 7LS - poke is view model rendering, testing, should move to somewhere more sensible
		m_curViewModelState.m_CSMParallelSplit.GetLightState().m_SunLightShaderParams.m_bIsRenderingViewModels = false;
	}
}

void CCascadeLightManager::BeginReflectionView()
{
	if ( !m_bCSMIsActive )
		return;
	if ( ( GetCSMQualityMode() > CSMQUALITY_LOW ) && !cl_csm_force_no_csm_in_reflections.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( false );
}

void CCascadeLightManager::EndReflectionView()
{
	if ( !m_bCSMIsActive )
		return;
	if ( ( GetCSMQualityMode() > CSMQUALITY_LOW ) && !cl_csm_force_no_csm_in_reflections.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( true );
}

void CCascadeLightManager::DumpStatus()
{
	Msg( "CSM enabled: %i\n", cl_csm_enabled.GetBool() );
	Msg( "Render targets allocated: %i\n", m_bRenderTargetsAllocated );
	Msg( "Depth texture resolution: %i\n", m_nDepthTextureResolution );
	Msg( "Hardware config supportsCascadedShadowMapping: %i\n", g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() );
	Msg( "Hardware config supportsShadowDepthTextures: %i\n", g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures() );
	Msg( "Hardware config SupportsBilinearPCFSampling: %i\n", g_pMaterialSystemHardwareConfig->SupportsBilinearPCFSampling() );
	Msg( "Current actual CSM quality level (%i=highest, will be forced to 0 if HW doesn't support bilinear PCF): %i\n", CSMQUALITY_TOTAL_MODES - 1, GetCSMQualityMode() );
	Msg( "env_cascade_light entity exists: %i\n", CCascadeLight::Get() != nullptr );
	if ( CCascadeLight::Get() )
	{
		Msg( "env_cascade_light values:\n" );
		Msg( "Shadow direction: %f %f %f\n", CCascadeLight::Get()->GetShadowDirection().x, CCascadeLight::Get()->GetShadowDirection().y, CCascadeLight::Get()->GetShadowDirection().z );
		Msg( "Light env shadow direction: %f %f %f\n", CCascadeLight::Get()->GetEnvLightShadowDirection().x, CCascadeLight::Get()->GetEnvLightShadowDirection().y, CCascadeLight::Get()->GetEnvLightShadowDirection().z );
		Msg( "Use light env shadow angles: %u\n", CCascadeLight::Get()->UseLightEnvAngles() );
		Msg( "Max shadow dist: %f\n", CCascadeLight::Get()->GetMaxShadowDist() );
	}
}

void CC_CSM_Status( const CCommand& args )
{
	g_CascadeLightManager.DumpStatus();
}

CSMQualityMode_t CCascadeLightManager::GetCSMQualityMode()
{

	if ( !g_pMaterialSystemHardwareConfig->SupportsBilinearPCFSampling() )
	{
		// DX9-class ATI cards: always using VERY_LOW, because the PCF sampling shader has hardcoded knowledge of the shadow depth texture's res
		return CSMQUALITY_VERY_LOW;
	}
		
	return materials->GetCurrentConfigForVideoCard().GetCSMQualityMode();
}

#undef CsmDbgMsg

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( EnvDetailController, DT_DetailController )
LINK_ENTITY_TO_CLASS_ALIASED(env_detail_controller,	EnvDetailController);

IMPLEMENT_REFLECT_TABLE( CEnvDetailController, DT_DetailController );

static CEnvDetailController *s_detailController = nullptr;
CEnvDetailController * GetDetailController()
{
	return s_detailController;
}

CEnvDetailController::CEnvDetailController()
{
	s_detailController = this;
}

CEnvDetailController::~CEnvDetailController()
{
	if ( s_detailController == this )
	{
		s_detailController = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
int CEnvDetailController::UpdateTransmitState()
{
	return 0;
}

class CBaseEntity;

using namespace ::ks::reflect;

extern bool  g_bDOFEnabled;
extern float g_flDOFNearBlurDepth;
extern float g_flDOFNearFocusDepth;
extern float g_flDOFFarFocusDepth;
extern float g_flDOFFarBlurDepth;
extern float g_flDOFNearBlurRadius;
extern float g_flDOFFarBlurRadius;

EHANDLE g_hDOFControllerInUse = INVALID_EHANDLE;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvDOFController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvDOFController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvDOFController, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CEnvDOFController();
	~CEnvDOFController();
	virtual void	OnDataChanged( DataUpdateType_t updateType );

private:
	[[= ks::reflect::Net{} ]] bool  m_bDOFEnabled;
	[[= ks::reflect::Net{} ]] float m_flNearBlurDepth;
	[[= ks::reflect::Net{} ]] float m_flNearFocusDepth;
	[[= ks::reflect::Net{} ]] float m_flFarFocusDepth;
	[[= ks::reflect::Net{} ]] float m_flFarBlurDepth;
	[[= ks::reflect::Net{} ]] float m_flNearBlurRadius;
	[[= ks::reflect::Net{} ]] float m_flFarBlurRadius;

private:
	CEnvDOFController( const CEnvDOFController & );
};

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvDOFController, DT_EnvDOFController, CEnvDOFController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvDOFController::CEnvDOFController( void )
:	m_bDOFEnabled( true ),
	m_flNearBlurDepth( 50.0f ),
	m_flNearFocusDepth( 100.0f ),
	m_flFarFocusDepth( 250.0f ),
	m_flFarBlurDepth( 1000.0f ),
	m_flNearBlurRadius( 0.0f ),		// no near blur by default
	m_flFarBlurRadius( 5.0f )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvDOFController::~CEnvDOFController( void )
{
	if ( g_hDOFControllerInUse == this )
	{
		g_bDOFEnabled = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	g_bDOFEnabled = m_bDOFEnabled && ( ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f ) );
	g_flDOFNearBlurDepth	= m_flNearBlurDepth;
	g_flDOFNearFocusDepth	= m_flNearFocusDepth;
	g_flDOFFarFocusDepth	= m_flFarFocusDepth;
	g_flDOFFarBlurDepth		= m_flFarBlurDepth;
	g_flDOFNearBlurRadius	= m_flNearBlurRadius;
	g_flDOFFarBlurRadius	= m_flFarBlurRadius;

	g_hDOFControllerInUse = this;
}

class CFogController;

using namespace ::ks::reflect;

// Purpose: An entity that allows level designer control over the fog parameters.

IMPLEMENT_NETWORKCLASS_ALIASED( FogController, DT_FogController )

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( CFogController, DT_FogController );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFogController::CFogController()
{
	// Make sure that old maps without fog fields don't get wacked out fog values.
	m_fog.enable = false;
	m_fog.maxdensity = 1.0f;
	m_fog.HDRColorScale = 1.0f;
}

class CParticleEffectBinding;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseAnimating;
class IParticleEffect;
struct Particle;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// An entity which emits other entities at points 
//-----------------------------------------------------------------------------
class CEnvParticleScript : public CBaseAnimating, public IParticleEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvParticleScript" } ]]
	[[= ks::reflect::From<"m_flSequenceScale", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvParticleScript, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	CEnvParticleScript();

// IParticleEffect overrides.
public:
	virtual bool	ShouldSimulate() const { return m_bSimulate; }
	virtual void	SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	virtual const Vector &GetSortOrigin();

// CBaseAnimating overrides
public:
	// NOTE: Ths enclosed particle effect binding will do all the drawing
	// But we have to return true, unlike other particle systems, for the animation events to work
	virtual bool ShouldDraw() { return true; }
	virtual int	DrawModel( int flags, const RenderableInstance_t &instance ) { return 0; }

	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	
private:

	// Creates, destroys particles attached to an attachment
	void CreateParticle( const char *pAttachmentName, const char *pSpriteName );
	void DestroyAllParticles( const char *pAttachmentName );
	void DestroyAllParticles( );

private:
	struct ParticleScriptParticle_t : public Particle
	{
		int m_nAttachment;
		float m_flSize;
	};

	CParticleEffectBinding	m_ParticleEffect;
	float m_flMaxParticleSize;
	int m_nOldSequence;
	float m_flSequenceScale;
	bool m_bSimulate;
};

REGISTER_EFFECT( CEnvParticleScript );

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CEnvParticleScript, DT_EnvParticleScript, CEnvParticleScript )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEnvParticleScript::CEnvParticleScript()
{
	m_flMaxParticleSize = 0.0f;
	m_bSimulate = true;
}

//-----------------------------------------------------------------------------
// Check for changed sequence numbers
//-----------------------------------------------------------------------------
void CEnvParticleScript::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_nOldSequence = GetSequence();
}

//-----------------------------------------------------------------------------
// Starts up the particle system
//-----------------------------------------------------------------------------
void CEnvParticleScript::OnDataChanged( DataUpdateType_t updateType )
{		
	BaseClass::OnDataChanged( updateType );

	if( updateType == DATA_UPDATE_CREATED )
	{
		ParticleMgr()->AddEffect( &m_ParticleEffect, this );
		g_pClientLeafSystem->EnableRendering( RenderHandle(), false );
	}

	if ( m_nOldSequence != GetSequence() )
	{
		DestroyAllParticles();
	}
}

//-----------------------------------------------------------------------------
// Creates, destroys particles attached to an attachment
//-----------------------------------------------------------------------------
void CEnvParticleScript::CreateParticle( const char *pAttachmentName, const char *pSpriteName )
{
	// Find the attachment
	int nAttachment = LookupAttachment( pAttachmentName );
	if ( nAttachment <= 0 )
		return;

	// Get the sprite materials
	PMaterialHandle hMat = m_ParticleEffect.FindOrAddMaterial( pSpriteName );
	ParticleScriptParticle_t *pParticle = 
		(ParticleScriptParticle_t*)m_ParticleEffect.AddParticle(sizeof(ParticleScriptParticle_t), hMat);

	if ( pParticle == nullptr )
		return;
	
	// Get the sprite size from the material's materialvars
	bool bFound = false;
	IMaterialVar *pMaterialVar = nullptr;
	IMaterial *pMaterial = ParticleMgr()->PMaterialToIMaterial( hMat );
	if ( pMaterial )
	{
		pMaterialVar = pMaterial->FindVar( "$spritesize", &bFound, false );
	}

	if ( bFound )
	{
		pParticle->m_flSize = pMaterialVar->GetFloatValue();
	}
	else
	{
		pParticle->m_flSize = 100.0f;
	}

	// Make sure the particle cull size reflects our particles
	if ( pParticle->m_flSize > m_flMaxParticleSize )
	{
		m_flMaxParticleSize = pParticle->m_flSize;
		m_ParticleEffect.SetParticleCullRadius( m_flMaxParticleSize );
	}

	// Place the particle on the attachment specified
	pParticle->m_nAttachment = nAttachment;
	QAngle vecAngles;
	GetAttachment( nAttachment, pParticle->m_Pos, vecAngles );

	if ( m_flSequenceScale != 1.0f )
	{
		pParticle->m_Pos -= GetAbsOrigin();
		pParticle->m_Pos *= m_flSequenceScale;
		pParticle->m_Pos += GetAbsOrigin();
	}
}

void CEnvParticleScript::DestroyAllParticles( const char *pAttachmentName )
{
	ks::ScratchArena<> scratch;
	int nAttachment = LookupAttachment( pAttachmentName );
	if ( nAttachment <= 0 )
		return;

	int nCount = m_ParticleEffect.GetNumActiveParticles();
	Particle** ppParticles = scratch.Alloc<Particle*>( nCount );
	int nActualCount = m_ParticleEffect.GetActiveParticleList( nCount, ppParticles );
	contract_assert( nActualCount == nCount );

	for ( int i = 0; i < nActualCount; ++i )
	{
		ParticleScriptParticle_t *pParticle = (ParticleScriptParticle_t*)ppParticles[i];
		if ( pParticle->m_nAttachment == nAttachment )
		{
			// Mark for deletion
			pParticle->m_nAttachment = -1;
		}
	}
}

void CEnvParticleScript::DestroyAllParticles( )
{
	ks::ScratchArena<> scratch;
	int nCount = m_ParticleEffect.GetNumActiveParticles();
	Particle** ppParticles = scratch.Alloc<Particle*>( nCount );
	int nActualCount = m_ParticleEffect.GetActiveParticleList( nCount, ppParticles );
	contract_assert( nActualCount == nCount );

	for ( int i = 0; i < nActualCount; ++i )
	{
		ParticleScriptParticle_t *pParticle = (ParticleScriptParticle_t*)ppParticles[i];

		// Mark for deletion
		pParticle->m_nAttachment = -1;
	}
}

//-----------------------------------------------------------------------------
// The animation events will create particles on the attachment points
//-----------------------------------------------------------------------------
void CEnvParticleScript::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	// Handle events to create + destroy particles
	switch( event )
	{
	case CL_EVENT_SPRITEGROUP_CREATE:
		{
			char pAttachmentName[256];
			char pSpriteName[256];
			int nArgs = sscanf( options, "%255s %255s", pAttachmentName, pSpriteName );
			if ( nArgs == 2 )
			{
				CreateParticle( pAttachmentName, pSpriteName );
			}
		}
		return;

	case CL_EVENT_SPRITEGROUP_DESTROY:
		{
			char pAttachmentName[256];
			int nArgs = sscanf( options, "%255s", pAttachmentName );
			if ( nArgs == 1 )
			{
				DestroyAllParticles( pAttachmentName );
			}
		}
		return;
	}

	// Fall back
	BaseClass::FireEvent( origin, angles, event, options );
}

//-----------------------------------------------------------------------------
// Simulate the particles
//-----------------------------------------------------------------------------
void CEnvParticleScript::RenderParticles( CParticleRenderIterator *pIterator )
{
	const ParticleScriptParticle_t* pParticle = (const ParticleScriptParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		Vector vecRenderPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, vecRenderPos );
		float sortKey = vecRenderPos.z;

		Vector color( 1, 1, 1 );
		RenderParticle_ColorSize( pIterator->GetParticleDraw(), vecRenderPos, color, 1.0f, pParticle->m_flSize );
		
		pParticle = (const ParticleScriptParticle_t*)pIterator->GetNext( sortKey );
	}
}

void CEnvParticleScript::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	ParticleScriptParticle_t* pParticle = (ParticleScriptParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Here's how we retire particles
		if ( pParticle->m_nAttachment == -1 )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			// Move the particle to the attachment point
			QAngle vecAngles;
			GetAttachment( pParticle->m_nAttachment, pParticle->m_Pos, vecAngles );

			if ( m_flSequenceScale != 1.0f )
			{
				pParticle->m_Pos -= GetAbsOrigin();
				pParticle->m_Pos *= m_flSequenceScale;
				pParticle->m_Pos += GetAbsOrigin();
			}
		}
		
		pParticle = (ParticleScriptParticle_t*)pIterator->GetNext();
	}
}

const Vector &CEnvParticleScript::GetSortOrigin()
{
	return GetAbsOrigin();
}

class CBasePlayer;
class CEnvProjectedTexture;

using namespace ::ks::reflect;

float CEnvProjectedTexture::m_flVisibleBBoxMinHeight = -std::numeric_limits<float>::max();

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvProjectedTexture, DT_EnvProjectedTexture, CEnvProjectedTexture )

CEnvProjectedTexture *CEnvProjectedTexture::Create( )
{
	CEnvProjectedTexture *pEnt = new CEnvProjectedTexture();

	pEnt->m_flNearZ = 4.0f;
	pEnt->m_flFarZ = 2000.0f;
//	strcpy( pEnt->m_SpotlightTextureName, "particle/rj" );
	pEnt->m_bLightWorld = true;
	pEnt->m_bLightOnlyTarget = false;
	pEnt->m_bSimpleProjection = true;
	pEnt->m_nShadowQuality = 1;
	pEnt->m_flLightFOV = 10.0f;
	pEnt->m_LightColor.r = 255;
	pEnt->m_LightColor.g = 255;
	pEnt->m_LightColor.b = 255;
	pEnt->m_LightColor.a = 255;
	pEnt->m_bEnableShadows = false;
	pEnt->m_flColorTransitionTime = 1.0f;
	pEnt->m_bCameraSpace = false;
	pEnt->SetAbsAngles( QAngle( 90, 0, 0 ) );
	pEnt->m_bAlwaysUpdate = true;
	pEnt->m_bState = true;
	pEnt->m_flProjectionSize = 500.0f;
	pEnt->m_flRotation = 0.0f;

	return pEnt;
}

CEnvProjectedTexture::CEnvProjectedTexture( void )
{
	m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_bForceUpdate = true;
	m_pMaterial = nullptr;
	m_bIsCurrentlyProjected = false;
	AddToEntityList( ENTITY_LIST_SIMULATE );
}

CEnvProjectedTexture::~CEnvProjectedTexture( void )
{
	ShutDownLightHandle();
}

void CEnvProjectedTexture::ShutDownLightHandle( void )
{
	// Clear out the light
	if( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		if ( m_bSimpleProjection == true )
		{
			g_pClientShadowMgr->DestroyProjection( m_LightHandle );
		}
		else
		{
			g_pClientShadowMgr->DestroyFlashlight( m_LightHandle );
		}
		m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}

	m_bIsCurrentlyProjected = false;
}

void CEnvProjectedTexture::SetMaterial( IMaterial *pMaterial )
{
	if ( pMaterial != m_ProjectedMaterial )
	{
		m_ProjectedMaterial.Init( pMaterial );
		pMaterial->AddRef();
	}
}

void CEnvProjectedTexture::SetLightColor( byte r, byte g, byte b, byte a )
{
	m_LightColor.r = r;
	m_LightColor.g = g;
	m_LightColor.b = b;
	m_LightColor.a = a;
}

void CEnvProjectedTexture::SetSize( float flSize )
{
	m_flProjectionSize = flSize;
}

void CEnvProjectedTexture::SetRotation( float flRotation )
{
	m_flRotation = flRotation;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : updateType -
//-----------------------------------------------------------------------------
void CEnvProjectedTexture::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_SpotlightTexture.Init( m_SpotlightTextureName, TEXTURE_GROUP_OTHER, true );
		m_ProjectedMaterial.Init( m_SpotlightTextureName, TEXTURE_GROUP_OTHER );
	}

	m_bForceUpdate = true;
	UpdateLight();
	BaseClass::OnDataChanged( updateType );
}

static ConVar asw_perf_wtf(side_registry(), "asw_perf_wtf", "0", FCVAR_DEVELOPMENTONLY, "Disable updating of projected shadow textures from UpdateLight" );
extern ConVar r_flashlightenableculling;

bool CEnvProjectedTexture::ShouldUpdate( void )
{
	CPULevel_t nCPULevel = GetCPULevel();
	bool bNoDraw = ( GetMinCPULevel() && GetMinCPULevel()-1 > nCPULevel );
	bNoDraw = bNoDraw || ( GetMaxCPULevel() && GetMaxCPULevel()-1 < nCPULevel );
	if ( bNoDraw )
		return false;

	GPULevel_t nGPULevel = GetGPULevel();
	bNoDraw = ( GetMinGPULevel() && GetMinGPULevel()-1 > nGPULevel );
	bNoDraw = bNoDraw || ( GetMaxGPULevel() && GetMaxGPULevel()-1 < nGPULevel );
	if ( bNoDraw )
		return false;

	return true;
}

extern ConVar r_flashlightdepthres;
void CEnvProjectedTexture::UpdateLight( void )
{
	if ( !ShouldUpdate() )
	{
		if ( m_bIsCurrentlyProjected )
		{
			ShutDownLightHandle();
		}
		return;
	}

	CVProfScope _( "CEnvProjectedTexture::UpdateLight", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	bool bVisible = true;

	Vector vLinearFloatLightColor( m_LightColor.r, m_LightColor.g, m_LightColor.b );
	float flLinearFloatLightAlpha = m_LightColor.a;

	if ( m_bAlwaysUpdate )
	{
		m_bForceUpdate = true;
	}

	if ( m_CurrentLinearFloatLightColor != vLinearFloatLightColor || m_flCurrentLinearFloatLightAlpha != flLinearFloatLightAlpha )
	{
		float flColorTransitionSpeed = gpGlobals->frametime * m_flColorTransitionTime * 255.0f;

		m_CurrentLinearFloatLightColor.x = Approach( vLinearFloatLightColor.x, m_CurrentLinearFloatLightColor.x, flColorTransitionSpeed );
		m_CurrentLinearFloatLightColor.y = Approach( vLinearFloatLightColor.y, m_CurrentLinearFloatLightColor.y, flColorTransitionSpeed );
		m_CurrentLinearFloatLightColor.z = Approach( vLinearFloatLightColor.z, m_CurrentLinearFloatLightColor.z, flColorTransitionSpeed );
		m_flCurrentLinearFloatLightAlpha = Approach( flLinearFloatLightAlpha, m_flCurrentLinearFloatLightAlpha, flColorTransitionSpeed );

		m_bForceUpdate = true;
	}

	if ( !m_bForceUpdate && r_flashlightenableculling.GetBool() )
	{
		bVisible = IsBBoxVisible();
	}

	if ( m_bState == false || !bVisible )
	{
		// Spotlight's extents aren't in view
		ShutDownLightHandle();

		return;
	}

	if ( m_LightHandle == CLIENTSHADOW_INVALID_HANDLE || m_hTargetEntity != nullptr || m_bForceUpdate )
	{
		Vector vForward, vRight, vUp, vPos = GetAbsOrigin();
		FlashlightState_t state;

		if ( m_hTargetEntity != nullptr )
		{
			if ( m_bCameraSpace )
			{
				const QAngle &angles = GetLocalAngles();

				CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
				if( pPlayer )
				{
					const QAngle playerAngles = pPlayer->GetAbsAngles();

					Vector vPlayerForward, vPlayerRight, vPlayerUp;
					AngleVectors( playerAngles, &vPlayerForward, &vPlayerRight, &vPlayerUp );

					matrix3x4_t	mRotMatrix;
					AngleMatrix( angles, mRotMatrix );

					VectorITransform( vPlayerForward, mRotMatrix, vForward );
					VectorITransform( vPlayerRight, mRotMatrix, vRight );
					VectorITransform( vPlayerUp, mRotMatrix, vUp );

					float dist = (m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin()).Length();
					vPos = m_hTargetEntity->GetAbsOrigin() - vForward*dist;

					VectorNormalize( vForward );
					VectorNormalize( vRight );
					VectorNormalize( vUp );
				}
			}
			else
			{
				vForward = m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin();
				VectorNormalize( vForward );

				// JasonM - unimplemented
				contract_assert( 0 );

				//Quaternion q = DirectionToOrientation( dir );

				//
				// JasonM - set up vRight, vUp
				//

				//			VectorNormalize( vRight );
				//			VectorNormalize( vUp );
			}
		}
		else
		{
			AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
		}

		state.m_fHorizontalFOVDegrees = m_flLightFOV;
		state.m_fVerticalFOVDegrees = m_flLightFOV;

		state.m_vecLightOrigin = vPos;
		BasisToQuaternion( vForward, vRight, vUp, state.m_quatOrientation );
		state.m_NearZ = m_flNearZ;
		state.m_FarZ = m_flFarZ;

		if ( r_flashlightenableculling.GetBool() )
		{
			// quickly check the proposed light's bbox against the view frustum to determine whether we
			// should bother to create it, if it doesn't exist, or cull it, if it does.

			// get the half-widths of the near and far planes,
			// based on the FOV which is in degrees. Remember that
			// on planet Valve, x is forward, y left, and z up.
			const float tanHalfAngle = std::tan( m_flLightFOV * ( std::numbers::pi/180.0f ) * 0.5f );
			const float halfWidthNear = tanHalfAngle * m_flNearZ;
			const float halfWidthFar = tanHalfAngle * m_flFarZ;
			// now we can build coordinates in local space: the near rectangle is eg
			// (0, -halfWidthNear, -halfWidthNear), (0,  halfWidthNear, -halfWidthNear),
			// (0,  halfWidthNear,  halfWidthNear), (0, -halfWidthNear,  halfWidthNear)

			VectorAligned vNearRect[4] = {
				VectorAligned( m_flNearZ, -halfWidthNear, -halfWidthNear), VectorAligned( m_flNearZ,  halfWidthNear, -halfWidthNear),
				VectorAligned( m_flNearZ,  halfWidthNear,  halfWidthNear), VectorAligned( m_flNearZ, -halfWidthNear,  halfWidthNear)
			};

			VectorAligned vFarRect[4] = {
				VectorAligned( m_flFarZ, -halfWidthFar, -halfWidthFar), VectorAligned( m_flFarZ,  halfWidthFar, -halfWidthFar),
				VectorAligned( m_flFarZ,  halfWidthFar,  halfWidthFar), VectorAligned( m_flFarZ, -halfWidthFar,  halfWidthFar)
			};

			matrix3x4_t matOrientation( vForward, -vRight, vUp, vPos );

			enum
			{
				kNEAR = 0,
				kFAR = 1,
			};
			VectorAligned vOutRects[2][4];

			for ( int i = 0 ; i < 4 ; ++i )
			{
				VectorTransform( vNearRect[i].Base(), matOrientation, vOutRects[0][i].Base() );
			}
			for ( int i = 0 ; i < 4 ; ++i )
			{
				VectorTransform( vFarRect[i].Base(), matOrientation, vOutRects[1][i].Base() );
			}

			// now take the min and max extents for the bbox, and see if it is visible.
			Vector mins = **vOutRects;
			Vector maxs = **vOutRects;
			for ( int i = 1; i < 8 ; ++i )
			{
				VectorMin( mins, *(*vOutRects+i), mins );
				VectorMax( maxs, *(*vOutRects+i), maxs );
			}

			bool bVisible = IsBBoxVisible( mins, maxs );

			if (!bVisible)
			{
				// Spotlight's extents aren't in view
				if ( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
				{
					ShutDownLightHandle();
				}

				return;
			}
		}

		float flAlpha = m_flCurrentLinearFloatLightAlpha * ( 1.0f / 255.0f );

		// Get the current light style value to throttle the brightness by
		flAlpha *= engine->LightStyleValue( m_iStyle );

		state.m_fQuadraticAtten = 0.0;
		state.m_fLinearAtten = 100;
		state.m_fConstantAtten = 0.0f;
		state.m_FarZAtten = m_flFarZ;
		state.m_fBrightnessScale = m_flBrightnessScale;
		state.m_Color[0] = m_CurrentLinearFloatLightColor.x * ( 1.0f / 255.0f ) * flAlpha;
		state.m_Color[1] = m_CurrentLinearFloatLightColor.y * ( 1.0f / 255.0f ) * flAlpha;
		state.m_Color[2] = m_CurrentLinearFloatLightColor.z * ( 1.0f / 255.0f ) * flAlpha;
		state.m_Color[3] = 0.0f; // fixme: need to make ambient work m_flAmbient;
		state.m_flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
		state.m_flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();
		state.m_bEnableShadows = m_bEnableShadows;
		state.m_flShadowMapResolution = r_flashlightdepthres.GetFloat();

		if ( m_bSimpleProjection )
		{
			state.m_pSpotlightTexture = nullptr;
			state.m_pProjectedMaterial = m_ProjectedMaterial;
		}
		else
		{
			state.m_pSpotlightTexture = m_SpotlightTexture;
			state.m_pProjectedMaterial = nullptr;
		}

		state.m_nSpotlightTextureFrame = m_nSpotlightTextureFrame;
		state.m_flProjectionSize = m_flProjectionSize;
		state.m_flProjectionRotation = m_flRotation;

		state.m_nShadowQuality = m_nShadowQuality; // Allow entity to affect shadow quality
		state.m_bShareBetweenSplitscreenPlayers = true;	// projected flashlight entities should always be shared among players, because they live in a map.

		if( m_LightHandle == CLIENTSHADOW_INVALID_HANDLE )
		{
			// Hack: env projected textures don't work like normal flashlights; they're not assigned to a given splitscreen slot,
			// but the flashlight code requires this
			HACK_GETLOCALPLAYER_GUARD( "Env projected texture" );
			if ( m_bSimpleProjection == true )
			{
				m_LightHandle = g_pClientShadowMgr->CreateProjection( state );
			}
			else
			{
				m_LightHandle = g_pClientShadowMgr->CreateFlashlight( state );
			}

			if ( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
			{
				m_bForceUpdate = false;
			}
		}
		else
		{
			if ( m_bSimpleProjection == true )
			{
				g_pClientShadowMgr->UpdateProjectionState( m_LightHandle, state );
			}
			else
			{
				g_pClientShadowMgr->UpdateFlashlightState( m_LightHandle, state );
			}
			m_bForceUpdate = false;
		}

		g_pClientShadowMgr->GetFrustumExtents( m_LightHandle, m_vecExtentsMin, m_vecExtentsMax );

		m_vecExtentsMin = m_vecExtentsMin - GetAbsOrigin();
		m_vecExtentsMax = m_vecExtentsMax - GetAbsOrigin();
	}

	if( m_bLightOnlyTarget )
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, m_hTargetEntity );
	}
	else
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, INVALID_EHANDLE );
	}

	g_pClientShadowMgr->SetFlashlightLightWorld( m_LightHandle, m_bLightWorld );

	if ( !asw_perf_wtf.GetBool() && !m_bForceUpdate )
	{
		g_pClientShadowMgr->UpdateProjectedTexture( m_LightHandle, true );
	}

	m_bIsCurrentlyProjected = true;
}

bool CEnvProjectedTexture::Simulate( void )
{
	UpdateLight();

	BaseClass::Simulate();
	return true;
}

bool CEnvProjectedTexture::IsBBoxVisible( Vector vecExtentsMin, Vector vecExtentsMax )
{
	// Z position clamped to the min height (but must be less than the max)
	float flVisibleBBoxMinHeight = std::min( vecExtentsMax.z - 1.0f, m_flVisibleBBoxMinHeight );
	vecExtentsMin.z = std::max( vecExtentsMin.z, flVisibleBBoxMinHeight );

	// Check if the bbox is in the view
	return !engine->CullBox( vecExtentsMin, vecExtentsMax );
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvScreenOverlay : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvScreenOverlay" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvScreenOverlay, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	void	PreDataUpdate( DataUpdateType_t updateType );
	void	PostDataUpdate( DataUpdateType_t updateType );

	void	HandleOverlaySwitch( void );
	void	StartOverlays( void );
	void	StopOverlays( void );
	void	StartCurrentOverlay( void );
	void	ClientThink( void );

protected:
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::String, .varlen = true } ]] char	m_iszOverlayNames[ MAX_SCREEN_OVERLAYS ][255];
	[[= ks::reflect::Net{ .varlen = true } ]] float	m_flOverlayTimes[ MAX_SCREEN_OVERLAYS ];
	[[= ks::reflect::Net{} ]] float	m_flStartTime;
	[[= ks::reflect::Net{} ]] int     m_iDesiredOverlay;
	[[= ks::reflect::Net{} ]] bool	m_bIsActive;
	bool	m_bWasActive;
	int		m_iCachedDesiredOverlay;
	int		m_iCurrentOverlay;
	float	m_flCurrentOverlayTime;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvScreenOverlay, DT_EnvScreenOverlay, CEnvScreenOverlay )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_bWasActive = m_bIsActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// If we have a start time now, start the overlays going
	if ( m_bIsActive && m_flStartTime > 0 && view->GetScreenOverlayMaterial() == nullptr )
	{
		StartOverlays();
	}
	
	if ( m_flStartTime == -1 )
	{
		 StopOverlays();
	}

	HandleOverlaySwitch();

	if ( updateType == DATA_UPDATE_CREATED &&
		CommandLine()->FindParm( "-makereslists" ) )
	{
		for ( int i = 0; i < MAX_SCREEN_OVERLAYS; ++i )
		{
			if ( m_iszOverlayNames[ i ] && m_iszOverlayNames[ i ][ 0 ] )
			{
				materials->FindMaterial( m_iszOverlayNames[ i ], TEXTURE_GROUP_CLIENT_EFFECTS, false );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::StopOverlays( void )
{
	SetNextClientThink( CLIENT_THINK_NEVER );

	if ( m_bWasActive && !m_bIsActive )
	{
		view->SetScreenOverlayMaterial( nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::StartOverlays( void )
{
	m_iCurrentOverlay = 0;
	m_flCurrentOverlayTime = 0;
	m_iCachedDesiredOverlay	= 0;
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	StartCurrentOverlay();
	HandleOverlaySwitch();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::HandleOverlaySwitch( void )
{
	if( m_iCachedDesiredOverlay != m_iDesiredOverlay )
	{
		m_iCurrentOverlay = m_iDesiredOverlay;
		m_iCachedDesiredOverlay = m_iDesiredOverlay;
		StartCurrentOverlay();
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::StartCurrentOverlay( void )
{
	if ( m_iCurrentOverlay == MAX_SCREEN_OVERLAYS || !m_iszOverlayNames[m_iCurrentOverlay] || !m_iszOverlayNames[m_iCurrentOverlay][0] )
	{
		// Hit the end of our overlays, so stop.
		m_flStartTime = 0;
		StopOverlays();
		return;
	}

	if ( m_flOverlayTimes[m_iCurrentOverlay] == -1 )
		 m_flCurrentOverlayTime = -1;
	else
		 m_flCurrentOverlayTime = gpGlobals->curtime + m_flOverlayTimes[m_iCurrentOverlay];

	// Bring up the current overlay
	IMaterial *pMaterial = materials->FindMaterial( m_iszOverlayNames[m_iCurrentOverlay], TEXTURE_GROUP_CLIENT_EFFECTS, false );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		view->SetScreenOverlayMaterial( pMaterial );
	}
	else
	{
		Warning("env_screenoverlay couldn't find overlay %s.\n", m_iszOverlayNames[m_iCurrentOverlay] );
		StopOverlays();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::ClientThink( void )
{
	// If the current overlay's run out, go to the next one
	if ( m_flCurrentOverlayTime != -1 && m_flCurrentOverlayTime < gpGlobals->curtime )
	{
		m_iCurrentOverlay++;
		StartCurrentOverlay();
	}
}

// Effect types
enum 
{
	SCREENEFFECT_EP2_ADVISOR_STUN,
	SCREENEFFECT_EP1_INTRO,
	SCREENEFFECT_EP2_GROGGY,
};

// ============================================================================
//  Screenspace effect
// ============================================================================

class CEnvScreenEffect : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvScreenEffect" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvScreenEffect, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	virtual void ReceiveMessage( int classID, bf_read &msg );

private:
	[[= ks::reflect::Net{} ]] float	m_flDuration;
	[[= ks::reflect::Net{} ]] int		m_nType;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvScreenEffect, DT_EnvScreenEffect, CEnvScreenEffect )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : classID - 
//			&msg - 
//-----------------------------------------------------------------------------
void CEnvScreenEffect::ReceiveMessage( int classID, bf_read &msg )
{
	// Make sure our IDs match
	if ( classID != GetClientClass()->m_ClassID )
	{
		// Message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	int messageType = msg.ReadByte();
	switch( messageType )
	{
		// Effect turning on
		case 0: // FIXME: Declare
			{		
				// Create a keyvalue block to set these params
				KeyValues *pKeys = new KeyValues( "keys" );
				if ( pKeys == nullptr )
					return;

				if ( m_nType == SCREENEFFECT_EP1_INTRO )
				{
					// Set our keys
					pKeys->SetFloat( "duration", m_flDuration );
					pKeys->SetInt( "fadeout", 0 );

					g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "episodic_intro", pKeys );
					g_pScreenSpaceEffects->EnableScreenSpaceEffect( "episodic_intro" );
				}
				else if ( m_nType == SCREENEFFECT_EP2_ADVISOR_STUN )
				{
					// Set our keys
					pKeys->SetFloat( "duration", m_flDuration );

					g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "episodic_stun", pKeys );
					g_pScreenSpaceEffects->EnableScreenSpaceEffect( "episodic_stun" );
				}
				else if ( m_nType == SCREENEFFECT_EP2_GROGGY )
				{
					// Set our keys
					pKeys->SetFloat( "duration", m_flDuration );
					pKeys->SetInt( "fadeout", 0 );

					g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "ep2_groggy", pKeys );
					g_pScreenSpaceEffects->EnableScreenSpaceEffect( "ep2_groggy" );
				}
                
				pKeys->deleteThis();
			}
			break;

		// Effect turning off
		case 1:	// FIXME: Declare
			
			if ( m_nType == SCREENEFFECT_EP1_INTRO )
			{
				// Create a keyvalue block to set these params
				KeyValues *pKeys = new KeyValues( "keys" );
				if ( pKeys == nullptr )
					return;

				// Set our keys
				pKeys->SetFloat( "duration", m_flDuration );
				pKeys->SetInt( "fadeout", 1 );

				g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "episodic_intro", pKeys );
			}
			else if ( m_nType == SCREENEFFECT_EP2_ADVISOR_STUN )
			{
				g_pScreenSpaceEffects->DisableScreenSpaceEffect( "episodic_stun" );
			}
			else if ( m_nType == SCREENEFFECT_EP2_GROGGY )
			{
				// Create a keyvalue block to set these params
				KeyValues *pKeys = new KeyValues( "keys" );
				if ( pKeys == nullptr )
					return;

				// Set our keys
				pKeys->SetFloat( "duration", m_flDuration );
				pKeys->SetInt( "fadeout", 1 );

				g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "ep2_groggy", pKeys );
			}

			break;
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

extern bool g_bUseCustomAutoExposureMin;
extern bool g_bUseCustomAutoExposureMax;
extern bool g_bUseCustomBloomScale;
extern float g_flCustomAutoExposureMin;
extern float g_flCustomAutoExposureMax;
extern float g_flCustomBloomScale;
extern float g_flCustomBloomScaleMinimum;
extern float g_flBloomExponent;
extern float g_flBloomSaturation;
extern float g_flTonemapPercentTarget;
extern float g_flTonemapPercentBrightPixels;
extern float g_flTonemapMinAvgLum;
extern float g_flTonemapRate;

EHANDLE g_hTonemapControllerInUse = INVALID_EHANDLE;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvTonemapController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvTonemapController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvTonemapController, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CEnvTonemapController();

//private:
	[[= ks::reflect::Net{} ]] bool m_bUseCustomAutoExposureMin;
	[[= ks::reflect::Net{} ]] bool m_bUseCustomAutoExposureMax;
	[[= ks::reflect::Net{} ]] bool m_bUseCustomBloomScale;
	[[= ks::reflect::Net{} ]] float m_flCustomAutoExposureMin;
	[[= ks::reflect::Net{} ]] float m_flCustomAutoExposureMax;
	[[= ks::reflect::Net{} ]] float m_flCustomBloomScale;
	[[= ks::reflect::Net{} ]] float m_flCustomBloomScaleMinimum;
	[[= ks::reflect::Net{} ]] float m_flBloomExponent;
	[[= ks::reflect::Net{} ]] float m_flBloomSaturation;
	[[= ks::reflect::Net{} ]] float m_flTonemapPercentTarget;
	[[= ks::reflect::Net{} ]] float m_flTonemapPercentBrightPixels;
	[[= ks::reflect::Net{} ]] float m_flTonemapMinAvgLum;
	[[= ks::reflect::Net{} ]] float m_flTonemapRate;

private:
	CEnvTonemapController( const CEnvTonemapController & );
};

IMPLEMENT_REFLECT_CLIENTCLASS( CEnvTonemapController, DT_EnvTonemapController, CEnvTonemapController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvTonemapController::CEnvTonemapController( void )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
	m_bUseCustomBloomScale = false;
	m_flCustomAutoExposureMin = 0;
	m_flCustomAutoExposureMax = 0;
	m_flCustomBloomScale = 0.0f;
	m_flCustomBloomScaleMinimum = 0.0f;
	m_flBloomExponent = 2.5f;
	m_flBloomSaturation = 1.0f;
	m_flTonemapPercentTarget = 65.0f;
	m_flTonemapPercentBrightPixels = 2.0f;
	m_flTonemapMinAvgLum = 3.0f;
	m_flTonemapRate = 1.0f;
}

void GetTonemapSettingsFromEnvTonemapController( void )
{
	CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		CEnvTonemapController *tonemapController = dynamic_cast< CEnvTonemapController * >(localPlayer->m_hTonemapController.Get());
		if ( tonemapController != nullptr )
		{
			g_bUseCustomAutoExposureMin = tonemapController->m_bUseCustomAutoExposureMin;
			g_bUseCustomAutoExposureMax = tonemapController->m_bUseCustomAutoExposureMax;
			g_bUseCustomBloomScale = tonemapController->m_bUseCustomBloomScale;
			g_flCustomAutoExposureMin = tonemapController->m_flCustomAutoExposureMin;
			g_flCustomAutoExposureMax = tonemapController->m_flCustomAutoExposureMax;
			g_flCustomBloomScale = tonemapController->m_flCustomBloomScale;
			g_flCustomBloomScaleMinimum = tonemapController->m_flCustomBloomScaleMinimum;

			g_flBloomExponent = tonemapController->m_flBloomExponent;
			g_flBloomSaturation = tonemapController->m_flBloomSaturation;
			g_flTonemapPercentTarget = tonemapController->m_flTonemapPercentTarget;
			g_flTonemapPercentBrightPixels = tonemapController->m_flTonemapPercentBrightPixels;
			g_flTonemapMinAvgLum = tonemapController->m_flTonemapMinAvgLum;
			g_flTonemapRate = tonemapController->m_flTonemapRate;
			return;
		}
	}

	g_bUseCustomAutoExposureMax = false;
	g_bUseCustomBloomScale = false;

	g_flBloomExponent = 2.5f;
	g_flBloomSaturation = 1.0f;
	g_flTonemapPercentTarget = 65.0f;
	g_flTonemapPercentBrightPixels = 2.0f;
	g_flTonemapMinAvgLum = 3.0f;
	g_flTonemapRate = 1.0f;
}

using namespace ::ks::reflect;

// Purpose: Information about algorithmic stuff that can occur on both client + server
// In order to reduce network traffic, it's possible to create a algorithms
// that will work on both the client and the server and be totally repeatable.
// All we need do is to send down initial conditions and let the algorithm
// compute the values at various times. Note that this algorithm will be called
// at different times with different frequencies on the client and server.
// The trick here is that in order for it to be repeatable, the algorithm either
// cannot depend on random numbers, or, if it does, we need to make sure that
// the random numbers generated are effectively done at the beginning of time,
// so that differences in frame rate on client and server won't matter. It also
// is important that the initial state sent across the network is identical
// bitwise so that we produce the exact same results. Therefore no compression
// should be used in the datatables.
// Note also that each algorithm must have its own random number stream so that
// it cannot possibly interact with other code using random numbers that will
// be called at various different intervals on the client + server. Use the
// CUniformRandomStream class for this.
// There are two types of client-server neutral code: Code that doesn't interact
// with player prediction, and code that does. The code that doesn't interact
// with player prediction simply has to be able to produce the result f(time)
// where time is monotonically increasing. For prediction, we have to produce
// the result f(time) where time does *not* monotonically increase (time can be
// anywhere between the "current" time and the prior 10 seconds).
// Code that is not used by player prediction can maintain state because later
// calls will always compute the value at some future time. This computation can
// use random number generation, but with the following restriction: Your code
// must generate exactly the same number of random numbers regardless of how
// frequently the code is called.
// In specific, this means that all random numbers used must either be computed
// at init time, or must be used in an 'event-based form'. Namely, use random
// numbers to compute the time at which events occur and the random inputs for
// those events.  When simulating forward, you must simulate all intervening
// time and generate the same number of random numbers.
// For functions planned to be used by player prediction, one method is to use
// some sort of stateless computation (where the only states are the initial
// state and time). Note that random number generators have state implicit in
// the number of calls made to that random number generator, and therefore you
// cannot call a random number generator unless you are able to
// 1) Use a random number generator that can return the ith random number, namely:
//	float r = random( i );	// i == the ith number in the random sequence
// 2) Be able to accurately know at any given time t how many random numbers
//		have already been generated (namely, compute the i in part 1 above).
// There is another alternative for code meant to be used by player prediction:
// you could just store a history of 'events' from which you could completely
// determine the value of f(time). That history would need to be at least 10
// seconds long, which is guaranteed to be longer than the amount of time that
// prediction would need. I've written a class which I haven't tested yet (but
// will be using soon) called CTimedEventQueue (currently located in
// env_wind_shared.h) which I plan to use to solve my problem (getting wind to
// blow players).

// Test concommand for wind/tree sway. Couldn't think of a better way to put it.
// Will move it out of this file when we figure out how the weather control will be implemented.
static void cl_tree_sway_dir( const CCommand &args );
static ConCommand cl_tree_sway_dir_command( side_registry(), "cl_tree_sway_dir", cl_tree_sway_dir, "sets tree sway wind direction and strength" );
static void cl_tree_sway_dir( const CCommand &args )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( args.ArgC() == 3 )
	{
		Vector windDir;
		windDir.x = V_atof( args.Arg( 1 ) );
		windDir.y = V_atof( args.Arg( 2 ) );
		windDir.z = 0;
		pRenderContext->SetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION, windDir );
	}
}

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

static std::vector< CEnvWindShared * > s_windControllers;

CEnvWindShared::CEnvWindShared()
{
	m_pWindSound = nullptr;
	s_windControllers.push_back( this );
}

CEnvWindShared::~CEnvWindShared()
{
	if (m_pWindSound)
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pWindSound );
	}
	ks::erase_first( s_windControllers, this );
}

void CEnvWindShared::Init( int nEntIndex, int iRandomSeed, float flTime, 
						  int iInitialWindYaw, float flInitialWindSpeed )
{
	m_iEntIndex = nEntIndex;
	m_flWindAngleVariation = m_flWindSpeedVariation = 1.0f;
	m_flStartTime = m_flSimTime = m_flSwitchTime = m_flVariationTime = flTime;
	m_iWindSeed = iRandomSeed;
	m_Stream.SetSeed( iRandomSeed );
	m_WindVariationStream.SetSeed( iRandomSeed );
	m_iWindDir = m_iInitialWindDir = iInitialWindYaw;
	// Bound it for networking as a postive integer
	m_iInitialWindDir = (int)( anglemod( m_iInitialWindDir ) );

	m_flAveWindSpeed = m_flWindSpeed = m_flInitialWindSpeed = flInitialWindSpeed;

	/*
	// Cache in the wind sound...
	if (!g_pEffects->IsServer())
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		m_pWindSound = controller.SoundCreate( -1, CHAN_STATIC, 
			"EnvWind.Loop", ATTN_NONE );
		controller.Play( m_pWindSound, 0.0f, 100 );
	}
	*/

	// Next time a change happens (which will happen immediately), it'll stop gusting
	m_bGusting = true;
}

//-----------------------------------------------------------------------------
// Computes wind variation
//-----------------------------------------------------------------------------

static constexpr auto WIND_VARIATION_UPDATE_TIME = 0.1f;

void CEnvWindShared::ComputeWindVariation( float flTime )
{
	// The wind variation is updated every 10th of a second..
	while( flTime >= m_flVariationTime )
	{
		m_flWindAngleVariation = m_WindVariationStream.RandomFloat( -10, 10 );
		m_flWindSpeedVariation = 1.0 + m_WindVariationStream.RandomFloat( -0.2, 0.2 );
		m_flVariationTime += WIND_VARIATION_UPDATE_TIME;
	}
}

//-----------------------------------------------------------------------------
// Updates the wind sound
//-----------------------------------------------------------------------------
void CEnvWindShared::UpdateWindSound( float flTotalWindSpeed )
{
	if (!g_pEffects->IsServer())
	{
		float flDuration = random_valve->RandomFloat( 1.0f, 2.0f );
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		// FIXME: Tweak with these numbers
		float flNormalizedWindSpeed = flTotalWindSpeed / 150.0f;
		if (flNormalizedWindSpeed > 1.0f)
			flNormalizedWindSpeed = 1.0f;
		float flPitch = 120 * Bias( flNormalizedWindSpeed, 0.3f ) + 100;
		float flVolume = 0.3f * Bias( flNormalizedWindSpeed, 0.3f ) + 0.7f;
		controller.SoundChangePitch( m_pWindSound, flPitch, flDuration );
		controller.SoundChangeVolume( m_pWindSound, flVolume, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Updates the swaying of trees
//-----------------------------------------------------------------------------
static constexpr auto TREE_SWAY_UPDATE_TIME = 2.0f;

void CEnvWindShared::UpdateTreeSway( float flTime )
{
	while( flTime >= m_flSwayTime )
	{
		// Since the wind is constantly changing, but we need smooth values, we cache them off here.
		m_PrevSwayVector = m_CurrentSwayVector;
		m_CurrentSwayVector = m_currentWindVector;
		m_flSwayTime += TREE_SWAY_UPDATE_TIME;
	}

	// Update vertex shader
	float flPercentage = ( 1 - ( ( m_flSwayTime - flTime ) / TREE_SWAY_UPDATE_TIME ) );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	// Dividing by 2 helps the numbers the shader is expecting stay in line with other expected game values.
	Vector vecWind = Lerp( flPercentage, m_PrevSwayVector, m_CurrentSwayVector ) / 2;
	pRenderContext->SetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION, vecWind );
}

//-----------------------------------------------------------------------------
// Updates the wind speed
//-----------------------------------------------------------------------------

static constexpr auto WIND_ACCELERATION = 150.0f; // wind speed can accelerate this many units per second
static constexpr auto WIND_DECELERATION = 15.0f; // wind speed can decelerate this many units per second

float CEnvWindShared::WindThink( float flTime )
{
	// NOTE: This algorithm can be client-server neutal because we're using 
	// the random number generator to generate *time* at which the wind changes.
	// We therefore need to structure the algorithm so that no matter the
	// frequency of calls to this function we produce the same wind speeds...

	ComputeWindVariation( flTime );

	// Update Tree Sway
	UpdateTreeSway( flTime );

	while (true)
	{
		// First, simulate up to the next switch time...
		float flTimeToSwitch = m_flSwitchTime - m_flSimTime;
		float flMaxDeltaTime = flTime - m_flSimTime;

		bool bGotToSwitchTime = (flMaxDeltaTime > flTimeToSwitch);

		float flSimDeltaTime = bGotToSwitchTime ? flTimeToSwitch : flMaxDeltaTime;

		// Now that we've chosen 
		// either ramp up, or sleep till change
		bool bReachedSteadyState = true;
		if ( m_flAveWindSpeed > m_flWindSpeed )
		{
			m_flWindSpeed += WIND_ACCELERATION * flSimDeltaTime;
			if (m_flWindSpeed > m_flAveWindSpeed)
				m_flWindSpeed = m_flAveWindSpeed;
			else
				bReachedSteadyState = false;
		}
		else if ( m_flAveWindSpeed < m_flWindSpeed )
		{
			m_flWindSpeed -= WIND_DECELERATION * flSimDeltaTime;
			if (m_flWindSpeed < m_flAveWindSpeed)
				m_flWindSpeed = m_flAveWindSpeed;
			else
				bReachedSteadyState = false;
		}

		// Update the sim time

		// If we didn't get to a switch point, then we're done simulating for now 
		if (!bGotToSwitchTime)
		{
			m_flSimTime = flTime;

			// We're about to exit, let's set the wind velocity...
			QAngle vecWindAngle( 0, m_iWindDir + m_flWindAngleVariation, 0 );
			AngleVectors( vecWindAngle, &m_currentWindVector );
			float flTotalWindSpeed = m_flWindSpeed * m_flWindSpeedVariation;
			g_SoftbodyEnvironment.SetWindDesc( m_currentWindVector, flTotalWindSpeed );
			m_currentWindVector *= flTotalWindSpeed;

			// If we reached a steady state, we don't need to be called until the switch time
			// Otherwise, we should be called immediately

			// FIXME: If we ever call this from prediction, we'll need
			// to only update the sound if it's a new time
			// Or, we'll need to update the sound elsewhere.
			// Update the sound....
//			UpdateWindSound( flTotalWindSpeed );

			// Always immediately call, the wind is forever varying
			return ( flTime + 0.01f );
		}

		m_flSimTime = m_flSwitchTime;

		// Switch gusting state..
		if( m_bGusting )
		{
			// wind is gusting, so return to normal wind
			m_flAveWindSpeed = m_Stream.RandomInt( m_iMinWind, m_iMaxWind );

			// set up for another gust later
			m_bGusting = false;
			m_flSwitchTime += m_flMinGustDelay + m_Stream.RandomFloat( 0, m_flMaxGustDelay );

		}
		else
		{
			// time for a gust.
			m_flAveWindSpeed = m_Stream.RandomInt( m_iMinGust, m_iMaxGust );

			// change wind direction, maybe a lot
			m_iWindDir = anglemod( m_iWindDir + m_Stream.RandomInt(-m_iGustDirChange, m_iGustDirChange) );

			// set up to stop the gust in a short while
			m_bGusting = true;

			// !!!HACKHACK - gust duration tied to the length of a particular wave file
			m_flSwitchTime += m_flGustDuration;
		}
	}
}

//-----------------------------------------------------------------------------
// Method to reset wind speed..
//-----------------------------------------------------------------------------
void ResetWindspeed()
{
	for ( CEnvWindShared *pController : s_windControllers )
	{
		pController->m_currentWindVector.Init( 0, 0, 0 );
	}

	g_SoftbodyEnvironment.SetNoWind();
}

//-----------------------------------------------------------------------------
// GetWindspeedAtTime was never finished to actually take time in to consideration.  We don't need 
// features that aren't written, but we do need to have multiple wind controllers on a map, so
// we need to find the one that is affecting the given location and return its speed.
//-----------------------------------------------------------------------------
Vector GetWindspeedAtLocation( const Vector &location )
{
	for ( CEnvWindShared *thisWindController : s_windControllers )
	{
		float distance = (thisWindController->m_location - location).Length();

		if( distance < thisWindController->m_windRadius )
		{
			// This location is within our area of influence, so return our computer wind vector
			return thisWindController->m_currentWindVector;
		}
	}

	for ( CEnvWindShared *thisWindController : s_windControllers )
	{

		if( thisWindController->m_windRadius == -1.0f )
		{
			// We do a second search for a global controller so you don't have to worry about order in the list.  
			return thisWindController->m_currentWindVector;
		}
	}

	return Vector(0,0,0);// No wind
}

//-----------------------------------------------------------------------------
// Method to sample the windspeed at a particular time
//-----------------------------------------------------------------------------
void GetWindspeedAtTime( float flTime, Vector &vecVelocity )
{
	// For now, ignore history and time.. fix later when we use wind to affect
	// client-side prediction
	if ( s_windControllers.empty() )
	{
		vecVelocity.Init( 0, 0, 0 );
	}
	else
	{
		VectorCopy( s_windControllers.front()->m_currentWindVector, vecVelocity );
	}
}

//-----------------------------------------------------------------------------
void CEnvWindShared::SetLocation( const Vector &location )
{
	m_location = location;
}
