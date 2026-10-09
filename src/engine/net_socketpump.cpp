// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: threaded UDP socket receive pump

import ks.core;
#include "net_ws_headers.h"
#include "sv_ipratelimit.h"

import std;

ConVar net_threaded_socket_recovery_time( "net_threaded_socket_recovery_time", "60", FCVAR_RELEASE, "Number of seconds over which the threaded socket pump algorithm will fully recover client ratelimit." );
ConVar net_threaded_socket_recovery_rate( "net_threaded_socket_recovery_rate", "6400", FCVAR_RELEASE, "Number of packets per second that threaded socket pump algorithm allows from client." );
ConVar net_threaded_socket_burst_cap( "net_threaded_socket_burst_cap", 
#ifdef DEDICATED
	"256",
#else
	"1024",
#endif
	FCVAR_RELEASE, "Max number of packets per burst beyond which threaded socket pump algorithm will start dropping packets." );

struct net_threaded_buffer_t
{
	int len;
	byte buf[ NET_MAX_MESSAGE ];

	inline byte *Base() { return buf + len; }
	inline int Capacity() { return sizeof( buf ) - len; }
	inline byte *MoveAppend( net_threaded_buffer_t *pOther )
	{
		if ( pOther->len > Capacity() )
			return nullptr;

		byte *pBase = Base();
		V_memcpy( pBase, pOther->buf, pOther->len );
		len += pOther->len;
		pOther->len = 0;
		return pBase;
	}
};
CTSPool<net_threaded_buffer_t> g_NetThreadedBuffers;

netadr_t g_NetAdrRatelimited;
int32 g_numRatelimitedPackets = -100;
class CThreadedSocketQueue
{
public:
	CThreadedSocketQueue() {}
	~CThreadedSocketQueue()
	{
		m_mapSocketThreads.clear();
	}

	static bool ShouldUseSocketsThreaded()
	{
		static bool s_bThreaded = !CommandLine()->FindParm( "-nothreadedsockets" );
		return s_bThreaded;
	}
	
	int recvfrom( ks::platform::socket *s, char * buf, int len, ks::platform::address * from )
	{
		CSocketThread *pThread = GetSocketThread( s );
		return pThread ? pThread->recvfrom( buf, len, from ) : 0;
	}

	void EnableThreadedRecv( ks::platform::socket *s, int nsSock, bool bEnable )
	{
		CSocketThread *pThread = GetSocketThread( s, nsSock, bEnable );
		( void ) ( pThread );
	}

	void CloseSocket( ks::platform::socket *s )
	{
		m_mapSocketThreads.erase( s );
	}

private:
	class CSocketThread
	{
	public:
		explicit CSocketThread( ks::platform::socket *s, int nsSock ) : m_s( s ), m_nsSock( nsSock ), m_hThread( nullptr ), m_pDataQueueBufferCollect( nullptr )
		{
			if ( !ks::platform::open_socket_pair( &m_sockSignalPipe[0], &m_sockSignalPipe[1], nullptr ) ) // 0=main; 1=pump
				Error( "socketpair failed!\n" );

			// Create the thread and start socket processing
			m_hThread = CreateSimpleThread( CallbackThreadProc, this );
			if ( !m_hThread )
				Error( "socket thread failed!\n" );
		}
		~CSocketThread()
		{
			// Kill the thread!
			ks::platform::send_bytes( m_sockSignalPipe[0], "", 1, nullptr ); // write one zero byte to wake the thread
			// wait for it to die
			ThreadJoin( m_hThread );
			ReleaseThreadHandle( m_hThread );
			m_hThread = nullptr;

			// Shutdown resources
			ks::platform::close_socket( m_sockSignalPipe[0] );
			ks::platform::close_socket( m_sockSignalPipe[1] );
			
			// Purge all packet data blocks
			m_tslstDataQueue.Purge();
			
			// Return all the buffers back into the pool
			do
			{
				if ( m_pDataQueueBufferCollect )
				{
					g_NetThreadedBuffers.PutObject( m_pDataQueueBufferCollect );
					m_pDataQueueBufferCollect = nullptr;
				}
			} while ( m_tslstBuffers.PopItem( &m_pDataQueueBufferCollect ) );
		}

		int recvfrom( char * buf, int len, ks::platform::address * from )
		{
			//
			// Called on application main thread
			//
			ReceivedData_t data;
			if ( !m_tslstDataQueue.PopItem( &data ) )
				return 0;

			V_memcpy( buf, data.buf, std::min( len, data.len ) );
			*from = data.from;

			//
			// We are about to return this data, free up allocated buffers up to this
			//
			if ( !m_pDataQueueBufferCollect )
			{
				m_tslstBuffers.PopItem( &m_pDataQueueBufferCollect );
			}

			if ( ( data.buf >= m_pDataQueueBufferCollect->buf ) && ( data.buf < m_pDataQueueBufferCollect->buf + sizeof( m_pDataQueueBufferCollect->buf ) ) )
				;// The returned data was still referencing the head of scratch buffers
			else
			{	// The returned data is ahead in the list, previous buffer can go back in the pool
				g_NetThreadedBuffers.PutObject( m_pDataQueueBufferCollect );
				m_pDataQueueBufferCollect = nullptr;
			}

			return data.len;
		}

