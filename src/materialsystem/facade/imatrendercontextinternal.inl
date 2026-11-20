import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vector4d;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatRenderContextInternal class to the render context implementations, selected by the kind
// the context was created with. Included by cmatrendercontextdispatch.cpp.

void IMatRenderContextInternal::UpdateHeightClipUserClipPlane()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->UpdateHeightClipUserClipPlane(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->UpdateHeightClipUserClipPlane(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::ApplyCustomClipPlanes()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ApplyCustomClipPlanes(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ApplyCustomClipPlanes(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::CommitRenderTargetAndViewport()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->CommitRenderTargetAndViewport(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->CommitRenderTargetAndViewport(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::OnRenderDataUnreferenced()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->OnRenderDataUnreferenced(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->OnRenderDataUnreferenced(); return;
	}
	std::unreachable();
}
float IMatRenderContextInternal::GetFloatRenderingParameter( int parm_number ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetFloatRenderingParameter( parm_number );
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetFloatRenderingParameter( parm_number );
	}
	std::unreachable();
}
int IMatRenderContextInternal::GetIntRenderingParameter( int parm_number ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetIntRenderingParameter( parm_number );
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetIntRenderingParameter( parm_number );
	}
	std::unreachable();
}
ITexture * IMatRenderContextInternal::GetTextureRenderingParameter( int parm_number ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetTextureRenderingParameter( parm_number );
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetTextureRenderingParameter( parm_number );
	}
	std::unreachable();
}
Vector IMatRenderContextInternal::GetVectorRenderingParameter( int parm_number ) const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetVectorRenderingParameter( parm_number );
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetVectorRenderingParameter( parm_number );
	}
	std::unreachable();
}
void IMatRenderContextInternal::SwapBuffers()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SwapBuffers(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SwapBuffers(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::SetCurrentMaterialInternal( IMaterialInternal* pCurrentMaterial )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetCurrentMaterialInternal( pCurrentMaterial ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetCurrentMaterialInternal( pCurrentMaterial ); return;
	}
	std::unreachable();
}
IMaterialInternal* IMatRenderContextInternal::GetCurrentMaterialInternal() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->GetCurrentMaterialInternal();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->GetCurrentMaterialInternal();
	}
	std::unreachable();
}
int IMatRenderContextInternal::GetLightmapPage()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetLightmapPage();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetLightmapPage();
	}
	std::unreachable();
}
void IMatRenderContextInternal::ForceDepthFuncEquals( bool arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ForceDepthFuncEquals( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ForceDepthFuncEquals( arg0 ); return;
	}
	std::unreachable();
}
bool IMatRenderContextInternal::InFlashlightMode() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->InFlashlightMode();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->InFlashlightMode();
	}
	std::unreachable();
}
bool IMatRenderContextInternal::IsCascadedShadowMapping() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->IsCascadedShadowMapping();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->IsCascadedShadowMapping();
	}
	std::unreachable();
}
void IMatRenderContextInternal::BindStandardTexture( Sampler_t arg0, TextureBindFlags_t nBindFlags, StandardTextureId_t arg2 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BindStandardTexture( arg0, nBindFlags, arg2 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BindStandardTexture( arg0, nBindFlags, arg2 ); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::GetLightmapDimensions( int * arg0, int * arg1 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->GetLightmapDimensions( arg0, arg1 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->GetLightmapDimensions( arg0, arg1 ); return;
	}
	std::unreachable();
}
MorphFormat_t IMatRenderContextInternal::GetBoundMorphFormat()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetBoundMorphFormat();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetBoundMorphFormat();
	}
	std::unreachable();
}
ITexture * IMatRenderContextInternal::GetRenderTargetEx( int arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetRenderTargetEx( arg0 );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetRenderTargetEx( arg0 );
	}
	std::unreachable();
}
void IMatRenderContextInternal::DrawClearBufferQuad( unsigned char arg0, unsigned char arg1, unsigned char arg2, unsigned char arg3, bool arg4, bool arg5, bool arg6 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->DrawClearBufferQuad( arg0, arg1, arg2, arg3, arg4, arg5, arg6 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->DrawClearBufferQuad( arg0, arg1, arg2, arg3, arg4, arg5, arg6 ); return;
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnDrawMesh( pMesh, firstIndex, numIndices );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnDrawMesh( pMesh, firstIndex, numIndices );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnDrawMesh( pMesh, pLists, nLists );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnDrawMesh( pMesh, pLists, nLists );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnDrawMeshModulated( pMesh, diffuseModulation, firstIndex, numIndices );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnDrawMeshModulated( pMesh, diffuseModulation, firstIndex, numIndices );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnSetFlexMesh( pStaticMesh, pMesh, nVertexOffsetInBytes );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnSetFlexMesh( pStaticMesh, pMesh, nVertexOffsetInBytes );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnSetColorMesh( pStaticMesh, pMesh, nVertexOffsetInBytes );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnSetColorMesh( pStaticMesh, pMesh, nVertexOffsetInBytes );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->OnSetPrimitiveType( pMesh, type );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->OnSetPrimitiveType( pMesh, type );
	}
	std::unreachable();
}
void IMatRenderContextInternal::SyncMatrices()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SyncMatrices(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SyncMatrices(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::SyncMatrix( MaterialMatrixMode_t arg0 )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SyncMatrix( arg0 ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SyncMatrix( arg0 ); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::ForceHardwareSync()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->ForceHardwareSync(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->ForceHardwareSync(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::BeginFrame()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->BeginFrame(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->BeginFrame(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::EndFrame()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EndFrame(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EndFrame(); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::SetFrameTime( float frameTime )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetFrameTime( frameTime ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetFrameTime( frameTime ); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::SetCurrentProxy( void *pProxy )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->SetCurrentProxy( pProxy ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->SetCurrentProxy( pProxy ); return;
	}
	std::unreachable();
}
void IMatRenderContextInternal::MarkRenderDataUnused( bool bBeginFrame )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->MarkRenderDataUnused( bBeginFrame ); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->MarkRenderDataUnused( bBeginFrame ); return;
	}
	std::unreachable();
}
CMatCallQueue * IMatRenderContextInternal::GetCallQueueInternal()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetCallQueueInternal();
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetCallQueueInternal();
	}
	std::unreachable();
}
void IMatRenderContextInternal::EvictManagedResources()
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: static_cast<CMatRenderContext *>( this )->EvictManagedResources(); return;
	case MatRenderContextKind::Queued: static_cast<CMatQueuedRenderContext *>( this )->EvictManagedResources(); return;
	}
	std::unreachable();
}
ShaderAPITextureHandle_t IMatRenderContextInternal::GetLightmapTexture( int nLightmapPage )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetLightmapTexture( nLightmapPage );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetLightmapTexture( nLightmapPage );
	}
	std::unreachable();
}
bool IMatRenderContextInternal::IsRenderingPaint() const
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<const CMatRenderContext *>( this )->IsRenderingPaint();
	case MatRenderContextKind::Queued: return static_cast<const CMatQueuedRenderContext *>( this )->IsRenderingPaint();
	}
	std::unreachable();
}
ShaderAPITextureHandle_t IMatRenderContextInternal::GetPaintmapTexture( int nLightmapPage )
{
	switch ( static_cast<const IMatRenderContextInternal *>( this )->Kind() )
	{
	case MatRenderContextKind::Hardware: return static_cast<CMatRenderContext *>( this )->GetPaintmapTexture( nLightmapPage );
	case MatRenderContextKind::Queued: return static_cast<CMatQueuedRenderContext *>( this )->GetPaintmapTexture( nLightmapPage );
	}
	std::unreachable();
}
