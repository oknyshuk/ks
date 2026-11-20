// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdlib.h>
#include <math.h>
#include "client_class.h"
#include "declare_class.h"
#include "engine/ivmodelinfo.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "reflect_annotations.h"
#include "reflect_recvtable.h"
#include "icliententity.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "shattersurfacetypes.h"
#include "imaterialproxydict.h"
#include "edict.h"
#include "irecipientfilter.h"
#include "const.h"
#include "reflect.h"
#include "predictable_entity.h"
#include "reflect_datamap.h"
#include "shareddefs.h"
#include "networkvar.h"
#include "view_shared.h"
#include "bspflags.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "soundinfo.h"
module ks.client;

import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector2d;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import std;
import ks.core.basetypes;
import ks.game.entity_cast;
import ks.math.vmatrix;
import ks.core.random;
import ks.core.vprof;
import ks.game.func_dust_shared;
import ks.tier1.vector_ops;
import :basetoggle;
import ks.math.ssemath;
import ks.tier1.createinterface;
import ks.core.platform;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

static constexpr auto VIEWER_PADDING = 80.0f;

class CFuncAreaPortalWindow : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncAreaPortalWindow" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CFuncAreaPortalWindow, CBaseEntity );

// Overrides.
public:
	RenderableTranslucencyType_t ComputeTranslucencyType( void );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool	ShouldReceiveProjectedTextures( int flags );

private:

	float			GetDistanceBlend();

public:
	[[= ks::reflect::Net{} ]] float			m_flFadeStartDist;	// Distance at which it starts fading (when <= this, alpha=m_flTranslucencyLimit).
	[[= ks::reflect::Net{} ]] float			m_flFadeDist;		// Distance at which it becomes solid.

	// 0-1 value - minimum translucency it's allowed to get to.
	[[= ks::reflect::Net{} ]] float			m_flTranslucencyLimit;

	[[= ks::reflect::Net{} ]] int				m_iBackgroundModelIndex;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncAreaPortalWindow, DT_FuncAreaPortalWindow, CFuncAreaPortalWindow )

RenderableTranslucencyType_t CFuncAreaPortalWindow::ComputeTranslucencyType( void )
{
	return RENDERABLE_IS_TRANSLUCENT;
}

int CFuncAreaPortalWindow::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if( !GetModel() )
		return 0;

	// Make sure we're a brush model.
	int modelType = modelinfo->GetModelType( GetModel() );
	if( modelType != mod_brush )
		return 0;

	// Draw the fading version.
	float flBlendAlpha = GetDistanceBlend(); 
	if ( flBlendAlpha == 0.0f )
		return 0;

	render->SetBlend( flBlendAlpha );

	DrawBrushModelMode_t mode = DBM_DRAW_ALL;
	if ( flags & STUDIO_TWOPASS )
	{
		mode = ( flags & STUDIO_TRANSPARENCY ) ? DBM_DRAW_TRANSLUCENT_ONLY : DBM_DRAW_OPAQUE_ONLY;
	}

	render->DrawBrushModelEx( 
		this, 
		(model_t *)GetModel(), 
		GetAbsOrigin(), 
		GetAbsAngles(), 
		mode );

	// Draw the optional foreground model next.
	// Only use the alpha in the texture from the thing in the front.
	if (m_iBackgroundModelIndex >= 0)
	{
		render->SetBlend( 1 );
		model_t *pBackground = ( model_t * )modelinfo->GetModel( m_iBackgroundModelIndex );
		if( pBackground && modelinfo->GetModelType( pBackground ) == mod_brush )
		{
			render->DrawBrushModelEx( 
				this, 
				pBackground, 
				GetAbsOrigin(), 
				GetAbsAngles(), 
				mode );
		}
	}

	return 1;
}

float CFuncAreaPortalWindow::GetDistanceBlend()
{
	// Get the viewer's distance to us.
	float flDist = CollisionProp()->CalcDistanceFromPoint( CurrentViewOrigin() );
	CBasePlayer *local = CBasePlayer::GetLocalPlayer();
	if ( local )
	{
		flDist *= local->GetFOVDistanceAdjustFactor();
	}
	
	return RemapValClamped( flDist, m_flFadeStartDist, m_flFadeDist, m_flTranslucencyLimit, 1 );
}

bool CFuncAreaPortalWindow::ShouldReceiveProjectedTextures( int flags )
{
	return false;
}

class CBaseEntity;

using namespace ::ks::reflect;

//==================================================
// CBreakableSurface
//==================================================

// Bits for m_nPanelBits
static constexpr auto BITS_PANEL_IS_SOLID = (1<<0);
static constexpr auto BITS_PANEL_IS_STALE = (1<<1);

bool CBreakableSurface::InLegalRange(int nWidth, int nHeight)
{ 
	return (nWidth < m_nNumWide && nHeight < m_nNumHigh && 
		nWidth >=0 && nHeight >= 0 );
}

bool CBreakableSurface::IsPanelSolid(int nWidth, int nHeight)
{ 
	return ( BITS_PANEL_IS_SOLID & m_nPanelBits[nWidth][nHeight] )!=0 ;
}

bool CBreakableSurface::IsPanelStale(int nWidth, int nHeight)
{ 
	return ( BITS_PANEL_IS_STALE & m_nPanelBits[nWidth][nHeight] )!=0 ; 
}

void CBreakableSurface::SetPanelSolid(int nWidth, int nHeight, bool value)
{ 
	if ( !InLegalRange( nWidth, nHeight ) )
		return;

	if ( value )
	{
		m_nPanelBits[nWidth][nHeight] |= BITS_PANEL_IS_SOLID;
	}
	else
	{
		m_nPanelBits[nWidth][nHeight] &= ~BITS_PANEL_IS_SOLID;
	}
}

void CBreakableSurface::SetPanelStale(int nWidth, int nHeight, bool value)
{ 
	if ( !InLegalRange( nWidth, nHeight) )
		return;

	if ( value )
	{
		m_nPanelBits[nWidth][nHeight] |= BITS_PANEL_IS_STALE;
	}
	else
	{
		m_nPanelBits[nWidth][nHeight] &= ~BITS_PANEL_IS_STALE;
	}
}

void CBreakableSurface::OnRestore()
{
	BaseClass::OnRestore();

	// FIXME:  This restores the general state, but not the random edge bits
	//  those would need to be serialized separately...
	// traverse everthing and restore bits
	// Initialize panels
	for (int w=0;w<m_nNumWide;w++)
	{ 
		for (int h=0;h<m_nNumHigh;h++)
		{
			// Force recomputation
			SetPanelSolid(w,h,IsPanelSolid(w,h));
			SetPanelStale(w,h,true);
			UpdateEdgeType( w, h, GetStyleType(w,h ) );
		}
	}
}

//Receive datatable
IMPLEMENT_REFLECT_CLIENTCLASS( CBreakableSurface, DT_BreakableSurface, CBreakableSurface )

//-----------------------------------------------------------------------------
// Gets at the cracked version of the material
//-----------------------------------------------------------------------------
void CBreakableSurface::FindCrackedMaterial()
{
	m_pCrackedMaterial.Init( nullptr );

	// First time we've seen it, get the material on the brush model
	int materialCount = modelinfo->GetModelMaterialCount( const_cast<model_t*>(GetModel()) );
	if( materialCount != 1 )
	{
		Warning( "Encountered func_breakablesurf that has a material applied to more than one surface!\n" );
		m_pCrackedMaterial.Init( "debug/debugempty", TEXTURE_GROUP_OTHER );
		return;
	}

	// Get at the first material; even if there are more than one.
	IMaterial* pMaterial;
	modelinfo->GetModelMaterials( const_cast<model_t*>(GetModel()), 1, &pMaterial );

	// The material should point to a cracked version of itself
	bool foundVar;
	IMaterialVar* pCrackName = pMaterial->FindVar( "$crackmaterial", &foundVar, false );
	if (foundVar)
	{
		m_pCrackedMaterial.Init( pCrackName->GetStringValue(), TEXTURE_GROUP_CLIENT_EFFECTS );
	}
	else
	{
		m_pCrackedMaterial.Init( pMaterial );
	}
}

//-----------------------------------------------------------------------------
// Gets at the base texture 
//-----------------------------------------------------------------------------

