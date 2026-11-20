// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include "edict.h"
#include "engine/ishadowmgr.h"
#include "engine_model_client.h"
#include "iclientrenderable.h"
#include "ivrenderview.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "basehandle.h"
#include "con_nprint.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "gamerules_register.h"
#include "tier2/fileutils.h"
#include "cmodel.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.tslist;
import ks.core.types;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.memstack;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :achievements_and_stats_interface;
import :iinput;
import :sharedvar;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import :noopanimcontroller;
import ks.tier1.strtools;
import ks.game.gamemovement;
import ks.tier1.bitbuf;
import ks.tier1.keyvalues;
import ks.vstdlib.vstrtools;
import :cstrike15.cs_shareddefs;
import :econ.econ_item_schema_minimal;
import ks.tier1.intrusive_list;
import ks.tier1.intrusive_ptr;
import ks.tier1.map_ops;
import ks.tier1.tracked_ref;

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Fast path model rendering

#undef max
#undef min

//-----------------------------------------------------------------------------
// Convars defined by other systems
//-----------------------------------------------------------------------------
ConVar r_lod( side_registry(), "r_lod", "-1" );
ConVar r_shadowlod( side_registry(), "r_shadowlod", "-1" );
ConVar r_drawmodellightorigin( side_registry(), "r_DrawModelLightOrigin", "0", FCVAR_CHEAT );
extern ConVar g_CV_FlexSmooth;
extern ConVar r_fastzreject;

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class CModelRenderSystem : public CAutoGameSystem, public IModelRenderSystem
{
	// Methods of IModelRenderSystem
public:
	virtual void DrawModels( ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode, bool bShadowDepthIncludeTranslucentMaterials = false );
	virtual void DrawBrushModels( ModelRenderSystemData_t *pModels, int nCount, ModelRenderMode_t renderMode );
	virtual void ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData );
	virtual void CleanupTranslucentTempData( TranslucentTempData_t *pTempData );
	virtual IMaterial *GetFastPathColorMaterial() { return m_DebugMaterial; }

	// Methods of IGameSystem
public:
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

	// Other public methods
public:
	CModelRenderSystem();
	virtual ~CModelRenderSystem();

private:
	struct ModelListNode_t
	{
		ModelRenderSystemData_t m_Entry;
		int32 m_nInitialListIndex : 24;
		uint32 m_bBoneMerge : 1;
		int32 m_nLOD : 7;
		ShaderStencilState_t *m_pStencilState;
		ModelListNode_t *m_pNext;
	};

	struct RenderModelInfo_t : public StudioArrayInstanceData_t
	{
		ModelRenderSystemData_t m_Entry;
		ModelInstanceHandle_t m_hInstance;
		matrix3x4a_t* m_pBoneToWorld;
		uint32 m_nInitialListIndex : 24;
		uint32 m_bSetupBonesOnly : 1;
		uint32 m_bBoneMerge : 1;
	};
						   
	struct ModelListByType_t : public StudioModelArrayInfo_t
	{
		RenderableLightingModel_t m_nLightingModel;
		const model_t *m_pModel;
		ModelListNode_t *m_pFirstNode;
		int m_nCount;
		int m_nSetupBoneCount;
		uint32 m_nParentDepth : 31;
		uint32 m_bWantsStencil : 1;
		RenderModelInfo_t *m_pRenderModels;
		ModelListByType_t *m_pNextLightingModel;

		// speed up std::sort by implementing these
		ModelListByType_t &operator=( const ModelListByType_t &rhs )
		{
			memcpy( this, &rhs, sizeof( ModelListByType_t ) );
			return *this;
		}

		ModelListByType_t() {}

		ModelListByType_t( const ModelListByType_t &rhs )
		{
			memcpy( this, &rhs, sizeof( ModelListByType_t ) );
		}
	};

	struct BrushModelList_t
	{
		ModelRenderSystemData_t m_Entry;
		uint32 m_nParentDepth : 31;
		uint32 m_bWantsStencil : 1;
		BrushArrayInstanceData_t *m_pInstanceData;

		BrushModelList_t() {}
	};

	struct LightingList_t
	{
		ModelListByType_t *m_pFirstModel;
		int m_nCount;
		int m_nTotalModelCount;
	};

private:
	int BucketModelsByMDL( ModelListByType_t *pModelList, ModelListNode_t *pModelListNodes, ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode, int *pModelsRenderingStencilCountOut );
	uint AddModelToLists( int &nModelTypeCount, ModelListByType_t *pModelList, int &nModelNodeCount, ModelListNode_t *pModelListNodes, int nDataIndex, ModelRenderSystemData_t &data, ModelRenderMode_t renderMode );
	void SortBucketsByDependency( int nModelTypeCount, ModelListByType_t *pModelList, LightingList_t *pLightingList ); 
	void ComputeModelLODs( int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode, ModelRenderMode_t renderMode );
	void SlamModelLODs( int nLOD, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode );
	void SortModels( RenderModelInfo_t *pSortedModelListNode, int nListTotal, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode );
	static bool SortLessFunc( const RenderModelInfo_t &left, const RenderModelInfo_t &right );
	void SetupBones( int nModelTypeCount, ModelListByType_t *pModelList );
	void SetupFlexes( int nModelTypeCount, ModelListByType_t *pModelList );
	void ComputeLightingOrigin( ModelListByType_t &list, LightingQuery_t *pLightingQuery, int nQueryStride );
	int SetupLighting( LightingList_t *pLightingList, int nModelTypeCount, ModelListByType_t *pModelList, DataCacheHandle_t *pColorMeshHandles, ModelRenderMode_t renderMode );
	void RenderModels( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, ModelRenderMode_t renderMode, bool bShadowDepthIncludeTranslucentMaterials = false );
	void SetupTranslucentData( int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, TranslucentInstanceRenderData_t *pRenderData );
	void SetupFlashlightsAndDecals( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, RenderModelInfo_t *pModelInfo, ModelRenderMode_t renderMode );
	void SetupPerInstanceColorModulation( int nModelTypeCount, ModelListByType_t *pModelList );
	void DebugDrawLightingOrigin( const ModelListByType_t &list, const RenderModelInfo_t &model );
	int BuildLightingList( ModelListByType_t **ppLists, unsigned char *pFlags, int *pTotalModels, const LightingList_t &lightingList );
	int SetupStaticPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandles );
	void SetupStandardLighting( LightingList_t &lightingList );
	int SetupPhysicsPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandles );

	void HookUpStaticLightingState( int nCount, ModelListByType_t **ppLists, unsigned char *pFlags, ITexture **ppEnvCubemap, MaterialLightingState_t *pLightingState, MaterialLightingState_t *pDecalLightingState, ColorMeshInfo_t **ppColorMeshInfo );
	void RenderDebugOverlays( int nModelTypeCount, ModelListByType_t *pModelList, ModelRenderMode_t renderMode );
	void RenderVCollideDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList );
	void RenderBBoxDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList );
	int ComputeParentDepth( CBaseEntity *pEnt );
	static bool DependencySortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right );
	static bool StencilSortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right );

	// Methods related to fastpath brush model rendering
	void AddBrushModelToList( int nInitialListIndex, ModelRenderSystemData_t &data, ModelRenderMode_t renderMode,
		BrushArrayInstanceData_t &instance, matrix3x4a_t &brushToWorld );
	void SetupPerInstanceColorModulation( int nCount, ModelRenderSystemData_t *pModels, BrushArrayInstanceData_t *pInstanceData, ModelRenderMode_t renderMode );

	CMemoryStack m_BoneToWorld;
	CTextureReference m_DefaultCubemap;
	CMaterialReference m_DebugMaterial;
	CMaterialReference m_ShadowBuild;
	IMatRenderContext *m_pRenderContext;

	int m_nColorMeshHandles;
	int m_nModelTypeCount;
	int m_nTotalModelCount;
	bool m_bShadowDepth;
	bool m_bHasInstanceData;
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CModelRenderSystem s_ModelRenderSystem;
IModelRenderSystem *g_pModelRenderSystem = &s_ModelRenderSystem;

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CModelRenderSystem::CModelRenderSystem()
{
	m_bHasInstanceData = false;
	m_BoneToWorld.Init( "CModelRenderSystem::m_BoneToWorld", 1 * 1024 * 1024, 32 * 1024, 0, 32 );
}

CModelRenderSystem::~CModelRenderSystem()
{
	m_BoneToWorld.Term();
}

//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CModelRenderSystem::LevelInitPostEntity()
{
	m_DefaultCubemap.Init( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP );
	m_DebugMaterial.Init( "debug/debugempty", TEXTURE_GROUP_OTHER );
	m_ShadowBuild.Init( "engine/shadowbuild", TEXTURE_GROUP_OTHER );
}

void CModelRenderSystem::LevelShutdownPreEntity()
{
	m_DefaultCubemap.Shutdown();
	m_DebugMaterial.Shutdown();
	m_ShadowBuild.Shutdown();
}

//-----------------------------------------------------------------------------
// returns bone setup dependency depth
//-----------------------------------------------------------------------------
int CModelRenderSystem::ComputeParentDepth( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return 0;

	int nDepth = 0;
	while ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && pEnt->GetParentAttachment() > 0 ) )
	{
		++nDepth;
		pEnt = pEnt->GetMoveParent();
	}

	return nDepth;
}

//-----------------------------------------------------------------------------
// Adds a model to the appropriate render lists
//-----------------------------------------------------------------------------
uint CModelRenderSystem::AddModelToLists( int &nModelTypeCountInOut, ModelListByType_t *pModelList, 
	int &nModelNodeCount, ModelListNode_t *pModelListNodes, int nInitialListIndex, ModelRenderSystemData_t &data, ModelRenderMode_t renderMode )
{
	int nModelTypeCount = nModelTypeCountInOut;
	// NOTE: we actually are bucketing both by model + also by lighting model
	// Bucketing by lighting model is not strictly necessary, but doing so
	// simplifies the code a lot in exchange for having two batches if the
	// same model is used but a different lighting model, something that could
	// theoretically happen if a static prop + physics prop use the same .mdl
	// My thought is that even if this split happens, it will be rare, and
	// we still will get a lot of sharing.

	// L4D2: We'll also bucket by whether the model renders stencil or not.
	// This allows us to keep stenciling models in the fastpath but group them together
	// to be rendered AFTER the 360 Z prepass ends. (360 doesn't allow stencil rendering
	// during the Z prepass).

	const model_t *pModel = data.m_pRenderable->GetModel();
	contract_assert( modelinfo->GetModelType( pModel ) == mod_studio );

	// PerfectWorld build needs to not render certain models from the exclude list, so just don't add those models to any lists
	if ( EngineModelClientFlags( pModel ) & ENGINE_MODEL_CLIENT_MODELFLAG_RENDER_DISABLED )
		return 0;	// Such models don't render anywhere and don't need stencil

	RenderableLightingModel_t nLightingModel = LIGHTING_MODEL_NONE;
	uint bWantsStencil = 0;
	ShaderStencilState_t tempStencil;
	if ( data.m_pModelRenderable )
	{
		data.m_pModelRenderable->GetRenderData( &nLightingModel, MODEL_DATA_LIGHTING_MODEL );
		if ( renderMode == MODEL_RENDER_MODE_NORMAL )
		{
			// I considered making a MODEL_DATA_STENCIL_ENABLE renderdata type that would only return a bool
			// if stencil was enabled, but it turns out most of the work for MODEL_DATA_STENCIL is computing
			// that bool, so pulling this out into a separate piece didn't turn out to be a perf win.
			bWantsStencil = data.m_pModelRenderable->GetRenderData( &tempStencil, MODEL_DATA_STENCIL ) ? 1 : 0;
		}
	}
	else
	{
		ExecuteOnce( [&]{ DevWarning( "data.m_pModelRenderable is NULL for %s\n", modelinfo->GetModelName( pModel ) ); } );
	}

	int j;
	for ( j = 0; j < nModelTypeCount; ++j )
	{
		if ( pModelList[j].m_pModel == pModel &&
			 pModelList[j].m_nLightingModel == nLightingModel &&
			 pModelList[j].m_bWantsStencil == bWantsStencil )
			break;
	}

	if ( j == nModelTypeCount )
	{
		// Bail if we're rendering into shadow depth map and this model doesn't cast shadows
		// NOTE: if m_pModelRenderable is NULL, it's a dependent bone setup so we need to keep it
		studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pModel );
		if ( ( renderMode != MODEL_RENDER_MODE_NORMAL ) && data.m_pModelRenderable && ( ( pStudioHdr->flags & STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS ) != 0 ) )
		{
			return bWantsStencil;
		}

		MDLHandle_t hMDL = modelinfo->GetCacheHandle( pModel );
		studiohwdata_t *pHardwareData = g_pMDLCache->GetHardwareData( hMDL );

		// This can occur if there was an error loading the model; for instance
		// if the vtx and mdl are out of sync.
		if ( !pHardwareData || !pHardwareData->m_pLODs )
			return bWantsStencil;
				  
		ModelListByType_t &list = pModelList[ nModelTypeCount ]; 
		list.m_pModel = pModel;
		list.m_nLightingModel = nLightingModel;
		list.m_bWantsStencil = bWantsStencil;
		list.m_pStudioHdr = pStudioHdr;
		list.m_pHardwareData = pHardwareData;
		list.m_nFlashlightCount = 0;
		list.m_pFlashlights = nullptr;
		list.m_nCount = 0;
		list.m_pFirstNode = 0;
		list.m_pRenderModels = 0;
		list.m_nParentDepth = 0;
		list.m_pNextLightingModel = nullptr;
		j = nModelTypeCount++;
	}

	CBaseEntity *pEntity = EntityCast<CBaseEntity>( data.m_pRenderable->GetIClientUnknown()->GetBaseEntity() );
	uint nParentDepth = ComputeParentDepth( pEntity );

	ModelListByType_t &list = pModelList[ j ]; 
	ModelListNode_t &node = pModelListNodes[ nModelNodeCount++ ];
	node.m_Entry = data;
	node.m_nInitialListIndex = nInitialListIndex;
	node.m_bBoneMerge = pEntity && pEntity->IsEffectActive( EF_BONEMERGE );

	if ( bWantsStencil && ( renderMode == MODEL_RENDER_MODE_NORMAL ) )
	{
		CMatRenderData< ShaderStencilState_t > rdStencil( m_pRenderContext, 1 );
		memcpy( &rdStencil[0], &tempStencil, sizeof( tempStencil ) );
		node.m_pStencilState = &rdStencil[0];
	}
	else
	{
		node.m_pStencilState = nullptr;
	}

	node.m_pNext = list.m_pFirstNode;
	list.m_nParentDepth = std::max( list.m_nParentDepth, nParentDepth );
	list.m_pFirstNode = &node;
	++list.m_nCount;
	nModelTypeCountInOut = nModelTypeCount;

	return bWantsStencil;
}

