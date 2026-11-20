import ks.core.basetypes;
import ks.core.threadtools;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.lightdesc;
import ks.math.vector;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.tier1.exprevaluator;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderAPI facade class to the shader API implementation.
// Included by the TU that defines the implementation, which must declare ShaderAPIImpl() returning it.

// IShaderAPI
void IShaderAPI::SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately )
{ ShaderAPIImpl().SetViewports( nCount, pViewports, setImmediately ); }
int IShaderAPI::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{ return ShaderAPIImpl().GetViewports( pViewports, nMax ); }
void IShaderAPI::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{ ShaderAPIImpl().ClearBuffers( bClearColor, bClearDepth, bClearStencil, renderTargetWidth, renderTargetHeight ); }
void IShaderAPI::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{ ShaderAPIImpl().ClearColor3ub( r, g, b ); }
void IShaderAPI::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{ ShaderAPIImpl().ClearColor4ub( r, g, b, a ); }
void IShaderAPI::BindVertexShader( VertexShaderHandle_t hVertexShader )
{ ShaderAPIImpl().BindVertexShader( hVertexShader ); }
void IShaderAPI::BindGeometryShader( GeometryShaderHandle_t hGeometryShader )
{ ShaderAPIImpl().BindGeometryShader( hGeometryShader ); }
void IShaderAPI::BindPixelShader( PixelShaderHandle_t hPixelShader )
{ ShaderAPIImpl().BindPixelShader( hPixelShader ); }
void IShaderAPI::SetRasterState( const ShaderRasterState_t& state )
{ ShaderAPIImpl().SetRasterState( state ); }
bool IShaderAPI::SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info )
{ return ShaderAPIImpl().SetMode( hwnd, nAdapter, info ); }
void IShaderAPI::ChangeVideoMode( const ShaderDeviceInfo_t &info )
{ ShaderAPIImpl().ChangeVideoMode( info ); }
StateSnapshot_t IShaderAPI::TakeSnapshot()
{ return ShaderAPIImpl().TakeSnapshot(); }
void IShaderAPI::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{ ShaderAPIImpl().TexMinFilter( texFilterMode ); }
void IShaderAPI::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{ ShaderAPIImpl().TexMagFilter( texFilterMode ); }
void IShaderAPI::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{ ShaderAPIImpl().TexWrap( coord, wrapMode ); }
void IShaderAPI::CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().CopyRenderTargetToTexture( textureHandle ); }
void IShaderAPI::Bind( IMaterial* pMaterial )
{ ShaderAPIImpl().Bind( pMaterial ); }
IMesh* IShaderAPI::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{ return ShaderAPIImpl().GetDynamicMesh( pMaterial, nHWSkinBoneCount, bBuffered, pVertexOverride, pIndexOverride ); }
IMesh* IShaderAPI::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{ return ShaderAPIImpl().GetDynamicMeshEx( pMaterial, vertexFormat, nHWSkinBoneCount, bBuffered, pVertexOverride, pIndexOverride ); }
bool IShaderAPI::IsTranslucent( StateSnapshot_t id ) const
{ return ShaderAPIImpl().IsTranslucent( id ); }
bool IShaderAPI::IsAlphaTested( StateSnapshot_t id ) const
{ return ShaderAPIImpl().IsAlphaTested( id ); }
bool IShaderAPI::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{ return ShaderAPIImpl().UsesVertexAndPixelShaders( id ); }
bool IShaderAPI::IsDepthWriteEnabled( StateSnapshot_t id ) const
{ return ShaderAPIImpl().IsDepthWriteEnabled( id ); }
VertexFormat_t IShaderAPI::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{ return ShaderAPIImpl().ComputeVertexFormat( numSnapshots, pIds ); }
VertexFormat_t IShaderAPI::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const
{ return ShaderAPIImpl().ComputeVertexUsage( numSnapshots, pIds ); }
void IShaderAPI::BeginPass( StateSnapshot_t snapshot )
{ ShaderAPIImpl().BeginPass( snapshot ); }
void IShaderAPI::RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount )
{ ShaderAPIImpl().RenderPass( pInstanceCommandBuffer, nPass, nPassCount ); }
void IShaderAPI::SetNumBoneWeights( int numBones )
{ ShaderAPIImpl().SetNumBoneWeights( numBones ); }
void IShaderAPI::SetLights( int nCount, const LightDesc_t *pDesc )
{ ShaderAPIImpl().SetLights( nCount, pDesc ); }
void IShaderAPI::SetLightingOrigin( Vector vLightingOrigin )
{ ShaderAPIImpl().SetLightingOrigin( vLightingOrigin ); }
void IShaderAPI::SetLightingState( const MaterialLightingState_t& state )
{ ShaderAPIImpl().SetLightingState( state ); }
void IShaderAPI::SetAmbientLightCube( Vector4D cube[6] )
{ ShaderAPIImpl().SetAmbientLightCube( cube ); }
void IShaderAPI::ShadeMode( ShaderShadeMode_t mode )
{ ShaderAPIImpl().ShadeMode( mode ); }
void IShaderAPI::CullMode( MaterialCullMode_t cullMode )
{ ShaderAPIImpl().CullMode( cullMode ); }
void IShaderAPI::FlipCullMode( void )
{ ShaderAPIImpl().FlipCullMode(); }
void IShaderAPI::BeginGeneratingCSMs()
{ ShaderAPIImpl().BeginGeneratingCSMs(); }
void IShaderAPI::EndGeneratingCSMs()
{ ShaderAPIImpl().EndGeneratingCSMs(); }
void IShaderAPI::PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{ ShaderAPIImpl().PerpareForCascadeDraw( cascade, fShadowSlopeScaleDepthBias, fShadowDepthBias ); }
void IShaderAPI::ForceDepthFuncEquals( bool bEnable )
{ ShaderAPIImpl().ForceDepthFuncEquals( bEnable ); }
void IShaderAPI::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{ ShaderAPIImpl().OverrideDepthEnable( bEnable, bDepthWriteEnable, bDepthTestEnable ); }
void IShaderAPI::SetHeightClipZ( float z )
{ ShaderAPIImpl().SetHeightClipZ( z ); }
void IShaderAPI::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{ ShaderAPIImpl().SetHeightClipMode( heightClipMode ); }
void IShaderAPI::SetClipPlane( int index, const float *pPlane )
{ ShaderAPIImpl().SetClipPlane( index, pPlane ); }
void IShaderAPI::EnableClipPlane( int index, bool bEnable )
{ ShaderAPIImpl().EnableClipPlane( index, bEnable ); }
ImageFormat IShaderAPI::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired ) const
{ return ShaderAPIImpl().GetNearestSupportedFormat( fmt, bFilteringRequired ); }
ImageFormat IShaderAPI::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{ return ShaderAPIImpl().GetNearestRenderTargetFormat( fmt ); }
bool IShaderAPI::DoRenderTargetsNeedSeparateDepthBuffer() const
{ return ShaderAPIImpl().DoRenderTargetsNeedSeparateDepthBuffer(); }
ShaderAPITextureHandle_t IShaderAPI::CreateTexture( int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName )
{ return ShaderAPIImpl().CreateTexture( width, height, depth, dstImageFormat, numMipLevels, numCopies, flags, pDebugName, pTextureGroupName ); }
void IShaderAPI::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().DeleteTexture( textureHandle ); }
ShaderAPITextureHandle_t IShaderAPI::CreateDepthTexture( ImageFormat renderTargetFormat, int width, int height, const char *pDebugName, bool bTexture, bool bAliasDepthSurfaceOverColorX360 )
{ return ShaderAPIImpl().CreateDepthTexture( renderTargetFormat, width, height, pDebugName, bTexture, bAliasDepthSurfaceOverColorX360 ); }
bool IShaderAPI::IsTexture( ShaderAPITextureHandle_t textureHandle )
{ return ShaderAPIImpl().IsTexture( textureHandle ); }
bool IShaderAPI::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{ return ShaderAPIImpl().IsTextureResident( textureHandle ); }
void IShaderAPI::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().ModifyTexture( textureHandle ); }
void IShaderAPI::TexImage2D( int level, int cubeFaceID, ImageFormat dstFormat, int zOffset, int width, int height, ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{ ShaderAPIImpl().TexImage2D( level, cubeFaceID, dstFormat, zOffset, width, height, srcFormat, bSrcIsTiled, imageData ); }
void IShaderAPI::TexSubImage2D( int level, int cubeFaceID, int xOffset, int yOffset, int zOffset, int width, int height, ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{ ShaderAPIImpl().TexSubImage2D( level, cubeFaceID, xOffset, yOffset, zOffset, width, height, srcFormat, srcStride, bSrcIsTiled, imageData ); }
bool IShaderAPI::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, int width, int height, CPixelWriter& writer )
{ return ShaderAPIImpl().TexLock( level, cubeFaceID, xOffset, yOffset, width, height, writer ); }
void IShaderAPI::TexUnlock()
{ ShaderAPIImpl().TexUnlock(); }
void IShaderAPI::UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture )
{ ShaderAPIImpl().UpdateTexture( xOffset, yOffset, w, h, hDstTexture, hSrcTexture ); }
void * IShaderAPI::LockTex( ShaderAPITextureHandle_t hTexture )
{ return ShaderAPIImpl().LockTex( hTexture ); }
void IShaderAPI::UnlockTex( ShaderAPITextureHandle_t hTexture )
{ ShaderAPIImpl().UnlockTex( hTexture ); }
void IShaderAPI::TexSetPriority( int priority )
{ ShaderAPIImpl().TexSetPriority( priority ); }
void IShaderAPI::BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().BindTexture( sampler, nBindFlags, textureHandle ); }
void IShaderAPI::SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{ ShaderAPIImpl().SetRenderTarget( colorTextureHandle, depthTextureHandle ); }
void IShaderAPI::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{ ShaderAPIImpl().ClearBuffersObeyStencil( bClearColor, bClearDepth ); }
void IShaderAPI::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{ ShaderAPIImpl().ReadPixels( x, y, width, height, data, dstFormat, pRenderTargetTexture ); }
void IShaderAPI::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{ ShaderAPIImpl().ReadPixelsAsync( x, y, width, height, data, dstFormat, pRenderTargetTexture, pPixelsReadEvent ); }
void IShaderAPI::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{ ShaderAPIImpl().ReadPixelsAsyncGetResult( x, y, width, height, data, dstFormat, pGetResultEvent ); }
void IShaderAPI::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{ ShaderAPIImpl().ReadPixels( pSrcRect, pDstRect, data, dstFormat, nDstStride ); }
void IShaderAPI::FlushHardware()
{ ShaderAPIImpl().FlushHardware(); }
void IShaderAPI::BeginFrame()
{ ShaderAPIImpl().BeginFrame(); }
void IShaderAPI::EndFrame()
{ ShaderAPIImpl().EndFrame(); }
int IShaderAPI::SelectionMode( bool selectionMode )
{ return ShaderAPIImpl().SelectionMode( selectionMode ); }
void IShaderAPI::SelectionBuffer( unsigned int* pBuffer, int size )
{ ShaderAPIImpl().SelectionBuffer( pBuffer, size ); }
void IShaderAPI::ClearSelectionNames()
{ ShaderAPIImpl().ClearSelectionNames(); }
void IShaderAPI::LoadSelectionName( int name )
{ ShaderAPIImpl().LoadSelectionName( name ); }
void IShaderAPI::PushSelectionName( int name )
{ ShaderAPIImpl().PushSelectionName( name ); }
void IShaderAPI::PopSelectionName()
{ ShaderAPIImpl().PopSelectionName(); }
void IShaderAPI::ForceHardwareSync()
{ ShaderAPIImpl().ForceHardwareSync(); }
void IShaderAPI::ClearSnapshots()
{ ShaderAPIImpl().ClearSnapshots(); }
void IShaderAPI::FogStart( float fStart )
{ ShaderAPIImpl().FogStart( fStart ); }
void IShaderAPI::FogEnd( float fEnd )
{ ShaderAPIImpl().FogEnd( fEnd ); }
void IShaderAPI::SetFogZ( float fogZ )
{ ShaderAPIImpl().SetFogZ( fogZ ); }
void IShaderAPI::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{ ShaderAPIImpl().SceneFogColor3ub( r, g, b ); }
void IShaderAPI::GetSceneFogColor( unsigned char *rgb )
{ ShaderAPIImpl().GetSceneFogColor( rgb ); }
void IShaderAPI::SceneFogMode( MaterialFogMode_t fogMode )
{ ShaderAPIImpl().SceneFogMode( fogMode ); }
bool IShaderAPI::CanDownloadTextures() const
{ return ShaderAPIImpl().CanDownloadTextures(); }
void IShaderAPI::ResetRenderState( bool bFullReset )
{ ShaderAPIImpl().ResetRenderState( bFullReset ); }
int IShaderAPI::GetCurrentDynamicVBSize( void )
{ return ShaderAPIImpl().GetCurrentDynamicVBSize(); }
void IShaderAPI::DestroyVertexBuffers( bool bExitingLevel )
{ ShaderAPIImpl().DestroyVertexBuffers( bExitingLevel ); }
void IShaderAPI::EvictManagedResources()
{ ShaderAPIImpl().EvictManagedResources(); }
void IShaderAPI::GetGPUMemoryStats( GPUMemoryStats &stats )
{ ShaderAPIImpl().GetGPUMemoryStats( stats ); }
void IShaderAPI::SetAnisotropicLevel( int nAnisotropyLevel )
{ ShaderAPIImpl().SetAnisotropicLevel( nAnisotropyLevel ); }
void IShaderAPI::SyncToken( const char *pToken )
{ ShaderAPIImpl().SyncToken( pToken ); }
void IShaderAPI::SetStandardVertexShaderConstants( float fOverbright )
{ ShaderAPIImpl().SetStandardVertexShaderConstants( fOverbright ); }
ShaderAPIOcclusionQuery_t IShaderAPI::CreateOcclusionQueryObject( void )
{ return ShaderAPIImpl().CreateOcclusionQueryObject(); }
void IShaderAPI::DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t a0 )
{ ShaderAPIImpl().DestroyOcclusionQueryObject( a0 ); }
void IShaderAPI::BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t a0 )
{ ShaderAPIImpl().BeginOcclusionQueryDrawing( a0 ); }
void IShaderAPI::EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t a0 )
{ ShaderAPIImpl().EndOcclusionQueryDrawing( a0 ); }
int IShaderAPI::OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t hQuery, bool bFlush )
{ return ShaderAPIImpl().OcclusionQuery_GetNumPixelsRendered( hQuery, bFlush ); }
void IShaderAPI::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{ ShaderAPIImpl().SetFlashlightState( state, worldToTexture ); }
bool IShaderAPI::IsCascadedShadowMapping() const
{ return ShaderAPIImpl().IsCascadedShadowMapping(); }
void IShaderAPI::SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
{ ShaderAPIImpl().SetCascadedShadowMappingState( state, pDepthTextureAtlas ); }
const CascadedShadowMappingState_t & IShaderAPI::GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale ) const
{ return ShaderAPIImpl().GetCascadedShadowMappingState( pDepthTextureAtlas, bLightMapScale ); }
void IShaderAPI::ClearVertexAndPixelShaderRefCounts()
{ ShaderAPIImpl().ClearVertexAndPixelShaderRefCounts(); }
void IShaderAPI::PurgeUnusedVertexAndPixelShaders()
{ ShaderAPIImpl().PurgeUnusedVertexAndPixelShaders(); }
void IShaderAPI::DXSupportLevelChanged( int nDXLevel )
{ ShaderAPIImpl().DXSupportLevelChanged( nDXLevel ); }
void IShaderAPI::EnableUserClipTransformOverride( bool bEnable )
{ ShaderAPIImpl().EnableUserClipTransformOverride( bEnable ); }
void IShaderAPI::UserClipTransform( const VMatrix &worldToView )
{ ShaderAPIImpl().UserClipTransform( worldToView ); }
void IShaderAPI::SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{ ShaderAPIImpl().SetRenderTargetEx( nRenderTargetID, colorTextureHandle, depthTextureHandle ); }
void IShaderAPI::CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{ ShaderAPIImpl().CopyRenderTargetToTextureEx( textureHandle, nRenderTargetID, pSrcRect, pDstRect ); }
void IShaderAPI::CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect, Rect_t *pDstRect )
{ ShaderAPIImpl().CopyTextureToRenderTargetEx( nRenderTargetID, textureHandle, pSrcRect, pDstRect ); }
void IShaderAPI::HandleDeviceLost()
{ ShaderAPIImpl().HandleDeviceLost(); }
void IShaderAPI::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{ ShaderAPIImpl().EnableLinearColorSpaceFrameBuffer( bEnable ); }
void IShaderAPI::SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
{ ShaderAPIImpl().SetFullScreenTextureHandle( h ); }
void IShaderAPI::SetFloatRenderingParameter( int parm_number, float value )
{ ShaderAPIImpl().SetFloatRenderingParameter( parm_number, value ); }
void IShaderAPI::SetIntRenderingParameter( int parm_number, int value )
{ ShaderAPIImpl().SetIntRenderingParameter( parm_number, value ); }
void IShaderAPI::SetVectorRenderingParameter( int parm_number, Vector const &value )
{ ShaderAPIImpl().SetVectorRenderingParameter( parm_number, value ); }
float IShaderAPI::GetFloatRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetFloatRenderingParameter( parm_number ); }
int IShaderAPI::GetIntRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetIntRenderingParameter( parm_number ); }
Vector IShaderAPI::GetVectorRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetVectorRenderingParameter( parm_number ); }
void IShaderAPI::SetFastClipPlane( const float *pPlane )
{ ShaderAPIImpl().SetFastClipPlane( pPlane ); }
void IShaderAPI::EnableFastClip( bool bEnable )
{ ShaderAPIImpl().EnableFastClip( bEnable ); }
void IShaderAPI::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{ ShaderAPIImpl().GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices ); }
int IShaderAPI::GetMaxVerticesToRender( IMaterial *pMaterial )
{ return ShaderAPIImpl().GetMaxVerticesToRender( pMaterial ); }
int IShaderAPI::GetMaxIndicesToRender()
{ return ShaderAPIImpl().GetMaxIndicesToRender(); }
void IShaderAPI::SetStencilState( const ShaderStencilState_t& state )
{ ShaderAPIImpl().SetStencilState( state ); }
void IShaderAPI::ClearStencilBufferRectangle( int xmin, int ymin, int xmax, int ymax, int value )
{ ShaderAPIImpl().ClearStencilBufferRectangle( xmin, ymin, xmax, ymax, value ); }
void IShaderAPI::DisableAllLocalLights()
{ ShaderAPIImpl().DisableAllLocalLights(); }
int IShaderAPI::CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 )
{ return ShaderAPIImpl().CompareSnapshots( snapshot0, snapshot1 ); }
IMesh * IShaderAPI::GetFlexMesh()
{ return ShaderAPIImpl().GetFlexMesh(); }
void IShaderAPI::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{ ShaderAPIImpl().SetFlashlightStateEx( state, worldToTexture, pFlashlightDepthTexture ); }
bool IShaderAPI::SupportsMSAAMode( int nMSAAMode )
{ return ShaderAPIImpl().SupportsMSAAMode( nMSAAMode ); }
void IShaderAPI::AntiAliasingHint( int nHint )
{ ShaderAPIImpl().AntiAliasingHint( nHint ); }
bool IShaderAPI::OwnGPUResources( bool bEnable )
{ return ShaderAPIImpl().OwnGPUResources( bEnable ); }
void IShaderAPI::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{ ShaderAPIImpl().GetFogDistances( fStart, fEnd, fFogZ ); }
void IShaderAPI::BeginPIXEvent( unsigned long color, const char *szName )
{ ShaderAPIImpl().BeginPIXEvent( color, szName ); }
void IShaderAPI::EndPIXEvent()
{ ShaderAPIImpl().EndPIXEvent(); }
void IShaderAPI::SetPIXMarker( unsigned long color, const char *szName )
{ ShaderAPIImpl().SetPIXMarker( color, szName ); }
void IShaderAPI::EnableAlphaToCoverage()
{ ShaderAPIImpl().EnableAlphaToCoverage(); }
void IShaderAPI::DisableAlphaToCoverage()
{ ShaderAPIImpl().DisableAlphaToCoverage(); }
void IShaderAPI::ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const
{ ShaderAPIImpl().ComputeVertexDescription( pBuffer, vertexFormat, desc ); }
int IShaderAPI::VertexFormatSize( VertexFormat_t vertexFormat ) const
{ return ShaderAPIImpl().VertexFormatSize( vertexFormat ); }
void IShaderAPI::SetDisallowAccess( bool a0 )
{ ShaderAPIImpl().SetDisallowAccess( a0 ); }
void IShaderAPI::EnableShaderShaderMutex( bool a0 )
{ ShaderAPIImpl().EnableShaderShaderMutex( a0 ); }
void IShaderAPI::ShaderLock()
{ ShaderAPIImpl().ShaderLock(); }
void IShaderAPI::ShaderUnlock()
{ ShaderAPIImpl().ShaderUnlock(); }
void IShaderAPI::SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{ ShaderAPIImpl().SetShadowDepthBiasFactors( fShadowSlopeScaleDepthBias, fShadowDepthBias ); }
void IShaderAPI::BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{ ShaderAPIImpl().BindVertexBuffer( nStreamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions ); }
void IShaderAPI::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{ ShaderAPIImpl().BindIndexBuffer( pIndexBuffer, nOffsetInBytes ); }
void IShaderAPI::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{ ShaderAPIImpl().Draw( primitiveType, nFirstIndex, nIndexCount ); }
void IShaderAPI::PerformFullScreenStencilOperation( void )
{ ShaderAPIImpl().PerformFullScreenStencilOperation(); }
void IShaderAPI::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{ ShaderAPIImpl().SetScissorRect( nLeft, nTop, nRight, nBottom, bEnableScissor ); }
bool IShaderAPI::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{ return ShaderAPIImpl().SupportsCSAAMode( nNumSamples, nQualityLevel ); }
void IShaderAPI::InvalidateDelayedShaderConstants( void )
{ ShaderAPIImpl().InvalidateDelayedShaderConstants(); }
float IShaderAPI::GammaToLinear_HardwareSpecific( float fGamma ) const
{ return ShaderAPIImpl().GammaToLinear_HardwareSpecific( fGamma ); }
float IShaderAPI::LinearToGamma_HardwareSpecific( float fLinear ) const
{ return ShaderAPIImpl().LinearToGamma_HardwareSpecific( fLinear ); }
void IShaderAPI::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{ ShaderAPIImpl().SetLinearToGammaConversionTextures( hSRGBWriteEnabledTexture, hIdentityTexture ); }
void IShaderAPI::BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().BindVertexTexture( nSampler, textureHandle ); }
void IShaderAPI::EnableHWMorphing( bool bEnable )
{ ShaderAPIImpl().EnableHWMorphing( bEnable ); }
void IShaderAPI::SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
{ ShaderAPIImpl().SetFlexWeights( nFirstWeight, nCount, pWeights ); }
void IShaderAPI::FogMaxDensity( float flMaxDensity )
{ ShaderAPIImpl().FogMaxDensity( flMaxDensity ); }
void * IShaderAPI::GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture )
{ return ShaderAPIImpl().GetD3DTexturePtr( hTexture ); }
bool IShaderAPI::IsStandardTextureHandleValid( StandardTextureId_t textureId )
{ return ShaderAPIImpl().IsStandardTextureHandleValid( textureId ); }
void IShaderAPI::CreateTextures( ShaderAPITextureHandle_t *pHandles, int count, int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName )
{ ShaderAPIImpl().CreateTextures( pHandles, count, width, height, depth, dstImageFormat, numMipLevels, numCopies, flags, pDebugName, pTextureGroupName ); }
void IShaderAPI::AcquireThreadOwnership()
{ ShaderAPIImpl().AcquireThreadOwnership(); }
void IShaderAPI::ReleaseThreadOwnership()
{ ShaderAPIImpl().ReleaseThreadOwnership(); }
void IShaderAPI::EnableBuffer2FramesAhead( bool bEnable )
{ ShaderAPIImpl().EnableBuffer2FramesAhead( bEnable ); }
void IShaderAPI::FlipCulling( bool bFlipCulling )
{ ShaderAPIImpl().FlipCulling( bFlipCulling ); }
void IShaderAPI::SetTextureRenderingParameter( int parm_number, ITexture *pTexture )
{ ShaderAPIImpl().SetTextureRenderingParameter( parm_number, pTexture ); }
void IShaderAPI::EnableSinglePassFlashlightMode( bool bEnable )
{ ShaderAPIImpl().EnableSinglePassFlashlightMode( bEnable ); }
void IShaderAPI::MatrixMode( MaterialMatrixMode_t matrixMode )
{ ShaderAPIImpl().MatrixMode( matrixMode ); }
void IShaderAPI::PushMatrix()
{ ShaderAPIImpl().PushMatrix(); }
void IShaderAPI::PopMatrix()
{ ShaderAPIImpl().PopMatrix(); }
void IShaderAPI::LoadMatrix( float *m )
{ ShaderAPIImpl().LoadMatrix( m ); }
void IShaderAPI::MultMatrix( float *m )
{ ShaderAPIImpl().MultMatrix( m ); }
void IShaderAPI::MultMatrixLocal( float *m )
{ ShaderAPIImpl().MultMatrixLocal( m ); }
void IShaderAPI::LoadIdentity( void )
{ ShaderAPIImpl().LoadIdentity(); }
void IShaderAPI::LoadCameraToWorld( void )
{ ShaderAPIImpl().LoadCameraToWorld(); }
void IShaderAPI::Ortho( double left, double right, double bottom, double top, double zNear, double zFar )
{ ShaderAPIImpl().Ortho( left, right, bottom, top, zNear, zFar ); }
void IShaderAPI::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{ ShaderAPIImpl().PerspectiveX( fovx, aspect, zNear, zFar ); }
void IShaderAPI::PickMatrix( int x, int y, int width, int height )
{ ShaderAPIImpl().PickMatrix( x, y, width, height ); }
void IShaderAPI::Rotate( float angle, float x, float y, float z )
{ ShaderAPIImpl().Rotate( angle, x, y, z ); }
void IShaderAPI::Translate( float x, float y, float z )
{ ShaderAPIImpl().Translate( x, y, z ); }
void IShaderAPI::Scale( float x, float y, float z )
{ ShaderAPIImpl().Scale( x, y, z ); }
void IShaderAPI::ScaleXY( float x, float y )
{ ShaderAPIImpl().ScaleXY( x, y ); }
void IShaderAPI::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{ ShaderAPIImpl().PerspectiveOffCenterX( fovx, aspect, zNear, zFar, bottom, top, left, right ); }
void IShaderAPI::LoadBoneMatrix( int boneIndex, const float *m )
{ ShaderAPIImpl().LoadBoneMatrix( boneIndex, m ); }
void IShaderAPI::SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle )
{ ShaderAPIImpl().SetStandardTextureHandle( nId, nHandle ); }
void IShaderAPI::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
{ ShaderAPIImpl().DrawInstances( nInstanceCount, pInstance ); }
void IShaderAPI::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{ ShaderAPIImpl().OverrideAlphaWriteEnable( bOverrideEnable, bAlphaWriteEnable ); }
void IShaderAPI::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{ ShaderAPIImpl().OverrideColorWriteEnable( bOverrideEnable, bColorWriteEnable ); }
void IShaderAPI::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{ ShaderAPIImpl().ClearBuffersObeyStencilEx( bClearColor, bClearAlpha, bClearDepth ); }
void IShaderAPI::OnPresent( void )
{ ShaderAPIImpl().OnPresent(); }
void IShaderAPI::UpdateGameTime( float flTime )
{ ShaderAPIImpl().UpdateGameTime( flTime ); }
bool IShaderAPI::IsStereoSupported() const
{ return ShaderAPIImpl().IsStereoSupported(); }
void IShaderAPI::UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame )
{ ShaderAPIImpl().UpdateStereoTexture( texHandle, pStereoActiveThisFrame ); }
void IShaderAPI::SetSRGBWrite( bool bState )
{ ShaderAPIImpl().SetSRGBWrite( bState ); }
void IShaderAPI::PrintfVA( char *fmt, std::va_list vargs )
{ ShaderAPIImpl().PrintfVA( fmt, vargs ); }
float IShaderAPI::Knob( char *knobname, float *setvalue )
{ return ShaderAPIImpl().Knob( knobname, setvalue ); }
void IShaderAPI::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{ ShaderAPIImpl().AddShaderComboInformation( pSemantics ); }
float IShaderAPI::GetLightMapScaleFactor() const
{ return ShaderAPIImpl().GetLightMapScaleFactor(); }
ShaderAPITextureHandle_t IShaderAPI::FindTexture( const char *pDebugName )
{ return ShaderAPIImpl().FindTexture( pDebugName ); }
void IShaderAPI::GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth )
{ ShaderAPIImpl().GetTextureDimensions( hTexture, nWidth, nHeight, nDepth ); }
ShaderAPITextureHandle_t IShaderAPI::GetStandardTextureHandle( StandardTextureId_t id )
{ return ShaderAPIImpl().GetStandardTextureHandle( id ); }

