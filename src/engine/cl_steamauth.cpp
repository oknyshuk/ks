//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//=============================================================================//
#ifdef _WIN32
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#else
#include <netinet/in.h>
#endif

#include "baseclient.h"
#include "utlvector.h"
#include "netadr.h"
#include "cl_steamauth.h"
#include "interface.h"
#include "filesystem_engine.h"
#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include "host.h"
#include "cmd.h"
#include "common.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/imaterialsystem.h"
#ifndef DEDICATED
#include "engineui.h"
#include "server.h"
#include "matchmaking/imatchframework.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

extern ConVar cl_hideserverip;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Client s_Steam3Client;
CSteam3Client  &Steam3Client()
{
	return s_Steam3Client;
}


static void Callback_SteamAPIWarningMessageHook( int n, const char *sz )
{
	if ( n == 0 )
	{
		Msg( "[STEAM] %s\n", sz );
	}
	else
	{
		Warning( "[STEAM] %s\n", sz );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Client::CSteam3Client() 
{
	m_bActive = false;
	m_bGSSecure = false;
	m_bGameOverlayActive = false;
	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteam3Client::~CSteam3Client()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CSteam3Client::Shutdown()
{	
	if ( !m_bActive )
		return;

	m_bActive = false;	
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CSteam3Client::Activate()
{
	if ( m_bActive )
		return;

	m_bActive = true;
	m_bGSSecure = false;

}


//-----------------------------------------------------------------------------
// Purpose: Get the steam3 logon cookie to use
//-----------------------------------------------------------------------------
void CSteam3Client::GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, uint64 unGSSteamID,  bool bSecure )
{
	m_bGSSecure = bSecure;

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Tell steam that we are leaving a server
//-----------------------------------------------------------------------------
void CSteam3Client::CancelAuthTicket()
{
	m_bGSSecure = false;
	if ( !SteamUser() )
		return;

}


//-----------------------------------------------------------------------------
// Purpose: Process any callbacks we may have
//-----------------------------------------------------------------------------
void CSteam3Client::RunFrame()
{
}


