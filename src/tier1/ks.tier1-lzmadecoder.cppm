//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//
//	LZMA SDK 4.43 Copyright (c) 1999-2006 Igor Pavlov (2006-05-01)
// Copyright (c) 2026 oknyshuk

export module ks.tier1.lzmadecoder;

import std;
import ks.platform;
import ks.simd;

export extern "C++" {

inline constexpr auto LZMA_ID = (('A'<<24)|('M'<<16)|('Z'<<8)|('L'));

// bind the buffer for correct identification
#pragma pack(1)
struct lzma_header_t
{
	unsigned int	id;
	unsigned int	actualSize;		// always little endian
	unsigned int	lzmaSize;		// always little endian
	unsigned char	properties[5];
};
#pragma pack()

typedef void ( *LZMAReadProgressCallbackFunc_t )();

class CLZMA
{
public:
	unsigned int	Uncompress( unsigned char *pInput, unsigned char *pOutput, LZMAReadProgressCallbackFunc_t pCallback = nullptr );
	bool			IsCompressed( unsigned char *pInput );
	unsigned int	GetActualSize( unsigned char *pInput );

private:
};

}
