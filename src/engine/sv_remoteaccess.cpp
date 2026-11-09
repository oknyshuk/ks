// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Handles all the functions for implementing remote access to the engine

// main precompiled header for server files

import ks.tier1;
import std;

#include "common.h"
#include "qlimits.h"
#include "quakedef.h"
#include "decal.h"
#include "host_cmd.h"
#include "cmodel_engine.h"
#include "sound.h"
#include "EngineSoundInternal.h"
#include "checksum_engine.h"
#include "host.h"
#include "keys.h"
#include "vengineserver_impl.h"
#include "server.h"
#include "eiface.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "console.h"
#include "edict.h"
#include "net.h"
#include "igameserverdata.h"
#include "sv_remoteaccess.h"
#include "sv_rcon.h"
#include "sv_filter.h"
#include "sys.h"
#include "sys_dll.h"
#include "vprof_engine.h"
#include "PlayerState.h"
#ifndef DEDICATED
#include "zip/XZip.h"
#endif
#include "cl_main.h"
#include "facade/igameserverdata.inl"

extern IServerGameDLL	*serverGameDLL;

CServerRemoteAccess g_ServerRemoteAccess;

ConVar sv_rcon_log( "sv_rcon_log", "1", 0, "Enable/disable rcon logging." );

//-----------------------------------------------------------------------------
// Host_Stats_f - prints out interesting stats about the server...
//-----------------------------------------------------------------------------
void Host_Stats_f (void)
{
	char stats[512];
	g_ServerRemoteAccess.GetStatsString(stats, sizeof(stats));
	ConMsg("  CPU   NetIn   NetOut    Uptime  Maps   FPS   Players  Svms    +-ms   ~tick\n%s\n", stats);
	//     "--cpu- ++++in++ +++out++ ---up-- ++cl+ ---fps- ++user+ ~~hms~~ ~~dev~~ ~tdev~~"
}
static ConCommand stats("stats", Host_Stats_f, "Prints server performance variables" );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerRemoteAccess::CServerRemoteAccess()
{
	m_iBytesSent = 0;
	m_iBytesReceived = 0;
	m_NextListenerID = 0;
	m_AdminUIID = INVALID_LISTENER_ID;
	m_nScreenshotListener = -1;
	m_nBugListener = -1;
}

//-----------------------------------------------------------------------------
// Purpose: unique id to associate data transfers with sessions
//-----------------------------------------------------------------------------
ra_listener_id CServerRemoteAccess::GetNextListenerID( bool authConnection, const netadr_t *adr )
{
	int i = std::ssize( m_ListenerIDs );
	ListenerStore_t &store = m_ListenerIDs.emplace_back();
	store.listenerID = i;
	store.authenticated = !authConnection;
	store.m_bHasAddress = ( adr != nullptr );
	if ( adr )
	{
		store.adr = *adr;
	}
	return i;
}


