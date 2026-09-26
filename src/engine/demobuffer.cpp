//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// TODO:
//  - USE A MEMPOOL OF SOME SORT
//  - GET STALE FRAME REMOVAL WORKING
//  - MAKE THREAD SAFE - NEED TO BE ABLE TO WRITE A .DEM FILE ON A SEP THREAD AND
//		NOT BE THROWING AWAY ELEMENTS THAT ARE BEING READ.  NEED A FLAG IN DATACHUNK
//		FOR WHETHER AN INSTANCE IS "IN USE," IE BEING READ TO WRITE TO A DEM FILE.
//
//===============================================================================

#include "demobuffer.h"
#include "filesystem.h"
#include "tier1/byte_writer.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>
#include "edict.h"
#include "host.h"
#include "vstdlib/jobthread.h"
#include <deque>
#include <memory>

#include "replayserver.h"	// TODO: Remove
#include "sv_client.h"
#ifndef DEDICATED
#include "cdll_int.h"
#include "client.h"
#endif

#ifndef DEDICATED
#if !defined( _DEBUG )
// These are the buffers defining how demo data is flushed to disk:
// We allocate 5 MB buffer (worth about 5 min of gameplay)
// once over 4 MB is used up we will commit the first 1 MB to disk
// This throttles disk IO, and ensures that the last 3 MB are always
// contained in memory and not committed to disk to prevent file peeking
// for cheating purposes during gameplay
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*1024*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*1024*1024)
#else
// Smaller sizes in debug engine.dll build to hammer on the subsystems involved
#define DISK_DEMO_BUFFER_TOTAL_SIZE (5*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE (4*20*1024)
#define DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE (1*20*1024)
#endif
#endif

//-----------------------------------------------------------------------------
// Disk-backed demo buffer. While recording, output is collected in memory and
// committed to the file in chunks; while playing back, the file is read through
// a sliding window.
//-----------------------------------------------------------------------------
class CDiskDemoBuffer : public IDemoBuffer
{
public:
	~CDiskDemoBuffer()
	{
		Close();
	}

	virtual bool Init( DemoBufferInitParams_t const& params )
	{
		// Convert to proper type
		StreamDemoBufferInitParams_t const* pParams = dynamic_cast< StreamDemoBufferInitParams_t const* >( &params );		Assert( pParams );

		Close();

		m_bReadOnly = pParams->bReadOnly;
		m_nGet = 0;
		m_nPut = 0;
		m_nOffset = 0;
		m_nMaxPut = 0;
		m_bGetFailed = false;
		m_bPutFailed = false;
		m_nDecodedOffset = -1;

		// Demo files are always little endian, which is the platform-native byte order.
		m_hFile = g_pFullFileSystem->OpenEx( pParams->pFilename, m_bReadOnly ? "rb" : "wb", 0, pParams->pszPath );
		if ( m_hFile != FILESYSTEM_INVALID_HANDLE && m_bReadOnly )
		{
			m_nMaxPut = g_pFullFileSystem->Size( m_hFile );
		}

#ifndef DEDICATED
		// Force a very large memory buffer on the clients, this prevents peeking into the demo stream
		m_nPendingCapacity = DISK_DEMO_BUFFER_TOTAL_SIZE;
#endif

		m_pPlaybackParams = nullptr;
#ifndef DEDICATED
		extern IDemoPlayer *demoplayer;
		extern IBaseClientDLL *g_ClientDLL;
		if ( demoplayer && g_ClientDLL )
		{
			m_pPlaybackParams = demoplayer->GetDemoPlaybackParameters();
		}
#endif

		return IsInitialized();
	}

	virtual void NotifySignonComplete() {}

	virtual void WriteHeader( void const *pData, int nSize )
	{
		// Goto file start
		SeekPut( true, 0 );

		// Write
		Put( pData, nSize );
	}

	virtual void				NotifyBeginFrame() {}
	virtual void				NotifyEndFrame() {}

	virtual void				PutChar( char c )						{ Put( &c, sizeof( c ) ); }
	virtual void				PutUnsignedChar( unsigned char uc )		{ Put( &uc, sizeof( uc ) ); }
	virtual void				PutInt( int i )							{ Put( &i, sizeof( i ) ); }

	virtual void				WriteTick( int nTick )					{ Put( &nTick, sizeof( nTick ) ); }

