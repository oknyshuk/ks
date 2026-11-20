#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.strtools;

#include "materialsystem/itexture.h"
#include "shaderapi/ishaderapi.h"

enum Sampler_t;
class IFileList;

enum RenderTargetType_t
{
	NO_RENDER_TARGET = 0,
	// GR - using shared depth buffer
	RENDER_TARGET = 1,
	// GR - using own depth buffer
	RENDER_TARGET_WITH_DEPTH = 2,
	// GR - no depth buffer
	RENDER_TARGET_NO_DEPTH = 3,
	// only cares about depth buffer
	RENDER_TARGET_ONLY_DEPTH = 4,
};

// The texture implementations; every texture is created as exactly one of them (ctexture.cpp).
// ITexture and ITextureInternal forward each call to the implementation named by the kind.
enum class TextureKind : unsigned char
{
	Standard,
	MultiRenderTarget,
	Reference,
};

class ITextureInternal : public ITexture
{
public:
	TextureKind Kind() const { return m_Kind; }


	void Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags );
	void Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 = SHADER_SAMPLER_INVALID );

	void GetReflectivity( Vector& reflectivity );

	// Set this as the render target, return false for failure
	bool SetRenderTarget( int nRenderTargetID );

	// Releases the texture's hw memory
	void Release();

	// Called before Download() on restore. Gives render targets a change to change whether or
	// not they force themselves to have a separate depth buffer due to AA.
	void OnRestore();

	// Resets the texture's filtering and clamping mode
	void SetFilteringAndClampingMode();

	// Used by tools.... loads up the non-fallback information about the texture 
	void Precache();

	// Stretch blit the framebuffer into this texture.
	void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = nullptr, Rect_t *pDstRect = nullptr );
	void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = nullptr, Rect_t *pDstRect = nullptr );

	ITexture *GetEmbeddedTexture( int nIndex );

	// Get the shaderapi texture handle associated w/ a particular frame
	ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel =0 );


	ImageFormat GetImageFormat() const;

	// Creates a new texture
	static ITextureInternal *CreateFileTexture( const char *pFileName, const char *pTextureGroupName );
	
	static ITextureInternal *CreateProceduralTexture( 
		const char			*pTextureName,
		const char			*pTextureGroupName, 
		int					w, 
		int					h,
		int					d,
		ImageFormat			fmt, 
		int					nFlags );

	static ITextureInternal *CreateRenderTarget( 
		const char *pRTName, // NULL for an auto-generated name.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type,
		unsigned int textureFlags, 
		unsigned int renderTargetFlags,
		bool bMultipleTargets );

	static void ChangeRenderTarget(
		ITextureInternal *pTexture,
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type,
		unsigned int textureFlags, 
		unsigned int renderTargetFlags );

	static ITextureInternal *CreateReferenceTextureFromHandle(
		const char *pTextureName,
		const char *pTextureGroupName,
		ShaderAPITextureHandle_t hTexture );

	static void Destroy( ITextureInternal *pTexture );

	// Set this as the render target, return false for failure
	bool SetRenderTarget( int nRenderTargetID, ITexture* pDepthTexture );

	// Bind this to a vertex texture sampler
	void BindVertexTexture( VertexTextureSampler_t sampler, int frameNum = 0 );

	void MarkAsPreloaded( bool bSet );
	bool IsPreloaded() const;

	void MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude = false );
	bool UpdateExcludedState();

	bool IsTempRenderTarget( void ) const;

	// Reload any files the texture is responsible for.
	void ReloadFilesInList( IFileList *pFilesToReload );

	bool IsMultiRenderTarget( void );


	bool MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit );

	bool IsForceExcluded() const;
	bool ClearForceExclusion();

	bool IsAsyncDone() const;

protected:
	explicit ITextureInternal( TextureKind kind ) : m_Kind( kind ) {}

	// A destructor calls the functions of its own class, not of a more derived one; a base class
	// destructor uses this to dispatch the way virtual calls did.
	void BecomeKind( TextureKind kind ) { m_Kind = kind; }

private:
	TextureKind m_Kind;
};

inline bool IsTextureInternalEnvCubemap( const ITextureInternal *pTexture )
{
	return ( pTexture == ( ITextureInternal * )-1 );
}

//-----------------------------------------------------------------------------
// Ensures that caller provided names are consistent to the dictionary
//-----------------------------------------------------------------------------
inline char *NormalizeTextureName( const char *pName, char *pOutName, int nOutNameSize )
{
	// hdr textures have an ldr version and need to resolve correctly
	int nLen = V_strlen( pName ) + 1;
	if ( nLen <= 5 || V_stricmp( pName + nLen - 5, ".hdr" ) )
	{
		// strip any non .hdr extension
		V_StripExtension( pName, pOutName, nOutNameSize ); 
	}
	else
	{
		// keep .hdr extension
		V_strncpy( pOutName, pName, nOutNameSize );
	}

	V_strlower( pOutName );
	V_FixSlashes( pOutName, '/' );

	return pOutName;
}
