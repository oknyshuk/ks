// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.platform;
import ks.core.random;
import ks.platform;
import ks.simd;

void srand(unsigned int)
{
}

int rand()
{
	return RandomInt( 0, VALVE_RAND_MAX );
}
