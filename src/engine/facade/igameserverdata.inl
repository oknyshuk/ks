// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IGameServerData class to CServerRemoteAccess. Included by sv_remoteaccess.cpp.

void IGameServerData::WriteDataRequest( ra_listener_id listener, const void *buffer, int bufferSize )
{ static_cast<CServerRemoteAccess*>(this)->WriteDataRequest( listener, buffer, bufferSize ); }
int IGameServerData::ReadDataResponse( ra_listener_id listener, void *buffer, int bufferSize )
{ return static_cast<CServerRemoteAccess*>(this)->ReadDataResponse( listener, buffer, bufferSize ); }
ra_listener_id IGameServerData::GetNextListenerID( bool authConnection, const netadr_t *adr )
{ return static_cast<CServerRemoteAccess*>(this)->GetNextListenerID( authConnection, adr ); }
void IGameServerData::RegisterAdminUIID( ra_listener_id listener )
{ static_cast<CServerRemoteAccess*>(this)->RegisterAdminUIID( listener ); }

