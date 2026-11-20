import ks.tier1.createinterface;
import ks.tier1.interfaces;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMaterialProxyFactory class to CMaterialProxyFactory, its one implementation.
// Included by engine/materialproxyfactory.cpp.

IMaterialProxy * IMaterialProxyFactory::CreateProxy( const char *proxyName )
{ return static_cast<CMaterialProxyFactory *>( this )->CreateProxy( proxyName ); }
void IMaterialProxyFactory::DeleteProxy( IMaterialProxy *pProxy )
{ static_cast<CMaterialProxyFactory *>( this )->DeleteProxy( pProxy ); }
CreateInterfaceFn IMaterialProxyFactory::GetFactory()
{ return static_cast<CMaterialProxyFactory *>( this )->GetFactory(); }
