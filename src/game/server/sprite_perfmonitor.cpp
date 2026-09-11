//====== Copyright (c) 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: an entity which turns on and off counting and display of the particle
// performance metric
//
//=============================================================================

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "convar.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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