static ITexture* GetBaseTexture( IMaterial* pMaterial )
{
	bool foundVar;
	IMaterialVar* pTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if (!foundVar)
		return 0;

	return pTextureVar->GetTextureValue();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::InitMaterial(WinEdge_t nEdgeType, int nEdgeStyle, char const* pMaterialName)
{
	m_pEdge[nEdgeType][nEdgeStyle].m_nRenderIndex  = -1;
	m_pEdge[nEdgeType][nEdgeStyle].m_nStyle		   = nEdgeStyle;
	m_pEdge[nEdgeType][nEdgeStyle].m_pMaterialEdge.Init(pMaterialName, TEXTURE_GROUP_CLIENT_EFFECTS);
	m_pEdge[nEdgeType][nEdgeStyle].m_pMaterialEdgeTexture.Init( GetBaseTexture( m_pEdge[nEdgeType][nEdgeStyle].m_pMaterialEdge ) );
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CBreakableSurface::CBreakableSurface()
{
	m_vNormal.Init();
	m_vCorner.Init();
	m_bIsBroken = false;

	m_pCurrentDetailTexture = nullptr;

	V_memset( m_PrevRawPanelBitVec, 0xff, sizeof( m_PrevRawPanelBitVec ) );
}

CBreakableSurface::~CBreakableSurface()
{
}

void CBreakableSurface::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Initialize panels
		m_nNumWide = MAX_NUM_PANELS;
		m_nNumHigh = MAX_NUM_PANELS;
		for (int w=0;w<MAX_NUM_PANELS;w++)
		{ 
			for (int h=0;h<MAX_NUM_PANELS;h++)
			{
				SetPanelSolid(w,h,true);
				SetPanelStale(w,h,false);

				m_RawPanelBitVec.Set( w + h * MAX_NUM_PANELS, true );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CBreakableSurface::OnDataChanged( DataUpdateType_t updateType )
{
	CBaseEntity::OnDataChanged( updateType );

	// FIXME: Slightly bogus, recomputation only necessary when m_bBroken changed
	OnTranslucencyTypeChanged();

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Get at the cracked material
		FindCrackedMaterial();

		// Use same solid box for all breakable surfaces
		m_pMaterialBox.Init( "models/brokenglass/glassbroken_solid", TEXTURE_GROUP_MODEL );
		m_pMaterialBoxTexture.Init( GetBaseTexture( m_pMaterialBox ) );

		// NOTE: If you add or change this list of materials, change
		// the precache list on func_breakablesurf.cpp on the server.

		// Load the edge types and styles for the specific surface type
		if (m_nSurfaceType == SHATTERSURFACE_TILE)
		{
			InitMaterial(EDGE_NONE,	0,"models/brokentile/tilebroken_03a");
			InitMaterial(EDGE_FULL,	0,"models/brokentile/tilebroken_03b");
			InitMaterial(EDGE_LEFT,	0,"models/brokentile/tilebroken_03c");
			InitMaterial(EDGE_RIGHT,0,"models/brokentile/tilebroken_03d");

			InitMaterial(EDGE_NONE,	1,"models/brokentile/tilebroken_02a");
			InitMaterial(EDGE_FULL,	1,"models/brokentile/tilebroken_02b");
			InitMaterial(EDGE_LEFT,	1,"models/brokentile/tilebroken_02c");
			InitMaterial(EDGE_RIGHT,1,"models/brokentile/tilebroken_02d");

			InitMaterial(EDGE_NONE,	2,"models/brokentile/tilebroken_01a");
			InitMaterial(EDGE_FULL,	2,"models/brokentile/tilebroken_01b");
			InitMaterial(EDGE_LEFT,	2,"models/brokentile/tilebroken_01c");
			InitMaterial(EDGE_RIGHT,2,"models/brokentile/tilebroken_01d");
		}
		else 
		{
			InitMaterial(EDGE_NONE,	0,"models/brokenglass/glassbroken_03a");
			InitMaterial(EDGE_FULL,	0,"models/brokenglass/glassbroken_03b");
			InitMaterial(EDGE_LEFT,	0,"models/brokenglass/glassbroken_03c");
			InitMaterial(EDGE_RIGHT,0,"models/brokenglass/glassbroken_03d");

			InitMaterial(EDGE_NONE,	1,"models/brokenglass/glassbroken_02a");
			InitMaterial(EDGE_FULL,	1,"models/brokenglass/glassbroken_02b");
			InitMaterial(EDGE_LEFT,	1,"models/brokenglass/glassbroken_02c");
			InitMaterial(EDGE_RIGHT,1,"models/brokenglass/glassbroken_02d");

			InitMaterial(EDGE_NONE,	2,"models/brokenglass/glassbroken_01a");
			InitMaterial(EDGE_FULL,	2,"models/brokenglass/glassbroken_01b");
			InitMaterial(EDGE_LEFT,	2,"models/brokenglass/glassbroken_01c");
			InitMaterial(EDGE_RIGHT,2,"models/brokenglass/glassbroken_01d");
		}
	}
	
	bool changed = false;

	for ( int j = 0; j < m_nNumHigh; j++ )
	{
		for ( int i = 0; i < m_nNumWide; i++ )
		{
			int offset = i + j * m_nNumWide;

			bool newVal = m_RawPanelBitVec[ offset ];
			bool oldVal = m_PrevRawPanelBitVec[ offset ];

			if ( newVal != oldVal )
			{
				changed = true;
			}

			SetPanelSolid(i,j,newVal);

			if ( !newVal && changed )
			{
					
				// Mark these panels and being stale (need edge type updated)
				// We update them in one fell swoop rather than as each panel
				// is updated, so we don't have to do duplicate operations
				SetPanelStale(i,	j ,true);
				SetPanelStale(i,   j+1,true);
				SetPanelStale(i,   j-1,true);
				SetPanelStale(i-1, j  ,true);
				SetPanelStale(i+1, j  ,true);
				SetPanelStale(i+1, j+1,true);
				SetPanelStale(i-1, j+1,true);
				SetPanelStale(i+1, j-1,true);
				SetPanelStale(i-1, j-1,true);
			}
		}
	}

	if ( changed )
	{
		for (int width=0;width<m_nNumWide;width++)
		{
			for (int height=0;height<m_nNumHigh;height++)
			{
				if ( IsPanelStale(width,height) )
				{
					UpdateEdgeType( width, height );
				}
			}
		}
	}

	V_memcpy( m_PrevRawPanelBitVec, m_RawPanelBitVec.Base(), sizeof( m_PrevRawPanelBitVec ) );
}

RenderableTranslucencyType_t CBreakableSurface::ComputeTranslucencyType( void )
{
	// Not an identity brush if it's broken
	if ( m_bIsBroken )
		return RENDERABLE_IS_TRANSLUCENT;

	return BaseClass::ComputeTranslucencyType();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
int CBreakableSurface::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	// If I'm not broken draw normally
	if (m_bIsBroken)
		render->InstallBrushSurfaceRenderer( this );

	// If it's broken, always draw it translucent
	BaseClass::DrawModel( m_bIsBroken ? flags | STUDIO_TRANSPARENCY : flags, instance );

	// Remove our nonstandard brush surface renderer...
	render->InstallBrushSurfaceRenderer( 0 );

	return 0;
}

bool CBreakableSurface::RenderBrushModelSurface( IClientEntity* pBaseEntity, IBrushSurface* pBrushSurface )
{
	// If tile draw highlight for grout
	if (m_nSurfaceType == SHATTERSURFACE_TILE)
	{
		DrawRenderListHighlights(pBrushSurface);
	}
	DrawSolidBlocks(pBrushSurface);
	DrawRenderList(pBrushSurface);

	// Don't draw decals
	return false;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawRenderList(IBrushSurface* pBrushSurface)
{
	// Get width and height steps
	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthStep,vHeightStep;
	AngleVectors(vAngles,nullptr,&vWidthStep,&vHeightStep);
	vWidthStep	*= m_flPanelWidth;
	vHeightStep *= m_flPanelHeight;

	CMeshBuilder	pMeshBuilder;
	int				nCurStyle		= -1;
	int				nCurEdgeType	= -1;
	CMatRenderContextPtr pRenderContext( materials );
	for ( const Panel_t &panel : m_RenderList )
	{
	
		if (nCurStyle		!= panel.m_nStyle		||
			nCurEdgeType	!= panel.m_nEdgeType	)
		{
			nCurStyle	 = panel.m_nStyle;
			nCurEdgeType = panel.m_nEdgeType;

			m_pCurrentDetailTexture = m_pEdge[nCurEdgeType][nCurStyle].m_pMaterialEdgeTexture;
			pRenderContext->Flush(false);
			pRenderContext->Bind(m_pCrackedMaterial, (IClientRenderable*)this);
		}

		Vector vRenderPos = m_vCorner + 
							(panel.m_nWidth*vWidthStep)	+ 
							(panel.m_nHeight*vHeightStep);

		IMesh* pMesh = pRenderContext->GetDynamicMesh( );
		DrawOneEdge(pBrushSurface, pMesh,&pMeshBuilder,vRenderPos,vWidthStep,vHeightStep,(WinSide_t)panel.m_nSide);
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawRenderListHighlights(IBrushSurface* pBrushSurface)
{
	// Get width and height steps
	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthStep,vHeightStep;
	AngleVectors(vAngles,nullptr,&vWidthStep,&vHeightStep);
	vWidthStep	*= m_flPanelWidth;
	vHeightStep *= m_flPanelHeight;

	CMeshBuilder	pMeshBuilder;
	int				nCurStyle		= -1;
	int				nCurEdgeType	= -1;
	CMatRenderContextPtr pRenderContext( materials );
	for ( const Panel_t &panel : m_RenderList )
	{
		nCurStyle	 = panel.m_nStyle;
		nCurEdgeType = panel.m_nEdgeType;
		IMaterial *pMat = m_pEdge[nCurEdgeType][nCurStyle].m_pMaterialEdge;
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, pMat );

		Vector vRenderPos = m_vCorner + 
							(panel.m_nWidth*vWidthStep)	+ 
							(panel.m_nHeight*vHeightStep) +
							(0.30*m_vNormal);

		DrawOneHighlight(pBrushSurface, pMesh,&pMeshBuilder,vRenderPos,vWidthStep,vHeightStep,(WinSide_t)panel.m_nSide);
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CBreakableSurface::HavePanel(int nWidth, int nHeight)
{
	// If I'm off the edge, always give support
	if (!InLegalRange(nWidth,nHeight))
	{
		return true;
	}
	return (IsPanelSolid(nWidth,nHeight));
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::UpdateEdgeType(int nWidth, int nHeight, int forceStyle /*=-1*/ )
{
	contract_assert( forceStyle < NUM_EDGE_STYLES );

	// -----------------------------------
	//  Check edge conditions
	// -----------------------------------
	if (!InLegalRange(nWidth,nHeight))
	{
		return;
	}

	// ----------------------------------
	//  If solid has no edges
	// ----------------------------------
	if (IsPanelSolid(nWidth,nHeight))
	{
		return;
	}
	
	// Panel is no longer stale
	SetPanelStale(nWidth, nHeight,false);

	// ----------------------------------
	//  Set edge type base on neighbors
	// ----------------------------------
	bool bUp		= HavePanel(nWidth,   nHeight+1);
	bool bDown		= HavePanel(nWidth,   nHeight-1);
	bool bLeft		= HavePanel(nWidth-1, nHeight  );
	bool bRight		= HavePanel(nWidth+1, nHeight  );

	bool bUpLeft	= HavePanel(nWidth-1,  nHeight+1);
	bool bUpRight	= HavePanel(nWidth+1,  nHeight+1);
	bool bDownLeft	= HavePanel(nWidth-1,  nHeight-1);
	bool bDownRight	= HavePanel(nWidth+1,  nHeight-1);

	//-------------
	// Top
	//-------------
	if (bUp)
	{
		bool bLeftEdge		= !bLeft  && bUpLeft;
		bool bRightEdge		= !bRight && bUpRight;

		if (bLeftEdge && bRightEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_TOP, EDGE_FULL, forceStyle );
		}
		else if (bLeftEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_TOP, EDGE_LEFT, forceStyle );
		}
		else if (bRightEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_TOP, EDGE_RIGHT, forceStyle );
		}
		else
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_TOP, EDGE_NONE, forceStyle );
		}
	}
	else
	{
		AddToRenderList(nWidth, nHeight, WIN_SIDE_TOP, EDGE_NOT, forceStyle );
	}
	//-------------
	// Bottom
	//-------------
	if (bDown)
	{
		bool bLeftEdge		= !bLeft  && bDownLeft;
		bool bRightEdge		= !bRight && bDownRight;

		if (bLeftEdge && bRightEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_BOTTOM, EDGE_FULL, forceStyle );
		}
		else if (bLeftEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_BOTTOM, EDGE_RIGHT, forceStyle );
		}
		else if (bRightEdge)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_BOTTOM, EDGE_LEFT, forceStyle );
		}
		else
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_BOTTOM, EDGE_NONE, forceStyle );
		}
	}
	else
	{
		AddToRenderList(nWidth, nHeight, WIN_SIDE_BOTTOM, EDGE_NOT, forceStyle );
	}
	//-------------
	// Left
	//-------------
	if (bLeft)
	{
		bool bTop		= !bUp 	 && bUpLeft;
		bool bBottom	= !bDown && bDownLeft;

		if (bTop && bBottom)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_LEFT, EDGE_FULL, forceStyle );
		}
		else if (bTop)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_LEFT, EDGE_RIGHT, forceStyle );
		}
		else if (bBottom)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_LEFT, EDGE_LEFT, forceStyle );
		}
		else
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_LEFT, EDGE_NONE, forceStyle );
		}
	}
	else
	{
		AddToRenderList(nWidth, nHeight, WIN_SIDE_LEFT, EDGE_NOT, forceStyle );
	}
	//-------------
	// Right
	//-------------
	if (bRight)
	{
		bool bTop		= !bUp 	 && bUpRight;
		bool bBottom	= !bDown && bDownRight;

		if (bTop && bBottom)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_RIGHT, EDGE_FULL, forceStyle );
		}
		else if (bTop)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_RIGHT, EDGE_LEFT, forceStyle );
		}
		else if (bBottom)
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_RIGHT, EDGE_RIGHT, forceStyle );
		}
		else
		{
			AddToRenderList(nWidth, nHeight, WIN_SIDE_RIGHT, EDGE_NONE, forceStyle );
		}
	}
	else
	{
		AddToRenderList(nWidth, nHeight, WIN_SIDE_RIGHT, EDGE_NOT, forceStyle );
	}
}

