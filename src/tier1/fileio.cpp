// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: A collection of utility classes to simplify file I/O, and
//			as much as possible contain portability problems. Here avoiding
//			including windows.h.



import std;
import ks.core.basetypes;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.appinstance;
import ks.tier1.fileio;
import ks.tier1.keyvalues;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.vstdlib.vstrtools;

// Linux hasn't got a good AIO library that we have found yet, so lets punt for now


//lwss - Add tier0/platform.h to get this to build. Should be ok since it is header-only.
//lwss end


inline constexpr int k_FileAttributeDirectory = 0x4000;

struct _finddata_t
{   
	_finddata_t()
	{
		name[0] = '\0';
		dirBase[0] = '\0';
		size = 0;
		attrib = 0;
		time_write = 0;
		time_create = 0;
		curName = 0;
		numNames = 0;
		listing = nullptr;
	}
	// public data
	char name[k_cMaxPath]; // the file name returned from the call
	char dirBase[k_cMaxPath];
	std::int64_t size;
	int attrib;
	std::time_t time_write;
	std::time_t time_create;
	int curName;
	int numNames;
	ks::platform::directory_listing *listing;
};


// FUTURE map _A_HIDDEN via checking filename against .*
inline constexpr int _A_HIDDEN = 0;

// FUTURE check 'read only' by checking mode against S_IRUSR
inline constexpr int _A_RDONLY = 0;

// no files under posix are 'system' or 'archive'
inline constexpr int _A_SYSTEM = 0;
inline constexpr int _A_ARCH = 0;

int _findfirst( const char *pchBasePath, struct _finddata_t *pFindData );
int _findnext( const int64 hFind, struct _finddata_t *pFindData );
bool _findclose( int64 hFind );
static int FileSelect( const char *name, const char *mask );



template <typename T1, typename T2> [[gnu::always_inline]] inline auto PvRealloc( T1 pv, T2 cub ) { return realloc( pv, cub ); }

