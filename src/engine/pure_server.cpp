// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "userid.h"
#include "pure_server.h"
#include "common.h"
#include "filesystem_engine.h"
#include "server.h"
#include "sv_filter.h"
import std;

extern ConVar sv_pure_consensus;
extern ConVar sv_pure_retiretime;
extern ConVar sv_pure_trace;

static char *g_SvPure2_ProtectedDirs[] =
{
	"sound",
	"models",
	"materials"
};

static bool IsProtectedBySvPure2( const char *pFilename )
{
	for ( int i=0; i < std::size( g_SvPure2_ProtectedDirs ); i++ )
	{
		const char *pProtectedDir = g_SvPure2_ProtectedDirs[i];
		int len = V_strlen( pProtectedDir );
		
		if ( V_strlen( pFilename ) < len+1 )
			return false;
		
		char tempStr[512];
		contract_assert( len < std::size( tempStr ) );
		std::memcpy( tempStr, pFilename, len );
		tempStr[len] = 0;
		
		if ( V_stricmp( tempStr, pProtectedDir ) == 0 )
		{
			if ( pFilename[len] == '/' || pFilename[len] == '\\' )
				return true;
		}		
	}
	return false;
}



CPureServerWhitelist::CCommand::CCommand()
{
}

CPureServerWhitelist::CCommand::~CCommand()
{
}


CPureServerWhitelist* CPureServerWhitelist::Create( IFileSystem *pFileSystem )
{
	CPureServerWhitelist *pRet = new CPureServerWhitelist;
	pRet->Init( pFileSystem );
	return pRet;
}


CPureServerWhitelist::CPureServerWhitelist()
{
	m_pFileSystem = nullptr;
	m_LoadCounter = 0;
	m_AllowFromDiskList.m_pWhitelist = this;
	m_ForceMatchList.m_pWhitelist = this;
	m_RefCount = 1;
	m_bFullyPureMode = false;
}


CPureServerWhitelist::~CPureServerWhitelist()
{
	Term();
}


void CPureServerWhitelist::Init( IFileSystem *pFileSystem )
{
	Term();
	m_pFileSystem = pFileSystem;
}


void CPureServerWhitelist::Term()
{
	m_FileCommands.clear();
	m_RecursiveDirCommands.clear();
	m_NonRecursiveDirCommands.clear();
	m_pFileSystem = nullptr;
	m_LoadCounter = 0;
}


bool CPureServerWhitelist::LoadFromKeyValues( KeyValues *kv )
{
	for ( KeyValues *pCurItem = kv->GetFirstValue(); pCurItem; pCurItem = pCurItem->GetNextValue() )
	{
		char szPathName[ MAX_PATH ];
		const char *pKeyValue = pCurItem->GetName();
		const char *pModifiers = pCurItem->GetString();
		if ( !pKeyValue || !pModifiers )
			continue;
	
		V_strncpy( szPathName, pKeyValue, sizeof(szPathName) );
		V_FixSlashes( szPathName );
		const char *pValue = szPathName;

		// Figure out the modifiers.
		bool bAllowFromDisk = false, bCheckCRC = false;
		std::vector< std::string > mods;
		V_SplitString( pModifiers, "+", mods );
		for ( int i=0; i < std::ssize( mods ); i++ )
		{
			if ( V_stricmp( mods[i].c_str(), "allow_from_disk" ) == 0 )
				bAllowFromDisk = true;
			else if ( V_stricmp( mods[i].c_str(), "check_crc" ) == 0 )
				bCheckCRC = true;
			else
				Warning( "Unknown modifier in whitelist file: %s.\n", mods[i].c_str() );
		}
		
		// Setup a new CCommand to hold this command.
		auto pCommand = std::make_unique< CPureServerWhitelist::CCommand >();
		pCommand->m_LoadOrder = m_LoadCounter++;
		pCommand->m_bAllowFromDisk = bAllowFromDisk;
		pCommand->m_bCheckCRC = bCheckCRC;

		// Figure out if they're referencing a file, a recursive directory, or a nonrecursive directory.		
		CommandDict_t *pList;
		const char *pEndPart = V_UnqualifiedFileName( pValue );
		if ( V_stricmp( pEndPart, "..." ) == 0 )
			pList = &m_RecursiveDirCommands;
		else if ( V_stricmp( pEndPart, "*.*" ) == 0 )
			pList = &m_NonRecursiveDirCommands;
		else
			pList = &m_FileCommands;
		
		// If it's a directory command, get rid of the *.* or ...
		char filePath[MAX_PATH];
		if ( pList == &m_RecursiveDirCommands || pList == &m_NonRecursiveDirCommands )
			V_ExtractFilePath( pValue, filePath, sizeof( filePath ) );
		else
			V_strncpy( filePath, pValue, sizeof( filePath ) );
		
		V_FixSlashes( filePath );

		// Add the command to the appropriate list.
		if ( !pList->contains( filePath ) )
		{
			pList->emplace( filePath, std::move( pCommand ) );
		}
		else
		{
			Error( "Pure server whitelist entry '%s' is a duplicate.\n", filePath );
		}		
	}	
	
	return true;
}


