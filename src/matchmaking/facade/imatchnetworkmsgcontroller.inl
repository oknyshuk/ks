// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchNetworkMsgController class to CMatchNetworkMsgControllerBase. Included by the source that defines CMatchNetworkMsgControllerBase.

MM_QOS_t IMatchNetworkMsgController::GetQOS()
{ return static_cast<CMatchNetworkMsgControllerBase*>(this)->GetQOS(); }
KeyValues * IMatchNetworkMsgController::GetActiveServerGameDetails( KeyValues *pRequest )
{ return static_cast<CMatchNetworkMsgControllerBase*>(this)->GetActiveServerGameDetails( pRequest ); }
KeyValues * IMatchNetworkMsgController::UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply )
{ return static_cast<CMatchNetworkMsgControllerBase*>(this)->UnpackGameDetailsFromQOS( pvQosReply ); }
void IMatchNetworkMsgController::PackageGameDetailsForQOS( KeyValues *pSettings, ks::byte_writer &buf )
{ static_cast<CMatchNetworkMsgControllerBase*>(this)->PackageGameDetailsForQOS( pSettings, buf ); }
KeyValues * IMatchNetworkMsgController::PackageGameDetailsForReservation( KeyValues *pSettings )
{ return static_cast<CMatchNetworkMsgControllerBase*>(this)->PackageGameDetailsForReservation( pSettings ); }
