#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "shaderapi/ishaderapi.h"
#include "datacache/idatacache.h"
#include "locald3dtypes.h"

// uncomment to get dynamic compilation for HLSL shaders
// **IMPORTANT**: if you are trying to use DYNAMIC_SHADER_COMPILE in a RELEASE build, you *must* modify the .vpc file to re-enable the /FC compiler option, otherwise the __FILE__ macro won't work properly in
// GetShaderSourcePath() and shader files won't be found.
//#define DYNAMIC_SHADER_COMPILE

// uncomment to get spew about what combos are being compiled.
//#define DYNAMIC_SHADER_COMPILE_VERBOSE

// uncomment to hide combos that are 0 (requires VERBOSE above)
//#define DYNAMIC_SHADER_COMPILE_THIN

// Uncomment to use remoteshadercompiler.exe as a shader compile server
// Must also set mat_remoteshadercompile to remote shader compile machine name
//#define REMOTE_DYNAMIC_SHADER_COMPILE

// uncomment and fill in with a path to use a specific set of shader source files. Meant for network use.
//		PC path format is of style "\\\\somemachine\\sourcetreeshare\\materialsystem\\stdshaders"
//		Mac path format is of style "/Volumes/jasonm/portal2/staging/src/materialsystem/stdshaders"
//		Linux path format is of style "/home/mariod/p4/csgo/trunk/src/materialsystem/stdshaders"
//		Xbox is not supported. Xbox's ability to see PC is not supported by XDK in Vista.
//#define DYNAMIC_SHADER_COMPILE_CUSTOM_PATH "/Volumes/Data/p4/csgo/staging/src/materialsystem/stdshaders"

//#define SHADER_COMBO_SPEW_VERBOSE 1

// uncomment to get disassembled (asm) shader code in your game dir as *.asm
//#define DYNAMIC_SHADER_COMPILE_WRITE_ASSEMBLY

// uncomment to get disassembled (asm) shader code in your game dir as *.asm
//#define WRITE_ASSEMBLY

//-----------------------------------------------------------------------------
// Vertex + pixel shader manager
//-----------------------------------------------------------------------------
class IShaderManager
{
public:
	// Initialize, shutdown
	void Init();
	void Shutdown();

	// Compiles vertex shaders
	IShaderBuffer *CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );

	// New version of these methods	[dx10 port]
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer );
	void DestroyVertexShader( VertexShaderHandle_t hShader );
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer );
	void DestroyPixelShader( PixelShaderHandle_t hShader );

	// Creates vertex, pixel shaders
	VertexShader_t CreateVertexShader( const char *pVertexShaderFile, int nStaticVshIndex = 0, char *debugLabel = nullptr  );
	PixelShader_t CreatePixelShader( const char *pPixelShaderFile, int nStaticPshIndex = 0, char *debugLabel = nullptr );

	// Sets which dynamic version of the vertex + pixel shader to use
	void SetVertexShaderIndex( int vshIndex );
	void SetPixelShaderIndex( int pshIndex );

	// Sets the vertex + pixel shader render state
	void SetVertexShader( VertexShader_t shader );
	void SetPixelShader( PixelShader_t shader );

	void SetPixelShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader  );
	void SetVertexShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader );

	// Resets the vertex + pixel shader state
	void ResetShaderState();

	// Flushes all shaders so they are reloaded+recompiled (does nothing unless dynamic shader compile is enabled)
	void FlushShaders();

	// Returns the current vertex + pixel shaders
	void *GetCurrentVertexShader();
	void *GetCurrentPixelShader();

	void ClearVertexAndPixelShaderRefCounts();
	void PurgeUnusedVertexAndPixelShaders();

	// The low-level dx call to set the vertex shader state
	void BindVertexShader( VertexShaderHandle_t shader );
	void BindPixelShader( PixelShaderHandle_t shader );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );



	HardwareShader_t GetVertexShader( VertexShader_t vs, int dynIdx );
	HardwareShader_t GetPixelShader( PixelShader_t ps, int dynIdx );
};

extern void DestroyAllVertexAndPixelShaders( void );
