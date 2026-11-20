// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: determine CPU speed under linux


import std;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;

// Compute the positive difference between two 64 bit numbers.
static inline uint64 diff(uint64 v1, uint64 v2)
{
	//  uint64 d = v1 - v2;
	//  if (d >= 0) return d; else return -d;
	return (v1 >= v2 ? v1 - v2 : v2 - v1);
}

uint64 GetCPUFreqFromPROC()
{
	double mhz = 0;
	char line[1024], *s, search_str[] = "cpu MHz";
	std::FILE *fp;

	/* open proc/cpuinfo */
	if ((fp = std::fopen("/proc/cpuinfo", "r")) == nullptr)
	{
		return 0;
	}

	/* ignore all lines until we reach MHz information */
	while (std::fgets(line, 1024, fp) != nullptr)
	{
		if (std::strstr(line, search_str) != nullptr)
		{
			/* ignore all characters in line up to : */
			for (s = line; *s && (*s != ':'); ++s)
				;

			/* get MHz number */
			if (*s && (std::sscanf(s+1, "%lf", &mhz) == 1))
				break;
		}
	}

	if (fp!=nullptr) std::fclose(fp);

	return (uint64)(mhz*1000000);
}


uint64 CalculateCPUFreq()
{
	char const *pFreq = std::getenv("CPU_MHZ");
	if ( pFreq )
	{
		uint64 retVal = 1000000;
		return retVal * std::atoi( pFreq );
	}

	// Compute the period. Loop until we get 3 consecutive periods that
	// are the same to within a small error. The error is chosen
	// to be +/- 0.02% on a P-200.
	const uint64 error = 40000;
	const int max_iterations = 600;
	int count;
	uint64 period, period1 = error * 2, period2 = 0,  period3 = 0;

	for (count = 0; count < max_iterations; count++)
	{
		const uint64 start_tsc = Plat_Rdtsc();
		const uint64 start_ns = ks::platform::monotonic_nanos();
		ks::platform::sleep_nanos( 5000000 ); // sleep for 5 msec
		const uint64 end_tsc = Plat_Rdtsc();
		const uint64 end_ns = ks::platform::monotonic_nanos();

		const double seconds = (double)( end_ns - start_ns ) * 1.0e-9;
		period3 = seconds > 0.0 ? (uint64)( (double)( end_tsc - start_tsc ) / seconds ) : 0;

		if (diff( period1, period2 ) <= error &&
			diff( period2, period3 ) <= error &&
			diff( period1, period3 ) <= error )
		{
			break;
		}

		period1 = period2;
		period2 = period3;
	}

	if ( count == max_iterations )
	{
		return GetCPUFreqFromPROC(); // fall back to /proc
	}

	// Set the period to the average period measured.
	period = ( period1 + period2 + period3 ) / 3;

	// Some Pentiums have broken TSCs that increment very
	// slowly or unevenly.
	if ( period < 10000000 )
	{
		return GetCPUFreqFromPROC(); // fall back to /proc
	}

	return period;
}
