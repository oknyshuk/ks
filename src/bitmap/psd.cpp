// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "bitmap/psd.h"
import std;
#include "filesystem.h"
#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "bitmap/bitmap.h"

//-----------------------------------------------------------------------------
// The PSD signature bytes
//-----------------------------------------------------------------------------
constexpr auto PSD_SIGNATURE = 0x38425053;
constexpr auto PSD_IMGRES_SIGNATURE = 0x3842494D;

//-----------------------------------------------------------------------------
// Format of the PSD header on disk
// NOTE: PSD file header, everything is bigendian
//-----------------------------------------------------------------------------
#pragma pack (1)

enum PSDMode_t
{
	MODE_GREYSCALE = 1,
	MODE_PALETTIZED = 2,
	MODE_RGBA = 3,
	MODE_CMYK = 4,
	MODE_MULTICHANNEL = 7,
	MODE_LAB = 9,

	MODE_COUNT = 10,
};

//////////////////////////////////////////////////////////////////////////
//
// BEGIN PSD FILE:
//
//	PSDHeader_t
//  unsigned int	numBytesPalette;
//	byte			palette[ numBytesPalette ]; = { (all red palette entries), (all green palette entries), (all blue palette entries) }, where numEntries = numBytesPalette/3;
//	unsigned int	numBytesImgResources;
//	byte			imgresources[ numBytesImgResources ]; = { sequence of PSDImgResEntry_t }
//	unsigned int	numBytesLayers;
//	byte			layers[ numBytesLayers ];
//	unsigned short	uCompressionInfo;
//	< ~ image data ~ >
//
// END PSD FILE
//
//////////////////////////////////////////////////////////////////////////

struct PSDHeader_t 
{
    unsigned int	m_nSignature;
    unsigned short	m_nVersion;
    unsigned char	m_pReserved[6];
    unsigned short	m_nChannels;
    unsigned int	m_nRows;
    unsigned int	m_nColumns;
    unsigned short	m_nDepth;
    unsigned short	m_nMode;
};

struct PSDPalette_t
{
	unsigned char *m_pRed;
	unsigned char *m_pGreen;
	unsigned char *m_pBlue;
};

//-----------------------------------------------------------------------------
// NOTE: This is how we could load files using file mapping
//-----------------------------------------------------------------------------
//HANDLE File = CreateFile(FileName,GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
//Assert(File != INVALID_HANDLE_VALUE);
//HANDLE FileMap = CreateFileMapping(File,0,PAGE_READONLY,0,0,0);
//Assert(FileMap != INVALID_HANDLE_VALUE);
//void *FileData = MapViewOfFile(FileMap,FILE_MAP_READ,0,0,0);


