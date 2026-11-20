#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;

// render.h -- public interface to refresh functions

extern float r_blend; // Global blending factor for the current entity
extern float r_colormod[3]; // Global color modulation for the current entity
extern bool g_bIsBlendingOrModulating;

#ifndef DEDICATED
#include "cl_splitscreen.h" // ISplitScreen
#endif

//-----------------------------------------------------------------------------
// Current view
//-----------------------------------------------------------------------------
inline const Vector &CurrentViewOrigin()
{
	extern Vector g_CurrentViewOrigin;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	contract_assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewOrigin;
}

inline const Vector &CurrentViewForward()
{
	extern Vector g_CurrentViewForward;
	extern Vector g_CurrentViewOrigin;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	contract_assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewForward;
}

inline const Vector &CurrentViewRight()
{
	extern Vector g_CurrentViewRight;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	contract_assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewRight;
}

inline const Vector &CurrentViewUp()
{
	extern Vector g_CurrentViewUp;
#ifndef DEDICATED
	extern bool g_bCanAccessCurrentView;
	contract_assert( g_bCanAccessCurrentView );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif
	return g_CurrentViewUp;
}


extern Vector g_MainViewOrigin[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewForward[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewRight[ MAX_SPLITSCREEN_CLIENTS ];
extern Vector g_MainViewUp[ MAX_SPLITSCREEN_CLIENTS ];

//-----------------------------------------------------------------------------
// Main view
//-----------------------------------------------------------------------------
inline const Vector &MainViewOrigin( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewOrigin[ nSlot ];
}

inline const Vector &MainViewForward( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewForward[ nSlot ];
}

inline const Vector &MainViewRight( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewRight[ nSlot ];
}

inline const Vector &MainViewUp( int nSlot = -1 )
{
#ifdef DEDICATED
	nSlot = 0;
#else
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
#endif
	return g_MainViewUp[ nSlot ];
}


void R_Init (void);
void R_LevelInit(void);
void R_LevelShutdown(void);

// Loads world geometry. Called when map changes or dx level changes
void R_LoadWorldGeometry( bool bDXChange = false );

#include "view_shared.h"
#include "ivrenderview.h"


class IRender
{
public:
	void	FrameBegin( void );
	void	FrameEnd( void );
	
	void	ViewSetupVis( bool novis, int numorigins, const Vector origin[] );

	void	ViewDrawFade( byte *color, IMaterial* pFadeMaterial, bool mapFullTextureToScreen = true );

	void	DrawSceneBegin( void );
	void	DrawSceneEnd( void );

	IWorldRenderList * CreateWorldList();
	void	BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth );
	void	BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth, float *pReflectionWaterHeight );
	void	DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust );

	// UNDONE: these are temporary functions that will end up on the other
	// side of this interface
	// accessors
//	virtual const Vector& UnreflectedViewOrigin() = 0;
	const Vector& ViewOrigin( void );
	const QAngle& ViewAngles( void );
	CViewSetup const &ViewGetCurrent( void );
	const VMatrix& ViewMatrix( void );
	const VMatrix& WorldToScreenMatrix( void );
	float	GetFramerate( void );
	float	GetZNear( void );
	float	GetZFar( void );

	// Query current fov and view model fov
	float	GetFov( void );
	float	GetFovY( void );
	float	GetFovViewmodel( void );

	// Compute the clip-space coordinates of a point in 3D
	// Clip-space is normalized screen coordinates (-1 to 1 in x and y)
	// Returns true if the point is behind the camera
	bool	ClipTransform( Vector const& point, Vector* pClip );

	// Compute the screen-space coordinates of a point in 3D
	// This returns actual pixels
	// Returns true if the point is behind the camera
	bool ScreenTransform( Vector const& point, Vector* pScreen );

	void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	void Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture );
	void Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	void PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes );
	void SetMainView( const Vector &vecOrigin, const QAngle &angles );
	void ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags );
	void OverrideViewFrustum( Frustum custom );
	void UpdateBrushModelLightmap( model_t *model, IClientRenderable *Renderable );
	void BeginUpdateLightmaps( void );
	void EndUpdateLightmaps();
	bool InLightmapUpdate( void ) const;
};

void R_PushDlights (void);

// UNDONE Remove this - pass this around to functions/systems that need it.
extern IRender *g_EngineRenderer;
