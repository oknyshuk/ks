// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1.null_mutex;

import std;
import ks.platform;
import ks.simd;

export extern "C++" {

namespace ks
{
	struct null_mutex
	{
		static void lock() noexcept				{}
		static void unlock() noexcept			{}
		static bool try_lock() noexcept			{ return true; }
	};
}

}