//-----------------------------------------------------------------------------
// Is it a PSD file?
//-----------------------------------------------------------------------------
bool IsPSDFile( ks::byte_reader &buf )
{
	const std::size_t nGet = buf.tell();
	PSDHeader_t header{};
	buf.read_bytes( &header, sizeof(header) );
	buf.seek( nGet );

	if ( DWordSwapC( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( WordSwapC( header.m_nVersion ) != 1 )
		return false;
	return ( WordSwapC( header.m_nDepth ) == 8 );
}

bool IsPSDFile( const char *pFileName, const char *pPathID )
{
	auto data = g_pFullFileSystem->ReadFile( pFileName, pPathID, sizeof(PSDHeader_t) );
	if ( !data )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	ks::byte_reader buf( *data );
	return IsPSDFile( buf );
}


//-----------------------------------------------------------------------------
// Returns information about the PSD file
//-----------------------------------------------------------------------------
bool PSDGetInfo( ks::byte_reader &buf, int *pWidth, int *pHeight, ImageFormat *pImageFormat, float *pSourceGamma )
{
	const std::size_t nGet = buf.tell();
	PSDHeader_t header{};
	buf.read_bytes( &header, sizeof(header) );
	buf.seek( nGet );

	if ( DWordSwapC( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( WordSwapC( header.m_nVersion ) != 1 )
		return false;
	if ( WordSwapC( header.m_nDepth ) != 8 )
		return false;

	*pWidth = DWordSwapC( header.m_nColumns );
	*pHeight = DWordSwapC( header.m_nRows );
	*pImageFormat = WordSwapC( header.m_nChannels ) == 3 ? IMAGE_FORMAT_RGB888 : IMAGE_FORMAT_RGBA8888;
	*pSourceGamma = ARTWORK_GAMMA;

	return true;
}

bool PSDGetInfo( const char *pFileName, const char *pPathID, int *pWidth, int *pHeight, ImageFormat *pImageFormat, float *pSourceGamma )
{
	auto data = g_pFullFileSystem->ReadFile( pFileName, pPathID, sizeof(PSDHeader_t) );
	if ( !data )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	ks::byte_reader buf( *data );
	return PSDGetInfo( buf, pWidth, pHeight, pImageFormat, pSourceGamma );
}

//-----------------------------------------------------------------------------
// Get PSD file image resources
//-----------------------------------------------------------------------------
PSDImageResources PSDGetImageResources( ks::byte_reader &buf )
{
	const std::size_t nGet = buf.tell();

	// Header
	PSDHeader_t header{};
	buf.read_bytes( &header, sizeof( header ) );

	// Then palette
	unsigned int numBytesPalette = DWordSwapC( buf.read<unsigned int>() );
	buf.skip( numBytesPalette );

	// Then image resources
	unsigned int numBytesImgResources = DWordSwapC( buf.read<unsigned int>() );
	PSDImageResources imgres( numBytesImgResources, reinterpret_cast< const unsigned char * >( buf.rest().data() ) );

	// Restore the seek
	buf.seek( nGet );

	return imgres;
}

//-----------------------------------------------------------------------------
// Converts from CMYK to RGB
//-----------------------------------------------------------------------------
static inline void CMYKToRGB( RGBA8888_t &color )
{
	unsigned char nCyan = 255 - color.r;
	unsigned char nMagenta = 255 - color.g;
	unsigned char nYellow = 255 - color.b;
	unsigned char nBlack = 255 - color.a;

	int nCyanBlack		= (int)nCyan + (int)nBlack;
	int nMagentaBlack	= (int)nMagenta + (int)nBlack;
	int nYellowBlack	= (int)nYellow + (int)nBlack;
	color.r = ( nCyanBlack < 255 ) ? 255 - nCyanBlack : 0;
	color.g = ( nMagentaBlack < 255 ) ? 255 - nMagentaBlack : 0;
	color.b = ( nYellowBlack < 255 ) ? 255 - nYellowBlack : 0;
	color.a = 255;
}


//-----------------------------------------------------------------------------
// Deals with uncompressed channels
//-----------------------------------------------------------------------------
static void PSDConvertToRGBA8888( int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	bool bShouldFillInAlpha = false;
	unsigned char *pDest = bitmap.GetBits();

	switch( mode )
	{
	case MODE_RGBA:
		bShouldFillInAlpha = ( nChannelsCount == 3 );
		break;

	case MODE_PALETTIZED:
		{
			// Convert from palette
			bShouldFillInAlpha = ( nChannelsCount == 1 );
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					unsigned char nPaletteIndex = pDest[0];
					pDest[0] = palette.m_pRed[nPaletteIndex];
					pDest[1] = palette.m_pGreen[nPaletteIndex];
					pDest[2] = palette.m_pBlue[nPaletteIndex];
				}
			}
		}
		break;

	case MODE_GREYSCALE:
		{
			// Monochrome
			bShouldFillInAlpha = ( nChannelsCount == 1 );
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					pDest[1] = pDest[0];
					pDest[2] = pDest[0];
				}
			}
		}
		break;

	case MODE_CMYK:
		{
			// NOTE: The conversion will fill in alpha by default
			bShouldFillInAlpha = false;
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					CMYKToRGB( *((RGBA8888_t*)pDest) );
				}
			}
		}
		break;
	}

	if ( bShouldFillInAlpha )
	{
		// No alpha channel, fill in white
		unsigned char *pDest = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
			{
				pDest[3] = 0xFF;
			}
		}
	}
}

	
//-----------------------------------------------------------------------------
// Deals with uncompressed channels
//-----------------------------------------------------------------------------
static int s_pChannelIndex[MODE_COUNT+1][4] = 
{ 
	{ -1, -1, -1, -1 },
	{ 0, 3, -1, -1 },		// MODE_GREYSCALE
	{ 0, 3, -1, -1 },		// MODE_PALETTIZED
	{ 0, 1, 2, 3 },			// MODE_RGBA
	{ 0, 1, 2, 3 },			// MODE_CMYK
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },		// MODE_MULTICHANNEL
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },		// MODE_LAB
	{ 3, -1, -1, -1 },		// Secret second pass mode for CMYK
};


