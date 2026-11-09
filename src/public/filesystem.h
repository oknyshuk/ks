#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;

import ks.tier1;

#include "tier2/tier2.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

class IFileList;

typedef void * FileHandle_t;
typedef int FileFindHandle_t;
typedef void (*FileSystemLoggingFunc_t)( const char *fileName, const char *accessType );
typedef int WaitForResourcesHandle_t;


//-----------------------------------------------------------------------------
// Enums used by the interface
//-----------------------------------------------------------------------------

inline constexpr auto FILESYSTEM_MAX_SEARCH_PATHS = 128;

enum FileSystemSeek_t
{
	FILESYSTEM_SEEK_HEAD	= 0,
	FILESYSTEM_SEEK_CURRENT = 1,
	FILESYSTEM_SEEK_TAIL	= 2,
};

enum
{
	FILESYSTEM_INVALID_FIND_HANDLE = -1
};

enum FileWarningLevel_t
{
	// A problem!
	FILESYSTEM_WARNING = -1,

	// Don't print anything
	FILESYSTEM_WARNING_QUIET = 0,

	// On shutdown, report names of files left unclosed
	FILESYSTEM_WARNING_REPORTUNCLOSED,

	// Report number of times a file was opened, closed
	FILESYSTEM_WARNING_REPORTUSAGE,

	// Report all open/close events to console ( !slow! )
	FILESYSTEM_WARNING_REPORTALLACCESSES,

	// Report all open/close/read events to the console ( !slower! )
	FILESYSTEM_WARNING_REPORTALLACCESSES_READ,

	// Report all open/close/read/write events to the console ( !slower! )
	FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE,

	// Report all open/close/read/write events and all async I/O file events to the console ( !slower(est)! )
	FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC,

};

// search path filtering
enum PathTypeFilter_t
{
	FILTER_NONE               = 0,	// no filtering, all search path types match
	FILTER_CULLPACK           = 1,	// pack based search paths are culled (maps and zips)
	FILTER_CULLNONPACK        = 2,	// non-pack based search paths are culled
	FILTER_CULLLOCALIZED      = 3,	// Ignore localized paths, assumes CULLNONPACK
	FILTER_CULLLOCALIZED_ANY  = 4,	// Ignore any localized paths
};

// search path querying (bit flags)
enum
{
	PATH_IS_NORMAL      = 0x00, // normal path, not pack based
	PATH_IS_PACKFILE    = 0x01, // path is a pack file
	PATH_IS_MAPPACKFILE = 0x02, // path is a map pack file
	PATH_IS_DVDDEV		= 0x04, // path is the dvddev cache
};
typedef uint32 PathTypeQuery_t;

template <typename T1> [[gnu::always_inline]] inline auto IS_PACKFILE( T1 n ) { return ( n & ( PATH_IS_PACKFILE | PATH_IS_MAPPACKFILE ) ); }
template <typename T1> [[gnu::always_inline]] inline auto IS_DVDDEV( T1 n ) { return ( n & PATH_IS_DVDDEV ); }


// In non-retail builds, enable the file blocking access tracking stuff...

enum SearchPathAdd_t
{
	PATH_ADD_TO_HEAD,			// First path searched
	PATH_ADD_TO_TAIL,			// Last path searched
	PATH_ADD_TO_TAIL_ATINDEX,	// First path searched
};

enum FilesystemOpenExFlags_t
{
	FSOPEN_UNBUFFERED		= (1 << 0),
	FSOPEN_FORCE_TRACK_CRC	= (1 << 1),		// This makes it calculate a CRC for the file (if the file came from disk) regardless 
											// of the IFileList passed to RegisterFileWhitelist.
	FSOPEN_NEVERINPACK	    = (1 << 2),		// 360 only, hint to FS that file is not allowed to be in pack file
};

#define FILESYSTEM_INVALID_HANDLE	( FileHandle_t )0

//-----------------------------------------------------------------------------
// Structures used by the interface
//-----------------------------------------------------------------------------

