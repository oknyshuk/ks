#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



import ks.tier1;
	#include "platform/linux/linux_support.h"
	#define INVALID_HANDLE_VALUE (void *)-1



import std;

#include "vpklib/packedstore.h"
#include "filesystem.h"
#include "bspfile.h"
#include "threadsaferefcountedobject.h"
#include "filetracker.h"


#define CORRECT_PATH_SEPARATOR '/'
#define INCORRECT_PATH_SEPARATOR '\\'

#define PATHSEPARATOR(c) ((c) == '/')

#define MAX_FILEPATH 512


// Case-insensitive pool of paths and path IDs. Views from it compare by data(); a null data() is "none".
extern ks::intern_pool_mt g_PathIDTable;
inline std::string_view PathIDSymbol( const char *p ) { return p ? g_PathIDTable.intern( p ) : std::string_view(); }

struct DLCCorrupt_t
{
	XCONTENT_DATA	m_ContentData;
};

struct DLCContent_t
{
	XCONTENT_DATA	m_ContentData;
	DWORD			m_LicenseMask;
	int				m_nController;
	char			m_szVolume[8];
	bool			m_bMounted;
	bool			m_bCorrupt;
};

struct CDLCLess
{
	bool operator()( const DLCContent_t& lhs, const DLCContent_t& rhs ) const
	{
		return ( ( lhs.m_LicenseMask & 0xFFFF0000 ) < ( rhs.m_LicenseMask & 0xFFFF0000 ) );
	}
};

enum FileMode_t
{
	FM_BINARY,
	FM_TEXT
};

enum FileType_t
{
	FT_NORMAL,
	FT_PACK_BINARY,
	FT_PACK_TEXT,
};

class IThreadPool;
class CAsyncJobFuliller;
class CBlockingFileItemList;
class CCompiledKeyValuesReader;
class CBaseFileSystem;
class CPackFileHandle;
class CPackFile;
class IFileList;
class CFileOpenInfo;
class CFileHandleTimer;

class CWhitelistSpecs
{
public:
	IFileList	*m_pWantCRCList;
	IFileList	*m_pAllowFromDiskList;
};
typedef CThreadSafeRefCountedObject<CWhitelistSpecs*> CWhitelistHolder;

//-----------------------------------------------------------------------------

class CIODelayAlarmThread;

class CFileHandle
{
public:
	CFileHandle( CBaseFileSystem* fs );
	virtual ~CFileHandle();

	void	Init( CBaseFileSystem* fs );

	int		GetSectorSize();
	bool	IsOK();
	void	Flush();
	void	SetBufferSize( int nBytes );

	int		Read( void* pBuffer, int nLength );
	int		Read( void* pBuffer, int nDestSize, int nLength );

	int		Write( const void* pBuffer, int nLength );
	int		Seek( int64 nOffset, int nWhence );
	int		Tell();
	int		Size();

	int64 AbsoluteBaseOffset();
	bool	EndOfFile();

	char *m_pszTrueFileName;
	char const *Name() const { return m_pszTrueFileName ? m_pszTrueFileName : ""; }

	void SetName( char const *pName )
	{
		contract_assert( pName );
		contract_assert( !m_pszTrueFileName );
		int len = V_strlen( pName );
		m_pszTrueFileName = new char[len + 1];
		std::memcpy( m_pszTrueFileName, pName, len + 1 );
	}

	CPackFileHandle		*m_pPackFileHandle;
	CPackedStoreFileHandle m_VPKHandle;
	int64				m_nLength;
	FileType_t			m_type;
	std::FILE				*m_pFile;

protected:
	CBaseFileSystem		*m_fs;

	enum
	{
	    // lwss: fix compiler warning with multi-char character constants.
	    // tested and these were the same order, although still not portable.
		MAGIC = 0x43464861, // 'CFHa',
		FREE_MAGIC = 0x4672654D // 'FreM'
	};
	unsigned int	m_nMagic;

	bool IsValid() const;
};

// A pack file handle - essentially represents a file inside the pack file.  
// Note, there is no provision for compression here at the current time.
class CPackFileHandle
{
public:
	inline CPackFileHandle( CPackFile* pOwner, int64 nBase, unsigned int nLength, unsigned int nIndex = -1, unsigned int nFilePointer = 0 );
	inline ~CPackFileHandle();

	int				Read( void* pBuffer, int nDestSize, int nBytes );
	int				Seek( int nOffset, int nWhence );
	int				Tell() { return m_nFilePointer; }
	int				Size() { return m_nLength; }

	inline void		SetBufferSize( int nBytes );
	inline int		GetSectorSize();
	inline int64	AbsoluteBaseOffset();

protected:
	int64			m_nBase;			// Base offset of the file inside the pack file.
	unsigned int	m_nFilePointer;		// Current seek pointer (0 based from the beginning of the file).
	CPackFile*		m_pOwner;			// Pack file that owns this handle
	unsigned int	m_nLength;			// Length of this file.
	unsigned int	m_nIndex;			// Index into the pack's directory table
};

//-----------------------------------------------------------------------------

class CPackFile : public CRefCounted<CRefCountServiceMT>
{		
public:

	inline CPackFile();
	inline virtual ~CPackFile();

	// The means by which you open files:
	virtual CFileHandle *OpenFile( const char *pFileName, const char *pOptions = "rb" );

	// The two functions a pack file must provide
	virtual bool Prepare( int64 fileLen = -1, int64 nFileOfs = 0 ) = 0;
	virtual bool FindFile( const char *pFilename,  int &nIndex, int64 &nPosition, int &nLength ) = 0;

