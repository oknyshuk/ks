//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( IENGINEUI_H )
#define IENGINEUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

// In-game panels are cropped to the current engine viewport size
enum PaintMode_t
{
	PAINT_UIPANELS		= (1<<0),
	PAINT_INGAMEPANELS  = (1<<1),
};

abstract_class IEngineUI
{
public:
	virtual					~IEngineUI( void ) { }

	virtual bool			IsGameUIVisible() = 0;

	virtual void			ActivateGameUI() = 0;
};

#define VENGINE_UI_VERSION	"VEngineUI001"

#if defined(_STATIC_LINKED) && defined(CLIENT_DLL)
namespace Client
{
extern IEngineUI *engineui;
}
#else
extern IEngineUI *engineui;
#endif

#endif // IENGINEUI_H
