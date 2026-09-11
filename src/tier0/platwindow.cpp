//========== Copyright (c) 2007, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_tier0.h"
#include "tier0/platwindow.h"




//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------
PlatWindow_t Plat_CreateWindow( void *hInstance, const char *pTitle, int nWidth, int nHeight, int nFlags )
{
	return PLAT_WINDOW_INVALID;
}


//-----------------------------------------------------------------------------
// Window title
//-----------------------------------------------------------------------------
void Plat_SetWindowTitle( PlatWindow_t hWindow, const char *pTitle )
{
}


//-----------------------------------------------------------------------------
// Window movement
//-----------------------------------------------------------------------------
void Plat_SetWindowPos( PlatWindow_t hWindow, int x, int y )
{
}


//-----------------------------------------------------------------------------
// Gets the desktop resolution
//-----------------------------------------------------------------------------
void Plat_GetDesktopResolution( int *pWidth, int *pHeight )
{
	*pWidth = 0;
	*pHeight = 0;
}

//-----------------------------------------------------------------------------
// Gets a window size
//-----------------------------------------------------------------------------
void Plat_GetWindowClientSize( PlatWindow_t hWindow, int *pWidth, int *pHeight )
{
	*pWidth = 0;
	*pHeight = 0;
}

//-----------------------------------------------------------------------------
// Is the window minimized?
//-----------------------------------------------------------------------------
bool Plat_IsWindowMinimized( PlatWindow_t hWindow )
{
	return false;
}

//-----------------------------------------------------------------------------
// Gets the shell window in a console app
//-----------------------------------------------------------------------------
PlatWindow_t Plat_GetShellWindow( )
{
	return PLAT_WINDOW_INVALID;
}


//-----------------------------------------------------------------------------
// Convert window -> Screen coordinates
//-----------------------------------------------------------------------------
void Plat_WindowToScreenCoords( PlatWindow_t hWnd, int &x, int &y )
{
}

void Plat_ScreenToWindowCoords( PlatWindow_t hWnd, int &x, int &y )
{
}