	virtual char				GetChar() OVERRIDE
	{
		char c = 0;
		Get( &c, sizeof( c ) );
		return c;
	}
	virtual unsigned char		GetUnsignedChar() OVERRIDE
	{
		unsigned char uc = 0;
		Get( &uc, sizeof( uc ) );
		return uc;
	}
	virtual int					GetInt() OVERRIDE
	{
		int i = 0;
		Get( &i, sizeof( i ) );
		return i;
	}

	virtual void				Get( void* pMem, int size )	OVERRIDE
	{
#ifndef DEDICATED
		if ( m_pPlaybackParams )
		{
			GetDecoded( pMem, size );
			return;
		}
#endif
		GetRaw( pMem, size );
	}

	virtual void				Put( const void* pMem, int size )
	{
		if ( size <= 0 || !IsWritable() )
			return;

		if ( int( m_pending.tell() ) + size > m_nPendingCapacity && !FlushPending( int( m_pending.tell() ) ) )
			return;

		m_pending.write_bytes( pMem, size );
		m_nPut += size;
		m_nMaxPut = std::max( m_nMaxPut, m_nPut );

#ifndef DEDICATED
		if ( int( m_pending.tell() ) > (
				( GetBaseLocalClient().IsActive() && GetBaseLocalClient().ishltv ) ? 2048 : DISK_DEMO_BUFFER_FLUSH_CONSIDER_SIZE
				) )
		{	// Periodically try to flush the demo buffer to disk
			FlushPending( DISK_DEMO_BUFFER_FLUSH_TODISK_SIZE );
		}
#endif
	}

	virtual bool				IsValid() const							{ return !m_bGetFailed && !m_bPutFailed; }
	virtual bool				IsInitialized() const					{ return IsValid() && m_hFile != FILESYSTEM_INVALID_HANDLE; }

	// Change where I'm writing (put)/reading (get)
	virtual void				SeekPut( bool bAbsolute, int offset )
	{
		if ( m_bReadOnly )
			return;

		const int nNextPut = bAbsolute ? offset : m_nPut + offset;
		if ( m_bPutFailed || !FlushPending( int( m_pending.tell() ) ) )
			return;

		m_nPut = nNextPut;
		m_nOffset = nNextPut;
		if ( m_hFile != FILESYSTEM_INVALID_HANDLE )
			g_pFullFileSystem->Seek( m_hFile, m_nOffset, FILESYSTEM_SEEK_HEAD );
		m_nMaxPut = std::max( m_nMaxPut, m_nPut );
	}

	virtual void				SeekGet( bool bAbsolute, int offset )
	{
		m_nGet = bAbsolute ? offset : m_nGet + offset;
		m_bGetFailed = m_nGet < 0 || m_nGet > m_nMaxPut;
	}

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const						{ return m_nPut; }
	virtual int					TellGet( ) const						{ return m_nGet; }

	virtual int					TellMaxPut( ) const						{ return m_nMaxPut; }

	virtual void				UpdateStartTick( int& nStartTick ) const {}
	virtual void				DumpToFile( char const* pFilename, const demoheader_t &header ) const {}

private:
	static constexpr int		READ_CHUNK_SIZE = 1024 * 1024;

	bool IsWritable() const	{ return !m_bReadOnly && !m_bPutFailed && m_hFile != FILESYSTEM_INVALID_HANDLE; }

	void Close()
	{
		if ( m_hFile == FILESYSTEM_INVALID_HANDLE )
			return;

		if ( !m_bReadOnly )
			FlushPending( int( m_pending.tell() ) );

		g_pFullFileSystem->Close( m_hFile );
		m_hFile = FILESYSTEM_INVALID_HANDLE;
		m_pending.clear();
		m_window.clear();
		m_decoded.clear();
	}

	bool FlushPending( int nBytes )
	{
		nBytes = std::min< int >( nBytes, m_pending.tell() );
		if ( nBytes <= 0 || m_hFile == FILESYSTEM_INVALID_HANDLE )
			return true;

		if ( g_pFullFileSystem->Write( m_pending.data(), nBytes, m_hFile ) != nBytes )
		{
			m_bPutFailed = true;
			return false;
		}

		m_nOffset += nBytes;
		if ( nBytes == int( m_pending.tell() ) )
		{
			m_pending.clear();
		}
		else
		{
			auto remaining = m_pending.release();
			remaining.erase( remaining.begin(), remaining.begin() + nBytes );
			m_pending = ks::byte_writer::adopt( std::move( remaining ) );
		}
		return true;
	}

