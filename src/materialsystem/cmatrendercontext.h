#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import ks.tier1;
import std;
#include "materialsystem_global.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/ishaderapi.h"
#include "imaterialinternal.h"
#include "shadersystem.h"
#include "imorphinternal.h"
#include "isubdinternal.h"
#include "imatrendercontextinternal.h"
#include "occlusionquerymgr.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "rocketui/rocketui.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;
class CMaterialSystem;
class CMatLightmaps;
class CMatPaintmaps;
typedef intp ShaderAPITextureHandle_t;
class IMorphMgrRenderContext;
class CMatCallQueue;


//-----------------------------------------------------------------------------
// Render targets
//-----------------------------------------------------------------------------
inline constexpr auto MAX_RENDER_TARGETS = 4;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CMatRenderContextBase : public IMatRenderContextInternal
{
	friend class IMatRenderContext;
	friend class IMatRenderContextInternal;
public:
	int										AddRef()				{ return CRefMT::Increment( &m_iRefs ); }
	int										Release();

	void							InitializeFrom( CMatRenderContextBase *pInitialState );

	InitReturnVal_t							Init();
	void									Shutdown();
	void									CompactMemory();

	void									SetFrameTime( float frameTime ) { m_FrameTime = frameTime; }

	ICallQueue *							GetCallQueue() { return nullptr; }
	CMatCallQueue *							GetCallQueueInternal() { return nullptr; }

	ITexture *								GetRenderTarget( void );
	ITexture *								GetRenderTargetEx( int nRenderTargetID );

	IMaterialInternal*						GetCurrentMaterialInternal() const								{ return m_pCurrentMaterial;	}	
	void							SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial)	{
																												m_pCurrentMaterial = pCurrentMaterial;
																												contract_assert( (m_pCurrentMaterial == nullptr) || ((IMaterialInternal *)m_pCurrentMaterial)->IsRealTimeVersion() );
																											}
	IMaterial *								GetCurrentMaterial()											{ return GetCurrentMaterialInternal(); }
	void *							GetCurrentProxy()												{ return m_pCurrentProxyData; }
	void							SetCurrentProxy( void *pProxyData )								{ m_pCurrentProxyData = pProxyData; }

	void									Bind( IMaterial *material, void *proxyData = nullptr );
	void									BindLightmapPage( int lightmapPageID );
	void									BindLocalCubemap( ITexture *pTexture );

	// matrix api
	void									MatrixMode( MaterialMatrixMode_t);
	void									PushMatrix();
	void									PopMatrix();
	void									LoadMatrix( const VMatrix& matrix );
	void									LoadMatrix( const matrix3x4_t& matrix );
	void									MultMatrix( const VMatrix& matrix );
	void									MultMatrixLocal( const VMatrix& matrix );
	void									MultMatrix( const matrix3x4_t& matrix );
	void									MultMatrixLocal( const matrix3x4_t& matrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix );
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	// Set the current texture that is a copy of the framebuffer.
	void									SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex );
	ITexture *								GetFrameBufferCopyTexture( int textureIndex );

	void									SetHeightClipMode( MaterialHeightClipMode_t mode );
	MaterialHeightClipMode_t				GetHeightClipMode( void );
	void									SetHeightClipZ( float z );

	bool									EnableClipping( bool bEnable );

	void									SetRenderTarget( ITexture *pTexture );
	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );

	void									Viewport( int x, int y, int width, int height );
	void									PushRenderTargetAndViewport( );
	void									PushRenderTargetAndViewport( ITexture *pTexture );
	void									PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH );
	void									PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH );
	void									PopRenderTargetAndViewport( void );

	void									SyncMatrices();
	void									SyncMatrix( MaterialMatrixMode_t );
	const VMatrix &							AccessCurrentMatrix() const { return m_pCurMatrixItem->matrix; }
	ShaderAPITextureHandle_t				GetLightmapTexture( int nLightmapPage );
	ShaderAPITextureHandle_t				GetPaintmapTexture( int nLightmapPage );

	void							UpdateHeightClipUserClipPlane( void ) {}
	void							ApplyCustomClipPlanes( void ) {}

	float									ComputePixelDiameterOfSphere( const Vector& origin, float flRadius );
	float									ComputePixelWidthOfSphere( const Vector& origin, float flRadius ); // FIXME: REMOVE THIS FUNCTION!

	void							GetWorldSpaceCameraPosition( Vector *pCameraPos );
	void							GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp );

	Vector									GetToneMappingScaleLinear();

	// Inherited from IMaterialSystemInternal
	int										GetLightmapPage( void );

	void *							LockRenderData( int nSizeInBytes );
	void							UnlockRenderData( void *pData );
	void							AddRefRenderData();
	void							ReleaseRenderData();
	bool							IsRenderData( const void *pData ) const;
	void									MarkRenderDataUnused( bool bFrameEnd );
	int										RenderDataSizeUsed() const;
	
	// debugging
	void							PrintfVA( char *fmt, std::va_list vargs );
	void							Printf( char *fmt, ... );
	float							Knob( char *knobname, float *setvalue = nullptr );
	
