// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

module;
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "bone_accessor.h"
#include "bone_setup.h"
#include "client_class.h"
#include "edict.h"
#include "networkvar.h"
#include "reflect.h"
#include "vphysics/constraints.h"
#include "reflect_datamap.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "engine/ivmodelinfo.h"
#include "iclientnetworkable.h"
#include "vcollide_parse.h"
#include "vphysics/collision_set.h"
#include "cdll_client_int.h"
#include "basehandle.h"
#include "dt_recv.h"
#include "irendertorthelperobject.h"
#include "istudiorender.h"
#include "renderparm.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "collisionutils.h"
#include "engine/IEngineTrace.h"
#include "materialsystem/imesh.h"
#include "rope_shared.h"
#include "simple_physics.h"
#include "tier2/resourceprecacher.h"
#include "videocfg/videocfg.h"
#include "vphysics_interface.h"
#include "te_effect_dispatch.h"
#include "choreoactor.h"
#include "choreoscene.h"
#include "networkstringtabledefs.h"
#include "soundflags.h"
#include "studio.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :sharedvar;
import :achievements_and_stats_interface;
import ks.game.mapentities_shared;
import ks.core.numeric;
import ks.core.platform;
import ks.core.wchartypes;
import ks.tier1.strtools;
import :entitydatainstantiator;
import ks.game.gamemovement;
import ks.core.random;
import ks.core.tslist;
import ks.core.types;
import ks.game.rope_helpers;
import ks.math.vector4d;
import ks.tier1.bitbuf;
import ks.tier1.keyvalues;
import ks.vstdlib.callqueue;
import ks.vstdlib.jobthread;
import ks.game.rumble_shared;
import ks.math.vector2d;
import ks.tier1.exprevaluator;
import ks.core.threadtools;
import ks.tier1.byte_reader;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CRagdoll;
class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

extern ConVar r_sequence_debug;

CRagdoll::CRagdoll()
{
	m_ragdoll.listCount = 0;
	m_vecLastOrigin.Init();
	m_flLastOriginChangeTime = - 1.0f;
	m_flAwakeTime = -1.0f;
	m_lastUpdate = -std::numeric_limits<float>::max();
}

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CRagdoll )

IPhysicsObject *CRagdoll::GetElement( int elementNum )
{ 
	return m_ragdoll.list[elementNum].pObject;
}

void CRagdoll::BuildRagdollBounds( CBaseEntity *ent )
{
	Vector mins, maxs, size;
	modelinfo->GetModelBounds( ent->GetModel(), mins, maxs );
	size = (maxs - mins) * 0.5;
	float radius = size.Length();

	m_mins.Init(-radius,-radius,-radius);
	m_maxs.Init(radius,radius,radius);
}

static ConVar cl_ragdoll_self_collision( side_registry(), "cl_ragdoll_self_collision", "1", FCVAR_DEVELOPMENTONLY );
extern ConVar cl_ragdoll_collide;

void CRagdoll::Init( 
	CBaseEntity *ent, 
	CStudioHdr *pstudiohdr, 
	const Vector &forceVector, 
	int forceBone, 
	const matrix3x4_t *pDeltaBones0, 
	const matrix3x4_t *pDeltaBones1, 
	const matrix3x4_t *pCurrentBonePosition, 
	float dt,
	bool bFixedConstraints,
	bool bBleedOutOnSleep )
{
	ragdollparams_t params;
	params.pGameData = static_cast<void *>( ent );
	params.modelIndex = ent->GetModelIndex();
	params.pCollide = modelinfo->GetVCollide( params.modelIndex );

	if ( !params.pCollide )
	{
		return;
	}

	params.pStudioHdr = pstudiohdr;
	params.forceVector = forceVector;
	params.forceBoneIndex = forceBone;
	params.forcePosition.Init();
	params.pCurrentBones = pCurrentBonePosition;
	params.jointFrictionScale = 1.0;
	params.allowStretch = false;
	params.fixedConstraints = bFixedConstraints;
	RagdollCreate( m_ragdoll, params, physenv );
	ent->VPhysicsSetObject( nullptr );
	ent->VPhysicsSetObject( m_ragdoll.list[0].pObject );
	// Mark the ragdoll as debris.
	ent->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	// forcibly disable ragdoll self collisions on the client in L4D / x360 to save perf
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		m_ragdoll.list[i].pObject->SetUseAlternateGravity(true);
		if ( !cl_ragdoll_self_collision.GetBool() && !cl_ragdoll_collide.GetBool() )
		{
			PhysSetGameFlags(m_ragdoll.list[i].pObject, FVPHYSICS_NO_SELF_COLLISIONS);
			m_ragdoll.list[i].pObject->SetCollisionHints(COLLISION_HINT_DEBRIS);
		}
	}

	RagdollApplyAnimationAsVelocity( m_ragdoll, pDeltaBones0, pDeltaBones1, dt );
	RagdollActivate( m_ragdoll, params.pCollide, ent->GetModelIndex() );
	m_ragdoll.list[0].pObject->GetPosition( &m_origin, 0 );

	// It's moving now...
	m_flLastOriginChangeTime = physenv->GetSimulationTime();
	m_flAwakeTime = physenv->GetSimulationTime();
	m_doBleedOut = bBleedOutOnSleep;

	// So traces hit it.
	ent->AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	if ( !m_ragdoll.listCount )
		return;

	BuildRagdollBounds( ent );

#if RAGDOLL_VISUALIZE
	memcpy( m_savedBone1, &pDeltaBones0[0], sizeof(matrix3x4_t) * pstudiohdr->numbones() );
	memcpy( m_savedBone2, &pDeltaBones1[0], sizeof(matrix3x4_t) * pstudiohdr->numbones() );
	memcpy( m_savedBone3, &pCurrentBonePosition[0], sizeof(matrix3x4_t) * pstudiohdr->numbones() );
#endif
}

CRagdoll::~CRagdoll( void )
{
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		IPhysicsObject *pObject = m_ragdoll.list[i].pObject;
		if ( pObject )
		{
			// Disable collision on all ragdoll parts before calling RagdollDestroy
			// (which might cause touch callbacks on the ragdoll otherwise, which is
			// very bad for a half deleted ragdoll).
			pObject->EnableCollisions( false );
		}
	}

	RagdollDestroy( m_ragdoll );
}

void CRagdoll::RagdollBone( CBaseEntity *ent, const mstudiobone_t *pbones, int boneCount, bool *boneSimulated, CBoneAccessor &pBoneToWorld )
{
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		if ( RagdollGetBoneMatrix( m_ragdoll, pBoneToWorld, i ) )
		{
			boneSimulated[m_ragdoll.boneIndex[i]] = true;
		}
	}
}

const Vector &CRagdoll::GetRagdollOrigin( )
{
	return m_origin;
}

void CRagdoll::GetRagdollBounds( Vector &theMins, Vector &theMaxs )
{
	theMins = m_mins;
	theMaxs = m_maxs;
}

void CRagdoll::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	if ( m_lastUpdate == gpGlobals->curtime )
		return;

	m_ragdoll.list[0].pObject->GetPosition( &m_origin, 0 );
	m_lastUpdate = gpGlobals->curtime;

	Vector origin = GetRagdollOrigin();
	RagdollComputeApproximateBbox( m_ragdoll, origin, m_mins, m_maxs );
	m_mins -= origin;
	m_maxs -= origin;
	m_allAsleep = RagdollIsAsleep( m_ragdoll );

	if ( !m_allAsleep )
	{
		if ( m_ragdoll.pGroup->IsInErrorState() )
		{
			CBaseEntity *pEntity = static_cast<CBaseEntity *>(m_ragdoll.list[0].pObject->GetGameData());
			RagdollSolveSeparation( m_ragdoll, pEntity );
		}
		// See if we should go to sleep...
		CheckSettleStationaryRagdoll();
	}
	else if ( m_doBleedOut )
	{
		// Stopped moving, now create the blood pool
		CreateBloodPool();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Transforms a vector from the given bone's space to world space
//-----------------------------------------------------------------------------
bool CRagdoll::TransformVectorToWorld( int iBoneIndex, const Vector *vPosition, Vector *vOut )
{
	int listIndex = -1;
	if( iBoneIndex >= 0 && iBoneIndex < m_ragdoll.listCount )
	{
		for ( int i = 0; i < m_ragdoll.listCount; ++i )
		{
			if ( m_ragdoll.boneIndex[i] == iBoneIndex )
			{
				listIndex = i;
			}
		}

		if( listIndex != -1 )
		{
			m_ragdoll.list[listIndex].pObject->LocalToWorld( vOut, *vPosition );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CRagdoll::PhysForceRagdollToSleep()
{
	IPhysicsObject *pList[32];
	Vector vel;
	AngularImpulse angVel;
	vel.Init();
	angVel.Init();
	for ( int i = 0; i < m_ragdoll.listCount; i++ )
	{
		if ( m_ragdoll.list[i].pObject )
		{
			m_ragdoll.list[i].pObject->SetVelocity( &vel, &angVel );
			pList[i] = m_ragdoll.list[i].pObject;
		}
	}
	physenv->ForceObjectsToSleep(pList, m_ragdoll.listCount);
}

static constexpr auto RAGDOLL_SLEEP_TOLERANCE = 1.0f;
static ConVar ragdoll_sleepaftertime( side_registry(), "ragdoll_sleepaftertime", "4", 0, "After this many seconds of being basically stationary, the ragdoll will go to sleep." );

void CRagdoll::CheckSettleStationaryRagdoll()
{
	float dt = physenv->GetSimulationTime() - m_flAwakeTime;
	float tolerance = clamp( (RAGDOLL_SLEEP_TOLERANCE * dt), RAGDOLL_SLEEP_TOLERANCE, (RAGDOLL_SLEEP_TOLERANCE * 10) );

	Vector delta = GetRagdollOrigin() - m_vecLastOrigin;
	for ( int i = 0; i < 3; ++i )
	{
		// It's still moving...
		if ( fabs( delta[ i ] ) > tolerance )
		{
			m_flLastOriginChangeTime = physenv->GetSimulationTime();
			m_vecLastOrigin = GetRagdollOrigin();
			//Msg( "%d [%p] Still moving (tolerance is %f, dt is %f)\n", gpGlobals->tickcount, this, tolerance, dt );
			return;
		}
	}

	//Msg( "%d [%p] Settling - tolerance is %f, delta is %f %f %f\n", gpGlobals->tickcount, this, tolerance, delta.x, delta.y, delta.z );

	// It has stopped moving, see if it
	dt = physenv->GetSimulationTime() - m_flLastOriginChangeTime;
	if ( dt < ragdoll_sleepaftertime.GetFloat() )
		return;

	//Msg( "%d [%p] FORCE SLEEP\n",gpGlobals->tickcount, this );

	// Force it to go to sleep
	PhysForceRagdollToSleep();

	// Now that it stopped, create the blood pool
	CreateBloodPool();
}

void CRagdoll::ResetRagdollSleepAfterTime( void )
{
	m_flLastOriginChangeTime = physenv->GetSimulationTime();
	m_flAwakeTime = physenv->GetSimulationTime();
}

void CRagdoll::CreateBloodPool( void )
{

}

void CRagdoll::DrawWireframe()
{
	IMaterial *pWireframe = materials->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER);

	int i;
	matrix3x4_t matrix;
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		static color32 debugColor = {0,255,255,0};

		// draw the actual physics positions, not the cleaned up animation position
		m_ragdoll.list[i].pObject->GetPositionMatrix( &matrix );
		const CPhysCollide *pCollide = m_ragdoll.list[i].pObject->GetCollide();
		engine->DebugDrawPhysCollide( pCollide, pWireframe, matrix, debugColor );
	}

#if RAGDOLL_VISUALIZE
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		static color32 debugColor = {255,0,0,0};

		const CPhysCollide *pCollide = m_ragdoll.list[i].pObject->GetCollide();
		engine->DebugDrawPhysCollide( pCollide, pWireframe, m_savedBone1[m_ragdoll.boneIndex[i]], debugColor );
	}
	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		static color32 debugColor = {0,255,0,0};

		const CPhysCollide *pCollide = m_ragdoll.list[i].pObject->GetCollide();
		engine->DebugDrawPhysCollide( pCollide, pWireframe, m_savedBone2[m_ragdoll.boneIndex[i]], debugColor );
	}

	for ( i = 0; i < m_ragdoll.listCount; i++ )
	{
		static color32 debugColor = {0,0,255,0};

		const CPhysCollide *pCollide = m_ragdoll.list[i].pObject->GetCollide();
		engine->DebugDrawPhysCollide( pCollide, pWireframe, m_savedBone3[m_ragdoll.boneIndex[i]], debugColor );
	}
#endif
}

CRagdoll *CreateRagdoll( 
	CBaseEntity *ent, 
	CStudioHdr *pstudiohdr, 
	const Vector &forceVector, 
	int forceBone, 
	const matrix3x4_t *pDeltaBones0, 
	const matrix3x4_t *pDeltaBones1, 
	const matrix3x4_t *pCurrentBonePosition,
	float dt,
	bool bFixedConstraints,
	bool bBleedOutOnSleep )
{
	CRagdoll *pRagdoll = new CRagdoll;
	pRagdoll->Init( ent, pstudiohdr, forceVector, forceBone, pDeltaBones0, pDeltaBones1, pCurrentBonePosition, dt, bFixedConstraints, bBleedOutOnSleep );

	if ( !pRagdoll->IsValid() )
	{
		Msg("Bad ragdoll for %s\n", pstudiohdr->pszName() );
		delete pRagdoll;
		pRagdoll = nullptr;
	}
	return pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CServerRagdoll : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_Ragdoll" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CServerRagdoll, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	CServerRagdoll( void );

	// Inherited from IClientUnknown
public:
	virtual IClientModelRenderable*	GetClientModelRenderable();

	virtual void PostDataUpdate( DataUpdateType_t updateType );

	virtual int InternalDrawModel( int flags, const RenderableInstance_t &instance );
	virtual CStudioHdr *OnNewModel( void );
	virtual void	SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

	void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual bool Simulate( void );
	virtual void AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed );
	IPhysicsObject *GetElement( int elementNum );
	virtual void UpdateOnRemove();
	virtual float LastBoneChangedTime();

	// Incoming from network
	[[= ks::reflect::Net{ .varlen = true } ]] Vector		m_ragPos[RAGDOLL_MAX_ELEMENTS];
	[[= ks::reflect::Net{ .varlen = true } ]] QAngle		m_ragAngles[RAGDOLL_MAX_ELEMENTS];

	CInterpolatedVarArray< Vector, RAGDOLL_MAX_ELEMENTS >	m_iv_ragPos;
	CInterpolatedVarArray< QAngle, RAGDOLL_MAX_ELEMENTS >	m_iv_ragAngles;

	int			m_elementCount;
	int			m_boneIndex[RAGDOLL_MAX_ELEMENTS];

private:
	CServerRagdoll( const CServerRagdoll &src );

	typedef CHandle<CBaseAnimating> CBaseAnimatingHandle;
	CNetworkVar( CBaseAnimatingHandle, m_hUnragdoll, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flBlendWeight, [[= ks::reflect::Net{} ]] );
	float m_flBlendWeightCurrent;
	CNetworkVar( int, m_nOverlaySequence, [[= ks::reflect::Net{} ]] );
	float m_flLastBoneChangeTime;
};

EXTERN_RECV_TABLE(DT_Ragdoll);
IMPLEMENT_REFLECT_CLIENTCLASS( CServerRagdoll, DT_Ragdoll, CRagdollProp )

CServerRagdoll::CServerRagdoll( void ) :
	m_iv_ragPos("CServerRagdoll::m_iv_ragPos"),
	m_iv_ragAngles("CServerRagdoll::m_iv_ragAngles")
{
	m_elementCount = 0;
	m_flLastBoneChangeTime = -std::numeric_limits<float>::max();

	AddVar( m_ragPos, &m_iv_ragPos, LATCH_SIMULATION_VAR  );
	AddVar( m_ragAngles, &m_iv_ragAngles, LATCH_SIMULATION_VAR );

	m_flBlendWeight = 0.0f;
	m_flBlendWeightCurrent = 0.0f;
	m_nOverlaySequence = -1;
}

void CServerRagdoll::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	m_iv_ragPos.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, true );
	m_iv_ragAngles.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, true );
	// this is the local client time at which this update becomes stale
	m_flLastBoneChangeTime = gpGlobals->curtime + GetInterpolationAmount(m_iv_ragPos.GetType());
	if ( m_flBlendWeightCurrent != m_flBlendWeight )
	{
		AddToEntityList( ENTITY_LIST_SIMULATE );
	}
}

float CServerRagdoll::LastBoneChangedTime()
{
	return m_flLastBoneChangeTime;
}

int CServerRagdoll::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	int ret = BaseClass::InternalDrawModel( flags, instance );
	if ( vcollide_wireframe.GetBool() )
	{
		vcollide_t *pCollide = modelinfo->GetVCollide( GetModelIndex() );
		IMaterial *pWireframe = materials->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER);

		matrix3x4_t matrix;
		for ( int i = 0; i < m_elementCount; i++ )
		{
			static color32 debugColor = {0,255,255,0};

			AngleMatrix( m_ragAngles[i], m_ragPos[i], matrix );
			engine->DebugDrawPhysCollide( pCollide->solids[i], pWireframe, matrix, debugColor );
		}
	}
	return ret;
}

CStudioHdr *CServerRagdoll::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	if ( !m_elementCount )
	{
		vcollide_t *pCollide = modelinfo->GetVCollide( GetModelIndex() );
		if ( !pCollide )
		{
			const char *pszName;
			pszName = modelinfo->GetModelName( modelinfo->GetModel( GetModelIndex() ) );
			Msg( "*** ERROR: CServerRagdoll::InitModel: %s missing vcollide data ***\n", (pszName) ? pszName : "<null>" );
			m_elementCount = 0;
		}
		else
		{
			m_elementCount = RagdollExtractBoneIndices( m_boneIndex, hdr, pCollide );
		}
		m_iv_ragPos.SetMaxCount( gpGlobals->curtime, m_elementCount );
		m_iv_ragAngles.SetMaxCount( gpGlobals->curtime, m_elementCount );
	}

	return hdr;
}

//-----------------------------------------------------------------------------
// Should we participate in the model fast path?
//-----------------------------------------------------------------------------
IClientModelRenderable*	CServerRagdoll::GetClientModelRenderable()
{
	// FIXME: Once we get modelrender->SetViewTarget working in the model fast
	// path, we can eliminate the check for eye attachment
	if ( m_iEyeAttachment > 0 )
		return nullptr;

	return BaseClass::GetClientModelRenderable();
}

//-----------------------------------------------------------------------------
// Purpose: clear out any face/eye values stored in the material system
//-----------------------------------------------------------------------------
void CServerRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );

	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return;

	int nFlexDescCount = hdr->numflexdesc();
	if ( nFlexDescCount )
	{
		memset( pFlexWeights, 0, nFlexWeightCount * sizeof(float) );
		if ( pFlexDelayedWeights )
		{
			memset( pFlexDelayedWeights, 0, nFlexWeightCount * sizeof(float) );
		}
	}

	if ( m_iEyeAttachment > 0 )
	{
		matrix3x4_t attToWorld;
		if (GetAttachment( m_iEyeAttachment, attToWorld ))
		{
			Vector local, tmp;
			local.Init( 1000.0f, 0.0f, 0.0f );
			VectorTransform( local, attToWorld, tmp );
			modelrender->SetViewTarget( GetModelPtr(), GetBody(), tmp );
		}
	}
}

void CServerRagdoll::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if( !CollisionProp()->IsBoundsDefinedInEntitySpace() )
	{
		IRotateAABB( EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), theMins, theMaxs );
	}
	else
	{
		theMins = CollisionProp()->OBBMins();
		theMaxs = CollisionProp()->OBBMaxs();
	}
}

bool CServerRagdoll::Simulate( void )
{
	bool bRet = BaseClass::Simulate();

	// Move blend weight toward target over 0.2 seconds
	m_flBlendWeightCurrent = Approach( m_flBlendWeight, m_flBlendWeightCurrent, gpGlobals->frametime * 5.0f );
	if ( m_flBlendWeightCurrent != m_flBlendWeight )
		bRet = true;
	return bRet;
}

void CServerRagdoll::AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	BaseClass::AccumulateLayers( boneSetup, pos, q, currentTime );

	if ( m_nOverlaySequence >= 0 && m_nOverlaySequence < boneSetup.GetStudioHdr()->GetNumSeq() )
	{
		if (r_sequence_debug.GetInt() == entindex())
		{
			DevMsgRT( "%8.4f : %30s : %5.3f : %4.2f\n", currentTime, boneSetup.GetStudioHdr()->pSeqdesc( m_nOverlaySequence ).pszLabel(), GetCycle(), m_flBlendWeightCurrent );
		}
		boneSetup.AccumulatePose( pos, q, m_nOverlaySequence, GetCycle(), m_flBlendWeightCurrent, currentTime, m_pIk );
	}
}

void CServerRagdoll::BuildTransformations( CStudioHdr *hdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	if ( !hdr )
		return;
	matrix3x4a_t bonematrix;
	bool boneSimulated[MAXSTUDIOBONES];

	// no bones have been simulated
	memset( boneSimulated, 0, sizeof(boneSimulated) );
	const mstudiobone_t *pbones = hdr->pBone( 0 );

	mstudioseqdesc_t *pSeqDesc = nullptr;
	if ( m_nOverlaySequence >= 0 && m_nOverlaySequence < hdr->GetNumSeq() )
	{
		pSeqDesc = &hdr->pSeqdesc( m_nOverlaySequence );
	}

	int i;
	for ( i = 0; i < m_elementCount; i++ )
	{
		int index = m_boneIndex[i];
		if ( index >= 0 )
		{
			if ( hdr->boneFlags(index) & boneMask )
			{
				boneSimulated[index] = true;
				matrix3x4_t &matrix = GetBoneForWrite( index );

				if ( m_flBlendWeightCurrent != 0.0f && pSeqDesc && 
					 // FIXME: this bone access is illegal
					 pSeqDesc->weight( index ) != 0.0f )
				{
					// Use the animated bone position instead
					boneSimulated[index] = false;
				}
				else
				{	
					AngleMatrix( m_ragAngles[i], m_ragPos[i], matrix );
				}
			}
		}
	}

	for ( i = 0; i < hdr->numbones(); i++ ) 
	{
		if ( !( hdr->boneFlags( i ) & boneMask ) )
			continue;

		// BUGBUG: Merge this code with the code in c_baseanimating somehow!!!
		// animate all non-simulated bones
		if ( boneSimulated[i] || 
			CalcProceduralBone( hdr, i, m_BoneAccessor ) )
		{
			continue;
		}
		else
		{
			QuaternionMatrix( q[i], pos[i], bonematrix );

			if (pbones[i].parent == -1) 
			{
				ConcatTransforms( cameraTransform, bonematrix, GetBoneForWrite( i ) );
			} 
			else 
			{
				ConcatTransforms_Aligned( GetBone( pbones[i].parent ), bonematrix, GetBoneForWrite( i ) );
			}
		}

		if ( pbones[i].parent == -1 ) 
		{
			// Apply client-side effects to the transformation matrix
		//	ApplyBoneMatrixTransform( GetBoneForWrite( i ) );
		}
	}
}

IPhysicsObject *CServerRagdoll::GetElement( int elementNum ) 
{ 
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 	virtual void
//-----------------------------------------------------------------------------
void CServerRagdoll::UpdateOnRemove()
{
	CBaseAnimating *anim = m_hUnragdoll.Get();
	if ( nullptr != anim && 
		anim->GetModel() && 
		( anim->GetModel() == GetModel() ) )
	{
		// Need to tell CBaseAnimating to blend out of the ragdoll data that we received last
		CBaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		anim->CreateUnragdollInfo( this );
	}

	// Do last to mimic destrictor order
	BaseClass::UpdateOnRemove();
}

static int GetHighestBit( int flags )
{
	for ( int i = 31; i >= 0; --i )
	{
		if ( flags & (1<<i) )
			return (1<<i);
	}

	return 0;
}

static constexpr auto ATTACH_INTERP_TIME = 0.2;
class CServerRagdollAttached : public CServerRagdoll
{
	[[= ks::reflect::NetTable{ .name = "DT_Ragdoll_Attached" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CServerRagdollAttached, CServerRagdoll );
public:
	CServerRagdollAttached( void ) 
	{
		m_bHasParent = false;
		m_vecOffset.Init();
	}
	DECLARE_CLIENTCLASS();
	bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
	{
		if ( GetMoveParent() )
		{
			// HACKHACK: Force the attached bone to be set up
			int index = m_boneIndex[m_ragdollAttachedObjectIndex];
			int boneFlags = GetModelPtr()->boneFlags(index);
			if ( !(boneFlags & boneMask) )
			{
				// BUGBUG: The attached bone is required and this call is going to skip it, so force it
				// HACKHACK: Assume the highest bit numbered bone flag is the minimum bone set
				boneMask |= GetHighestBit( boneFlags );
			}
		}
		return BaseClass::SetupBones( pBoneToWorldOut, nMaxBones, boneMask, currentTime );
	}

	virtual void BuildTransformations( CStudioHdr *hdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
	{
		CVProfScope _( "CServerRagdollAttached::SetupBones", 0, VPROF_BUDGETGROUP_CLIENT_ANIMATION, false, BUDGETFLAG_OTHER );

		if ( !hdr )
			return;

		float frac = RemapVal( gpGlobals->curtime, m_parentTime, m_parentTime+ATTACH_INTERP_TIME, 0, 1 );
		frac = clamp( frac, 0, 1 );
		// interpolate offset over some time
		Vector offset = m_vecOffset * (1-frac);

		CBaseAnimating *parent = static_cast< CBaseAnimating* >( GetMoveParent() );
		Vector worldOrigin;
		worldOrigin.Init();

		if ( parent )
		{
			contract_assert( parent != this );
			parent->SetupBones( nullptr, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

			matrix3x4_t boneToWorld;
			parent->GetCachedBoneMatrix( m_boneIndexAttached, boneToWorld );
			VectorTransform( m_attachmentPointBoneSpace, boneToWorld, worldOrigin );
		}
		BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );

		if ( parent )
		{
			int index = m_boneIndex[m_ragdollAttachedObjectIndex];
			const matrix3x4_t &matrix = GetBone( index );
			Vector ragOrigin;
			VectorTransform( m_attachmentPointRagdollSpace, matrix, ragOrigin );
			offset = worldOrigin - ragOrigin;
			// fixes culling
			SetAbsOrigin( worldOrigin );
			m_vecOffset = offset;
		}

		for ( int i = 0; i < hdr->numbones(); i++ )
		{
			if ( !( hdr->boneFlags( i ) & boneMask ) )
				continue;

			Vector pos;
			matrix3x4_t &matrix = GetBoneForWrite( i );
			MatrixGetColumn( matrix, 3, pos );
			pos += offset;
			MatrixSetColumn( pos, 3, matrix );
		}
	}
	void OnDataChanged( DataUpdateType_t updateType );
	virtual float LastBoneChangedTime() { return std::numeric_limits<float>::max(); }

	[[= ks::reflect::Net{} ]] Vector		m_attachmentPointBoneSpace;
	Vector		m_vecOffset;
	[[= ks::reflect::Net{} ]] Vector		m_attachmentPointRagdollSpace;
	[[= ks::reflect::Net{} ]] int			m_ragdollAttachedObjectIndex;
	[[= ks::reflect::Net{} ]] int			m_boneIndexAttached;
	float		m_parentTime;
	bool		m_bHasParent;
private:
	CServerRagdollAttached( const CServerRagdollAttached & );
};

EXTERN_RECV_TABLE(DT_Ragdoll_Attached);
IMPLEMENT_REFLECT_CLIENTCLASS( CServerRagdollAttached, DT_Ragdoll_Attached, CRagdollPropAttached )

void CServerRagdollAttached::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	bool bParentNow = GetMoveParent() ? true : false;
	if ( m_bHasParent != bParentNow )
	{
		if ( m_bHasParent )
		{
			m_parentTime = gpGlobals->curtime;
		}
		m_bHasParent = bParentNow;
	}
}

struct ragdoll_remember_t
{
	CBaseEntity	*ragdoll;
	int				tickCount;
};

struct ragdoll_memory_list_t
{
	std::vector< ragdoll_remember_t >	list;

	int tickCount;

	void Update()
	{
		if ( tickCount > gpGlobals->tickcount )
		{
			list.clear();
			return;
		}

		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			if ( list[i].tickCount != gpGlobals->tickcount )
			{
				ks::swap_remove( list, i );
			}
		}
	}

	bool IsInList( CBaseEntity *pRagdoll )
	{
		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			if ( list[i].ragdoll == pRagdoll )
				return true;
		}

		return false;
	}
	void AddToList( CBaseEntity *pRagdoll )
	{
		Update();
		int index = std::ssize( list ); list.emplace_back();
		list[index].ragdoll = pRagdoll;
		list[index].tickCount = gpGlobals->tickcount;
	}
};

static ragdoll_memory_list_t gRagdolls;

void NoteRagdollCreationTick( CBaseEntity *pRagdoll )
{
	gRagdolls.AddToList( pRagdoll );
}

// returns true if the ragdoll was created on this tick
bool WasRagdollCreatedOnCurrentTick( CBaseEntity *pRagdoll )
{
	gRagdolls.Update();
	return gRagdolls.IsInList( pRagdoll );
}

class CBaseEntity;

using namespace ::ks::reflect;

class CRagdollManager : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_RagdollManager", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CRagdollManager, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CRagdollManager();

// CBaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:

	[[= ks::reflect::Net{} ]] int		m_iCurrentMaxRagdollCount;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CRagdollManager, DT_RagdollManager, CRagdollManager )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CRagdollManager::CRagdollManager()
{
	m_iCurrentMaxRagdollCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CRagdollManager::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	s_RagdollLRU.SetMaxRagdollCount( m_iCurrentMaxRagdollCount );
}

