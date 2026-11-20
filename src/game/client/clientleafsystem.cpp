// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "bsptreedata.h"
#include "collisionutils.h"
#include "con_nprint.h"
#include "edict.h"
#include "engine/IClientLeafSystem.h"
#include "engine/ishadowmgr.h"
#include "iclientalphaproperty.h"
#include "iclientrenderable.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "cdll_client_int.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.game.entity_cast;
import ks.math.aabb;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.platform;
import ks.simd;
import ks.tier1.bidirectional_set;
import ks.tier1.convar;
import ks.tier1.fixed_pool;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.slot_pool;
import ks.tier1.small_vector;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.tier1.refcount;

class CClientAlphaProperty;
class CClientRenderablesList;
class CNewParticleEffect;
class CParticleEffectBinding;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: This file contains code to allow us to associate client data with bsp leaves.

static ConVar cl_drawleaf(side_registry(), "cl_drawleaf", "-1", FCVAR_CHEAT );
static ConVar r_PortalTestEnts( side_registry(), "r_PortalTestEnts", "1", FCVAR_CHEAT, "Clip entities against portal frustums." );
static ConVar r_portalsopenall( side_registry(), "r_portalsopenall", "0", FCVAR_CHEAT, "Open all portals" );

static ConVar r_shadows_on_renderables_enable( side_registry(), "r_shadows_on_renderables_enable", "0", 0, "Support casting RTT shadows onto other renderables" );

static ConVar cl_leafsystemvis( side_registry(), "cl_leafsystemvis", "0", FCVAR_CHEAT );

static ConVar r_alphafade_usefov( side_registry(), "r_alphafade_usefov", "1", FCVAR_CHEAT, "Account for FOV when computing an entity's distance-based alpha fade" );

extern ConVar cl_csm_disable_culling;
extern ConVar cl_csm_shadows;
extern ConVar cl_csm_entity_shadows;
extern ConVar cl_csm_static_prop_shadows;
extern ConVar cl_csm_sprite_shadows;
extern ConVar cl_csm_ignore_disable_shadow_depth_rendering;
extern ConVar cl_csm_optimize_static_props;

ks::fixed_pool CClientRenderablesList::s_Allocator( sizeof(CClientRenderablesList), 1, ks::fixed_pool::grow::slow, "CClientRenderablesList pool", alignof(CClientRenderablesList) );

//-----------------------------------------------------------------------------
// Threading helpers
//-----------------------------------------------------------------------------

static void FrameLock()
{
	mdlcache->BeginLock();
}

static void FrameUnlock()
{
	mdlcache->EndLock();
}

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class CClientLeafSystem : public IClientLeafSystem, public ISpatialLeafEnumerator, public IClientAlphaPropertyMgr
{
public:
	virtual char const *Name() { return "CClientLeafSystem"; }

	// constructor, destructor
	CClientLeafSystem();
	virtual ~CClientLeafSystem();

	// Methods of IClientSystem
	bool Init() { return true; }
	void PostInit() {}
	void Shutdown() {}

	virtual bool IsPerFrame() { return true; }

	void PreRender();
	void PostRender() { }
	void Update( float frametime ) { m_nDebugIndex = 0; }

	// Diagnostic function to identify translucent objects
	void HighlightAllTranslucentRenderables();

	void LevelInitPreEntity();
	void LevelInitPostEntity() {}
	void LevelShutdownPreEntity();
	void LevelShutdownPostEntity();

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

// Methods of IClientAlphaPropertyMgr
public:
	virtual IClientAlphaProperty *CreateClientAlphaProperty( IClientUnknown *pUnknown );
	virtual void DestroyClientAlphaProperty( IClientAlphaProperty *pAlphaProperty );

// Methods of IClientLeafSystem
public:
	
	virtual void AddRenderable( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabledFlags );
	virtual bool IsRenderableInPVS( IClientRenderable *pRenderable );
	virtual void CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled );
	virtual void RemoveRenderable( ClientRenderHandle_t handle );

	virtual void SetSubSystemDataInLeaf( int leaf, int nSubSystemIdx, CClientLeafSubSystemData *pData );
	virtual CClientLeafSubSystemData *GetSubSystemDataInLeaf( int leaf, int nSubSystemIdx );

	// FIXME: There's an incestuous relationship between DetailObjectSystem
	// and the ClientLeafSystem. Maybe they should be the same system?
	virtual void GetDetailObjectsInLeaf( int leaf, int& firstDetailObject, int& detailObjectCount );
 	virtual void SetDetailObjectsInLeaf( int leaf, int firstDetailObject, int detailObjectCount );
	virtual void DrawDetailObjectsInLeaf( int leaf, int frameNumber, int& nFirstDetailObject, int& nDetailObjectCount );
	virtual bool ShouldDrawDetailObjectsInLeaf( int leaf, int frameNumber );
	virtual void RenderableChanged( ClientRenderHandle_t handle );
	virtual void CollateViewModelRenderables( CViewModelRenderablesList *pList );
	virtual void BuildRenderablesList( const SetupRenderInfo_t &info );
	virtual void DrawStaticProps( bool enable );
	virtual void DrawSmallEntities( bool enable );
	virtual void EnableForceOpaquePass( ClientRenderHandle_t handle, bool bEnable );
	virtual bool IsEnableForceOpaquePass( ClientRenderHandle_t handle ) const;
	virtual void EnableAlternateSorting( ClientRenderHandle_t handle, bool bEnable );
	virtual void RenderWithViewModels( ClientRenderHandle_t handle, bool bEnable );
	virtual bool IsRenderingWithViewModels( ClientRenderHandle_t handle ) const;
	virtual void SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType );
	virtual RenderableTranslucencyType_t GetTranslucencyType( ClientRenderHandle_t handle ) const;
	virtual void SetModelType( ClientRenderHandle_t handle, RenderableModelType_t nType );
	virtual void EnableSplitscreenRendering( ClientRenderHandle_t handle, uint32 nFlags );
	virtual void EnableRendering( ClientRenderHandle_t handle, bool bEnable );
	virtual void EnableBloatedBounds( ClientRenderHandle_t handle, bool bEnable );
	virtual void DisableCachedRenderBounds( ClientRenderHandle_t handle, bool bDisable );
	virtual void RenderInFastReflections( ClientRenderHandle_t handle, bool bRenderInFastReflections );
	virtual bool IsRenderingInFastReflections( ClientRenderHandle_t handle ) const;
	virtual void DisableShadowDepthRendering( ClientRenderHandle_t handle, bool bDisable );
	virtual void DisableCSMRendering( ClientRenderHandle_t handle, bool bDisable );
	virtual void DisableShadowDepthCaching( ClientRenderHandle_t handle, bool bDisable );

	// Adds a renderable to a set of leaves
	virtual void AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves );
	void AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves, bool bReceiveShadows );

	// The following methods are related to shadows...
	virtual ClientLeafShadowHandle_t AddShadow( ClientShadowHandle_t userId, unsigned short flags );
	virtual void RemoveShadow( ClientLeafShadowHandle_t h );

	virtual void ProjectShadow( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList );
	virtual void ProjectFlashlight( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList );

	// Find all shadow casters in a set of leaves
	virtual void EnumerateShadowsInLeaves( int leafCount, WorldListLeafData_t* pLeaves, IClientLeafShadowEnum* pEnum );
	virtual void RecomputeRenderableLeaves();
	virtual void DisableLeafReinsertion( bool bDisable );

	//Assuming the renderable would be in a properly built render list, generate a render list entry
	virtual RenderGroup_t GenerateRenderListEntry( IClientRenderable *pRenderable, CClientRenderablesList::CEntry &entryOut );

	// Get renderable that render bound intersect with the query box
	virtual int GetEntitiesInBox( CBaseEntity **pEntityList, int listMax, const Vector& vWorldSpaceMins, const Vector& vWorldSpaceMaxs );

	virtual void ComputeAllBounds( void );

	// methods of ISpatialLeafEnumerator
public:

	bool EnumerateLeaf( int leaf, intp context );

	// Adds a shadow to a leaf
	void AddShadowToLeaf( int leaf, ClientLeafShadowHandle_t handle, bool bFlashlight );

	// Fill in a list of the leaves this renderable is in.
	// Returns -1 if the handle is invalid.
	int GetRenderableLeaves( ClientRenderHandle_t handle, int leaves[128] );

	// Get leaves this renderable is in
	virtual bool GetRenderableLeaf ( ClientRenderHandle_t handle, int* pOutLeaf, const int* pInIterator = 0, int* pOutIterator = 0 );

	// Singleton instance...
	static CClientLeafSystem s_ClientLeafSystem;

private:
	enum
	{
		RENDER_FLAGS_DISABLE_RENDERING		= 0x01,
		RENDER_FLAGS_HASCHANGED				= 0x02,
		RENDER_FLAGS_ALTERNATE_SORTING		= 0x04,
		RENDER_FLAGS_RENDER_WITH_VIEWMODELS	= 0x08,
		RENDER_FLAGS_BLOAT_BOUNDS			= 0x10,
		RENDER_FLAGS_BOUNDS_VALID			= 0x20,
		RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE = 0x40,
		RENDER_FLAGS_IS_SPRITE				= 0x80,
		RENDER_FLAGS_FORCE_OPAQUE_PASS		= 0x100,
	};

	// All the information associated with a particular handle
	struct RenderableInfo_t
	{
		IClientRenderable*	m_pRenderable;
		CClientAlphaProperty *m_pAlphaProperty;
		int					m_EnumCount;				// Have I been added to a particular shadow yet?
		int					m_nRenderFrame;
		unsigned short		m_FirstShadow;				// The first shadow caster that cast on it
		unsigned short		m_LeafList;					// What leafs is it in?
		short				m_Area;						// -1 if the renderable spans multiple areas.
		uint16				m_Flags;					// rendering flags
		uint16				m_bRenderInFastReflection : 1;	// Should we render in the "fast" reflection?
		uint16				m_bDisableShadowDepthRendering : 1;	// Should we not render into the shadow depth map?
		uint16				m_bDisableCSMRendering : 1;			// Should we not render into the CSM?
		uint16				m_bDisableShadowDepthCaching : 1;	// Should we not be cached in the shadow depth map?
		uint16				m_nSplitscreenEnabled : 2;	// splitscreen rendering flags
		uint16				m_nTranslucencyType : 2;	// RenderableTranslucencyType_t
		uint16				m_nModelType : 8;			// RenderableModelType_t
		Vector				m_vecBloatedAbsMins;		// Use this for tree insertion
		Vector				m_vecBloatedAbsMaxs;
		Vector				m_vecAbsMins;			// NOTE: These members are not threadsafe!!
		Vector				m_vecAbsMaxs;			// They can be updated from any viewpoint (based on RENDER_FLAGS_BOUNDS_VALID)
	};

	// The leaf contains an index into a list of renderables
	struct ClientLeaf_t
	{
		unsigned short	m_FirstElement;
		unsigned short	m_FirstShadow;

		unsigned short	m_FirstDetailProp;
		unsigned short	m_DetailPropCount;
		int				m_DetailPropRenderFrame;
		CClientLeafSubSystemData *m_pSubSystemData[N_CLSUBSYSTEMS];
	};

	// Shadow information
	struct ShadowInfo_t
	{
		unsigned short	m_FirstLeaf;
		unsigned short	m_FirstRenderable;
		int				m_EnumCount;
		ClientShadowHandle_t	m_Shadow;
		unsigned short	m_Flags;
	};

	struct EnumResult_t
	{
		int leaf;
		EnumResult_t *pNext;
	};

	struct EnumResultList_t
	{
		EnumResult_t *pHead;
		ClientRenderHandle_t handle;
	};

	struct BuildRenderListInfo_t
	{
		Vector	m_vecMins;
		Vector	m_vecMaxs;
		short	m_nArea;
		uint8	m_nAlpha;
		bool	m_bPerformOcclusionTest : 1;
		bool	m_bIgnoreZBuffer : 1;
	};

	struct AlphaInfo_t
	{
		CClientAlphaProperty *m_pAlphaProperty;
		Vector m_vecCenter;
		float m_flRadius;
		float m_flFadeFactor;
	};