//-----------------------------------------------------------------------------
// bucket models by type, return # of unique types
//-----------------------------------------------------------------------------
int CModelRenderSystem::BucketModelsByMDL( ModelListByType_t *pModelList, ModelListNode_t *pModelListNodes, ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode,
										   int *pModelsRenderingStencilCountOut )
{
	int nModelTypeCount = 0;
	int nModelNodeCount = 0;
	int nModelWantingStencil = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		nModelWantingStencil += AddModelToLists( nModelTypeCount, pModelList, nModelNodeCount, pModelListNodes, i, pEntities[i], renderMode );
	}
	*pModelsRenderingStencilCountOut = nModelWantingStencil;
	return nModelTypeCount;
}

//-----------------------------------------------------------------------------
// Sort model types function
//-----------------------------------------------------------------------------
bool CModelRenderSystem::DependencySortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right )
{
	// Ensures bone setup occurs in the correct order
	if ( left.m_nParentDepth != right.m_nParentDepth )
		return left.m_nParentDepth < right.m_nParentDepth;

	// Ensure stenciling models are at the end of the list.
	// This doesn't guarantee that stencil stuff is at the end because parent depth trumps it,
	// so we'll have to sort again before rendering.
	if ( left.m_bWantsStencil != right.m_bWantsStencil )
	{
		return left.m_bWantsStencil < right.m_bWantsStencil;
	}

	// Keep same models with different lighting types together
	return left.m_pModel < right.m_pModel;
}

//-----------------------------------------------------------------------------
// Sorts so that bone setup occurs in the appropriate order (parents set up first)
//-----------------------------------------------------------------------------
void CModelRenderSystem::SortBucketsByDependency( int nModelTypeCount, ModelListByType_t *pModelList, LightingList_t *pLightingList )
{
	std::sort( pModelList, pModelList + nModelTypeCount, DependencySortLessFunc ); 

	// Assign models to the appropriate lighting list
	for ( int i = nModelTypeCount; --i >= 0; )
	{
		ModelListByType_t &list = pModelList[ i ]; 

		// Hook into lighting list
		if ( list.m_nLightingModel == LIGHTING_MODEL_NONE )
			continue;

		LightingList_t &lightList = pLightingList[ list.m_nLightingModel ];
		list.m_pNextLightingModel = lightList.m_pFirstModel;
		lightList.m_pFirstModel = &list;
		++lightList.m_nCount;
		lightList.m_nTotalModelCount += list.m_nCount;
	}

}

//-----------------------------------------------------------------------------
// Slam model LODs to the appropriate level
//-----------------------------------------------------------------------------
void CModelRenderSystem::SlamModelLODs( int nLOD, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		int nLODCount = list.m_pHardwareData->m_NumLODs;
		int nRootLOD = list.m_pHardwareData->m_RootLOD;
		bool bHasShadowLOD = ( list.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) != 0;
		int nMaxLOD = bHasShadowLOD ? nLODCount - 2 : nLODCount - 1; 

		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
		}
	}
}

//-----------------------------------------------------------------------------
// Compute model LODs
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeModelLODs( int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode, ModelRenderMode_t renderMode )
{
	if ( renderMode == MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		// Slam to shadow lod
		//int nShadowLodConVar = r_shadowlod.GetInt();

		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			int nLODCount = list.m_pHardwareData->m_NumLODs;
			//int nRootLOD = list.m_pHardwareData->m_RootLOD;
			int nMaxLOD = nLODCount - 1; 

			for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
			{
				// Just always use the lowest LOD right now
				//int nLOD = nShadowLodConVar;
				//pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
				pNode->m_nLOD = nMaxLOD;
			}
		}
		return;
	}
		
	// Always slam r_lod to 0 for CS:GO.
	int nLOD = 0;

	if ( nLOD >= 0 )
	{
		SlamModelLODs( nLOD, nModelTypeCount, pModelList, pModelListNode );
		return;
	}

	ScreenSizeComputeInfo_t info;
	ComputeScreenSizeInfo( &info );

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		int nLODCount = list.m_pHardwareData->m_NumLODs;
		int nRootLOD = list.m_pHardwareData->m_RootLOD;
		int nMaxLOD = nLODCount - 1; 

		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			// FIXME: SIMD-ize, eliminate all extraneous calls (get view render state outside of loop)
			const Vector &vecRenderOrigin = pNode->m_Entry.m_pRenderable->GetRenderOrigin();

			// NOTE: The 2.0 is for legacy reasons
			float flScreenSize = 2.0f * ComputeScreenSize( vecRenderOrigin, 0.5f, info );

			float flMetric = list.m_pHardwareData->LODMetric( flScreenSize );
			nLOD = list.m_pHardwareData->GetLODForMetric( flMetric );
			pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
		}
	}
}

//-----------------------------------------------------------------------------
// Sort models function
//-----------------------------------------------------------------------------
bool CModelRenderSystem::SortLessFunc( const RenderModelInfo_t &left, const RenderModelInfo_t &right )
{
	// NOTE: Could do this, but it is not faster, because the cost of an integer multiply is about three
	// times that of a branch penalty:
	// int nLeft = left.m_nSkin * 1000000 + left.m_nLOD * 1000 + left.m_nBody;
	// int nRight = right.m_nSkin * 1000000 + right.m_nLOD * 1000 + right.m_nBody;
	// return nLeft > nRight;
	if ( left.m_bSetupBonesOnly != right.m_bSetupBonesOnly )
		return !left.m_bSetupBonesOnly;
	if ( left.m_nSkin != right.m_nSkin )
		return left.m_nSkin > right.m_nSkin;
	if ( left.m_nLOD != right.m_nLOD )
		return left.m_nLOD > right.m_nLOD;
	return left.m_nBody > right.m_nBody;
}

//-----------------------------------------------------------------------------
// Sort models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SortModels( RenderModelInfo_t *pRenderModelInfo, int nListTotal,
	int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode )
{
	// First place them in arrays
	memset( pRenderModelInfo, 0, sizeof(RenderModelInfo_t) * nListTotal );
	RenderModelInfo_t *pCurrInfo = pRenderModelInfo;
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		list.m_pRenderModels = pCurrInfo;
		list.m_nSetupBoneCount = 0;
		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			pCurrInfo->m_Entry = pNode->m_Entry;
			pCurrInfo->m_nLOD = pNode->m_nLOD;
			pCurrInfo->m_nSkin = pNode->m_Entry.m_pRenderable->GetSkin();
			pCurrInfo->m_nBody = pNode->m_Entry.m_pRenderable->GetBody();
			pCurrInfo->m_hInstance = pNode->m_Entry.m_pRenderable->GetModelInstance();
			pCurrInfo->m_Decals = STUDIORENDER_DECAL_INVALID;
			pCurrInfo->m_nInitialListIndex = pNode->m_nInitialListIndex;
			pCurrInfo->m_bBoneMerge = pNode->m_bBoneMerge;
			pCurrInfo->m_bSetupBonesOnly = ( pNode->m_Entry.m_pModelRenderable == nullptr );
			pCurrInfo->m_pStencilState = pNode->m_pStencilState;
			list.m_nSetupBoneCount += pCurrInfo->m_bSetupBonesOnly;
			++pCurrInfo;
		}

		// Sort within this model type. skin first, then LOD, then body.
		contract_assert( pCurrInfo - list.m_pRenderModels == list.m_nCount );
		std::sort( list.m_pRenderModels, list.m_pRenderModels + list.m_nCount, SortLessFunc ); 

		list.m_nCount -= list.m_nSetupBoneCount;
		list.m_nSetupBoneCount += list.m_nCount;
	}
}

//-----------------------------------------------------------------------------
// Sets up bones on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupBones( int nModelTypeCount, ModelListByType_t *pModelList )
{
	// FIXME: Can we make parallel bone setup faster? Yes, we can!
	const float flCurTime = gpGlobals->curtime;
	matrix3x4a_t pPoseToBone[MAXSTUDIOBONES];

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		const int nBoneCount = list.m_pStudioHdr->numbones;

		// Force setup of attachments if we're going to use an illumposition
		const int nAttachmentMask = ( list.m_pStudioHdr->IllumPositionAttachmentIndex() > 0 ) ? BONE_USED_BY_ATTACHMENT : 0;
		for ( int j = 0; j < list.m_nSetupBoneCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			const int nBoneMask = BONE_USED_BY_VERTEX_AT_LOD( pModel->m_nLOD ) | nAttachmentMask;
			pModel->m_pBoneToWorld = (matrix3x4a_t*)m_BoneToWorld.Alloc( nBoneCount * sizeof(matrix3x4a_t) );
			const bool bOk = pModel->m_Entry.m_pRenderable->SetupBones( pModel->m_pBoneToWorld, nBoneCount, nBoneMask, flCurTime );
			if ( !bOk )
			{
				for ( int k = 0; k < nBoneCount; ++k)
				{
					SetIdentityMatrix( pModel->m_pBoneToWorld[k] );
				}
			}
		}

		if ( list.m_nCount == 0 )
			continue;

		// Get the pose to bone for the model
		if ( !list.m_pStudioHdr->pLinearBones() )
		{
			// convert bone to world transformations into pose to world transformations
			for (int k = 0; k < nBoneCount; k++)
			{
				const mstudiobone_t *pCurBone = list.m_pStudioHdr->pBone( k );
				MatrixCopy( pCurBone->poseToBone, pPoseToBone[k] );
			}
		}
		else
		{
			mstudiolinearbone_t *pLinearBones = list.m_pStudioHdr->pLinearBones();

			// convert bone to world transformations into pose to world transformations
			for ( int k = 0; k < nBoneCount; k++)
			{
				MatrixCopy( pLinearBones->poseToBone(k), pPoseToBone[k] );
			}
		}

		// Apply the pose-to-bone matrix to all instances
		// NOTE: We should be able to optimize this a ton since it's very parallelizable
		// NOTE: We may well want to compute the aggregate bone to world here also.
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			CMatRenderData< matrix3x4a_t > rdPoseToWorld( m_pRenderContext, nBoneCount );
			pModel->m_pPoseToWorld = rdPoseToWorld.Base();

			for ( int b = 0; b < nBoneCount; b++ )
			{
				ConcatTransforms_Aligned( pModel->m_pBoneToWorld[b], pPoseToBone[b], pModel->m_pPoseToWorld[b] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Sets up flexes on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupFlexes( int nModelTypeCount, ModelListByType_t *pModelList )
{
	bool bUsesDelayedWeights = g_CV_FlexSmooth.GetBool();

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		const int nFlexCount = list.m_pStudioHdr->numflexdesc;
		if ( !nFlexCount )
			continue;

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			CMatRenderData< float > rdFlexWeights( m_pRenderContext );
			CMatRenderData< float > rdDelayedFlexWeights( m_pRenderContext );
			pModel->m_pFlexWeights = rdFlexWeights.Lock( nFlexCount );
			if ( bUsesDelayedWeights )
			{
				pModel->m_pDelayedFlexWeights = rdDelayedFlexWeights.Lock( nFlexCount );
			}
			pModel->m_Entry.m_pRenderable->SetupWeights( pModel->m_pBoneToWorld, nFlexCount, pModel->m_pFlexWeights, pModel->m_pDelayedFlexWeights );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws debugging information for lighting
//-----------------------------------------------------------------------------
void CModelRenderSystem::DebugDrawLightingOrigin( const ModelListByType_t &list, const RenderModelInfo_t &model )
{
	if ( !model.m_pLightingState )
		return;

	const Vector& lightOrigin = model.m_pLightingState->m_vecLightingOrigin;
	const matrix3x4_t &modelToWorld = model.m_Entry.m_pRenderable->RenderableToWorldTransform();

	// draw z planar cross at lighting origin
	Vector pt0;
	Vector pt1;
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.x -= 4;
	pt1.x += 4;
	debugoverlay->AddLineOverlay( pt0, pt1, 0, 255, 0, true, 0.0f );
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.y -= 4;
	pt1.y += 4;
	debugoverlay->AddLineOverlay( pt0, pt1, 0, 255, 0, true, 0.0f );

	// draw lines from the light origin to the hull boundaries to identify model
	Vector pt;
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );

	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );	
}

//-----------------------------------------------------------------------------
// Compute lighting origin on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeLightingOrigin( ModelListByType_t &list, LightingQuery_t *pLightingQueryBase, int nQueryStride )
{
	LightingQuery_t *pLightingQuery = pLightingQueryBase;

	int nAttachmentIndex = list.m_pStudioHdr->IllumPositionAttachmentIndex();
	bool bAmbientBoost = ( list.m_pStudioHdr->flags & STUDIOHDR_FLAGS_AMBIENT_BOOST ) != 0;
	const Vector &vecIllumPosition = list.m_pStudioHdr->illumposition;

	// ($TODO): We may want to pull the functionality of ComputeLightingOrigin inlined into this loop to prevent the virtual function call overhead and refactor to reduce the conditionals.
	for ( int j = 0; j < list.m_nCount; ++j, pLightingQuery = (LightingQuery_t*)( (unsigned char*)pLightingQuery + nQueryStride ) )
	{
		RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
		const matrix3x4_t &renderToWorld = pModel->m_Entry.m_pRenderable->RenderableToWorldTransform();
		
		CBaseEntity *pEnt = EntityCast<CBaseEntity>( pModel->m_Entry.m_pRenderable->GetIClientUnknown()->GetBaseEntity() );
		pEnt->ComputeLightingOrigin( nAttachmentIndex, vecIllumPosition, renderToWorld, pLightingQuery->m_LightingOrigin );

		//VectorTransform( vecIllumPosition, renderToWorld, pLightingQuery->m_LightingOrigin );
		pLightingQuery->m_InstanceHandle = pModel->m_hInstance;
		pLightingQuery->m_bAmbientBoost = bAmbientBoost;
	}

}

//-----------------------------------------------------------------------------
// Builds the model lighting list
//-----------------------------------------------------------------------------
enum
{
	LIGHTING_USES_ENV_CUBEMAP = 0x1,
	LIGHTING_IS_VERTEX_LIT = 0x2,
	LIGHTING_IS_STATIC_LIT = 0x4,
};

int CModelRenderSystem::BuildLightingList( ModelListByType_t **ppLists, unsigned char *pFlags, int *pTotalModels, const LightingList_t &lightingList )
{
	// FIXME: This may be better placed in the engine to avoid all the virtual calls?
	int nSetupCount = 0;
	*pTotalModels = 0;
	for ( ModelListByType_t* pList = lightingList.m_pFirstModel; pList; pList = pList->m_pNextLightingModel )
	{
		// FIXME: Under what conditions can the static prop skip lighting? [unlit materials]
		bool bIsLit = modelinfo->IsModelVertexLit( pList->m_pModel );
		bool bUsesEnvCubemap = modelinfo->UsesEnvCubemap( pList->m_pModel );
		bool bIsStaticLit = modelinfo->UsesStaticLighting( pList->m_pModel );
		if ( !bIsLit && !bUsesEnvCubemap && !bIsStaticLit )
			continue;
		ppLists[ nSetupCount ] = pList;
		pFlags[ nSetupCount ] = ( bIsStaticLit << 2 ) | ( bIsLit << 1 ) | ( bUsesEnvCubemap << 0 );
		*pTotalModels += pList->m_nCount;
		++nSetupCount;
	}

	return nSetupCount;
}

//-----------------------------------------------------------------------------
// Hook up computed lighting state
//-----------------------------------------------------------------------------
void CModelRenderSystem::HookUpStaticLightingState( int nCount, ModelListByType_t **ppLists, 
	unsigned char *pFlags, ITexture **ppEnvCubemap, MaterialLightingState_t *pLightingState,
	MaterialLightingState_t *pDecalLightingState, ColorMeshInfo_t **ppColorMeshInfo )
{
	// FIXME: This has got to be more efficient that this
	for ( int i = 0; i < nCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		if ( pFlags[i] & LIGHTING_USES_ENV_CUBEMAP )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pEnvCubemapTexture = ppEnvCubemap[j] ? ppEnvCubemap[j] : m_DefaultCubemap;
			}
		}

		if ( pFlags[i] & LIGHTING_IS_VERTEX_LIT )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pLightingState = &pLightingState[j];
				pModel->m_pDecalLightingState = &pDecalLightingState[j];
			}
		}

		if ( pFlags[i] & LIGHTING_IS_STATIC_LIT )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pColorMeshInfo = ppColorMeshInfo[j];
			}
		}

		ppEnvCubemap += list.m_nCount;
		pLightingState += list.m_nCount;
		pDecalLightingState += list.m_nCount;
		ppColorMeshInfo += list.m_nCount;
	}
}

