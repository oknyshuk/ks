// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.tier1;
#include "vpklib/packedstore.h"
#include "packedstore_internal.h"
#include "tier2/fileutils.h"
import std;



typedef uint16 PackFileIndex_t;
constexpr auto PACKFILEINDEX_END = 0xffff;


#pragma pack(1)
struct CFilePartDescr
{
	PackFileIndex_t m_nFileNumber;
	uint32 m_nFileDataOffset;
	uint32 m_nFileDataSize;
};


struct CFileHeaderFixedData
{
	uint32 m_nFileCRC;
	uint16 m_nMetaDataSize;
	CFilePartDescr m_PartDescriptors[1];					// variable length

	[[gnu::always_inline]] inline const void *MetaData( void ) const;
	
	[[gnu::always_inline]] inline const CFilePartDescr *FileData( int nPart = 0 ) const;

	uint32 TotalDataSize( void ) const
	{
		return m_nMetaDataSize + m_PartDescriptors[0].m_nFileDataSize;
	}

	std::size_t HeaderSizeIncludingMetaData( void ) const
	{
		std::size_t nRet = sizeof( *this ) - sizeof( m_PartDescriptors ) + m_nMetaDataSize;
		// see how many parts we have and count the size of their descriptors
		CFilePartDescr const *pPart = m_PartDescriptors;
		while( pPart->m_nFileNumber != PACKFILEINDEX_END )
		{
			nRet += sizeof( CFilePartDescr );
			pPart++;
		}
		nRet += sizeof( PackFileIndex_t );					// count terminator
		return nRet;
	}

};
#pragma pack()


static int s_FileHeaderSize( char const *pName, int nNumDataParts, int nNumMetaDataBytes )
{
	return 1 + std::strlen( pName ) + 							// name plus nul
		sizeof( uint32 ) +									// file crc
		sizeof( uint16 ) + 									// meta data size
		nNumMetaDataBytes +									// metadata
		nNumDataParts * sizeof( CFilePartDescr ) +			// part data
		sizeof( PackFileIndex_t );							// part data 0xff end marker
}

static int SkipFile( char const * &pData )					// returns highest file index
{
	int nHighestChunkIndex = -1;
	pData += 1 + V_strlen( pData );
	pData += sizeof( uint32 );
	int nMetaDataSize = *(reinterpret_cast<uint16 const *>( pData ) );
	pData += sizeof( uint16 );
	while ( *( ( PackFileIndex_t const *) pData ) != PACKFILEINDEX_END )
	{
		int nIdx = reinterpret_cast<CFilePartDescr const *>(pData)->m_nFileNumber;

		if ( nIdx != VPKFILENUMBER_EMBEDDED_IN_DIR_FILE )
			nHighestChunkIndex = std::max( nHighestChunkIndex, nIdx  );
		pData += sizeof( CFilePartDescr );
	}
	pData += sizeof( PackFileIndex_t );
	pData += nMetaDataSize;
	return nHighestChunkIndex;
}

static inline int SkipAllFilesInDir( char const * & pData )
{
	int nHighestChunkIndex = -1;
	pData += 1 + std::strlen( pData );					// skip dir name
	// now, march through all the files
	while( *pData )									// until we're out of files to look at
	{
		int nSkipIndex = SkipFile( pData );
		nHighestChunkIndex = std::max( nHighestChunkIndex, nSkipIndex );
	}
	pData++;												// skip end marker
	return nHighestChunkIndex;
}


CFileHeaderFixedData *CPackedStore::FindFileEntry( char const *pDirname, char const *pBaseName, char const *pExtension, uint8 **pExtBaseOut , uint8 **pNameBaseOut )
{
	if ( pExtBaseOut )
		*pExtBaseOut = nullptr;
	if ( pNameBaseOut )
		*pNameBaseOut = nullptr;

	auto itExt = m_DirsByExtension.find( pExtension );
	if ( itExt != m_DirsByExtension.end() )
	{
		auto itDir = itExt->second.find( pDirname );
		if ( itDir != itExt->second.end() )
		{
			if ( pExtBaseOut )
				*pExtBaseOut = (uint8 *) itDir->data();
			// we found the right directory. now, sequential search. data is heavily packed, so
			// this is a little awkward. See fileformat.txt
			char const *pData = itDir->data();
			pData += 1 + std::strlen( pData );					// skip dir name
			// now, march through all the files
			while( *pData )									// until we're out of files to look at
			{
				if ( !V_strcmp( pData, pBaseName ) )		// found it?
				{
					if ( pNameBaseOut )
						*pNameBaseOut = (uint8 *) pData;

					return ( CFileHeaderFixedData * )( pData + 1 + V_strlen( pData ) ); // return header
				}
				// this isn't it - skip over it
				SkipFile( pData );
			}
		}
	}
	return nullptr;
}


const void *CFileHeaderFixedData::MetaData( void ) const
{
	if ( ! m_nMetaDataSize )
		return nullptr;
	const CFilePartDescr *ret = &( m_PartDescriptors[0] );
	while( ret->m_nFileNumber != PACKFILEINDEX_END )
		ret++;
	return reinterpret_cast<uint8 const *>( ret ) + sizeof( PackFileIndex_t );
}
	
CFilePartDescr const *CFileHeaderFixedData::FileData( int nPart ) const
{
	return m_PartDescriptors + nPart;
}

void CPackedStore::Init( void )
{
	m_nHighestChunkFileIndex = -1;
	m_bUseDirFile = false;
	m_pszFileBaseName[0] = 0;
	m_pszFullPathName[0] = 0;
	m_DirsByExtension.clear();
	m_nDirectoryDataSize = 0;
	m_nWriteChunkSize = k_nVPKDefaultChunkSize;

	m_nSizeOfSignedData = 0;
	m_Signature.clear(); m_Signature.shrink_to_fit();
	m_SignaturePrivateKey.clear(); m_SignaturePrivateKey.shrink_to_fit();
	m_SignaturePublicKey.clear(); m_SignaturePublicKey.shrink_to_fit();
}
   
void CPackedStore::BuildHashTables( void )
{
	m_nHighestChunkFileIndex = -1;
	m_DirsByExtension.clear();
	char const *pData = reinterpret_cast< char const *>( DirectoryData() );
	while( *pData )
	{
		// for each extension
		auto &dirs = m_DirsByExtension[pData];
		// now, iterate over all directories associated with this extension
		pData += 1 + std::strlen( pData );
		while( *pData )
		{
			dirs.insert( pData );
			int nDirChunk = SkipAllFilesInDir( pData );
			m_nHighestChunkFileIndex = std::max( m_nHighestChunkFileIndex, nDirChunk );
		}
		// step past \0
		pData++;
	}
}


bool CPackedStore::IsEmpty( void ) const
{
	return ( std::ssize( m_DirectoryData ) <= 1 );
}

static void StripTrailingString( char *pszBuf, const char *pszStrip )
{
	int lBuf = V_strlen( pszBuf );
	int lStrip = V_strlen( pszStrip );
	if ( lBuf < lStrip )
		return;
	char *pExpectedPos = pszBuf + lBuf - lStrip;
	if ( V_stricmp( pExpectedPos, pszStrip ) == 0 )
		*pExpectedPos = '\0';
}

