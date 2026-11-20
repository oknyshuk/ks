#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//=============================================================================

import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
#include "bspfile.h"
import std;
#include "dispnode.h"
#include "builddisp.h"
#include "engine/ishadowmgr.h"
#include "getintersectingsurfaces_struct.h"
#include "surfacehandle.h"

struct model_t;
struct Ray_t;
struct RayDispOutput_t;
struct decal_t;
class CMeshBuilder;


//-----------------------------------------------------------------------------
// Handle to decals + shadows on displacements
//-----------------------------------------------------------------------------
typedef unsigned short DispDecalHandle_t;

enum
{
	DISP_DECAL_HANDLE_INVALID = (DispDecalHandle_t)~0
};

typedef unsigned short DispShadowHandle_t;

enum
{
	DISP_SHADOW_HANDLE_INVALID = (DispShadowHandle_t)~0
};


//-----------------------------------------------------------------------------
// Displacement interface to the engine (and WorldCraft?)
//-----------------------------------------------------------------------------
class IDispInfo
{
public:


	// Builds a list of displacement triangles intersecting the sphere.
	void		GetIntersectingSurfaces( GetIntersectingSurfaces_Struct *pStruct );

	void		RenderWireframeInLightmapPage( int pageId );
	
	void		GetBoundingBox( Vector &bbMin, Vector &bbMax );

	// Get and set the parent surfaces.
	void		SetParent( SurfaceHandle_t surfID );
	SurfaceHandle_t	GetParent();

	// Add dynamic lights to the lightmap for this surface.
	void AddDynamicLights( struct dlight_t *pLights, unsigned int lightMask );
	// Compute the mask for the lights hitting this surface.
	unsigned int ComputeDynamicLightMask( struct dlight_t *pLights );

	// Add and remove decals.
	// flSize is like the radius of the decal so the decal isn't put on any disp faces it's too far away from.
	DispDecalHandle_t	NotifyAddDecal( decal_t *pDecal, float flSize );
	void				NotifyRemoveDecal( DispDecalHandle_t h );
	
	DispShadowHandle_t	AddShadowDecal( ShadowHandle_t shadowHandle );
	void				RemoveShadowDecal( DispShadowHandle_t handle );

	// Compute shadow fragments for a particular shadow, return the vertex + index count of all fragments
	bool		ComputeShadowFragments( DispShadowHandle_t h, int& vertexCount, int& indexCount );

	// Tag the surface and check if it's tagged. You can untag all the surfaces
	// with DispInfo_ClearAllTags. Note: it just uses a frame counter to track the
	// tag state so it's really really fast to call ClearAllTags (just increments
	// a variable 99.999% of the time).
	bool		GetTag();
	void		SetTag();

	// Cast a ray against this surface
	bool	TestRay( Ray_t const& ray, float start, float end, float& dist, Vector2D* lightmapUV, Vector2D* textureUV );

	// Computes the texture + lightmap coordinate given a displacement uv
	void	ComputeLightmapAndTextureCoordinate( RayDispOutput_t const& uv, Vector2D* luv, Vector2D* tuv );
};


// ----------------------------------------------------------------------------- //
// Adds shadow rendering data to a particular mesh builder
// The function will return the new base index
// ----------------------------------------------------------------------------- //
int				DispInfo_AddShadowsToMeshBuilder( CMeshBuilder& meshBuilder, 
										DispShadowHandle_t h, int baseIndex );


typedef void* HDISPINFOARRAY;


// Use these to manage a list of IDispInfos.
HDISPINFOARRAY	DispInfo_CreateArray( int nElements );
void			DispInfo_DeleteArray( HDISPINFOARRAY hArray );
IDispInfo*		DispInfo_IndexArray( HDISPINFOARRAY hArray, int iElement );
int				DispInfo_ComputeIndex( HDISPINFOARRAY hArray, IDispInfo* pInfo );

// Clear the tags for all displacements in the array.
void			DispInfo_ClearAllTags( HDISPINFOARRAY hArray );


// Call this to render a list of displacements.
// If bOrtho is true, then no backface removal is done on dispinfos.
void			DispInfo_RenderListWorld( class IMatRenderContext *pRenderContext, int nSortGroup, SurfaceHandle_t *pList, int listCount, bool bOrtho, unsigned long flags, int DepthMode );
void			DispInfo_RenderListDecalsAndOverlays( class IMatRenderContext *pRenderContext, int nSortGroup, SurfaceHandle_t *pList, int listCount, bool bOrtho, unsigned long flags );
void			DispInfo_RenderListDebug( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount );

// This should be called from Map_LoadDisplacements (while the map file is open).
// It loads the displacement data from the file and prepares the displacements for rendering.
//
// bRestoring is set to true when just restoring the data from the mapfile
// (ie: displacements already are initialized but need new static buffers).
bool			DispInfo_LoadDisplacements( model_t *pWorld, bool bRestoring );

// Deletes all the static vertex buffers.
void			DispInfo_ReleaseMaterialSystemObjects( model_t *pWorld );
