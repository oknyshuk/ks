// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for server files

import std;
import ks.tier1;

#include "server.h"
#include "server.h"

static void SvSearchKeyChangeCallback( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
	sv.UpdateGameData();
}

ConVar sv_search_key( "sv_search_key",
						"",
						  FCVAR_RELEASE, "When searching for a dedicated server from lobby, restrict search to only dedicated servers having the same sv_search_key.", SvSearchKeyChangeCallback );
