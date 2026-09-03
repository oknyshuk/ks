//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#ifdef _WIN32
#pragma once
#pragma warning(disable : 4100)	// unreferenced formal parameter
#endif

#include <inetmessage.h>
#include <checksum_crc.h>
#include <checksum_md5.h>
#include <const.h>
#include <utlvector.h>
#include "qlimits.h"
#include "mathlib/vector.h"
#include <soundflags.h>
#include <bitbuf.h>
#include <inetchannel.h>
#include "protocol.h"
#include <inetmsghandler.h>
#include <igameevents.h>
#include <bitvec.h>
#include "networksystem/inetworksystem.h"
#include <engine/iserverplugin.h>
#include <color.h>
#include <tier0/tslist.h>
#include <tier1/utldelegate.h>
#include "tier1/utlstring.h"
#include "tier1/tokenset.h"
#include "netmessages_signon.h"

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

// lwss - eliminates a conflict with "Status" in protobuf
#ifdef Status
#undef Status
#endif
// lwss end

#include "netmessages_schema.h"
#include "proto_types.h"

// The schema keeps its own namespace: protobuf's classes of the same names are still
// reachable through the user-message headers, which reference two of these types.

// The message body is the schema struct itself; this adds the framing the net channel
// needs around it.
template< int msgType, typename PB_OBJECT_TYPE, int groupType = INetChannelInfo::GENERIC, bool bReliable = true >
class CNetMessagePB : public INetMessage, public PB_OBJECT_TYPE
{
public:
	typedef CNetMessagePB< msgType, PB_OBJECT_TYPE, groupType, bReliable > MyType_t;
	typedef PB_OBJECT_TYPE PBType_t;
	static const int sk_Type = msgType;

	CNetMessagePB() : m_bReliable( bReliable ) {}
	virtual ~CNetMessagePB() {}

	virtual bool ReadFromBuffer( bf_read &buffer )
	{
		const int size = buffer.ReadVarInt32();
		if ( size < 0 || size > NET_MAX_PAYLOAD || size > buffer.GetNumBytesLeft() )
			return false;

		// A byte-aligned buffer can be parsed in place.
		if ( ( buffer.GetNumBitsRead() % 8 ) == 0 )
		{
			const bool ok = ks::proto::read_bytes( body(),
				buffer.GetBasePointer() + buffer.GetNumBytesRead(), size );
			buffer.SeekRelative( size * 8 );
			return ok;
		}

		void *shifted = stackalloc( size );
		return buffer.ReadBytes( shifted, size ) &&
		       ks::proto::read_bytes( body(), shifted, size );
	}

	virtual bool WriteToBuffer( bf_write &buffer ) const
	{
		const int size = int( ks::proto::byte_size( body() ) );

		if ( ( buffer.GetNumBitsWritten() % 8 ) == 0 )
		{
			const int framed = size + 1 + buffer.ByteSizeVarInt32( GetType() ) +
			                   buffer.ByteSizeVarInt32( size );
			if ( buffer.GetNumBytesLeft() < framed )
				return false;

			buffer.WriteVarInt32( GetType() );
			buffer.WriteVarInt32( size );
			ks::proto::write( body(), (std::byte *)buffer.GetData() + buffer.GetNumBytesWritten() );
			buffer.SeekToBit( buffer.GetNumBitsWritten() + size * 8 );
			return true;
		}

		void *shifted = stackalloc( size );
		ks::proto::write( body(), (std::byte *)shifted );
		buffer.WriteVarInt32( GetType() );
		buffer.WriteVarInt32( size );
		return buffer.WriteBytes( shifted, size );
	}

	virtual const char *ToString() const
	{
		m_toString = ks::proto::to_text( body() );
		return m_toString.c_str();
	}

	virtual int GetType() const { return msgType; }
	virtual size_t GetSize() const { return sizeof( *this ); }
	virtual const char *GetName() const { return ks::proto::type_name<PBType_t>(); }
	virtual int GetGroup() const { return groupType; }

	virtual void SetReliable( bool state ) { m_bReliable = state; }
	virtual bool IsReliable() const { return m_bReliable; }

	virtual INetMessage *Clone() const
	{
		MyType_t *pClone = new MyType_t;
		pClone->body() = body();
		pClone->m_bReliable = m_bReliable;
		return pClone;
	}

	PBType_t &body() { return *this; }
	const PBType_t &body() const { return *this; }

