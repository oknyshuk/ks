// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "string_t.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "playerlocaldata.h"
#include "player.h"
#include "SkyCamera.h"
#include "playernet_vars.h"
#include "fogcontroller.h"

//=============================================================================

IMPLEMENT_REFLECT_TABLE( CPlayerLocalData, DT_Local );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( fogplayerparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( fogparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( sky3dparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( audioparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CPlayerLocalData )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CPlayerLocalData::CPlayerLocalData()
{
	std::memset( m_chAreaBits.m_Value, 0, sizeof( m_chAreaBits.m_Value ) );
	memset( m_chAreaPortalBits.m_Value, 0, sizeof( m_chAreaPortalBits.m_Value ) );

	m_iHideHUD = 0;
	m_flFOVRate = 0.0f;

	m_vecOverViewpoint.Zero();

	m_bDucked = false;
	m_bDucking = false;
	m_flLastDuckTime = -1.0f;
	m_bInDuckJump = false;
	m_nDuckTimeMsecs = 0; // REI: LEGACY, should be removed
	m_nDuckJumpTimeMsecs = 0; // REI: LEGACY, should be removed
	m_nJumpTimeMsecs = 0;
	m_nStepside = 0;
	m_flFallVelocity = 0.0f;
	m_nOldButtons = 0;
	m_pOldSkyCamera = nullptr;

	// $$$REI What's the safe way to initialize these things in constructor?
	m_viewPunchAngle.m_Value.Init();
	m_aimPunchAngle.m_Value.Init();
	m_aimPunchAngleVel.m_Value.Init();

	m_bDrawViewmodel = true;
	m_bWearingSuit = false;
	m_bPoisoned = false;
	m_flStepSize = 0.0f;
	m_bAllowAutoMovement = true;

	m_bAutoAimTarget = false;

	// m_skybox3d?
	// m_PlayerFog?
	// m_fog?

	// other fields on m_audio?  should this have a constructor too?
	m_audio.soundscapeIndex = 0;
	m_audio.localBits = 0;
	m_audio.entIndex = 0;

	m_bSlowMovement = false;
	m_fTBeamEndTime = 0.0f;
}

static Vector GetPlayerViewPosition( CBasePlayer *pl )
{
	CBaseEntity *pView = pl->GetViewEntity();
	if ( !pView )
	{
		pView = pl;
	}
	return pView->EyePosition();
}

void CPlayerLocalData::UpdateAreaBits( CBasePlayer *pl, unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] )
{
	Vector origin = GetPlayerViewPosition(pl);

	unsigned char tempBits[32] = { 0 };

	static_assert( ( sizeof( tempBits ) % 4 ) == 0 );

	int nDWords = sizeof( tempBits ) >> 2;

	static_assert( sizeof( tempBits ) >= sizeof( ((CPlayerLocalData*)0)->m_chAreaBits ) );

	int i;
	int area = engine->GetArea( origin );
	engine->GetAreaBits( area, tempBits, sizeof( tempBits ) );

	std::vector< int > vecAreasSeen;
	vecAreasSeen.push_back( area );

	std::vector< CHandle< CBasePlayer > > &list = pl->GetSplitScreenAndPictureInPicturePlayers();
	for ( i = 0; i < std::ssize( list ); ++i )
	{
		CBasePlayer *pSplit = list[ i ];
		if ( !pSplit )
			continue;

		unsigned char tempBits2[32] = { 0 };
		Vector org2 = GetPlayerViewPosition(pSplit);
		int area2 = engine->GetArea( org2 );

		// Already merged this area in?
		if ( ks::find_index( vecAreasSeen, area2 ) != -1 )
			continue;

		vecAreasSeen.push_back( area2 );
		engine->GetAreaBits( area2, tempBits2, sizeof( tempBits2 ) );

		unsigned int *pBase = (unsigned int *)tempBits;
		unsigned int *pAdd = (unsigned int *)tempBits2;

		// Now merge them together
		for ( int j = 0; j < nDWords; ++j )
		{
			*pBase++ |= *pAdd++;
		}
	}

	for ( i=0; i < m_chAreaBits.Count(); i++ )
	{
		if ( tempBits[i] != m_chAreaBits[ i ] )
		{
			m_chAreaBits.Set( i, tempBits[i] );
		}
	}

	for ( i=0; i < MAX_AREA_PORTAL_STATE_BYTES; i++ )
	{
		if ( chAreaPortalBits[i] != m_chAreaPortalBits[i] )
		{
			m_chAreaPortalBits.Set( i, chAreaPortalBits[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fills in CClientData values for local player just before sending over wire
// Input  : player - 
//-----------------------------------------------------------------------------

void ClientData_Update( CBasePlayer *pl )
{
	// HACKHACK: for 3d skybox 
	// UNDONE: Support multiple sky cameras?
	CSkyCamera *pSkyCamera = GetCurrentSkyCamera();
	if ( pSkyCamera != pl->m_Local.m_pOldSkyCamera )
	{
		pl->m_Local.m_pOldSkyCamera = pSkyCamera;
		pl->m_Local.m_skybox3d.CopyFrom(pSkyCamera->m_skyboxData);
	}
	else if ( !pSkyCamera )
	{
		pl->m_Local.m_skybox3d.area = 255;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UpdateAllClientData( void )
{
	CVProfScope _( "UpdateAllClientData", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	int i;
	CBasePlayer *pl;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pl = ( CBasePlayer * )UTIL_PlayerByIndex( i );
		if ( !pl )
			continue;

		ClientData_Update( pl );
	}
}
