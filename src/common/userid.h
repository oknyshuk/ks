#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.playerid;

inline constexpr auto IDTYPE_WON = 0;
inline constexpr auto IDTYPE_PLAYER = 1;
inline constexpr auto IDTYPE_VALVE = 2;
inline constexpr auto IDTYPE_HLTV = 3;
inline constexpr auto IDTYPE_REPLAY = 4;
typedef struct USERID_s
{
	int			idtype;

	uint32		instance;
	AccountID_t	accountid;
} USERID_t;
