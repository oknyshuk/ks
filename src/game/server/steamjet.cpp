// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements the server side of a steam jet particle system entity.

import std;
import ks.math;
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
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
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "steamjet.h"

//Networking
IMPLEMENT_REFLECT_SERVERCLASS( CSteamJet, DT_SteamJet )

LINK_ENTITY_TO_CLASS( env_steam, CSteamJet );
LINK_ENTITY_TO_CLASS( env_steamjet, CSteamJet ); // For support of legacy env_steamjet, which faced left instead of forward.

//Save/restore
IMPLEMENT_REFLECT_DATAMAP( CSteamJet )


CSteamJet::CSteamJet( void )
{
	m_flRollSpeed = 8.0f;
}
//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CSteamJet::Spawn( void )
{
	Precache();

	//
	// Legacy env_steamjet pointed left instead of forward.
	//
	if ( FClassnameIs( this, "env_steamjet" ))
	{
		m_bFaceLeft = true;
	}

	if ( m_InitialState )
	{
		m_bEmit = true;
	}
}

void CSteamJet::Precache( void )
{
	PrecacheMaterial( "particle/particle_smokegrenade" );
	PrecacheMaterial( "sprites/heatwave" );
}

 void CSteamJet::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
 {
	if (!pActivator->IsPlayer())
	{
		if (useType == USE_ON)
		{
			m_bEmit = true;
		}
		else if (useType == USE_OFF)
		{
			m_bEmit = false;
		}
	}
 }


//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the steam jet on/off.
//-----------------------------------------------------------------------------
void CSteamJet::InputToggle(inputdata_t &data)
{
	m_bEmit = !m_bEmit;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the steam jet.
//-----------------------------------------------------------------------------
void CSteamJet::InputTurnOn(inputdata_t &data)
{
	m_bEmit = true;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the steam jet.
//-----------------------------------------------------------------------------
void CSteamJet::InputTurnOff(inputdata_t &data)
{
	m_bEmit = false;
}
