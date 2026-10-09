#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "datamap.h"
import std;

inline constexpr auto MAX_BLOCK_BITS = 13;

#define MAX_BLOCK_SIZE (1<<MAX_BLOCK_BITS )

#define COMPILED_CAPTION_FILEID			MAKEID( 'V', 'C', 'C', 'D' )
inline constexpr auto COMPILED_CAPTION_VERSION = 1;

#pragma pack(1)
struct CompiledCaptionHeader_t
{
	int				magic;
	int				version;
	int				numblocks;
	int				blocksize;
	int				directorysize;
	int				dataoffset;
};

struct CaptionLookup_t
{
	unsigned int	hash;
	int				blockNum;
	unsigned short	offset;
	unsigned short	length;

	void SetHash( char const *string )
	{
		int len = V_strlen( string );
		ks::ScratchArena<> scratch;
		char *tempstr = (char *)scratch.AllocBytes( len + 1 );
		V_strncpy( tempstr, string, len + 1 );
		V_strlower( tempstr );
		CRC32_t temp;
		CRC32_Init( &temp );
		CRC32_ProcessBuffer( &temp, tempstr, len );
		CRC32_Final( &temp );

		hash = ( unsigned int )temp;
	}
};
#pragma pack()

class CCaptionLookupLess
{
public:
	bool	operator()( const CaptionLookup_t& lhs, const CaptionLookup_t& rhs ) const
	{
		return lhs.hash < rhs.hash;
	}
};

struct CaptionBlock_t
{
	byte	data[ MAX_BLOCK_SIZE ];
};
