// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// net_ws.c
// Windows IP Support layer.

import std;
import ks.tier1;
#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"
#include "matchmaking/imatchframework.h"
#include "ienginetoolinternal.h"
#include "server.h"
#include "mathlib/IceKey.H"
#include "engine/inetsupport.h"


static constexpr auto kHostPort = ks::IntToString<PORT_SERVER>();
static constexpr auto kClientPort = ks::IntToString<PORT_CLIENT>();
static constexpr auto kHltvPort = ks::IntToString<PORT_HLTV>();
static constexpr auto kHltvPort1 = ks::IntToString<PORT_HLTV1>();
constexpr auto NET_COMPRESSION_STACKBUF_SIZE = 4096;

static ConVar net_showsplits( "net_showsplits", "0", FCVAR_RELEASE, "Show info about packet splits" );

static ConVar net_splitrate( "net_splitrate", "1", FCVAR_RELEASE, "Number of fragments for a splitpacket that can be sent per frame" );

static ConVar ipname        ( "ip", "localhost", FCVAR_RELEASE, "Overrides IP for multihomed hosts" );
static ConVar ipname_tv     ( "ip_tv", "", FCVAR_RELEASE, "Overrides IP used to bind TV port for multihomed hosts" );
static ConVar ipname_tv1    ( "ip_tv1", "", FCVAR_RELEASE, "Overrides IP used to bind TV1 port for multihomed hosts" );
static ConVar ipname_relay	( "ip_relay", "", FCVAR_RELEASE, "Overrides IP used to redirect TV relay connections for NAT hosts" );
static ConVar hostport      ( "hostport", kHostPort.c_str() , FCVAR_RELEASE, "Host game server port" );
static ConVar hostip		( "hostip", "", FCVAR_RELEASE, "Host game server ip" );
static ConVar net_public_adr( "net_public_adr", "", FCVAR_RELEASE, "For servers behind NAT/DHCP meant to be exposed to the public internet, this is the public facing ip address string: (\"x.x.x.x\" )" );

static ConVar clientport    ( "clientport", kClientPort.c_str(), FCVAR_RELEASE, "Host game client port" );
static ConVar hltvport		( "tv_port", kHltvPort.c_str(), FCVAR_RELEASE, "Host GOTV[0] port" );
static ConVar hltvport1		( "tv_port1", kHltvPort1.c_str(), FCVAR_RELEASE, "Host GOTV[1] port" );

static ConVar fakelag		( "net_fakelag", "0", FCVAR_CHEAT, "Lag all incoming network data (including loopback) by this many milliseconds." );
static ConVar fakeloss		( "net_fakeloss", "0", FCVAR_CHEAT, "Simulate packet loss as a percentage (negative means drop 1/n packets)" ); 
static ConVar droppackets	( "net_droppackets", "0", FCVAR_CHEAT, "Drops next n packets on client" ); 
static ConVar fakejitter	( "net_fakejitter", "0", FCVAR_CHEAT, "Jitter fakelag packet time" );

static ConVar net_compressvoice( "net_compressvoice", "0", 0, "Attempt to compress out of band voice payloads (360 only)." );
ConVar net_usesocketsforloopback( "net_usesocketsforloopback", "0",
#ifdef _DEBUG
	FCVAR_RELEASE
#else
	0
#endif
	, "Use network sockets layer even for listen server local player's packets (multiplayer only)." );

static ConVar voice_verbose( "voice_verbose", "0", FCVAR_DEVELOPMENTONLY, "Turns on debug output with detailed spew about voice data processing." );

static ConVar voice_xsend_debug( "voice_xsend_debug", "0" );

#ifdef _DEBUG
static ConVar fakenoise		( "net_fakenoise", "0", FCVAR_CHEAT, "Simulate corrupt network packets (changes n bits per packet randomly)" ); 
static ConVar fakeshuffle	( "net_fakeshuffle", "0", FCVAR_CHEAT, "Shuffles order of every nth packet (needs net_fakelag)" ); 
static ConVar recvpackets	( "net_recvpackets", "-1", FCVAR_CHEAT, "Receive exactly next n packets if >= 0" ); 
static ConVar	net_savelargesplits( "net_savelargesplits", "-1", 0, "If not -1, then if a split has this many or more split parts, save the entire packet to disc for analysis." );
#endif


//-----------------------------------------------------------------------------
// Toggle Xbox 360 network security to allow cross-platform testing
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline constexpr auto X360SecureNetwork() { return false; }

extern ConVar net_showudp;
extern ConVar net_showudp_oob;
extern ConVar net_showudp_remoteonly;
extern ConVar net_showtcp;
extern ConVar net_blocksize;
extern int host_framecount;

extern bool ShouldChecksumPackets();
extern unsigned short BufferToShortChecksum( const void *pvData, std::size_t nLength );
extern void NET_InitParanoidMode();
void NET_ClearQueuedPacketsForChannel( INetChannel *chan );

constexpr auto DEF_LOOPBACK_SIZE = 2048;

typedef struct
{
	int			nPort;		// UDP/TCP use same port number
	bool		bListening;	// true if TCP port is listening
	ks::platform::socket *hUDP;		// handle to UDP socket
	ks::platform::socket *hTCP;		// handle to TCP socket
} netsocket_t;

typedef struct 
{
	ks::platform::socket *newsock;	// handle of new socket
	int				netsock;	// handle of listen socket
	float			time;
	netadr_t		addr;
} pendingsocket_t;


struct loopback_t : public ks::pooled< loopback_t >
{
	char		*data;		// loopback buffer
	int			datalen;	// current data length
	char		defbuffer[ DEF_LOOPBACK_SIZE ];

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< loopback_t >;
};

ks::fixed_pool loopback_t::s_Allocator( sizeof(loopback_t), 2, ks::fixed_pool::grow::slow, "loopback_t pool", alignof(loopback_t) );

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	int		nExpectedSplitSize;
	char	buffer[ NET_MAX_MESSAGE ];	// This has to be big enough to hold the largest message
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	int		packetID : 16;
	int		nSplitSize : 16;
} SPLITPACKET;
#pragma pack()

constexpr auto MIN_USER_MAXROUTABLE_SIZE = 576; // ( X.25 Networks )


inline constexpr auto MAX_SPLIT_SIZE = (MAX_ROUTABLE_PAYLOAD - sizeof( SPLITPACKET ));
#define MIN_SPLIT_SIZE	(MIN_USER_MAXROUTABLE_SIZE - sizeof( SPLITPACKET ))

static ConVar sv_maxroutable
	( 
	"sv_maxroutable", 
	"1200", 
	0, 
	"Server upper bound on net_maxroutable that a client can use.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_ROUTABLE_PAYLOAD 
	);

ConVar net_maxroutable
	( 
	"net_maxroutable", 
	"1200", 
	FCVAR_ARCHIVE | FCVAR_USERINFO, 
	"Requested max packet size before packets are 'split'.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_ROUTABLE_PAYLOAD 
	);

netadr_t	net_local_adr;
double		net_time = 0.0f;	// current time, updated each frame

static	std::vector< netsocket_t > net_sockets;	// the sockets
static	std::vector< netpacket_t >	net_packets;

static	bool net_multiplayer = false;	// if true, configured for Multiplayer
static	bool net_noip = false;	// Disable IP support, can't switch to MP mode
static	bool net_nodns = false;	// Disable DNS request to avoid long timeouts
bool net_notcp = true;	// Disable TCP support
static	bool net_nohltv = false; // disable HLTV support
static	bool net_addhltv1 = false; // by default, HLTV1 (sup)port is disabled. Must be enabled explicitly with -addhltv1 cmdline parameter
static	bool net_dedicated = false;	// true is dedicated system
static	ks::platform::socket_error net_error = ks::platform::socket_error::none;			// global error code updated with NET_GetLastError()


std::atomic<int> g_NetChannelsRefreshCounter{ 0 };
template < class V > struct LockedVector : V, std::recursive_mutex {};	// lock()/unlock() guard the vector
static LockedVector< std::vector< CNetChan* > >		s_NetChannels;
static LockedVector< std::vector< pendingsocket_t > >	s_PendingSockets;

CTSQueue<loopback_t *> s_LoopBacks[LOOPBACK_SOCKETS];
static netpacket_t*	s_pLagData[MAX_SOCKETS];  // List of lag structures, if fakelag is set.

//-----------------------------------------------------------------------------
// Purpose: parses "host[:port]" into a seam address
//-----------------------------------------------------------------------------
bool NET_StringToAddress( const char *s, ks::platform::address *address )
{
	char	copy[128];
	address->ip = 0;
	address->port = 0;

	V_strncpy (copy, s, sizeof( copy ) );
	// strip off a trailing :port if present
	for (char *colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			address->port = (unsigned short)std::atoi(colon+1);
		}
	}

	bool bNumeric = copy[0] >= '0' && copy[0] <= '9' && V_strstr( copy, "." );
	if ( !bNumeric && net_nodns )
		return false;	// DNS names disabled

	return ks::platform::resolve_host( copy, &address->ip );
}

int NET_GetLastError( void )
{
	net_error = ks::platform::last_socket_error();
	return (int)net_error;
}

/*
==================
NET_ClearLaggedList

==================
*/
void NET_ClearLaggedList(netpacket_t **pList)
{
	netpacket_t * p = (*pList);

	while ( p )
	{
		netpacket_t * n = p->pNext;

		if ( p->data )
		{
			delete[] p->data;
			p->data = nullptr;
		}
		delete p;
		p = n;
	}

	(*pList) = nullptr;
}

void NET_ClearLagData( int sock )
{
	if ( sock < MAX_SOCKETS && s_pLagData[sock] )
	{
		NET_ClearLaggedList( &s_pLagData[sock] );
	}
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
bool NET_StringToAdr ( const char *s, netadr_t *a)
{
	return a->SetFromString( s, !net_nodns );
}

void NET_FindAllNetChannelAddresses( int socket, std::vector< ks::platform::address > &arrNetChans )
{
	std::scoped_lock lock( s_NetChannels );

	int numChannels = std::ssize( s_NetChannels );

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan * chan = s_NetChannels[ i ];

		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// only ip based ones
		if ( chan->GetRemoteAddress().m_AddrType != NSAT_NETADR )
		{
			continue;
		}

		// and the IP:Port address 
		arrNetChans.push_back( chan->GetRemoteAddress().m_adr.ToAddress() );
	}
}

CNetChan *NET_FindNetChannel(int socket, const ns_address &adr )
{
	std::scoped_lock lock( s_NetChannels );

	int numChannels = std::ssize( s_NetChannels );

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan * chan = s_NetChannels[i];

		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// and the address 
		if ( adr == chan->GetRemoteAddress() )
		{
			return chan;	// found it
		}
	}

	return nullptr;	// no channel found
}

void NET_CloseSocket( ks::platform::socket *hSocket, int sock = -1)
{
	if ( !hSocket )
		return;

	// close socket handle
	// bugbug - shouldn't this clear net_sockets[sock].hUDP?
	ks::platform::close_socket( hSocket );

	// if hSocket mapped to hTCP, clear hTCP
	if ( sock >= 0 )
	{
		if ( net_sockets[sock].hTCP == hSocket )
		{
			net_sockets[sock].hTCP = nullptr;
			net_sockets[sock].bListening = false;
		}
	}

	NET_SocketPumpClose( hSocket );
}

