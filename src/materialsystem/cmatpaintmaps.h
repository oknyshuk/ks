#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;
import ks.tier1;
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ipaintmapdatamanager.h"

#include "pixelwriter.h"
#include "shaderapi/ishaderdynamic.h"

class CMaterialSystem;

//-----------------------------------------------------------------------------
// Manager of material system Paintmaps
//-----------------------------------------------------------------------------

class CMatPaintmaps
{
public:
	CMatPaintmaps();

	bool IsEnabled( void );
	void RegisterPaintmapDataManager( IPaintmapDataManager *pDataManager );

	void BeginPaintTextureAllocation( int iPaintmapCount );
	void EndPaintTextureAllocation( void );

	void AllocatePaintmap( int paintmap, int iWidth, int iHeight );

	void ReleasePaintmaps( void );
	void RestorePaintmaps( int nNumLightmaps );

	void CleanupPaintmaps( void );

	ShaderAPITextureHandle_t GetPaintmapPageTextureHandle( int paintmap );

	CMaterialSystem *GetMaterialSystem() const;


	// Derived from IPaintmapTextureManager
	//------------------------------------------------
	void BeginUpdatePaintmaps();
	void UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects );
	void EndUpdatePaintmaps();

	//------------------------------------------------
	IPaintmapDataManager *m_pDataManager;

private:
	void InitPaintmapBits( int paintmap );
	bool LockPaintmap( int paintmap );
	void FillRect( int paintmap, Rect_t* __restrict__ pRect, BYTE* __restrict__ pPaintData, Rect_t* __restrict__ pSubRect = nullptr ) __restrict__;
	void AllocatePaintmapTexture( int paintmap, int iWidth, int iHeight );

	CPixelWriter m_PaintmapPixelWriter;
	std::vector< ShaderAPITextureHandle_t > m_PaintmapTextureHandles;

	int m_nLockedPaintmap;
	int m_nUpdatingPaintmapsStackDepth;
};

//-----------------------------------------------------------------------------
