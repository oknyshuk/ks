// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IServer class to CBaseServer.
// Included by netdispatch.cpp.

int IServer::GetNumClients( void ) const
{ return static_cast<const CBaseServer*>(this)->GetNumClients(); }
int IServer::GetNumProxies( void ) const
{ return static_cast<const CBaseServer*>(this)->GetNumProxies(); }
int IServer::GetNumFakeClients() const
{ return static_cast<const CBaseServer*>(this)->GetNumFakeClients(); }
int IServer::GetMaxClients( void ) const
{ return static_cast<const CBaseServer*>(this)->GetMaxClients(); }
IClient * IServer::GetClient( int index )
{ return static_cast<CBaseServer*>(this)->GetClient( index ); }
int IServer::GetClientCount() const
{ return static_cast<const CBaseServer*>(this)->GetClientCount(); }
int IServer::GetUDPPort( void ) const
{ return static_cast<const CBaseServer*>(this)->GetUDPPort(); }
float IServer::GetTime( void ) const
{ return static_cast<const CBaseServer*>(this)->GetTime(); }
int IServer::GetTick( void ) const
{ return static_cast<const CBaseServer*>(this)->GetTick(); }
float IServer::GetTickInterval( void ) const
{ return static_cast<const CBaseServer*>(this)->GetTickInterval(); }
float IServer::GetTimescale( void ) const
{ return static_cast<const CBaseServer*>(this)->GetTimescale(); }
const char * IServer::GetName( void ) const
{ return static_cast<const CBaseServer*>(this)->GetName(); }
const char * IServer::GetMapName( void ) const
{ return static_cast<const CBaseServer*>(this)->GetMapName(); }
int IServer::GetSpawnCount( void ) const
{ return static_cast<const CBaseServer*>(this)->GetSpawnCount(); }
int IServer::GetNumClasses( void ) const
{ return static_cast<const CBaseServer*>(this)->GetNumClasses(); }
int IServer::GetClassBits( void ) const
{ return static_cast<const CBaseServer*>(this)->GetClassBits(); }
void IServer::GetNetStats( float &avgIn, float &avgOut )
{ static_cast<CBaseServer*>(this)->GetNetStats( avgIn, avgOut ); }
int IServer::GetNumPlayers()
{ return static_cast<CBaseServer*>(this)->GetNumPlayers(); }
bool IServer::GetPlayerInfo( int nClientIndex, player_info_t *pinfo )
{ return static_cast<CBaseServer*>(this)->GetPlayerInfo( nClientIndex, pinfo ); }
bool IServer::IsActive( void ) const
{ return static_cast<const CBaseServer*>(this)->IsActive(); }
bool IServer::IsLoading( void ) const
{ return static_cast<const CBaseServer*>(this)->IsLoading(); }
bool IServer::IsDedicated( void ) const
{ return static_cast<const CBaseServer*>(this)->IsDedicated(); }
bool IServer::IsPaused( void ) const
{ return static_cast<const CBaseServer*>(this)->IsPaused(); }
bool IServer::IsMultiplayer( void ) const
{ return static_cast<const CBaseServer*>(this)->IsMultiplayer(); }
bool IServer::IsPausable() const
{ return static_cast<const CBaseServer*>(this)->IsPausable(); }
bool IServer::IsHLTV() const
{ return static_cast<const CBaseServer*>(this)->IsHLTV(); }
bool IServer::IsReplay() const
{ return static_cast<const CBaseServer*>(this)->IsReplay(); }
const char * IServer::GetPassword() const
{ return static_cast<const CBaseServer*>(this)->GetPassword(); }
void IServer::SetPaused( bool paused )
{ static_cast<CBaseServer*>(this)->SetPaused( paused ); }
void IServer::SetTimescale( float flTimescale )
{ static_cast<CBaseServer*>(this)->SetTimescale( flTimescale ); }
void IServer::SetPassword( const char *password )
{ static_cast<CBaseServer*>(this)->SetPassword( password ); }
void IServer::BroadcastMessage( INetMessage &msg, bool onlyActive, bool reliable )
{ static_cast<CBaseServer*>(this)->BroadcastMessage( msg, onlyActive, reliable ); }
void IServer::BroadcastMessage( INetMessage &msg, IRecipientFilter &filter )
{ static_cast<CBaseServer*>(this)->BroadcastMessage( msg, filter ); }
void IServer::DisconnectClient( IClient *client, const char *reason )
{ static_cast<CBaseServer*>(this)->DisconnectClient( client, reason ); }

