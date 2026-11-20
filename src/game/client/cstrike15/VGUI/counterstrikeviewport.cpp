// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdlib.h>
#include "IGameUIFuncs.h"
#include "edict.h"
#include "game/client/iviewport.h"
#include "igameevents.h"
#include "../../cdll_client_int.h"
#include "../../cdll_util.h"
module ks.client;

import std;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.RocketUI.rkconsole;
import :cstrike15.VGUI.counterstrikeviewport;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :game_controls.baseviewport;
import :precache;
import ks.game.shareddefs;
import :triggers;
import :achievements_and_stats_interface;
import :baseentity;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import :particlemgr;
import :noopanimcontroller;

class CCSPlayer;

using namespace ::ks::reflect;

// Purpose: Client DLL VGUI2 Viewport

// vgui panel includes

// client dll/engine defines

// cstrike specific dialogs

// viewport definitions
// #include "user_message_register.h"

ConVar crosshair( side_registry(), "crosshair", "1", FCVAR_ARCHIVE  | FCVAR_SS );
ConVar cl_disablefreezecam(
        side_registry(), "cl_disablefreezecam",
        "0",
        FCVAR_ARCHIVE,
        "Turn on/off freezecam on client"
);
static void OpenPanelWithCheck( const char *panelToOpen, const char *panelToCheck )
{
	IViewPortPanel *checkPanel = GetViewPortInterface()->FindPanelByName( panelToCheck );
	if ( !checkPanel || !checkPanel->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( panelToOpen, true );
	}
}

void PrintBuyTimeOverMessage( void )
{
}

static void teammenu( const CCommand &args );
static ConCommand teammenu_command( side_registry(), "teammenu", teammenu, "Show team selection window", FCVAR_SERVER_CAN_EXECUTE );
static void teammenu( const CCommand &args )
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	
	if( pPlayer && pPlayer->CanShowTeamMenu() )
	{
		( ( CounterStrikeViewport * )GetViewPortInterface() )->SetChoseTeamAndClass( true );
		// RmlUi team-select (VGUI teardown): the vgui PANEL_TEAM panel no longer exists.
		RocketTeamMenuDocument::ShowPanel( true );
	}

}

