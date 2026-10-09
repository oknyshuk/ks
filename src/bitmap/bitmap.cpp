// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "bitmap/bitmap.h"
#include "bitmap/psd.h"
#include "bitmap/tgaloader.h"

import std;
bool Bitmap_t::IsValid() const
{
	if ( m_nWidth <= 0 || m_nHeight <= 0 || m_pBits == nullptr )
	{
		contract_assert( m_nWidth == 0 );
		contract_assert( m_nHeight == 0 );
		contract_assert( m_pBits == nullptr );
		return false;
	}
	return true;
}

void Bitmap_t::Clear()
{
	if ( m_pBits && m_bOwnsBuffer )
	{
		g_pMemAlloc->Free( m_pBits );
	}
	Reset();
}

void Bitmap_t::Init( int xs, int ys, ImageFormat imageFormat, int nStride )
{

	// Check for bogus allocation sizes
	if (xs <= 0 || ys <= 0 )
	{
		contract_assert( xs == 0 );
		contract_assert( ys == 0 );
		Clear();
		return;
	}

	int nPixSize = ImageLoader::SizeInBytes( imageFormat );

	// Auto detect stride
	if ( nStride == 0 )
	{
		nStride = nPixSize * xs;
	}

	// Check for NOP
	if (
		m_pBits
		&& m_bOwnsBuffer
		&& m_nWidth == xs
		&& m_nHeight == ys
		&& nStride == m_nStride
		&& nPixSize == m_nPixelSize )
	{
		// We're already got a buffer of the right size.
		// The only thing that might be wrong is the pixel format.
		m_ImageFormat = imageFormat;
		return;
	}

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = xs;
	m_nHeight = ys;
	m_ImageFormat = imageFormat;
	m_nPixelSize = nPixSize;
	m_nStride = nStride;

	// Allocate buffer.  Because this is a PC game,
	// failure is impossible....right?
	m_pBits = (byte *)MemAlloc_Alloc( ys * m_nStride );

	// Assume ownership
	m_bOwnsBuffer = true;
}

void Bitmap_t::SetBuffer( int nWidth, int nHeight, ImageFormat imageFormat, unsigned char *pBits, bool bAssumeOwnership, int nStride )
{
	contract_assert( pBits );
	contract_assert( nWidth > 0 );
	contract_assert( nHeight > 0 );

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_ImageFormat = imageFormat;
	m_nPixelSize = ImageLoader::SizeInBytes( imageFormat );
	if ( nStride == 0 )
	{
		m_nStride = m_nPixelSize * nWidth;
	}
	else
	{
		m_nStride = nStride;
	}

	// Set our buffer pointer
	m_pBits = pBits;

	// Assume ownership of the buffer, if requested
	m_bOwnsBuffer = bAssumeOwnership;

	// We should be good to go
	contract_assert( IsValid() );
}

Color Bitmap_t::GetColor( int x, int y ) const
{
	contract_assert( x >= 0 && x < m_nWidth );
	contract_assert( y >= 0 && y < m_nHeight );
	contract_assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			return Color( ptr[0], ptr[1], ptr[2], ptr[3] );

		case IMAGE_FORMAT_ABGR8888:
			return Color( ptr[3], ptr[2], ptr[1], ptr[0] );

		default:
			contract_assert( !"Unsupport image format!" );
			return Color( 255,0,255,255 );
	}
}

void Bitmap_t::SetColor( int x, int y, Color c )
{
	contract_assert( x >= 0 && x < m_nWidth );
	contract_assert( y >= 0 && y < m_nHeight );
	contract_assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			ptr[0] = c.r();
			ptr[1] = c.g();
			ptr[2] = c.b();
			ptr[3] = c.a();
			break;

		case IMAGE_FORMAT_ABGR8888:
			ptr[0] = c.a();
			ptr[1] = c.b();
			ptr[2] = c.g();
			ptr[3] = c.r();
			break;

		default:
			contract_assert( !"Unsupport image format!" );
			break;
	}
}

