// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IResourceAccessControl class to CResourceAccessControl. Included by resourceaccesscontrol.cpp.

ResourceList_t IResourceAccessControl::CreateResourceList( const char *pDebugName )
{ return static_cast<CResourceAccessControl*>(this)->CreateResourceList( pDebugName ); }
void IResourceAccessControl::DestroyAllResourceLists()
{ static_cast<CResourceAccessControl*>(this)->DestroyAllResourceLists(); }
void IResourceAccessControl::AddResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName )
{ static_cast<CResourceAccessControl*>(this)->AddResource( hResourceList, nType, pResourceName ); }
void IResourceAccessControl::LimitAccess( ResourceList_t hResourceList )
{ static_cast<CResourceAccessControl*>(this)->LimitAccess( hResourceList ); }
bool IResourceAccessControl::IsAccessAllowed( ResourceTypeOld_t nType, const char *pResource )
{ return static_cast<CResourceAccessControl*>(this)->IsAccessAllowed( nType, pResource ); }
bool IResourceAccessControl::Connect( CreateInterfaceFn factory )
{ return static_cast<CResourceAccessControl*>(this)->Connect( factory ); }
void IResourceAccessControl::Disconnect()
{ static_cast<CResourceAccessControl*>(this)->Disconnect(); }
void *IResourceAccessControl::QueryInterface( const char *pInterfaceName )
{ return static_cast<CResourceAccessControl*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IResourceAccessControl::Init()
{ return static_cast<CResourceAccessControl*>(this)->Init(); }
void IResourceAccessControl::Shutdown()
{ static_cast<CResourceAccessControl*>(this)->Shutdown(); }