static void PSDReadUncompressedChannels( ks::byte_reader &buf, int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	ks::ScratchArena<> scratch;
	unsigned char *pChannelRow = (unsigned char*)scratch.AllocBytes( bitmap.Width() );
	for ( int i=0; i<nChannelsCount; ++i )
	{
		int nIndex = s_pChannelIndex[mode][i];
		contract_assert( nIndex != -1 );

		unsigned char *pDest = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			buf.read_bytes( pChannelRow, bitmap.Width() );

			// Collate the channels together
			for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
			{
				pDest[nIndex] = pChannelRow[k];
			}
		}
	}

	PSDConvertToRGBA8888( nChannelsCount, mode, palette, bitmap );
}


//-----------------------------------------------------------------------------
// Deals with compressed channels
//-----------------------------------------------------------------------------
static void PSDReadCompressedChannels( ks::byte_reader &buf, int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	ks::ScratchArena<> scratch;
	unsigned char *pChannelRow = (unsigned char*)scratch.AllocBytes( bitmap.Width() );
	for ( int i=0; i<nChannelsCount; ++i )
	{
		int nIndex = s_pChannelIndex[mode][i];
		contract_assert( nIndex != -1 );

		unsigned char *pDest = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			unsigned char *pSrc = pChannelRow;
			unsigned int nPixelsRemaining = bitmap.Width();
			while ( nPixelsRemaining > 0 )
			{
				int nCount = buf.read<char>();
				if ( nCount >= 0 )
				{
					// If nCount is between 0 + 7F, it means copy the next nCount+1 bytes directly
					++nCount;
					contract_assert( (unsigned int)nCount <= nPixelsRemaining );
					buf.read_bytes( pSrc, nCount );
				}
				else
				{
					// If nCount is between 80 and FF, it means replicate the next byte -Count+1 times
					nCount = -nCount + 1;
					contract_assert( (unsigned int)nCount <= nPixelsRemaining );
					unsigned char nPattern = buf.read<unsigned char>();
					std::memset( pSrc, nPattern, nCount );
				}
				pSrc += nCount;
				nPixelsRemaining -= nCount;
			}
			contract_assert( nPixelsRemaining == 0 );

			// Collate the channels together
			for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
			{
				pDest[nIndex] = pChannelRow[k];
			}
		}
	}

	PSDConvertToRGBA8888( nChannelsCount, mode, palette, bitmap );
}


