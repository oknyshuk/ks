#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Decal flags that can be set publicly by other modules for the decal system.

// NOTE: If you add a flag here, make sure to add one to the private set of flags as well, 
// and update R_ConvertToPrivateDecalFlags

inline constexpr auto EDF_PLAYERSPRAY = 0x00000001;
inline constexpr auto EDF_IMMEDIATECLEANUP = 0x00000002;
