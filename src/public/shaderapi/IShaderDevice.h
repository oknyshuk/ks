#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.threadtools;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
#include "bitmap/imageformat.h"
#include "materialsystem/imaterial.h"
#include "shaderapi/ishaderdynamic.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
struct MaterialAdapterInfo_t;
class IMesh;


//-----------------------------------------------------------------------------
// Describes how to set the mode
//-----------------------------------------------------------------------------
inline constexpr auto SHADER_DISPLAY_MODE_VERSION = 1;

struct ShaderDisplayMode_t
{
	ShaderDisplayMode_t() { std::memset( this, 0, sizeof(ShaderDisplayMode_t) ); m_nVersion = SHADER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	int m_nWidth;					// 0 when running windowed means use desktop resolution
	int m_nHeight;
	ImageFormat m_Format;			// use ImageFormats (ignored for windowed mode)
	int m_nRefreshRateNumerator;	// Refresh rate. Use 0 in numerator + denominator for a default setting.
	int m_nRefreshRateDenominator;	// Refresh rate = numerator / denominator.
};

	
//-----------------------------------------------------------------------------
// Describes how to set the device
//-----------------------------------------------------------------------------
inline constexpr auto SHADER_DEVICE_INFO_VERSION = 1;

struct ShaderDeviceInfo_t
{
	ShaderDeviceInfo_t() { std::memset( this, 0, sizeof(ShaderDeviceInfo_t) ); m_nVersion = SHADER_DEVICE_INFO_VERSION; m_DisplayMode.m_nVersion = SHADER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	ShaderDisplayMode_t m_DisplayMode;
	int m_nBackBufferCount;				// valid values are 1 or 2 [2 results in triple buffering]
	int m_nAASamples;					// Number of AA samples to use
	int m_nAAQuality;					// AA quality level
	int m_nDXLevel;						// 0 means use recommended DX level for this adapter
	int m_nWindowedSizeLimitWidth;		// Used if m_bLimitWindowedSize is set, defines max bounds for the back buffer
	int m_nWindowedSizeLimitHeight;

	bool m_bWindowed : 1;
	bool m_bResizing : 1;				// Only is meaningful when using windowed mode; means the window can be resized.
	bool m_bUseStencil : 1;
	bool m_bLimitWindowedSize : 1;		// In windowed mode, should we prevent the back buffer from getting too large?
	bool m_bWaitForVSync : 1;			// Would we not present until vsync?
	bool m_bScaleToOutputResolution : 1;			// 360 ONLY: sets up hardware scaling
	bool m_bProgressive : 1;			// 360 ONLY: interlaced or progressive
	bool m_bUsingMultipleWindows : 1; 	// Forces D3DPresent to use _COPY instead
};


//-----------------------------------------------------------------------------
// Info for non-interactive mode
//-----------------------------------------------------------------------------
struct ShaderNonInteractiveInfo_t
{
	// background
	ShaderAPITextureHandle_t m_hTempFullscreenTexture;

	// logo
	ShaderAPITextureHandle_t m_hLogoTexture;
	float m_flLogoNormalizedX;
	float m_flLogoNormalizedY;
	float m_flLogoNormalizedW;
	float m_flLogoNormalizedH;

	// pacifier
	int m_nPacifierCount;
	ShaderAPITextureHandle_t m_pPacifierTextures[64];
	float m_flNormalizedX;
	float m_flNormalizedY;
	float m_flNormalizedSize;
};


//-----------------------------------------------------------------------------
// For vertex/index buffers. What type is it?
// (NOTE: mirror this with a similarly named enum at the material system level for backwards compatability.)
//-----------------------------------------------------------------------------
enum ShaderBufferType_t
{
	SHADER_BUFFER_TYPE_STATIC = 0,
	SHADER_BUFFER_TYPE_DYNAMIC,
	SHADER_BUFFER_TYPE_STATIC_TEMP,
	SHADER_BUFFER_TYPE_DYNAMIC_TEMP,

