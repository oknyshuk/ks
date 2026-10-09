// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

// Purpose: an entity which turns on and off counting and display of the particle
// performance metric

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
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "entityoutput.h"

//-----------------------------------------------------------------------------
// Purpose: Entity that particle performance measuring
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ParticlePerformanceMonitor" } ]]
      CParticlePerformanceMonitor : public CPointEntity
{
	DECLARE_CLASS( CParticlePerformanceMonitor, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn( void );
	int		UpdateTransmitState( void );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOnDisplay", .type = FIELD_VOID } ]] void	InputTurnOnDisplay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOffDisplay", .type = FIELD_VOID } ]] void	InputTurnOffDisplay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartMeasuring", .type = FIELD_VOID } ]] void	InputStartMeasuring( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopMeasuring", .type = FIELD_VOID } ]] void	InputStopMeasuring( inputdata_t &inputdata );

private:
	CNetworkVar( bool, m_bDisplayPerf, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bMeasurePerf, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

LINK_ENTITY_TO_CLASS( env_particle_performance_monitor, CParticlePerformanceMonitor );

IMPLEMENT_REFLECT_DATAMAP( CParticlePerformanceMonitor )

IMPLEMENT_REFLECT_SERVERCLASS( CParticlePerformanceMonitor, DT_ParticlePerformanceMonitor )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticlePerformanceMonitor::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	m_bDisplayPerf = false;
	m_bMeasurePerf = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CParticlePerformanceMonitor::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CParticlePerformanceMonitor::InputTurnOnDisplay( inputdata_t &inputdata )
{
	m_bDisplayPerf = true;
}

void CParticlePerformanceMonitor::InputTurnOffDisplay( inputdata_t &inputdata )
{
	m_bDisplayPerf = false;
}

void CParticlePerformanceMonitor::InputStartMeasuring( inputdata_t &inputdata )
{
	m_bMeasurePerf = true;
}

void CParticlePerformanceMonitor::InputStopMeasuring( inputdata_t &inputdata )
{
	m_bMeasurePerf = false;
}