	// This is the core IO routine for reading anything from a pack file, everything should go through here at some point
	virtual int ReadFromPack( int nIndex, void* buffer, int nDestBytes, int nBytes, int64 nOffset );
	
	// Returns the filename for a given file in the pack. Returns true if a filename is found, otherwise buffer is filled with "unknown"
	virtual bool IndexToFilename( int nIndex, char* buffer, int nBufferSize ) = 0;

	inline int GetSectorSize();

	virtual void SetupPreloadData() {}
	virtual void DiscardPreloadData() {}
	virtual int64 GetPackFileBaseOffset() = 0;

	virtual CRC32_t GetKVPoolKey() { return 0; }
	virtual bool GetStringFromKVPool( unsigned int key, char *pOutBuff, int buflen ) { return false; }

	// Note: threading model for pack files assumes that data
	// is segmented into pack files that aggregate files
	// meant to be read in one thread. Performance characteristics
	// tuned for that case
	std::recursive_mutex	m_mutex;

	// Path management:
	void SetPath( std::string_view path ) { m_Path = path; }
	std::string_view GetPath() const	{ contract_assert( m_Path.data() ); return m_Path; }
	std::string_view	m_Path;

	// possibly embedded pack
	int64				m_nBaseOffset;

	std::string			m_ZipName;

	bool				m_bIsMapPath;
	long				m_lPackFileTime;

	int					m_refCount;
	int					m_nOpenFiles;

	std::FILE				*m_hPackFileHandleFS;
	CPackedStoreFileHandle m_hPackFileHandleVPK;
	bool				m_bIsExcluded;

	int					m_PackFileID;
protected:
	int64				m_FileLength;
	CBaseFileSystem		*m_fs;

	friend class		CPackFileHandle;
};

class CZipPackFile : public CPackFile
{
public:
	CZipPackFile( CBaseFileSystem* fs, void *pSection = nullptr );
	~CZipPackFile();

	// Loads the pack file
	virtual bool Prepare( int64 fileLen = -1, int64 nFileOfs = 0 );
	virtual bool FindFile( const char *pFilename, int &nIndex, int64 &nOffset, int &nLength );
	virtual int  ReadFromPack( int nIndex, void* buffer, int nDestBytes, int nBytes, int64 nOffset  );

	int64 GetPackFileBaseOffset() { return m_nBaseOffset; }

	bool	IndexToFilename( int nIndex, char *pBuffer, int nBufferSize );

	virtual CRC32_t GetKVPoolKey();
	virtual bool GetStringFromKVPool( unsigned int key, char *pOutBuff, int buflen );

protected:
	#pragma pack(1)

	typedef struct
	{
		char name[ 112 ];
		int64 filepos;
		int64 filelen;
	} packfile64_t;

	typedef struct
	{
		char id[ 4 ];
		int64 dirofs;
		int64 dirlen;
	} packheader64_t;

	typedef struct
	{
		char id[ 8 ];
		int64 packheaderpos;
		int64 originalfilesize;
	} packappenededheader_t;

	#pragma pack()

	// A Pack file directory entry:
	class CPackFileEntry
	{
	public:
		unsigned int		m_nPosition;
		unsigned int		m_nLength;
		unsigned int		m_HashName;
		unsigned short		m_nPreloadIdx;
		unsigned short		pad;
		FileNameHandle_t	m_hDebugFilename;
	};

	// Find a file inside a pack file:
	const CPackFileEntry* FindFile( const char* pFileName );

	// Entries to the individual files stored inside the pack file, sorted by m_HashName.
	// Positions are the entry indices handed out by FindFile.
	std::vector< CPackFileEntry > m_PackFiles;

	bool						GetOffsetAndLength( const char *FileName, int &nBaseIndex, int64 &nFileOffset, int &nLength );

	// Preload Support
	void						SetupPreloadData();	
	void						DiscardPreloadData();	
	ZIP_PreloadDirectoryEntry*	GetPreloadEntry( int nEntryIndex );

	int64						m_nPreloadSectionOffset;
	unsigned int				m_nPreloadSectionSize;
	ZIP_PreloadHeader			*m_pPreloadHeader;
	unsigned short*				m_pPreloadRemapTable;
	ZIP_PreloadDirectoryEntry	*m_pPreloadDirectory;
	void*						m_pPreloadData;

	CRC32_t						m_KVPoolKey;
	ks::filename_table			m_KVStringPool;

	void						*m_pSection;
};

class CFileLoadInfo
{
public:
	bool	m_bDiskLoadDisallowed;		// If this is true, then the file may not be loaded off disk.
};


//-----------------------------------------------------------------------------

#ifdef AsyncRead
#undef AsyncRead
#undef AsyncReadMutiple
#endif

//-----------------------------------------------------------------------------

class CBaseFileSystem : public CTier2AppSystem< IFileSystem >
{
	friend class CPackFileHandle;
	friend class CPackFile;
	friend class CXZipPackFile;	
	friend class CFileHandle;
	friend class CFileTracker;
	friend class CFileTracker2;
	friend class CFileOpenInfo;

	typedef CTier2AppSystem< IFileSystem > BaseClass;

public:
	CBaseFileSystem();
	~CBaseFileSystem();

	// Methods of IAppSystem
	virtual void				*QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t		Init();
	virtual void				Shutdown();

	void						InitAsync();
	void						ShutdownAsync();

	void						ParsePathID( const char* &pFilename, const char* &pPathID, char tempPathID[MAX_PATH] );

