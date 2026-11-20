// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

module;
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "edict.h"
#include "engine/IClientLeafSystem.h"
#include "iclientrenderable.h"
#include "model_types.h"
#include "particles/particles.h"
#include "view_shared.h"
#include "cdll_client_int.h"
#include "particlemgr.h"
#include "datacache/iresourceaccesscontrol.h"
#include "declare_class.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "cmodel.h"
#include "collisionutils.h"
#include "tier3/mdlutils.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "const.h"
#include "reflect_recvtable.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.numeric;
import ks.core.platform;
import ks.core.scratcharena;
import ks.core.types;
import ks.core.vprof;
import ks.math.aabb;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.map_ops;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.small_vector;
import ks.tier1.vector_ops;
import ks.vstdlib.jobthread;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :util;
import :particlemgr;
import :cstrike15.cs_player;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :sharedvar;
import :cstrike15.cs_shareddefs;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.tier1.intrusive_list;
import ks.tier1.intrusive_ptr;
import ks.game.entity_cast;
import ks.game.gamemovement;
import ks.math.vector4d;
import ks.tier1.exprevaluator;
import ks.tier1.tracked_ref;
import ks.math.vplane;
import ks.core.color;
import ks.core.tslist;
import ks.math.fltx4;
import ks.math.ssemath;
import ks.math.transform;
import ks.tier1.strtools;

class CEffectMaterial;
class CNewParticleEffect;
class CParticleEffectBinding;
class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CParticleSubTexture;
class CParticleSubTextureGroup;
class CBaseEntity;
class IClientParticleListener;
class IParticleEffect;
struct Particle;
class ParticleDraw;

using namespace ::ks::reflect;

extern IParticleSystemQuery *g_pParticleSystemQuery;

static int g_nParticlesDrawn;
// CCycleCount	g_ParticleTimer;

ConVar r_DrawParticles(side_registry(), "r_drawparticles", "1", FCVAR_CHEAT, "Enable/disable particle rendering");
static ConVar particle_simulateoverflow( side_registry(), "particle_simulateoverflow", "0", FCVAR_CHEAT, "Used for stress-testing particle systems. Randomly denies creation of particles." );
ConVar cl_particles_show_bbox( side_registry(), "cl_particles_show_bbox", "0", FCVAR_CHEAT );
ConVar cl_particle_fallback_multiplier( side_registry(), "cl_particle_fallback_multiplier", "1", FCVAR_NONE, "Multiplier for falling back to cheaper effects under load." ); 
ConVar cl_particle_fallback_base( side_registry(), "cl_particle_fallback_base", "0", FCVAR_NONE, "Base for falling back to cheaper effects under load." ); 

// Don't fallback as fast ont he PC since the rest of the frame doesn't take as long and we can afford to have more high
// quality systems.
static ConVar cl_particle_sim_fallback_threshold_ms( side_registry(), "cl_particle_sim_fallback_threshold_ms", "6.0", FCVAR_NONE, "Amount of simulation time that can elapse before new systems start falling back to cheaper versions" );

// Don't fallback as fast on the PC since the rest of the frame doesn't take as long
static ConVar cl_particle_sim_fallback_base_multiplier( side_registry(), "cl_particle_sim_fallback_base_multiplier", "5", FCVAR_NONE, "How aggressive the switch to fallbacks will be depending on how far over the cl_particle_sim_fallback_threshold_ms the sim time is.  Higher numbers are more aggressive." );

static constexpr auto BUCKET_SORT_EVERY_N = 8; // It does a bucket sort for each material approximately every N times.
static constexpr auto BBOX_UPDATE_EVERY_N = 8; // It does a full bbox update (checks all particles instead of every eighth one).

//-----------------------------------------------------------------------------
//
// Particle manager implementation
//
//-----------------------------------------------------------------------------

static constexpr auto PARTICLE_SIZE = 96;

CParticleMgr *ParticleMgr()
{
	static CParticleMgr s_ParticleMgr;
	return &s_ParticleMgr;
}

//-----------------------------------------------------------------------------
// CParticleSubTextureGroup implementation.
//-----------------------------------------------------------------------------

CParticleSubTextureGroup::CParticleSubTextureGroup()
{
	m_pPageMaterial = nullptr;
}

CParticleSubTextureGroup::~CParticleSubTextureGroup()
{
}

//-----------------------------------------------------------------------------
// CParticleSubTexture implementation.
//-----------------------------------------------------------------------------

CParticleSubTexture::CParticleSubTexture()
{
	m_tCoordMins[0] = m_tCoordMins[0] = 0;
	m_tCoordMaxs[0] = m_tCoordMaxs[0] = 1;
	m_pGroup = &m_DefaultGroup;
	m_pMaterial = nullptr;

}

//-----------------------------------------------------------------------------
// CEffectMaterial.
//-----------------------------------------------------------------------------

CEffectMaterial::CEffectMaterial()
{
	m_Particles.m_pNext = m_Particles.m_pPrev = &m_Particles;
	m_pGroup = nullptr;
}

//-----------------------------------------------------------------------------
// CParticleEffectBinding.
//-----------------------------------------------------------------------------
CParticleEffectBinding::CParticleEffectBinding()
{
	m_pParticleMgr = nullptr;
	m_pSim = nullptr;

	m_LocalSpaceTransform.Identity();
	m_bLocalSpaceTransformIdentity = true;
	
	m_Flags = 0;
	SetAutoUpdateBBox( true );
	SetFirstFrameFlag( true );
	SetNeedsBBoxUpdate( true );
	SetAlwaysSimulate( true );
	SetEffectCameraSpace( true );
	SetDrawThruLeafSystem( true );
	SetAutoApplyLocalTransform( true );

	// default bbox
	m_Min.Init( -50, -50, -50 );
	m_Max.Init( 50, 50, 50 );

	m_LastMin = m_Min;
	m_LastMax = m_Max;

	SetParticleCullRadius( 0.0f );
	m_nActiveParticles = 0;

	m_FrameCode = 0;

	m_UpdateBBoxCounter = 0;

	memset( m_EffectMaterialHash, 0, sizeof( m_EffectMaterialHash ) );
}

CParticleEffectBinding::~CParticleEffectBinding()
{
	if( m_pParticleMgr )
		m_pParticleMgr->RemoveEffect( this );

	Term();
}

// The is the max size of the particles for use in bounding	computation
void CParticleEffectBinding::SetParticleCullRadius( float flMaxParticleRadius )
{
	if ( m_flParticleCullRadius != flMaxParticleRadius )
	{
		m_flParticleCullRadius = flMaxParticleRadius;

		if ( m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE )
		{
			ClientLeafSystem()->RenderableChanged( m_hRenderHandle );
		}
	}
}

const Vector& CParticleEffectBinding::GetRenderOrigin( void )
{
	return m_pSim->GetSortOrigin();
}

const QAngle& CParticleEffectBinding::GetRenderAngles( void )
{
	return vec3_angle;
}

const matrix3x4_t &	CParticleEffectBinding::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix( mat );
	PositionMatrix( GetRenderOrigin(), mat );
	return mat;
}

void CParticleEffectBinding::GetRenderBounds( Vector& mins, Vector& maxs )
{
	const Vector &vSortOrigin = m_pSim->GetSortOrigin();

	// Convert to local space (around the sort origin).
	mins = m_Min - vSortOrigin;
	mins.x -= m_flParticleCullRadius; mins.y -= m_flParticleCullRadius; mins.z -= m_flParticleCullRadius;
	maxs = m_Max - vSortOrigin;
	maxs.x += m_flParticleCullRadius; maxs.y += m_flParticleCullRadius; maxs.z += m_flParticleCullRadius;
}

bool CParticleEffectBinding::ShouldDraw( void )
{
	return GetFlag( FLAGS_DRAW_THRU_LEAF_SYSTEM ) != 0;
}

void CParticleEffectBinding::StartDrawMaterialParticles(
	CEffectMaterial *pMaterial,
	float flTimeDelta,
	IMesh* &pMesh,
	CMeshBuilder &builder,
	ParticleDraw &particleDraw,
	bool bWireframe )
{
	CMatRenderContextPtr pRenderContext( m_pParticleMgr->m_pMaterialSystem );

	// Setup the ParticleDraw and bind the material.
	if( bWireframe )
	{
		IMaterial *pMaterial = m_pParticleMgr->m_pMaterialSystem->FindMaterial( "debug/debugparticlewireframe", TEXTURE_GROUP_OTHER );
		pRenderContext->Bind( pMaterial, nullptr );
	}
	else
	{
		pRenderContext->Bind( pMaterial->m_pGroup->m_pPageMaterial, m_pParticleMgr );
	}

	pMesh = pRenderContext->GetDynamicMesh( true );

	builder.Begin( pMesh, MATERIAL_QUADS, NUM_PARTICLES_PER_BATCH * 4 );
	particleDraw.Init( &builder, pMaterial->m_pGroup->m_pPageMaterial, flTimeDelta );
}

void CParticleEffectBinding::BBoxCalcStart( Vector &bbMin, Vector &bbMax )
{
	if ( !GetAutoUpdateBBox() )
		return;

	// We're going to fully recompute the bbox.
	bbMin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	bbMax.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
}

void CParticleEffectBinding::BBoxCalcEnd( bool bboxSet, Vector &bbMin, Vector &bbMax )
{
	if ( !GetAutoUpdateBBox() )
		return;

	// Get the bbox into world space.
	Vector bbMinWorld, bbMaxWorld;
	if ( m_bLocalSpaceTransformIdentity )
	{
		bbMinWorld = bbMin;
		bbMaxWorld = bbMax;
	}
	else
	{
		TransformAABB( m_LocalSpaceTransform.As3x4(), bbMin, bbMax, bbMinWorld, bbMaxWorld );
	}

	// If there were ANY particles in the system, then we've got a valid bbox here. Otherwise,
	// we don't have anything, so leave m_Min and m_Max at the sort origin.
	if ( bboxSet )
	{
		m_Min = bbMinWorld;
		m_Max = bbMaxWorld;
	}
	else
	{
		m_Min = m_Max = m_pSim->GetSortOrigin();
	}
}

int CParticleEffectBinding::DrawModel( int flags, const RenderableInstance_t &instance )
{
	CVProfScope _( "CParticleEffectBinding::DrawModel", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	if ( !r_DrawParticles.GetInt() )
		return 0;

	contract_assert( flags != 0 );

	// If we're in commander mode and it's trying to draw the effect,
	// exit out. If the effect has FLAGS_ALWAYSSIMULATE set, then it'll come back
	// in here and simulate at the end of the frame.

	// NOTE: We do not check ParticleMgr()->ShouldRenderParticleSystems()
	// here as a sort of hack: the SFM currently plays back Tempents, which create
	// old-style particle systems back during playback, which means we want
	// them to display always
	if( !GetClientMode()->ShouldDrawParticles() )
		return 0;

	//Avoid drawing particles while building depth textures. Perf win.
	//At the very least, we absolutely should not do refraction updates below. So if this gets removed, be sure to wrap the refract/screen texture updates.
	if ( flags & ( STUDIO_SHADOWDEPTHTEXTURE | STUDIO_SSAODEPTHTEXTURE ) )
		return 0;

	SetDrawn( true );
	
	// Don't do anything if there are no particles.
	if( !m_nActiveParticles )
		return 1;

	// Reset the transformation matrix to identity.
	VMatrix mTempModel, mTempView;
	RenderStart( mTempModel, mTempView );

	bool bBucketSort = random_valve->RandomInt( 0, BUCKET_SORT_EVERY_N ) == 0;

	// Set frametime to zero if we've already rendered this frame.
	float flFrameTime = 0;
	if ( m_FrameCode != m_pParticleMgr->m_FrameCode )
	{
		m_FrameCode = m_pParticleMgr->m_FrameCode;
		flFrameTime = Helper_GetFrameTime();
	}

	// For each material, render...
	// This does an incremental bubble sort. It only does one pass every frame, and it will shuffle 
	// unsorted particles one step towards where they should be.
	bool bWireframe = false;
	for ( const auto &pMaterialOwner : m_Materials )
	{
		CEffectMaterial *pMaterial = pMaterialOwner.get();
		
		if ( pMaterial->m_pGroup->m_pPageMaterial && pMaterial->m_pGroup->m_pPageMaterial->NeedsPowerOfTwoFrameBufferTexture() )
		{
			UpdateRefractTexture();
		}

		if ( pMaterial->m_pGroup->m_pPageMaterial && pMaterial->m_pGroup->m_pPageMaterial->NeedsFullFrameBufferTexture() )
		{
			UpdateScreenEffectTexture();
		}
		
		DrawMaterialParticles( 
			bBucketSort,
			pMaterial, 
			flFrameTime,
			bWireframe );
	}

	if ( ShouldDrawInWireFrameMode() )
	{
		bWireframe = true;
		for ( const auto &pMaterialOwner : m_Materials )
		{
			CEffectMaterial *pMaterial = pMaterialOwner.get();
			
			DrawMaterialParticles( 
				bBucketSort,
				pMaterial, 
				flFrameTime,
				bWireframe );
		}
	}

	if ( cl_particles_show_bbox.GetBool() )
	{
		Vector center = (m_Min + m_Max)/2;
		Vector mins   = m_Min - center;
		Vector maxs   = m_Max - center;
	
		int r, g;
		if ( m_Flags & FLAGS_AUTOUPDATEBBOX )
		{
			// red is bad, the bbox update is costly
			r = 255;
			g = 0;
		}
		else
		{
			// green, this effect presents less cpu load 
			r = 0;
			g = 255;
		}
		
		debugoverlay->AddBoxOverlay( center, mins, maxs, QAngle( 0, 0, 0 ), r, g, 0, 16, 0 );
		debugoverlay->AddTextOverlayRGB( center, 0, 0, r, g, 0, 64, "%s:(%d)", m_pSim->GetEffectName(), m_nActiveParticles );
	}

	RenderEnd( mTempModel, mTempView );
	return 1;
}

PMaterialHandle CParticleEffectBinding::FindOrAddMaterial( const char *pMaterialName )
{
	if ( !m_pParticleMgr )
	{
		return nullptr;
	}

	return m_pParticleMgr->GetPMaterial( pMaterialName );
}

Particle* CParticleEffectBinding::AddParticle( int sizeInBytes, PMaterialHandle hMaterial )
{
	m_pParticleMgr->RepairPMaterial( hMaterial ); //HACKHACK: Remove this when we can stop leaking handles from level to level.

	// We've currently clamped the particle size to PARTICLE_SIZE,
	// we may need to change this algorithm if we get particles with
	// widely varying size
	if ( sizeInBytes > PARTICLE_SIZE )
	{
		contract_assert( sizeInBytes <= PARTICLE_SIZE );
		return nullptr;
	}

	// This is for testing - simulate it running out of memory.
	if ( particle_simulateoverflow.GetInt() )
	{
		if ( rand() % 10 <= 6 )
			return nullptr;
	}
	
	// Allocate the puppy. We are actually allocating space for the
	// internals + the actual data
	Particle* pParticle = m_pParticleMgr->AllocParticle( PARTICLE_SIZE );
	if( !pParticle )
		return nullptr;

	// Link it in
	CEffectMaterial *pEffectMat = GetEffectMaterial( hMaterial );
	InsertParticleAfter( pParticle, &pEffectMat->m_Particles );
	
	if ( hMaterial )
		pParticle->m_pSubTexture = hMaterial;
	else
		pParticle->m_pSubTexture = &m_pParticleMgr->m_DefaultInvalidSubTexture;

	++m_nActiveParticles;
	return pParticle;
}

void CParticleEffectBinding::SetBBox( const Vector &bbMin, const Vector &bbMax, bool bDisableAutoUpdate )
{
	m_Min = bbMin;
	m_Max = bbMax;
	
	if ( bDisableAutoUpdate )
		SetAutoUpdateBBox( false );
}

void CParticleEffectBinding::GetWorldspaceBounds( Vector *pMins, Vector *pMaxs )
{
	*pMins = m_Min;
	*pMaxs = m_Max;
}

void CParticleEffectBinding::SetLocalSpaceTransform( const matrix3x4_t &transform )
{
	m_LocalSpaceTransform.CopyFrom3x4( transform );
	if ( m_LocalSpaceTransform.IsIdentity() )
	{
		m_bLocalSpaceTransformIdentity = true;
	}
	else
	{
		m_bLocalSpaceTransformIdentity = false;
	}
}

bool CParticleEffectBinding::EnlargeBBoxToContain( const Vector &pt )
{
	if ( m_nActiveParticles == 0 )
	{
		m_Min = m_Max = pt;
		return true;
	}

	bool bHasChanged = false;

	// check min bounds
	if ( pt.x < m_Min.x ) 
		{ m_Min.x = pt.x; bHasChanged = true; }

	if ( pt.y < m_Min.y ) 
		{ m_Min.y = pt.y; bHasChanged = true; }

	if ( pt.z < m_Min.z ) 
		{ m_Min.z = pt.z; bHasChanged = true; }

	// check max bounds
	if ( pt.x > m_Max.x ) 
		{ m_Max.x = pt.x; bHasChanged = true; }

	if ( pt.y > m_Max.y ) 
		{ m_Max.y = pt.y; bHasChanged = true; }

	if ( pt.z > m_Max.z ) 
		{ m_Max.z = pt.z; bHasChanged = true; }

	return bHasChanged;
}

void CParticleEffectBinding::DetectChanges()
{
	// if we have no render handle, return
	if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	// if nothing changed, return
	if ( ( m_Min != m_LastMin ) || ( m_Max != m_LastMax ) ) 
	{
		// call leafsystem to update this guy
		ClientLeafSystem()->RenderableChanged( m_hRenderHandle );

		// remember last parameters
		m_LastMin = m_Min;
		m_LastMax = m_Max;
	}
}

void CParticleEffectBinding::GrowBBoxFromParticlePositions( CEffectMaterial *pMaterial, bool &bboxSet, Vector &bbMin, Vector &bbMax )
{
	// If its bbox is manually set, don't bother updating it here.
	if ( !GetAutoUpdateBBox() )
		return;

	for( Particle *pCur=pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur=pCur->m_pNext )
	{
		// Update bounding box 
		VectorMin( bbMin, pCur->m_Pos, bbMin );
		VectorMax( bbMax, pCur->m_Pos, bbMax );
		bboxSet = true;
	}
}

//-----------------------------------------------------------------------------
// Simulate particles
//-----------------------------------------------------------------------------
void CParticleEffectBinding::SimulateParticles( float flTimeDelta )
{
	if ( !m_pSim->ShouldSimulate() )
		return;

	if ( GetFlag( FLAGS_NEW_PARTICLE_SYSTEM ) )
	{
		CParticleSimulateIterator simulateIterator;
		simulateIterator.m_pEffectBinding = this;
		simulateIterator.m_pMaterial = nullptr; //pMaterial;
		simulateIterator.m_flTimeDelta = flTimeDelta;
		m_pSim->SimulateParticles( &simulateIterator );
	}
	else
	{
		Vector bbMin(0,0,0), bbMax(0,0,0);
		bool bboxSet = false;

		// slow the expensive update operation for particle systems that use auto-update-bbox
		// auto update the bbox after N frames then randomly 1/N or after 2*N frames 
		bool bFullBBoxUpdate = false;
		++m_UpdateBBoxCounter;
		if ( ( m_UpdateBBoxCounter >= BBOX_UPDATE_EVERY_N && random_valve->RandomInt( 0, BBOX_UPDATE_EVERY_N ) == 0 ) ||
			 ( m_UpdateBBoxCounter >= 2*BBOX_UPDATE_EVERY_N ) )
		{
			bFullBBoxUpdate = true;

			// reset watchdog
			m_UpdateBBoxCounter = 0;
		}

		if ( bFullBBoxUpdate )
		{
			BBoxCalcStart( bbMin, bbMax );
		}
		for ( const auto &pMaterialOwner : m_Materials )
		{
			CEffectMaterial *pMaterial = pMaterialOwner.get();

			CParticleSimulateIterator simulateIterator;

			simulateIterator.m_pEffectBinding = this;
			simulateIterator.m_pMaterial = pMaterial;
			simulateIterator.m_flTimeDelta = flTimeDelta;

			m_pSim->SimulateParticles( &simulateIterator );

			// Update the bbox.
			if ( bFullBBoxUpdate )
			{
				GrowBBoxFromParticlePositions( pMaterial, bboxSet, bbMin, bbMax );
			}
		}
		if ( bFullBBoxUpdate )
		{
			BBoxCalcEnd( bboxSet, bbMin, bbMax );
		}
	}
}

void CParticleEffectBinding::SetDrawThruLeafSystem( int bDraw )
{
	if ( bDraw )
	{
		// If SetDrawBeforeViewModel was called, then they shouldn't be telling it to draw through
		// the leaf system too.
		contract_assert( !( m_Flags & FLAGS_DRAW_BEFORE_VIEW_MODEL) );
	}

	SetFlag( FLAGS_DRAW_THRU_LEAF_SYSTEM, bDraw ); 
}

void CParticleEffectBinding::SetDrawBeforeViewModel( int bDraw )
{
	// Don't draw through the leaf system if they want it to specifically draw before the view model.
	if ( bDraw )
		m_Flags &= ~FLAGS_DRAW_THRU_LEAF_SYSTEM;
	
	SetFlag( FLAGS_DRAW_BEFORE_VIEW_MODEL, bDraw ); 
}

int CParticleEffectBinding::GetNumActiveParticles()
{
	return m_nActiveParticles;
}

// Build a list of all active particles
int CParticleEffectBinding::GetActiveParticleList( int nCount, Particle **ppParticleList )
{
	int nCurrCount = 0;

	for ( const auto &pMaterialOwner : m_Materials )
	{
		CEffectMaterial *pMaterial = pMaterialOwner.get();
		Particle *pParticle = pMaterial->m_Particles.m_pNext;
		for ( ; pParticle != &pMaterial->m_Particles; pParticle = pParticle->m_pNext )
		{
			ppParticleList[nCurrCount] = pParticle;
			if ( ++nCurrCount == nCount )
				return nCurrCount;
		}
	}

	return nCurrCount;
}

int CParticleEffectBinding::DrawMaterialParticles( 
	bool bBucketSort,
	CEffectMaterial *pMaterial, 
	float flTimeDelta,
	bool bWireframe
	 )
{
	// Setup everything.
	CMeshBuilder builder;
	ParticleDraw particleDraw;
	IMesh *pMesh = nullptr;

	StartDrawMaterialParticles( pMaterial, flTimeDelta, pMesh, builder, particleDraw, bWireframe );

	if ( m_nActiveParticles > MAX_TOTAL_PARTICLES )
		Error( "CParticleEffectBinding::DrawMaterialParticles: too many particles (%d should be less than %d)", m_nActiveParticles, MAX_TOTAL_PARTICLES );

	// Simluate and render all the particles.
	CParticleRenderIterator renderIterator;

	renderIterator.m_pEffectBinding = this;
	renderIterator.m_pMaterial = pMaterial;
	renderIterator.m_pParticleDraw = &particleDraw;
	renderIterator.m_pMeshBuilder = &builder;
	renderIterator.m_pMesh = pMesh;
	renderIterator.m_bBucketSort = bBucketSort;
	renderIterator.m_pMaterialSystem = m_pParticleMgr->m_pMaterialSystem;

	m_pSim->RenderParticles( &renderIterator );
	g_nParticlesDrawn += m_nActiveParticles;

	if( bBucketSort )
	{
		DoBucketSort( pMaterial, renderIterator.m_zCoords, renderIterator.m_nZCoords, renderIterator.m_MinZ, renderIterator.m_MaxZ );
	}

	// Flush out any remaining particles.
	builder.End( false, true );
	
	return m_nActiveParticles;
}

void CParticleEffectBinding::RenderStart( VMatrix &tempModel, VMatrix &tempView )
{
	if( IsEffectCameraSpace() )
	{
		CMatRenderContextPtr pRenderContext( m_pParticleMgr->m_pMaterialSystem );

		// Store matrices off so we can restore them in RenderEnd().
		pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);
		pRenderContext->GetMatrix(MATERIAL_MODEL, &tempModel);

		// We're gonna assume the model matrix was identity and blow it off
		// This means that the particle positions are all specified in world space
		// which makes bounding box computations faster. 
		m_pParticleMgr->m_mModelView = tempView;

		// Force the user clip planes to use the old view matrix
		pRenderContext->EnableUserClipTransformOverride( true );
		pRenderContext->UserClipTransform( tempView );

		// The particle renderers want to do things in camera space
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->LoadIdentity();
	}
	else
	{
		m_pParticleMgr->m_mModelView.Identity();
	}

	// Add their local space transform if they have one and they want it applied.
	if ( GetAutoApplyLocalTransform() && !m_bLocalSpaceTransformIdentity )
	{
		m_pParticleMgr->m_mModelView = m_pParticleMgr->m_mModelView * m_LocalSpaceTransform;
	}

	// Let the particle effect do any per-frame setup/processing here
	m_pSim->StartRender( m_pParticleMgr->m_mModelView );
}