private:
	// Adds a renderable to the list of renderables
	void AddRenderableToLeaf( int leaf, ClientRenderHandle_t handle, bool bReceiveShadows );

	void SortEntities( const Vector &vecRenderOrigin, const Vector &vecRenderForward, CClientRenderablesList::CEntry *pEntities, BuildRenderListInfo_t **pTranslucentRLInfo, int nEntities );

	// Returns -1 if the renderable spans more than one area. If it's totally in one area, then this returns the leaf.
	short GetRenderableArea( ClientRenderHandle_t handle );

	// remove renderables from leaves
	void RemoveFromTree( ClientRenderHandle_t handle );
	void InsertIntoTree( ClientRenderHandle_t &handle, const Vector &absMins, const Vector &absMaxs );

	// Adds, removes renderables from view model list
	void AddToViewModelList( ClientRenderHandle_t handle );
	void RemoveFromViewModelList( ClientRenderHandle_t handle );

	// Insert translucent renderables into list of translucent objects
	void InsertTranslucentRenderable( IClientRenderable* pRenderable,
		int& count, IClientRenderable** pList, float* pDist );

	// Adds a shadow to a leaf/removes shadow from renderable
	void AddShadowToRenderable( ClientRenderHandle_t renderHandle, ClientLeafShadowHandle_t shadowHandle );
	void RemoveShadowFromRenderables( ClientLeafShadowHandle_t handle );

	// Adds a shadow to a leaf/removes shadow from renderable
	bool ShouldRenderableReceiveShadow( ClientRenderHandle_t renderHandle, int nShadowFlags );

	// Adds a shadow to a leaf/removes shadow from leaf
	void RemoveShadowFromLeaves( ClientLeafShadowHandle_t handle );

	// Methods related to renderable list building
	void BuildRenderablesListForFastReflections( const SetupRenderInfo_t &info );
	void BuildRenderablesListForCSMView( const SetupRenderInfo_t &info );
		
	int ExtractStaticProps( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractSplitscreenRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractNonFastReflectedRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractTranslucentRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractDuplicates( int nFrameNumber, int nCount, RenderableInfo_t **ppRenderables );
	int ExtractDisableShadowDepthRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractDisableShadowDepthCacheRenderables( int nCount, RenderableInfo_t **ppRenderables );
	void ComputeBounds( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	int ExtractCulledRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, Frustum_t** ppFrustumList );
	int ExtractOccludedRenderables( int occlusionViewId, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	void AddRenderablesToRenderLists( const SetupRenderInfo_t &info, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, int nDetailCount, DetailRenderableInfo_t *pDetailInfo );
	void AddDependentRenderables( const SetupRenderInfo_t &info );

	int ComputeTranslucency( int nFrameNumber, int nViewID, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, const ScreenSizeComputeInfo_t &screenSizeInfo, const Vector& vecViewOrigin );
	void ComputeDistanceFade( int nCount, AlphaInfo_t *pAlphaInfo, BuildRenderListInfo_t *pRLInfo, float flDistScale, const Vector& vecViewOrigin );
	void ComputeScreenFade( const ScreenSizeComputeInfo_t &info, float flMinScreenWidth, float flMaxScreenWidth, int nCount, AlphaInfo_t *pAlphaInfo );

	void CalcRenderableWorldSpaceAABB_Bloated( const RenderableInfo_t &info, Vector &absMin, Vector &absMax );

	// Methods associated with the various bi-directional sets
	static unsigned short& FirstRenderableInLeaf( int leaf ) 
	{ 
		return s_ClientLeafSystem.m_Leaf[leaf].m_FirstElement;
	}

	static unsigned short& FirstLeafInRenderable( unsigned short renderable ) 
	{ 
		return s_ClientLeafSystem.m_Renderables[renderable].m_LeafList;
	}

	static unsigned short& FirstShadowInLeaf( int leaf ) 
	{ 
		return s_ClientLeafSystem.m_Leaf[leaf].m_FirstShadow;
	}

	static unsigned short& FirstLeafInShadow( ClientLeafShadowHandle_t shadow ) 
	{ 
		return s_ClientLeafSystem.m_Shadows[shadow].m_FirstLeaf;
	}

	static unsigned short& FirstShadowOnRenderable( unsigned short renderable ) 
	{ 
		return s_ClientLeafSystem.m_Renderables[renderable].m_FirstShadow;
	}

	static unsigned short& FirstRenderableInShadow( ClientLeafShadowHandle_t shadow ) 
	{ 
		return s_ClientLeafSystem.m_Shadows[shadow].m_FirstRenderable;
	}

	void FrameLock()
	{
		mdlcache->BeginLock();
	}

	void FrameUnlock()
	{
		mdlcache->EndLock();
	}

	// Stores data associated with each leaf.
	std::vector< ClientLeaf_t >	m_Leaf;

	// Stores all unique non-detail renderables
	// Slot handles: ClientRenderHandle_t values index side tables (m_RenderablesInLeaf etc.)
	ks::slot_pool< RenderableInfo_t, ClientRenderHandle_t >	m_Renderables;

	// Information associated with shadows registered with the client leaf system
	ks::slot_pool< ShadowInfo_t, ClientLeafShadowHandle_t >	m_Shadows;

	// Maintains the list of all renderables in a particular leaf
	ks::bidirectional_set< int, ClientRenderHandle_t, unsigned short >	m_RenderablesInLeaf;

	// Maintains a list of all shadows in a particular leaf 
	ks::bidirectional_set< int, ClientLeafShadowHandle_t, unsigned short >	m_ShadowsInLeaf;

	// Maintains a list of all shadows cast on a particular renderable
	ks::bidirectional_set< ClientRenderHandle_t, ClientLeafShadowHandle_t, unsigned short >	m_ShadowsOnRenderable;

	// Dirty list of renderables
	std::vector< ClientRenderHandle_t >	m_DirtyRenderables;

	// List of renderables in view model render groups
	std::vector< ClientRenderHandle_t >	m_ViewModels;

	// Should I draw static props?
	bool m_DrawStaticProps;
	bool m_DrawSmallObjects;
	bool m_bDisableLeafReinsertion;

	// A little enumerator to help us when adding shadows to renderables
	int	m_ShadowEnum;

	// Does anything use alternate sorting?
	int m_nAlternateSortCount;

	// Does anything disable shadow depth
	int m_nDisableShadowDepthCount;

	// Does anything disable shadow depth cache
	int m_nDisableShadowDepthCacheCount;

	// Number of alpha properties out there
	int m_nAlphaPropertyCount;

	ks::fixed_pool m_AlphaPropertyPool;

	int m_nDebugIndex;

	std::recursive_mutex m_DirtyRenderablesMutex;
};

//-----------------------------------------------------------------------------
// Methods of IClientAlphaPropertyMgr
//-----------------------------------------------------------------------------
IClientAlphaProperty *CClientLeafSystem::CreateClientAlphaProperty( IClientUnknown *pUnk )
{
	++m_nAlphaPropertyCount;
	CClientAlphaProperty *pProperty = (CClientAlphaProperty*)m_AlphaPropertyPool.alloc( sizeof(CClientAlphaProperty) );
	Construct( pProperty );
	pProperty->Init( pUnk );
	return pProperty;
}

void CClientLeafSystem::DestroyClientAlphaProperty( IClientAlphaProperty *pAlphaProperty )
{
	if ( !pAlphaProperty )
		return;

	Destruct( static_cast<CClientAlphaProperty*>( pAlphaProperty ) );
	m_AlphaPropertyPool.free( pAlphaProperty );
	contract_assert( m_nAlphaPropertyCount > 0 );
	if ( --m_nAlphaPropertyCount == 0 )
	{
		m_AlphaPropertyPool.clear();
	}
}

//-----------------------------------------------------------------------------
// Expose IClientLeafSystem to the client dll.
//-----------------------------------------------------------------------------
CClientLeafSystem CClientLeafSystem::s_ClientLeafSystem;
IClientLeafSystem *g_pClientLeafSystem = &CClientLeafSystem::s_ClientLeafSystem;
static InterfaceReg s_Reg_CClientLeafSystem_IClientLeafSystem( side_registry(), []() -> void * { return static_cast<IClientLeafSystem *>( &CClientLeafSystem::s_ClientLeafSystem ); }, CLIENTLEAFSYSTEM_INTERFACE_VERSION );;
static InterfaceReg s_Reg_CClientLeafSystem_IClientAlphaPropertyMgr( side_registry(), []() -> void * { return static_cast<IClientAlphaPropertyMgr *>( &CClientLeafSystem::s_ClientLeafSystem ); }, CLIENT_ALPHA_PROPERTY_MGR_INTERFACE_VERSION );;

IClientAlphaPropertyMgr *ClientAlphaPropertyMgr()
{
	return &CClientLeafSystem::s_ClientLeafSystem;
}

void CalcRenderableWorldSpaceAABB_Fast( IClientRenderable *pRenderable, Vector &absMin, Vector &absMax );

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------
static void ClientRenderBoundsWorldspace( IClientRenderable *pRenderable, Vector &absMins, Vector &absMaxs )
{
//	SNPROF("DefaultRenderBoundsWorldspace");

	// Tracker 37433:  This fixes a bug where if the stunstick is being wielded by a combine soldier, the fact that the stick was
	//  attached to the soldier's hand would move it such that it would get frustum culled near the edge of the screen.
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pRenderable->GetIClientUnknown()->GetBaseEntity() );
	if ( pEnt && ( pEnt->IsFollowingEntity() || ( pEnt->GetParentAttachment() > 0 ) ) )
	{
		CBaseEntity *pParent = pEnt->GetMoveParent();
		if ( pParent )
		{
			// Get the parent's abs space world bounds.
			CalcRenderableWorldSpaceAABB_Fast( pParent, absMins, absMaxs );

			// Add the maximum of our local render bounds. This is making the assumption that we can be at any
			// point and at any angle within the parent's world space bounds.
			Vector vAddMins, vAddMaxs;
			pEnt->GetRenderBounds( vAddMins, vAddMaxs );
			// if our origin is actually farther away than that, expand again
			float radius = pEnt->GetLocalOrigin().Length();

			float flBloatSize = std::max( vAddMins.Length(), vAddMaxs.Length() );
			flBloatSize = std::max(flBloatSize, radius);
			absMins -= Vector( flBloatSize, flBloatSize, flBloatSize );
			absMaxs += Vector( flBloatSize, flBloatSize, flBloatSize );
			return;
		}
	}

	Vector mins, maxs;
	pRenderable->GetRenderBounds( mins, maxs );

	// FIXME: Should I just use a sphere here?
	// Another option is to pass the OBB down the tree; makes for a better fit
	// Generate a world-aligned AABB
	const QAngle& angles = pRenderable->GetRenderAngles();
	if (angles == vec3_angle)
	{
		const Vector& origin = pRenderable->GetRenderOrigin();
		VectorAdd( mins, origin, absMins );
		VectorAdd( maxs, origin, absMaxs );
	}
	else
	{
		TransformAABB( pRenderable->RenderableToWorldTransform(), mins, maxs, absMins, absMaxs );
	}
	contract_assert( absMins.IsValid() && absMaxs.IsValid() );
}
static const bool s_bRenderBoundsInstalled = ( g_pfnClientRenderBoundsWorldspace = &ClientRenderBoundsWorldspace, true );

// Figure out a world space bounding box that encloses the entity's local render bounds in world space.
inline void CalcRenderableWorldSpaceAABB( 
	IClientRenderable *pRenderable, 
	Vector &absMins,
	Vector &absMaxs )
{
	pRenderable->GetRenderBoundsWorldspace( absMins, absMaxs );
}

// This gets an AABB for the renderable, but it doesn't cause a parent's bones to be setup.
// This is used for placement in the leaves, but the more expensive version is used for culling.
void CalcRenderableWorldSpaceAABB_Fast( IClientRenderable *pRenderable, Vector &absMin, Vector &absMax )
{
	if ( !pRenderable )
	{
		contract_assert( false && "Cannot calculate WorldSpaceAABB for NULL renderable!\n" );
		return;
	}

	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pRenderable->GetIClientUnknown()->GetBaseEntity() );
	if ( pEnt && ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && ( pEnt->GetParentAttachment() > 0 ) ) ) )
	{
		CBaseEntity *pParent = pEnt->GetMoveParent();
		if ( pParent )
		{
			// Get the parent's abs space world bounds.
			CalcRenderableWorldSpaceAABB_Fast( pParent, absMin, absMax );

			// Add the maximum of our local render bounds. This is making the assumption that we can be at any
			// point and at any angle within the parent's world space bounds.
			Vector vAddMins, vAddMaxs;
			pEnt->GetRenderBounds( vAddMins, vAddMaxs );
			// if our origin is actually farther away than that, expand again
			float radius = pEnt->GetLocalOrigin().Length();

			float flBloatSize = std::max( vAddMins.Length(), vAddMaxs.Length() );
			flBloatSize = std::max(flBloatSize, radius);
			absMin -= Vector( flBloatSize, flBloatSize, flBloatSize );
			absMax += Vector( flBloatSize, flBloatSize, flBloatSize );
		}
	}
	else
	{
		// Start out with our own render bounds. Since we don't have a parent, this won't incur any nasty 
		CalcRenderableWorldSpaceAABB( pRenderable, absMin, absMax );
	}
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CClientLeafSystem::CClientLeafSystem() : m_DrawStaticProps(true), m_DrawSmallObjects(true), 
	m_AlphaPropertyPool( sizeof( CClientAlphaProperty ), 1024, ks::fixed_pool::grow::slow, "CClientAlphaProperty", alignof( CClientAlphaProperty ) ) 
{
	// Set up the bi-directional lists...
	m_RenderablesInLeaf.init( FirstRenderableInLeaf, FirstLeafInRenderable );
	m_ShadowsInLeaf.init( FirstShadowInLeaf, FirstLeafInShadow ); 
	m_ShadowsOnRenderable.init( FirstShadowOnRenderable, FirstRenderableInShadow );
	m_nAlternateSortCount = 0;
	m_nDisableShadowDepthCount = 0;
	m_nDisableShadowDepthCacheCount = 0;
	m_bDisableLeafReinsertion = false;
}

CClientLeafSystem::~CClientLeafSystem()
{
}

//-----------------------------------------------------------------------------
// Activate, deactivate static props
//-----------------------------------------------------------------------------
void CClientLeafSystem::DrawStaticProps( bool enable )
{
	m_DrawStaticProps = enable;
}

void CClientLeafSystem::DrawSmallEntities( bool enable )
{
	m_DrawSmallObjects = enable;
}

void CClientLeafSystem::DisableLeafReinsertion( bool bDisable )
{
	m_bDisableLeafReinsertion = bDisable;
}

//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CClientLeafSystem::LevelInitPreEntity()
{

	m_RenderablesInLeaf.reserve( 1024 );
	m_ShadowsInLeaf.reserve( 256 );
	m_ShadowsOnRenderable.reserve( 256 );
	m_DirtyRenderables.reserve( 256 );

	// Add all the leaves we'll need
	int leafCount = engine->LevelLeafCount();
	m_Leaf.reserve( leafCount );

	ClientLeaf_t newLeaf;
	newLeaf.m_FirstElement = m_RenderablesInLeaf.invalid;
	newLeaf.m_FirstShadow = m_ShadowsInLeaf.invalid;
	memset( newLeaf.m_pSubSystemData, 0, sizeof( newLeaf.m_pSubSystemData ) );
	newLeaf.m_FirstDetailProp = 0;
	newLeaf.m_DetailPropCount = 0;
	newLeaf.m_DetailPropRenderFrame = -1;
	while ( --leafCount >= 0 )
	{
		m_Leaf.push_back( newLeaf );
	}
}

void CClientLeafSystem::LevelShutdownPreEntity()
{
}

