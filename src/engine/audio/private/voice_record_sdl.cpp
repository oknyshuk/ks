// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// This module implements the voice record and compression functions 

#include "ivoicerecord.h"


#define min(a,b)  (((a) < (b)) ? (a) : (b))
#include <AL/al.h>
#include <AL/alc.h>
import ks.core;

// ------------------------------------------------------------------------------
// IVoiceRecord
// ------------------------------------------------------------------------------

IVoiceRecord::IVoiceRecord() :
m_nSampleRate( 0 ), m_Device( nullptr )
{
	ClearInterfaces();
}

IVoiceRecord::~IVoiceRecord()
{
	ReleaseInterfaces();
}


void IVoiceRecord::Release()
{
	delete this;
}

bool IVoiceRecord::RecordStart()
{

	// Re-initialize the capture buffer if neccesary (should always be)
	if ( !m_Device )
	{
		InitalizeInterfaces();
	}
	
	if ( !m_Device )
		return false;

	alcGetError(m_Device);
	
	alcCaptureStart(m_Device);

	const ALenum error = alcGetError(m_Device);
	return error == AL_NO_ERROR;
}


void IVoiceRecord::RecordStop()
{
	// Stop capturing.
	if ( m_Device )
	{
		alcCaptureStop( m_Device );
	}
	
	// Release the capture buffer interface and any other resources that are no
	// longer needed
	ReleaseInterfaces();
}

bool IVoiceRecord::InitalizeInterfaces()
{	
	m_Device = alcCaptureOpenDevice( nullptr, m_nSampleRate, AL_FORMAT_MONO16, m_nSampleRate * 10 * 2);
	const ALenum error = alcGetError(m_Device);
	const bool result = error == AL_NO_ERROR;
	return m_Device != nullptr && result;
}

bool IVoiceRecord::Init(int sampleRate)
{
	m_nSampleRate = sampleRate;
	
	ReleaseInterfaces();
	
	return true;
}


void IVoiceRecord::ReleaseInterfaces()
{
    alcCaptureCloseDevice(m_Device);	
	ClearInterfaces();
}


void IVoiceRecord::ClearInterfaces()
{
	m_Device = nullptr;
}


void IVoiceRecord::Idle()
{
}


int IVoiceRecord::GetRecordedData(short *pOut, int nSamples )
{
	int frameCount = 0;
	alcGetIntegerv( m_Device,ALC_CAPTURE_SAMPLES,1,&frameCount );
	if ( frameCount > 0 )
	{
		frameCount = min( nSamples, frameCount );
		alcCaptureSamples( m_Device, pOut, frameCount );
		if ( alcGetError(m_Device) != ALC_NO_ERROR )
		{
			return 0;
		}
		return frameCount;
	}
	return 0;
}



IVoiceRecord* CreateVoiceRecord_DSound(int sampleRate)
{
	IVoiceRecord *pRecord = new IVoiceRecord;
	if(pRecord && pRecord->Init(sampleRate))
	{
		return pRecord;
	}
	else
	{
		if(pRecord)
			pRecord->Release();
		
		return nullptr;
	}
}
