#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


#define ROTATION_SPEED				0.6
inline constexpr auto TRADE_DURATION_MIN = 5;
inline constexpr auto TRADE_DURATION_MAX = 10;
inline constexpr auto SMOKEGRENADE_PARTICLERADIUS = 55;

inline constexpr auto SMOKESPHERE_EXPAND_TIME = 5.5; // Take N seconds to expand to SMOKESPHERE_MAX_RADIUS.

inline constexpr auto NUM_PARTICLES_PER_DIMENSION = 6;
inline constexpr auto SMOKEPARTICLE_OVERLAP = 0;

inline constexpr auto SMOKEPARTICLE_SIZE = 55;
inline constexpr auto NUM_MATERIAL_HANDLES = 1;


void InitSmokeFogOverlay();
void TermSmokeFogOverlay();
void DrawSmokeFogOverlay();


// Set these before calling DrawSmokeFogOverlay.
extern float	g_SmokeFogOverlayAlpha;
extern Vector	g_SmokeFogOverlayColor;