	bool GetRaw( void *pMem, int size )
	{
		if ( size <= 0 || m_bGetFailed || m_hFile == FILESYSTEM_INVALID_HANDLE || !m_bReadOnly )
			return false;

		if ( m_nGet + size > m_nMaxPut )
		{
			m_bGetFailed = true;
			return false;
		}

		if ( m_nGet < m_nWindowStart || m_nGet + size > m_nWindowStart + int( m_window.size() ) )
		{
			const int nToRead = std::min( std::max( size, READ_CHUNK_SIZE ), m_nMaxPut - m_nGet );
			g_pFullFileSystem->Seek( m_hFile, m_nGet, FILESYSTEM_SEEK_HEAD );
			m_window.resize( nToRead );
			const int nRead = g_pFullFileSystem->Read( m_window.data(), nToRead, m_hFile );
			m_window.resize( std::max( nRead, 0 ) );
			m_nWindowStart = m_nGet;
			if ( nRead != nToRead )
			{
				// The size was only a guess based on the file length
				m_nMaxPut = std::min( m_nMaxPut, m_nGet + std::max( nRead, 0 ) );
			}
			if ( m_nGet + size > m_nWindowStart + int( m_window.size() ) )
			{
				m_bGetFailed = true;
				return false;
			}
		}

		std::memcpy( pMem, m_window.data() + ( m_nGet - m_nWindowStart ), size );
		m_nGet += size;
		return true;
	}

#ifndef DEDICATED
	void GetDecoded( void *pMem, int size )
	{
		// Read from the nearest 16-byte aligned location
		const int nOriginalGet = m_nGet;
		if ( ( m_nDecodedOffset < 0 ) || // nothing decoded
			( nOriginalGet < m_nDecodedOffset ) || // reading earlier
			( nOriginalGet + size > m_nDecodedOffset + int( m_decoded.size() ) ) ) // could read beyond decoded buffer
		{
			int nNearestAlignedLocation = nOriginalGet &~0xF;
			int blockRead = ( nOriginalGet + size - nNearestAlignedLocation + 0xF )&~0xF;
			blockRead = std::max( 1024, blockRead ); // decrypt chunks of 1K bytes at a time

			m_nDecodedOffset = nNearestAlignedLocation;
			const int numBytes = std::max( 0, std::min( blockRead, m_nMaxPut - nNearestAlignedLocation ) );
			m_decoded.resize( numBytes );

			m_bGetFailed = false;
			m_nGet = nNearestAlignedLocation;
			if ( numBytes > 0 && !GetRaw( m_decoded.data(), numBytes ) )
				m_decoded.clear();

			// Decode the chunk
			extern IBaseClientDLL *g_ClientDLL;
			g_ClientDLL->PrepareSignedEvidenceData( m_decoded.data(), int( m_decoded.size() ), m_pPlaybackParams );
		}

		const int nSeekInDecodedBuffer = nOriginalGet - m_nDecodedOffset;
		if ( size > 0 && nSeekInDecodedBuffer >= 0 && nSeekInDecodedBuffer + size <= int( m_decoded.size() ) )
		{
			std::memcpy( pMem, m_decoded.data() + nSeekInDecodedBuffer, size );
			m_nGet = nOriginalGet + size;
		}
		else
		{
			m_nGet = nOriginalGet;
		}
		m_bGetFailed = m_nGet < 0 || m_nGet > m_nMaxPut;
	}
#endif

	FileHandle_t m_hFile = FILESYSTEM_INVALID_HANDLE;
	bool m_bReadOnly = false;
	bool m_bGetFailed = false;
	bool m_bPutFailed = false;

	// playback: file window
	int m_nGet = 0;
	int m_nMaxPut = 0;
	int m_nWindowStart = 0;
	std::vector< std::byte > m_window;

	// recording: bytes not yet committed; m_nOffset is the file position of the first of them
	ks::byte_writer m_pending;
	int m_nPut = 0;
	int m_nOffset = 0;
	int m_nPendingCapacity = 16 * 1024;

	std::vector< std::byte > m_decoded;
	int m_nDecodedOffset = -1;
	CDemoPlaybackParameters_t const *m_pPlaybackParams = nullptr;
};


//-----------------------------------------------------------------------------
// Specialty class with overrides for stream buffer
//-----------------------------------------------------------------------------
#if defined( REPLAY_ENABLED )
class CMemoryDemoBuffer : public IDemoBuffer
{
private:
	static int const CACHE_SIZE	= 1024 * 512;