/*
====================
NET_IPSocket
====================
*/
ks::platform::socket *NET_OpenSocket ( const char *net_interface, int& port, int protocol )
{
	ks::platform::address	address{};
	ks::platform::socket_error error = ks::platform::socket_error::none;
	ks::platform::socket *newsocket = nullptr;

	if ( protocol == IPPROTO_TCP )
	{
		newsocket = ks::platform::open_tcp( &error );
	}
	else // as UDP or VDP
	{
		newsocket = ks::platform::open_udp( &error );
	}

	if ( newsocket == nullptr )
	{
		NET_GetLastError(); 
		if ( net_error != ks::platform::socket_error::unsupported )
			Msg ("WARNING: NET_OpenSocket: socket failed: %s", NET_ErrorString(net_error));

		return nullptr;
	}

	if ( !ks::platform::set_nonblocking( newsocket, nullptr ) )
	{
		NET_GetLastError();
		Msg ("WARNING: NET_OpenSocket: set_nonblocking: %s\n", NET_ErrorString(net_error) );
	}
	
	if ( protocol == IPPROTO_TCP )
	{
		// set TCP options: keep TCP connection alive
		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::keep_alive, 1, nullptr ) )
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_KEEPALIVE: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}

		// set TCP options: Does not block close waiting for unsent data to be sent
		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::linger_off, 0, nullptr ) )
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_LINGER: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}

		// set TCP options: Disables the Nagle algorithm for send coalescing.
		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::no_delay, 1, nullptr ) )
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt TCP_NODELAY: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}
	}

	// Set the send and receive buffer sizes for TCP sockets, and UDP sockets on Windows. Windows UDP
	// sockets default to 8 KB buffers which makes dataloss very common. Linux (Ubuntu 12.04 anyway)
	// UDP sockets default to 208 KB so there is no need to change the setting.
	// Use net_usesocketsforloopback 1 to use sockets for listen servers for testing.
	if ( protocol == IPPROTO_TCP )
	{
		const int UDP_BUFFER_SIZE = 128 * 1024; // Better than 8 KB.
		const int bufferSize = ( protocol == IPPROTO_TCP ) ? NET_MAX_MESSAGE : UDP_BUFFER_SIZE;
		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::send_buffer, bufferSize, nullptr ) )
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_SNDBUF: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}

		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::receive_buffer, bufferSize, nullptr ) )
		{
			NET_GetLastError();		
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_RCVBUF: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}
	}

	if ( protocol == IPPROTO_TCP )
	{
		return newsocket;	// don't bind TCP sockets by default
	}

	// Set the socket to allow broadcasts
	if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::broadcast, 1, nullptr ) )
	{
		NET_GetLastError();		
		Msg ("WARNING: NET_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString(net_error));
		return nullptr;
	}

	if ( CommandLine()->FindParm( "-reuse" ) )
	{
		if ( !ks::platform::set_socket_option( newsocket, ks::platform::socket_option::reuse_address, 1, nullptr ) )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_OpenSocket: setsockopt SO_REUSEADDR: %s\n", NET_ErrorString(net_error));
			return nullptr;
		}
	}

	if (!net_interface || !net_interface[0] || !V_strcmp(net_interface, "localhost"))
	{
		address.ip = 0;
	}
	else
	{
		NET_StringToAddress (net_interface, &address);
	}

	int port_offset;	// try binding socket to port, try next 10 is port is already used

	int nNumTries = PORT_TRY_MAX;
	if ( IsChildProcess() )
		nNumTries = PORT_TRY_MAX_FORKED;

	// Add support for "+net_port_try" argument to override on command line
	int nCommandLineOverrideNumTries = nNumTries;
	nCommandLineOverrideNumTries = CommandLine()->ParmValue( "+net_port_try", nCommandLineOverrideNumTries );
	nCommandLineOverrideNumTries = CommandLine()->ParmValue( "-net_port_try", nCommandLineOverrideNumTries );
	if ( ( nCommandLineOverrideNumTries > 0 ) && ( nCommandLineOverrideNumTries < nNumTries ) )
		nNumTries = nCommandLineOverrideNumTries;

	for ( port_offset = 0; port_offset < nNumTries; port_offset++ )
	{
		if ( port == PORT_ANY )
		{
			address.port = 0;	// = INADDR_ANY
		}
		else
		{
			address.port = (unsigned short)( port + port_offset );
		}

		if ( ks::platform::bind( newsocket, address, &error ) )
		{
			if ( port != PORT_ANY && port_offset != 0 )
			{
				port += port_offset;	// update port
				ConDMsg( "Socket bound to non-default port %i because original port was already in use.\n", port );
			}
			break;
		}

		NET_GetLastError();

		if ( port == PORT_ANY || net_error != ks::platform::socket_error::address_in_use )
		{
			Msg ("WARNING: NET_OpenSocket: bind: %s\n", NET_ErrorString(net_error));
			NET_CloseSocket(newsocket,-1);
			return nullptr;
		}

		// Try next port
	}

	if ( port_offset == nNumTries )
	{
		Msg( "WARNING: UDP_OpenSocket: unable to bind socket\n" );
		NET_CloseSocket( newsocket,-1 );
		return nullptr;
	}
	
	return newsocket;
}