//bool LoadVTF( const char *pszFilename )
//{
//
//	// Load the raw file data
//	auto fileData = filesystem->ReadFile( pszFilename, "game" );
//	if ( !fileData )
//	{
//		Warning( "Failed to load %s\n", pszFilename);
//		return false;
//	}
//
//	ks::byte_reader reader( *fileData );
//	return LoadVTFFromBuffer( reader, pszFilename );
//}
//
//bool LoadVTFFromBuffer( ks::byte_reader &fileData, const char *pszDebugName = "buffer" )
//{
//
//	// Parse it into VTF object
//	IVTFTexture *pVTFTexture( CreateVTFTexture() );
//	if ( !pVTFTexture->Unserialize( fileData ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		Warning( "Failed to deserialize VTF %s\n", pszDebugName);
//		return false;
//	}
//
//	// We are re-reading our own files, so they should be 8888's
//	if ( pVTFTexture->Format() != IMAGE_FORMAT_RGBA8888 )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		Warning( "%s isn't RGBA8888\n", pszDebugName);
//		return false;
//	}
//
//	// Copy the image data
//	Allocate( pVTFTexture->Width(), pVTFTexture->Height() );
//	for ( int y = 0 ; y < m_nHeight ; ++y )
//	{
//		memcpy( PixPtr(0, y), pVTFTexture->ImageData(0, 0, 0, 0, y), m_nWidth*4 );
//	}
//
//	// Clean up
//	DestroyVTFTexture( pVTFTexture );
//	return true;
//}
//
//bool SaveVTF( ks::byte_writer &outBuffer )
//{
//	// Create the VTF to write into
//	IVTFTexture *pVTFTexture( CreateVTFTexture() );
//	const int nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SRGB;
//	if ( !pVTFTexture->Init( m_nWidth, m_nHeight, 1, IMAGE_FORMAT_RGBA8888, nFlags, 1, 1 ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		return false;
//	}
//
//	// write the rgba image to the vtf texture using the pixel writer
//	CPixelWriter pixelWriter;		
//	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );
//
//	for (int y = 0; y < m_nHeight; ++y)
//	{
//		pixelWriter.Seek( 0, y );
//		for (int x = 0; x < m_nWidth; ++x)
//		{
//			Color c = GetPix( x, y );
//			pixelWriter.WritePixel( c.r(), c.g(), c.b(), c.a() );
//		}
//	}
//
//	// Serialize to the buffer
//	if ( !pVTFTexture->Serialize( outBuffer ) )
//	{
//		DestroyVTFTexture( pVTFTexture );
//		return false;
//	}
//	DestroyVTFTexture( pVTFTexture );
//	return true;
//}

//void Resize( int nNewSizeX, int nNewSizeY, const Image *pImgSrc = NULL )
//{
//	if ( pImgSrc == NULL )
//	{
//		pImgSrc = this;
//	}
//
//	if ( nNewSizeX == m_nWidth && nNewSizeY == m_nHeight && pImgSrc == this )
//	{
//		return;
//	}
//
//	byte *pNewData = (byte *)MemAlloc_Alloc( nNewSizeX * nNewSizeY * 4 );
//	ImgUtl_StretchRGBAImage( pImgSrc->m_pBits, pImgSrc->m_nWidth, pImgSrc->m_nHeight, pNewData, nNewSizeX, nNewSizeY );
//	Clear();
//	m_pBits = pNewData;
//	m_nWidth = nNewSizeX;
//	m_nHeight = nNewSizeY;
//}
//
//void Crop( int x0, int y0, int nNewSizeX, int nNewSizeY, const Image *pImgSrc )
//{
//	if ( pImgSrc == NULL )
//	{
//		pImgSrc = this;
//	}
//
//	if ( nNewSizeX == m_nWidth && nNewSizeY == m_nHeight && pImgSrc == this )
//	{
//		return;
//	}
//
//
//	Assert( x0 >= 0 );
//	Assert( y0 >= 0 );
//	Assert( x0 + nNewSizeX <= pImgSrc->m_nWidth );
//	Assert( y0 + nNewSizeY <= pImgSrc->m_nHeight );
//
//	// Allocate new buffer
//	int nRowSize = nNewSizeX * 4;
//	byte *pNewData = (byte *)MemAlloc_Alloc( nNewSizeY * nRowSize );
//
//	// Copy data, one row at a time
//	for ( int y = 0 ; y < nNewSizeY ; ++y )
//	{
//		memcpy( pNewData + y*nRowSize, pImgSrc->PixPtr(x0, y0+y), nRowSize );
//	}
//
//	// Replace current buffer with the new one
//	Clear();
//	m_pBits = pNewData;
//	m_nWidth = nNewSizeX;
//	m_nHeight = nNewSizeY;
//}

