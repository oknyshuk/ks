#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "datacache/imdlcache.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterial;
struct vcollide_t;
struct model_t;
class CGameTrace;
typedef CGameTrace trace_t;
struct studiohdr_t;
struct virtualmodel_t;
typedef unsigned char byte;
struct virtualterrainparams_t;
class CPhysCollide;
typedef unsigned short MDLHandle_t;
namespace ks { class text_writer; }
class IClientRenderable;


//-----------------------------------------------------------------------------
// Indicates the type of translucency of an unmodulated renderable
//-----------------------------------------------------------------------------
enum RenderableTranslucencyType_t
{
	RENDERABLE_IS_OPAQUE = 0,
	RENDERABLE_IS_TRANSLUCENT,
	RENDERABLE_IS_TWO_PASS,	// has both translucent and opaque sub-partsa
};


//-----------------------------------------------------------------------------
// Purpose: a callback class that is notified when a model has finished loading
//-----------------------------------------------------------------------------
class IModelLoadCallback
{
public:
	virtual void OnModelLoadComplete( const model_t* pModel ) = 0;

protected:
	// Protected destructor so that nobody tries to delete via this interface.
	// Automatically unregisters if the callback is destroyed while still pending.
	~IModelLoadCallback();
};


//-----------------------------------------------------------------------------
// Purpose: Automate refcount tracking on a model index
//-----------------------------------------------------------------------------
class CRefCountedModelIndex
{
private:
	int m_nIndex;
public:
	CRefCountedModelIndex() : m_nIndex( -1 ) { }
	~CRefCountedModelIndex() { Set( -1 ); }

	CRefCountedModelIndex( const CRefCountedModelIndex& src ) : m_nIndex( -1 ) { Set( src.m_nIndex ); }
	CRefCountedModelIndex& operator=( const CRefCountedModelIndex& src ) { Set( src.m_nIndex ); return *this; }

	explicit CRefCountedModelIndex( int i ) : m_nIndex( -1 ) { Set( i ); }
	CRefCountedModelIndex& operator=( int i ) { Set( i ); return *this; }

	int Get() const { return m_nIndex; }
	void Set( int i );
	void Clear() { Set( -1 ); }

	operator int () const { return m_nIndex; }
};


//-----------------------------------------------------------------------------
// Model info interface
//-----------------------------------------------------------------------------

// change this when the new version is incompatable with the old
inline constexpr const char *VMODELINFO_CLIENT_INTERFACE_VERSION = "VModelInfoClient004";
inline constexpr const char *VMODELINFO_SERVER_INTERFACE_VERSION = "VModelInfoServer002";

// MODEL INDEX RULES
// If index >= 0, then index references the precached model string table
// If index == -1, then the model is invalid
// If index < -1, then the model is DYNAMIC and has a DYNAMIC INDEX of (-2 - index)
// - if the dynamic index is ODD, then the model is CLIENT ONLY
//   and has a m_LocalDynamicModels lookup index of (dynamic index)>>1
// - if the dynamic index is EVEN, then the model is NETWORKED
//   and has a dynamic model string table index of (dynamic index)>>1

inline bool IsDynamicModelIndex( int modelindex ) { return modelindex < -1; }
inline bool IsClientOnlyModelIndex( int modelindex ) { return modelindex < -1 && (modelindex & 1); }

class IVModelInfo
{
public:
	enum class kind_t : unsigned char { server, client };
	explicit IVModelInfo( kind_t kind ) : m_kind( kind ) {}


	const model_t			*GetModel( int modelindex ) const;
	// Returns index of model by name
	int						GetModelIndex( const char *name ) const;

	// Returns name of model
	const char				*GetModelName( const model_t *model ) const;
	vcollide_t				*GetVCollide( const model_t *model ) const;
	vcollide_t				*GetVCollide( int modelindex ) const;
	void					GetModelBounds( const model_t *model, Vector& mins, Vector& maxs ) const;
	void					GetModelRenderBounds( const model_t *model, Vector& mins, Vector& maxs ) const;
	int						GetModelFrameCount( const model_t *model ) const;
	int						GetModelType( const model_t *model ) const;
	void					*GetModelExtraData( const model_t *model );
	bool					ModelHasMaterialProxy( const model_t *model ) const;
	bool					IsTranslucent( model_t const* model ) const;
	bool					IsTranslucentTwoPass( const model_t *model ) const;
	RenderableTranslucencyType_t ComputeTranslucencyType( const model_t *model, int nSkin, int nBody );
	int						GetModelMaterialCount( const model_t* model ) const;
	int 					GetModelMaterials( const model_t *model, int count, IMaterial** ppMaterial );
	bool					IsModelVertexLit( const model_t *model ) const;
	const char				*GetModelKeyValueText( const model_t *model );
	bool					GetModelKeyValue( const model_t *model, ks::text_writer &buf ); // supports keyvalue blocks in submodels
	float					GetModelRadius( const model_t *model );

