#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Define local cl_dll hooks to the renderable textures in material system




class ITexture;
ITexture *GetPowerOfTwoFrameBufferTexture( void );
ITexture *GetFullFrameFrameBufferTexture( int textureIndex );
ITexture *GetWaterReflectionTexture( void );
ITexture *GetWaterRefractionTexture( void );
ITexture *GetFullscreenTexture( void );
ITexture *GetCameraTexture( void );
ITexture *GetFullFrameDepthTexture( void );
ITexture *GetCustomWeaponTexture();
ITexture *GetCustomWeaponTextureExp();

// SmallBufferHDRx=r16g16b16a16 quarter-sized texture
ITexture *GetSmallBufferHDR0( void );
ITexture *GetSmallBufferHDR1( void );

ITexture *GetSmallBuffer0( void );							// quarter-sized texture, same fmt as screen
ITexture *GetSmallBuffer1( void );							// quarter-sized texture, same fmt as screen

inline constexpr auto MAX_TEENY_TEXTURES = 3;

ITexture *GetTeenyTexture(int which);						// tiny 32x32 texture, always 8888

void ReleaseRenderTargets( void );