void Bitmap_t::MakeLogicalCopyOf( Bitmap_t &src, bool bTransferBufferOwnership )
{
	// What does it mean to make a logical copy of an
	// invalid bitmap?  I'll tell you what it means: you have a bug.
	contract_assert( src.IsValid() );

	// Free up anything we already own
	Clear();

	// Copy all of the member variables so we are
	// a logical copy of the source bitmap
	m_nWidth = src.m_nWidth;
	m_nHeight = src.m_nHeight;
	m_nPixelSize = src.m_nPixelSize;
	m_nStride = src.m_nStride;
	m_ImageFormat = src.m_ImageFormat;
	m_pBits = src.m_pBits;
	contract_assert( !m_bOwnsBuffer );

	// Check for assuming ownership of the buffer
	if ( bTransferBufferOwnership )
	{
		if ( src.m_bOwnsBuffer )
		{
			m_bOwnsBuffer = true;
			src.m_bOwnsBuffer = false;
		}
		else
		{
			// They don't own the buffer?  Then who does?
			// Maybe nobody, and it would safe to assume
			// ownership.  But more than likely, this is a
			// bug.
			contract_assert( src.m_bOwnsBuffer );

			// And a leak is better than a double-free.
			// Don't assume ownership of the buffer.
		}
	}
}

