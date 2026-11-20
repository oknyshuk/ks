import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVModelRender class to CModelRender. Included by l_studio.cpp.

int IVModelRender::DrawModel( int flags, IClientRenderable *pRenderable, ModelInstanceHandle_t instance, int entity_index, const model_t *model, Vector const& origin, QAngle const& angles, int skin, int body, int hitboxset, const matrix3x4_t *modelToWorld, const matrix3x4_t *pLightingOffset )
{ return static_cast<CModelRender*>(this)->DrawModel( flags, pRenderable, instance, entity_index, model, origin, angles, skin, body, hitboxset, modelToWorld, pLightingOffset ); }
void IVModelRender::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType, int nMaterialIndex )
{ static_cast<CModelRender*>(this)->ForcedMaterialOverride( newMaterial, nOverrideType, nMaterialIndex ); }
bool IVModelRender::IsForcedMaterialOverride()
{ return static_cast<CModelRender*>(this)->IsForcedMaterialOverride(); }
void IVModelRender::SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target )
{ static_cast<CModelRender*>(this)->SetViewTarget( pStudioHdr, nBodyIndex, target ); }
ModelInstanceHandle_t IVModelRender::CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t *pCache )
{ return static_cast<CModelRender*>(this)->CreateInstance( pRenderable, pCache ); }
void IVModelRender::DestroyInstance( ModelInstanceHandle_t handle )
{ static_cast<CModelRender*>(this)->DestroyInstance( handle ); }
void IVModelRender::SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t* pHandle )
{ static_cast<CModelRender*>(this)->SetStaticLighting( handle, pHandle ); }
LightCacheHandle_t IVModelRender::GetStaticLighting( ModelInstanceHandle_t handle )
{ return static_cast<CModelRender*>(this)->GetStaticLighting( handle ); }
bool IVModelRender::ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable )
{ return static_cast<CModelRender*>(this)->ChangeInstance( handle, pRenderable ); }
void IVModelRender::AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray, Vector const& decalUp, int decalIndex, int body, bool noPokeThru, int maxLODToDecal, IMaterial *pSpecifyMaterial, float w, float h, void *pvProxyUserData, int nAdditionalDecalFlags )
{ static_cast<CModelRender*>(this)->AddDecal( handle, ray, decalUp, decalIndex, body, noPokeThru, maxLODToDecal, pSpecifyMaterial, w, h, pvProxyUserData, nAdditionalDecalFlags ); }
void IVModelRender::RemoveAllDecals( ModelInstanceHandle_t handle )
{ static_cast<CModelRender*>(this)->RemoveAllDecals( handle ); }
bool IVModelRender::ModelHasDecals( ModelInstanceHandle_t handle )
{ return static_cast<CModelRender*>(this)->ModelHasDecals( handle ); }
void IVModelRender::RemoveAllDecalsFromAllModels( bool bRenderContextValid )
{ static_cast<CModelRender*>(this)->RemoveAllDecalsFromAllModels( bRenderContextValid ); }
matrix3x4a_t* IVModelRender::DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4a_t *pCustomBoneToWorld )
{ return static_cast<CModelRender*>(this)->DrawModelShadowSetup( pRenderable, body, skin, pInfo, pCustomBoneToWorld ); }
void IVModelRender::DrawModelShadow( IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4a_t *pCustomBoneToWorld )
{ static_cast<CModelRender*>(this)->DrawModelShadow( pRenderable, info, pCustomBoneToWorld ); }
bool IVModelRender::RecomputeStaticLighting( ModelInstanceHandle_t handle )
{ return static_cast<CModelRender*>(this)->RecomputeStaticLighting( handle ); }
void IVModelRender::ReleaseAllStaticPropColorData( void )
{ static_cast<CModelRender*>(this)->ReleaseAllStaticPropColorData(); }
void IVModelRender::RestoreAllStaticPropColorData( void )
{ static_cast<CModelRender*>(this)->RestoreAllStaticPropColorData(); }
int IVModelRender::DrawModelEx( ModelRenderInfo_t &pInfo )
{ return static_cast<CModelRender*>(this)->DrawModelEx( pInfo ); }
int IVModelRender::DrawModelExStaticProp( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo )
{ return static_cast<CModelRender*>(this)->DrawModelExStaticProp( pRenderContext, pInfo ); }
bool IVModelRender::DrawModelSetup( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t **ppBoneToWorldOut )
{ return static_cast<CModelRender*>(this)->DrawModelSetup( pRenderContext, pInfo, pState, ppBoneToWorldOut ); }
void IVModelRender::DrawModelExecute( IMatRenderContext *pRenderContext, const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pCustomBoneToWorld )
{ static_cast<CModelRender*>(this)->DrawModelExecute( pRenderContext, state, pInfo, pCustomBoneToWorld ); }
void IVModelRender::SetupLighting( const Vector &vecCenter )
{ static_cast<CModelRender*>(this)->SetupLighting( vecCenter ); }
int IVModelRender::DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth )
{ return static_cast<CModelRender*>(this)->DrawStaticPropArrayFast( pProps, count, bShadowDepth ); }
void IVModelRender::SuppressEngineLighting( bool bSuppress )
{ static_cast<CModelRender*>(this)->SuppressEngineLighting( bSuppress ); }
void IVModelRender::SetupColorMeshes( int nTotalVerts )
{ static_cast<CModelRender*>(this)->SetupColorMeshes( nTotalVerts ); }
void IVModelRender::SetupLightingEx( const Vector &vecCenter, ModelInstanceHandle_t handle )
{ static_cast<CModelRender*>(this)->SetupLightingEx( vecCenter, handle ); }
bool IVModelRender::GetBrightestShadowingLightSource( const Vector &vecCenter, Vector& lightPos, Vector& lightBrightness, bool bAllowNonTaggedLights )
{ return static_cast<CModelRender*>(this)->GetBrightestShadowingLightSource( vecCenter, lightPos, lightBrightness, bAllowNonTaggedLights ); }
void IVModelRender::ComputeLightingState( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState, ITexture **ppEnvCubemapTexture )
{ static_cast<CModelRender*>(this)->ComputeLightingState( nCount, pQuery, pState, ppEnvCubemapTexture ); }
void IVModelRender::GetModelDecalHandles( StudioDecalHandle_t *pDecals, int nDecalStride, int nCount, const ModelInstanceHandle_t *pHandles )
{ static_cast<CModelRender*>(this)->GetModelDecalHandles( pDecals, nDecalStride, nCount, pHandles ); }
void IVModelRender::ComputeStaticLightingState( int nCount, const StaticLightingQuery_t *pQuery, MaterialLightingState_t *pState, MaterialLightingState_t *pDecalState, ColorMeshInfo_t **ppStaticLighting, ITexture **ppEnvCubemapTexture, DataCacheHandle_t *pColorMeshHandles )
{ static_cast<CModelRender*>(this)->ComputeStaticLightingState( nCount, pQuery, pState, pDecalState, ppStaticLighting, ppEnvCubemapTexture, pColorMeshHandles ); }
void IVModelRender::CleanupStaticLightingState( int nCount, DataCacheHandle_t *pColorMeshHandles )
{ static_cast<CModelRender*>(this)->CleanupStaticLightingState( nCount, pColorMeshHandles ); }

