// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An entity that spawns and controls a particle system

#include "string_t.h"
#include "vphysics_interface.h"
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
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "particles/particles.h"
#include "particle_system.h"

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );


// Stripped down CBaseEntity send table
IMPLEMENT_REFLECT_SERVERCLASS( CParticleSystem, DT_ParticleSystem )

IMPLEMENT_REFLECT_DATAMAP( CParticleSystem )

LINK_ENTITY_TO_CLASS( info_particle_system, CParticleSystem );

CParticleSystem::CParticleSystem( void ) : m_bNoSave( false )
{
	for( int i = 0; i != kSERVERCONTROLLEDPOINTS; ++i )
	{
		m_iServerControlPointAssignments.GetForModify(i) = 255;
	}
}

//-----------------------------------------------------------------------------
// Precache 
//-----------------------------------------------------------------------------
void CParticleSystem::Precache_Impl( void )
{
	const char *pParticleSystemName = STRING( m_iszEffectName );
	if ( pParticleSystemName == nullptr || pParticleSystemName[0] == 0 )
	{
		Warning( "info_particle_system (%s) has no particle system name specified!\n", GetEntityName().ToCStr() );
	}

	PrecacheParticleSystem( pParticleSystemName );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	Precache();
	m_iEffectIndex = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// Find our particle effect index
	m_iEffectIndex = GetParticleSystemIndex( STRING(m_iszEffectName) );

	if ( m_bStartActive )
	{
		m_bStartActive = false;
		StartParticleSystem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CParticleSystem::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "snapshot_file" ) )
	{
		V_strncpy( m_szSnapshotFileName.GetForModify(), szValue, MAX_PATH );
		return true;
	}
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CParticleSystem::GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "snapshot_file" ) )
	{
		V_snprintf( szValue, iMaxLen, "%s", m_szSnapshotFileName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue_Impl( szKeyName, szValue, iMaxLen );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StartParticleSystemThink( void )
{
	StartParticleSystem();
}

//-----------------------------------------------------------------------------
// Purpose: Always transmitted to clients
//-----------------------------------------------------------------------------
int CParticleSystem::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StartParticleSystem( void )
{
	if ( m_bActive == false )
	{
		m_flStartTime = gpGlobals->curtime;
		m_bActive = true;
		
		// Setup our control points at this time (in case our targets weren't around at spawn time)
		ReadControlPointEnts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::StopParticleSystem( int nStopType )
{
	m_bActive = false;
	m_nStopType = nStopType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::InputStart( inputdata_t &inputdata )
{
	StartParticleSystem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystem::InputStop( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_NORMAL );
}

void CParticleSystem::InputDestroy( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_DESTROY_IMMEDIATELY );
}

void CParticleSystem::InputStopEndCap( inputdata_t &inputdata )
{
	StopParticleSystem( STOP_PLAY_ENDCAP );
}

//-----------------------------------------------------------------------------
// Purpose: Find each entity referred to by m_iszControlPointNames and 
// resolve it into the corresponding slot in m_hControlPointEnts
//-----------------------------------------------------------------------------
void CParticleSystem::ReadControlPointEnts( void )
{
	for ( int i = 0 ; i < kMAXCONTROLPOINTS; ++i )
	{
		if ( m_iszControlPointNames[i] == NULL_STRING )
			continue;

		CBaseEntity *pPointEnt = gEntList.FindEntityGeneric( nullptr, STRING( m_iszControlPointNames[i] ), this );
		contract_assert( pPointEnt != nullptr );
		if ( pPointEnt == nullptr )
		{
			Warning("Particle system %s could not find control point entity (%s)\n", GetEntityName().ToCStr(), m_iszControlPointNames[i].ToCStr() );
			continue;
		}

		m_hControlPointEnts.Set( i, pPointEnt );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Try to allocate one of the server controlled control points to 
// hold the value. Designed to let the server funnel some variables to
// particle systems (size, color, swirliness, ...)
//-----------------------------------------------------------------------------
bool CParticleSystem::SetControlPointValue( int iControlPoint, const Vector &vValue )
{
	for( int i = 0; i != kSERVERCONTROLLEDPOINTS; ++i )
	{
		if( m_iServerControlPointAssignments[i] == iControlPoint )
		{
			m_vServerControlPoints.GetForModify(i) = vValue;
			return true;
		}
		if( m_iServerControlPointAssignments[i] == 255 )
		{
			m_iServerControlPointAssignments.GetForModify(i) = iControlPoint;
			m_vServerControlPoints.GetForModify(i) = vValue;
			return true;
		}
	}

	Warning( "No free server controlled control points.\n" );
	return false; //already using up all of our server control points
}

//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
int CParticleSystem::ObjectCaps_Impl( void )
{ 
	int flags = 0;
	if ( m_bNoSave )
		flags = FCAP_DONT_SAVE;
	
	return BaseClass::ObjectCaps_Impl() | flags; 
}
