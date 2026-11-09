// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IEngine class to CEngine. Included by sys_engine.cpp.

bool IEngine::Load( bool dedicated, const char *rootdir )
{ return static_cast<CEngine*>(this)->Load( dedicated, rootdir ); }
void IEngine::Unload( void )
{ static_cast<CEngine*>(this)->Unload(); }
void IEngine::SetNextState( EngineState_t iNextState )
{ static_cast<CEngine*>(this)->SetNextState( iNextState ); }
IEngine::EngineState_t IEngine::GetState( void )
{ return static_cast<CEngine*>(this)->GetState(); }
void IEngine::Frame( void )
{ static_cast<CEngine*>(this)->Frame(); }
float IEngine::GetFrameTime( void )
{ return static_cast<CEngine*>(this)->GetFrameTime(); }
float IEngine::GetCurTime( void )
{ return static_cast<CEngine*>(this)->GetCurTime(); }
int IEngine::GetQuitting( void )
{ return static_cast<CEngine*>(this)->GetQuitting(); }
void IEngine::SetQuitting( int quittype )
{ static_cast<CEngine*>(this)->SetQuitting( quittype ); }

