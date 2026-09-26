//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "net_ws_headers.h"
#include <atomic>
#include <mutex>
#include "net_ws_queued_packet_sender.h"

#include "tier0/vprof.h"
#include "tier1/utlvector.h"
#include "tier1/vector_ops.h"
#include <memory>

ConVar net_queued_packet_thread( "net_queued_packet_thread", "1", 0, "Use a high priority thread to send queued packets out instead of sending them each frame." );
ConVar net_queue_trace( "net_queue_trace", "0", FCVAR_ACCESSIBLE_FROM_THREADS );

class CQueuedPacketSender : public CThread, public IQueuedPacketSender
{
public:
	CQueuedPacketSender();
	~CQueuedPacketSender();

	// IQueuedPacketSender

	virtual bool Setup();
	virtual void Shutdown();
	virtual bool IsRunning() { return CThread::IsAlive(); }

	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan );
	virtual void QueuePacket( INetChannel *pChan, SOCKET s, const char FAR *buf, int len, const ns_address &to, uint32 msecDelay );
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const;
private:

	// CThread Overrides
	virtual bool Start( unsigned int nBytesStack = 0 );
	virtual int Run();

private:

	class CQueuedPacket
	{
	public:
		uint32				m_unSendTime;
		const void 			*m_pChannel;  // We don't actually use the channel
		SOCKET				m_Socket;
		ns_address			to;
		std::vector< char >	buf;

		// We want the list sorted in ascending order, so note that we return > rather than <
		static bool LessFunc( const std::unique_ptr< CQueuedPacket > &lhs, const std::unique_ptr< CQueuedPacket > &rhs )
		{
			return lhs->m_unSendTime > rhs->m_unSendTime;
		}
	};

	std::vector< std::unique_ptr< CQueuedPacket > > m_QueuedPackets;	// heap by LessFunc (ks::heap_*)
	mutable std::recursive_mutex m_QueuedPacketsCS;
	CThreadEvent m_hThreadEvent;
	std::atomic<bool> m_bThreadShouldExit;
};

static CQueuedPacketSender g_QueuedPacketSender;
IQueuedPacketSender *g_pQueuedPackedSender = &g_QueuedPacketSender;


CQueuedPacketSender::CQueuedPacketSender()
{
	SetName( "QueuedPacketSender" );
	m_bThreadShouldExit = false;
}

CQueuedPacketSender::~CQueuedPacketSender()
{
	Shutdown();
}

bool CQueuedPacketSender::Setup()
{
	return Start();
}

bool CQueuedPacketSender::Start( unsigned nBytesStack )
{
	Shutdown();

	if ( CThread::Start( nBytesStack ) )
	{
		// Ahhh the perfect cross-platformness of the threads library.
		//SetPriority( PRIORITY_MAX );

		m_bThreadShouldExit = false;

		return true;
	}
	else
	{
		return false;
	}
}

void CQueuedPacketSender::Shutdown()
{
	if ( !IsAlive() )
		return;
		

	m_bThreadShouldExit = true;
	m_hThreadEvent.Set();
	
	Join(); // Wait for the thread to exit.

	m_QueuedPackets.clear();
	m_QueuedPackets.shrink_to_fit();
}

void CQueuedPacketSender::ClearQueuedPacketsForChannel( INetChannel *pChan )
{
	std::scoped_lock lock( m_QueuedPacketsCS );

	for ( int i = std::ssize( m_QueuedPackets )-1; i >= 0; i-- )
	{
		if ( m_QueuedPackets[ i ]->m_pChannel == pChan )
		{
			ks::heap_erase( m_QueuedPackets, i, CQueuedPacket::LessFunc );
		}
	}
}

bool CQueuedPacketSender::HasQueuedPackets( const INetChannel *pChan ) const
{
	std::scoped_lock lock( m_QueuedPacketsCS );

	for ( const auto &p : m_QueuedPackets )
	{
		if ( p->m_pChannel == pChan )
		{
			return true;
		}
	}

	return false;
}

void CQueuedPacketSender::QueuePacket( INetChannel *pChan, SOCKET s, const char FAR *buf, int len, const ns_address &to, uint32 msecDelay )
{
	std::scoped_lock lock( m_QueuedPacketsCS );

	// We'll pull all packets we should have sent by now and send them out right away
	uint32 msNow = Plat_MSTime();

	int nMaxQueuedPackets = 1024;
	if ( std::ssize( m_QueuedPackets ) < nMaxQueuedPackets )
	{
		// Add this packet to the queue.
		auto pPacket = std::make_unique< CQueuedPacket >();
		pPacket->m_unSendTime = msNow + msecDelay;
		pPacket->m_Socket = s;
		pPacket->m_pChannel = pChan;
		pPacket->buf.assign( ( (char*)buf ), ( (char*)buf ) + len );
		pPacket->to = to;
		ks::heap_push( m_QueuedPackets, std::move( pPacket ), CQueuedPacket::LessFunc );
	}
	else
	{
		static int nWarnings = 5;
		if ( --nWarnings > 0 )
		{
			Warning( "CQueuedPacketSender: num queued packets >= nMaxQueuedPackets. Not queueing anymore.\n" );
		}
	}

	// Tell the thread that we have a queued packet.
	m_hThreadEvent.Set();
}

extern int NET_SendToImpl( SOCKET s, const char FAR * buf, int len, const ns_address &to, int iGameDataLength );

int CQueuedPacketSender::Run()
{
	 // Normally TT_INFINITE but we wakeup every 500ms just in case.
	uint32 waitIntervalNoPackets = 500;
	uint32 waitInterval = waitIntervalNoPackets;
	while ( 1 )
	{
		m_hThreadEvent.Wait( waitInterval );
		{
			// Someone signaled the thread. Either we're being told to exit or 
			// we're being told that a packet was just queued.
			if ( m_bThreadShouldExit )
				return 0;
		}

		// Assume nothing to do and that we'll sleep again
		waitInterval = waitIntervalNoPackets;

		// OK, now send a packet.
		{
            SNPROF("NET_SendToImpl");
			std::scoped_lock lock( m_QueuedPacketsCS );
		
			// We'll pull all packets we should have sent by now and send them out right away
			uint32 msNow = Plat_MSTime();

			bool bTrace = net_queue_trace.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE;

			while ( !m_QueuedPackets.empty() )
			{
				CQueuedPacket *pPacket = m_QueuedPackets.front().get();
				if ( pPacket->m_unSendTime > msNow )
				{
					// Sleep until next we need this packet
					waitInterval = pPacket->m_unSendTime - msNow;
					if ( bTrace )
					{
						Warning( "SQ:  sleeping for %u msecs at %f\n", waitInterval, Plat_FloatTime() );
					}
					break;
				}

				// If it's a bot, don't do anything. Note: we DO want this code deep here because bots only
				// try to send packets when sv_stressbots is set, in which case we want it to act as closely
				// as a real player as possible.
				if ( !pPacket->to.IsNull() )
				{		
					if ( bTrace )
					{
						Warning( "SQ:  sending %d bytes at %f\n", (int)std::ssize( pPacket->buf ), Plat_FloatTime() );
					}

					NET_SendToImpl
					( 
						pPacket->m_Socket, 
						pPacket->buf.data(), 
						std::ssize( pPacket->buf ), 
						pPacket->to,
						-1 
					);
				}	
				
				ks::heap_pop( m_QueuedPackets, CQueuedPacket::LessFunc );
			}
		}
	}
}

