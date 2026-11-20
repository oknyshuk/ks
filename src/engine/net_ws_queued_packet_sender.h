#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Used to match against certain debug values of cvars.
#include "net_ws_headers.h"
import ks.core.types;
import ks.core.vprof;
import ks.tier1.convar;
import ks.tier1.ns_address;

inline constexpr auto NET_QUEUED_PACKET_THREAD_DEBUG_VALUE = 581304;

class INetChannel;

class IQueuedPacketSender
{
public:
	bool Setup();
	void Shutdown();
	bool IsRunning();
	void ClearQueuedPacketsForChannel( INetChannel *pChan );
	void QueuePacket( INetChannel *pChan, SOCKET s, const char  *buf, int len, const ns_address &to, uint32 msecDelay );
	bool HasQueuedPackets( const INetChannel *pChan ) const;
};

extern IQueuedPacketSender *g_pQueuedPackedSender;
extern ConVar net_queued_packet_thread;
