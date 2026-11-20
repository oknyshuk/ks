// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: implementation of the rcon server

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.logging;
import ks.core.scratcharena;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.netadr;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "server.h"
#include "sv_rcon.h"
#include "proto_oob.h" // PORT_RCON define
#include "sv_remoteaccess.h"
#include "cl_rcon.h"

static CRConServer g_RCONServer;
CRConServer & RCONServer()
{
	return g_RCONServer;
}

static void RconPasswordChanged_f( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	const char *pPassword = var.GetString(); 
#ifndef DEDICATED
	RCONClient().SetPassword( pPassword );
#endif
	RCONServer().SetPassword( pPassword );

}
ConVar  rcon_password	( "rcon_password", "", FCVAR_SERVER_CANNOT_QUERY|FCVAR_DONTRECORD|FCVAR_RELEASE, "remote console password.", RconPasswordChanged_f );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

CRConServer::CRConServer() : m_Socket( this )
{
}

CRConServer::CRConServer( const char *pNetAddress ) : m_Socket( this )
{
	SetAddress( pNetAddress );
}



//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRConServer::~CRConServer()
{
}


//-----------------------------------------------------------------------------
// Allows a server to request a listening client to connect to it
//-----------------------------------------------------------------------------
bool CRConServer::ConnectToListeningClient( const netadr_t &adr, bool bSingleSocket )
{
	if ( m_Socket.ConnectSocket( adr, bSingleSocket ) < 0 )
	{
		// This should probably go into its own channel, but for now send it where it was already going (the "console" channel).
		LoggingChannelID_t consoleChannel = LoggingSystem_FindChannel( "Console" );
		Log_Warning( consoleChannel, "Unable to connect to remote client (%s)\n", adr.ToString() );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the listening socket is created and listening
//-----------------------------------------------------------------------------
bool CRConServer::IsConnected()
{
	return m_Socket.IsListening();
}

void CRConServer::SetPassword( const char *pPassword )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Password = ks::nz( pPassword );
}

bool CRConServer::HasPassword() const
{
	return !m_Password.empty();
}

bool CRConServer::IsPassword( const char *pPassword ) const
{
	// Must have a password set to allow any rconning.
	if ( !HasPassword() )
		return false;

	// If the pw does not match, then not authed
	return ( V_strcmp( pPassword, m_Password.c_str() ) == 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Set the address to bind to
//-----------------------------------------------------------------------------
void CRConServer::SetAddress( const char *pNetAddress )
{
	NET_StringToAdr( pNetAddress, &m_Address );
	if ( m_Address.GetPort() == 0 )
	{
		m_Address.SetPort( PORT_RCON );
	}
}

bool CRConServer::CreateSocket()
{
	return m_Socket.CreateListenSocket( m_Address );
}


//-----------------------------------------------------------------------------
// Socket listener methods (bound by CSocketCreator)
//-----------------------------------------------------------------------------
bool CRConServer::ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr )
{
	return true;
}

void CRConServer::OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData )
{
	ConnectedRConSocket_t *pNewSocket = new ConnectedRConSocket_t;
	pNewSocket->lastRequestID = 0;
	pNewSocket->authed = false;
	pNewSocket->listenerID = g_ServerRemoteAccess.GetNextListenerID( true, &netAdr );
	*ppData = pNewSocket;
}

void CRConServer::OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData )
{
	m_bSocketDeleted = true;
	ConnectedRConSocket_t *pOldSocket = (ConnectedRConSocket_t*)( pData );
	delete pOldSocket;
}



