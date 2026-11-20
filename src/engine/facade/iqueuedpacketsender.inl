import ks.core.types;
import ks.core.vprof;
import ks.tier1.ns_address;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IQueuedPacketSender class to CQueuedPacketSender. Included by net_ws_queued_packet_sender.cpp.

bool IQueuedPacketSender::Setup()
{ return static_cast<CQueuedPacketSender*>(this)->Setup(); }
void IQueuedPacketSender::Shutdown()
{ static_cast<CQueuedPacketSender*>(this)->Shutdown(); }
bool IQueuedPacketSender::IsRunning()
{ return static_cast<CQueuedPacketSender*>(this)->IsRunning(); }
void IQueuedPacketSender::ClearQueuedPacketsForChannel( INetChannel *pChan )
{ static_cast<CQueuedPacketSender*>(this)->ClearQueuedPacketsForChannel( pChan ); }
void IQueuedPacketSender::QueuePacket( INetChannel *pChan, SOCKET s, const char *buf, int len, const ns_address &to, uint32 msecDelay )
{ static_cast<CQueuedPacketSender*>(this)->QueuePacket( pChan, s, buf, len, to, msecDelay ); }
bool IQueuedPacketSender::HasQueuedPackets( const INetChannel *pChan ) const
{ return static_cast<const CQueuedPacketSender*>(this)->HasQueuedPackets( pChan ); }

