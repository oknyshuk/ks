#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "networkstringtabledefs.h"

// String tables used by the game DLL
inline constexpr auto MAX_VGUI_SCREEN_STRING_BITS = 8;
inline constexpr auto MAX_VGUI_SCREEN_STRINGS = ( 1 << MAX_VGUI_SCREEN_STRING_BITS );
inline constexpr auto VGUI_SCREEN_INVALID_STRING = ( MAX_VGUI_SCREEN_STRINGS - 1 );

inline constexpr auto MAX_MATERIAL_STRING_BITS = 10;
inline constexpr auto MAX_MATERIAL_STRINGS = ( 1 << MAX_MATERIAL_STRING_BITS );
inline constexpr auto OVERLAY_MATERIAL_INVALID_STRING = ( MAX_MATERIAL_STRINGS - 1 );

inline constexpr auto MAX_CHOREO_SCENES_STRING_BITS = 13;
inline constexpr auto MAX_CHOREO_SCENES_STRINGS = ( 1 << MAX_CHOREO_SCENES_STRING_BITS );
inline constexpr auto CHOREO_SCENES_INVALID_STRING = ( MAX_CHOREO_SCENES_STRINGS - 1 );

inline constexpr auto MAX_PARTICLESYSTEMS_STRING_BITS = 10;
inline constexpr auto MAX_PARTICLESYSTEMS_STRINGS = ( 1 << MAX_PARTICLESYSTEMS_STRING_BITS );
inline constexpr auto PARTICLESYSTEMS_INVALID_STRING = ( MAX_PARTICLESYSTEMS_STRINGS - 1 );

inline constexpr auto MAX_MOVIE_STRING_BITS = 4;
inline constexpr auto MAX_MOVIE_STRINGS = ( 1 << MAX_MOVIE_STRING_BITS );
inline constexpr auto OVERLAY_MOVIE_INVALID_STRING = ( MAX_MOVIE_STRINGS - 1 );

extern INetworkStringTableContainer *networkstringtable;
extern INetworkStringTable *g_pStringTableVguiScreen;
extern INetworkStringTable *g_pStringTableClientSideChoreoScenes;

inline constexpr auto MAX_INFOPANEL_STRINGS = 128;

// save/load
