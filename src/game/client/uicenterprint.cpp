//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <stdarg.h>
#include "uicenterprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef TF_CLIENT_DLL
static ConVar		scr_centertime( "scr_centertime", "5" );
#else
static ConVar		scr_centertime( "scr_centertime", "4" );
#endif

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
