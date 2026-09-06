//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Team spawnpoint entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_TEAMSPAWNPOINT_H
#define TF_TEAMSPAWNPOINT_H

#include "reflect_annotations.h"
#pragma once

#include "baseentity.h"
#include "entityoutput.h"

class CTeam;

//-----------------------------------------------------------------------------
// Purpose: points at which the player can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS( CTeamSpawnPoint, CPointEntity );

	void	Activate( void );
	virtual bool	IsValid( CBasePlayer *pPlayer );

	[[= ks::reflect::Key{ .name = "OnPlayerSpawn" } ]] COutputEvent m_OnPlayerSpawn;

protected:	
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int		m_iDisabled;

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: points at which vehicles can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamVehicleSpawnPoint : public CTeamSpawnPoint
{
	DECLARE_CLASS( CTeamVehicleSpawnPoint, CTeamSpawnPoint );
public:
	void	Activate( void );
	bool	IsValid( void );

	[[= ks::reflect::Key{ .name = "OnVehicleSpawn" } ]] COutputEvent m_OnVehicleSpawn;

	DECLARE_DATADESC();
};


#endif // TF_TEAMSPAWNPOINT_H
