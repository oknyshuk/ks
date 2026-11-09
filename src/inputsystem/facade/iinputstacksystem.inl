// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IInputStackSystem class to CInputStackSystem. Included by inputstacksystem.cpp.

InputContextHandle_t IInputStackSystem::PushInputContext()
{ return static_cast<CInputStackSystem*>(this)->PushInputContext(); }
void IInputStackSystem::PopInputContext()
{ static_cast<CInputStackSystem*>(this)->PopInputContext(); }
void IInputStackSystem::EnableInputContext( InputContextHandle_t hContext, bool bEnable )
{ static_cast<CInputStackSystem*>(this)->EnableInputContext( hContext, bEnable ); }
void IInputStackSystem::SetCursorVisible( InputContextHandle_t hContext, bool bVisible )
{ static_cast<CInputStackSystem*>(this)->SetCursorVisible( hContext, bVisible ); }
void IInputStackSystem::SetCursorIcon( InputContextHandle_t hContext, InputCursorHandle_t hCursor )
{ static_cast<CInputStackSystem*>(this)->SetCursorIcon( hContext, hCursor ); }
void IInputStackSystem::SetMouseCapture( InputContextHandle_t hContext, bool bEnable )
{ static_cast<CInputStackSystem*>(this)->SetMouseCapture( hContext, bEnable ); }
void IInputStackSystem::SetCursorPosition( InputContextHandle_t hContext, int x, int y )
{ static_cast<CInputStackSystem*>(this)->SetCursorPosition( hContext, x, y ); }
bool IInputStackSystem::IsTopmostEnabledContext( InputContextHandle_t hContext ) const
{ return static_cast<const CInputStackSystem*>(this)->IsTopmostEnabledContext( hContext ); }
bool IInputStackSystem::Connect( CreateInterfaceFn factory )
{ return static_cast<CInputStackSystem*>(this)->Connect( factory ); }
void IInputStackSystem::Disconnect()
{ static_cast<CInputStackSystem*>(this)->Disconnect(); }
void *IInputStackSystem::QueryInterface( const char *pInterfaceName )
{ return static_cast<CInputStackSystem*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IInputStackSystem::Init()
{ return static_cast<CInputStackSystem*>(this)->Init(); }
void IInputStackSystem::Shutdown()
{ static_cast<CInputStackSystem*>(this)->Shutdown(); }
const AppSystemInfo_t *IInputStackSystem::GetDependencies()
{ return static_cast<CInputStackSystem*>(this)->GetDependencies(); }

