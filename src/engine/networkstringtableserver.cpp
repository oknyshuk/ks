// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "networkstringtable.h"
#include "eiface.h"
#include "server.h"
#include "LocalNetworkBackdoor.h"
import ks.tier1.createinterface;

static CNetworkStringTableContainer s_NetworkStringTableServer;
CNetworkStringTableContainer *networkStringTableContainerServer = &s_NetworkStringTableServer;

// Expose interface
static InterfaceReg s_Reg_CNetworkStringTableContainer_INetworkStringTableContainer( []() -> void * { return static_cast<INetworkStringTableContainer *>( &s_NetworkStringTableServer ); }, INTERFACENAME_NETWORKSTRINGTABLESERVER );;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SV_CreateNetworkStringTables( char const *pchMapName )
{
	// Gather the string table dictionary from the .bsp file
	g_pStringTableDictionary->OnLevelLoadStart( pchMapName, nullptr );

	// Remove any existing tables
	s_NetworkStringTableServer.RemoveAllTables();

	// Unset timing guard and create tables
	s_NetworkStringTableServer.AllowCreation( true );

	// Create engine tables
	sv.CreateEngineStringTables();

	// Create game code tables
	serverGameDLL->CreateNetworkStringTables();

	s_NetworkStringTableServer.AllowCreation( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SV_PrintStringTables( void )
{
	s_NetworkStringTableServer.Dump();
}

void SV_CreateDictionary( char const *pchMapName )
{
	s_NetworkStringTableServer.CreateDictionary( pchMapName );
}