//--------------------------------------------------------------------------------
// Purpose : Return index to panel in render list that meets these qualifications
// Input   :
// Output  :
//--------------------------------------------------------------------------------
int CBreakableSurface::FindRenderPanel(int nWidth, int nHeight, WinSide_t nWinSide)
{
	for ( int i = 0; i < (int)m_RenderList.size(); i++ )
	{
		if (m_RenderList[i].m_nSide			== nWinSide	&&
			m_RenderList[i].m_nWidth		== nWidth		&&
			m_RenderList[i].m_nHeight		== nHeight)
		{
			return i;
		}
	}
	return -1;
}

//----------------------------------------------------------------------------------
// Purpose : Returns first element in render list with the same edge type and style
// Input   :
// Output  :
//----------------------------------------------------------------------------------
int CBreakableSurface::FindFirstRenderTexture(WinEdge_t nEdgeType, int nStyle)
{
	for ( int i = 0; i < (int)m_RenderList.size(); i++ )
	{
		if (m_RenderList[i].m_nStyle		== nStyle		&&
			m_RenderList[i].m_nEdgeType		== nEdgeType	)
		{
			return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// Purpose : Add a edge to be rendered to the render list
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::AddToRenderList(int nWidth, int nHeight, WinSide_t nSide, WinEdge_t nEdgeType, int forceStyle )
{
	// -----------------------------------------------------
	// Try to find old panel
	int nOldPanelIndex = FindRenderPanel(nWidth,nHeight,nSide);

	// -----------------------------------------------------
	// If I have an old panel, get it's style and remove it
	// otherwise randomly chose a style
	int nStyle;
	if ( nOldPanelIndex >= 0 )
	{
		nStyle = m_RenderList[nOldPanelIndex].m_nStyle;
		m_RenderList.erase( m_RenderList.begin() + nOldPanelIndex );
	}
	else
	{
		nStyle = random_valve->RandomInt(0,NUM_EDGE_STYLES-1);
	}
		
	if ( forceStyle != -1 )
	{
		nStyle = forceStyle;
	}

	// -----------------------------------------------------
	// If my new panel has an edge, add it to render list
	if (nEdgeType != EDGE_NOT)
	{
		// Renderlist is sorted by texture type.  Find first element
		// that shares the same texture as the new panel
		int nTexIndex = FindFirstRenderTexture(nEdgeType, nStyle);

		// If texture was already in list, add after last use
		// Otherwise add to send of render list
		auto itNew = m_RenderList.insert( nTexIndex >= 0 ? m_RenderList.begin() + nTexIndex + 1 : m_RenderList.end(), Panel_t{} );

		// Now fill out my data
		itNew->m_nHeight	= nHeight;
		itNew->m_nWidth		= nWidth;
		itNew->m_nEdgeType	= nEdgeType;
		itNew->m_nSide		= nSide;
		itNew->m_nStyle		= nStyle;

		contract_assert( nStyle < NUM_EDGE_STYLES );
		SetStyleType( nWidth, nHeight, nStyle );
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawSolidBlocks(IBrushSurface* pBrushSurface)
{
	CMatRenderContextPtr pRenderContext( materials );

	m_pCurrentDetailTexture = m_pMaterialBoxTexture;

	// Gotta flush (in a non-stalling way) because we effectively 
	// have a new material due to the new base texture
	pRenderContext->Flush(false);
	pRenderContext->Bind(m_pCrackedMaterial, (IClientRenderable*)this);
	CMeshBuilder pMeshBuilder;

	// ---------------
	// Create panels
	// ---------------
	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthStep,vHeightStep;
	AngleVectors(vAngles,nullptr,&vWidthStep,&vHeightStep);
	vWidthStep	*= m_flPanelWidth;
	vHeightStep *= m_flPanelHeight;

	Vector vCurPos = m_vCorner;
	for (int width=0;width<m_nNumWide;width++)
	{
		int height;
		int nHCount = 0;
		for (height=0;height<m_nNumHigh;height++)
		{
			// Keep count of how many panes there are in a row
			if (IsPanelSolid(width,height))
			{
				nHCount++;
			}

			// Drow the strip and start counting again
			else if (nHCount > 0)
			{
				vCurPos = m_vCorner + vWidthStep*width + vHeightStep*(height-nHCount);
				IMesh* pMesh = pRenderContext->GetDynamicMesh( );
				DrawOneBlock(pBrushSurface, pMesh, &pMeshBuilder, vCurPos,vWidthStep,vHeightStep*nHCount);
				nHCount = 0;
			}
		}
		if (nHCount)
		{
			vCurPos = m_vCorner + vWidthStep*width + vHeightStep*(height-nHCount);
			IMesh* pMesh = pRenderContext->GetDynamicMesh( );
			DrawOneBlock(pBrushSurface, pMesh, &pMeshBuilder, vCurPos,vWidthStep,vHeightStep*nHCount);
		}
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawOneBlock(IBrushSurface* pBrushSurface, IMesh* pMesh, 
	CMeshBuilder *pMeshBuilder,  const Vector &vCurPos, const Vector &vWidthStep, 
	const Vector &vHeightStep)
{
	pMeshBuilder->Begin( pMesh, MATERIAL_QUADS, 1 );

	Vector2D texCoord, lightCoord;
	pBrushSurface->ComputeTextureCoordinate( vCurPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vCurPos, lightCoord );

	pMeshBuilder->Position3f( vCurPos.x, vCurPos.y, vCurPos.z );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2f( 0, 0, 1 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
 	pMeshBuilder->AdvanceVertex();

	Vector vNextPos = vCurPos + vWidthStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y, vNextPos.z );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2f( 0, 0, 0 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
 	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos + vHeightStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x , vNextPos.y, vNextPos.z );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2f( 0, 1, 0 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
 	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos - vWidthStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y , vNextPos.z);
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2f( 0, 1, 1 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
 	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->End();
	pMesh->Draw();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawOneEdge( IBrushSurface* pBrushSurface, IMesh* pMesh, 
	CMeshBuilder *pMeshBuilder,  const Vector &vStartPos,  const Vector &vWStep, 
	const Vector &vHStep, WinSide_t nEdge )
{
	pMeshBuilder->Begin( pMesh, MATERIAL_QUADS, 1 );

	Vector2D texCoord, lightCoord;
	pBrushSurface->ComputeTextureCoordinate( vStartPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vStartPos, lightCoord );

	pMeshBuilder->Position3f( vStartPos.x, vStartPos.y, vStartPos.z);
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();
	
	Vector vNextPos = vStartPos + vWStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x , vNextPos.y , vNextPos.z);
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos + vHStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y, vNextPos.z );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos - vWStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y, vNextPos.z );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	pMeshBuilder->TexCoord2fv( 2, texCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->End();
	pMesh->Draw();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DrawOneHighlight( IBrushSurface* pBrushSurface, IMesh* pMesh, 
	CMeshBuilder *pMeshBuilder,  const Vector &vStartPos,  const Vector &vWStep, 
	const Vector &vHStep, WinSide_t nEdge )
{
	Vector vColor = Vector(0.41,0.35,0.24);

	pMeshBuilder->Begin( pMesh, MATERIAL_QUADS, 1 );

	Vector2D texCoord, lightCoord;
	pBrushSurface->ComputeTextureCoordinate( vStartPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vStartPos, lightCoord );

	pMeshBuilder->Position3f( vStartPos.x, vStartPos.y, vStartPos.z);
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4f( vColor[0], vColor[1], vColor[2], 1.0);
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();
	
	Vector vNextPos = vStartPos + vWStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x , vNextPos.y , vNextPos.z);
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4f( vColor[0], vColor[1], vColor[2], 1.0);
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos + vHStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y, vNextPos.z );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4f( vColor[0], vColor[1], vColor[2], 1.0);
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	vNextPos = vNextPos - vWStep;

	pBrushSurface->ComputeTextureCoordinate( vNextPos, texCoord );
	pBrushSurface->ComputeLightmapCoordinate( vNextPos, lightCoord );

	pMeshBuilder->Position3f( vNextPos.x, vNextPos.y, vNextPos.z );
	pMeshBuilder->Normal3fv( m_vNormal.Base() );
	pMeshBuilder->Color4f( vColor[0], vColor[1], vColor[2], 1.0);
	pMeshBuilder->TexCoord2fv( 1, lightCoord.Base() );
	switch (nEdge)
	{
		case WIN_SIDE_RIGHT:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 0.0f );
			break;
		case WIN_SIDE_BOTTOM:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 0.0f );
			break;
		case WIN_SIDE_LEFT:
			pMeshBuilder->TexCoord2f( 0, 1.0f, 1.0f );
			break;
		case WIN_SIDE_TOP:
			pMeshBuilder->TexCoord2f( 0, 0.0f, 1.0f );
			break;
	}
 	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->End();
	pMesh->Draw();
}

bool CBreakableSurface::ShouldReceiveProjectedTextures( int flags )
{
	return false;
}

//------------------------------------------------------------------------------
// A material proxy that resets the texture to use the original surface texture
//------------------------------------------------------------------------------

CBreakableSurfaceProxy::CBreakableSurfaceProxy()
{
	m_BaseTextureVar = nullptr;
}

CBreakableSurfaceProxy::~CBreakableSurfaceProxy()
{
}

bool CBreakableSurfaceProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	return foundVar;
}

void CBreakableSurfaceProxy::OnBind( CBaseEntity *pC_BaseEntity )
{
	CBreakableSurface *pEnt = dynamic_cast< CBreakableSurface * >(pC_BaseEntity);
	if( !pEnt )
	{
		return;
	}

	// Use the current base texture specified by the suface
	m_BaseTextureVar->SetTextureValue( pEnt->m_pCurrentDetailTexture );
}

IMaterial *CBreakableSurfaceProxy::GetMaterial()
{
	if ( !m_BaseTextureVar )
		return nullptr;

	return m_BaseTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CBreakableSurfaceProxy, BreakableSurface );

class CBaseEntity;

using namespace ::ks::reflect;

class CFuncConveyor : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncConveyor" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncConveyor, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CFuncConveyor();

	float GetConveyorSpeed() { return m_flConveyorSpeed; }

private:
	[[= ks::reflect::Net{} ]] float m_flConveyorSpeed;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncConveyor, DT_FuncConveyor, CFuncConveyor )

CFuncConveyor::CFuncConveyor()
{
	m_flConveyorSpeed = 0.0;
}

class CConveyorMaterialProxy : public IMaterialProxy
{
public:
	CConveyorMaterialProxy();
	virtual ~CConveyorMaterialProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	CBaseEntity *BindArgToEntity( void *pArg );

	IMaterialVar *m_pTextureScrollVar;
};

CConveyorMaterialProxy::CConveyorMaterialProxy()
{
	m_pTextureScrollVar = nullptr;
}

CConveyorMaterialProxy::~CConveyorMaterialProxy()
{
}

bool CConveyorMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pScrollVarName = pKeyValues->GetString( "textureScrollVar" );
	if( !pScrollVarName )
		return false;

	bool foundVar;
	m_pTextureScrollVar = pMaterial->FindVar( pScrollVarName, &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

CBaseEntity *CConveyorMaterialProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
}

void CConveyorMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !pC_BaseEntity )
		return;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	if ( !pEntity )
		return;

	CFuncConveyor *pConveyor = dynamic_cast<CFuncConveyor*>(pEntity);

	if ( !pConveyor )
		return;

	if ( !m_pTextureScrollVar )
	{
		return;
	}

	float flConveyorSpeed	= pConveyor->GetConveyorSpeed();
	float flRate			= abs( flConveyorSpeed ) / 128.0;
	float flAngle			= (flConveyorSpeed >= 0) ? 180 : 0;

	float sOffset = gpGlobals->curtime * std::cos( flAngle * ( std::numbers::pi / 180.0f ) ) * flRate;
	float tOffset = gpGlobals->curtime * std::sin( flAngle * ( std::numbers::pi / 180.0f ) ) * flRate;
	
	// make sure that we are positive
	if( sOffset < 0.0f )
	{
		sOffset += 1.0f + -( int )sOffset;
	}
	if( tOffset < 0.0f )
	{
		tOffset += 1.0f + -( int )tOffset;
	}
			    
	// make sure that we are in a [0,1] range
	sOffset = sOffset - ( int )sOffset;
	tOffset = tOffset - ( int )tOffset;
	
	if (m_pTextureScrollVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)
	{
		VMatrix mat;
		MatrixBuildTranslation( mat, sOffset, tOffset, 0.0f );
		m_pTextureScrollVar->SetMatrixValue( mat );
	}
	else
	{
		m_pTextureScrollVar->SetVecValue( sOffset, tOffset, 0.0f );
	}
}

