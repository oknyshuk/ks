//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "vstdlib/ieventsystem.h"
#include "tier1/generichash.h"
#include "tier0/tslist.h"
#include "tier1/tier1.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>


//-----------------------------------------------------------------------------
// A hash of the event name
//-----------------------------------------------------------------------------
typedef uint32 EventNameHash_t;


//-----------------------------------------------------------------------------
// An event queue is a thread-safe event queue
// NOTE: There are questions about whether we should have a secondary queue
// similar to vgui, where if you are doing event handling, you want to
// process the secondary queue prior to processing the next primary queue
// Problem is w/ threadsafety: what happens if other threads which are not
// on the thread processing the events posts an event?
//-----------------------------------------------------------------------------
class CEventQueue
{
public:
	CEventQueue();
	~CEventQueue();

	// Posts an event
	void PostEvent( CFunctorCallback *pCallback, CFunctorData *pData );

	// Processes the event queue
	void ProcessEvents( );

	// Discards events that use a particular callback
	void DiscardEvents( CFunctorCallback *pCallback );

	// Refcount
	int AddRef() { return ++m_nRefCount; }
	int Release() { return --m_nRefCount; }
	int RefCount() { return m_nRefCount; }

private:
	struct QueuedEvent_t
	{
		void *m_pTarget;
		CFunctorData *m_pData;
		CFunctorCallback *m_pCallback;
	};

	void Cleanup();

	CTSQueue< CFunctorCallback* > m_QueuedEventDiscards;
	CTSQueue< QueuedEvent_t > m_Queue;
	std::atomic<int> m_nRefCount;

#ifdef _DEBUG
	bool m_bIsProcessingEvents;
	bool m_bIsDiscardingEvents;
#endif
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CEventQueue::CEventQueue()
{
#ifdef _DEBUG
	m_bIsProcessingEvents = false;
	m_bIsDiscardingEvents = false;
#endif
}

CEventQueue::~CEventQueue()
{
	Assert( !m_bIsProcessingEvents );
	Assert( !m_bIsDiscardingEvents );
	Cleanup();
}


//-----------------------------------------------------------------------------
// Cleans up the event queue
//-----------------------------------------------------------------------------
void CEventQueue::Cleanup()
{
	QueuedEvent_t event;
	while ( m_Queue.PopItem( &event ) )
	{
		event.m_pCallback->Release();
		event.m_pData->Release();
	}
}


//-----------------------------------------------------------------------------
// Posts an event
//-----------------------------------------------------------------------------
void CEventQueue::PostEvent( CFunctorCallback *pCallback, CFunctorData *pData )
{
	QueuedEvent_t event;
	event.m_pCallback = pCallback;
	event.m_pData = pData;
	pCallback->AddRef();
	pData->AddRef();
	m_Queue.PushItem( event );
}


//-----------------------------------------------------------------------------
// Discards events that use a particular callback
//-----------------------------------------------------------------------------
void CEventQueue::DiscardEvents( CFunctorCallback *pCallback )
{
	Assert( !m_bIsProcessingEvents );

#ifdef _DEBUG
	m_bIsDiscardingEvents = true;
#endif

	m_QueuedEventDiscards.PushItem( pCallback );

#ifdef _DEBUG
	m_bIsDiscardingEvents = false;
#endif
}


//-----------------------------------------------------------------------------
// Processes the event queue
//-----------------------------------------------------------------------------
void CEventQueue::ProcessEvents( )
{
	Assert( !m_bIsProcessingEvents );
	Assert( !m_bIsDiscardingEvents );

#ifdef _DEBUG
	m_bIsProcessingEvents = true;
#endif

	if ( m_QueuedEventDiscards.Count() == 0 )
	{
		// Dispatch all events
		QueuedEvent_t event;
		while ( m_Queue.PopItem( &event ) )
		{
			(*(event.m_pCallback))( event.m_pData );
			event.m_pData->Release();
			event.m_pCallback->Release();
		}
	}
	else
	{
		// Build list of callbacks which are stale and need to die
		std::vector< CFunctorCallback * > callbacks;
		callbacks.reserve( m_QueuedEventDiscards.Count() );

		CFunctorCallback *pCallback = nullptr;
		while( m_QueuedEventDiscards.PopItem( &pCallback ) )
		{
			callbacks.push_back( pCallback );
		}

		// Only invoke events on non-stale callbacks
		int nCount = std::ssize( callbacks );
		QueuedEvent_t event;
		while ( m_Queue.PopItem( &event ) )
		{
			bool bFound = false;
			for ( int i = 0; i < nCount; ++i )
			{
				bFound = ( event.m_pCallback == callbacks[i] );
				if ( bFound )
					break;
			}

			if ( !bFound )
			{
				(*(event.m_pCallback))( event.m_pData );
			}
			event.m_pData->Release();
			event.m_pCallback->Release();
		}
	}

#ifdef _DEBUG
	m_bIsProcessingEvents = false;
#endif
}


//-----------------------------------------------------------------------------
// An event id maintains a list of all interested listeners
//-----------------------------------------------------------------------------
class CEventId
{
public:
	// Registers/unregisters a listener of this event
	void RegisterListener( CEventQueue *pEventQueue, CFunctorCallback *pCallback );
	void UnregisterListener( CEventQueue *pEventQueue, CFunctorCallback *pCallback );

