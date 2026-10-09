// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "string_t.h"
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
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
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
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //

class [[= ks::reflect::NetTable{ .name = "DT_InfoOverlayAccessor", .base = false } ]]
      [[= ks::reflect::From<"m_iTextureFrameIndex", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CInfoOverlayAccessor : public CPointEntity
{
public:

	DECLARE_CLASS( CInfoOverlayAccessor, CPointEntity );

	int  	UpdateTransmitState();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:

	CNetworkVar( int, m_iOverlayID, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "OverlayID" } ]] );
};
							  

// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CInfoOverlayAccessor, DT_InfoOverlayAccessor )

LINK_ENTITY_TO_CLASS( info_overlay_accessor, CInfoOverlayAccessor );

IMPLEMENT_REFLECT_DATAMAP( CInfoOverlayAccessor )


int CInfoOverlayAccessor::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}