struct FileSystemStatistics
{
	std::atomic<unsigned>	nReads,
							nWrites,
							nBytesRead,
							nBytesWritten,
							nSeeks;
};

//-----------------------------------------------------------------------------
// File system allocation functions. Client must free on failure
//-----------------------------------------------------------------------------
typedef void *(*FSAllocFunc_t)( const char *pszFilename, unsigned nBytes );


//-----------------------------------------------------------------------------
// Used to display dirty disk error functions
//-----------------------------------------------------------------------------
typedef void (*FSDirtyDiskReportFunc_t)();


//-----------------------------------------------------------------------------
// Asynchronous support types
//-----------------------------------------------------------------------------
struct FSAsyncControl_t__ { int unused; };
using FSAsyncControl_t = FSAsyncControl_t__ *;
struct FSAsyncFile_t__ { int unused; };
using FSAsyncFile_t = FSAsyncFile_t__ *;
const FSAsyncFile_t FS_INVALID_ASYNC_FILE = (FSAsyncFile_t)(0x0000ffff);


//---------------------------------------------------------
// Async file status
//---------------------------------------------------------
enum FSAsyncStatus_t
{
	FSASYNC_ERR_ALIGNMENT    = -6,	// read parameters invalid for unbuffered IO
	FSASYNC_ERR_FAILURE      = -5,	// hard subsystem failure
	FSASYNC_ERR_READING      = -4,	// read error on file
	FSASYNC_ERR_NOMEMORY     = -3,	// out of memory for file read
	FSASYNC_ERR_UNKNOWNID    = -2,	// caller's provided id is not recognized
	FSASYNC_ERR_FILEOPEN     = -1,	// filename could not be opened (bad path, not exist, etc)
	FSASYNC_OK               = 0,	// operation is successful
	FSASYNC_STATUS_PENDING,			// file is properly queued, waiting for service
	FSASYNC_STATUS_INPROGRESS,		// file is being accessed
	FSASYNC_STATUS_ABORTED,			// file was aborted by caller
	FSASYNC_STATUS_UNSERVICED,		// file is not yet queued
};

//---------------------------------------------------------
// Async request flags
//---------------------------------------------------------
enum FSAsyncFlags_t
{
	FSASYNC_FLAGS_ALLOCNOFREE		= ( 1 << 0 ),	// do the allocation for dataPtr, but don't free
	FSASYNC_FLAGS_FREEDATAPTR		= ( 1 << 1 ),	// free the memory for the dataPtr post callback
	FSASYNC_FLAGS_SYNC				= ( 1 << 2 ),	// Actually perform the operation synchronously. Used to simplify client code paths
	FSASYNC_FLAGS_NULLTERMINATE		= ( 1 << 3 ),	// allocate an extra byte and null terminate the buffer read in
};

//---------------------------------------------------------
// Return value for CheckFileCRC.
//---------------------------------------------------------
enum EFileCRCStatus
{
	k_eFileCRCStatus_CantOpenFile,		// We don't have this file. 
	k_eFileCRCStatus_GotCRC,
	k_eFileCRCStatus_FileInVPK,
};

// Used in CacheFileCRCs.
enum ECacheCRCType
{
	k_eCacheCRCType_SingleFile,
	k_eCacheCRCType_Directory,
	k_eCacheCRCType_Directory_Recursive
};

//---------------------------------------------------------
// Optional completion callback for each async file serviced (or failed)
// call is not reentrant, async i/o guaranteed suspended until return
// Note: If you change the signature of the callback, you will have to account for it in FileSystemV12 (toml [4/18/2005] )
//---------------------------------------------------------
struct FileAsyncRequest_t;
typedef void (*FSAsyncCallbackFunc_t)(const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err);

//-----------------------------------------------------------------------------
// Used to add results from async directory scans
//-----------------------------------------------------------------------------
typedef void (*FSAsyncScanAddFunc_t)( void* pContext, char* pFoundPath, char* pFoundFile );
typedef void (*FSAsyncScanCompleteFunc_t)( void* pContext, FSAsyncStatus_t err );

