#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
#include "filesystem.h"
import std;

//-----------------------------------------------------------------------------
// Purpose: This is a simple abstraction that the RIFF classes use to read from
//			files/memory
//-----------------------------------------------------------------------------
class IFileReadBinary
{
public:
	// The one implementation lives in engine/snd_io.cpp: sound files are read from the sound/ directory
	// of the engine's file system.
	FileHandle_t open( const char *pFileName );
	int read( void *pOutput, int size, FileHandle_t file );
	void close( FileHandle_t file );
	void seek( FileHandle_t file, int pos );
	unsigned int tell( FileHandle_t file );
	unsigned int size( FileHandle_t file );
};


//-----------------------------------------------------------------------------
// Purpose: Used to read/parse a RIFF format file
//-----------------------------------------------------------------------------
class InFileRIFF
{
public:
	InFileRIFF( const char *pFileName, IFileReadBinary &io );
	~InFileRIFF( void );

	unsigned int RIFFName( void ) { return m_riffName; }
	unsigned int RIFFSize( void ) { return m_riffSize; }
	unsigned int GetFileSize() const { return m_nFileSize; }

	int		ReadInt( void );
	int		ReadData( void *pOutput, int dataSize );
	int		PositionGet( void );
	void	PositionSet( int position );
	bool	IsValid( void ) { return m_file != 0; }

private:
	const InFileRIFF & operator=( const InFileRIFF & );

	IFileReadBinary		&m_io;
	FileHandle_t		m_file;
	unsigned int		m_riffName;
	unsigned int		m_riffSize;
	unsigned int		m_nFileSize;
};


//-----------------------------------------------------------------------------
// Purpose: Used to iterate over an InFileRIFF
//-----------------------------------------------------------------------------
class IterateRIFF
{
public:
	IterateRIFF( InFileRIFF &riff, int size );
	IterateRIFF( IterateRIFF &parent );

	bool			ChunkAvailable( void );
	bool			ChunkNext( void );

	unsigned int	ChunkName( void );
	unsigned int	ChunkSize( void );
	int				ChunkRead( void *pOutput );
	int				ChunkReadPartial( void *pOutput, int dataSize );
	int				ChunkReadInt( void );
	int				ChunkFilePosition( void ) { return m_chunkPosition; }

private:
	const IterateRIFF & operator=( const IterateRIFF & );

	void	ChunkSetup( void );
	void	ChunkClear( void );

	InFileRIFF			&m_riff;
	int					m_start;
	int					m_size;

	unsigned int		m_chunkName;
	int					m_chunkSize;
	int					m_chunkPosition;
};

#define RIFF_ID					MAKEID('R','I','F','F')
#define RIFF_WAVE				MAKEID('W','A','V','E')
#define WAVE_FMT				MAKEID('f','m','t',' ')
#define WAVE_DATA				MAKEID('d','a','t','a')
#define WAVE_FACT				MAKEID('f','a','c','t')
#define WAVE_CUE				MAKEID('c','u','e',' ')
#define WAVE_SAMPLER			MAKEID('s','m','p','l')
#define WAVE_VALVEDATA			MAKEID('V','D','A','T')
#define WAVE_PADD				MAKEID('P','A','D','D')
#define WAVE_LIST				MAKEID('L','I','S','T') 

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM			0x0001
#endif
#ifndef WAVE_FORMAT_ADPCM
#define WAVE_FORMAT_ADPCM		0x0002
#endif
inline constexpr auto WAVE_FORMAT_XBOX_ADPCM = 0x0069;
#ifndef WAVE_FORMAT_XMA
#define WAVE_FORMAT_XMA			0x0165
#endif
#ifndef WAVE_FORMAT_MP3
#define WAVE_FORMAT_MP3			0x0003
#endif
// Used when doing some tests
#ifndef WAVE_FORMAT_TEMP
#define WAVE_FORMAT_TEMP		0x0004
#endif