ks::platform::socket *NET_ConnectSocket( int sock, const netadr_t &addr )
{
	contract_assert( (sock >= 0) && (sock < std::ssize( net_sockets )) );

	netsocket_t *netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( net_notcp )
		return nullptr;

	int anyport = PORT_ANY;

	netsock->hTCP = NET_OpenSocket( ipname.GetString(), anyport, true );

	if ( !netsock->hTCP )
	{
		Msg( "Warning! NET_ConnectSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
		return nullptr;
	}

	if ( !ks::platform::connect( netsock->hTCP, addr.ToAddress(), nullptr ) )
	{
		NET_GetLastError();

		if ( net_error != ks::platform::socket_error::again )
		{
			Msg ("NET_ConnectSocket: %s\n", NET_ErrorString( net_error ) );
			return nullptr;
		}
	}

	return net_sockets[sock].hTCP;
}

int NET_SendStream( ks::platform::socket *nSock, const char * buf, int len, int flags )
{
	int ret = (int)ks::platform::send_bytes( nSock, buf, len, nullptr );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == ks::platform::socket_error::again )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_SendStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

int NET_ReceiveStream( ks::platform::socket *nSock, char * buf, int len, int flags )
{
	int ret = (int)ks::platform::receive_bytes( nSock, buf, len, nullptr );
	if ( ret == -1 )
	{
		NET_GetLastError();

		if ( net_error == ks::platform::socket_error::again || 
			 net_error == ks::platform::socket_error::not_connected )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_ReceiveStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

INetChannel *NET_CreateNetChannel( int socket, const ns_address *adr, const char * name, INetChannelHandler * handler, const byte *pbEncryptionKey, bool bForceNewChannel )
{
	CNetChan *chan = nullptr;

	if ( !bForceNewChannel && adr != nullptr )
	{
		// try to find real network channel if already existing
		if ( ( chan = NET_FindNetChannel( socket, *adr ) ) != nullptr )
		{
			// channel already known, clear any old stuff before Setup wipes all
			chan->Clear();
		}
	}

	if ( !chan )
	{
		// create new channel
		chan = new CNetChan();

		std::scoped_lock lock( s_NetChannels );
		s_NetChannels.push_back( chan );
	}

	NET_ClearLagData( socket );

	// just reset and return
	ns_address adrToUse;
	if ( adr )
		adrToUse = *adr;
	else
		adrToUse.Clear();
	chan->Setup( socket, adrToUse, name, handler, pbEncryptionKey );

	++ g_NetChannelsRefreshCounter;

	return chan;
}

void NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan)
{
	if ( !netchan )
	{
		return;
	}

	std::scoped_lock lock( s_NetChannels );
	if ( ks::find_index( s_NetChannels, static_cast<CNetChan*>(netchan) ) == -1 )
	{
		DevMsg(1, "NET_CloseNetChannel: unknown channel.\n");
		return;
	}

	ks::erase_first( s_NetChannels, static_cast<CNetChan*>(netchan) );

	NET_ClearQueuedPacketsForChannel( netchan );
	
	if ( bDeleteNetChan )
		delete netchan;

	++ g_NetChannelsRefreshCounter;
}


/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


void NET_SendLoopPacket (int sock, int length, const unsigned char *data )
{
	// Never loop on anything other than client/server
	if ( sock != NS_CLIENT && sock != NS_SERVER )
		return;

	loopback_t	*loop;

	if ( length > NET_MAX_PAYLOAD )
	{
		DevMsg( "NET_SendLoopPacket:  packet too big (%i).\n", length );
		return;
	}

	loop = new loopback_t;

	if ( length <= DEF_LOOPBACK_SIZE )
	{
		loop->data = loop->defbuffer;
	}
	else
	{
		loop->data  = new char[ length ];
	}

	V_memcpy (loop->data, data, length);
	loop->datalen = length;

	if ( sock == NS_SERVER )
	{
		s_LoopBacks[NS_CLIENT].PushItem( loop );
	}
	else if ( sock == NS_CLIENT )
	{
		s_LoopBacks[NS_SERVER].PushItem( loop );
	}
	else
	{
		DevMsg( "NET_SendLoopPacket:  invalid socket (%i).\n", sock );
		return;
	}
}

//=============================================================================

int NET_CountLaggedList( netpacket_t *pList )
{
	int c = 0;
	netpacket_t *p = pList;
	
	while ( p )
	{
		c++;
		p = p->pNext;
	}

	return c;
}

/*
===================
NET_AddToLagged

===================
*/
void NET_AddToLagged( netpacket_t **pList, netpacket_t *pPacket )
{
	if ( pPacket->pNext )
	{
		Msg("NET_AddToLagged::Packet already linked\n");
		return;
	}

	// first copy packet

	netpacket_t *newPacket = new netpacket_t;

	(*newPacket) = (*pPacket);  // copy packet infos
	newPacket->data = new unsigned char[ pPacket->size ];	// create new data buffer
	V_memcpy( newPacket->data, pPacket->data, pPacket->size ); // copy packet data
	newPacket->pNext = nullptr;

	// if list is empty, this is our first element
	if ( (*pList) == nullptr )
	{
		(*pList) = newPacket;	// put packet in top of list
	}
	else
	{
		netpacket_t *last = (*pList);

		while ( last->pNext )
		{
			// got to end of list
			last = last->pNext;
		}

		// add at end
		last->pNext = newPacket;
	}
}

// Actual lag to use in msec
static float s_FakeLag = 0.0;

float NET_GetFakeLag()
{
	return s_FakeLag;
}

// How quickly we converge to a new value for fakelag
constexpr auto FAKELAG_CONVERGE = 200; // ms per second

/*
==============================
NET_AdjustLag

==============================
*/
void NET_AdjustLag( void )
{
	// Already converged?
	if ( fakelag.GetFloat() == s_FakeLag )
		return;

	static double s_LastTime = 0;
	
	// Bound time step
	
	float dt = clamp( net_time - s_LastTime, 0.0f, 0.2f );
	
	s_LastTime = net_time;

	// Figure out how far we have to go
	float diff = fakelag.GetFloat() - s_FakeLag;

	// How much can we converge this frame
	float converge = FAKELAG_CONVERGE * dt;

	// Last step, go the whole way
	if ( converge > fabs( diff ) )
	{
		converge = fabs( diff );
	}

	// Converge toward fakelag.GetFloat()
	if ( diff < 0.0 )
	{
		// Converge toward fakelag.GetFloat()
		s_FakeLag -= converge;
	}
	else
	{
		s_FakeLag += converge;
	}
}


bool NET_LagPacket (bool newdata, netpacket_t * packet)
{
	static int losscount[MAX_SOCKETS];

	if ( packet->source >= MAX_SOCKETS )
		return newdata; // fake lag not supported for extra sockets

	if ( (droppackets.GetInt() > 0)  && newdata && (packet->source == NS_CLIENT) )
	{
		droppackets.SetValue( droppackets.GetInt() - 1 );
		return false;
	}

	if ( fakeloss.GetFloat() && newdata )
	{
		losscount[packet->source]++;

		if ( fakeloss.GetFloat() > 0.0f )
		{
			// Act like we didn't hear anything if we are going to lose the packet.
			// Depends on random # generator.
			if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
				return false;
		}
		else
		{
			int ninterval;

			ninterval = (int)(fabs( fakeloss.GetFloat() ) );
			ninterval = std::max( 2, ninterval );

			if ( !( losscount[packet->source] % ninterval ) )
			{
				return false;
			}
		}
	}

	if (s_FakeLag <= 0.0)
	{
		// Never leave any old msgs around
		for ( int i=0; i<MAX_SOCKETS; i++ )
		{
			NET_ClearLagData( i );
		}
		return newdata;
	}

	// if new packet arrived in fakelag list
	if ( newdata )
	{
		NET_AddToLagged( &s_pLagData[packet->source], packet );
	}

	// Now check the correct list and feed any message that is old enough.
	netpacket_t *p = s_pLagData[packet->source]; // current packet

	if ( !p )
		return false;	// no packet in lag list

	float target = s_FakeLag;
	if ( fakejitter.GetFloat() > 0.0f )
	{
		float maxjitter = std::min( fakejitter.GetFloat(), target * 0.5f );
		target += RandomFloat( -maxjitter, maxjitter );
	}

	if ( (p->received + (target/1000.0f)) > net_time )
		return false;	// not time yet for this packet

#ifdef _DEBUG
	if ( fakeshuffle.GetInt() && p->pNext )
	{
		if ( !RandomInt( 0, fakeshuffle.GetInt() ) )
		{
			// swap p and p->next
			netpacket_t * t = p->pNext;
			p->pNext = t->pNext;
			t->pNext = p;
			p = t; 
		}
	}
#endif
	
	// remove packet p from list (is head)
	s_pLagData[packet->source] = p->pNext;
		
	// copy & adjust content
	packet->source	= p->source;	
	packet->from	= p->from;		
	packet->pNext	= nullptr;			// no next
	packet->received = net_time;	// new time
	packet->size	= p->size;		
	packet->wiresize = p->wiresize;
	packet->stream	= p->stream;
			
	V_memcpy( packet->data, p->data, p->size );

	// free lag packet
					
	delete[] p->data;
	delete p;
			
	return true;
}

// Calculate MAX_SPLITPACKET_SPLITS according to the smallest split size
inline constexpr auto MAX_SPLITPACKET_SPLITS = ( NET_MAX_MESSAGE / MIN_SPLIT_SIZE );
constexpr auto SPLIT_PACKET_STALE_TIME = 15.0f;

class CSplitPacketEntry
{
public:
	CSplitPacketEntry()
	{

		int i;
		for ( i = 0; i < MAX_SPLITPACKET_SPLITS; i++ )
		{
			splitflags[ i ] = -1;
		}

		std::memset( &netsplit, 0, sizeof( netsplit ) );
		lastactivetime = 0.0f;
	}

public:
	ns_address		from;
	int				splitflags[ MAX_SPLITPACKET_SPLITS ];
	LONGPACKET		netsplit;
	// host_time the last time any entry was received for this entry
	float			lastactivetime;
};

typedef std::vector< CSplitPacketEntry > vecSplitPacketEntries_t;
static std::vector< vecSplitPacketEntries_t > net_splitpackets;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void NET_DiscardStaleSplitpackets( const int sock )
{
	if ( !ks::is_valid_index( net_splitpackets, sock ) )
		return;
	
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i;
	for ( i = std::ssize( splitPacketEntries ) - 1; i >= 0; i-- )
	{
		CSplitPacketEntry *entry = &splitPacketEntries[ i ];
		contract_assert( entry );

		if ( net_time < ( entry->lastactivetime + SPLIT_PACKET_STALE_TIME ) )
			continue;

		splitPacketEntries.erase( splitPacketEntries.begin() + i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *from - 
// Output : CSplitPacketEntry
//-----------------------------------------------------------------------------
CSplitPacketEntry *NET_FindOrCreateSplitPacketEntry( const int sock, const ns_address &from )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	int i, count = std::ssize( splitPacketEntries );
	CSplitPacketEntry *entry = nullptr;
	for ( i = 0; i < count; i++ )
	{
		entry = &splitPacketEntries[ i ];
		contract_assert( entry );

		if ( from == entry->from )
			break;
	}

	if ( i >= count )
	{
		CSplitPacketEntry newentry;
		newentry.from = from;

		splitPacketEntries.push_back( newentry );

		entry = &splitPacketEntries[ std::ssize( splitPacketEntries ) - 1 ];
	}

	contract_assert( entry );
	return entry;
}

static const tokenset_t< ESocketIndex_t > s_SocketDescMap[] =
{						 
	{ "cl",		NS_CLIENT		},                          
	{ "sv",		NS_SERVER		},                          
	{ "htv",	NS_HLTV			},
	{ "htv1",	NS_HLTV1			},
	{ nullptr,		(ESocketIndex_t)-1 }
};

static char const *DescribeSocket( int sock )
{
	return s_SocketDescMap->GetNameByToken( ( ESocketIndex_t ) sock, "??" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pData - 
//			size - 
//			*outSize - 
// Output : bool
//-----------------------------------------------------------------------------
bool NET_GetLong( const int sock, netpacket_t *packet )
{
	int				packetNumber, packetCount, sequenceNumber, offset;
	short			packetID;
	SPLITPACKET		*pHeader;
	
	if ( packet->size < sizeof(SPLITPACKET) ) 
	{
		Msg( "Invalid split packet length %i\n", packet->size );
		return false;
	}

	CSplitPacketEntry *entry = NET_FindOrCreateSplitPacketEntry( sock, packet->from );
	contract_assert( entry );
	if ( !entry )
		return false;

	entry->lastactivetime = net_time;
	contract_assert( packet->from.CompareAdr( entry->from ) );

	pHeader = ( SPLITPACKET * )packet->data;
	// pHeader is network endian correct
	sequenceNumber	= ( pHeader->sequenceNumber );
	packetID		= ( (short)pHeader->packetID );
	// High byte is packet number
	packetNumber	= ( packetID >> 8 );	
	// Low byte is number of total packets
	packetCount		= ( packetID & 0xff );	

	int nSplitSizeMinusHeader = (int)( (short)pHeader->nSplitSize );
	if ( nSplitSizeMinusHeader < MIN_SPLIT_SIZE ||
		 nSplitSizeMinusHeader > MAX_SPLIT_SIZE )
	{
		Msg( "NET_GetLong:  Split packet from %s with invalid split size (number %i/ count %i) where size %i is out of valid range [%d - %d ]\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			(int)MIN_SPLIT_SIZE,
			(int)MAX_SPLIT_SIZE );
		return false;
	}

	if ( packetNumber >= MAX_SPLITPACKET_SPLITS ||
		 packetCount > MAX_SPLITPACKET_SPLITS )
	{
		Msg( "NET_GetLong:  Split packet from %s with too many split parts (number %i/ count %i) where %i is max count allowed\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			(int)MAX_SPLITPACKET_SPLITS );
		return false;
	}

	// First packet in split series?
	if ( entry->netsplit.currentSequence == -1 || 
		sequenceNumber != entry->netsplit.currentSequence )
	{
		entry->netsplit.currentSequence	= sequenceNumber;
		entry->netsplit.splitCount		= packetCount;
		entry->netsplit.nExpectedSplitSize = nSplitSizeMinusHeader;
	}

	if ( entry->netsplit.nExpectedSplitSize != nSplitSizeMinusHeader )
	{
		Msg( "NET_GetLong:  Split packet from %s with inconsistent split size (number %i/ count %i) where size %i not equal to initial size of %i\n", 
			ns_address_render( packet->from ).String(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			entry->netsplit.nExpectedSplitSize
			);
		return false;
	}

	int size = packet->size - sizeof(SPLITPACKET);

	if ( entry->splitflags[ packetNumber ] != sequenceNumber )
	{
		// Last packet in sequence? set size
		if ( packetNumber == (packetCount-1) )
		{
			entry->netsplit.totalSize = (packetCount-1) * nSplitSizeMinusHeader + size;
		}

		entry->netsplit.splitCount--;		// Count packet
		entry->splitflags[ packetNumber ] = sequenceNumber;

		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 3 )
		{
			Msg( "<-- [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i from %s\n", 
				DescribeSocket( sock ),
				packetNumber + 1, 
				packetCount, 
				sequenceNumber,
				size, 
				nSplitSizeMinusHeader + (int)sizeof( SPLITPACKET ), 
				ns_address_render( packet->from ).String() );
		}
	}
	else
	{
		Msg( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes ) from %s\n", packetNumber + 1, packetCount, size, ns_address_render( packet->from ).String() );
	}


	// Copy the incoming data to the appropriate place in the buffer
	offset = (packetNumber * nSplitSizeMinusHeader);
	std::memcpy( entry->netsplit.buffer + offset, packet->data + sizeof(SPLITPACKET), size );
	
	// Have we received all of the pieces to the packet?
	if ( entry->netsplit.splitCount <= 0 )
	{
		entry->netsplit.currentSequence = -1;	// Clear packet
		if ( entry->netsplit.totalSize > sizeof(entry->netsplit.buffer) )
		{
			Msg("Split packet too large! %d bytes from %s\n", entry->netsplit.totalSize, ns_address_render( packet->from ).String() );
			return false;
		}

		V_memcpy( packet->data, entry->netsplit.buffer, entry->netsplit.totalSize );
		packet->size = entry->netsplit.totalSize;
		packet->wiresize = entry->netsplit.totalSize;
		return true;
	}

	return false;
}


bool NET_GetLoopPacket ( netpacket_t * packet )
{
	contract_assert( packet );

	loopback_t	*loop = nullptr;

	if ( packet->source > NS_SERVER )
		return false;
		
	if ( !s_LoopBacks[packet->source].PopItem( &loop ) )
	{
		return false;
	}

	if (loop->datalen == 0)
	{
		// no packet in loopback buffer
		delete loop;
		return ( NET_LagPacket( false, packet ) );
	}

	// copy data from loopback buffer to packet 
	packet->from.SetAddrType( NSAT_NETADR );
	packet->from.m_adr.SetType( NA_LOOPBACK );
	packet->size = loop->datalen;
	packet->wiresize = loop->datalen;
	V_memcpy ( packet->data, loop->data, packet->size );
	
	loop->datalen = 0; // buffer is avalibale again

	if ( loop->data != loop->defbuffer )
	{
		delete[] loop->data;
		loop->data = loop->defbuffer;
	}

	delete loop;

	// allow lag system to modify packet
	return ( NET_LagPacket( true, packet ) );	
}

static int NET_ReceiveRawPacket( int sock, void *buf, int len, ns_address *from )
{
	ks::platform::socket *net_socket = net_sockets[ sock ].hUDP;

	int ret = NET_SocketPumpRecvFrom( net_socket, (char*)buf, len, from );
	if ( ret > 0 )
		return ret;

	// nothing
	return 0;
}

static bool NET_ReceiveDatagram_Helper( const int sock, netpacket_t * packet, bool &bNoMorePacketsInSocketPipe )
{
	ks::ScratchArena<> scratch;
	contract_assert( packet );
	contract_assert( net_multiplayer );

#if defined( _DEBUG )
	if ( recvpackets.GetInt() >= 0 )
	{
		ks::platform::socket *net_socket = net_sockets[packet->source].hUDP;
		unsigned long bytes = ks::platform::bytes_available( net_socket );

		if ( bytes <= 0 )
		{
			bNoMorePacketsInSocketPipe = true;
			return false;
		}

		if ( recvpackets.GetInt() == 0 )
		{
			bNoMorePacketsInSocketPipe = true;
			return false;
		}

		recvpackets.SetValue( recvpackets.GetInt() - 1 );
	}
#endif

	int ret = NET_ReceiveRawPacket( sock, packet->data, NET_MAX_MESSAGE, &packet->from );
	bNoMorePacketsInSocketPipe = ( ret <= 0 );
	if ( ret > 0 )
	{
		packet->wiresize = ret;

		ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > bufVoice( NET_COMPRESSION_STACKBUF_SIZE );

		unsigned int nVoiceBits = 0u;


		packet->size = ret;
		

		if ( ret < NET_MAX_MESSAGE )
		{
			// Check for split message
			if ( ( *(int *)packet->data ) == NET_HEADER_FLAG_SPLITPACKET )	
			{
				if ( !NET_GetLong( sock, packet ) )
					return false;
			}

			// Now check if the data on the wire is encrypted?
			ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecryptedAll( NET_COMPRESSION_STACKBUF_SIZE );
			if ( ( *(int *)packet->data ) != CONNECTIONLESS_HEADER )
			{
				// If the channel has encryption then decrypt the packet
				CNetChan * chan = NET_FindNetChannel( sock, packet->from );
				if ( !chan )
					return false;	// this is not an error during connect/disconnect, but non-connectionless packets must have a channel to process anyways

				if ( const unsigned char *pubEncryptionKey = chan->GetChannelEncryptionKey() )
				{
					// Decrypt the packet
					IceKey iceKey( 2 );
					iceKey.set( pubEncryptionKey );

					if ( ( packet->size % iceKey.blockSize() ) == 0 )
					{
						// Decrypt the message
						ks::grow_to( memDecryptedAll, packet->size );
						unsigned char *pchCryptoBuffer = ( unsigned char * ) scratch.AllocBytes( iceKey.blockSize() );
						for ( int k = 0; k < ( int ) packet->size; k += iceKey.blockSize() )
						{
							iceKey.decrypt( ( const unsigned char * ) ( packet->data + k ), pchCryptoBuffer );
							V_memcpy( memDecryptedAll.data() + k, pchCryptoBuffer, iceKey.blockSize() );
						}

						// Check how much random fudge we have
						int numRandomFudgeBytes = *memDecryptedAll.data();
						if ( ( numRandomFudgeBytes > 0 ) && ( int( numRandomFudgeBytes + 1 + sizeof( int32 ) ) < packet->size ) )
						{
							// Fetch the size of the encrypted message
							int32 numBytesWrittenWire = 0;
							V_memcpy( &numBytesWrittenWire, memDecryptedAll.data() + 1 + numRandomFudgeBytes, sizeof( int32 ) );
							int32 const numBytesWritten = DWordSwapC( numBytesWrittenWire );	// byteswap from the wire

							// Make sure the total size of the message matches the expectations
							if ( int( numRandomFudgeBytes + 1 + sizeof( int32 ) +numBytesWritten ) == packet->size )
							{
								// Fix the packet to point at decrypted data!
								packet->size = numBytesWritten;
								V_memcpy( packet->data, memDecryptedAll.data() + 1 + numRandomFudgeBytes + sizeof( int32 ), packet->size );
							}
						}
					}
				}
			}
			
			// Next check for compressed message
			if ( ( *(int *)packet->data) == NET_HEADER_FLAG_COMPRESSEDPACKET )
			{
				byte *pCompressedData = packet->data + sizeof( unsigned int );

				CLZSS lzss;
				// Decompress
				int actualSize = lzss.GetActualSize( pCompressedData );
				if ( actualSize <= 0 || actualSize > NET_MAX_PAYLOAD )
					return false;

				ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecompressed( NET_COMPRESSION_STACKBUF_SIZE );
				ks::grow_to( memDecompressed, actualSize );

				unsigned int uDecompressedSize = lzss.SafeUncompress( pCompressedData, memDecompressed.data(), actualSize );
				if ( uDecompressedSize == 0 || ((unsigned int)actualSize) != uDecompressedSize )
				{
					return false;
				}

				// packet->wiresize is already set
				V_memcpy( packet->data, memDecompressed.data(), uDecompressedSize );

				packet->size = uDecompressedSize;
			}

			if ( nVoiceBits > 0 )
			{
				// 9th byte is flag byte
				byte flagByte = *( (byte *)packet->data + sizeof( unsigned int ) + sizeof( unsigned int ) );
				unsigned int unPacketBits = packet->size << 3;
				int nPadBits = DECODE_PAD_BITS( flagByte );
				unPacketBits -= nPadBits;

				//check the CRC value in the original data packet
				if( ShouldChecksumPackets() )  
				{
					//we still want to honor the old checksum so we need to do it here instead of the usual location in CNetChan::ProcessPacketHeader
					//If the layout of the header ever changes this code will need to be updated.
					int checkSumByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte );
					packet->message.Seek( checkSumByteOffset << 3 );
					int oldChecksum = packet->message.ReadUBitLong( 16 );
					packet->message.Seek(0);

					int rawDataByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte ) + sizeof( unsigned short );
					void *pvData = packet->data + rawDataByteOffset;
					int nCheckSumBytes = packet->size - rawDataByteOffset;
					if ( nCheckSumBytes <= 0 || nCheckSumBytes > NET_MAX_PAYLOAD )
					{
						ConMsg ( "corrupted packet detected (checksumbytes %d)\n", nCheckSumBytes );
						return false;
					}

					unsigned short usDataCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
					if ( usDataCheckSum != oldChecksum )
					{
						ConMsg ( "corrupted packet detected\n" );
						return false;
					}
				}

				// create the combined gamedata + voicedata packet
				bf_write fixup;
				fixup.SetDebugName( "X360 Fixup" );
				fixup.StartWriting( packet->data, NET_MAX_MESSAGE, unPacketBits );
				fixup.WriteBits( bufVoice.data(), nVoiceBits );

				// Make sure we have enough bits to read a final ks::net::net_NOP opcode before compressing 
				int nRemainingBits = fixup.GetNumBitsWritten() % 8;
				if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
				{
					CNETMsg_NOP_t nop;
					nop.WriteToBuffer( fixup );
				}

				packet->size = fixup.GetNumBytesWritten();

				//recompute the new CRC value in the header.
				if( ShouldChecksumPackets() )
				{
					//CNetChan::ProcessPacketHeader will still be looking for the checksum so we need to generate one that will keep it happy.
					int checkSumByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte );
					fixup.SeekToBit( checkSumByteOffset << 3 );//seek to bit position of checksum

					int rawDataByteOffset = sizeof( unsigned int ) + sizeof( unsigned int ) + sizeof( byte ) + sizeof( unsigned short );
					void *pvData = packet->data + rawDataByteOffset;
					int nCheckSumBytes = packet->size - rawDataByteOffset;
					unsigned short newChecksum = BufferToShortChecksum( pvData, nCheckSumBytes );

					fixup.WriteUBitLong( newChecksum, 16 );
				}
			}

			return NET_LagPacket( true, packet );
		}
		else
		{
			ConDMsg ( "NET_ReceiveDatagram:  Oversize packet from %s\n", ns_address_render( packet->from ).String() );
		}
	}
	else if ( ret == -1  )									// error?
	{
		NET_GetLastError();

		switch ( net_error )
		{
		case ks::platform::socket_error::again:
		case ks::platform::socket_error::reset:
		case ks::platform::socket_error::refused:
			break;
		case ks::platform::socket_error::message_too_large:
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		default:
			// Let's continue even after errors
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		}
	}

	return false;
}

#define NET_WS_PACKET_STAT( sock, var )
bool NET_ReceiveDatagram ( const int sock, netpacket_t * packet )
{
	for ( ;; )
	{
		bool bNoMorePacketsInSocketPipe = true;
		bool bFoundGoodPacket = NET_ReceiveDatagram_Helper( sock, packet, bNoMorePacketsInSocketPipe );
		if ( bFoundGoodPacket )
		{
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalGood );
			return true;
		}
		if ( bNoMorePacketsInSocketPipe )
		{
			return false;
		}
		else
		{
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalBad );
			// continue, this was a bad code that in old networking code would cause a packet processing hitch
		}
	}
}

netpacket_t *NET_GetPacket (int sock, byte *scratch )
{
	if ( !ks::is_valid_index( net_packets, sock ) )
		return nullptr;
	
	// Each socket has its own netpacket to allow multithreading
	netpacket_t &inpacket = net_packets[sock];

	NET_AdjustLag();
	NET_DiscardStaleSplitpackets( sock );

	// setup new packet
	inpacket.from.Clear();
	inpacket.received = net_time;
	inpacket.source = sock;	
	inpacket.data = scratch;
	inpacket.size = 0;
	inpacket.wiresize = 0;
	inpacket.pNext = nullptr;
	inpacket.message.SetDebugName("inpacket.message");

	// Check loopback first
	if ( !NET_GetLoopPacket( &inpacket ) )
	{
		if ( !NET_IsMultiplayer() )
		{
			return nullptr;
		}

		// then check UDP data 
		if ( !NET_ReceiveDatagram( sock, &inpacket ) )
		{
			// at last check if the lag system has a packet for us
			if ( !NET_LagPacket (false, &inpacket) )
			{
				return nullptr;	// we don't have any new packet
			}
		}
	}
	
	contract_assert( inpacket.size ); 

#ifdef _DEBUG
	if ( fakenoise.GetInt() > 0 )
	{
		COM_AddNoise( inpacket.data, inpacket.size, fakenoise.GetInt() );
	}
#endif
	
	// prepare bitbuffer for reading packet with new size
	inpacket.message.StartReading( inpacket.data, inpacket.size );

	return &inpacket;
}

void NET_ProcessPending( void )
{
	std::scoped_lock lock( s_PendingSockets );
	for ( int i=0; i<std::ssize( s_PendingSockets );i++ )
	{
		pendingsocket_t * psock = &s_PendingSockets[i];

		char	headerBuf[5] [[gnu::aligned(4)]];

		if ( (net_time - psock->time) > TCP_CONNECT_TIMEOUT )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.erase( s_PendingSockets.begin() + i );
			continue;
		}

		int ret = NET_ReceiveStream( psock->newsock, headerBuf, sizeof(headerBuf), 0 );

		if ( ret == 0 )
		{
			continue;	// nothing received
		}
		else if ( ret == -1 )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.erase( s_PendingSockets.begin() + i );
			continue;	// connection closed somehow
		}
		
		bf_read		header( headerBuf, sizeof(headerBuf) );

		int cmd = header.ReadByte();
		unsigned long challengeNr = header.ReadLong();
		bool bOK = false;	

		if ( cmd == STREAM_CMD_ACKN )
		{
			std::scoped_lock channelsLock( s_NetChannels );
			for ( int j = 0; j < std::ssize( s_NetChannels ); j++ )
			{
				CNetChan * chan = s_NetChannels[j];

				if ( chan->GetSocket() != psock->netsock )
					continue;

				if ( challengeNr == chan->GetChallengeNr() && !chan->m_StreamSocket )
				{
					if ( psock->addr.CompareAdr( chan->remote_address.AsType<netadr_t>(), true ) )
					{
						chan->m_StreamSocket = psock->newsock;
						chan->m_StreamActive = true;
						
						chan->ResetStreaming();

						bOK = true;

						if ( net_showtcp.GetInt() )
						{
							Msg ("TCP <- %s: connection accepted\n", psock->addr.ToString() );
						}
						
						break;
					}
					else
					{
						Msg ("TCP <- %s: IP address mismatch.\n", psock->addr.ToString() );
					}
				}
			}
		}

		if ( !bOK )
		{
			Msg ("TCP <- %s: invalid connection request.\n", psock->addr.ToString() );
			NET_CloseSocket( psock->newsock );
		}

		s_PendingSockets.erase( s_PendingSockets.begin() + i );
	}
}

