// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialSystem class to the material system implementation.
// Included by cmaterialsystem.cpp, which defines MaterialSystemImpl().

bool IMaterialSystem::Connect( CreateInterfaceFn factory )
{ return MaterialSystemImpl().Connect( factory ); }
void IMaterialSystem::Disconnect()
{ MaterialSystemImpl().Disconnect(); }
void * IMaterialSystem::QueryInterface( const char *pInterfaceName )
{ return MaterialSystemImpl().QueryInterface( pInterfaceName ); }
InitReturnVal_t IMaterialSystem::Init()
{ return MaterialSystemImpl().Init(); }
void IMaterialSystem::Shutdown()
{ MaterialSystemImpl().Shutdown(); }
const AppSystemInfo_t * IMaterialSystem::GetDependencies()
{ return MaterialSystemImpl().GetDependencies(); }
AppSystemTier_t IMaterialSystem::GetTier()
{ return MaterialSystemImpl().GetTier(); }
void IMaterialSystem::Reconnect( CreateInterfaceFn factory, const char *pInterfaceName )
{ MaterialSystemImpl().Reconnect( factory, pInterfaceName ); }
CreateInterfaceFn IMaterialSystem::Init( char const* pShaderAPIDLL, IMaterialProxyFactory *pMaterialProxyFactory, CreateInterfaceFn fileSystemFactory, CreateInterfaceFn cvarFactory )
{ return MaterialSystemImpl().Init( pShaderAPIDLL, pMaterialProxyFactory, fileSystemFactory, cvarFactory ); }
void IMaterialSystem::SetShaderAPI( char const *pShaderAPIDLL )
{ MaterialSystemImpl().SetShaderAPI( pShaderAPIDLL ); }
void IMaterialSystem::SetAdapter( int nAdapter, int nFlags )
{ MaterialSystemImpl().SetAdapter( nAdapter, nFlags ); }
void IMaterialSystem::ModInit()
{ MaterialSystemImpl().ModInit(); }
void IMaterialSystem::ModShutdown()
{ MaterialSystemImpl().ModShutdown(); }
void IMaterialSystem::SetThreadMode( MaterialThreadMode_t mode, int nServiceThread )
{ MaterialSystemImpl().SetThreadMode( mode, nServiceThread ); }
MaterialThreadMode_t IMaterialSystem::GetThreadMode()
{ return MaterialSystemImpl().GetThreadMode(); }
bool IMaterialSystem::IsRenderThreadSafe()
{ return MaterialSystemImpl().IsRenderThreadSafe(); }
void IMaterialSystem::ExecuteQueued()
{ MaterialSystemImpl().ExecuteQueued(); }
void IMaterialSystem::OnDebugEvent( const char * pEvent )
{ MaterialSystemImpl().OnDebugEvent( pEvent ); }
IMaterialSystemHardwareConfig * IMaterialSystem::GetHardwareConfig( const char *pVersion, int *returnCode )
{ return MaterialSystemImpl().GetHardwareConfig( pVersion, returnCode ); }
bool IMaterialSystem::UpdateConfig( bool bForceUpdate )
{ return MaterialSystemImpl().UpdateConfig( bForceUpdate ); }
bool IMaterialSystem::OverrideConfig( const MaterialSystem_Config_t &config, bool bForceUpdate )
{ return MaterialSystemImpl().OverrideConfig( config, bForceUpdate ); }
const MaterialSystem_Config_t & IMaterialSystem::GetCurrentConfigForVideoCard() const
{ return MaterialSystemImpl().GetCurrentConfigForVideoCard(); }
bool IMaterialSystem::GetRecommendedConfigurationInfo( int nDXLevel, KeyValues * pKeyValues )
{ return MaterialSystemImpl().GetRecommendedConfigurationInfo( nDXLevel, pKeyValues ); }
int IMaterialSystem::GetDisplayAdapterCount() const
{ return MaterialSystemImpl().GetDisplayAdapterCount(); }
int IMaterialSystem::GetCurrentAdapter() const
{ return MaterialSystemImpl().GetCurrentAdapter(); }
void IMaterialSystem::GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
{ MaterialSystemImpl().GetDisplayAdapterInfo( adapter, info ); }
int IMaterialSystem::GetModeCount( int adapter ) const
{ return MaterialSystemImpl().GetModeCount( adapter ); }
void IMaterialSystem::GetModeInfo( int adapter, int mode, MaterialVideoMode_t& info ) const
{ MaterialSystemImpl().GetModeInfo( adapter, mode, info ); }
void IMaterialSystem::AddModeChangeCallBack( ModeChangeCallbackFunc_t func )
{ MaterialSystemImpl().AddModeChangeCallBack( func ); }
void IMaterialSystem::GetDisplayMode( MaterialVideoMode_t& mode ) const
{ MaterialSystemImpl().GetDisplayMode( mode ); }
bool IMaterialSystem::SetMode( void* hwnd, const MaterialSystem_Config_t &config )
{ return MaterialSystemImpl().SetMode( hwnd, config ); }
bool IMaterialSystem::SupportsMSAAMode( int nMSAAMode )
{ return MaterialSystemImpl().SupportsMSAAMode( nMSAAMode ); }
const MaterialSystemHardwareIdentifier_t & IMaterialSystem::GetVideoCardIdentifier() const
{ return MaterialSystemImpl().GetVideoCardIdentifier(); }
void IMaterialSystem::SpewDriverInfo() const
{ MaterialSystemImpl().SpewDriverInfo(); }
void IMaterialSystem::GetBackBufferDimensions( int &width, int &height ) const
{ MaterialSystemImpl().GetBackBufferDimensions( width, height ); }
ImageFormat IMaterialSystem::GetBackBufferFormat() const
{ return MaterialSystemImpl().GetBackBufferFormat(); }
const AspectRatioInfo_t & IMaterialSystem::GetAspectRatioInfo() const
{ return MaterialSystemImpl().GetAspectRatioInfo(); }
bool IMaterialSystem::SupportsHDRMode( HDRType_t nHDRModede )
{ return MaterialSystemImpl().SupportsHDRMode( nHDRModede ); }
bool IMaterialSystem::AddView( void* hwnd )
{ return MaterialSystemImpl().AddView( hwnd ); }
void IMaterialSystem::RemoveView( void* hwnd )
{ MaterialSystemImpl().RemoveView( hwnd ); }
void IMaterialSystem::SetView( void* hwnd )
{ MaterialSystemImpl().SetView( hwnd ); }
void IMaterialSystem::BeginFrame( float frameTime )
{ MaterialSystemImpl().BeginFrame( frameTime ); }
void IMaterialSystem::EndFrame()
{ MaterialSystemImpl().EndFrame(); }
void IMaterialSystem::Flush( bool flushHardware )
{ MaterialSystemImpl().Flush( flushHardware ); }
uint32 IMaterialSystem::GetCurrentFrameCount()
{ return MaterialSystemImpl().GetCurrentFrameCount(); }
void IMaterialSystem::SwapBuffers()
{ MaterialSystemImpl().SwapBuffers(); }
void IMaterialSystem::EvictManagedResources()
{ MaterialSystemImpl().EvictManagedResources(); }
void IMaterialSystem::ReleaseResources()
{ MaterialSystemImpl().ReleaseResources(); }
void IMaterialSystem::ReacquireResources()
{ MaterialSystemImpl().ReacquireResources(); }
void IMaterialSystem::AddReleaseFunc( MaterialBufferReleaseFunc_t func )
{ MaterialSystemImpl().AddReleaseFunc( func ); }
void IMaterialSystem::RemoveReleaseFunc( MaterialBufferReleaseFunc_t func )
{ MaterialSystemImpl().RemoveReleaseFunc( func ); }
void IMaterialSystem::AddRestoreFunc( MaterialBufferRestoreFunc_t func )
{ MaterialSystemImpl().AddRestoreFunc( func ); }
void IMaterialSystem::RemoveRestoreFunc( MaterialBufferRestoreFunc_t func )
{ MaterialSystemImpl().RemoveRestoreFunc( func ); }
void IMaterialSystem::AddEndFrameCleanupFunc( EndFrameCleanupFunc_t func )
{ MaterialSystemImpl().AddEndFrameCleanupFunc( func ); }
void IMaterialSystem::RemoveEndFrameCleanupFunc( EndFrameCleanupFunc_t func )
{ MaterialSystemImpl().RemoveEndFrameCleanupFunc( func ); }
void IMaterialSystem::OnLevelShutdown()
{ MaterialSystemImpl().OnLevelShutdown(); }
bool IMaterialSystem::AddOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData )
{ return MaterialSystemImpl().AddOnLevelShutdownFunc( func, pUserData ); }
bool IMaterialSystem::RemoveOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData )
{ return MaterialSystemImpl().RemoveOnLevelShutdownFunc( func, pUserData ); }
void IMaterialSystem::OnLevelLoadingComplete()
{ MaterialSystemImpl().OnLevelLoadingComplete(); }
void IMaterialSystem::ResetTempHWMemory( bool bExitingLevel )
{ MaterialSystemImpl().ResetTempHWMemory( bExitingLevel ); }
void IMaterialSystem::HandleDeviceLost()
{ MaterialSystemImpl().HandleDeviceLost(); }
int IMaterialSystem::ShaderCount() const
{ return MaterialSystemImpl().ShaderCount(); }
int IMaterialSystem::GetShaders( int nFirstShader, int nMaxCount, IShader **ppShaderList ) const
{ return MaterialSystemImpl().GetShaders( nFirstShader, nMaxCount, ppShaderList ); }
int IMaterialSystem::ShaderFlagCount() const
{ return MaterialSystemImpl().ShaderFlagCount(); }
const char * IMaterialSystem::ShaderFlagName( int nIndex ) const
{ return MaterialSystemImpl().ShaderFlagName( nIndex ); }
void IMaterialSystem::GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength )
{ MaterialSystemImpl().GetShaderFallback( pShaderName, pFallbackShader, nFallbackLength ); }
IMaterialProxyFactory * IMaterialSystem::GetMaterialProxyFactory()
{ return MaterialSystemImpl().GetMaterialProxyFactory(); }
void IMaterialSystem::SetMaterialProxyFactory( IMaterialProxyFactory* pFactory )
{ MaterialSystemImpl().SetMaterialProxyFactory( pFactory ); }
void IMaterialSystem::EnableEditorMaterials()
{ MaterialSystemImpl().EnableEditorMaterials(); }
void IMaterialSystem::EnableGBuffers()
{ MaterialSystemImpl().EnableGBuffers(); }
void IMaterialSystem::DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose )
{ MaterialSystemImpl().DebugPrintUsedMaterials( pSearchSubString, bVerbose ); }
void IMaterialSystem::DebugPrintUsedTextures()
{ MaterialSystemImpl().DebugPrintUsedTextures(); }
void IMaterialSystem::ToggleSuppressMaterial( char const* pMaterialName )
{ MaterialSystemImpl().ToggleSuppressMaterial( pMaterialName ); }
void IMaterialSystem::ToggleDebugMaterial( char const* pMaterialName )
{ MaterialSystemImpl().ToggleDebugMaterial( pMaterialName ); }
bool IMaterialSystem::UsingFastClipping()
{ return MaterialSystemImpl().UsingFastClipping(); }
int IMaterialSystem::StencilBufferBits()
{ return MaterialSystemImpl().StencilBufferBits(); }
void IMaterialSystem::UncacheAllMaterials()
{ MaterialSystemImpl().UncacheAllMaterials(); }
void IMaterialSystem::UncacheUnusedMaterials( bool bRecomputeStateSnapshots )
{ MaterialSystemImpl().UncacheUnusedMaterials( bRecomputeStateSnapshots ); }
void IMaterialSystem::CacheUsedMaterials()
{ MaterialSystemImpl().CacheUsedMaterials(); }
void IMaterialSystem::ReloadTextures()
{ MaterialSystemImpl().ReloadTextures(); }
void IMaterialSystem::ReloadMaterials( const char *pSubString )
{ MaterialSystemImpl().ReloadMaterials( pSubString ); }
IMaterial * IMaterialSystem::CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues )
{ return MaterialSystemImpl().CreateMaterial( pMaterialName, pVMTKeyValues ); }
IMaterial * IMaterialSystem::FindMaterial( char const* pMaterialName, const char *pTextureGroupName, bool complain, const char *pComplainPrefix )
{ return MaterialSystemImpl().FindMaterial( pMaterialName, pTextureGroupName, complain, pComplainPrefix ); }
bool IMaterialSystem::LoadKeyValuesFromVMTFile( KeyValues &vmtKeyValues, const char *pMaterialName, bool bUsesUNCFilename )
{ return MaterialSystemImpl().LoadKeyValuesFromVMTFile( vmtKeyValues, pMaterialName, bUsesUNCFilename ); }
MaterialHandle_t IMaterialSystem::FirstMaterial() const
{ return MaterialSystemImpl().FirstMaterial(); }
MaterialHandle_t IMaterialSystem::NextMaterial( MaterialHandle_t h ) const
{ return MaterialSystemImpl().NextMaterial( h ); }
MaterialHandle_t IMaterialSystem::InvalidMaterial() const
{ return MaterialSystemImpl().InvalidMaterial(); }
IMaterial* IMaterialSystem::GetMaterial( MaterialHandle_t h ) const
{ return MaterialSystemImpl().GetMaterial( h ); }
int IMaterialSystem::GetNumMaterials() const
{ return MaterialSystemImpl().GetNumMaterials(); }
ITexture * IMaterialSystem::FindTexture( char const* pTextureName, const char *pTextureGroupName, bool complain, int nAdditionalCreationFlags )
{ return MaterialSystemImpl().FindTexture( pTextureName, pTextureGroupName, complain, nAdditionalCreationFlags ); }
bool IMaterialSystem::IsTextureLoaded( char const* pTextureName ) const
{ return MaterialSystemImpl().IsTextureLoaded( pTextureName ); }
ITexture * IMaterialSystem::CreateProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, ImageFormat fmt, int nFlags )
{ return MaterialSystemImpl().CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, fmt, nFlags ); }
void IMaterialSystem::BeginRenderTargetAllocation()
{ MaterialSystemImpl().BeginRenderTargetAllocation(); }
void IMaterialSystem::EndRenderTargetAllocation()
{ MaterialSystemImpl().EndRenderTargetAllocation(); }
ITexture * IMaterialSystem::CreateRenderTargetTexture( int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat format, MaterialRenderTargetDepth_t depth )
{ return MaterialSystemImpl().CreateRenderTargetTexture( w, h, sizeMode, format, depth ); }
ITexture * IMaterialSystem::CreateNamedRenderTargetTextureEx( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat format, MaterialRenderTargetDepth_t depth, unsigned int textureFlags, unsigned int renderTargetFlags )
{ return MaterialSystemImpl().CreateNamedRenderTargetTextureEx( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags ); }
ITexture * IMaterialSystem::CreateNamedRenderTargetTexture( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat format, MaterialRenderTargetDepth_t depth, bool bClampTexCoords, bool bAutoMipMap )
{ return MaterialSystemImpl().CreateNamedRenderTargetTexture( pRTName, w, h, sizeMode, format, depth, bClampTexCoords, bAutoMipMap ); }
ITexture * IMaterialSystem::CreateNamedRenderTargetTextureEx2( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat format, MaterialRenderTargetDepth_t depth, unsigned int textureFlags, unsigned int renderTargetFlags )
{ return MaterialSystemImpl().CreateNamedRenderTargetTextureEx2( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags ); }
void IMaterialSystem::BeginLightmapAllocation()
{ MaterialSystemImpl().BeginLightmapAllocation(); }
void IMaterialSystem::EndLightmapAllocation()
{ MaterialSystemImpl().EndLightmapAllocation(); }
void IMaterialSystem::CleanupLightmaps()
{ MaterialSystemImpl().CleanupLightmaps(); }
int IMaterialSystem::AllocateLightmap( int width, int height, int offsetIntoLightmapPage[2], IMaterial *pMaterial )
{ return MaterialSystemImpl().AllocateLightmap( width, height, offsetIntoLightmapPage, pMaterial ); }
int IMaterialSystem::AllocateWhiteLightmap( IMaterial *pMaterial )
{ return MaterialSystemImpl().AllocateWhiteLightmap( pMaterial ); }
void IMaterialSystem::UpdateLightmap( int lightmapPageID, int lightmapSize[2], int offsetIntoLightmapPage[2], float *pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, float *pFloatImageBump3 )
{ MaterialSystemImpl().UpdateLightmap( lightmapPageID, lightmapSize, offsetIntoLightmapPage, pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3 ); }
int IMaterialSystem::GetNumSortIDs()
{ return MaterialSystemImpl().GetNumSortIDs(); }
void IMaterialSystem::GetSortInfo( MaterialSystem_SortInfo_t *sortInfoArray )
{ MaterialSystemImpl().GetSortInfo( sortInfoArray ); }
void IMaterialSystem::GetLightmapPageSize( int lightmap, int *width, int *height ) const
{ MaterialSystemImpl().GetLightmapPageSize( lightmap, width, height ); }
void IMaterialSystem::ResetMaterialLightmapPageInfo()
{ MaterialSystemImpl().ResetMaterialLightmapPageInfo(); }
bool IMaterialSystem::IsStereoSupported()
{ return MaterialSystemImpl().IsStereoSupported(); }
bool IMaterialSystem::IsStereoActiveThisFrame() const
{ return MaterialSystemImpl().IsStereoActiveThisFrame(); }
void IMaterialSystem::NVStereoUpdate()
{ MaterialSystemImpl().NVStereoUpdate(); }
void IMaterialSystem::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil )
{ MaterialSystemImpl().ClearBuffers( bClearColor, bClearDepth, bClearStencil ); }
void IMaterialSystem::SpinPresent( uint nFrames )
{ MaterialSystemImpl().SpinPresent( nFrames ); }
IMatRenderContext * IMaterialSystem::GetRenderContext()
{ return MaterialSystemImpl().GetRenderContext(); }
void IMaterialSystem::BeginUpdateLightmaps()
{ MaterialSystemImpl().BeginUpdateLightmaps(); }
void IMaterialSystem::EndUpdateLightmaps()
{ MaterialSystemImpl().EndUpdateLightmaps(); }
MaterialLock_t IMaterialSystem::Lock()
{ return MaterialSystemImpl().Lock(); }
void IMaterialSystem::Unlock( MaterialLock_t arg0 )
{ MaterialSystemImpl().Unlock( arg0 ); }
bool IMaterialSystem::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{ return MaterialSystemImpl().SupportsCSAAMode( nNumSamples, nQualityLevel ); }
void IMaterialSystem::RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func )
{ MaterialSystemImpl().RemoveModeChangeCallBack( func ); }
IMaterial * IMaterialSystem::FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues )
{ return MaterialSystemImpl().FindProceduralMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues ); }
void IMaterialSystem::AddTextureAlias( const char *pAlias, const char *pRealName )
{ MaterialSystemImpl().AddTextureAlias( pAlias, pRealName ); }
void IMaterialSystem::RemoveTextureAlias( const char *pAlias )
{ MaterialSystemImpl().RemoveTextureAlias( pAlias ); }
int IMaterialSystem::AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID )
{ return MaterialSystemImpl().AllocateDynamicLightmap( lightmapSize, pOutOffsetIntoPage, frameID ); }
void IMaterialSystem::SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache )
{ MaterialSystemImpl().SetExcludedTextures( pScriptName, bUsingWeaponModelCache ); }
void IMaterialSystem::UpdateExcludedTextures()
{ MaterialSystemImpl().UpdateExcludedTextures(); }
void IMaterialSystem::ClearForceExcludes()
{ MaterialSystemImpl().ClearForceExcludes(); }
bool IMaterialSystem::IsInFrame() const
{ return MaterialSystemImpl().IsInFrame(); }
void IMaterialSystem::CompactMemory()
{ MaterialSystemImpl().CompactMemory(); }
void IMaterialSystem::GetGPUMemoryStats( GPUMemoryStats &stats )
{ MaterialSystemImpl().GetGPUMemoryStats( stats ); }
void IMaterialSystem::ReloadFilesInList( IFileList *pFilesToReload )
{ MaterialSystemImpl().ReloadFilesInList( pFilesToReload ); }
bool IMaterialSystem::GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info ) const
{ return MaterialSystemImpl().GetTextureInformation( szTextureName, info ); }
void IMaterialSystem::FinishRenderTargetAllocation()
{ MaterialSystemImpl().FinishRenderTargetAllocation(); }
void IMaterialSystem::ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly()
{ MaterialSystemImpl().ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly(); }
bool IMaterialSystem::AllowThreading( bool bAllow, int nServiceThread )
{ return MaterialSystemImpl().AllowThreading( bAllow, nServiceThread ); }
bool IMaterialSystem::GetRecommendedVideoConfig( KeyValues *pKeyValues )
{ return MaterialSystemImpl().GetRecommendedVideoConfig( pKeyValues ); }
bool IMaterialSystem::CanDownloadTextures() const
{ return MaterialSystemImpl().CanDownloadTextures(); }
int IMaterialSystem::GetNumLightmapPages() const
{ return MaterialSystemImpl().GetNumLightmapPages(); }
void IMaterialSystem::RegisterPaintmapDataManager( IPaintmapDataManager *pDataManager )
{ MaterialSystemImpl().RegisterPaintmapDataManager( pDataManager ); }
void IMaterialSystem::BeginUpdatePaintmaps()
{ MaterialSystemImpl().BeginUpdatePaintmaps(); }
void IMaterialSystem::EndUpdatePaintmaps()
{ MaterialSystemImpl().EndUpdatePaintmaps(); }
void IMaterialSystem::UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects )
{ MaterialSystemImpl().UpdatePaintmap( paintmap, pPaintData, numRects, pRects ); }
ITexture * IMaterialSystem::CreateNamedMultiRenderTargetTexture( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat format, MaterialRenderTargetDepth_t depth, unsigned int textureFlags, unsigned int renderTargetFlags )
{ return MaterialSystemImpl().CreateNamedMultiRenderTargetTexture( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags ); }
void IMaterialSystem::RefreshFrontBufferNonInteractive()
{ MaterialSystemImpl().RefreshFrontBufferNonInteractive(); }
uint32 IMaterialSystem::GetFrameTimestamps( ApplicationPerformanceCountersInfo_t &apci, ApplicationInstantCountersInfo_t & aici )
{ return MaterialSystemImpl().GetFrameTimestamps( apci, aici ); }
void IMaterialSystem::AddEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func )
{ MaterialSystemImpl().AddEndFramePriorToNextContextFunc( func ); }
void IMaterialSystem::RemoveEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func )
{ MaterialSystemImpl().RemoveEndFramePriorToNextContextFunc( func ); }
ICustomMaterialManager * IMaterialSystem::GetCustomMaterialManager()
{ return MaterialSystemImpl().GetCustomMaterialManager(); }
ICompositeTextureGenerator * IMaterialSystem::GetCompositeTextureGenerator()
{ return MaterialSystemImpl().GetCompositeTextureGenerator(); }
