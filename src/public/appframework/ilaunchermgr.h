#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: The overlay doesn't properly work on OS X 64-bit because a bunch of 
// Cocoa functions that we hook were never ported to 64-bit. Until that is fixed,
// we basically have to work around this by making sure the cursor is visible 
// and set to something that is reasonable for usage in the overlay. 

import std;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.desktop;
#include "inputsystem/iinputsystem.h"


// if you rev this version also update materialsystem/cmaterialsystem.cpp CMaterialSystem::Connect as it defines the string directly
inline constexpr const char *SDLMGR_INTERFACE_VERSION = "SDLMgrInterface002";


class GLMDisplayDB;

class ILauncherMgr
{
public:
	// The one implementation is CSDLMgr (appframework/sdlmgr.cpp); these forward to it.

	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) {}

	// Create the window.
	bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen );
	
	// Get the next N buffered window-system events. Returns how many were written.
	// The events are the backend's own records, passed through untouched: only the
	// input system, which shares that backend, looks inside them.
	int GetEvents( ks::desktop::event *pEvents, int nMaxEventsToReturn );

	// Map SDL window coordinates into engine screen space (the backbuffer).
	void WindowToEngineCoords( float wx, float wy, int &ex, int &ey );

	// Set the mouse cursor position.
	void SetCursorPosition( int x, int y );

	void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen );
	void PumpWindowsMessageLoop();
		
	void DestroyGameWindow();
	void SetApplicationIcon( const char *pchAppIconFile );
	
	void GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta = false );

	void SetMouseVisible( bool bState );
	void SetMouseCursor( ks::desktop::cursor *hCursor );
	void SetForbidMouseGrab( bool bForbidMouseGrab );
	void OnFrameRendered();

	void ForceSystemCursorVisible();
	void UnforceSystemCursorVisible();
};

extern ILauncherMgr *g_pLauncherMgr;