	// file handling
	virtual FileHandle_t		Open( const char *pFileName, const char *pOptions, const char *pathID );
	virtual FileHandle_t		OpenEx( const char *pFileName, const char *pOptions, unsigned flags = 0, const char *pathID = 0, char **ppszResolvedFilename = nullptr );
	virtual void				Close( FileHandle_t );
	virtual void				Seek( FileHandle_t file, int pos, FileSystemSeek_t method );
	virtual unsigned int		Tell( FileHandle_t file );
	virtual unsigned int		Size( FileHandle_t file );
	virtual unsigned int		Size( const char *pFileName, const char *pPathID );

	virtual void				SetBufferSize( FileHandle_t file, unsigned nBytes );
	virtual bool				IsOk( FileHandle_t file );
	virtual void				Flush( FileHandle_t file );
	virtual bool				Precache( const char *pFileName, const char *pPathID );
	virtual bool				EndOfFile( FileHandle_t file );
 
	virtual int					Read( void *pOutput, int size, FileHandle_t file );
	virtual int					ReadEx( void* pOutput, int sizeDest, int size, FileHandle_t file );
	virtual int					Write( void const* pInput, int size, FileHandle_t file );
	virtual char				*ReadLine( char *pOutput, int maxChars, FileHandle_t file );
	[[gnu::format(printf, 3, 4)]] virtual int					FPrintf( FileHandle_t file, const char *pFormat, ... );

