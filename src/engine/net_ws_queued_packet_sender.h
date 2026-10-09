#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Used to match against certain debug values of cvars.
#include "net_ws_headers.h"

inline constexpr auto NET_QUEUED_PACKET_THREAD_DEBUG_VALUE = 581304;

class INetChannel;

class IQueuedPacketSender
{
public:
	virtual bool Setup() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsRunning() = 0;
	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan ) =  0;
	virtual void QueuePacket( INetChannel *pChan, SOCKET s, const char  *buf, int len, const ns_address &to, uint32 msecDelay ) = 0;
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const = 0;
};

extern IQueuedPacketSender *g_pQueuedPackedSender;
extern ConVar net_queued_packet_thread;
