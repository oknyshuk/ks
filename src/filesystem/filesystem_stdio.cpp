// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation





import ks.tier1;
#include "basefilesystem.h"

#include "filesystemasync.h"


import std;


#define GAMEINFO_FILENAME "GAMEINFO.TXT"

bool ShouldFailIo()
{
	return false;
}





static_assert( k_nSeekCur == FILESYSTEM_SEEK_CURRENT );
static_assert( k_nSeekSet == FILESYSTEM_SEEK_HEAD );
static_assert( k_nSeekEnd == FILESYSTEM_SEEK_TAIL );



//-----------------------------------------------------------------------------

class CFileSystem_Stdio : public CBaseFileSystem
{
public:
	CFileSystem_Stdio();
	~CFileSystem_Stdio();

	// Used to get at older versions
	void *QueryInterface( const char *pInterfaceName );

	// Higher level filesystem methods requiring specific behavior
	virtual void GetLocalCopy( const char *pFileName );
	virtual int	HintResourceNeed( const char *hintlist, int forgetEverything );
	virtual bool IsFileImmediatelyAvailable(const char *pFileName);
	virtual WaitForResourcesHandle_t WaitForResources( const char *resourcelist );
	virtual bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ );
	virtual void CancelWaitForResources( WaitForResourcesHandle_t handle );

	bool GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	void *AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset );
	void FreeOptimalReadBuffer( void *p );

protected:
	// implementation of CBaseFileSystem virtual functions
	virtual std::FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo );
	virtual void FS_setbufsize( std::FILE *fp, unsigned nBytes );
	virtual void FS_fclose( std::FILE *fp );
	virtual void FS_fseek( std::FILE *fp, int64 pos, int seekType );
	virtual long FS_ftell( std::FILE *fp );
	virtual int FS_feof( std::FILE *fp );
	virtual std::size_t FS_fread( void *dest, std::size_t destSize, std::size_t size, std::FILE *fp );
	virtual std::size_t FS_fwrite( const void *src, std::size_t size, std::FILE *fp );
	virtual bool FS_setmode( std::FILE *fp, FileMode_t mode );
	virtual std::size_t FS_vfprintf( std::FILE *fp, const char *fmt, std::va_list list );
	virtual int FS_ferror( std::FILE *fp );
	virtual int FS_fflush( std::FILE *fp );
	virtual char *FS_fgets( char *dest, int destSize, std::FILE *fp );
	virtual int FS_stat( const char *path, ks::platform::file_info *buf );
	virtual bool FS_setwritable( const char *path, bool writable );
	virtual HANDLE FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat);
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat);
	virtual bool FS_FindClose(HANDLE handle);
	virtual int FS_GetSectorSize( std::FILE * );

private:
	bool CanAsync() const
	{
		return m_bCanAsync;
	}

	bool m_bMounted;
	bool m_bCanAsync;
};


//-----------------------------------------------------------------------------
// Per-file worker classes
//-----------------------------------------------------------------------------
class CStdFilesystemFile
{
public:
	virtual ~CStdFilesystemFile() {}
	virtual void FS_setbufsize( unsigned nBytes ) = 0;
	virtual void FS_fclose() = 0;
	virtual void FS_fseek( int64 pos, int seekType ) = 0;
	virtual long FS_ftell() = 0;
	virtual int FS_feof() = 0;
	virtual std::size_t FS_fread( void *dest, std::size_t destSize, std::size_t size ) = 0;
	virtual std::size_t FS_fwrite( const void *src, std::size_t size ) = 0;
	virtual bool FS_setmode( FileMode_t mode ) = 0;
	virtual std::size_t FS_vfprintf( const char *fmt, std::va_list list ) = 0;
	virtual int FS_ferror() = 0;
	virtual int FS_fflush() = 0;
	virtual char *FS_fgets( char *dest, int destSize ) = 0;
	virtual int FS_GetSectorSize() { return 1; }
};

//---------------------------------------------------------

