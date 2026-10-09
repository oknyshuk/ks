// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib:ieventsystem;

import std;
import ks.tier1;

export extern "C++" {

//-----------------------------------------------------------------------------
// Event queues are used to allow listeners to decide at what point they
// want to deal with event processing
//-----------------------------------------------------------------------------
struct EventQueue_t__ { int unused; };
using EventQueue_t = EventQueue_t__ *;
inline constexpr auto EVENT_QUEUE_HANDLE_INVALID = ( (EventQueue_t)0 );

struct EventId_t__ { int unused; };
using EventId_t = EventId_t__ *;

//-----------------------------------------------------------------------------
// Global interface for posting/listening to events
//
// Usage for registering/unregistering listeners and posting events:
// DEFINE_EVENT( TestEvent, int, int );
// void OnTestEvent( const int &x, const int &y );
// void f()
// {
//		hTest = g_pEventSystem->CreateEventQueue();
//		TestEvent::RegisterFunc( hTest, OnTestEvent );
//			or TestEvent::RegisterFunc( hTest, myClassPtr, &CMyClass::OnTestEvent );
//		TestEvent::Post( 100, 200 );
//		TestEvent::PostToListener( myClassPtr, 100, 200 );
//		g_pEventSystem->ProcessEvents( hTest );
//		TestEvent::UnregisterFunc( hTest, OnTestEvent );
//		g_pEventSystem->DestroyEventQueue( hTest );
// }
//
// Note that the arguments of the event handlers are always const references
// to the arguments specified in the event declaration to avoid extra copies
// of the data.
//
// Also note that it's possible to post an event to a specific listener
// using PostToListener. Just specify the class name as the first argument.
//
// Shit gets funky when you pass pointers to events. You need to be very careful
// that the pointer is guaranteed to be valid until the messages are being posted.
// Also, because everything is a const reference, the function prototypes
// for stuff that accepts pointers looks like this strangeness:
//
// DEFINE_EVENT( TestEvent, int, char * );
// void OnTestEvent( const int &x, char * const & pName );
//
// Also note: You cannot register or unregister while in the middle of processing events
//
// NOTE: We use EventClass::RegisterMemberFunc / EventClass:RegisterFunc
// instead of overloading EventClass::Register to get more sane-looking
// error message from the compiler if it happens to be passed an invalid
// event handler.
//
// From a perf standpoint, registering + unregistering listeners, and 
// destroying event queues all can cause thread stalls. Do them with
// low frequency.
//-----------------------------------------------------------------------------
class IEventSystem : public IAppSystem
{
public:
	// Creates an event queue.
	// Creation can occur at any time. Destruction can happen at any time
	// provided you're not simultaneously processing events or registering/unregistering
	// listeners on the same thread
	virtual EventQueue_t CreateEventQueue() = 0;
	virtual void DestroyEventQueue( EventQueue_t hQueue ) = 0;

	// Processess queued events for a event queue
	virtual void ProcessEvents( EventQueue_t hQueue ) = 0;

	// Allows posting to an event id known at runtime, with arbitrary arguments.
	// Useful to allow systems to post event ids told to it by external systems.
	inline void PostEvent( EventId_t nEventId )
	{
		CFunctorData *pData = CreateFunctorData();
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, nullptr, pData );
	}

	inline void PostEvent( EventId_t nEventId, EventQueue_t hQueue )
	{
		CFunctorData *pData = CreateFunctorData();
		PostEventInternal( nEventId, hQueue, nullptr, pData );
	}

	inline void PostEventToListener( EventId_t nEventId, const void *pListener )
	{
		CFunctorData *pData = CreateFunctorData();
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, pListener, pData );
	}

	inline void PostEventToListener( EventId_t nEventId, EventQueue_t hQueue, const void *pListener )
	{
		CFunctorData *pData = CreateFunctorData();
		PostEventInternal( nEventId, hQueue, pListener, pData );
	}

	template <typename... ARGS>
	inline void PostEvent( EventId_t nEventId, ARGS &&... args )
	{
		CFunctorData *pData = CreateFunctorData( std::forward<ARGS>(args)... );
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, nullptr, pData );
	}

	template <typename... ARGS>
	inline void PostEvent( EventId_t nEventId, EventQueue_t hQueue, ARGS &&... args )
	{
		CFunctorData *pData = CreateFunctorData( std::forward<ARGS>(args)... );
		PostEventInternal( nEventId, hQueue, nullptr, pData );
	}

	template <typename... ARGS>
	inline void PostEventToListener( EventId_t nEventId, const void *pListener, ARGS &&... args )
	{
		CFunctorData *pData = CreateFunctorData( std::forward<ARGS>(args)... );
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, pListener, pData );
	}

	template <typename... ARGS>
	inline void PostEventToListener( EventId_t nEventId, EventQueue_t hQueue, const void *pListener, ARGS &&... args )
	{
		CFunctorData *pData = CreateFunctorData( std::forward<ARGS>(args)... );
		PostEventInternal( nEventId, hQueue, pListener, pData );
	}

