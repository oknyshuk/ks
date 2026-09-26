//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "sceneimage.h"
#include "choreoscene.h"
#include "iscenetokenprocessor.h"
#include "scenefilecache/SceneImageFile.h"

#include "lzma/lzma.h"

#include "tier1/byte_reader.h"
#include "tier1/byte_writer.h"
#include "tier1/intern_pool.h"
#include "tier1/map_ops.h"
#include "tier1/utlvector.h"

#include "scriplib.h"
#include "cmdlib.h"

#include "tier0/memalloc.h"
class CSceneImage : public ISceneImage
{
public:
	virtual bool CreateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status );
	// This will update the scenes.image file, or create a new one if it doesn't exist
	virtual bool UpdateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status, std::string *pFilesToUpdate, int nUpdateCount );

private:

	bool WriteSceneImageFile( ks::byte_writer &targetBuffer, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus );
};

static CSceneImage g_SceneImage;
ISceneImage *g_pSceneImage = &g_SceneImage;

struct SceneFile_t
{
	SceneFile_t()
	{
		msecs = 0;
		lastspeak_msecs = 0;
		crcFileName = (CRC32_t)0u;
	}

	// This gets used when we load back in from disk
	CRC32_t		crcFileName;
	// Otherwise, it's this
	std::string	fileName;
	std::vector< std::byte >	compiledBuffer;

	unsigned int		msecs;
	unsigned int		lastspeak_msecs;
	std::vector< short >	soundList;
};
std::vector< SceneFile_t > g_SceneFiles;

//-----------------------------------------------------------------------------
// Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char *CurrentToken( void )
	{
		return token;
	}

	bool GetToken( bool crossline )
	{
		return ::GetToken( crossline ) ? true : false;
	}

	bool TokenAvailable( void )
	{
		return ::TokenAvailable() ? true : false;
	}

	void SetFilename( const char *pFilename )
	{
		m_Filename = ks::nz( pFilename );
	}

	void Error( const char *fmt, ... )
	{
		char string[2048];
		va_list argptr;
		va_start( argptr, fmt );
		Q_vsnprintf( string, sizeof(string), fmt, argptr );
		va_end( argptr );

		Warning( "%s: %s", m_Filename.data(), string );
		Assert( 0 );
	}

private:
	std::string	m_Filename;
};
static CSceneTokenProcessor g_SceneTokenProcessor;
ISceneTokenProcessor *tokenprocessor = &g_SceneTokenProcessor;

// a simple case insensitive string pool
// the final pool contains all the unique strings seperated by a null
class CChoreoStringPool : public IChoreoStringPool
{
public:
	CChoreoStringPool()
	{
		m_nOffset = 0;
	}

	// Returns a valid id into the string table
	virtual short FindOrAddString( const char *pString )
	{
		auto [it, bAdded] = m_StringIds.try_emplace( pString, (int)m_Strings.size() );
		if ( !bAdded )
		{
			// found in pool
			return it->second;
		}

		m_Strings.push_back( pString );
		// advance by string and null
		m_nOffset += strlen( pString ) + 1;

		int stringId = it->second;
		Assert( stringId >= 0 && stringId <= 32767 );

		return stringId;
	}

	virtual bool GetString( short stringId, char *buff, int buffSize )
	{
		if ( stringId < 0 || stringId >= (int)m_Strings.size() )
		{
			V_strncpy( buff, "", buffSize );
			return false;
		}
		V_strncpy( buff, m_Strings[stringId].c_str(), buffSize );
		return true;
	}

	int GetNumStrings()
	{
		return (int)m_Strings.size();
	}

	unsigned int GetPoolSize()
	{
		return m_nOffset;
	}

