#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "togl/rendermechanism.h"
#include "shaderapi/ishaderapi.h"
#include "shaderapi_global.h"
#include "locald3dtypes.h"

// Colors for PIX graphs
#define PIX_VALVE_ORANGE	0xFFF5940F

//-----------------------------------------------------------------------------
// The shader API as seen inside shaderapidx9: a stateless facade over CShaderAPIDx8
//-----------------------------------------------------------------------------
class CShaderAPIBase : public IShaderAPI
{
public:
	// Called when the device is initializing or shutting down
	bool OnDeviceInit();
	void OnDeviceShutdown();

	// Pix events
	void BeginPIXEvent( unsigned long color, const char *szName );
	void EndPIXEvent();
	void AdvancePIXFrame();

	// Release, reacquire objects
	void ReleaseShaderObjects( bool bReleaseManagedResources = true );
	void RestoreShaderObjects();

	// Resets the render state to its well defined initial value
	void ResetRenderState( bool bFullReset = true );

	// Returns a d3d texture associated with a texture handle
	IDirect3DBaseTexture* GetD3DTexture( ShaderAPITextureHandle_t hTexture );

	// Queues a non-full reset of render state next BeginFrame.
	void QueueResetRenderState();

};


//-----------------------------------------------------------------------------
// Pix measurement class
//-----------------------------------------------------------------------------
class CPixEvent
{
public:
	CPixEvent( unsigned long color, const char *szName )
	{
		if ( g_pShaderAPIBase )
			g_pShaderAPIBase->BeginPIXEvent( color, szName );
	}

	~CPixEvent()
	{
		if ( g_pShaderAPIBase )
			g_pShaderAPIBase->EndPIXEvent();
	}
};
