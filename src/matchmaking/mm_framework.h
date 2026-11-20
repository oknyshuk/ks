#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation


import std;


import ks.core.threadtools;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
#include "const.h"

#include "inetmsghandler.h"
#include "appframework/IAppSystemGroup.h"
#include "matchmaking/imatchframework.h"
#include "igameevents.h"

#include "tier2/tier2.h"

#include "extkeyvalues.h"


class CMatchFramework;


#include "mm_extensions.h"
#include "mm_events.h"
#include "mm_voice.h"
#include "mm_session.h"
#include "mm_netmgr.h"

#include "matchsystem.h"
#include "playermanager.h"

class CMatchFramework : public IMatchFramework
{
	// App system protocol (see IMatchFramework)
public:
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();

	// Methods of IMatchFramework
public:
	// Run frame of the matchmaking framework
	void RunFrame();


	// Get matchmaking extensions
	IMatchExtensions * GetMatchExtensions();

	// Get events container
	IMatchEventsSubscription * GetEventsSubscription();

	// Get the matchmaking title interface
	IMatchTitle * GetMatchTitle();

	// Get the match session interface of the current match framework type
	IMatchSession * GetMatchSession();

	// Get the network msg encode/decode factory
	IMatchNetworkMsgController * GetMatchNetworkMsgController();

	// Get the match system
	IMatchSystem * GetMatchSystem();
	
	// Send the key values back to the server
	void ApplySettings( KeyValues* keyValues );

	// Entry point to create session
	void CreateSession( KeyValues *pSettings );

	// Entry point to match into a session
	void MatchSession( KeyValues *pSettings );

	// Close the session
	void CloseSession();

	// Checks to see if the current game is being played online ( as opposed to locally against bots )
	bool IsOnlineGame( void );

	// Called by the client to notify matchmaking that it should update matchmaking properties based
	// on player distribution among the teams.
	void UpdateTeamProperties( KeyValues *pTeamProperties );

	//
	// Match events, cracked internally before the external subscribers
	//
public:
	void OnEvent( KeyValues *pEvent );

	// Additional matchmaking title-defined interface
public:
	IMatchTitleGameSettingsMgr * GetMatchTitleGameSettingsMgr();

public:
	void SetCurrentMatchSession( IMatchSessionInternal *pNewMatchSession );

public:
	CMatchFramework();
	~CMatchFramework();

protected:
	IMatchSessionInternal *m_pMatchSession;

	bool m_bJoinTeamSession;
	KeyValues *m_pTeamSessionSettings;
};

extern CMatchFramework *g_pMMF;

extern const char *COM_GetModDirectory();
extern bool IsLocalClientConnectedToServer();
