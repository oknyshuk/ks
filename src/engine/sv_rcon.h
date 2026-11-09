#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "sv_main.h"
#include "netmessages.h"
#include "net.h"
#include "client.h"
import std;
#include "sv_remoteaccess.h"
#include "tier2/socketcreator.h"
#include "igameserverdata.h"

inline constexpr auto RCON_MAX_OUTSTANDING_SENDS = 100; // max packets to queue before dropping connection
inline constexpr auto RCON_MAX_RCON_COMMAND_LEN = 1024; // max size of an incoming command in bytes

//-----------------------------------------------------------------------------
// container class to handle network streams
//-----------------------------------------------------------------------------
class CRConServer
{
public:
	CRConServer( const char *netAddress );
	CRConServer();
	~CRConServer();

	void SetAddress( const char *netAddress );
	void SetPassword( const char *pPassword );
	bool HasPassword() const;
	bool IsPassword( const char *pPassword ) const;
	bool CreateSocket();
	void RunFrame();
	bool IsConnected();
	void FinishRedirect( const char *msg, const netadr_t &adr );
	bool HandleFailedRconAuth( const netadr_t &adr );
	void SetRequestID( ra_listener_id listener, int iRequestID );
	bool BCloseAcceptedSocket( ra_listener_id nIndex );

	// Allows a server to request a listening client to connect to it
	bool ConnectToListeningClient( const netadr_t &adr, bool bSingleSocket );

	// Socket listener methods (bound by CSocketCreator)
	bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr ); 
	void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData ); 
	void OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData );

private:
	struct PendingSend_t
	{
		explicit PendingSend_t( std::span< const std::byte > bytes ) : data( bytes.begin(), bytes.end() ) {}

		std::span< const std::byte > Remaining() const { return std::span( data ).subspan( nSent ); }

		std::vector< std::byte >	data;
		std::size_t						nSent = 0;
	};

	struct ConnectedRConSocket_t
	{
		bool			authed;
		int				lastRequestID;
		ra_listener_id	listenerID;
		std::vector< std::byte >	packetbuffer; // data buffer containing pending incoming data for this connection
		std::deque< PendingSend_t > m_OutstandingSends; // packets pending to be send (queued because the socket would block )
	};

	struct FailedRCon_t
	{
		int								badPasswordCount;
		netadr_t						adr;
		std::deque< float >				badPasswordTimes;	// oldest first
		bool operator==( const FailedRCon_t &rhs ) const { return ( adr.CompareAdr( rhs.adr) == 0 ); }
		bool operator<( const FailedRCon_t &rhs ) const;
	};

	ConnectedRConSocket_t* GetSocketData( int nIndex );
	void ProcessAccept();

	// NOTE: This function can remove elements. If calling it from a loop,
	// always iterate over accepted sockets backwards to avoid problems.
	bool SendRCONResponse( int nIndex, const void *data, int len, bool fromQueue = false );	

	CSocketCreator m_Socket;
	std::vector< FailedRCon_t >	m_failedRcons;
	std::string m_Password;
	netadr_t m_Address;
	bool m_bSocketDeleted;
};


inline CRConServer::ConnectedRConSocket_t* CRConServer::GetSocketData( int nIndex )
{
	return (ConnectedRConSocket_t*)m_Socket.GetAcceptedSocketData( nIndex );
}

CRConServer & RCONServer();
