//===== Copyright 2005-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: Helper methods + classes for sound
//
//===========================================================================//

#include "tier2/soundutils.h"
#include "tier2/riff.h"
#include "tier2/tier2.h"
#include "filesystem.h"

#ifdef IS_WINDOWS_PC

#include <windows.h> // WAVEFORMATEX, WAVEFORMAT and ADPCM WAVEFORMAT!!!
#include <mmreg.h>

#else


#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// RIFF reader/writers that use the file system
//-----------------------------------------------------------------------------
class CFSIOReadBinary : public IFileReadBinary
{
public:
	// inherited from IFileReadBinary
	virtual FileHandle_t open( const char *pFileName );
	virtual int read( void *pOutput, int size, FileHandle_t file );
	virtual void seek( FileHandle_t file, int pos );
	virtual unsigned int tell( FileHandle_t file );
	virtual unsigned int size( FileHandle_t file );
	virtual void close( FileHandle_t file );
};

class CFSIOWriteBinary : public IFileWriteBinary
{
public:
	virtual FileHandle_t create( const char *pFileName );
	virtual int write( void *pData, int size, FileHandle_t file );
	virtual void close( FileHandle_t file );
	virtual void seek( FileHandle_t file, int pos );
	virtual unsigned int tell( FileHandle_t file );
};


//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CFSIOReadBinary s_FSIoIn;
static CFSIOWriteBinary s_FSIoOut;

IFileReadBinary *g_pFSIOReadBinary = &s_FSIoIn;
IFileWriteBinary *g_pFSIOWriteBinary = &s_FSIoOut;


//-----------------------------------------------------------------------------
// RIFF reader that use the file system
//-----------------------------------------------------------------------------
FileHandle_t CFSIOReadBinary::open( const char *pFileName )
{
	return g_pFullFileSystem->Open( pFileName, "rb" );
}

int CFSIOReadBinary::read( void *pOutput, int size, FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Read( pOutput, size, file );
}

void CFSIOReadBinary::seek( FileHandle_t file, int pos )
{
	if ( !file )
		return;

	g_pFullFileSystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int CFSIOReadBinary::tell( FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Tell( file );
}

unsigned int CFSIOReadBinary::size( FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Size( file );
}

void CFSIOReadBinary::close( FileHandle_t file )
{
	if ( !file )
		return;

	g_pFullFileSystem->Close( file );
}


//-----------------------------------------------------------------------------
// RIFF writer that use the file system
//-----------------------------------------------------------------------------
FileHandle_t CFSIOWriteBinary::create( const char *pFileName )
{
	g_pFullFileSystem->SetFileWritable( pFileName, true );
	return g_pFullFileSystem->Open( pFileName, "wb" );
}

int CFSIOWriteBinary::write( void *pData, int size, FileHandle_t file )
{
	return g_pFullFileSystem->Write( pData, size, file );
}

void CFSIOWriteBinary::close( FileHandle_t file )
{
	g_pFullFileSystem->Close( file );
}

void CFSIOWriteBinary::seek( FileHandle_t file, int pos )
{
	g_pFullFileSystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int CFSIOWriteBinary::tell( FileHandle_t file )
{
	return g_pFullFileSystem->Tell( file );
}