	// Unregisters all listeners associated with a particular queue
	void UnregisterAllListeners( CEventQueue *pEventQueue );

	// Posts an event
	void PostEvent( CEventQueue *pEventQueue, const void *pListener, CFunctorData *pData );

private:
	struct SubscribedQueue_t
	{
		CEventQueue *m_pEventQueue;
		CFunctorCallback *m_pCallback;
	};

	std::vector< SubscribedQueue_t > m_SubscribedQueueList;
	std::shared_mutex m_ListenerLock;
};


//-----------------------------------------------------------------------------
// Registers a listener of this event
//-----------------------------------------------------------------------------
void CEventId::RegisterListener( CEventQueue *pEventQueue, CFunctorCallback *pCallback )
{
	// NOTE: We could probably move the write locks so they cover less code
	// but I'm wanting to be cautious
	std::unique_lock lock( m_ListenerLock );

	auto i = m_SubscribedQueueList.begin();
	for( ; i != m_SubscribedQueueList.end(); ++i )
	{
		if ( ( i->m_pEventQueue == pEventQueue ) && i->m_pCallback->IsEqual( pCallback ) )
		{
			Warning( "Tried to install the same listener on the same event id + queue twice!\n" );
			break;
		}
	}

	if ( i == m_SubscribedQueueList.end() )
	{
		SubscribedQueue_t queue;
		queue.m_pEventQueue = pEventQueue;
		queue.m_pCallback = pCallback;
		pCallback->AddRef();
		pEventQueue->AddRef();

		m_SubscribedQueueList.push_back( queue );
	}
}


//-----------------------------------------------------------------------------
// Unregisters a listener
//-----------------------------------------------------------------------------
void CEventId::UnregisterListener( CEventQueue *pEventQueue, CFunctorCallback *pCallback )
{
	// NOTE: We could probably move the write locks so they cover less code
	// but I'm wanting to be cautious
	std::unique_lock lock( m_ListenerLock );

	auto i = m_SubscribedQueueList.begin();
	for( ; i != m_SubscribedQueueList.end(); ++i )
	{
		if ( ( i->m_pEventQueue == pEventQueue ) && i->m_pCallback->IsEqual( pCallback ) )
			break;
	}

	if ( i != m_SubscribedQueueList.end() )
	{
		CFunctorCallback *pCachedCallback = i->m_pCallback;

		m_SubscribedQueueList.erase( i );

		// Remove the cached callback from the queued list of events
		// if it has a non-zero refcount
		if ( pCachedCallback->Release() > 0 )
		{
			pEventQueue->DiscardEvents( pCachedCallback );
		}
		pEventQueue->Release();
	}
}


//-----------------------------------------------------------------------------
// Unregisters all listeners associated with a particular queue
//-----------------------------------------------------------------------------
void CEventId::UnregisterAllListeners( CEventQueue *pEventQueue )
{
	// NOTE: We could probably move the write locks so they cover less code
	// but I'm wanting to be cautious
	std::unique_lock lock( m_ListenerLock );

	std::erase_if( m_SubscribedQueueList, [pEventQueue]( const SubscribedQueue_t &queue )
	{
		if ( queue.m_pEventQueue != pEventQueue )
			return false;

		queue.m_pCallback->Release();
		pEventQueue->Release();
		return true;
	} );
}


//-----------------------------------------------------------------------------
// Posts an event
//-----------------------------------------------------------------------------
void CEventId::PostEvent( CEventQueue *pEventQueue, const void *pListener, CFunctorData *pData )
{
	std::shared_lock lock( m_ListenerLock );

	for( SubscribedQueue_t &queue : m_SubscribedQueueList )
	{
		if ( pEventQueue && queue.m_pEventQueue != pEventQueue )
			continue;

		if ( pListener && pListener != queue.m_pCallback->GetTarget() )
			continue;

		queue.m_pEventQueue->PostEvent( queue.m_pCallback, pData );
	}
}



//-----------------------------------------------------------------------------
// Event system implementation
//-----------------------------------------------------------------------------
class CEventSystem : public CTier1AppSystem< IEventSystem >
{
	typedef CTier1AppSystem< IEventSystem > BaseClass;

