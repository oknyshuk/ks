// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// GameEventManager.cpp: implementation of the CGameEventManager class.
//
//////////////////////////////////////////////////////////////////////

import ks.core.dbg;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.bit_ops;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.tokenset;
import ks.tier1.vector_ops;
#include "GameEventManager.h"
import std;
#include "filesystem_engine.h"
#include "server.h"
#include "client.h"
#include "cl_splitscreen.h"
#include "gameeventtransmitter.h"

constexpr auto NETWORKED_TYPE_BITS = 4;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static CGameEventManager s_GameEventManager;
CGameEventManager &g_GameEventManager = s_GameEventManager;

static const tokenset_t< int > s_GameListenerTypeMap[] =
{
	{ "SERVERSIDE",     CGameEventManager::SERVERSIDE     }, // this is a server side listener, event logger etc
	{ "CLIENTSIDE",     CGameEventManager::CLIENTSIDE     }, // this is a client side listenet, HUD element etc
	{ "CLIENTSTUB",     CGameEventManager::CLIENTSTUB     }, // this is a serverside stub for a remote client listener (used by engine only)
	{ nullptr,             -1                                }
};

static const tokenset_t< int > s_GameEventTypesMap[] =
{
	{ "local",    CGameEventManager::TYPE_LOCAL   }, // 0 : don't network this field
	{ "string",   CGameEventManager::TYPE_STRING  }, // 1 : zero terminated ASCII string
	{ "float",    CGameEventManager::TYPE_FLOAT   }, // 2 : float 32 bit
	{ "long",     CGameEventManager::TYPE_LONG    }, // 3 : signed int 32 bit
	{ "short",    CGameEventManager::TYPE_SHORT   }, // 4 : signed int 16 bit
	{ "byte",     CGameEventManager::TYPE_BYTE    }, // 5 : unsigned int 8 bit
	{ "bool",     CGameEventManager::TYPE_BOOL    }, // 6 : unsigned int 1 bit
	{ "uint64",   CGameEventManager::TYPE_UINT64  }, // 7 : unsigned int 64 bit
	{ "wstring",  CGameEventManager::TYPE_WSTRING }, // 8 : zero terminated wide char string
	{ nullptr,       -1                             }
};

static ConVar net_showevents( "net_showevents", "0", 0, "Dump game events to console (1=client only, 2=all)." );
static ConVar net_showeventlisteners( "net_showeventlisteners", "0", 0, "Show listening addition/removals" );

// Expose CVEngineServer to the engine.

static InterfaceReg s_Reg_CGameEventManager_IGameEventManager2( []() -> void * { return &s_GameEventManager; }, INTERFACEVERSION_GAMEEVENTSMANAGER2 );;

CGameEvent::CGameEvent( CGameEventDescriptor *descriptor, const char *name )
{
	contract_assert( descriptor );

	m_pDescriptor = descriptor;
	m_pDataKeys = new KeyValues( name );
	m_pDataKeys->SetInt( "splitscreenplayer", GET_ACTIVE_SPLITSCREEN_SLOT() );
}

CGameEvent::~CGameEvent()
{
	m_pDataKeys->deleteThis();
}

bool CGameEvent::GetBool( const char *keyName, bool defaultValue) const
{
	return m_pDataKeys->GetInt( keyName, defaultValue ) != 0;
}

int CGameEvent::GetInt( const char *keyName, int defaultValue) const
{
	return m_pDataKeys->GetInt( keyName, defaultValue );
}

uint64 CGameEvent::GetUint64( const char *keyName, uint64 defaultValue) const
{
	return m_pDataKeys->GetUint64( keyName, defaultValue );
}

float CGameEvent::GetFloat( const char *keyName, float defaultValue ) const
{
	return m_pDataKeys->GetFloat( keyName, defaultValue );
}

const char *CGameEvent::GetString( const char *keyName, const char *defaultValue ) const
{
	return m_pDataKeys->GetString( keyName, defaultValue );
}

const void *CGameEvent::GetPtr( const char *keyName ) const
{
	contract_assert( IsLocal() );
	return m_pDataKeys->GetPtr( keyName );
}