void CClientLeafSystem::LevelShutdownPostEntity()
{
	m_nAlternateSortCount = 0;
	m_nDisableShadowDepthCount = 0;
	m_nDisableShadowDepthCacheCount = 0;
	m_ViewModels.clear(); m_ViewModels.shrink_to_fit();
	m_Renderables.clear();
	m_RenderablesInLeaf.clear();
	m_Shadows.clear();

	// delete subsystem data
	for( int i = 0; i < std::ssize( m_Leaf ) ; i++ )
	{
		for( int j = 0 ; j < std::size( m_Leaf[i].m_pSubSystemData ) ; j++ )
		{
			if ( m_Leaf[i].m_pSubSystemData[j] )
			{
				delete m_Leaf[i].m_pSubSystemData[j];
				m_Leaf[i].m_pSubSystemData[j] = nullptr;
			}
		}
	}
	m_Leaf.clear(); m_Leaf.shrink_to_fit();
	m_ShadowsInLeaf.clear();
	m_ShadowsOnRenderable.clear();
	m_DirtyRenderables.clear(); m_DirtyRenderables.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Computes a bloated bounding box to reduce insertions into the tree
//-----------------------------------------------------------------------------
static constexpr auto BBOX_GRANULARITY = 32.0f;
static constexpr auto MIN_SHRINK_VOLUME = ( 32.0f * 32.0f * 32.0f );

void CClientLeafSystem::CalcRenderableWorldSpaceAABB_Bloated( const RenderableInfo_t &info, Vector &absMin, Vector &absMax )
{
	CalcRenderableWorldSpaceAABB_Fast( info.m_pRenderable, absMin, absMax );

	// Bloat bounds to avoid reinsertion into tree
	absMin.x = std::floor( absMin.x / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMin.y = floor( absMin.y / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMin.z = floor( absMin.z / BBOX_GRANULARITY ) * BBOX_GRANULARITY;

	absMax.x = std::ceil( absMax.x / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMax.y = ceil( absMax.y / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMax.z = ceil( absMax.z / BBOX_GRANULARITY ) * BBOX_GRANULARITY;

	// Optimization to make particle systems not re-insert themselves
	if ( info.m_Flags & RENDER_FLAGS_BLOAT_BOUNDS )
	{
		Vector vecTempMin, vecTempMax;
		VectorMin( info.m_vecBloatedAbsMins, absMin, vecTempMin );
		VectorMax( info.m_vecBloatedAbsMaxs, absMax, vecTempMax );
		float flTempVolume = ComputeVolume( vecTempMin, vecTempMax );
		float flCurrVolume = ComputeVolume( absMin, absMax );

		if ( ( flTempVolume <= MIN_SHRINK_VOLUME ) || ( flCurrVolume * 2.0f >= flTempVolume ) )
		{
			absMin = vecTempMin;
			absMax = vecTempMax;
		}
	}
}

//-----------------------------------------------------------------------------
// This is what happens before rendering a particular view
//-----------------------------------------------------------------------------
void CClientLeafSystem::PreRender()
{
//	Assert( m_DirtyRenderables.Count() == 0 );

	// FIXME: This should never need to happen here!
	// At the moment, it's necessary because of the horrid viewmodel/combatweapon
	// confusion in the code where a combat weapon changes its rendering model
	// per view.
	RecomputeRenderableLeaves();
}

// Use this to make sure we're not adding the same renderables to the list while we're going through and re-inserting them into the clientleafsystem
static bool s_bIsInRecomputeRenderableLeaves = false;

void CClientLeafSystem::RecomputeRenderableLeaves()
{
	std::scoped_lock lock( m_DirtyRenderablesMutex );
	int i;
	int nIterations = 0;

	bool bDebugLeafSystem = cl_leafsystemvis.GetBool();

	Vector absMins, absMaxs;
	while ( !m_DirtyRenderables.empty() )
	{
		if ( ++nIterations > 10 )
		{
			Warning( "Too many dirty renderables!\n" );
			break;
		}

		s_bIsInRecomputeRenderableLeaves = true;

		int nDirty = std::ssize( m_DirtyRenderables );
		for ( i = nDirty; --i >= 0; )
		{
			ClientRenderHandle_t handle = m_DirtyRenderables[i];
			RenderableInfo_t &info = m_Renderables[ handle ];

			contract_assert( info.m_Flags & RENDER_FLAGS_HASCHANGED );

			// See note below
			info.m_Flags &= ~RENDER_FLAGS_HASCHANGED;

			if ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS )
				continue;

			CalcRenderableWorldSpaceAABB_Bloated( info, absMins, absMaxs );
			if ( absMins != info.m_vecBloatedAbsMins || absMaxs != info.m_vecBloatedAbsMaxs )
			{
				// Update position in leaf system
				RemoveFromTree( handle );
				InsertIntoTree( m_DirtyRenderables[i], absMins, absMaxs );
				if ( bDebugLeafSystem )
				{
					debugoverlay->AddBoxOverlay( vec3_origin, absMins, absMaxs, QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
				}
			}
		}

		s_bIsInRecomputeRenderableLeaves = false;

		// NOTE: If we get the following error displayed in the console spew
		//       "Re-entrancy found in CClientLeafSystem::RenderableChanged\n"
		//		 We'll have to reenable this code and remove the line that
		//		 removes the RENDER_FLAGS_HASCHANGED in the loop above.
		/*
		for ( i = nDirty; --i >= 0; )
		{
			// Cache off the area it's sitting in.
			ClientRenderHandle_t handle = m_DirtyRenderables[i];
			RenderableInfo_t& renderable = m_Renderables[ handle ];

			renderable.m_Flags &= ~RENDER_FLAGS_HASCHANGED;
		}
		*/

		m_DirtyRenderables.erase( m_DirtyRenderables.begin() + 0, m_DirtyRenderables.begin() + 0 + nDirty );
	}
}

//-----------------------------------------------------------------------------
// Creates a new renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled )
{
	contract_assert( pRenderable );
	contract_assert( pRenderable->RenderHandle() == INVALID_CLIENT_RENDER_HANDLE );

	ClientRenderHandle_t handle = m_Renderables.emplace();
	RenderableInfo_t &info = m_Renderables[handle];

	if ( nModelType == RENDERABLE_MODEL_UNKNOWN_TYPE )
	{
		int nType = modelinfo->GetModelType( pRenderable->GetModel() );
		switch( nType )
		{
		default: nModelType = RENDERABLE_MODEL_ENTITY; break;
		case mod_brush: nModelType = RENDERABLE_MODEL_BRUSH; break;
		case mod_studio: nModelType = RENDERABLE_MODEL_STUDIOMDL; break;
		}
	}
	
	int modelType = modelinfo->GetModelType( pRenderable->GetModel() );

	info.m_Area = -1;
	info.m_pRenderable = pRenderable;
	info.m_pAlphaProperty = static_cast< CClientAlphaProperty* >( pRenderable->GetIClientUnknown()->GetClientAlphaProperty() );
	info.m_FirstShadow = m_ShadowsOnRenderable.invalid;
	info.m_LeafList = m_RenderablesInLeaf.invalid;
	info.m_Flags = ( modelType == mod_sprite ) ? RENDER_FLAGS_IS_SPRITE : 0;
	info.m_bRenderInFastReflection = false;
	info.m_bDisableShadowDepthRendering = false;
	info.m_bDisableCSMRendering = false;
	info.m_bDisableShadowDepthCaching = false;
	info.m_nRenderFrame = -1;
	info.m_EnumCount = 0;
	info.m_nSplitscreenEnabled = nSplitscreenEnabled & 0x3;
	info.m_nTranslucencyType = nType;
	info.m_nModelType = nModelType;
	info.m_vecBloatedAbsMins.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	info.m_vecBloatedAbsMaxs.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
	info.m_vecAbsMins.Init();
	info.m_vecAbsMaxs.Init();

	pRenderable->RenderHandle() = handle;
	RenderWithViewModels( handle, bRenderWithViewModels );
}

//-----------------------------------------------------------------------------
// Call this if the model changes
//-----------------------------------------------------------------------------
void CClientLeafSystem::SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	info.m_nTranslucencyType = nType;
}

RenderableTranslucencyType_t CClientLeafSystem::GetTranslucencyType( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return RENDERABLE_IS_OPAQUE;

	const RenderableInfo_t &info = m_Renderables[handle];
	return (RenderableTranslucencyType_t)info.m_nTranslucencyType;
}

void CClientLeafSystem::RenderInFastReflections( ClientRenderHandle_t handle, bool bRenderInFastReflections )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	info.m_bRenderInFastReflection = bRenderInFastReflections;
}

bool CClientLeafSystem::IsRenderingInFastReflections( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return false;

	const RenderableInfo_t &info = m_Renderables[handle];
	return info.m_bRenderInFastReflection;
}

void CClientLeafSystem::DisableShadowDepthRendering( ClientRenderHandle_t handle, bool bDisable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bDisable != info.m_bDisableShadowDepthRendering )
	{
		info.m_bDisableShadowDepthRendering = bDisable;
		m_nDisableShadowDepthCount += bDisable ? 1 : -1;
		contract_assert( m_nDisableShadowDepthCount >= 0 );
	}
}

void CClientLeafSystem::DisableCSMRendering( ClientRenderHandle_t handle, bool bDisable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bDisable != info.m_bDisableCSMRendering )
	{
		info.m_bDisableCSMRendering = bDisable;
		m_nDisableShadowDepthCount += bDisable ? 1 : -1;
		contract_assert( m_nDisableShadowDepthCount >= 0 );
	}
}

void CClientLeafSystem::DisableShadowDepthCaching( ClientRenderHandle_t handle, bool bDisable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bDisable != info.m_bDisableShadowDepthCaching )
	{
		info.m_bDisableShadowDepthCaching = bDisable;
		m_nDisableShadowDepthCacheCount += bDisable ? 1 : -1;
		contract_assert( m_nDisableShadowDepthCacheCount >= 0 );
	}
}

void CClientLeafSystem::EnableSplitscreenRendering( ClientRenderHandle_t handle, uint32 nFlags )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	info.m_nSplitscreenEnabled = nFlags & 0x3;
}

void CClientLeafSystem::SetModelType( ClientRenderHandle_t handle, RenderableModelType_t nModelType )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( nModelType == RENDERABLE_MODEL_UNKNOWN_TYPE )
	{
		int nType = modelinfo->GetModelType( info.m_pRenderable->GetModel() );
		switch( nType )
		{
		default: nModelType = RENDERABLE_MODEL_ENTITY; break;
		case mod_brush: nModelType = RENDERABLE_MODEL_BRUSH; break;
		case mod_studio: nModelType = RENDERABLE_MODEL_STUDIOMDL; break;
		}
	}

	if ( info.m_nModelType != nModelType )
	{
		info.m_nModelType = nModelType;
		RenderableChanged( handle );
	}
}

void CClientLeafSystem::EnableRendering( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		info.m_Flags &= ~RENDER_FLAGS_DISABLE_RENDERING;
	}
	else
	{
		info.m_Flags |= RENDER_FLAGS_DISABLE_RENDERING;
	}
}

void CClientLeafSystem::EnableBloatedBounds( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		info.m_Flags |= RENDER_FLAGS_BLOAT_BOUNDS;
	}
	else
	{
		if ( info.m_Flags & RENDER_FLAGS_BLOAT_BOUNDS )
		{
			info.m_Flags &= ~RENDER_FLAGS_BLOAT_BOUNDS;

			// Necessary to generate unbloated bounds later
			RenderableChanged( handle );
		}
	}
}

void CClientLeafSystem::DisableCachedRenderBounds( ClientRenderHandle_t handle, bool bDisable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bDisable )
	{
		info.m_Flags |= RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE;
	}
	else
	{
		info.m_Flags &= ~RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE;
	}
}

//-----------------------------------------------------------------------------
// Draw translucent objects in the opaque renderables pass
//-----------------------------------------------------------------------------
void CClientLeafSystem::EnableForceOpaquePass( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		if ( (info.m_Flags & RENDER_FLAGS_FORCE_OPAQUE_PASS) == 0 )
		{
			info.m_Flags |= RENDER_FLAGS_FORCE_OPAQUE_PASS;
		}
	}
	else
	{
		if ( (info.m_Flags & RENDER_FLAGS_FORCE_OPAQUE_PASS) != 0 )
		{
			info.m_Flags &= ~RENDER_FLAGS_FORCE_OPAQUE_PASS;
		}
	}
}

bool CClientLeafSystem::IsEnableForceOpaquePass( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return false;
	return (m_Renderables[handle].m_Flags & RENDER_FLAGS_FORCE_OPAQUE_PASS) != 0;
}

//-----------------------------------------------------------------------------
// Use alternate translucent sorting algorithm (draw translucent objects in the furthest leaf they lie in)
//-----------------------------------------------------------------------------
void CClientLeafSystem::EnableAlternateSorting( ClientRenderHandle_t handle, bool bEnable )
{
	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		if ( ( info.m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) == 0 )
		{
			++m_nAlternateSortCount;
			info.m_Flags |= RENDER_FLAGS_ALTERNATE_SORTING;
		}
	}
	else
	{
		if ( ( info.m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) != 0 )
		{
			--m_nAlternateSortCount;
			info.m_Flags &= ~RENDER_FLAGS_ALTERNATE_SORTING; 
		}
	}
}

//-----------------------------------------------------------------------------
// Should this render with viewmodels?
//-----------------------------------------------------------------------------
void CClientLeafSystem::RenderWithViewModels( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		if ( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) == 0 )
		{
			info.m_Flags |= RENDER_FLAGS_RENDER_WITH_VIEWMODELS;
			AddToViewModelList( handle );
			RemoveFromTree( handle );
		}
	}
	else
	{
		if ( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) != 0 )
		{
			info.m_Flags &= ~RENDER_FLAGS_RENDER_WITH_VIEWMODELS; 
			RemoveFromViewModelList( handle );
			info.m_vecBloatedAbsMins.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
			info.m_vecBloatedAbsMaxs.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
			RenderableChanged( handle );
		}
	}
}

bool CClientLeafSystem::IsRenderingWithViewModels( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return false;
	return ( m_Renderables[handle].m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) != 0;
}

//-----------------------------------------------------------------------------
// Add/remove renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderable( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled )
{
	// force a relink we we try to draw it for the first time
	CreateRenderableHandle( pRenderable, bRenderWithViewModels, nType, nModelType, nSplitscreenEnabled );
	ClientRenderHandle_t handle = pRenderable->RenderHandle();
	RenderableChanged( handle );
}

