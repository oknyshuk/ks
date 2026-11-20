// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
#include "tier2/riff.h"
#include "snd_io.h"
#include "filesystem.h"
#include "filesystem_engine.h"
import std;

//-----------------------------------------------------------------------------
// Purpose: Implements Audio IO on the engine's COMMON filesystem
//-----------------------------------------------------------------------------
// prepend sound/ to the filename -- all sounds are loaded from the sound/ directory
FileHandle_t IFileReadBinary::open( const char *pFileName )
{
	char namebuffer[512];
	FileHandle_t hFile;
	V_strncpy( namebuffer, "sound", sizeof( namebuffer ) );

	// the server is sending back sound names with slashes in front... 
	if ( pFileName[0] != '/' && pFileName[0] != '\\' )
	{
		V_strncat( namebuffer, "/", sizeof( namebuffer ), COPY_ALL_CHARACTERS );
	}

	V_strncat( namebuffer, pFileName, sizeof( namebuffer ), COPY_ALL_CHARACTERS );

	hFile = g_pFileSystem->Open( namebuffer, "rb", "GAME" );

	return hFile;
}

int IFileReadBinary::read( void *pOutput, int size, FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFileSystem->Read( pOutput, size, file );
}

void IFileReadBinary::seek( FileHandle_t file, int pos )
{
	if ( !file )
		return;

	g_pFileSystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int IFileReadBinary::tell( FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFileSystem->Tell( file );
}

unsigned int IFileReadBinary::size( FileHandle_t file )
{
	if (!file)
		return 0;

	return g_pFileSystem->Size( file );
}

void IFileReadBinary::close( FileHandle_t file )
{
	if (!file)
		return;

	g_pFileSystem->Close( file );
}

static IFileReadBinary io;
IFileReadBinary *g_pSndIO = &io;
