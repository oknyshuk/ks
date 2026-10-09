#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import ks.vstdlib;
#include "imatrendercontextinternal.h"
#include "cmatrendercontext.h"
import std;

class CMaterialSystem;
class CMatQueuedMesh;
class CMatQueuedIndexBuffer;
class CPrimList;

//-----------------------------------------------------------------------------





//-----------------------------------------------------------------------------

#define Unsupported( funcName ) ExecuteOnce( [&]{ Msg( "CMatQueuedRenderContext: %s is unsupported\n", #funcName ); } )
#define FATAL_QUEUE 1

#ifdef FATAL_QUEUE
#define CannotSupport() ExecuteOnce( [&]{ Msg( "Called function that cannot be supported\n" ); } ); ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } )
#else
#define CannotSupport() ExecuteOnce( [&]{ Msg( "Called function that cannot be supported\n" ); } )
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CMatQueuedRenderContext : public CMatRenderContextBase
{
	typedef CMatRenderContextBase BaseClass;
public:
	CMatQueuedRenderContext()
	 :	m_pHardwareContext( nullptr ), 
		m_iRenderDepth( 0 ), 
		m_pQueuedMesh( nullptr ),
		m_WidthBackBuffer( 0 ), 
		m_HeightBackBuffer( 0 ),
		m_FogMode( MATERIAL_FOG_NONE ),
		m_flFogStart( 0 ),
		m_flFogEnd( 0 ),
		m_flFogZ( 0 ),
		m_flFogMaxDensity( 1.0 )
	{
		std::memset( &m_FogColor, 0, sizeof(m_FogColor) );
	}

	void									CycleDynamicBuffers( ); // For use in mat_queue_mode 1 only

	void									Init( CMaterialSystem *pMaterialSystem, CMatRenderContextBase *pHardwareContext );
	void									Shutdown();

	void									CompactMemory();

	bool									IsInitialized() const { return ( m_pHardwareContext != nullptr ); }

	void									BeginQueue( CMatRenderContextBase *pInitialState = nullptr );
	void									EndQueue( bool bCallQueued = false );

	void									BeginRender();
	void									EndRender();

	void									CallQueued( bool bTermAfterCall = false );
	void									FlushQueued();

	ICallQueue *							GetCallQueue();
	CMatCallQueue *							GetCallQueueInternal() { return &m_queue; }

	void EvictManagedResources()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( g_pShaderAPI, (void (IShaderAPI::*)())&IShaderAPI::EvictManagedResources); };

	bool									OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool									OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool									OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices );
	bool									OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool									OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes );
	bool									OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type );


	void Flush( bool a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool))&IMatRenderContext::Flush, a1 ); };

	void SwapBuffers()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::SwapBuffers); };

	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );
	void									GetRenderTargetDimensions( int &, int &) const;
	void									GetViewport( int& x, int& y, int& width, int& height ) const;

	void									Bind( IMaterial *, void * );
	void BindLocalCubemap( ITexture * a1 )	{ BaseClass::BindLocalCubemap( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *))&IMatRenderContext::BindLocalCubemap, a1 ); };
	void BindLightmapPage( int a1 )	{ BaseClass::BindLightmapPage( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int))&IMatRenderContext::BindLightmapPage, a1 ); };
	void DepthRange( float a1, float a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(float, float))&IMatRenderContext::DepthRange, a1, a2 ); };
	void ClearBuffers( bool a1, bool a2, bool a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool, bool))&IMatRenderContext::ClearBuffers, a1, a2, a3 ); };

	void									ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr );
	void									ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = nullptr, CThreadEvent *pPixelsReadEvent = nullptr );
	void									ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = nullptr );

	void									SetLightingState( const MaterialLightingState_t &desc );
	void									SetLights( int nCount, const LightDesc_t *pLights );

	void									SetLightingOrigin( Vector vLightingOrigin );

	void									SetAmbientLightCube( LightCube_t cube );

	void CopyRenderTargetToTexture( ITexture * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *))&IMatRenderContext::CopyRenderTargetToTexture, a1 ); };
	void SetFrameBufferCopyTexture( ITexture * a1, int a2 )	{ BaseClass::SetFrameBufferCopyTexture( a1, a2 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, int))&IMatRenderContext::SetFrameBufferCopyTexture, a1, a2 ); };

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
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	void									Viewport( int x, int y, int width, int height );

	void CullMode( MaterialCullMode_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(MaterialCullMode_t))&IMatRenderContext::CullMode, a1 ); };
	void FlipCullMode()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::FlipCullMode); };

	void BeginGeneratingCSMs()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::BeginGeneratingCSMs); };
	void EndGeneratingCSMs()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::EndGeneratingCSMs); };
	void PerpareForCascadeDraw( int a1, float a2, float a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int, float, float))&IMatRenderContext::PerpareForCascadeDraw, a1, a2, a3 ); };

	void									FogMode( MaterialFogMode_t fogMode );
	void									FogStart( float fStart );
	void									FogEnd( float fEnd );
	void									SetFogZ( float fogZ );
	MaterialFogMode_t						GetFogMode( void );
	void									GetFogDistances( float *fStart, float *fEnd, float *fFogZ );
	void									FogMaxDensity( float flMaxDensity );

	void									FogColor3f( float r, float g, float b );
	void									FogColor3fv( float const* rgb );
	void									FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void									FogColor3ubv( unsigned char const* rgb );

	void									GetFogColor( unsigned char *rgb );

	int	GetCurrentNumBones( ) const;
	void SetNumBoneWeights( int nBoneCount );

	IMesh * CreateStaticMesh( VertexFormat_t a1, const char * a2, IMaterial * a3, VertexStreamSpec_t * a4 )	{ return (m_pHardwareContext)->CreateStaticMesh( a1, a2, a3, a4 ); };
	void DestroyStaticMesh( IMesh * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(IMesh *))&IMatRenderContext::DestroyStaticMesh, a1 ); };

	IMesh *									GetDynamicMesh(bool, IMesh *, IMesh *, IMaterial * );
	IMesh*									GetDynamicMeshEx( VertexFormat_t, bool, IMesh*, IMesh*, IMaterial* );

