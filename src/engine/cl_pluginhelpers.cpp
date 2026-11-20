// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: baseclientstate.cpp: implementation of the CBaseClientState class.

// main precompiled header for client files

import std;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;






#include "client.h"
#include "cl_main.h"


#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "cl_pluginhelpers.h"

static CPluginUIManager s_PluginManager;
CPluginUIManager *g_PluginManager = &s_PluginManager;

ConVar cl_showpluginmessages ( "cl_showpluginmessages", "1", FCVAR_ARCHIVE, "Allow plugins to display messages to you" );

void PluginHelpers_Menu( const ks::net::CSVCMsg_Menu& msg )
{
}
