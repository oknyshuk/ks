#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

#include "igameevents.h"
extern IGameEventManager2 *gameeventmanager;

// A safer method than registering with the event manager directly.
// Avoids requiring the user to remove themselves as listeners in 
// their deconstructor, and sets the serverside variable based on
// our dll location.
//
// Events are delivered to the FireGameEvent( IGameEvent * ) member of the type that
// ListenForGameEvent() is called on. A class that registers through a base class (so the
// call is made on the base type) must call SetGameEventHandler( this ) in its constructor
// before anything registers.
class CGameEventListener
{
public:
	CGameEventListener() : m_bRegisteredForEvents(false)
	{
	}

	~CGameEventListener()
	{
		StopListeningForAllEvents();
	}

	// Bind T::FireGameEvent as the function the event manager calls for this listener.
	template< class T >
	void SetGameEventHandler( T *self )
	{
		m_Handler.emplace( std::cw<&T::FireGameEvent>, self );
	}

	void ListenForGameEvent( this auto &self, const char *name )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

		listener.m_bRegisteredForEvents = true;

#ifdef CLIENT_DLL
		bool bServerSide = false;
#else
		bool bServerSide = true;
#endif

		gameeventmanager->AddListener( &listener, *listener.m_Handler, name, bServerSide );
	}

	void ListenForAllGameEvents( this auto &self )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

#ifdef CLIENT_DLL
	bool bServerSide = false;
#else
	bool bServerSide = true;
#endif

		gameeventmanager->AddListenerGlobal( &listener, *listener.m_Handler, bServerSide );
	}


	void StopListeningForAllEvents()
	{
		// remove me from list
		if ( m_bRegisteredForEvents )
		{
			if ( gameeventmanager )
				gameeventmanager->RemoveListener( this );

			m_bRegisteredForEvents = false;
		}
	}

private:

	// What the event manager calls
	std::optional< GameEventCallback_t > m_Handler;

	// Have we registered for any events?
	bool m_bRegisteredForEvents;
};
