// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An entity that can be used to constrain the player's movement around it

import std;
import ks.math;
#include "edict.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

constexpr auto SF_TELEPORT_TO_SPAWN_POS = 0x00000001;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointPlayerMoveConstraint : public CBaseEntity
{
	DECLARE_CLASS( CPointPlayerMoveConstraint, CBaseEntity );
public:
	DECLARE_DATADESC();

	int		UpdateTransmitState( void );
	void	Activate( void );
	void	ConstraintThink( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "radius" } ]] float				m_flRadius;	
	[[= ks::reflect::Key{ .name = "width" } ]] float				m_flConstraintWidth;
	[[= ks::reflect::Key{ .name = "speedfactor" } ]] float				m_flSpeedFactor;
	float				m_flRadiusSquared;	
	std::vector< EHANDLE >	m_hConstrainedPlayers;
	[[= ks::reflect::Key{ .name = "OnConstraintBroken" } ]] COutputEvent		m_OnConstraintBroken;
};

LINK_ENTITY_TO_CLASS( point_playermoveconstraint, CPointPlayerMoveConstraint );

IMPLEMENT_REFLECT_DATAMAP( CPointPlayerMoveConstraint )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointPlayerMoveConstraint::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::Activate( void )
{
	BaseClass::Activate();

	m_flRadiusSquared = (m_flRadius * m_flRadius);
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::InputTurnOn( inputdata_t &inputdata )
{
	// Find all players within our radius and constraint them
	float flRadius = m_flRadius;
	// If we're in singleplayer, blow the radius a bunch
	if ( gpGlobals->maxClients == 1 )
	{
		flRadius = MAX_COORD_RANGE;
	}
	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = gEntList.FindEntityByClassnameWithin( pEntity, "player", GetLocalOrigin(), flRadius)) != nullptr )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEntity );
		contract_assert( pPlayer );
		
		// Only add him if he's not already constrained
		if ( ks::find_index( m_hConstrainedPlayers, pPlayer ) == -1 )
		{
			m_hConstrainedPlayers.push_back( pPlayer );

			pPlayer->ActivateMovementConstraint( this, GetAbsOrigin(), m_flRadius, m_flConstraintWidth, m_flSpeedFactor );
		}
	}

	// Only think if we found any
	if ( !m_hConstrainedPlayers.empty() )
	{
		SetThink( &CPointPlayerMoveConstraint::ConstraintThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//------------------------------------------------------------------------------
// Purpose: Release all players we've constrained
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::InputTurnOff( inputdata_t &inputdata )
{
	int iCount = std::ssize( m_hConstrainedPlayers );
	for ( int i = 0; i < iCount; i++ )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hConstrainedPlayers[i] );
		if ( pPlayer )
		{
			pPlayer->DeactivateMovementConstraint();
		}
	}

	m_hConstrainedPlayers.clear(); m_hConstrainedPlayers.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if any of our constrained players have broken the constraint
//-----------------------------------------------------------------------------
void CPointPlayerMoveConstraint::ConstraintThink( void )
{
	int iCount = std::ssize( m_hConstrainedPlayers );

	// Count backwards, because we might drop them if they've broken the constraint
	for ( int i = (iCount-1); i >= 0; i-- )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hConstrainedPlayers[i] );
		if ( pPlayer )
		{
			float flDistanceSqr = (pPlayer->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
			if ( flDistanceSqr > m_flRadiusSquared )
			{
				// Break the constraint to this player
				pPlayer->DeactivateMovementConstraint();
				m_hConstrainedPlayers.erase( m_hConstrainedPlayers.begin() + i );

				// Fire the broken output
				m_OnConstraintBroken.FireOutput( this, pPlayer );
			}
		}
	}

	// Only keep thinking if we any left
	if ( !m_hConstrainedPlayers.empty() )
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}
