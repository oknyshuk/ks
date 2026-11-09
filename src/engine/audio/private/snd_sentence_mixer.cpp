// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Sentence Mixing




import ks.tier1;
#include "sound.h"
import std;
#include "soundservice.h"

#include "snd_device.h"
#include "sound_private.h"
#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"
#include "snd_sfx.h"

#include "audio/private/posix_stubs.h"
#include "vox_private.h"
#include "snd_sentence_mixer.h"

//-----------------------------------------------------------------------------
// Purpose: This replaces the old sentence logic that was integrated with the
//			sound code.  Now it is a hierarchical mixer.
//-----------------------------------------------------------------------------
CAudioMixer *CreateSentenceMixer( voxword_t *pWords )
{
	if ( pWords )
	{
		return new CSentenceMixer( pWords );
	}

	return nullptr;
}

CSentenceMixer::CSentenceMixer( voxword_t *pWords ) : CAudioMixer( AudioMixerKind::Sentence )
{
	// count the expected number of words
	m_nNumWords = 0;
	while ( pWords[m_nNumWords].sfx != nullptr )
	{
		// get a private copy of the words
		m_VoxWords[m_nNumWords] = pWords[m_nNumWords++];
		if ( m_nNumWords >= std::size( m_VoxWords ) )
		{
			// very long sentence, prevent overflow
			break;
		}
	}	

	// startup all the mixers now, this serves as a hint to the audio streamer
	// actual mixing will commence when they are ALL ready
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		// it is possible to get a null mixer (due to wav error, etc)
		// the sentence will skip these words
		m_pWordMixers[nWord] = LoadWord( nWord );
	}
	contract_assert( m_nNumWords < std::size( m_pWordMixers ) );

	// find first valid word mixer
	m_currentWordIndex = 0;
	m_pCurrentWordMixer = nullptr;
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		if ( m_pWordMixers[nWord] )
		{
			m_currentWordIndex = nWord;
			m_pCurrentWordMixer = m_pWordMixers[nWord];
			break;
		}
	}

	m_bNewWord = ( m_pCurrentWordMixer != nullptr );	
}

CSentenceMixer::~CSentenceMixer( void )
{
	// free all words
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		FreeWord( nWord );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if mixing can commence, false otherwise
//-----------------------------------------------------------------------------
bool CSentenceMixer::IsReadyToMix()
{
	if ( !m_pCurrentWordMixer )
	{
		// no word, but mixing has to commence in order to shutdown
		return true;
	}

	// all the words should be available before mixing the sentence
	for ( int nWord = m_currentWordIndex; nWord < m_nNumWords; nWord++ )
	{
		if ( m_pWordMixers[nWord] && !m_pWordMixers[nWord]->IsReadyToMix() )
		{
			// Still waiting for async data to arrive
			return false;
		}
	}

	if ( m_bNewWord )
	{
		m_bNewWord = false;

		int start = m_VoxWords[m_currentWordIndex].start;
		int end = m_VoxWords[m_currentWordIndex].end;
		
		// don't allow overlapped ranges
		if ( end <= start )
		{
			end = 0;
		}

		if ( start || end )
		{
			int sampleCount = m_pCurrentWordMixer->GetSource()->SampleCount();
			if ( start > 0 && start < 100 )
			{
				m_pCurrentWordMixer->SetSampleStart( (int)(sampleCount * 0.01f * start) );
			}
			if ( end > 0 && end < 100 )
			{
				m_pCurrentWordMixer->SetSampleEnd( (int)(sampleCount * 0.01f * end) );
			}
		}
	}

	return true;
}

bool CSentenceMixer::ShouldContinueMixing( void )
{
	if ( m_pCurrentWordMixer )
	{
		// keep mixing until the words run out
		return true;
	}

	return false;
}

CAudioSource *CSentenceMixer::GetSource( void )
{
	if ( m_pCurrentWordMixer )
	{
		return m_pCurrentWordMixer->GetSource();
	}

	return nullptr;
}
	
// get the current position (next sample to be mixed)
int CSentenceMixer::GetSamplePosition( void )
{
	if ( m_pCurrentWordMixer )
	{
		return m_pCurrentWordMixer->GetSamplePosition();
	}

	return 0;
}

bool CSentenceMixer::IsSetSampleStartSupported() const
{
	return true;
}

void CSentenceMixer::SetSampleStart( int newPosition )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetSampleStart( newPosition );
	}
}

