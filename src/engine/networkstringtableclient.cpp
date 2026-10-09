// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "networkstringtable.h"
#include "client.h"

 	static CNetworkStringTableContainer s_NetworkStringTableClient;
	CNetworkStringTableContainer *networkStringTableContainerClient = &s_NetworkStringTableClient;

	// Expose to client .dll
	static InterfaceReg s_Reg_CNetworkStringTableContainer_INetworkStringTableContainer( []() -> void * { return static_cast<INetworkStringTableContainer *>( &s_NetworkStringTableClient ); }, INTERFACENAME_NETWORKSTRINGTABLECLIENT );;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_PrintStringTables( void )
{
#ifndef DEDICATED
	if ( GetBaseLocalClient().m_StringTableContainer )
	{
		GetBaseLocalClient().m_StringTableContainer->Dump();
	}
#endif
}