IMaterial *CConveyorMaterialProxy::GetMaterial()
{
	return m_pTextureScrollVar ? m_pTextureScrollVar->GetOwningMaterial() : nullptr;
}

EXPOSE_MATERIAL_PROXY( CConveyorMaterialProxy, ConveyorScroll );

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CFunc_Dust;
class SimpleParticle;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CFunc_Dust, DT_Func_Dust, CFunc_Dust )

// ------------------------------------------------------------------------------------ //
// CDustEffect implementation.
// ------------------------------------------------------------------------------------ //
static constexpr auto DUST_ACCEL = 50;

void CDustEffect::RenderParticles( CParticleRenderIterator *pIterator )
{
	const CFuncDustParticle *pParticle = (const CFuncDustParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Velocity.
		float flAlpha;
		if( m_pDust->m_DustFlags & DUSTFLAGS_FROZEN )
		{
			flAlpha = 1;
		}
		else
		{
			// Alpha.
			float flAngle = (pParticle->m_flLifetime / pParticle->m_flDieTime) * std::numbers::pi * 2;
			flAlpha = std::sin( flAngle - (std::numbers::pi * 0.5f) ) * 0.5f + 0.5f;
		}

		Vector tPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;

		if( -tPos.z <= m_pDust->m_DistMax )
		{
			flAlpha *= 1 + (tPos.z / m_pDust->m_DistMax);

			// Draw it.
			float flSize = pParticle->m_flSize;
			if( m_pDust->m_DustFlags & DUSTFLAGS_SCALEMOTES )
				flSize *= -tPos.z;

			RenderParticle_Color255Size(
				pIterator->GetParticleDraw(),
				tPos,
				Vector( m_pDust->m_Color.r, m_pDust->m_Color.g, m_pDust->m_Color.b ),
				flAlpha * m_pDust->m_Color.a,
				flSize
				);
		}

		pParticle = (const CFuncDustParticle*)pIterator->GetNext( sortKey );
	}
}

void CDustEffect::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	Vector vecWind;
	if ( m_pDust->m_bAffectedByWind )
		GetWindspeedAtTime( gpGlobals->curtime, vecWind );
	else
		vecWind = Vector( 0, 0, 0 );

	CFuncDustParticle *pParticle = (CFuncDustParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Velocity.
		if( !(m_pDust->m_DustFlags & DUSTFLAGS_FROZEN) )
		{
			// Kill the particle?
			pParticle->m_flLifetime += pIterator->GetTimeDelta();
			if( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			{
				pIterator->RemoveParticle( pParticle );
			}
			else
			{
				for ( int i = 0 ; i < 2 ; i++ )
				{
					if ( pParticle->m_vVelocity[i] < vecWind[i] )
					{
						pParticle->m_vVelocity[i] += ( gpGlobals->frametime * DUST_ACCEL );

						// clamp
						if ( pParticle->m_vVelocity[i] > vecWind[i] )
							pParticle->m_vVelocity[i] = vecWind[i];
					}
					else if (pParticle->m_vVelocity[i] > vecWind[i] )
					{
						pParticle->m_vVelocity[i] -= ( gpGlobals->frametime * DUST_ACCEL );

						// clamp.
						if ( pParticle->m_vVelocity[i] < vecWind[i] )
							pParticle->m_vVelocity[i] = vecWind[i];
					}
				}

				// Apply velocity.
				pParticle->m_Pos.MulAdd( pParticle->m_Pos, pParticle->m_vVelocity, pIterator->GetTimeDelta() );
			}
		}

		pParticle = (CFuncDustParticle*)pIterator->GetNext();
	}
}

// ------------------------------------------------------------------------------------ //
// CFunc_Dust implementation.
// ------------------------------------------------------------------------------------ //

CFunc_Dust::CFunc_Dust() : m_Effect( "CFunc_Dust" )
{
	m_Effect.m_pDust = this;
	m_Effect.SetDynamicallyAllocated( false ); // So it doesn't try to delete itself.
}

CFunc_Dust::~CFunc_Dust()
{
}

void CFunc_Dust::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if( updateType == DATA_UPDATE_CREATED )
	{
		m_hMaterial = m_Effect.GetPMaterial( "particle/sparkles" );

		m_Effect.SetSortOrigin( WorldSpaceCenter( ) );

		// Let us think each frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		
		// If we're setup to be frozen, just make a bunch of particles initially.
		if( m_DustFlags & DUSTFLAGS_FROZEN )
		{
			for( int i=0; i < m_SpawnRate; i++ )
			{
				AttemptSpawnNewParticle();
			}
		}
	}

	m_Spawner.Init( m_SpawnRate ); // N particles per second
}

