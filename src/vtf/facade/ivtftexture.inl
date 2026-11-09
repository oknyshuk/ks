// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVTFTexture class to CVTFTexture. Included by the source that defines CVTFTexture.

bool IVTFTexture::Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, int nFlags, int iFrameCount, int nForceMipCount )
{ return static_cast<CVTFTexture*>(this)->Init( nWidth, nHeight, nDepth, fmt, nFlags, iFrameCount, nForceMipCount ); }
void IVTFTexture::SetBumpScale( float flScale )
{ static_cast<CVTFTexture*>(this)->SetBumpScale( flScale ); }
void IVTFTexture::SetReflectivity( const Vector &vecReflectivity )
{ static_cast<CVTFTexture*>(this)->SetReflectivity( vecReflectivity ); }
void IVTFTexture::InitLowResImage( int nWidth, int nHeight, ImageFormat fmt )
{ static_cast<CVTFTexture*>(this)->InitLowResImage( nWidth, nHeight, fmt ); }
void * IVTFTexture::SetResourceData( uint32 eType, void const *pData, std::size_t nDataSize )
{ return static_cast<CVTFTexture*>(this)->SetResourceData( eType, pData, nDataSize ); }
void * IVTFTexture::GetResourceData( uint32 eType, std::size_t *pDataSize ) const
{ return static_cast<const CVTFTexture*>(this)->GetResourceData( eType, pDataSize ); }
bool IVTFTexture::HasResourceEntry( uint32 eType ) const
{ return static_cast<const CVTFTexture*>(this)->HasResourceEntry( eType ); }
unsigned int IVTFTexture::GetResourceTypes( uint32 *arrTypesBuffer, int numTypesBufferElems ) const
{ return static_cast<const CVTFTexture*>(this)->GetResourceTypes( arrTypesBuffer, numTypesBufferElems ); }
bool IVTFTexture::Unserialize( ks::byte_reader &buf, bool bHeaderOnly, int nSkipMipLevels )
{ return static_cast<CVTFTexture*>(this)->Unserialize( buf, bHeaderOnly, nSkipMipLevels ); }
bool IVTFTexture::Serialize( ks::byte_writer &buf )
{ return static_cast<CVTFTexture*>(this)->Serialize( buf ); }
void IVTFTexture::LowResFileInfo( int *pStartLocation, int *pSizeInBytes ) const
{ static_cast<const CVTFTexture*>(this)->LowResFileInfo( pStartLocation, pSizeInBytes ); }
void IVTFTexture::ImageFileInfo( int nFrame, int nFace, int nMip, int *pStartLocation, int *pSizeInBytes ) const
{ static_cast<const CVTFTexture*>(this)->ImageFileInfo( nFrame, nFace, nMip, pStartLocation, pSizeInBytes ); }
int IVTFTexture::FileSize( int nMipSkipCount ) const
{ return static_cast<const CVTFTexture*>(this)->FileSize( nMipSkipCount ); }
int IVTFTexture::Width() const
{ return static_cast<const CVTFTexture*>(this)->Width(); }
int IVTFTexture::Height() const
{ return static_cast<const CVTFTexture*>(this)->Height(); }
int IVTFTexture::Depth() const
{ return static_cast<const CVTFTexture*>(this)->Depth(); }
int IVTFTexture::MipCount() const
{ return static_cast<const CVTFTexture*>(this)->MipCount(); }
int IVTFTexture::RowSizeInBytes( int nMipLevel ) const
{ return static_cast<const CVTFTexture*>(this)->RowSizeInBytes( nMipLevel ); }
int IVTFTexture::FaceSizeInBytes( int nMipLevel ) const
{ return static_cast<const CVTFTexture*>(this)->FaceSizeInBytes( nMipLevel ); }
ImageFormat IVTFTexture::Format() const
{ return static_cast<const CVTFTexture*>(this)->Format(); }
int IVTFTexture::FaceCount() const
{ return static_cast<const CVTFTexture*>(this)->FaceCount(); }
int IVTFTexture::FrameCount() const
{ return static_cast<const CVTFTexture*>(this)->FrameCount(); }
int IVTFTexture::Flags() const
{ return static_cast<const CVTFTexture*>(this)->Flags(); }
float IVTFTexture::BumpScale() const
{ return static_cast<const CVTFTexture*>(this)->BumpScale(); }
int IVTFTexture::LowResWidth() const
{ return static_cast<const CVTFTexture*>(this)->LowResWidth(); }
int IVTFTexture::LowResHeight() const
{ return static_cast<const CVTFTexture*>(this)->LowResHeight(); }
ImageFormat IVTFTexture::LowResFormat() const
{ return static_cast<const CVTFTexture*>(this)->LowResFormat(); }
const Vector & IVTFTexture::Reflectivity() const
{ return static_cast<const CVTFTexture*>(this)->Reflectivity(); }
bool IVTFTexture::IsCubeMap() const
{ return static_cast<const CVTFTexture*>(this)->IsCubeMap(); }
bool IVTFTexture::IsNormalMap() const
{ return static_cast<const CVTFTexture*>(this)->IsNormalMap(); }
bool IVTFTexture::IsVolumeTexture() const
{ return static_cast<const CVTFTexture*>(this)->IsVolumeTexture(); }
void IVTFTexture::ComputeMipLevelDimensions( int iMipLevel, int *pMipWidth, int *pMipHeight, int *pMipDepth ) const
{ static_cast<const CVTFTexture*>(this)->ComputeMipLevelDimensions( iMipLevel, pMipWidth, pMipHeight, pMipDepth ); }
int IVTFTexture::ComputeMipSize( int iMipLevel ) const
{ return static_cast<const CVTFTexture*>(this)->ComputeMipSize( iMipLevel ); }
void IVTFTexture::ComputeMipLevelSubRect( Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect ) const
{ static_cast<const CVTFTexture*>(this)->ComputeMipLevelSubRect( pSrcRect, nMipLevel, pSubRect ); }
int IVTFTexture::ComputeFaceSize( int iStartingMipLevel ) const
{ return static_cast<const CVTFTexture*>(this)->ComputeFaceSize( iStartingMipLevel ); }
int IVTFTexture::ComputeTotalSize() const
{ return static_cast<const CVTFTexture*>(this)->ComputeTotalSize(); }
uint8 * IVTFTexture::ImageData()
{ return static_cast<CVTFTexture*>(this)->ImageData(); }
uint8 * IVTFTexture::ImageData( int iFrame, int iFace, int iMipLevel )
{ return static_cast<CVTFTexture*>(this)->ImageData( iFrame, iFace, iMipLevel ); }
uint8 * IVTFTexture::ImageData( int iFrame, int iFace, int iMipLevel, int x, int y, int z )
{ return static_cast<CVTFTexture*>(this)->ImageData( iFrame, iFace, iMipLevel, x, y, z ); }
uint8 * IVTFTexture::LowResImageData()
{ return static_cast<CVTFTexture*>(this)->LowResImageData(); }
void IVTFTexture::ConvertImageFormat( ImageFormat fmt, bool bNormalToDUDV, bool bNormalToDXT5GA )
{ static_cast<CVTFTexture*>(this)->ConvertImageFormat( fmt, bNormalToDUDV, bNormalToDXT5GA ); }
void IVTFTexture::GenerateSpheremap( LookDir_t lookDir )
{ static_cast<CVTFTexture*>(this)->GenerateSpheremap( lookDir ); }
void IVTFTexture::GenerateHemisphereMap( uint8 *pSphereMapBitsRGBA, int targetWidth, int targetHeight, LookDir_t lookDir, int iFrame )
{ static_cast<CVTFTexture*>(this)->GenerateHemisphereMap( pSphereMapBitsRGBA, targetWidth, targetHeight, lookDir, iFrame ); }
void IVTFTexture::FixCubemapFaceOrientation()
{ static_cast<CVTFTexture*>(this)->FixCubemapFaceOrientation(); }
void IVTFTexture::GenerateMipmaps()
{ static_cast<CVTFTexture*>(this)->GenerateMipmaps(); }
void IVTFTexture::PutOneOverMipLevelInAlpha()
{ static_cast<CVTFTexture*>(this)->PutOneOverMipLevelInAlpha(); }
void IVTFTexture::PremultAlphaWithMipFraction()
{ static_cast<CVTFTexture*>(this)->PremultAlphaWithMipFraction(); }
void IVTFTexture::ComputeReflectivity()
{ static_cast<CVTFTexture*>(this)->ComputeReflectivity(); }
void IVTFTexture::ComputeAlphaFlags()
{ static_cast<CVTFTexture*>(this)->ComputeAlphaFlags(); }
bool IVTFTexture::ConstructLowResImage()
{ return static_cast<CVTFTexture*>(this)->ConstructLowResImage(); }
void IVTFTexture::PostProcess( bool bGenerateSpheremap, LookDir_t lookDir, bool bAllowFixCubemapOrientation, bool bLoadedMiplevels )
{ static_cast<CVTFTexture*>(this)->PostProcess( bGenerateSpheremap, lookDir, bAllowFixCubemapOrientation, bLoadedMiplevels ); }
void IVTFTexture::MatchCubeMapBorders( int iStage, ImageFormat finalFormat, bool bSkybox )
{ static_cast<CVTFTexture*>(this)->MatchCubeMapBorders( iStage, finalFormat, bSkybox ); }
void IVTFTexture::SetAlphaTestThreshholds( float flBase, float flHighFreq )
{ static_cast<CVTFTexture*>(this)->SetAlphaTestThreshholds( flBase, flHighFreq ); }
bool IVTFTexture::IsPreTiled() const
{ return static_cast<const CVTFTexture*>(this)->IsPreTiled(); }
void IVTFTexture::SetPostProcessingSettings( VtfProcessingOptions const *pOptions )
{ static_cast<CVTFTexture*>(this)->SetPostProcessingSettings( pOptions ); }
