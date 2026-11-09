// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "cdll_client_int.h"
#include "usermessages.h"


static ConVar cl_show_usermessage( "cl_show_usermessage", "0", 0, "Shows the incoming user messages for this client and dumps them out the type and size of the messages to the console. Setting this to 2 will display message contents as well" ); // filter incoming voice data

//-----------------------------------------------------------------------------
extern int CL_GetHltvReplayDelay();
std::shared_ptr< void > IUserMessageBinder::Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore )
{
	if ( size < 0 || size > NET_MAX_PAYLOAD )
	{
		return nullptr;
	}

	if ( CL_GetHltvReplayDelay() )
	{
		if ( nPassthroughFlags != m_nExpectedPassthroughInReplay )
		{
			// this is a wrong timeline message. Ignore it.
			bSilentIgnore = true;
			return nullptr;
		}
	}

	return m_Parse( msg, size );
}


//-----------------------------------------------------------------------------
CUserMessages::CUserMessages()
{
}

//-----------------------------------------------------------------------------
CUserMessages::~CUserMessages()
{
	m_UserMessageBinderMap.clear();
}

// we might wanna move this definition here::
// template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
// virtual void * CUserMessageBinder::BindParams<msgType, PB_OBJECT_TYPE, nExpectedPassthroughInReplay >::Parse( int32 nPassthroughFlags, const void *msg, int size )

//-----------------------------------------------------------------------------
bool CUserMessages::DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg )
{
	auto index = m_UserMessageBinderMap.find( msg_type );
	if ( index == m_UserMessageBinderMap.end() )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Unknown msg type %i\n", msg_type );
		return true;
	}
		
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	// Handlers may bind/unbind while invoked; map nodes are stable, so the reference stays valid.
	UserMessageBinderVec_t &binders = index->second[ nSlot ];
	if ( std::ssize( binders ) == 0 )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	IUserMessageBinder *pHandler = binders[0];
	if ( !pHandler )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	bool bSilentIgnore = false;
	std::shared_ptr< void > pMsg = pHandler->Parse( nPassthroughFlags, msg, size, bSilentIgnore );

	if ( bSilentIgnore )
	{
		//DevMsg( "CUserMessages::DispatchUserMessage: Silently ignoring alt-timeline msg type %i\n", msg_type );
		return true;
	}

	if ( !pMsg )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Parse error msg type=%i\n", msg_type );
		contract_assert( 0 );
		return false;
	}

	//handle logging to the console if this is enabled
	if ( cl_show_usermessage.GetBool() )
	{
		Msg("DispatchUserMessage - %s(%d) bytes: %d\n", pHandler->TypeName(), msg_type, int( pHandler->ByteSize( pMsg.get() ) ) );
		//handle message content display if they have it set to a value > 1
		if( cl_show_usermessage.GetInt() > 1 )
			Msg("%s", pHandler->Describe( pMsg.get() ).c_str() );
	}

	bool result = true;
	for ( int i = 0; i < std::ssize( binders ); i++ )
	{
		IUserMessageBinder *h = binders[i];
		if ( h )
		{
			result = h->Invoke( pMsg.get() );
		}

		if ( !result )
		{
			break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void CUserMessages::BindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return;
	}

	int message = pMessageBinder->GetType();

	auto [index, bInserted] = m_UserMessageBinderMap.try_emplace( message );
	if ( bInserted )
	{
		index->second.assign( MAX_SPLITSCREEN_PLAYERS, {} );
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	index->second[ GET_ACTIVE_SPLITSCREEN_SLOT() ].push_back( pMessageBinder );
}

//-----------------------------------------------------------------------------
bool CUserMessages::UnbindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return true;
	}

	int message = pMessageBinder->GetType();

	auto index = m_UserMessageBinderMap.find( message );
	
	if ( index == m_UserMessageBinderMap.end() )
	{
		return false;
	}

	for ( int split = 0; split < MAX_SPLITSCREEN_PLAYERS; ++split )
	{
		auto &binders = index->second[ split ];
		for ( int i = 0; i < std::ssize( binders ); i++ )
		{
			if ( binders[i] == pMessageBinder )
			{
				binders.erase( binders.begin() + i );
				return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Singleton
CUserMessages *UserMessages()
{
	static CUserMessages g_UserMessages;
	return &g_UserMessages;
}
