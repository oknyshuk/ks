// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.platform;
import ks.simd;
#include "voice_mixer_controls.h"


IMixerControls* g_pMixerControls = nullptr;
void InitMixerControls()
{
	if ( !g_pMixerControls )
	{
		g_pMixerControls = new IMixerControls;
	}
}

void ShutdownMixerControls()
{
	delete g_pMixerControls;
	g_pMixerControls = nullptr;
}
