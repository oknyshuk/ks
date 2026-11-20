#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.platform;
import ks.core.threadtools;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
#include "matchmaking/imatchevents.h"

class PlayerManager;

import std;

#include "matchmaking/iplayer.h"
#include "matchmaking/iplayermanager.h"

class PlayerManager: public IPlayerManager
{
public :
	PlayerManager();
	~PlayerManager();

	//IPlayerManager
public:
	//
	// GetLocalPlayer
	//	returns a local player interface for a given controller index
	//
	IPlayerLocal * GetLocalPlayer( int iController );

	//
	// GetFriend
	//	returns player interface to the given friend or NULL if friend not found or not available
	//
	IPlayerFriend * GetFriendByXUID( XUID xuid );

	//
	// FindPlayer
	//	returns player interface by player's XUID or NULL if friend not found or not available
	//
	IPlayer * FindPlayer( XUID xuid );

	// Match events
public:
	void OnEvent( KeyValues *pEvent );
};


extern class PlayerManager *g_pPlayerManager;