	uint8*	m_pDataCache;			// Data cache for temporary writing
	uint8*	m_pWrite;				// Current write position (based on m_pDataCache)
	int		m_nBufferSize;			// Total buffer size
	int		m_nMaxPut;				// What's the most I've ever written?
	int		m_nCurrentTickOffset;	// Offset (relative to m_pDataCache) of tick - needed so we can rewrite ticks before dumping to disk

	struct DataChunk_t
	{
		int		nCurrentTickOffset;
		int		nTickcount;
		int		nDeltaTickcount;
		int		nSize;
		uint8	pData[1];
	};

	inline DataChunk_t* AllocDataChunk( int nSize, int nCurrentTickOffset )
	{
		Assert( nCurrentTickOffset >= 0 );

		int nActualSize = sizeof(DataChunk_t) + nSize - 1;		Assert( nActualSize < CACHE_SIZE );

		DataChunk_t* pNewFrame = (DataChunk_t*)new uint8[ nActualSize ]; 
		pNewFrame->nSize = nSize;
		pNewFrame->nCurrentTickOffset = nCurrentTickOffset;
		pNewFrame->nTickcount = -1;

		// TODO: pass in the delta tick - get rid of #include "replay" etc.
		pNewFrame->nDeltaTickcount = ( replay && replay->m_MasterClient ) ? replay->m_MasterClient->m_nDeltaTick : -1;

		return pNewFrame;
	}

	// Chunks are allocated as uint8 arrays.
	struct DataChunkDeleter
	{
		void operator()( DataChunk_t *p ) const { delete [] reinterpret_cast< uint8 * >( p ); }
	};

	bool m_bSignonComplete;

	DataChunk_t* m_pSignonData;

	std::deque< std::unique_ptr< DataChunk_t, DataChunkDeleter > > m_lstFrames;	// Represents a list of demo frames

	inline int GetTickCount()
	{
		extern CGlobalVars g_ServerGlobalVariables;
		return g_ServerGlobalVariables.tickcount;
	}

	void RemoveStaleFrames()
	{
		// Don't remove any frames in the midst of a write operation
		if ( m_nWriteCount > 0 )
			return;

		extern ConVar replay_movielength;

#ifdef _DEBUG
		int nNumFramesRemoved = 0;
#endif

		// Here we remove any frames that are beyond the length of the movie.
		while ( !m_lstFrames.empty() )
		{
			if ( m_lstFrames.front()->nTickcount >= GetTickCount() - TIME_TO_TICKS( replay_movielength.GetInt() ) )
				break;

			m_lstFrames.pop_front();
#ifdef _DEBUG
			++nNumFramesRemoved;
#endif
		}

#ifdef _DEBUG
		if ( nNumFramesRemoved > 0 )
		{
			DevMsg( "Replay: Removed %d frames(s) from recording buffer.\n", nNumFramesRemoved );
		}
#endif
	}

public:
	CMemoryDemoBuffer()
	:	
		m_nBufferSize( 0 ),
		m_nMaxPut( 0 ),
		m_nCurrentTickOffset( -1 ),
		m_pWrite( nullptr ),
		m_pSignonData( nullptr ),
		m_bSignonComplete( false )
	{
	}

	~CMemoryDemoBuffer()
	{
		delete [] m_pDataCache;
		DataChunkDeleter()( m_pSignonData );
	}

	virtual bool Init( DemoBufferInitParams_t const& params )
	{
		m_pDataCache = new uint8[ CACHE_SIZE ];
		m_pWrite = m_pDataCache;
		return true;
	}

	virtual bool IsInitialized() const	{ return m_pDataCache != nullptr; }

	virtual bool IsValid() const		{ return IsInitialized(); }

	virtual void WriteHeader( const void *pData, int nSize )
	{
		// There is no need to write the header until we dump the file to disk.

		/*
		// NOTE: Byteswap happens in dump

		// The header gets written twice, once at demo start, and once at demo stop.
		// If this is the first time, just write to the beginning of the cache
		if ( !m_bSignonComplete )
		{
			Assert( m_pWrite == m_pDataCache );		// Make sure this is the first thing we're writing
			Put( pData, nSize );
		}
		else	// Otherwise, write to the beginning of the header
		{
			AssertValidWritePtr( m_pHeaderData, nSize );
			V_memcpy( m_pHeaderData, pData, nSize );
		}
		*/
	}