void CPureServerWhitelist::EnableFullyPureMode()
{
	// In this mode, no files are allowed from disk.
	m_FileCommands.clear();
	m_RecursiveDirCommands.clear();
	m_NonRecursiveDirCommands.clear();
	m_bFullyPureMode = true;
}


bool CPureServerWhitelist::IsInFullyPureMode() const
{
	return m_bFullyPureMode;
}


void CPureServerWhitelist::UpdateCommandStats( CommandDict_t &commands, int *pHighest, int *pLongestPathName )
{
	for ( const auto &[name, pCommand] : commands )
	{
		*pHighest = std::max<std::common_type_t<int, short unsigned int>>( *pHighest, pCommand->m_LoadOrder );
		
		int len = (int)name.size();
		*pLongestPathName = std::max( *pLongestPathName, len );
	}
}

void CPureServerWhitelist::PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand )
{
	// Get rid of the trailing slash if there is one.
	char tempFileSpec[MAX_PATH];
	V_strncpy( tempFileSpec, pFileSpec, sizeof( tempFileSpec ) );
	int len = V_strlen( tempFileSpec );
	if ( len > 0 && (tempFileSpec[len-1] == '/' || tempFileSpec[len-1] == '\\') )
		tempFileSpec[len-1] = 0;
		
	if ( pExt )
		Msg( "%s%c%s", tempFileSpec, CORRECT_PATH_SEPARATOR, pExt );
	else
		Msg( "%s", tempFileSpec );

	len = V_strlen( pFileSpec );
	for ( int i=len; i < maxPathnameLen+6; i++ )
	{
		Msg( " " );
	}
	
	Msg( "\t" );
	if ( pCommand->m_bCheckCRC )
		Msg( "check_crc" );
	else
		Msg( "allow_from_disk" );
	
	Msg( "\n" );
}


CPureServerWhitelist::CommandDict_t::iterator CPureServerWhitelist::FindCommandByLoadOrder( CommandDict_t &commands, int iLoadOrder )
{
	for ( auto it = commands.begin(); it != commands.end(); ++it )
	{
		if ( it->second->m_LoadOrder == iLoadOrder )
			return it;
	}
	return commands.end();
}


void CPureServerWhitelist::PrintWhitelistContents()
{
	int highestLoadOrder = 0, longestPathName = 0;
	UpdateCommandStats( m_FileCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_RecursiveDirCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_NonRecursiveDirCommands, &highestLoadOrder, &longestPathName );
	
	for ( int iLoadOrder=0; iLoadOrder <= highestLoadOrder; iLoadOrder++ )
	{
		// Check regular file commands.
		auto iCommand = FindCommandByLoadOrder( m_FileCommands, iLoadOrder );
		if ( iCommand != m_FileCommands.end() )
		{
			PrintCommand( iCommand->first.c_str(), nullptr, longestPathName, iCommand->second.get() );
		}
		else
		{
			// Check recursive commands.
			iCommand = FindCommandByLoadOrder( m_RecursiveDirCommands, iLoadOrder );
			if ( iCommand != m_RecursiveDirCommands.end() )
			{
				PrintCommand( iCommand->first.c_str(), "...", longestPathName, iCommand->second.get() );
			}
			else
			{
				// Check *.* commands.
				iCommand = FindCommandByLoadOrder( m_NonRecursiveDirCommands, iLoadOrder );
				if ( iCommand != m_NonRecursiveDirCommands.end() )
				{
					PrintCommand( iCommand->first.c_str(), "*.*", longestPathName, iCommand->second.get() );
				}
			}
		}
	}
}


