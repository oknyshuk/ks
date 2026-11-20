import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.tier1.checksum_crc;
import ks.tier1.checksum_md5;
import ks.tier1.filename_table;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IBaseFileSystem, IFileSystem, IIoStats and IThreadedFileMD5Processor classes to their
// one implementation. Generated from the interface declarations; included by basefilesystem.cpp.

// ---- IBaseFileSystem -> CBaseFileSystem
int IBaseFileSystem::Read( void* pOutput, int size, FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->Read( pOutput, size, file ); }
int IBaseFileSystem::Write( void const* pInput, int size, FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->Write( pInput, size, file ); }
FileHandle_t IBaseFileSystem::Open( const char *pFileName, const char *pOptions, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->Open( pFileName, pOptions, pathID ); }
void IBaseFileSystem::Close( FileHandle_t file )
{ static_cast<CBaseFileSystem*>(this)->Close( file ); }
void IBaseFileSystem::Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType )
{ static_cast<CBaseFileSystem*>(this)->Seek( file, pos, seekType ); }
unsigned int IBaseFileSystem::Tell( FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->Tell( file ); }
unsigned int IBaseFileSystem::Size( FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->Size( file ); }
unsigned int IBaseFileSystem::Size( const char *pFileName, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->Size( pFileName, pPathID ); }
void IBaseFileSystem::Flush( FileHandle_t file )
{ static_cast<CBaseFileSystem*>(this)->Flush( file ); }
bool IBaseFileSystem::Precache( const char *pFileName, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->Precache( pFileName, pPathID ); }
bool IBaseFileSystem::FileExists( const char *pFileName, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->FileExists( pFileName, pPathID ); }
bool IBaseFileSystem::IsFileWritable( char const *pFileName, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->IsFileWritable( pFileName, pPathID ); }
bool IBaseFileSystem::SetFileWritable( char const *pFileName, bool writable, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->SetFileWritable( pFileName, writable, pPathID ); }
long IBaseFileSystem::GetFileTime( const char *pFileName, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->GetFileTime( pFileName, pPathID ); }
std::optional< std::vector< std::byte > > IBaseFileSystem::ReadFile( const char *pFileName, const char *pPath, int nMaxBytes, int nStartingByte )
{ return static_cast<CBaseFileSystem*>(this)->ReadFile( pFileName, pPath, nMaxBytes, nStartingByte ); }
bool IBaseFileSystem::WriteFile( const char *pFileName, const char *pPath, std::span< const std::byte > data )
{ return static_cast<CBaseFileSystem*>(this)->WriteFile( pFileName, pPath, data ); }
bool IBaseFileSystem::UnzipFile( const char *pFileName, const char *pPath, const char *pDestination )
{ return static_cast<CBaseFileSystem*>(this)->UnzipFile( pFileName, pPath, pDestination ); }