void CClientLeafSystem::RemoveRenderable( ClientRenderHandle_t handle )
{
	std::scoped_lock lock( m_DirtyRenderablesMutex );

	// This can happen upon level shutdown
	if (!m_Renderables.contains(handle))
		return;

	// Reset the render handle in the entity.
	IClientRenderable *pRenderable = m_Renderables[handle].m_pRenderable;
	contract_assert( handle == pRenderable->RenderHandle() );
	pRenderable->RenderHandle() = INVALID_CLIENT_RENDER_HANDLE;

	int nFlags = m_Renderables[handle].m_Flags;
	if ( nFlags & RENDER_FLAGS_ALTERNATE_SORTING )
	{
		--m_nAlternateSortCount;
	}

	if ( m_Renderables[handle].m_bDisableShadowDepthRendering )
	{
		--m_nDisableShadowDepthCount;
	}

	if ( m_Renderables[handle].m_bDisableShadowDepthCaching )
	{
		--m_nDisableShadowDepthCacheCount;
	}

	// Remove the renderable from the dirty list
	if ( nFlags & RENDER_FLAGS_HASCHANGED )
	{
		// NOTE: This isn't particularly fast (linear search),
		// but I'm assuming it's an unusual case where we remove 
		// renderables that are changing or that m_DirtyRenderables usually
		// only has a couple entries
		int i = ks::find_index( m_DirtyRenderables, handle );
		contract_assert( i != -1 );
		if ( ks::is_valid_index( m_DirtyRenderables, i ) )
		{
			ks::swap_remove( m_DirtyRenderables, i ); 
		}
	}

	if ( IsRenderingWithViewModels( handle ) )
	{
		RemoveFromViewModelList( handle );
	}

	RemoveFromTree( handle );
	m_Renderables.erase( handle );
}

int CClientLeafSystem::GetRenderableLeaves( ClientRenderHandle_t handle, int leaves[128] )
{
	if ( !m_Renderables.contains( handle ) )
		return -1;

	RenderableInfo_t *pRenderable = &m_Renderables[handle];
	if ( pRenderable->m_LeafList == m_RenderablesInLeaf.invalid )
		return -1;

	int nLeaves = 0;
	for ( int i=m_RenderablesInLeaf.first_bucket( handle ); i != m_RenderablesInLeaf.invalid; i = m_RenderablesInLeaf.next_bucket( i ) )
	{
		leaves[nLeaves++] = m_RenderablesInLeaf.bucket( i );
		if ( nLeaves >= 128 )
			break;
	}
	return nLeaves;
}

//-----------------------------------------------------------------------------
// Retrieve leaf handles to leaves a renderable is in
// the pOutLeaf parameter is filled with the leaf the renderable is in.
// If pInIterator is not specified, pOutLeaf is the first leaf in the list.
// if pInIterator is specified, that iterator is used to return the next leaf
// in the list in pOutLeaf.
// the pOutIterator parameter is filled with the iterater which index to the pOutLeaf returned.
//
// Returns false on failure cases where pOutLeaf will be invalid. CHECK THE RETURN!
//-----------------------------------------------------------------------------
bool CClientLeafSystem::GetRenderableLeaf(ClientRenderHandle_t handle, int* pOutLeaf, const int* pInIterator /* = 0 */, int* pOutIterator /* = 0  */)
{
	// bail on invalid handle
	if ( !m_Renderables.contains( handle ) )
		return false;

	// bail on no output value pointer
	if ( !pOutLeaf )
		return false;

	// an iterator was specified
	if ( pInIterator )
	{
		int iter = *pInIterator;

		// test for invalid iterator
		if ( iter == m_RenderablesInLeaf.invalid )
			return false;

		int iterNext =  m_RenderablesInLeaf.next_bucket( iter );

		// test for end of list
		if ( iterNext == m_RenderablesInLeaf.invalid )
			return false;

		// Give the caller the iterator used
		if ( pOutIterator )
		{
			*pOutIterator = iterNext;
		}
		
		// set output value to the next leaf
		*pOutLeaf = m_RenderablesInLeaf.bucket( iterNext );

	}
	else // no iter param, give them the first bucket in the renderable's list
	{
		int iter = m_RenderablesInLeaf.first_bucket( handle );

		if ( iter == m_RenderablesInLeaf.invalid )
			return false;

		// Set output value to this leaf
		*pOutLeaf = m_RenderablesInLeaf.bucket( iter );

		// give this iterator to caller
		if ( pOutIterator )
		{
			*pOutIterator = iter;
		}
		
	}
	
	return true;
}

bool CClientLeafSystem::IsRenderableInPVS( IClientRenderable *pRenderable )
{
	ClientRenderHandle_t handle = pRenderable->RenderHandle();
	int leaves[128];
	int nLeaves = GetRenderableLeaves( handle, leaves );
	if ( nLeaves == -1 )
		return false;

	// Ask the engine if this guy is visible.
	return render->AreAnyLeavesVisible( leaves, nLeaves );
}

void CClientLeafSystem::SetSubSystemDataInLeaf( int leaf, int nSubSystemIdx, CClientLeafSubSystemData *pData )
{
	contract_assert( nSubSystemIdx < N_CLSUBSYSTEMS );
	if ( m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx] )
		delete m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx];
	m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx] = pData;
}

CClientLeafSubSystemData *CClientLeafSystem::GetSubSystemDataInLeaf( int leaf, int nSubSystemIdx )
{
	contract_assert( nSubSystemIdx < N_CLSUBSYSTEMS );
	return m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx];
}

//-----------------------------------------------------------------------------
// Indicates which leaves detail objects are in
//-----------------------------------------------------------------------------
void CClientLeafSystem::SetDetailObjectsInLeaf( int leaf, int firstDetailObject,
											    int detailObjectCount )
{
	m_Leaf[leaf].m_FirstDetailProp = firstDetailObject;
	m_Leaf[leaf].m_DetailPropCount = detailObjectCount;
	if ( detailObjectCount )
		engine->SetLeafFlag( leaf, LEAF_FLAGS_CONTAINS_DETAILOBJECTS );	// for fast searches
}

//-----------------------------------------------------------------------------
// Returns the detail objects in a leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::GetDetailObjectsInLeaf( int leaf, int& firstDetailObject,
											    int& detailObjectCount )
{
	firstDetailObject = m_Leaf[leaf].m_FirstDetailProp;
	detailObjectCount = m_Leaf[leaf].m_DetailPropCount;
}

//-----------------------------------------------------------------------------
// Create/destroy shadows...
//-----------------------------------------------------------------------------
ClientLeafShadowHandle_t CClientLeafSystem::AddShadow( ClientShadowHandle_t userId, unsigned short flags )
{
	ClientLeafShadowHandle_t idx = m_Shadows.emplace();
	m_Shadows[idx].m_Shadow = userId;
	m_Shadows[idx].m_FirstLeaf = m_ShadowsInLeaf.invalid;
	m_Shadows[idx].m_FirstRenderable = m_ShadowsOnRenderable.invalid;
	m_Shadows[idx].m_EnumCount = 0;
	m_Shadows[idx].m_Flags = flags;
	return idx;
}

void CClientLeafSystem::RemoveShadow( ClientLeafShadowHandle_t handle )
{
	// Remove the shadow from all leaves + renderables...
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	// Blow away the handle
	m_Shadows.erase( handle );
}

//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from renderable
//-----------------------------------------------------------------------------
bool CClientLeafSystem::ShouldRenderableReceiveShadow( ClientRenderHandle_t renderHandle, int nShadowFlags )
{
	RenderableInfo_t &renderable = m_Renderables[renderHandle];
	if ( renderable.m_nModelType == RENDERABLE_MODEL_ENTITY )
		return false;

	return renderable.m_pRenderable->ShouldReceiveProjectedTextures( nShadowFlags );
}

//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddShadowToRenderable( ClientRenderHandle_t renderHandle, 
										ClientLeafShadowHandle_t shadowHandle )
{
	// Check if this renderable receives the type of projected texture that shadowHandle refers to.
	int nShadowFlags = m_Shadows[shadowHandle].m_Flags;
	if ( !ShouldRenderableReceiveShadow( renderHandle, nShadowFlags ) )
		return;

	m_ShadowsOnRenderable.add( renderHandle, shadowHandle );

	// Also, do some stuff specific to the particular types of renderables
	// Do AddShadowToReceiver to avoid branching
	static const byte arrRecvType[0x4] = {
		0,
		SHADOW_RECEIVER_STUDIO_MODEL,
		SHADOW_RECEIVER_STATIC_PROP,
		SHADOW_RECEIVER_BRUSH_MODEL
	};
	static_assert( RENDERABLE_MODEL_STUDIOMDL == 1 );
	static_assert( RENDERABLE_MODEL_STATIC_PROP == 2 );
	static_assert( RENDERABLE_MODEL_BRUSH == 3 );

	RenderableInfo_t const &ri = m_Renderables[renderHandle];
	if ( ri.m_nModelType < std::size( arrRecvType ) )
	{
		g_pClientShadowMgr->AddShadowToReceiver(
			m_Shadows[shadowHandle].m_Shadow,
			ri.m_pRenderable,
			( ShadowReceiver_t ) arrRecvType[ ri.m_nModelType ] );
	}
}

void CClientLeafSystem::RemoveShadowFromRenderables( ClientLeafShadowHandle_t handle )
{
	m_ShadowsOnRenderable.remove_element( handle );
}

//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddShadowToLeaf( int leaf, ClientLeafShadowHandle_t shadow, bool bFlashlight )
{
	m_ShadowsInLeaf.add( leaf, shadow ); 

	if ( !( bFlashlight || r_shadows_on_renderables_enable.GetBool() ) )
	{
		return;
	}

	// Add the shadow exactly once to all renderables in the leaf
	unsigned short i = m_RenderablesInLeaf.first_element( leaf );
	while ( i != m_RenderablesInLeaf.invalid )
	{
		ClientRenderHandle_t renderable = m_RenderablesInLeaf.element(i);
		RenderableInfo_t& info = m_Renderables[renderable];

		// Add each shadow exactly once to each renderable
		if (info.m_EnumCount != m_ShadowEnum)
		{
			AddShadowToRenderable( renderable, shadow );
			info.m_EnumCount = m_ShadowEnum;
		}

		i = m_RenderablesInLeaf.next_element(i);
	}
}

void CClientLeafSystem::RemoveShadowFromLeaves( ClientLeafShadowHandle_t handle )
{
	m_ShadowsInLeaf.remove_element( handle );
}

//-----------------------------------------------------------------------------
// Adds a shadow to all leaves listed
//-----------------------------------------------------------------------------
void CClientLeafSystem::ProjectShadow( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList )
{
	// Remove the shadow from any leaves it current exists in
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	contract_assert( ( m_Shadows[handle].m_Flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) == SHADOW_FLAGS_SHADOW );

	// This will help us to avoid adding the shadow multiple times to a renderable
	++m_ShadowEnum;

	for ( int i = 0; i < nLeafCount; ++i )
	{
		AddShadowToLeaf( pLeafList[i], handle, false );
	}
}

void CClientLeafSystem::ProjectFlashlight( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList )
{
	CVProfScope _( "CClientLeafSystem::ProjectFlashlight", 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );

	// Remove the shadow from any leaves it current exists in
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	contract_assert( ( m_Shadows[handle].m_Flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) != 0 );
	
	// This will help us to avoid adding the shadow multiple times to a renderable
	++m_ShadowEnum;

	for ( int i = 0; i < nLeafCount; ++i )
	{
		AddShadowToLeaf( pLeafList[i], handle, true );
	}
}

//-----------------------------------------------------------------------------
// Find all shadow casters in a set of leaves
//-----------------------------------------------------------------------------
void CClientLeafSystem::EnumerateShadowsInLeaves( int leafCount, WorldListLeafData_t* pLeaves, IClientLeafShadowEnum* pEnum )
{
	if (leafCount == 0)
		return;

	// This will help us to avoid enumerating the shadow multiple times
	++m_ShadowEnum;

	for (int i = 0; i < leafCount; ++i)
	{
		int leaf = pLeaves[i].leafIndex;

		unsigned short j = m_ShadowsInLeaf.first_element( leaf );
		while ( j != m_ShadowsInLeaf.invalid )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.element(j);
			ShadowInfo_t& info = m_Shadows[shadow];

			if (info.m_EnumCount != m_ShadowEnum)
			{
				pEnum->EnumShadow(info.m_Shadow);
				info.m_EnumCount = m_ShadowEnum;
			}

			j = m_ShadowsInLeaf.next_element(j);
		}
	}
}

//-----------------------------------------------------------------------------
// Adds a renderable to a leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderableToLeaf( int leaf, ClientRenderHandle_t renderable, bool bReceiveShadows )
{
	m_RenderablesInLeaf.add( leaf, renderable );

	bool bShadowsOnRenderables = r_shadows_on_renderables_enable.GetBool();

	if ( !bReceiveShadows )
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( bShadowsOnRenderables )
	{
		// skipping this code entirely is only safe with single-pass flashlight (i.e. on the 360)

		// Add all shadows in the leaf to the renderable...
		unsigned short i = m_ShadowsInLeaf.first_element( leaf );
		while ( i != m_ShadowsInLeaf.invalid )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.element(i);
			ShadowInfo_t& info = m_Shadows[shadow];

			// Add each shadow exactly once to each renderable
			if ( info.m_EnumCount != m_ShadowEnum )
			{
				AddShadowToRenderable( renderable, shadow );
				info.m_EnumCount = m_ShadowEnum;
			}

			i = m_ShadowsInLeaf.next_element(i);
		}
	}
	else 	{
		// for non-singlepass flashlight (i.e. PC) we need to still add all flashlights to the renderable
		// OR if we're culling individual objects with single pass flashlight
		
		// Add all flashlights in the leaf to the renderable...
		unsigned short i = m_ShadowsInLeaf.first_element( leaf );
		while ( i != m_ShadowsInLeaf.invalid )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.element(i);
			ShadowInfo_t& info = m_Shadows[shadow];

			// Add each flashlight exactly once to each renderable
			if ( ( info.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) && ( info.m_EnumCount != m_ShadowEnum ) )
			{
				AddShadowToRenderable( renderable, shadow );
				info.m_EnumCount = m_ShadowEnum;
			}

			i = m_ShadowsInLeaf.next_element(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Adds a renderable to a set of leaves
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves, bool bReceiveShadows )
{ 
	for (int j = 0; j < nLeafCount; ++j)
	{
		AddRenderableToLeaf( pLeaves[j], handle, bReceiveShadows ); 
	}

	m_Renderables[handle].m_Area = engine->GetLeavesArea( pLeaves, nLeafCount );
}

void CClientLeafSystem::AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves )
{ 
	bool bReceiveShadows = ShouldRenderableReceiveShadow( handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );
	AddRenderableToLeaves( handle, nLeafCount, pLeaves, bReceiveShadows );
}

