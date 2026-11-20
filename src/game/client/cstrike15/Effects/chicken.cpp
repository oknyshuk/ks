// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

module;
#include "client_class.h"
#include "../../reflect_recvtable.h"
module ks.client;

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :baseentity;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.Effects.chicken;
import :cstrike15.cs_player;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import :particlemgr;

using namespace ::ks::reflect;

// Purpose: Client side interactive, shootable chicken

#undef CCChicken

IMPLEMENT_REFLECT_CLIENTCLASS( CCChicken, DT_CChicken, CCChicken )

//-----------------------------------------------------------------------------------------------
CCChicken::CCChicken()
{
}

//-----------------------------------------------------------------------------------------------
CCChicken::~CCChicken()
{
	SetClientSideHolidayHatAddon( false );
}

//-----------------------------------------------------------------------------------------------
void CCChicken::Spawn( void )
{
	BaseClass::Spawn();

	SetClientSideHolidayHatAddon( false );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCChicken::RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
// 	CCHostage *pHostage = ( CCHostage * ) pStruct;
// 
// 	bool jumped = pData->m_Value.m_Int != 0;
// 
// 	if ( jumped )
// 	{
// 		// hostage jumped
// 		pHostage->m_PlayerAnimState->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
// 		pHostage->SetNextClientThink( gpGlobals->curtime );
// 	}
// 
// 	pHostage->m_jumpedThisFrame = jumped;
}

//-----------------------------------------------------------------------------------------------
void CCChicken::ClientThink()
{
	BaseClass::ClientThink();

	Activity currentActivity = GetSequenceActivity( GetSequence() );
	if ( m_lastActivity != currentActivity )
	{
		if ( currentActivity == ACT_CLIMB_UP )
			SetCycle( 0 );
		m_lastActivity = currentActivity;
	}

	bool bEnableHolidayHat = false;
	
	SetClientSideHolidayHatAddon( bEnableHolidayHat );
}

//-----------------------------------------------------------------------------
void CCChicken::SetClientSideHolidayHatAddon( bool bEnable )
{
	/*
	if ( bEnable && !m_hHolidayHatAddon.Get() )
	{
		// Create the hat
		CBreakableProp *pEnt = new CBreakableProp;
		pEnt->InitializeAsClientEntity( "models/antlers/antlers.mdl", false );
		pEnt->FollowEntity( this );
		pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
		pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
		pEnt->SetUseParentLightingOrigin( true );// This will make it so the weapons get lit with the same ambient cube that the player gets lit with.
		m_hHolidayHatAddon.Set( pEnt );
	}
	if ( !bEnable && m_hHolidayHatAddon.Get() )
	{
		// Remove the hat
		m_hHolidayHatAddon->Release();
		m_hHolidayHatAddon.Term();
	}
	*/
}
