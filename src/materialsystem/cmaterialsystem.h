#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import ks.tier1;
import std;

#include "materialsystem_global.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/ishaderapi.h"
#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/deformations.h"

#include "cmaterialdict.h"
#include "cmatlightmaps.h"
#include "cmatpaintmaps.h"
#include "cmatrendercontext.h"
#include "cmatqueuedrendercontext.h"
#include "materialsystem_global.h"
#include "custom_material.h"
#include "composite_texture.h"

//-----------------------------------------------------------------------------

class CJob;
struct DeferredUpdateLightmapInfo_t;

// typedefs to allow use of delegation macros
typedef int LightmapOffset_t[2];

//-----------------------------------------------------------------------------

extern std::recursive_mutex g_MatSysMutex;

enum AsyncTextureLoadError_t
{
	ASYNCTEXTURE_LOADERROR_NONE     = 0,
	ASYNCTEXTURE_LOADERROR_FILEOPEN = -1,
	ASYNCTEXTRUE_LOADERROR_READING  = -2,
};

struct AsyncTextureLoad_t
{
	AsyncTextureContext_t		*m_pContext;
	void						*m_pData;
	int							m_nNumReadBytes;
	AsyncTextureLoadError_t		m_LoadError;
};

//-----------------------------------------------------------------------------
// The material system implementation
//-----------------------------------------------------------------------------

class CMaterialSystem : public CTier2AppSystem< IMaterialSystemInternal >, public IShaderUtil
{
	typedef CTier2AppSystem< IMaterialSystemInternal > BaseClass;
public:

	CMaterialSystem();
	~CMaterialSystem();

	//---------------------------------------------------------
	// Initialization and shutdown
	//---------------------------------------------------------

	//
	// IAppSystem
	//
	virtual bool							Connect( CreateInterfaceFn factory );
	virtual void							Disconnect();
	virtual void *							QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t					Init();
	virtual void							Shutdown();
	virtual const AppSystemInfo_t *			GetDependencies( ) { return nullptr; }
	virtual AppSystemTier_t					GetTier() { return APP_SYSTEM_TIER2; }
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName )	{ BaseClass::Reconnect( factory, pInterfaceName ); }

	CreateInterfaceFn						Init( const char* pShaderDLL,
													IMaterialProxyFactory* pMaterialProxyFactory,
													CreateInterfaceFn fileSystemFactory,
													CreateInterfaceFn cvarFactory );

	// Call this to set an explicit shader version to use 
	// Must be called before Init().
	void									SetShaderAPI( const char *pShaderAPIDLL );

	// Must be called before Init(), if you're going to call it at all...
	void									SetAdapter( int nAdapter, int nFlags );

	void									ModInit();
	void									ModShutdown();

private:
	// Used to dynamically load and unload the shader api
	CreateInterfaceFn						CreateShaderAPI( const char* pShaderDLL );
	void									DestroyShaderAPI();
	
	// Method to get at interfaces supported by the SHADDERAPI
	void *									QueryShaderAPI( const char *pInterfaceName );

	// Initializes the color correction terms
	void									InitColorCorrection();

	// force the thread mode to single threaded, synchronizes with render thread if running
	void									ForceSingleThreaded();

	void									ThreadRelease( void );	// Tell QMS to release GL
	void									ThreadAcquire( void );	// Tell QMS to acquire GL

public:
	virtual void							SetThreadMode( MaterialThreadMode_t nextThreadMode, int nServiceThread );
	virtual MaterialThreadMode_t			GetThreadMode(); // current thread mode
	virtual bool							IsRenderThreadSafe( );
	virtual bool							AllowThreading( bool bAllow, int nServiceThread );
	virtual void							ExecuteQueued();
	virtual void                            OnDebugEvent( const char * pEvent = "" );

	//---------------------------------------------------------
	// Component accessors
	//---------------------------------------------------------
	const CMatLightmaps *					GetLightmaps() const		{ return &m_Lightmaps; }
	CMatLightmaps *							GetLightmaps()				{ return &m_Lightmaps; }

	const CMatPaintmaps *							GetPaintmaps( void ) const	{ return &m_Paintmaps; }
	CMatPaintmaps *							GetPaintmaps( void ) { return &m_Paintmaps; }

	IMatRenderContext *						GetRenderContext();
	IMatRenderContext *						CreateRenderContext( MaterialContextType_t type );
	IMatRenderContext *						SetRenderContext( IMatRenderContext * );

	const IMatRenderContextInternal *		GetRenderContextInternal() const	{ IMatRenderContextInternal *pRenderContext = m_pRenderContext; return ( pRenderContext ) ? pRenderContext : &m_HardwareRenderContext; }
	IMatRenderContextInternal *				GetRenderContextInternal()			{ IMatRenderContextInternal *pRenderContext = m_pRenderContext; return ( pRenderContext ) ? pRenderContext : &m_HardwareRenderContext; }

	const CMaterialDict *					GetMaterialDict() const		{ return &m_MaterialDict; }
	CMaterialDict *							GetMaterialDict()			{ return &m_MaterialDict; }

	virtual void							ReloadFilesInList( IFileList *pFilesToReload );

