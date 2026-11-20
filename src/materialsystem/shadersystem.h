#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.core.types;
import ks.platform;
import ks.simd;

// Purpose: Shader system:
//	The shader system makes a few fundamental assumptions about when
//	certain types of state get set.
// 1) Anything that can potentially affect vertex format must be set up
//		during the shader shadow/snapshot phase
// 2) Anything that we can dynamically mess with (through a material var)
//		should happen in the dynamic/render phase
// 3) In general, we try to cache off expensive state pre-processing in
//		the shader shadow phase (like texture stage pipeline).

#include "materialsystem/ishadersystem.h"
#include "shaderlib/BaseShader.h"
#include "materialsystem/materialsystem_config.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem_global.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IMaterialVar;
class TextureManager_t;
class ITextureInternal;
class ShaderSystem_t;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
enum MaterialPrimitiveType_t;
enum MaterialPropertyTypes_t;
enum MaterialIndexFormat_t;
enum ShaderParamType_t;


//-----------------------------------------------------------------------------
// for ShaderRenderState_t::m_flags
//-----------------------------------------------------------------------------
enum
{
	// The flags up here are computed from the shaders themselves

/*
	// lighting flags
	SHADER_UNLIT					= 0x0000,
	SHADER_VERTEX_LIT				= 0x0001,
	SHADER_NEEDS_LIGHTMAP			= 0x0002, 
	SHADER_NEEDS_BUMPED_LIGHTMAPS	= 0x0004,
	SHADER_LIGHTING_MASK			= 0x0007,
*/

	// opacity flags
	SHADER_OPACITY_ALPHATEST		= 0x0010,
	SHADER_OPACITY_OPAQUE			= 0x0020,
	SHADER_OPACITY_TRANSLUCENT		= 0x0040,
	SHADER_OPACITY_MASK				= 0x0070,
};


enum
{
	MAX_RENDER_PASSES = 3
};


//-----------------------------------------------------------------------------
// Information for a single render pass
//-----------------------------------------------------------------------------
struct RenderPassList_t
{
	int m_nPassCount;
	StateSnapshot_t	m_Snapshot[MAX_RENDER_PASSES];
	// per material shader-defined state
	CBasePerMaterialContextData *m_pContextData[MAX_RENDER_PASSES];
	CBasePerInstanceContextData *m_pInstanceData[MAX_RENDER_PASSES];
};

struct ShaderRenderState_t
{
	// These are the same, regardless of whether alpha or color mod is used
	int				m_Flags;	// Can't shrink this to a short
	VertexFormat_t	m_VertexFormat;
	VertexFormat_t	m_VertexUsage;

	// List of all snapshots
	RenderPassList_t *m_pSnapshots;
};


//-----------------------------------------------------------------------------
// Used to get the snapshot count
//-----------------------------------------------------------------------------
enum
{
	SNAPSHOT_COUNT_NORMAL = 8,
	SNAPSHOT_COUNT_EDITOR = 16,
	SNAPSHOT_COUNT_GBUFFER = 64,
};

inline int SnapshotTypeCount()
{
	int nMaterialUseFlags = MaterialSystem()->GetConfigurationFlags();
	if ( nMaterialUseFlags & MATCONFIG_FLAGS_SUPPORT_GBUFFER )
		return SNAPSHOT_COUNT_GBUFFER;
	if ( nMaterialUseFlags & MATCONFIG_FLAGS_SUPPORT_EDITOR )
		return SNAPSHOT_COUNT_EDITOR;
	return SNAPSHOT_COUNT_NORMAL;
}


//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------
inline void SetFlags( IMaterialVar **params, MaterialVarFlags_t _flag )
{
	params[FLAGS]->SetIntValue( params[FLAGS]->GetIntValueFast() | (_flag) );
}

inline void SetFlags2( IMaterialVar **params, MaterialVarFlags2_t _flag )
{
	params[FLAGS2]->SetIntValue( params[FLAGS2]->GetIntValueFast() | (_flag) );
}

inline bool IsFlagSet( IMaterialVar **params, MaterialVarFlags_t _flag )
{
	return ((params[FLAGS]->GetIntValueFast() & (_flag) ) != 0);
}

inline bool IsFlag2Set( IMaterialVar **params, MaterialVarFlags2_t _flag )
{
	return ((params[FLAGS2]->GetIntValueFast() & (_flag) ) != 0);
}



//-----------------------------------------------------------------------------
// Poll params + renderstate
//-----------------------------------------------------------------------------
inline bool	IsTranslucent( const ShaderRenderState_t* pRenderState )
{
	return (pRenderState->m_Flags & SHADER_OPACITY_TRANSLUCENT) != 0;
}

inline bool	IsAlphaTested( ShaderRenderState_t* pRenderState )
{
	return (pRenderState->m_Flags & SHADER_OPACITY_ALPHATEST) != 0;
}


//-----------------------------------------------------------------------------
// The shader system (a singleton)
//-----------------------------------------------------------------------------
class IShaderSystemInternal : public IShaderInit, public IShaderSystem
{
public:
	// Initialization, shutdown
	void		Init();
	void		Shutdown();
	void		ModInit();
	void		ModShutdown();

	// Methods related to reading in shader DLLs
	bool		LoadShaderDLL( const char *pFullPath );
	void		UnloadShaderDLL( const char *pFullPath );

	// Find me a shader!
	IShader*	FindShader( char const* pShaderName );

	// returns strings associated with the shader state flags...
	char const* ShaderStateString( int i ) const;
	int ShaderStateCount( ) const;

	// Rendering related methods

	// Create debugging materials
	void CreateDebugMaterials();

	// Cleans up the debugging materials
	void CleanUpDebugMaterials();

	// Call the SHADER_PARAM_INIT block of the shaders
	void InitShaderParameters( IShader *pShader, IMaterialVar **params, const char *pMaterialName );

	// Call the SHADER_INIT block of the shaders
	void InitShaderInstance( IShader *pShader, IMaterialVar **params, const char *pMaterialName, const char *pTextureGroupName );

	// go through each param and make sure it is the right type, load textures, 
	// compute state snapshots and vertex types, etc.
	bool InitRenderState( IShader *pShader, int numParams, IMaterialVar **params, ShaderRenderState_t* pRenderState, char const* pMaterialName );

	// When you're done with the shader, be sure to call this to clean up
	void CleanupRenderState( ShaderRenderState_t* pRenderState );

	// Draws the shader
	void DrawElements( IShader *pShader, IMaterialVar **params, ShaderRenderState_t* pShaderState, VertexCompressionType_t vertexCompression,
							   uint32 nMaterialVarTimeStamp, uint32 nModulationFlags, bool bRenderingPreTessPatchMesh );

	// Used to iterate over all shaders for editing purposes
	int	 ShaderCount() const;
	int  GetShaders( int nFirstShader, int nCount, IShader **ppShaderList ) const;
};