//-----------------------------------------------------------------------------
// Inserts an element into the tree
//-----------------------------------------------------------------------------
bool CClientLeafSystem::EnumerateLeaf( int leaf, intp context )
{
	EnumResultList_t *pList = (EnumResultList_t *)context;
	if ( ThreadInMainThread() )
	{
		bool bReceiveShadows = ShouldRenderableReceiveShadow( pList->handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );
		AddRenderableToLeaf( leaf, pList->handle, bReceiveShadows );
	}
	else
	{
		EnumResult_t *p = new EnumResult_t;
		p->leaf = leaf;
		p->pNext = pList->pHead;
		pList->pHead = p;
	}
	return true;
}

void CClientLeafSystem::InsertIntoTree( ClientRenderHandle_t &handle, const Vector &absMins, const Vector &absMaxs )
{
	// NOTE: The render bounds here are relative to the renderable's coordinate system
	RenderableInfo_t &info = m_Renderables[handle];
	contract_assert( absMins.IsValid() && absMaxs.IsValid() );

	contract_assert( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) == 0 );
	contract_assert( ThreadInMainThread() );

	info.m_vecBloatedAbsMins = absMins;
	info.m_vecBloatedAbsMaxs = absMaxs;

	// When we insert into the tree, increase the shadow enumerator
	// to make sure each shadow is added exactly once to each renderable
	m_ShadowEnum++;
	unsigned short leafList[1024];
	ISpatialQuery* pQuery = engine->GetBSPTreeQuery();
	int leafCount = pQuery->ListLeavesInBox( absMins, absMaxs, leafList, std::size( leafList ) );
	bool bReceiveShadows = ShouldRenderableReceiveShadow( handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( cl_leafsystemvis.GetBool() )
	{
		char pTemp[256];
		const char *pClassName = "<unknown renderable>";
		CBaseEntity *pEnt = EntityCast<CBaseEntity>( info.m_pRenderable->GetIClientUnknown()->GetBaseEntity() );
		if ( pEnt )
		{
			pClassName = pEnt->GetClassname();
		}
		else
		{
			CNewParticleEffect *pEffect = dynamic_cast< CNewParticleEffect*>( info.m_pRenderable );
			if ( pEffect )
			{
				V_snprintf( pTemp, sizeof(pTemp), "ps: %s", pEffect->GetName() );
				pClassName = pTemp;
			}
			else if ( dynamic_cast< CParticleEffectBinding* >( info.m_pRenderable ) )
			{
				pClassName = "<old particle system>";
			}
		}

		con_nprint_t np;
		np.time_to_live = 0.1f;
		np.fixed_width_font = true;
		np.color[0] = 1.0;
		np.color[1] = 0.8;
		np.color[2] = 0.1;
		np.index = m_nDebugIndex++;

		engine->Con_NXPrintf( &np, "%s", pClassName );
	}

	AddRenderableToLeaves( handle, leafCount, leafList, bReceiveShadows );
}

//-----------------------------------------------------------------------------
// Removes an element from the tree
//-----------------------------------------------------------------------------
void CClientLeafSystem::RemoveFromTree( ClientRenderHandle_t handle )
{
	m_RenderablesInLeaf.remove_element( handle );

	// Remove all shadows cast onto the object
	m_ShadowsOnRenderable.remove_bucket( handle );

	switch( m_Renderables[handle].m_nModelType )
	{
	case RENDERABLE_MODEL_BRUSH:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_BRUSH_MODEL );
		break;

	case RENDERABLE_MODEL_STATIC_PROP:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_STATIC_PROP );
		break;

	case RENDERABLE_MODEL_STUDIOMDL:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_STUDIO_MODEL );
		break;
	}

}

//-----------------------------------------------------------------------------
// Call this when the renderable moves
//-----------------------------------------------------------------------------
void CClientLeafSystem::RenderableChanged( ClientRenderHandle_t handle )
{
	std::scoped_lock lock( m_DirtyRenderablesMutex );

	// This should not be called during view rendering
//	Assert( !m_bDisableLeafReinsertion );

	contract_assert( handle != INVALID_CLIENT_RENDER_HANDLE );
	contract_assert( m_Renderables.contains( handle ) );
	if ( !m_Renderables.contains( handle ) )
		return;

	RenderableInfo_t &info = m_Renderables[handle]; 
	info.m_Flags &= ~RENDER_FLAGS_BOUNDS_VALID;
	if ( ( info.m_Flags & RENDER_FLAGS_HASCHANGED ) == 0 )
	{
		info.m_Flags |= RENDER_FLAGS_HASCHANGED;
		m_DirtyRenderables.push_back( handle );
	}
//#if _DEBUG
	else
	{
		if ( s_bIsInRecomputeRenderableLeaves )
		{
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "Re-entrancy found in CClientLeafSystem::RenderableChanged\n" );
			Warning( "Contact Shanon or Brian\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
		}
		// It had better be in the list
		contract_assert( ks::find_index( m_DirtyRenderables, handle ) != -1 );
	}
//#endif
}

//-----------------------------------------------------------------------------
// Adds, removes renderables from view model list
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddToViewModelList( ClientRenderHandle_t handle )
{
	contract_assert( ks::find_index( m_ViewModels, handle ) == -1 );
	m_ViewModels.push_back( handle );
}

void CClientLeafSystem::RemoveFromViewModelList( ClientRenderHandle_t handle )
{
	int i = ks::find_index( m_ViewModels, handle );
	contract_assert( i != -1 );
	ks::swap_remove( m_ViewModels, i );
}

//-----------------------------------------------------------------------------
// Detail system marks 
//-----------------------------------------------------------------------------
void CClientLeafSystem::DrawDetailObjectsInLeaf( int leaf, int nFrameNumber, int& nFirstDetailObject, int& nDetailObjectCount )
{
	ClientLeaf_t &leafInfo = m_Leaf[leaf];
	leafInfo.m_DetailPropRenderFrame = nFrameNumber;
	nFirstDetailObject = leafInfo.m_FirstDetailProp;
	nDetailObjectCount = leafInfo.m_DetailPropCount;
}

//-----------------------------------------------------------------------------
// Are we close enough to this leaf to draw detail props *and* are there any props in the leaf?
//-----------------------------------------------------------------------------
bool CClientLeafSystem::ShouldDrawDetailObjectsInLeaf( int leaf, int frameNumber )
{
	ClientLeaf_t &leafInfo = m_Leaf[leaf];
	return ( (leafInfo.m_DetailPropRenderFrame == frameNumber ) &&
			 ( ( leafInfo.m_DetailPropCount != 0 ) || ( leafInfo.m_pSubSystemData[CLSUBSYSTEM_DETAILOBJECTS] ) ) );
}

//-----------------------------------------------------------------------------
// Compute which leaf the translucent renderables should render in
//-----------------------------------------------------------------------------
#define LeafToMarker( leaf ) reinterpret_cast<RenderableInfo_t *>(( (leaf) << 1 ) | 1)
#define IsLeafMarker( p ) (bool)((reinterpret_cast<size_t>(p)) & 1)
#define MarkerToLeaf( p ) (int)((reinterpret_cast<size_t>(p)) >> 1)

//-----------------------------------------------------------------------------
// Adds a renderable to the list of renderables to render this frame
//-----------------------------------------------------------------------------
inline void AddRenderableToRenderList( CClientRenderablesList &renderList, IClientRenderable *pRenderable, 
	int iLeaf, RenderGroup_t group,	int nModelType, uint8 nAlphaModulation, bool bShadowDepthNoCache, bool bTwoPass = false )
{

	contract_assert( group >= 0 && group < RENDER_GROUP_COUNT );
	
	int &curCount = renderList.m_RenderGroupCounts[group];
	if ( curCount < CClientRenderablesList::MAX_GROUP_ENTITIES )
	{
		contract_assert( (iLeaf >= 0) && (iLeaf <= 65535) );

		CClientRenderablesList::CEntry *pEntry = &renderList.m_RenderGroups[group][curCount];
		pEntry->m_pRenderable = pRenderable;
		pEntry->m_iWorldListInfoLeaf = iLeaf;
		pEntry->m_nModelType = nModelType;
		pEntry->m_bShadowDepthNoCache = bShadowDepthNoCache;
		pEntry->m_TwoPass = bTwoPass;
		pEntry->m_bIsCombinedModel = false;
		if ( nModelType == RENDERABLE_MODEL_STUDIOMDL )
		{
			const model_t	*pModel = pRenderable->GetModel();
			MDLHandle_t		Handle = modelinfo->GetCacheHandle( pModel );
			bool			bIsCombined = g_pMDLCache->IsCombinedModel( Handle );
			pEntry->m_bIsCombinedModel = bIsCombined;
		}

		pEntry->m_InstanceData.m_nAlpha = nAlphaModulation;
		curCount++;
	}
	else
	{
		engine->Con_NPrintf( 10, "Warning: overflowed CClientRenderablesList group %d", group );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : renderList - 
//			renderGroup - 
//-----------------------------------------------------------------------------
void CClientLeafSystem::CollateViewModelRenderables( CViewModelRenderablesList *pList )
{
	bool bIsSplitScreenActive = engine->IsSplitScreenActive();
	int nSlotMask = 0;
	if ( bIsSplitScreenActive )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlotMask = 1 << GET_ACTIVE_SPLITSCREEN_SLOT();
	}

	CViewModelRenderablesList::RenderGroups_t &opaqueList = pList->m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentList = pList->m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	for ( int i = std::ssize( m_ViewModels )-1; i >= 0; --i )
	{
		ClientRenderHandle_t handle = m_ViewModels[i];
		RenderableInfo_t& renderable = m_Renderables[handle];
				  
		// Early out on splitscreen renderables if we don't want to render them
		if ( bIsSplitScreenActive )
		{
			if ( ( renderable.m_nSplitscreenEnabled & nSlotMask ) == 0 )
				continue;
		}

		int nAlpha = renderable.m_pAlphaProperty ? renderable.m_pAlphaProperty->ComputeRenderAlpha( ) : 255;
		bool bIsTransparent = ( nAlpha != 255 ) || ( renderable.m_nTranslucencyType != RENDERABLE_IS_OPAQUE );

		// That's why we need to test RENDER_GROUP_OPAQUE_ENTITY - it may have changed in ComputeFXBlend()
		if ( !bIsTransparent )
		{
			int nOpaqueIndex = std::ssize( opaqueList ); opaqueList.emplace_back();
			CViewModelRenderablesList::CEntry *pOpaqueEntry = &opaqueList[nOpaqueIndex];
			pOpaqueEntry->m_pRenderable = renderable.m_pRenderable;
			pOpaqueEntry->m_InstanceData.m_nAlpha = 255;
			pOpaqueEntry->m_InstanceData.m_bTwoPass = false;
		}
		else
		{
			int i = std::ssize( translucentList ); translucentList.emplace_back();
			CViewModelRenderablesList::CEntry *pTranslucentEntry = &translucentList[i];
			pTranslucentEntry->m_pRenderable = renderable.m_pRenderable;
			pTranslucentEntry->m_InstanceData.m_nAlpha = nAlpha;
			if ( renderable.m_nTranslucencyType == RENDERABLE_IS_TWO_PASS )
			{
				pTranslucentEntry->m_InstanceData.m_bTwoPass = true;
				int nOpaqueIndex = std::ssize( opaqueList ); opaqueList.emplace_back();
				CViewModelRenderablesList::CEntry *pOpaqueEntry = &opaqueList[nOpaqueIndex];
				pOpaqueEntry->m_pRenderable = renderable.m_pRenderable;
				pOpaqueEntry->m_InstanceData.m_nAlpha = 255;
				pOpaqueEntry->m_InstanceData.m_bTwoPass = true;
			}
			else
			{
				pTranslucentEntry->m_InstanceData.m_bTwoPass = false;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CClientLeafSystem::SortEntities( const Vector &vecRenderOrigin, const Vector &vecRenderForward, 
	CClientRenderablesList::CEntry *pEntities, BuildRenderListInfo_t **pTranslucentRLInfo, int nEntities )
{
	// Don't sort if we only have 1 entity
	if ( nEntities <= 1 )
		return;

	float dists[CClientRenderablesList::MAX_GROUP_ENTITIES];

	// First get a distance for each entity.
	int i;

	Vector boxcenter;
	Vector delta;
	for( i=0; i < nEntities; i++ )
	{
		VectorAdd( pTranslucentRLInfo[i]->m_vecMins, pTranslucentRLInfo[i]->m_vecMaxs, boxcenter );
		boxcenter *= 0.5f;

		// Compute distance...
		VectorSubtract( boxcenter, vecRenderOrigin, delta );
		dists[i] = DotProduct( delta, vecRenderForward );
	}

	// H-sort.
	int stepSize = 4;
	while( stepSize )
	{
		int end = nEntities - stepSize;
		for( i=0; i < end; i += stepSize )
		{
			if( dists[i] > dists[i+stepSize] )
			{
				V_swap( pEntities[i], pEntities[i+stepSize] );
				V_swap( dists[i], dists[i+stepSize] );

				if( i == 0 )
				{
					i = -stepSize;
				}
				else
				{
					i -= stepSize << 1;
				}
			}
		}

		stepSize >>= 1;
	}
}

//-----------------------------------------------------------------------------
// Extracts static props from the list of renderables
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractStaticProps( int nCount, RenderableInfo_t **ppRenderables )
{
	if ( m_DrawStaticProps )
		return nCount;

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			// Early out on static props if we don't want to render them
			if ( pInfo->m_nModelType == RENDERABLE_MODEL_STATIC_PROP )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Extracts renderables that are excluded in splitscreen
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractSplitscreenRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	if ( !IsSplitScreenSupported() )
		return nCount;

	if ( !engine->IsSplitScreenActive() )
		return nCount;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlotMask = 1 << GET_ACTIVE_SPLITSCREEN_SLOT();

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			// Early out on splitscreen renderables if we don't want to render them
			if ( ( pInfo->m_nSplitscreenEnabled & nSlotMask ) == 0 )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Extracts models which are *not* marked for "fast reflections"
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractNonFastReflectedRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	int nReflectedCount = 0;
	for ( int i = 0; i < nCount; ++ i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			if ( !pInfo->m_bRenderInFastReflection )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		
		ppRenderables[nReflectedCount++] = pInfo;
	}

	return nReflectedCount;
}

//-----------------------------------------------------------------------------
// Extracts models which are *not* marked for "fast reflections"
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractDisableShadowDepthRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	if ( m_nDisableShadowDepthCount == 0 )
		return nCount;

	int nNewCount = 0;
	for ( int i = 0; i < nCount; ++ i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			if ( pInfo->m_bDisableShadowDepthRendering )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}

		ppRenderables[nNewCount++] = pInfo;
	}

	return nNewCount;
}

//-----------------------------------------------------------------------------
// Extracts models which are cacheable or not depending on what we render now
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractDisableShadowDepthCacheRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	int nNewCount = 0;
	for ( int i = 0; i < nCount; ++ i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			if ( !pInfo->m_bDisableShadowDepthCaching )	// this means renderable is in depth cache and shouldn't be rendered again
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}

		ppRenderables[nNewCount++] = pInfo;
	}

	return nNewCount;
}

//-----------------------------------------------------------------------------
// Extracts duplicates
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractDuplicates( int nFrameNumber, int nCount, RenderableInfo_t **ppRenderables )
{
	// NOTE: We don't know whether these renderables are translucent or not
	// but we do know if they participate in alternate sorting, which is all we need.
	int nUniqueCount = 0;
	int nLeaf = 0;

	// For better sorting, we're gonna choose the leaf that is closest to the camera.
	// The leaf list passed in here is sorted front to back

	// FIXME: This algorithm won't work in a threaded context since it stores state in renderableinfo_t
	if ( m_nAlternateSortCount == 0 )
	{
		// I expect this is the typical case; nothing needs alternate sorting
		// look 8 entries ahead and precache to minimize cache misses
		for ( int i = 0; i < nCount; i++ )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				// Skip these bad boys altogether
				if ( pInfo->m_Flags & ( RENDER_FLAGS_RENDER_WITH_VIEWMODELS | RENDER_FLAGS_DISABLE_RENDERING ) )
					continue;

				// If we've seen this already, then we don't need to add it 
				if ( pInfo->m_nRenderFrame == nFrameNumber )
					continue;

				pInfo->m_nRenderFrame = nFrameNumber;
			}
			ppRenderables[nUniqueCount++] = pInfo;
		}
		return nUniqueCount;
	}

	// Here, we have to worry about alternate sorting. I'm not sure if I
	// can do better than 2n unless I cache off counts of each renderable 
	// in the first loop in BuildRenderablesList. I'm doing it this way
	// because I don't believe we'll ever use this path.
	int nAlternateSortCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			// If we've seen this already, then we don't need to add it 
			if ( ( pInfo->m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) == 0 )
			{
				if( pInfo->m_nRenderFrame == nFrameNumber )
					continue;
				pInfo->m_nRenderFrame = nFrameNumber;
			}
			else
			{
				// A little convoluted, but I don't want to store any unnecessary state
				// Basically, the render frame will == frame number + duplication count by the end
				// NOTE: This will produce a problem for a few frames every 4 billion frames when wraparound happens
				// tough noogies
				++nAlternateSortCount;
				if( pInfo->m_nRenderFrame < nFrameNumber )
					pInfo->m_nRenderFrame = nFrameNumber + 1;
				else
					++pInfo->m_nRenderFrame;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}

	if ( nAlternateSortCount )
	{
		// Extract out the renderables which use alternate sorting
		nCount = nUniqueCount;
		nUniqueCount = 0;
		nLeaf = 0;
		for ( int i = 0; i < nCount; ++i )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				if ( pInfo->m_Flags & RENDER_FLAGS_ALTERNATE_SORTING )
				{
					// Add in the last one we encountered
					if( --pInfo->m_nRenderFrame != nFrameNumber )
						continue;
				}
			}

			ppRenderables[nUniqueCount++] = pInfo;
		}
	}

	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Extracts static props from the list of renderables
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractTranslucentRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			if ( pInfo->m_nTranslucencyType == RENDERABLE_IS_TRANSLUCENT )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}

