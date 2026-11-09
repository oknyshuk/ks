#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "ifilelist.h"
import std;
#include "filesystem.h"
#include "vpklib/packedstore.h"

class CBaseFileSystem;
class CFileHandle;


enum EFileFlags
{
	k_eFileFlags_None					= 0x0000,

	k_eFileFlagsHasCRC					= 0x0002,	// m_CRC represents the most recently-loaded version of the file. This might be 
													// unset but m_CRC (and k_eFileFlagsGotCRCOnce) could be set, signifying that
													// the file has been opened since last time we calculated the CRC but we didn't
													// calculate a CRC for that file version.
	
	k_eFileFlagsGotCRCOnce				= 0x0008,	// This is set if we EVER had k_eFileFlagsHasCRC set.. m_CRC will still be set in that case,
													// but it'll be the last CRC we calculated and not necessarily 
	
	k_eFileFlagsFailedToLoadLastTime	= 0x0010	// This is used if we had a record of the file and the game tried to open it, but
													// it couldn't be opened. This will happen if the file was loaded from disk, then
													// sv_pure is turned on and the file is forced to not come from disk, but there is no
													// other version. In that case, the game should be told to retry the file
													// next time sv_pure is changed because if sv_pure goes back to 0 it -would- load
													// the file legitimately.
};


class CPathIDFileList;


class CFileInfo
{
public:
	CFileInfo();
	~CFileInfo();
	
	const char* GetFilename();
	const char* GetPathIDString();
	
public:	

	unsigned short	m_Flags;	// This is a combination of EFileFlags.
	CRC32_t			m_CRC;		// The CRC for this file.
	
	CPathIDFileList	*m_pPathIDFileList;
	const char		*m_pFilename;		// Our key in m_pPathIDFileList->m_Files
	
	int				m_iNeedsVerificationListIndex;	// Index into m_NeedsVerificationList or -1 if not in the list.
};


// This tracks a list of files for the specified path ID.
class CPathIDFileList
{
public:
	CPathIDFileList();
	~CPathIDFileList();
	CFileInfo*		FindFileInfo( const char *pFilename );
	CFileInfo*		AddFileInfo( const char *pFilename );

public:
	std::string						m_PathID;				// "" for a null path ID.
	std::map< std::string, std::unique_ptr< CFileInfo >, ks::path_iless >	m_Files;
};


//-----------------------------------------------------------------------------
// This tracks the files that have been opened by the filesystem.
// It remembers that they were loaded off-disk.
// If the filesystem is tracking CRCs, then it will calculate a CRC
// for each file that came off disk.
//
// TODO: This is similar to CBaseFileSystem::m_OpenedFiles - it could probably
// manage both sets of files in the same list. Having 2 separate lists might
// be confusing.
//-----------------------------------------------------------------------------
class CFileTracker
{
public:
	CFileTracker( CBaseFileSystem *pFileSystem );
	~CFileTracker();

	// If this is true, then we'll calculate CRCs for each file that came off disk.
	void	SetWantFileCRCs( bool bWantCRCs );

	// As files are opened, if it is calculating CRCs, it will add those files and their
	// CRCs to the "unverified CRC" list. The client can then ask the server to verify
	// those CRCs to make sure the client is "pure".
	void	MarkAllCRCsUnverified();
	void	MarkAllCRCsVerified( bool bLockMutex=true );

	// Cache a file's CRC. Loads the file and calculates the CRC if we don't have it yet.
	void			CacheFileCRC( const char *pPathID, const char *pRelativeFilename );
	EFileCRCStatus	CheckCachedFileCRC( const char *pPathID, const char *pRelativeFilename, CRC32_t *pCRC );

	// This is like CacheFileCRC, but it assumes that the same file would be found by pPathIDToCopyFrom, so it just
	// copies the CRC record from that path ID into the one in pPathID and avoids a redundant CRC calculation.
	void			CacheFileCRC_Copy( const char *pPathID, const char *pRelativeFilename, const char *pPathIDToCopyFrom );

	// When we don't have a whitelist, it loads files without bothering to calculate their CRCs (we'd only
	// need them on a pure server), but when we get a whitelist, we'll want the CRCs, so it goes back, opens those
	// files, and calculates the CRCs for them.
	void	CalculateMissingCRCs( IFileList *pForceMatchList );

	int		GetUnverifiedCRCFiles( CUnverifiedCRCFile *pFiles, int nMaxFiles );

	// Note that we just opened this file and calculate a CRC for it.
	void	NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, FileHandle_t fp );
	void	NoteFileFailedToLoad( const char *pFilename, const char *pPathID );
	
	// Get a file info from a specific path ID.
	CFileInfo*	GetFileInfo( const char *pFilename, const char *pPathID );
	
	// Get all file infos with the specified filename (i.e. in all path IDs).
	int			GetFileInfos( CFileInfo **ppFileInfos, int nMaxFileInfos, const char *pFilename );

	// Clear everything.
	void	Clear();

