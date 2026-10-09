#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Client handler for instruction players how to play

import ks.tier1;
#include "GameEventListener.h"
import std;
#include "igamesystem.h"

typedef void (*KeyValueBuilder)( KeyValues* );


struct KeyValueSaverData 
{
	char szFileName[ MAX_PATH ];
	bool bDirtySaveData;
	KeyValues *pKeyValues;
	KeyValueBuilder funcKeyValueBuilder;
};


class C_KeyValueSaver : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	C_KeyValueSaver() : CAutoGameSystemPerFrame( "C_KeyValueSaver" )
	{
		m_nSplitScreenSlot = -1;
	}

	void SetSlot( int nSlot ) { m_nSplitScreenSlot = nSlot; }

	// Methods of IGameSystem
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void Update( float frametime );

	// Methods of CGameEventListener
	virtual void FireGameEvent( IGameEvent *event );

	bool InitKeyValues( const char *pchFileName, KeyValueBuilder funcKeyValueBuilder );
	bool WriteDirtyKeyValues( const char *pchFileName, bool bForceWrite = false );

	KeyValues * GetKeyValues( const char *pchFileName, bool bForceReread = false );
	void MarkKeyValuesDirty( const char *pchFileName );

private:

	bool ReadKeyValues( KeyValueSaverData *pKeyValueData );
	bool WriteDirtyKeyValues( KeyValueSaverData *pKeyValueData, bool bForceWrite = false );
	void WriteAllDirtyKeyValues( void );

	KeyValueSaverData * FindKeyValueData( const char *pchFileName );

private:
	
	std::vector< KeyValueSaverData > m_KeyValueData;

	int		m_nSplitScreenSlot;
};

C_KeyValueSaver &KeyValueSaver();

// Merged from L4D but waiting on other code to be merged before this can compile 
void GameInstructor_Init();
void GameInstructor_Shutdown();