class CBaseAnimating;
class CBaseEntity;
class CClientRagdoll;

using namespace ::ks::reflect;

//CLIENT
//SERVER

CRagdollLowViolenceManager g_RagdollLVManager;

void CRagdollLowViolenceManager::SetLowViolence( const char *pMapName )
{
	// set the value using the engine's low violence settings
	m_bLowViolence = UTIL_IsLowViolence();

	// Turn the low violence ragdoll stuff off if we're in the HL2 Citadel maps because
	// the player has the super gravity gun and fading ragdolls will break things.
	if( hl2_episodic.GetBool() )
	{
		if ( V_stricmp( pMapName, "ep1_citadel_02" ) == 0 ||
			V_stricmp( pMapName, "ep1_citadel_02b" ) == 0 ||
			V_stricmp( pMapName, "ep1_citadel_03" ) == 0 )
		{
			m_bLowViolence = false;
		}
	}
	else
	{
		if ( V_stricmp( pMapName, "d3_citadel_03" ) == 0 ||
			V_stricmp( pMapName, "d3_citadel_04" ) == 0 ||
			V_stricmp( pMapName, "d3_citadel_05" ) == 0 ||
			V_stricmp( pMapName, "d3_breen_01" ) == 0 )
		{
			m_bLowViolence = false;
		}
	}
}

// A simple cache to store the ragdoll's data after it has been parsed.  Avoid re-parsing on every create
struct cache_ragdollsolid_t
{
	objectparams_t params;
	int surfacePropIndex;
	short boneIndex;
	short collideIndex;
};

struct cache_ragdollconstraint_t
{
	constraint_axislimit_t axes[3];
	matrix3x4_t constraintToAttached;
	short parentIndex;
	short childIndex;
};

// store the ragdoll as a single allocation header, then solids array, then constraints array linearly in memory
struct cache_ragdoll_t
{
	IPhysicsCollisionSet *pCollisionSet;
	ragdollanimatedfriction_t animfriction;
	short solidCount;
	short constraintCount;

	const cache_ragdollsolid_t *GetSolids() { return (cache_ragdollsolid_t *)(this+1); }
	const cache_ragdollconstraint_t *GetConstraints() { return (cache_ragdollconstraint_t *)(GetSolids()+solidCount); }
};

cache_ragdoll_t *CreateRagdollCache( vcollide_t *pOutput, cache_ragdollsolid_t *pSolids, cache_ragdollconstraint_t *pConstraints, cache_ragdoll_t *pRagdoll )
{
	std::size_t memSize = sizeof(cache_ragdoll_t);
	size_t solidSize = sizeof(cache_ragdollsolid_t) * pRagdoll->solidCount;
	size_t constraintSize = sizeof(cache_ragdollconstraint_t) * pRagdoll->constraintCount;

	cache_ragdoll_t *pMem = (cache_ragdoll_t *)physcollision->VCollideAllocUserData( pOutput, memSize + solidSize + constraintSize );
	V_memcpy( pMem, pRagdoll, sizeof(*pMem) );
	V_memcpy( (void *)pMem->GetSolids(), pSolids, solidSize );
	V_memcpy( (void *)pMem->GetConstraints(), pConstraints, constraintSize );
	return pMem;
}

// OPTIMIZE: Slow, hopefully this is only called by save/load
void RagdollSetupAnimatedFriction( IPhysicsEnvironment *pPhysEnv, ragdoll_t *ragdoll, int iModelIndex )
{
	vcollide_t* pCollide = modelinfo->GetVCollide( iModelIndex );

	if ( pCollide )
	{
		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );

		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();

			if ( !strcmpi( pBlock, "animatedfriction") ) 
			{
				pParse->ParseRagdollAnimatedFriction( &ragdoll->animfriction, nullptr );
			}
			else
			{
				pParse->SkipBlock();
			}
		}

		physcollision->VPhysicsKeyParserDestroy( pParse );
	}
}

static void RagdollAddSolids( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params, cache_ragdollsolid_t *pSolids, int solidCount, const cache_ragdollconstraint_t *pConstraints, int constraintCount )
{
	const char *pszName = params.pStudioHdr->pszName();
	Vector position;
	matrix3x4_t xform;
	// init parent index
	for ( int i = 0; i < solidCount; i++ )
	{
		ragdoll.list[i].parentIndex = -1;
	}
	// now set from constraints
	for ( int i = 0; i < constraintCount; i++ )
	{
		// save parent index
		ragdoll.list[pConstraints[i].childIndex].parentIndex = pConstraints[i].parentIndex;
		MatrixGetColumn( pConstraints[i].constraintToAttached, 3, ragdoll.list[pConstraints[i].childIndex].originParentSpace );
	}

	// now setup the solids, using parent indices
	for ( int i = 0; i < solidCount; i++ )
	{
		if ( params.fixedConstraints )
		{
			pSolids[i].params.mass = 1000.f;
		}

		ragdoll.boneIndex[i] = pSolids[i].boneIndex;
		pSolids[i].params.pName = pszName;
		pSolids[i].params.pGameData = params.pGameData;
		ragdoll.list[i].pObject = pPhysEnv->CreatePolyObject( params.pCollide->solids[pSolids[i].collideIndex], pSolids[i].surfacePropIndex, vec3_origin, vec3_angle, &pSolids[i].params );
		ragdoll.list[i].pObject->SetGameIndex( i );
		int parentIndex = ragdoll.list[i].parentIndex;
		MatrixCopy( params.pCurrentBones[ragdoll.boneIndex[i]], xform );
		if ( parentIndex >= 0 )
		{
			contract_assert( parentIndex<i );
			ragdoll.list[parentIndex].pObject->LocalToWorld( &position, ragdoll.list[i].originParentSpace );
			MatrixSetColumn( position, 3, xform );
		}
		ragdoll.list[i].pObject->SetPositionMatrix( xform, true );

		PhysSetGameFlags( ragdoll.list[i].pObject, FVPHYSICS_PART_OF_RAGDOLL );

	}
	ragdoll.listCount = solidCount;
}

static void RagdollAddConstraints( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params, const cache_ragdollconstraint_t *pConstraints, int constraintCount )
{
	constraint_ragdollparams_t constraint;
	for ( int i = 0; i < constraintCount; i++ )
	{
		constraint.Defaults();
		V_memcpy( constraint.axes, pConstraints[i].axes, sizeof(constraint.axes) );
		if ( params.jointFrictionScale > 0 )
		{
			for ( int k = 0; k < 3; k++ )
			{
				constraint.axes[k].torque *= params.jointFrictionScale;
			}
		}
		int parentIndex = pConstraints[i].parentIndex;
		int childIndex = pConstraints[i].childIndex;
		constraint.childIndex = childIndex;
		constraint.parentIndex = parentIndex;
		constraint.useClockwiseRotations = true;
		constraint.constraintToAttached = pConstraints[i].constraintToAttached;
		// UNDONE: We could transform the constraint limit axes relative to the bone space
		// using this data.  Do we need that feature?
		SetIdentityMatrix( constraint.constraintToReference );

		if ( params.fixedConstraints )
		{
			// Makes the ragdoll a statue...
			constraint_fixedparams_t fixed;
			fixed.Defaults();
			fixed.InitWithCurrentObjectState( ragdoll.list[childIndex].pObject, ragdoll.list[constraint.parentIndex].pObject );
			fixed.constraint.Defaults();
			ragdoll.list[childIndex].pConstraint = pPhysEnv->CreateFixedConstraint( ragdoll.list[childIndex].pObject, ragdoll.list[parentIndex].pObject, ragdoll.pGroup, fixed );
		}
		else
		{
			ragdoll.list[childIndex].pConstraint = pPhysEnv->CreateRagdollConstraint( ragdoll.list[childIndex].pObject, ragdoll.list[parentIndex].pObject, ragdoll.pGroup, constraint );
		}
	}
}

static cache_ragdoll_t *ParseRagdollIntoCache( CStudioHdr *pStudioHdr, vcollide_t *pCollide, int modelIndex )
{
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	cache_ragdollsolid_t solidList[RAGDOLL_MAX_ELEMENTS];
	cache_ragdollconstraint_t constraintList[RAGDOLL_MAX_ELEMENTS];
	solid_t solid;
	int constraintCount = 0;
	int solidCount = 0;
	cache_ragdoll_t cache;
	V_memset( &cache, 0, sizeof(cache) );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			pParse->ParseSolid( &solid, &g_SolidSetup );
			cache_ragdollsolid_t *pSolid = &solidList[solidCount];
			pSolid->boneIndex = Studio_BoneIndexByName( pStudioHdr, solid.name );
			if ( pSolid->boneIndex >= 0 )
			{
				pSolid->collideIndex = solid.index;
				pSolid->surfacePropIndex = physprops->GetSurfaceIndex( solid.surfaceprop );
				if ( pSolid->surfacePropIndex < 0 )
				{
					pSolid->surfacePropIndex = physprops->GetSurfaceIndex( "default" );
				}
				pSolid->params = solid.params;
				pSolid->params.enableCollisions = false;
				solidCount++;
			}
			else
			{
				Msg( "ParseRagdollIntoCache:  Couldn't Lookup Bone %s\n", solid.name );
			}
		}
		else if ( !strcmpi( pBlock, "ragdollconstraint" ) )
		{
			constraint_ragdollparams_t constraint;
			pParse->ParseRagdollConstraint( &constraint, nullptr );
			if( constraint.childIndex != constraint.parentIndex && constraint.childIndex >= 0 && constraint.parentIndex >= 0)
			{
				cache_ragdollconstraint_t *pOut = &constraintList[constraintCount];
				constraintCount++;
				V_memcpy( pOut->axes, constraint.axes, sizeof(constraint.axes) );
				pOut->parentIndex = constraint.parentIndex;
				pOut->childIndex = constraint.childIndex;
				Studio_CalcBoneToBoneTransform( pStudioHdr, solidList[constraint.childIndex].boneIndex, solidList[constraint.parentIndex].boneIndex, pOut->constraintToAttached );
			}
		}
		else if ( !strcmpi( pBlock, "collisionrules" ) )
		{
			ragdollcollisionrules_t rules;
			IPhysicsCollisionSet *pSet = physics->FindOrCreateCollisionSet( modelIndex, pCollide->solidCount );
			rules.Defaults(physics, pSet);
			pParse->ParseCollisionRules( &rules, nullptr );
			cache.pCollisionSet = rules.pCollisionSet;
		}
		else if ( !strcmpi( pBlock, "animatedfriction") ) 
		{
			pParse->ParseRagdollAnimatedFriction( &cache.animfriction, nullptr );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );
	cache.solidCount = solidCount;
	cache.constraintCount = constraintCount;
	return CreateRagdollCache( pCollide, solidList, constraintList, &cache );
}

static void RagdollCreateObjects( IPhysicsEnvironment *pPhysEnv, ragdoll_t &ragdoll, const ragdollparams_t &params )
{
	ragdoll.listCount = 0;
	ragdoll.pGroup = nullptr;
	ragdoll.allowStretch = params.allowStretch;
	std::memset( ragdoll.list, 0, sizeof(ragdoll.list) );
	memset( &ragdoll.animfriction, 0, sizeof(ragdoll.animfriction) );
	
	if ( !params.pCollide )
	{
		Warning( "Ragdoll has no pCollide!" );
		contract_assert( false );
		return;
	}

	if ( params.pCollide->solidCount > RAGDOLL_MAX_ELEMENTS )
	{
		Warning( "Ragdoll solid count %d exceeds maximum limit of %d - Ragdoll not created", params.pCollide->solidCount, RAGDOLL_MAX_ELEMENTS );
		contract_assert( false );
		return;
	}

	cache_ragdoll_t *pCache = (cache_ragdoll_t *)params.pCollide->pUserData;
	if ( !pCache )
	{
		pCache = ParseRagdollIntoCache(params.pStudioHdr, params.pCollide, params.modelIndex);
	}

	constraint_groupparams_t group;
	group.Defaults();
	ragdoll.pGroup = pPhysEnv->CreateConstraintGroup( group );
 
	RagdollAddSolids( pPhysEnv, ragdoll, params, const_cast<cache_ragdollsolid_t *>(pCache->GetSolids()), pCache->solidCount, pCache->GetConstraints(), pCache->constraintCount );
	RagdollAddConstraints( pPhysEnv, ragdoll, params, pCache->GetConstraints(), pCache->constraintCount );
}

void RagdollSetupCollisions( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex )
{
	contract_assert( pCollide );
	if (!pCollide)
		return;

	IPhysicsCollisionSet *pSet = physics->FindCollisionSet( modelIndex );
	if ( !pSet )
	{
		pSet = physics->FindOrCreateCollisionSet( modelIndex, pCollide->solidCount );
		if ( !pSet )
			return;

		bool bFoundRules = false;

		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();
			if ( !strcmpi( pBlock, "collisionrules" ) )
			{
				ragdollcollisionrules_t rules;
				rules.Defaults(physics, pSet);
				pParse->ParseCollisionRules( &rules, nullptr );
				contract_assert( rules.pCollisionSet == pSet );
				bFoundRules = true;
			}
			else
			{
				pParse->SkipBlock();
			}
		}
		physcollision->VPhysicsKeyParserDestroy( pParse );

		if ( !bFoundRules )
		{
			// these are the default rules - each piece collides with everything
			// except immediate parent/constrained object.
			int i;
			for ( i = 0; i < ragdoll.listCount; i++ )
			{
				for ( int j = i+1; j < ragdoll.listCount; j++ )
				{
					pSet->EnableCollisions( i, j );
				}
			}
			for ( i = 0; i < ragdoll.listCount; i++ )
			{
  				int parent = ragdoll.list[i].parentIndex;
				if ( parent >= 0 )
				{
  					contract_assert( ragdoll.list[i].pObject );
  					contract_assert( ragdoll.list[i].pConstraint );
					pSet->DisableCollisions( i, parent );
				}
 			}
		}
	}
}

void RagdollActivate( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex, bool bForceWake )
{
	RagdollSetupCollisions( ragdoll, pCollide, modelIndex );

	for ( int i = 0; i < ragdoll.listCount; i++ )
	{

		PhysSetGameFlags( ragdoll.list[i].pObject, FVPHYSICS_MULTIOBJECT_ENTITY );
		// now that the relationships are set, activate the collision system
		ragdoll.list[i].pObject->EnableCollisions( true );

		if ( bForceWake == true )
		{
			ragdoll.list[i].pObject->Wake();
		}
	}
	if ( ragdoll.pGroup )
	{
		// NOTE: This also wakes the objects
		ragdoll.pGroup->Activate();
		// so if we didn't want that, we'll need to put them back to sleep here
		if ( !bForceWake )
		{
			for ( int i = 0; i < ragdoll.listCount; i++ )
			{
				ragdoll.list[i].pObject->Sleep();
			}

		}
	}
}

bool RagdollCreate( ragdoll_t &ragdoll, const ragdollparams_t &params, IPhysicsEnvironment *pPhysEnv )
{
	RagdollCreateObjects( pPhysEnv, ragdoll, params );

	if ( !ragdoll.listCount )
		return false;

	int forceBone = params.forceBoneIndex;
	
	int i;
	float totalMass = 0;
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		totalMass += ragdoll.list[i].pObject->GetMass();
	}
	totalMass = std::max<std::common_type_t<float, int>>(totalMass,1);

	// apply force to the model
	Vector nudgeForce = params.forceVector;
	Vector forcePosition = params.forcePosition;
	// UNDONE: Test scaling the force by total mass on all bones
	
	// UNDONE: forcebone can be out of range when a body part breaks off - it uses the shared force bone from the original model
	// UNDONE: Remap this?
	if ( forceBone >= 0 && forceBone < ragdoll.listCount )
	{
		ragdoll.list[forceBone].pObject->ApplyForceCenter( nudgeForce );
		//nudgeForce *= 0.5;
		ragdoll.list[forceBone].pObject->GetPosition( &forcePosition, nullptr );
	}
	if ( forcePosition != vec3_origin )
	{
		for ( i = 0; i < ragdoll.listCount; i++ )
		{
			if ( forceBone != i )
			{
				float scale = ragdoll.list[i].pObject->GetMass() / totalMass;
				ragdoll.list[i].pObject->ApplyForceOffset( scale * nudgeForce, forcePosition );
			}
		}
	}
	return true;
}

void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pPrevBones, const matrix3x4_t *pCurrentBones, float dt )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector velocity;
		AngularImpulse angVel;
		int boneIndex = ragdoll.boneIndex[i];
		CalcBoneDerivatives( velocity, angVel, pPrevBones[boneIndex], pCurrentBones[boneIndex], dt );
		
		AngularImpulse localAngVelocity;

		// Angular velocity is always applied in local space in vphysics
		ragdoll.list[i].pObject->WorldToLocalVector( &localAngVelocity, angVel );
		ragdoll.list[i].pObject->AddVelocity( &velocity, &localAngVelocity );
	}
}

void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pBoneToWorld )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		matrix3x4_t inverse;
		MatrixInvert( pBoneToWorld[i], inverse );
		Quaternion q;
		Vector pos;
		MatrixAngles( inverse, q, pos );

		Vector velocity;
		AngularImpulse angVel;
		float flSpin;

		Vector localVelocity;
		AngularImpulse localAngVelocity;

		QuaternionAxisAngle( q, localAngVelocity, flSpin );
		localAngVelocity *= flSpin;
		localVelocity = pos;

		// move those bone-local coords back to world space using the ragdoll transform
		ragdoll.list[i].pObject->LocalToWorldVector( &velocity, localVelocity );

		ragdoll.list[i].pObject->AddVelocity( &velocity, &localAngVelocity );
	}
}

void RagdollDestroy( ragdoll_t &ragdoll )
{
	if ( !ragdoll.listCount )
		return;

	int i;
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		physenv->DestroyConstraint( ragdoll.list[i].pConstraint );
		ragdoll.list[i].pConstraint = nullptr;
	}
	for ( i = 0; i < ragdoll.listCount; i++ )
	{
		// during level transitions these can get temporarily loaded without physics objects
		// purely for the purpose of testing for PVS of transition.  If they fail they get
		// deleted before the physics objects are loaded.  The list count will be nonzero
		// since that is saved separately.
		if ( ragdoll.list[i].pObject )
		{
			physenv->DestroyObject( ragdoll.list[i].pObject );
		}
		ragdoll.list[i].pObject = nullptr;
	}
	physenv->DestroyConstraintGroup( ragdoll.pGroup );
	ragdoll.pGroup = nullptr;
	ragdoll.listCount = 0;
}

// Parse the ragdoll and obtain the mapping from each physics element index to a bone index
// returns num phys elements
int RagdollExtractBoneIndices( int *boneIndexOut, CStudioHdr *pStudioHdr, vcollide_t *pCollide )
{
	int elementCount = 0;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t solid;
			pParse->ParseSolid( &solid, nullptr );
			if ( elementCount < RAGDOLL_MAX_ELEMENTS )
			{
				boneIndexOut[elementCount] = Studio_BoneIndexByName( pStudioHdr, solid.name );
				elementCount++;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	return elementCount;
}

bool RagdollGetBoneMatrix( const ragdoll_t &ragdoll, CBoneAccessor &pBoneToWorld, int objectIndex )
{
	int boneIndex = ragdoll.boneIndex[objectIndex];
	if ( boneIndex < 0 )
		return false;

	const ragdollelement_t &element = ragdoll.list[objectIndex];

	// during restore if a model has changed since the file was saved, this could be NULL
	if ( !element.pObject )
		return false;
	element.pObject->GetPositionMatrix( &pBoneToWorld.GetBoneForWrite( boneIndex ) );
	if ( element.parentIndex >= 0 && !ragdoll.allowStretch )
	{
		// overwrite the position from physics to force rigid attachment
		// NOTE: On the client we actually override this with the proper parent bone in each LOD
		int parentBoneIndex = ragdoll.boneIndex[element.parentIndex];
		Vector out;
		VectorTransform( element.originParentSpace, pBoneToWorld.GetBone( parentBoneIndex ), out );
		MatrixSetColumn( out, 3, pBoneToWorld.GetBoneForWrite( boneIndex ) );
	}
	return true;
}

void RagdollComputeExactBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs )
{
	outMins = origin;
	outMaxs = origin;

	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector mins, maxs;
		IPhysicsObject *pObject = ragdoll.list[i].pObject;
		Vector objectOrg;
		QAngle objectAng;
		pObject->GetPosition( &objectOrg, &objectAng );
		physcollision->CollideGetAABB( &mins, &maxs, pObject->GetCollide(), objectOrg, objectAng );
		for ( int j = 0; j < 3; j++ )
		{
			if ( mins[j] < outMins[j] )
			{
				outMins[j] = mins[j];
			}
			if ( maxs[j] > outMaxs[j] )
			{
				outMaxs[j] = maxs[j];
			}
		}
	}
}

void RagdollComputeApproximateBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs )
{
	Vector mins, maxs;
	ClearBounds(mins,maxs);
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		Vector objectOrg;
		ragdoll.list[i].pObject->GetPosition( &objectOrg, nullptr );
		float radius = physcollision->CollideGetRadius( ragdoll.list[i].pObject->GetCollide() );
		for ( int k = 0; k < 3; k++ )
		{
			float ext = objectOrg[k] + radius;
			maxs[k] = fpmax( maxs[k], ext );
			ext = objectOrg[k] - radius;
			mins[k] = fpmin( mins[k], ext );
		}
	}
	outMins = mins;
	outMaxs = maxs;
}

bool RagdollIsAsleep( const ragdoll_t &ragdoll )
{
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		if ( ragdoll.list[i].pObject && !ragdoll.list[i].pObject->IsAsleep() )
			return false;
	}

	return true;
}

void RagdollSolveSeparation( ragdoll_t &ragdoll, CBaseEntity *pEntity )
{
	byte needsFix[256];
	int fixCount = 0;
	contract_assert( ragdoll.listCount<=std::size( needsFix ) );
	for ( int i = 0; i < ragdoll.listCount; i++ )
	{
		needsFix[i] = 0;
		const ragdollelement_t &element = ragdoll.list[i];
		if ( element.pConstraint && element.parentIndex >= 0 )
		{
			Vector start, target;
			element.pObject->GetPosition( &start, nullptr );
			ragdoll.list[element.parentIndex].pObject->LocalToWorld( &target, element.originParentSpace );
			if ( needsFix[element.parentIndex] )
			{
				needsFix[i] = 1;
				++fixCount;
				continue;
			}
			Vector dir = target-start;
			if ( dir.LengthSqr() > 1.0f )
			{
				// this fixes a bug in ep2 with antlion grubs, but causes problems in TF2 - revisit, but disable for TF now
				// heuristic: guess that anything separated and small mass ratio is in some state that's 
				// keeping the solver from fixing it
				float mass = element.pObject->GetMass();
				float massParent = ragdoll.list[element.parentIndex].pObject->GetMass();

				if ( mass*2.0f < massParent )
				{
					// if this is <0.5 mass of parent and still separated it's attached to something heavy or 
					// in a bad state
					needsFix[i] = 1;
					++fixCount;
					continue;
				}

				if ( PhysHasContactWithOtherInDirection(element.pObject, dir) )
				{
					Ray_t ray;
					trace_t tr;
					ray.Init( target, start );
					UTIL_TraceRay( ray, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tr );
					if ( tr.DidHit() )
					{
						needsFix[i] = 1;
						++fixCount;
					}
				}
			}
		}
	}

	if ( fixCount )
	{
		for ( int i = 0; i < ragdoll.listCount; i++ )
		{
			if ( !needsFix[i] )
				continue;

			const ragdollelement_t &element = ragdoll.list[i];
			Vector target, velocity;
			ragdoll.list[element.parentIndex].pObject->LocalToWorld( &target, element.originParentSpace );
			ragdoll.list[element.parentIndex].pObject->GetVelocityAtPoint( target, &velocity );
			matrix3x4_t xform;
			element.pObject->GetPositionMatrix( &xform );
			MatrixSetColumn( target, 3, xform );
			element.pObject->SetPositionMatrix( xform, true );
			element.pObject->SetVelocity( &velocity, &vec3_origin );
		}
		DevMsg(2, "TICK:%5d:Ragdoll separation count: %d\n", gpGlobals->tickcount, fixCount );
	}
	else
	{
		ragdoll.pGroup->ClearErrorState();
	}
}

//-----------------------------------------------------------------------------
// LRU
//-----------------------------------------------------------------------------
ConVar g_ragdoll_maxcount(side_registry(), "g_ragdoll_maxcount", "8", FCVAR_REPLICATED );
ConVar g_debug_ragdoll_removal(side_registry(), "g_debug_ragdoll_removal", "0", FCVAR_REPLICATED |FCVAR_CHEAT );

CRagdollLRURetirement s_RagdollLRU( "CRagdollLRURetirement" );

void CRagdollLRURetirement::LevelInitPreEntity( void )
{
	m_iMaxRagdolls = -1;
	m_LRUImportantRagdolls.clear();
	m_LRU.clear();
}

bool ShouldRemoveThisRagdoll( CBaseAnimating *pRagdoll )
{
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		return true;
	}

	/* we no longer ignore enemies just because they are on fire -- a ragdoll in front of me
	   is always a higher priority for retention than a flaming zombie behind me. At the 
	   time I put this in, the ragdolls do clean up their own effects if culled via SUB_Remove().
	   If you're encountering trouble with ragdolls leaving effects behind, try renabling the code below.
    /////////////////////
	//Just ignore it until we're done burning/dissolving.
	if ( pRagdoll->GetEffectEntity() )
		return false;
	*/

	Vector vMins, vMaxs;
		
	Vector origin = pRagdoll->m_pRagdoll->GetRagdollOrigin();
	pRagdoll->m_pRagdoll->GetRagdollBounds( vMins, vMaxs );

	if( engine->IsBoxInViewCluster( vMins + origin, vMaxs + origin) == false )
	{
		if ( g_debug_ragdoll_removal.GetBool() )
		{
			debugoverlay->AddBoxOverlay( origin, vMins, vMaxs, QAngle( 0, 0, 0 ), 0, 255, 0, 16, 5 );
			debugoverlay->AddLineOverlay( origin, origin + Vector( 0, 0, 64 ), 0, 255, 0, true, 5 );
		}

		return true;
	}
	else if( engine->CullBox( vMins + origin, vMaxs + origin ) == true )
	{
		if ( g_debug_ragdoll_removal.GetBool() )
		{
			debugoverlay->AddBoxOverlay( origin, vMins, vMaxs, QAngle( 0, 0, 0 ), 0, 0, 255, 16, 5 );
			debugoverlay->AddLineOverlay( origin, origin + Vector( 0, 0, 64 ), 0, 0, 255, true, 5 );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Cull stale ragdolls. There is an ifdef here: one version for episodic, 
// one for everything else.
//-----------------------------------------------------------------------------

void CRagdollLRURetirement::Update( float frametime ) // Non-episodic version
{
	CVProfScope _( "CRagdollLRURetirement::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	int iMaxRagdollCount = m_iMaxRagdolls;

	if ( iMaxRagdollCount == -1 )
	{
		iMaxRagdollCount = g_ragdoll_maxcount.GetInt();
	}

	// fade them all for the low violence version
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		iMaxRagdollCount = 0;
	}
	m_iRagdollCount = 0;
	m_iSimulatedRagdollCount = 0;

	// remove ragdolls with a forced retire time
	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		CBaseAnimating *pRagdoll = it->Get();

		//Just ignore it until we're done burning/dissolving.
		if ( pRagdoll && pRagdoll->GetEffectEntity() )
		{
			++it;
			continue;
		}

		// ignore if it's not time to force retire this ragdoll
		if ( it->GetForcedRetireTime() == 0.0f || gpGlobals->curtime < it->GetForcedRetireTime() )
		{
			++it;
			continue;
		}

		//Msg(" Removing ragdoll %s due to forced retire time of %f (now = %f)\n", pRagdoll->GetModelName(), it->GetForcedRetireTime(), gpGlobals->curtime );

		pRagdoll->SUB_Remove();
		it = m_LRU.erase( it );
	}

	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		CBaseAnimating *pRagdoll = it->Get();
		if ( pRagdoll )
		{
			m_iRagdollCount++;
			IPhysicsObject *pObject = pRagdoll->VPhysicsGetObject();
			if (pObject && !pObject->IsAsleep())
			{
				m_iSimulatedRagdollCount++;
			}
			if ( (int)m_LRU.size() > iMaxRagdollCount )
			{
				//Found one, we're done.
				if ( ShouldRemoveThisRagdoll( pRagdoll ) == true )
				{
					pRagdoll->SUB_Remove();

					m_LRU.erase( it );
					return;
				}
			}
			++it;
		}
		else 
		{
			it = m_LRU.erase( it );
		}
	}

	//////////////////////////////
	///   ORIGINAL ALGORITHM   ///
	//////////////////////////////
	// not episodic -- this is the original mechanism

	for ( auto it = m_LRU.begin(); it != m_LRU.end(); )
	{
		if ( (int)m_LRU.size() <=  iMaxRagdollCount )
			break;

		CBaseAnimating *pRagdoll = it->Get();

		//Just ignore it until we're done burning/dissolving.
		if ( pRagdoll && pRagdoll->GetEffectEntity() )
		{
			++it;
			continue;
		}

		pRagdoll->SUB_Remove();
		it = m_LRU.erase( it );
	}
}