protected:
	enum MatrixStackFlags_t
	{
		MSF_DIRTY		= ( 1 << 0 ),
		MSF_IDENTITY	= ( 1 << 1 ),
	};

	struct MatrixStackItem_t
	{
		VMatrix matrix;
		unsigned flags;
	};

	struct RenderTargetStackElement_t
	{
		// The render target
		ITexture *m_pRenderTargets[MAX_RENDER_TARGETS];

		// Optional depth texture (used for shadow mapping)
		ITexture *m_pDepthTexture;

		// Viewport dimensions
		int m_nViewX;
		int m_nViewY;
		int m_nViewW;
		int m_nViewH;
	};

	struct ScissorRectStackElement_t
	{
		int nLeft;
		int nTop;
		int nRight;
		int nBottom;
	};

	struct PlaneStackElement
	{
		float fValues[4];
		bool bHack_IsHeightClipPlane; //used to hack in compatibility between the user clip planes and the existing height clip plane code
		//I'm doing the hack this way to retain modder's flexibility to mess with clip plane ordering so that they can make special hacks on their side if necessary
		PlaneStackElement( void ) : bHack_IsHeightClipPlane( false ) { };
	};

protected:
	explicit CMatRenderContextBase( MatRenderContextKind kind );
	void CommitRenderTargetAndViewport( void ) {}
	void RecomputeViewState();
	void RecomputeViewProjState();
	void CurrentMatrixChanged();
	void OnRenderDataUnreferenced() {}

private:
	int									m_iRefs;

protected:
	IMaterialInternal *					m_pCurrentMaterial;
	void*								m_pCurrentProxyData;

	// The lightmap page
	int									m_lightmapPageID;
	ITextureInternal *					m_pUserDefinedLightmap;

	ITexture *							m_pLocalCubemapTexture;
	
	ITexture *							m_pCurrentFrameBufferCopyTexture[MAX_FB_TEXTURES];

	MaterialHeightClipMode_t			m_HeightClipMode;
	float								m_HeightClipZ;
	// The currently bound morph target
	IMorphInternal *					m_pBoundMorph;
	IMorphMgrRenderContext				*m_pMorphRenderContext;

	// Intially a stack of 32 elements allocated, growing by 16 on overflows
	std::vector< RenderTargetStackElement_t >	m_RenderTargetStack;

	// Intially a stack of 32 elements allocated, growing by 16 on overflows
	std::vector< ScissorRectStackElement_t >	m_ScissorRectStack;

	MaterialMatrixMode_t				m_MatrixMode;
	MatrixStackItem_t *					m_pCurMatrixItem;
	std::vector<MatrixStackItem_t>		m_MatrixStacks[NUM_MATRIX_MODES];

	// View state
	VMatrix								m_viewProjMatrix;
	Vector								m_vecViewOrigin;
	Vector								m_vecViewForward;
	Vector								m_vecViewUp;
	Vector								m_vecViewRight;

	float								m_FrameTime;

	Vector								m_LastSetToneMapScale;							   	
	ShaderViewport_t					m_Viewport;
	CMaterialSystem *					m_pMaterialSystem;

	static CMemoryStack					sm_RenderData[2];
	static int							sm_nRenderLockCount;
	static int							sm_nRenderStack;
	static MemoryStackMark_t			sm_nRenderCurrentAllocPoint;
	static int							sm_nInitializeCount;

	bool								m_bFlashlightEnable : 1;
	bool								m_bDirtyViewState : 1;
	bool								m_bDirtyViewProjState : 1;
	bool								m_bEnableClipping : 1;
	bool								m_bFullFrameDepthIsValid : 1;
	bool								m_bRenderingPaint : 1;
	bool								m_bCullingEnabledForSinglePassFlashlight : 1;
	bool								m_bSinglePassFlashlightMode : 1;
	bool								m_bCascadedShadowMappingEnabled : 1;
	

};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CMatRenderContext : public CMatRenderContextBase
{
	friend class IMatRenderContext;
	friend class IMatRenderContextInternal;
	typedef CMatRenderContextBase BaseClass;

public:
	CMatRenderContext();

	InitReturnVal_t							Init( CMaterialSystem *pMaterialSystem );
	void									Shutdown();

	void									BeginRender();
	void									EndRender();

	void									Flush( bool flushHardware = false );

	void									SwapBuffers();

	void									OnReleaseShaderObjects();

	void EvictManagedResources()	{ (g_pShaderAPI)->EvictManagedResources(); };

	// Set the current texture that is a copy of the framebuffer.
	void									SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex );

	IMesh *									CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void DestroyStaticMesh( IMesh * a1 )	{ (g_pShaderDevice)->DestroyStaticMesh( a1 ); };
	IMesh *									GetDynamicMesh( bool buffered, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 );
	IMesh*							GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 );
	IMesh * GetFlexMesh()	{ return (g_pShaderAPI)->GetFlexMesh(); };

