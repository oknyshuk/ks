// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
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
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "env_player_surface_trigger.h"

LINK_ENTITY_TO_CLASS( env_player_surface_trigger, CEnvPlayerSurfaceTrigger );

IMPLEMENT_REFLECT_DATAMAP( CEnvPlayerSurfaceTrigger )

// Global list of surface triggers
std::vector< CHandle<CEnvPlayerSurfaceTrigger> >	g_PlayerSurfaceTriggers;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvPlayerSurfaceTrigger::~CEnvPlayerSurfaceTrigger( void )
{
	ks::erase_first( g_PlayerSurfaceTriggers, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	m_iCurrentGameMaterial = 0;
	m_bDisabled = false;

	g_PlayerSurfaceTriggers.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::OnRestore( void )
{
	BaseClass::OnRestore();

	g_PlayerSurfaceTriggers.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial )
{
	// Ignore players in the air (stops bunny hoppers escaping triggers)
	if ( gameMaterial == 0 )
		return;

	// Loop through the surface triggers and tell them all about the change
	int iCount = std::ssize( g_PlayerSurfaceTriggers );
	for ( int i = 0; i < iCount; i++ )
	{
		g_PlayerSurfaceTriggers[i]->PlayerSurfaceChanged( pPlayer, gameMaterial );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial )
{
	if ( m_bDisabled )
		return;

	// Fire the output if we've changed, but only if it involves the target material
	if ( gameMaterial != (char)m_iCurrentGameMaterial &&
	     ( gameMaterial == m_iTargetGameMaterial || m_iCurrentGameMaterial == m_iTargetGameMaterial ) )
	{
		DevMsg( 2, "Player changed material to %d (was %d)\n", gameMaterial, m_iCurrentGameMaterial );

		m_iCurrentGameMaterial = (int)gameMaterial;

		SetThink( &CEnvPlayerSurfaceTrigger::UpdateMaterialThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function to fire outputs. Done this way so that sv_alternate ticks
//			doesn't allow multiple surface changes in the same tick to fire outputs.
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::UpdateMaterialThink( void )
{
	if ( m_iCurrentGameMaterial == m_iTargetGameMaterial )
	{
		m_OnSurfaceChangedToTarget.FireOutput( nullptr, this );
	}
	else 
	{
		m_OnSurfaceChangedFromTarget.FireOutput( nullptr, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}
