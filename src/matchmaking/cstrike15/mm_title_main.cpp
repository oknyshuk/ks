// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_title.h"
#include "../mm_title_main.h"


LINK_MATCHMAKING_LIB();

static CMatchTitle g_MatchTitle;
CMatchTitle *g_pMatchTitle = &g_MatchTitle;

IMatchTitle *g_pIMatchTitle = g_pMatchTitle;



//
// Init / shutdown
//

InitReturnVal_t MM_Title_Init()
{
	return g_pMatchTitle->Init();
}

void MM_Title_OnEvent( KeyValues *pEvent )
{
	g_pMatchTitle->OnEvent( pEvent );
}

void MM_Title_Shutdown()
{
	if ( g_pMatchTitle )
		g_pMatchTitle->Shutdown();
}
