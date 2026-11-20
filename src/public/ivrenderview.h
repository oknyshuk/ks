#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.math.vplane;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
#include "materialsystem/imaterialsystem.h"
#include "const.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CViewSetup;
class IClientEntity;
class IMaterial;
struct model_t;
class IClientRenderable;
class IMatRenderContext;


//-----------------------------------------------------------------------------
// Flags used by DrawWorldLists
//-----------------------------------------------------------------------------
enum
{
	DRAWWORLDLISTS_DRAW_STRICTLYABOVEWATER		= 0x001,
	DRAWWORLDLISTS_DRAW_STRICTLYUNDERWATER		= 0x002,
	DRAWWORLDLISTS_DRAW_INTERSECTSWATER			= 0x004,
	DRAWWORLDLISTS_DRAW_WATERSURFACE			= 0x008,
	DRAWWORLDLISTS_DRAW_SKYBOX					= 0x010,
	DRAWWORLDLISTS_DRAW_CLIPSKYBOX				= 0x020,
	DRAWWORLDLISTS_DRAW_SHADOWDEPTH				= 0x040,
	DRAWWORLDLISTS_DRAW_REFRACTION				= 0x080,
	DRAWWORLDLISTS_DRAW_REFLECTION				= 0x100,
	DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY			= 0x200,
	DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS		= 0x400,
	DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL		= 0x800,
	DRAWWORLDLISTS_DRAW_SIMPLE_WORLD_MODEL_WATER= 0x1000,
	DRAWWORLDLISTS_DRAW_SKIP_DISPLACEMENTS		= 0x2000,
	DRAWWORLDLISTS_DRAW_SSAO					= 0x4000,
};

enum
{
	MAT_SORT_GROUP_STRICTLY_ABOVEWATER = 0,
	MAT_SORT_GROUP_STRICTLY_UNDERWATER,
	MAT_SORT_GROUP_INTERSECTS_WATER_SURFACE,
	MAT_SORT_GROUP_WATERSURFACE,

	MAX_MAT_SORT_GROUPS
};

enum ERenderDepthMode_t
{
	DEPTH_MODE_NORMAL = 0,
	DEPTH_MODE_SHADOW = 1,
	DEPTH_MODE_SSA0 = 2,

	DEPTH_MODE_MAX
};


static const char* s_pMatSortGroupsString[] =
{
	"Sort: Abovewater",
	"Sort: Underwater",
	"Sort: IntersectsWater",
	"Sort: WaterSurface",
};

typedef VPlane Frustum[FRUSTUM_NUMPLANES];


//-----------------------------------------------------------------------------
// Leaf index
//-----------------------------------------------------------------------------
typedef unsigned short LeafIndex_t;
enum
{
	INVALID_LEAF_INDEX = (LeafIndex_t)~0
};


//-----------------------------------------------------------------------------
// Describes the leaves to be rendered this view, set by BuildWorldLists

//-----------------------------------------------------------------------------
// NOTE: This is slightly slower on x360 but saves memory
struct WorldListLeafData_t
{
	LeafIndex_t	leafIndex;	// 16 bits
	int16	waterData;
	uint16 	firstTranslucentSurface;	// engine-internal list index
	uint16	translucentSurfaceCount;	// count of translucent surfaces+disps
};
struct WorldListInfo_t
{
	int		m_ViewFogVolume;
	int		m_LeafCount;
	bool	m_bHasWater;
	WorldListLeafData_t	*m_pLeafDataList;
};

// Reference counted list of visible world surfaces. The only implementation is
// the engine's CWorldRenderList (gl_rsurf.cpp); the members forward to it.
class IWorldRenderList
{
public:
	int AddRef();
	int Release();
};

//-----------------------------------------------------------------------------
// Describes the fog volume for a particular point
//-----------------------------------------------------------------------------
struct VisibleFogVolumeInfo_t
{
	int		m_nVisibleFogVolume;
	int		m_nVisibleFogVolumeLeaf;
	bool	m_bEyeInFogVolume;
	float	m_flDistanceToWater;
	float	m_flWaterHeight;
	IMaterial *m_pFogVolumeMaterial;
};


//-----------------------------------------------------------------------------
// Vertex format for brush models
//-----------------------------------------------------------------------------
struct BrushVertex_t
{
	Vector		m_Pos;
	Vector		m_Normal;
	Vector		m_TangentS;
	Vector		m_TangentT;
	Vector2D	m_TexCoord;
	Vector2D	m_LightmapCoord;

private:
	BrushVertex_t( const BrushVertex_t& src );
};

//-----------------------------------------------------------------------------
// Visibility data for area portal culling
//-----------------------------------------------------------------------------
struct VisOverrideData_t
{
	Vector		m_vecVisOrigin;					// The point to to use as the viewpoint for area portal backface cull checks.
	float		m_fDistToAreaPortalTolerance;	// The distance from an area portal before using the full screen as the viewable portion.

