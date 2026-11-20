import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchFramework class to CMatchFramework. Included by the source that defines CMatchFramework.

bool IMatchFramework::Connect( CreateInterfaceFn factory )
{ return static_cast<CMatchFramework*>(this)->Connect( factory ); }
void IMatchFramework::Disconnect()
{ static_cast<CMatchFramework*>(this)->Disconnect(); }
void *IMatchFramework::QueryInterface( const char *pInterfaceName )
{ return static_cast<CMatchFramework*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IMatchFramework::Init()
{ return static_cast<CMatchFramework*>(this)->Init(); }
void IMatchFramework::Shutdown()
{ static_cast<CMatchFramework*>(this)->Shutdown(); }

void IMatchFramework::RunFrame()
{ static_cast<CMatchFramework*>(this)->RunFrame(); }
IMatchExtensions * IMatchFramework::GetMatchExtensions()
{ return static_cast<CMatchFramework*>(this)->GetMatchExtensions(); }
IMatchEventsSubscription * IMatchFramework::GetEventsSubscription()
{ return static_cast<CMatchFramework*>(this)->GetEventsSubscription(); }
IMatchTitle * IMatchFramework::GetMatchTitle()
{ return static_cast<CMatchFramework*>(this)->GetMatchTitle(); }
IMatchSession * IMatchFramework::GetMatchSession()
{ return static_cast<CMatchFramework*>(this)->GetMatchSession(); }
IMatchNetworkMsgController * IMatchFramework::GetMatchNetworkMsgController()
{ return static_cast<CMatchFramework*>(this)->GetMatchNetworkMsgController(); }
IMatchSystem * IMatchFramework::GetMatchSystem()
{ return static_cast<CMatchFramework*>(this)->GetMatchSystem(); }
void IMatchFramework::ApplySettings( KeyValues* keyValues )
{ static_cast<CMatchFramework*>(this)->ApplySettings( keyValues ); }
void IMatchFramework::CreateSession( KeyValues *pSettings )
{ static_cast<CMatchFramework*>(this)->CreateSession( pSettings ); }
void IMatchFramework::MatchSession( KeyValues *pSettings )
{ static_cast<CMatchFramework*>(this)->MatchSession( pSettings ); }
void IMatchFramework::CloseSession()
{ static_cast<CMatchFramework*>(this)->CloseSession(); }
bool IMatchFramework::IsOnlineGame()
{ return static_cast<CMatchFramework*>(this)->IsOnlineGame(); }
void IMatchFramework::UpdateTeamProperties( KeyValues *pTeamProperties )
{ static_cast<CMatchFramework*>(this)->UpdateTeamProperties( pTeamProperties ); }