class CStdioFile : public CStdFilesystemFile
{
public:
	static CStdioFile *FS_fopen( const char *filename, const char *options, int64 *size );

	virtual void FS_setbufsize( unsigned nBytes );
	virtual void FS_fclose();
	virtual void FS_fseek( int64 pos, int seekType );
	virtual long FS_ftell();
	virtual int FS_feof();
	virtual std::size_t FS_fread( void *dest, std::size_t destSize, std::size_t size);
	virtual std::size_t FS_fwrite( const void *src, std::size_t size );
	virtual bool FS_setmode( FileMode_t mode );
	virtual std::size_t FS_vfprintf( const char *fmt, std::va_list list );
	virtual int FS_ferror();
	virtual int FS_fflush();
	virtual char *FS_fgets( char *dest, int destSize );

	static std::map< int, std::atomic<int> > m_LockedFDMap;
private:
	CStdioFile( std::FILE *pFile, bool bWriteable )
		: m_pFile( pFile ), m_bWriteable( bWriteable )
	{
	}

	std::FILE *m_pFile;
	bool m_bWriteable;
};

std::map< int, std::atomic<int> > CStdioFile::m_LockedFDMap;

//-----------------------------------------------------------------------------




//-----------------------------------------------------------------------------
// singleton
//-----------------------------------------------------------------------------
CFileSystem_Stdio g_FileSystem_Stdio;

CAsyncFileSystem g_FileSystem_Async;

 
#if defined( DEDICATED ) && defined( LAUNCHERONLY ) // "hack" to allow us to not export a stdio version of the FILESYSTEM_INTERFACE_VERSION anywhere

IFileSystem *g_pFileSystem = &g_FileSystem_Stdio;
IBaseFileSystem *g_pBaseFileSystem = &g_FileSystem_Stdio;


#else

static InterfaceReg s_Reg_CFileSystem_Stdio_IFileSystem( []() -> void * { return static_cast<IFileSystem *>( &g_FileSystem_Stdio ); }, FILESYSTEM_INTERFACE_VERSION );;
static InterfaceReg s_Reg_CFileSystem_Stdio_IBaseFileSystem( []() -> void * { return static_cast<IBaseFileSystem *>( &g_FileSystem_Stdio ); }, BASEFILESYSTEM_INTERFACE_VERSION );;

#endif

static InterfaceReg s_Reg_CAsyncFileSystem_IAsyncFileSystem( []() -> void * { return static_cast<IAsyncFileSystem *>( &g_FileSystem_Async ); }, ASYNCFILESYSTEM_INTERFACE_VERSION );;


//-----------------------------------------------------------------------------

bool UseOptimalBufferAllocation()
{
	static bool bUseOptimalBufferAllocation = false;
	return bUseOptimalBufferAllocation;
}
ConVar filesystem_unbuffered_io( "filesystem_unbuffered_io", "1", 0, "" );
[[gnu::always_inline]] inline auto UseUnbufferedIO() { return ( UseOptimalBufferAllocation() && filesystem_unbuffered_io.GetBool() ); }

