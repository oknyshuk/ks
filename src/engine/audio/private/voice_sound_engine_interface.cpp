// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;


import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;

#include "tier2/riff.h"
#include "sound.h"
#include "soundservice.h"
#include "voice_sound_engine_interface.h"
#include "soundflags.h"

#include "snd_device.h"
#include "sound_private.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"
#include "snd_sfx.h"
#include "snd_wave_source.h"
#include "snd_wave_data.h"
#include "snd_wave_mixer_private.h"

#include "audio/private/posix_stubs.h"
#include "ivoicecodec.h"

// ------------------------------------------------------------------------- //
// CAudioSourceVoice.
// This feeds the data from an incoming voice channel (a guy on the server
// who is speaking) into the sound engine.
// ------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------- //
// CWaveDataVoice: the wave data of a voice channel.
// ----------------------------------------------------------------------------- //

CAudioSource &CWaveDataVoice::Source( void )
{
	return m_source;
}

// this file is in memory, simply pass along the data request to the source
int CWaveDataVoice::ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	return m_source.GetOutputData( pData, sampleIndex, sampleCount, copyBuf );
}

void CWaveDataVoice::UpdateLoopPosition( int nLoopPosition )
{
	// Should not be necessary in this implementation...
	contract_assert( false );
}


// ----------------------------------------------------------------------------- //
// Globals.
// ----------------------------------------------------------------------------- //

// The format we sample voice in.
extern WAVEFORMATEX g_VoiceSampleFormat;

class CVoiceSfx : public CSfxTable
{
public:
	CVoiceSfx() { m_Kind = SfxKind::Voice; }
};

static CVoiceSfx g_CVoiceSfx[VOICE_NUM_CHANNELS];

static float	g_VoiceOverdriveDuration = 0;
static bool		g_bVoiceOverdriveOn = false;

// When voice is on, all other sounds are decreased by this factor.
static ConVar voice_overdrive( "voice_overdrive", "2" );
static ConVar voice_overdrivefadetime( "voice_overdrivefadetime", "0.4" ); // How long it takes to fade in and out of the voice overdrive.

// The sound engine uses this to lower all sound volumes.
// All non-voice sounds are multiplied by this and divided by 256.
int g_SND_VoiceOverdriveInt = 256;


extern int Voice_SamplesPerSec();
extern int Voice_AvgBytesPerSec();

// ----------------------------------------------------------------------------- //
// CAudioSourceVoice implementation.
// ----------------------------------------------------------------------------- //

CAudioSourceVoice::CAudioSourceVoice( CSfxTable *pSfx, int iChannel )
	: CAudioSourceWave( AudioSourceKind::Voice, pSfx )
{
	m_iChannel = iChannel;
	m_refCount = 0;

	WAVEFORMATEX tmp = g_VoiceSampleFormat;
	tmp.nSamplesPerSec = Voice_SamplesPerSec();
	tmp.nAvgBytesPerSec = Voice_AvgBytesPerSec();
	Init((char*)&tmp, sizeof(tmp));
	m_sampleCount = tmp.nSamplesPerSec;
}

CAudioSourceVoice::~CAudioSourceVoice()
{
	Voice_OnAudioSourceShutdown( m_iChannel );
}

CAudioMixer *CAudioSourceVoice::CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError )
{
	soundError = SE_OK;
	CWaveDataVoice *pVoice = new CWaveDataVoice(*this);
	if(!pVoice)
	{
		contract_assert( false );
		soundError = SE_CANT_CREATE_MIXER;
		return nullptr;
	}

	CAudioMixer *pMixer = CreateWaveMixer( pVoice, WAVE_FORMAT_PCM, 1, BYTES_PER_SAMPLE*8, initialStreamPosition, skipInitialSamples, bUpdateDelayForChoreo );
	if(!pMixer)
	{
		delete pVoice;
		soundError = SE_CANT_CREATE_MIXER;
		return nullptr;
	}

	ReferenceAdd( pMixer );
	return pMixer;
}

int CAudioSourceVoice::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	int nSamplesGotten = Voice_GetOutputData(
		m_iChannel,
		copyBuf,
		AUDIOSOURCE_COPYBUF_SIZE,
		(int)samplePosition,
		sampleCount );
	
	// If there weren't enough bytes in the received data channel, pad it with zeros.
	if( nSamplesGotten < sampleCount )
	{
		if ( copyBuf != nullptr )
		{
			std::memset( &copyBuf[nSamplesGotten], 0, (sampleCount - nSamplesGotten) * BYTES_PER_SAMPLE );
		}
		nSamplesGotten = sampleCount;
	}

	*pData = copyBuf;
	return nSamplesGotten;
}

