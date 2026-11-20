// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdio.h>
#include <stdlib.h>
#include "edict.h"
#include "cdll_util.h"
#include "predictable_entity.h"
#include "viewangleanim.h"
#include "ivrenderview.h"
#include "materialsystem/icustommaterialmanager.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "view_shared.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :props;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :achievements_and_stats_interface;
import :iinput;
import :baseanimatedtextureproxy;
import :sharedvar;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import :noopanimcontroller;
import ks.core.platform;
import ks.core.vprof;
import ks.tier1.interfaces;
import ks.game.gamemovement;

using namespace ::ks::reflect;

extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;

// ConCommands useful for creating view animations
CViewAngleAnimation *g_pTestAnimation = nullptr;

// create a view animation object to be used for creating an animation. parameter is flags
static void viewanim_create( const CCommand &args );
static ConCommand viewanim_create_command( side_registry(), "viewanim_create", viewanim_create, "viewanim_create" );
static void viewanim_create( const CCommand &args )
{
	if ( g_pTestAnimation )
	{
		delete g_pTestAnimation;
		g_pTestAnimation = nullptr;
	}

	int flags = 0;
	if ( args.ArgC() > 1 )
	{
		flags = atoi( args[1] );
	}

	g_pTestAnimation = CREATE_ENTITY( CViewAngleAnimation, "viewangleanim" );

	if ( g_pTestAnimation )
	{
		g_pTestAnimation->Spawn();
	}	
}

// run the test animation
void TestViewAnim( void )
{
	if ( g_pTestAnimation )
	{
		QAngle angles;
		engine->GetViewAngles( angles );

		g_pTestAnimation->RunAnimation( angles );
	}
	else
		Msg( "No view anim created\n" );
}
ConCommand viewanim_test( side_registry(), "viewanim_test", TestViewAnim, "test view animation" );

// set view angles to (0,0,0)
void ResetViewAngles( void )
{
	// create a blank anim
	QAngle angles = vec3_angle;
	engine->SetViewAngles( angles );
}
ConCommand viewanim_reset( side_registry(), "viewanim_reset", ResetViewAngles, "reset view angles!", FCVAR_CHEAT );

// add a key frame to the test animation. first parameter is the time taken to get to this keyframe
static void viewanim_addkeyframe( const CCommand &args );
static ConCommand viewanim_addkeyframe_command( side_registry(), "viewanim_addkeyframe", viewanim_addkeyframe, "", FCVAR_CHEAT );
static void viewanim_addkeyframe( const CCommand &args )
{
	if ( g_pTestAnimation )
	{
		QAngle vecTarget;
		engine->GetViewAngles( vecTarget );

		float flDelay = 0.2;
		if (args.ArgC() > 1)
		{
			flDelay = atof( args[1] );
		}

		int iFlags = 0;
		if (args.ArgC() > 1)
		{
			iFlags = atof( args[2] );
		}

		g_pTestAnimation->AddKeyFrame( new CViewAngleKeyFrame( vecTarget, flDelay, iFlags ) );
	}
	else
		Msg( "No view anim created, use viewanim_create" );
}

// save the current test anim, pass filename
static void viewanim_save( const CCommand &args );
static ConCommand viewanim_save_command( side_registry(), "viewanim_save", viewanim_save, "Save current animation to file" );
static void viewanim_save( const CCommand &args )
{
	if (args.ArgC() < 2)
		return;

	if ( g_pTestAnimation )
	{	
		g_pTestAnimation->SaveAsAnimFile( args[1] );
	}
	else
	{
		Msg( "No view anim created\n" );
	}
}

// load a view animation file into the test anim
static void viewanim_load( const CCommand &args );
static ConCommand viewanim_load_command( side_registry(), "viewanim_load", viewanim_load, "load animation from file" );
static void viewanim_load( const CCommand &args )
{
	if (args.ArgC() < 2)
		return;

	if ( g_pTestAnimation )
	{	
		g_pTestAnimation->LoadViewAnimFile( args[1] );
	}
	else
		Msg( "No view anim created\n" );
}

