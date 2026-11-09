#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

#include "cs_weapon_parse.h"

bool BlackMarket_DownloadPrices( void );

inline constexpr auto MED_BUFFER_SIZE = 1024;
inline constexpr auto SMALL_BUFFER_SIZE = 255;
inline constexpr auto MAX_DNS_NAME = 255;

inline constexpr auto PRICE_BLOB_VERSION = 1;
inline constexpr const char *PRICE_BLOB_NAME = "weeklyprices.dat";

struct weeklyprice_t
{
	short iVersion;
	short iPreviousPrice[WEAPON_MAX];
	short iCurrentPrice[WEAPON_MAX];
};