// ------------ New Vertex/Index Buffer interface ----------------------------
	IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
	{
		return g_pShaderDevice->CreateVertexBuffer( SHADER_BUFFER_TYPE_STATIC, fmt, nVertexCount, pBudgetGroup );
	}

	IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
	{
		return g_pShaderDevice->CreateIndexBuffer( SHADER_BUFFER_TYPE_STATIC, fmt, nIndexCount, pBudgetGroup );
	}

	void DestroyVertexBuffer( IVertexBuffer * a1 )	{ (g_pShaderDevice)->DestroyVertexBuffer( a1 ); };
	void DestroyIndexBuffer( IIndexBuffer * a1 )	{ (g_pShaderDevice)->DestroyIndexBuffer( a1 ); };
	IVertexBuffer * GetDynamicVertexBuffer( int a1, VertexFormat_t a2, bool a3 )	{ return (g_pShaderDevice)->GetDynamicVertexBuffer( a1, a2, a3 ); };
	IIndexBuffer * GetDynamicIndexBuffer()	{ return (g_pShaderDevice)->GetDynamicIndexBuffer(); };
	void BindVertexBuffer( int a1, IVertexBuffer * a2, int a3, int a4, int a5, VertexFormat_t a6, int a7 )	{ (g_pShaderAPI)->BindVertexBuffer( a1, a2, a3, a4, a5, a6, a7 ); };
	void BindIndexBuffer( IIndexBuffer * a1, int a2 )	{ (g_pShaderAPI)->BindIndexBuffer( a1, a2 ); };
	void Draw( MaterialPrimitiveType_t a1, int a2, int a3 )	{ (g_pShaderAPI)->Draw( a1, a2, a3 ); };
