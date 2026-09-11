//========= Copyright 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: A collection of utility classes to simplify file I/O, and
//			as much as possible contain portability problems. Here avoiding 
//			including windows.h.
//
//=============================================================================


#include <sys/stat.h>


#define ASYNC_FILEIO
// Linux hasn't got a good AIO library that we have found yet, so lets punt for now
#undef ASYNC_FILEIO

#include <utime.h>
#include <dirent.h>
#include <unistd.h> // for unlink
#include <limits.h> // defines PATH_MAX
#include <alloca.h> // 'cause we like smashing the stack
#include <sys/fcntl.h>
#include <sys/statvfs.h>
#include <sched.h>

//lwss - Add tier0/platform.h to get this to build. Should be ok since it is header-only.
#include "tier0/platform.h"
#define int64 int64_t
//lwss end


// On OSX the native API file offset is always 64-bit
// and things like stat64 are deprecated.
// Use the 64-bit file I/O API.
typedef off64_t offBig_t;
typedef struct stat64 statBig_t;
typedef struct statvfs64 statvfsBig_t;
typedef struct dirent64 direntBig_t;
#define openBig open64
#define lseekBig lseek64
#define preadBig pread64
#define pwriteBig pwrite64
#define statBig stat64
#define lstatBig lstat64
#define readdirBig readdir64
#define scandirBig scandir64
#define alphasortBig alphasort64
#define fopenBig fopen64
#define fseekBig fseeko64
#define ftellBig ftello64
#define ftruncateBig ftruncate64
#define fstatBig fstat64
#define statvfsBig statvfs64
#define mmapBig mmap64

struct _finddata_t
{   
	_finddata_t()
	{
		name[0] = '\0';
		dirBase[0] = '\0';
		curName = 0;
		numNames = 0;
		namelist = nullptr;
	}
	// public data
	char name[PATH_MAX]; // the file name returned from the call
	char dirBase[PATH_MAX];
	offBig_t size;
	mode_t attrib;
	time_t time_write;
	time_t time_create;
	int curName;
	int numNames;
	direntBig_t **namelist;
};

#define _A_SUBDIR S_IFDIR

// FUTURE map _A_HIDDEN via checking filename against .*
#define _A_HIDDEN 0

// FUTURE check 'read only' by checking mode against S_IRUSR
#define _A_RDONLY 0

// no files under posix are 'system' or 'archive'
#define _A_SYSTEM 0
#define _A_ARCH   0

int _findfirst( const char *pchBasePath, struct _finddata_t *pFindData );
int _findnext( const int64 hFind, struct _finddata_t *pFindData );
bool _findclose( int64 hFind );
static int FileSelect( const char *name, const char *mask );


#include "tier1/fileio.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"
#include <errno.h>
#include "vstdlib/vstrtools.h"

#if defined( WIN32_FILEIO )
#include "winlite.h"
#endif

#if defined( ASYNC_FILEIO )
#include <aio.h>
#endif

#define INVALID_HANDLE_VALUE nullptr

#define _rmdir rmdir

#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE


#define PvAlloc( cub )  malloc( cub )
#define PvRealloc( pv, cub ) realloc( pv, cub )
#define FreePv( pv ) free( pv )

