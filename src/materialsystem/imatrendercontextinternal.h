#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;
import ks.tier1;
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "shaderapi/shareddefs.h"
#include "shaderapi/ishaderdynamic.h"
#include "imaterialsysteminternal.h"

class IMaterialInternal;

// typedefs to allow use of delegation macros
typedef Vector4D LightCube_t[6];


class CMatRenderContextBase;

// The render context implementations; every context is created as exactly one of them.
// IMatRenderContext and IMatRenderContextInternal forward each call to the implementation named by
// the kind (cmatrendercontextdispatch.cpp).
enum class MatRenderContextKind : unsigned char
{
	Hardware,	// CMatRenderContext: draws through the shader API
	Queued,		// CMatQueuedRenderContext: records the calls for the hardware context
};

class IMatRenderContextInternal : public IMatRenderContext
{
public:
	MatRenderContextKind Kind() const { return m_Kind; }

	// Functions of the shared base class that the hardware context overrides; the base class
	// calls them, so they dispatch on the kind too.
	void UpdateHeightClipUserClipPlane( void );
	void ApplyCustomClipPlanes( void );
	void CommitRenderTargetAndViewport( void );
	void OnRenderDataUnreferenced();

	float GetFloatRenderingParameter(int parm_number) const;
	int GetIntRenderingParameter(int parm_number) const;
	ITexture *GetTextureRenderingParameter(int parm_number) const;
	Vector GetVectorRenderingParameter(int parm_number) const;

	void SwapBuffers();

	void SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial);
	IMaterialInternal* GetCurrentMaterialInternal() const;
	int GetLightmapPage();
	void ForceDepthFuncEquals( bool);

	bool InFlashlightMode() const;
	bool IsCascadedShadowMapping() const;
	void BindStandardTexture( Sampler_t, TextureBindFlags_t nBindFlags, StandardTextureId_t );
	void GetLightmapDimensions( int *, int *);
	MorphFormat_t GetBoundMorphFormat();
	ITexture *GetRenderTargetEx( int );
	void DrawClearBufferQuad( unsigned char, unsigned char, unsigned char, unsigned char, bool, bool, bool );

	bool OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );
	bool OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type );

	void SyncMatrices();
	void SyncMatrix( MaterialMatrixMode_t );

	void ForceHardwareSync();
	void BeginFrame();
	void EndFrame();

	void SetFrameTime( float frameTime );
	void SetCurrentProxy( void *pProxy );
	void MarkRenderDataUnused( bool bBeginFrame );
	CMatCallQueue *GetCallQueueInternal();

	void EvictManagedResources();

	ShaderAPITextureHandle_t GetLightmapTexture( int nLightmapPage );
	bool IsRenderingPaint() const;
	ShaderAPITextureHandle_t GetPaintmapTexture( int nLightmapPage );
	

protected:
	explicit IMatRenderContextInternal( MatRenderContextKind kind ) : m_Kind( kind ) {}

private:
	MatRenderContextKind m_Kind;
};
