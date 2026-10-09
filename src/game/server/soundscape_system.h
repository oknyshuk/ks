#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "stringregistry.h"
import std;
#include "igamesystem.h"
class CEnvSoundscape;

struct clusterSoundscapeList_t
{
	unsigned short	soundscapeCount;
	unsigned short	firstSoundscape;
};



class CSoundscapeSystem : public CAutoGameSystemPerFrame
{
public:
	CSoundscapeSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	// game system
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void FrameUpdatePostEntityThink( void );
	virtual void LevelInitPreEntity( void );
	virtual void LevelInitPostEntity();

	virtual void AddSoundscapeFile( const char *filename );
	int	GetSoundscapeIndex( const char *pName );
	bool IsValidIndex( int index );

	void FlushSoundscapes( void );
	void AddSoundscapeEntity( CEnvSoundscape *pSoundscape );
	void RemoveSoundscapeEntity( CEnvSoundscape *pSoundscape );
	void PrintDebugInfo( void );

	void AddSoundscapeSounds( KeyValues *pSoundscape, int soundscapeIndex );
	void PrecacheSounds( int soundscapeIndex );

private:
	CStringRegistry							m_soundscapes;
	int										m_soundscapeCount;
	std::vector< CEnvSoundscape * >			m_soundscapeEntities;
	std::vector< clusterSoundscapeList_t >		m_soundscapesInCluster;
	std::vector< unsigned short >				m_soundscapeIndexList;
	int										m_activeIndex;
	std::vector< std::vector< std::string > >	m_soundscapeSounds;
};

extern CSoundscapeSystem g_SoundscapeSystem;