	void Clear() { body() = PBType_t{}; }
	void CopyFrom( const PBType_t &o ) { body() = o; }

protected:
	bool m_bReliable;
	mutable std::string m_toString;
};


class CNetMessageBinder
{
public:
	CNetMessageBinder()
		: m_pBind( NULL )
	{
	}

	~CNetMessageBinder()
	{
		delete m_pBind;
	}

	template< class _N >
	void Bind( INetChannel *pNetChannel, CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > handler )
	{
		delete m_pBind;
		m_pBind = new BindParams<_N>( pNetChannel, handler );
	}

	void Unbind()
	{
		delete m_pBind;
		m_pBind = NULL;
	}

	bool IsBound() const
	{
		return m_pBind != NULL;
	}

private:
	template < class _N >
	struct BindParams : public INetMessageBinder
	{
		BindParams( INetChannel *pNetChannel, CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > handler )
			: m_NetChannel( pNetChannel )
			, m_handler( handler )
		{
			if ( m_NetChannel )
			{
				m_NetChannel->RegisterMessage( this );
			}
		}

		virtual ~BindParams()
		{
			if ( m_NetChannel )
			{
				m_NetChannel->UnregisterMessage( this );
			}
		}

		virtual int	GetType( void ) const
		{
			return _N::sk_Type;
		}

		virtual void SetNetChannel(INetChannel * netchan)
		{
			if( m_NetChannel != netchan )
			{
				if( m_NetChannel )
					m_NetChannel->UnregisterMessage( this );

				m_NetChannel = netchan;

				if( m_NetChannel )
					m_NetChannel->RegisterMessage( this );
			}
		}

		virtual INetMessage *CreateFromBuffer( bf_read &buffer )
		{
			INetMessage *pMsg = new typename _N::MyType_t;
			if ( !pMsg->ReadFromBuffer( buffer ) )
			{
				delete pMsg;
				return NULL;
			}
			return pMsg;
		}

		virtual bool Process( const INetMessage &src )
		{
			const typename _N::MyType_t &typedSrc = static_cast< const typename _N::MyType_t & >( src );

			Assert( m_handler );
			if( m_handler )
			{
				return m_handler( static_cast< typename _N::PBType_t const & >( typedSrc ) );
			}
			return false;
		}

		INetChannel	*m_NetChannel;	// netchannel this message is from/for
		CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > m_handler;
	};

	INetMessageBinder *m_pBind;
};

///////////////////////////////////////////////////////////////////////////////////////
// bidirectional net messages:
///////////////////////////////////////////////////////////////////////////////////////

class CNETMsg_Tick_t : public CNetMessagePB< ks::net::net_Tick, ks::net::CNETMsg_Tick >
{
public:
	static float FrametimeToFloat( uint32 frametime ) { return ( float )frametime / 1000000.0f; }

	CNETMsg_Tick_t( int tick, float host_computationtime, float host_computationtime_stddeviation, float host_framestarttime_std_deviation )
	{
		SetReliable( false );
		this->tick = tick;
		this->host_computationtime = MIN( ( uint32 )( 1000000.0 * host_computationtime ), 1000000u );
		this->host_computationtime_std_deviation = MIN( ( uint32 )( 1000000.0 * host_computationtime_stddeviation ), 1000000u );
		this->host_framestarttime_std_deviation = MIN( ( uint32 )( 1000000.0 * host_framestarttime_std_deviation ), 1000000u );
	}
};

class CNETMsg_StringCmd_t : public CNetMessagePB< ks::net::net_StringCmd, ks::net::CNETMsg_StringCmd, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_StringCmd_t( const char *command )
	{
		this->command = command;
	}
};

class CNETMsg_PlayerAvatarData_t : public CNetMessagePB< ks::net::net_PlayerAvatarData, ks::net::CNETMsg_PlayerAvatarData, INetChannelInfo::PAINTMAP >
{
	// 12 KB player avatar 64x64 rgb only no alpha
	// WARNING-WARNING-WARNING
	// This message is extremely large for our net channels
	// and must be pumped through special fragmented waiting list
	// via chunk-based ack mechanism!
	// See: INetChannel::EnqueueVeryLargeAsyncTransfer
	// WARNING-WARNING-WARNING
public:
	CNETMsg_PlayerAvatarData_t() {}
	CNETMsg_PlayerAvatarData_t( uint32 unAccountID, void const *pvData, uint32 cbData )
	{
		accountid = unAccountID;
		rgb = std::string( ( const char * ) pvData, cbData );
	}
};