//-----------------------------------------------------------------------------
// Purpose: accept new connections and walk open sockets and handle any incoming data
//-----------------------------------------------------------------------------
void CRConServer::RunFrame()
{
	ks::ScratchArena<> scratch;
	m_Socket.RunFrame();
	m_bSocketDeleted = false;

	// handle incoming data
	// NOTE: Have to iterate in reverse since we may be killing sockets
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = nCount - 1; i >= 0; --i )
	{
		// process any outgoing data for this socket
		ConnectedRConSocket_t *pData = GetSocketData( i );
		SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( i );
		const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
		while ( !pData->m_OutstandingSends.empty() )
		{
			const auto packet = pData->m_OutstandingSends.front().Remaining();
			bool bSent = SendRCONResponse( i, packet.data(), int( packet.size() ), true );
			if ( bSent ) // all this packet was sent, remove it
			{
				pData->m_OutstandingSends.pop_front(); // delete this entry no matter what, SendRCONResponse() will re-queue if needed
			}
			else // must have blocked part way through, SendRCONResponse
			    // fixed up the queued entry
			{
				break;
			}
		}
		
		int sendLen = g_ServerRemoteAccess.GetDataResponseSize( pData->listenerID );
		if ( sendLen > 0 )
		{
			char sendBuf[4096];
			char *pBuf = sendBuf;
			bool bAllocate = ( sendLen + sizeof(int) > sizeof(sendBuf) );
			if ( bAllocate )
			{
				pBuf = new char[sendLen + sizeof(int)];
			}
			std::memcpy( pBuf, &sendLen, sizeof(sendLen) ); // copy the size of the packet in
			g_ServerRemoteAccess.ReadDataResponse( pData->listenerID, pBuf + sizeof(int), sendLen );
			SendRCONResponse( i, pBuf, sendLen + sizeof(int) );
			if ( bAllocate )
			{
				delete [] pBuf;
			}
		}

		// check for incoming data
		int pendingLen = 0;
		unsigned long readLen = 0;
		char ch;
		ks::platform::socket_error error = ks::platform::socket_error::none;
		pendingLen = (int)ks::platform::peek_bytes( hSocket, &ch, sizeof(ch), &error );
		if ( pendingLen == -1 && error == ks::platform::socket_error::again )
			continue;

		if ( pendingLen == 0 )
		{
			m_Socket.CloseAcceptedSocket( i );
			continue;
		}

		if ( pendingLen < 0 )
		{
			//DevMsg( "RCON Cmd: peek error %s\n", NET_ErrorString(WSAGetLastError()));
			m_Socket.CloseAcceptedSocket( i );
			continue;
		}

		// find out how much we have to read
		readLen = ks::platform::bytes_available( hSocket );
		if ( readLen > sizeof(int) ) // we have a command to process
		{
			std::vector< std::byte > &response = pData->packetbuffer;
			response.reserve( response.size() + readLen );
			bool bClosed = false;
			char *recvBuf = (char *)scratch.AllocBytes( ( std::min<std::common_type_t<int, long unsigned int>>( 1024, readLen ) ) ); // a buffer used for recv()
			unsigned int len = 0;
			while ( len < readLen )
			{
				int recvLen = (int)ks::platform::receive_bytes( hSocket, recvBuf , std::min<std::common_type_t<int, long unsigned int>>(1024, readLen - len) , &error );
				if ( recvLen == 0 ) // socket was closed
				{
					m_Socket.CloseAcceptedSocket( i );
					bClosed = true;
					break;
				}
				
				if ( recvLen < 0 && error != ks::platform::socket_error::again )
				{
					Warning( "RCON Cmd: recv error (%s)\n", NET_ErrorString( error ) );
					break;
				}

				if ( recvLen > 0 )
				{
					const auto received = ks::byte_view( recvBuf, recvLen );
					response.insert( response.end(), received.begin(), received.end() );
				}
				len += recvLen;
			}

			if ( bClosed )
				continue;

			ks::byte_reader reader( response );

			int size = reader.read<int>();
		
			if ( size > RCON_MAX_RCON_COMMAND_LEN )
			{
				HandleFailedRconAuth( socketAdr );
				m_Socket.CloseAcceptedSocket( i );
				continue;
			}
			
			while ( size > 0 && std::size_t( size ) <= reader.remaining() )
			{
				SV_RedirectStart( RD_SOCKET, &socketAdr );
				g_ServerRemoteAccess.WriteDataRequest( this, pData->listenerID, reader.rest().data(), size );
				SV_RedirectEnd();
				if ( m_bSocketDeleted )
					return;
				reader.skip( size ); // eat up the buffer we just sent

				if ( reader.remaining() >= sizeof(int) )
				{
					size = reader.read<int>(); // read how much is in this packet
				}
				else
				{
					size = 0; // finished the packet
				}
			}

			// Check and see if socket was closed as a result of processing - this can happen if the user has entered too many passwords
			int nNewCount = m_Socket.GetAcceptedSocketCount();
			if ( 0 == nNewCount || i > nNewCount || pData != GetSocketData( i )  ) 
			{
				break;
			}

			if ( size > 0 || reader.remaining() > 0 )
			{
				ks::byte_writer leftover;
				if ( size > 0 )
				{
					leftover.write<int>( size );
				}
				leftover.write_bytes( reader.rest() );
				response = leftover.release();
			}
			else
			{
				response = {};
			}
		}
	} // for each socket
}


