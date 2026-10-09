#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "dt.h"
import std;

class CDTIRecvTable;
class RecvProp;


// ------------------------------------------------------------------------------------ //
// CClientSendTable and CClientSendProp. This is the data that we receive a SendTable into.
//
// For the most part, it's just a SendTable, but we have slots for extra 
// data we need to store.
// ------------------------------------------------------------------------------------ //

class CClientSendProp
{
public:
	// Null when unset.
	const char*	GetTableName() const		{ return m_TableName.empty() ? nullptr : m_TableName.c_str(); }

	void		SetTableName( const char *pName )	{ m_TableName = pName ? pName : ""; }

private:
	std::string	m_TableName;	// For DPT_DataTable properties.. this tells the table name.
};


//
// CClientSendTables are the client's version of the SendTables that are sent down
// from the server. It stores these, then builds CRecvDecoders that allow it to
// decode packets of data.
//
class CClientSendTable
{
public:
								CClientSendTable();
								~CClientSendTable();
	
	int							GetNumProps() const		{ return m_SendTable.m_nProps; }
	CClientSendProp*			GetClientProp( int i )	{ return &m_Props[i]; }
	
	const char*					GetName()				{ return m_SendTable.GetName(); }
	SendTable*					GetSendTable()			{ return &m_SendTable; }


public:

	SendTable					m_SendTable;
	std::vector< CClientSendProp >	m_Props;	// Extra data for the properties.
};


// ------------------------------------------------------------------------------------ //
// CRecvDecoder.
// ------------------------------------------------------------------------------------ //

class CRecvDecoder
{
public:
				
					CRecvDecoder();

	const char*		GetName() const;
	SendTable*		GetSendTable() const;
	RecvTable*		GetRecvTable() const;

	int				GetNumProps() const;
	const RecvProp*	GetProp( int i ) const;
	const SendProp*	GetSendProp( int i ) const;

	int				GetNumDatatableProps() const;
	const RecvProp*	GetDatatableProp( int i ) const;

	bool			IsVectorLength( int iProp ) const	{ return m_Props[iProp] && m_Props[iProp]->IsVectorLength(); }

	// fn(i) for i in [0, n), vector lengths first; propOf(i) is i's prop. Use with lazy routes.
	template < class PropOf, class Fn >
	void			ForEachLengthsFirst( int n, PropOf propOf, Fn fn ) const
	{
		if ( !m_bHasVectorLengths )
		{
			for ( int i = 0; i < n; i++ )
				fn( i );
			return;
		}
		for ( int i = 0; i < n; i++ )
			if ( IsVectorLength( propOf( i ) ) )
				fn( i );
		for ( int i = 0; i < n; i++ )
			if ( !IsVectorLength( propOf( i ) ) )
				fn( i );
	}


public:
	
	RecvTable			*m_pTable;
	CClientSendTable	*m_pClientSendTable;

	// This is from the data that we've received from the server.
	CSendTablePrecalc	m_Precalc;

	// This mirrors m_Precalc.m_Props. 
	std::vector< const RecvProp* >	m_Props;
	std::vector< const RecvProp* >	m_DatatableProps;

	bool						m_bHasVectorLengths = false;	// any prop with RecvProp::IsVectorLength()

	CDTIRecvTable *m_pDTITable;
};


// ------------------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------------------ //

inline const char* CRecvDecoder::GetName() const
{
	return m_pTable->GetName(); 
}

inline SendTable* CRecvDecoder::GetSendTable() const
{
	return m_Precalc.GetSendTable(); 
}

inline RecvTable* CRecvDecoder::GetRecvTable() const
{
	return m_pTable; 
}

inline int CRecvDecoder::GetNumProps() const
{
	return std::ssize( m_Props );
}

inline const RecvProp* CRecvDecoder::GetProp( int i ) const
{
	return m_Props[i]; 
}

inline const SendProp* CRecvDecoder::GetSendProp( int i ) const
{
	return m_Precalc.GetProp( i );
}

inline int CRecvDecoder::GetNumDatatableProps() const
{
	return std::ssize( m_DatatableProps );
}

inline const RecvProp* CRecvDecoder::GetDatatableProp( int i ) const
{
	return m_DatatableProps[i]; 
}