	SHADER_BUFFER_TYPE_COUNT,
};

inline bool IsDynamicBufferType( ShaderBufferType_t type )
{
	return ( ( type == SHADER_BUFFER_TYPE_DYNAMIC ) || ( type == SHADER_BUFFER_TYPE_DYNAMIC_TEMP ) );
}


//-----------------------------------------------------------------------------
// Handle to a vertex, pixel, and geometry shader
//-----------------------------------------------------------------------------
struct VertexShaderHandle_t__ { int unused; };
using VertexShaderHandle_t = VertexShaderHandle_t__ *;
struct GeometryShaderHandle_t__ { int unused; };
using GeometryShaderHandle_t = GeometryShaderHandle_t__ *;
struct PixelShaderHandle_t__ { int unused; };
using PixelShaderHandle_t = PixelShaderHandle_t__ *;

inline constexpr auto VERTEX_SHADER_HANDLE_INVALID = ( (VertexShaderHandle_t)0 );
inline constexpr auto GEOMETRY_SHADER_HANDLE_INVALID = ( (GeometryShaderHandle_t)0 );
inline constexpr auto PIXEL_SHADER_HANDLE_INVALID = ( (PixelShaderHandle_t)0 );


//-----------------------------------------------------------------------------
// A shader buffer is a view of a block of memory holding a (compiled) shader.
// NOTE: This is meant to be instanced on the stack; so don't call Release!
//-----------------------------------------------------------------------------
class IShaderBuffer
{
public:
	IShaderBuffer( std::span< const std::byte > bytes ) : m_bytes( bytes ) {}

	std::size_t GetSize() const { return m_bytes.size(); }
	const void* GetBits() const { return m_bytes.data(); }
	void Release() { contract_assert( 0 ); }

private:
	std::span< const std::byte > m_bytes;
};


//-----------------------------------------------------------------------------
// Mode chance callback
//-----------------------------------------------------------------------------
typedef void (*ShaderModeChangeCallbackFunc_t)( void );

//-----------------------------------------------------------------------------
// Used by the UI to manage when the device is lost or reset, or when
// the mode changes
//-----------------------------------------------------------------------------

class IShaderDevice;

class IShaderDeviceDependentObject
{
public:
	virtual void DeviceLost( void ) = 0;
	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd ) = 0;
	virtual void ScreenSizeChanged( int width, int height ) = 0;
};


//-----------------------------------------------------------------------------
// Methods related to discovering and selecting devices
//-----------------------------------------------------------------------------
inline constexpr const char *SHADER_DEVICE_MGR_INTERFACE_VERSION = "ShaderDeviceMgr001";
class IShaderDeviceMgr
{
public:
	// Connection and lifetime
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();

	// Gets the number of adapters...
	int	 GetAdapterCount() const;

	// Returns info about each adapter
	void GetAdapterInfo( int nAdapter, MaterialAdapterInfo_t& info ) const;

	// Gets recommended congifuration for a particular adapter at a particular dx level
	bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pConfiguration );

	// Returns the number of modes
	int	 GetModeCount( int nAdapter ) const;

	// Returns mode information..
	void GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int nMode ) const;

	// Returns the current mode info for the requested adapter
	void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;

	// Initialization, shutdown
	bool SetAdapter( int nAdapter, int nFlags );

	// Sets the mode
	// Use the returned factory to get at an IShaderDevice and an IShaderRender
	// and any other interfaces we decide to create.
	// A returned factory of NULL indicates the mode was not set properly.
	CreateInterfaceFn SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );

	// Installs a callback to get called 
	void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func );
	void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func );

	bool GetRecommendedVideoConfig( int nAdapter, KeyValues *pConfiguration );

	void AddDeviceDependentObject( IShaderDeviceDependentObject *pObject );
	void RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject );
};


//-----------------------------------------------------------------------------
// Methods related to control of the device
//-----------------------------------------------------------------------------
inline constexpr const char *SHADER_DEVICE_INTERFACE_VERSION = "ShaderDevice001";
class IShaderDevice
{
public:
	// Releases/reloads resources when other apps want some memory
	void ReleaseResources( bool bReleaseManagedResources = true );
	void ReacquireResources();

	// returns the backbuffer format and dimensions
	ImageFormat GetBackBufferFormat() const;
	void GetBackBufferDimensions( int& width, int& height ) const;

	const AspectRatioInfo_t &GetAspectRatioInfo( void ) const;

	// Returns the current adapter in use
	int GetCurrentAdapter() const;

	// Are we using graphics?
	bool IsUsingGraphics() const;

	// Use this to spew information about the 3D layer 
	void SpewDriverInfo() const;

	// What's the bit depth of the stencil buffer?
	int StencilBufferBits() const;

