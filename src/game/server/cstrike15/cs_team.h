#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Team management class. Contains all the details for a specific team

import ks.tier1;
#include "reflect_annotations.h"

import std;
#include "team.h"

//-----------------------------------------------------------------------------
// Purpose: Team Manager
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_CSTeam" } ]]
      CCSTeam : public CTeam
{
	DECLARE_CLASS( CCSTeam, CTeam );
public:
	CCSTeam();
	virtual ~CCSTeam( void );

	DECLARE_SERVERCLASS();

	// Initialization
	virtual void Init( const char *pName, int iNumber );
	
	virtual void Precache( void );
	virtual void Think( void );

	void OnRoundPreStart( void );

	//-----------------------------------------------------------------------------
	// Players
	//-----------------------------------------------------------------------------
	virtual void AddPlayer( CBasePlayer *pPlayer );
	virtual void RemovePlayer( CBasePlayer *pPlayer );

	//-----------------------------------------------------------------------------
	// Utility funcs
	//-----------------------------------------------------------------------------
	CCSTeam*		GetEnemyTeam();

private:

	// Used to distribute resources to a team
	float	m_flNextResourceTime;

	int		m_iLastUpdateSentAt;
};


extern CCSTeam *GetGlobalCSTeam( int iIndex );
