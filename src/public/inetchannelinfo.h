#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr int FLOW_OUTGOING = 0;
inline constexpr int FLOW_INCOMING = 1;
#define MAX_FLOWS		2		// in & out

class INetChannelInfo
{
public:

	enum {
		GENERIC = 0,	// must be first and is default group
		LOCALPLAYER,	// bytes for local player entity update
		OTHERPLAYERS,	// bytes for other players update
		ENTITIES,		// all other entity bytes
		SOUNDS,			// game sounds
		EVENTS,			// event messages
		TEMPENTS,		// temp entities
		USERMESSAGES,	// user messages
		ENTMESSAGES,	// entity messages
		VOICE,			// voice data
		STRINGTABLE,	// a stringtable update
		MOVE,			// client move cmds
		STRINGCMD,		// string command
		SIGNON,			// various signondata
		PAINTMAP,			// paintmap data
		ENCRYPTED,		// encrypted data
		TOTAL,			// must be last and is not a real group
	};

	enum ENetworkEventType_t
	{
		k_ENetworkEventType_Generic = 0,		// Generic network event
		k_ENetworkEventType_TimedOut = 1,		// Network connection timed out
	};
	
	const char  *GetName( void ) const;	// get channel name
	const char  *GetAddress( void ) const; // get channel IP address as string
	float		GetTime( void ) const;	// current net time
	float		GetTimeConnected( void ) const;	// get connection time in seconds
	int			GetBufferSize( void ) const;	// netchannel packet history size
	int			GetDataRate( void ) const; // send data rate in byte/sec
	
	bool		IsLoopback( void ) const;	// true if loopback channel
	bool		IsTimingOut( void ) const;	// true if timing out
	bool		IsPlayback( void ) const;	// true if demo playback

	float		GetLatency( int flow ) const;	 // current latency (RTT), more accurate but jittering
	float		GetAvgLatency( int flow ) const; // average packet latency in seconds
	float		GetAvgLoss( int flow ) const;	 // avg packet loss[0..1]
	float		GetAvgChoke( int flow ) const;	 // avg packet choke[0..1]
	float		GetAvgData( int flow ) const;	 // data flow in bytes/sec
	float		GetAvgPackets( int flow ) const; // avg packets/sec
	int			GetTotalData( int flow ) const;	 // total flow in/out in bytes
	int			GetTotalPackets( int flow ) const;
	int			GetSequenceNr( int flow ) const;	// last send seq number
	bool		IsValidPacket( int flow, int frame_number ) const; // true if packet was not lost/dropped/chocked/flushed
	float		GetPacketTime( int flow, int frame_number ) const; // time when packet was send
	int			GetPacketBytes( int flow, int frame_number, int group ) const; // group size of this packet
	bool		GetStreamProgress( int flow, int *received, int *total ) const;  // TCP progress if transmitting
	float		GetTimeSinceLastReceived( void ) const;	// get time since last recieved packet in seconds
	float		GetCommandInterpolationAmount( int flow, int frame_number ) const;
	void		GetPacketResponseLatency( int flow, int frame_number, int *pnLatencyMsecs, int *pnChoke ) const;
	void		GetRemoteFramerate( float *pflFrameTime, float *pflFrameTimeStdDeviation, float *pflFrameStartTimeStdDeviation ) const;

	float		GetTimeoutSeconds() const;
};
