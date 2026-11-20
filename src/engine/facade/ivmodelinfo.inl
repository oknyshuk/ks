import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVModelInfo / IVModelInfoClient classes: kind-based dispatch to CModelInfoServer /
// CModelInfoClient. Included by ModelInfo.cpp.

const model_t * IVModelInfo::GetModel( int modelindex ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModel( modelindex );
	default: return static_cast<const CModelInfoClient*>(this)->GetModel( modelindex );
	}
}
int IVModelInfo::GetModelIndex( const char *name ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelIndex( name );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelIndex( name );
	}
}
const char * IVModelInfo::GetModelName( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelName( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelName( model );
	}
}
vcollide_t * IVModelInfo::GetVCollide( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetVCollide( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetVCollide( model );
	}
}
vcollide_t * IVModelInfo::GetVCollide( int modelindex ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetVCollide( modelindex );
	default: return static_cast<const CModelInfoClient*>(this)->GetVCollide( modelindex );
	}
}
void IVModelInfo::GetModelBounds( const model_t *model, Vector& mins, Vector& maxs ) const
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<const CModelInfoServer*>(this)->GetModelBounds( model, mins, maxs ); break;
	default: static_cast<const CModelInfoClient*>(this)->GetModelBounds( model, mins, maxs ); break;
	}
}
void IVModelInfo::GetModelRenderBounds( const model_t *model, Vector& mins, Vector& maxs ) const
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<const CModelInfoServer*>(this)->GetModelRenderBounds( model, mins, maxs ); break;
	default: static_cast<const CModelInfoClient*>(this)->GetModelRenderBounds( model, mins, maxs ); break;
	}
}
int IVModelInfo::GetModelFrameCount( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelFrameCount( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelFrameCount( model );
	}
}
int IVModelInfo::GetModelType( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelType( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelType( model );
	}
}
void * IVModelInfo::GetModelExtraData( const model_t *model )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelExtraData( model );
	default: return static_cast<CModelInfoClient*>(this)->GetModelExtraData( model );
	}
}
bool IVModelInfo::ModelHasMaterialProxy( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->ModelHasMaterialProxy( model );
	default: return static_cast<const CModelInfoClient*>(this)->ModelHasMaterialProxy( model );
	}
}
bool IVModelInfo::IsTranslucent( model_t const* model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->IsTranslucent( model );
	default: return static_cast<const CModelInfoClient*>(this)->IsTranslucent( model );
	}
}
bool IVModelInfo::IsTranslucentTwoPass( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->IsTranslucentTwoPass( model );
	default: return static_cast<const CModelInfoClient*>(this)->IsTranslucentTwoPass( model );
	}
}
RenderableTranslucencyType_t IVModelInfo::ComputeTranslucencyType( const model_t *model, int nSkin, int nBody )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->ComputeTranslucencyType( model, nSkin, nBody );
	default: return static_cast<CModelInfoClient*>(this)->ComputeTranslucencyType( model, nSkin, nBody );
	}
}
int IVModelInfo::GetModelMaterialCount( const model_t* model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelMaterialCount( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelMaterialCount( model );
	}
}
int IVModelInfo::GetModelMaterials( const model_t *model, int count, IMaterial** ppMaterial )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelMaterials( model, count, ppMaterial );
	default: return static_cast<CModelInfoClient*>(this)->GetModelMaterials( model, count, ppMaterial );
	}
}
bool IVModelInfo::IsModelVertexLit( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->IsModelVertexLit( model );
	default: return static_cast<const CModelInfoClient*>(this)->IsModelVertexLit( model );
	}
}
const char * IVModelInfo::GetModelKeyValueText( const model_t *model )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelKeyValueText( model );
	default: return static_cast<CModelInfoClient*>(this)->GetModelKeyValueText( model );
	}
}
bool IVModelInfo::GetModelKeyValue( const model_t *model, ks::text_writer &buf )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelKeyValue( model, buf );
	default: return static_cast<CModelInfoClient*>(this)->GetModelKeyValue( model, buf );
	}
}
float IVModelInfo::GetModelRadius( const model_t *model )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelRadius( model );
	default: return static_cast<CModelInfoClient*>(this)->GetModelRadius( model );
	}
}
const studiohdr_t * IVModelInfo::FindModel( const studiohdr_t *pStudioHdr, void **cache, const char *modelname ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->FindModel( pStudioHdr, cache, modelname );
	default: return static_cast<const CModelInfoClient*>(this)->FindModel( pStudioHdr, cache, modelname );
	}
}
const studiohdr_t * IVModelInfo::FindModel( void *cache ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->FindModel( cache );
	default: return static_cast<const CModelInfoClient*>(this)->FindModel( cache );
	}
}
virtualmodel_t * IVModelInfo::GetVirtualModel( const studiohdr_t *pStudioHdr ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetVirtualModel( pStudioHdr );
	default: return static_cast<const CModelInfoClient*>(this)->GetVirtualModel( pStudioHdr );
	}
}
byte * IVModelInfo::GetAnimBlock( const studiohdr_t *pStudioHdr, int nBlock, bool bPreloadIfMissing ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetAnimBlock( pStudioHdr, nBlock, bPreloadIfMissing );
	default: return static_cast<const CModelInfoClient*>(this)->GetAnimBlock( pStudioHdr, nBlock, bPreloadIfMissing );
	}
}
bool IVModelInfo::HasAnimBlockBeenPreloaded( const studiohdr_t *pStudioHdr, int nBlock ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->HasAnimBlockBeenPreloaded( pStudioHdr, nBlock );
	default: return static_cast<const CModelInfoClient*>(this)->HasAnimBlockBeenPreloaded( pStudioHdr, nBlock );
	}
}
void IVModelInfo::GetModelMaterialColorAndLighting( const model_t *model, Vector const& origin, QAngle const& angles, trace_t* pTrace, Vector& lighting, Vector& matColor )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->GetModelMaterialColorAndLighting( model, origin, angles, pTrace, lighting, matColor ); break;
	default: static_cast<CModelInfoClient*>(this)->GetModelMaterialColorAndLighting( model, origin, angles, pTrace, lighting, matColor ); break;
	}
}
void IVModelInfo::GetIlluminationPoint( const model_t *model, IClientRenderable *pRenderable, Vector const& origin, QAngle const& angles, Vector* pLightingCenter )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->GetIlluminationPoint( model, pRenderable, origin, angles, pLightingCenter ); break;
	default: static_cast<CModelInfoClient*>(this)->GetIlluminationPoint( model, pRenderable, origin, angles, pLightingCenter ); break;
	}
}
int IVModelInfo::GetModelContents( int modelIndex ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelContents( modelIndex );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelContents( modelIndex );
	}
}
studiohdr_t * IVModelInfo::GetStudiomodel( const model_t *mod )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetStudiomodel( mod );
	default: return static_cast<CModelInfoClient*>(this)->GetStudiomodel( mod );
	}
}
int IVModelInfo::GetModelSpriteWidth( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelSpriteWidth( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelSpriteWidth( model );
	}
}
int IVModelInfo::GetModelSpriteHeight( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetModelSpriteHeight( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetModelSpriteHeight( model );
	}
}
void IVModelInfo::SetLevelScreenFadeRange( float flMinSize, float flMaxSize )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->SetLevelScreenFadeRange( flMinSize, flMaxSize ); break;
	default: static_cast<CModelInfoClient*>(this)->SetLevelScreenFadeRange( flMinSize, flMaxSize ); break;
	}
}
void IVModelInfo::GetLevelScreenFadeRange( float *pMinArea, float *pMaxArea ) const
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<const CModelInfoServer*>(this)->GetLevelScreenFadeRange( pMinArea, pMaxArea ); break;
	default: static_cast<const CModelInfoClient*>(this)->GetLevelScreenFadeRange( pMinArea, pMaxArea ); break;
	}
}
void IVModelInfo::SetViewScreenFadeRange( float flMinSize, float flMaxSize )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->SetViewScreenFadeRange( flMinSize, flMaxSize ); break;
	default: static_cast<CModelInfoClient*>(this)->SetViewScreenFadeRange( flMinSize, flMaxSize ); break;
	}
}
unsigned char IVModelInfo::ComputeLevelScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->ComputeLevelScreenFade( vecAbsOrigin, flRadius, flFadeScale );
	default: return static_cast<const CModelInfoClient*>(this)->ComputeLevelScreenFade( vecAbsOrigin, flRadius, flFadeScale );
	}
}
unsigned char IVModelInfo::ComputeViewScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->ComputeViewScreenFade( vecAbsOrigin, flRadius, flFadeScale );
	default: return static_cast<const CModelInfoClient*>(this)->ComputeViewScreenFade( vecAbsOrigin, flRadius, flFadeScale );
	}
}
int IVModelInfo::GetAutoplayList( const studiohdr_t *pStudioHdr, unsigned short **pAutoplayList ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetAutoplayList( pStudioHdr, pAutoplayList );
	default: return static_cast<const CModelInfoClient*>(this)->GetAutoplayList( pStudioHdr, pAutoplayList );
	}
}
CPhysCollide * IVModelInfo::GetCollideForVirtualTerrain( int index )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetCollideForVirtualTerrain( index );
	default: return static_cast<CModelInfoClient*>(this)->GetCollideForVirtualTerrain( index );
	}
}
bool IVModelInfo::IsUsingFBTexture( const model_t *model, int nSkin, int nBody, void *pClientRenderable ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->IsUsingFBTexture( model, nSkin, nBody, pClientRenderable );
	default: return static_cast<const CModelInfoClient*>(this)->IsUsingFBTexture( model, nSkin, nBody, pClientRenderable );
	}
}
const model_t * IVModelInfo::FindOrLoadModel( const char *name ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->FindOrLoadModel( name );
	default: return static_cast<const CModelInfoClient*>(this)->FindOrLoadModel( name );
	}
}
MDLHandle_t IVModelInfo::GetCacheHandle( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetCacheHandle( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetCacheHandle( model );
	}
}
int IVModelInfo::GetBrushModelPlaneCount( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->GetBrushModelPlaneCount( model );
	default: return static_cast<const CModelInfoClient*>(this)->GetBrushModelPlaneCount( model );
	}
}
void IVModelInfo::GetBrushModelPlane( const model_t *model, int nIndex, cplane_t &plane, Vector *pOrigin ) const
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<const CModelInfoServer*>(this)->GetBrushModelPlane( model, nIndex, plane, pOrigin ); break;
	default: static_cast<const CModelInfoClient*>(this)->GetBrushModelPlane( model, nIndex, plane, pOrigin ); break;
	}
}
int IVModelInfo::GetSurfacepropsForVirtualTerrain( int index )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetSurfacepropsForVirtualTerrain( index );
	default: return static_cast<CModelInfoClient*>(this)->GetSurfacepropsForVirtualTerrain( index );
	}
}
bool IVModelInfo::UsesEnvCubemap( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->UsesEnvCubemap( model );
	default: return static_cast<const CModelInfoClient*>(this)->UsesEnvCubemap( model );
	}
}
bool IVModelInfo::UsesStaticLighting( const model_t *model ) const
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<const CModelInfoServer*>(this)->UsesStaticLighting( model );
	default: return static_cast<const CModelInfoClient*>(this)->UsesStaticLighting( model );
	}
}
int IVModelInfo::RegisterDynamicModel( const char *name, bool bClientSide )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->RegisterDynamicModel( name, bClientSide );
	default: return static_cast<CModelInfoClient*>(this)->RegisterDynamicModel( name, bClientSide );
	}
}
int IVModelInfo::RegisterCombinedDynamicModel( const char *pszName, MDLHandle_t Handle )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->RegisterCombinedDynamicModel( pszName, Handle );
	default: return static_cast<CModelInfoClient*>(this)->RegisterCombinedDynamicModel( pszName, Handle );
	}
}
void IVModelInfo::UpdateCombinedDynamicModel( int nModelIndex, MDLHandle_t Handle )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->UpdateCombinedDynamicModel( nModelIndex, Handle ); break;
	default: static_cast<CModelInfoClient*>(this)->UpdateCombinedDynamicModel( nModelIndex, Handle ); break;
	}
}
int IVModelInfo::BeginCombinedModel( const char *pszName, bool bReuseExisting )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->BeginCombinedModel( pszName, bReuseExisting );
	default: return static_cast<CModelInfoClient*>(this)->BeginCombinedModel( pszName, bReuseExisting );
	}
}
bool IVModelInfo::SetCombineModels( int nModelIndex, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->SetCombineModels( nModelIndex, vecModelsToCombine );
	default: return static_cast<CModelInfoClient*>(this)->SetCombineModels( nModelIndex, vecModelsToCombine );
	}
}
bool IVModelInfo::FinishCombinedModel( int nModelIndex, CombinedModelLoadedCallback pFunc, void *pUserData )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->FinishCombinedModel( nModelIndex, pFunc, pUserData );
	default: return static_cast<CModelInfoClient*>(this)->FinishCombinedModel( nModelIndex, pFunc, pUserData );
	}
}
void IVModelInfo::ReleaseCombinedModel( int nModelIndex )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->ReleaseCombinedModel( nModelIndex ); break;
	default: static_cast<CModelInfoClient*>(this)->ReleaseCombinedModel( nModelIndex ); break;
	}
}
bool IVModelInfo::IsDynamicModelLoading( int modelIndex )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->IsDynamicModelLoading( modelIndex );
	default: return static_cast<CModelInfoClient*>(this)->IsDynamicModelLoading( modelIndex );
	}
}
void IVModelInfo::AddRefDynamicModel( int modelIndex )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->AddRefDynamicModel( modelIndex ); break;
	default: static_cast<CModelInfoClient*>(this)->AddRefDynamicModel( modelIndex ); break;
	}
}
void IVModelInfo::ReleaseDynamicModel( int modelIndex )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->ReleaseDynamicModel( modelIndex ); break;
	default: static_cast<CModelInfoClient*>(this)->ReleaseDynamicModel( modelIndex ); break;
	}
}
bool IVModelInfo::RegisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback, bool bCallImmediatelyIfLoaded )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->RegisterModelLoadCallback( modelindex, pCallback, bCallImmediatelyIfLoaded );
	default: return static_cast<CModelInfoClient*>(this)->RegisterModelLoadCallback( modelindex, pCallback, bCallImmediatelyIfLoaded );
	}
}
void IVModelInfo::UnregisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->UnregisterModelLoadCallback( modelindex, pCallback ); break;
	default: static_cast<CModelInfoClient*>(this)->UnregisterModelLoadCallback( modelindex, pCallback ); break;
	}
}
void IVModelInfo::OnLevelChange()
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->OnLevelChange(); break;
	default: static_cast<CModelInfoClient*>(this)->OnLevelChange(); break;
	}
}
KeyValues * IVModelInfo::GetModelKeyValues( const model_t *pModel )
{
	switch ( m_kind )
	{
	case kind_t::server: return static_cast<CModelInfoServer*>(this)->GetModelKeyValues( pModel );
	default: return static_cast<CModelInfoClient*>(this)->GetModelKeyValues( pModel );
	}
}
void IVModelInfo::UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->UpdateViewWeaponModelCache( ppWeaponModels, nWeaponModels ); break;
	default: static_cast<CModelInfoClient*>(this)->UpdateViewWeaponModelCache( ppWeaponModels, nWeaponModels ); break;
	}
}
void IVModelInfo::TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{
	switch ( m_kind )
	{
	case kind_t::server: static_cast<CModelInfoServer*>(this)->TouchWorldWeaponModelCache( ppWeaponModels, nWeaponModels ); break;
	default: static_cast<CModelInfoClient*>(this)->TouchWorldWeaponModelCache( ppWeaponModels, nWeaponModels ); break;
	}
}

int IVModelInfoClient::GetModelClientSideIndex( const char *name ) const
{ return static_cast<const CModelInfoClient*>(this)->GetModelClientSideIndex( name ); }
void IVModelInfoClient::OnDynamicModelStringTableChanged( int nStringIndex, const char *pString, const void *pData )
{ static_cast<CModelInfoClient*>(this)->OnDynamicModelStringTableChanged( nStringIndex, pString, pData ); }
model_t * IVModelInfoClient::ReferenceModel( const char *name )
{ return static_cast<CModelInfoClient*>(this)->ReferenceModel( name ); }
void IVModelInfoClient::UnreferenceModel( model_t *model )
{ static_cast<CModelInfoClient*>(this)->UnreferenceModel( model ); }
void IVModelInfoClient::UnloadUnreferencedModels( void )
{ static_cast<CModelInfoClient*>(this)->UnloadUnreferencedModels(); }

