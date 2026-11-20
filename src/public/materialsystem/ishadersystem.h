#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An interface that should not ever be accessed directly from shaders
// but instead is visible only to shaderlib.

#include <materialsystem/IShader.h>
#include <materialsystem/ishadersystem_declarations.h>
import std;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "shaderlib/shadercombosemantics.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
enum Sampler_t;
class ITexture;
class IShader;
struct ShaderComboSemantics_t;

//-----------------------------------------------------------------------------
// The Shader system interface version
//-----------------------------------------------------------------------------
inline constexpr const char *SHADERSYSTEM_INTERFACE_VERSION = "ShaderSystem002";



//-----------------------------------------------------------------------------
// The shader system (a singleton)
//-----------------------------------------------------------------------------
class IShaderSystem
{
public:
	ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel = 0 );

	// Binds a texture
	void BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 );
	void BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 );

	// Takes a snapshot
	void TakeSnapshot( );

	// Draws a snapshot
	void DrawSnapshot( const unsigned char *pInstanceCommandBuffer, bool bMakeActualDrawCall = true );

	// Are we using graphics?
	bool IsUsingGraphics() const;

	// Are editor materials enabled?
	bool CanUseEditorMaterials() const;

	// Bind vertex texture
	void BindVertexTexture( VertexTextureSampler_t vtSampler, ITexture *pTexture, int nFrameVar = 0 );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );
};


//-----------------------------------------------------------------------------
// The Shader plug-in DLL interface version
//-----------------------------------------------------------------------------
inline constexpr const char *SHADER_DLL_INTERFACE_VERSION = "ShaderDLL004";


//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
class IShaderDLLInternal
{
public:
	// Here's where the app systems get to learn about each other 
	bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem );
	void Disconnect( bool bIsMaterialSystem );

	// Returns the number of shaders defined in this DLL
	int ShaderCount() const;

	// Returns information about each shader defined in this DLL
	IShader *GetShader( int nShader );

	// Deals with all of the shader combo semantics from inc files.
	int ShaderComboSemanticsCount() const;
	const ShaderComboSemantics_t *GetComboSemantics( int n );
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
IShaderDLLInternal *GetShaderDLLInternal();
