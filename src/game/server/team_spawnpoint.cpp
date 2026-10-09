// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Team spawnpoint handling

import std;
import ks.math;
#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "shareddefs.h"
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
#include "reflect_datamap.h"
#include "player.h"
#include "gamerules.h"
#include "team_spawnpoint.h"
#include "team.h"

LINK_ENTITY_TO_CLASS( info_player_teamspawn, CTeamSpawnPoint );

IMPLEMENT_REFLECT_DATAMAP( CTeamSpawnPoint )


//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::Activate( void )
{
	BaseClass::Activate();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_player_teamspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a player to spawn in?
//-----------------------------------------------------------------------------
bool CTeamSpawnPoint::IsValid( CBasePlayer *pPlayer )
{
	CBaseEntity *ent = nullptr;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() && plent != pPlayer )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputEnable( inputdata_t &inputdata )
{
	m_iDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = true;
}


//===========================================================================================================
// VEHICLE SPAWNPOINTS
//===========================================================================================================
LINK_ENTITY_TO_CLASS( info_vehicle_groundspawn, CTeamVehicleSpawnPoint );

IMPLEMENT_REFLECT_DATAMAP( CTeamVehicleSpawnPoint )


//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a vehicle to spawn in?
//-----------------------------------------------------------------------------
bool CTeamVehicleSpawnPoint::IsValid( void )
{
	CBaseEntity *ent = nullptr;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamVehicleSpawnPoint::Activate( void )
{
	BaseClass::Activate();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		// Don't add vehicle spawnpoints to the team for now
		//GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_vehicle_groundspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}
