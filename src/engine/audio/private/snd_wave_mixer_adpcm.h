#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "snd_wave_mixer_private.h"
#include "audio/private/posix_stubs.h"

class CAudioMixer;
class IWaveData;

CAudioMixer *CreateADPCMMixer( IWaveData *data );

//-----------------------------------------------------------------------------
// Purpose: Mixer for ADPCM encoded audio
//-----------------------------------------------------------------------------
class CAudioMixerWaveADPCM : public CAudioMixerWave
{
public:
	CAudioMixerWaveADPCM( IWaveData *data );
	~CAudioMixerWaveADPCM( void );
	
	void Mix( channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	int	 GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	// need to override this to fixup blocks
	bool IsSetSampleStartSupported() const;
	void SetSampleStart( int newPosition );
	int GetMixSampleSize() { return CalcSampleSize( 16, NumChannels() ); }

private:
	bool					DecodeBlock( void );
	int						NumChannels( void );
	void					DecompressBlockMono( short *pOut, const char *pIn, int count );
	void					DecompressBlockStereo( short *pOut, const char *pIn, int count );

	const ADPCMWAVEFORMAT	*m_pFormat;
	const ADPCMCOEFSET		*m_pCoefficients;

	short					*m_pSamples;
	int						m_sampleCount;
	int						m_samplePosition;

	int						m_blockSize;
	int						m_offset;

	int						m_totalBytes;
};


