// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: implementation of the rcon client

import std;
import ks.tier1;
#include "net_ws_headers.h"

#include "cl_rcon.h"
#include "vprof_engine.h"
#include "proto_oob.h" // PORT_RCON define
#include "cmd.h"
#include "tier2/fileutils.h"
#include "zip/XUnzip.h"



static CRConClient g_RCONClient;
CRConClient & RCONClient()
{
	return g_RCONClient;
}

static void RconAddressChanged_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
#ifndef DEDICATED
	ConVarRef var( pConVar );
	netadr_t to;
	const char *cmdargs = var.GetString(); 
	if ( !NET_StringToAdr( cmdargs, &to ) )
	{
		Msg( "Unable to resolve rcon address %s\n", var.GetString() );
		return;
	}
	RCONClient().SetAddress( to );
#endif
}

static ConVar	rcon_address( "rcon_address", "", FCVAR_SERVER_CANNOT_QUERY | FCVAR_DONTRECORD | FCVAR_RELEASE, "Address of remote server if sending unconnected rcon commands (format x.x.x.x:p) ", RconAddressChanged_f );



//-----------------------------------------------------------------------------
// Implementation of remote vprof
//-----------------------------------------------------------------------------
CRConVProfExport::CRConVProfExport()
{
}

void CRConVProfExport::AddListener()
{
}

void CRConVProfExport::RemoveListener()
{
}

void CRConVProfExport::SetBudgetFlagsFilter( int filter )
{
}

int CRConVProfExport::GetNumBudgetGroups()
{
	return std::ssize( m_Info );
}

void CRConVProfExport::GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos )
{
	std::memcpy( pInfos, m_Info.data(), GetNumBudgetGroups() * sizeof(CExportedBudgetGroupInfo) );
}

void CRConVProfExport::GetBudgetGroupTimes( float times[IVProfExport::MAX_BUDGETGROUP_TIMES] )
{
	int nGroups = std::min<int>( std::ssize( m_Times ), IVProfExport::MAX_BUDGETGROUP_TIMES );
	std::memset( times, 0, nGroups * sizeof(float) );
	nGroups = std::min( GetNumBudgetGroups(), nGroups );
	std::memcpy( times, m_Times.data(), nGroups * sizeof(float) );
}

void CRConVProfExport::PauseProfile()
{
	// NOTE: This only has effect when testing on a listen server
	// it shouldn't do anything in the wild. When drawing the budget panel
	// this will cause the time spent doing so to not be counted
	VProfExport_Pause();
}

void CRConVProfExport::ResumeProfile()
{
	// NOTE: This only has effect when testing on a listen server
	// it shouldn't do anything in the wild
	VProfExport_Resume();
}		

void CRConVProfExport::CleanupGroupData()
{
	int nCount = std::ssize( m_Info );
	for ( int i = 0; i < nCount; ++i )
	{
		delete[] m_Info[i].m_pName;
	}

	m_Info.clear();
}

void CRConVProfExport::OnRemoteGroupData( const void *data, int len )
{
	ks::byte_reader buf( data, len );
	int nFirstGroup = buf.read<int>();

	if ( nFirstGroup == 0 )
	{
		CleanupGroupData();
	}
	else
	{
		contract_assert( nFirstGroup == std::ssize( m_Info ) );
	}

	// NOTE: See WriteRemoteVProfGroupData in vprof_engine.cpp
	// to see the encoding of this data
	int nGroupCount = buf.read<int>();
	int nBase = std::ssize( m_Info ); m_Info.resize( m_Info.size() + nGroupCount );
	char temp[1024];
	for ( int i = 0; i < nGroupCount; ++i )
	{
		CExportedBudgetGroupInfo *pInfo = &m_Info[nBase + i];

		unsigned char red, green, blue, alpha;
		red = buf.read<unsigned char>();
		green = buf.read<unsigned char>();
		blue = buf.read<unsigned char>();
		alpha = buf.read<unsigned char>();
		buf.read_cstring( temp, sizeof(temp) );
		int nLen = V_strlen( temp );

		pInfo->m_Color.SetColor( red, green, blue, alpha );
		char *pBuf = new char[ nLen + 1 ];
		pInfo->m_pName = pBuf;
		std::memcpy( pBuf, temp, nLen+1 );
		pInfo->m_BudgetFlags = 0;
	}
}

