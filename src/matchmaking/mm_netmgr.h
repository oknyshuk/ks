#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

class CConnectionlessLanMgr;

#include "mm_framework.h"
import ks.tier1.keyvalues;

class CConnectionlessLanMgr
{
	//
	// IConnectionlessPacketHandler
	//
public:
	bool ProcessConnectionlessPacket( netpacket_t *packet );

public:
	void Update();
	void SendPacket( KeyValues *msg, char const *szAddress = nullptr, INetSupport::NetworkSocket_t eSock
		= INetSupport::NS_SOCK_CLIENT
		);
	KeyValues * UnpackPacket( netpacket_t *packet );

public:
	CConnectionlessLanMgr();
	~CConnectionlessLanMgr();

protected:
	std::vector< std::byte > m_buffer;
};

// Match events subscription singleton
extern CConnectionlessLanMgr *g_pConnectionlessLanMgr;
