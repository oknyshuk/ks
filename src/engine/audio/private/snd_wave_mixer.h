#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IWaveData;
class CAudioMixer;

// skipInitialSamples is used when we can't determine the initialStreamPosition (if there is no seek table for example).
// It has better granularity than the streamInitialPosition, but is less efficient when skipping large number of samples.
CAudioMixer *CreateWaveMixer( IWaveData *data, int format, int channels, int bits, int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo );
