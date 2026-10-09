// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client DLL VGUI2 Viewport

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include <cdll_client_int.h>
#include <cdll_util.h>



#include "IGameUIFuncs.h"
#include "hud.h"

// our definition
#include "baseviewport.h"
#include <filesystem.h>
import ks.tier1;
import std;

static IViewPort *s_pFullscreenViewportInterface;
static IViewPort *s_pViewportInterfaces[ MAX_SPLITSCREEN_PLAYERS ];

IViewPort *GetViewPortInterface()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return s_pViewportInterfaces[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

IViewPort *GetFullscreenViewPortInterface()
{
	return s_pFullscreenViewportInterface;
}

ConVar hud_autoreloadscript("hud_autoreloadscript", "0", FCVAR_NONE, "Automatically reloads the animation script each time one is ran");

static ConVar cl_leveloverviewmarker( "cl_leveloverviewmarker", "0", FCVAR_CHEAT );

static void showpanel( const CCommand &args );
static ConCommand showpanel_command( "showpanel", showpanel, "Shows a viewport panel <name>" );
static void showpanel( const CCommand &args )
{
	if ( !GetViewPortInterface() )
		return;

	if ( args.ArgC() != 2 )
		return;

	GetViewPortInterface()->ShowPanel( args[ 1 ], true );
}

static void hidepanel( const CCommand &args );
static ConCommand hidepanel_command( "hidepanel", hidepanel, "Hides a viewport panel <name>" );
static void hidepanel( const CCommand &args )
{
	if ( !GetViewPortInterface() )
		return;

	if ( args.ArgC() != 2 )
		return;

	GetViewPortInterface()->ShowPanel( args[ 1 ], false );
}

//================================================================
CBaseViewport::CBaseViewport()
{	
	m_bInitialized = false;
	m_bFullscreenViewport = false;

	m_GameuiFuncs = nullptr;
	m_GameEventManager = nullptr;

	m_bHasParent = false;
	m_pActivePanel = nullptr;


	m_OldSize[ 0 ] = m_OldSize[ 1 ] = -1;
}

void CBaseViewport::CreateDefaultPanels( void )
{
}

void CBaseViewport::UpdateAllPanels( void )
{
	for ( int i = 0; i < std::ssize( m_UnorderedPanels ); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];

		if ( p->IsVisible() )
		{
			p->Update();
		}
	}
}

IViewPortPanel* CBaseViewport::CreatePanelByName(const char *szPanelName)
{
	return nullptr;
}


bool CBaseViewport::AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName )
{
	if ( !pPanel )
	{
		return false;
	}

	if ( FindPanelByName( pPanel->GetName() ) != nullptr )
	{
		DevMsg("CBaseViewport::AddNewPanel: panel with name '%s' already exists.\n", pPanel->GetName() );
		return false;
	}

	m_Panels.emplace( pPanel->GetName(), pPanel );
	m_UnorderedPanels.push_back( pPanel );

	return true;
}

IViewPortPanel* CBaseViewport::FindPanelByName(const char *szPanelName)
{
	auto it = m_Panels.find( szPanelName );
	if ( it == m_Panels.end() )
		return nullptr;

	return it->second;
}

void CBaseViewport::PostMessageToPanel( const char *pName, KeyValues *pKeyValues )
{
}


void CBaseViewport::ShowPanel( const char *pName, bool state, KeyValues *data, bool autoDeleteData )
{
	if ( !data )
	{
		ShowPanel( pName, state );
		return;
	}

	// Also try to show the panel in the full screen viewport
	if ( this != s_pFullscreenViewportInterface )
	{
		GetFullscreenViewPortInterface()->ShowPanel( pName, state, data, false );
	}

	IViewPortPanel *panel = FindPanelByName( pName );
	if ( panel )
	{
		panel->SetData( data );
		GetViewPortInterface()->ShowPanel( panel, state );
	}

	if ( autoDeleteData )
	{
		data->deleteThis();
	}
}


