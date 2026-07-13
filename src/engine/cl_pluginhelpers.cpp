//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//===========================================================================//

#include "client_pch.h"
#include "cl_pluginhelpers.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static CPluginUIManager s_PluginManager;
CPluginUIManager *g_PluginManager = &s_PluginManager;

ConVar cl_showpluginmessages ( "cl_showpluginmessages", "1", FCVAR_ARCHIVE, "Allow plugins to display messages to you" );

void PluginHelpers_Menu( const CSVCMsg_Menu& msg )
{
}