// ---- IFileSystem -> CBaseFileSystem
void IFileSystem::AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType )
{ static_cast<CBaseFileSystem*>(this)->AddSearchPath( pPath, pathID, addType ); }
bool IFileSystem::RemoveSearchPath( const char *pPath, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->RemoveSearchPath( pPath, pathID ); }
void IFileSystem::RemoveAllSearchPaths( void )
{ static_cast<CBaseFileSystem*>(this)->RemoveAllSearchPaths(); }
void IFileSystem::RemoveSearchPaths( const char *szPathID )
{ static_cast<CBaseFileSystem*>(this)->RemoveSearchPaths( szPathID ); }
void IFileSystem::MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly )
{ static_cast<CBaseFileSystem*>(this)->MarkPathIDByRequestOnly( pPathID, bRequestOnly ); }
bool IFileSystem::IsFileInReadOnlySearchPath( const char *pPath, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->IsFileInReadOnlySearchPath( pPath, pathID ); }
const char * IFileSystem::RelativePathToFullPath( const char *pFileName, const char *pPathID, char *pLocalPath, int localPathBufferSize, PathTypeFilter_t pathFilter, PathTypeQuery_t *pPathType )
{ return static_cast<CBaseFileSystem*>(this)->RelativePathToFullPath( pFileName, pPathID, pLocalPath, localPathBufferSize, pathFilter, pPathType ); }
int IFileSystem::GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen )
{ return static_cast<CBaseFileSystem*>(this)->GetSearchPath( pathID, bGetPackFiles, pPath, nMaxLen ); }
bool IFileSystem::AddPackFile( const char *fullpath, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->AddPackFile( fullpath, pathID ); }
void IFileSystem::RemoveFile( char const* pRelativePath, const char *pathID )
{ static_cast<CBaseFileSystem*>(this)->RemoveFile( pRelativePath, pathID ); }
bool IFileSystem::RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->RenameFile( pOldPath, pNewPath, pathID ); }
void IFileSystem::CreateDirHierarchy( const char *path, const char *pathID )
{ static_cast<CBaseFileSystem*>(this)->CreateDirHierarchy( path, pathID ); }
bool IFileSystem::IsDirectory( const char *pFileName, const char *pathID )
{ return static_cast<CBaseFileSystem*>(this)->IsDirectory( pFileName, pathID ); }
void IFileSystem::FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime )
{ static_cast<CBaseFileSystem*>(this)->FileTimeToString( pStrip, maxCharsIncludingTerminator, fileTime ); }
void IFileSystem::SetBufferSize( FileHandle_t file, unsigned nBytes )
{ static_cast<CBaseFileSystem*>(this)->SetBufferSize( file, nBytes ); }
bool IFileSystem::IsOk( FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->IsOk( file ); }
bool IFileSystem::EndOfFile( FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->EndOfFile( file ); }
char * IFileSystem::ReadLine( char *pOutput, int maxChars, FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->ReadLine( pOutput, maxChars, file ); }
CSysModule * IFileSystem::LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly )
{ return static_cast<CBaseFileSystem*>(this)->LoadModule( pFileName, pPathID, bValidatedDllOnly ); }
void IFileSystem::UnloadModule( CSysModule *pModule )
{ static_cast<CBaseFileSystem*>(this)->UnloadModule( pModule ); }
const char * IFileSystem::FindFirst( const char *pWildCard, FileFindHandle_t *pHandle )
{ return static_cast<CBaseFileSystem*>(this)->FindFirst( pWildCard, pHandle ); }
const char * IFileSystem::FindNext( FileFindHandle_t handle )
{ return static_cast<CBaseFileSystem*>(this)->FindNext( handle ); }
bool IFileSystem::FindIsDirectory( FileFindHandle_t handle )
{ return static_cast<CBaseFileSystem*>(this)->FindIsDirectory( handle ); }
void IFileSystem::FindClose( FileFindHandle_t handle )
{ static_cast<CBaseFileSystem*>(this)->FindClose( handle ); }
const char * IFileSystem::FindFirstEx( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle )
{ return static_cast<CBaseFileSystem*>(this)->FindFirstEx( pWildCard, pPathID, pHandle ); }
void IFileSystem::FindFileAbsoluteList( std::vector< std::string > &outAbsolutePathNames, const char *pWildCard, const char *pPathID )
{ static_cast<CBaseFileSystem*>(this)->FindFileAbsoluteList( outAbsolutePathNames, pWildCard, pPathID ); }
const char * IFileSystem::GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize )
{ return static_cast<CBaseFileSystem*>(this)->GetLocalPath( pFileName, pLocalPath, localPathBufferSize ); }
bool IFileSystem::FullPathToRelativePath( const char *pFullpath, char *pRelative, int maxlen )
{ return static_cast<CBaseFileSystem*>(this)->FullPathToRelativePath( pFullpath, pRelative, maxlen ); }
bool IFileSystem::GetCurrentDirectory( char* pDirectory, int maxlen )
{ return static_cast<CBaseFileSystem*>(this)->GetCurrentDirectory( pDirectory, maxlen ); }
FileNameHandle_t IFileSystem::FindOrAddFileName( char const *pFileName )
{ return static_cast<CBaseFileSystem*>(this)->FindOrAddFileName( pFileName ); }
bool IFileSystem::String( const FileNameHandle_t& handle, char *buf, int buflen )
{ return static_cast<CBaseFileSystem*>(this)->String( handle, buf, buflen ); }
FSAsyncStatus_t IFileSystem::AsyncReadMultiple( const FileAsyncRequest_t *pRequests, int nRequests, FSAsyncControl_t *phControls )
{ return static_cast<CBaseFileSystem*>(this)->AsyncReadMultiple( pRequests, nRequests, phControls ); }
FSAsyncStatus_t IFileSystem::AsyncAppend( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncAppend( pFileName, pSrc, nSrcBytes, bFreeMemory, pControl ); }
FSAsyncStatus_t IFileSystem::AsyncAppendFile( const char *pAppendToFileName, const char *pAppendFromFileName, FSAsyncControl_t *pControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncAppendFile( pAppendToFileName, pAppendFromFileName, pControl ); }
void IFileSystem::AsyncFinishAll( int iToPriority )
{ static_cast<CBaseFileSystem*>(this)->AsyncFinishAll( iToPriority ); }
void IFileSystem::AsyncFinishAllWrites()
{ static_cast<CBaseFileSystem*>(this)->AsyncFinishAllWrites(); }
FSAsyncStatus_t IFileSystem::AsyncFlush()
{ return static_cast<CBaseFileSystem*>(this)->AsyncFlush(); }
bool IFileSystem::AsyncSuspend()
{ return static_cast<CBaseFileSystem*>(this)->AsyncSuspend(); }
bool IFileSystem::AsyncResume()
{ return static_cast<CBaseFileSystem*>(this)->AsyncResume(); }
FSAsyncStatus_t IFileSystem::AsyncBeginRead( const char *pszFile, FSAsyncFile_t *phFile )
{ return static_cast<CBaseFileSystem*>(this)->AsyncBeginRead( pszFile, phFile ); }
FSAsyncStatus_t IFileSystem::AsyncEndRead( FSAsyncFile_t hFile )
{ return static_cast<CBaseFileSystem*>(this)->AsyncEndRead( hFile ); }
FSAsyncStatus_t IFileSystem::AsyncFinish( FSAsyncControl_t hControl, bool wait )
{ return static_cast<CBaseFileSystem*>(this)->AsyncFinish( hControl, wait ); }
FSAsyncStatus_t IFileSystem::AsyncGetResult( FSAsyncControl_t hControl, void **ppData, int *pSize )
{ return static_cast<CBaseFileSystem*>(this)->AsyncGetResult( hControl, ppData, pSize ); }
FSAsyncStatus_t IFileSystem::AsyncAbort( FSAsyncControl_t hControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncAbort( hControl ); }
FSAsyncStatus_t IFileSystem::AsyncStatus( FSAsyncControl_t hControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncStatus( hControl ); }
FSAsyncStatus_t IFileSystem::AsyncSetPriority( FSAsyncControl_t hControl, int newPriority )
{ return static_cast<CBaseFileSystem*>(this)->AsyncSetPriority( hControl, newPriority ); }
void IFileSystem::AsyncAddRef( FSAsyncControl_t hControl )
{ static_cast<CBaseFileSystem*>(this)->AsyncAddRef( hControl ); }
void IFileSystem::AsyncRelease( FSAsyncControl_t hControl )
{ static_cast<CBaseFileSystem*>(this)->AsyncRelease( hControl ); }
WaitForResourcesHandle_t IFileSystem::WaitForResources( const char *resourcelist )
{ return static_cast<CBaseFileSystem*>(this)->WaitForResources( resourcelist ); }
bool IFileSystem::GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress, bool *complete )
{ return static_cast<CBaseFileSystem*>(this)->GetWaitForResourcesProgress( handle, progress, complete ); }
void IFileSystem::CancelWaitForResources( WaitForResourcesHandle_t handle )
{ static_cast<CBaseFileSystem*>(this)->CancelWaitForResources( handle ); }
int IFileSystem::HintResourceNeed( const char *hintlist, int forgetEverything )
{ return static_cast<CBaseFileSystem*>(this)->HintResourceNeed( hintlist, forgetEverything ); }
bool IFileSystem::IsFileImmediatelyAvailable( const char *pFileName )
{ return static_cast<CBaseFileSystem*>(this)->IsFileImmediatelyAvailable( pFileName ); }
void IFileSystem::GetLocalCopy( const char *pFileName )
{ static_cast<CBaseFileSystem*>(this)->GetLocalCopy( pFileName ); }
void IFileSystem::PrintOpenedFiles( void )
{ static_cast<CBaseFileSystem*>(this)->PrintOpenedFiles(); }
void IFileSystem::PrintSearchPaths( void )
{ static_cast<CBaseFileSystem*>(this)->PrintSearchPaths(); }
void IFileSystem::SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) )
{ static_cast<CBaseFileSystem*>(this)->SetWarningFunc( pfnWarning ); }
void IFileSystem::SetWarningLevel( FileWarningLevel_t level )
{ static_cast<CBaseFileSystem*>(this)->SetWarningLevel( level ); }
void IFileSystem::AddLoggingFunc( void (*pfnLogFunc)( const char *fileName, const char *accessType ) )
{ static_cast<CBaseFileSystem*>(this)->AddLoggingFunc( pfnLogFunc ); }
void IFileSystem::RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc )
{ static_cast<CBaseFileSystem*>(this)->RemoveLoggingFunc( logFunc ); }
const FileSystemStatistics * IFileSystem::GetFilesystemStatistics()
{ return static_cast<CBaseFileSystem*>(this)->GetFilesystemStatistics(); }
FileHandle_t IFileSystem::OpenEx( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename )
{ return static_cast<CBaseFileSystem*>(this)->OpenEx( pFileName, pOptions, flags, pathID, ppszResolvedFilename ); }
int IFileSystem::ReadEx( void* pOutput, int sizeDest, int size, FileHandle_t file )
{ return static_cast<CBaseFileSystem*>(this)->ReadEx( pOutput, sizeDest, size, file ); }
int IFileSystem::ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc )
{ return static_cast<CBaseFileSystem*>(this)->ReadFileEx( pFileName, pPath, ppBuf, bNullTerminate, bOptimalAlloc, nMaxBytes, nStartingByte, pfnAlloc ); }
FileNameHandle_t IFileSystem::FindFileName( char const *pFileName )
{ return static_cast<CBaseFileSystem*>(this)->FindFileName( pFileName ); }
void IFileSystem::SetupPreloadData()
{ static_cast<CBaseFileSystem*>(this)->SetupPreloadData(); }
void IFileSystem::DiscardPreloadData()
{ static_cast<CBaseFileSystem*>(this)->DiscardPreloadData(); }
KeyValues * IFileSystem::LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->LoadKeyValues( type, filename, pPathID ); }
bool IFileSystem::LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->LoadKeyValues( head, type, filename, pPathID ); }
FSAsyncStatus_t IFileSystem::AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, bAppend, pControl ); }
FSAsyncStatus_t IFileSystem::AsyncReadMultipleCreditAlloc( const FileAsyncRequest_t *pRequests, int nRequests, const char *pszFile, int line, FSAsyncControl_t *phControls )
{ return static_cast<CBaseFileSystem*>(this)->AsyncReadMultipleCreditAlloc( pRequests, nRequests, pszFile, line, phControls ); }
FSAsyncStatus_t IFileSystem::AsyncDirectoryScan( const char* pSearchSpec, bool recurseFolders, void* pContext, FSAsyncScanAddFunc_t pfnAdd, FSAsyncScanCompleteFunc_t pfnDone, FSAsyncControl_t *pControl )
{ return static_cast<CBaseFileSystem*>(this)->AsyncDirectoryScan( pSearchSpec, recurseFolders, pContext, pfnAdd, pfnDone, pControl ); }
bool IFileSystem::GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, std::size_t bufSizeInBytes )
{ return static_cast<CBaseFileSystem*>(this)->GetFileTypeForFullPath( pFullPath, buf, bufSizeInBytes ); }
std::optional< std::vector< std::byte > > IFileSystem::ReadToVector( FileHandle_t hFile, int nMaxBytes )
{ return static_cast<CBaseFileSystem*>(this)->ReadToVector( hFile, nMaxBytes ); }
bool IFileSystem::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{ return static_cast<CBaseFileSystem*>(this)->GetOptimalIOConstraints( hFile, pOffsetAlign, pSizeAlign, pBufferAlign ); }
void * IFileSystem::AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset )
{ return static_cast<CBaseFileSystem*>(this)->AllocOptimalReadBuffer( hFile, nSize, nOffset ); }
void IFileSystem::BeginMapAccess()
{ static_cast<CBaseFileSystem*>(this)->BeginMapAccess(); }
void IFileSystem::EndMapAccess()
{ static_cast<CBaseFileSystem*>(this)->EndMapAccess(); }
bool IFileSystem::FullPathToRelativePathEx( const char *pFullpath, const char *pPathId, char *pRelative, int maxlen )
{ return static_cast<CBaseFileSystem*>(this)->FullPathToRelativePathEx( pFullpath, pPathId, pRelative, maxlen ); }
int IFileSystem::GetPathIndex( const FileNameHandle_t &handle )
{ return static_cast<CBaseFileSystem*>(this)->GetPathIndex( handle ); }
long IFileSystem::GetPathTime( const char *pPath, const char *pPathID )
{ return static_cast<CBaseFileSystem*>(this)->GetPathTime( pPath, pPathID ); }
void IFileSystem::EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{ static_cast<CBaseFileSystem*>(this)->EnableWhitelistFileTracking( bEnable, bCacheAllVPKHashes, bRecalculateAndCheckHashes ); }
void IFileSystem::RegisterFileWhitelist( IFileList *pWantCRCList, IFileList *pAllowFromDiskList, IFileList **pFilesToReload )
{ static_cast<CBaseFileSystem*>(this)->RegisterFileWhitelist( pWantCRCList, pAllowFromDiskList, pFilesToReload ); }
void IFileSystem::MarkAllCRCsUnverified()
{ static_cast<CBaseFileSystem*>(this)->MarkAllCRCsUnverified(); }
void IFileSystem::CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter )
{ static_cast<CBaseFileSystem*>(this)->CacheFileCRCs( pPathname, eType, pFilter ); }
EFileCRCStatus IFileSystem::CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
{ return static_cast<CBaseFileSystem*>(this)->CheckCachedFileHash( pPathID, pRelativeFilename, nFileFraction, pFileHash ); }
int IFileSystem::GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles )
{ return static_cast<CBaseFileSystem*>(this)->GetUnverifiedFileHashes( pFiles, nMaxFiles ); }
int IFileSystem::GetWhitelistSpewFlags()
{ return static_cast<CBaseFileSystem*>(this)->GetWhitelistSpewFlags(); }
void IFileSystem::SetWhitelistSpewFlags( int flags )
{ static_cast<CBaseFileSystem*>(this)->SetWhitelistSpewFlags( flags ); }
void IFileSystem::InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func )
{ static_cast<CBaseFileSystem*>(this)->InstallDirtyDiskReportFunc( func ); }
bool IFileSystem::IsLaunchedFromXboxHDD()
{ return static_cast<CBaseFileSystem*>(this)->IsLaunchedFromXboxHDD(); }
bool IFileSystem::IsInstalledToXboxHDDCache()
{ return static_cast<CBaseFileSystem*>(this)->IsInstalledToXboxHDDCache(); }
bool IFileSystem::IsDVDHosted()
{ return static_cast<CBaseFileSystem*>(this)->IsDVDHosted(); }
bool IFileSystem::IsInstallAllowed()
{ return static_cast<CBaseFileSystem*>(this)->IsInstallAllowed(); }
int IFileSystem::GetSearchPathID( char *pPath, int nMaxLen )
{ return static_cast<CBaseFileSystem*>(this)->GetSearchPathID( pPath, nMaxLen ); }
bool IFileSystem::FixupSearchPathsAfterInstall()
{ return static_cast<CBaseFileSystem*>(this)->FixupSearchPathsAfterInstall(); }
FSDirtyDiskReportFunc_t IFileSystem::GetDirtyDiskReportFunc()
{ return static_cast<CBaseFileSystem*>(this)->GetDirtyDiskReportFunc(); }
void IFileSystem::AddVPKFile( char const *pszName, SearchPathAdd_t addType )
{ static_cast<CBaseFileSystem*>(this)->AddVPKFile( pszName, addType ); }
void IFileSystem::RemoveVPKFile( char const *pszName )
{ static_cast<CBaseFileSystem*>(this)->RemoveVPKFile( pszName ); }
void IFileSystem::GetVPKFileNames( std::vector< std::string > &destVector )
{ static_cast<CBaseFileSystem*>(this)->GetVPKFileNames( destVector ); }
void IFileSystem::RemoveAllMapSearchPaths()
{ static_cast<CBaseFileSystem*>(this)->RemoveAllMapSearchPaths(); }
void IFileSystem::SyncDvdDevCache()
{ static_cast<CBaseFileSystem*>(this)->SyncDvdDevCache(); }
bool IFileSystem::GetStringFromKVPool( CRC32_t poolKey, unsigned int key, char *pOutBuff, int buflen )
{ return static_cast<CBaseFileSystem*>(this)->GetStringFromKVPool( poolKey, key, pOutBuff, buflen ); }
bool IFileSystem::DiscoverDLC( int iController )
{ return static_cast<CBaseFileSystem*>(this)->DiscoverDLC( iController ); }
int IFileSystem::IsAnyDLCPresent( bool *pbDLCSearchPathMounted )
{ return static_cast<CBaseFileSystem*>(this)->IsAnyDLCPresent( pbDLCSearchPathMounted ); }
bool IFileSystem::GetAnyDLCInfo( int iDLC, unsigned int *pLicenseMask, wchar_t *pTitleBuff, int nOutTitleSize )
{ return static_cast<CBaseFileSystem*>(this)->GetAnyDLCInfo( iDLC, pLicenseMask, pTitleBuff, nOutTitleSize ); }
int IFileSystem::IsAnyCorruptDLC()
{ return static_cast<CBaseFileSystem*>(this)->IsAnyCorruptDLC(); }
bool IFileSystem::GetAnyCorruptDLCInfo( int iCorruptDLC, wchar_t *pTitleBuff, int nOutTitleSize )
{ return static_cast<CBaseFileSystem*>(this)->GetAnyCorruptDLCInfo( iCorruptDLC, pTitleBuff, nOutTitleSize ); }
bool IFileSystem::AddDLCSearchPaths()
{ return static_cast<CBaseFileSystem*>(this)->AddDLCSearchPaths(); }
bool IFileSystem::IsSpecificDLCPresent( unsigned int nDLCPackage )
{ return static_cast<CBaseFileSystem*>(this)->IsSpecificDLCPresent( nDLCPackage ); }
void IFileSystem::SetIODelayAlarm( float flThreshhold )
{ static_cast<CBaseFileSystem*>(this)->SetIODelayAlarm( flThreshhold ); }
bool IFileSystem::AddXLSPUpdateSearchPath( const void *pData, int nSize )
{ return static_cast<CBaseFileSystem*>(this)->AddXLSPUpdateSearchPath( pData, nSize ); }
IIoStats * IFileSystem::GetIoStats()
{ return static_cast<CBaseFileSystem*>(this)->GetIoStats(); }
void IFileSystem::CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{ static_cast<CBaseFileSystem*>(this)->CacheAllVPKFileHashes( bCacheAllVPKHashes, bRecalculateAndCheckHashes ); }
bool IFileSystem::CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value )
{ return static_cast<CBaseFileSystem*>(this)->CheckVPKFileHash( PackFileID, nPackFileNumber, nFileFraction, md5Value ); }
void IFileSystem::GetVPKFileStatisticsKV( KeyValues *pKV )
{ static_cast<CBaseFileSystem*>(this)->GetVPKFileStatisticsKV( pKV ); }

