#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This is input priority system, allowing various clients to
// cause input messages / cursor control to be routed to them as opposed to
// other clients.

import std;
import ks.tier1;
#include "inputsystem/iinputsystem.h"

///-----------------------------------------------------------------------------
/// A handle to an input context. These are arranged in a priority-based
/// stack; the top context on the stack which is also enabled wins.
///-----------------------------------------------------------------------------
struct InputContextHandle_t__ { int unused; };
using InputContextHandle_t = InputContextHandle_t__ *;
inline constexpr auto INPUT_CONTEXT_HANDLE_INVALID = ( (InputContextHandle_t)0 );


///-----------------------------------------------------------------------------
/// Purpose: This is input priority system, allowing various clients to
/// cause input messages / cursor control to be routed to them as opposed to
/// other clients.
///
/// NOTE: For Source1, it would be a huge change to move all input (like 
/// the code in engine/keys.cpp for example) to go through this interface. 
/// Therefore, I'm going to stick with only dealing with cursor control, 
/// which is necessary for Jen's new gameUI system to interoperate with VGui.
///-----------------------------------------------------------------------------
class IInputStackSystem
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies();
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	/// Allocates an input context, pushing it on top of the input stack, 
	/// thereby giving it top priority
	InputContextHandle_t PushInputContext();

	/// Pops the top input context off the input stack, and destroys it.
	void PopInputContext( );

	/// Enables/disables an input context, allowing something lower on the
	/// stack to have control of input. Disabling an input context which
	/// owns mouse capture
	void EnableInputContext( InputContextHandle_t hContext, bool bEnable );

	/// Allows a context to make the cursor visible;
	/// the topmost enabled context wins
	void SetCursorVisible( InputContextHandle_t hContext, bool bVisible );

	/// Allows a context to set the cursor icon;
	/// the topmost enabled context wins
	void SetCursorIcon( InputContextHandle_t hContext, InputCursorHandle_t hCursor );

	/// Allows a context to enable mouse capture. Disabling an input context
	/// deactivates mouse capture. Capture will occur if it happens on the
	/// topmost enabled context
	void SetMouseCapture( InputContextHandle_t hContext, bool bEnable );

	/// Allows a context to set the mouse position. It only has any effect if the
	/// specified context is the topmost enabled context
	void SetCursorPosition( InputContextHandle_t hContext, int x, int y );

	/// Returns true if the specified context is the topmost enabled context
	bool IsTopmostEnabledContext( InputContextHandle_t hContext ) const;
};

extern IInputStackSystem *g_pInputStackSystem;;
