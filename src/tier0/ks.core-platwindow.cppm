// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:platwindow;

import :platform;

export extern "C++" {

//-----------------------------------------------------------------------------
// Window handle
//-----------------------------------------------------------------------------
struct PlatWindow_t__ { int unused; };
using PlatWindow_t = PlatWindow_t__ *;
inline constexpr PlatWindow_t PLAT_WINDOW_INVALID = nullptr;

PlatWindow_t Plat_CreateWindow( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags );

//-----------------------------------------------------------------------------
// Window title
//-----------------------------------------------------------------------------
void Plat_SetWindowTitle( PlatWindow_t hWindow, const char *pTitle );

//-----------------------------------------------------------------------------
// Window movement
//-----------------------------------------------------------------------------
void Plat_SetWindowPos( PlatWindow_t hWindow, int x, int y );

//-----------------------------------------------------------------------------
// Gets the desktop resolution
//-----------------------------------------------------------------------------
void Plat_GetDesktopResolution( int *pWidth, int *pHeight );

//-----------------------------------------------------------------------------
// Gets a window size
//-----------------------------------------------------------------------------
void Plat_GetWindowClientSize( PlatWindow_t hWindow, int *pWidth, int *pHeight );

//-----------------------------------------------------------------------------
// Is the window minimized?
//-----------------------------------------------------------------------------
bool Plat_IsWindowMinimized( PlatWindow_t hWindow );

//-----------------------------------------------------------------------------
// Gets the shell window in a console app
//-----------------------------------------------------------------------------
PlatWindow_t Plat_GetShellWindow( );

//-----------------------------------------------------------------------------
// Convert window -> Screen coordinates and back
//-----------------------------------------------------------------------------
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y );
void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y );

}
