import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShadowMgr class to CShadowMgr. Included by shadowmgr.cpp.

ShadowHandle_t IShadowMgr::CreateShadow( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags )
{ return static_cast<CShadowMgr*>(this)->CreateShadow( pMaterial, pModelMaterial, pBindProxy, creationFlags ); }
void IShadowMgr::DestroyShadow( ShadowHandle_t handle )
{ static_cast<CShadowMgr*>(this)->DestroyShadow( handle ); }
void IShadowMgr::SetShadowMaterial( ShadowHandle_t handle, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy )
{ static_cast<CShadowMgr*>(this)->SetShadowMaterial( handle, pMaterial, pModelMaterial, pBindProxy ); }
void IShadowMgr::ProjectShadow( ShadowHandle_t handle, const Vector &origin, const Vector& projectionDir, const VMatrix& worldToShadow, const Vector2D& size, int nLeafCount, const int *pLeafList, float maxHeight, float falloffOffset, float falloffAmount, const Vector &vecCasterOrigin )
{ static_cast<CShadowMgr*>(this)->ProjectShadow( handle, origin, projectionDir, worldToShadow, size, nLeafCount, pLeafList, maxHeight, falloffOffset, falloffAmount, vecCasterOrigin ); }
void IShadowMgr::ProjectFlashlight( ShadowHandle_t handle, const VMatrix &worldToShadow, int nLeafCount, const int *pLeafList )
{ static_cast<CShadowMgr*>(this)->ProjectFlashlight( handle, worldToShadow, nLeafCount, pLeafList ); }
const ShadowInfo_t & IShadowMgr::GetInfo( ShadowHandle_t handle )
{ return static_cast<CShadowMgr*>(this)->GetInfo( handle ); }
const Frustum_t & IShadowMgr::GetFlashlightFrustum( ShadowHandle_t handle )
{ return static_cast<CShadowMgr*>(this)->GetFlashlightFrustum( handle ); }
void IShadowMgr::AddShadowToBrushModel( ShadowHandle_t handle, model_t* pModel, const Vector& origin, const QAngle& angles )
{ static_cast<CShadowMgr*>(this)->AddShadowToBrushModel( handle, pModel, origin, angles ); }
void IShadowMgr::RemoveAllShadowsFromBrushModel( model_t* pModel )
{ static_cast<CShadowMgr*>(this)->RemoveAllShadowsFromBrushModel( pModel ); }
void IShadowMgr::SetShadowTexCoord( ShadowHandle_t handle, float x, float y, float w, float h )
{ static_cast<CShadowMgr*>(this)->SetShadowTexCoord( handle, x, y, w, h ); }
void IShadowMgr::AddShadowToModel( ShadowHandle_t shadow, ModelInstanceHandle_t instance )
{ static_cast<CShadowMgr*>(this)->AddShadowToModel( shadow, instance ); }
void IShadowMgr::RemoveAllShadowsFromModel( ModelInstanceHandle_t instance )
{ static_cast<CShadowMgr*>(this)->RemoveAllShadowsFromModel( instance ); }
void IShadowMgr::ClearExtraClipPlanes( ShadowHandle_t shadow )
{ static_cast<CShadowMgr*>(this)->ClearExtraClipPlanes( shadow ); }
void IShadowMgr::AddExtraClipPlane( ShadowHandle_t shadow, const Vector& normal, float dist )
{ static_cast<CShadowMgr*>(this)->AddExtraClipPlane( shadow, normal, dist ); }
void IShadowMgr::EnableShadow( ShadowHandle_t shadow, bool bEnable )
{ static_cast<CShadowMgr*>(this)->EnableShadow( shadow, bEnable ); }
void IShadowMgr::SetFalloffBias( ShadowHandle_t shadow, unsigned char ucBias )
{ static_cast<CShadowMgr*>(this)->SetFalloffBias( shadow, ucBias ); }
void IShadowMgr::UpdateFlashlightState( ShadowHandle_t shadowHandle, const FlashlightState_t &lightState )
{ static_cast<CShadowMgr*>(this)->UpdateFlashlightState( shadowHandle, lightState ); }
void IShadowMgr::DrawFlashlightDepthTexture()
{ static_cast<CShadowMgr*>(this)->DrawFlashlightDepthTexture(); }
ShadowHandle_t IShadowMgr::CreateShadowEx( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags, int nEntIndex )
{ return static_cast<CShadowMgr*>(this)->CreateShadowEx( pMaterial, pModelMaterial, pBindProxy, creationFlags, nEntIndex ); }
void IShadowMgr::SetFlashlightDepthTexture( ShadowHandle_t shadowHandle, ITexture *pFlashlightDepthTexture, unsigned char ucShadowStencilBit )
{ static_cast<CShadowMgr*>(this)->SetFlashlightDepthTexture( shadowHandle, pFlashlightDepthTexture, ucShadowStencilBit ); }
const FlashlightState_t & IShadowMgr::GetFlashlightState( ShadowHandle_t handle )
{ return static_cast<CShadowMgr*>(this)->GetFlashlightState( handle ); }
void IShadowMgr::SetFlashlightRenderState( ShadowHandle_t handle )
{ static_cast<CShadowMgr*>(this)->SetFlashlightRenderState( handle ); }
void IShadowMgr::DrawVolumetrics()
{ static_cast<CShadowMgr*>(this)->DrawVolumetrics(); }
int IShadowMgr::GetNumShadowsOnModel( ModelInstanceHandle_t instance )
{ return static_cast<CShadowMgr*>(this)->GetNumShadowsOnModel( instance ); }
int IShadowMgr::GetShadowsOnModel( ModelInstanceHandle_t instance, ShadowHandle_t* pShadowArray, bool bNormalShadows, bool bFlashlightShadows )
{ return static_cast<CShadowMgr*>(this)->GetShadowsOnModel( instance, pShadowArray, bNormalShadows, bFlashlightShadows ); }
void IShadowMgr::FlashlightDrawCallback( ShadowDrawCallbackFn_t pCallback, void *pData )
{ static_cast<CShadowMgr*>(this)->FlashlightDrawCallback( pCallback, pData ); }
void IShadowMgr::SetSinglePassFlashlightRenderState( ShadowHandle_t handle )
{ static_cast<CShadowMgr*>(this)->SetSinglePassFlashlightRenderState( handle ); }
void IShadowMgr::PushSinglePassFlashlightStateEnabled( bool bEnable )
{ static_cast<CShadowMgr*>(this)->PushSinglePassFlashlightStateEnabled( bEnable ); }
void IShadowMgr::PopSinglePassFlashlightStateEnabled( void )
{ static_cast<CShadowMgr*>(this)->PopSinglePassFlashlightStateEnabled(); }
bool IShadowMgr::SinglePassFlashlightModeEnabled( void )
{ return static_cast<CShadowMgr*>(this)->SinglePassFlashlightModeEnabled(); }
int IShadowMgr::SetupFlashlightRenderInstanceInfo( ShadowHandle_t *pUniqueFlashlights, uint32 *pModelUsageMask, int nUsageStride, int nInstanceCount, const ModelInstanceHandle_t *pInstance )
{ return static_cast<CShadowMgr*>(this)->SetupFlashlightRenderInstanceInfo( pUniqueFlashlights, pModelUsageMask, nUsageStride, nInstanceCount, pInstance ); }
void IShadowMgr::GetFlashlightRenderInfo( FlashlightInstance_t *pFlashlightState, int nCount, const ShadowHandle_t *pHandles )
{ static_cast<CShadowMgr*>(this)->GetFlashlightRenderInfo( pFlashlightState, nCount, pHandles ); }
void IShadowMgr::RemoveAllDecalsFromShadow( ShadowHandle_t handle )
{ static_cast<CShadowMgr*>(this)->RemoveAllDecalsFromShadow( handle ); }
void IShadowMgr::SkipShadowForEntity( int nEntIndex )
{ static_cast<CShadowMgr*>(this)->SkipShadowForEntity( nEntIndex ); }
void IShadowMgr::PushFlashlightScissorBounds( void )
{ static_cast<CShadowMgr*>(this)->PushFlashlightScissorBounds(); }
void IShadowMgr::PopFlashlightScissorBounds( void )
{ static_cast<CShadowMgr*>(this)->PopFlashlightScissorBounds(); }
void IShadowMgr::DisableDropShadows()
{ static_cast<CShadowMgr*>(this)->DisableDropShadows(); }

