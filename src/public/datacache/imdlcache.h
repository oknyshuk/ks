#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1999 Valve Corporation

// Purpose: model loading and caching

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;

#include "filesystem.h"
#include "string_t.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct studiohdr_t;
struct studiohwdata_t;
struct vcollide_t;
struct virtualmodel_t;
struct vertexFileHeader_t;
class IDataCacheSection;
class IVTFTexture;

namespace OptimizedModel
{
	struct FileHeader_t;
}


//-----------------------------------------------------------------------------
// Reference to a loaded studiomdl 
//-----------------------------------------------------------------------------
typedef unsigned short MDLHandle_t;


inline MDLHandle_t VoidPtrToMDLHandle( void *ptr )
{
	return ( MDLHandle_t ) ( int ) ( intp ) ptr;
}

inline void* MDLHandleToVirtual( MDLHandle_t hndl )
{
	return (void*)(uintp)hndl;
}

enum
{
	MDLHANDLE_INVALID = (MDLHandle_t)~0 
};


//-----------------------------------------------------------------------------
// Cache data types
//-----------------------------------------------------------------------------
enum MDLCacheDataType_t
{
	MDLCACHE_NONE = -1,

	// Callbacks to get called when data is loaded or unloaded for these:
	MDLCACHE_STUDIOHDR = 0,
	MDLCACHE_STUDIOHWDATA,
	MDLCACHE_VCOLLIDE,

	// Callbacks NOT called when data is loaded or unloaded for these:
	MDLCACHE_ANIMBLOCK,
	MDLCACHE_VIRTUALMODEL,
	MDLCACHE_VERTEXES,
	MDLCACHE_DECODEDANIMBLOCK
};

class IMDLCacheNotify
{
public:

	// Called right after the data is loaded
	void OnDataLoaded( MDLCacheDataType_t type, MDLHandle_t handle );

	// Called right before combiner process data into cache
	void OnCombinerPreCache( MDLHandle_t OldHandle, MDLHandle_t NewHandle );

	// Called right before the data is unloaded
	void OnDataUnloaded( MDLCacheDataType_t type, MDLHandle_t handle );

	bool ShouldSupressLoadWarning( MDLHandle_t handle );
};


//-----------------------------------------------------------------------------
// Flags for flushing 
//-----------------------------------------------------------------------------
enum MDLCacheFlush_t
{
	MDLCACHE_FLUSH_STUDIOHDR		= 0x01,
	MDLCACHE_FLUSH_STUDIOHWDATA		= 0x02,
	MDLCACHE_FLUSH_VCOLLIDE			= 0x04,
	MDLCACHE_FLUSH_ANIMBLOCK		= 0x08,
	MDLCACHE_FLUSH_VIRTUALMODEL		= 0x10,
	MDLCACHE_FLUSH_AUTOPLAY         = 0x20,
	MDLCACHE_FLUSH_VERTEXES         = 0x40,
	MDLCACHE_FLUSH_COMBINED_DATA	= 0x80,

	MDLCACHE_FLUSH_IGNORELOCK       = 0x80000000,
	MDLCACHE_FLUSH_ALL              = 0xFFFFFFFF
};


//-----------------------------------------------------------------------------
// Combiner
//-----------------------------------------------------------------------------
inline constexpr auto COMBINER_MAX_MODELS = 15;
inline constexpr auto COMBINER_MAX_LODS = 8;
inline constexpr auto COMBINER_MAX_TEXTURES_PER_MATERIAL = 5;
inline constexpr auto COMBINER_MAX_ATLAS_GROUPS = 5;
inline constexpr auto COMBINER_MAX_MATERIALS_PER_INPUT_MODEL = 4;


enum ECombinedResult
{
	COMBINE_RESULT_FLAG_OK = 0,
	COMBINE_RESULT_FLAG_TOO_MANY_STRINGS,
	COMBINE_RESULT_FLAG_OUT_OF_MEMORY,
	COMBINE_RESULT_FLAG_TOO_MANY_BONES,
	COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE,	// Major feature of the formats that isn't supported.  You'll need to debug the specific cause of this.
	COMBINE_RESULT_FLAG_UNHANDLED_ISSUE,		// Minor section of the formats that wasn't expected.  You'll need to debug the specific cause of this.
	COMBINE_RESULT_FLAG_MISSING_ASSET_FILE,
	COMBINE_RESULT_FLAG_UNSUPPORTED_SHADER,
	COMBINE_RESULT_FLAG_FAILED_GOOD_PRACTICE,	// Not necessarily an error, but sanity checks to make sure we are doing good practices
	COMBINE_RESULT_FLAG_KEY_VALUE_FAILURE,
};


