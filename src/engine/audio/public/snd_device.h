#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.platform;
import ks.simd;

// Purpose: This abstracts the various hardware dependent implementations of sound
//			At the time of this writing there are Windows WAVEOUT, Direct Sound,
//			and Null implementations.

#include "snd_fixedint.h"
#include "snd_mix_buf.h"

// sound engine rate defines
#define SOUND_DMA_SPEED		44100		// hardware playback rate

inline constexpr auto SOUND_11k = 11025; // 11khz sample rate
#define SOUND_22k			22050		// 22khz sample rate
inline constexpr auto SOUND_44k = 44100; // 44khz sample rate
inline constexpr auto SOUND_ALL_RATES = 1; // mix all sample rates
 
inline constexpr auto SOUND_MIX_WET = 0; // mix only samples that don't have channel set to 'dry' or 'speaker' (default)
inline constexpr auto SOUND_MIX_DRY = 1; // mix only samples with channel set to 'dry' (ie: music)
inline constexpr auto SOUND_MIX_SPEAKER = 2; // mix only samples with channel set to 'speaker'

inline constexpr auto SOUND_BUSS_ROOM = (1<<0); // mix samples using channel dspmix value (based on distance from player)
inline constexpr auto SOUND_BUSS_FACING = (1<<1); // mix samples using channel dspface value (source facing)
inline constexpr auto SOUND_BUSS_FACINGAWAY = (1<<2); // mix samples using 1-dspface
inline constexpr auto SOUND_BUSS_SPEAKER = (1<<3); // mix ch->bspeaker samples in mono to speaker buffer
inline constexpr auto SOUND_BUSS_DRY = (1<<4); // mix ch->bdry samples into dry buffer

struct channel_t;


#include "soundsystem/lowlevel.h"
inline IAudioDevice2 *Audio_GetNullDevice()
{
	return Audio_CreateNullDevice();
}