public:
	//---------------------------------------------------------
	// Config management
	//---------------------------------------------------------

	// IMaterialSystem
	virtual IMaterialSystemHardwareConfig *	GetHardwareConfig( const char *pVersion, int *returnCode );

	// Get the current config for this video card (as last set by control panel or the default if not)
	virtual bool							UpdateConfig( bool forceUpdate );
	virtual bool							OverrideConfig( const MaterialSystem_Config_t &config, bool bForceUpdate );
	const MaterialSystem_Config_t &			GetCurrentConfigForVideoCard() const;

	// Gets *recommended* configuration information associated with the display card and dxsupport.
	virtual bool							GetRecommendedVideoConfig( KeyValues *pKeyValues );
	virtual bool							GetRecommendedConfigurationInfo( int nDXLevel, KeyValues * pKeyValues );

	// IShaderUtil
	MaterialSystem_Config_t &				GetConfig();

private:
	//---------------------------------

	// This is called when the config changes
	void									GenerateConfigFromConfigKeyValues( MaterialSystem_Config_t *pConfig, bool bOverwriteCommandLineValues );

	// Read/write config into convars
	void									ReadConfigFromConVars( MaterialSystem_Config_t *pConfig );
	void									WriteConfigIntoConVars( const MaterialSystem_Config_t &config );

