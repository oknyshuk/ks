//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "dt_recv_decoder.h"

// ------------------------------------------------------------------------------------ //
// CRecvDecoder.
// ------------------------------------------------------------------------------------ //

CRecvDecoder::CRecvDecoder()
{
	m_pTable = 0;
	m_pClientSendTable = 0;
}


CClientSendTable::CClientSendTable()
{
}


CClientSendTable::~CClientSendTable()
{
	delete [] m_SendTable.m_pNetTableName;
	
	for ( int iProp=0; iProp < m_SendTable.m_nProps; iProp++ )
	{
		delete [] m_SendTable.m_pProps[iProp].m_pVarName;
		delete [] m_SendTable.m_pProps[iProp].m_pExcludeDTName;
		delete [] m_SendTable.m_pProps[iProp].m_pParentArrayPropName;
	}

	delete [] m_SendTable.m_pProps;
}