void CFunc_Dust::ClientThink()
{
	// If frozen, don't make new particles.
	if( m_DustFlags & DUSTFLAGS_FROZEN )
		return;

	// Spawn particles?
	if( m_DustFlags & DUSTFLAGS_ON )
	{
		float flDelta = std::min( gpGlobals->frametime, 0.1f );
		while( m_Spawner.NextEvent( flDelta ) )
		{
			AttemptSpawnNewParticle();
		}
	}

	// Tell the particle manager our bbox.
	Vector vWorldMins, vWorldMaxs;
	CollisionProp()->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	vWorldMins -= Vector( m_flSizeMax, m_flSizeMax, m_flSizeMax );
	vWorldMaxs += Vector( m_flSizeMax, m_flSizeMax, m_flSizeMax );
	m_Effect.GetBinding().SetBBox( vWorldMins, vWorldMaxs );
}

bool CFunc_Dust::ShouldDraw()
{
	return false;
}

void CFunc_Dust::AttemptSpawnNewParticle()
{
	// Find a random spot inside our bmodel.
	static int nTests=10;

	for( int iTest=0; iTest < nTests; iTest++ )
	{
		Vector vPercent = RandomVector( 0, 1 );
		Vector vTest = WorldAlignMins() + (WorldAlignMaxs() - WorldAlignMins()) * vPercent;

		int contents = enginetrace->GetPointContents_Collideable( GetCollideable(), vTest );
		if( contents & CONTENTS_SOLID )
		{
			CFuncDustParticle *pParticle = (CFuncDustParticle*)m_Effect.AddParticle( 10, m_hMaterial, vTest );
			if( pParticle )
			{
				pParticle->m_vVelocity = RandomVector( -m_SpeedMax, m_SpeedMax );
				pParticle->m_vVelocity.z -= m_FallSpeed;

				pParticle->m_flLifetime = 0;
				pParticle->m_flDieTime = RandomFloat( m_LifetimeMin, m_LifetimeMax );

				if( m_DustFlags & DUSTFLAGS_SCALEMOTES )
					pParticle->m_flSize = RandomFloat( m_flSizeMin/10000.0f, m_flSizeMax/10000.0f );
				else
					pParticle->m_flSize = RandomFloat( m_flSizeMin, m_flSizeMax );
			
				pParticle->m_Color = m_Color;
			}

			break;
		}
	}
}

//
// Dust
//

//-----------------------------------------------------------------------------
// Spew out dust!
//-----------------------------------------------------------------------------
void FX_Dust( const Vector &vecOrigin, const Vector &vecDirection, float flSize, float flSpeed )
{
	CVProfScope _( "FX_Dust", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	
	int	numPuffs = (flSize*0.5f);

	if ( numPuffs < 1 )
		numPuffs = 1;
	if ( numPuffs > 32 )
		numPuffs = 32;

	float speed = flSpeed * 0.1f;

	if ( speed < 0 )
		speed = 1.0f;
	if (speed > 48.0f )
		speed = 48.0f;

	//FIXME: Better sampling area
	Vector offset = vecOrigin + ( vecDirection * flSize );

	//Find area ambient light color and use it to tint smoke
	Vector	worldLight = WorldGetLightForPoint( offset, true );

	// Throw puffs
	SimpleParticle particle;
	for ( int i = 0; i < numPuffs; i++ )
	{
		offset.Random( -(flSize*0.25f), flSize*0.25f );
		offset += vecOrigin + ( vecDirection * flSize );

		particle.m_Pos = offset;
		particle.m_flLifetime = 0.0f;
		particle.m_flDieTime  = random_valve->RandomFloat( 0.4f, 1.0f );
		
		particle.m_vecVelocity = vecDirection * random_valve->RandomFloat( speed*0.5f, speed ) * i;
		particle.m_vecVelocity[2] = 0.0f;

		int	color = random_valve->RandomInt( 48, 64 );

		particle.m_uchColor[0] = (color+16) + ( worldLight[0] * (float) color );
		particle.m_uchColor[1] = (color+8) + ( worldLight[1] * (float) color );
		particle.m_uchColor[2] = color + ( worldLight[2] * (float) color );

		particle.m_uchStartAlpha= random_valve->RandomInt( 64, 128 );
		particle.m_uchEndAlpha	= 0;
		particle.m_uchStartSize = random_valve->RandomInt( 2, 8 );
		particle.m_uchEndSize	= random_valve->RandomInt( 24, 48 );
		particle.m_flRoll		= random_valve->RandomInt( 0, 360 );
		particle.m_flRollDelta	= random_valve->RandomFloat( -0.5f, 0.5f );

		AddSimpleParticle( &particle, g_Mat_DustPuff[random_valve->RandomInt(0,1)] );
	}
}

class CTEDust: public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDust" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDust, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

				CTEDust();
	virtual		~CTEDust();

public:
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual	bool	ShouldDraw() { return true; }

public:

	[[= ks::reflect::Net{} ]] float		m_flSize;
	[[= ks::reflect::Net{} ]] float		m_flSpeed;
	[[= ks::reflect::Net{} ]] Vector		m_vecDirection;

protected:
	void		GetDustColor( Vector &color );
};

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEDust, DT_TEDust, CTEDust )

//==================================================
// CTEDust
//==================================================

CTEDust::CTEDust()
{
}

CTEDust::~CTEDust()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bNewEntity - whether or not to start a new entity
//-----------------------------------------------------------------------------
void CTEDust::PostDataUpdate( DataUpdateType_t updateType )
{
	FX_Dust( m_vecOrigin, m_vecDirection, m_flSize, m_flSpeed );
}

void TE_Dust( IRecipientFilter& filter, float delay,
			 const Vector &pos, const Vector &dir, float size, float speed )
{
	FX_Dust( pos, dir, size, speed );
}

class CBasePlayer;
class CFuncLadder;

using namespace ::ks::reflect;