void CParticleEffectBinding::RenderEnd( VMatrix &tempModel, VMatrix &tempView )
{
	if( IsEffectCameraSpace() )
	{
		CMatRenderContextPtr pRenderContext( m_pParticleMgr->m_pMaterialSystem );

		// Make user clip planes work normally
		pRenderContext->EnableUserClipTransformOverride( false );

		// Reset the model matrix.
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadMatrix( tempModel );

		// Reset the view matrix.
		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->LoadMatrix( tempView );
	}
}

void CParticleEffectBinding::DoBucketSort( CEffectMaterial *pMaterial, float *zCoords, int nZCoords, float minZ, float maxZ )
{
	// Do an O(N) bucket sort. This helps the sort when there are lots of particles.
	constexpr auto NUM_BUCKETS = 32;
	Particle buckets[NUM_BUCKETS];
	for( int iBucket=0; iBucket < NUM_BUCKETS; iBucket++ )
	{
		buckets[iBucket].m_pPrev = buckets[iBucket].m_pNext = &buckets[iBucket];
	}
	
	// Sort into buckets.
	int iCurParticle = 0;
	Particle *pNext, *pCur;
	for( pCur=pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur=pNext )
	{
		pNext = pCur->m_pNext;
		if( iCurParticle >= nZCoords )
			break;

		// Remove it..
		UnlinkParticle( pCur );

		// Add it to the appropriate bucket.
		float flPercent;
		if (maxZ == minZ)
			flPercent = 0;
		else
			flPercent = (zCoords[iCurParticle] - minZ) / (maxZ - minZ);

		int iAddBucket = (int)( flPercent * (NUM_BUCKETS - 0.0001f) );
		iAddBucket = NUM_BUCKETS - iAddBucket - 1;
		contract_assert( iAddBucket >= 0 && iAddBucket < NUM_BUCKETS );

		InsertParticleAfter( pCur, &buckets[iAddBucket] );

		++iCurParticle;
	}

	// Put the buckets back into the main list.
	for( int iReAddBucket=0; iReAddBucket < NUM_BUCKETS; iReAddBucket++ )
	{
		Particle *pListHead = &buckets[iReAddBucket];
		for( pCur=pListHead->m_pNext; pCur != pListHead; pCur=pNext )
		{
			pNext = pCur->m_pNext;
			InsertParticleAfter( pCur, &pMaterial->m_Particles );
			--iCurParticle;
		}
	}

	contract_assert( iCurParticle==0 );
}

void CParticleEffectBinding::Init( CParticleMgr *pMgr, IParticleEffect *pSim )
{
	// Must Term before reinitializing.
	contract_assert( !m_pSim && !m_pParticleMgr );

	m_pSim = pSim;
	m_pParticleMgr = pMgr;
}

void CParticleEffectBinding::Term()
{
	if ( !m_pParticleMgr )
		return;

	// Free materials.
	for ( const auto &pMaterialOwner : m_Materials )
	{
		CEffectMaterial *pMaterial = pMaterialOwner.get();

		// Remove all particles tied to this effect.
		Particle *pNext = nullptr;
		for(Particle *pCur = pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur=pNext )
		{
			pNext = pCur->m_pNext;
			
			RemoveParticle( pCur );
		}
	}	
	m_Materials.clear();

	memset( m_EffectMaterialHash, 0, sizeof( m_EffectMaterialHash ) );
}

void CParticleEffectBinding::RemoveParticle( Particle *pParticle )
{
	UnlinkParticle( pParticle );
	
	// Important that this is updated BEFORE NotifyDestroyParticle is called.
	--m_nActiveParticles;
	contract_assert( m_nActiveParticles >= 0 );

	// Let the effect do any necessary cleanup
	m_pSim->NotifyDestroyParticle(pParticle);

	// Remove it from the list of particles and deallocate
	m_pParticleMgr->FreeParticle(pParticle);
}

bool CParticleEffectBinding::RecalculateBoundingBox()
{
	if ( m_nActiveParticles == 0 )
	{
		m_Max = m_Min = m_pSim->GetSortOrigin();
		return false;
	}

	Vector bbMin(  1e28,  1e28,  1e28 );
	Vector bbMax( -1e28, -1e28, -1e28 );

	for ( const auto &pMaterialOwner : m_Materials )
	{
		CEffectMaterial *pMaterial = pMaterialOwner.get();
		
		for( Particle *pCur=pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur=pCur->m_pNext )
		{
			VectorMin( bbMin, pCur->m_Pos, bbMin );
			VectorMax( bbMax, pCur->m_Pos, bbMax );
		}
	}

	// Get the bbox into world space.
	if ( m_bLocalSpaceTransformIdentity )
	{
		m_Min = bbMin;
		m_Max = bbMax;
	}
	else
	{
		TransformAABB( m_LocalSpaceTransform.As3x4(), bbMin, bbMax, m_Min, m_Max );
	}

	return true;
}

CEffectMaterial* CParticleEffectBinding::GetEffectMaterial( CParticleSubTexture *pSubTexture )
{
	// Hash the IMaterial pointer.
	unsigned long index = (((unsigned long)pSubTexture->m_pGroup) >> 6) % EFFECT_MATERIAL_HASH_SIZE;
	for ( CEffectMaterial *pCur=m_EffectMaterialHash[index]; pCur; pCur = pCur->m_pHashedNext )
	{
		if ( pCur->m_pGroup == pSubTexture->m_pGroup )
			return pCur;
	}

	CEffectMaterial *pEffectMat = new CEffectMaterial;
	pEffectMat->m_pGroup = pSubTexture->m_pGroup;
	pEffectMat->m_pHashedNext = m_EffectMaterialHash[index];
	m_EffectMaterialHash[index] = pEffectMat;

	m_Materials.emplace_back( pEffectMat );
	return pEffectMat;
}

//-----------------------------------------------------------------------------
// CParticleMgr
//-----------------------------------------------------------------------------
CParticleMgr::CParticleMgr()
{
	m_nToolParticleEffectId = 0;
	m_bUpdatingEffects = false;
	m_bRenderParticleEffects = true;
	m_pMaterialSystem = nullptr;
	m_pThreadPool[0] = 0;
	m_pThreadPool[1] = 0;
	memset( &m_DirectionalLight, 0, sizeof( m_DirectionalLight ) );

	m_FrameCode = 1;

	m_DefaultInvalidSubTexture.m_pGroup = &m_DefaultInvalidSubTexture.m_DefaultGroup;
	m_DefaultInvalidSubTexture.m_pMaterial = nullptr;
	m_DefaultInvalidSubTexture.m_tCoordMins[0] = m_DefaultInvalidSubTexture.m_tCoordMins[1] = 0;
	m_DefaultInvalidSubTexture.m_tCoordMaxs[0] = m_DefaultInvalidSubTexture.m_tCoordMaxs[1] = 1;
	
	m_nCurrentParticlesAllocated = 0;
}

CParticleMgr::~CParticleMgr()
{
	Term(false);
}

//-----------------------------------------------------------------------------
// Initialization and shutdown
//-----------------------------------------------------------------------------
bool CParticleMgr::Init(unsigned long count, IMaterialSystem *pMaterials)
{
	Term();

	m_pMaterialSystem = pMaterials;

	// Initialize the particle system
	bool bPrecacheParticles = true;
	g_pParticleSystemMgr->Init( g_pParticleSystemQuery, bPrecacheParticles );
	// tell particle mgr to add the default simulation + rendering ops
	g_pParticleSystemMgr->AddBuiltinSimulationOperators();
	g_pParticleSystemMgr->AddBuiltinRenderingOperators();

	// Send true to load the sheets
	ParseParticleEffects( true );

	return true;
}

void CParticleMgr::Term(bool bCanReferenceOtherStaticObjects)
{
	// Free all the effects.
	for ( auto it = m_Effects.begin(); it != m_Effects.end(); )
	{
		CParticleEffectBinding *pEffect = *it++;
		pEffect->m_pSim->NotifyRemove();
	}
	m_Effects.clear();
	m_NewEffects.delete_all();

	for( auto &[name, pSubTexture] : m_SubTextures )
	{
		IMaterial *pMaterial = pSubTexture->m_pMaterial;
		if ( pMaterial )
			pMaterial->Release();
	}
	m_SubTextures.clear();

	for( int i = std::ssize( m_SubTextureGroups ); --i >= 0; )
	{	
		IMaterial *pMaterial = m_SubTextureGroups[i]->m_pPageMaterial;
		if ( pMaterial )
			pMaterial->Release();
	}
	m_SubTextureGroups.clear();

	if (bCanReferenceOtherStaticObjects)
		g_pParticleSystemMgr->UncacheAllParticleSystems();

	if ( m_pMaterialSystem )
	{
		m_pMaterialSystem->UncacheUnusedMaterials();
	}
	m_pMaterialSystem = nullptr;
	
	if ( m_pThreadPool[0] )
	{
		m_pThreadPool[0]->Stop();
		DestroyThreadPool( m_pThreadPool[0] );
		m_pThreadPool[0] = nullptr;
	}
	if ( m_pThreadPool[1] )
	{
		m_pThreadPool[1]->Stop();
		DestroyThreadPool( m_pThreadPool[1] );
		m_pThreadPool[1] = nullptr;
	}

	contract_assert( m_nCurrentParticlesAllocated == 0 );
}

void CParticleMgr::LevelInit()
{
	g_pParticleSystemMgr->SetLastSimulationTime( gpGlobals->curtime );
}

Particle *CParticleMgr::AllocParticle( int size )
{
	// Enforce max particle limit.
	if ( m_nCurrentParticlesAllocated >= MAX_TOTAL_PARTICLES )
		return nullptr;
		
	Particle *pRet = (Particle *)MemAlloc_Alloc( size );
	if ( pRet )
		++m_nCurrentParticlesAllocated;

	return pRet;
}

void CParticleMgr::FreeParticle( Particle *pParticle )
{
	contract_assert( m_nCurrentParticlesAllocated > 0 );
	if ( pParticle )
		--m_nCurrentParticlesAllocated;
	
	g_pMemAlloc->Free( pParticle );
}

//-----------------------------------------------------------------------------
// Should particle effects be rendered?
//-----------------------------------------------------------------------------
void CParticleMgr::RenderParticleSystems( bool bEnable )
{
	m_bRenderParticleEffects = bEnable;
}

bool CParticleMgr::ShouldRenderParticleSystems() const
{
	return m_bRenderParticleEffects;
}

//-----------------------------------------------------------------------------
// add a class that gets notified of entity events
//-----------------------------------------------------------------------------
void CParticleMgr::AddEffectListener( IClientParticleListener *pListener )
{
	int i = ks::find_index( m_effectListeners, pListener );
	if ( !ks::is_valid_index( m_effectListeners, i ) )
	{
		m_effectListeners.push_back( pListener );
	}
}

void CParticleMgr::RemoveEffectListener( IClientParticleListener *pListener )
{
	int i = ks::find_index( m_effectListeners, pListener );
	if ( ks::is_valid_index( m_effectListeners, i ) )
	{
		m_effectListeners.erase( m_effectListeners.begin() + i );
	}
}

//-----------------------------------------------------------------------------
// registers effects classes, and create instances of these effects classes
//-----------------------------------------------------------------------------

void CParticleMgr::RegisterEffect( const char *pEffectType, CreateParticleEffectFN func )
{
	contract_assert( !m_effectFactories.contains( pEffectType ) );

	m_effectFactories.emplace( pEffectType, func );
}

IParticleEffect *CParticleMgr::CreateEffect( const char *pEffectType )
{
	auto it = m_effectFactories.find( pEffectType );
	if ( it == m_effectFactories.end() )
	{
		Msg( "CParticleMgr::CreateEffect: factory not found for effect '%s'\n", pEffectType );
		return nullptr;
	}

	CreateParticleEffectFN func = it->second;
	if ( func == nullptr )
	{
		Msg( "CParticleMgr::CreateEffect: NULL factory for effect '%s'\n", pEffectType );
		return nullptr;
	}

	return func();
}

//-----------------------------------------------------------------------------
// Adds and removes effects from our global list
//-----------------------------------------------------------------------------
void CParticleMgr::AddEffect( CNewParticleEffect *pEffect )
{
	m_NewEffects.push_front( pEffect );

	if ( pEffect->IsValid() && pEffect->m_pDef->IsDrawnThroughLeafSystem() )
	{
		RenderableTranslucencyType_t nType;
		if ( pEffect->IsTranslucent() )
		{
			nType = pEffect->IsTwoPass() ? RENDERABLE_IS_TWO_PASS : RENDERABLE_IS_TRANSLUCENT;
		}
		else
		{
			nType = RENDERABLE_IS_OPAQUE;
		}
		ClientLeafSystem()->CreateRenderableHandle( pEffect, pEffect->m_pDef->IsViewModelEffect(), nType, RENDERABLE_MODEL_ENTITY );
		ClientLeafSystem()->EnableBloatedBounds( pEffect->RenderHandle(), true );

		CBaseEntity *pOwner = pEffect->GetOwner();
		ClientLeafSystem()->RenderInFastReflections( pEffect->RenderHandle(), pOwner ? pOwner->IsRenderingInFastReflections() : false );
	}
}

bool CParticleMgr::AddEffect( CParticleEffectBinding *pEffect, IParticleEffect *pSim )
{
	pEffect->Init( this, pSim );

	// Add it to the leaf system.
	ClientLeafSystem()->CreateRenderableHandle( pEffect, false, RENDERABLE_IS_TRANSLUCENT, RENDERABLE_MODEL_ENTITY );
	ClientLeafSystem()->EnableBloatedBounds( pEffect->RenderHandle(), true );

	pEffect->m_ListIndex = m_Effects.insert( m_Effects.end(), pEffect );

	// notify listeners
	int nListeners = std::ssize( m_effectListeners );
	for ( int i = 0; i < nListeners; ++i )
	{
		m_effectListeners[ i ]->OnParticleEffectAdded( pSim );
	}

	return true;
}

void CParticleMgr::RemoveEffect( CParticleEffectBinding *pEffect )
{
	// This prevents certain recursive situations where a NotifyRemove
	// call can wind up triggering another one, usually in an effect's
	// destructor.
	contract_assert( pEffect );
	if( !pEffect || pEffect->GetRemovalInProgressFlag() )
		return;
	pEffect->SetRemovalInProgressFlag();

	// Don't call RemoveEffect while inside an IParticleEffect's Update() function.
	// Return false from the Update function instead.
	contract_assert( !m_bUpdatingEffects );

	// notify listeners
	int nListeners = std::ssize( m_effectListeners );
	for ( int i = 0; i < nListeners; ++i )
	{
		m_effectListeners[ i ]->OnParticleEffectRemoved( pEffect->m_pSim );
	}

	// Take it out of the leaf system.
	ClientLeafSystem()->RemoveRenderable( pEffect->m_hRenderHandle );

	// Saved first: NotifyRemove may delete pEffect
	auto listIndex = pEffect->m_ListIndex;
	if ( pEffect->m_pSim )
	{
		pEffect->m_pSim->NotifyRemove();
		if ( listIndex )
			m_Effects.erase( *listIndex );
		
	}
	else
	{
		contract_assert( !listIndex );
	}
}

void CParticleMgr::RemoveEffect( CNewParticleEffect *pEffect )
{
	contract_assert( pEffect );
	if ( !pEffect )
		return;

	// Don't call RemoveEffect while inside an IParticleEffect's Update() function.
	// Return false from the Update function instead.
	contract_assert( !m_bUpdatingEffects );

	// Take it out of the leaf system.
	ClientLeafSystem()->RemoveRenderable( pEffect->m_hRenderHandle );

	m_NewEffects.erase( pEffect );
	pEffect->NotifyRemove();
}