	// Reads/writes whole files
	virtual std::optional< std::vector< std::byte > >	ReadFile( const char *pFileName, const char *pPath, int nMaxBytes = 0, int nStartingByte = 0 );
	virtual bool				WriteFile( const char *pFileName, const char *pPath, std::span< const std::byte > data );
	virtual bool				UnzipFile( const char *pFileName, const char *pPath, const char *pDestination );
	virtual int					ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes = 0, int nStartingByte = 0, FSAllocFunc_t pfnAlloc = nullptr );
	virtual std::optional< std::vector< std::byte > >	ReadToVector( FileHandle_t hFile, int nMaxBytes = 0 );

	// Optimal buffer
	bool						GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	void						*AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset )	{ return MemAlloc_Alloc( nSize ); }
	void						FreeOptimalReadBuffer( void *p ) { g_pMemAlloc->Free( p ); }

	// Gets the current working directory
	virtual bool				GetCurrentDirectory( char* pDirectory, int maxlen );

	virtual void				CreateDirHierarchy( const char *path, const char *pathID );

	// returns true if the file is a directory
	virtual bool				IsDirectory( const char *pFileName, const char *pathID );

	// path info
	virtual const char			*GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize );
	virtual bool				FullPathToRelativePath( const char *pFullpath, char *pRelative, int maxlen );

	// removes a file from disk
	virtual void				RemoveFile( char const* pRelativePath, const char *pathID );

	// Remove all search paths (including write path?)
	virtual void				RemoveAllSearchPaths( void );

	// Purpose: Removes all search paths for a given pathID, such as all "GAME" paths.
	virtual void				RemoveSearchPaths( const char *pathID );

	// STUFF FROM IFileSystem
	// Add paths in priority order (mod dir, game dir, ....)
	// Can also add pak files (errr, NOT YET!)
	virtual void				AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType );
	virtual bool				RemoveSearchPath( const char *pPath, const char *pathID );
	virtual void				PrintSearchPaths( void );

	virtual void				MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly );

	virtual bool				IsFileInReadOnlySearchPath(const char *pPath, const char *pathID = 0);

	virtual bool				FileExists( const char *pFileName, const char *pPathID = nullptr );
	virtual long				GetFileTime( const char *pFileName, const char *pPathID = nullptr );
	virtual bool				IsFileWritable( char const *pFileName, const char *pPathID = nullptr );
	virtual bool				SetFileWritable( char const *pFileName, bool writable, const char *pPathID = 0 );
	virtual void				FileTimeToString( char *pString, int maxChars, long fileTime );
	
	virtual const char			*FindFirst( const char *pWildCard, FileFindHandle_t *pHandle );
	virtual const char			*FindFirstEx( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle );
	virtual const char			*FindNext( FileFindHandle_t handle );
	virtual bool				FindIsDirectory( FileFindHandle_t handle );
	virtual void				FindClose( FileFindHandle_t handle );

	virtual void				FindFileAbsoluteList( std::vector< std::string > &outAbsolutePathNames, const char *pWildCard, const char *pPathID );

	virtual void				PrintOpenedFiles( void );
	virtual void				SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) );
	virtual void				SetWarningLevel( FileWarningLevel_t level );
	virtual void				AddLoggingFunc( FileSystemLoggingFunc_t logFunc );
	virtual void				RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc );
	virtual bool				RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID );

	virtual void				GetLocalCopy( const char *pFileName );

	virtual FileNameHandle_t	FindOrAddFileName( char const *pFileName );
	virtual FileNameHandle_t	FindFileName( char const *pFileName );
	virtual bool				String( const FileNameHandle_t& handle, char *buf, int buflen );
	virtual int					GetPathIndex( const FileNameHandle_t &handle );
	long						GetPathTime( const char *pFileName, const char *pPathID );
	
	bool						ShouldGameReloadFile( const char *pFilename );
	virtual void				EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );
	virtual void				RegisterFileWhitelist( IFileList *pWantCRCList, IFileList *pAllowFromDiskList, IFileList **pFilesToReload );
	virtual	void				MarkAllCRCsUnverified();
	virtual void				CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter );
	void						CacheFileCRCs_R( const char *pPathname, ECacheCRCType eType, IFileList *pFilter, std::set< std::string, ks::iless > &searchPathNames );
	virtual EFileCRCStatus		CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash );
	virtual int					GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles );
	virtual int					GetWhitelistSpewFlags();
	virtual void				SetWhitelistSpewFlags( int flags );
	virtual void				InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func );

	virtual void				CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );
	virtual bool				CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value );

	// Returns the file system statistics retreived by the implementation.  Returns NULL if not supported.
	virtual const FileSystemStatistics *GetFilesystemStatistics();
	
	// GetVPKFileStatisticsKV
	virtual void GetVPKFileStatisticsKV( KeyValues *pKV );

	// Load dlls
	virtual CSysModule 			*LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly );
	virtual void				UnloadModule( CSysModule *pModule );

	//--------------------------------------------------------
	// asynchronous file loading
	//--------------------------------------------------------
	virtual FSAsyncStatus_t		AsyncReadMultiple( const FileAsyncRequest_t *pRequests, int nRequests, FSAsyncControl_t *pControls );
	virtual FSAsyncStatus_t		AsyncReadMultipleCreditAlloc( const FileAsyncRequest_t *pRequests, int nRequests, const char *pszFile, int line, FSAsyncControl_t *phControls = nullptr );
	virtual FSAsyncStatus_t		AsyncDirectoryScan( const char* pSearchSpec, bool recurseFolders, void* pContext, FSAsyncScanAddFunc_t pfnAdd, FSAsyncScanCompleteFunc_t pfnDone, FSAsyncControl_t *pControl = nullptr );
	virtual FSAsyncStatus_t		AsyncFinish( FSAsyncControl_t hControl, bool wait );
	virtual FSAsyncStatus_t		AsyncGetResult( FSAsyncControl_t hControl, void **ppData, int *pSize );
	virtual FSAsyncStatus_t		AsyncAbort( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncStatus( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncSetPriority(FSAsyncControl_t hControl, int newPriority);
	virtual FSAsyncStatus_t		AsyncFlush();
	virtual FSAsyncStatus_t		AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl) { return AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, true, pControl); }
	virtual FSAsyncStatus_t		AsyncWrite(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl);
	virtual FSAsyncStatus_t		AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl);
	virtual void				AsyncFinishAll( int iToPriority = std::numeric_limits<int>::min() );
	virtual void				AsyncFinishAllWrites();
	virtual bool				AsyncSuspend();
	virtual bool				AsyncResume();

	virtual void				AsyncAddRef( FSAsyncControl_t hControl );
	virtual void				AsyncRelease( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncBeginRead( const char *pszFile, FSAsyncFile_t *phFile );
	virtual FSAsyncStatus_t		AsyncEndRead( FSAsyncFile_t hFile );

	//--------------------------------------------------------
	// pack files
	//--------------------------------------------------------
	bool						AddPackFile( const char *pFileName, const char *pathID );
	bool						AddPackFileFromPath( const char *pPath, const char *pakfile, bool bCheckForAppendedPack, const char *pathID );

	// converts a partial path into a full path
	// can be filtered to restrict path types and can provide info about resolved path
	virtual const char			*RelativePathToFullPath( const char *pFileName, const char *pPathID, char *pFullPath, int fullPathBufferSize, PathTypeFilter_t pathFilter = FILTER_NONE, PathTypeQuery_t *pPathType = nullptr );
	// Returns the search path, each path is separated by ;s. Returns the length of the string returned
	virtual int					GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen );
	virtual int					GetSearchPathID( char *pPath, int nMaxLen );

	virtual bool				GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, std::size_t bufSizeInBytes );

	virtual void				BeginMapAccess();
	virtual void				EndMapAccess();
	virtual bool				FullPathToRelativePathEx( const char *pFullpath, const char *pPathId, char *pRelative, int maxlen );

	FSAsyncStatus_t				SyncRead( const FileAsyncRequest_t &request );
	FSAsyncStatus_t				SyncWrite(const char *pszFilename, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend );
	FSAsyncStatus_t				SyncAppendFile(const char *pAppendToFileName, const char *pAppendFromFileName );
	FSAsyncStatus_t				SyncGetFileSize( const FileAsyncRequest_t &request );
	void						DoAsyncCallback( const FileAsyncRequest_t &request, void *pData, int nBytesRead, FSAsyncStatus_t result );

	void						SetupPreloadData();
	void						DiscardPreloadData();

	// If the "PreloadedData" hasn't been purged, then this'll try and instance the KeyValues using the fast path of compiled keyvalues loaded during startup.
	// Otherwise, it'll just fall through to the regular KeyValues loading routines
	virtual KeyValues			*LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );
	virtual bool				LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );

	virtual bool				IsLaunchedFromXboxHDD() { return m_bLaunchedFromXboxHDD; }
	virtual bool				IsInstalledToXboxHDDCache() { return m_bFoundXboxImageInCache; }
	virtual bool				IsDVDHosted() { return m_bDVDHosted; }
	virtual bool				IsInstallAllowed() { return m_bAllowXboxInstall; }
	virtual bool				FixupSearchPathsAfterInstall();

	virtual FSDirtyDiskReportFunc_t		GetDirtyDiskReportFunc() { return m_DirtyDiskReportFunc; }

	virtual void AddVPKFile( char const *pszName, SearchPathAdd_t addType = PATH_ADD_TO_TAIL );
	virtual void RemoveVPKFile( char const *pszName );
	virtual void GetVPKFileNames( std::vector< std::string > &destVector );

	virtual void				RemoveAllMapSearchPaths( void );

	void						BuildExcludeList();
	virtual void				SyncDvdDevCache();
	bool						FixupFATXFilename( const char *pFilename, char *pOutFilename, int nOutSize );
	virtual bool				GetStringFromKVPool( CRC32_t poolKey, unsigned int key, char *pOutBuff, int buflen );

	virtual bool				DiscoverDLC( int iController );
	virtual int					IsAnyDLCPresent( bool *pbDLCSearchPathMounted = nullptr );
	virtual bool				GetAnyDLCInfo( int iDLC, unsigned int *pLicenseMask, wchar_t *pTitleBuff, int nOutTitleSize );
	virtual int					IsAnyCorruptDLC();
	virtual bool				GetAnyCorruptDLCInfo( int iCorruptDLC, wchar_t *pTitleBuff, int nOutTitleSize );
	virtual bool				AddDLCSearchPaths();
	virtual bool				IsSpecificDLCPresent( unsigned int nDLCPackage );
	void						PrintDLCInfo();
	virtual void                SetIODelayAlarm( float flTime );
	virtual bool				AddXLSPUpdateSearchPath( const void *pData, int nSize );

	virtual IIoStats			*GetIoStats();

