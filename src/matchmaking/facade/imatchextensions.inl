// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchExtensions class to CMatchExtensions. Included by the source that defines CMatchExtensions.

void IMatchExtensions::RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface )
{ static_cast<CMatchExtensions*>(this)->RegisterExtensionInterface( szInterfaceString, pvInterface ); }
void IMatchExtensions::UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface )
{ static_cast<CMatchExtensions*>(this)->UnregisterExtensionInterface( szInterfaceString, pvInterface ); }
void * IMatchExtensions::GetRegisteredExtensionInterface( char const *szInterfaceString )
{ return static_cast<CMatchExtensions*>(this)->GetRegisteredExtensionInterface( szInterfaceString ); }
