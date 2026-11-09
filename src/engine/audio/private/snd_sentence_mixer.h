#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "snd_audio_source.h"
#include "vox_private.h"

//-----------------------------------------------------------------------------
// Purpose: This replaces the old sentence logic that was integrated with the
//			sound code.  Now it is a hierarchical mixer.
//-----------------------------------------------------------------------------
class CSentenceMixer : public CAudioMixer
{
public:
	CSentenceMixer( voxword_t *pWords );
	~CSentenceMixer( void );

	// return number of samples mixed
	int			MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	int			SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	bool		ShouldContinueMixing( void );

	CAudioSource*	GetSource( void );
	
	// get the current position (next sample to be mixed)
	int			GetSamplePosition( void );
	float		ModifyPitch( float pitch );
	float		GetVolumeScale( void );

	// BUGBUG: These are only applied to the current word, not the whole sentence!!!!
	bool		IsSetSampleStartSupported() const;
	void		SetSampleStart( int newPosition );
	void		SetSampleEnd( int newEndPosition );

	void		SetStartupDelaySamples( int delaySamples );
	int			GetMixSampleSize() { return m_pCurrentWordMixer ? m_pCurrentWordMixer->GetMixSampleSize() : 0; }

	bool		IsReadyToMix();

	int			GetPositionForSave() { return GetSamplePosition(); }
	void		SetPositionFromSaved( int savedPosition ) { SetSampleStart( savedPosition ); }

private:
	CAudioMixer			*LoadWord( int nWordIndex );
	void				FreeWord( int nWordIndex );

	// identifies the active word
	int					m_currentWordIndex;
	CAudioMixer			*m_pCurrentWordMixer;

	// set when a transition to a new word occurs
	bool				m_bNewWord;

	voxword_t			m_VoxWords[CVOXWORDMAX];
	CAudioMixer			*m_pWordMixers[CVOXWORDMAX];
	int					m_nNumWords;
};