static ConVar r_disable_distance_fade_on_big_props( side_registry(), "r_disable_distance_fade_on_big_props", "0", FCVAR_CHEAT, "Completely disable distance fading on large props" );
static ConVar r_disable_distance_fade_on_big_props_thresh( side_registry(), "r_disable_distance_fade_on_big_props_thresh", "48000", FCVAR_CHEAT, "Distance prop fade disable threshold size" );

//-----------------------------------------------------------------------------
// Computes translucency for all renderables
//-----------------------------------------------------------------------------
void CClientLeafSystem::ComputeDistanceFade( int nCount, AlphaInfo_t *pAlphaInfo, BuildRenderListInfo_t *pRLInfo, float flDistScale, const Vector& vecViewOrigin )
{
	// Distance fade computations
	float flDistFactorSq = 1.0f;
	CBasePlayer *pLocal = CBasePlayer::GetLocalPlayer();
	if ( pLocal && r_alphafade_usefov.GetBool() )
	{
		flDistFactorSq = pLocal->GetFOVDistanceAdjustFactor();
		flDistFactorSq *= flDistFactorSq;
	}
	flDistFactorSq *= flDistScale*flDistScale;

	const bool bDisableDistanceFadeOnBigProps = r_disable_distance_fade_on_big_props.GetBool();
	const float flDistanceFadeDisableThreshold = r_disable_distance_fade_on_big_props_thresh.GetFloat();
		
	for ( int i = 0; i < nCount; ++i )
	{
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( !pAlphaProp )
			continue;

		// Distance fade is inactive in this case
		if ( pAlphaProp->m_nDistFadeEnd == 0 )
			continue;

		if ( bDisableDistanceFadeOnBigProps ) 
		{
			// Just disable distance fading on very large props for CS:GO - it looks terrible and is distracting in many cases, and doesn't help CPU perf much if at all.
			Vector diag( pRLInfo[i].m_vecMaxs - pRLInfo[i].m_vecMins );
			// Not really box volume - hacked so one function is useful on zero thickness boxes too.
			float flFakeVol = ( diag.x * diag.x ) + ( diag.y * diag.y ) + ( diag.z * diag.z );
			if ( flFakeVol > flDistanceFadeDisableThreshold )
				continue;
		}

		float flCurrentDistanceSq;
		if ( pAlphaProp->m_nDistanceFadeMode == CLIENT_ALPHA_DISTANCE_FADE_USE_CENTER )
		{
			flCurrentDistanceSq = flDistFactorSq * vecViewOrigin.DistToSqr( pAlphaInfo[i].m_vecCenter );
		}
		else
		{
			flCurrentDistanceSq = flDistFactorSq * CalcSqrDistanceToAABB( pRLInfo[i].m_vecMins, pRLInfo[i].m_vecMaxs, vecViewOrigin );
		}

		float flDistFadeStartSq = pAlphaProp->m_nDistFadeStart;
		flDistFadeStartSq *= flDistFadeStartSq;
		if ( flCurrentDistanceSq <= flDistFadeStartSq )
			continue;

		float flDistFadeEndSq = pAlphaProp->m_nDistFadeEnd;
		flDistFadeEndSq *= flDistFadeEndSq;
		if ( flCurrentDistanceSq >= flDistFadeEndSq )
		{
			pAlphaInfo[i].m_flFadeFactor = 0.0f;
			continue;
		}

		// NOTE: Because of the if-checks above, flDistFadeEndSq != flDistFadeStartSq here
		pAlphaInfo[i].m_flFadeFactor = ( flDistFadeEndSq - flCurrentDistanceSq ) / ( flDistFadeEndSq - flDistFadeStartSq );
	}
}

float ComputeScreenSize( const Vector &vecOrigin, float flRadius, const ScreenSizeComputeInfo_t& info )
{
	// This is sort of faked, but it's faster that way
	// FIXME: Also, there's a much faster way to do this with similar triangles
	// but I want to make sure it exactly matches the current matrices, so
	// for now, I do it this conservative way
	/*
	Vector4D testPoint1, testPoint2;
	VectorMA( vecOrigin, flRadius, info.m_vecViewUp, testPoint1.AsVector3D() );
	VectorMA( vecOrigin, -flRadius, info.m_vecViewUp, testPoint2.AsVector3D() );
	testPoint1.w = testPoint2.w = 1.0f;

	Vector4D clipPos1, clipPos2;
	Vector4DMultiply( info.m_matViewProj, testPoint1, clipPos1 );
	Vector4DMultiply( info.m_matViewProj, testPoint2, clipPos2 );
	if (clipPos1.w >= 0.001f)
	{
		clipPos1.y /= clipPos1.w;
	}
	else
	{
		clipPos1.y *= 1000;
	}
	if (clipPos2.w >= 0.001f)
	{
		clipPos2.y /= clipPos2.w;
	}
	else
	{
		clipPos2.y *= 1000;
	}

	// The divide-by-two here is because y goes from -1 to 1 in projection space
	return info.m_nViewportHeight * fabs( clipPos2.y - clipPos1.y ) * 0.5f;
	*/

	// NOTE: Optimized version of the above algorithm, which only uses y and w components of clip
	// Can also optimize based on clipPos = a +/- b * r
	const float *pViewProjY	= info.m_matViewProj[1];
	const float *pViewProjW	= info.m_matViewProj[3];
	float flODotY		= pViewProjY[0] * vecOrigin.x			+ pViewProjY[1] * vecOrigin.y			+ pViewProjY[2] * vecOrigin.z			+ pViewProjY[3];
	float flViewDotY	= pViewProjY[0] * info.m_vecViewUp.x	+ pViewProjY[1] * info.m_vecViewUp.y	+ pViewProjY[2] * info.m_vecViewUp.z;  
	flViewDotY			*= flRadius;
	float flODotW		= pViewProjW[0] * vecOrigin.x			+ pViewProjW[1] * vecOrigin.y			+ pViewProjW[2] * vecOrigin.z			+ pViewProjW[3];
	float flViewDotW	= pViewProjW[0] * info.m_vecViewUp.x	+ pViewProjW[1] * info.m_vecViewUp.y	+ pViewProjW[2] * info.m_vecViewUp.z;
	flViewDotW			*= flRadius;
	float y0			= flODotY + flViewDotY;
	float w0			= flODotW + flViewDotW;
	y0					*= ( w0 >= 0.001f ) ? ( 1.0f / w0 ) : 1000.0f;
	float y1			= flODotY - flViewDotY;
	float w1			= flODotW - flViewDotW;
	y1					*= ( w1 >= 0.001f ) ? ( 1.0f / w1 ) : 1000.0f;

	// The divide-by-two here is because y goes from -1 to 1 in projection space
	return info.m_nViewportHeight * fabs( y1 - y0 ) * 0.5f;
}

void ComputeScreenSizeInfo( ScreenSizeComputeInfo_t *pInfo )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	VMatrix viewMatrix, projectionMatrix;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projectionMatrix );
	MatrixMultiply( projectionMatrix, viewMatrix, pInfo->m_matViewProj );

	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );
	pInfo->m_nViewportHeight = h;

	pRenderContext->GetWorldSpaceCameraVectors( nullptr, nullptr, &pInfo->m_vecViewUp );
}

void CClientLeafSystem::ComputeScreenFade( const ScreenSizeComputeInfo_t &info, float flMinScreenWidth, float flMaxScreenWidth, int nCount, AlphaInfo_t *pAlphaInfo )
{	
	if ( flMaxScreenWidth <= flMinScreenWidth )
	{
		flMaxScreenWidth = flMinScreenWidth;
	}
	if ( flMinScreenWidth <= 0 ) 
		return;

	float flFalloffFactor;
	if ( flMaxScreenWidth != flMinScreenWidth )
	{
		flFalloffFactor = 1.0f / ( flMaxScreenWidth - flMinScreenWidth );
	}
	else
	{
		flFalloffFactor = 1.0f;
	}
									    
	for ( int i = 0; i < nCount; ++i )
	{
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( !pAlphaProp )
			continue;

		// Fade is inactive in this case
		if ( pAlphaProp->m_flFadeScale <= 0.0f )
			continue;

		float flPixelWidth = ComputeScreenSize( pAlphaInfo[i].m_vecCenter, pAlphaInfo[i].m_flRadius, info ) / pAlphaProp->m_flFadeScale;

		// NOTE: This is to account for an error in the original screen computations years ago
		flPixelWidth *= 2.0f;

		float flAlpha = 0.0f;
		if ( flPixelWidth > flMinScreenWidth )
		{
			if ( ( flMaxScreenWidth >= 0 ) && ( flPixelWidth < flMaxScreenWidth ) )
			{
				flAlpha = flFalloffFactor * (flPixelWidth - flMinScreenWidth );
			}
			else
			{
				flAlpha = 1.0f;
			}
		}

		pAlphaInfo[i].m_flFadeFactor = std::min( pAlphaInfo[i].m_flFadeFactor, flAlpha );
	}
}

extern ConVar cl_leveloverview;

