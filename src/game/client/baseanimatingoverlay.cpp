// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "bone_setup.h"
#include "client_class.h"
#include "edict.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
#include "game/client/iclientrendertargets.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "networkvar.h"
#include "vphysics_interface.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.numeric;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interpolatedvar;
import ks.tier1.lerp_functions;
import ks.tier1.rangecheckedvar;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :baseentity;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :util;
import :sharedvar;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :particlemgr;
import ks.game.mapentities_shared;
import ks.core.cmdline;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.registry;
import ks.game.gamemovement;
import :cstrike15.cs_player;
import :cstrike15.cs_player_shared;
import ks.core.random;
import ks.math.vector2d;
import :cstrike15.cs_shareddefs;
import :econ.econ_item_schema_minimal;

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseEntity;

using namespace ::ks::reflect;

extern ConVar r_sequence_debug;

mstudioevent_for_client_server_t *GetEventIndexForSequence( mstudioseqdesc_t &seqdesc );

void CAnimationLayer::SetOwner( CBaseAnimatingOverlay *pOverlay )
{
	m_pOwner = pOverlay;
}

CBaseAnimatingOverlay *CAnimationLayer::GetOwner() const
{
	return m_pOwner;
}

void CAnimationLayer::Reset()
{
	if ( m_pOwner )
	{
		int nFlags = 0;
		if ( m_nSequence != 0 || m_flWeight != 0.0f )
		{
			nFlags |= BOUNDS_CHANGED;
		}
		if ( m_flCycle != 0.0f )
		{
			nFlags |= ANIMATION_CHANGED;
		}
		if ( nFlags )
		{
			m_pOwner->InvalidatePhysicsRecursive( nFlags );
		}
	}

	m_nSequence = 0;
	m_flPrevCycle = 0;
	m_flWeight = 0;
	m_flWeightDeltaRate = 0;
	m_flPlaybackRate = 0;
	m_flCycle = 0;
	m_flLayerAnimtime = 0;
	m_flLayerFadeOuttime = 0;
}

void CAnimationLayer::SetSequence( int nSequence )
{
	if ( m_pOwner && m_nSequence != nSequence )
	{
		m_pOwner->InvalidatePhysicsRecursive( BOUNDS_CHANGED );
	}
	m_nSequence = nSequence;
}

void CAnimationLayer::SetCycle( float flCycle )
{
	if ( m_pOwner && m_flCycle != flCycle )
	{
		m_pOwner->InvalidatePhysicsRecursive( ANIMATION_CHANGED );
	}
	m_flCycle = flCycle;
}

void CAnimationLayer::SetOrder( int order )
{
	if ( m_pOwner && ( m_nOrder != order ) )
	{
		if ( m_nOrder == CBaseAnimatingOverlay::MAX_OVERLAYS || order == CBaseAnimatingOverlay::MAX_OVERLAYS )
		{
			m_pOwner->InvalidatePhysicsRecursive( BOUNDS_CHANGED );
		}
	}
	m_nOrder = order;
}

void CAnimationLayer::SetWeight( float flWeight )
{
	if ( m_pOwner && m_flWeight != flWeight )
	{
		if ( m_flWeight == 0.0f || flWeight == 0.0f )
		{
			m_pOwner->InvalidatePhysicsRecursive( BOUNDS_CHANGED );
		}
	}
	m_flWeight = flWeight;
}

CBaseAnimatingOverlay::CBaseAnimatingOverlay()
{
	m_AnimOverlay.reserve( MAX_OVERLAYS );	// layer storage never moves

	// FIXME: where does this initialization go now?
	// AddVar( m_Layer, &m_iv_AnimOverlay, LATCH_ANIMATION_VAR );
}

#undef CBaseAnimatingOverlay

void RecvProxy_SequenceChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;

	if ( pLayer->GetOwner() )
		pLayer->GetOwner()->NotifyOnLayerChangeSequence( pLayer, pData->m_Value.m_Int );

	pLayer->SetSequence( pData->m_Value.m_Int );
}

void RecvProxy_WeightChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;

	if ( pLayer->GetOwner() )
		pLayer->GetOwner()->NotifyOnLayerChangeWeight( pLayer, pData->m_Value.m_Float );

	pLayer->SetWeight( pData->m_Value.m_Float );
}

void RecvProxy_WeightDeltaRateChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;
	pLayer->SetWeightDeltaRate( pData->m_Value.m_Float );
}

void RecvProxy_CycleChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;

	if ( pLayer->GetOwner() )
		pLayer->GetOwner()->NotifyOnLayerChangeCycle( pLayer, pData->m_Value.m_Float );

	pLayer->SetCycle( pData->m_Value.m_Float );
}

void RecvProxy_PlaybackRateChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;
	pLayer->SetPlaybackRate( pData->m_Value.m_Float );
}

void RecvProxy_OrderChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CAnimationLayer *pLayer = (CAnimationLayer *)pStruct;
	pLayer->SetOrder( pData->m_Value.m_Int );
}

IMPLEMENT_REFLECT_TABLE( CAnimationLayer, DT_Animationlayer );

const char *s_m_iv_AnimOverlayNames[CBaseAnimatingOverlay::MAX_OVERLAYS] =
{
	"CBaseAnimatingOverlay::m_iv_AnimOverlay00",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay01",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay02",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay03",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay04",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay05",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay06",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay07",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay08",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay09",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay10",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay11",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay12",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay13",
	"CBaseAnimatingOverlay::m_iv_AnimOverlay14"
};

void ResizeAnimationLayerCallback( void *pStruct, int offsetToUtlVector, int len )
{
	CBaseAnimatingOverlay *pEnt = (CBaseAnimatingOverlay*)pStruct;
	std::vector< CAnimationLayer > &layers = pEnt->m_AnimOverlay;
	std::vector< CInterpolatedVar< CAnimationLayer > > *pVecIV = &pEnt->m_iv_AnimOverlay;
	
	contract_assert( (char*)&layers - (char*)pEnt == offsetToUtlVector );
	contract_assert( std::ssize( layers ) == std::ssize( *pVecIV ) || std::ssize( *pVecIV ) == 0 );
	contract_assert( len >= 0 && len <= CBaseAnimatingOverlay::MAX_OVERLAYS );	// RecvProxy_UtlVectorLength clamps

	int diff = len - std::ssize( layers );
	if ( diff != 0 )
	{
		for ( CAnimationLayer &layer : layers )
			pEnt->RemoveVar( &layer );

		pEnt->InvalidatePhysicsRecursive( BOUNDS_CHANGED );

		if ( diff > 0 )
		{
			for ( int i = 0; i < diff; ++i )
				layers.emplace_back().SetOwner( pEnt );
			pVecIV->resize( pVecIV->size() + diff );
		}
		else
		{
			layers.resize( len );
			pVecIV->erase( pVecIV->begin() + len, pVecIV->begin() + len + ( -diff ) );
		}

		// Rebind all the variables in the ent's list.
		for ( int i=0; i < len; i++ )
		{
			IInterpolatedVar *pWatcher = &(*pVecIV)[ i ];
			pWatcher->SetDebugName( s_m_iv_AnimOverlayNames[i] );
			pEnt->AddVar( &layers[i], pWatcher, LATCH_ANIMATION_VAR, true );
		}
	}

	// FIXME: need to set historical values of nOrder in pVecIV to MAX_OVERLAY
}

IMPLEMENT_REFLECT_TABLE_IN( CBaseAnimatingOverlay, DT_OverlayVars );

IMPLEMENT_REFLECT_CLIENTCLASS( CBaseAnimatingOverlay, DT_BaseAnimatingOverlay, CBaseAnimatingOverlay )

IMPLEMENT_REFLECT_PREDMAP( CBaseAnimatingOverlay );

