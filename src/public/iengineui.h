#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

import std;
import ks.tier1;

// In-game panels are cropped to the current engine viewport size
enum PaintMode_t
{
	PAINT_UIPANELS		= (1<<0),
	PAINT_INGAMEPANELS  = (1<<1),
};

class IEngineUI
{
public:
	virtual					~IEngineUI( void ) { }

	virtual bool			IsGameUIVisible() = 0;

	virtual void			ActivateGameUI() = 0;
};

inline constexpr const char *VENGINE_UI_VERSION = "VEngineUI001";

extern IEngineUI *engineui;