std::vector< CFuncLadder * >	CFuncLadder::s_Ladders;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::CFuncLadder() :
	m_bDisabled( false )
{
	s_Ladders.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::~CFuncLadder()
{
	ks::erase_first( s_Ladders, this );
}

int CFuncLadder::GetLadderCount()
{
	return std::ssize( s_Ladders );
}

CFuncLadder *CFuncLadder::GetLadder( int index )
{
	if ( index < 0 || index >= std::ssize( s_Ladders ) )
		return nullptr;

	return s_Ladders[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::Spawn()
{
	BaseClass::Spawn();

	// Entity is symbolid
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	
	//AddFlag( FL_WORLDBRUSH );
	SetModelName( NULL_STRING );

	// Make entity invisible
	AddEffects( EF_NODRAW );
	// No model but should still network
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs = VEC_HULL_MAX;

	// This will swap them if they are inverted
	SetEndPoints( m_vecPlayerMountPositionTop, m_vecPlayerMountPositionBottom );

}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned or after reload from .sav file
//-----------------------------------------------------------------------------
void CFuncLadder::Activate()
{
	// Chain to base class
	BaseClass::Activate();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::SearchForDismountPoints()
{
}

void CFuncLadder::SetEndPoints( const Vector& p1, const Vector& p2 )
{
	m_vecPlayerMountPositionTop = p1;
	m_vecPlayerMountPositionBottom = p2;

	if ( m_vecPlayerMountPositionBottom.GetZ() > m_vecPlayerMountPositionTop.GetZ() )
	{
		Vector temp = m_vecPlayerMountPositionBottom;
		m_vecPlayerMountPositionBottom = m_vecPlayerMountPositionTop;
		m_vecPlayerMountPositionTop = temp;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::DrawDebugGeometryOverlays()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetTopPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionTop + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetBottomPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionBottom + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bottomToTopVec - 
//-----------------------------------------------------------------------------
void CFuncLadder::ComputeLadderDir( Vector& bottomToTopVec )
{
	Vector top;
	Vector bottom;

	GetTopPosition( top );
	GetBottomPosition( bottom );

	bottomToTopVec = top - bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CFuncLadder::GetDismountCount() const
{
	return std::ssize( m_Dismounts );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : CInfoLadderDismountHandle
//-----------------------------------------------------------------------------
CInfoLadderDismount *CFuncLadder::GetDismount( int index )
{
	if ( index < 0 || index >= std::ssize( m_Dismounts ) )
		return nullptr;
	return m_Dismounts[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			radius - 
//			list - 
//-----------------------------------------------------------------------------
void CFuncLadder::FindNearbyDismountPoints( const Vector& origin, float radius, std::vector< CInfoLadderDismountHandle >& list )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOn( CBasePlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOff( CBasePlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFuncLadder::DontGetOnLadder( void ) const
{
	return m_bFakeLadder;
}

IMPLEMENT_NETWORKCLASS_ALIASED( FuncLadder, DT_FuncLadder );

IMPLEMENT_REFLECT_TABLE( CFuncLadder, DT_FuncLadder );

LINK_ENTITY_TO_CLASS_ALIASED( func_useableladder, FuncLadder );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncLadder )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoLadderDismount::DrawDebugGeometryOverlays()
{
}

IMPLEMENT_NETWORKCLASS_ALIASED( InfoLadderDismount, DT_InfoLadderDismount );

IMPLEMENT_REFLECT_TABLE( CInfoLadderDismount, DT_InfoLadderDismount );

LINK_ENTITY_TO_CLASS_ALIASED( info_ladder_dismount, InfoLadderDismount );

bool CFuncLadder::IsEnabled() const
{
	return !m_bDisabled;
}

class CBaseEntity;

using namespace ::ks::reflect;

class CFunc_LOD : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Func_LOD" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFunc_LOD, CBaseEntity );
	DECLARE_CLIENTCLASS();

					CFunc_LOD();

// CBaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t type );

public:
// Replicated vars from the server.
// These are documented in the server-side entity.
public:
	[[= ks::reflect::Net{} ]] int m_nDisappearMinDist;
	[[= ks::reflect::Net{} ]] int m_nDisappearMaxDist;
};

// ------------------------------------------------------------------------- //
// Tables.
// ------------------------------------------------------------------------- //

// Datatable..
IMPLEMENT_REFLECT_CLIENTCLASS( CFunc_LOD, DT_Func_LOD, CFunc_LOD )

// ------------------------------------------------------------------------- //
// CFunc_LOD implementation.
// ------------------------------------------------------------------------- //

CFunc_LOD::CFunc_LOD()
{
	m_nDisappearMinDist = 5000;
	m_nDisappearMaxDist = 5800;
}

void CFunc_LOD::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);

	// Copy in fade parameters
	AlphaProp()->SetFade( 1.0f, m_nDisappearMinDist, m_nDisappearMaxDist );
}

using namespace ::ks::reflect;

// ------------------------------------------------------------------------
class CFuncMoveLinear: public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncMoveLinear" } ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{}, RecvProxy_LocalVelocity>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncMoveLinear, CBaseToggle );
	DECLARE_CLIENTCLASS();


	CFuncMoveLinear();
	virtual void OnDataChanged( DataUpdateType_t type );
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncMoveLinear, DT_FuncMoveLinear, CFuncMoveLinear )

CFuncMoveLinear::CFuncMoveLinear()
{
}

void CFuncMoveLinear::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetSolid(SOLID_VPHYSICS);
		VPhysicsInitShadow( false, false );
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

class CFuncOccluder : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncOccluder" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CFuncOccluder, CBaseEntity );

// Overrides.
public:
	virtual bool	ShouldDraw();
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

private:
	[[= ks::reflect::Net{} ]] int m_nOccluderIndex;
	[[= ks::reflect::Net{} ]] bool m_bActive;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncOccluder, DT_FuncOccluder, CFuncOccluder )

void CFuncOccluder::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	engine->ActivateOccluder( m_nOccluderIndex, m_bActive );
}

bool CFuncOccluder::ShouldDraw()
{
	return false;
}

int CFuncOccluder::DrawModel( int flags, const RenderableInstance_t &instance )
{
	contract_assert( 0 );
	return 0;
}

using namespace ::ks::reflect;

class CFuncReflectiveGlass : public CFuncBrush
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncReflectiveGlass" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncReflectiveGlass, CFuncBrush );
	DECLARE_CLIENTCLASS();

// CBaseEntity.
public:
	CFuncReflectiveGlass();
	virtual ~CFuncReflectiveGlass();

	virtual bool	ShouldDraw();

	CFuncReflectiveGlass	*m_pNext;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncReflectiveGlass, DT_FuncReflectiveGlass, CFuncReflectiveGlass )

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CEntityClassList<CFuncReflectiveGlass> g_ReflectiveGlassList;
template<> CFuncReflectiveGlass *CEntityClassList<CFuncReflectiveGlass>::m_pClassList = nullptr;

CFuncReflectiveGlass* GetReflectiveGlassList()
{
	return g_ReflectiveGlassList.m_pClassList;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CFuncReflectiveGlass::CFuncReflectiveGlass()
{
	g_ReflectiveGlassList.Insert( this );
}

CFuncReflectiveGlass::~CFuncReflectiveGlass()
{
	g_ReflectiveGlassList.Remove( this );
}

bool CFuncReflectiveGlass::ShouldDraw()
{
	return true;
}

//-----------------------------------------------------------------------------
// Do we have reflective glass in view?
//-----------------------------------------------------------------------------
bool IsReflectiveGlassInView( const CViewSetup& view, cplane_t &plane )
{
	// Early out if no cameras
	CFuncReflectiveGlass *pReflectiveGlass = GetReflectiveGlassList();
	if ( !pReflectiveGlass )
		return false;

	Frustum_t frustum;
	GeneratePerspectiveFrustum( view.origin, view.angles, view.zNear, view.zFar, view.fov, view.m_flAspectRatio, frustum );

	cplane_t localPlane;
	Vector vecOrigin, vecWorld, vecDelta, vecForward;
	AngleVectors( view.angles, &vecForward, nullptr, nullptr );

	for ( ; pReflectiveGlass != nullptr; pReflectiveGlass = pReflectiveGlass->m_pNext )
	{
		if ( pReflectiveGlass->IsDormant() )
			continue;

		Vector vecMins, vecMaxs;
		pReflectiveGlass->GetRenderBoundsWorldspace( vecMins, vecMaxs );
		if ( frustum.CullBox( vecMins, vecMaxs ) )
			continue;

		const model_t *pModel = pReflectiveGlass->GetModel();
		const matrix3x4_t& mat = pReflectiveGlass->EntityToWorldTransform();

		int nCount = modelinfo->GetBrushModelPlaneCount( pModel );
		for ( int i = 0; i < nCount; ++i )
		{
			modelinfo->GetBrushModelPlane( pModel, i, localPlane, &vecOrigin );

			MatrixTransformPlane( mat, localPlane, plane );			// Transform to world space
			VectorTransform( vecOrigin, mat, vecWorld );
					 
			if ( view.origin.Dot( plane.normal ) <= plane.dist )	// Check for view behind plane
				continue;
			
			VectorSubtract( vecWorld, view.origin, vecDelta );		// Backface cull
			if ( vecDelta.Dot( plane.normal ) >= 0 )
				continue;

			return true;
		}
	}

	return false;
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CFuncRotating : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncRotating" } ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_flSimulationTime", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }, RecvProxy_SimulationTime>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 1 }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 2 }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncRotating, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CFuncRotating();

private:
};

extern void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncRotating, DT_FuncRotating, CFuncRotating )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncRotating::CFuncRotating()
{
}

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;
struct Particle;

using namespace ::ks::reflect;

static constexpr auto SF_EMISSIVE = 0x00000001;

// ------------------------------------------------------------------------- //
// Definitions
// ------------------------------------------------------------------------- //

static Vector s_FadePlaneDirections[] =
{
	Vector( 1,0,0),
	Vector(-1,0,0),
	Vector(0, 1,0),
	Vector(0,-1,0),
	Vector(0,0, 1),
	Vector(0,0,-1)
};
#define NUM_FADE_PLANES	(sizeof(s_FadePlaneDirections)/sizeof(s_FadePlaneDirections[0]))

// ------------------------------------------------------------------------- //
// Classes
// ------------------------------------------------------------------------- //
class CFuncSmokeVolume : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncSmokeVolume" } ]]
	[[= ks::reflect::From<"m_Color1", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_Color2", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_MaterialName", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_ParticleDrawWidth", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_ParticleSpacingDistance", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_DensityRampSpeed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_RotationSpeed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_MovementSpeed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Density", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_maxDrawDistance", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Collision", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncSmokeVolume, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();

	CFuncSmokeVolume();
	~CFuncSmokeVolume();

	int IsEmissive( void ) { return ( m_spawnflags & SF_EMISSIVE ); }

private:
	class SmokeGrenadeParticle : public Particle
	{
	public:
		float				m_RotationFactor;
		float				m_CurRotation;
		float				m_FadeAlpha;		// Set as it moves around.
		unsigned char		m_ColorInterp;		// Amount between min and max colors.
		unsigned char		m_Color[4];
	};

// CBaseEntity.
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	
// IPrototypeAppEffect.
public:
	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	NotifyRemove();

private:
	// The SmokeEmitter represents a grid in 3D space.
	class SmokeParticleInfo
	{
	public:
		SmokeGrenadeParticle	*m_pParticle;
		int						m_TradeIndex;		// -1 if not exchanging yet.
		float					m_TradeClock;		// How long since they started trading.
		float					m_TradeDuration;	// How long the trade will take to finish.
		float					m_FadeAlpha;		// Calculated from nearby world geometry.
		unsigned char			m_Color[4];
	};

	inline int GetSmokeParticleIndex(int x, int y, int z)	
	{
		return z*m_xCount*m_yCount+y*m_xCount+x;
	}

	inline SmokeParticleInfo *GetSmokeParticleInfo(int x, int y, int z)	
	{
		return &m_pSmokeParticleInfos[GetSmokeParticleIndex(x,y,z)];
	}

	inline void	GetParticleInfoXYZ(int index, int &x, int &y, int &z)
	{
		contract_assert( index >= 0 && index < m_xCount * m_yCount * m_zCount );
		z = index / (m_xCount*m_yCount);
		int zIndex = z*m_xCount*m_yCount;
		y = (index - zIndex) / m_xCount;
		int yIndex = y*m_xCount;
		x = index - zIndex - yIndex;
	}

	inline bool IsValidXYZCoords(int x, int y, int z)
	{
		return x >= 0 && y >= 0 && z >= 0 && x < m_xCount && y < m_yCount && z < m_zCount;
	}

	inline Vector GetSmokeParticlePos(int x, int y, int z )	
	{
		return WorldAlignMins() + 
			Vector( x * m_SpacingRadius * 2 + m_SpacingRadius,
				    y * m_SpacingRadius * 2 + m_SpacingRadius,
				    z * m_SpacingRadius * 2 + m_SpacingRadius );
	}

	inline Vector GetSmokeParticlePosIndex(int index)
	{
		int x, y, z;
		GetParticleInfoXYZ(index, x, y, z);
		return GetSmokeParticlePos(x, y, z);
	}

	// Start filling the smoke volume
	void FillVolume();

private:
// State variables from server.
	color32 m_Color1;
	color32 m_Color2;
	char m_MaterialName[255];
	float m_ParticleDrawWidth;
	float m_ParticleSpacingDistance;
	float m_DensityRampSpeed;
	float m_RotationSpeed;
	float m_MovementSpeed;
	float m_Density;
	float m_maxDrawDistance;
	int	  m_spawnflags;

private:
	CFuncSmokeVolume( const CFuncSmokeVolume & );

	float				m_CurrentDensity;
	float				m_ParticleRadius;
	bool				m_bStarted;

	PMaterialHandle		m_MaterialHandle;

	SmokeParticleInfo	*m_pSmokeParticleInfos;
	int					m_xCount, m_yCount, m_zCount;
	float				m_SpacingRadius;

	Vector				m_MinColor;
	Vector				m_MaxColor;

	Vector m_vLastOrigin;
	QAngle m_vLastAngles;
	bool m_bFirstUpdate;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncSmokeVolume, DT_FuncSmokeVolume, CFuncSmokeVolume )

// Helpers.
// ------------------------------------------------------------------------- //

static inline void InterpColor(unsigned char dest[4], unsigned char src1[4], unsigned char src2[4], float percent)
{
	dest[0] = (unsigned char)(src1[0] + (src2[0] - src1[0]) * percent);
	dest[1] = (unsigned char)(src1[1] + (src2[1] - src1[1]) * percent);
	dest[2] = (unsigned char)(src1[2] + (src2[2] - src1[2]) * percent);
}

static inline int GetWorldPointContents(const Vector &vPos)
{
	return enginetrace->GetPointContents( vPos );
}

static inline void WorldTraceLine( const Vector &start, const Vector &end, int contentsMask, trace_t *trace )
{
	UTIL_TraceLine(start, end, contentsMask, nullptr, COLLISION_GROUP_NONE, trace);
}

static inline Vector EngineGetLightForPoint(const Vector &vPos)
{
	return engine->GetLightForPoint(vPos, true);
}

extern Vector g_vecRenderOrigin[ MAX_SPLITSCREEN_PLAYERS ];
static inline Vector& EngineGetVecRenderOrigin()
{
	return g_vecRenderOrigin[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

static inline float& EngineGetSmokeFogOverlayAlpha()
{
	return g_SmokeFogOverlayAlpha;
}

static inline CBaseEntity* ParticleGetEntity( int index )
{
	return cl_entitylist->GetEnt( index );
}

// ------------------------------------------------------------------------- //
// CFuncSmokeVolume
// ------------------------------------------------------------------------- //
CFuncSmokeVolume::CFuncSmokeVolume()
{
	m_bFirstUpdate = true;
	m_vLastOrigin.Init();
	m_vLastAngles.Init();

	m_pSmokeParticleInfos = nullptr;
	m_SpacingRadius = 0.0f;;
	m_ParticleRadius = 0.0f;
	m_MinColor.Init( 1.0, 1.0, 1.0 );
	m_MaxColor.Init( 1.0, 1.0, 1.0 );
}

CFuncSmokeVolume::~CFuncSmokeVolume()
{
	delete [] m_pSmokeParticleInfos;
}

void CFuncSmokeVolume::OnDataChanged( DataUpdateType_t updateType )
{		
	m_MinColor[0] = ( 1.0f / 255.0f ) * m_Color1.r;
	m_MinColor[1] = ( 1.0f / 255.0f ) * m_Color1.g;
	m_MinColor[2] = ( 1.0f / 255.0f ) * m_Color1.b;

	m_MaxColor[0] = ( 1.0f / 255.0f ) * m_Color2.r;
	m_MaxColor[1] = ( 1.0f / 255.0f ) * m_Color2.g;
	m_MaxColor[2] = ( 1.0f / 255.0f ) * m_Color2.b;

	m_ParticleRadius = m_ParticleDrawWidth * 0.5f;
	m_SpacingRadius = m_ParticleSpacingDistance * 0.5f;

	m_ParticleEffect.SetParticleCullRadius( m_ParticleRadius );

//	Warning( "m_Density: %f\n", m_Density );
//	Warning( "m_MovementSpeed: %f\n", m_MovementSpeed );
	
	if(updateType == DATA_UPDATE_CREATED)
	{
		Vector size = WorldAlignMaxs() - WorldAlignMins();
		m_xCount = 0.5f + ( size.x / ( m_SpacingRadius * 2.0f ) );
		m_yCount = 0.5f + ( size.y / ( m_SpacingRadius * 2.0f ) );
		m_zCount = 0.5f + ( size.z / ( m_SpacingRadius * 2.0f ) );
		m_CurrentDensity = m_Density;

		delete [] m_pSmokeParticleInfos;
		m_pSmokeParticleInfos = new SmokeParticleInfo[m_xCount * m_yCount * m_zCount];
		Start( ParticleMgr(), nullptr );
	}
	BaseClass::OnDataChanged( updateType );
}

void CFuncSmokeVolume::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if( !pParticleMgr->AddEffect( &m_ParticleEffect, this ) )
		return;

	m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial( m_MaterialName );
	FillVolume();

	m_bStarted = true;
}

void CFuncSmokeVolume::Update( float fTimeDelta )
{
	// Update our world space bbox if we've moved at all.
	// We do this manually because sometimes people make HUGE bboxes, and if they're constantly changing because their
	// particles wander outside the current bounds sometimes, it'll be linking them into all the leaves repeatedly.
	const Vector &curOrigin = GetAbsOrigin();
	const QAngle &curAngles = GetAbsAngles();
	if ( !VectorsAreEqual( curOrigin, m_vLastOrigin, 0.1 ) || 
		fabs( curAngles.x - m_vLastAngles.x ) > 0.1 || 
		fabs( curAngles.y - m_vLastAngles.y ) > 0.1 || 
		fabs( curAngles.z - m_vLastAngles.z ) > 0.1 ||
		m_bFirstUpdate )
	{
		m_bFirstUpdate = false;
		m_vLastAngles = curAngles;
		m_vLastOrigin = curOrigin;

		Vector vWorldMins, vWorldMaxs;
		CollisionProp()->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
		vWorldMins -= Vector( m_ParticleRadius, m_ParticleRadius, m_ParticleRadius );
		vWorldMaxs += Vector( m_ParticleRadius, m_ParticleRadius, m_ParticleRadius );

		m_ParticleEffect.SetBBox( vWorldMins, vWorldMaxs );
	}
		
	float minViewOrigingDistance = std::numeric_limits<float>::max();
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		float d = CollisionProp()->CalcDistanceFromPoint( MainViewOrigin(hh) );
		if ( d < minViewOrigingDistance )
			minViewOrigingDistance = d;
	}

	float targetDensity = m_Density;
	if ( m_maxDrawDistance > 0 && minViewOrigingDistance > m_maxDrawDistance )
	{
		targetDensity = 0.0f;
	}

	// lerp m_CurrentDensity towards m_Density at a rate of m_DensityRampSpeed
	if( m_CurrentDensity < targetDensity )
	{
		m_CurrentDensity += m_DensityRampSpeed * fTimeDelta;
		if( m_CurrentDensity > targetDensity )
		{
			m_CurrentDensity = targetDensity;
		}
	}
	else if( m_CurrentDensity > targetDensity )
	{
		m_CurrentDensity -= m_DensityRampSpeed * fTimeDelta;
		if( m_CurrentDensity < targetDensity )
		{
			m_CurrentDensity = targetDensity;
		}
	}

	if( m_CurrentDensity == 0.0f )
	{
		return;
	}
	
	// This is used to randomize the direction it chooses to move a particle in.

	int offsetLookup[3] = {-1,0,1};

	float tradeDurationMax = m_ParticleSpacingDistance / ( m_MovementSpeed + 0.1f );
	float tradeDurationMin = tradeDurationMax * 0.5f;

	if ( IS_NAN( tradeDurationMax ) || IS_NAN( tradeDurationMin ) )
		return;

//	Warning( "tradeDuration: [%f,%f]\n", tradeDurationMin, tradeDurationMax );
	
	// Update all the moving traders and establish new ones.
	int nTotal = m_xCount * m_yCount * m_zCount;
	for( int i=0; i < nTotal; i++ )
	{
		SmokeParticleInfo *pInfo = &m_pSmokeParticleInfos[i];

		if(!pInfo->m_pParticle)
			continue;
	
		if(pInfo->m_TradeIndex == -1)
		{
			pInfo->m_pParticle->m_FadeAlpha = pInfo->m_FadeAlpha;
			pInfo->m_pParticle->m_Color[0] = pInfo->m_Color[0];
			pInfo->m_pParticle->m_Color[1] = pInfo->m_Color[1];
			pInfo->m_pParticle->m_Color[2] = pInfo->m_Color[2];

			// Is there an adjacent one that's not trading?
			int x, y, z;
			GetParticleInfoXYZ(i, x, y, z);

			int xCountOffset = RandomInt( 0, VALVE_RAND_MAX );
			int yCountOffset = RandomInt( 0, VALVE_RAND_MAX );
			int zCountOffset = RandomInt( 0, VALVE_RAND_MAX );

			bool bFound = false;
			for(int xCount=0; xCount < 3 && !bFound; xCount++)
			{
				for(int yCount=0; yCount < 3 && !bFound; yCount++)
				{
					for(int zCount=0; zCount < 3; zCount++)
					{
						int testX = x + offsetLookup[(xCount+xCountOffset) % 3];
						int testY = y + offsetLookup[(yCount+yCountOffset) % 3];
						int testZ = z + offsetLookup[(zCount+zCountOffset) % 3];

						if(testX == x && testY == y && testZ == z)
							continue;

						if(IsValidXYZCoords(testX, testY, testZ))
						{
							SmokeParticleInfo *pOther = GetSmokeParticleInfo(testX, testY, testZ);
							if(pOther->m_pParticle && pOther->m_TradeIndex == -1)
							{
								// Ok, this one is looking to trade also.
								pInfo->m_TradeIndex = GetSmokeParticleIndex(testX, testY, testZ);
								pOther->m_TradeIndex = i;
								pInfo->m_TradeClock = pOther->m_TradeClock = 0;
								pOther->m_TradeDuration = pInfo->m_TradeDuration = FRand( tradeDurationMin, tradeDurationMax );
								
								bFound = true;
								break;
							}
						}
					}
				}
			}
		}
		else
		{
			SmokeParticleInfo *pOther = &m_pSmokeParticleInfos[pInfo->m_TradeIndex];
			contract_assert( pOther->m_TradeIndex == i );
			
			// This makes sure the trade only gets updated once per frame.
			if(pInfo < pOther)
			{
				// Increment the trade clock..
				pInfo->m_TradeClock = (pOther->m_TradeClock += fTimeDelta);
				int x, y, z;
				GetParticleInfoXYZ(i, x, y, z);
				Vector myPos = GetSmokeParticlePos(x, y, z);
				
				int otherX, otherY, otherZ;
				GetParticleInfoXYZ(pInfo->m_TradeIndex, otherX, otherY, otherZ);
				Vector otherPos = GetSmokeParticlePos(otherX, otherY, otherZ);

				// Is the trade finished?
				if(pInfo->m_TradeClock >= pInfo->m_TradeDuration)
				{
					pInfo->m_TradeIndex = pOther->m_TradeIndex = -1;
					
					pInfo->m_pParticle->m_Pos = otherPos;
					pOther->m_pParticle->m_Pos = myPos;

					SmokeGrenadeParticle *temp = pInfo->m_pParticle;
					pInfo->m_pParticle = pOther->m_pParticle;
					pOther->m_pParticle = temp;
				}
				else
				{			
					// Ok, move them closer.
					float percent = (float)std::cos(pInfo->m_TradeClock * 2 * 1.57079632f / pInfo->m_TradeDuration);
					percent = percent * 0.5 + 0.5;
					
					pInfo->m_pParticle->m_FadeAlpha  = pInfo->m_FadeAlpha + (pOther->m_FadeAlpha - pInfo->m_FadeAlpha) * (1 - percent);
					pOther->m_pParticle->m_FadeAlpha = pInfo->m_FadeAlpha + (pOther->m_FadeAlpha - pInfo->m_FadeAlpha) * percent;

					InterpColor(pInfo->m_pParticle->m_Color,  pInfo->m_Color, pOther->m_Color, 1-percent);
					InterpColor(pOther->m_pParticle->m_Color, pInfo->m_Color, pOther->m_Color, percent);

					pInfo->m_pParticle->m_Pos  = myPos + (otherPos - myPos) * (1 - percent);
					pOther->m_pParticle->m_Pos = myPos + (otherPos - myPos) * percent;
				}
			}
		}
	}
}

void CFuncSmokeVolume::RenderParticles( CParticleRenderIterator *pIterator )
{
	if ( m_CurrentDensity == 0 )
		return;

	const SmokeGrenadeParticle *pParticle = (const SmokeGrenadeParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		Vector renderPos = pParticle->m_Pos;

		// Fade out globally.
		float alpha = m_CurrentDensity;

		// Apply the precalculated fade alpha from world geometry.
		alpha *= pParticle->m_FadeAlpha;
		
		// TODO: optimize this whole routine!
		Vector color = m_MinColor + (m_MaxColor - m_MinColor) * (pParticle->m_ColorInterp / 255.1f);
		if ( IsEmissive() )
		{
			color.x += pParticle->m_Color[0] / 255.0f;
			color.y += pParticle->m_Color[1] / 255.0f;
			color.z += pParticle->m_Color[2] / 255.0f;

			color.x = clamp( color.x, 0.0f, 1.0f );
			color.y = clamp( color.y, 0.0f, 1.0f );
			color.z = clamp( color.z, 0.0f, 1.0f );
		}
		else
		{
			color.x *= pParticle->m_Color[0] / 255.0f;
			color.y *= pParticle->m_Color[1] / 255.0f;
			color.z *= pParticle->m_Color[2] / 255.0f;
		}
		
		Vector tRenderPos;
		TransformParticle( ParticleMgr()->GetModelView(), renderPos, tRenderPos );
		float sortKey = 1;//tRenderPos.z;

		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tRenderPos,
			color,
			alpha * GetAlphaDistanceFade(tRenderPos, 10, 30),	// Alpha
			m_ParticleRadius,
			pParticle->m_CurRotation
			);

		pParticle = (const SmokeGrenadeParticle*)pIterator->GetNext( sortKey );
	}
}

void CFuncSmokeVolume::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	if ( m_CurrentDensity == 0 )
		return;

	SmokeGrenadeParticle *pParticle = (SmokeGrenadeParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		pParticle->m_CurRotation += pParticle->m_RotationFactor * ( std::numbers::pi / 180.0f ) * m_RotationSpeed * pIterator->GetTimeDelta();
		pParticle = (SmokeGrenadeParticle*)pIterator->GetNext();
	}
}