void NET_ProcessListen(int sock)
{
	netsocket_t * netsock = &net_sockets[sock];
		
	if ( !netsock->bListening )
		return;

	ks::platform::address peer{};
	ks::platform::socket *newSocket = ks::platform::accept_connection( netsock->hTCP, &peer, nullptr );
	if ( newSocket == nullptr )
	{
		NET_GetLastError();

		if ( net_error != ks::platform::socket_error::again )
		{
			ConDMsg ("NET_ThreadListen: %s\n", NET_ErrorString(net_error));
		}
		return;
	}

	// new connection TCP request, put in pending queue

	pendingsocket_t psock;

	psock.newsock = newSocket;
	psock.netsock = sock;
	psock.addr.SetFromAddress( peer );
	psock.time = net_time;

	std::scoped_lock lock( s_PendingSockets );
	s_PendingSockets.push_back( psock );

	// tell client to send challenge number to identify

	char authcmd = STREAM_CMD_AUTH;

	NET_SendStream( newSocket, &authcmd, 1 , 0 );	

	if ( net_showtcp.GetInt() )
	{
		Msg ("TCP <- %s: connection request.\n", psock.addr.ToString() );
	}
}

void NET_ProcessSocket( int sock, IConnectionlessPacketHandler *handler )
{
	class CAutoNetProcessSocketStartEnd
	{
	public:
		CAutoNetProcessSocketStartEnd( int sock ) : m_sock( sock )
		{
			extern void On_NET_ProcessSocket_Start( ks::platform::socket *hUDP, int sock );
			On_NET_ProcessSocket_Start( net_sockets[m_sock].hUDP, m_sock );
			NET_WS_PACKET_STAT( sock, g_nSockUDPTotalProcess );
		}
		~CAutoNetProcessSocketStartEnd()
		{
			extern void On_NET_ProcessSocket_End( ks::platform::socket *hUDP, int sock );
			On_NET_ProcessSocket_End( net_sockets[m_sock].hUDP, m_sock );
		}
	private:
		int m_sock;
	}
	autoThreadSockController( sock );

	netpacket_t * packet;
	
	//Assert ( (sock >= 0) && (sock<net_sockets.Count()) );

	// Scope for the auto_lock
	{
		std::scoped_lock lock( s_NetChannels );

		// get streaming data from channel sockets
		int numChannels = std::ssize( s_NetChannels );

		for ( int i = (numChannels-1); i >= 0 ; i-- )
		{
			CNetChan *netchan = s_NetChannels[i];

			// sockets must match
			if ( sock != netchan->GetSocket() )
				continue;

			if ( !netchan->ProcessStream() )
			{
				netchan->GetMsgHandler()->ConnectionCrashed("TCP connection failed.");
			}
		}
	}

	// now get datagrams from sockets
	net_scratchbuffer_t scratch;
	while ( ( packet = NET_GetPacket ( sock, scratch.GetBuffer() ) ) != nullptr )
	{
		if ( Filter_ShouldDiscard ( packet->from ) )	// filtering is done by network layer
		{
			Filter_SendBan( packet->from );	// tell them we aren't listening...
			continue;
		} 

		// check for connectionless packet (0xffffffff) first
		if ( ( *(unsigned int *)packet->data ) == CONNECTIONLESS_HEADER )
		{
			packet->message.ReadLong();	// read the -1

			if ( net_showudp.GetInt() && net_showudp_oob.GetInt() )
			{
				Msg("UDP <- %s: sz=%d OOB '0x%02X' wire=%d\n", ns_address_render( packet->from ).String(), packet->size, packet->data[4], packet->wiresize );
//				for ( int k = 0; k < packet->size; ++ k )
//					Msg( " %02X", packet->data[k] );
//				Msg( "\n" );
//				for ( int k = 0; k < packet->size; ++ k )
//					Msg( "  %c", (packet->data[k] >= 32 && packet->data[k] < 127) ? packet->data[k] : '*' );
//				Msg( "\n" );
			}

			handler->ProcessConnectionlessPacket( packet );
			continue;
		}

		// check for packets from connected clients
		
		CNetChan * netchan = NET_FindNetChannel( sock, packet->from );

		if ( netchan )
		{
			netchan->ProcessPacket( packet, true );
		}
		/* else	// Not an error that may happen during connect or disconnect
		{
			Msg ("Sequenced packet without connection from %s\n" , ns_address_render( packet->from ).String() );
		}*/
	}
}

