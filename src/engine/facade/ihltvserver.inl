// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IHLTVServer class to CHLTVServer. Included by hltvserver.cpp.

IServer * IHLTVServer::GetBaseServer( void )
{ return static_cast<CHLTVServer*>(this)->GetBaseServer(); }
IHLTVDirector * IHLTVServer::GetDirector( void )
{ return static_cast<CHLTVServer*>(this)->GetDirector(); }
int IHLTVServer::GetHLTVSlot( void )
{ return static_cast<CHLTVServer*>(this)->GetHLTVSlot(); }
float IHLTVServer::GetOnlineTime( void )
{ return static_cast<CHLTVServer*>(this)->GetOnlineTime(); }
void IHLTVServer::GetLocalStats( int &proxies, int &slots, int &specs )
{ static_cast<CHLTVServer*>(this)->GetLocalStats( proxies, slots, specs ); }
void IHLTVServer::GetGlobalStats( int &proxies, int &slots, int &specs )
{ static_cast<CHLTVServer*>(this)->GetGlobalStats( proxies, slots, specs ); }
const netadr_s * IHLTVServer::GetRelayAddress( void )
{ return static_cast<CHLTVServer*>(this)->GetRelayAddress(); }
bool IHLTVServer::IsMasterProxy( void )
{ return static_cast<CHLTVServer*>(this)->IsMasterProxy(); }
bool IHLTVServer::IsDemoPlayback( void )
{ return static_cast<CHLTVServer*>(this)->IsDemoPlayback(); }
void IHLTVServer::BroadcastEvent( IGameEvent *event )
{ static_cast<CHLTVServer*>(this)->BroadcastEvent( event ); }
bool IHLTVServer::IsRecording()
{ return static_cast<CHLTVServer*>(this)->IsRecording(); }
const char* IHLTVServer::GetRecordingDemoFilename()
{ return static_cast<CHLTVServer*>(this)->GetRecordingDemoFilename(); }
void IHLTVServer::StartAutoRecording()
{ static_cast<CHLTVServer*>(this)->StartAutoRecording(); }
void IHLTVServer::StopRecording( const CGameInfo *pGameInfo )
{ static_cast<CHLTVServer*>(this)->StopRecording( pGameInfo ); }

