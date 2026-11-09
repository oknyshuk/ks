// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Team management class. Contains all the details for a specific team

#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "reflect_sendtable.h"
#include "cs_team.h"

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CCSTeam, DT_CSTeam )

LINK_ENTITY_TO_CLASS( cs_team_manager, CCSTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified TF team manager
//-----------------------------------------------------------------------------
CCSTeam *GetGlobalCSTeam( int iIndex )
{
	return (CCSTeam*)GetGlobalTeam( iIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
void CCSTeam::Init( const char *pName, int iNumber )
{
	BaseClass::Init( pName, iNumber );

	// Only detect changes every half-second.
	NetworkProp()->SetUpdateInterval( 0.75f );
}

CCSTeam::CCSTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam::~CCSTeam( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTeam::Precache_Impl( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CCSTeam::Think_Impl( void )
{
	BaseClass::Think_Impl();
}


void CCSTeam::OnRoundPreStart( void )
{
}

//------------------------------------------------------------------------------------------------------------------
// PLAYERS
//-----------------------------------------------------------------------------
// Purpose: Add the specified player to this team. Remove them from their current team, if any.
//-----------------------------------------------------------------------------
void CCSTeam::AddPlayer( CBasePlayer *pPlayer )
{
	BaseClass::AddPlayer( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the player's objects when they leave
//-----------------------------------------------------------------------------
void CCSTeam::RemovePlayer( CBasePlayer *pPlayer )
{
	BaseClass::RemovePlayer( pPlayer );
}

//------------------------------------------------------------------------------------------------------------------
// UTILITY FUNCS
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSTeam* CCSTeam::GetEnemyTeam()
{
	int iMyTeam = GetTeamNumber();

	if ( iMyTeam == TEAM_CT )
		return GetGlobalCSTeam( TEAM_TERRORIST );
	else if ( iMyTeam == TEAM_TERRORIST )
		return GetGlobalCSTeam( TEAM_CT );
	else
		return nullptr;
}
