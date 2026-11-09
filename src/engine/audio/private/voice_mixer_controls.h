#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IMixerControls
{

public:
	enum Control
	{
		// Microphone boost is a boolean switch that sound cards support which boosts the input signal by about +20dB.
		// If this isn't on, the mic is usually way too quiet.
		MicBoost=0,

		// Volume values are 0-1.
		MicVolume,
		
		// Mic playback muting. You usually want this set to false, otherwise the sound card echoes whatever you say into the mic.
		MicMute,
		
		NumControls
	};

	// The SDL build has no mixer to control: nothing can be read or set.
	bool	GetValue_Float(Control iControl, float &value) {return false;}
	bool	SetValue_Float(Control iControl, float value) {return false;}
	
	// Apps like RealJukebox will switch the waveIn input to use CD audio 
	// rather than the microphone. This should be called at startup to set it back.
	bool	SelectMicrophoneForWaveInput() {return false;}
};

extern IMixerControls *g_pMixerControls;
// Allocates a set of mixer controls.
void InitMixerControls();
void ShutdownMixerControls();
