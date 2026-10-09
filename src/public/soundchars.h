#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr auto CHAR_STREAM = '*'; // as one of 1st 2 chars in name, indicates streaming wav data
inline constexpr auto CHAR_USERVOX = '?'; // as one of 1st 2 chars in name, indicates user realtime voice data
inline constexpr auto CHAR_SENTENCE = '!'; // as one of 1st 2 chars in name, indicates sentence wav
inline constexpr auto CHAR_DRYMIX = '#'; // as one of 1st 2 chars in name, indicates wav bypasses dsp fx
inline constexpr auto CHAR_DOPPLER = '>'; // as one of 1st 2 chars in name, indicates doppler encoded stereo wav: left wav (incomming) and right wav (outgoing).
inline constexpr auto CHAR_DIRECTIONAL = '<'; // as one of 1st 2 chars in name, indicates stereo wav has direction cone: mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
inline constexpr auto CHAR_DISTVARIANT = '^'; // as one of 1st 2 chars in name, indicates distance variant encoded stereo wav (left is close, right is far)
inline constexpr auto CHAR_OMNI = '@'; // as one of 1st 2 chars in name, indicates non-directional wav (default mono or stereo)
inline constexpr auto CHAR_HRTF = '~'; // as one of 1st 2 chars in name, indicates wav should use HRTF spatialization for non-owners.
inline constexpr auto CHAR_RADIO = '+'; // as one of 1st 2 chars in name, indicates a 'radio' sound -- should be played without spatialization
inline constexpr auto CHAR_SPATIALSTEREO = ')'; // as one of 1st 2 chars in name, indicates spatialized stereo wav
inline constexpr auto CHAR_DIRSTEREO = '('; // as one of 1st 2 chars in name, indicates directional stereo wav (like doppler)
inline constexpr auto CHAR_FAST_PITCH = '}'; // as one of 1st 2 chars in name, forces low quality, non-interpolated pitch shift
inline constexpr auto CHAR_SUBTITLED = '$'; // as one of 1st 2 chars in name, indicates the subtitles are forced

inline bool IsSoundChar(char c)
{
	bool b;

	b = ( c == CHAR_STREAM || c == CHAR_USERVOX || c == CHAR_SENTENCE || c == CHAR_DRYMIX || c == CHAR_OMNI || c == CHAR_RADIO || c == CHAR_DIRSTEREO );
	b = b || ( c == CHAR_DOPPLER || c == CHAR_DIRECTIONAL || c == CHAR_DISTVARIANT || c == CHAR_HRTF || c == CHAR_SPATIALSTEREO || c == CHAR_FAST_PITCH );
	b = b || ( c == CHAR_SUBTITLED );

	return b;
}

// return pointer to first valid character in file name
// by skipping over CHAR_STREAM...CHAR_DRYMIX

inline char *PSkipSoundChars(const char *pch)
{
	char *pcht = (char *)pch;

	while ( 1 )
	{
		if (!IsSoundChar(*pcht))
			break;
		pcht++;
	}

	return pcht;
}


inline bool TestSoundChar(const char *pch, char c)
{
	char *pcht = (char *)pch;

	while ( 1 )
	{
		if (!IsSoundChar(*pcht))
			break;
		if (*pcht == c)
			return true;
		pcht++;
	}

	return false;
}
