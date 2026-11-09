// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchSession class to CMatchSessionOfflineCustom. Included by the source that defines CMatchSessionOfflineCustom.

KeyValues * IMatchSession::GetSessionSystemData()
{ return static_cast<CMatchSessionOfflineCustom*>(this)->GetSessionSystemData(); }
KeyValues * IMatchSession::GetSessionSettings()
{ return static_cast<CMatchSessionOfflineCustom*>(this)->GetSessionSettings(); }
void IMatchSession::UpdateSessionSettings( KeyValues *pSettings )
{ static_cast<CMatchSessionOfflineCustom*>(this)->UpdateSessionSettings( pSettings ); }
void IMatchSession::Command( KeyValues *pCommand )
{ static_cast<CMatchSessionOfflineCustom*>(this)->Command( pCommand ); }
uint64 IMatchSession::GetSessionID()
{ return static_cast<CMatchSessionOfflineCustom*>(this)->GetSessionID(); }
void IMatchSession::UpdateTeamProperties( KeyValues *pTeamProperties )
{ static_cast<CMatchSessionOfflineCustom*>(this)->UpdateTeamProperties( pTeamProperties ); }
