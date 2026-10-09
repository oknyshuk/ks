#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Device Common Routines

#include "snd_mix_buf.h"
#include "snd_channels.h"
#include "snd_fixedint.h"

void DEBUG_DrawPanCurves(void);

extern void		Device_MixUpsample( int sampleCount, int filtertype );
extern void		Device_ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );
extern void		Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono, int nWavType );
extern void		Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], const Vector& sourceDir, float mono, float flRearToStereoScale = 0.75 );
extern void		Device_Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