enum EDetailedError
{
	COMBINED_DETAIL_ERROR_NOT_SPECIFIED = 0,
	COMBINED_DETAIL_ERROR_TEXTURE_PACKER_NO_SPACE,
	COMBINED_DETAIL_ERROR_MODEL_PACKER_TOO_MANY_MATERIALS,
	COMBINED_DETAIL_ERROR_TEXTURE_COORDINATES_WRAP,
	COMBINED_DETAIL_ERROR_MODEL_LOWER_LOD_HIGHER_TRI_COUNT,
};


typedef struct SCombinedResults
{
	// individual model info
	int							m_nNumVerts[ COMBINER_MAX_MODELS ][ COMBINER_MAX_LODS ];
	int							m_nNumIndexes[ COMBINER_MAX_MODELS ][ COMBINER_MAX_LODS ];
	int							m_nBatches[ COMBINER_MAX_MODELS ][ COMBINER_MAX_LODS ];
	int							m_nNumLODs[ COMBINER_MAX_MODELS ];

	// cumulative combined info
	int							m_nCombinedNumVerts[ COMBINER_MAX_LODS ];
	int							m_nCombinedNumIndexes[ COMBINER_MAX_LODS ];
	int							m_nCombinedBatches[ COMBINER_MAX_LODS ];
	int							m_nCombinedNumLODs;
	int							m_nNumTexturePackIterations;
	unsigned int				m_nCombinedResults;
	char						m_szErrorMessage[ 256 ];
	char						m_szErrorDetails[ 4096 ];
	unsigned int				m_nDetailedError;
	float						m_flModelLoadDuration;
	float						m_flModelCombineDuration;
	float						m_flTextureLoadDuration;
	float						m_flTextureCombineDuration;
	float						m_flEngineProcessingDuration;
} TCombinedResults;


enum ECombinedAsset
{
	COMBINED_ASSET_MATERIAL	= 0,
	COMBINED_ASSET_TEXTURE,

	COMBINED_ASSET_MAX
};


enum ECombinerFlags
{
	COMBINER_FLAG_THREADING				= 0x00000001,
	COMBINER_FLAG_NO_DATA_PROCESSING	= 0x00000002,
};

struct STextureSubstitute_t
{
	STextureSubstitute_t()
		: m_iszMaterialParam( NULL_STRING )
		, m_pVTFTexture( nullptr )
	{
	}

	string_t	m_iszMaterialParam;
	IVTFTexture *m_pVTFTexture;
};

struct SCombinerModelInput_t
{
	SCombinerModelInput_t()
		: m_iszModelName( NULL_STRING )
		, m_nSkinFamily( 0 )
		, m_nBodyGroupSubModelSelection ( -1 )
	{
	}

	SCombinerModelInput_t( string_t iszModelName, int nSkinFamily = 0, int nBodyGroupSubModelSelection = -1 )
		: m_iszModelName( iszModelName )
		, m_nSkinFamily( nSkinFamily )
		, m_nBodyGroupSubModelSelection( nBodyGroupSubModelSelection )
	{
	}

	string_t				m_iszModelName;
	int						m_nSkinFamily;
	int						m_nBodyGroupSubModelSelection; // -1 means no selection, include all body part sub models
	STextureSubstitute_t	m_textureSubstitutes[ COMBINER_MAX_MATERIALS_PER_INPUT_MODEL ][ COMBINER_MAX_TEXTURES_PER_MATERIAL ];	// entries can have NULL contents, it means don't substitute (use the one the mdl says)
};

// Callback for the combining process
// pUserData: the pointer you optionally specified in the FinishedCombinedModel() call
// OldHandle: either the temporary model model returned by CreateCombinedModel() or MDLHANDLE_INVALID if reusing an existing handle
// NewHandle: the combined model handle, which may be your reused model handle specified in CreateCombinedModel()
// CombinedResults: various statistical and timing information about the model and combining process
typedef void (*CombinedModelLoadedCallback)( void *pUserData, MDLHandle_t OldHandle, MDLHandle_t NewHandle, TCombinedResults &CombinedResults );