	const studiohdr_t		*FindModel( const studiohdr_t *pStudioHdr, void **cache, const char *modelname ) const;
	const studiohdr_t		*FindModel( void *cache ) const;
	virtualmodel_t			*GetVirtualModel( const studiohdr_t *pStudioHdr ) const;
	byte					*GetAnimBlock( const studiohdr_t *pStudioHdr, int nBlock, bool bPreloadIfMissing ) const;
	bool					HasAnimBlockBeenPreloaded( const studiohdr_t *pStudioHdr, int nBlock ) const;

	// Available on client only!!!
	void					GetModelMaterialColorAndLighting( const model_t *model, Vector const& origin,
										QAngle const& angles, trace_t* pTrace, 
										Vector& lighting, Vector& matColor );
	void					GetIlluminationPoint( const model_t *model, IClientRenderable *pRenderable, Vector const& origin, 
										QAngle const& angles, Vector* pLightingCenter );

	int						GetModelContents( int modelIndex ) const;
	studiohdr_t				*GetStudiomodel( const model_t *mod );
	int						GetModelSpriteWidth( const model_t *model ) const;
	int						GetModelSpriteHeight( const model_t *model ) const;

	// Sets/gets a map-specified fade range (client only)
	void					SetLevelScreenFadeRange( float flMinSize, float flMaxSize );
	void					GetLevelScreenFadeRange( float *pMinArea, float *pMaxArea ) const;

	// Sets/gets a map-specified per-view fade range (client only)
	void					SetViewScreenFadeRange( float flMinSize, float flMaxSize );

	// Computes fade alpha based on distance fade + screen fade (client only)
	unsigned char			ComputeLevelScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const;
	unsigned char			ComputeViewScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const;

	// both client and server
	int						GetAutoplayList( const studiohdr_t *pStudioHdr, unsigned short **pAutoplayList ) const;

	// Gets a virtual terrain collision model (creates if necessary)
	// NOTE: This may return NULL if the terrain model cannot be virtualized
	CPhysCollide			*GetCollideForVirtualTerrain( int index );

	bool					IsUsingFBTexture( const model_t *model, int nSkin, int nBody, void /*IClientRenderable*/ *pClientRenderable ) const;

	const model_t			*FindOrLoadModel( const char *name ) const;

	MDLHandle_t				GetCacheHandle( const model_t *model ) const;

	// Returns planes of non-nodraw brush model surfaces
	int						GetBrushModelPlaneCount( const model_t *model ) const;
	void					GetBrushModelPlane( const model_t *model, int nIndex, cplane_t &plane, Vector *pOrigin ) const;
	int						GetSurfacepropsForVirtualTerrain( int index );
	bool					UsesEnvCubemap( const model_t *model ) const;
	bool					UsesStaticLighting( const model_t *model ) const;

	// Returns index of model by name, dynamically registered if not already known.
	int						RegisterDynamicModel( const char *name, bool bClientSide );
	int						RegisterCombinedDynamicModel( const char *pszName, MDLHandle_t Handle );
	void					UpdateCombinedDynamicModel( int nModelIndex, MDLHandle_t Handle );

	int						BeginCombinedModel( const char *pszName, bool bReuseExisting );
	bool					SetCombineModels( int nModelIndex, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );
	bool					FinishCombinedModel( int nModelIndex, CombinedModelLoadedCallback pFunc, void *pUserData = nullptr );
	void					ReleaseCombinedModel( int nModelIndex );

	bool					IsDynamicModelLoading( int modelIndex );
	void					AddRefDynamicModel( int modelIndex );
	void					ReleaseDynamicModel( int modelIndex );

	// Registers callback for when dynamic model has finished loading.
	bool					RegisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback, bool bCallImmediatelyIfLoaded = true );
	void					UnregisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback );

	// Poked by engine
	void					OnLevelChange();

	KeyValues				*GetModelKeyValues( const model_t *pModel );

	void					UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );
	void					TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );

protected:
	kind_t m_kind;
};


class IVModelInfoClient : public IVModelInfo
{
public:
	explicit IVModelInfoClient( kind_t kind ) : IVModelInfo( kind ) {}

	// Returns a model as a client-side index which is stable across server changes
	int						GetModelClientSideIndex( const char *name ) const;

	// Poked by engine
	void					OnDynamicModelStringTableChanged( int nStringIndex, const char *pString, const void *pData );

	// Reference and Unload
	// Don't assume Reference flags - doing this for now
	model_t					*ReferenceModel( const char *name );
	void					UnreferenceModel( model_t *model );
	void					UnloadUnreferencedModels( void );
};


struct virtualterrainparams_t
{
	// UNDONE: Add grouping here, specified in BSP file? (test grouping to see if this is necessary)
	int index;
};
