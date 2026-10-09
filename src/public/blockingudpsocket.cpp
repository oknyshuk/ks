// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "blockingudpsocket.h"

CBlockingUDPSocket::CBlockingUDPSocket() :
	m_cserIP(),
	m_Socket( nullptr )
{
	CreateSocket();
}

CBlockingUDPSocket::~CBlockingUDPSocket()
{
	ks::platform::close_socket( m_Socket );
}

bool CBlockingUDPSocket::CreateSocket( void )
{
	m_Socket = ks::platform::open_udp( nullptr );
	if ( m_Socket == nullptr )
		return false;

	ks::platform::address local{};
	if ( !ks::platform::bind( m_Socket, local, nullptr ) )
		return false;

	return true;
}

bool CBlockingUDPSocket::WaitForMessage( float timeOutInSeconds )
{
	return ks::platform::wait_readable( m_Socket, (unsigned long long)( timeOutInSeconds * 1000000000.0f ), nullptr );
}

unsigned int CBlockingUDPSocket::ReceiveSocketMessage( ks::platform::address *packet_from, unsigned char *buf, std::size_t bufsize )
{
	long received = ks::platform::receive_from( m_Socket, packet_from, buf, bufsize, nullptr );
	if ( received < 0 )
		return 0;

	buf[ received ] = 0;
	return (unsigned int)received;
}

bool CBlockingUDPSocket::SendSocketMessage( const ks::platform::address& rRecipient, const unsigned char *buf, std::size_t bufsize )
{
	return ks::platform::send_to( m_Socket, rRecipient, buf, bufsize, nullptr ) >= 0;
}
