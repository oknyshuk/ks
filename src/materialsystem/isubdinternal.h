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
	virtual bool ShouldAllocateTextures() = 0;			//
	virtual void AllocateTextures() = 0;				// Allocate & free textures
	virtual void FreeTextures() = 0;					//
	virtual void ReleaseResources() = 0;
	virtual void ReacquireResources() = 0;

	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual int GetOffsetGeometry() = 0;
	virtual int GetOffsetTangents() = 0;

	virtual float* Lock( int nNumRows ) = 0;
	virtual void Unlock() = 0;

	virtual ShaderAPITextureHandle_t SubDTexture() = 0;		// Return the subd position texture
};

extern ISubDMgr *g_pSubDMgr;
