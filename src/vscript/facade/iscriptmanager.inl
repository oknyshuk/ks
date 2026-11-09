// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IScriptManager class to CScriptManager. Included by vscript.cpp.

IScriptVM * IScriptManager::CreateVM( ScriptLanguage_t language )
{ return static_cast<CScriptManager*>(this)->CreateVM( language ); }
bool IScriptManager::Connect( CreateInterfaceFn factory )
{ return static_cast<CScriptManager*>(this)->Connect( factory ); }
void IScriptManager::Disconnect()
{ static_cast<CScriptManager*>(this)->Disconnect(); }
void *IScriptManager::QueryInterface( const char *pInterfaceName )
{ return static_cast<CScriptManager*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IScriptManager::Init()
{ return static_cast<CScriptManager*>(this)->Init(); }
void IScriptManager::Shutdown()
{ static_cast<CScriptManager*>(this)->Shutdown(); }

void IScriptManager::DestroyVM( IScriptVM *p )
{ static_cast<CScriptManager*>(this)->DestroyVM( p ); }