// ------------ New Vertex/Index Buffer interface ----------------------------
	IVertexBuffer * CreateStaticVertexBuffer( VertexFormat_t a1, int a2, const char * a3 )	{ return (m_pHardwareContext)->CreateStaticVertexBuffer( a1, a2, a3 ); };
	IIndexBuffer * CreateStaticIndexBuffer( MaterialIndexFormat_t a1, int a2, const char * a3 )	{ return (m_pHardwareContext)->CreateStaticIndexBuffer( a1, a2, a3 ); };
	void DestroyVertexBuffer( IVertexBuffer * a1 )	{ (m_pHardwareContext)->DestroyVertexBuffer( a1 ); };
	void DestroyIndexBuffer( IIndexBuffer * a1 )	{ (m_pHardwareContext)->DestroyIndexBuffer( a1 ); };
	IVertexBuffer * GetDynamicVertexBuffer( int a1, VertexFormat_t a2, bool a3 )	{ return (m_pHardwareContext)->GetDynamicVertexBuffer( a1, a2, a3 ); };
	IIndexBuffer * 							GetDynamicIndexBuffer( );
	void BindVertexBuffer( int a1, IVertexBuffer * a2, int a3, int a4, int a5, VertexFormat_t a6, int a7 )	{ (m_pHardwareContext)->BindVertexBuffer( a1, a2, a3, a4, a5, a6, a7 ); };
	void BindIndexBuffer( IIndexBuffer * a1, int a2 )	{ (m_pHardwareContext)->BindIndexBuffer( a1, a2 ); };
	void Draw( MaterialPrimitiveType_t a1, int a2, int a3 )	{ (m_pHardwareContext)->Draw( a1, a2, a3 ); };
