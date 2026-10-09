// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "voice_mixer_controls.h"


class CMixerControls : public IMixerControls
{
public:
	CMixerControls() {}
	virtual			~CMixerControls() {}
	
	virtual void	Release() {}
	virtual bool	GetValue_Float(Control iControl, float &value ) {return false;}
	virtual bool	SetValue_Float(Control iControl, float value) {return false;}
	virtual bool	SelectMicrophoneForWaveInput() {return false;}
	virtual const char *GetMixerName() {return "SDL"; }
	
private:
};

IMixerControls* g_pMixerControls = nullptr;
void InitMixerControls()
{
	if ( !g_pMixerControls )
	{
		g_pMixerControls = new CMixerControls;
	}
}

void ShutdownMixerControls()
{
	delete g_pMixerControls;
	g_pMixerControls = nullptr;
}