int CClientLeafSystem::ComputeTranslucency( int nFrameNumber, int nViewID, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, const ScreenSizeComputeInfo_t &screenSizeInfo, const Vector& vecViewOrigin )
{
	ks::ScratchArena<> scratch;

	AlphaInfo_t *pAlphaInfo = scratch.Alloc<AlphaInfo_t>( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
		{
			pAlphaInfo[i].m_pAlphaProperty = nullptr;
			continue;
		}

		Vector vecCenter;
		VectorAdd( pRLInfo[i].m_vecMaxs, pRLInfo[i].m_vecMins, vecCenter );
		vecCenter *= 0.5f;
		pAlphaInfo[i].m_vecCenter = vecCenter;
		pAlphaInfo[i].m_flRadius = vecCenter.DistTo( pRLInfo[i].m_vecMaxs );
		pAlphaInfo[i].m_pAlphaProperty = pInfo->m_pAlphaProperty;
		pAlphaInfo[i].m_flFadeFactor = 1.0f;
	}

	for ( int i = 0; i < nCount; ++i )
	{
		// FIXME: Computing the base alpha could potentially be sorted by renderfx type
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( pAlphaProp )
		{
			uint8 nAlpha = pAlphaProp->CClientAlphaProperty::ComputeRenderAlpha( );
			bool bIgnoreZBuffer = pAlphaProp->CClientAlphaProperty::IgnoresZBuffer();
			pRLInfo[i].m_nAlpha = nAlpha;
			pRLInfo[i].m_bIgnoreZBuffer = bIgnoreZBuffer;
		}
		else
		{
			pRLInfo[i].m_nAlpha = 255;
			pRLInfo[i].m_bIgnoreZBuffer = false;
		}
	}

	// If we're taking devshots, don't fade props at all
	bool bFadeProps = true;

	if ( nViewID == VIEW_3DSKY )
	{
		bFadeProps = false;
	}

	if ( !g_MakingDevShots && !cl_leveloverview.GetInt() && !input->CAM_IsThirdPersonOverview() && bFadeProps )
	{
		// We have three types of alpha fade calculation (the *minimum* of these alpha values is used):
		//  - distance fade			(fade based on distance    - set optionally, per-entity in Hammer,
		//							                             modified by a global cpu_level-based distance scale)
		//  - level screen fade		(fade based on screen area - set optionally, on the map entity in Hammer)
		//  - global screen fade	(fade based on screen area - set globally, based on cpu_level)
		float flMinLevelFadeArea,  flMaxLevelFadeArea;
		float flMinGlobalFadeArea, flMaxGlobalFadeArea, flGlobalDistFadeScale;
		modelinfo->GetLevelScreenFadeRange( &flMinLevelFadeArea, &flMaxLevelFadeArea );
		view->GetScreenFadeDistances( &flMinGlobalFadeArea, &flMaxGlobalFadeArea, &flGlobalDistFadeScale );

		ComputeDistanceFade( nCount, pAlphaInfo, pRLInfo, flGlobalDistFadeScale, vecViewOrigin );

		if ( ( flMinLevelFadeArea > 0.0f ) || ( flMinGlobalFadeArea > 0.0f ) )
		{
			ComputeScreenFade( screenSizeInfo, flMinLevelFadeArea,  flMaxLevelFadeArea,  nCount, pAlphaInfo );
			ComputeScreenFade( screenSizeInfo, flMinGlobalFadeArea, flMaxGlobalFadeArea, nCount, pAlphaInfo );
		}

		for ( int i = 0; i < nCount; ++i )
		{
			if ( !pAlphaInfo[i].m_pAlphaProperty )
				continue;

			float flAlpha = pRLInfo[i].m_nAlpha * pAlphaInfo[i].m_flFadeFactor;
			int nAlpha = (int)flAlpha;
			pRLInfo[i].m_nAlpha = clamp( nAlpha, 0, 255 );
		}
	}

// CTSRIKE15 only using cascade shadow mapping - code below is for projected shadow

	// Strip invisible ones out
	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !IsLeafMarker( ppRenderables[i] ) && ( !pRLInfo[i].m_nAlpha ) )
		{
			// Necessary for dependent models to be grabbed
			ppRenderables[i]->m_nRenderFrame--;
			continue;
		}

		ppRenderables[nUniqueCount] = ppRenderables[i];
		pRLInfo[nUniqueCount] = pRLInfo[i];
		++nUniqueCount;
	}
	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Computes bounds for all renderables
//-----------------------------------------------------------------------------