	virtual void NotifySignonComplete()
	{
		Assert( !m_pSignonData );

		// Compute size and allocate
		int nSize = m_pWrite - m_pDataCache;				Assert( nSize >= 0 );
		m_pSignonData = AllocDataChunk( nSize, -1 );

		// NOTE: No need to set m_pSignonData->nTickcount.

		// We're done with signon data, copy it over from the cache
		V_memcpy( m_pSignonData->pData, m_pDataCache, nSize );

		m_pWrite = nullptr;
		m_bSignonComplete = true;
	}

	virtual void NotifyBeginFrame()
	{
		if ( !m_bSignonComplete )
			return;

		Assert( m_pWrite == 0 );
		m_pWrite = m_pDataCache;
	}

	virtual void NotifyEndFrame()
	{
		if ( !m_bSignonComplete )
			return;

 		RemoveStaleFrames();

		// Allocate a new data chunk
		int nSize = m_pWrite - m_pDataCache;				Assert( nSize >= 0 );
		DataChunk_t* pNewFrame = AllocDataChunk( nSize, m_nCurrentTickOffset );

		// Set the time
		pNewFrame->nTickcount = GetTickCount();

		// Copy data from cache to new frame
		V_memcpy( pNewFrame->pData, m_pDataCache, nSize );

		// Add new frame to list
		m_lstFrames.emplace_back( pNewFrame );

#ifdef _DEBUG
		m_pWrite = nullptr;
#endif
	}

	// Change where I'm writing (put)/reading (get)
	virtual void SeekGet( bool bAbsolute, int offset )
	{
		// Don't call this.
		Assert( 0 );
	}

	virtual void SeekPut( bool bAbsolute, int nOffset )
	{
		// The only time this should get called is if we are about to write the header.
		Assert( bAbsolute && nOffset == 0 );
	}

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const			{ return m_pWrite - m_pDataCache; }
	virtual int					TellGet( ) const			{ Assert( 0 ); return 0; }

	// What's the most I've ever written?
	virtual int					TellMaxPut( ) const	{ return m_nMaxPut; }

	// Get functions should never get called.
	virtual char				GetChar()					{ Assert( 0 ); return 0; }
	virtual unsigned char		GetUnsignedChar()			{ Assert( 0 ); return 0; }
	virtual int					GetInt()					{ Assert( 0 ); return 0; }
	virtual void				Get( void* pMem, int size )	{ Assert( 0 ); }

	virtual void PutChar( char c )							{ Put( &c, sizeof( c ) ); }
	virtual void PutUnsignedChar( unsigned char uc ) 		{ Put( &uc, sizeof( uc ) ); }
	virtual void PutInt( int i )							{ Put( &i, sizeof( i ) ); }
	virtual void Put( const void* pMem, int nSize )
	{
		Assert( m_pWrite - m_pDataCache + nSize < CACHE_SIZE );
		V_memcpy( m_pWrite, pMem, nSize );
		m_pWrite += nSize;
		m_nBufferSize += nSize;
		m_nMaxPut = std::max( m_nMaxPut, nSize );
	}

	virtual void WriteTick( int nTick )
	{
		// Cache the relative position of the tick in memory for the given frame
		m_nCurrentTickOffset = m_pWrite - m_pDataCache;

		// Write the tick
		PutInt( nTick );
	}

	//
	// For thread safety - this counter keeps us from removing stale frames while writing a .dem file.
	// The idea here is that if the counter is anything but zero we should not remove stale frames.
	// We increment before creating a new job, and decrement from within that job, at the end.  We 
	// pass an iterator as an argument into the job's constructor so we know where to stop iterating
	// across the frame list.
	//
	mutable std::atomic<int> m_nWriteCount;

	//
	// Threaded .dem file write
	//
	class CDemWriteJob : public CJob
	{
	public:
		CDemWriteJob( const CMemoryDemoBuffer *pDemobuffer, const char *pFilename, const demoheader_t &header )
			: m_pDemobuffer( pDemobuffer ), m_pFilename( pFilename ), m_Header( header )
		{
			Assert( pFilename && pFilename[0] );
		}