int CAudioSourceVoice::SampleRate()
{
	return Voice_SamplesPerSec();
}

int CAudioSourceVoice::SampleSize()
{
	return BYTES_PER_SAMPLE;
}

int CAudioSourceVoice::SampleCount()
{
	return Voice_SamplesPerSec();
}

void CAudioSourceVoice::ReferenceAdd(CAudioMixer *pMixer)
{
	m_refCount++;
}

void CAudioSourceVoice::ReferenceRemove(CAudioMixer *pMixer)
{
	m_refCount--;
	if ( m_refCount <= 0 )
		CAudioSource::Delete( this );
}

bool CAudioSourceVoice::CanDelete()
{
	return m_refCount == 0;
}


// ----------------------------------------------------------------------------- //
// Interface implementation.
// ----------------------------------------------------------------------------- //

bool VoiceSE_Init()
{
	if( !snd_initialized )
		return false;

	g_SND_VoiceOverdriveInt = 256;
	return true;
}

void VoiceSE_Term()
{
	// Disable voice ducking.
	g_SND_VoiceOverdriveInt = 256;
}


void VoiceSE_Idle(float frametime)
{
	g_SND_VoiceOverdriveInt = 256;

	if( g_bVoiceOverdriveOn )
	{
		g_VoiceOverdriveDuration = std::min( g_VoiceOverdriveDuration+frametime, voice_overdrivefadetime.GetFloat() );
	}
	else
	{
		if(g_VoiceOverdriveDuration == 0)
			return;

		g_VoiceOverdriveDuration = std::max<std::common_type_t<float, int>>(g_VoiceOverdriveDuration-frametime, 0);
	}

	float percent = g_VoiceOverdriveDuration / voice_overdrivefadetime.GetFloat();
	percent = (float)(-std::cos(percent * 3.1415926535) * 0.5 + 0.5);		// Smooth it out..
	float voiceOverdrive = 1 + (voice_overdrive.GetFloat() - 1) * percent;
	g_SND_VoiceOverdriveInt = (int)(256 / voiceOverdrive);
}


int VoiceSE_StartChannel(
	int iChannel,	//! Which channel to start.
	int iEntity,
	bool bProximity,
	int nViewEntityIndex )
{
	contract_assert( iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS );

	// Start the sound.
	CSfxTable *sfx = &g_CVoiceSfx[iChannel];
	sfx->pSource = nullptr;
	Vector vOrigin(0,0,0);

	StartSoundParams_t params;
	params.staticsound = false;
	params.entchannel = (CHAN_VOICE_BASE+iChannel);
	params.pSfx = sfx;
	params.origin = vOrigin;
	params.fvol = 1.0f;
	params.flags = 0;
	params.pitch = PITCH_NORM;


	if ( bProximity == true )
	{
		params.bUpdatePositions = true;
		params.soundlevel = SNDLVL_TALKING;
		params.soundsource = iEntity;
	}
	else
	{
		params.soundlevel = SNDLVL_IDLE;
		params.soundsource = nViewEntityIndex;
	}


	return S_StartSound( params );
}

void VoiceSE_EndChannel(
	int iChannel,	//! Which channel to stop.
	int iEntity
	)
{
	contract_assert( iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS );

	S_StopSound( iEntity, CHAN_VOICE_BASE+iChannel );

	// Start the sound.
	CSfxTable *sfx = &g_CVoiceSfx[iChannel];
	sfx->pSource = nullptr;
}

void VoiceSE_StartOverdrive()
{
	g_bVoiceOverdriveOn = true;
}

void VoiceSE_EndOverdrive()
{
	g_bVoiceOverdriveOn = false;
}


void VoiceSE_InitMouth(int entnum)
{
}

void VoiceSE_CloseMouth(int entnum)
{
}

void VoiceSE_MoveMouth(int entnum, short *pSamples, int nSamples)
{
}


CAudioSource* Voice_SetupAudioSource( int soundsource, int entchannel )
{
	int iChannel = entchannel - CHAN_VOICE_BASE;
	if( iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS )
	{
		CSfxTable *sfx = &g_CVoiceSfx[iChannel];
		return new CAudioSourceVoice( sfx, iChannel );
	}
	else
		return nullptr;
}
