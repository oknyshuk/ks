// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_events.h"

CMatchEventsSubscription::CMatchEventsSubscription() :
	m_bAllowNestedBroadcasts( false ),
	m_bBroadcasting( false )
{
	;
}

CMatchEventsSubscription::~CMatchEventsSubscription()
{
	;
}

static CMatchEventsSubscription g_MatchEventsSubscription;
CMatchEventsSubscription *g_pMatchEventsSubscription = &g_MatchEventsSubscription;

//
// Implementation
//

int CMatchEventsSubscription::FindSink( const void *pOwner ) const
{
	for ( int i = 0; i < std::ssize( m_arrSinks ); ++ i )
	{
		if ( m_arrSinks[ i ].m_pOwner == pOwner )
			return i;
	}
	return -1;
}

void CMatchEventsSubscription::Subscribe( const void *pOwner, MatchEventHandler_t handler )
{
	if ( !pOwner )
		return;

	int idx = FindSink( pOwner );
	if ( ks::is_valid_index( m_arrSinks, idx ) )
	{
		contract_assert( ks::is_valid_index( m_arrRefCount, idx ) );
		contract_assert( m_arrRefCount[ idx ] > 0 );
		
		++ m_arrRefCount[ idx ];
	}
	else
	{
		m_arrSinks.push_back( { pOwner, std::make_shared< MatchEventHandler_t >( std::move( handler ) ) } );
		m_arrRefCount.push_back( 1 );
		
		contract_assert( std::ssize( m_arrSinks ) == std::ssize( m_arrRefCount ) );
	}
}

void CMatchEventsSubscription::Unsubscribe( const void *pOwner )
{
	if ( !pOwner )
		return;

	int idx = FindSink( pOwner );
	contract_assert( ks::is_valid_index( m_arrSinks, idx ) );
	if ( !ks::is_valid_index( m_arrSinks, idx ) )
		return;

	contract_assert( ks::is_valid_index( m_arrRefCount, idx ) );
	contract_assert( m_arrRefCount[ idx ] > 0 );

	-- m_arrRefCount[ idx ];
	if ( m_arrRefCount[ idx ] <= 0 )
	{
		m_arrSinks.erase( m_arrSinks.begin() + idx );
		m_arrRefCount.erase( m_arrRefCount.begin() + idx );

		contract_assert( std::ssize( m_arrSinks ) == std::ssize( m_arrRefCount ) );

		// Update outstanding iterators that are beyond removal point
		for ( int k = 0; k < std::ssize( m_arrIteratorsOutstanding ); ++ k )
		{
			if ( m_arrIteratorsOutstanding[k] >= idx )
				-- m_arrIteratorsOutstanding[k];
		}
	}
}

void CMatchEventsSubscription::Shutdown()
{
	m_bBroadcasting = true;	// Blocks all BroadcastEvent calls from being dispatched!
}

void CMatchEventsSubscription::BroadcastEvent( KeyValues *pEvent )
{
	//
	// Network raw packet decryption
	//
	if ( !V_stricmp( "OnNetLanConnectionlessPacket", pEvent->GetName() ) )
	{
		if ( void * pRawPacket = pEvent->GetPtr( "rawpkt" ) )
		{
			netpacket_t *pkt = ( netpacket_t * ) pRawPacket;
			pEvent->deleteThis();
			
			g_pConnectionlessLanMgr->ProcessConnectionlessPacket( pkt );
			return;
		}
	}

	//
	// Broadcasting events is reliable even when subscribers get added
	// or removed during broadcasts, or even broadcast nested events.
	//
	// Nested broadcasts are not allowed because it messes up the perception
	// of event timeline by external listeners, nested broadcast is enqueued
	// instead and broadcast after the original event has been broadcast to
	// all subscribers.
	//

	if ( m_bBroadcasting )
	{
		if ( !m_bAllowNestedBroadcasts )
		{
			m_arrQueuedEvents.push_back( pEvent );
			return;
		}
	}

	m_bBroadcasting = true;

	KeyValues::AutoDelete autoDeleteEvent( pEvent );
	m_arrSentEvents.insert( m_arrSentEvents.begin(), pEvent );

	// Internally events are cracked before external subscribers
	g_pMMF->OnEvent( pEvent );

	// iterate subscribers
	for ( m_arrIteratorsOutstanding.push_back( 0 );
		  m_arrIteratorsOutstanding.back() < std::ssize( m_arrSinks );
		  ++ m_arrIteratorsOutstanding.back() )
	{
		int i = m_arrIteratorsOutstanding.back();
		std::shared_ptr< MatchEventHandler_t > pHandler = m_arrSinks[ i ].m_pHandler;
		
		contract_assert( ks::is_valid_index( m_arrRefCount, i ) );
		contract_assert( m_arrRefCount[i] > 0 );
		contract_assert( pHandler );

		( *pHandler )( pEvent );
	}
	m_arrIteratorsOutstanding.resize( m_arrIteratorsOutstanding.size() - 1 );

	m_bBroadcasting = false;

	//
	// Broadcast queued events
	//
	if ( !m_arrQueuedEvents.empty() )
	{
		KeyValues *pQueued = m_arrQueuedEvents.front();
		m_arrQueuedEvents.erase( m_arrQueuedEvents.begin(), m_arrQueuedEvents.begin() + 1 );
		BroadcastEvent( pQueued );
		return;
	}

	//
	// No more queued events left, clean up registered event data
	//
	for ( int k = 0; k < std::ssize( m_arrEventData ); ++ k )
	{
		KeyValues *pRegistered = m_arrEventData[k];
		pRegistered->deleteThis();
	}
	m_arrEventData.clear(); m_arrEventData.shrink_to_fit();
	m_arrSentEvents.clear(); m_arrSentEvents.shrink_to_fit();
}

void CMatchEventsSubscription::RegisterEventData( KeyValues *pEventData )
{
	contract_assert( pEventData );
	if ( !pEventData )
		return;

	contract_assert( m_bBroadcasting );

	char const *szEventDataKey = pEventData->GetName();
	contract_assert( szEventDataKey && *szEventDataKey );
	if ( !szEventDataKey || !*szEventDataKey )
		return;

	for ( int k = 0; k < std::ssize( m_arrEventData ); ++ k )
	{
		KeyValues *&pRegistered = m_arrEventData[k];
		if ( !V_stricmp( szEventDataKey, pRegistered->GetName() ) )
		{
			pRegistered->deleteThis();
			pRegistered = pEventData;
			return;
		}
	}

	m_arrEventData.push_back( pEventData );
}

KeyValues * CMatchEventsSubscription::GetEventData( char const *szEventDataKey )
{
	contract_assert( m_bBroadcasting );

	for ( int k = 0; k < std::ssize( m_arrEventData ); ++ k )
	{
		KeyValues *pRegistered = m_arrEventData[k];
		if ( !V_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	for ( int k = 0; k < std::ssize( m_arrSentEvents ); ++ k )
	{
		KeyValues *pRegistered = m_arrSentEvents[k];
		if ( !V_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
#include "facade/imatcheventssubscription.inl"