// ------------ End ----------------------------

	void									BindLocalCubemap( ITexture *pTexture );
	ITexture *								GetLocalCubemap( void );
	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );
	void									GetRenderTargetDimensions( int &width, int &height) const;

	// matrix api
	void									MatrixMode( MaterialMatrixMode_t);
	void									PushMatrix();
	void									PopMatrix();
	void									LoadMatrix( const VMatrix& matrix );
	void									LoadMatrix( const matrix3x4_t& matrix );
	void									MultMatrix( const VMatrix& matrix );
	void									MultMatrixLocal( const VMatrix& matrix );
	void									MultMatrix( const matrix3x4_t& matrix );
	void									MultMatrixLocal( const matrix3x4_t& matrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix );
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	void									SyncMatrices();
	void									SyncMatrix( MaterialMatrixMode_t );
	bool									TestMatrixSync( MaterialMatrixMode_t );
	void									ForceSyncMatrix( MaterialMatrixMode_t );
	ShaderAPITextureHandle_t				GetLightmapTexture( int nLightmapPage );
	ShaderAPITextureHandle_t				GetPaintmapTexture( int nLightmapPage );

	// Allows us to override the depth buffer setting of a material
	void OverrideDepthEnable( bool a1, bool a2, bool a3 )	{ (g_pShaderAPI)->OverrideDepthEnable( a1, a2, a3 ); };
	void OverrideAlphaWriteEnable( bool a1, bool a2 )	{ (g_pShaderAPI)->OverrideAlphaWriteEnable( a1, a2 ); };
	void OverrideColorWriteEnable( bool a1, bool a2 )	{ (g_pShaderAPI)->OverrideColorWriteEnable( a1, a2 ); };

	// FIXME: This is a hack required for NVidia/XBox, can they fix in drivers?
	void									DrawScreenSpaceQuad( IMaterial* pMaterial );

	int										CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightMapID1, int lightMapID2 );

	void									Bind( IMaterial *material, void *proxyData = nullptr );
	void									BindLightmapPage( int lightmapPageID );
	void									BindPaintTexture( ITexture *pTexture );

	void SetLights( int a1, const LightDesc_t * a2 )	{ (g_pShaderAPI)->SetLights( a1, a2 ); };
	void SetLightingOrigin( Vector a1 )	{ (g_pShaderAPI)->SetLightingOrigin( a1 ); };

	void SetAmbientLightCube( LightCube_t a1 )	{ (g_pShaderAPI)->SetAmbientLightCube( a1 ); };
	void DisableAllLocalLights()	{ (g_pShaderAPI)->DisableAllLocalLights(); };
	void SetLightingState( const MaterialLightingState_t& a1 )	{ (g_pShaderAPI)->SetLightingState( a1 ); };

	void									CopyRenderTargetToTexture( ITexture *pTexture );

	void									ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil );
	void ClearColor3ub( unsigned char a1, unsigned char a2, unsigned char a3 )	{ (g_pShaderAPI)->ClearColor3ub( a1, a2, a3 ); };
	void ClearColor4ub( unsigned char a1, unsigned char a2, unsigned char a3, unsigned char a4 )	{ (g_pShaderAPI)->ClearColor4ub( a1, a2, a3, a4 ); };
	void ClearBuffersObeyStencil( bool a1, bool a2 )	{ (g_pShaderAPI)->ClearBuffersObeyStencil( a1, a2 ); };
	void ClearBuffersObeyStencilEx( bool a1, bool a2, bool a3 )	{ (g_pShaderAPI)->ClearBuffersObeyStencilEx( a1, a2, a3 ); };
	void PerformFullScreenStencilOperation()	{ (g_pShaderAPI)->PerformFullScreenStencilOperation(); };

	// read to a unsigned char rgb image.
	void ReadPixels( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6 )	{ (g_pShaderAPI)->ReadPixels( a1, a2, a3, a4, a5, a6 ); };
	void ReadPixels( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6, ITexture * a7 )	{ (g_pShaderAPI)->ReadPixels( a1, a2, a3, a4, a5, a6, a7 ); };
	void ReadPixelsAsync( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6 )	{ (g_pShaderAPI)->ReadPixelsAsync( a1, a2, a3, a4, a5, a6 ); };
	void ReadPixelsAsync( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6, ITexture * a7 )	{ (g_pShaderAPI)->ReadPixelsAsync( a1, a2, a3, a4, a5, a6, a7 ); };
	void ReadPixelsAsync( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6, ITexture * a7, CThreadEvent * a8 )	{ (g_pShaderAPI)->ReadPixelsAsync( a1, a2, a3, a4, a5, a6, a7, a8 ); };
	void ReadPixelsAsyncGetResult( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6 )	{ (g_pShaderAPI)->ReadPixelsAsyncGetResult( a1, a2, a3, a4, a5, a6 ); };
	void ReadPixelsAsyncGetResult( int a1, int a2, int a3, int a4, unsigned char * a5, ImageFormat a6, CThreadEvent * a7 )	{ (g_pShaderAPI)->ReadPixelsAsyncGetResult( a1, a2, a3, a4, a5, a6, a7 ); };
	void									ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride ) { g_pShaderAPI->ReadPixels( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride ); }

	// Gets/sets viewport
	void									Viewport( int x, int y, int width, int height );
	void									GetViewport( int& x, int& y, int& width, int& height ) const;
	void							DepthRange( float zNear, float zFar );

	// Selection mode methods
	int SelectionMode( bool a1 )	{ return (g_pShaderAPI)->SelectionMode( a1 ); };
	void SelectionBuffer( unsigned int * a1, int a2 )	{ (g_pShaderAPI)->SelectionBuffer( a1, a2 ); };
	void ClearSelectionNames()	{ (g_pShaderAPI)->ClearSelectionNames(); };
	void LoadSelectionName( int a1 )	{ (g_pShaderAPI)->LoadSelectionName( a1 ); };
	void PushSelectionName( int a1 )	{ (g_pShaderAPI)->PushSelectionName( a1 ); };
	void PopSelectionName()	{ (g_pShaderAPI)->PopSelectionName(); };

	// Sets the cull mode
	void CullMode( MaterialCullMode_t a1 )	{ (g_pShaderAPI)->CullMode( a1 ); };
	void FlipCullMode()	{ (g_pShaderAPI)->FlipCullMode(); };

	void BeginGeneratingCSMs()	{ (g_pShaderAPI)->BeginGeneratingCSMs(); };
	void EndGeneratingCSMs()	{ (g_pShaderAPI)->EndGeneratingCSMs(); };
	void PerpareForCascadeDraw( int a1, float a2, float a3 )	{ (g_pShaderAPI)->PerpareForCascadeDraw( a1, a2, a3 ); };

	// Sets the number of bones for skinning
	void SetNumBoneWeights( int a1 )	{ (g_pShaderAPI)->SetNumBoneWeights( a1 ); };

	void									LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix );
	void SetFlexWeights( int a1, int a2, const MorphWeight_t* a3 )	{ (g_pShaderAPI)->SetFlexWeights( a1, a2, a3 ); };

	// Fog-related methods
	void									FogMode( MaterialFogMode_t fogMode )								{ g_pShaderAPI->SceneFogMode( fogMode ); }
	void FogStart( float a1 )	{ (g_pShaderAPI)->FogStart( a1 ); };
	void FogEnd( float a1 )	{ (g_pShaderAPI)->FogEnd( a1 ); };
	void SetFogZ( float a1 )	{ (g_pShaderAPI)->SetFogZ( a1 ); };
	void									FogColor3f( float r, float g, float b );
	void									FogColor3fv( const float* rgb );
	void									FogColor3ub( unsigned char r, unsigned char g, unsigned char b )	{	g_pShaderAPI->SceneFogColor3ub( r, g, b ); }
	void									FogColor3ubv( unsigned char const* rgb );
	void									FogMaxDensity( float flMaxDensity );

	MaterialFogMode_t						GetFogMode( void )													{ return g_pShaderAPI->GetSceneFogMode(); }
	void									GetFogColor( unsigned char *rgb )									{ g_pShaderAPI->GetSceneFogColor( rgb ); }
	void GetFogDistances( float * a1, float * a2, float * a3 )	{ (g_pShaderAPI)->GetFogDistances( a1, a2, a3 ); };
	int										GetCurrentNumBones( ) const											{ return g_pShaderAPI->GetCurrentNumBones(); }	

	// Bind standard textures
	void									BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );
	ShaderAPITextureHandle_t				GetStandardTexture( StandardTextureId_t id );

	void							BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	void							GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	float							GetSubDHeight();

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a ViewProj transform for user clip planes
	void EnableUserClipTransformOverride( bool a1 )	{ (g_pShaderAPI)->EnableUserClipTransformOverride( a1 ); };
	void UserClipTransform( const VMatrix & a1 )	{ (g_pShaderAPI)->UserClipTransform( a1 ); };

	// Gets the window size
	void GetWindowSize( int & a1, int & a2 )	const { (g_pShaderDevice)->GetWindowSize( a1, a2 ); };

	// Methods related to occlusion query
	OcclusionQueryObjectHandle_t			CreateOcclusionQueryObject();
	void DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t a1 )	{ (g_pOcclusionQueryMgr)->DestroyOcclusionQueryObject( a1 ); };
	void BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t a1 )	{ (g_pOcclusionQueryMgr)->BeginOcclusionQueryDrawing( a1 ); };
	void EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t a1 )	{ (g_pOcclusionQueryMgr)->EndOcclusionQueryDrawing( a1 ); };
	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t a1 )	{ (g_pOcclusionQueryMgr)->ResetOcclusionQueryObject( a1 ); };
	int										OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h );

	bool									InFlashlightMode() const;
	bool									IsRenderingPaint() const;
	void							SetFlashlightMode( bool bEnable );
	void							SetRenderingPaint( bool bEnable );
	bool									GetFlashlightMode( ) const;

	bool							IsCascadedShadowMapping() const;
	void							SetCascadedShadowMapping( bool bEnable );
	void							SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas );
	
	bool							IsCullingEnabledForSinglePassFlashlight() const;
	void							EnableCullingForSinglePassFlashlight( bool bEnable );	

	void									SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
	void									SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );

	void									PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom );
	void									PopScissorRect();
	

	// Creates/destroys morph data associated w/ a particular material
	IMorph *								CreateMorph( MorphFormat_t format, const char *pDebugName );
	void									DestroyMorph( IMorph *pMorph );
	void									BindMorph( IMorph *pMorph );
	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	MorphFormat_t							GetBoundMorphFormat();

	void									DrawScreenSpaceRectangle( IMaterial *pMaterial,
																		int destx, int desty,
																		int width, int height,
																		float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
																															// destx/y
																		float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
																															// destx+width-1, desty+height-1
																		int src_texture_width, int src_texture_height,		// needed for fixup
																		void *pClientRenderable = nullptr,
																		int nXDice = 1,
																		int nYDice = 1 );

	// custom clip planes, beware that only the most recently pushed plane will actually be used in some hardware configurations
	void									PushCustomClipPlane( const float *fPlane );
	void									PopCustomClipPlane( void );
	void									ApplyCustomClipPlanes( void ); //updates the clip planes based on how many are supported by the hardware using the top of the stack first, at the end of the stack, the height clip plane will be evaluated
	
	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool a1 )	{ (g_pShaderAPI)->ForceDepthFuncEquals( a1 ); };

	// Two methods of building lightmap bits from floating point src data

	// What are the lightmap dimensions?
	void									GetLightmapDimensions( int *w, int *h );

	void									DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth );

	void									UpdateHeightClipUserClipPlane( void );

	// Private routine called by render target stack push and pop routines to commit new top of stack to the device
	void									CommitRenderTargetAndViewport( void );

	void									SyncToken( const char *pToken );

	void									BindLightmapTexture( ITexture *pLightmapTexture );
	void									CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect = nullptr );
	void									CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect = nullptr );
	
	void SetFloatRenderingParameter( int a1, float a2 )	{ (g_pShaderAPI)->SetFloatRenderingParameter( a1, a2 ); };
	void SetIntRenderingParameter( int a1, int a2 )	{ (g_pShaderAPI)->SetIntRenderingParameter( a1, a2 ); };
	void SetTextureRenderingParameter( int a1, ITexture * a2 )	{ (g_pShaderAPI)->SetTextureRenderingParameter( a1, a2 ); };
	void SetVectorRenderingParameter( int a1, const Vector & a2 )	{ (g_pShaderAPI)->SetVectorRenderingParameter( a1, a2 ); };
	float GetFloatRenderingParameter( int a1 )	const { return (g_pShaderAPI)->GetFloatRenderingParameter( a1 ); };
	int GetIntRenderingParameter( int a1 )	const { return (g_pShaderAPI)->GetIntRenderingParameter( a1 ); };
	ITexture * GetTextureRenderingParameter( int a1 )	const { return (g_pShaderAPI)->GetTextureRenderingParameter( a1 ); };
	Vector GetVectorRenderingParameter( int a1 )	const { return (g_pShaderAPI)->GetVectorRenderingParameter( a1 ); };

	void GetMaxToRender( IMesh * a1, bool a2, int * a3, int * a4 )	{ (g_pShaderAPI)->GetMaxToRender( a1, a2, a3, a4 ); };
	int GetMaxVerticesToRender( IMaterial * a1 )	{ return (g_pShaderAPI)->GetMaxVerticesToRender( a1 ); };
	int GetMaxIndicesToRender()	{ return (g_pShaderAPI)->GetMaxIndicesToRender(); };
	void SetStencilState( const ShaderStencilState_t & a1 )	{ (g_pShaderAPI)->SetStencilState( a1 ); };
	void ClearStencilBufferRectangle( int a1, int a2, int a3, int a4, int a5 )	{ (g_pShaderAPI)->ClearStencilBufferRectangle( a1, a2, a3, a4, a5 ); };

	// PIX support
	void BeginPIXEvent( unsigned long a1, const char * a2 )	{ (g_pShaderAPI)->BeginPIXEvent( a1, a2 ); };
	void EndPIXEvent()	{ (g_pShaderAPI)->EndPIXEvent(); };
	void SetPIXMarker( unsigned long a1, const char * a2 )	{ (g_pShaderAPI)->SetPIXMarker( a1, a2 ); };

	void SetShadowDepthBiasFactors( float a1, float a2 )	{ (g_pShaderAPI)->SetShadowDepthBiasFactors( a1, a2 ); };

	void									BeginBatch( IMesh* pIndices );
	void									BindBatch( IMesh* pVertices, IMaterial *pAutoBind = nullptr );
	void									DrawBatch( MaterialPrimitiveType_t primType, int firstIndex, int numIndices );
	void									EndBatch();

	void									SetToneMappingScaleLinear( const Vector &scale );

	bool									OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool									OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool									OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );
	bool									OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return true; }
	bool									OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return true; }
	bool									OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type ) { return true; }

	CMaterialSystem							*GetMaterialSystem() const;

	void ForceHardwareSync()	{ (g_pShaderAPI)->ForceHardwareSync(); };
	void BeginFrame()	{ (g_pShaderAPI)->BeginFrame(); };
	void EndFrame()	{ (g_pShaderAPI)->EndFrame(); };

	void							BeginMorphAccumulation();
	void							EndMorphAccumulation();
	void							AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights );
	bool							GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex );

	// Subdivision surface interface
	int								GetSubDBufferWidth();
	float							*LockSubDBuffer( int nNumRows );
	void							UnlockSubDBuffer();

	void PushDeformation( const DeformationBase_t * a1 )	{ (g_pShaderAPI)->PushDeformation( a1 ); };
	void PopDeformation()	{ (g_pShaderAPI)->PopDeformation(); };
	int GetNumActiveDeformations()	const { return (g_pShaderAPI)->GetNumActiveDeformations(); };

	// Color correction related methods..
	void EnableColorCorrection( bool a1 )	{ (g_pColorCorrectionSystem)->EnableColorCorrection( a1 ); };
	ColorCorrectionHandle_t AddLookup( const char * a1 )	{ return (g_pColorCorrectionSystem)->AddLookup( a1 ); };
	bool RemoveLookup( ColorCorrectionHandle_t a1 )	{ return (g_pColorCorrectionSystem)->RemoveLookup( a1 ); };
	void LockLookup( ColorCorrectionHandle_t a1 )	{ (g_pColorCorrectionSystem)->LockLookup( a1 ); };
	void LoadLookup( ColorCorrectionHandle_t a1, const char * a2 )	{ (g_pColorCorrectionSystem)->LoadLookup( a1, a2 ); };
	void UnlockLookup( ColorCorrectionHandle_t a1 )	{ (g_pColorCorrectionSystem)->UnlockLookup( a1 ); };
	void SetLookupWeight( ColorCorrectionHandle_t a1, float a2 )	{ (g_pColorCorrectionSystem)->SetLookupWeight( a1, a2 ); };
	void ResetLookupWeights()	{ (g_pColorCorrectionSystem)->ResetLookupWeights(); };
	void SetResetable( ColorCorrectionHandle_t a1, bool a2 )	{ (g_pColorCorrectionSystem)->SetResetable( a1, a2 ); };

	void							SetFullScreenDepthTextureValidityFlag( bool bIsValid );



	void AntiAliasingHint( int a1 )	{ (g_pShaderAPI)->AntiAliasingHint( a1 ); };

	// A special path used to tick the front buffer while loading on the 360
	void							SetNonInteractiveLogoTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedW, float flNormalizedH );
	void							SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize );
	void							SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode );
	void							EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode );
	void			                RefreshFrontBufferNonInteractive();

	void FlipCulling( bool a1 )	{ (g_pShaderAPI)->FlipCulling( a1 ); };