static void buymenu( const CCommand &args );
static ConCommand buymenu_command( side_registry(), "buymenu", buymenu, "Show or hide main buy menu", FCVAR_SERVER_CAN_EXECUTE );
extern ConVar mp_buy_anywhere;
extern ConVar mp_buy_during_immunity;
static void buymenu( const CCommand &args )
{
	bool bShowIt = true;

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( args.ArgC() == 2 )
	{
		bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if( pPlayer && bShowIt )
	{
		if ( pPlayer->m_lifeState != LIFE_ALIVE && pPlayer->State_Get() != STATE_ACTIVE )
			return;

		static ConVarRef sv_buy_status_override_ref( "sv_buy_status_override" );
		// UNUSED: int nGuardianTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;

		if ( CSGameRules()->IsPlayingCooperativeGametype() )
		{
			if ( CSGameRules()->IsWarmupPeriod() == false && 
				 CSGameRules()->m_flGuardianBuyUntilTime < gpGlobals->curtime )
			{
				int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
				int iBuyStatus = sv_buy_status_override_ref.GetInt();
				if ( iBuyStatus > 0 && (( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 )) )
					GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuy" );
				else
					GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuyTilNextWave" );
			}
			else
			{
				CSGameRules()->OpenBuyMenu( pPlayer->GetUserID() );
			}
		}
		else if ( CSGameRules()->IsPlayingCoopMission() && sv_buy_status_override_ref.GetInt() == 3 )
		{
			GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuy" );
		}
		else if ( !pPlayer->IsInBuyPeriod() )
		{
			PrintBuyTimeOverMessage();
		}
		else if ( !pPlayer->IsInBuyZone()  )
		{
			GetCenterPrint()->Print( "#SFUI_BuyMenu_NotInBuyZone" );
		}
		else
		{
			CSGameRules()->OpenBuyMenu( pPlayer->GetUserID() );
		}
	}
	else if( pPlayer && !bShowIt )
	{
		// Hide the menu
		CSGameRules()->CloseBuyMenu( pPlayer->GetUserID() );
	}

}

//CON_COMMAND_F( spec_help, "Show spectator help screen", FCVAR_CLIENTCMD_CAN_EXECUTE )
//{
//	if ( GetViewPortInterface() )
//		GetViewPortInterface()->ShowPanel( PANEL_INFO, true );
//}

static void spec_menu( const CCommand &args );
static ConCommand spec_menu_command( side_registry(), "spec_menu", spec_menu, "Activates spectator menu", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void spec_menu( const CCommand &args )
{
	bool bShowIt = true;

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( pPlayer && !pPlayer->IsObserver() )
		return;

	if ( args.ArgC() == 2 )
	{
		 bShowIt = atoi( args[ 1 ] ) == 1;
	}
	
	if ( GetViewPortInterface() )
		GetViewPortInterface()->ShowPanel( PANEL_SPECMENU, bShowIt );
}

static void spec_gui( const CCommand &args );
static ConCommand spec_gui_command( side_registry(), "spec_gui", spec_gui, "Shows or hides the spectator bar", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void spec_gui( const CCommand &args )
{
	bool bShowIt = true;

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( pPlayer && !pPlayer->IsObserver() )
		return;

	if ( args.ArgC() == 2 )
	{
		bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if ( bShowIt && GetViewPortInterface() && GetViewPortInterface()->GetActivePanel() )
	{
		// if the team screen is up, it takes precedence - don't show the spectator GUI
		if ( !V_strcmp( GetViewPortInterface()->GetActivePanel()->GetName(), PANEL_TEAM ) )
			return;
	}

	if ( GetViewPortInterface() )
		GetViewPortInterface()->ShowPanel( PANEL_SPECGUI, bShowIt );
}

static void togglescores( const CCommand &args );
static ConCommand togglescores_command( side_registry(), "togglescores", togglescores, "Toggles score panel", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void togglescores( const CCommand &args )
{
	if ( !GetViewPortInterface() )
		return;
	
	IViewPortPanel *scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );

	if ( !scoreboard )
		return;

	if ( scoreboard->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( scoreboard, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
	else
	{
		GetViewPortInterface()->ShowPanel( scoreboard, true );
	}
}

static void hidescores( const CCommand &args );
static ConCommand hidescores_command( side_registry(), "hidescores", hidescores, "Forcibly hide score panel", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void hidescores( const CCommand &args )
{
	if ( !GetViewPortInterface() )
		return;

	IViewPortPanel *scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );

	if ( !scoreboard )
		return;

	if ( scoreboard->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( scoreboard, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the VGUI subsystem starts up
//			Creates the sub panels and initialises them
//-----------------------------------------------------------------------------
void CounterStrikeViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	BaseClass::Start( pGameUIFuncs, pGameEventManager );
	SetChoseTeamAndClass( false );
}

IViewPortPanel* CounterStrikeViewport::CreatePanelByName( const char *szPanelName )
{
	IViewPortPanel* newpanel = nullptr;

	// overwrite MOD specific panel creation

	// create a generic base panel, don't add twice
	newpanel = BaseClass::CreatePanelByName( szPanelName );

	return newpanel; 
}

void CounterStrikeViewport::CreateDefaultPanels( void )
{
	AddNewPanel( CreatePanelByName( PANEL_TEAM ), "PANEL_TEAM" );

	AddNewPanel( CreatePanelByName( PANEL_BUY ), "PANEL_BUY" );

	BaseClass::CreateDefaultPanels();
}

int CounterStrikeViewport::GetDeathMessageStartHeight( void )
{
	int x = YRES( 2 );

	return x;
}

void CounterStrikeViewport::FireGameEvent( IGameEvent * event )
{
	const char * type = event->GetName();

	if ( V_strcmp( type, "game_newmap" ) == 0 || V_strcmp( type, "cs_win_panel_match" ) == 0 )
	{
		SetChoseTeamAndClass( false );
	}

	BaseClass::FireGameEvent( event );
}

void CounterStrikeViewport::UpdateAllPanels( void )
{
	bool bSomethingIsVisible = false;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	for ( int i = 0; i < std::ssize( m_UnorderedPanels ); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];

		if ( p->IsVisible() )
		{
			bSomethingIsVisible = true;
			p->Update();
			if ( m_pActivePanel == nullptr )
			{
				// if a visible panel exists, then there should be an activePanel.
				m_pActivePanel = p;
			}
		}
	}

	// see if we need to show a special ui instead of the hud
	if ( !bSomethingIsVisible )
	{
		CCSPlayer *pCSPlayer = CCSPlayer::GetLocalCSPlayer();

		const char* UIToShow = nullptr;

		if ( !pCSPlayer )
		{
			UIToShow = PANEL_SPECGUI;
		}
		else if ( pCSPlayer->GetObserverMode() != OBS_MODE_NONE )
		{
			if ( pCSPlayer->State_Get() != STATE_PICKINGTEAM && ( pCSPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) && !pCSPlayer->IsHLTV() )
			{
				// not a member of a team and not a spectator. show the team select screen.
				if ( (GetActivePanel() && !V_strcmp( GetActivePanel()->GetName(), PANEL_TEAM )) || !GetActivePanel() )
				{
					// don't show the team panel if the team panel is already up
					UIToShow = PANEL_TEAM;
				}
			}
			else
			{
				UIToShow = PANEL_SPECGUI;
			}
		}

		if ( UIToShow )
		{
			ShowPanel( UIToShow, true );
		}
	}
}

/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text
==========================
*/
//MIKETODO: positioning of chat text ( and other engine output )
/*
	#include "Exports.h"

	void CL_DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
	{
		RecClChatInputPosition( x, y );
		if ( GetViewPortInterface() )
		{
			GetViewPortInterface()->ChatInputPosition( x, y );
		}
	}

*/
