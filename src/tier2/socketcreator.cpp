// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Utility class to help in socket creation. Works for clients + servers

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.netadr;

#include "socketcreator.h"

const char *NET_ErrorString( ks::platform::socket_error error )
{
	return ks::platform::socket_error_text( error );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSocketCreator::CSocketCreator()
{
	m_hListenSocket = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSocketCreator::~CSocketCreator()
{
	Disconnect();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the listening socket is created and listening
//-----------------------------------------------------------------------------
bool CSocketCreator::IsListening() const
{
	return m_hListenSocket != nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Bind to a TCP port and accept incoming connections
//-----------------------------------------------------------------------------
bool CSocketCreator::CreateListenSocket( const netadr_t &netAdr, bool bListenOnAllInterfaces )
{
	CloseListenSocket();

	ks::platform::socket_error error = ks::platform::socket_error::none;
	m_ListenAddress = netAdr;
	m_hListenSocket = ks::platform::open_tcp( &error );
	if ( m_hListenSocket == nullptr )
	{
		Warning( "Socket unable to create socket (%s)\n", NET_ErrorString( error ) );
		return false;
	}

	if ( !ConfigureSocket( m_hListenSocket ) )
	{
		CloseListenSocket();
		return false;
	}

	ks::platform::address local = m_ListenAddress.ToAddress();
	if ( bListenOnAllInterfaces )
		local.ip = 0;
	if ( !ks::platform::bind( m_hListenSocket, local, &error ) )
	{
		Warning( "Socket bind failed (%s)\n", NET_ErrorString( error ) );
		CloseListenSocket();
		return false;
	}

	if ( !ks::platform::listen_on( m_hListenSocket, SOCKET_TCP_MAX_ACCEPTS, &error ) )
	{
		Warning( "Socket listen failed (%s)\n", NET_ErrorString( error ) );
		CloseListenSocket();
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Configures a socket for use
//-----------------------------------------------------------------------------
bool CSocketCreator::ConfigureSocket( SocketHandle_t sock )
{
	// disable NAGLE (rcon cmds are small in size)
	ks::platform::set_socket_option( sock, ks::platform::socket_option::no_delay, 1, nullptr );
	ks::platform::set_socket_option( sock, ks::platform::socket_option::reuse_address, 1, nullptr );

	ks::platform::socket_error error = ks::platform::socket_error::none;
	if ( !ks::platform::set_nonblocking( sock, &error ) )
	{
		Warning( "Socket accept set_nonblocking failed (%s)\n", NET_ErrorString( error ) );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle a new connection
//-----------------------------------------------------------------------------
void CSocketCreator::ProcessAccept()
{
	ks::platform::address peer{};
	ks::platform::socket_error error = ks::platform::socket_error::none;
	SocketHandle_t newSocket = ks::platform::accept_connection( m_hListenSocket, &peer, &error );
	if ( newSocket == nullptr )
	{
		if ( error != ks::platform::socket_error::again && error != ks::platform::socket_error::interrupted )
		{
			Warning ("Socket ProcessAccept Error: %s\n", NET_ErrorString( error ) );
		}
		return;
	}

	if ( !ConfigureSocket( newSocket ) )
	{
		ks::platform::close_socket( newSocket );
		return; 
	}

	netadr_t adr;
	adr.SetFromAddress( peer );
	if ( m_fnShouldAcceptSocket && !m_fnShouldAcceptSocket( newSocket, adr ) )
	{
		ks::platform::close_socket( newSocket );
		return;
	}

	// new connection TCP request, put in accepted queue
	int nIndex = std::ssize( m_hAcceptedSockets ); m_hAcceptedSockets.emplace_back();
	AcceptedSocket_t *pNewEntry = &m_hAcceptedSockets[nIndex];
	pNewEntry->m_hSocket = newSocket;
	pNewEntry->m_Address = adr;
	pNewEntry->m_pData = nullptr;

	void* pData = nullptr;
	if ( m_fnOnSocketAccepted )
	{
		m_fnOnSocketAccepted( newSocket, adr, &pData );
	}
	pNewEntry->m_pData = pData;
}


//-----------------------------------------------------------------------------
// Purpose: connect to the remote server
//-----------------------------------------------------------------------------
int CSocketCreator::ConnectSocket( const netadr_t &netAdr, bool bSingleSocket )
{
	if ( bSingleSocket )
	{
		CloseAllAcceptedSockets();
	}

	ks::platform::socket_error error = ks::platform::socket_error::none;
	SocketHandle_t hSocket = ks::platform::open_tcp( &error );
	if ( hSocket == nullptr )
	{
		Warning( "Unable to create socket (%s)\n", NET_ErrorString( error ) );
		return -1;
	}

	if ( !ks::platform::set_nonblocking( hSocket, &error ) )
	{
		Warning( "Socket set_nonblocking failed (%s)\n", NET_ErrorString( error ) );
		ks::platform::close_socket( hSocket );
		return -1;
	}

	// disable NAGLE (rcon cmds are small in size)
	ks::platform::set_socket_option( hSocket, ks::platform::socket_option::no_delay, 1, nullptr );

	if ( !ks::platform::connect( hSocket, netAdr.ToAddress(), &error ) )
	{
		if ( error != ks::platform::socket_error::again )
		{	
			Warning( "Socket connection failed (%s)\n", NET_ErrorString( error ) );
			ks::platform::close_socket( hSocket );
			return -1;
		}

		// block for at most 1 second
		if ( !ks::platform::wait_writable( hSocket, 1000000000ull, nullptr ) )
		{
			ks::platform::close_socket( hSocket );		// took too long to connect to, give up
			return -1;
		}
	}

	if ( m_fnShouldAcceptSocket && !m_fnShouldAcceptSocket( hSocket, netAdr ) )
	{
		ks::platform::close_socket( hSocket );
		return -1;
	}

	// new connection TCP request, put in accepted queue
	void *pData = nullptr;
	int nIndex = std::ssize( m_hAcceptedSockets ); m_hAcceptedSockets.emplace_back();
	AcceptedSocket_t *pNewEntry = &m_hAcceptedSockets[nIndex];
	pNewEntry->m_hSocket = hSocket;
	pNewEntry->m_Address = netAdr;
	pNewEntry->m_pData = nullptr;

	if ( m_fnOnSocketAccepted )
	{
		m_fnOnSocketAccepted( hSocket, netAdr, &pData );
	}

	pNewEntry->m_pData = pData;
	return nIndex;
}


//-----------------------------------------------------------------------------
// Purpose: close an open rcon connection
//-----------------------------------------------------------------------------
void CSocketCreator::CloseListenSocket()
{
	if ( m_hListenSocket != nullptr )
	{
		ks::platform::close_socket( m_hListenSocket );
		m_hListenSocket = nullptr;
	}
}

void CSocketCreator::CloseAcceptedSocket( int nIndex )
{
	if ( nIndex >= std::ssize( m_hAcceptedSockets ) )
		return;

	AcceptedSocket_t& connected = m_hAcceptedSockets[nIndex];
	if ( m_fnOnSocketClosed )
	{
		m_fnOnSocketClosed( connected.m_hSocket, connected.m_Address, connected.m_pData );
	}
	ks::platform::close_socket( connected.m_hSocket );
	m_hAcceptedSockets.erase( m_hAcceptedSockets.begin() + nIndex );
}

void CSocketCreator::CloseAllAcceptedSockets()
{
	int nCount = std::ssize( m_hAcceptedSockets );
	for ( int i = 0; i < nCount; ++i )
	{
		AcceptedSocket_t& connected = m_hAcceptedSockets[i];
		if ( m_fnOnSocketClosed )
		{
			m_fnOnSocketClosed( connected.m_hSocket, connected.m_Address, connected.m_pData );
		}
		ks::platform::close_socket( connected.m_hSocket );
	}
	m_hAcceptedSockets.clear();
}


void CSocketCreator::Disconnect()
{
	CloseListenSocket();
	CloseAllAcceptedSockets();
}


//-----------------------------------------------------------------------------
// Purpose: accept new connections and walk open sockets and handle any incoming data
//-----------------------------------------------------------------------------
void CSocketCreator::RunFrame()
{
	if ( IsListening() )
	{
		ProcessAccept(); // handle any new connection requests
	}
}


//-----------------------------------------------------------------------------
// Returns socket info
//-----------------------------------------------------------------------------
int CSocketCreator::GetAcceptedSocketCount() const
{
	return std::ssize( m_hAcceptedSockets );
}

SocketHandle_t CSocketCreator::GetAcceptedSocketHandle( int nIndex ) const
{
	return m_hAcceptedSockets[nIndex].m_hSocket;
}

const netadr_t& CSocketCreator::GetAcceptedSocketAddress( int nIndex ) const
{
	return m_hAcceptedSockets[nIndex].m_Address;
}

void* CSocketCreator::GetAcceptedSocketData( int nIndex )
{
	return m_hAcceptedSockets[nIndex].m_pData;
}
