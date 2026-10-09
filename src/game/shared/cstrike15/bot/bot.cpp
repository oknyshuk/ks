// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Author: Michael S. Booth (mike@turtlerockstudios.com), Leon Hartwig, 2003

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
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
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "cs_shareddefs.h"

#include "basegrenade_shared.h"

#include "bot.h"
#include "bot_util.h"

/// @todo Remove this nasty hack - CreateFakeClient() calls CBot::Spawn, which needs the profile and team
const BotProfile *g_botInitProfile = nullptr;
int g_botInitTeam = 0;

//
// NOTE: Because CBot had to be templatized, the code was moved into bot.h
//


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------

ActiveGrenade::ActiveGrenade( CBaseGrenade *grenadeEntity )
{
	m_entity = grenadeEntity;
	m_detonationPosition = grenadeEntity->GetAbsOrigin();
	m_dieTimestamp = 0.0f;

	m_isSmoke = false;
	m_isFlashbang = false;
	m_isMolotov = false;
	m_isDecoy = false;

	switch ( grenadeEntity->GetGrenadeType() )
	{
	case GRENADE_TYPE_EXPLOSIVE:	m_radius = HEGrenadeRadius;									break;
	case GRENADE_TYPE_FLASH:		m_radius = FlashbangGrenadeRadius; m_isFlashbang = true;	break;
	case GRENADE_TYPE_FIRE:			m_radius = MolotovGrenadeRadius; m_isMolotov = true;		break;
	case GRENADE_TYPE_DECOY:		m_radius = DecoyGrenadeRadius; m_isDecoy = true;			break;
	case GRENADE_TYPE_SMOKE:		m_radius = SmokeGrenadeRadius; m_isSmoke = true;			break;
	case GRENADE_TYPE_SENSOR:		m_radius = MolotovGrenadeRadius; m_isSensor = true;			break;
	default:
		contract_assert( false && "Invalid grenade type!\n" );
		m_radius = HEGrenadeRadius;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Called when the grenade in the world goes away
 */
void ActiveGrenade::OnEntityGone( void )
{
	if (m_isSmoke)
	{
		// smoke lingers after grenade is gone
		const float smokeLingerTime = 4.0f;
		m_dieTimestamp = gpGlobals->curtime + smokeLingerTime;
	}

	m_entity = nullptr;
}

//--------------------------------------------------------------------------------------------------------------
void ActiveGrenade::Update( void )
{
	if (m_entity != nullptr)
	{
		m_detonationPosition = m_entity->GetAbsOrigin();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this grenade is valid
 */
bool ActiveGrenade::IsValid( void ) const
{
	if ( m_isSmoke )
	{
		if ( m_entity == nullptr && gpGlobals->curtime > m_dieTimestamp )
		{
			return false;
		}
	}
	else
	{
		if ( m_entity == nullptr )
		{
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
const Vector &ActiveGrenade::GetPosition( void ) const
{
	// smoke grenades can vanish before the smoke itself does - refer to the detonation position
	if (m_entity == nullptr)
		return GetDetonationPosition();

	return m_entity->GetAbsOrigin();
}