public:
	//------------------------------------
	// Synchronous path for file operations
	//------------------------------------
	class CPathIDInfo
	{
	public:
		std::string_view GetPathID() const;
		const char* GetPathIDString() const;
		void SetPathID( std::string_view id );

	public:
		// See MarkPathIDByRequestOnly.
		bool m_bByRequestOnly;

	private:
		std::string_view m_PathID;
		const char *m_pDebugPathID;
	};

	////////////////////////////////////////////////
	// IMPLEMENTATION DETAILS FOR CBaseFileSystem //
	////////////////////////////////////////////////

	class CSearchPath
	{
		// One counted reference to a pack file: copies AddRef, moves transfer.
		class CPackFileRef
		{
		public:
			CPackFileRef() = default;
			CPackFileRef( const CPackFileRef &src ) : m_p( src.m_p ) { if ( m_p ) m_p->AddRef(); }
			CPackFileRef( CPackFileRef &&src ) noexcept : m_p( std::exchange( src.m_p, nullptr ) ) {}
			CPackFileRef &operator=( CPackFileRef src ) noexcept { std::swap( m_p, src.m_p ); return *this; }
			~CPackFileRef() { if ( m_p ) m_p->Release(); }

			void Adopt( CPackFile *p ) { *this = CPackFileRef(); m_p = p; }	// takes over the caller's reference
			CPackFile *Get() const { return m_p; }

		private:
			CPackFile *m_p = nullptr;
		};

	public:
							CSearchPath( void );

		const char* GetPathString() const;
		
		// Path ID ("game", "mod", "gamebin") accessors.
		std::string_view GetPathID() const;
		const char* GetPathIDString() const;

		// Search path (c:\hl2\hl2) accessors.
		void SetPath( std::string_view id );
		std::string_view GetPath() const;

		void SetPackFile(CPackFile *pPackFile) { m_pPackFile.Adopt( pPackFile ); }
		CPackFile *GetPackFile() const { return m_pPackFile.Get(); }

		int					m_storeId;

		// Used to track if its search 
		CPathIDInfo			*m_pPathIDInfo;

		bool				m_bIsDvdDevPath;

	private:
		std::string_view	m_Path;
		const char			*m_pDebugPath;
		CPackFileRef		m_pPackFile;
	public:
		bool				m_bIsLocalizedPath;
	};

	static void MarkLocalizedPath( CSearchPath *sp );

	class CSearchPathsVisits
	{
	public:
		void Reset()
		{
			m_Visits.clear();
		}

		bool MarkVisit( const CSearchPath &searchPath )
		{
			if ( ks::find_index( m_Visits, searchPath.m_storeId ) == -1 )
			{
				m_Visits.push_back( searchPath.m_storeId );
				return false;
			}
			return true;
		}

	private:
		std::vector< int > m_Visits;	// This is a copy of IDs for the search paths we've visited, so 
	};

	class CSearchPathsIterator
	{
	public:
		CSearchPathsIterator( CBaseFileSystem *pFileSystem, const char **ppszFilename, const char *pszPathID, PathTypeFilter_t pathTypeFilter = FILTER_NONE )
		  : m_iCurrent( -1 ),
			m_PathTypeFilter( pathTypeFilter ),
			m_bExcluded( false )
		{
			char tempPathID[MAX_PATH];
			if ( *ppszFilename && (*ppszFilename)[0] == '/' && (*ppszFilename)[1] == '/' ) // ONLY '//' (and not '\\') for our special format
			{
				// Allow for UNC-type syntax to specify the path ID.
				pFileSystem->ParsePathID( *ppszFilename, pszPathID, tempPathID );
			}
			if ( pszPathID )
			{
				m_pathID = PathIDSymbol( pszPathID );
			}

			if ( *ppszFilename && !V_IsAbsolutePath( *ppszFilename ) )
			{
				// Copy paths to minimize mutex lock time
				pFileSystem->m_SearchPathsMutex.lock();
				CopySearchPaths( pFileSystem->m_SearchPaths );
				pFileSystem->m_SearchPathsMutex.unlock();
				V_strncpy( m_Filename, *ppszFilename, sizeof( m_Filename ) );
				V_FixSlashes( m_Filename );
			}
			else
			{
				// If it's an absolute path, it isn't worth using the paths at all. Simplify
				// client logic by pretending there's a search path of 1
				m_EmptyPathIDInfo.m_bByRequestOnly = false;
				m_EmptySearchPath.m_pPathIDInfo = &m_EmptyPathIDInfo;
				m_EmptySearchPath.SetPath( m_pathID );
				m_EmptySearchPath.m_storeId = -1;
				m_Filename[0] = '\0';
			}
		}

		CSearchPathsIterator( CBaseFileSystem *pFileSystem, const char *pszPathID, PathTypeFilter_t pathTypeFilter = FILTER_NONE )
		  : m_iCurrent( -1 ),
			m_PathTypeFilter( pathTypeFilter ),
			m_bExcluded( false )
		{
			if ( pszPathID ) 
			{
				m_pathID =  PathIDSymbol( pszPathID );
			}
			// Copy paths to minimize mutex lock time
			pFileSystem->m_SearchPathsMutex.lock();
			CopySearchPaths( pFileSystem->m_SearchPaths );
			pFileSystem->m_SearchPathsMutex.unlock();
			m_Filename[0] = '\0';
		}

		CSearchPath *GetFirst();
		CSearchPath *GetNext();

	private:
		CSearchPathsIterator( const  CSearchPathsIterator & );
		void operator=(const CSearchPathsIterator &);
		void CopySearchPaths( const std::vector< CSearchPath >	&searchPaths )
		{
			m_SearchPaths = searchPaths;	// each copy AddRefs its pack file
		}

		int							m_iCurrent;
		std::string_view			m_pathID;
		std::vector< CSearchPath > 	m_SearchPaths;
		CSearchPathsVisits			m_visits;
		CSearchPath					m_EmptySearchPath;
		CPathIDInfo					m_EmptyPathIDInfo;
		PathTypeFilter_t			m_PathTypeFilter;
		char						m_Filename[MAX_PATH];	// set for relative names only
		bool						m_bExcluded;
	};

	friend class CSearchPathsIterator;

	struct FindData_t
	{
		WIN32_FIND_DATA		findData;
		int					currentSearchPathID;
		std::vector< char >	wildCardString;
		HANDLE				findHandle;
		CSearchPathsVisits	m_VisitedSearchPaths;	// This is a copy of IDs for the search paths we've visited, so avoids searching duplicate paths.
		int					m_CurrentStoreID;		// CSearchPath::m_storeId of the current search path.
		
		std::string_view	m_FilterPathID;			// What path ID are we looking at? Ignore all others. (Only set by FindFirstEx).
		
		std::set< std::string, ks::iless >	m_VisitedFiles;			// We go through the search paths in priority order, and we use this to make sure
													// that we don't return the same file more than once.
		std::vector< std::string >		m_fileMatchesFromVPK;
		std::vector< std::string >		m_dirMatchesFromVPK;
	};

	friend class CSearchPath;

	CWhitelistHolder	m_FileWhitelist;
	int					m_WhitelistSpewFlags; // Combination of WHITELIST_SPEW_ flags.

	// logging functions
	std::vector< FileSystemLoggingFunc_t > m_LogFuncs;

	std::recursive_mutex m_SearchPathsMutex;
	std::vector< CSearchPath > m_SearchPaths;
	std::vector< std::unique_ptr< CPathIDInfo > > m_PathIDInfos;
	ks::slot_pool< FindData_t, FileFindHandle_t > m_FindData;

	std::vector< CFileHandleTimer* > m_FileHandleTimers;				// Used to debug approximate times for each file read
	std::recursive_mutex m_FileHandleTimersMutex;

	int m_iMapLoad;

	// Global list of pack file handles
	std::vector< CPackFile * > m_ZipFiles;

	std::FILE *m_pLogFile;
	bool m_bOutputDebugString;

	IThreadPool *	m_pThreadPool;

	// Statistics:
	FileSystemStatistics m_Stats;

	float m_flDelayLimit;
	float m_flLastIOTime;

	
	[[gnu::always_inline]] inline void NoteIO( void )
	{
		if ( m_pDelayThread )
		{
			m_flLastIOTime = Plat_FloatTime();
		}
	}
	
	CIODelayAlarmThread *m_pDelayThread;


	CFileTracker2	m_FileTracker2;