LINK_ENTITY_TO_CLASS_CLIENTONLY( viewangleanim, CViewAngleAnimation );

CViewAngleAnimation::CViewAngleAnimation()
{
}

CViewAngleAnimation::~CViewAngleAnimation()
{
	DeleteKeyFrames();
}

void CViewAngleAnimation::Spawn( void )
{
	m_iFlags = 0;
	QAngle angles;
	engine->GetViewAngles( angles );	

	/*
	if ( m_iFlags & VIEWANIM_RELATIVE )
	{
		AddKeyFrame( new CViewAngleKeyFrame( vec3_angle, 0.0, 0 ) );

		// seed this so we can add keyframes and have them calc the delta properly
		m_vecBaseAngles = angles;
	}
	else
	{
		AddKeyFrame( new CViewAngleKeyFrame( angles, 0.0, 0 ) );
	}
	*/

	m_bFinished = true;	// don't run right away

	ClientEntityList().AddNonNetworkableEntity(	this );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CViewAngleAnimation::DeleteKeyFrames()
{
	int i, c;

	c = std::ssize( m_KeyFrames );
	for ( i = c - 1; i >= 0 ; --i )
	{
		delete m_KeyFrames[ i ];
	}
	m_KeyFrames.clear();
}

void CViewAngleAnimation::LoadViewAnimFile( const char *pKeyFrameFileName )
{
	DeleteKeyFrames();

	// load keyvalues from this file and stuff them in as keyframes
	KeyValues *pData = new KeyValues( pKeyFrameFileName );

	if ( false == pData->LoadFromFile( filesystem, pKeyFrameFileName, "GAME" ) )
	{
		Warning( "CViewAngleAnimation::LoadViewAnimFile failed to load script %s\n", pKeyFrameFileName );
		pData->deleteThis();
		return;
	}

	QAngle angles;
	float flTime;
	int iFlags;

	KeyValues *pKey = pData->GetFirstSubKey();

	while ( pKey )
	{
		// angles
		const char *pszAngles = pKey->GetString( "angles", "0 0 0" );
		sscanf( pszAngles, "%f %f %f", &angles[0], &angles[1], &angles[2] );

		// time
		flTime = pKey->GetFloat( "time", 0.001 );

		// flags
		iFlags = pKey->GetInt( "flags", 0 );

		AddKeyFrame( new CViewAngleKeyFrame( angles, flTime, iFlags ) );

		pKey = pKey->GetNextKey();
	}

	pData->deleteThis();
}

void CViewAngleAnimation::SaveAsAnimFile( const char *pKeyFrameFileName )
{
	// save all of our keyframes into the file
	KeyValues *pData = new KeyValues( pKeyFrameFileName );

	pData->SetInt( "flags", m_iFlags );

	KeyValues *pKey = new KeyValues( "keyframe" );
	int i;
	int c = std::ssize( m_KeyFrames );
	char buf[64];
	for ( i=0;i<c;i++ )
	{
		pKey = pData->CreateNewKey();

		V_snprintf( buf, sizeof(buf), "%f %f %f",
			m_KeyFrames[i]->m_vecAngles[0],
			m_KeyFrames[i]->m_vecAngles[1],
			m_KeyFrames[i]->m_vecAngles[2] );

		pKey->SetString( "angles", buf );
		pKey->SetFloat( "time", m_KeyFrames[i]->m_flTime );
		pKey->SetInt( "flags", m_KeyFrames[i]->m_iFlags );
	}

	pData->SaveToFile( filesystem, pKeyFrameFileName, nullptr );
	pData->deleteThis();
}

void CViewAngleAnimation::AddKeyFrame( CViewAngleKeyFrame *pKeyFrame )
{
	pKeyFrame->m_vecAngles -= m_vecBaseAngles;
	m_KeyFrames.push_back( pKeyFrame );
}

bool CViewAngleAnimation::IsFinished( void )
{
	return m_bFinished;
}

void CViewAngleAnimation::RunAnimation( QAngle angles )
{
	if ( m_KeyFrames.empty() )
	{
		Warning( "CViewAngleAnimation::RunAnimation called on an empty view animation\n" );
		return;
	}

	m_flAnimStartTime = gpGlobals->curtime;
	m_bFinished = false;
	m_vecBaseAngles = angles;

	m_iFlags = m_KeyFrames[0]->m_iFlags;

	if ( !( m_iFlags & VIEWANIM_RELATIVE ) )
	{
		m_KeyFrames[0]->m_vecAngles = angles;
	}
}

void CViewAngleAnimation::ClientThink()
{
	if ( IsFinished() )
		return;

	float flCurrentTime = gpGlobals->curtime - m_flAnimStartTime;

	if ( flCurrentTime < 0 )
		flCurrentTime = 0.001;

	// find two nearest points
	int i, c;
	c = std::ssize( m_KeyFrames );
	float flTime = 0;
	for ( i=0;i<c;i++ )
	{
		if ( flTime + m_KeyFrames[i]->m_flTime > flCurrentTime )
		{
			break;
		}

		flTime += m_KeyFrames[i]->m_flTime;
	}

	contract_assert( i > 0 );

	if ( i >= c )
	{
		if ( i > 0 )
		{
			// animation complete, set to end point
			SetAngles( m_KeyFrames[i-1]->m_vecAngles );
		}

		if ( m_pAnimCompleteCallback )
		{
			m_pAnimCompleteCallback();
		}

		m_bFinished = true;
		return;
	}

	if ( m_KeyFrames[i]->m_iFlags != m_iFlags )
	{
		if ( ( m_KeyFrames[i]->m_iFlags & VIEWANIM_RELATIVE ) && !( m_iFlags & VIEWANIM_RELATIVE ) )
		{
            // new relative position is current angles
			engine->GetViewAngles( m_vecBaseAngles );
		}

		// copy the rest over
		m_iFlags = m_KeyFrames[i]->m_iFlags;
	}

	// previous frame is m_KeyFrames[i-1];
	// next frame is m_KeyFrames[i];
	float flFraction = ( flCurrentTime - flTime ) / ( m_KeyFrames[i]->m_flTime );

	Vector v0, v1, v2, v3;

	if ( i-2 <= 0 )
	{
		QAngleToVector( m_KeyFrames[i-1]->m_vecAngles, v0 );
	}
	else
	{
		QAngleToVector( m_KeyFrames[i-2]->m_vecAngles, v0 );
	}

	QAngleToVector( m_KeyFrames[i-1]->m_vecAngles, v1 );
	QAngleToVector( m_KeyFrames[i]->m_vecAngles, v2 );

	if ( i+1 >= c )
	{
		QAngleToVector( m_KeyFrames[i]->m_vecAngles, v3 );
	}
	else
	{
		QAngleToVector( m_KeyFrames[i+1]->m_vecAngles, v3 );
	}

	Vector out;
	Catmull_Rom_Spline( v0,	v1, v2,	v3,	flFraction,	out );

	QAngle vecCalculatedAngles;
	QAngleToVector( out, vecCalculatedAngles );
	SetAngles( vecCalculatedAngles );
}

void CViewAngleAnimation::SetAngles( QAngle vecCalculatedAngles )
{
	if ( m_iFlags & VIEWANIM_RELATIVE )
		vecCalculatedAngles += m_vecBaseAngles;

	QAngle vecViewAngle;
	engine->GetViewAngles( vecViewAngle );

	if ( !(FBitSet( m_iFlags, VIEWANIM_IGNORE_X ) ) )
		vecViewAngle[PITCH] = vecCalculatedAngles[PITCH];

	if ( !(FBitSet( m_iFlags, VIEWANIM_IGNORE_Y ) ) )
		vecViewAngle[YAW] = vecCalculatedAngles[YAW];

	if ( !(FBitSet( m_iFlags, VIEWANIM_IGNORE_Z ) ) )
		vecViewAngle[ROLL] = vecCalculatedAngles[ROLL];

	// clamp pitch
	vecViewAngle[PITCH] = clamp( vecViewAngle[PITCH], -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat() );

	engine->SetViewAngles( vecViewAngle );
}

CViewAngleKeyFrame::CViewAngleKeyFrame( QAngle vecAngles, float flTime, int iFlags)
{
	m_vecAngles = vecAngles;
	m_flTime = flTime;
	m_iFlags = iFlags;
}

class CViewRender;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// debugging overlays
//-----------------------------------------------------------------------------
static ConVar cl_drawmaterial( side_registry(), "cl_drawmaterial", "", FCVAR_CHEAT, "Draw a particular material over the frame" );
static ConVar mat_showwatertextures( side_registry(), "mat_showwatertextures", "0", FCVAR_CHEAT );
static ConVar mat_wateroverlaysize( side_registry(), "mat_wateroverlaysize", "128" );
static ConVar mat_showframebuffertexture( side_registry(), "mat_showframebuffertexture", "0", FCVAR_CHEAT );
static ConVar mat_framebuffercopyoverlaysize( side_registry(), "mat_framebuffercopyoverlaysize", "128" );
static ConVar mat_showcamerarendertarget( side_registry(), "mat_showcamerarendertarget", "0", FCVAR_CHEAT );
static ConVar mat_camerarendertargetoverlaysize( side_registry(), "mat_camerarendertargetoverlaysize", "128", FCVAR_CHEAT );
static ConVar mat_hsv( side_registry(), "mat_hsv", "0", FCVAR_CHEAT );
static ConVar mat_yuv( side_registry(), "mat_yuv", "0", FCVAR_CHEAT );
static ConVar cl_overdraw_test( side_registry(), "cl_overdraw_test", "0", FCVAR_CHEAT | FCVAR_NEVER_AS_STRING );
static ConVar mat_drawTexture( side_registry(), "mat_drawTexture", "", 0, "Enable debug view texture" );
static ConVar mat_drawTextureScale( side_registry(), "mat_drawTextureScale", "1.0", 0, "Debug view texture scale" );

//-----------------------------------------------------------------------------
// debugging
//-----------------------------------------------------------------------------
// (the engine owns this cvar).
ConVar mat_wireframe( side_registry(), "mat_wireframe", "0", FCVAR_CHEAT );
const ConVar *sv_cheats = nullptr;
ConVar	mat_showlightmappage(  side_registry(), "mat_showlightmappage", "-1" ); // set this to the lightmap page that you want to see on screen, set to -1 to show nothing.
ConVar cl_drawshadowtexture( side_registry(), "cl_drawshadowtexture", "0", FCVAR_CHEAT );
extern ConVar cl_csm_debug_2d;
extern ConVar cl_csm_debug_3d;
ConVar cl_shadowtextureoverlaysize( side_registry(), "cl_shadowtextureoverlaysize", "256", FCVAR_CHEAT );

static ConVar r_flashlightdrawdepth( side_registry(), "r_flashlightdrawdepth", "0" );
static ConVar cl_custommaterial_debug_graph( side_registry(), "cl_custommaterial_debug_graph", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Lightmap debugging mode view
//-----------------------------------------------------------------------------
extern bool s_bCanAccessCurrentView;
class CLightmapDebugView : public CRendering3dView
{
public:
	explicit CLightmapDebugView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	void Draw()
	{
		AllowCurrentViewAccess( true );
		Frustum frustum;
		CMatRenderContextPtr pRenderContext( materials );
		render->Push3DView( pRenderContext, *this, 0, nullptr, frustum );
		BuildWorldRenderLists( true, -1, true, true );
		render->PopView( pRenderContext, frustum );
		AllowCurrentViewAccess( false );

		render->DrawLightmaps( m_pWorldRenderList, mat_showlightmappage.GetInt() );
	}

};

//-----------------------------------------------------------------------------
// Renders a material orthographically to screen...
//-----------------------------------------------------------------------------
static void RenderMaterial( const char *pMaterialName )
{
	// So it's not in the very top left
	float x = 100.0f, y = 100.0f;
	// float x = 0.0f, y = 0.0f;

	IMaterial *pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER, false );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( x, y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x + pMaterial->GetMappingWidth(), y, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x + pMaterial->GetMappingWidth(), y + pMaterial->GetMappingHeight(), 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( x, y + pMaterial->GetMappingHeight(), 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

static void OverlayWaterTexture( IMaterial *pMaterial, int xOffset, int yOffset, bool bFlip )
{
	// screen safe
	float xBaseOffset = 0;
	float yBaseOffset = 0;
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	float fFlip0 = bFlip ? 1.0f : 0.0f;
	float fFlip1 = bFlip ? 0.0f : 1.0f;

	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		float w = mat_wateroverlaysize.GetFloat();
		float h = mat_wateroverlaysize.GetFloat();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( xBaseOffset + xOffset * w, yBaseOffset + yOffset * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, fFlip1 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + ( xOffset + 1 ) * w, yBaseOffset + yOffset * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, fFlip1 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + ( xOffset + 1 ) * w, yBaseOffset + ( yOffset + 1 ) * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, fFlip0 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( xBaseOffset + xOffset * w, yBaseOffset + ( yOffset + 1 ) * h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, fFlip0 + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

static void OverlayWaterTextures( void )
{
	OverlayWaterTexture( materials->FindMaterial( "debug/debugreflect", nullptr ), 0, 0, false );
	OverlayWaterTexture( materials->FindMaterial( "debug/debugrefract", nullptr ), 0, 1, true );
}

void OverlayCameraRenderTarget( const char *pszMaterialName, float flX, float flY, float w, float h )
{
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	IMaterial *pMaterial;
	pMaterial = materials->FindMaterial( pszMaterialName, TEXTURE_GROUP_OTHER, true );
	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( flX, flY, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX+w, flY, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX+w, flY+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( flX, flY+h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

static void OverlayFrameBufferTexture( int nFrameBufferIndex )
{
	float offsetS = ( 0.5f / 256.0f );
	float offsetT = ( 0.5f / 256.0f );
	IMaterial *pMaterial;
	char buf[MAX_PATH];
	V_snprintf( buf, MAX_PATH, "debug/debugfbtexture%d", nFrameBufferIndex );
	pMaterial = materials->FindMaterial( buf, TEXTURE_GROUP_OTHER, true );
	if( !IsErrorMaterial( pMaterial ) )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pMaterial );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		float w = mat_framebuffercopyoverlaysize.GetFloat();
		float h = mat_framebuffercopyoverlaysize.GetFloat();

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( w * nFrameBufferIndex, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * ( nFrameBufferIndex + 1 ), 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 0.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * ( nFrameBufferIndex + 1 ), h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w * nFrameBufferIndex, h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f + offsetS, 1.0f + offsetT );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
// Debugging aid to display a texture
//-----------------------------------------------------------------------------
static void OverlayShowTexture( const char* textureName, float scale )
{
	bool			foundVar;
	IMaterial		*pMaterial;
	IMaterialVar	*BaseTextureVar;
	ITexture		*pTex;
	float			x, y, w, h;

	// ___error is created in code in CMaterialSystem::CreateDebugMaterials()
	pMaterial = materials->FindMaterial( "___error", TEXTURE_GROUP_OTHER, true );
	BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if (!foundVar)
		return;

	CMatRenderContextPtr pRenderContext( materials );

	if ( textureName && textureName[0] )
	{
		pTex = materials->FindTexture( textureName, TEXTURE_GROUP_OTHER, false );
		BaseTextureVar->SetTextureValue( pTex );

		w = pTex->GetActualWidth() * scale;
		h = pTex->GetActualHeight() * scale;
	}
	else
	{
		w = h = 64.0f * scale;
	}

	// Center relative to current viewport
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );
	x = ( nViewportWidth - w ) * 0.5f;
	y = ( nViewportHeight - h ) * 0.5f;

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x+w, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x+w, y+h, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( x, y+h, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	meshBuilder.End();
	pMesh->Draw();
}

static constexpr auto DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY = 60;
int m_iDebugCustomMaterialCountHistory[60] = {0};
float m_flDebugNextCustomMaterialCheckTime = 0.0f;

static void DebugOverlayNumActiveCustomMaterialsGraph()
{
	int iNumMaterials = g_pMaterialSystem->GetCustomMaterialManager()->DebugGetNumActiveCustomMaterials();

	IMaterial		*pMaterial;
	float			x, y;
	pMaterial = materials->FindMaterial( "vgui/white", TEXTURE_GROUP_OTHER, true );

	int backBufferWidth, backBufferHeight;
	materials->GetBackBufferDimensions( backBufferWidth, backBufferHeight );
	x = 64;
	y = (backBufferHeight / 5) * 3;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	int temp_x = x + ( 4*DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY );
	int temp_y = (backBufferHeight / 5);
	int iNumPrims = DEBUG_CUSTOM_MATERIAL_COUNT_HISTORY-1;

	//draw graph axis
	meshBuilder.Begin( pMesh, MATERIAL_LINES, iNumPrims + 2 );
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( temp_x, temp_y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3ub( 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	//draw graph segments in same batch
	for ( int i=0; i<iNumPrims; i++ )
	{
		temp_x = 4 + x + (i * 4);
		temp_y = y - (m_iDebugCustomMaterialCountHistory[i] / 2);
		meshBuilder.Position3f( temp_x, temp_y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 255 );
		meshBuilder.AdvanceVertex();
		temp_y = y - (m_iDebugCustomMaterialCountHistory[i+1] / 2);
		meshBuilder.Position3f( temp_x+4, temp_y, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color3ub( 255, 255, 255 );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	//update graph once per second
	if ( gpGlobals->curtime > m_flDebugNextCustomMaterialCheckTime || gpGlobals->curtime + 1.0f < m_flDebugNextCustomMaterialCheckTime )
	{
		m_flDebugNextCustomMaterialCheckTime = gpGlobals->curtime + 1.0f;

		for ( int i=0; i<iNumPrims; i++ )
			m_iDebugCustomMaterialCountHistory[i] = m_iDebugCustomMaterialCountHistory[i+1];
		m_iDebugCustomMaterialCountHistory[iNumPrims] = iNumMaterials;

		//draw the actual number of active materials at the head of the graph as text
		float fl_x = (float)(temp_x+10) / (float)(backBufferWidth);
		float fl_y = (float)(temp_y-10) / (float)(backBufferHeight);
		char szNumMaterials[32];
		V_snprintf( szNumMaterials, sizeof(szNumMaterials), "%i Active", iNumMaterials );
		NDebugOverlay::ScreenText( fl_x, fl_y, szNumMaterials, 255, 255, 255, 255, 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Debugging aid to display a color ramp
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Draws all the debugging info
//-----------------------------------------------------------------------------
void CDebugViewRender::Draw3DDebuggingInfo( const CViewSetup &view )
{
	CVProfScope _( "CViewRender::Draw3DDebuggingInfo", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( cl_csm_debug_3d.GetInt() )
	{
		g_CascadeLightManager.Draw3DDebugInfo();	
	}

	// Draw 3d overlays
	render->Draw3DDebugOverlays();

	// Draw the line file used for debugging leaks
	render->DrawLineFile();
}

//-----------------------------------------------------------------------------
// Draws all the debugging info
//-----------------------------------------------------------------------------
void CDebugViewRender::Draw2DDebuggingInfo( const CViewSetup &view )
{
	// HDRFIXME: Assert NULL rendertarget
	if ( mat_yuv.GetInt() )
	{
		IMaterial *pMaterial;
		pMaterial = materials->FindMaterial( "debug/yuv", TEXTURE_GROUP_OTHER, true );
		if( !IsErrorMaterial( pMaterial ) )
		{
			DrawScreenEffectMaterial( pMaterial, view.x, view.y, view.width, view.height );
		}
	}

	if ( mat_hsv.GetInt() )
	{
		IMaterial *pMaterial;
		pMaterial = materials->FindMaterial( "debug/hsv", TEXTURE_GROUP_OTHER, true );
		if( !IsErrorMaterial( pMaterial ) )
		{
			DrawScreenEffectMaterial( pMaterial, view.x, view.y, view.width, view.height );
		}
	}

	// Draw debugging lightmaps
	if ( mat_showlightmappage.GetInt() != -1 )
	{
		CLightmapDebugView clientView( static_cast<CViewRender *>( ::view ) );
		clientView.Setup( view );
		clientView.Draw();
	}

	if ( cl_drawshadowtexture.GetInt() )
	{
		int nSize = cl_shadowtextureoverlaysize.GetInt();
		g_pClientShadowMgr->RenderShadowTexture( nSize, nSize );
	}

	if ( cl_csm_debug_2d.GetInt() )
	{
		g_CascadeLightManager.Draw2DDebugInfo();
	}

	if ( cl_custommaterial_debug_graph.GetInt() )
	{
		DebugOverlayNumActiveCustomMaterialsGraph();
	}

	const char *pDrawMaterial = cl_drawmaterial.GetString();
	if ( pDrawMaterial && pDrawMaterial[0] )
	{
		RenderMaterial( pDrawMaterial ); 
	}

	if ( mat_showwatertextures.GetBool() )
	{
		OverlayWaterTextures();
	}

	if ( mat_showcamerarendertarget.GetBool() )
	{
		float w = mat_wateroverlaysize.GetFloat();
		float h = mat_wateroverlaysize.GetFloat();
		OverlayCameraRenderTarget( "debug/debugcamerarendertarget", 0, 0, w, h );
	}

	if ( mat_showframebuffertexture.GetBool() )
	{
		// HDRFIXME: Get rid of these rendertarget sets assuming that the assert at the top of this function is true.
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport( nullptr );
		OverlayFrameBufferTexture( 0 );
		OverlayFrameBufferTexture( 1 );
		pRenderContext->PopRenderTargetAndViewport( );
	}

	const char *pDrawTexture = mat_drawTexture.GetString();
	if ( pDrawTexture && pDrawTexture[0] )
	{		
		OverlayShowTexture( pDrawTexture, mat_drawTextureScale.GetFloat() );
	}

	if ( r_flashlightdrawdepth.GetBool() )
	{
		shadowmgr->DrawFlashlightDepthTexture( );
	}
}

//-----------------------------------------------------------------------------
// A console command allowing you to draw a material as an overlay
//-----------------------------------------------------------------------------
static void r_screenoverlay( const CCommand &args );
static ConCommand r_screenoverlay_command( side_registry(), "r_screenoverlay", r_screenoverlay, "Draw specified material as an overlay", FCVAR_CHEAT|FCVAR_SERVER_CAN_EXECUTE );
static void r_screenoverlay( const CCommand &args )
{
	if( args.ArgC() == 2 )
	{
		if ( !V_stricmp( "off", args[1] ) )
		{
			view->SetScreenOverlayMaterial( nullptr );
		}
		else
		{
			IMaterial *pMaterial = materials->FindMaterial( args[1], TEXTURE_GROUP_OTHER, false );
			if ( !IsErrorMaterial( pMaterial ) )
			{
				view->SetScreenOverlayMaterial( pMaterial );
			}
			else
			{
				view->SetScreenOverlayMaterial( nullptr );
			}
		}
	}
	else
	{
		IMaterial *pMaterial;
		pMaterial = view->GetScreenOverlayMaterial();
		Warning( "r_screenoverlay: %s\n", pMaterial ? pMaterial->GetName() : "off" );
	}
}

// Used to verify frame syncing.
void CDebugViewRender::GenerateOverdrawForTesting()
{

	if ( !cl_overdraw_test.GetInt() )
		return;

	for ( int i=0; i < 40; i++ )
	{
		g_SmokeFogOverlayAlpha = 20 / 255.0;
		
		g_SmokeFogOverlayColor.Init( 0.33f, 0.33f, 0.33f );

		DrawSmokeFogOverlay();
	}
	g_SmokeFogOverlayAlpha = 0;
}