	// Methods of IAppSystem
public:

	// Methods of IEventSystem
public:
	virtual EventQueue_t CreateEventQueue();
	virtual void DestroyEventQueue( EventQueue_t hQueue );
	virtual void ProcessEvents( EventQueue_t hQueue );
	virtual EventId_t RegisterEvent( const char *pEventName );
	virtual void PostEventInternal( EventId_t nEventId, EventQueue_t hQueue, const void *pListener, CFunctorData *pData );
	virtual void RegisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback );
	virtual void UnregisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback );

	// Other public methods
public:
	CEventSystem();
	virtual ~CEventSystem();

private:
	// Node-based, so the CEventId pointers handed out as EventId_t stay valid.
	std::unordered_map< EventNameHash_t, CEventId > m_EventIds;
	std::mutex m_EventIdsMutex;
};


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static CEventSystem s_EventSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEventSystem, IEventSystem, 
	EVENTSYSTEM_INTERFACE_VERSION, s_EventSystem )


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEventSystem::CEventSystem() : m_EventIds( 256 )
{	
}

CEventSystem::~CEventSystem()
{
}


//-----------------------------------------------------------------------------
// Create, destroy an event queue
//-----------------------------------------------------------------------------
EventQueue_t CEventSystem::CreateEventQueue()
{
	CEventQueue *pEventQueue = new CEventQueue;
	return (EventQueue_t)pEventQueue;
}

void CEventSystem::DestroyEventQueue( EventQueue_t hQueue )
{
	CEventQueue *pEventQueue = ( CEventQueue* )( hQueue );
	if ( !pEventQueue )
		return;

	if ( pEventQueue->RefCount() != 0 )
	{
		// This means it's still sitting in a listener list
		Warning( "Perf warning: Forgot to unregister listeners on event queue %p\n", hQueue );

		std::vector< CEventId * > eventIds;
		{
			std::lock_guard lock( m_EventIdsMutex );
			eventIds.reserve( m_EventIds.size() );
			for ( auto &[hId, eventId] : m_EventIds )
			{
				eventIds.push_back( &eventId );
			}
		}

		for ( CEventId *pEventId : eventIds )
		{
			pEventId->UnregisterAllListeners( pEventQueue );
		}
	}

	delete pEventQueue;
}


//-----------------------------------------------------------------------------
// Registers an event
//-----------------------------------------------------------------------------
EventId_t CEventSystem::RegisterEvent( const char *pEventName )
{
	EventNameHash_t hId = (EventNameHash_t)MurmurHash2( pEventName, Q_strlen(pEventName), 0xE1E47644 );
	std::lock_guard lock( m_EventIdsMutex );
	return ( EventId_t )( &m_EventIds.try_emplace( hId ).first->second );
}


//-----------------------------------------------------------------------------
// Registers, unregisters an event listener
//-----------------------------------------------------------------------------
void CEventSystem::RegisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback )
{
	CEventQueue *pEventQueue = ( CEventQueue* )( hQueue );
	if ( pEventQueue )
	{
		CEventId *pEventId = ( CEventId* )nEventId;
		if ( pEventId )
		{
			pEventId->RegisterListener( pEventQueue, pCallback );
		}
	}
	pCallback->Release();
}

void CEventSystem::UnregisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback )
{
	CEventQueue *pEventQueue = ( CEventQueue* )( hQueue );
	if ( pEventQueue )
	{
		CEventId *pEventId = ( CEventId* )nEventId;
		if ( pEventId )
		{
			pEventId->UnregisterListener( pEventQueue, pCallback );
		}
	}
	pCallback->Release();
}


//-----------------------------------------------------------------------------
// Posts an event to all current listeners in a single queue
//-----------------------------------------------------------------------------
void CEventSystem::PostEventInternal( EventId_t nEventId, EventQueue_t hQueue, const void *pListener, CFunctorData *pData )
{
	CEventQueue *pEventQueue = ( CEventQueue* )( hQueue );
	CEventId *pEventId = (CEventId*)nEventId;
	if ( pEventId )
	{
		pEventId->PostEvent( pEventQueue, pListener, pData );
	}
	pData->Release();
}


//-----------------------------------------------------------------------------
// Process queued events on a single queue
//-----------------------------------------------------------------------------
void CEventSystem::ProcessEvents( EventQueue_t hQueue )
{
	CEventQueue *pEventQueue = ( CEventQueue* )( hQueue );
	if ( pEventQueue )
	{
		pEventQueue->ProcessEvents();
	}
}