// ------------ End ----------------------------

	int SelectionMode( bool )
	{
		CannotSupport();
		return 0;
	}

	void SelectionBuffer( unsigned int *, int )
	{
		CannotSupport();
	}

	void ClearSelectionNames()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::ClearSelectionNames); };
	void LoadSelectionName( int a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int))&IMatRenderContext::LoadSelectionName, a1 ); };
	void PushSelectionName( int a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int))&IMatRenderContext::PushSelectionName, a1 ); };
	void PopSelectionName()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::PopSelectionName); };
	void ClearColor3ub( unsigned char a1, unsigned char a2, unsigned char a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(unsigned char, unsigned char, unsigned char))&IMatRenderContext::ClearColor3ub, a1, a2, a3 ); };
	void ClearColor4ub( unsigned char a1, unsigned char a2, unsigned char a3, unsigned char a4 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(unsigned char, unsigned char, unsigned char, unsigned char))&IMatRenderContext::ClearColor4ub, a1, a2, a3, a4 ); };
	void OverrideDepthEnable( bool a1, bool a2, bool a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool, bool))&IMatRenderContext::OverrideDepthEnable, a1, a2, a3 ); };
	void OverrideAlphaWriteEnable( bool a1, bool a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool))&IMatRenderContext::OverrideAlphaWriteEnable, a1, a2 ); };
	void OverrideColorWriteEnable( bool a1, bool a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool))&IMatRenderContext::OverrideColorWriteEnable, a1, a2 ); };
	void DrawScreenSpaceQuad( IMaterial * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(IMaterial *))&IMatRenderContext::DrawScreenSpaceQuad, a1 ); };

	void									SyncToken( const char *p );

	// Allocate and delete query objects.
	OcclusionQueryObjectHandle_t			CreateOcclusionQueryObject();
	void DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(OcclusionQueryObjectHandle_t))&IMatRenderContext::DestroyOcclusionQueryObject, a1 ); };
	void BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(OcclusionQueryObjectHandle_t))&IMatRenderContext::BeginOcclusionQueryDrawing, a1 ); };
	void EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(OcclusionQueryObjectHandle_t))&IMatRenderContext::EndOcclusionQueryDrawing, a1 ); };
	void ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(OcclusionQueryObjectHandle_t))&IMatRenderContext::ResetOcclusionQueryObject, a1 ); };
	int										OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t );

	void									SetFlashlightState( const FlashlightState_t &s, const VMatrix &m );

	void SetHeightClipMode( MaterialHeightClipMode_t a1 )	{ BaseClass::SetHeightClipMode( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(MaterialHeightClipMode_t))&IMatRenderContext::SetHeightClipMode, a1 ); };
	void SetHeightClipZ( float a1 )	{ BaseClass::SetHeightClipZ( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(float))&IMatRenderContext::SetHeightClipZ, a1 ); };

	bool									EnableClipping( bool bEnable );

	void EnableUserClipTransformOverride( bool a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool))&IMatRenderContext::EnableUserClipTransformOverride, a1 ); };

	void									UserClipTransform( const VMatrix &m );

	IMorph *CreateMorph( MorphFormat_t format, const char *pDebugName )
	{
		CannotSupport();
		return nullptr;
	}

	void DestroyMorph( IMorph * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(IMorph *))&IMatRenderContext::DestroyMorph, a1 ); };
	void BindMorph( IMorph * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(IMorph *))&IMatRenderContext::BindMorph, a1 ); };

	void									ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride );

	void									GetWindowSize( int &width, int &height ) const;

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

	void									LoadBoneMatrix( int i, const matrix3x4_t &m );

	void PushRenderTargetAndViewport( ITexture * a1, int a2, int a3, int a4, int a5 )	{ BaseClass::PushRenderTargetAndViewport( a1, a2, a3, a4, a5 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, int, int, int, int))&IMatRenderContext::PushRenderTargetAndViewport, a1, a2, a3, a4, a5 ); };
	void PushRenderTargetAndViewport( ITexture * a1, ITexture * a2, int a3, int a4, int a5, int a6 )	{ BaseClass::PushRenderTargetAndViewport( a1, a2, a3, a4, a5, a6 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, ITexture *, int, int, int, int))&IMatRenderContext::PushRenderTargetAndViewport, a1, a2, a3, a4, a5, a6 ); };
	void PushRenderTargetAndViewport( ITexture * a1 )	{ BaseClass::PushRenderTargetAndViewport( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *))&IMatRenderContext::PushRenderTargetAndViewport, a1 ); };
	void PushRenderTargetAndViewport()	{ BaseClass::PushRenderTargetAndViewport(); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::PushRenderTargetAndViewport ); };
	void PopRenderTargetAndViewport()	{ BaseClass::PopRenderTargetAndViewport(); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::PopRenderTargetAndViewport ); };
	void BindLightmapTexture( ITexture * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *))&IMatRenderContext::BindLightmapTexture, a1 ); };

	void									CopyRenderTargetToTextureEx( ITexture *pTexture, int i, Rect_t *pSrc, Rect_t *pDst );
	void									CopyTextureToRenderTargetEx( int i, ITexture *pTexture, Rect_t *pSrc, Rect_t *pDst );

	void SetFloatRenderingParameter( int a1, float a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int, float))&IMatRenderContext::SetFloatRenderingParameter, a1, a2 ); };
	void SetIntRenderingParameter( int a1, int a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int, int))&IMatRenderContext::SetIntRenderingParameter, a1, a2 ); };
	void SetTextureRenderingParameter( int a1, ITexture * a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int, ITexture *))&IMatRenderContext::SetTextureRenderingParameter, a1, a2 ); };

	void SetVectorRenderingParameter( int i, const Vector &v )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetVectorRenderingParameter, i, RefToVal( v ) );
	}

	float GetFloatRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return 0;
	}

	ITexture *GetTextureRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return 0;
	}

	Vector GetVectorRenderingParameter(int parm_number) const
	{
		CannotSupport();
		return Vector(0,0,0);
	}

	virtual void SetStencilState( const ShaderStencilState_t &state )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetStencilState, RefToVal( state ) );
	}

	void ClearStencilBufferRectangle( int a1, int a2, int a3, int a4, int a5 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int, int, int, int, int))&IMatRenderContext::ClearStencilBufferRectangle, a1, a2, a3, a4, a5 ); };

	void SetShadowDepthBiasFactors( float a1, float a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(float, float))&IMatRenderContext::SetShadowDepthBiasFactors, a1, a2 ); };

	void PushCustomClipPlane( const float *p )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushCustomClipPlane, m_queue.CopyArray( p, 4 ) );
	}

	void PopCustomClipPlane()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::PopCustomClipPlane); };

	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial );
	virtual int GetMaxIndicesToRender( );

	void DisableAllLocalLights()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::DisableAllLocalLights); };

	int CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightmapID1, int lightmapID2 ) 
	{ 
		CannotSupport();
		return 0; 
	}

	IMesh *GetFlexMesh();

	void SetFlashlightStateEx( const FlashlightState_t &s, const VMatrix &m, ITexture *p )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightStateEx, RefToVal( s ), RefToVal( m ), p );
	}

	void PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushScissorRect, nLeft, nTop, nRight, nBottom );
	}

	void PopScissorRect( )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PopScissorRect );
	}

	virtual void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance );

	virtual void PushDeformation( const DeformationBase_t *pDef )
	{
		CannotSupport();
	}

	void PopDeformation( )
	{
		CannotSupport();
	}

	int GetNumActiveDeformations( ) const
	{
		return 0;
	}
	
	ITexture *GetLocalCubemap()
	{
		return m_pLocalCubemapTexture;
	}

	void ClearBuffersObeyStencil( bool a1, bool a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool))&IMatRenderContext::ClearBuffersObeyStencil, a1, a2 ); };
	void ClearBuffersObeyStencilEx( bool a1, bool a2, bool a3 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool, bool, bool))&IMatRenderContext::ClearBuffersObeyStencilEx, a1, a2, a3 ); };
	void PerformFullScreenStencilOperation()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::PerformFullScreenStencilOperation); };
	void SetRenderingPaint( bool a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool))&IMatRenderContext::SetRenderingPaint, a1 ); };

	bool GetUserClipTransform( VMatrix &worldToView )
	{
		CannotSupport();
		return false;
	}

	bool InFlashlightMode( void ) const
	{
		CannotSupport();
		return false;
	}

	bool IsRenderingPaint() const
	{
		CannotSupport();
		return false;
	}

	virtual void SetFlashlightMode( bool bEnable )
	{
		m_bFlashlightEnable = bEnable;
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightMode, bEnable );
	}

	virtual bool GetFlashlightMode( void ) const { return m_bFlashlightEnable; }

	virtual bool IsCullingEnabledForSinglePassFlashlight( void ) const { return m_bCullingEnabledForSinglePassFlashlight; }

	virtual void EnableCullingForSinglePassFlashlight( bool bEnable ) 
	{ 
		m_bCullingEnabledForSinglePassFlashlight = bEnable; 
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EnableCullingForSinglePassFlashlight, bEnable );
	}

	virtual bool IsCascadedShadowMapping() const
	{
		CannotSupport();
		return false;
	}

	virtual void SetCascadedShadowMapping( bool bEnable )
	{
		m_bCascadedShadowMappingEnabled = bEnable;
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetCascadedShadowMapping, bEnable );
	}

	virtual void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
	{
		state, pDepthTextureAtlas;
		
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetCascadedShadowMappingState, RefToVal( state ), pDepthTextureAtlas );
	}
	
	void BeginPIXEvent( unsigned long color, const char *pszName )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BeginPIXEvent, color, m_queue.Copy( pszName ) );
	}

	void EndPIXEvent( )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndPIXEvent );
	}

	void SetPIXMarker( unsigned long color, const char *pszName )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetPIXMarker, color, m_queue.Copy( pszName ) );
	}

	void ForceHardwareSync()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::ForceHardwareSync); };
	void BeginFrame()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::BeginFrame); };
	void EndFrame()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::EndFrame); };

	void BeginMorphAccumulation()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::BeginMorphAccumulation); };
	void EndMorphAccumulation()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)())&IMatRenderContextInternal::EndMorphAccumulation); };
	virtual void AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
	{
		// FIXME: Must copy off the morph weights here. Not sure the pattern for this.
		contract_assert( 0 );
	}
	virtual bool GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pIMorph, int nVertex )
	{
		// FIXME: We must assign morph ids in the queued mode 
		// and pass the ids down to the morph mgr to get the texcoord
		contract_assert( 0 );
		pTexCoord->Init();
		return false;
	}

	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
	{
		contract_assert( 0 );
	}

	// Subdivision surface interface
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

	void UpdateGameTime( float a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(float))&IMatRenderContext::UpdateGameTime, a1 ); };


	//-------------------------------------------------------------------------

	void SetCurrentMaterialInternal( IMaterialInternal * a1 )	{ BaseClass::SetCurrentMaterialInternal( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)(IMaterialInternal *))&IMatRenderContextInternal::SetCurrentMaterialInternal, a1 ); };
	void SetCurrentProxy( void * a1 )	{ BaseClass::SetCurrentProxy( a1 ); (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContextInternal::*)(void *))&IMatRenderContextInternal::SetCurrentProxy, a1 ); };

	int GetLightmapPage()
	{
		CannotSupport();
		return 0;
	}

	void ForceDepthFuncEquals( bool )
	{
		CannotSupport();
	}

	void BindStandardTexture( Sampler_t, TextureBindFlags_t nBindFlags, StandardTextureId_t )
	{
		CannotSupport();
	}

	void GetLightmapDimensions( int *, int * )
	{
		CannotSupport();
	}

	MorphFormat_t GetBoundMorphFormat()
	{
		CannotSupport();
		return (MorphFormat_t)0;
	}

	void DrawClearBufferQuad( unsigned char, unsigned char, unsigned char, unsigned char, bool, bool, bool )
	{
		CannotSupport();
	}


	void BeginBatch( IMesh* pIndices );
	void BindBatch( IMesh* pVertices, IMaterial *pAutoBind = nullptr );
	void DrawBatch( MaterialPrimitiveType_t primType, int firstIndex, int numIndices );
	void EndBatch();

	// Color correction related methods..
	// Client cannot call IColorCorrectionSystem directly because it is not thread-safe
	// FIXME: Make IColorCorrectionSystem threadsafe?
	virtual ColorCorrectionHandle_t AddLookup( const char *pName );
	virtual void LockLookup( ColorCorrectionHandle_t handle );
	virtual void LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName );
	virtual void UnlockLookup( ColorCorrectionHandle_t handle );
	virtual bool RemoveLookup( ColorCorrectionHandle_t handle );

	void EnableColorCorrection( bool a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( g_pColorCorrectionSystem, (void (IColorCorrectionSystem::*)(bool))&IColorCorrectionSystem::EnableColorCorrection, a1 ); };
	void ResetLookupWeights()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( g_pColorCorrectionSystem, (void (IColorCorrectionSystem::*)())&IColorCorrectionSystem::ResetLookupWeights); };
	void SetLookupWeight( ColorCorrectionHandle_t a1, float a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( g_pColorCorrectionSystem, (void (IColorCorrectionSystem::*)(ColorCorrectionHandle_t, float))&IColorCorrectionSystem::SetLookupWeight, a1, a2 ); };
	void SetResetable( ColorCorrectionHandle_t a1, bool a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( g_pColorCorrectionSystem, (void (IColorCorrectionSystem::*)(ColorCorrectionHandle_t, bool))&IColorCorrectionSystem::SetResetable, a1, a2 ); };

	void SetFullScreenDepthTextureValidityFlag( bool a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(bool))&IMatRenderContext::SetFullScreenDepthTextureValidityFlag, a1 ); };

	void SetToneMappingScaleLinear( const Vector &scale )
	{
		m_LastSetToneMapScale = scale;
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetToneMappingScaleLinear, RefToVal( scale ) );
	}

	Vector GetToneMappingScaleLinear( void )
	{
		return m_LastSetToneMapScale;
	}
	

	void DeferredBeginBatch( );
	void DeferredDrawPrimList( IMesh *pMesh, CPrimList *pLists, int nLists );
	void DeferredSetFlexMesh( IMesh *pStaticMesh, int nVertexOffsetInBytes );



	void AntiAliasingHint( int a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(int))&IMatRenderContext::AntiAliasingHint, a1 ); };

	// A special path used to tick the front buffer while loading on the 360
	void SetNonInteractiveLogoTexture( ITexture * a1, float a2, float a3, float a4, float a5 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, float, float, float, float))&IMatRenderContext::SetNonInteractiveLogoTexture, a1, a2, a3, a4, a5 ); };
	void SetNonInteractivePacifierTexture( ITexture * a1, float a2, float a3, float a4 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, float, float, float))&IMatRenderContext::SetNonInteractivePacifierTexture, a1, a2, a3, a4 ); };
	void SetNonInteractiveTempFullscreenBuffer( ITexture * a1, MaterialNonInteractiveMode_t a2 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(ITexture *, MaterialNonInteractiveMode_t))&IMatRenderContext::SetNonInteractiveTempFullscreenBuffer, a1, a2 ); };
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(MaterialNonInteractiveMode_t))&IMatRenderContext::EnableNonInteractiveMode, a1 ); };
	void RefreshFrontBufferNonInteractive()	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)())&IMatRenderContext::RefreshFrontBufferNonInteractive); };

	void FlipCulling( bool a1 )	{ (g_pShaderAPI)->FlipCulling( a1 ); };

