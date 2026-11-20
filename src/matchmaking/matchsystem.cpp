// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"

#include "matchsystem.h"
#include "playermanager.h"

static CMatchSystem s_MatchSystem;
CMatchSystem *g_pMatchSystem = &s_MatchSystem;

bool IsValidXNKID( XNKID xnkid )
{
	for(int i = 0; i < 8; ++i)
	{
		if(xnkid.ab[i])
			return true;
	}
	return false;
}

CMatchSystem::CMatchSystem()
{
}

CMatchSystem::~CMatchSystem()
{
}

IPlayerManager * CMatchSystem::GetPlayerManager()
{
	return g_pPlayerManager;
}

IMatchVoice * CMatchSystem::GetMatchVoice()
{
	return g_pMatchVoice;
}

//-----------------------------------------------------------------------------
#include "facade/imatchsystem.inl"
import ks.core.platform;