//---------------------------------------------------------
// Description of an async request
//---------------------------------------------------------
struct FileAsyncRequest_t
{
	FileAsyncRequest_t()	{ std::memset( this, 0, sizeof(*this) ); hSpecificAsyncFile = FS_INVALID_ASYNC_FILE;	}
	const char *			pszFilename;		// file system name
	void *					pData;				// optional, system will alloc/free if NULL
	int						nOffset;			// optional initial seek_set, 0=beginning
	int						nBytes;				// optional read clamp, -1=exist test, 0=full read
	FSAsyncCallbackFunc_t	pfnCallback;		// optional completion callback
	void *					pContext;			// caller's unique file identifier
	int						priority;			// inter list priority, 0=lowest
	unsigned				flags;				// behavior modifier
	const char *			pszPathID;			// path ID (NOTE: this field is here to remain binary compatible with release HL2 filesystem interface)
	FSAsyncFile_t			hSpecificAsyncFile; // Optional hint obtained using AsyncBeginRead()
	FSAllocFunc_t			pfnAlloc;			// custom allocator. can be null. not compatible with FSASYNC_FLAGS_FREEDATAPTR
};


struct FileHash_t 
{
	enum EFileHashType_t
	{
		k_EFileHashTypeUnknown			= 0,
		k_EFileHashTypeEntireFile		= 1,
		k_EFileHashTypeIncompleteFile	= 2,
	};
	FileHash_t()
	{
		m_eFileHashType = FileHash_t::k_EFileHashTypeUnknown;
		m_cbFileLen = 0;
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
	}
	int m_eFileHashType;
	CRC32_t m_crcIOSequence;
	MD5Value_t m_md5contents;
	int m_cbFileLen;
	int m_PackFileID;
	int m_nPackFileNumber;

	bool operator==( const FileHash_t &src ) const
	{
		return m_crcIOSequence == src.m_crcIOSequence && 
			m_md5contents == src.m_md5contents && 
			m_eFileHashType == src.m_eFileHashType;
	}
	bool operator!=( const FileHash_t &src ) const
	{
		return m_crcIOSequence != src.m_crcIOSequence || 
			m_md5contents != src.m_md5contents || 
			m_eFileHashType != src.m_eFileHashType;
	}

};

class CUnverifiedFileHash
{
public:
	char m_PathID[MAX_PATH];
	char m_Filename[MAX_PATH];
	int m_nFileFraction;
	FileHash_t m_FileHash;
};

class CUnverifiedCRCFile
{
public:
	char m_PathID[MAX_PATH];
	char m_Filename[MAX_PATH];
	CRC32_t m_CRC;
};


// Spew flags for SetWhitelistSpewFlags (set with the fs_whitelist_spew_flags cvar).
// Update the comment for the fs_whitelist_spew_flags cvar if you change these.
inline constexpr auto WHITELIST_SPEW_WHILE_LOADING = 0x0001; // list files as they are added to the CRC tracker
inline constexpr auto WHITELIST_SPEW_RELOAD_FILES = 0x0002; // show files the filesystem is telling the engine to reload
inline constexpr auto WHITELIST_SPEW_DONT_RELOAD_FILES = 0x0004; // show files the filesystem is NOT telling the engine to reload



// DLC license mask flags is 32 publisher defined bits
// MSW 16 bits in 8.8: Type.SubVersion
// LSW 16 bits: Flags

// return id component
#define DLC_LICENSE_ID( x )				( ( ( (unsigned int)( x ) ) >> 24 ) & 0x000000FF )
// returns minor version component (not generally used, i.e. we dont rev dlc's yet)
#define DLC_LICENSE_MINORVERSION( x )	( ( ( (unsigned int)( x ) ) >> 16 ) & 0x000000FF )
// returns license flags
#define DLC_LICENSE_FLAGS( x )			( ( ( (unsigned int)( x ) ) & 0x0000FFFF ) )

inline constexpr auto DLCFLAGS_PRESENCE_ONLY = 0x0001; // causes no search path loadout



//-----------------------------------------------------------------------------
// Base file system interface
//-----------------------------------------------------------------------------