private:
	void					CalculateMissingCRC( const char *pFilename, const char *pPathID );
	CPathIDFileList*		GetPathIDFileList( const char *pPathID, bool bAutoAdd=true );

	CRC32_t					CalculateCRCForFile( FileHandle_t fp );

	void AddToNeedsVerificationList( CFileInfo *pInfo )
	{
		pInfo->m_iNeedsVerificationListIndex = (int)m_NeedsVerificationList.size();
		m_NeedsVerificationList.push_back( pInfo );
	}

private:
	std::vector< CFileInfo * >		m_NeedsVerificationList;	// The list of files that need CRCs verified.
	ks::idict< std::unique_ptr< CPathIDFileList > >	m_PathIDs;
	CBaseFileSystem					*m_pFileSystem;
	std::recursive_mutex				m_Mutex;	// Threads call into here, so we need to be safe.
};


inline const char* CFileInfo::GetFilename()
{
	return m_pFilename;
}

inline const char* CFileInfo::GetPathIDString()
{
	return m_pPathIDFileList->m_PathID.c_str();
}


struct TrackedFile_t
{
	TrackedFile_t()
	{
		m_nFileFraction = 0;
		m_nFilePos = 0;
		m_nLength = 0;
		m_cubSequentialRead = 0;
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
		m_bPackOrVPKFile = false;
		m_bFileInVPK = false;
		m_cubTotalRead = 0;
		m_bInRecentFileList = false;
	}
	CRC32_t m_crcIdentifier;
	std::string m_filename;
	std::string m_path;
	int m_nFileFraction;

	MD5Context_t m_md5ctx;
	FileHash_t m_filehashFinal;
	FileHash_t m_filehashInProgress;
	int m_cubTotalRead;

	int64 m_nFilePos;
	int64 m_nLength;
	int64 m_cubSequentialRead;
	bool m_bInRecentFileList;	// queued in CFileTracker2::m_RecentFileList (or popped without a path)
	int m_PackFileID;
	int m_nPackFileNumber;
	bool m_bPackOrVPKFile;
	bool m_bFileInVPK;

	void RebuildFileName( const char *pFilename, const char *pPathID )
	{
		m_filename = ks::nz( pFilename );
		m_path = ks::nz( pPathID );

		CRC32_t crcFilename;
		CRC32_Init( &crcFilename );
		CRC32_ProcessBuffer( &crcFilename, pFilename, V_strlen( pFilename ) );
		if ( pPathID )
			CRC32_ProcessBuffer( &crcFilename, pPathID, V_strlen( pPathID ) );
		CRC32_Final( &crcFilename );
		m_crcIdentifier = crcFilename;
	}
	static bool Less( const TrackedFile_t& lhs, const TrackedFile_t& rhs )
	{
		if ( lhs.m_crcIdentifier < rhs.m_crcIdentifier )
			return true;
		if ( lhs.m_crcIdentifier > rhs.m_crcIdentifier )
			return false;
		if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
			return true;
		if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
			return false;
		int nCmp = V_strcmp( lhs.m_filename.c_str(), rhs.m_filename.c_str() );
		if ( nCmp < 0 )
			return true;
		if ( nCmp > 0 )
			return false;
		nCmp = V_strcmp( lhs.m_path.c_str(), rhs.m_path.c_str() );
		if ( nCmp < 0 )
			return true;
		return false;
	}
	bool GetCRCValues( FileHash_t *pFileHash );

	void ProcessFileRead( void *dest, std::size_t nBytesRead );


};


struct FileInVPK_t
{
	FileInVPK_t()
	{
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
		m_nFileOffset = 0;
	}
	int m_PackFileID;
	int m_nPackFileNumber;
	int m_nFileOffset;
	int m_idxAllOpenedFiles;

	struct Less
	{
		bool operator()( const FileInVPK_t& lhs, const FileInVPK_t& rhs ) const
		{
			if ( lhs.m_nFileOffset < rhs.m_nFileOffset )
				return true;
			if ( lhs.m_nFileOffset > rhs.m_nFileOffset )
				return false;
			if ( lhs.m_nPackFileNumber < rhs.m_nPackFileNumber )
				return true;
			if ( lhs.m_nPackFileNumber > rhs.m_nPackFileNumber )
				return false;
			if ( lhs.m_PackFileID < rhs.m_PackFileID )
				return true;
			if ( lhs.m_PackFileID > rhs.m_PackFileID )
				return false;
			return false;
		}
	};

};


struct TrackedVPKFile_t
{
	TrackedVPKFile_t()
	{
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
	}
	int m_PackFileID;
	int m_nPackFileNumber;
	int m_nFileFraction;
	int m_idxAllOpenedFiles;