CAnimationLayer* CBaseAnimatingOverlay::GetAnimOverlay( int i, bool bUseOrder )
{
	contract_assert( i >= 0 && i < MAX_OVERLAYS );

	if ( m_AnimOverlay.empty() )
		return nullptr;

	if ( bUseOrder )
	{
		for ( CAnimationLayer &layer : m_AnimOverlay )
		{
			if ( layer.GetOrder() == i )
				return &layer;
		}
	}

	return &m_AnimOverlay[i];
}

void CBaseAnimatingOverlay::SetNumAnimOverlays( int num )
{
	while ( std::ssize( m_AnimOverlay ) < num )
		m_AnimOverlay.emplace_back().SetOwner( this );

	if ( std::ssize( m_AnimOverlay ) > num )
	{
		m_AnimOverlay.resize( num );
		InvalidatePhysicsRecursive( BOUNDS_CHANGED );
	}
}

int CBaseAnimatingOverlay::GetNumAnimOverlays() const
{
	return std::ssize( m_AnimOverlay );
}

void CBaseAnimatingOverlay::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	BaseClass::GetRenderBounds( theMins, theMaxs );

	if ( IsRagdoll() )
		return;

	MDLCACHE_CRITICAL_SECTION();

	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr || !pStudioHdr->SequencesAvailable() )
		return;

	int nSequences = pStudioHdr->GetNumSeq();

	int i;
	for (i = 0; i < std::ssize( m_AnimOverlay ); i++)
	{
		if ( m_AnimOverlay[i].m_flWeight > 0.0 && m_AnimOverlay[i].m_nOrder != MAX_OVERLAYS )
		{
			if ( m_AnimOverlay[i].m_nSequence >= nSequences )
				continue;

			mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( m_AnimOverlay[i].m_nSequence );
			VectorMin( seqdesc.bbmin, theMins, theMins );
			VectorMax( seqdesc.bbmax, theMaxs, theMaxs );
		}
	}
}

bool CBaseAnimatingOverlay::Interpolate( float flCurrentTime )
{
	bool bOk = BaseClass::Interpolate( flCurrentTime );

	CheckForLayerPhysicsInvalidate();

	return bOk;
}

void CBaseAnimatingOverlay::CheckForLayerChanges( CStudioHdr *hdr, float currentTime )
{
	CDisableRangeChecks disableRangeChecks;
	
	// FIXME: damn, there has to be a better way than this.
	int i;
	for (i = 0; i < std::ssize( m_iv_AnimOverlay ); i++)
	{
		CDisableRangeChecks disableRangeChecks; 

		int iHead, iPrev1, iPrev2;
		m_iv_AnimOverlay[i].GetInterpolationInfo( currentTime, &iHead, &iPrev1, &iPrev2 );

		// fake up previous cycle values.
		float t0;
		CAnimationLayer *pHead = m_iv_AnimOverlay[i].GetHistoryValue( iHead, t0 );
		// reset previous
		float t1;
		CAnimationLayer *pPrev1 = m_iv_AnimOverlay[i].GetHistoryValue( iPrev1, t1 );
		// reset previous previous
		float t2;
		CAnimationLayer *pPrev2 = m_iv_AnimOverlay[i].GetHistoryValue( iPrev2, t2 );

		if ( !pHead || !pPrev1 || pHead->m_nSequence == pPrev1->m_nSequence )
			continue;

		if (r_sequence_debug.GetInt() == entindex())
		{
			DevMsgRT( "(%7.4f : %30s : %5.3f : %4.2f : %1d)\n", t0, hdr->pSeqdesc( pHead->m_nSequence ).pszLabel(),  (float)pHead->m_flCycle,  (float)pHead->m_flWeight, i );
			DevMsgRT( "(%7.4f : %30s : %5.3f : %4.2f : %1d)\n", t1, hdr->pSeqdesc( pPrev1->m_nSequence ).pszLabel(),  (float)pPrev1->m_flCycle, (float)pPrev1->m_flWeight, i );
			if (pPrev2)
				DevMsgRT( "(%7.4f : %30s : %5.3f : %4.2f : %1d)\n", t2, hdr->pSeqdesc( pPrev2->m_nSequence ).pszLabel(),  (float)pPrev2->m_flCycle,  (float)pPrev2->m_flWeight, i );
		}

		pPrev1->m_nSequence = pHead->m_nSequence;
		pPrev1->m_flCycle = pHead->m_flPrevCycle;
		pPrev1->m_flWeight = pHead->m_flWeight;

		if (pPrev2)
		{
			float num = 0;
			if ( fabs( t0 - t1 ) > 0.001f )
				num = (t2 - t1) / (t0 - t1);

			pPrev2->m_nSequence = pHead->m_nSequence;
			float flTemp;
			if (IsSequenceLooping( hdr, pHead->m_nSequence ))
			{
				flTemp = LoopingLerp( num, (float)pHead->m_flPrevCycle, (float)pHead->m_flCycle );
			}
			else
			{
				flTemp = Lerp( num, (float)pHead->m_flPrevCycle, (float)pHead->m_flCycle );
			}
			pPrev2->m_flCycle = flTemp;
			pPrev2->m_flWeight = pHead->m_flWeight;
		}

		/*
		if (stricmp( r_seq_overlay_debug.GetString(), hdr->name ) == 0)
		{
			DevMsgRT( "(%30s %6.2f : %6.2f : %6.2f)\n", hdr->pSeqdesc( pHead->nSequence ).pszLabel(), (float)pPrev2->m_flCycle, (float)pPrev1->m_flCycle, (float)pHead->m_flCycle );
		}
		*/

		m_iv_AnimOverlay[i].SetLooping( IsSequenceLooping( hdr, pHead->m_nSequence ) );
		m_iv_AnimOverlay[i].Interpolate( currentTime );

		// reset event indexes
		m_flOverlayPrevEventCycle[i] = pHead->m_flPrevCycle - 0.01;
	}
}

