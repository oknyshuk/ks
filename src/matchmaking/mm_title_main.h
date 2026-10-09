#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import std;
import ks.tier1;
#include "matchmaking/imatchtitle.h"
#include "matchmaking/imatchevents.h"

extern InitReturnVal_t MM_Title_Init();
extern void MM_Title_Shutdown();

extern IMatchTitle *g_pIMatchTitle;
extern IMatchEventsSink *g_pIMatchTitleEventsSink;

extern IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr;


//
// LINK_MATCHMAKING_LIB() macro must be included in the matchmaking.dll code
// to force all required matchmaking objects linked into the DLL.
//
extern void LinkMatchmakingLib();
#define LINK_MATCHMAKING_LIB() \
namespace { \
	static class CLinkMatchmakingLib { \
	public: \
		CLinkMatchmakingLib() { \
			LinkMatchmakingLib(); \
		} \
	} s_LinkHelper; \
};
