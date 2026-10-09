#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IGameSystem;
struct audioparams_t;

extern IGameSystem *ClientSoundscapeSystem();

// call when audio params may have changed
extern void Soundscape_Update( audioparams_t &audio );

// Called on round restart, otherwise the soundscape system thinks all its
// sounds are still playing when they're not.
void Soundscape_OnStopAllSounds();
