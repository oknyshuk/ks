#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "itextureinternal.h"
class ITexture;
class ITextureInternal;
class IVTFTexture;

enum
{
	COLOR_CORRECTION_MAX_TEXTURES = 4,
	COLOR_CORRECTION_TEXTURE_SIZE = 32
};



//-----------------------------------------------------------------------------
// Texture manager interface
//-----------------------------------------------------------------------------
class ITextureManager
{
public:
	// Initialization + shutdown
	void Init( int nFlags );
	void Shutdown();

	// Allocate, free standard render target textures
	void AllocateStandardRenderTargets( );
	void FreeStandardRenderTargets();

	//Some render targets are managed by code outside of the materialsystem but are used by the materialsystem all the time.
	void CacheExternalStandardRenderTargets();

	// Creates a procedural texture
	// NOTE: Passing in NULL as a texture name will cause it to not
	// be able to be looked up by name using FindOrLoadTexture.
	// NOTE: Using compressed textures is not allowed; also 
	// Also, you may not get a texture with the requested size or format;
	// you'll get something close though.
	ITextureInternal *CreateProceduralTexture( 
		const char			*pTextureName, 
		const char			*pTextureGroupName,
		int					w,
		int					h, 
		int					d,
		ImageFormat			fmt,
		int					nFlags );

	// Creates a texture which is a render target
	ITextureInternal *CreateRenderTargetTexture( 
		const char				*pRTName,	// NULL for auto-generated name
		int						w, 
		int						h, 
		RenderTargetSizeMode_t	sizeMode, 
		ImageFormat				fmt, 
		RenderTargetType_t		type, 
		unsigned int			textureFlags,
		unsigned int			renderTargetFlags,
		bool					bMultipleTargets );

	// Loads a texture from disk
	ITextureInternal *FindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 );

	// Call this to reset the filtering state
	void ResetTextureFilteringState();

	// Reload all textures
	void ReloadTextures( void );

	// These two are used when we lose our video memory due to a mode switch etc
	void ReleaseTextures( bool bReleaseManaged = true );
	void RestoreRenderTargets( void );
	void RestoreNonRenderTargetTextures( void );

	// delete any texture that has a refcount <= 0
	void RemoveUnusedTextures( void );
	void DebugPrintUsedTextures( void );

	// Request a texture ID
	int	RequestNextTextureID();

	// Get at a couple standard textures
	ITextureInternal *ErrorTexture();
	ITextureInternal *NormalizationCubemap();
	ITextureInternal *SignedNormalizationCubemap();
	ITextureInternal *ColorCorrectionTexture( int index );
	ITextureInternal *ShadowNoise2D();
	ITextureInternal *SSAONoise2D();	
	ITextureInternal *IdentityLightWarp();
	ITextureInternal *FullFrameDepthTexture();
	ITextureInternal *StereoParamTexture(); 

	// Generates an error texture pattern
	void GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture );

	// Updates the color correction state
	void SetColorCorrectionTexture( int i, ITextureInternal *pTexture );

	void ForceAllTexturesIntoHardware( void );

	bool IsTextureLoaded( const char *pTextureName );
	
	bool GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info );

	void RemoveTexture( ITextureInternal *pTexture );

	void AddTextureAlias( const char *pAlias, const char *pRealName );
	void RemoveTextureAlias( const char *pAlias );

	void SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache );
	void UpdateExcludedTextures( void );
	void ClearForceExcludes( void );

	//Releases texture memory bits for temporary render targets, does NOT destroy the CTexture entirely
	void ReleaseTempRenderTargetBits( void );

	// See CL_HandlePureServerWhitelist for a description of the pure server stuff.
	void ReloadFilesInList( IFileList *pFilesToReload );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
inline ITextureManager *TextureManager()
{
	extern ITextureManager *g_pTextureManager;
	return g_pTextureManager;
}