void CBaseViewport::ShowPanel( const char *pName, bool state )
{
	// Also try to show the panel in the full screen viewport
	if ( this != s_pFullscreenViewportInterface )
	{
		GetFullscreenViewPortInterface()->ShowPanel( pName, state );
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( V_strcmp( pName, PANEL_ALL ) == 0 )
	{
		for ( int i = 0; i < std::ssize( m_UnorderedPanels ); ++i )
		{
			IViewPortPanel *p = m_UnorderedPanels[i];
			ShowPanel( p, state );
		}

		return;
	}

	IViewPortPanel * panel = nullptr;

	if ( V_strcmp( pName, PANEL_ACTIVE ) == 0 )
	{
		panel = m_pActivePanel;
	}
	else
	{
		panel = FindPanelByName( pName );
	}

	if ( !panel	)
		return;

	ShowPanel( panel, state );
}

void CBaseViewport::ShowPanel( IViewPortPanel* pPanel, bool state )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );

	if ( state )
	{
		// if this is an 'active' panel, deactivate old active panel
		if ( pPanel->HasInputElements() )
		{
			// don't show input panels during normal demo playback
			if ( engine->IsPlayingDemo() && !g_bEngineIsHLTV
				)
				return;

			if ( (m_pActivePanel != nullptr) && (m_pActivePanel != pPanel) && (m_pActivePanel->IsVisible()) )
			{
				// store a pointer to the currently active panel
				// so we can restore it later
				if ( pPanel->CanReplace( m_pActivePanel->GetName() ) )
				{

					// in cs, if the scoreboard tries to hide the spectator via this method, just skip it
					IViewPortPanel* pSpecGuiPanel = FindPanelByName(PANEL_SPECGUI);
					if ( pSpecGuiPanel != m_pActivePanel )
					{
						DevMsg("CBaseViewport::ShowPanel(0) %s\n", m_pActivePanel->GetName());
						m_pActivePanel->ShowPanel( false );
					}
				}
				else
				{
					return;
				}
			}

			m_pActivePanel = pPanel;
		}
	}
	else
	{
		// if this is our current active panel
		// update m_pActivePanel pointer
		if ( m_pActivePanel == pPanel )
		{
			m_pActivePanel = nullptr;
		}

	}

	// just show/hide panel
	DevMsg("CBaseViewport::ShowPanel(%d) %s\n", (int)state, pPanel->GetName());
	pPanel->ShowPanel( state );

	UpdateAllPanels(); // let other panels rearrange
}

IViewPortPanel* CBaseViewport::GetActivePanel( void )
{
	return m_pActivePanel;
}

void CBaseViewport::RecreatePanel( const char *szPanelName )
{
	IViewPortPanel *panel = FindPanelByName( szPanelName );
	if ( panel )
	{
		m_Panels.erase( m_Panels.find( szPanelName ) );
		for ( int i = std::ssize( m_UnorderedPanels ) - 1; i >= 0; --i )
		{
			if ( m_UnorderedPanels[ i ] == panel )
			{
				m_UnorderedPanels.erase( m_UnorderedPanels.begin() + i );
				break;
			}
		}

		delete panel;

		if ( m_pActivePanel == panel )
		{
			m_pActivePanel = nullptr;
		}


		AddNewPanel( CreatePanelByName( szPanelName ), szPanelName );
	}
}


void CBaseViewport::RemoveAllPanels( void)
{
	for ( int i = 0; i < std::ssize( m_UnorderedPanels ); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		delete p;
	}

	m_Panels.clear();
	m_UnorderedPanels.clear();
	m_pActivePanel = nullptr;
}

CBaseViewport::~CBaseViewport()
{
	m_bInitialized = false;

	RemoveAllPanels();
}

void CBaseViewport::InitViewportSingletons( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	s_pViewportInterfaces[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = this;
}

//-----------------------------------------------------------------------------
// Purpose: called when the viewport subsystem starts up
//-----------------------------------------------------------------------------
void CBaseViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	InitViewportSingletons();

	m_GameuiFuncs = pGameUIFuncs;
	m_GameEventManager = pGameEventManager;

	ListenForGameEvent( "game_newmap" );

	if ( !IsFullscreenViewport() )
	{
		CreateDefaultPanels();
	}

	m_bInitialized = true;
}

// Return true if the HUD's allowed to print text messages
bool CBaseViewport::AllowedToPrintText( void )
{
	return ( m_pActivePanel == nullptr);
} 

//-----------------------------------------------------------------------------
// Purpose: called when the engine shows the base client panel
//-----------------------------------------------------------------------------
void CBaseViewport::ActivateClientUI() 
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the engine hides the base client panel
//-----------------------------------------------------------------------------
void CBaseViewport::HideClientUI()
{
}

//-----------------------------------------------------------------------------
// Purpose: passes death msgs to the scoreboard to display specially
//-----------------------------------------------------------------------------
void CBaseViewport::FireGameEvent( IGameEvent * event)
{
	const char * type = event->GetName();

	if ( V_strcmp(type, "game_newmap") == 0 )
	{
		// hide all panels when reconnecting 
		ShowPanel( PANEL_ALL, false );

		if ( g_bEngineIsHLTV
			)
		{
			ShowPanel( PANEL_SPECGUI, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewport::ReloadScheme(const char *fromFile)
{
}

void CBaseViewport::LoadHudLayout( void )
{
}

int CBaseViewport::GetDeathMessageStartHeight( void )
{
	return YRES(2);
}

void CBaseViewport::SetAsFullscreenViewportInterface( void )
{
	s_pFullscreenViewportInterface = this;
	m_bFullscreenViewport = true;
}

bool CBaseViewport::IsFullscreenViewport() const
{
	return m_bFullscreenViewport;
}

void CBaseViewport::LevelInit( void )
{
	for ( int i = 0; i < std::ssize( m_UnorderedPanels ); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		if ( p )
		{
			p->LevelInit();
		}
	}
}
