#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
class CBaseEntity;

class IScratchPad3D;


inline constexpr auto SPDRAWWORLD_DRAW_WORLD = 0x001;
inline constexpr auto SPDRAWWORLD_DRAW_PLAYERS = 0x002; // Draw player entities?
inline constexpr auto SPDRAWWORLD_DRAW_ENTITIES = 0x004; // Draw entities other than players?
inline constexpr auto SPDRAWWORLD_DRAW_ENTITY_CLASSNAMES = 0x008;
inline constexpr auto SPDRAWWORLD_DRAW_EDICT_INDICES = 0x010;

inline constexpr auto SPDRAWWORLD_DRAW_ALL = 0xFFFFFFFF;


// Draws the world and various things in it into the scratchpad.
// flags is a combination of the SPDRAWWORLD_ flags.
void ScratchPad_DrawWorldToScratchPad(
	IScratchPad3D *pPad,
	unsigned long flags );

// Draw a specific entity into the scratch pad.
void ScratchPad_DrawEntityToScratchPad(
	IScratchPad3D *pPad,
	unsigned long flags,
	CBaseEntity *pEnt,
	const Vector &vColor );
