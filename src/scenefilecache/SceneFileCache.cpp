// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Cache for VCDs. PC async loads and uses the datacache to manage.
// 360 uses a baked resident image of aggregated compiled VCDs.

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.checksum_crc;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.lzmadecoder;
import ks.tier1.strtools;
#include "scenefilecache/ISceneFileCache.h"
#include "filesystem.h"
#include "scenefilecache/SceneImageFile.h"
#include "choreoscene.h"

IFileSystem	*filesystem = nullptr;
																				
class CSceneFileCache : public ISceneFileCache
{
public:
	// App system protocol
	bool			Connect( CreateInterfaceFn factory );
	void			Disconnect();
	void			*QueryInterface( const char *pInterfaceName ) { return nullptr; }
	InitReturnVal_t Init();
	void			Shutdown();

	// ISceneFileCache
	// Physically reloads image from disk
	void			Reload();

	std::size_t			GetSceneBufferSize( char const *pFilename );
	bool			GetSceneData( char const *pFilename, byte *buf, std::size_t bufsize );

	// alternate resident image implementation
	bool			GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData );
	short			GetSceneCachedSound( int iScene, int iSound );
	const char		*GetSceneString( short stringId );

private:
	// alternate implementation - uses a resident baked image of the file cache, contains all the compiled VCDs
	// single i/o read at startup to mount the image
	int						FindSceneInImage( const char *pSceneName );
	bool					GetSceneDataFromImage( const char *pSceneName, int iIndex, byte *pData, std::size_t *pLength );

private:
	std::vector< std::byte >		m_SceneImageFile;
};

bool CSceneFileCache::Connect( CreateInterfaceFn factory )
{
	if ( (filesystem = (IFileSystem *)factory( FILESYSTEM_INTERFACE_VERSION,nullptr )) == nullptr )
	{
		return false;
	}
	
	return true;
}

void CSceneFileCache::Disconnect()
{
}

InitReturnVal_t CSceneFileCache::Init()
{
	const char *pSceneImageName = "scenes/scenes"".image";

	if ( m_SceneImageFile.empty() )
	{

		if ( auto file = filesystem->ReadFile( pSceneImageName, "GAME" ) )
		{
			m_SceneImageFile = std::move( *file );
			SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
			if ( pHeader->nId != SCENE_IMAGE_ID || 
				pHeader->nVersion != SCENE_IMAGE_VERSION )
			{
				Error( "CSceneFileCache: Bad scene image file %s\n", pSceneImageName );
			}
		}
	}

	return INIT_OK;
}

void CSceneFileCache::Shutdown()
{
	std::vector< std::byte >().swap( m_SceneImageFile );
}

// Physically reloads image from disk
void CSceneFileCache::Reload()
{
	Shutdown();
	Init();
}

std::size_t CSceneFileCache::GetSceneBufferSize( char const *pFilename )
{
	std::size_t returnSize = 0;

	char fn[MAX_PATH];
	V_strncpy( fn, pFilename, sizeof( fn ) );
	V_FixSlashes( fn );
	V_strlower( fn );

	GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), nullptr, &returnSize );
	return returnSize;
}

bool CSceneFileCache::GetSceneData( char const *pFilename, byte *buf, std::size_t bufsize )
{
	contract_assert( pFilename );
	contract_assert( buf );
	contract_assert( bufsize > 0 );

	char fn[MAX_PATH];
	V_strncpy( fn, pFilename, sizeof( fn ) );
	V_FixSlashes( fn );
	V_strlower( fn );

	std::size_t nLength = bufsize;
	return GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), buf, &nLength );
}

bool CSceneFileCache::GetSceneCachedData( char const *pFilename, SceneCachedData_t * __restrict__ pData )
{
	int iScene = FindSceneInImage( pFilename );
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// not available
		pData->sceneId = -1;
		pData->msecs = 0;
		pData->m_fLastSpeakSecs = 0;
		pData->numSounds = 0;
		return false;
	}

	// get scene summary
	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	SceneImageSummary_t *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	
	pData->sceneId = iScene;
	pData->msecs = pSummary->msecs;
	pData->m_fLastSpeakSecs = pSummary->GetDurToSpeechEnd();
	pData->numSounds = pSummary->numSounds;

	return true;
}