//-----------------------------------------------------------------------------
// Sets up lighting on all models
//-----------------------------------------------------------------------------
int CModelRenderSystem::SetupStaticPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandle )
{
	ks::ScratchArena<> scratch;
	if ( lightingList.m_nCount == 0 )
		return 0;

	// Build list of everything that needs lighting
	int nTotalModels;
	ModelListByType_t **ppLists = scratch.Alloc<ModelListByType_t*>( lightingList.m_nCount );
	unsigned char *pFlags = scratch.Alloc<unsigned char>( lightingList.m_nCount );
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return 0;

	// Build queries used to compute lighting
	StaticLightingQuery_t *pLightingQuery = scratch.Alloc<StaticLightingQuery_t>( nTotalModels );
	int nOffset = 0;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );

		for ( int j = 0; j < list.m_nCount; ++j, ++nOffset )
		{
			pLightingQuery[ nOffset ].m_pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			pLightingQuery[ nOffset ].m_InstanceHandle = list.m_pRenderModels[j].m_hInstance;
			pLightingQuery[ nOffset ].m_bAmbientBoost = false;
		}
	}

	// Compute lighting origins
	staticpropmgr->GetLightingOrigins( &pLightingQuery[0].m_LightingOrigin, 
		sizeof(StaticLightingQuery_t), nTotalModels, &pLightingQuery[0].m_pRenderable, sizeof(StaticLightingQuery_t) );
			   
	// Does all lighting computations for all models
	ColorMeshInfo_t **ppColorMeshInfo = scratch.Alloc<ColorMeshInfo_t*>( nTotalModels );
	ITexture **ppEnvCubemap = scratch.Alloc<ITexture*>( nTotalModels );

	CMatRenderData< MaterialLightingState_t > rdLightingState( m_pRenderContext, 2 * nTotalModels );
	// bring this into cache and clear it, we're going to write to most of it anyway
	memset( rdLightingState.Base(), 0, sizeof(MaterialLightingState_t) * 2 * nTotalModels );

	MaterialLightingState_t *pLightingState = rdLightingState.Base();
	MaterialLightingState_t *pDecalLightingState = &rdLightingState[ nTotalModels ];
	modelrender->ComputeStaticLightingState( nTotalModels, pLightingQuery, pLightingState, pDecalLightingState, ppColorMeshInfo, ppEnvCubemap, pColorMeshHandle );

	// Hook up pointers
	HookUpStaticLightingState( nSetupCount, ppLists, pFlags, ppEnvCubemap, pLightingState, pDecalLightingState, ppColorMeshInfo );

	return nTotalModels;
}

void CModelRenderSystem::SetupStandardLighting( LightingList_t &lightingList )
{
	ks::ScratchArena<> scratch;
	if ( lightingList.m_nCount == 0 )
		return;

	// Determine which groups need lighting
	ModelListByType_t **ppLists = scratch.Alloc<ModelListByType_t*>( lightingList.m_nCount );
	unsigned char *pFlags = scratch.Alloc<unsigned char>( lightingList.m_nCount );
	int nTotalModels = 0;
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return;

	// Compute data necessary for lighting computations
	int nOffset = 0;
	LightingQuery_t *pLightingQuery = scratch.Alloc<LightingQuery_t>( nTotalModels );
	CMatRenderData<MaterialLightingState_t> rdLightingState( m_pRenderContext, nTotalModels );
	MaterialLightingState_t *pLightingState = rdLightingState.Base();

	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		ComputeLightingOrigin( list, &pLightingQuery[nOffset], sizeof(LightingQuery_t) );
		nOffset += list.m_nCount;
	}

	memset( pLightingState, 0, nTotalModels * sizeof(MaterialLightingState_t) );

	// Does all lighting computations for all models
	ITexture **ppEnvCubemap = scratch.Alloc<ITexture*>( nTotalModels );
	modelrender->ComputeLightingState( nTotalModels, pLightingQuery, pLightingState, ppEnvCubemap );

	// Hook up pointers
	MaterialLightingState_t *pCurrState = pLightingState;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		if ( pFlags[i] & 0x1 )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pEnvCubemapTexture = ppEnvCubemap[j] ? ppEnvCubemap[j] : m_DefaultCubemap;
			}
		}

		if ( pFlags[i] & 0x2 )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pLightingState = &pCurrState[j];
			}
		}

		ppEnvCubemap += list.m_nCount;
		pCurrState += list.m_nCount;
	}
}

int CModelRenderSystem::SetupPhysicsPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandle )
{
	ks::ScratchArena<> scratch;
	if ( lightingList.m_nCount == 0 )
		return 0;

	// NOTE: Physics prop lighting is the same as static prop lighting, only
	// the static lighting is *always* used (the system goes to the standard path
	// for physics props which are moving or which use bumpmapping).
	ModelListByType_t **ppLists = scratch.Alloc<ModelListByType_t*>( lightingList.m_nCount );
	unsigned char *pFlags = scratch.Alloc<unsigned char>( lightingList.m_nCount );
	int nTotalModels = 0;
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return 0;

	StaticLightingQuery_t *pLightingQuery = scratch.Alloc<StaticLightingQuery_t>( nTotalModels );
	int nOffset = 0;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );

		ComputeLightingOrigin( list, &pLightingQuery[nOffset], sizeof(StaticLightingQuery_t) );
		for ( int j = 0; j < list.m_nCount; ++j, ++nOffset )
		{
			pLightingQuery[ nOffset ].m_pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
		}
	}

	// Does all lighting computations for all models
	ColorMeshInfo_t **ppColorMeshInfo = scratch.Alloc<ColorMeshInfo_t*>( nTotalModels );
	ITexture **ppEnvCubemap = scratch.Alloc<ITexture*>( nTotalModels );
	CMatRenderData< MaterialLightingState_t > rdLightingState( m_pRenderContext, 2 * nTotalModels );
	MaterialLightingState_t *pLightingState = rdLightingState.Base();
	MaterialLightingState_t *pDecalLightingState = &pLightingState[ nTotalModels ];
	modelrender->ComputeStaticLightingState( nTotalModels, pLightingQuery, pLightingState, pDecalLightingState, ppColorMeshInfo, ppEnvCubemap, pColorMeshHandle );

	// Hook up pointers
	HookUpStaticLightingState( nSetupCount, ppLists, pFlags, ppEnvCubemap, pLightingState, pDecalLightingState, ppColorMeshInfo );
	return nTotalModels;
}

int CModelRenderSystem::SetupLighting( LightingList_t *pLightingList, int nModelTypeCount, ModelListByType_t *pModelList, DataCacheHandle_t *pColorMeshHandles, ModelRenderMode_t renderMode )
{
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
	{
		return 0;
	}

	int nCount = SetupStaticPropLighting( pLightingList[ LIGHTING_MODEL_STATIC_PROP ], pColorMeshHandles );
	pColorMeshHandles += nCount;
	SetupStandardLighting( pLightingList[ LIGHTING_MODEL_STANDARD ] );
	nCount += SetupPhysicsPropLighting( pLightingList[ LIGHTING_MODEL_PHYSICS_PROP ], pColorMeshHandles );

	// Debugging info
	if ( r_drawmodellightorigin.GetBool() )
	{
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			const ModelListByType_t &list = pModelList[ i ];
			if ( list.m_nLightingModel == LIGHTING_MODEL_NONE )
				continue;
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				const RenderModelInfo_t &info = list.m_pRenderModels[j];
				DebugDrawLightingOrigin( list, info );
			}
		}
	}

	return nCount;
}

//-----------------------------------------------------------------------------
// Setup render state related to flashlights and decals
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupFlashlightsAndDecals( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, RenderModelInfo_t *pRenderModels, ModelRenderMode_t renderMode )
{
	ks::ScratchArena<> scratch;
	// Skip lighting + decals if we don't need it
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
		return;

	ShadowHandle_t pFlashlights[MAX_FLASHLIGHTS_PER_INSTANCE_DRAW_CALL];
	int nInstCount = 0;
	ModelInstanceHandle_t *pModelInstanceHandle = scratch.Alloc<ModelInstanceHandle_t>( nTotalModelCount );
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			pModelInstanceHandle[nInstCount++] = pModel->m_hInstance;
		}		
	}
	
	if ( nTotalModelCount != nInstCount )
	{
		// FIXME: custom player model scaffolds have no geometry (only bones) and when used will always trip this warning - this system needs to account for models that don't 'render' in the traditional sense.
		// AssertMsgOnce( false, "Instance count does not match model count." );
		nTotalModelCount = nInstCount;
	}

	// Gets all decals
	StudioDecalHandle_t *pDecals = &pRenderModels->m_Decals;
	modelrender->GetModelDecalHandles( pDecals, sizeof(RenderModelInfo_t), nTotalModelCount, pModelInstanceHandle );

	// Builds a list of all flashlights affecting this model
	uint32 *pFlashlightUsage = &pRenderModels->m_nFlashlightUsage;
	pInfo->m_nFlashlightCount = shadowmgr->SetupFlashlightRenderInstanceInfo( pFlashlights, pFlashlightUsage, sizeof(RenderModelInfo_t), nTotalModelCount, pModelInstanceHandle );
	if ( pInfo->m_nFlashlightCount )
	{
		// Copy over the flashlight state
		// FIXME: Should we do this over the entire list of all instances?
		// There's going to be a fair amount of copying of flashlight_ts
		CMatRenderData< FlashlightInstance_t > rdFlashlights( m_pRenderContext, pInfo->m_nFlashlightCount );
		pInfo->m_pFlashlights = rdFlashlights.Base();
		shadowmgr->GetFlashlightRenderInfo( pInfo->m_pFlashlights, pInfo->m_nFlashlightCount, pFlashlights );
	}
	else
	{
		pInfo->m_pFlashlights = nullptr;
	}

	// FIXME: Hack!
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		list.m_nFlashlightCount = pInfo->m_nFlashlightCount;
		list.m_pFlashlights = pInfo->m_pFlashlights;
	}
}