void Bitmap_t::Crop( int x0, int y0, int nWidth, int nHeight, const Bitmap_t *pImgSource )
{
	// Check for cropping in place, then save off our data to a temp
	Bitmap_t temp;
	if ( pImgSource == this || !pImgSource )
	{
		temp.MakeLogicalCopyOf( *this, m_bOwnsBuffer );
		pImgSource = &temp;
	}

	// No source image?
	if ( !pImgSource->IsValid() )
	{
		contract_assert( pImgSource->IsValid() );
		return;
	}

	// Sanity check crop rectangle
	contract_assert( x0 >= 0 );
	contract_assert( y0 >= 0 );
	contract_assert( x0 + nWidth <= pImgSource->Width() );
	contract_assert( y0 + nHeight <= pImgSource->Height() );

	// Allocate buffer
	Init( nWidth, nHeight, pImgSource->Format() );

	// Something wrong?
	if ( !IsValid() )
	{
		contract_assert( IsValid() );
		return;
	}

	// Copy the data a row at a time
	int nRowSize = m_nWidth * m_nPixelSize;
	for ( int y = 0 ; y < m_nHeight ; ++y )
	{
		std::memcpy( GetPixel(y,0), pImgSource->GetPixel( x0, y + y0 ), nRowSize );
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nSrcX1, int nSrcY1, int nCopySizeX, int nCopySizeY, int nDestX1, int nDestY1 )
{
	// Safety
	if ( !src.IsValid() )
	{
		contract_assert( src.IsValid() );
		return;
	}
	if ( !IsValid() )
	{
		contract_assert( IsValid() );
		return;
	}

	// You need to specify a valid source rectangle, we cannot clip that for you
	if ( nSrcX1 < 0 || nSrcY1 < 0 || nSrcX1 + nCopySizeX > src.Width() || nSrcY1 + nCopySizeY > src.Height() )
	{
		contract_assert( nSrcX1 >= 0 );
		contract_assert( nSrcY1 >= 0 );
		contract_assert( nSrcX1 + nCopySizeX <= src.Width() );
		contract_assert( nSrcY1 + nCopySizeY <= src.Height() );
		return;
	}

	// But we can clip the rectangle if it extends outside the destination image in a perfectly
	// reasonable way
	if ( nDestX1 < 0 )
	{
		nCopySizeX += nDestX1;
		nDestX1 = 0;
	}
	if ( nDestX1 + nCopySizeX > Width() )
	{
		nCopySizeX = Width() - nDestX1;
	}
	if ( nDestY1 < 0 )
	{
		nCopySizeY += nDestY1;
		nDestY1 = 0;
	}
	if ( nDestY1 + nCopySizeY > Height() )
	{
		nCopySizeY = Height() - nDestY1;
	}
	if ( nCopySizeX <= 0 || nCopySizeY <= 0 )
	{
		return;
	}

	// Copy the pixel data
	for ( int y = 0 ; y < nCopySizeY ; ++y )
	{
		// Wow, this could be a lot faster in the common case
		// that the pixe formats are the same.  But...this code
		// is simple and works, and is NOT the root of all evil.
		for ( int x = 0 ; x < nCopySizeX ; ++x )
		{
			Color c = src.GetColor( nSrcX1 + x, nSrcY1 + y );
			SetColor( nDestX1 + x, nDestY1 + y, c );
		}
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nDestX1, int nDestY1 )
{
	SetPixelData( src, 0, 0, src.Width(), src.Height(), nDestX1, nDestY1 );
}

//-----------------------------------------------------------------------------
// Returns true if it's a PFM file
//-----------------------------------------------------------------------------
bool IsPFMFile( ks::byte_reader &buf )
{
	const std::size_t nGet = buf.tell();
	char c0 = buf.read<char>();
	char c1 = buf.read<char>();
	char c2 = buf.read<char>();
	buf.seek( nGet );

	return ( c0 == 'P' && ( c1 == 'F' || c1 == 'f' ) && c2 == 0xA );
}


//-----------------------------------------------------------------------------
// This reads an integer from a binary byte_reader.
//-----------------------------------------------------------------------------
static int ReadIntFromByteReader( ks::byte_reader &buf )
{
	int val = 0;
	int c;
	while( buf.ok() )
	{
		c = buf.read<char>();
		if( c >= '0' && c <= '9' )
		{
			val = val * 10 + ( c - '0' );
		}
		else
		{
			buf.seek( buf.tell() - 1 );
			break;
		}
	}
	return val;
}

static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t' || c == 10;
}

static void EatWhiteSpace( ks::byte_reader &buf )
{
	while( buf.ok() )
	{
		int	c = buf.read<char>();
		if( !IsWhitespace( c ) )
		{
			buf.seek( buf.tell() - 1 );
			return;
		}
	}
	return;
}


//-----------------------------------------------------------------------------
// Reads PFM info + advances to the texture bits
//-----------------------------------------------------------------------------
bool PFMGetInfo_AndAdvanceToTextureBits( ks::byte_reader &pfmBuffer, int &nWidth, int &nHeight, ImageFormat &imageFormat )
{
	pfmBuffer.seek( 0 );
	if( pfmBuffer.read<char>() != 'P' )
	{
		contract_assert( 0 );
		return false;
	}
	char c = pfmBuffer.read<char>();
	if ( c == 'F' )
	{
		imageFormat = IMAGE_FORMAT_RGB323232F;
	}
	else if ( c == 'f' )
	{
		imageFormat = IMAGE_FORMAT_R32F;
	}
	else
	{
		contract_assert( 0 );
		return false;
	}

	if( pfmBuffer.read<char>() != 0xa )
	{
		contract_assert( 0 );
		return false;
	}
	nWidth = ReadIntFromByteReader( pfmBuffer );
	EatWhiteSpace( pfmBuffer );
	nHeight = ReadIntFromByteReader( pfmBuffer );

	// eat crap until the next newline
	while( pfmBuffer.ok() && pfmBuffer.read<char>() != 0xa )
	{
	}

	int nScale = ReadIntFromByteReader( pfmBuffer );
	( void )nScale;

	// eat crap until the next newline
	while( pfmBuffer.ok() && pfmBuffer.read<char>() != 0xa )
	{
	}

	// Here, the buffer should be at the start of the texture data
	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t as RGBA32323232F data
// PFM source data doesn't have alpha, so we put 1.0f into alpha
//-----------------------------------------------------------------------------
bool PFMReadFileRGBA32323232F( ks::byte_reader &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	int nWidth = 0, nHeight = 0;
	ImageFormat imageFormat = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, imageFormat );		// Read the header (advances us to the texture bits)
	contract_assert( imageFormat == IMAGE_FORMAT_RGB323232F );

	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA32323232F );	// Init the bitmap

	// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
	//       Reading rows in reverse order here is the correct thing to do:
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		contract_assert( fileBuffer.ok() );
		if ( !fileBuffer.ok() )
			return false;

		float *pOut = ( (float *)bitmap.GetBits() ) + y*nWidth*4;	// Point to output row

		for ( int x = 0; x < nWidth; x++ )						// For every RGB input color
		{
			fileBuffer.read_bytes( pOut, sizeof( float ) );			// Get red
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			fileBuffer.read_bytes( pOut, sizeof( float ) );			// Get green
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			fileBuffer.read_bytes( pOut, sizeof( float ) );			// Get blue
			*pOut *= pfmScale;									// Scale into output
			pOut++;
			*pOut = 1.0f;										// 1.0f into alpha
			pOut++;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t as RGB323232F data
//-----------------------------------------------------------------------------
bool PFMReadFileRGB323232F( ks::byte_reader &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	// Read the header (advances us to the texture bits)
	int nWidth = 0, nHeight = 0;
	ImageFormat imageFormat = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, imageFormat );
	contract_assert( imageFormat == IMAGE_FORMAT_RGB323232F );

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGB323232F );

	// Read the texels
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		contract_assert( fileBuffer.ok() );
		if ( !fileBuffer.ok() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		float *pRow = ( (float *)bitmap.GetBits() ) + y*nWidth*3;

		fileBuffer.read_bytes( pRow, nWidth*3*sizeof( float ) );
		for ( int x = 0; x < nWidth*3; x++ )
		{
			pRow[ x ] *= pfmScale;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the x channel of a PFM file into a Bitmap_t as R32F data
//-----------------------------------------------------------------------------
bool PFMReadFileR32F( ks::byte_reader &fileBuffer, Bitmap_t &bitmap, float pfmScale )
{
	// Read the header (advances us to the texture bits)
	int nWidth, nHeight;
	ImageFormat fileImageFormat; // Format of data in file
	PFMGetInfo_AndAdvanceToTextureBits( fileBuffer, nWidth, nHeight, fileImageFormat );
	contract_assert( fileImageFormat == IMAGE_FORMAT_RGB323232F );

	// Init the bitmap
	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_R32F );

	float flMin = std::numeric_limits<float>::max();
	float flMax = std::numeric_limits<float>::min();

	// Read the texels, snarfing out just the x component
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		contract_assert( fileBuffer.ok() );
		if ( !fileBuffer.ok() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		float *pRow = ( (float *)bitmap.GetBits() ) + y*nWidth;

		for ( int x = 0; x < nWidth; x++ )
		{
			fileBuffer.read_bytes( pRow+x, sizeof( float ) );		// Grab x component and scale
			pRow[x] *= pfmScale;

			flMin = std::min( pRow[x], flMin );
			flMax = std::max( pRow[x], flMax );

			float flDummy[2];
			fileBuffer.read_bytes( &flDummy, 2*sizeof( float ) );	// Jump past y and z components in file
		}
	}

	std::printf( "Displacement Range: (%g, %g)\n", flMin, flMax );

	return true;
}


//-----------------------------------------------------------------------------
// Loads a PFM file into a Bitmap_t
//-----------------------------------------------------------------------------
bool PFMReadFile( ks::byte_reader &buf, Bitmap_t *pBitmap )
{
	// Read the header (advances us to the texture bits)
	int nWidth = 0, nHeight = 0;
	ImageFormat fmt = IMAGE_FORMAT_UNKNOWN;
	PFMGetInfo_AndAdvanceToTextureBits( buf, nWidth, nHeight, fmt );

	// Init the bitmap
	pBitmap->Init( nWidth, nHeight, fmt );

	int nRowBytes = ImageLoader::GetMemRequired( nWidth, 1, 1, 1, fmt );

	// Read the texels
	for( int y = ( nHeight - 1 ); y >= 0; y-- )
	{
		contract_assert( buf.ok() );
		if ( !buf.ok() )
			return false;

		// NOTE: PFMs are displayed *UPSIDE-DOWN* in Photoshop! (as of CS2... HDRShop gets it right)
		//       Reading rows in reverse order here is the correct thing to do:
		void *pDest = pBitmap->GetBits() + y * nRowBytes;
		buf.read_bytes( pDest, nRowBytes );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads a bitmap from an arbitrary file: could be a TGA, PSD, or PFM.
// LoadBitmap autodetects which type
//-----------------------------------------------------------------------------
BitmapFileType_t LoadBitmapFile( ks::byte_reader &buf, Bitmap_t *pBitmap )
{
	if ( IsPSDFile( buf ) )
	{
		// FIXME: Make it actually load the true format
		if ( !PSDReadFileRGBA8888( buf, *pBitmap ) )
			return BITMAP_FILE_TYPE_UNKNOWN;
		return BITMAP_FILE_TYPE_PSD;
	}

	if ( IsPFMFile( buf ) )
	{
		if ( !PFMReadFile( buf, pBitmap ) )
			return BITMAP_FILE_TYPE_UNKNOWN;
		return BITMAP_FILE_TYPE_PFM;
	}

	// It's not really possible to detect TGAness.. there's no identifier
	// Assume TGA file here
	int nWidth, nHeight;
	ImageFormat fmt;
	float flGamma;
	const std::size_t nGet = buf.tell();
	bool bOk = TGALoader::GetInfo( buf, &nWidth, &nHeight, &fmt, &flGamma );
	buf.seek( nGet );
	if ( !bOk )
		return BITMAP_FILE_TYPE_UNKNOWN;

	// FIXME: TGALoader is incredibly inefficient when trying to just get
	// the bits as-is as it loads into RGBA8888 and then color converts out
	pBitmap->Init( nWidth, nHeight, fmt );
	if ( !TGALoader::Load( pBitmap->GetBits(), buf, nWidth, nHeight, fmt, flGamma, false ) )
		return BITMAP_FILE_TYPE_UNKNOWN;

	return BITMAP_FILE_TYPE_TGA;
}