class CNETMsg_SignonState_t : public CNetMessagePB< ks::net::net_SignonState, ks::net::CNETMsg_SignonState, INetChannelInfo::SIGNON >
{
public:
	CNETMsg_SignonState_t( int state, int spawncount )
	{
		signon_state = state;
		spawn_count = spawncount;
		num_server_players = 0;
	}
};

inline void NetMsgSetCVarUsingDictionary( ks::net::CMsg_CVars::CVar *convar, char const * name, char const * value )
{
	convar->value = value;

	if ( 0 ) ( void ) 0;
	/** Removed for partner depot **/
	else
	{
#ifdef _DEBUG
		DevWarning( "Missing dictionary entry for cvar '%s'\n", name );
#endif
		convar->name = name;
	}
}

inline void NetMsgExpandCVarUsingDictionary( ks::net::CMsg_CVars::CVar *convar )
{
	if ( convar->name.has_value() )
		return;
	switch ( convar->dictionary_name )
	{
	case 0: return;
	/** Removed for partner depot **/
	default:
		DevWarning( "Invalid dictionary entry for cvar # %d\n", uint32( convar->dictionary_name ) );
		convar->name = "undefined";
		break;
	}
}

inline const char * NetMsgGetCVarUsingDictionary( ks::net::CMsg_CVars::CVar const &convar )
{
	if ( convar.name.has_value() )
		return convar.name->c_str();
	switch ( convar.dictionary_name )
	{
	case 0: return "";
	/** Removed for partner depot **/
default:
	DevWarning( "Invalid dictionary entry for cvar # %d\n", uint32( convar.dictionary_name ) );
	return "undefined";
	}
}

class CNETMsg_SetConVar_t : public CNetMessagePB< ks::net::net_SetConVar, ks::net::CNETMsg_SetConVar, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_SetConVar_t() {}
	CNETMsg_SetConVar_t( const char * name, const char * value )
	{
		AddToTail( name, value );
	}
	void AddToTail( const char * name, const char * value )
	{
		NetMsgSetCVarUsingDictionary( &convars.mut().cvars.emplace_back(), name, value );
	}
};

typedef CNetMessagePB< ks::net::net_NOP, ks::net::CNETMsg_NOP >											CNETMsg_NOP_t;
typedef CNetMessagePB< ks::net::net_Disconnect, ks::net::CNETMsg_Disconnect >								CNETMsg_Disconnect_t;
typedef CNetMessagePB< ks::net::net_File, ks::net::CNETMsg_File >											CNETMsg_File_t;
typedef CNetMessagePB< ks::net::net_SplitScreenUser, ks::net::CNETMsg_SplitScreenUser >					CNETMsg_SplitScreenUser_t;

///////////////////////////////////////////////////////////////////////////////////////
// Client messages: Sent from the client to the server
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< ks::net::clc_SplitPlayerConnect, ks::net::CCLCMsg_SplitPlayerConnect >							CCLCMsg_SplitPlayerConnect_t;
typedef CNetMessagePB< ks::net::clc_Move, ks::net::CCLCMsg_Move, INetChannelInfo::MOVE, false >						CCLCMsg_Move_t;
typedef CNetMessagePB< ks::net::clc_ClientInfo, ks::net::CCLCMsg_ClientInfo > 										CCLCMsg_ClientInfo_t;
typedef CNetMessagePB< ks::net::clc_VoiceData, ks::net::CCLCMsg_VoiceData, INetChannelInfo::VOICE, false >			CCLCMsg_VoiceData_t;
typedef CNetMessagePB< ks::net::clc_BaselineAck, ks::net::CCLCMsg_BaselineAck >										CCLCMsg_BaselineAck_t;
typedef CNetMessagePB< ks::net::clc_ListenEvents, ks::net::CCLCMsg_ListenEvents >										CCLCMsg_ListenEvents_t;
typedef CNetMessagePB< ks::net::clc_RespondCvarValue, ks::net::CCLCMsg_RespondCvarValue >								CCLCMsg_RespondCvarValue_t;
typedef CNetMessagePB< ks::net::clc_LoadingProgress, ks::net::CCLCMsg_LoadingProgress >								CCLCMsg_LoadingProgress_t;
typedef CNetMessagePB< ks::net::clc_CmdKeyValues, ks::net::CCLCMsg_CmdKeyValues >										CCLCMsg_CmdKeyValues_t;
typedef CNetMessagePB< ks::net::clc_HltvReplay, ks::net::CCLCMsg_HltvReplay >											CCLCMsg_HltvReplay_t;