	private:
		struct CPerNetChanRatelimit_t
		{
			double m_dblNetTimeMark;
			int32 m_numPackets;
			int32 m_numRatelimited;
		};
		static uintp CallbackThreadProc( void *pvParam ) { reinterpret_cast<CSocketThread*>(pvParam)->ThreadProc(); return 0; }
		void ThreadProc()
		{
			// Where are we getting new data?
			net_threaded_buffer_t *pThreadBufferSyscall = nullptr;
			net_threaded_buffer_t *pThreadBufferCollect = nullptr;
			
			ks::platform::address	from{};
			netadr_t adrt;
			adrt.SetType( NA_IP );
			adrt.Clear();

			extern std::atomic<int> g_NetChannelsRefreshCounter;
			int pumpNetChannelsRefreshCounter = -1;
			std::vector< ks::platform::address > arrNetChans;
			typedef std::map< uint64, CPerNetChanRatelimit_t > MapPerClientRatelimit_t;
			MapPerClientRatelimit_t mapPerClientRatelimit;

			const int numSyscallPollFDs = 2;
			ks::platform::poll_entry syscallPollFDs[ numSyscallPollFDs ];
			syscallPollFDs[ 0 ].s = m_s;
			syscallPollFDs[ 0 ].readable = false;
			syscallPollFDs[ 1 ].s = m_sockSignalPipe[1];
			syscallPollFDs[ 1 ].readable = false;

			volatile double &vdblNetTimeForThread = net_time;
			for ( ;; )
			{
				if ( !pThreadBufferSyscall )
				{
					// Need a new buffer
					net_threaded_buffer_t *pbuf = g_NetThreadedBuffers.GetObject();
					pbuf->len = 0;
					m_tslstBuffers.PushItem( pbuf );
					pThreadBufferSyscall = pbuf;
				}

				// Recv socket data
				int ret = (int)ks::platform::receive_from( m_s, &from, pThreadBufferSyscall->buf, sizeof( pThreadBufferSyscall->buf ), nullptr );
				if ( ret <= 0 )
				{
					// Efficiently sleep while we wait for next packet
					ret = ks::platform::poll_sockets( syscallPollFDs, numSyscallPollFDs, -1 ); // Sleep indefinitely until data available
					if ( syscallPollFDs[ 1 ].readable )
						return;	// Finish the socket reading thread if socketpair pump end of the pipe has incoming signal
					continue;
				}
				if ( ret > 0 )
				{
					// Is this a connectionless packet?
					if ( 0xFFFFFFFF == * ( uint32 * ) pThreadBufferSyscall->buf )
					{
						adrt.SetFromAddress( from );
						if ( !CheckConnectionLessRateLimits( adrt ) )
							ret = 0;
					}
					else
					{
						// Check if we need to refresh our netchans for this socket type
						if ( pumpNetChannelsRefreshCounter != g_NetChannelsRefreshCounter )
						{
							pumpNetChannelsRefreshCounter = g_NetChannelsRefreshCounter;
							arrNetChans.clear();
							extern void NET_FindAllNetChannelAddresses( int socket, std::vector< ks::platform::address > &arrNetChans );
							NET_FindAllNetChannelAddresses( m_nsSock, arrNetChans );

							// Since we just recomputed net channels, use this opportunity to expire obsolete ratelimits for old clients
							std::erase_if( mapPerClientRatelimit, [&]( const auto &entry )
							{
								double dblTimeSinceTimeMark = ( vdblNetTimeForThread - entry.second.m_dblNetTimeMark );
								return dblTimeSinceTimeMark > net_threaded_socket_recovery_time.GetFloat();
							} );
						}

						// This is a connection-oriented packet, must have a netchan
						bool bNetChanAvailable = false;
						for ( int k = 0; k < std::ssize( arrNetChans ); ++ k )
						{
							const ks::platform::address &sockAddressNetChan = arrNetChans[k];
							if ( sockAddressNetChan.ip == from.ip && sockAddressNetChan.port == from.port )
							{
								bNetChanAvailable = true;

								//
								// Track ratelimit on this netchan
								//
								uint64 uiRatelimitKey = ( uint64( from.ip ) << 32 ) | uint32( from.port );
								auto idxPerClientRateLimit = mapPerClientRatelimit.find( uiRatelimitKey );
								if ( idxPerClientRateLimit == mapPerClientRatelimit.end() )
								{
									CPerNetChanRatelimit_t pncrt;
									V_memset( &pncrt, 0, sizeof( pncrt ) );
									pncrt.m_dblNetTimeMark = vdblNetTimeForThread;
									pncrt.m_numPackets = 1;
									mapPerClientRatelimit.emplace( uiRatelimitKey, pncrt );
								}
								else
								{
									CPerNetChanRatelimit_t &pncrt = idxPerClientRateLimit->second;
									double dblTimeSinceTimeMark = ( vdblNetTimeForThread - pncrt.m_dblNetTimeMark );
									if ( ( dblTimeSinceTimeMark > net_threaded_socket_recovery_time.GetFloat() ) ||
										( dblTimeSinceTimeMark < 0 ) )
									{
										pncrt.m_numPackets = 0;
									}
									else if ( dblTimeSinceTimeMark > 0 )
									{
										int32 numPacketsToRecover = dblTimeSinceTimeMark * net_threaded_socket_recovery_rate.GetFloat();
										pncrt.m_numPackets = std::max( pncrt.m_numPackets - numPacketsToRecover, 0 );
									}
									
									if ( pncrt.m_numPackets > net_threaded_socket_burst_cap.GetInt() )
									{
										bNetChanAvailable = false;
										++ pncrt.m_numRatelimited;
										if ( pncrt.m_numRatelimited > g_numRatelimitedPackets + 5 )
										{
											g_NetAdrRatelimited.SetFromAddress( from ); // remember last ratelimited address for logging purposes
											g_numRatelimitedPackets = pncrt.m_numRatelimited;
										}
									}
									else
									{
										pncrt.m_dblNetTimeMark = vdblNetTimeForThread;
										++ pncrt.m_numPackets;
										pncrt.m_numRatelimited = 0;
									}
								}

								break;
							}
						}
						if ( !bNetChanAvailable )
							ret = 0;
					}
				}
				if ( ret > 0 )
				{
					ReceivedData_t recvData;
					recvData.buf = nullptr;
					recvData.len = ret;
					recvData.from = from;

					// Check if we still have more room in pending recv buffer
					pThreadBufferSyscall->len = ret;
					if ( byte *pbMoveAppend = pThreadBufferCollect ? pThreadBufferCollect->MoveAppend( pThreadBufferSyscall ) : nullptr )
					{
						recvData.buf = pbMoveAppend;
					}
					else
					{
						pThreadBufferCollect = pThreadBufferSyscall;
						pThreadBufferSyscall = nullptr;

						recvData.buf = pThreadBufferCollect->buf;
					}

					m_tslstDataQueue.PushItem( recvData );
				}
			}
		}

