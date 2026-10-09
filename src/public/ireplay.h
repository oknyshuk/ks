#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

class IServer;
class IReplayDirector;
class IGameEvent;
struct netadr_s;

//-----------------------------------------------------------------------------
// Interface the Replay module exposes to the engine
//-----------------------------------------------------------------------------
inline constexpr const char *INTERFACEVERSION_REPLAYSERVER = "ReplayServer001";

class IReplayServer : public IBaseInterface
{
public:
	virtual	~IReplayServer() {}

	virtual	IServer	*GetBaseServer( void ) = 0; // get Replay base server interface
	virtual	IReplayDirector *GetDirector( void ) = 0;	// get director interface
	virtual	int		GetReplaySlot( void ) = 0; // return entity index-1 of Replay in game
	virtual float	GetOnlineTime( void ) = 0; // seconds since broadcast started

	virtual void	BroadcastEvent(IGameEvent *event) = 0; // send a director command to all specs
};
