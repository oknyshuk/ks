import ks.core.threadtools;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchSessionInternal class to CMatchSessionOfflineCustom. Included by the source that defines CMatchSessionOfflineCustom.

void IMatchSessionInternal::OnEvent( KeyValues *pEvent )
{ static_cast<CMatchSessionOfflineCustom*>(this)->OnEvent( pEvent ); }
void IMatchSessionInternal::Update()
{ static_cast<CMatchSessionOfflineCustom*>(this)->Update(); }
void IMatchSessionInternal::Destroy()
{ static_cast<CMatchSessionOfflineCustom*>(this)->Destroy(); }
void IMatchSessionInternal::DebugPrint()
{ static_cast<CMatchSessionOfflineCustom*>(this)->DebugPrint(); }
bool IMatchSessionInternal::IsAnotherSessionJoinable( char const *pszAnotherSessionInfo )
{ return static_cast<CMatchSessionOfflineCustom*>(this)->IsAnotherSessionJoinable( pszAnotherSessionInfo ); }
