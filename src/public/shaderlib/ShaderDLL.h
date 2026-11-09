#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <materialsystem/IShader.h>
#include "shaderlib/shadercombosemantics.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShader;
class ICvar;
struct ShaderComboInformation_t;

//-----------------------------------------------------------------------------
// The standard implementation of CShaderDLL
//-----------------------------------------------------------------------------
class IShaderDLL
{
public:
	// Adds a shader to the list of shaders
	void InsertShader( IShader *pShader );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
IShaderDLL *GetShaderDLL();

//-----------------------------------------------------------------------------
// Singleton interface for CVars
//-----------------------------------------------------------------------------
ICvar *GetCVar();
