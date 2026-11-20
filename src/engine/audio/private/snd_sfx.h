#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.filename_table;

class CAudioSource;

// What the name of a sound table entry comes from
enum class SfxKind : unsigned char
{
	Named,		// a sound of the sound dictionary: the name its file name handle stands for
	Dummy,		// the one entry S_DummySfx() hands out (snd_dma.cpp)
	Voice,		// a voice channel (voice_sound_engine_interface.cpp)
};

class CSfxTable
{
public:
	CSfxTable();

	// gets sound name, possible decorated with prefixes
	const char	*getname( char *pBuf, std::size_t bufLen );
	// gets the filename, the part after the optional prefixes
	const char			*GetFileName(char *pBuf, std::size_t bufLen );
	FileNameHandle_t	GetFileNameHandle();

	void				SetFileNameHandle( FileNameHandle_t hName );
	bool				IsPrecachedSound();
	void				OnNameChanged( const char *pName );

	SfxKind				m_Kind;
	FileNameHandle_t	m_fnHandle;		// key in the sound dictionary, null until named
	CAudioSource		*pSource;

	bool				m_bUseErrorFilename : 1;
	bool				m_bIsUISound : 1;
	bool				m_bIsLateLoad : 1;
	bool				m_bMixGroupsCached : 1;
	bool				m_bIsMusic : 1;

	byte				m_mixGroupCount;
	// UNDONE: Use a fixed bit vec here?
	byte				m_mixGroupList[8];

private:
	// Only set in debug mode so you can see the name.
	const char *m_pDebugName;
};
