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
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "foundryhelpers_server.h"
#include "basetempentity.h"

//-----------------------------------------------------------------------------
// Purpose: This just marshalls certain FoundryHelpers_ calls to the client.
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEFoundryHelpers" } ]]
      CTEFoundryHelpers : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFoundryHelpers, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEFoundryHelpers( const char *pName ) :
		CBaseTempEntity( pName )
	{
	}

public:
	CNetworkVar( int, m_iEntity, [[= ks::reflect::Net{ .bits = 32 } ]] );	// -1 means turn the effect off for all entities.
};

IMPLEMENT_REFLECT_SERVERCLASS( CTEFoundryHelpers, DT_TEFoundryHelpers )

// Singleton to fire TEMuzzleFlash objects
static CTEFoundryHelpers g_TEFoundryHelpers( "FoundryHelpers" );


void FoundryHelpers_ClearEntityHighlightEffects()
{
	g_TEFoundryHelpers.m_iEntity = -1;
	
	CBroadcastRecipientFilter filter;
	g_TEFoundryHelpers.Create( filter, 0 );
}

void FoundryHelpers_AddEntityHighlightEffect( CBaseEntity *pEnt )
{
	g_TEFoundryHelpers.m_iEntity = pEnt->entindex();
	
	CBroadcastRecipientFilter filter;
	g_TEFoundryHelpers.Create( filter, 0 );
}


CBasePlayer* CheckInFoundryMode()
{
	if ( !serverfoundry )
	{
		Warning( "Not in Foundry mode.\n" );
		return nullptr;
	}

	return UTIL_GetListenServerHost();
}


void GetCrosshairOrNamedEntities( const CCommand &args, std::vector< CBaseEntity* > &entities )
{
	if ( args.ArgC() < 2 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,
			MASK_SHOT_HULL|CONTENTS_GRATE|CONTENTS_DEBRIS, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() && !tr.DidHitWorld() )
		{
			entities.push_back( tr.Ent<CBaseEntity>() );
		}
	}
	else
	{
		CBaseEntity *pEnt = nullptr;
		while ((pEnt = gEntList.FindEntityGeneric( pEnt, args[1] ) ) != nullptr)
		{
			entities.push_back( pEnt );
		}
	}
}


static void foundry_update_entity( const CCommand &args );
static ConCommand foundry_update_entity_command( "foundry_update_entity", foundry_update_entity, "Updates the entity's position/angles when in edit mode", FCVAR_CHEAT );
static void foundry_update_entity( const CCommand &args )
{
	if ( !CheckInFoundryMode() )
		return;

	std::vector< CBaseEntity* > entities;
	GetCrosshairOrNamedEntities( args, entities );

	for ( int i=0; i < std::ssize( entities ); i++ )
	{
		CBaseEntity *pEnt = entities[i];
		serverfoundry->MoveEntityTo( pEnt->GetHammerID(), pEnt->GetAbsOrigin(), pEnt->GetAbsAngles() );
	}
}

static void foundry_sync_hammer_view( const CCommand &args );
static ConCommand foundry_sync_hammer_view_command( "foundry_sync_hammer_view", foundry_sync_hammer_view, "Move Hammer's 3D view to the same position as the engine's 3D view.", FCVAR_CHEAT );
static void foundry_sync_hammer_view( const CCommand &args )
{
	CBasePlayer *pPlayer = CheckInFoundryMode();
	if ( !pPlayer )
		return;

	Vector vPos = pPlayer->EyePosition();
	QAngle vAngles = pPlayer->pl.v_angle;
	serverfoundry->MoveHammerViewTo( vPos, vAngles );
}

static void foundry_engine_get_mouse_control( const CCommand &args );
static ConCommand foundry_engine_get_mouse_control_command( "foundry_engine_get_mouse_control", foundry_engine_get_mouse_control, "Give the engine control of the mouse.", FCVAR_CHEAT );
static void foundry_engine_get_mouse_control( const CCommand &args )
{
	if ( !CheckInFoundryMode() )
		return;

	serverfoundry->EngineGetMouseControl();
}


static void foundry_engine_release_mouse_control( const CCommand &args );
static ConCommand foundry_engine_release_mouse_control_command( "foundry_engine_release_mouse_control", foundry_engine_release_mouse_control, "Give the control of the mouse back to Hammer.", FCVAR_CHEAT );
static void foundry_engine_release_mouse_control( const CCommand &args )
{
	if ( !CheckInFoundryMode() )
		return;

	serverfoundry->EngineReleaseMouseControl();
}

static void foundry_select_entity( const CCommand &args );
static ConCommand foundry_select_entity_command( "foundry_select_entity", foundry_select_entity, "Select the entity under the crosshair or select entities with the specified name.", FCVAR_CHEAT );
static void foundry_select_entity( const CCommand &args )
{
	CBasePlayer *pPlayer = CheckInFoundryMode();
	if ( !pPlayer )
		return;

	std::vector< CBaseEntity* > entities;
	GetCrosshairOrNamedEntities( args, entities );

	std::vector< int > hammerIDs;
	for ( int i=0; i < std::ssize( entities ); i++ )
	{
		CBaseEntity *pEnt = entities[i];
		hammerIDs.push_back( pEnt->GetHammerID() );
	}

	if ( hammerIDs.empty() )
	{
		Vector vPos = pPlayer->EyePosition();
		QAngle vAngles = pPlayer->pl.v_angle;
		serverfoundry->SelectionClickInCenterOfView( vPos, vAngles );
	}
	else
	{
		serverfoundry->SelectEntities( hammerIDs.data(), std::ssize( hammerIDs ) );
	}
}
