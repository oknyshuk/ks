//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Monotonic clock backing the timers in tier0/fasttimer.h.
//
//=============================================================================//

#include "pch_tier0.h"

#include <chrono>

#include "tier0/fasttimer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

uint64 Plat_SteadyNanos()
{
	return (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(
	    std::chrono::steady_clock::now().time_since_epoch() ).count();
}