bool GetStringHelper( ks::byte_reader & cmd, char *outBuf, int bufSize )
{
	outBuf[0] = 0;
	cmd.read_cstring(outBuf, bufSize);
	if ( !cmd.ok() )
	{
		cmd.seek( cmd.size() );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: handles a request
//-----------------------------------------------------------------------------
void CServerRemoteAccess::WriteDataRequest( CRConServer *pNetworkListener, ra_listener_id listener, const void *buffer, int bufferSize)
{
	m_iBytesReceived += bufferSize;
	// ConMsg("RemoteAccess: bytes received: %d\n", m_iBytesReceived);

	if ( bufferSize < 2*sizeof(int) ) // check that the buffer contains at least the id and type
	{
		return;
	}

	ks::byte_reader cmd(buffer, bufferSize);
	bool invalidRequest = false;

	while ( invalidRequest == false && (int)cmd.tell() < (int)(cmd.size() - 2 * sizeof(int) ) ) // while there is commands to read
	{
		// parse out the buffer
		int requestID = cmd.read<int>();
		pNetworkListener->SetRequestID( listener, requestID ); // tell the rcon server the ID so it can reflect it when the console redirect flushes
		int requestType = cmd.read<int>();

		switch (requestType)
		{
			case SERVERDATA_REQUESTVALUE:
				{
					if ( IsAuthenticated(listener) )
					{
						char variable[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						RequestValue( listener, requestID, variable);
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
					}
					else
					{
						char variable[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
					}
				}
				break;

			case SERVERDATA_SETVALUE:
				{
					if ( IsAuthenticated(listener) )
					{
						char variable[256];
						char value[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, value, sizeof(value) ) )
						{
							invalidRequest = true;
							break;
						}
						SetValue(variable, value);
					}
					else
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
					}
				}
				break;

			case SERVERDATA_EXECCOMMAND:
				{
					if ( IsAuthenticated(listener) )
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						
						ExecCommand(command);
						
						if ( listener != m_AdminUIID )
						{
							LogCommand( listener, va( "command \"%s\"", command) );
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
					}
					else
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						LogCommand( listener, "Bad Password" );
					}
				}
				break;

			case SERVERDATA_AUTH:
				{
					char password[512];
					if ( !GetStringHelper( cmd, password, sizeof( password ) ) )
					{
						invalidRequest = true;
						break;
					}
					CheckPassword( pNetworkListener, listener, requestID, password );
					if ( !GetStringHelper( cmd, password, sizeof( password ) ) )
					{
						invalidRequest = true;
						break;
					}

					if ( m_ListenerIDs[ listener ].authenticated )
					{
						// if the second string has a non-zero value, it is a userid.
						int userID = std::atoi( password );
						ConCommandBase *cmd = g_pCVar->FindCommand( "mp_disable_autokick" );
						if ( cmd )
						{
							Cbuf_AddText( CBUF_SERVER, va( "mp_disable_autokick %d\n", userID ) );
							Cbuf_Execute();
						}
					}
				}
				break;

			case SERVERDATA_TAKE_SCREENSHOT:
#ifndef DEDICATED
				m_nScreenshotListener = listener;
				CL_TakeJpeg( );
#endif
				break;

			case SERVERDATA_SEND_CONSOLE_LOG:
				{
#ifndef DEDICATED
					ks::text_writer buf;
					if ( GetConsoleLogFileData( buf ) )
					{
						HZIP hZip = CreateZipZ( 0, 1024 * 1024, ZIP_MEMORY );
						void *pMem = nullptr;
						unsigned long nLen;
						ZipAdd( hZip, "console.log", (void *)buf.c_str(), buf.size(), ZIP_MEMORY );
						ZipGetMemory( hZip, &pMem, &nLen );
						SendResponseToClient( listener, SERVERDATA_CONSOLE_LOG_RESPONSE, pMem, nLen );
						CloseZipZ( hZip );
					}
					else
					{
						LogCommand( listener, "Failed to read console log!\n" );
						RespondString( listener, requestID, "Failed to read console log!\n" );
					}
#endif
				}
				break;

			case SERVERDATA_VPROF:
				{
					char password[25];
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( IsAuthenticated(listener) )
					{
						RegisterVProfDataListener( listener );
						LogCommand( listener, "Remote VProf started!\n" );
						RespondString( listener, requestID, "Remote VProf started!\n" );
					}
				}
				break;
		
			case SERVERDATA_REMOVE_VPROF:
				{
					char password[25];
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( IsAuthenticated(listener) )
					{
						RemoveVProfDataListener( listener );
						LogCommand( listener, "Remote VProf finished!\n" );
						RespondString( listener, requestID, "Remote VProf finished!\n" );
					}
				}
				break;
			case SERVERDATA_SEND_REMOTEBUG:
				{
					if ( CommandLine()->CheckParm( "-remotebug" ) == nullptr )
					{
						Warning( "Received a remote bug request from rcon client, but not running with '-remotebug'. Ignoring.\n" );
						RespondString( listener, 0, "Remote machine using wrong bugreporter dll. Try running with '-remotebug'\n" );
						invalidRequest = true;
						break;
					}

					if ( IsAuthenticated(listener) )
					{
						ExecCommand("bug -auto");
						LogCommand( listener, "Remote bug submission\n" );
						m_nBugListener = listener;
					}
				}
				break;

			default:
				contract_assert( !("Unknown requestType in CServerRemoteAccess::WriteDataRequest()") );
				cmd.seek( cmd.size() );
				invalidRequest = true;
				break;
		};
	}
}

// NOTE: This version is used by the server DLL or server plugins
void CServerRemoteAccess::WriteDataRequest( ra_listener_id listener, const void *buffer, int bufferSize )
{
	WriteDataRequest( &RCONServer(), listener, buffer, bufferSize );
}

void CServerRemoteAccess::RemoteBug( const char *pBugPath )
{
	if ( m_nBugListener < 0 )
		return;

	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = m_nBugListener;

	ks::byte_writer &response = newResponse.packet;

	response.write<int>(0);
	response.write<int>(SERVERDATA_RESPONSE_REMOTEBUG);
	response.write_cstring(ks::nz(pBugPath));

	m_nBugListener = -1;
}

//-----------------------------------------------------------------------------
// Uploads a screenshot to a particular listener
//-----------------------------------------------------------------------------
void CServerRemoteAccess::UploadScreenshot( const char *pFileName )
{
#ifndef DEDICATED
	if ( m_nScreenshotListener < 0 )
		return;

	if ( auto buf = g_pFullFileSystem->ReadFile( pFileName, "MOD" ) )
	{
		HZIP hZip = CreateZipZ( 0, 1024 * 1024, ZIP_MEMORY );
		void *pMem = nullptr;
		unsigned long nLen;
		ZipAdd( hZip, "screenshot.jpg", buf->data(), buf->size(), ZIP_MEMORY );
		ZipGetMemory( hZip, &pMem, &nLen );
		SendResponseToClient( m_nScreenshotListener, SERVERDATA_SCREENSHOT_RESPONSE, pMem, nLen );
		CloseZipZ( hZip );
	}
	else
	{
		LogCommand( m_nScreenshotListener, "Failed to read screenshot!\n" );
		RespondString( m_nScreenshotListener, 0, "Failed to read screenshot!\n" );
	}

	m_nScreenshotListener = -1;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: log information about a command that ran
//-----------------------------------------------------------------------------
void CServerRemoteAccess::LogCommand( ra_listener_id listener, const char *msg )
{
	if ( !sv_rcon_log.GetBool() )
		return;
		
	if ( listener < (ra_listener_id)std::ssize( m_ListenerIDs ) && m_ListenerIDs[listener].m_bHasAddress )
	{
		Log_Msg( LOG_SERVER_LOG, "rcon from \"%s\": %s\n", m_ListenerIDs[listener].adr.ToString(), msg );
	}
	else
	{
		Log_Msg( LOG_SERVER_LOG, "rcon from \"unknown\": %s\n", msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: checks if this user has provided the correct password
//-----------------------------------------------------------------------------
void CServerRemoteAccess::CheckPassword( CRConServer *pNetworkListener, ra_listener_id listener, int requestID, const char *password )
{
	// If the pw does not match, then not authed
	if ( !pNetworkListener->IsPassword( password ) )
	{
		BadPassword( pNetworkListener, listener );
		return;
	}

		// allocate a spot in the list for the response
	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = listener; // record who we need to respond to

	ks::byte_writer &response = newResponse.packet;

	// build the response
	response.write<int>(requestID);
	response.write<int>(SERVERDATA_AUTH_RESPONSE);
	response.write_cstring("");
	response.write_cstring("");

	m_ListenerIDs[ listener ].authenticated = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if this connection has provided the correct password
//-----------------------------------------------------------------------------
bool CServerRemoteAccess::IsAuthenticated( ra_listener_id listener )
{
	contract_assert( listener >= 0 && listener < (ra_listener_id)std::ssize( m_ListenerIDs ) );
	return m_ListenerIDs[listener].authenticated;
}

extern ConVar sv_rcon_maxfailures;

//-----------------------------------------------------------------------------
// Purpose: send a bad password packet
// Returns true if socket was closed
//-----------------------------------------------------------------------------
void CServerRemoteAccess::BadPassword( CRConServer *pNetworkListener, ra_listener_id listener )
{
	ListenerStore_t& listenerStore = m_ListenerIDs[listener];

	listenerStore.authenticated = false;

	if ( pNetworkListener->HandleFailedRconAuth( listenerStore.adr ) )
	{
		// Close the socket if too many failed attempts
		pNetworkListener->BCloseAcceptedSocket( listener );
	}
	else
	{
		//
		// Respond to the rcon user
		//

		// allocate a spot in the list for the response
		DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
		newResponse.responderID = listener; // record who we need to respond to
		ks::byte_writer &response = newResponse.packet;

		// build the response
		response.write<int>(-1); // special flag for bad password
		response.write<int>(SERVERDATA_AUTH_RESPONSE);
		response.write_cstring("");
		response.write_cstring("");
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns the number of bytes read
//-----------------------------------------------------------------------------
int CServerRemoteAccess::GetDataResponseSize( ra_listener_id listener )
{
	for( DataResponse_t &resp : m_ResponsePackets )
	{
		// copy response into buffer
		if ( resp.responderID != listener ) // not for us, skip to the next entry
			continue;

		ks::byte_writer &response = resp.packet;
		return response.size();
	}
	return 0;
}

int CServerRemoteAccess::ReadDataResponse( ra_listener_id listener, void *buffer, int bufferSize )
{
	for( auto it = m_ResponsePackets.begin(); it != m_ResponsePackets.end(); ++it )
	{
		// copy response into buffer
		if ( it->responderID != listener ) // not for us, skip to the next entry
			continue;

		ks::byte_writer &response = it->packet;
		int bytesToCopy = response.size();
		contract_assert( bufferSize >= bytesToCopy );
		if (bytesToCopy <= bufferSize)
		{
			memcpy(buffer, response.data(), bytesToCopy);
		}
		else
		{
			// not enough room in buffer, don't return message
			bytesToCopy = 0;
		}

		m_iBytesSent += bytesToCopy;
		// ConMsg("RemoteAccess: bytes sent: %d\n", m_iBytesSent);

		// remove from list
		m_ResponsePackets.erase(it);
		// return bytes copied
		return bytesToCopy;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: looks up a cvar and posts a return value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::RequestValue( ra_listener_id listener, int requestID, const char *variable)
{
	// look up the cvar
	ks::text_writer value;
	LookupValue(variable, value);

	// allocate a spot in the list for the response
	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = listener; // record who we need to respond to

	ks::byte_writer &response = newResponse.packet;

	// build the response
	response.write<int>(requestID);
	response.write<int>(SERVERDATA_RESPONSE_VALUE);
	response.write_cstring(ks::nz(variable));

	//Assert(value.TellPut() > 0);
	response.write<int>(value.size());
	response.write_bytes(value.bytes());
}


//-----------------------------------------------------------------------------
// Purpose: looks up a cvar and posts a return value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::RespondString( ra_listener_id listener, int requestID, const char *pString )
{
	// allocate a spot in the list for the response
	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = listener; // record who we need to respond to

	ks::byte_writer &response = newResponse.packet;

	// build the response
	response.write<int>(requestID);
	response.write<int>(SERVERDATA_RESPONSE_STRING);
	response.write_cstring(ks::nz(pString));
}


//-----------------------------------------------------------------------------
// Purpose: Sets a cvar or value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SetValue(const char *variable, const char *value)
{
	// check for special types
	if (!stricmp(variable, "map"))
	{
		// push a map change command
		Cbuf_AddText( CBUF_SERVER, va( "changelevel %s\n", value ) );
		Cbuf_Execute();
	}
	else if (!stricmp(variable, "mapcycle"))
	{
		// write out a new mapcycle file
		ConVarRef mapcycle( "mapcyclefile" );
		if ( mapcycle.IsValid() )
		{
			FileHandle_t f = g_pFileSystem->Open(mapcycle.GetString(), "wt");
			if (!f)
			{
				// mapcycle file probably read only, fall pack to temporary file
				Msg("Couldn't write to read-only file %s, using file _temp_mapcycle.txt instead.\n", mapcycle.GetString());
				mapcycle.SetValue("_temp_mapcycle.txt" );
				f = g_pFileSystem->Open(mapcycle.GetString(), "wt");
				if (!f)
				{
					return;
				}
			}
			g_pFileSystem->Write(value, V_strlen(value) + 1, f);
			g_pFileSystem->Close(f);
		}
	}
	else
	{
		// Stick the cvar set in the command string, so client notification, replication, etc happens
		Cbuf_AddText( CBUF_SERVER, va("%s %s", variable, value) );
		Cbuf_AddText(CBUF_SERVER, "\n");
		Cbuf_Execute();
	}
}

//-----------------------------------------------------------------------------
// Purpose: execs a command
//-----------------------------------------------------------------------------
void CServerRemoteAccess::ExecCommand(const char *cmdString)
{
	Cbuf_AddText(CBUF_SERVER, (char *)cmdString);
	Cbuf_AddText(CBUF_SERVER, "\n");
	Cbuf_Execute();
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable
//-----------------------------------------------------------------------------
bool CServerRemoteAccess::LookupValue(const char *variable, ks::text_writer &value)
{
	ks::ScratchArena<> scratch;
	// first see if it's a cvar
	const char *strval = LookupStringValue(variable);
	if (strval)
	{
		value.put(strval);
		value.put('\0');
	}
	else if (!stricmp(variable, "stats"))
	{
		char stats[512];
		GetStatsString(stats, sizeof(stats));
		value.put(stats);
		value.put('\0');
	}
	else if (!stricmp(variable, "banlist"))
	{
		// returns a list of banned users and ip's
		GetUserBanList(value);
	}
	else if (!stricmp(variable, "playerlist"))
	{
		GetPlayerList(value);		
	}
	else if (!stricmp(variable, "maplist"))
	{
		GetMapList(value);
	}
	else if (!stricmp(variable, "uptime"))
	{
		int timeSeconds = (int)(Plat_FloatTime());
		value.put(timeSeconds);
		value.put('\0');
	}
	else if (!stricmp(variable, "ipaddress"))
	{
		char addr[25];
		V_snprintf( addr, sizeof(addr), "%s:%i", net_local_adr.ToString(true), sv.GetUDPPort());
		value.put( addr );
		value.put('\0');
	}
	else if (!stricmp(variable, "mapcycle"))
	{
		ConVarRef mapcycle( "mapcyclefile" );
		if ( mapcycle.IsValid() )
		{
			// send the mapcycle list file
			FileHandle_t f = g_pFileSystem->Open(mapcycle.GetString(), "rb" );

			if ( f == FILESYSTEM_INVALID_HANDLE )
				return true;
			
			int len = g_pFileSystem->Size(f);
			char *mapcycleData = (char *)scratch.AllocBytes( len+1 );
			if ( len && g_pFileSystem->Read( mapcycleData, len, f ) )
			{
				mapcycleData[len] = 0; // Make sure it's null terminated.
				value.put((const char *)mapcycleData);
				value.put('\0');
			}
			else
			{
				value.put( "" );
				value.put('\0');
			}

			g_pFileSystem->Close( f );


		}
	}
	else
	{
		// value not found, null terminate
		value.put('\0');
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable for simple string values
//-----------------------------------------------------------------------------
const char *CServerRemoteAccess::LookupStringValue(const char *variable)
{
	static char s_ReturnBuf[32];
	ConVar *pVar = g_pCVar->FindVar( variable );
	if ( pVar )
	{
		ConVarRef var( pVar );
		if ( var.IsValid() )
			return var.GetString();
	}

	// special types
	if ( !V_stricmp( variable, "map" ) )
		return sv.GetMapName();

	if ( !V_stricmp( variable, "playercount" ) )
	{
		V_snprintf( s_ReturnBuf, sizeof(s_ReturnBuf) - 1, "%d", sv.GetNumClients() - sv.GetNumProxies());
		return s_ReturnBuf;
	}
	
	if ( !V_stricmp( variable, "maxplayers" ) )
	{
		V_snprintf( s_ReturnBuf, sizeof(s_ReturnBuf) - 1, "%d", sv.GetMaxClients() );
		return s_ReturnBuf;
	}
	
	if ( !V_stricmp( variable, "gamedescription" ) && serverGameDLL )
		return serverGameDLL->GetGameDescription();

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: fills a buffer with a list of all banned IP addresses
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetUserBanList(ks::text_writer &value)
{
	// add user bans
	int i;
	for (i = 0; i < std::ssize( g_UserFilters ); i++)
	{
		value.printf("%i %s : %.3f min\n", i + 1, GetUserIDString(g_UserFilters[i].userid), g_UserFilters[i].banTime);
	}

	// add ip filters
	for (i = 0; i < std::ssize( g_IPFilters ) ; i++)
	{
		unsigned char b[4];
		*(unsigned *)b = g_IPFilters[i].compare;
		value.printf("%i %i.%i.%i.%i : %.3f min\n", i + 1 + (int)std::ssize( g_UserFilters ), b[0], b[1], b[2], b[3], g_IPFilters[i].banTime);
	}

	value.put('\0');
}

void CServerRemoteAccess::GetStatsString(char *buf, int bufSize)
{
	float avgIn=0,avgOut=0;

	sv.GetNetStats( avgIn, avgOut );

	// format: CPU percent, Bandwidth in, Bandwidth out, uptime, changelevels, framerate, total players
	snprintf(buf, bufSize - 1, "%6.1f %8.1f %8.1f %7i %5i %7.2f %7i %7.2f %7.2f %7.2f",
				sv.GetCPUUsage() * 100, 
				avgIn, 
				avgOut,
				(int)(Plat_FloatTime()) / 60,
				sv.GetSpawnCount() - 1,
				1.0/host_frametime, // frame rate
				sv.GetNumClients() - sv.GetNumProxies(),
				1000.0f*host_frameendtime_computationduration, 1000.0f*host_frametime_stddeviation, 1000.0f*host_framestarttime_stddeviation );
	buf[bufSize - 1] = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Fills buffer with details on everyone in the server
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetPlayerList(ks::text_writer &value)
{
	if ( !serverGameClients )
	{
		return;
	}

	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		CGameClient *client = sv.Client(i);
		if ( !client || !client->IsActive() )
			continue;

		CPlayerState *pl = static_cast<CPlayerState *>( serverGameClients->GetPlayerState( client->edict ) );
		if ( !pl )
			continue;

		// valid user, add to buffer
		// format per user, each user seperated by a newline '\n'
		//      "name authID ipAddress ping loss frags time"
		if ( client->IsFakeClient() )
		{
			value.printf("\"%s\" %s 0 0 0 %d 0\n",
				client->GetClientName(),
				client->GetNetworkIDString(),
				pl->score);
		}
		else
		{
			value.printf("\"%s\" %s %s %d %d %d %d\n", 
				client->GetClientName(),
				client->GetNetworkIDString(),
				client->GetNetChannel()->GetAddress(),
				(int)(client->GetNetChannel()->GetAvgLatency(FLOW_OUTGOING) * 1000.0f),
				(int)(client->GetNetChannel()->GetAvgLoss(FLOW_INCOMING)),
				pl->score,
				(int)(client->GetNetChannel()->GetTimeConnected()));
		}
	}

	value.put('\0');
}

//-----------------------------------------------------------------------------
// Purpose: Fills buffer with list of maps from this mod
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetMapList(ks::text_writer &value)
{
	// search the directory structure.
	char mapwild[MAX_QPATH];
	char friendly_com_gamedir[ MAX_OSPATH ];
	std::strcpy(mapwild, "maps/*.bsp");
	V_strncpy( friendly_com_gamedir, com_gamedir, sizeof(friendly_com_gamedir) );
	V_strlower( friendly_com_gamedir );
	
	char const *findfn = Sys_FindFirst( mapwild, nullptr, 0 );
	while ( findfn )
	{
		char curDir[MAX_PATH];
		snprintf(curDir, MAX_PATH, "maps/%s", findfn);
		g_pFileSystem->GetLocalPath(curDir, curDir, MAX_PATH);
		
		// limit maps displayed to ones for the mod only
		if (std::strstr(curDir, friendly_com_gamedir))
		{
			// clean up the map name
			char mapName[MAX_PATH];
			strcpy(mapName, findfn);
			char *extension = strstr(mapName, ".bsp");
			if (extension)
			{
				*extension = 0;
			}

			// write into buffer
			value.put(mapName);
			value.put("\n");
		}
		findfn = Sys_FindNext( nullptr, 0 );
	}

	Sys_FindClose();
	value.put('\0');
}

//-----------------------------------------------------------------------------
// Purpose: sends a message to all the watching admin UI's
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendMessageToAdminUI( ra_listener_id listenerID, const char *message)
{
	if ( listenerID != m_AdminUIID )
	{
		Warning( "ServerRemoteAccess: Sending AdminUI message to non-AdminUI listener\n" );
	}

	// allocate a spot in the list for the response
	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = listenerID; // record who we need to respond to
	ks::byte_writer &response = newResponse.packet;

	// post the message
	response.write<int>(0);
	response.write<int>(SERVERDATA_UPDATE);
	response.write_cstring(ks::nz(message));
}


//-----------------------------------------------------------------------------
// Purpose: Sends a response to the client
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendResponseToClient( ra_listener_id listenerID, ServerDataResponseType_t type, void *pData, int nDataLen )
{
	// allocate a spot in the list for the response
	DataResponse_t &newResponse = m_ResponsePackets.emplace_back();
	newResponse.responderID = listenerID; // record who we need to respond to
	ks::byte_writer &response = newResponse.packet;

	// post the message
	response.write<int>( 0 );
	response.write<int>( type );
	response.write<int>( nDataLen );
	response.write_bytes( pData, nDataLen );
}


//-----------------------------------------------------------------------------
// Purpose: sends an opaque blob of data from VProf to a remote rcon listener
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendVProfData( ra_listener_id listenerID, bool bGroupData, void *data, int len )
{
	SendResponseToClient( listenerID, bGroupData ? SERVERDATA_VPROF_GROUPS : SERVERDATA_VPROF_DATA, data, len );
}

//-----------------------------------------------------------------------------
// Purpose: C function for rest of engine to access CServerRemoteAccess class
//-----------------------------------------------------------------------------
extern "C" void NotifyDedicatedServerUI(const char *message)
{
	if ( g_ServerRemoteAccess.GetAdminUIID() != INVALID_LISTENER_ID ) // if we have an admin UI actually registered
	{
		g_ServerRemoteAccess.SendMessageToAdminUI( g_ServerRemoteAccess.GetAdminUIID(), message);
	}
}
