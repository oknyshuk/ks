#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;

#define INTERFACEVERSION_GAMEEVENTSMANAGER	"GAMEEVENTSMANAGER001"	// old game event manager, don't use it!
#define INTERFACEVERSION_GAMEEVENTSMANAGER2	"GAMEEVENTSMANAGER002"	// new game event manager,

//-----------------------------------------------------------------------------
// Purpose: Engine interface into global game event management
//-----------------------------------------------------------------------------

/* 

The GameEventManager keeps track and fires of all global game events. Game events 
are fired by game.dll for events like player death or team wins. Each event has a
unique name and comes with a KeyValue structure providing informations about this
event. Some events are generated also by the engine.

Events are networked to connected clients and invoked there to. Therefore you
have to specify all data fields and there data types in an public resource
file which is parsed by server and broadcasted to it's clients. A typical game 
event is defined like this:

	"game_start"				// a new game starts
	{
		"roundslimit"	"long"		// max round
		"timelimit"		"long"		// time limit
		"fraglimit"		"long"		// frag limit
		"objective"		"string"	// round objective
	}

All events must have unique names (case sensitive) and may have a list
of data fields. each data field must specify a data type, so the engine
knows how to serialize/unserialize that event for network transmission.
Valid data types are string, float, long, short, byte & bool. If a 
data field should not be broadcasted to clients, use the type "local".
*/
#define MAX_EVENT_NAME_LENGTH	32		// max game event name length
#define MAX_EVENT_BITS			9		// max bits needed for an event index
#define MAX_EVENT_NUMBER		(1<<MAX_EVENT_BITS)		// max number of events allowed
#define MAX_EVENT_BYTES			1024	// max size in bytes for a serialized event

class CGameEvent;
class CGameEventManager;
namespace ks::net { struct CSVCMsg_GameEvent; }
namespace ks::net { struct CSVCMsg_GameEventList; }
namespace ks::net { struct CCLCMsg_ListenEvents; }

// The engine has exactly one game event implementation and one event manager.
using IGameEvent = CGameEvent;
using IGameEventManager2 = CGameEventManager;

// Called by the manager if the event just occured. Non-owning: what it refers to must
// outlive the registration.
using GameEventCallback_t = std::function_ref< void( IGameEvent * ) >;

// Class for visiting every key of data on an event (see CGameEvent::ForEventData).
// The visitor type is static: a visitor hides the Visit* functions it cares about.
// return true to keep iterating, false to abort iteration
class IGameEventVisitor2
{
public:
	bool VisitLocal( const char* name, const void* value ) { return true; }
	bool VisitString( const char* name, const char* value ) { return true; }
	bool VisitFloat( const char* name, float value ) { return true; }
	bool VisitInt( const char* name, int value ) { return true; }
	bool VisitUint64( const char* name, uint64 value ) { return true; }
	bool VisitWString( const char* name, const wchar_t* value ) { return true; }
	bool VisitBool( const char* name, bool value ) { return true; }
};

class CGameEventCallback
{
public:
	CGameEventCallback( const void *owner, GameEventCallback_t callback, int nListenerType )
		: m_pOwner( owner ), m_Callback( callback ), m_nListenerType( nListenerType ) {}

	const void			*m_pOwner;			// identity of the listener (what RemoveListener is given)
	GameEventCallback_t	m_Callback;			// called when an event fires
	int					m_nListenerType;	// client or server side ?
};

class CGameEventDescriptor
{
public:
	CGameEventDescriptor()
	{
		eventid = -1;
		keys = nullptr;
		local = false;
		reliable = true;
		name = nullptr;

		numSerialized = 0;
		numUnSerialized = 0;
		totalSerializedBits = 0;
		totalUnserializedBits = 0;
	}

public:
	int			eventid;	// network index number, -1 = not networked
	const char	*name;		// key in CGameEventManager::m_EventMap
	KeyValues	*keys;		// KeyValue describing data types, if NULL only name 
	std::vector< CGameEventCallback* >	listeners;	// registered listeners
	bool		local;		// local event, never tell clients about that
	bool		reliable;	// send this event as reliable message