private:
	// NOTE: These are not meant to be called directly. See the comment above
	// the IEventSystem class definition for how to post/register/unregister events
	virtual EventId_t RegisterEvent( const char *pEventName ) = 0;
	virtual void PostEventInternal( EventId_t nEventId, EventQueue_t hQueue, const void *pListener, CFunctorData *pData ) = 0;
	virtual void RegisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback ) = 0;
	virtual void UnregisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback ) = 0;

	template < typename Event_t, typename... ARGS > friend class CEventSignature;
};

extern IEventSystem *g_pEventSystem;

//-----------------------------------------------------------------------------
// A typed event: holds the event's argument types and provides the posting
// and listener registration entry points.
//-----------------------------------------------------------------------------
template < typename Event_t, typename... ARGS >
class CEventSignature
{
public:
	static inline void Post( ARGS... args )
	{
		CFunctorData *pData = CreateFunctorData( args... );
		g_pEventSystem->PostEventInternal( Event_t::GetEventId(), EVENT_QUEUE_HANDLE_INVALID, nullptr, pData );
	}

	static inline void Post( EventQueue_t hQueue, ARGS... args )
	{
		CFunctorData *pData = CreateFunctorData( args... );
		g_pEventSystem->PostEventInternal( Event_t::GetEventId(), hQueue, nullptr, pData );
	}

	static inline void PostToListener( const void *pListener, ARGS... args )
	{
		CFunctorData *pData = CreateFunctorData( args... );
		g_pEventSystem->PostEventInternal( Event_t::GetEventId(), EVENT_QUEUE_HANDLE_INVALID, pListener, pData );
	}

	static inline void PostToListener( EventQueue_t hQueue, const void *pListener, ARGS... args )
	{
		CFunctorData *pData = CreateFunctorData( args... );
		g_pEventSystem->PostEventInternal( Event_t::GetEventId(), hQueue, pListener, pData );
	}

	static inline void RegisterFunc( EventQueue_t hQueue, void (*pfnProxied)( ARGS... ) )
	{
		CFunctorCallback *pCallback = CreateFunctorCallback( pfnProxied );
		g_pEventSystem->RegisterListener( Event_t::GetEventId(), hQueue, pCallback );
	}

	static inline void UnregisterFunc( EventQueue_t hQueue, void (*pfnProxied)( ARGS... ) )
	{
		CFunctorCallback *pCallback = CreateFunctorCallback( pfnProxied );
		g_pEventSystem->UnregisterListener( Event_t::GetEventId(), hQueue, pCallback );
	}

	template < class OBJECT_TYPE_PTR >
	static inline void RegisterMemberFunc( EventQueue_t hQueue, OBJECT_TYPE_PTR *pClass, void (OBJECT_TYPE_PTR::*pfnProxied)( ARGS... ) )
	{
		CFunctorCallback *pCallback = CreateFunctorCallback( pClass, pfnProxied );
		g_pEventSystem->RegisterListener( Event_t::GetEventId(), hQueue, pCallback );
	}

	template < class OBJECT_TYPE_PTR >
	static inline void UnregisterMemberFunc( EventQueue_t hQueue, OBJECT_TYPE_PTR *pClass, void (OBJECT_TYPE_PTR::*pfnProxied)( ARGS... ) )
	{
		CFunctorCallback *pCallback = CreateFunctorCallback( pClass, pfnProxied );
		g_pEventSystem->UnregisterListener( Event_t::GetEventId(), hQueue, pCallback );
	}

	static EventId_t RegisterEvent( const char *pEventName )
	{
		return g_pEventSystem->RegisterEvent( pEventName );
	}
};

}