void CParticleMgr::RemoveAllNewEffects()
{
	// Remove any of the new effects that were flagged to be removed.
	for( CNewParticleEffect *pNewEffect = m_NewEffects.head(); pNewEffect;  )
	{
		CNewParticleEffect *pNextEffect = pNewEffect->m_pNext;
		// see it any entitiy has a particle prop pointing at this one. this loop through all
		// entities shouldn't be important perf-wise because it only happens on reload
		CBaseEntityIterator iterator;
		CBaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != nullptr )
		{
			if ( pEnt->ParticleProp() )
			{
				pEnt->ParticleProp()->OnParticleSystemDeleted( pNewEffect );
			}
		}		
		RemoveEffect( pNewEffect );
		pNewEffect = pNextEffect;
	}
}

void CParticleMgr::RemoveAllEffects()
{
	for ( auto it = m_Effects.begin(); it != m_Effects.end(); )
	{
		CParticleEffectBinding *pEffect = *it++;
		RemoveEffect( pEffect );
	}

	RemoveAllNewEffects();

	// Don't release materials here - handles are kept alive for reuse across levels,
	// and RepairPMaterial() will re-acquire them. Materials are released once in Term().
	// Just clear the pointers so RepairPMaterial() knows to re-acquire.
	for( auto &[name, pSubTexture] : m_SubTextures )
	{
		pSubTexture->m_pMaterial = nullptr;
		pSubTexture->m_DefaultGroup.m_pPageMaterial = nullptr;
	}

	for( int i = std::ssize( m_SubTextureGroups ); --i >= 0; )
	{
		m_SubTextureGroups[i]->m_pPageMaterial = nullptr;
	}
}

CNewParticleEffect *CParticleMgr::FirstNewEffect()
{
	return m_NewEffects.head();
}

CNewParticleEffect *CParticleMgr::NextNewEffect( CNewParticleEffect *pEffect )
{
	return pEffect ? pEffect->m_pNext : nullptr;
}

void CParticleMgr::IncrementFrameCode()
{
	CVProfScope _( "CParticleMgr::IncrementFrameCode()", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	++m_FrameCode;
	if ( m_FrameCode == 0 )
	{
		// Reset all the CParticleEffectBindings..
		for ( CParticleEffectBinding *pEffect : m_Effects )
		{
			pEffect->m_FrameCode = 0;
		}

		m_FrameCode = 1;
	}
	//!!new!!
}

//-----------------------------------------------------------------------------
// Main rendering loop
//-----------------------------------------------------------------------------
void CParticleMgr::Simulate( float flTimeDelta )
{
	g_nParticlesDrawn = 0;

	if(!m_pMaterialSystem)
	{
		contract_assert( false );
		return;
	}

	// Update all the effects.
	UpdateAllEffects( flTimeDelta );
}

bool g_bMeasureParticlePerformance;
bool g_bDisplayParticlePerformance;

static int64 g_nNumParticlesSimulated;
static int64 g_nNumUSSpentSimulatingParticles;
static double g_flStartSimTime;

int GetParticlePerformance()
{
	if (! g_nNumUSSpentSimulatingParticles )
		return 0;
	return (1000*g_nNumParticlesSimulated) / g_nNumUSSpentSimulatingParticles;
}

void CParticleMgr::PostRender()
{
	CVProfScope _( "CParticleMgr::SimulateUndrawnEffects", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Simulate all effects that weren't drawn (if they have their 'always simulate' flag set).
	for ( CParticleEffectBinding *pEffect : m_Effects )
	{
		// Tell the effect if it was drawn or not.
		pEffect->SetWasDrawnPrevFrame( pEffect->WasDrawn() );

		// Now that we've rendered, clear this flag so it'll simulate next frame.
		pEffect->SetFlag( CParticleEffectBinding::FLAGS_FIRST_FRAME, false );	
	}
}

void CParticleMgr::DrawBeforeViewModelEffects()
{
	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	for ( CParticleEffectBinding *pEffect : m_Effects )
	{
		if ( pEffect->GetFlag( CParticleEffectBinding::FLAGS_DRAW_BEFORE_VIEW_MODEL ) )
		{
			contract_assert( !pEffect->WasDrawn() );
			pEffect->DrawModel( STUDIO_RENDER, instance );
		}
	}
}

void ResetParticlePerformanceCounters( void )
{
	g_nNumUSSpentSimulatingParticles = 0;
	g_nNumParticlesSimulated = 0;
}

void BeginSimulateParticles( void )
{
	g_flStartSimTime = Plat_FloatTime();
}

static ConVar r_particle_sim_spike_threshold_ms( side_registry(), "r_particle_sim_spike_threshold_ms", "0" );

void EndSimulateParticles( void )
{
	float flETime = Plat_FloatTime() - g_flStartSimTime;

	if ( g_bMeasureParticlePerformance )
	{
		g_nNumUSSpentSimulatingParticles += 1.0e6 * flETime;
	}

	g_pParticleSystemMgr->SetLastSimulationDuration( flETime );
	g_pParticleSystemMgr->CommitProfileInformation( flETime > .001 * r_particle_sim_spike_threshold_ms.GetInt() );
}

static ConVar r_threaded_particles( side_registry(), "r_threaded_particles", "1" );

static float s_flThreadedPSystemTimeStep;

static void PreProcessPSystem()
{
	mdlcache->BeginCoarseLock();
	mdlcache->BeginLock();
}

static void PostProcessPSystem()
{
	mdlcache->EndLock();
	mdlcache->EndCoarseLock();
}

static void ProcessPSystem( CNewParticleEffect *&pNewEffect )
{
	// If this is a new effect, then update its bbox so it goes in the
	// right leaves (if it has particles).
	if ( pNewEffect->m_bQueuedStartEmission )
	{
		pNewEffect->m_bQueuedStartEmission = false;
		pNewEffect->StartEmission();
	}
	int bFirstUpdate = pNewEffect->GetNeedsBBoxUpdate();
	if ( bFirstUpdate )
	{
		// If the effect already disabled auto-updating of the bbox, then it should have
		// set the bbox by now and we can ignore this responsibility here.
		if ( !pNewEffect->GetAutoUpdateBBox() || pNewEffect->RecalculateBoundingBox() )
		{
			pNewEffect->SetNeedsBBoxUpdate( false );
		}
	}

	// This flag will get set to true if the effect is drawn through the leaf system.
	pNewEffect->SetDrawn( false );

	if ( pNewEffect->GetFirstFrameFlag() )
	{
		pNewEffect->Simulate( 0.0f );
		pNewEffect->SetFirstFrameFlag( false );
	}
	else if ( pNewEffect->ShouldSimulate() )
	{
		pNewEffect->Simulate( s_flThreadedPSystemTimeStep );
	}

	if ( pNewEffect->IsFinished() )
	{
		pNewEffect->SetRemoveFlag();
	}
}

static void ProcessNonDrawingSystem( CParticleCollection *&pNonDrawingEffect )
{
	pNonDrawingEffect->Simulate( s_flThreadedPSystemTimeStep );
}

int CParticleMgr::ComputeParticleDefScreenArea( int nInfoCount, RetireInfo_t *pInfo, float *pTotalArea, CParticleSystemDefinition* pDef, 
	const CViewSetup& view, const VMatrix &worldToPixels, float flFocalDist )
{
	int nCollection = 0;
	float flCullCost = pDef->GetCullFillCost();
	float flCullRadius = pDef->GetCullRadius();
	float flCullRadiusSqr = flCullRadius * flCullRadius;
	*pTotalArea = 0.0f;

	CParticleCollection *pCollection = pDef->FirstCollection();
	for ( ; pCollection; pCollection = pCollection->GetNextCollectionUsingSameDef() )
	{
		CNewParticleEffect *pEffect = static_cast< CNewParticleEffect* >( pCollection );
		if ( !pEffect->ShouldPerformCullCheck() )
			continue;

		// Don't count parents
		contract_assert( !pCollection->m_pParent );
		contract_assert( nCollection < nInfoCount && pDef == pCollection->m_pDef );

		pInfo[nCollection].m_flScreenArea = 0.0f;
		pInfo[nCollection].m_pCollection = pCollection;
		pInfo[nCollection].m_bFirstFrame = false;

		Vector vecCenter, vecScreenCenter, vecCenterCam;
		vecCenter = pCollection->GetControlPointAtCurrentTime( pDef->GetCullControlPoint() );

		Vector3DMultiplyPositionProjective( worldToPixels, vecCenter, vecScreenCenter );
		float lSqr = vecCenter.DistToSqr( view.origin );

		float flProjRadius = ( lSqr > flCullRadiusSqr ) ? 0.5f * flFocalDist * flCullRadius / std::sqrt( lSqr - flCullRadiusSqr ) : 1.0f;
		flProjRadius *= view.width;

		float flMinX = std::max<std::common_type_t<int, vec_t>>( view.x, vecScreenCenter.x - flProjRadius );
		float flMaxX = std::min<std::common_type_t<int, vec_t>>( view.x + view.width, vecScreenCenter.x + flProjRadius );

		float flMinY = std::max<std::common_type_t<int, vec_t>>( view.y, vecScreenCenter.y - flProjRadius );
		float flMaxY = std::min<std::common_type_t<int, vec_t>>( view.y + view.height, vecScreenCenter.y + flProjRadius );

		// Clamp the min/max values to the screen so that particles particle systems outside of the view don't cause early retirement.
		flMinX = clamp( flMinX, view.x, view.width );
		flMaxX = clamp( flMaxX, view.x, view.width );
		flMinY = clamp( flMinY, view.y, view.height );
		flMaxY = clamp( flMaxY, view.y, view.height );

		float flArea = ( flMaxX - flMinX ) * ( flMaxY - flMinY );
		flArea *= flCullCost;
		*pTotalArea += flArea; 

		pInfo[nCollection].m_flScreenArea = flArea;
		pInfo[nCollection].m_pCollection = pCollection;
		pInfo[nCollection].m_bFirstFrame = pEffect->GetFirstFrameFlag();
		++nCollection;
	}

	return nCollection;
}

int CParticleMgr::RetireSort( const void *p1, const void *p2 ) 
{
	RetireInfo_t *pRetire1 = (RetireInfo_t*)p1;
	RetireInfo_t *pRetire2 = (RetireInfo_t*)p2;
	float flArea = pRetire1->m_flScreenArea - pRetire2->m_flScreenArea;
	if ( flArea == 0.0f )
		return 0;
	return ( flArea > 0 ) ? -1 : 1;
}

bool CParticleMgr::RetireParticleCollections( CParticleSystemDefinition* pDef, 
	int nCount, RetireInfo_t *pInfo, float flScreenArea, float flMaxTotalArea )
{
	bool bRetirementOccurred = false;

	// Don't cull out the particle system if there's only 1 and no replacement
	const char *pReplacementDef = pDef->GetCullReplacementDefinition();
	if ( ( !pReplacementDef || !pReplacementDef[0] ) && ( nCount <= 1 ) )
		return false;

	// Quicksort the retirement info
	qsort( pInfo, nCount, sizeof(RetireInfo_t), RetireSort );

	for ( int i = 0; i < nCount; ++i )
	{
		if ( flScreenArea <= flMaxTotalArea )
			break;

		// We can only replace stuff that's being emitted this frame
		if ( !pInfo[i].m_bFirstFrame )
			continue;

		CNewParticleEffect* pRetireEffect = static_cast< CNewParticleEffect* >( pInfo[i].m_pCollection );
		CNewParticleEffect* pNewEffect = pRetireEffect->ReplaceWith( pReplacementDef );
		if ( pNewEffect )
		{
			pNewEffect->Update( s_flThreadedPSystemTimeStep );
		}
		bRetirementOccurred = true;
		flScreenArea -= pInfo[i].m_flScreenArea;
	}

	return bRetirementOccurred;
}

// Next, see if there are new particle systems that need early retirement
static ConVar cl_particle_retire_cost( side_registry(), "cl_particle_retire_cost", "0", FCVAR_CHEAT );

bool CParticleMgr::EarlyRetireParticleSystems( int nCount, CNewParticleEffect **ppEffects )
{
	ks::ScratchArena<> scratch;
	// NOTE: Doing a cheap and hacky estimate of worst-case fillrate
	const CViewSetup *pViewSetup[ MAX_SPLITSCREEN_PLAYERS ];
	VMatrix worldToScreen[ MAX_SPLITSCREEN_PLAYERS ];
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		pViewSetup[ hh ] = view->GetPlayerViewSetup( hh );
		if ( pViewSetup[ hh ]->width == 0 || pViewSetup[ hh ]->height == 0 )
			return false;
	}

	float flMaxScreenArea = cl_particle_retire_cost.GetFloat() * 1000.0f;
	if ( flMaxScreenArea == 0.0f )
		return false;

	int nDefCount = 0;
	CParticleSystemDefinition **ppDefs = scratch.Alloc<CParticleSystemDefinition*>( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition *pDef = ppEffects[i]->m_pDef;

		// Skip stuff that doesn't have a cull radius set
		if ( pDef->GetCullRadius() == 0.0f )
			continue;

		// Only perform the cull check on creation
		if ( !ppEffects[i]->GetFirstFrameFlag() )
			continue;

		if ( pDef->HasRetirementBeenChecked( gpGlobals->framecount ) )
			continue;

		pDef->MarkRetirementCheck( gpGlobals->framecount );

		ppDefs[nDefCount++] = ppEffects[i]->m_pDef;
	}

	if ( nDefCount == 0 )
		return false;

	for ( int i = 0; i < nCount; ++i )
	{
		ppEffects[i]->MarkShouldPerformCullCheck( true );
	}

	float flFocalDist[ MAX_SPLITSCREEN_PLAYERS ];

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( ii )
	{
		VMatrix dummy1, dummy2, dummy3;
		render->GetMatricesForView( *pViewSetup[ ii ], &dummy1, &dummy2, &dummy3, &worldToScreen[ ii ] );
		flFocalDist[ ii ] = std::tan( DEG2RAD( pViewSetup[ ii ]->fov * 0.5f ) );
	}

	bool bRetiredCollections = true;
	float flScreenArea;
	int nSize = nCount * sizeof(RetireInfo_t);
	RetireInfo_t *pInfo = (RetireInfo_t*)scratch.AllocBytes( nSize );
	for ( int i = 0; i < nDefCount; ++i )
	{
		CParticleSystemDefinition* pDef = ppDefs[i];
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			int nActualCount = ComputeParticleDefScreenArea( nCount, pInfo, &flScreenArea, pDef, *pViewSetup[ hh ], worldToScreen[ hh ], flFocalDist[ hh ] );
			if ( flScreenArea > flMaxScreenArea )
			{
				if ( RetireParticleCollections( pDef, nActualCount, pInfo, flScreenArea, flMaxScreenArea ) )
				{
					bRetiredCollections = true;
					break;
				}
			}
		}
	}

	for ( int i = 0; i < nCount; ++i )
	{
		ppEffects[i]->MarkShouldPerformCullCheck( false );
	}
	return bRetiredCollections;
}

static ConVar particle_sim_alt_cores( side_registry(), "particle_sim_alt_cores", "2" );

void CParticleMgr::BuildParticleSimList( std::vector< CNewParticleEffect* > &list )
{
	float flNow = g_pParticleSystemMgr->GetLastSimulationTime();
	for( CNewParticleEffect *pNewEffect=m_NewEffects.head(); pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		if ( flNow >= pNewEffect->m_flNextSleepTime && pNewEffect->m_nActiveParticles > 0 )
		{
			if ( pNewEffect->GetOwner() )
			{
				//Not in PVS so we really want to respect NextSleepTime
				if ( !g_pClientLeafSystem->IsRenderableInPVS( pNewEffect->GetOwner()->GetClientRenderable() ) )
					continue;
			}
		}
		if ( pNewEffect->GetRemoveFlag() )
			continue;
		if ( g_bMeasureParticlePerformance )
		{
			g_nNumParticlesSimulated += pNewEffect->m_nActiveParticles;
		}
		list.push_back( pNewEffect );
	}
}

static ConVar r_particle_timescale( side_registry(), "r_particle_timescale", "1.0" );

static int CountChildParticleSystems( CParticleCollection *p )
{
	int nCount = 1;
	for ( CParticleCollection *pChild = p->m_Children.head(); pChild; pChild = pChild->m_pNext )
	{
		nCount += CountChildParticleSystems( pChild );
	}
	return nCount;
}

static ConVar cl_particle_max_count( side_registry(), "cl_particle_max_count", "0" );

struct ParticleInfo_t
{
	ParticleInfo_t() : m_nCount(0), m_nChildCount(0) {}
	int m_nCount;
	int m_nChildCount;
};

void CParticleMgr::SpewActiveParticleSystems( )
{
	ks::idict< ParticleInfo_t > histo;
	for( CNewParticleEffect *pNewEffect=m_NewEffects.head(); pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		ParticleInfo_t &info = histo[ pNewEffect->GetName() ];
		if ( ++info.m_nCount == 1 )
		{
			info.m_nChildCount = CountChildParticleSystems( pNewEffect );
		}
	}

	Msg( "Too many simultaneously active particle systems!\n" );
	Msg( "Name\t\t\t\t\tCount\t\tChild Count Per Instance\n" );
	for ( const auto &[name, info] : histo )
	{
		Msg( "%30s\t\t%d\t\t%d\n", name.c_str(), info.m_nCount, info.m_nChildCount );
	}
}

void CParticleMgr::UpdateNewEffects( float flTimeDelta )
{
// #ifdef TF_CLIENT_DLL
// 	extern bool g_bDontMakeSkipToTimeTakeForever;
// 	g_bDontMakeSkipToTimeTakeForever = true;
// #endif
	flTimeDelta *= r_particle_timescale.GetFloat();
	CVProfScope _( "CParticleMSG::UpdateNewEffects", 0, "Particle Simulation", false, BUDGETFLAG_OTHER );

	g_pParticleSystemMgr->SetLastSimulationTime( gpGlobals->curtime );

	int nParticleSystemCount = 0;
	int nMaxParticleCount = cl_particle_max_count.GetInt();

	BeginSimulateParticles();
	std::vector< CNewParticleEffect * > particlesToSimulate;
	BuildParticleSimList( particlesToSimulate );
	s_flThreadedPSystemTimeStep = flTimeDelta;

	int nCount = std::ssize( particlesToSimulate );

	// first, run non-reentrant part to get CP updates from entities
	for( int i=0; i<nCount; i++ )
	{
		// this one can call into random entity code which may not be thread-safe
		particlesToSimulate[i]->Update( s_flThreadedPSystemTimeStep );
		if ( nMaxParticleCount > 0 )
		{
			nParticleSystemCount += CountChildParticleSystems( particlesToSimulate[i] );
		}
	}

	// See if there are new particle systems that need early retirement
	// This has to happen after the first update
	if ( EarlyRetireParticleSystems( nCount, particlesToSimulate.data() ) )
	{
		particlesToSimulate.clear();
		BuildParticleSimList( particlesToSimulate );
		nCount = std::ssize( particlesToSimulate );
	}

	if ( nCount )
	{
		UpdateDirtySpatialPartitionEntities();
		if ( !r_threaded_particles.GetBool() )
		{
			for( int i=0; i<nCount; i++)
			{
				ProcessPSystem( particlesToSimulate[i] );
			}
		}
		else
		{
			int nAltCore = false;
			if ( !m_pThreadPool[1] || nAltCore == 0 )
			{
				ParallelProcess( particlesToSimulate.data(), nCount, ProcessPSystem, PreProcessPSystem, PostProcessPSystem );
			}
			else
			{
				if ( nAltCore > 2 )
				{
					nAltCore = 2;
				}
				CParallelProcessor<CNewParticleEffect*, CFuncJobItemProcessor<CNewParticleEffect*>, 3 > processor;
				processor.m_ItemProcessor.Init( ProcessPSystem, PreProcessPSystem, PostProcessPSystem );
				processor.Run( particlesToSimulate.data(), nCount, 1, std::numeric_limits<int>::max(), m_pThreadPool[nAltCore-1] );
			}
		}
	}

	// now, simulate the non-drawing ones
	ks::small_vector< CParticleCollection *, 128 > nonDrawingSimulateList;
	for( CNonDrawingParticleSystem *i = m_NonDrawingParticleSystems.head(); i; i = i->m_pNext )
	{
		nonDrawingSimulateList.push_back( i->m_pSystem );
	}
	if ( std::ssize( nonDrawingSimulateList ) )
	{
		ParallelProcess( nonDrawingSimulateList.data(), std::ssize( nonDrawingSimulateList ), ProcessNonDrawingSystem, PreProcessPSystem, PostProcessPSystem );
	}

	// now, run non-reentrant part for updating changes
	for( int i=0; i<nCount; i++)
	{
		// this one can call into random entity code which may not be thread-safe
		particlesToSimulate[i]->DetectChanges();
	}

	EndSimulateParticles();

	if ( nMaxParticleCount > 0 && ( nParticleSystemCount >= nMaxParticleCount ) )
	{
		SpewActiveParticleSystems();
	}
}