//-----------------------------------------------------------------------------
// Purpose: flush the response of a network command back to a user
//-----------------------------------------------------------------------------
void CRConServer::FinishRedirect( const char *msg, const netadr_t &adr )
{	
	// NOTE: Has to iterate in reverse; SendRCONResponse can close sockets
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = nCount - 1; i >= 0; --i )
	{
		const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
		if ( !adr.CompareAdr( socketAdr ) )
			continue;

		ks::byte_writer response;

		// build the response
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		response.write<int>(0); // the size, this gets set once we make the packet
		response.write<int>(pSocketData->lastRequestID);
		response.write<int>(SERVERDATA_RESPONSE_VALUE);
		response.write_cstring(ks::nz(msg));
		response.write_cstring("");
		int size = response.size() - sizeof(int); 
		response.seek( 0 );
		response.write<int>(size); // the size

		
//		OutputDebugString( va("RCON: String is %i long\n", V_strlen(msg)) ); // can't use DevMsg(), we are potentially inside the RedirectFlush() function
//		printf("RCON: String is %i long, packet size %i\n", V_strlen(msg), size );

		SendRCONResponse( i, response.data(), response.size() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: set the current outstanding request ID for this connection, used by the redirect flush above
//-----------------------------------------------------------------------------
void CRConServer::SetRequestID( ra_listener_id listener, int iRequestID )
{
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = 0; i < nCount; i++ )
	{
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		if ( pSocketData->listenerID == listener)
		{
			pSocketData->lastRequestID = iRequestID;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: send a buffer to a particular connection
//-----------------------------------------------------------------------------
bool CRConServer::SendRCONResponse( int nIndex, const void *data, int len, bool fromQueue )
{
	SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( nIndex );
	if ( hSocket == nullptr )
		return false;

	ConnectedRConSocket_t *pSocketData = GetSocketData( nIndex );

	// if we already have queued data pending then just add this to the end
	// of the queue
	if ( !fromQueue && !pSocketData->m_OutstandingSends.empty() )
	{
		if ( std::ssize( pSocketData->m_OutstandingSends ) > RCON_MAX_OUTSTANDING_SENDS )
		{
			m_Socket.CloseAcceptedSocket( nIndex );
			return false;
		}

		pSocketData->m_OutstandingSends.emplace_back( ks::byte_view( data, len ) );
		return true;
	}

	contract_assert( !( fromQueue && data != (pSocketData->m_OutstandingSends.front().Remaining().data())) );

	int sendLen = 0;
	while ( sendLen < len )
	{
		ks::platform::socket_error error = ks::platform::socket_error::none;
		int ret = (int)ks::platform::send_bytes( hSocket, (const char *)data + sendLen, len - sendLen, &error );
		if ( ret == -1 )
		{
			// can't finish sending this right now, push it back
			// on the TOP of the queue to be sent next time around
			if ( error != ks::platform::socket_error::again )
			{
				m_Socket.CloseAcceptedSocket( nIndex );
				return false;
			}

			if ( !fromQueue ) // we don't have an entry for this
					// yet, add a new one
			{
				pSocketData->m_OutstandingSends.emplace_front( ks::byte_view( (const char *)data + sendLen, len - sendLen ) );
			}
			else // update the existing queued item to show we 
			     // sent some of it (we only ever send the head of the list)
			{
				pSocketData->m_OutstandingSends.front().nSent += sendLen;
			}
			return false;
		}
		else if ( ret > 0 )
		{
			sendLen += ret;
		}
	}
//	printf("RCON: Sending packet %i in len\n", len);
//	OutputDebugString( va("RCON: Sending packet %i in len\n", len) ); // can't use DevMsg(), we are potentially inside the RedirectFlush() function
	return true;
}

ConVar sv_rcon_banpenalty( "sv_rcon_banpenalty", "0", 0, "Number of minutes to ban users who fail rcon authentication", true, 0, false, 0 );
ConVar sv_rcon_maxfailures( "sv_rcon_maxfailures", "10", 0, "Max number of times a user can fail rcon authentication before being banned", true, 1, true, 20 );
ConVar sv_rcon_minfailures( "sv_rcon_minfailures", "5", 0, "Number of times a user can fail rcon authentication in sv_rcon_minfailuretime before being banned", true, 1, true, 20 );
ConVar sv_rcon_minfailuretime( "sv_rcon_minfailuretime", "30", 0, "Number of seconds to track failed rcon authentications", true, 1, false, 0 );
ConVar sv_rcon_whitelist_address( "sv_rcon_whitelist_address", "", FCVAR_RELEASE, "When set, rcon failed authentications will never ban this address, e.g. '127.0.0.1'" );

//-----------------------------------------------------------------------------
// Purpose: compares failed rcons based on most recent failure time
//-----------------------------------------------------------------------------
bool CRConServer::FailedRCon_t::operator<(const struct CRConServer::FailedRCon_t &rhs) const
{
	int myTime = 0;
	int rhsTime = 0;

	if ( !badPasswordTimes.empty() )
		myTime = badPasswordTimes.back();

	if ( !rhs.badPasswordTimes.empty() )
		rhsTime = rhs.badPasswordTimes.back();

	return myTime < rhsTime;
}

//-----------------------------------------------------------------------------
// Purpose: tracks failed rcon attempts and bans repeat offenders
//-----------------------------------------------------------------------------
bool CRConServer::HandleFailedRconAuth( const netadr_t & adr )
{
	if ( sv_rcon_whitelist_address.GetString()[0] )
	{
		if ( !V_strcmp( adr.ToString( true ), sv_rcon_whitelist_address.GetString() ) )
		{
			ConMsg( "Rcon auth failed from rcon whitelist address %s\n", adr.ToString() );
			return false;
		}
	}

	int i;
	FailedRCon_t *failedRcon = nullptr;
	int nCount = std::ssize( m_failedRcons );
	for ( i=0; i < nCount; ++i )
	{
		if ( adr.CompareAdr( m_failedRcons[i].adr, true ) )
		{
			failedRcon = &m_failedRcons[i];
			break;
		}
	}

	if ( !failedRcon )
	{
		// remove an old rcon if necessary
		if ( nCount >= 32 )
		{
			// look for the one with the oldest failure
			int indexToRemove = -1;
			for ( i=0; i < nCount; ++i )
			{
				if ( indexToRemove < 0 || m_failedRcons[i] < m_failedRcons[indexToRemove] )
				{
					indexToRemove = i;
				}
			}
			if ( indexToRemove >= 0 )
			{
				m_failedRcons.erase( m_failedRcons.begin() + indexToRemove );
			}
		}

		// add the new rcon
		int index = std::ssize( m_failedRcons ); m_failedRcons.emplace_back();
		failedRcon = &m_failedRcons[index];
		failedRcon->adr = adr;
		failedRcon->badPasswordCount = 0;
		failedRcon->badPasswordTimes.clear();
	}

	// update this failed rcon
	++failedRcon->badPasswordCount;
	failedRcon->badPasswordTimes.push_back( sv.GetTime() );

	// remove old failure times (sv_rcon_maxfailures is limited to 20, so we won't be hurting anything by pruning)
	while ( failedRcon->badPasswordTimes.size() > 20 )
	{
		failedRcon->badPasswordTimes.pop_front();
	}

	// sanity-check the rcon banning cvars
	if ( sv_rcon_maxfailures.GetInt() < sv_rcon_minfailures.GetInt() )
	{
		int temp = sv_rcon_maxfailures.GetInt();
		sv_rcon_maxfailures.SetValue( sv_rcon_minfailures.GetInt() );
		sv_rcon_minfailures.SetValue( temp );
	}

//	ConMsg( "%d of %d bad password times tracked\n", (int)failedRcon->badPasswordTimes.size(), failedRcon->badPasswordCount );
//	ConMsg( "min=%d, max=%d, time=%.2f\n", sv_rcon_minfailures.GetInt(), sv_rcon_maxfailures.GetInt(), sv_rcon_minfailuretime.GetFloat() );

	// check if the user should be banned based on total failed attempts
	if ( failedRcon->badPasswordCount > sv_rcon_maxfailures.GetInt() )
	{
		ConMsg( "Banning %s for rcon hacking attempts\n", failedRcon->adr.ToString( true ) );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "addip %i %s\n", sv_rcon_banpenalty.GetInt(), failedRcon->adr.ToString( true ) ) );
		Cbuf_Execute();
		return true;
	}

	// check if the user should be banned based on recent failed attempts
	int recentFailures = 0;
	for ( i=std::ssize( failedRcon->badPasswordTimes )-1; i>=0; --i )
	{
		if ( failedRcon->badPasswordTimes[i] + sv_rcon_minfailuretime.GetFloat() >= sv.GetTime() )
		{
			++recentFailures;
		}
	}
	if ( recentFailures > sv_rcon_minfailures.GetInt() )
	{
		ConMsg( "Banning %s for rcon hacking attempts\n", failedRcon->adr.ToString( true ) );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "addip %i %s\n", sv_rcon_banpenalty.GetInt(), failedRcon->adr.ToString( true ) ) );
		Cbuf_Execute();
		return true;
	}

	return false;
}

bool CRConServer::BCloseAcceptedSocket( ra_listener_id listener )
{
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = 0; i < nCount; i++ )
	{
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		if ( pSocketData->listenerID == listener )
		{
			m_Socket.CloseAcceptedSocket( i );
			return true;
		}
	}
	return false;
}