//#define DEBUG_TF2_OVERLAYS
void CBaseAnimatingOverlay::AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	BaseClass::AccumulateLayers( boneSetup, pos, q, currentTime );
	int i;

	// resort the layers
	int layer[MAX_OVERLAYS];
	for (i = 0; i < MAX_OVERLAYS; i++)
	{
		layer[i] = MAX_OVERLAYS;
	}

	for (i = 0; i < std::ssize( m_AnimOverlay ); i++)
	{
		CAnimationLayer *pLayer = GetAnimOverlay( i );
		if ( pLayer )
		{
			layer[i] = clamp( pLayer->GetOrder(), 0, MAX_OVERLAYS - 1 );
		}
	}

	CheckForLayerChanges( boneSetup.GetStudioHdr(), currentTime );

	int nSequences = boneSetup.GetStudioHdr()->GetNumSeq();

	// add in the overlay layers
	int j;
	for (j = 0; j < MAX_OVERLAYS; j++)
	{
		i = layer[ j ];
		if ( i >= std::ssize( m_AnimOverlay ) )
		{
			continue;
		}

		if ( m_AnimOverlay[i].m_nSequence >= nSequences )
			continue;

		/*
		DevMsgRT( 1 , "%.3f  %.3f  %.3f\n", currentTime, fWeight, dadt );
		debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector( 0, 0, 64 ), -j - 1, 0, 
			"%2d(%s) : %6.2f : %6.2f", 
				m_AnimOverlay[i].m_nSequence,
				boneSetup.GetStudioHdr()->pSeqdesc( m_AnimOverlay[i].m_nSequence )->pszLabel(),
				m_AnimOverlay[i].m_flCycle, 
				m_AnimOverlay[i].m_flWeight
				);
		*/

		float fWeight = m_AnimOverlay[i].m_flWeight;
		if ( fWeight <= 0.0f )
		{
			continue;
		}

		// check to see if the sequence changed
		// FIXME: move this to somewhere more reasonable
		// do a nice spline interpolation of the values
		// if ( m_AnimOverlay[i].m_nSequence != m_iv_AnimOverlay.GetPrev( i )->nSequence )
		float fCycle = m_AnimOverlay[ i ].m_flCycle;
		fCycle = ClampCycle( fCycle, IsSequenceLooping( m_AnimOverlay[i].m_nSequence ) );

		if ( !IsFinite( fCycle ) )
		{
			contract_assert( false && "fCycle is nan!" );
			fCycle = 0;
		}

		if (fWeight > 1.0f)
		{
			fWeight = 1.0f;
		}

		boneSetup.AccumulatePose( pos, q, m_AnimOverlay[i].m_nSequence, fCycle, fWeight, currentTime, m_pIk );

		if (r_sequence_debug.GetInt() == entindex())
		{
			if (1)
			{
				DevMsgRT( "%8.4f : %30s : %5.3f : %4.2f : %1d\n", currentTime, boneSetup.GetStudioHdr()->pSeqdesc( m_AnimOverlay[i].m_nSequence ).pszLabel(), fCycle, fWeight, i );
			}
			else
			{
				int iHead, iPrev1, iPrev2;
				m_iv_AnimOverlay[i].GetInterpolationInfo( currentTime, &iHead, &iPrev1, &iPrev2 );

				// fake up previous cycle values.
				float t0;
				CAnimationLayer *pHead = m_iv_AnimOverlay[i].GetHistoryValue( iHead, t0 );
				// reset previous
				float t1;
				CAnimationLayer *pPrev1 = m_iv_AnimOverlay[i].GetHistoryValue( iPrev1, t1 );
				// reset previous previous
				float t2;
				CAnimationLayer *pPrev2 = m_iv_AnimOverlay[i].GetHistoryValue( iPrev2, t2 );

				if ( pHead && pPrev1 && pPrev2 )
				{
					DevMsgRT( "%6.2f : %30s %6.2f (%6.2f:%6.2f:%6.2f) : %6.2f (%6.2f:%6.2f:%6.2f) : %1d\n", currentTime, boneSetup.GetStudioHdr()->pSeqdesc( m_AnimOverlay[i].m_nSequence ).pszLabel(), 
						fCycle, (float)pPrev2->m_flCycle, (float)pPrev1->m_flCycle, (float)pHead->m_flCycle,
						fWeight, (float)pPrev2->m_flWeight, (float)pPrev1->m_flWeight, (float)pHead->m_flWeight,
						i );
				}
				else
				{
					DevMsgRT( "%6.2f : %30s %6.2f : %6.2f : %1d\n", currentTime, boneSetup.GetStudioHdr()->pSeqdesc( m_AnimOverlay[i].m_nSequence ).pszLabel(), fCycle, fWeight, i );
				}

			}
		}
	}
	//RegenerateDispatchedLayers( boneSetup, pos, q, currentTime );
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the sequence or weapon changed, if so find a matching sequence or clear the dispatch
//-----------------------------------------------------------------------------

bool CBaseAnimatingOverlay::UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence )
{
	if ( !pWeaponStudioHdr || !pLayer )
	{
		if ( pLayer )
			pLayer->m_nDispatchedDst = ACT_INVALID;
		return false;
	}	

	if ( pLayer->m_pDispatchedStudioHdr != pWeaponStudioHdr || pLayer->m_nDispatchedSrc != iSequence || pLayer->m_nDispatchedDst >= pWeaponStudioHdr->GetNumSeq()  )
	{
		pLayer->m_pDispatchedStudioHdr = pWeaponStudioHdr;
		pLayer->m_nDispatchedSrc = iSequence;
		if ( pWeaponStudioHdr )
		{
			const char *pszLayerName = GetSequenceName( iSequence );
			pLayer->m_nDispatchedDst = pWeaponStudioHdr->LookupSequence( pszLayerName );
		}
		else
		{
			pLayer->m_nDispatchedDst = ACT_INVALID;
		}
	}
	return (pLayer->m_nDispatchedDst != ACT_INVALID );
}

//-----------------------------------------------------------------------------
// Purpose: Play overlay sequences on dispatched model, merge results back to parent model
//-----------------------------------------------------------------------------

void CBaseAnimatingOverlay::AccumulateInterleavedDispatchedLayers( CBaseAnimatingOverlay *pWeapon, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime, bool bSetupInvisibleWeapon /* = false */ )
{
	bool bSetupWeapon = pWeapon != nullptr && pWeapon->m_pBoneMergeCache != nullptr && (pWeapon->IsVisible() || bSetupInvisibleWeapon) ;
	
	// reset event frame indices, etc
	CheckForLayerChanges( boneSetup.GetStudioHdr(), currentTime );

	if ( bSetupWeapon )
	{
		CStudioHdr *pWeaponStudioHdr = pWeapon->GetModelPtr();

		// copy matching player pose params to weapon pose params
		pWeapon->m_pBoneMergeCache->MergeMatchingPoseParams();
		float poseparam[MAXSTUDIOPOSEPARAM];
		pWeapon->GetPoseParameters( pWeaponStudioHdr, poseparam );

		// build a temporary setup for the weapon
		CIKContext weaponIK;
		weaponIK.Init( pWeaponStudioHdr, GetAbsAngles(), GetAbsOrigin(), gpGlobals->curtime, 0, BONE_USED_BY_BONE_MERGE );

		IBoneSetup weaponSetup( pWeaponStudioHdr, BONE_USED_BY_BONE_MERGE, poseparam );
		BoneVector weaponPos[MAXSTUDIOBONES];
		BoneQuaternionAligned weaponQ[MAXSTUDIOBONES];

		int nSequences = boneSetup.GetStudioHdr()->GetNumSeq();
		for ( int nLayerIdx = 0; nLayerIdx < GetNumAnimOverlays(); nLayerIdx++ )
		{
			CAnimationLayer *pLayer = GetAnimOverlay(nLayerIdx);

			if ( pLayer->GetSequence() <= 1 || pLayer->GetSequence() >= nSequences || pLayer->GetWeight() <= 0 )
				continue;

			float fCycle = pLayer->GetCycle();
			fCycle = ClampCycle( fCycle, IsSequenceLooping( pLayer->GetSequence() ) );

			UpdateDispatchLayer( pLayer, pWeaponStudioHdr, pLayer->GetSequence() );

			if ( pLayer->m_nDispatchedDst > 0 && pLayer->m_nDispatchedDst < pWeaponStudioHdr->GetNumSeq() )
			{
				// copy player bones to weapon setup bones
				pWeapon->m_pBoneMergeCache->CopyFromFollow( pos, q, BONE_USED_BY_BONE_MERGE, weaponPos, weaponQ );

				// respect ik rules on archetypal sequence, even if we're not playing it
				//mstudioseqdesc_t &seqdesc = ((CStudioHdr *)m_pStudioHdr)->pSeqdesc( pLayer->GetSequence() );
				//m_pIk->AddDependencies( seqdesc, pLayer->GetSequence(), pLayer->GetCycle(), m_flPoseParameter, pLayer->GetWeight() );

				// now that the weapon bones are in the position of the current player bones, set up the weapon animation onto that
				weaponSetup.AccumulatePose( weaponPos, weaponQ, pLayer->m_nDispatchedDst, pLayer->GetCycle(), pLayer->GetWeight(), currentTime, &weaponIK );

				//DrawSkeleton( this->GetModelPtr(), BONE_USED_BY_ANYTHING );
				//pWeapon->DrawSkeleton( pWeaponStudioHdr, BONE_USED_BY_ANYTHING );

				// merge weapon bones back
				pWeapon->m_pBoneMergeCache->CopyToFollow( weaponPos, weaponQ, BONE_USED_BY_BONE_MERGE, pos, q );

				weaponIK.CopyTo( m_pIk, pWeapon->m_pBoneMergeCache->GetRawIndexMapping() );
			}
			else
			{
				boneSetup.AccumulatePose( pos, q, pLayer->GetSequence(), fCycle, pLayer->GetWeight(), currentTime, m_pIk );
			}

		}

		//if ( bRanAnyWeaponLayers )
		//{
		//	
		//
		//	CBoneBitList boneComputed;
		//	
		//	pWeapon->UpdateIKLocks( currentTime );
		//	weaponIK.UpdateTargets( pos, q, pWeapon->m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );
		//	
		//	pWeapon->CalculateIKLocks( currentTime );
		//	weaponIK.SolveDependencies( pos, q, pWeapon->m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );
		//	
		//}
	}
	else
	{
		int nSequences = boneSetup.GetStudioHdr()->GetNumSeq();
		for ( int nLayerIdx = 0; nLayerIdx < GetNumAnimOverlays(); nLayerIdx++ )
		{
			CAnimationLayer *pLayer = GetAnimOverlay(nLayerIdx);

			if ( pLayer->GetSequence() < 0 || pLayer->GetSequence() >= nSequences || pLayer->GetWeight() <= 0 )
				continue;

			float fCycle = pLayer->GetCycle();
			fCycle = ClampCycle( fCycle, IsSequenceLooping( pLayer->GetSequence() ) );

			boneSetup.AccumulatePose( pos, q, pLayer->GetSequence(), fCycle, pLayer->GetWeight(), currentTime, m_pIk );
		}
	}

}