	// Extra data for network monitoring
	int numSerialized;
	int numUnSerialized;
	int totalSerializedBits;
	int totalUnserializedBits;
};

class CGameEvent
{
public:
	CGameEvent( CGameEventDescriptor *descriptor, const char *name );
	~CGameEvent();

	const char *GetName() const;	// get event name

	bool  IsReliable() const; // if event handled reliable
	bool  IsLocal() const; // if event is never networked
	bool  IsEmpty(const char *keyName = nullptr) const; // check if data field exists
	
	// Data access
	bool  GetBool( const char *keyName = nullptr, bool defaultValue = false ) const;
	int   GetInt( const char *keyName = nullptr, int defaultValue = 0 ) const;
	uint64 GetUint64( const char *keyName = nullptr, uint64 defaultValue = 0 ) const;
	float GetFloat( const char *keyName = nullptr, float defaultValue = 0.0f ) const;
	const char *GetString( const char *keyName = nullptr, const char *defaultValue = "" ) const;
	const wchar_t *GetWString( const char *keyName = nullptr, const wchar_t *defaultValue = L"" ) const;
	const void *GetPtr( const char *keyName = nullptr ) const;	// LOCAL only

	void SetBool( const char *keyName, bool value );
	void SetInt( const char *keyName, int value );
	void SetUint64( const char *keyName, uint64 value );
	void SetFloat( const char *keyName, float value );
	void SetString( const char *keyName, const char *value );
	void SetWString( const char *keyName, const wchar_t *value );
	void SetPtr( const char *keyName, const void * value );	// LOCAL only

	// returns true if iteration aborted normally, false if it was aborted by the visitor callback
	template< class Visitor >
	bool ForEventData( Visitor *visitor ) const;

	CGameEventDescriptor	*m_pDescriptor;
	KeyValues				*m_pDataKeys;
};

// NOTE: Every non-threadsafe externally-callable function must lock m_mutex before modifying the event manager
// member variables.  Client & server threads can call into this class simultaneously
//
// The engine's game event manager. Client and server reach it through the
// INTERFACEVERSION_GAMEEVENTSMANAGER2 interface; its members are exported from libengine.so.
class CGameEventManager
{
public:
	enum
	{
		SERVERSIDE = 0,		// this is a server side listener, event logger etc
		CLIENTSIDE,			// this is a client side listenet, HUD element etc
		CLIENTSTUB,			// this is a serverside stub for a remote client listener (used by engine only)
	};

	enum
	{
		TYPE_LOCAL = 0,	// not networked
		TYPE_STRING,	// zero terminated ASCII string
		TYPE_FLOAT,		// float 32 bit
		TYPE_LONG,		// signed int 32 bit
		TYPE_SHORT,		// signed int 16 bit
		TYPE_BYTE,		// unsigned int 8 bit
		TYPE_BOOL,		// unsigned int 1 bit
		TYPE_UINT64,	// unsigned int 64 bit
		TYPE_WSTRING,	// zero terminated wide char string
		TYPE_COUNT, 
	};

	CGameEventManager();
	~CGameEventManager();

	// load game event descriptions from a file eg "resource\gameevents.res"
	int LoadEventsFromFile( const char *filename );

	// removes all and anything
	void Reset();

	// adds a listener for a particular event. The listener is identified by the owner pointer.
	bool AddListener( const void *owner, GameEventCallback_t callback, const char *name, bool bServerSide );

	// returns true if this listener is listens to given event
	bool FindListener( const void *owner, const char *name );

	// removes a listener 
	void RemoveListener( const void *owner );

	// add a listener that listens to all events. 
	bool AddListenerGlobal( const void *owner, GameEventCallback_t callback, bool bServerSide );

	// create an event by name, but doesn't fire it. returns NULL is event is not
	// known or no listener is registered for it. bForce forces the creation even if no listener is active
	IGameEvent *CreateEvent( const char *name, bool bForce = false, int *pCookie = nullptr );

