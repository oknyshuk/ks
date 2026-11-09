#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

struct ALCdevice_struct;

// This is the voice recording interface. It provides 16-bit signed mono data from 
// a mic at some sample rate. (The one implementation records through OpenAL: voice_record_sdl.cpp.)
class IVoiceRecord
{
protected:

	~IVoiceRecord();

public:
	
	IVoiceRecord();

	// Use this to delete the object.
	void		Release();

	// Start/stop capturing.
	bool		RecordStart();
	void		RecordStop();

	// Initialize. The format of the data we expect from the provider is
	// 8-bit signed mono at the specified sample rate.
	bool		Init(int sampleRate);

	// Idle processing.
	void		Idle();

	// Get the most recent N samples. If nSamplesWanted is less than the number of
	// available samples, it discards the first samples and gives you the last ones.
	int			GetRecordedData(short *pOut, int nSamplesWanted);

private:
	bool				InitalizeInterfaces();	// Initialize the openal capture buffers and other interfaces
	void				ReleaseInterfaces();	// Release openal buffers and other interfaces
	void				ClearInterfaces();				// Clear members.

	ALCdevice_struct	*m_Device;

	int	m_nSampleRate;
};
