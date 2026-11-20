// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: declares a variety of constants

export module ks.core.constants;

import ks.core.platform;

export extern "C++" {

//-----------------------------------------------------------------------------
// numeric constants to avoid typos with wrong number of zeros
//-----------------------------------------------------------------------------
inline constexpr int64 k_nMillion = 1000000;
inline constexpr int64 k_nThousand = 1000;
inline constexpr int64 k_nKiloByte = 1024;
inline constexpr int64 k_nMegabyte = k_nKiloByte * k_nKiloByte;
inline constexpr int64 k_nGigabyte = k_nMegabyte * k_nKiloByte;

//-----------------------------------------------------------------------------
// Timing constants
//-----------------------------------------------------------------------------

inline constexpr unsigned int k_nSecondsPerHour = 60*60;
inline constexpr unsigned int k_nSecondsPerDay = k_nSecondsPerHour * 24;

inline constexpr int k_cSecondsPerMinute = 60;
inline constexpr int k_cSecondsPerHour = k_cSecondsPerMinute * 60;
inline constexpr int k_cSecondsPerDay = k_cSecondsPerHour * 24;
inline constexpr int k_cSecondsPerWeek = k_cSecondsPerDay * 7;
inline constexpr int k_cSecondsPerYear = k_cSecondsPerDay * 365;

}
