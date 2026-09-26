//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef R_DECAL_H
#define R_DECAL_H

#include "decal_private.h"
#include "decal_clip.h"
#include <set>
#include <vector>

#include "tier0/platform.h"
#include <memory>

// Initialize and shutdown the decal stuff.
// R_DecalTerm unlinks all the active decals (which frees their counterparts in displacements).
void R_DecalInit();
void R_DecalTerm( worldbrushdata_t *pBrushData, bool term_permanent_decals, bool term_player_decals );
void R_DecalTermNew( worldbrushdata_t *pBrushData, int nTick );
void R_DecalTermAll();
float ComputeDecalLightmapOffset( SurfaceHandle_t surfID );

// --------------------------------------------------------------- //
// Decal functions used for displacements.
// --------------------------------------------------------------- //

// Figure out where the decal maps onto the surface.
void R_SetupDecalClip( 
	CDecalVert* &pOutVerts,
	decal_t *pDecal,
	Vector &vSurfNormal,
	IMaterial *pMaterial,
	Vector textureSpaceBasis[3],
	float decalWorldScale[2] );

//-----------------------------------------------------------------------------
// Gets the decal material and radius based on the decal index
//-----------------------------------------------------------------------------
void R_DecalGetMaterialAndSize( int decalIndex, IMaterial*& pDecalMaterial, float& w, float& h );

//=============================================================================
//
// Decal Sort Structures.
//
enum 
{
	PERMANENT_LIGHTMAP = 0,
	LIGHTMAP,
	NONLIGHTMAP,

	DECALSORT_TYPE_COUNT,
};

struct DecalSortVertexFormat_t
{
	VertexFormat_t		m_VertexFormat;
	int					m_iSortTree;			// Sort tree index.
};

struct DecalMaterialSortData_t
{
	IMaterial	*m_pMaterial;
	int			m_iLightmapPage; 
	int			m_iBucket;			// Index into the s_aDecalMaterialHead list.
};

// Per-frame decal chains, one per material bucket; the whole pool is cleared each frame.
struct DecalSortNode_t
{
	decal_t		*m_pDecal;
	intp		m_iNext;
};
constexpr intp DECAL_SORT_INVALID = -1;

inline intp DecalSortPool_Add( std::vector< DecalSortNode_t > &pool, decal_t *pDecal, intp iNext = DECAL_SORT_INVALID )
{
	pool.push_back( { pDecal, iNext } );
	return intp( pool.size() ) - 1;
}

struct DecalMaterialBucket_t
{
	intp		m_iHead;
	intp		m_iTail;
	int			m_nCheckCount;
};

struct DecalSortTreeSortLess
{
	bool operator()( const DecalMaterialSortData_t &decal1, const DecalMaterialSortData_t &decal2 ) const
	{
		if ( ( decal1.m_iLightmapPage == -1 ) || ( decal2.m_iLightmapPage == -1 ) )
		{
			return ( ( intp )decal1.m_pMaterial < ( intp )decal2.m_pMaterial );
		}

		if ( ( intp )decal1.m_pMaterial == ( intp )decal2.m_pMaterial )
		{
			return ( decal1.m_iLightmapPage < decal2.m_iLightmapPage );
		}
		else
		{
			return ( ( intp )decal1.m_pMaterial < ( intp )decal2.m_pMaterial );
		}
	}
};

struct DecalSortTrees_t
{
	std::set< DecalMaterialSortData_t, DecalSortTreeSortLess >	m_Trees[DECALSORT_TYPE_COUNT];
	std::vector< DecalMaterialBucket_t >			m_aDecalSortBuckets[MAX_MAT_SORT_GROUPS+1][DECALSORT_TYPE_COUNT];
};

extern std::vector< DecalSortVertexFormat_t >	g_aDecalFormats;

// Surface decals.
extern std::vector< DecalSortNode_t >		g_aDecalSortPool;
extern std::vector< DecalSortTrees_t >			g_aDecalSortTrees;
extern int									g_nDecalSortCheckCount;
extern int									g_nBrushModelDecalSortCheckCount;

// Displacement decals.
extern std::vector< DecalSortNode_t >		g_aDispDecalSortPool;
extern std::vector< DecalSortTrees_t >			g_aDispDecalSortTrees;
extern int									g_nDispDecalSortCheckCount;

struct DecalBatchList_t
{
	IMaterial		*m_pMaterial;
	void			*m_pProxy;
	int				m_iLightmapPage;
	unsigned short	m_iStartIndex;
	unsigned short	m_nIndexCount;
};

struct DecalMeshList_t
{
	IMesh									*m_pMesh;
	std::inplace_vector< DecalBatchList_t, 128 >	m_aBatches;
};

void DecalSurfacesInit( bool bBrushModel );
void DecalSurfaceAdd( SurfaceHandle_t surfID, int renderGroup );
void DecalSurfaceDraw( IMatRenderContext *pRenderContext, int renderGroup, float flFade = 1.0f );
void DrawDecalsOnSingleSurface( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID );

void R_DecalReSortMaterials( void );
void R_DecalFlushDestroyList( bool bImmediateCleanup = false );

extern VMatrix g_BrushToWorldMatrix;

#endif // R_DECAL_H
