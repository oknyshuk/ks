// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IModelLoader class to CModelLoader. Included by modelloader.cpp.

void IModelLoader::Init( void )
{ static_cast<CModelLoader*>(this)->Init(); }
void IModelLoader::Shutdown( void )
{ static_cast<CModelLoader*>(this)->Shutdown(); }
const char * IModelLoader::GetName( const model_t *model )
{ return static_cast<CModelLoader*>(this)->GetName( model ); }
void * IModelLoader::GetExtraData( model_t *model )
{ return static_cast<CModelLoader*>(this)->GetExtraData( model ); }
int IModelLoader::GetModelFileSize( const char *name )
{ return static_cast<CModelLoader*>(this)->GetModelFileSize( name ); }
model_t * IModelLoader::GetModelForName( const char *name, REFERENCETYPE referencetype )
{ return static_cast<CModelLoader*>(this)->GetModelForName( name, referencetype ); }
model_t * IModelLoader::ReferenceModel( const char *name, REFERENCETYPE referencetype )
{ return static_cast<CModelLoader*>(this)->ReferenceModel( name, referencetype ); }
void IModelLoader::UnreferenceModel( model_t *model, REFERENCETYPE referencetype )
{ static_cast<CModelLoader*>(this)->UnreferenceModel( model, referencetype ); }
void IModelLoader::UnreferenceAllModels( REFERENCETYPE referencetype )
{ static_cast<CModelLoader*>(this)->UnreferenceAllModels( referencetype ); }
void IModelLoader::UnloadUnreferencedModels( void )
{ static_cast<CModelLoader*>(this)->UnloadUnreferencedModels(); }
void IModelLoader::PurgeUnusedModels( void )
{ static_cast<CModelLoader*>(this)->PurgeUnusedModels(); }
void IModelLoader::UnMountCompatibilityPaths( void )
{ static_cast<CModelLoader*>(this)->UnMountCompatibilityPaths(); }
void IModelLoader::AddCompatibilityPath( const char *szNewCompatibilityPath )
{ static_cast<CModelLoader*>(this)->AddCompatibilityPath( szNewCompatibilityPath ); }
bool IModelLoader::Map_GetRenderInfoAllocated( void )
{ return static_cast<CModelLoader*>(this)->Map_GetRenderInfoAllocated(); }
void IModelLoader::Map_SetRenderInfoAllocated( bool allocated )
{ static_cast<CModelLoader*>(this)->Map_SetRenderInfoAllocated( allocated ); }
void IModelLoader::Map_LoadDisplacements( model_t *model, bool bRestoring )
{ static_cast<CModelLoader*>(this)->Map_LoadDisplacements( model, bRestoring ); }
void IModelLoader::Print( void )
{ static_cast<CModelLoader*>(this)->Print(); }
bool IModelLoader::Map_IsValid( char const *pBaseMapName, bool bQuiet )
{ return static_cast<CModelLoader*>(this)->Map_IsValid( pBaseMapName, bQuiet ); }
void IModelLoader::RecomputeSurfaceFlags( model_t *mod )
{ static_cast<CModelLoader*>(this)->RecomputeSurfaceFlags( mod ); }
void IModelLoader::Studio_ReloadModels( ReloadType_t reloadType )
{ static_cast<CModelLoader*>(this)->Studio_ReloadModels( reloadType ); }
bool IModelLoader::IsLoaded( const model_t *mod )
{ return static_cast<CModelLoader*>(this)->IsLoaded( mod ); }
bool IModelLoader::LastLoadedMapHasHDRLighting( void )
{ return static_cast<CModelLoader*>(this)->LastLoadedMapHasHDRLighting(); }
bool IModelLoader::LastLoadedMapHasLightmapAlphaData( void )
{ return static_cast<CModelLoader*>(this)->LastLoadedMapHasLightmapAlphaData(); }
void IModelLoader::ReloadFilesInList( IFileList *pFilesToReload )
{ static_cast<CModelLoader*>(this)->ReloadFilesInList( pFilesToReload ); }
void IModelLoader::UpdateDynamicModels()
{ static_cast<CModelLoader*>(this)->UpdateDynamicModels(); }
void IModelLoader::FlushDynamicModels()
{ static_cast<CModelLoader*>(this)->FlushDynamicModels(); }
void IModelLoader::ForceUnloadNonClientDynamicModels()
{ static_cast<CModelLoader*>(this)->ForceUnloadNonClientDynamicModels(); }
model_t * IModelLoader::GetDynamicModel( const char *name, bool bClientOnly )
{ return static_cast<CModelLoader*>(this)->GetDynamicModel( name, bClientOnly ); }
model_t * IModelLoader::GetDynamicCombinedModel( const char *name, bool bClientOnly )
{ return static_cast<CModelLoader*>(this)->GetDynamicCombinedModel( name, bClientOnly ); }
void IModelLoader::UpdateDynamicCombinedModel( model_t *pModel, MDLHandle_t Handle, bool bClientSide )
{ static_cast<CModelLoader*>(this)->UpdateDynamicCombinedModel( pModel, Handle, bClientSide ); }
bool IModelLoader::SetCombineModels( model_t *pModel, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{ return static_cast<CModelLoader*>(this)->SetCombineModels( pModel, vecModelsToCombine ); }
bool IModelLoader::FinishCombinedModel( model_t *pModel, CombinedModelLoadedCallback pFunc, void *pUserData )
{ return static_cast<CModelLoader*>(this)->FinishCombinedModel( pModel, pFunc, pUserData ); }
bool IModelLoader::IsDynamicModelLoading( model_t *pModel )
{ return static_cast<CModelLoader*>(this)->IsDynamicModelLoading( pModel ); }
void IModelLoader::AddRefDynamicModel( model_t *pModel, bool bClientSideRef )
{ static_cast<CModelLoader*>(this)->AddRefDynamicModel( pModel, bClientSideRef ); }
void IModelLoader::ReleaseDynamicModel( model_t *pModel, bool bClientSideRef )
{ static_cast<CModelLoader*>(this)->ReleaseDynamicModel( pModel, bClientSideRef ); }
bool IModelLoader::RegisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded )
{ return static_cast<CModelLoader*>(this)->RegisterModelLoadCallback( pModel, pCallback, bCallImmediatelyIfLoaded ); }
void IModelLoader::UnregisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback )
{ static_cast<CModelLoader*>(this)->UnregisterModelLoadCallback( pModel, pCallback ); }
void IModelLoader::Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded )
{ static_cast<CModelLoader*>(this)->Client_OnServerModelStateChanged( pModel, bServerLoaded ); }
void IModelLoader::UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{ static_cast<CModelLoader*>(this)->UpdateViewWeaponModelCache( ppWeaponModels, nWeaponModels ); }
void IModelLoader::TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{ static_cast<CModelLoader*>(this)->TouchWorldWeaponModelCache( ppWeaponModels, nWeaponModels ); }
bool IModelLoader::IsModelInWeaponCache( const model_t *pModel )
{ return static_cast<CModelLoader*>(this)->IsModelInWeaponCache( pModel ); }
void IModelLoader::EvictAllWeaponsFromModelCache( bool bLoadingComplete )
{ static_cast<CModelLoader*>(this)->EvictAllWeaponsFromModelCache( bLoadingComplete ); }
bool IModelLoader::IsViewWeaponModelResident( const model_t *pModel )
{ return static_cast<CModelLoader*>(this)->IsViewWeaponModelResident( pModel ); }