void CFuncSmokeVolume::NotifyRemove()
{
	m_xCount = m_yCount = m_zCount = 0;
}

void CFuncSmokeVolume::FillVolume()
{
	Vector vPos;
	for(int x=0; x < m_xCount; x++)
	{
		for(int y=0; y < m_yCount; y++)
		{
			for(int z=0; z < m_zCount; z++)
			{
				vPos = GetSmokeParticlePos( x, y, z );
				if(SmokeParticleInfo *pInfo = GetSmokeParticleInfo(x,y,z))
				{
					int contents = GetWorldPointContents(vPos);
					if(contents & CONTENTS_SOLID)
					{
						pInfo->m_pParticle = nullptr;
					}
					else
					{
						SmokeGrenadeParticle *pParticle = 
							(SmokeGrenadeParticle*)m_ParticleEffect.AddParticle(sizeof(SmokeGrenadeParticle), m_MaterialHandle);

						if(pParticle)
						{
							pParticle->m_Pos = vPos;
							pParticle->m_ColorInterp = (unsigned char)( (rand() * 255) / VALVE_RAND_MAX );
							pParticle->m_RotationFactor = FRand( -1.0f, 1.0f ); // Rotation factor.
							pParticle->m_CurRotation = FRand( -m_RotationSpeed, m_RotationSpeed );
						}

						Vector vColor = EngineGetLightForPoint(vPos);
						pInfo->m_Color[0] = LinearToTexture( vColor.x );
						pInfo->m_Color[1] = LinearToTexture( vColor.y );
						pInfo->m_Color[2] = LinearToTexture( vColor.z );

						// Cast some rays and if it's too close to anything, fade its alpha down.
						pInfo->m_FadeAlpha = 1;

						for(int i=0; i < NUM_FADE_PLANES; i++)
						{
							trace_t trace;
							WorldTraceLine(vPos, vPos + s_FadePlaneDirections[i] * 100, MASK_SOLID_BRUSHONLY, &trace);
							if(trace.fraction < 1.0f)
							{
								float dist = DotProduct(trace.plane.normal, vPos) - trace.plane.dist;
								if(dist < 0)
								{
									pInfo->m_FadeAlpha = 0;
								}
								else if(dist < m_ParticleRadius)
								{
									float alphaScale = dist / m_ParticleRadius;
									alphaScale *= alphaScale * alphaScale;
									pInfo->m_FadeAlpha *= alphaScale;
								}
							}
						}

						pInfo->m_pParticle = pParticle;
						pInfo->m_TradeIndex = -1;
					}
				}
			}
		}
	}
}

