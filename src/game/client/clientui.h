//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTUI_H )
#define CLIENTUI_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"



struct vrect_t;

bool UI_Startup( CreateInterfaceFn appSystemFactory );
void UI_Shutdown( void );
void UI_CreateGlobalPanels( void );
void UI_CreateClientDLLRootPanel( void );
void UI_DestroyClientDLLRootPanel( void );
void UI_PreRender();
void UI_PostRender();
void UI_GetPanelBounds( int slot, int &x, int &y, int &w, int &h );
// If the engine is inset from the UI_GetPanelBounds due to splitscreen aspect ratio fixups...
void UI_GetEngineRenderBounds( int slot, int &x, int &y, int &w, int &h, int &insetX, int &insetY );
void UI_GetHudBounds( int slot, int &x, int &y, int &w, int &h );
void UI_GetTrueScreenSize( int &w, int &h );
void UI_OnScreenSizeChanged();
bool UI_IsSplitScreen();
bool UI_IsSplitScreenPIP();
bool IsWidescreen( void );

void UI_OnSplitScreenStateChanged();

#endif // CLIENTUI_H