void CBaseAnimatingOverlay::AccumulateDispatchedLayers( CBaseAnimatingOverlay *pWeapon, CStudioHdr *pWeaponStudioHdr,  IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	if ( !pWeapon->m_pBoneMergeCache )
		return;

	if ( !pWeapon->IsVisible() )
		return;

	// copy matching player pose params to weapon pose params
	pWeapon->m_pBoneMergeCache->MergeMatchingPoseParams();
	float		poseparam[MAXSTUDIOPOSEPARAM];
	pWeapon->GetPoseParameters( pWeaponStudioHdr, poseparam );

	// build a temporary setup for the weapon
	CIKContext weaponIK;
	weaponIK.Init( pWeaponStudioHdr, GetAbsAngles(), GetAbsOrigin(), gpGlobals->curtime, 0, BONE_USED_BY_BONE_MERGE );

	IBoneSetup weaponSetup( pWeaponStudioHdr, BONE_USED_BY_BONE_MERGE, poseparam );
	BoneVector weaponPos[MAXSTUDIOBONES];
	BoneQuaternionAligned weaponQ[MAXSTUDIOBONES];

	// copy player bones to weapon setup bones
	pWeapon->m_pBoneMergeCache->CopyFromFollow( pos, q, BONE_USED_BY_BONE_MERGE, weaponPos, weaponQ );

	// do layer animations
	// FIXME: some of the layers are player layers, not weapon layers
	// FIXME: how to interleave?
	for ( int i=0; i < GetNumAnimOverlays(); i++ )
	{
		CAnimationLayer *pLayer = GetAnimOverlay( i );
		if ( pLayer->GetOrder() >= MAX_OVERLAYS || pLayer->GetSequence() <= 1 || pLayer->GetWeight() <= 0.0f )
			continue;

		UpdateDispatchLayer( pLayer, pWeaponStudioHdr, pLayer->GetSequence() );

		if ( pLayer->m_nDispatchedDst > 0 && pLayer->m_nDispatchedDst < pWeaponStudioHdr->GetNumSeq() )
		{
			weaponSetup.AccumulatePose( weaponPos, weaponQ, pLayer->m_nDispatchedDst, pLayer->GetCycle(), pLayer->GetWeight(), currentTime, &weaponIK );
		}
	}
	// FIXME: merge weaponIK into m_pIK

	CBoneBitList boneComputed;
	
	pWeapon->UpdateIKLocks( currentTime );
	weaponIK.UpdateTargets( weaponPos, weaponQ, pWeapon->m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );

	pWeapon->CalculateIKLocks( currentTime );
	weaponIK.SolveDependencies( weaponPos, weaponQ, pWeapon->m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );

	// merge weapon bones back
	pWeapon->m_pBoneMergeCache->CopyToFollow( weaponPos, weaponQ, BONE_USED_BY_BONE_MERGE, pos, q );
}

//-----------------------------------------------------------------------------
// Purpose: Duplicate parent models dispatched overlay sequences so that any local bones get animated
//-----------------------------------------------------------------------------
	
void CBaseAnimatingOverlay::RegenerateDispatchedLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	// find who I'm following and see if I'm their dispatched model
	if ( m_pBoneMergeCache && m_pBoneMergeCache->IsCopied() )
	{
		CBaseEntity *pFollowEnt = GetFollowedEntity();
		if ( pFollowEnt )
		{
			CBaseAnimatingOverlay *pFollow = pFollowEnt->GetBaseAnimatingOverlay();
			if ( pFollow )
			{
				for ( int i=0; i < pFollow->GetNumAnimOverlays(); i++ )
				{
					CAnimationLayer *pLayer = pFollow->GetAnimOverlay( i );
					if ( pLayer->m_pDispatchedStudioHdr == nullptr || pLayer->GetOrder() >= MAX_OVERLAYS || pLayer->GetSequence() == -1 || pLayer->GetWeight() <= 0.0f )
						continue;

					// FIXME: why do the CStudioHdr's not match?
					if ( pLayer->m_pDispatchedStudioHdr->GetRenderHdr() == boneSetup.GetStudioHdr()->GetRenderHdr() )
					{
						if ( pLayer->m_nDispatchedDst != ACT_INVALID )
						{
							boneSetup.AccumulatePose( pos, q, pLayer->m_nDispatchedDst, pLayer->m_flCycle, pLayer->m_flWeight, currentTime, m_pIk );
						}
					}
				}
			}
		}
	}
}

