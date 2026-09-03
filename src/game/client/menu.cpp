//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// menu.cpp
//
// generic menu handler
//
#include "cbase.h"
#include "text_message.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "weapon_selection.h"

#include "localize/ilocalize.h"
#include <keyvalues.h>

#define MAX_MENU_STRING	512
wchar_t g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

#include "menu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudMenu );
DECLARE_HUD_MESSAGE( CHudMenu, ShowMenu );

//
//-----------------------------------------------------
//

static char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMenu::CHudMenu( const char *pElementName ) :
	CHudElement( pElementName )
{
	m_nSelectedItem = -1;
	m_flOpenCloseTime = 1.0f;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Init( void )
{
	HOOK_HUD_MESSAGE( CHudMenu, ShowMenu );

	m_bMenuTakesInput = false;
	m_bMenuDisplayed = false;
	m_bitsValidSlots = 0;
	m_Processed.RemoveAll();
	m_nMaxPixels = 0;
	m_nHeight = 0;
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudMenu::IsMenuOpen( void )
{
	return m_bMenuDisplayed && m_bMenuTakesInput;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::OnThink()
{
	float flSelectionTimeout = MENU_SELECTION_TIMEOUT;

	// If we've been open for a while without input, hide
	if ( m_bMenuDisplayed && ( gpGlobals->curtime - m_flSelectionTime > flSelectionTimeout ) )
	{
		m_bMenuDisplayed = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMenu::ShouldDraw( void )
{
	bool draw = CHudElement::ShouldDraw() && m_bMenuDisplayed;
	if ( !draw )
		return false;

	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 && m_flShutoffTime <= gpGlobals->realtime )
	{  
		// times up, shutoff
		m_bMenuDisplayed = false;
		return false;
	}

	return draw;
}

//-----------------------------------------------------------------------------
// Purpose: selects an item from the menu
//-----------------------------------------------------------------------------
void CHudMenu::SelectMenuItem( int menu_item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		Q_snprintf( szbuf, sizeof( szbuf ), "menuselect %d\n", menu_item );
		engine->ClientCmd( szbuf );

		m_nSelectedItem = menu_item;
		// Pulse the selection
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuPulse");

		// remove the menu quickly
		m_bMenuTakesInput = false;
		m_flShutoffTime = gpGlobals->realtime + m_flOpenCloseTime;
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuClose");
	}
}

void CHudMenu::ProcessText( void )
{
	m_Processed.RemoveAll();
	m_nMaxPixels = 0;
	m_nHeight = 0;

	int i = 0;
	int startpos = i;
	int menuitem = 0;
	while ( i < MAX_MENU_STRING  )
	{
		wchar_t ch = g_szMenuString[ i ];
		if ( ch == 0 )
			break;

		if ( i == startpos && 
			( ch == L'-' && g_szMenuString[ i + 1 ] == L'>' ) )
		{
			// Special handling for menu item specifiers
			swscanf( &g_szMenuString[ i + 2 ], L"%d", &menuitem );
			i += 2;
			startpos += 2;

			continue;
		}

		// Skip to end of line
		while ( i < MAX_MENU_STRING && g_szMenuString[i] != 0 && g_szMenuString[i] != L'\n' )
		{
			i++;
		}

		// Store off line
		if ( ( i - startpos ) >= 1 )
		{
			ProcessedLine line;
			line.menuitem = menuitem;
			line.startchar = startpos;
			line.length = i - startpos;
			line.pixels = 0;
			line.height = 0;

			m_Processed.AddToTail( line );
		}

		menuitem = 0;

		// Skip delimiter
		if ( g_szMenuString[i] == '\n' )
		{
			i++;
		}
		startpos = i;
	}

	// Add final block
	if ( i - startpos >= 1 )
	{
		ProcessedLine line;
		line.menuitem = menuitem;
		line.startchar = startpos;
		line.length = i - startpos;
		line.pixels = 0;
		line.height = 0;

		m_Processed.AddToTail( line );
	}
}
//-----------------------------------------------------------------------------
// Purpose: Local method to hide a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//-----------------------------------------------------------------------------
void CHudMenu::HideMenu( void )
{
	m_bMenuTakesInput = false;
	m_flShutoffTime = gpGlobals->realtime + m_flOpenCloseTime;
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuClose");
}

//-----------------------------------------------------------------------------
// Purpose: Local method to bring up a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//
//   takes two values:
//		menuName  : menu name string 
//		validSlots: a bitfield describing the valid keys
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu( const char * menuName, int validSlots )
{
	m_flShutoffTime = -1;
	m_bitsValidSlots = validSlots;

	Q_strncpy( g_szPrelocalisedMenuString, menuName, sizeof( g_szPrelocalisedMenuString ) );

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;

	// we have the whole string, so we can localise it now
	char szMenuString[MAX_MENU_STRING];
    hudtextmessage->LocaliseTextString( g_szPrelocalisedMenuString, szMenuString, sizeof( szMenuString ) );
    ConvertCRtoNL( szMenuString );
	g_pLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
	
	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu_KeyValueItems( KeyValues *pKV )
{
	m_flShutoffTime = -1;
	m_bitsValidSlots = 0;

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;
	
	g_szMenuString[0] = '\0';

	wchar_t wItem[128];

	int i = 0;
	for ( KeyValues *item = pKV->GetFirstSubKey(); item != NULL; item = item->GetNextKey() )
	{
		// Set this slot valid
		m_bitsValidSlots |= (1<<i);

		const char *pszItem = item->GetName();
		const wchar_t *wLocalizedItem = g_pLocalize->Find( pszItem );

		Q_snwprintf( wItem, sizeof( wItem )/ sizeof( wchar_t ), L"%d. %ls\n", i+1, wLocalizedItem );

		Q_snwprintf( g_szMenuString, sizeof( g_szMenuString )/ sizeof( wchar_t ), L"%ls%ls", g_szMenuString, wItem );

		i++;
	}

	// put a cancel on the end
	m_bitsValidSlots |= (1<<9);

	Q_snwprintf( wItem, sizeof( wItem )/ sizeof( wchar_t ), L"0. %ls", g_pLocalize->Find( "#Cancel" ) );

	Q_snwprintf( g_szMenuString, sizeof( g_szMenuString )/ sizeof( wchar_t ), L"%ls\n%ls", g_szMenuString, wItem );

	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for ShowMenu message
//   takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, false if it's the last string
//		string: menu string to display
//  if this message is never received, then scores will simply be the combined totals of the players.
//-----------------------------------------------------------------------------
bool CHudMenu::MsgFunc_ShowMenu( const ks::net::CCSUsrMsg_ShowMenu &msg)
{
	m_bitsValidSlots = msg.bits_valid_slots;
	int DisplayTime = msg.display_time;
	
	if ( DisplayTime > 0 )
	{
		m_flShutoffTime = m_flOpenCloseTime + DisplayTime + gpGlobals->realtime;

	}
	else
	{
		m_flShutoffTime = -1;
	}

	if ( m_bitsValidSlots )
	{
		Q_strncpy( g_szPrelocalisedMenuString, msg.menu_string->c_str(), sizeof( g_szPrelocalisedMenuString ) );

		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
		m_nSelectedItem = -1;
			
		// we have the whole string, so we can localise it now
		char szMenuString[MAX_MENU_STRING];
		hudtextmessage->LocaliseTextString( g_szPrelocalisedMenuString, szMenuString, sizeof( szMenuString ) );
		ConvertCRtoNL( szMenuString );
		g_pLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
			
		ProcessText();

		m_bMenuDisplayed = true;
		m_bMenuTakesInput = true;

		m_flSelectionTime = gpGlobals->curtime;
	}
	else
	{
		HideMenu();
	}

	return true;
}