void CParticleMgr::UpdateAllEffects( float flTimeDelta )
{
	m_bUpdatingEffects = true;

	g_pParticleSystemQuery->PreSimulate();

	g_pParticleSystemMgr->SetFallbackParameters( cl_particle_fallback_base.GetFloat(), cl_particle_fallback_multiplier.GetFloat(), 
							cl_particle_sim_fallback_base_multiplier.GetFloat(), cl_particle_sim_fallback_threshold_ms.GetFloat() );
	g_pParticleSystemMgr->SetSystemLevel( GetCPULevel(), GetGPULevel() );

	if( flTimeDelta > 0.1f )
		flTimeDelta = 0.1f;

	for ( CParticleEffectBinding *pEffect : m_Effects )
	{
		// Don't update this effect if it will be removed.
		if( pEffect->GetRemoveFlag() )
			continue;

		// If this is a new effect, then update its bbox so it goes in the
		// right leaves (if it has particles).
		int bFirstUpdate = pEffect->GetNeedsBBoxUpdate();
		if ( bFirstUpdate )
		{
			// If the effect already disabled auto-updating of the bbox, then it should have
			// set the bbox by now and we can ignore this responsibility here.
			if ( !pEffect->GetAutoUpdateBBox() || pEffect->RecalculateBoundingBox() )
			{
				pEffect->SetNeedsBBoxUpdate( false );
			}
		}

		// This flag will get set to true if the effect is drawn through the leaf system.
		pEffect->SetDrawn( false );

		// Update the effect.
		pEffect->m_pSim->Update( flTimeDelta );

		if ( pEffect->GetFirstFrameFlag() )
			pEffect->SetFirstFrameFlag( false );
		else
			pEffect->SimulateParticles( flTimeDelta );

		// Update its position in the leaf system if its bbox changed.
		pEffect->DetectChanges();
	}

	if ( g_bMeasureParticlePerformance )					// use fixed time step
	{
		for( float dt=0.0f; dt <= flTimeDelta ; dt+= 0.01f )
		{
			UpdateNewEffects( 0.01f );
		}
	}
	else
	{
		UpdateNewEffects( flTimeDelta );
	}

	m_bUpdatingEffects = false;

	// Remove any effects that were flagged to be removed.
	for ( auto it = m_Effects.begin(); it != m_Effects.end(); )
	{
		CParticleEffectBinding *pEffect = *it++;

		if( pEffect->GetRemoveFlag() )
		{
			RemoveEffect( pEffect );
		}
	}

	// Remove any of the new effects that were flagged to be removed.
	for( CNewParticleEffect *pNewEffect=m_NewEffects.head(); pNewEffect;  )
	{
		CNewParticleEffect *pNextEffect = pNewEffect->m_pNext;
		if ( pNewEffect->GetRemoveFlag() )
		{
			RemoveEffect( pNewEffect );
		}

		pNewEffect = pNextEffect;
	}

	g_pParticleSystemQuery->PostSimulate();
}

void CParticleMgr::RemoveOldParticleEffects( float flTime )
{
	for( CNewParticleEffect *pNewEffect=m_NewEffects.head(); pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		if ( pNewEffect->m_flCurTime > flTime )
		{
			pNewEffect->StopEmission( false, true, true );
		}
	}
}

void CParticleMgr::SetRemoveAllParticleEffects()
{
	for( CNewParticleEffect *pNewEffect=m_NewEffects.head(); pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		pNewEffect->StopEmission( false, true, true );
		pNewEffect->SetRemoveFlag();
	}
}

CParticleSubTextureGroup* CParticleMgr::FindOrAddSubTextureGroup( IMaterial *pPageMaterial )
{
	for ( int i=0; i < std::ssize( m_SubTextureGroups ); i++ )
	{
		if ( m_SubTextureGroups[i]->m_pPageMaterial == pPageMaterial )
			return m_SubTextureGroups[i].get();
	}

	CParticleSubTextureGroup *pGroup = m_SubTextureGroups.emplace_back( std::make_unique< CParticleSubTextureGroup >() ).get();
	pGroup->m_pPageMaterial = pPageMaterial;
	pPageMaterial->AddRef();

	return pGroup;
}

PMaterialHandle CParticleMgr::GetPMaterial( const char *pMaterialName )
{
	if( !m_pMaterialSystem )
	{
		contract_assert( false );
		return nullptr;
	}

	auto hMat = m_SubTextures.find( pMaterialName );
	if ( hMat == m_SubTextures.end() )
	{
		IMaterial *pIMaterial = m_pMaterialSystem->FindMaterial( pMaterialName, TEXTURE_GROUP_PARTICLE );
		if ( pIMaterial )
		{
			pIMaterial->AddRef();

			CParticleSubTexture *pSubTexture = m_SubTextures.emplace( pMaterialName, std::make_unique< CParticleSubTexture >() ).first->second.get();
			pSubTexture->m_pMaterial = pIMaterial;

			// See if it's got a group name. If not, make a group with a special name.
			IMaterial *pPageMaterial = pIMaterial->GetMaterialPage();
			if ( pIMaterial->InMaterialPage() && pPageMaterial )
			{
				float flOffset[2], flScale[2];
				pIMaterial->GetMaterialOffset( flOffset );
				pIMaterial->GetMaterialScale( flScale );
				
				pSubTexture->m_tCoordMins[0] = (0*flScale[0] + flOffset[0]) * pPageMaterial->GetMappingWidth();
				pSubTexture->m_tCoordMaxs[0] = (1*flScale[0] + flOffset[0]) * pPageMaterial->GetMappingWidth();
				
				pSubTexture->m_tCoordMins[1] = (0*flScale[1] + flOffset[1]) * pPageMaterial->GetMappingHeight();
				pSubTexture->m_tCoordMaxs[1] = (1*flScale[1] + flOffset[1]) * pPageMaterial->GetMappingHeight();

				pSubTexture->m_pGroup = FindOrAddSubTextureGroup( pPageMaterial );
			}
			else
			{
				// Ok, this material isn't part of a group. Give it its own subtexture group.
				pSubTexture->m_pGroup = &pSubTexture->m_DefaultGroup;
				pSubTexture->m_DefaultGroup.m_pPageMaterial = pIMaterial;
				pPageMaterial = pIMaterial; // For tcoord scaling.
				
				pSubTexture->m_tCoordMins[0] = pSubTexture->m_tCoordMins[1] = 0;
				pSubTexture->m_tCoordMaxs[0] = pIMaterial->GetMappingWidth();
				pSubTexture->m_tCoordMaxs[1] = pIMaterial->GetMappingHeight();
			}

			// Rescale the texture coordinates.
			pSubTexture->m_tCoordMins[0] = (pSubTexture->m_tCoordMins[0] + 0.5f) / pPageMaterial->GetMappingWidth();
			pSubTexture->m_tCoordMins[1] = (pSubTexture->m_tCoordMins[1] + 0.5f) / pPageMaterial->GetMappingHeight();
			pSubTexture->m_tCoordMaxs[0] = (pSubTexture->m_tCoordMaxs[0] - 0.5f) / pPageMaterial->GetMappingWidth();
			pSubTexture->m_tCoordMaxs[1] = (pSubTexture->m_tCoordMaxs[1] - 0.5f) / pPageMaterial->GetMappingHeight();
		
			return pSubTexture;
		}
		else
		{
			return nullptr;
		} 
	}
	else
	{
		RepairPMaterial( hMat->second.get() ); //HACKHACK: Remove this when we can stop leaking handles from level to level.

		return hMat->second.get();
	}
}

// Re-associate a material handle with its material after level change.
// Materials keep their ref count across levels - we just need to re-acquire the pointer.
void CParticleMgr::RepairPMaterial( PMaterialHandle hMaterial )
{
	if( hMaterial->m_pMaterial != nullptr )
		return;

	const char *pMaterialName = nullptr;
	for( auto &[name, pSubTexture] : m_SubTextures )
	{
		if( pSubTexture.get() == hMaterial )
		{
			pMaterialName = name.c_str();
			break;
		}
	}
	contract_assert( pMaterialName != nullptr );

	IMaterial *pIMaterial = m_pMaterialSystem->FindMaterial( pMaterialName, TEXTURE_GROUP_PARTICLE );
	hMaterial->m_pMaterial = pIMaterial;
	if ( pIMaterial != nullptr )
	{
		// No AddRef needed - material keeps its ref from initial GetPMaterial() call.
		// Refs are only released once in Term() at shutdown.
		CMatRenderContextPtr pRenderContext( m_pMaterialSystem );
		pRenderContext->Bind( pIMaterial, this );

		IMaterial *pPageMaterial = pIMaterial->GetMaterialPage();
		if ( pIMaterial->InMaterialPage() && pPageMaterial )
		{
			if ( hMaterial->m_pGroup->m_pPageMaterial == nullptr )
			{
				hMaterial->m_pGroup->m_pPageMaterial = pPageMaterial;
				// No AddRef for page material either
			}
		}
		else
		{
			hMaterial->m_pGroup->m_pPageMaterial = pIMaterial;
		}
	}
}

IMaterial* CParticleMgr::PMaterialToIMaterial( PMaterialHandle hMaterial )
{
	if ( hMaterial )
	{
		RepairPMaterial( hMaterial ); //HACKHACK: Remove this when we can stop leaking handles from level to level.
		return hMaterial->m_pMaterial;
	}
	else
		return nullptr;
}

void CParticleMgr::GetDirectionalLightInfo( CParticleLightInfo &info ) const
{
	info = m_DirectionalLight;
}

void CParticleMgr::SetDirectionalLightInfo( const CParticleLightInfo &info )
{
	m_DirectionalLight = info;
}

//---------------------------------------------------------------------------
// non-drawing effects support
//---------------------------------------------------------------------------

CNonDrawingParticleSystem *CParticleMgr::CreateNonDrawingEffect( const char *pEffectName )
{
	CNonDrawingParticleSystem *pNew = new CNonDrawingParticleSystem;
	pNew->m_pSystem = g_pParticleSystemMgr->CreateParticleCollection( pEffectName );
	m_NonDrawingParticleSystems.push_front( pNew );
	return pNew;
}

CNonDrawingParticleSystem::~CNonDrawingParticleSystem( void )
{
	ParticleMgr()->m_NonDrawingParticleSystems.erase( this );
	delete m_pSystem;
}

void CParticleMgr::SpewInfo( bool bDetail )
{
	DevMsg( "Particle Effect Systems:\n" );
	int i = 0;
	for ( CParticleEffectBinding *pEffect : m_Effects )
	{
		const char *pEffectName;
		pEffectName = pEffect->m_pSim->GetEffectName();
		DevMsg( "%3d: NumActive: %3d, AutoBBox: %3s \"%s\" \n", i++, (int)pEffect->m_nActiveParticles, pEffect->GetAutoUpdateBBox() ? "on" : "off", pEffectName );
	}
}
static void cl_particles_dump_effects( const CCommand &args );
static ConCommand cl_particles_dump_effects_command( side_registry(), "cl_particles_dump_effects", cl_particles_dump_effects, "" );
static void cl_particles_dump_effects( const CCommand &args )
{
	ParticleMgr()->SpewInfo( true );
}

// ------------------------------------------------------------------------------------ //
// ------------------------------------------------------------------------------------ //
float Helper_GetTime()
{
	return gpGlobals->curtime;
}

float Helper_RandomFloat( float minVal, float maxVal )
{
	return random_valve->RandomFloat( minVal, maxVal );
}

int Helper_RandomInt( int minVal, int maxVal )
{
	return random_valve->RandomInt( minVal, maxVal );
}

float Helper_GetFrameTime()
{
	return gpGlobals->frametime;
}

int CParticleMgr::AllocateToolParticleEffectId()
{
	return m_nToolParticleEffectId++;
}

VMatrix& CParticleMgr::GetModelView()
{
	return m_mModelView;
}

const matrix3x4_t& CParticleEffectBinding::GetLocalSpaceTransform() const
{
	return m_LocalSpaceTransform.As3x4();
}

void StandardParticle_t::SetColor(float r, float g, float b)
{
	m_Color[0] = (unsigned char)(r * 255.9f);
	m_Color[1] = (unsigned char)(g * 255.9f);
	m_Color[2] = (unsigned char)(b * 255.9f);
}

void StandardParticle_t::SetAlpha(float a)
{
	m_Color[3] = (unsigned char)(a * 255.9f);
}

struct Particle;
class SimpleParticle;

using namespace ::ks::reflect;

// Singletons for each type of particle system.
// 0 = world, 1 = skybox
static ks::intrusive_ptr<CSimpleEmitter> g_pSimpleSingleton[2];
static ks::intrusive_ptr<CEmberEffect> g_pEmberSingleton[2];
static ks::intrusive_ptr<CFireSmokeEffect> g_pFireSmokeSingleton[2];
static ks::intrusive_ptr<CFireParticle> g_pFireSingleton[2];

class CEZParticleInit : public CAutoGameSystem
{
public:
	CEZParticleInit() : CAutoGameSystem( "CEZParticleInit" )
	{
	}

	template< class T >
	ks::intrusive_ptr<T> InitSingleton( ks::intrusive_ptr<T> pEmitter )
	{
		if ( !pEmitter )
		{
			Error( "InitSingleton: pEmitter is NULL" );
		}
	
		pEmitter->GetBinding().SetDrawThruLeafSystem( false );				// Draw in DrawSingletons instead.
		pEmitter->SetSortOrigin( Vector( 0, 0, 0 ) );

		// Since we draw manually in DrawSingletons, we don't care about
		// the bbox, so don't waste cycles inserting it into the leaf system
		// when it's not going to draw through that anyway.
		// (TODO: SetDrawThruLeafSystem(false) should trigger this automatically
		// in CParticleMgr).
		pEmitter->GetBinding().SetBBox( Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		return pEmitter;
	}

	virtual void LevelInitPreEntity()
	{
		g_pSimpleSingleton[0] = InitSingleton( CSimpleEmitter::Create( "Simple Particle Singleton" ) );
		g_pSimpleSingleton[1] = InitSingleton( CSimpleEmitter::Create( "Simple Particle Singleton [sky]" ) );
		
		g_pEmberSingleton[0] = InitSingleton( CEmberEffect::Create( "Ember Particle Singleton" ) );
		g_pEmberSingleton[1] = InitSingleton( CEmberEffect::Create( "Ember Particle Singleton [sky]" ) );
		
		g_pFireSmokeSingleton[0] = InitSingleton( CFireSmokeEffect::Create( "Fire Smoke Particle Singleton" ) );
		g_pFireSmokeSingleton[1] = InitSingleton( CFireSmokeEffect::Create( "Fire Smoke Particle Singleton [sky]" ) );
		
		g_pFireSingleton[0] = InitSingleton( CFireParticle::Create( "Fire Particle Singleton" ) );
		g_pFireSingleton[1] = InitSingleton( CFireParticle::Create( "Fire Particle Singleton [sky]" ) );
	}

	virtual void LevelShutdownPreEntity()
	{
		g_pSimpleSingleton[0] = g_pSimpleSingleton[1] = nullptr;
		g_pEmberSingleton[0] = g_pEmberSingleton[1] = nullptr;
		g_pFireSmokeSingleton[0] = g_pFireSmokeSingleton[1] = nullptr;
		g_pFireSingleton[0] = g_pFireSingleton[1] = nullptr;
	}
};

static CEZParticleInit g_EZParticleInit;

template<class T>
inline void CopyParticle( const T *pSrc, T *pDest )
{
	if ( pDest )
	{
		// Copy the particle, but don't screw up the linked list it's in.
		Particle *pPrev = pDest->m_pPrev;
		Particle *pNext = pDest->m_pNext;
		PMaterialHandle pSubTexture = pDest->m_pSubTexture;
		
		*pDest = *pSrc;
		
		pDest->m_pPrev = pPrev;
		pDest->m_pNext = pNext;
		pDest->m_pSubTexture = pSubTexture;
	}
}
		
void AddSimpleParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox )
{
	if ( g_pSimpleSingleton[bInSkybox] )
	{
		SimpleParticle *pNew = g_pSimpleSingleton[bInSkybox]->AddSimpleParticle( hMaterial, pParticle->m_Pos );
		CopyParticle( pParticle, pNew );
	}
}

void AddEmberParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox )
{
	if ( g_pEmberSingleton[bInSkybox] )
	{
		SimpleParticle *pNew = g_pEmberSingleton[bInSkybox]->AddSimpleParticle( hMaterial, pParticle->m_Pos );
		CopyParticle( pParticle, pNew );
	}
}

void AddFireSmokeParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox )
{
	if ( g_pFireSmokeSingleton[bInSkybox] )
	{
		SimpleParticle *pNew = g_pFireSmokeSingleton[bInSkybox]->AddSimpleParticle( hMaterial, pParticle->m_Pos );
		CopyParticle( pParticle, pNew );
	}
}

void AddFireParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox )
{
	if ( g_pFireSingleton[bInSkybox] )
	{
		SimpleParticle *pNew = g_pFireSingleton[bInSkybox]->AddSimpleParticle( hMaterial, pParticle->m_Pos );
		CopyParticle( pParticle, pNew );
	}
}

void DrawParticleSingletons( bool bInSkybox )
{
	RenderableInstance_t instance;
	instance.m_nAlpha = 255;

	if ( g_pSimpleSingleton[bInSkybox] )
	{
		g_pSimpleSingleton[bInSkybox]->GetBinding().DrawModel( STUDIO_RENDER, instance );
	}

	if ( g_pEmberSingleton[bInSkybox] )
	{
		g_pEmberSingleton[bInSkybox]->GetBinding().DrawModel( STUDIO_RENDER, instance );
	}

	if ( g_pFireSmokeSingleton[bInSkybox] )
	{
		g_pFireSmokeSingleton[bInSkybox]->GetBinding().DrawModel( STUDIO_RENDER, instance );
	}
	
	if ( g_pFireSingleton[bInSkybox] )
	{
		g_pFireSingleton[bInSkybox]->GetBinding().DrawModel( STUDIO_RENDER, instance );
	}
}

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class CBasePlayer;
class SimpleParticle;

using namespace ::ks::reflect;

// Purpose: Particles which are simulated locally to some space (attachment, bone, etc)

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CLocalSpaceEmitter::CLocalSpaceEmitter( const char *pDebugName ) :
	CSimpleEmitter( pDebugName )
{
}

