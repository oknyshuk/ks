#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: The overlay doesn't properly work on OS X 64-bit because a bunch of 
// Cocoa functions that we hook were never ported to 64-bit. Until that is fixed,
// we basically have to work around this by making sure the cursor is visible 
// and set to something that is reasonable for usage in the overlay. 

import std;
import ks.tier1;
import ks.desktop;
#include "inputsystem/iinputsystem.h"


// if you rev this version also update materialsystem/cmaterialsystem.cpp CMaterialSystem::Connect as it defines the string directly
inline constexpr const char *SDLMGR_INTERFACE_VERSION = "SDLMgrInterface002";


class GLMDisplayDB;

class ILauncherMgr : public IAppSystem
{
public:
	virtual bool Connect( CreateInterfaceFn factory ) = 0;
	virtual void Disconnect() = 0;
	
	virtual void *QueryInterface( const char *pInterfaceName ) = 0;
	
	// Init, shutdown
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;
	
	// Create the window.
	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen ) = 0;
	
	// Get the next N buffered window-system events. Returns how many were written.
	// The events are the backend's own records, passed through untouched: only the
	// input system, which shares that backend, looks inside them.
	virtual int GetEvents( ks::desktop::event *pEvents, int nMaxEventsToReturn ) = 0;

	// Map SDL window coordinates into engine screen space (the backbuffer).
	virtual void WindowToEngineCoords( float wx, float wy, int &ex, int &ey ) = 0;

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y ) = 0;

	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen ) = 0;
	virtual void PumpWindowsMessageLoop() = 0;
		
	virtual void DestroyGameWindow() = 0;
	virtual void SetApplicationIcon( const char *pchAppIconFile ) = 0;
	
	virtual void GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta = false ) = 0;

	virtual void SetMouseVisible( bool bState ) = 0;
	virtual void SetMouseCursor( ks::desktop::cursor *hCursor ) = 0;
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab ) = 0;
	virtual void OnFrameRendered() = 0;

	virtual void ForceSystemCursorVisible() = 0;
	virtual void UnforceSystemCursorVisible() = 0;
};

extern ILauncherMgr *g_pLauncherMgr;