void NET_LogBadPacket(netpacket_t * packet)
{
	FileHandle_t fp;
	int i = 0;
	char filename[ MAX_OSPATH ];
	bool done = false;

	while ( i < 1000 && !done )
	{
		V_snprintf( filename, sizeof( filename ), "badpacket%03i.dat", i );
		fp = g_pFileSystem->Open( filename, "rb" );
		if ( !fp )
		{
			fp = g_pFileSystem->Open( filename, "wb" );
			g_pFileSystem->Write( packet->data, packet->size, fp );
			done = true;
		}
		if ( fp )
		{
			g_pFileSystem->Close( fp );
		}
		i++;
	}

	if ( i < 1000 )
	{
		Msg( "Error buffer for %s written to %s\n", ns_address_render( packet->from ).String(), filename );
	}
	else
	{
		Msg( "Couldn't write error buffer, delete error###.dat files to make space\n" );
	}
}

static int NET_SendRawPacket( SOCKET s, const void *buf, int len, const ns_address &to )
{
	switch ( to.m_AddrType )
	{
		case NSAT_NETADR:
			return (int)ks::platform::send_to( s, to.AsType<netadr_t>().ToAddress(), (const char *)buf, len, nullptr );
	}

	Warning( "Attempt to send to unknown address type %d\n", to.m_AddrType );
	contract_assert( false );
	return -1;
}