//-----------------------------------------------------------------------------
// Purpose: Constructor from UTF8
//-----------------------------------------------------------------------------
CPathString::CPathString( const char *pchUTF8Path )
{
	// Need to first turn into an absolute path, so \\?\ pre-pended paths will be ok
	m_pchUTF8Path = new char[ MAX_UNICODE_PATH_IN_UTF8 ];
	m_pwchWideCharPathPrepended = nullptr;

	// First, convert to absolute path, which also does Q_FixSlashes for us.
	Q_MakeAbsolutePath( m_pchUTF8Path, MAX_UNICODE_PATH * 4, pchUTF8Path );

	// Second, fix any double slashes
	V_FixDoubleSlashes( m_pchUTF8Path );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPathString::~CPathString()
{
	if ( m_pwchWideCharPathPrepended )
	{
		delete[] m_pwchWideCharPathPrepended;
		m_pwchWideCharPathPrepended = nullptr;
	}

	if ( m_pchUTF8Path )
	{
		delete[] m_pchUTF8Path;
		m_pchUTF8Path = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Access UTF8 path
//-----------------------------------------------------------------------------
const char * CPathString::GetUTF8Path()
{
	return m_pchUTF8Path;
}


//-----------------------------------------------------------------------------
// Purpose: Gets wchar_t based path, with \\?\ pre-pended (allowing long paths 
// on Win32, should only be used with unicode extended path aware filesystem calls)
//-----------------------------------------------------------------------------
const wchar_t *CPathString::GetWCharPathPrePended()
{
	PopulateWCharPath();
	return m_pwchWideCharPathPrepended; 
}


//-----------------------------------------------------------------------------
// Purpose: Builds wchar path string
//-----------------------------------------------------------------------------
void CPathString::PopulateWCharPath()
{
	if ( m_pwchWideCharPathPrepended )
		return;

	// Check if the UTF8 path starts with \\, which on Win32 means it's a UNC path, and then needs a different prefix
	if ( m_pchUTF8Path[0] == '\\' && m_pchUTF8Path[1] == '\\' )
	{
		m_pwchWideCharPathPrepended = new wchar_t[MAX_UNICODE_PATH+8];
		Q_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\UNC\\", 8*sizeof(wchar_t) );
#ifdef DBGFLAG_ASSERT
		int cchResult =
#endif
			Q_UTF8ToUnicode( m_pchUTF8Path+2, m_pwchWideCharPathPrepended+8, MAX_UNICODE_PATH*sizeof(wchar_t) );
		Assert( cchResult );

		// Be sure we NULL terminate within our allocated region incase Q_UTF8ToUnicode failed, though we're already in bad shape then.
		m_pwchWideCharPathPrepended[MAX_UNICODE_PATH+7] = 0;
	}
	else
	{
		m_pwchWideCharPathPrepended = new wchar_t[MAX_UNICODE_PATH+4];
		Q_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\", 4*sizeof(wchar_t) );
#ifdef DBGFLAG_ASSERT
		int cchResult =
#endif
			Q_UTF8ToUnicode( m_pchUTF8Path, m_pwchWideCharPathPrepended+4, MAX_UNICODE_PATH*sizeof(wchar_t) );
		Assert( cchResult );

		// Be sure we NULL terminate within our allocated region incase Q_UTF8ToUnicode failed, though we're already in bad shape then.
		m_pwchWideCharPathPrepended[MAX_UNICODE_PATH+3] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper on PS3 to find next entry that matches the provided pattern
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

CDirIterator::CDirIterator( const char *pchPath, const char *pchPattern )
{
	CPathString strPath( pchPath );
	m_pFindData = nullptr;

	// +2 so we can potentially add path separator as well as null termination
	char *pchPathAndPattern = new char[Q_strlen( strPath.GetUTF8Path() ) + Q_strlen( pchPattern ) + 2];

	// be resilient about whether the caller passes us a path with a terminal path separator or not.

	// put in the path
	if (pchPath)
	{
		Q_strncpy( pchPathAndPattern, strPath.GetUTF8Path(), Q_strlen( strPath.GetUTF8Path() ) + 1 );

		// identify whether we've got a terminal separator. add one if not.
		char *pchRest = pchPathAndPattern + Q_strlen( pchPathAndPattern ) - 1;
		if (*pchRest != CORRECT_PATH_SEPARATOR)
		{
			*++pchRest = CORRECT_PATH_SEPARATOR;
		}
		pchRest++;

		// now put in the search pattern.
		Q_strncpy( pchRest, pchPattern, Q_strlen( pchPattern ) + 1 );

		Init( pchPathAndPattern );
	}
	else
	{
		pchPathAndPattern[0] = 0;
		m_bNoFiles = true;
		m_bUsedFirstFile = true;

		m_hFind = -1;
		m_pFindData = new _finddata_t;
		memset( m_pFindData, 0, sizeof(*m_pFindData) );

	}
	delete[] pchPathAndPattern;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDirIterator::CDirIterator( const char *pchSearchPath )
{
	Init( pchSearchPath );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize iteration structure
//-----------------------------------------------------------------------------
void CDirIterator::Init( const char *pchSearchPath )
{
	CPathString strBasePath( pchSearchPath );

	m_pFindData = new _finddata_t;
	memset( m_pFindData, 0, sizeof(*m_pFindData) );

	m_hFind = _findfirst( strBasePath.GetUTF8Path(), m_pFindData );
	bool bSuccess = (m_hFind != -1);

	if (!bSuccess)
	{
		m_bNoFiles = true;
		m_bUsedFirstFile = true;
	}
	else
	{
		m_bNoFiles = false;
		// if we're pointing at . or .., set it as used
		// so we'll look for the next item when BNextFile() is called
		m_bUsedFirstFile = !BValidFilename();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDirIterator::~CDirIterator()
{
	if (m_hFind != -1)
	{
		_findclose( m_hFind );
	}
	if (m_pFindData)
	{
		for (int i = 0; i < m_pFindData->numNames; i++)
		{
			// scandir allocates with malloc, so free with free
			free( m_pFindData->namelist[i] );
		}
		free( m_pFindData->namelist );
		delete m_pFindData;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check for successful construction
//-----------------------------------------------------------------------------
bool CDirIterator::IsValid() const
{
	return m_hFind != -1;
}

//-----------------------------------------------------------------------------
// Purpose: Filter out . and ..
//-----------------------------------------------------------------------------
bool CDirIterator::BValidFilename()
{
	const char *pch = m_pFindData->name;

	if ((pch[0] == '.' && pch[1] == 0) ||
		(pch[0] == '.' && pch[1] == '.' && pch[2] == 0))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if there is a file to read
//-----------------------------------------------------------------------------
bool CDirIterator::BNextFile()
{
	if (m_bNoFiles)
		return false;

	// use the first result
	if (!m_bUsedFirstFile)
	{
		m_bUsedFirstFile = true;
		return true;
	}

	// find the next item
	for (;;)
	{
		bool bFound = (_findnext( m_hFind, m_pFindData ) == 0);

		if (!bFound)
		{
			// done
			m_bNoFiles = true;
			return false;
		}

		// skip over the '.' and '..' paths
		if (!BValidFilename())
			continue;

		break;
	}

	// have one more file
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns name (filename portion only) of the current file.
// Name is emitted in UTF-8 encoding.
// NOTE: This method returns a pointer into a static buffer, either a member
// or the buffer inside the _finddata_t.
//-----------------------------------------------------------------------------
const char *CDirIterator::CurrentFileName()
{
	return m_pFindData->name;
}


//-----------------------------------------------------------------------------
// Purpose: returns size of the file
//-----------------------------------------------------------------------------
int64 CDirIterator::CurrentFileLength() const
{
	return (int64)m_pFindData->size;
}


//-----------------------------------------------------------------------------
// Purpose: returns last write time of the file
//-----------------------------------------------------------------------------
time64_t CDirIterator::CurrentFileWriteTime() const
{
	return m_pFindData->time_write;
}


//-----------------------------------------------------------------------------
// Purpose: returns the creation time of the file
//-----------------------------------------------------------------------------
time64_t CDirIterator::CurrentFileCreateTime() const
{
	return m_pFindData->time_create;
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is a directory
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsDir() const
{
	return (m_pFindData->attrib & _A_SUBDIR ? true : false);
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is a hidden file
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsHidden() const
{
	return (m_pFindData->attrib & _A_HIDDEN ? true : false);
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is read-only
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsReadOnly() const
{
	return (m_pFindData->attrib & _A_RDONLY ? true : false);
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is marked as a system file
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsSystem() const
{
	return (m_pFindData->attrib & _A_SYSTEM ? true : false);
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is marked for archiving
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsMarkedForArchive() const
{
	return (m_pFindData->attrib & _A_ARCH ? true : false);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFileWriter::CFileWriter( bool bAsync ) 
{ 
#ifdef ASYNC_FILEIO
    m_bDefaultAsync = bAsync;
#else
    m_bDefaultAsync = false;
#endif
    m_hFileDest = INVALID_HANDLE_VALUE; 
    m_bAsync = m_bDefaultAsync; 
	m_cPendingCallbacksFromOtherThreads = 0;
    m_cubOutstanding = 0;
    m_cubWritten = 0;
    m_unThreadID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFileWriter::~CFileWriter() 
{ 
    Close(); 
}


#ifdef ASYNC_FILEIO
// our own version of overlapped structure passed through async writes
struct FileWriterOverlapped_t : public aiocb
{
    CFileWriter *m_pFileWriter;
    void *m_pvData;
    size_t m_cubData;
};
#endif


//-----------------------------------------------------------------------------
// Purpose: sets which file to write to
//-----------------------------------------------------------------------------
bool CFileWriter::BSetFile( const char *pchFile, bool bAllowOpenExisting )
{
    CPathString strPath( pchFile );

    // make sure the full path to file exists
    CUtlString strCopyUTF8 = strPath.GetUTF8Path();
    Q_StripFilename( const_cast<char*>(strCopyUTF8.Access()) );
    CreateDirRecursive( strCopyUTF8.Access() );
    
    Close();
    m_bAsync = m_bDefaultAsync;
    m_cubWritten = 0;
	m_cubOutstanding = 0;
	m_unThreadID = 0;
	m_cPendingCallbacksFromOtherThreads = 0;


    int flags = O_WRONLY;
    if ( bAllowOpenExisting )
        flags |= O_CREAT;
    else
        flags |= O_CREAT | O_TRUNC;

    m_hFileDest = (HANDLE)open( strPath.GetUTF8Path(), flags, S_IRWXU );
    if ( bAllowOpenExisting )
    {
        off_t offset = lseek( (intptr_t)m_hFileDest, 0, SEEK_END );
        m_cubWritten = offset;
    }
    
    m_unThreadID = ThreadGetCurrentId();
    return ( m_hFileDest != INVALID_HANDLE_VALUE );
}


void CFileWriter::Sleep( uint nMSec )
{
    if ( nMSec == 0 )
        sched_yield();
    else 
        usleep( nMSec * 1000 );
}

//-----------------------------------------------------------------------------
// Purpose: Seeks to a specific location in the file
//-----------------------------------------------------------------------------
bool CFileWriter::Seek( uint64 offset, ESeekOrigin eOrigin )
{
    if ( m_bAsync )
    {
        AssertMsg( false, "Seeking to a position not supported with async io" );
        return false;
    }

    bool bSuccess = false;

    int orgin = SEEK_SET;
    switch( eOrigin )
    {
    case k_ESeekCur:
        orgin = SEEK_CUR;
        break;
    case k_ESeekEnd:
        orgin = SEEK_END;
        break;
    default:
        orgin = SEEK_SET;
    }

    // fseeko will work on 64 bit file offsets if _FILE_OFFSET_BITS 64 is defined, is this the best way
    // to do this on posix builds?
    bSuccess = lseek( (intptr_t)m_hFileDest, (off_t)offset, orgin ) != -1;

    return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: posts a buffer to be written to the file
//-----------------------------------------------------------------------------
bool CFileWriter::Write( const void *pvData, uint32 cubData )
{
    if  ( cubData == 0 )
        return true;

    BOOL bRet = 0;
#ifdef ASYNC_FILEIO
    if ( m_bAsync )
    {
		// get any outstanding write callbacks
		if ( m_cubOutstanding > 0 )
		{
			::SleepEx( 0, TRUE );
		}

		// make sure we don't have too much data outstanding
		while ( m_cubOutstanding > (10*k_nMegabyte) )
		{
			::SleepEx( 10, TRUE );
		}
	
        // build the overlapped info that will get passed through the write
        FileWriterOverlapped_t *pFileWriterOverlapped = new FileWriterOverlapped_t;
        memset( pFileWriterOverlapped, 0x0, sizeof(FileWriterOverlapped_t) );
        pFileWriterOverlapped->m_pFileWriter = this;
        pFileWriterOverlapped->m_pvData = PvAlloc( cubData );
        pFileWriterOverlapped->m_cubData = cubData;
        memcpy( pFileWriterOverlapped->m_pvData, pvData, cubData );

        // work out where to write to
        pFileWriterOverlapped->aio_offset = m_cubWritten;
        pFileWriterOverlapped->aio_buf = pFileWriterOverlapped->m_pvData;
        pFileWriterOverlapped->aio_nbytes = pFileWriterOverlapped->m_cubData;
        pFileWriterOverlapped->aio_fildes = (int)m_hFileDest;
    
        /* Link the AIO request with a thread callback */
        pFileWriterOverlapped->aio_sigevent.sigev_notify = SIGEV_THREAD;
        pFileWriterOverlapped->aio_sigevent.sigev_notify_function = &CFileWriter::ThreadedWriteFileCompletionFunc;
        pFileWriterOverlapped->aio_sigevent.sigev_notify_attributes = nullptr;
        pFileWriterOverlapped->aio_sigevent.sigev_value.sival_ptr = pFileWriterOverlapped;
                 

      
        bRet = aio_write( pFileWriterOverlapped );
        bRet = !bRet; // aio_read returns 0 on success, this func returns success if bRet != 0
        if ( bRet )
		{
			ThreadInterlockedExchangeAdd( &m_cubOutstanding, cubData );
		
			if ( ThreadGetCurrentId() != m_unThreadID  )
			{
				// this is not the main thread so we have to wait here 
				ThreadInterlockedIncrement( &m_cPendingCallbacksFromOtherThreads );

				while ( m_cPendingCallbacksFromOtherThreads )
				{
					// we have to wait here since the OS can signal us only
					// on this current thread
					::SleepEx( 10, TRUE );
				}
			}
		}
    }
    else
#endif // ASYNC_FILEIO
    {
        bRet = write( (intptr_t)m_hFileDest, pvData, cubData );
    }

    // increment
    m_cubWritten += cubData;

    return ( bRet != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Convenient printf with no dynamic memory allocation
//-----------------------------------------------------------------------------
int CFileWriter::Printf( char *pDest, int bufferLen, char const *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
	// _vsnprintf will not write a terminator if the output string uses the entire buffer you provide
	int len = _vsnprintf( pDest, bufferLen-1, pFormat, marker );
	va_end( marker );

	// Len < 0 represents an overflow on windows; len > buffer length on posix
	if (( len < 0 ) || (len >= bufferLen ) )
	{
		len = bufferLen-1;
	}
	pDest[len] = 0;

	if ( !Write( pDest, len	) )
		return 0;

	return len;
}


//-----------------------------------------------------------------------------
// Purpose: ensures any writes have been completed
//-----------------------------------------------------------------------------
void CFileWriter::Flush()
{

    if ( m_unThreadID == ThreadGetCurrentId() )
	{
		// wait for all writes to be complete
		int cWaits = 0;
		const int k_nMaxWaits = 60000; /* roughly one minute */

		while ( m_cubOutstanding && cWaits < k_nMaxWaits   )
		{
			Sleep( 10 );
			cWaits++;
		}
		AssertMsg1( cWaits < k_nMaxWaits, "Waited 60k iterations in CFileWriter::Flush - m_cubOutstanding = %u", m_cubOutstanding );
	}
}


//-----------------------------------------------------------------------------
// Purpose: check if file is open
//-----------------------------------------------------------------------------
bool CFileWriter::BFileOpen()
{
    if ( m_hFileDest != INVALID_HANDLE_VALUE )
        return true;

    return false;
}


//-----------------------------------------------------------------------------
// Purpose: closes the file
//-----------------------------------------------------------------------------
void CFileWriter::Close()
{
    if ( m_hFileDest != INVALID_HANDLE_VALUE )
    {
		Flush();

		// temp handle to avoid double close in threaded environment
		HANDLE hFileDest = m_hFileDest;
       	m_hFileDest = INVALID_HANDLE_VALUE; 
        close( (intptr_t)hFileDest );
    }

	// Close has to be called from thread that called BSetFile
	Assert( m_cPendingCallbacksFromOtherThreads == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: async callback for when a file write has completed
//-----------------------------------------------------------------------------
#ifdef ASYNC_FILEIO
void CFileWriter::ThreadedWriteFileCompletionFunc( sigval sigval )
{
	FileWriterOverlapped_t *pFileWriterOverlapped = (FileWriterOverlapped_t *)sigval.sival_ptr;
	if ( aio_error( pFileWriterOverlapped ) == 0 ) 
	{
		uint nBytesWrite = aio_return( pFileWriterOverlapped );
		Assert( nBytesWrite == pFileWriterOverlapped->m_cubData );

		pFileWriterOverlapped->m_pFileWriter->m_cOutstandingWrites--;
		pFileWriterOverlapped->m_pFileWriter->m_cubOutstanding += ( 0 - pFileWriterOverlapped->m_cubData );
		FreePv( pFileWriterOverlapped->m_pvData );
		delete pFileWriterOverlapped;
	}
}
#endif // ASYNC_FILEIO



// a buffer full of file names
static const int k_cubDirWatchBufferSize = 8 * 1024;


//-----------------------------------------------------------------------------
// Purpose: directory watching
//-----------------------------------------------------------------------------
CDirWatcher::CDirWatcher()
{
	m_hFile = nullptr;
	m_pOverlapped = nullptr;
	m_pFileInfo = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: directory watching
//-----------------------------------------------------------------------------
CDirWatcher::~CDirWatcher()
{
	if ( m_pFileInfo )
	{
		free( m_pFileInfo );
	}
	if ( m_pOverlapped )
	{
		free( m_pOverlapped );
	}
}



//-----------------------------------------------------------------------------
// Purpose: only one directory can be watched at a time
//-----------------------------------------------------------------------------
void CDirWatcher::SetDirToWatch( const char *pchDir )
{
	if ( !pchDir || !*pchDir )
		return;
	
	CPathString strPath( pchDir );
	Assert( !"Impl me" );
}




//-----------------------------------------------------------------------------
// Purpose: used by callback functions to push a file onto the list
//-----------------------------------------------------------------------------
void CDirWatcher::AddFileToChangeList( const char *pchFile )
{
	// make sure it isn't already in the list
	FOR_EACH_LL( m_listChangedFiles, i )
	{
		if ( !Q_stricmp( m_listChangedFiles[i], pchFile ) )
			return;
	}

	m_listChangedFiles.AddToTail( pchFile );
}


//-----------------------------------------------------------------------------
// Purpose: retrieve any changes
//-----------------------------------------------------------------------------
bool CDirWatcher::GetChangedFile( CUtlString *psFile )
{

	if ( !m_listChangedFiles.Count() )
		return false;

	*psFile = m_listChangedFiles[m_listChangedFiles.Head()];
	m_listChangedFiles.Remove( m_listChangedFiles.Head() );
	return true;
}



#ifdef DBGFLAG_VALIDATE
void CDirWatcher::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	validator.ClaimMemory( m_pOverlapped );
	validator.ClaimMemory( m_pFileInfo );
	ValidateObj( m_listChangedFiles );
	FOR_EACH_LL( m_listChangedFiles, i )
	{
		ValidateObj( m_listChangedFiles[i] );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: utility function to create dirs & subdirs
//-----------------------------------------------------------------------------
bool CreateDirRecursive( const char *pchPathIn )
{
	CPathString strPath( pchPathIn );

	// Cast away const, we're going to modify in place even though that's kind of evil
	char *path = (char *)strPath.GetUTF8Path();

	// Does it already exist?
	if ( BFileExists( path ) )
		return true;

	// Walk backwards to first non-existing dir that we find
	char *s = path + Q_strlen(path) - 1;

	while ( s > path )
	{
		if ( *s == CORRECT_PATH_SEPARATOR )
		{
			*s = '\0';
			bool bExists = BFileExists( path );
			*s = CORRECT_PATH_SEPARATOR;

			if ( bExists )
			{
				++s;
				break;
			}
		}
		--s;
	}

	// and then move forwards from there

	while ( *s )
	{
		if ( *s == CORRECT_PATH_SEPARATOR )
		{
			*s = '\0';
			BCreateDirectory( path );
			*s = CORRECT_PATH_SEPARATOR;
		}
		s++;
	}

	if ( !BCreateDirectory( path )  )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the directory, returning true if it is created, or if it already existed
//-----------------------------------------------------------------------------
bool BCreateDirectory( const char *path )
{
	CPathString pathStr( path );
	int i = mkdir( pathStr.GetUTF8Path(), S_IRWXU | S_IRWXG | S_IRWXO );
	if ( i == 0 )
		return true;
	if ( errno == EEXIST )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: make a file writable
//-----------------------------------------------------------------------------
bool MakeFileWriteable( const char *pszFileNameIn )
{
	CPathString strPath( pszFileNameIn );
#if defined( WIN32_FILEIO )
	DWORD dwFileAttributes = ::GetFileAttributesW( strPath.GetWCharPathPrePended() );

	if (dwFileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		// remove flags that make it read only, if necessary
		if (dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
		{
			dwFileAttributes &= ~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);
			::SetFileAttributesW( strPath.GetWCharPathPrePended(), dwFileAttributes );
		}
		return true;
	}
	return false;
#else
	statBig_t statBuf;
	if (statBig( strPath.GetUTF8Path(), &statBuf ) != 0)
		return false;
	if (statBuf.st_mode & _S_IWRITE)
		return true;
	int ret = chmod( strPath.GetUTF8Path(), statBuf.st_mode | _S_IWRITE );
	return (ret == 0);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: deletes a file
//-----------------------------------------------------------------------------
bool UnlinkFile( const char *pchFileIn )
{
	CPathString strPath( pchFileIn );
	return (0 == _unlink( strPath.GetUTF8Path() ));
}

//-----------------------------------------------------------------------------
// Purpose: Checks if a file exists
// Input:   pchFileName - file name to check existence of (UTF8 - unqualified, relative, or fully qualified)
// Output:  true if successful (file did not exist, or it existed and was deleted);
//          false if unsuccessful (file existed but could not be deleted)
//-----------------------------------------------------------------------------
bool BFileExists( const char *pchFileNameIn )
{
	CPathString strPath( pchFileNameIn );

#if defined( WIN32_FILEIO )
	// Checking file attributes is fastest way to determine existence
	return (INVALID_FILE_ATTRIBUTES != ::GetFileAttributesW( strPath.GetWCharPathPrePended() ));
#else
	statBig_t buf;
	return (0 == statBig( strPath.GetUTF8Path(), &buf ));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Deletes a file if it exists.  If the file is read-only, will attempt
//          to change file attributes and delete it.
// Input:   pchFileName - file name to delete (unqualified, relative, or fully qualified)
// Output:  true if successful (file did not exist, or it existed and was deleted);
//          false if unsuccessful (file existed but could not be deleted)
//-----------------------------------------------------------------------------
bool BDeleteFileIfExists( const char * pchFileName )
{
	// vast majority don't need to be touched/tested to delete them, so don't
	// take the penalty in the common case.
	if (UnlinkFile( pchFileName ))
		return true;

	if (BFileExists( pchFileName ))
	{
		MakeFileWriteable( pchFileName );
		return UnlinkFile( pchFileName );
	}
	else
	{
		return true; // doesn't exist
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes an empty directory that works on multiple platforms.
//-----------------------------------------------------------------------------
bool BRemoveDirectory( const char *pchPathIn )
{
	MakeFileWriteable( pchPathIn );

	CPathString strPath( pchPathIn );
#if defined( WIN32_FILEIO )
	if (::RemoveDirectoryW( strPath.GetWCharPathPrePended() ))
		return true;
	return false;
#else
	return _rmdir( pchPathIn ) == 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Removes a directory and all subdirectories and all files in those directories
//-----------------------------------------------------------------------------
bool BRemoveDirectoryRecursive( const char *pchPathIn )
{
	CDirIterator dirIter( pchPathIn, "*" );

	while (dirIter.BNextFile())
	{
		uint32 unLenPath = Q_strlen( pchPathIn ) + Q_strlen( dirIter.CurrentFileName() ) + 2;
		char *pchPath = new char[unLenPath];
		Q_snprintf( pchPath, unLenPath, "%s%c%s", pchPathIn, CORRECT_PATH_SEPARATOR, dirIter.CurrentFileName() );

		if (dirIter.BCurrentIsDir())
		{
			BRemoveDirectoryRecursive( pchPath );
		}
		else
		{
			// Shouldn't have files in the root dir, delete them if found
			BDeleteFileIfExists( pchPath );
		}
		delete[] pchPath;
	}

	return BRemoveDirectory( pchPathIn );
}


// findfirst/findnext implementation from filesystem/linux_support.[h|cpp]
// modified a bit for PS3


static char selectBuf[PATH_MAX];

static int FileSelect( const direntBig_t *ent )
{
	const char *mask = selectBuf;
	const char *name = ent->d_name;

	return FileSelect( name, mask );
}


static int FileSelect( const char *name, const char *mask )
{
	//printf("Test:%s %s\n",mask,name);

	if (!strcmp( name, "." ) || !strcmp( name, ".." )) return 0;

	if (!strcmp( mask, "*.*" ) || !strcmp( mask, "*" )) return 1;

	while (*mask && *name)
	{
		if (*mask == '*')
		{
			mask++; // move to the next char in the mask
			if (!*mask) // if this is the end of the mask its a match 
			{
				return 1;
			}
			while (*name && toupper( *name ) != toupper( *mask ))
			{ // while the two don't meet up again
				name++;
			}
			if (!*name)
			{ // end of the name
				break;
			}
		}
		else if (*mask != '?')
		{
			if (toupper( *mask ) != toupper( *name ))
			{   // mismatched!
				return 0;
			}
			else
			{
				mask++;
				name++;
				if (!*mask && !*name)
				{ // if its at the end of the buffer
					return 1;
				}

			}

		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}

	return(!*mask && !*name); // both of the strings are at the end
}


int FillDataStruct( _finddata_t *dat )
{
	statBig_t fileStat;

	if (dat->curName >= dat->numNames)
		return -1;

	Q_strncpy( dat->name, dat->namelist[dat->curName]->d_name, sizeof(dat->name) );
	char szFullPath[MAX_PATH];
	Q_snprintf( szFullPath, sizeof(szFullPath), "%s%c%s", dat->dirBase, CORRECT_PATH_SEPARATOR, dat->name );
	if (statBig( szFullPath, &fileStat ) == 0)
	{
		dat->attrib = fileStat.st_mode;
		dat->size = fileStat.st_size;
		dat->time_write = fileStat.st_mtime;
		dat->time_create = fileStat.st_ctime;
	}
	else
	{
		dat->attrib = 0;
		dat->size = 0;
		dat->time_write = 0;
		dat->time_create = 0;
	}
	free( dat->namelist[dat->curName] );
	dat->namelist[dat->curName] = nullptr;
	dat->curName++;
	return 1;
}

int _findfirst( const char *fileName, _finddata_t *dat )
{
	char nameStore[PATH_MAX];
	char *dir = nullptr;
	int n, iret = -1;

	Q_strncpy( nameStore, fileName, sizeof(nameStore) );

	if (strrchr( nameStore, '/' ))
	{
		dir = nameStore;
		while (strrchr( dir, '/' ))
		{
			statBig_t dirChk;

			// zero this with the dir name
			dir = strrchr( nameStore, '/' );
			*dir = '\0';
			if (dir == nameStore)
			{
				dir = "/";
			}
			else
			{
				dir = nameStore;
			}

			if (statBig( dir, &dirChk ) == 0 && S_ISDIR( dirChk.st_mode ))
			{
				break;
			}
		}
	}
	else
	{
		// couldn't find a dir separator...
		return -1;
	}

	if (strlen( dir ) > 0)
	{
		if (strlen( dir ) == 1)
			Q_strncpy( selectBuf, fileName + 1, sizeof(selectBuf) );
		else
			Q_strncpy( selectBuf, fileName + strlen( dir ) + 1, sizeof(selectBuf) );

		n = scandirBig( dir, &dat->namelist, FileSelect, alphasortBig );
		if (n < 0)
		{
			// silently return, nothing interesting
		}
		else
		{
			dat->curName = 0;
			dat->numNames = n; // n is the number of matches
			Q_strncpy( dat->dirBase, dir, sizeof(dat->dirBase) );
			iret = FillDataStruct( dat );
			if (iret < 0)
			{
				free( dat->namelist );
				dat->namelist = nullptr;
				dat->curName = 0;
				dat->numNames = 0;
			}
		}
	}

	//  printf("Returning: %i \n",iret);
	return iret;
}

int _findnext( int64 handle, _finddata_t *dat )
{
	if (dat->curName >= dat->numNames)
	{
		free( dat->namelist );
		dat->namelist = nullptr;
		dat->curName = 0;
		dat->numNames = 0;
		return -1; // no matches left
	}

	FillDataStruct( dat );
	return 0;
}

bool _findclose( int64 handle )
{
	return true;
}


