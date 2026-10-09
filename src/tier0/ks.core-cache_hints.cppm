// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:cache_hints;

import std;
import ks.platform;
import :prelude;
import :color;
import :logging;
import :dbg;
import :memalloc;
import :threadtools;
import :tslist;

export extern "C++" {

inline constexpr int CACHE_LINE_SIZE = 128;

// Zeroes the 128-byte line that pBase + nOffset falls in.
inline void ZeroCacheLine128( void *pBase, std::intptr_t nOffset )
{
	std::intptr_t nLine = reinterpret_cast<std::intptr_t>( static_cast<char *>( pBase ) + nOffset ) & -128;
	std::memset( reinterpret_cast<void *>( nLine ), 0, 128 );
}

}