//-----------------------------------------------------------------------------
// Purpose: Constructor from UTF8
//-----------------------------------------------------------------------------
CPathString::CPathString( const char *pchUTF8Path )
{
	// Need to first turn into an absolute path, so \\?\ pre-pended paths will be ok
	m_pchUTF8Path = new char[ MAX_UNICODE_PATH_IN_UTF8 ];
	m_pwchWideCharPathPrepended = nullptr;

	// First, convert to absolute path, which also does V_FixSlashes for us.
	V_MakeAbsolutePath( m_pchUTF8Path, MAX_UNICODE_PATH * 4, pchUTF8Path );

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
		V_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\UNC\\", 8*sizeof(wchar_t) );
			V_UTF8ToUnicode( m_pchUTF8Path+2, m_pwchWideCharPathPrepended+8, MAX_UNICODE_PATH*sizeof(wchar_t) );

		// Be sure we NULL terminate within our allocated region incase V_UTF8ToUnicode failed, though we're already in bad shape then.
		m_pwchWideCharPathPrepended[MAX_UNICODE_PATH+7] = 0;
	}
	else
	{
		m_pwchWideCharPathPrepended = new wchar_t[MAX_UNICODE_PATH+4];
		V_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\", 4*sizeof(wchar_t) );
			V_UTF8ToUnicode( m_pchUTF8Path, m_pwchWideCharPathPrepended+4, MAX_UNICODE_PATH*sizeof(wchar_t) );

		// Be sure we NULL terminate within our allocated region incase V_UTF8ToUnicode failed, though we're already in bad shape then.
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
	char *pchPathAndPattern = new char[V_strlen( strPath.GetUTF8Path() ) + V_strlen( pchPattern ) + 2];

	// be resilient about whether the caller passes us a path with a terminal path separator or not.

	// put in the path
	if (pchPath)
	{
		V_strncpy( pchPathAndPattern, strPath.GetUTF8Path(), V_strlen( strPath.GetUTF8Path() ) + 1 );

		// identify whether we've got a terminal separator. add one if not.
		char *pchRest = pchPathAndPattern + V_strlen( pchPathAndPattern ) - 1;
		if (*pchRest != CORRECT_PATH_SEPARATOR)
		{
			*++pchRest = CORRECT_PATH_SEPARATOR;
		}
		pchRest++;

		// now put in the search pattern.
		V_strncpy( pchRest, pchPattern, V_strlen( pchPattern ) + 1 );

		Init( pchPathAndPattern );
	}
	else
	{
		pchPathAndPattern[0] = 0;
		m_bNoFiles = true;
		m_bUsedFirstFile = true;

		m_hFind = -1;
		m_pFindData = new _finddata_t;
		std::memset( m_pFindData, 0, sizeof(*m_pFindData) );

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
	std::memset( m_pFindData, 0, sizeof(*m_pFindData) );

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
		ks::platform::free_listing( m_pFindData->listing );
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
	return (m_pFindData->attrib & k_FileAttributeDirectory ? true : false);
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
    m_bDefaultAsync = false;
    m_hFileDest = nullptr; 
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




//-----------------------------------------------------------------------------
// Purpose: sets which file to write to
//-----------------------------------------------------------------------------
bool CFileWriter::BSetFile( const char *pchFile, bool bAllowOpenExisting )
{
    CPathString strPath( pchFile );

    // make sure the full path to file exists
    std::string strCopyUTF8 = ks::nz( strPath.GetUTF8Path() );
    V_StripFilename( const_cast<char*>(strCopyUTF8.data()) );
    CreateDirRecursive( strCopyUTF8.data() );
    
    Close();
    m_bAsync = m_bDefaultAsync;
    m_cubWritten = 0;
	m_cubOutstanding = 0;
	m_unThreadID = 0;
	m_cPendingCallbacksFromOtherThreads = 0;


    ks::platform::file *pDest = ks::platform::open_file( strPath.GetUTF8Path(), bAllowOpenExisting ? ks::platform::file_mode::write_keep : ks::platform::file_mode::write, nullptr );
    m_hFileDest = pDest ? (HANDLE)pDest : nullptr;
    if ( pDest && bAllowOpenExisting )
    {
        long long offset = ks::platform::seek_file( pDest, 0, k_nSeekEnd, nullptr );
        m_cubWritten = offset;
    }
    
    m_unThreadID = ThreadGetCurrentId();
    return ( m_hFileDest != nullptr );
}


void CFileWriter::Sleep( uint nMSec )
{
    if ( nMSec == 0 )
        ks::platform::thread_yield();
    else 
        ks::platform::sleep_nanos( (unsigned long long)nMSec * 1000000ull );
}

//-----------------------------------------------------------------------------
// Purpose: Seeks to a specific location in the file
//-----------------------------------------------------------------------------
bool CFileWriter::Seek( uint64 offset, ESeekOrigin eOrigin )
{
    if ( m_bAsync )
    {
        contract_assert( false && "Seeking to a position not supported with async io" );
        return false;
    }

    bool bSuccess = false;

    int orgin = k_nSeekSet;
    switch( eOrigin )
    {
    case k_ESeekCur:
        orgin = k_nSeekCur;
        break;
    case k_ESeekEnd:
        orgin = k_nSeekEnd;
        break;
    default:
        orgin = k_nSeekSet;
    }

    // fseeko will work on 64 bit file offsets if _FILE_OFFSET_BITS 64 is defined, is this the best way
    // to do this on posix builds?
    bSuccess = ks::platform::seek_file( (ks::platform::file *)m_hFileDest, (long long)offset, orgin, nullptr ) != -1;

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
    {
        bRet = ks::platform::write_file( (ks::platform::file *)m_hFileDest, pvData, cubData, nullptr ) >= 0;
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
	std::va_list marker;

	__builtin_va_start( marker, pFormat );
	// _vsnprintf will not write a terminator if the output string uses the entire buffer you provide
	int len = std::vsnprintf( pDest, bufferLen-1, pFormat, marker );
	__builtin_va_end( marker );

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
		contract_assert( ( cWaits < k_nMaxWaits ) && "Waited 60k iterations in CFileWriter::Flush - m_cubOutstanding = %u" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: check if file is open
//-----------------------------------------------------------------------------
bool CFileWriter::BFileOpen()
{
    if ( m_hFileDest != nullptr )
        return true;

    return false;
}


//-----------------------------------------------------------------------------
// Purpose: closes the file
//-----------------------------------------------------------------------------
void CFileWriter::Close()
{
    if ( m_hFileDest != nullptr )
    {
		Flush();

		// temp handle to avoid double close in threaded environment
		HANDLE hFileDest = m_hFileDest;
       	m_hFileDest = nullptr; 
        ks::platform::close_file( (ks::platform::file *)hFileDest );
    }

	// Close has to be called from thread that called BSetFile
	contract_assert( m_cPendingCallbacksFromOtherThreads == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: async callback for when a file write has completed
//-----------------------------------------------------------------------------



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
		std::free( m_pFileInfo );
	}
	if ( m_pOverlapped )
	{
		std::free( m_pOverlapped );
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
	contract_assert( !"Impl me" );
}




//-----------------------------------------------------------------------------
// Purpose: used by callback functions to push a file onto the list
//-----------------------------------------------------------------------------
void CDirWatcher::AddFileToChangeList( const char *pchFile )
{
	// make sure it isn't already in the list
	for ( const std::string &sFile : m_listChangedFiles )
	{
		if ( !V_stricmp( sFile.c_str(), pchFile ) )
			return;
	}

	m_listChangedFiles.push_back( ks::nz( pchFile ) );
}


//-----------------------------------------------------------------------------
// Purpose: retrieve any changes
//-----------------------------------------------------------------------------
bool CDirWatcher::GetChangedFile( std::string *psFile )
{

	if ( m_listChangedFiles.empty() )
		return false;

	*psFile = std::move( m_listChangedFiles.front() );
	m_listChangedFiles.pop_front();
	return true;
}



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
	char *s = path + V_strlen(path) - 1;

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
	if ( ks::platform::create_directory( pathStr.GetUTF8Path() ) )
		return true;

	return ks::platform::path_exists( pathStr.GetUTF8Path() );
}

//-----------------------------------------------------------------------------
// Purpose: make a file writable
//-----------------------------------------------------------------------------
bool MakeFileWriteable( const char *pszFileNameIn )
{
	CPathString strPath( pszFileNameIn );
	return ks::platform::set_path_writable( strPath.GetUTF8Path(), true );
}

//-----------------------------------------------------------------------------
// Purpose: deletes a file
//-----------------------------------------------------------------------------
bool UnlinkFile( const char *pchFileIn )
{
	CPathString strPath( pchFileIn );
	return ks::platform::remove_file( strPath.GetUTF8Path(), nullptr );
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

	return ks::platform::path_exists( strPath.GetUTF8Path() );
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
	return ks::platform::remove_directory( strPath.GetUTF8Path() );
}

//-----------------------------------------------------------------------------
// Purpose: Removes a directory and all subdirectories and all files in those directories
//-----------------------------------------------------------------------------
bool BRemoveDirectoryRecursive( const char *pchPathIn )
{
	CDirIterator dirIter( pchPathIn, "*" );

	while (dirIter.BNextFile())
	{
		uint32 unLenPath = V_strlen( pchPathIn ) + V_strlen( dirIter.CurrentFileName() ) + 2;
		char *pchPath = new char[unLenPath];
		V_snprintf( pchPath, unLenPath, "%s%c%s", pchPathIn, CORRECT_PATH_SEPARATOR, dirIter.CurrentFileName() );

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


static int FillDataStruct( _finddata_t *dat )
{
	ks::platform::directory_entry entry;
	if (dat->curName >= dat->numNames || !ks::platform::listing_entry( dat->listing, (unsigned long)dat->curName, &entry ))
		return -1;

	V_strncpy( dat->name, entry.name, sizeof(dat->name) );
	dat->attrib = entry.info.is_directory ? k_FileAttributeDirectory : 0;
	dat->size = entry.info.size;
	dat->time_write = (std::time_t)entry.info.modified_seconds;
	dat->time_create = (std::time_t)entry.info.created_seconds;
	dat->curName++;
	return 1;
}

int _findfirst( const char *fileName, _finddata_t *dat )
{
	char nameStore[k_cMaxPath];
	char *dir = nullptr;
	int iret = -1;

	V_strncpy( nameStore, fileName, sizeof(nameStore) );

	if (std::strrchr( nameStore, '/' ))
	{
		dir = nameStore;
		while (std::strrchr( dir, '/' ))
		{
			// zero this with the dir name
			dir = std::strrchr( nameStore, '/' );
			*dir = '\0';
			if (dir == nameStore)
			{
				dir = (char *)"/";
			}
			else
			{
				dir = nameStore;
			}

			ks::platform::file_info dirCheck{};
			if (ks::platform::stat_path( dir, &dirCheck ) && dirCheck.is_directory)
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

	if (std::strlen( dir ) > 0)
	{
		const char *mask = (std::strlen( dir ) == 1) ? fileName + 1 : fileName + std::strlen( dir ) + 1;

		dat->listing = ks::platform::list_directory( dir, mask );
		if (dat->listing != nullptr)
		{
			dat->curName = 0;
			dat->numNames = (int)ks::platform::listing_count( dat->listing );
			V_strncpy( dat->dirBase, dir, sizeof(dat->dirBase) );
			iret = FillDataStruct( dat );
			if (iret < 0)
			{
				ks::platform::free_listing( dat->listing );
				dat->listing = nullptr;
				dat->curName = 0;
				dat->numNames = 0;
			}
		}
	}

	return iret;
}

int _findnext( int64 handle, _finddata_t *dat )
{
	if (dat->curName >= dat->numNames)
	{
		ks::platform::free_listing( dat->listing );
		dat->listing = nullptr;
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
