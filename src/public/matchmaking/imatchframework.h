#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

class IMatchFramework;
class IMatchSession;

import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;

import std;

#include "inetchannel.h"

#include "imatchasync.h"
#include "imatchtitle.h"
#include "imatchnetworkmsg.h"
#include "imatchextensions.h"
#include "imatchevents.h"
#include "imatchsystem.h"
#include "iplayermanager.h"
#include "iplayer.h"
#include "imatchvoice.h"

typedef void (*RankedMatchStartCallback)( KeyValues *pSettings, uint32 volatile *pResult );

class IMatchFramework
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

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
};

inline constexpr const char *IMATCHFRAMEWORK_VERSION_STRING = "MATCHFRAMEWORK_001";



class IMatchSession
{
public:
	// Get an internal pointer to session system-specific data
	KeyValues * GetSessionSystemData();

	// Get an internal pointer to session settings
	KeyValues * GetSessionSettings();

	// Update session settings, only changing keys and values need
	// to be passed and they will be updated
	void UpdateSessionSettings( KeyValues *pSettings );

	// Issue a session command
	void Command( KeyValues *pCommand );

	// Get the lobby or XSession ID 
	uint64 GetSessionID();

	// Callback when team changes
	void UpdateTeamProperties( KeyValues *pTeamProperties );
};
