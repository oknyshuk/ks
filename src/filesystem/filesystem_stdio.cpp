//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"


#include "basefilesystem.h"

#include "filesystemasync.h"

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"

#include <fcntl.h>
#include <sys/file.h>
#include "tier1/convar.h"
#include "tier0/vprof.h"
#include "tier1/fmtstr.h"
#include "tier1/utlrbtree.h"


#define GAMEINFO_FILENAME "GAMEINFO.TXT"

bool ShouldFailIo()
{
	return false;
}





// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ASSERT_INVARIANT( SEEK_CUR == FILESYSTEM_SEEK_CURRENT );
ASSERT_INVARIANT( SEEK_SET == FILESYSTEM_SEEK_HEAD );
ASSERT_INVARIANT( SEEK_END == FILESYSTEM_SEEK_TAIL );



#if __DARWIN_64_BIT_INO_T
#error badness
#endif

#if _DARWIN_FEATURE_64_BIT_INODE
#error additional badness
#endif
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
	virtual bool IsSteam() const { return false; }
	virtual	FilesystemMountRetval_t MountSteamContent( int nExtraAppId = -1 ) { return FILESYSTEM_MOUNT_OK; }

	bool GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	void *AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset );
	void FreeOptimalReadBuffer( void *p );

protected:
	// implementation of CBaseFileSystem virtual functions
	virtual FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo );
	virtual void FS_setbufsize( FILE *fp, unsigned nBytes );
	virtual void FS_fclose( FILE *fp );
	virtual void FS_fseek( FILE *fp, int64 pos, int seekType );
	virtual long FS_ftell( FILE *fp );
	virtual int FS_feof( FILE *fp );
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size, FILE *fp );
	virtual size_t FS_fwrite( const void *src, size_t size, FILE *fp );
	virtual bool FS_setmode( FILE *fp, FileMode_t mode );
	virtual size_t FS_vfprintf( FILE *fp, const char *fmt, va_list list );
	virtual int FS_ferror( FILE *fp );
	virtual int FS_fflush( FILE *fp );
	virtual char *FS_fgets( char *dest, int destSize, FILE *fp );
	virtual int FS_stat( const char *path, struct _stat *buf );
	virtual int FS_chmod( const char *path, int pmode );
	virtual HANDLE FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat);
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat);
	virtual bool FS_FindClose(HANDLE handle);
	virtual int FS_GetSectorSize( FILE * );

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
abstract_class CStdFilesystemFile
{
public:
	virtual ~CStdFilesystemFile() {}
	virtual void FS_setbufsize( unsigned nBytes ) = 0;
	virtual void FS_fclose() = 0;
	virtual void FS_fseek( int64 pos, int seekType ) = 0;
	virtual long FS_ftell() = 0;
	virtual int FS_feof() = 0;
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size ) = 0;
	virtual size_t FS_fwrite( const void *src, size_t size ) = 0;
	virtual bool FS_setmode( FileMode_t mode ) = 0;
	virtual size_t FS_vfprintf( const char *fmt, va_list list ) = 0;
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
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size);
	virtual size_t FS_fwrite( const void *src, size_t size );
	virtual bool FS_setmode( FileMode_t mode );
	virtual size_t FS_vfprintf( const char *fmt, va_list list );
	virtual int FS_ferror();
	virtual int FS_fflush();
	virtual char *FS_fgets( char *dest, int destSize );

	static CUtlMap< int, CInterlockedInt > m_LockedFDMap;
private:
	CStdioFile( FILE *pFile, bool bWriteable )
		: m_pFile( pFile ), m_bWriteable( bWriteable )
	{
	}

	FILE *m_pFile;
	bool m_bWriteable;
};

CUtlMap< int, CInterlockedInt > CStdioFile::m_LockedFDMap;

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

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Stdio, IFileSystem, FILESYSTEM_INTERFACE_VERSION, g_FileSystem_Stdio );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Stdio, IBaseFileSystem, BASEFILESYSTEM_INTERFACE_VERSION, g_FileSystem_Stdio );

#endif

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAsyncFileSystem, IAsyncFileSystem, ASYNCFILESYSTEM_INTERFACE_VERSION, g_FileSystem_Async );


//-----------------------------------------------------------------------------