void CClientLeafSystem::ComputeAllBounds( void )
{
	VectorAligned vecAbsMins, vecAbsMaxs;
	MDLCACHE_CRITICAL_SECTION();
	for ( RenderableInfo_t &renderableInfo : m_Renderables )
	{
		RenderableInfo_t *pInfo = &renderableInfo;

		if (pInfo->m_Flags & RENDER_FLAGS_DISABLE_RENDERING )
			continue;

		if ( ( pInfo->m_Flags & RENDER_FLAGS_BOUNDS_VALID ) == 0 )
		{
			CalcRenderableWorldSpaceAABB( pInfo->m_pRenderable, pInfo->m_vecAbsMins, pInfo->m_vecAbsMaxs );
			if ( ( pInfo->m_Flags & RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE ) == 0 )
			{
				pInfo->m_Flags |= RENDER_FLAGS_BOUNDS_VALID;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Computes bounds for all renderables in the list
//-----------------------------------------------------------------------------
void CClientLeafSystem::ComputeBounds( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{

	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
			continue;

		// UNDONE: Investigate speed tradeoffs of occlusion culling brush models too?
		pRLInfo[i].m_bPerformOcclusionTest = ( pInfo->m_nModelType == RENDERABLE_MODEL_STATIC_PROP || pInfo->m_nModelType == RENDERABLE_MODEL_STUDIOMDL ); 
		pRLInfo[i].m_nArea = pInfo->m_Area;
		pRLInfo[i].m_nAlpha = 255;	// necessary to set for shadow depth rendering

		// Bounds should be valid as ComputeAllBounds called from CViewRender::RenderView
		pRLInfo[i].m_vecMins = pInfo->m_vecAbsMins;
		pRLInfo[i].m_vecMaxs = pInfo->m_vecAbsMaxs;
	}
}

//-----------------------------------------------------------------------------
// Culls renderables based on view frustum + areaportals
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractCulledRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, Frustum_t** ppFrustumList )
{

	bool bPortalTestEnts = r_PortalTestEnts.GetBool() && !r_portalsopenall.GetBool();

	// FIXME: sort by area and inline cull. Should make it a bunch faster
	int nUniqueCount = 0;
	if ( bPortalTestEnts )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			BuildRenderListInfo_t &rlInfo = pRLInfo[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				int frustumIndex = rlInfo.m_nArea + 1;
				if ( ppFrustumList[frustumIndex]->CullBox( rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
				{
					// Necessary for dependent models to be grabbed
					pInfo->m_nRenderFrame--;
					continue;
				}
			}
			pRLInfo[nUniqueCount] = rlInfo;
			ppRenderables[nUniqueCount] = pInfo;
			++nUniqueCount;
		}
		return nUniqueCount;
	}

	// Debug mode, doesn't need to be fast
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		BuildRenderListInfo_t &rlInfo = pRLInfo[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			// cull with main frustum
			if ( ppFrustumList[0]->CullBox( rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		pRLInfo[nUniqueCount] = rlInfo;
		ppRenderables[nUniqueCount] = pInfo;
		++nUniqueCount;
	}
	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Culls renderables based on occlusion
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractOccludedRenderables( int occlusionViewId, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{
	static ConVarRef r_occlusion("r_occlusion");

	// occlusion is off, just return
	if ( !r_occlusion.GetBool() )
		return nCount;

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		BuildRenderListInfo_t &rlInfo = pRLInfo[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			if ( rlInfo.m_bPerformOcclusionTest )
			{
				// test to see if this renderable is occluded by the engine's occlusion system
				if ( engine->IsOccluded( occlusionViewId, rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
				{
					// Necessary for dependent models to be grabbed
					pInfo->m_nRenderFrame--;
					continue;
				}
			}
		}
		pRLInfo[nUniqueCount] = rlInfo;
		ppRenderables[nUniqueCount] = pInfo;
		++nUniqueCount;
	}

	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Adds renderables into their final lists
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddDependentRenderables( const SetupRenderInfo_t &info )
{
	// NOTE: This turns out to have non-zero cost.
	// Remove early out if we actually end up needing to use this
	return;
}

//-----------------------------------------------------------------------------
// Adds renderables into their final lists
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderablesToRenderLists( const SetupRenderInfo_t &info, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, int nDetailCount, DetailRenderableInfo_t *pDetailInfo )
{
	ks::ScratchArena<> scratch;

	// Vitaliy (2/15/2013) -- stop rendering the detail props since the rendering code crashes when they are rendered anyways
	nDetailCount = 0;

	CClientRenderablesList::CEntry *pTranslucentEntries = info.m_pRenderList->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
	int &nTranslucentEntries = info.m_pRenderList->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT];
	BuildRenderListInfo_t **pTranslucentRLInfo = scratch.Alloc<BuildRenderListInfo_t*>( nCount );

	int nTranslucent = 0;
	int nCurDetail = 0;
	int nTLucInfoCount = 0;
	int nWorldListLeafIndex = -1;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
		{
			// Add detail props for this leaf
			if ( !info.m_bDrawDepthViewNonCachedObjectsOnly ) // Detail props are considered cacheable
			{
				for( ; nCurDetail < nDetailCount; ++nCurDetail )
				{
					contract_assert( 0 );

					DetailRenderableInfo_t &detailInfo = pDetailInfo[nCurDetail];
					if ( detailInfo.m_nLeafIndex > nWorldListLeafIndex )
						break;
					contract_assert( detailInfo.m_nLeafIndex == nWorldListLeafIndex );
					AddRenderableToRenderList( *info.m_pRenderList, detailInfo.m_pRenderable, 
						nWorldListLeafIndex, detailInfo.m_nRenderGroup, RENDERABLE_MODEL_ENTITY, detailInfo.m_InstanceData.m_nAlpha,
						false, // detail props are allowed to be cached
						false // single pass
						);
				}
			}

			int nNewTranslucent = nTranslucentEntries - nTranslucent;
			if ( ( nNewTranslucent != 0 ) && info.m_bDrawTranslucentObjects )
			{
				// Sort the new translucent entities.
				contract_assert( nNewTranslucent == nTLucInfoCount );
				SortEntities( info.m_vecRenderOrigin, info.m_vecRenderForward, &pTranslucentEntries[nTranslucent], pTranslucentRLInfo, nNewTranslucent );
			}

			nTranslucent = nTranslucentEntries;
			nTLucInfoCount = 0;
			nWorldListLeafIndex++;
			continue;
		}

		bool bIsTranslucent = ( pRLInfo[i].m_nAlpha != 255 ) || ( pInfo->m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 
		if ( !bIsTranslucent || (pInfo->m_Flags & RENDER_FLAGS_FORCE_OPAQUE_PASS) )
		{
			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, RENDER_GROUP_OPAQUE, pInfo->m_nModelType, pRLInfo[i].m_nAlpha, pInfo->m_bDisableShadowDepthCaching );
			continue;
		}

		// FIXME: Remove call to GetFXBlend
		bool bIsTwoPass = ( pInfo->m_nTranslucencyType == RENDERABLE_IS_TWO_PASS ) && ( pRLInfo[i].m_nAlpha == 255 );	// Two pass?

		// Add to appropriate list if drawing translucent objects (shadow depth mapping will skip this)
		if ( info.m_bDrawTranslucentObjects ) 
		{
			RenderGroup_t group;
			if ( !pRLInfo[i].m_bIgnoreZBuffer )
			{
				group = RENDER_GROUP_TRANSLUCENT;
				pTranslucentRLInfo[nTLucInfoCount++] = &pRLInfo[i];
			}
			else
			{
				group = RENDER_GROUP_TRANSLUCENT_IGNOREZ;
			}
			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, group, pInfo->m_nModelType, pRLInfo[i].m_nAlpha, pInfo->m_bDisableShadowDepthCaching, bIsTwoPass );
		}

		if ( bIsTwoPass )	// Also add to opaque list if it's a two-pass model... 
		{
			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, RENDER_GROUP_OPAQUE, pInfo->m_nModelType, 255, pInfo->m_bDisableShadowDepthCaching, bIsTwoPass );
		}
	}

	// Add detail props for this leaf
	for( ; nCurDetail < nDetailCount; ++nCurDetail )
	{
		contract_assert( 0 );
		DetailRenderableInfo_t &detailInfo = pDetailInfo[nCurDetail];
		if ( detailInfo.m_nLeafIndex > nWorldListLeafIndex )
			break;
		contract_assert( detailInfo.m_nLeafIndex == nWorldListLeafIndex );
		AddRenderableToRenderList( *info.m_pRenderList, detailInfo.m_pRenderable, 
			nWorldListLeafIndex, detailInfo.m_nRenderGroup, RENDERABLE_MODEL_ENTITY, detailInfo.m_InstanceData.m_nAlpha,
			false, // detail props are allowed to be cached
			false // single pass
			);
	}

	int nNewTranslucent = nTranslucentEntries - nTranslucent;
	if( ( nNewTranslucent != 0 ) && info.m_bDrawTranslucentObjects )
	{
		// Sort the new translucent entities.
		contract_assert( nNewTranslucent == nTLucInfoCount );
		SortEntities( info.m_vecRenderOrigin, info.m_vecRenderForward, &pTranslucentEntries[nTranslucent], pTranslucentRLInfo, nNewTranslucent );
	}

	AddDependentRenderables( info );
}

static ConVar r_drawallrenderables( side_registry(), "r_drawallrenderables", "0", FCVAR_CHEAT, "Draw all renderables, even ones inside solid leaves." );
extern ConVar r_flashlight_nostaticgeo;

static ConVar r_fastreflectionfastpath( side_registry(), "r_fastreflectionfastpath", "1" );

void CClientLeafSystem::BuildRenderablesListForFastReflections( const SetupRenderInfo_t &info )
{
	for ( RenderableInfo_t &renderableInfo : m_Renderables )
	{
		RenderableInfo_t *pInfo = &renderableInfo;

		if ( pInfo->m_Flags & ( RENDER_FLAGS_RENDER_WITH_VIEWMODELS | RENDER_FLAGS_DISABLE_RENDERING ) )
			continue;

		// If we've seen this already, then we don't need to add it 
		if ( pInfo->m_nRenderFrame == info.m_nRenderFrame )
			continue;

		pInfo->m_nRenderFrame = info.m_nRenderFrame;

		if ( pInfo->m_bRenderInFastReflection )
		{
			// Use frustum 0 instead of [area index + 1] beacuse we're not guaranteed to have valid per-area frustums
			// wehn taking the simple world model + fast reflection path.
			// Frustum 0 is always valid (and more conservative than area frustums in slots 1, 2, 3...) because it's the camera frustum,
			// whereas the area frustums are smaller/refined frusta that result from clipping the camera frustum against 2D screen-space area portals
			int nFrustumIndex = 0; // pInfo->m_Area + 1;
			
			// Bounds should be valid as ComputeAllBounds called from CViewRender::RenderView
			if ( !info.m_ppFrustumList[ nFrustumIndex ]->CullBox( pInfo->m_vecAbsMins, pInfo->m_vecAbsMaxs ) )
			{
				RenderGroup_t renderGroup = RENDER_GROUP_OPAQUE;
				CClientAlphaProperty *pAlphaProp = pInfo->m_pAlphaProperty;

				if ( pAlphaProp )
				{
					int nAlpha = pAlphaProp->ComputeRenderAlpha();
					bool bIsTranslucent = ( nAlpha != 255 ) || ( pInfo->m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 
					if ( bIsTranslucent )
					{
						renderGroup = pAlphaProp->IgnoresZBuffer() ? RENDER_GROUP_TRANSLUCENT_IGNOREZ : RENDER_GROUP_TRANSLUCENT;
					}
				}

				AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
					0, renderGroup, pInfo->m_nModelType, 255, pInfo->m_bDisableShadowDepthCaching );
			}
		}
	}
}

ConVar r_highlight_translucent_renderables( side_registry(), "r_highlight_translucent_renderables", "0" );

void CClientLeafSystem::HighlightAllTranslucentRenderables()
{
	for ( RenderableInfo_t &info : m_Renderables )
	{

		int nAlpha = info.m_pAlphaProperty ? info.m_pAlphaProperty->ComputeRenderAlpha( ) : 255;
		bool bIsTranslucent = ( nAlpha != 255 ) || ( info.m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 
		if ( bIsTranslucent )
		{
			CBaseEntity *pEntity = dynamic_cast< CBaseEntity * >( info.m_pRenderable );
			if ( pEntity )
			{
				Vector vecSurroundMins, vecSurroundMaxs;
				pEntity->CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
				Vector center = 0.5f * (vecSurroundMins + vecSurroundMaxs);
				Vector extents = vecSurroundMaxs - center;
				NDebugOverlay::Box( center, -extents, extents, 150, 150, 0, 0, 0 );
				NDebugOverlay::EntityTextAtPosition( center, 0, pEntity->GetDebugName(), 0, 255, 255, 255, 255 );
			}
		}
	}
}

void CClientLeafSystem::BuildRenderablesListForCSMView( const SetupRenderInfo_t &setupInfo )
{	
	//Frustum_t *list[MAX_MAP_AREAS];
	//engine->GetFrustumList( list, ARRAYSIZE(list) );

	const bool bDisableCSMCulling = cl_csm_disable_culling.GetBool();
	const bool bShadowEntities = cl_csm_entity_shadows.GetBool() && cl_csm_shadows.GetBool();
	const bool bShadowStaticProps = cl_csm_static_prop_shadows.GetBool() && cl_csm_shadows.GetBool();
	const bool bShadowSprites = cl_csm_sprite_shadows.GetBool() && cl_csm_shadows.GetBool();
	const bool bIgnoreDisableShadowDepthRendering = cl_csm_ignore_disable_shadow_depth_rendering.GetBool();
	const bool bOptimizedCSMStaticProps = cl_csm_optimize_static_props.GetBool();

	const CVolumeCuller *pCSMVolumeCuller = setupInfo.m_pCSMVolumeCuller;

	for ( RenderableInfo_t &renderableInfo : m_Renderables )
	{
		RenderableInfo_t *pInfo = &renderableInfo;
		if ( ( !bIgnoreDisableShadowDepthRendering ) && ( pInfo->m_bDisableShadowDepthRendering || ( pInfo->m_bDisableCSMRendering && bOptimizedCSMStaticProps ) ) )
			continue;
		if ( pInfo->m_Flags & ( RENDER_FLAGS_DISABLE_RENDERING | RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) )
			continue;
		if ( !bShadowEntities )
		{
			if ( ( pInfo->m_nModelType == RENDERABLE_MODEL_ENTITY ) || ( pInfo->m_nModelType == RENDERABLE_MODEL_STUDIOMDL ) )
				continue;
		}
		if ( ( !bShadowStaticProps ) && ( pInfo->m_nModelType == RENDERABLE_MODEL_STATIC_PROP ) )
			continue;
		if ( ( !bShadowSprites ) && ( pInfo->m_Flags & RENDER_FLAGS_IS_SPRITE ) )
			continue;
		
		// If we've seen this already, then we don't need to add it 
		if ( pInfo->m_nRenderFrame == setupInfo.m_nRenderFrame )
			continue;

		pInfo->m_nRenderFrame = setupInfo.m_nRenderFrame;

		VectorAligned vecAbsMins, vecAbsMaxs;
		// Bounds should be valid as ComputeAllBounds called from CViewRender::RenderView
		vecAbsMins = pInfo->m_vecAbsMins;
		vecAbsMaxs = pInfo->m_vecAbsMaxs;

		if ( ( !bDisableCSMCulling ) && ( pCSMVolumeCuller ) && ( !pCSMVolumeCuller->CheckBox( vecAbsMins, vecAbsMaxs ) ) )
			continue;

		RenderGroup_t renderGroup = RENDER_GROUP_OPAQUE;
		CClientAlphaProperty *pAlphaProp = pInfo->m_pAlphaProperty;
		if ( pAlphaProp )
		{
			int nAlpha = pAlphaProp->ComputeRenderAlpha();
			bool bIsTranslucent = ( nAlpha != 255 ) || ( pInfo->m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 
			if ( bIsTranslucent )
			{
				renderGroup = pAlphaProp->IgnoresZBuffer() ? RENDER_GROUP_TRANSLUCENT_IGNOREZ : RENDER_GROUP_TRANSLUCENT;
			}
		}

		AddRenderableToRenderList( *setupInfo.m_pRenderList, pInfo->m_pRenderable, 
			0, renderGroup, pInfo->m_nModelType, 255, pInfo->m_bDisableShadowDepthCaching );
	}

}

//-----------------------------------------------------------------------------
// Main entry point to build renderable lists
//-----------------------------------------------------------------------------

void CClientLeafSystem::BuildRenderablesList( const SetupRenderInfo_t &info )
{	
	ks::ScratchArena<> scratch;
	;

	info.m_pWorldListInfo = g_viewBuilder.GetWorldListInfoElement( info.m_nBuildViewID );
	info.m_pRenderList    = g_viewBuilder.GetRenderablesListElement( info.m_nBuildViewID );
	info.m_pCSMVolumeCuller = g_viewBuilder.GetBuildViewVolumeCuller( info.m_nBuildViewID );
	info.m_pFrustum = g_viewBuilder.GetBuildViewFrustum( info.m_nBuildViewID );
	info.m_ppFrustumList = g_viewBuilder.GetBuildViewFrustumList( info.m_nBuildViewID );

	if ( r_highlight_translucent_renderables.GetBool() )
	{
		HighlightAllTranslucentRenderables();
	}

	if ( info.m_bFastEntityRendering && r_fastreflectionfastpath.GetBool() )
	{
		BuildRenderablesListForFastReflections( info );

		return;
	}

	if ( info.m_bCSMView )
	{
		BuildRenderablesListForCSMView( info );
		return;
	}

	// Deal with detail objects
	ks::small_vector< DetailRenderableInfo_t, 2048 > detailRenderables;

	// Get the fade information for detail objects
	float flDetailDist = g_pDetailObjectSystem->ComputeDetailFadeInfo( &info.m_pRenderList->m_DetailFade );
	g_pDetailObjectSystem->BuildRenderingData( detailRenderables, info, flDetailDist, info.m_pRenderList->m_DetailFade );

	// First build a non-unique list of renderables, separated by special leaf markers
	ks::small_vector< RenderableInfo_t *, 2048 > orderedList;

	if ( info.m_nViewID != VIEW_3DSKY && r_drawallrenderables.GetBool() )
	{
		// HACK - treat all renderables as being in first visible leaf
		int leaf = info.m_pWorldListInfo->m_pLeafDataList[ 0 ].leafIndex;
		orderedList.push_back( LeafToMarker( leaf ) );

		for ( RenderableInfo_t &renderableInfo : m_Renderables )
		{
			orderedList.push_back( &renderableInfo );
		}
	}
	else
	{

		int leaf = 0;
		for ( int i = 0; i < info.m_pWorldListInfo->m_LeafCount; ++i )
		{
			leaf = info.m_pWorldListInfo->m_pLeafDataList[i].leafIndex;
			orderedList.push_back( LeafToMarker( leaf ) );

			// iterate over all elements in this leaf
			unsigned short idx = m_RenderablesInLeaf.first_element(leaf);

			for ( ; idx != m_RenderablesInLeaf.invalid; idx = m_RenderablesInLeaf.next_element( idx ) )
			{
				orderedList.push_back( &m_Renderables[ m_RenderablesInLeaf.element(idx) ] );
			}
		}
	}

	// Debugging
	int nCount = std::ssize( orderedList );
	RenderableInfo_t **ppRenderables = orderedList.data();

	{

	nCount = ExtractDuplicates( info.m_nRenderFrame, nCount, ppRenderables );
	nCount = ExtractStaticProps( nCount, ppRenderables );
	nCount = ExtractSplitscreenRenderables( nCount, ppRenderables );
	}

	if ( info.m_bFastEntityRendering )
	{
		nCount = ExtractNonFastReflectedRenderables( nCount, ppRenderables );
	}

	if ( info.m_nViewID == VIEW_SHADOW_DEPTH_TEXTURE )
	{
		nCount = ExtractDisableShadowDepthRenderables( nCount, ppRenderables );
		if ( info.m_bDrawDepthViewNonCachedObjectsOnly )
		{
			nCount = ExtractDisableShadowDepthCacheRenderables( nCount, ppRenderables );
		}
	}
	
	if ( !info.m_bDrawTranslucentObjects )
	{
		nCount = ExtractTranslucentRenderables( nCount, ppRenderables );
	}

	BuildRenderListInfo_t* pRLInfo = scratch.Alloc<BuildRenderListInfo_t>( nCount );

	ComputeBounds( nCount, ppRenderables, pRLInfo );

	nCount = ExtractCulledRenderables( nCount, ppRenderables, pRLInfo, info.m_ppFrustumList );

	if ( info.m_bDrawTranslucentObjects )
	{
		nCount = ComputeTranslucency( gpGlobals->framecount /*info.m_nRenderFrame*/, info.m_nViewID, nCount, ppRenderables, pRLInfo, info.m_screenSizeInfo, info.m_vecRenderOrigin );
	}

	// 
	nCount = ExtractOccludedRenderables( info.m_nOcclustionViewID, nCount, ppRenderables, pRLInfo );

	AddRenderablesToRenderLists( info, nCount, ppRenderables, pRLInfo, std::ssize( detailRenderables ), detailRenderables.data() );

//	Msg("PPU count = %d, detailcount = %d\n",nCount, detailRenderables.Count() );

}

RenderGroup_t CClientLeafSystem::GenerateRenderListEntry( IClientRenderable *pRenderable, CClientRenderablesList::CEntry &entryOut )
{
	for ( RenderableInfo_t &info : m_Renderables )
	{
		if( info.m_pRenderable == pRenderable )
		{			
			int nAlpha = info.m_pAlphaProperty ? info.m_pAlphaProperty->ComputeRenderAlpha( ) : 255;
			bool bIsTranslucent = ( nAlpha != 255 ) || ( info.m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 

			entryOut.m_pRenderable = pRenderable;
			entryOut.m_iWorldListInfoLeaf = 0; //info.m_RenderLeaf;			
			entryOut.m_bShadowDepthNoCache = false;
			entryOut.m_TwoPass = ( info.m_nTranslucencyType == RENDERABLE_IS_TWO_PASS );
			entryOut.m_nModelType = info.m_nModelType;
			entryOut.m_InstanceData.m_nAlpha = nAlpha;
			if ( !bIsTranslucent )
				return RENDER_GROUP_OPAQUE;
			return info.m_pAlphaProperty->IgnoresZBuffer() ? RENDER_GROUP_TRANSLUCENT_IGNOREZ : RENDER_GROUP_TRANSLUCENT;
		}
	}

	entryOut.m_pRenderable = nullptr;
	entryOut.m_iWorldListInfoLeaf = 0;
	entryOut.m_bShadowDepthNoCache = false;
	entryOut.m_TwoPass = false;
	entryOut.m_nModelType = RENDERABLE_MODEL_ENTITY;
	entryOut.m_InstanceData.m_nAlpha = 255;

	return RENDER_GROUP_COUNT;
}

int CClientLeafSystem::GetEntitiesInBox( CBaseEntity **pEntityList, int listMax, const Vector& vWorldSpaceMins, const Vector& vWorldSpaceMaxs )
{
	// NOTE: The render bounds here are relative to the renderable's coordinate system
	contract_assert( vWorldSpaceMins.IsValid() && vWorldSpaceMaxs.IsValid() );
	contract_assert( ThreadInMainThread() );

	// Make use of m_ShadowEnum to avoid adding same entity to the list
	++m_ShadowEnum;

	// When we insert into the tree, increase the shadow enumerator
	// to make sure each shadow is added exactly once to each renderable
	unsigned short leafList[1024];
	ISpatialQuery* pQuery = engine->GetBSPTreeQuery();
	int leafCount = pQuery->ListLeavesInBox( vWorldSpaceMins, vWorldSpaceMaxs, leafList, std::size( leafList ) );
	int totalCount = 0;
	for ( int i=0; i<leafCount; ++i )
	{
		unsigned short leaf = leafList[i];

		// iterate over all elements in this leaf
		unsigned short idx = m_RenderablesInLeaf.first_element(leaf);
		for ( ; idx != m_RenderablesInLeaf.invalid; idx = m_RenderablesInLeaf.next_element( idx ) )
		{
			ClientRenderHandle_t handle = m_RenderablesInLeaf.element(idx);
			RenderableInfo_t &info = m_Renderables[ handle ];

			// Add each shadow exactly once to each renderable
			if (info.m_EnumCount != m_ShadowEnum)
			{
				info.m_EnumCount = m_ShadowEnum;
			}
			else
			{
				continue;
			}

			if ( IsBoxIntersectingBox( vWorldSpaceMins, vWorldSpaceMaxs, info.m_vecAbsMins, info.m_vecAbsMaxs ) )
			{
				CBaseEntity *pEnt = EntityCast<CBaseEntity>( m_Renderables[ handle ].m_pRenderable->GetIClientUnknown()->GetBaseEntity() );
				if ( pEnt )
				{
					pEntityList[totalCount] = pEnt;
					++totalCount;
				}
			}
		}
	}

	return totalCount;
}

CClientRenderablesList::CClientRenderablesList()
{
	int i;
	for( i=0; i < RENDER_GROUP_COUNT; i++ )
	{
		m_RenderGroupCounts[i] = 0;
	}
	m_nBoneSetupDependencyCount = 0;
}

SetupRenderInfo_t::SetupRenderInfo_t()
{
	m_pWorldListInfo = nullptr;
	m_pRenderList = nullptr;
	m_pCSMVolumeCuller = nullptr;
	m_pFrustum = nullptr;
	m_ppFrustumList = nullptr;
	m_nBuildViewID = -1;
	m_nOcclustionViewID = -1;

	m_bDrawDetailObjects = true;
	m_bDrawTranslucentObjects = true;
	m_bFastEntityRendering = false;
	m_bDrawDepthViewNonCachedObjectsOnly = false;
	m_bCSMView = false;
}
