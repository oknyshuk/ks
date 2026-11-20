// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
#if defined(CLIENT_DLL)
#include "const.h"
#include "string_t.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "datamap.h"
#endif
#include "closedcaptions.h"
#include "filesystem.h"

// Assumed to be set up by calling code
bool AsyncCaption_t::LoadFromFile( char const *pchFullPath )
{
	FileHandle_t fh = g_pFullFileSystem->Open( pchFullPath, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return false;


	// Read the header
	g_pFullFileSystem->Read( &m_Header, sizeof( m_Header ), fh );
	if ( m_Header.magic != COMPILED_CAPTION_FILEID )
	{
			Error( "Invalid file id for %s\n", pchFullPath );
	}
	if ( m_Header.version != COMPILED_CAPTION_VERSION )
	{
			Error( "Invalid file version for %s\n", pchFullPath );
	}
	if ( m_Header.directorysize < 0 || m_Header.directorysize > 64 * 1024 )
	{
			Error( "Invalid directory size %d for %s\n", m_Header.directorysize, pchFullPath );
	}
	//if ( m_Header.blocksize != MAX_BLOCK_SIZE )
	//	Error( "Invalid block size %d, expecting %d for %s\n", m_Header.blocksize, MAX_BLOCK_SIZE, pchFullPath );

	int directoryBytes = m_Header.directorysize * sizeof( CaptionLookup_t );
	std::vector< CaptionLookup_t > directory( m_Header.directorysize );

	g_pFullFileSystem->Read( directory.data(), directoryBytes, fh );
	g_pFullFileSystem->Close( fh );

	m_CaptionDirectory = CaptionDictionary_t( directory.begin(), directory.end() );

	m_DataBaseFile = pchFullPath;
	return true;
}
