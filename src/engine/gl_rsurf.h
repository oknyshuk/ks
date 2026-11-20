#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
#include "bsptreedata.h"
#include "materialsystem/imesh.h"
#include "surfacehandle.h"
#include "ivrenderview.h"
#include "gl_model_private.h"

struct WorldListInfo_t;
class IMaterial;
class IClientRenderable;
class IBrushRenderer;
class IClientEntity;
struct model_t;
struct VisibleFogVolumeInfo_t;

struct LightmapUpdateInfo_t
{
	SurfaceHandle_t m_SurfHandle;
	int m_nDlightMask;
	short m_nTransformIndex;
	bool m_bNeedsLightmap;
	bool m_bNeedsBumpmap;
};

struct LightmapTransformInfo_t
{
	model_t *pModel;
	matrix3x4_t xform;
};
extern std::vector< LightmapUpdateInfo_t > g_LightmapUpdateList;
extern std::vector< LightmapTransformInfo_t > g_LightmapTransformList;

//-----------------------------------------------------------------------------
// Helper class to iterate over leaves
//-----------------------------------------------------------------------------
class IEngineSpatialQuery : public ISpatialQuery
{
public:
};

extern IEngineSpatialQuery* g_pToolBSPTree;

class IWorldRenderList;
IWorldRenderList *AllocWorldRenderList();

void R_Surface_LevelInit();
void R_Surface_LevelShutdown();
void R_SceneBegin( void );
void R_SceneEnd( void );
void R_BuildWorldLists( IWorldRenderList *pRenderList, WorldListInfo_t* pInfo, int iForceViewLeaf, const struct VisOverrideData_t* pVisData, bool bShadowDepth = false, float *pWaterReflectionHeight = nullptr );
void R_DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pRenderList, unsigned long flags, float waterZAdjust );

void R_BuildWorldLists_Epilogue( IWorldRenderList *pRenderListIn, WorldListInfo_t* pInfo, bool bShadowDepth );

void R_GetWorldListIndicesInfo( WorldListIndicesInfo_t * pInfoOut, IWorldRenderList *pRenderList, unsigned long nFlags );

int R_GetNumIndicesForWorldList( IWorldRenderList *pRenderList, unsigned long nFlags );

void R_GetVisibleFogVolume( const Vector& vEyePoint, const VisOverrideData_t *pVisOverrideData, VisibleFogVolumeInfo_t *pInfo );
void R_SetFogVolumeState( int fogVolume, bool useHeightFog );
IMaterial *R_GetFogVolumeMaterial( int fogVolume, bool bEyeInFogVolume );
void R_SetupSkyTexture( model_t *pWorld );

void Shader_DrawLightmapPageChains( IWorldRenderList *pRenderList, int pageId );
void Shader_DrawLightmapPageSurface( SurfaceHandle_t surfID, float red, float green, float blue );
void Shader_DrawTranslucentSurfaces( IMatRenderContext *pRenderContext, IWorldRenderList *pRenderList, int *pSortList, int sortCount, unsigned long flags );
bool Shader_LeafContainsTranslucentSurfaces( IWorldRenderList *pRenderList, int sortIndex, unsigned long flags );
void R_DrawTopView( bool enable );
void R_TopViewNoBackfaceCulling( bool bDisable );
void R_TopViewNoVisCheck( bool bDisable );
void R_TopViewBounds( const Vector2D & mins, const Vector2D & maxs );
void R_SetTopViewVolumeCuller( const CVolumeCuller *pTopViewVolumeCuller );

// Resets a world render list
void ResetWorldRenderList( IWorldRenderList *pRenderList );

// Computes the centroid of a surface
void Surf_ComputeCentroid( SurfaceHandle_t surfID, Vector *pVecCentroid );

// Installs a client-side renderer for brush models
void R_InstallBrushRenderOverride( IBrushRenderer* pBrushRenderer );

// update dlight status on a brush model
extern int R_MarkDlightsOnBrushModel( model_t *model, IClientRenderable *pRenderable );

void R_DrawBrushModel( 
	IClientEntity *baseentity, 
	model_t *model, 
	const Vector& origin, 
	const QAngle& angles, 
	ERenderDepthMode_t DepthMode, bool bDrawOpaque, bool bDrawTranslucent );

void R_DrawBrushModel( 
	IClientEntity *baseentity, 
	model_t *model, 
	const matrix3x4a_t& brushModelToWorld, 
	bool bShadowDepth, bool bDrawOpaque, bool bDrawTranslucent );

// Draws arrays of brush models
void R_DrawBrushModelArray( IMatRenderContext* pRenderContext, int nCount, 
	const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags );

// Draws arrays of brush models( this version is used by queued render contexts)
inline void R_DrawBrushModelArray( int nCount, 
	const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	R_DrawBrushModelArray( pRenderContext, nCount, pInstanceData, nModelTypeFlags );
}


void R_DrawBrushModelShadow( IClientRenderable* pRender );
void R_BrushBatchInit( void );

