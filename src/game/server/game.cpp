// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "shareddefs.h"
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
#include "basetempentity.h"
#include "player.h"
#include "game.h"

void MapCycleFileChangedCallback( ConVar *var, const char *pOldString, float flOldValue )
{
	if ( V_stricmp( pOldString, mapcyclefile.GetString() ) != 0 )
	{
		if ( GameRules() )
		{
			// For multiplayer games, forces the mapcyclefile to be reloaded
			GameRules()->ResetMapCycleTimeStamp();
		}
	}
}

ConVar	displaysoundlist( "displaysoundlist","0" );
ConVar  mapcyclefile( "mapcyclefile", "mapcycle.txt", FCVAR_NONE, "Name of the .txt file used to cycle the maps on multiplayer servers ", MapCycleFileChangedCallback );
ConVar  loopsingleplayermaps("loopsingleplayermaps", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar  servercfgfile( "servercfgfile","server.cfg", FCVAR_RELEASE );
ConVar  lservercfgfile( "lservercfgfile","listenserver.cfg" );

// multiplayer server rules
ConVar	teamplay( "mp_teamplay","0", FCVAR_NOTIFY );
ConVar	falldamage( "mp_falldamage","0", FCVAR_NOTIFY );
ConVar	weaponstay( "mp_weaponstay","0", FCVAR_NOTIFY );
ConVar	forcerespawn( "mp_forcerespawn","1", FCVAR_NOTIFY );
ConVar	footsteps( "mp_footsteps","1", FCVAR_NOTIFY );
ConVar	flashlight( "mp_flashlight","0", FCVAR_NOTIFY );
ConVar	aimcrosshair( "mp_autocrosshair","1", FCVAR_NOTIFY );
ConVar	teamlist( "mp_teamlist","hgrunt;scientist", FCVAR_NOTIFY );
ConVar	teamoverride( "mp_teamoverride","1" );
ConVar	defaultteam( "mp_defaultteam","0" );
ConVar	allowNPCs( "mp_allowNPCs","1", FCVAR_NOTIFY );

// Engine Cvars
const ConVar	*g_pDeveloper = nullptr;


ConVar suitvolume( "suitvolume", "0.25", FCVAR_ARCHIVE );

static bool GameDLL_ConVarAccessor( ConCommandBase *pCommand )
{
	// Remember "unlinked" default value for replicated cvars
	bool replicated = pCommand->IsFlagSet( FCVAR_REPLICATED );
	const char *defvalue = nullptr;
	if ( replicated && !pCommand->IsCommand() )
	{
		defvalue = ( ( ConVar * )pCommand)->GetDefault();
	}

	// Link to engine's list instead
	cvar->RegisterConCommand( pCommand );

	if ( !cvar->GetCommandLineValue( pCommand->GetName() ) )
	{
		// NOTE:  If not overridden at the command line, then if it's a replicated cvar, make sure that it's
		//  value is the server's value.  This solves a problem where think_limit is defined in shared
		//  code but the value is inside and #if defined( _DEBUG ) block and if you have a debug game .dll
		//  and a release client, then the limiit was coming from the client even though the server value 
		//  was the one that was important during debugging.  Now the server trumps the client value for
		//  replicated ConVars by setting the value here after the ConVar has been linked.
		if ( replicated && defvalue && !pCommand->IsCommand() )
		{
			ConVar *var = ( ConVar * )pCommand;
			var->SetValue( defvalue );
		}
	}

	return true;
}

// Register your console variables here
// This gets called one time when the game is initialied
void InitializeCvars( void )
{
	// Register cvars here:
	ConVar_Register( ks::server_registry().convars, FCVAR_GAMEDLL, &GameDLL_ConVarAccessor ); 

	g_pDeveloper	= cvar->FindVar( "developer" );
}