protected:
	//----------------------------------------------------------------------------
	// Purpose: Functions implementing basic file system behavior.
	//----------------------------------------------------------------------------
	virtual std::FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo ) = 0;
	virtual void FS_setbufsize( std::FILE *fp, unsigned nBytes ) = 0;
	virtual void FS_fclose( std::FILE *fp ) = 0;
	virtual void FS_fseek( std::FILE *fp, int64 pos, int seekType ) = 0;
	virtual long FS_ftell( std::FILE *fp ) = 0;
	virtual int FS_feof( std::FILE *fp ) = 0;
	std::size_t FS_fread( void *dest, std::size_t size, std::FILE *fp ) { return FS_fread( dest, (std::size_t)-1, size, fp ); }
	virtual std::size_t FS_fread( void *dest, std::size_t destSize, std::size_t size, std::FILE *fp ) = 0;
    virtual std::size_t FS_fwrite( const void *src, std::size_t size, std::FILE *fp ) = 0;
	virtual bool FS_setmode( std::FILE *fp, FileMode_t mode ) { return false; }
	virtual std::size_t FS_vfprintf( std::FILE *fp, const char *fmt, std::va_list list ) = 0;
	virtual int FS_ferror( std::FILE *fp ) = 0;
	virtual int FS_fflush( std::FILE *fp ) = 0;
	virtual char *FS_fgets( char *dest, int destSize, std::FILE *fp ) = 0;
	virtual int FS_stat( const char *path, ks::platform::file_info *buf ) = 0;
	virtual bool FS_setwritable( const char *path, bool writable ) = 0;
	virtual HANDLE FS_FindFirstFile( const char *findname, WIN32_FIND_DATA *dat) = 0;
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat) = 0;
	virtual bool FS_FindClose(HANDLE handle) = 0;
	virtual int FS_GetSectorSize( std::FILE * ) { return 1; }

	void GetFileNameForHandle( FileHandle_t handle, char *buf, std::size_t buflen );