void CPureServerWhitelist::Encode( ks::byte_writer &buf )
{
	EncodeCommandList( m_FileCommands, buf );
	EncodeCommandList( m_RecursiveDirCommands, buf );
	EncodeCommandList( m_NonRecursiveDirCommands, buf );
	buf.write<char>( (char)m_bFullyPureMode );
}


void CPureServerWhitelist::EncodeCommandList( CommandDict_t &theList, ks::byte_writer &buf )
{
	buf.write<int>( (int)theList.size() );
	for ( const auto &[name, pCommand] : theList )
	{
		
		unsigned char val = 0;
		if ( pCommand->m_bAllowFromDisk )
			val |= 0x01;
		if ( pCommand->m_bCheckCRC )
			val |= 0x02;
		
		buf.write<unsigned char>( val );
		buf.write<unsigned short>( pCommand->m_LoadOrder );
		buf.write_cstring( name );
	}
}


void CPureServerWhitelist::Decode( ks::byte_reader &buf )
{
	DecodeCommandList( m_FileCommands, buf );
	DecodeCommandList( m_RecursiveDirCommands, buf );
	DecodeCommandList( m_NonRecursiveDirCommands, buf );

	if ( buf.remaining() >= 1 )
	{
		m_bFullyPureMode = (buf.read<char>() != 0);
	}
	else
	{
		m_bFullyPureMode = false;
	}
}


void CPureServerWhitelist::CacheFileCRCs()
{
	InternalCacheFileCRCs( m_FileCommands, k_eCacheCRCType_SingleFile );
	InternalCacheFileCRCs( m_NonRecursiveDirCommands, k_eCacheCRCType_Directory );
	InternalCacheFileCRCs( m_RecursiveDirCommands, k_eCacheCRCType_Directory_Recursive );
}


void CPureServerWhitelist::InternalCacheFileCRCs( CommandDict_t &theList, ECacheCRCType eType )
{
	for ( const auto &[name, pCommand] : theList )
	{
		if ( pCommand->m_bCheckCRC )
		{
			const char *pPathname = name.c_str();
			m_pFileSystem->CacheFileCRCs( pPathname, eType, &m_ForceMatchList );
		}
	}
}


