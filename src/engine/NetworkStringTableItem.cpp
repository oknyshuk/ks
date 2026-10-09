// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "quakedef.h"
#include "networkstringtableitem.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::CNetworkStringTableItem( void )
{
	m_pUserData = nullptr;
	m_nUserDataLength = 0;
	m_nTickChanged = 0;

	m_nTickCreated = 0;
	m_pChangeList = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::~CNetworkStringTableItem( void )
{
	if ( m_pChangeList )
	{
		// free changelist and elements

		for ( int i=0; i < std::ssize( *m_pChangeList ); i++ )
		{
			itemchange_s item = (*m_pChangeList)[ i ];

			if ( item.data )
				delete[] item.data;
		}

		delete m_pChangeList; // destructor calls Purge()

		m_pUserData = nullptr;
	}
		
	if ( m_pUserData )
	{
		delete[] m_pUserData;
	}
}

void CNetworkStringTableItem::EnableChangeHistory( void )
{
	if ( m_pChangeList )
		return; // already enabled

	m_pChangeList = new std::vector< itemchange_s >;

	contract_assert( m_pChangeList );
}

void CNetworkStringTableItem::UpdateChangeList( int tick, int length, const void *userData )
{
	int count = std::ssize( *m_pChangeList );
	itemchange_s item;

	if ( count > 0 )
	{	
		// check if different from last change in list
		item = (*m_pChangeList)[ count-1 ];

		if ( !item.data && !userData )
			return; // both NULL data

		if ( item.length == length )
		{
			if ( item.data && userData )
			{
				if ( V_memcmp( (void*)userData, (void*)item.data, length ) == 0 )
				{
					return; // no data or size change
				}
			}
		}

		if ( item.tick == tick )
		{
			// two changes within same tick frame, remove last change from list
			if ( item.data )
			{
				delete[] item.data;
			}

			m_pChangeList->erase( m_pChangeList->begin() + ( count-1 ) );
		}
				
	}

	item.tick = tick;

	// add new user data and time stamp

	if ( userData && length )
	{
		item.data = new unsigned char[length];
		item.length = length;
		V_memcpy( item.data, userData, length );
	}
	else
	{
		item.data = nullptr;
		item.length = 0;
	}

	m_pChangeList->push_back( item );
}

int CNetworkStringTableItem::RestoreTick( int tick )
{
	contract_assert( std::ssize( *m_pChangeList )>0 );

	int index = 1;

	itemchange_s *item = &(*m_pChangeList)[ 0 ];

	while ( index < std::ssize( *m_pChangeList ) )
	{
		itemchange_s *nextitem = &(*m_pChangeList)[ index ];

		if ( nextitem->tick > tick )
		{
			break;
		}

		item = nextitem;
		index++;
	}

	if ( item->tick > tick )
	{
		// this string was created after tick, so ignore it right now
		m_pUserData = nullptr;
		m_nUserDataLength = 0;
		m_nTickChanged = 0;
	}
	else
	{
		// restore user data for this string
		m_pUserData = item->data;
		m_nUserDataLength = item->length;
		m_nTickChanged = item->tick;
	}

	return m_nTickChanged;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *string - 
//-----------------------------------------------------------------------------
bool CNetworkStringTableItem::SetUserData( int tick, int length, const void *userData )
{

	if ( m_pChangeList )
	{
		UpdateChangeList( tick, length, userData );
		return false;
	}
	contract_assert( m_nTickCreated > 0 && m_nTickCreated <= tick );

	contract_assert( m_nTickChanged > 0 && m_nTickChanged <= tick );
	contract_assert( length < CNetworkStringTableItem::MAX_USERDATA_SIZE );

	// no old or new data
	if ( !userData && !m_pUserData )
		return false;

	if ( m_pUserData && 
		length == m_nUserDataLength &&
		!V_memcmp( m_pUserData, (void*)userData, length ) )
	{
		return false; // old & new data are equal
	}

	if ( m_pUserData )
		delete[] m_pUserData;

	m_nUserDataLength = length;

	if ( length > 0 )
	{
		m_pUserData = new unsigned char[ length ];
		V_memcpy( m_pUserData, userData, length );
	}
	else
	{
		m_pUserData = nullptr; 
	}

	m_nTickChanged = tick;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
// Output : const void
//-----------------------------------------------------------------------------
const void *CNetworkStringTableItem::GetUserData( int *length )
{
	if ( length )
		*length = m_nUserDataLength;

	return ( const void * )m_pUserData;
}