const wchar_t *CGameEvent::GetWString( const char *keyName, const wchar_t *defaultValue ) const
{
	return m_pDataKeys->GetWString( keyName, defaultValue );
}


void CGameEvent::SetBool( const char *keyName, bool value )
{
	m_pDataKeys->SetInt( keyName, value?1:0 );
}

void CGameEvent::SetInt( const char *keyName, int value )
{
	m_pDataKeys->SetInt( keyName, value );
}

void CGameEvent::SetUint64( const char *keyName, uint64 value )
{
	m_pDataKeys->SetUint64( keyName, value );
}

void CGameEvent::SetFloat( const char *keyName, float value )
{
	m_pDataKeys->SetFloat( keyName, value );
}

void CGameEvent::SetString( const char *keyName, const char *value )
{
	m_pDataKeys->SetString( keyName, value );
}

void CGameEvent::SetPtr( const char *keyName, const void *value )
{
	contract_assert( IsLocal() );
	m_pDataKeys->SetPtr( keyName, const_cast< void *>( value ) );
}

void CGameEvent::SetWString( const char* keyName, const wchar_t* value )
{
	m_pDataKeys->SetWString( keyName, value );
}

bool CGameEvent::IsEmpty( const char *keyName ) const
{
	return m_pDataKeys->IsEmpty( keyName );
}

const char *CGameEvent::GetName() const
{
	return m_pDataKeys->GetName();
}

bool CGameEvent::IsLocal() const
{
	return m_pDescriptor->local;
}

bool CGameEvent::IsReliable() const
{
	return m_pDescriptor->reliable;
}

CGameEventManager::CGameEventManager()
{
	Reset();
}

CGameEventManager::~CGameEventManager()
{
	Reset();
}

bool CGameEventManager::Init()
{
	Reset();

	LoadEventsFromFile( "resource/serverevents.res" );

	g_GameEventTransmitter.Init();

	return true;
}

void CGameEventManager::Shutdown()
{
	Reset();
}

void CGameEventManager::Reset()
{
	std::scoped_lock lock( m_mutex );
	int number = std::ssize( m_GameEvents );

	for (int i = 0; i<number; i++)
	{
		CGameEventDescriptor &e = m_GameEvents[ i ];

		if ( e.keys )
		{
			e.keys->deleteThis(); // free the value keys
			e.keys = nullptr;
		}

		e.listeners.clear(); e.listeners.shrink_to_fit();	// remove listeners
	}

	m_GameEvents.clear(); m_GameEvents.shrink_to_fit();
	m_Listeners.clear();
	m_EventFiles.clear();
	m_EventFileNames.clear();
	m_EventMap.clear();
	m_bClientListenersChanged = true;

	contract_assert( std::ssize( m_GameEvents ) == 0 );
}

bool CGameEventManager::HasClientListenersChanged( bool bReset /* = true  */)
{
	if ( !m_bClientListenersChanged )
		return false;

	if ( bReset )
		m_bClientListenersChanged = false;

	return true;
}

void CGameEventManager::WriteEventList(ks::net::CSVCMsg_GameEventList *msg)
{
	for (int i=0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		if ( descriptor.local )
			continue;

		contract_assert( descriptor.eventid >= 0 && descriptor.eventid < MAX_EVENT_NUMBER );

		ks::net::CSVCMsg_GameEventList::descriptor_t *pDescriptor = &msg->descriptors.emplace_back();
		const char *pName = descriptor.name;

		pDescriptor->eventid = descriptor.eventid;
		pDescriptor->name = pName;

		KeyValues *key = descriptor.keys->GetFirstSubKey();

		while ( key )
		{
			int type = key->GetInt();

			if ( type != TYPE_LOCAL )
			{
				ks::net::CSVCMsg_GameEventList::key_t *pKey = &pDescriptor->keys.emplace_back();

				pKey->type = type;
				pKey->name = key->GetName();
			}

			key = key->GetNextKey();
		}
	}
}

