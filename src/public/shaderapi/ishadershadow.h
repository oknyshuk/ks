#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "shaderapi/shareddefs.h"
#include <materialsystem/imaterial.h>
import ks.math;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CMeshBuilder;
class IMaterialVar;


//-----------------------------------------------------------------------------
// important enumerations
//-----------------------------------------------------------------------------
enum ShaderDepthFunc_t 
{ 
	SHADER_DEPTHFUNC_NEVER,
	SHADER_DEPTHFUNC_NEARER,
	SHADER_DEPTHFUNC_EQUAL,
	SHADER_DEPTHFUNC_NEAREROREQUAL,
	SHADER_DEPTHFUNC_FARTHER,
	SHADER_DEPTHFUNC_NOTEQUAL,
	SHADER_DEPTHFUNC_FARTHEROREQUAL,
	SHADER_DEPTHFUNC_ALWAYS
};

enum ShaderBlendFactor_t
{
	SHADER_BLEND_ZERO,
	SHADER_BLEND_ONE,
	SHADER_BLEND_DST_COLOR,
	SHADER_BLEND_ONE_MINUS_DST_COLOR,
	SHADER_BLEND_SRC_ALPHA,
	SHADER_BLEND_ONE_MINUS_SRC_ALPHA,
	SHADER_BLEND_DST_ALPHA,
	SHADER_BLEND_ONE_MINUS_DST_ALPHA,
	SHADER_BLEND_SRC_ALPHA_SATURATE,
	SHADER_BLEND_SRC_COLOR,
	SHADER_BLEND_ONE_MINUS_SRC_COLOR
};

enum ShaderBlendOp_t
{
	SHADER_BLEND_OP_ADD,
	SHADER_BLEND_OP_SUBTRACT,
	SHADER_BLEND_OP_REVSUBTRACT,
	SHADER_BLEND_OP_MIN,
	SHADER_BLEND_OP_MAX
};

enum ShaderAlphaFunc_t
{
	SHADER_ALPHAFUNC_NEVER,
	SHADER_ALPHAFUNC_LESS,
	SHADER_ALPHAFUNC_EQUAL,
	SHADER_ALPHAFUNC_LEQUAL,
	SHADER_ALPHAFUNC_GREATER,
	SHADER_ALPHAFUNC_NOTEQUAL,
	SHADER_ALPHAFUNC_GEQUAL,
	SHADER_ALPHAFUNC_ALWAYS
};

enum ShaderTexChannel_t
{
	SHADER_TEXCHANNEL_COLOR = 0,
	SHADER_TEXCHANNEL_ALPHA
};

enum ShaderPolyModeFace_t
{
	SHADER_POLYMODEFACE_FRONT,
	SHADER_POLYMODEFACE_BACK,
	SHADER_POLYMODEFACE_FRONT_AND_BACK,
};

enum ShaderPolyMode_t
{
	SHADER_POLYMODE_POINT,
	SHADER_POLYMODE_LINE,
	SHADER_POLYMODE_FILL
};

enum ShaderFogMode_t
{
	SHADER_FOGMODE_DISABLED = 0,
	SHADER_FOGMODE_OO_OVERBRIGHT,
	SHADER_FOGMODE_BLACK,
	SHADER_FOGMODE_GREY,
	SHADER_FOGMODE_FOGCOLOR,
	SHADER_FOGMODE_WHITE,
	SHADER_FOGMODE_NUMFOGMODES
};



// m_ZBias has only two bits in ShadowState_t, so be careful extending this enum
enum PolygonOffsetMode_t
{
	SHADER_POLYOFFSET_DISABLE		= 0x0,
	SHADER_POLYOFFSET_DECAL			= 0x1,
	SHADER_POLYOFFSET_SHADOW_BIAS	= 0x2,
	SHADER_POLYOFFSET_RESERVED		= 0x3	// Reserved for future use
};


//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
inline constexpr const char *SHADERSHADOW_INTERFACE_VERSION = "ShaderShadow010";


//-----------------------------------------------------------------------------
// the shader API interface (methods called from shaders)
//-----------------------------------------------------------------------------
class IShaderShadow
{
public:
	// Sets the default *shadow* state
	void SetDefaultState();

	// Methods related to depth buffering
	void DepthFunc( ShaderDepthFunc_t depthFunc );
	void EnableDepthWrites( bool bEnable );
	void EnableDepthTest( bool bEnable );
	void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode );

	// Suppresses/activates color writing 
	void EnableColorWrites( bool bEnable );
	void EnableAlphaWrites( bool bEnable );

	// Methods related to alpha blending
	void EnableBlending( bool bEnable );
	void EnableBlendingForceOpaque( bool bEnable ); // enables alpha blending on a batch but does not force it to render with translucents
	void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );
	void EnableBlendingSeparateAlpha( bool bEnable );
	void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );
	// More below...

	// Alpha testing
	void EnableAlphaTest( bool bEnable );
	void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ );

	// Wireframe/filled polygons
	void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode );

	// Back face culling
	void EnableCulling( bool bEnable );

	// Indicates the vertex format for use with a vertex shader
	// The flags to pass in here come from the VertexFormatFlags_t enum
	// If pTexCoordDimensions is *not* specified, we assume all coordinates
	// are 2-dimensional
	void VertexShaderVertexFormat( unsigned int nFlags, 
			int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize );

	// Pixel and vertex shader methods
	void SetVertexShader( const char* pFileName, int nStaticVshIndex );
	void SetPixelShader( const char* pFileName, int nStaticPshIndex = 0 );

	// Convert from linear to gamma color space on writes to frame buffer.
	void EnableSRGBWrite( bool bEnable );

	// Convert from gamma to linear on texture fetch.
	void EnableSRGBRead( Sampler_t sampler, bool bEnable );

	// Per texture unit stuff
	void EnableTexture( Sampler_t sampler, bool bEnable );

	void FogMode( ShaderFogMode_t fogMode, bool bVertexFog );

	void DisableFogGammaCorrection( bool bDisable ); //some blending modes won't work properly with corrected fog

	// Alpha to coverage
	void EnableAlphaToCoverage( bool bEnable );

	// Shadow map filtering
	//virtual void SetShadowDepthFiltering( Sampler_t stage ) = 0;

	// Per vertex texture unit stuff
	void EnableVertexTexture( VertexTextureSampler_t sampler, bool bEnable );
	
	// More alpha blending state
	void BlendOp( ShaderBlendOp_t blendOp );
	void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp );

	float GetLightMapScaleFactor( void ) const;
};
// end class IShaderShadow