CPackedStore::CPackedStore( char const *pFileBasename, char *pszFName, IBaseFileSystem *pFS, bool bOpenForWrite ):m_PackedStoreReadCache( pFS )
{
	Init();
	m_pFileSystem = pFS;
	m_PackedStoreReadCache.m_pPackedStore = this;
	m_DirectoryData.push_back( 0 );

	if ( pFileBasename )
	{
		V_strcpy( m_pszFileBaseName, pFileBasename );
		StripTrailingString( m_pszFileBaseName, ".vpk" );
		StripTrailingString( m_pszFileBaseName, "_dir" );
		std::sprintf( pszFName, "%s_dir.vpk", m_pszFileBaseName );
		CInputFile dirFile( pszFName );

		// Try to load the VPK as a standalone (probably an addon) even if the standard _dir name is not present
		if ( dirFile.IsOk() )
		{
			m_bUseDirFile = true;
		}
		else
		{
			m_bUseDirFile = false;
			std::sprintf( pszFName, "%s.vpk", m_pszFileBaseName );
			dirFile.Open( pszFName );
		}

		bool bNewFileFormat = false;
		if ( dirFile.IsOk() )
		{
			// first, check if it is the new versioned variant
			VPKDirHeader_t dirHeader;
			// try to read the header.
			if ( 
				( dirFile.Read( &dirHeader, sizeof( dirHeader ) ) == sizeof( dirHeader ) ) &&
				( dirHeader.m_nHeaderMarker == VPK_HEADER_MARKER ) )
			{
				if ( dirHeader.m_nVersion == VPK_PREVIOUS_VERSION ) 
				{
					// fill in the fields of the new header.
					dirHeader.m_nEmbeddedChunkSize = dirFile.Size() - dirHeader.m_nDirectorySize - sizeof( VPKDirHeaderOld_t );
					dirHeader.m_nChunkHashesSize = 0;
					dirHeader.m_nSelfHashesSize = 0;
					dirHeader.m_nSignatureSize = 0;
					// pretend we didnt read the extra header
					dirFile.Seek( sizeof( VPKDirHeaderOld_t ) );
				}
				else if ( dirHeader.m_nVersion != VPK_CURRENT_VERSION )
				{
					Error( "Unknown version %d for vpk %s", dirHeader.m_nVersion, pFileBasename );
				}
				bNewFileFormat = true;
			}
			else											// its an old file
			{
				dirFile.Seek( 0 );
				// fill in a fake header, zero out garbage we read
				dirHeader.m_nDirectorySize = dirFile.Size();
				dirHeader.m_nEmbeddedChunkSize = 0;
				dirHeader.m_nChunkHashesSize = 0;
				dirHeader.m_nSelfHashesSize = 0;
				dirHeader.m_nSignatureSize = 0;
			}
			uint32 nSizeOfHeader = dirFile.Tell();
			int nSize = dirHeader.m_nDirectorySize;
			m_nDirectoryDataSize = dirHeader.m_nDirectorySize;
			// Flush out the existing allocation so that we allocate exactly the right size.
			// This saves about 3 MB of address space currently (5.1 MB was rounded up to 8 MB).
			m_DirectoryData.clear(); m_DirectoryData.shrink_to_fit();
			m_DirectoryData.assign( nSize, {} );
			dirFile.MustRead( DirectoryData(), nSize );
			// now, if we are opening for write, read the entire contents of the embedded data chunk in the dir into ram
			if ( bOpenForWrite && bNewFileFormat )
			{
				if ( dirHeader.m_nEmbeddedChunkSize )
				{
					std::vector< uint8 > readBuffer;
					int nRemainingSize = dirHeader.m_nEmbeddedChunkSize;


					// We'll allocate around half a meg of contiguous memory for the read. Any more and the SDK's VPK 
					// utility has a higher chance of choking on low-end machines.
					readBuffer.assign( 524288, {} );

					while ( nRemainingSize > 0 ) 
					{
						int nReadSize = std::min( nRemainingSize , 524288 );

						dirFile.MustRead( readBuffer.data(), nReadSize );

						for ( int i = 0; i < nReadSize; i++ )
						{
							m_EmbeddedChunkData.push_back( readBuffer[i] );
						}
						nRemainingSize -= nReadSize;
					}
				}
			}
			int cbVecHashes = dirHeader.m_nChunkHashesSize;
			int ctHashes = cbVecHashes/sizeof(m_vecChunkHashFraction[0]);
			ks::ensure_size( m_vecChunkHashFraction, ctHashes );
			dirFile.MustRead( m_vecChunkHashFraction.data(), cbVecHashes );
			// strictly increasing: sorted, no duplicates
			contract_assert( std::ranges::adjacent_find( m_vecChunkHashFraction, std::not_fn( ChunkHashFractionLess_t() ) ) == m_vecChunkHashFraction.end() );

			// now read the self hashes
			V_memset( m_DirectoryMD5.bits, 0, sizeof(m_DirectoryMD5.bits) );
			V_memset( m_ChunkHashesMD5.bits, 0, sizeof(m_ChunkHashesMD5.bits) );
			V_memset( m_TotalFileMD5.bits, 0, sizeof(m_TotalFileMD5.bits) );
			if ( dirHeader.m_nSelfHashesSize == 3*sizeof(m_DirectoryMD5.bits) )
			{
				// first is an MD5 of directory data
				dirFile.MustRead( m_DirectoryMD5.bits, sizeof(m_DirectoryMD5.bits) );
				// next is an MD5 of 
				dirFile.MustRead( m_ChunkHashesMD5.bits, sizeof(m_ChunkHashesMD5.bits) );
				// at this point the filesystem has calculated an MD5 of everything in the file up to this point.
				// we could ask it for a snapshot of that MD5 value and then be able to compare it to m_TotalFileMD5
				// but we would have to do it *before* we read it
				dirFile.MustRead( m_TotalFileMD5.bits, sizeof(m_TotalFileMD5.bits) );
			}

			// Is there a signature?
			m_nSizeOfSignedData = 0;
			if ( dirHeader.m_nSignatureSize != 0 )
			{

				// Everything immediately proceeeding it should have been signed.
				m_nSizeOfSignedData = dirFile.Tell();
				uint32 nExpectedSignedSize = nSizeOfHeader + dirHeader.ComputeSizeofSignedDataAfterHeader();
				if ( m_nSizeOfSignedData != nExpectedSignedSize )
				{
					Error( "Size mismatch determining size of signed data block (%d vs %d)", m_nSizeOfSignedData, nExpectedSignedSize );
				}

				// Read the public key
				uint32 cubPublicKey = 0;
				dirFile.MustRead( &cubPublicKey, sizeof(cubPublicKey) );
				m_SignaturePublicKey.assign( cubPublicKey, {} );
				dirFile.MustRead( m_SignaturePublicKey.data(), cubPublicKey );

				// Read the private key
				uint32 cubSignature = 0;
				dirFile.MustRead( &cubSignature, sizeof(cubSignature) );
				m_Signature.assign( cubSignature, {} );
				dirFile.MustRead( m_Signature.data(), cubSignature );
			}
		}
		V_MakeAbsolutePath( m_pszFullPathName, sizeof( m_pszFullPathName ), m_pszFileBaseName );
		V_strcat_safe( m_pszFullPathName, ".vpk" );
		//V_strlower( m_pszFullPathName ); // NO!  this screws up linux.
		V_FixSlashes( m_pszFullPathName );
	}
	BuildHashTables();
}


void CPackedStore::GetDataFileName( char *pchFileNameOut, int cchFileNameOut, int nFileNumber ) const
{
	if ( nFileNumber == VPKFILENUMBER_EMBEDDED_IN_DIR_FILE )
	{
		if ( m_bUseDirFile )
		{
			V_snprintf( pchFileNameOut, cchFileNameOut, "%s_dir.vpk", m_pszFileBaseName );
		}
		else
		{
			V_snprintf( pchFileNameOut, cchFileNameOut, "%s.vpk", m_pszFileBaseName );
		}
	}
	else
	{
		V_snprintf( pchFileNameOut, cchFileNameOut, "%s_%03d.vpk", m_pszFileBaseName, nFileNumber );
	}

}

CPackedStore::~CPackedStore( void )
{
	for (int i = 0; i < std::size( m_FileHandles ); i++ )
	{
		if ( m_FileHandles[i].m_nFileNumber != -1 )
		{
			m_pFileSystem->Close( m_FileHandles[i].m_hFileHandle );

		}
	}

	m_directoryList.clear();
	m_dirContents.clear();
}

void SplitFileComponents( char const *pFileName, char *pDirOut, char *pBaseOut, char *pExtOut )
{
	char pTmpDirOut[MAX_PATH];
	V_ExtractFilePath( pFileName, pTmpDirOut, MAX_PATH );
	// now, pTmpDirOut to pDirOut, except when we find more then one '\' in a row, only output one
	char *pOutDirPtr = pDirOut;
	for( char *pDirInPtr = pTmpDirOut; *pDirInPtr; pDirInPtr++ )
	{
		char c = *( pDirInPtr );
		*( pOutDirPtr++ ) = c;
		// if we copied a \, skip all subsequent slashes
		while( ( c == '\\'  ) && ( pDirInPtr[1] == c ) )
		{
			pDirInPtr++;
		}
	}
	*( pOutDirPtr ) = 0;									// null terminate

	if ( !pDirOut[0] )
		std::strcpy( pDirOut, " " );								// blank dir name
	V_strcpy( pBaseOut, V_UnqualifiedFileName( pFileName ) );
	char *pDot = std::strrchr( pBaseOut, '.' );
	if ( pDot )
	{
		*pDot = 0;
		V_strncpy( pExtOut, pDot+1, MAX_PATH );
	}
	else
	{
		pExtOut[0]=' ';
		pExtOut[1]=0;
	}

	V_FixSlashes( pDirOut, '/' );
	V_strlower( pDirOut );

	// the game sometimes asks for paths like dir1/../dir2/ we will replace this with dir2/. This
	// one line of perl code sucks in c++.
	for(;;)
	{
		char *pDotDot = V_strstr( pDirOut + 1, "/../" );		// start at second char. we don't want a beginning /
		if (! pDotDot )
		{
			break;
		}
		// search backwards from the /.. for the previous directory part
		char *pPrevSlash = pDotDot - 1;
		while( ( pPrevSlash > pDirOut ) && ( pPrevSlash[0] != '/' ) )
		{
			pPrevSlash--;
		}
		// if our path was dir0/dir1/../dir2, we are now pointing at "/dir1".
		// is strmove in all compilers? that would be better than this loop
		char *pStrIn = pDotDot + 3;
		for(;;)
		{
			*pPrevSlash = *pStrIn;
			if ( pStrIn[0] )
			{
				++pPrevSlash;
				++pStrIn;
			}
			else
			{
				break;
			}
		}
	}

	
	char *pLastDirChar = pDirOut + std::strlen( pDirOut ) - 1;
	if ( ( pLastDirChar[0] == '/' ) || ( pLastDirChar[0] == '\\' ) )
		*pLastDirChar = 0;									// kill trailing slash
	V_strlower( pBaseOut );
	V_strlower( pExtOut );
}

CPackedStoreFileHandle CPackedStore::OpenFile( char const *pFileName )
{
	char dirName[MAX_PATH];
	char baseName[MAX_PATH];
	char extName[MAX_PATH];

	// Fix up the filename first
	char tempFileName[MAX_PATH];

	V_strncpy( tempFileName, pFileName, sizeof( tempFileName ) );
	V_FixSlashes( tempFileName, CORRECT_PATH_SEPARATOR );
//	V_RemoveDotSlashes( tempFileName, CORRECT_PATH_SEPARATOR, true );
	V_FixDoubleSlashes( tempFileName );
	if ( !V_IsAbsolutePath( tempFileName ) )
	{
		V_strlower( tempFileName );
	}
	
	SplitFileComponents( tempFileName, dirName, baseName, extName );

	CPackedStoreFileHandle ret;

	CFileHeaderFixedData *pHeader = FindFileEntry( dirName, baseName, extName, nullptr, &( ret.m_pDirFileNamePtr ) );
	
	if ( pHeader )
	{
		ret.m_nFileNumber = pHeader->m_PartDescriptors[0].m_nFileNumber;
		ret.m_nFileOffset = pHeader->m_PartDescriptors[0].m_nFileDataOffset;
		ret.m_nFileSize = pHeader->m_PartDescriptors[0].m_nFileDataSize + pHeader->m_nMetaDataSize;
		ret.m_nCurrentFileOffset = 0;
		ret.m_pMetaData = pHeader->MetaData();
		ret.m_nMetaDataSize = pHeader->m_nMetaDataSize;
  		ret.m_pHeaderData = pHeader;
		ret.m_pOwner = this;
	}
	else
	{
		ret.m_nFileNumber = -1;
		ret.m_pOwner = nullptr;
	}
	return ret;

}