//	DEFINE_QUEUED_CALL_1(					EnableSinglePassFlashlightMode, bool, IMatRenderContext, m_pHardwareContext );

	virtual void EnableSinglePassFlashlightMode( bool bEnable ) 
	{ 
		m_bSinglePassFlashlightMode = bEnable;
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EnableSinglePassFlashlightMode, bEnable );
	}

	virtual bool SinglePassFlashlightModeEnabled( void ) const { return m_bSinglePassFlashlightMode; }

	//--------------------------------------------------------
	// Memory allocation calls for queued mesh, et. al.
	//--------------------------------------------------------
	byte *AllocVertices( int nVerts, int nVertexSize, bool *pUsingExternalMemory );
	byte *AllocIndices( int nIndices, int nIndexSize, bool *pUsingExternalMemory );
	byte *ReallocVertices( byte *pVerts, int nVertsOld, int nVertsNew, int nVertexSize, bool bExternalMemory );
	byte *ReallocIndices( byte *pIndices, int nIndicesOld, int nIndicesNew, int nIndexSize, bool bExternalMemory );
	void FreeVertices( byte *pVerts, int nVerts, int nVertexSize );
	void FreeIndices( byte *pIndices, int nIndices, int nIndexSize );

	//--------------------------------------------------------
	// debug logging - no-op in queued context
	//--------------------------------------------------------
	virtual void							Printf( char *fmt, ... ) {};
	virtual void							PrintfVA( char *fmt, std::va_list vargs ){};
	virtual float							Knob( char *knobname, float *setvalue=nullptr ) { return 0.0f; };	



	//--------------------------------------------------------
	// RocketUI rendering
	//--------------------------------------------------------
	void RenderRocketHUD( void * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(void *))&IMatRenderContext::RenderRocketHUD, a1 ); };
	void RenderRocketMenu( void * a1 )	{ (*(const_cast<CMatCallQueue *>(&m_queue))).QueueCall( m_pHardwareContext, (void (IMatRenderContext::*)(void *))&IMatRenderContext::RenderRocketMenu, a1 ); };

	virtual ColorCorrectionHandle_t			FindLookup( const char *pName );