protected:
	//-----------------------------------------------------------------------------
	// Purpose: For tracking unclosed files
	// NOTE:  The symbol table could take up memory that we don't want to eat here.
	// In that case, we shouldn't store them in a table, or we should store them as locally allocates stings
	//  so we can control the size
	//-----------------------------------------------------------------------------
	class COpenedFile
	{
	public:
		bool operator==( const COpenedFile& src ) const { return src.m_pFile == m_pFile; }

		void		SetName( char const *name ) { m_Name = name; }
		char const	*GetName( void ) const { return m_Name.empty() ? "???" : m_Name.c_str(); }

		std::FILE		*m_pFile = nullptr;
		std::string	m_Name;
	};

	std::recursive_mutex m_OpenedFilesMutex;
	std::vector< COpenedFile >	m_OpenedFiles;
	ks::idict< bool >			m_NonexistingFilesExtensions;

	FileWarningLevel_t			m_fwLevel;
	void						(*m_pfnWarning)( const char *fmt, ... );

	std::FILE						*Trace_FOpen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo=nullptr );
	void						Trace_FClose( std::FILE *fp );
	void						Trace_FRead( int size, std::FILE* file );
	void						Trace_FWrite( int size, std::FILE* file );

	void						Trace_DumpUnclosedFiles( void );

public:
	void						LogAccessToFile( char const *accesstype, char const *fullpath, char const *options );
	void						FileSystemWarning( FileWarningLevel_t level, const char *fmt, ... );

protected:
	// Note: if pFoundStoreID is passed in, then it will set that to the CSearchPath::m_storeId value of the search path it found the file in.
	const char*					FindFirstHelper( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle, int *pFoundStoreID );
	bool						FindNextFileHelper( FindData_t *pFindData, int *pFoundStoreID );

	void 						FindFileAbsoluteListHelper( std::vector< std::string > &outAbsolutePathNames, FindData_t &findData, const char *pAbsoluteFindName );

	void						AddMapPackFile( const char *pPath, const char *pPathID, SearchPathAdd_t addType );
	void						AddPackFiles( const char *pPath, std::string_view pathID, SearchPathAdd_t addType, int iForceInsertIndex = 0 );
	bool						PreparePackFile( CPackFile &packfile, int offsetofpackinmetafile, int64 filelen );

	// Goes through all the search paths (or just the one specified) and calls FindFile on them. Returns the first successful result, if any.
	FileHandle_t				FindFileInSearchPaths( const char *pFileName, const char *pOptions, const char *pathID, unsigned flags, char **ppszResolvedFilename = nullptr, bool bTrackCRCs=false );

	bool						HandleOpenFromZipFile( CFileOpenInfo &openInfo );
	void		 				HandleOpenFromPackFile( CPackFile *pPackFile, CFileOpenInfo &openInfo );
	void						HandleOpenRegularFile( CFileOpenInfo &openInfo, bool bIsAbsolutePath );

	FileHandle_t				FindFile( const CSearchPath *path, const char *pFileName, const char *pOptions, unsigned flags, char **ppszResolvedFilename = nullptr, bool bTrackCRCs=false );
	int							FastFindFile( const CSearchPath *path, const char *pFileName );
	long						FastFileTime( const CSearchPath *path, const char *pFileName );

	const char					*GetWritePath( const char *pFilename, const char *pathID );

	// Computes a full write path
	void						ComputeFullWritePath( char* pDest, int maxlen, const char *pWritePathID, char const *pRelativePath );

	void						AddSearchPathInternal( const char *pPath, const char *pathID, SearchPathAdd_t addType, bool bAddPackFiles, int iForceInsertIndex = 0 );

	// Opens a file for read or write
	FileHandle_t OpenForRead( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename = nullptr );
	FileHandle_t OpenForWrite( const char *pFileName, const char *pOptions, const char *pathID );
	CSearchPath *FindWritePath( const char *pFilename, const char *pathID );

	// Helper function for fs_log file logging
	void LogFileAccess( const char *pFullFileName );

	bool LookupKeyValuesRootKeyName( char const *filename, char const *pPathID, char *rootName, std::size_t bufsize );

	// If bByRequestOnly is -1, then it will default to false if it doesn't already exist, and it 
	// won't change it if it does already exist. Otherwise, it will be set to the value of bByRequestOnly.
	CPathIDInfo*				FindOrAddPathIDInfo( std::string_view id, int bByRequestOnly );
	static bool					FilterByPathID( const CSearchPath *pSearchPath, std::string_view pathID );

	// Global/shared filename/path table
	ks::filename_table			m_FileNames;

	// This manages most of the info we use for pure servers (whether files came from off-disk, their CRCs, which ones are unverified, etc).
	CFileTracker	m_FileTracker;
	int				m_WhitelistFileTrackingEnabled;	// -1 if unset, 0 if disabled (single player), 1 if enabled (multiplayer).
	FSDirtyDiskReportFunc_t m_DirtyDiskReportFunc;

	static std::string_view		m_GamePathID;
	static std::string_view		m_BSPPathID;

	static bool					m_bFoundXboxImageInCache;
	static bool					m_bLaunchedFromXboxHDD;
	static bool					m_bDVDHosted;
	static bool					m_bAllowXboxInstall;
	static bool					m_bSearchPathsPatchedAfterInstall;

	// Pack exclude paths are strictly for 360 to allow holes in search paths and pack files
	// which fall through to support new or dynamic data on the host pc.
	struct ExcludeFilePath_t
	{
		FileNameHandle_t m_hName;
		FileNameHandle_t m_hFixedName;
	};
	static std::vector< FileNameHandle_t > m_ExcludeFilePaths;
	std::vector< CPackedStore * > m_VPKFiles;
	std::set< std::string, ks::iless > m_VPKDirectories;
	CPackedStoreFileHandle FindFileInVPKs( const char *pFileName );

	static std::flat_multiset< DLCContent_t, CDLCLess > m_DLCContents;
	static std::vector< DLCCorrupt_t > m_CorruptDLC;
};

