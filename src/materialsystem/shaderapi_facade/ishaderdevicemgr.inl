import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IShaderDeviceMgr facade class to the device manager implementation.
// Included by the TU that defines the implementation, which must declare ShaderDeviceMgrImpl() returning it.

// IShaderDeviceMgr
bool IShaderDeviceMgr::Connect( CreateInterfaceFn factory )
{ return ShaderDeviceMgrImpl().Connect( factory ); }
void IShaderDeviceMgr::Disconnect()
{ ShaderDeviceMgrImpl().Disconnect(); }
void * IShaderDeviceMgr::QueryInterface( const char *pInterfaceName )
{ return ShaderDeviceMgrImpl().QueryInterface( pInterfaceName ); }
InitReturnVal_t IShaderDeviceMgr::Init()
{ return ShaderDeviceMgrImpl().Init(); }
void IShaderDeviceMgr::Shutdown()
{ ShaderDeviceMgrImpl().Shutdown(); }
int IShaderDeviceMgr::GetAdapterCount() const
{ return ShaderDeviceMgrImpl().GetAdapterCount(); }
void IShaderDeviceMgr::GetAdapterInfo( int nAdapter, MaterialAdapterInfo_t& info ) const
{ ShaderDeviceMgrImpl().GetAdapterInfo( nAdapter, info ); }
bool IShaderDeviceMgr::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pConfiguration )
{ return ShaderDeviceMgrImpl().GetRecommendedConfigurationInfo( nAdapter, nDXLevel, pConfiguration ); }
int IShaderDeviceMgr::GetModeCount( int nAdapter ) const
{ return ShaderDeviceMgrImpl().GetModeCount( nAdapter ); }
void IShaderDeviceMgr::GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int nMode ) const
{ ShaderDeviceMgrImpl().GetModeInfo( pInfo, nAdapter, nMode ); }
void IShaderDeviceMgr::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const
{ ShaderDeviceMgrImpl().GetCurrentModeInfo( pInfo, nAdapter ); }
bool IShaderDeviceMgr::SetAdapter( int nAdapter, int nFlags )
{ return ShaderDeviceMgrImpl().SetAdapter( nAdapter, nFlags ); }
CreateInterfaceFn IShaderDeviceMgr::SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode )
{ return ShaderDeviceMgrImpl().SetMode( hWnd, nAdapter, mode ); }
void IShaderDeviceMgr::AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{ ShaderDeviceMgrImpl().AddModeChangeCallback( func ); }
void IShaderDeviceMgr::RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func )
{ ShaderDeviceMgrImpl().RemoveModeChangeCallback( func ); }
bool IShaderDeviceMgr::GetRecommendedVideoConfig( int nAdapter, KeyValues *pConfiguration )
{ return ShaderDeviceMgrImpl().GetRecommendedVideoConfig( nAdapter, pConfiguration ); }
void IShaderDeviceMgr::AddDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{ ShaderDeviceMgrImpl().AddDeviceDependentObject( pObject ); }
void IShaderDeviceMgr::RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject )
{ ShaderDeviceMgrImpl().RemoveDeviceDependentObject( pObject ); }

