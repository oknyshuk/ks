import ks.math.compressed_vector;
import ks.math.lightdesc;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IStudioRender class to CStudioRenderContext. Included by studiorendercontext.cpp.

void IStudioRender::BeginFrame( void )
{ static_cast<CStudioRenderContext*>(this)->BeginFrame(); }
void IStudioRender::EndFrame( void )
{ static_cast<CStudioRenderContext*>(this)->EndFrame(); }
void IStudioRender::UpdateConfig( const StudioRenderConfig_t& config )
{ static_cast<CStudioRenderContext*>(this)->UpdateConfig( config ); }
void IStudioRender::GetCurrentConfig( StudioRenderConfig_t& config )
{ static_cast<CStudioRenderContext*>(this)->GetCurrentConfig( config ); }
bool IStudioRender::LoadModel( studiohdr_t *pStudioHdr, void *pVtxData, studiohwdata_t *pHardwareData )
{ return static_cast<CStudioRenderContext*>(this)->LoadModel( pStudioHdr, pVtxData, pHardwareData ); }
void IStudioRender::UnloadModel( studiohwdata_t *pHardwareData )
{ static_cast<CStudioRenderContext*>(this)->UnloadModel( pHardwareData ); }
void IStudioRender::RefreshStudioHdr( studiohdr_t* pStudioHdr, studiohwdata_t* pHardwareData )
{ static_cast<CStudioRenderContext*>(this)->RefreshStudioHdr( pStudioHdr, pHardwareData ); }
void IStudioRender::SetEyeViewTarget( const studiohdr_t *pStudioHdr, int nBodyIndex, const Vector& worldPosition )
{ static_cast<CStudioRenderContext*>(this)->SetEyeViewTarget( pStudioHdr, nBodyIndex, worldPosition ); }
int IStudioRender::GetNumAmbientLightSamples()
{ return static_cast<CStudioRenderContext*>(this)->GetNumAmbientLightSamples(); }
const Vector * IStudioRender::GetAmbientLightDirections()
{ return static_cast<CStudioRenderContext*>(this)->GetAmbientLightDirections(); }
void IStudioRender::SetAmbientLightColors( const Vector4D *pAmbientOnlyColors )
{ static_cast<CStudioRenderContext*>(this)->SetAmbientLightColors( pAmbientOnlyColors ); }
void IStudioRender::SetAmbientLightColors( const Vector *pAmbientOnlyColors )
{ static_cast<CStudioRenderContext*>(this)->SetAmbientLightColors( pAmbientOnlyColors ); }
void IStudioRender::SetLocalLights( int numLights, const LightDesc_t *pLights )
{ static_cast<CStudioRenderContext*>(this)->SetLocalLights( numLights, pLights ); }
void IStudioRender::SetViewState( const Vector& viewOrigin, const Vector& viewRight, const Vector& viewUp, const Vector& viewPlaneNormal )
{ static_cast<CStudioRenderContext*>(this)->SetViewState( viewOrigin, viewRight, viewUp, viewPlaneNormal ); }
int IStudioRender::GetNumLODs( const studiohwdata_t &hardwareData ) const
{ return static_cast<const CStudioRenderContext*>(this)->GetNumLODs( hardwareData ); }
float IStudioRender::GetLODSwitchValue( const studiohwdata_t &hardwareData, int lod ) const
{ return static_cast<const CStudioRenderContext*>(this)->GetLODSwitchValue( hardwareData, lod ); }
void IStudioRender::SetLODSwitchValue( studiohwdata_t &hardwareData, int lod, float switchValue )
{ static_cast<CStudioRenderContext*>(this)->SetLODSwitchValue( hardwareData, lod, switchValue ); }
void IStudioRender::SetColorModulation( float const* pColor )
{ static_cast<CStudioRenderContext*>(this)->SetColorModulation( pColor ); }
void IStudioRender::SetAlphaModulation( float flAlpha )
{ static_cast<CStudioRenderContext*>(this)->SetAlphaModulation( flAlpha ); }
void IStudioRender::DrawModel( DrawModelResults_t *pResults, const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, float *pFlexWeights, float *pFlexDelayedWeights, const Vector &modelOrigin, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawModel( pResults, info, pBoneToWorld, pFlexWeights, pFlexDelayedWeights, modelOrigin, flags ); }
void IStudioRender::DrawModelStaticProp( const DrawModelInfo_t& drawInfo, const matrix3x4_t &modelToWorld, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawModelStaticProp( drawInfo, modelToWorld, flags ); }
void IStudioRender::DrawStaticPropDecals( const DrawModelInfo_t &drawInfo, const matrix3x4_t &modelToWorld )
{ static_cast<CStudioRenderContext*>(this)->DrawStaticPropDecals( drawInfo, modelToWorld ); }
void IStudioRender::DrawStaticPropShadows( const DrawModelInfo_t &drawInfo, const matrix3x4_t &modelToWorld, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawStaticPropShadows( drawInfo, modelToWorld, flags ); }
void IStudioRender::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType, int nMaterialIndex )
{ static_cast<CStudioRenderContext*>(this)->ForcedMaterialOverride( newMaterial, nOverrideType, nMaterialIndex ); }
bool IStudioRender::IsForcedMaterialOverride()
{ return static_cast<CStudioRenderContext*>(this)->IsForcedMaterialOverride(); }
StudioDecalHandle_t IStudioRender::CreateDecalList( studiohwdata_t *pHardwareData )
{ return static_cast<CStudioRenderContext*>(this)->CreateDecalList( pHardwareData ); }
void IStudioRender::DestroyDecalList( StudioDecalHandle_t handle )
{ static_cast<CStudioRenderContext*>(this)->DestroyDecalList( handle ); }
void IStudioRender::AddDecal( StudioDecalHandle_t handle, studiohdr_t *pStudioHdr, matrix3x4_t *pBoneToWorld, const Ray_t & ray, const Vector& decalUp, IMaterial* pDecalMaterial, float radius, int body, bool noPokethru, int maxLODToDecal, void *pvProxyUserData, int nAdditionalDecalFlags )
{ static_cast<CStudioRenderContext*>(this)->AddDecal( handle, pStudioHdr, pBoneToWorld, ray, decalUp, pDecalMaterial, radius, body, noPokethru, maxLODToDecal, pvProxyUserData, nAdditionalDecalFlags ); }
void IStudioRender::ComputeLighting( const Vector* pAmbient, int lightCount, LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting )
{ static_cast<CStudioRenderContext*>(this)->ComputeLighting( pAmbient, lightCount, pLights, pt, normal, lighting ); }
void IStudioRender::ComputeLightingConstDirectional( const Vector* pAmbient, int lightCount, LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting, float flDirectionalAmount )
{ static_cast<CStudioRenderContext*>(this)->ComputeLightingConstDirectional( pAmbient, lightCount, pLights, pt, normal, lighting, flDirectionalAmount ); }
void IStudioRender::AddShadow( IMaterial* pMaterial, void* pProxyData, FlashlightState_t *m_pFlashlightState, VMatrix *pWorldToTexture, ITexture *pFlashlightDepthTexture )
{ static_cast<CStudioRenderContext*>(this)->AddShadow( pMaterial, pProxyData, m_pFlashlightState, pWorldToTexture, pFlashlightDepthTexture ); }
void IStudioRender::ClearAllShadows()
{ static_cast<CStudioRenderContext*>(this)->ClearAllShadows(); }
int IStudioRender::ComputeModelLod( studiohwdata_t* pHardwareData, float unitSphereSize, float *pMetric )
{ return static_cast<CStudioRenderContext*>(this)->ComputeModelLod( pHardwareData, unitSphereSize, pMetric ); }
void IStudioRender::GetPerfStats( DrawModelResults_t *pResults, const DrawModelInfo_t &info, ks::text_writer *pSpewBuf ) const
{ static_cast<const CStudioRenderContext*>(this)->GetPerfStats( pResults, info, pSpewBuf ); }
void IStudioRender::GetTriangles( const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, GetTriangles_Output_t &out )
{ static_cast<CStudioRenderContext*>(this)->GetTriangles( info, pBoneToWorld, out ); }
int IStudioRender::GetMaterialList( studiohdr_t *pStudioHdr, int count, IMaterial** ppMaterials )
{ return static_cast<CStudioRenderContext*>(this)->GetMaterialList( pStudioHdr, count, ppMaterials ); }
int IStudioRender::GetMaterialListFromBodyAndSkin( MDLHandle_t studio, int nSkin, int nBody, int nCountOutputMaterials, IMaterial** ppOutputMaterials )
{ return static_cast<CStudioRenderContext*>(this)->GetMaterialListFromBodyAndSkin( studio, nSkin, nBody, nCountOutputMaterials, ppOutputMaterials ); }
void IStudioRender::DrawModelArrayStaticProp( const DrawModelInfo_t& drawInfo, int nInstanceCount, const MeshInstanceData_t *pInstanceData, ColorMeshInfo_t **pColorMeshes )
{ static_cast<CStudioRenderContext*>(this)->DrawModelArrayStaticProp( drawInfo, nInstanceCount, pInstanceData, pColorMeshes ); }
void IStudioRender::DrawModelArray( const StudioModelArrayInfo_t &drawInfo, int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawModelArray( drawInfo, nCount, pInstanceData, nInstanceStride, flags ); }
void IStudioRender::DrawModelShadowArray( int nCount, StudioArrayData_t *pShadowData, int nInstanceStride, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawModelShadowArray( nCount, pShadowData, nInstanceStride, flags ); }
void IStudioRender::DrawModelArray( const StudioModelArrayInfo2_t &info, int nCount, StudioArrayData_t *pArrayData, int nInstanceStride, int flags )
{ static_cast<CStudioRenderContext*>(this)->DrawModelArray( info, nCount, pArrayData, nInstanceStride, flags ); }
void IStudioRender::GatherRenderedFaceInfo( FaceInfoCallbackFunc_t pFunc )
{ static_cast<CStudioRenderContext*>(this)->GatherRenderedFaceInfo( pFunc ); }
bool IStudioRender::Connect( CreateInterfaceFn factory )
{ return static_cast<CStudioRenderContext*>(this)->Connect( factory ); }
void IStudioRender::Disconnect()
{ static_cast<CStudioRenderContext*>(this)->Disconnect(); }
void *IStudioRender::QueryInterface( const char *pInterfaceName )
{ return static_cast<CStudioRenderContext*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IStudioRender::Init()
{ return static_cast<CStudioRenderContext*>(this)->Init(); }
void IStudioRender::Shutdown()
{ static_cast<CStudioRenderContext*>(this)->Shutdown(); }

