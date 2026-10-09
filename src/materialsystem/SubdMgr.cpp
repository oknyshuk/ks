// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Manager for texture containing subd data

#include "isubdinternal.h"
#include "materialsystem_global.h"
#include "studio.h"

constexpr auto SUBD_TEXTURE_WIDTH = 30;
constexpr auto SUBD_TEXTURE_HEIGHT = 8192; // If we need more than 8192 patches, we may be in trouble

//-----------------------------------------------------------------------------
// Subdivision surface manager class
//-----------------------------------------------------------------------------
class CSubDMgr : public ISubDMgr
{
public:
	CSubDMgr();

	virtual bool ShouldAllocateTextures();
	virtual void AllocateTextures();
	virtual void FreeTextures();
	virtual void ReacquireResources();
	virtual void ReleaseResources();
	
	virtual int GetWidth();
	virtual int GetHeight();
	virtual int GetOffsetGeometry();
	virtual int GetOffsetTangents();

	virtual float* Lock( int nNumRows );
	virtual void Unlock();

	virtual ShaderAPITextureHandle_t SubDTexture();

private:
	ShaderAPITextureHandle_t m_hSysMem;
	ShaderAPITextureHandle_t m_hDefaultPool;
	int m_nRowsLocked;
	int m_nOffsetGeometry;
	int m_nOffsetTangents;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CSubDMgr s_SubDMgr;
ISubDMgr *g_pSubDMgr = &s_SubDMgr;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSubDMgr::CSubDMgr()
{
	m_nRowsLocked = 0;
	m_nOffsetGeometry = 0;
	m_nOffsetTangents = 0;
	m_hSysMem = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_hDefaultPool = INVALID_SHADERAPI_TEXTURE_HANDLE;
}

bool CSubDMgr::ShouldAllocateTextures()
{
	return true;
//	return g_pMaterialSystemHardwareConfig->SupportsSubdivisionSurfaces(); 
}

int CSubDMgr::GetWidth()
{
	return SUBD_TEXTURE_WIDTH;
}

int CSubDMgr::GetHeight()
{
	return SUBD_TEXTURE_HEIGHT;
}

int CSubDMgr::GetOffsetGeometry()
{
	return m_nOffsetGeometry;
}

int CSubDMgr::GetOffsetTangents()
{
	return m_nOffsetTangents;
}

void CSubDMgr::AllocateTextures()
{
	ReacquireResources();
}

void CSubDMgr::FreeTextures()
{
	ReleaseResources();
}

void CSubDMgr::ReleaseResources()
{
	if ( g_pShaderAPI->IsTexture( m_hSysMem ) )
	{
		g_pShaderAPI->DeleteTexture( m_hSysMem );
		m_hSysMem = INVALID_SHADERAPI_TEXTURE_HANDLE;
	}

	if ( g_pShaderAPI->IsTexture( m_hDefaultPool ) )
	{
		g_pShaderAPI->DeleteTexture( m_hDefaultPool );
		m_hDefaultPool = INVALID_SHADERAPI_TEXTURE_HANDLE;
	}
}

void CSubDMgr::ReacquireResources()
{
	contract_assert( m_hSysMem == INVALID_SHADERAPI_TEXTURE_HANDLE );
	contract_assert( m_hDefaultPool == INVALID_SHADERAPI_TEXTURE_HANDLE );

	// These scratch textures are ~4mb (30*8192*16 bytes) and can
	// accommodate data for 8192 patches
	g_pShaderAPI->CreateTextures( &m_hSysMem, 1, SUBD_TEXTURE_WIDTH, SUBD_TEXTURE_HEIGHT, 0, IMAGE_FORMAT_RGBA32323232F, 1, 1, TEXTURE_CREATE_SYSMEM, "_SubDSysMemTexture", "SubD Textures" );

	g_pShaderAPI->CreateTextures( &m_hDefaultPool, 1, SUBD_TEXTURE_WIDTH, SUBD_TEXTURE_HEIGHT, 0, IMAGE_FORMAT_RGBA32323232F, 1, 1, TEXTURE_CREATE_DEFAULT_POOL | TEXTURE_CREATE_VERTEXTEXTURE, "_SubDVidMemTexture", "SubD Textures" );
	g_pShaderAPI->ModifyTexture( m_hDefaultPool );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_NEAREST );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_NEAREST );
}

float *CSubDMgr::Lock( int nRows )
{
	m_nRowsLocked = nRows;
	return (float *) g_pShaderAPI->LockTex( m_hSysMem );
}

void CSubDMgr::Unlock()
{
	g_pShaderAPI->UnlockTex( m_hSysMem );
	g_pShaderAPI->UpdateTexture( 0, 0, SUBD_TEXTURE_WIDTH, m_nRowsLocked, m_hDefaultPool, m_hSysMem );
}

ShaderAPITextureHandle_t CSubDMgr::SubDTexture()
{
	return m_hDefaultPool;
}