	// build the final pool
	void GetTableAndPool( std::vector< unsigned int > &offsets, ks::byte_writer &buffer )
	{
		offsets.clear(); offsets.shrink_to_fit();
		buffer.clear();

		offsets.reserve( m_Strings.size() );
		buffer.reserve( m_nOffset );

		unsigned int currentOffset = 0;
		for ( int i = 0; i < (int)m_Strings.size(); i++ )
		{
			offsets.push_back( currentOffset );

			const char *pString = m_Strings[i].c_str();
			buffer.write_cstring( pString );

			currentOffset += strlen( pString ) + 1;
		}
		Assert( currentOffset == m_nOffset );

		// align string pool to end on dword boundary
		while ( buffer.tell() & 0x03 )
		{
			buffer.write< char >( '\0' );
			m_nOffset++;
		}
	}

	void DumpPool()
	{
		for ( int i = 0; i < (int)m_Strings.size(); i++ )
		{
			const char *pString;
			pString = m_Strings[i].c_str();
			Msg( "%s\n", pString );
		}
	}

	void Reset()
	{
		m_Strings.clear();
		m_StringIds.clear();
		m_nOffset = 0;
	}

private:
	// ids are insertion-order indices, persisted in the compiled scenes
	std::vector< std::string >	m_Strings;
	ks::idict< int >			m_StringIds;
	unsigned int			m_nOffset;
};
CChoreoStringPool g_ChoreoStringPool;

//-----------------------------------------------------------------------------
// Helper for crawling events to determine sounds
//-----------------------------------------------------------------------------
void FindSoundsInEvent( CChoreoEvent *pEvent, std::vector< short >& soundList )
{
	if ( !pEvent || pEvent->GetType() != CChoreoEvent::SPEAK )
		return;

	unsigned short stringId = g_ChoreoStringPool.FindOrAddString( pEvent->GetParameters() );
	if ( ks::find_index( soundList, stringId ) == -1 )
	{
		soundList.push_back( stringId );
	}

	if ( pEvent->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( pEvent->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			stringId = g_ChoreoStringPool.FindOrAddString( tok );
			if ( ks::find_index( soundList, stringId ) == -1 )
			{
				soundList.push_back( stringId );
			}
		}
	}
}

static void ChoreScene_MsgDummy( const char *fmt, ... )
{
}

bool UpdateTargetFile_VCD( SceneFile_t *pEntry, const char *pSourceName, const char *pTargetName, bool bWriteToZip, bool bLittleEndian )
{
	pEntry->crcFileName = (CRC32_t)0u;
	pEntry->fileName.assign( ks::nz( pSourceName ) );

	auto sourceBuf = scriptlib->ReadFileToBuffer( pSourceName );
	if ( !sourceBuf )
	{
		return false;
	}

	CRC32_t crcSource;
	CRC32_Init( &crcSource );
	CRC32_ProcessBuffer( &crcSource, sourceBuf->data(), sourceBuf->size() );
	CRC32_Final( &crcSource );

	ParseFromMemory( (char *)sourceBuf->data(), sourceBuf->size() );

	g_SceneTokenProcessor.SetFilename( pSourceName );

	void (*pfnMsgLoad)( const char *fmt, ... ) = ChoreScene_MsgDummy;
#ifndef DBGFLAG_STRINGS_STRIP
	pfnMsgLoad = Msg;
#endif
	CChoreoScene *pChoreoScene = ChoreoLoadScene( pSourceName, nullptr, &g_SceneTokenProcessor, pfnMsgLoad );
	if ( !pChoreoScene )
	{
		return false;
	}

	// Walk all events looking for SPEAK events
	CChoreoEvent *pEvent;
	for ( int i = 0; i < pChoreoScene->GetNumEvents(); ++i )
	{
		pEvent = pChoreoScene->GetEvent( i );
		FindSoundsInEvent( pEvent, pEntry->soundList );
	}

	// calc duration
	pEntry->msecs = (unsigned int)( pChoreoScene->FindStopTime() * 1000.0f + 0.5f );
	pEntry->lastspeak_msecs = (unsigned int)( pChoreoScene->FindLastSpeakTime() * 1000.0f + 0.5f );

	// compile to binary buffer in the platform-native byte order
	ks::byte_writer compiled;
	pChoreoScene->SaveToBinaryBuffer( compiled, crcSource, &g_ChoreoStringPool );

	unsigned int compressedSize;
	unsigned char *pCompressedBuffer = LZMA_Compress( (unsigned char *)compiled.data(), compiled.size(), &compressedSize );
	if ( pCompressedBuffer )
	{
		// replace the compiled buffer with the compressed version
		pEntry->compiledBuffer.assign( (const std::byte *)pCompressedBuffer, (const std::byte *)pCompressedBuffer + compressedSize );
		g_pMemAlloc->Free( pCompressedBuffer );
	}
	else
	{
		pEntry->compiledBuffer = compiled.release();
	}

	delete pChoreoScene;

	return true;
}