const matrix3x4_t& CLocalSpaceEmitter::GetTransformMatrix() const
{
	return m_ParticleEffect.GetLocalSpaceTransform();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a local space emitter
//-----------------------------------------------------------------------------
ks::intrusive_ptr<CLocalSpaceEmitter> CLocalSpaceEmitter::Create( const char *pDebugName, 
	ClientEntityHandle_t hEntity, int nAttachment, int fFlags )
{
	CLocalSpaceEmitter *pRet = new CLocalSpaceEmitter( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	pRet->m_hEntity = hEntity;
	pRet->m_nAttachment = nAttachment;
	pRet->m_fFlags = fFlags;
	
	pRet->SetupTransformMatrix();

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: Used to build the transformation matrix for this frame
//-----------------------------------------------------------------------------
void CLocalSpaceEmitter::Update( float flTimeDelta )
{
	SetupTransformMatrix();
}

extern void FormatViewModelAttachment( CBasePlayer *pPlayer, Vector &vOrigin, bool bInverse );

void CLocalSpaceEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		// Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		// If we're dead, we're done
		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
		{
			pIterator->RemoveParticle( pParticle );
		}
	
		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}

void CLocalSpaceEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	const matrix3x4_t &mLocalToWorld = GetTransformMatrix();
	const VMatrix &mModelView = ParticleMgr()->GetModelView();

	const SimpleParticle *pParticle = (const SimpleParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		// Transform it
		Vector screenPos, worldPos;
		VectorTransform( pParticle->m_Pos, mLocalToWorld, worldPos );
		
		// Correct viewmodel squashing
		if ( m_fFlags & FLE_VIEWMODEL )
		{
			FormatViewModelAttachment( nullptr, worldPos, false );
		}

		TransformParticle( mModelView, worldPos, screenPos );
		
		float sortKey = (int) screenPos.z;

		// Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			screenPos,
			UpdateColor( pParticle ),
			UpdateAlpha( pParticle ) * GetAlphaDistanceFade( screenPos, m_flNearClipMin, m_flNearClipMax ),
			UpdateScale( pParticle ),
			pParticle->m_flRoll 
			);

		pParticle = (const SimpleParticle *)pIterator->GetNext( sortKey );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create the matrix by which we'll transform the particle's local 
//			space into world space, via the attachment's transform
//-----------------------------------------------------------------------------
void CLocalSpaceEmitter::SetupTransformMatrix( void )
{
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( m_hEntity );
	if ( pRenderable )
	{
		matrix3x4_t mat;
		if ( pRenderable->GetAttachment( m_nAttachment, mat ) == false )
		{
			// This attachment is bogus!
			contract_assert( 0 );
		}
	
		// Tell the particle effect so it knows
		Vector origin;
		MatrixGetColumn( mat, 3, origin );
		m_ParticleEffect.SetLocalSpaceTransform( mat );
		SetSortOrigin( origin );

		CBaseEntity *pEnt = EntityCast<CBaseEntity>( pRenderable->GetIClientUnknown()->GetBaseEntity() );
		if ( pEnt )
		{
			Vector vWorldMins, vWorldMaxs;
			float scale = pEnt->CollisionProp()->BoundingRadius();
			vWorldMins[0] = origin[0] - scale;
			vWorldMins[1] = origin[1] - scale;
			vWorldMins[2] = origin[2] - scale;
			vWorldMaxs[0] = origin[0] + scale;
			vWorldMaxs[1] = origin[1] + scale;
			vWorldMaxs[2] = origin[2] + scale;
			GetBinding().SetBBox( vWorldMins, vWorldMaxs, true );
		}
	}

	// We preapply the local transform because we need to squash it for viewmodel FOV.
	m_ParticleEffect.SetAutoApplyLocalTransform( false );
}

class CNewParticleEffect;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

extern ConVar cl_particleeffect_aabb_buffer;
extern ConVar cl_particles_show_bbox;
ConVar cl_particles_show_controlpoints( side_registry(), "cl_particles_show_controlpoints", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, CParticleSystemDefinition *pEffect )
{
	m_hOwner = pOwner;
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pEffect->GetName() ) )
		{
			pEffect = g_pParticleSystemMgr->FindParticleSystem( "error" );
		}
	}
	Init( pEffect );
	Construct();
}

CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, const char* pEffectName )
{
	m_hOwner = pOwner;
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pEffectName ) )
		{
			pEffectName = "error";
		}
	}
	Init( pEffectName );
	Construct();
}

CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, int nPrecacheIndex )
{
	m_hOwner = pOwner;
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pDef->GetName() ) )
		{
			// This is the error effect
			pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
		}
	} 

	Init( pDef );
	Construct();
}

static ConVar cl_aggregate_particles( side_registry(), "cl_aggregate_particles", "1" );

CNewParticleEffect *CNewParticleEffect::CreateOrAggregate( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
																	 Vector const &vecAggregatePosition, const char *pDebugName,
																	 int nSplitScreenSlot )
{
	if (!pDef) { return nullptr; }

	CNewParticleEffect *pAggregateTarget = nullptr;
	// see if we should aggregate
	bool bCanAggregate = ( pOwner == nullptr ) && ( pDef->m_flAggregateRadius > 0.0 ) && ( cl_aggregate_particles.GetInt() != 0 );
	if ( bCanAggregate )
	{
		CParticleSystemDefinition *pDefFallback = pDef;
		do
		{
			float flAggregateDistSqr =  ( pDefFallback->m_flAggregateRadius * pDefFallback->m_flAggregateRadius ) + 0.1;
			for( CParticleCollection *pSystem = pDefFallback->FirstCollection(); pSystem; pSystem = pSystem->GetNextCollectionUsingSameDef() )
			{
				CNewParticleEffect *pEffectCheck = static_cast<CNewParticleEffect *>( pSystem );
				if ( ! pEffectCheck->m_bDisableAggregation )
				{
					float flDistSQ = vecAggregatePosition.DistToSqr( pEffectCheck->m_vecAggregationCenter );
					if ( ( flDistSQ < flAggregateDistSqr ) && 
						 ( pSystem->m_nMaxAllowedParticles - pSystem->m_nActiveParticles > pDefFallback->m_nAggregationMinAvailableParticles ) &&
						 ( pEffectCheck->m_nSplitScreenUser == nSplitScreenSlot ) )
					{
						flAggregateDistSqr = flDistSQ;
						pAggregateTarget = pEffectCheck;
					}
				}
			}
			pDefFallback = pDefFallback->GetFallbackReplacementDefinition();
		} while ( pDefFallback );
	}
	if ( ! pAggregateTarget )
	{
		// we need a new one
		pAggregateTarget = new CNewParticleEffect( pOwner, pDef );
		pAggregateTarget->SetDrawOnlyForSplitScreenUser( nSplitScreenSlot );
		pAggregateTarget->SetDynamicallyAllocated( true );
	}
	else
	{
		// just reset the old one
		pAggregateTarget->Restart( RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN );
	}
	if ( bCanAggregate )
	{
		pAggregateTarget->m_vecAggregationCenter = vecAggregatePosition;
	}
	pAggregateTarget->m_pDebugName = pDebugName;
	pAggregateTarget->m_bDisableAggregation = false;
	return pAggregateTarget;

}

void CNewParticleEffect::RemoveParticleEffect( int nPrecacheIndex )
{
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );

	if ( pDef == nullptr )
		return;

	for( CParticleCollection *pSystem = pDef->FirstCollection(); pSystem; pSystem = pSystem->GetNextCollectionUsingSameDef() )
	{
		CNewParticleEffect *pEffectCheck = static_cast<CNewParticleEffect *>( pSystem );

		if ( pEffectCheck )
		{
			pEffectCheck->SetRemoveFlag();
		}
	}
}

CNewParticleEffect *CNewParticleEffect::CreateOrAggregate( 
	CBaseEntity *pOwner, const char *pParticleSystemName,
	Vector const &vecAggregatePosition, 
	const char *pDebugName,
	int nSplitScreenUser )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", pParticleSystemName );
		pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
	}
	return CreateOrAggregate( pOwner, pDef, vecAggregatePosition, pDebugName, nSplitScreenUser );
}

CNewParticleEffect *CNewParticleEffect::CreateOrAggregatePrecached( 
	CBaseEntity *pOwner, int nPrecacheIndex,
	Vector const &vecAggregatePosition, const char *pDebugName,
	int nSplitScreenUser )
{
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", GetParticleSystemNameFromIndex( nPrecacheIndex ) );
		pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
	}
	return CreateOrAggregate( pOwner, pDef, vecAggregatePosition, pDebugName, nSplitScreenUser );
}

void CNewParticleEffect::Construct()
{
	m_vSortOrigin.Init();

	m_bDontRemove = false;
	m_bRemove = false;
	m_bDrawn = false;
	m_bNeedsBBoxUpdate = false;
	m_bIsFirstFrame = true;
	m_bAutoUpdateBBox = false;
	m_bAllocated = true;
	m_bSimulate = true;
	m_bRecord = false;
	m_bShouldPerformCullCheck = false;
	m_bDisableAggregation = true;							// will be reset when someone creates it via CreateOrAggregate

	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
	m_RefCount = 0;
	ParticleMgr()->AddEffect( this );
	m_LastMax = Vector( -1.0e6, -1.0e6, -1.0e6 );
	m_LastMin = Vector( 1.0e6, 1.0e6, 1.0e6 );
	m_MinBounds = Vector( 1.0e6, 1.0e6, 1.0e6 );
	m_MaxBounds = Vector( -1.0e6, -1.0e6, -1.0e6 );
	m_pDebugName = nullptr;
	m_nSplitScreenUser = -1;

	SetRenderable( this );

	RecordCreation();
}

CNewParticleEffect::~CNewParticleEffect(void)
{

	m_bAllocated = false;
	if ( m_hOwner )
	{
		// NOTE: This can provoke another NotifyRemove call which is why flags is set to 0
		m_hOwner->ParticleProp()->OnParticleSystemDeleted( this );
	}
}

//-----------------------------------------------------------------------------
// Refcounting
//-----------------------------------------------------------------------------
void CNewParticleEffect::AddRef()
{
	++m_RefCount;
}

void CNewParticleEffect::Release()
{
	contract_assert( m_RefCount > 0 );
	--m_RefCount;

	// If all the particles are already gone, delete ourselves now.
	// If there are still particles, wait for the last NotifyDestroyParticle.
	if ( m_RefCount == 0 )
	{
		if ( m_bAllocated )
		{
			if ( IsFinished() )
			{
				SetRemoveFlag();
			}
		}
	}
}

void CNewParticleEffect::NotifyRemove()
{
	if ( m_bAllocated )
	{
		delete this;
	}
}

int CNewParticleEffect::IsReleased()
{
	return m_RefCount == 0;
}

void CNewParticleEffect::SetOwner( CBaseEntity *pOwner ) 
{ 
	if ( m_hOwner.Get()	!= pOwner )
	{
		m_hOwner = pOwner; 
		ClientLeafSystem()->RenderInFastReflections( RenderHandle(), pOwner ? pOwner->IsRenderingInFastReflections() : false );
	}
}

//-----------------------------------------------------------------------------
// Refraction and soft particle support
//-----------------------------------------------------------------------------
int CNewParticleEffect::GetRenderFlags( void )
{
	int nRet = 0;
    // NOTE: Need to do this because CParticleCollection's version is non-virtual
	if ( CParticleCollection::UsesPowerOfTwoFrameBufferTexture( true ) )
		nRet |= ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB | ERENDERFLAGS_REFRACT_ONLY_ONCE_PER_FRAME;
	if ( CParticleCollection::UsesFullFrameBufferTexture( true ) )
		nRet |= ERENDERFLAGS_NEEDS_FULL_FB;
	return nRet;
}

void CNewParticleEffect::StopEmission( bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop, bool bPlayEndCap )
{

	CParticleCollection::StopEmission( bInfiniteOnly, bRemoveAllParticles, bWakeOnStop, bPlayEndCap );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewParticleEffect::SetDormant( bool bDormant )
{
	CParticleCollection::SetDormant( bDormant );
}

void CNewParticleEffect::SetControlPointEntity( int nWhichPoint, CBaseEntity *pEntity )
{

	if ( pEntity )
	{
		CParticleCollection::SetControlPointObject( nWhichPoint, &m_hControlPointOwners[ nWhichPoint ] );
		m_hControlPointOwners[ nWhichPoint ] = pEntity;
	}
	else
		CParticleCollection::SetControlPointObject( nWhichPoint, nullptr );
}

void CNewParticleEffect::SetControlPoint( int nWhichPoint, const Vector &v )
{

	CParticleCollection::SetControlPoint( nWhichPoint, v );
}

void CNewParticleEffect::SetToolRecording( bool bRecord )
{
	if ( bRecord == m_bRecord )
		return;

	m_bRecord = bRecord;

	if ( m_bRecord )
	{
		RecordCreation();
	}
}

void CNewParticleEffect::RecordCreation()
{
}

void CNewParticleEffect::RecordControlPointOrientation( int nWhichPoint )
{
}

void CNewParticleEffect::SetControlPointOrientation( int nWhichPoint, 
	const Vector &forward, const Vector &right, const Vector &up )
{
	CParticleCollection::SetControlPointOrientation( nWhichPoint, forward, right, up );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointOrientation( int nWhichPoint, const Quaternion &q )
{
	CParticleCollection::SetControlPointOrientation( nWhichPoint, q );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointForwardVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointForwardVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointUpVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointUpVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointRightVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointRightVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}

//-----------------------------------------------------------------------------
// Called when the particle effect is about to update
//-----------------------------------------------------------------------------
void CNewParticleEffect::Update( float flTimeDelta )
{
	if ( m_hOwner )
	{
		m_hOwner->ParticleProp()->OnParticleSystemUpdated( this, flTimeDelta );
	}
}

//-----------------------------------------------------------------------------
// Bounding box
//-----------------------------------------------------------------------------
CNewParticleEffect* CNewParticleEffect::ReplaceWith( const char *pParticleSystemName )
{
	StopEmission( false, true, true );
	if ( !pParticleSystemName || !pParticleSystemName[0] )
		return nullptr;

	CNewParticleEffect *pNewEffect = CNewParticleEffect::Create( GetOwner(), pParticleSystemName, pParticleSystemName );
	if ( !pNewEffect )
		return nullptr;

	// Copy over the control point data
	for ( int i = 0; i < MAX_PARTICLE_CONTROL_POINTS; ++i )
	{
		if ( !ReadsControlPoint( i ) )
			continue;

		Vector vecForward, vecRight, vecUp;
		pNewEffect->SetControlPoint( i, GetControlPointAtCurrentTime( i ) );
		GetControlPointOrientationAtCurrentTime( i, &vecForward, &vecRight, &vecUp );
		pNewEffect->SetControlPointOrientation( i, vecForward, vecRight, vecUp );
		pNewEffect->SetControlPointParent( i, GetControlPointParent( i ) );
	}

	if ( m_hOwner )
	{
		m_hOwner->ParticleProp()->ReplaceParticleEffect( this, pNewEffect );
	}

	// fixup any other references to the old system, to point to the new system
	while( m_References.head() )
	{
		// this will remove the reference from m_References
		m_References.head()->Set( pNewEffect );
	}

	// At this point any references should have been redirected,
	// but we may still be running with some stray particles, so we
	// might not be flagged for removal - force the issue!
	contract_assert( m_RefCount == 0 );
	SetRemoveFlag();

	return pNewEffect;
}

//-----------------------------------------------------------------------------
// Bounding box
//-----------------------------------------------------------------------------
void CNewParticleEffect::SetParticleCullRadius( float radius )
{
}

bool CNewParticleEffect::RecalculateBoundingBox()
{
	BloatBoundsUsingControlPoint();
	if ( !m_bBoundsValid )
	{
		m_MaxBounds = m_MinBounds = GetRenderOrigin();
		return false;
	}

	return true;
}

void CNewParticleEffect::GetRenderBounds( Vector& mins, Vector& maxs )
{
	if ( !m_bBoundsValid )
	{
		mins = vec3_origin;
		maxs = mins;
		return;
	}
	VectorSubtract( m_MinBounds, GetRenderOrigin(), mins );
	VectorSubtract( m_MaxBounds, GetRenderOrigin(), maxs );
}

void CNewParticleEffect::DetectChanges()
{
	// if we have no render handle, return
	if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	// Turn off rendering if the bounds aren't valid
	g_pClientLeafSystem->EnableRendering( m_hRenderHandle, m_bBoundsValid );

	if ( !m_bBoundsValid )
	{
		m_LastMin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		m_LastMin.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
		return;
	}

	if ( m_MinBounds != m_LastMin || m_MaxBounds != m_LastMax ) 
	{
		// call leafsystem to update this guy
		ClientLeafSystem()->RenderableChanged( m_hRenderHandle );

		// remember last parameters
		m_LastMin = m_MinBounds;
		m_LastMax = m_MaxBounds;
	}
}

extern ConVar r_DrawParticles;

//-----------------------------------------------------------------------------
// Rendering
//-----------------------------------------------------------------------------
int CNewParticleEffect::DrawModel( int flags, const RenderableInstance_t &instance )
{
	CVProfScope _( "CNewParticleEffect::DrawModel", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	if ( r_DrawParticles.GetBool() == false )
		return 0;

	if ( !GetClientMode()->ShouldDrawParticles() || !ParticleMgr()->ShouldRenderParticleSystems() )
		return 0;

	if ( flags & ( STUDIO_SHADOWDEPTHTEXTURE | STUDIO_SSAODEPTHTEXTURE ) )
		return 0;

	if ( m_hOwner && m_hOwner->IsDormant() )
		return 0;

	int nViewRecursionLevel = 0;

	// do distance cull check here. We do it here instead of in particles so we can easily only do
	// it for root objects, not bothering to cull children individually
	CMatRenderContextPtr pRenderContext( materials );
	if ( !m_pDef->IsScreenSpaceEffect() && !m_pDef->IsViewModelEffect() )
	{
		Vector vecCamera;
		pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
		if ( ( CalcSqrDistanceToAABB( m_MinBounds, m_MaxBounds, vecCamera ) > ( m_pDef->m_flMaxDrawDistance * m_pDef->m_flMaxDrawDistance ) ) )
			return 0;
	}

	if ( m_pDef->IsViewModelEffect() )
	{
		CBasePlayer* pPlayer = CBasePlayer::GetLocalPlayer();
		if ( !pPlayer || !pPlayer->IsAlive() )
			return 0;

		CBaseAnimating *pRenderedWeaponModel = pPlayer->GetRenderedWeaponModel();
		if ( !pRenderedWeaponModel || !pRenderedWeaponModel->IsViewModel() )
			return 0;
	}

	if ( ( flags & STUDIO_TRANSPARENCY ) || !IsBatchable() || !m_pDef->IsDrawnThroughLeafSystem() )
	{
		CBaseEntity *pCameraObject = GetSplitScreenViewPlayer();
		if( ((CBasePlayer *)pCameraObject)->GetViewEntity() )
		{
			pCameraObject = ((CBasePlayer *)pCameraObject)->GetViewEntity();
		}

		CBaseEntity *pSkipRenderObject = ( m_pDef->m_nSkipRenderControlPoint != -1 ) ? GetControlPointEntity( m_pDef->m_nSkipRenderControlPoint ).Get() : nullptr;
		CBaseEntity *pAllowRenderObject = ( m_pDef->m_nAllowRenderControlPoint != -1 ) ? GetControlPointEntity( m_pDef->m_nAllowRenderControlPoint ).Get() : nullptr;
		if ( pSkipRenderObject && ( pSkipRenderObject->IsBaseCombatWeapon() || ( pSkipRenderObject->GetBaseAnimating() && pSkipRenderObject->GetBaseAnimating()->IsViewModel() ) ) )
			pSkipRenderObject = pSkipRenderObject->GetOwnerEntity();
		if ( pAllowRenderObject && ( pAllowRenderObject->IsBaseCombatWeapon() || ( pAllowRenderObject->GetBaseAnimating() && pAllowRenderObject->GetBaseAnimating()->IsViewModel() ) ) )
			pAllowRenderObject = pAllowRenderObject->GetOwnerEntity();

		// apply logic that lets you skip rendering a system if the camera is attached to its entity
		if ( pCameraObject )
		{
			if ( pCameraObject == pSkipRenderObject )
			{
				{
					if ( nViewRecursionLevel == 0 )
						return 0;
				}
			}

			if ( pAllowRenderObject && (pCameraObject != pAllowRenderObject) )
				return 0;
		}

		Vector4D vecDiffuseModulation( 1.0f, 1.0f, 1.0f, 1.0f ); //instance.m_nAlpha / 255.0f );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		Render( nViewRecursionLevel, pRenderContext, vecDiffuseModulation, ( flags & STUDIO_TRANSPARENCY ) ? IsTwoPass() : false, pCameraObject );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
	}
	else
	{
		g_pParticleSystemMgr->AddToRenderCache( this );
	}

	if ( cl_particles_show_bbox.GetBool() )
	{
		Vector center = GetRenderOrigin();
		Vector mins   = m_MinBounds - center;
		Vector maxs   = m_MaxBounds - center;
	
		int r, g;
		if ( GetAutoUpdateBBox() )
		{
			// red is bad, the bbox update is costly
			r = 255;
			g = 0;
		}
		else
		{
			// green, this effect presents less cpu load 
			r = 0;
			g = 255;
		}
		Vector vecCurCPPos = GetControlPointAtCurrentTime( 0 );
		debugoverlay->AddBoxOverlay( center, mins, maxs, QAngle( 0, 0, 0 ), r, g, 0, 16, 0 );
		debugoverlay->AddTextOverlayRGB( center, 0, 0, r, g, 0, 64, "%s:(%d)", GetEffectName(),
										 m_nActiveParticles );
	}
	if ( cl_particles_show_controlpoints.GetBool() )
	{
		for ( int i = 0; i < MAX_PARTICLE_CONTROL_POINTS; ++i )
		{
			if ( ReadsControlPoint( i ) )
			{
				Vector vecPos = GetControlPointAtCurrentTime( i );
				Vector vecForward, vecRight, vecUp;
				GetControlPointOrientationAtCurrentTime( i, &vecForward, &vecRight, &vecUp );
				NDebugOverlay::Line( vecPos, vecPos + ( vecForward * 4.0f ), 255, 0, 0, true, 0.05f );
				NDebugOverlay::Line( vecPos, vecPos + ( vecRight * 4.0f ), 0, 255, 0, true, 0.05f );
				NDebugOverlay::Line( vecPos, vecPos + ( vecUp * 4.0f ), 0, 0, 255, true, 0.05f );
			}
		}
	}

	return 1;
}

void CNewParticleEffect::SetDrawOnlyForSplitScreenUser( int nSlot )
{
	if ( nSlot != m_nSplitScreenUser )
	{
		m_nSplitScreenUser = nSlot;
		if ( IsSplitScreenSupported() && ( m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE ) )
		{
			g_pClientLeafSystem->EnableSplitscreenRendering( m_hRenderHandle, ComputeSplitscreenRenderingFlags( this ) );
		}
	}
}

bool CNewParticleEffect::ShouldDrawForSplitScreenUser( int nSlot )
{
	if ( m_nSplitScreenUser == -1 )
		return true;
	return m_nSplitScreenUser == nSlot;
}

bool CNewParticleEffect::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	matrix3x4a_t mat;
	mat.Init( Vector( 0, -1, 0), Vector( 1, 0, 0), Vector( 0, 0, 1 ), vec3_origin );
	MatrixMultiply( m_DrawModelMatrix, mat, pBoneToWorldOut[0] );
	return true;
}

static void DumpParticleStats_f( void )
{
	g_pParticleSystemMgr->DumpProfileInformation();
}

static ConCommand cl_dump_particle_stats( side_registry(), "cl_dump_particle_stats", DumpParticleStats_f, "dump particle profiling info to particle_profile.csv") ;

static void cl_particles_dumplist( const CCommand &args );
static ConCommand cl_particles_dumplist_command( side_registry(), "cl_particles_dumplist", cl_particles_dumplist, "Dump all new particles, optional name substring." );
static void cl_particles_dumplist( const CCommand &args )
{
	if ( args.ArgC() == 2 )
	{
		g_pParticleSystemMgr->DumpParticleList( args.Arg(1) );
	}
	else
	{
		g_pParticleSystemMgr->DumpParticleList( nullptr );
	}
}

int CNewParticleEffect::GetToolParticleEffectId() const
{
	return m_nToolParticleEffectId;
}

int CNewParticleEffect::AllocateToolParticleEffectId()
{
	m_nToolParticleEffectId = ParticleMgr()->AllocateToolParticleEffectId();
	return m_nToolParticleEffectId;
}

// Call this before adding a bunch of particles to give it a rough estimate of where
// your particles are for sorting amongst other translucent entities.
void CNewParticleEffect::SetSortOrigin( const Vector &vSortOrigin )
{
	m_vSortOrigin = vSortOrigin;
}

const Vector &CNewParticleEffect::GetSortOrigin( void )
{
	return m_vSortOrigin;
}

bool CNewParticleEffect::ShouldDraw( void )
{
	return true;
}

const QAngle& CNewParticleEffect::GetRenderAngles( void )
{
	return vec3_angle;
}

const matrix3x4_t &	CNewParticleEffect::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix( mat );
	PositionMatrix( GetRenderOrigin(), mat );
	return mat;
}

Vector const &CNewParticleEffect::GetRenderOrigin( void )
{
	return m_vSortOrigin;
}

PMaterialHandle CNewParticleEffect::GetPMaterial(const char *name)
{
	//!!
	contract_assert( 0 );
	return nullptr;
}

Particle* CNewParticleEffect::AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin )
{
	//!!
	contract_assert( 0 );
	return nullptr;
}