		struct ReceivedData_t
		{
			byte *buf;
			int len;
			ks::platform::address	from;
		};

		
	//
	// Thread object data members
	//
	private:
		ks::platform::socket * const m_s;									// [const] accessed by pump thread in ::recvfrom
		int const m_nsSock;								// [const] accessed by pump thread doing netchans lookup
		ThreadHandle_t m_hThread;						// Thread handle, accessed by main thread
		CTSQueue< ReceivedData_t > m_tslstDataQueue;	// FIFO - actual data packets pumped from socket, thread-safe access on both threads
		CTSQueue< net_threaded_buffer_t * > m_tslstBuffers;	// FIFO - buffers storing data pumped from socket, multiple packets can be stored in one memory chunk, thread-safe access on both threads
		net_threaded_buffer_t *m_pDataQueueBufferCollect; // Main thread tracking when collect buffer can be returned to global pool
		ks::platform::socket *m_sockSignalPipe[2] = { nullptr, nullptr };
	};
	CSocketThread * GetSocketThread( ks::platform::socket *s, int nsSock = -1, bool bRequired = false )
	{
		auto idx = m_mapSocketThreads.find( s );
		if ( idx != m_mapSocketThreads.end() )
			return idx->second.get();
		if ( bRequired )
		{
			return m_mapSocketThreads.emplace( s, std::make_unique< CSocketThread >( s, nsSock ) ).first->second.get();
		}
		return nullptr;
	}
	std::map< ks::platform::socket *, std::unique_ptr< CSocketThread > > m_mapSocketThreads;
};
CThreadedSocketQueue g_ThreadedSocketQueue;

void On_NET_ProcessSocket_Start( ks::platform::socket *hUDP, int sock )
{
	if ( g_numRatelimitedPackets > 0 )
	{	// Spew about ratelimit on the main thread
		ConMsg( "Net channel ratelimit exceeded for %s: %d packets rejected.\n", g_NetAdrRatelimited.ToString(), g_numRatelimitedPackets );
		g_NetAdrRatelimited.Clear();
		g_numRatelimitedPackets = -100;
	}
}
void On_NET_ProcessSocket_End( ks::platform::socket *hUDP, int sock )
{
	if ( CThreadedSocketQueue::ShouldUseSocketsThreaded() )
		g_ThreadedSocketQueue.EnableThreadedRecv( hUDP, sock, true );
}

void NET_SocketPumpClose( ks::platform::socket *s )
{
	if ( CThreadedSocketQueue::ShouldUseSocketsThreaded() )
		g_ThreadedSocketQueue.CloseSocket( s );
}

int NET_SocketPumpRecvFrom( ks::platform::socket *s, char *buf, int len, ns_address *from )
{
	ks::platform::address sadrfrom{};
	int iret = ( CThreadedSocketQueue::ShouldUseSocketsThreaded() )
		? g_ThreadedSocketQueue.recvfrom( s, buf, len, &sadrfrom )
		: (int)ks::platform::receive_from( s, &sadrfrom, buf, len, nullptr );

	if ( iret > 0 )
	{
		from->SetFromAddress( sadrfrom );
		return iret;
	}

	return 0;
}
