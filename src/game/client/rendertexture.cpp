// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements local hooks into named renderable textures.
// See matrendertexture.cpp in material system for list of available RT's

#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "rendertexture.h"

void ReleaseRenderTargets( int nChangeFlags );

void AddReleaseFunc( void )
{
	static bool bAdded = false;
	if( !bAdded )
	{
		bAdded = true;
		materials->AddReleaseFunc( ReleaseRenderTargets );
	}
}

//=============================================================================
// Stereo Param Texture
//=============================================================================
static CTextureReference s_pStereoParamTexture;
ITexture *GetStereoParamTexture( void )
{
	if ( !s_pStereoParamTexture )
	{
		s_pStereoParamTexture.Init( materials->FindTexture( "stereoparam", TEXTURE_GROUP_OTHER ) );
		AddReleaseFunc();
	}

	return s_pStereoParamTexture;
}

//=============================================================================
// Power of Two Frame Buffer Texture
//=============================================================================
static CTextureReference s_pPowerOfTwoFrameBufferTexture;
ITexture *GetPowerOfTwoFrameBufferTexture( void )
{

	if ( !s_pPowerOfTwoFrameBufferTexture )
	{
		s_pPowerOfTwoFrameBufferTexture.Init( materials->FindTexture( "_rt_PowerOfTwoFB", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pPowerOfTwoFrameBufferTexture;
}

//=============================================================================
// Fullscreen Texture
//=============================================================================
static CTextureReference s_pFullscreenTexture;
ITexture *GetFullscreenTexture( void )
{
	if ( !s_pFullscreenTexture )
	{
		s_pFullscreenTexture.Init( materials->FindTexture( "_rt_Fullscreen", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pFullscreenTexture;
}

//=============================================================================
// Camera Texture
//=============================================================================
static CTextureReference s_pCameraTexture;
ITexture *GetCameraTexture( void )
{
	if ( !s_pCameraTexture )
	{
		s_pCameraTexture.Init( materials->FindTexture( "_rt_Camera", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pCameraTexture;
}

//=============================================================================
// Full Frame Depth Texture
//=============================================================================
static CTextureReference s_pFullFrameDepthTexture;
ITexture *GetFullFrameDepthTexture( void )
{
	if ( !s_pFullFrameDepthTexture )
	{
		s_pFullFrameDepthTexture.Init( materials->FindTexture( "_rt_FullFrameDepth", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pFullFrameDepthTexture;
}

//=============================================================================
// Full Frame Buffer Textures
//=============================================================================
static CTextureReference s_pFullFrameFrameBufferTexture[MAX_FB_TEXTURES];
ITexture *GetFullFrameFrameBufferTexture( int textureIndex )
{
	if ( !s_pFullFrameFrameBufferTexture[textureIndex] )
	{
		char name[256];
		if( textureIndex != 0 )
		{
			V_snprintf( name, std::size( name ), "_rt_FullFrameFB%d", textureIndex );
		}
		else
		{
			V_strcpy( name, "_rt_FullFrameFB" );
		}
		s_pFullFrameFrameBufferTexture[textureIndex].Init( materials->FindTexture( name, TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pFullFrameFrameBufferTexture[textureIndex];
}


//=============================================================================
// Water reflection
//=============================================================================
static CTextureReference s_pWaterReflectionTexture;
ITexture *GetWaterReflectionTexture( void )
{
	if ( !s_pWaterReflectionTexture )
	{
		s_pWaterReflectionTexture.Init( materials->FindTexture( "_rt_WaterReflection", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pWaterReflectionTexture;
}

//=============================================================================
// Water refraction
//=============================================================================
static CTextureReference s_pWaterRefractionTexture;
ITexture *GetWaterRefractionTexture( void )
{
	if ( !s_pWaterRefractionTexture )
	{
		s_pWaterRefractionTexture.Init( materials->FindTexture( "_rt_WaterRefraction", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pWaterRefractionTexture;
}

//=============================================================================
// Custom Weapon RT
//=============================================================================
static CTextureReference s_pCustomWeaponTexture;
ITexture *GetCustomWeaponTexture( void )
{ 
	if ( !s_pCustomWeaponTexture )
	{
		s_pCustomWeaponTexture.Init( materials->FindTexture( "_rt_CustomWeapon", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTexture;
}

//=============================================================================
// Custom Weapon RT for exponent
//=============================================================================
static CTextureReference s_pCustomWeaponTextureExp;
ITexture *GetCustomWeaponTextureExp( void )
{ 
	if ( !s_pCustomWeaponTextureExp )
	{
		s_pCustomWeaponTextureExp.Init( materials->FindTexture( "_rt_CustomWeaponExp", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTextureExp;
}

//=============================================================================
// Small Buffer HDR0
//=============================================================================
static CTextureReference s_pSmallBufferHDR0;
ITexture *GetSmallBufferHDR0( void )
{
	if ( !s_pSmallBufferHDR0 )
	{
		s_pSmallBufferHDR0.Init( materials->FindTexture( "_rt_SmallHDR0", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR0;
}

//=============================================================================
// Small Buffer HDR1
//=============================================================================
static CTextureReference s_pSmallBufferHDR1;
ITexture *GetSmallBufferHDR1( void )
{
	if ( !s_pSmallBufferHDR1 )
	{
		s_pSmallBufferHDR1.Init( materials->FindTexture( "_rt_SmallHDR1", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR1;
}

//=============================================================================
// Quarter Sized FB0
//=============================================================================
static CTextureReference s_pQuarterSizedFB0;
ITexture *GetSmallBuffer0( void )
{
	if ( !s_pQuarterSizedFB0 )
	{
		s_pQuarterSizedFB0.Init( materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB0;
}

//=============================================================================
// Quarter Sized FB1
//=============================================================================
static CTextureReference s_pQuarterSizedFB1;
ITexture *GetSmallBuffer1( void )
{
	if ( !s_pQuarterSizedFB1 )
	{
		s_pQuarterSizedFB1.Init( materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB1;
}

//=============================================================================
// Teeny Textures
//=============================================================================
static CTextureReference s_TeenyTextures[MAX_TEENY_TEXTURES];
ITexture *GetTeenyTexture( int which )
{

	contract_assert( which < MAX_TEENY_TEXTURES );

	if ( !s_TeenyTextures[which] )
	{
		char nbuf[20];
		V_snprintf( nbuf, std::size( nbuf ), "_rt_TeenyFB%d", which );
		s_TeenyTextures[which].Init( materials->FindTexture( nbuf, TEXTURE_GROUP_RENDER_TARGET ) );
		AddReleaseFunc();
	}
	return s_TeenyTextures[which];
}

void ReleaseRenderTargets( int nChangeFlags )
{
	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		return;

	s_pPowerOfTwoFrameBufferTexture.Shutdown();
	s_pCameraTexture.Shutdown();
	s_pWaterReflectionTexture.Shutdown();
	s_pWaterRefractionTexture.Shutdown();
	s_pQuarterSizedFB0.Shutdown();
	s_pQuarterSizedFB1.Shutdown();
	s_pFullFrameDepthTexture.Shutdown();
	s_pCustomWeaponTexture.Shutdown();

	for (int i=0; i<MAX_FB_TEXTURES; ++i)
	{
		s_pFullFrameFrameBufferTexture[i].Shutdown();
	}
}