//-----------------------------------------------------------------------------
// The main MDL cacher 
//----------------------------------------------------------------------------- 
class IMDLCache
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	// Used to install callbacks for when data is loaded + unloaded
	// Returns the prior notify
	void SetCacheNotify( IMDLCacheNotify *pNotify );

	// NOTE: This assumes the "GAME" path if you don't use
	// the UNC method of specifying files. This will also increment
	// the reference count of the MDL
	MDLHandle_t FindMDL( const char *pMDLRelativePath );

	// Reference counting
	int AddRef( MDLHandle_t handle );
	int Release( MDLHandle_t handle );
	int GetRef( MDLHandle_t handle );

	// Gets at the various data associated with a MDL
	studiohdr_t *GetStudioHdr( MDLHandle_t handle );
	studiohwdata_t *GetHardwareData( MDLHandle_t handle );
	vcollide_t *GetVCollide( MDLHandle_t handle );
	unsigned char *GetAnimBlock( MDLHandle_t handle, int nBlock, bool preloadIfMissing );
	bool HasAnimBlockBeenPreloaded( MDLHandle_t handle, int nBlock );
	virtualmodel_t *GetVirtualModel( MDLHandle_t handle );
	int GetAutoplayList( MDLHandle_t handle, unsigned short **pOut );
	vertexFileHeader_t *GetVertexData( MDLHandle_t handle );

	// Brings all data associated with an MDL into memory
	void TouchAllData( MDLHandle_t handle );

	// Gets/sets user data associated with the MDL
	void SetUserData( MDLHandle_t handle, void* pData );
	void *GetUserData( MDLHandle_t handle );

	// Is this MDL using the error model?
	bool IsErrorModel( MDLHandle_t handle );
	bool IsOverBudget( MDLHandle_t handle );

	// Flushes the cache, force a full discard
	void Flush( MDLCacheFlush_t nFlushFlags = MDLCACHE_FLUSH_ALL );

	// Flushes a particular model out of memory
	void Flush( MDLHandle_t handle, int nFlushFlags = MDLCACHE_FLUSH_ALL );

	// Returns the name of the model (its relative path)
	const char *GetModelName( MDLHandle_t handle );

	IDataCacheSection *GetCacheSection( MDLCacheDataType_t type );

	// faster access when you already have the studiohdr
	virtualmodel_t *GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle );

	// all cache entries that subsequently allocated or successfully checked 
	// are considered "locked" and will not be freed when additional memory is needed
	void BeginLock();

	// reset all protected blocks to normal
	void EndLock();

	// returns a pointer to a counter that is incremented every time the cache has been out of the locked state (EVIL)
	int *GetFrameUnlockCounterPtrOLD();

	// Finish all pending async operations
	void FinishPendingLoads();

	vcollide_t *GetVCollideEx( MDLHandle_t handle, bool synchronousLoad = true );
	bool GetVCollideSize( MDLHandle_t handle, int *pVCollideSize );

	bool GetAsyncLoad( MDLCacheDataType_t type );
	bool SetAsyncLoad( MDLCacheDataType_t type, bool bAsync );

	void BeginMapLoad();
	void EndMapLoad();
	void MarkAsLoaded( MDLHandle_t handle );

	void InitPreloadData( bool rebuild );
	void ShutdownPreloadData();

	bool IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type );

	int *GetFrameUnlockCounterPtr( MDLCacheDataType_t type );

	studiohdr_t *LockStudioHdr( MDLHandle_t handle );
	void UnlockStudioHdr( MDLHandle_t handle );

	bool PreloadModel( MDLHandle_t handle );

	// Hammer uses this. If a model has an error loading in GetStudioHdr, then it is flagged
	// as an error model and any further attempts to load it will just get the error model.
	// That is, until you call this function. Then it will load the correct model.
	void ResetErrorModelStatus( MDLHandle_t handle );

	void MarkFrame();

	// Locking for things that we can lock over longer intervals than
	// resources locked by BeginLock/EndLock
	void BeginCoarseLock();
	void EndCoarseLock();

	void ReloadVCollide( MDLHandle_t handle );

	bool ReleaseAnimBlockAllocator();

	bool RestoreHardwareData( MDLHandle_t handle, FSAsyncControl_t *pAsyncVTXControl, FSAsyncControl_t *pAsyncVVDControl );

	void DisableVCollideLoad();
	void EnableVCollideLoad();

	void DisableFileNotFoundWarnings();
	void EnableFileNotFoundWarnings();

	bool ProcessPendingHardwareRestore();

	void UnloadQueuedHardwareData( );

	void DumpDictionaryState( );

	// ========================
	// combined models

	// This will create a combined model by the name specified in pszModelName and return back a temporary model handle.
	// May return MDLHANDLE_INVALID if the name is a duplicate of an existing model
	MDLHandle_t	CreateCombinedModel( const char *pszModelName );

	// This will create a combined model in replace of an existing model handle.  The handle should be valid to use during the entire process.
	// Returns true if the handle is acceptable to use for the combining process.
	bool		CreateCombinedModel( MDLHandle_t handle );

	// Sets the models for the combining process
	bool		SetCombineModels( MDLHandle_t handle, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );

	// Indicates that you are done with the setup of the combining process.  You must specify a callback function and optionally
	// a user pointer that will be supplied to you in the callback
	bool		FinishCombinedModel( MDLHandle_t handle, CombinedModelLoadedCallback pFunc, void *pUserData = nullptr );

	// Lets you query a model handle to see if it is a placeholder returned by CreateCombinedModel()
	bool		IsCombinedPlaceholder( MDLHandle_t handle );

	// Lets you query a model handle to see if it is a combined model
	bool		IsCombinedModel( MDLHandle_t handle );

	// Returns the number of models contained in the combined model.  Includes primary and all secondary models
	int			GetNumCombinedSubModels( MDLHandle_t handle );

	// Gets the file name for a sub model
	void		GetCombinedSubModelFilename( MDLHandle_t handle, int nSubModelIndex, char *pszResult, int nResultSize );

	// Returns the KV of the material set for the model.  You are responsible for calling deleteThis() on the result.
	KeyValues	*GetCombinedMaterialKV( MDLHandle_t handle, int nAtlasGroup = 0 );	

	// Internal engine use function to help drive the combining process.
	void		UpdateCombiner( );	// should be called from main thread after rendering has completed

	// Internal engine function to help get access to specific combined assets
	void		*GetCombinedInternalAsset( ECombinedAsset AssetType, const char *pszAssetID = nullptr, int *nSize = nullptr );

	// Set Combiner Flags
	void		SetCombinerFlags( unsigned nFlags );

	// Clear Combiner Flags
	void		ClearCombinerFlags( unsigned nFlags );

	// Dump out resident combiner info
	void		DebugCombinerInfo( );
};

