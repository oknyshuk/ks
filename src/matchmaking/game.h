#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;
#include "mm_framework.h"
#include "matchmaking/igame.h"
import std;

//In an idea world game is manageing all of the aspects of the game session, however in our case 
//it will serve as an interface into the engine and matchmaking to get the information we need.
class CMatchGame : public IMatchGame
{
public:
	CMatchGame();

	~CMatchGame();

	//IMatchGame

	virtual void Reset();

	virtual XNKID GetXNKID();

	virtual int GetNumPlayers();

	virtual IPlayer * GetPlayer(int index);

	virtual void AddPlayer( XUID xuid, int pExperience[ MATCH_MAX_DIFFICULTIES ] = nullptr, int pSkills[ MATCH_MAX_DIFFICULTIES ][ MATCH_MAX_SKILL_FIELDS ] = nullptr );

	virtual void RemovePlayer( XUID xuid );

	virtual void RemoveAllPlayers( );

	virtual int GetAggregateExperience( );

	virtual int GetAggregateSkill( int iSkillType );

private:

	typedef std::vector< IPlayer * > IPlayerList_t;
	IPlayerList_t m_PlayerList;
};
