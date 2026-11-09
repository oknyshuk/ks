// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Game-specific impact effect hooks

#include "rocketui/rmlui.h"
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "c_recipientfilter.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#include "fx_impact.h"
#include "tempent.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"

//-----------------------------------------------------------------------------
// Purpose: Handle weapon effect callbacks
//-----------------------------------------------------------------------------
void CStrike_EjectBrass( int shell, const CEffectData &data )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if( !pPlayer )
		return;

	DevWarning( "Unhandled CStrike EjectBrass effect\n" );
}

void CStrike_FX_EjectBrass_9mm_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_9MM, data );
}

void CStrike_FX_EjectBrass_57_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_57, data );
}

void CStrike_FX_EjectBrass_12Gauge_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_12GAUGE, data );
}

void CStrike_FX_EjectBrass_556_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_556, data );
}

void CStrike_FX_EjectBrass_762Nato_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_762NATO, data );
}

void CStrike_FX_EjectBrass_338Mag_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_338MAG, data );
}

DECLARE_CLIENT_EFFECT( EjectBrass_9mm,		CStrike_FX_EjectBrass_9mm_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_12Gauge,	CStrike_FX_EjectBrass_12Gauge_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_57,		CStrike_FX_EjectBrass_57_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_556,		CStrike_FX_EjectBrass_556_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_762Nato,	CStrike_FX_EjectBrass_762Nato_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_338Mag,	CStrike_FX_EjectBrass_338Mag_Callback );
