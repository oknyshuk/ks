#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "inetmessage.h"

class	INetChannel;
typedef struct netpacket_s netpacket_t;

// Processes a connectionless packet (the engine passes the server / client state / LAN manager that owns the socket)
using ConnectionlessPacketHandler_t = std::function_ref< bool ( netpacket_t *packet ) >;