	// PORTAL2-specific
	Vector		m_vPortalCorners[4];			// When rendering a portal view, these are the 4 corners of the portal you are looking through (used to shrink the frustum)
	bool		m_bTrimFrustumToPortalCorners;

	Vector		m_vPortalOrigin;
	Vector		m_vPortalForward;
	float		m_flPortalRadius;
	
};


//-----------------------------------------------------------------------------
// interface for asking about the Brush surfaces from the client DLL
//-----------------------------------------------------------------------------

class IBrushSurface
{
public:
	// Computes texture coordinates + lightmap coordinates given a world position
	void ComputeTextureCoordinate( Vector const& worldPos, Vector2D& texCoord );
	void ComputeLightmapCoordinate( Vector const& worldPos, Vector2D& lightmapCoord );

	// Gets the vertex data for this surface
	int  GetVertexCount() const;
	void GetVertexData( BrushVertex_t* pVerts );

	// Gets at the material properties for this surface
	IMaterial* GetMaterial();
};


//-----------------------------------------------------------------------------
// interface for installing a new renderer for brush surfaces
//-----------------------------------------------------------------------------

class IBrushRenderer
{
public:
	// Draws the surface; returns true if decals should be rendered on this surface
	virtual bool RenderBrushModelSurface( IClientEntity* pBaseEntity, IBrushSurface* pBrushSurface ) = 0;
};


inline constexpr int MAX_VIS_LEAVES = 32;
//-----------------------------------------------------------------------------
// Purpose: Interface to client .dll to set up a rendering pass over world
//  The client .dll can call Render multiple times to overlay one or more world
//  views on top of one another
//-----------------------------------------------------------------------------
enum DrawBrushModelMode_t
{
	DBM_DRAW_ALL = 0,
	DBM_DRAW_OPAQUE_ONLY,
	DBM_DRAW_TRANSLUCENT_ONLY,
};


//-----------------------------------------------------------------------------
// Brush model instance rendering state
//-----------------------------------------------------------------------------
struct BrushArrayInstanceData_t
{
	matrix3x4a_t *m_pBrushToWorld;			// NOTE: Not inlined because it has alignment restrictions; this way the instance struct size can change without memory penalty
	const model_t *m_pBrushModel;			// NOTE: Only the first two fields are used when rendering shadows
	Vector4D m_DiffuseModulation;
	ShaderStencilState_t *m_pStencilState;
};


class IVRenderView
{
public:

	// Draw normal brush model.
	// If pMaterialOverride is non-null, then all the faces of the bmodel will
	// set this material rather than their regular material.
	void			DrawBrushModel( 
		IClientEntity *baseentity, 
		model_t *model, 
		const Vector& origin, 
		const QAngle& angles, 
		bool bUnused );
	
	// Draw brush model that has no origin/angles change ( uses identity transform )
	// FIXME, Material proxy IClientEntity *baseentity is unused right now, use DrawBrushModel for brushes with
	//  proxies for now.
	void			DrawIdentityBrushModel( IWorldRenderList *pList, model_t *model );

	// Mark this dynamic light as having changed this frame ( so light maps affected will be recomputed )
	void			TouchLight( struct dlight_t *light );
	// Draw 3D Overlays
	void			Draw3DDebugOverlays( void );
	// Sets global blending fraction
	void			SetBlend( float blend );
	float			GetBlend( void );

	// Sets global color modulation
	void			SetColorModulation( float const* blend );
	void			GetColorModulation( float* blend );

	// Wrap entire scene drawing
	void			SceneBegin( void );
	void			SceneEnd( void );

	// Gets the fog volume for a particular point
	void			GetVisibleFogVolume( const Vector& eyePoint, const VisOverrideData_t *pVisOverrideData, VisibleFogVolumeInfo_t *pInfo );

	// Wraps world drawing
	// If iForceViewLeaf is not -1, then it uses the specified leaf as your starting area for setting up area portal culling.
	// This is used by water since your reflected view origin is often in solid space, but we still want to treat it as though
	// the first portal we're looking out of is a water portal, so our view effectively originates under the water.
	IWorldRenderList * CreateWorldList();
	void			BuildWorldLists_Epilogue( IWorldRenderList *pList, WorldListInfo_t* pInfo, bool bShadowDepth );
	void			BuildWorldLists( IWorldRenderList *pList, WorldListInfo_t* pInfo, int iForceFViewLeaf, const VisOverrideData_t* pVisData = nullptr, bool bShadowDepth = false, float *pReflectionWaterHeight = nullptr );
	void			DrawWorldLists( IMatRenderContext *pRenderContext, IWorldRenderList *pList, unsigned long flags, float waterZAdjust );
	void			GetWorldListIndicesInfo( WorldListIndicesInfo_t * pIndicesInfoOut, IWorldRenderList *pList, unsigned long nFlags );