void CPureServerWhitelist::DecodeCommandList( CommandDict_t &theList, ks::byte_reader &buf )
{
	int nCommands = buf.read<int>();
	
	for ( int i=0; i < nCommands; i++ )
	{
		auto pCommand = std::make_unique< CPureServerWhitelist::CCommand >();
		
		unsigned char val = buf.read<unsigned char>();
		pCommand->m_bAllowFromDisk = (( val & 0x01 ) != 0);
		pCommand->m_bCheckCRC      = (( val & 0x02 ) != 0);

		pCommand->m_LoadOrder = buf.read<unsigned short>();

		char str[MAX_PATH];
		buf.read_cstring( str, sizeof( str )-1 );
		V_FixSlashes( str );
		
		theList.try_emplace( str, std::move( pCommand ) );	// a repeated entry keeps the first
	}
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::GetBestEntry( const char *pFilename )
{
	// NOTE: Since this is a user-specified file, we don't have the added complexity of path IDs in here.
	// So when the filesystem asks if a file is in the whitelist, we just ignore the path ID.
	
	// Make sure we have a relative pathname with fixed slashes..
	char relativeFilename[MAX_PATH];
	V_strncpy( relativeFilename, pFilename, sizeof( relativeFilename ) );

	// Convert the path to relative if necessary.
	if ( !V_IsAbsolutePath( relativeFilename ) || m_pFileSystem->FullPathToRelativePath( pFilename, relativeFilename, sizeof( relativeFilename ) ) )
	{
		V_FixSlashes( relativeFilename );
		
		// Get the directory this thing is in.
		char relativeDir[MAX_PATH];
		if ( !V_ExtractFilePath( relativeFilename, relativeDir, sizeof( relativeDir ) )	)
			relativeDir[0] = 0;
		
		
		// Check each of our dictionaries to see if there is an entry for this thing.
		CCommand *pBestEntry = nullptr;
		
		pBestEntry = CheckEntry( m_FileCommands, relativeFilename, pBestEntry );
		if ( relativeDir[0] != 0 )
		{
			pBestEntry = CheckEntry( m_NonRecursiveDirCommands, relativeDir, pBestEntry );

			while ( relativeDir[0] != 0 )
			{
				// Check for this directory.
				pBestEntry = CheckEntry( m_RecursiveDirCommands, relativeDir, pBestEntry );
				if ( !V_StripLastDir( relativeDir, sizeof( relativeDir ) ) )
					break;
			}
		}
			
		return pBestEntry;
	}
	
	// Either we couldn't find an entry, or they specified an absolute path that we could not convert to a relative path.
	return nullptr;
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::CheckEntry( 
	CommandDict_t &dict, 
	const char *pEntryName, 
	CPureServerWhitelist::CCommand *pBestEntry )
{
	auto i = dict.find( pEntryName );
	if ( i != dict.end() && (!pBestEntry || i->second->m_LoadOrder > pBestEntry->m_LoadOrder) )
		pBestEntry = i->second.get();
	
	return pBestEntry;
}


void CPureServerWhitelist::Release()
{
	if ( --m_RefCount <= 0 )
		delete this;
}


IFileList* CPureServerWhitelist::GetAllowFromDiskList()
{
	++m_RefCount;
	return &m_AllowFromDiskList;
}


IFileList* CPureServerWhitelist::GetForceMatchList()
{
	++m_RefCount;
	return &m_ForceMatchList;
}


// --------------------------------------------------------------------------------------------------- //
// CAllowFromDiskList/CForceMatchList implementation.
// --------------------------------------------------------------------------------------------------- //

bool CPureServerWhitelist::CAllowFromDiskList::IsFileInList( const char *pFilename )
{
	// In "fully pure" mode, all files must come from disk.
	if ( m_pWhitelist->m_bFullyPureMode )
	{
		// Only protect maps, models, and sounds.
		if ( IsProtectedBySvPure2( pFilename ) )
			return false;
		else
			return true;
	}
		
	CCommand *pCommand = m_pWhitelist->GetBestEntry( pFilename );
	if ( pCommand )
		return pCommand->m_bAllowFromDisk;
	else
		return true;	// All files are allowed to come from disk by default.
}

void CPureServerWhitelist::CAllowFromDiskList::Release()
{
	m_pWhitelist->Release();
}

bool CPureServerWhitelist::CForceMatchList::IsFileInList( const char *pFilename )
{
	// In "fully pure" mode, all files must match the server files
	if ( m_pWhitelist->m_bFullyPureMode )
		return true;

	CCommand *pCommand = m_pWhitelist->GetBestEntry( pFilename );
	if ( pCommand )
		return pCommand->m_bCheckCRC;
	else
		return false;	// By default, no files require the CRC check.
}

void CPureServerWhitelist::CForceMatchList::Release()
{
	m_pWhitelist->Release();
}


void CPureFileTracker::AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord )
{
	UserReportedFileHash_t userFileHash;
	userFileHash.m_idxFile = idxFile;
	userFileHash.m_userID = userID;
	userFileHash.m_FileHash = *pFileHash;
	auto [itUserReported, bInserted] = m_treeUserReportedFileHash.insert( userFileHash );
	if ( bInserted )
	{
		if ( bAddMasterRecord )
		{
			// count the number of matches for this idxFile
			// if it exceeds > 5 then make a master record
			auto itFirst = itUserReported;
			auto itLast = itUserReported;
			int ctMatches = 1;
			int ctTotalFiles = 1;
			// first go forward
			for ( auto it = std::next( itUserReported ); it != m_treeUserReportedFileHash.end() && it->m_idxFile == itUserReported->m_idxFile; ++it )
			{
				if ( it->m_FileHash == itUserReported->m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				itLast = it;
			}
			// then backwards
			for ( auto it = itUserReported; it != m_treeUserReportedFileHash.begin(); )
			{
				--it;
				if ( it->m_idxFile != itUserReported->m_idxFile )
					break;
				if ( it->m_FileHash == itUserReported->m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				itFirst = it;
			}
			// if ctTotalFiles >> ctMatches then that means clients are reading different bits from the file.
			// in order to get this right we need to ask them to read the entire thing
			if ( ctMatches >= sv_pure_consensus.GetInt() )
			{
				MasterFileHash_t masterFileHashNew;
				masterFileHashNew.m_idxFile = itUserReported->m_idxFile;
				masterFileHashNew.m_cMatches = ctMatches;
				masterFileHashNew.m_FileHash = itUserReported->m_FileHash;
				m_treeMasterFileHashes.insert( masterFileHashNew );
				// remove all the individual records that matched the new master, we don't need them anymore
				// (itUserReported is among them, so compare against the copy)
				auto itRemove = itFirst;
				while ( itRemove != m_treeUserReportedFileHash.end() )
				{
					auto itNext = std::next( itRemove );
					bool bLast = ( itRemove == itLast );
					if ( itRemove->m_FileHash == masterFileHashNew.m_FileHash )
						m_treeUserReportedFileHash.erase( itRemove );
					if ( bLast )
						break;
					itRemove = itNext;
				}
			}
		}
	}
	else
	{
		itUserReported->m_FileHash = *pFileHash;
	}
	// we dont have enough data to decide if you match or not yet - so we call it a match
}


void FileRenderHelper( USERID_t userID, const char *pchMessage, const char *pchPath, const char *pchFileName, const FileHash_t *pFileHash, int nFileFraction, const FileHash_t *pFileHashLocal )
{
	char rgch[256];
	char hex[ 34 ];
	V_memset( hex, 0, sizeof( hex ) );
	V_binarytohex( (const byte *)&pFileHash->m_md5contents.bits, sizeof( pFileHash->m_md5contents.bits ), hex, sizeof( hex ) );

	char hex2[ 34 ];
	V_memset( hex2, 0, sizeof( hex2 ) );
	if ( pFileHashLocal )
		V_binarytohex( (const byte *)&pFileHashLocal->m_md5contents.bits, sizeof( pFileHashLocal->m_md5contents.bits ), hex2, sizeof( hex2 ) );

	if ( pFileHash->m_PackFileID )
	{
		V_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %8.8x %6.6x ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_PackFileID, pFileHash->m_nPackFileNumber, nFileFraction, pFileHash->m_cbFileLen,
			pchMessage, 
			hex, hex2 );
	}
	else
	{
		V_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %lx ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_eFileHashType, pFileHash->m_cbFileLen, pFileHash->m_eFileHashType ? pFileHash->m_crcIOSequence : 0,
			pchMessage, 
			hex, hex2 );
	}
	if ( userID.idtype != 0 )
		Msg( "[%s] %s\n", GetUserIDString(userID), rgch );
	else
		Msg( "%s", rgch );

}


bool CPureFileTracker::DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID )
{
	// if the server has been idle for more than 15 minutes, discard all this data
	const float flRetireTime = sv_pure_retiretime.GetFloat();
	float flCurTime = Plat_FloatTime();
	if ( ( flCurTime - m_flLastFileReceivedTime ) > flRetireTime )
	{
		m_treeMasterFileHashes.clear();
		m_treeUserReportedFileHash.clear();
	}
	m_flLastFileReceivedTime = flCurTime;

	// The clients must send us all files. We decide if it is whitelisted or not
	// That way the clients can not hide modified files in a whitelisted directory
	if ( pFileHash->m_PackFileID == 0 && 
		!sv.GetPureServerWhitelist()->GetForceMatchList()->IsFileInList( pRelativeFilename ) )
	{

		if ( sv_pure_trace.GetInt() == 4 )
		{
			char warningStr[1024] = {0};
			V_snprintf( warningStr, sizeof( warningStr ), "Pure server: file [%s]\\%s ignored by whitelist.", pPathID, pRelativeFilename );
			Msg( "[%s] %s\n", GetUserIDString(userID), warningStr );
		}

		return true;
	}

	char rgchFilenameFixed[MAX_PATH];
	V_strncpy( rgchFilenameFixed, pRelativeFilename, sizeof( rgchFilenameFixed ) );
	V_FixSlashes( rgchFilenameFixed );

	// first look up the file and see if we have ever seen it before
	CRC32_t crcFilename;
	CRC32_Init( &crcFilename );
	CRC32_ProcessBuffer( &crcFilename, rgchFilenameFixed, V_strlen( rgchFilenameFixed ) );
	CRC32_ProcessBuffer( &crcFilename, pPathID, V_strlen( pPathID ) );
	CRC32_Final( &crcFilename );
	UserReportedFile_t ufile;
	ufile.m_crcIdentifier = crcFilename;
	ufile.m_filename = rgchFilenameFixed;
	ufile.m_path = ks::nz( pPathID );
	ufile.m_nFileFraction = nFileFraction;
	auto [itFile, bNewFile] = m_treeAllReportedFiles.try_emplace( std::move( ufile ), (int)m_vecReportedFilesByIndex.size() );
	if ( bNewFile )
	{
		m_vecReportedFilesByIndex.push_back( &itFile->first );
	}
	else
	{
		m_cMatchedFile++;
	}
	int idxFile = itFile->second;
	// then check if we have a master CRC for the file
	MasterFileHash_t masterFileHash;
	masterFileHash.m_idxFile = idxFile;
	auto itMaster = m_treeMasterFileHashes.find( masterFileHash );
	// dont do anything with this yet

	// check to see if we have loaded the file locally and can match it
	FileHash_t filehashLocal;
	EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
	if ( eStatus == k_eFileCRCStatus_FileInVPK)
	{
		// you managed to load a file outside a VPK that the server has in the VPK
		// this is possible if the user explodes the VPKs into individual files and then deletes the VPKs
		FileRenderHelper( userID, "file should be in VPK", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, nullptr );
		return false;
	}
	// if the user sent us a full file hash, but we dont have one, hash it now
	if ( pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile && 
		( eStatus != k_eFileCRCStatus_GotCRC || filehashLocal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile ) )
	{
		// lets actually read the file so we get a complete file hash
		FileHandle_t f = g_pFileSystem->Open( rgchFilenameFixed, "rb", pPathID);
		// try to load the file and really compute the hash - should only have to do this once ever
		if ( f )
		{
			// load file into a null-terminated buffer
			int fileSize = g_pFileSystem->Size( f );
			unsigned bufSize = g_pFileSystem->GetOptimalReadSize( f, fileSize );

			char *buffer = (char*)g_pFileSystem->AllocOptimalReadBuffer( f, bufSize );
			contract_assert( buffer );

			// read into local buffer
			bool bRetOK = ( g_pFileSystem->ReadEx( buffer, bufSize, fileSize, f ) != 0 );
			bRetOK;
			g_pFileSystem->FreeOptimalReadBuffer( buffer );

			g_pFileSystem->Close( f );	// close file after reading

			eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
		}
		else
		{
			// what should we do if we couldn't open the file? should probably kick
			FileRenderHelper( userID, "could not open file to hash ( benign for now )", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, nullptr );
		}
	}
	if ( eStatus == k_eFileCRCStatus_GotCRC )
	{
		if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
			pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile )
		{
			if ( filehashLocal == *pFileHash )
			{
				m_cMatchedFileFullHash++;
				return true;
			}
			else
			{
				// don't need to check anything else
				// did not match - record so that we have a record of the file that did not match ( just for reporting )
				AddUserReportedFileHash( idxFile, pFileHash, userID, false );
				FileRenderHelper( userID, "file does not match", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, &filehashLocal );
				return false;
			}
		}
	}

	// if this is a VPK file, we have completely cataloged all the VPK files, so no suprises are allowed
	if ( pFileHash->m_PackFileID )
	{
		AddUserReportedFileHash( idxFile, pFileHash, userID, false );
		FileRenderHelper( userID, "unrecognized vpk file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, nullptr );
		return false;
	}


	// now lets see if we have a master file hash for this
	if ( itMaster != m_treeMasterFileHashes.end() )
	{
		m_cMatchedMasterFile++;

		const FileHash_t *pFileHashLocal = &itMaster->m_FileHash;
		if ( *pFileHashLocal == *pFileHash )
		{
			m_cMatchedMasterFileHash++;
			return true;
		}
		else
		{
			// did not match - record so that we have a record of the file that did not match ( just for reporting )
			AddUserReportedFileHash( idxFile, pFileHash, userID, false );
			// and then return failure
			FileRenderHelper( userID, "file does not match server master file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, pFileHashLocal );
			return false;
		}
	}

	// no master record, accumulate individual record so we can get a consensus
	if ( sv_pure_trace.GetInt() == 3 )
	{
		FileRenderHelper( userID, "server does not have hash for this file. Waiting for consensus", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, nullptr );
	}

	AddUserReportedFileHash( idxFile, pFileHash, userID, true );

	// we dont have enough data to decide if you match or not yet - so we call it a match
	return true;
}


int CPureFileTracker::ListUserFiles( bool bListAll, const char *pchFilenameFind )
{
	std::flat_set< int > m_vecReportedFiles;
	for ( const auto &[ufile, idxFindFile] : m_treeAllReportedFiles )
	{
		if ( pchFilenameFind && V_stristr( ufile.m_filename.c_str(), pchFilenameFind ) )
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.c_str(), ufile.m_filename.c_str(), 0, &filehashLocal );

			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				USERID_t useridFake = { 0, 0 };
				FileRenderHelper( useridFake, "Found: ",ufile.m_path.c_str(),ufile.m_filename.c_str(), &filehashLocal, 0, nullptr );
				m_vecReportedFiles.insert( idxFindFile );
			}
			else
			{
				Msg( "File not found %s %s %x\n", ufile.m_filename.c_str(), ufile.m_path.c_str(), idxFindFile );
			}
		}
	}


	int cTotalFiles = 0;
	int cTotalMatches = 0;
	for ( auto it = m_treeUserReportedFileHash.begin(); it != m_treeUserReportedFileHash.end(); ++it )
	{
		const UserReportedFileHash_t &file = *it;

		int ctMatches = 1;
		int ctFiles = 1;
		// check this against all others for the same file
		for ( auto itNext = std::next( it ); itNext != m_treeUserReportedFileHash.end() && file.m_idxFile == itNext->m_idxFile; ++itNext )
		{
			if ( file.m_FileHash == itNext->m_FileHash )
			{
				ctMatches++;
				cTotalMatches++;
			}
			ctFiles++;
		}
		cTotalFiles++;

		// do we have a master for this one?
		MasterFileHash_t masterFileHashFind;
		masterFileHashFind.m_idxFile = file.m_idxFile;
		auto itMaster = m_treeMasterFileHashes.find( masterFileHashFind );

		const UserReportedFile_t &ufile = *m_vecReportedFilesByIndex[file.m_idxFile];

		bool bOutput = false;
		if ( V_stristr( ufile.m_filename.c_str(), "bin\\pak01" )!=nullptr || V_stristr( ufile.m_filename.c_str(), ".vpk" )!=nullptr )
			bOutput = true;
		else
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.c_str(), ufile.m_filename.c_str(), 0, &filehashLocal );
			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					file.m_FileHash.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					filehashLocal != file.m_FileHash )
				{
					bOutput = true;
				}
			}
		}

		if ( ctMatches != ctFiles || itMaster != m_treeMasterFileHashes.end() || bListAll || ( pchFilenameFind && m_vecReportedFiles.contains( file.m_idxFile ) ) || bOutput )
		{
			char rgch[256];
			V_sprintf_safe( rgch, "reports=%d matches=%d Hash details:", ctFiles, ctMatches );
			const FileHash_t *pFileHashMaster = nullptr;
			if ( itMaster != m_treeMasterFileHashes.end() )
				pFileHashMaster = &itMaster->m_FileHash;
			FileRenderHelper( file.m_userID, rgch, ufile.m_path.c_str(), ufile.m_filename.c_str(), &file.m_FileHash, 0, pFileHashMaster );
		}
	}
	Msg( "Total user files %d %d %d \n", (int)m_treeUserReportedFileHash.size(), cTotalFiles, cTotalMatches );
	Msg( "Total files %d, total with authoritative hashes %d \n", (int)m_treeAllReportedFiles.size(), (int)m_treeMasterFileHashes.size() );
	Msg( "Matching files %d %d %d \n", m_cMatchedFile, m_cMatchedMasterFile, m_cMatchedMasterFileHash );

	return 0;
}