CPackedStoreFileHandle CPackedStore::GetHandleForHashingFiles()
{
	CPackedStoreFileHandle ret;
	ret.m_nFileNumber = 0;
	ret.m_nFileOffset = 0;
	ret.m_nFileSize = 0;
	ret.m_nMetaDataSize = 0;
	ret.m_nCurrentFileOffset = 0;
	ret.m_pDirFileNamePtr = nullptr;
	ret.m_pHeaderData = nullptr;
	ret.m_pMetaData = nullptr;
	ret.m_pOwner = this;
	return ret;
}

void CPackedStore::Write( void )
{
	// !KLUDGE!
	// Write the whole header into a buffer in memory.
	// We do this so we can easily sign it.
	ks::byte_writer bufDirFile;

	VPKDirHeader_t headerOut;
	headerOut.m_nDirectorySize = std::ssize( m_DirectoryData );
	headerOut.m_nEmbeddedChunkSize = std::ssize( m_EmbeddedChunkData );
	headerOut.m_nChunkHashesSize = std::ssize( m_vecChunkHashFraction )*sizeof(m_vecChunkHashFraction[0]);
	headerOut.m_nSelfHashesSize = 3*sizeof(m_DirectoryMD5.bits);
	headerOut.m_nSignatureSize = 0;

	// Do we plan on signing this thing and writing a signature?
	m_Signature.clear(); m_Signature.shrink_to_fit();
	if ( !m_SignaturePrivateKey.empty() && std::ssize( m_SignaturePublicKey ) > 0 )
	{
			Error( "VPK signing not implemented" );
	}

	bufDirFile.write_bytes( &headerOut, sizeof( headerOut ) );
	bufDirFile.write_bytes( DirectoryData(), std::ssize( m_DirectoryData ) );

	if ( std::ssize( m_EmbeddedChunkData ) )
	{
		int nRemainingSize = std::ssize( m_EmbeddedChunkData );
		std::vector< uint8 > writeBuffer;
		
		writeBuffer.assign( 524288, {} );
		int nChunkOffset = 0;

		while ( nRemainingSize > 0 )
		{
			// We'll write around half a meg of contiguous memory at once. Any more and the SDK's VPK 
			// utility has a higher chance of choking on low-end machines.
			int nWriteSize = std::min( nRemainingSize, 524288 );
			
			for ( int i = 0; i < nWriteSize; i++ )
			{
				writeBuffer[i] = m_EmbeddedChunkData[nChunkOffset++];
			}

			bufDirFile.write_bytes( writeBuffer.data(), nWriteSize );
			nRemainingSize -= nWriteSize;
		}
	}

	// write the chunk hashes out
	bufDirFile.write_bytes( m_vecChunkHashFraction.data(), std::ssize( m_vecChunkHashFraction )*sizeof(m_vecChunkHashFraction[0]) );

	// write out the MD5s of the 2 main pieces of data
	bufDirFile.write_bytes( m_DirectoryMD5.bits, sizeof( m_DirectoryMD5.bits ) );
	bufDirFile.write_bytes( m_ChunkHashesMD5.bits, sizeof( m_ChunkHashesMD5.bits ) );

	// compute the final MD5 ( of everything in the file up to this point )
	MD5_ProcessSingleBuffer( bufDirFile.data(), bufDirFile.size(), m_TotalFileMD5 );
	bufDirFile.write_bytes( m_TotalFileMD5.bits, sizeof( m_TotalFileMD5.bits ) );

	// Should we sign all this stuff?
	m_nSizeOfSignedData = 0;

	char szOutFileName[MAX_PATH];

	// Delete any existing header file, either the standalone kind,
	// or the _dir kind.
	V_sprintf_safe( szOutFileName, "%s.vpk", m_pszFileBaseName );
	if ( g_pFullFileSystem->FileExists( szOutFileName ) )
		g_pFullFileSystem->RemoveFile( szOutFileName );
	V_sprintf_safe( szOutFileName, "%s_dir.vpk", m_pszFileBaseName );
	if ( g_pFullFileSystem->FileExists( szOutFileName ) )
		g_pFullFileSystem->RemoveFile( szOutFileName );

	// Force on multi-chunk mode if we have any files in a chunk
	if ( m_nHighestChunkFileIndex >= 0 )
		m_bUseDirFile = true;


	// Fetch actual name to write
	GetDataFileName( szOutFileName, sizeof(szOutFileName), VPKFILENUMBER_EMBEDDED_IN_DIR_FILE );

	// Now actually write the data to disk
	COutputFile dirFile( szOutFileName );
	dirFile.Write( bufDirFile.data(), bufDirFile.size() );
	dirFile.Close();
}

CPackedStoreReadCache::CPackedStoreReadCache( IBaseFileSystem *pFS )
{
	m_pPackedStore = nullptr;
	m_cItemsInCache = 0;
	m_pFileSystem = pFS;
	m_cubReadFromCache = 0;
	m_cReadFromCache = 0;
	m_cDiscardsFromCache = 0;
	m_cAddedToCache = 0;
	m_cCacheMiss = 0;
	m_cubCacheMiss = 0;
	m_cFileErrors = 0;
	m_cFileErrorsCorrected = 0;
	m_cFileResultsDifferent = 0;
}

// check if the read request can be satisfied from the read cache we have in 1MB chunks
bool CPackedStoreReadCache::BCanSatisfyFromReadCache( uint8 *pOutData, CPackedStoreFileHandle &handle, FileHandleTracker_t &fHandle, int nDesiredPos, int nNumBytes, int &nRead )
{
#ifdef DEDICATED
	// Never use the read cache on dedicated servers. This saves memory. We rely
	// on the OS disk cache which will be shared by all server processes.
	return false;
#else
	nRead = 0;
	int nFileFraction = nDesiredPos & k_nCacheBufferMask;
	int nOffset = nDesiredPos - nFileFraction;
	int cubReadChunk = nOffset + nNumBytes;
	if ( cubReadChunk > k_cubCacheBufferSize )
		cubReadChunk = ( k_nCacheBufferMask - nOffset ) & (k_cubCacheBufferSize-1);
	else
		cubReadChunk = nNumBytes;
	// the request might straddle multiple chunks - we make sure we have all of the data, if we are missing any, we fail
	while ( nNumBytes )
	{
		int nReadChunk = 0;
		if ( !BCanSatisfyFromReadCacheInternal( pOutData, handle, fHandle, nDesiredPos, cubReadChunk, nReadChunk ) )
		{
			return false;
		}
		nNumBytes -= cubReadChunk;
		pOutData += cubReadChunk;
		nDesiredPos += cubReadChunk;
		nRead += nReadChunk;
		nFileFraction += k_cubCacheBufferSize;
		cubReadChunk = nNumBytes;
		if ( cubReadChunk > k_cubCacheBufferSize )
			cubReadChunk = k_cubCacheBufferSize;
	}
	return true;
#endif
}


// read a single line into the cache
bool CPackedStoreReadCache::ReadCacheLine( FileHandleTracker_t &fHandle, CachedVPKRead_t &cachedVPKRead, int &nRead )
{
	m_pFileSystem->Seek( fHandle.m_hFileHandle, cachedVPKRead.m_nFileFraction, FILESYSTEM_SEEK_HEAD );
	nRead = m_pFileSystem->Read( cachedVPKRead.m_pubBuffer, 1024*1024, fHandle.m_hFileHandle );
	m_pFileSystem->Seek( fHandle.m_hFileHandle, fHandle.m_nCurOfs, FILESYSTEM_SEEK_HEAD );
	cachedVPKRead.m_cubBuffer = nRead;
	cachedVPKRead.m_hMD5RequestHandle = m_pFileTracker->SubmitThreadedMD5Request( cachedVPKRead.m_pubBuffer, cachedVPKRead.m_cubBuffer, m_pPackedStore->m_PackFileID, cachedVPKRead.m_nPackFileNumber, cachedVPKRead.m_nFileFraction );
	return true;
}


// check if the MD5 matches
bool CPackedStoreReadCache::CheckMd5Result( CachedVPKRead_t &cachedVPKRead, MD5Value_t &md5Value )
{
	ChunkHashFraction_t chunkHashFraction;
	if ( !m_pPackedStore->FindFileHashFraction( cachedVPKRead.m_nPackFileNumber, cachedVPKRead.m_nFileFraction, chunkHashFraction ) )
		return true;

	if ( cachedVPKRead.m_cFailedHashes > 0 )
	{
		if ( V_memcmp( &md5Value, &cachedVPKRead.m_md5Value, sizeof( MD5Value_t ) ) != 0 )
			m_cFileResultsDifferent++;
	}
	if ( cachedVPKRead.m_cFailedHashes == 0 )
		V_memcpy( &cachedVPKRead.m_md5Value, &md5Value, sizeof( MD5Value_t ) );
	else
		V_memcpy( &cachedVPKRead.m_md5ValueRetry, &md5Value, sizeof( MD5Value_t ) );

	if ( V_memcmp( &md5Value, &chunkHashFraction.m_md5contents, sizeof( MD5Value_t ) ) != 0	)
	{
		// we got an error reading this chunk, record the error
		m_cFileErrors++;
		cachedVPKRead.m_cFailedHashes++;
		// give a copy to the fail whale - ONLY the first time, we only want to retry once
		if ( cachedVPKRead.m_cFailedHashes == 1 )
			m_queueCachedVPKReadsRetry.PushItem( cachedVPKRead );
		return false;
	}
	if ( cachedVPKRead.m_cFailedHashes > 0 )
	{
		m_cFileErrorsCorrected++;
	}
	return true;
}


