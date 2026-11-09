// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchTitleGameSettingsMgr class to CMatchTitleGameSettingsMgr. Included by the source that defines CMatchTitleGameSettingsMgr.

void IMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->ExtendServerDetails( pDetails, pRequest ); }
void IMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->ExtendLobbyDetailsTemplate( pDetails, szReason, pFullSettings ); }
void IMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings, char const *szReason )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->InitializeGameSettings( pSettings, szReason ); }
void IMatchTitleGameSettingsMgr::SetBspnameFromMapgroup( KeyValues *pSettings )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->SetBspnameFromMapgroup( pSettings ); }
void IMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->ExtendGameSettingsUpdateKeys( pSettings, pUpdateDeleteKeys ); }
void IMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->ExecuteCommand( pCommand, pSessionSystemData, pSettings, ppPlayersUpdated ); }
void IMatchTitleGameSettingsMgr::PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{ static_cast<CMatchTitleGameSettingsMgr*>(this)->PrepareLobbyForGame( pSettings, ppPlayersUpdated ); }
