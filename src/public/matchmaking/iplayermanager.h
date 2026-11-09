#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core;
class IPlayer;
class IPlayerFriend;
class IPlayerLocal;

class IPlayerManager
{
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
	//	returns player interface by player's XUID or NULL if player not found or not available
	//
	IPlayer * FindPlayer( XUID xuid );
};
