// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "uicenterprint.h"

static ConVar		scr_centertime( "scr_centertime", "4" );

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CCenterPrint::CCenterPrint( void )
{
}

void CCenterPrint::SetTextColor( int r, int g, int b, int a )
{
}

void CCenterPrint::Print( const char *text )
{
}

void CCenterPrint::Print( const wchar_t *text )
{
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, char *text )
{
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, wchar_t *text )
{
}

void CCenterPrint::Clear( void )
{
}

void CCenterPrint::Destroy( void )
{
}

static CCenterPrint g_CenterString[ MAX_SPLITSCREEN_PLAYERS ];
CCenterPrint *GetCenterPrint()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_CenterString[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}
