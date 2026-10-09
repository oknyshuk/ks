// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#define SUPPORT_NET_CONSOLE 1

#if SUPPORT_NET_CONSOLE



import ks.core;

#include "mathlib/expressioncalculator.h"

// main precompiled header for client files

import ks.tier1;
import ks.math;

#include "common.h"
#include "qlimits.h"
#include "iprediction.h"
#include "icliententitylist.h"

#include "sysexternal.h"
#include "cmd.h"
#include "protocol.h"
#include "render.h"


#include "screen.h"

#include "gl_shader.h"

#include "cdll_engine_int.h"
#include "client_class.h"
#include "client.h"
#include "cl_main.h"
#include "cl_pred.h"

#include "con_nprint.h"
#include "debugoverlay.h"
#include "demo.h"
#include "host_state.h"
#include "host.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
import std;
#include "console.h"
#include "netconsole.h"


extern IVEngineClient *engineClient;

#if SUPPORT_NET_CONSOLE
void InitNetConsole( void )
{
	if ( !g_pNetConsoleMgr )
		g_pNetConsoleMgr = new CNetConsoleMgr;
}
#endif

CNetConsoleMgr::CNetConsoleMgr( void ) : m_Socket( this )
{
	m_bActive = false;
	m_bPasswordProtected = false;
	int nPassword = CommandLine()->FindParm( "-netconpassword" );
	if ( nPassword )
	{
		char const *pPassword = CommandLine()->GetParm( nPassword + 1 );
		V_strncpy( m_pPassword, pPassword, sizeof( m_pPassword ) );
		m_bPasswordProtected = true;
	}
	int nPort = CommandLine()->FindParm( "-netconport" );
	if ( nPort )
	{
		char const *pPortNum = CommandLine()->GetParm( nPort + 1 );
		m_Address = net_local_adr;
		int nPortNumber = EvaluateExpression( pPortNum, -1 );
		if ( nPortNumber > 0 )
		{
			m_Address.SetPort( nPortNumber );
			m_bActive = true;
			m_Socket.CreateListenSocket( m_Address, true );
		}
	}
	// now, handle cmds from parent process
	if ( g_nForkID > 0 )
	{
		// set this socket to non-blocking
		ks::platform::set_nonblocking( g_nSocketToParentProcess, nullptr );
		m_ParentConnection.m_hSocket = g_nSocketToParentProcess;
		m_ParentConnection.m_bAuthorized = true;			// no password needed from parent
		m_ParentConnection.m_bInputOnly = true;				// we don't want to spew to here
	}

}

CNetConsoleMgr::CNetConsoleMgr( int nPort ) : m_Socket( this )
{
	m_bPasswordProtected = false;
	m_Address.SetPort( nPort );
	m_bActive = true;
	m_Socket.CreateListenSocket( m_Address, true );
}


static char const s_pszPasswordMessage[]="This server is password protected for console access. Must send PASS command\n\r";

void CNetConsoleMgr::Execute( CConnectedNetConsoleData *pData )
{
	if ( std::memcmp( pData->m_pszInputCommandBuffer, "PASS ", 5 ) == 0 )
	{
		if ( V_strcmp( pData->m_pszInputCommandBuffer + 5, m_pPassword ) == 0 )
			pData->m_bAuthorized = true;
		else
		{
			// bad password
			Warning( "Bad password attempt from net console\n" );
			pData->m_bAuthorized = false;
		}
	}
	else
	{
		if ( pData->m_bAuthorized )
		{
#ifdef DEDICATED
			Cbuf_AddText(CBUF_SERVER, pData->m_pszInputCommandBuffer, kCommandSrcUserInput );
			Cbuf_Execute();
#else
			engineClient->ClientCmd_Unrestricted( pData->m_pszInputCommandBuffer, true );
#endif
		}
		else
		{
			SocketHandle_t hSocket = pData->m_hSocket;
			ks::platform::send_bytes( hSocket, s_pszPasswordMessage, std::strlen( s_pszPasswordMessage ), nullptr );
		}
	}
}

void CNetConsoleMgr::SendStringToNetConsoles( char const *pString )
{
	ks::ScratchArena<> scratch;
	m_Socket.RunFrame();
	int nCount = NumConnectedSockets();
	if ( nCount )
	{
		// lets add the lf to any cr's
		char *pTmp = (char * ) scratch.AllocBytes( std::strlen( pString ) * 2 + 1 );
		char *oString = pTmp;
		char const *pIn = pString;
		while ( *pIn )
		{
			if ( *pIn  == '\n' )
				*( oString++ ) = '\r';
			*( oString++ ) = *( pIn++ );
		} 
		*( oString++ ) = 0;

		for ( int i = 0; i < nCount; i++ )
		{
			CConnectedNetConsoleData *pData = GetConnection( i );
			if ( pData->m_bAuthorized && ( ! pData->m_bInputOnly ) ) // no output to un-authed net consoles
			{
				ks::platform::send_bytes( pData->m_hSocket, pTmp, oString - pTmp - 1, nullptr );
			}
		}
	}
}

void CNetConsoleMgr::RunFrame( void )
{
	// check for incoming data
	m_Socket.RunFrame();
	int nCount = NumConnectedSockets();
	for ( int i = nCount - 1; i >= 0; i-- )
	{
		CConnectedNetConsoleData *pData = GetConnection( i );
		SocketHandle_t hSocket = pData->m_hSocket;
		char ch;
		ks::platform::socket_error error = ks::platform::socket_error::none;
		long pendingLen = ks::platform::peek_bytes( hSocket, &ch, sizeof(ch), &error );
		if ( pendingLen == -1 && error == ks::platform::socket_error::again )
			continue;
			
		if ( pendingLen <= 0 )							// eof or error
		{
			CloseConnection( i );
			continue;
		}

		// find out how much we have to read
		unsigned long readLen = ks::platform::bytes_available( hSocket );
		while( readLen > 0  )
		{
			char recvBuf[256];
			long recvLen = ks::platform::receive_bytes( hSocket, recvBuf , std::min( sizeof( recvBuf ) , readLen ), &error );
			if ( recvLen == 0 ) // socket was closed
			{
				CloseConnection( i );
				break;
			}
				
			if ( recvLen < 0 && error != ks::platform::socket_error::again )
			{
				break;
			}
				
			readLen -= recvLen;
			// now, lets write what we've got into the command buffer
			HandleInputChars( recvBuf, recvLen, pData );
		}
	}
}


void CNetConsoleMgr::HandleInputChars( char const *pIn, int recvLen, CConnectedNetConsoleData *pData )
{
	while( recvLen )
	{
		switch( *pIn )
		{
			case '\r':
			case '\n':
			{
				if ( pData->m_nCharsInCommandBuffer )
				{
					pData->m_pszInputCommandBuffer[pData->m_nCharsInCommandBuffer] = 0;
					Execute( pData );
				}
				pData->m_nCharsInCommandBuffer = 0;
				break;
			}
			default:
			{
				if ( pData->m_nCharsInCommandBuffer < MAX_NETCONSOLE_INPUT_LEN - 1 )
					pData->m_pszInputCommandBuffer[pData->m_nCharsInCommandBuffer++] = *pIn;
				break;
			}
		}
		pIn++;
		recvLen--;
	}
}


CNetConsoleMgr *g_pNetConsoleMgr;

#endif // support_netconsole

