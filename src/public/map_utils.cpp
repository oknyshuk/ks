// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(GAME_DLL)
#endif
#if defined(GAME_DLL)
import std;
#endif
#include "bspfile.h"
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;

void SetupLightNormalFromProps( const QAngle &angles, float angle, float pitch, Vector &output )
{
	if (angle == ANGLE_UP)
	{
		output[0] = output[1] = 0;
		output[2] = 1;
	}
	else if (angle == ANGLE_DOWN)
	{
		output[0] = output[1] = 0;
		output[2] = -1;
	}
	else
	{
		// if we don't have a specific "angle" use the "angles" YAW
		if ( !angle )
		{
			angle = angles[YAW];
		}
		
		output[2] = 0;
		output[0] = (float)std::cos (angle/180*std::numbers::pi);
		output[1] = (float)std::sin (angle/180*std::numbers::pi);
	}
	
	if ( !pitch )
	{
		// if we don't have a specific "pitch" use the "angles" PITCH
		pitch = angles[PITCH];
	}
	
	output[2] = (float)std::sin(pitch/180*std::numbers::pi);
	output[0] *= (float)std::cos(pitch/180*std::numbers::pi);
	output[1] *= (float)std::cos(pitch/180*std::numbers::pi);
}
