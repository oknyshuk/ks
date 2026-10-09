#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// net.h -- Half-Life's interface to the networking layer
// For banning IP addresses (or allowing private games)

import ks.tier1;
#include "common.h"
import std;
#include "inetchannel.h"
#include "networksystem/inetworksystem.h"

// Flow control bytes per second limits:
// 16,000 bytes per second = 128kbps
// default rate: 192kbytes per second = 1.5mbps
// 0.75Mbytes per second = 6mbps
inline constexpr auto MIN_RATE = 16000;
inline constexpr auto DEFAULT_RATE = 196608;
inline constexpr auto MAX_RATE = 786432;

inline constexpr auto SIGNON_TIME_OUT = 300.0f; // signon disconnect timeout

inline constexpr auto FRAGMENT_BITS = 8;
inline constexpr auto FRAGMENT_SIZE = (1<<FRAGMENT_BITS);
inline constexpr auto MAX_FILE_SIZE_BITS = 26;
inline constexpr auto MAX_FILE_SIZE = ((1<<MAX_FILE_SIZE_BITS)-1); // maximum transferable size is	64MB

// 0 == regular, 1 == file stream
inline constexpr auto MAX_STREAMS = 2;

inline constexpr auto FRAG_NORMAL_STREAM = 0;
inline constexpr auto FRAG_FILE_STREAM = 1;

inline constexpr auto TCP_CONNECT_TIMEOUT = 4.0f;
inline constexpr auto PORT_ANY = -1;
inline constexpr auto PORT_TRY_MAX = 10; // the number of different ports to try to find an unused one
inline constexpr auto PORT_TRY_MAX_FORKED = 150; // the number to try when we are running as a forked child process (for the server farm)


inline constexpr auto TCP_MAX_ACCEPTS = 8;

inline constexpr auto LOOPBACK_SOCKETS = 2;

inline constexpr auto STREAM_CMD_NONE = 0; // waiting for next blob
inline constexpr auto STREAM_CMD_AUTH = 1; // first command, send back challengenr
inline constexpr auto STREAM_CMD_DATA = 2; // receiving a data blob
inline constexpr auto STREAM_CMD_FILE = 3; // receiving a file blob
inline constexpr auto STREAM_CMD_ACKN = 4; // acknowledged a recveived blob

// NETWORKING INFO

class INetChannel;

enum ESocketIndex_t
{
	NS_INVALID = -1,

	NS_CLIENT = 0,	// client socket
	NS_SERVER,	// server socket
	NS_HLTV,
	NS_HLTV1, // Note: NS_HLTV1 must follow NS_HLTV, NS_HLTV2 must follow NS_HLTV1, etc.
	MAX_SOCKETS
};

extern	netadr_t	net_local_adr;
extern	double		net_time;

class INetChannelHandler;
class IConnectionlessPacketHandler;

// Start up networking
void		NET_Init( bool bDedicated );


// initialize queued packet sender. must do after fork(), not before
void NET_InitPostFork( void );


// Shut down networking
void		NET_Shutdown (void);
// Read any incoming packets, dispatch to known netchannels and call handler for connectionless packets
void		NET_ProcessSocket( int sock, IConnectionlessPacketHandler * handler );
// Set a port to listen mode
void		NET_ListenSocket( int sock, bool listen );
// Send connectionsless string over the wire
void		NET_OutOfBandPrintf(int sock, const ns_address &adr, const char *format, ...) [[gnu::format(printf, 3, 4)]];
void		NET_OutOfBandDelayedPrintf(int sock, const ns_address &adr, uint32 unMillisecondsDelay, const char *format, ...) [[gnu::format(printf, 4, 5)]];
// Send a raw packet, connectionless must be provided (chan can be NULL)
int			NET_SendPacket ( INetChannel *chan, int sock,  const ns_address &to, const  unsigned char *data, int length, bf_write *pVoicePayload = nullptr, bool bUseCompression = false, uint32 unMillisecondsDelay = 0u );
// Called periodically to maybe send any queued packets (up to 4 per frame)
void		NET_SendQueuedPackets();
// Start set current network configuration
void		NET_SetMultiplayer(bool multiplayer);
// Set net_time
void		NET_SetTime( double realtime );
// RunFrame must be called each system frame before reading/sending on any socket
void		NET_RunFrame( double realtime );
// Check configuration state
bool		NET_IsMultiplayer( void );
bool		NET_IsDedicated( void );
// Writes a error file with bad packet content
void		NET_LogBadPacket(netpacket_t * packet);

// bForceNew (used for bots) tells it not to share INetChannels (bots will crash when disconnecting if they
// share an INetChannel).
INetChannel	*NET_CreateNetChannel( int socketnumber, const ns_address *adr, const char * name, INetChannelHandler * handler, const byte *pbEncryptionKey, bool bForceNew );
void		NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan);
void		NET_PrintChannelStatus( INetChannel * chan );

void		NET_WriteStringCmd( const char * cmd, bf_write *buf );

// Address conversion
bool		NET_StringToAdr ( const char *s, netadr_t *a);
// Convert from host to network byte ordering
unsigned short NET_HostToNetShort( unsigned short us_in );
// and vice versa
unsigned short NET_NetToHostShort( unsigned short us_in );

// Find out what port is mapped to a local socket
unsigned short NET_GetUDPPort(int socket);

// add/remove extra sockets for testing
int NET_AddExtraSocket( int port );
void NET_RemoveAllExtraSockets();

const char *NET_ErrorString( ks::platform::socket_error error ); // translate a socket error into a friendly string

// Returns true if compression succeeded, false otherwise
bool NET_BufferToBufferCompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen );
bool NET_BufferToBufferDecompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen );

void NET_TerminateConnection(int sock, const ns_address &peer );

void NET_SleepUntilMessages( int nMilliseconds );

// Returns the address from the net_public_adr convar if it is set
// Returns false if not able to deduce address
bool NET_GetPublicAdr( netadr_t &adr );

//============================================================================
//
// Encrypted network channel communication support
//

inline constexpr auto NET_CRYPT_KEY_LENGTH = 16;
bool NET_CryptVerifyServerCertificateAndAllocateSessionKey( bool bOfficial, const ns_address &from,
	const byte *pchKeyPub, int numKeyPub,
	const byte *pchKeySgn, int numKeySgn,
	byte **pbAllocatedKey, int *pnAllocatedCryptoBlockSize );
bool NET_CryptVerifyClientSessionKey( bool bOfficial,
	const byte *pchKeyPri, int numKeyPri,
	const byte *pbEncryptedKey, int numEncryptedBytes,
	byte *pbPlainKey, int numPlainKeyBytes );

enum ENetworkCertificate_t
{
	k_ENetworkCertificate_PublicKey,
	k_ENetworkCertificate_PrivateKey,
	k_ENetworkCertificate_Signature,
	k_ENetworkCertificate_Max
};
bool NET_CryptGetNetworkCertificate( ENetworkCertificate_t eType, const byte **pbData, int *pnumBytes );

//============================================================================

// Message data
typedef struct
{
	// Size of message sent/received
	int		size;
	// Time that message was sent/received
	float	time;
} flowstats_t;

void NET_SocketPumpClose( ks::platform::socket *s );
int NET_SocketPumpRecvFrom( ks::platform::socket *s, char *buf, int len, ns_address *from );

// Some hackery to avoid using va() in constructor since we cache off the pointer to the string in the ConVar!!!