//-----------------------------------------------------------------------------
// Create binary compiled version of VCD. Stores to a dictionary for later
// post processing
//-----------------------------------------------------------------------------
bool CreateTargetFile_VCD( const char *pSourceName, const char *pTargetName, bool bWriteToZip, bool bLittleEndian )
{
	int iScene = std::ssize( g_SceneFiles ); g_SceneFiles.emplace_back();

	bool bRet = UpdateTargetFile_VCD( &g_SceneFiles[ iScene ], pSourceName, pTargetName, bWriteToZip, bLittleEndian );
	if ( !bRet )
	{
		g_SceneFiles.erase( g_SceneFiles.begin() + iScene );
	}
	return bRet;
}


struct CSceneImageEntryLessFunc
{
	bool operator()( const SceneImageEntry_t &entryLHS, const SceneImageEntry_t &entryRHS ) const
	{
		return entryLHS.crcFilename < entryRHS.crcFilename;
	}
};



//-----------------------------------------------------------------------------
// A Scene image file contains all the compiled .XCD
//-----------------------------------------------------------------------------
bool CSceneImage::CreateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus )
{
	std::vector< fileList_t >	vcdFileList;
	ks::intern_pool			vcdSymbolTable( true );

	Msg( "\n" );

	// get all the VCD files according to the seacrh paths
	char searchPaths[512];
	g_pFullFileSystem->GetSearchPath( "GAME", false, searchPaths, sizeof( searchPaths ) );
	char *pPath = strtok( searchPaths, ";" );
	while ( pPath )
	{
		int currentCount;
		char szPath[MAX_PATH];
		V_ComposeFileName( pPath, "scenes/*.vcd", szPath, sizeof( szPath ) );

		scriptlib->FindFiles( szPath, true, vcdFileList );

		currentCount = std::ssize( vcdFileList );
		Msg( "Scenes: Searching '%s' - Found %d scenes.\n", szPath, (int)std::ssize( vcdFileList ) - currentCount );

		pPath = strtok( nullptr, ";" );
	}

	if ( vcdFileList.empty() )
	{
		Msg( "Scenes: No Scene Files found!\n" );
		return false;
	}

	// iterate and convert all the VCD files
	bool bGameIsTF = V_stristr( pchModPath, "\\tf" ) != nullptr;
	for ( int i=0; i<std::ssize( vcdFileList ); i++ )
	{
		const char *pFilename = vcdFileList[i].fileName.c_str();
		const char *pSceneName = V_stristr( pFilename, "scenes\\" );
		if ( !pSceneName )
		{
			continue;
		}

		if ( !bLittleEndian && bGameIsTF && V_stristr( pSceneName, "high\\" ) )
		{
			continue;
		}

		// process files in order they would be found in search paths
		// i.e. skipping later processed files that match an earlier conversion
		if ( !vcdSymbolTable.contains( pSceneName ) )
		{
			vcdSymbolTable.intern( pSceneName );

			pStatus->UpdateStatus( pFilename, bQuiet, i, std::ssize( vcdFileList ) );

			if ( !CreateTargetFile_VCD( pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' conversion!\n", pFilename );
			}
		}
	}

	if ( g_SceneFiles.empty() )
	{
		// nothing to do
		return true;
	}

	return WriteSceneImageFile( targetBuffer, bLittleEndian, bQuiet, pStatus );
}

bool CSceneImage::WriteSceneImageFile( ks::byte_writer &targetBuffer, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus )
{
	Msg( "Scenes: Finalizing %d unique scenes.\n", (int)std::ssize( g_SceneFiles ) );

	// get the string pool
	std::vector< unsigned int > stringOffsets;
	ks::byte_writer stringPool;
	g_ChoreoStringPool.GetTableAndPool( stringOffsets, stringPool );

	if ( !bQuiet )
	{
		Msg( "Scenes: String Table: %d bytes\n", (int)( stringOffsets.size() * sizeof( int ) ) );
		Msg( "Scenes: String Pool: %d bytes\n", (int)stringPool.size() );
	}

	// first header, then lookup table, then string pool blob
	int stringPoolStart = sizeof( SceneImageHeader_t ) + std::ssize( stringOffsets ) * sizeof( int );
	// then directory
	int sceneEntryStart = stringPoolStart + stringPool.size();
	// then variable sized summaries
	int sceneSummaryStart = sceneEntryStart + std::ssize( g_SceneFiles ) * sizeof( SceneImageEntry_t );
	// then variable sized compiled binary scene data
	int sceneDataStart = 0;

	// construct header
	SceneImageHeader_t imageHeader = { 0 };
	imageHeader.nId = SCENE_IMAGE_ID;
	imageHeader.nVersion = SCENE_IMAGE_VERSION;
	imageHeader.nNumScenes = std::ssize( g_SceneFiles );
	imageHeader.nNumStrings = std::ssize( stringOffsets );
	imageHeader.nSceneEntryOffset = sceneEntryStart;
	if ( !bLittleEndian )
	{
		imageHeader.nId = BigLong( imageHeader.nId );
		imageHeader.nVersion = BigLong( imageHeader.nVersion );
		imageHeader.nNumScenes = BigLong( imageHeader.nNumScenes );
		imageHeader.nNumStrings = BigLong( imageHeader.nNumStrings );
		imageHeader.nSceneEntryOffset = BigLong( imageHeader.nSceneEntryOffset );
	}
	targetBuffer.write_bytes( &imageHeader, sizeof( imageHeader ) );

	// header is immediately followed by string table and pool
	for ( int i = 0; i < std::ssize( stringOffsets ); i++ )
	{
		unsigned int offset = stringPoolStart + stringOffsets[i];
		if ( !bLittleEndian )
		{
			offset = BigLong( offset );
		}
		targetBuffer.write< int >( offset );
	}
	Assert( stringPoolStart == (int)targetBuffer.tell() );
	targetBuffer.write_bytes( stringPool.bytes() );

	// construct directory
	// sorted by CSceneImageEntryLessFunc; entries are fixed up in place below
	std::vector< SceneImageEntry_t > imageDirectory;
	imageDirectory.reserve( std::ssize( g_SceneFiles ) );

	// build directory
	// directory is linear sorted by filename checksum for later binary search
	for ( int i = 0; i < std::ssize( g_SceneFiles ); i++ )
	{
		SceneImageEntry_t imageEntry = { 0 };

		CRC32_t crcFilename = g_SceneFiles[ i ].crcFileName;
		if ( crcFilename == 0 )
		{
			Assert( !g_SceneFiles[i].fileName.empty() );

			// name needs to be normalized for determinstic later CRC name calc
			// calc crc based on scenes\anydir\anyscene.vcd
			char szCleanName[MAX_PATH];
			V_strncpy( szCleanName, g_SceneFiles[i].fileName.c_str(), sizeof( szCleanName ) );
			V_strlower( szCleanName );
			V_FixSlashes( szCleanName );
			char *pName = V_stristr( szCleanName, "scenes\\" );
			if ( !pName )
			{
				// must have scenes\ in filename
				Error( "CreateSceneImageFile: Unexpected lack of scenes prefix on %s\n", g_SceneFiles[i].fileName.c_str() );
			}

			crcFilename = CRC32_ProcessSingleBuffer( pName, strlen( pName ) );
		}

		imageEntry.crcFilename = crcFilename;

		// temp store an index to its file, fixup later, necessary to access post sort
		imageEntry.nDataOffset = i;
		if ( std::ranges::binary_search( imageDirectory, imageEntry, CSceneImageEntryLessFunc() ) )
		{
			// filename checksums must be unique or runtime binary search would be bogus
			Error( "CreateSceneImageFile: Unexpected filename checksum collision!\n" );
		}		

		imageDirectory.insert( std::ranges::upper_bound( imageDirectory, imageEntry, CSceneImageEntryLessFunc() ), imageEntry );
	}

	// determine sort order and start of data after dynamic summaries
	std::vector< int > writeOrder;
	writeOrder.reserve( std::ssize( g_SceneFiles ) );
	sceneDataStart = sceneSummaryStart;
	for ( int i = 0; i < std::ssize( imageDirectory ); i++ )
	{
		// reclaim offset, indicates write order of scene file
		int iScene = imageDirectory[i].nDataOffset;
		writeOrder.push_back( iScene );

		// march past each variable sized summary to determine start of scene data
		int numSounds = std::ssize( g_SceneFiles[iScene].soundList );
		sceneDataStart += sizeof( SceneImageSummary_t ) + ( numSounds - 1 ) * sizeof( int );
	}

	// finalize and write directory
	Assert( sceneEntryStart == (int)targetBuffer.tell() );
	int nSummaryOffset = sceneSummaryStart;
	int nDataOffset = sceneDataStart;
	for ( int i = 0; i < std::ssize( imageDirectory ); i++ )
	{
		int iScene = writeOrder[i];

		imageDirectory[i].nDataOffset = nDataOffset;
		imageDirectory[i].nDataLength = g_SceneFiles[iScene].compiledBuffer.size();
		imageDirectory[i].nSceneSummaryOffset = nSummaryOffset;
		if ( !bLittleEndian )
		{
			imageDirectory[i].crcFilename = BigLong( imageDirectory[i].crcFilename );
			imageDirectory[i].nDataOffset = BigLong( imageDirectory[i].nDataOffset );
			imageDirectory[i].nDataLength = BigLong( imageDirectory[i].nDataLength );
			imageDirectory[i].nSceneSummaryOffset = BigLong( imageDirectory[i].nSceneSummaryOffset );
		}
		targetBuffer.write_bytes( &imageDirectory[i], sizeof( SceneImageEntry_t ) );

		int numSounds = std::ssize( g_SceneFiles[iScene].soundList );
		nSummaryOffset += sizeof( SceneImageSummary_t ) + (numSounds - 1) * sizeof( int );

		nDataOffset += g_SceneFiles[iScene].compiledBuffer.size();
	}

	// finalize and write summaries
	Assert( sceneSummaryStart == (int)targetBuffer.tell() );
	for ( int i = 0; i < std::ssize( imageDirectory ); i++ )
	{
		int iScene = writeOrder[i];
		int msecs = g_SceneFiles[iScene].msecs;
		int soundCount = std::ssize( g_SceneFiles[iScene].soundList );
		unsigned int lastspeak = g_SceneFiles[iScene].lastspeak_msecs;
		Assert( lastspeak < msecs );
		if ( !bLittleEndian )
		{
			msecs = BigLong( msecs );
			soundCount = BigLong( soundCount );
			lastspeak = BigLong( lastspeak );
		}
		targetBuffer.write< int >( msecs );
		targetBuffer.write< int >( lastspeak );
		targetBuffer.write< int >( soundCount );
		for ( int j = 0; j < std::ssize( g_SceneFiles[iScene].soundList ); j++ )
		{
			int soundId = g_SceneFiles[iScene].soundList[j];
			if ( !bLittleEndian )
			{
				soundId = BigLong( soundId );
			}
			targetBuffer.write< int >( soundId );
		}
	}

	// finalize and write data
	Assert( sceneDataStart == (int)targetBuffer.tell() );
	for ( int i = 0; i < std::ssize( imageDirectory ); i++ )
	{	
		int iScene = writeOrder[i];
		targetBuffer.write_bytes( g_SceneFiles[iScene].compiledBuffer );
	}

	if ( !bQuiet )
	{
		Msg( "Scenes: Final size: %.2f MB\n", targetBuffer.tell() / (1024.0f * 1024.0f ) );
	}

	// cleanup
	g_SceneFiles.clear(); g_SceneFiles.shrink_to_fit();

	return true;
}

static int FindSceneByCRC( CRC32_t crcFilename )
{
	// use binary search, entries are sorted by ascending crc
	int nLowerIdx = 1;
	int nUpperIdx = std::ssize( g_SceneFiles );
	for ( ;; )
	{
		if ( nUpperIdx < nLowerIdx )
		{
			return -1;
		}
		else
		{
			int nMiddleIndex = ( nLowerIdx + nUpperIdx )/2;
			CRC32_t nProbe = g_SceneFiles[nMiddleIndex-1].crcFileName;
			if ( crcFilename < nProbe )
			{
				nUpperIdx = nMiddleIndex - 1;
			}
			else
			{
				if ( crcFilename > nProbe )
				{
					nLowerIdx = nMiddleIndex + 1;
				}
				else
				{
					return nMiddleIndex - 1;
				}
			}
		}
	}

	return -1;
}

// This will update the scenes.image file, or create a new one if it doesn't exist
// The caller should pass in the existing .image file in targetBuffer!!!
bool CSceneImage::UpdateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus, std::string *pFilesToUpdate, int nUpdateCount )
{
	// Prime everything using existing data file
	if ( targetBuffer.size() == 0 )
	{
		return CreateSceneImageFile( targetBuffer, pchModPath, bLittleEndian, bQuiet, pStatus );
	}

	bool bSuccess = true;

	g_SceneFiles.clear(); g_SceneFiles.shrink_to_fit();
	g_ChoreoStringPool.Reset();

	ks::byte_reader existing = targetBuffer.reader();

	// Load stuff
	SceneImageHeader_t imageHeader;
	existing.read_bytes( &imageHeader, sizeof( imageHeader ) );
	if ( !bLittleEndian )
	{
		imageHeader.nId = BigLong( imageHeader.nId );
		imageHeader.nVersion = BigLong( imageHeader.nVersion );
		imageHeader.nNumScenes = BigLong( imageHeader.nNumScenes );
		imageHeader.nNumStrings = BigLong( imageHeader.nNumStrings );
		imageHeader.nSceneEntryOffset = BigLong( imageHeader.nSceneEntryOffset );
	}

	if ( imageHeader.nId != SCENE_IMAGE_ID )
	{
		bSuccess = false;
	}
	if ( imageHeader.nVersion != SCENE_IMAGE_VERSION )
	{
		bSuccess = false;
	}

	if ( bSuccess )
	{
		// Re-Build the string pool

		// first header, then lookup table, then string pool blob
		int stringPoolStart = sizeof( SceneImageHeader_t ) + imageHeader.nNumStrings * sizeof( int );
		// then directory
		int sceneEntryStart = imageHeader.nSceneEntryOffset;

		existing.seek( stringPoolStart );

		char str[ 4096 ];
		for ( int i = 0; i < imageHeader.nNumStrings; ++i )
		{
			existing.read_cstring( str, sizeof( str ) );
			g_ChoreoStringPool.FindOrAddString( str );
		}

		// Now read in the file data

		for ( int i = 0; i < imageHeader.nNumScenes; ++i )
		{
			existing.seek( sceneEntryStart + i * sizeof( SceneImageEntry_t ) );
			SceneImageEntry_t entry = existing.read< SceneImageEntry_t >();
			SceneImageEntry_t *pEntry = &entry;
			if ( !bLittleEndian )
			{
				pEntry->crcFilename = BigLong( pEntry->crcFilename );
				pEntry->nDataOffset = BigLong( pEntry->nDataOffset );
				pEntry->nDataLength = BigLong( pEntry->nDataLength );
				pEntry->nSceneSummaryOffset = BigLong( pEntry->nSceneSummaryOffset );
			}

			existing.seek( pEntry->nSceneSummaryOffset );
			unsigned int summaryMsecs = existing.read< unsigned int >();
			unsigned int summaryLastSpeech = existing.read< unsigned int >();
			int summaryNumSounds = existing.read< int >();

			// Now read in the data
			int idx = std::ssize( g_SceneFiles ); g_SceneFiles.emplace_back();
			SceneFile_t &scene = g_SceneFiles[ idx ];

			// We only load the crc based filenames for appending/replacing
			scene.crcFileName = pEntry->crcFilename;
			existing.seek( pEntry->nDataOffset );
			const auto data = existing.read_span( pEntry->nDataLength );
			scene.compiledBuffer.assign( data.begin(), data.end() );
			scene.msecs = summaryMsecs;
			scene.lastspeak_msecs = summaryLastSpeech;
			// Load sounds
			existing.seek( pEntry->nSceneSummaryOffset + offsetof( SceneImageSummary_t, soundStrings ) );
			for ( int j = 0 ; j < summaryNumSounds; ++j )
			{
				scene.soundList.push_back( existing.read< int >() );
			}
		}
	}

	Assert( std::ssize( g_SceneFiles ) == imageHeader.nNumScenes );

	// Now validate that the scenes list is sorted correctly
	CRC32_t current = (CRC32_t)0;
	for ( int i = 0 ; i < std::ssize( g_SceneFiles ); ++i )
	{
		CRC32_t crc = g_SceneFiles[ i ].crcFileName;
		Assert( crc != (CRC32_t)0 );
		if ( crc <= current )
		{
			Error( "UpdateSceneImageFile:  Scene Files not in CRC order\n" );
		}
		current = crc;
	}

	// Now add the additional files
	bool bGameIsTF = V_stristr( pchModPath, "\\tf" ) != nullptr;
	for ( int i = 0; i < nUpdateCount; ++i )
	{
		const char *pFilename = pFilesToUpdate[i].c_str();
		const char *pSceneName = V_stristr( pFilename, "scenes\\" );
		if ( !pSceneName )
		{
			continue;
		}

		if ( !bLittleEndian && bGameIsTF && V_stristr( pSceneName, "high\\" ) )
		{
			continue;
		}

		// name needs to be normalized for determinstic later CRC name calc
		// calc crc based on scenes\anydir\anyscene.vcd
		char szCleanName[MAX_PATH];
		V_strncpy( szCleanName, pFilename, sizeof( szCleanName ) );
		V_strlower( szCleanName );
		V_FixSlashes( szCleanName );
		char *pName = V_stristr( szCleanName, "scenes\\" );
		if ( !pName )
		{
			// must have scenes\ in filename
			Error( "UpdateSceneImageFile: Unexpected lack of scenes prefix on %s\n", pFilename );
		}

		CRC32_t crcFilename = CRC32_ProcessSingleBuffer( pName, strlen( pName ) );

		pStatus->UpdateStatus( pFilename, bQuiet, i, nUpdateCount );

		int idx = FindSceneByCRC( crcFilename );
		// Not found, append entry
		if ( idx == -1 )
		{
			if ( !CreateTargetFile_VCD( pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' conversion!\n", pFilename );
			}
		}
		else
		// Found it, let's just update entry
		{
			if ( !UpdateTargetFile_VCD( &g_SceneFiles[ idx ], pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' update!\n", pFilename );
			}
		}
	}

	// Now write the final data out
	targetBuffer.clear();
	return WriteSceneImageFile( targetBuffer, bLittleEndian, bQuiet, pStatus );
}