int CPureFileTracker::ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax )
{
	g_pFileSystem->MarkAllCRCsUnverified();
	
	int cTotal = 0;
	int cTotalMatch = 0;
	int count = 0;
	do
	{
		CUnverifiedFileHash rgUnverifiedFiles[1];
		count = g_pFileSystem->GetUnverifiedFileHashes( rgUnverifiedFiles, std::size( rgUnverifiedFiles ) );

		if ( count && ( bListAll || ( pchFilenameFind && V_stristr( rgUnverifiedFiles[0].m_Filename, pchFilenameFind ) && rgUnverifiedFiles[0].m_nFileFraction >= nFileFractionMin && rgUnverifiedFiles[0].m_nFileFraction <= nFileFractionMax ) ) )
		{
			USERID_t useridFake = { 0, 0 };
			FileRenderHelper( useridFake, "", rgUnverifiedFiles[0].m_PathID, rgUnverifiedFiles[0].m_Filename, &rgUnverifiedFiles[0].m_FileHash, rgUnverifiedFiles[0].m_nFileFraction, nullptr );
			if ( rgUnverifiedFiles[0].m_FileHash.m_PackFileID )
			{
				g_pFileSystem->CheckVPKFileHash( rgUnverifiedFiles[0].m_FileHash.m_PackFileID, rgUnverifiedFiles[0].m_FileHash.m_nPackFileNumber, rgUnverifiedFiles[0].m_nFileFraction, rgUnverifiedFiles[0].m_FileHash.m_md5contents );
			}
			cTotalMatch++;
		}
		if ( count )
			cTotal++;
	} while ( count );

	Msg( "Total files %d Matching files %d \n", cTotal, cTotalMatch );

	return 0;
}