int CPackedStoreReadCache::FindBufferToUse()
{
	int idxLRU = 0;
	CachedVPKReadMap_t::iterator itToRemove = m_rgCurrentCacheIndex[0];
	uint32 uTimeLowest = (uint32)~0; // MAXINT
	// find the oldest item, reuse its buffer
	for ( int i = 0; i < m_cItemsInCache; i++ )
	{
		if ( m_rgLastUsedTime[i] < uTimeLowest )
		{
			uTimeLowest = m_rgLastUsedTime[i];
			itToRemove = m_rgCurrentCacheIndex[i];
			idxLRU = i;
		}
		// while we are here check if the MD5 is done
		if ( m_rgCurrentCacheIndex[i]->second.m_hMD5RequestHandle )
		{
			CachedVPKRead_t &cachedVPKRead = m_rgCurrentCacheIndex[i]->second;
			MD5Value_t md5Value;
			if ( m_pFileTracker->IsMD5RequestComplete( cachedVPKRead.m_hMD5RequestHandle, &md5Value ) )
			{
				// if it is done, check the results
				cachedVPKRead.m_hMD5RequestHandle = 0;
				// if we got bad data - stop looking, just use this one
				if ( !CheckMd5Result( cachedVPKRead, md5Value ) )
					return i;
			}
		}
	}

	// if we submitted its MD5 for processing, then wait until that is done
	if ( itToRemove->second.m_hMD5RequestHandle )
	{
		CachedVPKRead_t &cachedVPKRead = itToRemove->second;
		MD5Value_t md5Value;
		m_pFileTracker->BlockUntilMD5RequestComplete( cachedVPKRead.m_hMD5RequestHandle, &md5Value );
		cachedVPKRead.m_hMD5RequestHandle = 0;
		// make sure it matches what it is supposed to match
		CheckMd5Result( cachedVPKRead, md5Value );
	}
	return idxLRU;
}


// manage the cache
bool CPackedStoreReadCache::BCanSatisfyFromReadCacheInternal( uint8 *pOutData, CPackedStoreFileHandle &handle, FileHandleTracker_t &fHandle, int nDesiredPos, int nNumBytes, int &nRead )
{
	bool bSuccess = false;
	m_rwlock.lock_shared();
	bool bLockedForWrite = false;

	CachedVPKRead_t cachedVPKRead;
	cachedVPKRead.m_nPackFileNumber = handle.m_nFileNumber;
	cachedVPKRead.m_nFileFraction = nDesiredPos & k_nCacheBufferMask;
	const std::pair< int, int > key( cachedVPKRead.m_nPackFileNumber, cachedVPKRead.m_nFileFraction );
	auto itTrackedVPKFile = m_treeCachedVPKRead.find( key );
	if ( itTrackedVPKFile == m_treeCachedVPKRead.end() )
	{
		m_rwlock.unlock_shared();
		m_rwlock.lock();
		bLockedForWrite = true;
		// if we didnt find it, we had to grab the write lock, it may have been added while we waited
		itTrackedVPKFile = m_treeCachedVPKRead.find( key );
	}
	if ( itTrackedVPKFile == m_treeCachedVPKRead.end() )
	{
		// if we are over our limit, remove one and reuse the buffer
		cachedVPKRead.m_pubBuffer = nullptr;
		int idxLRU = -1;

		if ( m_cItemsInCache >= k_nCacheBuffersToKeep )
		{
			idxLRU = FindBufferToUse();
			CachedVPKRead_t &toRemove = m_rgCurrentCacheIndex[idxLRU]->second;

			cachedVPKRead.m_pubBuffer = toRemove.m_pubBuffer;
			toRemove.m_pubBuffer = nullptr;
			m_cDiscardsFromCache++;
		}
		else
		{
			idxLRU = m_cItemsInCache;
			m_cItemsInCache++;
		}
		if ( cachedVPKRead.m_pubBuffer == nullptr )
		{
			cachedVPKRead.m_pubBuffer = (uint8 *)MemAlloc_Alloc( k_cubCacheBufferSize );
		}
		cachedVPKRead.m_idxLRU = idxLRU;
		ReadCacheLine( fHandle, cachedVPKRead, nRead );
		itTrackedVPKFile = m_treeCachedVPKRead.emplace( key, cachedVPKRead ).first;
		m_cAddedToCache++;

		// this item is in the cache
		m_rgCurrentCacheIndex[idxLRU] = itTrackedVPKFile;
		m_rgLastUsedTime[idxLRU] = Plat_MSTime();
	}
	else
	{
		cachedVPKRead = itTrackedVPKFile->second;
		if ( cachedVPKRead.m_pubBuffer == nullptr )
		{
			// this chunk has been read, MD5ed, and then LRUd away
			// we will not read it again, we fall back to normal file I/O
			m_cCacheMiss ++;
			m_cubCacheMiss += nNumBytes;
			bSuccess = false;
		}
		else
		{
			m_cubReadFromCache += nNumBytes;
			m_cReadFromCache ++;
			m_rgLastUsedTime[itTrackedVPKFile->second.m_idxLRU] = Plat_MSTime();
		}
	}
	if ( cachedVPKRead.m_pubBuffer != nullptr && cachedVPKRead.m_cubBuffer + cachedVPKRead.m_nFileFraction >= nDesiredPos+nNumBytes )
	{
		int nOffset = nDesiredPos - cachedVPKRead.m_nFileFraction;
		std::memcpy( pOutData, (uint8 *)&cachedVPKRead.m_pubBuffer[nOffset], nNumBytes );
		nRead = nNumBytes;
		bSuccess = true;
	}
	if ( bLockedForWrite )
		m_rwlock.unlock();
	else
		m_rwlock.unlock_shared();

	return bSuccess;
}

// Reread the bad cache line - takes the fHandle lock
void CPackedStoreReadCache::RereadBadCacheLine( CachedVPKRead_t &cachedVPKRead )
{
	int nRead = cachedVPKRead.m_cubBuffer;
	cachedVPKRead.m_pubBuffer = (uint8 *)MemAlloc_Alloc( k_cubCacheBufferSize );
	FileHandleTracker_t &fHandle = m_pPackedStore->GetFileHandle( cachedVPKRead.m_nPackFileNumber );
	fHandle.m_Mutex.lock();
	ReadCacheLine( fHandle, cachedVPKRead, nRead );
	fHandle.m_Mutex.unlock();
}

// Recheck the MD5 of the cache line - takes the cache lock
void CPackedStoreReadCache::RecheckBadCacheLine( CachedVPKRead_t &cachedVPKRead )
{
	std::scoped_lock lock( m_rwlock );
	ChunkHashFraction_t chunkHashFraction;
	m_pPackedStore->FindFileHashFraction( cachedVPKRead.m_nPackFileNumber, cachedVPKRead.m_nFileFraction, chunkHashFraction );

	MD5Value_t md5ValueSecondTry;
	m_pFileTracker->BlockUntilMD5RequestComplete( cachedVPKRead.m_hMD5RequestHandle, &md5ValueSecondTry );
	cachedVPKRead.m_hMD5RequestHandle = 0;
	CheckMd5Result( cachedVPKRead, md5ValueSecondTry );
	cachedVPKRead.m_pubBuffer = nullptr;
	// m_listCachedVPKReadsFailed contains all the data about failed reads - for error or OGS reporting
	m_listCachedVPKReadsFailed.push_back( cachedVPKRead );
}

// try reloading anything that failed its md5 check
// this is currently only for gathering information, doesnt do anything to repair the cache
void CPackedStoreReadCache::RetryAllBadCacheLines()
{
	while( m_queueCachedVPKReadsRetry.Count() )
	{
		CachedVPKRead_t cachedVPKRead;
		if ( m_queueCachedVPKReadsRetry.PopItem( &cachedVPKRead ) )
		{
			// retry anything that didnt match one time
			RereadBadCacheLine( cachedVPKRead );
			RecheckBadCacheLine( cachedVPKRead );
		}
	}
}

void CPackedStore::GetPackFileLoadErrorSummary( std::string &sErrors )
{
	for ( const CachedVPKRead_t &failed : m_PackedStoreReadCache.m_listCachedVPKReadsFailed )
	{
		char szDataFileName[MAX_PATH];
		CPackedStoreFileHandle fhandle = GetHandleForHashingFiles();
		fhandle.m_nFileNumber = failed.m_nPackFileNumber;
		fhandle.GetPackFileName( szDataFileName, sizeof(szDataFileName) );
		const char *pszFileName = V_GetFileName( szDataFileName );

		std::string sTemp;
		sTemp = ks::format_printf( "Pack File %s at offset %x length %x errorcount = %d \n", 
			pszFileName,
			failed.m_nFileFraction, 
			failed.m_cubBuffer,
			failed.m_cFailedHashes );
		sErrors += sTemp ;
		
		char hex[sizeof(MD5Value_t)*2 + 1 ];
		V_binarytohex( failed.m_md5Value.bits, 
					sizeof(MD5Value_t),	hex, sizeof( hex ) );

		ChunkHashFraction_t chunkHashFraction;
		FindFileHashFraction( failed.m_nPackFileNumber, failed.m_nFileFraction, chunkHashFraction );

		char hex2[sizeof(MD5Value_t)*2 + 1 ];
		V_binarytohex( static_cast< byte* >( chunkHashFraction.m_md5contents.bits ), 
			sizeof(MD5Value_t),	hex2, sizeof( hex2 ) );
		sTemp = ks::format_printf( "Last Md5 Value %s Should be %s \n", hex, hex2 );

		sErrors += sTemp ;
	}
}

