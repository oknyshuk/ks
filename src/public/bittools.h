#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import std;

// Number of bits needed to store the values [0, n) / the next power of two >= n.
inline constexpr int MINIMUM_BITS_NEEDED( int n ) { return std::bit_width( static_cast<unsigned>( n - 1 ) ); }
inline constexpr int ROUND_TO_POWER_OF_2( int n ) { return 1 << MINIMUM_BITS_NEEDED( n ); }
