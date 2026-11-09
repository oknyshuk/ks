#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "inetchannelinfo.h"
import std;

class	IDemoRecorder;
class	INetMessage;
class	INetChannelHandler;
class	INetChannelInfo;
class	INetMessageBinder;
typedef struct netpacket_s netpacket_t;

#ifndef NET_PACKET_ST_DEFINED
#define NET_PACKET_ST_DEFINED
typedef struct netpacket_s
{
	ns_address		from;		// sender address
	int				source;		// received source 
	double			received;	// received time
	unsigned char	*data;		// pointer to raw packet data
	bf_read			message;	// easy bitbuf data access
	int				size;		// size in bytes
	int				wiresize;   // size in bytes before decompression
	bool			stream;		// was send as stream
	struct netpacket_s *pNext;	// for internal use, should be NULL in public
} netpacket_t;
#endif // NET_PACKET_ST_DEFINED


class INetChannel : public INetChannelInfo
{
public:

	void	SetDataRate(float rate);

	bool	RegisterMessage(INetMessageBinder *msg);
	bool	UnregisterMessage(INetMessageBinder *msg);

	bool	StartStreaming( unsigned int challengeNr );
	void	ResetStreaming( void );
	void	SetTimeout(float seconds, bool bForceExact = false);
	void	SetDemoRecorder(IDemoRecorder *recorder);
	void	SetChallengeNr(unsigned int chnr);
	
	void	Reset( void );
	void	Clear( void );
	void	Shutdown(const char *reason);
	
	void	ProcessPlayback( void );
	bool	ProcessStream( void );
	void	ProcessPacket( struct netpacket_s* packet, bool bHasHeader );
			
	bool	SendNetMsg(INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	[[gnu::always_inline]] inline bool SendNetMsg(INetMessage const &msg, bool bForceReliable = false, bool bVoice = false ) { return SendNetMsg( *( (INetMessage *) &msg ), bForceReliable, bVoice ); }
	bool	SendData(bf_write &msg, bool bReliable = true);
	bool	SendFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile );
	void	DenyFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile );
	void	RequestFile_OLD(const char *filename, unsigned int transferID);	// get rid of this function when we version the 
	void	SetChoked( void );
	int		SendDatagram(bf_write *data);		
	bool	Transmit(bool onlyReliable = false);

	const ns_address &GetRemoteAddress( void ) const;
	INetChannelHandler *GetMsgHandler( void ) const;
	int				GetDropNumber( void ) const;
	int				GetSocket( void ) const;
	unsigned int	GetChallengeNr( void ) const;
	void			GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck );
	void			SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck );
		
	void	UpdateMessageStats( int msggroup, int bits);
	bool	CanPacket( void ) const;
	bool	IsOverflowed( void ) const;
	bool	IsTimedOut( void ) const;
	bool	HasPendingReliableData( void );

	void	SetFileTransmissionMode(bool bBackgroundMode);
	void	SetCompressionMode( bool bUseCompression );
	unsigned int RequestFile(const char *filename, bool bIsReplayDemoFile );
	float	GetTimeSinceLastReceived( void ) const;	// get time since last received packet in seconds

	void	SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice = false );

	bool	IsNull() const;
	int		GetNumBitsWritten( bool bReliable );
	void	SetInterpolationAmount( float flInterpolationAmount );
	void	SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation, float flFrameStartTimeStdDeviation );

	// Max # of payload bytes before we must split/fragment the packet
	void	SetMaxRoutablePayloadSize( int nSplitSize );
	int		GetMaxRoutablePayloadSize();

	// For routing messages to a different handler
	bool	SetActiveChannel( INetChannel *pNewChannel );
	void	AttachSplitPlayer( int nSplitPlayerSlot, INetChannel *pChannel );
	void	DetachSplitPlayer( int nSplitPlayerSlot );

	bool	IsRemoteDisconnected() const;

	bool	WasLastMessageReliable() const; // True if the last (or currently processing) message was sent via the reliable channel

	const unsigned char * GetChannelEncryptionKey() const;	// Returns a buffer with channel encryption key data (network layer determines the buffer size)

	bool	EnqueueVeryLargeAsyncTransfer( INetMessage &msg );	// Enqueues a message for a large async transfer
};

// Handler registration of one net message type on a net channel. The typed parts (how to
// parse the message, what to do with it) are callables supplied by CNetMessageBinder::Bind.
class INetMessageBinder
{
public:
	INetMessageBinder( INetChannel *pNetChannel, int type,
		std::move_only_function< INetMessage *( bf_read & ) > create,
		std::move_only_function< bool( const INetMessage & ) > process )
		: m_NetChannel( pNetChannel )
		, m_nType( type )
		, m_Create( std::move( create ) )
		, m_Process( std::move( process ) )
	{
		if ( m_NetChannel )
		{
			m_NetChannel->RegisterMessage( this );
		}
	}

	~INetMessageBinder()
	{
		if ( m_NetChannel )
		{
			m_NetChannel->UnregisterMessage( this );
		}
	}

	INetMessageBinder( const INetMessageBinder & ) = delete;
	INetMessageBinder &operator=( const INetMessageBinder & ) = delete;

	int GetType( void ) const { return m_nType; } // returns module specific header tag eg svc_serverinfo

	void SetNetChannel( INetChannel *netchan ) // netchannel this message is from/for
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

	INetMessage *CreateFromBuffer( bf_read &buffer ) { return m_Create( buffer ); }
	bool Process( const INetMessage &src ) { return m_Process( src ); }

private:
	INetChannel	*m_NetChannel;	// netchannel this message is from/for
	int m_nType;
	std::move_only_function< INetMessage *( bf_read & ) > m_Create;
	std::move_only_function< bool( const INetMessage & ) > m_Process;
};
