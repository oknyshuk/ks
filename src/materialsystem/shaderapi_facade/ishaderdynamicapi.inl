import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vmatrix;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderDynamicAPI facade class to the shader API implementation.
// Included by the TU that defines the implementation, which must declare ShaderAPIImpl() returning it.

// IShaderDynamicAPI
void IShaderDynamicAPI::SetVertexShaderViewProj()
{ ShaderAPIImpl().SetVertexShaderViewProj(); }
void IShaderDynamicAPI::UpdateVertexShaderMatrix( int m )
{ ShaderAPIImpl().UpdateVertexShaderMatrix( m ); }
void IShaderDynamicAPI::SetVertexShaderModelViewProjAndModelView()
{ ShaderAPIImpl().SetVertexShaderModelViewProjAndModelView(); }
void IShaderDynamicAPI::SetVertexShaderCameraPos()
{ ShaderAPIImpl().SetVertexShaderCameraPos(); }
bool IShaderDynamicAPI::SetSkinningMatrices( const MeshInstanceData_t &instance )
{ return ShaderAPIImpl().SetSkinningMatrices( instance ); }
void IShaderDynamicAPI::BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{ ShaderAPIImpl().BindTexture( sampler, nBindFlags, textureHandle ); }
double IShaderDynamicAPI::CurrentTime() const
{ return ShaderAPIImpl().CurrentTime(); }
void IShaderDynamicAPI::GetLightmapDimensions( int *w, int *h )
{ ShaderAPIImpl().GetLightmapDimensions( w, h ); }
MaterialFogMode_t IShaderDynamicAPI::GetSceneFogMode()
{ return ShaderAPIImpl().GetSceneFogMode(); }
void IShaderDynamicAPI::GetSceneFogColor( unsigned char *rgb )
{ ShaderAPIImpl().GetSceneFogColor( rgb ); }
void IShaderDynamicAPI::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{ ShaderAPIImpl().SetVertexShaderConstant( var, pVec, numConst, bForce ); }
void IShaderDynamicAPI::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{ ShaderAPIImpl().SetPixelShaderConstant( var, pVec, numConst, bForce ); }
void IShaderDynamicAPI::SetDefaultState()
{ ShaderAPIImpl().SetDefaultState(); }
void IShaderDynamicAPI::GetWorldSpaceCameraPosition( float* pPos ) const
{ ShaderAPIImpl().GetWorldSpaceCameraPosition( pPos ); }
void IShaderDynamicAPI::GetWorldSpaceCameraDirection( float* pDir ) const
{ ShaderAPIImpl().GetWorldSpaceCameraDirection( pDir ); }
int IShaderDynamicAPI::GetCurrentNumBones( void ) const
{ return ShaderAPIImpl().GetCurrentNumBones(); }
MaterialFogMode_t IShaderDynamicAPI::GetCurrentFogType( void ) const
{ return ShaderAPIImpl().GetCurrentFogType(); }
void IShaderDynamicAPI::SetVertexShaderIndex( int vshIndex )
{ ShaderAPIImpl().SetVertexShaderIndex( vshIndex ); }
void IShaderDynamicAPI::SetPixelShaderIndex( int pshIndex )
{ ShaderAPIImpl().SetPixelShaderIndex( pshIndex ); }
void IShaderDynamicAPI::GetBackBufferDimensions( int& width, int& height ) const
{ ShaderAPIImpl().GetBackBufferDimensions( width, height ); }
const AspectRatioInfo_t & IShaderDynamicAPI::GetAspectRatioInfo( void ) const
{ return ShaderAPIImpl().GetAspectRatioInfo(); }
void IShaderDynamicAPI::GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight ) const
{ ShaderAPIImpl().GetCurrentRenderTargetDimensions( nWidth, nHeight ); }
void IShaderDynamicAPI::GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight ) const
{ ShaderAPIImpl().GetCurrentViewport( nX, nY, nWidth, nHeight ); }
void IShaderDynamicAPI::SetPixelShaderFogParams( int reg )
{ ShaderAPIImpl().SetPixelShaderFogParams( reg ); }
bool IShaderDynamicAPI::InFlashlightMode() const
{ return ShaderAPIImpl().InFlashlightMode(); }
const FlashlightState_t & IShaderDynamicAPI::GetFlashlightState( VMatrix &worldToTexture ) const
{ return ShaderAPIImpl().GetFlashlightState( worldToTexture ); }
bool IShaderDynamicAPI::InEditorMode() const
{ return ShaderAPIImpl().InEditorMode(); }
bool IShaderDynamicAPI::IsCascadedShadowMapping() const
{ return ShaderAPIImpl().IsCascadedShadowMapping(); }
const CascadedShadowMappingState_t & IShaderDynamicAPI::GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale ) const
{ return ShaderAPIImpl().GetCascadedShadowMappingState( pDepthTextureAtlas, bLightMapScale ); }
void IShaderDynamicAPI::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{ ShaderAPIImpl().BindStandardTexture( sampler, nBindFlags, id ); }
ITexture * IShaderDynamicAPI::GetRenderTargetEx( int nRenderTargetID ) const
{ return ShaderAPIImpl().GetRenderTargetEx( nRenderTargetID ); }
void IShaderDynamicAPI::SetToneMappingScaleLinear( const Vector &scale )
{ ShaderAPIImpl().SetToneMappingScaleLinear( scale ); }
const Vector & IShaderDynamicAPI::GetToneMappingScaleLinear( void ) const
{ return ShaderAPIImpl().GetToneMappingScaleLinear(); }
void IShaderDynamicAPI::SetFloatRenderingParameter( int parm_number, float value )
{ ShaderAPIImpl().SetFloatRenderingParameter( parm_number, value ); }
void IShaderDynamicAPI::SetIntRenderingParameter( int parm_number, int value )
{ ShaderAPIImpl().SetIntRenderingParameter( parm_number, value ); }
void IShaderDynamicAPI::SetVectorRenderingParameter( int parm_number, Vector const &value )
{ ShaderAPIImpl().SetVectorRenderingParameter( parm_number, value ); }
float IShaderDynamicAPI::GetFloatRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetFloatRenderingParameter( parm_number ); }
int IShaderDynamicAPI::GetIntRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetIntRenderingParameter( parm_number ); }
Vector IShaderDynamicAPI::GetVectorRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetVectorRenderingParameter( parm_number ); }
const FlashlightState_t & IShaderDynamicAPI::GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const
{ return ShaderAPIImpl().GetFlashlightStateEx( worldToTexture, pFlashlightDepthTexture ); }
void IShaderDynamicAPI::GetDX9LightState( LightState_t *state ) const
{ ShaderAPIImpl().GetDX9LightState( state ); }
int IShaderDynamicAPI::GetPixelFogCombo()
{ return ShaderAPIImpl().GetPixelFogCombo(); }
void IShaderDynamicAPI::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{ ShaderAPIImpl().BindStandardVertexTexture( sampler, id ); }
bool IShaderDynamicAPI::IsHWMorphingEnabled() const
{ return ShaderAPIImpl().IsHWMorphingEnabled(); }
void IShaderDynamicAPI::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{ ShaderAPIImpl().GetStandardTextureDimensions( pWidth, pHeight, id ); }
void IShaderDynamicAPI::SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce )
{ ShaderAPIImpl().SetBooleanVertexShaderConstant( var, pVec, numBools, bForce ); }
void IShaderDynamicAPI::SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{ ShaderAPIImpl().SetIntegerVertexShaderConstant( var, pVec, numIntVecs, bForce ); }
void IShaderDynamicAPI::SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce )
{ ShaderAPIImpl().SetBooleanPixelShaderConstant( var, pVec, numBools, bForce ); }
void IShaderDynamicAPI::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{ ShaderAPIImpl().SetIntegerPixelShaderConstant( var, pVec, numIntVecs, bForce ); }
bool IShaderDynamicAPI::ShouldWriteDepthToDestAlpha( void ) const
{ return ShaderAPIImpl().ShouldWriteDepthToDestAlpha(); }
void IShaderDynamicAPI::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{ ShaderAPIImpl().GetMatrix( matrixMode, dst ); }
void IShaderDynamicAPI::PushDeformation( DeformationBase_t const *Deformation )
{ ShaderAPIImpl().PushDeformation( Deformation ); }
void IShaderDynamicAPI::PopDeformation()
{ ShaderAPIImpl().PopDeformation(); }
int IShaderDynamicAPI::GetNumActiveDeformations() const
{ return ShaderAPIImpl().GetNumActiveDeformations(); }
int IShaderDynamicAPI::GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations, float *pConstantValuesOut, int nBufferSize, int nMaximumDeformations, int *pNumDefsOut ) const
{ return ShaderAPIImpl().GetPackedDeformationInformation( nMaskOfUnderstoodDeformations, pConstantValuesOut, nBufferSize, nMaximumDeformations, pNumDefsOut ); }
void IShaderDynamicAPI::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{ ShaderAPIImpl().MarkUnusedVertexFields( nFlags, nTexCoordCount, pUnusedTexCoords ); }
void IShaderDynamicAPI::ExecuteCommandBuffer( uint8 *pCmdBuffer )
{ ShaderAPIImpl().ExecuteCommandBuffer( pCmdBuffer ); }
void IShaderDynamicAPI::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{ ShaderAPIImpl().GetCurrentColorCorrection( pInfo ); }
ITexture * IShaderDynamicAPI::GetTextureRenderingParameter( int parm_number ) const
{ return ShaderAPIImpl().GetTextureRenderingParameter( parm_number ); }
void IShaderDynamicAPI::SetScreenSizeForVPOS( int pshReg )
{ ShaderAPIImpl().SetScreenSizeForVPOS( pshReg ); }
void IShaderDynamicAPI::SetVSNearAndFarZ( int vshReg )
{ ShaderAPIImpl().SetVSNearAndFarZ( vshReg ); }
float IShaderDynamicAPI::GetFarZ()
{ return ShaderAPIImpl().GetFarZ(); }
bool IShaderDynamicAPI::SinglePassFlashlightModeEnabled( void )
{ return ShaderAPIImpl().SinglePassFlashlightModeEnabled(); }
void IShaderDynamicAPI::GetActualProjectionMatrix( float *pMatrix )
{ ShaderAPIImpl().GetActualProjectionMatrix( pMatrix ); }
void IShaderDynamicAPI::SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale )
{ ShaderAPIImpl().SetDepthFeatheringShaderConstants( iConstant, fDepthBlendScale ); }
void IShaderDynamicAPI::GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const
{ ShaderAPIImpl().GetFlashlightShaderInfo( pShadowsEnabled, pUberLight ); }
float IShaderDynamicAPI::GetFlashlightAmbientOcclusion() const
{ return ShaderAPIImpl().GetFlashlightAmbientOcclusion(); }
void IShaderDynamicAPI::SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode )
{ ShaderAPIImpl().SetTextureFilterMode( sampler, nMode ); }
TessellationMode_t IShaderDynamicAPI::GetTessellationMode() const
{ return ShaderAPIImpl().GetTessellationMode(); }
float IShaderDynamicAPI::GetSubDHeight()
{ return ShaderAPIImpl().GetSubDHeight(); }
bool IShaderDynamicAPI::IsRenderingPaint() const
{ return ShaderAPIImpl().IsRenderingPaint(); }
bool IShaderDynamicAPI::IsStereoActiveThisFrame() const
{ return ShaderAPIImpl().IsStereoActiveThisFrame(); }
bool IShaderDynamicAPI::IsStandardTextureHandleValid( StandardTextureId_t textureId )
{ return ShaderAPIImpl().IsStandardTextureHandleValid( textureId ); }

