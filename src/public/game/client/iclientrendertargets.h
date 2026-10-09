#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Exposes interfaces to the engine which allow the client to setup their own render targets
//			during the proper period of material system's init.

import std;
import ks.tier1;

class IMaterialSystem;
class IMaterialSystemHardwareConfig;

//---------------------------------------------------------------------------------------------------
// Purpose: Exposes interfaces to the engine which allow the client to setup their own render targets
//			during the proper period of material system's init.
//---------------------------------------------------------------------------------------------------
class IClientRenderTargets
{
public:
	// Pass the material system interface to the client-- Their Material System singleton has not been created
	// at the time they receive this call.
	virtual void InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig ) = 0;

	// Call shutdown on every created refrence-- Clients keep track of this themselves
	// and should add shutdown code to this function whenever they add a new render target.
	virtual void ShutdownClientRenderTargets( void ) = 0;

};

inline constexpr const char *CLIENTRENDERTARGETS_INTERFACE_VERSION = "ClientRenderTargets001";

extern IClientRenderTargets * g_pClientRenderTargets;
