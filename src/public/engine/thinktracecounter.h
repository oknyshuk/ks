#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Some macros for the raytraces-in-think-function-counter.
//         They're in a header because they're included in a bunch of
//         places, but on some cases they need to define files and in
//         others only extern them.

#ifdef THINK_TRACE_COUNTER_COMPILED
	// create a macro that is true if we are allowed to debug traces during thinks, and compiles out to nothing otherwise.
		#ifdef THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_ENGINE
			bool DEBUG_THINK_TRACE_COUNTER_ALLOWED()
			{
				return true;
			}
		#elif defined( THINK_TRACE_COUNTER_COMPILE_FUNCTIONS_SERVER )
			bool DEBUG_THINK_TRACE_COUNTER_ALLOWED()
			{
				return true;
			}
		#else
			extern bool DEBUG_THINK_TRACE_COUNTER_ALLOWED();
		#endif
#else

	// The think-trace counter is not compiled in, so nothing may trap on it.
	inline bool DEBUG_THINK_TRACE_COUNTER_ALLOWED() { return false; }

#endif
