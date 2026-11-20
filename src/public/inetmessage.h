#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: INetMessage interface

import std;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;

class INetMsgHandler;
class INetMessage;
class INetChannel;

// A net message: the closed set of message classes is listed in common/netmessage_list.h; the operations
// that depend on the message class dispatch on m_kind (defined in common/netmessages.cpp).
class INetMessage
{
public:
	INetMessage( unsigned char kind, int type, int group, bool reliable )
		: m_kind( kind ), m_bReliable( reliable ), m_nType( type ), m_nGroup( group ) {}

	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).
	
	void	SetNetChannel( INetChannel * netchan ) { DebuggerBreak(); return; }
	void	SetReliable( bool state ) { m_bReliable = state; }	// set to true if it's a reliable message
	
	bool	Process( void ) { DebuggerBreak(); return false; }
	
	bool	ReadFromBuffer( bf_read &buffer ); // returns true if parsing was OK
	bool	WriteToBuffer( bf_write &buffer ) const;	// returns true if writing was OK
		
	bool	IsReliable( void ) const { return m_bReliable; }  // true, if message needs reliable handling
	
	int				GetType( void ) const { return m_nType; } // returns module specific header tag eg svc_serverinfo
	int				GetGroup( void ) const { return m_nGroup; }	// returns net message group of this message
	const char		*GetName( void ) const;	// returns network message name, eg "svc_serverinfo"

	INetChannel		*GetNetChannel( void ) const { DebuggerBreak(); return nullptr; }

	const char		*ToString( void ) const; // returns a human readable string about message content
	std::size_t			GetSize() const;

	// Deletes the message as the class it really is
	void			DeleteThis();

protected:
	unsigned char	m_kind;		// net_message_kind of the message class (common/netmessage_list.h)
	bool			m_bReliable;
	int				m_nType;
	int				m_nGroup;
};

// For owners of messages that are not released by a plain delete
struct INetMessageDeleter
{
	void operator()( INetMessage *pMsg ) const { pMsg->DeleteThis(); }
};

class INetChannelHandler
{
public:
	enum class handler_kind_t : unsigned char { client, state };
	explicit INetChannelHandler( handler_kind_t kind ) : m_handler_kind( kind ) {}


	void ConnectionStart(INetChannel *chan);	// called first time network channel is established
	void ConnectionStop( );	// called first time network channel is established

	void ConnectionClosing(const char *reason); // network channel is being closed by remote site

	void ConnectionCrashed(const char *reason); // network error occured

	void PacketStart(int incoming_sequence, int outgoing_acknowledged);	// called each time a new packet arrived

	void PacketEnd( void ); // all messages has been parsed

	void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile); // other side request a file for download

	void FileReceived(const char *fileName, unsigned int transferID, bool isReplayDemoFile); // we received a file

	void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile);	// a file request was denied by other side

	void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile);	// we sent a file

	bool ChangeSplitscreenUser( int nSplitScreenUserSlot ); // interleaved networking used by SS system is changing the SS player slot that the subsequent messages pertain to

protected:
	handler_kind_t m_handler_kind;
};
