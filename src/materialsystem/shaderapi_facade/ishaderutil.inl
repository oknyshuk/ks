import ks.core.types;
import ks.math.vector4d;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderUtil facade class to the material system.
// Included by the TU that defines the implementation, which must declare ShaderUtilImpl() returning it.

// IShaderUtil
MaterialSystem_Config_t& IShaderUtil::GetConfig()
{ return ShaderUtilImpl().GetConfig(); }
bool IShaderUtil::ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat, unsigned char *dst, enum ImageFormat dstImageFormat, int width, int height, int srcStride, int dstStride )
{ return ShaderUtilImpl().ConvertImageFormat( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride ); }
int IShaderUtil::GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap )
{ return ShaderUtilImpl().GetMemRequired( width, height, depth, format, mipmap ); }
const ImageFormatInfo_t& IShaderUtil::ImageFormatInfo( ImageFormat fmt ) const
{ return ShaderUtilImpl().ImageFormatInfo( fmt ); }
void IShaderUtil::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{ ShaderUtilImpl().BindStandardTexture( sampler, nBindFlags, id ); }
void IShaderUtil::GetLightmapDimensions( int *w, int *h )
{ ShaderUtilImpl().GetLightmapDimensions( w, h ); }
void IShaderUtil::ReleaseShaderObjects( int nChangeFlags )
{ ShaderUtilImpl().ReleaseShaderObjects( nChangeFlags ); }
void IShaderUtil::RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags )
{ ShaderUtilImpl().RestoreShaderObjects( shaderFactory, nChangeFlags ); }
bool IShaderUtil::InFlashlightMode() const
{ return ShaderUtilImpl().InFlashlightMode(); }
bool IShaderUtil::IsCascadedShadowMapping() const
{ return ShaderUtilImpl().IsCascadedShadowMapping(); }
void IShaderUtil::NoteAnisotropicLevel( int currentLevel )
{ ShaderUtilImpl().NoteAnisotropicLevel( currentLevel ); }
bool IShaderUtil::InEditorMode() const
{ return ShaderUtilImpl().InEditorMode(); }
ITexture * IShaderUtil::GetRenderTargetEx( int nRenderTargetID )
{ return ShaderUtilImpl().GetRenderTargetEx( nRenderTargetID ); }
void IShaderUtil::DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth )
{ ShaderUtilImpl().DrawClearBufferQuad( r, g, b, a, bClearColor, bClearAlpha, bClearDepth ); }
bool IShaderUtil::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{ return ShaderUtilImpl().OnDrawMesh( pMesh, firstIndex, numIndices ); }
bool IShaderUtil::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{ return ShaderUtilImpl().OnDrawMesh( pMesh, pLists, nLists ); }
bool IShaderUtil::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{ return ShaderUtilImpl().OnSetFlexMesh( pStaticMesh, pMesh, nVertexOffsetInBytes ); }
bool IShaderUtil::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{ return ShaderUtilImpl().OnSetColorMesh( pStaticMesh, pMesh, nVertexOffsetInBytes ); }
bool IShaderUtil::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{ return ShaderUtilImpl().OnSetPrimitiveType( pMesh, type ); }
void IShaderUtil::SyncMatrices()
{ ShaderUtilImpl().SyncMatrices(); }
void IShaderUtil::SyncMatrix( MaterialMatrixMode_t a0 )
{ ShaderUtilImpl().SyncMatrix( a0 ); }
void IShaderUtil::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{ ShaderUtilImpl().BindStandardVertexTexture( sampler, id ); }
void IShaderUtil::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{ ShaderUtilImpl().GetStandardTextureDimensions( pWidth, pHeight, id ); }
int IShaderUtil::MaxHWMorphBatchCount() const
{ return ShaderUtilImpl().MaxHWMorphBatchCount(); }
void IShaderUtil::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{ ShaderUtilImpl().GetCurrentColorCorrection( pInfo ); }
ShaderAPITextureHandle_t IShaderUtil::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel )
{ return ShaderUtilImpl().GetShaderAPITextureBindHandle( pTexture, nFrame, nTextureChannel ); }
float IShaderUtil::GetSubDHeight()
{ return ShaderUtilImpl().GetSubDHeight(); }
bool IShaderUtil::OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices )
{ return ShaderUtilImpl().OnDrawMeshModulated( pMesh, diffuseModulation, firstIndex, numIndices ); }
void IShaderUtil::OnThreadEvent( uint32 threadEvent )
{ ShaderUtilImpl().OnThreadEvent( threadEvent ); }
MaterialThreadMode_t IShaderUtil::GetThreadMode()
{ return ShaderUtilImpl().GetThreadMode(); }
bool IShaderUtil::IsRenderThreadSafe()
{ return ShaderUtilImpl().IsRenderThreadSafe(); }
void IShaderUtil::UncacheUnusedMaterials( bool bRecomputeStateSnapshots )
{ ShaderUtilImpl().UncacheUnusedMaterials( bRecomputeStateSnapshots ); }
bool IShaderUtil::IsInFrame() const
{ return ShaderUtilImpl().IsInFrame(); }
ShaderAPITextureHandle_t IShaderUtil::GetLightmapTexture( ShaderAPITextureHandle_t nLightmapPage )
{ return ShaderUtilImpl().GetLightmapTexture( nLightmapPage ); }
bool IShaderUtil::IsRenderingPaint() const
{ return ShaderUtilImpl().IsRenderingPaint(); }
ShaderAPITextureHandle_t IShaderUtil::GetPaintmapTexture( ShaderAPITextureHandle_t nLightmapPage )
{ return ShaderUtilImpl().GetPaintmapTexture( nLightmapPage ); }
ShaderAPITextureHandle_t IShaderUtil::GetStandardTexture( StandardTextureId_t id )
{ return ShaderUtilImpl().GetStandardTexture( id ); }

