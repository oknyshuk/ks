// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// hltvtest.cpp: implementation of the CHLTVTestSystem class.
//
//////////////////////////////////////////////////////////////////////
import std;
import ks.tier1;
#include "hltvtest.h"
#include "hltvserver.h"

CHLTVTestSystem *hltvtest = nullptr;

CHLTVTestSystem::CHLTVTestSystem(void)
{

}

CHLTVTestSystem::~CHLTVTestSystem(void)
{
	StopsTest();
}

void CHLTVTestSystem::RunFrame()
{
	for ( auto &pServer : m_Servers )
	{
		pServer->RunFrame();
	}
}

bool CHLTVTestSystem::StartTest(int nClients, const char *pszAddress)
{
	contract_assert( std::ssize( m_Servers ) == 0 );

	while ( std::ssize( m_Servers ) < nClients )
	{
		extern ConVar tv_snapshotrate;
		CHLTVServer *pServer = m_Servers.emplace_back( std::make_unique< CHLTVServer >( 0, tv_snapshotrate.GetFloat() ) ).get();
		pServer->Init( NET_IsDedicated() );
		pServer->m_ClientState.m_Socket = NET_AddExtraSocket( PORT_ANY );
	}

	for( int i=0; i<nClients; i++ )
	{
		m_Servers[i]->ConnectRelay( pszAddress );
	}

	return true;
}

void CHLTVTestSystem::RetryTest(int nClients)
{
	int maxClients = std::min<int>( nClients+1, std::ssize( m_Servers ) );
		
	for ( int i=0; i<maxClients; i++ )
	{
		CHLTVServer *pHLTV = m_Servers[i].get();
		if ( pHLTV->m_ClientState.m_Remote.Count() > 0 )
		{
			pHLTV->ConnectRelay( pHLTV->m_ClientState.m_Remote.Get( 0 ).m_szRetryAddress.c_str() );
		}
	}
}

bool CHLTVTestSystem::StopsTest()
{
	for ( auto &pServer : m_Servers )
	{
		pServer->Shutdown();
	}

	m_Servers.clear();

	NET_RemoveAllExtraSockets();

	return true;
}
