// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "shareddefs.h"
#include "util.h"
#include "timedeventmgr.h"

// ------------------------------------------------------------------------------------------ //
// CEventRegister.
// ------------------------------------------------------------------------------------------ //

CEventRegister::CEventRegister()
{
	m_bRegistered = false;
	m_pEventMgr = nullptr;
}


CEventRegister::~CEventRegister()
{
	Term();
}


void CEventRegister::Init( CTimedEventMgr *pMgr, IEventRegisterCallback *pCallback )
{
	Term();
	m_pEventMgr = pMgr;
	m_pCallback = pCallback;
}


void CEventRegister::Term()
{
	// Unregister.
	if ( m_pEventMgr && m_bRegistered )
	{
		m_pEventMgr->RemoveEvent( this );
	}
}
	

void CEventRegister::SetUpdateInterval( float interval )
{
	contract_assert( m_pEventMgr );

	if ( m_pEventMgr )
	{
		// Register for this event.
		m_flUpdateInterval = interval;
		m_flNextEventTime = gpGlobals->curtime + m_flUpdateInterval;

		m_pEventMgr->RegisterForNextEvent( this );
	}
}


void CEventRegister::StopUpdates()
{
	if ( m_pEventMgr )
	{
		// Unregister our next event.
		m_pEventMgr->RemoveEvent( this );
	}
}


void CEventRegister::Reregister()
{
	if ( m_flUpdateInterval > 1e-6 && m_pEventMgr )
	{
		while ( m_flNextEventTime <= gpGlobals->curtime )
		{
			m_flNextEventTime += m_flUpdateInterval;
		}

		m_pEventMgr->RegisterForNextEvent( this );
	}
}


// ------------------------------------------------------------------------------------------ //
// CTimedEventMgr.
// ------------------------------------------------------------------------------------------ //

bool TimedEventMgr_LessFunc( CEventRegister* const &a, CEventRegister* const &b )
{
	return a->m_flNextEventTime > b->m_flNextEventTime;
}


CTimedEventMgr::CTimedEventMgr() = default;


void CTimedEventMgr::FireEvents()
{
	CVProfScope _( "CTimedEventMgr::FireEvents", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	while ( !m_Events.empty() )
	{
		// Fire the top element, then break out.
		CEventRegister *pEvent = m_Events.front();
		if ( gpGlobals->curtime >= pEvent->m_flNextEventTime )
		{
			// Reregister for the timed event, then fire the callback for the event.
			ks::heap_pop( m_Events, TimedEventMgr_LessFunc );
			pEvent->m_bRegistered = false;
			pEvent->Reregister();

			pEvent->m_pCallback->FireEvent();
		}
		else
		{
			break;
		}
	}
}


void CTimedEventMgr::RegisterForNextEvent( CEventRegister *pEvent )
{
	RemoveEvent( pEvent );
	ks::heap_push( m_Events, pEvent, TimedEventMgr_LessFunc );
	pEvent->m_bRegistered = true;
}


void CTimedEventMgr::RemoveEvent( CEventRegister *pEvent )
{
	if ( pEvent->m_bRegistered )
	{
		// Find the event in the list and remove it.
		int i = ks::find_index( m_Events, pEvent );
		if ( i >= 0 )
			ks::heap_erase( m_Events, i, TimedEventMgr_LessFunc );
	}
}
