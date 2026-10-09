#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Entity that propagates general data needed by clients for every player.

#include "reflect_annotations.h"

#include "shareddefs.h"
#include "baseentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_PlayerResource", .base = false } ]]
      CPlayerResource : public CBaseEntity
{
	DECLARE_CLASS( CPlayerResource, CBaseEntity );
public:
	DECLARE_SERVERCLASS();

	virtual void Spawn( void );
	virtual	int	 ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	virtual void ResourceThink( void );
	virtual void UpdatePlayerData( void );
	virtual int  UpdateTransmitState(void);
	int GetPlayerSmoothPing( int iClientIndex );

protected:
	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	CNetworkArray( int, m_iPing, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iKills, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkArray( int, m_iAssists, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkArray( int, m_iDeaths, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 12 } ]] );
	CNetworkArray( int, m_bConnected, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iTeam, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 4 } ]] );
	CNetworkArray( int, m_iPendingTeam, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 4 } ]] );
	CNetworkArray( int, m_bAlive, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iHealth, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 10 } ]] );

	CNetworkArray( int, m_iCoachingTeam, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 4 } ]] );
		
	int	m_nUpdateCounter;
};

extern CPlayerResource *g_pPlayerResource;
