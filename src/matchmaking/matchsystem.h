#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.platform;
import ks.platform;
import ks.simd;
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
	IPlayerManager * GetPlayerManager();

	IMatchVoice * GetMatchVoice();

};

extern CMatchSystem *g_pMatchSystem;