bool CGameEventManager::ParseEventList(const ks::net::CSVCMsg_GameEventList& msg)
{
	int i;
	std::scoped_lock lock( m_mutex );

	// reset eventids to -1 first
	for ( i=0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];
		descriptor.eventid = -1;
	}

	// map server event IDs
	int nNumEvents = msg.descriptors.size();
	for (i = 0; i<nNumEvents; i++)
	{
		const ks::net::CSVCMsg_GameEventList::descriptor_t& EventDescriptor = msg.descriptors[ i ];

		const char *name = EventDescriptor.name->c_str();

		CGameEventDescriptor *descriptor = GetEventDescriptor( name );

		// if event is known to client...
		if ( descriptor )
		{
			descriptor->eventid = EventDescriptor.eventid;

			// remove old definition list
			if ( descriptor->keys )
				descriptor->keys->deleteThis();

			descriptor->keys = new KeyValues("descriptor");

			int nNumKeys = EventDescriptor.keys.size();
			for (int key = 0; key < nNumKeys; key++)
			{
				const ks::net::CSVCMsg_GameEventList::key_t &Key = EventDescriptor.keys[ key ];

				descriptor->keys->SetInt( Key.name->c_str(), Key.type );
			}
		}
	}

	// force client to answer what events he listens to
	m_bClientListenersChanged = true;

	return true;
}

void CGameEventManager::WriteListenEventList(ks::net::CCLCMsg_ListenEvents *msg)
{
	std::bitset<MAX_EVENT_NUMBER> EventArray;

	EventArray.reset();

	// and know tell the server what events we want to listen to
	for (int i=0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		if ( descriptor.local )
		{
			continue; // event isn't networked
		}

		bool bHasClientListener = false;

		for ( int j=0; j<std::ssize( descriptor.listeners ); j++ )
		{
			CGameEventCallback *listener = descriptor.listeners[j];

			if ( listener->m_nListenerType == CGameEventManager::CLIENTSIDE )
			{
				// if we have a client side listener and server knows this event, add it
				bHasClientListener = true;
				break;
			}
		}

		if ( !bHasClientListener )
			continue;

		if ( descriptor.eventid == -1 )
		{
			const char *pName = descriptor.name;
			DevMsg("Warning! Client listens to event '%s' unknown by server.\n", pName );
			continue;
		}

		EventArray.set( descriptor.eventid );
	}

	int count = ( std::ssize( m_GameEvents ) + 31 ) / 32;
	for( int i = 0; i < count; i++ )
	{
		msg->event_mask.emplace_back( ks::get_word( EventArray, i ) );
	}
}

IGameEvent *CGameEventManager::CreateEvent( CGameEventDescriptor *descriptor, const char *name )
{
	std::scoped_lock lock( m_mutex );
	return new CGameEvent ( descriptor, name );
}

IGameEvent *CGameEventManager::CreateEvent( const char *name, bool bForce, int *pCookie )
{
	std::scoped_lock lock( m_mutex );
	if ( !name || !name[0] )
		return nullptr;

	CGameEventDescriptor *descriptor = GetEventDescriptor( name, pCookie );

	// check if this event name is known
	if ( !descriptor )
	{
		DevMsg( "CreateEvent: event '%s' not registered.\n", name );
		return nullptr;
	}

	// event is known but no one listen to it
	if ( descriptor->listeners.empty() && !bForce )
	{
		return nullptr;
	}

	// create & return the new event
	return new CGameEvent ( descriptor, name );
}

ConVar display_game_events("display_game_events", "0", FCVAR_CHEAT );
bool CGameEventManager::FireEvent( IGameEvent *event, bool bServerOnly )
{
	if( display_game_events.GetBool() )
	{
		Msg("Game Event Fired: %s\n", event->GetName() );
	}

	return FireEventIntern( event, bServerOnly, false );
}

bool CGameEventManager::FireEventClientSide( IGameEvent *event )
{
	return FireEventIntern( event, false, true );
}

