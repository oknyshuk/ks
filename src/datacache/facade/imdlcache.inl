import ks.core.basetypes;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMDLCache and IStudioDataCache classes to CMDLCache. Included by mdlcache.cpp.

void IMDLCache::SetCacheNotify( IMDLCacheNotify *pNotify )
{ static_cast<CMDLCache*>(this)->SetCacheNotify( pNotify ); }
MDLHandle_t IMDLCache::FindMDL( const char *pMDLRelativePath )
{ return static_cast<CMDLCache*>(this)->FindMDL( pMDLRelativePath ); }
int IMDLCache::AddRef( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->AddRef( handle ); }
int IMDLCache::Release( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->Release( handle ); }
int IMDLCache::GetRef( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetRef( handle ); }
studiohdr_t * IMDLCache::GetStudioHdr( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetStudioHdr( handle ); }
studiohwdata_t * IMDLCache::GetHardwareData( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetHardwareData( handle ); }
vcollide_t * IMDLCache::GetVCollide( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetVCollide( handle ); }
unsigned char * IMDLCache::GetAnimBlock( MDLHandle_t handle, int nBlock, bool preloadIfMissing )
{ return static_cast<CMDLCache*>(this)->GetAnimBlock( handle, nBlock, preloadIfMissing ); }
bool IMDLCache::HasAnimBlockBeenPreloaded( MDLHandle_t handle, int nBlock )
{ return static_cast<CMDLCache*>(this)->HasAnimBlockBeenPreloaded( handle, nBlock ); }
virtualmodel_t * IMDLCache::GetVirtualModel( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetVirtualModel( handle ); }
int IMDLCache::GetAutoplayList( MDLHandle_t handle, unsigned short **pOut )
{ return static_cast<CMDLCache*>(this)->GetAutoplayList( handle, pOut ); }
vertexFileHeader_t * IMDLCache::GetVertexData( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetVertexData( handle ); }
void IMDLCache::TouchAllData( MDLHandle_t handle )
{ static_cast<CMDLCache*>(this)->TouchAllData( handle ); }
void IMDLCache::SetUserData( MDLHandle_t handle, void* pData )
{ static_cast<CMDLCache*>(this)->SetUserData( handle, pData ); }
void * IMDLCache::GetUserData( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetUserData( handle ); }
bool IMDLCache::IsErrorModel( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->IsErrorModel( handle ); }
bool IMDLCache::IsOverBudget( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->IsOverBudget( handle ); }
void IMDLCache::Flush( MDLCacheFlush_t nFlushFlags )
{ static_cast<CMDLCache*>(this)->Flush( nFlushFlags ); }
void IMDLCache::Flush( MDLHandle_t handle, int nFlushFlags )
{ static_cast<CMDLCache*>(this)->Flush( handle, nFlushFlags ); }
const char * IMDLCache::GetModelName( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetModelName( handle ); }
IDataCacheSection * IMDLCache::GetCacheSection( MDLCacheDataType_t type )
{ return static_cast<CMDLCache*>(this)->GetCacheSection( type ); }
virtualmodel_t * IMDLCache::GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetVirtualModelFast( pStudioHdr, handle ); }
void IMDLCache::BeginLock()
{ static_cast<CMDLCache*>(this)->BeginLock(); }
void IMDLCache::EndLock()
{ static_cast<CMDLCache*>(this)->EndLock(); }
int * IMDLCache::GetFrameUnlockCounterPtrOLD()
{ return static_cast<CMDLCache*>(this)->GetFrameUnlockCounterPtrOLD(); }
void IMDLCache::FinishPendingLoads()
{ static_cast<CMDLCache*>(this)->FinishPendingLoads(); }
vcollide_t * IMDLCache::GetVCollideEx( MDLHandle_t handle, bool synchronousLoad )
{ return static_cast<CMDLCache*>(this)->GetVCollideEx( handle, synchronousLoad ); }
bool IMDLCache::GetVCollideSize( MDLHandle_t handle, int *pVCollideSize )
{ return static_cast<CMDLCache*>(this)->GetVCollideSize( handle, pVCollideSize ); }
bool IMDLCache::GetAsyncLoad( MDLCacheDataType_t type )
{ return static_cast<CMDLCache*>(this)->GetAsyncLoad( type ); }
bool IMDLCache::SetAsyncLoad( MDLCacheDataType_t type, bool bAsync )
{ return static_cast<CMDLCache*>(this)->SetAsyncLoad( type, bAsync ); }
void IMDLCache::BeginMapLoad()
{ static_cast<CMDLCache*>(this)->BeginMapLoad(); }
void IMDLCache::EndMapLoad()
{ static_cast<CMDLCache*>(this)->EndMapLoad(); }
void IMDLCache::MarkAsLoaded( MDLHandle_t handle )
{ static_cast<CMDLCache*>(this)->MarkAsLoaded( handle ); }
void IMDLCache::InitPreloadData( bool rebuild )
{ static_cast<CMDLCache*>(this)->InitPreloadData( rebuild ); }
void IMDLCache::ShutdownPreloadData()
{ static_cast<CMDLCache*>(this)->ShutdownPreloadData(); }
bool IMDLCache::IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type )
{ return static_cast<CMDLCache*>(this)->IsDataLoaded( handle, type ); }
int * IMDLCache::GetFrameUnlockCounterPtr( MDLCacheDataType_t type )
{ return static_cast<CMDLCache*>(this)->GetFrameUnlockCounterPtr( type ); }
studiohdr_t * IMDLCache::LockStudioHdr( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->LockStudioHdr( handle ); }
void IMDLCache::UnlockStudioHdr( MDLHandle_t handle )
{ static_cast<CMDLCache*>(this)->UnlockStudioHdr( handle ); }
bool IMDLCache::PreloadModel( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->PreloadModel( handle ); }
void IMDLCache::ResetErrorModelStatus( MDLHandle_t handle )
{ static_cast<CMDLCache*>(this)->ResetErrorModelStatus( handle ); }
void IMDLCache::MarkFrame()
{ static_cast<CMDLCache*>(this)->MarkFrame(); }
void IMDLCache::BeginCoarseLock()
{ static_cast<CMDLCache*>(this)->BeginCoarseLock(); }
void IMDLCache::EndCoarseLock()
{ static_cast<CMDLCache*>(this)->EndCoarseLock(); }
void IMDLCache::ReloadVCollide( MDLHandle_t handle )
{ static_cast<CMDLCache*>(this)->ReloadVCollide( handle ); }
bool IMDLCache::ReleaseAnimBlockAllocator()
{ return static_cast<CMDLCache*>(this)->ReleaseAnimBlockAllocator(); }
bool IMDLCache::RestoreHardwareData( MDLHandle_t handle, FSAsyncControl_t *pAsyncVTXControl, FSAsyncControl_t *pAsyncVVDControl )
{ return static_cast<CMDLCache*>(this)->RestoreHardwareData( handle, pAsyncVTXControl, pAsyncVVDControl ); }
void IMDLCache::DisableVCollideLoad()
{ static_cast<CMDLCache*>(this)->DisableVCollideLoad(); }
void IMDLCache::EnableVCollideLoad()
{ static_cast<CMDLCache*>(this)->EnableVCollideLoad(); }
void IMDLCache::DisableFileNotFoundWarnings()
{ static_cast<CMDLCache*>(this)->DisableFileNotFoundWarnings(); }
void IMDLCache::EnableFileNotFoundWarnings()
{ static_cast<CMDLCache*>(this)->EnableFileNotFoundWarnings(); }
bool IMDLCache::ProcessPendingHardwareRestore()
{ return static_cast<CMDLCache*>(this)->ProcessPendingHardwareRestore(); }
void IMDLCache::UnloadQueuedHardwareData()
{ static_cast<CMDLCache*>(this)->UnloadQueuedHardwareData(); }
void IMDLCache::DumpDictionaryState()
{ static_cast<CMDLCache*>(this)->DumpDictionaryState(); }
MDLHandle_t IMDLCache::CreateCombinedModel( const char *pszModelName )
{ return static_cast<CMDLCache*>(this)->CreateCombinedModel( pszModelName ); }
bool IMDLCache::CreateCombinedModel( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->CreateCombinedModel( handle ); }
bool IMDLCache::SetCombineModels( MDLHandle_t handle, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{ return static_cast<CMDLCache*>(this)->SetCombineModels( handle, vecModelsToCombine ); }
bool IMDLCache::FinishCombinedModel( MDLHandle_t handle, CombinedModelLoadedCallback pFunc, void *pUserData )
{ return static_cast<CMDLCache*>(this)->FinishCombinedModel( handle, pFunc, pUserData ); }
bool IMDLCache::IsCombinedPlaceholder( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->IsCombinedPlaceholder( handle ); }
bool IMDLCache::IsCombinedModel( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->IsCombinedModel( handle ); }
int IMDLCache::GetNumCombinedSubModels( MDLHandle_t handle )
{ return static_cast<CMDLCache*>(this)->GetNumCombinedSubModels( handle ); }
void IMDLCache::GetCombinedSubModelFilename( MDLHandle_t handle, int nSubModelIndex, char *pszResult, int nResultSize )
{ static_cast<CMDLCache*>(this)->GetCombinedSubModelFilename( handle, nSubModelIndex, pszResult, nResultSize ); }
KeyValues * IMDLCache::GetCombinedMaterialKV( MDLHandle_t handle, int nAtlasGroup )
{ return static_cast<CMDLCache*>(this)->GetCombinedMaterialKV( handle, nAtlasGroup ); }
void IMDLCache::UpdateCombiner()
{ static_cast<CMDLCache*>(this)->UpdateCombiner(); }
void * IMDLCache::GetCombinedInternalAsset( ECombinedAsset AssetType, const char *pszAssetID, int *nSize )
{ return static_cast<CMDLCache*>(this)->GetCombinedInternalAsset( AssetType, pszAssetID, nSize ); }
void IMDLCache::SetCombinerFlags( unsigned nFlags )
{ static_cast<CMDLCache*>(this)->SetCombinerFlags( nFlags ); }
void IMDLCache::ClearCombinerFlags( unsigned nFlags )
{ static_cast<CMDLCache*>(this)->ClearCombinerFlags( nFlags ); }
void IMDLCache::DebugCombinerInfo()
{ static_cast<CMDLCache*>(this)->DebugCombinerInfo(); }

bool IStudioDataCache::VerifyHeaders( studiohdr_t *pStudioHdr )
{ return static_cast<CMDLCache*>(this)->VerifyHeaders( pStudioHdr ); }
vertexFileHeader_t * IStudioDataCache::CacheVertexData( studiohdr_t *pStudioHdr )
{ return static_cast<CMDLCache*>(this)->CacheVertexData( pStudioHdr ); }
bool IMDLCache::Connect( CreateInterfaceFn factory )
{ return static_cast<CMDLCache*>(this)->Connect( factory ); }
void IMDLCache::Disconnect()
{ static_cast<CMDLCache*>(this)->Disconnect(); }
void *IMDLCache::QueryInterface( const char *pInterfaceName )
{ return static_cast<CMDLCache*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IMDLCache::Init()
{ return static_cast<CMDLCache*>(this)->Init(); }
void IMDLCache::Shutdown()
{ static_cast<CMDLCache*>(this)->Shutdown(); }
bool IStudioDataCache::Connect( CreateInterfaceFn factory )
{ return static_cast<CMDLCache*>(this)->Connect( factory ); }
void IStudioDataCache::Disconnect()
{ static_cast<CMDLCache*>(this)->Disconnect(); }
void *IStudioDataCache::QueryInterface( const char *pInterfaceName )
{ return static_cast<CMDLCache*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IStudioDataCache::Init()
{ return static_cast<CMDLCache*>(this)->Init(); }
void IStudioDataCache::Shutdown()
{ static_cast<CMDLCache*>(this)->Shutdown(); }
