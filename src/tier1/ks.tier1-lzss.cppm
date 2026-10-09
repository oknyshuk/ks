// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:lzss;

import std;
import ks.math;

export extern "C++" {

inline constexpr int LZSS_ID = ( 'S' << 24 ) | ( 'S' << 16 ) | ( 'Z' << 8 ) | 'L';

// bind the buffer for correct identification
struct lzss_header_t
{
	unsigned int	id;
	unsigned int	actualSize;	// always little endian
};

inline constexpr auto DEFAULT_LZSS_WINDOW_SIZE = 4096;

class CLZSS
{
public:
	unsigned char*	Compress( unsigned char *pInput, int inputlen, unsigned int *pOutputSize );
	unsigned char*	CompressNoAlloc( unsigned char *pInput, int inputlen, unsigned char *pOutput, unsigned int *pOutputSize );
	unsigned int	Uncompress( unsigned char *pInput, unsigned char *pOutput );
	unsigned int	SafeUncompress( unsigned char *pInput, unsigned char *pOutput, unsigned int unBufSize );
	bool			IsCompressed( unsigned char *pInput );
	unsigned int	GetActualSize( unsigned char *pInput );

	// windowsize must be a power of two.
	[[gnu::always_inline]] inline CLZSS( int nWindowSize = DEFAULT_LZSS_WINDOW_SIZE );

private:
	// expected to be sixteen bytes
	struct lzss_node_t
	{
		unsigned char	*pData;
		lzss_node_t		*pPrev;
		lzss_node_t		*pNext;
		char			empty[4];
	};

	struct lzss_list_t
	{
		lzss_node_t *pStart;
		lzss_node_t *pEnd;
	};

	void			BuildHash( unsigned char *pData );
	lzss_list_t		*m_pHashTable;	
	lzss_node_t		*m_pHashTarget;
	int             m_nWindowSize;

};

[[gnu::always_inline]] inline CLZSS::CLZSS( int nWindowSize )
{
	m_nWindowSize = nWindowSize;
}

}
