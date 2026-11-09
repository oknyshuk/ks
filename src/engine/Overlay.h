#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Model loading / unloading interface

// This is a workaround for the fact that we get massive decal flicker
// when looking at a decal at a glancing angle while standing right next to it.
class IMatRenderContext;

inline constexpr auto OVERLAY_AVOID_FLICKER_NORMAL_OFFSET = 0.1f;


//-----------------------------------------------------------------------------
// Overlay fragments
//-----------------------------------------------------------------------------
typedef unsigned short OverlayFragmentHandle_t;

enum
{
	OVERLAY_FRAGMENT_INVALID = (OverlayFragmentHandle_t)~0
};


//=============================================================================
//
// Overlay Manager Interface
//
class IOverlayMgr
{
public:
	// Memory allocation/de-allocation.
	bool	LoadOverlays( );
	void	UnloadOverlays( );

	void	CreateFragments( void );

	void	ReSortMaterials( void );

	// Drawing
	// clears all
	void	ClearRenderLists();
	// clears a particular sort group
	void	ClearRenderLists( int nSortGroup );
	void	AddFragmentListToRenderList( int nSortGroup, OverlayFragmentHandle_t iFragment, bool bDisp );
	void	RenderOverlays( IMatRenderContext *pRenderContext, int nSortGroup );

	// Sets the client renderable for an overlay's material proxy to bind to
	void	SetOverlayBindProxy( int iOverlayID, void *pBindProxy );

	void	UpdateOverlayRenderLevels( int nCPULevel, int nGPULevel );
	void	RenderAllUnlitOverlays( IMatRenderContext *pRenderContext, int nSortGroup );
};


//-----------------------------------------------------------------------------
// Overlay manager singleton
//-----------------------------------------------------------------------------
IOverlayMgr *OverlayMgr();
