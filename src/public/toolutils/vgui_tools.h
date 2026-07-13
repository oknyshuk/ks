//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VGUI_TOOLS_H
#define VGUI_TOOLS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

#include <vgui/VGUI.h>

// Every tool must expose this back to us
extern char const *GetVGuiControlsModuleName();

bool UI_Startup( CreateInterfaceFn appSystemFactory );
bool UI_PostInit();
void UI_Shutdown( void );

// Must be implemented by .dll
void VGUI_CreateToolRootPanel( void );
void VGUI_DestroyToolRootPanel( void );
vgui::VPANEL UI_GetToolRootPanel( void );

#endif // VGUI_TOOLS_H