//	DELEGATE_TO_OBJECT_1V(					EnableSinglePassFlashlightMode, bool, g_pShaderAPI );
	void							EnableSinglePassFlashlightMode( bool bEnable );	
	bool							SinglePassFlashlightModeEnabled() const;

	void DrawInstances( int a1, const MeshInstanceData_t * a2 )	{ (g_pShaderAPI)->DrawInstances( a1, a2 ); };

	void UpdateGameTime( float a1 )	{ (g_pShaderAPI)->UpdateGameTime( a1 ); };

	//--------------------------------------------------------
	// debug logging - no-op in queued context
	//--------------------------------------------------------
	void							Printf( char *fmt, ... );
	void							PrintfVA( char *fmt, std::va_list vargs );;
	float							Knob( char *knobname, float *setvalue=nullptr );	



	//---------------------------------------------------------
	// RocketUI rendering
	//--------------------------------------------------------
	void RenderRocketHUD(void *cmdList) { if (g_pRocketUI) g_pRocketUI->RenderHUDFrame(cmdList); }
	void RenderRocketMenu(void *cmdList) { if (g_pRocketUI) g_pRocketUI->RenderMenuFrame(cmdList); }

	ColorCorrectionHandle_t FindLookup( const char * a1 )	{ return (g_pColorCorrectionSystem)->FindLookup( a1 ); };

	//---------------------------------------------------------