// ---- IIoStats -> CIoStats
void IIoStats::OnFileSeek( int nTimeInMs )
{ static_cast<CIoStats*>(this)->OnFileSeek( nTimeInMs ); }
void IIoStats::OnFileRead( int nTimeInMs, int nBytesRead )
{ static_cast<CIoStats*>(this)->OnFileRead( nTimeInMs, nBytesRead ); }
void IIoStats::OnFileOpen( const char * pFileName )
{ static_cast<CIoStats*>(this)->OnFileOpen( pFileName ); }
int IIoStats::GetNumberOfFileSeeks()
{ return static_cast<CIoStats*>(this)->GetNumberOfFileSeeks(); }
int IIoStats::GetTimeInFileSeek()
{ return static_cast<CIoStats*>(this)->GetTimeInFileSeek(); }
int IIoStats::GetNumberOfFileReads()
{ return static_cast<CIoStats*>(this)->GetNumberOfFileReads(); }
int IIoStats::GetTimeInFileReads()
{ return static_cast<CIoStats*>(this)->GetTimeInFileReads(); }
int IIoStats::GetFileReadTotalSize()
{ return static_cast<CIoStats*>(this)->GetFileReadTotalSize(); }
int IIoStats::GetNumberOfFileOpens()
{ return static_cast<CIoStats*>(this)->GetNumberOfFileOpens(); }
void IIoStats::Reset()
{ static_cast<CIoStats*>(this)->Reset(); }

