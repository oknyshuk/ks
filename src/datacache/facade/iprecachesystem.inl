// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPrecacheSystem class to CPrecacheSystem. Included by precachesystem.cpp.

void IPrecacheSystem::Cache( PrecacheHandler_t pPrecacheHandler, PrecacheSystem_t nSystem, const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList )
{ static_cast<CPrecacheSystem*>(this)->Cache( pPrecacheHandler, nSystem, pName, bPrecache, hResourceList, bBuildResourceList ); }
void IPrecacheSystem::UncacheAll( PrecacheHandler_t pPrecacheHandler )
{ static_cast<CPrecacheSystem*>(this)->UncacheAll( pPrecacheHandler ); }
void IPrecacheSystem::Register( CBaseResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem )
{ static_cast<CPrecacheSystem*>(this)->Register( pResourcePrecacherFirst, nSystem ); }
void IPrecacheSystem::LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName )
{ static_cast<CPrecacheSystem*>(this)->LimitResourceAccess( nSystem, pName ); }
void IPrecacheSystem::EndLimitedResourceAccess()
{ static_cast<CPrecacheSystem*>(this)->EndLimitedResourceAccess(); }
bool IPrecacheSystem::Connect( CreateInterfaceFn factory )
{ return static_cast<CPrecacheSystem*>(this)->Connect( factory ); }
void IPrecacheSystem::Disconnect()
{ static_cast<CPrecacheSystem*>(this)->Disconnect(); }
void *IPrecacheSystem::QueryInterface( const char *pInterfaceName )
{ return static_cast<CPrecacheSystem*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IPrecacheSystem::Init()
{ return static_cast<CPrecacheSystem*>(this)->Init(); }
void IPrecacheSystem::Shutdown()
{ static_cast<CPrecacheSystem*>(this)->Shutdown(); }
