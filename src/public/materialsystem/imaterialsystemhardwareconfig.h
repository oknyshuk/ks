#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

[[gnu::always_inline]] inline constexpr auto IsPlatformOpenGL() { return false; }

import std;
import ks.tier1;
//#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "imaterialsystem.h"
//-----------------------------------------------------------------------------
// Material system interface version
//-----------------------------------------------------------------------------

[[gnu::always_inline]] inline bool IsOpenGL( void )
{
	return IsPlatformOpenGL();
}

//-----------------------------------------------------------------------------
// Material system interface version
//-----------------------------------------------------------------------------

// For now, vertex compression is simply "on or off" (for the sake of simplicity
// and MeshBuilder perf.), but later we may support multiple flavours.
enum VertexCompressionType_t
{
	// This indicates an uninitialized VertexCompressionType_t value
	VERTEX_COMPRESSION_INVALID = 0xFFFFFFFF,

	// 'VERTEX_COMPRESSION_NONE' means that no elements of a vertex are compressed
	VERTEX_COMPRESSION_NONE = 0,

	// Currently (more stuff may be added as needed), 'VERTEX_COMPRESSION_ON' means:
	//  - if a vertex contains VERTEX_ELEMENT_NORMAL, this is compressed
	//    (see CVertexBuilder::CompressedNormal3f)
	//  - if a vertex contains VERTEX_ELEMENT_USERDATA4 (and a normal - together defining a tangent
	//    frame, with the binormal reconstructed in the vertex shader), this is compressed
	//    (see CVertexBuilder::CompressedUserData)
	//  - if a vertex contains VERTEX_ELEMENT_BONEWEIGHTSx, this is compressed
	//    (see CVertexBuilder::CompressedBoneWeight3fv)
	VERTEX_COMPRESSION_ON = 1
};


//-----------------------------------------------------------------------------
// Shadow filter types
// Important notes: These enums directly correspond to combo indices.
// If you change these, make the corresponding change in common_ps_fxc.h
// Cheap ones are at the end on purpose, and are only run on ps2b
// SHADOWFILTERMODE_DEFAULT must be 0.
//-----------------------------------------------------------------------------
enum ShadowFilterMode_t
{
	SHADOWFILTERMODE_DEFAULT = 0,

	NVIDIA_PCF = 0,
	ATI_NO_PCF_FETCH4 = 1,
	NVIDIA_PCF_CHEAP = 2,
	ATI_NOPCF = 3,

	// Game consoles use a different set of combo indices to control shadow filtering.
	GAMECONSOLE_NINE_TAP_PCF = 0,
	GAMECONSOLE_SINGLE_TAP_PCF = 1,

	// All modes >= SHADOWFILTERMODE_FIRST_CHEAP_MODE are considered the "cheap" modes.

	SHADOWFILTERMODE_FIRST_CHEAP_MODE = NVIDIA_PCF_CHEAP,
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

enum CSMQualityMode_t
{
	CSMQUALITY_VERY_LOW,
	CSMQUALITY_LOW,
	CSMQUALITY_MEDIUM,
	CSMQUALITY_HIGH,

	CSMQUALITY_TOTAL_MODES
};

// CSMShaderMode_t must match the CSM_MODE static combo in the pixel shaders
enum CSMShaderMode_t
{
	CSMSHADERMODE_LOW_OR_VERY_LOW	= 0,
	CSMSHADERMODE_MEDIUM			= 1,
	CSMSHADERMODE_HIGH				= 2,
	CSMSHADERMODE_ATIFETCH4			= 3,

	CSMSHADERMODE_TOTAL_MODES
};

//-----------------------------------------------------------------------------
// Material system configuration
//-----------------------------------------------------------------------------
class IMaterialSystemHardwareConfig
{
public:
	int	 GetFrameBufferColorDepth() const;
	int  GetSamplerCount() const;
	bool HasSetDeviceGammaRamp() const;
	bool SupportsStaticControlFlow() const;
	VertexCompressionType_t SupportsCompressedVertices() const;
	int  MaximumAnisotropicLevel() const;	// 0 means no anisotropic filtering
	int  MaxTextureWidth() const;
	int  MaxTextureHeight() const;
	int	 TextureMemorySize() const;
	bool SupportsMipmappedCubemaps() const;

