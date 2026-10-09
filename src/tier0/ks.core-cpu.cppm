// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// CPU frequency measurement, for detecting thermal throttling. The measurement threads the
// original ran are gone; GetCPUFrequencyResults() reports "no data" (all zero) until something
// measures again. See tier0/cpumonitoring.cpp.

export module ks.core:cpu;

export struct CPUFrequencyResults
{
	double m_timeStamp;			// Time (from Plat_FloatTime) when the measurements were made.
	float m_GHz;
	float m_percentage;
	float m_lowestPercentage;
};

export CPUFrequencyResults GetCPUFrequencyResults( bool fGetDisabledResults = false );

// Percentages of nominal speed below which the CPU is considered throttled.
export inline constexpr int kCPUMonitoringWarning1 = 80;
export inline constexpr int kCPUMonitoringWarning2 = 50;
