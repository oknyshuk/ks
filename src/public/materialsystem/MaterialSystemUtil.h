//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#ifndef MATERIALSYSTEMUTIL_H
#define MATERIALSYSTEMUTIL_H


#include "bitmap/imageformat.h" //ImageFormat enum definition
#include "materialsystem/imaterialsystem.h"  // RenderTargetSizeMode_t and MaterialRenderTargetDepth_t definition
#include "materialsystem/itexture.h"
#include <utility>
//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterial;
class KeyValues;

class KeyValues;


//-----------------------------------------------------------------------------
// Little utility class to deal with material references
//-----------------------------------------------------------------------------
class CMaterialReference
{
public:
	// constructor, destructor
	CMaterialReference( char const* pMaterialName = 0, const char *pTextureGroupName = 0, bool bComplain = true );
	~CMaterialReference();

	// Attach to a material
	void Init( const char* pMaterialName, const char *pTextureGroupName, bool bComplain = true );
	void Init( const char *pMaterialName, KeyValues *pVMTKeyValues );
	void Init( IMaterial* pMaterial );
	void Init( CMaterialReference& ref );
	void Init( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues );

	// Detach from a material
	void Shutdown( bool bDeleteIfUnreferenced = false );
	bool IsValid() { return m_pMaterial != 0; }

	// Automatic casts to IMaterial
	operator IMaterial*() { return m_pMaterial; }
	operator const IMaterial *() const { return m_pMaterial; }
	IMaterial* operator->() { return m_pMaterial; }

	// Copies take a reference; moves transfer it.
	CMaterialReference( const CMaterialReference &ref ) : m_pMaterial( nullptr ) { Init( ref.m_pMaterial ); }
	CMaterialReference( CMaterialReference &&ref ) : m_pMaterial( std::exchange( ref.m_pMaterial, nullptr ) ) {}
	const CMaterialReference& operator=( const CMaterialReference &ref );
	CMaterialReference &operator=( CMaterialReference &&ref )
	{
		if ( this != &ref )
		{
			Shutdown();
			m_pMaterial = std::exchange( ref.m_pMaterial, nullptr );
		}
		return *this;
	}

private:
	IMaterial* m_pMaterial;
};

//-----------------------------------------------------------------------------
// Little utility class to deal with texture references
//-----------------------------------------------------------------------------
class CTextureReference
{
public:
	// constructor, destructor
	CTextureReference( );
	CTextureReference( const CTextureReference &ref );
	~CTextureReference();

	// Attach to a texture
	void Init( char const* pTexture, const char *pTextureGroupName, bool bComplain = true, int nAdditionalCreationFlags = 0 );
	void InitProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, ImageFormat fmt, int nFlags );
	void InitRenderTarget( int w, int h, RenderTargetSizeMode_t sizeMode, ImageFormat fmt, MaterialRenderTargetDepth_t depth, bool bHDR, char *pStrOptionalName = nullptr );
	void Init( ITexture* pTexture );

	// Detach from a texture
	void Shutdown( bool bDeleteIfUnReferenced = false );
	bool IsValid() { return m_pTexture != 0; }

	// Automatic casts to ITexture
	operator ITexture*() { return m_pTexture; }
	operator ITexture const*() const { return m_pTexture; }
	ITexture* operator->() { return m_pTexture; }

	// Copies take a reference; moves transfer it.
	CTextureReference( CTextureReference &&ref ) : m_pTexture( std::exchange( ref.m_pTexture, nullptr ) ) {}
	const CTextureReference& operator=( const CTextureReference &ref );
	CTextureReference &operator=( CTextureReference &&ref )
	{
		if ( this != &ref )
		{
			Shutdown();
			m_pTexture = std::exchange( ref.m_pTexture, nullptr );
		}
		return *this;
	}

private:
	ITexture* m_pTexture;
};


#endif // !MATERIALSYSTEMUTIL_H
