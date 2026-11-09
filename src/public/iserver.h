#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <inetmsghandler.h>
#include <const.h>

class INetMessage;
class IRecipientFilter;
class IClient;

typedef struct player_info_s player_info_t;

class IServer
{
public:

	int		GetNumClients( void ) const; // returns current number of clients
	int		GetNumProxies( void ) const; // returns number of attached HLTV proxies
	int		GetNumFakeClients() const; // returns number of fake clients/bots
	int		GetMaxClients( void ) const; // returns current client limit
	IClient	*GetClient( int index ); // returns interface to client 
	int		GetClientCount() const; // returns number of clients slots (used & unused)
	int		GetUDPPort( void ) const; // returns current used UDP port
	float	GetTime( void ) const;	// returns game world time
	int		GetTick( void ) const;	// returns game world tick
	float	GetTickInterval( void ) const; // tick interval in seconds
	float	GetTimescale( void ) const; // returns the game time scale (multiplied in conjunction with host_timescale)
	const char *GetName( void ) const;	// public server name
	const char *GetMapName( void ) const; // current map name (BSP)
	int		GetSpawnCount( void ) const;	
	int		GetNumClasses( void ) const;
	int		GetClassBits( void ) const;
	void	GetNetStats( float &avgIn, float &avgOut ); // total net in/out in bytes/sec
	int		GetNumPlayers();
	bool	GetPlayerInfo( int nClientIndex, player_info_t *pinfo );

	bool	IsActive( void ) const;	
	bool	IsLoading( void ) const;
	bool	IsDedicated( void ) const;
	bool	IsPaused( void ) const;
	bool	IsMultiplayer( void ) const;
	bool	IsPausable() const;
	bool	IsHLTV() const;
	bool	IsReplay() const;

	const char * GetPassword() const;	// returns the password or NULL if none set	

	void	SetPaused(bool paused);
	void	SetTimescale( float flTimescale );
	void	SetPassword(const char *password); // set password (NULL to disable)

	void	BroadcastMessage( INetMessage &msg, bool onlyActive = false, bool reliable = false);
	void	BroadcastMessage( INetMessage &msg, IRecipientFilter &filter );

	void	DisconnectClient( IClient *client, const char *reason );
};