const char *CNewParticleEffect::GetEffectName()
{
	return GetName();
}

void CNewParticleEffect::SetDontRemove( bool bSet )
{
	m_bDontRemove = bSet;
}

void CNewParticleEffect::SetDrawn( bool bDrawn )
{
	m_bDrawn = bDrawn;
}

void CNewParticleEffect::SetFirstFrameFlag( bool bFirst )
{
	m_bIsFirstFrame = bFirst;
}

void CNewParticleEffect::SetDynamicallyAllocated( bool bDynamic )
{
	m_bAllocated = bDynamic;
}

void CNewParticleEffect::SetNeedsBBoxUpdate( bool bNeedsUpdate )
{
	m_bNeedsBBoxUpdate = bNeedsUpdate;
}

void CNewParticleEffect::SetAutoUpdateBBox( bool bNeedsUpdate )
{
	m_bAutoUpdateBBox = bNeedsUpdate;
}

void CNewParticleEffect::SetRemoveFlag( void )
{
	m_bRemove = true;
}

bool CNewParticleEffect::GetRemoveFlag( void )
{
	return m_bRemove;
}

bool CNewParticleEffect::GetFirstFrameFlag( void )
{
	return m_bIsFirstFrame;
}

bool CNewParticleEffect::GetNeedsBBoxUpdate( void )
{
	return m_bNeedsBBoxUpdate;
}

bool CNewParticleEffect::GetAutoUpdateBBox( void )
{
	return m_bAutoUpdateBBox;
}

bool CNewParticleEffect::ShouldPerformCullCheck() const
{
	return m_bShouldPerformCullCheck;
}

void CNewParticleEffect::MarkShouldPerformCullCheck( bool bEnable )
{
	m_bShouldPerformCullCheck = bEnable;
}

CNewParticleEffect *CNewParticleEffect::Create( CBaseEntity *pOwner, const char *pParticleSystemName, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, pParticleSystemName );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

CNewParticleEffect *CNewParticleEffect::Create( CBaseEntity *pOwner, CParticleSystemDefinition *pDef, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, pDef );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

CNewParticleEffect *CNewParticleEffect::CreatePrecached( CBaseEntity *pOwner, int nPrecacheIndex, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, nPrecacheIndex );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

class CParticleRenderIterator;
class CParticleSimulateIterator;
struct Particle;
class SimpleParticle;

using namespace ::ks::reflect;

