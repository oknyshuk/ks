 //================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//	ilaunchermgr.h
//
//==================================================================================================
#ifndef ILAUNCHERMGR_H
#define ILAUNCHERMGR_H

#ifdef _WIN32
#pragma once
#endif


// Purpose: The overlay doesn't properly work on OS X 64-bit because a bunch of 
// Cocoa functions that we hook were never ported to 64-bit. Until that is fixed,
// we basically have to work around this by making sure the cursor is visible 
// and set to something that is reasonable for usage in the overlay. 
#define WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND 1

#include "tier0/threadtools.h"
#include "appframework/iappsystem.h"
#include "inputsystem/iinputsystem.h"

#if defined( USE_SDL )
#include <SDL3/SDL_events.h>
#endif

// if you rev this version also update materialsystem/cmaterialsystem.cpp CMaterialSystem::Connect as it defines the string directly
#if defined( USE_SDL )
    #define  SDLMGR_INTERFACE_VERSION "SDLMgrInterface002"
#endif


class GLMDisplayDB;

#if defined( USE_SDL )
typedef struct SDL_Cursor SDL_Cursor;
#endif

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
#ifdef USE_SDL
	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen ) = 0;
#else
	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height ) = 0;
#endif
	
	// Get the next N buffered SDL events. Returns how many were written.
	// These are raw SDL events: nothing re-encodes or reinterprets input on the way
	// through, CInputSystem translates them straight into Source input events.
	virtual int GetEvents( SDL_Event *pEvents, int nMaxEventsToReturn ) = 0;

	// Map SDL window coordinates into engine screen space (the backbuffer).
	virtual void WindowToEngineCoords( float wx, float wy, int &ex, int &ey ) = 0;

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y ) = 0;

#ifdef USE_SDL
	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen ) = 0;
#else
	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight ) = 0;
#endif
	virtual void PumpWindowsMessageLoop() = 0;
		
	virtual void DestroyGameWindow() = 0;
	virtual void SetApplicationIcon( const char *pchAppIconFile ) = 0;
	
	virtual void GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta = false ) = 0;

	virtual void SetMouseVisible( bool bState ) = 0;
#ifdef USE_SDL
	virtual int GetActiveDisplayIndex() = 0;
	virtual void SetMouseCursor( SDL_Cursor *hCursor ) = 0;
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab ) = 0;
	virtual void OnFrameRendered() = 0;
#endif		

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	virtual void ForceSystemCursorVisible() = 0;
	virtual void UnforceSystemCursorVisible() = 0;
#endif
};

extern ILauncherMgr *g_pLauncherMgr;


#endif // ILAUNCHERMGR_H