IGameEvent *CGameEventManager::DuplicateEvent( IGameEvent *event )
{
	CGameEvent *gameEvent = event;

	if ( !gameEvent )
		return nullptr;

	// create new instance
	const char *pName = gameEvent->m_pDescriptor->name;
	CGameEvent *newEvent = new CGameEvent ( gameEvent->m_pDescriptor, pName );

	// free keys
	newEvent->m_pDataKeys->deleteThis();

	// and make copy
	newEvent->m_pDataKeys = gameEvent->m_pDataKeys->MakeCopy();

	return newEvent;
}

void CGameEventManager::ConPrintEvent( IGameEvent *event)
{
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
		return;

	KeyValues *key = descriptor->keys->GetFirstSubKey();

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		switch ( type )
		{
		case TYPE_LOCAL : ConMsg( "- \"%s\" = \"%s\" (local)\n", keyName, event->GetString(keyName) ); break;
		case TYPE_STRING : ConMsg( "- \"%s\" = \"%s\"\n", keyName, event->GetString(keyName) ); break;
		case TYPE_WSTRING : ConMsg( "- \"%s\" = \"" "%ls" "\"\n", keyName, event->GetWString(keyName) ); break;
		case TYPE_FLOAT : ConMsg( "- \"%s\" = \"%.2f\"\n", keyName, event->GetFloat(keyName) ); break;
		default: ConMsg( "- \"%s\" = \"%i\"\n", keyName, event->GetInt(keyName) ); break;
		}
		key = key->GetNextKey();
	}
}

bool CGameEventManager::FireEventIntern( IGameEvent *event, bool bServerOnly, bool bClientOnly )
{
	std::scoped_lock lock( m_mutex );
	if ( event == nullptr )
		return false;

	contract_assert( !(bServerOnly && bClientOnly) ); // it can't be both

	CVProfScope _( "CGameEventManager::FireEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, bClientOnly ? BUDGETFLAG_CLIENT : ( bServerOnly ? BUDGETFLAG_SERVER : BUDGETFLAG_OTHER ) );

	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( descriptor == nullptr )
	{
		DevMsg( "FireEvent: event '%s' not registered.\n", event->GetName() );
		FreeEvent( event );
		return false;
	}

	// show game events in console
	if ( net_showevents.GetInt() > 0 )
	{
		if ( bClientOnly )
		{
#ifndef DEDICATED
			const char *pName = descriptor->name;
			ConMsg( "Game event \"%s\", Tick %i:\n", pName, GetBaseLocalClient().GetClientTickCount() );
			ConPrintEvent( event );
#endif
		}
		else if ( net_showevents.GetInt() > 1 )
		{
			const char *pName = descriptor->name;
			ConMsg( "Server event \"%s\", Tick %i:\n", pName, sv.GetTick() );
			ConPrintEvent( event );
		}


	}

	for ( int i = 0; i < std::ssize( descriptor->listeners ); i++ )
	{
		CGameEventCallback *listener = descriptor->listeners[ i ];

		contract_assert( listener );

		// don't trigger server listners for clientside only events
		if ( listener->m_nListenerType == SERVERSIDE && bClientOnly )
			continue;

        // don't trigger clientside events, if not explicit a clientside event
		if ( listener->m_nListenerType == CLIENTSIDE && !bClientOnly )
			continue;

		// don't broadcast events if server side only
		if ( listener->m_nListenerType == CLIENTSTUB && (bServerOnly || bClientOnly) )
			continue;

		// if the event is local, don't tell clients about it
		if ( listener->m_nListenerType == CLIENTSTUB && descriptor->local )
			continue;

		// TODO optimized the serialize event for clients, call only once and not per client

		// fire event in this listener module
		listener->m_Callback( event );
	}

	if ( bClientOnly || ( !V_stricmp( "portal2", COM_GetModDirectory() ) ) )
	{
		// Pass all client only events  to the game event transmiter
		g_GameEventTransmitter.TransmitGameEvent( event );
	}

	// free event resources
	FreeEvent( event );

	return true;
}

bool CGameEventManager::SerializeEvent( IGameEvent *event, ks::net::CSVCMsg_GameEvent *eventMsg )
{
	std::scoped_lock lock( m_mutex );
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	contract_assert( descriptor );

	eventMsg->eventid = descriptor->eventid;

	// now iterate trough all fields described in gameevents.res and put them in the buffer

	KeyValues * key = descriptor->keys->GetFirstSubKey();

	if ( net_showevents.GetInt() > 2 )
	{
		const char *pName = descriptor->name;
		DevMsg("Serializing event '%s' (%i):\n", pName, descriptor->eventid );
	}

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		if ( net_showevents.GetInt() > 2 )
		{
			DevMsg(" - %s (%s)\n", keyName, s_GameEventTypesMap->GetNameByToken( type ) );
		}

		if( type != TYPE_LOCAL )
		{
			ks::net::CSVCMsg_GameEvent::key_t *pKey = &eventMsg->keys.emplace_back();

			//Make sure every key is used in the event
			// Assert( event->FindKey(keyName) && "GameEvent field not found in passed KeyValues" );

			pKey->type = type;

			// see s_GameEventTypesMap for index
			switch ( type )
			{
			case TYPE_STRING: pKey->val_string = event->GetString( keyName, ""); break;
			case TYPE_FLOAT : pKey->val_float = event->GetFloat( keyName, 0.0f); break;
			case TYPE_LONG	: pKey->val_long = event->GetInt( keyName, 0); break;
			case TYPE_SHORT	: pKey->val_short = event->GetInt( keyName, 0); break;
			case TYPE_BYTE	: pKey->val_byte = event->GetInt( keyName, 0); break;
			case TYPE_BOOL	: pKey->val_bool = !!event->GetInt( keyName, 0); break;
			case TYPE_UINT64: pKey->val_uint64 = event->GetUint64( keyName, 0); break;
			case TYPE_WSTRING:
				{
					const wchar_t *pStr = event->GetWString( keyName, L"");
					pKey->val_wstring = std::string( ( const char * ) pStr, std::wcslen( pStr ) + 1 );
				}
				break;
			default: DevMsg(1, "CGameEventManager: unknown type %i for key '%s'.\n", type, key->GetName() ); break;
			}
		}

		key = key->GetNextKey();
	}

	if ( net_showevents.GetInt() > 2 )
	{
		int nBytes = int( ks::proto::byte_size( *eventMsg ) );
		Msg( " took %d bits, %d bytes\n", nBytes * 8, nBytes );
	}

	descriptor->numSerialized++;
	descriptor->totalSerializedBits += int( ks::proto::byte_size( *eventMsg ) ) * 8;

	return true;
}