ConVar filesystem_native( "filesystem_native", "1", 0, "Use native FS or STDIO" );
ConVar filesystem_max_stdio_read( "filesystem_max_stdio_read", "16", 0, "" );
ConVar filesystem_report_buffered_io( "filesystem_report_buffered_io", "0" );


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CFileSystem_Stdio::CFileSystem_Stdio()
{
	m_bMounted = false;
	m_bCanAsync = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileSystem_Stdio::~CFileSystem_Stdio()
{
	contract_assert( !m_bMounted );
}


//-----------------------------------------------------------------------------
// QueryInterface: 
//-----------------------------------------------------------------------------
void *CFileSystem_Stdio::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!V_strncmp(	pInterfaceName, FILESYSTEM_INTERFACE_VERSION, V_strlen(FILESYSTEM_INTERFACE_VERSION) + 1))
		return (IFileSystem*)this;

	return CBaseFileSystem::QueryInterface( pInterfaceName );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{
	unsigned sectorSize;
	
	CFileHandle *fh = ( CFileHandle *)hFile;
	if ( fh && fh->m_VPKHandle  )
	{
		return false;
	}
	if ( hFile && UseOptimalBufferAllocation() )
	{
		sectorSize = fh->GetSectorSize();
		
		if ( !sectorSize || ( fh->m_pPackFileHandle && ( fh->m_pPackFileHandle->AbsoluteBaseOffset() % sectorSize ) ) )
		{
			sectorSize = 1;
		}
	}
	else
	{
		sectorSize = 1;
	}

	if ( pOffsetAlign )
	{
		*pOffsetAlign = sectorSize;
	}

	if ( pSizeAlign )
	{
		*pSizeAlign = sectorSize;
	}

	if ( pBufferAlign )
	{
		{
			*pBufferAlign = sectorSize;
		}
	}

	return ( sectorSize > 1 );
}