	struct Less
	{
		bool operator()( const TrackedVPKFile_t& lhs, const TrackedVPKFile_t& rhs ) const
		{
			if ( lhs.m_nPackFileNumber < rhs.m_nPackFileNumber )
				return true;
			if ( lhs.m_nPackFileNumber > rhs.m_nPackFileNumber )
				return false;
			if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
				return true;
			if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
				return false;
			if ( lhs.m_PackFileID < rhs.m_PackFileID )
				return true;
			if ( lhs.m_PackFileID > rhs.m_PackFileID )
				return false;
			return false;
		}
	};

};

class StuffToMD5_t
{
public:
	uint8 *m_pubBuffer;
	int m_cubBuffer;
	MD5Value_t m_md5Value;
	int m_PackFileID;
	int m_nPackFileNumber;
	int m_nPackFileFraction;
	int m_idxListSubmittedJobs;
};

class SubmittedMd5Job_t
{
public:
	bool m_bFinished;
	MD5Value_t m_md5Value;
};

class CFileTracker2
	: public IThreadedFileMD5Processor
{
public:
	CFileTracker2( CBaseFileSystem *pFileSystem );
	~CFileTracker2();
	void InitAsyncThread();
	void ShutdownAsync();

	void MarkAllCRCsUnverified();
	int	GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles );
	EFileCRCStatus CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash );

	unsigned ThreadedProcessMD5Requests();
	int SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction );
	bool			BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );
	bool			IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );

	int NotePackFileOpened( const char *pRawFileName, const char *pFilename, const char *pPathID, int64 nLength );
	void NotePackFileAccess( const char *pFilename, const char *pPathID, CPackedStoreFileHandle &VPKHandle );
	void NotePackFileRead( CPackedStoreFileHandle &VPKHandle, void *pBuffer, int nReadLength );
	void AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &fhandle );

	void NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, std::FILE *fp, int64 nLength );
	void RecordFileSeek( std::FILE *fp, int64 pos, int seekType );
	void RecordFileClose( std::FILE *fp );
	void RecordFileRead( void *dest, std::size_t nBytesRead, std::size_t nBytesRequested, std::FILE *fp );
	int ListOpenedFiles( bool bListAll, const char *pchFilenameFind, bool bListRecentFiles );

private:
	int IdxFileFromName( const char *pFilename, const char *pPathID, int nFileFraction, int64 nLength, bool bPackOrVPKFile, bool bRecordInRecentList );

	static const int k_nFileFractionSize = 0x00100000; // 1 MB
	static const int k_nFileFractionMask = 0xFFF00000; // 1 MB

	// Orders file indices by their TrackedFile_t; also accepts a probe TrackedFile_t.
	struct OpenedFileLess
	{
		using is_transparent = void;
		const std::deque< TrackedFile_t > *m_pFiles;
		const TrackedFile_t &At( int i ) const					{ return (*m_pFiles)[i]; }
		const TrackedFile_t &At( const TrackedFile_t &f ) const	{ return f; }
		template < class A, class B >
		bool operator()( const A &a, const B &b ) const			{ return TrackedFile_t::Less( At( a ), At( b ) ); }
	};

	int FindOpenedFile( const TrackedFile_t &probe ) const
	{
		auto it = m_OpenedFilesByName.find( probe );
		return it == m_OpenedFilesByName.end() ? -1 : *it;
	}
	int InsertOpenedFile( const TrackedFile_t &file )
	{
		int idx = (int)m_AllOpenedFiles.size();
		m_AllOpenedFiles.push_back( file );
		m_OpenedFilesByName.insert( idx );
		return idx;
	}

	// Index + 1 is the VPK m_PackFileID that goes over the network; files are never removed.
	std::deque< TrackedFile_t > m_AllOpenedFiles;
	std::set< int, OpenedFileLess > m_OpenedFilesByName{ OpenedFileLess{ &m_AllOpenedFiles } };
	std::map< std::FILE *, int > m_mapAllOpenFiles; // points into m_AllOpenedFiles

	std::set< FileInVPK_t, FileInVPK_t::Less > m_treeFileInVPK;

	std::set< TrackedVPKFile_t, TrackedVPKFile_t::Less > m_treeTrackedVPKFiles;

	std::deque< int > m_RecentFileList; // points into m_AllOpenedFiles
	int m_cMissedReads;
	CBaseFileSystem					*m_pFileSystem;
	std::recursive_mutex				m_Mutex;	// Threads call into here, so we need to be safe.

	bool m_bComputeFileHashes;
	CThreadEvent m_threadEventWorkToDo;
	CThreadEvent m_threadEventWorkCompleted;
	std::atomic<bool> m_bThreadShouldRun;
	ThreadHandle_t m_hWorkThread;

	CTSQueue< StuffToMD5_t >				m_PendingJobs;
	CTSQueue< StuffToMD5_t >				m_CompletedJobs;
	ks::slot_pool< SubmittedMd5Job_t >		m_SubmittedJobs;	// handle + 1 is the request id handed out

	// just stats
	int m_cThreadBlocks;
};