protected:
	
	IMaterialInternal *GetMaterialInternal( MaterialHandle_t ) const;
	IMaterialInternal *GetDrawFlatMaterial();
	IMaterialInternal *GetRenderTargetBlitMaterial();
	IMaterialInternal *GetBufferClearObeyStencil( int i );
	IMaterialInternal *GetReloadZcullMaterial();

	ShaderAPITextureHandle_t GetFullbrightLightmapTextureHandle() const;
	ShaderAPITextureHandle_t GetFullbrightBumpedLightmapTextureHandle() const;
	ShaderAPITextureHandle_t GetBlackTextureHandle() const;
	ShaderAPITextureHandle_t GetBlackAlphaZeroTextureHandle() const;
	ShaderAPITextureHandle_t GetFlatNormalTextureHandle() const;
	ShaderAPITextureHandle_t GetFlatSSBumpTextureHandle() const;
	ShaderAPITextureHandle_t GetGreyTextureHandle() const;
	ShaderAPITextureHandle_t GetGreyAlphaZeroTextureHandle() const;
	ShaderAPITextureHandle_t GetWhiteTextureHandle() const;
	ShaderAPITextureHandle_t GetFrameBufferTextureHandle() const;
	ShaderAPITextureHandle_t GetMaxDepthTextureHandle() const;

	// Helper methods
	void BindLightmap( Sampler_t stage, TextureBindFlags_t nBindFlags );
	void BindBumpLightmap( Sampler_t stage, TextureBindFlags_t nBindFlags );
	void BindFullbrightLightmap( Sampler_t stage, TextureBindFlags_t nBindFlags );
	void BindBumpedFullbrightLightmap( Sampler_t stage, TextureBindFlags_t nBindFlags );
	void BindPaintTexture( Sampler_t stage, TextureBindFlags_t nBindFlags );

	void OnRenderDataUnreferenced();

	const CMatLightmaps *GetLightmaps() const;
	CMatLightmaps *GetLightmaps();

	const CMatPaintmaps *GetPaintmaps() const;
	CMatPaintmaps *GetPaintmaps();

	std::vector< PlaneStackElement >		m_CustomClipPlanes; //implemented as a vector so we can remove in special ways

	IMesh *m_pBatchIndices;
	IMesh *m_pBatchMesh;
	IIndexBuffer *m_pCurrentIndexBuffer;

	CTextureReference m_pNonInteractiveTempFullscreenBuffer[MATERIAL_NON_INTERACTIVE_MODE_COUNT];
	CTextureReference m_pNonInteractivePacifier;
	CTextureReference m_pNonInteractiveLogo;

	MaterialNonInteractiveMode_t m_NonInteractiveMode;

	float m_flNormalizedX;
	float m_flNormalizedY;
	float m_flNormalizedSize;

	float m_flLogoNormalizedX;
	float m_flLogoNormalizedY;
	float m_flLogoNormalizedW;
	float m_flLogoNormalizedH;
};


