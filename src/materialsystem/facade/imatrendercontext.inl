import ks.core.basetypes;
import ks.core.threadtools;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.lightdesc;
import ks.math.mathlib;
import ks.math.polyhedron;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.tier1.exprevaluator;
import ks.vstdlib.callqueue;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatRenderContext class to the render context implementations, selected by the kind
// the context was created with. Included by cmatrendercontextdispatch.cpp.

int IMatRenderContext::AddRef()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->AddRef();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->AddRef();
	}
	std::unreachable();
}
int IMatRenderContext::Release()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->Release();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->Release();
	}
	std::unreachable();
}
void IMatRenderContext::BeginRender()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginRender(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginRender(); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndRender()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndRender(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndRender(); return;
	}
	std::unreachable();
}
void IMatRenderContext::Flush( bool flushHardware )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Flush( flushHardware ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Flush( flushHardware ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindLocalCubemap( ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindLocalCubemap( pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindLocalCubemap( pTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetRenderTarget( ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetRenderTarget( pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetRenderTarget( pTexture ); return;
	}
	std::unreachable();
}
ITexture * IMatRenderContext::GetRenderTarget()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetRenderTarget();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetRenderTarget();
	}
	std::unreachable();
}
void IMatRenderContext::GetRenderTargetDimensions( int &width, int &height ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<const CMatRenderContext *>( this )->GetRenderTargetDimensions( width, height ); return;
	case MatRenderContextKind::Queued: static_cast<const CMatQueuedRenderContext *>( this )->GetRenderTargetDimensions( width, height ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Bind( IMaterial *material, void *proxyData )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Bind( material, proxyData ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Bind( material, proxyData ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindLightmapPage( int lightmapPageID )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindLightmapPage( lightmapPageID ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindLightmapPage( lightmapPageID ); return;
	}
	std::unreachable();
}
void IMatRenderContext::DepthRange( float zNear, float zFar )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DepthRange( zNear, zFar ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DepthRange( zNear, zFar ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearBuffers( bClearColor, bClearDepth, bClearStencil ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearBuffers( bClearColor, bClearDepth, bClearStencil ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ReadPixels( x, y, width, height, data, dstFormat, pRenderTargetTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ReadPixels( x, y, width, height, data, dstFormat, pRenderTargetTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ReadPixelsAsync( x, y, width, height, data, dstFormat, pRenderTargetTexture, pPixelsReadEvent ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ReadPixelsAsync( x, y, width, height, data, dstFormat, pRenderTargetTexture, pPixelsReadEvent ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ReadPixelsAsyncGetResult( x, y, width, height, data, dstFormat, pGetResultEvent ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ReadPixelsAsyncGetResult( x, y, width, height, data, dstFormat, pGetResultEvent ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetLightingState( const MaterialLightingState_t& state )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetLightingState( state ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetLightingState( state ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetLights( int nCount, const LightDesc_t *pLights )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetLights( nCount, pLights ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetLights( nCount, pLights ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetAmbientLightCube( Vector4D cube[6] )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetAmbientLightCube( cube ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetAmbientLightCube( cube ); return;
	}
	std::unreachable();
}
void IMatRenderContext::CopyRenderTargetToTexture( ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->CopyRenderTargetToTexture( pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->CopyRenderTargetToTexture( pTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFrameBufferCopyTexture( pTexture, textureIndex ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFrameBufferCopyTexture( pTexture, textureIndex ); return;
	}
	std::unreachable();
}
ITexture * IMatRenderContext::GetFrameBufferCopyTexture( int textureIndex )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetFrameBufferCopyTexture( textureIndex );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetFrameBufferCopyTexture( textureIndex );
	}
	std::unreachable();
}
void IMatRenderContext::MatrixMode( MaterialMatrixMode_t matrixMode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MatrixMode( matrixMode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MatrixMode( matrixMode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushMatrix()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushMatrix(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushMatrix(); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopMatrix()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopMatrix(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopMatrix(); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadMatrix( VMatrix const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadMatrix( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadMatrix( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadMatrix( matrix3x4_t const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadMatrix( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadMatrix( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::MultMatrix( VMatrix const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MultMatrix( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MultMatrix( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::MultMatrix( matrix3x4_t const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MultMatrix( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MultMatrix( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::MultMatrixLocal( VMatrix const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MultMatrixLocal( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MultMatrixLocal( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::MultMatrixLocal( matrix3x4_t const& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MultMatrixLocal( matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MultMatrixLocal( matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetMatrix( matrixMode, matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetMatrix( matrixMode, matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetMatrix( matrixMode, matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetMatrix( matrixMode, matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadIdentity()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadIdentity(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadIdentity(); return;
	}
	std::unreachable();
}
void IMatRenderContext::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Ortho( left, top, right, bottom, zNear, zFar ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Ortho( left, top, right, bottom, zNear, zFar ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PerspectiveX( fovx, aspect, zNear, zFar ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PerspectiveX( fovx, aspect, zNear, zFar ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PickMatrix( int x, int y, int width, int height )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PickMatrix( x, y, width, height ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PickMatrix( x, y, width, height ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Rotate( float angle, float x, float y, float z )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Rotate( angle, x, y, z ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Rotate( angle, x, y, z ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Translate( float x, float y, float z )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Translate( x, y, z ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Translate( x, y, z ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Scale( float x, float y, float z )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Scale( x, y, z ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Scale( x, y, z ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Viewport( int x, int y, int width, int height )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Viewport( x, y, width, height ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Viewport( x, y, width, height ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetViewport( int& x, int& y, int& width, int& height ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<const CMatRenderContext *>( this )->GetViewport( x, y, width, height ); return;
	case MatRenderContextKind::Queued: static_cast<const CMatQueuedRenderContext *>( this )->GetViewport( x, y, width, height ); return;
	}
	std::unreachable();
}
void IMatRenderContext::CullMode( MaterialCullMode_t cullMode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->CullMode( cullMode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->CullMode( cullMode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FlipCullMode()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FlipCullMode(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FlipCullMode(); return;
	}
	std::unreachable();
}
void IMatRenderContext::BeginGeneratingCSMs()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginGeneratingCSMs(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginGeneratingCSMs(); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndGeneratingCSMs()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndGeneratingCSMs(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndGeneratingCSMs(); return;
	}
	std::unreachable();
}
void IMatRenderContext::PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PerpareForCascadeDraw( cascade, fShadowSlopeScaleDepthBias, fShadowDepthBias ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PerpareForCascadeDraw( cascade, fShadowSlopeScaleDepthBias, fShadowDepthBias ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetHeightClipMode( MaterialHeightClipMode_t nHeightClipMode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetHeightClipMode( nHeightClipMode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetHeightClipMode( nHeightClipMode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetHeightClipZ( float z )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetHeightClipZ( z ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetHeightClipZ( z ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogMode( MaterialFogMode_t fogMode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogMode( fogMode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogMode( fogMode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogStart( float fStart )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogStart( fStart ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogStart( fStart ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogEnd( float fEnd )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogEnd( fEnd ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogEnd( fEnd ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFogZ( float fogZ )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFogZ( fogZ ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFogZ( fogZ ); return;
	}
	std::unreachable();
}
MaterialFogMode_t IMatRenderContext::GetFogMode()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetFogMode();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetFogMode();
	}
	std::unreachable();
}
void IMatRenderContext::FogColor3f( float r, float g, float b )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogColor3f( r, g, b ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogColor3f( r, g, b ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogColor3fv( float const* rgb )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogColor3fv( rgb ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogColor3fv( rgb ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogColor3ub( r, g, b ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogColor3ub( r, g, b ); return;
	}
	std::unreachable();
}
void IMatRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogColor3ubv( rgb ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogColor3ubv( rgb ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetFogColor( unsigned char *rgb )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetFogColor( rgb ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetFogColor( rgb ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetNumBoneWeights( int numBones )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetNumBoneWeights( numBones ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetNumBoneWeights( numBones ); return;
	}
	std::unreachable();
}
IMesh* IMatRenderContext::CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial, VertexStreamSpec_t *pStreamSpec )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CreateStaticMesh( fmt, pTextureBudgetGroup, pMaterial, pStreamSpec );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CreateStaticMesh( fmt, pTextureBudgetGroup, pMaterial, pStreamSpec );
	}
	std::unreachable();
}
void IMatRenderContext::DestroyStaticMesh( IMesh* mesh )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DestroyStaticMesh( mesh ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DestroyStaticMesh( mesh ); return;
	}
	std::unreachable();
}
IMesh* IMatRenderContext::GetDynamicMesh( bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetDynamicMesh( buffered, pVertexOverride, pIndexOverride, pAutoBind );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetDynamicMesh( buffered, pVertexOverride, pIndexOverride, pAutoBind );
	}
	std::unreachable();
}
IVertexBuffer * IMatRenderContext::CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CreateStaticVertexBuffer( fmt, nVertexCount, pTextureBudgetGroup );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CreateStaticVertexBuffer( fmt, nVertexCount, pTextureBudgetGroup );
	}
	std::unreachable();
}
IIndexBuffer * IMatRenderContext::CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CreateStaticIndexBuffer( fmt, nIndexCount, pTextureBudgetGroup );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CreateStaticIndexBuffer( fmt, nIndexCount, pTextureBudgetGroup );
	}
	std::unreachable();
}
void IMatRenderContext::DestroyVertexBuffer( IVertexBuffer * arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DestroyVertexBuffer( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DestroyVertexBuffer( arg0 ); return;
	}
	std::unreachable();
}
void IMatRenderContext::DestroyIndexBuffer( IIndexBuffer * arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DestroyIndexBuffer( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DestroyIndexBuffer( arg0 ); return;
	}
	std::unreachable();
}
IVertexBuffer * IMatRenderContext::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetDynamicVertexBuffer( streamID, vertexFormat, bBuffered );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetDynamicVertexBuffer( streamID, vertexFormat, bBuffered );
	}
	std::unreachable();
}
IIndexBuffer * IMatRenderContext::GetDynamicIndexBuffer()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetDynamicIndexBuffer();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetDynamicIndexBuffer();
	}
	std::unreachable();
}
void IMatRenderContext::BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindVertexBuffer( streamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindVertexBuffer( streamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindIndexBuffer( pIndexBuffer, nOffsetInBytes ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindIndexBuffer( pIndexBuffer, nOffsetInBytes ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Draw( primitiveType, firstIndex, numIndices ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Draw( primitiveType, firstIndex, numIndices ); return;
	}
	std::unreachable();
}
int IMatRenderContext::SelectionMode( bool selectionMode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->SelectionMode( selectionMode );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->SelectionMode( selectionMode );
	}
	std::unreachable();
}
void IMatRenderContext::SelectionBuffer( unsigned int* pBuffer, int size )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SelectionBuffer( pBuffer, size ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SelectionBuffer( pBuffer, size ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearSelectionNames()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearSelectionNames(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearSelectionNames(); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadSelectionName( int name )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadSelectionName( name ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadSelectionName( name ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushSelectionName( int name )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushSelectionName( name ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushSelectionName( name ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopSelectionName()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopSelectionName(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopSelectionName(); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearColor3ub( r, g, b ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearColor3ub( r, g, b ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearColor4ub( r, g, b, a ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearColor4ub( r, g, b, a ); return;
	}
	std::unreachable();
}
void IMatRenderContext::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->OverrideDepthEnable( bEnable, bDepthWriteEnable, bDepthTestEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->OverrideDepthEnable( bEnable, bDepthWriteEnable, bDepthTestEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::DrawScreenSpaceQuad( IMaterial* pMaterial )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DrawScreenSpaceQuad( pMaterial ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DrawScreenSpaceQuad( pMaterial ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SyncToken( const char *pToken )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SyncToken( pToken ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SyncToken( pToken ); return;
	}
	std::unreachable();
}
float IMatRenderContext::ComputePixelWidthOfSphere( const Vector& origin, float flRadius )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->ComputePixelWidthOfSphere( origin, flRadius );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->ComputePixelWidthOfSphere( origin, flRadius );
	}
	std::unreachable();
}
OcclusionQueryObjectHandle_t IMatRenderContext::CreateOcclusionQueryObject()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CreateOcclusionQueryObject();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CreateOcclusionQueryObject();
	}
	std::unreachable();
}
void IMatRenderContext::DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DestroyOcclusionQueryObject( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DestroyOcclusionQueryObject( arg0 ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginOcclusionQueryDrawing( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginOcclusionQueryDrawing( arg0 ); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndOcclusionQueryDrawing( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndOcclusionQueryDrawing( arg0 ); return;
	}
	std::unreachable();
}
int IMatRenderContext::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OcclusionQuery_GetNumPixelsRendered( arg0 );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OcclusionQuery_GetNumPixelsRendered( arg0 );
	}
	std::unreachable();
}
void IMatRenderContext::SetFlashlightMode( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFlashlightMode( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFlashlightMode( bEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFlashlightState( state, worldToTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFlashlightState( state, worldToTexture ); return;
	}
	std::unreachable();
}
bool IMatRenderContext::IsCascadedShadowMapping() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->IsCascadedShadowMapping();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->IsCascadedShadowMapping();
	}
	std::unreachable();
}
void IMatRenderContext::SetCascadedShadowMapping( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetCascadedShadowMapping( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetCascadedShadowMapping( bEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetCascadedShadowMappingState( state, pDepthTextureAtlas ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetCascadedShadowMappingState( state, pDepthTextureAtlas ); return;
	}
	std::unreachable();
}
MaterialHeightClipMode_t IMatRenderContext::GetHeightClipMode()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetHeightClipMode();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetHeightClipMode();
	}
	std::unreachable();
}
float IMatRenderContext::ComputePixelDiameterOfSphere( const Vector& vecAbsOrigin, float flRadius )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->ComputePixelDiameterOfSphere( vecAbsOrigin, flRadius );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->ComputePixelDiameterOfSphere( vecAbsOrigin, flRadius );
	}
	std::unreachable();
}
void IMatRenderContext::EnableUserClipTransformOverride( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EnableUserClipTransformOverride( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EnableUserClipTransformOverride( bEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::UserClipTransform( const VMatrix &worldToView )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UserClipTransform( worldToView ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UserClipTransform( worldToView ); return;
	}
	std::unreachable();
}
bool IMatRenderContext::GetFlashlightMode() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetFlashlightMode();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetFlashlightMode();
	}
	std::unreachable();
}
bool IMatRenderContext::IsCullingEnabledForSinglePassFlashlight() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->IsCullingEnabledForSinglePassFlashlight();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->IsCullingEnabledForSinglePassFlashlight();
	}
	std::unreachable();
}
void IMatRenderContext::EnableCullingForSinglePassFlashlight( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EnableCullingForSinglePassFlashlight( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EnableCullingForSinglePassFlashlight( bEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ResetOcclusionQueryObject( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ResetOcclusionQueryObject( arg0 ); return;
	}
	std::unreachable();
}
IMorph * IMatRenderContext::CreateMorph( MorphFormat_t format, const char *pDebugName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CreateMorph( format, pDebugName );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CreateMorph( format, pDebugName );
	}
	std::unreachable();
}
void IMatRenderContext::DestroyMorph( IMorph *pMorph )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DestroyMorph( pMorph ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DestroyMorph( pMorph ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindMorph( IMorph *pMorph )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindMorph( pMorph ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindMorph( pMorph ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFlexWeights( nFirstWeight, nCount, pWeights ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFlexWeights( nFirstWeight, nCount, pWeights ); return;
	}
	std::unreachable();
}
void * IMatRenderContext::LockRenderData( int nSizeInBytes )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->LockRenderData( nSizeInBytes );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->LockRenderData( nSizeInBytes );
	}
	std::unreachable();
}
void IMatRenderContext::UnlockRenderData( void *pData )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UnlockRenderData( pData ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UnlockRenderData( pData ); return;
	}
	std::unreachable();
}
void IMatRenderContext::AddRefRenderData()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->AddRefRenderData(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->AddRefRenderData(); return;
	}
	std::unreachable();
}
void IMatRenderContext::ReleaseRenderData()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ReleaseRenderData(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ReleaseRenderData(); return;
	}
	std::unreachable();
}
bool IMatRenderContext::IsRenderData( const void *pData ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->IsRenderData( pData );
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->IsRenderData( pData );
	}
	std::unreachable();
}
void IMatRenderContext::ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ReadPixelsAndStretch( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ReadPixelsAndStretch( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetWindowSize( int &width, int &height ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<const CMatRenderContext *>( this )->GetWindowSize( width, height ); return;
	case MatRenderContextKind::Queued: static_cast<const CMatQueuedRenderContext *>( this )->GetWindowSize( width, height ); return;
	}
	std::unreachable();
}
void IMatRenderContext::DrawScreenSpaceRectangle( IMaterial *pMaterial, int destx, int desty, int width, int height, float src_texture_x0, float src_texture_y0, float src_texture_x1, float src_texture_y1, int src_texture_width, int src_texture_height, void *pClientRenderable, int nXDice, int nYDice )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DrawScreenSpaceRectangle( pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0, src_texture_x1, src_texture_y1, src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DrawScreenSpaceRectangle( pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0, src_texture_x1, src_texture_y1, src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice ); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadBoneMatrix( boneIndex, matrix ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadBoneMatrix( boneIndex, matrix ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushRenderTargetAndViewport()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushRenderTargetAndViewport(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushRenderTargetAndViewport(); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushRenderTargetAndViewport( ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushRenderTargetAndViewport( pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushRenderTargetAndViewport( pTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushRenderTargetAndViewport( pTexture, nViewX, nViewY, nViewW, nViewH ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushRenderTargetAndViewport( pTexture, nViewX, nViewY, nViewW, nViewH ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushRenderTargetAndViewport( pTexture, pDepthTexture, nViewX, nViewY, nViewW, nViewH ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushRenderTargetAndViewport( pTexture, pDepthTexture, nViewX, nViewY, nViewW, nViewH ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopRenderTargetAndViewport()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopRenderTargetAndViewport(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopRenderTargetAndViewport(); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindLightmapTexture( ITexture *pLightmapTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindLightmapTexture( pLightmapTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindLightmapTexture( pLightmapTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->CopyRenderTargetToTextureEx( pTexture, nRenderTargetID, pSrcRect, pDstRect ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->CopyRenderTargetToTextureEx( pTexture, nRenderTargetID, pSrcRect, pDstRect ); return;
	}
	std::unreachable();
}
void IMatRenderContext::CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->CopyTextureToRenderTargetEx( nRenderTargetID, pTexture, pSrcRect, pDstRect ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->CopyTextureToRenderTargetEx( nRenderTargetID, pTexture, pSrcRect, pDstRect ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PerspectiveOffCenterX( fovx, aspect, zNear, zFar, bottom, top, left, right ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PerspectiveOffCenterX( fovx, aspect, zNear, zFar, bottom, top, left, right ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFloatRenderingParameter( int parm_number, float value )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFloatRenderingParameter( parm_number, value ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFloatRenderingParameter( parm_number, value ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetIntRenderingParameter( int parm_number, int value )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetIntRenderingParameter( parm_number, value ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetIntRenderingParameter( parm_number, value ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetVectorRenderingParameter( int parm_number, Vector const &value )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetVectorRenderingParameter( parm_number, value ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetVectorRenderingParameter( parm_number, value ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetStencilState( const ShaderStencilState_t &state )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetStencilState( state ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetStencilState( state ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearStencilBufferRectangle( int xmin, int ymin, int xmax, int ymax, int value )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearStencilBufferRectangle( xmin, ymin, xmax, ymax, value ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearStencilBufferRectangle( xmin, ymin, xmax, ymax, value ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetRenderTargetEx( nRenderTargetID, pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetRenderTargetEx( nRenderTargetID, pTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushCustomClipPlane( const float *pPlane )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushCustomClipPlane( pPlane ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushCustomClipPlane( pPlane ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopCustomClipPlane()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopCustomClipPlane(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopCustomClipPlane(); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices ); return;
	}
	std::unreachable();
}
int IMatRenderContext::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetMaxVerticesToRender( pMaterial );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetMaxVerticesToRender( pMaterial );
	}
	std::unreachable();
}
int IMatRenderContext::GetMaxIndicesToRender()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetMaxIndicesToRender();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetMaxIndicesToRender();
	}
	std::unreachable();
}
void IMatRenderContext::DisableAllLocalLights()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DisableAllLocalLights(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DisableAllLocalLights(); return;
	}
	std::unreachable();
}
int IMatRenderContext::CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightMapID1, int lightMapID2 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->CompareMaterialCombos( pMaterial1, pMaterial2, lightMapID1, lightMapID2 );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->CompareMaterialCombos( pMaterial1, pMaterial2, lightMapID1, lightMapID2 );
	}
	std::unreachable();
}
IMesh * IMatRenderContext::GetFlexMesh()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetFlexMesh();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetFlexMesh();
	}
	std::unreachable();
}
void IMatRenderContext::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFlashlightStateEx( state, worldToTexture, pFlashlightDepthTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFlashlightStateEx( state, worldToTexture, pFlashlightDepthTexture ); return;
	}
	std::unreachable();
}
ITexture * IMatRenderContext::GetLocalCubemap()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetLocalCubemap();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetLocalCubemap();
	}
	std::unreachable();
}
void IMatRenderContext::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearBuffersObeyStencil( bClearColor, bClearDepth ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearBuffersObeyStencil( bClearColor, bClearDepth ); return;
	}
	std::unreachable();
}
bool IMatRenderContext::EnableClipping( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->EnableClipping( bEnable );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->EnableClipping( bEnable );
	}
	std::unreachable();
}
void IMatRenderContext::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetFogDistances( fStart, fEnd, fFogZ ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetFogDistances( fStart, fEnd, fFogZ ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BeginPIXEvent( unsigned long color, const char *szName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginPIXEvent( color, szName ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginPIXEvent( color, szName ); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndPIXEvent()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndPIXEvent(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndPIXEvent(); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetPIXMarker( unsigned long color, const char *szName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetPIXMarker( color, szName ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetPIXMarker( color, szName ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BeginBatch( IMesh* pIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginBatch( pIndices ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginBatch( pIndices ); return;
	}
	std::unreachable();
}
void IMatRenderContext::BindBatch( IMesh* pVertices, IMaterial *pAutoBind )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindBatch( pVertices, pAutoBind ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindBatch( pVertices, pAutoBind ); return;
	}
	std::unreachable();
}
void IMatRenderContext::DrawBatch( MaterialPrimitiveType_t primType, int firstIndex, int numIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DrawBatch( primType, firstIndex, numIndices ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DrawBatch( primType, firstIndex, numIndices ); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndBatch()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndBatch(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndBatch(); return;
	}
	std::unreachable();
}
ICallQueue * IMatRenderContext::GetCallQueue()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetCallQueue();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetCallQueue();
	}
	std::unreachable();
}
void IMatRenderContext::GetWorldSpaceCameraPosition( Vector *pCameraPos )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetWorldSpaceCameraPosition( pCameraPos ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetWorldSpaceCameraPosition( pCameraPos ); return;
	}
	std::unreachable();
}
void IMatRenderContext::GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetWorldSpaceCameraVectors( pVecForward, pVecRight, pVecUp ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetWorldSpaceCameraVectors( pVecForward, pVecRight, pVecUp ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetToneMappingScaleLinear( const Vector &scale )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetToneMappingScaleLinear( scale ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetToneMappingScaleLinear( scale ); return;
	}
	std::unreachable();
}
Vector IMatRenderContext::GetToneMappingScaleLinear()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetToneMappingScaleLinear();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetToneMappingScaleLinear();
	}
	std::unreachable();
}
void IMatRenderContext::SetShadowDepthBiasFactors( float fSlopeScaleDepthBias, float fDepthBias )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetShadowDepthBiasFactors( fSlopeScaleDepthBias, fDepthBias ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetShadowDepthBiasFactors( fSlopeScaleDepthBias, fDepthBias ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PerformFullScreenStencilOperation()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PerformFullScreenStencilOperation(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PerformFullScreenStencilOperation(); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetLightingOrigin( Vector vLightingOrigin )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetLightingOrigin( vLightingOrigin ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetLightingOrigin( vLightingOrigin ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushScissorRect( nLeft, nTop, nRight, nBottom ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushScissorRect( nLeft, nTop, nRight, nBottom ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopScissorRect()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopScissorRect(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopScissorRect(); return;
	}
	std::unreachable();
}
void IMatRenderContext::BeginMorphAccumulation()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginMorphAccumulation(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginMorphAccumulation(); return;
	}
	std::unreachable();
}
void IMatRenderContext::EndMorphAccumulation()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndMorphAccumulation(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndMorphAccumulation(); return;
	}
	std::unreachable();
}
void IMatRenderContext::AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->AccumulateMorph( pMorph, nMorphCount, pWeights ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->AccumulateMorph( pMorph, nMorphCount, pWeights ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PushDeformation( DeformationBase_t const *Deformation )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PushDeformation( Deformation ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PushDeformation( Deformation ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PopDeformation()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PopDeformation(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PopDeformation(); return;
	}
	std::unreachable();
}
int IMatRenderContext::GetNumActiveDeformations() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetNumActiveDeformations();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetNumActiveDeformations();
	}
	std::unreachable();
}
bool IMatRenderContext::GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetMorphAccumulatorTexCoord( pTexCoord, pMorph, nVertex );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetMorphAccumulatorTexCoord( pTexCoord, pMorph, nVertex );
	}
	std::unreachable();
}
IMesh* IMatRenderContext::GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetDynamicMeshEx( vertexFormat, bBuffered, pVertexOverride, pIndexOverride, pAutoBind );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetDynamicMeshEx( vertexFormat, bBuffered, pVertexOverride, pIndexOverride, pAutoBind );
	}
	std::unreachable();
}
void IMatRenderContext::FogMaxDensity( float flMaxDensity )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FogMaxDensity( flMaxDensity ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FogMaxDensity( flMaxDensity ); return;
	}
	std::unreachable();
}
void IMatRenderContext::AntiAliasingHint( int nHint )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->AntiAliasingHint( nHint ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->AntiAliasingHint( nHint ); return;
	}
	std::unreachable();
}
IMaterial * IMatRenderContext::GetCurrentMaterial()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetCurrentMaterial();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetCurrentMaterial();
	}
	std::unreachable();
}
int IMatRenderContext::GetCurrentNumBones() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetCurrentNumBones();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetCurrentNumBones();
	}
	std::unreachable();
}
void * IMatRenderContext::GetCurrentProxy()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetCurrentProxy();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetCurrentProxy();
	}
	std::unreachable();
}
void IMatRenderContext::EnableColorCorrection( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EnableColorCorrection( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EnableColorCorrection( bEnable ); return;
	}
	std::unreachable();
}
ColorCorrectionHandle_t IMatRenderContext::AddLookup( const char *pName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->AddLookup( pName );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->AddLookup( pName );
	}
	std::unreachable();
}
bool IMatRenderContext::RemoveLookup( ColorCorrectionHandle_t handle )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->RemoveLookup( handle );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->RemoveLookup( handle );
	}
	std::unreachable();
}
void IMatRenderContext::LockLookup( ColorCorrectionHandle_t handle )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LockLookup( handle ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LockLookup( handle ); return;
	}
	std::unreachable();
}
void IMatRenderContext::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->LoadLookup( handle, pLookupName ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->LoadLookup( handle, pLookupName ); return;
	}
	std::unreachable();
}
void IMatRenderContext::UnlockLookup( ColorCorrectionHandle_t handle )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UnlockLookup( handle ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UnlockLookup( handle ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetLookupWeight( ColorCorrectionHandle_t handle, float flWeight )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetLookupWeight( handle, flWeight ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetLookupWeight( handle, flWeight ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ResetLookupWeights()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ResetLookupWeights(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ResetLookupWeights(); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetResetable( ColorCorrectionHandle_t handle, bool bResetable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetResetable( handle, bResetable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetResetable( handle, bResetable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetFullScreenDepthTextureValidityFlag( bool bIsValid )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFullScreenDepthTextureValidityFlag( bIsValid ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFullScreenDepthTextureValidityFlag( bIsValid ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetNonInteractiveLogoTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedW, float flNormalizedH )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetNonInteractiveLogoTexture( pTexture, flNormalizedX, flNormalizedY, flNormalizedW, flNormalizedH ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetNonInteractiveLogoTexture( pTexture, flNormalizedX, flNormalizedY, flNormalizedW, flNormalizedH ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetNonInteractivePacifierTexture( pTexture, flNormalizedX, flNormalizedY, flNormalizedSize ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetNonInteractivePacifierTexture( pTexture, flNormalizedX, flNormalizedY, flNormalizedSize ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetNonInteractiveTempFullscreenBuffer( pTexture, mode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetNonInteractiveTempFullscreenBuffer( pTexture, mode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EnableNonInteractiveMode( mode ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EnableNonInteractiveMode( mode ); return;
	}
	std::unreachable();
}
void IMatRenderContext::RefreshFrontBufferNonInteractive()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->RefreshFrontBufferNonInteractive(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->RefreshFrontBufferNonInteractive(); return;
	}
	std::unreachable();
}
void IMatRenderContext::FlipCulling( bool bFlipCulling )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->FlipCulling( bFlipCulling ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->FlipCulling( bFlipCulling ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetTextureRenderingParameter( int parm_number, ITexture *pTexture )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetTextureRenderingParameter( parm_number, pTexture ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetTextureRenderingParameter( parm_number, pTexture ); return;
	}
	std::unreachable();
}
void IMatRenderContext::EnableSinglePassFlashlightMode( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EnableSinglePassFlashlightMode( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EnableSinglePassFlashlightMode( bEnable ); return;
	}
	std::unreachable();
}
bool IMatRenderContext::SinglePassFlashlightModeEnabled() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->SinglePassFlashlightModeEnabled();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->SinglePassFlashlightModeEnabled();
	}
	std::unreachable();
}
void IMatRenderContext::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DrawInstances( nInstanceCount, pInstance ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DrawInstances( nInstanceCount, pInstance ); return;
	}
	std::unreachable();
}
void IMatRenderContext::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->OverrideAlphaWriteEnable( bOverrideEnable, bAlphaWriteEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->OverrideAlphaWriteEnable( bOverrideEnable, bAlphaWriteEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->OverrideColorWriteEnable( bOverrideEnable, bColorWriteEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->OverrideColorWriteEnable( bOverrideEnable, bColorWriteEnable ); return;
	}
	std::unreachable();
}
void IMatRenderContext::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ClearBuffersObeyStencilEx( bClearColor, bClearAlpha, bClearDepth ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ClearBuffersObeyStencilEx( bClearColor, bClearAlpha, bClearDepth ); return;
	}
	std::unreachable();
}
int IMatRenderContext::GetSubDBufferWidth()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetSubDBufferWidth();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetSubDBufferWidth();
	}
	std::unreachable();
}
float* IMatRenderContext::LockSubDBuffer( int nNumRows )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->LockSubDBuffer( nNumRows );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->LockSubDBuffer( nNumRows );
	}
	std::unreachable();
}
void IMatRenderContext::UnlockSubDBuffer()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UnlockSubDBuffer(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UnlockSubDBuffer(); return;
	}
	std::unreachable();
}
void IMatRenderContext::UpdateGameTime( float flTime )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UpdateGameTime( flTime ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UpdateGameTime( flTime ); return;
	}
	std::unreachable();
}
void IMatRenderContext::PrintfVA( char *fmt, std::va_list vargs )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->PrintfVA( fmt, vargs ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->PrintfVA( fmt, vargs ); return;
	}
	std::unreachable();
}
void IMatRenderContext::Printf( char *fmt, ... )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->Printf( fmt ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->Printf( fmt ); return;
	}
	std::unreachable();
}
float IMatRenderContext::Knob( char *knobname, float *setvalue )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->Knob( knobname, setvalue );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->Knob( knobname, setvalue );
	}
	std::unreachable();
}
void IMatRenderContext::RenderRocketHUD( void *cmdList )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->RenderRocketHUD( cmdList ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->RenderRocketHUD( cmdList ); return;
	}
	std::unreachable();
}
void IMatRenderContext::RenderRocketMenu( void *cmdList )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->RenderRocketMenu( cmdList ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->RenderRocketMenu( cmdList ); return;
	}
	std::unreachable();
}
void IMatRenderContext::SetRenderingPaint( bool bEnable )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetRenderingPaint( bEnable ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetRenderingPaint( bEnable ); return;
	}
	std::unreachable();
}
ColorCorrectionHandle_t IMatRenderContext::FindLookup( const char *pName )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->FindLookup( pName );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->FindLookup( pName );
	}
	std::unreachable();
}
