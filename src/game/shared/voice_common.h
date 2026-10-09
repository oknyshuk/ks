#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "const.h"
#include "shareddefs.h"

inline constexpr auto VOICE_MAX_PLAYERS = MAX_PLAYERS;
inline constexpr auto VOICE_MAX_PLAYERS_DW = ((VOICE_MAX_PLAYERS / 32) + !!(VOICE_MAX_PLAYERS & 31));
inline constexpr auto VOICE_DEFAULT_PROXIMITY_RANGE = 1200; //100 feet
