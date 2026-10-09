#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "globals.h"

extern void GameDLLInit( void );

extern ConVar	displaysoundlist;
extern ConVar	mapcyclefile;
extern ConVar	servercfgfile;
extern ConVar	lservercfgfile;

// multiplayer server rules
extern ConVar	teamplay;
extern ConVar	fraglimit;
extern ConVar	falldamage;
extern ConVar	weaponstay;
extern ConVar	forcerespawn;
extern ConVar	footsteps;
extern ConVar	flashlight;
extern ConVar	aimcrosshair;
extern ConVar	teamlist;
extern ConVar	teamoverride;
extern ConVar	defaultteam;
extern ConVar	allowNPCs;

extern ConVar	suitvolume;

// Engine Cvars
extern const ConVar *g_pDeveloper;
