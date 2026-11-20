#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
#include "bitmap/imageformat.h" // ImageFormat defn.
#include "materialsystem/imaterialsystem.h"

class IVTFTexture;
class ITexture;


struct AsyncTextureContext_t
{
	ITexture		*m_pTexture;

	// snapshot at the point of async start
	// used to resolve disparity at moment of latent async arrival
	unsigned int	m_nInternalFlags;
	int				m_nDesiredTempDimensionLimit;
	int				m_nActualDimensionLimit;

	// Keeps track of the VTF texture in case the shader api texture gets
	// created the next frame. Generating the texture from the file can 
	// then be split in multiple parts across frames.
	IVTFTexture		*m_pVTFTexture;
};

//-----------------------------------------------------------------------------
// This will get called on procedural textures to re-fill the textures
// with the appropriate bit pattern. Calling Download() will also
// cause this interface to be called. It will also be called upon
// mode switch, or on other occasions where the bits are discarded.
//-----------------------------------------------------------------------------
class ITextureRegenerator
{
public:
	// Wraps a regenerator object that outlives the texture it is set on (a member or a static).
	// It provides
	//	void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	// which is called when the texture bits need to be regenerated.  Use the VTFTexture interface,
	// which has been set up with the appropriate texture size + format.  The rect specifies which
	// part of the texture needs to be updated; you can choose to update all of the bits if you prefer.
	// A Release() of the object, if it has one, is called when the texture lets go of the regenerator.
	template < class T >
		requires ( !std::is_same_v< std::remove_cvref_t< T >, ITextureRegenerator > )
	explicit ITextureRegenerator( T &regenerator )
		: m_Regenerate( [&regenerator]( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect ) { regenerator.RegenerateTextureBits( pTexture, pVTFTexture, pRect ); } )
	{
		if constexpr ( requires { regenerator.Release(); } )
			m_Release = [&regenerator] { regenerator.Release(); };
	}

	// A regenerator that owns the object it calls; Release() destroys both.
	template < class T >
	static ITextureRegenerator *Owned( T regenerator )
	{
		auto *pRegen = new ITextureRegenerator( std::move( regenerator ), OwnedTag{} );
		pRegen->m_bOwned = true;
		return pRegen;
	}

	ITextureRegenerator( const ITextureRegenerator & ) = delete;
	ITextureRegenerator &operator=( const ITextureRegenerator & ) = delete;

	void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect ) { m_Regenerate( pTexture, pVTFTexture, pRect ); }

	// This will be called when the regenerator needs to be deleted
	// which will happen when the texture is destroyed
	void Release()
	{
		if ( m_bOwned )
			delete this;
		else if ( m_Release )
			m_Release();
	}

private:
	struct OwnedTag {};
	template < class T >
	ITextureRegenerator( T &&regenerator, OwnedTag )
		: m_Regenerate( [r = std::forward< T >( regenerator )]( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect ) mutable { r.RegenerateTextureBits( pTexture, pVTFTexture, pRect ); } )
	{
	}

	std::move_only_function< void( ITexture *, IVTFTexture *, Rect_t * ) > m_Regenerate;
	std::move_only_function< void() > m_Release;
	bool m_bOwned = false;
};

class ITexture
{
public:
	// Various texture polling methods
	const char *GetName( void ) const;
	int GetMappingWidth() const;
	int GetMappingHeight() const;
	int GetActualWidth() const;
	int GetActualHeight() const;
	int GetNumAnimationFrames() const;
	bool IsTranslucent() const;
	bool IsMipmapped() const;

	void GetLowResColorSample( float s, float t, float *color ) const;

	// Gets texture resource data of the specified type.
	// Params:
	//		eDataType		type of resource to retrieve.
	//		pnumBytes		on return is the number of bytes available in the read-only data buffer or is undefined
	// Returns:
	//		pointer to the resource data, or NULL
	void *GetResourceData( uint32 eDataType, std::size_t *pNumBytes ) const;

	// Methods associated with reference count
	void IncrementReferenceCount( void );
	void DecrementReferenceCount( void );

	inline void AddRef() { IncrementReferenceCount(); }
	inline void Release() { DecrementReferenceCount(); }

	// Used to modify the texture bits (procedural textures only)
	void SetTextureRegenerator( ITextureRegenerator *pTextureRegen, bool releaseExisting = true );

	// Reconstruct the texture bits in HW memory

	// If rect is not specified, reconstruct all bits, otherwise just
	// reconstruct a subrect.
	void Download( Rect_t *pRect = 0, int nAdditionalCreationFlags = 0 );

	// Uses for stats. . .get the approximate size of the texture in it's current format.
	int GetApproximateVidMemBytes( void ) const;

	// Returns true if the texture data couldn't be loaded.
	bool IsError() const;

	// NOTE: Stuff after this is added after shipping HL2.

	// For volume textures
	bool IsVolumeTexture() const;
	int GetMappingDepth() const;
	int GetActualDepth() const;

	ImageFormat GetImageFormat() const;

	// Various information about the texture
	bool IsRenderTarget() const;
	bool IsCubeMap() const;
	bool IsNormalMap() const;
	bool IsProcedural() const;
	bool IsDefaultPool() const;

	void DeleteIfUnreferenced();


	// swap everything except the name with another texture
	void SwapContents( ITexture *pOther );

	// Retrieve the vtf flags mask
	unsigned int GetFlags( void ) const;

	// Force LOD override (automatically downloads the texture)
	void ForceLODOverride( int iNumLodsOverrideUpOrDown );

	// Force exclude override (automatically downloads the texture)
	void ForceExcludeOverride( int iExcludeOverride );

	//swap out the active texture surface, only valid for MultiRenderTarget textures
	void AddDownsizedSubTarget( const char *szName, int iDownsizePow2, MaterialRenderTargetDepth_t depth );
	void SetActiveSubTarget( const char *szName ); //NULL to return to base target

	int GetReferenceCount() const;

	bool IsTempExcluded() const;
	bool CanBeTempExcluded() const;

	bool FinishAsyncDownload( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, bool bAbort, float flMaxTimeMs );

	bool IsForceExcluded() const;
	bool ClearForceExclusion();
};


inline bool IsErrorTexture( ITexture *pTex )
{
	return !pTex || pTex->IsError();
}