class CCLCMsg_FileCRCCheck_t : public CNetMessagePB< ks::net::clc_FileCRCCheck, ks::net::CCLCMsg_FileCRCCheck >
{
public:
	// Warning: These routines may use the va() function...
	static void SetPath( ks::net::CCLCMsg_FileCRCCheck& msg, const char *path );
	static const char *GetPath( const ks::net::CCLCMsg_FileCRCCheck& msg );
	static void SetFileName( ks::net::CCLCMsg_FileCRCCheck& msg, const char *fileName );
	static const char *GetFileName( const ks::net::CCLCMsg_FileCRCCheck& msg );
};

///////////////////////////////////////////////////////////////////////////////////////
// Server messages: Sent from the server to the client
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< ks::net::svc_ServerInfo, ks::net::CSVCMsg_ServerInfo, INetChannelInfo::SIGNON >					CSVCMsg_ServerInfo_t;
typedef CNetMessagePB< ks::net::svc_ClassInfo, ks::net::CSVCMsg_ClassInfo, INetChannelInfo::SIGNON >						CSVCMsg_ClassInfo_t;
typedef CNetMessagePB< ks::net::svc_SendTable, ks::net::CSVCMsg_SendTable, INetChannelInfo::SIGNON >					    CSVCMsg_SendTable_t;
typedef CNetMessagePB< ks::net::svc_Print, ks::net::CSVCMsg_Print, INetChannelInfo::GENERIC, false >						CSVCMsg_Print_t;
typedef CNetMessagePB< ks::net::svc_SetPause, ks::net::CSVCMsg_SetPause >													CSVCMsg_SetPause_t;
typedef CNetMessagePB< ks::net::svc_SetView, ks::net::CSVCMsg_SetView >												    CSVCMsg_SetView_t;
typedef CNetMessagePB< ks::net::svc_CreateStringTable, ks::net::CSVCMsg_CreateStringTable, INetChannelInfo::SIGNON >	    CSVCMsg_CreateStringTable_t;
typedef CNetMessagePB< ks::net::svc_UpdateStringTable, ks::net::CSVCMsg_UpdateStringTable, INetChannelInfo::STRINGTABLE >	CSVCMsg_UpdateStringTable_t;
typedef CNetMessagePB< ks::net::svc_VoiceInit, ks::net::CSVCMsg_VoiceInit, INetChannelInfo::SIGNON >						CSVCMsg_VoiceInit_t;
typedef CNetMessagePB< ks::net::svc_VoiceData, ks::net::CSVCMsg_VoiceData, INetChannelInfo::VOICE, false >				CSVCMsg_VoiceData_t;
typedef CNetMessagePB< ks::net::svc_FixAngle, ks::net::CSVCMsg_FixAngle, INetChannelInfo::GENERIC, false >			    CSVCMsg_FixAngle_t;
typedef CNetMessagePB< ks::net::svc_Prefetch, ks::net::CSVCMsg_Prefetch, INetChannelInfo::SOUNDS >					    CSVCMsg_Prefetch_t;
typedef CNetMessagePB< ks::net::svc_CrosshairAngle, ks::net::CSVCMsg_CrosshairAngle >									    CSVCMsg_CrosshairAngle_t;
typedef CNetMessagePB< ks::net::svc_BSPDecal, ks::net::CSVCMsg_BSPDecal >												    CSVCMsg_BSPDecal_t;
typedef CNetMessagePB< ks::net::svc_SplitScreen, ks::net::CSVCMsg_SplitScreen >										    CSVCMsg_SplitScreen_t;
typedef CNetMessagePB< ks::net::svc_GetCvarValue, ks::net::CSVCMsg_GetCvarValue >										    CSVCMsg_GetCvarValue_t;
typedef CNetMessagePB< ks::net::svc_Menu, ks::net::CSVCMsg_Menu, INetChannelInfo::GENERIC, false >					    CSVCMsg_Menu_t;
typedef CNetMessagePB< ks::net::svc_UserMessage, ks::net::CSVCMsg_UserMessage, INetChannelInfo::USERMESSAGES, false >		CSVCMsg_UserMessage_t;
typedef CNetMessagePB< ks::net::svc_PaintmapData, ks::net::CSVCMsg_PaintmapData, INetChannelInfo::PAINTMAP >			    CSVCMsg_PaintmapData_t;
typedef CNetMessagePB< ks::net::svc_GameEvent, ks::net::CSVCMsg_GameEvent, INetChannelInfo::EVENTS >					    CSVCMsg_GameEvent_t;
typedef CNetMessagePB< ks::net::svc_GameEventList, ks::net::CSVCMsg_GameEventList >									    CSVCMsg_GameEventList_t;
typedef CNetMessagePB< ks::net::svc_TempEntities, ks::net::CSVCMsg_TempEntities, INetChannelInfo::TEMPENTS, false >	    CSVCMsg_TempEntities_t;
typedef CNetMessagePB< ks::net::svc_PacketEntities, ks::net::CSVCMsg_PacketEntities, INetChannelInfo::ENTITIES >		    CSVCMsg_PacketEntities_t;
typedef CNetMessagePB< ks::net::svc_Sounds, ks::net::CSVCMsg_Sounds, INetChannelInfo::SOUNDS >							CSVCMsg_Sounds_t;
typedef CNetMessagePB< ks::net::svc_EntityMessage, ks::net::CSVCMsg_EntityMsg, INetChannelInfo::ENTMESSAGES, false >		CSVCMsg_EntityMsg_t;
typedef CNetMessagePB< ks::net::svc_CmdKeyValues, ks::net::CSVCMsg_CmdKeyValues >											CSVCMsg_CmdKeyValues_t;
typedef CNetMessagePB< ks::net::svc_EncryptedData, ks::net::CSVCMsg_EncryptedData, INetChannelInfo::ENCRYPTED >			CSVCMsg_EncryptedData_t;
typedef CNetMessagePB< ks::net::svc_HltvReplay, ks::net::CSVCMsg_HltvReplay, INetChannelInfo::ENTITIES >					CSVCMsg_HltvReplay_t;
typedef CNetMessagePB< ks::net::svc_Broadcast_Command, ks::net::CSVCMsg_Broadcast_Command, INetChannelInfo::STRINGCMD >	CSVCMsg_Broadcast_Command_t;


