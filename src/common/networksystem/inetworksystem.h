#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.netadr;
import std;


// This is the packet payload without any header bytes (which are attached for actual sending)
inline constexpr auto NET_MAX_PAYLOAD = ( 262144 - 4); // largest message we can send in bytes
inline constexpr auto NET_MAX_PAYLOAD_BITS = 18; // 2^NET_MAX_PAYLOAD_BITS > NET_MAX_PAYLOAD
// This is just the client_t->netchan.datagram buffer size (shouldn't ever need to be huge)
inline constexpr auto NET_MAX_DATAGRAM_PAYLOAD = 4000; // = maximum unreliable playload size

// UDP has 28 byte headers
inline constexpr auto UDP_HEADER_SIZE = (20+8); // IP = 20, UDP = 8

#define MAX_ROUTABLE_PAYLOAD		1200	// x360 requires <= 1260

#if (MAX_ROUTABLE_PAYLOAD & 3) != 0
#error Bit buffers must be a multiple of 4 bytes
#endif

inline constexpr auto MIN_ROUTABLE_PAYLOAD = 16; // minimum playload size

inline constexpr auto NETMSG_TYPE_BITS = 8; // must be 2^NETMSG_TYPE_BITS > SVC_LASTMSG

// This is the payload plus any header info (excluding UDP header)

inline constexpr auto HEADER_BYTES = 9; // 2*4 bytes seqnr, 1 byte flags

// Pad this to next higher 16 byte boundary
// This is the largest packet that can come in/out over the wire, before processing the header
//  bytes will be stripped by the networking channel layer
inline constexpr auto NET_MAX_MESSAGE = PAD_NUMBER( ( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 );

inline constexpr auto NET_HEADER_FLAG_SPLITPACKET = -2;
inline constexpr auto NET_HEADER_FLAG_COMPRESSEDPACKET = -3;

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class INetworkMessageHandler;
class INetworkMessage;
class INetChannel;
class INetworkMessageFactory;
typedef struct netadr_s netadr_t;
class CNetPacket;
