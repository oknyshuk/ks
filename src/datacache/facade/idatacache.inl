import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDataCacheSection and IDataCache classes to their implementation. Included by datacache.cpp.

IDataCache * IDataCacheSection::GetSharedCache()
{ return static_cast<CDataCacheSection*>(this)->GetSharedCache(); }
const char * IDataCacheSection::GetName()
{ return static_cast<CDataCacheSection*>(this)->GetName(); }
void IDataCacheSection::SetLimits( const DataCacheLimits_t &limits )
{ static_cast<CDataCacheSection*>(this)->SetLimits( limits ); }
void IDataCacheSection::SetOptions( unsigned options )
{ static_cast<CDataCacheSection*>(this)->SetOptions( options ); }
void IDataCacheSection::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{ static_cast<CDataCacheSection*>(this)->GetStatus( pStatus, pLimits ); }
void IDataCacheSection::EnsureCapacity( unsigned nBytes, unsigned nItems )
{ static_cast<CDataCacheSection*>(this)->EnsureCapacity( nBytes, nItems ); }
bool IDataCacheSection::Add( DataCacheClientID_t clientId, const void *pItemData, unsigned size, DataCacheHandle_t *pHandle )
{ return static_cast<CDataCacheSection*>(this)->Add( clientId, pItemData, size, pHandle ); }
DataCacheHandle_t IDataCacheSection::Find( DataCacheClientID_t clientId )
{ return static_cast<CDataCacheSection*>(this)->Find( clientId ); }
DataCacheRemoveResult_t IDataCacheSection::Remove( DataCacheHandle_t handle, const void **ppItemData, unsigned *pItemSize, bool bNotify )
{ return static_cast<CDataCacheSection*>(this)->Remove( handle, ppItemData, pItemSize, bNotify ); }
bool IDataCacheSection::IsPresent( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->IsPresent( handle ); }
void * IDataCacheSection::Lock( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->Lock( handle ); }
int IDataCacheSection::Unlock( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->Unlock( handle ); }
void * IDataCacheSection::Get( DataCacheHandle_t handle, bool bFrameLock )
{ return static_cast<CDataCacheSection*>(this)->Get( handle, bFrameLock ); }
void * IDataCacheSection::GetNoTouch( DataCacheHandle_t handle, bool bFrameLock )
{ return static_cast<CDataCacheSection*>(this)->GetNoTouch( handle, bFrameLock ); }
int IDataCacheSection::BeginFrameLocking()
{ return static_cast<CDataCacheSection*>(this)->BeginFrameLocking(); }
bool IDataCacheSection::IsFrameLocking()
{ return static_cast<CDataCacheSection*>(this)->IsFrameLocking(); }
void * IDataCacheSection::FrameLock( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->FrameLock( handle ); }
int IDataCacheSection::EndFrameLocking()
{ return static_cast<CDataCacheSection*>(this)->EndFrameLocking(); }
int * IDataCacheSection::GetFrameUnlockCounterPtr()
{ return static_cast<CDataCacheSection*>(this)->GetFrameUnlockCounterPtr(); }
int IDataCacheSection::GetLockCount( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->GetLockCount( handle ); }
int IDataCacheSection::BreakLock( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->BreakLock( handle ); }
bool IDataCacheSection::Touch( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->Touch( handle ); }
bool IDataCacheSection::Age( DataCacheHandle_t handle )
{ return static_cast<CDataCacheSection*>(this)->Age( handle ); }
unsigned IDataCacheSection::Flush( bool bUnlockedOnly, bool bNotify )
{ return static_cast<CDataCacheSection*>(this)->Flush( bUnlockedOnly, bNotify ); }
unsigned IDataCacheSection::Purge( unsigned nBytes )
{ return static_cast<CDataCacheSection*>(this)->Purge( nBytes ); }
void IDataCacheSection::OutputReport( DataCacheReportType_t reportType )
{ static_cast<CDataCacheSection*>(this)->OutputReport( reportType ); }
void IDataCacheSection::UpdateSize( DataCacheHandle_t handle, unsigned int nNewSize )
{ static_cast<CDataCacheSection*>(this)->UpdateSize( handle, nNewSize ); }
void IDataCacheSection::LockMutex()
{ static_cast<CDataCacheSection*>(this)->LockMutex(); }
void IDataCacheSection::UnlockMutex()
{ static_cast<CDataCacheSection*>(this)->UnlockMutex(); }
bool IDataCacheSection::AddEx( DataCacheClientID_t clientId, const void *pItemData, unsigned size, unsigned flags, DataCacheHandle_t *pHandle )
{ return static_cast<CDataCacheSection*>(this)->AddEx( clientId, pItemData, size, flags, pHandle ); }
unsigned int IDataCacheSection::GetOptions()
{ return static_cast<CDataCacheSection*>(this)->GetOptions(); }
void IDataCacheSection::GetAndLockMultiple( void **ppData, int nCount, DataCacheHandle_t *pHandles )
{ static_cast<CDataCacheSection*>(this)->GetAndLockMultiple( ppData, nCount, pHandles ); }

void IDataCache::SetSize( int nMaxBytes )
{ static_cast<CDataCache*>(this)->SetSize( nMaxBytes ); }
void IDataCache::SetOptions( unsigned options )
{ static_cast<CDataCache*>(this)->SetOptions( options ); }
void IDataCache::SetSectionLimits( const char *pszSectionName, const DataCacheLimits_t &limits )
{ static_cast<CDataCache*>(this)->SetSectionLimits( pszSectionName, limits ); }
void IDataCache::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{ static_cast<CDataCache*>(this)->GetStatus( pStatus, pLimits ); }
IDataCacheSection * IDataCache::AddSection( IDataCacheClient *pClient, const char *pszSectionName, const DataCacheLimits_t &limits, bool bSupportFastFind )
{ return static_cast<CDataCache*>(this)->AddSection( pClient, pszSectionName, limits, bSupportFastFind ); }
void IDataCache::RemoveSection( const char *pszClientName, bool bCallFlush )
{ static_cast<CDataCache*>(this)->RemoveSection( pszClientName, bCallFlush ); }
IDataCacheSection * IDataCache::FindSection( const char *pszClientName )
{ return static_cast<CDataCache*>(this)->FindSection( pszClientName ); }
unsigned IDataCache::Purge( unsigned nBytes )
{ return static_cast<CDataCache*>(this)->Purge( nBytes ); }
unsigned IDataCache::Flush( bool bUnlockedOnly, bool bNotify )
{ return static_cast<CDataCache*>(this)->Flush( bUnlockedOnly, bNotify ); }
void IDataCache::OutputReport( DataCacheReportType_t reportType, const char *pszSection )
{ static_cast<CDataCache*>(this)->OutputReport( reportType, pszSection ); }
int IDataCache::GetSectionCount()
{ return static_cast<CDataCache*>(this)->GetSectionCount(); }
const char * IDataCache::GetSectionName( int iIndex )
{ return static_cast<CDataCache*>(this)->GetSectionName( iIndex ); }
bool IDataCache::Connect( CreateInterfaceFn factory )
{ return static_cast<CDataCache*>(this)->Connect( factory ); }
void IDataCache::Disconnect()
{ static_cast<CDataCache*>(this)->Disconnect(); }
void *IDataCache::QueryInterface( const char *pInterfaceName )
{ return static_cast<CDataCache*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IDataCache::Init()
{ return static_cast<CDataCache*>(this)->Init(); }
void IDataCache::Shutdown()
{ static_cast<CDataCache*>(this)->Shutdown(); }
