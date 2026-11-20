#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
import std;
import ks.core.basetypes;
import ks.core.types;
import ks.platform;
import ks.simd;
#include "demofile/demoformat.h"

//-----------------------------------------------------------------------------
struct DemoBufferInitParams_t
{
	enum class kind_t : unsigned char { stream, memory };
	explicit DemoBufferInitParams_t( kind_t kind ) : m_kind( kind ) {}

	kind_t m_kind;
};

struct StreamDemoBufferInitParams_t : public DemoBufferInitParams_t
{
	StreamDemoBufferInitParams_t( char const* filename, char const* path, bool readOnly, int openfileflags ) : DemoBufferInitParams_t( kind_t::stream ), pFilename( filename ), pszPath( path ), bReadOnly( readOnly ), nOpenFileFlags( openfileflags ) {}
	char const*	pFilename;
	char const*	pszPath;
	bool		bReadOnly;
	int			nOpenFileFlags;
};

struct MemoryDemoBufferInitParams_t : public DemoBufferInitParams_t
{
	MemoryDemoBufferInitParams_t( int maxsize ) : DemoBufferInitParams_t( kind_t::memory ), nMaxSize( maxsize ) {}
	int			nMaxSize;	// TODO: make use of this parameter
};


//-----------------------------------------------------------------------------
// Demo buffer interface - allows for one implementation of the demo format
// (demofile.cpp) with a common interface for writing to disk or writing to
// a buffer in memory.
//-----------------------------------------------------------------------------
class IDemoBuffer
{
public:
	bool				Init( DemoBufferInitParams_t const& params );

	void				NotifySignonComplete();

	bool				IsInitialized() const;
	bool				IsValid() const;

	void				WriteHeader( const void *pData, int nSize );

	void				NotifyBeginFrame();
	void				NotifyEndFrame();

	// Change where I'm writing (put)/reading (get)
	void				SeekPut( bool bAbsolute, int offset );
	void				SeekGet( bool bAbsolute, int offset );

	// Where am I writing (put)/reading (get)?
	int					TellPut( ) const;
	int					TellGet( ) const;

	// What's the most I've ever written?
	int					TellMaxPut( ) const;

	char				GetChar( );
	unsigned char		GetUnsignedChar( );
	int					GetInt( );
	void				Get( void* pMem, int size );

	void				PutChar( char c );
	void				PutUnsignedChar( unsigned char uc );
	void				PutInt( int i );
	void				Put( const void* pMem, int size );

	void				WriteTick( int nTick );

	void				UpdateStartTick( int& nStartTick ) const;
	void				DumpToFile( char const* pFilename, const demoheader_t &header ) const;
};

IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t &params );
void DestroyDemoBuffer( IDemoBuffer *pBuffer );