inline std::string_view CBaseFileSystem::CPathIDInfo::GetPathID() const
{
	return m_PathID;
}


inline const char* CBaseFileSystem::CPathIDInfo::GetPathIDString() const
{
	return ks::nz( m_PathID.data() );
}


inline const char* CBaseFileSystem::CSearchPath::GetPathString() const
{
	return ks::nz( m_Path.data() );
}


inline void CBaseFileSystem::CPathIDInfo::SetPathID( std::string_view sym )
{
	m_PathID = sym;
	m_pDebugPathID = GetPathIDString();
}


inline std::string_view CBaseFileSystem::CSearchPath::GetPathID() const
{
	return m_pPathIDInfo->GetPathID();
}


inline const char* CBaseFileSystem::CSearchPath::GetPathIDString() const
{
	return m_pPathIDInfo->GetPathIDString();
}


inline void CBaseFileSystem::CSearchPath::SetPath( std::string_view id )
{
	m_Path = id;
	m_pDebugPath = ks::nz( m_Path.data() );
	MarkLocalizedPath( this );
}


inline std::string_view CBaseFileSystem::CSearchPath::GetPath() const
{
	return m_Path;
}


inline bool CBaseFileSystem::FilterByPathID( const CSearchPath *pSearchPath, std::string_view pathID )
{
	if ( !pathID.data() )
	{
		// They didn't specify a specific search path, so if this search path's path ID is by
		// request only, then ignore it.
		return pSearchPath->m_pPathIDInfo->m_bByRequestOnly;
	}
	else
	{
		// Bit of a hack, but specifying "BSP" as the search path will search in "GAME" for only the map/.bsp pack file path
		if ( pathID.data() == m_BSPPathID.data() )
		{
			if ( pSearchPath->GetPathID().data() != m_GamePathID.data() )
				return true;

			if ( !pSearchPath->GetPackFile() )
				return true;

			if ( !pSearchPath->GetPackFile()->m_bIsMapPath )
				return true;

			return false;
		}
		else
		{
			return (pSearchPath->GetPathID().data() != pathID.data());
		}
	}
}


// Pack file handle implementation:
                 
inline CPackFileHandle::CPackFileHandle( CPackFile* pOwner, int64 nBase, unsigned int nLength, unsigned int nIndex, unsigned int nFilePointer )
{
	m_pOwner = pOwner;
	m_nBase = nBase;
	m_nLength = nLength;
	m_nIndex = nIndex;
	m_nFilePointer = nFilePointer;
	pOwner->AddRef();
}

inline CPackFileHandle::~CPackFileHandle()
{
	m_pOwner->m_mutex.lock();
	--m_pOwner->m_nOpenFiles;
	if ( m_pOwner->m_nOpenFiles == 0 && m_pOwner->m_bIsMapPath )
	{
		if ( m_pOwner->m_hPackFileHandleFS )
		{
			m_pOwner->m_fs->Trace_FClose( m_pOwner->m_hPackFileHandleFS );
			m_pOwner->m_hPackFileHandleFS = nullptr;
		}
	}
	m_pOwner->Release();
	m_pOwner->m_mutex.unlock();
}

inline void CPackFileHandle::SetBufferSize( int nBytes ) 
{
	if ( m_pOwner->m_hPackFileHandleFS )
	{
		m_pOwner->m_fs->FS_setbufsize( m_pOwner->m_hPackFileHandleFS, nBytes );
	}
}

inline int CPackFileHandle::GetSectorSize() 
{ 
	return m_pOwner->GetSectorSize(); 
}

inline int64 CPackFileHandle::AbsoluteBaseOffset() 
{ 
	return m_pOwner->GetPackFileBaseOffset() + m_nBase;
}

// Pack file implementation:
inline CPackFile::CPackFile()
{
	m_FileLength = 0;
	m_hPackFileHandleFS = nullptr;
	m_fs = nullptr;
	m_nBaseOffset = 0;
	m_bIsMapPath = false;
	m_lPackFileTime = 0L;
	m_refCount = 0;
	m_nOpenFiles = 0;
	m_PackFileID = 0;
}

inline CPackFile::~CPackFile()
{
	if ( m_nOpenFiles )
	{
		Error( "Closing pack file with %d open files!\n", m_nOpenFiles );
	}

	if ( m_hPackFileHandleFS )
	{
		m_fs->Trace_FClose( m_hPackFileHandleFS );
		m_hPackFileHandleFS = nullptr;
	}

	ks::erase_first( m_fs->m_ZipFiles, this );
}


inline int CPackFile::GetSectorSize()
{
	if ( m_hPackFileHandleFS )
	{
		return m_fs->FS_GetSectorSize( m_hPackFileHandleFS );
	}
	else if ( m_hPackFileHandleVPK )
	{
		return 2048;
	}
	else
	{
		return -1;
	}
}



// singleton accessor
CBaseFileSystem *BaseFileSystem();
