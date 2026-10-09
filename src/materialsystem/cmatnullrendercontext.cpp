// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;


import ks.tier1;

#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "bitmap/tgawriter.h"
#include "bitmap/tgaloader.h"
#include "datacache/idatacache.h"
#include "filesystem.h"
#include "pixelwriter.h"

#include "materialsystem_global.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/IColorCorrection.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"

#include "cmatnullrendercontext.h"

#include "cmatrendercontext.h"
#include "itextureinternal.h"

class CMatNullRenderContext : public CMatRenderContextBase
{
public:
	CMatNullRenderContext()
		:	m_WidthBackBuffer( 0 ), 
			m_HeightBackBuffer( 0 )
	{
	}

	virtual void InitializeFrom( CMatRenderContextBase *pInitialState )
	{
		CMatRenderContextBase::InitializeFrom( pInitialState );
		g_pShaderAPI->GetBackBufferDimensions( m_WidthBackBuffer, m_HeightBackBuffer );
	}

	void BeginRender()
	{
	}

	void EndRender()
	{
	}

	void Flush(bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetRenderTargetDimensions(int &,int &) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DepthRange(float,float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearBuffers(bool,bool,bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixels(int,int,int,int,unsigned char *,ImageFormat, ITexture *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAsync(int,int,int,int,unsigned char *,ImageFormat, ITexture *, CThreadEvent *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAsyncGetResult(int,int,int,int,unsigned char *,ImageFormat,CThreadEvent *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetLightingState( const MaterialLightingState_t &state )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetLights( int nCount, const LightDesc_t *pDesc )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetLightingOrigin( Vector vLightingOrigin )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetAmbientLightCube(Vector4D [])
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyRenderTargetToTexture(ITexture *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFrameBufferCopyTexture(ITexture *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	ITexture *GetFrameBufferCopyTexture(int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void GetViewport( int& x, int& y, int& width, int& height ) const
	{
		// Verify valid top of RT stack
		contract_assert( !m_RenderTargetStack.empty() );

		// Grab the top of stack
		const RenderTargetStackElement_t& element = m_RenderTargetStack.back();

		// If either dimension is negative, set to full bounds of current target
		if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
		{
			// Viewport origin at target origin
			x = y = 0;

			// If target is back buffer
			if ( element.m_pRenderTargets[0] == nullptr )
			{
				width = m_WidthBackBuffer;
				height = m_HeightBackBuffer;
			}
			else // if target is texture
			{
				width = element.m_pRenderTargets[0]->GetActualWidth();
				height = element.m_pRenderTargets[0]->GetActualHeight();
			}
		}
		else // use the bounds from the stack directly
		{
			x = element.m_nViewX;
			y = element.m_nViewY;
			width = element.m_nViewW;
			height = element.m_nViewH;
		}
	}

	void BeginGeneratingCSMs()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndGeneratingCSMs()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CullMode(MaterialCullMode_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FlipCullMode( void )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogMode(MaterialFogMode_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogStart(float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogEnd(float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFogZ(float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	MaterialFogMode_t GetFogMode()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return MATERIAL_FOG_NONE;
	}

	int	GetCurrentNumBones( ) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void FogColor3f(float,float,float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3fv(const float *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3ub(unsigned char,unsigned char,unsigned char)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void FogColor3ubv(const unsigned char *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetFogColor(unsigned char *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetNumBoneWeights(int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMesh *CreateStaticMesh(VertexFormat_t,const char *,IMaterial *,VertexStreamSpec_t *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void DestroyStaticMesh(IMesh *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMesh *GetDynamicMesh(bool,IMesh *,IMesh *,IMaterial *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	virtual IMesh* GetDynamicMeshEx( VertexFormat_t, bool, IMesh*, IMesh*, IMaterial * )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	int SelectionMode(bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void SelectionBuffer(unsigned int *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearSelectionNames()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void LoadSelectionName(int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushSelectionName(int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopSelectionName()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearColor3ub(unsigned char,unsigned char,unsigned char)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearColor4ub(unsigned char,unsigned char,unsigned char,unsigned char)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideDepthEnable( bool, bool, bool )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideAlphaWriteEnable( bool, bool )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void OverrideColorWriteEnable( bool, bool )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawScreenSpaceQuad(IMaterial *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SyncToken(const char *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	OcclusionQueryObjectHandle_t CreateOcclusionQueryObject()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void DestroyOcclusionQueryObject(OcclusionQueryObjectHandle_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginOcclusionQueryDrawing(OcclusionQueryObjectHandle_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndOcclusionQueryDrawing(OcclusionQueryObjectHandle_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int OcclusionQuery_GetNumPixelsRendered(OcclusionQueryObjectHandle_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 1;
	}

	virtual void SetFlashlightMode(bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}
	virtual bool GetFlashlightMode( void ) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual bool IsCullingEnabledForSinglePassFlashlight( void ) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual void EnableCullingForSinglePassFlashlight( bool )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetRenderingPaint( bool bEnable )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFlashlightState(const FlashlightState_t &,const VMatrix &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual bool IsCascadedShadowMapping() const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return false;
	}

	virtual void SetCascadedShadowMapping( bool bEnable )
	{
		bEnable;
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
	{
		state, pDepthTextureAtlas;
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom  )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopScissorRect()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void PushDeformation( DeformationBase_t const *Deformation )
	{
	}

	virtual void PopDeformation( )
	{
	}

	virtual int GetNumActiveDeformations() const
	{
		return 0;
	}

	void EnableUserClipTransformOverride(bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void UserClipTransform(const VMatrix &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	IMorph *CreateMorph(MorphFormat_t, const char *pDebugName)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void DestroyMorph(IMorph *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindMorph(IMorph *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetMorphTargetFactors(int,float *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ReadPixelsAndStretch(Rect_t *,Rect_t *,unsigned char *,ImageFormat,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetWindowSize(int &,int &) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawScreenSpaceRectangle(IMaterial *,int,int,int,int,float,float,float,float,int,int,void*,int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void LoadBoneMatrix(int,const matrix3x4_t &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindLightmapTexture(ITexture *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyRenderTargetToTextureEx(ITexture *,int,Rect_t *,Rect_t *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void CopyTextureToRenderTargetEx(int,ITexture *,Rect_t *,Rect_t *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetFloatRenderingParameter(int,float)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetIntRenderingParameter(int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetTextureRenderingParameter( int, ITexture * )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetVectorRenderingParameter(int,const Vector &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void SetStencilState( const ShaderStencilState_t & )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearStencilBufferRectangle(int,int,int,int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PushCustomClipPlane(const float *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PopCustomClipPlane()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void GetMaxToRender(IMesh *,bool,int *,int *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int GetMaxVerticesToRender(IMaterial *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	int GetMaxIndicesToRender()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void DisableAllLocalLights()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	int CompareMaterialCombos(IMaterial *,IMaterial *,int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	IMesh *GetFlexMesh()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void SetFlashlightStateEx(const FlashlightState_t &,const VMatrix &,ITexture *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	ITexture *GetLocalCubemap()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return nullptr;
	}

	void ClearBuffersObeyStencil(bool,bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void PerformFullScreenStencilOperation( void )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	bool GetUserClipTransform(VMatrix &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void GetFogDistances(float *,float *,float *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginPIXEvent(unsigned long,const char *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndPIXEvent()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetPIXMarker(unsigned long,const char *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginBatch(IMesh *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BindBatch(IMesh *,IMaterial *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void DrawBatch(MaterialPrimitiveType_t, int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndBatch()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetToneMappingScaleLinear(const Vector &)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	float GetFloatRenderingParameter(int) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	int GetIntRenderingParameter(int) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	ITexture *GetTextureRenderingParameter(int) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	Vector GetVectorRenderingParameter(int) const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return Vector(0,0,0);
	}

	void SwapBuffers()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void ForceDepthFuncEquals(bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	bool InFlashlightMode() const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool IsRenderingPaint() const
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void GetLightmapDimensions(int *,int *)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	MorphFormat_t GetBoundMorphFormat()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return 0;
	}

	void DrawClearBufferQuad(unsigned char,unsigned char,unsigned char,unsigned char,bool,bool,bool)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}


	bool OnDrawMesh(IMesh *,CPrimList *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnDrawMesh(IMesh *,int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnDrawMeshModulated(IMesh *, const Vector4D&, int,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetFlexMesh(IMesh *,IMesh *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetColorMesh(IMesh *,IMesh *,int)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	bool OnSetPrimitiveType(IMesh *,MaterialPrimitiveType_t)
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
		return true;
	}

	void ForceHardwareSync()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void BeginFrame()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EndFrame()
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void SetShadowDepthBiasFactors( float fSlopeScaleDepthBias, float fDepthBias )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}
	
	void BindStandardTexture( Sampler_t, TextureBindFlags_t nBindFlags, StandardTextureId_t )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	void EvictManagedResources()
	{
		contract_assert( false && "EvictManagedResources only provides base features, not a stub (right now)" );
	}

// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
	{
		contract_assert( 0 );
		return nullptr;
	}
	IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
	{
		contract_assert( 0 );
		return nullptr;
	}
	void DestroyVertexBuffer( IVertexBuffer * )
	{
		contract_assert( 0 );
	}
	void DestroyIndexBuffer( IIndexBuffer * )
	{
		contract_assert( 0 );
	}
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBufferedtrue )
	{
		contract_assert( 0 );
		return nullptr;
	}
	IIndexBuffer *GetDynamicIndexBuffer( )
	{
		contract_assert( 0 );
		return nullptr;
	}
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions1 )
	{
		contract_assert( 0 );
	}
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
	{
		contract_assert( 0 );
	}
	void Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices )
	{
		contract_assert( 0 );
	}
	virtual void BeginMorphAccumulation()
	{
		contract_assert( 0 );
	}
	virtual void EndMorphAccumulation()
	{
		contract_assert( 0 );
	}
	virtual void AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
	{
		contract_assert( 0 );
	}
	virtual bool GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
	{
		contract_assert( 0 );
		pTexCoord->Init();
		return false;
	}

	virtual int GetSubDBufferWidth()
	{
		contract_assert( 0 );
		return 0;
	}
	virtual float* LockSubDBuffer( int nNumRows )
	{
		contract_assert( 0 );
		return nullptr;
	}
	virtual void UnlockSubDBuffer()
	{
		contract_assert( 0 );
	}

	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights ) {}

	virtual void FogMaxDensity( float flMaxDensity )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	virtual void EnableColorCorrection( bool bEnable ) {}
	virtual ColorCorrectionHandle_t AddLookup( const char *pName ) { return 0; }
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle ) { return true; }
	virtual void LockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName ) {}
	virtual void UnlockLookup( ColorCorrectionHandle_t handle ) {}
	virtual void SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight ) {}
	virtual void ResetLookupWeights( ) {}
	virtual void SetResetable( ColorCorrectionHandle_t handle, bool bResetable ) {}
	virtual void SetFullScreenDepthTextureValidityFlag( bool bIsValid ) {}

	virtual void SetNonInteractiveLogoTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedW, float flNormalizedH ) {}
	virtual void SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize ) {}
	virtual void SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode ) {}
	virtual void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode ) {}
	virtual void RefreshFrontBufferNonInteractive() {}

	virtual void FlipCulling( bool bFlipCulling ) {}




	virtual void AntiAliasingHint(int )
	{
		contract_assert( 0 );
	}
	
	virtual void EnableSinglePassFlashlightMode( bool bEnable )
	{
		contract_assert( 0 );
	}

	virtual bool SinglePassFlashlightModeEnabled( void ) const
	{
		contract_assert( 0 );
		return false;
	}

	void UpdateGameTime( float )
	{
		contract_assert( false && "CMatNullRenderContext only provides base features, not a stub (right now)" );
	}

	// RocketUI stubs - null context doesn't render
	virtual void RenderRocketHUD(void *) {}
	virtual void RenderRocketMenu(void *) {}

	//--------------------------------------------------------
	// debug logging - no-op in queued context
	//--------------------------------------------------------
	virtual void							Printf( char *fmt, ... ) {};
	virtual void							PrintfVA( char *fmt, std::va_list vargs ){};
	virtual float							Knob( char *knobname, float *setvalue=nullptr ) { return 0.0f; };	


	virtual ColorCorrectionHandle_t FindLookup( const char *pName ) { return 0; }

	int m_WidthBackBuffer, m_HeightBackBuffer;
};


CMatRenderContextBase *CreateNullRenderContext()
{
	return new CMatNullRenderContext;
}