	// Are we using a mode that uses MSAA
	bool IsAAEnabled() const;

	// Does a page flip
	void Present();

	// Returns the window size
	void GetWindowSize( int &nWidth, int &nHeight ) const;

	// Gamma ramp control
	void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled );

	// Creates/ destroys a child window
	bool AddView( void* hWnd );
	void RemoveView( void* hWnd );

	// Activates a view
	void SetView( void* hWnd );

	// Shader compilation
	IShaderBuffer* CompileShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );

	// Shader creation, destruction
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer );
	void DestroyVertexShader( VertexShaderHandle_t hShader );
	GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer );
	void DestroyGeometryShader( GeometryShaderHandle_t hShader );
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer );
	void DestroyPixelShader( PixelShaderHandle_t hShader );

	// Utility methods to make shader creation simpler
	// NOTE: The span versions take a compiled shader
	VertexShaderHandle_t CreateVertexShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );
	VertexShaderHandle_t CreateVertexShader( std::span< const std::byte > compiled );
	GeometryShaderHandle_t CreateGeometryShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );
	GeometryShaderHandle_t CreateGeometryShader( std::span< const std::byte > compiled );
	PixelShaderHandle_t CreatePixelShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion );
	PixelShaderHandle_t CreatePixelShader( std::span< const std::byte > compiled );

	// NOTE: Deprecated!! Use CreateVertexBuffer/CreateIndexBuffer instead
	// Creates/destroys Mesh
	IMesh* CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void DestroyStaticMesh( IMesh* mesh );

	// Creates/destroys static vertex + index buffers
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer );

	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer );

	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	IIndexBuffer *GetDynamicIndexBuffer();

	// A special path used to tick the front buffer while loading on the 360
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo = nullptr );
	void RefreshFrontBufferNonInteractive( );
	void HandleThreadEvent( uint32 threadEvent );

	
	void OnDebugEvent( const char * pEvent = "" );
};


//-----------------------------------------------------------------------------
// Inline methods of IShaderDevice
//-----------------------------------------------------------------------------
inline VertexShaderHandle_t IShaderDevice::CreateVertexShader( std::span< const std::byte > compiled )
{
	IShaderBuffer shaderBuffer( compiled );
	return CreateVertexShader( &shaderBuffer );
}

inline VertexShaderHandle_t IShaderDevice::CreateVertexShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{
	VertexShaderHandle_t hVertexShader = VERTEX_SHADER_HANDLE_INVALID;
	IShaderBuffer* pShaderBuffer = CompileShader( pProgram, nBufLen, pShaderVersion );
	if ( pShaderBuffer )
	{
		hVertexShader = CreateVertexShader( pShaderBuffer );
		pShaderBuffer->Release();
	}
	return hVertexShader;
}

inline GeometryShaderHandle_t IShaderDevice::CreateGeometryShader( std::span< const std::byte > compiled )
{
	IShaderBuffer shaderBuffer( compiled );
	return CreateGeometryShader( &shaderBuffer );
}

inline GeometryShaderHandle_t IShaderDevice::CreateGeometryShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{
	GeometryShaderHandle_t hGeometryShader = GEOMETRY_SHADER_HANDLE_INVALID;
	IShaderBuffer* pShaderBuffer = CompileShader( pProgram, nBufLen, pShaderVersion );
	if ( pShaderBuffer )
	{
		hGeometryShader = CreateGeometryShader( pShaderBuffer );
		pShaderBuffer->Release();
	}
	return hGeometryShader;
}

inline PixelShaderHandle_t IShaderDevice::CreatePixelShader( std::span< const std::byte > compiled )
{
	IShaderBuffer shaderBuffer( compiled );
	return CreatePixelShader( &shaderBuffer );
}

inline PixelShaderHandle_t IShaderDevice::CreatePixelShader( const char *pProgram, std::size_t nBufLen, const char *pShaderVersion )
{
	PixelShaderHandle_t hPixelShader = PIXEL_SHADER_HANDLE_INVALID;
	IShaderBuffer* pShaderBuffer = CompileShader( pProgram, nBufLen, pShaderVersion );
	if ( pShaderBuffer )
	{
		hPixelShader = CreatePixelShader( pShaderBuffer );
		pShaderBuffer->Release();
	}
	return hPixelShader;
}
