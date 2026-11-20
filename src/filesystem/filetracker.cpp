// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "basefilesystem.h"
import std;
import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.tier1.checksum_crc;
import ks.tier1.checksum_md5;
import ks.tier1.keyvalues;
import ks.tier1.string_ops;
import ks.tier1.strtools;

//-----------------------------------------------------------------------------
// CFileTracker.
//-----------------------------------------------------------------------------

CFileTracker::CFileTracker( CBaseFileSystem *pFileSystem )
{
	m_pFileSystem = pFileSystem;
}

CFileTracker::~CFileTracker()
{
	Clear();
}

void CFileTracker::NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, FileHandle_t fp )
{
	std::scoped_lock lock( m_Mutex );

	if ( !pPathID )
		pPathID = "";
	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pInfo = pPath->FindFileInfo( pFilename );

	if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
	{
		if ( pInfo )
			Warning( "(Duplicate): [%s]\\%s", pPathID, pFilename );
		else
			Warning( "(Unique   ): [%s]\\%s", pPathID, pFilename );
	}

	if ( pInfo )
	{
		// Clear all the flags, but remember if we ever had a CRC.
		pInfo->m_Flags &= k_eFileFlagsGotCRCOnce;
		pInfo->m_Flags &= ~k_eFileFlagsFailedToLoadLastTime;
	}
	else
	{
		pInfo = pPath->AddFileInfo( pFilename );
		pInfo->m_Flags = k_eFileFlags_None;
	}
	
	if ( !fp )
	{
		if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
		{
			Warning( "\n" );
		}

		return;
	}

	
	// Remember that we calculated the CRC and that it is unverified.
	pInfo->m_CRC = CalculateCRCForFile( fp );
	pInfo->m_Flags |= k_eFileFlagsHasCRC | k_eFileFlagsGotCRCOnce;
	if ( pInfo->m_iNeedsVerificationListIndex == -1 )
		AddToNeedsVerificationList( pInfo );

	if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
	{
		Warning( " - %u\n", pInfo->m_CRC );
	}
}

void CFileTracker::NoteFileFailedToLoad( const char *pFilename, const char *pPathID )
{
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pInfo = pPath->FindFileInfo( pFilename );
	if ( pInfo )
	{
		pInfo->m_Flags |= k_eFileFlagsFailedToLoadLastTime;
	}
}

CRC32_t CFileTracker::CalculateCRCForFile( FileHandle_t fp )
{	
	CRC32_t crc;
	
	// Calculate the CRC.
	unsigned int initialFilePos = m_pFileSystem->Tell( fp );
	m_pFileSystem->Seek( fp, 0, FILESYSTEM_SEEK_HEAD );

	constexpr auto CRC_CHUNK_SIZE = (32*1024);
	char tempBuf[CRC_CHUNK_SIZE];

	CRC32_Init( &crc );
	
	unsigned int fileLength = m_pFileSystem->Size( fp );
		
	int nChunks = fileLength / CRC_CHUNK_SIZE + 1;
	unsigned int curStartByte = 0;
	for ( int iChunk=0; iChunk < nChunks; iChunk++ )
	{
		int curEndByte = std::min( curStartByte + CRC_CHUNK_SIZE, fileLength );
		int chunkLen = curEndByte - curStartByte;
		if ( chunkLen == 0 )
			break;
			
		m_pFileSystem->Read( tempBuf, chunkLen, fp ); // TODO: handle errors here..
		CRC32_ProcessBuffer( &crc, tempBuf, chunkLen );
		
		curStartByte += CRC_CHUNK_SIZE;
	}
	CRC32_Final( &crc );

	// Go back to where we started.
	m_pFileSystem->Seek( fp, initialFilePos, FILESYSTEM_SEEK_HEAD );
	return crc;
}

CFileInfo* CFileTracker::GetFileInfo( const char *pFilename, const char *pPathID )
{
	std::scoped_lock lock( m_Mutex );
	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID, false );
	if ( !pPath )
		return nullptr;

	return pPath->FindFileInfo( pFilename );
}

int CFileTracker::GetFileInfos( CFileInfo **ppFileInfos, int nMaxFileInfos, const char *pFilename )
{
	std::scoped_lock lock( m_Mutex );

	int nOut = 0;
	for ( auto &[pathID, pPath] : m_PathIDs )
	{
		CFileInfo *pCur = pPath->FindFileInfo( pFilename );
		if ( pCur )
		{
			if ( nOut < nMaxFileInfos )
			{
				ppFileInfos[nOut++] = pCur;
			}
			else
			{
				contract_assert( !"CFileTracker::GetFileInfos - overflowed list!" );
			}
		}
	}

	return nOut;
}

