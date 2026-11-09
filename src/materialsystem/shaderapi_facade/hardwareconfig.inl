// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialSystemHardwareConfig and IHardwareConfigInternal facade classes to the hardware config implementation.
// Included by the TU that defines the implementation, which must declare HardwareConfigImpl() returning it.

// IMaterialSystemHardwareConfig
int IMaterialSystemHardwareConfig::GetFrameBufferColorDepth() const
{ return HardwareConfigImpl().GetFrameBufferColorDepth(); }
int IMaterialSystemHardwareConfig::GetSamplerCount() const
{ return HardwareConfigImpl().GetSamplerCount(); }
bool IMaterialSystemHardwareConfig::HasSetDeviceGammaRamp() const
{ return HardwareConfigImpl().HasSetDeviceGammaRamp(); }
bool IMaterialSystemHardwareConfig::SupportsStaticControlFlow() const
{ return HardwareConfigImpl().SupportsStaticControlFlow(); }
VertexCompressionType_t IMaterialSystemHardwareConfig::SupportsCompressedVertices() const
{ return HardwareConfigImpl().SupportsCompressedVertices(); }
int IMaterialSystemHardwareConfig::MaximumAnisotropicLevel() const
{ return HardwareConfigImpl().MaximumAnisotropicLevel(); }
int IMaterialSystemHardwareConfig::MaxTextureWidth() const
{ return HardwareConfigImpl().MaxTextureWidth(); }
int IMaterialSystemHardwareConfig::MaxTextureHeight() const
{ return HardwareConfigImpl().MaxTextureHeight(); }
int IMaterialSystemHardwareConfig::TextureMemorySize() const
{ return HardwareConfigImpl().TextureMemorySize(); }
bool IMaterialSystemHardwareConfig::SupportsMipmappedCubemaps() const
{ return HardwareConfigImpl().SupportsMipmappedCubemaps(); }
int IMaterialSystemHardwareConfig::NumVertexShaderConstants() const
{ return HardwareConfigImpl().NumVertexShaderConstants(); }
int IMaterialSystemHardwareConfig::NumPixelShaderConstants() const
{ return HardwareConfigImpl().NumPixelShaderConstants(); }
int IMaterialSystemHardwareConfig::MaxNumLights() const
{ return HardwareConfigImpl().MaxNumLights(); }
int IMaterialSystemHardwareConfig::MaxTextureAspectRatio() const
{ return HardwareConfigImpl().MaxTextureAspectRatio(); }
int IMaterialSystemHardwareConfig::MaxVertexShaderBlendMatrices() const
{ return HardwareConfigImpl().MaxVertexShaderBlendMatrices(); }
int IMaterialSystemHardwareConfig::MaxUserClipPlanes() const
{ return HardwareConfigImpl().MaxUserClipPlanes(); }
bool IMaterialSystemHardwareConfig::UseFastClipping() const
{ return HardwareConfigImpl().UseFastClipping(); }
int IMaterialSystemHardwareConfig::GetDXSupportLevel() const
{ return HardwareConfigImpl().GetDXSupportLevel(); }
const char * IMaterialSystemHardwareConfig::GetShaderDLLName() const
{ return HardwareConfigImpl().GetShaderDLLName(); }
bool IMaterialSystemHardwareConfig::ReadPixelsFromFrontBuffer() const
{ return HardwareConfigImpl().ReadPixelsFromFrontBuffer(); }
bool IMaterialSystemHardwareConfig::PreferDynamicTextures() const
{ return HardwareConfigImpl().PreferDynamicTextures(); }
bool IMaterialSystemHardwareConfig::SupportsHDR() const
{ return HardwareConfigImpl().SupportsHDR(); }
bool IMaterialSystemHardwareConfig::NeedsAAClamp() const
{ return HardwareConfigImpl().NeedsAAClamp(); }
bool IMaterialSystemHardwareConfig::NeedsATICentroidHack() const
{ return HardwareConfigImpl().NeedsATICentroidHack(); }
int IMaterialSystemHardwareConfig::GetMaxDXSupportLevel() const
{ return HardwareConfigImpl().GetMaxDXSupportLevel(); }
bool IMaterialSystemHardwareConfig::SpecifiesFogColorInLinearSpace() const
{ return HardwareConfigImpl().SpecifiesFogColorInLinearSpace(); }
bool IMaterialSystemHardwareConfig::SupportsSRGB() const
{ return HardwareConfigImpl().SupportsSRGB(); }
bool IMaterialSystemHardwareConfig::FakeSRGBWrite() const
{ return HardwareConfigImpl().FakeSRGBWrite(); }
bool IMaterialSystemHardwareConfig::CanDoSRGBReadFromRTs() const
{ return HardwareConfigImpl().CanDoSRGBReadFromRTs(); }
bool IMaterialSystemHardwareConfig::SupportsGLMixedSizeTargets() const
{ return HardwareConfigImpl().SupportsGLMixedSizeTargets(); }
bool IMaterialSystemHardwareConfig::IsAAEnabled() const
{ return HardwareConfigImpl().IsAAEnabled(); }
int IMaterialSystemHardwareConfig::GetVertexSamplerCount() const
{ return HardwareConfigImpl().GetVertexSamplerCount(); }
int IMaterialSystemHardwareConfig::GetMaxVertexTextureDimension() const
{ return HardwareConfigImpl().GetMaxVertexTextureDimension(); }
int IMaterialSystemHardwareConfig::MaxTextureDepth() const
{ return HardwareConfigImpl().MaxTextureDepth(); }
HDRType_t IMaterialSystemHardwareConfig::GetHDRType() const
{ return HardwareConfigImpl().GetHDRType(); }
HDRType_t IMaterialSystemHardwareConfig::GetHardwareHDRType() const
{ return HardwareConfigImpl().GetHardwareHDRType(); }
bool IMaterialSystemHardwareConfig::SupportsStreamOffset() const
{ return HardwareConfigImpl().SupportsStreamOffset(); }
int IMaterialSystemHardwareConfig::StencilBufferBits() const
{ return HardwareConfigImpl().StencilBufferBits(); }
int IMaterialSystemHardwareConfig::MaxViewports() const
{ return HardwareConfigImpl().MaxViewports(); }
void IMaterialSystemHardwareConfig::OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport )
{ HardwareConfigImpl().OverrideStreamOffsetSupport( bOverrideEnabled, bEnableSupport ); }
ShadowFilterMode_t IMaterialSystemHardwareConfig::GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const
{ return HardwareConfigImpl().GetShadowFilterMode( bForceLowQualityShadows, bPS30 ); }
int IMaterialSystemHardwareConfig::NeedsShaderSRGBConversion() const
{ return HardwareConfigImpl().NeedsShaderSRGBConversion(); }
bool IMaterialSystemHardwareConfig::UsesSRGBCorrectBlending() const
{ return HardwareConfigImpl().UsesSRGBCorrectBlending(); }
bool IMaterialSystemHardwareConfig::HasFastVertexTextures() const
{ return HardwareConfigImpl().HasFastVertexTextures(); }
int IMaterialSystemHardwareConfig::MaxHWMorphBatchCount() const
{ return HardwareConfigImpl().MaxHWMorphBatchCount(); }
bool IMaterialSystemHardwareConfig::SupportsHDRMode( HDRType_t nHDRMode ) const
{ return HardwareConfigImpl().SupportsHDRMode( nHDRMode ); }
bool IMaterialSystemHardwareConfig::GetHDREnabled( void ) const
{ return HardwareConfigImpl().GetHDREnabled(); }
void IMaterialSystemHardwareConfig::SetHDREnabled( bool bEnable )
{ HardwareConfigImpl().SetHDREnabled( bEnable ); }
bool IMaterialSystemHardwareConfig::SupportsBorderColor( void ) const
{ return HardwareConfigImpl().SupportsBorderColor(); }
bool IMaterialSystemHardwareConfig::SupportsFetch4( void ) const
{ return HardwareConfigImpl().SupportsFetch4(); }
float IMaterialSystemHardwareConfig::GetShadowDepthBias() const
{ return HardwareConfigImpl().GetShadowDepthBias(); }
float IMaterialSystemHardwareConfig::GetShadowSlopeScaleDepthBias() const
{ return HardwareConfigImpl().GetShadowSlopeScaleDepthBias(); }
bool IMaterialSystemHardwareConfig::PreferZPrepass() const
{ return HardwareConfigImpl().PreferZPrepass(); }
bool IMaterialSystemHardwareConfig::SuppressPixelShaderCentroidHackFixup() const
{ return HardwareConfigImpl().SuppressPixelShaderCentroidHackFixup(); }
bool IMaterialSystemHardwareConfig::PreferTexturesInHWMemory() const
{ return HardwareConfigImpl().PreferTexturesInHWMemory(); }
bool IMaterialSystemHardwareConfig::PreferHardwareSync() const
{ return HardwareConfigImpl().PreferHardwareSync(); }
bool IMaterialSystemHardwareConfig::ActualHasFastVertexTextures() const
{ return HardwareConfigImpl().ActualHasFastVertexTextures(); }
bool IMaterialSystemHardwareConfig::SupportsShadowDepthTextures( void ) const
{ return HardwareConfigImpl().SupportsShadowDepthTextures(); }
ImageFormat IMaterialSystemHardwareConfig::GetShadowDepthTextureFormat( void ) const
{ return HardwareConfigImpl().GetShadowDepthTextureFormat(); }
ImageFormat IMaterialSystemHardwareConfig::GetHighPrecisionShadowDepthTextureFormat( void ) const
{ return HardwareConfigImpl().GetHighPrecisionShadowDepthTextureFormat(); }
ImageFormat IMaterialSystemHardwareConfig::GetNullTextureFormat( void ) const
{ return HardwareConfigImpl().GetNullTextureFormat(); }
int IMaterialSystemHardwareConfig::GetMinDXSupportLevel() const
{ return HardwareConfigImpl().GetMinDXSupportLevel(); }
bool IMaterialSystemHardwareConfig::IsUnsupported() const
{ return HardwareConfigImpl().IsUnsupported(); }
float IMaterialSystemHardwareConfig::GetLightMapScaleFactor() const
{ return HardwareConfigImpl().GetLightMapScaleFactor(); }
bool IMaterialSystemHardwareConfig::SupportsCascadedShadowMapping() const
{ return HardwareConfigImpl().SupportsCascadedShadowMapping(); }
CSMQualityMode_t IMaterialSystemHardwareConfig::GetCSMQuality() const
{ return HardwareConfigImpl().GetCSMQuality(); }
bool IMaterialSystemHardwareConfig::SupportsBilinearPCFSampling() const
{ return HardwareConfigImpl().SupportsBilinearPCFSampling(); }
CSMShaderMode_t IMaterialSystemHardwareConfig::GetCSMShaderMode( CSMQualityMode_t nQualityLevel ) const
{ return HardwareConfigImpl().GetCSMShaderMode( nQualityLevel ); }
bool IMaterialSystemHardwareConfig::GetCSMAccurateBlending( void ) const
{ return HardwareConfigImpl().GetCSMAccurateBlending(); }
void IMaterialSystemHardwareConfig::SetCSMAccurateBlending( bool bEnable )
{ HardwareConfigImpl().SetCSMAccurateBlending( bEnable ); }
bool IMaterialSystemHardwareConfig::SupportsResolveDepth() const
{ return HardwareConfigImpl().SupportsResolveDepth(); }
bool IMaterialSystemHardwareConfig::HasFullResolutionDepthTexture() const
{ return HardwareConfigImpl().HasFullResolutionDepthTexture(); }

// IHardwareConfigInternal
const char * IHardwareConfigInternal::GetHWSpecificShaderDLLName() const
{ return HardwareConfigImpl().GetHWSpecificShaderDLLName(); }

