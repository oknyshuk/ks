#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;



// This interface is actually exported by the shader API DLL.


class IDebugTextureInfo
{
public:
	
	// Use this to turn on the mode where it builds the debug texture list.
	// At the end of the next frame, GetDebugTextureList() will return a valid list of the textures.
	void EnableDebugTextureList( bool bEnable );
	
	// If this is on, then it will return all textures that exist, not just the ones that were bound in the last frame.
	// It is required to enable debug texture list to get this.
	void EnableGetAllTextures( bool bEnable );

	// Use this to get the results of the texture list.
	// Do NOT release the KeyValues after using them.
	// There will be a bunch of subkeys, each with these values:
	//    Name   - the texture's filename
	//    Binds  - how many times the texture was bound
	//    Format - ImageFormat of the texture
	//    Width  - Width of the texture
	//    Height - Height of the texture
	// It is required to enable debug texture list to get this.
	// You MUST release the texture list after having copied it or whatever so that it can be updated again on a separate thread.
	KeyValues* LockDebugTextureList( void );
	void UnlockDebugTextureList( void );

	// Texture memory usage
	enum TextureMemoryType
	{
		MEMORY_RESERVED_MIN = 0,
		MEMORY_BOUND_LAST_FRAME,		// sums up textures bound last frame
		MEMORY_TOTAL_LOADED,			// total texture memory used
		MEMORY_ESTIMATE_PICMIP_1,		// estimate of running with "picmip 1"
		MEMORY_ESTIMATE_PICMIP_2,		// estimate of running with "picmip 2"
		MEMORY_RESERVED_MAX
	};

	// This returns how much memory was used.
	int GetTextureMemoryUsed( TextureMemoryType eTextureMemory );

	// Use this to determine if texture debug info was computed within last numFramesAllowed frames.
	bool IsDebugTextureListFresh( int numFramesAllowed = 1 );

	// Enable debug texture rendering when texture binds should not count towards textures
	// used during a frame. Returns the old state of debug texture rendering flag to use
	// it for restoring the mode.
	bool SetDebugTextureRendering( bool bEnable );

};
