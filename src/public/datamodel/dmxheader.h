#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

//-----------------------------------------------------------------------------
// data file format info
//-----------------------------------------------------------------------------
inline constexpr const char *DMX_LEGACY_VERSION_STARTING_TOKEN = "<!-- DMXVersion";
inline constexpr const char *DMX_LEGACY_VERSION_ENDING_TOKEN = "-->";

inline constexpr const char *DMX_VERSION_STARTING_TOKEN = "<!-- dmx";
inline constexpr const char *DMX_VERSION_ENDING_TOKEN = "-->";

inline constexpr const char *GENERIC_DMX_FORMAT = "dmx";


enum
{
	DMX_MAX_FORMAT_NAME_MAX_LENGTH = 64,
	DMX_MAX_HEADER_LENGTH = 40 + 2 * DMX_MAX_FORMAT_NAME_MAX_LENGTH,
};

struct DmxHeader_t
{
	char encodingName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	int nEncodingVersion;
	char formatName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	int nFormatVersion;

	DmxHeader_t() : nEncodingVersion( -1 ), nFormatVersion( -1 )
	{
		encodingName[ 0 ] = formatName[ 0 ] = '\0';
	}
};

//-----------------------------------------------------------------------------
// file id - also used to refer to elements that don't have file associations
//-----------------------------------------------------------------------------
enum DmFileId_t
{
	DMFILEID_INVALID = 0xffffffff
};
