//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

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

void CMatchEventsSubscription::Subscribe( IMatchEventsSink *pSink )
{
	if ( !pSink )
		return;

	int idx = ks::find_index( m_arrSinks, pSink );
	if ( ks::is_valid_index( m_arrSinks, idx ) )
	{
		Assert( ks::is_valid_index( m_arrRefCount, idx ) );
		Assert( m_arrRefCount[ idx ] > 0 );
		
		++ m_arrRefCount[ idx ];
	}
	else
	{
		m_arrSinks.push_back( pSink );
		m_arrRefCount.push_back( 1 );
		
		Assert( std::ssize( m_arrSinks ) == std::ssize( m_arrRefCount ) );
	}
}

void CMatchEventsSubscription::Unsubscribe( IMatchEventsSink *pSink )
{
	if ( !pSink )
		return;

	int idx = ks::find_index( m_arrSinks, pSink );
	Assert( ks::is_valid_index( m_arrSinks, idx ) );
	if ( !ks::is_valid_index( m_arrSinks, idx ) )
		return;

	Assert( ks::is_valid_index( m_arrRefCount, idx ) );
	Assert( m_arrRefCount[ idx ] > 0 );

	-- m_arrRefCount[ idx ];
	if ( m_arrRefCount[ idx ] <= 0 )
	{
		m_arrSinks.erase( m_arrSinks.begin() + idx );
		m_arrRefCount.erase( m_arrRefCount.begin() + idx );

		Assert( std::ssize( m_arrSinks ) == std::ssize( m_arrRefCount ) );

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
	if ( !Q_stricmp( "OnNetLanConnectionlessPacket", pEvent->GetName() ) )
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
		IMatchEventsSink *pSink = m_arrSinks[ i ];
		
		Assert( ks::is_valid_index( m_arrRefCount, i ) );
		Assert( m_arrRefCount[i] > 0 );
		Assert( pSink );

		pSink->OnEvent( pEvent );
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
	Assert( pEventData );
	if ( !pEventData )
		return;

	Assert( m_bBroadcasting );

	char const *szEventDataKey = pEventData->GetName();
	Assert( szEventDataKey && *szEventDataKey );
	if ( !szEventDataKey || !*szEventDataKey )
		return;

	for ( int k = 0; k < std::ssize( m_arrEventData ); ++ k )
	{
		KeyValues *&pRegistered = m_arrEventData[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
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
	Assert( m_bBroadcasting );

	for ( int k = 0; k < std::ssize( m_arrEventData ); ++ k )
	{
		KeyValues *pRegistered = m_arrEventData[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	for ( int k = 0; k < std::ssize( m_arrSentEvents ); ++ k )
	{
		KeyValues *pRegistered = m_arrSentEvents[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	return nullptr;
}


