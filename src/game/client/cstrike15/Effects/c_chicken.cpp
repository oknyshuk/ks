// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

// Purpose: Client side interactive, shootable chicken

#include "rocketui/rmlui.h"
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
#include "c_chicken.h"
#include "c_breakableprop.h"
#include "c_cs_player.h"
#include "reflect_recvtable.h"

#undef CChicken

IMPLEMENT_REFLECT_CLIENTCLASS( C_CChicken, DT_CChicken, CChicken )


//-----------------------------------------------------------------------------------------------
C_CChicken::C_CChicken()
{
}


//-----------------------------------------------------------------------------------------------
C_CChicken::~C_CChicken()
{
	SetClientSideHolidayHatAddon( false );
}


//-----------------------------------------------------------------------------------------------
void C_CChicken::Spawn( void )
{
	BaseClass::Spawn();

	SetClientSideHolidayHatAddon( false );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_CChicken::RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
// 	C_CHostage *pHostage = ( C_CHostage * ) pStruct;
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
void C_CChicken::ClientThink()
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
void C_CChicken::SetClientSideHolidayHatAddon( bool bEnable )
{
	/*
	if ( bEnable && !m_hHolidayHatAddon.Get() )
	{
		// Create the hat
		C_BreakableProp *pEnt = new C_BreakableProp;
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