// ---- IThreadedFileMD5Processor -> CFileTracker2
int IThreadedFileMD5Processor::SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction )
{ return static_cast<CFileTracker2*>(this)->SubmitThreadedMD5Request( pubBuffer, cubBuffer, PackFileID, nPackFileNumber, nPackFileFraction ); }
bool IThreadedFileMD5Processor::BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{ return static_cast<CFileTracker2*>(this)->BlockUntilMD5RequestComplete( iRequest, pMd5ValueOut ); }
bool IThreadedFileMD5Processor::IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{ return static_cast<CFileTracker2*>(this)->IsMD5RequestComplete( iRequest, pMd5ValueOut ); }

// ---- the members the generator cannot forward
void IFileSystem::FreeOptimalReadBuffer( void *p )
{ static_cast<CBaseFileSystem*>(this)->FreeOptimalReadBuffer( p ); }
int IFileSystem::FPrintf( FileHandle_t file, const char *pFormat, ... )
{
	std::va_list args;
	__builtin_va_start( args, pFormat );
	int len = static_cast<CBaseFileSystem*>(this)->FVPrintf( file, pFormat, args );
	__builtin_va_end( args );
	return len;
}
bool IFileSystem::ShouldGameReloadFile( const char *pFilename )
{ return static_cast<CBaseFileSystem*>(this)->ShouldGameReloadFile( pFilename ); }

// ---- App system protocol
bool IFileSystem::Connect( CreateInterfaceFn factory )
{ return static_cast<CBaseFileSystem*>(this)->Connect( factory ); }
void IFileSystem::Disconnect()
{ static_cast<CBaseFileSystem*>(this)->Disconnect(); }
void *IFileSystem::QueryInterface( const char *pInterfaceName )
{ return static_cast<CBaseFileSystem*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IFileSystem::Init()
{ return static_cast<CBaseFileSystem*>(this)->Init(); }
void IFileSystem::Shutdown()
{ static_cast<CBaseFileSystem*>(this)->Shutdown(); }
