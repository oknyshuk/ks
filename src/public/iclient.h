#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "inetmsghandler.h"
#include "userid.h"
import std;

class IServer;
class INetMessage;
struct NetMessageCvar_t;
namespace ks::net { struct CMsg_CVars; }

enum CrossPlayPlatform_t
{
	CROSSPLAYPLATFORM_UNKNOWN = 0,
	CROSSPLAYPLATFORM_PC,
	CROSSPLAYPLATFORM_X360,
	CROSSPLAYPLATFORM_PS3,

	CROSSPLAYPLATFORM_LAST = CROSSPLAYPLATFORM_PS3,
};

inline bool IsCrossPlayPlatformAConsole( CrossPlayPlatform_t platform )
{
	return (platform == CROSSPLAYPLATFORM_X360) || (platform == CROSSPLAYPLATFORM_PS3);
}

//provide the compiled code's platform for convenience
#	define CROSSPLAYPLATFORM_THISPLATFORM CROSSPLAYPLATFORM_PC

struct HltvReplayParams_t
{
	HltvReplayParams_t()
	{
		m_nPrimaryTargetEntIndex = 0;
		m_flPlaybackSpeed = 1.0f;
		m_flDelay = 10.0f;
		m_flStopAt = 0.0f;
		m_flSlowdownBeginAt = 0;
		m_flSlowdownEndAt = 0;
		m_flSlowdownRate = 1.0f;
		m_bAbortCurrentReplay = false;
	}
	int m_nPrimaryTargetEntIndex;
	float m_flDelay;
	float m_flStopAt;
	float m_flPlaybackSpeed;
	float m_flSlowdownBeginAt;
	float m_flSlowdownEndAt;
	float m_flSlowdownRate;
	bool m_bAbortCurrentReplay; // even if replay is in progress, restart it - this may cause a stutter on the client, so use with care
};

class IClient : public INetChannelHandler
{
public:
	IClient() : INetChannelHandler( handler_kind_t::client ) {}


	// connect client
	void	Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const ks::net::CMsg_CVars *pVecCvars = nullptr );

	// set the client in a pending state waiting for a new game
	void	Inactivate( void );
	
	// Reconnect without dropiing the netchannel
	void	Reconnect( void );				// froce reconnect

	// disconnects a client with a given reason
	void	Disconnect( const char *reason );

	bool	ChangeSplitscreenUser( int nSplitScreenUserSlot );

	int				GetPlayerSlot() const; // returns client slot (usually entity number-1)
	int				GetUserID() const; // unique ID on this server 
	const USERID_t	GetNetworkID() const; // network wide ID
	const char		*GetClientName() const;	// returns client name
	INetChannel		*GetNetChannel(); // returns client netchannel
	IServer			*GetServer(); // returns the object server the client belongs to
	const char		*GetUserSetting(const char *cvar) const; // returns a clients FCVAR_USERINFO setting
	const char		*GetNetworkIDString() const; // returns a human readable representation of the network id

	// set/get client data rate in bytes/second
	void	SetRate( int nRate, bool bForce );
	int		GetRate( void ) const;
	
	// set/get updates/second rate
	void	SetUpdateRate( float fUpdateRate, bool bForce );
	float	GetUpdateRate( void ) const;	

	// clear complete object & free all memory 
	void	Clear( void );

	// returns the highest world tick number acknowledge by client
	int		GetMaxAckTickCount() const;
	
	// execute a client command
	bool	ExecuteStringCommand( const char *s );
	// send client a network message
	bool	SendNetMsg(INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	// send client a text message
	void	ClientPrintf (const char *fmt, ...);

		// client has established network channels, nothing else
	bool	IsConnected( void ) const;
	// client is downloading signon data
	bool	IsSpawned( void ) const;
	// client active is ingame, receiving snapshots
	bool	IsActive( void ) const;
	// returns true, if client is not a real player
	bool	IsFakeClient( void ) const;
	// returns true, if client is a HLTV proxy
	bool	IsHLTV( void ) const;
	// returns true, if client hears this player
	bool	IsHearingClient(int index) const;
	// returns true, if client hears this player by proximity
	bool	IsProximityHearingClient(int index) const;

	void	SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize );

	// returns true, if client is a split screen user
	bool	IsSplitScreenUser( void ) const;

	bool	CheckConnect( void );

	bool	IsLowViolenceClient( void ) const;

	IClient	*GetSplitScreenOwner();

	// get the number of players on this client's machine
	int		GetNumPlayers();

	bool	IsHumanPlayer() const;

	CrossPlayPlatform_t GetClientPlatform() const;

	bool StartHltvReplay( const HltvReplayParams_t &params );
	void StopHltvReplay();
	int GetHltvReplayDelay();
	const char * GetHltvReplayStatus() const;
	bool CanStartHltvReplay();
	void ResetReplayRequestTime();
};