IGameEvent *CGameEventManager::UnserializeEvent( const ks::net::CSVCMsg_GameEvent& eventMsg )
{
	std::scoped_lock lock( m_mutex );

	// read event id

	int eventid = eventMsg.eventid;

	// get event description
	CGameEventDescriptor *descriptor = GetEventDescriptor( eventid );

	if ( descriptor == nullptr )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: unknown event id %i.\n", eventid );
		return nullptr;
	}

	// create new event
	const char *pName = descriptor->name;
	IGameEvent *event = CreateEvent( descriptor, pName );

	if ( !event )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: failed to create event %s.\n", pName );
		return nullptr;
	}

	int nNumKeys = eventMsg.keys.size();
	KeyValues * key = descriptor->keys->GetFirstSubKey();

	for( int i = 0; key && ( i < nNumKeys ); i++, key = key->GetNextKey() )
	{
		const ks::net::CSVCMsg_GameEvent::key_t &KeyEvent = eventMsg.keys[ i ];
		const char * keyName = key->GetName();

		int type = KeyEvent.type;

		switch ( type )
		{
		case TYPE_LOCAL		: break; // ignore
		case TYPE_STRING	: event->SetString( keyName, KeyEvent.val_string->c_str() ); break;
		case TYPE_FLOAT		: event->SetFloat( keyName, KeyEvent.val_float ); break;
		case TYPE_LONG		: event->SetInt( keyName, KeyEvent.val_long ); break;
		case TYPE_SHORT		: event->SetInt( keyName, KeyEvent.val_short ); break;
		case TYPE_BYTE		: event->SetInt( keyName, KeyEvent.val_byte ); break;
		case TYPE_BOOL		: event->SetInt( keyName, KeyEvent.val_bool ); break;
		case TYPE_UINT64	: event->SetUint64( keyName, KeyEvent.val_uint64 ); break;
		case TYPE_WSTRING	: event->SetWString( keyName, (wchar_t*)KeyEvent.val_wstring->data() ); break;

		default: DevMsg(1, "CGameEventManager: unknown type %i for key '%s' [%s].\n", type, key->GetName(), pName ); break;
		}
	}

	descriptor->numUnSerialized++;
	descriptor->totalUnserializedBits += int( ks::proto::byte_size( eventMsg ) ) * 8;

	return event;
}

