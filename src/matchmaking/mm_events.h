#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.threadtools;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
#include "mm_framework.h"
import std;

class CMatchEventsSubscription : public IMatchEventsSubscription
{
	// Methods of IMatchEventsSubscription
public:
	void Subscribe( const void *pOwner, MatchEventHandler_t handler );
	void Unsubscribe( const void *pOwner );

	void BroadcastEvent( KeyValues *pEvent );

	void RegisterEventData( KeyValues *pEventData );
	KeyValues * GetEventData( char const *szEventDataKey );

public:
	int FindSink( const void *pOwner ) const;
	bool IsBroacasting() const { return m_bBroadcasting; }
	void Shutdown();

public:
	CMatchEventsSubscription();
	~CMatchEventsSubscription();

protected:
	// The handler is shared so that a subscriber unsubscribing from inside its own handler
	// does not destroy the handler that is running.
	struct Sink_t
	{
		const void *m_pOwner;
		std::shared_ptr< MatchEventHandler_t > m_pHandler;
	};
	std::vector< Sink_t > m_arrSinks;
	std::vector< int > m_arrRefCount;
	std::vector< int > m_arrIteratorsOutstanding;

	bool m_bBroadcasting;
	bool m_bAllowNestedBroadcasts;
	std::vector< KeyValues * > m_arrQueuedEvents;
	std::vector< KeyValues * > m_arrEventData;
	std::vector< KeyValues * > m_arrSentEvents;
};

// Match events subscription singleton
extern CMatchEventsSubscription *g_pMatchEventsSubscription;