int NET_SendToImpl( SOCKET s, const char * buf, int len, const ns_address &to, int iGameDataLength )
{
	int nSend = NET_SendRawPacket( s, buf, len, to );

	return nSend;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo();
bool CL_IsPortalDemo();
static int NET_SendTo( bool verbose, SOCKET s, const char * buf, int len, const ns_address &to, int iGameDataLength )
{	
	int nSend = 0;
	
	
	// If it's 0.0.0.0:0, then it's a fake player + sv_stressbots and we've plumbed everything all 
	// the way through here, where we finally bail out.
	if ( to.IsNull() )
	{		
		return len;
	}

	// Don't send anything out in VCR mode.. it just annoys other people testing in multiplayer.
#ifndef DEDICATED
	if ( ( CL_IsHL2Demo() || CL_IsPortalDemo() ) && !net_dedicated )
	{
		Error( "NET_SendTo: Error" );
	}
#endif // !DEDICATED

	nSend = NET_SendToImpl
	( 
		s, 
		buf,
		len,
		to, 
		iGameDataLength 
	);

#if defined( _DEBUG )
	if ( verbose && 
		( nSend > 0 ) && 
		( len > MAX_ROUTABLE_PAYLOAD ) )
	{
		ConDMsg( "NET_SendTo:  Packet length (%i) > (%i) bytes\n", len, MAX_ROUTABLE_PAYLOAD );
	}
#endif
	return nSend;
}

#if defined( _DEBUG )

#include "filesystem.h"
#include "filesystem_engine.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *NET_GetDebugFilename( char const *prefix )
{
	static char filename[ MAX_OSPATH ];

	int i;

	for ( i = 0; i < 10000; i++ )
	{
		V_snprintf( filename, sizeof( filename ), "debug/%s%04i.dat", prefix, i );
		if ( g_pFileSystem->FileExists( filename ) )
			continue;

		return filename;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*buf - 
//			len - 
//-----------------------------------------------------------------------------
void NET_StorePacket( char const *filename, byte const *buf, int len )
{
	FileHandle_t fh;

	g_pFileSystem->CreateDirHierarchy( "debug/", "DEFAULT_WRITE_PATH" );
	fh = g_pFileSystem->Open( filename, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFileSystem->Write( buf, len, fh );
		g_pFileSystem->Close( fh );
	}
}

#endif // _DEBUG

struct SendQueueItem_t
{
	SendQueueItem_t() :
		m_pChannel( nullptr ),
		m_Socket( nullptr )
	{
	}

	CNetChan	*m_pChannel;
	SOCKET		m_Socket;
	std::vector< std::byte >	m_Buffer;
	ns_address	m_To;
};

struct SendQueue_t
{
	SendQueue_t() : 
		m_nHostFrame( 0 )
	{
	}
	int									m_nHostFrame;
	std::deque< SendQueueItem_t >		m_SendQueue;
};

static SendQueue_t g_SendQueue;

static int NET_QueuePacketForSend( CNetChan *chan, bool verbose, SOCKET s, const char *buf, int len, const ns_address &to, uint32 msecDelay )
{
	// If net_queued_packet_thread was -1 at startup, then we don't even have a thread.
	if ( net_queued_packet_thread.GetInt() && g_pQueuedPackedSender->IsRunning() )
	{
		g_pQueuedPackedSender->QueuePacket( chan, s, buf, len, to, msecDelay );
	}
	else
	{
		contract_assert( chan );
		// Set up data structure
		SendQueueItem_t *sq = &g_SendQueue.m_SendQueue.emplace_back();
		sq->m_Socket = s;
		sq->m_pChannel = chan;
		sq->m_Buffer.assign( (const std::byte *)buf, (const std::byte *)buf + len );
		sq->m_To = to;
		sq->m_pChannel->IncrementQueuedPackets();
	}
	
	return len;
}

void NET_SendQueuedPacket( SendQueueItem_t *sq )
{
	// Msg( "Send queued packet %d\n", sq->m_Buffer.size() );
	NET_SendTo
	( 
		false, 
		sq->m_Socket, 
		( const char  * )sq->m_Buffer.data(), 
		sq->m_Buffer.size(), 
		sq->m_To,
		-1
	);

	sq->m_pChannel->DecrementQueuedPackets();
}

void NET_ClearQueuedPacketsForChannel( INetChannel *channel )
{
	std::erase_if( g_SendQueue.m_SendQueue, [channel]( const SendQueueItem_t &e ) { return e.m_pChannel == channel; } );
}

void NET_SendQueuedPackets()
{
	// Only do this once per frame
	if ( host_framecount == g_SendQueue.m_nHostFrame )
		return;
	g_SendQueue.m_nHostFrame = host_framecount;

	std::deque< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	int nRemaining = net_splitrate.GetInt();
	while ( nRemaining )
	{
		if ( !list.empty() )
		{
			NET_SendQueuedPacket( &list.front() );
			list.pop_front();
			--nRemaining;
		}
		else
		{
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
static std::atomic<int32> s_SplitPacketSequenceNumber[ MAX_SOCKETS ] = {1};

static int NET_SendLong( INetChannel *chan, int sock, SOCKET s, const char * buf, int len, const ns_address &to, int nMaxRoutableSize )
{
	CNetChan *netchan = dynamic_cast< CNetChan * >( chan );

	short nSplitSizeMinusHeader = nMaxRoutableSize - sizeof( SPLITPACKET );

	int nSequenceNumber = -1;
	if ( netchan )
	{
		nSequenceNumber = netchan->IncrementSplitPacketSequence();
	}
	else
	{
		nSequenceNumber = ++s_SplitPacketSequenceNumber[ sock ];
	}

	const char *sendbuf = buf;
	int sendlen = len;

	char			packet[ MAX_ROUTABLE_PAYLOAD ];
	SPLITPACKET		*pPacket = (SPLITPACKET *)packet;

	// Make pPacket data network endian correct
	pPacket->netID = ( NET_HEADER_FLAG_SPLITPACKET );
	pPacket->sequenceNumber = ( nSequenceNumber );
	pPacket->nSplitSize = ( nSplitSizeMinusHeader );
	
	int nPacketCount = (sendlen + nSplitSizeMinusHeader - 1) / nSplitSizeMinusHeader;

#if defined( _DEBUG )
	if ( net_savelargesplits.GetInt() != -1 && nPacketCount >= net_savelargesplits.GetInt() )
	{
		char const *filename = NET_GetDebugFilename( "splitpacket" );
		if ( filename )
		{
			Msg( "Saving split packet of %i bytes and %i packets to file %s\n",
				sendlen, nPacketCount, filename );

			NET_StorePacket( filename, (byte const *)sendbuf, sendlen );
		}
		else
		{
			Msg( "Too many files in debug directory, clear out old data!\n" );
		}
	}
#endif

	int nBytesLeft = sendlen;
	int nPacketNumber = 0;
	int nTotalBytesSent = 0;
	int nFragmentsSent = 0;

	while ( nBytesLeft > 0 )
	{
		int size = std::min<std::common_type_t<short int, int>>( nSplitSizeMinusHeader, nBytesLeft );

		pPacket->packetID = ( (short)(( nPacketNumber << 8 ) + nPacketCount) );
		
		V_memcpy( packet + sizeof(SPLITPACKET), sendbuf + (nPacketNumber * nSplitSizeMinusHeader), size );
		
		int ret = 0;

		// Setting net_queued_packet_thread to NET_QUEUED_PACKET_THREAD_DEBUG_VALUE goes into a mode where all packets are queued.. can be used to stress-test it.
		// Linux threads aren't prioritized well enough for this to work well (i.e. the queued packet thread doesn't get enough
		// attention to flush itself well). The behavior the queue fixes is that if you send too many DP packets
		// without giving up your timeslice, it'll just discard the 7th and later packets until you Sleep() (issue might be on client recipient side, need to
		// snif packets to double check)

		if ( netchan && (nFragmentsSent >= net_splitrate.GetInt() || net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE) )
		{
			// Don't let this rate get too high or user's won't be able to receive all of the parts
			// since they'll be too close together.
			// This should use the same rate as is used elsewhere so that we throttle all sends to the
			// same delays. Note that setting the socket's send/receive buffers to a larger size helps
			// to minimize the issues. However unthrottled UDP is still a bad thing.

			float flMaxSplitpacketDataRateBytesPerSecond = (float)netchan->GetDataRate();

			// Calculate the delay (measured from now) for when this packet should be sent.
			uint32 delay = (int)( 1000.0f * ( (float)( nPacketNumber * ( nMaxRoutableSize + UDP_HEADER_SIZE ) ) / flMaxSplitpacketDataRateBytesPerSecond ) + 0.5f );

			ret = NET_QueuePacketForSend( netchan, false, s, packet, size + sizeof(SPLITPACKET), to, delay );
		}
		else
		{
			// Also, we send the first packet no matter what
			// w/o a netchan, if there are too many splits, its possible the packet can't be delivered.  However, this would only apply to out of band stuff like
			//  server query packets, which should never require splitting anyway.
			ret = NET_SendTo( false, s, packet, size + sizeof(SPLITPACKET), to, -1 );
		}

		// First split send
		++nFragmentsSent;

		if ( ret < 0 )
		{
			return ret;
		}

		if ( ret >= size )
		{
			nTotalBytesSent += size;
		}

		nBytesLeft -= size;
		++nPacketNumber;

		// Always bitch about split packets in debug
		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 2 )
		{
			Msg( "--> [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i to %s [ total %4i ]\n",
				DescribeSocket( sock ),
				nPacketNumber, 
				nPacketCount, 
				nSequenceNumber,
				size,
				nMaxRoutableSize,
				ns_address_render( to ).String(),
				sendlen );
		}
	}
	
	return nTotalBytesSent;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			length - 
//			*data - 
//			to - 
// Output : void NET_SendPacket
//-----------------------------------------------------------------------------

int NET_SendPacket ( INetChannel *chan, int sock,  const ns_address &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = NULL */, bool bUseCompression /*=false*/, uint32 unMillisecondsDelay /*=0u*/ )
{
	ks::ScratchArena<> scratch;
	int		ret;

	if ( (*(unsigned int*)data == CONNECTIONLESS_HEADER) && bUseCompression )
	{
		Warning( "[NET] Cannot send compressed connectionless packet to %s '0x%02X'\n", ns_address_render( to ).String(), data[4] );
		contract_assert( 0 );
		return 0;
	}
	if ( ( *( unsigned int* ) data == CONNECTIONLESS_HEADER ) && ( length > MAX_ROUTABLE_PAYLOAD ) )
	{
		Warning( "[NET] Cannot send connectionless packet to %s '0x%02X' exceeding MTU (%u)\n", ns_address_render( to ).String(), data[ 4 ], length );
		contract_assert( 0 );
		return 0;
	}

	if ( net_showudp.GetInt() && (*(unsigned int*)data == CONNECTIONLESS_HEADER) && net_showudp_oob.GetInt() )
	{
		contract_assert( !bUseCompression );
		if ( !net_showudp_remoteonly.GetBool() || !( to.IsLocalhost() || to.IsLoopback() ) )
		{
			Msg("UDP -> %s: sz=%d OOB '0x%02X'\n", ns_address_render( to ).String(), length, data[4] );
		}
	}

	// Only NA_LOOPBACK means the peer lives in this process. A numeric 127.0.0.1
	// peer is a separate process on this machine and must go over real sockets.
	if ( !NET_IsMultiplayer() || to.IsLoopback() )
	{
		contract_assert( !pVoicePayload );

		NET_SendLoopPacket(sock, length, data );
		return length;
	}

	ks::platform::socket *net_socket = nullptr;
	if ( to.IsType<netadr_t>() )
	{
		net_socket = net_sockets[sock].hUDP;
		if (!net_socket)
			return length;
	}

	if ( (droppackets.GetInt() < 0)  && sock == NS_CLIENT )
	{
		droppackets.SetValue( droppackets.GetInt() + 1 );
		return length;
	}

	if ( fakeloss.GetFloat() > 0.0f )
	{
		// simulate sending this packet
		if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
			return length;
	}

	ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressed( NET_COMPRESSION_STACKBUF_SIZE );
	ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressedVoice( NET_COMPRESSION_STACKBUF_SIZE );
	ks::small_vector< byte, NET_COMPRESSION_STACKBUF_SIZE > memEncryptedAll( NET_COMPRESSION_STACKBUF_SIZE );

	int iGameDataLength = pVoicePayload ? length : -1;

	bool bWroteVoice = false;
	unsigned int nVoiceBytes = 0;

	if ( pVoicePayload )
	{
		ks::grow_to( memCompressedVoice, pVoicePayload->GetNumBytesWritten() + sizeof( unsigned short ) );

		byte *pVoice = (byte *)memCompressedVoice.data();

		unsigned short usVoiceBits = pVoicePayload->GetNumBitsWritten();
		*( unsigned short * )pVoice = ( usVoiceBits );
		pVoice += sizeof( unsigned short );
		
		unsigned int nCompressedLength = pVoicePayload->GetNumBytesWritten();
		byte *pOutput = nullptr;
		if ( net_compressvoice.GetBool() )
		{
			CLZSS lzss;
			pOutput = lzss.CompressNoAlloc( pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten(), (byte *)pVoice, &nCompressedLength );
		}
		if ( !pOutput )
		{
			V_memcpy( pVoice, pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten() );
		}

		nVoiceBytes = nCompressedLength + sizeof( unsigned short );
	}

	if ( voice_xsend_debug.GetBool() && nVoiceBytes )
	{
		DevMsg( "XVoice: voice data payload for %p: %d bytes\n", chan, nVoiceBytes );
	}

	if ( bUseCompression )
	{
		CLZSS lzss;
		unsigned int nCompressedLength = length;
	
		ks::grow_to( memCompressed, length + nVoiceBytes + sizeof( unsigned int ) );

		*(int *)memCompressed.data() = ( NET_HEADER_FLAG_COMPRESSEDPACKET );

		byte *pOutput = lzss.CompressNoAlloc( (byte *)data, length, memCompressed.data() + sizeof( unsigned int ), &nCompressedLength );
		if ( pOutput )
		{
			data	= memCompressed.data();
			length	= nCompressedLength + sizeof( unsigned int );

			if ( pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
			{
				byte *pVoice = (byte *)memCompressed.data() + length;
				V_memcpy( pVoice, memCompressedVoice.data(), nVoiceBytes );
			}
			
			iGameDataLength = length;

			length += nVoiceBytes;

			bWroteVoice = true;
		}
	}
	
	if ( !bWroteVoice && pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
	{
		ks::grow_to( memCompressed, length + nVoiceBytes );

		byte *pVoice = (byte *)memCompressed.data();
		V_memcpy( pVoice, (const void *)data, length );
		pVoice += length;
		V_memcpy( pVoice, memCompressedVoice.data(), nVoiceBytes );
		data	= memCompressed.data();

		length  += nVoiceBytes;
	}

	// If the network channel has encryption key then we should encrypt
	if ( const unsigned char *pubEncryptionKey = chan ? chan->GetChannelEncryptionKey() : nullptr )
	{
		IceKey iceKey( 2 );
		iceKey.set( pubEncryptionKey );

		// Generate some random fudge, ICE operates on 64-bit blocks, so make sure our total size is a multiple of 8 bytes
		int numRandomFudgeBytes = RandomInt( 16, 72 );
		int numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + length;
		numRandomFudgeBytes += iceKey.blockSize() - ( numTotalEncryptedBytes % iceKey.blockSize() );
		numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + length;

		char *pchRandomFudgeBytes = ( char * ) scratch.AllocBytes( numRandomFudgeBytes );
		for ( int k = 0; k < numRandomFudgeBytes; ++k )
			pchRandomFudgeBytes[ k ] = RandomInt( 16, 250 );

		// Prepare the encrypted memory
		ks::grow_to( memEncryptedAll, numTotalEncryptedBytes );
		* memEncryptedAll.data() = numRandomFudgeBytes;
		V_memcpy( memEncryptedAll.data() + 1, pchRandomFudgeBytes, numRandomFudgeBytes );

		int32 const numBytesWrittenWire = DWordSwapC( length );	// byteswap for the wire
		V_memcpy( memEncryptedAll.data() + 1 + numRandomFudgeBytes, &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
		V_memcpy( memEncryptedAll.data() + 1 + numRandomFudgeBytes + sizeof( int32 ), data, length );

		// Encrypt the message
		unsigned char *pchCryptoBuffer = ( unsigned char * ) scratch.AllocBytes( iceKey.blockSize() );
		for ( int k = 0; k < numTotalEncryptedBytes; k += iceKey.blockSize() )
		{
			iceKey.encrypt( ( const unsigned char * ) ( memEncryptedAll.data() + k ), pchCryptoBuffer );
			V_memcpy( memEncryptedAll.data() + k, pchCryptoBuffer, iceKey.blockSize() );
		}

		// Set the pointers to network out the encrypted data
		data = memEncryptedAll.data();
		length = numTotalEncryptedBytes;
	}

	// Do we need to break this packet up?
	int nMaxRoutable = MAX_ROUTABLE_PAYLOAD;
	if ( chan )
	{
		nMaxRoutable = clamp( chan->GetMaxRoutablePayloadSize(), MIN_USER_MAXROUTABLE_SIZE, std::min( sv_maxroutable.GetInt(), MAX_ROUTABLE_PAYLOAD ) );
	}

	if ( ( unMillisecondsDelay != 0u ) &&
		( length > nMaxRoutable ) )
	{
		Warning( "Can't delay send a packet larger than maxroutable size %d/%d\n", length, nMaxRoutable );
		unMillisecondsDelay = 0u;
	}

	if ( unMillisecondsDelay != 0u )
	{
		ret = NET_QueuePacketForSend( static_cast< CNetChan * >( chan ), false, net_socket, (const char *)data, length, to, unMillisecondsDelay );
	}
	else if ( length <= nMaxRoutable && 
		!(net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE && chan ) )	
	{
		// simple case, small packet, just send it
		ret = NET_SendTo( true, net_socket, (const char *)data, length, to, iGameDataLength );
	}
	else
	{
		// split packet into smaller pieces
		ret = NET_SendLong( chan, sock, net_socket, (const char *)data, length, to, nMaxRoutable );
	}
	
	if (ret == -1)
	{
		NET_GetLastError();
		
		// wouldblock is silent
		if ( net_error == ks::platform::socket_error::again )
			return 0;

		if ( net_error == ks::platform::socket_error::reset )
			return 0;

		// some PPP links dont allow broadcasts
		if ( ( net_error == ks::platform::socket_error::address_unavailable) && ( to.IsBroadcast() ) )
			return 0;

		ConDMsg ("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(net_error), ns_address_render( to ).String() );
		ret = length;
	}
	

	return ret;
}

void NET_OutOfBandPrintf(int sock, const ns_address &adr, const char *format, ...)
{
	std::va_list		argptr;
	char		string[MAX_ROUTABLE_PAYLOAD];
	
	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	__builtin_va_start (argptr, format);
	V_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	__builtin_va_end (argptr);

	int length = V_strlen(string+4) + 5;

	NET_SendPacket ( nullptr, sock, adr, (byte *)string, length );
}

void NET_OutOfBandDelayedPrintf(int sock, const ns_address &adr, uint32 unMillisecondsDelay, const char *format, ...)
{
	std::va_list		argptr;
	char		string[MAX_ROUTABLE_PAYLOAD];

	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	__builtin_va_start (argptr, format);
	V_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	__builtin_va_end (argptr);

	int length = V_strlen(string+4) + 5;

	NET_SendPacket ( nullptr, sock, adr, (byte *)string, length, 0, false, unMillisecondsDelay );
}

/*
====================
NET_CloseAllSockets
====================
*/
void NET_CloseAllSockets (void)
{
	// shut down any existing and open sockets
	for (int i=0 ; i<std::ssize( net_sockets ) ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );

			net_sockets[i].nPort = 0;
			net_sockets[i].bListening = false;
			net_sockets[i].hUDP = nullptr;
			net_sockets[i].hTCP = nullptr;
		}
	}

	// shut down all pending sockets
	std::scoped_lock lock( s_PendingSockets );
	for(int j=0; j<std::ssize( s_PendingSockets );j++ )
	{
		NET_CloseSocket( s_PendingSockets[j].newsock );
	}

	s_PendingSockets.clear();
}

/*
====================
NET_FlushAllSockets
====================
*/
void NET_FlushAllSockets( void )
{
	// drain any packets that my still lurk in our incoming queue
	char data[2048];
	ns_address from;
	
	for (int i=0 ; i<std::ssize( net_sockets ) ; i++)
	{
		if ( net_sockets[i].hUDP )
		{
			int bytes = 1;

			// loop until no packets are pending anymore
			while ( bytes > 0  )
			{
				bytes = NET_ReceiveRawPacket( i, data, sizeof(data), &from );
			}
		}
	}
}


static void OpenSocketInternal( int nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny )
{
	std::vector< std::string > vecBindableAddresses;
	if ( ( NS_HLTV == nModule )&& ipname_tv.GetString()[0] )
	{
		vecBindableAddresses.push_back( std::string( ipname_tv.GetString() ) );
	}
	else if ( ( NS_HLTV1 == nModule ) && ipname_tv1.GetString()[ 0 ] )
	{
		vecBindableAddresses.push_back( std::string( ipname_tv1.GetString() ) );
	}
	else
	{
		vecBindableAddresses.push_back( std::string( ipname.GetString() ) );
	}

	int port = nSetPort ? nSetPort : nDefaultPort;
	ks::platform::socket **handle = nullptr;
	if( nProtocol == IPPROTO_TCP )
	{
		handle = &net_sockets[nModule].hTCP;
	}
	else if ( nProtocol == IPPROTO_UDP || nProtocol == IPPROTO_UDP )
	{
		handle = &net_sockets[nModule].hUDP;
	}
	else
	{
		Sys_Error( "Unrecognized protocol type %d", nProtocol );
		return;
	}

	if ( !net_sockets[nModule].nPort )
	{
		int nSavePort = port;
		for ( int i = 0; i < std::ssize( vecBindableAddresses ); ++i )
		{
			port = nSavePort;

			char const *pchIPAddressToBind = vecBindableAddresses[ i ].c_str();

			if ( i > 0 )
			{
				Msg( "Trying to open socket on %s\n", pchIPAddressToBind );
			}

			*handle = NET_OpenSocket (pchIPAddressToBind, port, nProtocol );

			if ( !*handle && bTryAny )
			{
				port = PORT_ANY;	// try again with PORT_ANY
				*handle = NET_OpenSocket (pchIPAddressToBind, port, nProtocol );
			}

			// Stop on first success
			if ( *handle )
				break;
		}

		if ( !*handle )
		{
			Warning( "Couldn't allocate any %s IP port, tried %d addresses %s", pName, (int)std::ssize( vecBindableAddresses ),
				( (int)std::ssize( vecBindableAddresses ) ? vecBindableAddresses.front().data() : "(none)" ) );
			Plat_ExitProcess( 0 );	// cause a silent exit without writing a core dump
			return;
		}

		net_sockets[nModule].nPort = port;
	}
	else
	{
		Msg("WARNING: NET_OpenSockets: %s port %i already open.\n", pName, net_sockets[nModule].nPort );
	}
}

/*
====================
NET_OpenSockets
====================
*/
void NET_OpenSockets (void)
{	
	// Xbox 360 uses VDP protocol to combine encrypted game data with clear voice data
	const int nProtocol = IPPROTO_UDP;

	OpenSocketInternal( NS_SERVER, hostport.GetInt(), PORT_SERVER, "server", nProtocol, false );
	if ( !net_dedicated )
		OpenSocketInternal( NS_CLIENT, clientport.GetInt(), PORT_SERVER, "client", nProtocol, true );


	if ( !net_nohltv )
	{
		OpenSocketInternal( NS_HLTV, hltvport.GetInt(), PORT_HLTV, "hltv", nProtocol, false );
		if ( net_addhltv1 )
		{
			OpenSocketInternal( NS_HLTV1, hltvport1.GetInt(), PORT_HLTV1, "hltv1", nProtocol, false );
		}
	}
}

bool NET_IsSocketOpen( int nSockIdx )
{
	if ( !ks::is_valid_index( net_sockets, nSockIdx ) )
		return false;

	if ( !net_sockets[nSockIdx].hUDP )
		return false;

	return true;
}

int NET_AddExtraSocket( int port )
{
	int newSocket = std::ssize( net_sockets ); net_sockets.emplace_back();

	V_memset( &net_sockets[newSocket], 0, sizeof(netsocket_t) );

	OpenSocketInternal( newSocket, port, PORT_ANY, "extra", IPPROTO_UDP, true );

	ks::ensure_size( net_packets, newSocket+1 );
	ks::ensure_size( net_splitpackets, newSocket+1 );

	return newSocket;
}

void NET_RemoveAllExtraSockets()
{
	for (int i=MAX_SOCKETS ; i<std::ssize( net_sockets ) ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );
		}
	}
	net_sockets.erase( net_sockets.begin() + MAX_SOCKETS, net_sockets.begin() + MAX_SOCKETS + ( std::ssize( net_sockets )-MAX_SOCKETS ) );

	contract_assert( std::ssize( net_sockets ) == MAX_SOCKETS );
}

unsigned short NET_GetUDPPort(int socket)
{
	return ks::is_valid_index( net_sockets, socket ) ?
		net_sockets[socket].nPort : 0;
}

static void NET_ConPrintByteStream( uint8 const *pb, int nSize )
{
	for ( int k = 0; k < nSize; ++ k, ++ pb )
		Msg( " %02X", *pb );
}

/*
================
NET_GetLocalAddress

Returns the servers' ip address as a string.
================
*/
void NET_GetLocalAddress (void)
{
	net_local_adr.Clear();

	if ( net_noip )
	{
		Msg("TCP/UDP Disabled.\n");
	}
	else
	{
		char	buff[512];

		// If we have changed the ip var from the command line, use that instead.
		if ( V_strcmp(ipname.GetString(), "localhost") )
		{
			V_strncpy(buff, ipname.GetString(), sizeof( buff ) );	// use IP set with ipname
		}
		else
		{
			ks::platform::host_name( buff, sizeof(buff) );	// get own IP address
			buff[sizeof(buff)-1] = 0;			// Ensure that it doesn't overrun the buffer
		}

		NET_StringToAdr (buff, &net_local_adr);

		int ipaddr = ( net_local_adr.ip[0] << 24 ) + 
			( net_local_adr.ip[1] << 16 ) + 
			( net_local_adr.ip[2] << 8 ) + 
			net_local_adr.ip[3];

		hostip.SetValue( ipaddr );
	}
}


/*
====================
NET_IsConfigured

Is winsock ip initialized?
====================
*/
bool NET_IsMultiplayer( void )
{
	return net_multiplayer;
}

bool NET_IsDedicated( void )
{
	return net_dedicated;
}



/*
====================
NET_SetTime

Updates net_time
====================
*/
void NET_SetTime( double realtime )
{
	static double s_last_realtime = 0;

	double frametime = realtime - s_last_realtime;
	s_last_realtime = realtime;

	if ( frametime > 1.0f )
	{
		// if we have very long frame times because of loading stuff
		// don't apply that to net time to avoid unwanted timeouts
		frametime = 1.0f;
	}
	else if ( frametime < 0.0f )
	{
		frametime = 0.0f;
	}

#if !defined( DEDICATED )
	// adjust network time so fakelag works with host_timescale
	net_time += frametime * g_pEngineToolInternal->GetTimescale();
#else
    	net_time += frametime * sv.GetTimescale();
#endif
}

/*
====================
NET_RunFrame

RunFrame must be called each system frame before reading/sending on any socket
====================
*/
void NET_RunFrame( double realtime )
{
	NET_SetTime( realtime );

	RCONServer().RunFrame();

	Con_RunFrame();
#ifndef DEDICATED
	RCONClient().RunFrame();
#endif

	if ( g_pMatchFramework )
	{
		g_pMatchFramework->RunFrame();
	}

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	// process TCP sockets:
	for ( int i=0; i< std::ssize( net_sockets ); i++ )
	{
		if ( net_sockets[i].hTCP && net_sockets[i].bListening )
		{
			NET_ProcessListen( i );
		}
	}

	NET_ProcessPending();
}

void NET_ClearLoopbackBuffers()
{
	for (int i = 0; i < LOOPBACK_SOCKETS; i++)
	{
		loopback_t *loop = nullptr;

		while ( s_LoopBacks[i].PopItem( &loop ) )
		{
			if ( loop->data && loop->data != loop->defbuffer )
			{
				delete [] loop->data;
			}
			delete loop;
		}
	}
}

void NET_ConfigLoopbackBuffers( bool bAlloc )
{
	NET_ClearLoopbackBuffers();
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/

void NET_Config ( void )
{
	// free anything
	NET_CloseAllSockets();	// close all UDP/TCP sockets

	net_time = 0.0f;

	// now reconfigure

	if ( net_multiplayer )
	{	
		// don't allocate loopback buffers
		NET_ConfigLoopbackBuffers( false );

		// get localhost IP address
		NET_GetLocalAddress();

		// reopen sockets if in MP mode
		NET_OpenSockets();

		// setup the rcon server sockets
		if ( net_dedicated || CommandLine()->FindParm( "-usercon" ) )
		{
			netadr_t rconAddr = net_local_adr;
			rconAddr.SetPort( net_sockets[NS_SERVER].nPort );
			RCONServer().SetAddress( rconAddr.ToString() );
			RCONServer().CreateSocket();
		}
	}
	else
	{
		// allocate loopback buffers
		NET_ConfigLoopbackBuffers( true );
	}
	

	DevMsg( "mode %s, dedicated %s, ports %i SV / %i CL\n", 
		net_multiplayer?"MP":"SP", net_dedicated?"Yes":"No", 
		net_sockets[NS_SERVER].nPort, net_sockets[NS_CLIENT].nPort );
}

/*
====================
NET_SetDedicated

A single player game will only use the loopback code
====================
*/

void NET_SetDedicated ()
{
	if ( net_noip )
	{
		Msg( "Warning! Dedicated not possible with -noip parameter.\n");
		return;		
	}

	net_dedicated = true;
}

void NET_ListenSocket( int sock, bool bListen )
{
	contract_assert( (sock >= 0) && (sock < std::ssize( net_sockets )) );

	netsocket_t * netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	if ( bListen )
	{
		const char * net_interface = ipname.GetString();

		netsock->hTCP = NET_OpenSocket( net_interface, netsock->nPort, true );

		if ( !netsock->hTCP )
		{
			Msg( "Warning! NET_ListenSocket failed opening socket %i, port %i.\n", sock, net_sockets[sock].nPort );
			return;
		}

		ks::platform::address address{};

		if (!net_interface || !net_interface[0] || !V_strcmp(net_interface, "localhost"))
		{
			address.ip = 0;
		}
		else
		{
			NET_StringToAddress (net_interface, &address);
		}

		address.port = (unsigned short)( netsock->nPort );

		if ( !ks::platform::bind( netsock->hTCP, address, nullptr ) )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket bind failed on socket %i, port %i.\n", sock, netsock->nPort );
			return;
		}

		if ( !ks::platform::listen_on( netsock->hTCP, TCP_MAX_ACCEPTS, nullptr ) )
		{
			NET_GetLastError();
			Msg ("WARNING: NET_ListenSocket listen failed on socket %i, port %i.\n", sock, netsock->nPort );
			return;
		}

		netsock->bListening = true;
	}
}

void NET_SetMultiplayer(bool multiplayer)
{
	if ( net_noip && multiplayer )
	{
		Msg( "Warning! Multiplayer mode not available with -noip parameter.\n");
		return;		
	}

	if ( net_dedicated && !multiplayer )
	{
		Msg( "Warning! Singleplayer mode not available on dedicated server.\n");
		return;		
	}

	// reconfigure if changed
	if ( net_multiplayer != multiplayer )
	{
		net_multiplayer = multiplayer;
		NET_Config();
	}

	// clear loopback buffer in single player mode
	if ( !multiplayer )
	{
		NET_ClearLoopbackBuffers();
	}
}

void NET_InitPostFork( void )
{
	if ( CommandLine()->FindParm( "-NoQueuedPacketThread" ) )
		Warning( "Found -NoQueuedPacketThread, so no queued packet thread will be created.\n" );
	else
		g_pQueuedPackedSender->Setup();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bIsDedicated - 
//-----------------------------------------------------------------------------
void NET_Init( bool bIsDedicated )
{
	// In dedicated server mode network must be initialized post-fork
	
	// single entry guard
	{
		static bool sbNetworkingIntialized = false;

		if ( sbNetworkingIntialized )
		{
			return;
		}

		sbNetworkingIntialized = true;
	}

	if (CommandLine()->FindParm("-nodns"))
	{
		net_nodns = true;
	}

	if (CommandLine()->FindParm("-usetcp"))
	{
		net_notcp = false;
	}

	if ( CommandLine()->FindParm( "-nohltv" ) )
	{
		net_nohltv = true;
	}

	if ( CommandLine()->FindParm( "-addhltv1" ) )
	{
		net_addhltv1 = true;
	}

	if (CommandLine()->FindParm("-noip"))
	{
		net_noip = true;
	}
	else
	{
	}

	contract_assert( NET_MAX_PAYLOAD < (1<<NET_MAX_PAYLOAD_BITS) );
	contract_assert( MAX_FILE_SIZE < (1<<MAX_FILE_SIZE_BITS) );

	net_time = 0.0f;

	//
	// Process ports configuration
	//
	{
		// Host port
		int nHostPort = hostport.GetInt();
		nHostPort = CommandLine()->ParmValue( "-port", nHostPort );
		nHostPort = CommandLine()->ParmValue( "+port", nHostPort );
		nHostPort = CommandLine()->ParmValue( "+hostport", nHostPort );
		hostport.SetValue( nHostPort );

		// Client port
		int nClientPort = clientport.GetInt();
		nClientPort = CommandLine()->ParmValue( "+clientport", nClientPort );
		clientport.SetValue( nClientPort );

		// HLTV ports
		{
			int nHltvPort = hltvport.GetInt();
			nHltvPort = CommandLine()->ParmValue( "+tv_port", nHltvPort );
			hltvport.SetValue( nHltvPort );
		}

		{
			int nHltvPort1 = hltvport1.GetInt();
			nHltvPort1 = CommandLine()->ParmValue( "+tv_port1", nHltvPort1 );
			hltvport1.SetValue( nHltvPort1 );
		}
	}

	// clear static stuff
	ks::ensure_size( net_sockets, MAX_SOCKETS );
	ks::ensure_size( net_packets, MAX_SOCKETS );
	ks::ensure_size( net_splitpackets, MAX_SOCKETS );

	for ( int i = 0; i < MAX_SOCKETS; ++i )
	{
		s_pLagData[i] = nullptr;
		V_memset( &net_sockets[i], 0, sizeof(netsocket_t) );
	}

	if ( const char *ip = CommandLine()->ParmValue( "-ip" ) ) // if they had a command line option for IP
	{
		ipname.SetValue( ip );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_tv = CommandLine()->ParmValue( "-ip_tv" ) ) // if they had a command line option for IP for GOTV
	{
		ipname_tv.SetValue( ip_tv );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_tv1 = CommandLine()->ParmValue( "-ip_tv1" ) ) // if they had a command line option for IP for GOTV
	{
		ipname_tv1.SetValue( ip_tv1 );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}
	if ( const char *ip_relay = CommandLine()->ParmValue( "-ip_relay" ) ) // if they had a command line option for IP relay for GOTV
	{
		ipname_relay.SetValue( ip_relay );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}

	if ( bIsDedicated )
	{
		// set dedicated MP mode
		NET_SetDedicated();
	}
	else
	{
		// set SP mode
		NET_ConfigLoopbackBuffers( true );
	}

	NET_InitParanoidMode();

	NET_SetMultiplayer( !!( g_pMatchFramework->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SETTING_MULTIPLAYER ) );
}

/*
====================
NET_Shutdown

====================
*/
void NET_Shutdown (void)
{
	int nError = 0;

	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		NET_ClearLaggedList( &s_pLagData[i] );
	}

	g_pQueuedPackedSender->Shutdown();

	net_multiplayer = false;
	net_dedicated = false;

	NET_CloseAllSockets();
	NET_ConfigLoopbackBuffers( false );


	contract_assert( std::ssize( s_NetChannels ) == 0 );
	contract_assert( std::ssize( s_PendingSockets ) == 0 );
}

void NET_PrintChannelStatus( INetChannel * chan )
{
	Msg( "NetChannel '%s':\n", chan->GetName() );
	Msg( "- remote IP: %s\n", chan->GetAddress() );
	Msg( "- online: %s\n", COM_FormatSeconds( chan->GetTimeConnected() ) );
	Msg( "- reliable: %s\n", chan->HasPendingReliableData()?"pending data":"available" );
	Msg( "- latency: %.1f, loss %.2f\n", chan->GetAvgLatency(FLOW_OUTGOING), chan->GetAvgLoss(FLOW_INCOMING) );
	Msg( "- packets: in %.1f/s, out %.1f/s\n", chan->GetAvgPackets(FLOW_INCOMING), chan->GetAvgPackets(FLOW_OUTGOING) );
	Msg( "- choke: in %.2f, out %.2f\n", chan->GetAvgChoke(FLOW_INCOMING), chan->GetAvgChoke(FLOW_OUTGOING) );
	Msg( "- flow: in %.1f, out %.1f kB/s\n", chan->GetAvgData(FLOW_INCOMING)/1024.0f, chan->GetAvgData(FLOW_OUTGOING)/1024.0f );
	Msg( "- total: in %.1f, out %.1f MB\n\n", (float)chan->GetTotalData(FLOW_INCOMING)/(1024*1024), (float)chan->GetTotalData(FLOW_OUTGOING)/(1024*1024) );
}

static void net_channels( const CCommand &args );
static ConCommand net_channels_command( "net_channels", net_channels, "Shows net channel info" );
static void net_channels( const CCommand &args )
{
	int numChannels = std::ssize( s_NetChannels );

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	std::scoped_lock lock( s_NetChannels );
	for ( int i = 0; i < numChannels; i++ )
	{
		NET_PrintChannelStatus( s_NetChannels[i] );
	}
}

static void net_start( const CCommand &args );
static ConCommand net_start_command( "net_start", net_start, "Inits multiplayer network sockets" );
static void net_start( const CCommand &args )
{
	net_multiplayer = true;
	NET_Config();
}

static void net_status( const CCommand &args );
static ConCommand net_status_command( "net_status", net_status, "Shows current network status" );
static void net_status( const CCommand &args )
{
	std::scoped_lock lock( s_NetChannels );
	int numChannels = std::ssize( s_NetChannels );

	ConMsg("Net status for host %s:\n", 
		net_local_adr.ToString(true) );

	ConMsg("- Config: %s, %s, %i connections\n",
		net_multiplayer?"Multiplayer":"Singleplayer",
		net_dedicated?"dedicated":"listen",
		numChannels	);
		

	ConMsg("- Ports: " );
	for ( int k = 0; k < MAX_SOCKETS; ++ k )
	{
		ConMsg( "%s%d %u, ", DescribeSocket( k ), k, NET_GetUDPPort( k ) );
	}
	ConMsg( "%d total.\n", MAX_SOCKETS );

	if ( numChannels <= 0 )
	{
		return;
	}

	// gather statistics:

	float avgLatencyOut = 0;
	float avgLatencyIn = 0;
	float avgPacketsOut = 0;
	float avgPacketsIn = 0;
	float avgLossOut = 0;
	float avgLossIn = 0;
	float avgDataOut = 0;
	float avgDataIn = 0;

	for ( int i = 0; i < numChannels; i++ )
	{
		CNetChan *chan = s_NetChannels[i];

		avgLatencyOut += chan->GetAvgLatency(FLOW_OUTGOING);
		avgLatencyIn += chan->GetAvgLatency(FLOW_INCOMING);

		avgLossIn += chan->GetAvgLoss(FLOW_INCOMING);
		avgLossOut += chan->GetAvgLoss(FLOW_OUTGOING);

		avgPacketsIn += chan->GetAvgPackets(FLOW_INCOMING);
		avgPacketsOut += chan->GetAvgPackets(FLOW_OUTGOING);
		
		avgDataIn += chan->GetAvgData(FLOW_INCOMING);
		avgDataOut += chan->GetAvgData(FLOW_OUTGOING);
	}

	ConMsg( "- Latency: avg out %.2fs, in %.2fs\n",  avgLatencyOut/numChannels, avgLatencyIn/numChannels );
 	ConMsg( "- Loss:    avg out %.1f, in %.1f\n", avgLossOut/numChannels, avgLossIn/numChannels );
	ConMsg( "- Packets: net total out  %.1f/s, in %.1f/s\n", avgPacketsOut, avgPacketsIn );
	ConMsg( "           per client out %.1f/s, in %.1f/s\n", avgPacketsOut/numChannels, avgPacketsIn/numChannels );
	ConMsg( "- Data:    net total out  %.1f, in %.1f kB/s\n", avgDataOut/1024.0f, avgDataIn/1024.0f );
	ConMsg( "           per client out %.1f, in %.1f kB/s\n", (avgDataOut/numChannels)/1024.0f, (avgDataIn/numChannels)/1024.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer compression from source into dest
// Input  : *dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// Output : int
//-----------------------------------------------------------------------------
bool NET_BufferToBufferCompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen )
{
	contract_assert( dest );
	contract_assert( destLen );
	contract_assert( source );

	V_memcpy( dest, source, sourceLen );
	CLZSS s;
	unsigned int uCompressedLen = 0;
	byte *pbOut = s.Compress( (byte *)source, sourceLen, &uCompressedLen );
	if ( pbOut && uCompressedLen > 0 && uCompressedLen <= *destLen )
	{
		V_memcpy( dest, pbOut, uCompressedLen );
		*destLen = uCompressedLen;
		g_pMemAlloc->Free( pbOut );
	}
	else
	{
		if ( pbOut )
		{
			g_pMemAlloc->Free( pbOut );
		}
		V_memcpy( dest, source, sourceLen );
		*destLen = sourceLen;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer decompression from source into dest
// Input  : *dest - 
//			*destLen - 
//			*source - 
//			sourceLen - 
// Output : int
//-----------------------------------------------------------------------------
bool NET_BufferToBufferDecompress( char *dest, unsigned int *destLen, char *source, unsigned int sourceLen )
{
	CLZSS s;
	if ( s.IsCompressed( (byte *)source ) )
	{
		unsigned int uDecompressedLen = s.GetActualSize( (byte *)source );
		if ( uDecompressedLen > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, uDecompressedLen );
			return false;
		}
		else
		{
			*destLen = s.SafeUncompress( (byte *)source, (byte *)dest, *destLen );
		}
	}
	else
	{
		if ( sourceLen > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, sourceLen );
			return false;
		}

		V_memcpy( dest, source, sourceLen );
		*destLen = sourceLen;
	}

	return true;
}

void NET_SleepUntilMessages( int nMilliseconds )
{
	if ( !net_sockets[NS_SERVER].hUDP )
	{
		Sys_Sleep( nMilliseconds );
		return;
	}

	ks::platform::wait_readable( net_sockets[NS_SERVER].hUDP, (unsigned long long)nMilliseconds * 1000000ull, nullptr );
}

bool NET_GetPublicAdr( netadr_t &adr )
{
	bool bRet = false;
	unsigned short port = NET_GetUDPPort( NS_SERVER );
	if ( net_public_adr.GetString()[ 0 ] )
	{
		bRet = true;
		adr.SetType( NA_IP );
		adr.SetFromString( net_public_adr.GetString() );
		if ( adr.GetPort() == 0 )
			adr.SetPort( port );
	}
	return bRet;
}

void NET_TerminateConnection( int sock, const ns_address &peer )
{
}

//////////////////////////////////////////////////////////////////////////
//
// Encrypted networking
//
//////////////////////////////////////////////////////////////////////////

bool NET_CryptVerifyServerCertificateAndAllocateSessionKey( bool bOfficial, const ns_address &from,
	const byte *pchKeyPub, int numKeyPub,
	const byte *pchKeySgn, int numKeySgn,
	byte **pbAllocatedKey, int *pnAllocatedCryptoBlockSize )
{
	contract_assert( false );
	return false;
}

bool NET_CryptVerifyClientSessionKey( bool bOfficial,
	const byte *pchKeyPri, int numKeyPri,
	const byte *pbEncryptedKey, int numEncryptedBytes,
	byte *pbPlainKey, int numPlainKeyBytes )
{
	contract_assert( false );
	return false;
}

bool NET_CryptGetNetworkCertificate( ENetworkCertificate_t eType, const byte **pbData, int *pnumBytes )
{
	contract_assert( false );
	return false;
}