//-----------------------------------------------------------------------------

inline void CMatRenderContext::LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix )
{
	g_pShaderAPI->LoadBoneMatrix( boneIndex, matrix.Base() );
}

//-----------------------------------------------------------------------------


inline bool CMatRenderContext::InFlashlightMode() const
{
	return m_bFlashlightEnable;
}

inline bool CMatRenderContext::IsRenderingPaint() const
{
	return m_bRenderingPaint;
}

inline void CMatRenderContext::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	SetFlashlightStateEx( state, worldToTexture, nullptr );
}

inline bool CMatRenderContext::IsCascadedShadowMapping() const
{
	return m_bCascadedShadowMappingEnabled;
}

inline void CMatRenderContext::SetCascadedShadowMapping( bool bEnable )
{
	m_bCascadedShadowMappingEnabled = bEnable;
}

inline float CMatRenderContextBase::ComputePixelWidthOfSphere( const Vector& vecOrigin, float flRadius )
{
	return ComputePixelDiameterOfSphere( vecOrigin, flRadius ) * 2.0f;
}

inline void CMatRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	g_pShaderAPI->SceneFogColor3ub( rgb[0], rgb[1], rgb[2] );
}

inline void CMatRenderContext::FogMaxDensity( float flMaxDensity )
{
	g_pShaderAPI->FogMaxDensity( flMaxDensity );
}