// This is the minimal interface that can be implemented to provide access to
// a named set of files.
inline constexpr const char *BASEFILESYSTEM_INTERFACE_VERSION = "VBaseFileSystem011";

// This interface is for VPK files to communicate with FileTracker
class IThreadedFileMD5Processor
{
public:
	int				SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction );
	bool			BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );
	bool			IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );
};

class IBaseFileSystem
{
public:
	int				Read( void* pOutput, int size, FileHandle_t file );
	int				Write( void const* pInput, int size, FileHandle_t file );

	// if pathID is NULL, all paths will be searched for the file
	FileHandle_t	Open( const char *pFileName, const char *pOptions, const char *pathID = 0 );
	void			Close( FileHandle_t file );


	void			Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType );
	unsigned int	Tell( FileHandle_t file );
	unsigned int	Size( FileHandle_t file );
	unsigned int	Size( const char *pFileName, const char *pPathID = 0 );

	void			Flush( FileHandle_t file );
	bool			Precache( const char *pFileName, const char *pPathID = 0 );

	bool			FileExists( const char *pFileName, const char *pPathID = 0 );
	bool			IsFileWritable( char const *pFileName, const char *pPathID = 0 );
	bool			SetFileWritable( char const *pFileName, bool writable, const char *pPathID = 0 );

	long			GetFileTime( const char *pFileName, const char *pPathID = 0 );

	//--------------------------------------------------------
	// Reads/writes files to utlbuffers. Use this for optimal read performance when doing open/read/close
	//--------------------------------------------------------
	std::optional< std::vector< std::byte > >	ReadFile( const char *pFileName, const char *pPath, int nMaxBytes = 0, int nStartingByte = 0 );
	bool			WriteFile( const char *pFileName, const char *pPath, std::span< const std::byte > data );
	bool			UnzipFile( const char *pFileName, const char *pPath, const char *pDestination );
};

class IIoStats
{
public:
	void OnFileSeek( int nTimeInMs );
	void OnFileRead( int nTimeInMs, int nBytesRead );
	void OnFileOpen( const char * pFileName );

	int GetNumberOfFileSeeks();
	int GetTimeInFileSeek();

	int GetNumberOfFileReads();
	int GetTimeInFileReads();
	int GetFileReadTotalSize();

	int GetNumberOfFileOpens();

	void Reset();
};

