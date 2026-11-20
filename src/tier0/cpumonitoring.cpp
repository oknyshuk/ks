// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: A non-trivial number of Valve customers hit performance problems because their CPUs overheat
// and are thermally throttled. While thermal throttling is better than melting it is still a
// hardware flaw and it leads to a bad user experience. In some cases the CPU frequency drops
// (constantly or occasionally) by 50-75%, leading to equal or greater framerate drops.
// This is equivalent to a car that goes into limp-home mode to let it continue running after the
// radiator fails -- it's better than destroying the engine, but clearly it needs to be fixed.
// When CPU monitoring is enabled a bunch of background threads are created that wake up at
// the set frequency, spin in a loop to measure the actual usable CPU frequency, then sleep again.
// A delay loop is used to measure the frequency because this is portable (it works for Intel
// and AMD and handles both frequency throttling and duty-cycle reductions) and it doesn't
// require administrator privileges. This technique has been used in VTrace for a while.
// This code doesn't use normal worker threads because of the special purpose nature of this
// work. The threads are started on demand and are never terminated, in order to simplify
// the code.

module ks.core.cpu;

CPUFrequencyResults GetCPUFrequencyResults( bool )
{
	// Zero-initialized results mean no data is available.
	return {};
}