void CModelRenderSystem::SetupPerInstanceColorModulation( int nModelTypeCount, ModelListByType_t *pModelList )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		if ( !list.m_nCount )
			continue;

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			IClientRenderable *pRenderable = pModel->m_Entry.m_pRenderable;
			pRenderable->GetColorModulation( pModel->m_DiffuseModulation.AsVector3D().Base() );
			pModel->m_DiffuseModulation.w = pModel->m_Entry.m_InstanceData.m_nAlpha * ( 1.0f / 255.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Call into studiorender
//-----------------------------------------------------------------------------
ConVar cl_colorfastpath( side_registry(), "cl_colorfastpath", "0" );
void CModelRenderSystem::RenderModels( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, ModelRenderMode_t renderMode, bool bShadowDepthIncludeTranslucentMaterials )
{
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		bool bColorize = cl_colorfastpath.GetBool();
		if ( bColorize )
		{
			g_pStudioRender->ForcedMaterialOverride( m_DebugMaterial );
		}

		const int nFlags = STUDIORENDER_DRAW_OPAQUE_ONLY;
		CMatRenderData< StudioArrayData_t > rdArray( m_pRenderContext, nModelTypeCount );

		int nNonStencilModelTypeCount = 0;
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			rdArray[ i ].m_pStudioHdr = list.m_pStudioHdr;
			rdArray[ i ].m_pHardwareData = list.m_pHardwareData;
			rdArray[ i ].m_pInstanceData = list.m_pRenderModels;
			rdArray[ i ].m_nCount = list.m_nCount;
			nNonStencilModelTypeCount += list.m_bWantsStencil ? 0 : 1;

		}

		// PC renders all models in one go regardless of stencil state
		// PS/3 renders all models in one go because models have a lot of vertices and few pixels (as of Portal2). So we end Z Pass earlier, before we RenderModels()
		g_pStudioRender->DrawModelArray( *pInfo, nModelTypeCount, rdArray.Base(), sizeof(RenderModelInfo_t), nFlags );
		g_pStudioRender->ForcedMaterialOverride( nullptr );
	}
	else if ( renderMode == MODEL_RENDER_MODE_SHADOW_DEPTH )
	{
		// NOTE: Use this path because we can aggregate draw calls across mdls

		int nFlags = STUDIORENDER_SHADOWDEPTHTEXTURE;
		if ( bShadowDepthIncludeTranslucentMaterials )
			nFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE_INCLUDE_TRANSLUCENT_MATERIALS;
		else
			nFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
		CMatRenderData< StudioArrayData_t > rdShadow( m_pRenderContext, nModelTypeCount );
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			rdShadow[ i ].m_pStudioHdr = list.m_pStudioHdr;
			rdShadow[ i ].m_pHardwareData = list.m_pHardwareData;
			rdShadow[ i ].m_pInstanceData = list.m_pRenderModels;
			rdShadow[ i ].m_nCount = list.m_nCount;
		}
		g_pStudioRender->DrawModelShadowArray( nModelTypeCount, rdShadow.Base(), sizeof(RenderModelInfo_t), nFlags );
	}
	else if ( renderMode == MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		// shouldn't get here unless the code is ported from l4d2 to drive this properly.
		contract_assert( 0 );
	}
}

//-----------------------------------------------------------------------------
// Call into studiorender
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupTranslucentData( int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, TranslucentInstanceRenderData_t *pRenderData )
{
	memset( pRenderData, 0, nTotalModelCount * sizeof( TranslucentInstanceRenderData_t ) );
	CMatRenderData< StudioModelArrayInfo_t > arrayInfo( m_pRenderContext, nModelTypeCount );
	CMatRenderData< StudioArrayInstanceData_t > instanceData( m_pRenderContext, nTotalModelCount );

	int nCurInstance = 0;
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		StudioModelArrayInfo_t *pModelInfo = &arrayInfo[i];
		memcpy( pModelInfo, &list, sizeof( StudioModelArrayInfo_t ) );

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t &info = list.m_pRenderModels[j];

			StudioArrayInstanceData_t *pInstanceData = &instanceData[nCurInstance++];
			memcpy( pInstanceData, &info, sizeof( StudioArrayInstanceData_t ) );

			TranslucentInstanceRenderData_t &data = pRenderData[ info.m_nInitialListIndex ];
			data.m_pModelInfo = pModelInfo;
			data.m_pInstanceData = pInstanceData;
		}
	}
}

//-----------------------------------------------------------------------------
// Renders debug overlays
//-----------------------------------------------------------------------------
void CModelRenderSystem::RenderVCollideDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList )
{
	if ( !vcollide_wireframe.GetBool() )
		return;

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			IClientRenderable *pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			CBaseAnimating *pAnim = dynamic_cast< CBaseAnimating * >( pRenderable );
			if ( pAnim && pAnim->IsRagdoll() )
			{
				pAnim->m_pRagdoll->DrawWireframe();
				continue;
			}
			
			ICollideable *pCollideable = pRenderable->GetIClientUnknown()->GetCollideable();
			if ( pCollideable && ( pCollideable->GetSolid() == SOLID_VPHYSICS ) &&
				IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
			{
				vcollide_t *pCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
				if ( pCollide && pCollide->solidCount == 1 )
				{
					static color32 debugColor = {0,255,255,0};
					engine->DebugDrawPhysCollide( pCollide->solids[0], nullptr, pCollideable->CollisionToWorldTransform(), debugColor );

					CBaseEntity *pEntity = EntityCast<CBaseEntity>( pRenderable->GetIClientUnknown()->GetBaseEntity() );
					if ( pEntity && pEntity->VPhysicsGetObject() )
					{
						static color32 debugColorPhys = {255,0,0,0};
						matrix3x4_t matrix;
						pEntity->VPhysicsGetObject()->GetPositionMatrix( &matrix );
						engine->DebugDrawPhysCollide( pCollide->solids[0], nullptr, matrix, debugColorPhys );
					}
				}
				continue;
			}
			else if ( pCollideable && vcollide_wireframe.GetInt() > 1 )
			{
				vcollide_t *pCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
				if ( pCollide && pCollide->solidCount == 1 )
				{
					static color32 debugColor = {0,255,0,0};
					engine->DebugDrawPhysCollide( pCollide->solids[0], nullptr, pAnim->RenderableToWorldTransform(), debugColor );
				}
				continue;
			}
		}
	}
}

void CModelRenderSystem::RenderBBoxDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			IClientRenderable *pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			if ( !pRenderable->GetIClientUnknown() )
				continue;
			CBaseEntity *pEntity = EntityCast<CBaseEntity>( pRenderable->GetIClientUnknown()->GetBaseEntity() );
			if ( !pEntity )
				continue;

			pEntity->DrawBBoxVisualizations();
		}
	}
}

//-----------------------------------------------------------------------------
// Renders debug overlays
//-----------------------------------------------------------------------------
void CModelRenderSystem::RenderDebugOverlays( int nModelTypeCount, ModelListByType_t *pModelList, ModelRenderMode_t renderMode )
{
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
	{
		return;
	}

	RenderVCollideDebugOverlay( nModelTypeCount, pModelList );
	RenderBBoxDebugOverlay( nModelTypeCount, pModelList );
}

//-----------------------------------------------------------------------------
// Sort model types function
//-----------------------------------------------------------------------------
bool CModelRenderSystem::StencilSortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right )
{
	// Ensure stenciling models are at the end of the list
	if ( left.m_bWantsStencil != right.m_bWantsStencil )
	{
		return left.m_bWantsStencil < right.m_bWantsStencil;
	}

	// Keep same models with different lighting types together
	return left.m_pModel < right.m_pModel;
}

//-----------------------------------------------------------------------------
// Draw models
//-----------------------------------------------------------------------------
static ConVar cl_skipfastpath( side_registry(), "cl_skipfastpath", "0", FCVAR_CHEAT, "Set to 1 to stop all models that go through the model fast path from rendering" );
void CModelRenderSystem::DrawModels( ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode, bool bShadowDepthIncludeTranslucentMaterials )
{
	ks::ScratchArena<> scratch;
	if ( nCount == 0 || cl_skipfastpath.GetInt() )
		return;

	CVProfScope _( "CModelRenderSystem::DrawModels", 0, VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING, false, BUDGETFLAG_OTHER );
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	// While doing this, we need materialsystem to keep around its temp allocations
	// which we use for bone matrices + flexes
	CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
	m_pRenderContext = matRenderContext;

	( m_pRenderContext, "CModelRenderSystem::DrawModels (FASTPATH)" );

	CMatRenderDataReference rdLock( m_pRenderContext );

	// FIXME: This is infected-specific for perf test reasons.
	// Will break into a more fixed pipeline at a later date
	DataCacheHandle_t *pColorMeshHandles = nullptr;
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		pColorMeshHandles = scratch.Alloc<DataCacheHandle_t>( nCount );
	}
	ModelListByType_t *pModelList = scratch.Alloc<ModelListByType_t>( nCount );
	ModelListNode_t *pModelListNode = scratch.Alloc<ModelListNode_t>( nCount );
	int nModelsRenderingStencilCount = 0;
	int nModelTypeCount = BucketModelsByMDL( pModelList, pModelListNode, pEntities, nCount, renderMode, &nModelsRenderingStencilCount );

	LightingList_t pLightingList[ LIGHTING_MODEL_COUNT ];
	memset( pLightingList, 0, LIGHTING_MODEL_COUNT * sizeof(LightingList_t) );
	SortBucketsByDependency( nModelTypeCount, pModelList, pLightingList );

	// Compute LODs for each model
	ComputeModelLODs( nModelTypeCount, pModelList, pModelListNode, renderMode );

	// Sort processing list by body, lod, skin, etc.
	CMatRenderData< RenderModelInfo_t > rdRenderModelInfo( m_pRenderContext, nCount );
	RenderModelInfo_t *pSortedModelListNode = rdRenderModelInfo.Base();	
	SortModels( pSortedModelListNode, nCount, nModelTypeCount, pModelList, pModelListNode );

	// Setup bones
	SetupBones( nModelTypeCount, pModelList );

	// Setup flexes
	if ( renderMode != MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		SetupFlexes( nModelTypeCount, pModelList );
	}

	// Setup lighting
	int nColorMeshHandles = SetupLighting( pLightingList, nModelTypeCount, pModelList, pColorMeshHandles, renderMode );

	// Setup flashlights + decals
	StudioModelArrayInfo2_t info;
	SetupFlashlightsAndDecals( &info, nModelTypeCount, pModelList, nCount, pSortedModelListNode, renderMode );

	// Setup per-instance color modulation
	SetupPerInstanceColorModulation( nModelTypeCount, pModelList );

	// Setup per-instance wound data
	//SetupInfectedWoundRenderData( nModelTypeCount, pModelList, nCount, renderMode );

	// Draw models
	RenderModels( &info, nModelTypeCount, pModelList, nCount, renderMode, bShadowDepthIncludeTranslucentMaterials );

	rdLock.Release();
	
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		modelrender->CleanupStaticLightingState( nColorMeshHandles, pColorMeshHandles );
	}

	// Blat out temporary memory for bone-to-world transforms
	m_BoneToWorld.FreeAll( false );

	RenderDebugOverlays( nModelTypeCount, pModelList, renderMode );

	m_pRenderContext = nullptr;
}

//-----------------------------------------------------------------------------
// Computes per-instance data for fast path rendering
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData )
{
	ks::ScratchArena<> scratch;
	if ( nCount == 0 )
	{
		pTempData->m_nColorMeshHandleCount = 0;
		pTempData->m_bReleaseRenderData = false;
		return;
	}
						 
	CVProfScope _( "CModelRenderSystem::ComputeTranslucentRenderData", 0, VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING, false, BUDGETFLAG_OTHER );
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	// While doing this, we need materialsystem to keep around its temp allocations
	// which we use for bone matrices + flexes
	CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
	m_pRenderContext = matRenderContext;

	( m_pRenderContext, "CModelRenderSystem::ComputeTranslucentRenderData (FASTPATH)" );

	m_pRenderContext->AddRefRenderData();
	pTempData->m_bReleaseRenderData = true;

	ModelRenderMode_t renderMode = MODEL_RENDER_MODE_NORMAL;

	// FIXME: This is infected-specific for perf test reasons.
	// Will break into a more fixed pipeline at a later date
	DataCacheHandle_t *pColorMeshHandles = pTempData->m_pColorMeshHandles;
	ModelListByType_t *pModelList = scratch.Alloc<ModelListByType_t>( nCount );
	ModelListNode_t *pModelListNode = scratch.Alloc<ModelListNode_t>( nCount );
	int nModelsRenderingStencilCount = 0;
	int nModelTypeCount = BucketModelsByMDL( pModelList, pModelListNode, pModels, nCount, renderMode, &nModelsRenderingStencilCount );

	LightingList_t pLightingList[ LIGHTING_MODEL_COUNT ];
	memset( pLightingList, 0, LIGHTING_MODEL_COUNT * sizeof(LightingList_t) );
	SortBucketsByDependency( nModelTypeCount, pModelList, pLightingList );

	// Compute LODs for each model
	ComputeModelLODs( nModelTypeCount, pModelList, pModelListNode, renderMode );

	// Sort processing list by body, lod, skin, etc.
	RenderModelInfo_t *pSortedModelListNode = scratch.Alloc<RenderModelInfo_t>( nCount );
	SortModels( pSortedModelListNode, nCount, nModelTypeCount, pModelList, pModelListNode );

	// Setup bones
	SetupBones( nModelTypeCount, pModelList );

	// Setup flexes
	SetupFlexes( nModelTypeCount, pModelList );

	// Setup lighting
	pTempData->m_nColorMeshHandleCount = SetupLighting( pLightingList, nModelTypeCount, pModelList, pColorMeshHandles, renderMode );

	// Setup flashlights + decals
	StudioModelArrayInfo2_t info;
	SetupFlashlightsAndDecals( &info, nModelTypeCount, pModelList, nCount, pSortedModelListNode, renderMode );

	// Setup per-instance color modulation
	SetupPerInstanceColorModulation( nModelTypeCount, pModelList );

	// Setup per-instance wound data
	// SetupInfectedWoundRenderData( nModelTypeCount, pModelList, nCount, renderMode );

	// Draw models
	SetupTranslucentData( nModelTypeCount, pModelList, nCount, pRenderData );
	
	// Blat out temporary memory for bone-to-world transforms
	m_BoneToWorld.FreeAll( false );

	RenderDebugOverlays( nModelTypeCount, pModelList, renderMode );

	m_pRenderContext = nullptr;
}

