#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.tier1;

#include "togl/rendermechanism.h"
#include "shaderapi/IShaderDevice.h"
#include "IHardwareConfigInternal.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterialsystem.h"
#include "hardwareconfig.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// define this if you want to run with NVPERFHUD
//-----------------------------------------------------------------------------
//#define NVPERFHUD 1


//-----------------------------------------------------------------------------
// Uncomment this to activate the reference rasterizer
//-----------------------------------------------------------------------------
//#define USE_REFERENCE_RASTERIZER 1

//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrBase
{
public:
	// constructor, destructor
	CShaderDeviceMgrBase();
	~CShaderDeviceMgrBase();

	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );

	// Implemented by CShaderDeviceMgrDx8
	void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;
	int GetAdapterCount() const;
	void GetAdapterInfo( int nAdapter, MaterialAdapterInfo_t& info ) const;
	int GetModeCount( int nAdapter ) const;
	void GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int nMode ) const;

	// Methods of IShaderDeviceMgr
	bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pCongifuration );
	void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func );
	void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func );
	bool GetRecommendedVideoConfig( int nAdapter, KeyValues *pConfiguration );

	void AddDeviceDependentObject( IShaderDeviceDependentObject *pObject );
	void RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject );
	void InvokeDeviceLostNotifications( void );
	void InvokeDeviceResetNotifications( IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentParameters, void *pHWnd );

	// Reads in the hardware caps from the dxsupport.cfg file
	void ReadHardwareCaps( HardwareCaps_t &caps, int nDxLevel );

	// Reads in the max + preferred DX support level
	void ReadDXSupportLevels( HardwareCaps_t &caps );

	// Returns the hardware caps for a particular adapter
	const HardwareCaps_t& GetHardwareCaps( int nAdapter ) const;

	// Invokes mode change callbacks
	void InvokeModeChangeCallbacks( int screenWidth, int screenHeight );

	// Factory to return from SetMode
	static void* ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode );

	// Returns only valid dx levels
	int GetClosestActualDXLevel( int nDxLevel ) const;

protected:
	struct AdapterInfo_t
	{
		HardwareCaps_t m_ActualCaps;
	};

private:
	// Reads in the dxsupport.cfg keyvalues
	KeyValues *ReadDXSupportKeyValues();

	// Reads in ConVars + config variables
	void LoadConfig( KeyValues *pKeyValues, KeyValues *pConfiguration );

	// Loads the hardware caps, for cases in which the D3D caps lie or where we need to augment the caps
	void LoadHardwareCaps( KeyValues *pGroup, HardwareCaps_t &caps );

	// Gets the recommended configuration associated with a particular dx level
	bool GetRecommendedVideoConfig( int nAdapter, int nVendorID, int nDeviceID, KeyValues *pConfiguration );
	bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, int nVendorID, int nDeviceID, KeyValues *pConfiguration );

	// Returns the amount of video memory in bytes for a particular adapter
	int GetVidMemBytes( int nAdapter ) const;

	// Returns the physical screen desktop resolution
	void GetDesktopResolution( int *pWidth, int *pHeight, int nAdapter ) const;

	// Looks for override keyvalues in the dxsupport cfg keyvalues
	KeyValues *FindDXLevelSpecificConfig( KeyValues *pKeyValues, int nDxLevel );
	KeyValues *FindDXLevelAndVendorSpecificConfig( KeyValues *pKeyValues, int nDxLevel, int nVendorID );
	KeyValues *FindCPUSpecificConfig( KeyValues *pKeyValues, int nCPUMhz, bool bAMD );
	KeyValues *FindMemorySpecificConfig( KeyValues *pKeyValues, int nSystemRamMB );
	KeyValues *FindVidMemSpecificConfig( KeyValues *pKeyValues, int nVideoRamMB );
	KeyValues *FindCardSpecificConfig( KeyValues *pKeyValues, int nVendorID, int nDeviceID );

protected:
	// Stores adapter info for all adapters
	std::vector< AdapterInfo_t > m_Adapters;

	// Installed mode change callbacks
	std::vector< ShaderModeChangeCallbackFunc_t > m_ModeChangeCallbacks;
	std::vector< IShaderDeviceDependentObject* > m_DeviceDependentObjects;

	KeyValues *m_pDXSupport;
};


//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceBase
{
public:
	enum IPCMessage_t
	{
		RELEASE_MESSAGE		= 0x5E740DE0,
		REACQUIRE_MESSAGE	= 0x5E740DE1,
		EVICT_MESSAGE		= 0x5E740DE2,
	};

	// Implemented by CShaderDeviceDx8 / CShaderAPIDx8
	ImageFormat GetBackBufferFormat() const;
	int StencilBufferBits() const;
	bool IsAAEnabled() const;
	void ReleaseResources( bool bReleaseManagedResources = true );
	void ReacquireResources();

	// Methods of IShaderDevice
public:
	bool AddView( void* hWnd );
	void RemoveView( void* hWnd );
	void SetView( void* hWnd );
	void GetWindowSize( int& nWidth, int& nHeight ) const;

	// Methods exposed to the rest of shader api; implemented by CShaderDeviceDx8 / CShaderAPIDx8
	bool InitDevice( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );
	void ShutdownDevice();
	bool IsDeactivated() const;

public:
	// constructor, destructor
	CShaderDeviceBase();
	~CShaderDeviceBase();

	void OtherAppInitializing( bool initializing );
	void EvictManagedResourcesInternal();

	void* GetIPCHWnd();
	void SendIPCMessage( IPCMessage_t message );

protected:
	// IPC communication for multiple shaderapi apps
	void InstallWindowHook( void *hWnd );
	void RemoveWindowHook( void *hWnd );
	void SetCurrentThreadAsOwner();
	void RemoveThreadOwner();
	bool ThreadOwnsDevice() const;

	// Finds a child window
	int  FindView( void* hWnd ) const;

	int m_nAdapter;
	void *m_hWnd;
	void* m_hWndCookie;
	bool m_bInitialized : 1;
	bool m_bIsMinimized : 1;

	// The current view hwnd
	void* m_ViewHWnd;

	int	m_nWindowWidth;
	int m_nWindowHeight;
	ThreadId_t m_dwThreadId;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline void* CShaderDeviceBase::GetIPCHWnd()
{
	return m_hWndCookie;
}
