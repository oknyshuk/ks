// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.generichash;
import ks.tier1.interfaces;
import ks.tier1.strtools;
#include "resourcefile/resourcedictionary.h"
import std;
#include "tier2/fileutils.h"

constexpr auto RESOURCE_ID_HASH_SEED = 0xEDABCDEF;


//-----------------------------------------------------------------------------
// Computes a resource id given a resource name
//-----------------------------------------------------------------------------
ResourceId_t ComputeResourceIdHash( const char *pResourceName )
{
	if ( !pResourceName || !pResourceName[0] )
		return RESOURCE_ID_INVALID;
	int nLength = V_strlen( pResourceName );
	return (ResourceId_t)MurmurHash2( pResourceName, nLength, RESOURCE_ID_HASH_SEED );
}
				
void GenerateResourceFileName( const char *pFileName, char *pResourceFileName, std::size_t nBufLen )
{
	char pContentName[MAX_PATH];
	char pContentNameNoExt[MAX_PATH];
	char pFixedContentName[MAX_PATH];
	if ( V_IsAbsolutePath( pFileName ) )
	{
		ComputeModContentFilename( pFileName, pContentName, sizeof(pContentName) );
		V_StripExtension( pContentName, pContentNameNoExt, sizeof(pContentNameNoExt) );
		V_FixupPathName( pFixedContentName, sizeof(pFixedContentName), pContentNameNoExt );
		g_pFullFileSystem->FullPathToRelativePathEx( pFixedContentName, "CONTENT", pResourceFileName, nBufLen );
	}
	else
	{
		V_StripExtension( pFileName, pContentNameNoExt, sizeof(pContentNameNoExt) );
		V_FixupPathName( pResourceFileName, nBufLen, pContentNameNoExt );
	}
}

void GenerateResourceName( const char *pFileName, const char *pSubResourceName, char *pResourceName, std::size_t pBufLen )
{
	if ( !pSubResourceName )
	{
		pSubResourceName = "";
	}

	char pFixedFileName[MAX_PATH];
	char pFixedSubResourceName[MAX_PATH];
	GenerateResourceFileName( pFileName, pFixedFileName, sizeof(pFixedFileName) );
	V_strncpy( pFixedSubResourceName, pSubResourceName, sizeof(pFixedSubResourceName) );
	V_strlower( pFixedSubResourceName );
	V_snprintf( pResourceName, pBufLen, "%s::%s", pFixedFileName, pFixedSubResourceName );
}

ResourceId_t ComputeResourceIdHash( const char *pFileName, const char *pSubResourceName )
{
	if ( !pFileName || !pFileName[0] )
		return RESOURCE_ID_INVALID;

	char pTemp[MAX_PATH+128];
	GenerateResourceName( pFileName, pSubResourceName, pTemp, sizeof(pTemp) );
	return ComputeResourceIdHash( pTemp );
}
