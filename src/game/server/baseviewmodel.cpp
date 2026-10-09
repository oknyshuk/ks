// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "baseviewmodel.h"
#include "player.h"
#include "studio.h"
#include "hltvdirector.h"
#include "replaydirector.h"

void SendProxy_AnimTime( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_SequenceChanged( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: Save Data for Base Weapon object
//-----------------------------------------------------------------------------// 

int CBaseViewModel::UpdateTransmitState()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

int CBaseViewModel::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Check if recipient owns this weapon viewmodel
	CBasePlayer *pOwner = ToBasePlayer( m_hOwner );

	if ( pOwner && 
		 ( pOwner->edict() == pInfo->m_pClientEnt ||
			// If we're using the other guys network connection in split screen, then also force transmit
		 pOwner->IsSplitScreenUserOnEdict( pInfo->m_pClientEnt ) ) )
	{
		return FL_EDICT_ALWAYS;
	}

	// check if recipient (or one of his splitscreen parasites) is spectating the owner of this viewmodel
	CBasePlayer *pPlayer = ToBasePlayer( CBaseEntity::Instance( pInfo->m_pClientEnt ) );
	if ( pPlayer)
	{
		// Bug 28591:  In splitscreen, when the second slot is the one in spectator mode, it wouldn't
		//  get the viewmodel for the spectatee.
		//
		// The new logic is to loop through the splitscreen parasites (as well as the host player) 
		//  and see if any of them are observing the viewmodel owner, and if so, FL_EDICT_ALWAYS the vm for them, too.

		// This container was the source of most of the allocation cost in CS:GO so using a std::inplace_vector to avoid
		// allocations is important.
		ks::small_vector< CBasePlayer *, MAX_SPLITSCREEN_CLIENTS > checkList;
		checkList.push_back( pPlayer );
		std::vector< CHandle< CBasePlayer > > &vecParasites = pPlayer->GetSplitScreenPlayers();
		for ( int i = 0; i < std::ssize( vecParasites ); ++i )
		{
			checkList.push_back( vecParasites[ i ] );
		}

		for ( int i = 0; i < std::ssize( checkList ); ++i )
		{
			CBasePlayer *pPlayer = checkList[ i ];
			if ( !pPlayer )
				continue;

			if ( pPlayer->IsHLTV() || pPlayer->IsReplay() )
			{
				// if this is the HLTV or Replay client, transmit all viewmodels in our PVS
				return FL_EDICT_PVSCHECK;
			}
			if ( (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)  && (pPlayer->GetObserverTarget() == pOwner) )
			{
				return FL_EDICT_ALWAYS;
			}
		}
	}

	// Don't send to anyone else except the local player or his spectator
	return FL_EDICT_DONTSEND;
}

void CBaseViewModel::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );
}
