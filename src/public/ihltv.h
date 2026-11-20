#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;

class IServer;
class IHLTVDirector;
class CGameEvent;
using IGameEvent = CGameEvent;
class CGameInfo;
struct netadr_s;



//-----------------------------------------------------------------------------
// Interface the HLTV module exposes to the engine
//-----------------------------------------------------------------------------
inline constexpr const char *INTERFACEVERSION_HLTVSERVER = "HLTVServer001";

class IHLTVServer
{
public:


	IServer	*GetBaseServer( void ); // get HLTV base server interface
	IHLTVDirector *GetDirector( void );	// get director interface
	int		GetHLTVSlot( void ); // return entity index-1 of HLTV in game
	float	GetOnlineTime( void ); // seconds since broadcast started
	void	GetLocalStats( int &proxies, int &slots, int &specs ); 
	void	GetGlobalStats( int &proxies, int &slots, int &specs ); 

	const netadr_s *GetRelayAddress( void ); // returns relay address

	bool	IsMasterProxy( void ); // true, if this is the HLTV master proxy
	bool	IsDemoPlayback( void ); // true if this is a HLTV demo

	void	BroadcastEvent(IGameEvent *event); // send a director command to all specs

	bool		IsRecording();
	const char* GetRecordingDemoFilename();
	void		StartAutoRecording();
	void		StopRecording( const CGameInfo *pGameInfo = nullptr );
};
