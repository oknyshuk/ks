#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.types;
import ks.platform;
import ks.simd;

#include "togl/rendermechanism.h"


#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif

struct IDirect3DTexture9;
struct IDirect3DBaseTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexBuffer9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DVolumeTexture9;

typedef struct _D3DLIGHT9				D3DLIGHT9;
typedef struct _D3DADAPTER_IDENTIFIER9	D3DADAPTER_IDENTIFIER9;
typedef struct _D3DCAPS9				D3DCAPS9;
typedef struct _D3DVIEWPORT9			D3DVIEWPORT9;
typedef struct _D3DMATERIAL9			D3DMATERIAL9;
typedef IDirect3DTexture9				IDirect3DTexture;
typedef IDirect3DBaseTexture9			IDirect3DBaseTexture;
typedef IDirect3DCubeTexture9			IDirect3DCubeTexture;
typedef IDirect3DVolumeTexture9 		IDirect3DVolumeTexture;
typedef IDirect3DDevice9				IDirect3DDevice;
typedef D3DMATERIAL9					D3DMATERIAL;
typedef D3DLIGHT9						D3DLIGHT;
typedef IDirect3DSurface9				IDirect3DSurface;
typedef D3DCAPS9						D3DCAPS;
typedef IDirect3DIndexBuffer9			IDirect3DIndexBuffer;
typedef IDirect3DVertexBuffer9			IDirect3DVertexBuffer;
typedef IDirect3DPixelShader9			IDirect3DPixelShader;
typedef IDirect3DDevice					*LPDIRECT3DDEVICE;
typedef IDirect3DIndexBuffer			*LPDIRECT3DINDEXBUFFER;
typedef IDirect3DVertexBuffer			*LPDIRECT3DVERTEXBUFFER;

class CDx9Types
{
public:
	typedef IDirect3DTexture9				IDirect3DTexture;
	typedef IDirect3DBaseTexture9			IDirect3DBaseTexture;
	typedef IDirect3DCubeTexture9			IDirect3DCubeTexture;
	typedef IDirect3DVolumeTexture9 		IDirect3DVolumeTexture;
	typedef IDirect3DDevice9				IDirect3DDevice;
	typedef D3DMATERIAL9					D3DMATERIAL;
	typedef D3DLIGHT9						D3DLIGHT;
	typedef IDirect3DSurface9				IDirect3DSurface;
	typedef D3DCAPS9						D3DCAPS;
	typedef IDirect3DIndexBuffer9			IDirect3DIndexBuffer;
	typedef IDirect3DVertexBuffer9			IDirect3DVertexBuffer;
	typedef IDirect3DPixelShader9			IDirect3DPixelShader;
	typedef IDirect3DDevice					*LPDIRECT3DDEVICE;
	typedef IDirect3DIndexBuffer			*LPDIRECT3DINDEXBUFFER;
	typedef IDirect3DVertexBuffer			*LPDIRECT3DVERTEXBUFFER;
};

typedef void *HardwareShader_t;

//-----------------------------------------------------------------------------
// The vertex and pixel shader type
//-----------------------------------------------------------------------------
typedef intp VertexShader_t;
typedef intp PixelShader_t;	

//-----------------------------------------------------------------------------
// Bitpattern for an invalid shader
//-----------------------------------------------------------------------------
inline constexpr auto INVALID_SHADER = ( 0xFFFFFFFF );
inline constexpr auto INVALID_HARDWARE_SHADER = ( nullptr );

inline constexpr auto D3DSAMP_NOTSUPPORTED = D3DSAMP_FORCE_DWORD;
inline constexpr auto D3DRS_NOTSUPPORTED = D3DRS_FORCE_DWORD;