void CPackedStore::GetPackFileLoadErrorSummaryKV( KeyValues *pKV )
{
	pKV->SetInt( "BytesReadFromCache" ,		m_PackedStoreReadCache.m_cubReadFromCache );
	pKV->SetInt( "ItemsReadFromCache" ,		m_PackedStoreReadCache.m_cReadFromCache );
	pKV->SetInt( "DiscardsFromCache" ,		m_PackedStoreReadCache.m_cDiscardsFromCache );
	pKV->SetInt( "AddedToCache" ,			m_PackedStoreReadCache.m_cAddedToCache );
	pKV->SetInt( "CacheMisses" ,			m_PackedStoreReadCache.m_cCacheMiss );
	pKV->SetInt( "FileErrorCount" ,			m_PackedStoreReadCache.m_cFileErrors );
	pKV->SetInt( "FileErrorsCorrected" ,	m_PackedStoreReadCache.m_cFileErrorsCorrected );
	pKV->SetInt( "FileResultsDifferent" ,	m_PackedStoreReadCache.m_cFileResultsDifferent );

	for ( const CachedVPKRead_t &failed : m_PackedStoreReadCache.m_listCachedVPKReadsFailed )
	{
		char szDataFileName[MAX_PATH];
		CPackedStoreFileHandle fhandle = GetHandleForHashingFiles();
		fhandle.m_nFileNumber = failed.m_nPackFileNumber;
		fhandle.GetPackFileName( szDataFileName, sizeof(szDataFileName) );

		KeyValues *pKV1 = pKV->CreateNewKey();
		pKV1->SetInt( "PackFileID", m_PackFileID );
		pKV1->SetInt( "PackFileNumber", failed.m_nPackFileNumber );
		pKV1->SetInt( "FileFraction", failed.m_nFileFraction );

		char hex[sizeof(MD5Value_t)*2 + 1 ];
		V_binarytohex( failed.m_md5Value.bits, sizeof(MD5Value_t), hex, sizeof( hex ) );

		ChunkHashFraction_t chunkHashFraction;
		FindFileHashFraction( failed.m_nPackFileNumber, failed.m_nFileFraction, chunkHashFraction );

		char hex2[sizeof(MD5Value_t)*2 + 1 ];
		V_binarytohex( chunkHashFraction.m_md5contents.bits, sizeof(MD5Value_t), hex2, sizeof( hex2 ) );

		char hex3[sizeof(MD5Value_t)*2 + 1 ];
		V_binarytohex( failed.m_md5ValueRetry.bits, sizeof(MD5Value_t), hex3, sizeof( hex3 ) );

		pKV1->SetString( "ChunkMd5Master", hex2 );
		pKV1->SetString( "ChunkMd5First", hex );
		pKV1->SetString( "ChunkMd5Second", hex3 );
	}
}


int CPackedStore::ReadData( CPackedStoreFileHandle &handle, void *pOutData, int nNumBytes )
{
	int nRet = 0;

	// clamp read size to file size
	nNumBytes = std::min( nNumBytes, handle.m_nFileSize - handle.m_nCurrentFileOffset );
	if ( nNumBytes > 0 )
	{
		// first satisfy from the metadata, if we can
		int nNumMetaDataBytes = std::min( nNumBytes, handle.m_nMetaDataSize - handle.m_nCurrentFileOffset );
		if ( nNumMetaDataBytes > 0 )
		{
			std::memcpy( pOutData, reinterpret_cast<uint8 const *>( handle.m_pMetaData )
					+ handle.m_nCurrentFileOffset, nNumMetaDataBytes );
			nRet += nNumMetaDataBytes;
			pOutData = reinterpret_cast<uint8 *>( pOutData ) + nNumMetaDataBytes;
			handle.m_nCurrentFileOffset += nNumMetaDataBytes;
			nNumBytes -= nNumMetaDataBytes;
		}
		// satisfy remaining bytes from file
		if ( nNumBytes > 0 )
		{
			FileHandleTracker_t &fHandle = GetFileHandle( handle.m_nFileNumber );
			int nDesiredPos = handle.m_nFileOffset + handle.m_nCurrentFileOffset - handle.m_nMetaDataSize;
			int nRead;
			fHandle.m_Mutex.lock();
			if ( handle.m_nFileNumber == VPKFILENUMBER_EMBEDDED_IN_DIR_FILE )
			{
				// for file data in the directory header, all offsets are relative to the size of the dir header.
				nDesiredPos += m_nDirectoryDataSize + sizeof( VPKDirHeader_t );
			}

			if ( m_PackedStoreReadCache.BCanSatisfyFromReadCache( (uint8 *)pOutData, handle, fHandle, nDesiredPos, nNumBytes, nRead ) )
			{
				handle.m_nCurrentFileOffset += nRead;
			}
			else
			{
				m_pFileSystem->Seek( fHandle.m_hFileHandle, nDesiredPos, FILESYSTEM_SEEK_HEAD );
				nRead = m_pFileSystem->Read( pOutData, nNumBytes, fHandle.m_hFileHandle );
				handle.m_nCurrentFileOffset += nRead;
				fHandle.m_nCurOfs = nRead + nDesiredPos;
			}
			contract_assert( nRead == nNumBytes );
			nRet += nRead;
			fHandle.m_Mutex.unlock();
		}
	}
	m_PackedStoreReadCache.RetryAllBadCacheLines();
	return nRet;
}

bool CPackedStore::HashEntirePackFile( CPackedStoreFileHandle &handle, int64 &nFileSize, int nFileFraction, int nFractionSize, FileHash_t &fileHash )
{
constexpr auto CRC_CHUNK_SIZE = (32*1024);
	unsigned char tempBuf[CRC_CHUNK_SIZE];

	FileHandleTracker_t &fHandle = GetFileHandle( handle.m_nFileNumber );
	fHandle.m_Mutex.lock();
	
	unsigned int fileLength = m_pFileSystem->Size( fHandle.m_hFileHandle );
	nFileSize = fileLength;
	MD5Context_t ctx;
	std::memset(&ctx, 0, sizeof(MD5Context_t));
	MD5Init(&ctx);

	int nDesiredPos = nFileFraction;
	m_pFileSystem->Seek( fHandle.m_hFileHandle, nDesiredPos, FILESYSTEM_SEEK_HEAD );

	int nFractionLength = ( fileLength - nFileFraction );
	if ( nFractionLength > nFractionSize )
		nFractionLength = nFractionSize;
	int nChunks = nFractionLength / CRC_CHUNK_SIZE + 1;
	unsigned int curStartByte = 0;
	for ( int iChunk=0; iChunk < nChunks; iChunk++ )
	{
		int curEndByte = std::min( curStartByte + CRC_CHUNK_SIZE, (uint)nFractionLength );
		int chunkLen = curEndByte - curStartByte;
		if ( chunkLen == 0 )
			break;

		int nRead;
		nRead = m_pFileSystem->Read( tempBuf, chunkLen, fHandle.m_hFileHandle );
		MD5Update(&ctx, tempBuf, nRead);

		curStartByte += CRC_CHUNK_SIZE;
	}
	MD5Final( fileHash.m_md5contents.bits, &ctx);
	fileHash.m_crcIOSequence = nFractionLength;
	fileHash.m_cbFileLen = nFractionLength;
	fileHash.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
	fileHash.m_nPackFileNumber = handle.m_nFileNumber;
	fileHash.m_PackFileID = handle.m_pOwner->m_PackFileID;

	// seek back to where it was
	m_pFileSystem->Seek( fHandle.m_hFileHandle, fHandle.m_nCurOfs, FILESYSTEM_SEEK_HEAD );
	fHandle.m_Mutex.unlock();

	return true;
}

void CPackedStore::DiscardChunkHashes( int iChunkFileIndex )
{
	// Wow, this could be a LOT faster because the list is
	// sorted.  Probably not worth optimizing
	std::erase_if( m_vecChunkHashFraction, [iChunkFileIndex]( const ChunkHashFraction_t &hash ) { return hash.m_nPackFileNumber == iChunkFileIndex; } );
}

void CPackedStore::HashChunkFile( int iChunkFileIndex )
{
	std::scoped_lock lock( m_Mutex );
	static const int k_nFileFractionSize = 0x00100000; // 1 MB
	static const int k_nFileFractionMask = 0xFFF00000; // 1 MB

	// Purge any hashes we already have for this chunk.
	DiscardChunkHashes( iChunkFileIndex );

	CPackedStoreFileHandle VPKHandle = GetHandleForHashingFiles();
	VPKHandle.m_nFileNumber = iChunkFileIndex;

	int nFileFraction = 0;
	while ( 1 )
	{
		FileHash_t filehash;

		// VPKHandle.m_nFileNumber;
		// nFileFraction;
		int64 fileSize = 0;
		// if we have never hashed this before - do it now
		HashEntirePackFile( VPKHandle, fileSize, nFileFraction, k_nFileFractionSize, filehash );
		ChunkHashFraction_t fileHashFraction;
		fileHashFraction.m_cbChunkLen = filehash.m_cbFileLen;
		fileHashFraction.m_nPackFileNumber = VPKHandle.m_nFileNumber;
		fileHashFraction.m_nFileFraction = nFileFraction;
		V_memcpy( fileHashFraction.m_md5contents.bits, filehash.m_md5contents.bits, sizeof(fileHashFraction.m_md5contents) );
		m_vecChunkHashFraction.insert( std::ranges::upper_bound( m_vecChunkHashFraction, fileHashFraction, ChunkHashFractionLess_t() ), fileHashFraction );
		// move to next section
		nFileFraction += k_nFileFractionSize;
		// if we are at EOF we are done
		if ( nFileFraction > fileSize )
			break;
	}
}


void CPackedStore::HashAllChunkFiles()
{
	// Rebuild the directory hash tables.  The main reason to do this is
	// so that the highest chunk number is correct, in case chunks have
	// been removed.
	BuildHashTables();

	// make brand new hashes
	m_vecChunkHashFraction.clear(); m_vecChunkHashFraction.shrink_to_fit();
	for ( int iChunkFileIndex = 0 ; iChunkFileIndex <= GetHighestChunkFileIndex() ; ++iChunkFileIndex )
		HashChunkFile( iChunkFileIndex );
}