private:
	void QueueMatrixSync();
	void DeferredDrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstances );

	[[gnu::always_inline]] inline CMemoryStack &Vertices()
	{
		return m_Vertices;
	}
	[[gnu::always_inline]] inline CMemoryStack &Indices()
	{
		return m_Indices;
	}

	//friend class CMatQueuedMesh;
	friend class CCallQueueExternal;

	CMatCallQueue m_queue;
	CMatQueuedMesh *m_pQueuedMesh;
	CMatQueuedMesh *m_pQueuedFlexMesh;
	CMatQueuedIndexBuffer *m_pQueuedIndexBuffer;

	CMatRenderContextBase *m_pHardwareContext;

	int m_iRenderDepth;
	int m_WidthBackBuffer, m_HeightBackBuffer;
	int m_nBoneCount;
	MaterialFogMode_t m_FogMode;
	float m_flFogStart;
	float m_flFogEnd;
	float m_flFogZ;
	float m_flFogMaxDensity;
	color24 m_FogColor;

	CMemoryStack m_Vertices;
	CMemoryStack m_Indices;

	class CCallQueueExternal : public ICallQueue
	{
		void QueueFunctorInternal( CFunctor *pFunctor )
		{
			// m_CallQueueExternal is a member of the enclosing context; recover the outer object
			// from the address of this member.
			CMatQueuedRenderContext *pOuter = reinterpret_cast<CMatQueuedRenderContext *>(
				reinterpret_cast<uint8 *>( this ) - __builtin_offsetof( CMatQueuedRenderContext, m_CallQueueExternal ) );
			pOuter->m_queue.QueueFunctor( pFunctor );
			pFunctor->Release();
		}
	};

	CCallQueueExternal m_CallQueueExternal;
};

//-----------------------------------------------------------------------------