extern IMDLCache *g_pMDLCache;;


//-----------------------------------------------------------------------------
// Critical section helper code
//-----------------------------------------------------------------------------
class CMDLCacheCriticalSection
{
public:
	CMDLCacheCriticalSection( IMDLCache *pCache ) : m_pCache( pCache )
	{
		m_pCache->BeginLock();
	}

	~CMDLCacheCriticalSection()
	{
		m_pCache->EndLock();
	}

private:
	IMDLCache *m_pCache;
};

class CMDLCacheCoarseCriticalSection
{
public:
	CMDLCacheCoarseCriticalSection( IMDLCache *pCache ) : m_pCache( pCache )
	{
		m_pCache->BeginCoarseLock();
	}

	~CMDLCacheCoarseCriticalSection()
	{
		m_pCache->EndCoarseLock();
	}

private:
	IMDLCache *m_pCache;
};


#define MDLCACHE_CRITICAL_SECTION_( pCache ) CMDLCacheCriticalSection cacheCriticalSection(pCache)
#define MDLCACHE_COARSE_LOCK_( pCache ) CMDLCacheCoarseCriticalSection cacheCoarseCriticalSection(pCache)
#define MDLCACHE_CRITICAL_SECTION() MDLCACHE_CRITICAL_SECTION_(mdlcache)
#define MDLCACHE_COARSE_LOCK() MDLCACHE_COARSE_LOCK_(mdlcache)