short CSceneFileCache::GetSceneCachedSound( int iScene, int iSound )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// huh?, image file not present or bad index
		return -1;
	}

	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	SceneImageSummary_t *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	if ( iSound < 0 || iSound >= pSummary->numSounds )
	{
		// bad index
		contract_assert( 0 );
		return -1;
	}

	return pSummary->soundStrings[iSound];
}

const char *CSceneFileCache::GetSceneString( short stringId )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
	if ( !pHeader || stringId < 0 || stringId >= pHeader->nNumStrings )
	{
		// huh?, image file not present, or index bad
		return nullptr;
	}

	return pHeader->String( stringId );
}

//-----------------------------------------------------------------------------
//  Returns -1 if not found, otherwise [0..n] index.
//-----------------------------------------------------------------------------
int CSceneFileCache::FindSceneInImage( const char *pSceneName )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
	if ( !pHeader )
	{
		return -1;
	}
	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );

	char szCleanName[MAX_PATH];
	V_strncpy( szCleanName, pSceneName, sizeof( szCleanName ) );
	V_strlower( szCleanName );
	V_FixSlashes( szCleanName, '\\' );
	// Many vcd's in CSGO have a '.' in the filename, which breaks this call
	// We're going to assume that all filenames have the correct extension
//	V_SetExtension( szCleanName, ".vcd", sizeof( szCleanName ) );

	CRC32_t crcFilename = CRC32_ProcessSingleBuffer( szCleanName, std::strlen( szCleanName ) );

	// use binary search, entries are sorted by ascending crc
	int nLowerIdx = 1;
	int nUpperIdx = pHeader->nNumScenes;
	for ( ;; )
	{
		if ( nUpperIdx < nLowerIdx )
		{
			return -1;
		}
		else
		{
			int nMiddleIndex = ( nLowerIdx + nUpperIdx )/2;
			CRC32_t nProbe = pEntries[nMiddleIndex-1].crcFilename;
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

//-----------------------------------------------------------------------------
//  Returns true if success, false otherwise. Caller must free ouput scene data
//-----------------------------------------------------------------------------
bool CSceneFileCache::GetSceneDataFromImage( const char *pFileName, int iScene, byte *pSceneData, std::size_t *pSceneLength )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.data();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		if ( pSceneData )
		{
			*pSceneData = 0;
		}
		if ( pSceneLength )
		{
			*pSceneLength = 0;
		}
		return false;
	}

	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	unsigned char *pData = (unsigned char *)pHeader + pEntries[iScene].nDataOffset;
	CLZMA lzma;
	bool bIsCompressed;
	bIsCompressed = lzma.IsCompressed( pData );
	if ( bIsCompressed )
	{
		int originalSize = lzma.GetActualSize( pData );
		if ( pSceneData )
		{
			int nMaxLen = *pSceneLength;
			if ( originalSize <= nMaxLen )
			{
				lzma.Uncompress( pData, pSceneData );
			}
			else
			{
				unsigned char *pOutputData = (unsigned char *)MemAlloc_Alloc( originalSize );
				lzma.Uncompress( pData, pOutputData );
				V_memcpy( pSceneData, pOutputData, nMaxLen );
				g_pMemAlloc->Free( pOutputData );
			}
		}
		if ( pSceneLength )
		{
			*pSceneLength = originalSize;
		}
	}
	else
	{
		if ( pSceneData )
		{
			std::size_t nCountToCopy = std::min(*pSceneLength, (std::size_t)pEntries[iScene].nDataLength );
			V_memcpy( pSceneData, pData, nCountToCopy );
		}
		if ( pSceneLength )
		{
			*pSceneLength = (std::size_t)pEntries[iScene].nDataLength;
		}
	}
	return true;
}

static CSceneFileCache g_SceneFileCache;
static InterfaceReg s_Reg_CSceneFileCache_ISceneFileCache( []() -> void * { return static_cast<ISceneFileCache *>( &g_SceneFileCache ); }, SCENE_FILE_CACHE_INTERFACE_VERSION );;


// The ISceneFileCache members forward to CSceneFileCache
#include "facade/iscenefilecache.inl"