void CFileTracker::CalculateMissingCRCs( IFileList *pWantCRCList )
{
	// First build a list of files that need a CRC and don't have one.
	m_Mutex.lock();
	std::vector< CFileInfo * > needCRCList;

	for ( auto &[pathID, pPath] : m_PathIDs )
	{
		for ( auto &[filename, pOwnedInfo] : pPath->m_Files )
		{
			CFileInfo *pInfo = pOwnedInfo.get();
			
			if ( !( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
			{
				// If the new "force match" list doesn't care whether the file has a CRC or not, then don't bother to calculate it.
				if ( !pWantCRCList->IsFileInList( pInfo->GetFilename() ) )
					continue;
				
				needCRCList.push_back( pInfo );
			}
		}
	}

	m_Mutex.unlock();
	
	// Then, when the mutex is not locked, go generate the CRCs for them.
	for ( CFileInfo *pInfo : needCRCList )
	{
		CalculateMissingCRC( pInfo->GetFilename(), pInfo->GetPathIDString() );
	}
}


void CFileTracker::CacheFileCRC( const char *pPathID, const char *pRelativeFilename )
{
	contract_assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pInfo = GetFileInfo( pRelativeFilename, pPathID );
	if ( !pInfo )
	{
		CalculateMissingCRC( pRelativeFilename, pPathID );
		pInfo = GetFileInfo( pRelativeFilename, pPathID );
	}
	
	if ( !pInfo )
		return;
	
	// Already cached a CRC for this file?
	if ( !( pInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
	{
		// Ok, it's from disk but we don't have the CRC.
		CalculateMissingCRC( pInfo->GetFilename(), pInfo->GetPathIDString() );
	}
}


void CFileTracker::CacheFileCRC_Copy( const char *pPathID, const char *pRelativeFilename, const char *pPathIDToCopyFrom )
{
	contract_assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pSourceInfo = GetFileInfo( pRelativeFilename, pPathIDToCopyFrom );
	if ( !pSourceInfo || !( pSourceInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
	{
		// Strange, we don't have a CRC for the one they wanted to copy from, so calculate that CRC.
		CacheFileCRC( pPathIDToCopyFrom, pRelativeFilename );
		if ( !( pSourceInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
		{
			// Still didn't get it. Ok.. well get a CRC for the target one anyway.
			CacheFileCRC( pPathID, pRelativeFilename );
			return;
		}
	}

	// Setup a CFileInfo for the target..	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pDestInfo = pPath->FindFileInfo( pRelativeFilename );
	if ( !pDestInfo )
		pDestInfo = pPath->AddFileInfo( pRelativeFilename );

	pDestInfo->m_CRC = pSourceInfo->m_CRC;
	pDestInfo->m_Flags = pSourceInfo->m_Flags;
}


EFileCRCStatus CFileTracker::CheckCachedFileCRC( const char *pPathID, const char *pRelativeFilename, CRC32_t *pCRC )
{
	contract_assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pInfo = GetFileInfo( pRelativeFilename, pPathID );
	if ( pInfo && (pInfo->m_Flags & k_eFileFlagsGotCRCOnce) )
	{
		*pCRC = pInfo->m_CRC;
		return k_eFileCRCStatus_GotCRC;
	}
	else
	{
		return k_eFileCRCStatus_CantOpenFile;
	}
}



void CFileTracker::CalculateMissingCRC( const char *pFilename, const char *pPathID )
{
	//
	// Cache off this file's flags and restore it after the FindFileInSearchPaths call.
	//
	// This works around an exploit where they do this:
	//		- Run a (local) listen server and load a hacked material
	//		- Disconnect
	//		- Delete (or rename) the hacked material file
	//		- Connect to a server using sv_pure 1 and allow_from_disk+check_crc on the hacked file
	//
	// What happens is that it comes through here (CalculateMissingCRC) after getting the server's whitelist.
	// Then it calls FindFileInSearchPaths below, which can modify the flags of the file so that
	// it doesn't give that file to the server for the CRC check in GetUnverifiedCRCFiles.
	//
	// By preserving the flags here and not letting FindFileInSearchPaths modify it, we make sure that
	// we remember that the hacked file was loaded from disk.
	//
	int nOldFlags = -1;
	CFileInfo *pInfo = GetFileInfo( pFilename, pPathID );
	if ( pInfo )
		nOldFlags = pInfo->m_Flags;

	// Force it to make a CRC of disk files.
	FileHandle_t fh = m_pFileSystem->FindFileInSearchPaths( pFilename, "rb", pPathID, FSOPEN_FORCE_TRACK_CRC, nullptr, true );
	if ( !fh )
		return;

	if ( pInfo )
	{
		// Restore the flags (see above for a description of why we do this).
		if ( nOldFlags != -1 )
			pInfo->m_Flags = nOldFlags;

		// Now we're about to modify the file itself.. lock the mutex.
		std::scoped_lock lock( m_Mutex );

		// The FindFileInSearchPaths call might have done the CRC for us.
		if ( !( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
		{
			pInfo->m_CRC = CalculateCRCForFile( fh );
			pInfo->m_Flags |= k_eFileFlagsHasCRC | k_eFileFlagsGotCRCOnce;
			if ( pInfo->m_iNeedsVerificationListIndex == -1 )
			{
				AddToNeedsVerificationList( pInfo );
			}
		}
	}
	else
	{
		contract_assert( false );
	}		

	m_pFileSystem->Close( fh );
}


void CFileTracker::MarkAllCRCsUnverified()
{
	std::scoped_lock lock( m_Mutex );

	// First clear the 'needs verification' list.
	MarkAllCRCsVerified();

	contract_assert( m_NeedsVerificationList.empty() );
	for ( auto &[pathID, pPath] : m_PathIDs )
	{
		for ( auto &[filename, pOwnedInfo] : pPath->m_Files )
		{
			CFileInfo *pInfo = pOwnedInfo.get();
			
			if ( ( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
			{
				AddToNeedsVerificationList( pInfo );
			}
		}
	}
}


void CFileTracker::MarkAllCRCsVerified( bool bLockMutex )
{
	if ( bLockMutex )
		m_Mutex.lock();

	for ( CFileInfo *pInfo : m_NeedsVerificationList )
	{
		pInfo->m_iNeedsVerificationListIndex = -1;
	}
	
	m_NeedsVerificationList.clear();
	m_NeedsVerificationList.shrink_to_fit();

	if ( bLockMutex )
		m_Mutex.unlock();
}


void CFileTracker::Clear()
{
	std::scoped_lock lock( m_Mutex );

	m_PathIDs.clear();
}

CPathIDFileList* CFileTracker::GetPathIDFileList( const char *pPathID, bool bAutoAdd )
{
	std::scoped_lock lock( m_Mutex );

	if ( !pPathID )
		pPathID = "";
		
	auto i = m_PathIDs.find( pPathID );
	if ( i == m_PathIDs.end() )
	{
		if ( bAutoAdd )
		{
			auto pPath = std::make_unique< CPathIDFileList >();
			pPath->m_PathID = pPathID;
			return m_PathIDs.emplace( pPathID, std::move( pPath ) ).first->second.get();
		}
		else
		{
			return nullptr;
		}
	}
	else
	{
		return i->second.get();
	}
}

//-----------------------------------------------------------------------------
// CFileInfo implementation.
//-----------------------------------------------------------------------------

CFileInfo::CFileInfo()
{
	m_iNeedsVerificationListIndex = -1;
}

CFileInfo::~CFileInfo()
{
}


//-----------------------------------------------------------------------------
// CPathIDFileList implementation..
//-----------------------------------------------------------------------------

CPathIDFileList::CPathIDFileList()
{
}

CPathIDFileList::~CPathIDFileList()
{
}

CFileInfo* CPathIDFileList::FindFileInfo( const char *pFilename )
{
	contract_assert( !V_IsAbsolutePath( pFilename ) );

	auto i = m_Files.find( pFilename );
	if ( i == m_Files.end() )
		return nullptr;
	else
		return i->second.get();
}

CFileInfo* CPathIDFileList::AddFileInfo( const char *pFilename )
{
	contract_assert( !V_IsAbsolutePath( pFilename ) );
	contract_assert( !m_Files.contains( pFilename ) );
	
	auto it = m_Files.emplace( pFilename, std::make_unique< CFileInfo >() ).first;
	CFileInfo *pFileInfo = it->second.get();
	pFileInfo->m_pPathIDFileList = this;
	pFileInfo->m_pFilename = it->first.c_str();
	return pFileInfo;
}


uintp ThreadStubProcessMD5Requests( void *pParam )
{
	return ((CFileTracker2 *)pParam)->ThreadedProcessMD5Requests();
}


//-----------------------------------------------------------------------------
// ThreadedProcessMD5Requests
// Calculate the MD5s of all the blocks submitted to us
//-----------------------------------------------------------------------------
unsigned CFileTracker2::ThreadedProcessMD5Requests()
{
	while ( m_bThreadShouldRun )
	{
		StuffToMD5_t stuff;
		while ( m_PendingJobs.PopItem( &stuff ) )
		{

			MD5Context_t ctx;
			std::memset(&ctx, 0, sizeof(MD5Context_t));
			MD5Init(&ctx);
			MD5Update(&ctx, stuff.m_pubBuffer, stuff.m_cubBuffer );
			MD5Final( stuff.m_md5Value.bits, &ctx);
			TrackedVPKFile_t trackedVPKFileFind;
			trackedVPKFileFind.m_nPackFileNumber	= stuff.m_nPackFileNumber;
			trackedVPKFileFind.m_PackFileID			= stuff.m_PackFileID;
			trackedVPKFileFind.m_nFileFraction		= stuff.m_nPackFileFraction;

			{
				// update the FileTracker MD5 database
				std::scoped_lock lock( m_Mutex );
				auto itTrackedVPKFile = m_treeTrackedVPKFiles.find( trackedVPKFileFind );
				if ( itTrackedVPKFile != m_treeTrackedVPKFiles.end() )
				{
					TrackedFile_t &trackedfile = m_AllOpenedFiles[itTrackedVPKFile->m_idxAllOpenedFiles];

					std::memcpy( trackedfile.m_filehashFinal.m_md5contents.bits, stuff.m_md5Value.bits, sizeof( trackedfile.m_filehashFinal.m_md5contents.bits ) );
					trackedfile.m_filehashFinal.m_crcIOSequence = stuff.m_cubBuffer;
					trackedfile.m_filehashFinal.m_cbFileLen = stuff.m_cubBuffer;
					trackedfile.m_filehashFinal.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
					trackedfile.m_filehashFinal.m_nPackFileNumber = trackedVPKFileFind.m_nPackFileNumber;
					trackedfile.m_filehashFinal.m_PackFileID = trackedVPKFileFind.m_PackFileID;
				}
			}
			m_CompletedJobs.PushItem( stuff );
			m_threadEventWorkCompleted.Set();
		}
		m_threadEventWorkToDo.Wait( 1000 );
	}
	return 0;
}


//-----------------------------------------------------------------------------
// SubmitThreadedMD5Request
// add pubBuffer,cubBuffer to our queue of stuff to MD5
// caller promises that the memory will remain valid
// until BlockUntilMD5RequestComplete() is called
// returns: request handle
//-----------------------------------------------------------------------------
int CFileTracker2::SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction )
{
	StuffToMD5_t stuff;
	int idxList;
	{
		std::scoped_lock lock( m_Mutex );
		if ( !m_bComputeFileHashes )
			return 0;
		int idxAllFiles = -1;
		TrackedVPKFile_t trackedVPKFileFind;
		trackedVPKFileFind.m_nPackFileNumber = nPackFileNumber;
		trackedVPKFileFind.m_PackFileID = PackFileID;
		trackedVPKFileFind.m_nFileFraction = nPackFileFraction;
		auto itTrackedVPKFile = m_treeTrackedVPKFiles.find( trackedVPKFileFind );
		if ( itTrackedVPKFile != m_treeTrackedVPKFiles.end() )
		{
			idxAllFiles = itTrackedVPKFile->m_idxAllOpenedFiles;
			// dont early out if we have already done the MD5, if the caller wants us
			// to do it again - then do it again
		}
		else
		{
			// this is an error, we should already know about the file
			return 0;
		}

		SubmittedMd5Job_t submittedjob;
		submittedjob.m_bFinished = false;
		idxList = m_SubmittedJobs.emplace( submittedjob );

		stuff.m_pubBuffer = pubBuffer;
		stuff.m_cubBuffer = cubBuffer;
		stuff.m_PackFileID = PackFileID;
		stuff.m_nPackFileNumber = nPackFileNumber;
		stuff.m_nPackFileFraction = nPackFileFraction;
		stuff.m_idxListSubmittedJobs = idxList;
	}

	// submit the work
	m_PendingJobs.PushItem( stuff );
	m_threadEventWorkToDo.Set();

	return idxList + 1;
}


//-----------------------------------------------------------------------------
// IsMD5RequestComplete
// is request identified by iRequest finished?
// ( the caller wants to free the memory, but now must wait until we finish
// calculating the MD5 )
//-----------------------------------------------------------------------------
bool CFileTracker2::IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{
	std::scoped_lock lock( m_Mutex );
	int idxListWaiting = iRequest - 1;

	// deal with all completed jobs
	StuffToMD5_t stuff;
	while ( m_CompletedJobs.PopItem( &stuff ) )
	{
		int idxList = stuff.m_idxListSubmittedJobs;
		V_memcpy( &m_SubmittedJobs[idxList].m_md5Value, &stuff.m_md5Value, sizeof( MD5Value_t ) );
		m_SubmittedJobs[idxList].m_bFinished = true;
	}

	// did the one we wanted finish?
	if ( m_SubmittedJobs[idxListWaiting].m_bFinished )
	{
		V_memcpy( pMd5ValueOut, &m_SubmittedJobs[idxListWaiting].m_md5Value, sizeof( MD5Value_t ) );
		// you can not ask again, we have removed it from the list
		m_SubmittedJobs.erase(idxListWaiting);
		return true;
	}
	// not done yet
	return false;
}

//-----------------------------------------------------------------------------
// BlockUntilMD5RequestComplete
// block until request identified by iRequest is finished
// ( the caller wants to free the memory, but now must wait until we finish
// calculating the MD5 )
//-----------------------------------------------------------------------------
bool CFileTracker2::BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{
	while ( 1 )
	{
		if ( IsMD5RequestComplete( iRequest, pMd5ValueOut ) )
			return true;
		m_cThreadBlocks++;
		m_threadEventWorkCompleted.Wait( 100 );
	}
	return false;
}


// CFileTracker2 will replace most of CFileTracker soon
CFileTracker2::CFileTracker2( CBaseFileSystem *pFileSystem )
{
	m_pFileSystem = pFileSystem;
	m_cMissedReads = 0;
	m_bComputeFileHashes = true;

	m_cThreadBlocks = 0;
	m_bThreadShouldRun = true;
	m_hWorkThread = nullptr;
}


CFileTracker2::~CFileTracker2()
{
	contract_assert( !m_bThreadShouldRun );
	contract_assert( m_hWorkThread==nullptr );
}


void CFileTracker2::InitAsyncThread()
{
	contract_assert( m_hWorkThread == nullptr );
	if ( m_hWorkThread == nullptr )
		m_hWorkThread = CreateSimpleThread( ThreadStubProcessMD5Requests, this );
}


void CFileTracker2::ShutdownAsync()
{
	m_bThreadShouldRun = false;
	m_threadEventWorkToDo.Set();
	if ( m_hWorkThread )
	{
		// wait for it to die
		ThreadJoin( m_hWorkThread );
		ReleaseThreadHandle( m_hWorkThread );
		m_hWorkThread = nullptr;
	}
}


void CFileTracker2::MarkAllCRCsUnverified()
{
	std::scoped_lock lock( m_Mutex );

	m_RecentFileList.clear();

	// every m_bInRecentFileList in m_AllOpenedFiles is bad now

	// this is going to hit every file we have *ever* loaded. how do we prune this list?
	// lets send VPK files first, then everything else
	for ( int i : m_OpenedFilesByName )
	{
		TrackedFile_t &trackedfile = m_AllOpenedFiles[i];

		trackedfile.m_bInRecentFileList = false;
		// skip these for now
		if ( trackedfile.m_filehashInProgress.m_cbFileLen == 0 && trackedfile.m_filehashFinal.m_cbFileLen == 0 )
			continue;

		if ( trackedfile.m_bPackOrVPKFile )
		{
			m_RecentFileList.push_front( i );
			trackedfile.m_bInRecentFileList = true;
		}
		else if ( !trackedfile.m_bFileInVPK && !trackedfile.m_path.empty() )
		{
			m_RecentFileList.push_back( i );
			trackedfile.m_bInRecentFileList = true;
		}
	}	
	// do it again for any we skipped because we had not yet read the data
	for ( int i : m_OpenedFilesByName )
	{
		TrackedFile_t &trackedfile = m_AllOpenedFiles[i];
		if ( trackedfile.m_bInRecentFileList )
			continue;

		if ( trackedfile.m_bPackOrVPKFile || ( !trackedfile.m_bFileInVPK && !trackedfile.m_path.empty() ) )
		{
			m_RecentFileList.push_back( i );
			trackedfile.m_bInRecentFileList = true;
		}
	}	
}


int CFileTracker2::GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles )
{
	contract_assert( nMaxFiles > 0 );

	std::scoped_lock lock( m_Mutex );

	// We send all files regardless of the whitelist, let the server figure it out

	int iOutFile = 0;
	while ( !m_RecentFileList.empty() )
	{
		// pop off the head
		int idx = m_RecentFileList.front();

		TrackedFile_t &file = m_AllOpenedFiles[idx];

		// we could be in the degenerate case of a file that has been opened but we have not yet read any bytes
		if ( file.m_filehashInProgress.m_cbFileLen == 0 && file.m_filehashFinal.m_cbFileLen == 0 )
		{
			// put it back to the end of the line
			m_RecentFileList.pop_front();
			m_RecentFileList.push_back( idx );
			break;
		}

		// ok we have a good file - proceed
		m_RecentFileList.pop_front();

		// if the file has no "path" associated, then it was some raw file open that we do not care about
		if ( file.m_path.empty() )
			continue;

		file.m_bInRecentFileList = false;

		// Add this file to their list.
		CUnverifiedFileHash *pOutFile = &pFiles[iOutFile];
		V_strncpy( pOutFile->m_Filename, file.m_filename.c_str(), sizeof( pOutFile->m_Filename ) );
		V_strncpy( pOutFile->m_PathID, file.m_path.c_str(), sizeof( pOutFile->m_PathID ) );
		pOutFile->m_nFileFraction = file.m_nFileFraction;

		file.GetCRCValues( &pOutFile->m_FileHash );

		++iOutFile;
		if ( iOutFile >= nMaxFiles )
			break;
	}

	return iOutFile;
}

EFileCRCStatus CFileTracker2::CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
{
	contract_assert( ThreadInMainThread() );
	std::scoped_lock lock( m_Mutex );

	CRC32_t crcFilename;
	CRC32_Init( &crcFilename );
	CRC32_ProcessBuffer( &crcFilename, pRelativeFilename, V_strlen( pRelativeFilename ) );
	CRC32_ProcessBuffer( &crcFilename, pPathID, V_strlen( pPathID ) );
	CRC32_Final( &crcFilename );

	TrackedFile_t trackedfileFind;
	trackedfileFind.m_filename = ks::nz( pRelativeFilename );
	trackedfileFind.m_path = ks::nz( pPathID );
	trackedfileFind.m_crcIdentifier = crcFilename;
	trackedfileFind.m_nFileFraction = nFileFraction;

	int idx = FindOpenedFile( trackedfileFind );
	if ( idx != -1 )
	{
		TrackedFile_t &trackedfile = m_AllOpenedFiles[idx];
		if ( trackedfile.m_bFileInVPK )
		{
			// the FileHash is not meaningful, because the file is in a VPK, we have hashed the entire VPK
			// if the user is sending us a hash for this file, it means he has extracted it from the VPK and tricked the client into loading it
			// instead of the version in the VPK.
			return k_eFileCRCStatus_FileInVPK;
		}
		trackedfile.GetCRCValues( pFileHash );
		return k_eFileCRCStatus_GotCRC;
	}
	else
	{
		return k_eFileCRCStatus_CantOpenFile;
	}
}

bool TrackedFile_t::GetCRCValues( FileHash_t *pFileHash )
{
	if ( m_filehashFinal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile )
	{
		MD5Context_t ctx;
		V_memcpy( &ctx, &m_md5ctx, sizeof( ctx ));
		MD5Final( pFileHash->m_md5contents.bits, &ctx );
		CRC32_t crcT = m_filehashInProgress.m_crcIOSequence;
		CRC32_Final( &crcT );
		pFileHash->m_crcIOSequence = crcT;
		pFileHash->m_eFileHashType = FileHash_t::k_EFileHashTypeIncompleteFile;
		pFileHash->m_cbFileLen = m_filehashInProgress.m_cbFileLen;
		pFileHash->m_nPackFileNumber = m_filehashInProgress.m_nPackFileNumber;
		pFileHash->m_PackFileID = m_filehashInProgress.m_PackFileID;
	}
	else
	{
		V_memcpy( pFileHash->m_md5contents.bits, m_filehashFinal.m_md5contents.bits, sizeof(m_filehashFinal.m_md5contents));
		pFileHash->m_crcIOSequence = m_filehashFinal.m_crcIOSequence;
		pFileHash->m_eFileHashType = m_filehashFinal.m_eFileHashType;
		pFileHash->m_cbFileLen = m_filehashFinal.m_cbFileLen;
		pFileHash->m_nPackFileNumber = m_filehashFinal.m_nPackFileNumber;
		pFileHash->m_PackFileID = m_filehashFinal.m_PackFileID;
	}
	return true;
}

void TrackedFile_t::ProcessFileRead( void *dest, std::size_t nBytesRead )
{
	if ( m_filehashFinal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile )
	{
		m_filehashInProgress.m_cbFileLen += nBytesRead;

		// if this single read is the entire file - discard any previous partial checksum
		if ( m_nFilePos == 0 && nBytesRead == m_nLength )
		{
			std::memset(&m_md5ctx, 0, sizeof(m_md5ctx));
			MD5Init( &m_md5ctx );
		}

		MD5Update( &m_md5ctx, (unsigned char *)dest, nBytesRead );

		if ( m_nFilePos == m_cubSequentialRead )
			m_cubSequentialRead += nBytesRead;

		if (( m_nFilePos == 0 && nBytesRead == m_nLength ) ||
			( m_cubSequentialRead == m_nLength && m_filehashInProgress.m_cbFileLen == m_nLength ) )
		{
			// we have now hashed the entire file - mark it done and never touch it again
			MD5Final( m_filehashInProgress.m_md5contents.bits, &m_md5ctx );
			// its not a CRC in this case - its the actual length
			m_filehashInProgress.m_crcIOSequence = m_nLength;
			m_filehashInProgress.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
			V_memcpy( &m_filehashFinal, &m_filehashInProgress, sizeof( m_filehashFinal ) );
		}
		else
		{
			CRC32_ProcessBuffer( &m_filehashInProgress.m_crcIOSequence, &m_nFilePos, sizeof(int64) );
		}
		m_nFilePos += nBytesRead;
	}
}


void CFileTracker2::NotePackFileAccess( const char *pFilename, const char *pPathID, CPackedStoreFileHandle &VPKHandle )
{
	std::scoped_lock lock( m_Mutex );
	int idxFileVPK = VPKHandle.m_pOwner->m_PackFileID - 1;

	// we must have seen the VPK file first
	if ( !( idxFileVPK >= 0 && idxFileVPK < (int)m_AllOpenedFiles.size() ) )
		return;

	int idxFile = IdxFileFromName( pFilename, pPathID, 0, VPKHandle.m_nFileSize, false, false );
	TrackedFile_t &trackedfile = m_AllOpenedFiles[idxFile];
	// we could use the CRC data from the VPK header - and verify it
	// VPKHandle.GetFileCRCFromHeaderData();
	// for now all we are going to do is track that this file came from a VPK
	trackedfile.m_nLength = VPKHandle.m_nFileSize;
	trackedfile.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedfile.m_nPackFileNumber = VPKHandle.m_nFileNumber; // this might be useful to send up
	trackedfile.m_bFileInVPK = true;

	FileInVPK_t fiv;
	fiv.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	fiv.m_nPackFileNumber = VPKHandle.m_nFileNumber;
	fiv.m_nFileOffset = VPKHandle.m_nFileOffset;
	fiv.m_idxAllOpenedFiles = idxFile;

	m_treeFileInVPK.insert( fiv );

	TrackedVPKFile_t trackedVPKFileFind;
	trackedVPKFileFind.m_nPackFileNumber = VPKHandle.m_nFileNumber;
	trackedVPKFileFind.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedVPKFileFind.m_nFileFraction = VPKHandle.m_nFileOffset & k_nFileFractionMask;
	int nFileEnd = ( VPKHandle.m_nFileOffset+VPKHandle.m_nFileSize ) & k_nFileFractionMask;
	// if it straddles the 1MB boundary, record all
	while ( trackedVPKFileFind.m_nFileFraction <= nFileEnd )
	{
		if ( !m_treeTrackedVPKFiles.contains( trackedVPKFileFind ) )
		{
			char szDataFileName[MAX_PATH];
			VPKHandle.GetPackFileName( szDataFileName, sizeof(szDataFileName) );
			const char *pszFileName = V_GetFileName( szDataFileName );
			trackedVPKFileFind.m_idxAllOpenedFiles = IdxFileFromName( pszFileName, "GAME", trackedVPKFileFind.m_nFileFraction, 0, true, true );
			m_treeTrackedVPKFiles.insert( trackedVPKFileFind );
		}
		trackedVPKFileFind.m_nFileFraction += k_nFileFractionSize;
	}
}

// MD5 VPK files in 1MB chunks
void CFileTracker2::NotePackFileRead( CPackedStoreFileHandle &VPKHandle, void *pBuffer, int nReadLength )
{
	// This is all a no-op because we are using the VPK SubmitThreadedMD5Request API
}


// This is used by the dedicated server
void CFileTracker2::AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &VPKHandle )
{
	std::scoped_lock lock( m_Mutex );
	m_bComputeFileHashes = false; // since we trust the hashes given here, do not recompute them later
	char szDataFileName[MAX_PATH];
	VPKHandle.m_nFileNumber = nPackFileNumber;
	VPKHandle.GetPackFileName( szDataFileName, sizeof(szDataFileName) );
	const char *pszFileName = V_GetFileName( szDataFileName );

	TrackedVPKFile_t trackedVPKFile;
	trackedVPKFile.m_nPackFileNumber = VPKHandle.m_nFileNumber;
	trackedVPKFile.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedVPKFile.m_nFileFraction = nFileFraction;
	int idxAllFiles = trackedVPKFile.m_idxAllOpenedFiles = IdxFileFromName( pszFileName, "GAME", nFileFraction, 0, true, true );
	if ( idxAllFiles != -1 )
	{
		// an existing record already maps this key to the same file
		m_treeTrackedVPKFiles.insert( trackedVPKFile );
	}

	TrackedFile_t &trackedfile = m_AllOpenedFiles[idxAllFiles];
	trackedfile.m_bFileInVPK = false;
	trackedfile.m_bPackOrVPKFile = true;
	trackedfile.m_nLength = k_nFileFractionSize;
	trackedfile.m_filehashFinal.m_cbFileLen = cbFileLen;
	trackedfile.m_filehashFinal.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
	trackedfile.m_filehashFinal.m_nPackFileNumber = nPackFileNumber;
	trackedfile.m_filehashFinal.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedfile.m_filehashFinal.m_crcIOSequence = cbFileLen;
	V_memcpy( trackedfile.m_filehashFinal.m_md5contents.bits, md5.bits, sizeof( md5.bits) );
}

int CFileTracker2::IdxFileFromName( const char *pFilename, const char *pPathID, int nFileFraction, int64 nLength, bool bPackOrVPKFile, bool bRecordInRecentList )
{
	TrackedFile_t trackedfile;
	trackedfile.RebuildFileName( pFilename, pPathID );
	trackedfile.m_nLength = nLength;
	trackedfile.m_bPackOrVPKFile = bPackOrVPKFile;
	trackedfile.m_nFileFraction = nFileFraction;
	MD5Init( &trackedfile.m_md5ctx );
	CRC32_Init( &trackedfile.m_filehashInProgress.m_crcIOSequence );

	trackedfile.m_bInRecentFileList = false;

	int idxFile = FindOpenedFile( trackedfile );
	if ( idxFile == -1 )
	{
		idxFile = InsertOpenedFile( trackedfile );
		if ( bRecordInRecentList )
		{
			m_RecentFileList.push_back( idxFile );
			m_AllOpenedFiles[idxFile].m_bInRecentFileList = true;
		}
	}
	else
	{
		if ( bRecordInRecentList )
		{
			if ( !m_AllOpenedFiles[idxFile].m_bInRecentFileList )
			{
				m_RecentFileList.push_back( idxFile );
				m_AllOpenedFiles[idxFile].m_bInRecentFileList = true;
			}
		}
	}
	return idxFile;
}

int CFileTracker2::NotePackFileOpened( const char *pRawFileName, const char *pFilename, const char *pPathID, int64 nLength )
{
	std::scoped_lock lock( m_Mutex );
	TrackedFile_t trackedfileToFind;
	trackedfileToFind.RebuildFileName( pRawFileName, nullptr );
	int idxFile = FindOpenedFile( trackedfileToFind );
	if ( idxFile != -1 )
	{
		TrackedFile_t &trackedfile = m_AllOpenedFiles[idxFile];
		// we have the real name we want to use. correct the name
		m_OpenedFilesByName.erase( idxFile );		// re-keyed below
		trackedfile.RebuildFileName( pFilename, pPathID );
		trackedfile.m_bPackOrVPKFile = true;
		trackedfile.m_PackFileID = idxFile + 1;
		trackedfile.m_filehashFinal.m_PackFileID = trackedfile.m_PackFileID;
		trackedfile.m_filehashFinal.m_nPackFileNumber = -1;
		trackedfile.m_filehashInProgress.m_PackFileID = trackedfile.m_PackFileID;
		trackedfile.m_filehashInProgress.m_nPackFileNumber = -1;
		m_OpenedFilesByName.insert( idxFile );
	}
	return idxFile + 1;
}

void CFileTracker2::NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, std::FILE *fp, int64 nLength )
{
	std::scoped_lock lock( m_Mutex );

	int idxFile = IdxFileFromName( pFilename, pPathID, 0, nLength, false, true );
	m_mapAllOpenFiles.insert_or_assign( fp, idxFile );	// a reused FILE * now refers to this file
}

void CFileTracker2::RecordFileClose( std::FILE *fp )
{
	//VPROF_BUDGET("CFileTracker2::RecordFileClose", "PureFileTracker2");
	std::scoped_lock lock( m_Mutex );

	auto idx = m_mapAllOpenFiles.find( fp );
	if ( idx != m_mapAllOpenFiles.end() )
	{
		int idx2 = idx->second;
		TrackedFile_t &trackedfile = m_AllOpenedFiles[idx2];

		// don't touch the CRCs, we will just keep CRCing every byte we load, even if we open/close the file multiple times
		// note that if we successfully read the entire file - it is in m_crcFinal
		trackedfile.m_nFilePos = 0;
		trackedfile.m_cubSequentialRead = 0;

		m_mapAllOpenFiles.erase( idx );
	}
}

void CFileTracker2::RecordFileSeek( std::FILE *fp, int64 pos, int seekType )
{
	std::scoped_lock lock( m_Mutex );
	auto idx = m_mapAllOpenFiles.find( fp );
	if ( idx != m_mapAllOpenFiles.end() )
	{
		int idx2 = idx->second;
		if ( idx2 != -1 )
		{
			TrackedFile_t &trackedfile = m_AllOpenedFiles[idx2];
			// if we have hashed the entire file already - we don't need to do it again
			if ( trackedfile.m_filehashFinal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile )
			{
				if ( seekType == FILESYSTEM_SEEK_HEAD )
					trackedfile.m_nFilePos = pos;
				else if ( seekType == FILESYSTEM_SEEK_TAIL )
					trackedfile.m_nFilePos = trackedfile.m_nLength - pos;
				else if ( seekType == FILESYSTEM_SEEK_CURRENT )
					trackedfile.m_nFilePos = trackedfile.m_nFilePos + pos;

				CRC32_ProcessBuffer( &trackedfile.m_filehashInProgress.m_crcIOSequence, &pos, sizeof(int64) );
				CRC32_ProcessBuffer( &trackedfile.m_filehashInProgress.m_crcIOSequence, &seekType, sizeof(int) );
			}
		}
	}
}


void CFileTracker2::RecordFileRead( void *dest, std::size_t nBytesRead, std::size_t nBytesRequested, std::FILE *fp )
{

	//VPROF_BUDGET("CFileTracker2::RecordFileRead", "PureFileTracker2");
	std::scoped_lock lock( m_Mutex );

	auto idx = m_mapAllOpenFiles.find( fp );
	if ( idx != m_mapAllOpenFiles.end() )
	{
		int idx2 = idx->second;
		if ( idx2 != -1 )
		{
			TrackedFile_t &trackedfile = m_AllOpenedFiles[idx2];
			trackedfile.ProcessFileRead( dest, nBytesRead );
		}
	}
	else
	{
		m_cMissedReads++;
	}
}

int CFileTracker2::ListOpenedFiles( bool bListAll, const char *pchFilenameFind, bool bRecentFileList )
{
	std::scoped_lock lock( m_Mutex );

	int cPackFiles = 0;
	if ( !bRecentFileList )
	{
		for ( int i : m_OpenedFilesByName )
		{
			TrackedFile_t &file = m_AllOpenedFiles[i];

			if ( file.m_PackFileID )
				cPackFiles++;
			if ( bListAll || ( pchFilenameFind && V_stristr( file.m_filename.c_str(), pchFilenameFind ) ) )
			{
				Msg( "FileTracker %s ( %d, %d, %d ) %d: %d %d\n", 
					file.m_filename.c_str(), file.m_PackFileID, file.m_nPackFileNumber, 0, 
					file.m_filehashInProgress.m_eFileHashType, file.m_filehashFinal.m_cbFileLen, file.m_cubTotalRead );
			}
		}
	}
	else
	{
		for ( int idx : m_RecentFileList )
		{

			TrackedFile_t &file = m_AllOpenedFiles[idx];

			if ( file.m_PackFileID )
				cPackFiles++;
			if ( bListAll || ( pchFilenameFind && V_stristr( file.m_filename.c_str(), pchFilenameFind ) ) )
			{
				Msg( "FileTracker %s ( %d, %d, %d ) %d: %d %d\n", 
					file.m_filename.c_str(), file.m_PackFileID, file.m_nPackFileNumber, 0, 
					file.m_filehashInProgress.m_eFileHashType, file.m_filehashFinal.m_cbFileLen, file.m_cubTotalRead );
			}
		}
	}

	Msg( "FileTracker: %d files %d VPK files\n", (int)m_AllOpenedFiles.size(), cPackFiles );
	return (int)m_AllOpenedFiles.size();
}
