#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

struct model_t;
class IMaterial;
class IFileList;


#include "engine/ivmodelinfo.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IModelLoader
{
public:
	enum REFERENCETYPE
	{
		// The name is allocated, but nothing else is in memory or being referenced
		FMODELLOADER_NOTLOADEDORREFERENCED = 0,
		// The model has been loaded into memory
		FMODELLOADER_LOADED	= (1<<0),

		// The model is being referenced by the server code
		FMODELLOADER_SERVER = (1<<1),
		// The model is being referenced by the client code
		FMODELLOADER_CLIENT = (1<<2),
		// The model is being referenced in the client .dll
		FMODELLOADER_CLIENTDLL = (1<<3),
		// The model is being referenced by static props
		FMODELLOADER_STATICPROP	= (1<<4),
		// The model is a detail prop
		FMODELLOADER_DETAILPROP = (1<<5),
		// The model is the simple version of the world geometry
		FMODELLOADER_SIMPLEWORLD = (1<<6),
		// The model is dynamically loaded
		FMODELLOADER_DYNSERVER = (1<<7),
		FMODELLOADER_DYNCLIENT = (1<<8),
		FMODELLOADER_COMBINED = (1<<9),
		FMODELLOADER_DYNAMIC = FMODELLOADER_DYNSERVER | FMODELLOADER_DYNCLIENT | FMODELLOADER_COMBINED,

		FMODELLOADER_REFERENCEMASK = (FMODELLOADER_SERVER | FMODELLOADER_CLIENT | FMODELLOADER_CLIENTDLL | FMODELLOADER_STATICPROP | FMODELLOADER_DETAILPROP | FMODELLOADER_DYNAMIC | FMODELLOADER_SIMPLEWORLD ),

		// The model was touched by the preload method
		FMODELLOADER_TOUCHED_BY_PRELOAD = (1<<15),
		// The model was loaded by the preload method, a postload fixup is required
		FMODELLOADER_LOADED_BY_PRELOAD = (1<<16),
		// The model touched its materials as part of its load
		FMODELLOADER_TOUCHED_MATERIALS = (1<<17),
	};

	enum ReloadType_t
	{
		RELOAD_LOD_CHANGED = 0,
		RELOAD_EVERYTHING,
		RELOAD_REFRESH_MODELS,
	};

	// Start up modelloader subsystem
	void		Init( void );
	void		Shutdown( void );

	// Look up name for model
	const char *GetName( const model_t *model );

	// Check for extra data, reload studio model if needed
	void		*GetExtraData( model_t *model );

	// Get disk size for model
	int			GetModelFileSize( const char *name );

	// Finds the model, and loads it if it isn't already present.  Updates reference flags
	model_t		*GetModelForName( const char *name, REFERENCETYPE referencetype );
	model_t		*ReferenceModel( const char *name, REFERENCETYPE referencetype );
	// Unmasks the referencetype field for the model
	void		UnreferenceModel( model_t *model, REFERENCETYPE referencetype );
	// Unmasks the specified reference type across all models
	void		UnreferenceAllModels( REFERENCETYPE referencetype );

	// For any models with referencetype blank, frees all memory associated with the model
	//  and frees up the models slot
	void		UnloadUnreferencedModels( void );
	void		PurgeUnusedModels( void );

	void		UnMountCompatibilityPaths( void );
	void		AddCompatibilityPath( const char *szNewCompatibilityPath );

	// On the client only, there is some information that is computed at the time we are just
	//  about to render the map the first time.  If we don't change/unload the map, then we
	//  shouldn't have to recompute it each time we reconnect to the same map
	bool		Map_GetRenderInfoAllocated( void );
	void		Map_SetRenderInfoAllocated( bool allocated );

	// Load all the displacements for rendering. Set bRestoring to true if we're recovering from an alt+tab.
	void		Map_LoadDisplacements( model_t *model, bool bRestoring );

	// Print which models are in the cache/known
	void		Print( void );

	// Validate version/header of a .bsp file
	bool		Map_IsValid( char const *pBaseMapName, bool bQuiet = false );

	// Recomputes surface flags
	void		RecomputeSurfaceFlags( model_t *mod );

	// Reloads all models
	void		Studio_ReloadModels( ReloadType_t reloadType );

	// Is a model loaded?
	bool		IsLoaded( const model_t *mod );

	bool		LastLoadedMapHasHDRLighting( void );

	bool		LastLoadedMapHasLightmapAlphaData( void );

	// See CL_HandlePureServerWhitelist for what this is for.
	void		ReloadFilesInList( IFileList *pFilesToReload );

	// Called by app system once per frame to poll and update dynamic models
	void		UpdateDynamicModels();

	// Called by server and client engine code to flush unreferenced dynamic models
	void		FlushDynamicModels();

	// Called by server and client engine code to flush unreferenced dynamic models
	void		ForceUnloadNonClientDynamicModels();

	// Called by client code to load dynamic models, instead of GetModelForName.
	model_t		*GetDynamicModel( const char *name, bool bClientOnly );
	model_t		*GetDynamicCombinedModel( const char *name, bool bClientOnly );
	void		UpdateDynamicCombinedModel( model_t *pModel, MDLHandle_t Handle, bool bClientSide );

	bool		SetCombineModels( model_t *pModel, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );
	bool		FinishCombinedModel( model_t *pModel, CombinedModelLoadedCallback pFunc, void *pUserData = nullptr );

	// Called by client code to query dynamic model state
	bool		IsDynamicModelLoading( model_t *pModel );

	// Called by client code to refcount dynamic models
	void		AddRefDynamicModel( model_t *pModel, bool bClientSideRef );
	void		ReleaseDynamicModel( model_t *pModel, bool bClientSideRef );

	// Called by client code
	bool		RegisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded = true );

	// Called by client code or IModelLoadCallback destructor
	void		UnregisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback );

	// Called by client engine code when server indicates that a model has finished loading or has been unloaded
	void		Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded );

	void		UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );
	void		TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );
	bool		IsModelInWeaponCache( const model_t *pModel );
	void		EvictAllWeaponsFromModelCache( bool bLoadingComplete );
	bool		IsViewWeaponModelResident( const model_t *pModel );
};