void CPackedStore::ComputeDirectoryHash( MD5Value_t &md5Directory )
{
	MD5Context_t ctx;
	std::memset(&ctx, 0, sizeof(MD5Context_t));
	MD5Init(&ctx);
	MD5Update(&ctx, m_DirectoryData.data(), std::ssize( m_DirectoryData ) );
	MD5Final( md5Directory.bits, &ctx);
}


void CPackedStore::ComputeChunkHash( MD5Value_t &md5ChunkHashes )
{
	MD5Context_t ctx;
	std::memset(&ctx, 0, sizeof(MD5Context_t));
	MD5Init(&ctx);
	MD5Update(&ctx, (uint8 *)m_vecChunkHashFraction.data(), std::ssize( m_vecChunkHashFraction )*sizeof(m_vecChunkHashFraction[0]) );
	MD5Final( md5ChunkHashes.bits, &ctx);
}


bool CPackedStore::BTestDirectoryHash()
{
	if ( !BFileContainedHashes() )
		return true;
	MD5Value_t md5Directory;
	ComputeDirectoryHash( md5Directory );
	return V_memcmp( m_DirectoryMD5.bits, md5Directory.bits, sizeof( md5Directory.bits ) ) == 0;
}


bool CPackedStore::BTestMasterChunkHash()
{
	if ( !BFileContainedHashes() )
		return true;
	MD5Value_t md5ChunkHashes;
	ComputeChunkHash( md5ChunkHashes );
	return V_memcmp( m_ChunkHashesMD5.bits, md5ChunkHashes.bits, sizeof( md5ChunkHashes.bits ) ) == 0;
}


void CPackedStore::HashEverything()
{
	HashAllChunkFiles();
	HashMetadata();
}

void CPackedStore::HashMetadata()
{
	ComputeDirectoryHash( m_DirectoryMD5 );
	ComputeChunkHash( m_ChunkHashesMD5 );
}


bool CPackedStore::FindFileHashFraction( int nPackFileNumber, int nFileFraction, ChunkHashFraction_t &fileHashFraction )
{
	ChunkHashFraction_t fileHashFractionFind;
	fileHashFractionFind.m_nFileFraction = nFileFraction;
	fileHashFractionFind.m_nPackFileNumber = nPackFileNumber;

	auto it = std::ranges::lower_bound( m_vecChunkHashFraction, fileHashFractionFind, ChunkHashFractionLess_t() );
	if ( it == m_vecChunkHashFraction.end() || ChunkHashFractionLess_t()( fileHashFractionFind, *it ) )
	{
		contract_assert( false );
		return false;
	}
	fileHashFraction = *it;
	return true;
}

void CPackedStore::GetPackFileName( CPackedStoreFileHandle &handle, char *pchFileNameOut, int cchFileNameOut ) const
{
	GetDataFileName( pchFileNameOut, cchFileNameOut, handle.m_nFileNumber );
}

