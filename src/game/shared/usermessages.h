#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "shareddefs.h"

#include "networksystem/inetworksystem.h"

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

#include "usermessages_schema.h"
#include "proto_types.h"

//-----------------------------------------------------------------------------
class IUserMessageBinder;

//-----------------------------------------------------------------------------
// Purpose: Interface for registering and dispatching usermessages
// Shred code creates same ordered list on client/server
//-----------------------------------------------------------------------------
class CUserMessages
{
public:

	CUserMessages();
	~CUserMessages();

	bool DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg );
	void BindMessage( IUserMessageBinder *pMessageBinder );
	bool UnbindMessage( IUserMessageBinder *pMessageBinder );

private:
	typedef std::vector< IUserMessageBinder * > UserMessageBinderVec_t;
	typedef std::map< int, std::inplace_vector< UserMessageBinderVec_t, MAX_SPLITSCREEN_PLAYERS > > UserMessageHandlerMap_t;
	UserMessageHandlerMap_t m_UserMessageBinderMap;
};

extern CUserMessages *UserMessages();

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// One bound handler of a user message type. The typed parts are supplied by
// CUserMessageBinder as callables. A parsed message is passed around type-erased so
// that every handler bound to the same message id shares one parse.
class IUserMessageBinder
{
public:
	IUserMessageBinder( int type, int32 nExpectedPassthroughInReplay, const char *typeName,
		std::move_only_function< std::shared_ptr< void >( const void *msg, int size ) > parse,
		std::move_only_function< bool( const void *msg ) > invoke,
		std::move_only_function< std::size_t( const void *msg ) > byteSize,
		std::move_only_function< std::string( const void *msg ) > describe )
		: m_nType( type )
		, m_nExpectedPassthroughInReplay( nExpectedPassthroughInReplay )
		, m_pszTypeName( typeName )
		, m_Parse( std::move( parse ) )
		, m_Invoke( std::move( invoke ) )
		, m_ByteSize( std::move( byteSize ) )
		, m_Describe( std::move( describe ) )
	{
		UserMessages()->BindMessage( this );
	}

	~IUserMessageBinder()
	{
		UserMessages()->UnbindMessage( this );
	}

	IUserMessageBinder( const IUserMessageBinder & ) = delete;
	IUserMessageBinder &operator=( const IUserMessageBinder & ) = delete;

	int GetType() const { return m_nType; }

	std::shared_ptr< void > Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore );
	bool Invoke( const void *msg ) { return msg && m_Invoke( msg ); }
	const char *TypeName() const { return m_pszTypeName; }
	std::size_t ByteSize( const void *msg ) { return m_ByteSize( msg ); }
	std::string Describe( const void *msg ) { return m_Describe( msg ); }

private:
	int m_nType;
	int32 m_nExpectedPassthroughInReplay;
	const char *m_pszTypeName;
	std::move_only_function< std::shared_ptr< void >( const void *msg, int size ) > m_Parse;
	std::move_only_function< bool( const void *msg ) > m_Invoke;
	std::move_only_function< std::size_t( const void *msg ) > m_ByteSize;
	std::move_only_function< std::string( const void *msg ) > m_Describe;
};

//-----------------------------------------------------------------------------
class CUserMessageBinder
{
public:
	CUserMessageBinder() = default;

	template< int msgType, typename PB_OBJECT_TYPE >
	void Bind( std::move_only_function< bool ( const PB_OBJECT_TYPE & obj ) > handler )
	{
		m_pBind.reset();
		m_pBind = Create< msgType, PB_OBJECT_TYPE, 0 >( std::move( handler ) );
	}

	template< int msgType, typename PB_OBJECT_TYPE >
	void BindRealtimePassthrough( std::move_only_function< bool ( const PB_OBJECT_TYPE & obj ) > handler )
	{
		m_pBind.reset();
		m_pBind = Create< msgType, PB_OBJECT_TYPE, 1 >( std::move( handler ) );
	}

	void Unbind()
	{
		m_pBind.reset();
	}
	bool IsBound() const
	{
		return m_pBind != nullptr;
	}

private:
	template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
	static std::unique_ptr< IUserMessageBinder > Create( std::move_only_function< bool ( PB_OBJECT_TYPE const &msg ) > handler )
	{
		auto parse = []( const void *msg, int size ) -> std::shared_ptr< void >
		{
			auto pMsg = std::make_shared< PB_OBJECT_TYPE >();
			if ( !ks::proto::read_bytes( *pMsg, msg, size ) )
			{
				return nullptr;
			}
			return pMsg;
		};

		auto invoke = [handler = std::move( handler )]( const void *msg ) mutable -> bool
		{
			return handler( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		auto byteSize = []( const void *msg ) -> std::size_t
		{
			return ks::proto::byte_size( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		auto describe = []( const void *msg ) -> std::string
		{
			return ks::proto::to_text( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		return std::make_unique< IUserMessageBinder >( msgType, nExpectedPassthroughInReplay,
			ks::proto::type_name< PB_OBJECT_TYPE >(),
			std::move( parse ), std::move( invoke ), std::move( byteSize ), std::move( describe ) );
	}

	std::unique_ptr< IUserMessageBinder > m_pBind;
};

#else

// The server never binds user messages; shared code only declares binders.
class CUserMessageBinder
{
public:
	void Unbind() {}
	bool IsBound() const { return false; }
};

#endif