//This is pretty hacky, it's only called on the server so it just calls the update method.
void CRagdollLRURetirement::FrameUpdatePostEntityThink( void )
{
	Update( 0 );
}

ConVar g_ragdoll_important_maxcount( side_registry(), "g_ragdoll_important_maxcount", "2", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Move it to the top of the LRU
//-----------------------------------------------------------------------------
void CRagdollLRURetirement::MoveToTopOfLRU( CBaseAnimating *pRagdoll, bool bImportant, float flForcedRetireTime )
{
	if ( bImportant )
	{
		m_LRUImportantRagdolls.emplace_back( pRagdoll, flForcedRetireTime );

		if ( (int)m_LRUImportantRagdolls.size() > g_ragdoll_important_maxcount.GetInt() )
		{
			CBaseAnimating *pRagdoll = m_LRUImportantRagdolls.front().Get();

			if ( pRagdoll )
			{
				pRagdoll->SUB_Remove();
				m_LRUImportantRagdolls.erase( m_LRUImportantRagdolls.begin() );
			}

		}
		return;
	}
	for ( auto it = m_LRU.begin(); it != m_LRU.end(); ++it )
	{
		if ( it->Get() == pRagdoll )
		{
			m_LRU.erase( it );
			break;
		}
	}

	m_LRU.emplace_back( pRagdoll, flForcedRetireTime );
}

//EFFECT/ENTITY TRANSFERS

//CLIENT

static constexpr auto DEFAULT_FADE_START = 2.0f;
static constexpr auto DEFAULT_MODEL_FADE_START = 1.9f;
static constexpr auto DEFAULT_MODEL_FADE_LENGTH = 0.1f;
static constexpr auto DEFAULT_FADEIN_LENGTH = 1.0f;

CEntityDissolve *DissolveEffect( CBaseAnimating *pTarget, float flTime )
{
	CEntityDissolve *pDissolve = new CEntityDissolve;

	if ( pDissolve->InitializeAsClientEntity( "sprites/blueglow1.vmt", false ) == false )
	{
		UTIL_Remove( pDissolve );
		return nullptr;
	}

	if ( pDissolve != nullptr )
	{
		pTarget->AddFlag( FL_DISSOLVING );
		pDissolve->SetParent( pTarget );
		pDissolve->OnDataChanged( DATA_UPDATE_CREATED );
		pDissolve->SetAbsOrigin( pTarget->GetAbsOrigin() );

		pDissolve->m_flStartTime = flTime;
		pDissolve->m_flFadeOutStart = DEFAULT_FADE_START;
		pDissolve->m_flFadeOutModelStart = DEFAULT_MODEL_FADE_START;
		pDissolve->m_flFadeOutModelLength = DEFAULT_MODEL_FADE_LENGTH;
		pDissolve->m_flFadeInLength = DEFAULT_FADEIN_LENGTH;
		
		pDissolve->m_nDissolveType = 0;
		pDissolve->m_flNextSparkTime = 0.0f;
		pDissolve->m_flFadeOutLength = 0.0f;
		pDissolve->m_flFadeInStart = 0.0f;

		// Let this entity know it needs to delete itself when it's done
		pDissolve->SetServerLinkState( false );
		pTarget->SetEffectEntity( pDissolve );
	}

	return pDissolve;

}

CEntityFlame *FireEffect( CBaseAnimating *pTarget, CBaseEntity *pServerFire, float *flScaleEnd, float *flTimeStart, float *flTimeEnd )
{
	CEntityFlame *pFire = new CEntityFlame;

	if ( pFire->InitializeAsClientEntity( nullptr, false ) == false )
	{
		UTIL_Remove( pFire );
		return nullptr;
	}

	if ( pFire != nullptr )
	{
		pFire->RemoveFromLeafSystem();
		
		pTarget->AddFlag( FL_ONFIRE );
		pFire->SetParent( pTarget );
		pFire->m_hEntAttached = (CBaseEntity *) pTarget;

		pFire->OnDataChanged( DATA_UPDATE_CREATED );
		pFire->SetAbsOrigin( pTarget->GetAbsOrigin() );

		//Play a sound
		CBroadcastRecipientFilter filter;
		pTarget->EmitSound( filter, pTarget->GetSoundSourceIndex(), "General.BurningFlesh" );

		pFire->SetNextClientThink( gpGlobals->curtime + 7.0f );
	}

	return pFire;
}

void CBaseAnimating::IgniteRagdoll( CBaseAnimating *pSource )
{
	CBaseEntity *pChild = pSource->GetEffectEntity();
	
	if ( pChild )
	{
		CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( pChild );
		CClientRagdoll *pRagdoll = dynamic_cast< CClientRagdoll * > ( this );

		if ( pFireChild )
		{
			pRagdoll->SetEffectEntity ( FireEffect( pRagdoll, pFireChild, nullptr, nullptr, nullptr ) );
		}
	}
}

void CBaseAnimating::TransferDissolveFrom( CBaseAnimating *pSource )
{
	CBaseEntity *pChild = pSource->GetEffectEntity();
	
	if ( pChild )
	{
		CEntityDissolve *pDissolveChild = dynamic_cast<CEntityDissolve *>( pChild );

		if ( pDissolveChild )
		{
			CClientRagdoll *pRagdoll = dynamic_cast< CClientRagdoll * > ( this );

			if ( pRagdoll )
			{
				pRagdoll->m_flEffectTime = pDissolveChild->m_flStartTime;

				CEntityDissolve *pDissolve = DissolveEffect( pRagdoll, pRagdoll->m_flEffectTime );

				if ( pDissolve )
				{
					pDissolve->SetRenderMode( pDissolveChild->GetRenderMode() );
					pDissolve->SetRenderFX( pDissolveChild->GetRenderFX() );
					pDissolve->SetRenderColor( 255, 255, 255 );
					pDissolve->SetRenderAlpha( 255 );
					pDissolveChild->SetRenderAlpha( 0 );

					pDissolve->m_vDissolverOrigin = pDissolveChild->m_vDissolverOrigin;
					pDissolve->m_nDissolveType = pDissolveChild->m_nDissolveType;

					if ( pDissolve->m_nDissolveType == ENTITY_DISSOLVE_CORE )
					{
						pDissolve->m_nMagnitude = pDissolveChild->m_nMagnitude;
						pDissolve->m_flFadeOutStart = CORE_DISSOLVE_FADE_START;
						pDissolve->m_flFadeOutModelStart = CORE_DISSOLVE_MODEL_FADE_START;
						pDissolve->m_flFadeOutModelLength = CORE_DISSOLVE_MODEL_FADE_LENGTH;
						pDissolve->m_flFadeInLength = CORE_DISSOLVE_FADEIN_LENGTH;
					}
				}
			}
		}
	}
}

//SERVER

class CBasePlayer;
class CRecipientFilter;
class CTeam;

using namespace ::ks::reflect;

static IPredictionSystem g_RecipientFilterPredictionSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRecipientFilter::CRecipientFilter()
{
	Reset();
}

CRecipientFilter::~CRecipientFilter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
void CRecipientFilter::CopyFrom( const CRecipientFilter& src )
{
	m_bReliable = src.IsReliable();
	m_bInitMessage = src.IsInitMessage();

	m_bUsingPredictionRules = src.IsUsingPredictionRules();
	m_bIgnorePredictionCull = src.IgnorePredictionCull();

	int c = src.GetRecipientCount();
	for ( int i = 0; i < c; ++i )
	{
		m_Recipients.push_back( src.GetRecipientIndex( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_Recipients.clear();
	m_bUsingPredictionRules = false;
	m_bIgnorePredictionCull = false;
}

void CRecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}

bool CRecipientFilter::IsReliable( void ) const
{
	return m_bReliable;
}

int CRecipientFilter::GetRecipientCount( void ) const
{
	return std::ssize( m_Recipients );
}

int	CRecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void CRecipientFilter::AddAllPlayers( void )
{
	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	m_Recipients.clear();
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		AddRecipient( CBasePlayer::GetLocalPlayer( hh ) );
	}
}

void CRecipientFilter::AddRecipient( CBasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// If we're predicting and this is not the first time we've predicted this sound
	//  then don't send it to the local player again.
	if ( m_bUsingPredictionRules )
	{
		contract_assert( CBasePlayer::IsLocalPlayer( player ) );
		contract_assert( prediction->InPrediction() );

		// Only add local player if this is the first time doing prediction
		if ( !g_RecipientFilterPredictionSystem.CanPredict() )
		{
			return;
		}
	}

	// Already in list
	if ( ks::find_index( m_Recipients, index ) != -1 )
		return;

	// this is a client side filter, only add the local player
	if ( !CBasePlayer::IsLocalPlayer( player ) )
		return;

	m_Recipients.push_back( index );
}

void CRecipientFilter::RemoveRecipient( CBasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// Remove it if it's in the list
	ks::erase_first( m_Recipients, index );
}

void CRecipientFilter::AddRecipientsByTeam( CTeam *team )
{
	AddAllPlayers();
}

void CRecipientFilter::RemoveRecipientsByTeam( CTeam *team )
{
	contract_assert( 0 );
}

void CRecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer( hh );
		if ( !pPlayer )
			continue;

		// only add the local player on client side
		if ( !playerbits[ pPlayer->index ] )
			continue;

		AddRecipient( pPlayer );
	}
}

void CRecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	AddAllPlayers();
}

void CRecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	AddAllPlayers();
}

void CRecipientFilter::UsePredictionRules( void )
{
	if ( m_bUsingPredictionRules )
		return;

	if ( !prediction->InPrediction() )
	{
		contract_assert( 0 );
		return;
	}

	if ( !CBasePlayer::HasAnyLocalPlayer() )
	{
		contract_assert( 0 );
		return;
	}

	m_bUsingPredictionRules = true;

	// Cull list now, if needed
	int c = GetRecipientCount();
	if ( c == 0 )
		return;

	if ( !g_RecipientFilterPredictionSystem.CanPredict() )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			RemoveRecipient( CBasePlayer::GetLocalPlayer( hh ) );
		}
	}
}

void CRecipientFilter::RemoveSplitScreenPlayers()
{
	for ( int i = GetRecipientCount() - 1; i >= 0; --i )
	{
		int idx = m_Recipients[ i ];
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( idx );
		if ( !pPlayer || !pPlayer->IsSplitScreenPlayer() )
			continue;

		m_Recipients.erase( m_Recipients.begin() + i );
	}
}

// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
void CRecipientFilter::ReplaceSplitScreenPlayersWithOwners()
{
	// coop
	if( gpGlobals->maxClients >= 2 && CBasePlayer::HasAnyLocalPlayer() ) 
	{
		bool addedHostPlayer = false;
		const int count = std::ssize( m_Recipients );
		for( int i = 0; i < count; ++i )
		{
			// If this is a split screen player
			CBasePlayer* pPlayer = UTIL_PlayerByIndex( m_Recipients[i] );
			if( pPlayer && pPlayer->IsSplitScreenPlayer() )
			{
				// Add the host player, if it exists, to the filter
				CBasePlayer* pHostPlayer = CBasePlayer::GetLocalPlayer( 0 );
				if( pHostPlayer )
				{
					m_Recipients.push_back( pHostPlayer->entindex() );
					addedHostPlayer = true;
				}
				break;
			}
		}

		if( addedHostPlayer )
		{
			// Remove all split screen players
			RemoveSplitScreenPlayers();

			// Remove duplicates
			RemoveDuplicateRecipients();
		}
	}
}
bool CRecipientFilter::IsUsingPredictionRules( void ) const
{
	return m_bUsingPredictionRules;
}

bool CRecipientFilter::IgnorePredictionCull( void ) const
{
	return m_bIgnorePredictionCull;
}

void CRecipientFilter::SetIgnorePredictionCull( bool ignore )
{
	m_bIgnorePredictionCull = ignore;
}

void CRecipientFilter::RemoveDuplicateRecipients()
{
	for( int i = 0; i < std::ssize( m_Recipients ); ++i )
	{
		int currentElem = m_Recipients[i];
		for( int j = std::ssize( m_Recipients ) - 1; j > i ; --j )
		{
			if( m_Recipients[j] == currentElem )
				ks::swap_remove( m_Recipients, j );
		}
	}
}

CLocalPlayerFilter::CLocalPlayerFilter()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		if ( CBasePlayer::GetLocalPlayer() )
		{
			AddRecipient( CBasePlayer::GetLocalPlayer() );
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: implements various common send proxies

void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	int *pInt = (int *)pOut;
	
	*pInt = pData->m_Value.m_Int - 1;
}

void RecvProxy_ShortSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	short *pInt = (short *)pOut;
	
	*pInt = pData->m_Value.m_Int - 1;
}

RecvProp RecvPropIntWithMinusOneFlag( const char *pVarName, int offset, int sizeofVar, RecvVarProxyFn proxyFn )
{
	return RecvPropInt( pVarName, offset, sizeofVar, 0, proxyFn );
}

//-----------------------------------------------------------------------------
// Purpose: Okay, so we have to queue up the actual ehandle to entity lookup for the following reason:
//  If a player has an EHandle/CHandle to an object such as a weapon, since the player is in slot 1-31, then
//  if the weapon is created and given to the player in the same frame, then the weapon won't have been
//  created at the time we parse this EHandle index, since the player is ahead of every other entity in the
//  packet (except for the world).
// So instead, we remember which ehandles need to be set and we set them after all network data has
//  been received.  Sigh.
// Input  : *pData - 
//			*pStruct - 
//			*pOut - 
//-----------------------------------------------------------------------------

void RecvProxy_IntToEHandle( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseHandle *pEHandle = (CBaseHandle*)pOut;
	
	if ( pData->m_Value.m_Int == INVALID_NETWORKED_EHANDLE_VALUE )
	{
		*pEHandle = INVALID_EHANDLE;
	}
	else
	{
		int iEntity = pData->m_Value.m_Int & ((1 << MAX_EDICT_BITS) - 1);
		int iSerialNum = pData->m_Value.m_Int >> MAX_EDICT_BITS;
		
		pEHandle->Init( iEntity, iSerialNum );
	}
}

RecvProp RecvPropEHandle(
	const char *pVarName,
	int offset, 
	int sizeofVar,
	RecvVarProxyFn proxyFn )
{
	return RecvPropInt( pVarName, offset, sizeofVar, 0, proxyFn );
}

RecvProp RecvPropBool(
	const char *pVarName,
	int offset, 
	int sizeofVar )
{
	contract_assert( sizeofVar == sizeof( bool ) );
	return RecvPropInt( pVarName, offset, sizeofVar );
}

//-----------------------------------------------------------------------------
// Moveparent receive proxies
//-----------------------------------------------------------------------------
void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CHandle<CBaseEntity> *pHandle = (CHandle<CBaseEntity>*)pOut;
	RecvProxy_IntToEHandle( pData, pStruct, (CBaseHandle*)pHandle );
}

void RecvProxy_InterpolationAmountChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// m_bSimulatedEveryTick & m_bAnimatedEveryTick are boolean
	if ( *((bool*)pOut) != (pData->m_Value.m_Int != 0) )
	{
		// Have the regular proxy store the data.
		RecvProxy_Int32ToInt8( pData, pStruct, pOut );

		CBaseEntity *pEntity = (CBaseEntity *) pStruct;
		pEntity->Interp_UpdateInterpolationAmounts( pEntity->GetVarMapping() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Decodes a time value
// Input  : *pStruct - ( CBaseEntity * ) used to flag animtime is changine
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
static void RecvProxy_Time( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	float	t;
	float	clock_base;
	float	offset;

	// Get msec offset
	offset	= ( float )pData->m_Value.m_Int / 1000.0f;

	// Get base
	clock_base = std::floor( engine->GetLastTimeStamp() );

	// Add together and clamp to msec precision
	t = ClampToMsec( clock_base + offset );

	// Store decoded value
	*( float * )pOut = t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVarName - 
//			sizeofVar - 
// Output : RecvProp
//-----------------------------------------------------------------------------
RecvProp RecvPropTime(
	const char *pVarName,
	int offset, 
	int sizeofVar/*=kSizeofIgnore*/ )
{
//	return RecvPropInt( pVarName, offset, sizeofVar, 0, RecvProxy_Time );
	return RecvPropFloat( pVarName, offset, sizeofVar );
};

//-----------------------------------------------------------------------------
// Purpose: Okay, so we have to queue up the actual ehandle to entity lookup for the following reason:
//  If a player has an EHandle/CHandle to an object such as a weapon, since the player is in slot 1-31, then
//  if the weapon is created and given to the player in the same frame, then the weapon won't have been
//  created at the time we parse this EHandle index, since the player is ahead of every other entity in the
//  packet (except for the world).
// So instead, we remember which ehandles need to be set and we set them after all network data has
//  been received.  Sigh.
// Input  : *pData - 
//			*pStruct - 
//			*pOut - 
//-----------------------------------------------------------------------------

// Purpose: Class to help with rendering CMDLs & CMergedMDLs to textures.

extern IRenderToRTHelper *g_pRenderToRTHelper;

// uncomment these to write out VTFs of the inventory icons with no lighting to be used for the "default icon images"
//#define WRITE_OUT_VTF
//#define ICON_VTF_BASE_PATH "d:/temp"

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CRenderToRTHelper s_RenderToRTHelper;
static InterfaceReg s_Reg_CRenderToRTHelper_IRenderToRTHelper( side_registry(), []() -> void * { return static_cast<IRenderToRTHelper *>( &s_RenderToRTHelper ); }, RENDER_TO_RT_HELPER_INTERFACE_VERSION );;

CRenderToRTHelper::CRenderToRTHelper()
	: m_pCurrentObjectToRender( nullptr )
	, m_pPixelsReadEvent( nullptr )
	, m_pRenderTarget( nullptr )
{
}

CRenderToRTHelper::~CRenderToRTHelper()
{
}

bool ProcessRenderToRTHelper()
{
	return ( g_pRenderToRTHelper != nullptr ) ? g_pRenderToRTHelper->Process() : false;
}

bool CRenderToRTHelper::Init()
{
	if ( !g_pMaterialSystem )
		return false;

	std::memset( &m_LightingState, 0, sizeof(MaterialLightingState_t) );
	for ( int i = 0; i < 6; ++i )
	{
		m_LightingState.m_vecAmbientCube[i].Init( 0.15f, 0.15f, 0.15f );
	}

	//the default light is a blueish rim
	m_LightingState.m_pLocalLightDesc[0].InitDirectional( Vector( -1.0f, 0.3f, 1.0f ), Vector( 1.5f, 1.8f, 2.0f ) );
	m_LightingState.m_nLocalLightCount = 1;

	m_pRenderTarget = g_pMaterialSystem->FindTexture( "render_to_rt_helper", TEXTURE_GROUP_RENDER_TARGET );
	contract_assert( m_pRenderTarget );
	m_pRenderTarget->AddRef();

	g_pMaterialSystem->AddEndFramePriorToNextContextFunc( ::ProcessRenderToRTHelper );

	return true;
}

void CRenderToRTHelper::Shutdown( void )
{
	g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( ::ProcessRenderToRTHelper );

	if ( m_pRenderTarget )
	{
		m_pRenderTarget->Release();
		m_pRenderTarget = nullptr;
	}
}

void CRenderToRTHelper::LookAt( Camera_t& camera, const Vector &vecCenter, float flRadius, QAngle cameraAngles, Vector cameraOffset )
{
	// Compute the distance to the camera for the object based on its
	// radius and fov.

	// clamp to a reasonable range
	flRadius = clamp(flRadius, 6.0f, 20.0f);

	// since tan( fov/2 ) = f/d
	// cos( fov/2 ) = r / r' where r = sphere radius, r' = perp distance from sphere center to max extent of camera
	// d/f = r'/d' where d' is distance of camera to sphere
	// d' = r' / tan( fov/2 ) * r' = r / ( cos (fov/2) * tan( fov/2 ) ) = r / sin( fov/2 )
	float flFOVx = camera.m_flFOVX;
	flFOVx *= std::numbers::pi / 360.0f;
	Vector vecCameraOffset( cameraOffset );
	vecCameraOffset.x -= ( flRadius / std::sin( flFOVx ) );

	// now setup the camera's origin and angles
	matrix3x4_t matCameraPivot;
	matrix3x4_t offset;
	matrix3x4_t worldToCamera;
	AngleMatrix( cameraAngles, vecCenter, matCameraPivot );
	SetIdentityMatrix( offset );
	MatrixSetColumn( vecCameraOffset, 3, offset );
	ConcatTransforms( matCameraPivot, offset, worldToCamera );
	MatrixAngles( worldToCamera, camera.m_angles, camera.m_origin );
}

RenderToRTData_t *CRenderToRTHelper::CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF )
{
	RenderToRTData_t *pRenderToRTData = new RenderToRTData_t;
	pRenderToRTData->m_pObject = pObject;
	pRenderToRTData->m_pResultVTF = pResultVTF;
	pRenderToRTData->m_LightingState = m_LightingState;
	pRenderToRTData->m_cameraAngles = QAngle( 0.0f, -150.0f, 0.0f );
	pRenderToRTData->m_cameraOffset = Vector( 0.0f, 1.0f, -1.0f );
	pRenderToRTData->m_cameraFOV = 20.0f;
	pRenderToRTData->m_bUsingExplicitModelCameraPosAnglesFromAttachment = false;
	SetIdentityMatrix( pRenderToRTData->m_rootToWorld );
	pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_CREATED;

	m_objectsToRender.push_back( pRenderToRTData );
	
	return pRenderToRTData;
}

void CRenderToRTHelper::DestroyRenderToRTData( RenderToRTData_t *pRenderToRTData )
{
	if ( pRenderToRTData->m_stage == RENDER_TO_RT_STAGE_WAITING_FOR_READ_BACK && m_pPixelsReadEvent )
	{
		m_pPixelsReadEvent->Wait();
		delete m_pPixelsReadEvent;
		m_pPixelsReadEvent = nullptr;
	}

	if ( m_pCurrentObjectToRender == pRenderToRTData )
	{
		m_pCurrentObjectToRender = nullptr;
	}

	pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_UNDEFINED;

	ks::erase_first( m_objectsToRender, pRenderToRTData );
	delete pRenderToRTData;
}

void CRenderToRTHelper::StartRenderToRT( RenderToRTData_t *pRenderToRTData )
{
	if ( pRenderToRTData->m_stage == RENDER_TO_RT_STAGE_CREATED )
	{
		// update camera here, because m_cameraAngles may have been changed
		pRenderToRTData->m_Camera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 1.0f, 1000.0f, pRenderToRTData->m_cameraFOV, 1.0f );

		if ( !pRenderToRTData->m_bUsingExplicitModelCameraPosAnglesFromAttachment )
		{
			float flRadius;
			Vector vecCenter;
			pRenderToRTData->m_pObject->GetBoundingSphere( vecCenter, flRadius );
			LookAt( pRenderToRTData->m_Camera, vecCenter, flRadius, pRenderToRTData->m_cameraAngles, pRenderToRTData->m_cameraOffset );
		}
		else
		{
			pRenderToRTData->m_Camera.InitViewParameters( pRenderToRTData->m_cameraOffset, pRenderToRTData->m_cameraAngles );
		}

		pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_STARTED;
	}
}

bool CRenderToRTHelper::Process()
{
	bool bDidWork = false;

	if ( !m_pCurrentObjectToRender && !m_objectsToRender.empty() )
	{
		for ( RenderToRTData_t *pData : m_objectsToRender )
		{
			if ( pData->m_stage == RENDER_TO_RT_STAGE_STARTED )
			{
				m_pCurrentObjectToRender = pData;
				break;
			}
		}
	}

	if ( m_pCurrentObjectToRender )
	{	
		if ( m_pCurrentObjectToRender->m_stage == RENDER_TO_RT_STAGE_STARTED && m_pRenderTarget != nullptr && materials->CanDownloadTextures() )
		{
			// render it and save

			m_pCurrentObjectToRender->m_pResultVTF->Init( m_pRenderTarget->GetActualWidth(), m_pRenderTarget->GetActualHeight(), m_pRenderTarget->GetActualDepth(), IMAGE_FORMAT_BGRA8888, 0, 1 );

			int resolutionX = m_pRenderTarget->GetActualWidth();
			int resolutionY = m_pRenderTarget->GetActualHeight();
			unsigned char *pDestImage = m_pCurrentObjectToRender->m_pResultVTF->ImageData( 0, 0, 0 );

			StudioRenderConfig_t oldStudioRenderConfig;
			g_pStudioRender->GetCurrentConfig( oldStudioRenderConfig );

			MaterialLock_t hLock = materials->Lock();

			//init render context and set the the custom weapon RT as the current target
			CMatRenderContextPtr pRenderContext( materials );
	
			pRenderContext->PushRenderTargetAndViewport( m_pRenderTarget, 0, 0, resolutionX, resolutionY );

			VMatrix view, projection;
			ComputeViewMatrix( &view, m_pCurrentObjectToRender->m_Camera );
			ComputeProjectionMatrix( &projection, m_pCurrentObjectToRender->m_Camera, resolutionX, resolutionY );

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PushMatrix();
			pRenderContext->LoadIdentity( );

			pRenderContext->MatrixMode( MATERIAL_VIEW );
			pRenderContext->PushMatrix();
			pRenderContext->LoadMatrix( view );

			pRenderContext->MatrixMode( MATERIAL_PROJECTION );
			pRenderContext->PushMatrix();
			pRenderContext->LoadMatrix( projection );
		
			pRenderContext->SetLightingState( m_pCurrentObjectToRender->m_LightingState );

			pRenderContext->ClearColor4ub( 128, 128, 128, 0 ); 
			//pRenderContext->ClearBuffersObeyStencilEx( true, true, true );
			pRenderContext->ClearBuffers( true, true, true );

			// We want the models to use their natural alpha, not depth in alpha
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, 0 );

			// flashlights can't work in the model panel under queued mode (the state isn't ready yet, so causes a crash)
			pRenderContext->SetFlashlightMode( false );

			pRenderContext->BindLocalCubemap( m_pCurrentObjectToRender->m_pObject->GetEnvCubeMap() );

			m_pCurrentObjectToRender->m_pObject->Draw( m_pCurrentObjectToRender->m_rootToWorld );

			pRenderContext->ReadPixels( 0, 0, resolutionX, resolutionY, pDestImage, IMAGE_FORMAT_RGBA8888, m_pRenderTarget );

			pRenderContext->PopRenderTargetAndViewport();

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();

			pRenderContext->MatrixMode( MATERIAL_VIEW );
			pRenderContext->PopMatrix();

			pRenderContext->MatrixMode( MATERIAL_PROJECTION );
			pRenderContext->PopMatrix();
			
			pRenderContext->Flush();
			materials->Unlock( hLock );

			g_pStudioRender->UpdateConfig( oldStudioRenderConfig );

			m_pCurrentObjectToRender->m_stage = RENDER_TO_RT_STAGE_DONE;

			bDidWork = true;

			m_pCurrentObjectToRender = nullptr;
		}
	}

	return bDidWork;
}

// Purpose: Implements local hooks into named renderable textures.
// See matrendertexture.cpp in material system for list of available RT's

void ReleaseRenderTargets( int nChangeFlags );

void AddReleaseFunc( void )
{
	static bool bAdded = false;
	if( !bAdded )
	{
		bAdded = true;
		materials->AddReleaseFunc( ReleaseRenderTargets );
	}
}