int R_GetBrushModelPlaneCount( const model_t *model );
const cplane_t &R_GetBrushModelPlane( const model_t *model, int nIndex, Vector *pOrigin );

bool TangentSpaceSurfaceSetup( SurfaceHandle_t surfID, Vector &tVect );
void TangentSpaceComputeBasis( Vector& tangentS, Vector& tangentT, const Vector& normal, const Vector& tVect, bool negateTangent );

inline int BuildIndicesForSurface( CIndexBuilder &meshBuilder, SurfaceHandle_t surfID )
{
	int nSurfTriangleCount = MSurf_VertCount( surfID ) - 2;
	int startVert = MSurf_VertBufferIndex( surfID );
	contract_assert( startVert!=0xFFFF );

	// NOTE: This switch appears to help performance
	// add surface to this batch
	switch (nSurfTriangleCount)
	{
	case 1:
		meshBuilder.FastTriangle(startVert);
		break;

	case 2:
		meshBuilder.FastQuad(startVert);
		break;

	default:
		meshBuilder.FastPolygon(startVert, nSurfTriangleCount);
		break;
	}
	return nSurfTriangleCount;
}

inline void BuildIndicesForWorldSurface( CIndexBuilder &meshBuilder, SurfaceHandle_t surfID, worldbrushdata_t *pData )
{
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		contract_assert( pPrim->vertCount==0 );
		unsigned short startVert = MSurf_VertBufferIndex( surfID );
		contract_assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3) );

		CIndexBuilder &indexBuilder = meshBuilder;
		indexBuilder.FastIndexList( &pData->primindices[pPrim->firstIndex], startVert, pPrim->indexCount );
	}
	else
	{
		BuildIndicesForSurface( meshBuilder, surfID );
		
	}
}

// each surface has an index to the first vertex in the depth fill VB
extern std::vector< uint16 > g_DepthFillVBFirstVertexForSurface;

inline int BuildDepthFillIndicesForSurface( CIndexBuilder &meshBuilder, SurfaceHandle_t surfID )
{
	int nSurfTriangleCount = MSurf_VertCount( surfID ) - 2;
	int startVert = g_DepthFillVBFirstVertexForSurface[ MSurf_Index(surfID )];
	contract_assert( startVert!=0xFFFF );

	// NOTE: This switch appears to help performance
	// add surface to this batch
	switch (nSurfTriangleCount)
	{
	case 1:
		meshBuilder.FastTriangle(startVert);
		break;

	case 2:
		meshBuilder.FastQuad(startVert);
		break;

	default:
		meshBuilder.FastPolygon(startVert, nSurfTriangleCount);
		break;
	}
	return nSurfTriangleCount;
}

inline void BuildDepthFillIndicesForWorldSurface( CIndexBuilder &meshBuilder, SurfaceHandle_t surfID, worldbrushdata_t *pData )
{
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		contract_assert( pPrim->vertCount==0 );
		unsigned short startVert = g_DepthFillVBFirstVertexForSurface[ MSurf_Index(surfID )];
		contract_assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3) );

		CIndexBuilder &indexBuilder = meshBuilder;
		indexBuilder.FastIndexList( &pData->primindices[pPrim->firstIndex], startVert, pPrim->indexCount );
	}
	else
	{
		BuildDepthFillIndicesForSurface( meshBuilder, surfID );
	}
}


inline int GetIndexCountForWorldSurface( SurfaceHandle_t surfID )
{
	return ( MSurf_VertCount( surfID ) - 2 ) * 3;
}


struct CShaderDebug
{
	bool		wireframe;
	bool		normals;
	bool		luxels;
	bool		bumpBasis;
	bool		surfacematerials;
	bool		anydebug;
	int			surfaceid;

	void TestAnyDebug()
	{
		anydebug = wireframe || normals || luxels || bumpBasis || ( surfaceid != 0 ) || surfacematerials;
	}
};

extern CShaderDebug g_ShaderDebug;

inline constexpr auto BRUSHMODEL_DECAL_SORT_GROUP = MAX_MAT_SORT_GROUPS;
const int MAX_VERTEX_FORMAT_CHANGES = 128;
inline constexpr auto BACKFACE_EPSILON = -0.01f;
void Shader_GetSurfVertexAndIndexCount( SurfaceHandle_t surfaceHandle, int *pVertexCount, int *pIndexCount );

// Draw debugging information
void DrawDebugInformation( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount );
void DrawDebugInformation( IMatRenderContext *pRenderContext, const matrix3x4a_t &brushToWorld, SurfaceHandle_t *pList, int listCount );


class CBrushModelTransform
{
public:
	CBrushModelTransform( const Vector &origin, const QAngle &angles, IMatRenderContext *pRenderContext );
	CBrushModelTransform( const matrix3x4a_t &matrix, IMatRenderContext *pRenderContext );
	~CBrushModelTransform();
	VMatrix *GetNonIdentityMatrix();
	inline bool IsIdentity() { return m_bIdentity; }
	Vector	m_savedModelorg;
	bool	m_bIdentity;
};