#undef NUM_FADE_PLANES

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// An entity which emits other entities at points 
//-----------------------------------------------------------------------------
class CFuncTrackTrain : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncTrackTrain" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncTrackTrain, CBaseEntity );
	DECLARE_CLIENTCLASS();

public:
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual bool GetSoundSpatialization( SpatializationInfo_t& info );

	virtual bool IsBaseTrain( void ) const { return true; }

private:
	int m_nLongAxis;
	float m_flRadius;
	float m_flLineLength;
};

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CFuncTrackTrain, DT_FuncTrackTrain, CFuncTrackTrain )

//-----------------------------------------------------------------------------
// Sound spatialization
//-----------------------------------------------------------------------------
void CFuncTrackTrain::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if (updateType == DATA_UPDATE_CREATED)
	{
		// Compute the cross-sectional area and dimension and length of the line segment
		int nIndex1, nIndex2;
		const Vector &vecOBBSize = CollisionProp()->OBBSize();
		if ( ( vecOBBSize.x > vecOBBSize.y ) && ( vecOBBSize.x > vecOBBSize.z ) )
		{
			m_nLongAxis = 0;
			nIndex1 = 1; nIndex2 = 2;
		}
		else if ( vecOBBSize.y > vecOBBSize.z )
		{
			m_nLongAxis = 1;
			nIndex1 = 0; nIndex2 = 2;
		}
		else
		{
			m_nLongAxis = 2;
			nIndex1 = 0; nIndex2 = 1;
		}

		m_flRadius = std::sqrt( vecOBBSize[nIndex1] * vecOBBSize[nIndex1] + vecOBBSize[nIndex2] * vecOBBSize[nIndex2] ) * 0.5f;
		m_flLineLength = vecOBBSize[m_nLongAxis];
	}
}

//-----------------------------------------------------------------------------
// Sound spatialization
//-----------------------------------------------------------------------------
bool CFuncTrackTrain::GetSoundSpatialization( SpatializationInfo_t& info )
{
	// Out of PVS
	if ( IsDormant() )
		return false;
	
	if ( info.pflRadius )
	{
		*info.pflRadius = m_flRadius;
	}
	
	if ( info.pOrigin )
	{
		Vector vecStart, vecEnd, vecWorldDir;
		Vector vecDir = vec3_origin;
		vecDir[m_nLongAxis] = 1.0f;
		VectorRotate( vecDir, EntityToWorldTransform(), vecWorldDir );
		VectorMA( WorldSpaceCenter(), -0.5f * m_flLineLength, vecWorldDir, vecStart );
		VectorMA( vecStart, m_flLineLength, vecWorldDir, vecEnd );

		float t;
		CalcClosestPointOnLine( info.info.vListenerOrigin, vecStart, vecEnd, *info.pOrigin, &t ); 
		if ( t < 0.0f )
		{
			*info.pOrigin = vecStart;
		}
		else if ( t > 1.0f )
		{
			*info.pOrigin = vecEnd;
		}
	}

	if ( info.pAngles )
	{
		VectorCopy( CollisionProp()->GetCollisionAngles(), *info.pAngles );
	}

	return true;
}
