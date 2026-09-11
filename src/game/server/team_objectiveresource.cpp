//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: An entity that networks the state of the game's objectives.
//			May contain data for objectives that aren't used by your mod, but
//			the extra data will never be networked as long as it's zeroed out.
//
//=============================================================================
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "team_objectiveresource.h"
#include "shareddefs.h"
#include <coordsize.h>
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CAPHUD_PARITY_BITS		6
#define CAPHUD_PARITY_MASK		((1<<CAPHUD_PARITY_BITS)-1)

#define LAZY_UPDATE_TIME		3

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CBaseTeamObjectiveResource, DT_BaseTeamObjectiveResource )


CBaseTeamObjectiveResource *g_pObjectiveResource = nullptr;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTeamObjectiveResource::CBaseTeamObjectiveResource()
{
	g_pObjectiveResource = this;
	m_bPlayingMiniRounds = false;
	m_iUpdateCapHudParity = 0;
	m_bControlPointsReset = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTeamObjectiveResource::~CBaseTeamObjectiveResource()
{
	Assert( g_pObjectiveResource == this );
	g_pObjectiveResource = nullptr;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::Spawn( void )
{
	m_iNumControlPoints = 0;

	// If you hit this, you've got too many teams for the control point system to handle.
	Assert( GetNumberOfTeams() < MAX_CONTROL_POINT_TEAMS );

	for ( int i=0; i < MAX_CONTROL_POINTS; i++ )
	{
		// data variables
		m_vCPPositions.Set( i, vec3_origin );
		m_bCPIsVisible.Set( i, true );
		m_bBlocked.Set( i, false );

		// state variables
		m_iOwner.Set( i, TEAM_UNASSIGNED );
		m_iCappingTeam.Set( i, TEAM_UNASSIGNED );
		m_iTeamInZone.Set( i, TEAM_UNASSIGNED );
		m_bInMiniRound.Set( i, true );
		m_iWarnOnCap.Set( i, CP_WARN_NORMAL );
		m_flLazyCapPerc.Set( i, 0.0 );

		for ( int team = 0; team < MAX_CONTROL_POINT_TEAMS; team++ )
		{
			int iTeamIndex = TEAM_ARRAY( i, team );

			m_iTeamIcons.Set( iTeamIndex, 0 );
			m_iTeamOverlays.Set( iTeamIndex, 0 );
			m_iTeamReqCappers.Set( iTeamIndex, 0 );
			m_flTeamCapTime.Set( iTeamIndex, 0.0f );
			m_iNumTeamMembers.Set( TEAM_ARRAY( i, team ), 0 );
			for ( int ipoint = 0; ipoint < MAX_PREVIOUS_POINTS; ipoint++ )
			{
				int iIntIndex = ipoint + (i * MAX_PREVIOUS_POINTS) + (team * MAX_CONTROL_POINTS * MAX_PREVIOUS_POINTS);
				m_iPreviousPoints.Set( iIntIndex, -1 );
			}
			m_bTeamCanCap.Set( iTeamIndex, false );
		}
	}

	for ( int i = 0; i < MAX_TEAMS; i++ )
	{
		m_iBaseControlPoints.Set( i, -1 );
	}

	SetThink( &CBaseTeamObjectiveResource::ObjectiveThink );
	SetNextThink( gpGlobals->curtime + LAZY_UPDATE_TIME );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::ObjectiveThink( void )
{
	SetNextThink( gpGlobals->curtime + LAZY_UPDATE_TIME );

	for ( int i = 0; i < m_iNumControlPoints; i++ )
	{
		if ( m_iCappingTeam[i] )
		{
			m_flLazyCapPerc.Set( i, m_flCapPercentages[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The objective resource is always transmitted to clients
//-----------------------------------------------------------------------------
int CBaseTeamObjectiveResource::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Round is starting, reset state
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::ResetControlPoints( void )
{
	for ( int i=0; i < MAX_CONTROL_POINTS; i++ )
	{
		m_iCappingTeam.Set( i, TEAM_UNASSIGNED );
		m_iTeamInZone.Set( i, TEAM_UNASSIGNED );
		m_bInMiniRound.Set( i, true );

		for ( int team = 0; team < MAX_CONTROL_POINT_TEAMS; team++ )
		{
			m_iNumTeamMembers.Set( TEAM_ARRAY( i, team ), 0.0f );
		}
	}

	UpdateCapHudElement();
	m_bControlPointsReset = !m_bControlPointsReset;
}

//-----------------------------------------------------------------------------
// Purpose: Data setting functions
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetNumControlPoints( int num )
{
	Assert( num <= MAX_CONTROL_POINTS );
	m_iNumControlPoints = num;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPIcons( int index, int iTeam, int iIcon )
{
	AssertValidIndex(index);
	m_iTeamIcons.Set( TEAM_ARRAY( index, iTeam ), iIcon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPOverlays( int index, int iTeam, int iIcon )
{
	AssertValidIndex(index);
	m_iTeamOverlays.Set( TEAM_ARRAY( index, iTeam ), iIcon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetTeamBaseIcons( int iTeam, int iBaseIcon )
{
	Assert( iTeam >= 0 && iTeam < MAX_TEAMS );
	m_iTeamBaseIcons.Set( iTeam, iBaseIcon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPPosition( int index, const Vector& vPosition )
{
	AssertValidIndex(index);
	m_vCPPositions.Set( index, vPosition );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPVisible( int index, bool bVisible )
{
	AssertValidIndex(index);
	m_bCPIsVisible.Set( index, bVisible );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetWarnOnCap( int index, int iWarnLevel )
{
	AssertValidIndex(index);
	m_iWarnOnCap.Set( index, iWarnLevel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetWarnSound( int index, string_t iszSound )
{
	AssertValidIndex(index);
	m_iszWarnSound.Set( index, iszSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPRequiredCappers( int index, int iTeam, int iReqPlayers )
{
	AssertValidIndex(index);
	m_iTeamReqCappers.Set( TEAM_ARRAY( index, iTeam ), iReqPlayers );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPCapTime( int index, int iTeam, float flTime )
{
	AssertValidIndex(index);
	m_flTeamCapTime.Set( TEAM_ARRAY( index, iTeam ), flTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCPCapPercentage( int index, float flTime )
{
	AssertValidIndex(index);
	m_flCapPercentages[index] = flTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBaseTeamObjectiveResource::GetCPCapPercentage( int index )
{
	AssertValidIndex(index);
	return m_flCapPercentages[index];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetTeamCanCap( int index, int iTeam, bool bCanCap )
{
	AssertValidIndex(index);
	m_bTeamCanCap.Set( TEAM_ARRAY( index, iTeam ), bCanCap );
	UpdateCapHudElement();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetBaseCP( int index, int iTeam )
{
	Assert( iTeam < MAX_TEAMS );
	m_iBaseControlPoints.Set( iTeam, index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetPreviousPoint( int index, int iTeam, int iPrevIndex, int iPrevPoint )
{
	AssertValidIndex(index);
	Assert( iPrevIndex >= 0 && iPrevIndex < MAX_PREVIOUS_POINTS );
	int iIntIndex = iPrevIndex + (index * MAX_PREVIOUS_POINTS) + (iTeam * MAX_CONTROL_POINTS * MAX_PREVIOUS_POINTS);
	m_iPreviousPoints.Set( iIntIndex, iPrevPoint );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTeamObjectiveResource::GetPreviousPointForPoint( int index, int team, int iPrevIndex )
{
	AssertValidIndex(index);
	Assert( iPrevIndex >= 0 && iPrevIndex < MAX_PREVIOUS_POINTS );
	int iIntIndex = iPrevIndex + (index * MAX_PREVIOUS_POINTS) + (team * MAX_CONTROL_POINTS * MAX_PREVIOUS_POINTS);
	return m_iPreviousPoints[ iIntIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTeamObjectiveResource::TeamCanCapPoint( int index, int team )
{
	AssertValidIndex(index);
	return m_bTeamCanCap[ TEAM_ARRAY( index, team ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Data setting functions
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetNumPlayers( int index, int team, int iNumPlayers )
{
	AssertValidIndex(index);
	m_iNumTeamMembers.Set( TEAM_ARRAY( index, team ), iNumPlayers );
	UpdateCapHudElement();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::StartCap( int index, int team )
{
	AssertValidIndex(index);
	if ( m_iCappingTeam.Get( index ) != team )
	{
		m_iCappingTeam.Set( index, team );
		UpdateCapHudElement();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetOwningTeam( int index, int team )
{
	AssertValidIndex(index);
	m_iOwner.Set( index, team );

	// clear the capper
	m_iCappingTeam.Set( index, TEAM_UNASSIGNED );
	UpdateCapHudElement();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCappingTeam( int index, int team )
{
	AssertValidIndex(index);
	if ( m_iCappingTeam.Get( index ) != team )
	{
		m_iCappingTeam.Set( index, team );
		UpdateCapHudElement();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetTeamInZone( int index, int team )
{
	AssertValidIndex(index);
	if ( m_iTeamInZone.Get( index ) != team )
	{
		m_iTeamInZone.Set( index, team );
		UpdateCapHudElement();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetCapBlocked( int index, bool bBlocked )
{
	AssertValidIndex(index);
	if ( m_bBlocked.Get( index ) != bBlocked )
	{
		m_bBlocked.Set( index, bBlocked );
		UpdateCapHudElement();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTeamObjectiveResource::GetOwningTeam( int index )
{
	if ( index >= m_iNumControlPoints )
		return TEAM_UNASSIGNED;

	return m_iOwner[index];
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::UpdateCapHudElement( void )
{
	m_iUpdateCapHudParity = (m_iUpdateCapHudParity + 1) & CAPHUD_PARITY_MASK;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTeamObjectiveResource::SetTrainPathDistance( int index, float flDistance )
{
	AssertValidIndex(index);

	m_flPathDistance.Set( index, flDistance );
}
