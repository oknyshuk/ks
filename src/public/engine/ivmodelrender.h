#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;
#include "istudiorender.h"
#include "datacache/idatacache.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
struct mstudioanimdesc_t;
struct mstudioseqdesc_t;
struct model_t;
class IClientRenderable;
struct studiohdr_t;
class IMaterial;
class CStudioHdr;
struct MaterialLightingState_t;

struct LightCacheHandle_t__;
using LightCacheHandle_t = LightCacheHandle_t__ *; 


//-----------------------------------------------------------------------------
// Model rendering state
//-----------------------------------------------------------------------------
struct DrawModelState_t
{
	studiohdr_t*			m_pStudioHdr;
	studiohwdata_t*			m_pStudioHWData;
	IClientRenderable*		m_pRenderable;
	const matrix3x4_t		*m_pModelToWorld;
	StudioDecalHandle_t		m_decals;
	int						m_drawFlags;
	int						m_lod;
};


//-----------------------------------------------------------------------------
// Model Rendering + instance data
//-----------------------------------------------------------------------------

// change this when the new version is incompatable with the old
inline constexpr const char *VENGINE_HUDMODEL_INTERFACE_VERSION = "VEngineModel016";

typedef unsigned short ModelInstanceHandle_t;

enum
{
	MODEL_INSTANCE_INVALID = (ModelInstanceHandle_t)~0
};

struct ModelRenderInfo_t
{
	Vector origin;
	QAngle angles; 
	IClientRenderable *pRenderable;
	const model_t *pModel;
	const matrix3x4_t *pModelToWorld;
	const matrix3x4_t *pLightingOffset;
	const Vector *pLightingOrigin;
	int flags;
	int entity_index;
	int skin;
	int body;
	int hitboxset;
	ModelInstanceHandle_t instance;

	ModelRenderInfo_t()
	{
		pModelToWorld = nullptr;
		pLightingOffset = nullptr;
		pLightingOrigin = nullptr;
	}
};

struct StaticPropRenderInfo_t
{
	const matrix3x4_t		*pModelToWorld;
	const model_t			*pModel;
	IClientRenderable		*pRenderable;
	Vector					*pLightingOrigin;
	ModelInstanceHandle_t	instance;
	uint8					skin;
	uint8					alpha;
};

struct LightingQuery_t
{
	Vector m_LightingOrigin;
	ModelInstanceHandle_t m_InstanceHandle;
	bool m_bAmbientBoost;
};

struct StaticLightingQuery_t : public LightingQuery_t
{
	IClientRenderable *m_pRenderable;
};


// UNDONE: Move this to hud export code, subsume previous functions
class IVModelRender
{
public:
	int		DrawModel(	int flags,
								IClientRenderable *pRenderable,
								ModelInstanceHandle_t instance,
								int entity_index, 
								const model_t *model, 
								Vector const& origin, 
								QAngle const& angles, 
								int skin,
								int body,
								int hitboxset,
								const matrix3x4_t *modelToWorld = nullptr,
								const matrix3x4_t *pLightingOffset = nullptr );

	// This causes a material to be used when rendering the model instead 
	// of the materials the model was compiled with
	void	ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType = OVERRIDE_NORMAL, int nMaterialIndex = -1 );
	bool	IsForcedMaterialOverride();

	void	SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target );

	// Creates, destroys instance data to be associated with the model
	ModelInstanceHandle_t CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t *pCache = nullptr );
	void DestroyInstance( ModelInstanceHandle_t handle );

	// Associates a particular lighting condition with a model instance handle.
	// FIXME: This feature currently only works for static props. To make it work for entities, etc.,
	// we must clean up the lightcache handles as the model instances are removed.
	// At the moment, since only the static prop manager uses this, it cleans up all LightCacheHandles 
	// at level shutdown.
	void SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t* pHandle );
	LightCacheHandle_t GetStaticLighting( ModelInstanceHandle_t handle );

	// moves an existing InstanceHandle to a nex Renderable to keep decals etc. Models must be the same
	bool ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable );

	// Creates a decal on a model instance by doing a planar projection
	// along the ray. The material is the decal material, the radius is the
	// radius of the decal to create.
	void AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray, 
		Vector const& decalUp, int decalIndex, int body, bool noPokeThru = false, int maxLODToDecal = ADDDECAL_TO_ALL_LODS, IMaterial *pSpecifyMaterial = nullptr, float w=1.0f, float h=1.0f, void *pvProxyUserData = nullptr, int nAdditionalDecalFlags = 0 );

	// Removes all the decals on a model instance
	void RemoveAllDecals( ModelInstanceHandle_t handle );

	// Returns true if the model instance is valid and the decal list is not empty
	bool ModelHasDecals( ModelInstanceHandle_t handle );

	// Remove all decals from all models
	void RemoveAllDecalsFromAllModels( bool bRenderContextValid );

	// Shadow rendering, DrawModelShadowSetup returns the address of the bone-to-world array, NULL in case of error
	matrix3x4a_t* DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4a_t *pCustomBoneToWorld = nullptr );
	void DrawModelShadow(  IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4a_t *pCustomBoneToWorld = nullptr );

	// This gets called when overbright, etc gets changed to recompute static prop lighting.
	bool RecomputeStaticLighting( ModelInstanceHandle_t handle );

	void ReleaseAllStaticPropColorData( void );
	void RestoreAllStaticPropColorData( void );

	// Extended version of drawmodel
	int	DrawModelEx( ModelRenderInfo_t &pInfo );

	int	DrawModelExStaticProp( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo );

	bool DrawModelSetup( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t **ppBoneToWorldOut );
	void DrawModelExecute( IMatRenderContext *pRenderContext, const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pCustomBoneToWorld = nullptr );

	// Sets up lighting context for a point in space
	void SetupLighting( const Vector &vecCenter );
	
	// doesn't support any debug visualization modes or other model options, but draws static props in the
	// fastest way possible
	int DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth );

	// Allow client to override lighting state
	void SuppressEngineLighting( bool bSuppress );

	void SetupColorMeshes( int nTotalVerts );

	// Sets up lighting context for a point in space, with smooth interpolation per model.
	// Passing MODEL_INSTANCE_INVALID as a handle is equivalent to calling SetupLighting.
	void SetupLightingEx( const Vector &vecCenter, ModelInstanceHandle_t handle );

	// Finds the brightest light source illuminating a point. Returns false if there isn't any.
	bool GetBrightestShadowingLightSource( const Vector &vecCenter, Vector& lightPos, Vector& lightBrightness, bool bAllowNonTaggedLights );

	// Computes lighting state for an array of lighting requests
	void ComputeLightingState( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState, ITexture **ppEnvCubemapTexture );

	// Gets an array of decal handles given model instances
	void GetModelDecalHandles( StudioDecalHandle_t *pDecals, int nDecalStride, int nCount, const ModelInstanceHandle_t *pHandles );

	// Computes lighting state for an array of lighting requests for renderables which use static lighting
	void ComputeStaticLightingState( int nCount, const StaticLightingQuery_t *pQuery, MaterialLightingState_t *pState, MaterialLightingState_t *pDecalState, ColorMeshInfo_t **ppStaticLighting, ITexture **ppEnvCubemapTexture, DataCacheHandle_t *pColorMeshHandles );

	// Cleans up lighting state. Must be called after the draw call that uses
	// the color meshes return from ComputeStaticLightingState has been issued
	void CleanupStaticLightingState( int nCount, DataCacheHandle_t *pColorMeshHandles );
};