// Used for debugging to make sure all particle effects get freed when we exit.
std::vector<CParticleEffect*> g_ParticleEffects;
class CEffectChecker
{
public:
	~CEffectChecker()
	{
		contract_assert( g_ParticleEffects.empty() );
	}
} g_EffectChecker;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CParticleEffect::CParticleEffect( const char *pName )
{
	m_pDebugName = pName;
	m_vSortOrigin.Init();
	m_Flags = FLAG_ALLOCATED;
	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
	m_RefCount = 0;
	m_bSimulate = true;
	ParticleMgr()->AddEffect( &m_ParticleEffect, this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CParticleEffect::~CParticleEffect( void )
{
	// HACKHACK: Prevent re-entering the destructor, clear m_Flags.
	// For some reason we'll get a callback into NotifyRemove() after being deleted!
	// Investigate dangling pointer
	m_Flags = 0;

}

void CParticleEffect::SetDynamicallyAllocated( bool bDynamic )
{
	if( bDynamic )
		m_Flags |= FLAG_ALLOCATED;
	else
		m_Flags &= ~FLAG_ALLOCATED;
}

int CParticleEffect::IsReleased()
{
	return m_RefCount == 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleEffect::AddRef()
{
	++m_RefCount;
}

void CParticleEffect::Release()
{
	contract_assert( m_RefCount > 0 );
	--m_RefCount;

	// If all the particles are already gone, delete ourselves now.
	// If there are still particles, wait for the last NotifyDestroyParticle.
	if ( m_RefCount == 0 )
	{
		if ( m_Flags & FLAG_ALLOCATED )
		{
			if ( m_ParticleEffect.GetNumActiveParticles() == 0 )
			{
				m_ParticleEffect.SetRemoveFlag();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vSortOrigin - 
//-----------------------------------------------------------------------------
const Vector &CParticleEffect::GetSortOrigin()
{
	contract_assert( m_vSortOrigin.IsValid() );
	return m_vSortOrigin;
}

const char *CParticleEffect::GetEffectName()
{
	return m_pDebugName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParticle - 
//-----------------------------------------------------------------------------
void CParticleEffect::NotifyDestroyParticle( Particle* pParticle )
{
	// Go away if we're released and there are no more particles.
	if( m_ParticleEffect.GetNumActiveParticles() == 0 && IsReleased() && (m_Flags & FLAG_ALLOCATED) && !(m_Flags & FLAG_DONT_REMOVE) )
	{
		m_ParticleEffect.SetRemoveFlag();
	}
}

void CParticleEffect::Update( float flTimeDelta )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleEffect::NotifyRemove()
{
	if( m_Flags & FLAG_ALLOCATED )
	{
		delete this;
	}
}

void CParticleEffect::SetSortOrigin( const Vector &vSortOrigin )
{
	if ( GetBinding().GetAutoUpdateBBox() )
	{
		if ( m_ParticleEffect.EnlargeBBoxToContain( vSortOrigin ) )
		{
			m_vSortOrigin = vSortOrigin;
		}
	}
	else
	{
		// If not auto-updating bbox, don't change the bbox, just set the sort origin.
		m_vSortOrigin = vSortOrigin;
	}
}

void CParticleEffect::SetParticleCullRadius( float radius )
{
	m_ParticleEffect.SetParticleCullRadius( radius );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : PMaterialHandle
//-----------------------------------------------------------------------------
PMaterialHandle CParticleEffect::GetPMaterial(const char *name)
{
	return m_ParticleEffect.FindOrAddMaterial(name);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : particleSize - 
//			material - 
// Output : SimpleParticle
//-----------------------------------------------------------------------------
Particle *CParticleEffect::AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin )
{
	// If you get here, then you must call SetSortOrigin before adding particles.
	contract_assert( m_vSortOrigin.IsValid() );

	Particle *pParticle = (Particle *) m_ParticleEffect.AddParticle( particleSize, material );

	if( pParticle == nullptr )
		return nullptr;

	pParticle->m_Pos = origin;
	return pParticle;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

REGISTER_EFFECT_USING_CREATE( CSimpleEmitter );

CSimpleEmitter::CSimpleEmitter( const char *pDebugName ) : CParticleEffect( pDebugName )
{
	m_flNearClipMin	= 16.0f;
	m_flNearClipMax	= 64.0f;
	m_nSplitScreenPlayerSlot = -1;
}

CSimpleEmitter::~CSimpleEmitter()
{
}

ks::intrusive_ptr<CSimpleEmitter> CSimpleEmitter::Create( const char *pDebugName )
{
	CSimpleEmitter *pRet = new CSimpleEmitter( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: Set the internal near clip range for this particle system
// Input  : nearClipMin - beginning of clip range
//			nearClipMax - end of clip range
//-----------------------------------------------------------------------------
void CSimpleEmitter::SetNearClip( float nearClipMin, float nearClipMax )
{ 
	m_flNearClipMin = nearClipMin;
	m_flNearClipMax = nearClipMax;
}

SimpleParticle*	CSimpleEmitter::AddSimpleParticle( 
	PMaterialHandle hMaterial, 
	const Vector &vOrigin,
	float flDieTime,
	unsigned char uchSize )
{
	SimpleParticle *pRet = (SimpleParticle*)AddParticle( sizeof( SimpleParticle ), hMaterial, vOrigin );
	if ( pRet )
	{
		pRet->m_Pos = vOrigin;
		pRet->m_vecVelocity.Init();
		pRet->m_flRoll = 0;
		pRet->m_flRollDelta = 0;
		pRet->m_flLifetime = 0;
		pRet->m_flDieTime = flDieTime;
		pRet->m_uchColor[0] = pRet->m_uchColor[1] = pRet->m_uchColor[2] = 0;
		pRet->m_uchStartAlpha = pRet->m_uchEndAlpha = 255;
		pRet->m_uchStartSize = pRet->m_uchEndSize = uchSize;
		pRet->m_iFlags = 0;
	}

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateAlpha( const SimpleParticle *pParticle )
{
	return (pParticle->m_uchStartAlpha/255.0f) + ( (float)(pParticle->m_uchEndAlpha/255.0f) - (float)(pParticle->m_uchStartAlpha/255.0f) ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateScale( const SimpleParticle *pParticle )
{
	return	(float)pParticle->m_uchStartSize + ( (float)pParticle->m_uchEndSize - (float)pParticle->m_uchStartSize ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------

static constexpr auto WIND_ACCEL = 50;

void CSimpleEmitter::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	if (pParticle->m_iFlags & SIMPLE_PARTICLE_FLAG_WINDBLOWN)
	{
		Vector vecWind;
		GetWindspeedAtTime( gpGlobals->curtime, vecWind );

		for ( int i = 0 ; i < 2 ; i++ )
		{
			if ( pParticle->m_vecVelocity[i] < vecWind[i] )
			{
				pParticle->m_vecVelocity[i] += ( timeDelta * WIND_ACCEL );

				// clamp
				if ( pParticle->m_vecVelocity[i] > vecWind[i] )
					pParticle->m_vecVelocity[i] = vecWind[i];
			}
			else if (pParticle->m_vecVelocity[i] > vecWind[i] )
			{
				pParticle->m_vecVelocity[i] -= ( timeDelta * WIND_ACCEL );

				// clamp.
				if ( pParticle->m_vecVelocity[i] < vecWind[i] )
					pParticle->m_vecVelocity[i] = vecWind[i];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;

	return pParticle->m_flRoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector CSimpleEmitter::UpdateColor( const SimpleParticle *pParticle )
{
	static Vector	cColor;

	cColor[0] = pParticle->m_uchColor[0] / 255.0f;
	cColor[1] = pParticle->m_uchColor[1] / 255.0f;
	cColor[2] = pParticle->m_uchColor[2] / 255.0f;

	return cColor;
}

void CSimpleEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}

void CSimpleEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	if ( m_nSplitScreenPlayerSlot != -1 && m_nSplitScreenPlayerSlot != GET_ACTIVE_SPLITSCREEN_SLOT() )
		return;

	const SimpleParticle *pParticle = (const SimpleParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		//Render
		Vector	tPos;

		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;

		//Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tPos,
			UpdateColor( pParticle ),
			UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax ),
			UpdateScale( pParticle ),
			pParticle->m_flRoll
			);

		pParticle = (const SimpleParticle *)pIterator->GetNext( sortKey );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CSimpleEmitter::SetDrawBeforeViewModel( bool state )
{
	m_ParticleEffect.SetDrawBeforeViewModel( state );
}

void CSimpleEmitter::SetShouldDrawForSplitScreenUser( int nSlot )
{
	m_nSplitScreenPlayerSlot = nSlot;
}

//==================================================
// Particle Library
//==================================================

CEmberEffect::CEmberEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}

ks::intrusive_ptr<CEmberEffect> CEmberEffect::Create( const char *pDebugName )
{
	CEmberEffect *pRet = new CEmberEffect( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

void CEmberEffect::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	float	speed = VectorNormalize( pParticle->m_vecVelocity );
	Vector	offset;

	speed -= ( 12.0f * timeDelta );

	offset.Random( -0.125f, 0.125f );

	pParticle->m_vecVelocity += offset;
	VectorNormalize( pParticle->m_vecVelocity );

	pParticle->m_vecVelocity *= speed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector CEmberEffect::UpdateColor( const SimpleParticle *pParticle )
{
	Vector	color;
	float	ramp = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime );

	color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
	color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
	color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

	return color;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
CFireSmokeEffect::CFireSmokeEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}

ks::intrusive_ptr<CFireSmokeEffect> CFireSmokeEffect::Create( const char *pDebugName )
{
	CFireSmokeEffect *pRet = new CFireSmokeEffect( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

float CFireSmokeEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
void CFireSmokeEffect::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------
CFireParticle::CFireParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}

ks::intrusive_ptr<CFireParticle> CFireParticle::Create( const char *pDebugName )
{
	CFireParticle *pRet = new CFireParticle( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

Vector CFireParticle::UpdateColor( const SimpleParticle *pParticle )
{
	for ( int i = 0; i < 3; i++ )
	{
		//FIXME: This is frame dependant... but I don't want to store off start/end colors yet
		//pParticle->m_uchColor[i] = std::max( 0, pParticle->m_uchColor[i]-2 );
	}

	return CSimpleEmitter::UpdateColor( pParticle );
}

int CParticleEffect::GetToolParticleEffectId() const
{
	return m_nToolParticleEffectId;
}

int CParticleEffect::AllocateToolParticleEffectId()
{
	m_nToolParticleEffectId = ParticleMgr()->AllocateToolParticleEffectId();
	return m_nToolParticleEffectId;
}

void CParticleEffect::SetDontRemove( bool bSet)
{
	if( bSet )
		AddFlags( FLAG_DONT_REMOVE );
	else
		RemoveFlags( FLAG_DONT_REMOVE );
}

class CParticleMgr;

CParticleSphereRenderer::CParticleSphereRenderer()
{
	m_vBaseColor.Init();
	std::memset( &m_AmbientLight, 0, sizeof( m_AmbientLight ) );
	memset( &m_DirectionalLight, 0, sizeof( m_DirectionalLight ) );

	m_bUsingPixelShaders = false;
	m_iLastTickStartRenderCalled = -1;
	m_pParticleMgr = nullptr;
}

void CParticleSphereRenderer::Init( CParticleMgr *pParticleMgr, IMaterial *pMaterial )
{
	m_pParticleMgr = pParticleMgr;

	// Figure out how we need to draw.
	bool bFound = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$USINGPIXELSHADER", &bFound, false );
	if( bFound && pVar && pVar->GetIntValue() )
		m_bUsingPixelShaders = true;
	else
		m_bUsingPixelShaders = false;
}

void CParticleSphereRenderer::StartRender( VMatrix &effectMatrix )
{
	// We're about to be rendered.. set our directional lighting parameters for this particle system.
	if ( m_pParticleMgr )
	{
		m_pParticleMgr->SetDirectionalLightInfo( m_DirectionalLight );
	}

	m_iLastTickStartRenderCalled = gpGlobals->tickcount;
}

void CParticleSphereRenderer::AddLightColor( 
	Vector const *pPos,
	Vector const *pLightPos, 
	Vector const *pLightColor, 
	float flLightIntensity,
	Vector *pOutColor )
{
	if( flLightIntensity )
	{
		float fDist = pPos->DistToSqr( *pLightPos );
		float fAmt;
		if( fDist > 0.0001f )
			fAmt = flLightIntensity / fDist;
		else
			fAmt = 1000.f;

		*pOutColor += *pLightColor * fAmt;
	}
}

const Vector& CParticleSphereRenderer::GetBaseColor() const
{
	return m_vBaseColor;
}

void CParticleSphereRenderer::SetBaseColor( const Vector &vColor )
{
	m_vBaseColor = vColor;
}

const CParticleLightInfo& CParticleSphereRenderer::GetAmbientLight() const
{
	return m_AmbientLight;
}

void CParticleSphereRenderer::SetAmbientLight( const CParticleLightInfo &info )
{
	m_AmbientLight = info;
}

const CParticleLightInfo& CParticleSphereRenderer::GetDirectionalLight() const
{
	return m_DirectionalLight;
}

void CParticleSphereRenderer::SetDirectionalLight( const CParticleLightInfo &info )
{
	m_DirectionalLight = info;
}

void CParticleSphereRenderer::RenderParticle( 
	ParticleDraw *pDraw,
	const Vector &vOriginalPos,
	const Vector &vTransformedPos,
	float flAlpha,
	float flParticleSize,
	float flAngle )
{
	// Make sure they called StartRender on us so we were able to set the directional light parameters.

	Vector vColor = m_vBaseColor;
	AddLightColor( &vOriginalPos, &m_AmbientLight.m_vPos, &m_AmbientLight.m_vColor, m_AmbientLight.m_flIntensity, &vColor );
	
	// If the DX8 shader isn't going to handle the directional light color, then add its contribution here.
	if( !m_bUsingPixelShaders )
	{
		AddLightColor( &vOriginalPos, &m_DirectionalLight.m_vPos, &m_DirectionalLight.m_vColor, m_DirectionalLight.m_flIntensity, &vColor );
	}
	
	ClampColor( vColor );

	RenderParticle_Color255SizeNormalAngle(
		pDraw,
		vTransformedPos,
		vColor,			// ambient color
		flAlpha,		// alpha
		flParticleSize,
		vec3_origin,
		flAngle );
}

void CParticleSphereRenderer::RenderParticle_AddColor( 
	ParticleDraw *pDraw,
	const Vector &vOriginalPos,
	const Vector &vTransformedPos,
	float flAlpha,
	float flParticleSize,
	const Vector &vToAdd0to1
	)
{
	// Make sure they called StartRender on us so we were able to set the directional light parameters.

	Vector vColor = m_vBaseColor + vToAdd0to1;
	AddLightColor( &vOriginalPos, &m_AmbientLight.m_vPos, &m_AmbientLight.m_vColor, m_AmbientLight.m_flIntensity, &vColor );
	
	// If the DX8 shader isn't going to handle the directional light color, then add its contribution here.
	if( !m_bUsingPixelShaders )
	{
		AddLightColor( &vOriginalPos, &m_DirectionalLight.m_vPos, &m_DirectionalLight.m_vColor, m_DirectionalLight.m_flIntensity, &vColor );
	}
	
	ClampColor( vColor );

	RenderParticle_Color255Size(
		pDraw,
		vTransformedPos,
		vColor,			// ambient color
		flAlpha,		// alpha
		flParticleSize );
}

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CEnvProjectedTexture;

using namespace ::ks::reflect;

// Purpose: particle system definitions

typedef struct SProjectedTextureInfo
{
	int						m_nParticleID;
	IMaterial				*m_pMaterial;
	Vector					m_vOrigin;
	float					m_flSize;
	float					m_flRotation;
	float					m_r, m_g, m_b, m_a;
	CEnvProjectedTexture	*m_pEntity;
	bool					m_bUsedThisFrame;
} TProjectedTextureInfo;

static constexpr auto POINT_AT_ORIGIN_EPSILON = 0.1f;
//-----------------------------------------------------------------------------
// Interface to allow the particle system to call back into the game code
//-----------------------------------------------------------------------------
class CParticleSystemQuery
{
public:
	bool IsEditor( ) { return false; }

	// The answers of the game: ParticleQueryHooks below hands them to the particle system
	void GetLightingAtPoint( const Vector& vecOrigin, Color &cTint );
	void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const IHandleEntity *ignore,
							int collisionGroup, CBaseTrace *ptr );

	bool IsPointInSolid( const Vector& vecPos, const int nContentsMask );

	bool MovePointInsideControllingObject( CParticleCollection *pParticles,
												   void *pObject,
												   Vector *pPnt );
	void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionalBias,
		Vector *pHitBoxRelativeCoordOut,
		int *pHitBoxIndexOut,
		int nDesiredHitbox, 
		const char *pszHitboxSetName );

	void GetClosestControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsIn,
		float flBBoxScale,
		Vector *pPntsIn,
		Vector *pHitBoxRelativeCoordOut,
		int *pHitBoxIndexOut,
		int nDesiredHitbox, 
		const char *pszHitboxSetName );

	int GetRayTraceEnvironmentFromName( const char *pszRtEnvName );

	int GetCollisionGroupFromName( const char *pszCollisionGroupName );

	int GetControllingObjectHitBoxInfo(
		CParticleCollection *pParticles,
		int nControlPointNumber,
		int nBufSize,										// # of output slots available
		ModelHitBoxInfo_t *pHitBoxOutputBuffer, 
		const char *pszHitboxSetName );

	bool IsPointInControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecPos, bool bBBoxOnly, 
		const char *pszHitboxSetName  );

	void GetControllingObjectOBBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecMin, Vector vecMax );
	
	// Traces Four Rays against a defined RayTraceEnvironment
	void TraceAgainstRayTraceEnv( int envnumber, const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const ;

	Vector GetLocalPlayerPos( void );
	void GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight = nullptr, Vector *pUp = nullptr );

	Vector GetCurrentViewOrigin();

	int GetActivityCount();
	const char *GetActivityNameFromIndex( int nActivityIndex );
	int GetActivityNumber( void *pModel, const char *m_pszActivityName );

	float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale );
	void SetUpLightingEnvironment( const Vector& pos );

	void PreSimulate( );

	void PostSimulate( );

	void DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration );

	void BeginDrawModels( int nMaxNumToDraw, Vector const &vecCenterPosition, CParticleCollection *pParticles )
	{
	}

	void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
							int nSkin, int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f );

	void FinishDrawModels( CParticleCollection *pParticles )
	{
	}

	void *GetModel( char const *pMdlName );

	void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar );

private:

	CTSQueue< TProjectedTextureInfo * > m_ProjectedInfoAdds;
	std::vector< TProjectedTextureInfo * > m_ActiveProjectedInfos;

};

static CParticleSystemQuery s_ParticleSystemQuery;

#define KS_PARTICLE_QUERY_HOOK( name ) { std::cw<&CParticleSystemQuery::name>, &s_ParticleSystemQuery }
static const ParticleQueryHooks s_ParticleQueryHooks =
{
	KS_PARTICLE_QUERY_HOOK( GetLightingAtPoint ),
	KS_PARTICLE_QUERY_HOOK( TraceLine ),
	KS_PARTICLE_QUERY_HOOK( IsPointInSolid ),
	KS_PARTICLE_QUERY_HOOK( MovePointInsideControllingObject ),
	KS_PARTICLE_QUERY_HOOK( GetRayTraceEnvironmentFromName ),
	KS_PARTICLE_QUERY_HOOK( GetCollisionGroupFromName ),
	KS_PARTICLE_QUERY_HOOK( GetRandomPointsOnControllingObjectHitBox ),
	KS_PARTICLE_QUERY_HOOK( GetClosestControllingObjectHitBox ),
	KS_PARTICLE_QUERY_HOOK( GetControllingObjectHitBoxInfo ),
	KS_PARTICLE_QUERY_HOOK( GetControllingObjectOBBox ),
	KS_PARTICLE_QUERY_HOOK( TraceAgainstRayTraceEnv ),
	KS_PARTICLE_QUERY_HOOK( GetLocalPlayerPos ),
	KS_PARTICLE_QUERY_HOOK( GetLocalPlayerEyeVectors ),
	KS_PARTICLE_QUERY_HOOK( GetCurrentViewOrigin ),
	KS_PARTICLE_QUERY_HOOK( GetActivityCount ),
	KS_PARTICLE_QUERY_HOOK( GetActivityNameFromIndex ),
	KS_PARTICLE_QUERY_HOOK( GetActivityNumber ),
	KS_PARTICLE_QUERY_HOOK( GetPixelVisibility ),
	KS_PARTICLE_QUERY_HOOK( SetUpLightingEnvironment ),
	KS_PARTICLE_QUERY_HOOK( PreSimulate ),
	KS_PARTICLE_QUERY_HOOK( PostSimulate ),
	KS_PARTICLE_QUERY_HOOK( DebugDrawLine ),
	KS_PARTICLE_QUERY_HOOK( GetModel ),
	KS_PARTICLE_QUERY_HOOK( DrawModel ),
	KS_PARTICLE_QUERY_HOOK( UpdateProjectedTexture ),
};
#undef KS_PARTICLE_QUERY_HOOK

static IParticleSystemQuery s_ParticleSystemQueryForParticles( s_ParticleQueryHooks );
IParticleSystemQuery *g_pParticleSystemQuery = &s_ParticleSystemQueryForParticles;

static std::recursive_mutex s_LightMutex;
static std::recursive_mutex s_BoneMutex;

//-----------------------------------------------------------------------------
// Inherited from IParticleSystemQuery
//-----------------------------------------------------------------------------
void CParticleSystemQuery::GetLightingAtPoint( const Vector& vecOrigin, Color &cTint )
{
	CVProfScope _( "CParticleSystemQuery::GetLightingAtPoint", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( engine->IsInGame() )
	{
		s_LightMutex.lock();
		// Compute our lighting at our position
		Vector totalColor = engine->GetLightForPoint( vecOrigin, true );
		s_LightMutex.unlock();

		// Get our lighting information
		cTint.SetColor( totalColor.x*255, totalColor.y*255, totalColor.z*255, 0 );
	}
	else
	{
		// FIXME: Go through to the engine from the server to get these values
		cTint.SetColor( 255, 255, 255, 255 );
 	}

}

void CParticleSystemQuery::SetUpLightingEnvironment( const Vector& pos )
{
	if ( !engine->IsInGame() )
		return;

	s_LightMutex.lock();
	modelrender->SetupLighting( pos );
	s_LightMutex.unlock();
}

void CParticleSystemQuery::TraceLine( const Vector& vecAbsStart,
									  const Vector& vecAbsEnd, unsigned int mask, 
									  const IHandleEntity *ignore,
									  int collisionGroup, CBaseTrace *ptr )
{
	bool bDoTrace = false;
	bDoTrace = engine->IsInGame();
	if ( bDoTrace )
	{
		trace_t tempTrace;
		UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, ignore, collisionGroup, &tempTrace );
		std::memcpy( ptr, &tempTrace, sizeof ( CBaseTrace ) );
	}
	else
	{
		ptr->startsolid = 0;
		ptr->fraction = 1.0;
	}

}

bool CParticleSystemQuery::IsPointInSolid( const Vector& vecPos, const int nContentsMask )
{
	bool bDoTrace = false;
	bDoTrace = engine->IsInGame();
	if ( bDoTrace )
	{
		return ( UTIL_PointContents(vecPos, nContentsMask) & nContentsMask ) != 0;
	}

	return false;
}

bool CParticleSystemQuery::MovePointInsideControllingObject( 
	CParticleCollection *pParticles, void *pObject, Vector *pPnt )
{
	if (! pObject )
		return true;										// accept the input point unmodified

	Ray_t ray;
	trace_t tr;
	ray.Init( *pPnt, *pPnt );
	enginetrace->ClipRayToEntity( ray, MASK_ALL, (CBaseEntity *) pObject, &tr );
	
	return ( tr.startsolid );
}

static float GetSurfaceCoord( float flRand, float flMinX, float flMaxX )
{
	return Lerp( flRand, flMinX, flMaxX );

}

void CParticleSystemQuery::GetRandomPointsOnControllingObjectHitBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, 
	int nNumPtsOut,
	float flBBoxScale,
	int nNumTrysToGetAPointInsideTheModel,
	Vector *pPntsOut,
	Vector vecDirectionalBias,
	Vector *pHitBoxRelativeCoordOut,
	int *pHitBoxIndexOut,
	int nDesiredHitbox, 
	const char *pszHitboxSetName )
{

	bool bSucesss = false;

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = nullptr;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		float flRandMax = flBBoxScale;
		float flRandMin = 1.0 - flBBoxScale;
		Vector vecBasePos;
		pParticles->GetControlPointAtTime( nControlPointNumber, pParticles->m_flCurTime, &vecBasePos );

		s_BoneMutex.lock();
		CBaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
		if ( pAnimating )
		{
			
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{
		
				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
				
				if ( pStudioHdr )
				{
					// Try to get the desired set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), pszHitboxSetName );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
					
					if ( set )
					{
						bSucesss = true;
						
						Vector vecWorldPosition;
						float u = 0, v = 0, w = 0;
						int nHitbox = 0;
						int nNumIters = nNumTrysToGetAPointInsideTheModel;
						if (! vecDirectionalBias.IsZero( 0.0001 ) )
							nNumIters = std::max( nNumIters, 5 );
						int nHitboxMin = 0;
						int nHitboxMax = set->numhitboxes - 1;
						if ( nDesiredHitbox >= 0 )
						{
							nHitboxMin = std::min( set->numhitboxes - 1, nDesiredHitbox );
							nHitboxMax = std::min( set->numhitboxes - 1, nDesiredHitbox );
						}
				
						for( int i=0 ; i < nNumPtsOut; i++)
						{
							int nTryCnt = nNumIters;
							float flBestPointGoodness = -1.0e20;
							do
							{
								int nTryHitbox = pParticles->RandomInt( nHitboxMin, nHitboxMax );
								mstudiobbox_t *pBox = set->pHitbox(nTryHitbox);
								
								// E3 HACK - check for hitboxes at the origin and ignore those
								if ( fabs( (*hitboxbones[pBox->bone])[0][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[1][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[2][3] ) < POINT_AT_ORIGIN_EPSILON )
								{
									continue;
								}

								float flTryU = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryV = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryW = pParticles->RandomFloat( flRandMin, flRandMax );

								Vector vecLocalPosition;
								vecLocalPosition.x = GetSurfaceCoord( flTryU, pBox->bbmin.x*pAnimating->GetModelHierarchyScale(), pBox->bbmax.x*pAnimating->GetModelHierarchyScale() );
								vecLocalPosition.y = GetSurfaceCoord( flTryV, pBox->bbmin.y*pAnimating->GetModelHierarchyScale(), pBox->bbmax.y*pAnimating->GetModelHierarchyScale() );
								vecLocalPosition.z = GetSurfaceCoord( flTryW, pBox->bbmin.z*pAnimating->GetModelHierarchyScale(), pBox->bbmax.z*pAnimating->GetModelHierarchyScale() );

								Vector vecTryWorldPosition;

								VectorTransform( vecLocalPosition, *hitboxbones[pBox->bone], vecTryWorldPosition );
								
								float flPointGoodness = pParticles->RandomFloat( 0, 72 )
									+ DotProduct( vecTryWorldPosition - vecBasePos, 
												  vecDirectionalBias );

								if ( nNumTrysToGetAPointInsideTheModel )
								{
									// do a point in solid test
									Ray_t ray;
									trace_t tr;
									ray.Init( vecTryWorldPosition, vecTryWorldPosition );
									enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
									if ( tr.startsolid )
										flPointGoodness += 1000.; // got a point inside!
								}
								if ( flPointGoodness > flBestPointGoodness )
								{
									u = flTryU;
									v = flTryV;
									w = flTryW;
									vecWorldPosition = vecTryWorldPosition;
									nHitbox = nTryHitbox;
									flBestPointGoodness = flPointGoodness;
								}
							} while ( nTryCnt-- );
							*( pPntsOut++ ) = vecWorldPosition;
							if ( pHitBoxRelativeCoordOut )
								( pHitBoxRelativeCoordOut++ )->Init( u, v, w );
							if ( pHitBoxIndexOut )
								*( pHitBoxIndexOut++ ) = nHitbox;
						}
					}
				}
			}
		}

		if ( pMoveParent->IsBrushModel() )
		{
			Vector vecMin;
			Vector vecMax;
			matrix3x4_t matOrientation;
			Vector VecOrigin;
			pMoveParent->GetRenderBounds( vecMin, vecMax  );
			VecOrigin = pMoveParent->GetRenderOrigin();
			matOrientation = pMoveParent->EntityToWorldTransform();

			Vector vecWorldPosition;
			float u = 0, v = 0, w = 0;
			int nHitbox = 0;
			int nNumIters = nNumTrysToGetAPointInsideTheModel;
			if (! vecDirectionalBias.IsZero( 0.0001 ) )
				nNumIters = std::max( nNumIters, 5 );

			for( int i=0 ; i < nNumPtsOut; i++)
			{
				int nTryCnt = nNumIters;
				float flBestPointGoodness = -1.0e20;
				do
				{
					float flTryU = pParticles->RandomFloat( flRandMin, flRandMax );
					float flTryV = pParticles->RandomFloat( flRandMin, flRandMax );
					float flTryW = pParticles->RandomFloat( flRandMin, flRandMax );

					Vector vecLocalPosition;
					vecLocalPosition.x = GetSurfaceCoord( flTryU, vecMin.x, vecMax.x );
					vecLocalPosition.y = GetSurfaceCoord( flTryV, vecMin.y, vecMax.y );
					vecLocalPosition.z = GetSurfaceCoord( flTryW, vecMin.z, vecMax.z );

					Vector vecTryWorldPosition;
					VectorTransform( vecLocalPosition, matOrientation, vecTryWorldPosition );

					float flPointGoodness = pParticles->RandomFloat( 0, 72 )
						+ DotProduct( vecTryWorldPosition - vecBasePos, 
						vecDirectionalBias );

					if ( nNumTrysToGetAPointInsideTheModel )
					{
						// do a point in solid test
						Ray_t ray;
						trace_t tr;
						ray.Init( vecTryWorldPosition, vecTryWorldPosition );
						enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
						if ( tr.startsolid )
							flPointGoodness += 1000.; // got a point inside!
					}
					if ( flPointGoodness > flBestPointGoodness )
					{
						u = flTryU;
						v = flTryV;
						w = flTryW;
						vecWorldPosition = vecTryWorldPosition;
						nHitbox = 0;
						flBestPointGoodness = flPointGoodness;
					}
				} while ( nTryCnt-- );
				*( pPntsOut++ ) = vecWorldPosition;
				if ( pHitBoxRelativeCoordOut )
					( pHitBoxRelativeCoordOut++ )->Init( u, v, w );
				if ( pHitBoxIndexOut )
					*( pHitBoxIndexOut++ ) = nHitbox;
			}
		}

		s_BoneMutex.unlock();
	}
	if (! bSucesss )
	{
		// don't have a model or am in editor or something - fill return with control point
		for( int i=0 ; i < nNumPtsOut; i++)
		{
			pPntsOut[i] = pParticles->ControlPoint( nControlPointNumber ).m_Position; // fallback if anything goes wrong
			
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;
			
			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}
}

void CParticleSystemQuery::GetClosestControllingObjectHitBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, 
	int nNumPtsIn,
	float flBBoxScale,
	Vector *pPntsIn,
	Vector *pHitBoxRelativeCoordOut,
	int *pHitBoxIndexOut,
	int nDesiredHitbox, 
	const char *pszHitboxSetName )
{
	bool bSucesss = false;

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = nullptr;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		float flRandMax = flBBoxScale;
		float flRandMin = 1.0 - flBBoxScale;
		Vector vecBasePos;
		pParticles->GetControlPointAtTime( nControlPointNumber, pParticles->m_flCurTime, &vecBasePos );

		s_BoneMutex.lock();
		CBaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
		if ( pAnimating )
		{

			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];

			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{

				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );

				if ( pStudioHdr )
				{
					// Try to get the desired set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), pszHitboxSetName );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );

					if ( set )
					{
						bSucesss = true;

						Vector vecWorldPosition;
						float u = 0, v = 0, w = 0;
						int nHitbox = 0;

						int nHitboxMin = 0;
						int nHitboxMax = set->numhitboxes - 1;
						if ( nDesiredHitbox >= 0 )
						{
							nHitboxMin = std::min( set->numhitboxes - 1, nDesiredHitbox );
							nHitboxMax = std::min( set->numhitboxes - 1, nDesiredHitbox );
						}

						for( int i=0 ; i < nNumPtsIn; i++)
						{
							float flBestPointGoodness = std::numeric_limits<float>::max();
							Vector vecCurrentPoint = *( pPntsIn++ );
							for ( int j = nHitboxMin; j < nHitboxMax; j++ )
							{
								int nTryHitbox = j;
								mstudiobbox_t *pBox = set->pHitbox(nTryHitbox);

								float flTryU = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryV = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryW = pParticles->RandomFloat( flRandMin, flRandMax );

								Vector vecLocalPosition;
								vecLocalPosition.x = GetSurfaceCoord( flTryU, pBox->bbmin.x*pAnimating->GetModelScale(), pBox->bbmax.x*pAnimating->GetModelScale() );
								vecLocalPosition.y = GetSurfaceCoord( flTryV, pBox->bbmin.y*pAnimating->GetModelScale(), pBox->bbmax.y*pAnimating->GetModelScale() );
								vecLocalPosition.z = GetSurfaceCoord( flTryW, pBox->bbmin.z*pAnimating->GetModelScale(), pBox->bbmax.z*pAnimating->GetModelScale() );

								Vector vecTryWorldPosition;

								VectorTransform( vecLocalPosition, *hitboxbones[pBox->bone], vecTryWorldPosition );

								Vector vecBoxDistance;
								VectorTransform( ( ( pBox->bbmin + pBox->bbmax ) / 2 ), *hitboxbones[pBox->bone], vecBoxDistance );
								vecBoxDistance -= vecCurrentPoint;
								float flPointGoodness = vecBoxDistance.Length();

								if ( flPointGoodness < flBestPointGoodness )
								{
									u = flTryU;
									v = flTryV;
									w = flTryW;
									nHitbox = nTryHitbox;
									flBestPointGoodness = flPointGoodness;
								}
							}
							if ( pHitBoxRelativeCoordOut )
								( pHitBoxRelativeCoordOut++ )->Init( u, v, w );
							if ( pHitBoxIndexOut )
								*( pHitBoxIndexOut++ ) = nHitbox;
						}
					}
				}
			}
		}
		s_BoneMutex.unlock();
	}
	if (! bSucesss )
	{
		// don't have a model or am in editor or something - fill return with control point
		for( int i=0 ; i < nNumPtsIn; i++)
		{
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;

			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}
}

int CParticleSystemQuery::GetControllingObjectHitBoxInfo(
	CParticleCollection *pParticles,
	int nControlPointNumber,
	int nBufSize,										// # of output slots available
	ModelHitBoxInfo_t *pHitBoxOutputBuffer, 
	const char *pszHitboxSetName )
{
	int nRet = 0;

	s_BoneMutex.lock();

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = nullptr;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}

	if ( pMoveParent )
	{
		CBaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
		if ( pAnimating )
		{
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{
		
				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
				
				if ( pStudioHdr )
				{
					// Try to get the desired set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), pszHitboxSetName );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
					
					if ( set )
					{
						for( int i=0 ; i < set->numhitboxes; i++ )
						{
							mstudiobbox_t *pBox = set->pHitbox( i );

							// E3 HACK - check for hitboxes at the origin and ignore those
							if ( fabs( (*hitboxbones[pBox->bone])[0][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[1][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[2][3] ) < POINT_AT_ORIGIN_EPSILON )
							{
								continue;
							}

							pHitBoxOutputBuffer[nRet].m_vecBoxMins.x = pBox->bbmin.x;
							pHitBoxOutputBuffer[nRet].m_vecBoxMins.y = pBox->bbmin.y;
							pHitBoxOutputBuffer[nRet].m_vecBoxMins.z = pBox->bbmin.z;

							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.x = pBox->bbmax.x;
							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.y = pBox->bbmax.y;
							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.z = pBox->bbmax.z;

							pHitBoxOutputBuffer[nRet].m_Transform = *hitboxbones[pBox->bone];

							nRet++;

							if ( nRet >= nBufSize )
							{
								break;
							}
						}
					}
				}
			}
		}
		if ( pMoveParent->IsBrushModel() )
		{
			Vector vecMin;
			Vector vecMax;
			matrix3x4_t matOrientation;
			pMoveParent->GetRenderBounds( vecMin, vecMax  );
			matOrientation = pMoveParent->EntityToWorldTransform();
			pHitBoxOutputBuffer[0].m_vecBoxMins = vecMin;
			pHitBoxOutputBuffer[0].m_vecBoxMaxes = vecMax;
			pHitBoxOutputBuffer[0].m_Transform = matOrientation;
			nRet = 1;
		}
	}
	s_BoneMutex.unlock();
	return nRet;
}

bool CParticleSystemQuery::IsPointInControllingObjectHitBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, Vector vecPos, bool bBBoxOnly, 
	const char *pszHitboxSetName )
{
	bool bSuccess = false;

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = nullptr;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		s_BoneMutex.lock();
		CBaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();

		bool bInBBox = false;
		Vector vecBBoxMin;
		Vector vecBBoxMax;
		Vector vecOrigin;

		vecBBoxMin = pMoveParent->CollisionProp()->OBBMins();
		vecBBoxMax = pMoveParent->CollisionProp()->OBBMaxs();

		Vector vecLocalPos = vecPos;

		if ( pMoveParent->CollisionProp()->IsBoundsDefinedInEntitySpace() )
		{
			matrix3x4_t matOrientation;
			matOrientation = pMoveParent->EntityToWorldTransform();
			VectorITransform( vecPos, matOrientation, vecLocalPos );
		}
		if ( IsPointInBox( vecLocalPos, vecBBoxMin, vecBBoxMax ) )
			bInBBox = true;

		if ( bInBBox && bBBoxOnly )
			bSuccess = true;
		else if ( pAnimating && bInBBox )
		{
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{

				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );

				if ( pStudioHdr )
				{
					// Try to get the "effects" set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), pszHitboxSetName );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );

					if ( set )
					{
						// do a point in solid test
						Ray_t ray;
						trace_t tr;
						ray.Init( vecPos, vecPos );
						enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
						if ( tr.startsolid )
							bSuccess = true;
					}
				}
			}
		}
		else if ( pMoveParent->IsBrushModel() && bInBBox )
		{
			// do a point in solid test
			Ray_t ray;
			trace_t tr;
			ray.Init( vecPos, vecPos );
			enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
			if ( tr.startsolid )
				bSuccess = true;
		}

		s_BoneMutex.unlock();
	}
	return bSuccess;
}

void CParticleSystemQuery::GetControllingObjectOBBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, Vector vecMin, Vector vecMax )
{
	vecMin = vecMax = vec3_origin;

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = nullptr;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		vecMin = pMoveParent->CollisionProp()->OBBMins();
		vecMax = pMoveParent->CollisionProp()->OBBMaxs();
	}
}

extern std::vector< std::unique_ptr< RayTracingEnvironment > > g_RayTraceEnvironments;

void CParticleSystemQuery::TraceAgainstRayTraceEnv( int envnumber, const FourRays &rays, fltx4 TMin, fltx4 TMax,
													  RayTracingResult *rslt_out, int32 skip_id ) const
{
	if ( ks::is_valid_index( g_RayTraceEnvironments, envnumber ) )
	{
		RayTracingEnvironment *RtEnv = g_RayTraceEnvironments[ envnumber ].get();
		RtEnv->Trace4Rays( rays, TMin, TMax, rslt_out, skip_id );
	}
}

struct RayTraceEnvironmentNameRecord_t
{
	const char *m_pszGroupName;
	int m_nGroupID;
};

static RayTraceEnvironmentNameRecord_t s_RtEnvNameMap[]={
	{ "PRECIPITATION", 0 },
	{ "PRECIPITATIONBLOCKER", 1 },
};

int CParticleSystemQuery::GetRayTraceEnvironmentFromName( const char *pszRtEnvName )
{
	for(int i = 0; i < std::size( s_RtEnvNameMap ); i++ )
	{
		if ( ! stricmp( s_RtEnvNameMap[i].m_pszGroupName, pszRtEnvName ) )
			return s_RtEnvNameMap[i].m_nGroupID;
	}
	return 0;
}

struct CollisionGroupNameRecord_t
{
	const char *m_pszGroupName;
	int m_nGroupID;
};

static CollisionGroupNameRecord_t s_NameMap[]={
	{ "NONE", COLLISION_GROUP_NONE },
	{ "DEBRIS", COLLISION_GROUP_DEBRIS },
	{ "INTERACTIVE", COLLISION_GROUP_INTERACTIVE },
	{ "NPC", COLLISION_GROUP_NPC },
	{ "ACTOR", COLLISION_GROUP_NPC_ACTOR },
	{ "PASSABLE", COLLISION_GROUP_PASSABLE_DOOR },	
};

int CParticleSystemQuery::GetCollisionGroupFromName( const char *pszCollisionGroupName )
{
	for(int i = 0; i < std::size( s_NameMap ); i++ )
	{
		if ( ! stricmp( s_NameMap[i].m_pszGroupName, pszCollisionGroupName ) )
			return s_NameMap[i].m_nGroupID;
	}
	return COLLISION_GROUP_NONE;
}

Vector CParticleSystemQuery::GetLocalPlayerPos( void )
{
//	HACK_GETLOCALPLAYER_GUARD( "CParticleSystemQuery::GetLocalPlayerPos" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return vec3_origin;
	return pPlayer->WorldSpaceCenter();
}

void CParticleSystemQuery::GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight, Vector *pUp )
{
//	HACK_GETLOCALPLAYER_GUARD( "CParticleSystemQuery::GetLocalPlayerPos" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		*pForward = vec3_origin;
		*pRight = vec3_origin;
		*pUp = vec3_origin;
		return;
	}
	pPlayer->EyeVectors( pForward, pRight, pUp );
}

Vector CParticleSystemQuery::GetCurrentViewOrigin()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	return CurrentViewOrigin();

}

float CParticleSystemQuery::GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	pixelvis_queryparams_t params;
	params.Init( vecOrigin, flScale, 1.0 );
	float flVisibility = PixelVisibility_FractionVisible( params, pQueryHandle );
	flVisibility = std::max( 0.0f, flVisibility );
	return flVisibility;
}

void CParticleSystemQuery::DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration )
{
	debugoverlay->AddLineOverlay( origin, target, r, g, b, noDepthTest, duration );
}

static void SetBodygroup( studiohdr_t *pstudiohdr, int &body, int iGroup, int iValue )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if ( iGroup >= pstudiohdr->numbodyparts )
	{
		return;
	}

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if ( iValue >= pbodypart->nummodels )
	{
		return;
	}

	int iCurrent = ( body / pbodypart->base ) % pbodypart->nummodels;

	body = ( body - ( iCurrent * pbodypart->base ) + ( iValue * pbodypart->base ) );
}