extern IModelLoader *modelloader;

//-----------------------------------------------------------------------------
// Purpose: Loads the lump to temporary memory and automatically cleans up the
//  memory when it goes out of scope.
//-----------------------------------------------------------------------------

class CMapLoadHelper
{
public:
						CMapLoadHelper( int lumpToLoad, bool bUncompress = true );
						~CMapLoadHelper( void );

	// Get raw memory pointer
	byte				*LumpBase( void );
	int					LumpSize( void );
	int					LumpOffset( void );
	int					LumpVersion() const;
	const char			*GetMapPathName( void );
	char				*GetLoadName( void );
	char				*GetDiskName( void );
	struct worldbrushdata_t	*GetMap( void );
	// use these to explicitly manage compressed lumps (by default, decompression is done transparently):
	int					UncompressedLumpSize( void );
	void				UncompressLump( void *pExternalBuffer = nullptr );

	// Global setup/shutdown
	static void			Init( model_t *pMapModel, const char *pLoadname );
	static void			InitFromMemory( model_t *pMapModel, const void *pData, int nDataSize );
	static void			Shutdown( void );
	static int			GetRefCount( void );
	
	// Free the lighting lump (increases free memory during loading on 360)
	static void			FreeLightingLump();

	// Returns the size of a particular lump without loading it
	static int			LumpSize( int lumpId );
	static int			LumpOffset( int lumpId );

	// Loads one element in a lump.
	void				LoadLumpElement( int nElemIndex, int nElemSize, void *pData );
	void				LoadLumpData( int offset, int size, void *pData );

private:
	int					m_nLumpSize;
	int					m_nLumpOffset;
	int					m_nLumpVersion;
	byte				*m_pRawData;
	byte				*m_pData;
	byte				*m_pUncompressedData;
	int					m_nUncompressedLumpSize;
	bool				m_bUncompressedDataExternal;

	// Handling for lump files
	int					m_nLumpID;
	char				m_szLumpFilename[MAX_PATH];
};


//-----------------------------------------------------------------------------
// Recomputes translucency for the model...
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t Mod_ComputeTranslucencyType( model_t* mod, int nSkin, int nBody );


//-----------------------------------------------------------------------------
// game lumps
//-----------------------------------------------------------------------------

int Mod_GameLumpSize( int lumpId );
int Mod_GameLumpVersion( int lumpId );
bool Mod_LoadGameLump( int lumpId, void* pBuffer, int size );

// returns the material count...
int Mod_GetMaterialCount( model_t* mod );

// returns the first n materials.
int Mod_GetModelMaterials( model_t* mod, int count, IMaterial** ppMaterial );

bool Mod_MarkWaterSurfaces( model_t *pModel );

//-----------------------------------------------------------------------------
// Hooks the cache notify into the MDL cache system 
//-----------------------------------------------------------------------------
void ConnectMDLCacheNotify( );
void DisconnectMDLCacheNotify( );

//-----------------------------------------------------------------------------
// Initialize studiomdl state
//-----------------------------------------------------------------------------
void InitStudioModelState( model_t *pModel );

//-----------------------------------------------------------------------------
// Convert the full bsp name into the actual platform bsp name
//-----------------------------------------------------------------------------
char *GetMapPathNameOnDisk( char *pDiskName, const char *pFullMapName, unsigned int nDiskNameSize );

// has the lightstyle lighting changed since last updated?
bool Mod_NeedsLightstyleUpdate( model_t *pModel );

// set the material system config for hdr or not
void EnableHDR( bool bEnable );

void DeallocateLightingData( worldbrushdata_t *pBrushData );

extern bool g_bLoadedMapHasBakedPropLighting;
extern bool g_bBakedPropLightingNoSeparateHDR;
extern bool g_bHasLightmapAlphaData;
extern bool g_bBakedPropLightingStreams3;
extern bool g_bHasLightmapAlphaData3;