//=============================================================================
// Stereo Param Texture
//=============================================================================
static CTextureReference s_pStereoParamTexture;
ITexture *GetStereoParamTexture( void )
{
	if ( !s_pStereoParamTexture )
	{
		s_pStereoParamTexture.Init( materials->FindTexture( "stereoparam", TEXTURE_GROUP_OTHER ) );
		AddReleaseFunc();
	}

	return s_pStereoParamTexture;
}

//=============================================================================
// Power of Two Frame Buffer Texture
//=============================================================================
static CTextureReference s_pPowerOfTwoFrameBufferTexture;
ITexture *GetPowerOfTwoFrameBufferTexture( void )
{

	if ( !s_pPowerOfTwoFrameBufferTexture )
	{
		s_pPowerOfTwoFrameBufferTexture.Init( materials->FindTexture( "_rt_PowerOfTwoFB", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pPowerOfTwoFrameBufferTexture;
}

//=============================================================================
// Fullscreen Texture
//=============================================================================
static CTextureReference s_pFullscreenTexture;
ITexture *GetFullscreenTexture( void )
{
	if ( !s_pFullscreenTexture )
	{
		s_pFullscreenTexture.Init( materials->FindTexture( "_rt_Fullscreen", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pFullscreenTexture;
}

//=============================================================================
// Camera Texture
//=============================================================================
static CTextureReference s_pCameraTexture;
ITexture *GetCameraTexture( void )
{
	if ( !s_pCameraTexture )
	{
		s_pCameraTexture.Init( materials->FindTexture( "_rt_Camera", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pCameraTexture;
}

//=============================================================================
// Full Frame Depth Texture
//=============================================================================
static CTextureReference s_pFullFrameDepthTexture;
ITexture *GetFullFrameDepthTexture( void )
{
	if ( !s_pFullFrameDepthTexture )
	{
		s_pFullFrameDepthTexture.Init( materials->FindTexture( "_rt_FullFrameDepth", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pFullFrameDepthTexture;
}

//=============================================================================
// Full Frame Buffer Textures
//=============================================================================
static CTextureReference s_pFullFrameFrameBufferTexture[MAX_FB_TEXTURES];
ITexture *GetFullFrameFrameBufferTexture( int textureIndex )
{
	if ( !s_pFullFrameFrameBufferTexture[textureIndex] )
	{
		char name[256];
		if( textureIndex != 0 )
		{
			V_snprintf( name, std::size( name ), "_rt_FullFrameFB%d", textureIndex );
		}
		else
		{
			V_strcpy( name, "_rt_FullFrameFB" );
		}
		s_pFullFrameFrameBufferTexture[textureIndex].Init( materials->FindTexture( name, TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pFullFrameFrameBufferTexture[textureIndex];
}

//=============================================================================
// Water reflection
//=============================================================================
static CTextureReference s_pWaterReflectionTexture;
ITexture *GetWaterReflectionTexture( void )
{
	if ( !s_pWaterReflectionTexture )
	{
		s_pWaterReflectionTexture.Init( materials->FindTexture( "_rt_WaterReflection", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pWaterReflectionTexture;
}

//=============================================================================
// Water refraction
//=============================================================================
static CTextureReference s_pWaterRefractionTexture;
ITexture *GetWaterRefractionTexture( void )
{
	if ( !s_pWaterRefractionTexture )
	{
		s_pWaterRefractionTexture.Init( materials->FindTexture( "_rt_WaterRefraction", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pWaterRefractionTexture;
}

//=============================================================================
// Custom Weapon RT
//=============================================================================
static CTextureReference s_pCustomWeaponTexture;
ITexture *GetCustomWeaponTexture( void )
{ 
	if ( !s_pCustomWeaponTexture )
	{
		s_pCustomWeaponTexture.Init( materials->FindTexture( "_rt_CustomWeapon", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTexture;
}

//=============================================================================
// Custom Weapon RT for exponent
//=============================================================================
static CTextureReference s_pCustomWeaponTextureExp;
ITexture *GetCustomWeaponTextureExp( void )
{ 
	if ( !s_pCustomWeaponTextureExp )
	{
		s_pCustomWeaponTextureExp.Init( materials->FindTexture( "_rt_CustomWeaponExp", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTextureExp;
}

//=============================================================================
// Small Buffer HDR0
//=============================================================================
static CTextureReference s_pSmallBufferHDR0;
ITexture *GetSmallBufferHDR0( void )
{
	if ( !s_pSmallBufferHDR0 )
	{
		s_pSmallBufferHDR0.Init( materials->FindTexture( "_rt_SmallHDR0", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR0;
}

//=============================================================================
// Small Buffer HDR1
//=============================================================================
static CTextureReference s_pSmallBufferHDR1;
ITexture *GetSmallBufferHDR1( void )
{
	if ( !s_pSmallBufferHDR1 )
	{
		s_pSmallBufferHDR1.Init( materials->FindTexture( "_rt_SmallHDR1", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR1;
}

//=============================================================================
// Quarter Sized FB0
//=============================================================================
static CTextureReference s_pQuarterSizedFB0;
ITexture *GetSmallBuffer0( void )
{
	if ( !s_pQuarterSizedFB0 )
	{
		s_pQuarterSizedFB0.Init( materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB0;
}

//=============================================================================
// Quarter Sized FB1
//=============================================================================
static CTextureReference s_pQuarterSizedFB1;
ITexture *GetSmallBuffer1( void )
{
	if ( !s_pQuarterSizedFB1 )
	{
		s_pQuarterSizedFB1.Init( materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB1;
}

//=============================================================================
// Teeny Textures
//=============================================================================
static CTextureReference s_TeenyTextures[MAX_TEENY_TEXTURES];
ITexture *GetTeenyTexture( int which )
{

	contract_assert( which < MAX_TEENY_TEXTURES );

	if ( !s_TeenyTextures[which] )
	{
		char nbuf[20];
		V_snprintf( nbuf, std::size( nbuf ), "_rt_TeenyFB%d", which );
		s_TeenyTextures[which].Init( materials->FindTexture( nbuf, TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	return s_TeenyTextures[which];
}

void ReleaseRenderTargets( int nChangeFlags )
{
	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		return;

	s_pPowerOfTwoFrameBufferTexture.Shutdown();
	s_pCameraTexture.Shutdown();
	s_pWaterReflectionTexture.Shutdown();
	s_pWaterRefractionTexture.Shutdown();
	s_pQuarterSizedFB0.Shutdown();
	s_pQuarterSizedFB1.Shutdown();
	s_pFullFrameDepthTexture.Shutdown();
	s_pCustomWeaponTexture.Shutdown();

	for (int i=0; i<MAX_FB_TEXTURES; ++i)
	{
		s_pFullFrameFrameBufferTexture[i].Shutdown();
	}
}

class CEffectData;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CRopeKeyframe;

using namespace ::ks::reflect;

//int		g_sModelXmasBulb;	// holds the sprite index for splattered blood

//Precache the rope shadowdepth material
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheRopes )
	PRECACHE( MATERIAL, "cable/rope_shadowdepth" )
	//PRECACHE_INDEX( MODEL, "effects/christmas_bulb.vmt", g_sModelXmasBulb )
PRECACHE_REGISTER_END()

void RecvProxy_RecomputeSprings( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Have the regular proxy store the data.
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	CRopeKeyframe *pRope = (CRopeKeyframe*)pStruct;
	pRope->RecomputeSprings();
}

IMPLEMENT_REFLECT_CLIENTCLASS( CRopeKeyframe, DT_RopeKeyframe, CRopeKeyframe )

static constexpr auto ROPE_IMPULSE_SCALE = 20;
static constexpr auto ROPE_IMPULSE_DECAY = 0.95;

static ConVar rope_shake( side_registry(), "rope_shake", "0" );
static ConVar rope_subdiv( side_registry(), "rope_subdiv", "2", FCVAR_MATERIAL_SYSTEM_THREAD, "Rope subdivision amount", true, 0, true, MAX_ROPE_SUBDIVS );
static ConVar rope_collide( side_registry(), "rope_collide", "1", 0, "Collide rope with the world" );

static ConVar rope_smooth( side_registry(), "rope_smooth", "1", 0, "Do an antialiasing effect on ropes" );
static ConVar rope_smooth_enlarge( side_registry(), "rope_smooth_enlarge", "1.4", 0, "How much to enlarge ropes in screen space for antialiasing effect" );

static ConVar rope_smooth_minwidth( side_registry(), "rope_smooth_minwidth", "0.3", 0, "When using smoothing, this is the min screenspace width it lets a rope shrink to" );
static ConVar rope_smooth_minalpha( side_registry(), "rope_smooth_minalpha", "0.2", 0, "Alpha for rope antialiasing effect" );

static ConVar rope_smooth_maxalphawidth( side_registry(), "rope_smooth_maxalphawidth", "1.75" );
static ConVar rope_smooth_maxalpha( side_registry(), "rope_smooth_maxalpha", "0.5", 0, "Alpha for rope antialiasing effect" );

static ConVar mat_fullbright( side_registry(), "mat_fullbright", "0", FCVAR_CHEAT ); // get it from the engine
static ConVar r_drawropes( side_registry(), "r_drawropes", "1", FCVAR_CHEAT );
static ConVar r_ropetranslucent( side_registry(), "r_ropetranslucent", "1");
static ConVar r_rope_holiday_light_scale( side_registry(), "r_rope_holiday_light_scale", "0.14", FCVAR_DEVELOPMENTONLY );
//static ConVar r_ropes_holiday_lights_allowed( "r_ropes_holiday_lights_allowed", "0", FCVAR_DEVELOPMENTONLY );
static ConVar r_ropes_holiday_lights_type( side_registry(), "r_ropes_holiday_lights_type", "0", FCVAR_DEVELOPMENTONLY, "0 == sprites, 1 == models" );
static ConVar r_ropes_holiday_max_dist_to_draw( side_registry(), "r_ropes_holiday_max_dist_to_draw", "700", FCVAR_DEVELOPMENTONLY, "If a ropes length (from one end to the other, not counting slack), exceeds this distance, it won't have lights." );

static ConVar rope_wind_dist( side_registry(), "rope_wind_dist", "1000", 0, "Don't use CPU applying small wind gusts to ropes when they're past this distance." );
static ConVar rope_averagelight( side_registry(), "rope_averagelight", "1", 0, "Makes ropes use average of cubemap lighting instead of max intensity." );

static ConVar rope_rendersolid( side_registry(), "rope_rendersolid", "1" );

static ConVar rope_solid_minwidth( side_registry(), "rope_solid_minwidth", "0.3" );
static ConVar rope_solid_maxwidth( side_registry(), "rope_solid_maxwidth", "1" );

static ConVar rope_solid_minalpha( side_registry(), "rope_solid_minalpha", "0.0" );
static ConVar rope_solid_maxalpha( side_registry(), "rope_solid_maxalpha", "1" );

static int			g_nRopePointsSimulated;

static IMaterial *g_pSplineCableShadowdepth = nullptr;
// Active ropes.
std::vector<CRopeKeyframe*> g_Ropes;

static Vector	g_FullBright_LightValues[ROPE_MAX_SEGMENTS];
class CFullBrightLightValuesInit
{
public:
	CFullBrightLightValuesInit()
	{
		for( int i=0; i < ROPE_MAX_SEGMENTS; i++ )
			g_FullBright_LightValues[i].Init( 1, 1, 1 );
	}
} g_FullBrightLightValuesInit;

// This can be exposed through the entity if we ever care.
static float g_flLockAmount = 0.1;
static float g_flLockFalloff = 0.3;

class CRopeDelayedEffects
{
public:
	float m_flTimeProcessedOnMainThread; // gpGlobals->curtime
	std::recursive_mutex m_mtx;
	std::vector< CEffectData > m_arrEffects;
}
g_RopeDelayedEffects;

//=============================================================================
//
// Rope mananger.
//
class CRopeManager : public IRopeManager
{
public:

	CRopeManager();
	~CRopeManager();

	void ResetRenderCache( void );
	void AddToRenderCache( CRopeKeyframe *pRope );
	void DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth );

	void SetHolidayLightMode( bool bHoliday ) { m_bDrawHolidayLights = bHoliday; }
	bool IsHolidayLightMode( void );
	int GetHolidayLightStyle( void );
	
	enum { MAX_ROPE_RENDERCACHE	= 128 };

	void RemoveRopeFromQueuedRenderCaches( CRopeKeyframe *pRope );
	
private:
	struct RopeRenderData_t
	{
		IMaterial			*m_pSolidMaterial;
		int					m_nCacheCount;
		CRopeKeyframe		*m_aCache[MAX_ROPE_RENDERCACHE];
	};

	struct RopeQueuedRenderCache_t
	{
		RopeRenderData_t *pCaches;
		int iCacheCount;
		std::recursive_mutex *m_pRopeDataMutex;
		RopeQueuedRenderCache_t( void ) : pCaches(nullptr), iCacheCount(0) { };
	};

	void DrawRenderCache_NonQueued( bool bShadowDepth, RopeRenderData_t *pRenderCache, int nRenderCacheCount, const Vector &vCurrentViewForward, const Vector &vCurrentViewOrigin, CRopeKeyframe::BuildRopeQueuedData_t *pBuildRopeQueuedData, std::recursive_mutex *pRopeDataMutex );

	std::vector< RopeRenderData_t >	m_aRenderCache;
	
	//in queued material system mode we need to store off data for later use.
	IMaterial* m_pDepthWriteMaterial;
	std::deque<RopeQueuedRenderCache_t> m_RopeQueuedRenderCaches;	
	std::recursive_mutex		m_RopeQueuedRenderCaches_Mutex; //mutex just for changing m_RopeQueuedRenderCaches
	bool m_bDrawHolidayLights;
	bool m_bHolidayInitialized;
	int m_nHolidayLightsStyle;
};

static CRopeManager s_RopeManager;

IRopeManager *RopeManager()
{
	return &s_RopeManager;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CRopeManager::CRopeManager()
{
	m_aRenderCache.clear(); m_aRenderCache.shrink_to_fit();
	m_pDepthWriteMaterial = nullptr;
	m_bDrawHolidayLights = false;
	m_bHolidayInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CRopeManager::~CRopeManager()
{
	m_aRenderCache.clear(); m_aRenderCache.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRopeManager::ResetRenderCache( void )
{
	int nRenderCacheCount = std::ssize( m_aRenderCache );
	for ( int iRenderCache = 0; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		m_aRenderCache[iRenderCache].m_nCacheCount = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRopeManager::AddToRenderCache( CRopeKeyframe *pRope )
{
	if( !pRope->GetSolidMaterial() )
	{
		return;
	}

	// Find the current rope list.
	int iRenderCache = 0;
	int nRenderCacheCount = std::ssize( m_aRenderCache );
	for ( ; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		if ( pRope->GetSolidMaterial() == m_aRenderCache[iRenderCache].m_pSolidMaterial )
			break;
	}

	// A full rope list should have been generate in CreateRenderCache
	// If we didn't find one, then allocate the mofo.
	if ( iRenderCache == nRenderCacheCount )
	{
		int iRenderCache = std::ssize( m_aRenderCache ); m_aRenderCache.emplace_back();
		m_aRenderCache[iRenderCache].m_pSolidMaterial = pRope->GetSolidMaterial();
		m_aRenderCache[iRenderCache].m_nCacheCount = 0;
	}

	if ( m_aRenderCache[iRenderCache].m_nCacheCount >= MAX_ROPE_RENDERCACHE )
	{
		Warning( "CRopeManager::AddToRenderCache count to large for cache!\n" );
		return;
	}

	m_aRenderCache[iRenderCache].m_aCache[m_aRenderCache[iRenderCache].m_nCacheCount] = pRope;
	++m_aRenderCache[iRenderCache].m_nCacheCount;
}
#define OUTPUT_2SPLINE_VERTS( v, t, u )							\
	meshBuilder.Color4ub( v, nRed, nGreen, nBlue, nAlpha );		\
	meshBuilder.Position3f( v, (t), u, 0 );						\
	meshBuilder.TexCoord4fv( v, 0, vecP0.Base() );				\
	meshBuilder.TexCoord4fv( v, 1, vecP1.Base() );				\
	meshBuilder.TexCoord4fv( v, 2, vecP2.Base() );				\
	meshBuilder.TexCoord4fv( v, 3, vecP3.Base() );				\
	meshBuilder.Color4ub( v + 1, nRed, nGreen, nBlue, nAlpha );	\
	meshBuilder.Position3f( v + 1, (t), u, 1 );					\
	meshBuilder.TexCoord4fv( v + 1, 0, vecP0.Base() );			\
	meshBuilder.TexCoord4fv( v + 1, 1, vecP1.Base() );			\
	meshBuilder.TexCoord4fv( v + 1, 2, vecP2.Base() );			\
	meshBuilder.TexCoord4fv( v + 1, 3, vecP3.Base() );			\
	v += 2;

#define OUTPUT_SPLINE_QUAD( i, c )								\
	meshBuilder.FastIndex( i, c );								\
	meshBuilder.FastIndex( i + 1, c + 1 );						\
	meshBuilder.FastIndex( i + 2, c + 2 );						\
	meshBuilder.FastIndex( i + 3, c + 1 );						\
	meshBuilder.FastIndex( i + 4, c + 3 );						\
	meshBuilder.FastIndex( i + 5, c + 2 );						\
	i += 6;														

void CRopeManager::DrawRenderCache_NonQueued( bool bShadowDepth, RopeRenderData_t *pRenderCache, int nRenderCacheCount, const Vector &vCurrentViewForward, const Vector &vCurrentViewOrigin, CRopeKeyframe::BuildRopeQueuedData_t *pBuildRopeQueuedData, std::recursive_mutex *pRopeDataMutex )
{
	CVProfScope _( "CRopeManager::DrawRenderCache", 0, VPROF_BUDGETGROUP_ROPES, false, BUDGETFLAG_OTHER );
	
	std::recursive_mutex dummyMutex;
	if( pRopeDataMutex == nullptr )
		pRopeDataMutex = &dummyMutex;

	if ( bShadowDepth && !m_pDepthWriteMaterial && g_pMaterialSystem )
	{
		KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
		pVMTKeyValues->SetInt( "$no_fullbright", 1 );
		pVMTKeyValues->SetInt( "$alphatest", 0 );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		m_pDepthWriteMaterial = g_pMaterialSystem->FindProceduralMaterial( "__DepthWrite01", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	}
	CMatRenderContextPtr pRenderContext( materials );

	// UNDONE: needs to use the queued data
	do
	{
		std::scoped_lock lock( *pRopeDataMutex );
		int defaultSubdiv = rope_subdiv.GetInt();
		for ( int iRenderCache = 0; iRenderCache < nRenderCacheCount; ++iRenderCache )
		{
			int nCacheCount = pRenderCache[iRenderCache].m_nCacheCount;

			int nTotalVerts = 0;
			int nTotalIndices = 0;
			for ( int iCache = 0; iCache < nCacheCount; ++iCache )
			{
				CRopeKeyframe *pRope = pRenderCache[iRenderCache].m_aCache[iCache];
				if ( pRope )
				{
					int segs =  pRope->m_RopePhysics.NumNodes()-1;
					int nSubdivCount = (pRope->m_Subdiv != 255 ? pRope->m_Subdiv : defaultSubdiv) + 1;
					nTotalVerts += ((2 * nSubdivCount) * segs) + 2;
					nTotalIndices += (6 * nSubdivCount) * segs;
				}
			}
			if ( nTotalVerts == 0 )
				continue;		

			IMaterial *pMaterial = bShadowDepth ? g_pSplineCableShadowdepth : pRenderCache[iRenderCache].m_pSolidMaterial;

			// Need to make sure that all rope materials use the splinerope shader since there are a lot of assumptions about how the shader interfaces with this code.
			contract_assert( V_stricmp( pMaterial->GetShaderName(), "splinerope" ) == 0 );

			pRenderContext->Bind( pMaterial );

			int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
			int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

			IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
			CMeshBuilder meshBuilder;
			int meshVertCount = std::min(nTotalVerts, nMaxVertices);
			int meshIndexCount = std::min(nTotalIndices, nMaxIndices);
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, meshVertCount, meshIndexCount );
			if ( meshBuilder.m_ActualVertexSize == 0 )
			{
				// We're likely in alt+tab, and since we're using fast vertex/index routines, we need to see if we're writing into valid vertex data
				meshBuilder.End();
				break;
			}

			int nCurIDX = 0;

			int availableVerts = meshVertCount;
			int availableIndices = meshIndexCount;
			float flLastU = 1.0f;

			int nVertices = 0;
			int nIndices = 0;
			for ( int iCache = 0; iCache < nCacheCount; ++iCache )
			{
				CRopeKeyframe *pRope = pRenderCache[iRenderCache].m_aCache[iCache];
				if ( pRope )
				{
					CSimplePhysics::CNode *pNode = pRope->m_RopePhysics.GetFirstNode();
					int nSegmentsToRender = pRope->m_RopePhysics.NumNodes()-1;
					if ( !nSegmentsToRender )
						continue;

					int nParticles = pRope->m_RopePhysics.NumNodes();
					int nSubdivCount = (pRope->m_Subdiv != 255 ? pRope->m_Subdiv : defaultSubdiv) + 1;

					int nNumIndicesPerSegment = 6 * nSubdivCount;
					int nNumVerticesPerSegment = 2 * nSubdivCount;

					int nSegmentsAvailableInBuffer = std::min( ( availableVerts - 2 ) / nNumVerticesPerSegment,
						( availableIndices ) / nNumIndicesPerSegment );

					int segmentsInBuffer = std::min(nSegmentsAvailableInBuffer,nSegmentsToRender);
					availableIndices -= nNumIndicesPerSegment * segmentsInBuffer;
					availableVerts -= 2 + (nNumVerticesPerSegment * segmentsInBuffer);

					float width = pRope->m_Width;
					Vector vModColor = pRope->m_vColorMod;
					Vector *pColors = pRope->m_LightValues;
					
					// Figure out texture scale.
					float flPixelsPerInch = 4.0f / pRope->m_TextureScale;
					// This is the total number of texels for the length of the whole rope.
					float flTotalTexCoord = flPixelsPerInch * ( pRope->m_RopeLength + pRope->m_Slack + ROPESLACK_FUDGEFACTOR );
					int nTotalPoints = (nSegmentsToRender * (nSubdivCount-1)) + 1;
					float flDU = ( flTotalTexCoord / nTotalPoints ) / ( float )pRope->m_TextureHeight;
					float flU = pRope->m_flCurScroll;
					float m_flTStep = 1.0f / float(nSubdivCount);

					bool bFirstPoint = true;

					// initialize first spline segment
					Vector4D vecP1;
					Vector4D vecP2;
					vecP1.Init( pNode[0].m_vPredicted, pRope->m_Width );
					vecP2.Init( pNode[1].m_vPredicted, pRope->m_Width );
					Vector4D vecP0 = vecP1;

					uint8 nRed = 0;
					uint8 nGreen = 0;
					uint8 nBlue = 0;
					uint8 nAlpha = 255;

					Vector4D vecDelta = vecP2;
					vecDelta -= vecP1;
					vecP0 -= vecDelta;

					Vector4D vecP3;

					if ( nParticles < 3 )
					{
						vecP3 = vecP2;
						vecP3 += vecDelta;
					}
					else
					{
						vecP3.Init( pNode[2].m_vPredicted, width );
					}
					int nPnt = 3;
					int nColor = 1;
					Vector vColor0( pColors[0].x * vModColor.x, pColors[0].y * vModColor.y, pColors[0].z * vModColor.z );
					Vector vColor1( pColors[1].x * vModColor.x, pColors[1].y * vModColor.y, pColors[1].z * vModColor.z );

					float flT = 0;
					do
					{
						if ( ! nSegmentsAvailableInBuffer )
						{
							meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>( nVertices );
							meshBuilder.AdvanceIndices( nIndices );
							meshBuilder.End();
							pMesh->Draw();
							nTotalVerts -= (meshVertCount - availableVerts);
							nTotalIndices -= (meshIndexCount - availableIndices);
							meshVertCount = std::min(nTotalVerts, nMaxVertices);
							meshIndexCount = std::min(nTotalIndices, nMaxIndices);
							meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, meshVertCount, meshIndexCount );
							nVertices = 0;
							nIndices = 0;
							availableVerts = meshVertCount;
							availableIndices = meshIndexCount;
							// copy the last emitted points
							OUTPUT_2SPLINE_VERTS( nVertices, flT, flLastU );

							nSegmentsAvailableInBuffer = std::min( ( availableVerts - 2 ) / nNumVerticesPerSegment,
								availableIndices / nNumIndicesPerSegment );

							nCurIDX = 0;
						}
						nSegmentsAvailableInBuffer--;
						flT = 0.;
						for( int nSlice = 0 ; nSlice < nSubdivCount; nSlice++ )
						{
							float omt = 1.0f - flT;
							nRed = FastFToC( (vColor0.x * omt) + (vColor1.x*flT) );
							nGreen = FastFToC( (vColor0.y * omt) + (vColor1.y*flT) );
							nBlue = FastFToC( (vColor0.z * omt) + (vColor1.z*flT) );
							OUTPUT_2SPLINE_VERTS( nVertices, flT, flU );
							flT += m_flTStep;
							flU += flDU;
							if ( ! bFirstPoint )
							{
								OUTPUT_SPLINE_QUAD( nIndices, nCurIDX );
								nCurIDX += 2;
							}
							bFirstPoint = false;
						}
						// next segment
						vColor0 = vColor1;
						if ( nColor < nParticles-1 )
						{
							nColor++;
							vColor1.Init( pColors[nColor].x * vModColor.x, pColors[nColor].y * vModColor.y, pColors[nColor].z * vModColor.z );
						}
						if ( nSegmentsToRender > 1 )
						{
							vecP0 = vecP1;
							vecP1 = vecP2;
							vecP2 = vecP3;

							if ( nPnt < nParticles )
							{
								vecP3.AsVector3D() = pNode[nPnt].m_vPredicted;				
								nPnt++;
							}
							else
							{
								// fake last point by extrapolating
								vecP3 += vecP2;
								vecP3 -= vecP1;
							}
						}

					} while( --nSegmentsToRender );

					int nSegsToRender = pRope->m_RopePhysics.NumNodes()-1;
					Vector vecRopeStart = pNode[0].m_vPredicted;
					Vector vecRopeEnd = pNode[nSegsToRender].m_vPredicted;
					float flDist = (vecRopeStart - vecRopeEnd).Length();
					if ( flDist < r_ropes_holiday_max_dist_to_draw.GetFloat() )
					{
						for ( int nSeg = 1; nSeg < nSegsToRender; ++nSeg )
						{
							CEffectData data;
							if ( RopeManager()->IsHolidayLightMode() &&
								 pRope->m_ropeType == ROPE_TYPE_DEFAULT &&
								 pRope->m_iDefaultRopeMaterialModelIndex == pRope->m_iRopeMaterialModelIndex &&
								 pRope->m_RopePhysics.NumNodes() >= 5 &&
								 m_RopeQueuedRenderCaches.size() == 1 &&
								 r_rope_holiday_light_scale.GetFloat() > 0.0f )
							{
								int xy = ( int )( pNode[nSeg].m_vPredicted.x + pNode[nSeg].m_vPredicted.y );
								data.m_nMaterial = xy;
								int z = ( int )pNode[nSeg].m_vPredicted.z;
								data.m_nHitBox = ( z << 8 );
								data.m_flScale = r_rope_holiday_light_scale.GetFloat();
								data.m_vOrigin = pNode[nSeg].m_vPredicted;
								{
									std::scoped_lock lock( g_RopeDelayedEffects.m_mtx );
									g_RopeDelayedEffects.m_arrEffects.push_back( data );
								}
							}
						}
					}

					// output last piece
					OUTPUT_2SPLINE_VERTS( nVertices, 1.0, flU );
					OUTPUT_SPLINE_QUAD( nIndices, nCurIDX );
					nCurIDX += 4;
					flLastU = flU;
				}
			}

			meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>( nVertices );
			meshBuilder.AdvanceIndices( nIndices );
			meshBuilder.End();
			pMesh->Draw();
		}
	}
	while( false ); //wrap in a single execution loop for bail out scenarios

	m_RopeQueuedRenderCaches_Mutex.lock();
	if( pBuildRopeQueuedData && !m_RopeQueuedRenderCaches.empty() )
	{
		delete m_RopeQueuedRenderCaches.front().m_pRopeDataMutex;
		m_RopeQueuedRenderCaches.pop_front();
	}
	m_RopeQueuedRenderCaches_Mutex.unlock();
}

ConVar r_queued_ropes( side_registry(), "r_queued_ropes", "1" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRopeManager::DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth )
{
	int iRenderCacheCount = std::ssize( m_aRenderCache );

	if( iRenderCacheCount == 0 )
		return;

	// Check to see if we want to render the ropes.
	if( !r_drawropes.GetBool() )
		return;

	Vector vForward = CurrentViewForward();
	Vector vOrigin = CurrentViewOrigin();

	ICallQueue *pCallQueue;
	if( r_queued_ropes.GetBool() && (pCallQueue = pRenderContext->GetCallQueue()) != nullptr )
	{
		//material queue available and desired
		CRopeManager::RopeRenderData_t *pRenderCache = m_aRenderCache.data();
		
		int iRopeCount = 0;
		int iNodeCount = 0;
		for( int i = 0; i != iRenderCacheCount; ++i )
		{
			CRopeManager::RopeRenderData_t *pCache = &pRenderCache[i];
			int iCacheCount = pCache->m_nCacheCount;
			iRopeCount += iCacheCount;
			for( int j = 0; j != iCacheCount; ++j )
			{
				CRopeKeyframe *pRope = pCache->m_aCache[j];
				if( pRope )
					iNodeCount += pRope->m_RopePhysics.NumNodes();
				else
					--iRopeCount;
			}
		}

		if( iRopeCount == 0 )
			return; //nothing to draw

		size_t iMemoryNeeded = (iRenderCacheCount * sizeof(CRopeManager::RopeRenderData_t)) + 
								(iRopeCount * sizeof(CRopeKeyframe::BuildRopeQueuedData_t)) +
								(iNodeCount * (sizeof(Vector) * 2));

		CMatRenderData< byte > rd(pRenderContext, iMemoryNeeded);
		void *pMemory = rd.Base();

		CRopeManager::RopeRenderData_t *pRenderCachesStart = (CRopeManager::RopeRenderData_t *)pMemory;
		CRopeKeyframe::BuildRopeQueuedData_t *pBuildRopeQueuedDataStart = (CRopeKeyframe::BuildRopeQueuedData_t *)(pRenderCachesStart + iRenderCacheCount);
		Vector *pVectorDataStart = (Vector *)(pBuildRopeQueuedDataStart + iRopeCount);
		
		//memcpy( pRenderCachesStart, m_aRenderCache.Base(), iRenderCacheCount * sizeof( CRopeManager::RopeRenderData_t ) );

		RopeQueuedRenderCache_t cache;
		cache.pCaches = pRenderCachesStart;
		cache.iCacheCount = iRenderCacheCount;
		cache.m_pRopeDataMutex = new std::recursive_mutex;
		
		CRopeKeyframe::BuildRopeQueuedData_t *pWriteRopeQueuedData = pBuildRopeQueuedDataStart;
		Vector *pVectorWrite = (Vector *)pVectorDataStart;

		//Setup the rest of our data. This writes to two separate areas of memory at the same time. One area for the CRopeKeyframe::BuildRopeQueuedData_t array, the other for mini-arrays of vector data
		for( int i = 0; i != iRenderCacheCount; ++i )
		{
			CRopeManager::RopeRenderData_t *pReadCache = &pRenderCache[i];
			CRopeManager::RopeRenderData_t *pWriteCache = &pRenderCachesStart[i];
			int iCacheCount = pReadCache->m_nCacheCount;
			pWriteCache->m_nCacheCount = 0;
			pWriteCache->m_pSolidMaterial = pReadCache->m_pSolidMaterial;
			for( int j = 0; j != iCacheCount; ++j )
			{
				CRopeKeyframe *pRope = pReadCache->m_aCache[j];
				if( pRope == nullptr )
					continue;

				pWriteCache->m_aCache[pWriteCache->m_nCacheCount] = pRope;
				++pWriteCache->m_nCacheCount;

				int iNodes = pRope->m_RopePhysics.NumNodes();

				//setup the CRopeKeyframe::BuildRopeQueuedData_t struct
				pWriteRopeQueuedData->m_iNodeCount = pRope->m_RopePhysics.NumNodes();
				pWriteRopeQueuedData->m_vColorMod = pRope->m_vColorMod;
				pWriteRopeQueuedData->m_RopeLength = pRope->m_RopeLength;
				pWriteRopeQueuedData->m_Slack = pRope->m_Slack;
				pWriteRopeQueuedData->m_pPredictedPositions = pVectorWrite;
				pWriteRopeQueuedData->m_pLightValues = pVectorWrite + iNodes;
				++pWriteRopeQueuedData;

				//make two arrays, one of predicted positions followed immediately by light values
				for( int k = 0; k != iNodes; ++k )
				{					
					pVectorWrite[0] = pRope->m_RopePhysics.GetNode( k )->m_vPredicted;
					pVectorWrite[iNodes] = pRope->m_LightValues[k];
					++pVectorWrite;
				}
				pVectorWrite += iNodes; //so we don't overwrite the light values with the next rope's predicted positions
			}
		}

		m_RopeQueuedRenderCaches_Mutex.lock();
		m_RopeQueuedRenderCaches.push_back( cache );
		std::recursive_mutex *pRopeDataMutex = m_RopeQueuedRenderCaches.back().m_pRopeDataMutex;
		m_RopeQueuedRenderCaches_Mutex.unlock();

		contract_assert( ((void *)pVectorWrite == (void *)(((uint8 *)pMemory) + iMemoryNeeded)) && ((void *)pWriteRopeQueuedData == (void *)pVectorDataStart) );		
		pCallQueue->QueueCall( this, &CRopeManager::DrawRenderCache_NonQueued, bShadowDepth, pRenderCachesStart, iRenderCacheCount, vForward, vOrigin, pBuildRopeQueuedDataStart, pRopeDataMutex );

// 		if ( IsHolidayLightMode() )
// 		{
// 			// With holiday lights we need to also build the ropes non-queued without rendering them
// 			DrawRenderCache_NonQueued( bShadowDepth, m_aRenderCache.Base(), iRenderCacheCount, vForward, vOrigin, NULL, NULL );
// 		}
	}
	else
	{
		DrawRenderCache_NonQueued( bShadowDepth, m_aRenderCache.data(), iRenderCacheCount, vForward, vOrigin, nullptr, nullptr );
	}
}

bool CRopeManager::IsHolidayLightMode( void )
{
	/* Removed for partner depot */
	bool bDrawHolidayLights = false;
	return bDrawHolidayLights;
}

int CRopeManager::GetHolidayLightStyle( void )
{
	return r_ropes_holiday_lights_type.GetInt();
}

void CRopeManager::RemoveRopeFromQueuedRenderCaches( CRopeKeyframe *pRope )
{
	//remove this rope from queued render caches
	std::scoped_lock lock( m_RopeQueuedRenderCaches_Mutex );
	
	for ( RopeQueuedRenderCache_t &RenderCacheData : m_RopeQueuedRenderCaches )
	{
		for( int i = 0; i != RenderCacheData.iCacheCount; ++i )
		{
			RopeRenderData_t *pCache = &RenderCacheData.pCaches[i];
			for( int j = 0; j != pCache->m_nCacheCount; ++j )
			{
				if( pCache->m_aCache[j] == pRope )
				{
					RenderCacheData.m_pRopeDataMutex->lock();
					pCache->m_aCache[j] = nullptr;
					RenderCacheData.m_pRopeDataMutex->unlock();
				}
			}
		}
	}
}

//=============================================================================

// ------------------------------------------------------------------------------------ //
// Global functions.
// ------------------------------------------------------------------------------------ //

void Rope_ResetCounters()
{
	g_nRopePointsSimulated = 0;
}

// ------------------------------------------------------------------------------------ //
// This handles the rope shake command.
// ------------------------------------------------------------------------------------ //

void ShakeRopesCallback( const CEffectData &data )
{
	Vector vCenter = data.m_vOrigin;
	float flRadius = data.m_flRadius;
	float flMagnitude = data.m_flMagnitude;

	// Now find any nearby ropes and shake them.
	for ( CRopeKeyframe *pRope : g_Ropes )
	{
		pRope->ShakeRope( vCenter, flRadius, flMagnitude );
	}
}

DECLARE_CLIENT_EFFECT( ShakeRopes, ShakeRopesCallback )

// ------------------------------------------------------------------------------------ //
// CRopeKeyframe::CPhysicsDelegate
// ------------------------------------------------------------------------------------ //
static constexpr auto WIND_FORCE_FACTOR = 10;

void CRopeKeyframe::CPhysicsDelegate::GetNodeForces( CSimplePhysics::CNode *pNodes, int iNode, Vector *pAccel )
{
	// Gravity.
	if ( !( m_pKeyframe->GetRopeFlags() & ROPE_NO_GRAVITY ) )
	{
		pAccel->Init( ROPE_GRAVITY );
	}

	if( !m_pKeyframe->m_LinksTouchingSomething[iNode] && m_pKeyframe->m_bApplyWind)
	{
		Vector vecWindVel;
		GetWindspeedAtTime(gpGlobals->curtime, vecWindVel);
		if ( vecWindVel.LengthSqr() > 0 )
		{
			Vector vecWindAccel;
			VectorMA( *pAccel, WIND_FORCE_FACTOR, vecWindVel, *pAccel );
		}
		else
		{
			if ( ( m_pKeyframe->m_flCurrentGustLifetime != 0.0f ) && ( m_pKeyframe->m_flCurrentGustTimer < m_pKeyframe->m_flCurrentGustLifetime ) )
			{
				float div = m_pKeyframe->m_flCurrentGustTimer / m_pKeyframe->m_flCurrentGustLifetime;
				float scale = 1 - std::cos( div * std::numbers::pi );

				*pAccel += m_pKeyframe->m_vWindDir * scale;
			}
		}
	}

	// HACK.. shake the rope around.
	static float scale=15000;
	if( rope_shake.GetInt() )
	{
		*pAccel += RandomVector( -scale, scale );
	}

	// Apply any instananeous forces and reset
	*pAccel += ROPE_IMPULSE_SCALE * m_pKeyframe->m_vecImpulse;
	m_pKeyframe->m_vecImpulse *= ROPE_IMPULSE_DECAY;
	if ( m_pKeyframe->m_vecImpulse.LengthSqr() < 0.1f )
	{
		m_pKeyframe->m_vecImpulse = vec3_origin;
	}
}

void LockNodeDirection( 
	CSimplePhysics::CNode *pNodes, 
	int parity, 
	int nFalloffNodes,
	float flLockAmount,
	float flLockFalloff,
	const Vector &vIdealDir ) 
{
	for ( int i=0; i < nFalloffNodes; i++ )
	{
		Vector &v0 = pNodes[i*parity].m_vPos;
		Vector &v1 = pNodes[(i+1)*parity].m_vPos;

		Vector vDir = v1 - v0;
		float len = vDir.Length();
		if ( len > 0.0001f )
		{
			vDir /= len;

			Vector vActual;
			VectorLerp( vDir, vIdealDir, flLockAmount, vActual );
			v1 = v0 + vActual * len;

			flLockAmount *= flLockFalloff;
		}
	}
}

void CRopeKeyframe::CPhysicsDelegate::ApplyConstraints( CSimplePhysics::CNode *pNodes, int nNodes )
{
	CVProfScope _( "CPhysicsDelegate::ApplyConstraints", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Collide with the world.
	if( ((m_pKeyframe->m_RopeFlags & ROPE_COLLIDE) && 
		rope_collide.GetInt()) || 
		(rope_collide.GetInt() == 2) )
	{
		CTraceFilterWorldOnly traceFilter;

		for( int i=0; i < nNodes; i++ )
		{
			CSimplePhysics::CNode *pNode = &pNodes[i];

			int iIteration;
			const int nIterations = 10;
			for( iIteration=0; iIteration < nIterations; iIteration++ )
			{
				trace_t trace;
				UTIL_TraceHull( pNode->m_vPrevPos, pNode->m_vPos, 
					Vector(-2,-2,-2), Vector(2,2,2), MASK_SOLID_BRUSHONLY, &traceFilter, &trace );

				if( trace.fraction == 1 )
					break;

				if( trace.fraction == 0 || trace.allsolid || trace.startsolid )
				{
					m_pKeyframe->m_LinksTouchingSomething[i] = true;
					pNode->m_vPos = pNode->m_vPrevPos;
					break;
				}

				// Apply some friction.
				const float flSlowFactor = 0.3f;
				pNode->m_vPos -= (pNode->m_vPos - pNode->m_vPrevPos) * flSlowFactor;

				// Move it out along the face normal.
				float distBehind = trace.plane.normal.Dot( pNode->m_vPos ) - trace.plane.dist;
				pNode->m_vPos += trace.plane.normal * (-distBehind + 2.2);
				m_pKeyframe->m_LinksTouchingSomething[i] = true;
			}

			if( iIteration == nIterations )
				pNodes[i].m_vPos = pNodes[i].m_vPrevPos;
		}
	}

	// Lock the endpoints.
	QAngle angles;
	if( m_pKeyframe->m_fLockedPoints & ROPE_LOCK_START_POINT )
	{
		m_pKeyframe->GetEndPointAttachment( 0, pNodes[0].m_vPos, angles );
		if (( m_pKeyframe->m_fLockedPoints & ROPE_LOCK_START_DIRECTION ) && (nNodes > 3))
		{
			Vector forward;
			AngleVectors( angles, &forward );

			int parity = 1;
			int nFalloffNodes = std::min( 2, nNodes - 2 );
			LockNodeDirection( pNodes, parity, nFalloffNodes, g_flLockAmount, g_flLockFalloff, forward );
		}
	}

	if( m_pKeyframe->m_fLockedPoints & ROPE_LOCK_END_POINT )
	{
		m_pKeyframe->GetEndPointAttachment( 1, pNodes[nNodes-1].m_vPos, angles );
		if( m_pKeyframe->m_fLockedPoints & ROPE_LOCK_END_DIRECTION && (nNodes > 3))
		{
			Vector forward;
			AngleVectors( angles, &forward );
			
			int parity = -1;
			int nFalloffNodes = std::min( 2, nNodes - 2 );
			LockNodeDirection( &pNodes[nNodes-1], parity, nFalloffNodes, g_flLockAmount, g_flLockFalloff, forward );
		}
	}
}

// ------------------------------------------------------------------------------------ //
// CRopeKeyframe
// ------------------------------------------------------------------------------------ //

CRopeKeyframe::CRopeKeyframe()
{
	m_bEndPointAttachmentPositionsDirty = true;
	m_bEndPointAttachmentAnglesDirty = true;
	m_PhysicsDelegate.m_pKeyframe = this;
	m_pMaterial = nullptr;
	m_bPhysicsInitted = false;
	m_RopeFlags = 0;
	m_TextureHeight = 1;
	m_hStartPoint = m_hEndPoint = INVALID_EHANDLE;
	m_iStartAttachment = m_iEndAttachment = 0;
	m_vColorMod.Init( 1, 1, 1 );
	m_nLinksTouchingSomething = 0;
	m_Subdiv = 255; // default to using the cvar
	m_flCurrentGustLifetime = 0.0f;
	m_flCurrentGustTimer = 0.0f;

	m_fLockedPoints = 0;
	m_fPrevLockedPoints = 0;
	
	m_iForcePointMoveCounter = 0;
	m_flCurScroll = m_flScrollSpeed = 0;
	m_TextureScale = 4;	// 4:1
	m_vecImpulse.Init();

	m_ropeType = ROPE_TYPE_DEFAULT;

	g_Ropes.push_back( this );
}

CRopeKeyframe::~CRopeKeyframe()
{
	s_RopeManager.RemoveRopeFromQueuedRenderCaches( this );	
	ks::erase_first( g_Ropes, this );
}

CRopeKeyframe* CRopeKeyframe::Create(
	CBaseEntity *pStartEnt,
	CBaseEntity *pEndEnt,
	int iStartAttachment,
	int iEndAttachment,
	float ropeWidth,
	const char *pMaterialName,
	int numSegments,
	int ropeFlags,
	rope_type ropeType
	)
{
	CRopeKeyframe *pRope = new CRopeKeyframe;

	pRope->InitializeAsClientEntity( nullptr, false );
	
	if ( pStartEnt )
	{
		pRope->m_hStartPoint = pStartEnt;
		pRope->m_fLockedPoints |= ROPE_LOCK_START_POINT;
	}

	if ( pEndEnt )
	{
		pRope->m_hEndPoint = pEndEnt;
		pRope->m_fLockedPoints |= ROPE_LOCK_END_POINT;
	}

	pRope->m_iStartAttachment = iStartAttachment;
	pRope->m_iEndAttachment = iEndAttachment;
	pRope->m_Width = ropeWidth;
	pRope->m_nSegments = clamp( numSegments, 2, ROPE_MAX_SEGMENTS );
	pRope->m_RopeFlags = ropeFlags;
	pRope->m_ropeType = ropeType;

	pRope->FinishInit( pMaterialName );
	return pRope;
}

CRopeKeyframe* CRopeKeyframe::CreateFromKeyValues( CBaseAnimating *pEnt, KeyValues *pValues )
{
	CRopeKeyframe *pRope = CRopeKeyframe::Create( 
		pEnt,
		pEnt,
		pEnt->LookupAttachment( pValues->GetString( "StartAttachment" ) ),
		pEnt->LookupAttachment( pValues->GetString( "EndAttachment" ) ),
		pValues->GetFloat( "Width", 0.5 ),
		pValues->GetString( "Material" ),
		pValues->GetInt( "NumSegments" ),
		0 );

	if ( pRope )
	{
		if ( pValues->GetInt( "Gravity", 1 ) == 0 )
		{
			pRope->m_RopeFlags |= ROPE_NO_GRAVITY;
		}

		pRope->m_RopeLength = pValues->GetInt( "Length" );
		pRope->m_TextureScale = pValues->GetFloat( "TextureScale", pRope->m_TextureScale );
		pRope->m_Slack = 0;
		pRope->m_RopeFlags |= ROPE_SIMULATE;
	}

	return pRope;
}

int CRopeKeyframe::GetRopesIntersectingAABB( CRopeKeyframe **pRopes, int nMaxRopes, const Vector &vAbsMin, const Vector &vAbsMax )
{
	if ( nMaxRopes == 0 )
		return 0;
	
	int nRopes = 0;
	for ( CRopeKeyframe *pRope : g_Ropes )
	{
		Vector v1, v2;
		if ( pRope->GetEndPointPos( 0, v1 ) && pRope->GetEndPointPos( 1, v2 ) )
		{
			if ( IsBoxIntersectingRay( v1, v2-v1, vAbsMin, vAbsMax, 0.1f ) )
			{
				pRopes[nRopes++] = pRope;
				if ( nRopes == nMaxRopes )
					break;
			}
		}
	}

	return nRopes;
}

void CRopeKeyframe::SetSlack( int slack )
{
	m_Slack = slack;
	RecomputeSprings();
}

void CRopeKeyframe::SetRopeFlags( int flags )
{
	m_RopeFlags = flags;
	UpdateVisibility();
}

void CRopeKeyframe::AddRopeFlags( int flags )
{
	m_RopeFlags |= flags;
	UpdateVisibility();
}
	
int CRopeKeyframe::GetRopeFlags() const
{
	return m_RopeFlags;
}

void CRopeKeyframe::SetupHangDistance( float flHangDist )
{
	CBaseEntity *pEnt1 = m_hStartPoint;
	CBaseEntity *pEnt2 = m_hEndPoint;
	if ( !pEnt1 || !pEnt2 )
		return;

	QAngle dummyAngles;

	// Calculate starting conditions so we can force it to hang down N inches.
	Vector v1 = pEnt1->GetAbsOrigin();
	pEnt1->GetAttachment( m_iStartAttachment, v1, dummyAngles );
		
	Vector v2 = pEnt2->GetAbsOrigin();
	pEnt2->GetAttachment( m_iEndAttachment, v2, dummyAngles );

	float flSlack, flLen;
	CalcRopeStartingConditions( v1, v2, ROPE_MAX_SEGMENTS, flHangDist, &flLen, &flSlack );

	m_RopeLength = (int)flLen;
	m_Slack = (int)flSlack;

	RecomputeSprings();
}

void CRopeKeyframe::SetStartEntity( CBaseEntity *pEnt )
{
	m_hStartPoint = pEnt;
}

void CRopeKeyframe::SetEndEntity( CBaseEntity *pEnt )
{
	m_hEndPoint = pEnt;
}

CBaseEntity* CRopeKeyframe::GetStartEntity() const
{
	return m_hStartPoint;
}

CBaseEntity* CRopeKeyframe::GetEndEntity() const
{
	return m_hEndPoint;
}

CSimplePhysics::IHelper* CRopeKeyframe::HookPhysics( CSimplePhysics::IHelper *pHook )
{
	m_RopePhysics.SetDelegate( pHook );
	return &m_PhysicsDelegate;
}

void CRopeKeyframe::SetColorMod( const Vector &vColorMod )
{
	m_vColorMod = vColorMod;
}

void CRopeKeyframe::RecomputeSprings()
{
	m_RopePhysics.ResetSpringLength(
		(m_RopeLength + m_Slack + ROPESLACK_FUDGEFACTOR) / (m_RopePhysics.NumNodes() - 1) );
}

void CRopeKeyframe::ShakeRope( const Vector &vCenter, float flRadius, float flMagnitude )
{
	// Sum up whatever it would apply to all of our points.
	bool bWantsThink = false;
	for ( int i=0; i < m_nSegments; i++ )
	{
		CSimplePhysics::CNode *pNode = m_RopePhysics.GetNode( i );

		float flDist = (pNode->m_vPos - vCenter).Length();
	
		float flShakeAmount = 1.0f - flDist / flRadius;
		if ( flShakeAmount >= 0 )
		{
			m_vecImpulse.z += flShakeAmount * flMagnitude;
			bWantsThink = true;
		}
	}

	if ( bWantsThink )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void CRopeKeyframe::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	m_bNewDataThisFrame = true;
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	if( updateType != DATA_UPDATE_CREATED )
		return;

	// Figure out the material name.
	char str[512];
	const model_t *pModel = modelinfo->GetModel( m_iRopeMaterialModelIndex );
	if ( pModel )
	{
		V_strncpy( str, modelinfo->GetModelName( pModel ), sizeof( str ) );

		// Get rid of the extension because the material system doesn't want it.
		char *pExt = V_stristr( str, ".vmt" );
		if ( pExt )
			pExt[0] = 0;
	}
	else
	{
		V_strncpy( str, "missing_rope_material", sizeof( str ) );
	}
	
	FinishInit( str );
}

void CRopeKeyframe::FinishInit( const char *pMaterialName )
{
	// Get the material from the material system.	
	m_pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER );

	if ( !g_pSplineCableShadowdepth )
	{
		g_pSplineCableShadowdepth = g_pMaterialSystem->FindMaterial( "cable/rope_shadowdepth", TEXTURE_GROUP_OTHER );
		g_pSplineCableShadowdepth->IncrementReferenceCount();
	}

	if( m_pMaterial )
		m_TextureHeight = m_pMaterial->GetMappingHeight();
	else
		m_TextureHeight = 1;

	// Init rope physics.
	m_nSegments = clamp( m_nSegments, 2, ROPE_MAX_SEGMENTS );
	m_RopePhysics.SetNumNodes( m_nSegments );

	SetCollisionBounds( Vector( -10, -10, -10 ), Vector( 10, 10, 10 ) );

	// We want to think every frame.
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CRopeKeyframe::RunRopeSimulation( float flSeconds )
{
	// First, forget about links touching things.
	for ( int i=0; i < m_nSegments; i++ )
		m_LinksTouchingSomething[i] = false;

	// Simulate, and it will mark which links touched things.
	m_RopePhysics.Simulate( flSeconds );

	// Now count how many links touched something.
	m_nLinksTouchingSomething = 0;
	for ( int i=0; i < m_nSegments; i++ )
	{
		if ( m_LinksTouchingSomething[i] )
			++m_nLinksTouchingSomething;
	}
}

Vector CRopeKeyframe::ConstrainNode( const Vector &vNormal, const Vector &vNodePosition, const Vector &vMidpiont, float fNormalLength )
{
	// Get triangle edges formed
	Vector vMidpointToNode = vNodePosition - vMidpiont;
	Vector vMidpointToNodeProjected = vMidpointToNode.Dot( vNormal ) * vNormal;
	float fMidpointToNodeLengh = VectorNormalize( vMidpointToNode );
	float fMidpointToNodeProjectedLengh = VectorNormalize( vMidpointToNodeProjected );

	// See if it's past an endpoint
	if ( fMidpointToNodeProjectedLengh < fNormalLength + 1.0f )
		return vNodePosition;

	// Apply the ratio between the triangles
	return vMidpiont + vMidpointToNode * fMidpointToNodeLengh * ( fNormalLength / fMidpointToNodeProjectedLengh );
}

void CRopeKeyframe::ConstrainNodesBetweenEndpoints( void )
{
	if ( !m_bConstrainBetweenEndpoints )
		return;

	// Get midpoint and normals
	Vector vMidpiont = ( m_vCachedEndPointAttachmentPos[ 0 ] + m_vCachedEndPointAttachmentPos[ 1 ] ) / 2.0f;
	Vector vNormal = vMidpiont - m_vCachedEndPointAttachmentPos[ 0 ];
	float fNormalLength = VectorNormalize( vNormal );

	// Loop through all the middle segments and ensure their positions are constrained between the endpoints
	for ( int i = 1; i < m_RopePhysics.NumNodes() - 1; ++i )
	{
		// Fix the current position
		m_RopePhysics.GetNode( i )->m_vPos = ConstrainNode( vNormal, m_RopePhysics.GetNode( i )->m_vPos, vMidpiont, fNormalLength );

		// Fix the predicted position
		m_RopePhysics.GetNode( i )->m_vPredicted = ConstrainNode( vNormal, m_RopePhysics.GetNode( i )->m_vPredicted, vMidpiont, fNormalLength );
	}
}

void CRopeKeyframe::ClientThink()
{
	// Only recalculate the endpoint attachments once per frame.
	m_bEndPointAttachmentPositionsDirty = true;
	m_bEndPointAttachmentAnglesDirty = true;
	
	// update the holiday lights here even if they aren't simulated
	if ( m_ropeType == ROPE_TYPE_DEFAULT )
		UpdateHolidayLights();

	if( !InitRopePhysics() ) // init if not already
		return;

	if( !r_drawropes.GetBool() )
		return;

	if ( DetectRestingState( m_bApplyWind ) )
	{
		if ( ( m_RopeFlags & ROPE_USE_WIND ) == 0 )
		{
			SetNextClientThink( CLIENT_THINK_NEVER );
		}
		return;
	}

	// Update the simulation.
	RunRopeSimulation( gpGlobals->frametime );

	g_nRopePointsSimulated += m_RopePhysics.NumNodes();

	m_bNewDataThisFrame = false;

	// Setup a new wind gust?
	if ( m_bApplyWind )
	{
		m_flCurrentGustTimer += gpGlobals->frametime;
		m_flTimeToNextGust -= gpGlobals->frametime;
		if( m_flTimeToNextGust <= 0 )
		{
			m_vWindDir = RandomVector( -1, 1 );
			VectorNormalize( m_vWindDir );

			static float basicScale = 50;
			m_vWindDir *= basicScale;
			m_vWindDir *= RandomFloat( -1.0f, 1.0f );
			
			m_flCurrentGustTimer = 0;
			m_flCurrentGustLifetime = RandomFloat( 2.0f, 3.0f );

			m_flTimeToNextGust = RandomFloat( 3.0f, 4.0f );
		}
	}

	UpdateBBox();
}

int CRopeKeyframe::DrawModel( int flags, const RenderableInstance_t &instance )
{
	CVProfScope _( "CRopeKeyframe::DrawModel", 0, VPROF_BUDGETGROUP_ROPES, false, BUDGETFLAG_OTHER );
	if( !InitRopePhysics() )
		return 0;

	if ( !m_bReadyToDraw )
		return 0;

	// Resize the rope
	if( m_RopeFlags & ROPE_RESIZE )
	{
		RecomputeSprings();
	}

	// If our start & end entities have models, but are nodraw, then we don't draw
	if ( m_hStartPoint && m_hStartPoint->IsDormant() && m_hEndPoint && m_hEndPoint->IsDormant() )
	{
		// Check models because rope endpoints are point entities
		if ( m_hStartPoint->GetModelIndex() && m_hEndPoint->GetModelIndex() )
			return 0;
	}

	ConstrainNodesBetweenEndpoints();

	RopeManager()->AddToRenderCache( this );
	return 1;
}

bool CRopeKeyframe::ShouldDraw()
{
	if( !r_ropetranslucent.GetBool() ) 
		return false;

	if( !(m_RopeFlags & ROPE_SIMULATE) )
		return false;

	CPULevel_t nCPULevel = GetCPULevel();
	bool bNoDraw = ( GetMinCPULevel() && GetMinCPULevel()-1 > nCPULevel );
	bNoDraw = bNoDraw || ( GetMaxCPULevel() && GetMaxCPULevel()-1 < nCPULevel );
	if ( bNoDraw )
		return false;

	GPULevel_t nGPULevel = GetGPULevel();
	bNoDraw = ( GetMinGPULevel() && GetMinGPULevel()-1 > nGPULevel );
	bNoDraw = bNoDraw || ( GetMaxGPULevel() && GetMaxGPULevel()-1 < nGPULevel );
	if ( bNoDraw )
		return false;

	return true;
}

const Vector& CRopeKeyframe::WorldSpaceCenter( ) const
{
	return GetAbsOrigin();
}

bool CRopeKeyframe::GetAttachment( int number, matrix3x4_t &matrix )
{
	int nNodes = m_RopePhysics.NumNodes();
	if ( (number != ROPE_ATTACHMENT_START_POINT && number != ROPE_ATTACHMENT_END_POINT) || nNodes < 2 )
		return false;

	// Now setup the orientation based on the last segment.
	Vector vForward, origin;
	if ( number == ROPE_ATTACHMENT_START_POINT )
	{
		origin = m_RopePhysics.GetNode( 0 )->m_vPredicted;
		vForward = m_RopePhysics.GetNode( 0 )->m_vPredicted - m_RopePhysics.GetNode( 1 )->m_vPredicted;
	}
	else
	{
		origin = m_RopePhysics.GetNode( nNodes-1 )->m_vPredicted;
		vForward = m_RopePhysics.GetNode( nNodes-1 )->m_vPredicted - m_RopePhysics.GetNode( nNodes-2 )->m_vPredicted;
	}
	VectorMatrix( vForward, matrix );
	PositionMatrix( origin, matrix );
	return true;
}

bool CRopeKeyframe::GetAttachment( int number, Vector &origin )
{
	int nNodes = m_RopePhysics.NumNodes();
	if ( (number != ROPE_ATTACHMENT_START_POINT && number != ROPE_ATTACHMENT_END_POINT) || nNodes < 2 )
		return false;

	// Now setup the orientation based on the last segment.
	if ( number == ROPE_ATTACHMENT_START_POINT )
	{
		origin = m_RopePhysics.GetNode( 0 )->m_vPredicted;
	}
	else
	{
		origin = m_RopePhysics.GetNode( nNodes-1 )->m_vPredicted;
	}
	return true;
}

bool CRopeKeyframe::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	contract_assert( 0 );
	return false;
}

bool CRopeKeyframe::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	int nNodes = m_RopePhysics.NumNodes();
	if ( (number == ROPE_ATTACHMENT_START_POINT || number == ROPE_ATTACHMENT_END_POINT) && nNodes >= 2 )
	{
		// Now setup the orientation based on the last segment.
		Vector vForward;
		if ( number == ROPE_ATTACHMENT_START_POINT )
		{
			origin = m_RopePhysics.GetNode( 0 )->m_vPredicted;
			vForward = m_RopePhysics.GetNode( 0 )->m_vPredicted - m_RopePhysics.GetNode( 1 )->m_vPredicted;
		}
		else
		{
			origin = m_RopePhysics.GetNode( nNodes-1 )->m_vPredicted;
			vForward = m_RopePhysics.GetNode( nNodes-1 )->m_vPredicted - m_RopePhysics.GetNode( nNodes-2 )->m_vPredicted;
		}
		VectorAngles( vForward, angles );

		return true;
	}
	
	return false;
}

bool CRopeKeyframe::AnyPointsMoved()
{
	int nNodeCount = m_RopePhysics.NumNodes();
	for( int i=0; i < nNodeCount; i++ )
	{
		CSimplePhysics::CNode *pNode = m_RopePhysics.GetNode( i );
		float flMoveDistSqr = pNode->m_vPos.DistToSqr( pNode->m_vPrevPos );
		if( flMoveDistSqr > 0.25f )
		{
			if ( m_iForcePointMoveCounter < 5 )
			{
				m_iForcePointMoveCounter = 5;
			}
			return true;
		}
	}

	if( m_iForcePointMoveCounter >= 0 )
	{
		--m_iForcePointMoveCounter;
		return true;
	}

	return false;
}

bool CRopeKeyframe::DidEndPointMove( int iPt )
{
	// If this point isn't locked anyway, just break out.
	if( !( m_fLockedPoints & (1 << iPt) ) )
		return false;

	bool bOld = m_bPrevEndPointPos[iPt];
	Vector vOld = m_vPrevEndPointPos[iPt];

	m_bPrevEndPointPos[iPt] = GetEndPointPos( iPt, m_vPrevEndPointPos[iPt] );
	
	// If it wasn't and isn't attached to anything, don't register a change.
	if( !bOld && !m_bPrevEndPointPos[iPt] )
		return true;

	// Register a change if the endpoint moves.
	if( !VectorsAreEqual( vOld, m_vPrevEndPointPos[iPt], 0.1 ) )
		return true;

	return false;
}

bool CRopeKeyframe::DetectRestingState( bool &bApplyWind )
{
	bApplyWind = false;

	if( m_fPrevLockedPoints != m_fLockedPoints )
	{
		// Force it to move the points for some number of frames when they get detached or
		// after we get new data. This allows them to accelerate from gravity.
		m_iForcePointMoveCounter = 10; 
		m_fPrevLockedPoints = m_fLockedPoints;
		return false;
	}

	if( m_bNewDataThisFrame )
	{
		// Simulate if anything about us changed this frame, such as our position due to hierarchy.
		// FIXME: this won't work when hierarchy is client side
		return false;
	}

	// Make sure our attachment points haven't moved.
	if( DidEndPointMove( 0 ) || DidEndPointMove( 1 ) )
		return false;

	// See how close we are to the line.
	Vector &vEnd1 = m_RopePhysics.GetFirstNode()->m_vPos;
	Vector &vEnd2 = m_RopePhysics.GetLastNode()->m_vPos;
	
	if ( m_RopeFlags & ROPE_USE_WIND )
	{
		// Don't apply wind if more than half of the nodes are touching something.
		float flDist1 = std::numeric_limits<float>::max();
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			// ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			float d = CalcDistanceToLineSegment( MainViewOrigin( hh ), vEnd1, vEnd2 );
			if ( d < flDist1 )
			{
				flDist1 = d;
			}
		}
		if( m_nLinksTouchingSomething < (m_RopePhysics.NumNodes() >> 1) )
		{
			bApplyWind = flDist1 < rope_wind_dist.GetFloat();
		}
	}

	if ( m_vecPreviousImpulse != m_vecImpulse )
	{
		m_vecPreviousImpulse = m_vecImpulse;
		return false;
	}

	return !AnyPointsMoved() && !bApplyWind && !rope_shake.GetInt();
}

// simple struct to precompute basis for catmull rom splines for faster evaluation
struct catmull_t
{
	Vector t3;
	Vector t2;
	Vector t;
	Vector c;
};

// bake out the terms of the catmull rom spline
void Catmull_Rom_Spline_Matrix( const Vector &p1, const Vector &p2, const Vector &p3, const Vector &p4, catmull_t &output )
{
	output.t3 = 0.5f * ((-1*p1) + (3*p2) + (-3*p3) + p4);	// 0.5 t^3 * [ (-1*p1) + ( 3*p2) + (-3*p3) + p4 ]
	output.t2 = 0.5f * ((2*p1) + (-5*p2) + (4*p3) - p4);		// 0.5 t^2 * [ ( 2*p1) + (-5*p2) + ( 4*p3) - p4 ]
	output.t = 0.5f * ((-1*p1) + p3);						// 0.5 t * [ (-1*p1) + p3 ]
	output.c = p2;											// p2
}

// evaluate one point on the spline, t is a vector of (t, t^2, t^3)
inline void Catmull_Rom_Eval( const catmull_t &spline, const Vector &t, Vector &output )
{
	contract_assert( spline.c.IsValid() );
	contract_assert( spline.t.IsValid() );
	contract_assert( spline.t2.IsValid() );
	contract_assert( spline.t3.IsValid() );
	output = spline.c + (t.x * spline.t) + (t.y*spline.t2) + (t.z * spline.t3);
}

void CRopeKeyframe::UpdateBBox()
{
	Vector &vStart = m_RopePhysics.GetFirstNode()->m_vPos;
	Vector &vEnd   = m_RopePhysics.GetLastNode()->m_vPos;

	Vector mins, maxs;

	VectorMin( vStart, vEnd, mins );
	VectorMax( vStart, vEnd, maxs );

	for( int i=1; i < m_RopePhysics.NumNodes()-1; i++ )
	{
		const Vector &vPos = m_RopePhysics.GetNode(i)->m_vPos;
		AddPointToBounds( vPos, mins, maxs );
	}
	
	mins -= GetAbsOrigin();
	maxs -= GetAbsOrigin();
	SetCollisionBounds( mins, maxs );
}

bool CRopeKeyframe::InitRopePhysics()
{
	if( !(m_RopeFlags & ROPE_SIMULATE) )
		return 0;

	if( m_bPhysicsInitted )
	{
		return true;
	}

	// Must have both entities to work.
	m_bPrevEndPointPos[0] = GetEndPointPos( 0, m_vPrevEndPointPos[0] );
	if( !m_bPrevEndPointPos[0] )
		return false;

	// They're allowed to not have an end attachment point so the rope can dangle.
	m_bPrevEndPointPos[1] = GetEndPointPos( 1, m_vPrevEndPointPos[1] );
	if( !m_bPrevEndPointPos[1] )
		m_vPrevEndPointPos[1] = m_vPrevEndPointPos[0];

	const Vector &vStart = m_vPrevEndPointPos[0];
	const Vector &vAttached = m_vPrevEndPointPos[1];

	m_RopePhysics.SetupSimulation( 0, &m_PhysicsDelegate );
	RecomputeSprings();
	m_RopePhysics.Restart();

	// Initialize the positions of the nodes.
	for( int i=0; i < m_RopePhysics.NumNodes(); i++ )
	{
		CSimplePhysics::CNode *pNode = m_RopePhysics.GetNode( i );
		float t = (float)i / (m_RopePhysics.NumNodes() - 1);
		
		VectorLerp( vStart, vAttached, t, pNode->m_vPos );
		pNode->m_vPrevPos = pNode->m_vPos;
	}

	// Simulate for a bit to let it sag.
	if ( m_RopeFlags & ROPE_INITIAL_HANG )
	{
		RunRopeSimulation( 5 );
	}

	CalcLightValues();

	// Set our bounds for visibility.
	UpdateBBox();

	m_flTimeToNextGust = RandomFloat( 1.0f, 3.0f );
	m_bPhysicsInitted = true;
	
	return true;
}

bool CRopeKeyframe::CalculateEndPointAttachment( CBaseEntity *pEnt, int iAttachment, Vector &vPos, QAngle *pAngles )
{
	CVProfScope _( "CRopeKeyframe::CalculateEndPointAttachment", 0, VPROF_BUDGETGROUP_ROPES, false, BUDGETFLAG_OTHER );

	if( !pEnt )
		return false;

	if ( m_RopeFlags & ROPE_PLAYER_WPN_ATTACH )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEnt );
		if ( pPlayer )
		{
			CBaseAnimating *pModel = pPlayer->GetRenderedWeaponModel();
			if ( !pModel )
				return false;

			int iAttachment = pModel->LookupAttachment( "buff_attach" );
			if ( pAngles )
				return pModel->GetAttachment( iAttachment, vPos, *pAngles );
			return pModel->GetAttachment( iAttachment, vPos );
		}
	}

	if( iAttachment > 0 )
	{
		bool bOk;
		if ( pAngles )
		{
			bOk = pEnt->GetAttachment( iAttachment, vPos, *pAngles );
		}
		else
		{
			bOk = pEnt->GetAttachment( iAttachment, vPos );
		}
		if ( bOk )
			return true;
	}

	vPos = pEnt->WorldSpaceCenter( );
	if ( pAngles )
	{
		*pAngles = pEnt->GetAbsAngles();
	}
	return true;
}

bool CRopeKeyframe::GetEndPointPos( int iPt, Vector &vPos )
{
	// By caching the results here, we avoid doing this a bunch of times per frame.
	if ( m_bEndPointAttachmentPositionsDirty )
	{
		CalculateEndPointAttachment( m_hStartPoint, m_iStartAttachment, m_vCachedEndPointAttachmentPos[0], nullptr );
		CalculateEndPointAttachment( m_hEndPoint, m_iEndAttachment, m_vCachedEndPointAttachmentPos[1], nullptr );
		m_bEndPointAttachmentPositionsDirty = false;
	}

	contract_assert( iPt == 0 || iPt == 1 );
	vPos = m_vCachedEndPointAttachmentPos[iPt];
	return true;
}

bool CRopeKeyframe::GetEndPointAttachment( int iPt, Vector &vPos, QAngle &angle )
{
	// By caching the results here, we avoid doing this a bunch of times per frame.
	if ( m_bEndPointAttachmentPositionsDirty || m_bEndPointAttachmentAnglesDirty )
	{
		CalculateEndPointAttachment( m_hStartPoint, m_iStartAttachment, m_vCachedEndPointAttachmentPos[0], &m_vCachedEndPointAttachmentAngle[0] );
		CalculateEndPointAttachment( m_hEndPoint, m_iEndAttachment, m_vCachedEndPointAttachmentPos[1], &m_vCachedEndPointAttachmentAngle[1] );
		m_bEndPointAttachmentPositionsDirty = false;
		m_bEndPointAttachmentAnglesDirty = false;
	}

	contract_assert( iPt == 0 || iPt == 1 );
	vPos = m_vCachedEndPointAttachmentPos[iPt];
	angle = m_vCachedEndPointAttachmentAngle[iPt];
	return true;
}

void CRopeKeyframe::UpdateHolidayLights( void )
{
	if ( !RopeManager()->IsHolidayLightMode() )
		return;

	if ( ( gpGlobals->curtime != g_RopeDelayedEffects.m_flTimeProcessedOnMainThread ) && std::ssize( g_RopeDelayedEffects.m_arrEffects ) )
	{
		g_RopeDelayedEffects.m_flTimeProcessedOnMainThread = gpGlobals->curtime;
		std::scoped_lock lock( g_RopeDelayedEffects.m_mtx );
		for ( const auto &effect : g_RopeDelayedEffects.m_arrEffects )
		{
			DispatchEffect( "CS_HolidayLight", effect );
		}
		g_RopeDelayedEffects.m_arrEffects.clear();
	}
}

void CRopeKeyframe::CalcLightValues()
{
	Vector boxColors[6];

	for( int i=0; i < m_RopePhysics.NumNodes(); i++ )
	{
		const Vector &vPos = m_RopePhysics.GetNode(i)->m_vPredicted;
		engine->ComputeLighting( vPos, nullptr, true, m_LightValues[i], boxColors );

		if ( !rope_averagelight.GetInt() )
		{
			// The engine averages the lighting across the 6 box faces, but we would rather just get the MAX intensity
			// since we do our own half-lambert lighting in the rope shader to simulate directionality.
			//
			// So here, we take the average of all the incoming light, and scale it to use the max intensity of all the box sides.
			float flMaxIntensity = 0;
			for ( int iSide=0; iSide < 6; iSide++ )
			{
				float flLen = boxColors[iSide].Length();
				flMaxIntensity = std::max( flMaxIntensity, flLen );
			}

			VectorNormalize( m_LightValues[i] );
			m_LightValues[i] *= flMaxIntensity;
			float flMax = std::max( m_LightValues[i].x, std::max( m_LightValues[i].y, m_LightValues[i].z ) );
			if ( flMax > 1 )
				m_LightValues[i] /= flMax;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CRopeKeyframe::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	// Read instantaneous fore data
	m_vecImpulse.x   = msg.ReadFloat();
	m_vecImpulse.y   = msg.ReadFloat();
	m_vecImpulse.z   = msg.ReadFloat();
}

#undef OUTPUT_2SPLINE_VERTS
#undef OUTPUT_SPLINE_QUAD

class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Rumble effects mixer for XBox

void StopAllRumbleEffects( int userID );

ConVar cl_rumblescale( side_registry(), "cl_rumblescale", "1.0", FCVAR_ARCHIVE | FCVAR_SS, "Scale sensitivity of rumble effects (0 to 1.0)" ); 
ConVar cl_debugrumble( side_registry(), "cl_debugrumble", "0", FCVAR_ARCHIVE, "Turn on rumble debugging spew" );

static constexpr auto MAX_RUMBLE_CHANNELS = 3; // Max concurrent rumble effects per player
static constexpr auto NUM_WAVE_SAMPLES = 30; // Effects play at 10hz

typedef struct
{
	float			amplitude_left[NUM_WAVE_SAMPLES];
	float			amplitude_right[NUM_WAVE_SAMPLES];
	int				numSamples;
} RumbleWaveform_t;

//=========================================================
// Structure for a rumble effect channel. This is akin to
// a sound channel that is playing a sound.
//=========================================================
typedef struct
{
	float			starttime;			// When did this effect start playing? (gpGlobals->curtime)
	int				waveformIndex;		// Type of effect waveform used (an enum from rumble_shared.h)
	int				priority;			// How important this effect is (for making replacement decisions)
	bool			in_use;				// Is this channel in use?? (true if effect is currently playing, false if done or otherwise available)
	unsigned char	rumbleFlags;		// Flags pertaining to the effect currently playing on this channel.
	float			scale;				// Some effects are updated while they are running.
} RumbleChannel_t;

//=========================================================
// This structure contains parameters necessary to generate
// a sine or sawtooth waveform. 
//=========================================================
typedef struct tagWaveGenParams
{
	float	cycles;				// AKA frequency
	float	amplitudescale;	
	bool	leftChannel;		// If false, generating for the right channel

	float	maxAmplitude;		// Clamping
	float	minAmplitude;

	void Set( float c_cycles, float c_amplitudescale, bool c_leftChannel, float c_minAmplitude, float c_maxAmplitude )
	{
		cycles = c_cycles;
		amplitudescale = c_amplitudescale;
		leftChannel = c_leftChannel;
		minAmplitude = c_minAmplitude;
		maxAmplitude = c_maxAmplitude;
	}

	// CTOR
	tagWaveGenParams( float c_cycles, float c_amplitudescale, bool c_leftChannel, float c_minAmplitude, float c_maxAmplitude ) 
	{
		Set( c_cycles, c_amplitudescale, c_leftChannel, c_minAmplitude, c_maxAmplitude );
	}

} WaveGenParams_t;

//---------------------------------------------------------
//---------------------------------------------------------
void TerminateWaveform( RumbleWaveform_t *pWaveform, int samples )
{
	if( samples <= NUM_WAVE_SAMPLES )
	{
		pWaveform->numSamples = samples;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void EaseInWaveform( RumbleWaveform_t *pWaveform, int samples, bool left )
{
	float step = 1.0f / ((float)samples);
	float factor = 0.0f;

	for( int i = 0 ; i < samples ; i++ )
	{
		if( left )
		{
			pWaveform->amplitude_left[i] *= factor;
		}
		else
		{
			pWaveform->amplitude_right[i] *= factor;
		}

		factor += step;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void EaseOutWaveform( RumbleWaveform_t *pWaveform, int samples, bool left )
{
	float step = 1.0f / ((float)samples);
	float factor = 0.0f;

	int i = NUM_WAVE_SAMPLES - 1;

	for( int j = 0 ; j < samples ; j++ )
	{
		if( left )
		{
			pWaveform->amplitude_left[i] *= factor;
		}
		else
		{
			pWaveform->amplitude_right[i] *= factor;
		}

		factor += step;
		i--;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void GenerateSawtoothEffect( RumbleWaveform_t *pWaveform, const WaveGenParams_t &params )
{
	float delta = params.maxAmplitude - params.minAmplitude;
	int waveLength = NUM_WAVE_SAMPLES / params.cycles;
	float vstep = (delta / waveLength);

	float amplitude = params.minAmplitude;

	for( int i = 0 ; i < NUM_WAVE_SAMPLES ; i++ )
	{
		if( params.leftChannel )
		{
			pWaveform->amplitude_left[i] = amplitude;
		}
		else
		{
			pWaveform->amplitude_right[i] = amplitude;
		}

		amplitude += vstep;

		if( amplitude > params.maxAmplitude )
		{
			amplitude = params.minAmplitude;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void GenerateSquareWaveEffect( RumbleWaveform_t *pWaveform, const WaveGenParams_t &params )
{
	int i = 0;
	int j;

	int steps = ((float)NUM_WAVE_SAMPLES) / (params.cycles*2.0f);

	if ( steps <= 0 )
		return;

	while( i < NUM_WAVE_SAMPLES )
	{
		for( j = 0 ; j < steps && i < NUM_WAVE_SAMPLES ; j++ )
		{
			if( params.leftChannel )
			{
				pWaveform->amplitude_left[i++] = params.minAmplitude;
			}
			else
			{
				pWaveform->amplitude_right[i++] = params.minAmplitude;
			}
		}
		for( j = 0 ; j < steps && i < NUM_WAVE_SAMPLES ; j++ )
		{
			if( params.leftChannel )
			{
				pWaveform->amplitude_left[i++] = params.maxAmplitude;
			}
			else
			{
				pWaveform->amplitude_right[i++] = params.maxAmplitude;
			}
		}
	}
}

//---------------------------------------------------------
// If you pass a numSamples, this wave will only be that many
// samples long.
//---------------------------------------------------------
void GenerateFlatEffect( RumbleWaveform_t *pWaveform, const WaveGenParams_t &params )
{
	for( int i = 0 ; i < NUM_WAVE_SAMPLES ; i++ )
	{
		if( params.leftChannel )
		{
			pWaveform->amplitude_left[i] = params.maxAmplitude;
		}
		else
		{
			pWaveform->amplitude_right[i] = params.maxAmplitude;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void GenerateSineWaveEffect( RumbleWaveform_t *pWaveform, const WaveGenParams_t &params )
{
	float step = (360.0f * (params.cycles * 0.5f) ) / ((float)NUM_WAVE_SAMPLES);
	float degrees = 180.0f + step; // 180 to start at 0

	for( int i = 0 ; i < NUM_WAVE_SAMPLES ; i++ )
	{
		float radians = DEG2RAD(degrees);
		float value = fabs( std::sin(radians) );

		value *= params.amplitudescale;

		if( value < params.minAmplitude )
			value = params.minAmplitude;

		if( value > params.maxAmplitude )
			value = params.maxAmplitude;

		if( params.leftChannel )
		{
			pWaveform->amplitude_left[i] = value;
		}
		else
		{
			pWaveform->amplitude_right[i] = value;
		}

		degrees += step;
	}
}

//=========================================================
//=========================================================
class CRumbleEffects
{
public:
	CRumbleEffects()
	{
		Init();
	}

	void Init();
	void SetOutputEnabled( bool bEnable );
	void StartEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags );
	void StopEffect( int userID, int effectIndex );
	void StopAllEffects( int userID );
	void ComputeAmplitudes( RumbleChannel_t *pChannel, float curtime, float *pLeft, float *pRight );
	void UpdateEffects( int userID, float curtime );
	void UpdateScreenShakeRumble( int userID, float shake, float balance );

	RumbleChannel_t *FindExistingChannel( int userID, int index );
	RumbleChannel_t	*FindAvailableChannel( int userID, int priority );

public:
	RumbleChannel_t	m_Channels[ MAX_SPLITSCREEN_PLAYERS ][ MAX_RUMBLE_CHANNELS ];

	RumbleWaveform_t m_Waveforms[ NUM_RUMBLE_EFFECTS ];

	float	m_flScreenShake[ MAX_SPLITSCREEN_PLAYERS ];
	bool	m_bOutputEnabled;
};

CRumbleEffects g_RumbleEffects;

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::Init()
{
	SetOutputEnabled( true );

	int userID,channel;
	for( userID = 0 ; userID < MAX_SPLITSCREEN_PLAYERS ; userID++ )
	{
		for( channel = 0 ; channel < MAX_RUMBLE_CHANNELS ; channel++ )
		{
			m_Channels[userID][channel].in_use = false;
			m_Channels[userID][channel].priority = 0;
		}
	}

	// Every effect defaults to this many samples. Call TerminateWaveform() to trim these.
	for ( int i = 0 ; i < NUM_RUMBLE_EFFECTS ; i++ )
	{
		m_Waveforms[i].numSamples = NUM_WAVE_SAMPLES;
	}

	// Jeep Idle
	WaveGenParams_t params( 1, 1.0f, false, 0.0f, 0.15f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_JEEP_ENGINE_LOOP], params );

	// Pistol
	params.Set( 1, 1.0f, false, 0.0f, 0.6f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_PISTOL], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_PISTOL], 1 );

	// SMG1
	params.Set( 1, 1.0f, true, 0.0f, 0.4f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SMG1], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SMG1], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_SMG1], 1 );

	// AR2
	params.Set( 1, 1.0f, true, 0.0f, 0.55f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_AR2], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_AR2], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_AR2], 1 );

	// AR2 Alt
	params.Set( 1, 1.0f, true, 0.0, 0.5f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_AR2_ALT_FIRE], params );
	EaseInWaveform( &m_Waveforms[RUMBLE_AR2_ALT_FIRE], 5, true );
	params.Set( 1, 1.0f, false, 0.0, 0.7f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_AR2_ALT_FIRE], params );
	EaseInWaveform( &m_Waveforms[RUMBLE_AR2_ALT_FIRE], 5, false );
	TerminateWaveform( &m_Waveforms[RUMBLE_AR2_ALT_FIRE], 7 );

	// 357
	params.Set( 1, 1.0f, true, 0.0f, 0.75f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_357], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.75f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_357], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_357], 2 );

	// Shotgun
	params.Set( 1, 1.0f, true, 0.0f, 0.7f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SHOTGUN_SINGLE], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.7f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SHOTGUN_SINGLE], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_SHOTGUN_SINGLE], 3 );

	params.Set( 1, 1.0f, true, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SHOTGUN_DOUBLE], params );
	params.Set( 1, 1.0f, false, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_SHOTGUN_DOUBLE], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_SHOTGUN_DOUBLE], 3 );

	// RPG Missile
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_RPG_MISSILE], params );
	EaseOutWaveform( &m_Waveforms[RUMBLE_RPG_MISSILE], 30, false );
	TerminateWaveform( &m_Waveforms[RUMBLE_RPG_MISSILE], 6 );

	// Physcannon open forks
	params.Set( 1, 1.0f, false, 0.0f, 0.25f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_PHYSCANNON_OPEN], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_PHYSCANNON_OPEN], 4 );

	// Physcannon holding something
	params.Set( 1, 1.0f, true, 0.0f, 0.2f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_PHYSCANNON_LOW], params );
	params.Set( 6, 1.0f, false, 0.0f, 0.25f );
	GenerateSquareWaveEffect( &m_Waveforms[RUMBLE_PHYSCANNON_LOW], params );

	// Crowbar
	params.Set( 1, 1.0f, false, 0.0f, 0.25f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_CROWBAR_SWING], params );
	EaseOutWaveform( &m_Waveforms[RUMBLE_CROWBAR_SWING], 30, false );
	TerminateWaveform( &m_Waveforms[RUMBLE_CROWBAR_SWING], 3 );

	// Airboat gun
	params.Set( 1, 1.0f, false, 0.0f, 0.4f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_AIRBOAT_GUN], params );
	params.Set( 12, 1.0f, true, 0.0f, 0.5f );
	GenerateSawtoothEffect( &m_Waveforms[RUMBLE_AIRBOAT_GUN], params );

	// Generic flat effects.
	params.Set( 1, 1.0f, true, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FLAT_LEFT], params );

	params.Set( 1, 1.0f, false, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FLAT_RIGHT], params );
	
	params.Set( 1, 1.0f, true, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FLAT_BOTH], params );
	params.Set( 1, 1.0f, false, 0.0f, 1.0f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FLAT_BOTH], params );

	// Impact from a long fall
	params.Set( 1, 1.0f, false, 0.0f, 0.5f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FALL_LONG], params );
	params.Set( 1, 1.0f, true, 0.0f, 0.5f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FALL_LONG], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_FALL_LONG], 3 );

	// Impact from a short fall
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FALL_SHORT], params );
	params.Set( 1, 1.0f, true, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_FALL_SHORT], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_FALL_SHORT], 2 );

	// Portalgun left (blue) shot
	params.Set( 1, 1.0f, true, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_PORTALGUN_LEFT], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_PORTALGUN_LEFT], 2 );

	// Portalgun right (red) shot
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_PORTALGUN_RIGHT], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_PORTALGUN_RIGHT], 2 );

	// Portal failed to place feedback
	params.Set( 4, 1.0f, true, 0.0f, 0.25f );
	GenerateSquareWaveEffect( &m_Waveforms[RUMBLE_PORTAL_PLACEMENT_FAILURE], params );
	params.Set( 4, 1.0f, false, 0.0f, 0.25f );
	GenerateSquareWaveEffect( &m_Waveforms[RUMBLE_PORTAL_PLACEMENT_FAILURE], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_PORTAL_PLACEMENT_FAILURE], 6 );

	// RUMBLE_DMG_LOW (copied from 357)
	params.Set( 1, 1.0f, true, 0.0f, 0.75f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_DMG_LOW], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.75f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_DMG_LOW], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_DMG_LOW], 2 );

	// RUMBLE_DMG_HIGH (copied from AR2)
	params.Set( 1, 1.0f, true, 0.0f, 0.5f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_DMG_HIGH], params );
	params.Set( 1, 1.0f, false, 0.0f, 0.3f );
	GenerateFlatEffect( &m_Waveforms[RUMBLE_DMG_HIGH], params );
	TerminateWaveform( &m_Waveforms[RUMBLE_DMG_HIGH], 1 );

}

//---------------------------------------------------------
//---------------------------------------------------------
RumbleChannel_t *CRumbleEffects::FindExistingChannel( int userID, int index )
{
	RumbleChannel_t *pChannel;

	for( int i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		pChannel = &m_Channels[userID][i];

		if( pChannel->in_use && pChannel->waveformIndex == index )
		{
			// This effect is already playing. Provide this channel for the 
			// effect to be re-started on.
			return pChannel;
		}
	}

	return nullptr;
}

//---------------------------------------------------------
// priority - the priority of the effect we want to play.
//---------------------------------------------------------
RumbleChannel_t	*CRumbleEffects::FindAvailableChannel( int userID, int priority )
{
	RumbleChannel_t *pChannel;
	int i;

	for( i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		pChannel = &m_Channels[userID][i];

		if( !pChannel->in_use )
		{
			return pChannel;
		}
	}

	int lowestPriority = priority;
	RumbleChannel_t	*pBestChannel = nullptr;
	float oldestChannel = std::numeric_limits<float>::max();

	// All channels already in use. Find a channel to slam. Make sure it belongs to this userID
	for( i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		pChannel = &m_Channels[userID][i];

		if( (pChannel->rumbleFlags & RUMBLE_FLAG_LOOP) )
			continue;

		if( pChannel->priority < lowestPriority )
		{
			// Always happily slam a lower priority sound.
			pBestChannel = pChannel;
			lowestPriority = pChannel->priority;
		}
		else if( pChannel->priority == lowestPriority )
		{
			// Priority is the same, so replace the oldest.
			if( pBestChannel )
			{
				// If we already have a channel of the same priority to discard, make sure we discard the oldest.
				float age = gpGlobals->curtime - pChannel->starttime;

				if( age > oldestChannel )
				{
					pBestChannel = pChannel;
					oldestChannel = age;
				}
			}
			else
			{
				// Take it.
				pBestChannel = pChannel;
				oldestChannel = gpGlobals->curtime - pChannel->starttime;
			}
		}
	}

	return pBestChannel; // Can still be NULL if we couldn't find a channel to slam.
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::SetOutputEnabled( bool bEnable )
{
	m_bOutputEnabled = bEnable;

	if( !bEnable )
	{
		// Tell the hardware to shut down motors right now, in case this gets called
		// and some other process blocks us before the next rumble system update.
		for( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS ; i++ )
		{
			m_flScreenShake[ i ] = 0.0f;
			StopAllRumbleEffects( i );
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::StartEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags )
{
	if( effectIndex == RUMBLE_STOP_ALL )
	{
		StopAllEffects( userID );
		return;
	}

	if( rumbleFlags & RUMBLE_FLAG_STOP )
	{
		StopEffect( userID, effectIndex );
		return;
	}

	if ( !input->ControllerModeActive() )
		return;

	int priority = 1;
	RumbleChannel_t *pChannel = nullptr;

	if( (rumbleFlags & RUMBLE_FLAG_RESTART) )
	{
		// Try to find any active instance of this effect and replace it.
		pChannel = FindExistingChannel( userID, effectIndex );
	}

	if( (rumbleFlags & RUMBLE_FLAG_ONLYONE) )
	{
		pChannel = FindExistingChannel( userID, effectIndex );

		if( pChannel )
		{
			// Bail out. An instance of this effect is already playing.
			return;
		}
	}

	if( (rumbleFlags & RUMBLE_FLAG_UPDATE_SCALE) )
	{
		pChannel = FindExistingChannel( userID, effectIndex );
		if( pChannel )
		{
			pChannel->scale = ((float)rumbleData) / 100.0f;
		}

		// It's possible to return without finding a rumble to update.
		// This means you tried to update a rumble you never started.
		return;
	}

	if( !pChannel )
	{
		pChannel = FindAvailableChannel( userID, priority );
	}

	if( pChannel )
	{
		pChannel->waveformIndex = effectIndex;
		pChannel->priority = 1;
		pChannel->starttime = gpGlobals->curtime;
		pChannel->in_use = true;
		pChannel->rumbleFlags = rumbleFlags;

		if( rumbleFlags & RUMBLE_FLAG_INITIAL_SCALE )
		{
			pChannel->scale = ((float)rumbleData) / 100.0f;
		}
		else
		{
			pChannel->scale = 1.0f;
		}
	}

	if( (rumbleFlags & RUMBLE_FLAG_RANDOM_AMPLITUDE) )
	{
		pChannel->scale = random_valve->RandomFloat( 0.1f, 1.0f );
	}
}

//---------------------------------------------------------
// Find all playing effects of this type and stop them.
//---------------------------------------------------------
void CRumbleEffects::StopEffect( int userID, int effectIndex )
{
	for( int i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		if( m_Channels[userID][i].in_use && m_Channels[userID][i].waveformIndex == effectIndex )
		{
			m_Channels[userID][i].in_use = false;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::StopAllEffects( int userID )
{
	for( int i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		m_Channels[userID][i].in_use = false;
	}

	m_flScreenShake[ userID ] = 0.0f;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::ComputeAmplitudes( RumbleChannel_t *pChannel, float curtime, float *pLeft, float *pRight )
{
	// How long has this waveform been playing?
	float elapsed = curtime - pChannel->starttime;
	
	if( elapsed >= (NUM_WAVE_SAMPLES/10) )
	{
		if( (pChannel->rumbleFlags & RUMBLE_FLAG_LOOP) )
		{
			// This effect loops. Just fixup the start time and recompute elapsed.
			pChannel->starttime = curtime;
			elapsed = curtime - pChannel->starttime;
		}
		else
		{
			// This effect is done! Should it loop?
			*pLeft = 0;
			*pRight = 0;
			pChannel->in_use = false;
			return;
		}
	}

	// Figure out which sample we're playing FROM. 
	int seconds = ((int) elapsed);
	int sample = (int)(elapsed*10.0f);

	// Get the fraction bit.
	float fraction, left, right;
	fraction = elapsed - seconds;

	if( sample == m_Waveforms[pChannel->waveformIndex].numSamples )
	{
		// This effect is done. Send zeroes to the mixer for this
		// final frame and then turn the channel off. (Unless it loops!)

		if( (pChannel->rumbleFlags & RUMBLE_FLAG_LOOP) )
		{
			// Loop this effect
			pChannel->starttime = gpGlobals->curtime;

			// Send the first sample.
			left = m_Waveforms[pChannel->waveformIndex].amplitude_left[0];
			right = m_Waveforms[pChannel->waveformIndex].amplitude_right[0];
		}
		else
		{
			left = 0.0f;
			right = 0.0f;
			pChannel->in_use = false;
		}
	}
	else
	{
		// Use values for the last sample that we have passed
		left = m_Waveforms[pChannel->waveformIndex].amplitude_left[sample];
		right = m_Waveforms[pChannel->waveformIndex].amplitude_right[sample];
	}

	left *= pChannel->scale;
	right *= pChannel->scale;

	if( cl_debugrumble.GetBool() )
	{
		Msg("Seconds:%d Fraction:%f Sample:%d  L:%f R:%f\n", seconds, fraction, sample, left, right );
	}

	if( !m_bOutputEnabled )
	{
		// Send zeroes to stop any current rumbling, and to keep it silenced.
		left = 0;
		right = 0;
	}

	*pLeft = left;
	*pRight = right;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::UpdateScreenShakeRumble( int userID, float shake, float balance )
{
	if( m_bOutputEnabled )
	{
		m_flScreenShake[ userID ] = shake;
	}
	else
	{
		// Silence
		m_flScreenShake[ userID ] = 0.0f;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CRumbleEffects::UpdateEffects( int userID, float curtime )
{
	float fLeftMotor = 0.0f;
	float fRightMotor = 0.0f;

	for( int i = 0 ; i < MAX_RUMBLE_CHANNELS ; i++ )
	{
		// Expire old channels
		RumbleChannel_t *pChannel = &m_Channels[userID][i];

		if( pChannel->in_use )
		{
			float left = 0, right = 0;

			ComputeAmplitudes( pChannel, curtime, &left, &right );
			
			fLeftMotor += left;
			fRightMotor += right;
		}
	}

	// Add in any screenshake
	float shakeLeft = 0.0f;
	float shakeRight = 0.0f;
	if( m_flScreenShake[ userID ] != 0.0f )
	{
		if( m_flScreenShake[ userID ] < 0.0f )
		{
			shakeLeft = fabs( m_flScreenShake[ userID ] );
		}
		else
		{
			shakeRight = m_flScreenShake[ userID ];
		}
	}

	fLeftMotor += shakeLeft;
	fRightMotor += shakeRight;

	fLeftMotor *= cl_rumblescale.GetFloat();
	fRightMotor *= cl_rumblescale.GetFloat();

	if ( engine->IsPaused() || !input->ControllerModeActive() )
	{
		// Send nothing when paused or vibration disabled or not using controller.
		fLeftMotor = 0.0f;
		fRightMotor = 0.0f;
	}
	inputsystem->SetRumble( fLeftMotor, fRightMotor, userID );
}

//---------------------------------------------------------
//---------------------------------------------------------
void StopAllRumbleEffects( int userID )
{
	// Kill all rumble channels that have effects assigned to this userID,
	// and stop the motors. 
	g_RumbleEffects.StopAllEffects( userID );

	inputsystem->StopRumble( userID );
}

//---------------------------------------------------------
//---------------------------------------------------------
void RumbleEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags )
{
	g_RumbleEffects.StartEffect( userID, effectIndex, rumbleData, rumbleFlags );	
}

//---------------------------------------------------------
//---------------------------------------------------------
void UpdateRumbleEffects( int userID )
{

// dkorus: maybe only stop weapon driven rumble effects?
// this was short circuiting the damage rumbles when being killed
    CBasePlayer *player = CBasePlayer::GetLocalPlayer( XBX_GetSlotByUserId( userID ) );

	if( !player )
	{
		StopAllRumbleEffects( userID );
		return;
	}

	g_RumbleEffects.UpdateEffects( userID, gpGlobals->curtime );
	
}

//---------------------------------------------------------
//---------------------------------------------------------
void UpdateScreenShakeRumble( int userID, float shake, float balance )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer( XBX_GetSlotByUserId( userID ) );

	if( !player || !player->IsAlive() )
	{
		return;
	}

	g_RumbleEffects.UpdateScreenShakeRumble( userID, shake, balance );
}

//---------------------------------------------------------
//---------------------------------------------------------
void EnableRumbleOutput( bool bEnable )
{
	g_RumbleEffects.SetOutputEnabled( bEnable );
}

class CBaseFlex;
class CBasePlayer;
class CSceneEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Decodes animtime and notes when it changes
// Input  : *pStruct - ( CBaseEntity * ) used to flag animtime is changine
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_ForcedClientTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CSceneEntity *pScene = reinterpret_cast< CSceneEntity * >( pStruct );
	*(float *)pOut = pData->m_Value.m_Float;
	pScene->OnResetClientTime();
}

IMPLEMENT_REFLECT_CLIENTCLASS( CSceneEntity, DT_SceneEntity, CSceneEntity )

CSceneEntity::CSceneEntity( void )
{
	m_pScene = nullptr;
	m_bMultiplayer = false;

	m_hOwner = nullptr;
	m_bClientOnly = false;
}

CSceneEntity::~CSceneEntity( void )
{
	UnloadScene();
}

void CSceneEntity::OnResetClientTime()
{
	m_flCurrentTime = m_flForceClientTime;
}

char const *CSceneEntity::GetSceneFileName()
{
	char const *pStr = g_pStringTableClientSideChoreoScenes->GetString( m_nSceneStringIndex );
	if ( pStr )
		return pStr;

	static bool bFirst = true;
	if ( bFirst )
	{
		bFirst = false;
		contract_assert( 0 );
		Warning( "GetSceneFilename() failed for scene index %d\n", m_nSceneStringIndex );
	}
	return "";
}

ConVar mp_usehwmvcds( side_registry(), "mp_usehwmvcds", "0", 0, "Enable the use of the hw morph vcd(s). (-1 = never, 1 = always, 0 = based upon GPU)" ); // -1 = never, 0 = if hasfastvertextextures, 1 = always
ConVar scene_vcdautosave( side_registry(), "scene_vcdautosave", "0", 0, "Create a savegame before VCD playback" ); 

bool UseHWMorphVCDs()
{
	if ( mp_usehwmvcds.GetInt() == 0 )
		return g_pMaterialSystemHardwareConfig->HasFastVertexTextures();
	return mp_usehwmvcds.GetInt() > 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CSceneEntity::GetHWMorphSceneFileName( const char *pFilename, char *pHWMFilename )
{
	// Are we even using hardware morph?
	if ( !UseHWMorphVCDs() )
		return false;

	// Multi-player only!
	if ( !m_bMultiplayer )
		return false;

	// Do we have a valid filename?
	if ( !( pFilename && pFilename[0] ) )
		return false;

	// Check to see if we already have an player/hwm/* filename.
	if ( ( V_strstr( pFilename, "/high" ) != nullptr ) || ( V_strstr( pFilename, "\\high" ) != nullptr ) )
	{
		V_strcpy( pHWMFilename, pFilename );
		return true;
	}

	// Find the hardware morph scene name and pass that along as well.
	char szScene[MAX_PATH];
	V_strcpy( szScene, pFilename );

	char szSceneHWM[MAX_PATH];
	szSceneHWM[0] = '\0';

	char *pszToken = strtok( szScene, "/\\" );
	while ( pszToken != nullptr )
	{
		if ( !V_stricmp( pszToken, "low" ) )
		{
			V_strcat( szSceneHWM, "high", sizeof( szSceneHWM ) );
		}
		else
		{
			V_strcat( szSceneHWM, pszToken, sizeof( szSceneHWM ) );
		}

		pszToken = strtok( nullptr, "/\\" );
		if ( pszToken != nullptr )
		{
			V_strcat( szSceneHWM, "\\", sizeof( szSceneHWM ) );
		}
	}

	V_strcpy( pHWMFilename, szSceneHWM );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSceneEntity::ResetActorFlexesForScene()
{
	int nActorCount = m_pScene->GetNumActors();
	for( int iActor = 0; iActor < nActorCount; ++iActor )
	{
		CChoreoActor *pChoreoActor = m_pScene->GetActor( iActor );
		if ( !pChoreoActor )
			continue;

		CBaseFlex *pFlexActor = FindNamedActor( pChoreoActor );
		if ( !pFlexActor )
			continue;

		CStudioHdr *pStudioHdr = pFlexActor->GetModelPtr();
		if ( !pStudioHdr )
			continue;

		if ( pStudioHdr->numflexdesc() == 0 )
			continue;

		// Reset the flex weights to their starting position.
		LocalFlexController_t iController;
		for ( iController = LocalFlexController_t(0); iController < pStudioHdr->numflexcontrollers(); ++iController )
		{
			pFlexActor->SetFlexWeight( iController, 0.0f );
		}

		// Reset the prediction interpolation values.
		pFlexActor->m_iv_flexWeight.Reset( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSceneEntity::StopClientOnlyScene()
{
	if ( m_pScene )
	{
		m_pScene->ResetSimulation();

		if ( m_hOwner.Get() )
		{
			m_hOwner->RemoveChoreoScene( m_pScene );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSceneEntity::SetupClientOnlyScene( const char *pszFilename, CBaseFlex *pOwner /* = NULL */, bool bMultiplayer /* = false */ )
{
	m_bIsPlayingBack = true;
	m_bMultiplayer = bMultiplayer;
	m_hOwner = pOwner;
	m_bClientOnly = true;

	char szFilename[128];
	contract_assert( V_strlen( pszFilename ) < 128 );
	V_strcpy( szFilename, pszFilename );

	char szSceneHWM[128];
	if ( GetHWMorphSceneFileName( szFilename, szSceneHWM ) )
	{
		V_strcpy( szFilename, szSceneHWM );
	}

	contract_assert( szFilename && szFilename[ 0 ] );
	if (  szFilename && szFilename[ 0 ] )
	{
		LoadSceneFromFile( szFilename );
		
		contract_assert( m_pScene );

		// Should handle gestures and sequences client side.
		if ( m_bMultiplayer )
		{
			if ( m_pScene )
			{
				int types[6];
				types[0] = CChoreoEvent::FLEXANIMATION;
				types[1] = CChoreoEvent::EXPRESSION;
				types[2] = CChoreoEvent::GESTURE;
				types[3] = CChoreoEvent::SEQUENCE;
				types[4] = CChoreoEvent::SPEAK;
				types[5] = CChoreoEvent::LOOP;
				m_pScene->RemoveEventsExceptTypes( types, 6 );
			}

			PrefetchAnimBlocks( m_pScene );
		}
		else
		{
			if ( m_pScene )
			{
				int types[ 2 ];
				types[ 0 ] =  CChoreoEvent::FLEXANIMATION;
				types[ 1 ] =  CChoreoEvent::EXPRESSION;
				m_pScene->RemoveEventsExceptTypes( types, 2 );
			}
		}

		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	if ( m_hOwner.Get() )
	{
		contract_assert( m_pScene );

		if ( m_pScene )
		{
			ClearSceneEvents( m_pScene, false );

			if ( m_bIsPlayingBack )
			{
				m_pScene->ResetSimulation();
				m_hOwner->StartChoreoScene( m_pScene );
			}
			else
			{
				m_pScene->ResetSimulation();
				m_hOwner->RemoveChoreoScene( m_pScene );
			}

			// Reset the flex weights when we start a new scene.  This is normally done on the player model, but since
			// we don't have a player here yet - we need to do this!
			ResetActorFlexesForScene();
		}
	}
	else
	{
		for( int i = 0; i < std::ssize( m_hActorList ) ; ++i )
		{
			CBaseFlex *actor = m_hActorList[ i ].Get();
			if ( !actor )
				continue;

			contract_assert( m_pScene );

			if ( m_pScene )
			{
				ClearSceneEvents( m_pScene, false );

				if ( m_bIsPlayingBack )
				{
					m_pScene->ResetSimulation();
					actor->StartChoreoScene( m_pScene );
				}
				else
				{
					m_pScene->ResetSimulation();
					actor->RemoveChoreoScene( m_pScene );
				}
			}
		}
	}
}

void CSceneEntity::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	char const *str = GetSceneFileName();
	char szFilename[MAX_PATH];
	contract_assert( V_strlen( str ) < MAX_PATH );
	V_strcpy( szFilename, str );

	char szSceneHWM[MAX_PATH];
	if ( GetHWMorphSceneFileName( szFilename, szSceneHWM ) )
	{
		V_strcpy( szFilename, szSceneHWM );
	}

	if ( updateType == DATA_UPDATE_CREATED )
	{
		contract_assert( szFilename && szFilename[ 0 ] );
		if (  szFilename && szFilename[ 0 ] )
		{
			LoadSceneFromFile( szFilename );

			// Kill everything except flex events
			contract_assert( m_pScene );

			// Should handle gestures and sequences clientside.
			if ( m_bMultiplayer )
			{
				if ( m_pScene )
				{
					int types[6];
					types[0] = CChoreoEvent::FLEXANIMATION;
					types[1] = CChoreoEvent::EXPRESSION;
					types[2] = CChoreoEvent::GESTURE;
					types[3] = CChoreoEvent::SEQUENCE;				
					types[4] = CChoreoEvent::SPEAK;
					types[5] = CChoreoEvent::LOOP;
					m_pScene->RemoveEventsExceptTypes( types, 6 );
				}

				PrefetchAnimBlocks( m_pScene );
			}
			else
			{
				if ( m_pScene )
				{
					int types[ 2 ];
					types[ 0 ] =  CChoreoEvent::FLEXANIMATION;
					types[ 1 ] =  CChoreoEvent::EXPRESSION;
					m_pScene->RemoveEventsExceptTypes( types, 2 );
				}
			}

			SetNextClientThink( CLIENT_THINK_ALWAYS );
		}
	}

	// Playback state changed...
	if ( m_bWasPlaying != m_bIsPlayingBack )
	{
		for(int i = 0; i < std::ssize( m_hActorList ) ; ++i )
		{
			CBaseFlex *actor = m_hActorList[ i ].Get();
			if ( !actor )
				continue;

			contract_assert( m_pScene );

			if ( m_pScene )
			{
				ClearSceneEvents( m_pScene, false );

				if ( m_bIsPlayingBack )
				{
					m_pScene->ResetSimulation();
					actor->StartChoreoScene( m_pScene );
				}
				else
				{
					m_pScene->ResetSimulation();
					actor->RemoveChoreoScene( m_pScene );
				}
			}
		}
	}
}

void CSceneEntity::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_bWasPlaying = m_bIsPlayingBack;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame that an event is active (Start/EndEvent as also
//  called)
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CSceneEntity::ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Called for events that are part of a pause condition
// Input  : *event - 
// Output : Returns true on event completed, false on non-completion.
//-----------------------------------------------------------------------------
bool CSceneEntity::CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	return true;
}

CBaseFlex *CSceneEntity::FindNamedActor( CChoreoActor *pChoreoActor )
{
	if ( !m_pScene )
		return nullptr;

	if ( m_hOwner.Get() != nullptr )
	{
		return m_hOwner.Get();
	}

	int idx = m_pScene->FindActorIndex( pChoreoActor );
	if ( idx < 0 || idx >= std::ssize( m_hActorList ) )
		return nullptr;

	return m_hActorList[ idx ].Get();
}

//-----------------------------------------------------------------------------
// Purpose: All events are leading edge triggered
// Input  : currenttime - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	contract_assert( event );

	if ( !V_stricmp( event->GetName(), "NULL" ) )
 	{
 		Scene_Printf( "%s : %8.2f:  ignored %s\n", GetSceneFileName(), currenttime, event->GetDescription() );
 		return;
 	}
 
	CBaseFlex *pActor = nullptr;
	CChoreoActor *actor = event->GetActor();
	if ( actor )
	{
		pActor = FindNamedActor( actor );
		if ( nullptr == pActor )
		{
			// This can occur if we haven't been networked an actor yet... we need to queue it so that we can 
			//  fire off the start event as soon as we have the actor resident on the client.
			QueueStartEvent( currenttime, scene, event );
			return;
		}
	}

	Scene_Printf( "%s : %8.2f:  start %s\n", GetSceneFileName(), currenttime, event->GetDescription() );
	if ( event )
	{
		BlackBox_Record( "vcd", "%s : %8.2f:  start %s", GetSceneFileName(), currenttime, event->GetDescription() );
	}
	
	switch ( event->GetType() )
	{
	case CChoreoEvent::FLEXANIMATION:
		{
			if ( pActor )
			{
				DispatchStartFlexAnimation( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::EXPRESSION:
		{
			if ( pActor )
			{
				DispatchStartExpression( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			// Verify data.
			contract_assert( m_bMultiplayer );
			contract_assert( scene != nullptr );
			contract_assert( event != nullptr );

			if ( pActor )
			{
				DispatchStartGesture( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::SEQUENCE:
		{
			// Verify data.
			contract_assert( m_bMultiplayer );
			contract_assert( scene != nullptr );
			contract_assert( event != nullptr );

			if ( pActor )
			{
				DispatchStartSequence( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::LOOP:
		{
			// Verify data.
			contract_assert( m_bMultiplayer );
			contract_assert( scene != nullptr );
			contract_assert( event != nullptr );

			DispatchProcessLoop( scene, event );
		}
	case CChoreoEvent::SPEAK:
		{
			if ( IsClientOnly() && pActor )
			{
				// FIXME: dB hack.  soundlevel needs to be moved into inside of wav?
				soundlevel_t iSoundlevel = SNDLVL_TALKING;
				if ( event->GetParameters2() )
				{
					iSoundlevel = (soundlevel_t)atoi( event->GetParameters2() );
					if ( iSoundlevel == SNDLVL_NONE )
					{
						iSoundlevel = SNDLVL_TALKING;
					}
				}

				DispatchStartSpeak( scene, pActor, event, iSoundlevel );
			}
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchProcessLoop( CChoreoScene *scene, CChoreoEvent *event )
{
	contract_assert( event->GetType() == CChoreoEvent::LOOP );

	float backtime = (float)atof( event->GetParameters() );

	bool process = true;
	int counter = event->GetLoopCount();
	if ( counter != -1 )
	{
		int remaining = event->GetNumLoopsRemaining();
		if ( remaining <= 0 )
		{
			process = false;
		}
		else
		{
			event->SetNumLoopsRemaining( --remaining );
		}
	}

	if ( !process )
		return;

	scene->LoopToTime( backtime );
	SetCurrentTime( backtime, true );
}

//-----------------------------------------------------------------------------
// Purpose: Playback sound file that contains phonemes
// Input  : *actor - 
//			*parameters - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchStartSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event, soundlevel_t iSoundlevel )
{
	// Emit sound
	if ( IsClientOnly() && actor )
	{
		CSingleUserRecipientFilter filter( CBasePlayer::GetLocalPlayer() );

		float time_in_past = m_flCurrentTime - event->GetStartTime() ;
		float soundtime = gpGlobals->curtime - time_in_past;

		EmitSound_t es;
		es.m_nChannel = CHAN_VOICE;
		es.m_flVolume = 1;
		es.m_SoundLevel = iSoundlevel;
		es.m_flSoundTime = soundtime;

		// No CC since we do it manually
		// FIXME:  This will  change
		es.m_bEmitCloseCaption = false;
		es.m_pSoundName = event->GetParameters();

		EmitSound( filter, actor->entindex(), es );
		actor->AddSceneEvent( scene, event, nullptr, IsClientOnly(), this );

		// Close captioning removed (VGUI teardown)
	}
}

void CSceneEntity::DispatchEndSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	if ( IsClientOnly() )
	{
		actor->RemoveSceneEvent( scene, event, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : currenttime - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	contract_assert( event );

	if ( !V_stricmp( event->GetName(), "NULL" ) )
 	{
 		return;
 	}

	CBaseFlex *pActor = nullptr;
	CChoreoActor *actor = event->GetActor();
	if ( actor )
	{
		pActor = FindNamedActor( actor );
	}

	Scene_Printf( "%s : %8.2f:  finish %s\n", GetSceneFileName(), currenttime, event->GetDescription() );

	switch ( event->GetType() )
	{
	case CChoreoEvent::FLEXANIMATION:
		{
			if ( pActor )
			{
				DispatchEndFlexAnimation( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::EXPRESSION:
		{
			if ( pActor )
			{
				DispatchEndExpression( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			if ( pActor )
			{
				DispatchEndGesture( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::SEQUENCE:
		{
			if ( pActor )
			{
				DispatchEndSequence( scene, pActor, event );
			}
		}
		break;
	case CChoreoEvent::SPEAK:
		{
			if ( IsClientOnly() && pActor )
			{
				DispatchEndSpeak( scene, pActor, event );
			}
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Binary compiled VCDs get their strings from a pool
//-----------------------------------------------------------------------------
class CChoreoStringPool : public IChoreoStringPool
{
public:
	short FindOrAddString( const char *pString )
	{
		// huh?, no compilation at run time, only fetches
		contract_assert( 0 );
		return -1;
	}

	bool GetString( short stringId, char *buff, int buffSize )
	{
		// fetch from compiled pool
		const char *pString = scenefilecache->GetSceneString( stringId );
		if ( !pString )
		{
			V_strncpy( buff, "", buffSize );
			return false;
		}
		V_strncpy( buff, pString, buffSize );
		return true;
	} 	
};
CChoreoStringPool g_ChoreoStringPool;

CChoreoScene *CSceneEntity::LoadScene( const char *filename )
{
	char loadfile[ 512 ];
	V_strncpy( loadfile, filename, sizeof( loadfile ) );
	V_SetExtension( loadfile, ".vcd", sizeof( loadfile ) );
	V_FixSlashes( loadfile );

	char *pBuffer = nullptr;
	size_t bufsize = scenefilecache->GetSceneBufferSize( loadfile );
	if ( bufsize <= 0 )
		return nullptr;

	pBuffer = new char[ bufsize ];
	if ( !scenefilecache->GetSceneData( filename, (byte *)pBuffer, bufsize ) )
	{
		delete[] pBuffer;
		return nullptr;
	}

	CChoreoScene *pScene;
	if ( IsBufferBinaryVCD( pBuffer, bufsize ) )
	{
		pScene = new CChoreoScene( this );
		ks::byte_reader buf( pBuffer, bufsize );
		if ( !pScene->RestoreFromBinaryBuffer( buf, loadfile, &g_ChoreoStringPool ) )
		{
			Warning( "Unable to restore binary scene '%s'\n", loadfile );
			delete pScene;
			pScene = nullptr;
		}
		else
		{
			pScene->SetPrintFunc( Scene_Printf );
			pScene->SetEventCallbackInterface( this );
		}
	}
	else
	{
		g_TokenProcessor.SetBuffer( pBuffer );
		pScene = ChoreoLoadScene( loadfile, this, &g_TokenProcessor, Scene_Printf );
	}

	delete[] pBuffer;
	return pScene;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CSceneEntity::LoadSceneFromFile( const char *filename )
{
	// Save game if convar is set - useful when iterating on a scene with Foundry
	if ( scene_vcdautosave.GetBool() )
	{
		char szVCDFileName[64];
		char szSaveFileName[64];
		char szClientCmd[128];

		// Create the faceposer sub-directory under the root savegame directory
		if (!g_pFullFileSystem->IsDirectory( "SAVE\\faceposer", "MOD" ))
		{
			g_pFullFileSystem->CreateDirHierarchy( "SAVE\\faceposer", "MOD" );
		}

		// Construct save command to send to the engine
		V_FileBase( filename, szVCDFileName, sizeof(szVCDFileName) );
		V_snprintf( szClientCmd, sizeof(szClientCmd), "save faceposer\\%s\n", szVCDFileName );

		// Construct name of file that would be created if savegame occurs
		V_snprintf( szSaveFileName, sizeof(szSaveFileName), "SAVE\\faceposer\\%s.sav", szVCDFileName );
		
		// Only create a savegame for this VCD if there isn't one already
		if (!g_pFullFileSystem->FileExists( szSaveFileName, "MOD" ))
		{
			engine->ClientCmd( szClientCmd );
		}
	}

	UnloadScene();
	m_pScene = LoadScene( filename );
}

void CSceneEntity::ClearSceneEvents( CChoreoScene *scene, bool canceled )
{
	if ( !m_pScene )
		return;

	Scene_Printf( "%s : %8.2f:  clearing events\n", GetSceneFileName(), m_flCurrentTime );

	int i;
	for ( i = 0 ; i < m_pScene->GetNumActors(); i++ )
	{
		CBaseFlex *pActor = FindNamedActor( m_pScene->GetActor( i ) );
		if ( !pActor )
			continue;

		// Clear any existing expressions
		pActor->ClearSceneEvents( scene, canceled );
	}

	WipeQueuedEvents();
	OnResetClientTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSceneEntity::UnloadScene( void )
{
	WipeQueuedEvents();

	if ( m_pScene )
	{
		ClearSceneEvents( m_pScene, false );
		for ( int i = 0 ; i < m_pScene->GetNumActors(); i++ )
		{
			CBaseFlex *pTestActor = FindNamedActor( m_pScene->GetActor( i ) );

			if ( !pTestActor )
				continue;
		
			pTestActor->RemoveChoreoScene( m_pScene );
		}
	}
	delete m_pScene;
	m_pScene = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchStartFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->AddSceneEvent( scene, event, nullptr, IsClientOnly() || IsMultiplayer(), this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchEndFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->RemoveSceneEvent( scene, event, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchStartExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->AddSceneEvent( scene, event, nullptr, IsClientOnly() || IsMultiplayer(), this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*event - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchEndExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->RemoveSceneEvent( scene, event, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*parameters - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchStartGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	// Ingore null gestures
	if ( !V_stricmp( event->GetName(), "NULL" ) )
		return;

	actor->AddSceneEvent( scene, event, nullptr, IsClientOnly() || IsMultiplayer(), this ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchStartSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->AddSceneEvent( scene, event, nullptr, IsClientOnly() || IsMultiplayer(), this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchEndSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	actor->RemoveSceneEvent( scene, event, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*parameters - 
//-----------------------------------------------------------------------------
void CSceneEntity::DispatchEndGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event )
{
	// Ingore null gestures
	if ( !V_stricmp( event->GetName(), "NULL" ) )
		return;

	actor->RemoveSceneEvent( scene, event, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSceneEntity::DoThink( float frametime )
{
	if ( !m_pScene )
		return;

	if ( !m_bIsPlayingBack )
	{
		WipeQueuedEvents();
		return;
	}

	CheckQueuedEvents();

	if ( m_bPaused )
	{
		return;
	}

	// Msg( "CL:  %d, %f for %s\n", gpGlobals->tickcount, m_flCurrentTime, m_pScene->GetFilename() );

	// Tell scene to go
	m_pScene->Think( m_flCurrentTime );
	// Drive simulation time for scene
	m_flCurrentTime += gpGlobals->frametime;
}

void CSceneEntity::ClientThink()
{
	DoThink( gpGlobals->frametime );
}

void CSceneEntity::CheckQueuedEvents()
{
// Check for duplicates
	std::vector< QueuedEvents_t > events;
	events = m_QueuedEvents;
	m_QueuedEvents.clear();

	int c = std::ssize( events );
	for ( int i = 0; i < c; ++i )
	{
		const QueuedEvents_t& check = events[ i ];
		
		// Retry starting this event
		StartEvent( check.starttime, check.scene, check.event );
	}
}

void CSceneEntity::WipeQueuedEvents()
{
	m_QueuedEvents.clear(); m_QueuedEvents.shrink_to_fit();
}

void CSceneEntity::QueueStartEvent( float starttime, CChoreoScene *scene, CChoreoEvent *event )
{
	// Check for duplicates
	int c = std::ssize( m_QueuedEvents );
	for ( int i = 0; i < c; ++i )
	{
		const QueuedEvents_t& check = m_QueuedEvents[ i ];
		if ( check.scene == scene && 
			 check.event == event )
			return;
	}

	QueuedEvents_t qe;
	qe.scene = scene;
	qe.event = event;
	qe.starttime = starttime;
	m_QueuedEvents.push_back( qe );
}

//-----------------------------------------------------------------------------
// Purpose: Resets time such that the client version of the .vcd is also updated, if appropriate
// Input  : t - 
//			forceClientSync - unused for now, we may want to reenable this at some point
//-----------------------------------------------------------------------------
void CSceneEntity::SetCurrentTime( float t, bool forceClientSync )
{
	m_flCurrentTime = t;
	m_flForceClientTime = t;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSceneEntity::PrefetchAnimBlocks( CChoreoScene *pScene )
{
	contract_assert( pScene && m_bMultiplayer );
	if ( !pScene || !m_bMultiplayer )
		return;

	// Build a fast lookup, too
	std::map<CChoreoActor*,CBaseFlex*> actorMap;

	int nSpew = 0;
	int nResident = 0;
	int nChecked = 0;

	// Iterate events and precache necessary resources
	for ( int i = 0; i < pScene->GetNumEvents(); i++ )
	{
		CChoreoEvent *pEvent = pScene->GetEvent( i );
		if ( !pEvent )
			continue;

		// load any necessary data
		switch ( pEvent->GetType() )
		{
		default:
			break;
		case CChoreoEvent::SEQUENCE:
		case CChoreoEvent::GESTURE:
			{
				CChoreoActor *pActor = pEvent->GetActor();
				if ( pActor )
				{
					CBaseFlex *pFlex = nullptr;
					auto idx = actorMap.find( pActor );
					if ( idx == actorMap.end() )
					{
						pFlex = FindNamedActor( pActor );
						actorMap.emplace( pActor, pFlex );
					}
					else
					{
						pFlex = idx->second;
					}

					if ( pFlex )
					{
						int iSequence = pFlex->LookupSequence( pEvent->GetParameters() );
						if ( iSequence >= 0 )
						{
							CStudioHdr *pStudioHdr = pFlex->GetModelPtr();
							if ( pStudioHdr )
							{
								// Now look up the animblock
								mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( iSequence );
								for ( int i = 0 ; i < seqdesc.groupsize[ 0 ] ; ++i )
								{
									for ( int j = 0; j < seqdesc.groupsize[ 1 ]; ++j )
									{
										int iAnimation = seqdesc.anim( i, j );
										int iBaseAnimation = pStudioHdr->iRelativeAnim( iSequence, iAnimation );
										mstudioanimdesc_t &animdesc = pStudioHdr->pAnimdesc( iBaseAnimation );

										++nChecked;

										if ( nSpew != 0 )
										{
											Msg( "%s checking block %d\n", pStudioHdr->pszName(), animdesc.animblock );
										}

										// Async load the animation
										int iFrame = 0;
										const byte *panim = animdesc.pAnim( &iFrame );
										if ( panim )
										{
											++nResident;
											if ( nSpew > 1 )
											{
												Msg( "%s:%s[%i:%i] was resident\n", pStudioHdr->pszName(), animdesc.pszName(), i, j );
											}
										}
										else
										{
											if ( nSpew != 0 )
											{
												Msg( "%s:%s[%i:%i] async load\n", pStudioHdr->pszName(), animdesc.pszName(), i, j );
											}
										}
									}
								}
							}
						}
					}
				}
				break;
			}
		}
	}

	if ( !nSpew || nChecked <= 0 )
		return;

	Msg( "%d of %d animations resident\n", nResident, nChecked );
}

class CBaseEntity;

using namespace ::ks::reflect;

ConVar scene_print( side_registry(), "scene_print", "0", FCVAR_REPLICATED, "When playing back a scene, print timing and event info to console." );
ConVar scene_clientflex( side_registry(), "scene_clientflex", "1", FCVAR_REPLICATED, "Do client side flex animation." );

CSceneEventInfo::CSceneEventInfo()
	:
m_pEvent( 0 ),
m_pScene( 0 ),
m_pActor( 0 ),
m_hSceneEntity( INVALID_EHANDLE ),
m_bStarted( false ),
m_iLayer( -1 ),
m_iPriority( 0 ),
m_nSequence( 0 ),
m_bIsGesture( false ),
m_flWeight( 0.0f ),
m_hTarget(),
m_bIsMoving( false ),
m_bHasArrived( false ),
m_flInitialYaw( 0.0f ),
m_flTargetYaw( 0.0f ),
m_flFacingYaw( 0.0f ),
m_nType( 0 ),
m_flNext( 0.0f ),
m_bClientSide( false ),
m_pExpHdr( nullptr )
{
}

CSceneEventInfo::~CSceneEventInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFormat - 
//			... - 
// Output : static void
//-----------------------------------------------------------------------------
void Scene_Printf( const char *pFormat, ... )
{
	int val = scene_print.GetInt();
	if ( !val )
		return;

	if ( val >= 2 )
	{
		if ( CBaseEntity::IsServer() && val != 2 )
		{
			return;
		}
        else if ( !CBaseEntity::IsServer() && val != 3 )
		{
			return;
		}
	}

	std::va_list marker;
	char msg[8192];

	__builtin_va_start(marker, pFormat);
	V_vsnprintf(msg, sizeof(msg), pFormat, marker);
	__builtin_va_end(marker);	
	
	Msg( "%8.3f[%d] %s:  %s", gpGlobals->curtime, gpGlobals->tickcount, CBaseEntity::IsServer() ? "sv" : "cl", msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CSceneTokenProcessor::CurrentToken( void )
{
	return m_szToken;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	// NOTE: crossline is ignored here, may need to implement if needed
	m_pBuffer = engine->ParseFile( m_pBuffer, m_szToken, sizeof( m_szToken ) );
	if ( 0 <= (int)std::strlen( m_szToken ) )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	char const *search_p = m_pBuffer;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if ( !*search_p )
			return false;

	}

	if (*search_p == ';' || *search_p == '#' ||		 // semicolon and # is comment field
		(*search_p == '/' && *((search_p)+1) == '/')) // also make // a comment field
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::Error( const char *fmt, ... )
{
	char string[ 2048 ];
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	V_vsnprintf( string, sizeof(string), fmt, argptr );
	__builtin_va_end( argptr );

	Warning( "%s", string );
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::SetBuffer( char *buffer )
{
	m_pBuffer = buffer;
}

CSceneTokenProcessor g_TokenProcessor;

class CAnimationLayer;

using namespace ::ks::reflect;

// -----------------------------------------------------------------------------
// CSequenceTransitioner implementation.
// -----------------------------------------------------------------------------

void CSequenceTransitioner::CheckForSequenceChange( 
	CStudioHdr *hdr,
	int nCurSequence, 
	bool bForceNewSequence,
	bool bInterpolate )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	// FIXME?: this should detect that what's been asked to be drawn isn't what was expected
	// due to not only sequence change, by frame index, rate, or whatever.  When that happens, 
	// it should insert the previous rules.

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
		m_animationQueue[0].SetOwner( nullptr );
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	if (currentblend->m_flLayerAnimtime && 
		(currentblend->GetSequence() != nCurSequence || bForceNewSequence ))
	{
		if ( nCurSequence < 0 || nCurSequence >= hdr->GetNumSeq() )
		{
			// remove all entries
			m_animationQueue.clear();
		}
		else
		{
			mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( nCurSequence );
			// sequence changed
			if ((seqdesc.flags & STUDIO_SNAP) || !bInterpolate )
			{
				// remove all entries
				m_animationQueue.clear();
			}
			else
			{
				mstudioseqdesc_t &prevseqdesc = hdr->pSeqdesc( currentblend->GetSequence() );
				currentblend->m_flLayerFadeOuttime = std::min( prevseqdesc.fadeouttime, seqdesc.fadeintime );
				/*
				// clip blends to time remaining
				if ( !IsSequenceLooping(hdr, currentblend->GetSequence()) )
				{
					float length = Studio_Duration( hdr, currentblend->GetSequence(), flPoseParameter ) / currentblend->GetPlaybackRate();
					float timeLeft = (1.0 - currentblend->GetCycle()) * length;
					if (timeLeft < currentblend->m_flLayerFadeOuttime)
						currentblend->m_flLayerFadeOuttime = timeLeft;
				}
				*/
			}
			if ( std::ssize( m_animationQueue ) > 2 )
				m_animationQueue.erase( m_animationQueue.begin(), m_animationQueue.begin() + 1 );
		}
		// push previously set sequence
		m_animationQueue.emplace_back();
		currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];
		currentblend->SetOwner( nullptr );
	}

	currentblend->SetSequence( -1 );
	currentblend->m_flLayerAnimtime = 0.0;
	currentblend->m_flLayerFadeOuttime = 0.0;
}

void CSequenceTransitioner::UpdateCurrent( 
	CStudioHdr *hdr,
	int nCurSequence, 
	float flCurCycle,
	float flCurPlaybackRate,
	float flCurTime )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
		m_animationQueue[0].SetOwner( nullptr );
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	// keep track of current sequence
	currentblend->SetSequence( nCurSequence );
	currentblend->m_flLayerAnimtime = flCurTime;
	currentblend->SetCycle( flCurCycle );
	currentblend->SetPlaybackRate( flCurPlaybackRate );

	// calc blending weights for previous sequences
	int i;
	for (i = 0; i < std::ssize( m_animationQueue ) - 1;)
	{
 		float s = m_animationQueue[i].GetFadeout( flCurTime );

		if (s > 0)
		{
			m_animationQueue[i].SetWeight( s );
			i++;
		}
		else
		{
			m_animationQueue.erase( m_animationQueue.begin() + i );
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Shadow control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class CShadowControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ShadowControl" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CShadowControl, CBaseEntity );

	DECLARE_CLIENTCLASS();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

private:
	[[= ks::reflect::Net{} ]] Vector m_shadowDirection;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32 m_shadowColor;
	[[= ks::reflect::Net{} ]] float m_flShadowMaxDist;
	[[= ks::reflect::Net{} ]] bool m_bDisableShadows;
	[[= ks::reflect::Net{} ]] bool m_bEnableLocalLightShadows;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CShadowControl, DT_ShadowControl, CShadowControl )

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CShadowControl::OnDataChanged(DataUpdateType_t updateType)
{
	// Set the color, direction, distance...
	g_pClientShadowMgr->SetShadowDirection( m_shadowDirection );
	g_pClientShadowMgr->SetShadowColor( m_shadowColor.r, m_shadowColor.g, m_shadowColor.b );
	g_pClientShadowMgr->SetShadowDistance( m_flShadowMaxDist );
	g_pClientShadowMgr->SetShadowsDisabled( m_bDisableShadows );
	g_pClientShadowMgr->SetShadowFromWorldLightsEnabled( m_bEnableLocalLightShadows );
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CShadowControl::ShouldDraw()
{
	return false;
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Simple key interpolations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &out - 
//			t - 
//			&start - 
//			&end - 
//-----------------------------------------------------------------------------
void CSimpleKeyInterp::Interp( Vector &out, float t, const CSimpleKeyInterp &start, const CSimpleKeyInterp &end )
{
	float delta = end.GetTime() - start.GetTime();
	t = clamp( t-start.GetTime(), 0, delta );

	float unitT = (delta > 0) ? (t / delta) : 1;

	switch( end.m_interp )
	{
	case KEY_SPLINE:
		unitT = SimpleSpline( unitT );
		break;
	case KEY_ACCELERATE:
		unitT *= unitT;
		break;
	case KEY_DECELERATE:
		unitT = std::sqrt(unitT);
		break;
	default:
	case KEY_LINEAR:
		//unitT = unitT;
		break;
	}
	out = (1-unitT) * ((Vector)start) + unitT * ((Vector)end);
}

//-----------------------------------------------------------------------------
// Simple key list
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &key - 
// Output : int
//-----------------------------------------------------------------------------
int CSimpleKeyList::Insert( const CSimpleKeyInterp &key )
{
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		if ( key.GetTime() < m_list[i].GetTime() )
			m_list.insert( m_list.begin() + i, key ); return i;
	}

	m_list.push_back( key ); return (int)std::ssize( m_list ) - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &out - 
//			t - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSimpleKeyList::Interp( Vector &out, float t )
{
	int startIndex = -1;

	out.Init();
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		if ( t < m_list[i].GetTime() )
		{
			// before start
			if ( startIndex < 0 )
				return false;
			CSimpleKeyInterp::Interp( out, t, m_list[startIndex], m_list[i] );
			return true;
		}
		startIndex = i;
	}

	// past end
	return false;
}

CSimpleKeyInterp::CSimpleKeyInterp( float t, simplekeyinterp_t interp, float x, float y, float z) : Vector( x, y, z )
{
	m_interp = interp;
	m_keyTime = t;
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatchBase, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatch, CStopwatchBase )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandStopwatch, CStopwatchBase )

//-----------------------------------------------------------------------------

void CSimpleSimTimer::Set( float minInterval, float maxInterval)
{
	if ( maxInterval > 0.0 )
		m_next = gpGlobals->curtime + random_valve->RandomFloat( minInterval, maxInterval );
	else
		m_next = gpGlobals->curtime + minInterval;
}

float CSimpleSimTimer::GetRemaining() const
{
	float result = m_next - gpGlobals->curtime;
	if (result < 0 )
		return 0;
	return result;
}

void CSimTimer::Set( float interval, bool startExpired)
{
	m_interval = interval;
	m_next = (startExpired) ? -1.0 : gpGlobals->curtime + m_interval;
}

void CRandSimTimer::Set( float minInterval, float maxInterval, bool startExpired)
{
	m_minInterval = minInterval;
	m_maxInterval = maxInterval;
	
	if (startExpired)
	{
		m_next = -1;
	}
	else
	{
		if ( m_maxInterval == 0 )
			m_next = gpGlobals->curtime + m_minInterval;
		else
			m_next = gpGlobals->curtime + random_valve->RandomFloat( m_minInterval, m_maxInterval );
	}
}

void CSimpleStopwatch::Start( float minCountdown, float maxCountdown)
{
	m_fIsRunning = true;
	CSimpleSimTimer::Set( minCountdown, maxCountdown );
}

void CStopwatch::Start( float intervalOverride)
{
	m_fIsRunning = true;
	m_next = gpGlobals->curtime + intervalOverride;
}

void CRandStopwatch::Set( float minInterval, float maxInterval)
{
	m_minInterval = minInterval;
	m_maxInterval = maxInterval;
}

void CRandStopwatch::Start( float minOverride, float maxOverride)
{
	m_fIsRunning = true;
	if ( maxOverride == 0 )
		m_next = gpGlobals->curtime + minOverride;
	else
		m_next = gpGlobals->curtime + random_valve->RandomFloat( minOverride, maxOverride );
}

bool CThinkOnceSemaphore::EnterThink()
{
	if ( m_lastTime == gpGlobals->curtime )
		return false;
	m_lastTime = gpGlobals->curtime;
	return true;
}

bool CThinkOnceSemaphore::DidThink() const
{
	return ( gpGlobals->curtime == m_lastTime );

}

using namespace ::ks::reflect;

static constexpr auto SLIDESHOW_LIST_BUFFER_MAX = 8192;

enum SlideshowCycleTypes
{
	SLIDESHOW_CYCLE_RANDOM,
	SLIDESHOW_CYCLE_FORWARD,
	SLIDESHOW_CYCLE_BACKWARD,

	SLIDESHOW_CYCLE_TOTAL
};

std::vector< CSlideshowDisplay* > g_SlideshowDisplays;

IMPLEMENT_REFLECT_CLIENTCLASS( CSlideshowDisplay, DT_SlideshowDisplay, CSlideshowDisplay )

CSlideshowDisplay::CSlideshowDisplay()
{
	g_SlideshowDisplays.push_back( this );
}

CSlideshowDisplay::~CSlideshowDisplay()
{
	ks::erase_first( g_SlideshowDisplays, this );
}

void CSlideshowDisplay::Spawn( void )
{
	BaseClass::Spawn();

	m_NextSlideTime = 0;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CSlideshowDisplay::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		BuildSlideShowImagesList();
	}
	else if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		m_iCurrentSlideList = 0;
		m_iCurrentSlide = 0;
	}
}

int CSlideshowDisplay::GetMaterialIndex( int iSlideIndex )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return m_SlideMaterialLists[ 0 ]->iSlideMaterials[ iSlideIndex ];
}

int CSlideshowDisplay::NumMaterials( void )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return std::ssize( m_SlideMaterialLists[ 0 ]->iSlideMaterials );
}

void CSlideshowDisplay::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( !m_bEnabled )
		return;

	// Check if it's time for the next slide
	if ( m_NextSlideTime > gpGlobals->curtime )
		return;

	// Set the time to cycle to the next slide
	m_NextSlideTime = gpGlobals->curtime + RandomFloat( m_fMinSlideTime, m_fMaxSlideTime );

	// Get the amount of items to pick from
	int iNumCurrentSlideLists;
	for ( iNumCurrentSlideLists = 0; iNumCurrentSlideLists < 16; ++iNumCurrentSlideLists )
	{
		if ( m_chCurrentSlideLists[ iNumCurrentSlideLists ] == (unsigned char)-1 )
			break;
	}

	// Bail if no slide lists are selected
	if ( iNumCurrentSlideLists == 0 )
		return;

	// Cycle the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
		{
			int iOldSlideList = m_iCurrentSlideList;
			m_iCurrentSlideList = RandomInt( 0, iNumCurrentSlideLists - 1 );

			// Prevent repeats if we don't want them
			if ( m_bNoListRepeats && iNumCurrentSlideLists > 1 && m_iCurrentSlideList == iOldSlideList )
			{
				++m_iCurrentSlideList;

				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
			}

			break;
		}

		case SLIDESHOW_CYCLE_FORWARD:
			if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
				m_iCurrentSlideList = 0;
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			if ( m_iCurrentSlideList < 0 )
				m_iCurrentSlideList = iNumCurrentSlideLists - 1;
			break;
	}

	SlideMaterialList_t *pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];

	// Cycle in the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
			m_iCurrentSlide = RandomInt( 0, std::ssize( pSlideMaterialList->iSlideMaterials ) - 1 );
			break;

		case SLIDESHOW_CYCLE_FORWARD:
			++m_iCurrentSlide;
			if ( m_iCurrentSlide >= std::ssize( pSlideMaterialList->iSlideMaterials ) )
			{
				++m_iCurrentSlideList;
				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = 0;
			}
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			--m_iCurrentSlide;
			if ( m_iCurrentSlide < 0 )
			{
				--m_iCurrentSlideList;
				if ( m_iCurrentSlideList < 0 )
					m_iCurrentSlideList = iNumCurrentSlideLists - 1;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = std::ssize( pSlideMaterialList->iSlideMaterials ) - 1;
			}
			break;
	}

	// Set the current material to what we've cycled to
	m_iCurrentMaterialIndex = pSlideMaterialList->iSlideMaterials[ m_iCurrentSlide ];
	m_iCurrentSlideIndex = pSlideMaterialList->iSlideIndex[ m_iCurrentSlide ];
}

void CSlideshowDisplay::BuildSlideShowImagesList( void )
{
	FileFindHandle_t matHandle;
	char szDirectory[k_cMaxPath];
	char szMatFileName[k_cMaxPath] = {'\0'};
	char szFileBuffer[ SLIDESHOW_LIST_BUFFER_MAX ];
	char *pchCurrentLine = nullptr;

	{
		V_snprintf( szDirectory, sizeof( szDirectory ), "materials/vgui/%s/*.vmt", m_szSlideshowDirectory );
		const char *pMatFileName = g_pFullFileSystem->FindFirst( szDirectory, &matHandle );

		if ( pMatFileName )
			V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
	}

	int iSlideIndex = 0;

	while ( szMatFileName[ 0 ] )
	{
		char szFileName[k_cMaxPath];
		V_snprintf( szFileName, sizeof( szFileName ), "vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );
		szFileName[ V_strlen( szFileName ) - 4 ] = '\0';

		int iMatIndex = ::GetMaterialIndex( szFileName );

		// Get material keywords
		char szFullFileName[k_cMaxPath];
		V_snprintf( szFullFileName, sizeof( szFullFileName ), "materials/vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );

		KeyValues *pMaterialKeys = new KeyValues( "material" );
		bool bLoaded = pMaterialKeys->LoadFromFile( g_pFullFileSystem, szFullFileName, nullptr );
		if ( bLoaded )
		{
			char szKeywords[ 256 ];
			V_strcpy_safe( szKeywords, pMaterialKeys->GetString( "%keywords", "" ) );

			char *pchKeyword = szKeywords;

			while ( pchKeyword[ 0 ] != '\0' )
			{
				char *pNextKeyword = pchKeyword;

				// Skip commas and spaces
				while ( pNextKeyword[ 0 ] != '\0' && pNextKeyword[ 0 ] != ',' )
					++pNextKeyword;

				if ( pNextKeyword[ 0 ] != '\0' )
				{
					pNextKeyword[ 0 ] = '\0';
					++pNextKeyword;

					while ( pNextKeyword[ 0 ] != '\0' && ( pNextKeyword[ 0 ] == ',' || pNextKeyword[ 0 ] == ' ' ) )
						++pNextKeyword;
				}

				// Find the list with the current keyword
				int iList;
				for ( iList = 0; iList < std::ssize( m_SlideMaterialLists ); ++iList )
				{
					if ( V_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, pchKeyword ) == 0 )
						break;
				}

				if ( iList >= std::ssize( m_SlideMaterialLists ) )
				{
					// Couldn't find the list, so create it
					iList = std::ssize( m_SlideMaterialLists ); m_SlideMaterialLists.push_back( new SlideMaterialList_t );
					V_strcpy_safe( m_SlideMaterialLists[ iList ]->szSlideKeyword, pchKeyword );
				}

				// Add material index to this list
				m_SlideMaterialLists[ iList ]->iSlideMaterials.push_back( iMatIndex );
				m_SlideMaterialLists[ iList ]->iSlideIndex.push_back( iSlideIndex );

				pchKeyword = pNextKeyword;
			}
		}
		pMaterialKeys->deleteThis();
		pMaterialKeys = nullptr;

		// Find the generic list
		int iList;
		for ( iList = 0; iList < std::ssize( m_SlideMaterialLists ); ++iList )
		{
			if ( V_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, "" ) == 0 )
				break;
		}

		if ( iList >= std::ssize( m_SlideMaterialLists ) )
		{
			// Couldn't find the generic list, so create it
			m_SlideMaterialLists.insert( m_SlideMaterialLists.begin(), new SlideMaterialList_t ); iList = 0;
			V_strcpy_safe( m_SlideMaterialLists[ iList ]->szSlideKeyword, "" );
		}

		// Add material index to this list
		m_SlideMaterialLists[ iList ]->iSlideMaterials.push_back( iMatIndex );
		m_SlideMaterialLists[ iList ]->iSlideIndex.push_back( iSlideIndex );
		
		{
			const char *pMatFileName = g_pFullFileSystem->FindNext( matHandle );

			if ( pMatFileName )
				V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
			else
				szMatFileName[ 0 ] = '\0';
		}

		++iSlideIndex;
	}

	g_pFullFileSystem->FindClose( matHandle );
}
