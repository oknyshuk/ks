#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "shaderapi/shareddefs.h"
#include "itextureinternal.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;

//-----------------------------------------------------------------------------
// Subdivision surface manager class
//-----------------------------------------------------------------------------
class ISubDMgr
{
public:
	bool ShouldAllocateTextures();			//
	void AllocateTextures();				// Allocate & free textures
	void FreeTextures();					//
	void ReleaseResources();
	void ReacquireResources();

	int GetWidth();
	int GetHeight();
	int GetOffsetGeometry();
	int GetOffsetTangents();

	float* Lock( int nNumRows );
	void Unlock();

	ShaderAPITextureHandle_t SubDTexture();		// Return the subd position texture
};

extern ISubDMgr *g_pSubDMgr;
