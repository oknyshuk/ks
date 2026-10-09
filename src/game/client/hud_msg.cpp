// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//
//  hud_msg.cpp
//
#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "c_recipientfilter.h"
#include "clientmode.h"
#include "hudelement.h"
#include "engine/IEngineSound.h"

/// USER-DEFINED SERVER MESSAGE HANDLERS

bool CHud::MsgFunc_ResetHUD( const ks::net::CCSUsrMsg_ResetHud& msg )
{
	ResetHUD();
	return true;
}

void CHud::ResetHUD()
{
	// clear all hud data
	GetClientMode()->GetViewportAnimationController()->CancelAllAnimations();

	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[i]->Reset();
	}

	GetClientMode()->GetViewportAnimationController()->RunAllAnimationsToCompletion();

	// reset sensitivity
	m_flMouseSensitivity = 0;
	m_flMouseSensitivityFactor = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CHud::MsgFunc_SendAudio(const ks::net::CCSUsrMsg_SendAudio& msg )
{
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, msg.radio_sound->c_str() );
	return true;
}