FileHandleTracker_t & CPackedStore::GetFileHandle( int nFileNumber )
{
	std::scoped_lock lock( m_Mutex );
	int nFileHandleIdx = nFileNumber % std::size( m_FileHandles );

	if ( m_FileHandles[nFileHandleIdx].m_nFileNumber == nFileNumber )
	{
		return m_FileHandles[nFileHandleIdx];
	}
	else if ( m_FileHandles[nFileHandleIdx].m_nFileNumber == -1 )
	{
		// no luck finding the handle - need a new one
		char pszDataFileName[MAX_PATH];
		GetDataFileName( pszDataFileName, sizeof(pszDataFileName), nFileNumber );
		m_FileHandles[nFileHandleIdx].m_nCurOfs = 0;
		m_FileHandles[nFileHandleIdx].m_hFileHandle = m_pFileSystem->Open( pszDataFileName, "rb" );
		if ( m_FileHandles[nFileHandleIdx].m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
		{
			m_FileHandles[nFileHandleIdx].m_nFileNumber = nFileNumber;
		}
		return m_FileHandles[nFileHandleIdx];
	}
	Error( "Exceeded limit of number of vpk files supported (%d)!\n", MAX_ARCHIVE_FILES_TO_KEEP_OPEN_AT_ONCE );
	static FileHandleTracker_t invalid;
	invalid.m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	return invalid;
}

bool CPackedStore::RemoveFileFromDirectory( const char *pszName )
{
	// Remove it without building hash tables
	if ( !InternalRemoveFileFromDirectory( pszName ) )
		return false;

	// We removed it, we need to rebuild hash tables
	BuildHashTables();
	return true;
}

bool CPackedStore::InternalRemoveFileFromDirectory( const char *pszName )
{
	CPackedStoreFileHandle pData = OpenFile( pszName );
	if ( !pData )
		return false;

	CFileHeaderFixedData *pHeader = pData.m_pHeaderData;
	// delete the old header so we can insert a new one with updated contents
	int nBytesToRemove = ( int )( V_strlen( ( char * ) pData.m_pDirFileNamePtr ) + 1 + pHeader->HeaderSizeIncludingMetaData() );
	{ auto first = m_DirectoryData.begin() + ( pData.m_pDirFileNamePtr - m_DirectoryData.data() ); m_DirectoryData.erase( first, first + nBytesToRemove ); }
	return true;
}

void CPackedStore::AddFileToDirectory( const VPKContentFileInfo_t &info )
{
	ks::ScratchArena<> scratch;
	// this method is fairly complicated because it has to do inserts into the packed directory
	// data Our strategy is to build out the whole ext _ dir _ file record. if none of this is
	// already present, we will just insert it in the head of the file. If the extension is
	// present, we'll insert the dir+file part. If the extension + dir is present, we just insert
	// the file part at the right place. If everything is present, we just need to return the
	// current record

	// First, remove it if it's already there,
	// without rebuilding the hash tables
	InternalRemoveFileFromDirectory( info.m_sName.c_str() );

	// let's build out a header
	char pszExt[MAX_PATH];
	char pszBase[MAX_PATH];
	char pszDir[MAX_PATH];
	SplitFileComponents( info.m_sName.c_str(), pszDir, pszBase, pszExt );
	int nNumDataParts = 1;
	int nFileDataSize = s_FileHeaderSize( pszBase, nNumDataParts, info.m_iPreloadSize );
	int nTotalHeaderSize = ( int )( nFileDataSize + ( 2 + std::strlen( pszExt ) ) + ( 2 + std::strlen( pszDir ) ) );
	char *pBuf = ( char * ) scratch.AllocBytes( nTotalHeaderSize );
	char *pOut = pBuf;
	std::strcpy( pOut, pszExt );
	pOut += std::strlen( pszExt );
	*( pOut++ ) = 0;										// null on ext name
	std::strcpy( pOut, pszDir );
	pOut += std::strlen( pszDir );
	*( pOut++ ) = 0;									// null at end of dir name
	std::strcpy( pOut, pszBase );
	pOut += std::strlen( pszBase );
	*( pOut++ ) = 0;
	uint32 nCRC = info.m_crc;
	std::memcpy( pOut, &nCRC, sizeof( nCRC ) );
	pOut += sizeof( int );
	if ( info.m_iPreloadSize > 0xffff )
		Error( "Preload size for '%s' is too big", info.m_sName.c_str() );
	uint16 nMetaDataSize = (uint16)info.m_iPreloadSize;
	std::memcpy( pOut, &nMetaDataSize, sizeof( uint16 ) );
	pOut += sizeof( uint16 );

	// now, build file parts.
	CFilePartDescr newPart;
	newPart.m_nFileDataSize = info.GetSizeInChunkFile();
	newPart.m_nFileNumber = ( info.m_idxChunk < 0 ) ? VPKFILENUMBER_EMBEDDED_IN_DIR_FILE : info.m_idxChunk;
	newPart.m_nFileDataOffset = info.m_iOffsetInChunk;

	std::memcpy( pOut, &newPart, sizeof( newPart ) );
	pOut += sizeof( newPart );

	PackFileIndex_t endOfPartMarker = PACKFILEINDEX_END;
	std::memcpy( pOut, &endOfPartMarker, sizeof( endOfPartMarker ) );
	pOut += sizeof( PackFileIndex_t );
	if ( nMetaDataSize )
	{
		contract_assert( info.m_pPreloadData );
		std::memcpy( pOut, info.m_pPreloadData, nMetaDataSize );
		pOut += nMetaDataSize;
	}
	*( pOut++ ) = 0;									// mark no more files in dir
	*( pOut++ ) = 0;									// mark no more dirs in extension
	contract_assert( pOut - pBuf == nTotalHeaderSize );

	// now, we need to insert our header, figuring out how many of the fields are already there
	int nInsertOffset = 0;
	auto itExt = m_DirsByExtension.find( pszExt );
	char *pHeaderInsertPtr = pBuf;

	if ( itExt != m_DirsByExtension.end() )
	{
		// this is not a new extension. we should not insert the extension record
		nTotalHeaderSize -= 2 + std::strlen( pszExt );		// null + end of dir list marker
		pHeaderInsertPtr += 1 + std::strlen( pszExt );					// don't insert the name + null
		// now, look for the directory
		auto itDir = itExt->second.find( pszDir );
		if ( itDir != itExt->second.end() )
		{
			// dir and extension found. all we need to do is insert the file data itself
			nTotalHeaderSize -= 2 + std::strlen( pszDir );	// null + end of file list marker
			pHeaderInsertPtr += 1 + std::strlen( pszDir );
			char const *pStartOfDirFileData = itDir->data() + 1 + itDir->size();
			nInsertOffset = pStartOfDirFileData - ( char const * ) ( m_DirectoryData.data() );
		}
		else
		{
			char const *pStartOfExtFileData = itExt->first.data() + 1 + itExt->first.size();
			nInsertOffset = pStartOfExtFileData - ( char const * ) ( m_DirectoryData.data() );
		}
	}
	m_DirectoryData.insert( m_DirectoryData.begin() + nInsertOffset, nTotalHeaderSize, {} );
	std::memcpy( &m_DirectoryData[nInsertOffset], pHeaderInsertPtr, nTotalHeaderSize );
	BuildHashTables();
}

ePackedStoreAddResultCode CPackedStore::AddFile( char const *pFile, uint16 nMetaDataSize, const void *pFileData, uint32 nFileTotalSize, bool bMultiChunk, uint32 const *pCrcValue )
{

	// Calculate CRC if they didn't provide one
	uint32 nCRC;
	if ( pCrcValue )
	{
		nCRC = *pCrcValue;
	}
	else
	{
		nCRC = CRC32_ProcessSingleBuffer( pFileData, nFileTotalSize );
	}

	// Check if it is already here with the same contents
	CPackedStoreFileHandle pData = OpenFile( pFile );
	ePackedStoreAddResultCode nRslt = EPADD_NEWFILE;
	if ( pData )											// already in pack
	{
		CFileHeaderFixedData *pHeader = pData.m_pHeaderData;
		if ( ( nFileTotalSize == pHeader->TotalDataSize() ) && ( pHeader->m_nFileCRC == nCRC ) && ( nMetaDataSize == pHeader->m_nMetaDataSize  ) ) // file unchanged?
		{
			return EPADD_ADDSAMEFILE;
		}
		nRslt = EPADD_UPDATEFILE;
	}

	// Build up the directory info into an interface structure
	VPKContentFileInfo_t dirEntry;
	dirEntry.m_sName = ks::nz( pFile );
	dirEntry.m_iTotalSize = nFileTotalSize;
	dirEntry.m_iPreloadSize = Min( (uint32)nMetaDataSize, (uint32)nFileTotalSize ) ;
	dirEntry.m_pPreloadData = ( dirEntry.m_iPreloadSize > 0 ) ? pFileData : nullptr;
	dirEntry.m_crc = nCRC;
	uint32 nBytesInChunk = dirEntry.GetSizeInChunkFile();
	const unsigned char *pDataStart = (const unsigned char *)pFileData + dirEntry.m_iPreloadSize;

	if ( bMultiChunk && nBytesInChunk > 0 )
	{

		// Check if we need to start a new chunk
		char szDataFileName[MAX_PATH];
		if ( m_nHighestChunkFileIndex < 0 )
		{
			dirEntry.m_idxChunk = 0;
			dirEntry.m_iOffsetInChunk = 0;
		}
		else
		{
			dirEntry.m_idxChunk = m_nHighestChunkFileIndex;

			// Append to most recent chunk
			GetDataFileName( szDataFileName, sizeof(szDataFileName), m_nHighestChunkFileIndex );
			dirEntry.m_iOffsetInChunk = g_pFullFileSystem->Size( szDataFileName );
			if ( (int)dirEntry.m_iOffsetInChunk <= 0 ) // technical wrong, but we shouldn't have 2GB chunks.  (Sort of defeats the whole purpose.)
			{
				// Note, there is one possible failure case.  if we have a file whose data
				// is actually all in the preload section, but it is marked as being
				// in a chunk, then we might have a zero byte "chunk."  We really should
				// not be assigning any files to "chunks" if they are entirely in the preload
				// area.
				Error( "Error querying %s for file size\n", szDataFileName );
			}

			// Check if we need to start a new chunk
			if ( (int)dirEntry.m_iOffsetInChunk >= m_nWriteChunkSize )
			{
				++dirEntry.m_idxChunk;
				dirEntry.m_iOffsetInChunk = 0;
			}
		}

		m_nHighestChunkFileIndex = std::max( m_nHighestChunkFileIndex, dirEntry.m_idxChunk );

		// write the actual data
		GetDataFileName( szDataFileName, sizeof(szDataFileName), dirEntry.m_idxChunk );
		FileHandle_t fHandle = m_pFileSystem->Open( szDataFileName, "rb+" );
		if ( !fHandle && dirEntry.m_iOffsetInChunk == 0 )
			fHandle = m_pFileSystem->Open( szDataFileName, "wb" );
		if ( !fHandle )
			Error( "Cannot open %s for writing", szDataFileName );

		m_pFileSystem->Seek( fHandle, dirEntry.m_iOffsetInChunk, FILESYSTEM_SEEK_HEAD );
		m_pFileSystem->Write( pDataStart, nBytesInChunk, fHandle );
		m_pFileSystem->Close( fHandle );

		// Force on the use of the "dir" file
		m_bUseDirFile = true;
	}
	else
	{
		// append to the dir data.
		dirEntry.m_idxChunk = VPKFILENUMBER_EMBEDDED_IN_DIR_FILE;
		dirEntry.m_iOffsetInChunk = std::ssize( m_EmbeddedChunkData );
		m_EmbeddedChunkData.insert( m_EmbeddedChunkData.end(), pDataStart, pDataStart + nBytesInChunk );
	}

	// Update the directory
	AddFileToDirectory( dirEntry );

	return nRslt;
}

int CPackedStore::GetFileList( std::vector< std::string > &outFilenames, bool bFormattedOutput, bool bSortedOutput )
{
	return GetFileList( nullptr, outFilenames, bFormattedOutput, bSortedOutput );
}

int CPackedStore::GetFileList( const char *pWildCard, std::vector< std::string > &outFilenames, bool bFormattedOutput, bool bSortedOutput )
{
	// Separate the wildcard base from the extension
	char szWildCardPath[MAX_PATH];
	char szWildCardBase[64];
	char szWildCardExt[20];
	bool bNoBaseWildcard = false;
	bool bNoExtWildcard = false;

	szWildCardPath[0] = szWildCardExt[0] = szWildCardBase[0] = 0;

	// Parse the wildcard string into a base and extension used for string comparisons
	if ( pWildCard )
	{
		V_ExtractFilePath( pWildCard, szWildCardPath, sizeof( szWildCardPath ) );
		V_FixSlashes( szWildCardPath, '/' );

		V_FileBase( pWildCard, szWildCardBase, sizeof( szWildCardBase ) );
		V_ExtractFileExtension( pWildCard, szWildCardExt, sizeof( szWildCardExt ) );

		// Remove '*' from the base and extension strings so that the string comparison calls will match
		char *pcStar = std::strchr( szWildCardBase, '*' );
		pcStar ? *pcStar = 0 : bNoBaseWildcard = true;
		pcStar = std::strchr( szWildCardExt, '*' );
		pcStar ? *pcStar = 0 : bNoExtWildcard = true;
	}

	char const *pData = reinterpret_cast< char const *>( DirectoryData() );
	while( *pData )
	{
		// for each extension
		char pszCurExtension[MAX_PATH];
		if ( pData[0] != ' ' )
			std::sprintf( pszCurExtension, ".%s", pData );
		else
			pszCurExtension[0] = 0;
		// now, iterate over all directories associated with this extension
		pData += 1 + std::strlen( pData );
		while( *pData )
		{
			char pszCurDir[MAX_PATH];
			if ( pData[0] != ' ' )
				std::sprintf( pszCurDir, "%s/", pData );
			else
				pszCurDir[0] = 0;
			pData += 1 + std::strlen( pData );					// skip dir name
			// now, march through all the files
			while( *pData )									// until we're out of files to look at
			{
				char pszFNameOut[MAX_PATH*2];
				if ( bFormattedOutput )
				{
					CFileHeaderFixedData const *pHeader = reinterpret_cast< CFileHeaderFixedData const *>( pData + 1 + std::strlen( pData ) );
					std::sprintf( pszFNameOut, "%s%s%s crc=0x%x metadatasz=%d", pszCurDir, pData, pszCurExtension, pHeader->m_nFileCRC, pHeader->m_nMetaDataSize );
					CFilePartDescr const *pPart = &( pHeader->m_PartDescriptors[0] );
					while( pPart->m_nFileNumber != PACKFILEINDEX_END )
					{
						std::sprintf( pszFNameOut + std::strlen( pszFNameOut )," fnumber=%d ofs=0x%x sz=%d",
								 pPart->m_nFileNumber, pPart->m_nFileDataOffset, pPart->m_nFileDataSize );
						pPart++;
					}
				}
				else
				{
 					V_strncpy( pszFNameOut, pszCurDir, sizeof( pszFNameOut ) );
 					V_strncat( pszFNameOut, pData, sizeof( pszFNameOut ) );
 					V_strncat( pszFNameOut, pszCurExtension, sizeof( pszFNameOut ) );
				}
				SkipFile( pData );

				bool matches = true;

				if ( pWildCard )
				{
					// See if the filename matches the wildcards
					char szFNameOutPath[MAX_PATH];
					char szFNameOutBase[64];
					char szFNameOutExt[20];

					V_ExtractFilePath( pszFNameOut, szFNameOutPath, sizeof( szFNameOutPath ) );
					V_FileBase( pszFNameOut, szFNameOutBase, sizeof( szFNameOutBase ) );
					V_ExtractFileExtension( pszFNameOut, szFNameOutExt, sizeof( szFNameOutExt ) );

					matches =  !V_strnicmp( szFNameOutPath, szWildCardPath, sizeof( szWildCardPath ) );
					matches = matches && ( !V_strlen( szWildCardExt ) || bNoExtWildcard ? 0 == V_strnicmp( szFNameOutExt, szWildCardExt, std::strlen( szWildCardExt ) ) : 0 != V_stristr(szFNameOutExt, szWildCardExt ) );
					matches = matches && ( !V_strlen( szWildCardBase ) || bNoBaseWildcard ? 0 == V_strnicmp( szFNameOutBase, szWildCardBase, std::strlen( szWildCardBase ) ) : 0 != V_stristr(szFNameOutBase, szWildCardBase ) );
				}

				// Add the file to the output list
				if ( matches )
				{
					char *pFName = new char[1 + std::strlen( pszFNameOut ) ];
					std::strcpy( pFName, pszFNameOut );
					outFilenames.push_back( pFName );
				}
			}
			pData++;												// skip end marker
		}
		pData++;												// skip end marker
	}

	if ( bSortedOutput )
	{
		std::ranges::sort( outFilenames );
	}
	
	return std::ssize( outFilenames );
}

void CPackedStore::GetFileList( const char *pWildcard, std::vector< VPKContentFileInfo_t > &outVecResults )
{

	// !KLUDGE! Get the filenames first, and then "find" them again.
	std::vector< std::string > vecFilenames;
	GetFileList( vecFilenames, false, false );

	for ( const std::string &sFilename : vecFilenames )
	{
		// Locate where it is in the existing file
		CPackedStoreFileHandle h = OpenFile( sFilename.c_str() );
		if ( !h )
			Error( "File '%s' was returned by GetFileList, but OpenFile() fails?!", sFilename.c_str() );

		// Convert to output structure
		VPKContentFileInfo_t &f = outVecResults.emplace_back();
		f.m_sName = sFilename.c_str();
		f.m_idxChunk = ( h.m_nFileNumber == VPKFILENUMBER_EMBEDDED_IN_DIR_FILE ) ? -1 : h.m_nFileNumber;
		f.m_iTotalSize = h.m_nFileSize;
		f.m_iOffsetInChunk = h.m_nFileOffset;
		f.m_iPreloadSize = h.m_nMetaDataSize;
		f.m_crc = h.m_pHeaderData->m_nFileCRC;
		f.m_pPreloadData = h.m_pHeaderData->MetaData();
	}
}

int CPackedStore::GetFileAndDirLists( std::vector< std::string > &outDirnames, std::vector< std::string > &outFilenames, bool bSortedOutput )
{
	return GetFileAndDirLists( nullptr, outDirnames, outFilenames, bSortedOutput );
}

void CPackedStore::BuildFindFirstCache()
{
	std::vector< std::string > allVPKFiles;
	char szLastDirFound[MAX_PATH];

	// Init
	V_strncpy( szLastDirFound, "$$$$$$$HighlyUnlikelyPathForInitializationPurposes#######", sizeof( szLastDirFound ) );

	// Get all files in the VPK
	GetFileList( allVPKFiles, false, true );

	// Add directories to directory list and files into map
	for ( const std::string &sVPKFile : allVPKFiles )
	{
		char szFilePath[MAX_PATH];

		V_ExtractFilePath( sVPKFile.c_str(), szFilePath, sizeof( szFilePath ) );
		V_StripTrailingSlash( szFilePath );
		
		// New directory
		if ( V_strnicmp( szFilePath, szLastDirFound, sizeof( szLastDirFound ) ) )
		{
			// Mark the new one as the last one encountered
			V_strncpy( szLastDirFound, szFilePath, sizeof( szFilePath ) );

			// Add it
			m_directoryList.emplace_back( szFilePath );
			m_dirContents.emplace_back();
		}

		m_dirContents.back().emplace_back( V_UnqualifiedFileName( sVPKFile.c_str() ) );
	}
}

int CPackedStore::GetFileAndDirLists( const char *pWildCard, std::vector< std::string > &outDirnames, std::vector< std::string > &outFilenames, bool bSortedOutput )
{


	// If this is the first time we've called FindFirst on this CPackedStore then let's build the caches
	if ( m_directoryList.empty() )
	{
		BuildFindFirstCache();
	}

	// printf("CPackedStore::GetFileAndDirLists - Searching for %s\n", pWildCard? pWildCard: "NULL");

	if ( pWildCard )
	{
		std::set< std::string, ks::iless > AddedDirectories; // Used to remove duplicate paths

		char szWildCardPath[MAX_PATH];
		char szWildCardBase[64];
		char szWildCardExt[20];
		
		int nLenWildcardPath = 0;
		int nLenWildcardBase = 0;
		int nLenWildcardExt = 0;

		bool bBaseWildcard = true;
		bool bExtWildcard = true;

		szWildCardPath[0] = szWildCardExt[0] = szWildCardBase[0] = '\0';
		//
		// Parse the wildcard string into a base and extension used for string comparisons
		//
		V_ExtractFilePath( pWildCard, szWildCardPath, sizeof( szWildCardPath ) );
		V_FixSlashes( szWildCardPath, '/' );

		V_FileBase( pWildCard, szWildCardBase, sizeof( szWildCardBase ) );
		V_ExtractFileExtension( pWildCard, szWildCardExt, sizeof( szWildCardExt ) );

		// From the pattern, we now have the directory path up to the file pattern, the filename base, and the filename 
		// extension.

		// Remove '*' from the base and extension strings so that the string comparison calls will match
		char *pcStar = std::strchr( szWildCardBase, '*' );
		pcStar ? *pcStar = 0 : bBaseWildcard = false;

		pcStar = std::strchr( szWildCardExt, '*' );
		pcStar ? *pcStar = 0 : bExtWildcard = false;

		nLenWildcardPath = V_strlen( szWildCardPath );
		nLenWildcardBase = V_strlen( szWildCardBase );
		nLenWildcardExt = V_strlen( szWildCardExt );

		// Generate the list of directories and files that match the wildcard
		//

		//
		// Directories first
		//
		for ( const std::string &sDirectory : m_directoryList )
		{
			// Does this file's path match the wildcard path?
			if  (  (  nLenWildcardPath && ( 0 == V_strnicmp( sDirectory.c_str(), szWildCardPath, nLenWildcardPath ) ) ) 
				|| ( !nLenWildcardPath && sDirectory.empty() ) )
			{
				// Extract the sub-directory name if there is one
				char szSubDir[64];
				char *szSubDirExtension = nullptr; // this is anything after a '.' in szSubDir
				bool bBaseMatch = false;
				bool bExtMatch = false;

				// Copy everything to the right of the root directory
				V_strncpy( szSubDir, &sDirectory[nLenWildcardPath], sizeof( szSubDir ) );

				// Set the next / to NULL and we have our subdirectory
				char *pSlash = std::strchr( szSubDir, '/' );
				pSlash ? *pSlash = 0 : 0;

				szSubDirExtension = std::strchr( szSubDir, '.' );
				if ( szSubDirExtension )
				{
					// Null out the . and move the szSubDirExtension to point to the extension
					*szSubDirExtension = '\0';
					szSubDirExtension++;
				}

				// If we have a base dir name, and we have a szWildCardBase to match against
				if ( bBaseWildcard )
					bBaseMatch = true;  // The base is the wildCard ("*"), so whatever we have as the base matches
				else 
					bBaseMatch = ( 0 == V_strnicmp( szSubDir, szWildCardBase, nLenWildcardBase ) );

				// If we have an extension and we have a szWildCardExtension to mach against
				if ( bExtWildcard )
					bExtMatch = true; // The extension is the wildcard ("*"), so whatever we have as the extension matches
				else 
					bExtMatch = ( nullptr == szSubDirExtension && '\0' == *szWildCardExt ) || (( nullptr != szSubDirExtension ) && ( 0 == V_strnicmp( szSubDirExtension, szWildCardExt, nLenWildcardExt ) ));

				// If both parts match, then add it to the list of directories that match
				if ( bBaseMatch && bExtMatch )
				{
					char szFullPathToDir[ MAX_PATH ];

					V_strncpy( szFullPathToDir, szWildCardPath, nLenWildcardPath );
					V_strcat_safe( szFullPathToDir, "/" );
					V_strcat_safe( szFullPathToDir, szSubDir );
		
					// Add the subdirectory to the list if it isn't already there
					if ( AddedDirectories.insert( szFullPathToDir ).second )
					{
						outDirnames.push_back( szFullPathToDir );
					}
				}
			}
		}

		//
		// Files
		//
		for ( int i = 0; i < std::ssize( m_directoryList ); i++ )
		{
			// We no longer want the trailing slash
			V_StripTrailingSlash( szWildCardPath );

			// Find the directory that matches the wildcard path
			if ( !V_strnicmp( szWildCardPath, m_directoryList[i].c_str(), sizeof( szWildCardPath ) ) )
			{
				std::vector< std::string > &filesInDirectory = m_dirContents[i];

				// Use the cached list of files in this directory
				for ( const std::string &sFile : filesInDirectory )
				{
					bool matches = true;


					// See if the filename matches the wildcards
					char szFNameOutBase[64];
					char szFNameOutExt[20];

					V_FileBase( sFile.c_str(), szFNameOutBase, sizeof( szFNameOutBase ) );
					V_ExtractFileExtension( sFile.c_str(), szFNameOutExt, sizeof( szFNameOutExt ) );

					// Since we have a sorted list we can optimize using the return code of the compare
					int c = V_strnicmp( szWildCardBase, szFNameOutBase, nLenWildcardBase );
					if ( c < 0 )
						break;
					if ( c > 0 )
						continue;
					matches = ( (nLenWildcardExt <= 0) || bBaseWildcard ? 0 == V_strnicmp( szFNameOutExt, szWildCardExt, nLenWildcardExt ) : V_stristr( szFNameOutExt, szWildCardExt ) != nullptr );

					// Add the file to the output list
					if ( matches )
					{	
						bool bFound = false;

						for ( const std::string &sOutFilename : outFilenames )
						{
							if ( !V_strncmp( sOutFilename.c_str(), sFile.c_str(), (int)sFile.size() ) )
							{
								bFound = true;
								break;
							}
						}

						if ( !bFound )
						{
							outFilenames.emplace_back( sFile );
						}
					}
				}
			}
		}
	}
	else // Otherwise, simply return the base data
	{
		// Add all the files as well
		for ( int i = 0; i < std::ssize( m_directoryList ); i++ )
		{
			// Add all directories
			outDirnames.emplace_back( m_directoryList[i] );

			// Now add all files
			std::vector< std::string > &filesInDirectory = m_dirContents[i];
			for ( const std::string &sFile : filesInDirectory )
			{
				outFilenames.emplace_back( sFile );
			}
		}
	}

	// Sort the output if requested
	if ( bSortedOutput )
	{
		std::ranges::sort( outDirnames );
		std::ranges::sort( outFilenames );
	}

	return std::ssize( outDirnames );
}
