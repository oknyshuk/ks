// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetworkStringDict class to CNetworkStringDict. Included by networkstringtable.cpp.

unsigned int INetworkStringDict::Count()
{ return static_cast<CNetworkStringDict*>(this)->Count(); }
void INetworkStringDict::Purge()
{ static_cast<CNetworkStringDict*>(this)->Purge(); }
const char * INetworkStringDict::String( int index )
{ return static_cast<CNetworkStringDict*>(this)->String( index ); }
bool INetworkStringDict::IsValidIndex( int index )
{ return static_cast<CNetworkStringDict*>(this)->IsValidIndex( index ); }
int INetworkStringDict::Insert( const char *pString )
{ return static_cast<CNetworkStringDict*>(this)->Insert( pString ); }
int INetworkStringDict::Find( const char *pString )
{ return static_cast<CNetworkStringDict*>(this)->Find( pString ); }
void INetworkStringDict::UpdateDictionary( int index )
{ static_cast<CNetworkStringDict*>(this)->UpdateDictionary( index ); }
int INetworkStringDict::DictionaryIndex( int index )
{ return static_cast<CNetworkStringDict*>(this)->DictionaryIndex( index ); }
CNetworkStringTableItem & INetworkStringDict::Element( int index )
{ return static_cast<CNetworkStringDict*>(this)->Element( index ); }
const CNetworkStringTableItem & INetworkStringDict::Element( int index ) const
{ return static_cast<const CNetworkStringDict*>(this)->Element( index ); }