void CRConVProfExport::OnRemoteData( const void *data, int len )
{
	// NOTE: See WriteRemoteVProfData in vprof_engine.cpp
	// to see the encoding of this data
	int nCount = len / sizeof(float);
	contract_assert( nCount == std::ssize( m_Info ) );

	m_Times.assign( nCount, {} );
	std::memcpy( m_Times.data(), data, nCount * sizeof(float) );
}


static void vprof_remote_start( const CCommand &args );
static ConCommand vprof_remote_start_command( "vprof_remote_start", vprof_remote_start, "Request a VProf data stream from the remote server (requires authentication)" );
static void vprof_remote_start( const CCommand &args )
{
	// TODO: Make this work (it might already!)
//	RCONClient().StartVProfData();
}

static void vprof_remote_stop( const CCommand &args );
static ConCommand vprof_remote_stop_command( "vprof_remote_stop", vprof_remote_stop, "Stop an existing remote VProf data request" );
static void vprof_remote_stop( const CCommand &args )
{
	// TODO: Make this work (it might already!)
//	RCONClient().StopVProfData();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

CRConClient::CRConClient() : m_Socket( this )
{
	m_bAuthenticated = false;
	m_iAuthRequestID = 1; // must start at 1
	m_iReqID = 0;
	m_nScreenShotIndex = 0;
	m_nConsoleLogIndex = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRConClient::~CRConClient()
{
}


//-----------------------------------------------------------------------------
// Changes the password
//-----------------------------------------------------------------------------
void CRConClient::SetPassword( const char *pPassword )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Password = ks::nz( pPassword );
}

void CRConClient::SetRemoteFileDirectory( const char *pDir )
{
	m_RemoteFileDir = ks::nz( pDir );
	m_nScreenShotIndex = 0;
	m_nConsoleLogIndex = 0;
	g_pFullFileSystem->CreateDirHierarchy( pDir, "MOD" );
}


//-----------------------------------------------------------------------------
// Purpose: set the addresss of the remote server
//-----------------------------------------------------------------------------
void CRConClient::SetAddress( const netadr_t &netAdr )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Address = netAdr;
	if ( m_Address.GetPort() == 0  )
	{
		m_Address.SetPort( PORT_SERVER ); // override the port setting, by default rcon tries to bind to the same port as the server
	}
}


//-----------------------------------------------------------------------------
// Inherited from ISocketCreatorListener
//-----------------------------------------------------------------------------
bool CRConClient::ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr )
{
	// Can't connect if we're already connected
	return !IsConnected();
}

void CRConClient::OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData )
{
}

void CRConClient::OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData )
{
	// reset state
	m_bAuthenticated = false;
	m_iReqID = 0;
	m_iAuthRequestID = 1; // must start at 1
	m_SendBuffer.clear();
	m_RecvBuffer.clear();
}



//-----------------------------------------------------------------------------
// Connects to the address specified by SetAddress
//-----------------------------------------------------------------------------
bool CRConClient::ConnectSocket()
{
	if ( m_Socket.ConnectSocket( m_Address, true ) < 0 )
	{
		Warning( "Unable to connect to remote server (%s)\n", m_Address.ToString() );
		return false;
	}
	return true;
}

void CRConClient::CloseSocket()
{
	m_Socket.CloseAllAcceptedSockets();
}


//-----------------------------------------------------------------------------
// Are we connected?
//-----------------------------------------------------------------------------
bool CRConClient::IsConnected()	const
{
	return m_Socket.GetAcceptedSocketCount() > 0;
}


//-----------------------------------------------------------------------------
// Creates a listen server, connects to remote machines that connect to it
//-----------------------------------------------------------------------------
void CRConClient::CreateListenSocket( const netadr_t &netAdr )
{
	m_Socket.CreateListenSocket( netAdr );
}

void CRConClient::CloseListenSocket()
{
	m_Socket.CloseListenSocket( );
}


//-----------------------------------------------------------------------------
// Purpose: send queued messages
//-----------------------------------------------------------------------------
void CRConClient::SendQueuedData()
{
	SocketHandle_t hSocket = GetSocketHandle();
	ks::byte_reader reader( m_SendBuffer );
	while ( reader.remaining() > sizeof(int) )
	{
		std::size_t nSize = reader.peek<int>();
		contract_assert( nSize >= reader.remaining() - sizeof( int ) );
		ks::platform::socket_error error = ks::platform::socket_error::none;
		long ret = ks::platform::send_bytes( hSocket, reader.rest().data(), nSize + sizeof( int ), &error );
		if ( ret != -1 )
		{
			reader.skip( nSize + sizeof( int ) );
			continue;
		}

		if ( error != ks::platform::socket_error::again )
		{
			Warning( "Lost RCON connection, please retry command.\n"); 
			CloseSocket();
		}
		break;
	}

	// CloseSocket() above empties the buffer
	const std::size_t nSizeRemaining = m_SendBuffer.empty() ? 0 : reader.remaining();
	if ( nSizeRemaining <= sizeof(int) )
	{
		m_SendBuffer.clear();
		return;
	}

	// In this case, we've still got queued messages to send
	// Keep the portion of the buffer we didn't process for next time
	m_SendBuffer.erase( m_SendBuffer.begin(), m_SendBuffer.begin() + reader.tell() );
}


