// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Shadow control entity.

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

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Water LOD control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_WaterLODControl", .base = false } ]]
      CWaterLODControl : public CBaseEntity
{
public:
	DECLARE_CLASS( CWaterLODControl, CBaseEntity );

	CWaterLODControl();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState();
	void SetCheapWaterStartDistance( inputdata_t &inputdata );
	void SetCheapWaterEndDistance( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVar( float, m_flCheapWaterStartDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "cheapwaterstartdistance" } ]] [[= ks::reflect::Key{ .name = "SetCheapWaterStartDistance", .input = true } ]] );
	CNetworkVar( float, m_flCheapWaterEndDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "cheapwaterenddistance" } ]] [[= ks::reflect::Key{ .name = "SetCheapWaterEndDistance", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(water_lod_control, CWaterLODControl);

IMPLEMENT_REFLECT_DATAMAP( CWaterLODControl )


IMPLEMENT_REFLECT_SERVERCLASS( CWaterLODControl, DT_WaterLODControl )


CWaterLODControl::CWaterLODControl()
{
	m_flCheapWaterStartDistance = 1000.0f;
	m_flCheapWaterEndDistance = 2000.0f;
}


//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CWaterLODControl::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CWaterLODControl::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "cheapwaterstartdistance" ) )
	{
		m_flCheapWaterStartDistance = std::atof( szValue );
		return true;
	}

	if ( FStrEq( szKeyName, "cheapwaterenddistance" ) )
	{
		m_flCheapWaterEndDistance = std::atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CWaterLODControl::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CWaterLODControl::SetCheapWaterStartDistance( inputdata_t &inputdata )
{
	m_flCheapWaterStartDistance = std::atof( inputdata.value.String() );
}

void CWaterLODControl::SetCheapWaterEndDistance( inputdata_t &inputdata )
{
	m_flCheapWaterEndDistance = std::atof( inputdata.value.String() );
}
