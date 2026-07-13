//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#pragma warning( disable : 4800  )  // disable forcing int to bool performance warning

#include "cbase.h"
#include <cdll_client_int.h>
#include <cdll_util.h>
#include <globalvars_base.h>

#include <keyvalues.h>

#include <igameresources.h>

#include "IGameUIFuncs.h"
#include "hud.h"

// our definition
#include "baseviewport.h"
#include <filesystem.h>
#include <convar.h>
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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

CON_COMMAND( showpanel, "Shows a viewport panel <name>" )
{
	if ( !GetViewPortInterface() )
		return;

	if ( args.ArgC() != 2 )
		return;

	GetViewPortInterface()->ShowPanel( args[ 1 ], true );
}

CON_COMMAND( hidepanel, "Hides a viewport panel <name>" )
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

	m_GameuiFuncs = NULL;
	m_GameEventManager = NULL;

	m_bHasParent = false;
	m_pActivePanel = NULL;

#if !defined( CSTRIKE15 )
	m_pLastActivePanel = NULL;
#endif

	m_OldSize[ 0 ] = m_OldSize[ 1 ] = -1;
}

void CBaseViewport::CreateDefaultPanels( void )
{
}

void CBaseViewport::UpdateAllPanels( void )
{
	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
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
	return NULL;
}


bool CBaseViewport::AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName )
{
	if ( !pPanel )
	{
		return false;
	}

	if ( FindPanelByName( pPanel->GetName() ) != NULL )
	{
		DevMsg("CBaseViewport::AddNewPanel: panel with name '%s' already exists.\n", pPanel->GetName() );
		return false;
	}

	m_Panels.Insert( pPanel->GetName(), pPanel );
	m_UnorderedPanels.AddToTail( pPanel );

	return true;
}

IViewPortPanel* CBaseViewport::FindPanelByName(const char *szPanelName)
{
	int idx = m_Panels.Find( szPanelName );
	if ( idx == m_Panels.InvalidIndex() )
		return NULL;

	return m_Panels[ idx ];
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

	if ( Q_strcmp( pName, PANEL_ALL ) == 0 )
	{
		for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
		{
			IViewPortPanel *p = m_UnorderedPanels[i];
			ShowPanel( p, state );
		}

		return;
	}

	IViewPortPanel * panel = NULL;

	if ( Q_strcmp( pName, PANEL_ACTIVE ) == 0 )
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
#if defined( REPLAY_ENABLED )
				&& !engine->IsReplay()
#endif
				)
				return;

			if ( (m_pActivePanel != NULL) && (m_pActivePanel != pPanel) && (m_pActivePanel->IsVisible()) )
			{
				// store a pointer to the currently active panel
				// so we can restore it later
				if ( pPanel->CanReplace( m_pActivePanel->GetName() ) )
				{
#if !defined( CSTRIKE15 )
					m_pLastActivePanel = m_pActivePanel;
#endif

#ifdef CSTRIKE15 
					// in cs, if the scoreboard tries to hide the spectator via this method, just skip it
					IViewPortPanel* pSpecGuiPanel = FindPanelByName(PANEL_SPECGUI);
					if ( pSpecGuiPanel != m_pActivePanel )
					{
						DevMsg("CBaseViewport::ShowPanel(0) %s\n", m_pActivePanel->GetName());
						m_pActivePanel->ShowPanel( false );
					}
#else
					DevMsg("CBaseViewport::ShowPanel(0) %s\n", m_pActivePanel->GetName());
					m_pActivePanel->ShowPanel( false );
#endif
				}
				else
				{
#if !defined( CSTRIKE15 )
					m_pLastActivePanel = pPanel;
#endif
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
			m_pActivePanel = NULL;
		}

#if !defined( CSTRIKE15 )
		// restore the previous active panel if it exists
		if( m_pLastActivePanel )
		{
			m_pActivePanel = m_pLastActivePanel;
			m_pLastActivePanel = NULL;

			DevMsg("CBaseViewport::ShowPanel(1) %s\n", m_pActivePanel->GetName());
			m_pActivePanel->ShowPanel( true );
		}
#endif
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
		m_Panels.Remove( szPanelName );
		for ( int i = m_UnorderedPanels.Count() - 1; i >= 0; --i )
		{
			if ( m_UnorderedPanels[ i ] == panel )
			{
				m_UnorderedPanels.Remove( i );
				break;
			}
		}

		delete panel;

		if ( m_pActivePanel == panel )
		{
			m_pActivePanel = NULL;
		}

#if !defined( CSTRIKE15 )
		if ( m_pLastActivePanel == panel )
		{
			m_pLastActivePanel = NULL;
		}
#endif

		AddNewPanel( CreatePanelByName( szPanelName ), szPanelName );
	}
}


void CBaseViewport::RemoveAllPanels( void)
{
	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		delete p;
	}

	m_Panels.RemoveAll();
	m_UnorderedPanels.RemoveAll();
	m_pActivePanel = NULL;
#if !defined( CSTRIKE15 )
	m_pLastActivePanel = NULL;
#endif
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

// Return TRUE if the HUD's allowed to print text messages
bool CBaseViewport::AllowedToPrintText( void )
{
	return ( m_pActivePanel == NULL);
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

	if ( Q_strcmp(type, "game_newmap") == 0 )
	{
		// hide all panels when reconnecting 
		ShowPanel( PANEL_ALL, false );

		if ( g_bEngineIsHLTV
#if defined( REPLAY_ENABLED )
			|| engine->IsReplay()
#endif
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
	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];
		if ( p )
		{
			p->LevelInit();
		}
	}
}