KeyValues* CGameEventManager::GetEventDataTypes( IGameEvent* event )
{
	if ( event == nullptr )
		return nullptr;

	CGameEventDescriptor* descriptor = GetEventDescriptor( event );
	if ( descriptor == nullptr )
		return nullptr;

	return descriptor->keys;
}

// returns true if this listener is listens to given event
bool CGameEventManager::FindListener( const void *listener, const char *name )
{
	std::scoped_lock lock( m_mutex );
	CGameEventDescriptor *pDescriptor = GetEventDescriptor( name );

	if ( !pDescriptor )
		return false; // event is unknown

	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( !pCallback )
		return false; // listener is unknown

	// see if listener is in the list for this event
	return ks::is_valid_index( pDescriptor->listeners, ks::find_index( pDescriptor->listeners, pCallback ) );
}

CGameEventCallback* CGameEventManager::FindEventListener( const void* pOwner )
{
	for (int i=0; i < std::ssize( m_Listeners ); i++ )
	{
		CGameEventCallback *listener = m_Listeners[ i ].get();

		if ( listener->m_pOwner == pOwner )
		{
			return listener;
		}
	}

	return nullptr;
}

void CGameEventManager::RemoveListener( const void *listener )
{
	std::scoped_lock lock( m_mutex );
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == nullptr )
	{
		return;
	}

	// remove reference from events
	for (int i=0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor &et = m_GameEvents[ i ];
		ks::erase_first( et.listeners, pCallback );
	}

	if ( pCallback->m_nListenerType == CLIENTSIDE )
	{
		m_bClientListenersChanged = true;
	}

	// and from global list
	std::erase_if( m_Listeners, [pCallback]( const auto &p ) { return p.get() == pCallback; } );	// frees the callback
}

bool CGameEventManager::AddListenerGlobal( const void *listener, GameEventCallback_t callback, bool bServerSide )
{
	std::scoped_lock lock( m_mutex );

	if ( !listener )
		return false;

	for ( int i = 0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[ i ];

		AddListener( listener, callback, descriptor, bServerSide ? SERVERSIDE : CLIENTSIDE );
	}

	return true;
}

int CGameEventManager::LoadEventsFromFile( const char * filename )
{
	std::scoped_lock lock( m_mutex );
	if ( !m_EventFiles.contains( filename ) )
	{
		m_EventFileNames.push_back( m_EventFiles.intern( filename ) );
	}

	KeyValues * key = new KeyValues(filename);
	KeyValues::AutoDelete autodelete_key( key );

	if  ( !key->LoadFromFile( g_pFileSystem, filename, "GAME" ) )
		return false;

	int count = 0;	// number new events

	KeyValues * subkey = key->GetFirstSubKey();

	while ( subkey )
	{
		if ( subkey->GetDataType() == KeyValues::TYPE_NONE )
		{
			RegisterEvent( subkey );
			count++;
		}

		subkey = subkey->GetNextKey();
	}

	if ( net_showevents.GetBool() )
		DevMsg( "Event System loaded %i events from file %s.\n", (int)std::ssize( m_GameEvents ), filename );

	return std::ssize( m_GameEvents );
}

void CGameEventManager::ReloadEventDefinitions()
{
	for ( int i=0; i< std::ssize( m_EventFileNames ); i++ )
	{
		const char *filename = m_EventFileNames[i].data();
		LoadEventsFromFile( filename );
	}

	// we are the server, build string table now
	int number = std::ssize( m_GameEvents );

	for (int j = 0; j<number; j++)
	{
		m_GameEvents[j].eventid = j;
	}
}