// End playback at newEndPosition
void CSentenceMixer::SetSampleEnd( int newEndPosition )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetSampleEnd( newEndPosition );
	}
}

void CSentenceMixer::SetStartupDelaySamples( int delaySamples )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetStartupDelaySamples( delaySamples );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Free a word
//-----------------------------------------------------------------------------
void CSentenceMixer::FreeWord( int nWord )
{
	if ( m_pWordMixers[nWord] )
	{
		CAudioMixer::Delete( m_pWordMixers[nWord] );
		m_pWordMixers[nWord] = nullptr;
	}	

	if ( m_VoxWords[nWord].sfx )
	{
		// If this wave wasn't precached by the game code
		if ( !m_VoxWords[nWord].fKeepCached )
		{
			// If this was the last mixer that had a reference
			if ( m_VoxWords[nWord].sfx->pSource->CanDelete() )
			{
				// free the source
				CAudioSource::Delete( m_VoxWords[nWord].sfx->pSource );
				m_VoxWords[nWord].sfx->pSource = nullptr;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Load a word
//-----------------------------------------------------------------------------
CAudioMixer *CSentenceMixer::LoadWord( int nWord )
{
	CAudioMixer *pMixer = nullptr;
	if ( m_VoxWords[nWord].sfx )
	{
		SoundError soundError;
		CAudioSource *pSource = S_LoadSound( m_VoxWords[nWord].sfx, nullptr, soundError );
		if ( pSource )
		{
			pSource->SetSentenceWord( true );
			SoundError soundError;
			pMixer = pSource->CreateMixer( 0, 0, false, soundError );
		}
	}

	return pMixer;
}

float CSentenceMixer::ModifyPitch( float pitch )
{
	if ( m_pCurrentWordMixer )
	{
		if ( m_VoxWords[m_currentWordIndex].pitch > 0 )
		{
			pitch += (m_VoxWords[m_currentWordIndex].pitch - 100) * 0.01f;
		}
	}
	return pitch;
}

float CSentenceMixer::GetVolumeScale( void )
{
	if ( m_pCurrentWordMixer )
	{
		if ( m_VoxWords[m_currentWordIndex].volume )
		{
			float volume = m_VoxWords[m_currentWordIndex].volume * 0.01;
			if ( volume < 1.0f )
				return volume;
		}
	}
	return 1.0f;
}

int  CSentenceMixer::SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	contract_assert( 0 );
	return 0;
}

// return number of samples mixed
int CSentenceMixer::MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	if ( !m_pCurrentWordMixer )
	{
		return 0;
	}

	// save this to compute total output
	int startingOffset = outputOffset;

	while ( sampleCount > 0 && m_pCurrentWordMixer )
	{
		int outputCount = m_pCurrentWordMixer->MixDataToDevice( pChannel, sampleCount, outputRate, outputOffset );

		outputOffset += outputCount;
		sampleCount -= outputCount;

		if ( !m_pCurrentWordMixer->ShouldContinueMixing() )
		{
			if ( pChannel->flags.m_bHasMouth )
			{
				SND_ClearMouth( pChannel );
			}
			// advance to next valid word mixer
			do 
			{
				m_currentWordIndex++;
				if ( m_currentWordIndex >= m_nNumWords )
				{
					// end of sentence
					m_pCurrentWordMixer = nullptr;
					break;
				}
				m_pCurrentWordMixer = m_pWordMixers[m_currentWordIndex];
			}
			while ( m_pCurrentWordMixer == nullptr );

			if ( m_pCurrentWordMixer )
			{
				m_bNewWord = true;

				pChannel->sfx = m_VoxWords[m_currentWordIndex].sfx;
				if ( !IsReadyToMix() )
				{
					// current word isn't ready, stop mixing
					break;
				}
			}
		}
	}

	return outputOffset - startingOffset;
}
