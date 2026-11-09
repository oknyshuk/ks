#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "snd_audio_source.h"

//-----------------------------------------------------------------------------
// Purpose: Linear iterator over source data.
//			Keeps track of position in source, and maintains necessary buffers
//-----------------------------------------------------------------------------
enum class WaveDataKind : unsigned char
{
	StreamAsync,	// streamed from a file (snd_wave_data.cpp)
	MemoryAsync,	// loaded into memory (snd_wave_data.cpp)
	Voice,			// the data of a voice channel (CWaveDataVoice, voice_sound_engine_interface.cpp)
};

class IWaveData
{
public:
	// The wave data are a closed set (WaveDataKind): every call below switches on the kind and
	// calls the member of the same name of its class (snd_wave_data.cpp).

	// Destroys wave data of any kind
	static void					Delete( IWaveData *pData );

	CAudioSource				&Source( void );
	int							ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	bool						IsReadyToMix();
	void						UpdateLoopPosition( int nLoopPosition );

protected:
	explicit IWaveData( WaveDataKind kind ) : m_Kind( kind ) {}
	~IWaveData( void ) {}

	WaveDataKind				m_Kind;
};

class CAudioSourceWave;

// The data of a voice channel is whatever the voice source gets from the voice code
class CWaveDataVoice : public IWaveData
{
public:
	explicit CWaveDataVoice( CAudioSourceWave &source ) : IWaveData( WaveDataKind::Voice ), m_source( source ) {}

	CAudioSource &Source( void );
	// this file is in memory, simply pass along the data request to the source
	int ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	bool IsReadyToMix() { return true; }
	void UpdateLoopPosition( int nLoopPosition );

private:
	CAudioSourceWave		&m_source;	// pointer to source
};

class IFileReadBinary;
class CSfxTable;

extern IWaveData *CreateWaveDataStream( CAudioSource &source, CAudioSource *pStreamSource, const char *pFileName, int dataStart, int dataSize, CSfxTable *pSfx, int startOffset, int skipInitialSamples, SoundError &soundError );
extern IWaveData *CreateWaveDataMemory( CAudioSource &source );

void PrefetchDataStream( const char *pFileName, int dataOffset, int dataSize );
