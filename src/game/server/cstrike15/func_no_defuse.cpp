// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2011 Valve Corporation

// Purpose: No defuse area

#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
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
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "cs_player.h"
#include "func_no_defuse.h"

LINK_ENTITY_TO_CLASS( func_no_defuse, CNoDefuseArea );


void CNoDefuseArea::Spawn_Impl()
{
	InitTrigger();
	SetTouch( &CNoDefuseArea::NoDefuseAreaTouch );
}

void CNoDefuseArea::NoDefuseAreaTouch( CBaseEntity* pOther )
{
	CCSPlayer *player = dynamic_cast< CCSPlayer* >( pOther );
	// CT players are not allowed to defuse a bomb when they are in a no defuse zone.
	// the is to help prevent bugs with CTs being able to defuse bombs through walls and floors etc.
	if ( player )
	{
		player->m_bInNoDefuseArea = true;
	}
}
