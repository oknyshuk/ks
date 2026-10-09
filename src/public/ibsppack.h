#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

class IFileSystem;

class IBSPPack
{
public:
	virtual void LoadBSPFile( IFileSystem *pFileSystem, char *filename ) = 0;
	virtual void WriteBSPFile( char *filename ) = 0;
	virtual void ClearPackFile( void ) = 0;
	virtual void AddFileToPack( const char *relativename, const char *fullpath ) = 0;
	virtual void AddBufferToPack( const char *relativename, void *data, int length, bool bTextMode ) = 0;
	virtual void SetHDRMode( bool bHDR ) = 0;

	// populates list of files that bsp owns, i.e. world/cubmap materials, statis props, etc
	virtual bool GetBSPDependants( IFileSystem *pFileSystem, const char *pBSPFilename, std::vector< std::string > *pList ) = 0;

	// This is here to avoid having things like the runtime engine.dll have to statically link to LZMA for the encoder!!!
	virtual void AddBufferToPackAndLZMACompress( const char *relativename, void *data, int length ) = 0;
	virtual void RemoveFileFromPack( char const *relativename ) = 0;
};

// Bumped when the vtable changes. The engine fetches this exact name, so a stale bsppack module is
// rejected by the factory rather than handed over with a vtable that no longer matches.
inline constexpr const char *IBSPPACK_VERSION_STRING = "IBSPPACK004";