bool CGameEventManager::AddListener( const void *listener, GameEventCallback_t callback, const char *event, bool bServerSide )
{
	std::scoped_lock lock( m_mutex );
	if ( !event )
		return false;

	// look for the event descriptor
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
	{
		Warning( "CGameEventManager::AddListener: event '%s' unknown.\n", event );
		return false;	// that should not happen
	}

	return AddListener( listener, callback, descriptor, bServerSide ? SERVERSIDE : CLIENTSIDE );
}

bool CGameEventManager::AddListener( const void *listener, GameEventCallback_t callback, CGameEventDescriptor *descriptor,  int nListenerType )
{
	std::scoped_lock lock( m_mutex );
	if ( !listener || !descriptor )
		return false;	// bahh

	// check if we already know this listener
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == nullptr )
	{
		// add new callback
		pCallback = m_Listeners.emplace_back( std::make_unique< CGameEventCallback >( listener, callback, nListenerType ) ).get();
	}
	else
	{
		// make sure that it hasn't changed:
		contract_assert( pCallback->m_nListenerType == nListenerType );
		contract_assert( pCallback->m_pOwner == listener );
	}

	// add to event listeners list if not already in there
	if ( ks::find_index( descriptor->listeners, pCallback ) == -1 )
	{
		descriptor->listeners.push_back( pCallback );

		if ( net_showeventlisteners.GetBool() )
		{
			const char *name = descriptor->name;
			Msg("[GAMEEVENT] Event '%s' added %s listener %p\n",
				name ? name : "UNKNOWN",
				s_GameListenerTypeMap->GetNameByToken( nListenerType ),
				listener );
		}

		if ( nListenerType == CLIENTSIDE )
			m_bClientListenersChanged = true;
	}

	return true;
}


