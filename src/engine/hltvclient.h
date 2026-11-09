#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseclient.h"

class CHLTVServer;

class CHLTVClient : public CBaseClient
{
	typedef CBaseClient BaseClass;

public:
	CHLTVClient(int slot, CBaseServer *pServer);
	~CHLTVClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);

	void PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );

	void FileReceived( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileDenied(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileSent(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );
	
	// IClient interface
	bool	ExecuteStringCommand( const char *s );
	void	SpawnPlayer( void );
	bool	ShouldSendMessages( void );
	bool	SendSnapshot( CClientFrame * pFrame );
	bool	SendSignonData( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( float fUpdaterate, bool bForce ); // override;
	void	UpdateUserSettings();
	
public: // IClientMessageHandlers
		
	
	bool CLCMsg_RespondCvarValue( const ks::net::CCLCMsg_RespondCvarValue& msg );
	bool CLCMsg_FileCRCCheck( const ks::net::CCLCMsg_FileCRCCheck& msg );

	bool NETMsg_SetConVar( const ks::net::CNETMsg_SetConVar& msg );
	bool NETMsg_PlayerAvatarData( const ks::net::CNETMsg_PlayerAvatarData& msg ) { return true; }
	
	bool CLCMsg_Move( const ks::net::CCLCMsg_Move& msg );
	bool CLCMsg_ClientInfo( const ks::net::CCLCMsg_ClientInfo& msg );
	bool CLCMsg_VoiceData( const ks::net::CCLCMsg_VoiceData& msg );
	bool CLCMsg_ListenEvents( const ks::net::CCLCMsg_ListenEvents& msg );

public:
	CClientFrame *GetDeltaFrame( int nTick );

public:
	bool	ProcessSignonStateMsg(int state, int spawncount);
	
public:
	CHLTVServer *m_pHLTV;
	int		m_nLastSendTick;	// last send tick, don't send ticks twice
	double	m_fLastSendTime;	// last net time we send a packet
	char	m_szPassword[128];	// client password
	double	m_flLastChatTime;	// last time user send a chat text
	bool	m_bNoChat;			// if true don't send chat message to this client
	char	m_szChatGroup[128];	// client password
};