///////////////////////////////////////////////////////////////////////////////////////
// Utility classes
///////////////////////////////////////////////////////////////////////////////////////

class CmdKeyValuesHelper
{
public:

	static void CLCMsg_SetKeyValues( ks::net::CCLCMsg_CmdKeyValues& msg, const KeyValues *keyValues );
	static KeyValues* CLCMsg_GetKeyValues ( const ks::net::CCLCMsg_CmdKeyValues& msg );

	static void SVCMsg_SetKeyValues( ks::net::CSVCMsg_CmdKeyValues& msg, const KeyValues *keyValues );
	static KeyValues *SVCMsg_GetKeyValues ( const ks::net::CSVCMsg_CmdKeyValues& msg );
};

class INetChannel;
class CmdEncryptedDataMessageCodec
{
public:
	static bool SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, INetMessage *pMsgPlaintextInput, char const *key );
	static bool SVCMsg_EncryptedData_Process( ks::net::CSVCMsg_EncryptedData const &msgEncryptedInput, INetChannel *pProcessingChannel, char const *key );
};

//////////////////////////////////////////////////////////////////////////
// Helper class to share network buffers in the entire process
//////////////////////////////////////////////////////////////////////////

class net_scratchbuffer_t
{
public:
	net_scratchbuffer_t()
	{
		m_pBufferNetMaxMessage = sm_NetScratchBuffers.Get();
		if ( !m_pBufferNetMaxMessage )
			m_pBufferNetMaxMessage = new buffer_t;
	}
	~net_scratchbuffer_t()
	{
		sm_NetScratchBuffers.PutObject( m_pBufferNetMaxMessage );
	}
	byte * GetBuffer() const
	{
		return m_pBufferNetMaxMessage->buf;
	}
	int Size() const
	{
		return NET_MAX_MESSAGE;
	}

private:
	struct buffer_t { byte buf[ NET_MAX_MESSAGE ]; };
	buffer_t *m_pBufferNetMaxMessage;	// buffer that is allocated and returned to shared pool

private:
	net_scratchbuffer_t( const net_scratchbuffer_t& );				// FORBID
	net_scratchbuffer_t& operator=( const net_scratchbuffer_t& );	// FORBID
	static CTSPool< buffer_t > sm_NetScratchBuffers;
};

#endif // NETMESSAGES_H
