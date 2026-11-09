#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "materialsystem/imaterial.h"
import std;
#include "shaderapi/ishaderapi.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class ITexture;
struct MaterialSystem_Config_t;
struct ImageFormatInfo_t;
enum Sampler_t;
enum VertexTextureSampler_t;
enum StandardTextureId_t;
class CPrimList;
struct ShaderColorCorrectionInfo_t;

inline constexpr const char *SHADER_UTIL_INTERFACE_VERSION = "VShaderUtil001";

enum shaderthreadevent_t
{
	SHADER_THREAD_RELEASE_RESOURCES =	1,
	SHADER_THREAD_ACQUIRE_RESOURCES =	2,
	SHADER_THREAD_DEVICE_LOST =			3,
	SHADER_THREAD_EVICT_RESOURCES =		4,
	SHADER_THREAD_OTHER_APP_START =		5,
	SHADER_THREAD_OTHER_APP_END =		6,
	SHADER_THREAD_RESET_RENDER_STATE =	7,
};

class IShaderUtil
{
public:
	// Method to allow clients access to the MaterialSystem_Config
	MaterialSystem_Config_t& GetConfig();

	// Allows us to convert image formats
	bool ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat,
									 unsigned char *dst, enum ImageFormat dstImageFormat, 
									 int width, int height, int srcStride = 0, int dstStride = 0 );

	// Figures out the amount of memory needed by a bitmap
	int GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap );

	// Gets image format info
	const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt ) const;

    // Bind standard textures
	void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );

	// What are the lightmap dimensions?
	void GetLightmapDimensions( int *w, int *h );

	// These methods are called when the shader must eject + restore HW memory
	void ReleaseShaderObjects( int nChangeFlags = 0 );
	void RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags = 0 );

	bool InFlashlightMode() const;
	bool IsCascadedShadowMapping() const;

	// For the shader API to shove the current version of aniso level into the
	// "definitive" place (g_config) when the shader API decides to change it.
	// Eventually, we should have a better system of who owns the definitive
	// versions of config vars.
	void NoteAnisotropicLevel( int currentLevel );

	// NOTE: Stuff after this is added after shipping HL2.

	// Are we rendering through the editor?
	bool InEditorMode() const;

	ITexture *GetRenderTargetEx( int nRenderTargetID );

	// Tells the material system to draw a buffer clearing quad
	void DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth );



	// Calls from meshes to material system to handle queing/threading
	bool OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type );

	void SyncMatrices();
	void SyncMatrix( MaterialMatrixMode_t );

	void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	int MaxHWMorphBatchCount() const;

	// Interface for mat system to tell shaderapi about color correction
	void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );

	// Gets texture handles for ITextures
	ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel );

	float GetSubDHeight();

	bool OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );

	// received an event while not in owning thread, handle this outside
	void OnThreadEvent( uint32 threadEvent );

	MaterialThreadMode_t GetThreadMode();
	bool  				 IsRenderThreadSafe( );

	// Remove any materials from memory that aren't in use as determined
	// by the IMaterial's reference count.
	void UncacheUnusedMaterials( bool bRecomputeStateSnapshots = false );

	bool IsInFrame( ) const;
	
	// Returns the texture for a particular lightmap page
	ShaderAPITextureHandle_t GetLightmapTexture( ShaderAPITextureHandle_t nLightmapPage );
	bool IsRenderingPaint() const;
	ShaderAPITextureHandle_t GetPaintmapTexture( ShaderAPITextureHandle_t nLightmapPage );

	ShaderAPITextureHandle_t GetStandardTexture( StandardTextureId_t id );
};
