// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: NOTE: To make use of this file, g_pFullFileSystem must be defined, or you can modify
// this source to take an IFileSystem * as input.

// @note Tom Bui: we need to use fopen below in the jpeg code, so we can't have this on...

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.jpeg;
#include "const.h"

#include "imageutils.h"
#include "filesystem.h"
#include "bitmap/bitmap.h"
#include "vtf/vtf.h"

// clang3 on OSX folks the attribute into the prototype, causing a compile failure
// filed radar bug 10397783


#ifdef ENGINE_DLL
	#include "common.h"
#elif CLIENT_DLL
	// @note Tom Bui: instead of forcing the project to include EngineInterface.h...
	#include "cdll_int.h"
	// engine interface singleton accessors
	extern IVEngineClient *engine;
	extern class IGameUIFuncs *gameuifuncs;
	extern class IEngineSound *enginesound;
	extern class IMatchmaking *matchmaking;
	extern class IAchievementMgr *achievementmgr; 
#elif ENGINE_DLL
	#include "engineinterface.h"
#elif defined(CLIENT_DLL) || defined(GAME_DLL)
	#include "cdll_int.h"
	extern IVEngineClient *engine;
#endif


// clang3 on OSX folds the attribute into the prototype, causing a compile failure
// filed radar bug 10397783

#include "bitmap/tgawriter.h"
#include "ivtex.h"




