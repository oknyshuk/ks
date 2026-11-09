#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

import std;
import ks.tier1;
#include "shaderdevicebase.h"
#include "shaderapidx8_global.h"
#include "materialsystem/imaterialsystem.h"

//-----------------------------------------------------------------------------
// Describes which D3DDEVTYPE to use
//-----------------------------------------------------------------------------
#define DX8_DEVTYPE	D3DDEVTYPE_HAL
    
// PC:    By default, PIX profiling is explicitly disallowed using the D3DPERF_SetOptions(1) API on PC
// X360:  PIX_INSTRUMENTATION will only generate PIX events in RELEASE builds on 360
// Also be sure to enable PIX_ENABLE in imaterialsystem.h
// Uncomment to use PIX instrumentation:
//#define	PIX_INSTRUMENTATION

inline constexpr auto MAX_PIX_ERRORS = 3;


//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrDx8 : public CShaderDeviceMgrBase
{
	typedef CShaderDeviceMgrBase BaseClass;

public:
	// constructor, destructor
	CShaderDeviceMgrDx8();
	~CShaderDeviceMgrDx8();

	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	InitReturnVal_t Init();
	void Shutdown();

	// Methods of IShaderDevice
	int	 GetAdapterCount() const;
	void GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;
	int	 GetModeCount( int nAdapter ) const;
	void GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int mode ) const;
	void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;
	bool SetAdapter( int nAdapter, int nFlags );
	CreateInterfaceFn SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );

	// Determines hardware caps from D3D
	bool ComputeCapsFromD3D( HardwareCaps_t *pCaps, int nAdapter );

	// Forces caps to a specific dx level
	void ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps );

	// Validates the mode...
	bool ValidateMode( int nAdapter, const ShaderDeviceInfo_t &info ) const;

	// Returns the amount of video memory in bytes for a particular adapter
	int GetVidMemBytes( int nAdapter ) const;

	// Returns the screen resolution
	void GetDesktopResolution( int *pWidth, int *pHeight, int nAdapter ) const;

	[[gnu::always_inline]] inline IDirect3D9 *D3D() const
	{ 
		return m_pD3D; 
	}


protected:
	// Determine capabilities
	bool DetermineHardwareCaps( );

private:
	// Initialize adapter information
	void InitAdapterInfo();

	// Code to detect support for texture border mode (not a simple caps check)
	void CheckBorderColorSupport( HardwareCaps_t *pCaps, int nAdapter );

	// Vendor-dependent code to detect support for various flavors of shadow mapping
	void CheckVendorDependentShadowMappingSupport( HardwareCaps_t *pCaps, int nAdapter );

	// Vendor-dependent code to detect Alpha To Coverage Backdoors
	void CheckVendorDependentAlphaToCoverage( HardwareCaps_t *pCaps, int nAdapter );

	// Vendor-dependent code to detect support for optimal depth buffer rt resolve
	void CheckVendorDependentDepthResolveSupport( HardwareCaps_t *pCaps, int nAdapter );

	// Compute the effective DX support level based on all the other caps
	void ComputeDXSupportLevel( HardwareCaps_t &caps );

	// Used to enumerate adapters, attach to windows
	IDirect3D9 *m_pD3D;

	bool m_bAdapterInfoIntialized : 1;
};

extern CShaderDeviceMgrDx8* g_pShaderDeviceMgrDx8;


//-----------------------------------------------------------------------------
// IDirect3D accessor
//-----------------------------------------------------------------------------

inline IDirect3D9* D3D()  
{
	return g_pShaderDeviceMgrDx8->D3D();
}


inline constexpr auto NUM_FRAME_SYNC_QUERIES = 2;
inline constexpr auto NUM_FRAME_SYNC_FRAMES_LATENCY = 0;

//-----------------------------------------------------------------------------
// The Dx8implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceDx8 : public CShaderDeviceBase
{
	// Methods of IShaderDevice
public:
	bool IsUsingGraphics() const;
	ImageFormat GetBackBufferFormat() const;
	void GetBackBufferDimensions( int& width, int& height ) const;
	const AspectRatioInfo_t &GetAspectRatioInfo( void ) const;
	void Present();
	IShaderBuffer* CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer *pBuffer );
	void DestroyVertexShader( VertexShaderHandle_t hShader );
	GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer );
	void DestroyGeometryShader( GeometryShaderHandle_t hShader );
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer );
	void DestroyPixelShader( PixelShaderHandle_t hShader );
	void ReleaseResources( bool bReleaseManagedResources = true );
	void ReacquireResources();
	IMesh* CreateStaticMesh( VertexFormat_t format, const char *pBudgetGroup, IMaterial * pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void DestroyStaticMesh( IMesh* mesh );
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer );
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer );
	IVertexBuffer *GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	IIndexBuffer *GetDynamicIndexBuffer();
	void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled );
	void SpewDriverInfo() const;
	int GetCurrentAdapter() const;
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo = nullptr );
	void RefreshFrontBufferNonInteractive();

	// Alternative method for ib/vs
	// NOTE: If this works, remove GetDynamicVertexBuffer/IndexBuffer

	// Methods of CShaderDeviceBase
public:
	bool InitDevice( void* hWnd, int nAdapter, const ShaderDeviceInfo_t &info );
	void ShutdownDevice();
	bool IsDeactivated() const;

	// Other public methods
public:
	// constructor, destructor
	CShaderDeviceDx8();
	~CShaderDeviceDx8();

	// Call this when another app is initializing or finished initializing
	void OtherAppInitializing( bool initializing );
	
	// This handles any events queued because they were called outside of the owning thread
	void HandleThreadEvent( uint32 threadEvent );

	// FIXME: Make private
	// Which device are we using?
	UINT		m_DisplayAdapter;
	D3DDEVTYPE	m_DeviceType;

