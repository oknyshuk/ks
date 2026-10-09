#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "qlimits.h"

inline constexpr const char *CUSTOM_FILES_FOLDER = "downloads";

// Turns a CRC value into a filename.
class CCustomFilename
{
public:
	CCustomFilename( CRC32_t value ) 
	{
		char hex[16];
		V_binarytohex( (byte *)&value, sizeof( value ), hex, sizeof( hex ) );
		V_snprintf( m_Filename, sizeof( m_Filename ), "%s/%s.dat", CUSTOM_FILES_FOLDER, hex );
	}

	char m_Filename[MAX_OSPATH];
};

// Validate a VTF file.
bool LogoFile_IsValidVTFFile( const void *pData, int len );

// Read in and validate a logo file.
bool LogoFile_ReadFile( CRC32_t crcValue, std::vector< char > &fileData );