void IShadowMgrInternal::LevelInit( int nSurfCount )
{ static_cast<CShadowMgr*>(this)->LevelInit( nSurfCount ); }
void IShadowMgrInternal::LevelShutdown()
{ static_cast<CShadowMgr*>(this)->LevelShutdown(); }
void IShadowMgrInternal::AddShadowsOnSurfaceToRenderList( ShadowDecalHandle_t decalHandle )
{ static_cast<CShadowMgr*>(this)->AddShadowsOnSurfaceToRenderList( decalHandle ); }
void IShadowMgrInternal::RenderProjectedTextures( IMatRenderContext *pRenderContext, VMatrix const* pModelToWorld )
{ static_cast<CShadowMgr*>(this)->RenderProjectedTextures( pRenderContext, pModelToWorld ); }
void IShadowMgrInternal::RenderShadows( IMatRenderContext *pRenderContext, VMatrix const* pModelToWorld )
{ static_cast<CShadowMgr*>(this)->RenderShadows( pRenderContext, pModelToWorld ); }
void IShadowMgrInternal::RenderFlashlights( bool bDoMasking, bool bDoSimpleProjections, VMatrix const* pModelToWorld )
{ static_cast<CShadowMgr*>(this)->RenderFlashlights( bDoMasking, bDoSimpleProjections, pModelToWorld ); }
void IShadowMgrInternal::ClearShadowRenderList()
{ static_cast<CShadowMgr*>(this)->ClearShadowRenderList(); }
int IShadowMgrInternal::ProjectAndClipVertices( ShadowHandle_t handle, int count, Vector** ppPosition, ShadowVertex_t*** ppOutVertex )
{ return static_cast<CShadowMgr*>(this)->ProjectAndClipVertices( handle, count, ppPosition, ppOutVertex ); }
void IShadowMgrInternal::ComputeRenderInfo( ShadowDecalRenderInfo_t* pInfo, ShadowHandle_t handle ) const
{ static_cast<const CShadowMgr*>(this)->ComputeRenderInfo( pInfo, handle ); }
unsigned short IShadowMgrInternal::InvalidShadowIndex()
{ return static_cast<CShadowMgr*>(this)->InvalidShadowIndex(); }
void IShadowMgrInternal::SetModelShadowState( ModelInstanceHandle_t instance )
{ static_cast<CShadowMgr*>(this)->SetModelShadowState( instance ); }
void IShadowMgrInternal::SetNumWorldMaterialBuckets( int numMaterialSortBins )
{ static_cast<CShadowMgr*>(this)->SetNumWorldMaterialBuckets( numMaterialSortBins ); }
void IShadowMgrInternal::DrawFlashlightDecals( IMatRenderContext *pRenderContext, int sortGroup, bool bDoMasking, float flFade )
{ static_cast<CShadowMgr*>(this)->DrawFlashlightDecals( pRenderContext, sortGroup, bDoMasking, flFade ); }
void IShadowMgrInternal::DrawFlashlightDecalsOnSurfaceList( IMatRenderContext *pRenderContext, SurfaceHandle_t *pList, int listCount, bool bDoMasking )
{ static_cast<CShadowMgr*>(this)->DrawFlashlightDecalsOnSurfaceList( pRenderContext, pList, listCount, bDoMasking ); }
void IShadowMgrInternal::DrawFlashlightOverlays( IMatRenderContext *pRenderContext, int nSortGroup, bool bDoMasking )
{ static_cast<CShadowMgr*>(this)->DrawFlashlightOverlays( pRenderContext, nSortGroup, bDoMasking ); }
void IShadowMgrInternal::DrawFlashlightDepthTexture()
{ static_cast<CShadowMgr*>(this)->DrawFlashlightDepthTexture(); }
void IShadowMgrInternal::DrawFlashlightDecalsOnDisplacements( IMatRenderContext *pRenderContext, int sortGroup, CDispInfo **visibleDisps, int nVisibleDisps, bool bDoMasking )
{ static_cast<CShadowMgr*>(this)->DrawFlashlightDecalsOnDisplacements( pRenderContext, sortGroup, visibleDisps, nVisibleDisps, bDoMasking ); }
void IShadowMgrInternal::SetFlashlightStencilMasks( bool bDoMasking )
{ static_cast<CShadowMgr*>(this)->SetFlashlightStencilMasks( bDoMasking ); }
bool IShadowMgrInternal::ModelHasShadows( ModelInstanceHandle_t instance )
{ return static_cast<CShadowMgr*>(this)->ModelHasShadows( instance ); }
int IShadowMgrInternal::ProjectAndClipVerticesEx( ShadowHandle_t handle, int count, Vector** ppPosition, ShadowVertex_t*** ppOutVertex, ShadowClipState_t& clip )
{ return static_cast<CShadowMgr*>(this)->ProjectAndClipVerticesEx( handle, count, ppPosition, ppOutVertex, clip ); }

