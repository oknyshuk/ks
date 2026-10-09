// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Monotonic clock backing the timers in ks.core:time.

module ks.core;

import std;

uint64 Plat_SteadyNanos()
{
	return (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(
	    std::chrono::steady_clock::now().time_since_epoch() ).count();
}
