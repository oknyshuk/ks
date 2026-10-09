// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: area portal entity: toggles visibility areas on/off
// NOTE: These are not really brush entities.  They are brush entities from a
// designer/worldcraft perspective, but by the time they reach the game, the
// brush model is gone and this is, in effect, a point entity.

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

class [[= ks::reflect::NetTable{ .name = "DT_FuncOccluder", .base = false } ]]
      CFuncOccluder : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncOccluder, CBaseEntity );

					CFuncOccluder();

	virtual void	Spawn( void );
	virtual int		UpdateTransmitState( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputActivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

private:
	CNetworkVar( bool, m_bActive, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "StartActive" } ]] );
	CNetworkVar( int, m_nOccluderIndex, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "occludernumber" } ]] );
};

LINK_ENTITY_TO_CLASS( func_occluder, CFuncOccluder );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncOccluder, DT_FuncOccluder )


IMPLEMENT_REFLECT_DATAMAP( CFuncOccluder )


//------------------------------------------------------------------------------
// Occluder :
//------------------------------------------------------------------------------
CFuncOccluder::CFuncOccluder()
{
	m_bActive = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncOccluder::Spawn( void )
{
    Precache( );    

	m_takedamage	= DAMAGE_NO;
	SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );
	
	// set size and link into world.
	SetModel( STRING( GetModelName() ) );
}


//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputDeactivate( inputdata_t &inputdata )
{
	m_bActive = false;
}


//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputActivate( inputdata_t &inputdata )
{
	m_bActive = true;
}


//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;
}


//------------------------------------------------------------------------------
// We always want to transmit these bad boys
//------------------------------------------------------------------------------
int CFuncOccluder::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}