namespace
{

void AppendToWriter( const void *pBytes, unsigned long nSize, void *pContext )
{
	static_cast< ks::byte_writer * >( pContext )->write_bytes( pBytes, nSize );
}

bool LoadDiskFile( const char *pPath, std::vector< std::byte > &contents )
{
	ks::platform::file *pFile = ks::platform::open_file( pPath, ks::platform::file_mode::read, nullptr );
	if ( pFile == nullptr )
		return false;

	contents.resize( ks::platform::file_size( pFile ) );
	const long nRead = ks::platform::read_file( pFile, contents.data(), contents.size(), nullptr );
	ks::platform::close_file( pFile );
	return nRead == static_cast< long >( contents.size() );
}

ConversionErrorType DecodeJPEG( const void *pData, size_t nSize, int nChannels, ks::byte_writer &dst, int &width, int &height )
{
	if ( !ks::jpeg::read_header( pData, nSize, &width, &height ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		return CE_ERROR_PARSING_SOURCE;
	}

	const size_t nBytes = static_cast< size_t >( width ) * height * nChannels;
	std::span< std::byte > pixels = dst.claim_span( nBytes );
	if ( pixels.size() != nBytes )
		return CE_MEMORY_ERROR;

	if ( !ks::jpeg::decode( pData, nSize, nChannels, reinterpret_cast< unsigned char * >( pixels.data() ), nBytes ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		dst.clear();
		return CE_ERROR_PARSING_SOURCE;
	}
	return CE_SUCCESS;
}

bool WriteBufferToFile( const char *pFilename, const ks::byte_writer &buffer )
{
	FileHandle_t fh = g_pFullFileSystem->Open( pFilename, "wb", "LOCAL" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return false;

	g_pFullFileSystem->Write( buffer.data(), buffer.tell(), fh );
	g_pFullFileSystem->Close( fh );
	return true;
}

} // namespace

// convert the JPEG file given to a TGA file at the given output path.
ConversionErrorType ImgUtl_ConvertJPEGToTGA( const char *jpegpath, const char *tgaPath, bool bRequirePowerOfTwo )
{
	std::vector< std::byte > file;
	if ( !LoadDiskFile( jpegpath, file ) )
		return CE_CANT_OPEN_SOURCE_FILE;

	int image_width = 0, image_height = 0;
	if ( !ks::jpeg::read_header( file.data(), file.size(), &image_width, &image_height ) )
		return CE_ERROR_PARSING_SOURCE;

	// Check for valid width and height (ie. power of 2 and print out an error and exit if not).
	if ( bRequirePowerOfTwo && ( !IsPowerOfTwo( image_height ) || !IsPowerOfTwo( image_width ) ) )
		return CE_SOURCE_FILE_SIZE_NOT_SUPPORTED;

	std::vector< unsigned char > pixels( static_cast< size_t >( image_width ) * image_height * 3 );
	if ( !ks::jpeg::decode( file.data(), file.size(), 3, pixels.data(), pixels.size() ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		return CE_ERROR_PARSING_SOURCE;
	}

	// ok, at this point we have read in the JPEG image to our buffer, now we need to write it out as a TGA file.
	ks::byte_writer outBuf;
	bool bRetVal = TGAWriter::WriteToBuffer( pixels.data(), outBuf, image_width, image_height, IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888 );
	if ( bRetVal && !g_pFullFileSystem->WriteFile( tgaPath, nullptr, outBuf.bytes() ) )
		bRetVal = false;

	return bRetVal ? CE_SUCCESS : CE_ERROR_WRITING_OUTPUT_FILE;
}

// convert the bmp file given to a TGA file at the given destination path.
ConversionErrorType ImgUtl_ConvertBMPToTGA(const char *bmpPath, const char *tgaPath)
{

	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
}

unsigned char *ImgUtl_ReadVTFAsRGBA( const char *vtfPath, int &width, int &height, ConversionErrorType &errcode )
{
	// Just load the whole file into a memory buffer
	auto fileContents = g_pFullFileSystem->ReadFile( vtfPath, nullptr );
	if ( !fileContents )
	{
		errcode = CE_CANT_OPEN_SOURCE_FILE;
		return nullptr;
	}

	ks::byte_reader bufFileContents( *fileContents );
	IVTFTexture *pVTFTexture = CreateVTFTexture();
	if ( !pVTFTexture->Unserialize( bufFileContents ) )
	{
		DestroyVTFTexture( pVTFTexture );
		errcode = CE_ERROR_PARSING_SOURCE;
		return nullptr;
	}

	width = pVTFTexture->Width();
	height = pVTFTexture->Height();
	pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );

	int nMemSize = ImageLoader::GetMemRequired( width, height, 1, IMAGE_FORMAT_RGBA8888, false );
	unsigned char *pMemImage = (unsigned char *)MemAlloc_Alloc(nMemSize);
	if ( pMemImage == nullptr )
	{
		DestroyVTFTexture( pVTFTexture );
		errcode = CE_MEMORY_ERROR;
		return nullptr;
	}
	V_memcpy( pMemImage, pVTFTexture->ImageData(), nMemSize );

	DestroyVTFTexture( pVTFTexture );

	errcode = CE_SUCCESS;
	return pMemImage;
}

// read a TGA header from the current point in the file stream.
static void ImgUtl_ReadTGAHeader(std::FILE *infile, TGAHeader &header)
{
	if (infile == nullptr)
	{
		return;
	}

	fread(&header.identsize, sizeof(header.identsize), 1, infile);
	fread(&header.colourmaptype, sizeof(header.colourmaptype), 1, infile);
	fread(&header.imagetype, sizeof(header.imagetype), 1, infile);
	fread(&header.colourmapstart, sizeof(header.colourmapstart), 1, infile);
	fread(&header.colourmaplength, sizeof(header.colourmaplength), 1, infile);
	fread(&header.colourmapbits, sizeof(header.colourmapbits), 1, infile);
	fread(&header.xstart, sizeof(header.xstart), 1, infile);
	fread(&header.ystart, sizeof(header.ystart), 1, infile);
	fread(&header.width, sizeof(header.width), 1, infile);
	fread(&header.height, sizeof(header.height), 1, infile);
	fread(&header.bits, sizeof(header.bits), 1, infile);
	fread(&header.descriptor, sizeof(header.descriptor), 1, infile);
}

// write a TGA header to the current point in the file stream.
static void WriteTGAHeader(std::FILE *outfile, TGAHeader &header)
{
	if (outfile == nullptr)
	{
		return;
	}

	fwrite(&header.identsize, sizeof(header.identsize), 1, outfile);
	fwrite(&header.colourmaptype, sizeof(header.colourmaptype), 1, outfile);
	fwrite(&header.imagetype, sizeof(header.imagetype), 1, outfile);
	fwrite(&header.colourmapstart, sizeof(header.colourmapstart), 1, outfile);
	fwrite(&header.colourmaplength, sizeof(header.colourmaplength), 1, outfile);
	fwrite(&header.colourmapbits, sizeof(header.colourmapbits), 1, outfile);
	fwrite(&header.xstart, sizeof(header.xstart), 1, outfile);
	fwrite(&header.ystart, sizeof(header.ystart), 1, outfile);
	fwrite(&header.width, sizeof(header.width), 1, outfile);
	fwrite(&header.height, sizeof(header.height), 1, outfile);
	fwrite(&header.bits, sizeof(header.bits), 1, outfile);
	fwrite(&header.descriptor, sizeof(header.descriptor), 1, outfile);
}

// reads in a TGA file and converts it to 32 bit RGBA color values in a memory buffer.
unsigned char * ImgUtl_ReadTGAAsRGBA(const char *tgaPath, int &width, int &height, ConversionErrorType &errcode, TGAHeader &tgaHeader )
{
	std::FILE *tgaFile = fopen(tgaPath, "rb");
	if (tgaFile == nullptr)
	{
		errcode = CE_CANT_OPEN_SOURCE_FILE;
		return nullptr;
	}

	// read header for TGA file.
	ImgUtl_ReadTGAHeader(tgaFile, tgaHeader);

	if (
		( tgaHeader.imagetype != 2 ) // image type 2 is uncompressed RGB, other types not supported.
		|| ( tgaHeader.descriptor & 0x10 ) // Origin on righthand side (flipped horizontally from common sense) --- nobody ever uses this
		|| ( tgaHeader.bits != 24 && tgaHeader.bits != 32 ) // Must be 24- ot 32-bit
	)
	{
		fclose(tgaFile);

		errcode = CE_SOURCE_FILE_TGA_FORMAT_NOT_SUPPORTED;
		return nullptr;
	}

	int tgaDataSize = tgaHeader.width * tgaHeader.height * tgaHeader.bits / 8;
	unsigned char *tgaData = (unsigned char *)MemAlloc_Alloc(tgaDataSize);
	if (tgaData == nullptr)
	{
		fclose(tgaFile);

		errcode = CE_MEMORY_ERROR;
		return nullptr;
	}

	fread(tgaData, 1, tgaDataSize, tgaFile);

	fclose(tgaFile);

	width = tgaHeader.width;
	height = tgaHeader.height;
	int numPixels = tgaHeader.width * tgaHeader.height;

	if (tgaHeader.bits == 24)
	{
		// image needs to be converted to a 32-bit image.

		unsigned char *retBuf = (unsigned char *)MemAlloc_Alloc(numPixels * 4);
		if (retBuf == nullptr)
		{
			g_pMemAlloc->Free(tgaData);

			errcode = CE_MEMORY_ERROR;
			return nullptr;
		}

		// convert from BGR to RGBA color format.
		for (int index = 0; index < numPixels; ++index)
		{
			retBuf[index * 4] = tgaData[index * 3 + 2];
			retBuf[index * 4 + 1] = tgaData[index * 3 + 1];
			retBuf[index * 4 + 2] = tgaData[index * 3];
			retBuf[index * 4 + 3] = 0xff;
		}

		g_pMemAlloc->Free(tgaData);
		tgaData = retBuf;
		tgaHeader.bits = 32;
	}
	else if (tgaHeader.bits == 32)
	{
		// Swap blue and red to convert BGR -> RGB
		for (int index = 0; index < numPixels; ++index)
		{
			V_swap( tgaData[index*4], tgaData[index*4 + 2] );
		}
	}

	// Flip image vertically if necessary
	if ( !( tgaHeader.descriptor & 0x20 ) )
	{
		int y0 = 0;
		int y1 = height-1;
		int iStride = width*4;
		while ( y0 < y1 )
		{
			unsigned char *ptr0 = tgaData + y0*iStride;
			unsigned char *ptr1 = tgaData + y1*iStride;
			for ( int i = 0 ; i < iStride ; ++i )
			{
				V_swap( ptr0[i], ptr1[i] );
			}
			++y0;
			--y1;
		}
		tgaHeader.descriptor |= 0x20;
	}

	errcode = CE_SUCCESS;
	return tgaData;
}

unsigned char *ImgUtl_ReadJPEGAsRGBA( const char *jpegPath, int &width, int &height, ConversionErrorType &errcode )
{
	std::vector< std::byte > file;
	if ( !LoadDiskFile( jpegPath, file ) )
	{
		errcode = CE_CANT_OPEN_SOURCE_FILE;
		return nullptr;
	}

	if ( !ks::jpeg::read_header( file.data(), file.size(), &width, &height ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		errcode = CE_ERROR_PARSING_SOURCE;
		return nullptr;
	}

	const size_t nBytes = static_cast< size_t >( width ) * height * 4;
	unsigned char *buf = (unsigned char *)MemAlloc_Alloc( nBytes );
	if ( buf == nullptr )
	{
		errcode = CE_MEMORY_ERROR;
		return nullptr;
	}

	if ( !ks::jpeg::decode( file.data(), file.size(), 4, buf, nBytes ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		g_pMemAlloc->Free( buf );
		errcode = CE_ERROR_PARSING_SOURCE;
		return nullptr;
	}

	errcode = CE_SUCCESS;
	return buf;
}

ConversionErrorType ImgUtl_ReadJPEGAsRGBA( ks::byte_reader &srcBuf, ks::byte_writer &dstBuf, int &width, int &height )
{
	return DecodeJPEG( srcBuf.bytes().data(), srcBuf.size(), 4, dstBuf, width, height );
}

unsigned char *ImgUtl_ReadPNGAsRGBA( const char *pngPath, int &width, int &height, ConversionErrorType &errcode )
{

	// Just load the whole file into a memory buffer
	auto fileContents = g_pFullFileSystem->ReadFile( pngPath, nullptr );
	if ( !fileContents )
	{
		errcode = CE_CANT_OPEN_SOURCE_FILE;
		return nullptr;
	}

	// Load it
	ks::byte_reader bufFileContents( *fileContents );
	return ImgUtl_ReadPNGAsRGBAFromBuffer( bufFileContents, width, height, errcode );

}

unsigned char		*ImgUtl_ReadPNGAsRGBAFromBuffer( ks::byte_reader &buffer, int &width, int &height, ConversionErrorType &errcode )
{
	errcode = CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	return nullptr;
}

unsigned char *ImgUtl_ReadBMPAsRGBA( const char *bmpPath, int &width, int &height, ConversionErrorType &errcode )
{
	errcode = CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	return nullptr;
}

unsigned char *ImgUtl_ReadImageAsRGBA( const char *path, int &width, int &height, ConversionErrorType &errcode )
{

	// Split out the file extension
	const char *pExt = V_GetFileExtension( path );
	if ( pExt )
	{
		if ( !V_stricmp(pExt, "vtf") )
		{
			return ImgUtl_ReadVTFAsRGBA( path, width, height, errcode );
		}
		if ( !V_stricmp(pExt, "bmp") )
		{
			return ImgUtl_ReadBMPAsRGBA( path, width, height, errcode );
		}
		if ( !V_stricmp(pExt, "jpg") || !V_stricmp(pExt, "jpeg") )
		{
			return ImgUtl_ReadJPEGAsRGBA( path, width, height, errcode );
		}
		if ( !V_stricmp(pExt, "png") )
		{
			return ImgUtl_ReadPNGAsRGBA( path, width, height, errcode );
		}
		if ( !V_stricmp(pExt, "tga") )
		{
			TGAHeader header;
			return ImgUtl_ReadTGAAsRGBA( path, width, height, errcode, header );
		}
	}

	errcode = CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	return nullptr;
}

// resizes the file specified by tgaPath so that it has dimensions that are
// powers-of-two and is equal to or smaller than (nMaxWidth)x(nMaxHeight).
// also converts from 24-bit RGB to 32-bit RGB (with 8-bit alpha)
ConversionErrorType ImgUtl_ConvertTGA(const char *tgaPath, int nMaxWidth/*=-1*/, int nMaxHeight/*=-1*/)
{
	int tgaWidth = 0, tgaHeight = 0;
	ConversionErrorType errcode;
	TGAHeader tgaHeader;
	unsigned char *srcBuffer = ImgUtl_ReadTGAAsRGBA(tgaPath, tgaWidth, tgaHeight, errcode, tgaHeader);

	if (srcBuffer == nullptr)
	{
		return errcode;
	}

	int paddedImageWidth, paddedImageHeight;

	if ((tgaWidth <= 0) || (tgaHeight <= 0))
	{
		g_pMemAlloc->Free(srcBuffer);
		return CE_ERROR_PARSING_SOURCE;
	}

	// get the nearest power of two that is greater than the width of the image.
	paddedImageWidth = tgaWidth;
	if (!IsPowerOfTwo(paddedImageWidth))
	{
		// width is not a power of two, calculate the next highest power of two value.
		int i = 1;
		while (paddedImageWidth > 1)
		{
			paddedImageWidth = paddedImageWidth >> 1;
			++i;
		}

		paddedImageWidth = paddedImageWidth << i;
	}

	// make sure the width is less than or equal to nMaxWidth
	if (nMaxWidth != -1 && paddedImageWidth > nMaxWidth)
	{
		paddedImageWidth = nMaxWidth;
	}

	// get the nearest power of two that is greater than the height of the image
	paddedImageHeight = tgaHeight;
	if (!IsPowerOfTwo(paddedImageHeight))
	{
		// height is not a power of two, calculate the next highest power of two value.
		int i = 1;
		while (paddedImageHeight > 1)
		{
			paddedImageHeight = paddedImageHeight >> 1;
			++i;
		}

		paddedImageHeight = paddedImageHeight << i;
	}

	// make sure the height is less than or equal to nMaxHeight
	if (nMaxHeight != -1 && paddedImageHeight > nMaxHeight)
	{
		paddedImageHeight = nMaxHeight;
	}

	// compute the amount of stretching that needs to be done to both width and height to get the image to fit.
	float widthRatio = (float)paddedImageWidth / tgaWidth;
	float heightRatio = (float)paddedImageHeight / tgaHeight;

	int finalWidth;
	int finalHeight;

	// compute the final dimensions of the stretched image.
	if (widthRatio < heightRatio)
	{
		finalWidth = paddedImageWidth;
		finalHeight = (int)(tgaHeight * widthRatio + 0.5f);
		// i.e.  for 1x1 size pixels in the resized image we will take color from sourceRatio x sourceRatio sized pixels in the source image.
	}
	else if (heightRatio < widthRatio)
	{
		finalHeight = paddedImageHeight;
		finalWidth = (int)(tgaWidth * heightRatio + 0.5f);
	}
	else
	{
		finalHeight = paddedImageHeight;
		finalWidth = paddedImageWidth;
	}

	unsigned char *resizeBuffer = (unsigned char *)MemAlloc_Alloc(finalWidth * finalHeight * 4);

	// do the actual stretching
	ImgUtl_StretchRGBAImage(srcBuffer, tgaWidth, tgaHeight, resizeBuffer, finalWidth, finalHeight);

	g_pMemAlloc->Free(srcBuffer);  // don't need this anymore.

	///////////////////////////////////////////////////////////////////////
	///// need to pad the image so both dimensions are power of two's /////
	///////////////////////////////////////////////////////////////////////
	unsigned char *finalBuffer = (unsigned char *)MemAlloc_Alloc(paddedImageWidth * paddedImageHeight * 4);
	ImgUtl_PadRGBAImage(resizeBuffer, finalWidth, finalHeight, finalBuffer, paddedImageWidth, paddedImageHeight);

	std::FILE *outfile = fopen(tgaPath, "wb");
	if (outfile == nullptr)
	{
		g_pMemAlloc->Free(resizeBuffer);
		g_pMemAlloc->Free(finalBuffer);

		return CE_ERROR_WRITING_OUTPUT_FILE;
	}

	tgaHeader.width = paddedImageWidth;
	tgaHeader.height = paddedImageHeight;

	WriteTGAHeader(outfile, tgaHeader);

	// Write the image data --- remember that TGA uses BGRA data
	int numPixels = paddedImageWidth * paddedImageHeight;
	for (int i = 0 ; i < numPixels ; ++i )
	{
		fputc( finalBuffer[i*4 + 2], outfile ); // B
		fputc( finalBuffer[i*4 + 1], outfile ); // G
		fputc( finalBuffer[i*4    ], outfile ); // R
		fputc( finalBuffer[i*4 + 3], outfile ); // A
	}

	fclose(outfile);

	g_pMemAlloc->Free(resizeBuffer);
	g_pMemAlloc->Free(finalBuffer);

	return CE_SUCCESS;
}

// resize by stretching (or compressing) an RGBA image pointed to by srcBuf into the buffer pointed to by destBuf.
// the buffers are assumed to be sized appropriately to accomidate RGBA images of the given widths and heights.
ConversionErrorType ImgUtl_StretchRGBAImage(const unsigned char *srcBuf, const int srcWidth, const int srcHeight,
									 unsigned char *destBuf, const int destWidth, const int destHeight)
{
	if ((srcBuf == nullptr) || (destBuf == nullptr))
	{
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	int destRow,destColumn;

	float ratioX = (float)srcWidth / (float)destWidth;
	float ratioY = (float)srcHeight / (float)destHeight;

	// loop through all the pixels in the destination image.
	for (destRow = 0; destRow < destHeight; ++destRow)
	{
		for (destColumn = 0; destColumn < destWidth; ++destColumn)
		{
			// calculate the center of the pixel in the source image.
			float srcCenterX = ratioX * (destColumn + 0.5f);
			float srcCenterY = ratioY * (destRow + 0.5f);

			// calculate the starting and ending coords for this destination pixel in the source image.
			float srcStartX = srcCenterX - (ratioX / 2.0f);
			if (srcStartX < 0.0f)
			{
				srcStartX = 0.0f; // this should never happen, but just in case.
			}

			float srcStartY = srcCenterY - (ratioY / 2.0f);
			if (srcStartY < 0.0f)
			{
				srcStartY = 0.0f; // this should never happen, but just in case.
			}

			float srcEndX = srcCenterX + (ratioX / 2.0f);
			if (srcEndX > srcWidth)
			{
				srcEndX = srcWidth; // this should never happen, but just in case.
			}

			float srcEndY = srcCenterY + (ratioY / 2.0f);
			if (srcEndY > srcHeight)
			{
				srcEndY = srcHeight; // this should never happen, but just in case.
			}

			// Calculate the percentage of each source pixels' contribution to the destination pixel color.

			float srcCurrentX; // initialized at the start of the y loop.
			float srcCurrentY = srcStartY;

			float destRed = 0.0f;
			float destGreen = 0.0f;
			float destBlue = 0.0f;
			float destAlpha = 0.0f;

			//// loop for the parts of the source image that will contribute color to the destination pixel.
			while (srcCurrentY < srcEndY)
			{
				float srcCurrentEndY = (float)((int)srcCurrentY + 1);
				if (srcCurrentEndY > srcEndY)
				{
					srcCurrentEndY = srcEndY;
				}

				float srcCurrentHeight = srcCurrentEndY - srcCurrentY;

				srcCurrentX = srcStartX;

				while (srcCurrentX < srcEndX)
				{
					float srcCurrentEndX = (float)((int)srcCurrentX + 1);
					if (srcCurrentEndX > srcEndX)
					{
						srcCurrentEndX = srcEndX;
					}
					float srcCurrentWidth = srcCurrentEndX - srcCurrentX;

					// compute the percentage of the destination pixel's color this source pixel will contribute.
					float srcColorPercentage = (srcCurrentWidth / ratioX) * (srcCurrentHeight / ratioY);

					int srcCurrentPixelX = (int)srcCurrentX;
					int srcCurrentPixelY = (int)srcCurrentY;

					// get the color values for this source pixel.
					unsigned char srcCurrentRed = srcBuf[(srcCurrentPixelY * srcWidth * 4) + (srcCurrentPixelX * 4)];
					unsigned char srcCurrentGreen = srcBuf[(srcCurrentPixelY * srcWidth * 4) + (srcCurrentPixelX * 4) + 1];
					unsigned char srcCurrentBlue = srcBuf[(srcCurrentPixelY * srcWidth * 4) + (srcCurrentPixelX * 4) + 2];
					unsigned char srcCurrentAlpha = srcBuf[(srcCurrentPixelY * srcWidth * 4) + (srcCurrentPixelX * 4) + 3];

					// add the color contribution from this source pixel to the destination pixel.
					destRed += srcCurrentRed * srcColorPercentage;
					destGreen += srcCurrentGreen * srcColorPercentage;
					destBlue += srcCurrentBlue * srcColorPercentage;
					destAlpha += srcCurrentAlpha * srcColorPercentage;

					srcCurrentX = srcCurrentEndX;
				}

				srcCurrentY = srcCurrentEndY;
			}

			// assign the computed color to the destination pixel, round to the nearest value.  Make sure the value doesn't exceed 255.
			destBuf[(destRow * destWidth * 4) + (destColumn * 4)] = std::min((int)(destRed + 0.5f), 255);
			destBuf[(destRow * destWidth * 4) + (destColumn * 4) + 1] = std::min((int)(destGreen + 0.5f), 255);
			destBuf[(destRow * destWidth * 4) + (destColumn * 4) + 2] = std::min((int)(destBlue + 0.5f), 255);
			destBuf[(destRow * destWidth * 4) + (destColumn * 4) + 3] = std::min((int)(destAlpha + 0.5f), 255);
		} // column loop
	} // row loop

	return CE_SUCCESS;
}

ConversionErrorType ImgUtl_PadRGBAImage(const unsigned char *srcBuf, const int srcWidth, const int srcHeight,
								 unsigned char *destBuf, const int destWidth, const int destHeight)
{
	if ((srcBuf == nullptr) || (destBuf == nullptr))
	{
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	memset(destBuf, 0, destWidth * destHeight * 4);

	if ((destWidth < srcWidth) || (destHeight < srcHeight))
	{
		return CE_ERROR_PARSING_SOURCE;
	}

	if ((srcWidth == destWidth) && (srcHeight == destHeight))
	{
		// no padding is needed, just copy the buffer straight over and call it done.
		memcpy(destBuf, srcBuf, destWidth * destHeight * 4);
		return CE_SUCCESS;
	}

	if (destWidth == srcWidth)
	{
		// only the top and bottom of the image need padding.
		// do this separately since we can do this more efficiently than the other cases.
		int numRowsToPad = (destHeight - srcHeight) / 2;
		memcpy(destBuf + (numRowsToPad * destWidth * 4), srcBuf, srcWidth * srcHeight * 4);
	}
	else
	{
		int numColumnsToPad = (destWidth - srcWidth) / 2;
		int numRowsToPad = (destHeight - srcHeight) / 2;
		int lastRow = numRowsToPad + srcHeight;
		int row;
		for (row = numRowsToPad; row < lastRow; ++row)
		{
			unsigned char * destOffset = destBuf + (row * destWidth * 4) + (numColumnsToPad * 4);
			const unsigned char * srcOffset = srcBuf + ((row - numRowsToPad) * srcWidth * 4);
			memcpy(destOffset, srcOffset, srcWidth * 4);
		}
	}

	return CE_SUCCESS;
}

// convert TGA file at the given location to a VTF file of the same root name at the same location.
ConversionErrorType ImgUtl_ConvertTGAToVTF(const char *tgaPath, int nMaxWidth/*=-1*/, int nMaxHeight/*=-1*/ )
{
	std::FILE *infile = fopen(tgaPath, "rb");
	if (infile == nullptr)
	{
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// read out the header of the image.
	TGAHeader header;
	ImgUtl_ReadTGAHeader(infile, header);

	// check to make sure that the TGA has the proper dimensions and size.
	if (!IsPowerOfTwo(header.width) || !IsPowerOfTwo(header.height))
	{
		fclose(infile);
		return CE_SOURCE_FILE_SIZE_NOT_SUPPORTED;
	}

	// check to make sure that the TGA isn't too big, if we care.
	if ( ( nMaxWidth != -1 && header.width > nMaxWidth ) || ( nMaxHeight != -1 && header.height > nMaxHeight ) )
	{
		fclose(infile);
		return CE_SOURCE_FILE_SIZE_NOT_SUPPORTED;
	}

	int imageMemoryFootprint = header.width * header.height * header.bits / 8;

	std::vector< std::byte > inbuf( imageMemoryFootprint );

	// read in the image
	fread(inbuf.data(), imageMemoryFootprint, 1, infile);

	fclose(infile);

	// load vtex_dll.dll and get the interface to it.
	CSysModule *vtexmod = Sys_LoadModule("vtex_dll");
	if (vtexmod == nullptr)
	{
		return CE_ERROR_LOADING_DLL;
	}

	CreateInterfaceFn factory = Sys_GetFactory(vtexmod);
	if (factory == nullptr)
	{
		Sys_UnloadModule(vtexmod);
		return CE_ERROR_LOADING_DLL;
	}

	IVTex *vtex = (IVTex *)factory(IVTEX_VERSION_STRING, nullptr);
	if (vtex == nullptr)
	{
		Sys_UnloadModule(vtexmod);
		return CE_ERROR_LOADING_DLL;
	}

	char *vtfParams[4];

	// the 0th entry is skipped cause normally thats the program name.
	vtfParams[0] = "";
	vtfParams[1] = "-quiet";
	vtfParams[2] = "-dontusegamedir";
	vtfParams[3] = (char *)tgaPath;

	// call vtex to do the conversion.
	vtex->VTex(4, vtfParams);

	Sys_UnloadModule(vtexmod);

	return CE_SUCCESS;
}

static void DoCopyFile( const char *source, const char *destination )
{
#if   defined( ENGINE_DLL )
	::COM_CopyFile( source, destination );
#else
	engine->CopyLocalFile( source, destination );
#endif
}

static void DoDeleteFile( const char *filename )
{
	ks::platform::remove_file( filename, nullptr );
}

ConversionErrorType	ImgUtl_ConvertToVTFAndDumpVMT( const char *pInPath, const char *pMaterialsSubDir, int nMaxWidth/*=-1*/, int nMaxHeight/*=-1*/ )
{
	if ((pInPath == nullptr) || (pInPath[0] == 0))
	{
		return CE_ERROR_PARSING_SOURCE;
	}

	ConversionErrorType nErrorCode = CE_SUCCESS;

	// get the extension of the file we're to convert
	char extension[MAX_PATH];
	const char *constchar = pInPath + strlen(pInPath);
	while ((constchar > pInPath) && (*(constchar-1) != '.'))
	{
		--constchar;
	}
	V_strncpy(extension, constchar, MAX_PATH);

	bool deleteIntermediateTGA = false;
	bool deleteIntermediateVTF = false;
	bool convertTGAToVTF = true;
	char tgaPath[MAX_PATH*2];
	char *c;
	bool failed = false;

	V_strncpy(tgaPath, pInPath, sizeof(tgaPath));

	// Construct a TGA version if necessary
	if (stricmp(extension, "tga"))
	{
		c = tgaPath + strlen(tgaPath);
		while ((c > tgaPath) && (*(c-1) != '\\') && (*(c-1) != '/'))
		{
			--c;
		}
		*c = 0;

		char origpath[MAX_PATH*2];
		V_strncpy(origpath, tgaPath, sizeof(origpath));

		int index = 0;
		do {
			V_snprintf(tgaPath, sizeof(tgaPath), "%stemp%d.tga", origpath, index);
			++index;
		} while (ks::platform::path_exists( tgaPath ));

		if (!stricmp(extension, "jpg") || !stricmp(extension, "jpeg"))
		{
			// convert from the jpeg file format to the TGA file format
			nErrorCode = ImgUtl_ConvertJPEGToTGA(pInPath, tgaPath, false);
			if (nErrorCode == CE_SUCCESS)
			{
				deleteIntermediateTGA = true;
			}
			else
			{
				failed = true;
			}
		}
		else if (!stricmp(extension, "bmp"))
		{
			// convert from the bmp file format to the TGA file format
			nErrorCode = ImgUtl_ConvertBMPToTGA(pInPath, tgaPath);

			if (nErrorCode == CE_SUCCESS)
			{
				deleteIntermediateTGA = true;
			}
			else
			{
				failed = true;
			}
		}
		else if (!stricmp(extension, "vtf"))
		{
			// if the file is already in the vtf format there's no need to convert it.
			convertTGAToVTF = false;
		}
	}

	// Convert the TGA file to the VTF format if necessary
	if (convertTGAToVTF && !failed)
	{
		nErrorCode = ImgUtl_ConvertTGA( tgaPath, nMaxWidth, nMaxHeight ); // resize TGA so that it has power-of-two dimensions with a max size of (nMaxWidth)x(nMaxHeight).
		if (nErrorCode != CE_SUCCESS)
		{
			failed = true;
		}

		if (!failed)
		{
			char tempPath[MAX_PATH*2];
			V_strncpy(tempPath, tgaPath, sizeof(tempPath));

			nErrorCode = ImgUtl_ConvertTGAToVTF( tempPath, nMaxWidth, nMaxHeight );
			if (nErrorCode == CE_SUCCESS)
			{
				deleteIntermediateVTF = true;
			}
			else
			{
				failed = true;
			}
		}
	}

	char finalPath[MAX_PATH*2];
	finalPath[0] = 0;
	char vtfPath[MAX_PATH*2];
	vtfPath[0] = 0;

	if (!failed)
	{
		V_strncpy(vtfPath, tgaPath, sizeof(vtfPath));

		// rename the tga file to be a vtf file.
		c = vtfPath + strlen(vtfPath);
		while ((c > vtfPath) && (*(c-1) != '.'))
		{
			--c;
		}
		*c = 0;
		V_strncat(vtfPath, "vtf", sizeof(vtfPath), COPY_ALL_CHARACTERS);

		// get the vtfFilename from the path.
		const char *vtfFilename = pInPath + strlen(pInPath);
		while ((vtfFilename > pInPath) && (*(vtfFilename-1) != '\\') && (*(vtfFilename-1) != '/'))
		{
			--vtfFilename;
		}

		// Create a safe version of pOutDir with corrected slashes
		char szOutDir[MAX_PATH*2];
		V_strcpy_safe( szOutDir, "/materials/" );
		if ( pMaterialsSubDir[0] == '\\' || pMaterialsSubDir[0] == '/' )
			pMaterialsSubDir = pMaterialsSubDir + 1;
		V_strcat(szOutDir, pMaterialsSubDir, sizeof(szOutDir) );
		V_StripTrailingSlash( szOutDir );
		V_AppendSlash( szOutDir, sizeof(szOutDir) );
		V_FixSlashes( szOutDir, CORRECT_PATH_SEPARATOR );

#ifdef ENGINE_DLL
		V_strncpy(finalPath, com_gamedir, sizeof(finalPath));
#elif defined(CLIENT_DLL) || defined(GAME_DLL)
		V_strncpy(finalPath, engine->GetGameDirectory(), sizeof(finalPath));
#endif
		V_strncat(finalPath, szOutDir, sizeof(finalPath), COPY_ALL_CHARACTERS);
		V_strncat(finalPath, vtfFilename, sizeof(finalPath), COPY_ALL_CHARACTERS);

		c = finalPath + strlen(finalPath);
		while ((c > finalPath) && (*(c-1) != '.'))
		{
			--c;
		}
		*c = 0;
		V_strncat(finalPath,"vtf", sizeof(finalPath), COPY_ALL_CHARACTERS);

		// make sure the directory exists before we try to copy the file.
		g_pFullFileSystem->CreateDirHierarchy(szOutDir + 1, "GAME");
		//g_pFullFileSystem->CreateDirHierarchy("materials/VGUI/logos/", "GAME");

		// write out the spray VMT file.
		nErrorCode = ImgUtl_WriteGenericVMT(finalPath, pMaterialsSubDir);
		if (nErrorCode != CE_SUCCESS)
		{
			failed = true;
		}

		if (!failed)
		{
			// copy vtf file to the final location.
			DoCopyFile( vtfPath, finalPath );
		}
	}

	// delete the intermediate VTF file if one was made.
	if (deleteIntermediateVTF)
	{
		DoDeleteFile( vtfPath );
		
		// the TGA->VTF conversion process generates a .txt file if one wasn't already there.
		// in this case, delete the .txt file.
		c = vtfPath + strlen(vtfPath);
		while ((c > vtfPath) && (*(c-1) != '.'))
		{
			--c;
		}
		V_strncpy(c, "txt", sizeof(vtfPath)-(c-vtfPath));

		DoDeleteFile( vtfPath );
	}

	// delete the intermediate TGA file if one was made.
	if (deleteIntermediateTGA)
	{
		DoDeleteFile( tgaPath );
	}

	return nErrorCode;
}

ConversionErrorType ImgUtl_WriteGenericVMT( const char *vtfPath, const char *pMaterialsSubDir )
{
	if (vtfPath == nullptr || pMaterialsSubDir == nullptr )
	{
		return CE_ERROR_WRITING_OUTPUT_FILE;
	}

	// make the vmt filename
	char vmtPath[MAX_PATH*4];
	V_strncpy(vmtPath, vtfPath, sizeof(vmtPath));
	char *c = vmtPath + strlen(vmtPath);
	while ((c > vmtPath) && (*(c-1) != '.'))
	{
		--c;
	}
	V_strncpy(c, "vmt", sizeof(vmtPath) - (c - vmtPath));

	// get the root filename for the vtf file
	char filename[MAX_PATH];
	while ((c > vmtPath) && (*(c-1) != '/') && (*(c-1) != '\\'))
	{
		--c;
	}

	int i = 0;
	while ((*c != 0) && (*c != '.'))
	{
		filename[i++] = *(c++);
	}
	filename[i] = 0;

	// create the vmt file.
	std::FILE *vmtFile = fopen(vmtPath, "w");
	if (vmtFile == nullptr)
	{
		return CE_ERROR_WRITING_OUTPUT_FILE;
	}

	// make a copy of the subdir and remove any trailing slash
	char szMaterialsSubDir[ MAX_PATH*2 ];
	V_strcpy( szMaterialsSubDir, pMaterialsSubDir );
	V_StripTrailingSlash( szMaterialsSubDir );

	// fix slashes
	V_FixSlashes( szMaterialsSubDir );

	// write the contents of the file.
	fprintf(vmtFile, "\"UnlitGeneric\"\n{\n\t\"$basetexture\"	\"%s%c%s\"\n\t\"$translucent\" \"1\"\n\t\"$ignorez\" \"1\"\n\t\"$vertexcolor\" \"1\"\n\t\"$vertexalpha\" \"1\"\n}\n", szMaterialsSubDir, CORRECT_PATH_SEPARATOR, filename);

	fclose(vmtFile);

	return CE_SUCCESS;
}

ConversionErrorType ImgUtl_WriteRGBAAsPNGToBuffer( const unsigned char *pRGBAData, int nWidth, int nHeight, ks::byte_writer &bufOutData, int nStride )
{
	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
}

ConversionErrorType ImgUtl_WriteRGBAAsJPEGToBuffer( const unsigned char *pRGBAData, int nWidth, int nHeight, ks::byte_writer &bufOutData, int nStride )
{
	const int nRowBytes = nStride > 0 ? nStride : nWidth * 4;
	std::vector< unsigned char > rgb( static_cast< size_t >( nWidth ) * nHeight * 3 );
	for ( int y = 0; y < nHeight; ++y )
	{
		const unsigned char *pSrcRow = pRGBAData + static_cast< size_t >( y ) * nRowBytes;
		unsigned char *pDstRow = rgb.data() + static_cast< size_t >( y ) * nWidth * 3;
		for ( int x = 0; x < nWidth; ++x )
		{
			pDstRow[x*3+2] = pSrcRow[x*4+2];
			pDstRow[x*3+1] = pSrcRow[x*4+1];
			pDstRow[x*3] = pSrcRow[x*4];
		}
	}

	if ( !ks::jpeg::encode( rgb.data(), nWidth, nHeight, 100, AppendToWriter, &bufOutData ) )
	{
		Warning( "%s\n", ks::jpeg::last_error() );
		return CE_ERROR_PARSING_SOURCE;
	}
	return CE_SUCCESS;
}

ConversionErrorType ImgUtl_LoadBitmap( const char *pszFilename, Bitmap_t &bitmap )
{
	bitmap.Clear();
	ConversionErrorType nErrorCode;
	int width, height;
	unsigned char *buffer = ImgUtl_ReadImageAsRGBA( pszFilename, width, height, nErrorCode );
	if ( nErrorCode != CE_SUCCESS )
	{
		return nErrorCode;
	}

	// Install the buffer into the bitmap, and transfer ownership
	bitmap.SetBuffer( width, height, IMAGE_FORMAT_RGBA8888, buffer, true, width*4 );
	return CE_SUCCESS;
}

static ConversionErrorType ImgUtl_LoadJPEGBitmapFromBuffer( ks::byte_reader &fileData, Bitmap_t &bitmap )
{
	// @todo implement
	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
}

static ConversionErrorType ImgUtl_SaveJPEGBitmapToBuffer( ks::byte_writer &fileData, const Bitmap_t &bitmap )
{
	if ( !bitmap.IsValid() )
	{
		contract_assert( bitmap.IsValid() );
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// Sorry, only RGBA8888 supported right now
	if ( bitmap.Format() != IMAGE_FORMAT_RGBA8888 )
	{
		contract_assert( bitmap.Format() == IMAGE_FORMAT_RGBA8888 );
		return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	}

	// Do it
	ConversionErrorType result = ImgUtl_WriteRGBAAsJPEGToBuffer(
		bitmap.GetBits(),
		bitmap.Width(),
		bitmap.Height(),
		fileData,
		bitmap.Stride()
		);
	return result;
}

ConversionErrorType ImgUtl_LoadBitmapFromBuffer( ks::byte_reader &fileData, Bitmap_t &bitmap, ImageFileFormat eImageFileFormat )
{
	switch ( eImageFileFormat )
	{
	case kImageFileFormat_PNG:
		return ImgUtl_LoadPNGBitmapFromBuffer( fileData, bitmap );
	case kImageFileFormat_JPG:
		return ImgUtl_LoadJPEGBitmapFromBuffer( fileData, bitmap );
	}
	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
}

ConversionErrorType ImgUtl_SaveBitmapToBuffer( ks::byte_writer &fileData, const Bitmap_t &bitmap, ImageFileFormat eImageFileFormat )
{
	switch ( eImageFileFormat )
	{
	case kImageFileFormat_PNG:
		return ImgUtl_SavePNGBitmapToBuffer( fileData, bitmap );
	case kImageFileFormat_JPG:
		return ImgUtl_SaveJPEGBitmapToBuffer( fileData, bitmap );
	}
	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
}

ConversionErrorType ImgUtl_LoadPNGBitmapFromBuffer( ks::byte_reader &fileData, Bitmap_t &bitmap )
{
	bitmap.Clear();
	ConversionErrorType nErrorCode;
	int width, height;
	unsigned char *buffer = ImgUtl_ReadPNGAsRGBAFromBuffer( fileData, width, height, nErrorCode );
	if ( nErrorCode != CE_SUCCESS )
	{
		return nErrorCode;
	}

	// Install the buffer into the bitmap, and transfer ownership
	bitmap.SetBuffer( width, height, IMAGE_FORMAT_RGBA8888, buffer, true, width*4 );
	return CE_SUCCESS;
}

ConversionErrorType ImgUtl_SavePNGBitmapToBuffer( ks::byte_writer &fileData, const Bitmap_t &bitmap )
{
	if ( !bitmap.IsValid() )
	{
		contract_assert( bitmap.IsValid() );
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// Sorry, only RGBA8888 supported right now
	if ( bitmap.Format() != IMAGE_FORMAT_RGBA8888 )
	{
		contract_assert( bitmap.Format() == IMAGE_FORMAT_RGBA8888 );
		return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	}

	// Do it
	ConversionErrorType result = ImgUtl_WriteRGBAAsPNGToBuffer(
		bitmap.GetBits(),
		bitmap.Width(),
		bitmap.Height(),
		fileData,
		bitmap.Stride()
	);
	return result;
}

ConversionErrorType ImgUtl_ResizeBitmap( Bitmap_t &destBitmap, int nWidth, int nHeight, const Bitmap_t *pImgSource )
{

	// Check for resizing in place, then save off data into a temp
	Bitmap_t temp;
	if ( pImgSource == nullptr || pImgSource == &destBitmap )
	{
		temp.MakeLogicalCopyOf( destBitmap, destBitmap.GetOwnsBuffer() );
		pImgSource = &temp;
	}

	// No source image?
	if ( !pImgSource->IsValid() )
	{
		contract_assert( pImgSource->IsValid() );
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// Sorry, we're using an existing rescaling routine that
	// only withs for RGBA images with assumed stride
	if (
		pImgSource->Format() != IMAGE_FORMAT_RGBA8888
		|| pImgSource->Stride() != pImgSource->Width()*4
	) {
		contract_assert( pImgSource->Format() == IMAGE_FORMAT_RGBA8888 );
		contract_assert( pImgSource->Stride() == pImgSource->Width()*4 );
		return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	}

	// Allocate buffer
	destBitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA8888 );

	// Something wrong?
	if ( !destBitmap.IsValid() )
	{
		contract_assert( destBitmap.IsValid() );
		return CE_MEMORY_ERROR;
	}

	// Do it
	return ImgUtl_StretchRGBAImage(
		pImgSource->GetBits(), pImgSource->Width(), pImgSource->Height(),
		destBitmap.GetBits(), destBitmap.Width(), destBitmap.Height()
	);
}
//-----------------------------------------------------------------------------
// Purpose: Read a JPEG from disk
//-----------------------------------------------------------------------------
ConversionErrorType ImgUtl_ReadJPEGToRGB( ks::byte_reader &srcBuf, ks::byte_writer &dstBuf, int &width, int &height )
{
	return DecodeJPEG( srcBuf.bytes().data(), srcBuf.size(), 3, dstBuf, width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Take our RGBA buffer and crop it down to a proper size with offset
//-----------------------------------------------------------------------------
ConversionErrorType ImgUtl_CropRGBA( int x0, int y0, int nSrcWidth, int nSrcHeight, int nDestWidth, int nDestHeight, const unsigned char *pIn, unsigned char *pOut )
{
	// Allocate new buffer
	const int nRowSize = nDestWidth * 4;

	// Copy data, one row at a time
	for ( int y = 0 ; y < nDestHeight; ++y )
	{
		memcpy( pOut + y*nRowSize, pIn + ( ((y0+y)*nSrcWidth) + x0 ) * 4, nRowSize );
	}

	return CE_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: Write three channel RGB data to a JPEG file
//-----------------------------------------------------------------------------
bool ImgUtl_WriteRGBAToJPEG( unsigned char *pSrcBuf, unsigned int nSrcWidth, unsigned int nSrcHeight, const char *lpszFilename )
{
	ks::byte_writer dstBuf;
	if ( ImgUtl_WriteRGBAAsJPEGToBuffer( pSrcBuf, nSrcWidth, nSrcHeight, dstBuf, 0 ) != CE_SUCCESS )
		return false;

	WriteBufferToFile( lpszFilename, dstBuf );
	return true;
}