void CModelRenderSystem::CleanupTranslucentTempData( TranslucentTempData_t *pTempData )
{
	if ( pTempData->m_bReleaseRenderData )
	{
		modelrender->CleanupStaticLightingState( pTempData->m_nColorMeshHandleCount, pTempData->m_pColorMeshHandles );
		CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
		matRenderContext->ReleaseRenderData();
	}
}

//-----------------------------------------------------------------------------
//
// Brush model rendering system starts here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Adds a model to the list of brush models to render
//-----------------------------------------------------------------------------
void CModelRenderSystem::AddBrushModelToList( int nInitialListIndex, ModelRenderSystemData_t &data, 
	ModelRenderMode_t renderMode, BrushArrayInstanceData_t &instance, matrix3x4a_t &brushToWorld )
{
	const model_t *pModel = data.m_pRenderable->GetModel();
	contract_assert( modelinfo->GetModelType( pModel ) == mod_brush );

	brushToWorld = data.m_pRenderable->RenderableToWorldTransform();
	instance.m_pBrushToWorld = &brushToWorld;
	instance.m_pBrushModel = pModel;
	instance.m_pStencilState = nullptr;

	uint bWantsStencil = 0;
	if ( data.m_pModelRenderable )
	{
		if ( renderMode == MODEL_RENDER_MODE_NORMAL )
		{
			// I considered making a MODEL_DATA_STENCIL_ENABLE renderdata type that would only return a bool
			// if stencil was enabled, but it turns out most of the work for MODEL_DATA_STENCIL is computing
			// that bool, so pulling this out into a separate piece didn't turn out to be a perf win.
			ShaderStencilState_t tempStencil;
			bWantsStencil = data.m_pModelRenderable->GetRenderData( &tempStencil, MODEL_DATA_STENCIL ) ? 1 : 0;
			if ( bWantsStencil )
			{
				CMatRenderData< ShaderStencilState_t > rdStencil( m_pRenderContext, 1 );
				memcpy( &rdStencil[0], &tempStencil, sizeof( tempStencil ) );
				instance.m_pStencilState = &rdStencil[0];

				// Not working yet...
				contract_assert( 0 );
			}
		}
	}
	else
	{
		ExecuteOnce( [&]{ DevWarning( "data.m_pModelRenderable is NULL for %s\n", modelinfo->GetModelName( pModel ) ); } );
	}
}

void CModelRenderSystem::SetupPerInstanceColorModulation( int nCount, ModelRenderSystemData_t *pModels, BrushArrayInstanceData_t *pInstanceData, ModelRenderMode_t renderMode )
{
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
		return;

	Vector vecColorModulation;
	for ( int i = 0; i < nCount; ++i )
	{
		IClientRenderable *pRenderable = pModels[i].m_pRenderable;
		pRenderable->GetColorModulation( pInstanceData[i].m_DiffuseModulation.AsVector3D().Base() );
		pInstanceData[i].m_DiffuseModulation.w = pModels[i].m_InstanceData.m_nAlpha * ( 1.0f / 255.0f );
	}
}

void CModelRenderSystem::DrawBrushModels( ModelRenderSystemData_t *pModels, int nCount, ModelRenderMode_t renderMode )
{
	if ( nCount == 0 || cl_skipfastpath.GetInt() )
		return;

	CVProfScope _( "CModelRenderSystem::DrawBrushModels", 0, VPROF_BUDGETGROUP_BRUSH_FAST_PATH_RENDERING, false, BUDGETFLAG_OTHER );

	// While doing this, we need materialsystem to keep around its temp allocations
	// which we use for bone matrices + flexes
	CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
	m_pRenderContext = matRenderContext;

	( m_pRenderContext, "CModelRenderSystem::DrawBrushModels (FASTPATH)" );

	CMatRenderDataReference rdLock( m_pRenderContext );

	CMatRenderData< BrushArrayInstanceData_t > rdInstances( m_pRenderContext, nCount );
	CMatRenderData< matrix3x4a_t > rdMatrices( m_pRenderContext, nCount );

	for ( int i = 0; i < nCount; ++i )
	{
		AddBrushModelToList( i, pModels[i], renderMode, rdInstances[i], rdMatrices[i] );
	}

	SetupPerInstanceColorModulation( nCount, pModels, rdInstances.Base(), renderMode );

	// Two pass is telling it to only do the opaque portion of the brush model
	int nFlags = STUDIO_RENDER | STUDIO_TWOPASS;
	switch( renderMode )
	{
	case MODEL_RENDER_MODE_NORMAL:
		break;

	case MODEL_RENDER_MODE_SHADOW_DEPTH:
		nFlags |= STUDIO_SHADOWDEPTHTEXTURE;
		break;

	case MODEL_RENDER_MODE_RTT_SHADOWS:
		nFlags |= STUDIO_SHADOWTEXTURE;
		break;
	};

	render->DrawBrushModelArray( m_pRenderContext, nCount, rdInstances.Base(), nFlags );

	rdLock.Release();

//	RenderDebugOverlays( nModelTypeCount, pModelList, renderMode );

	m_pRenderContext = nullptr;
}

class CBaseEntity;

using namespace ::ks::reflect;

extern CMoveData *g_pMoveData;

class CMoveHelperClient : public IMoveHelper
{
public:
					CMoveHelperClient( void );
	virtual			~CMoveHelperClient( void );

	char const*		GetName( EntityHandle_t handle ) const;

	// touch lists
	virtual void	ResetTouchList( void );
	virtual bool	AddToTouched( const trace_t& tr, const Vector& impactvelocity );
	virtual void	ProcessImpacts( void );

	// Numbered line printf
	virtual void	Con_NPrintf( int idx, char const* fmt, ... );
		
	virtual bool	PlayerFallingDamage(void);
	virtual void	PlayerSetAnimation( PLAYER_ANIM eAnim );

	// These have separate server vs client impementations
	virtual void	StartSound( const Vector& origin, int channel, char const* sample, float volume, soundlevel_t soundlevel, int fFlags, int pitch );
	virtual void	StartSound( const Vector& origin, const char *soundname ); 
	virtual void	PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	virtual IPhysicsSurfaceProps *GetSurfaceProps( void );

	virtual bool IsWorldEntity( const CBaseHandle &handle );

	void			SetHost( CEntityCore *host );

private:
	// results, tallied on client and server, but only used by server to run SV_Impact.
	// we store off our velocity in the trace_t structure so that we can determine results
	// of shoving boxes etc. around.
	struct touchlist_t
	{
		Vector	deltavelocity;
		trace_t trace;

		touchlist_t() {}
		touchlist_t( touchlist_t && ) = default;	// std::vector relocation; copies stay private
		touchlist_t &operator=( touchlist_t && ) = default;

	private:
		touchlist_t( const touchlist_t &src );
	};

	std::vector< touchlist_t >			m_TouchList;

	CBaseEntity*	m_pHost;
};	

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------


static CMoveHelperClient s_MoveHelperClient;

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CMoveHelperClient::CMoveHelperClient( void )
{
	m_pHost = 0;
	g_pMoveHelper = this;
}

CMoveHelperClient::~CMoveHelperClient( void )
{
	g_pMoveHelper = nullptr;
}

//-----------------------------------------------------------------------------
// Indicates which entity we're going to move
//-----------------------------------------------------------------------------