		virtual JobStatus_t DoExecute()
		{
			// TODO: Does it make sense to return JOB_OK here even on failure?
			JobStatus_t nResult = JOB_OK;

			if ( m_pFilename && m_pFilename[0] != '\0' )
			{
				ks::byte_writer buf;
				{
					// NOTE: We include the sync tick frame as part of our signon data, which makes the header signon length vary by 6 bytes
					// (2 chars and 1 int) from our own signon data size. 
					const int nTickSyncFrameSize = 6;
					Assert( m_pDemobuffer->m_pSignonData->nSize == m_Header.signonlength + nTickSyncFrameSize );

					// Compute adjusted time/ticks/frames, since we may have removed stale frames
					const auto &lstFrames = m_pDemobuffer->m_lstFrames;
					Assert( !lstFrames.empty() );

					const DataChunk_t *pHead = lstFrames.front().get();
					const DataChunk_t *pTail = lstFrames.back().get();

					demoheader_t header = m_Header;
					header.playback_time = TICKS_TO_TIME( pTail->nTickcount - pHead->nTickcount );
					header.playback_ticks = pTail->nTickcount - pHead->nTickcount;
					header.playback_frames = std::ssize( lstFrames );

					// Write header
					buf.write( header );

					// Write signon data
					AssertValidReadPtr( m_pDemobuffer->m_pSignonData );
					buf.write_bytes( m_pDemobuffer->m_pSignonData->pData, m_pDemobuffer->m_pSignonData->nSize );

					// Get first recording tick (NOTE: not start global tick).  Recording ticks start at 0 but
					// when we remove stale frames the first recording tick becomes greater than zero.  We use
					// nStartTick here to shift all frame recording ticks down nStartTick ticks.
					int nStartTick;
					if ( !lstFrames.empty() )
					{
						DataChunk_t *pFrame = lstFrames.front().get();
						int *pTickData = reinterpret_cast< int * >( pFrame->pData + pFrame->nCurrentTickOffset );
						V_memcpy( &nStartTick, pTickData, sizeof( int ) );
					}

					// Write frames
					for ( const auto &frame : lstFrames )
					{
						DataChunk_t *pFrame = frame.get();			Assert( pFrame->nSize >= 0 );

						// Overwrite the tick for the given frame if one exists
						if ( nStartTick > 0 && pFrame->nCurrentTickOffset >= 0 )
						{
							int nNewTick;
							int *pTickData = reinterpret_cast< int * >( pFrame->pData + pFrame->nCurrentTickOffset );
							
							// Copy tick from buffer to nNewTick
							V_memcpy( &nNewTick, pTickData, sizeof( int ) );

							// Subtract out start tick
							nNewTick -= nStartTick;

							// Copy back to buffer
							V_memcpy( pTickData, &nNewTick, sizeof( int ) );
						}

						buf.write_bytes( pFrame->pData, pFrame->nSize );
					}

					// Write dem_stop cmd
					buf.write< unsigned char >( dem_stop );
					buf.write< int >( lstFrames.back()->nTickcount );
					buf.write< char >( 0 );

					if ( !g_pFullFileSystem->WriteFile( m_pFilename, nullptr, buf.bytes() ) )
					{
						Warning( "demobuffer: Failed to open file for writing, %s\n", m_pFilename );
					}
				}
			}

			// Decrement the write counter
			m_pDemobuffer->m_nWriteCount--;

			return nResult;
		}

	private:
		const CMemoryDemoBuffer	*m_pDemobuffer;
		const char				*m_pFilename;
		const demoheader_t		&m_Header;
	};

	virtual void UpdateStartTick( int& nStartTick ) const
	{
		if ( m_lstFrames.empty() )
			return;

		nStartTick = m_lstFrames.front()->nTickcount;
	}

	virtual void DumpToFile( const char *pFilename, const demoheader_t &header ) const
	{
		Assert( !false );	// TODO: Not supporting 360 yet.  Need alternate thread pool setup to do so.

		// HACK:
		int n = m_nWriteCount;

		// Critical section
		m_nWriteCount++;

		// Start a new thread
		CDemWriteJob* pJob = new CDemWriteJob( this, pFilename, header );
		g_pThreadPool->AddJob( pJob );

		while ( m_nWriteCount > n )
			DevMsg( "Waiting for file dump to complete\n" );
	}
};
#endif

//-----------------------------------------------------------------------------
// Factory function
//-----------------------------------------------------------------------------
IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t& params )
{
	IDemoBuffer *pRet;
#if defined( REPLAY_ENABLED )
	if ( bMemoryBuffer )
	{
		pRet = static_cast< IDemoBuffer* >( new CMemoryDemoBuffer() );
	}
	else
#endif
	{
		pRet = static_cast< IDemoBuffer* >( new CDiskDemoBuffer() );
	}

	if ( !pRet->Init( params ) )
	{
		delete pRet;
		return nullptr;
	}

	return pRet;
}
