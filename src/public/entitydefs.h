#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Entity definitions needed by hammer, compile tools, and the game.

inline constexpr auto MAX_ENTITY_NAME_LEN = 256;
inline constexpr auto MAX_IO_NAME_LEN = 256;

inline constexpr auto VMF_IOPARAM_STRING_DELIMITER = 0x1b; // Use ESC as a delimiter so we can pass commas etc. in I/O parameters
