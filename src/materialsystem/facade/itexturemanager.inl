// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ITextureManager class to its one implementation, CTextureManager.
// Included by texturemanager.cpp, which defines TextureManagerImpl().

void ITextureManager::Init( int nFlags )
{ TextureManagerImpl().Init( nFlags ); }
void ITextureManager::Shutdown()
{ TextureManagerImpl().Shutdown(); }
void ITextureManager::AllocateStandardRenderTargets()
{ TextureManagerImpl().AllocateStandardRenderTargets(); }
void ITextureManager::FreeStandardRenderTargets()
{ TextureManagerImpl().FreeStandardRenderTargets(); }
void ITextureManager::CacheExternalStandardRenderTargets()
{ TextureManagerImpl().CacheExternalStandardRenderTargets(); }
ITextureInternal * ITextureManager::CreateProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags )
{ return TextureManagerImpl().CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, d, fmt, nFlags ); }
ITextureInternal * ITextureManager::CreateRenderTargetTexture( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat fmt, RenderTargetType_t type, unsigned int textureFlags, unsigned int renderTargetFlags, bool bMultipleTargets )
{ return TextureManagerImpl().CreateRenderTargetTexture( pRTName, w, h, sizeMode, fmt, type, textureFlags, renderTargetFlags, bMultipleTargets ); }
ITextureInternal * ITextureManager::FindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags )
{ return TextureManagerImpl().FindOrLoadTexture( pTextureName, pTextureGroupName, nAdditionalCreationFlags ); }
void ITextureManager::ResetTextureFilteringState()
{ TextureManagerImpl().ResetTextureFilteringState(); }
void ITextureManager::ReloadTextures()
{ TextureManagerImpl().ReloadTextures(); }
void ITextureManager::ReleaseTextures( bool bReleaseManaged )
{ TextureManagerImpl().ReleaseTextures( bReleaseManaged ); }
void ITextureManager::RestoreRenderTargets()
{ TextureManagerImpl().RestoreRenderTargets(); }
void ITextureManager::RestoreNonRenderTargetTextures()
{ TextureManagerImpl().RestoreNonRenderTargetTextures(); }
void ITextureManager::RemoveUnusedTextures()
{ TextureManagerImpl().RemoveUnusedTextures(); }
void ITextureManager::DebugPrintUsedTextures()
{ TextureManagerImpl().DebugPrintUsedTextures(); }
int ITextureManager::RequestNextTextureID()
{ return TextureManagerImpl().RequestNextTextureID(); }
ITextureInternal * ITextureManager::ErrorTexture()
{ return TextureManagerImpl().ErrorTexture(); }
ITextureInternal * ITextureManager::NormalizationCubemap()
{ return TextureManagerImpl().NormalizationCubemap(); }
ITextureInternal * ITextureManager::SignedNormalizationCubemap()
{ return TextureManagerImpl().SignedNormalizationCubemap(); }
ITextureInternal * ITextureManager::ColorCorrectionTexture( int index )
{ return TextureManagerImpl().ColorCorrectionTexture( index ); }
ITextureInternal * ITextureManager::ShadowNoise2D()
{ return TextureManagerImpl().ShadowNoise2D(); }
ITextureInternal * ITextureManager::SSAONoise2D()
{ return TextureManagerImpl().SSAONoise2D(); }
ITextureInternal * ITextureManager::IdentityLightWarp()
{ return TextureManagerImpl().IdentityLightWarp(); }
ITextureInternal * ITextureManager::FullFrameDepthTexture()
{ return TextureManagerImpl().FullFrameDepthTexture(); }
ITextureInternal * ITextureManager::StereoParamTexture()
{ return TextureManagerImpl().StereoParamTexture(); }
void ITextureManager::GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture )
{ TextureManagerImpl().GenerateErrorTexture( pTexture, pVTFTexture ); }
void ITextureManager::SetColorCorrectionTexture( int i, ITextureInternal *pTexture )
{ TextureManagerImpl().SetColorCorrectionTexture( i, pTexture ); }
void ITextureManager::ForceAllTexturesIntoHardware()
{ TextureManagerImpl().ForceAllTexturesIntoHardware(); }
bool ITextureManager::IsTextureLoaded( const char *pTextureName )
{ return TextureManagerImpl().IsTextureLoaded( pTextureName ); }
bool ITextureManager::GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info )
{ return TextureManagerImpl().GetTextureInformation( szTextureName, info ); }
void ITextureManager::RemoveTexture( ITextureInternal *pTexture )
{ TextureManagerImpl().RemoveTexture( pTexture ); }
void ITextureManager::AddTextureAlias( const char *pAlias, const char *pRealName )
{ TextureManagerImpl().AddTextureAlias( pAlias, pRealName ); }
void ITextureManager::RemoveTextureAlias( const char *pAlias )
{ TextureManagerImpl().RemoveTextureAlias( pAlias ); }
void ITextureManager::SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache )
{ TextureManagerImpl().SetExcludedTextures( pScriptName, bUsingWeaponModelCache ); }
void ITextureManager::UpdateExcludedTextures()
{ TextureManagerImpl().UpdateExcludedTextures(); }
void ITextureManager::ClearForceExcludes()
{ TextureManagerImpl().ClearForceExcludes(); }
void ITextureManager::ReleaseTempRenderTargetBits()
{ TextureManagerImpl().ReleaseTempRenderTargetBits(); }
void ITextureManager::ReloadFilesInList( IFileList *pFilesToReload )
{ TextureManagerImpl().ReloadFilesInList( pFilesToReload ); }