	// Optimization for top view
	void			DrawTopView( bool enable );
	void			TopViewNoBackfaceCulling( bool bDisable );
	void			TopViewNoVisCheck( bool bDisable );
	void			TopViewBounds( Vector2D const& mins, Vector2D const& maxs );
	void			SetTopViewVolumeCuller( const CVolumeCuller *pVolumeCuller );

	// Draw lights
	void			DrawLights( void );
	// FIXME:  This function is a stub, doesn't do anything in the engine right now
	void			DrawMaskEntities( void );

	// Draw surfaces with alpha, don't call in shadow depth pass
	void			DrawTranslucentSurfaces( IMatRenderContext *pRenderContext, IWorldRenderList *pList, int *pSortList, int sortCount, unsigned long flags );

	// Draw Particles ( just draws the linefine for debugging map leaks )
	void			DrawLineFile( void );
	// Draw lightmaps
	void			DrawLightmaps( IWorldRenderList *pList, int pageId );
	// Wraps view render sequence, sets up a view
	void			ViewSetupVis( bool novis, int numorigins, const Vector origin[] );

	// Return true if any of these leaves are visible in the current PVS.
	bool			AreAnyLeavesVisible( int *leafList, int nLeaves );

	// Sets up view fade parameters
	void			ViewDrawFade( byte *color, IMaterial *pMaterial, bool mapFullTextureToScreen = true );
	// Sets up the projection matrix for the specified field of view
	void			OLD_SetProjectionMatrix( float fov, float zNear, float zFar );
	// Determine lighting at specified position
	colorVec		GetLightAtPoint( Vector& pos );
	// Whose eyes are we looking through?
	int				GetViewEntity( void );
	bool			IsViewEntity( int entindex );
	// Get engine field of view setting
	float			GetFieldOfView( void );
	// 1 == ducking, 0 == not
	unsigned char	**GetAreaBits( void );

	// Set up fog for a particular leaf
	void			SetFogVolumeState( int nVisibleFogVolume, bool bUseHeightFog );

	// Installs a brush surface draw override method, null means use normal renderer
	void			InstallBrushSurfaceRenderer( IBrushRenderer* pBrushRenderer );

	// Draw brush model shadow
	void			DrawBrushModelShadow( IClientRenderable *pRenderable );

	// Does the leaf contain translucent surfaces?
	bool			LeafContainsTranslucentSurfaces( IWorldRenderList *pList, int sortIndex, unsigned long flags );

	bool			DoesBoxIntersectWaterVolume( const Vector &mins, const Vector &maxs, int leafWaterDataID );

	void			SetAreaState( 
			unsigned char chAreaBits[MAX_AREA_STATE_BYTES],
			unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] );

	// Push, pop views (see PushViewFlags_t above for flags)
	void			Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	void			Push2DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes );
	void			PopView( IMatRenderContext *pRenderContext, Frustum frustumPlanes );

	// Sets the main view
	void			SetMainView( const Vector &vecOrigin, const QAngle &angles );

	enum
	{
		 VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS = 0x00000001
	};

	// Wraps view render sequence, sets up a view
	void			ViewSetupVisEx( bool novis, int numorigins, const Vector origin[], unsigned int &returnFlags );

	//replaces the current view frustum with a rhyming replacement of your choice
	void			OverrideViewFrustum( Frustum custom );

	void			DrawBrushModelShadowDepth( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, ERenderDepthMode_t DepthMode );
	void			UpdateBrushModelLightmap( model_t *model, IClientRenderable *pRenderable );
	void			BeginUpdateLightmaps( void );
	void			EndUpdateLightmaps();
	void			OLD_SetOffCenterProjectionMatrix( float fov, float zNear, float zFar, float flAspectRatio, float flBottom, float flTop, float flLeft, float flRight );
	void			OLD_SetProjectionMatrixOrtho( float left, float top, float right, float bottom, float zNear, float zFar );
	void			Push3DView( IMatRenderContext *pRenderContext, const CViewSetup &view, int nFlags, ITexture* pRenderTarget, Frustum frustumPlanes, ITexture* pDepthTexture );
	void			GetMatricesForView( const CViewSetup &view, VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection, VMatrix *pWorldToPixels );
	void			DrawBrushModelEx( IClientEntity *baseentity, model_t *model, const Vector& origin, const QAngle& angles, DrawBrushModelMode_t mode );

	bool			DoesBrushModelNeedPowerOf2Framebuffer( const model_t *model );

	// draw an array of brush models (see model_types.h for the model type flags, i.e. STUDIO_SHADOWDEPTHTEXTURE)
	void			DrawBrushModelArray( IMatRenderContext* pContext, int nCount, const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags );
};

// change this when the new version is incompatable with the old
inline constexpr const char *VENGINE_RENDERVIEW_INTERFACE_VERSION = "VEngineRenderView014";

