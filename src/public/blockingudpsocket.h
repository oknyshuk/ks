#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;


class CBlockingUDPSocket
{
public:
	explicit CBlockingUDPSocket();
	virtual ~CBlockingUDPSocket();

	bool WaitForMessage( float timeOutInSeconds );
	unsigned int ReceiveSocketMessage( ks::platform::address *packet_from, unsigned char *buf, std::size_t bufsize );
	bool SendSocketMessage( const ks::platform::address& rRecipient, const unsigned char *buf, std::size_t bufsize );

	bool	IsValid() const { return m_Socket != nullptr; }

protected:
	bool CreateSocket (void);

	netadr_t			m_cserIP;
	ks::platform::socket *m_Socket = nullptr;

private:
	CBlockingUDPSocket( const CBlockingUDPSocket& ); // = delete
	CBlockingUDPSocket& operator= ( const CBlockingUDPSocket& ); // = delete
};
