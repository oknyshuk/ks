#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;
#include "mm_framework.h"

import std;

enum PacketTargetType
{
	PTT_PLAYER = 0,
	PTT_GAME,
};

bool IsValidXNKID( XNKID xnkid );

class CMatchSystem : public IMatchSystem
{
public:
	CMatchSystem();
	~CMatchSystem();

	//IMatchSystem
public:
	virtual IPlayerManager * GetPlayerManager();

	virtual IMatchVoice * GetMatchVoice();

	virtual IServerManager * GetUserGroupsServerManager();

	virtual ISearchManager * CreateGameSearchManager( KeyValues *pParams );

	virtual IDatacenter * GetDatacenter();

	virtual IDlcManager * GetDlcManager();
};

extern CMatchSystem *g_pMatchSystem;