bool CGameEventManager::RegisterEvent( KeyValues * event)
{
	if ( event == nullptr )
		return false;

	std::scoped_lock lock( m_mutex );
	if ( std::ssize( m_GameEvents ) == MAX_EVENT_NUMBER )
	{
		DevMsg( "CGameEventManager: couldn't register event '%s', limit reached (%i).\n",
			event->GetName(), MAX_EVENT_NUMBER );
		return false;
	}

	const char *name = event->GetName();

	CGameEventDescriptor *descriptor = GetEventDescriptor( name );

	if ( !descriptor )
	{
		// event not known yet, create new one
		int index = std::ssize( m_GameEvents ); m_GameEvents.emplace_back();
		descriptor =  &m_GameEvents[ index ];

		descriptor->name = m_EventMap.emplace( event->GetName(), index ).first->first.c_str();
	}
	else
	{
		// descriptor already know, but delete old definitions
		descriptor->keys->deleteThis();
	}

	// create new descriptor keys
	descriptor->keys = new KeyValues("descriptor");

	KeyValues *subkey = event->GetFirstSubKey();

	// interate through subkeys

	while ( subkey )
	{
		const char *keyName = subkey->GetName();

		// ok, check it's data type
		const char * type = subkey->GetString();

		if ( !V_strcmp( "local", keyName) )
		{
			descriptor->local = V_atoi( type ) != 0;
		}
		else if ( !V_strcmp( "reliable", keyName) )
		{
			descriptor->reliable = V_atoi( type ) != 0;
		}
		else
		{
			int i = s_GameEventTypesMap->GetToken( type );

			if ( i < 0 )
			{
				descriptor->keys->SetInt( keyName, 0 );	// unknown
				DevMsg( "CGameEventManager:: unknown type '%s' for key '%s' [%s].\n", type, subkey->GetName(), name );
			}
			else
			{
				// set data type
				descriptor->keys->SetInt( keyName, i );	// set data type
			}
		}

		subkey = subkey->GetNextKey();
	}

	return true;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(IGameEvent *event)
{
	CGameEvent *gameevent = event;

	if ( !gameevent )
		return nullptr;

	return gameevent->m_pDescriptor;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(int eventid) // returns event name or NULL
{
	if ( eventid < 0 )
		return nullptr;

	for ( int i = 0; i < std::ssize( m_GameEvents ); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( descriptor->eventid == eventid )
			return descriptor;
	}

	return nullptr;
}

void CGameEventManager::FreeEvent( IGameEvent *event )
{
	std::scoped_lock lock( m_mutex );
	if ( !event )
		return;

	delete event;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(const char * name, int *pCookie)
{
	const uint32 cookieBit = 0x80000000;
	const uint32 cookieMask = ~cookieBit;

	if ( !name || !name[0] )
		return nullptr;

	if ( pCookie && *pCookie )
	{
		int gameEventIndex = uint32(*pCookie) & cookieMask;
		CGameEventDescriptor *pDescriptor = &m_GameEvents[gameEventIndex];
		if ( !V_stricmp( pDescriptor->name, name ) )
		{
			return pDescriptor;
		}
	}
	auto eventMapIndex = m_EventMap.find( name );
	if ( eventMapIndex == m_EventMap.end() )
		return nullptr;
	int gameEventIndex = eventMapIndex->second;
	if ( pCookie )
	{
		*pCookie = cookieBit | gameEventIndex;
	}
	return &m_GameEvents[gameEventIndex];
}

void CGameEventManager::VerifyListenerList( void )
{
	int nGameEventCount = std::ssize( m_GameEvents );
	for ( int iEvent = 0; iEvent < nGameEventCount; ++iEvent )
	{
		CGameEventDescriptor *pEvent = &m_GameEvents[ iEvent ];
		const char *pName = pEvent->name;
		if ( !pEvent )
		{
			Msg( "VerifyListenerList-Bug: Bad event in list! (%d)\n", iEvent);
			continue;
		}

		int nListenerCount = std::ssize( pEvent->listeners );
		for ( int iListen = 0; iListen < nListenerCount; ++iListen )
		{
			CGameEventCallback *pListener = pEvent->listeners[ iListen ];
			if ( !pListener)
			{
				Msg( "VerifyListenerList-Bug: Bad listener in list! (%s)\n", pName );
				continue;
			}
		}
	}
}

void CGameEventManager::DumpEventNetworkStats( void )
{
	// find longest name
	int len = 0;
	for ( int i = 0; i < std::ssize( m_GameEvents ); ++i )
	{
		CGameEventDescriptor &e = m_GameEvents[ i ];
		const char *name = e.name;
		if ( !name )
		{
			continue;
		}

		int l = V_strlen( name );
		if ( l > len )
		{
			len = l;
		}
	}

	//        ----- ----- ------- ------- ------- -------
	Msg( "%*s  Out    In  OutBits InBits  OutSize InSize  Notes\n", len, "Name" );
	//            %5d %5d %7d %7d %7d %7d


	for ( int i = 0; i < std::ssize( m_GameEvents ); ++i )
	{
		CGameEventDescriptor &e = m_GameEvents[ i ];
		const char *name = e.name;
		if ( !name )
		{
			continue;
		}

		if ( !e.numSerialized && !e.numUnSerialized )
		{
			continue;
		}

		Msg( "%*s %5d %5d %7d %7d %7d %7d",
			 len,
			 name,
			 e.numSerialized,
			 e.numUnSerialized,
			 e.totalSerializedBits,
			 e.totalUnserializedBits,
			 e.numSerialized ? e.totalSerializedBits / e.numSerialized : 0,
			 e.numUnSerialized ? e.totalUnserializedBits / e.numUnSerialized : 0 );
		if ( e.local )
		{
			Msg( " local" );
		}
		if ( e.reliable )
		{
			Msg( " reliable" );
		}
		Msg( "\n" );
	}

}


static void net_dumpeventstats( const CCommand &args );
static ConCommand net_dumpeventstats_command( "net_dumpeventstats", net_dumpeventstats, "Dumps out a report of game event network usage", 0 );
static void net_dumpeventstats( const CCommand &args )
{
	s_GameEventManager.DumpEventNetworkStats();
}
