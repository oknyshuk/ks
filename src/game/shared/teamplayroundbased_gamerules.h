#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

// Purpose: round based game rule convars shared by the multiplayer rules.

#include "teamplay_gamerules.h"

extern ConVar mp_respawnwavetime;

#ifdef GAME_DLL
	extern ConVar mp_showroundtransitions;
	extern ConVar mp_enableroundwaittime;
	extern ConVar mp_showcleanedupents;
	extern ConVar mp_bonusroundtime;
	extern ConVar mp_restartround;
	extern ConVar mp_winlimit;
	extern ConVar mp_stalemate_timelimit;
	extern ConVar mp_stalemate_enable;
#endif

// Utility function
bool FindInList( const char **pStrings, const char *pToFind );
