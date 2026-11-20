#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "snd_audio_source.h"
#include "snd_wave_mixer.h"
#include "sound_private.h"
#include "snd_wave_source.h"
import ks.core.basetypes;
import ks.core.types;

class IWaveData;

class CAudioMixerWave : public CAudioMixer
{
public:
							CAudioMixerWave( AudioMixerKind kind, IWaveData *data );
							~CAudioMixerWave( void );

	int						MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	int						SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	bool					ShouldContinueMixing( void );

	// The two functions of the wave mixers that differ by kind: they switch on the kind and call
	// the member of the same name of the mixer's class (snd_wave_mixer.cpp).
	void					Mix( channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	int						GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	// the default of GetOutputData: reads straight from the wave data
	int						GetOutputDataFromSource( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	CAudioSource*			GetSource( void );
	int						GetSamplePosition( void );
	float					ModifyPitch( float pitch );
	float					GetVolumeScale( void );
	
	// Move the current position to newPosition
	bool					IsSetSampleStartSupported() const;
	void					SetSampleStart( int newPosition );
	
	// End playback at newEndPosition
	void					SetSampleEnd( int newEndPosition );

	void					SetStartupDelaySamples( int delaySamples );
	
	// private helper routines

	char *					LoadMixBuffer( channel_t *pChannel, int sample_load_request, int *psamples_loaded, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	int						MixDataToDevice_( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset, bool bSkipAllSamples );
	int						GetSampleLoadRequest( double rate, int sampleCount, bool bInterpolated_pitch );

	bool					IsReadyToMix();
	int						GetPositionForSave() { return Mixer().GetSamplePosition(); }
	void					SetPositionFromSaved( int savedPosition ) { Mixer().SetSampleStart(savedPosition); }

protected:
	// the mixer as the interface sees it: calls through it reach the member of the mixer's own class
	CAudioMixer				&Mixer() { return *this; }
	const CAudioMixer		&Mixer() const { return *this; }

	double				m_fsample_index;			// index of next sample to output
	int64				m_sample_max_loaded;		// count of total samples loaded - ie: the index of 
													// the next sample to be loaded.
	int64				m_sample_loaded_index;		// index of last sample loaded

	IWaveData			*m_pData;
	double 				m_forcedEndSample;
	bool				m_finished;
	int					m_delaySamples;
};

// 8/16 bit mono/stereo PCM: maps mixing to the device's mixer of that format
class CAudioMixerWavePCM : public CAudioMixerWave
{
public:
	CAudioMixerWavePCM( IWaveData *data, int channels, int bits ) : CAudioMixerWave( AudioMixerKind::WavePCM, data ), m_nChannels( channels ), m_nBits( bits ) {}
	int GetMixSampleSize() { return CalcSampleSize( m_nBits, m_nChannels ); }
	void Mix( channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	int GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) { return GetOutputDataFromSource( pData, sampleCount, copyBuf ); }

private:
	int m_nChannels;		// 1 or 2
	int m_nBits;			// 8 or 16
};