public:
	// -----------------------------------------------------------
	// Device methods
	// -----------------------------------------------------------
	int										GetDisplayAdapterCount() const;
	int										GetCurrentAdapter() const;
	void									GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;
	int										GetModeCount( int adapter ) const;
	void									GetModeInfo( int adapter, int mode, MaterialVideoMode_t& info ) const;
	void									AddModeChangeCallBack( ModeChangeCallbackFunc_t func );
	void									RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func );

	// Returns the mode info for the current display device
	void									GetDisplayMode( MaterialVideoMode_t& info ) const;
	bool									SetMode( void* hwnd, const MaterialSystem_Config_t &config );

	// Reports support for a given MSAA mode
	bool									SupportsMSAAMode( int nMSAAMode );

	// Reports support for a given CSAA mode
	bool									SupportsCSAAMode( int nNumSamples, int nQualityLevel );

	bool				                    SupportsHDRMode( HDRType_t nHDRModede );

	bool									UsesSRGBCorrectBlending() const;

	// Shadow depth bias factors
	void									SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	void									FlipCulling( bool bFlipCulling );

	const MaterialSystemHardwareIdentifier_t &GetVideoCardIdentifier() const;

	// Use this to spew information about the 3D layer 
	void									SpewDriverInfo() const;

	void GetBackBufferDimensions( int & a1, int & a2 )	const { (g_pShaderDevice)->GetBackBufferDimensions( a1, a2 ); };
	ImageFormat GetBackBufferFormat()	const { return (g_pShaderDevice)->GetBackBufferFormat(); };
	const AspectRatioInfo_t & GetAspectRatioInfo()	const { return (g_pShaderAPI)->GetAspectRatioInfo(); };


	void PushDeformation( DeformationBase_t const * a1 )	{ (g_pShaderAPI)->PushDeformation( a1 ); };

	void PopDeformation()	{ (g_pShaderAPI)->PopDeformation(); };

	int GetNumActiveDeformations()	const { return (g_pShaderAPI)->GetNumActiveDeformations(); };

	// -----------------------------------------------------------
	// Window methods
	// -----------------------------------------------------------
	// Creates/ destroys a child window
	bool									AddView( void* hwnd );
	void									RemoveView( void* hwnd );

	// Sets the view
	void									SetView( void* hwnd );

	// -----------------------------------------------------------
	// Control flow
	// -----------------------------------------------------------
	void									BeginFrame( float frameTime );
	void									EndFrame();
	virtual bool							IsInFrame( ) const;
	void									Flush( bool flushHardware = false );
	void									SwapBuffers();
	uint32									GetCurrentFrameCount();

	// Flushes managed textures from the texture cacher
	void									EvictManagedResources();

	void									ReleaseResources();
	void									ReacquireResources();

	// Recomputes a state snapshot
	void									RecomputeAllStateSnapshots();

	void									NoteAnisotropicLevel( int currentLevel );

	// -----------------------------------------------------------
	// Device loss/restore
	// -----------------------------------------------------------
	// Installs a function to be called when we need to release vertex buffers
	void									AddReleaseFunc( MaterialBufferReleaseFunc_t func );
	void									RemoveReleaseFunc( MaterialBufferReleaseFunc_t func );

	// Installs a function to be called when we need to restore vertex buffers
	void									AddRestoreFunc( MaterialBufferRestoreFunc_t func );
	void									RemoveRestoreFunc( MaterialBufferRestoreFunc_t func );

	// Called by the shader API when it's just about to lose video memory
	void									ReleaseShaderObjects( int nChangeFlags );
	void									RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags = 0 );

	// Installs a function to be called when we need to delete objects at the end of the render frame
	virtual void							AddEndFrameCleanupFunc( EndFrameCleanupFunc_t func );
	virtual void							RemoveEndFrameCleanupFunc( EndFrameCleanupFunc_t func );

	// Gets called when the level is shuts down, will call the registered callback
	virtual void							OnLevelShutdown();
	// Installs a function to be called when the level is shuts down
	virtual bool							AddOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData );
	virtual bool							RemoveOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData );

	// Installs a function to be called when we need to perform operation before next rendering context is started
	virtual void							AddEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func );
	virtual void							RemoveEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func );

	virtual void							OnLevelLoadingComplete();

	// Release temporary HW memory...
	void									ResetTempHWMemory( bool bExitingLevel = false );

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	void									HandleDeviceLost();


	// -----------------------------------------------------------
	// Shaders
	// -----------------------------------------------------------
	int										ShaderCount() const;
	int										GetShaders( int nFirstShader, int nCount, IShader **ppShaderList ) const;
	int										ShaderFlagCount() const;
	const char *							ShaderFlagName( int nIndex ) const;
	void									GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength );

	// -----------------------------------------------------------
	// Material proxies
	// -----------------------------------------------------------
	IMaterialProxyFactory*					GetMaterialProxyFactory();
	void									SetMaterialProxyFactory( IMaterialProxyFactory* pFactory );

	IClientMaterialSystem*					GetClientMaterialSystemInterface();

	// -----------------------------------------------------------
	// Refresh the spinner on load screens
	// -----------------------------------------------------------
	void									RefreshFrontBufferNonInteractive();
	virtual uint32 GetFrameTimestamps( ApplicationPerformanceCountersInfo_t &apci, ApplicationInstantCountersInfo_t & aici );

	bool									CanDownloadTextures() const;

	// -----------------------------------------------------------
	// Editor mode
	// -----------------------------------------------------------
	bool									InEditorMode() const;

	// Used to enable editor materials. Must be called before Init.
	void									EnableEditorMaterials();
	void EnableGBuffers( void );


	// Can we use editor materials?
	bool                        CanUseEditorMaterials( void ) const;

	int                         GetConfigurationFlags( void ) const;


	// -----------------------------------------------------------
	// Stub mode mode
	// -----------------------------------------------------------
	void									SetInStubMode( bool bInStubMode );
	bool									IsInStubMode();


	//---------------------------------------------------------
	// Image formats
	//---------------------------------------------------------
	ImageFormatInfo_t const&				ImageFormatInfo( ImageFormat fmt) const;
	
	int										GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap );

	bool									ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat,
																unsigned char *dst, enum ImageFormat dstImageFormat, 
																int width, int height, int srcStride = 0, int dstStride = 0 );


	//---------------------------------------------------------
	// Debug support
	//---------------------------------------------------------
	void									CreateDebugMaterials();
	void									CleanUpDebugMaterials();
	void									CleanUpErrorMaterial();

	void									DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose );
	void									DebugPrintUsedTextures();

	void									ToggleSuppressMaterial( const char* pMaterialName );
	void									ToggleDebugMaterial( const char* pMaterialName );


	//---------------------------------------------------------
	// Misc features
	//---------------------------------------------------------

	//returns whether fast clipping is being used or not - needed to be exposed for better per-object clip behavior
	bool									UsingFastClipping();

	int										StencilBufferBits();


	//---------------------------------------------------------
	// Standard material and textures
	//---------------------------------------------------------
	void									AllocateStandardTextures();
	void									ReleaseStandardTextures();

	IMaterialInternal *						GetDrawFlatMaterial()									{ return m_pDrawFlatMaterial; }
	IMaterialInternal *						GetBufferClearObeyStencil( int i )						{ return m_pBufferClearObeyStencil[i]; }
	IMaterialInternal *						GetReloadZcullMaterial()								{ return m_pReloadZcullMaterial; }
	IMaterialInternal *						GetRenderTargetBlitMaterial()							{ return m_pRenderTargetBlitMaterial; }

	ShaderAPITextureHandle_t				GetFullbrightLightmapTextureHandle() const				{ return m_FullbrightLightmapTextureHandle; }
	ShaderAPITextureHandle_t				GetFullbrightBumpedLightmapTextureHandle() const		{ return m_FullbrightBumpedLightmapTextureHandle; }
	ShaderAPITextureHandle_t				GetBlackTextureHandle() const							{ return m_BlackTextureHandle; }
	ShaderAPITextureHandle_t				GetBlackAlphaZeroTextureHandle() const					{ return m_BlackAlphaZeroTextureHandle; }
	ShaderAPITextureHandle_t				GetFlatNormalTextureHandle() const						{ return m_FlatNormalTextureHandle; }
	ShaderAPITextureHandle_t				GetFlatSSBumpTextureHandle() const						{ return m_FlatSSBumpTextureHandle; }
	ShaderAPITextureHandle_t				GetGreyTextureHandle() const							{ return m_GreyTextureHandle; }
	ShaderAPITextureHandle_t				GetGreyAlphaZeroTextureHandle() const					{ return m_GreyAlphaZeroTextureHandle; }
	ShaderAPITextureHandle_t				GetWhiteTextureHandle() const							{ return m_WhiteTextureHandle; }
	ShaderAPITextureHandle_t				GetLinearToGammaTableTextureHandle() const				{ return m_LinearToGammaTableTextureHandle; }
	ShaderAPITextureHandle_t				GetLinearToGammaIdentityTableTextureHandle() const		{ return m_LinearToGammaIdentityTableTextureHandle; }
	ShaderAPITextureHandle_t				GetMaxDepthTextureHandle() const						{ return m_MaxDepthTextureHandle; }

	//---------------------------------------------------------
	// Material and texture management
	//---------------------------------------------------------
	void									UncacheAllMaterials();
	void									UncacheUnusedMaterials( bool bRecomputeStateSnapshots );
	void									CacheUsedMaterials();
	void									ReloadTextures();
	void									ReloadMaterials( const char *pSubString = nullptr );

	// Create new materials
	IMaterial *								CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues );
	IMaterial *								FindMaterial( const char *materialName, const char *pTextureGroupName, bool complain = true, const char *pComplainPrefix = nullptr );
	virtual IMaterial *						FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues = nullptr );
	const char *							GetForcedTextureLoadPathID() { return m_pForcedTextureLoadPathID; }

	bool									LoadKeyValuesFromVMTFile( KeyValues &vmtKeyValues, const char *pMaterialName, bool bUsesUNCFilename  );

	//---------------------------------

	MaterialHandle_t FirstMaterial()	const { return (&m_MaterialDict)->FirstMaterial(); };
	MaterialHandle_t NextMaterial( MaterialHandle_t a1 )	const { return (&m_MaterialDict)->NextMaterial( a1 ); };
	MaterialHandle_t InvalidMaterial()	const { return (&m_MaterialDict)->InvalidMaterial(); };
	IMaterial * GetMaterial( MaterialHandle_t a1 )	const { return (&m_MaterialDict)->GetMaterial( a1 ); };
	IMaterialInternal * GetMaterialInternal( MaterialHandle_t a1 )	const { return (&m_MaterialDict)->GetMaterialInternal( a1 ); };
	int GetNumMaterials()	const { return (&m_MaterialDict)->GetNumMaterials(); };
	void AddMaterialToMaterialList( IMaterialInternal * a1 )	{ (&m_MaterialDict)->AddMaterialToMaterialList( a1 ); };
	void RemoveMaterial( IMaterialInternal * a1 )	{ (&m_MaterialDict)->RemoveMaterial( a1 ); };
	void RemoveMaterialSubRect( IMaterialInternal * a1 )	{ (&m_MaterialDict)->RemoveMaterialSubRect( a1 ); };

	//---------------------------------

	ITexture *								FindTexture( const char* pTextureName, const char *pTextureGroupName, bool complain = true, int nAdditionalCreationFlags = 0  );

	bool									IsTextureLoaded( const char* pTextureName ) const;
	bool									GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info ) const;

	void									AddTextureAlias( const char *pAlias, const char *pRealName );
	void									RemoveTextureAlias( const char *pAlias );

	void									SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache );
	void									UpdateExcludedTextures( void );
	void									ClearForceExcludes( void );

	// Creates a procedural texture
	ITexture *								CreateProceduralTexture( const char	*pTextureName, 
																		const char			*pTextureGroupName, 
																		int					w, 
																		int					h, 
																		ImageFormat			fmt, 
																		int					nFlags );

   	virtual ICustomMaterialManager			*GetCustomMaterialManager();
	virtual ICompositeTextureGenerator		*GetCompositeTextureGenerator();


	//
	// Render targets
	//
	void									BeginRenderTargetAllocation();
	void									EndRenderTargetAllocation();	// Simulate an Alt-Tab in here, which causes a release/restore of all resources

	void FinishRenderTargetAllocation( void );
	
	void ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly( void );


	// Creates a texture for use as a render target
	ITexture *								CreateRenderTargetTexture( int w, 
																		int h, 
																		RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																		ImageFormat	format,
																		MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED );

	ITexture *								CreateNamedRenderTargetTextureEx(  const char *pRTName,				// Pass in NULL here for an unnamed render target.
																				int w, 
																				int h, 
																				RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																				ImageFormat format, 
																				MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
																				unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
																				unsigned int renderTargetFlags = 0 );

	ITexture *								CreateNamedRenderTargetTexture( const char *pRTName, 
																			int w, 
																			int h, 
																			RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																			ImageFormat format, 
																			MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
																			bool bClampTexCoords = true, 
																			bool bAutoMipMap = false );

	ITexture *								CreateNamedRenderTargetTextureEx2( const char *pRTName,				// Pass in NULL here for an unnamed render target.
																				int w, 
																				int h, 
																				RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																				ImageFormat format, 
																				MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
																				unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
																				unsigned int renderTargetFlags = 0 );

	ITexture *								CreateNamedMultiRenderTargetTexture( const char *pRTName,				// Pass in NULL here for an unnamed render target.
																					int w, 
																					int h, 
																					RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																					ImageFormat format, 
																					MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED, 
																					unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
																					unsigned int renderTargetFlags = 0 );


	// -----------------------------------------------------------
	
	bool									OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool									OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool									OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );
	bool OnSetFlexMesh( IMesh * a1, IMesh * a2, int a3 )	{ return (GetRenderContextInternal())->OnSetFlexMesh( a1, a2, a3 ); };
	bool OnSetColorMesh( IMesh * a1, IMesh * a2, int a3 )	{ return (GetRenderContextInternal())->OnSetColorMesh( a1, a2, a3 ); };
	bool OnSetPrimitiveType( IMesh * a1, MaterialPrimitiveType_t a2 )	{ return (GetRenderContextInternal())->OnSetPrimitiveType( a1, a2 ); };
	void SyncMatrices()	{ (GetRenderContextInternal())->SyncMatrices(); };
	void SyncMatrix( MaterialMatrixMode_t a1 )	{ (GetRenderContextInternal())->SyncMatrix( a1 ); };
	void									OnThreadEvent( uint32 threadEvent );
	ShaderAPITextureHandle_t GetLightmapTexture( ShaderAPITextureHandle_t a1 )	{ return (GetRenderContextInternal())->GetLightmapTexture( a1 ); };
	ShaderAPITextureHandle_t GetPaintmapTexture( ShaderAPITextureHandle_t a1 )	{ return (GetRenderContextInternal())->GetPaintmapTexture( a1 ); };


	// -----------------------------------------------------------
	// Lightmaps delegates
	// -----------------------------------------------------------
	void BeginLightmapAllocation()	{ (&m_Lightmaps)->BeginLightmapAllocation(); };

	void EndLightmapAllocation()
	{
		GetLightmaps()->EndLightmapAllocation();
		AllocateStandardTextures();
	}

	void CleanupLightmaps()
	{
		GetLightmaps()->CleanupLightmaps();
	}

	int AllocateLightmap( int a1, int a2, LightmapOffset_t a3, IMaterial * a4 )	{ return (&m_Lightmaps)->AllocateLightmap( a1, a2, a3, a4 ); };
	int AllocateWhiteLightmap( IMaterial * a1 )	{ return (&m_Lightmaps)->AllocateWhiteLightmap( a1 ); };
	int AllocateDynamicLightmap( LightmapOffset_t a1, int * a2, int a3 )	{ return (&m_Lightmaps)->AllocateDynamicLightmap( a1, a2, a3 ); };
	void									UpdateLightmap( int, LightmapOffset_t, LightmapOffset_t, float *, float *, float *, float * );
	int GetNumSortIDs()	{ return (&m_Lightmaps)->GetNumSortIDs(); };
	void GetSortInfo( MaterialSystem_SortInfo_t * a1 )	{ (&m_Lightmaps)->GetSortInfo( a1 ); };
	void GetLightmapPageSize( int a1, int * a2, int * a3 )	const { (&m_Lightmaps)->GetLightmapPageSize( a1, a2, a3 ); };
	void ResetMaterialLightmapPageInfo()	{ (&m_Lightmaps)->ResetMaterialLightmapPageInfo(); };
	int GetNumLightmapPages()	const { return (&m_Lightmaps)->GetNumLightmapPages(); };
	int GetLightmapWidth( int a1 )	const { return (&m_Lightmaps)->GetLightmapWidth( a1 ); };
	int GetLightmapHeight( int a1 )	const { return (&m_Lightmaps)->GetLightmapHeight( a1 ); };
	void BeginUpdateLightmaps()	{ (&m_Lightmaps)->BeginUpdateLightmaps(); };
	void EndUpdateLightmaps()	{ (&m_Lightmaps)->EndUpdateLightmaps(); };


	// -----------------------------------------------------------
	// Paint map delegates
	// -----------------------------------------------------------
	void RegisterPaintmapDataManager( IPaintmapDataManager * a1 )	{ return (&m_Paintmaps)->RegisterPaintmapDataManager( a1 ); };
	void BeginUpdatePaintmaps()	{ (&m_Paintmaps)->BeginUpdatePaintmaps(); };
	void EndUpdatePaintmaps()	{ (&m_Paintmaps)->EndUpdatePaintmaps(); };
	void UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects );

	// -----------------------------------------------------------
	// Stereo
	// -----------------------------------------------------------
	bool IsStereoSupported();
	bool IsStereoActiveThisFrame() const;
	void NVStereoUpdate();

	bool m_bStereoBoolsInitialized;
	bool m_bIsStereoSupported;
	bool m_bIsStereoActiveThisFrame;

	// -----------------------------------------------------------
	// Render context delegates
	// -----------------------------------------------------------

	// IMaterialSystem
	void ClearBuffers( bool a1, bool a2, bool a3 )	{ (GetRenderContextInternal())->ClearBuffers( a1, a2, a3 ); };

	// IMaterialSystemInternal
	IMaterial * GetCurrentMaterial()	{ return (GetRenderContextInternal())->GetCurrentMaterial(); };
	int GetLightmapPage()	{ return (GetRenderContextInternal())->GetLightmapPage(); };
	ITexture * GetLocalCubemap()	{ return (GetRenderContextInternal())->GetLocalCubemap(); };
	void ForceDepthFuncEquals( bool a1 )	{ (GetRenderContextInternal())->ForceDepthFuncEquals( a1 ); };
	MaterialHeightClipMode_t GetHeightClipMode()	{ return (GetRenderContextInternal())->GetHeightClipMode(); };
	bool InFlashlightMode()	const { return (GetRenderContextInternal())->InFlashlightMode(); };
	bool IsRenderingPaint()	const { return (GetRenderContextInternal())->IsRenderingPaint(); };
	bool IsCascadedShadowMapping()	const { return (GetRenderContextInternal())->IsCascadedShadowMapping(); };

	// IShaderUtil
	void BindStandardTexture( Sampler_t a1, TextureBindFlags_t a2, StandardTextureId_t a3 )	{ (&m_HardwareRenderContext)->BindStandardTexture( a1, a2, a3 ); };

	ShaderAPITextureHandle_t GetStandardTexture( StandardTextureId_t a1 )	{ return (&m_HardwareRenderContext)->GetStandardTexture( a1 ); };

	void BindStandardVertexTexture( VertexTextureSampler_t a1, StandardTextureId_t a2 )	{ (&m_HardwareRenderContext)->BindStandardVertexTexture( a1, a2 ); };
	void GetLightmapDimensions( int * a1, int * a2 )	{ (&m_HardwareRenderContext)->GetLightmapDimensions( a1, a2 ); };
	void GetStandardTextureDimensions( int * a1, int * a2, StandardTextureId_t a3 )	{ (&m_HardwareRenderContext)->GetStandardTextureDimensions( a1, a2, a3 ); };
	float GetSubDHeight()	{ return (&m_HardwareRenderContext)->GetSubDHeight(); };

	MorphFormat_t GetBoundMorphFormat()	{ return (&m_HardwareRenderContext)->GetBoundMorphFormat(); };
	ITexture * GetRenderTargetEx( int a1 )	{ return (&m_HardwareRenderContext)->GetRenderTargetEx( a1 ); };
	void DrawClearBufferQuad( unsigned char a1, unsigned char a2, unsigned char a3, unsigned char a4, bool a5, bool a6, bool a7 )	{ (&m_HardwareRenderContext)->DrawClearBufferQuad( a1, a2, a3, a4, a5, a6, a7 ); };
	int MaxHWMorphBatchCount()	const { return (g_pMorphMgr)->MaxHWMorphBatchCount(); };
	void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* a1 )	{ (g_pColorCorrectionSystem)->GetCurrentColorCorrection( a1 ); };
	virtual ShaderAPITextureHandle_t		GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel = 0 );

	virtual void SpinPresent( uint nFrames );

	MaterialLock_t							Lock();
	void									Unlock( MaterialLock_t );
	CMatCallQueue *							GetRenderCallQueue();
	uint									GetRenderThreadId() const { return m_nRenderThreadID; }
	void									UnbindMaterial( IMaterial *pMaterial );

	virtual void							CompactMemory();

	// Get stats on GPU memory usage
	virtual void							GetGPUMemoryStats( GPUMemoryStats &stats );

	bool									IsLevelLoadingComplete() const;

	void									OnAsyncTextureDataComplete( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, AsyncTextureLoadError_t loadError );

	// -----------------------------------------------------------
