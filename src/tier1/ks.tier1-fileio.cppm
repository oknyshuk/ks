// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.fileio;

import std;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
export import ks.tier1.utlstring;

export extern "C++" {

typedef std::int64_t time64_t;

class CPathString
{
public:

	// Constructors: Automatically fixes slashes and removes double slashes when the object is
	// constructed, and then knows how to append magic \\?\ on Windows for unicode paths
	CPathString( const char *pchUTF8Path );
	~CPathString();

	// Gets the path in UTF8
	const char *GetUTF8Path();

	// Gets wchar_t based path, with \\?\ pre-pended (allowing long paths on Win32, should only be used with unicode aware filesystem calls)
	const wchar_t *GetWCharPathPrePended();

private:

	void PopulateWCharPath();

	char *m_pchUTF8Path;
	wchar_t *m_pwchWideCharPathPrepended;

};

// iterator class, initialize with the path & pattern you want to want files/dirs for.
//
// all string setters and accessors use UTF-8 encoding.
class CDirIterator
{
public:

	CDirIterator( const char *pchSearchPath );

	CDirIterator( const char *pchPath, const char *pchPattern );
	~CDirIterator();

	bool IsValid() const;

	// fetch the next file
	bool BNextFile();

	// name of the current file - file portion only, not full path
	const char *CurrentFileName();

	// size of the current file
	int64 CurrentFileLength() const;

	// creation time of the current file
	time64_t CurrentFileCreateTime() const;

	// mod time of the current file
	time64_t CurrentFileWriteTime() const;

	// mode/type checks:

	// is the current file actually a directory?
	bool BCurrentIsDir() const;

	// is the current file hidden?
	bool BCurrentIsHidden() const;

	// is the current file read-only?
	bool BCurrentIsReadOnly() const;

	// is the current file a system file?
	bool BCurrentIsSystem() const;

	// is the current file's archive bit set?
	bool BCurrentIsMarkedForArchive() const;

private:
	void Init( const char *pchSearchPath );
	bool BValidFilename();
	bool m_bNoFiles, m_bUsedFirstFile;

	int64 m_hFind;
	struct _finddata_t *m_pFindData;
};

//-----------------------------------------------------------------------------
// Purpose: Encapsulates buffered async writing to a large file (one that will require multiple write calls)
//			calling Close() or destructing this object will block until the file is completely written
//-----------------------------------------------------------------------------
class CFileWriter
{
public:

	// Possible seek types
	enum ESeekOrigin
	{
		k_ESeekSet,
		k_ESeekCur,
		k_ESeekEnd
	};

	CFileWriter( bool bAsync = false );
	~CFileWriter();

	bool BFileOpen();
	bool BSetFile( const char *pchFile, bool bAllowOpenExisting = false );
	bool Write( const void *pvData, uint32 cubData );
	int  Printf( char *pDest, int bufferLen, char const *pFormat, ... );
	bool Seek( uint64 offset, ESeekOrigin eOrigin );
	void Flush();
	void Close();
	uint64 GetBytesWritten();

	void Sleep( uint nMSec ); // system specific sleep call

private:
	HANDLE m_hFileDest;
	uint64 m_cubWritten;
	std::atomic<int> m_cubOutstanding;
	bool m_bAsync;
	bool m_bDefaultAsync;
	uint32 m_unThreadID; // main thread for this FileWriter. On this thread we support correct async IO

	// if the CFileWriter is called from any other thread, we block until the write is complete
	// this is not great but a good enough for log files and we didn't need a full blow IOCP manager for this.
	std::atomic<int> m_cPendingCallbacksFromOtherThreads;

};

// data accessor
inline uint64 CFileWriter::GetBytesWritten()
{
	return m_cubWritten;
}

//-----------------------------------------------------------------------------
// Purpose: Encapsulates watching a directory for file changes
//-----------------------------------------------------------------------------
class CDirWatcher
{
public:
	CDirWatcher();
	~CDirWatcher();

	// only one directory can be watched at a time
	void SetDirToWatch( const char *pchDir );

	// retrieve any changes
	bool GetChangedFile( std::string *psFile );

private:
	std::deque<std::string> m_listChangedFiles;
	void *m_hFile;
	void *m_pOverlapped;
	void *m_pFileInfo;
	friend class CDirWatcherFriend;

	void AddFileToChangeList( const char *pchFile );
};

bool CreateDirRecursive( const char *pchPathIn );
bool BFileExists( const char *pchFileNameIn );
bool BCreateDirectory( const char *path );
bool BRemoveDirectoryRecursive( const char *pchPath );

}