void CBaseAnimatingOverlay::DoAnimationEvents( CStudioHdr *pStudioHdr )
{
	MDLCACHE_CRITICAL_SECTION();
	if ( !pStudioHdr || !pStudioHdr->SequencesAvailable() )
		return;

	int nSequences = pStudioHdr->GetNumSeq();

	BaseClass::DoAnimationEvents( pStudioHdr );

	bool watch = false; // V_strstr( hdr->name, "rifle" ) ? true : false;

	CheckForLayerChanges( pStudioHdr, gpGlobals->curtime ); // !!!

	int j;
	for (j = 0; j < std::ssize( m_AnimOverlay ); j++)
	{
		if ( m_AnimOverlay[j].m_nSequence < 0 || m_AnimOverlay[j].m_nSequence >= nSequences )
		{
			continue;
		}

		// Don't bother with 0-weight layers
		if ( m_AnimOverlay[j].m_flWeight == 0.0f || m_AnimOverlay[j].m_nOrder == MAX_OVERLAYS )
		{
			continue;
		}

		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( m_AnimOverlay[j].m_nSequence );
		if ( seqdesc.numevents == 0 )
			continue;

		// stalled?
		if (m_AnimOverlay[j].m_flCycle == m_flOverlayPrevEventCycle[j])
			continue;

		bool bLoopingSequence = IsSequenceLooping( m_AnimOverlay[j].m_nSequence );

		bool bLooped = false;

		//in client code, m_flOverlayPrevEventCycle is set to -1 when we first start an overlay, looping or not
		if ( bLoopingSequence &&
			m_flOverlayPrevEventCycle[j] > 0.0f &&
			m_AnimOverlay[j].m_flCycle <= m_flOverlayPrevEventCycle[j] )
		{
			if (m_flOverlayPrevEventCycle[j] - m_AnimOverlay[j].m_flCycle > 0.5)
			{
				bLooped = true;
			}
			else
			{
				// things have backed up, which is bad since it'll probably result in a hitch in the animation playback
				// but, don't play events again for the same time slice
				return;
			}
		}

		mstudioevent_for_client_server_t *pevent = GetEventIndexForSequence( seqdesc );

		// This makes sure events that occur at the end of a sequence occur are
		// sent before events that occur at the beginning of a sequence.
		if (bLooped)
		{
			for (int i = 0; i < (int)seqdesc.numevents; i++)
			{
				// ignore all non-client-side events
				if ( pevent[i].type & AE_TYPE_NEWEVENTSYSTEM )
				{
					if ( !(pevent[i].type & AE_TYPE_CLIENT) )
						 continue;
				}
				else if ( pevent[i].Event_OldSystem() < EVENT_CLIENT ) //Adrian - Support the old event system
					continue;
			
				if ( pevent[i].cycle <= m_flOverlayPrevEventCycle[j] )
					continue;
				
				if ( watch )
				{
					Msg( "%i FE %i Looped cycle %f, prev %f ev %f (time %.3f)\n",
						gpGlobals->tickcount,
						pevent[i].Event(),
						pevent[i].cycle,
						(float)m_flOverlayPrevEventCycle[j],
						(float)m_AnimOverlay[j].m_flCycle,
						gpGlobals->curtime );
				}
					
				FireEvent( GetAbsOrigin(), GetAbsAngles(), pevent[ i ].Event(), pevent[ i ].pszOptions() );
			}

			// Necessary to get the next loop working
			m_flOverlayPrevEventCycle[j] = -0.01;
		}

		for (int i = 0; i < (int)seqdesc.numevents; i++)
		{
			if ( pevent[i].type & AE_TYPE_NEWEVENTSYSTEM )
			{
				if ( !(pevent[i].type & AE_TYPE_CLIENT) )
					 continue;
			}
			else if ( pevent[i].Event_OldSystem() < EVENT_CLIENT ) //Adrian - Support the old event system
				continue;

			bool bStartedSequence = ( m_flOverlayPrevEventCycle[j] > m_AnimOverlay[j].m_flCycle || m_flOverlayPrevEventCycle[j] == 0 );

			if ( ( ( pevent[i].cycle > m_flOverlayPrevEventCycle[j] || bStartedSequence && pevent[i].cycle == 0 ) && pevent[i].cycle <= m_AnimOverlay[j].m_flCycle) )
			{
				if ( watch )
				{
					Msg( "%i (seq: %d) FE %i Normal cycle %f, prev %f ev %f (time %.3f)\n",
						gpGlobals->tickcount,
						(int)m_AnimOverlay[j].m_nSequence,
						(int)pevent[i].Event(),
						(float)pevent[i].cycle,
						(float)m_flOverlayPrevEventCycle[j],
						(float)m_AnimOverlay[j].m_flCycle,
						gpGlobals->curtime );
				}

				FireEvent( GetAbsOrigin(), GetAbsAngles(), pevent[ i ].Event(), pevent[ i ].pszOptions() );
			}
		}

		m_flOverlayPrevEventCycle[j] = m_AnimOverlay[j].m_flCycle;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CStudioHdr *CBaseAnimatingOverlay::OnNewModel()
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	//// Clear out animation layers
	//for ( int i=0; i < std::ssize( m_AnimOverlay ); i++ )
	//{
	//	m_AnimOverlay[i].Reset();
	//	m_AnimOverlay[i].m_nOrder = MAX_OVERLAYS;
	//}

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseAnimatingOverlay::CheckInterpChanges( void )
{
	CDisableRangeChecks disableRangeChecks; 

	for (int i = 0; i < std::ssize( m_AnimOverlay ); i++)
	{
		int iHead, iPrev1, iPrev2;
		m_iv_AnimOverlay[i].GetInterpolationInfo( gpGlobals->curtime, &iHead, &iPrev1, &iPrev2 );

		float t0;
		CAnimationLayer *pHead = m_iv_AnimOverlay[i].GetHistoryValue( iHead, t0 );

		float t1;
		CAnimationLayer *pPrev = m_iv_AnimOverlay[i].GetHistoryValue( iPrev1, t1 );

		if ( !pHead || !pPrev )
			continue;

		m_AnimOverlay[ i ].m_nInvalidatePhysicsBits = CheckForSequenceBoxChanges( *pHead, *pPrev );
	}

	CheckForLayerPhysicsInvalidate();
}

void CBaseAnimatingOverlay::CheckForLayerPhysicsInvalidate( void )
{
	// When the layers interpolate they may change the animation or bbox so we 
	//  have them accumulate the changes and call InvalidatePhysicsRecursive if any
	//  changes are needed.
	int nInvalidatePhysicsChangeBits = 0;

	int nLayerCount = std::ssize( m_AnimOverlay );
	for ( int i = 0; i < nLayerCount; ++i )
	{
		int nChangeBits = m_AnimOverlay[ i ].m_nInvalidatePhysicsBits;
		if ( nChangeBits )
		{
			nInvalidatePhysicsChangeBits |= nChangeBits;
			continue;
		}
	}

	if ( nInvalidatePhysicsChangeBits )
	{
		InvalidatePhysicsRecursive( nInvalidatePhysicsChangeBits );
	}
}

void CBaseAnimatingOverlay::SetOverlayPrevEventCycle( int nSlot, float flValue )
{
	m_flOverlayPrevEventCycle[nSlot] = flValue;
}

using namespace ::ks::reflect;

// Purpose: Implementation for CBaseClientRenderTargets class.
//			Provides Init functions for common render textures used by the engine.
//			Mod makers can inherit from this class, and call the Create functions for
//			only the render textures the want for their mod.

ConVar cl_disable_water_render_targets( side_registry(), "cl_disable_water_render_targets", "0" );

ITexture* CBaseClientRenderTargets::CreateWaterReflectionTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-reflectionTextureSize", iSize );
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterReflection",
		iSize, iSize, RT_SIZE_PICMIP,
		pMaterialSystem->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CBaseClientRenderTargets::CreateWaterRefractionTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-reflectionTextureSize", iSize );
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterRefraction",
		iSize, iSize, RT_SIZE_PICMIP,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888, 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CBaseClientRenderTargets::CreateCameraTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-monitorTextureSize", iSize );
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Camera",
		iSize, iSize, RT_SIZE_DEFAULT,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

//-----------------------------------------------------------------------------
// Purpose: Called by the engine in material system init and shutdown.
//			Clients should override this in their inherited version, but the base
//			is to init all standard render targets for use.
// Input  : pMaterialSystem - the engine's material system (our singleton is not yet inited at the time this is called)
//			pHardwareConfig - the user hardware config, useful for conditional render target setup
//-----------------------------------------------------------------------------
void CBaseClientRenderTargets::SetupClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig, int iWaterTextureSize, int iCameraTextureSize )
{
	IMaterialSystem *pSave = materials;

	// Make sure our config is loaded before we try to init rendertargets
	ConfigureCurrentSystemLevel();

	// Water effects
	materials = pMaterialSystem;							// in case not initted yet for mat system util
	g_pMaterialSystem = pMaterialSystem;
	g_pMaterialSystemHardwareConfig = pHardwareConfig;
	if ( iWaterTextureSize && !cl_disable_water_render_targets.GetBool() )
	{
		m_WaterReflectionTexture.Init( CreateWaterReflectionTexture( pMaterialSystem, iWaterTextureSize ) );
		m_WaterRefractionTexture.Init( CreateWaterRefractionTexture( pMaterialSystem, iWaterTextureSize ) );
	}

	// mdonofrio - Don't need monitors or flashlight shadows for CS15. 
	// Still need to initrendertargets for shadowmgr (blob/rtt shadows) - it will skip depth textures if convars set appropriately??

	// Monitors
	if ( iCameraTextureSize )
		m_CameraTexture.Init( CreateCameraTexture( pMaterialSystem, iCameraTextureSize ) );

	ITexture *pGlintTexture = pMaterialSystem->CreateNamedRenderTargetTextureEx2( 
		"_rt_eyeglint", 32, 32, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_BGRA8888, MATERIAL_RT_DEPTH_NONE );
	pGlintTexture->IncrementReferenceCount();

	g_CascadeLightManager.InitRenderTargets();

	g_pClientShadowMgr->InitRenderTargets();
	
	m_RenderToRTHelperTexture.Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2( "render_to_rt_helper", 512, 512, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_SINGLECOPY ) );

	materials = pSave;
}

void CBaseClientRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	SetupClientRenderTargets( pMaterialSystem, pHardwareConfig );
}

//-----------------------------------------------------------------------------
// Purpose: Shut down each CTextureReference we created in InitClientRenderTargets.
//			Called by the engine in material system shutdown.
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseClientRenderTargets::ShutdownClientRenderTargets()
{
	// Water effects
	m_WaterReflectionTexture.Shutdown();
	m_WaterRefractionTexture.Shutdown();

	// Monitors
	m_CameraTexture.Shutdown();

	g_pClientShadowMgr->ShutdownRenderTargets();

	g_CascadeLightManager.ShutdownRenderTargets();

	m_RenderToRTHelperTexture.Shutdown();

}

static CBaseClientRenderTargets g_BaseClientRenderTargets;
static InterfaceReg s_Reg_CBaseClientRenderTargets_IClientRenderTargets( side_registry(), []() -> void * { return static_cast<IClientRenderTargets *>( &g_BaseClientRenderTargets ); }, CLIENTRENDERTARGETS_INTERFACE_VERSION );;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Client's CBaseCombatCharacter entity

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatCharacter::CBaseCombatCharacter()
{
	for ( int i=0; i < m_iAmmo.Count(); i++ )
		m_iAmmo.Set( i, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatCharacter::~CBaseCombatCharacter()
{
}

/*
//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( g_pGameRules->GetAmmoDef()->Index(szName) );
}
*/

//-----------------------------------------------------------------------------
// Purpose: Overload our muzzle flash and send it to any actively held weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::DoMuzzleFlash()
{
	// Our weapon takes our muzzle flash command
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

void CBaseCombatCharacter::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// view weapon model cache monitoring
	// NOTE: expected to be updated ONLY once per frame for the primary player ONLY!
	// the expectation is that there is ONLY one customer that requires view models
	// otherwise the lower level code will thrash as it tries to maintain a single player's view model inventory
	{
		const char *viewWeapons[MAX_WEAPONS];
		int nNumViewWeapons = 0;

		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
		if ( pLocalPlayer == this && !pLocalPlayer->IsObserver() )
		{
			// want to know what this player's weapon inventory is to keep all of these in cache
			for ( int i = 0; i < MAX_WEAPONS; i++ )
			{
				CBaseCombatWeapon *pWeapon = GetWeapon( i );
				if ( !pWeapon )
					continue;

				viewWeapons[nNumViewWeapons] = pWeapon->GetViewModel();
				nNumViewWeapons++;
			}
		}
		else if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverTarget() == this ) 
		{
			// once spectating, PURPOSELY only the active view weapon gets tracked
			// cycling through spectators is the more common pattern and tracking just the active weapon prevents massive cache thrashing 
			// otherwise maintaining the observer targets inventories would needlessly thrash the cache as the player rapidly cycles
			CBaseCombatWeapon *pWeapon = pLocalPlayer->GetActiveWeapon();
			if ( pWeapon )
			{
				viewWeapons[nNumViewWeapons] = pWeapon->GetViewModel();
				nNumViewWeapons++;
			}
		}

		if ( nNumViewWeapons )
		{
			// view model weapons are subject to a cache policy that needs to be kept accurate for a SINGLE Player
			modelinfo->UpdateViewWeaponModelCache( viewWeapons, nNumViewWeapons );
		}
	}

	// world weapon model cache monitoring
	// world weapons have a much looser cache policy and just needs to monitor the important set of world weapons
	{
		const char *worldWeapons[MAX_WEAPONS];
		int nNumWorldWeapons = 0;

		// want to track any world models that are active weapons
		// the world weapons lying on the ground are the ones that become LRU purge candidates
		CBasePlayer *pPlayer = ToBasePlayer( this );
		if ( pPlayer )
		{
			CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
			if ( pWeapon )
			{
				worldWeapons[nNumWorldWeapons] = pWeapon->GetWorldModel();
				nNumWorldWeapons++;
			}
		}

		CCSPlayer *pCSPlayer = CCSPlayer::GetLocalCSPlayer();
		if ( pCSPlayer == this )
		{
			int weaponEntIndex = pCSPlayer->GetTargetedWeapon();
			if ( weaponEntIndex > 0 ) //0 is a valid entity index, but will never be used for a weapon
			{			
				CBaseEntity *pEnt = cl_entitylist->GetEnt( weaponEntIndex );
				CBaseCombatWeapon *pWeapon = dynamic_cast< CBaseCombatWeapon * >( pEnt );
				if ( pWeapon )
				{
					worldWeapons[nNumWorldWeapons] = pWeapon->GetWorldModel();
					nNumWorldWeapons++;
				}
			}
		}

		if ( nNumWorldWeapons )
		{
			modelinfo->TouchWorldWeaponModelCache( worldWeapons, nNumWorldWeapons );
		}
	}
}

bool CBaseCombatCharacter::HasEverBeenInjured( void ) const
{
	return ( m_flTimeOfLastInjury != 0.0f );
}

float CBaseCombatCharacter::GetTimeSinceLastInjury( void ) const
{
	return gpGlobals->curtime - m_flTimeOfLastInjury;
}

IMPLEMENT_CLIENTCLASS(CBaseCombatCharacter, DT_BaseCombatCharacter, CBaseCombatCharacter);

// Only send active weapon index to local player
IMPLEMENT_REFLECT_TABLE_IN( CBaseCombatCharacter, DT_BCCLocalPlayerExclusive );

// The server's DT_BCCNonLocalPlayerExclusive is empty -- no member claims it -- so the
// m_hMyWeapons entry this used to carry under CSTRIKE15 could never be resolved: the decoder
// walks the props the server actually transmits. The array is in the primary table on both sides.
IMPLEMENT_REFLECT_TABLE_IN( CBaseCombatCharacter, DT_BCCNonLocalPlayerExclusive );

IMPLEMENT_REFLECT_TABLE( CBaseCombatCharacter, DT_BaseCombatCharacter );

IMPLEMENT_REFLECT_PREDMAP( CBaseCombatCharacter );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::WeaponCount() const
{
	return MAX_WEAPONS;
}

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CEconItemView;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Switches to the best weapon that is also better than the given weapon.
// Input  : pCurrent - The current weapon used by the player.
// Output : Returns true if the weapon was switched, false if there was no better
//			weapon to switch to.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent)
{
	CBaseCombatWeapon *pNewWeapon = g_pGameRules->GetNextBestWeapon(this, pCurrent);
	
	if ( ( pNewWeapon != nullptr ) && ( pNewWeapon != pCurrent ) )
	{
		return Weapon_Switch( pNewWeapon );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	if ( pWeapon == nullptr )
		return false;

	// Already have it out?
	if ( m_hActiveWeapon.Get() == pWeapon )
	{
		if ( !m_hActiveWeapon->IsWeaponVisible() || m_hActiveWeapon->IsHolstered() )
			return m_hActiveWeapon->Deploy( );
		return false;
	}

	if (!Weapon_CanSwitchTo(pWeapon))
	{
		return false;
	}

	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->IsAlwaysActive() )
		{
			if ( !m_hActiveWeapon->Holster( pWeapon ) )
			{
				return false;
			}
		}
	}

	m_hActiveWeapon = pWeapon;
	return pWeapon->Deploy( );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if (IsPlayer())
	{
		CBasePlayer *pPlayer = (CBasePlayer *)this;
		
	}

	if ( !pWeapon->HasAnyAmmo() && !GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->CanHolster() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetActiveWeapon() const
{
	return m_hActiveWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetWeapon( int i ) const
{
	contract_assert( (i >= 0) && (i < MAX_WEAPONS) );
	return m_hMyWeapons[i].Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iCount - 
//			iAmmoIndex - 
//-----------------------------------------------------------------------------
extern ConVar sv_infinite_ammo;
void CBaseCombatCharacter::RemoveAmmo( int iCount, int iAmmoIndex )
{
	if (iCount <= 0)
		return;

	if ( iAmmoIndex < 0 )
		return;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return;

	if ( sv_infinite_ammo.GetInt() == 2 ) // infinite total ammo but magazine reloads are still required.
		return;

	// Ammo pickup sound
	m_iAmmo.Set( iAmmoIndex, std::max( m_iAmmo[iAmmoIndex] - iCount, 0 ) );
}

void CBaseCombatCharacter::RemoveAmmo( int iCount, const char *szName )
{
	RemoveAmmo( iCount, GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAllAmmo( )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_iAmmo.Set( i, 0 );
	}
}

//-----------------------------------------------------------------------------
// FIXME: This is a sort of hack back-door only used by physgun!
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetAmmoCount( int iCount, int iAmmoIndex )
{
	// NOTE: No sound, no max check! Seems pretty bogus to me!
	m_iAmmo.Set( iAmmoIndex, iCount );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of a particular type owned
//			owned by the character
// Input  :	Ammo Index
// Output :	The amount of ammo
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetAmmoCount( int iAmmoIndex ) const
{

	// DEPRECATED - Ido says please use GetReserveAmmoCount

	if ( iAmmoIndex == -1 )
		return 0;

	// Infinite ammo?
	if ( GetAmmoDef()->CanCarryInfiniteAmmo( iAmmoIndex ) )
		return 999;

	return m_iAmmo[ iAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CBaseCombatWeapon* CBaseCombatCharacter::Weapon_OwnsThisType( const char *pszWeapon, int iSubType ) const
{
	for ( int i = 0; i < MAX_WEAPONS; i++ ) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs( m_hMyWeapons[i], pszWeapon ) )
		{
			// Make sure it matches the subtype
			if ( m_hMyWeapons[i]->GetSubType() == iSubType )
			{
				return m_hMyWeapons[i];
			}
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested slot
// Input  : slot - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetSlot( int slot ) const
{
	int	targetSlot = slot;

	// Check for that slot being occupied already
	for ( int i=0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[i].Get() != nullptr )
		{
			// If the slots match, it's already occupied
			if ( m_hMyWeapons[i]->GetSlot() == targetSlot )
				return m_hMyWeapons[i];
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested loadout position
// Input  : position - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetPosition( int position ) const
{
	// Check for that slot being occupied already
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[ i ].Get() != nullptr )
		{
			CEconItemView * pItem = m_hMyWeapons[ i ]->GetEconItemView() ;
			if ( !pItem )
				continue;

			loadout_positions_t unItemPos = ( loadout_positions_t )pItem->GetItemDefinition()->GetLoadoutSlot( GetTeamNumber() );

			// If the slots match, it's already occupied
			if ( unItemPos == position )
				return m_hMyWeapons[ i ];
		}
	}

	return nullptr;
}

int CBaseCombatCharacter::BloodColor()
{
	return m_bloodColor;
}

//-----------------------------------------------------------------------------
// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetBloodColor( int nBloodColor )
{
	m_bloodColor = nBloodColor;
}

//-----------------------------------------------------------------------------
/**
The main visibility check.  Checks all the entity specific reasons that could 
make IsVisible fail.  Then checks points in space to get environmental reasons.
This is LOS, plus invisibility and fog and smoke and such.
*/

enum VisCacheResult_t
{
	VISCACHE_UNKNOWN = 0,
	VISCACHE_IS_VISIBLE,
	VISCACHE_IS_NOT_VISIBLE,
};

enum
{
	VIS_CACHE_MAX_ENTRIES = 0xFFFF
};

static constexpr auto VIS_CACHE_ENTRY_LIFE = .090f;

class CCombatCharVisCache : public CAutoGameSystemPerFrame
{
public:
	virtual void FrameUpdatePreEntityThink();
	virtual void LevelShutdownPreEntity();

private:
	struct VisCacheKey_t
	{
		CHandle< CBaseCombatCharacter >	m_hEntity1;
		CHandle< CBaseCombatCharacter >	m_hEntity2;
	};

	struct CVisCacheKeyLess
	{
		bool operator()( const VisCacheKey_t &lhs, const VisCacheKey_t &rhs ) const
		{
			return ( std::memcmp( &lhs, &rhs, sizeof( VisCacheKey_t ) ) < 0 );
		}
	};

	struct VisCacheEntry_t
	{
		float							m_flTime;
		bool							m_bEntity1CanSeeEntity2;
		bool							m_bEntity2CanSeeEntity1;
	};

	using VisCacheMap_t = std::map< VisCacheKey_t, VisCacheEntry_t, CVisCacheKeyLess >;

public:
	// An entry, and whether the caller's pair is stored the other way round.
	struct VisCacheRef_t
	{
		VisCacheMap_t::iterator	m_it;
		bool					m_bValid;
		bool					m_bReverse;
	};

	VisCacheRef_t LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 );
	VisCacheResult_t HasVisibility( const VisCacheRef_t &ref ) const;
	void RegisterVisibility( const VisCacheRef_t &ref, bool bChar1SeesChar2, bool bChar2SeesChar1 );

private:
	VisCacheMap_t m_VisCache;

	mutable int m_nTestCount;
	mutable int m_nHitCount;
};

void CCombatCharVisCache::FrameUpdatePreEntityThink()
{
	//	Msg( "test: %d/%d\n", m_nHitCount, m_nTestCount );

	// Lazy retirement of vis cache
	// NOTE: 256 was chosen heuristically based on a playthrough where 200
	// was the max # in the viscache where nothing could be retired.
	if ( m_VisCache.size() < 256 )
		return;

	for ( int i = 0; i < 8; ++i )
	{
		auto it = std::next( m_VisCache.begin(), RandomInt( 0, (int)m_VisCache.size() - 1 ) );
		const VisCacheKey_t &key = it->first;
		if ( !key.m_hEntity1.IsValid() || !key.m_hEntity2.IsValid() || ( gpGlobals->curtime - it->second.m_flTime > 10.0f ) )
		{
			m_VisCache.erase( it );
		}
	}
}

void CCombatCharVisCache::LevelShutdownPreEntity()
{
	m_VisCache.clear();
}

CCombatCharVisCache::VisCacheRef_t CCombatCharVisCache::LookupVisibility( const CBaseCombatCharacter *pChar1, CBaseCombatCharacter *pChar2 )
{
	VisCacheKey_t cacheKey;
	if ( pChar1 < pChar2 )
	{
		cacheKey.m_hEntity1 = pChar1;
		cacheKey.m_hEntity2 = pChar2;
	}
	else
	{
		cacheKey.m_hEntity1 = pChar2;
		cacheKey.m_hEntity2 = pChar1;
	}

	auto it = m_VisCache.find( cacheKey );
	if ( it == m_VisCache.end() )
	{
		if ( m_VisCache.size() >= VIS_CACHE_MAX_ENTRIES )
			return { m_VisCache.end(), false, false };

		it = m_VisCache.emplace( cacheKey, VisCacheEntry_t{} ).first;
		it->second.m_flTime = gpGlobals->curtime - 2.0f * VIS_CACHE_ENTRY_LIFE;
	}

	return { it, true, !( pChar1 < pChar2 ) };
}

VisCacheResult_t CCombatCharVisCache::HasVisibility( const VisCacheRef_t &ref ) const
{
	if ( !ref.m_bValid )
		return VISCACHE_UNKNOWN;

	m_nTestCount++;

	bool bReverse = ref.m_bReverse;
	const VisCacheEntry_t &entry = ref.m_it->second;
	if ( gpGlobals->curtime - entry.m_flTime > VIS_CACHE_ENTRY_LIFE )
		return VISCACHE_UNKNOWN;

	m_nHitCount++;

	bool bIsVisible = !bReverse ? entry.m_bEntity1CanSeeEntity2 : entry.m_bEntity2CanSeeEntity1;
	return bIsVisible ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
}

void CCombatCharVisCache::RegisterVisibility( const VisCacheRef_t &ref, bool bEntity1CanSeeEntity2, bool bEntity2CanSeeEntity1 )
{
	if ( !ref.m_bValid )
		return;

	bool bReverse = ref.m_bReverse;
	VisCacheEntry_t &entry = ref.m_it->second;
	entry.m_flTime = gpGlobals->curtime;
	if ( !bReverse )
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity1CanSeeEntity2;
		entry.m_bEntity2CanSeeEntity1 = bEntity2CanSeeEntity1;
	}
	else
	{
		entry.m_bEntity1CanSeeEntity2 = bEntity2CanSeeEntity1;
		entry.m_bEntity2CanSeeEntity1 = bEntity1CanSeeEntity2;
	}
}

static CCombatCharVisCache s_CombatCharVisCache;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsAbleToSee( const CBaseEntity *pEntity, FieldOfViewCheckType checkFOV )
{
	CBaseCombatCharacter *pBCC = const_cast<CBaseEntity *>( pEntity )->MyCombatCharacterPointer();
	if ( pBCC )
		return IsAbleToSee( pBCC, checkFOV );

	// Test this every time; it's cheap.
	Vector vecEyePosition = EyePosition();
	Vector vecTargetPosition = pEntity->WorldSpaceCenter();

	if ( !ComputeLOS( vecEyePosition, vecTargetPosition ) )
		return false;

	return ( checkFOV != USE_FOV || IsInFieldOfView( vecTargetPosition ) );
}

static void ComputeSeeTestPosition( Vector *pEyePosition, CBaseCombatCharacter *pBCC )
{
	{
		*pEyePosition = pBCC->EyePosition();
	}	
}

bool CBaseCombatCharacter::IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV )
{
	Vector vecEyePosition, vecOtherEyePosition;
	ComputeSeeTestPosition( &vecEyePosition, this );
	ComputeSeeTestPosition( &vecOtherEyePosition, pBCC );

	// Check if we have a cached-off visibility
	CCombatCharVisCache::VisCacheRef_t cacheRef = s_CombatCharVisCache.LookupVisibility( this, pBCC );
	VisCacheResult_t nResult = s_CombatCharVisCache.HasVisibility( cacheRef );

	// Compute symmetric visibility
	if ( nResult == VISCACHE_UNKNOWN )
	{
		bool bThisCanSeeOther = false, bOtherCanSeeThis = false;
		if ( ComputeLOS( vecEyePosition, vecOtherEyePosition ) )
		{
			bThisCanSeeOther = true, bOtherCanSeeThis = true;
		}

		s_CombatCharVisCache.RegisterVisibility( cacheRef, bThisCanSeeOther, bOtherCanSeeThis );
		nResult = bThisCanSeeOther ? VISCACHE_IS_VISIBLE : VISCACHE_IS_NOT_VISIBLE;
	}

	if ( nResult == VISCACHE_IS_VISIBLE )
		return ( checkFOV != USE_FOV || IsInFieldOfView( pBCC ) );

	return false;
}

class CTraceFilterNoCombatCharacters : public CTraceFilterSimple
{
public:
	CTraceFilterNoCombatCharacters( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
		{
			CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
			if ( !pEntity )
				return false;

			if ( pEntity->MyCombatCharacterPointer() || pEntity->MyCombatWeaponPointer() )
				return false;

			// Honor BlockLOS - this lets us see through partially-broken doors, etc
			if ( !pEntity->BlocksLOS() )
				return false;

			return true;
		}

		return false;
	}
};

bool CBaseCombatCharacter::ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const
{
	// We simply can't see because the world is in the way.
	trace_t result;
	CTraceFilterNoCombatCharacters traceFilter( nullptr, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecEyePosition, vecTarget, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &result );
	return ( result.fraction == 1.0f );
}

//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const CBaseEntity *target, float cosTolerance ) const
{
	return IsLookingTowards( target->WorldSpaceCenter(), cosTolerance ) || IsLookingTowards( target->EyePosition(), cosTolerance ) || IsLookingTowards( target->GetAbsOrigin(), cosTolerance );
}

//-----------------------------------------------------------------------------
/**
Return true if our view direction is pointing at the given target, 
within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
*/
bool CBaseCombatCharacter::IsLookingTowards( const Vector &target, float cosTolerance ) const
{
	Vector toTarget = target - EyePosition();
	toTarget.NormalizeInPlace();

	Vector forward;
	AngleVectors( EyeAngles(), &forward );

	return ( DotProduct( forward, toTarget ) >= cosTolerance );
}

//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( CBaseEntity *entity ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );
	float flTolerance = pPlayer ? std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) : BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE;

	Vector vecForward;
	Vector vecEyePosition = EyePosition();
	AngleVectors( EyeAngles(), &vecForward );

	// FIXME: Use a faster check than this!

	// Check 3 spots, or else when standing right next to someone looking at their eyes, 
	// the angle will be too great to see their center.
	Vector vecToTarget = entity->GetAbsOrigin() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->WorldSpaceCenter() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	if ( DotProduct( vecForward, vecToTarget ) >= flTolerance )
		return true;

	vecToTarget = entity->EyePosition() - vecEyePosition;
	vecToTarget.NormalizeInPlace();
	return ( DotProduct( vecForward, vecToTarget ) >= flTolerance );
}

//-----------------------------------------------------------------------------
/**
Returns true if we are looking towards something within a tolerence determined 
by our field of view
*/
bool CBaseCombatCharacter::IsInFieldOfView( const Vector &pos ) const
{
	CBasePlayer *pPlayer = ToBasePlayer( const_cast< CBaseCombatCharacter* >( this ) );

	if ( pPlayer )
		return IsLookingTowards( pos, std::cos( DEG2RAD( pPlayer->GetFOV() * 0.5f ) ) );

	return IsLookingTowards( pos );
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/

bool CBaseCombatCharacter::IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType ) const
{
	if ( entity->MyCombatCharacterPointer() )
		return IsLineOfSightClear( entity->EyePosition(), checkType, entity );
	return IsLineOfSightClear( entity->WorldSpaceCenter(), checkType, entity );
}

//-----------------------------------------------------------------------------
/**
Strictly checks Line of Sight only.
*/
static bool TraceFilterNoCombatCharacters( IHandleEntity *pServerEntity, int contentsMask )
{
	// Honor BlockLOS also to allow seeing through partially-broken doors
	CBaseEntity *entity = EntityFromEntityHandle( pServerEntity );
	return ( entity->MyCombatCharacterPointer() == nullptr && !entity->MyCombatWeaponPointer() && entity->BlocksLOS() );
}

bool CBaseCombatCharacter::IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType, CBaseEntity *entityToIgnore ) const
{
	if( checkType == IGNORE_ACTORS )
	{

		// use the query cache unless it causes problems
		trace_t trace;
		CTraceFilterNoCombatCharacters traceFilter( entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE | CONTENTS_MONSTER, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
	else
	{
		trace_t trace;
		CTraceFilterSkipTwoEntities traceFilter( this, entityToIgnore, COLLISION_GROUP_NONE );
		UTIL_TraceLine( EyePosition(), pos, MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE, &traceFilter, &trace );

		return trace.fraction == 1.0f;
	}
}
