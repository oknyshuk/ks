#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math;
#include "engine/ishadowmgr.h"
import std;
#include "surfacehandle.h"
class IMatRenderContext;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDispInfo;


//-----------------------------------------------------------------------------
// Shadow decals are applied to a single surface
//-----------------------------------------------------------------------------
typedef unsigned short ShadowDecalHandle_t;

enum
{
	SHADOW_DECAL_HANDLE_INVALID = (ShadowDecalHandle_t)~0
};


//-----------------------------------------------------------------------------
// This structure contains the vertex information for shadows
//-----------------------------------------------------------------------------
struct ShadowVertex_t
{
	Vector	m_Position;
	Vector	m_ShadowSpaceTexCoord;
};

enum
{
	SHADOW_VERTEX_TEMP_COUNT = 48
};

//-----------------------------------------------------------------------------
// Used to clip the shadow decals
//-----------------------------------------------------------------------------
struct ShadowClipState_t
{
	int m_CurrVert;
	int	m_TempCount;
	int	m_ClipCount;
	ShadowVertex_t	m_pTempVertices[SHADOW_VERTEX_TEMP_COUNT];
	ShadowVertex_t*	__restrict__ m_ppClipVertices[2][SHADOW_VERTEX_TEMP_COUNT];
};


//-----------------------------------------------------------------------------
// This structure contains info to accelerate shadow darkness computation
//-----------------------------------------------------------------------------
struct ShadowDecalRenderInfo_t
{
	Vector2D m_vTexOrigin;
	Vector2D m_vTexSize;
	float m_flFalloffOffset;
	float m_flOOZFalloffDist;
	float m_flFalloffAmount;
	float m_flFalloffBias;
	Vector m_vShadowFalloffParams;	// params munged for use in the shader
};


//-----------------------------------------------------------------------------
// Shadow-related functionality internal to the engine
//-----------------------------------------------------------------------------
class IShadowMgrInternal : public IShadowMgr
{
public:
	void LevelInit( int nSurfCount );
	void LevelShutdown();

	// This surface has been rendered; and we'll want to render the shadows
	// on this surface
	void AddShadowsOnSurfaceToRenderList( ShadowDecalHandle_t decalHandle );

	// This will render all shadows that were previously added using
	// AddShadowsOnSurfaceToRenderList. If there's a model to world transform
	// for the shadow receiver, then send it in!
	// NOTE: This draws both shadows and projected textures.
	void RenderProjectedTextures( IMatRenderContext *pRenderContext, VMatrix const* pModelToWorld = 0 );

	// NOTE: This draws shadows
	void RenderShadows( IMatRenderContext *pRenderContext, VMatrix const* pModelToWorld = 0 );

	// NOTE: This draws flashlights
	void RenderFlashlights( bool bDoMasking, bool bDoSimpleProjections, VMatrix const* pModelToWorld = 0 );

	// Clears the list of shadows to render 
	void ClearShadowRenderList();

	// Projects + clips shadows
	// count + ppPosition describe an array of pointers to vertex positions
	// of the unclipped shadow
	// ppOutVertex is pointed to the head of an array of pointers to
	// clipped vertices the function returns the number of clipped vertices
	int ProjectAndClipVertices( ShadowHandle_t handle, int count, 
		Vector** ppPosition, ShadowVertex_t*** ppOutVertex );

	// Computes information for rendering
	void ComputeRenderInfo( ShadowDecalRenderInfo_t* pInfo, ShadowHandle_t handle ) const;

	// Shadow state...
	unsigned short InvalidShadowIndex( );
	void SetModelShadowState( ModelInstanceHandle_t instance );

	void SetNumWorldMaterialBuckets( int numMaterialSortBins );

	void DrawFlashlightDecals( IMatRenderContext *pRenderContext, int sortGroup, bool bDoMasking, float flFade = 1.0f );

	void DrawFlashlightDecalsOnSurfaceList( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, bool bDoMasking );

	void DrawFlashlightOverlays( IMatRenderContext *pRenderContext, int nSortGroup, bool bDoMasking );

	void DrawFlashlightDepthTexture( );

	void DrawFlashlightDecalsOnDisplacements( IMatRenderContext *pRenderContext, int sortGroup, CDispInfo **visibleDisps, int nVisibleDisps, bool bDoMasking );

	void SetFlashlightStencilMasks( bool bDoMasking );
	bool ModelHasShadows( ModelInstanceHandle_t instance );

	int ProjectAndClipVerticesEx( ShadowHandle_t handle, int count, 
		Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip );
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
#ifndef DEDICATED
extern IShadowMgrInternal* g_pShadowMgr;
#endif
