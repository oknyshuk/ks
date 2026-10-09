#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// protocol.h -- communications protocols

inline constexpr const char *INSTANCE_BASELINE_TABLENAME = "instancebaseline";
inline constexpr const char *LIGHT_STYLES_TABLENAME = "lightstyles";
inline constexpr const char *USER_INFO_TABLENAME = "userinfo";
#define SERVER_STARTUP_DATA_TABLENAME	"server_query_info"	// the name is a remnant...
inline constexpr const char *DYNAMIC_MODEL_TABLENAME = "dynamicmodel";


//#define CURRENT_PROTOCOL    1


inline constexpr auto DELTA_OFFSET_BITS = 5;
inline constexpr auto DELTA_OFFSET_MAX = ( ( 1 << DELTA_OFFSET_BITS ) - 1 );

inline constexpr auto DELTASIZE_BITS = 20; // must be: 2^DELTASIZE_BITS > (NET_MAX_PAYLOAD * 8)

// Largest # of commands to send in a packet
inline constexpr auto NUM_NEW_COMMAND_BITS = 4;
inline constexpr auto MAX_NEW_COMMANDS = ((1 << NUM_NEW_COMMAND_BITS)-1);

// Max number of history commands to send ( 2 by default ) in case of dropped packets
inline constexpr auto NUM_BACKUP_COMMAND_BITS = 3;
inline constexpr auto MAX_BACKUP_COMMANDS = ((1 << NUM_BACKUP_COMMAND_BITS)-1);


inline constexpr auto PROTOCOL_AUTHCERTIFICATE = 0x01; // Connection from client is using a WON authenticated certificate
inline constexpr auto PROTOCOL_HASHEDCDKEY = 0x02; // Connection from client is using hashed CD key because WON comm. channel was unreachable
inline constexpr auto PROTOCOL_LASTVALID = 0x02; // Last valid protocol

inline constexpr auto CONNECTIONLESS_HEADER = 0xFFFFFFFF; // all OOB packet start with this sequence
inline constexpr auto AUTH_KEYSIZE = 2048; // max size needed to contain an authentication key (both server and client)

// each channel packet has 1 byte of FLAG bits
inline constexpr auto PACKET_FLAG_RELIABLE = (1<<0); // packet contains subchannel stream data
inline constexpr auto PACKET_FLAG_COMPRESSED = (1<<1); // packet is compressed
inline constexpr auto PACKET_FLAG_ENCRYPTED = (1<<2); // packet is encrypted
inline constexpr auto PACKET_FLAG_SPLIT = (1<<3); // packet is split
inline constexpr auto PACKET_FLAG_CHOKED = (1<<4); // packet was choked by sender

// NOTE:  Bits 5, 6, and 7 are used to specify the # of padding bits at the end of the packet!!!
#define ENCODE_PAD_BITS( x ) ( ( x << 5 ) & 0xff )
#define DECODE_PAD_BITS( x ) ( ( x >> 5 ) & 0xff )

//
// client to server
//

inline constexpr auto RES_FATALIFMISSING = (1<<0); // Disconnect if we can't get this file.
inline constexpr auto RES_PRELOAD = (1<<1); // Load on client rather than just reserving name

// Some day we may want to integrate Zoid's CL 1295766 - Rewrite of Source networking to be protobuf based.
