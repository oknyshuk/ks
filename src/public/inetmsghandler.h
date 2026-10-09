#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "inetmessage.h"

class	INetChannel;
typedef struct netpacket_s netpacket_t;

class IConnectionlessPacketHandler
{
public:
	virtual	~IConnectionlessPacketHandler( void ) {};

	virtual bool ProcessConnectionlessPacket( netpacket_t *packet ) = 0;	// process a connectionless packet
};
