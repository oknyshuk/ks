#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "bitmap/imageformat.h"
import std;
import ks.tier1.byte_reader;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

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