	// fires a server event created earlier, if bDontBroadcast is set, event is not send to clients
	bool FireEvent( IGameEvent *event, bool bDontBroadcast = false );

	// fires an event for the local client only, should be used only by client code
	bool FireEventClientSide( IGameEvent *event );

	// create a new copy of this event, must be free later
	IGameEvent *DuplicateEvent( IGameEvent *event );

	// if an event was created but not fired for some reason, it has to bee freed, same UnserializeEvent
	void FreeEvent( IGameEvent *event );

	// write/read event to/from bitbuffer
	bool SerializeEvent( IGameEvent *event, ks::net::CSVCMsg_GameEvent *eventMsg );
	IGameEvent *UnserializeEvent( const ks::net::CSVCMsg_GameEvent& eventMsg ); // create new KeyValues, must be deleted

	KeyValues* GetEventDataTypes( IGameEvent* event );

	void DumpEventNetworkStats();

public:
	bool Init();
	void Shutdown();
	void ReloadEventDefinitions();	// called by server on new map
	bool AddListener( const void *owner, GameEventCallback_t callback, CGameEventDescriptor *descriptor, int nListenerType );

	CGameEventDescriptor *GetEventDescriptor( const char *name, int *pCookie = nullptr );
	CGameEventDescriptor *GetEventDescriptor( IGameEvent *event );
	CGameEventDescriptor *GetEventDescriptor( int eventid );

	void WriteEventList(ks::net::CSVCMsg_GameEventList *msg);
	bool ParseEventList(const ks::net::CSVCMsg_GameEventList& msg);

	void WriteListenEventList(ks::net::CCLCMsg_ListenEvents *msg);
	bool HasClientListenersChanged( bool bReset = true );
	void ConPrintEvent( IGameEvent *event);
	
	// Debug!
	void VerifyListenerList( void );
	
protected:

	IGameEvent *CreateEvent( CGameEventDescriptor *descriptor, const char *name );
	bool RegisterEvent( KeyValues * keys );
	void UnregisterEvent(int index);
	bool FireEventIntern( IGameEvent *event, bool bServerSide, bool bClientOnly );
	CGameEventCallback* FindEventListener( const void* owner );
	
	std::vector< CGameEventDescriptor >	m_GameEvents;	// list of all known events
	std::vector< std::unique_ptr< CGameEventCallback > >	m_Listeners;	// owns every callback; descriptors only point at them
	ks::intern_pool						m_EventFiles;	// list of all loaded event files
	std::vector< std::string_view >		m_EventFileNames;	// load order, views into m_EventFiles
	ks::idict< int >					m_EventMap;	// name -> index in m_GameEvents
	std::recursive_mutex				m_mutex;		// lock this when modifying the event table

	bool	m_bClientListenersChanged;	// true every time client changed listeners
};

template< class Visitor >
bool CGameEvent::ForEventData( Visitor *visitor ) const
{
	CGameEventDescriptor *descriptor = m_pDescriptor;

	bool iterate = true;
	for ( KeyValues* key = descriptor->keys->GetFirstSubKey(); key && iterate; key = key->GetNextKey() )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		// see s_GameEventTypesMap for index
		switch ( type )
		{
		case CGameEventManager::TYPE_LOCAL: iterate = visitor->VisitLocal( keyName, GetPtr( keyName ) ); break;
		case CGameEventManager::TYPE_STRING: iterate = visitor->VisitString( keyName, GetString( keyName, "" ) ); break;
		case CGameEventManager::TYPE_FLOAT: iterate = visitor->VisitFloat( keyName, GetFloat( keyName, 0.0f ) ); break;
		case CGameEventManager::TYPE_LONG: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_SHORT: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_BYTE: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_BOOL: iterate = visitor->VisitBool( keyName, GetBool( keyName, false ) ); break;
		case CGameEventManager::TYPE_UINT64: iterate = visitor->VisitUint64( keyName, GetUint64( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_WSTRING: iterate = visitor->VisitWString( keyName, GetWString( keyName, L"" ) ); break;
		}
	}

	return iterate;
}