// was from launcher.cpp in EA PS3 port, but can't do that in a PRX
const char* GetGameMode()
{
	return "portal2";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void *CFileSystem_Stdio::AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset )
{
	if ( !UseOptimalBufferAllocation() )
	{
		return CBaseFileSystem::AllocOptimalReadBuffer( hFile, nSize, nOffset );
	}

	unsigned sectorSize;
	if ( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		CFileHandle *fh = ( CFileHandle *)hFile;
		sectorSize = fh->GetSectorSize();

		if ( !nSize )
		{
			nSize = fh->Size();
		}

		if ( fh->m_pPackFileHandle )
		{
			nOffset += fh->m_pPackFileHandle->AbsoluteBaseOffset();
		}
	}
	else
	{
		// an invalid handle gets a fake "optimal" but valid buffer
		// this path is for a caller that isn't doing i/o, 
		// but needs an "optimal" buffer that can end up passed to FreeOptimalReadBuffer()
		sectorSize = 4;
	}

	bool bOffsetIsAligned = ( nOffset % sectorSize == 0 );
	unsigned nAllocSize = ( bOffsetIsAligned ) ? AlignValue( nSize, sectorSize ) : nSize;

	{
		unsigned nAllocAlignment = ( bOffsetIsAligned ) ? sectorSize : 4;
		return MemAlloc_AllocAlignedUnattributed( nAllocSize, nAllocAlignment );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FreeOptimalReadBuffer( void *p )
{
	if ( !UseOptimalBufferAllocation() )
	{
		CBaseFileSystem::FreeOptimalReadBuffer( p );
		return;
	}

	if ( p )
	{
		{
			 MemAlloc_FreeAligned( p );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::FILE *CFileSystem_Stdio::FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo )
{
	if( ShouldFailIo() )
		return nullptr;

	CStdFilesystemFile *pFile = nullptr;

	pFile = CStdioFile::FS_fopen( filename, options, size );

	return (std::FILE *)pFile;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_setbufsize( std::FILE *fp, unsigned nBytes )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	pFile->FS_setbufsize( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fclose( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileClose( fp );

	pFile->FS_fclose();
	delete pFile;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fseek( std::FILE *fp, int64 pos, int seekType )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileSeek( fp, pos, seekType );

	pFile->FS_fseek( pos, seekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CFileSystem_Stdio::FS_ftell( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ftell();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_feof( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_feof();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::size_t CFileSystem_Stdio::FS_fread( void *dest, std::size_t destSize, std::size_t size, std::FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	std::size_t nBytesRead = pFile->FS_fread( dest, destSize, size);

	Trace_FRead( nBytesRead, fp );

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileRead( dest, nBytesRead, size, fp );

	return nBytesRead;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::size_t CFileSystem_Stdio::FS_fwrite( const void *src, std::size_t size, std::FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	std::size_t nBytesWritten = pFile->FS_fwrite(src, size);

	return nBytesWritten;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_setmode( std::FILE *fp, FileMode_t mode )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_setmode( mode );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::size_t CFileSystem_Stdio::FS_vfprintf( std::FILE *fp, const char *fmt, std::va_list list )
{
	if( ShouldFailIo() )
		return 0;
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_vfprintf(fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_ferror( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ferror();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_fflush( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_fflush();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CFileSystem_Stdio::FS_fgets( char *dest, int destSize, std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_fgets(dest, destSize);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			pmode - 
// Output : int
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_setwritable( const char *path, bool writable )
{
	if ( !path )
		return false;

	bool ok = ks::platform::set_path_writable( path, writable );
	if ( !ok )
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(path, file) )
		{
			ok = ks::platform::set_path_writable( file, writable );
		}
	}	
	return ok;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_stat( const char *path, ks::platform::file_info *buf )
{
	if ( !path )
	{
		return -1;
	}

	bool ok = ks::platform::stat_path( path, buf );
	if ( !ok )
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive( path, file ) )
		{
			ok = ks::platform::stat_path( file, buf );
		}
	}	
	return ok ? 0 : -1;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
HANDLE CFileSystem_Stdio::FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat)
{
	return ::FindFirstFile(const_cast<char *>(findname), dat);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat)
{
	return (::FindNextFile(handle, dat) != 0);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_FindClose(HANDLE handle)
{
	return (::FindClose(handle) != 0);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_GetSectorSize( std::FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_GetSectorSize();
}

//-----------------------------------------------------------------------------
// Purpose: files are always immediately available on disk
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::IsFileImmediatelyAvailable(const char *pFileName)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: unnecessary in stdio
//-----------------------------------------------------------------------------
WaitForResourcesHandle_t CFileSystem_Stdio::WaitForResources( const char *resourcelist )
{

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: unnecessary in stdio
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ )
{
	CVProfScope _( "GetWaitForResourcesProgress (stdio)", 0, VPROF_BUDGETGROUP_OTHER_FILESYSTEM, false, BUDGETFLAG_OTHER );

	// always return that we're complete
	*progress = 0.0f;
	*complete = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: unnecessary in stdio
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::CancelWaitForResources( WaitForResourcesHandle_t handle )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::GetLocalCopy( const char *pFileName )
{
	// do nothing. . everything is local.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::HintResourceNeed( const char *hintlist, int forgetEverything )
{
	// do nothing. . everything is local.
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
CStdioFile *CStdioFile::FS_fopen( const char *filename, const char *options, int64 *size )
{

	std::FILE *pFile = nullptr;

	// stop newline characters at end of filename
	contract_assert( !std::strchr(filename, '\n') && !std::strchr(filename, '\r') );
	
	pFile = std::fopen(filename, options);
	if (pFile && size)
	{
		// todo: replace with filelength()? 
		ks::platform::file_info buf;
		if ( ks::platform::stat_path( filename, &buf ) )
		{
			*size = buf.size;
		}
	}

	if(!pFile && !std::strchr(options,'w') && !std::strchr(options,'+') ) // try opening the lower cased version
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(filename, file ) )
		{	
			pFile = std::fopen( file, options );

			if (pFile && size)
			{
				// todo: replace with filelength()? 
				ks::platform::file_info buf;
				if ( ks::platform::stat_path( file, &buf ) )
				{
					*size = buf.size;
				}
			}
		}
	}

	if ( pFile )
	{
		bool bWriteable = false;
		if ( std::strchr(options,'w') || std::strchr(options,'a') )
			bWriteable = true;
		
		if ( bWriteable )
		{
			// Win32 has an undocumented feature that is serialized ALL writes to a file across threads (i.e only 1 thread can open a file at a time)
			// so use flock here to mimic that behavior
			
			ThreadId_t curThread = ThreadGetCurrentId();
			
			int fd = (int)ks::platform::stream_identity( pFile );
			auto iLockID = m_LockedFDMap.find( fd );
			ks::platform::lock_result lockResult = ks::platform::lock_stream( pFile, false );
			if ( lockResult == ks::platform::lock_result::would_block )
			{
				if ( iLockID != m_LockedFDMap.end() && 
					 iLockID->second != -1 && 
					 curThread != iLockID->second )
				{
					if ( ks::platform::lock_stream( pFile, true ) != ks::platform::lock_result::acquired )
					{
						std::fclose( pFile );
						return nullptr;
					}
				}
			}
			else if ( lockResult == ks::platform::lock_result::failed )
			{
				std::fclose( pFile );
				return nullptr;
			}

			if ( iLockID != m_LockedFDMap.end() )
				iLockID->second = curThread;
			else
				m_LockedFDMap.emplace( fd, curThread );
			
			std::rewind( pFile );
		}
		return new CStdioFile( pFile, bWriteable );
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_setbufsize( unsigned nBytes )
{
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_fclose()
{
	if ( m_bWriteable )
	{
		std::fflush( m_pFile );
		int fd = (int)ks::platform::stream_identity( m_pFile );
		ks::platform::unlock_stream( m_pFile );
		auto iLockID = m_LockedFDMap.find( fd );
		if ( iLockID != m_LockedFDMap.end() )
			iLockID->second = -1;
	}
	std::fclose(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_fseek( int64 pos, int seekType )
{
	int nNewSeekType = seekType;
	
	// Handle values outside the 32-bit signed range.
	// Only 0 or 1 of the while loops will execute inthe same call.

	while ( pos > std::numeric_limits<int>::max() )
	{
		std::fseek( m_pFile, std::numeric_limits<int>::max(), nNewSeekType );
		pos -= std::numeric_limits<int>::max();
		nNewSeekType = k_nSeekCur; // Now all seeks need to be relative
	}

	while ( pos < std::numeric_limits<int>::min() )
	{
		std::fseek( m_pFile, std::numeric_limits<int>::min(), nNewSeekType );
		pos -= std::numeric_limits<int>::min();
		nNewSeekType = k_nSeekCur; // Now all seeks need to be relative
	}

	std::fseek( m_pFile, pos, nNewSeekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CStdioFile::FS_ftell()
{
	return std::ftell(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_feof()
{
	return std::feof(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::size_t CStdioFile::FS_fread( void *dest, std::size_t destSize, std::size_t size )
{
	// read (size) of bytes to ensure truncated reads returns bytes read and not 0
	return std::fread( dest, 1, size, m_pFile );
}


constexpr auto WRITE_CHUNK = (256 * 1024);

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//
// This routine breaks data into chunks if the amount to be written is beyond WRITE_CHUNK (256kb)
// Windows can fail on monolithic writes of ~12MB or more, so we work around that here
//-----------------------------------------------------------------------------
std::size_t CStdioFile::FS_fwrite( const void *src, std::size_t size )
{
	if ( size > WRITE_CHUNK )
	{
		std::size_t remaining = size;
		const byte* current = (const byte *) src;
		std::size_t total = 0;

		while ( remaining > 0 )
		{
			std::size_t bytesToCopy = std::min<std::common_type_t<std::size_t, int>>(remaining, WRITE_CHUNK);

			total += std::fwrite(current, 1, bytesToCopy, m_pFile);

			remaining -= bytesToCopy;
			current += bytesToCopy;
		}

		contract_assert( total == size );
		return total;
	}

	return std::fwrite(src, 1, size, m_pFile);// return number of bytes written (because we have size = 1, count = bytes, so it return bytes)
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CStdioFile::FS_setmode( FileMode_t mode )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
std::size_t CStdioFile::FS_vfprintf( const char *fmt, std::va_list list )
{
	return std::vfprintf(m_pFile, fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_ferror()
{
	return std::ferror(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_fflush()
{
	return std::fflush(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CStdioFile::FS_fgets( char *dest, int destSize )
{
	return std::fgets(dest, destSize, m_pFile);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
