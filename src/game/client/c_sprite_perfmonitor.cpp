//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


extern bool g_bMeasureParticlePerformance;
extern bool g_bDisplayParticlePerformance;

void ResetParticlePerformanceCounters( void );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ParticlePerformanceMonitor" } ]]
      C_ParticlePerformanceMonitor : public C_BaseEntity
{
	DECLARE_CLASS( C_ParticlePerformanceMonitor, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_ParticlePerformanceMonitor();
	~C_ParticlePerformanceMonitor();
	virtual void	OnDataChanged( DataUpdateType_t updateType );

private:
	[[= ks::reflect::Net{} ]] bool m_bDisplayPerf;
	[[= ks::reflect::Net{} ]] bool m_bMeasurePerf;
private:
	C_ParticlePerformanceMonitor( const C_ParticlePerformanceMonitor & );
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_ParticlePerformanceMonitor, DT_ParticlePerformanceMonitor, CParticlePerformanceMonitor )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ParticlePerformanceMonitor::C_ParticlePerformanceMonitor( void )
{
	m_bDisplayPerf = false;
	m_bMeasurePerf = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ParticlePerformanceMonitor::~C_ParticlePerformanceMonitor( void )
{
	g_bMeasureParticlePerformance = false;
	g_bDisplayParticlePerformance = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ParticlePerformanceMonitor::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	if ( m_bMeasurePerf && ( ! g_bMeasureParticlePerformance ) )
		ResetParticlePerformanceCounters();
	g_bMeasureParticlePerformance = m_bMeasurePerf;
	g_bDisplayParticlePerformance = m_bDisplayPerf;
}