	int	 NumVertexShaderConstants() const;
	int	 NumPixelShaderConstants() const;
	int	 MaxNumLights() const;
	int	 MaxTextureAspectRatio() const;
	int	 MaxVertexShaderBlendMatrices() const;
	int	 MaxUserClipPlanes() const;
	bool UseFastClipping() const;

	// This here should be the major item looked at when checking for compat
	// from anywhere other than the material system	shaders
	int GetDXSupportLevel() const;
	const char *GetShaderDLLName() const;

	bool ReadPixelsFromFrontBuffer() const;

	// Are dx dynamic textures preferred?
	bool PreferDynamicTextures() const;

	bool SupportsHDR() const;

	bool NeedsAAClamp() const;
	bool NeedsATICentroidHack() const;

	// This is the max dx support level supported by the card
	int	 GetMaxDXSupportLevel() const;

	// Does the card specify fog color in linear space when sRGBWrites are enabled?
	bool SpecifiesFogColorInLinearSpace() const;

	// Does the card support sRGB reads/writes?
	bool SupportsSRGB() const;
	bool FakeSRGBWrite() const;
	bool CanDoSRGBReadFromRTs() const;

	bool SupportsGLMixedSizeTargets() const;

	bool IsAAEnabled() const;	// Is antialiasing being used?

	// NOTE: Anything after this was added after shipping HL2.
	int GetVertexSamplerCount() const;
	int GetMaxVertexTextureDimension() const;

	int  MaxTextureDepth() const;

	HDRType_t GetHDRType() const;
	HDRType_t GetHardwareHDRType() const;

	bool SupportsStreamOffset() const;

	int StencilBufferBits() const;
	int MaxViewports() const;

	void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport );

	ShadowFilterMode_t GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const;

	int NeedsShaderSRGBConversion() const;

	bool UsesSRGBCorrectBlending() const;

	bool HasFastVertexTextures() const;
	int MaxHWMorphBatchCount() const;

	bool SupportsHDRMode( HDRType_t nHDRMode ) const;

	bool GetHDREnabled( void ) const;
	void SetHDREnabled( bool bEnable );

	bool SupportsBorderColor( void ) const;
	bool SupportsFetch4( void ) const;

	float GetShadowDepthBias() const;
	float GetShadowSlopeScaleDepthBias() const;

	bool PreferZPrepass() const;

	bool SuppressPixelShaderCentroidHackFixup() const;
	bool PreferTexturesInHWMemory() const;
	bool PreferHardwareSync() const;
	bool ActualHasFastVertexTextures() const;

	bool SupportsShadowDepthTextures( void ) const;
	ImageFormat GetShadowDepthTextureFormat( void ) const;
	ImageFormat GetHighPrecisionShadowDepthTextureFormat( void ) const;
	ImageFormat GetNullTextureFormat( void ) const;
	int	GetMinDXSupportLevel() const;
	bool IsUnsupported() const;

	float GetLightMapScaleFactor() const;

	bool SupportsCascadedShadowMapping() const;
	CSMQualityMode_t GetCSMQuality() const;
	bool SupportsBilinearPCFSampling() const;
	CSMShaderMode_t GetCSMShaderMode( CSMQualityMode_t nQualityLevel ) const;
	bool GetCSMAccurateBlending( void ) const;
	void SetCSMAccurateBlending( bool bEnable );

	bool SupportsResolveDepth() const;
	bool HasFullResolutionDepthTexture() const;

	// Backward compat for stdshaders
#if defined ( STDSHADER_DBG_DLL_EXPORT ) || defined( STDSHADER_DX9_DLL_EXPORT )
	inline bool SupportsPixelShaders_2_b() const { return GetDXSupportLevel() >= 92; }
	inline bool SupportsPixelShaders_3_0() const { return GetDXSupportLevel() >= 95; }
#endif

	inline bool ShouldAlwaysUseShaderModel2bShaders() const { return IsOpenGL(); }
	inline bool PlatformRequiresNonNullPixelShaders() const { return true; }
};