//-----------------------------------------------------------------------------
// Purpose: parse received data
//-----------------------------------------------------------------------------
void CRConClient::ParseReceivedData()
{
	ks::byte_reader reader( m_RecvBuffer );

	int size = reader.read<int>();
	while ( size && size <= int( reader.remaining() ) )
	{
		//DevMsg( "RCON: got packet %i long\n", size );
		int reqID = reader.read<int>();
		int cmdID = reader.read<int>(); // ignore the cmd id
		//		DevMsg( "RCON Cmd: <-- %i %i %i\n", reqID, cmdID, readLen );

		switch( cmdID )
		{
		case SERVERDATA_AUTH_RESPONSE:
			{
				if ( reqID == -1 ) // bad password
				{
					Msg( "Bad RCON password\n" );
					m_bAuthenticated = false;
				}
				else
				{
					contract_assert( reqID == m_iAuthRequestID );
					m_bAuthenticated = true;
				}
				char dummy[2];
				reader.read_cstring( dummy, sizeof(dummy) );
				reader.read_cstring( dummy, sizeof(dummy) );
			}
			break;

		case SERVERDATA_SCREENSHOT_RESPONSE:
			{
				int nDataSize = reader.read<int>();
				SaveRemoteScreenshot( reader.rest().data(), nDataSize );
				reader.skip( nDataSize );
			}
			break;

		case SERVERDATA_CONSOLE_LOG_RESPONSE:
			{
				int nDataSize = reader.read<int>();
				SaveRemoteConsoleLog( reader.rest().data(), nDataSize );
				reader.skip( nDataSize );
			}
			break;

		case SERVERDATA_VPROF_DATA:
			{
				int nDataSize = reader.read<int>();
				m_VProfExport.OnRemoteData( reader.rest().data(), nDataSize );
				reader.skip( nDataSize );
			}
			break;

		case SERVERDATA_VPROF_GROUPS:
			{
				int nDataSize = reader.read<int>();
				m_VProfExport.OnRemoteGroupData( reader.rest().data(), nDataSize );
				reader.skip( nDataSize );
			}
			break;

		case SERVERDATA_RESPONSE_STRING:
			{
				char pBuf[2048];
				reader.read_cstring( pBuf, sizeof(pBuf) );
				Msg( "%s", pBuf );
			}
			break;
		case SERVERDATA_RESPONSE_REMOTEBUG:
			{
				// Our connected server has submitted a partial bug report
				// to a directory. Start a bug report and populate the fields
				// with the data in the provided directory.
				char pBuf[MAX_PATH];
				reader.read_cstring( pBuf, sizeof(pBuf) );

				std::string bugcmd;
				bugcmd = ks::format_printf( "bug -remotebugpath %s",pBuf );

				Cbuf_AddText( CBUF_SERVER, bugcmd.data() );
				Cbuf_Execute();
			}
			break;

		default:
			{
				// Displays a message from the server
				int strLen = reader.remaining();
				std::vector<char> msg( strLen + 1 );
				reader.read_cstring( msg.data(), std::ssize( msg ) );

				msg[ std::ssize( msg ) - 1 ] = '\0';
				Msg( "%s", (const char *)msg.data() );
				reader.read_cstring( msg.data(), std::ssize( msg ) ); // ignore the second string
			}
			break;
		}

		if ( reader.remaining() >= sizeof(int) )
		{
			size = reader.read<int>(); // read how much is in this packet
		}
		else
		{
			size = 0; // finished the packet
		}
	}

	if ( size || reader.remaining() > 0 )
	{
		// In this case, we've got a partial message; we didn't get it all.
		// Keep the portion of the buffer we didn't process for next time
		ks::byte_writer leftover;
		if ( size > 0 )
		{
			leftover.write<int>( size );
		}
		leftover.write_bytes( reader.rest() );
		m_RecvBuffer = leftover.release();
	}
	else
	{
		m_RecvBuffer.clear();
	}
}


