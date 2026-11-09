#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "igameserverdata.h"


typedef ks::platform::socket *SocketHandle_t;


// A listener is any object with these three methods; CSocketCreator binds to it in the
// templated constructor.
//   bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t &netAdr );  // return false to disallow socket acceptance
//   void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData );  // allocate data associated w/ the socket
//   void OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData );


//-----------------------------------------------------------------------------
// container class to handle network streams
//-----------------------------------------------------------------------------
class CSocketCreator 
{
public:
	CSocketCreator();

	template< class LISTENER >
	CSocketCreator( LISTENER *pListener )
		: m_fnShouldAcceptSocket( [pListener]( SocketHandle_t hSocket, const netadr_t &netAdr ) { return pListener->ShouldAcceptSocket( hSocket, netAdr ); } ),
		  m_fnOnSocketAccepted( [pListener]( SocketHandle_t hSocket, const netadr_t &netAdr, void **ppData ) { pListener->OnSocketAccepted( hSocket, netAdr, ppData ); } ),
		  m_fnOnSocketClosed( [pListener]( SocketHandle_t hSocket, const netadr_t &netAdr, void *pData ) { pListener->OnSocketClosed( hSocket, netAdr, pData ); } )
	{
		m_hListenSocket = nullptr;
	}

	~CSocketCreator();

	// Call this once per frame
	void RunFrame();

	// This method is used to put the socket in a mode where it's listening
	// for connections and a connection is made once the request is received
	bool CreateListenSocket( const netadr_t &netAdr, bool bListenOnAllInterfaces = false );
	void CloseListenSocket();
	bool IsListening() const;

	// This method is used to connect to/disconnect from an external listening socket creator
	// Returns accepted socket index, or -1 if it failed.
	// Use GetAcceptedSocket* methods to access this socket's data
	// if bSingleSocket == true, all accepted sockets are closed before the new one is opened
	// NOTE: Closing an accepted socket will re-index all the sockets with higher indices
	int ConnectSocket( const netadr_t &netAdr, bool bSingleSocket );
	void CloseAcceptedSocket( int nIndex );
	void CloseAllAcceptedSockets();
	int GetAcceptedSocketCount() const;
	SocketHandle_t GetAcceptedSocketHandle( int nIndex ) const;
	const netadr_t& GetAcceptedSocketAddress( int nIndex ) const;
	void* GetAcceptedSocketData( int nIndex );

	// Closes all open sockets (listen + accepted)
	void Disconnect();

private:
	enum
	{
		SOCKET_TCP_MAX_ACCEPTS = 2
	};

	void ProcessAccept();
	bool ConfigureSocket( SocketHandle_t sock );

public:
	struct AcceptedSocket_t
	{
		SocketHandle_t	m_hSocket;
		netadr_t		m_Address;
		void			*m_pData;

		bool operator==( const AcceptedSocket_t &rhs ) const { return ( m_Address.CompareAdr( rhs.m_Address ) == 0 ); }
	};

	std::move_only_function< bool( SocketHandle_t, const netadr_t & ) > m_fnShouldAcceptSocket;
	std::move_only_function< void( SocketHandle_t, const netadr_t &, void ** ) > m_fnOnSocketAccepted;
	std::move_only_function< void( SocketHandle_t, const netadr_t &, void * ) > m_fnOnSocketClosed;
	std::vector< AcceptedSocket_t > m_hAcceptedSockets;
	SocketHandle_t	m_hListenSocket;	// Used to accept connections
	netadr_t		m_ListenAddress;	// Address used to listen on
};