//-----------------------------------------------------------------------------
// Reads the PSD file into the specified buffer
//-----------------------------------------------------------------------------
bool PSDReadFileRGBA8888( ks::byte_reader &buf, Bitmap_t &bitmap )
{
	ks::ScratchArena<> scratch;
	PSDHeader_t header{};
	buf.read_bytes( &header, sizeof(header) );

	if ( DWordSwapC( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( WordSwapC( header.m_nVersion ) != 1 )
		return false;
	if ( WordSwapC( header.m_nDepth ) != 8 )
		return false;

	PSDMode_t mode = (PSDMode_t)WordSwapC( header.m_nMode );
	int nChannelsCount = WordSwapC( header.m_nChannels );

	if ( mode == MODE_MULTICHANNEL || mode == MODE_LAB )
		return false;

	switch ( mode )
	{
	case MODE_RGBA:
		if ( nChannelsCount < 3 )
			return false;
		break;

	case MODE_GREYSCALE:
	case MODE_PALETTIZED:
		if ( nChannelsCount != 1 && nChannelsCount != 2 )
			return false;
		break;

	case MODE_CMYK:
		if ( nChannelsCount < 4 )
			return false;
		break;

	default:
		Warning( "Unsupported PSD color mode!\n" );
		return false;
	}

	int nWidth = DWordSwapC( header.m_nColumns );
	int nHeight = DWordSwapC( header.m_nRows );

	// Skip parts of memory we don't care about
	int nColorModeSize = DWordSwapC( buf.read<unsigned int>() );
	contract_assert( nColorModeSize % 3 == 0 );
	unsigned char *pPaletteBits = (unsigned char*)scratch.AllocBytes( nColorModeSize );
	PSDPalette_t palette;
	palette.m_pRed = palette.m_pGreen = palette.m_pBlue = 0;
	if ( nColorModeSize )
	{
		int nPaletteSize = nColorModeSize / 3;
		buf.read_bytes( pPaletteBits, nColorModeSize );
		palette.m_pRed = pPaletteBits;
		palette.m_pGreen = palette.m_pRed + nPaletteSize;
		palette.m_pBlue = palette.m_pGreen + nPaletteSize;
	}
 	int nImageResourcesSize = DWordSwapC( buf.read<unsigned int>() );
	buf.skip( nImageResourcesSize );
	int nLayersSize = DWordSwapC( buf.read<unsigned int>() );
	buf.skip( nLayersSize );

	unsigned short nCompressionType = WordSwapC( buf.read<short>() );

	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA8888 );

	bool bSecondPassCMYKA = ( nChannelsCount > 4 && mode == MODE_CMYK );
	if ( nCompressionType == 0 )
	{
		PSDReadUncompressedChannels( buf, ( nChannelsCount > 4 ) ? 4 : nChannelsCount, mode, palette, bitmap );
	}
	else
	{
		// Skip the data that indicates the length of each compressed row in bytes
		// NOTE: There are two bytes per row per channel
		unsigned int nLineLengthData = sizeof(unsigned short) * bitmap.Height() * nChannelsCount;
		buf.skip( nLineLengthData );
		PSDReadCompressedChannels( buf, ( nChannelsCount > 4 ) ? 4 : nChannelsCount, mode, palette, bitmap );
	}

	// Read the alpha in a second pass for CMYKA
	if ( bSecondPassCMYKA )
	{
		if ( nCompressionType == 0 )
		{
			PSDReadUncompressedChannels( buf, 1, MODE_COUNT, palette, bitmap );
		}
		else
		{
			PSDReadCompressedChannels( buf, 1, MODE_COUNT, palette, bitmap );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the heightfield from a file
//-----------------------------------------------------------------------------
bool PSDReadFileRGBA8888( const char *pFileName, const char *pPathID, Bitmap_t &bitmap )
{
	auto data = g_pFullFileSystem->ReadFile( pFileName, pPathID );
	if ( !data )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	ks::byte_reader buf( *data );
	return PSDReadFileRGBA8888( buf, bitmap );
}


//////////////////////////////////////////////////////////////////////////
//
// PSD Helper structs implementation
//
//////////////////////////////////////////////////////////////////////////

PSDImageResources::ResElement PSDImageResources::FindElement( Resource eType ) const
{
	ResElement res;
	std::memset( &res, 0, sizeof( res ) );

	unsigned char const *pvBuffer = m_pvBuffer, * const pvBufferEnd = m_pvBuffer + m_numBytes;
	while ( pvBuffer < pvBufferEnd )
	{
		// 4 : signature
		// 2 : type
		// 4 : reserved
		// 2 : length
		// bytes[ length ]

		uint32 uSignature = DWordSwapC( *( uint32* )( pvBuffer ) );
		pvBuffer += 4;
		if ( uSignature != PSD_IMGRES_SIGNATURE )
			break;

		unsigned short uType = WordSwapC( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 6;

		unsigned short uLength = WordSwapC( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		if ( uType == eType )
		{
			res.m_eType = eType;
			res.m_numBytes = uLength;
			res.m_pvData = pvBuffer;
			break;
		}
		else
		{
			pvBuffer += ( ( uLength + 1 ) &~1 );
		}
	}

	return res;
}

PSDResFileInfo::ResFileInfoElement PSDResFileInfo::FindElement( ResFileInfo eType ) const
{
	ResFileInfoElement res;
	std::memset( &res, 0, sizeof( res ) );

	unsigned char const *pvBuffer = m_res.m_pvData, * const pvBufferEnd = pvBuffer + m_res.m_numBytes;
	while ( pvBuffer < pvBufferEnd )
	{
		// 2 : = 0x1C02
		// 1 : type
		// 2 : length
		// bytes[ length ]
		
		unsigned short uResLabel = WordSwapC( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		unsigned char uType = *pvBuffer;
		pvBuffer += 1;

		unsigned short uLength = WordSwapC( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		if ( uType == eType && uResLabel == 0x1C02 )
		{
			res.m_eType = eType;
			res.m_numBytes = uLength;
			res.m_pvData = pvBuffer;
			break;
		}
		else
		{
			pvBuffer += uLength;
		}
	}

	return res;
}
