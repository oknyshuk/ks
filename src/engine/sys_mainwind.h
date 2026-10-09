#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

struct InputContextHandle_t__;
using InputContextHandle_t = InputContextHandle_t__ *;

int MapEngineKeyToVirtualKey( int keyCode );


// Pause VCR playback.
void VCR_EnterPausedState();


// During VCR playback, this can be adjusted to slow down the playback.
extern int g_iVCRPlaybackSleepInterval;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
extern bool g_bVCRSingleStep;

extern bool g_bShowVCRPlaybackDisplay;

InputContextHandle_t GetGameInputContext();
