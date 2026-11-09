// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: round based game rule convars shared by the multiplayer rules.

#include "teamplayroundbased_gamerules.h"

ConVar mp_capstyle( "mp_capstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture points used. 0 = Fixed players required to cap. 1 = More players cap faster, but longer cap times." );
ConVar mp_blockstyle( "mp_blockstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture point blocking used. 0 = Blocks break captures completely. 1 = Blocks only pause captures." );
ConVar mp_respawnwavetime( "mp_respawnwavetime", "10.0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Time between respawn waves." );
ConVar mp_capdeteriorate_time( "mp_capdeteriorate_time", "90.0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Time it takes for a full capture point to deteriorate." );
ConVar mp_tournament ( "mp_tournament", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );


ConVar mp_teams_unbalance_limit( "mp_teams_unbalance_limit", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
					 "Teams are unbalanced when one team has this many more players than the other team. (0 disables check)",
					 true, 0,	// min value
					 true, 30	// max value
					 );

ConVar mp_maxrounds( "mp_maxrounds", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "max number of rounds to play before server changes maps", true, 0, false, 0 );
ConVar mp_winlimit( "mp_winlimit", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Max score one team can reach before server changes maps", true, 0, false, 0 );
ConVar mp_disable_respawn_times( "mp_disable_respawn_times", "0", FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar mp_bonusroundtime( "mp_bonusroundtime", "15", FCVAR_REPLICATED, "Time after round win until round restarts", true, 5, true, 15 );

#ifdef GAME_DLL
ConVar mp_showroundtransitions( "mp_showroundtransitions", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show gamestate round transitions." );
ConVar mp_enableroundwaittime( "mp_enableroundwaittime", "1", FCVAR_REPLICATED, "Enable timers to wait between rounds." );
ConVar mp_showcleanedupents( "mp_showcleanedupents", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show entities that are removed on round respawn." );
ConVar mp_restartround( "mp_restartround", "0", FCVAR_GAMEDLL, "If non-zero, the current round will restart in the specified number of seconds" );	

ConVar mp_stalemate_timelimit( "mp_stalemate_timelimit", "240", FCVAR_REPLICATED, "Timelimit (in seconds) of the stalemate round." );
ConVar mp_autoteambalance( "mp_autoteambalance", "1", FCVAR_NOTIFY | FCVAR_RELEASE );

ConVar mp_stalemate_enable( "mp_stalemate_enable", "0", FCVAR_NOTIFY, "Enable/Disable stalemate mode." );
ConVar mp_stalemate_at_timelimit( "mp_stalemate_at_timelimit", "0", FCVAR_NOTIFY, "Allow the match to end when mp_timelimit hits instead of waiting for the end of the current round." );
#endif // GAME_DLL

// Utility function
bool FindInList( const char **pStrings, const char *pToFind )
{
	int i = 0;
	while ( pStrings[i][0] != 0 )
	{
		if ( V_stricmp( pStrings[i], pToFind ) == 0 )
			return true;
		i++;
	}

	return false;
}
