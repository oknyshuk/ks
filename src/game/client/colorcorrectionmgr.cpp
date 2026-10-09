// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Singleton manager for color correction on the client

#include "rocketui/rmlui.h"
import ks.tier1;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "colorcorrectionmgr.h"
#include "clientmode.h"

//------------------------------------------------------------------------------
// Singleton access
//------------------------------------------------------------------------------
static CColorCorrectionMgr s_ColorCorrectionMgr;
CColorCorrectionMgr *g_pColorCorrectionMgr = &s_ColorCorrectionMgr;

static ConVar mat_colcorrection_editor( "mat_colcorrection_editor", "0" );

static std::vector< C_ColorCorrection * > g_ColorCorrectionList;
static std::vector< C_ColorCorrectionVolume * > g_ColorCorrectionVolumeList;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
CColorCorrectionMgr::CColorCorrectionMgr()
{
	m_nActiveWeightCount = 0;
}

//------------------------------------------------------------------------------
// Creates, destroys color corrections
//------------------------------------------------------------------------------
ClientCCHandle_t CColorCorrectionMgr::AddColorCorrection( const char *pName, const char *pFileName )
{
	if ( !pFileName )
	{
		pFileName = pName;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ColorCorrectionHandle_t ccHandle = pRenderContext->AddLookup( pName );
	if ( ccHandle )
	{
		pRenderContext->LockLookup( ccHandle );
		pRenderContext->LoadLookup( ccHandle, pFileName );
		pRenderContext->UnlockLookup( ccHandle );
	}
	else
	{
		Warning("Cannot find color correction lookup file: '%s'\n", pFileName );
	}

	return (ClientCCHandle_t)ccHandle;
}

void CColorCorrectionMgr::RemoveColorCorrection( ClientCCHandle_t h )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->RemoveLookup( ccHandle );
	}
}

ClientCCHandle_t CColorCorrectionMgr::FindColorCorrection( const char *pName )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	ColorCorrectionHandle_t ccHandle = pRenderContext->FindLookup( pName );
	
	return (ClientCCHandle_t)ccHandle;
}

ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionEntity( C_ColorCorrection *pEntity, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		contract_assert( ks::find_index( g_ColorCorrectionList, pEntity ) == -1 );
		g_ColorCorrectionList.push_back( pEntity );
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionEntity( C_ColorCorrection *pEntity, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	ks::swap_erase_first( g_ColorCorrectionList, pEntity );
}

ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionVolume( C_ColorCorrectionVolume *pVolume, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		contract_assert( ks::find_index( g_ColorCorrectionVolumeList, pVolume ) == -1 );
		g_ColorCorrectionVolumeList.push_back( pVolume );
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionVolume( C_ColorCorrectionVolume *pVolume, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	ks::swap_erase_first( g_ColorCorrectionVolumeList, pVolume );
}

//------------------------------------------------------------------------------
// Modify color correction weights
//------------------------------------------------------------------------------
void CColorCorrectionMgr::SetColorCorrectionWeight( ClientCCHandle_t h, float flWeight, bool bExclusive )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		SetWeightParams_t params = { h, flWeight, bExclusive };
		m_colorCorrectionWeights.push_back( params );
		if( bExclusive && m_bHaveExclusiveWeight && ( flWeight != 0.0f ) )
		{
			DevWarning( "Found multiple active color_correction entities with exclusive setting enabled. This is invalid.\n" );
		}
		if ( bExclusive )
		{
			m_bHaveExclusiveWeight = true;
			m_flExclusiveWeight = flWeight;
		}
	}
}

void CColorCorrectionMgr::CommitColorCorrectionWeights()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	for ( int i = 0; i < std::ssize( m_colorCorrectionWeights ); i++ )
	{
		ColorCorrectionHandle_t ccHandle = reinterpret_cast<ColorCorrectionHandle_t>( m_colorCorrectionWeights[i].handle );
		float flWeight = m_colorCorrectionWeights[i].flWeight;
		if ( !m_colorCorrectionWeights[i].bExclusive )
		{
			flWeight = (1.0f - m_flExclusiveWeight ) * m_colorCorrectionWeights[i].flWeight;
		}
		pRenderContext->SetLookupWeight( ccHandle, flWeight );

		// FIXME: NOTE! This doesn't work if the same handle has
		// its weight set twice with no intervening calls to ResetColorCorrectionWeights
		// which, at the moment, is true
		if ( flWeight != 0.0f )
		{
			++m_nActiveWeightCount;
		}
	}
	m_colorCorrectionWeights.clear();
}

void CColorCorrectionMgr::ResetColorCorrectionWeights()
{
	CVProfScope _( "ResetColorCorrectionWeights", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// FIXME: Where should I put this? It needs to happen prior to SimulateEntities()
	// which is where the client thinks for c_colorcorrection + c_colorcorrectionvolumes
	// update the color correction weights.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ResetLookupWeights();
	m_nActiveWeightCount = 0;
	m_bHaveExclusiveWeight = false;
	m_flExclusiveWeight = 0.0f;
	m_colorCorrectionWeights.clear();
}

void CColorCorrectionMgr::SetResetable( ClientCCHandle_t h, bool bResetable )
{
	// NOTE: Setting stuff to be not resettable doesn't work when in queued mode
	// because the logic that sets m_nActiveWeightCount to 0 in ResetColorCorrectionWeights
	// is no longer valid when stuff is not resettable.
	contract_assert( bResetable || !g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED );
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->SetResetable( ccHandle, bResetable );
	}
}


//------------------------------------------------------------------------------
// Is color correction active?
//------------------------------------------------------------------------------
bool CColorCorrectionMgr::HasNonZeroColorCorrectionWeights() const
{
	return ( m_nActiveWeightCount != 0 ) || mat_colcorrection_editor.GetBool();
}

void CColorCorrectionMgr::UpdateColorCorrection()
{
	ResetColorCorrectionWeights();
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	IClientMode *pClientMode = GetClientMode();

	contract_assert( pClientMode );
	if ( !pPlayer || !pClientMode )
	{
		return;
	}

	pClientMode->OnColorCorrectionWeightsReset();
	float ccScale = pClientMode->GetColorCorrectionScale();

	UpdateColorCorrectionEntities( pPlayer, ccScale, g_ColorCorrectionList.data(), std::ssize( g_ColorCorrectionList ) );
	UpdateColorCorrectionVolumes( pPlayer, ccScale, g_ColorCorrectionVolumeList.data(), std::ssize( g_ColorCorrectionVolumeList ) );
	CommitColorCorrectionWeights();
}
