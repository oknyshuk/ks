#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "savegame_version.h"
import std;

// The launcher includes this file, too
#ifndef LAUNCHERONLY

void Host_Init( bool bIsDedicated );
void Host_Shutdown(void);
int  Host_Frame (float time, int iState );
void Host_ShutdownServer(void);
bool Host_NewGame( char *mapName, char *mapGroupName, bool loadGame, bool bBackgroundLevel, bool bSplitScreenConnect, const char *pszOldMap = nullptr, const char *pszLandmark = nullptr );
void Host_Changelevel( bool loadfromsavedgame, const char *mapname, char *mapGroupName, const char *start );
void Host_PrintStatus( cmd_source_t commandSource, void ( *print )(const char *fmt, ...), bool bShort );

const char *GetHostProductString();
const char *GetHostVersionString();
int32 GetHostVersion();
int32 GetClientVersion();
int32 GetServerVersion();

extern ConVar host_name;

extern int  gHostSpawnCount;

#endif // LAUNCHERONLY
