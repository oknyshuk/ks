//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//=============================================================================//

//-----------------------------------------------------------------------------
// Purpose: the plugin message handler
//-----------------------------------------------------------------------------
#include "netmessages.h"

class CPluginUIManager
{
public:
	bool IsVisible() const { return false; }
	void Shutdown() {}
};

extern CPluginUIManager *g_PluginManager;


void PluginHelpers_Menu( const ks::net::CSVCMsg_Menu& msg );
