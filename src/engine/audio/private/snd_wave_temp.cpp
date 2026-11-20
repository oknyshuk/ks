// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Create an output wave stream.  Used to record audio for in-engine movies or
// mixer debugging.

import std;


import ks.core.dbg;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;

#include "tier2/riff.h"
#include "sound.h"
#include "soundservice.h"
#include "filesystem.h"

#include "snd_device.h"
#include "sound_private.h"
#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_convars.h"
#include "snd_dev_xaudio.h"
#include "snd_wave_source.h"
#include "snd_wave_temp.h"
#include "snd_wave_data.h"
#include "snd_wave_mixer_private.h"

#include "audio/private/posix_stubs.h"

extern IFileSystem *g_pFileSystem;
// FIXME: shouldn't this API be part of IFileSystem?
extern bool COM_CopyFile( const char *netpath, const char *cachepath );

// Create a wave file
void WaveCreateTmpFile( const char *filename, int rate, int bits, int channels )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	V_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "wb" );
	if ( file == FILESYSTEM_INVALID_HANDLE )
		return;

	int chunkid = ( RIFF_ID );
	int chunksize = ( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	chunkid = ( RIFF_WAVE );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );

	// create a 16-bit PCM stereo output file
	PCMWAVEFORMAT fmt = { 0 };
	fmt.wf.wFormatTag = ( (short)WAVE_FORMAT_PCM );
	fmt.wf.nChannels = ( (short)channels );
	fmt.wf.nSamplesPerSec = ( rate );
	fmt.wf.nAvgBytesPerSec = ( rate * bits * channels / 8 );
	fmt.wf.nBlockAlign = ( (short)( 2 * channels ) );
	fmt.wBitsPerSample = ( (short)bits );

	chunkid = ( WAVE_FMT );
	chunksize = ( sizeof(fmt) );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );
	g_pFileSystem->Write( &fmt, sizeof( PCMWAVEFORMAT ), file );

	chunkid = ( WAVE_DATA );
	chunksize = ( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	g_pFileSystem->Close( file );
}

void WaveAppendTmpFile( const char *filename, void *pBuffer, int sampleBits, int numSamples )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	V_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( file == FILESYSTEM_INVALID_HANDLE )
		return;

	g_pFileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );

	g_pFileSystem->Write( pBuffer, numSamples * sampleBits/8, file );

	g_pFileSystem->Close( file );
}

void WaveFixupTmpFile( const char *filename )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename, sizeof( tmpfilename ) );
	V_DefaultExtension( tmpfilename, ".WAV", sizeof( tmpfilename ) );

	FileHandle_t file;
	file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( FILESYSTEM_INVALID_HANDLE == file )
	{
		Warning( "WaveFixupTmpFile( '%s' ) failed to open file for editing\n", tmpfilename );
		return;
	}
	
	// file size goes in RIFF chunk
	int size = g_pFileSystem->Size( file ) - 2*sizeof( int );
	// offset to data chunk
	int headerSize = (sizeof(int)*5 + sizeof(PCMWAVEFORMAT));
	// size of data chunk
	int dataSize = size - headerSize;

	size = ( size );
	g_pFileSystem->Seek( file, sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &size, sizeof( int ), file );

	// skip the header and the 4-byte chunk tag and write the size
	dataSize = ( dataSize );
	g_pFileSystem->Seek( file, headerSize+sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &dataSize, sizeof( int ), file );

	g_pFileSystem->Close( file );
}

static void movie_fixwave( const CCommand &args );
static ConCommand movie_fixwave_command( "movie_fixwave", movie_fixwave, "Fixup corrupted .wav file if engine crashed during startmovie/endmovie, etc." );
static void movie_fixwave( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		Msg ("Usage: movie_fixwave wavname\n");
		return;
	}

	char const *wavname = args.Arg( 1 );
	if ( !g_pFileSystem->FileExists( wavname ) )
	{
		Warning( "movie_fixwave: File '%s' does not exist\n", wavname );
		return;
	}

	char tmpfilename[256];
	V_StripExtension( wavname, tmpfilename, sizeof( tmpfilename ) );
	V_strncat( tmpfilename, "_fixed", sizeof( tmpfilename ), COPY_ALL_CHARACTERS );
	V_DefaultExtension( tmpfilename, ".wav", sizeof( tmpfilename ) );

	// Now copy the file
	Msg( "Copying '%s' to '%s'\n", wavname, tmpfilename );
	COM_CopyFile( wavname, tmpfilename );

	Msg( "Performing fixup on '%s'\n", tmpfilename );
	WaveFixupTmpFile( tmpfilename );
}