CPureFileTracker g_PureFileTracker;

void CC_ListPureServerFiles(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( args.ArgC() > 1 && (std::atoi(args[1]) > 0), nullptr );
}

static ConCommand svpurelistuserfiles("sv_pure_listuserfiles", CC_ListPureServerFiles, "ListPureServerFiles");


void CC_PureServerFindFile(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( false, args[1] );
}

static ConCommand svpurefinduserfiles("sv_pure_finduserfiles", CC_PureServerFindFile, "ListPureServerFiles");

void CC_PureServerListTrackedFiles(const CCommand &args)
{
	// BUGBUG! Because this code is in engine instead of server, it exists in the client - ugh!
	// Remove this command from client before shipping for realz.
	//if ( !sv.IsDedicated() )
	//	return;
	int nFileFractionMin = args.ArgC() >= 3 ? V_atoi(args[2]) : 0;
	int nFileFractionMax = args.ArgC() >= 4 ? V_atoi(args[3]) : nFileFractionMin;
	if ( nFileFractionMax < 0 ) 
		nFileFractionMax = 0x7FFFFFFF;
	g_PureFileTracker.ListAllTrackedFiles( args.ArgC() <= 1, args.ArgC() >= 2 ? args[1] : nullptr, nFileFractionMin, nFileFractionMax );
}

static ConCommand svpurelistfiles("sv_pure_listfiles", CC_PureServerListTrackedFiles, "ListPureServerFiles");

void CC_PureServerCheckVPKFiles(const CCommand &args)
{
	if ( sv.IsDedicated() )
		BeginWatchdogTimer( 5 * 60 );							// reset watchdog timer to allow 5 minutes for the VPK check
	g_pFileSystem->CacheAllVPKFileHashes( false, true );
	if ( sv.IsDedicated() )
		EndWatchdogTimer();
}

static ConCommand svpurecheckvpks("sv_pure_checkvpk", CC_PureServerCheckVPKFiles, "CheckPureServerVPKFiles");