inline ITexture *CMatRenderContext::GetLocalCubemap( void )
{
	return m_pLocalCubemapTexture;
}

inline void CMatRenderContextBase::SetRenderTarget( ITexture *pTexture_ )
{
	SetRenderTargetEx( 0, pTexture_ );
}

//-----------------------------------------------------------------------------
// Gets the bound morph's vertex format; returns 0 if no morph is bound
//-----------------------------------------------------------------------------
inline MorphFormat_t CMatRenderContext::GetBoundMorphFormat()
{
	return m_pBoundMorph ? m_pBoundMorph->GetMorphFormat() : 0;
}

//-----------------------------------------------------------------------------
// Use this to create static vertex and index buffers 
//-----------------------------------------------------------------------------
inline IMesh* CMatRenderContext::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial, VertexStreamSpec_t *pStreamSpec )
{
	return g_pShaderDevice->CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial, pStreamSpec );
}

inline void CMatRenderContext::SyncToken( const char *pToken )
{
	if ( g_pShaderAPI )
	{
		g_pShaderAPI->SyncToken( pToken );
	}
}

inline enum MaterialHeightClipMode_t CMatRenderContextBase::GetHeightClipMode( void )
{
	return m_HeightClipMode;
}

inline int CMatRenderContextBase::GetLightmapPage( void )
{
	return m_lightmapPageID;
}

inline CMaterialSystem *CMatRenderContext::GetMaterialSystem() const
{
	return m_pMaterialSystem;
}


//-----------------------------------------------------------------------------
