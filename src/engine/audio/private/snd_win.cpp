// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation




import ks.tier1;
#include "sound.h"
import std;

#include "snd_device.h"
#include "sound_private.h"
#include "snd_dev_xaudio.h"
#include "snd_wave_mixer_private.h"

#include "audio/private/posix_stubs.h"


bool snd_firsttime = true;

/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 
IAudioDevice2 *g_AudioDevice = nullptr;

/*
==================
S_BlockSound
==================
*/
void S_BlockSound( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->Pause();
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->UnPause();
}

void S_UpdateWindowFocus( bool bWindowHasFocus )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->UpdateFocus( bWindowHasFocus );
}

/*
==================
AutoDetectInit

Try to find a sound device to mix for.
Returns a CAudioNULLDevice if nothing is found.
==================
*/
/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	g_AudioDevice->Shutdown();
	delete g_AudioDevice;
	g_AudioDevice = nullptr;
}
