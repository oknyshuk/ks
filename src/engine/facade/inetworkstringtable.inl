// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetworkStringTable class to CNetworkStringTable. Included by networkstringtable.cpp.

const char * INetworkStringTable::GetTableName( void ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetTableName(); }
TABLEID INetworkStringTable::GetTableId( void ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetTableId(); }
int INetworkStringTable::GetNumStrings( void ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetNumStrings(); }
int INetworkStringTable::GetMaxStrings( void ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetMaxStrings(); }
int INetworkStringTable::GetEntryBits( void ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetEntryBits(); }
void INetworkStringTable::SetTick( int tick )
{ static_cast<CNetworkStringTable*>(this)->SetTick( tick ); }
bool INetworkStringTable::ChangedSinceTick( int tick ) const
{ return static_cast<const CNetworkStringTable*>(this)->ChangedSinceTick( tick ); }
int INetworkStringTable::AddString( bool bIsServer, const char *value, int length, const void *userdata )
{ return static_cast<CNetworkStringTable*>(this)->AddString( bIsServer, value, length, userdata ); }
const char * INetworkStringTable::GetString( int stringNumber ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetString( stringNumber ); }
void INetworkStringTable::SetStringUserData( int stringNumber, int length, const void *userdata )
{ static_cast<CNetworkStringTable*>(this)->SetStringUserData( stringNumber, length, userdata ); }
const void * INetworkStringTable::GetStringUserData( int stringNumber, int *length ) const
{ return static_cast<const CNetworkStringTable*>(this)->GetStringUserData( stringNumber, length ); }
int INetworkStringTable::FindStringIndex( char const *string )
{ return static_cast<CNetworkStringTable*>(this)->FindStringIndex( string ); }
void INetworkStringTable::SetStringChangedCallback( void *object, pfnStringChanged changeFunc )
{ static_cast<CNetworkStringTable*>(this)->SetStringChangedCallback( object, changeFunc ); }

