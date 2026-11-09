#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: NOTE: This file is for backward compat!
// We'll get rid of it soon. Most of the contents of this file were moved
// into shaderpi/ishadershadow.h, shaderapi/ishaderdynamic.h, or
// shaderapi/shareddefs.h

#include <shaderapi/shareddefs.h>
#include <shaderapi/ishadershadow.h>
#include <shaderapi/ishaderdynamic.h>

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IMaterialVar;


//-----------------------------------------------------------------------------
// Methods that can be called from the SHADER_INIT blocks of shaders
//-----------------------------------------------------------------------------
class IShaderInit
{
public:
	// Loads up a texture
	void LoadTexture( IMaterialVar *pTextureVar, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 ); 
	void LoadBumpMap( IMaterialVar *pTextureVar, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 );
	void LoadCubeMap( IMaterialVar **ppParams, IMaterialVar *pTextureVar, int nAdditionalCreationFlags = 0 );
};
