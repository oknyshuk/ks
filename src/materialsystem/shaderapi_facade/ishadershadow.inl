// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderShadow facade class to the shader shadow implementation.
// Included by the TU that defines the implementation, which must declare ShaderShadowImpl() returning it.

// IShaderShadow
void IShaderShadow::SetDefaultState()
{ ShaderShadowImpl().SetDefaultState(); }
void IShaderShadow::DepthFunc( ShaderDepthFunc_t depthFunc )
{ ShaderShadowImpl().DepthFunc( depthFunc ); }
void IShaderShadow::EnableDepthWrites( bool bEnable )
{ ShaderShadowImpl().EnableDepthWrites( bEnable ); }
void IShaderShadow::EnableDepthTest( bool bEnable )
{ ShaderShadowImpl().EnableDepthTest( bEnable ); }
void IShaderShadow::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{ ShaderShadowImpl().EnablePolyOffset( nOffsetMode ); }
void IShaderShadow::EnableColorWrites( bool bEnable )
{ ShaderShadowImpl().EnableColorWrites( bEnable ); }
void IShaderShadow::EnableAlphaWrites( bool bEnable )
{ ShaderShadowImpl().EnableAlphaWrites( bEnable ); }
void IShaderShadow::EnableBlending( bool bEnable )
{ ShaderShadowImpl().EnableBlending( bEnable ); }
void IShaderShadow::EnableBlendingForceOpaque( bool bEnable )
{ ShaderShadowImpl().EnableBlendingForceOpaque( bEnable ); }
void IShaderShadow::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{ ShaderShadowImpl().BlendFunc( srcFactor, dstFactor ); }
void IShaderShadow::EnableBlendingSeparateAlpha( bool bEnable )
{ ShaderShadowImpl().EnableBlendingSeparateAlpha( bEnable ); }
void IShaderShadow::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{ ShaderShadowImpl().BlendFuncSeparateAlpha( srcFactor, dstFactor ); }
void IShaderShadow::EnableAlphaTest( bool bEnable )
{ ShaderShadowImpl().EnableAlphaTest( bEnable ); }
void IShaderShadow::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef )
{ ShaderShadowImpl().AlphaFunc( alphaFunc, alphaRef ); }
void IShaderShadow::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{ ShaderShadowImpl().PolyMode( face, polyMode ); }
void IShaderShadow::EnableCulling( bool bEnable )
{ ShaderShadowImpl().EnableCulling( bEnable ); }
void IShaderShadow::VertexShaderVertexFormat( unsigned int nFlags, int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize )
{ ShaderShadowImpl().VertexShaderVertexFormat( nFlags, nTexCoordCount, pTexCoordDimensions, nUserDataSize ); }
void IShaderShadow::SetVertexShader( const char* pFileName, int nStaticVshIndex )
{ ShaderShadowImpl().SetVertexShader( pFileName, nStaticVshIndex ); }
void IShaderShadow::SetPixelShader( const char* pFileName, int nStaticPshIndex )
{ ShaderShadowImpl().SetPixelShader( pFileName, nStaticPshIndex ); }
void IShaderShadow::EnableSRGBWrite( bool bEnable )
{ ShaderShadowImpl().EnableSRGBWrite( bEnable ); }
void IShaderShadow::EnableSRGBRead( Sampler_t sampler, bool bEnable )
{ ShaderShadowImpl().EnableSRGBRead( sampler, bEnable ); }
void IShaderShadow::EnableTexture( Sampler_t sampler, bool bEnable )
{ ShaderShadowImpl().EnableTexture( sampler, bEnable ); }
void IShaderShadow::FogMode( ShaderFogMode_t fogMode, bool bVertexFog )
{ ShaderShadowImpl().FogMode( fogMode, bVertexFog ); }
void IShaderShadow::DisableFogGammaCorrection( bool bDisable )
{ ShaderShadowImpl().DisableFogGammaCorrection( bDisable ); }
void IShaderShadow::EnableAlphaToCoverage( bool bEnable )
{ ShaderShadowImpl().EnableAlphaToCoverage( bEnable ); }
void IShaderShadow::EnableVertexTexture( VertexTextureSampler_t sampler, bool bEnable )
{ ShaderShadowImpl().EnableVertexTexture( sampler, bEnable ); }
void IShaderShadow::BlendOp( ShaderBlendOp_t blendOp )
{ ShaderShadowImpl().BlendOp( blendOp ); }
void IShaderShadow::BlendOpSeparateAlpha( ShaderBlendOp_t blendOp )
{ ShaderShadowImpl().BlendOpSeparateAlpha( blendOp ); }
float IShaderShadow::GetLightMapScaleFactor( void ) const
{ return ShaderShadowImpl().GetLightMapScaleFactor(); }

