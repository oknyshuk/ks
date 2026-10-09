#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;
#include "matchmaking/imatchevents.h"

class PlayerManager;

import std;

#include "matchmaking/iplayer.h"
#include "matchmaking/iplayermanager.h"

class PlayerManager: public IPlayerManager, public IMatchEventsSink
{
public :
	PlayerManager();
	virtual ~PlayerManager();

	//IPlayerManager
public:
	//
	// GetLocalPlayer
	//	returns a local player interface for a given controller index
	//
	virtual IPlayerLocal * GetLocalPlayer( int iController );

	//
	// GetFriend
	//	returns player interface to the given friend or NULL if friend not found or not available
	//
	virtual IPlayerFriend * GetFriendByXUID( XUID xuid );

	//
	// FindPlayer
	//	returns player interface by player's XUID or NULL if friend not found or not available
	//
	virtual IPlayer * FindPlayer( XUID xuid );

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );
};


extern class PlayerManager *g_pPlayerManager;
