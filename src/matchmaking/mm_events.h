#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;
#include "mm_framework.h"
import std;

class CMatchEventsSubscription : public IMatchEventsSubscription
{
	// Methods of IMatchEventsSubscription
public:
	virtual void Subscribe( IMatchEventsSink *pSink );
	virtual void Unsubscribe( IMatchEventsSink *pSink );

	virtual void BroadcastEvent( KeyValues *pEvent );

	virtual void RegisterEventData( KeyValues *pEventData );
	virtual KeyValues * GetEventData( char const *szEventDataKey );

public:
	bool IsBroacasting() const { return m_bBroadcasting; }
	void Shutdown();

public:
	CMatchEventsSubscription();
	~CMatchEventsSubscription();

protected:
	std::vector< IMatchEventsSink * > m_arrSinks;
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