//-----------------------------------------------------------------------------
// Purpose: check for any server responses
//-----------------------------------------------------------------------------
void CRConClient::RunFrame()
{
	ks::ScratchArena<> scratch;
	m_Socket.RunFrame();

	if ( !IsConnected() )
		return;

	SendQueuedData();

	SocketHandle_t hSocket = GetSocketHandle();
	char ch;
	ks::platform::socket_error error = ks::platform::socket_error::none;
	long pendingLen = ks::platform::peek_bytes( hSocket, &ch, sizeof(ch), &error );
	if ( pendingLen == -1 && error == ks::platform::socket_error::again )
		return;

	if ( pendingLen == 0 ) // socket got closed
	{
		CloseSocket();
		return;
	}
	
	if ( pendingLen < 0 )
	{
		CloseSocket();
		Warning( "Lost RCON connection, please retry command (%s)\n", NET_ErrorString( error ) );
		return;
	}

	// find out how much we have to read
	unsigned long readLen = ks::platform::bytes_available( hSocket );
	if ( readLen <= sizeof(int) ) 
		return;

	// we have a command to process
	// Read data into the receive buffer
	m_RecvBuffer.reserve( m_RecvBuffer.size() + readLen + 1 );
	char *recvbuffer = (char *)scratch.AllocBytes( ( std::min<std::common_type_t<int, long unsigned int>>( 1024, readLen + 1 ) ) );
	unsigned int len = 0;
	while ( len < readLen )
	{
		long recvLen = ks::platform::receive_bytes( hSocket, recvbuffer , std::min<std::common_type_t<int, long unsigned int>>( 1024, readLen - len ) , &error );
		if ( recvLen == 0 ) // socket was closed
		{
			CloseSocket();
			break;
		}
		
		if ( recvLen < 0 && error != ks::platform::socket_error::again )
		{
			Warning( "RCON Cmd: recv error (%s)\n", NET_ErrorString( error ) );
			break;
		}

		if ( recvLen > 0 )
		{
			const auto received = ks::byte_view( recvbuffer, recvLen );
			m_RecvBuffer.insert( m_RecvBuffer.end(), received.begin(), received.end() );
		}
		len += recvLen;
	}
	
	ParseReceivedData();
}


//-----------------------------------------------------------------------------
// Purpose: send a response to the server
//-----------------------------------------------------------------------------
void CRConClient::SendResponse( ks::byte_writer &response, bool bAutoAuthenticate )
{
	if ( bAutoAuthenticate && !IsAuthenticated() )
	{
		Authenticate();
		if ( IsConnected() )
		{
			m_SendBuffer.insert( m_SendBuffer.end(), response.bytes().begin(), response.bytes().end() );
		}
		return;
	}

	ks::platform::socket_error error = ks::platform::socket_error::none;
	long ret = ks::platform::send_bytes( GetSocketHandle(), response.data(), response.size(), &error );
	if ( ret == -1 )
	{
		if ( error == ks::platform::socket_error::again )
		{
			m_SendBuffer.insert( m_SendBuffer.end(), response.bytes().begin(), response.bytes().end() );
		}
		else
		{
			Warning( "Lost RCON connection, please retry command\n" ); 
			CloseSocket();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: builds a simple command to send to the server
//-----------------------------------------------------------------------------
void CRConClient::BuildResponse( ks::byte_writer &response, ServerDataRequestType_t msg, const char *pString1, const char *pString2 )
{
	// build the response
	response.write<int>(0); // the size, filled in below
	response.write<int>(m_iReqID++);
	response.write<int>(msg);
	response.write_cstring(ks::nz(pString1));
	response.write_cstring(ks::nz(pString2));
	int nSize = response.size() - sizeof(int); 
	response.seek( 0 );
	response.write<int>( nSize ); // the size
}


//-----------------------------------------------------------------------------
// Purpose: authenticate ourselves
//-----------------------------------------------------------------------------
void CRConClient::Authenticate()
{
	ks::byte_writer response;

	// build the response
	response.write<int>(0); // the size, filled in below
	response.write<int>(++m_iAuthRequestID);
	response.write<int>(SERVERDATA_AUTH);
	response.write_cstring( m_Password );

	// Use the otherwise-empty second string for the userid.  The server will use this to
	// exec "mp_disable_autokick <userid>" upon successful authentication.
	bool addedUserID = false;
	if ( GetBaseLocalClient().IsConnected() )
	{
		if ( GetBaseLocalClient().m_nPlayerSlot < GetBaseLocalClient().m_nMaxClients && GetBaseLocalClient().m_nPlayerSlot >= 0 )
		{
			contract_assert( GetBaseLocalClient().m_pUserInfoTable );
			if ( GetBaseLocalClient().m_pUserInfoTable )
			{
				player_info_t *pi = (player_info_t*) GetBaseLocalClient().m_pUserInfoTable->GetStringUserData( GetBaseLocalClient().m_nPlayerSlot, nullptr );
				if ( pi )
				{
					addedUserID = true;
					// Fixup from network order (little endian)
					response.write_cstring( va( "%d", ( pi->userID ) ) );
				}
			}
		}
	}

	if ( !addedUserID )
	{
		response.write_cstring( "" );
	}
	int size = response.size() - sizeof(int); 
	response.seek( 0 );
	response.write<int>(size); // the size

	SendResponse( response, false );
}


//-----------------------------------------------------------------------------
// Purpose: send an rcon command to a connected server
//-----------------------------------------------------------------------------
void CRConClient::SendCmd( const char *msg )
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_EXECCOMMAND, msg, "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: Start vprofiling
//-----------------------------------------------------------------------------
void CRConClient::StartVProfData()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	// Override the vprof export to point to our local profiling data
	OverrideVProfExport( &m_VProfExport );

	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_VPROF, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: Stop vprofiling
//-----------------------------------------------------------------------------
void CRConClient::StopVProfData()
{
	// Reset the vprof export to point to the normal profiling data
	ResetVProfExport( &m_VProfExport );

	// Don't bother restarting a connection to turn this off
	if ( !IsConnected() )
		return;

	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_REMOVE_VPROF, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: get data from the server
//-----------------------------------------------------------------------------
void CRConClient::TakeScreenshot()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_TAKE_SCREENSHOT, "", "" );
	SendResponse( response );
}

void CRConClient::GrabConsoleLog()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_SEND_CONSOLE_LOG, "", "" );
	SendResponse( response );
}