//-----------------------------------------------------------------------------
// Main file system interface
//-----------------------------------------------------------------------------
class IFileSystem : public IBaseFileSystem
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool			Connect( CreateInterfaceFn factory );
	void			Disconnect();
	void			*QueryInterface( const char *pInterfaceName );
	InitReturnVal_t	Init();
	void			Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void			Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	//--------------------------------------------------------
	// Search path manipulation
	//--------------------------------------------------------

	// Add paths in priority order (mod dir, game dir, ....)
	// If one or more .pak files are in the specified directory, then they are
	//  added after the file system path
	// If the path is the relative path to a .bsp file, then any previous .bsp file 
	//  override is cleared and the current .bsp is searched for an embedded PAK file
	//  and this file becomes the highest priority search path ( i.e., it's looked at first
	//   even before the mod's file system path ).
	void			AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType = PATH_ADD_TO_TAIL );
	bool			RemoveSearchPath( const char *pPath, const char *pathID = 0 );

	// Remove all search paths (including write path?)
	void			RemoveAllSearchPaths( void );

	// Remove search paths associated with a given pathID
	void			RemoveSearchPaths( const char *szPathID );

	// This is for optimization. If you mark a path ID as "by request only", then files inside it
	// will only be accessed if the path ID is specifically requested. Otherwise, it will be ignored.
	// If there are currently no search paths with the specified path ID, then it will still
	// remember it in case you add search paths with this path ID.
	void			MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly );

	bool			IsFileInReadOnlySearchPath(const char *pPath, const char *pathID = 0);

	// converts a partial path into a full path
	const char		*RelativePathToFullPath( const char *pFileName, const char *pPathID, char *pLocalPath, int localPathBufferSize, PathTypeFilter_t pathFilter = FILTER_NONE, PathTypeQuery_t *pPathType = nullptr );
	// Returns the search path, each path is separated by ;s. Returns the length of the string returned
	int				GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen );

	// interface for custom pack files > 4Gb
	bool			AddPackFile( const char *fullpath, const char *pathID );

	//--------------------------------------------------------
	// File manipulation operations
	//--------------------------------------------------------

	// Deletes a file (on the WritePath)
	void			RemoveFile( char const* pRelativePath, const char *pathID = 0 );

	// Renames a file (on the WritePath)
	bool			RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID = 0 );

	// create a local directory structure
	void			CreateDirHierarchy( const char *path, const char *pathID = 0 );

	// File I/O and info
	bool			IsDirectory( const char *pFileName, const char *pathID = 0 );

	void			FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime );

	//--------------------------------------------------------
	// Open file operations
	//--------------------------------------------------------

	void			SetBufferSize( FileHandle_t file, unsigned nBytes );

	bool			IsOk( FileHandle_t file );

	bool			EndOfFile( FileHandle_t file );

	char			*ReadLine( char *pOutput, int maxChars, FileHandle_t file );
	// Don't let SWIG see the attribute or it will complain.
	[[gnu::format(printf, 3, 4)]] int				FPrintf( FileHandle_t file, const char *pFormat, ... );

	//--------------------------------------------------------
	// Dynamic library operations
	//--------------------------------------------------------

	// load/unload modules
	CSysModule 		*LoadModule( const char *pFileName, const char *pPathID = 0, bool bValidatedDllOnly = true );
	void			UnloadModule( CSysModule *pModule );

	//--------------------------------------------------------
	// File searching operations
	//--------------------------------------------------------

	// FindFirst/FindNext. Also see FindFirstEx.
	const char		*FindFirst( const char *pWildCard, FileFindHandle_t *pHandle );
	const char		*FindNext( FileFindHandle_t handle );
	bool			FindIsDirectory( FileFindHandle_t handle );
	void			FindClose( FileFindHandle_t handle );

	// Same as FindFirst, but you can filter by path ID, which can make it faster.
	const char		*FindFirstEx( 
		const char *pWildCard, 
		const char *pPathID,
		FileFindHandle_t *pHandle
		);

	// Searches for a file in all paths and results absolute path names for the file, works in pack files (zip and vpk) too
	// Lets you search for something like sound/sound.cache and get a list of every sound cache
	void			FindFileAbsoluteList( std::vector< std::string > &outAbsolutePathNames, const char *pWildCard, const char *pPathID );

	//--------------------------------------------------------
	// File name and directory operations
	//--------------------------------------------------------

	// FIXME: This method is obsolete! Use RelativePathToFullPath instead!
	// converts a partial path into a full path
	const char		*GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize );

	// Returns true on success ( based on current list of search paths, otherwise false if 
	//  it can't be resolved )
	bool			FullPathToRelativePath( const char *pFullpath, char *pRelative, int maxlen );

	// Gets the current working directory
	bool			GetCurrentDirectory( char* pDirectory, int maxlen );

	//--------------------------------------------------------
	// Filename dictionary operations
	//--------------------------------------------------------

	FileNameHandle_t	FindOrAddFileName( char const *pFileName );
	bool				String( const FileNameHandle_t& handle, char *buf, int buflen );

	//--------------------------------------------------------
	// Asynchronous file operations
	//--------------------------------------------------------

	//------------------------------------
	// Global operations
	//------------------------------------
			FSAsyncStatus_t	AsyncRead( const FileAsyncRequest_t &request, FSAsyncControl_t *phControl = nullptr )	{ return AsyncReadMultiple( &request, 1, phControl ); 	}
	FSAsyncStatus_t	AsyncReadMultiple( const FileAsyncRequest_t *pRequests, int nRequests,  FSAsyncControl_t *phControls = nullptr );
	FSAsyncStatus_t	AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl = nullptr );
	FSAsyncStatus_t	AsyncAppendFile(const char *pAppendToFileName, const char *pAppendFromFileName, FSAsyncControl_t *pControl = nullptr );
	void			AsyncFinishAll( int iToPriority = 0 );
	void			AsyncFinishAllWrites();
	FSAsyncStatus_t	AsyncFlush();
	bool			AsyncSuspend();
	bool			AsyncResume();

	//------------------------------------
	// Functions to hold a file open if planning on doing mutiple reads. Use is optional,
	// and is taken only as a hint
	//------------------------------------
	FSAsyncStatus_t	AsyncBeginRead( const char *pszFile, FSAsyncFile_t *phFile );
	FSAsyncStatus_t	AsyncEndRead( FSAsyncFile_t hFile );

	//------------------------------------
	// Request management
	//------------------------------------
	FSAsyncStatus_t	AsyncFinish( FSAsyncControl_t hControl, bool wait = true );
	FSAsyncStatus_t	AsyncGetResult( FSAsyncControl_t hControl, void **ppData, int *pSize );
	FSAsyncStatus_t	AsyncAbort( FSAsyncControl_t hControl );
	FSAsyncStatus_t	AsyncStatus( FSAsyncControl_t hControl );
	// set a new priority for a file already in the queue
	FSAsyncStatus_t	AsyncSetPriority(FSAsyncControl_t hControl, int newPriority);
	void			AsyncAddRef( FSAsyncControl_t hControl );
	void			AsyncRelease( FSAsyncControl_t hControl );

	//--------------------------------------------------------
	// Remote resource management
	//--------------------------------------------------------

	// starts waiting for resources to be available
	// returns FILESYSTEM_INVALID_HANDLE if there is nothing to wait on
	WaitForResourcesHandle_t WaitForResources( const char *resourcelist );
	// get progress on waiting for resources; progress is a float [0, 1], complete is true on the waiting being done
	// returns false if no progress is available
	// any calls after complete is true or on an invalid handle will return false, 0.0f, true
	bool			GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ );
	// cancels a progress call
	void			CancelWaitForResources( WaitForResourcesHandle_t handle );

	// hints that a set of files will be loaded in near future
	// HintResourceNeed() is not to be confused with resource precaching.
	int				HintResourceNeed( const char *hintlist, int forgetEverything );
	// returns true if a file is on disk
	bool			IsFileImmediatelyAvailable(const char *pFileName);
	
	// copies file out of pak/bsp onto disk (to be accessible by third-party code)
	void			GetLocalCopy( const char *pFileName );

	//--------------------------------------------------------
	// Debugging operations
	//--------------------------------------------------------

	// Dump to printf/OutputDebugString the list of files that have not been closed
	void			PrintOpenedFiles( void );
	void			PrintSearchPaths( void );

	// output
	void			SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) );
	void			SetWarningLevel( FileWarningLevel_t level );
	void			AddLoggingFunc( void (*pfnLogFunc)( const char *fileName, const char *accessType ) );
	void			RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc );

	// Returns the file system statistics retreived by the implementation.  Returns NULL if not supported.
	const FileSystemStatistics *GetFilesystemStatistics();

	//--------------------------------------------------------
	// Start of new functions after Lost Coast release (7/05)
	//--------------------------------------------------------

	FileHandle_t	OpenEx( const char *pFileName, const char *pOptions, unsigned flags = 0, const char *pathID = 0, char **ppszResolvedFilename = nullptr );

	// Extended version of read provides more context to allow for more optimal reading
	int				ReadEx( void* pOutput, int sizeDest, int size, FileHandle_t file );
	int				ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate = false, bool bOptimalAlloc = false, int nMaxBytes = 0, int nStartingByte = 0, FSAllocFunc_t pfnAlloc = nullptr );

	FileNameHandle_t	FindFileName( char const *pFileName );

	void SetupPreloadData();
	void DiscardPreloadData();

	// Fixme, we could do these via a string embedded into the compiled data, etc...
	enum KeyValuesPreloadType_t
	{
		TYPE_VMT,
		TYPE_SOUNDEMITTER,
		TYPE_SOUNDSCAPE,
		TYPE_SOUNDOPERATORS,
		NUM_PRELOAD_TYPES
	};

	// If the "PreloadedData" hasn't been purged, then this'll try and instance the KeyValues using the fast path of compiled keyvalues loaded during startup.
	// Otherwise, it'll just fall through to the regular KeyValues loading routines
	KeyValues	*LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );
	bool		LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );

	FSAsyncStatus_t	AsyncWrite(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend = false, FSAsyncControl_t *pControl = nullptr );
	// Async read functions with memory blame
	FSAsyncStatus_t			AsyncReadCreditAlloc( const FileAsyncRequest_t &request, const char *pszFile, int line, FSAsyncControl_t *phControl = nullptr )	{ return AsyncReadMultipleCreditAlloc( &request, 1, pszFile, line, phControl ); 	}
	FSAsyncStatus_t	AsyncReadMultipleCreditAlloc( const FileAsyncRequest_t *pRequests, int nRequests, const char *pszFile, int line, FSAsyncControl_t *phControls = nullptr );

	FSAsyncStatus_t AsyncDirectoryScan( const char* pSearchSpec, bool recurseFolders,  void* pContext, FSAsyncScanAddFunc_t pfnAdd, FSAsyncScanCompleteFunc_t pfnDone, FSAsyncControl_t *pControl = nullptr );

	bool			GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, std::size_t bufSizeInBytes );

	//--------------------------------------------------------
	//--------------------------------------------------------
	std::optional< std::vector< std::byte > >	ReadToVector( FileHandle_t hFile, int nMaxBytes = 0 );

	//--------------------------------------------------------
	// Optimal IO operations
	//--------------------------------------------------------
	bool		GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	inline unsigned		GetOptimalReadSize( FileHandle_t hFile, unsigned nLogicalSize );
	void		*AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize = 0, unsigned nOffset = 0 );
	void		FreeOptimalReadBuffer( void * );

	//--------------------------------------------------------
	//
	//--------------------------------------------------------
	void		BeginMapAccess();
	void		EndMapAccess();

	// Returns true on success, otherwise false if it can't be resolved
	bool		FullPathToRelativePathEx( const char *pFullpath, const char *pPathId, char *pRelative, int maxlen );

	int			GetPathIndex( const FileNameHandle_t &handle );
	long		GetPathTime( const char *pPath, const char *pPathID );


	//--------------------------------------------------------
	// Whitelisting for pure servers.
	//--------------------------------------------------------

	// This should be called ONCE at startup. Multiplayer games (gameinfo.txt does not contain singleplayer_only)
	// want to enable this so sv_pure works.
	void			EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );

	// This is called when the client connects to a server using a pure_server_whitelist.txt file.
	//
	// Files listed in pWantCRCList will have CRCs calculated for them IF they come off disk
	// (and those CRCs will come out of GetUnverifiedCRCFiles).
	//
	// Files listed in pAllowFromDiskList will be allowed to load from disk. All other files will
	// be forced to not come from disk.
	//
	// The filesystem hangs onto the whitelists you pass in here, and it will Release() them when it closes down
	// or when you call this function again.
	//
	// NOTE: The whitelists you pass in here will be accessed from multiple threads, so make sure the 
	//       IsFileInList function is thread safe.
	//
	// If pFilesToReload is non-null, the filesystem will hand back a list of files that should be reloaded because they
	// are now "dirty". For example, if you were on a non-pure server and you loaded a certain model, and then you connected
	// to a pure server that said that model could not come from disk, then pFilesToReload would specify that model
	// and the engine should reload it.
	//
	// Be sure to call Release() on pFilesToReload.
	void			RegisterFileWhitelist( IFileList *pWantCRCList, IFileList *pAllowFromDiskList, IFileList **pFilesToReload );

	// Called when the client logs onto a server. Any files that came off disk should be marked as 
	// unverified because this server may have a different set of files it wants to guarantee.
	void			MarkAllCRCsUnverified();

	// As the server loads whitelists when it transitions maps, it calls this to calculate CRCs for any files marked
	// with check_crc.   Then it calls CheckCachedFileCRC later when it gets client requests to verify CRCs.
	void			CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter );
	EFileCRCStatus	CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash );

	// Fills in the list of files that have been loaded off disk and have not been verified.
	// Returns the number of files filled in (between 0 and nMaxFiles).
	//
	// This also removes any files it's returning from the unverified CRC list, so they won't be
	// returned from here again.
	// The client sends batches of these to the server to verify.
	int				GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles );
	
	// Control debug message output.
	// Pass a combination of WHITELIST_SPEW_ flags.
	int				GetWhitelistSpewFlags();
	void			SetWhitelistSpewFlags( int flags );

	// Installs a callback used to display a dirty disk dialog
	void			InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func );

	bool			IsLaunchedFromXboxHDD();
	bool			IsInstalledToXboxHDDCache();
	bool			IsDVDHosted();
	bool			IsInstallAllowed();

	int				GetSearchPathID( char *pPath, int nMaxLen );
	bool			FixupSearchPathsAfterInstall();
	
	FSDirtyDiskReportFunc_t		GetDirtyDiskReportFunc();

	void AddVPKFile( char const *pszName, SearchPathAdd_t addType = PATH_ADD_TO_TAIL );
	void RemoveVPKFile( char const *pszName );
	void GetVPKFileNames( std::vector< std::string > &destVector );
	void			RemoveAllMapSearchPaths();
	void			SyncDvdDevCache();

	bool			GetStringFromKVPool( CRC32_t poolKey, unsigned int key, char *pOutBuff, int buflen );

	bool			DiscoverDLC( int iController );
	int				IsAnyDLCPresent( bool *pbDLCSearchPathMounted = nullptr );
	bool			GetAnyDLCInfo( int iDLC, unsigned int *pLicenseMask, wchar_t *pTitleBuff, int nOutTitleSize );
	int				IsAnyCorruptDLC();
	bool			GetAnyCorruptDLCInfo( int iCorruptDLC, wchar_t *pTitleBuff, int nOutTitleSize );
	bool			AddDLCSearchPaths();
	bool			IsSpecificDLCPresent( unsigned int nDLCPackage );
	
	// call this to look for CPU-hogs during loading processes. When you set this, a breakpoint
	// will be issued whenever the indicated # of seconds go by without an i/o request.  Passing
	// 0.0 will turn off the functionality.
	void            SetIODelayAlarm( float flThreshhold );

	bool			AddXLSPUpdateSearchPath( const void *pData, int nSize );
	
	IIoStats		*GetIoStats();

	void			CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );
	bool			CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value );

	void			GetVPKFileStatisticsKV( KeyValues *pKV );

	// Whether the game should reload a file, given the whitelist now in force (see RegisterFileWhitelist)
	bool			ShouldGameReloadFile( const char *pFilename );

};

//-----------------------------------------------------------------------------

[[gnu::always_inline]] inline constexpr auto GetLastProfileFileRead() { return nullptr; }


//-----------------------------------------------------------------------------

inline unsigned IFileSystem::GetOptimalReadSize( FileHandle_t hFile, unsigned nLogicalSize ) 
{ 
	unsigned align; 
	if ( GetOptimalIOConstraints( hFile, &align, nullptr, nullptr ) ) 
		return AlignValue( nLogicalSize, align );
	else
		return nLogicalSize;
}

//-----------------------------------------------------------------------------
// Async memory tracking
//-----------------------------------------------------------------------------

#if defined(_DEBUG)
#define AsyncRead( a, b ) AsyncReadCreditAlloc( a, __FILE__, __LINE__, b )
template <typename T1, typename T2, typename T3>
inline auto AsyncReadMutiple( T1 a, T2 b, T3 c, std::source_location loc = std::source_location::current() )
{
	return AsyncReadMultipleCreditAlloc( a, b, loc.file_name(), static_cast<int>( loc.line() ), c );
}
#endif

//-----------------------------------------------------------------------------
// Globals Exposed
//-----------------------------------------------------------------------------
extern IFileSystem *g_pFullFileSystem;;