private:
	void DebugPrintUsedMaterials( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_DebugPrintUsedMaterials_register{ this, "mat_showmaterials", &CMaterialSystem::DebugPrintUsedMaterials, "Show materials.", 0 };
	void DebugPrintUsedMaterialsVerbose( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_DebugPrintUsedMaterialsVerbose_register{ this, "mat_showmaterialsverbose", &CMaterialSystem::DebugPrintUsedMaterialsVerbose, "Show materials (verbose version).", 0 };
	void DebugPrintUsedTextures( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_DebugPrintUsedTextures_register{ this, "mat_showtextures", &CMaterialSystem::DebugPrintUsedTextures, "Show used textures.", 0 };
	void ReloadAllMaterials( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_ReloadAllMaterials_register{ this, "mat_reloadallmaterials", &CMaterialSystem::ReloadAllMaterials, "Reloads all materials", FCVAR_CHEAT };
	void ReloadMaterials( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_ReloadMaterials_register{ this, "mat_reloadmaterial", &CMaterialSystem::ReloadMaterials, "Reloads a single material", FCVAR_CHEAT };
	void ReloadTextures( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_ReloadTextures_register{ this, "mat_reloadtextures", &CMaterialSystem::ReloadTextures, "Reloads all textures", FCVAR_CHEAT };
	void DebugPrintAspectRatioInfo( const CCommand &args );
	CConCommandMemberAccessor< CMaterialSystem > m_DebugPrintAspectRatioInfo_register{ this, "mat_showaspectratioinfo", &CMaterialSystem::DebugPrintAspectRatioInfo, "Spew info about the hardware aspect ratio", FCVAR_DEVELOPMENTONLY };


	friend void* InstantiateMaterialSystemV76Interface();
	friend CMaterialSystem *CMatLightmaps::GetMaterialSystem() const;
	friend CMaterialSystem *CMatPaintmaps::GetMaterialSystem() const;

	void ThreadExecuteQueuedContext( CMatQueuedRenderContext *pContext );

	IThreadPool * CreateMatQueueThreadPool();
	void DestroyMatQueueThreadPool();

	
	void									ServiceAsyncTextureLoads();
	void									ServiceEndFramePriorToNextContext();

	// -----------------------------------------------------------

	CMaterialDict							m_MaterialDict;
	CMatLightmaps							m_Lightmaps;
	CMatPaintmaps							m_Paintmaps;


	static thread_local IMatRenderContextInternal * m_pRenderContext;
	CMatRenderContext						m_HardwareRenderContext;

	CMatQueuedRenderContext					m_QueuedRenderContexts[2];
	int										m_iCurQueuedContext;

	MaterialThreadMode_t					m_ThreadMode;
	MaterialThreadMode_t					m_IdealThreadMode;
	int										m_nServiceThread;

	//---------------------------------

	char *									m_pShaderDLL;
	CSysModule *							m_ShaderHInst; // Used to dynamically load the shader DLL
	CreateInterfaceFn						m_ShaderAPIFactory;

	int										m_nAdapter;
	int										m_nAdapterFlags;
	int 									m_nConfigurationFlags; // MATCONFIG_FLAGS_xxxx
	ShaderDisplayMode_t						m_nAdapterInfo;

	//---------------------------------

	IMaterialProxyFactory *					m_pMaterialProxyFactory;
	IClientMaterialSystem *					m_pClientMaterialSystemInterface;
	
	//---------------------------------
	// Callback methods for releasing + restoring video memory
	std::vector< MaterialBufferReleaseFunc_t > m_ReleaseFunc;
	std::vector< MaterialBufferRestoreFunc_t > m_RestoreFunc;
	std::vector< EndFrameCleanupFunc_t > m_EndFrameCleanupFunc;
	class COnLevelShutdownFunc
	{
	public:
		COnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData )
			:
			m_Func(func),
			m_pUserData(pUserData)
		{
			// Do nothing... Is there a CUtlPair class or something like that? 
		}
		bool operator==(const COnLevelShutdownFunc & other) const
		{
			return ( (m_Func == other.m_Func) && (m_pUserData == other.m_pUserData) );
		}
		OnLevelShutdownFunc_t	m_Func;
		void *					m_pUserData;
	};
	std::vector< COnLevelShutdownFunc > m_OnLevelShutdownFuncs;

	std::vector< EndFramePriorToNextContextFunc_t > m_EndFramePriorToNextContextFunc;

	//---------------------------------

	bool                                    m_bRequestedEditorMaterials;
	bool                                    m_bRequestedGBuffers;

	//---------------------------------

	// Store texids for various things
	ShaderAPITextureHandle_t				m_FullbrightLightmapTextureHandle;
	ShaderAPITextureHandle_t				m_FullbrightBumpedLightmapTextureHandle;
	ShaderAPITextureHandle_t 				m_BlackTextureHandle;
	ShaderAPITextureHandle_t				m_BlackAlphaZeroTextureHandle;
	ShaderAPITextureHandle_t				m_FlatNormalTextureHandle;
	ShaderAPITextureHandle_t				m_FlatSSBumpTextureHandle;
	ShaderAPITextureHandle_t				m_GreyTextureHandle;
	ShaderAPITextureHandle_t				m_GreyAlphaZeroTextureHandle;
	ShaderAPITextureHandle_t				m_WhiteTextureHandle;
	ShaderAPITextureHandle_t				m_LinearToGammaTableTextureHandle; //linear to gamma srgb conversion lookup for the current hardware
	ShaderAPITextureHandle_t				m_LinearToGammaIdentityTableTextureHandle; //An identity lookup for when srgb writes are off
	ShaderAPITextureHandle_t				m_MaxDepthTextureHandle; //a 1x1 texture with maximum depth values.

	// Have we allocated the standard lightmaps?
	bool									m_StandardTexturesAllocated;

	// Do we need to restore mananged resources next time RestoreShaderObjects is called
	bool									m_bRestoreManangedResources;

	// Deferred material reload ( so we don't call it when ALT-TABd
	bool									m_bDeferredMaterialReload;
	char									*m_pSubString; // allocated substring for deferred material reload

	//---------------------------------
	// Shared materials used for debugging....
	//---------------------------------

	enum BufferClearType_t //bClearColor + ( bClearAlpha << 1 ) + ( bClearDepth << 2 )
	{
		BUFFER_CLEAR_NONE,
		BUFFER_CLEAR_COLOR,
		BUFFER_CLEAR_ALPHA,
		BUFFER_CLEAR_COLOR_AND_ALPHA,
		BUFFER_CLEAR_DEPTH,
		BUFFER_CLEAR_COLOR_AND_DEPTH,
		BUFFER_CLEAR_ALPHA_AND_DEPTH,
		BUFFER_CLEAR_COLOR_AND_ALPHA_AND_DEPTH,

		BUFFER_CLEAR_TYPE_COUNT
	};

    IMaterialInternal *						m_pBufferClearObeyStencil[BUFFER_CLEAR_TYPE_COUNT];
	IMaterialInternal *						m_pReloadZcullMaterial;
	IMaterialInternal *						m_pDrawFlatMaterial;
	IMaterialInternal *						m_pRenderTargetBlitMaterial;
	IMaterialInternal *						m_pBIKPreloadMaterial; // PORTAL2 - a dummy material which is used to ensure that the (~2 KB) bink shaders are always loaded

	//---------------------------------

	const char *							m_pForcedTextureLoadPathID;
	uint									m_nRenderThreadID;
	// nesting counter for render target allocation
	int	  								    m_nAllocatingRenderTargets;
	bool                                    m_bDisableRenderTargetAllocationForever;

	bool									m_bInStubMode;
	bool									m_bGeneratedConfig;
	bool									m_bInFrame;
	bool									m_bForcedSingleThreaded;
	bool									m_bAllowQueuedRendering;
	bool									m_bThreadHasOwnership;
	bool									m_bLevelLoadingComplete;

	CTSQueue< AsyncTextureLoad_t >			m_QueuedAsyncTextureLoads;
	AsyncTextureLoad_t						m_AsyncTextureLoad;
	AsyncTextureLoad_t *					m_pActiveAsyncTextureLoad;

	//---------------------------------

	CCustomMaterialManager					m_CustomMaterialManager;
	CCompositeTextureGenerator				m_CompositeTextureGenerator;


	CJob *									m_pActiveAsyncJob;
	std::vector< uint32 >						m_threadEvents;

	IThreadPool *							m_pMatQueueThreadPool;

	inline void InitializePS3Fonts(){};
};


//-----------------------------------------------------------------------------

inline CMaterialSystem *CMatLightmaps::GetMaterialSystem() const
{
	return ( ( CMaterialSystem * ) ( (uint8 *)this - __builtin_offsetof( CMaterialSystem, m_Lightmaps ) ) );
}

inline CMaterialSystem *CMatPaintmaps::GetMaterialSystem() const
{
	return ( ( CMaterialSystem * ) ( (uint8 *)this - __builtin_offsetof( CMaterialSystem, m_Paintmaps ) ) );
}

//-----------------------------------------------------------------------------
