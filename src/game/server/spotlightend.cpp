// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Dynamic light at the end of a spotlight

#include "vphysics_interface.h"
#include "const.h"
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
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "spotlightend.h"

LINK_ENTITY_TO_CLASS(spotlight_end, CSpotlightEnd);

IMPLEMENT_REFLECT_SERVERCLASS( CSpotlightEnd, DT_SpotlightEnd )


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CSpotlightEnd::Spawn_Impl( void )
{
	Precache();
	m_flLightScale  = 100;
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}