// callback to draw models given abstract ptr
void CParticleSystemQuery::DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel, 
									  int nSkin, int nAnimationSequence, float flAnimationRate, float r, float g, float b, float a )
{
	model_t *pMDL = ( model_t * ) pModel;
	if ( pMDL )
	{
		MDLHandle_t hStudioHdr = modelinfo->GetCacheHandle( pMDL );
		studiohdr_t *pStudioHdr = mdlcache->GetStudioHdr( hStudioHdr );

		CMDL	MDL;
		MDL.SetMDL( hStudioHdr );

		SetBodygroup( pStudioHdr, MDL.m_nBody, nBodyPart, nSubModel );
		MDL.m_Color = Color( r * 255, g * 255, b * 255, a * 255 );
	
		MDL.m_nSkin = nSkin;
		MDL.m_nSequence = nAnimationSequence;
		MDL.m_flPlaybackRate = flAnimationRate;
		MDL.m_flTime = pParticles->m_flCurTime;

		if ( pStudioHdr )
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pStudioHdr->numbones );
			MDL.SetUpBones( DrawMatrix, pStudioHdr->numbones, rdBoneToWorld.Base() );
			MDL.Draw(DrawMatrix, rdBoneToWorld.Base(), STUDIORENDER_DRAW_NO_SHADOWS );
		}
		else
		{
			MDL.Draw(DrawMatrix);
		}

		//debugging
		//	debugoverlay->AddTextOverlay( origin, 0.1, "p", pMDL );
		//Vector myOrigin = Vector( DrawMatrix.m_flMatVal[0][3], DrawMatrix.m_flMatVal[1][3], DrawMatrix.m_flMatVal[2][3] );
		//Vector vecFwd, vecRight, vecUp;
		//MatrixVectors( pRenderable->m_DrawModelMatrix, &vecFwd, &vecRight, &vecUp );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecFwd, 255, 0, 0, true, 0.1 );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecUp, 0, 0, 255, true, 0.1 );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecRight, 0, 255, 0, true, 0.1 );
	}
}

void *CParticleSystemQuery::GetModel( char const *pMdlName )
{
// 	int modelIndex = modelinfo->GetModelIndex( pMdlName );
// 	const model_t *pModel = modelinfo->GetModel( modOelIndex );

	std::string	ModelName = "models/";

	ModelName += pMdlName;

 	model_t *pModel = (model_t *)engine->LoadModel( ModelName.c_str() ); //, true );
 	//pMdlName = "models/weapons/shells/shell_pistol.mdl";
	return ( void * )pModel;
}

void CParticleSystemQuery::PreSimulate( ) 
{ 
	for( int i = 0; i < std::ssize( m_ActiveProjectedInfos ); i++ )
	{
		m_ActiveProjectedInfos[ i ]->m_bUsedThisFrame = false;
	}
}

void CParticleSystemQuery::PostSimulate( ) 
{
	TProjectedTextureInfo *pInfo = nullptr;

	while( m_ProjectedInfoAdds.PopItem( &pInfo ) == true )
	{
		m_ActiveProjectedInfos.push_back( pInfo );
	}

	for( int i = 0; i < std::ssize( m_ActiveProjectedInfos ); i++ )
	{
		if ( m_ActiveProjectedInfos[ i ]->m_bUsedThisFrame == false )
		{
			delete m_ActiveProjectedInfos[ i ]->m_pEntity;
			m_ActiveProjectedInfos.erase( m_ActiveProjectedInfos.begin() + i );
			i--;
			continue;
		}
		if ( m_ActiveProjectedInfos[ i ]->m_pEntity == nullptr )
		{
			m_ActiveProjectedInfos[ i ]->m_pEntity = CEnvProjectedTexture::Create();
		}

		m_ActiveProjectedInfos[ i ]->m_pEntity->SetAbsOrigin( m_ActiveProjectedInfos[ i ]->m_vOrigin );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetMaterial( m_ActiveProjectedInfos[ i ]->m_pMaterial );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetLightColor( m_ActiveProjectedInfos[ i ]->m_r * 255, m_ActiveProjectedInfos[ i ]->m_g * 255, m_ActiveProjectedInfos[ i ]->m_b * 255, m_ActiveProjectedInfos[ i ]->m_a * 255 );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetSize( m_ActiveProjectedInfos[ i ]->m_flSize );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetRotation( m_ActiveProjectedInfos[ i ]->m_flRotation );
	}
}

void CParticleSystemQuery::UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar )
{
	TProjectedTextureInfo *pInfo = reinterpret_cast< TProjectedTextureInfo * >( pUserVar );
	if ( pInfo == nullptr )
	{
		pUserVar = pInfo = new TProjectedTextureInfo;
		memset( pInfo, 0, sizeof( *pInfo ) );
		m_ProjectedInfoAdds.PushItem( pInfo );
	}

	pInfo->m_nParticleID = nParticleID;
	pInfo->m_pMaterial = pMaterial;
	pInfo->m_vOrigin = vOrigin;
	pInfo->m_flSize = flRadius;
	pInfo->m_flRotation = flRotation;
	pInfo->m_r = r;
	pInfo->m_g = g;
	pInfo->m_b = b;
	pInfo->m_a = a;
	pInfo->m_bUsedThisFrame = true;

//	ClientEntityList().AddNonNetworkableEntity( this );
}

int CParticleSystemQuery::GetActivityCount()
{
	return 0;
}
const char* CParticleSystemQuery::GetActivityNameFromIndex( int nActivityIndex )
{
	return 0;
}

int CParticleSystemQuery::GetActivityNumber( void *pModel, const char *m_pszActivityName )
{
	model_t *pMDL = ( model_t * ) pModel;

	if ( pMDL )
	{
		studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pMDL );

		if ( !pStudioHdr )
			return -1;

		CStudioHdr studioHdr( pStudioHdr, mdlcache );

		int nActivityNum = LookupActivity( &studioHdr, m_pszActivityName );

		return SelectWeightedSequence( &studioHdr, nActivityNum );
	}

	return -1;
}

class IGameSystem;

using namespace ::ks::reflect;

static constexpr auto VAR = 0;
static constexpr auto ON = 1;
static constexpr auto OFF = 2;
static constexpr auto DETAILS = 3;

static CPerfVisualBenchmark s_PerfVisualBenchmark; // singleton

IGameSystem* PerfVisualBenchmark() { return &s_PerfVisualBenchmark; }

extern ConVar cl_mouseenable;

void usrCmd_Start()
{
	s_PerfVisualBenchmark.Start();
}

void usrCmd_Abort()
{
	s_PerfVisualBenchmark.Stop();
}

static ConCommand perfvisualbenchmark(side_registry(), "perfvisualbenchmark", usrCmd_Start);
static ConCommand perfvisualbenchmark_abort(side_registry(), "perfvisualbenchmark_abort", usrCmd_Abort);

CPerfVisualBenchmark::CPerfVisualBenchmark()
{
}

CPerfVisualBenchmark::~CPerfVisualBenchmark()
{
}

bool CPerfVisualBenchmark::Init()
{
	RunInfo_t runInfo;
	
	runInfo.m_pVarName = "";
	runInfo.m_pOnVal = "";
	runInfo.m_pOffVal = "";
	runInfo.m_pDescription = "Default";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawdetailprops";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "detail props";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawworld";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "world geometry";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawentities";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "entities";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_3dsky";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "3D skybox";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawdecals";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "decals";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "mat_viewportscale";
	runInfo.m_pOnVal = ".2";
	runInfo.m_pOffVal = "1.0";
	runInfo.m_pDescription = "fillrate";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawstaticprops";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "fillrate";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawbrushmodels";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "brush models";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_renderoverlayfragment";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "overlays";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawdisp";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "displacements";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawviewmodel";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "viewmodel";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "cl_drawhud";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "hud";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawparticles";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "particles";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawsprites";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "sprites";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "mat_bumpmap";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "bump mapping";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME_RELOAD_MATERIALS;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "mat_specular";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "specularity";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME_RELOAD_MATERIALS;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "mat_drawwater";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "water";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_dynamic";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "dynamic lighting";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_shadows";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "shadows";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	runInfo.m_pVarName = "r_drawropes";
	runInfo.m_pOnVal = "0";
	runInfo.m_pOffVal = "1";
	runInfo.m_pDescription = "ropes";
	runInfo.m_flStabilizeTime = FPS_STABILIZE_TIME;
	m_RunInfo.push_back( runInfo );

	m_bIsOn = false;					//is this thing on?
	return true;
}

void CPerfVisualBenchmark::Start()
{
	m_bSaveMouseEnable = cl_mouseenable.GetBool();
	cl_mouseenable.SetValue( 0 );

	m_iCurVar = 0;
	m_flTimer = gpGlobals->realtime + FPS_STABILIZE_TIME;
	m_bWaiting = true;
	m_bIsOn = true;									// showtime!
	engine->ClientCmd_Unrestricted("cancelselect");				// exit menu and console
//	engine->ClientCmd_Unrestricted("wait");				
//	engine->ClientCmd_Unrestricted("setpause");					// pause the mofo
	engine->ClientCmd_Unrestricted("host_timescale 0.0001");					// pause the mofo
	
}

void CPerfVisualBenchmark::Stop()
{
	cl_mouseenable.SetValue( m_bSaveMouseEnable );

	m_bIsOn = false;
	Print();
	engine->ClientCmd_Unrestricted("host_timescale 0");					// pause the mofo
//	engine->ClientCmd_Unrestricted("unpause");				// unpause the mofo
//	engine->ClientCmd_Unrestricted("wait");				
	engine->ClientCmd_Unrestricted("toggleconsole");
}

void CPerfVisualBenchmark::PreRender( )
{
	if (!m_bIsOn)
		return;

	// Wait for the timer
	if ( m_flTimer > gpGlobals->realtime )
		return;

	if ( m_bWaiting )
	{
		m_flTimer = gpGlobals->realtime + FPS_MEASURE_TIME;
		m_flStartMeasureTime = gpGlobals->realtime;
		m_nStartFrameCount = gpGlobals->framecount;
		m_bWaiting = false;
		return;
	}

	// Ok, we were measuring, lets calculate the results
	float flDenom = gpGlobals->realtime - m_flStartMeasureTime;
	if (flDenom == 0)
	{
		flDenom = 1.0f;
	}

	// note the current avged fps;
	float flAveFPS = (gpGlobals->framecount - m_nStartFrameCount) / flDenom;
	m_RunInfo[m_iCurVar].m_flFPS = flAveFPS;

	m_flTimer = gpGlobals->realtime + FPS_STABILIZE_TIME;
	m_bWaiting = true;

	char combuffer[255];

	// Turn off any previous value
	if ( m_RunInfo[m_iCurVar].m_pVarName )
	{
		V_snprintf(combuffer, sizeof(combuffer), "%s %s\n", m_RunInfo[m_iCurVar].m_pVarName, m_RunInfo[m_iCurVar].m_pOffVal );	//turn off current var
		engine->ClientCmd_Unrestricted(combuffer);
	}

	// next var
	m_iCurVar++;				
	if (m_iCurVar == std::ssize( m_RunInfo ))
	{
		Stop();
		return;
	}

	V_snprintf(combuffer, sizeof(combuffer), "%s %s\n",m_RunInfo[m_iCurVar].m_pVarName, m_RunInfo[m_iCurVar].m_pOnVal);  //turn on next var
	engine->ClientCmd_Unrestricted(combuffer);
}

void CPerfVisualBenchmark::Print()				//  sort and print into console
{
	for (int i = 0; i<std::ssize( m_RunInfo ); i++)
	{
		int curMax = 0;
		for (int j = 0; j<std::ssize( m_RunInfo ); j++)
		{
			if (m_RunInfo[j].m_flFPS > m_RunInfo[curMax].m_flFPS)
			{
				curMax = j;
			}
		}
		Msg("%.0f fps - %s\n",m_RunInfo[curMax].m_flFPS, m_RunInfo[curMax].m_pDescription);
		m_RunInfo[curMax].m_flFPS=-1;
	}
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CPhysBox, DT_PhysBox, CPhysBox )

CPhysBox::CPhysBox()
{
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t CPhysBox::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;
	return SHADOWS_RENDER_TO_TEXTURE;
}

CPhysBox::~CPhysBox()
{
}