void CMoveHelperClient::SetHost( CEntityCore *host )
{
	m_pHost = static_cast<CBaseEntity *>( host );

	// In case any stuff is ever left over, sigh...
	ResetTouchList();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
char const* CMoveHelperClient::GetName( EntityHandle_t handle ) const
{
	return "";
}

//-----------------------------------------------------------------------------
// Touch list
//-----------------------------------------------------------------------------

void CMoveHelperClient::ResetTouchList( void )
{
	m_TouchList.clear();
}

//-----------------------------------------------------------------------------
// Adds to the touched list 
//-----------------------------------------------------------------------------

bool CMoveHelperClient::AddToTouched( const trace_t& tr, const Vector& impactvelocity )
{
	int i;

	// Look for duplicates
	for (i = 0; i < std::ssize( m_TouchList ); i++)
	{
		if (m_TouchList[i].trace.Ent<CBaseEntity>() == tr.Ent<CBaseEntity>())
		{
			return false;
		}
	}

	i = std::ssize( m_TouchList ); m_TouchList.emplace_back();
	m_TouchList[i].trace = tr;
	VectorCopy( impactvelocity, m_TouchList[i].deltavelocity );

	return true;
}

void CMoveHelperClient::ProcessImpacts( void )
{
	m_pHost->PhysicsTouchTriggers();

	// Don't bother if the player ain't solid
	if ( m_pHost->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		return;

	// Save off the velocity, cause we need to temporarily reset it
	Vector vel = m_pHost->GetAbsVelocity();

	// Touch other objects that were intersected during the movement.
	for (int i = 0 ; i < std::ssize( m_TouchList ); i++)
	{
		// Run the impact function as if we had run it during movement.
		CBaseEntity *entity = ClientEntityList().GetEnt( m_TouchList[i].trace.Ent<CBaseEntity>()->entindex() );
		if ( !entity )
			continue;

		contract_assert( entity != m_pHost );
		// Don't ever collide with self!!!!
		if ( entity == m_pHost )
			continue;

		// Reconstruct trace results.
		m_TouchList[i].trace.m_pEnt = entity;

		// Use the velocity we had when we collided, so boxes will move, etc.
		m_pHost->SetAbsVelocity( m_TouchList[i].deltavelocity );

		entity->PhysicsImpact( m_pHost, m_TouchList[i].trace );
	}

	// Restore the velocity
	m_pHost->SetAbsVelocity( vel );

	// So no stuff is ever left over, sigh...
	ResetTouchList();
}

void CMoveHelperClient::StartSound( const Vector& origin, const char *soundname )
{
	if ( !soundname )
		return;

	CLocalPlayerFilter filter;
	filter.UsePredictionRules();
	CBaseEntity::EmitSound( filter, m_pHost->entindex(), soundname, &origin );
}

//-----------------------------------------------------------------------------
// Play a sound
//-----------------------------------------------------------------------------

void CMoveHelperClient::StartSound( const Vector& origin, int channel, 
	char const* pSample, float volume, soundlevel_t soundlevel, int fFlags, int pitch )
{
	if ( pSample )
	{
		CBaseEntity::PrecacheScriptSound( pSample );
		CLocalPlayerFilter filter;
		filter.UsePredictionRules();

		EmitSound_t ep;
		ep.m_nChannel = channel;
		ep.m_pSoundName =  pSample;
		ep.m_flVolume = volume;
		ep.m_SoundLevel = soundlevel;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &origin;

		CBaseEntity::EmitSound( filter, m_pHost->entindex(), ep );
	}
}

//-----------------------------------------------------------------------------
// Play a event
//-----------------------------------------------------------------------------

void CMoveHelperClient::PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	// TODO
	if (g_pMoveData->m_bFirstRunOfFunctions )
	{
	}
}

//-----------------------------------------------------------------------------
// Surface properties interface
//-----------------------------------------------------------------------------

extern IPhysicsSurfaceProps *physprops;
IPhysicsSurfaceProps *CMoveHelperClient::GetSurfaceProps( void )
{
	return physprops;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bDeveloper - 
//			*pFormat - 
//			... - 
//-----------------------------------------------------------------------------
void CMoveHelperClient::Con_NPrintf( int idx, char const* pFormat, ...)
{
	va_list marker;
	char msg[8192];

	__builtin_va_start(marker, pFormat);
	V_vsnprintf(msg, sizeof( msg ), pFormat, marker);
	__builtin_va_end(marker);
	
	engine->Con_NPrintf( idx, "%s", msg );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player falls onto a surface fast enough to take
//			damage, according to the rules in CGameMovement::CheckFalling.
// Output : Returns true if the player survived the fall, false if they died.
//-----------------------------------------------------------------------------
bool CMoveHelperClient::PlayerFallingDamage(void)
{
	// Do nothing; falling damage is applied in MoveHelper_Server::PlayerFallingDamage.
	return(true);
}

//-----------------------------------------------------------------------------
// Purpose: Sets an animation in the player.
// Input  : eAnim - Animation to set.
//-----------------------------------------------------------------------------
void CMoveHelperClient::PlayerSetAnimation( PLAYER_ANIM eAnim )
{
	 // Do nothing on the client. Animations are set on the server.
}

bool CMoveHelperClient::IsWorldEntity( const CBaseHandle &handle )
{
	return handle == cl_entitylist->GetNetworkableHandle( 0 );
}

// some cvars used by player movement system
inline constexpr const char *DEFAULT_GRAVITY_STRING = "800";

ConVar	sv_gravity		( side_registry(), "sv_gravity", DEFAULT_GRAVITY_STRING, FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "World gravity." );

ConVar	sv_stopspeed	( side_registry(), "sv_stopspeed","80", FCVAR_NOTIFY | FCVAR_REPLICATED |  FCVAR_RELEASE, "Minimum stopping speed when on ground." );
ConVar	sv_noclipaccelerate( side_registry(), "sv_noclipaccelerate", "5", FCVAR_NOTIFY | FCVAR_ARCHIVE | FCVAR_REPLICATED);
ConVar	sv_noclipspeed	( side_registry(), "sv_noclipspeed", "5", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_specaccelerate( side_registry(), "sv_specaccelerate", "5", FCVAR_NOTIFY | FCVAR_ARCHIVE | FCVAR_REPLICATED);
ConVar	sv_specspeed	( side_registry(), "sv_specspeed", "3", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_specnoclip	( side_registry(), "sv_specnoclip", "1", FCVAR_ARCHIVE | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar	sv_maxspeed		( side_registry(), "sv_maxspeed", "320", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE);

ConVar	sv_accelerate	( side_registry(), "sv_accelerate", "5.5", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "Linear acceleration amount (old value is 5.6)" );
ConVar	sv_accelerate_use_weapon_speed	( side_registry(), "sv_accelerate_use_weapon_speed", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar	sv_accelerate_debug_speed	( side_registry(), "sv_accelerate_debug_speed", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar	sv_airaccelerate(  side_registry(), "sv_airaccelerate", "12", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE );    
ConVar	sv_wateraccelerate(  side_registry(), "sv_wateraccelerate", "10", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_RELEASE );     
ConVar	sv_waterfriction(  side_registry(), "sv_waterfriction", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_RELEASE );      
ConVar	sv_footsteps	( side_registry(), "sv_footsteps", "1", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Play footstep sound for players" );
ConVar	sv_rollspeed	( side_registry(), "sv_rollspeed", "200", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);
ConVar	sv_rollangle	( side_registry(), "sv_rollangle", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Max view roll angle");

ConVar	sv_friction		( side_registry(), "sv_friction","5.2", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "World friction." );

ConVar	sv_bounce		( side_registry(), "sv_bounce","0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "Bounce multiplier for when physically simulated objects collide with other objects." );
ConVar	sv_maxvelocity	( side_registry(), "sv_maxvelocity","3500", FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum speed any ballistically moving object is allowed to attain per axis." );

ConVar	sv_stepsize		( side_registry(), "sv_stepsize","18", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

ConVar	sv_skyname		( side_registry(), "sv_skyname", "sky_urb01", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Current name of the skybox texture" );
ConVar	sv_backspeed	( side_registry(), "sv_backspeed", "0.6", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How much to slow down backwards motion" );
ConVar  sv_waterdist	( side_registry(), "sv_waterdist","12", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Vertical view fixup when eyes are near water plane." );

// Vehicle convars
ConVar r_VehicleViewDampen( side_registry(), "r_VehicleViewDampen", "1", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

// Jeep convars
ConVar r_JeepViewDampenFreq( side_registry(), "r_JeepViewDampenFreq", "7.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar r_JeepViewDampenDamp( side_registry(), "r_JeepViewDampenDamp", "1.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar r_JeepViewZHeight( side_registry(), "r_JeepViewZHeight", "10.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

// Airboat convars
ConVar r_AirboatViewDampenFreq( side_registry(), "r_AirboatViewDampenFreq", "7.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar r_AirboatViewDampenDamp( side_registry(), "r_AirboatViewDampenDamp", "1.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED);
ConVar r_AirboatViewZHeight( side_registry(), "r_AirboatViewZHeight", "0.0", FCVAR_CHEAT | FCVAR_NOTIFY | FCVAR_REPLICATED );

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CMovieDisplay, DT_MovieDisplay, CMovieDisplay )

CMovieDisplay::CMovieDisplay()
{
}

CMovieDisplay::~CMovieDisplay()
{
}

//-----------------------------------------------------------------------------
// Purpose: Recieve a message from the server
//-----------------------------------------------------------------------------
void CMovieDisplay::ReceiveMessage( int classID, bf_read &msg )
{
	// Make sure our IDs match
	if ( classID != GetClientClass()->m_ClassID )
	{
		// Message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	// currently we only get one message - that we want to take over as the master
	m_bWantsToBeMaster = true;
}

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBaseParticleEntity;

using namespace ::ks::reflect;

// ------------------------------------------------------------------------- //
// Definitions
// ------------------------------------------------------------------------- //
static constexpr auto NUM_MOVIEEXPLOSION_EMITTERS = 50;
static constexpr auto EXPLOSION_EMITTER_LIFETIME = 3;
static constexpr auto EMITTED_PARTICLE_LIFETIME = 1;

// ------------------------------------------------------------------------- //
// Classes
// ------------------------------------------------------------------------- //
class MovieExplosionEmitter
{
public:
	Vector		m_Pos;
	Vector		m_Velocity;
	float		m_Lifetime;
	TimedEvent	m_ParticleSpawn;
};

class CMovieExplosion : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_MovieExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CMovieExplosion, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();

					CMovieExplosion();
					~CMovieExplosion();

// CBaseEntity.
public:
	virtual void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	MovieExplosionEmitter	m_Emitters[NUM_MOVIEEXPLOSION_EMITTERS];
	float					m_EmitterLifetime;

	CParticleMgr			*m_pParticleMgr;
	PMaterialHandle			m_iFireballMaterial;

	// Setup for temporary usage in SimulateAndRender.
	float					m_EmitterAlpha;

private:
					CMovieExplosion( const CMovieExplosion & );

};

// Expose to the particle app.

IMPLEMENT_REFLECT_CLIENTCLASS( CMovieExplosion, DT_MovieExplosion, MovieExplosion )

// ------------------------------------------------------------------------- //
// CMovieExplosion
// ------------------------------------------------------------------------- //
CMovieExplosion::CMovieExplosion()
{
	m_pParticleMgr = nullptr;
}

CMovieExplosion::~CMovieExplosion()
{
	if(m_pParticleMgr)
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
}

void CMovieExplosion::OnDataChanged(DataUpdateType_t updateType)
{
	CBaseEntity::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED)
	{
		Start( ParticleMgr(), nullptr );
	}
}

void CMovieExplosion::Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)
{
	if(!pParticleMgr->AddEffect(&m_ParticleEffect, this))
		return;

	// Setup our emitters.
	for(int iEmitter=0; iEmitter < NUM_MOVIEEXPLOSION_EMITTERS; iEmitter++)
	{
		MovieExplosionEmitter *pEmitter = &m_Emitters[iEmitter];
		pEmitter->m_Velocity = RandomVector(-1, 1) * 200;

		pEmitter->m_Pos = GetAbsOrigin();
			
		pEmitter->m_Lifetime = 0;
		pEmitter->m_ParticleSpawn.Init(15);
	}
	m_EmitterLifetime = 0;

	// Get our materials.
	m_iFireballMaterial = m_ParticleEffect.FindOrAddMaterial("particle/particle_sphere");

	m_pParticleMgr = pParticleMgr;
}

void CMovieExplosion::Update(float fTimeDelta)
{
	if(!m_pParticleMgr)
		return;

	m_EmitterLifetime += fTimeDelta;
	if(m_EmitterLifetime > EXPLOSION_EMITTER_LIFETIME)
		return;

	m_EmitterAlpha = (float)std::sin(m_EmitterLifetime * 3.14159f / EXPLOSION_EMITTER_LIFETIME);

	// Simulate the emitters and have them spit out particles.
	for(int iEmitter=0; iEmitter < NUM_MOVIEEXPLOSION_EMITTERS; iEmitter++)
	{
		MovieExplosionEmitter *pEmitter = &m_Emitters[iEmitter];

		pEmitter->m_Pos = pEmitter->m_Pos + pEmitter->m_Velocity * fTimeDelta;
		pEmitter->m_Velocity = pEmitter->m_Velocity * 0.9;

		float tempDelta = fTimeDelta;
		while(pEmitter->m_ParticleSpawn.NextEvent(tempDelta))
		{
			StandardParticle_t *pParticle = 
				(StandardParticle_t*)m_ParticleEffect.AddParticle( sizeof(StandardParticle_t), m_iFireballMaterial);

			if(pParticle)
			{
				pParticle->m_Pos = pEmitter->m_Pos;
				pParticle->m_Velocity = pEmitter->m_Velocity * 0.2f + RandomVector(-20, 20);
			}
		}
	}
}

void CMovieExplosion::RenderParticles( CParticleRenderIterator *pIterator )
{
	const StandardParticle_t *pParticle = (const StandardParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Draw.
		Vector tPos;
		TransformParticle(m_pParticleMgr->GetModelView(), pParticle->m_Pos, tPos);
		float sortKey = tPos.z;

		float lifetimePercent = pParticle->m_Lifetime / EMITTED_PARTICLE_LIFETIME;
		Vector color;
		color.x = std::sin(lifetimePercent * 3.14159);
		color.y = color.x * 0.5f;
		color.z = 0;
		RenderParticle_ColorSize(
			pIterator->GetParticleDraw(),
			tPos,
			color,
			m_EmitterAlpha * sin(3.14159 * lifetimePercent),
			10);

		pParticle = (const StandardParticle_t*)pIterator->GetNext( sortKey );
	}
}

void CMovieExplosion::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	StandardParticle_t *pParticle = (StandardParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Update its lifetime.
		pParticle->m_Lifetime += pIterator->GetTimeDelta();
		if(pParticle->m_Lifetime > 1)
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{		
			// Move it (this comes after rendering to make it clear that moving the particle here won't change
			// its rendering for this frame since m_TransformedPos has already been set).
			pParticle->m_Pos = pParticle->m_Pos + pParticle->m_Velocity * pIterator->GetTimeDelta();
		}
		
		pParticle = (StandardParticle_t*)pIterator->GetNext();
	}
}

using namespace ::ks::reflect;

const char *g_pszMPConcepts[] =
{
	"TLK_FIREWEAPON",				// MP_CONCEPT_FIREWEAPON
	"TLK_HURT",						// MP_CONCEPT_HURT
	"TLK_PLAYER_EXPRESSION",		// MP_CONCEPT_PLAYER_EXPRESSION
	"TLK_WINDMINIGUN",				// MP_CONCEPT_WINDMINIGUN
	"TLK_FIREMINIGUN",				// MP_CONCEPT_FIREMINIGUN
	"TLK_PLAYER_MEDIC",				// MP_CONCEPT_PLAYER_MEDIC
	"TLK_DETONATED_OBJECT",			// MP_CONCEPT_DETONATED_OBJECT 
	"TLK_KILLED_PLAYER",			// MP_CONCEPT_KILLED_PLAYER
	"TLK_KILLED_OBJECT",			// MP_CONCEPT_KILLED_OBJECT
	"TLK_PLAYER_PAIN",				// MP_CONCEPT_PLAYER_PAIN
	"TLK_PLAYER_ATTACKER_PAIN",		// MP_CONCEPT_PLAYER_ATTACKER_PAIN
	"TLK_PLAYER_TAUNT",				// MP_CONCEPT_PLAYER_TAUNT
	"TLK_PLAYER_HELP",				// MP_CONCEPT_PLAYER_HELP
	"TLK_PLAYER_GO",				// MP_CONCEPT_PLAYER_GO
	"TLK_PLAYER_MOVEUP",			// MP_CONCEPT_PLAYER_MOVEUP
	"TLK_PLAYER_LEFT",				// MP_CONCEPT_PLAYER_LEFT
	"TLK_PLAYER_RIGHT",				// MP_CONCEPT_PLAYER_RIGHT
	"TLK_PLAYER_YES",				// MP_CONCEPT_PLAYER_YES
	"TLK_PLAYER_NO",				// MP_CONCEPT_PLAYER_NO
	"TLK_PLAYER_INCOMING",			// MP_CONCEPT_PLAYER_INCOMING
	"TLK_PLAYER_CLOAKEDSPY",		// MP_CONCEPT_PLAYER_CLOAKEDSPY
	"TLK_PLAYER_SENTRYAHEAD",		// MP_CONCEPT_PLAYER_SENTRYAHEAD
	"TLK_PLAYER_TELEPORTERHERE",	// MP_CONCEPT_PLAYER_TELEPORTERHERE
	"TLK_PLAYER_DISPENSERHERE",		// MP_CONCEPT_PLAYER_DISPENSERHERE
	"TLK_PLAYER_SENTRYHERE",		// MP_CONCEPT_PLAYER_SENTRYHERE
	"TLK_PLAYER_ACTIVATECHARGE",	// MP_CONCEPT_PLAYER_ACTIVATECHARGE
	"TLK_PLAYER_CHARGEREADY",		// MP_CONCEPT_PLAYER_CHARGEREADY
	"TLK_PLAYER_TAUNTS"	,			// MP_CONCEPT_PLAYER_TAUNTS
	"TLK_PLAYER_BATTLECRY",			// MP_CONCEPT_PLAYER_BATTLECRY
	"TLK_PLAYER_CHEERS",			// MP_CONCEPT_PLAYER_CHEERS
	"TLK_PLAYER_JEERS",				// MP_CONCEPT_PLAYER_JEERS
	"TLK_PLAYER_POSITIVE",			// MP_CONCEPT_PLAYER_POSITIVE
	"TLK_PLAYER_NEGATIVE",			// MP_CONCEPT_PLAYER_NEGATIVE
	"TLK_PLAYER_NICESHOT",			// MP_CONCEPT_PLAYER_NICESHOT
	"TLK_PLAYER_GOODJOB",			// MP_CONCEPT_PLAYER_GOODJOB
	"TLK_MEDIC_STARTEDHEALING",		// MP_CONCEPT_MEDIC_STARTEDHEALING
	"TLK_MEDIC_CHARGEREADY",		// MP_CONCEPT_MEDIC_CHARGEREADY
	"TLK_MEDIC_STOPPEDHEALING",		// MP_CONCEPT_MEDIC_STOPPEDHEALING
	"TLK_MEDIC_CHARGEDEPLOYED",		// MP_CONCEPT_MEDIC_CHARGEDEPLOYED
	"TLK_FLAGPICKUP",				// MP_CONCEPT_FLAGPICKUP
	"TLK_FLAGCAPTURED",				// MP_CONCEPT_FLAGCAPTURED
	"TLK_ROUND_START",				// MP_CONCEPT_ROUND_START
	"TLK_SUDDENDEATH_START",		// MP_CONCEPT_SUDDENDEATH_START
	"TLK_ONFIRE",					// MP_CONCEPT_ONFIRE
	"TLK_STALEMATE",				// MP_CONCEPT_STALEMATE
	"TLK_BUILDING_OBJECT",			// MP_CONCEPT_BUILDING_OBJECT
	"TLK_LOST_OBJECT",				// MP_CONCEPT_LOST_OBJECT
	"TLK_SPY_SAPPER",				// MP_CONCEPT_SPY_SAPPER
	"TLK_TELEPORTED",				// MP_CONCEPT_TELEPORTED
	"TLK_LOST_CONTROL_POINT",		// MP_CONCEPT_LOST_CONTROL_POINT
	"TLK_CAPTURED_POINT",			// MP_CONCEPT_CAPTURED_POINT
	"TLK_CAPTURE_BLOCKED",			// MP_CONCEPT_CAPTURE_BLOCKED
	"TLK_HEALTARGET_STARTEDHEALING",// MP_CONCEPT_HEALTARGET_STARTEDHEALING
	"TLK_HEALTARGET_CHARGEREADY",	// MP_CONCEPT_HEALTARGET_CHARGEREADY
	"TLK_HEALTARGET_STOPPEDHEALING",// MP_CONCEPT_HEALTARGET_STOPPEDHEALING
	"TLK_HEALTARGET_CHARGEDEPLOYED",// MP_CONCEPT_HEALTARGET_CHARGEDEPLOYED
	"TLK_MINIGUN_FIREWEAPON",		// MP_CONCEPT_MINIGUN_FIREWEAPON
	"TLK_DIED",						// MP_CONCEPT_DIED
	"TLK_PLAYER_THANKS",			// MP_CONCEPT_PLAYER_THANKS
	"TLK_CART_MOVING_FORWARD",		// MP_CONCEPT_CART_MOVING_FORWARD
	"TLK_CART_MOVING_BACKWARD",		// MP_CONCEPT_CART_MOVING_BACKWARD
	"TLK_CART_STOP",				// MP_CONCEPT_CART_STOP
	"TLK_ATE_FOOD",					// MP_CONCEPT_ATE_FOOD
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetMPConceptIndexFromString( const char *pszConcept )
{
	// Make sure our concept string and enum arrays are the same length
	contract_assert( std::size( g_pszMPConcepts ) == MP_TF_CONCEPT_COUNT );

	for ( int iConcept = 0; iConcept < std::size( g_pszMPConcepts ); ++iConcept )
	{
		if ( !V_stricmp( pszConcept, g_pszMPConcepts[iConcept] ) )
			return iConcept;
	}

	return MP_CONCEPT_NONE;
}

using namespace ::ks::reflect;

// Purpose: Contains the implementation of game rules for multiplayer.

extern ConVar mp_verbose_changelevel_spew;
extern ConVar sv_kick_ban_duration;
extern ConVar mp_autokick;

REGISTER_GAMERULES_CLASS( CMultiplayRules );

ConVar mp_match_restart_delay(
		side_registry(), "mp_match_restart_delay", 
		"15", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Time (in seconds) until a match restarts.",
		true, 1,
		true, 120 );

ConVar mapcycledisabled(
				   side_registry(), "mapcycledisabled",
				   "0",
				   FCVAR_REPLICATED | FCVAR_RELEASE,
				   "repeats the same map after each match instead of using the map cycle");

ConVar mp_timelimit( side_registry(), "mp_timelimit", "5", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_RELEASE, "game time per map in minutes"
					);

ConVar fraglimit( side_registry(), "mp_fraglimit","0", FCVAR_NOTIFY|FCVAR_REPLICATED, "The number of kills at which the map ends" );

ConVar nextlevel( side_registry(), "nextlevel", 
	"", 
	FCVAR_NOTIFY | FCVAR_RELEASE | FCVAR_REPLICATED,
	"If set to a valid map name, will trigger a changelevel to the specified map at the end of the round"
				  
	);

//=========================================================
//=========================================================
bool CMultiplayRules::IsMultiplayer( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldGibCorpse( void )
{
	int iDamage = ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetShowOnHud( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetNoPhysicsForce( void )
{
	int iTimeBasedDamage = Damage_GetTimeBased();
	int iDamage = ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldNotBleed( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldGibCorpse( int iDmgType )
{
	// Damage types that gib the corpse.
	return ( ( iDmgType & ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShowOnHUD( int iDmgType )
{
	// Damage types that have client HUD art.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_NoPhysicsForce( int iDmgType )
{
	// Damage types that don't have to supply a physics force & position.
	int iTimeBasedDamage = Damage_GetTimeBased();
	return ( ( iDmgType & ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldNotBleed( int iDmgType )
{
	// Damage types that don't make the player bleed.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID ) ) != 0 );
}

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************
CMultiplayRules::CMultiplayRules()
{

	LoadVoiceCommandScript();
}

	//-----------------------------------------------------------------------------
	// Purpose: Shared script resource of voice menu commands and hud strings
	//-----------------------------------------------------------------------------
	void CMultiplayRules::LoadVoiceCommandScript( void )
	{
		KeyValues *pKV = new KeyValues( "VoiceCommands" );

		if ( pKV->LoadFromFile( filesystem, "scripts/voicecommands.txt", "GAME" ) )
		{
			for ( KeyValues *menu = pKV->GetFirstSubKey(); menu != nullptr; menu = menu->GetNextKey() )
			{
				int iMenuIndex = std::ssize( m_VoiceCommandMenus ); m_VoiceCommandMenus.emplace_back();

				int iNumItems = 0;

				// for each subkey of this menu, add a menu item
				for ( KeyValues *menuitem = menu->GetFirstSubKey(); menuitem != nullptr; menuitem = menuitem->GetNextKey() )
				{
					iNumItems++;

					if ( iNumItems > 9 )
					{
						Warning( "Trying to load more than 9 menu items in voicecommands.txt, extras ignored" );
						continue;
					}

					VoiceCommandMenuItem_t item;

					V_strncpy( item.m_szSubtitle, menuitem->GetString( "subtitle", "" ), MAX_VOICE_COMMAND_SUBTITLE );
					V_strncpy( item.m_szMenuLabel, menuitem->GetString( "menu_label", "" ), MAX_VOICE_COMMAND_SUBTITLE );

					m_VoiceCommandMenus[ iMenuIndex ].push_back( item );
				}
			}
		}

		pKV->deleteThis();
	}

	const char *CMultiplayRules::GetVoiceCommandSubtitle( int iMenu, int iItem )
	{
		contract_assert( iMenu >= 0 && iMenu < std::ssize( m_VoiceCommandMenus ) );
		if ( iMenu < 0 || iMenu >= std::ssize( m_VoiceCommandMenus ) )
			return "";

		contract_assert( iItem >= 0 && iItem < std::ssize( m_VoiceCommandMenus[ iMenu ] ) );
		if ( iItem < 0 || iItem >= std::ssize( m_VoiceCommandMenus[ iMenu ] ) )
			return "";

		VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus[ iMenu ][ iItem ];

		contract_assert( pItem );

		return pItem->m_szSubtitle;
	}

	// Returns false if no such menu is declared or if it's an empty menu
	bool CMultiplayRules::GetVoiceMenuLabels( int iMenu, KeyValues *pKV )
	{
		contract_assert( iMenu >= 0 && iMenu < std::ssize( m_VoiceCommandMenus ) );
		if ( iMenu < 0 || iMenu >= std::ssize( m_VoiceCommandMenus ) )
			return false;

		int iNumItems = std::ssize( m_VoiceCommandMenus[ iMenu ] );

		for ( int i=0; i<iNumItems; i++ )
		{
			VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus[ iMenu ][ i ];

			KeyValues *pLabelKV = new KeyValues( pItem->m_szMenuLabel );

			pKV->AddSubKey( pLabelKV );
		}

		return iNumItems > 0;
	}

class CBasePlayer;
class IGameSystem;

using namespace ::ks::reflect;

// Purpose: Provides an interface to enable positional audio support in Mumble

struct MumbleSharedMemory_t
{
	uint32_t uiVersion;
	uint32_t uiTick;
	float	fAvatarPosition[3];
	float	fAvatarFront[3];
	float	fAvatarTop[3];
	wchar_t	name[256];
	float	fCameraPosition[3];
	float	fCameraFront[3];
	float	fCameraTop[3];
	wchar_t	identity[256];
	uint32_t context_len;
	unsigned char context[256];
	wchar_t description[2048];
};

MumbleSharedMemory_t *g_pMumbleMemory = nullptr;
ks::platform::shared_memory *g_pMumbleMapping = nullptr;

ConVar sv_mumble_positionalaudio( side_registry(), "sv_mumble_positionalaudio", "1", FCVAR_REPLICATED, "Allows players using Mumble to have support for positional audio." );

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CMumbleSystem g_MumbleSystem;

IGameSystem *MumbleSystem()
{
	return &g_MumbleSystem;
}

bool CMumbleSystem::Init()
{
	return true;
}

void CMumbleSystem::LevelInitPostEntity()
{
	if ( g_pMumbleMemory )
		return;

	char memname[256];
	V_sprintf_safe( memname, "/MumbleLink.%d", (int)ks::platform::user_id() );

	g_pMumbleMapping = ks::platform::open_shared_memory( memname, sizeof(MumbleSharedMemory_t), nullptr );
	if ( g_pMumbleMapping == nullptr )
	{
		return;
	}

	g_pMumbleMemory = (MumbleSharedMemory_t *)ks::platform::shared_memory_data( g_pMumbleMapping );
}

void CMumbleSystem::LevelShutdownPreEntity()
{
	if ( g_pMumbleMemory )
	{
		ks::platform::close_shared_memory( g_pMumbleMapping );
		g_pMumbleMapping = nullptr;
		g_pMumbleMemory = nullptr;
	}
}

void VectorToMumbleFloatArray( const Vector &vec, float *rgfl )
{
	// Traditional left-handed coordinate system
	rgfl[0] = vec.x; // X
	rgfl[1] = vec.z; // Y is up
	rgfl[2] = vec.y; // Z is into the screen
}

void CMumbleSystem::PostRender()
{
	if ( !g_pMumbleMemory || !sv_mumble_positionalaudio.GetBool() )
		return;

	if ( g_pMumbleMemory->uiVersion != 2 )
	{
		V_wcscpy_safe( g_pMumbleMemory->name, L"Source: " );
		wchar_t wcsGameDir[MAX_PATH];
		V_UTF8ToUnicode( COM_GetModDirectory(), wcsGameDir, sizeof(wcsGameDir) );
		V_wcscat_safe( g_pMumbleMemory->name, wcsGameDir );

		V_wcscpy_safe( g_pMumbleMemory->description, L"Links Source engine games to Mumble." );
		g_pMumbleMemory->uiVersion = 2;
	}

	g_pMumbleMemory->uiTick++;

	Vector vecOriginPlayer, vecOriginCamera = MainViewOrigin( 0 );
	QAngle anglesPlayer, anglesCamera = MainViewAngles( 0 );

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		bool bIsOnTeam = pPlayer->GetTeamNumber() == TEAM_TERRORIST || pPlayer->GetTeamNumber() == TEAM_CT;
		if ( pPlayer->IsAlive() && bIsOnTeam )
		{
			vecOriginPlayer = pPlayer->EyePosition();
		}
		else
		{
			// a zero player origin disables positional audio
			vecOriginPlayer = vec3_origin;
		}
		anglesPlayer = pPlayer->GetAbsAngles();
	}
	else
	{
		vecOriginPlayer = vecOriginCamera;
		anglesPlayer = anglesCamera;
	}

	anglesPlayer.x = 0;

	Vector vecPlayerForward, vecPlayerUp, vecCameraForward, vecCameraUp;
	AngleVectors( anglesPlayer, &vecPlayerForward, nullptr, &vecPlayerUp );
	AngleVectors( anglesCamera, &vecCameraForward, nullptr, &vecCameraUp );

	// 1 Source unit is about one inch
	// 1 mumble unit = 1 meter
	vecOriginPlayer *= METERS_PER_INCH;
	vecOriginCamera *= METERS_PER_INCH;

	VectorToMumbleFloatArray( vecPlayerForward, g_pMumbleMemory->fAvatarFront );
	VectorToMumbleFloatArray( vecPlayerUp, g_pMumbleMemory->fAvatarTop );
	VectorToMumbleFloatArray( vecOriginPlayer, g_pMumbleMemory->fAvatarPosition );

	VectorToMumbleFloatArray( vecCameraForward, g_pMumbleMemory->fCameraFront );
	VectorToMumbleFloatArray( vecCameraUp, g_pMumbleMemory->fCameraTop );
	VectorToMumbleFloatArray( vecOriginCamera, g_pMumbleMemory->fCameraPosition );

	g_pMumbleMemory->context_len = 0;
}

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Functionality to render object motion blur

ConVar mat_object_motion_blur_enable( side_registry(), "mat_object_motion_blur_enable", "0" );
ConVar mat_object_motion_blur_model_scale( side_registry(), "mat_object_motion_blur_model_scale", "1.2" );

CObjectMotionBlurManager g_ObjectMotionBlurManager;

int CObjectMotionBlurManager::RegisterObject( CBaseAnimating *pEntity, float flVelocityScale )
{
	int nIndex;
	if ( m_nFirstFreeSlot == ObjectMotionBlurDefinition_t::END_OF_FREE_LIST )
	{
		nIndex = std::ssize( m_ObjectMotionBlurDefinitions ); m_ObjectMotionBlurDefinitions.emplace_back();
	}
	else
	{
		nIndex = m_nFirstFreeSlot;
		m_nFirstFreeSlot = m_ObjectMotionBlurDefinitions[nIndex].m_nNextFreeSlot;
	}
	m_ObjectMotionBlurDefinitions[nIndex].m_pEntity = pEntity;
	m_ObjectMotionBlurDefinitions[nIndex].m_flVelocityScale = flVelocityScale;
	m_ObjectMotionBlurDefinitions[nIndex].m_nNextFreeSlot = ObjectMotionBlurDefinition_t::ENTRY_IN_USE;

	return nIndex;
}

void CObjectMotionBlurManager::UnregisterObject( int nObjectHandle )
{
	contract_assert( !m_ObjectMotionBlurDefinitions[nObjectHandle].IsUnused() );

	m_ObjectMotionBlurDefinitions[nObjectHandle].m_nNextFreeSlot = m_nFirstFreeSlot;
	m_ObjectMotionBlurDefinitions[nObjectHandle].m_pEntity = nullptr;
	m_nFirstFreeSlot = nObjectHandle;
}

void CObjectMotionBlurManager::DrawObjects()
{
	for ( int i = 0; i < std::ssize( m_ObjectMotionBlurDefinitions ); ++ i )
	{
		if ( m_ObjectMotionBlurDefinitions[i].ShouldDraw() )
		{
			m_ObjectMotionBlurDefinitions[i].DrawModel();
		}
	}
}

int CObjectMotionBlurManager::GetDrawableObjectCount()
{
	int nCount = 0;
	for ( int i = 0; i < std::ssize( m_ObjectMotionBlurDefinitions ); ++ i )
	{
		if ( m_ObjectMotionBlurDefinitions[i].ShouldDraw() )
		{
			nCount++;
		}
	}
	return nCount;
}

void CObjectMotionBlurManager::ObjectMotionBlurDefinition_t::DrawModel()
{
	Vector vVelocity;
	m_pEntity->EstimateAbsVelocity( vVelocity );
	float flR = ( m_flVelocityScale * vVelocity.x + 128.0f ) / 256.0f;
	float flG = ( m_flVelocityScale * vVelocity.y + 128.0f ) / 256.0f;

	float flColor[3] = { flR, flG, 0.0f };
	render->SetColorModulation( flColor );

	CBaseEntity *pAttachment;
	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	
	if ( mat_object_motion_blur_model_scale.GetFloat() != 1.0f )
	{
		m_pEntity->SetModelScale( mat_object_motion_blur_model_scale.GetFloat() );
		m_pEntity->InvalidateBoneCache();

		m_pEntity->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
		pAttachment = m_pEntity->FirstMoveChild();

		while ( pAttachment != nullptr )
		{
			if ( pAttachment->ShouldDraw() )
			{
				pAttachment->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
			}
			pAttachment = pAttachment->NextMovePeer();
		}

		m_pEntity->SetModelScale( 1.0f );
		m_pEntity->InvalidateBoneCache();
	}

	flColor[2] = 1.0f;
	render->SetColorModulation( flColor );

	m_pEntity->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
	pAttachment = m_pEntity->FirstMoveChild();

	while ( pAttachment != nullptr )
	{
		if ( pAttachment->ShouldDraw() )
		{
			pAttachment->DrawModel( STUDIO_RENDER | STUDIO_SKIP_FLEXES | STUDIO_DONOTMODIFYSTENCILSTATE | STUDIO_NOLIGHTING_OR_CUBEMAP | STUDIO_SKIP_DECALS, instance );
		}
		pAttachment = pAttachment->NextMovePeer();
	}
}

void CObjectMotionBlurManager::SetVelocityScale( int nObjectHandle, float flVelocityScale)
{
	contract_assert( !m_ObjectMotionBlurDefinitions[nObjectHandle].IsUnused() );
	m_ObjectMotionBlurDefinitions[nObjectHandle].m_flVelocityScale = flVelocityScale;
}

class CUserCmd;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------------------------------
ConVar sv_pushaway_force( side_registry(), "sv_pushaway_force", "30000", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How hard physics objects are pushed away from the players on the server." );
ConVar sv_pushaway_min_player_speed( side_registry(), "sv_pushaway_min_player_speed", "75", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "If a player is moving slower than this, don't push away physics objects (enables ducking behind things)." );
ConVar sv_pushaway_max_force( side_registry(), "sv_pushaway_max_force", "1000", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Maximum amount of force applied to physics objects by players." );
ConVar sv_pushaway_clientside( side_registry(), "sv_pushaway_clientside", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Clientside physics push away (0=off, 1=only localplayer, 1=all players)" );

ConVar sv_pushaway_player_force( side_registry(), "sv_pushaway_player_force", "200000", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "How hard the player is pushed away from physics objects (falls off with inverse square of distance)." );
ConVar sv_pushaway_max_player_force( side_registry(), "sv_pushaway_max_player_force", "10000", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Maximum of how hard the player is pushed away from physics objects." );

ConVar sv_turbophysics( side_registry(), "sv_turbophysics", "0", FCVAR_REPLICATED, "Turns on turbo physics" );

//-----------------------------------------------------------------------------------------------------
bool IsPushAwayEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	if ( pEnt->GetCollisionGroup() != COLLISION_GROUP_PUSHAWAY )
	{
		// Try backing away from doors that are currently rotating, to prevent blocking them
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------------------------------
bool IsPushableEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	if ( sv_turbophysics.GetBool() )
	{
		if ( pEnt->GetCollisionGroup() == COLLISION_GROUP_NONE )
		{
			if ( FClassnameIs( pEnt, "class CPhysicsPropMultiplayer" ) )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
bool IsBreakableEntity( CBaseEntity *pEnt )
{
	if ( pEnt == nullptr )
		return false;

	// first check to see if it's already broken
	if ( pEnt->m_iHealth < 0 && pEnt->GetMaxHealth() > 0 )
		return true;

	// If we won't be able to break it, don't try
	if ( pEnt->m_takedamage != DAMAGE_YES )
		return false;

	if ( pEnt->GetCollisionGroup() != COLLISION_GROUP_PUSHAWAY && pEnt->GetCollisionGroup() != COLLISION_GROUP_BREAKABLE_GLASS && pEnt->GetCollisionGroup() != COLLISION_GROUP_NONE )
		return false;

	if ( pEnt->m_iHealth > 200 )
		return false;

	IMultiplayerPhysics *pPhysicsInterface = dynamic_cast< IMultiplayerPhysics * >( pEnt );
	if ( pPhysicsInterface )
	{
		if ( pPhysicsInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			return false;
	}
	else
	{
		if ((FClassnameIs( pEnt, "func_breakable" ) || FClassnameIs( pEnt, "func_breakable_surf" )))
		{
			if (FClassnameIs( pEnt, "func_breakable_surf" ))
			{
				// don't try to break it if it has already been broken
				CBreakableSurface *surf = static_cast< CBreakableSurface * >( pEnt );

				if ( surf->m_bIsBroken )
					return false;
			}
		}
		else if ( pEnt->PhysicsSolidMaskForEntity() & CONTENTS_PLAYERCLIP )
		{
			// hostages and players use CONTENTS_PLAYERCLIP, so we can use it to ignore them
			return false;
		}
	}

	IBreakableWithPropData *pBreakableInterface = dynamic_cast< IBreakableWithPropData * >( pEnt );
	if ( pBreakableInterface )
	{
		// Bullets don't damage it - ignore
		if ( pBreakableInterface->GetDmgModBullet() <= 0.0f )
		{
			return false;
		}
	}

// what the shit is this
/*
	CBreakableProp *pProp = dynamic_cast< CBreakableProp * >( pEnt );
	if ( pProp )
	{
		// It takes a large amount of damage to even scratch it - ignore
		if ( pProp->m_iMinHealthDmg >= 50 )
		{
			return false;
		}
	}
*/

	return true;
}

//-----------------------------------------------------------------------------------------------------
int GetPushawayEnts( CBaseCombatCharacter *pPushingEntity, CBaseEntity **ents, int nMaxEnts, float flPlayerExpand, int PartitionMask, CPushAwayEnumerator *enumerator )
{
	
	Vector vExpand( flPlayerExpand, flPlayerExpand, flPlayerExpand );

	Ray_t ray;
	ray.Init( pPushingEntity->GetAbsOrigin(), pPushingEntity->GetAbsOrigin(), pPushingEntity->GetCollideable()->OBBMins() - vExpand, pPushingEntity->GetCollideable()->OBBMaxs() + vExpand );

	CPushAwayEnumerator *physPropEnum = nullptr;
	if  ( !enumerator )
	{
		physPropEnum = new CPushAwayEnumerator( ents, nMaxEnts );
		enumerator = physPropEnum;
	}

	::partition->EnumerateElementsAlongRay( PartitionMask, ray, false, enumerator );

	int numHit = enumerator->m_nAlreadyHit;

	if ( physPropEnum )
		delete physPropEnum;

	return numHit;
}

void AvoidPushawayProps( CBaseCombatCharacter *pPlayer, CUserCmd *pCmd )
{
	// Figure out what direction we're moving and the extents of the box we're going to sweep 
	// against physics objects.
	Vector currentdir;
	Vector rightdir;
	if (g_pGameRules->IsTopDown())
	{
		AngleVectors( g_pGameRules->GetTopDownMovementAxis(), &currentdir, &rightdir, nullptr );
	}
	else
	{
		AngleVectors( pCmd->viewangles, &currentdir, &rightdir, nullptr );
	}

	CBaseEntity *props[512];
	int nEnts = GetPushawayEnts( pPlayer, props, std::size( props ), 0.0f, PARTITION_CLIENT_SOLID_EDICTS, nullptr );

	const Vector & ourCenter = pPlayer->WorldSpaceCenter();
	Vector nearestPropPoint;
	Vector nearestPlayerPoint;

	for ( int i=0; i < nEnts; i++ )
	{
		// Don't respond to this entity on the client unless it has PHYSICS_MULTIPLAYER_FULL set.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );
		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			continue;

		const float minMass = 10.0f; // minimum mass that can push a player back
		const float maxMass = 30.0f; // cap at a decently large value
		float mass = maxMass;
		if ( pInterface )
		{
			mass = pInterface->GetMass();
		}
		mass = clamp( mass, minMass, maxMass );
		
		mass = std::max<std::common_type_t<float, int>>( mass, 0 );
		mass /= maxMass; // bring into a 0..1 range

		// Push away from the collision point. The closer our center is to the collision point,
		// the harder we push away.
		props[i]->CollisionProp()->CalcNearestPoint( ourCenter, &nearestPropPoint );
		pPlayer->CollisionProp()->CalcNearestPoint( nearestPropPoint, &nearestPlayerPoint );
		Vector vPushAway = (nearestPlayerPoint - nearestPropPoint);
		float flDist = VectorNormalize( vPushAway );

		const float MaxPushawayDistance = 5.0f;
		if ( flDist > MaxPushawayDistance && !pPlayer->CollisionProp()->IsPointInBounds( nearestPropPoint ) )
		{
			continue;
		}

		// If we're not pushing, try from our center to the nearest edge of the prop
		if ( vPushAway.IsZero() )
		{
			vPushAway = (ourCenter - nearestPropPoint);
			flDist = VectorNormalize( vPushAway );
		}

		// If we're still not pushing, try from our center to the center of the prop
		if ( vPushAway.IsZero() )
		{
			vPushAway = (ourCenter - props[i]->WorldSpaceCenter());
			flDist = VectorNormalize( vPushAway );
		}

		flDist = std::max<std::common_type_t<float, int>>( flDist, 1 );

		float flForce = sv_pushaway_player_force.GetFloat() / flDist * mass;
		flForce = std::min( flForce, sv_pushaway_max_player_force.GetFloat() );

		{
			flForce *= 0.25f;
		}

		vPushAway *= flForce;

		pCmd->forwardmove += vPushAway.Dot( currentdir );
		pCmd->sidemove    += vPushAway.Dot( rightdir );
	}
}

//-----------------------------------------------------------------------------------------------------
void PerformObstaclePushaway( CBaseCombatCharacter *pPushingEntity )
{
	if (  pPushingEntity->m_lifeState != LIFE_ALIVE )
		return;

	// Give a push to any barrels that we're touching.
	// The client handles adjusting our usercmd to push us away.
	CBaseEntity *props[256];

	// if sv_pushaway_clientside is disabled, clientside phys objects don't bounce away
	if ( sv_pushaway_clientside.GetInt() == 0 )
		return;

	// if sv_pushaway_clientside is 1, only local player can push them
	CBasePlayer *pPlayer = pPushingEntity->IsPlayer() ? (dynamic_cast< CBasePlayer * >(pPushingEntity)) : nullptr;
	if ( (sv_pushaway_clientside.GetInt() == 1) && (!pPlayer || !CBasePlayer::IsLocalPlayer( pPlayer )) )
		return;

	int nEnts = GetPushawayEnts( pPushingEntity, props, std::size( props ), 3.0f, PARTITION_CLIENT_RESPONSIVE_EDICTS, nullptr );
	
	for ( int i=0; i < nEnts; i++ )
	{
		// If this entity uas PHYSICS_MULTIPLAYER_FULL set (ie: it's not just debris), and we're moving too slow, don't push it away.
		// Instead, let the client bounce off it. This allows players to get close to and duck behind things without knocking them over.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );

		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() == PHYSICS_MULTIPLAYER_SOLID )
		{
			if ( pPushingEntity->GetAbsVelocity().Length2D() < sv_pushaway_min_player_speed.GetFloat() )
				continue;
		}

		IPhysicsObject *pObj = props[i]->VPhysicsGetObject();

		if ( pObj )
		{		
			Vector vPushAway = (props[i]->WorldSpaceCenter() - pPushingEntity->WorldSpaceCenter());
			vPushAway.z = 0;
			
			float flDist = VectorNormalize( vPushAway );
			flDist = std::max<std::common_type_t<float, int>>( flDist, 1 );
			
			float flForce = sv_pushaway_force.GetFloat() / flDist;
			flForce = std::min( flForce, sv_pushaway_max_force.GetFloat() );

			pObj->ApplyForceOffset( vPushAway * flForce, pPushingEntity->WorldSpaceCenter() );
		}
	}
}

CPushAwayEnumerator::CPushAwayEnumerator(CBaseEntity **ents, int nMaxEnts)
{
	m_nAlreadyHit = 0;
	m_AlreadyHit = ents;
	m_nMaxHits = nMaxEnts;
}