protected:
	enum DeviceState_t
	{
		DEVICE_STATE_OK = 0,
		DEVICE_STATE_OTHER_APP_INIT,
		DEVICE_STATE_LOST_DEVICE,
		DEVICE_STATE_NEEDS_RESET,
	};

	struct NonInteractiveRefreshState_t
	{
		IDirect3DVertexShader9 *m_pVertexShader;
		IDirect3DPixelShader9 *m_pPixelShader;
		IDirect3DPixelShader9 *m_pPixelShaderStartup;
		IDirect3DPixelShader9 *m_pPixelShaderStartupPass2;
		IDirect3DVertexDeclaration9 *m_pVertexDecl;
		ShaderNonInteractiveInfo_t m_Info;
		MaterialNonInteractiveMode_t m_Mode;
		float m_flLastPacifierTime;
		int m_nPacifierFrame;

		float m_flStartTime;
		float m_flLastPresentTime;
		float m_flPeakDt;
		float m_flTotalDt;
		int m_nSamples;
		int m_nCountAbove66;
	};

protected:
	// Creates the D3D Device
	bool CreateD3DDevice( void* pHWnd, int nAdapter, const ShaderDeviceInfo_t &info );

	// Actually creates the D3D Device once the present parameters are set up
	IDirect3DDevice9* InvokeCreateDevice( void* hWnd, int nAdapter, DWORD deviceCreationFlags );

	// Checks for CreateQuery support
	void DetectQuerySupport( IDirect3DDevice9* pD3DDevice );

	// Computes the presentation parameters
	void SetPresentParameters( void* hWnd, int nAdapter, const ShaderDeviceInfo_t &info, bool bSetSymbolsOnly = false );
	void CalcBackBufferDimensions( const ShaderDisplayMode_t &mode, const ShaderDeviceInfo_t &info, int *pBackBufferWidth, int *pBackBufferHeight );

	// Computes the supersample flags
	D3DMULTISAMPLE_TYPE ComputeMultisampleType( int nSampleCount );

	// Is the device active?
	bool IsActive() const;

	// Try to reset the device, returned true if it succeeded
	bool TryDeviceReset();

	// Queue up the fact that the device was lost
	void MarkDeviceLost();

	// Deals with lost devices
	void CheckDeviceLost( bool bOtherAppInitializing );

	// Changes the window size
	bool ResizeWindow( const ShaderDeviceInfo_t &info );

	// Deals with the frame synching object
	void AllocFrameSyncObjects( void );
	void FreeFrameSyncObjects( void );

	// Alloc and free objects that are necessary for frame syncing
	void AllocFrameSyncTextureObject();
	void FreeFrameSyncTextureObject();

	// Alloc and free objects necessary for noninteractive frame refresh on the x360
	bool AllocNonInteractiveRefreshObjects();
	void FreeNonInteractiveRefreshObjects();
	bool BuildStaticShader(	bool bVertexShader, void **ppShader, const char *shaderName,
							const char *strShaderProgram, const DWORD *shaderData, unsigned int shaderSize );

	// FIXME: This is for backward compat; I still haven't solved a way of decoupling this
	bool OnAdapterSet();
	void ResetRenderState( bool bFullReset = true );

	// For measuring if we meed TCR 022 on the XBox (refreshing often enough)
	void UpdatePresentStats();

	bool InNonInteractiveMode() const;

	void ReacquireResourcesInternal( bool bResetState = false, bool bForceReacquire = false, char const *pszForceReason = nullptr );


	void OnDebugEvent( const char * pEvent = "" );

	IDirect3DDevice9	*m_pD3DDevice;

	D3DPRESENT_PARAMETERS m_PresentParameters;
	ImageFormat			m_AdapterFormat;

	// Mode info
	int					m_DeviceSupportsCreateQuery;

	ShaderDeviceInfo_t	m_PendingVideoModeChangeConfig;
	DeviceState_t		m_DeviceState;

	bool				m_bOtherAppInitializing : 1;
	bool				m_bQueuedDeviceLost : 1;
	bool				m_IsResizing : 1;
	bool				m_bPendingVideoModeChange : 1;
	bool				m_bUsingStencil : 1;
	bool				m_bResourcesReleased : 1;

	// amount of stencil variation we have available
	int					m_iStencilBufferBits;


	// Frame sync objects
	IDirect3DQuery9		*m_pFrameSyncQueryObject[NUM_FRAME_SYNC_QUERIES];
	bool				m_bQueryIssued[NUM_FRAME_SYNC_QUERIES];
	int					m_currentSyncQuery;
	IDirect3DTexture9	*m_pFrameSyncTexture;


	AspectRatioInfo_t m_AspectRatioInfo;

	// Used for x360 only
	NonInteractiveRefreshState_t m_NonInteractiveRefresh;
	std::recursive_mutex m_nonInteractiveModeMutex;
	friend class CShaderDeviceMgrDx8;

	int	m_numReleaseResourcesRefCount;		// This is holding the number of ReleaseResources calls queued up,
											// for every ReleaseResources call there should be a matching call to
											// ReacquireResources, only the last top-level ReacquireResources will
											// have effect. Nested ReleaseResources calls are bugs.
};


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
class D3DDeviceWrapper;
D3DDeviceWrapper *Dx9Device();

extern CShaderDeviceDx8* g_pShaderDeviceDx8;


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

// used to determine if we're deactivated
[[gnu::always_inline]] inline bool CShaderDeviceDx8::IsDeactivated() const 
{ 
	return ( ( ( m_DeviceState != DEVICE_STATE_OK ) || m_bQueuedDeviceLost || m_numReleaseResourcesRefCount ) ); 
}