void CRConClient::SendBugRequest()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
		{
			Warning( "Could not connect to remote machine, remote bug command failed\n" );
			return;
		}
	}
	
	ks::byte_writer response;
	BuildResponse( response, SERVERDATA_SEND_REMOTEBUG, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// We've got data from the server, save it
//-----------------------------------------------------------------------------
void CRConClient::SaveRemoteScreenshot( const void* pBuffer, int nBufLen )
{
	char pScreenshotPath[MAX_PATH];
	do 
	{
		V_snprintf( pScreenshotPath, sizeof( pScreenshotPath ), "%s/screenshot%04d.jpg", m_RemoteFileDir.data(), m_nScreenShotIndex++ );	
	} while ( g_pFullFileSystem->FileExists( pScreenshotPath, "MOD" ) );

	char pFullPath[MAX_PATH];
	GetModSubdirectory( pScreenshotPath, pFullPath, sizeof(pFullPath) );
	HZIP hZip = OpenZip( (void*)pBuffer, nBufLen, ZIP_MEMORY );

	int nIndex;
	ZIPENTRY zipInfo;
	FindZipItemA( hZip, "screenshot.jpg", true, &nIndex, &zipInfo );
	if ( nIndex >= 0 )
	{
		UnzipItem( hZip, nIndex, pFullPath, 0, ZIP_FILENAME );
	}
	CloseZipU( hZip );
}

void CRConClient::SaveRemoteConsoleLog( const void* pBuffer, int nBufLen )
{
	if ( nBufLen == 0 )
		return;

	char pLogPath[MAX_PATH];
	do 
	{
		V_snprintf( pLogPath, sizeof( pLogPath ), "%s/console%04d.log", m_RemoteFileDir.data(), m_nConsoleLogIndex++ );	
	} while ( g_pFullFileSystem->FileExists( pLogPath, "MOD" ) );

	char pFullPath[MAX_PATH];
	GetModSubdirectory( pLogPath, pFullPath, sizeof(pFullPath) );
	HZIP hZip = OpenZip( (void*)pBuffer, nBufLen, ZIP_MEMORY );

	int nIndex;
	ZIPENTRY zipInfo;
	FindZipItemA( hZip, "console.log", true, &nIndex, &zipInfo );
	if ( nIndex >= 0 )
	{
		UnzipItem( hZip, nIndex, pFullPath, 0, ZIP_FILENAME );
	}
	CloseZipU( hZip );
}
