#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//=====================================================================
// FX presets
//=====================================================================

inline constexpr auto SXROOM_OFF = 0;

inline constexpr auto SXROOM_GENERIC = 1; // general, low reflective, diffuse room

inline constexpr auto SXROOM_METALIC_S = 2; // highly reflective, parallel surfaces
inline constexpr auto SXROOM_METALIC_M = 3;
inline constexpr auto SXROOM_METALIC_L = 4;

inline constexpr auto SXROOM_TUNNEL_S = 5; // resonant reflective, long surfaces
inline constexpr auto SXROOM_TUNNEL_M = 6;
inline constexpr auto SXROOM_TUNNEL_L = 7;

inline constexpr auto SXROOM_CHAMBER_S = 8; // diffuse, moderately reflective surfaces
inline constexpr auto SXROOM_CHAMBER_M = 9;
inline constexpr auto SXROOM_CHAMBER_L = 10;

inline constexpr auto SXROOM_BRITE_S = 11; // diffuse, highly reflective
inline constexpr auto SXROOM_BRITE_M = 12;
inline constexpr auto SXROOM_BRITE_L = 13;

inline constexpr auto SXROOM_WATER1 = 14; // underwater fx
inline constexpr auto SXROOM_WATER2 = 15;
inline constexpr auto SXROOM_WATER3 = 16;

inline constexpr auto SXROOM_CONCRETE_S = 17; // bare, reflective, parallel surfaces
inline constexpr auto SXROOM_CONCRETE_M = 18;
inline constexpr auto SXROOM_CONCRETE_L = 19;

inline constexpr auto SXROOM_OUTSIDE1 = 20; // echoing, moderately reflective
inline constexpr auto SXROOM_OUTSIDE2 = 21; // echoing, dull
inline constexpr auto SXROOM_OUTSIDE3 = 22; // echoing, very dull

inline constexpr auto SXROOM_CAVERN_S = 23; // large, echoing area
inline constexpr auto SXROOM_CAVERN_M = 24;
inline constexpr auto SXROOM_CAVERN_L = 25;

inline constexpr auto SXROOM_WEIRDO1 = 26;
inline constexpr auto SXROOM_WEIRDO2 = 27;
inline constexpr auto SXROOM_WEIRDO3 = 28;

inline constexpr auto CSXROOM = 29;