bool UseOptimalBufferAllocation()
{
	static bool bUseOptimalBufferAllocation = false;
	return bUseOptimalBufferAllocation;
}
ConVar filesystem_unbuffered_io( "filesystem_unbuffered_io", "1", 0, "" );
#define UseUnbufferedIO() ( UseOptimalBufferAllocation() && filesystem_unbuffered_io.GetBool() )

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
	SetDefLessFunc( CStdioFile::m_LockedFDMap );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileSystem_Stdio::~CFileSystem_Stdio()
{
	Assert(!m_bMounted);
}


//-----------------------------------------------------------------------------
// QueryInterface: 
//-----------------------------------------------------------------------------
void *CFileSystem_Stdio::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, FILESYSTEM_INTERFACE_VERSION, Q_strlen(FILESYSTEM_INTERFACE_VERSION) + 1))
		return (IFileSystem*)this;

	return CBaseFileSystem::QueryInterface( pInterfaceName );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{
	unsigned sectorSize;
	
	CFileHandle *fh = ( CFileHandle *)hFile;
#ifdef SUPPORT_VPK
	if ( fh && fh->m_VPKHandle  )
	{
		return false;
	}
#endif
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
		return _aligned_malloc( nAllocSize, nAllocAlignment );
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
			 _aligned_free( p );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
FILE *CFileSystem_Stdio::FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo )
{
	if( ShouldFailIo() )
		return NULL;

	CStdFilesystemFile *pFile = NULL;

	if ( pInfo )
		pInfo->m_bLoadedFromSteamCache = false;




	pFile = CStdioFile::FS_fopen( filename, options, size );

	return (FILE *)pFile;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_setbufsize( FILE *fp, unsigned nBytes )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	pFile->FS_setbufsize( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fclose( FILE *fp )
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
void CFileSystem_Stdio::FS_fseek( FILE *fp, int64 pos, int seekType )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileSeek( fp, pos, seekType );

	pFile->FS_fseek( pos, seekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CFileSystem_Stdio::FS_ftell( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ftell();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_feof( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_feof();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fread( void *dest, size_t destSize, size_t size, FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	size_t nBytesRead = pFile->FS_fread( dest, destSize, size);

	Trace_FRead( nBytesRead, fp );

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileRead( dest, nBytesRead, size, fp );

	return nBytesRead;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fwrite( const void *src, size_t size, FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	size_t nBytesWritten = pFile->FS_fwrite(src, size);

	return nBytesWritten;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_setmode( FILE *fp, FileMode_t mode )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_setmode( mode );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_vfprintf( FILE *fp, const char *fmt, va_list list )
{
	if( ShouldFailIo() )
		return 0;
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_vfprintf(fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_ferror( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ferror();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_fflush( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_fflush();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CFileSystem_Stdio::FS_fgets( char *dest, int destSize, FILE *fp )
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
int CFileSystem_Stdio::FS_chmod( const char *path, int pmode )
{
	if ( !path )
		return -1;

	int rt = _chmod( path, pmode );
	if (rt==-1)
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(path, file) )
		{
			rt=_chmod(file,pmode);
		}
	}	
	return rt;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_stat( const char *path, struct _stat *buf )
{
	if ( !path )
	{
		return -1;
	}

	int rt;
    rt = _stat( path, buf );
	if ( rt == -1 )
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive( path, file ) )
		{
			rt = _stat( file, buf );
		}
	}	
	return rt;
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
int CFileSystem_Stdio::FS_GetSectorSize( FILE *fp )
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

// enable this if you want the stdio filesystem to pretend it's steam, and make people wait for resources
//#define DEBUG_WAIT_FOR_RESOURCES_API

#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
static float g_flDebugProgress = 0.0f;
#endif

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
WaitForResourcesHandle_t CFileSystem_Stdio::WaitForResources( const char *resourcelist )
{
#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
	g_flDebugProgress = 0.0f;
#endif

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ )
{
	VPROF_BUDGET( "GetWaitForResourcesProgress (stdio)", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
	g_flDebugProgress += 0.002f;
	if (g_flDebugProgress < 1.0f)
	{
		*progress = g_flDebugProgress;
		*complete = false;
		return true;
	}
#endif

	// always return that we're complete
	*progress = 0.0f;
	*complete = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
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
	MEM_ALLOC_CREDIT();

	FILE *pFile = NULL;

	// stop newline characters at end of filename
	Assert(!strchr(filename, '\n') && !strchr(filename, '\r'));
	
	pFile = fopen(filename, options);
	if (pFile && size)
	{
		// todo: replace with filelength()? 
		struct _stat buf;
		int rt = _stat( filename, &buf );
		if (rt == 0)
		{
			*size = buf.st_size;
		}
	}

	if(!pFile && !strchr(options,'w') && !strchr(options,'+') ) // try opening the lower cased version
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(filename, file ) )
		{	
			pFile = fopen( file, options );

			if (pFile && size)
			{
				// todo: replace with filelength()? 
				struct _stat buf;
				int rt = _stat( file, &buf );
				if (rt == 0)
				{
					*size = buf.st_size;
				}
			}
		}
	}

	if ( pFile )
	{
		bool bWriteable = false;
		if ( strchr(options,'w') || strchr(options,'a') )
			bWriteable = true;
		
		if ( bWriteable )
		{
			// Win32 has an undocumented feature that is serialized ALL writes to a file across threads (i.e only 1 thread can open a file at a time)
			// so use flock here to mimic that behavior
			
			ThreadId_t curThread = ThreadGetCurrentId();
			
			int fd = fileno_unlocked( pFile );
			int iLockID = m_LockedFDMap.Find( fd );
			int ret = flock( fd, LOCK_EX | LOCK_NB );
			if ( ret < 0 )
			{
				if ( errno == EWOULDBLOCK  )
				{
					if ( iLockID != m_LockedFDMap.InvalidIndex() && 
						 m_LockedFDMap[iLockID] != -1 && 
						 curThread != m_LockedFDMap[iLockID] )
					{
						ret = flock( fd, LOCK_EX );
						if ( ret < 0 )
						{
							fclose( pFile );
							return NULL;
						}
					}
				}
				else 
				{
					fclose( pFile );
					return NULL;
				}
			}

			if ( iLockID != m_LockedFDMap.InvalidIndex() )
				m_LockedFDMap[iLockID] = curThread;
			else
				m_LockedFDMap.Insert( fd, curThread );
			
			rewind( pFile );
		}
		return new CStdioFile( pFile, bWriteable );
	}

	return NULL;
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
		fflush( m_pFile );
		int fd = fileno_unlocked( m_pFile );
		flock( fd, LOCK_UN );
		int iLockID = m_LockedFDMap.Find( fd );
		if ( iLockID != m_LockedFDMap.InvalidIndex() )
			m_LockedFDMap[ iLockID ] = -1;
	}
	fclose(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_fseek( int64 pos, int seekType )
{
	int nNewSeekType = seekType;
	
	// Handle values outside the 32-bit signed range.
	// Only 0 or 1 of the while loops will execute inthe same call.

	while ( pos > INT_MAX )
	{
		fseek( m_pFile, INT_MAX, nNewSeekType );
		pos -= INT_MAX;
		nNewSeekType = SEEK_CUR; // Now all seeks need to be relative
	}

	while ( pos < INT_MIN )
	{
		fseek( m_pFile, INT_MIN, nNewSeekType );
		pos -= INT_MIN;
		nNewSeekType = SEEK_CUR; // Now all seeks need to be relative
	}

	fseek( m_pFile, pos, nNewSeekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CStdioFile::FS_ftell()
{
	return ftell(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_feof()
{
	return feof(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CStdioFile::FS_fread( void *dest, size_t destSize, size_t size )
{
	// read (size) of bytes to ensure truncated reads returns bytes read and not 0
	return fread( dest, 1, size, m_pFile );
}


#define WRITE_CHUNK		(256 * 1024)

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//
// This routine breaks data into chunks if the amount to be written is beyond WRITE_CHUNK (256kb)
// Windows can fail on monolithic writes of ~12MB or more, so we work around that here
//-----------------------------------------------------------------------------
size_t CStdioFile::FS_fwrite( const void *src, size_t size )
{
	if ( size > WRITE_CHUNK )
	{
		size_t remaining = size;
		const byte* current = (const byte *) src;
		size_t total = 0;

		while ( remaining > 0 )
		{
			size_t bytesToCopy = MIN(remaining, WRITE_CHUNK);

			total += fwrite(current, 1, bytesToCopy, m_pFile);

			remaining -= bytesToCopy;
			current += bytesToCopy;
		}

		Assert( total == size );
		return total;
	}

	return fwrite(src, 1, size, m_pFile);// return number of bytes written (because we have size = 1, count = bytes, so it return bytes)
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
size_t CStdioFile::FS_vfprintf( const char *fmt, va_list list )
{
	return vfprintf(m_pFile, fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_ferror()
{
	return ferror(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_fflush()
{
	return fflush(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CStdioFile::FS_fgets( char *dest, int destSize )
{
	return fgets(dest, destSize, m_pFile);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------


