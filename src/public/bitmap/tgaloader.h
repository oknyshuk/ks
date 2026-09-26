//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef TGALOADER_H
#define TGALOADER_H


#include "bitmap/imageformat.h"
#include <vector>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace ks { class byte_reader; }


namespace TGALoader
{

int TGAHeaderSize();

bool GetInfo( const char *fileName, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma );
bool GetInfo( ks::byte_reader &buf, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma );

bool Load( unsigned char *imageData, const char *fileName, int width, int height, 
		   ImageFormat imageFormat, float targetGamma, bool mipmap );
bool Load( unsigned char *imageData, ks::byte_reader &buf, int width, int height, 
			ImageFormat imageFormat, float targetGamma, bool mipmap );

bool LoadRGBA8888( const char *pFileName, std::vector<unsigned char> &outputData, int &outWidth, int &outHeight );
bool LoadRGBA8888( ks::byte_reader &buf, std::vector<unsigned char> &outputData, int &outWidth, int &outHeight );

} // end namespace TGALoader

#endif // TGALOADER_H
