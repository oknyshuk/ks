// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "dt_stack.h"
import std;

CDatatableStack::CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID )
	: m_RecurseAndCallProxies( std::cw<&NoRecurse> )
{
	m_pPrecalc = pPrecalc;

	m_pStructBase = pStructBase;
	m_ObjectID = objectID;
	
	m_iCurProp = 0;
	m_pCurProp = nullptr;

	m_bInitted = false;

#ifdef _DEBUG
	memset( m_pProxies, 0xFF, sizeof( m_pProxies ) );
#endif

	m_bLocalNetworkBackDoor = false;
}


void CDatatableStack::Init( bool bExplicitRoutes, bool bLocalNetworkBackDoor )
{
	m_bLocalNetworkBackDoor = bLocalNetworkBackDoor;

	if ( bExplicitRoutes )
	{
		std::fill_n( m_pProxies, std::ssize( m_pPrecalc->m_ProxyPaths ), UnroutedProxy() );
	}
	else
	{
		// Walk down the tree and call all the datatable proxies as we hit them.
		RecurseAndCallProxies( &m_pPrecalc->m_Root, m_pStructBase );
	}

	m_bInitted = true;
}
