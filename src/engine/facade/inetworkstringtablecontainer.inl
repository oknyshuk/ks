// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetworkStringTableContainer class to CNetworkStringTableContainer. Included by networkstringtable.cpp.

INetworkStringTable * INetworkStringTableContainer::CreateStringTable( const char *tableName, int maxentries, int userdatafixedsize, int userdatanetworkbits, int flags )
{ return static_cast<CNetworkStringTableContainer*>(this)->CreateStringTable( tableName, maxentries, userdatafixedsize, userdatanetworkbits, flags ); }
void INetworkStringTableContainer::RemoveAllTables( void )
{ static_cast<CNetworkStringTableContainer*>(this)->RemoveAllTables(); }
INetworkStringTable * INetworkStringTableContainer::FindTable( const char *tableName ) const
{ return static_cast<const CNetworkStringTableContainer*>(this)->FindTable( tableName ); }
INetworkStringTable * INetworkStringTableContainer::GetTable( TABLEID stringTable ) const
{ return static_cast<const CNetworkStringTableContainer*>(this)->GetTable( stringTable ); }
int INetworkStringTableContainer::GetNumTables( void ) const
{ return static_cast<const CNetworkStringTableContainer*>(this)->GetNumTables(); }
void INetworkStringTableContainer::SetAllowClientSideAddString( INetworkStringTable *table, bool bAllowClientSideAddString )
{ static_cast<CNetworkStringTableContainer*>(this)->SetAllowClientSideAddString( table, bAllowClientSideAddString ); }
void INetworkStringTableContainer::CreateDictionary( char const *pchMapName )
{ static_cast<CNetworkStringTableContainer*>(this)->CreateDictionary( pchMapName ); }

