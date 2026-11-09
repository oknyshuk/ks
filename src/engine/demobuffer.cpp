// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: TODO:
// - USE A MEMPOOL OF SOME SORT
// - GET STALE FRAME REMOVAL WORKING
// - MAKE THREAD SAFE - NEED TO BE ABLE TO WRITE A .DEM FILE ON A SEP THREAD AND
//		NOT BE THROWING AWAY ELEMENTS THAT ARE BEING READ.  NEED A FLAG IN DATACHUNK
//		FOR WHETHER AN INSTANCE IS "IN USE," IE BEING READ TO WRITE TO A DEM FILE.

import ks.vstdlib;
#include "demobuffer.h"
#include "filesystem.h"
import std;
#include "edict.h"
#include "host.h"

#include "sv_client.h"
#ifndef DEDICATED
#include "demo.h"
#endif
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

	bool Init( DemoBufferInitParams_t const& params )
	{
		// Convert to proper type
		contract_assert( params.m_kind == DemoBufferInitParams_t::kind_t::stream );
		if ( params.m_kind != DemoBufferInitParams_t::kind_t::stream )
			return false;
		StreamDemoBufferInitParams_t const* pParams = static_cast< StreamDemoBufferInitParams_t const* >( &params );

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

	void NotifySignonComplete() {}

	void WriteHeader( void const *pData, int nSize )
	{
		// Goto file start
		SeekPut( true, 0 );

		// Write
		Put( pData, nSize );
	}

	void				NotifyBeginFrame() {}
	void				NotifyEndFrame() {}

	void				PutChar( char c )						{ Put( &c, sizeof( c ) ); }
	void				PutUnsignedChar( unsigned char uc )		{ Put( &uc, sizeof( uc ) ); }
	void				PutInt( int i )							{ Put( &i, sizeof( i ) ); }

	void				WriteTick( int nTick )					{ Put( &nTick, sizeof( nTick ) ); }

	char				GetChar()
	{
		char c = 0;
		Get( &c, sizeof( c ) );
		return c;
	}
	unsigned char		GetUnsignedChar()
	{
		unsigned char uc = 0;
		Get( &uc, sizeof( uc ) );
		return uc;
	}
	int					GetInt()
	{
		int i = 0;
		Get( &i, sizeof( i ) );
		return i;
	}

	void				Get( void* pMem, int size )
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

	void				Put( const void* pMem, int size )
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

	bool				IsValid() const							{ return !m_bGetFailed && !m_bPutFailed; }
	bool				IsInitialized() const					{ return IsValid() && m_hFile != FILESYSTEM_INVALID_HANDLE; }

	// Change where I'm writing (put)/reading (get)
	void				SeekPut( bool bAbsolute, int offset )
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

	void				SeekGet( bool bAbsolute, int offset )
	{
		m_nGet = bAbsolute ? offset : m_nGet + offset;
		m_bGetFailed = m_nGet < 0 || m_nGet > m_nMaxPut;
	}

	// Where am I writing (put)/reading (get)?
	int					TellPut( ) const						{ return m_nPut; }
	int					TellGet( ) const						{ return m_nGet; }

	int					TellMaxPut( ) const						{ return m_nMaxPut; }

	void				UpdateStartTick( int& nStartTick ) const {}
	void				DumpToFile( char const* pFilename, const demoheader_t &header ) const {}

public:
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

#include "facade/idemobuffer.inl"



//-----------------------------------------------------------------------------
// Specialty class with overrides for stream buffer
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Factory function
//-----------------------------------------------------------------------------
IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t& params )
{
	IDemoBuffer *pRet;
	{
		pRet = static_cast< IDemoBuffer* >( new CDiskDemoBuffer() );
	}

	if ( !pRet->Init( params ) )
	{
		DestroyDemoBuffer( pRet );
		return nullptr;
	}

	return pRet;
}

void DestroyDemoBuffer( IDemoBuffer *pBuffer )
{
	delete static_cast< CDiskDemoBuffer * >( pBuffer );
}
